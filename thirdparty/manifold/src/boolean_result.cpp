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

#include <algorithm>
#include <array>
#include <map>

#include "./boolean3.h"
#include "./parallel.h"
#include "./utils.h"

#if (MANIFOLD_PAR == 1) && __has_include(<tbb/concurrent_map.h>)
#define TBB_PREVIEW_CONCURRENT_ORDERED_CONTAINERS 1
#include <tbb/concurrent_map.h>
#include <tbb/parallel_for.h>

template <typename K, typename V>
using concurrent_map = tbb::concurrent_map<K, V>;
#else
template <typename K, typename V>
// not really concurrent when tbb is disabled
using concurrent_map = std::map<K, V>;
#endif

using namespace manifold;

template <>
struct std::hash<std::pair<int, int>> {
  size_t operator()(const std::pair<int, int> &p) const {
    return std::hash<int>()(p.first) ^ std::hash<int>()(p.second);
  }
};

namespace {

constexpr int kParallelThreshold = 128;

struct AbsSum {
  int operator()(int a, int b) const { return abs(a) + abs(b); }
};

struct DuplicateVerts {
  VecView<vec3> vertPosR;
  VecView<const int> inclusion;
  VecView<const int> vertR;
  VecView<const vec3> vertPosP;

  void operator()(const int vert) {
    const int n = std::abs(inclusion[vert]);
    for (int i = 0; i < n; ++i) {
      vertPosR[vertR[vert] + i] = vertPosP[vert];
    }
  }
};

template <bool atomic>
struct CountVerts {
  VecView<Halfedge> halfedges;
  VecView<int> count;
  VecView<const int> inclusion;

  void operator()(size_t i) {
    if (atomic)
      AtomicAdd(count[i / 3], std::abs(inclusion[halfedges[i].startVert]));
    else
      count[i / 3] += std::abs(inclusion[halfedges[i].startVert]);
  }
};

template <const bool inverted, const bool atomic>
struct CountNewVerts {
  VecView<int> countP;
  VecView<int> countQ;
  VecView<const int> i12;
  const SparseIndices &pq;
  VecView<const Halfedge> halfedges;

