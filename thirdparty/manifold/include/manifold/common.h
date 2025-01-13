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
#include <limits>
#include <vector>

#ifdef MANIFOLD_DEBUG
#include <chrono>
#endif

#include "manifold/linalg.h"

namespace manifold {
/** @addtogroup Math
 * @ingroup Core
 * @brief Simple math operations.
 * */

/** @addtogroup LinAlg
 *  @{
 */
namespace la = linalg;
using vec2 = la::vec<double, 2>;
using vec3 = la::vec<double, 3>;
using vec4 = la::vec<double, 4>;
using bvec4 = la::vec<bool, 4>;
using mat2 = la::mat<double, 2, 2>;
using mat3x2 = la::mat<double, 3, 2>;
using mat4x2 = la::mat<double, 4, 2>;
using mat2x3 = la::mat<double, 2, 3>;
using mat3 = la::mat<double, 3, 3>;
using mat4x3 = la::mat<double, 4, 3>;
using mat3x4 = la::mat<double, 3, 4>;
using mat4 = la::mat<double, 4, 4>;
using ivec2 = la::vec<int, 2>;
using ivec3 = la::vec<int, 3>;
using ivec4 = la::vec<int, 4>;
using quat = la::vec<double, 4>;
/** @} */

/** @addtogroup Scalar
 * @ingroup Math
 *  @brief Simple scalar operations.
 *  @{
 */

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kTwoPi = 6.28318530717958647692528676655900576;
constexpr double kHalfPi = 1.57079632679489661923132169163975144;

/**
 * Convert degrees to radians.
 *
 * @param a Angle in degrees.
 */
constexpr double radians(double a) { return a * kPi / 180; }

/**
 * Convert radians to degrees.
 *
 * @param a Angle in radians.
 */
constexpr double degrees(double a) { return a * 180 / kPi; }

/**
 * Performs smooth Hermite interpolation between 0 and 1 when edge0 < x < edge1.
 *
 * @param edge0 Specifies the value of the lower edge of the Hermite function.
 * @param edge1 Specifies the value of the upper edge of the Hermite function.
 * @param a Specifies the source value for interpolation.
 */
constexpr double smoothstep(double edge0, double edge1, double a) {
  const double x = la::clamp((a - edge0) / (edge1 - edge0), 0, 1);
  return x * x * (3 - 2 * x);
}

/**
 * Sine function where multiples of 90 degrees come out exact.
 *
 * @param x Angle in degrees.
 */
inline double sind(double x) {
  if (!la::isfinite(x)) return sin(x);
  if (x < 0.0) return -sind(-x);
  int quo;
  x = remquo(fabs(x), 90.0, &quo);
  switch (quo % 4) {
    case 0:
      return sin(radians(x));
    case 1:
      return cos(radians(x));
    case 2:
      return -sin(radians(x));
    case 3:
      return -cos(radians(x));
  }
  return 0.0;
}

/**
 * Cosine function where multiples of 90 degrees come out exact.
 *
 * @param x Angle in degrees.
 */
inline double cosd(double x) { return sind(x + 90.0); }
/** @} */

/** @addtogroup Structs
 * @ingroup Core
 * @brief Miscellaneous data structures for interfacing with this library.
 *  @{
 */

/**
 * @brief Single polygon contour, wound CCW. First and last point are implicitly
 * connected. Should ensure all input is
 * [&epsilon;-valid](https://github.com/elalish/manifold/wiki/Manifold-Library#definition-of-%CE%B5-valid).
 */
using SimplePolygon = std::vector<vec2>;

/**
 * @brief Set of polygons with holes. Order of contours is arbitrary. Can
 * contain any depth of nested holes and any number of separate polygons. Should
 * ensure all input is
 * [&epsilon;-valid](https://github.com/elalish/manifold/wiki/Manifold-Library#definition-of-%CE%B5-valid).
 */
using Polygons = std::vector<SimplePolygon>;

/**
 * @brief Defines which edges to sharpen and how much for the Manifold.Smooth()
 * constructor.
 */
struct Smoothness {
  /// The halfedge index = 3 * tri + i, referring to Mesh.triVerts[tri][i].
  size_t halfedge;
  /// A value between 0 and 1, where 0 is sharp and 1 is the default and the
  /// curvature is interpolated between these values. The two paired halfedges
  /// can have different values while maintaining C-1 continuity (except for 0).
  double smoothness;
};

/**
 * @brief Axis-aligned 3D box, primarily for bounding.
 */
struct Box {
  vec3 min = vec3(std::numeric_limits<double>::infinity());
  vec3 max = vec3(-std::numeric_limits<double>::infinity());

