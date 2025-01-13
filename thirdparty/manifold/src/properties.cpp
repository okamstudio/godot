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

#include <limits>

#include "./impl.h"
#include "./parallel.h"
#include "./tri_dist.h"

namespace {
using namespace manifold;

struct CurvatureAngles {
  VecView<double> meanCurvature;
  VecView<double> gaussianCurvature;
  VecView<double> area;
  VecView<double> degree;
  VecView<const Halfedge> halfedge;
  VecView<const vec3> vertPos;
  VecView<const vec3> triNormal;

  void operator()(size_t tri) {
    vec3 edge[3];
    vec3 edgeLength(0.0);
    for (int i : {0, 1, 2}) {
      const int startVert = halfedge[3 * tri + i].startVert;
      const int endVert = halfedge[3 * tri + i].endVert;
      edge[i] = vertPos[endVert] - vertPos[startVert];
      edgeLength[i] = la::length(edge[i]);
      edge[i] /= edgeLength[i];
      const int neighborTri = halfedge[3 * tri + i].pairedHalfedge / 3;
      const double dihedral =
          0.25 * edgeLength[i] *
          std::asin(la::dot(la::cross(triNormal[tri], triNormal[neighborTri]),
                            edge[i]));
      AtomicAdd(meanCurvature[startVert], dihedral);
      AtomicAdd(meanCurvature[endVert], dihedral);
      AtomicAdd(degree[startVert], 1.0);
    }

    vec3 phi;
    phi[0] = std::acos(-la::dot(edge[2], edge[0]));
    phi[1] = std::acos(-la::dot(edge[0], edge[1]));
    phi[2] = kPi - phi[0] - phi[1];
    const double area3 = edgeLength[0] * edgeLength[1] *
                         la::length(la::cross(edge[0], edge[1])) / 6;

    for (int i : {0, 1, 2}) {
      const int vert = halfedge[3 * tri + i].startVert;
      AtomicAdd(gaussianCurvature[vert], -phi[i]);
      AtomicAdd(area[vert], area3);
    }
  }
};

struct UpdateProperties {
  VecView<ivec3> triProp;
  VecView<double> properties;
  VecView<uint8_t> counters;

  VecView<const double> oldProperties;
  VecView<const Halfedge> halfedge;
  VecView<const double> meanCurvature;
  VecView<const double> gaussianCurvature;
  const int oldNumProp;
  const int numProp;
  const int gaussianIdx;
  const int meanIdx;

  void operator()(const size_t tri) {
    for (const int i : {0, 1, 2}) {
      const int vert = halfedge[3 * tri + i].startVert;
      if (oldNumProp == 0) {
        triProp[tri][i] = vert;
      }
      const int propVert = triProp[tri][i];

      auto old = std::atomic_exchange(
          reinterpret_cast<std::atomic<uint8_t>*>(&counters[propVert]),
          static_cast<uint8_t>(1));
      if (old == 1) continue;

      for (int p = 0; p < oldNumProp; ++p) {
        properties[numProp * propVert + p] =
            oldProperties[oldNumProp * propVert + p];
      }

      if (gaussianIdx >= 0) {
        properties[numProp * propVert + gaussianIdx] = gaussianCurvature[vert];
      }
      if (meanIdx >= 0) {
        properties[numProp * propVert + meanIdx] = meanCurvature[vert];
      }
    }
  }
};

struct CheckHalfedges {
  VecView<const Halfedge> halfedges;

  bool operator()(size_t edge) const {
    const Halfedge halfedge = halfedges[edge];
    if (halfedge.startVert == -1 || halfedge.endVert == -1) return true;
    if (halfedge.pairedHalfedge == -1) return false;

    const Halfedge paired = halfedges[halfedge.pairedHalfedge];
    bool good = true;
    good &= paired.pairedHalfedge == static_cast<int>(edge);
    good &= halfedge.startVert != halfedge.endVert;
    good &= halfedge.startVert == paired.endVert;
    good &= halfedge.endVert == paired.startVert;
    return good;
  }
};

struct CheckCCW {
  VecView<const Halfedge> halfedges;
  VecView<const vec3> vertPos;
  VecView<const vec3> triNormal;
  const double tol;

