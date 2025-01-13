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
#include "manifold/common.h"

namespace manifold {

/** @addtogroup Structs
 *  @{
 */

/**
 * @brief Polygon vertex.
 */
struct PolyVert {
  /// X-Y position
  vec2 pos;
  /// ID or index into another vertex vector
  int idx;
};

/**
 * @brief Single polygon contour, wound CCW, with indices. First and last point
 * are implicitly connected. Should ensure all input is
 * [&epsilon;-valid](https://github.com/elalish/manifold/wiki/Manifold-Library#definition-of-%CE%B5-valid).
 */
using SimplePolygonIdx = std::vector<PolyVert>;

/**
 * @brief Set of indexed polygons with holes. Order of contours is arbitrary.
 * Can contain any depth of nested holes and any number of separate polygons.
 * Should ensure all input is
 * [&epsilon;-valid](https://github.com/elalish/manifold/wiki/Manifold-Library#definition-of-%CE%B5-valid).
 */
using PolygonsIdx = std::vector<SimplePolygonIdx>;
/** @} */

/** @addtogroup Triangulation
 *  @ingroup Core
 * @brief Polygon triangulation
 *  @{
 */
std::vector<ivec3> TriangulateIdx(const PolygonsIdx &polys,
                                  double epsilon = -1);

std::vector<ivec3> Triangulate(const Polygons &polygons, double epsilon = -1);

ExecutionParams &PolygonParams();
/** @} */
}  // namespace manifold
