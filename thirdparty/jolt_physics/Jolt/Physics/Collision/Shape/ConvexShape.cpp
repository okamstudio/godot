// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/GetTrianglesContext.h>
#include <Jolt/Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/NarrowPhaseStats.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Geometry/EPAPenetrationDepth.h>
#include <Jolt/Geometry/OrientedBox.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_ABSTRACT(ConvexShapeSettings)
{
	JPH_ADD_BASE_CLASS(ConvexShapeSettings, ShapeSettings)

	JPH_ADD_ATTRIBUTE(ConvexShapeSettings, mDensity)
	JPH_ADD_ATTRIBUTE(ConvexShapeSettings, mMaterial)
}

const StaticArray<Vec3, 384> ConvexShape::sUnitSphereTriangles = []() {
	const int level = 2;

	StaticArray<Vec3, 384> verts;
	GetTrianglesContextVertexList::sCreateHalfUnitSphereTop(verts, level);
	GetTrianglesContextVertexList::sCreateHalfUnitSphereBottom(verts, level);
	return verts;
}();

void ConvexShape::sCollideConvexVsConvex(const Shape *inShape1, const Shape *inShape2, Vec3Arg inScale1, Vec3Arg inScale2, Mat44Arg inCenterOfMassTransform1, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, const CollideShapeSettings &inCollideShapeSettings, CollideShapeCollector &ioCollector, [[maybe_unused]] const ShapeFilter &inShapeFilter)
{
	JPH_PROFILE_FUNCTION();

	// Get the shapes
	JPH_ASSERT(inShape1->GetType() == EShapeType::Convex);
	JPH_ASSERT(inShape2->GetType() == EShapeType::Convex);
	const ConvexShape *shape1 = static_cast<const ConvexShape *>(inShape1);
	const ConvexShape *shape2 = static_cast<const ConvexShape *>(inShape2);

	// Get transforms
	Mat44 inverse_transform1 = inCenterOfMassTransform1.InversedRotationTranslation();
	Mat44 transform_2_to_1 = inverse_transform1 * inCenterOfMassTransform2;

	// Get bounding boxes
	AABox shape1_bbox = shape1->GetLocalBounds().Scaled(inScale1);
	shape1_bbox.ExpandBy(Vec3::sReplicate(inCollideShapeSettings.mMaxSeparationDistance));
	AABox shape2_bbox = shape2->GetLocalBounds().Scaled(inScale2);

	// Check if they overlap
	if (!OrientedBox(transform_2_to_1, shape2_bbox).Overlaps(shape1_bbox))
		return;

	// Note: As we don't remember the penetration axis from the last iteration, and it is likely that shape2 is pushed out of
	// collision relative to shape1 by comparing their COM's, we use that as an initial penetration axis: shape2.com - shape1.com
	// This has been seen to improve performance by approx. 1% over using a fixed axis like (1, 0, 0).
	Vec3 penetration_axis = transform_2_to_1.GetTranslation();

	// Ensure that we do not pass in a near zero penetration axis
	if (penetration_axis.IsNearZero())
		penetration_axis = Vec3::sAxisX();

	Vec3 point1, point2;
	EPAPenetrationDepth pen_depth;
	EPAPenetrationDepth::EStatus status;

	// Scope to limit lifetime of SupportBuffer
	{
		// Create support function
		SupportBuffer buffer1_excl_cvx_radius, buffer2_excl_cvx_radius;
		const Support *shape1_excl_cvx_radius = shape1->GetSupportFunction(ConvexShape::ESupportMode::ExcludeConvexRadius, buffer1_excl_cvx_radius, inScale1);
		const Support *shape2_excl_cvx_radius = shape2->GetSupportFunction(ConvexShape::ESupportMode::ExcludeConvexRadius, buffer2_excl_cvx_radius, inScale2);

		// Transform shape 2 in the space of shape 1
		TransformedConvexObject<Support> transformed2_excl_cvx_radius(transform_2_to_1, *shape2_excl_cvx_radius);

		// Perform GJK step
		status = pen_depth.GetPenetrationDepthStepGJK(*shape1_excl_cvx_radius, shape1_excl_cvx_radius->GetConvexRadius() + inCollideShapeSettings.mMaxSeparationDistance, transformed2_excl_cvx_radius, shape2_excl_cvx_radius->GetConvexRadius(), inCollideShapeSettings.mCollisionTolerance, penetration_axis, point1, point2);
	}

	// Check result of collision detection
	switch (status)
	{
	case EPAPenetrationDepth::EStatus::Colliding:
		break;

	case EPAPenetrationDepth::EStatus::NotColliding:
		return;

	case EPAPenetrationDepth::EStatus::Indeterminate:
		{
			// Need to run expensive EPA algorithm

			// Create support function
			SupportBuffer buffer1_incl_cvx_radius, buffer2_incl_cvx_radius;
			const Support *shape1_incl_cvx_radius = shape1->GetSupportFunction(ConvexShape::ESupportMode::IncludeConvexRadius, buffer1_incl_cvx_radius, inScale1);
			const Support *shape2_incl_cvx_radius = shape2->GetSupportFunction(ConvexShape::ESupportMode::IncludeConvexRadius, buffer2_incl_cvx_radius, inScale2);

			// Add separation distance
			AddConvexRadius<Support> shape1_add_max_separation_distance(*shape1_incl_cvx_radius, inCollideShapeSettings.mMaxSeparationDistance);

			// Transform shape 2 in the space of shape 1
			TransformedConvexObject<Support> transformed2_incl_cvx_radius(transform_2_to_1, *shape2_incl_cvx_radius);

			// Perform EPA step
			if (!pen_depth.GetPenetrationDepthStepEPA(shape1_add_max_separation_distance, transformed2_incl_cvx_radius, inCollideShapeSettings.mPenetrationTolerance, penetration_axis, point1, point2))
				return;
			break;
		}
	}

	// Check if the penetration is bigger than the early out fraction
	float penetration_depth = (point2 - point1).Length() - inCollideShapeSettings.mMaxSeparationDistance;
	if (-penetration_depth >= ioCollector.GetEarlyOutFraction())
		return;

	// Correct point1 for the added separation distance
	float penetration_axis_len = penetration_axis.Length();
	if (penetration_axis_len > 0.0f)
		point1 -= penetration_axis * (inCollideShapeSettings.mMaxSeparationDistance / penetration_axis_len);

	// Convert to world space
	point1 = inCenterOfMassTransform1 * point1;
	point2 = inCenterOfMassTransform1 * point2;
	Vec3 penetration_axis_world = inCenterOfMassTransform1.Multiply3x3(penetration_axis);

	// Create collision result
	CollideShapeResult result(point1, point2, penetration_axis_world, penetration_depth, inSubShapeIDCreator1.GetID(), inSubShapeIDCreator2.GetID(), TransformedShape::sGetBodyID(ioCollector.GetContext()));

	// Gather faces
	if (inCollideShapeSettings.mCollectFacesMode == ECollectFacesMode::CollectFaces)
	{
		// Get supporting face of shape 1
		shape1->GetSupportingFace(SubShapeID(), -penetration_axis, inScale1, inCenterOfMassTransform1, result.mShape1Face);

		// Get supporting face of shape 2
		shape2->GetSupportingFace(SubShapeID(), transform_2_to_1.Multiply3x3Transposed(penetration_axis), inScale2, inCenterOfMassTransform2, result.mShape2Face);
	}

	// Notify the collector
	JPH_IF_TRACK_NARROWPHASE_STATS(TrackNarrowPhaseCollector track;)
	ioCollector.AddHit(result);
}

