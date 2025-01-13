// Copyright 2022 The Manifold Authors.
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

#if (MANIFOLD_PAR == 1) && __has_include(<tbb/concurrent_priority_queue.h>)
#include <tbb/tbb.h>
#define TBB_PREVIEW_CONCURRENT_ORDERED_CONTAINERS 1
#include <tbb/concurrent_priority_queue.h>
#endif

#include <algorithm>

#include "./boolean3.h"
#include "./csg_tree.h"
#include "./impl.h"
#include "./mesh_fixes.h"
#include "./parallel.h"

constexpr int kParallelThreshold = 4096;

namespace {
using namespace manifold;
struct MeshCompare {
  bool operator()(const std::shared_ptr<CsgLeafNode> &a,
                  const std::shared_ptr<CsgLeafNode> &b) {
    return a->GetImpl()->NumVert() < b->GetImpl()->NumVert();
  }
};

}  // namespace
namespace manifold {

std::shared_ptr<CsgNode> CsgNode::Boolean(
    const std::shared_ptr<CsgNode> &second, OpType op) {
  if (second->GetNodeType() != CsgNodeType::Leaf) {
    // "this" is not a CsgOpNode (which overrides Boolean), but if "second" is
    // and the operation is commutative, we let it built the tree.
    if ((op == OpType::Add || op == OpType::Intersect)) {
      return std::static_pointer_cast<CsgOpNode>(second)->Boolean(
          shared_from_this(), op);
    }
  }
  std::vector<std::shared_ptr<CsgNode>> children({shared_from_this(), second});
  return std::make_shared<CsgOpNode>(children, op);
}

std::shared_ptr<CsgNode> CsgNode::Translate(const vec3 &t) const {
  mat3x4 transform = la::identity;
  transform[3] += t;
  return Transform(transform);
}

std::shared_ptr<CsgNode> CsgNode::Scale(const vec3 &v) const {
  mat3x4 transform;
  for (int i : {0, 1, 2}) transform[i][i] = v[i];
  return Transform(transform);
}

std::shared_ptr<CsgNode> CsgNode::Rotate(double xDegrees, double yDegrees,
                                         double zDegrees) const {
  mat3 rX({1.0, 0.0, 0.0},                        //
          {0.0, cosd(xDegrees), sind(xDegrees)},  //
          {0.0, -sind(xDegrees), cosd(xDegrees)});
  mat3 rY({cosd(yDegrees), 0.0, -sind(yDegrees)},  //
          {0.0, 1.0, 0.0},                         //
          {sind(yDegrees), 0.0, cosd(yDegrees)});
  mat3 rZ({cosd(zDegrees), sind(zDegrees), 0.0},   //
          {-sind(zDegrees), cosd(zDegrees), 0.0},  //
          {0.0, 0.0, 1.0});
  mat3x4 transform(rZ * rY * rX, vec3());
  return Transform(transform);
}

CsgLeafNode::CsgLeafNode() : pImpl_(std::make_shared<Manifold::Impl>()) {}

CsgLeafNode::CsgLeafNode(std::shared_ptr<const Manifold::Impl> pImpl_)
    : pImpl_(pImpl_) {}

CsgLeafNode::CsgLeafNode(std::shared_ptr<const Manifold::Impl> pImpl_,
                         mat3x4 transform_)
    : pImpl_(pImpl_), transform_(transform_) {}

std::shared_ptr<const Manifold::Impl> CsgLeafNode::GetImpl() const {
  if (transform_ == mat3x4(la::identity)) return pImpl_;
  pImpl_ =
      std::make_shared<const Manifold::Impl>(pImpl_->Transform(transform_));
  transform_ = la::identity;
  return pImpl_;
}

std::shared_ptr<CsgLeafNode> CsgLeafNode::ToLeafNode() const {
  return std::make_shared<CsgLeafNode>(*this);
}

std::shared_ptr<CsgNode> CsgLeafNode::Transform(const mat3x4 &m) const {
  return std::make_shared<CsgLeafNode>(pImpl_, m * Mat4(transform_));
}

CsgNodeType CsgLeafNode::GetNodeType() const { return CsgNodeType::Leaf; }

std::shared_ptr<CsgLeafNode> ImplToLeaf(Manifold::Impl &&impl) {
  return std::make_shared<CsgLeafNode>(std::make_shared<Manifold::Impl>(impl));
}

std::shared_ptr<CsgLeafNode> SimpleBoolean(const Manifold::Impl &a,
                                           const Manifold::Impl &b, OpType op) {
#ifdef MANIFOLD_DEBUG
  auto dump = [&]() {
    dump_lock.lock();
    std::cout << "LHS self-intersecting: " << a.IsSelfIntersecting()
              << std::endl;
    std::cout << "RHS self-intersecting: " << b.IsSelfIntersecting()
              << std::endl;
    if (ManifoldParams().verbose) {
      if (op == OpType::Add)
        std::cout << "Add";
      else if (op == OpType::Intersect)
        std::cout << "Intersect";
      else
        std::cout << "Subtract";
      std::cout << std::endl;
      std::cout << a;
      std::cout << b;
    }
    dump_lock.unlock();
  };
  try {
    Boolean3 boolean(a, b, op);
    auto impl = boolean.Result(op);
    if (ManifoldParams().intermediateChecks && impl.IsSelfIntersecting()) {
      dump_lock.lock();
      std::cout << "self intersections detected" << std::endl;
      dump_lock.unlock();
      throw logicErr("self intersection detected");
    }
    return ImplToLeaf(std::move(impl));
  } catch (logicErr &err) {
    dump();
    throw err;
  } catch (geometryErr &err) {
    dump();
    throw err;
  }
#else
  return ImplToLeaf(Boolean3(a, b, op).Result(op));
#endif
}

/**
 * Efficient union of a set of pairwise disjoint meshes.
 */
std::shared_ptr<CsgLeafNode> CsgLeafNode::Compose(
    const std::vector<std::shared_ptr<CsgLeafNode>> &nodes) {
  ZoneScoped;
  double epsilon = -1;
  double tolerance = -1;
  int numVert = 0;
  int numEdge = 0;
  int numTri = 0;
  int numPropVert = 0;
  std::vector<int> vertIndices;
  std::vector<int> edgeIndices;
  std::vector<int> triIndices;
  std::vector<int> propVertIndices;
  int numPropOut = 0;
  for (auto &node : nodes) {
    if (node->pImpl_->status_ != Manifold::Error::NoError) {
      Manifold::Impl impl;
      impl.status_ = node->pImpl_->status_;
      return ImplToLeaf(std::move(impl));
    }
    double nodeOldScale = node->pImpl_->bBox_.Scale();
    double nodeNewScale =
        node->pImpl_->bBox_.Transform(node->transform_).Scale();
    double nodeEpsilon = node->pImpl_->epsilon_;
    nodeEpsilon *= std::max(1.0, nodeNewScale / nodeOldScale);
    nodeEpsilon = std::max(nodeEpsilon, kPrecision * nodeNewScale);
    if (!std::isfinite(nodeEpsilon)) nodeEpsilon = -1;
    epsilon = std::max(epsilon, nodeEpsilon);
    tolerance = std::max(tolerance, node->pImpl_->tolerance_);

    vertIndices.push_back(numVert);
    edgeIndices.push_back(numEdge * 2);
    triIndices.push_back(numTri);
    propVertIndices.push_back(numPropVert);
    numVert += node->pImpl_->NumVert();
    numEdge += node->pImpl_->NumEdge();
    numTri += node->pImpl_->NumTri();
    const int numProp = node->pImpl_->NumProp();
    numPropOut = std::max(numPropOut, numProp);
    numPropVert +=
        numProp == 0 ? 1
                     : node->pImpl_->meshRelation_.properties.size() / numProp;
  }

  Manifold::Impl combined;
  combined.epsilon_ = epsilon;
  combined.tolerance_ = tolerance;
  combined.vertPos_.resize(numVert);
  combined.halfedge_.resize(2 * numEdge);
  combined.faceNormal_.resize(numTri);
  combined.halfedgeTangent_.resize(2 * numEdge);
  combined.meshRelation_.triRef.resize(numTri);
  if (numPropOut > 0) {
    combined.meshRelation_.numProp = numPropOut;
    combined.meshRelation_.properties.resize(numPropOut * numPropVert, 0);
    combined.meshRelation_.triProperties.resize(numTri);
  }
  auto policy = autoPolicy(numTri);

  // if we are already parallelizing for each node, do not perform multithreaded
  // copying as it will slightly hurt performance
  if (nodes.size() > 1 && policy == ExecutionPolicy::Par)
    policy = ExecutionPolicy::Seq;

  for_each_n(
      nodes.size() > 1 ? ExecutionPolicy::Par : ExecutionPolicy::Seq,
      countAt(0), nodes.size(),
      [&nodes, &vertIndices, &edgeIndices, &triIndices, &propVertIndices,
       numPropOut, &combined, policy](int i) {
        auto &node = nodes[i];
        copy(node->pImpl_->halfedgeTangent_.begin(),
             node->pImpl_->halfedgeTangent_.end(),
             combined.halfedgeTangent_.begin() + edgeIndices[i]);
        const int nextVert = vertIndices[i];
        const int nextEdge = edgeIndices[i];
        const int nextFace = triIndices[i];
        transform(node->pImpl_->halfedge_.begin(),
                  node->pImpl_->halfedge_.end(),
                  combined.halfedge_.begin() + edgeIndices[i],
                  [nextVert, nextEdge, nextFace](Halfedge edge) {
                    edge.startVert += nextVert;
                    edge.endVert += nextVert;
                    edge.pairedHalfedge += nextEdge;
                    return edge;
                  });

        if (numPropOut > 0) {
          auto start =
              combined.meshRelation_.triProperties.begin() + triIndices[i];
          if (node->pImpl_->NumProp() > 0) {
            auto &triProp = node->pImpl_->meshRelation_.triProperties;
            const int nextProp = propVertIndices[i];
            transform(triProp.begin(), triProp.end(), start,
                      [nextProp](ivec3 tri) {
                        tri += nextProp;
                        return tri;
                      });

            const int numProp = node->pImpl_->NumProp();
            auto &oldProp = node->pImpl_->meshRelation_.properties;
            auto &newProp = combined.meshRelation_.properties;
            for (int p = 0; p < numProp; ++p) {
              auto oldRange =
                  StridedRange(oldProp.cbegin() + p, oldProp.cend(), numProp);
              auto newRange = StridedRange(
                  newProp.begin() + numPropOut * propVertIndices[i] + p,
                  newProp.end(), numPropOut);
              copy(oldRange.begin(), oldRange.end(), newRange.begin());
            }
          } else {
            // point all triangles at single new property of zeros.
            fill(start, start + node->pImpl_->NumTri(),
                 ivec3(propVertIndices[i]));
          }
        }

        if (node->transform_ == mat3x4(la::identity)) {
          copy(node->pImpl_->vertPos_.begin(), node->pImpl_->vertPos_.end(),
               combined.vertPos_.begin() + vertIndices[i]);
          copy(node->pImpl_->faceNormal_.begin(),
               node->pImpl_->faceNormal_.end(),
               combined.faceNormal_.begin() + triIndices[i]);
        } else {
          // no need to apply the transform to the node, just copy the vertices
          // and face normals and apply transform on the fly
          const mat3x4 transform = node->transform_;
          auto vertPosBegin = TransformIterator(
              node->pImpl_->vertPos_.begin(), [&transform](vec3 position) {
                return transform * vec4(position, 1.0);
              });
          mat3 normalTransform =
              la::inverse(la::transpose(mat3(node->transform_)));
          auto faceNormalBegin =
              TransformIterator(node->pImpl_->faceNormal_.begin(),
                                TransformNormals({normalTransform}));
          copy_n(vertPosBegin, node->pImpl_->vertPos_.size(),
                 combined.vertPos_.begin() + vertIndices[i]);
          copy_n(faceNormalBegin, node->pImpl_->faceNormal_.size(),
                 combined.faceNormal_.begin() + triIndices[i]);

          const bool invert = la::determinant(mat3(node->transform_)) < 0;
          for_each_n(policy, countAt(0), node->pImpl_->halfedgeTangent_.size(),
                     TransformTangents{combined.halfedgeTangent_,
                                       edgeIndices[i], mat3(node->transform_),
                                       invert, node->pImpl_->halfedgeTangent_,
                                       node->pImpl_->halfedge_});
          if (invert)
            for_each_n(policy, countAt(triIndices[i]), node->pImpl_->NumTri(),
                       FlipTris({combined.halfedge_}));
        }
        // Since the nodes may be copies containing the same meshIDs, it is
        // important to add an offset so that each node instance gets
        // unique meshIDs.
        const int offset = i * Manifold::Impl::meshIDCounter_;
        transform(node->pImpl_->meshRelation_.triRef.begin(),
                  node->pImpl_->meshRelation_.triRef.end(),
                  combined.meshRelation_.triRef.begin() + triIndices[i],
                  [offset](TriRef ref) {
                    ref.meshID += offset;
                    return ref;
                  });
      });

  for (size_t i = 0; i < nodes.size(); i++) {
    auto &node = nodes[i];
    const int offset = i * Manifold::Impl::meshIDCounter_;

    for (const auto &pair : node->pImpl_->meshRelation_.meshIDtransform) {
      combined.meshRelation_.meshIDtransform[pair.first + offset] = pair.second;
    }
  }

  // required to remove parts that are smaller than the tolerance
  combined.SimplifyTopology();
  combined.Finish();
  combined.IncrementMeshIDs();
  return ImplToLeaf(std::move(combined));
}

/**
 * Efficient boolean operation on a set of nodes utilizing commutativity of the
 * operation. Only supports union and intersection.
 */
std::shared_ptr<CsgLeafNode> BatchBoolean(
    OpType operation, std::vector<std::shared_ptr<CsgLeafNode>> &results) {
  ZoneScoped;
  DEBUG_ASSERT(operation != OpType::Subtract, logicErr,
               "BatchBoolean doesn't support Difference.");
  // common cases
  if (results.size() == 0) return std::make_shared<CsgLeafNode>();
  if (results.size() == 1) return results.front();
  if (results.size() == 2)
    return SimpleBoolean(*results[0]->GetImpl(), *results[1]->GetImpl(),
                         operation);
#if (MANIFOLD_PAR == 1) && __has_include(<tbb/tbb.h>)
  tbb::task_group group;
  tbb::concurrent_priority_queue<std::shared_ptr<CsgLeafNode>, MeshCompare>
      queue(results.size());
  for (auto result : results) {
    queue.emplace(result);
  }
  results.clear();
  std::function<void()> process = [&]() {
    while (queue.size() > 1) {
      std::shared_ptr<CsgLeafNode> a, b;
      if (!queue.try_pop(a)) continue;
      if (!queue.try_pop(b)) {
        queue.push(a);
        continue;
      }
      group.run([&, a, b]() {
        queue.emplace(SimpleBoolean(*a->GetImpl(), *b->GetImpl(), operation));
        return group.run(process);
      });
    }
  };
  group.run_and_wait(process);
  std::shared_ptr<CsgLeafNode> r;
  queue.try_pop(r);
  return r;
#endif
  // apply boolean operations starting from smaller meshes
  // the assumption is that boolean operations on smaller meshes is faster,
  // due to less data being copied and processed
  auto cmpFn = MeshCompare();
  std::make_heap(results.begin(), results.end(), cmpFn);
  while (results.size() > 1) {
    std::pop_heap(results.begin(), results.end(), cmpFn);
    auto a = std::move(results.back());
    results.pop_back();
    std::pop_heap(results.begin(), results.end(), cmpFn);
    auto b = std::move(results.back());
    results.pop_back();
    // boolean operation
    auto result = SimpleBoolean(*a->GetImpl(), *b->GetImpl(), operation);
    if (results.size() == 0) return result;
    results.push_back(result);
    std::push_heap(results.begin(), results.end(), cmpFn);
  }
  return results.front();
}

/**
 * Efficient union operation on a set of nodes by doing Compose as much as
 * possible.
 */
std::shared_ptr<CsgLeafNode> BatchUnion(
    std::vector<std::shared_ptr<CsgLeafNode>> &children) {
  ZoneScoped;
  // INVARIANT: children_ is a vector of leaf nodes
  // this kMaxUnionSize is a heuristic to avoid the pairwise disjoint check
  // with O(n^2) complexity to take too long.
  // If the number of children exceeded this limit, we will operate on chunks
  // with size kMaxUnionSize.
  constexpr size_t kMaxUnionSize = 1000;
  DEBUG_ASSERT(!children.empty(), logicErr,
               "BatchUnion should not have empty children");
  while (children.size() > 1) {
    const size_t start = (children.size() > kMaxUnionSize)
                             ? (children.size() - kMaxUnionSize)
                             : 0;
    Vec<Box> boxes;
    boxes.reserve(children.size() - start);
    for (size_t i = start; i < children.size(); i++) {
      boxes.push_back(children[i]->GetImpl()->bBox_);
    }
    // partition the children into a set of disjoint sets
    // each set contains a set of children that are pairwise disjoint
    std::vector<Vec<size_t>> disjointSets;
    for (size_t i = 0; i < boxes.size(); i++) {
      auto lambda = [&boxes, i](const Vec<size_t> &set) {
        return std::find_if(set.begin(), set.end(), [&boxes, i](size_t j) {
                 return boxes[i].DoesOverlap(boxes[j]);
               }) == set.end();
      };
      auto it = std::find_if(disjointSets.begin(), disjointSets.end(), lambda);
      if (it == disjointSets.end()) {
        disjointSets.push_back(std::vector<size_t>{i});
      } else {
        it->push_back(i);
      }
    }
    // compose each set of disjoint children
    std::vector<std::shared_ptr<CsgLeafNode>> impls;
    for (auto &set : disjointSets) {
      if (set.size() == 1) {
        impls.push_back(children[start + set[0]]);
      } else {
        std::vector<std::shared_ptr<CsgLeafNode>> tmp;
        for (size_t j : set) {
          tmp.push_back(children[start + j]);
        }
        impls.push_back(CsgLeafNode::Compose(tmp));
      }
    }

    children.erase(children.begin() + start, children.end());
    children.push_back(BatchBoolean(OpType::Add, impls));
    // move it to the front as we process from the back, and the newly added
    // child should be quite complicated
    std::swap(children.front(), children.back());
  }
  return children.front();
}

CsgOpNode::CsgOpNode() {}

CsgOpNode::CsgOpNode(const std::vector<std::shared_ptr<CsgNode>> &children,
                     OpType op)
    : impl_(children), op_(op) {}

CsgOpNode::~CsgOpNode() {
  if (impl_.UseCount() == 1) {
    auto impl = impl_.GetGuard();
    std::vector<std::shared_ptr<CsgOpNode>> toProcess;
    auto handleChildren =
        [&toProcess](std::vector<std::shared_ptr<CsgNode>> &children) {
          while (!children.empty()) {
            // move out so shrinking the vector will not trigger recursive drop
            auto movedChild = std::move(children.back());
            children.pop_back();
            if (movedChild->GetNodeType() != CsgNodeType::Leaf)
              toProcess.push_back(
                  std::static_pointer_cast<CsgOpNode>(std::move(movedChild)));
          }
        };
    handleChildren(*impl);
    while (!toProcess.empty()) {
      auto child = std::move(toProcess.back());
      toProcess.pop_back();
      if (impl_.UseCount() == 1) {
        auto childImpl = child->impl_.GetGuard();
        handleChildren(*childImpl);
      }
    }
  }
}

std::shared_ptr<CsgNode> CsgOpNode::Boolean(
    const std::shared_ptr<CsgNode> &second, OpType op) {
  std::vector<std::shared_ptr<CsgNode>> children;
  children.push_back(shared_from_this());
  children.push_back(second);

  return std::make_shared<CsgOpNode>(children, op);
}

std::shared_ptr<CsgNode> CsgOpNode::Transform(const mat3x4 &m) const {
  auto node = std::make_shared<CsgOpNode>();
  node->impl_ = impl_;
  node->transform_ = m * Mat4(transform_);
  node->op_ = op_;
  return node;
}

struct CsgStackFrame {
  using Nodes = std::vector<std::shared_ptr<CsgLeafNode>>;