  void operator()(const int idx) {
    int edgeP = pq.Get(idx, inverted);
    int faceQ = pq.Get(idx, !inverted);
    int inclusion = std::abs(i12[idx]);

    if (atomic) {
      AtomicAdd(countQ[faceQ], inclusion);
      const Halfedge half = halfedges[edgeP];
      AtomicAdd(countP[edgeP / 3], inclusion);
      AtomicAdd(countP[half.pairedHalfedge / 3], inclusion);
    } else {
      countQ[faceQ] += inclusion;
      const Halfedge half = halfedges[edgeP];
      countP[edgeP / 3] += inclusion;
      countP[half.pairedHalfedge / 3] += inclusion;
    }
  }
};

std::tuple<Vec<int>, Vec<int>> SizeOutput(
    Manifold::Impl &outR, const Manifold::Impl &inP, const Manifold::Impl &inQ,
    const Vec<int> &i03, const Vec<int> &i30, const Vec<int> &i12,
    const Vec<int> &i21, const SparseIndices &p1q2, const SparseIndices &p2q1,
    bool invertQ) {
  ZoneScoped;
  Vec<int> sidesPerFacePQ(inP.NumTri() + inQ.NumTri(), 0);
  // note: numFaceR <= facePQ2R.size() = sidesPerFacePQ.size() + 1

  auto sidesPerFaceP = sidesPerFacePQ.view(0, inP.NumTri());
  auto sidesPerFaceQ = sidesPerFacePQ.view(inP.NumTri(), inQ.NumTri());

  if (inP.halfedge_.size() >= 1e5) {
    for_each(ExecutionPolicy::Par, countAt(0_uz), countAt(inP.halfedge_.size()),
             CountVerts<true>({inP.halfedge_, sidesPerFaceP, i03}));
    for_each(ExecutionPolicy::Par, countAt(0_uz), countAt(inQ.halfedge_.size()),
             CountVerts<true>({inQ.halfedge_, sidesPerFaceQ, i30}));
  } else {
    for_each(ExecutionPolicy::Seq, countAt(0_uz), countAt(inP.halfedge_.size()),
             CountVerts<false>({inP.halfedge_, sidesPerFaceP, i03}));
    for_each(ExecutionPolicy::Seq, countAt(0_uz), countAt(inQ.halfedge_.size()),
             CountVerts<false>({inQ.halfedge_, sidesPerFaceQ, i30}));
  }

  if (i12.size() >= 1e5) {
    for_each_n(ExecutionPolicy::Par, countAt(0), i12.size(),
               CountNewVerts<false, true>(
                   {sidesPerFaceP, sidesPerFaceQ, i12, p1q2, inP.halfedge_}));
    for_each_n(ExecutionPolicy::Par, countAt(0), i21.size(),
               CountNewVerts<true, true>(
                   {sidesPerFaceQ, sidesPerFaceP, i21, p2q1, inQ.halfedge_}));
  } else {
    for_each_n(ExecutionPolicy::Seq, countAt(0), i12.size(),
               CountNewVerts<false, false>(
                   {sidesPerFaceP, sidesPerFaceQ, i12, p1q2, inP.halfedge_}));
    for_each_n(ExecutionPolicy::Seq, countAt(0), i21.size(),
               CountNewVerts<true, false>(
                   {sidesPerFaceQ, sidesPerFaceP, i21, p2q1, inQ.halfedge_}));
  }

  Vec<int> facePQ2R(inP.NumTri() + inQ.NumTri() + 1, 0);
  auto keepFace = TransformIterator(sidesPerFacePQ.begin(),
                                    [](int x) { return x > 0 ? 1 : 0; });

  inclusive_scan(keepFace, keepFace + sidesPerFacePQ.size(),
                 facePQ2R.begin() + 1);
  int numFaceR = facePQ2R.back();
  facePQ2R.resize(inP.NumTri() + inQ.NumTri());

  outR.faceNormal_.resize(numFaceR);

  Vec<size_t> tmpBuffer(outR.faceNormal_.size());
  auto faceIds = TransformIterator(countAt(0_uz), [&sidesPerFacePQ](size_t i) {
    if (sidesPerFacePQ[i] > 0) return i;
    return std::numeric_limits<size_t>::max();
  });

  auto next =
      copy_if(faceIds, faceIds + inP.faceNormal_.size(), tmpBuffer.begin(),
              [](size_t v) { return v != std::numeric_limits<size_t>::max(); });

  gather(tmpBuffer.begin(), next, inP.faceNormal_.begin(),
         outR.faceNormal_.begin());

  auto faceIdsQ =
      TransformIterator(countAt(0_uz), [&sidesPerFacePQ, &inP](size_t i) {
        if (sidesPerFacePQ[i + inP.faceNormal_.size()] > 0) return i;
        return std::numeric_limits<size_t>::max();
      });
  auto end =
      copy_if(faceIdsQ, faceIdsQ + inQ.faceNormal_.size(), next,
              [](size_t v) { return v != std::numeric_limits<size_t>::max(); });

  if (invertQ) {
    gather(next, end,
           TransformIterator(inQ.faceNormal_.begin(), Negate<vec3>()),
           outR.faceNormal_.begin() + std::distance(tmpBuffer.begin(), next));
  } else {
    gather(next, end, inQ.faceNormal_.begin(),
           outR.faceNormal_.begin() + std::distance(tmpBuffer.begin(), next));
  }

  auto newEnd = remove(sidesPerFacePQ.begin(), sidesPerFacePQ.end(), 0);
  Vec<int> faceEdge(newEnd - sidesPerFacePQ.begin() + 1, 0);
  inclusive_scan(sidesPerFacePQ.begin(), newEnd, faceEdge.begin() + 1);
  outR.halfedge_.resize(faceEdge.back());

  return std::make_tuple(faceEdge, facePQ2R);
}

struct EdgePos {
  int vert;
  double edgePos;
  bool isStart;
};

// thread sanitizer doesn't really know how to check when there are too many
// mutex
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
__attribute__((no_sanitize("thread")))
#endif
#endif
void AddNewEdgeVerts(
    // we need concurrent_map because we will be adding things concurrently
    concurrent_map<int, std::vector<EdgePos>> &edgesP,
    concurrent_map<std::pair<int, int>, std::vector<EdgePos>> &edgesNew,
    const SparseIndices &p1q2, const Vec<int> &i12, const Vec<int> &v12R,
    const Vec<Halfedge> &halfedgeP, bool forward) {
  ZoneScoped;
  // For each edge of P that intersects a face of Q (p1q2), add this vertex to
  // P's corresponding edge vector and to the two new edges, which are
  // intersections between the face of Q and the two faces of P attached to the
  // edge. The direction and duplicity are given by i12, while v12R remaps to
  // the output vert index. When forward is false, all is reversed.
  auto process = [&](std::function<void(size_t)> lock,
                     std::function<void(size_t)> unlock, size_t i) {
    const int edgeP = p1q2.Get(i, !forward);
    const int faceQ = p1q2.Get(i, forward);
    const int vert = v12R[i];
    const int inclusion = i12[i];

    Halfedge halfedge = halfedgeP[edgeP];
    std::pair<int, int> keyRight = {halfedge.pairedHalfedge / 3, faceQ};
    if (!forward) std::swap(keyRight.first, keyRight.second);

    std::pair<int, int> keyLeft = {edgeP / 3, faceQ};
    if (!forward) std::swap(keyLeft.first, keyLeft.second);

    bool direction = inclusion < 0;
    std::hash<std::pair<int, int>> pairHasher;
    std::array<std::tuple<bool, size_t, std::vector<EdgePos> *>, 3> edges = {
        std::make_tuple(direction, std::hash<int>{}(edgeP), &edgesP[edgeP]),
        std::make_tuple(direction ^ !forward,  // revert if not forward
                        pairHasher(keyRight), &edgesNew[keyRight]),
        std::make_tuple(direction ^ forward,  // revert if forward
                        pairHasher(keyLeft), &edgesNew[keyLeft])};
    for (const auto &tuple : edges) {
      lock(std::get<1>(tuple));
      for (int j = 0; j < std::abs(inclusion); ++j)
        std::get<2>(tuple)->push_back({vert + j, 0.0, std::get<0>(tuple)});
      unlock(std::get<1>(tuple));
      direction = !direction;
    }
  };
#if (MANIFOLD_PAR == 1) && __has_include(<tbb/tbb.h>)
  // parallelize operations, requires concurrent_map so we can only enable this
  // with tbb
  if (p1q2.size() > kParallelThreshold) {
    // ideally we should have 1 mutex per key, but kParallelThreshold is enough
    // to avoid contention for most of the cases
    std::array<std::mutex, kParallelThreshold> mutexes;
    static tbb::affinity_partitioner ap;
    auto processFun = std::bind(
        process, [&](size_t hash) { mutexes[hash % mutexes.size()].lock(); },
        [&](size_t hash) { mutexes[hash % mutexes.size()].unlock(); },
        std::placeholders::_1);
    tbb::parallel_for(
        tbb::blocked_range<size_t>(0_uz, p1q2.size(), 32),
        [&](const tbb::blocked_range<size_t> &range) {
          for (size_t i = range.begin(); i != range.end(); i++) processFun(i);
        },
        ap);
    return;
  }
#endif
  auto processFun = std::bind(
      process, [](size_t _) {}, [](size_t _) {}, std::placeholders::_1);
  for (size_t i = 0; i < p1q2.size(); ++i) processFun(i);
}

std::vector<Halfedge> PairUp(std::vector<EdgePos> &edgePos) {
  // Pair start vertices with end vertices to form edges. The choice of pairing
  // is arbitrary for the manifoldness guarantee, but must be ordered to be
  // geometrically valid. If the order does not go start-end-start-end... then
  // the input and output are not geometrically valid and this algorithm becomes
  // a heuristic.
  DEBUG_ASSERT(edgePos.size() % 2 == 0, topologyErr,
               "Non-manifold edge! Not an even number of points.");
  size_t nEdges = edgePos.size() / 2;
  auto middle = std::partition(edgePos.begin(), edgePos.end(),
                               [](EdgePos x) { return x.isStart; });
  DEBUG_ASSERT(static_cast<size_t>(middle - edgePos.begin()) == nEdges,
               topologyErr, "Non-manifold edge!");
  auto cmp = [](EdgePos a, EdgePos b) { return a.edgePos < b.edgePos; };
  std::stable_sort(edgePos.begin(), middle, cmp);
  std::stable_sort(middle, edgePos.end(), cmp);
  std::vector<Halfedge> edges;
  for (size_t i = 0; i < nEdges; ++i)
    edges.push_back({edgePos[i].vert, edgePos[i + nEdges].vert, -1});
  return edges;
}

void AppendPartialEdges(Manifold::Impl &outR, Vec<char> &wholeHalfedgeP,
                        Vec<int> &facePtrR,
                        concurrent_map<int, std::vector<EdgePos>> &edgesP,
                        Vec<TriRef> &halfedgeRef, const Manifold::Impl &inP,
                        const Vec<int> &i03, const Vec<int> &vP2R,
                        const Vec<int>::IterC faceP2R, bool forward) {
  ZoneScoped;
  // Each edge in the map is partially retained; for each of these, look up
  // their original verts and include them based on their winding number (i03),
  // while remapping them to the output using vP2R. Use the verts position
  // projected along the edge vector to pair them up, then distribute these
  // edges to their faces.
  Vec<Halfedge> &halfedgeR = outR.halfedge_;
  const Vec<vec3> &vertPosP = inP.vertPos_;
  const Vec<Halfedge> &halfedgeP = inP.halfedge_;

  for (auto &value : edgesP) {
    const int edgeP = value.first;
    std::vector<EdgePos> &edgePosP = value.second;

    const Halfedge &halfedge = halfedgeP[edgeP];
    wholeHalfedgeP[edgeP] = false;
    wholeHalfedgeP[halfedge.pairedHalfedge] = false;

    const int vStart = halfedge.startVert;
    const int vEnd = halfedge.endVert;
    const vec3 edgeVec = vertPosP[vEnd] - vertPosP[vStart];
    // Fill in the edge positions of the old points.
    for (EdgePos &edge : edgePosP) {
      edge.edgePos = la::dot(outR.vertPos_[edge.vert], edgeVec);
    }

    int inclusion = i03[vStart];
    EdgePos edgePos = {vP2R[vStart],
                       la::dot(outR.vertPos_[vP2R[vStart]], edgeVec),
                       inclusion > 0};
    for (int j = 0; j < std::abs(inclusion); ++j) {
      edgePosP.push_back(edgePos);
      ++edgePos.vert;
    }

    inclusion = i03[vEnd];
    edgePos = {vP2R[vEnd], la::dot(outR.vertPos_[vP2R[vEnd]], edgeVec),
               inclusion < 0};
    for (int j = 0; j < std::abs(inclusion); ++j) {
      edgePosP.push_back(edgePos);
      ++edgePos.vert;
    }

    // sort edges into start/end pairs along length
    std::vector<Halfedge> edges = PairUp(edgePosP);

    // add halfedges to result
    const int faceLeftP = edgeP / 3;
    const int faceLeft = faceP2R[faceLeftP];
    const int faceRightP = halfedge.pairedHalfedge / 3;
    const int faceRight = faceP2R[faceRightP];
    // Negative inclusion means the halfedges are reversed, which means our
    // reference is now to the endVert instead of the startVert, which is one
    // position advanced CCW. This is only valid if this is a retained vert; it
    // will be ignored later if the vert is new.
    const TriRef forwardRef = {forward ? 0 : 1, -1, faceLeftP};
    const TriRef backwardRef = {forward ? 0 : 1, -1, faceRightP};

    for (Halfedge e : edges) {
      const int forwardEdge = facePtrR[faceLeft]++;
      const int backwardEdge = facePtrR[faceRight]++;

      e.pairedHalfedge = backwardEdge;
      halfedgeR[forwardEdge] = e;
      halfedgeRef[forwardEdge] = forwardRef;

      std::swap(e.startVert, e.endVert);
      e.pairedHalfedge = forwardEdge;
      halfedgeR[backwardEdge] = e;
      halfedgeRef[backwardEdge] = backwardRef;
    }
  }
}

void AppendNewEdges(
    Manifold::Impl &outR, Vec<int> &facePtrR,
    concurrent_map<std::pair<int, int>, std::vector<EdgePos>> &edgesNew,
    Vec<TriRef> &halfedgeRef, const Vec<int> &facePQ2R, const int numFaceP) {
  ZoneScoped;
  // Pair up each edge's verts and distribute to faces based on indices in key.
  Vec<Halfedge> &halfedgeR = outR.halfedge_;
  Vec<vec3> &vertPosR = outR.vertPos_;

  for (auto &value : edgesNew) {
    const int faceP = value.first.first;
    const int faceQ = value.first.second;
    std::vector<EdgePos> &edgePos = value.second;

    Box bbox;
    for (auto edge : edgePos) {
      bbox.Union(vertPosR[edge.vert]);
    }
    const vec3 size = bbox.Size();
    // Order the points along their longest dimension.
    const int i = (size.x > size.y && size.x > size.z) ? 0
                  : size.y > size.z                    ? 1
                                                       : 2;
    for (auto &edge : edgePos) {
      edge.edgePos = vertPosR[edge.vert][i];
    }

    // sort edges into start/end pairs along length.
    std::vector<Halfedge> edges = PairUp(edgePos);

    // add halfedges to result
    const int faceLeft = facePQ2R[faceP];
    const int faceRight = facePQ2R[numFaceP + faceQ];
    const TriRef forwardRef = {0, -1, faceP};
    const TriRef backwardRef = {1, -1, faceQ};
    for (Halfedge e : edges) {
      const int forwardEdge = facePtrR[faceLeft]++;
      const int backwardEdge = facePtrR[faceRight]++;

      e.pairedHalfedge = backwardEdge;
      halfedgeR[forwardEdge] = e;
      halfedgeRef[forwardEdge] = forwardRef;

      std::swap(e.startVert, e.endVert);
      e.pairedHalfedge = forwardEdge;
      halfedgeR[backwardEdge] = e;
      halfedgeRef[backwardEdge] = backwardRef;
    }
  }
}

struct DuplicateHalfedges {
  VecView<Halfedge> halfedgesR;
  VecView<TriRef> halfedgeRef;
  VecView<int> facePtr;
  VecView<const char> wholeHalfedgeP;
  VecView<const Halfedge> halfedgesP;
  VecView<const int> i03;
  VecView<const int> vP2R;
  VecView<const int> faceP2R;
  const bool forward;

