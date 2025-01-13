// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/TriangleShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/GetTrianglesContext.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CastConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CastSphereVsTriangles.h>
#include <Jolt/Physics/Collision/CollideConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CollideSphereVsTriangles.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVerticesVsTriangles.h>
#include <Jolt/Geometry/ConvexSupport.h>
#include <Jolt/Geometry/RayTriangle.h>
#include <Jolt/Geometry/ClosestPoint.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(TriangleShapeSettings)
{
	JPH_ADD_BASE_CLASS(TriangleShapeSettings, ConvexShapeSettings)

	JPH_ADD_ATTRIBUTE(TriangleShapeSettings, mV1)
	JPH_ADD_ATTRIBUTE(TriangleShapeSettings, mV2)
	JPH_ADD_ATTRIBUTE(TriangleShapeSettings, mV3)
	JPH_ADD_ATTRIBUTE(TriangleShapeSettings, mConvexRadius)
}

ShapeSettings::ShapeResult TriangleShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new TriangleShape(*this, mCachedResult);
	return mCachedResult;
}

TriangleShape::TriangleShape(const TriangleShapeSettings &inSettings, ShapeResult &outResult) :
	ConvexShape(EShapeSubType::Triangle, inSettings, outResult),
	mV1(inSettings.mV1),
	mV2(inSettings.mV2),
	mV3(inSettings.mV3),
	mConvexRadius(inSettings.mConvexRadius)
{
	if (inSettings.mConvexRadius < 0.0f)
	{
		outResult.SetError("Invalid convex radius");
		return;
	}

	outResult.Set(this);
}

AABox TriangleShape::GetLocalBounds() const
{
	AABox bounds(mV1, mV1);
	bounds.Encapsulate(mV2);
	bounds.Encapsulate(mV3);
	bounds.ExpandBy(Vec3::sReplicate(mConvexRadius));
	return bounds;
}

AABox TriangleShape::GetWorldSpaceBounds(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	JPH_ASSERT(IsValidScale(inScale));

	Vec3 v1 = inCenterOfMassTransform * (inScale * mV1);
	Vec3 v2 = inCenterOfMassTransform * (inScale * mV2);
	Vec3 v3 = inCenterOfMassTransform * (inScale * mV3);

	AABox bounds(v1, v1);
	bounds.Encapsulate(v2);
	bounds.Encapsulate(v3);
	bounds.ExpandBy(inScale * mConvexRadius);
	return bounds;
}

class TriangleShape::TriangleNoConvex final : public Support
{
public:
							TriangleNoConvex(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3) :
		mTriangleSuport(inV1, inV2, inV3)
	{
		static_assert(sizeof(TriangleNoConvex) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(TriangleNoConvex)));
	}

	virtual Vec3			GetSupport(Vec3Arg inDirection) const override
	{
		return mTriangleSuport.GetSupport(inDirection);
	}

	virtual float			GetConvexRadius() const override
	{
		return 0.0f;
	}

private:
	TriangleConvexSupport	mTriangleSuport;
};

class TriangleShape::TriangleWithConvex final : public Support
{
public:
							TriangleWithConvex(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3, float inConvexRadius) :
		mConvexRadius(inConvexRadius),
		mTriangleSuport(inV1, inV2, inV3)
	{
		static_assert(sizeof(TriangleWithConvex) <= sizeof(SupportBuffer), "Buffer size too small");
		JPH_ASSERT(IsAligned(this, alignof(TriangleWithConvex)));
	}

	virtual Vec3			GetSupport(Vec3Arg inDirection) const override
	{
		Vec3 support = mTriangleSuport.GetSupport(inDirection);
		float len = inDirection.Length();
		if (len > 0.0f)
			support += (mConvexRadius / len) * inDirection;
		return support;
	}

	virtual float			GetConvexRadius() const override
	{
		return mConvexRadius;
	}

private:
	float					mConvexRadius;
	TriangleConvexSupport	mTriangleSuport;
};

const ConvexShape::Support *TriangleShape::GetSupportFunction(ESupportMode inMode, SupportBuffer &inBuffer, Vec3Arg inScale) const
{
	switch (inMode)
	{
	case ESupportMode::IncludeConvexRadius:
	case ESupportMode::Default:
		if (mConvexRadius > 0.0f)
			return new (&inBuffer) TriangleWithConvex(inScale * mV1, inScale * mV2, inScale * mV3, mConvexRadius);
		[[fallthrough]];

	case ESupportMode::ExcludeConvexRadius:
		return new (&inBuffer) TriangleNoConvex(inScale * mV1, inScale * mV2, inScale * mV3);
	}

	JPH_ASSERT(false);
	return nullptr;
}

void TriangleShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");

	// Calculate transform with scale
	Mat44 transform = inCenterOfMassTransform.PreScaled(inScale);

	// Flip triangle if scaled inside out
	if (ScaleHelpers::IsInsideOut(inScale))
	{
		outVertices.push_back(transform * mV1);
		outVertices.push_back(transform * mV3);
		outVertices.push_back(transform * mV2);
	}
	else
	{
		outVertices.push_back(transform * mV1);
		outVertices.push_back(transform * mV2);
		outVertices.push_back(transform * mV3);
	}
}

MassProperties TriangleShape::GetMassProperties() const
{
	// We cannot calculate the volume for a triangle, so we return invalid mass properties.
	// If you want your triangle to be dynamic, then you should provide the mass properties yourself when
	// creating a Body:
	//
	// BodyCreationSettings::mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;
	// BodyCreationSettings::mMassPropertiesOverride.SetMassAndInertiaOfSolidBox(Vec3::sReplicate(1.0f), 1000.0f);
	//
	// Note that this makes the triangle shape behave the same as a mesh shape with a single triangle.
	// In practice there is very little use for a dynamic triangle shape as back side collisions will be ignored
	// so if the triangle falls the wrong way it will sink through the floor.
	return MassProperties();
}

Vec3 TriangleShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	JPH_ASSERT(inSubShapeID.IsEmpty(), "Invalid subshape ID");

	Vec3 cross = (mV2 - mV1).Cross(mV3 - mV1);
	float len = cross.Length();
	return len != 0.0f? cross / len : Vec3::sAxisY();
}

void TriangleShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const
{
	// A triangle has no volume
	outTotalVolume = outSubmergedVolume = 0.0f;
	outCenterOfBuoyancy = Vec3::sZero();
}

#ifdef JPH_DEBUG_RENDERER
void TriangleShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	RVec3 v1 = inCenterOfMassTransform * (inScale * mV1);
	RVec3 v2 = inCenterOfMassTransform * (inScale * mV2);
	RVec3 v3 = inCenterOfMassTransform * (inScale * mV3);

	if (ScaleHelpers::IsInsideOut(inScale))
		std::swap(v1, v2);

	if (inDrawWireframe)
		inRenderer->DrawWireTriangle(v1, v2, v3, inUseMaterialColors? GetMaterial()->GetDebugColor() : inColor);
	else
		inRenderer->DrawTriangle(v1, v2, v3, inUseMaterialColors? GetMaterial()->GetDebugColor() : inColor);
}
#endif // JPH_DEBUG_RENDERER

bool TriangleShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	float fraction = RayTriangle(inRay.mOrigin, inRay.mDirection, mV1, mV2, mV3);
	if (fraction < ioHit.mFraction)
	{
		ioHit.mFraction = fraction;
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}
	return false;
}

void TriangleShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// Back facing check
	if (inRayCastSettings.mBackFaceModeTriangles == EBackFaceMode::IgnoreBackFaces && (mV2 - mV1).Cross(mV3 - mV1).Dot(inRay.mDirection) > 0.0f)
		return;

	// Test ray against triangle
	float fraction = RayTriangle(inRay.mOrigin, inRay.mDirection, mV1, mV2, mV3);
	if (fraction < ioCollector.GetEarlyOutFraction())
	{
		// Better hit than the current hit
		RayCastResult hit;
		hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
		hit.mFraction = fraction;
		hit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		ioCollector.AddHit(hit);
	}
}

void TriangleShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Can't be inside a triangle
}

void TriangleShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	CollideSoftBodyVerticesVsTriangles collider(inCenterOfMassTransform, inScale);

	for (CollideSoftBodyVertexIterator v = inVertices, sbv_end = inVertices + inNumVertices; v != sbv_end; ++v)
		if (v.GetInvMass() > 0.0f)
		{
			collider.StartVertex(v);
			collider.ProcessTriangle(mV1, mV2, mV3);
			collider.FinishVertex(v, inCollidingShapeIndex);
		}
}

void TriangleShape::sCollideConvexVsTriangle(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape1->GetType() == EShapeType::Convex);
	const ConvexShape *shape1 = static_cast<const ConvexShape *>(inShape1);
	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::Triangle);
	const TriangleShape *shape2 = static_cast<const TriangleShape *>(inShape2);

	CollideConvexVsTriangles collider(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	collider.Collide(shape2->mV1, shape2->mV2, shape2->mV3, 0b111, inSubShapeIDCreator2.GetID());
}

