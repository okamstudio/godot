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

#include "manifold/polygon.h"

#include <functional>
#include <map>
#include <set>

#include "./collider.h"
#include "./parallel.h"
#include "./utils.h"
#include "manifold/optional_assert.h"

namespace {
using namespace manifold;

static ExecutionParams params;

constexpr double kBest = -std::numeric_limits<double>::infinity();

// it seems that MSVC cannot optimize la::determinant(mat2(a, b))
constexpr double determinant2x2(vec2 a, vec2 b) {
  return a.x * b.y - a.y * b.x;
}

#ifdef MANIFOLD_DEBUG
struct PolyEdge {
  int startVert, endVert;
};

std::vector<PolyEdge> Polygons2Edges(const PolygonsIdx &polys) {
  std::vector<PolyEdge> halfedges;
  for (const auto &poly : polys) {
    for (size_t i = 1; i < poly.size(); ++i) {
      halfedges.push_back({poly[i - 1].idx, poly[i].idx});
    }
    halfedges.push_back({poly.back().idx, poly[0].idx});
  }
  return halfedges;
}

std::vector<PolyEdge> Triangles2Edges(const std::vector<ivec3> &triangles) {
  std::vector<PolyEdge> halfedges;
  halfedges.reserve(triangles.size() * 3);
  for (const ivec3 &tri : triangles) {
    halfedges.push_back({tri[0], tri[1]});
    halfedges.push_back({tri[1], tri[2]});
    halfedges.push_back({tri[2], tri[0]});
  }
  return halfedges;
}

void CheckTopology(const std::vector<PolyEdge> &halfedges) {
  DEBUG_ASSERT(halfedges.size() % 2 == 0, topologyErr,
               "Odd number of halfedges.");
  size_t n_edges = halfedges.size() / 2;
  std::vector<PolyEdge> forward(halfedges.size()), backward(halfedges.size());

  auto end = std::copy_if(halfedges.begin(), halfedges.end(), forward.begin(),
                          [](PolyEdge e) { return e.endVert > e.startVert; });
  DEBUG_ASSERT(
      static_cast<size_t>(std::distance(forward.begin(), end)) == n_edges,
      topologyErr, "Half of halfedges should be forward.");
  forward.resize(n_edges);

  end = std::copy_if(halfedges.begin(), halfedges.end(), backward.begin(),
                     [](PolyEdge e) { return e.endVert < e.startVert; });
  DEBUG_ASSERT(
      static_cast<size_t>(std::distance(backward.begin(), end)) == n_edges,
      topologyErr, "Half of halfedges should be backward.");
  backward.resize(n_edges);

  std::for_each(backward.begin(), backward.end(),
                [](PolyEdge &e) { std::swap(e.startVert, e.endVert); });
  auto cmp = [](const PolyEdge &a, const PolyEdge &b) {
    return a.startVert < b.startVert ||
           (a.startVert == b.startVert && a.endVert < b.endVert);
  };
  std::stable_sort(forward.begin(), forward.end(), cmp);
  std::stable_sort(backward.begin(), backward.end(), cmp);
  for (size_t i = 0; i < n_edges; ++i) {
    DEBUG_ASSERT(forward[i].startVert == backward[i].startVert &&
                     forward[i].endVert == backward[i].endVert,
                 topologyErr, "Not manifold.");
  }
}

void CheckTopology(const std::vector<ivec3> &triangles,
                   const PolygonsIdx &polys) {
  std::vector<PolyEdge> halfedges = Triangles2Edges(triangles);
  std::vector<PolyEdge> openEdges = Polygons2Edges(polys);
  for (PolyEdge e : openEdges) {
    halfedges.push_back({e.endVert, e.startVert});
  }
  CheckTopology(halfedges);
}

void CheckGeometry(const std::vector<ivec3> &triangles,
                   const PolygonsIdx &polys, double epsilon) {
  std::unordered_map<int, vec2> vertPos;
  for (const auto &poly : polys) {
    for (size_t i = 0; i < poly.size(); ++i) {
      vertPos[poly[i].idx] = poly[i].pos;
    }
  }
  DEBUG_ASSERT(std::all_of(triangles.begin(), triangles.end(),
                           [&vertPos, epsilon](const ivec3 &tri) {
                             return CCW(vertPos[tri[0]], vertPos[tri[1]],
                                        vertPos[tri[2]], epsilon) >= 0;
                           }),
               geometryErr, "triangulation is not entirely CCW!");
}

void Dump(const PolygonsIdx &polys, double epsilon) {
  std::cout << "Polygon 0 " << epsilon << " " << polys.size() << std::endl;
  for (auto poly : polys) {
    std::cout << poly.size() << std::endl;
    for (auto v : poly) {
      std::cout << v.pos.x << " " << v.pos.y << std::endl;
    }
  }
  std::cout << "# ... " << std::endl;
  for (auto poly : polys) {
    std::cout << "show(array([" << std::endl;
    for (auto v : poly) {
      std::cout << "  [" << v.pos.x << ", " << v.pos.y << "]," << std::endl;
    }
    std::cout << "]))" << std::endl;
  }
}

void PrintFailure(const std::exception &e, const PolygonsIdx &polys,
                  std::vector<ivec3> &triangles, double epsilon) {
  std::cout << "-----------------------------------" << std::endl;
  std::cout << "Triangulation failed! Precision = " << epsilon << std::endl;
  std::cout << e.what() << std::endl;
  if (triangles.size() > 1000 && !PolygonParams().verbose) {
    std::cout << "Output truncated due to producing " << triangles.size()
              << " triangles." << std::endl;
    return;
  }
  Dump(polys, epsilon);
  std::cout << "produced this triangulation:" << std::endl;
  for (size_t j = 0; j < triangles.size(); ++j) {
    std::cout << triangles[j][0] << ", " << triangles[j][1] << ", "
              << triangles[j][2] << std::endl;
  }
}

#define PRINT(msg) \
  if (params.verbose) std::cout << msg << std::endl;
#else
#define PRINT(msg)
#endif

/**
 * Tests if the input polygons are convex by searching for any reflex vertices.
 * Exactly colinear edges and zero-length edges are treated conservatively as
 * reflex. Does not check for overlaps.
 */
bool IsConvex(const PolygonsIdx &polys, double epsilon) {
  for (const SimplePolygonIdx &poly : polys) {
    const vec2 firstEdge = poly[0].pos - poly[poly.size() - 1].pos;
    // Zero-length edges comes out NaN, which won't trip the early return, but
    // it's okay because that zero-length edge will also get tested
    // non-normalized and will trip det == 0.
    vec2 lastEdge = la::normalize(firstEdge);
    for (size_t v = 0; v < poly.size(); ++v) {
      const vec2 edge =
          v + 1 < poly.size() ? poly[v + 1].pos - poly[v].pos : firstEdge;
      const double det = determinant2x2(lastEdge, edge);
      if (det <= 0 || (std::abs(det) < epsilon && la::dot(lastEdge, edge) < 0))
        return false;
      lastEdge = la::normalize(edge);
    }
  }
  return true;
}

/**
 * Triangulates a set of convex polygons by alternating instead of a fan, to
 * avoid creating high-degree vertices.
 */
std::vector<ivec3> TriangulateConvex(const PolygonsIdx &polys) {
  const size_t numTri = manifold::transform_reduce(
      polys.begin(), polys.end(), 0_uz,
      [](size_t a, size_t b) { return a + b; },
      [](const SimplePolygonIdx &poly) { return poly.size() - 2; });
  std::vector<ivec3> triangles;
  triangles.reserve(numTri);
  for (const SimplePolygonIdx &poly : polys) {
    size_t i = 0;
    size_t k = poly.size() - 1;
    bool right = true;
    while (i + 1 < k) {
      const size_t j = right ? i + 1 : k - 1;
      triangles.push_back({poly[i].idx, poly[j].idx, poly[k].idx});
      if (right) {
        i = j;
      } else {
        k = j;
      }
      right = !right;
    }
  }
  return triangles;
}

/**
 * Ear-clipping triangulator based on David Eberly's approach from Geometric
 * Tools, but adjusted to handle epsilon-valid polygons, and including a
 * fallback that ensures a manifold triangulation even for overlapping polygons.
 * This is an O(n^2) algorithm, but hopefully this is not a big problem as the
 * number of edges in a given polygon is generally much less than the number of
 * triangles in a mesh, and relatively few faces even need triangulation.
 *
 * The main adjustments for robustness involve clipping the sharpest ears first
 * (a known technique to get higher triangle quality), and doing an exhaustive
 * search to determine ear convexity exactly if the first geometric result is
 * within epsilon.
 */

class EarClip {
 public:
  EarClip(const PolygonsIdx &polys, double epsilon) : epsilon_(epsilon) {
    ZoneScoped;

    size_t numVert = 0;
    for (const SimplePolygonIdx &poly : polys) {
      numVert += poly.size();
    }
    polygon_.reserve(numVert + 2 * polys.size());

    std::vector<VertItr> starts = Initialize(polys);

    for (VertItr v = polygon_.begin(); v != polygon_.end(); ++v) {
      ClipIfDegenerate(v);
    }

    for (const VertItr first : starts) {
      FindStart(first);
    }
  }

