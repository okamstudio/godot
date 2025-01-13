// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/FindRoot.h>

JPH_NAMESPACE_BEGIN

/// Tests a ray starting at inRayOrigin and extending infinitely in inRayDirection against a sphere,
/// @return FLT_MAX if there is no intersection, otherwise the fraction along the ray.
/// @param inRayOrigin Ray origin. If the ray starts inside the sphere, the returned fraction will be 0.
/// @param inRayDirection Ray direction. Does not need to be normalized.
/// @param inSphereCenter Position of the center of the sphere
/// @param inSphereRadius Radius of the sphere
JPH_INLINE float RaySphere(Vec3Arg inRayOrigin, Vec3Arg inRayDirection, Vec3Arg inSphereCenter, float inSphereRadius)
{
	// Solve: |RayOrigin + fraction * RayDirection - SphereCenter|^2 = SphereRadius^2 for fraction
	Vec3 center_origin = inRayOrigin - inSphereCenter;
	float a = inRayDirection.LengthSq();
	float b = 2.0f * inRayDirection.Dot(center_origin);
	float c = center_origin.LengthSq() - inSphereRadius * inSphereRadius;
	float fraction1, fraction2;
	if (FindRoot(a, b, c, fraction1, fraction2) == 0)
		return c <= 0.0f? 0.0f : FLT_MAX; // Return if origin is inside the sphere

	// Sort so that the smallest is first
	if (fraction1 > fraction2)
		std::swap(fraction1, fraction2);

	// Test solution with lowest fraction, this will be the ray entering the sphere
	if (fraction1 >= 0.0f)
		return fraction1; // Sphere is before the ray start

	// Test solution with highest fraction, this will be the ray leaving the sphere
	if (fraction2 >= 0.0f)
		return 0.0f; // We start inside the sphere

	// No solution
	return FLT_MAX;
}

/// Tests a ray starting at inRayOrigin and extending infinitely in inRayDirection against a sphere.
/// Outputs entry and exit points (outMinFraction and outMaxFraction) along the ray (which could be negative if the hit point is before the start of the ray).
/// @param inRayOrigin Ray origin. If the ray starts inside the sphere, the returned fraction will be 0.
/// @param inRayDirection Ray direction. Does not need to be normalized.
/// @param inSphereCenter Position of the center of the sphere.
/// @param inSphereRadius Radius of the sphere.
/// @param outMinFraction Returned lowest intersection fraction
/// @param outMaxFraction Returned highest intersection fraction
/// @return The amount of intersections with the sphere.
/// If 1 intersection is returned outMinFraction will be equal to outMaxFraction
JPH_INLINE int RaySphere(Vec3Arg inRayOrigin, Vec3Arg inRayDirection, Vec3Arg inSphereCenter, float inSphereRadius, float &outMinFraction, float &outMaxFraction)
{
	// Solve: |RayOrigin + fraction * RayDirection - SphereCenter|^2 = SphereRadius^2 for fraction
	Vec3 center_origin = inRayOrigin - inSphereCenter;
	float a = inRayDirection.LengthSq();
	float b = 2.0f * inRayDirection.Dot(center_origin);
	float c = center_origin.LengthSq() - inSphereRadius * inSphereRadius;
	float fraction1, fraction2;
	switch (FindRoot(a, b, c, fraction1, fraction2))
	{
	case 0:
		if (c <= 0.0f)
		{
			// Origin inside sphere
			outMinFraction = outMaxFraction = 0.0f;
			return 1;
		}
		else
		{
			// Origin outside of the sphere
			return 0;
		}
		break;

	case 1:
		// Ray is touching the sphere
		outMinFraction = outMaxFraction = fraction1;
		return 1;

	default:
		// Ray enters and exits the sphere

		// Sort so that the smallest is first
		if (fraction1 > fraction2)
			std::swap(fraction1, fraction2);

		outMinFraction = fraction1;
		outMaxFraction = fraction2;
		return 2;
	}
}

JPH_NAMESPACE_END
