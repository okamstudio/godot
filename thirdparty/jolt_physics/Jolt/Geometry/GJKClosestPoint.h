// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/FPException.h>
#include <Jolt/Geometry/ClosestPoint.h>
#include <Jolt/Geometry/ConvexSupport.h>

//#define JPH_GJK_DEBUG
#ifdef JPH_GJK_DEBUG
	#include <Jolt/Core/StringTools.h>
	#include <Jolt/Renderer/DebugRenderer.h>
#endif

JPH_NAMESPACE_BEGIN

/// Convex vs convex collision detection
/// Based on: A Fast and Robust GJK Implementation for Collision Detection of Convex Objects - Gino van den Bergen
class GJKClosestPoint : public NonCopyable
{
private:
	/// Get new closest point to origin given simplex mY of mNumPoints points
	///
	/// @param inPrevVLenSq Length of |outV|^2 from the previous iteration, used as a maximum value when selecting a new closest point.
	/// @param outV Closest point
	/// @param outVLenSq |outV|^2
	/// @param outSet Set of points that form the new simplex closest to the origin (bit 1 = mY[0], bit 2 = mY[1], ...)
	///
	/// If LastPointPartOfClosestFeature is true then the last point added will be assumed to be part of the closest feature and the function will do less work.
	///
	/// @return True if new closest point was found.
	/// False if the function failed, in this case the output variables are not modified
	template <bool LastPointPartOfClosestFeature>
	bool		GetClosest(float inPrevVLenSq, Vec3 &outV, float &outVLenSq, uint32 &outSet) const
	{
#ifdef JPH_GJK_DEBUG
		for (int i = 0; i < mNumPoints; ++i)
			Trace("y[%d] = [%s], |y[%d]| = %g", i, ConvertToString(mY[i]).c_str(), i, (double)mY[i].Length());
#endif

		uint32 set;
		Vec3 v;

		switch (mNumPoints)
		{
		case 1:
			// Single point
			set = 0b0001;
			v = mY[0];
			break;

		case 2:
			// Line segment
			v = ClosestPoint::GetClosestPointOnLine(mY[0], mY[1], set);
			break;

		case 3:
			// Triangle
			v = ClosestPoint::GetClosestPointOnTriangle<LastPointPartOfClosestFeature>(mY[0], mY[1], mY[2], set);
			break;

		case 4:
			// Tetrahedron
			v = ClosestPoint::GetClosestPointOnTetrahedron<LastPointPartOfClosestFeature>(mY[0], mY[1], mY[2], mY[3], set);
			break;

		default:
			JPH_ASSERT(false);
			return false;
		}

#ifdef JPH_GJK_DEBUG
		Trace("GetClosest: set = 0b%s, v = [%s], |v| = %g", NibbleToBinary(set), ConvertToString(v).c_str(), (double)v.Length());
#endif

		float v_len_sq = v.LengthSq();
		if (v_len_sq < inPrevVLenSq) // Note, comparison order important: If v_len_sq is NaN then this expression will be false so we will return false
		{
			// Return closest point
			outV = v;
			outVLenSq = v_len_sq;
			outSet = set;
			return true;
		}

		// No better match found
#ifdef JPH_GJK_DEBUG
		Trace("New closer point is further away, failed to converge");
#endif
		return false;
	}

	// Get max(|Y_0|^2 .. |Y_n|^2)
	float		GetMaxYLengthSq() const
	{
		float y_len_sq = mY[0].LengthSq();
		for (int i = 1; i < mNumPoints; ++i)
			y_len_sq = max(y_len_sq, mY[i].LengthSq());
		return y_len_sq;
	}

	// Remove points that are not in the set, only updates mY
	void		UpdatePointSetY(uint32 inSet)
	{
		int num_points = 0;
		for (int i = 0; i < mNumPoints; ++i)
			if ((inSet & (1 << i)) != 0)
			{
				mY[num_points] = mY[i];
				++num_points;
			}
		mNumPoints = num_points;
	}

	// Remove points that are not in the set, only updates mP
	void		UpdatePointSetP(uint32 inSet)
	{
		int num_points = 0;
		for (int i = 0; i < mNumPoints; ++i)
			if ((inSet & (1 << i)) != 0)
			{
				mP[num_points] = mP[i];
				++num_points;
			}
		mNumPoints = num_points;
	}

