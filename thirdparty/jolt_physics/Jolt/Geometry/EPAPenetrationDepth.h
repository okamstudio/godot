// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/StaticArray.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Geometry/GJKClosestPoint.h>
#include <Jolt/Geometry/EPAConvexHullBuilder.h>

//#define JPH_EPA_PENETRATION_DEPTH_DEBUG

JPH_NAMESPACE_BEGIN

/// Implementation of Expanding Polytope Algorithm as described in:
///
/// Proximity Queries and Penetration Depth Computation on 3D Game Objects - Gino van den Bergen
///
/// The implementation of this algorithm does not completely follow the article, instead of splitting
/// triangles at each edge as in fig. 7 in the article, we build a convex hull (removing any triangles that
/// are facing the new point, thereby avoiding the problem of getting really oblong triangles as mentioned in
/// the article).
///
/// The algorithm roughly works like:
///
/// - Start with a simplex of the Minkowski sum (difference) of two objects that was calculated by GJK
/// - This simplex should contain the origin (or else GJK would have reported: no collision)
/// - In cases where the simplex consists of 1 - 3 points, find some extra support points (of the Minkowski sum) to get to at least 4 points
/// - Convert this into a convex hull with non-zero volume (which includes the origin)
/// - A: Calculate the closest point to the origin for all triangles of the hull and take the closest one
/// - Calculate a new support point (of the Minkowski sum) in this direction and add this point to the convex hull
/// - This will remove all faces that are facing the new point and will create new triangles to fill up the hole
/// - Loop to A until no closer point found
/// - The closest point indicates the position / direction of least penetration
class EPAPenetrationDepth
{
private:
	// Typedefs
	static constexpr int cMaxPoints = EPAConvexHullBuilder::cMaxPoints;
	static constexpr int cMaxPointsToIncludeOriginInHull = 32;
	static_assert(cMaxPointsToIncludeOriginInHull < cMaxPoints);

	using Triangle = EPAConvexHullBuilder::Triangle;
	using Points = EPAConvexHullBuilder::Points;

	/// The GJK algorithm, used to start the EPA algorithm
	GJKClosestPoint		mGJK;

#ifdef JPH_ENABLE_ASSERTS
	/// Tolerance as passed to the GJK algorithm, used for asserting.
	float				mGJKTolerance = 0.0f;
#endif // JPH_ENABLE_ASSERTS

	/// A list of support points for the EPA algorithm
	class SupportPoints
	{
	public:
		/// List of support points
		Points			mY;
		Vec3			mP[cMaxPoints];
		Vec3			mQ[cMaxPoints];

		/// Calculate and add new support point to the list of points
		template <typename A, typename B>
		Vec3			Add(const A &inA, const B &inB, Vec3Arg inDirection, int &outIndex)
		{
			// Get support point of the minkowski sum A - B
			Vec3 p = inA.GetSupport(inDirection);
			Vec3 q = inB.GetSupport(-inDirection);
			Vec3 w = p - q;

			// Store new point
			outIndex = mY.size();
			mY.push_back(w);
			mP[outIndex] = p;
			mQ[outIndex] = q;

			return w;
		}
	};

public:
	/// Return code for GetPenetrationDepthStepGJK
	enum class EStatus
	{
		NotColliding,		///< Returned if the objects don't collide, in this case outPointA/outPointB are invalid
		Colliding,			///< Returned if the objects penetrate
		Indeterminate		///< Returned if the objects penetrate further than the convex radius. In this case you need to call GetPenetrationDepthStepEPA to get the actual penetration depth.
	};

