// Copyright 2023 The Manifold Authors.
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

#include "./hashtable.h"
#include "./impl.h"
#include "./parallel.h"
#include "./utils.h"
#include "./vec.h"
#include "manifold/manifold.h"

namespace {
using namespace manifold;

constexpr int kCrossing = -2;
constexpr int kNone = -1;
constexpr ivec4 kVoxelOffset(1, 1, 1, 0);
// Maximum fraction of spacing that a vert can move.
constexpr double kS = 0.25;
// Corresponding approximate distance ratio bound.
constexpr double kD = 1 / kS - 1;
// Maximum number of opposed verts (of 7) to allow collapse.
constexpr int kMaxOpposed = 3;

ivec3 TetTri0(int i) {
  constexpr ivec3 tetTri0[16] = {{-1, -1, -1},  //
                                 {0, 3, 4},     //
                                 {0, 1, 5},     //
                                 {1, 5, 3},     //
                                 {1, 4, 2},     //
                                 {1, 0, 3},     //
                                 {2, 5, 0},     //
                                 {5, 3, 2},     //
                                 {2, 3, 5},     //
                                 {0, 5, 2},     //
                                 {3, 0, 1},     //
                                 {2, 4, 1},     //
                                 {3, 5, 1},     //
                                 {5, 1, 0},     //
                                 {4, 3, 0},     //
                                 {-1, -1, -1}};
  return tetTri0[i];
}

ivec3 TetTri1(int i) {
  constexpr ivec3 tetTri1[16] = {{-1, -1, -1},  //
                                 {-1, -1, -1},  //
                                 {-1, -1, -1},  //
                                 {3, 4, 1},     //
                                 {-1, -1, -1},  //
                                 {3, 2, 1},     //
                                 {0, 4, 2},     //
                                 {-1, -1, -1},  //
                                 {-1, -1, -1},  //
                                 {2, 4, 0},     //
                                 {1, 2, 3},     //
                                 {-1, -1, -1},  //
                                 {1, 4, 3},     //
                                 {-1, -1, -1},  //
                                 {-1, -1, -1},  //
                                 {-1, -1, -1}};
  return tetTri1[i];
}

ivec4 Neighbor(ivec4 base, int i) {
  constexpr ivec4 neighbors[14] = {{0, 0, 0, 1},     //
                                   {1, 0, 0, 0},     //
                                   {0, 1, 0, 0},     //
                                   {0, 0, 1, 0},     //
                                   {-1, 0, 0, 1},    //
                                   {0, -1, 0, 1},    //
                                   {0, 0, -1, 1},    //
                                   {-1, -1, -1, 1},  //
                                   {-1, 0, 0, 0},    //
                                   {0, -1, 0, 0},    //
                                   {0, 0, -1, 0},    //
                                   {0, -1, -1, 1},   //
                                   {-1, 0, -1, 1},   //
                                   {-1, -1, 0, 1}};
  ivec4 neighborIndex = base + neighbors[i];
  if (neighborIndex.w == 2) {
    neighborIndex += 1;
    neighborIndex.w = 0;
  }
  return neighborIndex;
}

Uint64 EncodeIndex(ivec4 gridPos, ivec3 gridPow) {
  return static_cast<Uint64>(gridPos.w) | static_cast<Uint64>(gridPos.z) << 1 |
         static_cast<Uint64>(gridPos.y) << (1 + gridPow.z) |
         static_cast<Uint64>(gridPos.x) << (1 + gridPow.z + gridPow.y);
}

ivec4 DecodeIndex(Uint64 idx, ivec3 gridPow) {
  ivec4 gridPos;
  gridPos.w = idx & 1;
  idx = idx >> 1;
  gridPos.z = idx & ((1 << gridPow.z) - 1);
  idx = idx >> gridPow.z;
  gridPos.y = idx & ((1 << gridPow.y) - 1);
  idx = idx >> gridPow.y;
  gridPos.x = idx & ((1 << gridPow.x) - 1);
  return gridPos;
}

vec3 Position(ivec4 gridIndex, vec3 origin, vec3 spacing) {
  return origin + spacing * (vec3(gridIndex) + (gridIndex.w == 1 ? 0.0 : -0.5));
}

vec3 Bound(vec3 pos, vec3 origin, vec3 spacing, ivec3 gridSize) {
  return min(max(pos, origin), origin + spacing * (vec3(gridSize) - 1));
}

double BoundedSDF(ivec4 gridIndex, vec3 origin, vec3 spacing, ivec3 gridSize,
                  double level, std::function<double(vec3)> sdf) {
  const ivec3 xyz(gridIndex);
  const int lowerBoundDist = minelem(xyz);
  const int upperBoundDist = minelem(gridSize - xyz);
  const int boundDist = std::min(lowerBoundDist, upperBoundDist - gridIndex.w);

  if (boundDist < 0) {
    return 0.0;
  }
  const double d = sdf(Position(gridIndex, origin, spacing)) - level;
  return boundDist == 0 ? std::min(d, 0.0) : d;
}

// Simplified ITP root finding algorithm - same worst-case performance as
// bisection, better average performance.
inline vec3 FindSurface(vec3 pos0, double d0, vec3 pos1, double d1, double tol,
                        double level, std::function<double(vec3)> sdf) {
  if (d0 == 0) {
    return pos0;
  } else if (d1 == 0) {
    return pos1;
  }

  // Sole tuning parameter, k: (0, 1) - smaller value gets better median
  // performance, but also hits the worst case more often.
  const double k = 0.1;
  const double check = 2 * tol / la::length(pos0 - pos1);
  double frac = 1;
  double biFrac = 1;
  while (frac > check) {
    const double t = la::lerp(d0 / (d0 - d1), 0.5, k);
    const double r = biFrac / frac - 0.5;
    const double x = la::abs(t - 0.5) < r ? t : 0.5 - r * (t < 0.5 ? 1 : -1);

    const vec3 mid = la::lerp(pos0, pos1, x);
    const double d = sdf(mid) - level;

    if ((d > 0) == (d0 > 0)) {
      d0 = d;
      pos0 = mid;
      frac *= 1 - x;
    } else {
      d1 = d;
      pos1 = mid;
      frac *= x;
    }
    biFrac /= 2;
  }

  return la::lerp(pos0, pos1, d0 / (d0 - d1));
}

/**
 * Each GridVert is connected to 14 others, and in charge of 7 of these edges
 * (see Neighbor() above). Each edge that changes sign contributes one vert,
 * unless the GridVert is close enough to the surface, in which case it
 * contributes only a single movedVert and all crossing edgeVerts refer to that.
 */
struct GridVert {
  double distance = NAN;
  int movedVert = kNone;
  int edgeVerts[7] = {kNone, kNone, kNone, kNone, kNone, kNone, kNone};