bool ConvexShape::CastRay(const RayCast &inRay, const SubShapeIDCreator &inSubShapeIDCreator, RayCastResult &ioHit) const
{
	// Note: This is a fallback routine, most convex shapes should implement a more performant version!

	JPH_PROFILE_FUNCTION();

	// Create support function
	SupportBuffer buffer;
	const Support *support = GetSupportFunction(ConvexShape::ESupportMode::IncludeConvexRadius, buffer, Vec3::sReplicate(1.0f));

	// Cast ray
	GJKClosestPoint gjk;
	if (gjk.CastRay(inRay.mOrigin, inRay.mDirection, cDefaultCollisionTolerance, *support, ioHit.mFraction))
	{
		ioHit.mSubShapeID2 = inSubShapeIDCreator.GetID();
		return true;
	}

	return false;
}

void ConvexShape::CastRay(const RayCast &inRay, const RayCastSettings &inRayCastSettings, const SubShapeIDCreator &inSubShapeIDCreator, CastRayCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Note: This is a fallback routine, most convex shapes should implement a more performant version!

	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// First do a normal raycast, limited to the early out fraction
	RayCastResult hit;
	hit.mFraction = ioCollector.GetEarlyOutFraction();
	if (CastRay(inRay, inSubShapeIDCreator, hit))
	{
		// Check front side
		if (inRayCastSettings.mTreatConvexAsSolid || hit.mFraction > 0.0f)
		{
			hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
			ioCollector.AddHit(hit);
		}

		// Check if we want back facing hits and the collector still accepts additional hits
		if (inRayCastSettings.mBackFaceModeConvex == EBackFaceMode::CollideWithBackFaces && !ioCollector.ShouldEarlyOut())
		{
			// Invert the ray, going from the early out fraction back to the fraction where we found our forward hit
			float start_fraction = min(1.0f, ioCollector.GetEarlyOutFraction());
			float delta_fraction = hit.mFraction - start_fraction;
			if (delta_fraction < 0.0f)
			{
				RayCast inverted_ray { inRay.mOrigin + start_fraction * inRay.mDirection, delta_fraction * inRay.mDirection };

				// Cast another ray
				RayCastResult inverted_hit;
				inverted_hit.mFraction = 1.0f;
				if (CastRay(inverted_ray, inSubShapeIDCreator, inverted_hit)
					&& inverted_hit.mFraction > 0.0f) // Ignore hits with fraction 0, this means the ray ends inside the object and we don't want to report it as a back facing hit
				{
					// Invert fraction and rescale it to the fraction of the original ray
					inverted_hit.mFraction = hit.mFraction + (inverted_hit.mFraction - 1.0f) * delta_fraction;
					inverted_hit.mBodyID = TransformedShape::sGetBodyID(ioCollector.GetContext());
					ioCollector.AddHit(inverted_hit);
				}
			}
		}
	}
}

