// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/OrientedBox.h>

JPH_NAMESPACE_BEGIN

/// Helper functions that process 4 axis aligned boxes at the same time using SIMD
/// Test if 4 bounding boxes overlap with 1 bounding box, splat 1 box
JPH_INLINE UVec4 AABox4VsBox(const AABox &inBox1, Vec4Arg inBox2MinX, Vec4Arg inBox2MinY, Vec4Arg inBox2MinZ, Vec4Arg inBox2MaxX, Vec4Arg inBox2MaxY, Vec4Arg inBox2MaxZ)
{
	// Splat values of box 1
	Vec4 box1_minx = inBox1.mMin.SplatX();
	Vec4 box1_miny = inBox1.mMin.SplatY();
	Vec4 box1_minz = inBox1.mMin.SplatZ();
	Vec4 box1_maxx = inBox1.mMax.SplatX();
	Vec4 box1_maxy = inBox1.mMax.SplatY();
	Vec4 box1_maxz = inBox1.mMax.SplatZ();

	// Test separation over each axis
	UVec4 nooverlapx = UVec4::sOr(Vec4::sGreater(box1_minx, inBox2MaxX), Vec4::sGreater(inBox2MinX, box1_maxx));
	UVec4 nooverlapy = UVec4::sOr(Vec4::sGreater(box1_miny, inBox2MaxY), Vec4::sGreater(inBox2MinY, box1_maxy));
	UVec4 nooverlapz = UVec4::sOr(Vec4::sGreater(box1_minz, inBox2MaxZ), Vec4::sGreater(inBox2MinZ, box1_maxz));

	// Return overlap
	return UVec4::sNot(UVec4::sOr(UVec4::sOr(nooverlapx, nooverlapy), nooverlapz));
}

/// Scale 4 axis aligned boxes
JPH_INLINE void AABox4Scale(Vec3Arg inScale, Vec4Arg inBoxMinX, Vec4Arg inBoxMinY, Vec4Arg inBoxMinZ, Vec4Arg inBoxMaxX, Vec4Arg inBoxMaxY, Vec4Arg inBoxMaxZ, Vec4 &outBoundsMinX, Vec4 &outBoundsMinY, Vec4 &outBoundsMinZ, Vec4 &outBoundsMaxX, Vec4 &outBoundsMaxY, Vec4 &outBoundsMaxZ)
{
	Vec4 scale_x = inScale.SplatX();
	Vec4 scaled_min_x = scale_x * inBoxMinX;
	Vec4 scaled_max_x = scale_x * inBoxMaxX;
	outBoundsMinX = Vec4::sMin(scaled_min_x, scaled_max_x); // Negative scale can flip min and max
	outBoundsMaxX = Vec4::sMax(scaled_min_x, scaled_max_x);

	Vec4 scale_y = inScale.SplatY();
	Vec4 scaled_min_y = scale_y * inBoxMinY;
	Vec4 scaled_max_y = scale_y * inBoxMaxY;
	outBoundsMinY = Vec4::sMin(scaled_min_y, scaled_max_y);
	outBoundsMaxY = Vec4::sMax(scaled_min_y, scaled_max_y);

	Vec4 scale_z = inScale.SplatZ();
	Vec4 scaled_min_z = scale_z * inBoxMinZ;
	Vec4 scaled_max_z = scale_z * inBoxMaxZ;
	outBoundsMinZ = Vec4::sMin(scaled_min_z, scaled_max_z);
	outBoundsMaxZ = Vec4::sMax(scaled_min_z, scaled_max_z);
}

/// Enlarge 4 bounding boxes with extent (add to both sides)
JPH_INLINE void AABox4EnlargeWithExtent(Vec3Arg inExtent, Vec4 &ioBoundsMinX, Vec4 &ioBoundsMinY, Vec4 &ioBoundsMinZ, Vec4 &ioBoundsMaxX, Vec4 &ioBoundsMaxY, Vec4 &ioBoundsMaxZ)
{
	Vec4 extent_x = inExtent.SplatX();
	ioBoundsMinX -= extent_x;
	ioBoundsMaxX += extent_x;

	Vec4 extent_y = inExtent.SplatY();
	ioBoundsMinY -= extent_y;
	ioBoundsMaxY += extent_y;

	Vec4 extent_z = inExtent.SplatZ();
	ioBoundsMinZ -= extent_z;
	ioBoundsMaxZ += extent_z;
}

