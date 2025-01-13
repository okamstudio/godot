// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Core/ByteBuffer.h>
#include <Jolt/Geometry/Triangle.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

class ConvexShape;
class CollideShapeSettings;

/// Class that constructs a MeshShape
class JPH_EXPORT MeshShapeSettings final : public ShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, MeshShapeSettings)

public:
	/// Default constructor for deserialization
									MeshShapeSettings() = default;

	/// Create a mesh shape.
									MeshShapeSettings(const TriangleList &inTriangles, PhysicsMaterialList inMaterials = PhysicsMaterialList());
									MeshShapeSettings(VertexList inVertices, IndexedTriangleList inTriangles, PhysicsMaterialList inMaterials = PhysicsMaterialList());

	/// Sanitize the mesh data. Remove duplicate and degenerate triangles. This is called automatically when constructing the MeshShapeSettings with a list of (indexed-) triangles.
	void							Sanitize();

	// See: ShapeSettings
	virtual ShapeResult				Create() const override;

	/// Vertices belonging to mIndexedTriangles
	VertexList						mTriangleVertices;

	/// Original list of indexed triangles (triangles will be reordered internally in the mesh shape).
	/// Triangles must be provided in counter clockwise order.
	/// Degenerate triangles will automatically be removed during mesh creation but no other mesh simplifications are performed, use an external library if this is desired.
	/// For simulation, the triangles are considered to be single sided.
	/// For ray casts you can choose to make triangles double sided by setting RayCastSettings::mBackFaceMode to EBackFaceMode::CollideWithBackFaces.
	/// For collide shape tests you can use CollideShapeSettings::mBackFaceMode and for shape casts you can use ShapeCastSettings::mBackFaceModeTriangles.
	IndexedTriangleList				mIndexedTriangles;

	/// Materials assigned to the triangles. Each triangle specifies which material it uses through its mMaterialIndex
	PhysicsMaterialList				mMaterials;

	/// Maximum number of triangles in each leaf of the axis aligned box tree. This is a balance between memory and performance. Can be in the range [1, MeshShape::MaxTrianglesPerLeaf].
	/// Sensible values are between 4 (for better performance) and 8 (for less memory usage).
	uint							mMaxTrianglesPerLeaf = 8;

	/// Cosine of the threshold angle (if the angle between the two triangles is bigger than this, the edge is active, note that a concave edge is always inactive).
	/// Setting this value too small can cause ghost collisions with edges, setting it too big can cause depenetration artifacts (objects not depenetrating quickly).
	/// Valid ranges are between cos(0 degrees) and cos(90 degrees). The default value is cos(5 degrees).
	float							mActiveEdgeCosThresholdAngle = 0.996195f;					// cos(5 degrees)

	/// When true, we store the user data coming from Triangle::mUserData or IndexedTriangle::mUserData in the mesh shape.
	/// This can be used to store additional data like the original index of the triangle in the mesh.
	/// Can be retrieved using MeshShape::GetTriangleUserData.
	/// Turning this on increases the memory used by the MeshShape by roughly 25%.
	bool							mPerTriangleUserData = false;
};

/// A mesh shape, consisting of triangles. Mesh shapes are mostly used for static geometry.
/// They can be used by dynamic or kinematic objects but only if they don't collide with other mesh or heightfield shapes as those collisions are currently not supported.
/// Note that if you make a mesh shape a dynamic or kinematic object, you need to provide a mass yourself as mesh shapes don't need to form a closed hull so don't have a well defined volume from which the mass can be calculated.
class JPH_EXPORT MeshShape final : public Shape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
									MeshShape() : Shape(EShapeType::Mesh, EShapeSubType::Mesh) { }
									MeshShape(const MeshShapeSettings &inSettings, ShapeResult &outResult);

	// See Shape::MustBeStatic
	virtual bool					MustBeStatic() const override								{ return true; }

	// See Shape::GetLocalBounds
	virtual AABox					GetLocalBounds() const override;

	// See Shape::GetSubShapeIDBitsRecursive
	virtual uint					GetSubShapeIDBitsRecursive() const override;

	// See Shape::GetInnerRadius
	virtual float					GetInnerRadius() const override								{ return 0.0f; }

	// See Shape::GetMassProperties
	virtual MassProperties			GetMassProperties() const override;

	// See Shape::GetMaterial
	virtual const PhysicsMaterial *	GetMaterial(const SubShapeID &inSubShapeID) const override;

	/// Get the list of all materials
	const PhysicsMaterialList &		GetMaterialList() const										{ return mMaterials; }

	/// Determine which material index a particular sub shape uses (note that if there are no materials this function will return 0 so check the array size)
	/// Note: This could for example be used to create a decorator shape around a mesh shape that overrides the GetMaterial call to replace a material with another material.
	uint							GetMaterialIndex(const SubShapeID &inSubShapeID) const;

	// See Shape::GetSurfaceNormal
	virtual Vec3					GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override;

	// See Shape::GetSupportingFace
	virtual void					GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;

