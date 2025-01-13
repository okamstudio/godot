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

#include <algorithm>
#include <atomic>
#include <map>

#include "./hashtable.h"
#include "./mesh_fixes.h"
#include "./parallel.h"
#include "./svd.h"

namespace {
using namespace manifold;

constexpr uint64_t kRemove = std::numeric_limits<uint64_t>::max();

void AtomicAddVec3(vec3& target, const vec3& add) {
  for (int i : {0, 1, 2}) {
    std::atomic<double>& tar =
        reinterpret_cast<std::atomic<double>&>(target[i]);
    double old_val = tar.load(std::memory_order_relaxed);
    while (!tar.compare_exchange_weak(old_val, old_val + add[i],
                                      std::memory_order_relaxed)) {
    }
  }
}

struct Transform4x3 {
  const mat3x4 transform;

  vec3 operator()(vec3 position) { return transform * vec4(position, 1.0); }
};

template <bool calculateTriNormal>
struct AssignNormals {
  VecView<vec3> faceNormal;
  VecView<vec3> vertNormal;
  VecView<const vec3> vertPos;
  VecView<const Halfedge> halfedges;

  void operator()(const int face) {
    vec3& triNormal = faceNormal[face];

    ivec3 triVerts;
    for (int i : {0, 1, 2}) triVerts[i] = halfedges[3 * face + i].startVert;

    vec3 edge[3];
    for (int i : {0, 1, 2}) {
      const int j = (i + 1) % 3;
      edge[i] = la::normalize(vertPos[triVerts[j]] - vertPos[triVerts[i]]);
    }

    if (calculateTriNormal) {
      triNormal = la::normalize(la::cross(edge[0], edge[1]));
      if (std::isnan(triNormal.x)) triNormal = vec3(0, 0, 1);
    }

    // corner angles
    vec3 phi;
    double dot = -la::dot(edge[2], edge[0]);
    phi[0] = dot >= 1 ? 0 : (dot <= -1 ? kPi : std::acos(dot));
    dot = -la::dot(edge[0], edge[1]);
    phi[1] = dot >= 1 ? 0 : (dot <= -1 ? kPi : std::acos(dot));
    phi[2] = kPi - phi[0] - phi[1];

    // assign weighted sum
    for (int i : {0, 1, 2}) {
      AtomicAddVec3(vertNormal[triVerts[i]], phi[i] * triNormal);
    }
  }
};

struct UpdateMeshID {
  const HashTableD<uint32_t> meshIDold2new;

  void operator()(TriRef& ref) { ref.meshID = meshIDold2new[ref.meshID]; }
};

struct CoplanarEdge {
  VecView<std::pair<int, int>> face2face;
  VecView<double> triArea;
  VecView<const Halfedge> halfedge;
  VecView<const vec3> vertPos;
  VecView<const TriRef> triRef;
  VecView<const ivec3> triProp;
  const int numProp;
  const double epsilon;
  const double tolerance;