	/// Calculates penetration depth between two objects, first step of two (the GJK step)
	///
	/// @param inAExcludingConvexRadius Object A without convex radius.
	/// @param inBExcludingConvexRadius Object B without convex radius.
	/// @param inConvexRadiusA Convex radius for A.
	/// @param inConvexRadiusB Convex radius for B.
	/// @param ioV Pass in previously returned value or (1, 0, 0). On return this value is changed to direction to move B out of collision along the shortest path (magnitude is meaningless).
	/// @param inTolerance Minimal distance before A and B are considered colliding.
	/// @param outPointA Position on A that has the least amount of penetration.
	/// @param outPointB Position on B that has the least amount of penetration.
	/// Use |outPointB - outPointA| to get the distance of penetration.
	template <typename AE, typename BE>
	EStatus				GetPenetrationDepthStepGJK(const AE &inAExcludingConvexRadius, float inConvexRadiusA, const BE &inBExcludingConvexRadius, float inConvexRadiusB, float inTolerance, Vec3 &ioV, Vec3 &outPointA, Vec3 &outPointB)
	{
		JPH_PROFILE_FUNCTION();

		JPH_IF_ENABLE_ASSERTS(mGJKTolerance = inTolerance;)

		// Don't supply a zero ioV, we only want to get points on the hull of the Minkowsky sum and not internal points.
		//
		// Note that if the assert below triggers, it is very likely that you have a MeshShape that contains a degenerate triangle (e.g. a sliver).
		// Go up a couple of levels in the call stack to see if we're indeed testing a triangle and if it is degenerate.
		// If this is the case then fix the triangles you supply to the MeshShape.
		JPH_ASSERT(!ioV.IsNearZero());

		// Get closest points
		float combined_radius = inConvexRadiusA + inConvexRadiusB;
		float combined_radius_sq = combined_radius * combined_radius;
		float closest_points_dist_sq = mGJK.GetClosestPoints(inAExcludingConvexRadius, inBExcludingConvexRadius, inTolerance, combined_radius_sq, ioV, outPointA, outPointB);
		if (closest_points_dist_sq > combined_radius_sq)
		{
			// No collision
			return EStatus::NotColliding;
		}
		if (closest_points_dist_sq > 0.0f)
		{
			// Collision within convex radius, adjust points for convex radius
			float v_len = sqrt(closest_points_dist_sq); // GetClosestPoints function returns |ioV|^2 when return value < FLT_MAX
			outPointA += ioV * (inConvexRadiusA / v_len);
			outPointB -= ioV * (inConvexRadiusB / v_len);
			return EStatus::Colliding;
		}

		return EStatus::Indeterminate;
	}

	/// Calculates penetration depth between two objects, second step (the EPA step)
	///
	/// @param inAIncludingConvexRadius Object A with convex radius
	/// @param inBIncludingConvexRadius Object B with convex radius
	/// @param inTolerance A factor that determines the accuracy of the result. If the change of the squared distance is less than inTolerance * current_penetration_depth^2 the algorithm will terminate. Should be bigger or equal to FLT_EPSILON.
	/// @param outV Direction to move B out of collision along the shortest path (magnitude is meaningless)
	/// @param outPointA Position on A that has the least amount of penetration
	/// @param outPointB Position on B that has the least amount of penetration
	/// Use |outPointB - outPointA| to get the distance of penetration
	///
	/// @return False if the objects don't collide, in this case outPointA/outPointB are invalid.
	/// True if the objects penetrate
	template <typename AI, typename BI>
	bool				GetPenetrationDepthStepEPA(const AI &inAIncludingConvexRadius, const BI &inBIncludingConvexRadius, float inTolerance, Vec3 &outV, Vec3 &outPointA, Vec3 &outPointB)
	{
		JPH_PROFILE_FUNCTION();

		// Check that the tolerance makes sense (smaller value than this will just result in needless iterations)
		JPH_ASSERT(inTolerance >= FLT_EPSILON);

		// Fetch the simplex from GJK algorithm
		SupportPoints support_points;
		mGJK.GetClosestPointsSimplex(support_points.mY.data(), support_points.mP, support_points.mQ, support_points.mY.GetSizeRef());

		// Fill up the amount of support points to 4
		switch (support_points.mY.size())
		{
		case 1:
			{
				// 1 vertex, which must be at the origin, which is useless for our purpose
				JPH_ASSERT(support_points.mY[0].IsNearZero(Square(mGJKTolerance)));
				support_points.mY.pop_back();

				// Add support points in 4 directions to form a tetrahedron around the origin
				int p1, p2, p3, p4;
				(void)support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, Vec3(0, 1, 0), p1);
				(void)support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, Vec3(-1, -1, -1), p2);
				(void)support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, Vec3(1, -1, -1), p3);
				(void)support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, Vec3(0, -1, 1), p4);
				JPH_ASSERT(p1 == 0);
				JPH_ASSERT(p2 == 1);
				JPH_ASSERT(p3 == 2);
				JPH_ASSERT(p4 == 3);
				break;
			}

		case 2:
			{
				// Two vertices, create 3 extra by taking perpendicular axis and rotating it around in 120 degree increments
				Vec3 axis = (support_points.mY[1] - support_points.mY[0]).Normalized();
				Mat44 rotation = Mat44::sRotation(axis, DegreesToRadians(120.0f));
				Vec3 dir1 = axis.GetNormalizedPerpendicular();
				Vec3 dir2 = rotation * dir1;
				Vec3 dir3 = rotation * dir2;
				int p1, p2, p3;
				(void)support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, dir1, p1);
				(void)support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, dir2, p2);
				(void)support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, dir3, p3);
				JPH_ASSERT(p1 == 2);
				JPH_ASSERT(p2 == 3);
				JPH_ASSERT(p3 == 4);
				break;
			}

		case 3:
		case 4:
			// We already have enough points
			break;
		}

		// Create hull out of the initial points
		JPH_ASSERT(support_points.mY.size() >= 3);
		EPAConvexHullBuilder hull(support_points.mY);
