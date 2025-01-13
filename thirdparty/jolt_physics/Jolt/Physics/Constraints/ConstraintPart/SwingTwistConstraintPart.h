// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Geometry/Ellipse.h>
#include <Jolt/Physics/Constraints/ConstraintPart/RotationEulerConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AngleConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// How the swing limit behaves
enum class ESwingType : uint8
{
	Cone,						///< Swing is limited by a cone shape, note that this cone starts to deform for larger swing angles. Cone limits only support limits that are symmetric around 0.
	Pyramid,					///< Swing is limited by a pyramid shape, note that this pyramid starts to deform for larger swing angles.
};

/// Quaternion based constraint that decomposes the rotation in constraint space in swing and twist: q = q_swing * q_twist
/// where q_swing.x = 0 and where q_twist.y = q_twist.z = 0
///
/// - Rotation around the twist (x-axis) is within [inTwistMinAngle, inTwistMaxAngle].
/// - Rotation around the swing axis (y and z axis) are limited to an ellipsoid in quaternion space formed by the equation:
///
/// (q_swing.y / sin(inSwingYHalfAngle / 2))^2 + (q_swing.z / sin(inSwingZHalfAngle / 2))^2 <= 1
///
/// Which roughly corresponds to an elliptic cone shape with major axis (inSwingYHalfAngle, inSwingZHalfAngle).
///
/// In case inSwingYHalfAngle = 0, the rotation around Y will be constrained to 0 and the rotation around Z
/// will be constrained between [-inSwingZHalfAngle, inSwingZHalfAngle]. Vice versa if inSwingZHalfAngle = 0.
class SwingTwistConstraintPart
{
public:
	/// Override the swing type
	void						SetSwingType(ESwingType inSwingType)
	{
		mSwingType = inSwingType;
	}

	/// Get the swing type for this part
	ESwingType					GetSwingType() const
	{
		return mSwingType;
	}

