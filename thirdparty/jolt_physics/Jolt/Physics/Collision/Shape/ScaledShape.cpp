// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(ScaledShapeSettings)
{
	JPH_ADD_BASE_CLASS(ScaledShapeSettings, DecoratedShapeSettings)

	JPH_ADD_ATTRIBUTE(ScaledShapeSettings, mScale)
}

ShapeSettings::ShapeResult ScaledShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new ScaledShape(*this, mCachedResult);
	return mCachedResult;
}

ScaledShape::ScaledShape(const ScaledShapeSettings &inSettings, ShapeResult &outResult) :
	DecoratedShape(EShapeSubType::Scaled, inSettings, outResult),
	mScale(inSettings.mScale)
{
	if (outResult.HasError())
		return;

	if (ScaleHelpers::IsZeroScale(inSettings.mScale))
	{
		outResult.SetError("Can't use zero scale!");
		return;
	}

	outResult.Set(this);
}

MassProperties ScaledShape::GetMassProperties() const
{
	MassProperties p = mInnerShape->GetMassProperties();
	p.Scale(mScale);
	return p;
}

AABox ScaledShape::GetLocalBounds() const
{
	return mInnerShape->GetLocalBounds().Scaled(mScale);
}

AABox ScaledShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	return mInnerShape->GetWorldSpaceBounds(inCenterOfMassTransform, inScale * mScale);
}

TransformedShape ScaledShape::GetSubShapeTransformedShape(const SubShapeID &inSubShapeID, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, SubShapeID &outRemainder) const
{
	// We don't use any bits in the sub shape ID
	outRemainder = inSubShapeID;

	TransformedShape ts(RVec3(inPositionCOM), inRotation, mInnerShape, BodyID());
	ts.SetShapeScale(inScale * mScale);
	return ts;
}

Vec3 ScaledShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	// Transform the surface point to local space and pass the query on
	Vec3 normal = mInnerShape->GetSurfaceNormal(inSubShapeID, inLocalSurfacePosition / mScale);

	// Need to transform the plane normals using inScale
	// Transforming a direction with matrix M is done through multiplying by (M^-1)^T
	// In this case M is a diagonal matrix with the scale vector, so we need to multiply our normal by 1 / scale and renormalize afterwards
	return (normal / mScale).Normalized();
}

void ScaledShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	mInnerShape->GetSupportingFace(inSubShapeID, inDirection, inScale * mScale, inCenterOfMassTransform, outVertices);
}

void ScaledShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const
{
	mInnerShape->GetSubmergedVolume(inCenterOfMassTransform, inScale * mScale, inSurface, outTotalVolume, outSubmergedVolume, outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, inBaseOffset));
}

#ifdef JPH_DEBUG_RENDERER
void ScaledShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	mInnerShape->Draw(inRenderer, inCenterOfMassTransform, inScale * mScale, inColor, inUseMaterialColors, inDrawWireframe);
}

void ScaledShape::DrawGetSupportFunction(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inDrawSupportDirection) const
{
	mInnerShape->DrawGetSupportFunction(inRenderer, inCenterOfMassTransform, inScale * mScale, inColor, inDrawSupportDirection);
}

void ScaledShape::DrawGetSupportingFace(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	mInnerShape->DrawGetSupportingFace(inRenderer, inCenterOfMassTransform, inScale * mScale);
}
#endif // JPH_DEBUG_RENDERER

bool ScaledShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	Vec3 inv_scale = mScale.Reciprocal();
	RayCast scaled_ray { inv_scale * inRay.mOrigin, inv_scale * inRay.mDirection };
	return mInnerShape->CastRay(scaled_ray, inSubShapeIDCreator, ioHit);
}

void ScaledShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	Vec3 inv_scale = mScale.Reciprocal();
	RayCast scaled_ray { inv_scale * inRay.mOrigin, inv_scale * inRay.mDirection };
	return mInnerShape->CastRay(scaled_ray, inRayCastSettings, inSubShapeIDCreator, ioCollector, inShapeFilter);
}

void ScaledShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	Vec3 inv_scale = mScale.Reciprocal();
	mInnerShape->CollidePoint(inv_scale * inPoint, inSubShapeIDCreator, ioCollector, inShapeFilter);
}

void ScaledShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	mInnerShape->CollideSoftBodyVertices(inCenterOfMassTransform, inScale * mScale, inVertices, inNumVertices, inCollidingShapeIndex);
}

void ScaledShape::CollectTransformedShapes(const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	mInnerShape->CollectTransformedShapes(inBox, inPositionCOM, inRotation, inScale * mScale, inSubShapeIDCreator, ioCollector, inShapeFilter);
}

void ScaledShape::TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const
{
	mInnerShape->TransformShape(inCenterOfMassTransform * Mat44::sScale(mScale), ioCollector);
}

void ScaledShape::SaveBinaryState(StreamOut &inStream) const
{
	DecoratedShape::SaveBinaryState(inStream);

	inStream.Write(mScale);
}

void ScaledShape::RestoreBinaryState(StreamIn &inStream)
{
	DecoratedShape::RestoreBinaryState(inStream);

	inStream.Read(mScale);
}

float ScaledShape::GetVolume() const
{
	return abs(mScale.GetX() * mScale.GetY() * mScale.GetZ()) * mInnerShape->GetVolume();
}

bool ScaledShape::IsValidScale(Vec3Arg inScale) const
{
	return mInnerShape->IsValidScale(inScale * mScale);
}

Vec3 ScaledShape::MakeScaleValid(Vec3Arg inScale) const
{
	return mInnerShape->MakeScaleValid(mScale * inScale) / mScale;
}

void ScaledShape::sCollideScaledVsShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::Scaled);
	const ScaledShape *shape1 = static_cast<const ScaledShape *>(inShape1);

	CollisionDispatch::sCollideShapeVsShape(shape1->GetInnerShape(), inShape2, inScale1 * shape1->GetScale(), inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
}

void ScaledShape::sCollideShapeVsScaled(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::Scaled);
	const ScaledShape *shape2 = static_cast<const ScaledShape *>(inShape2);

	CollisionDispatch::sCollideShapeVsShape(inShape1, shape2->GetInnerShape(), inScale1, inScale2 * shape2->GetScale(), inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
}

void ScaledShape::sCastScaledVsShape(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShapeCast.mShape->GetSubType() == EShapeSubType::Scaled);
	const ScaledShape *shape = static_cast<const ScaledShape *>(inShapeCast.mShape);

	ShapeCast scaled_cast(shape->GetInnerShape(), inShapeCast.mScale * shape->GetScale(), inShapeCast.mCenterOfMassStart, inShapeCast.mDirection);
	CollisionDispatch::sCastShapeVsShapeLocalSpace(scaled_cast, inShapeCastSettings, inShape, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
}

void ScaledShape::sCastShapeVsScaled(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::Scaled);
	const ScaledShape *shape = static_cast<const ScaledShape *>(inShape);

	CollisionDispatch::sCastShapeVsShapeLocalSpace(inShapeCast, inShapeCastSettings, shape->mInnerShape, inScale * shape->mScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
}

void ScaledShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::Scaled);
	f.mConstruct = []() -> Shape * { return new ScaledShape; };
	f.mColor = Color::sYellow;

	for (EShapeSubType s : sAllSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(EShapeSubType::Scaled, s, sCollideScaledVsShape);
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::Scaled, sCollideShapeVsScaled);
		CollisionDispatch::sRegisterCastShape(EShapeSubType::Scaled, s, sCastScaledVsShape);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::Scaled, sCastShapeVsScaled);
	}
}

JPH_NAMESPACE_END
