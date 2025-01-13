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
#include <functional>
#include <memory>

#include "manifold/common.h"
#include "manifold/vec_view.h"

namespace manifold {

/**
 * @ingroup Debug
 *
 * Allows modification of the assertions checked in MANIFOLD_DEBUG mode.
 *
 * @return ExecutionParams&
 */
ExecutionParams& ManifoldParams();

class CsgNode;
class CsgLeafNode;

/** @addtogroup Core
 *  @brief The central classes of the library
 *  @{
 */

/**
 * @brief Mesh input/output suitable for pushing directly into graphics
 * libraries.
 *
 * This may not be manifold since the verts are duplicated along property
 * boundaries that do not match. The additional merge vectors store this missing
 * information, allowing the manifold to be reconstructed. MeshGL is an alias
 * for the standard single-precision version. Use MeshGL64 to output the full
 * double precision that Manifold uses internally.
 */
template <typename Precision, typename I = uint32_t>
struct MeshGLP {
  /// Number of property vertices
  I NumVert() const { return vertProperties.size() / numProp; };
  /// Number of triangles
  I NumTri() const { return triVerts.size() / 3; };
  /// Number of properties per vertex, always >= 3.
  I numProp = 3;
  /// Flat, GL-style interleaved list of all vertex properties: propVal =
  /// vertProperties[vert * numProp + propIdx]. The first three properties are
  /// always the position x, y, z.
  std::vector<Precision> vertProperties;
  /// The vertex indices of the three triangle corners in CCW (from the outside)
  /// order, for each triangle.
  std::vector<I> triVerts;
  /// Optional: A list of only the vertex indicies that need to be merged to
  /// reconstruct the manifold.
  std::vector<I> mergeFromVert;
  /// Optional: The same length as mergeFromVert, and the corresponding value
  /// contains the vertex to merge with. It will have an identical position, but
  /// the other properties may differ.
  std::vector<I> mergeToVert;
  /// Optional: Indicates runs of triangles that correspond to a particular
  /// input mesh instance. The runs encompass all of triVerts and are sorted
  /// by runOriginalID. Run i begins at triVerts[runIndex[i]] and ends at
  /// triVerts[runIndex[i+1]]. All runIndex values are divisible by 3. Returned
  /// runIndex will always be 1 longer than runOriginalID, but same length is
  /// also allowed as input: triVerts.size() will be automatically appended in
  /// this case.
  std::vector<I> runIndex;
  /// Optional: The OriginalID of the mesh this triangle run came from. This ID
  /// is ideal for reapplying materials to the output mesh. Multiple runs may
  /// have the same ID, e.g. representing different copies of the same input
  /// mesh. If you create an input MeshGL that you want to be able to reference
  /// as one or more originals, be sure to set unique values from ReserveIDs().
  std::vector<uint32_t> runOriginalID;
  /// Optional: For each run, a 3x4 transform is stored representing how the
  /// corresponding original mesh was transformed to create this triangle run.
  /// This matrix is stored in column-major order and the length of the overall
  /// vector is 12 * runOriginalID.size().
  std::vector<Precision> runTransform;
  /// Optional: Length NumTri, contains the source face ID this
  /// triangle comes from. When auto-generated, this ID will be a triangle index
  /// into the original mesh. This index/ID is purely for external use (e.g.
  /// recreating polygonal faces) and will not affect Manifold's algorithms.
  std::vector<I> faceID;
  /// Optional: The X-Y-Z-W weighted tangent vectors for smooth Refine(). If
  /// non-empty, must be exactly four times as long as Mesh.triVerts. Indexed
  /// as 4 * (3 * tri + i) + j, i < 3, j < 4, representing the tangent value
  /// Mesh.triVerts[tri][i] along the CCW edge. If empty, mesh is faceted.
  std::vector<Precision> halfedgeTangent;
  /// Tolerance for mesh simplification. When creating a Manifold, the tolerance
  /// used will be the maximum of this and a baseline tolerance from the size of
  /// the bounding box. Any edge shorter than tolerance may be collapsed.
  /// Tolerance may be enlarged when floating point error accumulates.
  Precision tolerance = 0;