	/// Set limits for this constraint (see description above for parameters)
	void						SetLimits(float inTwistMinAngle, float inTwistMaxAngle, float inSwingYMinAngle, float inSwingYMaxAngle, float inSwingZMinAngle, float inSwingZMaxAngle)
	{
		constexpr float cLockedAngle = DegreesToRadians(0.5f);
		constexpr float cFreeAngle = DegreesToRadians(179.5f);

		// Assume sane input
		JPH_ASSERT(inTwistMinAngle <= inTwistMaxAngle);
		JPH_ASSERT(inSwingYMinAngle <= inSwingYMaxAngle);
		JPH_ASSERT(inSwingZMinAngle <= inSwingZMaxAngle);
		JPH_ASSERT(inSwingYMinAngle >= -JPH_PI && inSwingYMaxAngle <= JPH_PI);
		JPH_ASSERT(inSwingZMinAngle >= -JPH_PI && inSwingZMaxAngle <= JPH_PI);

		// Calculate the sine and cosine of the half angles
		Vec4 half_twist = 0.5f * Vec4(inTwistMinAngle, inTwistMaxAngle, 0, 0);
		Vec4 twist_s, twist_c;
		half_twist.SinCos(twist_s, twist_c);
		Vec4 half_swing = 0.5f * Vec4(inSwingYMinAngle, inSwingYMaxAngle, inSwingZMinAngle, inSwingZMaxAngle);
		Vec4 swing_s, swing_c;
		half_swing.SinCos(swing_s, swing_c);

		// Store half angles for pyramid limit
		mSwingYHalfMinAngle = half_swing.GetX();
		mSwingYHalfMaxAngle = half_swing.GetY();
		mSwingZHalfMinAngle = half_swing.GetZ();
		mSwingZHalfMaxAngle = half_swing.GetW();

		// Store axis flags which are used at runtime to quickly decided which constraints to apply
		mRotationFlags = 0;
		if (inTwistMinAngle > -cLockedAngle && inTwistMaxAngle < cLockedAngle)
		{
			mRotationFlags |= TwistXLocked;
			mSinTwistHalfMinAngle = 0.0f;
			mSinTwistHalfMaxAngle = 0.0f;
			mCosTwistHalfMinAngle = 1.0f;
			mCosTwistHalfMaxAngle = 1.0f;
		}
		else if (inTwistMinAngle < -cFreeAngle && inTwistMaxAngle > cFreeAngle)
		{
			mRotationFlags |= TwistXFree;
			mSinTwistHalfMinAngle = -1.0f;
			mSinTwistHalfMaxAngle = 1.0f;
			mCosTwistHalfMinAngle = 0.0f;
			mCosTwistHalfMaxAngle = 0.0f;
		}
		else
		{
			mSinTwistHalfMinAngle = twist_s.GetX();
			mSinTwistHalfMaxAngle = twist_s.GetY();
			mCosTwistHalfMinAngle = twist_c.GetX();
			mCosTwistHalfMaxAngle = twist_c.GetY();
		}

		if (inSwingYMinAngle > -cLockedAngle && inSwingYMaxAngle < cLockedAngle)
		{
			mRotationFlags |= SwingYLocked;
			mSinSwingYHalfMinAngle = 0.0f;
			mSinSwingYHalfMaxAngle = 0.0f;
			mCosSwingYHalfMinAngle = 1.0f;
			mCosSwingYHalfMaxAngle = 1.0f;
		}
		else if (inSwingYMinAngle < -cFreeAngle && inSwingYMaxAngle > cFreeAngle)
		{
			mRotationFlags |= SwingYFree;
			mSinSwingYHalfMinAngle = -1.0f;
			mSinSwingYHalfMaxAngle = 1.0f;
			mCosSwingYHalfMinAngle = 0.0f;
			mCosSwingYHalfMaxAngle = 0.0f;
		}
		else
		{
			mSinSwingYHalfMinAngle = swing_s.GetX();
			mSinSwingYHalfMaxAngle = swing_s.GetY();
			mCosSwingYHalfMinAngle = swing_c.GetX();
			mCosSwingYHalfMaxAngle = swing_c.GetY();
			JPH_ASSERT(mSinSwingYHalfMinAngle <= mSinSwingYHalfMaxAngle);
		}

		if (inSwingZMinAngle > -cLockedAngle && inSwingZMaxAngle < cLockedAngle)
		{
			mRotationFlags |= SwingZLocked;
			mSinSwingZHalfMinAngle = 0.0f;
			mSinSwingZHalfMaxAngle = 0.0f;
			mCosSwingZHalfMinAngle = 1.0f;
			mCosSwingZHalfMaxAngle = 1.0f;
		}
		else if (inSwingZMinAngle < -cFreeAngle && inSwingZMaxAngle > cFreeAngle)
		{
			mRotationFlags |= SwingZFree;
			mSinSwingZHalfMinAngle = -1.0f;
			mSinSwingZHalfMaxAngle = 1.0f;
			mCosSwingZHalfMinAngle = 0.0f;
			mCosSwingZHalfMaxAngle = 0.0f;
		}
		else
		{
			mSinSwingZHalfMinAngle = swing_s.GetZ();
			mSinSwingZHalfMaxAngle = swing_s.GetW();
			mCosSwingZHalfMinAngle = swing_c.GetZ();
			mCosSwingZHalfMaxAngle = swing_c.GetW();
			JPH_ASSERT(mSinSwingZHalfMinAngle <= mSinSwingZHalfMaxAngle);
		}
	}

	/// Flags to indicate which axis got clamped by ClampSwingTwist
	static constexpr uint		cClampedTwistMin = 1 << 0;
	static constexpr uint		cClampedTwistMax = 1 << 1;
	static constexpr uint		cClampedSwingYMin = 1 << 2;
	static constexpr uint		cClampedSwingYMax = 1 << 3;
	static constexpr uint		cClampedSwingZMin = 1 << 4;
	static constexpr uint		cClampedSwingZMax = 1 << 5;