  void operator()(const int idx) {
    if (!wholeHalfedgeP[idx]) return;
    Halfedge halfedge = halfedgesP[idx];
    if (!halfedge.IsForward()) return;

    const int inclusion = i03[halfedge.startVert];
    if (inclusion == 0) return;
    if (inclusion < 0) {  // reverse
      std::swap(halfedge.startVert, halfedge.endVert);
    }
    halfedge.startVert = vP2R[halfedge.startVert];
    halfedge.endVert = vP2R[halfedge.endVert];
    const int faceLeftP = idx / 3;
    const int newFace = faceP2R[faceLeftP];
    const int faceRightP = halfedge.pairedHalfedge / 3;
    const int faceRight = faceP2R[faceRightP];
    // Negative inclusion means the halfedges are reversed, which means our
    // reference is now to the endVert instead of the startVert, which is one
    // position advanced CCW.
    const TriRef forwardRef = {forward ? 0 : 1, -1, faceLeftP};
    const TriRef backwardRef = {forward ? 0 : 1, -1, faceRightP};

    for (int i = 0; i < std::abs(inclusion); ++i) {
      int forwardEdge = AtomicAdd(facePtr[newFace], 1);
      int backwardEdge = AtomicAdd(facePtr[faceRight], 1);
      halfedge.pairedHalfedge = backwardEdge;

      halfedgesR[forwardEdge] = halfedge;
      halfedgesR[backwardEdge] = {halfedge.endVert, halfedge.startVert,
                                  forwardEdge};
      halfedgeRef[forwardEdge] = forwardRef;
      halfedgeRef[backwardEdge] = backwardRef;

      ++halfedge.startVert;
      ++halfedge.endVert;
    }
  }
};

void AppendWholeEdges(Manifold::Impl &outR, Vec<int> &facePtrR,
                      Vec<TriRef> &halfedgeRef, const Manifold::Impl &inP,
                      const Vec<char> wholeHalfedgeP, const Vec<int> &i03,
                      const Vec<int> &vP2R, VecView<const int> faceP2R,
                      bool forward) {
  ZoneScoped;
  for_each_n(
      autoPolicy(inP.halfedge_.size()), countAt(0), inP.halfedge_.size(),
      DuplicateHalfedges({outR.halfedge_, halfedgeRef, facePtrR, wholeHalfedgeP,
                          inP.halfedge_, i03, vP2R, faceP2R, forward}));
}

struct MapTriRef {
  VecView<const TriRef> triRefP;
  VecView<const TriRef> triRefQ;
  const int offsetQ;

