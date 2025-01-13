// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Helper structure holding the reciprocal of a ray for Ray vs AABox testing
class RayInvDirection
{
public:
	/// Constructors
	inline			RayInvDirection() = default;
	inline explicit	RayInvDirection(Vec3Arg inDirection) { Set(inDirection); }

	/// Set reciprocal from ray direction
	inline void		Set(Vec3Arg inDirection)
	{
		// if (abs(inDirection) <= Epsilon) the ray is nearly parallel to the slab.
		mIsParallel = Vec3::sLessOrEqual(inDirection.Abs(), Vec3::sReplicate(1.0e-20f));

		// Calculate 1 / direction while avoiding division by zero
		mInvDirection = Vec3::sSelect(inDirection, Vec3::sReplicate(1.0f), mIsParallel).Reciprocal();
	}

	Vec3			mInvDirection;					///< 1 / ray direction
	UVec4			mIsParallel;					///< for each component if it is parallel to the coordinate axis
};

/// Intersect AABB with ray, returns minimal distance along ray or FLT_MAX if no hit
/// Note: Can return negative value if ray starts in box
JPH_INLINE float RayAABox(Vec3Arg inOrigin, const RayInvDirection &inInvDirection, Vec3Arg inBoundsMin, Vec3Arg inBoundsMax)
{
	// Constants
	Vec3 flt_min = Vec3::sReplicate(-FLT_MAX);
	Vec3 flt_max = Vec3::sReplicate(FLT_MAX);

	// Test against all three axii simultaneously.
	Vec3 t1 = (inBoundsMin - inOrigin) * inInvDirection.mInvDirection;
	Vec3 t2 = (inBoundsMax - inOrigin) * inInvDirection.mInvDirection;

	// Compute the max of min(t1,t2) and the min of max(t1,t2) ensuring we don't
	// use the results from any directions parallel to the slab.
	Vec3 t_min = Vec3::sSelect(Vec3::sMin(t1, t2), flt_min, inInvDirection.mIsParallel);
	Vec3 t_max = Vec3::sSelect(Vec3::sMax(t1, t2), flt_max, inInvDirection.mIsParallel);

	// t_min.xyz = maximum(t_min.x, t_min.y, t_min.z);
	t_min = Vec3::sMax(t_min, t_min.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>());
	t_min = Vec3::sMax(t_min, t_min.Swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>());

	// t_max.xyz = minimum(t_max.x, t_max.y, t_max.z);
	t_max = Vec3::sMin(t_max, t_max.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>());
	t_max = Vec3::sMin(t_max, t_max.Swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>());

	// if (t_min > t_max) return FLT_MAX;
	UVec4 no_intersection = Vec3::sGreater(t_min, t_max);

	// if (t_max < 0.0f) return FLT_MAX;
	no_intersection = UVec4::sOr(no_intersection, Vec3::sLess(t_max, Vec3::sZero()));

	// if (inInvDirection.mIsParallel && !(Min <= inOrigin && inOrigin <= Max)) return FLT_MAX; else return t_min;
	UVec4 no_parallel_overlap = UVec4::sOr(Vec3::sLess(inOrigin, inBoundsMin), Vec3::sGreater(inOrigin, inBoundsMax));
	no_intersection = UVec4::sOr(no_intersection, UVec4::sAnd(inInvDirection.mIsParallel, no_parallel_overlap));
	no_intersection = UVec4::sOr(no_intersection, no_intersection.SplatY());
	no_intersection = UVec4::sOr(no_intersection, no_intersection.SplatZ());
	return Vec3::sSelect(t_min, flt_max, no_intersection).GetX();
}

