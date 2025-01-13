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

#include "manifold/cross_section.h"

#include "../utils.h"
#include "clipper2/clipper.core.h"
#include "clipper2/clipper.h"
#include "clipper2/clipper.offset.h"

namespace C2 = Clipper2Lib;

using namespace manifold;

namespace manifold {
struct PathImpl {
  PathImpl(const C2::PathsD paths_) : paths_(paths_) {}
  operator const C2::PathsD&() const { return paths_; }
  const C2::PathsD paths_;
};
}  // namespace manifold

namespace {
const int precision_ = 8;

C2::ClipType cliptype_of_op(OpType op) {
  C2::ClipType ct = C2::ClipType::Union;
  switch (op) {
    case OpType::Add:
      break;
    case OpType::Subtract:
      ct = C2::ClipType::Difference;
      break;
    case OpType::Intersect:
      ct = C2::ClipType::Intersection;
      break;
  };
  return ct;
}

C2::FillRule fr(CrossSection::FillRule fillrule) {
  C2::FillRule fr = C2::FillRule::EvenOdd;
  switch (fillrule) {
    case CrossSection::FillRule::EvenOdd:
      break;
    case CrossSection::FillRule::NonZero:
      fr = C2::FillRule::NonZero;
      break;
    case CrossSection::FillRule::Positive:
      fr = C2::FillRule::Positive;
      break;
    case CrossSection::FillRule::Negative:
      fr = C2::FillRule::Negative;
      break;
  };
  return fr;
}

C2::JoinType jt(CrossSection::JoinType jointype) {
  C2::JoinType jt = C2::JoinType::Square;
  switch (jointype) {
    case CrossSection::JoinType::Square:
      break;
    case CrossSection::JoinType::Round:
      jt = C2::JoinType::Round;
      break;
    case CrossSection::JoinType::Miter:
      jt = C2::JoinType::Miter;
      break;
  };
  return jt;
}

vec2 v2_of_pd(const C2::PointD p) { return {p.x, p.y}; }

C2::PointD v2_to_pd(const vec2 v) { return C2::PointD(v.x, v.y); }

C2::PathD pathd_of_contour(const SimplePolygon& ctr) {
  auto p = C2::PathD();
  p.reserve(ctr.size());
  for (auto v : ctr) {
    p.push_back(v2_to_pd(v));
  }
  return p;
}

C2::PathsD transform(const C2::PathsD ps, const mat2x3 m) {
  const bool invert = la::determinant(mat2(m)) < 0;
  auto transformed = C2::PathsD();
  transformed.reserve(ps.size());
  for (auto path : ps) {
    auto sz = path.size();
    auto s = C2::PathD(sz);
    for (size_t i = 0; i < sz; ++i) {
      auto idx = invert ? sz - 1 - i : i;
      s[idx] = v2_to_pd(m * vec3(path[i].x, path[i].y, 1));
    }
    transformed.push_back(s);
  }
  return transformed;
}

std::shared_ptr<const PathImpl> shared_paths(const C2::PathsD& ps) {
  return std::make_shared<const PathImpl>(ps);
}

// forward declaration for mutual recursion
void decompose_hole(const C2::PolyTreeD* outline,
                    std::vector<C2::PathsD>& polys, C2::PathsD& poly,
                    size_t n_holes, size_t j);

void decompose_outline(const C2::PolyTreeD* tree,
                       std::vector<C2::PathsD>& polys, size_t i) {
  auto n_outlines = tree->Count();
  if (i < n_outlines) {
    auto outline = tree->Child(i);
    auto n_holes = outline->Count();
    auto poly = C2::PathsD(n_holes + 1);
    poly[0] = outline->Polygon();
    decompose_hole(outline, polys, poly, n_holes, 0);
    polys.push_back(poly);
    if (i < n_outlines - 1) {
      decompose_outline(tree, polys, i + 1);
    }
  }
}

void decompose_hole(const C2::PolyTreeD* outline,
                    std::vector<C2::PathsD>& polys, C2::PathsD& poly,
                    size_t n_holes, size_t j) {
  if (j < n_holes) {
    auto child = outline->Child(j);
    decompose_outline(child, polys, 0);
    poly[j + 1] = child->Polygon();
    decompose_hole(outline, polys, poly, n_holes, j + 1);
  }
}

void flatten(const C2::PolyTreeD* tree, C2::PathsD& polys, size_t i) {
  auto n_outlines = tree->Count();
  if (i < n_outlines) {
    auto outline = tree->Child(i);
    flatten(outline, polys, 0);
    polys.push_back(outline->Polygon());
    if (i < n_outlines - 1) {
      flatten(tree, polys, i + 1);
    }
  }
}

bool V2Lesser(vec2 a, vec2 b) {
  if (a.x == b.x) return a.y < b.y;
  return a.x < b.x;
}

void HullBacktrack(const vec2& pt, std::vector<vec2>& stack) {
  auto sz = stack.size();
  while (sz >= 2 && CCW(stack[sz - 2], stack[sz - 1], pt, 0.0) <= 0.0) {
    stack.pop_back();
    sz = stack.size();
  }
}

// Based on method described here:
// https://www.hackerearth.com/practice/math/geometry/line-sweep-technique/tutorial/
// Changed to follow:
// https://en.wikibooks.org/wiki/Algorithm_Implementation/Geometry/Convex_hull/Monotone_chain
// This is the same algorithm (Andrew, also called Montone Chain).
C2::PathD HullImpl(SimplePolygon& pts) {
  size_t len = pts.size();
  if (len < 3) return C2::PathD();  // not enough points to create a polygon
  std::sort(pts.begin(), pts.end(), V2Lesser);

  auto lower = std::vector<vec2>{};
  for (auto& pt : pts) {
    HullBacktrack(pt, lower);
    lower.push_back(pt);
  }
  auto upper = std::vector<vec2>{};
  for (auto pt_iter = pts.rbegin(); pt_iter != pts.rend(); pt_iter++) {
    HullBacktrack(*pt_iter, upper);
    upper.push_back(*pt_iter);
  }

  upper.pop_back();
  lower.pop_back();

  auto path = C2::PathD();
  path.reserve(lower.size() + upper.size());
  for (const auto& l : lower) path.push_back(v2_to_pd(l));
  for (const auto& u : upper) path.push_back(v2_to_pd(u));
  return path;
}
}  // namespace