#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		hull.DrawLabel("Build initial hull");
#endif
#ifdef JPH_EPA_PENETRATION_DEPTH_DEBUG
		Trace("Init: num_points = %u", (uint)support_points.mY.size());
#endif
		hull.Initialize(0, 1, 2);
		for (typename Points::size_type i = 3; i < support_points.mY.size(); ++i)
		{
			float dist_sq;
			Triangle *t = hull.FindFacingTriangle(support_points.mY[i], dist_sq);
			if (t != nullptr)
			{
				EPAConvexHullBuilder::NewTriangles new_triangles;
				if (!hull.AddPoint(t, i, FLT_MAX, new_triangles))
				{
					// We can't recover from a failure to add a point to the hull because the old triangles have been unlinked already.
					// Assume no collision. This can happen if the shapes touch in 1 point (or plane) in which case the hull is degenerate.
					return false;
				}
			}
		}

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		hull.DrawLabel("Complete hull");

		// Generate the hull of the Minkowski difference for visualization
		MinkowskiDifference diff(inAIncludingConvexRadius, inBIncludingConvexRadius);
		DebugRenderer::GeometryRef geometry = DebugRenderer::sInstance->CreateTriangleGeometryForConvex([&diff](Vec3Arg inDirection) { return diff.GetSupport(inDirection); });
		hull.DrawGeometry(geometry, Color::sYellow);

		hull.DrawLabel("Ensure origin in hull");
#endif

		// Loop until we are sure that the origin is inside the hull
		for (;;)
		{
			// Get the next closest triangle
			Triangle *t = hull.PeekClosestTriangleInQueue();

			// Don't process removed triangles, just free them (because they're in a heap we don't remove them earlier since we would have to rebuild the sorted heap)
			if (t->mRemoved)
			{
				hull.PopClosestTriangleFromQueue();

				// If we run out of triangles, we couldn't include the origin in the hull so there must be very little penetration and we report no collision.
				if (!hull.HasNextTriangle())
					return false;

				hull.FreeTriangle(t);
				continue;
			}

			// If the closest to the triangle is zero or positive, the origin is in the hull and we can proceed to the main algorithm
			if (t->mClosestLenSq >= 0.0f)
				break;

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
			hull.DrawLabel("Next iteration");
#endif
#ifdef JPH_EPA_PENETRATION_DEPTH_DEBUG
			Trace("EncapsulateOrigin: verts = (%d, %d, %d), closest_dist_sq = %g, centroid = (%g, %g, %g), normal = (%g, %g, %g)",
				t->mEdge[0].mStartIdx, t->mEdge[1].mStartIdx, t->mEdge[2].mStartIdx,
				t->mClosestLenSq,
				t->mCentroid.GetX(), t->mCentroid.GetY(), t->mCentroid.GetZ(),
				t->mNormal.GetX(), t->mNormal.GetY(), t->mNormal.GetZ());
#endif

			// Remove the triangle from the queue before we start adding new ones (which may result in a new closest triangle at the front of the queue)
			hull.PopClosestTriangleFromQueue();

			// Add a support point to get the origin inside the hull
			int new_index;
			Vec3 w = support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, t->mNormal, new_index);

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
			// Draw the point that we're adding
			hull.DrawMarker(w, Color::sRed, 1.0f);
			hull.DrawWireTriangle(*t, Color::sRed);
			hull.DrawState();