  bool operator()(size_t face) const {
    if (halfedges[3 * face].pairedHalfedge < 0) return true;

    const mat2x3 projection = GetAxisAlignedProjection(triNormal[face]);
    vec2 v[3];
    for (int i : {0, 1, 2})
      v[i] = projection * vertPos[halfedges[3 * face + i].startVert];

    int ccw = CCW(v[0], v[1], v[2], std::abs(tol));
    bool check = tol > 0 ? ccw >= 0 : ccw == 0;

#ifdef MANIFOLD_DEBUG
    if (tol > 0 && !check) {
      vec2 v1 = v[1] - v[0];
      vec2 v2 = v[2] - v[0];
      double area = v1.x * v2.y - v1.y * v2.x;
      double base2 = std::max(la::dot(v1, v1), la::dot(v2, v2));
      double base = std::sqrt(base2);
      vec3 V0 = vertPos[halfedges[3 * face].startVert];
      vec3 V1 = vertPos[halfedges[3 * face + 1].startVert];
      vec3 V2 = vertPos[halfedges[3 * face + 2].startVert];
      vec3 norm = la::cross(V1 - V0, V2 - V0);
      printf(
          "Tri %ld does not match normal, approx height = %g, base = %g\n"
          "tol = %g, area2 = %g, base2*tol2 = %g\n"
          "normal = %g, %g, %g\n"
          "norm = %g, %g, %g\nverts: %d, %d, %d\n",
          static_cast<long>(face), area / base, base, tol, area * area,
          base2 * tol * tol, triNormal[face].x, triNormal[face].y,
          triNormal[face].z, norm.x, norm.y, norm.z,
          halfedges[3 * face].startVert, halfedges[3 * face + 1].startVert,
          halfedges[3 * face + 2].startVert);
    }
#endif
    return check;
  }
};
}  // namespace

