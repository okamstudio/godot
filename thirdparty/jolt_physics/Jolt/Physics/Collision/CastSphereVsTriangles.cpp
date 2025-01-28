// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/CastSphereVsTriangles.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ActiveEdges.h>
#include <Jolt/Physics/Collision/NarrowPhaseStats.h>
#include <Jolt/Geometry/ClosestPoint.h>
#include <Jolt/Geometry/RaySphere.h>
#include <Jolt/Core/Profiler.h>

JPH_NAMESPACE_BEGIN

CastSphereVsTriangles::CastSphereVsTriangles(const ShapeCast &inShapeCast, const ShapeCastSettings &inShapeCastSettings, Vec3Arg inScale, Mat44Arg inCenterOfMassTransform2, const SubShapeIDCreator &inSubShapeIDCreator1, CastShapeCollector &ioCollector) :
	mStart(inShapeCast.mCenterOfMassStart.GetTranslation()),
	mDirection(inShapeCast.mDirection),
	mShapeCastSettings(inShapeCastSettings),
	mCenterOfMassTransform2(inCenterOfMassTransform2),
	mScale(inScale),
	mSubShapeIDCreator1(inSubShapeIDCreator1),
	mCollector(ioCollector)
{
	// Cast to sphere shape
	JPH_ASSERT(inShapeCast.mShape->GetSubType() == EShapeSubType::Sphere);
	const SphereShape *sphere = static_cast<const SphereShape *>(inShapeCast.mShape);

	// Scale the radius
	mRadius = sphere->GetRadius() * abs(inShapeCast.mScale.GetX());

	// Determine if shape is inside out or not
	mScaleSign = ScaleHelpers::IsInsideOut(inScale)? -1.0f : 1.0f;
}

void CastSphereVsTriangles::AddHit(bool inBackFacing, const SubShapeID &inSubShapeID2, float inFraction, Vec3Arg inContactPointA, Vec3Arg inContactPointB, Vec3Arg inContactNormal)
{
	// Convert to world space
	Vec3 contact_point_a = mCenterOfMassTransform2 * (mStart + inContactPointA);
	Vec3 contact_point_b = mCenterOfMassTransform2 * (mStart + inContactPointB);
	Vec3 contact_normal_world = mCenterOfMassTransform2.Multiply3x3(inContactNormal);

	// Its a hit, store the sub shape id's
	ShapeCastResult result(inFraction, contact_point_a, contact_point_b, contact_normal_world, inBackFacing, mSubShapeIDCreator1.GetID(), inSubShapeID2, TransformedShape::sGetBodyID(mCollector.GetContext()));

	// Note: We don't gather faces here because that's only useful if both shapes have a face. Since the sphere always has only 1 contact point, the manifold is always a point.

	JPH_IF_TRACK_NARROWPHASE_STATS(TrackNarrowPhaseCollector track;)
	mCollector.AddHit(result);
}

void CastSphereVsTriangles::AddHitWithActiveEdgeDetection(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, bool inBackFacing, Vec3Arg inTriangleNormal, uint8 inActiveEdges, const SubShapeID &inSubShapeID2, float inFraction, Vec3Arg inContactPointA, Vec3Arg inContactPointB, Vec3Arg inContactNormal)
{
	// Check if we have enabled active edge detection
	Vec3 contact_normal = inContactNormal;
	if (mShapeCastSettings.mActiveEdgeMode == EActiveEdgeMode::CollideOnlyWithActive && inActiveEdges != 0b111)
	{
		// Convert the active edge velocity hint to local space
		Vec3 active_edge_movement_direction = mCenterOfMassTransform2.Multiply3x3Transposed(mShapeCastSettings.mActiveEdgeMovementDirection);

		// Update the contact normal to account for active edges
		// Note that we flip the triangle normal as the penetration axis is pointing towards the triangle instead of away
		contact_normal = ActiveEdges::FixNormal(inV0, inV1, inV2, inBackFacing? inTriangleNormal : -inTriangleNormal, inActiveEdges, inContactPointB, inContactNormal, active_edge_movement_direction);
	}

	AddHit(inBackFacing, inSubShapeID2, inFraction, inContactPointA, inContactPointB, contact_normal);
}