#endif

			// Add the point to the hull, if we fail we terminate and report no collision
			EPAConvexHullBuilder::NewTriangles new_triangles;
			if (!t->IsFacing(w) || !hull.AddPoint(t, new_index, FLT_MAX, new_triangles))
				return false;

			// The triangle is facing the support point "w" and can now be safely removed
			JPH_ASSERT(t->mRemoved);
			hull.FreeTriangle(t);

			// If we run out of triangles or points, we couldn't include the origin in the hull so there must be very little penetration and we report no collision.
			if (!hull.HasNextTriangle() || support_points.mY.size() >= cMaxPointsToIncludeOriginInHull)
				return false;
		}

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		hull.DrawLabel("Main algorithm");
#endif

		// Current closest distance to origin
		float closest_dist_sq = FLT_MAX;

		// Remember last good triangle
		Triangle *last = nullptr;

		// If we want to flip the penetration depth
		bool flip_v_sign = false;

		// Loop until closest point found
		do
		{
			// Get closest triangle to the origin
			Triangle *t = hull.PopClosestTriangleFromQueue();

			// Don't process removed triangles, just free them (because they're in a heap we don't remove them earlier since we would have to rebuild the sorted heap)
			if (t->mRemoved)
			{
				hull.FreeTriangle(t);
				continue;
			}

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
			hull.DrawLabel("Next iteration");
#endif
#ifdef JPH_EPA_PENETRATION_DEPTH_DEBUG
			Trace("FindClosest: verts = (%d, %d, %d), closest_len_sq = %g, centroid = (%g, %g, %g), normal = (%g, %g, %g)",
				t->mEdge[0].mStartIdx, t->mEdge[1].mStartIdx, t->mEdge[2].mStartIdx,
				t->mClosestLenSq,
				t->mCentroid.GetX(), t->mCentroid.GetY(), t->mCentroid.GetZ(),
				t->mNormal.GetX(), t->mNormal.GetY(), t->mNormal.GetZ());
#endif
			// Check if next triangle is further away than closest point, we've found the closest point
			if (t->mClosestLenSq >= closest_dist_sq)
				break;

			// Replace last good with this triangle
			if (last != nullptr)
				hull.FreeTriangle(last);
			last = t;

			// Add support point in direction of normal of the plane
			// Note that the article uses the closest point between the origin and plane, but this always has the exact same direction as the normal (if the origin is behind the plane)
			// and this way we do less calculations and lose less precision
			int new_index;
			Vec3 w = support_points.Add(inAIncludingConvexRadius, inBIncludingConvexRadius, t->mNormal, new_index);

			// Project w onto the triangle normal
			float dot = t->mNormal.Dot(w);

			// Check if we just found a separating axis. This can happen if the shape shrunk by convex radius and then expanded by
			// convex radius is bigger then the original shape due to inaccuracies in the shrinking process.
			if (dot < 0.0f)
				return false;

			// Get the distance squared (along normal) to the support point
			float dist_sq = Square(dot) / t->mNormal.LengthSq();

#ifdef JPH_EPA_PENETRATION_DEPTH_DEBUG
			Trace("FindClosest: w = (%g, %g, %g), dot = %g, dist_sq = %g",
				w.GetX(), w.GetY(), w.GetZ(),
				dot, dist_sq);
#endif
#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
			// Draw the point that we're adding
			hull.DrawMarker(w, Color::sPurple, 1.0f);
			hull.DrawWireTriangle(*t, Color::sPurple);
			hull.DrawState();
#endif

			// If the error became small enough, we've converged
			if (dist_sq - t->mClosestLenSq < t->mClosestLenSq * inTolerance)
			{
#ifdef JPH_EPA_PENETRATION_DEPTH_DEBUG
				Trace("Converged");
#endif // JPH_EPA_PENETRATION_DEPTH_DEBUG
				break;
			}

			// Keep track of the minimum distance
			closest_dist_sq = min(closest_dist_sq, dist_sq);

			// If the triangle thinks this point is not front facing, we've reached numerical precision and we're done
			if (!t->IsFacing(w))
			{
#ifdef JPH_EPA_PENETRATION_DEPTH_DEBUG
				Trace("Not facing triangle");
#endif // JPH_EPA_PENETRATION_DEPTH_DEBUG
				break;
			}

			// Add point to hull
			EPAConvexHullBuilder::NewTriangles new_triangles;
			if (!hull.AddPoint(t, new_index, closest_dist_sq, new_triangles))
			{
#ifdef JPH_EPA_PENETRATION_DEPTH_DEBUG
				Trace("Could not add point");
#endif // JPH_EPA_PENETRATION_DEPTH_DEBUG
				break;
			}

			// If the hull is starting to form defects then we're reaching numerical precision and we have to stop
			bool has_defect = false;
			for (const Triangle *nt : new_triangles)
				if (nt->IsFacingOrigin())
				{
					has_defect = true;
					break;
				}
			if (has_defect)
			{
#ifdef JPH_EPA_PENETRATION_DEPTH_DEBUG
				Trace("Has defect");
#endif // JPH_EPA_PENETRATION_DEPTH_DEBUG
				// When the hull has defects it is possible that the origin has been classified on the wrong side of the triangle
				// so we do an additional check to see if the penetration in the -triangle normal direction is smaller than
				// the penetration in the triangle normal direction. If so we must flip the sign of the penetration depth.
				Vec3 w2 = inAIncludingConvexRadius.GetSupport(-t->mNormal) - inBIncludingConvexRadius.GetSupport(t->mNormal);
				float dot2 = -t->mNormal.Dot(w2);
				if (dot2 < dot)
					flip_v_sign = true;
				break;
			}
		}
		while (hull.HasNextTriangle() && support_points.mY.size() < cMaxPoints);

		// Determine closest points, if last == null it means the hull was a plane so there's no penetration
		if (last == nullptr)
			return false;