  MeshGLP() = default;

  /**
   * Updates the mergeFromVert and mergeToVert vectors in order to create a
   * manifold solid. If the MeshGL is already manifold, no change will occur and
   * the function will return false. Otherwise, this will merge verts along open
   * edges within tolerance (the maximum of the MeshGL tolerance and the
   * baseline bounding-box tolerance), keeping any from the existing merge
   * vectors, and return true.
   *
   * There is no guarantee the result will be manifold - this is a best-effort
   * helper function designed primarily to aid in the case where a manifold
   * multi-material MeshGL was produced, but its merge vectors were lost due to
   * a round-trip through a file format. Constructing a Manifold from the result
   * will report an error status if it is not manifold.
   */
  bool Merge();

  /**
   * Returns the x, y, z position of the ith vertex.
   *
   * @param v vertex index.
   */
  la::vec<Precision, 3> GetVertPos(size_t v) const {
    size_t offset = v * numProp;
    return la::vec<Precision, 3>(vertProperties[offset],
                                 vertProperties[offset + 1],
                                 vertProperties[offset + 2]);
  }

  /**
   * Returns the three vertex indices of the ith triangle.
   *
   * @param t triangle index.
   */
  la::vec<I, 3> GetTriVerts(size_t t) const {
    size_t offset = 3 * t;
    return la::vec<I, 3>(triVerts[offset], triVerts[offset + 1],
                         triVerts[offset + 2]);
  }

  /**
   * Returns the x, y, z, w tangent of the ith halfedge.
   *
   * @param h halfedge index (3 * triangle_index + [0|1|2]).
   */
  la::vec<Precision, 4> GetTangent(size_t h) const {
    size_t offset = 4 * h;
    return la::vec<Precision, 4>(
        halfedgeTangent[offset], halfedgeTangent[offset + 1],
        halfedgeTangent[offset + 2], halfedgeTangent[offset + 3]);
  }
};

/**
 * @brief Single-precision - ideal for most uses, especially graphics.
 */
using MeshGL = MeshGLP<float>;
/**
 * @brief Double-precision, 64-bit indices - best for huge meshes.
 */
using MeshGL64 = MeshGLP<double, uint64_t>;

/**
 * @brief This library's internal representation of an oriented, 2-manifold,
 * triangle mesh - a simple boundary-representation of a solid object. Use this
 * class to store and operate on solids, and use MeshGL for input and output.
 *
 * In addition to storing geometric data, a Manifold can also store an arbitrary
 * number of vertex properties. These could be anything, e.g. normals, UV
 * coordinates, colors, etc, but this library is completely agnostic. All
 * properties are merely float values indexed by channel number. It is up to the
 * user to associate channel numbers with meaning.
 *
 * Manifold allows vertex properties to be shared for efficient storage, or to
 * have multiple property verts associated with a single geometric vertex,
 * allowing sudden property changes, e.g. at Boolean intersections, without
 * sacrificing manifoldness.
 *
 * Manifolds also keep track of their relationships to their inputs, via
 * OriginalIDs and the faceIDs and transforms accessible through MeshGL. This
 * allows object-level properties to be re-associated with the output after many
 * operations, particularly useful for materials. Since separate object's
 * properties are not mixed, there is no requirement that channels have
 * consistent meaning between different inputs.
 */
class Manifold {
 public:
  /** @name Basics
   *  Copy / move / assignment
   */
  ///@{
  Manifold();
  ~Manifold();
  Manifold(const Manifold& other);
  Manifold& operator=(const Manifold& other);
  Manifold(Manifold&&) noexcept;
  Manifold& operator=(Manifold&&) noexcept;
  ///@}