  /**
   * Default constructor is an infinite box that contains all space.
   */
  constexpr Box() {}

  /**
   * Creates a box that contains the two given points.
   */
  constexpr Box(const vec3 p1, const vec3 p2) {
    min = la::min(p1, p2);
    max = la::max(p1, p2);
  }

  /**
   * Returns the dimensions of the Box.
   */
  constexpr vec3 Size() const { return max - min; }

  /**
   * Returns the center point of the Box.
   */
  constexpr vec3 Center() const { return 0.5 * (max + min); }

  /**
   * Returns the absolute-largest coordinate value of any contained
   * point.
   */
  constexpr double Scale() const {
    vec3 absMax = la::max(la::abs(min), la::abs(max));
    return la::max(absMax.x, la::max(absMax.y, absMax.z));
  }

  /**
   * Does this box contain (includes equal) the given point?
   */
  constexpr bool Contains(const vec3& p) const {
    return la::all(la::gequal(p, min)) && la::all(la::gequal(max, p));
  }

  /**
   * Does this box contain (includes equal) the given box?
   */
  constexpr bool Contains(const Box& box) const {
    return la::all(la::gequal(box.min, min)) &&
           la::all(la::gequal(max, box.max));
  }

  /**
   * Expand this box to include the given point.
   */
  void Union(const vec3 p) {
    min = la::min(min, p);
    max = la::max(max, p);
  }

  /**
   * Expand this box to include the given box.
   */
  constexpr Box Union(const Box& box) const {
    Box out;
    out.min = la::min(min, box.min);
    out.max = la::max(max, box.max);
    return out;
  }

  /**
   * Transform the given box by the given axis-aligned affine transform.
   *
   * Ensure the transform passed in is axis-aligned (rotations are all
   * multiples of 90 degrees), or else the resulting bounding box will no longer
   * bound properly.
   */
  constexpr Box Transform(const mat3x4& transform) const {
    Box out;
    vec3 minT = transform * vec4(min, 1.0);
    vec3 maxT = transform * vec4(max, 1.0);
    out.min = la::min(minT, maxT);
    out.max = la::max(minT, maxT);
    return out;
  }

  /**
   * Shift this box by the given vector.
   */
  constexpr Box operator+(vec3 shift) const {
    Box out;
    out.min = min + shift;
    out.max = max + shift;
    return out;
  }

  /**
   * Shift this box in-place by the given vector.
   */
  Box& operator+=(vec3 shift) {
    min += shift;
    max += shift;
    return *this;
  }

  /**
   * Scale this box by the given vector.
   */
  constexpr Box operator*(vec3 scale) const {
    Box out;
    out.min = min * scale;
    out.max = max * scale;
    return out;
  }

  /**
   * Scale this box in-place by the given vector.
   */
  Box& operator*=(vec3 scale) {
    min *= scale;
    max *= scale;
    return *this;
  }

  /**
   * Does this box overlap the one given (including equality)?
   */
  constexpr bool DoesOverlap(const Box& box) const {
    return min.x <= box.max.x && min.y <= box.max.y && min.z <= box.max.z &&
           max.x >= box.min.x && max.y >= box.min.y && max.z >= box.min.z;
  }

  /**
   * Does the given point project within the XY extent of this box
   * (including equality)?
   */
  constexpr bool DoesOverlap(vec3 p) const {  // projected in z
    return p.x <= max.x && p.x >= min.x && p.y <= max.y && p.y >= min.y;
  }

  /**
   * Does this box have finite bounds?
   */
  constexpr bool IsFinite() const {
    return la::all(la::isfinite(min)) && la::all(la::isfinite(max));
  }
};

/**
 * @brief Axis-aligned 2D box, primarily for bounding.
 */
struct Rect {
  vec2 min = vec2(std::numeric_limits<double>::infinity());
  vec2 max = vec2(-std::numeric_limits<double>::infinity());

  /**
   * Default constructor is an empty rectangle..
   */
  constexpr Rect() {}

  /**
   * Create a rectangle that contains the two given points.
   */
  constexpr Rect(const vec2 a, const vec2 b) {
    min = la::min(a, b);
    max = la::max(a, b);
  }

  /** @name Information
   *  Details of the rectangle
   */
  ///@{

  /**
   * Return the dimensions of the rectangle.
   */
  constexpr vec2 Size() const { return max - min; }

  /**
   * Return the area of the rectangle.
   */
  constexpr double Area() const {
    auto sz = Size();
    return sz.x * sz.y;
  }

