// Copyright 2021 The Manifold Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "./impl.h"
#include "./parallel.h"

namespace {
using namespace manifold;

// Returns a normalized vector orthogonal to ref, in the plane of ref and in,
// unless in and ref are colinear, in which case it falls back to the plane of
// ref and altIn.
vec3 OrthogonalTo(vec3 in, vec3 altIn, vec3 ref) {
  vec3 out = in - la::dot(in, ref) * ref;
  if (la::dot(out, out) < kPrecision * la::dot(in, in)) {
    out = altIn - la::dot(altIn, ref) * ref;
  }
  return SafeNormalize(out);
}

double Wrap(double radians) {
  return radians < -kPi  ? radians + kTwoPi
         : radians > kPi ? radians - kTwoPi
                         : radians;
}

// Get the angle between two unit-vectors.
double AngleBetween(vec3 a, vec3 b) {
  const double dot = la::dot(a, b);
  return dot >= 1 ? 0 : (dot <= -1 ? kPi : la::acos(dot));
}

// Calculate a tangent vector in the form of a weighted cubic Bezier taking as
// input the desired tangent direction (length doesn't matter) and the edge
// vector to the neighboring vertex. In a symmetric situation where the tangents
// at each end are mirror images of each other, this will result in a circular
// arc.
vec4 CircularTangent(const vec3& tangent, const vec3& edgeVec) {
  const vec3 dir = SafeNormalize(tangent);

  double weight = std::max(0.5, la::dot(dir, SafeNormalize(edgeVec)));
  // Quadratic weighted bezier for circular interpolation
  const vec4 bz2 = vec4(dir * 0.5 * la::length(edgeVec), weight);
  // Equivalent cubic weighted bezier
  const vec4 bz3 = la::lerp(vec4(0, 0, 0, 1), bz2, 2 / 3.0);
  // Convert from homogeneous form to geometric form
  return vec4(vec3(bz3) / bz3.w, bz3.w);
}

struct InterpTri {
  VecView<vec3> vertPos;
  VecView<const Barycentric> vertBary;
  const Manifold::Impl* impl;

  static vec4 Homogeneous(vec4 v) {
    v.x *= v.w;
    v.y *= v.w;
    v.z *= v.w;
    return v;
  }

  static vec4 Homogeneous(vec3 v) { return vec4(v, 1.0); }

  static vec3 HNormalize(vec4 v) {
    return v.w == 0 ? vec3(v) : (vec3(v) / v.w);
  }

  static vec4 Scale(vec4 v, double scale) { return vec4(scale * vec3(v), v.w); }

  static vec4 Bezier(vec3 point, vec4 tangent) {
    return Homogeneous(vec4(point, 0) + tangent);
  }

  static mat4x2 CubicBezier2Linear(vec4 p0, vec4 p1, vec4 p2, vec4 p3,
                                   double x) {
    mat4x2 out;
    vec4 p12 = la::lerp(p1, p2, x);
    out[0] = la::lerp(la::lerp(p0, p1, x), p12, x);
    out[1] = la::lerp(p12, la::lerp(p2, p3, x), x);
    return out;
  }

  static vec3 BezierPoint(mat4x2 points, double x) {
    return HNormalize(la::lerp(points[0], points[1], x));
  }

  static vec3 BezierTangent(mat4x2 points) {
    return SafeNormalize(HNormalize(points[1]) - HNormalize(points[0]));
  }

  static vec3 RotateFromTo(vec3 v, quat start, quat end) {
    return la::qrot(end, la::qrot(la::qconj(start), v));
  }

  static quat Slerp(const quat& x, const quat& y, double a, bool longWay) {
    quat z = y;
    double cosTheta = la::dot(x, y);

    // Take the long way around the sphere only when requested
    if ((cosTheta < 0) != longWay) {
      z = -y;
      cosTheta = -cosTheta;
    }

    if (cosTheta > 1.0 - std::numeric_limits<double>::epsilon()) {
      return la::lerp(x, z, a);  // for numerical stability
    } else {
      double angle = std::acos(cosTheta);
      return (std::sin((1.0 - a) * angle) * x + std::sin(a * angle) * z) /
             std::sin(angle);
    }
  }

  static mat4x2 Bezier2Bezier(const mat3x2& corners, const mat4x2& tangentsX,
                              const mat4x2& tangentsY, double x,
                              const vec3& anchor) {
    const mat4x2 bez = CubicBezier2Linear(
        Homogeneous(corners[0]), Bezier(corners[0], tangentsX[0]),
        Bezier(corners[1], tangentsX[1]), Homogeneous(corners[1]), x);
    const vec3 end = BezierPoint(bez, x);
    const vec3 tangent = BezierTangent(bez);

    const mat3x2 nTangentsX(SafeNormalize(vec3(tangentsX[0])),
                            -SafeNormalize(vec3(tangentsX[1])));
    const mat3x2 biTangents = {
        OrthogonalTo(vec3(tangentsY[0]), (anchor - corners[0]), nTangentsX[0]),
        OrthogonalTo(vec3(tangentsY[1]), (anchor - corners[1]), nTangentsX[1])};

    const quat q0 = la::rotation_quat(mat3(
        nTangentsX[0], biTangents[0], la::cross(nTangentsX[0], biTangents[0])));
    const quat q1 = la::rotation_quat(mat3(
        nTangentsX[1], biTangents[1], la::cross(nTangentsX[1], biTangents[1])));
    const vec3 edge = corners[1] - corners[0];
    const bool longWay =
        la::dot(nTangentsX[0], edge) + la::dot(nTangentsX[1], edge) < 0;
    const quat qTmp = Slerp(q0, q1, x, longWay);
    const quat q = la::qmul(la::rotation_quat(la::qxdir(qTmp), tangent), qTmp);

    const vec3 delta = la::lerp(RotateFromTo(vec3(tangentsY[0]), q0, q),
                                RotateFromTo(vec3(tangentsY[1]), q1, q), x);
    const double deltaW = la::lerp(tangentsY[0].w, tangentsY[1].w, x);

    return {Homogeneous(end), vec4(delta, deltaW)};
  }

