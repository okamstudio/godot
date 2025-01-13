// Copyright 2024 The Manifold Authors.
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

template <>
struct std::hash<manifold::ivec4> {
  size_t operator()(const manifold::ivec4& p) const {
    return std::hash<int>()(p.x) ^ std::hash<int>()(p.y) ^
           std::hash<int>()(p.z) ^ std::hash<int>()(p.w);
  }
};

namespace {
using namespace manifold;

class Partition {
 public:
  // The cached partitions don't have idx - it's added to the copy returned
  // from GetPartition that contains the mapping of the input divisions into the
  // sorted divisions that are uniquely cached.
  ivec4 idx;
  ivec4 sortedDivisions;
  Vec<vec4> vertBary;
  Vec<ivec3> triVert;

  int InteriorOffset() const {
    return sortedDivisions[0] + sortedDivisions[1] + sortedDivisions[2] +
           sortedDivisions[3];
  }

  int NumInterior() const { return vertBary.size() - InteriorOffset(); }

  static Partition GetPartition(ivec4 divisions) {
    if (divisions[0] == 0) return Partition();  // skip wrong side of quad

    ivec4 sortedDiv = divisions;
    ivec4 triIdx = {0, 1, 2, 3};
    if (divisions[3] == 0) {  // triangle
      if (sortedDiv[2] > sortedDiv[1]) {
        std::swap(sortedDiv[2], sortedDiv[1]);
        std::swap(triIdx[2], triIdx[1]);
      }
      if (sortedDiv[1] > sortedDiv[0]) {
        std::swap(sortedDiv[1], sortedDiv[0]);
        std::swap(triIdx[1], triIdx[0]);
        if (sortedDiv[2] > sortedDiv[1]) {
          std::swap(sortedDiv[2], sortedDiv[1]);
          std::swap(triIdx[2], triIdx[1]);
        }
      }
    } else {  // quad
      int minIdx = 0;
      int min = divisions[minIdx];
      int next = divisions[1];
      for (const int i : {1, 2, 3}) {
        const int n = divisions[(i + 1) % 4];
        if (divisions[i] < min || (divisions[i] == min && n < next)) {
          minIdx = i;
          min = divisions[i];
          next = n;
        }
      }
      // Backwards (mirrored) quads get a separate cache key for now for
      // simplicity, so there is no reversal necessary for quads when
      // re-indexing.
      ivec4 tmp = sortedDiv;
      for (const int i : {0, 1, 2, 3}) {
        triIdx[i] = (i + minIdx) % 4;
        sortedDiv[i] = tmp[triIdx[i]];
      }
    }

    Partition partition = GetCachedPartition(sortedDiv);
    partition.idx = triIdx;

    return partition;
  }

  Vec<ivec3> Reindex(ivec4 triVerts, ivec4 edgeOffsets, bvec4 edgeFwd,
                     int interiorOffset) const {
    Vec<int> newVerts;
    newVerts.reserve(vertBary.size());
    ivec4 triIdx = idx;
    ivec4 outTri = {0, 1, 2, 3};
    if (triVerts[3] < 0 && idx[1] != Next3(idx[0])) {
      triIdx = {idx[2], idx[0], idx[1], idx[3]};
      edgeFwd = !edgeFwd;
      std::swap(outTri[0], outTri[1]);
    }
    for (const int i : {0, 1, 2, 3}) {
      if (triVerts[triIdx[i]] >= 0) newVerts.push_back(triVerts[triIdx[i]]);
    }
    for (const int i : {0, 1, 2, 3}) {
      const int n = sortedDivisions[i] - 1;
      int offset = edgeOffsets[idx[i]] + (edgeFwd[idx[i]] ? 0 : n - 1);
      for (int j = 0; j < n; ++j) {
        newVerts.push_back(offset);
        offset += edgeFwd[idx[i]] ? 1 : -1;
      }
    }
    const int offset = interiorOffset - newVerts.size();
    size_t old = newVerts.size();
    newVerts.resize(vertBary.size());
    std::iota(newVerts.begin() + old, newVerts.end(), old + offset);

    const int numTri = triVert.size();
    Vec<ivec3> newTriVert(numTri);
    for_each_n(autoPolicy(numTri), countAt(0), numTri,
               [&newTriVert, &outTri, &newVerts, this](const int tri) {
                 for (const int j : {0, 1, 2}) {
                   newTriVert[tri][outTri[j]] = newVerts[triVert[tri][j]];
                 }
               });
    return newTriVert;
  }