  /**
   * Returns the absolute-largest coordinate value of any contained
   * point.
   */
  constexpr double Scale() const {
    vec2 absMax = la::max(la::abs(min), la::abs(max));
    return la::max(absMax.x, absMax.y);
  }

  /**
   * Returns the center point of the rectangle.
   */
  constexpr vec2 Center() const { return 0.5 * (max + min); }

  /**
   * Does this rectangle contain (includes on border) the given point?
   */
  constexpr bool Contains(const vec2& p) const {
    return la::all(la::gequal(p, min)) && la::all(la::gequal(max, p));
  }

  /**
   * Does this rectangle contain (includes equal) the given rectangle?
   */
  constexpr bool Contains(const Rect& rect) const {
    return la::all(la::gequal(rect.min, min)) &&
           la::all(la::gequal(max, rect.max));
  }

  /**
   * Does this rectangle overlap the one given (including equality)?
   */
  constexpr bool DoesOverlap(const Rect& rect) const {
    return min.x <= rect.max.x && min.y <= rect.max.y && max.x >= rect.min.x &&
           max.y >= rect.min.y;
  }

  /**
   * Is the rectangle empty (containing no space)?
   */
  constexpr bool IsEmpty() const { return max.y <= min.y || max.x <= min.x; };

  /**
   * Does this recangle have finite bounds?
   */
  constexpr bool IsFinite() const {
    return la::all(la::isfinite(min)) && la::all(la::isfinite(max));
  }

  ///@}

  /** @name Modification
   */
  ///@{

  /**
   * Expand this rectangle (in place) to include the given point.
   */
  void Union(const vec2 p) {
    min = la::min(min, p);
    max = la::max(max, p);
  }

  /**
   * Expand this rectangle to include the given Rect.
   */
  constexpr Rect Union(const Rect& rect) const {
    Rect out;
    out.min = la::min(min, rect.min);
    out.max = la::max(max, rect.max);
    return out;
  }

  /**
   * Shift this rectangle by the given vector.
   */
  constexpr Rect operator+(const vec2 shift) const {
    Rect out;
    out.min = min + shift;
    out.max = max + shift;
    return out;
  }

  /**
   * Shift this rectangle in-place by the given vector.
   */
  Rect& operator+=(const vec2 shift) {
    min += shift;
    max += shift;
    return *this;
  }

  /**
   * Scale this rectangle by the given vector.
   */
  constexpr Rect operator*(const vec2 scale) const {
    Rect out;
    out.min = min * scale;
    out.max = max * scale;
    return out;
  }

  /**
   * Scale this rectangle in-place by the given vector.
   */
  Rect& operator*=(const vec2 scale) {
    min *= scale;
    max *= scale;
    return *this;
  }

  /**
   * Transform the rectangle by the given axis-aligned affine transform.
   *
   * Ensure the transform passed in is axis-aligned (rotations are all
   * multiples of 90 degrees), or else the resulting rectangle will no longer
   * bound properly.
   */
  constexpr Rect Transform(const mat2x3& m) const {
    Rect rect;
    rect.min = m * vec3(min, 1);
    rect.max = m * vec3(max, 1);
    return rect;
  }
  ///@}
};

/**
 * @brief Boolean operation type: Add (Union), Subtract (Difference), and
 * Intersect.
 */
enum class OpType { Add, Subtract, Intersect };

constexpr int DEFAULT_SEGMENTS = 0;
constexpr double DEFAULT_ANGLE = 10.0;
constexpr double DEFAULT_LENGTH = 1.0;
/**
 * @brief These static properties control how circular shapes are quantized by
 * default on construction.
 *
 * If circularSegments is specified, it takes
 * precedence. If it is zero, then instead the minimum is used of the segments
 * calculated based on edge length and angle, rounded up to the nearest
 * multiple of four. To get numbers not divisible by four, circularSegments
 * must be specified.
 */
class Quality {
 private:
  inline static int circularSegments_ = DEFAULT_SEGMENTS;
  inline static double circularAngle_ = DEFAULT_ANGLE;
  inline static double circularEdgeLength_ = DEFAULT_LENGTH;

 public:
  /**
   * Sets an angle constraint the default number of circular segments for the
   * CrossSection::Circle(), Manifold::Cylinder(), Manifold::Sphere(), and
   * Manifold::Revolve() constructors. The number of segments will be rounded up
   * to the nearest factor of four.
   *
   * @param angle The minimum angle in degrees between consecutive segments. The
   * angle will increase if the the segments hit the minimum edge length.
   * Default is 10 degrees.
   */
  static void SetMinCircularAngle(double angle) {
    if (angle <= 0) return;
    circularAngle_ = angle;
  }