namespace manifold {

/**
 * The default constructor is an empty cross-section (containing no contours).
 */
CrossSection::CrossSection() {
  paths_ = std::make_shared<const PathImpl>(C2::PathsD());
}

CrossSection::~CrossSection() = default;
CrossSection::CrossSection(CrossSection&&) noexcept = default;
CrossSection& CrossSection::operator=(CrossSection&&) noexcept = default;

/**
 * The copy constructor avoids copying the underlying paths vector (sharing
 * with its parent via shared_ptr), however subsequent transformations, and
 * their application will not be shared. It is generally recommended to avoid
 * this, opting instead to simply create CrossSections with the available
 * const methods.
 */
CrossSection::CrossSection(const CrossSection& other) {
  paths_ = other.paths_;
  transform_ = other.transform_;
}

CrossSection& CrossSection::operator=(const CrossSection& other) {
  if (this != &other) {
    paths_ = other.paths_;
    transform_ = other.transform_;
  }
  return *this;
};

// Private, skips unioning.
CrossSection::CrossSection(std::shared_ptr<const PathImpl> ps) { paths_ = ps; }

/**
 * Create a 2d cross-section from a single contour. A boolean union operation
 * (with Positive filling rule by default) is performed to ensure the
 * resulting CrossSection is free of self-intersections.
 *
 * @param contour A closed path outlining the desired cross-section.
 * @param fillrule The filling rule used to interpret polygon sub-regions
 * created by self-intersections in contour.
 */
CrossSection::CrossSection(const SimplePolygon& contour, FillRule fillrule) {
  auto ps = C2::PathsD{(pathd_of_contour(contour))};
  paths_ = shared_paths(C2::Union(ps, fr(fillrule), precision_));
}

/**
 * Create a 2d cross-section from a set of contours (complex polygons). A
 * boolean union operation (with Positive filling rule by default) is
 * performed to combine overlapping polygons and ensure the resulting
 * CrossSection is free of intersections.
 *
 * @param contours A set of closed paths describing zero or more complex
 * polygons.
 * @param fillrule The filling rule used to interpret polygon sub-regions in
 * contours.
 */
CrossSection::CrossSection(const Polygons& contours, FillRule fillrule) {
  auto ps = C2::PathsD();
  ps.reserve(contours.size());
  for (auto ctr : contours) {
    ps.push_back(pathd_of_contour(ctr));
  }
  paths_ = shared_paths(C2::Union(ps, fr(fillrule), precision_));
}

/**
 * Create a 2d cross-section from an axis-aligned rectangle (bounding box).
 *
 * @param rect An axis-aligned rectangular bounding box.
 */
CrossSection::CrossSection(const Rect& rect) {
  C2::PathD p(4);
  p[0] = C2::PointD(rect.min.x, rect.min.y);
  p[1] = C2::PointD(rect.max.x, rect.min.y);
  p[2] = C2::PointD(rect.max.x, rect.max.y);
  p[3] = C2::PointD(rect.min.x, rect.max.y);
  paths_ = shared_paths(C2::PathsD{p});
}

// Private
// All access to paths_ should be done through the GetPaths() method, which
// applies the accumulated transform_
std::shared_ptr<const PathImpl> CrossSection::GetPaths() const {
  if (transform_ == mat2x3(la::identity)) {
    return paths_;
  }
  paths_ = shared_paths(::transform(paths_->paths_, transform_));
  transform_ = mat2x3(la::identity);
  return paths_;
}

/**
 * Constructs a square with the given XY dimensions. By default it is
 * positioned in the first quadrant, touching the origin. If any dimensions in
 * size are negative, or if all are zero, an empty Manifold will be returned.
 *
 * @param size The X, and Y dimensions of the square.
 * @param center Set to true to shift the center to the origin.
 */
CrossSection CrossSection::Square(const vec2 size, bool center) {
  if (size.x < 0.0 || size.y < 0.0 || la::length(size) == 0.0) {
    return CrossSection();
  }

  auto p = C2::PathD(4);
  if (center) {
    const auto w = size.x / 2;
    const auto h = size.y / 2;
    p[0] = C2::PointD(w, h);
    p[1] = C2::PointD(-w, h);
    p[2] = C2::PointD(-w, -h);
    p[3] = C2::PointD(w, -h);
  } else {
    const double x = size.x;
    const double y = size.y;
    p[0] = C2::PointD(0.0, 0.0);
    p[1] = C2::PointD(x, 0.0);
    p[2] = C2::PointD(x, y);
    p[3] = C2::PointD(0.0, y);
  }
  return CrossSection(shared_paths(C2::PathsD{p}));
}

/**
 * Constructs a circle of a given radius.
 *
 * @param radius Radius of the circle. Must be positive.
 * @param circularSegments Number of segments along its diameter. Default is
 * calculated by the static Quality defaults according to the radius.
 */
CrossSection CrossSection::Circle(double radius, int circularSegments) {
  if (radius <= 0.0) {
    return CrossSection();
  }
  int n = circularSegments > 2 ? circularSegments
                               : Quality::GetCircularSegments(radius);
  double dPhi = 360.0 / n;
  auto circle = C2::PathD(n);
  for (int i = 0; i < n; ++i) {
    circle[i] = C2::PointD(radius * cosd(dPhi * i), radius * sind(dPhi * i));
  }
  return CrossSection(shared_paths(C2::PathsD{circle}));
}

/**
 * Perform the given boolean operation between this and another CrossSection.
 */
CrossSection CrossSection::Boolean(const CrossSection& second,
                                   OpType op) const {
  auto ct = cliptype_of_op(op);
  auto res = C2::BooleanOp(ct, C2::FillRule::Positive, GetPaths()->paths_,
                           second.GetPaths()->paths_, precision_);
  return CrossSection(shared_paths(res));
}

/**
 * Perform the given boolean operation on a list of CrossSections. In case of
 * Subtract, all CrossSections in the tail are differenced from the head.
 */
CrossSection CrossSection::BatchBoolean(
    const std::vector<CrossSection>& crossSections, OpType op) {
  if (crossSections.size() == 0)
    return CrossSection();
  else if (crossSections.size() == 1)
    return crossSections[0];

  auto subjs = crossSections[0].GetPaths();
  int n_clips = 0;
  for (size_t i = 1; i < crossSections.size(); ++i) {
    n_clips += crossSections[i].GetPaths()->paths_.size();
  }
  auto clips = C2::PathsD();
  clips.reserve(n_clips);
  for (size_t i = 1; i < crossSections.size(); ++i) {
    auto ps = crossSections[i].GetPaths();
    clips.insert(clips.end(), ps->paths_.begin(), ps->paths_.end());
  }

  auto ct = cliptype_of_op(op);
  auto res = C2::BooleanOp(ct, C2::FillRule::Positive, subjs->paths_, clips,
                           precision_);
  return CrossSection(shared_paths(res));
}

/**
 * Compute the boolean union between two cross-sections.
 */
CrossSection CrossSection::operator+(const CrossSection& Q) const {
  return Boolean(Q, OpType::Add);
}

/**
 * Compute the boolean union between two cross-sections, assigning the result
 * to the first.
 */
CrossSection& CrossSection::operator+=(const CrossSection& Q) {
  *this = *this + Q;
  return *this;
}

/**
 * Compute the boolean difference of a (clip) cross-section from another
 * (subject).
 */
CrossSection CrossSection::operator-(const CrossSection& Q) const {
  return Boolean(Q, OpType::Subtract);
}

/**
 * Compute the boolean difference of a (clip) cross-section from a another
 * (subject), assigning the result to the subject.
 */
CrossSection& CrossSection::operator-=(const CrossSection& Q) {
  *this = *this - Q;
  return *this;
}

/**
 * Compute the boolean intersection between two cross-sections.
 */
CrossSection CrossSection::operator^(const CrossSection& Q) const {
  return Boolean(Q, OpType::Intersect);
}

/**
 * Compute the boolean intersection between two cross-sections, assigning the
 * result to the first.
 */
CrossSection& CrossSection::operator^=(const CrossSection& Q) {
  *this = *this ^ Q;
  return *this;
}

/**
 * Construct a CrossSection from a vector of other CrossSections (batch
 * boolean union).
 */
CrossSection CrossSection::Compose(std::vector<CrossSection>& crossSections) {
  return BatchBoolean(crossSections, OpType::Add);
}

/**
 * This operation returns a vector of CrossSections that are topologically
 * disconnected, each containing one outline contour with zero or more
 * holes.
 */
std::vector<CrossSection> CrossSection::Decompose() const {
  if (NumContour() < 2) {
    return std::vector<CrossSection>{CrossSection(*this)};
  }

  C2::PolyTreeD tree;
  C2::BooleanOp(C2::ClipType::Union, C2::FillRule::Positive, GetPaths()->paths_,
                C2::PathsD(), tree, precision_);

  auto polys = std::vector<C2::PathsD>();
  decompose_outline(&tree, polys, 0);

  auto comps = std::vector<CrossSection>();
  comps.reserve(polys.size());
  // reverse the stack while wrapping
  for (auto poly = polys.rbegin(); poly != polys.rend(); ++poly)
    comps.emplace_back(CrossSection(shared_paths(*poly)));

  return comps;
}

/**
 * Move this CrossSection in space. This operation can be chained. Transforms
 * are combined and applied lazily.
 *
 * @param v The vector to add to every vertex.
 */
CrossSection CrossSection::Translate(const vec2 v) const {
  mat2x3 m({1.0, 0.0},  //
           {0.0, 1.0},  //
           {v.x, v.y});
  return Transform(m);
}

/**
 * Applies a (Z-axis) rotation to the CrossSection, in degrees. This operation
 * can be chained. Transforms are combined and applied lazily.
 *
 * @param degrees degrees about the Z-axis to rotate.
 */
CrossSection CrossSection::Rotate(double degrees) const {
  auto s = sind(degrees);
  auto c = cosd(degrees);
  mat2x3 m({c, s},   //
           {-s, c},  //
           {0.0, 0.0});
  return Transform(m);
}

/**
 * Scale this CrossSection in space. This operation can be chained. Transforms
 * are combined and applied lazily.
 *
 * @param scale The vector to multiply every vertex by per component.
 */
CrossSection CrossSection::Scale(const vec2 scale) const {
  mat2x3 m({scale.x, 0.0},  //
           {0.0, scale.y},  //
           {0.0, 0.0});
  return Transform(m);
}

/**
 * Mirror this CrossSection over the arbitrary axis described by the unit form
 * of the given vector. If the length of the vector is zero, an empty
 * CrossSection is returned. This operation can be chained. Transforms are
 * combined and applied lazily.
 *
 * @param ax the axis to be mirrored over
 */
CrossSection CrossSection::Mirror(const vec2 ax) const {
  if (la::length(ax) == 0.) {
    return CrossSection();
  }
  auto n = la::normalize(la::abs(ax));
  auto m = mat2x3(mat2(la::identity) - 2.0 * la::outerprod(n, n), vec2(0.0));
  return Transform(m);
}

/**
 * Transform this CrossSection in space. The first two columns form a 2x2
 * matrix transform and the last is a translation vector. This operation can
 * be chained. Transforms are combined and applied lazily.
 *
 * @param m The affine transform matrix to apply to all the vertices.
 */
CrossSection CrossSection::Transform(const mat2x3& m) const {
  auto transformed = CrossSection();
  transformed.transform_ = m * Mat3(transform_);
  transformed.paths_ = paths_;
  return transformed;
}

/**
 * Move the vertices of this CrossSection (creating a new one) according to
 * any arbitrary input function, followed by a union operation (with a
 * Positive fill rule) that ensures any introduced intersections are not
 * included in the result.
 *
 * @param warpFunc A function that modifies a given vertex position.
 */
CrossSection CrossSection::Warp(std::function<void(vec2&)> warpFunc) const {
  return WarpBatch([&warpFunc](VecView<vec2> vecs) {
    for (vec2& p : vecs) {
      warpFunc(p);
    }
  });
}

/**
 * Same as CrossSection::Warp but calls warpFunc with
 * a VecView which is roughly equivalent to std::span
 * pointing to all vec2 elements to be modified in-place
 *
 * @param warpFunc A function that modifies multiple vertex positions.
 */
CrossSection CrossSection::WarpBatch(
    std::function<void(VecView<vec2>)> warpFunc) const {
  std::vector<vec2> tmp_verts;
  C2::PathsD paths = GetPaths()->paths_;  // deep copy
  for (C2::PathD const& path : paths) {
    for (C2::PointD const& p : path) {
      tmp_verts.push_back(v2_of_pd(p));
    }
  }

  warpFunc(VecView<vec2>(tmp_verts.data(), tmp_verts.size()));

  auto cursor = tmp_verts.begin();
  for (C2::PathD& path : paths) {
    for (C2::PointD& p : path) {
      p = v2_to_pd(*cursor);
      ++cursor;
    }
  }

  return CrossSection(
      shared_paths(C2::Union(paths, C2::FillRule::Positive, precision_)));
}

/**
 * Remove vertices from the contours in this CrossSection that are less than
 * the specified distance epsilon from an imaginary line that passes through
 * its two adjacent vertices. Near duplicate vertices and collinear points
 * will be removed at lower epsilons, with elimination of line segments
 * becoming increasingly aggressive with larger epsilons.
 *
 * It is recommended to apply this function following Offset, in order to
 * clean up any spurious tiny line segments introduced that do not improve
 * quality in any meaningful way. This is particularly important if further
 * offseting operations are to be performed, which would compound the issue.
 */
CrossSection CrossSection::Simplify(double epsilon) const {
  C2::PolyTreeD tree;
  C2::BooleanOp(C2::ClipType::Union, C2::FillRule::Positive, GetPaths()->paths_,
                C2::PathsD(), tree, precision_);

  C2::PathsD polys;
  flatten(&tree, polys, 0);

  // Filter out contours less than epsilon wide.
  C2::PathsD filtered;
  for (C2::PathD poly : polys) {
    auto area = C2::Area(poly);
    Rect box;
    for (auto vert : poly) {
      box.Union(vec2(vert.x, vert.y));
    }
    vec2 size = box.Size();
    if (std::abs(area) > std::max(size.x, size.y) * epsilon) {
      filtered.push_back(poly);
    }
  }

  auto ps = SimplifyPaths(filtered, epsilon, true);
  return CrossSection(shared_paths(ps));
}

/**
 * Inflate the contours in CrossSection by the specified delta, handling
 * corners according to the given JoinType.
 *
 * @param delta Positive deltas will cause the expansion of outlining contours
 * to expand, and retraction of inner (hole) contours. Negative deltas will
 * have the opposite effect.
 * @param jointype The join type specifying the treatment of contour joins
 * (corners).
 * @param miter_limit The maximum distance in multiples of delta that vertices
 * can be offset from their original positions with before squaring is
 * applied, <B>when the join type is Miter</B> (default is 2, which is the
 * minimum allowed). See the [Clipper2
 * MiterLimit](http://www.angusj.com/clipper2/Docs/Units/Clipper.Offset/Classes/ClipperOffset/Properties/MiterLimit.htm)
 * page for a visual example.
 * @param circularSegments Number of segments per 360 degrees of
 * <B>JoinType::Round</B> corners (roughly, the number of vertices that
 * will be added to each contour). Default is calculated by the static Quality
 * defaults according to the radius.
 */
CrossSection CrossSection::Offset(double delta, JoinType jointype,
                                  double miter_limit,
                                  int circularSegments) const {
  double arc_tol = 0.;
  if (jointype == JoinType::Round) {
    int n = circularSegments > 2 ? circularSegments
                                 : Quality::GetCircularSegments(delta);
    // This calculates tolerance as a function of circular segments and delta
    // (radius) in order to get back the same number of segments in Clipper2:
    // steps_per_360 = PI / acos(1 - arc_tol / abs_delta)
    const double abs_delta = std::fabs(delta);
    const double scaled_delta = abs_delta * std::pow(10, precision_);
    arc_tol = (std::cos(Clipper2Lib::PI / n) - 1) * -scaled_delta;
  }
  auto ps =
      C2::InflatePaths(GetPaths()->paths_, delta, jt(jointype),
                       C2::EndType::Polygon, miter_limit, precision_, arc_tol);
  return CrossSection(shared_paths(ps));
}

/**
 * Compute the convex hull enveloping a set of cross-sections.
 *
 * @param crossSections A vector of cross-sections over which to compute a
 * convex hull.
 */
CrossSection CrossSection::Hull(
    const std::vector<CrossSection>& crossSections) {
  int n = 0;
  for (auto cs : crossSections) n += cs.NumVert();
  SimplePolygon pts;
  pts.reserve(n);
  for (auto cs : crossSections) {
    auto paths = cs.GetPaths()->paths_;
    for (auto path : paths) {
      for (auto p : path) {
        pts.push_back(v2_of_pd(p));
      }
    }
  }
  return CrossSection(shared_paths(C2::PathsD{HullImpl(pts)}));
}

/**
 * Compute the convex hull of this cross-section.
 */
CrossSection CrossSection::Hull() const {
  return Hull(std::vector<CrossSection>{*this});
}

/**
 * Compute the convex hull of a set of points. If the given points are fewer
 * than 3, an empty CrossSection will be returned.
 *
 * @param pts A vector of 2-dimensional points over which to compute a convex
 * hull.
 */
CrossSection CrossSection::Hull(SimplePolygon pts) {
  return CrossSection(shared_paths(C2::PathsD{HullImpl(pts)}));
}

/**
 * Compute the convex hull of a set of points/polygons. If the given points are
 * fewer than 3, an empty CrossSection will be returned.
 *
 * @param polys A vector of vectors of 2-dimensional points over which to
 * compute a convex hull.
 */
CrossSection CrossSection::Hull(const Polygons polys) {
  SimplePolygon pts;
  for (auto poly : polys) {
    for (auto p : poly) {
      pts.push_back(p);
    }
  }
  return Hull(pts);
}

/**
 * Return the total area covered by complex polygons making up the
 * CrossSection.
 */
double CrossSection::Area() const { return C2::Area(GetPaths()->paths_); }

/**
 * Return the number of vertices in the CrossSection.
 */
size_t CrossSection::NumVert() const {
  size_t n = 0;
  auto paths = GetPaths()->paths_;
  for (auto p : paths) {
    n += p.size();
  }
  return n;
}

/**
 * Return the number of contours (both outer and inner paths) in the
 * CrossSection.
 */
size_t CrossSection::NumContour() const { return GetPaths()->paths_.size(); }

/**
 * Does the CrossSection contain any contours?
 */
bool CrossSection::IsEmpty() const { return GetPaths()->paths_.empty(); }

/**
 * Returns the axis-aligned bounding rectangle of all the CrossSections'
 * vertices.
 */
Rect CrossSection::Bounds() const {
  auto r = C2::GetBounds(GetPaths()->paths_);
  return Rect({r.left, r.bottom}, {r.right, r.top});
}

/**
 * Return the contours of this CrossSection as a Polygons.
 */
Polygons CrossSection::ToPolygons() const {
  auto polys = Polygons();
  auto paths = GetPaths()->paths_;
  polys.reserve(paths.size());
  for (auto p : paths) {
    auto sp = SimplePolygon();
    sp.reserve(p.size());
    for (auto v : p) {
      sp.push_back({v.x, v.y});
    }
    polys.push_back(sp);
  }
  return polys;
}
}  // namespace manifold
