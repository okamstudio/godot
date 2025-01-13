// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/PathConstraintPathHermite.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(PathConstraintPathHermite::Point)
{
	JPH_ADD_ATTRIBUTE(PathConstraintPathHermite::Point, mPosition)
	JPH_ADD_ATTRIBUTE(PathConstraintPathHermite::Point, mTangent)
	JPH_ADD_ATTRIBUTE(PathConstraintPathHermite::Point, mNormal)
}

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(PathConstraintPathHermite)
{
	JPH_ADD_BASE_CLASS(PathConstraintPathHermite, PathConstraintPath)

	JPH_ADD_ATTRIBUTE(PathConstraintPathHermite, mPoints)
}

// Calculate position and tangent for a Cubic Hermite Spline segment
static inline void sCalculatePositionAndTangent(Vec3Arg inP1, Vec3Arg inM1, Vec3Arg inP2, Vec3Arg inM2, float inT, Vec3 &outPosition, Vec3 &outTangent)
{
	// Calculate factors for Cubic Hermite Spline
	// See: https://en.wikipedia.org/wiki/Cubic_Hermite_spline
	float t2 = inT * inT;
	float t3 = inT * t2;
	float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
	float h10 = t3 - 2.0f * t2 + inT;
	float h01 = -2.0f * t3 + 3.0f * t2;
	float h11 = t3 - t2;

	// Calculate d/dt for factors to calculate the tangent
	float ddt_h00 = 6.0f * (t2 - inT);
	float ddt_h10 = 3.0f * t2 - 4.0f * inT + 1.0f;
	float ddt_h01 = -ddt_h00;
	float ddt_h11 = 3.0f * t2 - 2.0f * inT;

	outPosition = h00 * inP1 + h10 * inM1 + h01 * inP2 + h11 * inM2;
	outTangent = ddt_h00 * inP1 + ddt_h10 * inM1 + ddt_h01 * inP2 + ddt_h11 * inM2;
}

// Calculate the closest point to the origin for a Cubic Hermite Spline segment
// This is used to get an estimate for the interval in which the closest point can be found,
// the interval [0, 1] is too big for Newton Raphson to work on because it is solving a 5th degree polynomial which may
// have multiple local minima that are not the root. This happens especially when the path is straight (tangents aligned with inP2 - inP1).
// Based on the bisection method: https://en.wikipedia.org/wiki/Bisection_method
static inline void sCalculateClosestPointThroughBisection(Vec3Arg inP1, Vec3Arg inM1, Vec3Arg inP2, Vec3Arg inM2, float &outTMin, float &outTMax)
{
	outTMin = 0.0f;
	outTMax = 1.0f;

	// To get the closest point of the curve to the origin we need to solve:
	// d/dt P(t) . P(t) = 0 for t, where P(t) is the point on the curve segment
	// Using d/dt (a(t) . b(t)) = d/dt a(t) . b(t) + a(t) . d/dt b(t)
	// See: https://proofwiki.org/wiki/Derivative_of_Dot_Product_of_Vector-Valued_Functions
	// d/dt P(t) . P(t) = 2 P(t) d/dt P(t) = 2 P(t) . Tangent(t)

	// Calculate the derivative at t = 0, we know P(0) = inP1 and Tangent(0) = inM1
	float ddt_min = inP1.Dot(inM1); // Leaving out factor 2, we're only interested in the root
	if (abs(ddt_min) < 1.0e-6f)
	{
		// Derivative is near zero, we found our root
		outTMax = 0.0f;
		return;
	}
	bool ddt_min_negative = ddt_min < 0.0f;

	// Calculate derivative at t = 1, we know P(1) = inP2 and Tangent(1) = inM2
	float ddt_max = inP2.Dot(inM2);
	if (abs(ddt_max) < 1.0e-6f)
	{
		// Derivative is near zero, we found our root
		outTMin = 1.0f;
		return;
	}
	bool ddt_max_negative = ddt_max < 0.0f;

	// If the signs of the derivative are not different, this algorithm can't find the root
	if (ddt_min_negative == ddt_max_negative)
		return;

	// With 4 iterations we'll get a result accurate to 1 / 2^4 = 0.0625
	for (int iteration = 0; iteration < 4; ++iteration)
	{
		float t_mid = 0.5f * (outTMin + outTMax);
		Vec3 position, tangent;
		sCalculatePositionAndTangent(inP1, inM1, inP2, inM2, t_mid, position, tangent);
		float ddt_mid = position.Dot(tangent);
		if (abs(ddt_mid) < 1.0e-6f)
		{
			// Derivative is near zero, we found our root
			outTMin = outTMax = t_mid;
			return;
		}
		bool ddt_mid_negative = ddt_mid < 0.0f;

		// Update the search interval so that the signs of the derivative at both ends of the interval are still different
		if (ddt_mid_negative == ddt_min_negative)
			outTMin = t_mid;
		else
			outTMax = t_mid;
	}
}