  void operator()(TriRef &triRef) {
    const int tri = triRef.tri;
    const bool PQ = triRef.meshID == 0;
    triRef = PQ ? triRefP[tri] : triRefQ[tri];
    if (!PQ) triRef.meshID += offsetQ;
  }
};

void UpdateReference(Manifold::Impl &outR, const Manifold::Impl &inP,
                     const Manifold::Impl &inQ, bool invertQ) {
  const int offsetQ = Manifold::Impl::meshIDCounter_;
  for_each_n(
      autoPolicy(outR.NumTri(), 1e5), outR.meshRelation_.triRef.begin(),
      outR.NumTri(),
      MapTriRef({inP.meshRelation_.triRef, inQ.meshRelation_.triRef, offsetQ}));

  for (const auto &pair : inP.meshRelation_.meshIDtransform) {
    outR.meshRelation_.meshIDtransform[pair.first] = pair.second;
  }
  for (const auto &pair : inQ.meshRelation_.meshIDtransform) {
    outR.meshRelation_.meshIDtransform[pair.first + offsetQ] = pair.second;
    outR.meshRelation_.meshIDtransform[pair.first + offsetQ].backSide ^=
        invertQ;
  }
}

struct Barycentric {
  VecView<vec3> uvw;
  VecView<const TriRef> ref;
  VecView<const vec3> vertPosP;
  VecView<const vec3> vertPosQ;
  VecView<const vec3> vertPosR;
  VecView<const Halfedge> halfedgeP;
  VecView<const Halfedge> halfedgeQ;
  VecView<const Halfedge> halfedgeR;
  const double epsilon;