  inline bool HasMoved() const { return movedVert >= 0; }

  inline bool SameSide(double dist) const {
    return (dist > 0) == (distance > 0);
  }

  inline int Inside() const { return distance > 0 ? 1 : -1; }

  inline int NeighborInside(int i) const {
    return Inside() * (edgeVerts[i] == kNone ? 1 : -1);
  }
};

struct NearSurface {
  VecView<vec3> vertPos;
  VecView<int> vertIndex;
  HashTableD<GridVert> gridVerts;
  VecView<const double> voxels;
  const std::function<double(vec3)> sdf;
  const vec3 origin;
  const ivec3 gridSize;
  const ivec3 gridPow;
  const vec3 spacing;
  const double level;
  const double tol;

  inline void operator()(Uint64 index) {
    ZoneScoped;
    if (gridVerts.Full()) return;

    const ivec4 gridIndex = DecodeIndex(index, gridPow);

    if (la::any(la::greater(ivec3(gridIndex), gridSize))) return;

    GridVert gridVert;
    gridVert.distance = voxels[EncodeIndex(gridIndex + kVoxelOffset, gridPow)];

    bool keep = false;
    double vMax = 0;
    int closestNeighbor = -1;
    int opposedVerts = 0;
    for (int i = 0; i < 7; ++i) {
      const double val =
          voxels[EncodeIndex(Neighbor(gridIndex, i) + kVoxelOffset, gridPow)];
      const double valOp = voxels[EncodeIndex(
          Neighbor(gridIndex, i + 7) + kVoxelOffset, gridPow)];

      if (!gridVert.SameSide(val)) {
        gridVert.edgeVerts[i] = kCrossing;
        keep = true;
        if (!gridVert.SameSide(valOp)) {
          ++opposedVerts;
        }
        // Approximate bound on vert movement.
        if (la::abs(val) > kD * la::abs(gridVert.distance) &&
            la::abs(val) > la::abs(vMax)) {
          vMax = val;
          closestNeighbor = i;
        }
      } else if (!gridVert.SameSide(valOp) &&
                 la::abs(valOp) > kD * la::abs(gridVert.distance) &&
                 la::abs(valOp) > la::abs(vMax)) {
        vMax = valOp;
        closestNeighbor = i + 7;
      }
    }

    // This is where we collapse all the crossing edge verts into this GridVert,
    // speeding up the algorithm and avoiding poor quality triangles. Without
    // this step the result is guaranteed 2-manifold, but with this step it can
    // become an even-manifold with kissing verts. These must be removed in a
    // post-process: CleanupTopology().
    if (closestNeighbor >= 0 && opposedVerts <= kMaxOpposed) {
      const vec3 gridPos = Position(gridIndex, origin, spacing);
      const ivec4 neighborIndex = Neighbor(gridIndex, closestNeighbor);
      const vec3 pos = FindSurface(gridPos, gridVert.distance,
                                   Position(neighborIndex, origin, spacing),
                                   vMax, tol, level, sdf);
      // Bound the delta of each vert to ensure the tetrahedron cannot invert.
      if (la::all(la::less(la::abs(pos - gridPos), kS * spacing))) {
        const int idx = AtomicAdd(vertIndex[0], 1);
        vertPos[idx] = Bound(pos, origin, spacing, gridSize);
        gridVert.movedVert = idx;
        for (int j = 0; j < 7; ++j) {
          if (gridVert.edgeVerts[j] == kCrossing) gridVert.edgeVerts[j] = idx;
        }
        keep = true;
      }
    } else {
      for (int j = 0; j < 7; ++j) gridVert.edgeVerts[j] = kNone;
    }

    if (keep) gridVerts.Insert(index, gridVert);
  }
};

struct ComputeVerts {
  VecView<vec3> vertPos;
  VecView<int> vertIndex;
  HashTableD<GridVert> gridVerts;
  VecView<const double> voxels;
  const std::function<double(vec3)> sdf;
  const vec3 origin;
  const ivec3 gridSize;
  const ivec3 gridPow;
  const vec3 spacing;
  const double level;
  const double tol;