// Calculate the closest point to the origin for a Cubic Hermite Spline segment
// Only considers the range t e [inTMin, inTMax] and will stop as soon as the closest point falls outside of that range
static inline float sCalculateClosestPointThroughNewtonRaphson(Vec3Arg inP1, Vec3Arg inM1, Vec3Arg inP2, Vec3Arg inM2, float inTMin, float inTMax, float &outDistanceSq)
{
	// This is the closest position on the curve to the origin that we found
	Vec3 position;

	// Calculate the size of the interval
	float interval = inTMax - inTMin;

	// Start in the middle of the interval
	float t = 0.5f * (inTMin + inTMax);

	// Do max 10 iterations to prevent taking too much CPU time
	for (int iteration = 0; iteration < 10; ++iteration)
	{
		// Calculate derivative at t, see comment at sCalculateClosestPointThroughBisection for derivation of the equations
		Vec3 tangent;
		sCalculatePositionAndTangent(inP1, inM1, inP2, inM2, t, position, tangent);
		float ddt = position.Dot(tangent); // Leaving out factor 2, we're only interested in the root

		// Calculate derivative of ddt: d^2/dt P(t) . P(t) = d/dt (2 P(t) . Tangent(t))
		// = 2 (d/dt P(t)) . Tangent(t) + P(t) . d/dt Tangent(t)) = 2 (Tangent(t) . Tangent(t) + P(t) . d/dt Tangent(t))
		float d2dt_h00 = 12.0f * t - 6.0f;
		float d2dt_h10 = 6.0f * t - 4.0f;
		float d2dt_h01 = -d2dt_h00;
		float d2dt_h11 = 6.0f * t - 2.0f;
		Vec3 ddt_tangent = d2dt_h00 * inP1 + d2dt_h10 * inM1 + d2dt_h01 * inP2 + d2dt_h11 * inM2;
		float d2dt = tangent.Dot(tangent) + position.Dot(ddt_tangent); // Leaving out factor 2, because we left it out above too

		// If d2dt is zero, the curve is flat and there are multiple t's for which we are closest to the origin, stop now
		if (d2dt == 0.0f)
			break;

		// Do a Newton Raphson step
		// See: https://en.wikipedia.org/wiki/Newton%27s_method
		// Clamp against [-interval, interval] to avoid overshooting too much, we're not interested outside the interval
		float delta = Clamp(-ddt / d2dt, -interval, interval);

		// If we're stepping away further from t e [inTMin, inTMax] stop now
		if ((t > inTMax && delta > 0.0f) || (t < inTMin && delta < 0.0f))
			break;

		// If we've converged, stop now
		t += delta;
		if (abs(delta) < 1.0e-4f)
			break;
	}

	// Calculate the distance squared for the origin to the curve
	outDistanceSq = position.LengthSq();
	return t;
}

void PathConstraintPathHermite::GetIndexAndT(float inFraction, int &outIndex, float &outT) const
{
	int num_points = int(mPoints.size());

	// Start by truncating the fraction to get the index and storing the remainder in t
	int index = int(trunc(inFraction));
	float t = inFraction - float(index);

	if (IsLooping())
	{
		JPH_ASSERT(!mPoints.front().mPosition.IsClose(mPoints.back().mPosition), "A looping path should have a different first and last point!");

		// Make sure index is positive by adding a multiple of num_points
		if (index < 0)
			index += (-index / num_points + 1) * num_points;

		// Index needs to be modulo num_points
		index = index % num_points;
	}
	else
	{
		// Clamp against range of points
		if (index < 0)
		{
			index = 0;
			t = 0.0f;
		}
		else if (index >= num_points - 1)
		{
			index = num_points - 2;
			t = 1.0f;
		}
	}

	outIndex = index;
	outT = t;
}