void ConvexShape::CollidePoint(Vec3Arg inPoint, const SubShapeIDCreator &inSubShapeIDCreator, CollidePointCollector &ioCollector, const ShapeFilter &inShapeFilter) const
{
	// Test shape filter
	if (!inShapeFilter.ShouldCollide(this, inSubShapeIDCreator.GetID()))
		return;

	// First test bounding box
	if (GetLocalBounds().Contains(inPoint))
	{
		// Create support function
		SupportBuffer buffer;
		const Support *support = GetSupportFunction(ConvexShape::ESupportMode::IncludeConvexRadius, buffer, Vec3::sReplicate(1.0f));

		// Create support function for point
		PointConvexSupport point { inPoint };

		// Test intersection
		GJKClosestPoint gjk;
		Vec3 v = inPoint;
		if (gjk.Intersects(*support, point, cDefaultCollisionTolerance, v))
			ioCollector.AddHit({ TransformedShape::sGetBodyID(ioCollector.GetContext()), inSubShapeIDCreator.GetID() });
	}
}

void ConvexShape::sCastConvexVsConvex(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, const Shape *inShape, Vec3Arg inScale, [[maybe_unused]] const ShapeFilter &inShapeFilter, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, const SubShapeIDCreator &inSubShapeIDCreator2, CastShapeCollector &ioCollector)
{
	JPH_PROFILE_FUNCTION();

	// Only supported for convex shapes
	JPH_ASSERT(inShapeCast.mShape->GetType() == EShapeType::Convex);
	const ConvexShape *cast_shape = static_cast<const ConvexShape *>(inShapeCast.mShape);

	JPH_ASSERT(inShape->GetType() == EShapeType::Convex);
	const ConvexShape *shape = static_cast<const ConvexShape *>(inShape);

	// Determine if we want to use the actual shape or a shrunken shape with convex radius
	ConvexShape::ESupportMode support_mode = inShapeCastSettings.mUseShrunkenShapeAndConvexRadius? ConvexShape::ESupportMode::ExcludeConvexRadius : ConvexShape::ESupportMode::Default;

	// Create support function for shape to cast
	SupportBuffer cast_buffer;
	const Support *cast_support = cast_shape->GetSupportFunction(support_mode, cast_buffer, inShapeCast.mScale);

	// Create support function for target shape
	SupportBuffer target_buffer;
	const Support *target_support = shape->GetSupportFunction(support_mode, target_buffer, inScale);

	// Do a raycast against the result
	EPAPenetrationDepth epa;
	float fraction = ioCollector.GetEarlyOutFraction();
	Vec3 contact_point_a, contact_point_b, contact_normal;
	if (epa.CastShape(inShapeCast.mCenterOfMassStart, inShapeCast.mDirection, inShapeCastSettings.mCollisionTolerance, inShapeCastSettings.mPenetrationTolerance, *cast_support, *target_support, cast_support->GetConvexRadius(), target_support->GetConvexRadius(), inShapeCastSettings.mReturnDeepestPoint, fraction, contact_point_a, contact_point_b, contact_normal)
		&& (inShapeCastSettings.mBackFaceModeConvex == EBackFaceMode::CollideWithBackFaces
			|| contact_normal.Dot(inShapeCast.mDirection) > 0.0f)) // Test if backfacing
	{
		// Convert to world space
		contact_point_a = inCenterOfMassTransform2 * contact_point_a;
		contact_point_b = inCenterOfMassTransform2 * contact_point_b;
		Vec3 contact_normal_world = inCenterOfMassTransform2.Multiply3x3(contact_normal);

		ShapeCastResult result(fraction, contact_point_a, contact_point_b, contact_normal_world, false, inSubShapeIDCreator1.GetID(), inSubShapeIDCreator2.GetID(), TransformedShape::sGetBodyID(ioCollector.GetContext()));

		// Early out if this hit is deeper than the collector's early out value
		if (fraction == 0.0f && -result.mPenetrationDepth >= ioCollector.GetEarlyOutFraction())
			return;

		// Gather faces
		if (inShapeCastSettings.mCollectFacesMode == ECollectFacesMode::CollectFaces)
		{
			// Get supporting face of shape 1
			Mat44 transform_1_to_2 = inShapeCast.mCenterOfMassStart;
			transform_1_to_2.SetTranslation(transform_1_to_2.GetTranslation() + fraction * inShapeCast.mDirection);
			cast_shape->GetSupportingFace(SubShapeID(), transform_1_to_2.Multiply3x3Transposed(-contact_normal), inShapeCast.mScale, inCenterOfMassTransform2 * transform_1_to_2, result.mShape1Face);

			// Get supporting face of shape 2
			shape->GetSupportingFace(SubShapeID(), contact_normal, inScale, inCenterOfMassTransform2, result.mShape2Face);
		}

		JPH_IF_TRACK_NARROWPHASE_STATS(TrackNarrowPhaseCollector track;)
		ioCollector.AddHit(result);
	}
}