/// Test if 4 bounding boxes overlap with a point
JPH_INLINE UVec4 AABox4VsPoint(Vec3Arg inPoint, Vec4Arg inBoxMinX, Vec4Arg inBoxMinY, Vec4Arg inBoxMinZ, Vec4Arg inBoxMaxX, Vec4Arg inBoxMaxY, Vec4Arg inBoxMaxZ)
{
	// Splat point to 4 component vectors
	Vec4 point_x = Vec4(inPoint).SplatX();
	Vec4 point_y = Vec4(inPoint).SplatY();
	Vec4 point_z = Vec4(inPoint).SplatZ();

	// Test if point overlaps with box
	UVec4 overlapx = UVec4::sAnd(Vec4::sGreaterOrEqual(point_x, inBoxMinX), Vec4::sLessOrEqual(point_x, inBoxMaxX));
	UVec4 overlapy = UVec4::sAnd(Vec4::sGreaterOrEqual(point_y, inBoxMinY), Vec4::sLessOrEqual(point_y, inBoxMaxY));
	UVec4 overlapz = UVec4::sAnd(Vec4::sGreaterOrEqual(point_z, inBoxMinZ), Vec4::sLessOrEqual(point_z, inBoxMaxZ));

	// Test if all are overlapping
	return UVec4::sAnd(UVec4::sAnd(overlapx, overlapy), overlapz);
}