	// Remove points that are not in the set, only updates mP and mQ
	void		UpdatePointSetPQ(uint32 inSet)
	{
		int num_points = 0;
		for (int i = 0; i < mNumPoints; ++i)
			if ((inSet & (1 << i)) != 0)
			{
				mP[num_points] = mP[i];
				mQ[num_points] = mQ[i];
				++num_points;
			}
		mNumPoints = num_points;
	}

	// Remove points that are not in the set, updates mY, mP and mQ
	void		UpdatePointSetYPQ(uint32 inSet)
	{
		int num_points = 0;
		for (int i = 0; i < mNumPoints; ++i)
			if ((inSet & (1 << i)) != 0)
			{
				mY[num_points] = mY[i];
				mP[num_points] = mP[i];
				mQ[num_points] = mQ[i];
				++num_points;
			}
		mNumPoints = num_points;
	}

	// Calculate closest points on A and B
	void		CalculatePointAAndB(Vec3 &outPointA, Vec3 &outPointB) const
	{
		switch (mNumPoints)
		{
		case 1:
			outPointA = mP[0];
			outPointB = mQ[0];
			break;

		case 2:
			{
				float u, v;
				ClosestPoint::GetBaryCentricCoordinates(mY[0], mY[1], u, v);
				outPointA = u * mP[0] + v * mP[1];
				outPointB = u * mQ[0] + v * mQ[1];
			}
			break;

		case 3:
			{
				float u, v, w;
				ClosestPoint::GetBaryCentricCoordinates(mY[0], mY[1], mY[2], u, v, w);
				outPointA = u * mP[0] + v * mP[1] + w * mP[2];
				outPointB = u * mQ[0] + v * mQ[1] + w * mQ[2];
			}
			break;

		case 4:
		#ifdef JPH_DEBUG
			memset(&outPointA, 0xcd, sizeof(outPointA));
			memset(&outPointB, 0xcd, sizeof(outPointB));
		#endif
			break;
		}
	}

public:
	/// Test if inA and inB intersect
	///
	/// @param inA The convex object A, must support the GetSupport(Vec3) function.
	/// @param inB The convex object B, must support the GetSupport(Vec3) function.
	///	@param inTolerance Minimal distance between objects when the objects are considered to be colliding
	///	@param ioV is used as initial separating axis (provide a zero vector if you don't know yet)
	///
	///	@return True if they intersect (in which case ioV = (0, 0, 0)).
	///	False if they don't intersect in which case ioV is a separating axis in the direction from A to B (magnitude is meaningless)
	template <typename A, typename B>
	bool		Intersects(const A &inA, const B &inB, float inTolerance, Vec3 &ioV)
	{
		float tolerance_sq = Square(inTolerance);

		// Reset state
		mNumPoints = 0;

#ifdef JPH_GJK_DEBUG
		for (int i = 0; i < 4; ++i)
			mY[i] = Vec3::sZero();
#endif

		// Previous length^2 of v
		float prev_v_len_sq = FLT_MAX;

		for (;;)
		{
#ifdef JPH_GJK_DEBUG
			Trace("v = [%s], num_points = %d", ConvertToString(ioV).c_str(), mNumPoints);
#endif

			// Get support points for shape A and B in the direction of v
			Vec3 p = inA.GetSupport(ioV);
			Vec3 q = inB.GetSupport(-ioV);

			// Get support point of the minkowski sum A - B of v
			Vec3 w = p - q;

			// If the support point sA-B(v) is in the opposite direction as v, then we have found a separating axis and there is no intersection
			if (ioV.Dot(w) < 0.0f)
			{
				// Separating axis found
#ifdef JPH_GJK_DEBUG
				Trace("Separating axis");
#endif
				return false;
			}

			// Store the point for later use
			mY[mNumPoints] = w;
			++mNumPoints;

#ifdef JPH_GJK_DEBUG
			Trace("w = [%s]", ConvertToString(w).c_str());
#endif

			// Determine the new closest point
			float v_len_sq;			// Length^2 of v
			uint32 set;				// Set of points that form the new simplex
			if (!GetClosest<true>(prev_v_len_sq, ioV, v_len_sq, set))
				return false;

			// If there are 4 points, the origin is inside the tetrahedron and we're done
			if (set == 0xf)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Full simplex");
#endif
				ioV = Vec3::sZero();
				return true;
			}

			// If v is very close to zero, we consider this a collision
			if (v_len_sq <= tolerance_sq)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Distance zero");
#endif
				ioV = Vec3::sZero();
				return true;
			}