namespace manifold {

/**
 * Returns true if this manifold is in fact an oriented even manifold and all of
 * the data structures are consistent.
 */
bool Manifold::Impl::IsManifold() const {
  if (halfedge_.size() == 0) return true;
  return all_of(countAt(0_uz), countAt(halfedge_.size()),
                CheckHalfedges({halfedge_}));
}

/**
 * Returns true if this manifold is in fact an oriented 2-manifold and all of
 * the data structures are consistent.
 */
bool Manifold::Impl::Is2Manifold() const {
  if (halfedge_.size() == 0) return true;
  if (!IsManifold()) return false;

  Vec<Halfedge> halfedge(halfedge_);
  stable_sort(halfedge.begin(), halfedge.end());

  return all_of(
      countAt(0_uz), countAt(2 * NumEdge() - 1), [&halfedge](size_t edge) {
        const Halfedge h = halfedge[edge];
        if (h.startVert == -1 && h.endVert == -1 && h.pairedHalfedge == -1)
          return true;
        return h.startVert != halfedge[edge + 1].startVert ||
               h.endVert != halfedge[edge + 1].endVert;
      });
}

/**
 * Returns true if all triangles are CCW relative to their triNormals_.
 */
bool Manifold::Impl::MatchesTriNormals() const {
  if (halfedge_.size() == 0 || faceNormal_.size() != NumTri()) return true;
  return all_of(countAt(0_uz), countAt(NumTri()),
                CheckCCW({halfedge_, vertPos_, faceNormal_, 2 * epsilon_}));
}

/**
 * Returns the number of triangles that are colinear within epsilon_.
 */
int Manifold::Impl::NumDegenerateTris() const {
  if (halfedge_.size() == 0 || faceNormal_.size() != NumTri()) return true;
  return count_if(
      countAt(0_uz), countAt(NumTri()),
      CheckCCW({halfedge_, vertPos_, faceNormal_, -1 * epsilon_ / 2}));
}

double Manifold::Impl::GetProperty(Property prop) const {
  ZoneScoped;
  if (IsEmpty()) return 0;

  auto Volume = [this](size_t tri) {
    const vec3 v = vertPos_[halfedge_[3 * tri].startVert];
    vec3 crossP = la::cross(vertPos_[halfedge_[3 * tri + 1].startVert] - v,
                            vertPos_[halfedge_[3 * tri + 2].startVert] - v);
    return la::dot(crossP, v) / 6.0;
  };

  auto Area = [this](size_t tri) {
    const vec3 v = vertPos_[halfedge_[3 * tri].startVert];
    return la::length(
               la::cross(vertPos_[halfedge_[3 * tri + 1].startVert] - v,
                         vertPos_[halfedge_[3 * tri + 2].startVert] - v)) /
           2.0;
  };

  // Kahan summation
  double value = 0;
  double valueCompensation = 0;
  for (size_t i = 0; i < NumTri(); ++i) {
    const double value1 = prop == Property::SurfaceArea ? Area(i) : Volume(i);
    const double t = value + value1;
    valueCompensation += (value - t) + value1;
    value = t;
  }
  value += valueCompensation;
  return value;
}

void Manifold::Impl::CalculateCurvature(int gaussianIdx, int meanIdx) {
  ZoneScoped;
  if (IsEmpty()) return;
  if (gaussianIdx < 0 && meanIdx < 0) return;
  Vec<double> vertMeanCurvature(NumVert(), 0);
  Vec<double> vertGaussianCurvature(NumVert(), kTwoPi);
  Vec<double> vertArea(NumVert(), 0);
  Vec<double> degree(NumVert(), 0);
  auto policy = autoPolicy(NumTri(), 1e4);
  for_each(policy, countAt(0_uz), countAt(NumTri()),
           CurvatureAngles({vertMeanCurvature, vertGaussianCurvature, vertArea,
                            degree, halfedge_, vertPos_, faceNormal_}));
  for_each_n(policy, countAt(0), NumVert(),
             [&vertMeanCurvature, &vertGaussianCurvature, &vertArea,
              &degree](const int vert) {
               const double factor = degree[vert] / (6 * vertArea[vert]);
               vertMeanCurvature[vert] *= factor;
               vertGaussianCurvature[vert] *= factor;
             });

  const int oldNumProp = NumProp();
  const int numProp = std::max(oldNumProp, std::max(gaussianIdx, meanIdx) + 1);
  const Vec<double> oldProperties = meshRelation_.properties;
  meshRelation_.properties = Vec<double>(numProp * NumPropVert(), 0);
  meshRelation_.numProp = numProp;
  if (meshRelation_.triProperties.size() == 0) {
    meshRelation_.triProperties.resize(NumTri());
  }

  const Vec<uint8_t> counters(NumPropVert(), 0);
  for_each_n(
      policy, countAt(0_uz), NumTri(),
      UpdateProperties({meshRelation_.triProperties, meshRelation_.properties,
                        counters, oldProperties, halfedge_, vertMeanCurvature,
                        vertGaussianCurvature, oldNumProp, numProp, gaussianIdx,
                        meanIdx}));
}

/**
 * Calculates the bounding box of the entire manifold, which is stored
 * internally to short-cut Boolean operations. Ignores NaNs.
 */
void Manifold::Impl::CalculateBBox() {
  bBox_.min =
      reduce(vertPos_.begin(), vertPos_.end(),
             vec3(std::numeric_limits<double>::infinity()), [](auto a, auto b) {
               if (std::isnan(a.x)) return b;
               if (std::isnan(b.x)) return a;
               return la::min(a, b);
             });
  bBox_.max = reduce(vertPos_.begin(), vertPos_.end(),
                     vec3(-std::numeric_limits<double>::infinity()),
                     [](auto a, auto b) {
                       if (std::isnan(a.x)) return b;
                       if (std::isnan(b.x)) return a;
                       return la::max(a, b);
                     });
}

/**
 * Determines if all verts are finite. Checking just the bounding box dimensions
 * is insufficient as it ignores NaNs.
 */
bool Manifold::Impl::IsFinite() const {
  return transform_reduce(
      vertPos_.begin(), vertPos_.end(), true,
      [](bool a, bool b) { return a && b; },
      [](auto v) { return la::all(la::isfinite(v)); });
}

/**
 * Checks that the input triVerts array has all indices inside bounds of the
 * vertPos_ array.
 */
bool Manifold::Impl::IsIndexInBounds(VecView<const ivec3> triVerts) const {
  ivec2 minmax = transform_reduce(
      triVerts.begin(), triVerts.end(),
      ivec2(std::numeric_limits<int>::max(), std::numeric_limits<int>::min()),
      [](auto a, auto b) {
        a[0] = std::min(a[0], b[0]);
        a[1] = std::max(a[1], b[1]);
        return a;
      },
      [](auto tri) {
        return ivec2(std::min(tri[0], std::min(tri[1], tri[2])),
                     std::max(tri[0], std::max(tri[1], tri[2])));
      });

  return minmax[0] >= 0 && minmax[1] < static_cast<int>(NumVert());
}

/*
 * Returns the minimum gap between two manifolds. Returns a double between
 * 0 and searchLength.
 */
double Manifold::Impl::MinGap(const Manifold::Impl& other,
                              double searchLength) const {
  ZoneScoped;
  Vec<Box> faceBoxOther;
  Vec<uint32_t> faceMortonOther;

  other.GetFaceBoxMorton(faceBoxOther, faceMortonOther);

  transform(faceBoxOther.begin(), faceBoxOther.end(), faceBoxOther.begin(),
            [searchLength](const Box& box) {
              return Box(box.min - vec3(searchLength),
                         box.max + vec3(searchLength));
            });

  SparseIndices collisions = collider_.Collisions(faceBoxOther.cview());

  double minDistanceSquared = transform_reduce(
      countAt(0_uz), countAt(collisions.size()), searchLength * searchLength,
      [](double a, double b) { return std::min(a, b); },
      [&collisions, this, &other](int i) {
        const int tri = collisions.Get(i, 1);
        const int triOther = collisions.Get(i, 0);

        std::array<vec3, 3> p;
        std::array<vec3, 3> q;

        for (const int j : {0, 1, 2}) {
          p[j] = vertPos_[halfedge_[3 * tri + j].startVert];
          q[j] = other.vertPos_[other.halfedge_[3 * triOther + j].startVert];
        }

        return DistanceTriangleTriangleSquared(p, q);
      });

  return sqrt(minDistanceSquared);
};

}  // namespace manifold