 private:
  static inline auto cacheLock = std::mutex();
  static inline auto cache =
      std::unordered_map<ivec4, std::unique_ptr<Partition>>();

  // This triangulation is purely topological - it depends only on the number of
  // divisions of the three sides of the triangle. This allows them to be cached
  // and reused for similar triangles. The shape of the final surface is defined
  // by the tangents and the barycentric coordinates of the new verts. For
  // triangles, the input must be sorted: n[0] >= n[1] >= n[2] > 0.
  static Partition GetCachedPartition(ivec4 n) {
    {
      auto lockGuard = std::lock_guard<std::mutex>(cacheLock);
      auto cached = cache.find(n);
      if (cached != cache.end()) {
        return *cached->second;
      }
    }
    Partition partition;
    partition.sortedDivisions = n;
    if (n[3] > 0) {  // quad
      partition.vertBary.push_back({1, 0, 0, 0});
      partition.vertBary.push_back({0, 1, 0, 0});
      partition.vertBary.push_back({0, 0, 1, 0});
      partition.vertBary.push_back({0, 0, 0, 1});
      ivec4 edgeOffsets;
      edgeOffsets[0] = 4;
      for (const int i : {0, 1, 2, 3}) {
        if (i > 0) {
          edgeOffsets[i] = edgeOffsets[i - 1] + n[i - 1] - 1;
        }
        const vec4 nextBary = partition.vertBary[(i + 1) % 4];
        for (int j = 1; j < n[i]; ++j) {
          partition.vertBary.push_back(
              la::lerp(partition.vertBary[i], nextBary, (double)j / n[i]));
        }
      }
      PartitionQuad(partition.triVert, partition.vertBary, {0, 1, 2, 3},
                    edgeOffsets, n - 1, {true, true, true, true});
    } else {  // tri
      partition.vertBary.push_back({1, 0, 0, 0});
      partition.vertBary.push_back({0, 1, 0, 0});
      partition.vertBary.push_back({0, 0, 1, 0});
      for (const int i : {0, 1, 2}) {
        const vec4 nextBary = partition.vertBary[(i + 1) % 3];
        for (int j = 1; j < n[i]; ++j) {
          partition.vertBary.push_back(
              la::lerp(partition.vertBary[i], nextBary, (double)j / n[i]));
        }
      }
      const ivec3 edgeOffsets = {3, 3 + n[0] - 1, 3 + n[0] - 1 + n[1] - 1};

      const double f = n[2] * n[2] + n[0] * n[0];
      if (n[1] == 1) {
        if (n[0] == 1) {
          partition.triVert.push_back({0, 1, 2});
        } else {
          PartitionFan(partition.triVert, {0, 1, 2}, n[0] - 1, edgeOffsets[0]);
        }
      } else if (n[1] * n[1] > f - std::sqrt(2.0) * n[0] * n[2]) {  // acute-ish
        partition.triVert.push_back({edgeOffsets[1] - 1, 1, edgeOffsets[1]});
        PartitionQuad(partition.triVert, partition.vertBary,
                      {edgeOffsets[1] - 1, edgeOffsets[1], 2, 0},
                      {-1, edgeOffsets[1] + 1, edgeOffsets[2], edgeOffsets[0]},
                      {0, n[1] - 2, n[2] - 1, n[0] - 2},
                      {true, true, true, true});
      } else {  // obtuse -> spit into two acute
        // portion of n[0] under n[2]
        const int ns =
            std::min(n[0] - 2, (int)std::round((f - n[1] * n[1]) / (2 * n[0])));
        // height from n[0]: nh <= n[2]
        const int nh =
            std::max(1., std::round(std::sqrt(n[2] * n[2] - ns * ns)));

        const int hOffset = partition.vertBary.size();
        const vec4 middleBary = partition.vertBary[edgeOffsets[0] + ns - 1];
        for (int j = 1; j < nh; ++j) {
          partition.vertBary.push_back(
              la::lerp(partition.vertBary[2], middleBary, (double)j / nh));
        }

        partition.triVert.push_back({edgeOffsets[1] - 1, 1, edgeOffsets[1]});
        PartitionQuad(
            partition.triVert, partition.vertBary,
            {edgeOffsets[1] - 1, edgeOffsets[1], 2, edgeOffsets[0] + ns - 1},
            {-1, edgeOffsets[1] + 1, hOffset, edgeOffsets[0] + ns},
            {0, n[1] - 2, nh - 1, n[0] - ns - 2}, {true, true, true, true});

        if (n[2] == 1) {
          PartitionFan(partition.triVert, {0, edgeOffsets[0] + ns - 1, 2},
                       ns - 1, edgeOffsets[0]);
        } else {
          if (ns == 1) {
            partition.triVert.push_back({hOffset, 2, edgeOffsets[2]});
            PartitionQuad(partition.triVert, partition.vertBary,
                          {hOffset, edgeOffsets[2], 0, edgeOffsets[0]},
                          {-1, edgeOffsets[2] + 1, -1, hOffset + nh - 2},
                          {0, n[2] - 2, ns - 1, nh - 2},
                          {true, true, true, false});
          } else {
            partition.triVert.push_back({hOffset - 1, 0, edgeOffsets[0]});
            PartitionQuad(
                partition.triVert, partition.vertBary,
                {hOffset - 1, edgeOffsets[0], edgeOffsets[0] + ns - 1, 2},
                {-1, edgeOffsets[0] + 1, hOffset + nh - 2, edgeOffsets[2]},
                {0, ns - 2, nh - 1, n[2] - 2}, {true, true, false, true});
          }
        }
      }
    }

    auto lockGuard = std::lock_guard<std::mutex>(cacheLock);
    cache.insert({n, std::make_unique<Partition>(partition)});
    return partition;
  }