/// Intersect 4 AABBs with ray, returns minimal distance along ray or FLT_MAX if no hit
/// Note: Can return negative value if ray starts in box
JPH_INLINE Vec4 RayAABox4(Vec3Arg inOrigin, const RayInvDirection &inInvDirection, Vec4Arg inBoundsMinX, Vec4Arg inBoundsMinY, Vec4Arg inBoundsMinZ, Vec4Arg inBoundsMaxX, Vec4Arg inBoundsMaxY, Vec4Arg inBoundsMaxZ)
{
	// Constants
	Vec4 flt_min = Vec4::sReplicate(-FLT_MAX);
	Vec4 flt_max = Vec4::sReplicate(FLT_MAX);

	// Origin
	Vec4 originx = inOrigin.SplatX();
	Vec4 originy = inOrigin.SplatY();
	Vec4 originz = inOrigin.SplatZ();

	// Parallel
	UVec4 parallelx = inInvDirection.mIsParallel.SplatX();
	UVec4 parallely = inInvDirection.mIsParallel.SplatY();
	UVec4 parallelz = inInvDirection.mIsParallel.SplatZ();

	// Inverse direction
	Vec4 invdirx = inInvDirection.mInvDirection.SplatX();
	Vec4 invdiry = inInvDirection.mInvDirection.SplatY();
	Vec4 invdirz = inInvDirection.mInvDirection.SplatZ();

	// Test against all three axii simultaneously.
	Vec4 t1x = (inBoundsMinX - originx) * invdirx;
	Vec4 t1y = (inBoundsMinY - originy) * invdiry;
	Vec4 t1z = (inBoundsMinZ - originz) * invdirz;
	Vec4 t2x = (inBoundsMaxX - originx) * invdirx;
	Vec4 t2y = (inBoundsMaxY - originy) * invdiry;
	Vec4 t2z = (inBoundsMaxZ - originz) * invdirz;

	// Compute the max of min(t1,t2) and the min of max(t1,t2) ensuring we don't
	// use the results from any directions parallel to the slab.
	Vec4 t_minx = Vec4::sSelect(Vec4::sMin(t1x, t2x), flt_min, parallelx);
	Vec4 t_miny = Vec4::sSelect(Vec4::sMin(t1y, t2y), flt_min, parallely);
	Vec4 t_minz = Vec4::sSelect(Vec4::sMin(t1z, t2z), flt_min, parallelz);
	Vec4 t_maxx = Vec4::sSelect(Vec4::sMax(t1x, t2x), flt_max, parallelx);
	Vec4 t_maxy = Vec4::sSelect(Vec4::sMax(t1y, t2y), flt_max, parallely);
	Vec4 t_maxz = Vec4::sSelect(Vec4::sMax(t1z, t2z), flt_max, parallelz);

	// t_min.xyz = maximum(t_min.x, t_min.y, t_min.z);
	Vec4 t_min = Vec4::sMax(Vec4::sMax(t_minx, t_miny), t_minz);

	// t_max.xyz = minimum(t_max.x, t_max.y, t_max.z);
	Vec4 t_max = Vec4::sMin(Vec4::sMin(t_maxx, t_maxy), t_maxz);

	// if (t_min > t_max) return FLT_MAX;
	UVec4 no_intersection = Vec4::sGreater(t_min, t_max);

	// if (t_max < 0.0f) return FLT_MAX;
	no_intersection = UVec4::sOr(no_intersection, Vec4::sLess(t_max, Vec4::sZero()));

	// if bounds are invalid return FLOAT_MAX;
	UVec4 bounds_invalid = UVec4::sOr(UVec4::sOr(Vec4::sGreater(inBoundsMinX, inBoundsMaxX), Vec4::sGreater(inBoundsMinY, inBoundsMaxY)), Vec4::sGreater(inBoundsMinZ, inBoundsMaxZ));
	no_intersection = UVec4::sOr(no_intersection, bounds_invalid);

	// if (inInvDirection.mIsParallel && !(Min <= inOrigin && inOrigin <= Max)) return FLT_MAX; else return t_min;
	UVec4 no_parallel_overlapx = UVec4::sAnd(parallelx, UVec4::sOr(Vec4::sLess(originx, inBoundsMinX), Vec4::sGreater(originx, inBoundsMaxX)));
	UVec4 no_parallel_overlapy = UVec4::sAnd(parallely, UVec4::sOr(Vec4::sLess(originy, inBoundsMinY), Vec4::sGreater(originy, inBoundsMaxY)));
	UVec4 no_parallel_overlapz = UVec4::sAnd(parallelz, UVec4::sOr(Vec4::sLess(originz, inBoundsMinZ), Vec4::sGreater(originz, inBoundsMaxZ)));
	no_intersection = UVec4::sOr(no_intersection, UVec4::sOr(UVec4::sOr(no_parallel_overlapx, no_parallel_overlapy), no_parallel_overlapz));
	return Vec4::sSelect(t_min, flt_max, no_intersection);
}

/// Intersect AABB with ray, returns minimal and maximal distance along ray or FLT_MAX, -FLT_MAX if no hit
/// Note: Can return negative value for outMin if ray starts in box
JPH_INLINE void RayAABox(Vec3Arg inOrigin, const RayInvDirection &inInvDirection, Vec3Arg inBoundsMin, Vec3Arg inBoundsMax, float &outMin, float &outMax)
{
	// Constants
	Vec3 flt_min = Vec3::sReplicate(-FLT_MAX);
	Vec3 flt_max = Vec3::sReplicate(FLT_MAX);

	// Test against all three axii simultaneously.
	Vec3 t1 = (inBoundsMin - inOrigin) * inInvDirection.mInvDirection;
	Vec3 t2 = (inBoundsMax - inOrigin) * inInvDirection.mInvDirection;

	// Compute the max of min(t1,t2) and the min of max(t1,t2) ensuring we don't
	// use the results from any directions parallel to the slab.
	Vec3 t_min = Vec3::sSelect(Vec3::sMin(t1, t2), flt_min, inInvDirection.mIsParallel);
	Vec3 t_max = Vec3::sSelect(Vec3::sMax(t1, t2), flt_max, inInvDirection.mIsParallel);

	// t_min.xyz = maximum(t_min.x, t_min.y, t_min.z);
	t_min = Vec3::sMax(t_min, t_min.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>());
	t_min = Vec3::sMax(t_min, t_min.Swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>());

	// t_max.xyz = minimum(t_max.x, t_max.y, t_max.z);
	t_max = Vec3::sMin(t_max, t_max.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>());
	t_max = Vec3::sMin(t_max, t_max.Swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>());

	// if (t_min > t_max) return FLT_MAX;
	UVec4 no_intersection = Vec3::sGreater(t_min, t_max);

	// if (t_max < 0.0f) return FLT_MAX;
	no_intersection = UVec4::sOr(no_intersection, Vec3::sLess(t_max, Vec3::sZero()));

	// if (inInvDirection.mIsParallel && !(Min <= inOrigin && inOrigin <= Max)) return FLT_MAX; else return t_min;
	UVec4 no_parallel_overlap = UVec4::sOr(Vec3::sLess(inOrigin, inBoundsMin), Vec3::sGreater(inOrigin, inBoundsMax));
	no_intersection = UVec4::sOr(no_intersection, UVec4::sAnd(inInvDirection.mIsParallel, no_parallel_overlap));
	no_intersection = UVec4::sOr(no_intersection, no_intersection.SplatY());
	no_intersection = UVec4::sOr(no_intersection, no_intersection.SplatZ());
	outMin = Vec3::sSelect(t_min, flt_max, no_intersection).GetX();
	outMax = Vec3::sSelect(t_max, flt_min, no_intersection).GetX();
}