			// If v is very small compared to the length of y, we also consider this a collision
			if (v_len_sq <= FLT_EPSILON * GetMaxYLengthSq())
			{
#ifdef JPH_GJK_DEBUG
				Trace("Machine precision reached");
#endif
				ioV = Vec3::sZero();
				return true;
			}

			// The next separation axis to test is the negative of the closest point of the Minkowski sum to the origin
			// Note: This must be done before terminating as converged since the separating axis is -v
			ioV = -ioV;

			// If the squared length of v is not changing enough, we've converged and there is no collision
			JPH_ASSERT(prev_v_len_sq >= v_len_sq);
			if (prev_v_len_sq - v_len_sq <= FLT_EPSILON * prev_v_len_sq)
			{
				// v is a separating axis
#ifdef JPH_GJK_DEBUG
				Trace("Converged");
#endif
				return false;
			}
			prev_v_len_sq = v_len_sq;

			// Update the points of the simplex
			UpdatePointSetY(set);
		}
	}

	/// Get closest points between inA and inB
	///
	/// @param inA The convex object A, must support the GetSupport(Vec3) function.
	/// @param inB The convex object B, must support the GetSupport(Vec3) function.
	///	@param inTolerance The minimal distance between A and B before the objects are considered colliding and processing is terminated.
	///	@param inMaxDistSq The maximum squared distance between A and B before the objects are considered infinitely far away and processing is terminated.
	///	@param ioV Initial guess for the separating axis. Start with any non-zero vector if you don't know.
	///		If return value is 0, ioV = (0, 0, 0).
	///		If the return value is bigger than 0 but smaller than FLT_MAX, ioV will be the separating axis in the direction from A to B and its length the squared distance between A and B.
	///		If the return value is FLT_MAX, ioV will be the separating axis in the direction from A to B and the magnitude of the vector is meaningless.
	///	@param outPointA , outPointB
	///		If the return value is 0 the points are invalid.
	///		If the return value is bigger than 0 but smaller than FLT_MAX these will contain the closest point on A and B.
	///		If the return value is FLT_MAX the points are invalid.
	///
	///	@return The squared distance between A and B or FLT_MAX when they are further away than inMaxDistSq.
	template <typename A, typename B>
	float		GetClosestPoints(const A &inA, const B &inB, float inTolerance, float inMaxDistSq, Vec3 &ioV, Vec3 &outPointA, Vec3 &outPointB)
	{
		float tolerance_sq = Square(inTolerance);

		// Reset state
		mNumPoints = 0;

#ifdef JPH_GJK_DEBUG
		// Generate the hull of the Minkowski difference for visualization
		MinkowskiDifference diff(inA, inB);
		mGeometry = DebugRenderer::sInstance->CreateTriangleGeometryForConvex([&diff](Vec3Arg inDirection) { return diff.GetSupport(inDirection); });

		for (int i = 0; i < 4; ++i)
		{
			mY[i] = Vec3::sZero();
			mP[i] = Vec3::sZero();
			mQ[i] = Vec3::sZero();
		}
#endif

		// Length^2 of v
		float v_len_sq = ioV.LengthSq();

		// Previous length^2 of v
		float prev_v_len_sq = FLT_MAX;

		for (;;)
		{
#ifdef JPH_GJK_DEBUG
			Trace("v = [%s], num_points = %d", ConvertToString(ioV).c_str(), mNumPoints);
#endif

			// Get support points for shape A and B in the direction of v
			Vec3 p = inA.GetSupport(ioV);
			Vec3 q = inB.GetSupport(-ioV);

			// Get support point of the minkowski sum A - B of v
			Vec3 w = p - q;

			float dot = ioV.Dot(w);

#ifdef JPH_GJK_DEBUG
			// Draw -ioV to show the closest point to the origin from the previous simplex
			DebugRenderer::sInstance->DrawArrow(mOffset, mOffset - ioV, Color::sOrange, 0.05f);

			// Draw ioV to show where we're probing next
			DebugRenderer::sInstance->DrawArrow(mOffset, mOffset + ioV, Color::sCyan, 0.05f);

			// Draw w, the support point
			DebugRenderer::sInstance->DrawArrow(mOffset, mOffset + w, Color::sGreen, 0.05f);
			DebugRenderer::sInstance->DrawMarker(mOffset + w, Color::sGreen, 1.0f);

			// Draw the simplex and the Minkowski difference around it
			DrawState();
#endif

			// Test if we have a separation of more than inMaxDistSq, in which case we terminate early
			if (dot < 0.0f && dot * dot > v_len_sq * inMaxDistSq)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Distance bigger than max");
#endif
#ifdef JPH_DEBUG
				memset(&outPointA, 0xcd, sizeof(outPointA));
				memset(&outPointB, 0xcd, sizeof(outPointB));
#endif
				return FLT_MAX;
			}

			// Store the point for later use
			mY[mNumPoints] = w;
			mP[mNumPoints] = p;
			mQ[mNumPoints] = q;
			++mNumPoints;

#ifdef JPH_GJK_DEBUG
			Trace("w = [%s]", ConvertToString(w).c_str());
#endif

			uint32 set;
			if (!GetClosest<true>(prev_v_len_sq, ioV, v_len_sq, set))
			{
				--mNumPoints; // Undo add last point
				break;
			}

			// If there are 4 points, the origin is inside the tetrahedron and we're done
			if (set == 0xf)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Full simplex");
#endif
				ioV = Vec3::sZero();
				v_len_sq = 0.0f;
				break;
			}

			// Update the points of the simplex
			UpdatePointSetYPQ(set);

			// If v is very close to zero, we consider this a collision
			if (v_len_sq <= tolerance_sq)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Distance zero");
