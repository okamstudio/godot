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
#include "./parallel.h"
#include "./sparse.h"
#include "./utils.h"
#include "./vec.h"
#include "manifold/common.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#if (MANIFOLD_PAR == 1)
#include <tbb/combinable.h>
#endif

namespace manifold {

namespace collider_internal {
// Adjustable parameters
constexpr int kInitialLength = 128;
constexpr int kLengthMultiple = 4;
constexpr int kSequentialThreshold = 512;
// Fundamental constants
constexpr int kRoot = 1;

#ifdef _MSC_VER

#ifndef _WINDEF_
typedef unsigned long DWORD;
#endif

uint32_t inline ctz(uint32_t value) {
  DWORD trailing_zero = 0;

  if (_BitScanForward(&trailing_zero, value)) {
    return trailing_zero;
  } else {
    // This is undefined, I better choose 32 than 0
    return 32;
  }
}

uint32_t inline clz(uint32_t value) {
  DWORD leading_zero = 0;

  if (_BitScanReverse(&leading_zero, value)) {
    return 31 - leading_zero;
  } else {
    // Same remarks as above
    return 32;
  }
}
#endif

constexpr inline bool IsLeaf(int node) { return node % 2 == 0; }
constexpr inline bool IsInternal(int node) { return node % 2 == 1; }
constexpr inline int Node2Internal(int node) { return (node - 1) / 2; }
constexpr inline int Internal2Node(int internal) { return internal * 2 + 1; }
constexpr inline int Node2Leaf(int node) { return node / 2; }
constexpr inline int Leaf2Node(int leaf) { return leaf * 2; }

struct CreateRadixTree {
  VecView<int> nodeParent_;
  VecView<std::pair<int, int>> internalChildren_;
  const VecView<const uint32_t> leafMorton_;

  int PrefixLength(uint32_t a, uint32_t b) const {
// count-leading-zeros is used to find the number of identical highest-order
// bits
#ifdef _MSC_VER
    // return __lzcnt(a ^ b);
    return clz(a ^ b);
#else
    return __builtin_clz(a ^ b);
#endif
  }

  int PrefixLength(int i, int j) const {
    if (j < 0 || j >= static_cast<int>(leafMorton_.size())) {
      return -1;
    } else {
      int out;
      if (leafMorton_[i] == leafMorton_[j])
        // use index to disambiguate
        out = 32 +
              PrefixLength(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
      else
        out = PrefixLength(leafMorton_[i], leafMorton_[j]);
      return out;
    }
  }

  int RangeEnd(int i) const {
    // Determine direction of range (+1 or -1)
    int dir = PrefixLength(i, i + 1) - PrefixLength(i, i - 1);
    dir = (dir > 0) - (dir < 0);
    // Compute conservative range length with exponential increase
    int commonPrefix = PrefixLength(i, i - dir);
    int max_length = kInitialLength;
    while (PrefixLength(i, i + dir * max_length) > commonPrefix)
      max_length *= kLengthMultiple;
    // Compute precise range length with binary search
    int length = 0;
    for (int step = max_length / 2; step > 0; step /= 2) {
      if (PrefixLength(i, i + dir * (length + step)) > commonPrefix)
        length += step;
    }
    return i + dir * length;
  }

  int FindSplit(int first, int last) const {
    int commonPrefix = PrefixLength(first, last);
    // Find the furthest object that shares more than commonPrefix bits with the
    // first one, using binary search.
    int split = first;
    int step = last - first;
    do {
      step = (step + 1) >> 1;  // divide by 2, rounding up
      int newSplit = split + step;
      if (newSplit < last) {
        int splitPrefix = PrefixLength(first, newSplit);
        if (splitPrefix > commonPrefix) split = newSplit;
      }
    } while (step > 1);
    return split;
  }

  void operator()(int internal) {
    int first = internal;
    // Find the range of objects with a common prefix
    int last = RangeEnd(first);
    if (first > last) std::swap(first, last);
    // Determine where the next-highest difference occurs
    int split = FindSplit(first, last);
    int child1 = split == first ? Leaf2Node(split) : Internal2Node(split);
    ++split;
    int child2 = split == last ? Leaf2Node(split) : Internal2Node(split);
    // Record parent_child relationships.
    internalChildren_[internal].first = child1;
    internalChildren_[internal].second = child2;
    int node = Internal2Node(internal);
    nodeParent_[child1] = node;
    nodeParent_[child2] = node;
  }
};

template <typename T, const bool selfCollision, typename Recorder>
struct FindCollision {
  VecView<const T> queries;
  VecView<const Box> nodeBBox_;
  VecView<const std::pair<int, int>> internalChildren_;
  Recorder recorder;