  void operator()(int idx) {
    ZoneScoped;
    Uint64 baseKey = gridVerts.KeyAt(idx);
    if (baseKey == kOpen) return;

    GridVert& gridVert = gridVerts.At(idx);

    if (gridVert.HasMoved()) return;

    const ivec4 gridIndex = DecodeIndex(baseKey, gridPow);

    const vec3 position = Position(gridIndex, origin, spacing);

    // These seven edges are uniquely owned by this gridVert; any of them
    // which intersect the surface create a vert.
    for (int i = 0; i < 7; ++i) {
      const ivec4 neighborIndex = Neighbor(gridIndex, i);
      const GridVert& neighbor = gridVerts[EncodeIndex(neighborIndex, gridPow)];

      const double val =
          std::isfinite(neighbor.distance)
              ? neighbor.distance
              : voxels[EncodeIndex(neighborIndex + kVoxelOffset, gridPow)];
      if (gridVert.SameSide(val)) continue;

      if (neighbor.HasMoved()) {
        gridVert.edgeVerts[i] = neighbor.movedVert;
        continue;
      }

      const int idx = AtomicAdd(vertIndex[0], 1);
      const vec3 pos = FindSurface(position, gridVert.distance,
                                   Position(neighborIndex, origin, spacing),
                                   val, tol, level, sdf);
      vertPos[idx] = Bound(pos, origin, spacing, gridSize);
      gridVert.edgeVerts[i] = idx;
    }
  }
};

struct BuildTris {
  VecView<ivec3> triVerts;
  VecView<int> triIndex;
  const HashTableD<GridVert> gridVerts;
  const ivec3 gridPow;