  bool finalize;
  OpType parent_op;
  mat3x4 transform;
  Nodes *positive_dest;
  Nodes *negative_dest;
  std::shared_ptr<const CsgOpNode> op_node;
  Nodes positive_children;
  Nodes negative_children;

  CsgStackFrame(bool finalize, OpType parent_op, mat3x4 transform,
                Nodes *positive_dest, Nodes *negative_dest,
                std::shared_ptr<const CsgOpNode> op_node)
      : finalize(finalize),
        parent_op(parent_op),
        transform(transform),
        positive_dest(positive_dest),
        negative_dest(negative_dest),
        op_node(op_node) {}
};

std::shared_ptr<CsgLeafNode> CsgOpNode::ToLeafNode() const {
  if (cache_ != nullptr) return cache_;

  // Note: We do need a pointer here to avoid vector pointers from being
  // invalidated after pushing elements into the explicit stack.
  // It is a `shared_ptr` because we may want to drop the stack frame while
  // still referring to some of the elements inside the old frame.
  // It is possible to use `unique_ptr`, extending the lifetime of the frame
  // when we remove it from the stack, but it is a bit more complicated and
  // there is no measurable overhead from using `shared_ptr` here...
  std::vector<std::shared_ptr<CsgStackFrame>> stack;
  // initial node, positive_dest is a nullptr because we don't need to put the
  // result anywhere else (except in the cache_).
  stack.push_back(std::make_shared<CsgStackFrame>(
      false, op_, la::identity, nullptr, nullptr,
      std::static_pointer_cast<const CsgOpNode>(shared_from_this())));

  // Instead of actually using recursion in the algorithm, we use an explicit
  // stack, do DFS and store the intermediate states into `CsgStackFrame` to
  // avoid stack overflow.
  //
  // Before performing boolean operations, we should make sure that all children
  // are `CsgLeafNodes`, i.e. are actual meshes that can be operated on. Hence,
  // we do it in two steps:
  // 1. Populate `children` (`positive_children` and `negative_children`)
  //    If the child is a `CsgOpNode`, we either collapse it or compute its
  //    boolean operation result.
  // 2. Performs boolean after populating the `children` set.
  //    After a boolean operation is completed, we put the result back to its
  //    parent's `children` set.
  //
  // When we populate `children`, we perform collapsing on-the-fly.
  // For example, we want to turn `(Union a (Union b c))` into `(Union a b c)`.
  // This allows more efficient `BatchBoolean`/`BatchUnion` calls.
  // We can do this when the child operation is the same as the parent
  // operation, except when the operation is `Subtract` (see below).
  // Note that to avoid repeating work, we will not collapse nodes that are
  // reused. And in the special case where the children set only contains one
  // element, we don't need any operation, so we can collapse that as well.
  // Instead of moving `b` and `c` into the parent, and running this collapsing
  // check until a fixed point, we remember the `positive_dest` where we should
  // put the `CsgLeafNode` into. Normally, the `positive_dest` pointer point to
  // the parent `children` set. However, when a child is being collapsed, we
  // keep using the old `positive_dest` pointer for the grandchildren. Hence,
  // removing a node by collapsing takes O(1) time. We also need to store the
  // parent operation type for checking if the node is eligible for collapsing,
  // and transform matrix because we need to re-apply the transformation to the
  // children.
  //
  // `Subtract` is handled differently from `Add` and `Intersect`.
  // For the first operand, it is treated as normal subtract. Negative children
  // in this operand is propagated to the parent, which is equivalent to
  // collapsing `(a - b) - c` into `a - (b + c)`.
  // For the remaining operands, they are treated as a nested `Add` node,
  // collapsing `a - (b + (c + d))` into `a - (b + c + d)`.
  //
  // `impl` should always contain either the raw set of children or
  // the NOT transformed result, while `cache_` should contain the transformed
  // result. This is because `impl` can be shared between `CsgOpNode` that
  // differ in `transform_`, so we want it to be able to share the result.
  // ===========================================================================
  // Recursive version (pseudocode only):
  //
  // void f(CsgOpNode node, OpType parent_op, mat3x4 transform,
  //        Nodes *positive_dest, Nodes *negative_dest) {
  //   auto impl = node->impl_.GetGuard();
  //   // can collapse when we have the same operation as the parent and is
  //   // unique, or when we have only one children.
  //   const OpType op = node->op_;
  //   const bool canCollapse = (op == parent_op && IsUnique(node)) ||
  //                            impl->size() == 1;
  //   const mat3x4 transform2 = canCollapse ? transform * node->transform_
  //                                         : la::identity;
  //   Nodes positive_children, negative_children;
  //   Nodes* pos_dest = canCollapse ? positive_dest : &positive_children;
  //   Nodes* neg_dest = canCollapse ? negative_dest : &negative_children;
  //   for (size_t i = 0; i < impl->size(); i++) {
  //     auto child = (*impl)[i];
  //     const bool negative = op == OpType::Subtract && i != 0;
  //     Nodes *dest1 = negative ? neg_dest : pos_dest;
  //     Nodes *dest2 = (op == OpType::Subtract && i == 0) ?
  //                      neg_dest : nullptr;
  //     if (child->GetNodeType() == CsgNodeType::Leaf)
  //       dest1.push_back(child);
  //     else
  //       f(child, op, transform2, dest1, dest2);
  //   }
  //   if (canCollapse) return;
  //   if (node->op_ == OpType::Add)
  //     *impl = {BatchUnion(positive_children)};
  //   else if (node->op_ == OpType::Intersect)
  //     *impl = {BatchBoolean(Intersect, positive_children)};
  //   else // subtract
  //     *impl = { BatchUnion(positive_children) -
  //                         BatchUnion(negative_children)};
  //   // node local transform
  //   node->cache_ = (*impl)[0].Transform(node.transform);
  //   // collapsed node transforms
  //   if (destination)
  //     destination->push_back(node->cache_->Transform(transform));
  // }
  while (!stack.empty()) {
    std::shared_ptr<CsgStackFrame> frame = stack.back();
    auto impl = frame->op_node->impl_.GetGuard();
    if (frame->finalize) {
      switch (frame->op_node->op_) {
        case OpType::Add:
          *impl = {BatchUnion(frame->positive_children)};
          break;
        case OpType::Intersect: {
          *impl = {BatchBoolean(OpType::Intersect, frame->positive_children)};
          break;
        };
        case OpType::Subtract:
          if (frame->positive_children.empty()) {
            // nothing to subtract from, so the result is empty.
            *impl = {std::make_shared<CsgLeafNode>()};
          } else {
            auto positive = BatchUnion(frame->positive_children);
            if (frame->negative_children.empty()) {
              // nothing to subtract, result equal to the LHS.
              *impl = {frame->positive_children[0]};
            } else {
              auto negative = BatchUnion(frame->negative_children);
              *impl = {SimpleBoolean(*positive->GetImpl(), *negative->GetImpl(),
                                     OpType::Subtract)};
            }
          }
          break;
      }
      frame->op_node->cache_ = std::static_pointer_cast<CsgLeafNode>(
          (*impl)[0]->Transform(frame->op_node->transform_));
      if (frame->positive_dest != nullptr)
        frame->positive_dest->push_back(std::static_pointer_cast<CsgLeafNode>(
            frame->op_node->cache_->Transform(frame->transform)));
      stack.pop_back();
    } else {
      auto add_children =
          [&stack](std::shared_ptr<CsgNode> &node, OpType op, mat3x4 transform,
                   CsgStackFrame::Nodes *dest1, CsgStackFrame::Nodes *dest2) {
            if (node->GetNodeType() == CsgNodeType::Leaf)
              dest1->push_back(std::static_pointer_cast<CsgLeafNode>(
                  node->Transform(transform)));
            else
              stack.push_back(std::make_shared<CsgStackFrame>(
                  false, op, transform, dest1, dest2,
                  std::static_pointer_cast<const CsgOpNode>(node)));
          };
      // op_node use_count == 2 because it is both inside one CsgOpNode
      // and in our stack.
      // if there is only one child, we can also collapse.
      const OpType op = frame->op_node->op_;
      const bool canCollapse =
          frame->positive_dest != nullptr &&
          ((op == frame->parent_op && frame->op_node.use_count() <= 2 &&
            frame->op_node->impl_.UseCount() == 1) ||
           impl->size() == 1);
      if (canCollapse)
        stack.pop_back();
      else
        frame->finalize = true;

      const mat3x4 transform =
          canCollapse ? (frame->transform * Mat4(frame->op_node->transform_))
                      : la::identity;
      CsgStackFrame::Nodes *pos_dest =
          canCollapse ? frame->positive_dest : &frame->positive_children;
      CsgStackFrame::Nodes *neg_dest =
          canCollapse ? frame->negative_dest : &frame->negative_children;
      for (size_t i = 0; i < impl->size(); i++) {
        const bool negative = op == OpType::Subtract && i != 0;
        CsgStackFrame::Nodes *dest1 = negative ? neg_dest : pos_dest;
        CsgStackFrame::Nodes *dest2 =
            (op == OpType::Subtract && i == 0) ? neg_dest : nullptr;
        add_children((*impl)[i], negative ? OpType::Add : op, transform, dest1,
                     dest2);
      }
    }
  }
  return cache_;
}

CsgNodeType CsgOpNode::GetNodeType() const {
  switch (op_) {
    case OpType::Add:
      return CsgNodeType::Union;
    case OpType::Subtract:
      return CsgNodeType::Difference;
    case OpType::Intersect:
      return CsgNodeType::Intersection;
  }
  // unreachable...
  return CsgNodeType::Leaf;
}

}  // namespace manifold
