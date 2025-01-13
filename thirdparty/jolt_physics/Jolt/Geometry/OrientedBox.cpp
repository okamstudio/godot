// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Geometry/OrientedBox.h>
#include <Jolt/Geometry/AABox.h>

JPH_NAMESPACE_BEGIN

bool OrientedBox::Overlaps(const AABox &inBox, float inEpsilon) const
{
	// Taken from: Real Time Collision Detection - Christer Ericson
	// Chapter 4.4.1, page 103-105.
	// Note that the code is swapped around: A is the aabox and B is the oriented box (this saves us from having to invert the orientation of the oriented box)

	// Convert AABox to center / extent representation
	Vec3 a_center = inBox.GetCenter();
	Vec3 a_half_extents = inBox.GetExtent();

	// Compute rotation matrix expressing b in a's coordinate frame
	Mat44 rot(mOrientation.GetColumn4(0), mOrientation.GetColumn4(1), mOrientation.GetColumn4(2), mOrientation.GetColumn4(3) - Vec4(a_center, 0));

	// Compute common subexpressions. Add in an epsilon term to
	// counteract arithmetic errors when two edges are parallel and
	// their cross product is (near) null (see text for details)
	Vec3 epsilon = Vec3::sReplicate(inEpsilon);
	Vec3 abs_r[3] { rot.GetAxisX().Abs() + epsilon, rot.GetAxisY().Abs() + epsilon, rot.GetAxisZ().Abs() + epsilon };

	// Test axes L = A0, L = A1, L = A2
	float ra, rb;
	for (int i = 0; i < 3; i++)
	{
		ra = a_half_extents[i];
		rb = mHalfExtents[0] * abs_r[0][i] + mHalfExtents[1] * abs_r[1][i] + mHalfExtents[2] * abs_r[2][i];
		if (abs(rot(i, 3)) > ra + rb) return false;
	}

	// Test axes L = B0, L = B1, L = B2
	for (int i = 0; i < 3; i++)
	{
		ra = a_half_extents.Dot(abs_r[i]);
		rb = mHalfExtents[i];
		if (abs(rot.GetTranslation().Dot(rot.GetColumn3(i))) > ra + rb) return false;
	}

	// Test axis L = A0 x B0
	ra = a_half_extents[1] * abs_r[0][2] + a_half_extents[2] * abs_r[0][1];
	rb = mHalfExtents[1] * abs_r[2][0] + mHalfExtents[2] * abs_r[1][0];
	if (abs(rot(2, 3) * rot(1, 0) - rot(1, 3) * rot(2, 0)) > ra + rb) return false;

	// Test axis L = A0 x B1
	ra = a_half_extents[1] * abs_r[1][2] + a_half_extents[2] * abs_r[1][1];
	rb = mHalfExtents[0] * abs_r[2][0] + mHalfExtents[2] * abs_r[0][0];
	if (abs(rot(2, 3) * rot(1, 1) - rot(1, 3) * rot(2, 1)) > ra + rb) return false;

	// Test axis L = A0 x B2
	ra = a_half_extents[1] * abs_r[2][2] + a_half_extents[2] * abs_r[2][1];
	rb = mHalfExtents[0] * abs_r[1][0] + mHalfExtents[1] * abs_r[0][0];
	if (abs(rot(2, 3) * rot(1, 2) - rot(1, 3) * rot(2, 2)) > ra + rb) return false;

	// Test axis L = A1 x B0
	ra = a_half_extents[0] * abs_r[0][2] + a_half_extents[2] * abs_r[0][0];
	rb = mHalfExtents[1] * abs_r[2][1] + mHalfExtents[2] * abs_r[1][1];
	if (abs(rot(0, 3) * rot(2, 0) - rot(2, 3) * rot(0, 0)) > ra + rb) return false;

	// Test axis L = A1 x B1
	ra = a_half_extents[0] * abs_r[1][2] + a_half_extents[2] * abs_r[1][0];
	rb = mHalfExtents[0] * abs_r[2][1] + mHalfExtents[2] * abs_r[0][1];
	if (abs(rot(0, 3) * rot(2, 1) - rot(2, 3) * rot(0, 1)) > ra + rb) return false;

	// Test axis L = A1 x B2
	ra = a_half_extents[0] * abs_r[2][2] + a_half_extents[2] * abs_r[2][0];
	rb = mHalfExtents[0] * abs_r[1][1] + mHalfExtents[1] * abs_r[0][1];
	if (abs(rot(0, 3) * rot(2, 2) - rot(2, 3) * rot(0, 2)) > ra + rb) return false;

	// Test axis L = A2 x B0
	ra = a_half_extents[0] * abs_r[0][1] + a_half_extents[1] * abs_r[0][0];
	rb = mHalfExtents[1] * abs_r[2][2] + mHalfExtents[2] * abs_r[1][2];
	if (abs(rot(1, 3) * rot(0, 0) - rot(0, 3) * rot(1, 0)) > ra + rb) return false;

	// Test axis L = A2 x B1
	ra = a_half_extents[0] * abs_r[1][1] + a_half_extents[1] * abs_r[1][0];
	rb = mHalfExtents[0] * abs_r[2][2] + mHalfExtents[2] * abs_r[0][2];
	if (abs(rot(1, 3) * rot(0, 1) - rot(0, 3) * rot(1, 1)) > ra + rb) return false;

	// Test axis L = A2 x B2
	ra = a_half_extents[0] * abs_r[2][1] + a_half_extents[1] * abs_r[2][0];
	rb = mHalfExtents[0] * abs_r[1][2] + mHalfExtents[1] * abs_r[0][2];
	if (abs(rot(1, 3) * rot(0, 2) - rot(0, 3) * rot(1, 2)) > ra + rb) return false;

	// Since no separating axis is found, the OBB and AAB must be intersecting
	return true;
}