class ConvexShape::CSGetTrianglesContext
{
public:
				CSGetTrianglesContext(const ConvexShape *inShape, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) :
		mLocalToWorld(Mat44::sRotationTranslation(inRotation, inPositionCOM) * Mat44::sScale(inScale)),
		mIsInsideOut(ScaleHelpers::IsInsideOut(inScale))
	{
		mSupport = inShape->GetSupportFunction(ESupportMode::IncludeConvexRadius, mSupportBuffer, Vec3::sReplicate(1.0f));
	}

	SupportBuffer		mSupportBuffer;
	const Support *		mSupport;
	Mat44				mLocalToWorld;
	bool				mIsInsideOut;
	size_t				mCurrentVertex = 0;
};

void ConvexShape::GetTrianglesStart(GetTrianglesContext &ioContext, const AABox &inBox, Vec3Arg inPositionCOM, QuatArg inRotation, Vec3Arg inScale) const
{
	static_assert(sizeof(CSGetTrianglesContext) <= sizeof(GetTrianglesContext), "GetTrianglesContext too small");
	JPH_ASSERT(IsAligned(&ioContext, alignof(CSGetTrianglesContext)));

	new (&ioContext) CSGetTrianglesContext(this, inPositionCOM, inRotation, inScale);
}

int ConvexShape::GetTrianglesNext(GetTrianglesContext &ioContext, int inMaxTrianglesRequested, Float3 *outTriangleVertices, const PhysicsMaterial **outMaterials) const
{
	JPH_ASSERT(inMaxTrianglesRequested >= cGetTrianglesMinTrianglesRequested);

	CSGetTrianglesContext &context = (CSGetTrianglesContext &)ioContext;

	int total_num_vertices = min(inMaxTrianglesRequested * 3, int(sUnitSphereTriangles.size() - context.mCurrentVertex));

	if (context.mIsInsideOut)
	{
		// Store triangles flipped
		for (const Vec3 *v = sUnitSphereTriangles.data() + context.mCurrentVertex, *v_end = v + total_num_vertices; v < v_end; v += 3)
		{
			(context.mLocalToWorld * context.mSupport->GetSupport(v[0])).StoreFloat3(outTriangleVertices++);
			(context.mLocalToWorld * context.mSupport->GetSupport(v[2])).StoreFloat3(outTriangleVertices++);
			(context.mLocalToWorld * context.mSupport->GetSupport(v[1])).StoreFloat3(outTriangleVertices++);
		}
	}
	else
	{
		// Store triangles
		for (const Vec3 *v = sUnitSphereTriangles.data() + context.mCurrentVertex, *v_end = v + total_num_vertices; v < v_end; v += 3)
		{
			(context.mLocalToWorld * context.mSupport->GetSupport(v[0])).StoreFloat3(outTriangleVertices++);
			(context.mLocalToWorld * context.mSupport->GetSupport(v[1])).StoreFloat3(outTriangleVertices++);
			(context.mLocalToWorld * context.mSupport->GetSupport(v[2])).StoreFloat3(outTriangleVertices++);
		}
	}

	context.mCurrentVertex += total_num_vertices;
	int total_num_triangles = total_num_vertices / 3;

	// Store materials
	if (outMaterials != nullptr)
	{
		const PhysicsMaterial *material = GetMaterial();
		for (const PhysicsMaterial **m = outMaterials, **m_end = outMaterials + total_num_triangles; m < m_end; ++m)
			*m = material;
	}

	return total_num_triangles;
}