float PathConstraintPathHermite::GetClosestPoint(Vec3Arg inPosition, float inFractionHint) const
{
	JPH_PROFILE_FUNCTION();

	int num_points = int(mPoints.size());

	// Start with last point on the path, in the non-looping case we won't be visiting this point
	float best_dist_sq = (mPoints[num_points - 1].mPosition - inPosition).LengthSq();
	float best_t = float(num_points - 1);

	// Loop over all points
	for (int i = 0, max_i = IsLooping()? num_points : num_points - 1; i < max_i; ++i)
	{
		const Point &p1 = mPoints[i];
		const Point &p2 = mPoints[(i + 1) % num_points];

		// Make the curve relative to inPosition
		Vec3 p1_pos = p1.mPosition - inPosition;
		Vec3 p2_pos = p2.mPosition - inPosition;

		// Get distance to p1
		float dist_sq = p1_pos.LengthSq();
		if (dist_sq < best_dist_sq)
		{
			best_t = float(i);
			best_dist_sq = dist_sq;
		}

		// First find an interval for the closest point so that we can start doing Newton Raphson steps
		float t_min, t_max;
		sCalculateClosestPointThroughBisection(p1_pos, p1.mTangent, p2_pos, p2.mTangent, t_min, t_max);

		if (t_min == t_max)
		{
			// If the function above returned no interval then it found the root already and we can just calculate the distance
			Vec3 position, tangent;
			sCalculatePositionAndTangent(p1_pos, p1.mTangent, p2_pos, p2.mTangent, t_min, position, tangent);
			dist_sq = position.LengthSq();
			if (dist_sq < best_dist_sq)
			{
				best_t = float(i) + t_min;
				best_dist_sq = dist_sq;
			}
		}
		else
		{
			// Get closest distance along curve segment
			float t = sCalculateClosestPointThroughNewtonRaphson(p1_pos, p1.mTangent, p2_pos, p2.mTangent, t_min, t_max, dist_sq);
			if (t >= 0.0f && t <= 1.0f && dist_sq < best_dist_sq)
			{
				best_t = float(i) + t;
				best_dist_sq = dist_sq;
			}
		}
	}

	return best_t;
}

void PathConstraintPathHermite::GetPointOnPath(float inFraction, Vec3 &outPathPosition, Vec3 &outPathTangent, Vec3 &outPathNormal, Vec3 &outPathBinormal) const
{
	JPH_PROFILE_FUNCTION();

	// Determine which hermite spline segment we need
	int index;
	float t;
	GetIndexAndT(inFraction, index, t);

	// Get the points on the segment
	const Point &p1 = mPoints[index];
	const Point &p2 = mPoints[(index + 1) % int(mPoints.size())];

	// Calculate the position and tangent on the path
	Vec3 tangent;
	sCalculatePositionAndTangent(p1.mPosition, p1.mTangent, p2.mPosition, p2.mTangent, t, outPathPosition, tangent);
	outPathTangent = tangent.Normalized();

	// Just linearly interpolate the normal
	Vec3 normal = (1.0f - t) * p1.mNormal + t * p2.mNormal;

	// Calculate binormal
	outPathBinormal = normal.Cross(outPathTangent).Normalized();

	// Recalculate normal so it is perpendicular to both (linear interpolation will cause it not to be)
	outPathNormal = outPathTangent.Cross(outPathBinormal);
	JPH_ASSERT(outPathNormal.IsNormalized());
}

void PathConstraintPathHermite::SaveBinaryState(StreamOut &inStream) const
{
	PathConstraintPath::SaveBinaryState(inStream);

	inStream.Write(mPoints);
}

void PathConstraintPathHermite::RestoreBinaryState(StreamIn &inStream)
{
	PathConstraintPath::RestoreBinaryState(inStream);

	inStream.Read(mPoints);
}

JPH_NAMESPACE_END