#ifdef JPH_EPA_CONVEX_BUILDER_DRAW
		hull.DrawLabel("Closest found");
		hull.DrawWireTriangle(*last, Color::sWhite);
		hull.DrawArrow(last->mCentroid, last->mCentroid + last->mNormal.NormalizedOr(Vec3::sZero()), Color::sWhite, 0.1f);
		hull.DrawState();
#endif

		// Calculate penetration by getting the vector from the origin to the closest point on the triangle:
		// distance = (centroid - origin) . normal / |normal|, closest = origin + distance * normal / |normal|
		outV = (last->mCentroid.Dot(last->mNormal) / last->mNormal.LengthSq()) * last->mNormal;

		// If penetration is near zero, treat this as a non collision since we cannot find a good normal
		if (outV.IsNearZero())
			return false;

		// Check if we have to flip the sign of the penetration depth
		if (flip_v_sign)
			outV = -outV;

		// Use the barycentric coordinates for the closest point to the origin to find the contact points on A and B
		Vec3 p0 = support_points.mP[last->mEdge[0].mStartIdx];
		Vec3 p1 = support_points.mP[last->mEdge[1].mStartIdx];
		Vec3 p2 = support_points.mP[last->mEdge[2].mStartIdx];

		Vec3 q0 = support_points.mQ[last->mEdge[0].mStartIdx];
		Vec3 q1 = support_points.mQ[last->mEdge[1].mStartIdx];
		Vec3 q2 = support_points.mQ[last->mEdge[2].mStartIdx];

		if (last->mLambdaRelativeTo0)
		{
			// y0 was the reference vertex
			outPointA = p0 + last->mLambda[0] * (p1 - p0) + last->mLambda[1] * (p2 - p0);
			outPointB = q0 + last->mLambda[0] * (q1 - q0) + last->mLambda[1] * (q2 - q0);
		}
		else
		{
			// y1 was the reference vertex
			outPointA = p1 + last->mLambda[0] * (p0 - p1) + last->mLambda[1] * (p2 - p1);
			outPointB = q1 + last->mLambda[0] * (q0 - q1) + last->mLambda[1] * (q2 - q1);
		}

		return true;
	}

	/// This function combines the GJK and EPA steps and is provided as a convenience function.
	/// Note: less performant since you're providing all support functions in one go
	/// Note 2: You need to initialize ioV, see documentation at GetPenetrationDepthStepGJK!
	template <typename AE, typename AI, typename BE, typename BI>
	bool				GetPenetrationDepth(const AE &inAExcludingConvexRadius, const AI &inAIncludingConvexRadius, float inConvexRadiusA, const BE &inBExcludingConvexRadius, const BI &inBIncludingConvexRadius, float inConvexRadiusB, float inCollisionToleranceSq, float inPenetrationTolerance, Vec3 &ioV, Vec3 &outPointA, Vec3 &outPointB)
	{
		// Check result of collision detection
		switch (GetPenetrationDepthStepGJK(inAExcludingConvexRadius, inConvexRadiusA, inBExcludingConvexRadius, inConvexRadiusB, inCollisionToleranceSq, ioV, outPointA, outPointB))
		{
		case EPAPenetrationDepth::EStatus::Colliding:
			return true;

		case EPAPenetrationDepth::EStatus::NotColliding:
			return false;

		case EPAPenetrationDepth::EStatus::Indeterminate:
			return GetPenetrationDepthStepEPA(inAIncludingConvexRadius, inBIncludingConvexRadius, inPenetrationTolerance, ioV, outPointA, outPointB);
		}

		JPH_ASSERT(false);
		return false;
	}

	/// Test if a cast shape inA moving from inStart to lambda * inStart.GetTranslation() + inDirection where lambda e [0, ioLambda> intersects inB
	///
	/// @param inStart Start position and orientation of the convex object
	/// @param inDirection Direction of the sweep (ioLambda * inDirection determines length)
	///	@param inCollisionTolerance The minimal distance between A and B before they are considered colliding
	/// @param inPenetrationTolerance A factor that determines the accuracy of the result. If the change of the squared distance is less than inTolerance * current_penetration_depth^2 the algorithm will terminate. Should be bigger or equal to FLT_EPSILON.
	/// @param inA The convex object A, must support the GetSupport(Vec3) function.
	/// @param inB The convex object B, must support the GetSupport(Vec3) function.
	/// @param inConvexRadiusA The convex radius of A, this will be added on all sides to pad A.
	/// @param inConvexRadiusB The convex radius of B, this will be added on all sides to pad B.
	/// @param inReturnDeepestPoint If the shapes are initially intersecting this determines if the EPA algorithm will run to find the deepest point
	/// @param ioLambda The max fraction along the sweep, on output updated with the actual collision fraction.
	///	@param outPointA is the contact point on A
	///	@param outPointB is the contact point on B
	/// @param outContactNormal is either the contact normal when the objects are touching or the penetration axis when the objects are penetrating at the start of the sweep (pointing from A to B, length will not be 1)
	///
	/// @return true if the a hit was found, in which case ioLambda, outPointA, outPointB and outSurfaceNormal are updated.
	template <typename A, typename B>
	bool				CastShape(Mat44Arg inStart, Vec3Arg inDirection, float inCollisionTolerance, float inPenetrationTolerance, const A &inA, const B &inB, float inConvexRadiusA, float inConvexRadiusB, bool inReturnDeepestPoint, float &ioLambda, Vec3 &outPointA, Vec3 &outPointB, Vec3 &outContactNormal)
	{
		JPH_IF_ENABLE_ASSERTS(mGJKTolerance = inCollisionTolerance;)

		// First determine if there's a collision at all
		if (!mGJK.CastShape(inStart, inDirection, inCollisionTolerance, inA, inB, inConvexRadiusA, inConvexRadiusB, ioLambda, outPointA, outPointB, outContactNormal))
			return false;

		// When our contact normal is too small, we don't have an accurate result
		bool contact_normal_invalid = outContactNormal.IsNearZero(Square(inCollisionTolerance));

		if (inReturnDeepestPoint
			&& ioLambda == 0.0f // Only when lambda = 0 we can have the bodies overlap
			&& (inConvexRadiusA + inConvexRadiusB == 0.0f // When no convex radius was provided we can never trust contact points at lambda = 0
				|| contact_normal_invalid))
		{
			// If we're initially intersecting, we need to run the EPA algorithm in order to find the deepest contact point
			AddConvexRadius<A> add_convex_a(inA, inConvexRadiusA);
			AddConvexRadius<B> add_convex_b(inB, inConvexRadiusB);
			TransformedConvexObject<AddConvexRadius<A>> transformed_a(inStart, add_convex_a);
			if (!GetPenetrationDepthStepEPA(transformed_a, add_convex_b, inPenetrationTolerance, outContactNormal, outPointA, outPointB))
				return false;
		}
		else if (contact_normal_invalid)
		{
			// If we weren't able to calculate a contact normal, use the cast direction instead
			outContactNormal = inDirection;
		}

		return true;
	}
};

JPH_NAMESPACE_END