  /** @name Input & Output
   *  Create and retrieve arbitrary manifolds
   */
  ///@{
  Manifold(const MeshGL&);
  Manifold(const MeshGL64&);
  MeshGL GetMeshGL(int normalIdx = -1) const;
  MeshGL64 GetMeshGL64(int normalIdx = -1) const;
  ///@}

  /** @name Constructors
   *  Topological ops, primitives, and SDF
   */
  ///@{
  std::vector<Manifold> Decompose() const;
  static Manifold Compose(const std::vector<Manifold>&);
  static Manifold Tetrahedron();
  static Manifold Cube(vec3 size = vec3(1.0), bool center = false);
  static Manifold Cylinder(double height, double radiusLow,
                           double radiusHigh = -1.0, int circularSegments = 0,
                           bool center = false);
  static Manifold Sphere(double radius, int circularSegments = 0);
  static Manifold LevelSet(std::function<double(vec3)> sdf, Box bounds,
                           double edgeLength, double level = 0,
                           double tolerance = -1, bool canParallel = true);
  ///@}

  /** @name Polygons
   * 3D to 2D and 2D to 3D
   */
  ///@{
  Polygons Slice(double height = 0) const;
  Polygons Project() const;
  static Manifold Extrude(const Polygons& crossSection, double height,
                          int nDivisions = 0, double twistDegrees = 0.0,
                          vec2 scaleTop = vec2(1.0));
  static Manifold Revolve(const Polygons& crossSection,
                          int circularSegments = 0,
                          double revolveDegrees = 360.0f);
  ///@}

  enum class Error {
    NoError,
    NonFiniteVertex,
    NotManifold,
    VertexOutOfBounds,
    PropertiesWrongLength,
    MissingPositionProperties,
    MergeVectorsDifferentLengths,
    MergeIndexOutOfBounds,
    TransformWrongLength,
    RunIndexWrongLength,
    FaceIDWrongLength,
    InvalidConstruction,
  };

  /** @name Information
   *  Details of the manifold
   */
  ///@{
  Error Status() const;
  bool IsEmpty() const;
  size_t NumVert() const;
  size_t NumEdge() const;
  size_t NumTri() const;
  size_t NumProp() const;
  size_t NumPropVert() const;
  Box BoundingBox() const;
  int Genus() const;
  double GetTolerance() const;
  ///@}

  /** @name Measurement
   */
  ///@{
  double SurfaceArea() const;
  double Volume() const;
  double MinGap(const Manifold& other, double searchLength) const;
  ///@}

  /** @name Mesh ID
   *  Details of the manifold's relation to its input meshes, for the purposes
   * of reapplying mesh properties.
   */
  ///@{
  int OriginalID() const;
  Manifold AsOriginal() const;
  static uint32_t ReserveIDs(uint32_t);
  ///@}

  /** @name Transformations
   */
  ///@{
  Manifold Translate(vec3) const;
  Manifold Scale(vec3) const;
  Manifold Rotate(double xDegrees, double yDegrees = 0.0,
                  double zDegrees = 0.0) const;
  Manifold Mirror(vec3) const;
  Manifold Transform(const mat3x4&) const;
  Manifold Warp(std::function<void(vec3&)>) const;
  Manifold WarpBatch(std::function<void(VecView<vec3>)>) const;
  Manifold SetTolerance(double) const;
  ///@}

  /** @name Boolean
   *  Combine two manifolds
   */
  ///@{
  Manifold Boolean(const Manifold& second, OpType op) const;
  static Manifold BatchBoolean(const std::vector<Manifold>& manifolds,
                               OpType op);
  // Boolean operation shorthand
  Manifold operator+(const Manifold&) const;  // Add (Union)
  Manifold& operator+=(const Manifold&);
  Manifold operator-(const Manifold&) const;  // Subtract (Difference)
  Manifold& operator-=(const Manifold&);
  Manifold operator^(const Manifold&) const;  // Intersect
  Manifold& operator^=(const Manifold&);
  std::pair<Manifold, Manifold> Split(const Manifold&) const;
  std::pair<Manifold, Manifold> SplitByPlane(vec3 normal,
                                             double originOffset) const;
  Manifold TrimByPlane(vec3 normal, double originOffset) const;
  ///@}