  std::vector<ivec3> Triangulate() {
    ZoneScoped;

    for (const VertItr start : holes_) {
      CutKeyhole(start);
    }

    for (const VertItr start : simples_) {
      TriangulatePoly(start);
    }

    return triangles_;
  }

  double GetPrecision() const { return epsilon_; }

 private:
  struct Vert;
  typedef std::vector<Vert>::iterator VertItr;
  typedef std::vector<Vert>::const_iterator VertItrC;
  struct MaxX {
    bool operator()(const VertItr &a, const VertItr &b) const {
      return a->pos.x > b->pos.x;
    }
  };
  struct MinCost {
    bool operator()(const VertItr &a, const VertItr &b) const {
      return a->cost < b->cost;
    }
  };
  typedef std::set<VertItr, MinCost>::iterator qItr;

  // The flat list where all the Verts are stored. Not used much for traversal.
  std::vector<Vert> polygon_;
  // The set of right-most starting points, one for each negative-area contour.
  std::multiset<VertItr, MaxX> holes_;
  // The set of starting points, one for each positive-area contour.
  std::vector<VertItr> outers_;
  // The set of starting points, one for each simple polygon.
  std::vector<VertItr> simples_;
  // Maps each hole (by way of starting point) to its bounding box.
  std::map<VertItr, Rect> hole2BBox_;
  // A priority queue of valid ears - the multiset allows them to be updated.
  std::multiset<VertItr, MinCost> earsQueue_;
  // The output triangulation.
  std::vector<ivec3> triangles_;
  // Bounding box of the entire set of polygons
  Rect bBox_;
  // Working epsilon: max of float error and input value.
  double epsilon_;

