// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2024 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/CollideSoftBodyVertexIterator.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Geometry/Plane.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(PlaneShapeSettings)
{
	JPH_ADD_BASE_CLASS(PlaneShapeSettings, ShapeSettings)

	JPH_ADD_ATTRIBUTE(PlaneShapeSettings, mPlane)
	JPH_ADD_ATTRIBUTE(PlaneShapeSettings, mMaterial)
	JPH_ADD_ATTRIBUTE(PlaneShapeSettings, mHalfExtent)
}

ShapeSettings::ShapeResult PlaneShapeSettings::Create() const
{
	if (mCachedResult.IsEmpty())
		Ref<Shape> shape = new PlaneShape(*this, mCachedResult);
	return mCachedResult;
}

inline static void sPlaneGetOrthogonalBasis(Vec3Arg inNormal, Vec3 &outPerp1, Vec3 &outPerp2)
{
	outPerp1 = inNormal.Cross(Vec3::sAxisY()).NormalizedOr(Vec3::sAxisX());
	outPerp2 = outPerp1.Cross(inNormal).Normalized();
	outPerp1 = inNormal.Cross(outPerp2);
}

void PlaneShape::GetVertices(Vec3 *outVertices) const
{
	// Create orthogonal basis
	Vec3 normal = mPlane.GetNormal();
	Vec3 perp1, perp2;
	sPlaneGetOrthogonalBasis(normal, perp1, perp2);

	// Scale basis
	perp1 *= mHalfExtent;
	perp2 *= mHalfExtent;

	// Calculate corners
	Vec3 point = -normal * mPlane.GetConstant();
	outVertices[0] = point + perp1 + perp2;
	outVertices[1] = point + perp1 - perp2;
	outVertices[2] = point - perp1 - perp2;
	outVertices[3] = point - perp1 + perp2;
}

void PlaneShape::CalculateLocalBounds()
{
	// Get the vertices of the plane
	Vec3 vertices[4];
	GetVertices(vertices);

	// Encapsulate the vertices and a point mHalfExtent behind the plane
	mLocalBounds = AABox();
	Vec3 normal = mPlane.GetNormal();
	for (const Vec3 &v : vertices)
	{
		mLocalBounds.Encapsulate(v);
		mLocalBounds.Encapsulate(v - mHalfExtent * normal);
	}
}

PlaneShape::PlaneShape(const PlaneShapeSettings &inSettings, ShapeResult &outResult) :
	Shape(EShapeType::Plane, EShapeSubType::Plane, inSettings, outResult),
	mPlane(inSettings.mPlane),
	mMaterial(inSettings.mMaterial),
	mHalfExtent(inSettings.mHalfExtent)
{
	if (!mPlane.GetNormal().IsNormalized())
	{
		outResult.SetError("Plane normal needs to be normalized!");
		return;
	}

	CalculateLocalBounds();

	outResult.Set(this);
}

MassProperties PlaneShape::GetMassProperties() const
{
	// Object should always be static, return default mass properties
	return MassProperties();
}

void PlaneShape::GetSupportingFace(const SubShapeID &inSubShapeID, Vec3Arg inDirection, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform, SupportingFace &outVertices) const
{
	// Get the vertices of the plane
	Vec3 vertices[4];
	GetVertices(vertices);

	// Reverse if scale is inside out
	if (ScaleHelpers::IsInsideOut(inScale))
	{
		std::swap(vertices[0], vertices[3]);
		std::swap(vertices[1], vertices[2]);
	}

	// Transform them to world space
	outVertices.clear();
	Mat44 com = inCenterOfMassTransform.PreScaled(inScale);
	for (const Vec3 &v : vertices)
		outVertices.push_back(com * v);
}