  inline int RecordCollision(int node, const int queryIdx, SparseIndices& ind) {
    bool overlaps = nodeBBox_[node].DoesOverlap(queries[queryIdx]);
    if (overlaps && IsLeaf(node)) {
      int leafIdx = Node2Leaf(node);
      if (!selfCollision || leafIdx != queryIdx) {
        recorder.record(queryIdx, leafIdx, ind);
      }
    }
    return overlaps && IsInternal(node);  // Should traverse into node
  }

  void operator()(const int queryIdx) {
    // stack cannot overflow because radix tree has max depth 30 (Morton code) +
    // 32 (index).
    int stack[64];
    int top = -1;
    // Depth-first search
    int node = kRoot;
    SparseIndices& ind = recorder.local();
    while (1) {
      int internal = Node2Internal(node);
      int child1 = internalChildren_[internal].first;
      int child2 = internalChildren_[internal].second;

      int traverse1 = RecordCollision(child1, queryIdx, ind);
      int traverse2 = RecordCollision(child2, queryIdx, ind);

      if (!traverse1 && !traverse2) {
        if (top < 0) break;   // done
        node = stack[top--];  // get a saved node
      } else {
        node = traverse1 ? child1 : child2;  // go here next
        if (traverse1 && traverse2) {
          stack[++top] = child2;  // save the other for later
        }
      }
    }
  }
};

template <const bool inverted>
struct SeqCollisionRecorder {
  SparseIndices& queryTri_;
  inline void record(int queryIdx, int leafIdx, SparseIndices& ind) const {
    if (inverted)
      ind.Add(leafIdx, queryIdx);
    else
      ind.Add(queryIdx, leafIdx);
  }
  SparseIndices& local() { return queryTri_; }
};

#if (MANIFOLD_PAR == 1)
template <const bool inverted>
struct ParCollisionRecorder {
  tbb::combinable<SparseIndices>& store;
  inline void record(int queryIdx, int leafIdx, SparseIndices& ind) const {
    // Add may invoke something in parallel, and it may return in
    // another thread, making thread local unsafe
    // we need to explicitly forbid parallelization by passing a flag
    if (inverted)
      ind.Add(leafIdx, queryIdx, true);
    else
      ind.Add(queryIdx, leafIdx, true);
  }
  SparseIndices& local() { return store.local(); }
};
#endif

struct BuildInternalBoxes {
  VecView<Box> nodeBBox_;
  VecView<int> counter_;
  const VecView<int> nodeParent_;
  const VecView<std::pair<int, int>> internalChildren_;

  void operator()(int leaf) {
    int node = Leaf2Node(leaf);
    do {
      node = nodeParent_[node];
      int internal = Node2Internal(node);
      if (AtomicAdd(counter_[internal], 1) == 0) return;
      nodeBBox_[node] = nodeBBox_[internalChildren_[internal].first].Union(
          nodeBBox_[internalChildren_[internal].second]);
    } while (node != kRoot);
  }
};

struct TransformBox {
  const mat3x4 transform;
  void operator()(Box& box) { box = box.Transform(transform); }
};

constexpr inline uint32_t SpreadBits3(uint32_t v) {
  v = 0xFF0000FFu & (v * 0x00010001u);
  v = 0x0F00F00Fu & (v * 0x00000101u);
  v = 0xC30C30C3u & (v * 0x00000011u);
  v = 0x49249249u & (v * 0x00000005u);
  return v;
}
}  // namespace collider_internal

/** @ingroup Private */
class Collider {
 public:
  Collider() {};