  struct IdxCollider {
    Collider collider;
    std::vector<VertItr> itr;
    SparseIndices ind;
  };

  // A circularly-linked list representing the polygon(s) that still need to be
  // triangulated. This gets smaller as ears are clipped until it degenerates to
  // two points and terminates.
  struct Vert {
    int mesh_idx;
    double cost;
    qItr ear;
    vec2 pos, rightDir;
    VertItr left, right;

    // Shorter than half of epsilon, to be conservative so that it doesn't
    // cause CW triangles that exceed epsilon due to rounding error.
    bool IsShort(double epsilon) const {
      const vec2 edge = right->pos - pos;
      return la::dot(edge, edge) * 4 < epsilon * epsilon;
    }

    // Like CCW, returns 1 if v is on the inside of the angle formed at this
    // vert, -1 on the outside, and 0 if it's within epsilon of the boundary.
    // Ensure v is more than epsilon from pos, as case this will not return 0.
    int Interior(vec2 v, double epsilon) const {
      const vec2 diff = v - pos;
      if (la::dot(diff, diff) < epsilon * epsilon) {
        return 0;
      }
      return CCW(pos, left->pos, right->pos, epsilon) +
             CCW(pos, right->pos, v, epsilon) + CCW(pos, v, left->pos, epsilon);
    }

    // Returns true if Vert is on the inside of the edge that goes from tail to
    // tail->right. This will walk the edges if necessary until a clear answer
    // is found (beyond epsilon). If toLeft is true, this Vert will walk its
    // edges to the left. This should be chosen so that the edges walk in the
    // same general direction - tail always walks to the right.
    bool InsideEdge(VertItr tail, double epsilon, bool toLeft) const {
      const double p2 = epsilon * epsilon;
      VertItr nextL = left->right;
      VertItr nextR = tail->right;
      VertItr center = tail;
      VertItr last = center;

      while (nextL != nextR && tail != nextR &&
             nextL != (toLeft ? right : left)) {
        const vec2 edgeL = nextL->pos - center->pos;
        const double l2 = la::dot(edgeL, edgeL);
        if (l2 <= p2) {
          nextL = toLeft ? nextL->left : nextL->right;
          continue;
        }

        const vec2 edgeR = nextR->pos - center->pos;
        const double r2 = la::dot(edgeR, edgeR);
        if (r2 <= p2) {
          nextR = nextR->right;
          continue;
        }

        const vec2 vecLR = nextR->pos - nextL->pos;
        const double lr2 = la::dot(vecLR, vecLR);
        if (lr2 <= p2) {
          last = center;
          center = nextL;
          nextL = toLeft ? nextL->left : nextL->right;
          if (nextL == nextR) break;
          nextR = nextR->right;
          continue;
        }

        int convexity = CCW(nextL->pos, center->pos, nextR->pos, epsilon);
        if (center != last) {
          convexity += CCW(last->pos, center->pos, nextL->pos, epsilon) +
                       CCW(nextR->pos, center->pos, last->pos, epsilon);
        }
        if (convexity != 0) return convexity > 0;

        if (l2 < r2) {
          center = nextL;
          nextL = toLeft ? nextL->left : nextL->right;
        } else {
          center = nextR;
          nextR = nextR->right;
        }
        last = center;
      }
      // The whole polygon is degenerate - consider this to be convex.
      return true;
    }