	/// Helper function to determine if we're clamped against the min or max limit
	static JPH_INLINE bool		sDistanceToMinShorter(float inDeltaMin, float inDeltaMax)
	{
		// We're outside of the limits, get actual delta to min/max range
		// Note that a swing/twist of -1 and 1 represent the same angle, so if the difference is bigger than 1, the shortest angle is the other way around (2 - difference)
		// We should actually be working with angles rather than sin(angle / 2). When the difference is small the approximation is accurate, but
		// when working with extreme values the calculation is off and e.g. when the limit is between 0 and 180 a value of approx -60 will clamp
		// to 180 rather than 0 (you'd expect anything > -90 to go to 0).
		inDeltaMin = abs(inDeltaMin);
		if (inDeltaMin > 1.0f) inDeltaMin = 2.0f - inDeltaMin;
		inDeltaMax = abs(inDeltaMax);
		if (inDeltaMax > 1.0f) inDeltaMax = 2.0f - inDeltaMax;
		return inDeltaMin < inDeltaMax;
	}

	/// Clamp twist and swing against the constraint limits, returns which parts were clamped (everything assumed in constraint space)
	inline void					ClampSwingTwist(Quat &ioSwing, Quat &ioTwist, uint &outClampedAxis) const
	{
		// Start with not clamped
		outClampedAxis = 0;

		// Check that swing and twist quaternions don't contain rotations around the wrong axis
		JPH_ASSERT(ioSwing.GetX() == 0.0f);
		JPH_ASSERT(ioTwist.GetY() == 0.0f);
		JPH_ASSERT(ioTwist.GetZ() == 0.0f);

		// Ensure quaternions have w > 0
		bool negate_swing = ioSwing.GetW() < 0.0f;
		if (negate_swing)
			ioSwing = -ioSwing;
		bool negate_twist = ioTwist.GetW() < 0.0f;
		if (negate_twist)
			ioTwist = -ioTwist;

		if (mRotationFlags & TwistXLocked)
		{
			// Twist axis is locked, clamp whenever twist is not identity
			outClampedAxis |= ioTwist.GetX() != 0.0f? (cClampedTwistMin | cClampedTwistMax) : 0;
			ioTwist = Quat::sIdentity();
		}
		else if ((mRotationFlags & TwistXFree) == 0)
		{
			// Twist axis has limit, clamp whenever out of range
			float delta_min = mSinTwistHalfMinAngle - ioTwist.GetX();
			float delta_max = ioTwist.GetX() - mSinTwistHalfMaxAngle;
			if (delta_min > 0.0f || delta_max > 0.0f)
			{
				// Pick the twist that corresponds to the smallest delta
				if (sDistanceToMinShorter(delta_min, delta_max))
				{
					ioTwist = Quat(mSinTwistHalfMinAngle, 0, 0, mCosTwistHalfMinAngle);
					outClampedAxis |= cClampedTwistMin;
				}
				else
				{
					ioTwist = Quat(mSinTwistHalfMaxAngle, 0, 0, mCosTwistHalfMaxAngle);
					outClampedAxis |= cClampedTwistMax;
				}
			}
		}

		// Clamp swing
		if (mRotationFlags & SwingYLocked)
		{
			if (mRotationFlags & SwingZLocked)
			{
				// Both swing Y and Z are disabled, no degrees of freedom in swing
				outClampedAxis |= ioSwing.GetY() != 0.0f? (cClampedSwingYMin | cClampedSwingYMax) : 0;
				outClampedAxis |= ioSwing.GetZ() != 0.0f? (cClampedSwingZMin | cClampedSwingZMax) : 0;
				ioSwing = Quat::sIdentity();
			}
			else
			{
				// Swing Y angle disabled, only 1 degree of freedom in swing
				outClampedAxis |= ioSwing.GetY() != 0.0f? (cClampedSwingYMin | cClampedSwingYMax) : 0;
				float delta_min = mSinSwingZHalfMinAngle - ioSwing.GetZ();
				float delta_max = ioSwing.GetZ() - mSinSwingZHalfMaxAngle;
				if (delta_min > 0.0f || delta_max > 0.0f)
				{
					// Pick the swing that corresponds to the smallest delta
					if (sDistanceToMinShorter(delta_min, delta_max))
					{
						ioSwing = Quat(0, 0, mSinSwingZHalfMinAngle, mCosSwingZHalfMinAngle);
						outClampedAxis |= cClampedSwingZMin;
					}
					else
					{
						ioSwing = Quat(0, 0, mSinSwingZHalfMaxAngle, mCosSwingZHalfMaxAngle);
						outClampedAxis |= cClampedSwingZMax;
					}
				}
				else if ((outClampedAxis & cClampedSwingYMin) != 0)
				{
					float z = ioSwing.GetZ();
					ioSwing = Quat(0, 0, z, sqrt(1.0f - Square(z)));
				}
			}
		}
		else if (mRotationFlags & SwingZLocked)
		{
			// Swing Z angle disabled, only 1 degree of freedom in swing
			outClampedAxis |= ioSwing.GetZ() != 0.0f? (cClampedSwingZMin | cClampedSwingZMax) : 0;
			float delta_min = mSinSwingYHalfMinAngle - ioSwing.GetY();
			float delta_max = ioSwing.GetY() - mSinSwingYHalfMaxAngle;
			if (delta_min > 0.0f || delta_max > 0.0f)
			{
				// Pick the swing that corresponds to the smallest delta
				if (sDistanceToMinShorter(delta_min, delta_max))
				{
					ioSwing = Quat(0, mSinSwingYHalfMinAngle, 0, mCosSwingYHalfMinAngle);
					outClampedAxis |= cClampedSwingYMin;
				}
				else
				{
					ioSwing = Quat(0, mSinSwingYHalfMaxAngle, 0, mCosSwingYHalfMaxAngle);
					outClampedAxis |= cClampedSwingYMax;
				}
			}
			else if ((outClampedAxis & cClampedSwingZMin) != 0)
			{
				float y = ioSwing.GetY();
				ioSwing = Quat(0, y, 0, sqrt(1.0f - Square(y)));
			}
		}
		else
		{
			// Two degrees of freedom
			if (mSwingType == ESwingType::Cone)
			{
				// Use ellipse to solve limits
				Ellipse ellipse(mSinSwingYHalfMaxAngle, mSinSwingZHalfMaxAngle);
				Float2 point(ioSwing.GetY(), ioSwing.GetZ());
				if (!ellipse.IsInside(point))
				{
					Float2 closest = ellipse.GetClosestPoint(point);
					ioSwing = Quat(0, closest.x, closest.y, sqrt(max(0.0f, 1.0f - Square(closest.x) - Square(closest.y))));
					outClampedAxis |= cClampedSwingYMin | cClampedSwingYMax | cClampedSwingZMin | cClampedSwingZMax; // We're not using the flags on which side we got clamped here
				}
			}
			else
			{
				// Use pyramid to solve limits
				// The quaternion rotating by angle y around the Y axis then rotating by angle z around the Z axis is:
				// q = Quat::sRotation(Vec3::sAxisZ(), z) * Quat::sRotation(Vec3::sAxisY(), y)
				// [q.x, q.y, q.z, q.w] = [-sin(y / 2) * sin(z / 2), sin(y / 2) * cos(z / 2), cos(y / 2) * sin(z / 2), cos(y / 2) * cos(z / 2)]
				// So we can calculate y / 2 = atan2(q.y, q.w) and z / 2 = atan2(q.z, q.w)
				Vec4 half_angle = Vec4::sATan2(ioSwing.GetXYZW().Swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z>(), ioSwing.GetXYZW().SplatW());
				Vec4 min_half_angle(mSwingYHalfMinAngle, mSwingYHalfMinAngle, mSwingZHalfMinAngle, mSwingZHalfMinAngle);
				Vec4 max_half_angle(mSwingYHalfMaxAngle, mSwingYHalfMaxAngle, mSwingZHalfMaxAngle, mSwingZHalfMaxAngle);
				Vec4 clamped_half_angle = Vec4::sMin(Vec4::sMax(half_angle, min_half_angle), max_half_angle);
				UVec4 unclamped = Vec4::sEquals(half_angle, clamped_half_angle);
				if (!unclamped.TestAllTrue())
				{
					// We now calculate the quaternion again using the formula for q above,
					// but we leave out the x component in order to not introduce twist
					Vec4 s, c;
					clamped_half_angle.SinCos(s, c);
					ioSwing = Quat(0, s.GetY() * c.GetZ(), c.GetY() * s.GetZ(), c.GetY() * c.GetZ()).Normalized();
					outClampedAxis |= cClampedSwingYMin | cClampedSwingYMax | cClampedSwingZMin | cClampedSwingZMax; // We're not using the flags on which side we got clamped here
				}
			}
		}

		// Flip sign back
		if (negate_swing)
			ioSwing = -ioSwing;
		if (negate_twist)
			ioTwist = -ioTwist;

		JPH_ASSERT(ioSwing.IsNormalized());
		JPH_ASSERT(ioTwist.IsNormalized());
	}

