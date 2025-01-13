// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/PhysicsSettings.h>

JPH_NAMESPACE_BEGIN

/// Class that constructs a BoxShape
class JPH_EXPORT BoxShapeSettings final : public ConvexShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, BoxShapeSettings)

public:
	/// Default constructor for deserialization
							BoxShapeSettings() = default;

	/// Create a box with half edge length inHalfExtent and convex radius inConvexRadius.
	/// (internally the convex radius will be subtracted from the half extent so the total box will not grow with the convex radius).
							BoxShapeSettings(Vec3Arg inHalfExtent, float inConvexRadius = cDefaultConvexRadius, const PhysicsMaterial *inMaterial = nullptr) : ConvexShapeSettings(inMaterial), mHalfExtent(inHalfExtent), mConvexRadius(inConvexRadius) { }

	// See: ShapeSettings
	virtual ShapeResult		Create() const override;

	Vec3					mHalfExtent = Vec3::sZero();								///< Half the size of the box (including convex radius)
	float					mConvexRadius = 0.0f;
};

/// A box, centered around the origin
class JPH_EXPORT BoxShape final : public ConvexShape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
							BoxShape() : ConvexShape(EShapeSubType::Box) { }
							BoxShape(const BoxShapeSettings &inSettings, ShapeResult &outResult);

	/// Create a box with half edge length inHalfExtent and convex radius inConvexRadius.
	/// (internally the convex radius will be subtracted from the half extent so the total box will not grow with the convex radius).
							BoxShape(Vec3Arg inHalfExtent, float inConvexRadius = cDefaultConvexRadius, const PhysicsMaterial *inMaterial = nullptr) : ConvexShape(EShapeSubType::Box, inMaterial), mHalfExtent(inHalfExtent), mConvexRadius(inConvexRadius) { JPH_ASSERT(inConvexRadius >= 0.0f); JPH_ASSERT(inHalfExtent.ReduceMin() >= inConvexRadius); }

	/// Get half extent of box
	Vec3					GetHalfExtent() const										{ return mHalfExtent; }

	// See Shape::GetLocalBounds
	virtual AABox			GetLocalBounds() const override								{ return AABox(-mHalfExtent, mHalfExtent); }

	// See Shape::GetInnerRadius
	virtual float			GetInnerRadius() const override								{ return mHalfExtent.ReduceMin(); }

	// See Shape::GetMassProperties
	virtual MassProperties	GetMassProperties() const override;

	// See Shape::GetSurfaceNormal
	virtual Vec3			GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override;

	// See Shape::GetSupportingFace
	virtual void			GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;

	// See ConvexShape::GetSupportFunction
	virtual const Support *	GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const override;

#ifdef JPH_DEBUG_RENDERER
	// See Shape::Draw
	virtual void			Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;
#endif // JPH_DEBUG_RENDERER

	// See Shape::CastRay
	virtual bool			CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const override;
	virtual void			CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollidePoint
	virtual void			CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollideSoftBodyVertices
	virtual void			CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const override;

	// See Shape::GetTrianglesStart
	virtual void			GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const override;

	// See Shape::GetTrianglesNext
	virtual int				GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const override;

	// See Shape
	virtual void			SaveBinaryState(StreamOut &inStream) const override;

	// See Shape::GetStats
	virtual Stats			GetStats() const override									{ return Stats(sizeof(*this), 12); }

	// See Shape::GetVolume
	virtual float			GetVolume() const override									{ return GetLocalBounds().GetVolume(); }

	/// Get the convex radius of this box
	float					GetConvexRadius() const										{ return mConvexRadius; }

	// Register shape functions with the registry
	static void				sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void			RestoreBinaryState(StreamIn &inStream) override;

private:
	// Class for GetSupportFunction
	class					Box;

	Vec3					mHalfExtent = Vec3::sZero();								///< Half the size of the box (including convex radius)
	float					mConvexRadius = 0.0f;
};

JPH_NAMESPACE_END
