// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Math/FindRoot.h>

JPH_NAMESPACE_BEGIN

/// Tests a ray starting at inRayOrigin and extending infinitely in inRayDirection
/// against an infinite cylinder centered along the Y axis
/// @return FLT_MAX if there is no intersection, otherwise the fraction along the ray.
/// @param inRayDirection Direction of the ray. Does not need to be normalized.
/// @param inRayOrigin Origin of the ray. If the ray starts inside the cylinder, the returned fraction will be 0.
/// @param inCylinderRadius Radius of the infinite cylinder
JPH_INLINE float RayCylinder(Vec3Arg inRayOrigin, Vec3Arg inRayDirection, float inCylinderRadius)
{
	// Remove Y component of ray to see of ray intersects with infinite cylinder
	UVec4 mask_y = UVec4(0, 0xffffffff, 0, 0);
	Vec3 origin_xz = Vec3::sSelect(inRayOrigin, Vec3::sZero(), mask_y);
	float origin_xz_len_sq = origin_xz.LengthSq();
	float r_sq = Square(inCylinderRadius);
	if (origin_xz_len_sq > r_sq)
	{
		// Ray starts outside of the infinite cylinder
		// Solve: |RayOrigin_xz + fraction * RayDirection_xz|^2 = r^2 to find fraction
		Vec3 direction_xz = Vec3::sSelect(inRayDirection, Vec3::sZero(), mask_y);
		float a = direction_xz.LengthSq();
		float b = 2.0f * origin_xz.Dot(direction_xz);
		float c = origin_xz_len_sq - r_sq;
		float fraction1, fraction2;
		if (FindRoot(a, b, c, fraction1, fraction2) == 0)
			return FLT_MAX; // No intersection with infinite cylinder

		// Get fraction corresponding to the ray entering the circle
		float fraction = min(fraction1, fraction2);
		if (fraction >= 0.0f)
			return fraction;
	}
	else
	{
		// Ray starts inside the infinite cylinder
		return 0.0f;
	}

	// No collision
	return FLT_MAX;
}

/// Test a ray against a cylinder centered around the origin with its axis along the Y axis and half height specified.
/// @return FLT_MAX if there is no intersection, otherwise the fraction along the ray.
/// @param inRayDirection Ray direction. Does not need to be normalized.
/// @param inRayOrigin Origin of the ray. If the ray starts inside the cylinder, the returned fraction will be 0.
/// @param inCylinderRadius Radius of the cylinder
/// @param inCylinderHalfHeight Distance from the origin to the top (or bottom) of the cylinder
JPH_INLINE float RayCylinder(Vec3Arg inRayOrigin, Vec3Arg inRayDirection, float inCylinderHalfHeight, float inCylinderRadius)
{
	// Test infinite cylinder
	float fraction = RayCylinder(inRayOrigin, inRayDirection, inCylinderRadius);
	if (fraction == FLT_MAX)
		return FLT_MAX;

	// If this hit is in the finite cylinder we have our fraction
	if (abs(inRayOrigin.GetY() + fraction * inRayDirection.GetY()) <= inCylinderHalfHeight)
		return fraction;

	// Check if ray could hit the top or bottom plane of the cylinder
	float direction_y = inRayDirection.GetY();
	if (direction_y != 0.0f)
	{
		// Solving line equation: x = ray_origin + fraction * ray_direction
		// and plane equation: plane_normal . x + plane_constant = 0
		// fraction = (-plane_constant - plane_normal . ray_origin) / (plane_normal . ray_direction)
		// when the ray_direction.y < 0:
		// plane_constant = -cylinder_half_height, plane_normal = (0, 1, 0)
		// else
		// plane_constant = -cylinder_half_height, plane_normal = (0, -1, 0)
		float origin_y = inRayOrigin.GetY();
		float plane_fraction;
		if (direction_y < 0.0f)
			plane_fraction = (inCylinderHalfHeight - origin_y) / direction_y;
		else
			plane_fraction = -(inCylinderHalfHeight + origin_y) / direction_y;

		// Check if the hit is in front of the ray
		if (plane_fraction >= 0.0f)
		{
			// Test if this hit is inside the cylinder
			Vec3 point = inRayOrigin + plane_fraction * inRayDirection;
			float dist_sq = Square(point.GetX()) + Square(point.GetZ());
			if (dist_sq <= Square(inCylinderRadius))
				return plane_fraction;
		}
	}

	// No collision
	return FLT_MAX;
}

JPH_NAMESPACE_END