// This is a simplified version of the ray cylinder test from: Real Time Collision Detection - Christer Ericson
// Chapter 5.3.7, page 194-197. Some conditions have been removed as we're not interested in hitting the caps of the cylinder.
// Note that the ray origin is assumed to be the origin here.
float CastSphereVsTriangles::RayCylinder(Vec3Arg inRayDirection, Vec3Arg inCylinderA, Vec3Arg inCylinderB, float inRadius) const
{
	// Calculate cylinder axis
	Vec3 axis = inCylinderB - inCylinderA;

	// Make ray start relative to cylinder side A (moving cylinder A to the origin)
	Vec3 start = -inCylinderA;

	// Test if segment is fully on the A side of the cylinder
	float start_dot_axis = start.Dot(axis);
	float direction_dot_axis = inRayDirection.Dot(axis);
	float end_dot_axis = start_dot_axis + direction_dot_axis;
	if (start_dot_axis < 0.0f && end_dot_axis < 0.0f)
		return FLT_MAX;

	// Test if segment is fully on the B side of the cylinder
	float axis_len_sq = axis.LengthSq();
	if (start_dot_axis > axis_len_sq && end_dot_axis > axis_len_sq)
		return FLT_MAX;

	// Calculate a, b and c, the factors for quadratic equation
	// We're basically solving the ray: x = start + direction * t
	// The closest point to x on the segment A B is: w = (x . axis) * axis / (axis . axis)
	// The distance between x and w should be radius: (x - w) . (x - w) = radius^2
	// Solving this gives the following:
	float a = axis_len_sq * inRayDirection.LengthSq() - Square(direction_dot_axis);
	if (abs(a) < 1.0e-6f)
		return FLT_MAX; // Segment runs parallel to cylinder axis, stop processing, we will either hit at fraction = 0 or we'll hit a vertex
	float b = axis_len_sq * start.Dot(inRayDirection) - direction_dot_axis * start_dot_axis; // should be multiplied by 2, instead we'll divide a and c by 2 when we solve the quadratic equation
	float c = axis_len_sq * (start.LengthSq() - Square(inRadius)) - Square(start_dot_axis);
	float det = Square(b) - a * c; // normally 4 * a * c but since both a and c need to be divided by 2 we lose the 4
	if (det < 0.0f)
		return FLT_MAX; // No solution to quadratic equation

	// Solve fraction t where the ray hits the cylinder
	float t = -(b + sqrt(det)) / a; // normally divided by 2 * a but since a should be divided by 2 we lose the 2
	if (t < 0.0f || t > 1.0f)
		return FLT_MAX; // Intersection lies outside segment
	if (start_dot_axis + t * direction_dot_axis < 0.0f || start_dot_axis + t * direction_dot_axis > axis_len_sq)
		return FLT_MAX; // Intersection outside the end point of the cylinder, stop processing, we will possibly hit a vertex
	return t;
}