	/// Calculate properties used during the functions below
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inConstraintRotation The current rotation of the constraint in constraint space
	/// @param inConstraintToWorld Rotates from constraint space into world space
	inline void					CalculateConstraintProperties(const Body &inBody1, const Body &inBody2, QuatArg inConstraintRotation, QuatArg inConstraintToWorld)
	{
		// Decompose into swing and twist
		Quat q_swing, q_twist;
		inConstraintRotation.GetSwingTwist(q_swing, q_twist);

		// Clamp against joint limits
		Quat q_clamped_swing = q_swing, q_clamped_twist = q_twist;
		uint clamped_axis;
		ClampSwingTwist(q_clamped_swing, q_clamped_twist, clamped_axis);

		if (mRotationFlags & SwingYLocked)
		{
			Quat twist_to_world = inConstraintToWorld * q_swing;
			mWorldSpaceSwingLimitYRotationAxis = twist_to_world.RotateAxisY();
			mWorldSpaceSwingLimitZRotationAxis = twist_to_world.RotateAxisZ();

			if (mRotationFlags & SwingZLocked)
			{
				// Swing fully locked
				mSwingLimitYConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceSwingLimitYRotationAxis);
				mSwingLimitZConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceSwingLimitZRotationAxis);
			}
			else
			{
				// Swing only locked around Y
				mSwingLimitYConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceSwingLimitYRotationAxis);
				if ((clamped_axis & (cClampedSwingZMin | cClampedSwingZMax)) != 0)
				{
					if ((clamped_axis & cClampedSwingZMin) != 0)
						mWorldSpaceSwingLimitZRotationAxis = -mWorldSpaceSwingLimitZRotationAxis; // Flip axis if hitting min limit because the impulse limit is going to be between [-FLT_MAX, 0]
					mSwingLimitZConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceSwingLimitZRotationAxis);
				}
				else
					mSwingLimitZConstraintPart.Deactivate();
			}
		}
		else if (mRotationFlags & SwingZLocked)
		{
			// Swing only locked around Z
			Quat twist_to_world = inConstraintToWorld * q_swing;
			mWorldSpaceSwingLimitYRotationAxis = twist_to_world.RotateAxisY();
			mWorldSpaceSwingLimitZRotationAxis = twist_to_world.RotateAxisZ();

			if ((clamped_axis & (cClampedSwingYMin | cClampedSwingYMax)) != 0)
			{
				if ((clamped_axis & cClampedSwingYMin) != 0)
					mWorldSpaceSwingLimitYRotationAxis = -mWorldSpaceSwingLimitYRotationAxis; // Flip axis if hitting min limit because the impulse limit is going to be between [-FLT_MAX, 0]
				mSwingLimitYConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceSwingLimitYRotationAxis);
			}
			else
				mSwingLimitYConstraintPart.Deactivate();
			mSwingLimitZConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceSwingLimitZRotationAxis);
		}
		else if ((mRotationFlags & SwingYZFree) != SwingYZFree)
		{
			// Swing has limits around Y and Z
			if ((clamped_axis & (cClampedSwingYMin | cClampedSwingYMax | cClampedSwingZMin | cClampedSwingZMax)) != 0)
			{
				// Calculate axis of rotation from clamped swing to swing
				Vec3 current = (inConstraintToWorld * q_swing).RotateAxisX();
				Vec3 desired = (inConstraintToWorld * q_clamped_swing).RotateAxisX();
				mWorldSpaceSwingLimitYRotationAxis = desired.Cross(current);
				float len = mWorldSpaceSwingLimitYRotationAxis.Length();
				if (len != 0.0f)
				{
					mWorldSpaceSwingLimitYRotationAxis /= len;
					mSwingLimitYConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceSwingLimitYRotationAxis);
				}
				else
					mSwingLimitYConstraintPart.Deactivate();
			}
			else
				mSwingLimitYConstraintPart.Deactivate();
			mSwingLimitZConstraintPart.Deactivate();
		}
		else
		{
			// No swing limits
			mSwingLimitYConstraintPart.Deactivate();
			mSwingLimitZConstraintPart.Deactivate();
		}

		if (mRotationFlags & TwistXLocked)
		{
			// Twist locked, always activate constraint
			mWorldSpaceTwistLimitRotationAxis = (inConstraintToWorld * q_swing).RotateAxisX();
			mTwistLimitConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceTwistLimitRotationAxis);
		}
		else if ((mRotationFlags & TwistXFree) == 0)
		{
			// Twist has limits
			if ((clamped_axis & (cClampedTwistMin | cClampedTwistMax)) != 0)
			{
				mWorldSpaceTwistLimitRotationAxis = (inConstraintToWorld * q_swing).RotateAxisX();
				if ((clamped_axis & cClampedTwistMin) != 0)
					mWorldSpaceTwistLimitRotationAxis = -mWorldSpaceTwistLimitRotationAxis; // Flip axis if hitting min limit because the impulse limit is going to be between [-FLT_MAX, 0]
				mTwistLimitConstraintPart.CalculateConstraintProperties(inBody1, inBody2, mWorldSpaceTwistLimitRotationAxis);
			}
			else
				mTwistLimitConstraintPart.Deactivate();
		}
		else
		{
			// No twist limits
			mTwistLimitConstraintPart.Deactivate();
		}
	}

	/// Deactivate this constraint
	void						Deactivate()
	{
		mSwingLimitYConstraintPart.Deactivate();
		mSwingLimitZConstraintPart.Deactivate();
		mTwistLimitConstraintPart.Deactivate();
	}

	/// Check if constraint is active
	inline bool					IsActive() const
	{
		return mSwingLimitYConstraintPart.IsActive() || mSwingLimitZConstraintPart.IsActive() || mTwistLimitConstraintPart.IsActive();
	}

	/// Must be called from the WarmStartVelocityConstraint call to apply the previous frame's impulses
	inline void					WarmStart(Body &ioBody1, Body &ioBody2, float inWarmStartImpulseRatio)
	{
		mSwingLimitYConstraintPart.WarmStart(ioBody1, ioBody2, inWarmStartImpulseRatio);
		mSwingLimitZConstraintPart.WarmStart(ioBody1, ioBody2, inWarmStartImpulseRatio);
		mTwistLimitConstraintPart.WarmStart(ioBody1, ioBody2, inWarmStartImpulseRatio);
	}

	/// Iteratively update the velocity constraint. Makes sure d/dt C(...) = 0, where C is the constraint equation.
	inline bool					SolveVelocityConstraint(Body &ioBody1, Body &ioBody2)
	{
		bool impulse = false;

		// Solve swing constraint
		if (mSwingLimitYConstraintPart.IsActive())
			impulse |= mSwingLimitYConstraintPart.SolveVelocityConstraint(ioBody1, ioBody2, mWorldSpaceSwingLimitYRotationAxis, -FLT_MAX, mSinSwingYHalfMinAngle == mSinSwingYHalfMaxAngle? FLT_MAX : 0.0f);

		if (mSwingLimitZConstraintPart.IsActive())
			impulse |= mSwingLimitZConstraintPart.SolveVelocityConstraint(ioBody1, ioBody2, mWorldSpaceSwingLimitZRotationAxis, -FLT_MAX, mSinSwingZHalfMinAngle == mSinSwingZHalfMaxAngle? FLT_MAX : 0.0f);

		// Solve twist constraint
		if (mTwistLimitConstraintPart.IsActive())
			impulse |= mTwistLimitConstraintPart.SolveVelocityConstraint(ioBody1, ioBody2, mWorldSpaceTwistLimitRotationAxis, -FLT_MAX, mSinTwistHalfMinAngle == mSinTwistHalfMaxAngle? FLT_MAX : 0.0f);

		return impulse;
	}

	/// Iteratively update the position constraint. Makes sure C(...) = 0.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inConstraintRotation The current rotation of the constraint in constraint space
	/// @param inConstraintToBody1 , inConstraintToBody2 Rotates from constraint space to body 1/2 space
	/// @param inBaumgarte Baumgarte constant (fraction of the error to correct)
	inline bool					SolvePositionConstraint(Body &ioBody1, Body &ioBody2, QuatArg inConstraintRotation, QuatArg inConstraintToBody1, QuatArg inConstraintToBody2, float inBaumgarte) const
	{
		Quat q_swing, q_twist;
		inConstraintRotation.GetSwingTwist(q_swing, q_twist);

		uint clamped_axis;
		ClampSwingTwist(q_swing, q_twist, clamped_axis);

		// Solve rotation violations
		if (clamped_axis != 0)
		{
			RotationEulerConstraintPart part;
			Quat inv_initial_orientation = inConstraintToBody2 * (inConstraintToBody1 * q_swing * q_twist).Conjugated();
			part.CalculateConstraintProperties(ioBody1, Mat44::sRotation(ioBody1.GetRotation()), ioBody2, Mat44::sRotation(ioBody2.GetRotation()));
			return part.SolvePositionConstraint(ioBody1, ioBody2, inv_initial_orientation, inBaumgarte);
		}

		return false;
	}

	/// Return lagrange multiplier for swing
	inline float				GetTotalSwingYLambda() const
	{
		return mSwingLimitYConstraintPart.GetTotalLambda();
	}

	inline float				GetTotalSwingZLambda() const
	{
		return mSwingLimitZConstraintPart.GetTotalLambda();
	}

	/// Return lagrange multiplier for twist
	inline float				GetTotalTwistLambda() const
	{
		return mTwistLimitConstraintPart.GetTotalLambda();
	}

	/// Save state of this constraint part
	void						SaveState(StateRecorder &inStream) const
	{
		mSwingLimitYConstraintPart.SaveState(inStream);
		mSwingLimitZConstraintPart.SaveState(inStream);
		mTwistLimitConstraintPart.SaveState(inStream);
	}

	/// Restore state of this constraint part
	void						RestoreState(StateRecorder &inStream)
	{
		mSwingLimitYConstraintPart.RestoreState(inStream);
		mSwingLimitZConstraintPart.RestoreState(inStream);
		mTwistLimitConstraintPart.RestoreState(inStream);
	}