  /**
   * Sets a length constraint the default number of circular segments for the
   * CrossSection::Circle(), Manifold::Cylinder(), Manifold::Sphere(), and
   * Manifold::Revolve() constructors. The number of segments will be rounded up
   * to the nearest factor of four.
   *
   * @param length The minimum length of segments. The length will
   * increase if the the segments hit the minimum angle. Default is 1.0.
   */
  static void SetMinCircularEdgeLength(double length) {
    if (length <= 0) return;
    circularEdgeLength_ = length;
  }

  /**
   * Sets the default number of circular segments for the
   * CrossSection::Circle(), Manifold::Cylinder(), Manifold::Sphere(), and
   * Manifold::Revolve() constructors. Overrides the edge length and angle
   * constraints and sets the number of segments to exactly this value.
   *
   * @param number Number of circular segments. Default is 0, meaning no
   * constraint is applied.
   */
  static void SetCircularSegments(int number) {
    if (number < 3 && number != 0) return;
    circularSegments_ = number;
  }

  /**
   * Determine the result of the SetMinCircularAngle(),
   * SetMinCircularEdgeLength(), and SetCircularSegments() defaults.
   *
   * @param radius For a given radius of circle, determine how many default
   * segments there will be.
   */
  static int GetCircularSegments(double radius) {
    if (circularSegments_ > 0) return circularSegments_;
    int nSegA = 360.0 / circularAngle_;
    int nSegL = 2.0 * radius * kPi / circularEdgeLength_;
    int nSeg = fmin(nSegA, nSegL) + 3;
    nSeg -= nSeg % 4;
    return std::max(nSeg, 3);
  }

  /**
   * Resets the circular construction parameters to their defaults if
   * SetMinCircularAngle, SetMinCircularEdgeLength, or SetCircularSegments have
   * been called.
   */
  static void ResetToDefaults() {
    circularSegments_ = DEFAULT_SEGMENTS;
    circularAngle_ = DEFAULT_ANGLE;
    circularEdgeLength_ = DEFAULT_LENGTH;
  }
};
/** @} */

/** @addtogroup Debug
 * @ingroup Optional
 * @{
 */

/**
 * @brief Global parameters that control debugging output. Only has an
 * effect when compiled with the MANIFOLD_DEBUG flag.
 */
struct ExecutionParams {
  /// Perform extra sanity checks and assertions on the intermediate data
  /// structures.
  bool intermediateChecks = false;
  /// Verbose output primarily of the Boolean, including timing info and vector
  /// sizes.
  bool verbose = false;
  /// If processOverlaps is false, a geometric check will be performed to assert
  /// all triangles are CCW.
  bool processOverlaps = true;
  /// Suppresses printed errors regarding CW triangles. Has no effect if
  /// processOverlaps is true.
  bool suppressErrors = false;
  /// Perform optional but recommended triangle cleanups in SimplifyTopology()
  bool cleanupTriangles = true;
};
/** @} */

#ifdef MANIFOLD_DEBUG
inline std::ostream& operator<<(std::ostream& stream, const Box& box) {
  return stream << "min: " << box.min << ", "
                << "max: " << box.max;
}

inline std::ostream& operator<<(std::ostream& stream, const Rect& box) {
  return stream << "min: " << box.min << ", "
                << "max: " << box.max;
}

/**
 * Print the contents of this vector to standard output. Only exists if compiled
 * with MANIFOLD_DEBUG flag.
 */
template <typename T>
void Dump(const std::vector<T>& vec) {
  std::cout << "Vec = " << std::endl;
  for (size_t i = 0; i < vec.size(); ++i) {
    std::cout << i << ", " << vec[i] << ", " << std::endl;
  }
  std::cout << std::endl;
}

template <typename T>
void Diff(const std::vector<T>& a, const std::vector<T>& b) {
  std::cout << "Diff = " << std::endl;
  if (a.size() != b.size()) {
    std::cout << "a and b must have the same length, aborting Diff"
              << std::endl;
    return;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i])
      std::cout << i << ": " << a[i] << ", " << b[i] << std::endl;
  }
  std::cout << std::endl;
}

struct Timer {
  std::chrono::high_resolution_clock::time_point start, end;

  void Start() { start = std::chrono::high_resolution_clock::now(); }

  void Stop() { end = std::chrono::high_resolution_clock::now(); }

  float Elapsed() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
  }
  void Print(std::string message) {
    std::cout << "----------- " << std::round(Elapsed()) << " ms for "
              << message << std::endl;
  }
};
#endif
}  // namespace manifold
