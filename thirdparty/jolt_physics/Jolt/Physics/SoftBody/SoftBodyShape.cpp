// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/SoftBody/SoftBodyShape.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Geometry/RayTriangle.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Collision/CastConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CastSphereVsTriangles.h>
#include <Jolt/Physics/Collision/CollideConvexVsTriangles.h>
#include <Jolt/Physics/Collision/CollideSphereVsTriangles.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

uint SoftBodyShape::GetSubShapeIDBits() const
{
	// Ensure we have enough bits to encode our shape [0, n - 1]
	uint32 n = (uint32)mSoftBodyMotionProperties->GetFaces().size() - 1;
	return 32 - CountLeadingZeros(n);
}

uint32 SoftBodyShape::GetFaceIndex(const SubShapeID &inSubShapeID) const
{
	SubShapeID remainder;
	uint32 face_index = inSubShapeID.PopID(GetSubShapeIDBits(), remainder);
	JPH_ASSERT(remainder.IsEmpty());
	return face_index;
}

AABox SoftBodyShape::GetLocalBounds() const
{
	return mSoftBodyMotionProperties->GetLocalBounds();
}

bool SoftBodyShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	JPH_PROFILE_FUNCTION();

	uint num_triangle_bits = GetSubShapeIDBits();
	uint triangle_idx = uint(-1);

	const Array<SoftBodyVertex> &vertices = mSoftBodyMotionProperties->GetVertices();
	for (const SoftBodyMotionProperties::Face &f : mSoftBodyMotionProperties->GetFaces())
	{
		Vec3 x1 = vertices[f.mVertex[0]].mPosition;
		Vec3 x2 = vertices[f.mVertex[1]].mPosition;
		Vec3 x3 = vertices[f.mVertex[2]].mPosition;

		float fraction = RayTriangle(inRay.mOrigin, inRay.mDirection, x1, x2, x3);
		if (fraction < ioHit.mFraction)
		{
			// Store fraction
			ioHit.mFraction = fraction;

			// Store triangle index
			triangle_idx = uint(&f - mSoftBodyMotionProperties->GetFaces().data());
		}
	}

	if (triangle_idx == uint(-1))
		return false;

	ioHit.mSubShapeID2 = inSubShapeIDCreator.PushID(triangle_idx, num_triangle_bits).GetID();
	return true;
}

void SoftBodyShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	uint num_triangle_bits = GetSubShapeIDBits();

	const Array<SoftBodyVertex> &vertices = mSoftBodyMotionProperties->GetVertices();
	for (const SoftBodyMotionProperties::Face &f : mSoftBodyMotionProperties->GetFaces())
	{
		Vec3 x1 = vertices[f.mVertex[0]].mPosition;
		Vec3 x2 = vertices[f.mVertex[1]].mPosition;
		Vec3 x3 = vertices[f.mVertex[2]].mPosition;

		// Back facing check
		if (inRayCastSettings.mBackFaceModeTriangles == EBackFaceMode::IgnoreBackFaces && (x2 - x1).Cross(x3 - x1).Dot(inRay.mDirection) > 0.0f)
			continue;

		// Test ray against triangle
		float fraction = RayTriangle(inRay.mOrigin, inRay.mDirection, x1, x2, x3);
		if (fraction < ioCollector.GetEarlyOutFraction())
		{
			// Better hit than the current hit
			RayCastResult hit;
			hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
			hit.mFraction = fraction;
			hit.mSubShapeID2 = inSubShapeIDCreator.PushID(uint(&f - mSoftBodyMotionProperties->GetFaces().data()), num_triangle_bits).GetID();
			ioCollector.AddHit(hit);
		}
	}
}

void SoftBodyShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	sCollidePointUsingRayCast(*this, inPoint, inSubShapeIDCreator, ioCollector, inShapeFilter);
}

void SoftBodyShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	/* Not implemented */
}

const PhysicsMaterial *SoftBodyShape::GetMaterial(const SubShapeID &inSubShapeID) const
{
	SubShapeID remainder;
	uint triangle_idx = inSubShapeID.PopID(GetSubShapeIDBits(), remainder);
	JPH_ASSERT(remainder.IsEmpty());

	const SoftBodyMotionProperties::Face &f = mSoftBodyMotionProperties->GetFace(triangle_idx);
	return mSoftBodyMotionProperties->GetMaterials()[f.mMaterialIndex];
}

Vec3 SoftBodyShape::GetSurfaceNormal(const SubShapeID &inSubShapeID, Vec3Arg inLocalSurfacePosition) const
{
	SubShapeID remainder;
	uint triangle_idx = inSubShapeID.PopID(GetSubShapeIDBits(), remainder);
	JPH_ASSERT(remainder.IsEmpty());

	const SoftBodyMotionProperties::Face &f = mSoftBodyMotionProperties->GetFace(triangle_idx);
	const Array<SoftBodyVertex> &vertices = mSoftBodyMotionProperties->GetVertices();

	Vec3 x1 = vertices[f.mVertex[0]].mPosition;
	Vec3 x2 = vertices[f.mVertex[1]].mPosition;
	Vec3 x3 = vertices[f.mVertex[2]].mPosition;

	return (x2 - x1).Cross(x3 - x1).NormalizedOr(Vec3::sAxisY());
}

void SoftBodyShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	SubShapeID remainder;
	uint triangle_idx = inSubShapeID.PopID(GetSubShapeIDBits(), remainder);
	JPH_ASSERT(remainder.IsEmpty());

	const SoftBodyMotionProperties::Face &f = mSoftBodyMotionProperties->GetFace(triangle_idx);
	const Array<SoftBodyVertex> &vertices = mSoftBodyMotionProperties->GetVertices();

	for (uint32 i : f.mVertex)
		outVertices.push_back(inCenterOfMassTransform * (inScale * vertices[i].mPosition));
}

void SoftBodyShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const
{
	outSubmergedVolume = 0.0f;
	outTotalVolume = mSoftBodyMotionProperties->GetVolume();
	outCenterOfBuoyancy = Vec3::sZero();
}

#ifdef JPH_DEBUG_RENDERER

void SoftBodyShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	const Array<SoftBodyVertex> &vertices = mSoftBodyMotionProperties->GetVertices();
	for (const SoftBodyMotionProperties::Face &f : mSoftBodyMotionProperties->GetFaces())
	{
		RVec3 x1 = inCenterOfMassTransform * vertices[f.mVertex[0]].mPosition;
		RVec3 x2 = inCenterOfMassTransform * vertices[f.mVertex[1]].mPosition;
		RVec3 x3 = inCenterOfMassTransform * vertices[f.mVertex[2]].mPosition;

		inRenderer->DrawTriangle(x1, x2, x3, inColor, DebugRenderer::ECastShadow::On);
	}
}

#endif // JPH_DEBUG_RENDERER

struct SoftBodyShape::SBSGetTrianglesContext
{
	Mat44		mCenterOfMassTransform;
	int			mTriangleIndex;
};

void SoftBodyShape::GetTrianglesStart(GetTrianglesContext &ioContext, [[maybe_unused]] const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	SBSGetTrianglesContext &context = reinterpret_cast<SBSGetTrianglesContext &>(ioContext);
	context.mCenterOfMassTransform = Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(inScale);
	context.mTriangleIndex = 0;
}

int SoftBodyShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	SBSGetTrianglesContext &context = reinterpret_cast<SBSGetTrianglesContext &>(ioContext);

	const Array<SoftBodyMotionProperties::Face> &faces = mSoftBodyMotionProperties->GetFaces();
	const Array<SoftBodyVertex> &vertices = mSoftBodyMotionProperties->GetVertices();
	const PhysicsMaterialList &materials = mSoftBodyMotionProperties->GetMaterials();

	int num_triangles = min(inMaxTrianglesRequested, (int)faces.size() - context.mTriangleIndex);
	for (int i = 0; i < num_triangles; ++i)
	{
		const SoftBodyMotionProperties::Face &f = faces[context.mTriangleIndex + i];

		Vec3 x1 = context.mCenterOfMassTransform * vertices[f.mVertex[0]].mPosition;
		Vec3 x2 = context.mCenterOfMassTransform * vertices[f.mVertex[1]].mPosition;
		Vec3 x3 = context.mCenterOfMassTransform * vertices[f.mVertex[2]].mPosition;

		x1.StoreFloat3(outTriangleVertices++);
		x2.StoreFloat3(outTriangleVertices++);
		x3.StoreFloat3(outTriangleVertices++);

		if (outMaterials != nullptr)
			*outMaterials++ = materials[f.mMaterialIndex];
	}

	context.mTriangleIndex += num_triangles;
	return num_triangles;
}

Shape::Stats SoftBodyShape::GetStats() const
{
	return Stats(sizeof(*this), (uint)mSoftBodyMotionProperties->GetFaces().size());
}

float SoftBodyShape::GetVolume() const
{
	return mSoftBodyMotionProperties->GetVolume();
}

void SoftBodyShape::sCollideConvexVsSoftBody(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape1->GetType() == EShapeType::Convex);
	const ConvexShape *shape1 = static_cast<const ConvexShape *>(inShape1);
	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::SoftBody);
	const SoftBodyShape *shape2 = static_cast<const SoftBodyShape *>(inShape2);

	const Array<SoftBodyVertex> &vertices = shape2->mSoftBodyMotionProperties->GetVertices();
	const Array<SoftBodyMotionProperties::Face> &faces = shape2->mSoftBodyMotionProperties->GetFaces();
	uint num_triangle_bits = shape2->GetSubShapeIDBits();

	CollideConvexVsTriangles collider(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	for (const SoftBodyMotionProperties::Face &f : faces)
	{
		Vec3 x1 = vertices[f.mVertex[0]].mPosition;
		Vec3 x2 = vertices[f.mVertex[1]].mPosition;
		Vec3 x3 = vertices[f.mVertex[2]].mPosition;

		collider.Collide(x1, x2, x3, 0b111, inSubShapeIDCreator2.PushID(uint(&f - faces.data()), num_triangle_bits).GetID());
	}
}