  void CreateTri(const ivec3& tri, const int edges[6]) {
    if (tri[0] < 0) return;
    const ivec3 verts(edges[tri[0]], edges[tri[1]], edges[tri[2]]);
    if (verts[0] == verts[1] || verts[1] == verts[2] || verts[2] == verts[0])
      return;
    int idx = AtomicAdd(triIndex[0], 1);
    triVerts[idx] = verts;
  }

  void CreateTris(const ivec4& tet, const int edges[6]) {
    const int i = (tet[0] > 0 ? 1 : 0) + (tet[1] > 0 ? 2 : 0) +
                  (tet[2] > 0 ? 4 : 0) + (tet[3] > 0 ? 8 : 0);
    CreateTri(TetTri0(i), edges);
    CreateTri(TetTri1(i), edges);
  }

  void operator()(int idx) {
    ZoneScoped;
    Uint64 baseKey = gridVerts.KeyAt(idx);
    if (baseKey == kOpen) return;

    const GridVert& base = gridVerts.At(idx);
    const ivec4 baseIndex = DecodeIndex(baseKey, gridPow);

    ivec4 leadIndex = baseIndex;
    if (leadIndex.w == 0)
      leadIndex.w = 1;
    else {
      leadIndex += 1;
      leadIndex.w = 0;
    }

    // This GridVert is in charge of the 6 tetrahedra surrounding its edge in
    // the (1,1,1) direction (edge 0).
    ivec4 tet(base.NeighborInside(0), base.Inside(), -2, -2);
    ivec4 thisIndex = baseIndex;
    thisIndex.x += 1;

    GridVert thisVert = gridVerts[EncodeIndex(thisIndex, gridPow)];

    tet[2] = base.NeighborInside(1);
    for (const int i : {0, 1, 2}) {
      thisIndex = leadIndex;
      --thisIndex[Prev3(i)];
      // Indices take unsigned input, so check for negatives, given the
      // decrement. If negative, the vert is outside and only connected to other
      // outside verts - no edgeVerts.
      GridVert nextVert = thisIndex[Prev3(i)] < 0
                              ? GridVert()
                              : gridVerts[EncodeIndex(thisIndex, gridPow)];
      tet[3] = base.NeighborInside(Prev3(i) + 4);

      const int edges1[6] = {base.edgeVerts[0],
                             base.edgeVerts[i + 1],
                             nextVert.edgeVerts[Next3(i) + 4],
                             nextVert.edgeVerts[Prev3(i) + 1],
                             thisVert.edgeVerts[i + 4],
                             base.edgeVerts[Prev3(i) + 4]};
      thisVert = nextVert;
      CreateTris(tet, edges1);

      thisIndex = baseIndex;
      ++thisIndex[Next3(i)];
      nextVert = gridVerts[EncodeIndex(thisIndex, gridPow)];
      tet[2] = tet[3];
      tet[3] = base.NeighborInside(Next3(i) + 1);

      const int edges2[6] = {base.edgeVerts[0],
                             edges1[5],
                             thisVert.edgeVerts[i + 4],
                             nextVert.edgeVerts[Next3(i) + 4],
                             edges1[3],
                             base.edgeVerts[Next3(i) + 1]};
      thisVert = nextVert;
      CreateTris(tet, edges2);

      tet[2] = tet[3];
    }
  }
};
}  // namespace