void ConvexShape::GetSubmergedVolume(Mat44Arg inCenterOfMassTransform, Vec3Arg inScale, const Plane &inSurface, float &outTotalVolume, float &outSubmergedVolume, Vec3 &outCenterOfBuoyancy JPH_IF_DEBUG_RENDERER(, RVec3Arg inBaseOffset)) const
{
	// Calculate total volume
	Vec3 abs_scale = inScale.Abs();
	Vec3 extent = GetLocalBounds().GetExtent() * abs_scale;
	outTotalVolume = 8.0f * extent.GetX() * extent.GetY() * extent.GetZ();

	// Points of the bounding box
	Vec3 points[] =
	{
		Vec3(-1, -1, -1),
		Vec3( 1, -1, -1),
		Vec3(-1,  1, -1),
		Vec3( 1,  1, -1),
		Vec3(-1, -1,  1),
		Vec3( 1, -1,  1),
		Vec3(-1,  1,  1),
		Vec3( 1,  1,  1),
	};

	// Faces of the bounding box
	using Face = int[5];
	#define MAKE_FACE(a, b, c, d) { a, b, c, d, ((1 << a) | (1 << b) | (1 << c) | (1 << d)) } // Last int is a bit mask that indicates which indices are used
	Face faces[] =
	{
		MAKE_FACE(0, 2, 3, 1),
		MAKE_FACE(4, 6, 2, 0),
		MAKE_FACE(4, 5, 7, 6),
		MAKE_FACE(1, 3, 7, 5),
		MAKE_FACE(2, 6, 7, 3),
		MAKE_FACE(0, 1, 5, 4),
	};

	PolyhedronSubmergedVolumeCalculator::Point *buffer = (PolyhedronSubmergedVolumeCalculator::Point *)JPH_STACK_ALLOC(8 * sizeof(PolyhedronSubmergedVolumeCalculator::Point));
	PolyhedronSubmergedVolumeCalculator submerged_vol_calc(inCenterOfMassTransform * Mat44::sScale(extent), points, sizeof(Vec3), 8, inSurface, buffer JPH_IF_DEBUG_RENDERER(, inBaseOffset));

	if (submerged_vol_calc.AreAllAbove())
	{
		// We're above the water
		outSubmergedVolume = 0.0f;
		outCenterOfBuoyancy = Vec3::sZero();
	}
	else if (submerged_vol_calc.AreAllBelow())
	{
		// We're fully submerged
		outSubmergedVolume = outTotalVolume;
		outCenterOfBuoyancy = inCenterOfMassTransform.GetTranslation();
	}
	else
	{
		// Calculate submerged volume
		int reference_point_bit = 1 << submerged_vol_calc.GetReferencePointIdx();
		for (const Face &f : faces)
		{
			// Test if this face includes the reference point
			if ((f[4] & reference_point_bit) == 0)
			{
				// Triangulate the face (a quad)
				submerged_vol_calc.AddFace(f[0], f[1], f[2]);
				submerged_vol_calc.AddFace(f[0], f[2], f[3]);
			}
		}

		submerged_vol_calc.GetResult(outSubmergedVolume, outCenterOfBuoyancy);
	}
}

