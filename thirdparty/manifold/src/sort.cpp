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

#include <atomic>
#include <set>

#include "./impl.h"
#include "./parallel.h"

namespace {
using namespace manifold;

constexpr uint32_t kNoCode = 0xFFFFFFFFu;

uint32_t MortonCode(vec3 position, Box bBox) {
  // Unreferenced vertices are marked NaN, and this will sort them to the end
  // (the Morton code only uses the first 30 of 32 bits).
  if (std::isnan(position.x)) return kNoCode;

  return Collider::MortonCode(position, bBox);
}

struct Reindex {
  VecView<const int> indexInv;

  void operator()(Halfedge& edge) {
    if (edge.startVert < 0) return;
    edge.startVert = indexInv[edge.startVert];
    edge.endVert = indexInv[edge.endVert];
  }
};

struct MarkProp {
  VecView<int> keep;

  void operator()(ivec3 triProp) {
    for (const int i : {0, 1, 2}) {
      reinterpret_cast<std::atomic<int>*>(&keep[triProp[i]])
          ->store(1, std::memory_order_relaxed);
    }
  }
};

struct ReindexProps {
  VecView<const int> old2new;

  void operator()(ivec3& triProp) {
    for (const int i : {0, 1, 2}) {
      triProp[i] = old2new[triProp[i]];
    }
  }
};

struct ReindexFace {
  VecView<Halfedge> halfedge;
  VecView<vec4> halfedgeTangent;
  VecView<const Halfedge> oldHalfedge;
  VecView<const vec4> oldHalfedgeTangent;
  VecView<const int> faceNew2Old;
  VecView<const int> faceOld2New;

