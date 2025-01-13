// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(OffsetCenterOfMassShapeSettings)
{
	JPH_ADD_BASE_CLASS(OffsetCenterOfMassShapeSettings, DecoratedShapeSettings)

	JPH_ADD_ATTRIBUTE(OffsetCenterOfMassShapeSettings, mOffset)
}

ShapeSettings::ShapeResult OffsetCenterOfMassShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new OffsetCenterOfMassShape(*this, mCachedResult);
	return mCachedResult;
}

OffsetCenterOfMassShape::OffsetCenterOfMassShape(const OffsetCenterOfMassShapeSettings &inSettings, ShapeResult &outResult) :
	DecoratedShape(EShapeSubType::OffsetCenterOfMass, inSettings, outResult),
	mOffset(inSettings.mOffset)
{
	if (outResult.HasError())
		return;

	outResult.Set(this);
}

AABox OffsetCenterOfMassShape::GetLocalBounds() const
{
	AABox bounds = mInnerShape->GetLocalBounds();
	bounds.mMin -= mOffset;
	bounds.mMax -= mOffset;
	return bounds;
}

AABox OffsetCenterOfMassShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	return mInnerShape->GetWorldSpaceBounds(inCenterOfMassTransform.PreTranslated(-inScale * mOffset), inScale);
}

TransformedShape OffsetCenterOfMassShape::GetSubShapeTransformedShape(const SubShapeID &inSubShapeID, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, SubShapeID &outRemainder) const
{
	// We don't use any bits in the sub shape ID
	outRemainder = inSubShapeID;

	TransformedShape ts(RVec3(inPositionCOM - inRotation * (inScale * mOffset)), inRotation, mInnerShape, BodyID());
	ts.SetShapeScale(inScale);
	return ts;
}

Vec3 OffsetCenterOfMassShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	// Transform surface position to local space and pass call on
	return mInnerShape->GetSurfaceNormal(inSubShapeID, inLocalSurfacePosition + mOffset);
}

void OffsetCenterOfMassShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	mInnerShape->GetSupportingFace(inSubShapeID, inDirection, inScale, inCenterOfMassTransform.PreTranslated(-inScale * mOffset), outVertices);
}

void OffsetCenterOfMassShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const
{
	mInnerShape->GetSubmergedVolume(inCenterOfMassTransform.PreTranslated(-inScale * mOffset), inScale, inSurface, outTotalVolume, outSubmergedVolume, outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, inBaseOffset));
}

#ifdef JPH_DEBUG_RENDERER
void OffsetCenterOfMassShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	mInnerShape->Draw(inRenderer, inCenterOfMassTransform.PreTranslated(-inScale * mOffset), inScale, inColor, inUseMaterialColors, inDrawWireframe);
}

void OffsetCenterOfMassShape::DrawGetSupportFunction(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inDrawSupportDirection) const
{
	mInnerShape->DrawGetSupportFunction(inRenderer, inCenterOfMassTransform.PreTranslated(-inScale * mOffset), inScale, inColor, inDrawSupportDirection);
}

void OffsetCenterOfMassShape::DrawGetSupportingFace(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	mInnerShape->DrawGetSupportingFace(inRenderer, inCenterOfMassTransform.PreTranslated(-inScale * mOffset), inScale);
}
#endif // JPH_DEBUG_RENDERER

bool OffsetCenterOfMassShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	// Transform the ray to local space
	RayCast ray = inRay;
	ray.mOrigin += mOffset;

	return mInnerShape->CastRay(ray, inSubShapeIDCreator, ioHit);
}

void OffsetCenterOfMassShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// Transform the ray to local space
	RayCast ray = inRay;
	ray.mOrigin += mOffset;

	return mInnerShape->CastRay(ray, inRayCastSettings, inSubShapeIDCreator, ioCollector, inShapeFilter);
}

void OffsetCenterOfMassShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// Pass the point on to the inner shape in local space
	mInnerShape->CollidePoint(inPoint + mOffset, inSubShapeIDCreator, ioCollector, inShapeFilter);
}

void OffsetCenterOfMassShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	mInnerShape->CollideSoftBodyVertices(inCenterOfMassTransform.PreTranslated(-inScale * mOffset), inScale, inVertices, inNumVertices, inCollidingShapeIndex);
}