#ifdef JPH_DEBUG_RENDERER
void PlaneShape::Draw(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inUseMaterialColors, bool inDrawWireframe) const
{
	// Get the vertices of the plane
	Vec3 local_vertices[4];
	GetVertices(local_vertices);

	// Reverse if scale is inside out
	if (ScaleHelpers::IsInsideOut(inScale))
	{
		std::swap(local_vertices[0], local_vertices[3]);
		std::swap(local_vertices[1], local_vertices[2]);
	}

	// Transform them to world space
	RMat44 com = inCenterOfMassTransform.PreScaled(inScale);
	RVec3 vertices[4];
	for (uint i = 0; i < 4; ++i)
		vertices[i] = com * local_vertices[i];

	// Determine the color
	Color color = inUseMaterialColors? GetMaterial(SubShapeID())->GetDebugColor() : inColor;

	// Draw the plane
	if (inDrawWireframe)
	{
		inRenderer->DrawWireTriangle(vertices[0], vertices[1], vertices[2], color);
		inRenderer->DrawWireTriangle(vertices[0], vertices[2], vertices[3], color);
	}
	else
	{
		inRenderer->DrawTriangle(vertices[0], vertices[1], vertices[2], color, DebugRenderer::ECastShadow::On);
		inRenderer->DrawTriangle(vertices[0], vertices[2], vertices[3], color, DebugRenderer::ECastShadow::On);
	}
}
#endif // JPH_DEBUG_RENDERER

bool PlaneShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	JPH_PROFILE_FUNCTION();

	// Test starting inside of negative half space
	float distance = mPlane.SignedDistance(inRay.mOrigin);
	if (distance <= 0.0f)
	{
		ioHit.mFraction = 0.0f;
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}

	// Test ray parallel to plane
	float dot = inRay.mDirection.Dot(mPlane.GetNormal());
	if (dot == 0.0f)
		return false;

	// Calculate hit fraction
	float fraction = -distance / dot;
	if (fraction >= 0.0f && fraction < ioHit.mFraction)
	{
		ioHit.mFraction = fraction;
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}

	return false;
}

void PlaneShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// Inside solid half space?
	float distance = mPlane.SignedDistance(inRay.mOrigin);
	if (inRayCastSettings.mTreatConvexAsSolid
		&& distance <= 0.0f // Inside plane
		&& ioCollector.GetEarlyOutFraction() > 0.0f) // Willing to accept hits at fraction 0
	{
		// Hit at fraction 0
		RayCastResult hit;
		hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
		hit.mFraction = 0.0f;
		hit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		ioCollector.AddHit(hit);
	}

	float dot = inRay.mDirection.Dot(mPlane.GetNormal());
	if (dot != 0.0f // Parallel ray will not hit plane
		&& (inRayCastSettings.mBackFaceModeConvex == EBackFaceMode::CollideWithBackFaces || dot < 0.0f)) // Back face culling
	{
		// Calculate hit with plane
		float fraction = -distance / dot;
		if (fraction >= 0.0f && fraction < ioCollector.GetEarlyOutFraction())
		{
			RayCastResult hit;
			hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
			hit.mFraction = fraction;
			hit.mSubShapeID2 = inSubShapeIDCreator.GetID();
			ioCollector.AddHit(hit);
		}
	}
}

void PlaneShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	JPH_PROFILE_FUNCTION();

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// Check if the point is inside the plane
	if (mPlane.SignedDistance(inPoint) < 0.0f)
		ioCollector.AddHit({ TransformedShape::sGetBodyID(ioCollector.GetContext()), inSubShapeIDCreator.GetID() });
}

void PlaneShape::CollideSoftBodyVertices(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const CollideSoftBodyVertexIterator &inVertices, uint inNumVertices, int inCollidingShapeIndex) const
{
	JPH_PROFILE_FUNCTION();

	// Convert plane to world space
	Plane plane = mPlane.Scaled(inScale).GetTransformed(inCenterOfMassTransform);

	for (CollideSoftBodyVertexIterator v = inVertices, sbv_end = inVertices + inNumVertices; v != sbv_end; ++v)
		if (v.GetInvMass() > 0.0f)
		{
			// Calculate penetration
			float penetration = -plane.SignedDistance(v.GetPosition());
			if (v.UpdatePenetration(penetration))
				v.SetCollision(plane, inCollidingShapeIndex);
		}
}