  // Side 0 has added edges while sides 1 and 2 do not. Fan spreads from vert 2.
  static void PartitionFan(Vec<ivec3>& triVert, ivec3 cornerVerts, int added,
                           int edgeOffset) {
    int last = cornerVerts[0];
    for (int i = 0; i < added; ++i) {
      const int next = edgeOffset + i;
      triVert.push_back({last, next, cornerVerts[2]});
      last = next;
    }
    triVert.push_back({last, cornerVerts[1], cornerVerts[2]});
  }

  // Partitions are parallel to the first edge unless two consecutive edgeAdded
  // are zero, in which case a terminal triangulation is performed.
  static void PartitionQuad(Vec<ivec3>& triVert, Vec<vec4>& vertBary,
                            ivec4 cornerVerts, ivec4 edgeOffsets,
                            ivec4 edgeAdded, bvec4 edgeFwd) {
    auto GetEdgeVert = [&](int edge, int idx) {
      return edgeOffsets[edge] + (edgeFwd[edge] ? 1 : -1) * idx;
    };

    DEBUG_ASSERT(la::all(la::gequal(edgeAdded, ivec4(0))), logicErr,
                 "negative divisions!");

    int corner = -1;
    int last = 3;
    int maxEdge = -1;
    for (const int i : {0, 1, 2, 3}) {
      if (corner == -1 && edgeAdded[i] == 0 && edgeAdded[last] == 0) {
        corner = i;
      }
      if (edgeAdded[i] > 0) {
        maxEdge = maxEdge == -1 ? i : -2;
      }
      last = i;
    }
    if (corner >= 0) {  // terminate
      if (maxEdge >= 0) {
        ivec4 edge = (ivec4(0, 1, 2, 3) + maxEdge) % 4;
        const int middle = edgeAdded[maxEdge] / 2;
        triVert.push_back({cornerVerts[edge[2]], cornerVerts[edge[3]],
                           GetEdgeVert(maxEdge, middle)});
        int last = cornerVerts[edge[0]];
        for (int i = 0; i <= middle; ++i) {
          const int next = GetEdgeVert(maxEdge, i);
          triVert.push_back({cornerVerts[edge[3]], last, next});
          last = next;
        }
        last = cornerVerts[edge[1]];
        for (int i = edgeAdded[maxEdge] - 1; i >= middle; --i) {
          const int next = GetEdgeVert(maxEdge, i);
          triVert.push_back({cornerVerts[edge[2]], next, last});
          last = next;
        }
      } else {
        int sideVert = cornerVerts[0];  // initial value is unused
        for (const int j : {1, 2}) {
          const int side = (corner + j) % 4;
          if (j == 2 && edgeAdded[side] > 0) {
            triVert.push_back(
                {cornerVerts[side], GetEdgeVert(side, 0), sideVert});
          } else {
            sideVert = cornerVerts[side];
          }
          for (int i = 0; i < edgeAdded[side]; ++i) {
            const int nextVert = GetEdgeVert(side, i);
            triVert.push_back({cornerVerts[corner], sideVert, nextVert});
            sideVert = nextVert;
          }
          if (j == 2 || edgeAdded[side] == 0) {
            triVert.push_back({cornerVerts[corner], sideVert,
                               cornerVerts[(corner + j + 1) % 4]});
          }
        }
      }
      return;
    }
    // recursively partition
    const int partitions = 1 + std::min(edgeAdded[1], edgeAdded[3]);
    ivec4 newCornerVerts = {cornerVerts[1], -1, -1, cornerVerts[0]};
    ivec4 newEdgeOffsets = {edgeOffsets[1], -1,
                            GetEdgeVert(3, edgeAdded[3] + 1), edgeOffsets[0]};
    ivec4 newEdgeAdded = {0, -1, 0, edgeAdded[0]};
    bvec4 newEdgeFwd = {edgeFwd[1], true, edgeFwd[3], edgeFwd[0]};

    for (int i = 1; i < partitions; ++i) {
      const int cornerOffset1 = (edgeAdded[1] * i) / partitions;
      const int cornerOffset3 =
          edgeAdded[3] - 1 - (edgeAdded[3] * i) / partitions;
      const int nextOffset1 = GetEdgeVert(1, cornerOffset1 + 1);
      const int nextOffset3 = GetEdgeVert(3, cornerOffset3 + 1);
      const int added = std::round(la::lerp(
          (double)edgeAdded[0], (double)edgeAdded[2], (double)i / partitions));

      newCornerVerts[1] = GetEdgeVert(1, cornerOffset1);
      newCornerVerts[2] = GetEdgeVert(3, cornerOffset3);
      newEdgeAdded[0] = std::abs(nextOffset1 - newEdgeOffsets[0]) - 1;
      newEdgeAdded[1] = added;
      newEdgeAdded[2] = std::abs(nextOffset3 - newEdgeOffsets[2]) - 1;
      newEdgeOffsets[1] = vertBary.size();
      newEdgeOffsets[2] = nextOffset3;

      for (int j = 0; j < added; ++j) {
        vertBary.push_back(la::lerp(vertBary[newCornerVerts[1]],
                                    vertBary[newCornerVerts[2]],
                                    (j + 1.0) / (added + 1.0)));
      }

      PartitionQuad(triVert, vertBary, newCornerVerts, newEdgeOffsets,
                    newEdgeAdded, newEdgeFwd);

      newCornerVerts[0] = newCornerVerts[1];
      newCornerVerts[3] = newCornerVerts[2];
      newEdgeAdded[3] = newEdgeAdded[1];
      newEdgeOffsets[0] = nextOffset1;
      newEdgeOffsets[3] = newEdgeOffsets[1] + newEdgeAdded[1] - 1;
      newEdgeFwd[3] = false;
    }

    newCornerVerts[1] = cornerVerts[2];
    newCornerVerts[2] = cornerVerts[3];
    newEdgeOffsets[1] = edgeOffsets[2];
    newEdgeAdded[0] =
        edgeAdded[1] - std::abs(newEdgeOffsets[0] - edgeOffsets[1]);
    newEdgeAdded[1] = edgeAdded[2];
    newEdgeAdded[2] = std::abs(newEdgeOffsets[2] - edgeOffsets[3]) - 1;
    newEdgeOffsets[2] = edgeOffsets[3];
    newEdgeFwd[1] = edgeFwd[2];

    PartitionQuad(triVert, vertBary, newCornerVerts, newEdgeOffsets,
                  newEdgeAdded, newEdgeFwd);
  }
};
}  // namespace