void OffsetCenterOfMassShape::CollectTransformedShapes(const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale, const SubShapeIDCreator &inSubShapeIDCreator, TransformedShapeCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	mInnerShape->CollectTransformedShapes(inBox, inPositionCOM - inRotation * (inScale * mOffset), inRotation, inScale, inSubShapeIDCreator, ioCollector, inShapeFilter);
}

void OffsetCenterOfMassShape::TransformShape(Mat44Arg inCenterOfMassTransform, TransformedShapeCollector &ioCollector) const
{
	mInnerShape->TransformShape(inCenterOfMassTransform.PreTranslated(-mOffset), ioCollector);
}

void OffsetCenterOfMassShape::sCollideOffsetCenterOfMassVsShape(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::OffsetCenterOfMass);
	const OffsetCenterOfMassShape *shape1 = static_cast<const OffsetCenterOfMassShape *>(inShape1);

	CollisionDispatch::sCollideShapeVsShape(shape1->mInnerShape, inShape2, inScale1, inScale2, inCenterOfMassTransform1.PreTranslated(-inScale1 * shape1->mOffset), inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
}

void OffsetCenterOfMassShape::sCollideShapeVsOffsetCenterOfMass(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::OffsetCenterOfMass);
	const OffsetCenterOfMassShape *shape2 = static_cast<const OffsetCenterOfMassShape *>(inShape2);

	CollisionDispatch::sCollideShapeVsShape(inShape1, shape2->mInnerShape, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2.PreTranslated(-inScale2 * shape2->mOffset), inSubShapeIDCreator1, inSubShapeIDCreator2, inCollideShapeSettings, ioCollector, inShapeFilter);
}

void OffsetCenterOfMassShape::sCastOffsetCenterOfMassVsShape(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	// Fetch offset center of mass shape from cast shape
	JPH_ASSERT(inShapeCast.mShape->GetSubType() == EShapeSubType::OffsetCenterOfMass);
	const OffsetCenterOfMassShape *shape1 = static_cast<const OffsetCenterOfMassShape *>(inShapeCast.mShape);

	// Transform the shape cast and update the shape
	ShapeCast shape_cast(shape1->mInnerShape, inShapeCast.mScale, inShapeCast.mCenterOfMassStart.PreTranslated(-inShapeCast.mScale * shape1->mOffset), inShapeCast.mDirection);

	CollisionDispatch::sCastShapeVsShapeLocalSpace(shape_cast, inShapeCastSettings, inShape, inScale, inShapeFilter, inCenterOfMassTransform2, inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
}

void OffsetCenterOfMassShape::sCastShapeVsOffsetCenterOfMass(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::OffsetCenterOfMass);
	const OffsetCenterOfMassShape *shape = static_cast<const OffsetCenterOfMassShape *>(inShape);

	// Transform the shape cast
	ShapeCast shape_cast = inShapeCast.PostTransformed(Mat44::sTranslation(inScale * shape->mOffset));

	CollisionDispatch::sCastShapeVsShapeLocalSpace(shape_cast, inShapeCastSettings, shape->mInnerShape, inScale, inShapeFilter, inCenterOfMassTransform2.PreTranslated(-inScale * shape->mOffset), inSubShapeIDCreator1, inSubShapeIDCreator2, ioCollector);
}

void OffsetCenterOfMassShape::SaveBinaryState(StreamOut &inStream) const
{
	DecoratedShape::SaveBinaryState(inStream);

	inStream.Write(mOffset);
}

void OffsetCenterOfMassShape::RestoreBinaryState(StreamIn &inStream)
{
	DecoratedShape::RestoreBinaryState(inStream);

	inStream.Read(mOffset);
}

void OffsetCenterOfMassShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::OffsetCenterOfMass);
	f.mConstruct = []() -> Shape * { return new OffsetCenterOfMassShape; };
	f.mColor = Color::sCyan;

	for (EShapeSubType s : sAllSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(EShapeSubType::OffsetCenterOfMass, s, sCollideOffsetCenterOfMassVsShape);
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::OffsetCenterOfMass, sCollideShapeVsOffsetCenterOfMass);
		CollisionDispatch::sRegisterCastShape(EShapeSubType::OffsetCenterOfMass, s, sCastOffsetCenterOfMassVsShape);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::OffsetCenterOfMass, sCastShapeVsOffsetCenterOfMass);
	}
}

JPH_NAMESPACE_END