void SoftBodyShape::sCollideSphereVsSoftBody(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_ASSERT(inShape1->GetSubType() == EShapeSubType::Sphere);
	const SphereShape *shape1 = static_cast<const SphereShape *>(inShape1);
	JPH_ASSERT(inShape2->GetSubType() == EShapeSubType::SoftBody);
	const SoftBodyShape *shape2 = static_cast<const SoftBodyShape *>(inShape2);

	const Array<SoftBodyVertex> &vertices = shape2->mSoftBodyMotionProperties->GetVertices();
	const Array<SoftBodyMotionProperties::Face> &faces = shape2->mSoftBodyMotionProperties->GetFaces();
	uint num_triangle_bits = shape2->GetSubShapeIDBits();

	CollideSphereVsTriangles collider(shape1, inScale1, inScale2, inCenterOfMassTransform1, inCenterOfMassTransform2, inSubShapeIDCreator1.GetID(), inCollideShapeSettings, ioCollector);
	for (const SoftBodyMotionProperties::Face &f : faces)
	{
		Vec3 x1 = vertices[f.mVertex[0]].mPosition;
		Vec3 x2 = vertices[f.mVertex[1]].mPosition;
		Vec3 x3 = vertices[f.mVertex[2]].mPosition;

		collider.Collide(x1, x2, x3, 0b111, inSubShapeIDCreator2.PushID(uint(&f - faces.data()), num_triangle_bits).GetID());
	}
}

void SoftBodyShape::sCastConvexVsSoftBody(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::SoftBody);
	const SoftBodyShape *shape = static_cast<const SoftBodyShape *>(inShape);

	const Array<SoftBodyVertex> &vertices = shape->mSoftBodyMotionProperties->GetVertices();
	const Array<SoftBodyMotionProperties::Face> &faces = shape->mSoftBodyMotionProperties->GetFaces();
	uint num_triangle_bits = shape->GetSubShapeIDBits();

	CastConvexVsTriangles caster(inShapeCast, inShapeCastSettings, inScale, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	for (const SoftBodyMotionProperties::Face &f : faces)
	{
		Vec3 x1 = vertices[f.mVertex[0]].mPosition;
		Vec3 x2 = vertices[f.mVertex[1]].mPosition;
		Vec3 x3 = vertices[f.mVertex[2]].mPosition;

		caster.Cast(x1, x2, x3, 0b111, inSubShapeIDCreator2.PushID(uint(&f - faces.data()), num_triangle_bits).GetID());
	}
}

void SoftBodyShape::sCastSphereVsSoftBody(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_ASSERT(inShape->GetSubType() == EShapeSubType::SoftBody);
	const SoftBodyShape *shape = static_cast<const SoftBodyShape *>(inShape);

	const Array<SoftBodyVertex> &vertices = shape->mSoftBodyMotionProperties->GetVertices();
	const Array<SoftBodyMotionProperties::Face> &faces = shape->mSoftBodyMotionProperties->GetFaces();
	uint num_triangle_bits = shape->GetSubShapeIDBits();

	CastSphereVsTriangles caster(inShapeCast, inShapeCastSettings, inScale, inCenterOfMassTransform2, inSubShapeIDCreator1, ioCollector);
	for (const SoftBodyMotionProperties::Face &f : faces)
	{
		Vec3 x1 = vertices[f.mVertex[0]].mPosition;
		Vec3 x2 = vertices[f.mVertex[1]].mPosition;
		Vec3 x3 = vertices[f.mVertex[2]].mPosition;

		caster.Cast(x1, x2, x3, 0b111, inSubShapeIDCreator2.PushID(uint(&f - faces.data()), num_triangle_bits).GetID());
	}
}

void SoftBodyShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::SoftBody);
	f.mConstruct = nullptr; // Not supposed to be constructed by users!
	f.mColor = Color::sDarkGreen;

	for (EShapeSubType s : sConvexSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::SoftBody, sCollideConvexVsSoftBody);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::SoftBody, sCastConvexVsSoftBody);
	}

	// Specialized collision functions
	CollisionDispatch::sRegisterCollideShape(EShapeSubType::Sphere, EShapeSubType::SoftBody, sCollideSphereVsSoftBody);
	CollisionDispatch::sRegisterCastShape(EShapeSubType::Sphere, EShapeSubType::SoftBody, sCastSphereVsSoftBody);
}

JPH_NAMESPACE_END