  static vec3 Bezier2D(const mat3x4& corners, const mat4& tangentsX,
                       const mat4& tangentsY, double x, double y,
                       const vec3& centroid) {
    mat4x2 bez0 =
        Bezier2Bezier({corners[0], corners[1]}, {tangentsX[0], tangentsX[1]},
                      {tangentsY[0], tangentsY[1]}, x, centroid);
    mat4x2 bez1 =
        Bezier2Bezier({corners[2], corners[3]}, {tangentsX[2], tangentsX[3]},
                      {tangentsY[2], tangentsY[3]}, 1 - x, centroid);

    const mat4x2 bez =
        CubicBezier2Linear(bez0[0], Bezier(vec3(bez0[0]), bez0[1]),
                           Bezier(vec3(bez1[0]), bez1[1]), bez1[0], y);
    return BezierPoint(bez, y);
  }

  void operator()(const int vert) {
    vec3& pos = vertPos[vert];
    const int tri = vertBary[vert].tri;
    const vec4 uvw = vertBary[vert].uvw;

    const ivec4 halfedges = impl->GetHalfedges(tri);
    const mat3x4 corners = {
        impl->vertPos_[impl->halfedge_[halfedges[0]].startVert],
        impl->vertPos_[impl->halfedge_[halfedges[1]].startVert],
        impl->vertPos_[impl->halfedge_[halfedges[2]].startVert],
        halfedges[3] < 0
            ? vec3(0.0)
            : impl->vertPos_[impl->halfedge_[halfedges[3]].startVert]};

    for (const int i : {0, 1, 2, 3}) {
      if (uvw[i] == 1) {
        pos = corners[i];
        return;
      }
    }

    vec4 posH(0.0);

    if (halfedges[3] < 0) {  // tri
      const mat4x3 tangentR = {impl->halfedgeTangent_[halfedges[0]],
                               impl->halfedgeTangent_[halfedges[1]],
                               impl->halfedgeTangent_[halfedges[2]]};
      const mat4x3 tangentL = {
          impl->halfedgeTangent_[impl->halfedge_[halfedges[2]].pairedHalfedge],
          impl->halfedgeTangent_[impl->halfedge_[halfedges[0]].pairedHalfedge],
          impl->halfedgeTangent_[impl->halfedge_[halfedges[1]].pairedHalfedge]};
      const vec3 centroid = mat3(corners) * vec3(1.0 / 3);

      for (const int i : {0, 1, 2}) {
        const int j = Next3(i);
        const int k = Prev3(i);
        const double x = uvw[k] / (1 - uvw[i]);

        const mat4x2 bez =
            Bezier2Bezier({corners[j], corners[k]}, {tangentR[j], tangentL[k]},
                          {tangentL[j], tangentR[k]}, x, centroid);

        const mat4x2 bez1 = CubicBezier2Linear(
            bez[0], Bezier(vec3(bez[0]), bez[1]),
            Bezier(corners[i], la::lerp(tangentR[i], tangentL[i], x)),
            Homogeneous(corners[i]), uvw[i]);
        const vec3 p = BezierPoint(bez1, uvw[i]);
        posH += Homogeneous(vec4(p, uvw[j] * uvw[k]));
      }
    } else {  // quad
      const mat4 tangentsX = {
          impl->halfedgeTangent_[halfedges[0]],
          impl->halfedgeTangent_[impl->halfedge_[halfedges[0]].pairedHalfedge],
          impl->halfedgeTangent_[halfedges[2]],
          impl->halfedgeTangent_[impl->halfedge_[halfedges[2]].pairedHalfedge]};
      const mat4 tangentsY = {
          impl->halfedgeTangent_[impl->halfedge_[halfedges[3]].pairedHalfedge],
          impl->halfedgeTangent_[halfedges[1]],
          impl->halfedgeTangent_[impl->halfedge_[halfedges[1]].pairedHalfedge],
          impl->halfedgeTangent_[halfedges[3]]};
      const vec3 centroid = corners * vec4(0.25);
      const double x = uvw[1] + uvw[2];
      const double y = uvw[2] + uvw[3];
      const vec3 pX = Bezier2D(corners, tangentsX, tangentsY, x, y, centroid);
      const vec3 pY =
          Bezier2D({corners[1], corners[2], corners[3], corners[0]},
                   {tangentsY[1], tangentsY[2], tangentsY[3], tangentsY[0]},
                   {tangentsX[1], tangentsX[2], tangentsX[3], tangentsX[0]}, y,
                   1 - x, centroid);
      posH += Homogeneous(vec4(pX, x * (1 - x)));
      posH += Homogeneous(vec4(pY, y * (1 - y)));
    }
    pos = HNormalize(posH);
  }
};
}  // namespace