#endif
				ioV = Vec3::sZero();
				v_len_sq = 0.0f;
				break;
			}

			// If v is very small compared to the length of y, we also consider this a collision
#ifdef JPH_GJK_DEBUG
			Trace("Check v small compared to y: %g <= %g", (double)v_len_sq, (double)(FLT_EPSILON * GetMaxYLengthSq()));
#endif
			if (v_len_sq <= FLT_EPSILON * GetMaxYLengthSq())
			{
#ifdef JPH_GJK_DEBUG
				Trace("Machine precision reached");
#endif
				ioV = Vec3::sZero();
				v_len_sq = 0.0f;
				break;
			}

			// The next separation axis to test is the negative of the closest point of the Minkowski sum to the origin
			// Note: This must be done before terminating as converged since the separating axis is -v
			ioV = -ioV;

			// If the squared length of v is not changing enough, we've converged and there is no collision
#ifdef JPH_GJK_DEBUG
			Trace("Check v not changing enough: %g <= %g", (double)(prev_v_len_sq - v_len_sq), (double)(FLT_EPSILON * prev_v_len_sq));
#endif
			JPH_ASSERT(prev_v_len_sq >= v_len_sq);
			if (prev_v_len_sq - v_len_sq <= FLT_EPSILON * prev_v_len_sq)
			{
				// v is a separating axis
#ifdef JPH_GJK_DEBUG
				Trace("Converged");
#endif
				break;
			}
			prev_v_len_sq = v_len_sq;
		}

		// Get the closest points
		CalculatePointAAndB(outPointA, outPointB);

#ifdef JPH_GJK_DEBUG
		Trace("Return: v = [%s], |v| = %g", ConvertToString(ioV).c_str(), (double)ioV.Length());

		// Draw -ioV to show the closest point to the origin from the previous simplex
		DebugRenderer::sInstance->DrawArrow(mOffset, mOffset - ioV, Color::sOrange, 0.05f);

		// Draw the closest points
		DebugRenderer::sInstance->DrawMarker(mOffset + outPointA, Color::sGreen, 1.0f);
		DebugRenderer::sInstance->DrawMarker(mOffset + outPointB, Color::sPurple, 1.0f);

		// Draw the simplex and the Minkowski difference around it
		DrawState();