namespace manifold {

/**
 * Returns the tri side index (0-2) connected to the other side of this quad if
 * this tri is part of a quad, or -1 otherwise.
 */
int Manifold::Impl::GetNeighbor(int tri) const {
  int neighbor = -1;
  for (const int i : {0, 1, 2}) {
    if (IsMarkedInsideQuad(3 * tri + i)) {
      neighbor = neighbor == -1 ? i : -2;
    }
  }
  return neighbor;
}

/**
 * For the given triangle index, returns either the three halfedge indices of
 * that triangle and halfedges[3] = -1, or if the triangle is part of a quad, it
 * returns those four indices. If the triangle is part of a quad and is not the
 * lower of the two triangle indices, it returns all -1s.
 */
ivec4 Manifold::Impl::GetHalfedges(int tri) const {
  ivec4 halfedges(-1);
  for (const int i : {0, 1, 2}) {
    halfedges[i] = 3 * tri + i;
  }
  const int neighbor = GetNeighbor(tri);
  if (neighbor >= 0) {  // quad
    const int pair = halfedge_[3 * tri + neighbor].pairedHalfedge;
    if (pair / 3 < tri) {
      return ivec4(-1);  // only process lower tri index
    }
    // The order here matters to keep small quads split the way they started, or
    // else it can create a 4-manifold edge.
    halfedges[2] = NextHalfedge(halfedges[neighbor]);
    halfedges[3] = NextHalfedge(halfedges[2]);
    halfedges[0] = NextHalfedge(pair);
    halfedges[1] = NextHalfedge(halfedges[0]);
  }
  return halfedges;
}

/**
 * Returns the BaryIndices, which gives the tri and indices (0-3), such that
 * GetHalfedges(val.tri)[val.start4] points back to this halfedge, and val.end4
 * will point to the next one. This function handles this for both triangles and
 * quads. Returns {-1, -1, -1} if the edge is the interior of a quad.
 */
Manifold::Impl::BaryIndices Manifold::Impl::GetIndices(int halfedge) const {
  int tri = halfedge / 3;
  int idx = halfedge % 3;
  const int neighbor = GetNeighbor(tri);
  if (idx == neighbor) {
    return {-1, -1, -1};
  }

  if (neighbor < 0) {  // tri
    return {tri, idx, Next3(idx)};
  } else {  // quad
    const int pair = halfedge_[3 * tri + neighbor].pairedHalfedge;
    if (pair / 3 < tri) {
      tri = pair / 3;
      idx = Next3(neighbor) == idx ? 0 : 1;
    } else {
      idx = Next3(neighbor) == idx ? 2 : 3;
    }
    return {tri, idx, (idx + 1) % 4};
  }
}

/**
 * Retained verts are part of several triangles, and it doesn't matter which one
 * the vertBary refers to. Here, whichever is last will win and it's done on the
 * CPU for simplicity for now. Using AtomicCAS on .tri should work for a GPU
 * version if desired.
 */
void Manifold::Impl::FillRetainedVerts(Vec<Barycentric>& vertBary) const {
  const int numTri = halfedge_.size() / 3;
  for (int tri = 0; tri < numTri; ++tri) {
    for (const int i : {0, 1, 2}) {
      const BaryIndices indices = GetIndices(3 * tri + i);
      if (indices.start4 < 0) continue;  // skip quad interiors
      vec4 uvw(0.0);
      uvw[indices.start4] = 1;
      vertBary[halfedge_[3 * tri + i].startVert] = {indices.tri, uvw};
    }
  }
}

/**
 * Split each edge into n pieces as defined by calling the edgeDivisions
 * function, and sub-triangulate each triangle accordingly. This function
 * doesn't run Finish(), as that is expensive and it'll need to be run after
 * the new vertices have moved, which is a likely scenario after refinement
 * (smoothing).
 */
Vec<Barycentric> Manifold::Impl::Subdivide(
    std::function<int(vec3, vec4, vec4)> edgeDivisions, bool keepInterior) {
  Vec<TmpEdge> edges = CreateTmpEdges(halfedge_);
  const int numVert = NumVert();
  const int numEdge = edges.size();
  const int numTri = NumTri();
  Vec<int> half2Edge(2 * numEdge);
  auto policy = autoPolicy(numEdge, 1e4);
  for_each_n(policy, countAt(0), numEdge,
             [&half2Edge, &edges, this](const int edge) {
               const int idx = edges[edge].halfedgeIdx;
               half2Edge[idx] = edge;
               half2Edge[halfedge_[idx].pairedHalfedge] = edge;
             });

  Vec<ivec4> faceHalfedges(numTri);
  for_each_n(policy, countAt(0), numTri, [&faceHalfedges, this](const int tri) {
    faceHalfedges[tri] = GetHalfedges(tri);
  });

  Vec<int> edgeAdded(numEdge);
  for_each_n(policy, countAt(0), numEdge,
             [&edgeAdded, &edges, edgeDivisions, this](const int i) {
               const TmpEdge edge = edges[i];
               const int hIdx = edge.halfedgeIdx;
               if (IsMarkedInsideQuad(hIdx)) {
                 edgeAdded[i] = 0;
                 return;
               }
               const vec3 vec = vertPos_[edge.first] - vertPos_[edge.second];
               const vec4 tangent0 = halfedgeTangent_.empty()
                                         ? vec4(0.0)
                                         : halfedgeTangent_[hIdx];
               const vec4 tangent1 =
                   halfedgeTangent_.empty()
                       ? vec4(0.0)
                       : halfedgeTangent_[halfedge_[hIdx].pairedHalfedge];
               edgeAdded[i] = edgeDivisions(vec, tangent0, tangent1);
             });

  if (keepInterior) {
    // Triangles where the greatest number of divisions exceeds the sum of the
    // other two sides will be triangulated as a strip, since if the sub-edges
    // were all equal length it would be degenerate. This leads to poor results
    // with RefineToTolerance, so we avoid this case by adding some extra
    // divisions to the short sides so that the triangulation has some thickness
    // and creates more interior facets.
    Vec<int> tmp(numEdge);
    for_each_n(
        policy, countAt(0), numEdge,
        [&tmp, &edgeAdded, &edges, &half2Edge, this](const int i) {
          tmp[i] = edgeAdded[i];
          const TmpEdge edge = edges[i];
          int hIdx = edge.halfedgeIdx;
          if (IsMarkedInsideQuad(hIdx)) return;

          const int thisAdded = tmp[i];
          auto Added = [&edgeAdded, &half2Edge, thisAdded, this](int hIdx) {
            int longest = 0;
            int total = 0;
            for (int j : {0, 1, 2}) {
              const int added = edgeAdded[half2Edge[hIdx]];
              longest = la::max(longest, added);
              total += added;
              hIdx = NextHalfedge(hIdx);
              if (IsMarkedInsideQuad(hIdx)) {
                // No extra on quads
                longest = 0;
                total = 1;
                break;
              }
            }
            const int minExtra = longest * 0.2 + 1;
            const int extra = 2 * longest + minExtra - total;
            return extra > 0 ? (extra * (longest - thisAdded)) / longest : 0;
          };

          tmp[i] += la::max(Added(hIdx), Added(halfedge_[hIdx].pairedHalfedge));
        });
    edgeAdded.swap(tmp);
  }

  Vec<int> edgeOffset(numEdge);
  exclusive_scan(edgeAdded.begin(), edgeAdded.end(), edgeOffset.begin(),
                 numVert);

  Vec<Barycentric> vertBary(edgeOffset.back() + edgeAdded.back());
  const int totalEdgeAdded = vertBary.size() - numVert;
  FillRetainedVerts(vertBary);
  for_each_n(policy, countAt(0), numEdge,
             [&vertBary, &edges, &edgeAdded, &edgeOffset, this](const int i) {
               const int n = edgeAdded[i];
               const int offset = edgeOffset[i];

               const BaryIndices indices = GetIndices(edges[i].halfedgeIdx);
               if (indices.tri < 0) {
                 return;  // inside quad
               }
               const double frac = 1.0 / (n + 1);

               for (int i = 0; i < n; ++i) {
                 vec4 uvw(0.0);
                 uvw[indices.end4] = (i + 1) * frac;
                 uvw[indices.start4] = 1 - uvw[indices.end4];
                 vertBary[offset + i].uvw = uvw;
                 vertBary[offset + i].tri = indices.tri;
               }
             });

  std::vector<Partition> subTris(numTri);
  for_each_n(policy, countAt(0), numTri,
             [this, &subTris, &half2Edge, &edgeAdded, &faceHalfedges](int tri) {
               const ivec4 halfedges = faceHalfedges[tri];
               ivec4 divisions(0);
               for (const int i : {0, 1, 2, 3}) {
                 if (halfedges[i] >= 0) {
                   divisions[i] = edgeAdded[half2Edge[halfedges[i]]] + 1;
                 }
               }
               subTris[tri] = Partition::GetPartition(divisions);
             });

  Vec<int> triOffset(numTri);
  auto numSubTris =
      TransformIterator(subTris.begin(), [](const Partition& part) {
        return static_cast<int>(part.triVert.size());
      });
  manifold::exclusive_scan(numSubTris, numSubTris + numTri, triOffset.begin(),
                           0);

  Vec<int> interiorOffset(numTri);
  auto numInterior =
      TransformIterator(subTris.begin(), [](const Partition& part) {
        return static_cast<int>(part.NumInterior());
      });
  manifold::exclusive_scan(numInterior, numInterior + numTri,
                           interiorOffset.begin(),
                           static_cast<int>(vertBary.size()));

  Vec<ivec3> triVerts(triOffset.back() + subTris.back().triVert.size());
  vertBary.resize(interiorOffset.back() + subTris.back().NumInterior());
  Vec<TriRef> triRef(triVerts.size());
  for_each_n(
      policy, countAt(0), numTri,
      [this, &triVerts, &triRef, &vertBary, &subTris, &edgeOffset, &half2Edge,
       &triOffset, &interiorOffset, &faceHalfedges](int tri) {
        const ivec4 halfedges = faceHalfedges[tri];
        if (halfedges[0] < 0) return;
        ivec4 tri3;
        ivec4 edgeOffsets;
        bvec4 edgeFwd(false);
        for (const int i : {0, 1, 2, 3}) {
          if (halfedges[i] < 0) {
            tri3[i] = -1;
            continue;
          }
          const Halfedge& halfedge = halfedge_[halfedges[i]];
          tri3[i] = halfedge.startVert;
          edgeOffsets[i] = edgeOffset[half2Edge[halfedges[i]]];
          edgeFwd[i] = halfedge.IsForward();
        }

        Vec<ivec3> newTris = subTris[tri].Reindex(tri3, edgeOffsets, edgeFwd,
                                                  interiorOffset[tri]);
        copy(newTris.begin(), newTris.end(), triVerts.begin() + triOffset[tri]);
        auto start = triRef.begin() + triOffset[tri];
        fill(start, start + newTris.size(), meshRelation_.triRef[tri]);

        const ivec4 idx = subTris[tri].idx;
        const ivec4 vIdx = halfedges[3] >= 0 || idx[1] == Next3(idx[0])
                               ? idx
                               : ivec4(idx[2], idx[0], idx[1], idx[3]);
        ivec4 rIdx;
        for (const int i : {0, 1, 2, 3}) {
          rIdx[vIdx[i]] = i;
        }

        const auto& subBary = subTris[tri].vertBary;
        transform(subBary.begin() + subTris[tri].InteriorOffset(),
                  subBary.end(), vertBary.begin() + interiorOffset[tri],
                  [tri, rIdx](vec4 bary) {
                    return Barycentric({tri,
                                        {bary[rIdx[0]], bary[rIdx[1]],
                                         bary[rIdx[2]], bary[rIdx[3]]}});
                  });
      });
  meshRelation_.triRef = triRef;

  Vec<vec3> newVertPos(vertBary.size());
  for_each_n(policy, countAt(0), vertBary.size(),
             [&newVertPos, &vertBary, &faceHalfedges, this](const int vert) {
               const Barycentric bary = vertBary[vert];
               const ivec4 halfedges = faceHalfedges[bary.tri];
               if (halfedges[3] < 0) {
                 mat3 triPos;
                 for (const int i : {0, 1, 2}) {
                   triPos[i] = vertPos_[halfedge_[halfedges[i]].startVert];
                 }
                 newVertPos[vert] = triPos * vec3(bary.uvw);
               } else {
                 mat3x4 quadPos;
                 for (const int i : {0, 1, 2, 3}) {
                   quadPos[i] = vertPos_[halfedge_[halfedges[i]].startVert];
                 }
                 newVertPos[vert] = quadPos * bary.uvw;
               }
             });
  vertPos_ = newVertPos;

  faceNormal_.resize(0);

  if (meshRelation_.numProp > 0) {
    const int numPropVert = NumPropVert();
    const int addedVerts = NumVert() - numVert;
    const int propOffset = numPropVert - numVert;
    Vec<double> prop(meshRelation_.numProp *
                     (numPropVert + addedVerts + totalEdgeAdded));

    // copy retained prop verts
    copy(meshRelation_.properties.begin(), meshRelation_.properties.end(),
         prop.begin());

    // copy interior prop verts and forward edge prop verts
    for_each_n(
        policy, countAt(0), addedVerts,
        [&prop, &vertBary, &faceHalfedges, numVert, numPropVert,
         this](const int i) {
          const int vert = numPropVert + i;
          const Barycentric bary = vertBary[numVert + i];
          const ivec4 halfedges = faceHalfedges[bary.tri];
          auto& rel = meshRelation_;

          for (int p = 0; p < rel.numProp; ++p) {
            if (halfedges[3] < 0) {
              vec3 triProp;
              for (const int i : {0, 1, 2}) {
                triProp[i] = rel.properties[rel.triProperties[bary.tri][i] *
                                                rel.numProp +
                                            p];
              }
              prop[vert * rel.numProp + p] = la::dot(triProp, vec3(bary.uvw));
            } else {
              vec4 quadProp;
              for (const int i : {0, 1, 2, 3}) {
                const int tri = halfedges[i] / 3;
                const int j = halfedges[i] % 3;
                quadProp[i] =
                    rel.properties[rel.triProperties[tri][j] * rel.numProp + p];
              }
              prop[vert * rel.numProp + p] = la::dot(quadProp, bary.uvw);
            }
          }
        });

    // copy backward edge prop verts
    for_each_n(policy, countAt(0), numEdge,
               [this, &prop, &edges, &edgeAdded, &edgeOffset, propOffset,
                addedVerts](const int i) {
                 const int n = edgeAdded[i];
                 const int offset = edgeOffset[i] + propOffset + addedVerts;
                 auto& rel = meshRelation_;

                 const double frac = 1.0 / (n + 1);
                 const int halfedgeIdx =
                     halfedge_[edges[i].halfedgeIdx].pairedHalfedge;
                 const int v0 = halfedgeIdx % 3;
                 const int tri = halfedgeIdx / 3;
                 const int prop0 = rel.triProperties[tri][v0];
                 const int prop1 = rel.triProperties[tri][Next3(v0)];
                 for (int i = 0; i < n; ++i) {
                   for (int p = 0; p < rel.numProp; ++p) {
                     prop[(offset + i) * rel.numProp + p] =
                         la::lerp(rel.properties[prop0 * rel.numProp + p],
                                  rel.properties[prop1 * rel.numProp + p],
                                  (i + 1) * frac);
                   }
                 }
               });

    Vec<ivec3> triProp(triVerts.size());
    for_each_n(policy, countAt(0), numTri,
               [this, &triProp, &subTris, &edgeOffset, &half2Edge, &triOffset,
                &interiorOffset, &faceHalfedges, propOffset,
                addedVerts](const int tri) {
                 const ivec4 halfedges = faceHalfedges[tri];
                 if (halfedges[0] < 0) return;

                 auto& rel = meshRelation_;
                 ivec4 tri3;
                 ivec4 edgeOffsets;
                 bvec4 edgeFwd(true);
                 for (const int i : {0, 1, 2, 3}) {
                   if (halfedges[i] < 0) {
                     tri3[i] = -1;
                     continue;
                   }
                   const int thisTri = halfedges[i] / 3;
                   const int j = halfedges[i] % 3;
                   const Halfedge& halfedge = halfedge_[halfedges[i]];
                   tri3[i] = rel.triProperties[thisTri][j];
                   edgeOffsets[i] = edgeOffset[half2Edge[halfedges[i]]];
                   if (!halfedge.IsForward()) {
                     const int pairTri = halfedge.pairedHalfedge / 3;
                     const int k = halfedge.pairedHalfedge % 3;
                     if (rel.triProperties[pairTri][k] !=
                             rel.triProperties[thisTri][Next3(j)] ||
                         rel.triProperties[pairTri][Next3(k)] !=
                             rel.triProperties[thisTri][j]) {
                       edgeOffsets[i] += addedVerts;
                     } else {
                       edgeFwd[i] = false;
                     }
                   }
                 }

                 Vec<ivec3> newTris = subTris[tri].Reindex(
                     tri3, edgeOffsets + propOffset, edgeFwd,
                     interiorOffset[tri] + propOffset);
                 copy(newTris.begin(), newTris.end(),
                      triProp.begin() + triOffset[tri]);
               });

    meshRelation_.properties = prop;
    meshRelation_.triProperties = triProp;
  }

  CreateHalfedges(triVerts);

  return vertBary;
}

}  // namespace manifold
