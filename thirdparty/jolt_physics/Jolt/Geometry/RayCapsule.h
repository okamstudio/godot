// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/RayCylinder.h>
#include <Jolt/Geometry/RaySphere.h>

JPH_NAMESPACE_BEGIN

/// Tests a ray starting at inRayOrigin and extending infinitely in inRayDirection
/// against a capsule centered around the origin with its axis along the Y axis and half height specified.
/// @return FLT_MAX if there is no intersection, otherwise the fraction along the ray.
/// @param inRayDirection Ray direction. Does not need to be normalized.
/// @param inRayOrigin Origin of the ray. If the ray starts inside the capsule, the returned fraction will be 0.
/// @param inCapsuleHalfHeight Distance from the origin to the center of the top sphere (or that of the bottom)
/// @param inCapsuleRadius Radius of the top/bottom sphere
JPH_INLINE float RayCapsule(Vec3Arg inRayOrigin, Vec3Arg inRayDirection, float inCapsuleHalfHeight, float inCapsuleRadius)
{
	// Test infinite cylinder
	float cylinder = RayCylinder(inRayOrigin, inRayDirection, inCapsuleRadius);
	if (cylinder == FLT_MAX)
		return FLT_MAX;

	// If this hit is in the finite cylinder we have our fraction
	if (abs(inRayOrigin.GetY() + cylinder * inRayDirection.GetY()) <= inCapsuleHalfHeight)
		return cylinder;

	// Test upper and lower sphere
	Vec3 sphere_center(0, inCapsuleHalfHeight, 0);
	float upper = RaySphere(inRayOrigin, inRayDirection, sphere_center, inCapsuleRadius);
	float lower = RaySphere(inRayOrigin, inRayDirection, -sphere_center, inCapsuleRadius);
	return min(upper, lower);
}

JPH_NAMESPACE_END
