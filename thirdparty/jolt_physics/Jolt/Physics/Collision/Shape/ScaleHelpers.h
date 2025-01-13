// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/PhysicsSettings.h>

JPH_NAMESPACE_BEGIN

/// Helper functions to get properties of a scaling vector
namespace ScaleHelpers
{
	/// Minimum valid scale value. This is used to prevent division by zero when scaling a shape with a zero scale.
	static constexpr float	cMinScale = 1.0e-6f;

	/// The tolerance used to check if components of the scale vector are the same
	static constexpr float	cScaleToleranceSq = 1.0e-8f;

	/// Test if a scale is identity
	inline bool				IsNotScaled(Vec3Arg inScale)									{ return inScale.IsClose(Vec3::sReplicate(1.0f), cScaleToleranceSq); }

	/// Test if a scale is uniform
	inline bool				IsUniformScale(Vec3Arg inScale)									{ return inScale.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>().IsClose(inScale, cScaleToleranceSq); }

	/// Test if a scale is uniform in XZ
	inline bool				IsUniformScaleXZ(Vec3Arg inScale)								{ return inScale.Swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>().IsClose(inScale, ScaleHelpers::cScaleToleranceSq); }

	/// Scale the convex radius of an object
	inline float			ScaleConvexRadius(float inConvexRadius, Vec3Arg inScale)		{ return min(inConvexRadius * inScale.Abs().ReduceMin(), cDefaultConvexRadius); }

	/// Test if a scale flips an object inside out (which requires flipping all normals and polygon windings)
	inline bool				IsInsideOut(Vec3Arg inScale)									{ return (CountBits(Vec3::sLess(inScale, Vec3::sZero()).GetTrues() & 0x7) & 1) != 0; }

	/// Test if any of the components of the scale have a value below cMinScale
	inline bool				IsZeroScale(Vec3Arg inScale)									{ return Vec3::sLess(inScale.Abs(), Vec3::sReplicate(cMinScale)).TestAnyXYZTrue(); }

	/// Ensure that the scale for each component is at least cMinScale
	inline Vec3				MakeNonZeroScale(Vec3Arg inScale)								{ return inScale.GetSign() * Vec3::sMax(inScale.Abs(), Vec3::sReplicate(cMinScale)); }

	/// Get the average scale if inScale, used to make the scale uniform when a shape doesn't support non-uniform scale
	inline Vec3				MakeUniformScale(Vec3Arg inScale)								{ return Vec3::sReplicate((inScale.GetX() + inScale.GetY() + inScale.GetZ()) / 3.0f); }

	/// Average the scale in XZ, used to make the scale uniform when a shape doesn't support non-uniform scale in the XZ plane
	inline Vec3				MakeUniformScaleXZ(Vec3Arg inScale)								{ return 0.5f * (inScale + inScale.Swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>()); }

	/// Checks in scale can be rotated to child shape
	/// @param inRotation Rotation of child shape
	/// @param inScale Scale in local space of parent shape
	/// @return True if the scale is valid (no shearing introduced)
	inline bool				CanScaleBeRotated(QuatArg inRotation, Vec3Arg inScale)
	{
		// inScale is a scale in local space of the shape, so the transform for the shape (ignoring translation) is: T = Mat44::sScale(inScale) * mRotation.
		// when we pass the scale to the child it needs to be local to the child, so we want T = mRotation * Mat44::sScale(ChildScale).
		// Solving for ChildScale: ChildScale = mRotation^-1 * Mat44::sScale(inScale) * mRotation = mRotation^T * Mat44::sScale(inScale) * mRotation
		// If any of the off diagonal elements are non-zero, it means the scale / rotation is not compatible.
		Mat44 r = Mat44::sRotation(inRotation);
		Mat44 child_scale = r.Multiply3x3LeftTransposed(r.PostScaled(inScale));

		// Get the columns, but zero the diagonal
		Vec4 zero = Vec4::sZero();
		Vec4 c0 = Vec4::sSelect(child_scale.GetColumn4(0), zero, UVec4(0xffffffff, 0, 0, 0)).Abs();
		Vec4 c1 = Vec4::sSelect(child_scale.GetColumn4(1), zero, UVec4(0, 0xffffffff, 0, 0)).Abs();
		Vec4 c2 = Vec4::sSelect(child_scale.GetColumn4(2), zero, UVec4(0, 0, 0xffffffff, 0)).Abs();

		// Check if all elements are less than epsilon
		Vec4 epsilon = Vec4::sReplicate(1.0e-6f);
		return UVec4::sAnd(UVec4::sAnd(Vec4::sLess(c0, epsilon), Vec4::sLess(c1, epsilon)), Vec4::sLess(c2, epsilon)).TestAllTrue();
	}

	/// Adjust scale for rotated child shape
	/// @param inRotation Rotation of child shape
	/// @param inScale Scale in local space of parent shape
	/// @return Rotated scale
	inline Vec3				RotateScale(QuatArg inRotation, Vec3Arg inScale)
	{
		// Get the diagonal of mRotation^T * Mat44::sScale(inScale) * mRotation (see comment at CanScaleBeRotated)
		Mat44 r = Mat44::sRotation(inRotation);
		return r.Multiply3x3LeftTransposed(r.PostScaled(inScale)).GetDiagonal3();
	}
}

JPH_NAMESPACE_END