/// Intersect AABB with ray, returns true if there is a hit closer than inClosest
JPH_INLINE bool RayAABoxHits(Vec3Arg inOrigin, const RayInvDirection &inInvDirection, Vec3Arg inBoundsMin, Vec3Arg inBoundsMax, float inClosest)
{
	// Constants
	Vec3 flt_min = Vec3::sReplicate(-FLT_MAX);
	Vec3 flt_max = Vec3::sReplicate(FLT_MAX);

	// Test against all three axii simultaneously.
	Vec3 t1 = (inBoundsMin - inOrigin) * inInvDirection.mInvDirection;
	Vec3 t2 = (inBoundsMax - inOrigin) * inInvDirection.mInvDirection;

	// Compute the max of min(t1,t2) and the min of max(t1,t2) ensuring we don't
	// use the results from any directions parallel to the slab.
	Vec3 t_min = Vec3::sSelect(Vec3::sMin(t1, t2), flt_min, inInvDirection.mIsParallel);
	Vec3 t_max = Vec3::sSelect(Vec3::sMax(t1, t2), flt_max, inInvDirection.mIsParallel);

	// t_min.xyz = maximum(t_min.x, t_min.y, t_min.z);
	t_min = Vec3::sMax(t_min, t_min.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>());
	t_min = Vec3::sMax(t_min, t_min.Swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>());

	// t_max.xyz = minimum(t_max.x, t_max.y, t_max.z);
	t_max = Vec3::sMin(t_max, t_max.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>());
	t_max = Vec3::sMin(t_max, t_max.Swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>());

	// if (t_min > t_max) return false;
	UVec4 no_intersection = Vec3::sGreater(t_min, t_max);

	// if (t_max < 0.0f) return false;
	no_intersection = UVec4::sOr(no_intersection, Vec3::sLess(t_max, Vec3::sZero()));

	// if (t_min > inClosest) return false;
	no_intersection = UVec4::sOr(no_intersection, Vec3::sGreater(t_min, Vec3::sReplicate(inClosest)));

	// if (inInvDirection.mIsParallel && !(Min <= inOrigin && inOrigin <= Max)) return false; else return true;
	UVec4 no_parallel_overlap = UVec4::sOr(Vec3::sLess(inOrigin, inBoundsMin), Vec3::sGreater(inOrigin, inBoundsMax));
	no_intersection = UVec4::sOr(no_intersection, UVec4::sAnd(inInvDirection.mIsParallel, no_parallel_overlap));

	return !no_intersection.TestAnyXYZTrue();
}

/// Intersect AABB with ray without hit fraction, based on separating axis test
/// @see http://www.codercorner.com/RayAABB.cpp
JPH_INLINE bool RayAABoxHits(Vec3Arg inOrigin, Vec3Arg inDirection, Vec3Arg inBoundsMin, Vec3Arg inBoundsMax)
{
	Vec3 extents = inBoundsMax - inBoundsMin;

	Vec3 diff = 2.0f * inOrigin - inBoundsMin - inBoundsMax;
	Vec3 abs_diff = diff.Abs();

	UVec4 no_intersection = UVec4::sAnd(Vec3::sGreater(abs_diff, extents), Vec3::sGreaterOrEqual(diff * inDirection, Vec3::sZero()));

	Vec3 abs_dir = inDirection.Abs();
	Vec3 abs_dir_yzz = abs_dir.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z>();
	Vec3 abs_dir_xyx = abs_dir.Swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>();

	Vec3 extents_yzz = extents.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z>();
	Vec3 extents_xyx = extents.Swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>();

	Vec3 diff_yzx = diff.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>();

	Vec3 dir_yzx = inDirection.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>();

	no_intersection = UVec4::sOr(no_intersection, Vec3::sGreater((inDirection * diff_yzx - dir_yzx * diff).Abs(), extents_xyx * abs_dir_yzz + extents_yzz * abs_dir_xyx));

	return !no_intersection.TestAnyXYZTrue();
}

JPH_NAMESPACE_END