private:
	// CONFIGURATION PROPERTIES FOLLOW

	enum ERotationFlags
	{
		/// Indicates that axis is completely locked (cannot rotate around this axis)
		TwistXLocked			= 1 << 0,
		SwingYLocked			= 1 << 1,
		SwingZLocked			= 1 << 2,

		/// Indicates that axis is completely free (can rotate around without limits)
		TwistXFree				= 1 << 3,
		SwingYFree				= 1 << 4,
		SwingZFree				= 1 << 5,
		SwingYZFree				= SwingYFree | SwingZFree
	};

	uint8						mRotationFlags;

	// Constants
	ESwingType					mSwingType = ESwingType::Cone;
	float						mSinTwistHalfMinAngle;
	float						mSinTwistHalfMaxAngle;
	float						mCosTwistHalfMinAngle;
	float						mCosTwistHalfMaxAngle;
	float						mSwingYHalfMinAngle;
	float						mSwingYHalfMaxAngle;
	float						mSwingZHalfMinAngle;
	float						mSwingZHalfMaxAngle;
	float						mSinSwingYHalfMinAngle;
	float						mSinSwingYHalfMaxAngle;
	float						mSinSwingZHalfMinAngle;
	float						mSinSwingZHalfMaxAngle;
	float						mCosSwingYHalfMinAngle;
	float						mCosSwingYHalfMaxAngle;
	float						mCosSwingZHalfMinAngle;
	float						mCosSwingZHalfMaxAngle;

	// RUN TIME PROPERTIES FOLLOW

	/// Rotation axis for the angle constraint parts
	Vec3						mWorldSpaceSwingLimitYRotationAxis;
	Vec3						mWorldSpaceSwingLimitZRotationAxis;
	Vec3						mWorldSpaceTwistLimitRotationAxis;

	/// The constraint parts
	AngleConstraintPart			mSwingLimitYConstraintPart;
	AngleConstraintPart			mSwingLimitZConstraintPart;
	AngleConstraintPart			mTwistLimitConstraintPart;
};

JPH_NAMESPACE_END