#ifdef JPH_DEBUG_RENDERER
void ConvexShape::DrawGetSupportFunction(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale, ColorArg inColor, bool inDrawSupportDirection) const
{
	// Get the support function with convex radius
	SupportBuffer buffer;
	const Support *support = GetSupportFunction(ESupportMode::ExcludeConvexRadius, buffer, inScale);
	AddConvexRadius<Support> add_convex(*support, support->GetConvexRadius());

	// Draw the shape
	DebugRenderer::GeometryRef geometry = inRenderer->CreateTriangleGeometryForConvex([&add_convex](Vec3Arg inDirection) { return add_convex.GetSupport(inDirection); });
	AABox bounds = geometry->mBounds.Transformed(inCenterOfMassTransform);
	float lod_scale_sq = geometry->mBounds.GetExtent().LengthSq();
	inRenderer->DrawGeometry(inCenterOfMassTransform, bounds, lod_scale_sq, inColor, geometry);

	if (inDrawSupportDirection)
	{
		// Iterate on all directions and draw the support point and an arrow in the direction that was sampled to test if the support points make sense
		for (Vec3 v : Vec3::sUnitSphere)
		{
			Vec3 direction = 0.05f * v;
			Vec3 pos = add_convex.GetSupport(direction);
			RVec3 from = inCenterOfMassTransform * pos;
			RVec3 to = inCenterOfMassTransform * (pos + direction);
			inRenderer->DrawMarker(from, Color::sWhite, 0.001f);
			inRenderer->DrawArrow(from, to, Color::sWhite, 0.001f);
		}
	}
}

