// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

/// Quaternion based constraint that constrains rotation around all axis so that only translation is allowed.
///
/// NOTE: This constraint part is more expensive than the RotationEulerConstraintPart and slightly more correct since
/// RotationEulerConstraintPart::SolvePositionConstraint contains an approximation. In practice the difference
/// is small, so the RotationEulerConstraintPart is probably the better choice.
///
/// Rotation is fixed between bodies like this:
///
/// q2 = q1 r0
///
/// Where:
/// q1, q2 = world space quaternions representing rotation of body 1 and 2.
/// r0 = initial rotation between bodies in local space of body 1, this can be calculated by:
///
/// q20 = q10 r0
/// <=> r0 = q10^* q20
///
/// Where:
/// q10, q20 = initial world space rotations of body 1 and 2.
/// q10^* = conjugate of quaternion q10 (which is the same as the inverse for a unit quaternion)
///
/// We exclusively use the conjugate below:
///
/// r0^* = q20^* q10
///
/// The error in the rotation is (in local space of body 1):
///
/// q2 = q1 error r0
/// <=> error = q1^* q2 r0^*
///
/// The imaginary part of the quaternion represents the rotation axis * sin(angle / 2). The real part of the quaternion
/// does not add any additional information (we know the quaternion in normalized) and we're removing 3 degrees of freedom
/// so we want 3 parameters. Therefore we define the constraint equation like:
///
/// C = A q1^* q2 r0^* = 0
///
/// Where (if you write a quaternion as [real-part, i-part, j-part, k-part]):
///
///		    [0, 1, 0, 0]
///		A = [0, 0, 1, 0]
///		    [0, 0, 0, 1]
///
/// or in our case since we store a quaternion like [i-part, j-part, k-part, real-part]:
///
///		    [1, 0, 0, 0]
///		A = [0, 1, 0, 0]
///		    [0, 0, 1, 0]
///
/// Time derivative:
///
/// d/dt C = A (q1^* d/dt(q2) + d/dt(q1^*) q2) r0^*
/// = A (q1^* (1/2 W2 q2) + (1/2 W1 q1)^* q2) r0^*
/// = 1/2 A (q1^* W2 q2 + q1^* W1^* q2) r0^*
/// = 1/2 A (q1^* W2 q2 - q1^* W1 * q2) r0^*
/// = 1/2 A ML(q1^*) MR(q2 r0^*) (W2 - W1)
/// = 1/2 A ML(q1^*) MR(q2 r0^*) A^T (w2 - w1)
///
/// Where:
/// W1 = [0, w1], W2 = [0, w2] (converting angular velocity to imaginary part of quaternion).
/// w1, w2 = angular velocity of body 1 and 2.
/// d/dt(q) = 1/2 W q (time derivative of a quaternion).
/// W^* = -W (conjugate negates angular velocity as quaternion).
/// ML(q): 4x4 matrix so that q * p = ML(q) * p, where q and p are quaternions.
/// MR(p): 4x4 matrix so that q * p = MR(p) * q, where q and p are quaternions.
/// A^T: Transpose of A.
///
/// Jacobian:
///
/// J = [0, -1/2 A ML(q1^*) MR(q2 r0^*) A^T, 0, 1/2 A ML(q1^*) MR(q2 r0^*) A^T]
/// = [0, -JP, 0, JP]
///
/// Suggested reading:
/// - 3D Constraint Derivations for Impulse Solvers - Marijn Tamis
/// - Game Physics Pearls - Section 9 - Quaternion Based Constraints - Claude Lacoursiere
class RotationQuatConstraintPart
{
private:
	/// Internal helper function to update velocities of bodies after Lagrange multiplier is calculated
	JPH_INLINE bool				ApplyVelocityStep(Body &ioBody1, Body &ioBody2, Vec3Arg inLambda) const
	{
		// Apply impulse if delta is not zero
		if (inLambda != Vec3::sZero())
		{
			// Calculate velocity change due to constraint
			//
			// Impulse:
			// P = J^T lambda
			//
			// Euler velocity integration:
			// v' = v + M^-1 P
			if (ioBody1.IsDynamic())
				ioBody1.GetMotionProperties()->SubAngularVelocityStep(mInvI1_JPT.Multiply3x3(inLambda));
			if (ioBody2.IsDynamic())
				ioBody2.GetMotionProperties()->AddAngularVelocityStep(mInvI2_JPT.Multiply3x3(inLambda));
			return true;
		}

		return false;
	}

public:
	/// Return inverse of initial rotation from body 1 to body 2 in body 1 space
	static Quat					sGetInvInitialOrientation(const Body &inBody1, const Body &inBody2)
	{
		// q20 = q10 r0
		// <=> r0 = q10^-1 q20
		// <=> r0^-1 = q20^-1 q10
		//
		// where:
		//
		// q20 = initial orientation of body 2
		// q10 = initial orientation of body 1
		// r0 = initial rotation from body 1 to body 2
		return inBody2.GetRotation().Conjugated() * inBody1.GetRotation();
	}