void TriangleShape::sCollideSphereVsTriangle(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::Sphere);
	const SphereShape *shape1 = static_cast<const SphereShape *>(inShape1);
	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::Triangle);
	const TriangleShape *shape2 = static_cast<const TriangleShape *>(inShape2);

	CollideSphereVsTriangles collider(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	collider.Collide(shape2->mV1, shape2->mV2, shape2->mV3, 0b111, inSubShapeIDCreator2.GetID());
}

void TriangleShape::sCastConvexVsTriangle(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::Triangle);
	const TriangleShape *shape = static_cast<const TriangleShape *>(inShape);

	CastConvexVsTriangles caster(inShapeCast, inShapeCastSettings, inScale, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	caster.Cast(shape->mV1, shape->mV2, shape->mV3, 0b111, inSubShapeIDCreator2.GetID());
}

void TriangleShape::sCastSphereVsTriangle(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::Triangle);
	const TriangleShape *shape = static_cast<const TriangleShape *>(inShape);

	CastSphereVsTriangles caster(inShapeCast, inShapeCastSettings, inScale, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	caster.Cast(shape->mV1, shape->mV2, shape->mV3, 0b111, inSubShapeIDCreator2.GetID());
}

class TriangleShape::TSGetTrianglesContext
{
public:
					TSGetTrianglesContext(Vec3Arg inV1, Vec3Arg inV2, Vec3Arg inV3) : mV1(inV1), mV2(inV2), mV3(inV3) { }

	Vec3			mV1;
	Vec3			mV2;
	Vec3			mV3;

	bool			mIsDone = false;
};

void TriangleShape::GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	static_assert(sizeof(TSGetTrianglesContext) <= sizeof(GetTrianglesContext), "GetTrianglesContext too small");
	JPH_ASSERT(IsAligned(&ioContext, alignof(TSGetTrianglesContext)));

	Mat44 m = Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(inScale);

	new (&ioContext) TSGetTrianglesContext(m * mV1, m * mV2, m * mV3);
}

int TriangleShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	static_assert(cGetTrianglesMinTrianglesRequested >= 3, "cGetTrianglesMinTrianglesRequested is too small");
	JPH_ASSERT(inMaxTrianglesRequested >= cGetTrianglesMinTrianglesRequested);

	TSGetTrianglesContext &context = (TSGetTrianglesContext &)ioContext;

	// Only return the triangle the 1st time
	if (context.mIsDone)
		return 0;
	context.mIsDone = true;

	// Store triangle
	context.mV1.StoreFloat3(outTriangleVertices);
	context.mV2.StoreFloat3(outTriangleVertices + 1);
	context.mV3.StoreFloat3(outTriangleVertices + 2);

	// Store material
	if (outMaterials != nullptr)
		*outMaterials = GetMaterial();

	return 1;
}

void TriangleShape::SaveBinaryState(StreamOut &inStream) const
{
	ConvexShape::SaveBinaryState(inStream);

	inStream.Write(mV1);
	inStream.Write(mV2);
	inStream.Write(mV3);
	inStream.Write(mConvexRadius);
}

void TriangleShape::RestoreBinaryState(StreamIn &inStream)
{
	ConvexShape::RestoreBinaryState(inStream);

	inStream.Read(mV1);
	inStream.Read(mV2);
	inStream.Read(mV3);
	inStream.Read(mConvexRadius);
}

bool TriangleShape::IsValidScale(Vec3Arg inScale) const
{
	return ConvexShape::IsValidScale(inScale) && (mConvexRadius == 0.0f || ScaleHelpers::IsUniformScale(inScale.Abs()));
}

Vec3 TriangleShape::MakeScaleValid(Vec3Arg inScale) const
{
	Vec3 scale = ScaleHelpers::MakeNonZeroScale(inScale);

	if (mConvexRadius == 0.0f)
		return scale;

	return scale.GetSign() * ScaleHelpers::MakeUniformScale(scale.Abs());
}

void TriangleShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::Triangle);
	f.mConstruct = []() -> Shape * { return new TriangleShape; };
	f.mColor = Color::sGreen;

	for (EShapeSubType s : sConvexSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::Triangle, sCollideConvexVsTriangle);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::Triangle, sCastConvexVsTriangle);
	}

	// Specialized collision functions
	CollisionDispatch::sRegisterCollideShape(EShapeSubType::Sphere, EShapeSubType::Triangle, sCollideSphereVsTriangle);
	CollisionDispatch::sRegisterCastShape(EShapeSubType::Sphere, EShapeSubType::Triangle, sCastSphereVsTriangle);
}

JPH_NAMESPACE_END