    // A major key to robustness is to only clip convex ears, but this is
    // difficult to determine when an edge is folded back on itself. This
    // function walks down the kinks in a degenerate portion of a polygon until
    // it finds a clear geometric result. In the vast majority of cases the loop
    // will only need one or two iterations.
    bool IsConvex(double epsilon) const {
      const int convexity = CCW(left->pos, pos, right->pos, epsilon);
      if (convexity != 0) {
        return convexity > 0;
      }
      if (la::dot(left->pos - pos, right->pos - pos) <= 0) {
        return true;
      }
      return left->InsideEdge(left->right, epsilon, true);
    }

    // Subtly different from !IsConvex because IsConvex will return true for
    // colinear non-folded verts, while IsReflex will always check until actual
    // certainty is determined.
    bool IsReflex(double epsilon) const {
      return !left->InsideEdge(left->right, epsilon, true);
    }

    // Returns the x-value on this edge corresponding to the start.y value,
    // returning NAN if the edge does not cross the value from below to above,
    // right of start - all within a epsilon tolerance. If onTop != 0, this
    // restricts which end is allowed to terminate within the epsilon band.
    double InterpY2X(vec2 start, int onTop, double epsilon) const {
      if (la::abs(pos.y - start.y) <= epsilon) {
        if (right->pos.y <= start.y + epsilon || onTop == 1) {
          return NAN;
        } else {
          return pos.x;
        }
      } else if (pos.y < start.y - epsilon) {
        if (right->pos.y > start.y + epsilon) {
          return pos.x + (start.y - pos.y) * (right->pos.x - pos.x) /
                             (right->pos.y - pos.y);
        } else if (right->pos.y < start.y - epsilon || onTop == -1) {
          return NAN;
        } else {
          return right->pos.x;
        }
      } else {
        return NAN;
      }
    }

    // This finds the cost of this vert relative to one of the two closed sides
    // of the ear. Points are valid even when they touch, so long as their edge
    // goes to the outside. No need to check the other side, since all verts are
    // processed in the EarCost loop.
    double SignedDist(VertItr v, vec2 unit, double epsilon) const {
      double d = determinant2x2(unit, v->pos - pos);
      if (std::abs(d) < epsilon) {
        double dR = determinant2x2(unit, v->right->pos - pos);
        if (std::abs(dR) > epsilon) return dR;
        double dL = determinant2x2(unit, v->left->pos - pos);
        if (std::abs(dL) > epsilon) return dL;
      }
      return d;
    }

    // Find the cost of Vert v within this ear, where openSide is the unit
    // vector from Verts right to left - passed in for reuse.
    double Cost(VertItr v, vec2 openSide, double epsilon) const {
      double cost = std::min(SignedDist(v, rightDir, epsilon),
                             SignedDist(v, left->rightDir, epsilon));

      const double openCost = determinant2x2(openSide, v->pos - right->pos);
      return std::min(cost, openCost);
    }

    // For verts outside the ear, apply a cost based on the Delaunay condition
    // to aid in prioritization and produce cleaner triangulations. This doesn't
    // affect robustness, but may be adjusted to improve output.
    static double DelaunayCost(vec2 diff, double scale, double epsilon) {
      return -epsilon - scale * la::dot(diff, diff);
    }