/// Test if 4 bounding boxes overlap with an oriented box
JPH_INLINE UVec4 AABox4VsBox(Mat44Arg inOrientation, Vec3Arg inHalfExtents, Vec4Arg inBoxMinX, Vec4Arg inBoxMinY, Vec4Arg inBoxMinZ, Vec4Arg inBoxMaxX, Vec4Arg inBoxMaxY, Vec4Arg inBoxMaxZ, float inEpsilon = 1.0e-6f)
{
	// Taken from: Real Time Collision Detection - Christer Ericson
	// Chapter 4.4.1, page 103-105.
	// Note that the code is swapped around: A is the aabox and B is the oriented box (this saves us from having to invert the orientation of the oriented box)

	// Compute translation vector t (the translation of B in the space of A)
	Vec4 t[3] {
		inOrientation.GetTranslation().SplatX() - 0.5f * (inBoxMinX + inBoxMaxX),
		inOrientation.GetTranslation().SplatY() - 0.5f * (inBoxMinY + inBoxMaxY),
		inOrientation.GetTranslation().SplatZ() - 0.5f * (inBoxMinZ + inBoxMaxZ) };

	// Compute common subexpressions. Add in an epsilon term to
	// counteract arithmetic errors when two edges are parallel and
	// their cross product is (near) null (see text for details)
	Vec3 epsilon = Vec3::sReplicate(inEpsilon);
	Vec3 abs_r[3] { inOrientation.GetAxisX().Abs() + epsilon, inOrientation.GetAxisY().Abs() + epsilon, inOrientation.GetAxisZ().Abs() + epsilon };

	// Half extents for a
	Vec4 a_half_extents[3] {
		0.5f * (inBoxMaxX - inBoxMinX),
		0.5f * (inBoxMaxY - inBoxMinY),
		0.5f * (inBoxMaxZ - inBoxMinZ) };

	// Half extents of b
	Vec4 b_half_extents_x = inHalfExtents.SplatX();
	Vec4 b_half_extents_y = inHalfExtents.SplatY();
	Vec4 b_half_extents_z = inHalfExtents.SplatZ();

	// Each component corresponds to 1 overlapping OBB vs ABB
	UVec4 overlaps = UVec4(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff);

	// Test axes L = A0, L = A1, L = A2
	Vec4 ra, rb;
	for (int i = 0; i < 3; i++)
	{
		ra = a_half_extents[i];
		rb = b_half_extents_x * abs_r[0][i] + b_half_extents_y * abs_r[1][i] + b_half_extents_z * abs_r[2][i];
		overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual(t[i].Abs(), ra + rb));
	}

	// Test axes L = B0, L = B1, L = B2
	for (int i = 0; i < 3; i++)
	{
		ra = a_half_extents[0] * abs_r[i][0] + a_half_extents[1] * abs_r[i][1] + a_half_extents[2] * abs_r[i][2];
		rb = Vec4::sReplicate(inHalfExtents[i]);
		overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[0] * inOrientation(0, i) + t[1] * inOrientation(1, i) + t[2] * inOrientation(2, i)).Abs(), ra + rb));
	}

	// Test axis L = A0 x B0
	ra = a_half_extents[1] * abs_r[0][2] + a_half_extents[2] * abs_r[0][1];
	rb = b_half_extents_y * abs_r[2][0] + b_half_extents_z * abs_r[1][0];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[2] * inOrientation(1, 0) - t[1] * inOrientation(2, 0)).Abs(), ra + rb));

	// Test axis L = A0 x B1
	ra = a_half_extents[1] * abs_r[1][2] + a_half_extents[2] * abs_r[1][1];
	rb = b_half_extents_x * abs_r[2][0] + b_half_extents_z * abs_r[0][0];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[2] * inOrientation(1, 1) - t[1] * inOrientation(2, 1)).Abs(), ra + rb));

	// Test axis L = A0 x B2
	ra = a_half_extents[1] * abs_r[2][2] + a_half_extents[2] * abs_r[2][1];
	rb = b_half_extents_x * abs_r[1][0] + b_half_extents_y * abs_r[0][0];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[2] * inOrientation(1, 2) - t[1] * inOrientation(2, 2)).Abs(), ra + rb));

	// Test axis L = A1 x B0
	ra = a_half_extents[0] * abs_r[0][2] + a_half_extents[2] * abs_r[0][0];
	rb = b_half_extents_y * abs_r[2][1] + b_half_extents_z * abs_r[1][1];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[0] * inOrientation(2, 0) - t[2] * inOrientation(0, 0)).Abs(), ra + rb));

	// Test axis L = A1 x B1
	ra = a_half_extents[0] * abs_r[1][2] + a_half_extents[2] * abs_r[1][0];
	rb = b_half_extents_x * abs_r[2][1] + b_half_extents_z * abs_r[0][1];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[0] * inOrientation(2, 1) - t[2] * inOrientation(0, 1)).Abs(), ra + rb));

	// Test axis L = A1 x B2
	ra = a_half_extents[0] * abs_r[2][2] + a_half_extents[2] * abs_r[2][0];
	rb = b_half_extents_x * abs_r[1][1] + b_half_extents_y * abs_r[0][1];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[0] * inOrientation(2, 2) - t[2] * inOrientation(0, 2)).Abs(), ra + rb));

	// Test axis L = A2 x B0
	ra = a_half_extents[0] * abs_r[0][1] + a_half_extents[1] * abs_r[0][0];
	rb = b_half_extents_y * abs_r[2][2] + b_half_extents_z * abs_r[1][2];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[1] * inOrientation(0, 0) - t[0] * inOrientation(1, 0)).Abs(), ra + rb));

	// Test axis L = A2 x B1
	ra = a_half_extents[0] * abs_r[1][1] + a_half_extents[1] * abs_r[1][0];
	rb = b_half_extents_x * abs_r[2][2] + b_half_extents_z * abs_r[0][2];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[1] * inOrientation(0, 1) - t[0] * inOrientation(1, 1)).Abs(), ra + rb));

	// Test axis L = A2 x B2
	ra = a_half_extents[0] * abs_r[2][1] + a_half_extents[1] * abs_r[2][0];
	rb = b_half_extents_x * abs_r[1][2] + b_half_extents_y * abs_r[0][2];
	overlaps = UVec4::sAnd(overlaps, Vec4::sLessOrEqual((t[1] * inOrientation(0, 2) - t[0] * inOrientation(1, 2)).Abs(), ra + rb));

	// Return if the OBB vs AABBs are intersecting
	return overlaps;
}