bool OrientedBox::Overlaps(const OrientedBox &inBox, float inEpsilon) const
{
	// Taken from: Real Time Collision Detection - Christer Ericson
	// Chapter 4.4.1, page 103-105.
	// Note that A is this, B is inBox

	// Compute rotation matrix expressing b in a's coordinate frame
	Mat44 rot = mOrientation.InversedRotationTranslation() * inBox.mOrientation;

	// Compute common subexpressions. Add in an epsilon term to
	// counteract arithmetic errors when two edges are parallel and
	// their cross product is (near) null (see text for details)
	Vec3 epsilon = Vec3::sReplicate(inEpsilon);
	Vec3 abs_r[3] { rot.GetAxisX().Abs() + epsilon, rot.GetAxisY().Abs() + epsilon, rot.GetAxisZ().Abs() + epsilon };

	// Test axes L = A0, L = A1, L = A2
	float ra, rb;
	for (int i = 0; i < 3; i++)
	{
		ra = mHalfExtents[i];
		rb = inBox.mHalfExtents[0] * abs_r[0][i] + inBox.mHalfExtents[1] * abs_r[1][i] + inBox.mHalfExtents[2] * abs_r[2][i];
		if (abs(rot(i, 3)) > ra + rb) return false;
	}

	// Test axes L = B0, L = B1, L = B2
	for (int i = 0; i < 3; i++)
	{
		ra = mHalfExtents.Dot(abs_r[i]);
		rb = inBox.mHalfExtents[i];
		if (abs(rot.GetTranslation().Dot(rot.GetColumn3(i))) > ra + rb) return false;
	}

	// Test axis L = A0 x B0
	ra = mHalfExtents[1] * abs_r[0][2] + mHalfExtents[2] * abs_r[0][1];
	rb = inBox.mHalfExtents[1] * abs_r[2][0] + inBox.mHalfExtents[2] * abs_r[1][0];
	if (abs(rot(2, 3) * rot(1, 0) - rot(1, 3) * rot(2, 0)) > ra + rb) return false;

	// Test axis L = A0 x B1
	ra = mHalfExtents[1] * abs_r[1][2] + mHalfExtents[2] * abs_r[1][1];
	rb = inBox.mHalfExtents[0] * abs_r[2][0] + inBox.mHalfExtents[2] * abs_r[0][0];
	if (abs(rot(2, 3) * rot(1, 1) - rot(1, 3) * rot(2, 1)) > ra + rb) return false;

	// Test axis L = A0 x B2
	ra = mHalfExtents[1] * abs_r[2][2] + mHalfExtents[2] * abs_r[2][1];
	rb = inBox.mHalfExtents[0] * abs_r[1][0] + inBox.mHalfExtents[1] * abs_r[0][0];
	if (abs(rot(2, 3) * rot(1, 2) - rot(1, 3) * rot(2, 2)) > ra + rb) return false;

	// Test axis L = A1 x B0
	ra = mHalfExtents[0] * abs_r[0][2] + mHalfExtents[2] * abs_r[0][0];
	rb = inBox.mHalfExtents[1] * abs_r[2][1] + inBox.mHalfExtents[2] * abs_r[1][1];
	if (abs(rot(0, 3) * rot(2, 0) - rot(2, 3) * rot(0, 0)) > ra + rb) return false;

	// Test axis L = A1 x B1
	ra = mHalfExtents[0] * abs_r[1][2] + mHalfExtents[2] * abs_r[1][0];
	rb = inBox.mHalfExtents[0] * abs_r[2][1] + inBox.mHalfExtents[2] * abs_r[0][1];
	if (abs(rot(0, 3) * rot(2, 1) - rot(2, 3) * rot(0, 1)) > ra + rb) return false;

	// Test axis L = A1 x B2
	ra = mHalfExtents[0] * abs_r[2][2] + mHalfExtents[2] * abs_r[2][0];
	rb = inBox.mHalfExtents[0] * abs_r[1][1] + inBox.mHalfExtents[1] * abs_r[0][1];
	if (abs(rot(0, 3) * rot(2, 2) - rot(2, 3) * rot(0, 2)) > ra + rb) return false;

	// Test axis L = A2 x B0
	ra = mHalfExtents[0] * abs_r[0][1] + mHalfExtents[1] * abs_r[0][0];
	rb = inBox.mHalfExtents[1] * abs_r[2][2] + inBox.mHalfExtents[2] * abs_r[1][2];
	if (abs(rot(1, 3) * rot(0, 0) - rot(0, 3) * rot(1, 0)) > ra + rb) return false;

	// Test axis L = A2 x B1
	ra = mHalfExtents[0] * abs_r[1][1] + mHalfExtents[1] * abs_r[1][0];
	rb = inBox.mHalfExtents[0] * abs_r[2][2] + inBox.mHalfExtents[2] * abs_r[0][2];
	if (abs(rot(1, 3) * rot(0, 1) - rot(0, 3) * rot(1, 1)) > ra + rb) return false;

	// Test axis L = A2 x B2
	ra = mHalfExtents[0] * abs_r[2][1] + mHalfExtents[1] * abs_r[2][0];
	rb = inBox.mHalfExtents[0] * abs_r[1][2] + inBox.mHalfExtents[1] * abs_r[0][2];
	if (abs(rot(1, 3) * rot(0, 2) - rot(0, 3) * rot(1, 2)) > ra + rb) return false;

	// Since no separating axis is found, the OBBs must be intersecting
	return true;
}

JPH_NAMESPACE_END
