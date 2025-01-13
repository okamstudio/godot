// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/PhysicsSettings.h>

JPH_NAMESPACE_BEGIN

/// Class that constructs a TaperedCylinderShape
class JPH_EXPORT TaperedCylinderShapeSettings final : public ConvexShapeSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, TaperedCylinderShapeSettings)

public:
	/// Default constructor for deserialization
							TaperedCylinderShapeSettings() = default;

	/// Create a tapered cylinder centered around the origin with bottom at (0, -inHalfHeightOfTaperedCylinder, 0) with radius inBottomRadius and top at (0, inHalfHeightOfTaperedCylinder, 0) with radius inTopRadius
							TaperedCylinderShapeSettings(float inHalfHeightOfTaperedCylinder, float inTopRadius, float inBottomRadius, float inConvexRadius = cDefaultConvexRadius, const PhysicsMaterial *inMaterial = nullptr);

	// See: ShapeSettings
	virtual ShapeResult		Create() const override;

	float					mHalfHeight = 0.0f;
	float					mTopRadius = 0.0f;
	float					mBottomRadius = 0.0f;
	float					mConvexRadius = 0.0f;
};

/// A cylinder with different top and bottom radii
class JPH_EXPORT TaperedCylinderShape final : public ConvexShape
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
							TaperedCylinderShape() : ConvexShape(EShapeSubType::TaperedCylinder) { }
							TaperedCylinderShape(const TaperedCylinderShapeSettings &inSettings, ShapeResult &outResult);

	/// Get top radius of the tapered cylinder
	inline float			GetTopRadius() const													{ return mTopRadius; }

	/// Get bottom radius of the tapered cylinder
	inline float			GetBottomRadius() const													{ return mBottomRadius; }

	/// Get convex radius of the tapered cylinder
	inline float			GetConvexRadius() const													{ return mConvexRadius; }

	/// Get half height of the tapered cylinder
	inline float			GetHalfHeight() const													{ return 0.5f * (mTop - mBottom); }

	// See Shape::GetCenterOfMass
	virtual Vec3			GetCenterOfMass() const override										{ return Vec3(0, -0.5f * (mTop + mBottom), 0); }

	// See Shape::GetLocalBounds
	virtual AABox			GetLocalBounds() const override;

	// See Shape::GetInnerRadius
	virtual float			GetInnerRadius() const override											{ return min(mTopRadius, mBottomRadius); }

	// See Shape::GetMassProperties
	virtual MassProperties	GetMassProperties() const override;

	// See Shape::GetSurfaceNormal
	virtual Vec3			GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const override;

	// See Shape::GetSupportingFace
	virtual void			GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const override;

	// See ConvexShape::GetSupportFunction
	virtual const Support *	GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const override;

	// See: Shape::CollidePoint
	virtual void			CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter = { }) const override;

	// See: Shape::CollideSoftBodyVertices
	virtual void			CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const override;

	// See Shape::GetTrianglesStart
	virtual void			GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const override;

	// See Shape::GetTrianglesNext
	virtual int				GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials = nullptr) const override;

#ifdef JPH_DEBUG_RENDERER
	// See Shape::Draw
	virtual void			Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const override;
#endif // JPH_DEBUG_RENDERER

	// See Shape
	virtual void			SaveBinaryState(StreamOut &inStream) const override;

	// See Shape::GetStats
	virtual Stats			GetStats() const override												{ return Stats(sizeof(*this), 0); }

	// See Shape::GetVolume
	virtual float			GetVolume() const override;

	// See Shape::IsValidScale
	virtual bool			IsValidScale(Vec3Arg inScale) const override;

	// See Shape::MakeScaleValid
	virtual Vec3			MakeScaleValid(Vec3Arg inScale) const override;

	// Register shape functions with the registry
	static void				sRegister();

protected:
	// See: Shape::RestoreBinaryState
	virtual void			RestoreBinaryState(StreamIn &inStream) override;

private:
	// Class for GetSupportFunction
	class					TaperedCylinder;

	// Class for GetTrianglesTart
	class					TCSGetTrianglesContext;

	// Scale the cylinder
	JPH_INLINE void			GetScaled(Vec3Arg inScale, float &outTop, float &outBottom, float &outTopRadius, float &outBottomRadius, float &outConvexRadius) const;

	float					mTop = 0.0f;
	float					mBottom = 0.0f;
	float					mTopRadius = 0.0f;
	float					mBottomRadius = 0.0f;
	float					mConvexRadius = 0.0f;
};

JPH_NAMESPACE_END