#endif

		JPH_ASSERT(ioV.LengthSq() == v_len_sq);
		return v_len_sq;
	}

	/// Get the resulting simplex after the GetClosestPoints algorithm finishes.
	/// If it returned a squared distance of 0, the origin will be contained in the simplex.
	void		GetClosestPointsSimplex(Vec3 *outY, Vec3 *outP, Vec3 *outQ, uint &outNumPoints) const
	{
		uint size = sizeof(Vec3) * mNumPoints;
		memcpy(outY, mY, size);
		memcpy(outP, mP, size);
		memcpy(outQ, mQ, size);
		outNumPoints = mNumPoints;
	}

	/// Test if a ray inRayOrigin + lambda * inRayDirection for lambda e [0, ioLambda> intersects inA
	///
	/// Code based upon: Ray Casting against General Convex Objects with Application to Continuous Collision Detection - Gino van den Bergen
	///
	/// @param inRayOrigin Origin of the ray
	/// @param inRayDirection Direction of the ray (ioLambda * inDirection determines length)
	///	@param inTolerance The minimal distance between the ray and A before it is considered colliding
	/// @param inA A convex object that has the GetSupport(Vec3) function
	/// @param ioLambda The max fraction along the ray, on output updated with the actual collision fraction.
	///
	///	@return true if a hit was found, ioLambda is the solution for lambda.
	template <typename A>
	bool		CastRay(Vec3Arg inRayOrigin, Vec3Arg inRayDirection, float inTolerance, const A &inA, float &ioLambda)
	{
		float tolerance_sq = Square(inTolerance);

		// Reset state
		mNumPoints = 0;

		float lambda = 0.0f;
		Vec3 x = inRayOrigin;
		Vec3 v = x - inA.GetSupport(Vec3::sZero());
		float v_len_sq = FLT_MAX;
		bool allow_restart = false;

		for (;;)
		{
#ifdef JPH_GJK_DEBUG
			Trace("v = [%s], num_points = %d", ConvertToString(v).c_str(), mNumPoints);
#endif

			// Get new support point
			Vec3 p = inA.GetSupport(v);
			Vec3 w = x - p;

#ifdef JPH_GJK_DEBUG
			Trace("w = [%s]", ConvertToString(w).c_str());
#endif

			float v_dot_w = v.Dot(w);
#ifdef JPH_GJK_DEBUG
			Trace("v . w = %g", (double)v_dot_w);
#endif
			if (v_dot_w > 0.0f)
			{
				// If ray and normal are in the same direction, we've passed A and there's no collision
				float v_dot_r = v.Dot(inRayDirection);
#ifdef JPH_GJK_DEBUG
				Trace("v . r = %g", (double)v_dot_r);
#endif
				if (v_dot_r >= 0.0f)
					return false;

				// Update the lower bound for lambda
				float delta = v_dot_w / v_dot_r;
				float old_lambda = lambda;
				lambda -= delta;
#ifdef JPH_GJK_DEBUG
				Trace("lambda = %g, delta = %g", (double)lambda, (double)delta);
#endif

				// If lambda didn't change, we cannot converge any further and we assume a hit
				if (old_lambda == lambda)
					break;

				// If lambda is bigger or equal than max, we don't have a hit
				if (lambda >= ioLambda)
					return false;

				// Update x to new closest point on the ray
				x = inRayOrigin + lambda * inRayDirection;

				// We've shifted x, so reset v_len_sq so that it is not used as early out for GetClosest
				v_len_sq = FLT_MAX;

				// We allow rebuilding the simplex once after x changes because the simplex was built
				// for another x and numerical round off builds up as you keep adding points to an
				// existing simplex
				allow_restart = true;
			}

			// Add p to set P: P = P U {p}
			mP[mNumPoints] = p;
			++mNumPoints;

			// Calculate Y = {x} - P
			for (int i = 0; i < mNumPoints; ++i)
				mY[i] = x - mP[i];

			// Determine the new closest point from Y to origin
			uint32 set;						// Set of points that form the new simplex
			if (!GetClosest<false>(v_len_sq, v, v_len_sq, set))
			{
#ifdef JPH_GJK_DEBUG
				Trace("Failed to converge");
#endif

				// Only allow 1 restart, if we still can't get a closest point
				// we're so close that we return this as a hit
				if (!allow_restart)
					break;

				// If we fail to converge, we start again with the last point as simplex
#ifdef JPH_GJK_DEBUG
				Trace("Restarting");
#endif
				allow_restart = false;
				mP[0] = p;
				mNumPoints = 1;
				v = x - p;
				v_len_sq = FLT_MAX;
				continue;
			}
			else if (set == 0xf)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Full simplex");
#endif

				// We're inside the tetrahedron, we have a hit (verify that length of v is 0)
				JPH_ASSERT(v_len_sq == 0.0f);
				break;
			}

			// Update the points P to form the new simplex
			// Note: We're not updating Y as Y will shift with x so we have to calculate it every iteration
			UpdatePointSetP(set);

			// Check if x is close enough to inA
			if (v_len_sq <= tolerance_sq)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Converged");
#endif
				break;
			}
		}

		// Store hit fraction
		ioLambda = lambda;
		return true;
	}

	/// Test if a cast shape inA moving from inStart to lambda * inStart.GetTranslation() + inDirection where lambda e [0, ioLambda> intersects inB
	///
	/// @param inStart Start position and orientation of the convex object
	/// @param inDirection Direction of the sweep (ioLambda * inDirection determines length)
	///	@param inTolerance The minimal distance between A and B before they are considered colliding
	/// @param inA The convex object A, must support the GetSupport(Vec3) function.
	/// @param inB The convex object B, must support the GetSupport(Vec3) function.
	/// @param ioLambda The max fraction along the sweep, on output updated with the actual collision fraction.
	///
	/// @return true if a hit was found, ioLambda is the solution for lambda.
	template <typename A, typename B>
	bool		CastShape(Mat44Arg inStart, Vec3Arg inDirection, float inTolerance, const A &inA, const B &inB, float &ioLambda)
	{
		// Transform the shape to be cast to the starting position
		TransformedConvexObject transformed_a(inStart, inA);

		// Calculate the minkowski difference inB - inA
		// inA is moving, so we need to add the back side of inB to the front side of inA
		MinkowskiDifference difference(inB, transformed_a);

		// Do a raycast against the Minkowski difference
		return CastRay(Vec3::sZero(), inDirection, inTolerance, difference, ioLambda);
	}

	/// Test if a cast shape inA moving from inStart to lambda * inStart.GetTranslation() + inDirection where lambda e [0, ioLambda> intersects inB
	///
	/// @param inStart Start position and orientation of the convex object
	/// @param inDirection Direction of the sweep (ioLambda * inDirection determines length)
	///	@param inTolerance The minimal distance between A and B before they are considered colliding
	/// @param inA The convex object A, must support the GetSupport(Vec3) function.
	/// @param inB The convex object B, must support the GetSupport(Vec3) function.
	/// @param inConvexRadiusA The convex radius of A, this will be added on all sides to pad A.
	/// @param inConvexRadiusB The convex radius of B, this will be added on all sides to pad B.
	/// @param ioLambda The max fraction along the sweep, on output updated with the actual collision fraction.
	///	@param outPointA is the contact point on A (if outSeparatingAxis is near zero, this may not be not the deepest point)
	///	@param outPointB is the contact point on B (if outSeparatingAxis is near zero, this may not be not the deepest point)
	/// @param outSeparatingAxis On return this will contain a vector that points from A to B along the smallest distance of separation.
	/// The length of this vector indicates the separation of A and B without their convex radius.
	/// If it is near zero, the direction may not be accurate as the bodies may overlap when lambda = 0.
	///
	///	@return true if a hit was found, ioLambda is the solution for lambda and outPoint and outSeparatingAxis are valid.
	template <typename A, typename B>
	bool		CastShape(Mat44Arg inStart, Vec3Arg inDirection, float inTolerance, const A &inA, const B &inB, float inConvexRadiusA, float inConvexRadiusB, float &ioLambda, Vec3 &outPointA, Vec3 &outPointB, Vec3 &outSeparatingAxis)
	{
		float tolerance_sq = Square(inTolerance);

		// Calculate how close A and B (without their convex radius) need to be to each other in order for us to consider this a collision
		float sum_convex_radius = inConvexRadiusA + inConvexRadiusB;

		// Transform the shape to be cast to the starting position
		TransformedConvexObject transformed_a(inStart, inA);

		// Reset state
		mNumPoints = 0;

		float lambda = 0.0f;
		Vec3 x = Vec3::sZero(); // Since A is already transformed we can start the cast from zero
		Vec3 v = -inB.GetSupport(Vec3::sZero()) + transformed_a.GetSupport(Vec3::sZero()); // See CastRay: v = x - inA.GetSupport(Vec3::sZero()) where inA is the Minkowski difference inB - transformed_a (see CastShape above) and x is zero
		float v_len_sq = FLT_MAX;
		bool allow_restart = false;

		// Keeps track of separating axis of the previous iteration.
		// Initialized at zero as we don't know if our first v is actually a separating axis.
		Vec3 prev_v = Vec3::sZero();

		for (;;)
		{
#ifdef JPH_GJK_DEBUG
			Trace("v = [%s], num_points = %d", ConvertToString(v).c_str(), mNumPoints);
#endif

			// Calculate the minkowski difference inB - inA
			// inA is moving, so we need to add the back side of inB to the front side of inA
			// Keep the support points on A and B separate so that in the end we can calculate a contact point
			Vec3 p = transformed_a.GetSupport(-v);
			Vec3 q = inB.GetSupport(v);
			Vec3 w = x - (q - p);

#ifdef JPH_GJK_DEBUG
			Trace("w = [%s]", ConvertToString(w).c_str());
#endif

			// Difference from article to this code:
			// We did not include the convex radius in p and q in order to be able to calculate a good separating axis at the end of the algorithm.
			// However when moving forward along inDirection we do need to take this into account so that we keep A and B separated by the sum of their convex radii.
			// From p we have to subtract: inConvexRadiusA * v / |v|
			// To q we have to add: inConvexRadiusB * v / |v|
			// This means that to w we have to add: -(inConvexRadiusA + inConvexRadiusB) * v / |v|
			// So to v . w we have to add: v . (-(inConvexRadiusA + inConvexRadiusB) * v / |v|) = -(inConvexRadiusA + inConvexRadiusB) * |v|
			float v_dot_w = v.Dot(w) - sum_convex_radius * v.Length();
#ifdef JPH_GJK_DEBUG
			Trace("v . w = %g", (double)v_dot_w);
#endif
			if (v_dot_w > 0.0f)
			{
				// If ray and normal are in the same direction, we've passed A and there's no collision
				float v_dot_r = v.Dot(inDirection);
#ifdef JPH_GJK_DEBUG
				Trace("v . r = %g", (double)v_dot_r);
#endif
				if (v_dot_r >= 0.0f)
					return false;

				// Update the lower bound for lambda
				float delta = v_dot_w / v_dot_r;
				float old_lambda = lambda;
				lambda -= delta;
#ifdef JPH_GJK_DEBUG
				Trace("lambda = %g, delta = %g", (double)lambda, (double)delta);
#endif

				// If lambda didn't change, we cannot converge any further and we assume a hit
				if (old_lambda == lambda)
					break;

				// If lambda is bigger or equal than max, we don't have a hit
				if (lambda >= ioLambda)
					return false;

				// Update x to new closest point on the ray
				x = lambda * inDirection;

				// We've shifted x, so reset v_len_sq so that it is not used as early out when GetClosest returns false
				v_len_sq = FLT_MAX;

				// Now that we've moved, we know that A and B are not intersecting at lambda = 0, so we can update our tolerance to stop iterating
				// as soon as A and B are inConvexRadiusA + inConvexRadiusB apart
				tolerance_sq = Square(inTolerance + sum_convex_radius);

				// We allow rebuilding the simplex once after x changes because the simplex was built
				// for another x and numerical round off builds up as you keep adding points to an
				// existing simplex
				allow_restart = true;
			}

			// Add p to set P, q to set Q: P = P U {p}, Q = Q U {q}
			mP[mNumPoints] = p;
			mQ[mNumPoints] = q;
			++mNumPoints;

			// Calculate Y = {x} - (Q - P)
			for (int i = 0; i < mNumPoints; ++i)
				mY[i] = x - (mQ[i] - mP[i]);

			// Determine the new closest point from Y to origin
			uint32 set;						// Set of points that form the new simplex
			if (!GetClosest<false>(v_len_sq, v, v_len_sq, set))
			{
#ifdef JPH_GJK_DEBUG
				Trace("Failed to converge");
#endif

				// Only allow 1 restart, if we still can't get a closest point
				// we're so close that we return this as a hit
				if (!allow_restart)
					break;

				// If we fail to converge, we start again with the last point as simplex
#ifdef JPH_GJK_DEBUG
				Trace("Restarting");
#endif
				allow_restart = false;
				mP[0] = p;
				mQ[0] = q;
				mNumPoints = 1;
				v = x - q;
				v_len_sq = FLT_MAX;
				continue;
			}
			else if (set == 0xf)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Full simplex");
#endif

				// We're inside the tetrahedron, we have a hit (verify that length of v is 0)
				JPH_ASSERT(v_len_sq == 0.0f);
				break;
			}

			// Update the points P and Q to form the new simplex
			// Note: We're not updating Y as Y will shift with x so we have to calculate it every iteration
			UpdatePointSetPQ(set);

			// Check if A and B are touching according to our tolerance
			if (v_len_sq <= tolerance_sq)
			{
#ifdef JPH_GJK_DEBUG
				Trace("Converged");
#endif
				break;
			}

			// Store our v to return as separating axis
			prev_v = v;
		}

		// Calculate Y = {x} - (Q - P) again so we can calculate the contact points
		for (int i = 0; i < mNumPoints; ++i)
			mY[i] = x - (mQ[i] - mP[i]);

		// Calculate the offset we need to apply to A and B to correct for the convex radius
		Vec3 normalized_v = v.NormalizedOr(Vec3::sZero());
		Vec3 convex_radius_a = inConvexRadiusA * normalized_v;
		Vec3 convex_radius_b = inConvexRadiusB * normalized_v;

		// Get the contact point
		// Note that A and B will coincide when lambda > 0. In this case we calculate only B as it is more accurate as it contains less terms.
		switch (mNumPoints)
		{
		case 1:
			outPointB = mQ[0] + convex_radius_b;
			outPointA = lambda > 0.0f? outPointB : mP[0] - convex_radius_a;
			break;

		case 2:
			{
				float bu, bv;
				ClosestPoint::GetBaryCentricCoordinates(mY[0], mY[1], bu, bv);
				outPointB = bu * mQ[0] + bv * mQ[1] + convex_radius_b;
				outPointA = lambda > 0.0f? outPointB : bu * mP[0] + bv * mP[1] - convex_radius_a;
			}
			break;

		case 3:
		case 4: // A full simplex, we can't properly determine a contact point! As contact point we take the closest point of the previous iteration.
			{
				float bu, bv, bw;
				ClosestPoint::GetBaryCentricCoordinates(mY[0], mY[1], mY[2], bu, bv, bw);
				outPointB = bu * mQ[0] + bv * mQ[1] + bw * mQ[2] + convex_radius_b;
				outPointA = lambda > 0.0f? outPointB : bu * mP[0] + bv * mP[1] + bw * mP[2] - convex_radius_a;
			}
			break;
		}

		// Store separating axis, in case we have a convex radius we can just return v,
		// otherwise v will be very small and we resort to returning previous v as an approximation.
		outSeparatingAxis = sum_convex_radius > 0.0f? -v : -prev_v;

		// Store hit fraction
		ioLambda = lambda;
		return true;
	}