namespace manifold {

/**
 * Constructs a level-set manifold from the input Signed-Distance Function
 * (SDF). This uses a form of Marching Tetrahedra (akin to Marching
 * Cubes, but better for manifoldness). Instead of using a cubic grid, it uses a
 * body-centered cubic grid (two shifted cubic grids). These grid points are
 * snapped to the surface where possible to keep short edges from forming.
 *
 * @param sdf The signed-distance functor, containing this function signature:
 * `double operator()(vec3 point)`, which returns the
 * signed distance of a given point in R^3. Positive values are inside,
 * negative outside. There is no requirement that the function be a true
 * distance, or even continuous.
 * @param bounds An axis-aligned box that defines the extent of the grid.
 * @param edgeLength Approximate maximum edge length of the triangles in the
 * final result. This affects grid spacing, and hence has a strong effect on
 * performance.
 * @param level Extract the surface at this value of your sdf; defaults to
 * zero. You can inset your mesh by using a positive value, or outset it with a
 * negative value.
 * @param tolerance Ensure each vertex is within this distance of the true
 * surface. Defaults to -1, which will return the interpolated
 * crossing-point based on the two nearest grid points. Small positive values
 * will require more sdf evaluations per output vertex.
 * @param canParallel Parallel policies violate will crash language runtimes
 * with runtime locks that expect to not be called back by unregistered threads.
 * This allows bindings use LevelSet despite being compiled with MANIFOLD_PAR
 * active.
 */
Manifold Manifold::LevelSet(std::function<double(vec3)> sdf, Box bounds,
                            double edgeLength, double level, double tolerance,
                            bool canParallel) {
  if (tolerance <= 0) {
    tolerance = std::numeric_limits<double>::infinity();
  }

  auto pImpl_ = std::make_shared<Impl>();
  auto& vertPos = pImpl_->vertPos_;

  const vec3 dim = bounds.Size();
  const ivec3 gridSize(dim / edgeLength + 1.0);
  const vec3 spacing = dim / (vec3(gridSize - 1));

  const ivec3 gridPow(la::log2(gridSize + 2) + 1);
  const Uint64 maxIndex = EncodeIndex(ivec4(gridSize + 2, 1), gridPow);

  // Parallel policies violate will crash language runtimes with runtime locks
  // that expect to not be called back by unregistered threads. This allows
  // bindings use LevelSet despite being compiled with MANIFOLD_PAR
  // active.
  const auto pol = canParallel ? autoPolicy(maxIndex) : ExecutionPolicy::Seq;

  const vec3 origin = bounds.min;
  Vec<double> voxels(maxIndex);
  for_each_n(
      pol, countAt(0_uz), maxIndex,
      [&voxels, sdf, level, origin, spacing, gridSize, gridPow](Uint64 idx) {
        voxels[idx] = BoundedSDF(DecodeIndex(idx, gridPow) - kVoxelOffset,
                                 origin, spacing, gridSize, level, sdf);
      });

  size_t tableSize = std::min(
      2 * maxIndex, static_cast<Uint64>(10 * la::pow(maxIndex, 0.667)));
  HashTable<GridVert> gridVerts(tableSize);
  vertPos.resize(gridVerts.Size() * 7);

  while (1) {
    Vec<int> index(1, 0);
    for_each_n(pol, countAt(0_uz), EncodeIndex(ivec4(gridSize, 1), gridPow),
               NearSurface({vertPos, index, gridVerts.D(), voxels, sdf, origin,
                            gridSize, gridPow, spacing, level, tolerance}));

    if (gridVerts.Full()) {  // Resize HashTable
      const vec3 lastVert = vertPos[index[0] - 1];
      const Uint64 lastIndex =
          EncodeIndex(ivec4(ivec3((lastVert - origin) / spacing), 1), gridPow);
      const double ratio = static_cast<double>(maxIndex) / lastIndex;

      if (ratio > 1000)  // do not trust the ratio if it is too large
        tableSize *= 2;
      else
        tableSize *= ratio;
      gridVerts = HashTable<GridVert>(tableSize);
      vertPos = Vec<vec3>(gridVerts.Size() * 7);
    } else {  // Success
      for_each_n(
          pol, countAt(0), gridVerts.Size(),
          ComputeVerts({vertPos, index, gridVerts.D(), voxels, sdf, origin,
                        gridSize, gridPow, spacing, level, tolerance}));
      vertPos.resize(index[0]);
      break;
    }
  }

  Vec<ivec3> triVerts(gridVerts.Entries() * 12);  // worst case

  Vec<int> index(1, 0);
  for_each_n(pol, countAt(0), gridVerts.Size(),
             BuildTris({triVerts, index, gridVerts.D(), gridPow}));
  triVerts.resize(index[0]);

  pImpl_->CreateHalfedges(triVerts);
  pImpl_->CleanupTopology();
  pImpl_->Finish();
  pImpl_->InitializeOriginal();
  return Manifold(pImpl_);
}
}  // namespace manifold
