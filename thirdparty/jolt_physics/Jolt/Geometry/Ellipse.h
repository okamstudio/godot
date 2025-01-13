// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/Float2.h>

JPH_NAMESPACE_BEGIN

/// Ellipse centered around the origin
/// @see https://en.wikipedia.org/wiki/Ellipse
class Ellipse
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct ellipse with radius A along the X-axis and B along the Y-axis
					Ellipse(float inA, float inB) : mA(inA), mB(inB) { JPH_ASSERT(inA > 0.0f); JPH_ASSERT(inB > 0.0f); }

	/// Check if inPoint is inside the ellipse
	bool			IsInside(const Float2 &inPoint) const
	{
		return Square(inPoint.x / mA) + Square(inPoint.y / mB) <= 1.0f;
	}

	/// Get the closest point on the ellipse to inPoint
	/// Assumes inPoint is outside the ellipse
	/// @see Rotation Joint Limits in Quaternion Space by Gino van den Bergen, section 10.1 in Game Engine Gems 3.
	Float2			GetClosestPoint(const Float2 &inPoint) const
	{
		float a_sq = Square(mA);
		float b_sq = Square(mB);

		// Equation of ellipse: f(x, y) = (x/a)^2 + (y/b)^2 - 1 = 0											[1]
		// Normal on surface: (df/dx, df/dy) = (2 x / a^2, 2 y / b^2)
		// Closest point (x', y') on ellipse to point (x, y): (x', y') + t (x / a^2, y / b^2) = (x, y)
		// <=> (x', y') = (a^2 x / (t + a^2), b^2 y / (t + b^2))
		// Requiring point to be on ellipse (substituting into [1]): g(t) = (a x / (t + a^2))^2 + (b y / (t + b^2))^2 - 1 = 0

		// Newton Raphson iteration, starting at t = 0
		float t = 0.0f;
		for (;;)
		{
			// Calculate g(t)
			float t_plus_a_sq = t + a_sq;
			float t_plus_b_sq = t + b_sq;
			float gt = Square(mA * inPoint.x / t_plus_a_sq) + Square(mB * inPoint.y / t_plus_b_sq) - 1.0f;

			// Check if g(t) it is close enough to zero
			if (abs(gt) < 1.0e-6f)
				return Float2(a_sq * inPoint.x / t_plus_a_sq, b_sq * inPoint.y / t_plus_b_sq);

			// Get derivative dg/dt = g'(t) = -2 (b^2 y^2 / (t + b^2)^3 + a^2 x^2 / (t + a^2)^3)
			float gt_accent = -2.0f *
				(a_sq * Square(inPoint.x) / Cubed(t_plus_a_sq)
				+ b_sq * Square(inPoint.y) / Cubed(t_plus_b_sq));

			// Calculate t for next iteration: tn+1 = tn - g(t) / g'(t)
			float tn = t - gt / gt_accent;
			t = tn;
		}
	}

	/// Get normal at point inPoint (non-normalized vector)
	Float2			GetNormal(const Float2 &inPoint) const
	{
		// Calculated by [d/dx f(x, y), d/dy f(x, y)], where f(x, y) is the ellipse equation from above
		return Float2(inPoint.x / Square(mA), inPoint.y / Square(mB));
	}

private:
	float			mA;				///< Radius along X-axis
	float			mB;				///< Radius along Y-axis
};

JPH_NAMESPACE_END
