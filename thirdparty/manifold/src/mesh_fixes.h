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
#pragma once
#include "./shared.h"

namespace {
using namespace manifold;

inline int FlipHalfedge(int halfedge) {
  const int tri = halfedge / 3;
  const int vert = 2 - (halfedge - 3 * tri);
  return 3 * tri + vert;
}

struct TransformNormals {
  mat3 transform;

  vec3 operator()(vec3 normal) const {
    normal = la::normalize(transform * normal);
    if (std::isnan(normal.x)) normal = vec3(0.0);
    return normal;
  }
};

struct TransformTangents {
  VecView<vec4> tangent;
  const int edgeOffset;
  const mat3 transform;
  const bool invert;
  VecView<const vec4> oldTangents;
  VecView<const Halfedge> halfedge;

  void operator()(const int edgeOut) {
    const int edgeIn =
        invert ? halfedge[FlipHalfedge(edgeOut)].pairedHalfedge : edgeOut;
    tangent[edgeOut + edgeOffset] =
        vec4(transform * vec3(oldTangents[edgeIn]), oldTangents[edgeIn].w);
  }
};

struct FlipTris {
  VecView<Halfedge> halfedge;

  void operator()(const int tri) {
    std::swap(halfedge[3 * tri], halfedge[3 * tri + 2]);

    for (const int i : {0, 1, 2}) {
      std::swap(halfedge[3 * tri + i].startVert, halfedge[3 * tri + i].endVert);
      halfedge[3 * tri + i].pairedHalfedge =
          FlipHalfedge(halfedge[3 * tri + i].pairedHalfedge);
    }
  }
};
}  // namespace
