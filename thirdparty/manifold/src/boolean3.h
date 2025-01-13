// Copyright 2020 The Manifold Authors.
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
#include "./impl.h"

#ifdef MANIFOLD_DEBUG
#define PRINT(msg) \
  if (ManifoldParams().verbose) std::cout << msg << std::endl;
#else
#define PRINT(msg)
#endif

/**
 * The notation in these files is abbreviated due to the complexity of the
 * functions involved. The key is that the input manifolds are P and Q, while
 * the output is R, and these letters in both upper and lower case refer to
 * these objects. Operations are based on dimensionality: vert: 0, edge: 1,
 * face: 2, solid: 3. X denotes a winding-number type quantity from the source
 * paper of this algorithm, while S is closely related but includes only the
 * subset of X values which "shadow" (are on the correct side of).
 *
 * Nearly everything here are sparse arrays, where for instance each pair in
 * p2q1 refers to a face index of P interacting with a halfedge index of Q.
 * Adjacent arrays like x21 refer to the values of X corresponding to each
 * sparse index pair.
 *
 * Note many functions are designed to work symmetrically, for instance for both
 * p2q1 and p1q2. Inside of these functions P and Q are marked as though the
 * function is forwards, but it may include a Boolean "reverse" that indicates P
 * and Q have been swapped.
 */

namespace manifold {

/** @ingroup Private */
class Boolean3 {
 public:
  Boolean3(const Manifold::Impl& inP, const Manifold::Impl& inQ, OpType op);
  Manifold::Impl Result(OpType op) const;

 private:
  const Manifold::Impl &inP_, &inQ_;
  const double expandP_;
  SparseIndices p1q2_, p2q1_;
  Vec<int> x12_, x21_, w03_, w30_;
  Vec<vec3> v12_, v21_;
};
}  // namespace manifold