// This is a version of GetSupportingFace that returns a face that is large enough to cover the shape we're colliding with but not as large as the regular GetSupportedFace to avoid numerical precision issues
inline static void sGetSupportingFace(const ConvexShape *inShape, Vec3Arg inShapeCOM, const Plane &inPlane, Mat44Arg inPlaneToWorld, ConvexShape::SupportingFace &outPlaneFace)
{
	// Project COM of shape onto plane
	Plane world_plane = inPlane.GetTransformed(inPlaneToWorld);
	Vec3 center = world_plane.ProjectPointOnPlane(inShapeCOM);

	// Create orthogonal basis for the plane
	Vec3 normal = world_plane.GetNormal();
	Vec3 perp1, perp2;
	sPlaneGetOrthogonalBasis(normal, perp1, perp2);

	// Base the size of the face on the bounding box of the shape, ensuring that it is large enough to cover the entire shape
	float size = inShape->GetLocalBounds().GetSize().Length();
	perp1 *= size;
	perp2 *= size;

	// Emit the vertices
	outPlaneFace.resize(4);
	outPlaneFace[0] = center + perp1 + perp2;
	outPlaneFace[1] = center + perp1 - perp2;
	outPlaneFace[2] = center - perp1 - perp2;
	outPlaneFace[3] = center - perp1 + perp2;
}

void PlaneShape::sCastConvexVsPlane(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	// Get the shapes
	JPH_ASSERT(inShapeCast.mShape->GetType() == EShapeType::Convex);
	JPH_ASSERT(inShape->GetType() == EShapeType::Plane);
	const ConvexShape *convex_shape = static_cast<const ConvexShape *>(inShapeCast.mShape);
	const PlaneShape *plane_shape = static_cast<const PlaneShape *>(inShape);

	// Shape cast is provided relative to COM of inShape, so all we need to do is transform our plane with inScale
	Plane plane = plane_shape->mPlane.Scaled(inScale);
	Vec3 normal = plane.GetNormal();

	// Get support function
	ConvexShape::SupportBuffer shape1_support_buffer;
	const ConvexShape::Support *shape1_support = convex_shape->GetSupportFunction(ConvexShape::ESupportMode::Default, shape1_support_buffer, inShapeCast.mScale);

	// Get the support point of the convex shape in the opposite direction of the plane normal in our local space
	Vec3 normal_in_convex_shape_space = inShapeCast.mCenterOfMassStart.Multiply3x3Transposed(normal);
	Vec3 support_point = inShapeCast.mCenterOfMassStart * shape1_support->GetSupport(-normal_in_convex_shape_space);
	float signed_distance = plane.SignedDistance(support_point);
	float convex_radius = shape1_support->GetConvexRadius();
	float penetration_depth = -signed_distance + convex_radius;
	float dot = inShapeCast.mDirection.Dot(normal);

	// Collision output
	Mat44 com_hit;
	Vec3 point1, point2;
	float fraction;

	// Do we start in collision?
	if (penetration_depth > 0.0f)
	{
		// Back face culling?
		if (inShapeCastSettings.mBackFaceModeConvex == EBackFaceMode::IgnoreBackFaces && dot > 0.0f)
			return;

		// Shallower hit?
		if (penetration_depth <= -ioCollector.GetEarlyOutFraction())
			return;

		// We're hitting at fraction 0
		fraction = 0.0f;

		// Get contact point
		com_hit = inCenterOfMassTransform2;
		point1 = inCenterOfMassTransform2 * (support_point - normal * convex_radius);
		point2 = inCenterOfMassTransform2 * (support_point - normal * signed_distance);
	}
	else if (dot < 0.0f) // Moving towards the plane?
	{
		// Calculate hit fraction
		fraction = penetration_depth / dot;
		JPH_ASSERT(fraction >= 0.0f);

		// Further than early out fraction?
		if (fraction >= ioCollector.GetEarlyOutFraction())
			return;

		// Get contact point
		com_hit = inCenterOfMassTransform2.PostTranslated(fraction * inShapeCast.mDirection);
		point1 = point2 = com_hit * (support_point - normal * convex_radius);
	}
	else
	{
		// Moving away from the plane
		return;
	}

	// Create cast result
	Vec3 penetration_axis_world = com_hit.Multiply3x3(-normal);
	bool back_facing = dot > 0.0f;
	ShapeCastResult result(fraction, point1, point2, penetration_axis_world, back_facing, inSubShapeIDCreator1.GetID(), inSubShapeIDCreator2.GetID(), TransformedShape::sGetBodyID(ioCollector.GetContext()));

	// Gather faces
	if (inShapeCastSettings.mCollectFacesMode == ECollectFacesMode::CollectFaces)
	{
		// Get supporting face of convex shape
		Mat44 shape_to_world = com_hit * inShapeCast.mCenterOfMassStart;
		convex_shape->GetSupportingFace(SubShapeID(), normal_in_convex_shape_space, inShapeCast.mScale, shape_to_world, result.mShape1Face);

		// Get supporting face of plane
		if (!result.mShape1Face.empty())
			sGetSupportingFace(convex_shape, shape_to_world.GetTranslation(), plane, inCenterOfMassTransform2, result.mShape2Face);
	}

	// Notify the collector
	JPH_IF_TRACK_NARROWPHASE_STATS(TrackNarrowPhaseCollector track;)
	ioCollector.AddHit(result);
}