  void operator()(const int edgeIdx) {
    const Halfedge edge = halfedge[edgeIdx];
    const Halfedge pair = halfedge[edge.pairedHalfedge];
    const int edgeFace = edgeIdx / 3;
    const int pairFace = edge.pairedHalfedge / 3;

    if (triRef[edgeFace].meshID != triRef[pairFace].meshID) return;

    const vec3 base = vertPos[edge.startVert];
    const int baseNum = edgeIdx - 3 * edgeFace;
    const int jointNum = edge.pairedHalfedge - 3 * pairFace;

    if (numProp > 0) {
      if (triProp[edgeFace][baseNum] != triProp[pairFace][Next3(jointNum)] ||
          triProp[edgeFace][Next3(baseNum)] != triProp[pairFace][jointNum])
        return;
    }

    if (!edge.IsForward()) return;

    const int edgeNum = baseNum == 0 ? 2 : baseNum - 1;
    const int pairNum = jointNum == 0 ? 2 : jointNum - 1;
    const vec3 jointVec = vertPos[pair.startVert] - base;
    const vec3 edgeVec =
        vertPos[halfedge[3 * edgeFace + edgeNum].startVert] - base;
    const vec3 pairVec =
        vertPos[halfedge[3 * pairFace + pairNum].startVert] - base;

    const double length = std::max(la::length(jointVec), la::length(edgeVec));
    const double lengthPair =
        std::max(la::length(jointVec), la::length(pairVec));
    vec3 normal = la::cross(jointVec, edgeVec);
    const double area = la::length(normal);
    const double areaPair = la::length(la::cross(pairVec, jointVec));

    // make sure we only write this once
    if (edgeIdx % 3 == 0) triArea[edgeFace] = area;
    // Don't link degenerate triangles
    if (area < length * epsilon || areaPair < lengthPair * epsilon) return;

    const double volume = std::abs(la::dot(normal, pairVec));
    // Only operate on coplanar triangles
    if (volume > std::max(area, areaPair) * tolerance) return;

    face2face[edgeIdx] = std::make_pair(edgeFace, pairFace);
  }
};

struct CheckCoplanarity {
  VecView<int> comp2tri;
  VecView<const Halfedge> halfedge;
  VecView<const vec3> vertPos;
  std::vector<int>* components;
  const double tolerance;

  void operator()(int tri) {
    const int component = (*components)[tri];
    const int referenceTri =
        reinterpret_cast<std::atomic<int>*>(&comp2tri[component])
            ->load(std::memory_order_relaxed);
    if (referenceTri < 0 || referenceTri == tri) return;

    const vec3 origin = vertPos[halfedge[3 * referenceTri].startVert];
    const vec3 normal = la::normalize(
        la::cross(vertPos[halfedge[3 * referenceTri + 1].startVert] - origin,
                  vertPos[halfedge[3 * referenceTri + 2].startVert] - origin));

    for (const int i : {0, 1, 2}) {
      const vec3 vert = vertPos[halfedge[3 * tri + i].startVert];
      // If any component vertex is not coplanar with the component's reference
      // triangle, unmark the entire component so that none of its triangles are
      // marked coplanar.
      if (std::abs(la::dot(normal, vert - origin)) > tolerance) {
        reinterpret_cast<std::atomic<int>*>(&comp2tri[component])
            ->store(-1, std::memory_order_relaxed);
        break;
      }
    }
  }
};

int GetLabels(std::vector<int>& components,
              const Vec<std::pair<int, int>>& edges, int numNodes) {
  UnionFind<> uf(numNodes);
  for (auto edge : edges) {
    if (edge.first == -1 || edge.second == -1) continue;
    uf.unionXY(edge.first, edge.second);
  }

  return uf.connectedComponents(components);
}

void DedupePropVerts(manifold::Vec<ivec3>& triProp,
                     const Vec<std::pair<int, int>>& vert2vert,
                     size_t numPropVert) {
  ZoneScoped;
  std::vector<int> vertLabels;
  const int numLabels = GetLabels(vertLabels, vert2vert, numPropVert);

  std::vector<int> label2vert(numLabels);
  for (size_t v = 0; v < numPropVert; ++v) label2vert[vertLabels[v]] = v;
  for (auto& prop : triProp)
    for (int i : {0, 1, 2}) prop[i] = label2vert[vertLabels[prop[i]]];
}
}  // namespace