    // This is the expensive part of the algorithm, checking this ear against
    // every Vert to ensure none are inside. The Collider brings the total
    // triangulator cost down from O(n^2) to O(nlogn) for most large polygons.
    //
    // Think of a cost as vaguely a distance metric - 0 is right on the edge of
    // being invalid. cost > epsilon is definitely invalid. Cost < -epsilon
    // is definitely valid, so all improvement costs are designed to always give
    // values < -epsilon so they will never affect validity. The first
    // totalCost is designed to give priority to sharper angles. Any cost < (-1
    // - epsilon) has satisfied the Delaunay condition.
    double EarCost(double epsilon, IdxCollider &collider) const {
      vec2 openSide = left->pos - right->pos;
      const vec2 center = 0.5 * (left->pos + right->pos);
      const double scale = 4 / la::dot(openSide, openSide);
      const double radius = la::length(openSide) / 2;
      openSide = la::normalize(openSide);

      double totalCost = la::dot(left->rightDir, rightDir) - 1 - epsilon;
      if (CCW(pos, left->pos, right->pos, epsilon) == 0) {
        // Clip folded ears first
        return totalCost;
      }

      Box earBox = Box{vec3(center.x - radius, center.y - radius, 0),
                       vec3(center.x + radius, center.y + radius, 0)};
      earBox.Union(vec3(pos, 0));
      collider.collider.Collisions(VecView<const Box>(&earBox, 1),
                                   collider.ind);

      const int lid = left->mesh_idx;
      const int rid = right->mesh_idx;

      totalCost = transform_reduce(
          countAt(0), countAt(collider.ind.size()), totalCost,
          [](double a, double b) { return std::max(a, b); },
          [&](size_t i) {
            const VertItr test = collider.itr[collider.ind.Get(i, true)];
            if (!Clipped(test) && test->mesh_idx != mesh_idx &&
                test->mesh_idx != lid &&
                test->mesh_idx != rid) {  // Skip duplicated verts
              double cost = Cost(test, openSide, epsilon);
              if (cost < -epsilon) {
                cost = DelaunayCost(test->pos - center, scale, epsilon);
              }
              return cost;
            }
            return std::numeric_limits<double>::lowest();
          });
      collider.ind.Clear();
      return totalCost;
    }

    void PrintVert() const {
#ifdef MANIFOLD_DEBUG
      if (!params.verbose) return;
      std::cout << "vert: " << mesh_idx << ", left: " << left->mesh_idx
                << ", right: " << right->mesh_idx << ", cost: " << cost
                << std::endl;
#endif
    }
  };

  static vec2 SafeNormalize(vec2 v) {
    vec2 n = la::normalize(v);
    return std::isfinite(n.x) ? n : vec2(0, 0);
  }

  // This function and JoinPolygons are the only functions that affect the
  // circular list data structure. This helps ensure it remains circular.
  static void Link(VertItr left, VertItr right) {
    left->right = right;
    right->left = left;
    left->rightDir = SafeNormalize(right->pos - left->pos);
  }

  // When an ear vert is clipped, its neighbors get linked, so they get unlinked
  // from it, but it is still linked to them.
  static bool Clipped(VertItr v) { return v->right->left != v; }

  // Apply func to each un-clipped vert in a polygon and return an un-clipped
  // vert.
  VertItrC Loop(VertItr first, std::function<void(VertItr)> func) const {
    VertItr v = first;
    do {
      if (Clipped(v)) {
        // Update first to an un-clipped vert so we will return to it instead
        // of infinite-looping.
        first = v->right->left;
        if (!Clipped(first)) {
          v = first;
          if (v->right == v->left) {
            return polygon_.end();
          }
          func(v);
        }
      } else {
        if (v->right == v->left) {
          return polygon_.end();
        }
        func(v);
      }
      v = v->right;
    } while (v != first);
    return v;
  }

  // Remove this vert from the circular list and output a corresponding
  // triangle.
  void ClipEar(VertItrC ear) {
    Link(ear->left, ear->right);
    if (ear->left->mesh_idx != ear->mesh_idx &&
        ear->mesh_idx != ear->right->mesh_idx &&
        ear->right->mesh_idx != ear->left->mesh_idx) {
      // Filter out topological degenerates, which can form in bad
      // triangulations of polygons with holes, due to vert duplication.
      triangles_.push_back(
          {ear->left->mesh_idx, ear->mesh_idx, ear->right->mesh_idx});
    } else {
      PRINT("Topological degenerate!");
    }
  }

  // If an ear will make a degenerate triangle, clip it early to avoid
  // difficulty in key-holing. This function is recursive, as the process of
  // clipping may cause the neighbors to degenerate. Reflex degenerates *must
  // not* be clipped, unless they have a short edge.
  void ClipIfDegenerate(VertItr ear) {
    if (Clipped(ear)) {
      return;
    }
    if (ear->left == ear->right) {
      return;
    }
    if (ear->IsShort(epsilon_) ||
        (CCW(ear->left->pos, ear->pos, ear->right->pos, epsilon_) == 0 &&
         la::dot(ear->left->pos - ear->pos, ear->right->pos - ear->pos) > 0 &&
         ear->IsConvex(epsilon_))) {
      ClipEar(ear);
      ClipIfDegenerate(ear->left);
      ClipIfDegenerate(ear->right);
    }
  }