struct PlaneShape::PSGetTrianglesContext
{
	Float3	mVertices[4];
	bool	mDone = false;
};

void PlaneShape::GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	static_assert(sizeof(PSGetTrianglesContext) <= sizeof(GetTrianglesContext), "GetTrianglesContext too small");
	JPH_ASSERT(IsAligned(&ioContext, alignof(PSGetTrianglesContext)));

	PSGetTrianglesContext *context = new (&ioContext) PSGetTrianglesContext();

	// Get the vertices of the plane
	Vec3 vertices[4];
	GetVertices(vertices);

	// Reverse if scale is inside out
	if (ScaleHelpers::IsInsideOut(inScale))
	{
		std::swap(vertices[0], vertices[3]);
		std::swap(vertices[1], vertices[2]);
	}

	// Transform them to world space
	Mat44 com = Mat44::sRotationTranslation(inRotation, inPositionCOM).PreScaled(inScale);
	for (uint i = 0; i < 4; ++i)
		(com * vertices[i]).StoreFloat3(&context->mVertices[i]);
}

int PlaneShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	static_assert(cGetTrianglesMinTrianglesRequested >= 2, "cGetTrianglesMinTrianglesRequested is too small");
	JPH_ASSERT(inMaxTrianglesRequested >= cGetTrianglesMinTrianglesRequested);

	// Check if we're done
	PSGetTrianglesContext &context = (PSGetTrianglesContext &)ioContext;
	if (context.mDone)
		return 0;
	context.mDone = true;

	// 1st triangle
	outTriangleVertices[0] = context.mVertices[0];
	outTriangleVertices[1] = context.mVertices[1];
	outTriangleVertices[2] = context.mVertices[2];

	// 2nd triangle
	outTriangleVertices[3] = context.mVertices[0];
	outTriangleVertices[4] = context.mVertices[2];
	outTriangleVertices[5] = context.mVertices[3];

	if (outMaterials != nullptr)
	{
		// Get material
		const PhysicsMaterial *material = GetMaterial(SubShapeID());
		outMaterials[0] = material;
		outMaterials[1] = material;
	}

	return 2;
}