namespace manifold {

/**
 * Get the property normal associated with the startVert of this halfedge, where
 * normalIdx shows the beginning of where normals are stored in the properties.
 */
vec3 Manifold::Impl::GetNormal(int halfedge, int normalIdx) const {
  const int tri = halfedge / 3;
  const int j = halfedge % 3;
  const int prop = meshRelation_.triProperties[tri][j];
  vec3 normal;
  for (const int i : {0, 1, 2}) {
    normal[i] =
        meshRelation_.properties[prop * meshRelation_.numProp + normalIdx + i];
  }
  return normal;
}

/**
 * Returns a circular tangent for the requested halfedge, orthogonal to the
 * given normal vector, and avoiding folding.
 */
vec4 Manifold::Impl::TangentFromNormal(const vec3& normal, int halfedge) const {
  const Halfedge edge = halfedge_[halfedge];
  const vec3 edgeVec = vertPos_[edge.endVert] - vertPos_[edge.startVert];
  const vec3 edgeNormal =
      faceNormal_[halfedge / 3] + faceNormal_[edge.pairedHalfedge / 3];
  vec3 dir = la::cross(la::cross(edgeNormal, edgeVec), normal);
  return CircularTangent(dir, edgeVec);
}

/**
 * Returns true if this halfedge should be marked as the interior of a quad, as
 * defined by its two triangles referring to the same face, and those triangles
 * having no further face neighbors beyond.
 */
bool Manifold::Impl::IsInsideQuad(int halfedge) const {
  if (halfedgeTangent_.size() > 0) {
    return halfedgeTangent_[halfedge].w < 0;
  }
  const int tri = halfedge / 3;
  const TriRef ref = meshRelation_.triRef[tri];
  const int pair = halfedge_[halfedge].pairedHalfedge;
  const int pairTri = pair / 3;
  const TriRef pairRef = meshRelation_.triRef[pairTri];
  if (!ref.SameFace(pairRef)) return false;

  auto SameFace = [this](int halfedge, const TriRef& ref) {
    return ref.SameFace(
        meshRelation_.triRef[halfedge_[halfedge].pairedHalfedge / 3]);
  };

  int neighbor = NextHalfedge(halfedge);
  if (SameFace(neighbor, ref)) return false;
  neighbor = NextHalfedge(neighbor);
  if (SameFace(neighbor, ref)) return false;
  neighbor = NextHalfedge(pair);
  if (SameFace(neighbor, pairRef)) return false;
  neighbor = NextHalfedge(neighbor);
  if (SameFace(neighbor, pairRef)) return false;
  return true;
}

/**
 * Returns true if this halfedge is an interior of a quad, as defined by its
 * halfedge tangent having negative weight.
 */
bool Manifold::Impl::IsMarkedInsideQuad(int halfedge) const {
  return halfedgeTangent_.size() > 0 && halfedgeTangent_[halfedge].w < 0;
}

// sharpenedEdges are referenced to the input Mesh, but the triangles have
// been sorted in creating the Manifold, so the indices are converted using
// meshRelation_.
std::vector<Smoothness> Manifold::Impl::UpdateSharpenedEdges(
    const std::vector<Smoothness>& sharpenedEdges) const {
  std::unordered_map<int, int> oldHalfedge2New;
  for (size_t tri = 0; tri < NumTri(); ++tri) {
    int oldTri = meshRelation_.triRef[tri].tri;
    for (int i : {0, 1, 2}) oldHalfedge2New[3 * oldTri + i] = 3 * tri + i;
  }
  std::vector<Smoothness> newSharp = sharpenedEdges;
  for (Smoothness& edge : newSharp) {
    edge.halfedge = oldHalfedge2New[edge.halfedge];
  }
  return newSharp;
}

// Find faces containing at least 3 triangles - these will not have
// interpolated normals - all their vert normals must match their face normal.
Vec<bool> Manifold::Impl::FlatFaces() const {
  const int numTri = NumTri();
  Vec<bool> triIsFlatFace(numTri, false);
  for_each_n(autoPolicy(numTri, 1e5), countAt(0), numTri,
             [this, &triIsFlatFace](const int tri) {
               const TriRef& ref = meshRelation_.triRef[tri];
               int faceNeighbors = 0;
               ivec3 faceTris = {-1, -1, -1};
               for (const int j : {0, 1, 2}) {
                 const int neighborTri =
                     halfedge_[3 * tri + j].pairedHalfedge / 3;
                 const TriRef& jRef = meshRelation_.triRef[neighborTri];
                 if (jRef.SameFace(ref)) {
                   ++faceNeighbors;
                   faceTris[j] = neighborTri;
                 }
               }
               if (faceNeighbors > 1) {
                 triIsFlatFace[tri] = true;
                 for (const int j : {0, 1, 2}) {
                   if (faceTris[j] >= 0) {
                     triIsFlatFace[faceTris[j]] = true;
                   }
                 }
               }
             });
  return triIsFlatFace;
}

// Returns a vector of length numVert that has a tri that is part of a
// neighboring flat face if there is only one flat face. If there are none it
// gets -1, and if there are more than one it gets -2.
Vec<int> Manifold::Impl::VertFlatFace(const Vec<bool>& flatFaces) const {
  Vec<int> vertFlatFace(NumVert(), -1);
  Vec<TriRef> vertRef(NumVert(), {-1, -1, -1});
  for (size_t tri = 0; tri < NumTri(); ++tri) {
    if (flatFaces[tri]) {
      for (const int j : {0, 1, 2}) {
        const int vert = halfedge_[3 * tri + j].startVert;
        if (vertRef[vert].SameFace(meshRelation_.triRef[tri])) continue;
        vertRef[vert] = meshRelation_.triRef[tri];
        vertFlatFace[vert] = vertFlatFace[vert] == -1 ? tri : -2;
      }
    }
  }
  return vertFlatFace;
}

Vec<int> Manifold::Impl::VertHalfedge() const {
  Vec<int> vertHalfedge(NumVert());
  Vec<uint8_t> counters(NumVert(), 0);
  for_each_n(autoPolicy(halfedge_.size(), 1e5), countAt(0), halfedge_.size(),
             [&vertHalfedge, &counters, this](const int idx) {
               auto old = std::atomic_exchange(
                   reinterpret_cast<std::atomic<uint8_t>*>(
                       &counters[halfedge_[idx].startVert]),
                   static_cast<uint8_t>(1));
               if (old == 1) return;
               // arbitrary, last one wins.
               vertHalfedge[halfedge_[idx].startVert] = idx;
             });
  return vertHalfedge;
}

std::vector<Smoothness> Manifold::Impl::SharpenEdges(
    double minSharpAngle, double minSmoothness) const {
  std::vector<Smoothness> sharpenedEdges;
  const double minRadians = radians(minSharpAngle);
  for (size_t e = 0; e < halfedge_.size(); ++e) {
    if (!halfedge_[e].IsForward()) continue;
    const size_t pair = halfedge_[e].pairedHalfedge;
    const double dihedral =
        std::acos(la::dot(faceNormal_[e / 3], faceNormal_[pair / 3]));
    if (dihedral > minRadians) {
      sharpenedEdges.push_back({e, minSmoothness});
      sharpenedEdges.push_back({pair, minSmoothness});
    }
  }
  return sharpenedEdges;
}

/**
 * Sharpen tangents that intersect an edge to sharpen that edge. The weight is
 * unchanged, as this has a squared effect on radius of curvature, except
 * in the case of zero radius, which is marked with weight = 0.
 */
void Manifold::Impl::SharpenTangent(int halfedge, double smoothness) {
  halfedgeTangent_[halfedge] =
      vec4(smoothness * vec3(halfedgeTangent_[halfedge]),
           smoothness == 0 ? 0 : halfedgeTangent_[halfedge].w);
}

/**
 * Instead of calculating the internal shared normals like CalculateNormals
 * does, this method fills in vertex properties, unshared across edges that
 * are bent more than minSharpAngle.
 */
void Manifold::Impl::SetNormals(int normalIdx, double minSharpAngle) {
  if (IsEmpty()) return;
  if (normalIdx < 0) return;

  const int oldNumProp = NumProp();
  const int numTri = NumTri();

  Vec<bool> triIsFlatFace = FlatFaces();
  Vec<int> vertFlatFace = VertFlatFace(triIsFlatFace);
  Vec<int> vertNumSharp(NumVert(), 0);
  for (size_t e = 0; e < halfedge_.size(); ++e) {
    if (!halfedge_[e].IsForward()) continue;
    const int pair = halfedge_[e].pairedHalfedge;
    const int tri1 = e / 3;
    const int tri2 = pair / 3;
    const double dihedral =
        degrees(std::acos(la::dot(faceNormal_[tri1], faceNormal_[tri2])));
    if (dihedral > minSharpAngle) {
      ++vertNumSharp[halfedge_[e].startVert];
      ++vertNumSharp[halfedge_[e].endVert];
    } else {
      const bool faceSplit =
          triIsFlatFace[tri1] != triIsFlatFace[tri2] ||
          (triIsFlatFace[tri1] && triIsFlatFace[tri2] &&
           !meshRelation_.triRef[tri1].SameFace(meshRelation_.triRef[tri2]));
      if (vertFlatFace[halfedge_[e].startVert] == -2 && faceSplit) {
        ++vertNumSharp[halfedge_[e].startVert];
      }
      if (vertFlatFace[halfedge_[e].endVert] == -2 && faceSplit) {
        ++vertNumSharp[halfedge_[e].endVert];
      }
    }
  }

  const int numProp = std::max(oldNumProp, normalIdx + 3);
  Vec<double> oldProperties(numProp * NumPropVert(), 0);
  meshRelation_.properties.swap(oldProperties);
  meshRelation_.numProp = numProp;
  if (meshRelation_.triProperties.size() == 0) {
    meshRelation_.triProperties.resize(numTri);
    for_each_n(autoPolicy(numTri, 1e5), countAt(0), numTri, [this](int tri) {
      for (const int j : {0, 1, 2})
        meshRelation_.triProperties[tri][j] = halfedge_[3 * tri + j].startVert;
    });
  }
  Vec<ivec3> oldTriProp(numTri, {-1, -1, -1});
  meshRelation_.triProperties.swap(oldTriProp);

  for (int tri = 0; tri < numTri; ++tri) {
    for (const int i : {0, 1, 2}) {
      if (meshRelation_.triProperties[tri][i] >= 0) continue;
      int startEdge = 3 * tri + i;
      const int vert = halfedge_[startEdge].startVert;

      if (vertNumSharp[vert] < 2) {  // vertex has single normal
        const vec3 normal = vertFlatFace[vert] >= 0
                                ? faceNormal_[vertFlatFace[vert]]
                                : vertNormal_[vert];
        int lastProp = -1;
        ForVert(startEdge, [&](int current) {
          const int thisTri = current / 3;
          const int j = current - 3 * thisTri;
          const int prop = oldTriProp[thisTri][j];
          meshRelation_.triProperties[thisTri][j] = prop;
          if (prop == lastProp) return;
          lastProp = prop;
          // update property vertex
          auto start = oldProperties.begin() + prop * oldNumProp;
          std::copy(start, start + oldNumProp,
                    meshRelation_.properties.begin() + prop * numProp);
          for (const int i : {0, 1, 2})
            meshRelation_.properties[prop * numProp + normalIdx + i] =
                normal[i];
        });
      } else {  // vertex has multiple normals
        const vec3 centerPos = vertPos_[vert];
        // Length degree
        std::vector<int> group;
        // Length number of normals
        std::vector<vec3> normals;
        int current = startEdge;
        int prevFace = current / 3;

        do {  // find a sharp edge to start on
          int next = NextHalfedge(halfedge_[current].pairedHalfedge);
          const int face = next / 3;

          const double dihedral = degrees(
              std::acos(la::dot(faceNormal_[face], faceNormal_[prevFace])));
          if (dihedral > minSharpAngle ||
              triIsFlatFace[face] != triIsFlatFace[prevFace] ||
              (triIsFlatFace[face] && triIsFlatFace[prevFace] &&
               !meshRelation_.triRef[face].SameFace(
                   meshRelation_.triRef[prevFace]))) {
            break;
          }
          current = next;
          prevFace = face;
        } while (current != startEdge);

        const int endEdge = current;

        struct FaceEdge {
          int face;
          vec3 edgeVec;
        };

        // calculate pseudo-normals between each sharp edge
        ForVert<FaceEdge>(
            endEdge,
            [this, centerPos, &vertNumSharp, &vertFlatFace](int current) {
              if (IsInsideQuad(current)) {
                return FaceEdge({current / 3, vec3(NAN)});
              }
              const int vert = halfedge_[current].endVert;
              vec3 pos = vertPos_[vert];
              const vec3 edgeVec = centerPos - pos;
              if (vertNumSharp[vert] < 2) {
                // opposite vert has fixed normal
                const vec3 normal = vertFlatFace[vert] >= 0
                                        ? faceNormal_[vertFlatFace[vert]]
                                        : vertNormal_[vert];
                // Flair out the normal we're calculating to give the edge a
                // more constant curvature to meet the opposite normal. Achieve
                // this by pointing the tangent toward the opposite bezier
                // control point instead of the vert itself.
                pos += vec3(TangentFromNormal(
                    normal, halfedge_[current].pairedHalfedge));
              }
              return FaceEdge({current / 3, SafeNormalize(pos - centerPos)});
            },
            [this, &triIsFlatFace, &normals, &group, minSharpAngle](
                int current, const FaceEdge& here, FaceEdge& next) {
              const double dihedral = degrees(std::acos(
                  la::dot(faceNormal_[here.face], faceNormal_[next.face])));
              if (dihedral > minSharpAngle ||
                  triIsFlatFace[here.face] != triIsFlatFace[next.face] ||
                  (triIsFlatFace[here.face] && triIsFlatFace[next.face] &&
                   !meshRelation_.triRef[here.face].SameFace(
                       meshRelation_.triRef[next.face]))) {
                normals.push_back(vec3(0.0));
              }
              group.push_back(normals.size() - 1);
              if (std::isfinite(next.edgeVec.x)) {
                normals.back() +=
                    SafeNormalize(la::cross(next.edgeVec, here.edgeVec)) *
                    AngleBetween(here.edgeVec, next.edgeVec);
              } else {
                next.edgeVec = here.edgeVec;
              }
            });

        for (auto& normal : normals) {
          normal = SafeNormalize(normal);
        }

        int lastGroup = 0;
        int lastProp = -1;
        int newProp = -1;
        int idx = 0;
        ForVert(endEdge, [&](int current1) {
          const int thisTri = current1 / 3;
          const int j = current1 - 3 * thisTri;
          const int prop = oldTriProp[thisTri][j];
          auto start = oldProperties.begin() + prop * oldNumProp;

          if (group[idx] != lastGroup && group[idx] != 0 && prop == lastProp) {
            // split property vertex, duplicating but with an updated normal
            lastGroup = group[idx];
            newProp = NumPropVert();
            meshRelation_.properties.resize(meshRelation_.properties.size() +
                                            numProp);
            std::copy(start, start + oldNumProp,
                      meshRelation_.properties.begin() + newProp * numProp);
            for (const int i : {0, 1, 2}) {
              meshRelation_.properties[newProp * numProp + normalIdx + i] =
                  normals[group[idx]][i];
            }
          } else if (prop != lastProp) {
            // update property vertex
            lastProp = prop;
            newProp = prop;
            std::copy(start, start + oldNumProp,
                      meshRelation_.properties.begin() + prop * numProp);
            for (const int i : {0, 1, 2})
              meshRelation_.properties[prop * numProp + normalIdx + i] =
                  normals[group[idx]][i];
          }

          // point to updated property vertex
          meshRelation_.triProperties[thisTri][j] = newProp;
          ++idx;
        });
      }
    }
  }
}

/**
 * Tangents get flattened to create sharp edges by setting their weight to zero.
 * This is the natural limit of reducing the weight to increase the sharpness
 * smoothly. This limit gives a decent shape, but it causes the parameterization
 * to be stretched and compresses it near the edges, which is good for resolving
 * tight curvature, but bad for property interpolation. This function fixes the
 * parameter stretch at the limit for sharp edges, since there is no curvature
 * to resolve. Note this also changes the overall shape - making it more evenly
 * curved.
 */
void Manifold::Impl::LinearizeFlatTangents() {
  const int n = halfedgeTangent_.size();
  for_each_n(autoPolicy(n, 1e4), countAt(0), n, [this](const int halfedge) {
    vec4& tangent = halfedgeTangent_[halfedge];
    vec4& otherTangent = halfedgeTangent_[halfedge_[halfedge].pairedHalfedge];

    const bool flat[2] = {tangent.w == 0, otherTangent.w == 0};
    if (!halfedge_[halfedge].IsForward() || (!flat[0] && !flat[1])) {
      return;
    }

    const vec3 edgeVec = vertPos_[halfedge_[halfedge].endVert] -
                         vertPos_[halfedge_[halfedge].startVert];

    if (flat[0] && flat[1]) {
      tangent = vec4(edgeVec / 3.0, 1);
      otherTangent = vec4(-edgeVec / 3.0, 1);
    } else if (flat[0]) {
      tangent = vec4((edgeVec + vec3(otherTangent)) / 2.0, 1);
    } else {
      otherTangent = vec4((-edgeVec + vec3(tangent)) / 2.0, 1);
    }
  });
}

/**
 * Redistribute the tangents around each vertex so that the angles between them
 * have the same ratios as the angles of the triangles between the corresponding
 * edges. This avoids folding the output shape and gives smoother results. There
 * must be at least one fixed halfedge on a vertex for that vertex to be
 * operated on. If there is only one, then that halfedge is not treated as
 * fixed, but the whole circle is turned to an average orientation.
 */
void Manifold::Impl::DistributeTangents(const Vec<bool>& fixedHalfedges) {
  const int numHalfedge = fixedHalfedges.size();
  for_each_n(
      autoPolicy(numHalfedge, 1e4), countAt(0), numHalfedge,
      [this, &fixedHalfedges](int halfedge) {
        if (!fixedHalfedges[halfedge]) return;

        if (IsMarkedInsideQuad(halfedge)) {
          halfedge = NextHalfedge(halfedge_[halfedge].pairedHalfedge);
        }

        vec3 normal(0.0);
        Vec<double> currentAngle;
        Vec<double> desiredAngle;

        const vec3 approxNormal = vertNormal_[halfedge_[halfedge].startVert];
        const vec3 center = vertPos_[halfedge_[halfedge].startVert];
        vec3 lastEdgeVec =
            SafeNormalize(vertPos_[halfedge_[halfedge].endVert] - center);
        const vec3 firstTangent =
            SafeNormalize(vec3(halfedgeTangent_[halfedge]));
        vec3 lastTangent = firstTangent;
        int current = halfedge;
        do {
          current = NextHalfedge(halfedge_[current].pairedHalfedge);
          if (IsMarkedInsideQuad(current)) continue;
          const vec3 thisEdgeVec =
              SafeNormalize(vertPos_[halfedge_[current].endVert] - center);
          const vec3 thisTangent =
              SafeNormalize(vec3(halfedgeTangent_[current]));
          normal += la::cross(thisTangent, lastTangent);
          // cumulative sum
          desiredAngle.push_back(
              AngleBetween(thisEdgeVec, lastEdgeVec) +
              (desiredAngle.size() > 0 ? desiredAngle.back() : 0));
          if (current == halfedge) {
            currentAngle.push_back(kTwoPi);
          } else {
            currentAngle.push_back(AngleBetween(thisTangent, firstTangent));
            if (la::dot(approxNormal, la::cross(thisTangent, firstTangent)) <
                0) {
              currentAngle.back() = kTwoPi - currentAngle.back();
            }
          }
          lastEdgeVec = thisEdgeVec;
          lastTangent = thisTangent;
        } while (!fixedHalfedges[current]);

        if (currentAngle.size() == 1 || la::dot(normal, normal) == 0) return;

        const double scale = currentAngle.back() / desiredAngle.back();
        double offset = 0;
        if (current == halfedge) {  // only one - find average offset
          for (size_t i = 0; i < currentAngle.size(); ++i) {
            offset += Wrap(currentAngle[i] - scale * desiredAngle[i]);
          }
          offset /= currentAngle.size();
        }

        current = halfedge;
        size_t i = 0;
        do {
          current = NextHalfedge(halfedge_[current].pairedHalfedge);
          if (IsMarkedInsideQuad(current)) continue;
          desiredAngle[i] *= scale;
          const double lastAngle = i > 0 ? desiredAngle[i - 1] : 0;
          // shrink obtuse angles
          if (desiredAngle[i] - lastAngle > kPi) {
            desiredAngle[i] = lastAngle + kPi;
          } else if (i + 1 < desiredAngle.size() &&
                     scale * desiredAngle[i + 1] - desiredAngle[i] > kPi) {
            desiredAngle[i] = scale * desiredAngle[i + 1] - kPi;
          }
          const double angle = currentAngle[i] - desiredAngle[i] - offset;
          vec3 tangent(halfedgeTangent_[current]);
          const quat q = la::rotation_quat(la::normalize(normal), angle);
          halfedgeTangent_[current] =
              vec4(la::qrot(q, tangent), halfedgeTangent_[current].w);
          ++i;
        } while (!fixedHalfedges[current]);
      });
}

/**
 * Calculates halfedgeTangent_, allowing the manifold to be refined and
 * smoothed. The tangents form weighted cubic Beziers along each edge. This
 * function creates circular arcs where possible (minimizing maximum curvature),
 * constrained to the indicated property normals. Across edges that form
 * discontinuities in the normals, the tangent vectors are zero-length, allowing
 * the shape to form a sharp corner with minimal oscillation.
 */
void Manifold::Impl::CreateTangents(int normalIdx) {
  ZoneScoped;
  const int numVert = NumVert();
  const int numHalfedge = halfedge_.size();
  halfedgeTangent_.resize(0);
  Vec<vec4> tangent(numHalfedge);
  Vec<bool> fixedHalfedge(numHalfedge, false);

  Vec<int> vertHalfedge = VertHalfedge();
  for_each_n(
      autoPolicy(numVert, 1e4), vertHalfedge.begin(), numVert,
      [this, &tangent, &fixedHalfedge, normalIdx](int e) {
        struct FlatNormal {
          bool isFlatFace;
          vec3 normal;
        };

        ivec2 faceEdges(-1, -1);

        ForVert<FlatNormal>(
            e,
            [normalIdx, this](int halfedge) {
              const vec3 normal = GetNormal(halfedge, normalIdx);
              const vec3 diff = faceNormal_[halfedge / 3] - normal;
              return FlatNormal(
                  {la::dot(diff, diff) < kPrecision * kPrecision, normal});
            },
            [&faceEdges, &tangent, &fixedHalfedge, this](
                int halfedge, const FlatNormal& here, const FlatNormal& next) {
              if (IsInsideQuad(halfedge)) {
                tangent[halfedge] = {0, 0, 0, -1};
                return;
              }
              // mark special edges
              const vec3 diff = next.normal - here.normal;
              const bool differentNormals =
                  la::dot(diff, diff) > kPrecision * kPrecision;
              if (differentNormals || here.isFlatFace != next.isFlatFace) {
                fixedHalfedge[halfedge] = true;
                if (faceEdges[0] == -1) {
                  faceEdges[0] = halfedge;
                } else if (faceEdges[1] == -1) {
                  faceEdges[1] = halfedge;
                } else {
                  faceEdges[0] = -2;
                }
              }
              // calculate tangents
              if (differentNormals) {
                const vec3 edgeVec = vertPos_[halfedge_[halfedge].endVert] -
                                     vertPos_[halfedge_[halfedge].startVert];
                const vec3 dir = la::cross(here.normal, next.normal);
                tangent[halfedge] = CircularTangent(
                    (la::dot(dir, edgeVec) < 0 ? -1.0 : 1.0) * dir, edgeVec);
              } else {
                tangent[halfedge] = TangentFromNormal(here.normal, halfedge);
              }
            });

        if (faceEdges[0] >= 0 && faceEdges[1] >= 0) {
          const vec3 edge0 = vertPos_[halfedge_[faceEdges[0]].endVert] -
                             vertPos_[halfedge_[faceEdges[0]].startVert];
          const vec3 edge1 = vertPos_[halfedge_[faceEdges[1]].endVert] -
                             vertPos_[halfedge_[faceEdges[1]].startVert];
          const vec3 newTangent = la::normalize(edge0) - la::normalize(edge1);
          tangent[faceEdges[0]] = CircularTangent(newTangent, edge0);
          tangent[faceEdges[1]] = CircularTangent(-newTangent, edge1);
        } else if (faceEdges[0] == -1 && faceEdges[0] == -1) {
          fixedHalfedge[e] = true;
        }
      });

  halfedgeTangent_.swap(tangent);
  DistributeTangents(fixedHalfedge);
}

/**
 * Calculates halfedgeTangent_, allowing the manifold to be refined and
 * smoothed. The tangents form weighted cubic Beziers along each edge. This
 * function creates circular arcs where possible (minimizing maximum curvature),
 * constrained to the vertex normals. Where sharpenedEdges are specified, the
 * tangents are shortened that intersect the sharpened edge, concentrating the
 * curvature there, while the tangents of the sharp edges themselves are aligned
 * for continuity.
 */
void Manifold::Impl::CreateTangents(std::vector<Smoothness> sharpenedEdges) {
  ZoneScoped;
  const int numHalfedge = halfedge_.size();
  halfedgeTangent_.resize(0);
  Vec<vec4> tangent(numHalfedge);
  Vec<bool> fixedHalfedge(numHalfedge, false);

  Vec<int> vertHalfedge = VertHalfedge();
  Vec<bool> triIsFlatFace = FlatFaces();
  Vec<int> vertFlatFace = VertFlatFace(triIsFlatFace);
  Vec<vec3> vertNormal = vertNormal_;
  for (size_t v = 0; v < NumVert(); ++v) {
    if (vertFlatFace[v] >= 0) {
      vertNormal[v] = faceNormal_[vertFlatFace[v]];
    }
  }

  for_each_n(autoPolicy(numHalfedge, 1e4), countAt(0), numHalfedge,
             [&tangent, &vertNormal, this](const int edgeIdx) {
               tangent[edgeIdx] =
                   IsInsideQuad(edgeIdx)
                       ? vec4(0, 0, 0, -1)
                       : TangentFromNormal(
                             vertNormal[halfedge_[edgeIdx].startVert], edgeIdx);
             });

  halfedgeTangent_.swap(tangent);

  // Add sharpened edges around faces, just on the face side.
  for (size_t tri = 0; tri < NumTri(); ++tri) {
    if (!triIsFlatFace[tri]) continue;
    for (const int j : {0, 1, 2}) {
      const int tri2 = halfedge_[3 * tri + j].pairedHalfedge / 3;
      if (!triIsFlatFace[tri2] ||
          !meshRelation_.triRef[tri].SameFace(meshRelation_.triRef[tri2])) {
        sharpenedEdges.push_back({3 * tri + j, 0});
      }
    }
  }

  using Pair = std::pair<Smoothness, Smoothness>;
  // Fill in missing pairs with default smoothness = 1.
  std::map<int, Pair> edges;
  for (Smoothness edge : sharpenedEdges) {
    if (edge.smoothness >= 1) continue;
    const bool forward = halfedge_[edge.halfedge].IsForward();
    const int pair = halfedge_[edge.halfedge].pairedHalfedge;
    const int idx = forward ? edge.halfedge : pair;
    if (edges.find(idx) == edges.end()) {
      edges[idx] = {edge, {static_cast<size_t>(pair), 1}};
      if (!forward) std::swap(edges[idx].first, edges[idx].second);
    } else {
      Smoothness& e = forward ? edges[idx].first : edges[idx].second;
      e.smoothness = std::min(edge.smoothness, e.smoothness);
    }
  }

  std::map<int, std::vector<Pair>> vertTangents;
  for (const auto& value : edges) {
    const Pair edge = value.second;
    vertTangents[halfedge_[edge.first.halfedge].startVert].push_back(edge);
    vertTangents[halfedge_[edge.second.halfedge].startVert].push_back(
        {edge.second, edge.first});
  }

  const int numVert = NumVert();
  for_each_n(
      autoPolicy(numVert, 1e4), countAt(0), numVert,
      [this, &vertTangents, &fixedHalfedge, &vertHalfedge,
       &triIsFlatFace](int v) {
        auto it = vertTangents.find(v);
        if (it == vertTangents.end()) {
          fixedHalfedge[vertHalfedge[v]] = true;
          return;
        }
        const std::vector<Pair>& vert = it->second;
        // Sharp edges that end are smooth at their terminal vert.
        if (vert.size() == 1) return;
        if (vert.size() == 2) {  // Make continuous edge
          const int first = vert[0].first.halfedge;
          const int second = vert[1].first.halfedge;
          fixedHalfedge[first] = true;
          fixedHalfedge[second] = true;
          const vec3 newTangent = la::normalize(vec3(halfedgeTangent_[first]) -
                                                vec3(halfedgeTangent_[second]));

          const vec3 pos = vertPos_[halfedge_[first].startVert];
          halfedgeTangent_[first] = CircularTangent(
              newTangent, vertPos_[halfedge_[first].endVert] - pos);
          halfedgeTangent_[second] = CircularTangent(
              -newTangent, vertPos_[halfedge_[second].endVert] - pos);

          double smoothness =
              (vert[0].second.smoothness + vert[1].first.smoothness) / 2;
          ForVert(first, [this, &smoothness, &vert, first,
                          second](int current) {
            if (current == second) {
              smoothness =
                  (vert[1].second.smoothness + vert[0].first.smoothness) / 2;
            } else if (current != first && !IsMarkedInsideQuad(current)) {
              SharpenTangent(current, smoothness);
            }
          });
        } else {  // Sharpen vertex uniformly
          double smoothness = 0;
          double denom = 0;
          for (const Pair& pair : vert) {
            smoothness += pair.first.smoothness;
            smoothness += pair.second.smoothness;
            denom += pair.first.smoothness == 0 ? 0 : 1;
            denom += pair.second.smoothness == 0 ? 0 : 1;
          }
          smoothness /= denom;

          ForVert(vert[0].first.halfedge,
                  [this, &triIsFlatFace, smoothness](int current) {
                    if (!IsMarkedInsideQuad(current)) {
                      const int pair = halfedge_[current].pairedHalfedge;
                      SharpenTangent(current, triIsFlatFace[current / 3] ||
                                                      triIsFlatFace[pair / 3]
                                                  ? 0
                                                  : smoothness);
                    }
                  });
        }
      });

  LinearizeFlatTangents();
  DistributeTangents(fixedHalfedge);
}

void Manifold::Impl::Refine(std::function<int(vec3, vec4, vec4)> edgeDivisions,
                            bool keepInterior) {
  if (IsEmpty()) return;
  Manifold::Impl old = *this;
  Vec<Barycentric> vertBary = Subdivide(edgeDivisions, keepInterior);
  if (vertBary.size() == 0) return;

  if (old.halfedgeTangent_.size() == old.halfedge_.size()) {
    for_each_n(autoPolicy(NumTri(), 1e4), countAt(0), NumVert(),
               InterpTri({vertPos_, vertBary, &old}));
  }

  halfedgeTangent_.resize(0);
  Finish();
  CreateFaces();
  meshRelation_.originalID = -1;
}

}  // namespace manifold