  // Build the circular list polygon structures.
  std::vector<VertItr> Initialize(const PolygonsIdx &polys) {
    std::vector<VertItr> starts;
    for (const SimplePolygonIdx &poly : polys) {
      auto vert = poly.begin();
      polygon_.push_back({vert->idx, 0.0, earsQueue_.end(), vert->pos});
      const VertItr first = std::prev(polygon_.end());

      bBox_.Union(first->pos);
      VertItr last = first;
      // This is not the real rightmost start, but just an arbitrary vert for
      // now to identify each polygon.
      starts.push_back(first);

      for (++vert; vert != poly.end(); ++vert) {
        bBox_.Union(vert->pos);

        polygon_.push_back({vert->idx, 0.0, earsQueue_.end(), vert->pos});
        VertItr next = std::prev(polygon_.end());

        Link(last, next);
        last = next;
      }
      Link(last, first);
    }

    if (epsilon_ < 0) epsilon_ = bBox_.Scale() * kPrecision;

    // Slightly more than enough, since each hole can cause two extra triangles.
    triangles_.reserve(polygon_.size() + 2 * starts.size());
    return starts;
  }

  // Find the actual rightmost starts after degenerate removal. Also calculate
  // the polygon bounding boxes.
  void FindStart(VertItr first) {
    const vec2 origin = first->pos;

    VertItr start = first;
    double maxX = -std::numeric_limits<double>::infinity();
    Rect bBox;
    // Kahan summation
    double area = 0;
    double areaCompensation = 0;

    auto AddPoint = [&](VertItr v) {
      bBox.Union(v->pos);
      const double area1 =
          determinant2x2(v->pos - origin, v->right->pos - origin);
      const double t1 = area + area1;
      areaCompensation += (area - t1) + area1;
      area = t1;

      if (v->pos.x > maxX) {
        maxX = v->pos.x;
        start = v;
      }
    };

    if (Loop(first, AddPoint) == polygon_.end()) {
      // No polygon left if all ears were degenerate and already clipped.
      return;
    }

    area += areaCompensation;
    const vec2 size = bBox.Size();
    const double minArea = epsilon_ * std::max(size.x, size.y);

    if (std::isfinite(maxX) && area < -minArea) {
      holes_.insert(start);
      hole2BBox_.insert({start, bBox});
    } else {
      simples_.push_back(start);
      if (area > minArea) {
        outers_.push_back(start);
      }
    }
  }

  // All holes must be key-holed (attached to an outer polygon) before ear
  // clipping can commence. Instead of relying on sorting, which may be
  // incorrect due to epsilon, we check for polygon edges both ahead and
  // behind to ensure all valid options are found.
  void CutKeyhole(const VertItr start) {
    const Rect bBox = hole2BBox_[start];
    const int onTop = start->pos.y >= bBox.max.y - epsilon_   ? 1
                      : start->pos.y <= bBox.min.y + epsilon_ ? -1
                                                              : 0;
    VertItr connector = polygon_.end();

    auto CheckEdge = [&](VertItr edge) {
      const double x = edge->InterpY2X(start->pos, onTop, epsilon_);
      if (std::isfinite(x) && start->InsideEdge(edge, epsilon_, true) &&
          (connector == polygon_.end() ||
           CCW({x, start->pos.y}, connector->pos, connector->right->pos,
               epsilon_) == 1 ||
           (connector->pos.y < edge->pos.y
                ? edge->InsideEdge(connector, epsilon_, false)
                : !connector->InsideEdge(edge, epsilon_, false)))) {
        connector = edge;
      }
    };

    for (const VertItr first : outers_) {
      Loop(first, CheckEdge);
    }

    if (connector == polygon_.end()) {
      PRINT("hole did not find an outer contour!");
      simples_.push_back(start);
      return;
    }

    connector = FindCloserBridge(start, connector);

    JoinPolygons(start, connector);

#ifdef MANIFOLD_DEBUG
    if (params.verbose) {
      std::cout << "connected " << start->mesh_idx << " to "
                << connector->mesh_idx << std::endl;
    }
#endif
  }

