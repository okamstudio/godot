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

#pragma once
#include <map>

#include "./collider.h"
#include "./shared.h"
#include "./sparse.h"
#include "./vec.h"
#include "manifold/manifold.h"
#include "manifold/polygon.h"

namespace manifold {

/** @ingroup Private */
struct Manifold::Impl {
  struct Relation {
    int originalID = -1;
    mat3x4 transform = la::identity;
    bool backSide = false;
  };
  struct MeshRelationD {
    /// The originalID of this Manifold if it is an original; -1 otherwise.
    int originalID = -1;
    int numProp = 0;
    Vec<double> properties;
    std::map<int, Relation> meshIDtransform;
    Vec<TriRef> triRef;
    Vec<ivec3> triProperties;
  };
  struct BaryIndices {
    int tri, start4, end4;
  };

  Box bBox_;
  double epsilon_ = -1;
  double tolerance_ = -1;
  Error status_ = Error::NoError;
  Vec<vec3> vertPos_;
  Vec<Halfedge> halfedge_;
  Vec<vec3> vertNormal_;
  Vec<vec3> faceNormal_;
  Vec<vec4> halfedgeTangent_;
  MeshRelationD meshRelation_;
  Collider collider_;

  static std::atomic<uint32_t> meshIDCounter_;
  static uint32_t ReserveIDs(uint32_t);

  Impl() {}
  enum class Shape { Tetrahedron, Cube, Octahedron };
  Impl(Shape, const mat3x4 = la::identity);