	/// Calculate properties used during the functions below
	inline void					CalculateConstraintProperties(const Body &inBody1, Mat44Arg inRotation1, const Body &inBody2, Mat44Arg inRotation2, QuatArg inInvInitialOrientation)
	{
		// Calculate: JP = 1/2 A ML(q1^*) MR(q2 r0^*) A^T
		Mat44 jp = (Mat44::sQuatLeftMultiply(0.5f * inBody1.GetRotation().Conjugated()) * Mat44::sQuatRightMultiply(inBody2.GetRotation() * inInvInitialOrientation)).GetRotationSafe();

		// Calculate properties used during constraint solving
		Mat44 inv_i1 = inBody1.IsDynamic()? inBody1.GetMotionProperties()->GetInverseInertiaForRotation(inRotation1) : Mat44::sZero();
		Mat44 inv_i2 = inBody2.IsDynamic()? inBody2.GetMotionProperties()->GetInverseInertiaForRotation(inRotation2) : Mat44::sZero();
		mInvI1_JPT = inv_i1.Multiply3x3RightTransposed(jp);
		mInvI2_JPT = inv_i2.Multiply3x3RightTransposed(jp);

		// Calculate effective mass: K^-1 = (J M^-1 J^T)^-1
		// = (JP * I1^-1 * JP^T + JP * I2^-1 * JP^T)^-1
		// = (JP * (I1^-1 + I2^-1) * JP^T)^-1
		if (!mEffectiveMass.SetInversed3x3(jp.Multiply3x3(inv_i1 + inv_i2).Multiply3x3RightTransposed(jp)))
			Deactivate();
		else
			mEffectiveMass_JP = mEffectiveMass.Multiply3x3(jp);
	}

	/// Deactivate this constraint
	inline void					Deactivate()
	{
		mEffectiveMass = Mat44::sZero();
		mEffectiveMass_JP = Mat44::sZero();
		mTotalLambda = Vec3::sZero();
	}

	/// Check if constraint is active
	inline bool					IsActive() const
	{
		return mEffectiveMass(3, 3) != 0.0f;
	}

	/// Must be called from the WarmStartVelocityConstraint call to apply the previous frame's impulses
	inline void					WarmStart(Body &ioBody1, Body &ioBody2, float inWarmStartImpulseRatio)
	{
		mTotalLambda *= inWarmStartImpulseRatio;
		ApplyVelocityStep(ioBody1, ioBody2, mTotalLambda);
	}

	/// Iteratively update the velocity constraint. Makes sure d/dt C(...) = 0, where C is the constraint equation.
	inline bool					SolveVelocityConstraint(Body &ioBody1, Body &ioBody2)
	{
		// Calculate lagrange multiplier:
		//
		// lambda = -K^-1 (J v + b)
		Vec3 lambda = mEffectiveMass_JP.Multiply3x3(ioBody1.GetAngularVelocity() - ioBody2.GetAngularVelocity());
		mTotalLambda += lambda;
		return ApplyVelocityStep(ioBody1, ioBody2, lambda);
	}

	/// Iteratively update the position constraint. Makes sure C(...) = 0.
	inline bool					SolvePositionConstraint(Body &ioBody1, Body &ioBody2, QuatArg inInvInitialOrientation, float inBaumgarte) const
	{
		// Calculate constraint equation
		Vec3 c = (ioBody1.GetRotation().Conjugated() * ioBody2.GetRotation() * inInvInitialOrientation).GetXYZ();
		if (c != Vec3::sZero())
		{
			// Calculate lagrange multiplier (lambda) for Baumgarte stabilization:
			//
			// lambda = -K^-1 * beta / dt * C
			//
			// We should divide by inDeltaTime, but we should multiply by inDeltaTime in the Euler step below so they're cancelled out
			Vec3 lambda = -inBaumgarte * mEffectiveMass * c;

			// Directly integrate velocity change for one time step
			//
			// Euler velocity integration:
			// dv = M^-1 P
			//
			// Impulse:
			// P = J^T lambda
			//
			// Euler position integration:
			// x' = x + dv * dt
			//
			// Note we don't accumulate velocities for the stabilization. This is using the approach described in 'Modeling and
			// Solving Constraints' by Erin Catto presented at GDC 2007. On slide 78 it is suggested to split up the Baumgarte
			// stabilization for positional drift so that it does not actually add to the momentum. We combine an Euler velocity
			// integrate + a position integrate and then discard the velocity change.
			if (ioBody1.IsDynamic())
				ioBody1.SubRotationStep(mInvI1_JPT.Multiply3x3(lambda));
			if (ioBody2.IsDynamic())
				ioBody2.AddRotationStep(mInvI2_JPT.Multiply3x3(lambda));
			return true;
		}

		return false;
	}

	/// Return lagrange multiplier
	Vec3						GetTotalLambda() const
	{
		return mTotalLambda;
	}

	/// Save state of this constraint part
	void						SaveState(StateRecorder &inStream) const
	{
		inStream.Write(mTotalLambda);
	}

	/// Restore state of this constraint part
	void						RestoreState(StateRecorder &inStream)
	{
		inStream.Read(mTotalLambda);
	}

private:
	Mat44						mInvI1_JPT;
	Mat44						mInvI2_JPT;
	Mat44						mEffectiveMass;
	Mat44						mEffectiveMass_JP;
	Vec3						mTotalLambda { Vec3::sZero() };
};

JPH_NAMESPACE_END