  // This converts the initial guess for the keyhole location into the final one
  // and returns it. It does so by finding any reflex verts inside the triangle
  // containing the best connection and the initial horizontal line.
  VertItr FindCloserBridge(VertItr start, VertItr edge) {
    VertItr connector =
        edge->pos.x < start->pos.x          ? edge->right
        : edge->right->pos.x < start->pos.x ? edge
        : edge->right->pos.y - start->pos.y > start->pos.y - edge->pos.y
            ? edge
            : edge->right;
    if (la::abs(connector->pos.y - start->pos.y) <= epsilon_) {
      return connector;
    }
    const double above = connector->pos.y > start->pos.y ? 1 : -1;

    auto CheckVert = [&](VertItr vert) {
      const double inside =
          above * CCW(start->pos, vert->pos, connector->pos, epsilon_);
      if (vert->pos.x > start->pos.x - epsilon_ &&
          vert->pos.y * above > start->pos.y * above - epsilon_ &&
          (inside > 0 || (inside == 0 && vert->pos.x < connector->pos.x)) &&
          vert->InsideEdge(edge, epsilon_, true) && vert->IsReflex(epsilon_)) {
        connector = vert;
      }
    };

    for (const VertItr first : outers_) {
      Loop(first, CheckVert);
    }

    return connector;
  }

  // Creates a keyhole between the start vert of a hole and the connector vert
  // of an outer polygon. To do this, both verts are duplicated and reattached.
  // This process may create degenerate ears, so these are clipped if necessary
  // to keep from confusing subsequent key-holing operations.
  void JoinPolygons(VertItr start, VertItr connector) {
    polygon_.push_back(*start);
    const VertItr newStart = std::prev(polygon_.end());
    polygon_.push_back(*connector);
    const VertItr newConnector = std::prev(polygon_.end());

    start->right->left = newStart;
    connector->left->right = newConnector;
    Link(start, connector);
    Link(newConnector, newStart);

    ClipIfDegenerate(start);
    ClipIfDegenerate(newStart);
    ClipIfDegenerate(connector);
    ClipIfDegenerate(newConnector);
  }

  // Recalculate the cost of the Vert v ear, updating it in the queue by
  // removing and reinserting it.
  void ProcessEar(VertItr v, IdxCollider &collider) {
    if (v->ear != earsQueue_.end()) {
      earsQueue_.erase(v->ear);
      v->ear = earsQueue_.end();
    }
    if (v->IsShort(epsilon_)) {
      v->cost = kBest;
      v->ear = earsQueue_.insert(v);
    } else if (v->IsConvex(2 * epsilon_)) {
      v->cost = v->EarCost(epsilon_, collider);
      v->ear = earsQueue_.insert(v);
    } else {
      v->cost = 1;  // not used, but marks reflex verts for debug
    }
  }

  // Create a collider of all vertices in this polygon, each expanded by
  // epsilon_. Each ear uses this BVH to quickly find a subset of vertices to
  // check for cost.
  IdxCollider VertCollider(VertItr start) const {
    Vec<Box> vertBox;
    Vec<uint32_t> vertMorton;
    std::vector<VertItr> itr;
    const Box box(vec3(bBox_.min, 0), vec3(bBox_.max, 0));

    Loop(start, [&vertBox, &vertMorton, &itr, &box, this](VertItr v) {
      itr.push_back(v);
      const vec3 pos(v->pos, 0);
      vertBox.push_back({pos - epsilon_, pos + epsilon_});
      vertMorton.push_back(Collider::MortonCode(pos, box));
    });

    if (itr.empty()) {
      return {Collider(), itr};
    }

    const int numVert = itr.size();
    Vec<int> vertNew2Old(numVert);
    sequence(vertNew2Old.begin(), vertNew2Old.end());

    stable_sort(vertNew2Old.begin(), vertNew2Old.end(),
                [&vertMorton](const int a, const int b) {
                  return vertMorton[a] < vertMorton[b];
                });
    Permute(vertMorton, vertNew2Old);
    Permute(vertBox, vertNew2Old);
    Permute(itr, vertNew2Old);

    return {Collider(vertBox, vertMorton), itr};
  }