  template <typename Precision, typename I>
  Impl(const MeshGLP<Precision, I>& meshGL) {
    const uint32_t numVert = meshGL.NumVert();
    const uint32_t numTri = meshGL.NumTri();

    if (meshGL.numProp < 3) {
      MarkFailure(Error::MissingPositionProperties);
      return;
    }

    if (meshGL.mergeFromVert.size() != meshGL.mergeToVert.size()) {
      MarkFailure(Error::MergeVectorsDifferentLengths);
      return;
    }

    if (!meshGL.runTransform.empty() &&
        12 * meshGL.runOriginalID.size() != meshGL.runTransform.size()) {
      MarkFailure(Error::TransformWrongLength);
      return;
    }

    if (!meshGL.runOriginalID.empty() && !meshGL.runIndex.empty() &&
        meshGL.runOriginalID.size() + 1 != meshGL.runIndex.size() &&
        meshGL.runOriginalID.size() != meshGL.runIndex.size()) {
      MarkFailure(Error::RunIndexWrongLength);
      return;
    }

    if (!meshGL.faceID.empty() && meshGL.faceID.size() != meshGL.NumTri()) {
      MarkFailure(Error::FaceIDWrongLength);
      return;
    }

    std::vector<int> prop2vert(numVert);
    std::iota(prop2vert.begin(), prop2vert.end(), 0);
    for (size_t i = 0; i < meshGL.mergeFromVert.size(); ++i) {
      const uint32_t from = meshGL.mergeFromVert[i];
      const uint32_t to = meshGL.mergeToVert[i];
      if (from >= numVert || to >= numVert) {
        MarkFailure(Error::MergeIndexOutOfBounds);
        return;
      }
      prop2vert[from] = to;
    }

    const auto numProp = meshGL.numProp - 3;
    meshRelation_.numProp = numProp;
    meshRelation_.properties.resize(meshGL.NumVert() * numProp);
    tolerance_ = meshGL.tolerance;
    // This will have unreferenced duplicate positions that will be removed by
    // Impl::RemoveUnreferencedVerts().
    vertPos_.resize(meshGL.NumVert());

    for (size_t i = 0; i < meshGL.NumVert(); ++i) {
      for (const int j : {0, 1, 2})
        vertPos_[i][j] = meshGL.vertProperties[meshGL.numProp * i + j];
      for (size_t j = 0; j < numProp; ++j)
        meshRelation_.properties[i * numProp + j] =
            meshGL.vertProperties[meshGL.numProp * i + 3 + j];
    }

    halfedgeTangent_.resize(meshGL.halfedgeTangent.size() / 4);
    for (size_t i = 0; i < halfedgeTangent_.size(); ++i) {
      for (const int j : {0, 1, 2, 3})
        halfedgeTangent_[i][j] = meshGL.halfedgeTangent[4 * i + j];
    }

    Vec<TriRef> triRef;
    if (!meshGL.runOriginalID.empty()) {
      auto runIndex = meshGL.runIndex;
      const auto runEnd = meshGL.triVerts.size();
      if (runIndex.empty()) {
        runIndex = {0, static_cast<I>(runEnd)};
      } else if (runIndex.size() == meshGL.runOriginalID.size()) {
        runIndex.push_back(runEnd);
      }
      triRef.resize(meshGL.NumTri());
      const auto startID = Impl::ReserveIDs(meshGL.runOriginalID.size());
      for (size_t i = 0; i < meshGL.runOriginalID.size(); ++i) {
        const int meshID = startID + i;
        const int originalID = meshGL.runOriginalID[i];
        for (size_t tri = runIndex[i] / 3; tri < runIndex[i + 1] / 3; ++tri) {
          TriRef& ref = triRef[tri];
          ref.meshID = meshID;
          ref.originalID = originalID;
          ref.tri = meshGL.faceID.empty() ? tri : meshGL.faceID[tri];
          ref.faceID = tri;
        }

        if (meshGL.runTransform.empty()) {
          meshRelation_.meshIDtransform[meshID] = {originalID};
        } else {
          const Precision* m = meshGL.runTransform.data() + 12 * i;
          meshRelation_.meshIDtransform[meshID] = {originalID,
                                                   {{m[0], m[1], m[2]},
                                                    {m[3], m[4], m[5]},
                                                    {m[6], m[7], m[8]},
                                                    {m[9], m[10], m[11]}}};
        }
      }
    }

    Vec<ivec3> triVerts;
    triVerts.reserve(numTri);
    for (size_t i = 0; i < numTri; ++i) {
      ivec3 tri;
      for (const size_t j : {0, 1, 2}) {
        uint32_t vert = (uint32_t)meshGL.triVerts[3 * i + j];
        if (vert >= numVert) {
          MarkFailure(Error::VertexOutOfBounds);
          return;
        }
        tri[j] = prop2vert[vert];
      }
      if (tri[0] != tri[1] && tri[1] != tri[2] && tri[2] != tri[0]) {
        triVerts.push_back(tri);
        if (triRef.size() > 0) {
          meshRelation_.triRef.push_back(triRef[i]);
        }
        if (numProp > 0) {
          meshRelation_.triProperties.push_back(
              ivec3(static_cast<uint32_t>(meshGL.triVerts[3 * i]),
                    static_cast<uint32_t>(meshGL.triVerts[3 * i + 1]),
                    static_cast<uint32_t>(meshGL.triVerts[3 * i + 2])));
        }
      }
    }

    CreateHalfedges(triVerts);
    if (!IsManifold()) {
      MarkFailure(Error::NotManifold);
      return;
    }

    CalculateBBox();
    if (!IsFinite()) {
      MarkFailure(Error::NonFiniteVertex);
      return;
    }
    SetEpsilon(-1, std::is_same<Precision, float>::value);

    SplitPinchedVerts();

    CalculateNormals();

    if (meshGL.runOriginalID.empty()) {
      InitializeOriginal();
    }

    CreateFaces();

    SimplifyTopology();
    Finish();

    // A Manifold created from an input mesh is never an original - the input is
    // the original.
    meshRelation_.originalID = -1;
  }

  inline void ForVert(int halfedge, std::function<void(int halfedge)> func) {
    int current = halfedge;
    do {
      current = NextHalfedge(halfedge_[current].pairedHalfedge);
      func(current);
    } while (current != halfedge);
  }

  template <typename T>
  void ForVert(
      int halfedge, std::function<T(int halfedge)> transform,
      std::function<void(int halfedge, const T& here, T& next)> binaryOp) {
    T here = transform(halfedge);
    int current = halfedge;
    do {
      const int nextHalfedge = NextHalfedge(halfedge_[current].pairedHalfedge);
      T next = transform(nextHalfedge);
      binaryOp(current, here, next);
      here = next;
      current = nextHalfedge;
    } while (current != halfedge);
  }

  void CreateFaces();
  void RemoveUnreferencedVerts();
  void InitializeOriginal(bool keepFaceID = false);
  void CreateHalfedges(const Vec<ivec3>& triVerts);
  void CalculateNormals();
  void IncrementMeshIDs();