void CastSphereVsTriangles::Cast(Vec3Arg inV0, Vec3Arg inV1, Vec3Arg inV2, uint8 inActiveEdges, const SubShapeID &inSubShapeID2)
{
	JPH_PROFILE_FUNCTION();

	// Scale triangle and make it relative to the start of the cast
	Vec3 v0 = mScale * inV0 - mStart;
	Vec3 v1 = mScale * inV1 - mStart;
	Vec3 v2 = mScale * inV2 - mStart;

	// Calculate triangle normal
	Vec3 triangle_normal = mScaleSign * (v1 - v0).Cross(v2 - v0);
	float triangle_normal_len = triangle_normal.Length();
	if (triangle_normal_len == 0.0f)
		return; // Degenerate triangle
	triangle_normal /= triangle_normal_len;

	// Backface check
	float normal_dot_direction = triangle_normal.Dot(mDirection);
	bool back_facing = normal_dot_direction > 0.0f;
	if (mShapeCastSettings.mBackFaceModeTriangles == EBackFaceMode::IgnoreBackFaces && back_facing)
		return;

	// Test if distance between the sphere and plane of triangle is smaller or equal than the radius
	if (abs(v0.Dot(triangle_normal)) <= mRadius)
	{
		// Check if the sphere intersects at the start of the cast
		uint32 closest_feature;
		Vec3 q = ClosestPoint::GetClosestPointOnTriangle(v0, v1, v2, closest_feature);
		float q_len_sq = q.LengthSq();
		if (q_len_sq <= Square(mRadius))
		{
			// Early out if this hit is deeper than the collector's early out value
			float q_len = sqrt(q_len_sq);
			float penetration_depth = mRadius - q_len;
			if (-penetration_depth >= mCollector.GetEarlyOutFraction())
				return;

			// Generate contact point
			Vec3 contact_normal = q_len > 0.0f? q / q_len : Vec3::sAxisY();
			Vec3 contact_point_a = q + contact_normal * penetration_depth;
			Vec3 contact_point_b = q;
			AddHitWithActiveEdgeDetection(v0, v1, v2, back_facing, triangle_normal, inActiveEdges, inSubShapeID2, 0.0f, contact_point_a, contact_point_b, contact_normal);
			return;
		}
	}
	else
	{
		// Check if cast is not parallel to the plane of the triangle
		float abs_normal_dot_direction = abs(normal_dot_direction);
		if (abs_normal_dot_direction > 1.0e-6f)
		{
			// Calculate the point on the sphere that will hit the triangle's plane first and calculate a fraction where it will do so
			Vec3 d = Sign(normal_dot_direction) * mRadius * triangle_normal;
			float plane_intersection = (v0 - d).Dot(triangle_normal) / normal_dot_direction;

			// Check if sphere will hit in the interval that we're interested in
			if (plane_intersection * abs_normal_dot_direction < -mRadius	// Sphere hits the plane before the sweep, cannot intersect
				|| plane_intersection >= mCollector.GetEarlyOutFraction())	// Sphere hits the plane after the sweep / early out fraction, cannot intersect
				return;

			// We can only report an interior hit if we're hitting the plane during our sweep and not before
			if (plane_intersection >= 0.0f)
			{
				// Calculate the point of contact on the plane
				Vec3 p = d + plane_intersection * mDirection;

				// Check if this is an interior point
				float u, v, w;
				if (ClosestPoint::GetBaryCentricCoordinates(v0 - p, v1 - p, v2 - p, u, v, w)
					&& u >= 0.0f && v >= 0.0f && w >= 0.0f)
				{
					// Interior point, we found the collision point. We don't need to check active edges.
					AddHit(back_facing, inSubShapeID2, plane_intersection, p, p, back_facing? triangle_normal : -triangle_normal);
					return;
				}
			}
		}
	}

	// Test 3 edges
	float fraction = RayCylinder(mDirection, v0, v1, mRadius);
	fraction = min(fraction, RayCylinder(mDirection, v1, v2, mRadius));
	fraction = min(fraction, RayCylinder(mDirection, v2, v0, mRadius));

	// Test 3 vertices
	fraction = min(fraction, RaySphere(Vec3::sZero(), mDirection, v0, mRadius));
	fraction = min(fraction, RaySphere(Vec3::sZero(), mDirection, v1, mRadius));
	fraction = min(fraction, RaySphere(Vec3::sZero(), mDirection, v2, mRadius));

	// Check if we have a collision
	JPH_ASSERT(fraction >= 0.0f);
	if (fraction < mCollector.GetEarlyOutFraction())
	{
		// Calculate the center of the sphere at the point of contact
		Vec3 p = fraction * mDirection;

		// Get contact point and normal
		uint32 closest_feature;
		Vec3 q = ClosestPoint::GetClosestPointOnTriangle(v0 - p, v1 - p, v2 - p, closest_feature);
		Vec3 contact_normal = q.Normalized();
		Vec3 contact_point_ab = p + q;
		AddHitWithActiveEdgeDetection(v0, v1, v2, back_facing, triangle_normal, inActiveEdges, inSubShapeID2, fraction, contact_point_ab, contact_point_ab, contact_normal);
	}
}

JPH_NAMESPACE_END