  void operator()(const int tri) {
    const TriRef refPQ = ref[tri];
    if (halfedgeR[3 * tri].startVert < 0) return;

    const int triPQ = refPQ.tri;
    const bool PQ = refPQ.meshID == 0;
    const auto &vertPos = PQ ? vertPosP : vertPosQ;
    const auto &halfedge = PQ ? halfedgeP : halfedgeQ;

    mat3 triPos;
    for (const int j : {0, 1, 2})
      triPos[j] = vertPos[halfedge[3 * triPQ + j].startVert];

    for (const int i : {0, 1, 2}) {
      const int vert = halfedgeR[3 * tri + i].startVert;
      uvw[3 * tri + i] = GetBarycentric(vertPosR[vert], triPos, epsilon);
    }
  }
};

void CreateProperties(Manifold::Impl &outR, const Manifold::Impl &inP,
                      const Manifold::Impl &inQ) {
  ZoneScoped;
  const int numPropP = inP.NumProp();
  const int numPropQ = inQ.NumProp();
  const int numProp = std::max(numPropP, numPropQ);
  outR.meshRelation_.numProp = numProp;
  if (numProp == 0) return;

  const int numTri = outR.NumTri();
  outR.meshRelation_.triProperties.resize(numTri);

  Vec<vec3> bary(outR.halfedge_.size());
  for_each_n(autoPolicy(numTri, 1e4), countAt(0), numTri,
             Barycentric({bary, outR.meshRelation_.triRef, inP.vertPos_,
                          inQ.vertPos_, outR.vertPos_, inP.halfedge_,
                          inQ.halfedge_, outR.halfedge_, outR.epsilon_}));

  using Entry = std::pair<ivec3, int>;
  int idMissProp = outR.NumVert();
  std::vector<std::vector<Entry>> propIdx(outR.NumVert() + 1);
  std::vector<int> propMissIdx[2];
  propMissIdx[0].resize(inQ.NumPropVert(), -1);
  propMissIdx[1].resize(inP.NumPropVert(), -1);

  outR.meshRelation_.properties.reserve(outR.NumVert() * numProp);
  int idx = 0;

  for (int tri = 0; tri < numTri; ++tri) {
    // Skip collapsed triangles
    if (outR.halfedge_[3 * tri].startVert < 0) continue;

    const TriRef ref = outR.meshRelation_.triRef[tri];
    const bool PQ = ref.meshID == 0;
    const int oldNumProp = PQ ? numPropP : numPropQ;
    const auto &properties =
        PQ ? inP.meshRelation_.properties : inQ.meshRelation_.properties;
    const ivec3 &triProp = oldNumProp == 0 ? ivec3(-1)
                           : PQ ? inP.meshRelation_.triProperties[ref.tri]
                                : inQ.meshRelation_.triProperties[ref.tri];

    for (const int i : {0, 1, 2}) {
      const int vert = outR.halfedge_[3 * tri + i].startVert;
      const vec3 &uvw = bary[3 * tri + i];

      ivec4 key(PQ, idMissProp, -1, -1);
      if (oldNumProp > 0) {
        int edge = -2;
        for (const int j : {0, 1, 2}) {
          if (uvw[j] == 1) {
            // On a retained vert, the propVert must also match
            key[2] = triProp[j];
            edge = -1;
            break;
          }
          if (uvw[j] == 0) edge = j;
        }
        if (edge >= 0) {
          // On an edge, both propVerts must match
          const int p0 = triProp[Next3(edge)];
          const int p1 = triProp[Prev3(edge)];
          key[1] = vert;
          key[2] = std::min(p0, p1);
          key[3] = std::max(p0, p1);
        } else if (edge == -2) {
          key[1] = vert;
        }
      }

      if (key.y == idMissProp && key.z >= 0) {
        // only key.x/key.z matters
        auto &entry = propMissIdx[key.x][key.z];
        if (entry >= 0) {
          outR.meshRelation_.triProperties[tri][i] = entry;
          continue;
        }
        entry = idx;
      } else {
        auto &bin = propIdx[key.y];
        bool bFound = false;
        for (const auto &b : bin) {
          if (b.first == ivec3(key.x, key.z, key.w)) {
            bFound = true;
            outR.meshRelation_.triProperties[tri][i] = b.second;
            break;
          }
        }
        if (bFound) continue;
        bin.push_back(std::make_pair(ivec3(key.x, key.z, key.w), idx));
      }

      outR.meshRelation_.triProperties[tri][i] = idx++;
      for (int p = 0; p < numProp; ++p) {
        if (p < oldNumProp) {
          vec3 oldProps;
          for (const int j : {0, 1, 2})
            oldProps[j] = properties[oldNumProp * triProp[j] + p];
          outR.meshRelation_.properties.push_back(la::dot(uvw, oldProps));
        } else {
          outR.meshRelation_.properties.push_back(0);
        }
      }
    }
  }
}
}  // namespace