  // The main ear-clipping loop. This is called once for each simple polygon -
  // all holes have already been key-holed and joined to an outer polygon.
  void TriangulatePoly(VertItr start) {
    ZoneScoped;

    IdxCollider vertCollider = VertCollider(start);

    if (vertCollider.itr.empty()) {
      PRINT("Empty poly");
      return;
    }

    // A simple polygon always creates two fewer triangles than it has verts.
    int numTri = -2;
    earsQueue_.clear();

    auto QueueVert = [&](VertItr v) {
      ProcessEar(v, vertCollider);
      ++numTri;
      v->PrintVert();
    };

    VertItrC v = Loop(start, QueueVert);
    if (v == polygon_.end()) return;
    Dump(v);

    while (numTri > 0) {
      const qItr ear = earsQueue_.begin();
      if (ear != earsQueue_.end()) {
        v = *ear;
        // Cost should always be negative, generally < -epsilon.
        v->PrintVert();
        earsQueue_.erase(ear);
      } else {
        PRINT("No ear found!");
      }

      ClipEar(v);
      --numTri;

      ProcessEar(v->left, vertCollider);
      ProcessEar(v->right, vertCollider);
      // This is a backup vert that is used if the queue is empty (geometrically
      // invalid polygon), to ensure manifoldness.
      v = v->right;
    }

    DEBUG_ASSERT(v->right == v->left, logicErr, "Triangulator error!");
    PRINT("Finished poly");
  }

  void Dump(VertItrC start) const {
#ifdef MANIFOLD_DEBUG
    if (!params.verbose) return;
    VertItrC v = start;
    std::cout << "show(array([" << std::setprecision(15) << std::endl;
    do {
      std::cout << "  [" << v->pos.x << ", " << v->pos.y << "],# "
                << v->mesh_idx << ", cost: " << v->cost << std::endl;
      v = v->right;
    } while (v != start);
    std::cout << "  [" << v->pos.x << ", " << v->pos.y << "],# " << v->mesh_idx
              << std::endl;
    std::cout << "]))" << std::endl;

    v = start;
    std::cout << "polys.push_back({" << std::setprecision(15) << std::endl;
    do {
      std::cout << "    {" << v->pos.x << ", " << v->pos.y << "},  //"
                << std::endl;
      v = v->right;
    } while (v != start);
    std::cout << "});" << std::endl;
#endif
  }
};
}  // namespace

namespace manifold {

/**
 * @brief Triangulates a set of &epsilon;-valid polygons. If the input is not
 * &epsilon;-valid, the triangulation may overlap, but will always return a
 * manifold result that matches the input edge directions.
 *
 * @param polys The set of polygons, wound CCW and representing multiple
 * polygons and/or holes. These have 2D-projected positions as well as
 * references back to the original vertices.
 * @param epsilon The value of &epsilon;, bounding the uncertainty of the
 * input.
 * @return std::vector<ivec3> The triangles, referencing the original
 * vertex indicies.
 */
std::vector<ivec3> TriangulateIdx(const PolygonsIdx &polys, double epsilon) {
  std::vector<ivec3> triangles;
  double updatedEpsilon = epsilon;
#ifdef MANIFOLD_DEBUG
  try {
#endif
    if (IsConvex(polys, epsilon)) {  // fast path
      triangles = TriangulateConvex(polys);
    } else {
      EarClip triangulator(polys, epsilon);
      triangles = triangulator.Triangulate();
      updatedEpsilon = triangulator.GetPrecision();
    }
#ifdef MANIFOLD_DEBUG
    if (params.intermediateChecks) {
      CheckTopology(triangles, polys);
      if (!params.processOverlaps) {
        CheckGeometry(triangles, polys, 2 * updatedEpsilon);
      }
    }
  } catch (const geometryErr &e) {
    if (!params.suppressErrors) {
      PrintFailure(e, polys, triangles, updatedEpsilon);
    }
    throw;
  } catch (const std::exception &e) {
    PrintFailure(e, polys, triangles, updatedEpsilon);
    throw;
  }
#endif
  return triangles;
}

/**
 * @brief Triangulates a set of &epsilon;-valid polygons. If the input is not
 * &epsilon;-valid, the triangulation may overlap, but will always return a
 * manifold result that matches the input edge directions.
 *
 * @param polygons The set of polygons, wound CCW and representing multiple
 * polygons and/or holes.
 * @param epsilon The value of &epsilon;, bounding the uncertainty of the
 * input.
 * @return std::vector<ivec3> The triangles, referencing the original
 * polygon points in order.
 */
std::vector<ivec3> Triangulate(const Polygons &polygons, double epsilon) {
  int idx = 0;
  PolygonsIdx polygonsIndexed;
  for (const auto &poly : polygons) {
    SimplePolygonIdx simpleIndexed;
    for (const vec2 &polyVert : poly) {
      simpleIndexed.push_back({polyVert, idx++});
    }
    polygonsIndexed.push_back(simpleIndexed);
  }
  return TriangulateIdx(polygonsIndexed, epsilon);
}

ExecutionParams &PolygonParams() { return params; }

}  // namespace manifold