private:
#ifdef JPH_GJK_DEBUG
	/// Draw state of algorithm
	void		DrawState()
	{
		RMat44 origin = RMat44::sTranslation(mOffset);

		// Draw origin
		DebugRenderer::sInstance->DrawCoordinateSystem(origin, 1.0f);

		// Draw the hull
		DebugRenderer::sInstance->DrawGeometry(origin, mGeometry->mBounds.Transformed(origin), mGeometry->mBounds.GetExtent().LengthSq(), Color::sYellow, mGeometry);

		// Draw Y
		for (int i = 0; i < mNumPoints; ++i)
		{
			// Draw support point
			RVec3 y_i = origin * mY[i];
			DebugRenderer::sInstance->DrawMarker(y_i, Color::sRed, 1.0f);
			for (int j = i + 1; j < mNumPoints; ++j)
			{
				// Draw edge
				RVec3 y_j = origin * mY[j];
				DebugRenderer::sInstance->DrawLine(y_i, y_j, Color::sRed);
				for (int k = j + 1; k < mNumPoints; ++k)
				{
					// Make sure triangle faces the origin
					RVec3 y_k = origin * mY[k];
					RVec3 center = (y_i + y_j + y_k) / Real(3);
					RVec3 normal = (y_j - y_i).Cross(y_k - y_i);
					if (normal.Dot(center) < Real(0))
						DebugRenderer::sInstance->DrawTriangle(y_i, y_j, y_k, Color::sLightGrey);
					else
						DebugRenderer::sInstance->DrawTriangle(y_i, y_k, y_j, Color::sLightGrey);
				}
			}
		}

		// Offset to the right
		mOffset += Vec3(mGeometry->mBounds.GetSize().GetX() + 2.0f, 0, 0);
	}
#endif // JPH_GJK_DEBUG

	Vec3		mY[4];						///< Support points on A - B
	Vec3		mP[4];						///< Support point on A
	Vec3		mQ[4];						///< Support point on B
	int			mNumPoints = 0;				///< Number of points in mY, mP and mQ that are valid

#ifdef JPH_GJK_DEBUG
	DebugRenderer::GeometryRef	mGeometry;	///< A visualization of the minkowski difference for state drawing
	RVec3		mOffset = RVec3::sZero();	///< Offset to use for state drawing
#endif
};

JPH_NAMESPACE_END