namespace manifold {

Manifold::Impl Boolean3::Result(OpType op) const {
#ifdef MANIFOLD_DEBUG
  Timer assemble;
  assemble.Start();
#endif

  DEBUG_ASSERT((expandP_ > 0) == (op == OpType::Add), logicErr,
               "Result op type not compatible with constructor op type.");
  const int c1 = op == OpType::Intersect ? 0 : 1;
  const int c2 = op == OpType::Add ? 1 : 0;
  const int c3 = op == OpType::Intersect ? 1 : -1;

  if (inP_.status_ != Manifold::Error::NoError) {
    auto impl = Manifold::Impl();
    impl.status_ = inP_.status_;
    return impl;
  }
  if (inQ_.status_ != Manifold::Error::NoError) {
    auto impl = Manifold::Impl();
    impl.status_ = inQ_.status_;
    return impl;
  }

  if (inP_.IsEmpty()) {
    if (!inQ_.IsEmpty() && op == OpType::Add) {
      return inQ_;
    }
    return Manifold::Impl();
  } else if (inQ_.IsEmpty()) {
    if (op == OpType::Intersect) {
      return Manifold::Impl();
    }
    return inP_;
  }

  const bool invertQ = op == OpType::Subtract;

  // Convert winding numbers to inclusion values based on operation type.
  Vec<int> i12(x12_.size());
  Vec<int> i21(x21_.size());
  Vec<int> i03(w03_.size());
  Vec<int> i30(w30_.size());

  transform(x12_.begin(), x12_.end(), i12.begin(),
            [c3](int v) { return c3 * v; });
  transform(x21_.begin(), x21_.end(), i21.begin(),
            [c3](int v) { return c3 * v; });
  transform(w03_.begin(), w03_.end(), i03.begin(),
            [c1, c3](int v) { return c1 + c3 * v; });
  transform(w30_.begin(), w30_.end(), i30.begin(),
            [c2, c3](int v) { return c2 + c3 * v; });

  Vec<int> vP2R(inP_.NumVert());
  exclusive_scan(i03.begin(), i03.end(), vP2R.begin(), 0, AbsSum());
  int numVertR = AbsSum()(vP2R.back(), i03.back());
  const int nPv = numVertR;

  Vec<int> vQ2R(inQ_.NumVert());
  exclusive_scan(i30.begin(), i30.end(), vQ2R.begin(), numVertR, AbsSum());
  numVertR = AbsSum()(vQ2R.back(), i30.back());
  const int nQv = numVertR - nPv;

  Vec<int> v12R(v12_.size());
  if (v12_.size() > 0) {
    exclusive_scan(i12.begin(), i12.end(), v12R.begin(), numVertR, AbsSum());
    numVertR = AbsSum()(v12R.back(), i12.back());
  }
  const int n12 = numVertR - nPv - nQv;

  Vec<int> v21R(v21_.size());
  if (v21_.size() > 0) {
    exclusive_scan(i21.begin(), i21.end(), v21R.begin(), numVertR, AbsSum());
    numVertR = AbsSum()(v21R.back(), i21.back());
  }
  const int n21 = numVertR - nPv - nQv - n12;

  // Create the output Manifold
  Manifold::Impl outR;

  if (numVertR == 0) return outR;

  outR.epsilon_ = std::max(inP_.epsilon_, inQ_.epsilon_);
  outR.tolerance_ = std::max(inP_.tolerance_, inQ_.tolerance_);

  outR.vertPos_.resize(numVertR);
  // Add vertices, duplicating for inclusion numbers not in [-1, 1].
  // Retained vertices from P and Q:
  for_each_n(autoPolicy(inP_.NumVert(), 1e4), countAt(0), inP_.NumVert(),
             DuplicateVerts({outR.vertPos_, i03, vP2R, inP_.vertPos_}));
  for_each_n(autoPolicy(inQ_.NumVert(), 1e4), countAt(0), inQ_.NumVert(),
             DuplicateVerts({outR.vertPos_, i30, vQ2R, inQ_.vertPos_}));
  // New vertices created from intersections:
  for_each_n(autoPolicy(i12.size(), 1e4), countAt(0), i12.size(),
             DuplicateVerts({outR.vertPos_, i12, v12R, v12_}));
  for_each_n(autoPolicy(i21.size(), 1e4), countAt(0), i21.size(),
             DuplicateVerts({outR.vertPos_, i21, v21R, v21_}));

  PRINT(nPv << " verts from inP");
  PRINT(nQv << " verts from inQ");
  PRINT(n12 << " new verts from edgesP -> facesQ");
  PRINT(n21 << " new verts from facesP -> edgesQ");

  // Build up new polygonal faces from triangle intersections. At this point the
  // calculation switches from parallel to serial.

  // Level 3

  // This key is the forward halfedge index of P or Q. Only includes intersected
  // edges.
  concurrent_map<int, std::vector<EdgePos>> edgesP, edgesQ;
  // This key is the face index of <P, Q>
  concurrent_map<std::pair<int, int>, std::vector<EdgePos>> edgesNew;

  AddNewEdgeVerts(edgesP, edgesNew, p1q2_, i12, v12R, inP_.halfedge_, true);
  AddNewEdgeVerts(edgesQ, edgesNew, p2q1_, i21, v21R, inQ_.halfedge_, false);

  v12R.clear();
  v21R.clear();

  // Level 4
  Vec<int> faceEdge;
  Vec<int> facePQ2R;
  std::tie(faceEdge, facePQ2R) =
      SizeOutput(outR, inP_, inQ_, i03, i30, i12, i21, p1q2_, p2q1_, invertQ);

  i12.clear();
  i21.clear();

  // This gets incremented for each halfedge that's added to a face so that the
  // next one knows where to slot in.
  Vec<int> facePtrR = faceEdge;
  // Intersected halfedges are marked false.
  Vec<char> wholeHalfedgeP(inP_.halfedge_.size(), true);
  Vec<char> wholeHalfedgeQ(inQ_.halfedge_.size(), true);
  // The halfedgeRef contains the data that will become triRef once the faces
  // are triangulated.
  Vec<TriRef> halfedgeRef(2 * outR.NumEdge());

  AppendPartialEdges(outR, wholeHalfedgeP, facePtrR, edgesP, halfedgeRef, inP_,
                     i03, vP2R, facePQ2R.begin(), true);
  AppendPartialEdges(outR, wholeHalfedgeQ, facePtrR, edgesQ, halfedgeRef, inQ_,
                     i30, vQ2R, facePQ2R.begin() + inP_.NumTri(), false);

  edgesP.clear();
  edgesQ.clear();

  AppendNewEdges(outR, facePtrR, edgesNew, halfedgeRef, facePQ2R,
                 inP_.NumTri());

  edgesNew.clear();

  AppendWholeEdges(outR, facePtrR, halfedgeRef, inP_, wholeHalfedgeP, i03, vP2R,
                   facePQ2R.cview(0, inP_.NumTri()), true);
  AppendWholeEdges(outR, facePtrR, halfedgeRef, inQ_, wholeHalfedgeQ, i30, vQ2R,
                   facePQ2R.cview(inP_.NumTri(), inQ_.NumTri()), false);

  wholeHalfedgeP.clear();
  wholeHalfedgeQ.clear();
  facePtrR.clear();
  facePQ2R.clear();
  i03.clear();
  i30.clear();
  vP2R.clear();
  vQ2R.clear();

#ifdef MANIFOLD_DEBUG
  assemble.Stop();
  Timer triangulate;
  triangulate.Start();
#endif

  // Level 6

  if (ManifoldParams().intermediateChecks)
    DEBUG_ASSERT(outR.IsManifold(), logicErr, "polygon mesh is not manifold!");

  outR.Face2Tri(faceEdge, halfedgeRef);
  halfedgeRef.clear();
  faceEdge.clear();

#ifdef MANIFOLD_DEBUG
  triangulate.Stop();
  Timer simplify;
  simplify.Start();
#endif

  if (ManifoldParams().intermediateChecks)
    DEBUG_ASSERT(outR.IsManifold(), logicErr,
                 "triangulated mesh is not manifold!");

  CreateProperties(outR, inP_, inQ_);

  UpdateReference(outR, inP_, inQ_, invertQ);

  outR.SimplifyTopology();

  if (ManifoldParams().intermediateChecks)
    DEBUG_ASSERT(outR.Is2Manifold(), logicErr,
                 "simplified mesh is not 2-manifold!");

#ifdef MANIFOLD_DEBUG
  simplify.Stop();
  Timer sort;
  sort.Start();
#endif

  outR.Finish();
  outR.IncrementMeshIDs();

#ifdef MANIFOLD_DEBUG
  sort.Stop();
  if (ManifoldParams().verbose) {
    assemble.Print("Assembly");
    triangulate.Print("Triangulation");
    simplify.Print("Simplification");
    sort.Print("Sorting");
    std::cout << outR.NumVert() << " verts and " << outR.NumTri() << " tris"
              << std::endl;
  }
#endif

  return outR;
}

}  // namespace manifold