  Collider(const VecView<const Box>& leafBB,
           const VecView<const uint32_t>& leafMorton) {
    ZoneScoped;
    DEBUG_ASSERT(leafBB.size() == leafMorton.size(), userErr,
                 "vectors must be the same length");
    int num_nodes = 2 * leafBB.size() - 1;
    // assign and allocate members
    nodeBBox_.resize(num_nodes);
    nodeParent_.resize(num_nodes, -1);
    internalChildren_.resize(leafBB.size() - 1, std::make_pair(-1, -1));
    // organize tree
    for_each_n(autoPolicy(NumInternal(), 1e4), countAt(0), NumInternal(),
               collider_internal::CreateRadixTree(
                   {nodeParent_, internalChildren_, leafMorton}));
    UpdateBoxes(leafBB);
  }

  bool Transform(mat3x4 transform) {
    ZoneScoped;
    bool axisAligned = true;
    for (int row : {0, 1, 2}) {
      int count = 0;
      for (int col : {0, 1, 2}) {
        if (transform[col][row] == 0.0) ++count;
      }
      if (count != 2) axisAligned = false;
    }
    if (axisAligned) {
      for_each(autoPolicy(nodeBBox_.size(), 1e5), nodeBBox_.begin(),
               nodeBBox_.end(),
               [transform](Box& box) { box = box.Transform(transform); });
    }
    return axisAligned;
  }

  void UpdateBoxes(const VecView<const Box>& leafBB) {
    ZoneScoped;
    DEBUG_ASSERT(leafBB.size() == NumLeaves(), userErr,
                 "must have the same number of updated boxes as original");
    // copy in leaf node Boxes
    auto leaves = StridedRange(nodeBBox_.begin(), nodeBBox_.end(), 2);
    copy(leafBB.cbegin(), leafBB.cend(), leaves.begin());
    // create global counters
    Vec<int> counter(NumInternal(), 0);
    // kernel over leaves to save internal Boxes
    for_each_n(autoPolicy(NumInternal(), 1e3), countAt(0), NumLeaves(),
               collider_internal::BuildInternalBoxes(
                   {nodeBBox_, counter, nodeParent_, internalChildren_}));
  }

  template <const bool selfCollision = false, const bool inverted = false,
            typename T>
  void Collisions(const VecView<const T>& queriesIn,
                  SparseIndices& queryTri) const {
    ZoneScoped;
    using collider_internal::FindCollision;
#if (MANIFOLD_PAR == 1)
    if (queriesIn.size() > collider_internal::kSequentialThreshold) {
      tbb::combinable<SparseIndices> store;
      for_each_n(
          ExecutionPolicy::Par, countAt(0), queriesIn.size(),
          FindCollision<T, selfCollision,
                        collider_internal::ParCollisionRecorder<inverted>>{
              queriesIn, nodeBBox_, internalChildren_, {store}});

      std::vector<SparseIndices> tmp;
      store.combine_each(
          [&](SparseIndices& ind) { tmp.emplace_back(std::move(ind)); });
      queryTri.FromIndices(tmp);
      return;
    }
#endif
    for_each_n(ExecutionPolicy::Seq, countAt(0), queriesIn.size(),
               FindCollision<T, selfCollision,
                             collider_internal::SeqCollisionRecorder<inverted>>{
                   queriesIn, nodeBBox_, internalChildren_, {queryTri}});
  }

  template <const bool selfCollision = false, const bool inverted = false,
            typename T>
  SparseIndices Collisions(const VecView<const T>& queriesIn) const {
    SparseIndices result;
    Collisions<selfCollision, inverted, T>(queriesIn, result);
    return result;
  }

  static uint32_t MortonCode(vec3 position, Box bBox) {
    using collider_internal::SpreadBits3;
    vec3 xyz = (position - bBox.min) / (bBox.max - bBox.min);
    xyz = la::min(vec3(1023.0), la::max(vec3(0.0), 1024.0 * xyz));
    uint32_t x = SpreadBits3(static_cast<uint32_t>(xyz.x));
    uint32_t y = SpreadBits3(static_cast<uint32_t>(xyz.y));
    uint32_t z = SpreadBits3(static_cast<uint32_t>(xyz.z));
    return x * 4 + y * 2 + z;
  }

 private:
  Vec<Box> nodeBBox_;
  Vec<int> nodeParent_;
  // even nodes are leaves, odd nodes are internal, root is 1
  Vec<std::pair<int, int>> internalChildren_;

  size_t NumInternal() const { return internalChildren_.size(); };
  size_t NumLeaves() const {
    return internalChildren_.empty() ? 0 : (NumInternal() + 1);
  };
};

}  // namespace manifold