void PlaneShape::sCollideConvexVsPlane(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	// Get the shapes
	JPH_ASSERT(inShape1->GetType() == EShapeType::Convex);
	JPH_ASSERT(inShape2->GetType() == EShapeType::Plane);
	const ConvexShape *shape1 = static_cast<const ConvexShape *>(inShape1);
	const PlaneShape *shape2 = static_cast<const PlaneShape *>(inShape2);

	// Transform the plane to the space of the convex shape
	Plane scaled_plane = shape2->mPlane.Scaled(inScale2);
	Plane plane = scaled_plane.GetTransformed(inCenterOfMassTransform1.InversedRotationTranslation() * inCenterOfMassTransform2);
	Vec3 normal = plane.GetNormal();

	// Get support function
	ConvexShape::SupportBuffer shape1_support_buffer;
	const ConvexShape::Support *shape1_support = shape1->GetSupportFunction(ConvexShape::ESupportMode::Default, shape1_support_buffer, inScale1);

	// Get the support point of the convex shape in the opposite direction of the plane normal
	Vec3 support_point = shape1_support->GetSupport(-normal);
	float signed_distance = plane.SignedDistance(support_point);
	float convex_radius = shape1_support->GetConvexRadius();
	float penetration_depth = -signed_distance + convex_radius;
	if (penetration_depth > -inCollideShapeSettings.mMaxSeparationDistance)
	{
		// Get contact point
		Vec3 point1 = inCenterOfMassTransform1 * (support_point - normal * convex_radius);
		Vec3 point2 = inCenterOfMassTransform1 * (support_point - normal * signed_distance);
		Vec3 penetration_axis_world = inCenterOfMassTransform1.Multiply3x3(-normal);

		// Create collision result
		CollideShapeResult result(point1, point2, penetration_axis_world, penetration_depth, inSubShapeIDCreator1.GetID(), inSubShapeIDCreator2.GetID(), TransformedShape::sGetBodyID(ioCollector.GetContext()));

		// Gather faces
		if (inCollideShapeSettings.mCollectFacesMode == ECollectFacesMode::CollectFaces)
		{
			// Get supporting face of shape 1
			shape1->GetSupportingFace(SubShapeID(), normal, inScale1, inCenterOfMassTransform1, result.mShape1Face);

			// Get supporting face of shape 2
			if (!result.mShape1Face.empty())
				sGetSupportingFace(shape1, inCenterOfMassTransform1.GetTranslation(), scaled_plane, inCenterOfMassTransform2, result.mShape2Face);
		}

		// Notify the collector
		JPH_IF_TRACK_NARROWPHASE_STATS(TrackNarrowPhaseCollector track;)
		ioCollector.AddHit(result);
	}
}

void PlaneShape::SaveBinaryState(StreamOut &inStream) const
{
	Shape::SaveBinaryState(inStream);

	inStream.Write(mPlane);
	inStream.Write(mHalfExtent);
}

void PlaneShape::RestoreBinaryState(StreamIn &inStream)
{
	Shape::RestoreBinaryState(inStream);

	inStream.Read(mPlane);
	inStream.Read(mHalfExtent);

	CalculateLocalBounds();
}

void PlaneShape::SaveMaterialState(PhysicsMaterialList &outMaterials) const
{
	outMaterials = { mMaterial };
}

void PlaneShape::RestoreMaterialState(const PhysicsMaterialRefC *inMaterials, uint inNumMaterials)
{
	JPH_ASSERT(inNumMaterials == 1);
	mMaterial = inMaterials[0];
}

void PlaneShape::sRegister()
{
	ShapeFunctions &f = ShapeFunctions::sGet(EShapeSubType::Plane);
	f.mConstruct = []() -> Shape * { return new PlaneShape; };
	f.mColor = Color::sDarkRed;

	for (EShapeSubType s : sConvexSubShapeTypes)
	{
		CollisionDispatch::sRegisterCollideShape(s, EShapeSubType::Plane, sCollideConvexVsPlane);
		CollisionDispatch::sRegisterCastShape(s, EShapeSubType::Plane, sCastConvexVsPlane);

		CollisionDispatch::sRegisterCastShape(EShapeSubType::Plane, s, CollisionDispatch::sReversedCastShape);
		CollisionDispatch::sRegisterCollideShape(EShapeSubType::Plane, s, CollisionDispatch::sReversedCollideShape);
	}
}

JPH_NAMESPACE_END
