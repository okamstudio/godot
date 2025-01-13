// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

JPH_NAMESPACE_BEGIN

class CollideShapeSettings;

/// Class that constructs a PlaneShape
class JPH_EXPORT PlaneShapeSettings final : public ShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, PlaneShapeSettings)

public:
	/// Default constructor for deserialization
									PlaneShapeSettings() = default;

	/// Create a plane shape.
									PlaneShapeSettings(const Plane &inPlane, const PhysicsMaterial *inMaterial = nullptr, float inHalfExtent = cDefaultHalfExtent) : mPlane(inPlane), mMaterial(inMaterial), mHalfExtent(inHalfExtent) { }

	// See: ShapeSettings
	virtual ShapeResult				Create() const override;

	Plane							mPlane;														///< Plane that describes the shape. The negative half space is considered solid.

	RefConst<PhysicsMaterial>		mMaterial;													///< Surface material of the plane

	static constexpr float			cDefaultHalfExtent = 1000.0f;								///< Default half-extent of the plane (total size along 1 axis will be 2 * half-extent)

	float							mHalfExtent = cDefaultHalfExtent;							///< The bounding box of this plane will run from [-half_extent, half_extent]. Keep this as low as possible for better broad phase performance.
};

/// A plane shape. The negative half space is considered solid. Planes cannot be dynamic objects, only static or kinematic.
/// The plane is considered an infinite shape, but testing collision outside of its bounding box (defined by the half-extent parameter) will not return a collision result.
/// At the edge of the bounding box collision with the plane will be inconsistent. If you need something of a well defined size, a box shape may be better.
class JPH_EXPORT PlaneShape final : public Shape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
									PlaneShape() : Shape(EShapeType::Plane, EShapeSubType::Plane) { }
									PlaneShape(const Plane &inPlane, const PhysicsMaterial *inMaterial = nullptr, float inHalfExtent = PlaneShapeSettings::cDefaultHalfExtent) : Shape(EShapeType::Plane, EShapeSubType::Plane), mPlane(inPlane), mMaterial(inMaterial), mHalfExtent(inHalfExtent) { CalculateLocalBounds(); }
									PlaneShape(const PlaneShapeSettings &inSettings, ShapeResult &outResult);

	/// Get the plane
	const Plane &					GetPlane() const											{ return mPlane; }

	/// Get the half-extent of the bounding box of the plane
	float							GetHalfExtent() const										{ return mHalfExtent; }

	// See Shape::MustBeStatic
	virtual bool					MustBeStatic() const override								{ return true; }

	// See Shape::GetLocalBounds
	virtual AABox					GetLocalBounds() const override								{ return mLocalBounds; }

	// See Shape::GetSubShapeIDBitsRecursive
	virtual uint					GetSubShapeIDBitsRecursive() const override					{ return 0; }

	// See Shape::GetInnerRadius
	virtual float					GetInnerRadius() const override								{ return 0.0f; }

	// See Shape::GetMassProperties
	virtual MassProperties			GetMassProperties() const override;

	// See Shape::GetMaterial
	virtual const PhysicsMaterial *	GetMaterial(const SubShapeID &inSubShapeID) const override	{ JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID"); return mMaterial != nullptr? mMaterial : PhysicsMaterial::sDefault; }

	// See Shape::GetSurfaceNormal
	virtual Vec3					GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override { JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID"); return mPlane.GetNormal(); }

	// See Shape::GetSupportingFace
	virtual void					GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;

#ifdef JPH_DEBUG_RENDERER
	// See Shape::Draw
	virtual void					Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;
#endif // JPH_DEBUG_RENDERER

	// See Shape::CastRay
	virtual bool					CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const override;
	virtual void					CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollidePoint
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
	virtual Stats					GetStats() const override									{ return Stats(sizeof(*this), 0); }

	// See Shape::GetVolume
	virtual float					GetVolume() const override									{ return 0; }

	// Register shape functions with the registry
	static void						sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void					RestoreBinaryState(StreamIn &inStream) override;

private:
	struct							PSGetTrianglesContext;										///< Context class for GetTrianglesStart/Next

	// Get 4 vertices that form the plane
	void							GetVertices(Vec3 *outVertices) const;

	// Cache the local bounds
	void							CalculateLocalBounds();

	// Helper functions called by CollisionDispatch
	static void						sCollideConvexVsPlane(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCastConvexVsPlane(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

	Plane							mPlane;
	RefConst<PhysicsMaterial>		mMaterial;
	float							mHalfExtent;
	AABox							mLocalBounds;
};

JPH_NAMESPACE_END