#ifdef JPH_DEBUG_RENDERER
	// See Shape::Draw
	virtual void					Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;
#endif // JPH_DEBUG_RENDERER

	// See Shape::CastRay
	virtual bool					CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const override;
	virtual void					CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	/// See: Shape::CollidePoint
	/// Note that for CollidePoint to work for a mesh shape, the mesh needs to be closed (a manifold) or multiple non-intersecting manifolds. Triangles may be facing the interior of the manifold.
	/// Insideness is tested by counting the amount of triangles encountered when casting an infinite ray from inPoint. If the number of hits is odd we're inside, if it's even we're outside.
	virtual void					CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollideSoftBodyVertices
	virtual void					CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const override;

	// See Shape::GetTrianglesStart
	virtual void					GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const override;

	// See Shape::GetTrianglesNext
	virtual int						GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const override;

	// See Shape::GetSubmergedVolume
	virtual void					GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const override { JPH_ASSERT(false, "Not supported"); }

	// See Shape
	virtual void					SaveBinaryState(StreamOut &inStream) const override;
	virtual void					SaveMaterialState(PhysicsMaterialList &outMaterials) const override;
	virtual void					RestoreMaterialState(const PhysicsMaterialRefC *inMaterials, uint inNumMaterials) override;

	// See Shape::GetStats
	virtual Stats					GetStats() const override;

	// See Shape::GetVolume
	virtual float					GetVolume() const override									{ return 0; }

	// When MeshShape::mPerTriangleUserData is true, this function can be used to retrieve the user data that was stored in the mesh shape.
	uint32							GetTriangleUserData(const SubShapeID &inSubShapeID) const;

#ifdef JPH_DEBUG_RENDERER
	// Settings
	static bool						sDrawTriangleGroups;
	static bool						sDrawTriangleOutlines;
#endif // JPH_DEBUG_RENDERER

	// Register shape functions with the registry
	static void						sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void					RestoreBinaryState(StreamIn &inStream) override;

private:
	struct							MSGetTrianglesContext;										///< Context class for GetTrianglesStart/Next

	static constexpr int			NumTriangleBits = 3;										///< How many bits to reserve to encode the triangle index
	static constexpr int			MaxTrianglesPerLeaf = 1 << NumTriangleBits;					///< Number of triangles that are stored max per leaf aabb node

	/// Find and flag active edges
	static void						sFindActiveEdges(const MeshShapeSettings &inSettings, IndexedTriangleList &ioIndices);

	/// Visit the entire tree using a visitor pattern
	template <class Visitor>
	void							WalkTree(Visitor &ioVisitor) const;

	/// Same as above but with a callback per triangle instead of per block of triangles
	template <class Visitor>
	void							WalkTreePerTriangle(const SubShapeIDCreator &inSubShapeIDCreator2, Visitor &ioVisitor) const;

	/// Decode a sub shape ID
	inline void						DecodeSubShapeID(const SubShapeID &inSubShapeID, const void *&outTriangleBlock, uint32 &outTriangleIndex) const;

	// Helper functions called by CollisionDispatch
	static void						sCollideConvexVsMesh(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCollideSphereVsMesh(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCastConvexVsMesh(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);
	static void						sCastSphereVsMesh(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

	/// Materials assigned to the triangles. Each triangle specifies which material it uses through its mMaterialIndex
	PhysicsMaterialList				mMaterials;

	ByteBuffer						mTree;														///< Resulting packed data structure

	/// 8 bit flags stored per triangle
	enum ETriangleFlags
	{
		/// Material index
		FLAGS_MATERIAL_BITS			= 5,
		FLAGS_MATERIAL_MASK			= (1 << FLAGS_MATERIAL_BITS) - 1,

		/// Active edge bits
		FLAGS_ACTIVE_EGDE_SHIFT		= FLAGS_MATERIAL_BITS,
		FLAGS_ACTIVE_EDGE_BITS		= 3,
		FLAGS_ACTIVE_EDGE_MASK		= (1 << FLAGS_ACTIVE_EDGE_BITS) - 1
	};

#ifdef JPH_DEBUG_RENDERER
	mutable DebugRenderer::GeometryRef	mGeometry;												///< Debug rendering data
	mutable bool					mCachedTrianglesColoredPerGroup = false;					///< This is used to regenerate the triangle batch if the drawing settings change
	mutable bool					mCachedUseMaterialColors = false;							///< This is used to regenerate the triangle batch if the drawing settings change
#endif // JPH_DEBUG_RENDERER
};

JPH_NAMESPACE_END