  /** @name Properties
   * Create and modify vertex properties.
   */
  ///@{
  Manifold SetProperties(
      int numProp,
      std::function<void(double*, vec3, const double*)> propFunc) const;
  Manifold CalculateCurvature(int gaussianIdx, int meanIdx) const;
  Manifold CalculateNormals(int normalIdx, double minSharpAngle = 60) const;
  ///@}

  /** @name Smoothing
   * Smooth meshes by calculating tangent vectors and refining to a higher
   * triangle count.
   */
  ///@{
  Manifold Refine(int) const;
  Manifold RefineToLength(double) const;
  Manifold RefineToTolerance(double) const;
  Manifold SmoothByNormals(int normalIdx) const;
  Manifold SmoothOut(double minSharpAngle = 60, double minSmoothness = 0) const;
  static Manifold Smooth(const MeshGL&,
                         const std::vector<Smoothness>& sharpenedEdges = {});
  static Manifold Smooth(const MeshGL64&,
                         const std::vector<Smoothness>& sharpenedEdges = {});
  ///@}

  /** @name Convex Hull
   */
  ///@{
  Manifold Hull() const;
  static Manifold Hull(const std::vector<Manifold>& manifolds);
  static Manifold Hull(const std::vector<vec3>& pts);
  ///@}

  /** @name Testing Hooks
   *  These are just for internal testing.
   */
  ///@{
  bool MatchesTriNormals() const;
  size_t NumDegenerateTris() const;
  size_t NumOverlaps(const Manifold& second) const;
  double GetEpsilon() const;
  ///@}

  struct Impl;

 private:
  Manifold(std::shared_ptr<CsgNode> pNode_);
  Manifold(std::shared_ptr<Impl> pImpl_);
  static Manifold Invalid();
  mutable std::shared_ptr<CsgNode> pNode_;

  CsgLeafNode& GetCsgLeafNode() const;
};
/** @} */

/** @addtogroup Debug
 *  @ingroup Optional
 *  @brief Debugging features
 *
 * The features require compiler flags to be enabled. Assertions are enabled
 * with the MANIFOLD_DEBUG flag and then controlled with ExecutionParams.
 *  @{
 */
#ifdef MANIFOLD_DEBUG
inline std::string ToString(const Manifold::Error& error) {
  switch (error) {
    case Manifold::Error::NoError:
      return "No Error";
    case Manifold::Error::NonFiniteVertex:
      return "Non Finite Vertex";
    case Manifold::Error::NotManifold:
      return "Not Manifold";
    case Manifold::Error::VertexOutOfBounds:
      return "Vertex Out Of Bounds";
    case Manifold::Error::PropertiesWrongLength:
      return "Properties Wrong Length";
    case Manifold::Error::MissingPositionProperties:
      return "Missing Position Properties";
    case Manifold::Error::MergeVectorsDifferentLengths:
      return "Merge Vectors Different Lengths";
    case Manifold::Error::MergeIndexOutOfBounds:
      return "Merge Index Out Of Bounds";
    case Manifold::Error::TransformWrongLength:
      return "Transform Wrong Length";
    case Manifold::Error::RunIndexWrongLength:
      return "Run Index Wrong Length";
    case Manifold::Error::FaceIDWrongLength:
      return "Face ID Wrong Length";
    case Manifold::Error::InvalidConstruction:
      return "Invalid Construction";
    default:
      return "Unknown Error";
  };
}

inline std::ostream& operator<<(std::ostream& stream,
                                const Manifold::Error& error) {
  return stream << ToString(error);
}
#endif
/** @} */
}  // namespace manifold
