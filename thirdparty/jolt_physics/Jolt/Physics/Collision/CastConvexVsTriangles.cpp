// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/CastConvexVsTriangles.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/ActiveEdges.h>
#include <Jolt/Physics/Collision/NarrowPhaseStats.h>
#include <Jolt/Geometry/EPAPenetrationDepth.h>

JPH_NAMESPACE_BEGIN

CastConvexVsTriangles::CastConvexVsTriangles(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, CastShapeCollector &ioCollector) :
	mShapeCast(inShapeCast),
	mShapeCastSettings(inShapeCastSettings),
	mCenterOfMassTransform2(inCenterOfMassTransform2),
	mScale(inScale),
	mSubShapeIDCreator1(inSubShapeIDCreator1),
	mCollector(ioCollector)
{
	JPH_ASSERT(inShapeCast.mShape->GetType() == EShapeType::Convex);

	// Determine if shape is inside out or not
	mScaleSign = ScaleHelpers::IsInsideOut(inScale)? -1.0f : 1.0f;
}

void CastConvexVsTriangles::Cast(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, const SubShapeID &inSubShapeID2)
{
	JPH_PROFILE_FUNCTION();

	// Scale triangle
	Vec3 v0 = mScale * inV0;
	Vec3 v1 = mScale * inV1;
	Vec3 v2 = mScale * inV2;

	// Calculate triangle normal
	Vec3 triangle_normal = mScaleSign * (v1 - v0).Cross(v2 - v0);

	// Backface check
	bool back_facing = triangle_normal.Dot(mShapeCast.mDirection) > 0.0f;
	if (mShapeCastSettings.mBackFaceModeTriangles == EBackFaceMode::IgnoreBackFaces && back_facing)
		return;

	// Create triangle support function
	TriangleConvexSupport triangle { v0, v1, v2 };

	// Check if we already created the cast shape support function
	if (mSupport == nullptr)
	{
		// Determine if we want to use the actual shape or a shrunken shape with convex radius
		ConvexShape::ESupportMode support_mode = mShapeCastSettings.mUseShrunkenShapeAndConvexRadius? ConvexShape::ESupportMode::ExcludeConvexRadius : ConvexShape::ESupportMode::Default;

		// Create support function
		mSupport = static_cast<const ConvexShape *>(mShapeCast.mShape)->GetSupportFunction(support_mode, mSupportBuffer, mShapeCast.mScale);
	}

	EPAPenetrationDepth epa;
	float fraction = mCollector.GetEarlyOutFraction();
	Vec3 contact_point_a, contact_point_b, contact_normal;
	if (epa.CastShape(mShapeCast.mCenterOfMassStart, mShapeCast.mDirection, mShapeCastSettings.mCollisionTolerance, mShapeCastSettings.mPenetrationTolerance, *mSupport, triangle, mSupport->GetConvexRadius(), 0.0f, mShapeCastSettings.mReturnDeepestPoint, fraction, contact_point_a, contact_point_b, contact_normal))
	{
		// Check if we have enabled active edge detection
		if (mShapeCastSettings.mActiveEdgeMode == EActiveEdgeMode::CollideOnlyWithActive && inActiveEdges != 0b111)
		{
			// Convert the active edge velocity hint to local space
			Vec3 active_edge_movement_direction = mCenterOfMassTransform2.Multiply3x3Transposed(mShapeCastSettings.mActiveEdgeMovementDirection);

			// Update the contact normal to account for active edges
			// Note that we flip the triangle normal as the penetration axis is pointing towards the triangle instead of away
			contact_normal = ActiveEdges::FixNormal(v0, v1, v2, back_facing? triangle_normal : -triangle_normal, inActiveEdges, contact_point_b, contact_normal, active_edge_movement_direction);
		}

		// Convert to world space
		contact_point_a = mCenterOfMassTransform2 * contact_point_a;
		contact_point_b = mCenterOfMassTransform2 * contact_point_b;
		Vec3 contact_normal_world = mCenterOfMassTransform2.Multiply3x3(contact_normal);

		// Its a hit, store the sub shape id's
		ShapeCastResult result(fraction, contact_point_a, contact_point_b, contact_normal_world, back_facing, mSubShapeIDCreator1.GetID(), inSubShapeID2, TransformedShape::sGetBodyID(mCollector.GetContext()));

		// Early out if this hit is deeper than the collector's early out value
		if (fraction == 0.0f && -result.mPenetrationDepth >= mCollector.GetEarlyOutFraction())
			return;

		// Gather faces
		if (mShapeCastSettings.mCollectFacesMode == ECollectFacesMode::CollectFaces)
		{
			// Get supporting face of shape 1
			Mat44 transform_1_to_2 = mShapeCast.mCenterOfMassStart;
			transform_1_to_2.SetTranslation(transform_1_to_2.GetTranslation() + fraction * mShapeCast.mDirection);
			static_cast<const ConvexShape *>(mShapeCast.mShape)->GetSupportingFace(SubShapeID(), transform_1_to_2.Multiply3x3Transposed(-contact_normal), mShapeCast.mScale, mCenterOfMassTransform2 * transform_1_to_2, result.mShape1Face);

			// Get face of the triangle
			triangle.GetSupportingFace(contact_normal, result.mShape2Face);

			// Convert to world space
			for (Vec3 &p : result.mShape2Face)
				p = mCenterOfMassTransform2 * p;
		}

		JPH_IF_TRACK_NARROWPHASE_STATS(TrackNarrowPhaseCollector track;)
		mCollector.AddHit(result);
	}
}

JPH_NAMESPACE_END