  void Update();
  void MarkFailure(Error status);
  void Warp(std::function<void(vec3&)> warpFunc);
  void WarpBatch(std::function<void(VecView<vec3>)> warpFunc);
  Impl Transform(const mat3x4& transform) const;
  SparseIndices EdgeCollisions(const Impl& B, bool inverted = false) const;
  SparseIndices VertexCollisionsZ(VecView<const vec3> vertsIn,
                                  bool inverted = false) const;

  bool IsEmpty() const { return NumTri() == 0; }
  size_t NumVert() const { return vertPos_.size(); }
  size_t NumEdge() const { return halfedge_.size() / 2; }
  size_t NumTri() const { return halfedge_.size() / 3; }
  size_t NumProp() const { return meshRelation_.numProp; }
  size_t NumPropVert() const {
    return NumProp() == 0 ? NumVert()
                          : meshRelation_.properties.size() / NumProp();
  }

  // properties.cu
  enum class Property { Volume, SurfaceArea };
  double GetProperty(Property prop) const;
  void CalculateCurvature(int gaussianIdx, int meanIdx);
  void CalculateBBox();
  bool IsFinite() const;
  bool IsIndexInBounds(VecView<const ivec3> triVerts) const;
  void SetEpsilon(double minEpsilon = -1, bool useSingle = false);
  bool IsManifold() const;
  bool Is2Manifold() const;
  bool MatchesTriNormals() const;
  int NumDegenerateTris() const;
  double MinGap(const Impl& other, double searchLength) const;

  // sort.cu
  void Finish();
  void SortVerts();
  void ReindexVerts(const Vec<int>& vertNew2Old, size_t numOldVert);
  void CompactProps();
  void GetFaceBoxMorton(Vec<Box>& faceBox, Vec<uint32_t>& faceMorton) const;
  void SortFaces(Vec<Box>& faceBox, Vec<uint32_t>& faceMorton);
  void GatherFaces(const Vec<int>& faceNew2Old);
  void GatherFaces(const Impl& old, const Vec<int>& faceNew2Old);

  // face_op.cu
  void Face2Tri(const Vec<int>& faceEdge, const Vec<TriRef>& halfedgeRef);
  PolygonsIdx Face2Polygons(VecView<Halfedge>::IterC start,
                            VecView<Halfedge>::IterC end,
                            mat2x3 projection) const;
  Polygons Slice(double height) const;
  Polygons Project() const;

  // edge_op.cu
  void CleanupTopology();
  void SimplifyTopology();
  void DedupeEdge(int edge);
  void CollapseEdge(int edge, std::vector<int>& edges);
  void RecursiveEdgeSwap(int edge, int& tag, std::vector<int>& visited,
                         std::vector<int>& edgeSwapStack,
                         std::vector<int>& edges);
  void RemoveIfFolded(int edge);
  void PairUp(int edge0, int edge1);
  void UpdateVert(int vert, int startEdge, int endEdge);
  void FormLoop(int current, int end);
  void CollapseTri(const ivec3& triEdge);
  void SplitPinchedVerts();

  // subdivision.cpp
  int GetNeighbor(int tri) const;
  ivec4 GetHalfedges(int tri) const;
  BaryIndices GetIndices(int halfedge) const;
  void FillRetainedVerts(Vec<Barycentric>& vertBary) const;
  Vec<Barycentric> Subdivide(std::function<int(vec3, vec4, vec4)>,
                             bool = false);

  // smoothing.cpp
  bool IsInsideQuad(int halfedge) const;
  bool IsMarkedInsideQuad(int halfedge) const;
  vec3 GetNormal(int halfedge, int normalIdx) const;
  vec4 TangentFromNormal(const vec3& normal, int halfedge) const;
  std::vector<Smoothness> UpdateSharpenedEdges(
      const std::vector<Smoothness>&) const;
  Vec<bool> FlatFaces() const;
  Vec<int> VertFlatFace(const Vec<bool>&) const;
  Vec<int> VertHalfedge() const;
  std::vector<Smoothness> SharpenEdges(double minSharpAngle,
                                       double minSmoothness) const;
  void SharpenTangent(int halfedge, double smoothness);
  void SetNormals(int normalIdx, double minSharpAngle);
  void LinearizeFlatTangents();
  void DistributeTangents(const Vec<bool>& fixedHalfedges);
  void CreateTangents(int normalIdx);
  void CreateTangents(std::vector<Smoothness>);
  void Refine(std::function<int(vec3, vec4, vec4)>, bool = false);

  // quickhull.cpp
  void Hull(VecView<vec3> vertPos);
};
}  // namespace manifold