  void operator()(int newFace) {
    const int oldFace = faceNew2Old[newFace];
    for (const int i : {0, 1, 2}) {
      const int oldEdge = 3 * oldFace + i;
      Halfedge edge = oldHalfedge[oldEdge];
      const int pairedFace = edge.pairedHalfedge / 3;
      const int offset = edge.pairedHalfedge - 3 * pairedFace;
      edge.pairedHalfedge = 3 * faceOld2New[pairedFace] + offset;
      const int newEdge = 3 * newFace + i;
      halfedge[newEdge] = edge;
      if (!oldHalfedgeTangent.empty()) {
        halfedgeTangent[newEdge] = oldHalfedgeTangent[oldEdge];
      }
    }
  }
};

template <typename Precision, typename I>
bool MergeMeshGLP(MeshGLP<Precision, I>& mesh) {
  ZoneScoped;
  std::multiset<std::pair<int, int>> openEdges;

  std::vector<int> merge(mesh.NumVert());
  std::iota(merge.begin(), merge.end(), 0);
  for (size_t i = 0; i < mesh.mergeFromVert.size(); ++i) {
    merge[mesh.mergeFromVert[i]] = mesh.mergeToVert[i];
  }

  const auto numVert = mesh.NumVert();
  const auto numTri = mesh.NumTri();
  const int next[3] = {1, 2, 0};
  for (size_t tri = 0; tri < numTri; ++tri) {
    for (int i : {0, 1, 2}) {
      auto edge = std::make_pair(merge[mesh.triVerts[3 * tri + next[i]]],
                                 merge[mesh.triVerts[3 * tri + i]]);
      auto it = openEdges.find(edge);
      if (it == openEdges.end()) {
        std::swap(edge.first, edge.second);
        openEdges.insert(edge);
      } else {
        openEdges.erase(it);
      }
    }
  }
  if (openEdges.empty()) {
    return false;
  }

  const auto numOpenVert = openEdges.size();
  Vec<int> openVerts(numOpenVert);
  int i = 0;
  for (const auto& edge : openEdges) {
    const int vert = edge.first;
    openVerts[i++] = vert;
  }

  Vec<Precision> vertPropD(mesh.vertProperties);
  Box bBox;
  for (const int i : {0, 1, 2}) {
    auto iPos =
        StridedRange(vertPropD.begin() + i, vertPropD.end(), mesh.numProp);
    auto minMax = manifold::transform_reduce(
        iPos.begin(), iPos.end(),
        std::make_pair(std::numeric_limits<double>::infinity(),
                       -std::numeric_limits<double>::infinity()),
        [](auto a, auto b) {
          return std::make_pair(std::min(a.first, b.first),
                                std::max(a.second, b.second));
        },
        [](double f) { return std::make_pair(f, f); });
    bBox.min[i] = minMax.first;
    bBox.max[i] = minMax.second;
  }

  const double tolerance = std::max(static_cast<double>(mesh.tolerance),
                                    (std::is_same<Precision, float>::value
                                         ? std::numeric_limits<float>::epsilon()
                                         : kPrecision) *
                                        bBox.Scale());

  auto policy = autoPolicy(numOpenVert, 1e5);
  Vec<Box> vertBox(numOpenVert);
  Vec<uint32_t> vertMorton(numOpenVert);

  for_each_n(policy, countAt(0), numOpenVert,
             [&vertMorton, &vertBox, &openVerts, &bBox, &mesh,
              tolerance](const int i) {
               int vert = openVerts[i];

               const vec3 center(mesh.vertProperties[mesh.numProp * vert],
                                 mesh.vertProperties[mesh.numProp * vert + 1],
                                 mesh.vertProperties[mesh.numProp * vert + 2]);

               vertBox[i].min = center - tolerance / 2.0;
               vertBox[i].max = center + tolerance / 2.0;

               vertMorton[i] = MortonCode(center, bBox);
             });

  Vec<int> vertNew2Old(numOpenVert);
  sequence(vertNew2Old.begin(), vertNew2Old.end());

  stable_sort(vertNew2Old.begin(), vertNew2Old.end(),
              [&vertMorton](const int& a, const int& b) {
                return vertMorton[a] < vertMorton[b];
              });

  Permute(vertMorton, vertNew2Old);
  Permute(vertBox, vertNew2Old);
  Permute(openVerts, vertNew2Old);
  Collider collider(vertBox, vertMorton);
  SparseIndices toMerge = collider.Collisions<true>(vertBox.cview());

  UnionFind<> uf(numVert);
  for (size_t i = 0; i < mesh.mergeFromVert.size(); ++i) {
    uf.unionXY(static_cast<int>(mesh.mergeFromVert[i]),
               static_cast<int>(mesh.mergeToVert[i]));
  }
  for (size_t i = 0; i < toMerge.size(); ++i) {
    uf.unionXY(openVerts[toMerge.Get(i, false)],
               openVerts[toMerge.Get(i, true)]);
  }

  mesh.mergeToVert.clear();
  mesh.mergeFromVert.clear();
  for (size_t v = 0; v < numVert; ++v) {
    const size_t mergeTo = uf.find(v);
    if (mergeTo != v) {
      mesh.mergeFromVert.push_back(v);
      mesh.mergeToVert.push_back(mergeTo);
    }
  }

  return true;
}
}  // namespace

