// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/Shape.h>

JPH_NAMESPACE_BEGIN

class SoftBodyMotionProperties;
class CollideShapeSettings;

/// Shape used exclusively for soft bodies. Adds the ability to perform collision checks against soft bodies.
class JPH_EXPORT SoftBodyShape final : public Shape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
									SoftBodyShape()											: Shape(EShapeType::SoftBody, EShapeSubType::SoftBody) { }

	/// Determine amount of bits needed to encode sub shape id
	uint							GetSubShapeIDBits() const;

	/// Convert a sub shape ID back to a face index
	uint32							GetFaceIndex(const SubShapeID &inSubShapeID) const;

	// See Shape
	virtual bool					MustBeStatic() const override							{ return false; }
	virtual Vec3					GetCenterOfMass() const override						{ return Vec3::sZero(); }
	virtual AABox					GetLocalBounds() const override;
	virtual uint					GetSubShapeIDBitsRecursive() const override				{ return GetSubShapeIDBits(); }
	virtual float					GetInnerRadius() const override							{ return 0.0f; }
	virtual MassProperties			GetMassProperties() const override						{ return MassProperties(); }
	virtual const PhysicsMaterial *	GetMaterial(const SubShapeID &inSubShapeID) const override;
	virtual Vec3					GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override;
	virtual void					GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;
	virtual void					GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy
#ifdef JPH_DEBUG_RENDERER // Not using JPH_IF_DEBUG_RENDERER for Doxygen
		, RVec3Arg inBaseOffset
#endif
		) const override;
#ifdef JPH_DEBUG_RENDERER
	virtual void					Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;
#endif // JPH_DEBUG_RENDERER
	virtual bool					CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const override;
	virtual void					CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;
	virtual void					CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;
	virtual void					CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const override;
	virtual void					GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const override;
	virtual int						GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const override;
	virtual Stats					GetStats() const override;
	virtual float					GetVolume() const override;

	// Register shape functions with the registry
	static void						sRegister();

private:
	// Helper functions called by CollisionDispatch
	static void						sCollideConvexVsSoftBody(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCollideSphereVsSoftBody(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter);
	static void						sCastConvexVsSoftBody(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);
	static void						sCastSphereVsSoftBody(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector);

	struct SBSGetTrianglesContext;

	friend class BodyManager;

	const SoftBodyMotionProperties *mSoftBodyMotionProperties;
};

JPH_NAMESPACE_END