/// Convenience function that tests 4 AABoxes vs OrientedBox
JPH_INLINE UVec4 AABox4VsBox(const OrientedBox &inBox, Vec4Arg inBoxMinX, Vec4Arg inBoxMinY, Vec4Arg inBoxMinZ, Vec4Arg inBoxMaxX, Vec4Arg inBoxMaxY, Vec4Arg inBoxMaxZ, float inEpsilon = 1.0e-6f)
{
	return AABox4VsBox(inBox.mOrientation, inBox.mHalfExtents, inBoxMinX, inBoxMinY, inBoxMinZ, inBoxMaxX, inBoxMaxY, inBoxMaxZ, inEpsilon);
}

/// Get the squared distance between 4 AABoxes and a point
JPH_INLINE Vec4 AABox4DistanceSqToPoint(Vec4Arg inPointX, Vec4Arg inPointY, Vec4Arg inPointZ, Vec4Arg inBoxMinX, Vec4Arg inBoxMinY, Vec4Arg inBoxMinZ, Vec4Arg inBoxMaxX, Vec4Arg inBoxMaxY, Vec4Arg inBoxMaxZ)
{
	// Get closest point on box
	Vec4 closest_x = Vec4::sMin(Vec4::sMax(inPointX, inBoxMinX), inBoxMaxX);
	Vec4 closest_y = Vec4::sMin(Vec4::sMax(inPointY, inBoxMinY), inBoxMaxY);
	Vec4 closest_z = Vec4::sMin(Vec4::sMax(inPointZ, inBoxMinZ), inBoxMaxZ);

	// Return the squared distance between the box and point
	return Square(closest_x - inPointX) + Square(closest_y - inPointY) + Square(closest_z - inPointZ);
}

/// Get the squared distance between 4 AABoxes and a point
JPH_INLINE Vec4 AABox4DistanceSqToPoint(Vec3 inPoint, Vec4Arg inBoxMinX, Vec4Arg inBoxMinY, Vec4Arg inBoxMinZ, Vec4Arg inBoxMaxX, Vec4Arg inBoxMaxY, Vec4Arg inBoxMaxZ)
{
	return AABox4DistanceSqToPoint(inPoint.SplatX(), inPoint.SplatY(), inPoint.SplatZ(), inBoxMinX, inBoxMinY, inBoxMinZ, inBoxMaxX, inBoxMaxY, inBoxMaxZ);
}

/// Test 4 AABoxes vs a sphere
JPH_INLINE UVec4 AABox4VsSphere(Vec4Arg inCenterX, Vec4Arg inCenterY, Vec4Arg inCenterZ, Vec4Arg inRadiusSq, Vec4Arg inBoxMinX, Vec4Arg inBoxMinY, Vec4Arg inBoxMinZ, Vec4Arg inBoxMaxX, Vec4Arg inBoxMaxY, Vec4Arg inBoxMaxZ)
{
	// Test the distance from the center of the sphere to the box is smaller than the radius
	Vec4 distance_sq = AABox4DistanceSqToPoint(inCenterX, inCenterY, inCenterZ, inBoxMinX, inBoxMinY, inBoxMinZ, inBoxMaxX, inBoxMaxY, inBoxMaxZ);
	return Vec4::sLessOrEqual(distance_sq, inRadiusSq);
}

/// Test 4 AABoxes vs a sphere
JPH_INLINE UVec4 AABox4VsSphere(Vec3Arg inCenter, float inRadiusSq, Vec4Arg inBoxMinX, Vec4Arg inBoxMinY, Vec4Arg inBoxMinZ, Vec4Arg inBoxMaxX, Vec4Arg inBoxMaxY, Vec4Arg inBoxMaxZ)
{
	return AABox4VsSphere(inCenter.SplatX(), inCenter.SplatY(), inCenter.SplatZ(), Vec4::sReplicate(inRadiusSq), inBoxMinX, inBoxMinY, inBoxMinZ, inBoxMaxX, inBoxMaxY, inBoxMaxZ);
}

JPH_NAMESPACE_END