namespace manifold {

std::atomic<uint32_t> Manifold::Impl::meshIDCounter_(1);

uint32_t Manifold::Impl::ReserveIDs(uint32_t n) {
  return Manifold::Impl::meshIDCounter_.fetch_add(n, std::memory_order_relaxed);
}

/**
 * Create either a unit tetrahedron, cube or octahedron. The cube is in the
 * first octant, while the others are symmetric about the origin.
 */
Manifold::Impl::Impl(Shape shape, const mat3x4 m) {
  std::vector<vec3> vertPos;
  std::vector<ivec3> triVerts;
  switch (shape) {
    case Shape::Tetrahedron:
      vertPos = {{-1.0, -1.0, 1.0},
                 {-1.0, 1.0, -1.0},
                 {1.0, -1.0, -1.0},
                 {1.0, 1.0, 1.0}};
      triVerts = {{2, 0, 1}, {0, 3, 1}, {2, 3, 0}, {3, 2, 1}};
      break;
    case Shape::Cube:
      vertPos = {{0.0, 0.0, 0.0},  //
                 {0.0, 0.0, 1.0},  //
                 {0.0, 1.0, 0.0},  //
                 {0.0, 1.0, 1.0},  //
                 {1.0, 0.0, 0.0},  //
                 {1.0, 0.0, 1.0},  //
                 {1.0, 1.0, 0.0},  //
                 {1.0, 1.0, 1.0}};
      triVerts = {{1, 0, 4}, {2, 4, 0},  //
                  {1, 3, 0}, {3, 1, 5},  //
                  {3, 2, 0}, {3, 7, 2},  //
                  {5, 4, 6}, {5, 1, 4},  //
                  {6, 4, 2}, {7, 6, 2},  //
                  {7, 3, 5}, {7, 5, 6}};
      break;
    case Shape::Octahedron:
      vertPos = {{1.0, 0.0, 0.0},   //
                 {-1.0, 0.0, 0.0},  //
                 {0.0, 1.0, 0.0},   //
                 {0.0, -1.0, 0.0},  //
                 {0.0, 0.0, 1.0},   //
                 {0.0, 0.0, -1.0}};
      triVerts = {{0, 2, 4}, {1, 5, 3},  //
                  {2, 1, 4}, {3, 5, 0},  //
                  {1, 3, 4}, {0, 5, 2},  //
                  {3, 0, 4}, {2, 5, 1}};
      break;
  }
  vertPos_ = vertPos;
  for (auto& v : vertPos_) v = m * vec4(v, 1.0);
  CreateHalfedges(triVerts);
  Finish();
  InitializeOriginal();
  CreateFaces();
}

void Manifold::Impl::RemoveUnreferencedVerts() {
  ZoneScoped;
  Vec<int> vertOld2New(NumVert(), 0);
  auto policy = autoPolicy(NumVert(), 1e5);
  for_each(policy, halfedge_.cbegin(), halfedge_.cend(),
           [&vertOld2New](Halfedge h) {
             reinterpret_cast<std::atomic<int>*>(&vertOld2New[h.startVert])
                 ->store(1, std::memory_order_relaxed);
           });

  const Vec<vec3> oldVertPos = vertPos_;

  Vec<size_t> tmpBuffer(oldVertPos.size());
  auto vertIdIter = TransformIterator(countAt(0_uz), [&vertOld2New](size_t i) {
    if (vertOld2New[i] > 0) return i;
    return std::numeric_limits<size_t>::max();
  });

  auto next =
      copy_if(vertIdIter, vertIdIter + tmpBuffer.size(), tmpBuffer.begin(),
              [](size_t v) { return v != std::numeric_limits<size_t>::max(); });
  if (next == tmpBuffer.end()) return;

  gather(tmpBuffer.begin(), next, oldVertPos.begin(), vertPos_.begin());

  vertPos_.resize(std::distance(tmpBuffer.begin(), next));

  exclusive_scan(vertOld2New.begin(), vertOld2New.end(), vertOld2New.begin());

  for_each(policy, halfedge_.begin(), halfedge_.end(),
           [&vertOld2New](Halfedge& h) {
             h.startVert = vertOld2New[h.startVert];
             h.endVert = vertOld2New[h.endVert];
           });
}

void Manifold::Impl::InitializeOriginal(bool keepFaceID) {
  const int meshID = ReserveIDs(1);
  meshRelation_.originalID = meshID;
  auto& triRef = meshRelation_.triRef;
  triRef.resize(NumTri());
  for_each_n(autoPolicy(NumTri(), 1e5), countAt(0), NumTri(),
             [meshID, keepFaceID, &triRef](const int tri) {
               triRef[tri] = {meshID, meshID, tri,
                              keepFaceID ? triRef[tri].faceID : tri};
             });
  meshRelation_.meshIDtransform.clear();
  meshRelation_.meshIDtransform[meshID] = {meshID};
}

void Manifold::Impl::CreateFaces() {
  ZoneScoped;
  Vec<std::pair<int, int>> face2face(halfedge_.size(), {-1, -1});
  Vec<std::pair<int, int>> vert2vert(halfedge_.size(), {-1, -1});
  Vec<double> triArea(NumTri());

  const size_t numProp = NumProp();
  if (numProp > 0) {
    for_each_n(
        autoPolicy(halfedge_.size(), 1e4), countAt(0), halfedge_.size(),
        [&vert2vert, numProp, this](const int edgeIdx) {
          const Halfedge edge = halfedge_[edgeIdx];
          const Halfedge pair = halfedge_[edge.pairedHalfedge];
          const int edgeFace = edgeIdx / 3;
          const int pairFace = edge.pairedHalfedge / 3;

          if (meshRelation_.triRef[edgeFace].meshID !=
              meshRelation_.triRef[pairFace].meshID)
            return;

          const int baseNum = edgeIdx - 3 * edgeFace;
          const int jointNum = edge.pairedHalfedge - 3 * pairFace;

          const int prop0 = meshRelation_.triProperties[edgeFace][baseNum];
          const int prop1 =
              meshRelation_
                  .triProperties[pairFace][jointNum == 2 ? 0 : jointNum + 1];
          if (prop0 == prop1) return;

          bool propEqual = true;
          for (size_t p = 0; p < numProp; ++p) {
            if (meshRelation_.properties[numProp * prop0 + p] !=
                meshRelation_.properties[numProp * prop1 + p]) {
              propEqual = false;
              break;
            }
          }
          if (propEqual) {
            vert2vert[edgeIdx] = std::make_pair(prop0, prop1);
          }
        });
    DedupePropVerts(meshRelation_.triProperties, vert2vert, NumPropVert());
  }

  for_each_n(autoPolicy(halfedge_.size(), 1e4), countAt(0), halfedge_.size(),
             CoplanarEdge({face2face, triArea, halfedge_, vertPos_,
                           meshRelation_.triRef, meshRelation_.triProperties,
                           meshRelation_.numProp, epsilon_, tolerance_}));

  std::vector<int> components;
  const int numComponent = GetLabels(components, face2face, NumTri());

  Vec<int> comp2tri(numComponent, -1);
  for (size_t tri = 0; tri < NumTri(); ++tri) {
    const int comp = components[tri];
    const int current = comp2tri[comp];
    if (current < 0 || triArea[tri] > triArea[current]) {
      comp2tri[comp] = tri;
      triArea[comp] = triArea[tri];
    }
  }

  for_each_n(autoPolicy(halfedge_.size(), 1e4), countAt(0), NumTri(),
             CheckCoplanarity(
                 {comp2tri, halfedge_, vertPos_, &components, tolerance_}));

  Vec<TriRef>& triRef = meshRelation_.triRef;
  for (size_t tri = 0; tri < NumTri(); ++tri) {
    const int referenceTri = comp2tri[components[tri]];
    if (referenceTri >= 0) {
      triRef[tri].faceID = referenceTri;
    }
  }
}

/**
 * Create the halfedge_ data structure from an input triVerts array like Mesh.
 */
void Manifold::Impl::CreateHalfedges(const Vec<ivec3>& triVerts) {
  ZoneScoped;
  const size_t numTri = triVerts.size();
  const int numHalfedge = 3 * numTri;
  // drop the old value first to avoid copy
  halfedge_.resize(0);
  halfedge_.resize(numHalfedge);
  Vec<uint64_t> edge(numHalfedge);
  Vec<int> ids(numHalfedge);
  auto policy = autoPolicy(numTri, 1e5);
  sequence(ids.begin(), ids.end());
  for_each_n(policy, countAt(0), numTri,
             [this, &edge, &triVerts](const int tri) {
               const ivec3& verts = triVerts[tri];
               for (const int i : {0, 1, 2}) {
                 const int j = (i + 1) % 3;
                 const int e = 3 * tri + i;
                 halfedge_[e] = {verts[i], verts[j], -1};
                 // Sort the forward halfedges in front of the backward ones
                 // by setting the highest-order bit.
                 edge[e] = uint64_t(verts[i] < verts[j] ? 1 : 0) << 63 |
                           ((uint64_t)std::min(verts[i], verts[j])) << 32 |
                           std::max(verts[i], verts[j]);
               }
             });
  // Stable sort is required here so that halfedges from the same face are
  // paired together (the triangles were created in face order). In some
  // degenerate situations the triangulator can add the same internal edge in
  // two different faces, causing this edge to not be 2-manifold. These are
  // fixed by duplicating verts in SimplifyTopology.
  stable_sort(ids.begin(), ids.end(), [&edge](const int& a, const int& b) {
    return edge[a] < edge[b];
  });

  // Mark opposed triangles for removal
  const int numEdge = numHalfedge / 2;
  for (int i = 0; i < numEdge; ++i) {
    const int pair0 = ids[i];
    Halfedge h0 = halfedge_[pair0];
    int k = i + numEdge;
    while (1) {
      const int pair1 = ids[k];
      Halfedge h1 = halfedge_[pair1];
      if (h0.startVert != h1.endVert || h0.endVert != h1.startVert) break;
      if (halfedge_[NextHalfedge(pair0)].endVert ==
          halfedge_[NextHalfedge(pair1)].endVert) {
        // Reorder so that remaining edges pair up
        if (k != i + numEdge) std::swap(ids[i + numEdge], ids[k]);
        break;
      }
      ++k;
      if (k >= numHalfedge) break;
    }
  }

  // Once sorted, the first half of the range is the forward halfedges, which
  // correspond to their backward pair at the same offset in the second half
  // of the range.
  for_each_n(policy, countAt(0), numEdge, [this, &ids, numEdge](int i) {
    const int pair0 = ids[i];
    const int pair1 = ids[i + numEdge];
    halfedge_[pair0].pairedHalfedge = pair1;
    halfedge_[pair1].pairedHalfedge = pair0;
  });

  // When opposed triangles are removed, they may strand unreferenced verts.
  RemoveUnreferencedVerts();
}

/**
 * Does a full recalculation of the face bounding boxes, including updating
 * the collider, but does not resort the faces.
 */
void Manifold::Impl::Update() {
  CalculateBBox();
  Vec<Box> faceBox;
  Vec<uint32_t> faceMorton;
  GetFaceBoxMorton(faceBox, faceMorton);
  collider_.UpdateBoxes(faceBox);
}

void Manifold::Impl::MarkFailure(Error status) {
  bBox_ = Box();
  vertPos_.resize(0);
  halfedge_.resize(0);
  vertNormal_.resize(0);
  faceNormal_.resize(0);
  halfedgeTangent_.resize(0);
  meshRelation_ = MeshRelationD();
  status_ = status;
}

void Manifold::Impl::Warp(std::function<void(vec3&)> warpFunc) {
  WarpBatch([&warpFunc](VecView<vec3> vecs) {
    for_each(ExecutionPolicy::Seq, vecs.begin(), vecs.end(), warpFunc);
  });
}

void Manifold::Impl::WarpBatch(std::function<void(VecView<vec3>)> warpFunc) {
  warpFunc(vertPos_.view());
  CalculateBBox();
  if (!IsFinite()) {
    MarkFailure(Error::NonFiniteVertex);
    return;
  }
  Update();
  faceNormal_.resize(0);  // force recalculation of triNormal
  CalculateNormals();
  SetEpsilon();
  Finish();
  CreateFaces();
  meshRelation_.originalID = -1;
}

Manifold::Impl Manifold::Impl::Transform(const mat3x4& transform_) const {
  ZoneScoped;
  if (transform_ == mat3x4(la::identity)) return *this;
  auto policy = autoPolicy(NumVert());
  Impl result;
  if (status_ != Manifold::Error::NoError) {
    result.status_ = status_;
    return result;
  }
  if (!all(la::isfinite(transform_))) {
    result.MarkFailure(Error::NonFiniteVertex);
    return result;
  }
  result.collider_ = collider_;
  result.meshRelation_ = meshRelation_;
  result.epsilon_ = epsilon_;
  result.tolerance_ = tolerance_;
  result.bBox_ = bBox_;
  result.halfedge_ = halfedge_;
  result.halfedgeTangent_.resize(halfedgeTangent_.size());

  result.meshRelation_.originalID = -1;
  for (auto& m : result.meshRelation_.meshIDtransform) {
    m.second.transform = transform_ * Mat4(m.second.transform);
  }

  result.vertPos_.resize(NumVert());
  result.faceNormal_.resize(faceNormal_.size());
  result.vertNormal_.resize(vertNormal_.size());
  transform(vertPos_.begin(), vertPos_.end(), result.vertPos_.begin(),
            Transform4x3({transform_}));

  mat3 normalTransform = NormalTransform(transform_);
  transform(faceNormal_.begin(), faceNormal_.end(), result.faceNormal_.begin(),
            TransformNormals({normalTransform}));
  transform(vertNormal_.begin(), vertNormal_.end(), result.vertNormal_.begin(),
            TransformNormals({normalTransform}));

  const bool invert = la::determinant(mat3(transform_)) < 0;

  if (halfedgeTangent_.size() > 0) {
    for_each_n(policy, countAt(0), halfedgeTangent_.size(),
               TransformTangents({result.halfedgeTangent_, 0, mat3(transform_),
                                  invert, halfedgeTangent_, halfedge_}));
  }

  if (invert) {
    for_each_n(policy, countAt(0), result.NumTri(),
               FlipTris({result.halfedge_}));
  }

  // This optimization does a cheap collider update if the transform is
  // axis-aligned.
  if (!result.collider_.Transform(transform_)) result.Update();

  result.CalculateBBox();
  // Scale epsilon by the norm of the 3x3 portion of the transform.
  result.epsilon_ *= SpectralNorm(mat3(transform_));
  // Maximum of inherited epsilon loss and translational epsilon loss.
  result.SetEpsilon(result.epsilon_);
  return result;
}

/**
 * Sets epsilon based on the bounding box, and limits its minimum value
 * by the optional input.
 */
void Manifold::Impl::SetEpsilon(double minEpsilon, bool useSingle) {
  epsilon_ = MaxEpsilon(minEpsilon, bBox_);
  double minTol = epsilon_;
  if (useSingle)
    minTol =
        std::max(minTol, std::numeric_limits<float>::epsilon() * bBox_.Scale());
  tolerance_ = std::max(tolerance_, minTol);
}

/**
 * If face normals are already present, this function uses them to compute
 * vertex normals (angle-weighted pseudo-normals); otherwise it also computes
 * the face normals. Face normals are only calculated when needed because
 * nearly degenerate faces will accrue rounding error, while the Boolean can
 * retain their original normal, which is more accurate and can help with
 * merging coplanar faces.
 *
 * If the face normals have been invalidated by an operation like Warp(),
 * ensure you do faceNormal_.resize(0) before calling this function to force
 * recalculation.
 */
void Manifold::Impl::CalculateNormals() {
  ZoneScoped;
  vertNormal_.resize(NumVert());
  auto policy = autoPolicy(NumTri(), 1e4);
  fill(vertNormal_.begin(), vertNormal_.end(), vec3(0.0));
  bool calculateTriNormal = false;
  if (faceNormal_.size() != NumTri()) {
    faceNormal_.resize(NumTri());
    calculateTriNormal = true;
  }
  if (calculateTriNormal)
    for_each_n(
        policy, countAt(0), NumTri(),
        AssignNormals<true>({faceNormal_, vertNormal_, vertPos_, halfedge_}));
  else
    for_each_n(
        policy, countAt(0), NumTri(),
        AssignNormals<false>({faceNormal_, vertNormal_, vertPos_, halfedge_}));
  for_each(policy, vertNormal_.begin(), vertNormal_.end(),
           [](vec3& v) { v = SafeNormalize(v); });
}

/**
 * Remaps all the contained meshIDs to new unique values to represent new
 * instances of these meshes.
 */
void Manifold::Impl::IncrementMeshIDs() {
  HashTable<uint32_t> meshIDold2new(meshRelation_.meshIDtransform.size() * 2);
  // Update keys of the transform map
  std::map<int, Relation> oldTransforms;
  std::swap(meshRelation_.meshIDtransform, oldTransforms);
  const int numMeshIDs = oldTransforms.size();
  int nextMeshID = ReserveIDs(numMeshIDs);
  for (const auto& pair : oldTransforms) {
    meshIDold2new.D().Insert(pair.first, nextMeshID);
    meshRelation_.meshIDtransform[nextMeshID++] = pair.second;
  }

  const size_t numTri = NumTri();
  for_each_n(autoPolicy(numTri, 1e5), meshRelation_.triRef.begin(), numTri,
             UpdateMeshID({meshIDold2new.D()}));
}

/**
 * Returns a sparse array of the bounding box overlaps between the edges of
 * the input manifold, Q and the faces of this manifold. Returned indices only
 * point to forward halfedges.
 */
SparseIndices Manifold::Impl::EdgeCollisions(const Impl& Q,
                                             bool inverted) const {
  ZoneScoped;
  Vec<TmpEdge> edges = CreateTmpEdges(Q.halfedge_);
  const size_t numEdge = edges.size();
  Vec<Box> QedgeBB(numEdge);
  const auto& vertPos = Q.vertPos_;
  auto policy = autoPolicy(numEdge, 1e5);
  for_each_n(
      policy, countAt(0), numEdge, [&QedgeBB, &edges, &vertPos](const int e) {
        QedgeBB[e] = Box(vertPos[edges[e].first], vertPos[edges[e].second]);
      });

  SparseIndices q1p2(0);
  if (inverted)
    q1p2 = collider_.Collisions<false, true>(QedgeBB.cview());
  else
    q1p2 = collider_.Collisions<false, false>(QedgeBB.cview());

  if (inverted)
    for_each(policy, countAt(0_uz), countAt(q1p2.size()),
             ReindexEdge<true>({edges, q1p2}));
  else
    for_each(policy, countAt(0_uz), countAt(q1p2.size()),
             ReindexEdge<false>({edges, q1p2}));
  return q1p2;
}

/**
 * Returns a sparse array of the input vertices that project inside the XY
 * bounding boxes of the faces of this manifold.
 */
SparseIndices Manifold::Impl::VertexCollisionsZ(VecView<const vec3> vertsIn,
                                                bool inverted) const {
  ZoneScoped;
  if (inverted)
    return collider_.Collisions<false, true>(vertsIn);
  else
    return collider_.Collisions<false, false>(vertsIn);
}

}  // namespace manifold