namespace manifold {

/**
 * Once halfedge_ has been filled in, this function can be called to create the
 * rest of the internal data structures. This function also removes the verts
 * and halfedges flagged for removal (NaN verts and -1 halfedges).
 */
void Manifold::Impl::Finish() {
  if (halfedge_.size() == 0) return;

  CalculateBBox();
  SetEpsilon(epsilon_);
  if (!bBox_.IsFinite()) {
    // Decimated out of existence - early out.
    MarkFailure(Error::NoError);
    return;
  }

  SortVerts();
  Vec<Box> faceBox;
  Vec<uint32_t> faceMorton;
  GetFaceBoxMorton(faceBox, faceMorton);
  SortFaces(faceBox, faceMorton);
  if (halfedge_.size() == 0) return;
  CompactProps();

  DEBUG_ASSERT(halfedge_.size() % 6 == 0, topologyErr,
               "Not an even number of faces after sorting faces!");

#ifdef MANIFOLD_DEBUG
  auto MaxOrMinus = [](int a, int b) {
    return std::min(a, b) < 0 ? -1 : std::max(a, b);
  };
  int face = 0;
  Halfedge extrema = {0, 0, 0};
  for (size_t i = 0; i < halfedge_.size(); i++) {
    Halfedge e = halfedge_[i];
    if (!e.IsForward()) std::swap(e.startVert, e.endVert);
    extrema.startVert = std::min(extrema.startVert, e.startVert);
    extrema.endVert = std::min(extrema.endVert, e.endVert);
    extrema.pairedHalfedge =
        MaxOrMinus(extrema.pairedHalfedge, e.pairedHalfedge);
    face = MaxOrMinus(face, i / 3);
  }
  DEBUG_ASSERT(extrema.startVert >= 0, topologyErr,
               "Vertex index is negative!");
  DEBUG_ASSERT(extrema.endVert < static_cast<int>(NumVert()), topologyErr,
               "Vertex index exceeds number of verts!");
  DEBUG_ASSERT(extrema.pairedHalfedge >= 0, topologyErr,
               "Halfedge index is negative!");
  DEBUG_ASSERT(extrema.pairedHalfedge < 2 * static_cast<int>(NumEdge()),
               topologyErr, "Halfedge index exceeds number of halfedges!");
  DEBUG_ASSERT(face >= 0, topologyErr, "Face index is negative!");
  DEBUG_ASSERT(face < static_cast<int>(NumTri()), topologyErr,
               "Face index exceeds number of faces!");
#endif

  DEBUG_ASSERT(meshRelation_.triRef.size() == NumTri() ||
                   meshRelation_.triRef.size() == 0,
               logicErr, "Mesh Relation doesn't fit!");
  DEBUG_ASSERT(faceNormal_.size() == NumTri() || faceNormal_.size() == 0,
               logicErr,
               "faceNormal size = " + std::to_string(faceNormal_.size()) +
                   ", NumTri = " + std::to_string(NumTri()));
  CalculateNormals();
  collider_ = Collider(faceBox, faceMorton);

  DEBUG_ASSERT(Is2Manifold(), logicErr, "mesh is not 2-manifold!");
}

/**
 * Sorts the vertices according to their Morton code.
 */
void Manifold::Impl::SortVerts() {
  ZoneScoped;
  const auto numVert = NumVert();
  Vec<uint32_t> vertMorton(numVert);
  auto policy = autoPolicy(numVert, 1e5);
  for_each_n(policy, countAt(0), numVert, [this, &vertMorton](const int vert) {
    vertMorton[vert] = MortonCode(vertPos_[vert], bBox_);
  });

  Vec<int> vertNew2Old(numVert);
  sequence(vertNew2Old.begin(), vertNew2Old.end());

  stable_sort(vertNew2Old.begin(), vertNew2Old.end(),
              [&vertMorton](const int& a, const int& b) {
                return vertMorton[a] < vertMorton[b];
              });

  ReindexVerts(vertNew2Old, numVert);

  // Verts were flagged for removal with NaNs and assigned kNoCode to sort
  // them to the end, which allows them to be removed.
  const auto newNumVert = std::find_if(vertNew2Old.begin(), vertNew2Old.end(),
                                       [&vertMorton](const int vert) {
                                         return vertMorton[vert] == kNoCode;
                                       }) -
                          vertNew2Old.begin();

  vertNew2Old.resize(newNumVert);
  Permute(vertPos_, vertNew2Old);

  if (vertNormal_.size() == numVert) {
    Permute(vertNormal_, vertNew2Old);
  }
}

/**
 * Updates the halfedges to point to new vert indices based on a mapping,
 * vertNew2Old. This may be a subset, so the total number of original verts is
 * also given.
 */
void Manifold::Impl::ReindexVerts(const Vec<int>& vertNew2Old,
                                  size_t oldNumVert) {
  ZoneScoped;
  Vec<int> vertOld2New(oldNumVert);
  scatter(countAt(0), countAt(static_cast<int>(NumVert())), vertNew2Old.begin(),
          vertOld2New.begin());
  for_each(autoPolicy(oldNumVert, 1e5), halfedge_.begin(), halfedge_.end(),
           Reindex({vertOld2New}));
}

/**
 * Removes unreferenced property verts and reindexes triProperties.
 */
void Manifold::Impl::CompactProps() {
  ZoneScoped;
  if (meshRelation_.numProp == 0) return;

  const auto numVerts = meshRelation_.properties.size() / meshRelation_.numProp;
  Vec<int> keep(numVerts, 0);
  auto policy = autoPolicy(numVerts, 1e5);

  for_each(policy, meshRelation_.triProperties.cbegin(),
           meshRelation_.triProperties.cend(), MarkProp({keep}));
  Vec<int> propOld2New(numVerts + 1, 0);
  inclusive_scan(keep.begin(), keep.end(), propOld2New.begin() + 1);

  Vec<double> oldProp = meshRelation_.properties;
  const int numVertsNew = propOld2New[numVerts];
  const int numProp = meshRelation_.numProp;
  auto& properties = meshRelation_.properties;
  properties.resize(numProp * numVertsNew);
  for_each_n(
      policy, countAt(0), numVerts,
      [&properties, &oldProp, &propOld2New, &keep, &numProp](const int oldIdx) {
        if (keep[oldIdx] == 0) return;
        for (int p = 0; p < numProp; ++p) {
          properties[propOld2New[oldIdx] * numProp + p] =
              oldProp[oldIdx * numProp + p];
        }
      });
  for_each_n(policy, meshRelation_.triProperties.begin(), NumTri(),
             ReindexProps({propOld2New}));
}

/**
 * Fills the faceBox and faceMorton input with the bounding boxes and Morton
 * codes of the faces, respectively. The Morton code is based on the center of
 * the bounding box.
 */
void Manifold::Impl::GetFaceBoxMorton(Vec<Box>& faceBox,
                                      Vec<uint32_t>& faceMorton) const {
  ZoneScoped;
  faceBox.resize(NumTri());
  faceMorton.resize(NumTri());
  for_each_n(autoPolicy(NumTri(), 1e5), countAt(0), NumTri(),
             [this, &faceBox, &faceMorton](const int face) {
               // Removed tris are marked by all halfedges having pairedHalfedge
               // = -1, and this will sort them to the end (the Morton code only
               // uses the first 30 of 32 bits).
               if (halfedge_[3 * face].pairedHalfedge < 0) {
                 faceMorton[face] = kNoCode;
                 return;
               }

               vec3 center(0.0);

               for (const int i : {0, 1, 2}) {
                 const vec3 pos = vertPos_[halfedge_[3 * face + i].startVert];
                 center += pos;
                 faceBox[face].Union(pos);
               }
               center /= 3;

               faceMorton[face] = MortonCode(center, bBox_);
             });
}

/**
 * Sorts the faces of this manifold according to their input Morton code. The
 * bounding box and Morton code arrays are also sorted accordingly.
 */
void Manifold::Impl::SortFaces(Vec<Box>& faceBox, Vec<uint32_t>& faceMorton) {
  ZoneScoped;
  Vec<int> faceNew2Old(NumTri());
  sequence(faceNew2Old.begin(), faceNew2Old.end());

  stable_sort(faceNew2Old.begin(), faceNew2Old.end(),
              [&faceMorton](const int& a, const int& b) {
                return faceMorton[a] < faceMorton[b];
              });

  // Tris were flagged for removal with pairedHalfedge = -1 and assigned kNoCode
  // to sort them to the end, which allows them to be removed.
  const int newNumTri = std::find_if(faceNew2Old.begin(), faceNew2Old.end(),
                                     [&faceMorton](const int face) {
                                       return faceMorton[face] == kNoCode;
                                     }) -
                        faceNew2Old.begin();
  faceNew2Old.resize(newNumTri);

  Permute(faceMorton, faceNew2Old);
  Permute(faceBox, faceNew2Old);
  GatherFaces(faceNew2Old);
}

/**
 * Creates the halfedge_ vector for this manifold by copying a set of faces from
 * another manifold, given by oldHalfedge. Input faceNew2Old defines the old
 * faces to gather into this.
 */
void Manifold::Impl::GatherFaces(const Vec<int>& faceNew2Old) {
  ZoneScoped;
  const auto numTri = faceNew2Old.size();
  if (meshRelation_.triRef.size() == NumTri())
    Permute(meshRelation_.triRef, faceNew2Old);
  if (meshRelation_.triProperties.size() == NumTri())
    Permute(meshRelation_.triProperties, faceNew2Old);
  if (faceNormal_.size() == NumTri()) Permute(faceNormal_, faceNew2Old);

  Vec<Halfedge> oldHalfedge(std::move(halfedge_));
  Vec<vec4> oldHalfedgeTangent(std::move(halfedgeTangent_));
  Vec<int> faceOld2New(oldHalfedge.size() / 3);
  auto policy = autoPolicy(numTri, 1e5);
  scatter(countAt(0_uz), countAt(numTri), faceNew2Old.begin(),
          faceOld2New.begin());

  halfedge_.resize(3 * numTri);
  if (oldHalfedgeTangent.size() != 0) halfedgeTangent_.resize(3 * numTri);
  for_each_n(policy, countAt(0), numTri,
             ReindexFace({halfedge_, halfedgeTangent_, oldHalfedge,
                          oldHalfedgeTangent, faceNew2Old, faceOld2New}));
}

void Manifold::Impl::GatherFaces(const Impl& old, const Vec<int>& faceNew2Old) {
  ZoneScoped;
  const auto numTri = faceNew2Old.size();

  meshRelation_.triRef.resize(numTri);
  gather(faceNew2Old.begin(), faceNew2Old.end(),
         old.meshRelation_.triRef.begin(), meshRelation_.triRef.begin());

  for (const auto& pair : old.meshRelation_.meshIDtransform) {
    meshRelation_.meshIDtransform[pair.first] = pair.second;
  }

  if (old.meshRelation_.triProperties.size() > 0) {
    meshRelation_.triProperties.resize(numTri);
    gather(faceNew2Old.begin(), faceNew2Old.end(),
           old.meshRelation_.triProperties.begin(),
           meshRelation_.triProperties.begin());
    meshRelation_.numProp = old.meshRelation_.numProp;
    meshRelation_.properties = old.meshRelation_.properties;
  }

  if (old.faceNormal_.size() == old.NumTri()) {
    faceNormal_.resize(numTri);
    gather(faceNew2Old.begin(), faceNew2Old.end(), old.faceNormal_.begin(),
           faceNormal_.begin());
  }

  Vec<int> faceOld2New(old.NumTri());
  scatter(countAt(0_uz), countAt(numTri), faceNew2Old.begin(),
          faceOld2New.begin());

  halfedge_.resize(3 * numTri);
  if (old.halfedgeTangent_.size() != 0) halfedgeTangent_.resize(3 * numTri);
  for_each_n(autoPolicy(numTri, 1e5), countAt(0), numTri,
             ReindexFace({halfedge_, halfedgeTangent_, old.halfedge_,
                          old.halfedgeTangent_, faceNew2Old, faceOld2New}));
}

/**
 * Updates the mergeFromVert and mergeToVert vectors in order to create a
 * manifold solid. If the MeshGL is already manifold, no change will occur and
 * the function will return false. Otherwise, this will merge verts along open
 * edges within tolerance (the maximum of the MeshGL tolerance and the
 * baseline bounding-box tolerance), keeping any from the existing merge
 * vectors, and return true.
 *
 * There is no guarantee the result will be manifold - this is a best-effort
 * helper function designed primarily to aid in the case where a manifold
 * multi-material MeshGL was produced, but its merge vectors were lost due to
 * a round-trip through a file format. Constructing a Manifold from the result
 * will report an error status if it is not manifold.
 */
template <>
bool MeshGL::Merge() {
  return MergeMeshGLP(*this);
}

template <>
bool MeshGL64::Merge() {
  return MergeMeshGLP(*this);
}
}  // namespace manifold