void ConvexShape::DrawGetSupportingFace(DebugRenderer *inRenderer, RMat44Arg inCenterOfMassTransform, Vec3Arg inScale) const
{
	// Sample directions and map which faces belong to which directions
	using FaceToDirection = UnorderedMap<SupportingFace, Array<Vec3>>;
	FaceToDirection faces;
	for (Vec3 v : Vec3::sUnitSphere)
	{
		Vec3 direction = 0.05f * v;

		SupportingFace face;
		GetSupportingFace(SubShapeID(), direction, inScale, Mat44::sIdentity(), face);

		if (!face.empty())
		{
			JPH_ASSERT(face.size() >= 2, "The GetSupportingFace function should either return nothing or at least an edge");
			faces[face].push_back(direction);
		}
	}

	// Draw each face in a unique color and draw corresponding directions
	int color_it = 0;
	for (FaceToDirection::value_type &ftd : faces)
	{
		Color color = Color::sGetDistinctColor(color_it++);

		// Create copy of face (key in map is read only)
		SupportingFace face = ftd.first;

		// Displace the face a little bit forward so it is easier to see
		Vec3 normal = face.size() >= 3? (face[2] - face[1]).Cross(face[0] - face[1]).Normalized() : Vec3::sZero();
		Vec3 displacement = 0.001f * normal;

		// Transform face to world space and calculate center of mass
		Vec3 com_ls = Vec3::sZero();
		for (Vec3 &v : face)
		{
			v = inCenterOfMassTransform.Multiply3x3(v + displacement);
			com_ls += v;
		}
		RVec3 com = inCenterOfMassTransform.GetTranslation() + com_ls / (float)face.size();

		// Draw the polygon and directions
		inRenderer->DrawWirePolygon(RMat44::sTranslation(inCenterOfMassTransform.GetTranslation()), face, color, face.size() >= 3? 0.001f : 0.0f);
		if (face.size() >= 3)
			inRenderer->DrawArrow(com, com + inCenterOfMassTransform.Multiply3x3(normal), color, 0.01f);
		for (Vec3 &v : ftd.second)
			inRenderer->DrawArrow(com, com + inCenterOfMassTransform.Multiply3x3(-v), color, 0.001f);
	}
}
#endif // JPH_DEBUG_RENDERER

void ConvexShape::SaveBinaryState(StreamOut &inStream) const
{
	Shape::SaveBinaryState(inStream);

	inStream.Write(mDensity);
}

void ConvexShape::RestoreBinaryState(StreamIn &inStream)
{
	Shape::RestoreBinaryState(inStream);

	inStream.Read(mDensity);
}

void ConvexShape::SaveMaterialState(PhysicsMaterialList &outMaterials) const
{
	outMaterials.clear();
	outMaterials.push_back(mMaterial);
}

void ConvexShape::RestoreMaterialState(const PhysicsMaterialRefC *inMaterials, uint inNumMaterials)
{
	JPH_ASSERT(inNumMaterials == 1);
	mMaterial = inMaterials[0];
}

void ConvexShape::sRegister()
{
	for (EShapeSubType s1 : sConvexSubShapeTypes)
		for (EShapeSubType s2 : sConvexSubShapeTypes)
		{
			CollisionDispatch::sRegisterCollideShape(s1, s2, sCollideConvexVsConvex);
			CollisionDispatch::sRegisterCastShape(s1, s2, sCastConvexVsConvex);
		}
}

JPH_NAMESPACE_END
