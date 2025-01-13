// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

/// Constraint that constrains a rotation to a translation
///
/// Constraint equation:
///
/// C = Theta(t) - r d(t)
///
/// Derivative:
///
/// d/dt C = 0
/// <=> w1 . a - r v2 . b = 0
///
/// Jacobian:
///
/// \f[J = \begin{bmatrix}0 & a^T & -r b^T & 0\end{bmatrix}\f]
///
/// Used terms (here and below, everything in world space):\n
/// a = axis around which body 1 rotates (normalized).\n
/// b = axis along which body 2 slides (normalized).\n
/// Theta(t) = rotation around a of body 1.\n
/// d(t) = distance body 2 slides.\n
/// r = ratio between rotation and translation.\n
/// v = [v1, w1, v2, w2].\n
/// v1, v2 = linear velocity of body 1 and 2.\n
/// w1, w2 = angular velocity of body 1 and 2.\n
/// M = mass matrix, a diagonal matrix of the mass and inertia with diagonal [m1, I1, m2, I2].\n
/// \f$K^{-1} = \left( J M^{-1} J^T \right)^{-1}\f$ = effective mass.\n
/// \f$\beta\f$ = baumgarte constant.
class RackAndPinionConstraintPart
{
	/// Internal helper function to update velocities of bodies after Lagrange multiplier is calculated
	JPH_INLINE bool				ApplyVelocityStep(Body &ioBody1, Body &ioBody2, float inLambda) const
	{
		// Apply impulse if delta is not zero
		if (inLambda != 0.0f)
		{
			// Calculate velocity change due to constraint
			//
			// Impulse:
			// P = J^T lambda
			//
			// Euler velocity integration:
			// v' = v + M^-1 P
			ioBody1.GetMotionProperties()->AddAngularVelocityStep(inLambda * mInvI1_A);
			ioBody2.GetMotionProperties()->SubLinearVelocityStep(inLambda * mRatio_InvM2_B);
			return true;
		}

		return false;
	}

public:
	/// Calculate properties used during the functions below
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inWorldSpaceHingeAxis The axis around which body 1 rotates
	/// @param inWorldSpaceSliderAxis The axis along which body 2 slides
	/// @param inRatio The ratio between rotation and translation
	inline void					CalculateConstraintProperties(const Body &inBody1, Vec3Arg inWorldSpaceHingeAxis, const Body &inBody2, Vec3Arg inWorldSpaceSliderAxis, float inRatio)
	{
		JPH_ASSERT(inWorldSpaceHingeAxis.IsNormalized(1.0e-4f));
		JPH_ASSERT(inWorldSpaceSliderAxis.IsNormalized(1.0e-4f));

		// Calculate: I1^-1 a
		mInvI1_A = inBody1.GetMotionProperties()->MultiplyWorldSpaceInverseInertiaByVector(inBody1.GetRotation(), inWorldSpaceHingeAxis);

		// Calculate: r/m2 b
		float inv_m2 = inBody2.GetMotionProperties()->GetInverseMass();
		mRatio_InvM2_B = inRatio * inv_m2 * inWorldSpaceSliderAxis;

		// K^-1 = 1 / (J M^-1 J^T) = 1 / (a^T I1^-1 a + 1/m2 * r^2 * b . b)
		float inv_effective_mass = (inWorldSpaceHingeAxis.Dot(mInvI1_A) + inv_m2 * Square(inRatio));
		if (inv_effective_mass == 0.0f)
			Deactivate();
		else
			mEffectiveMass = 1.0f / inv_effective_mass;
	}

	/// Deactivate this constraint
	inline void					Deactivate()
	{
		mEffectiveMass = 0.0f;
		mTotalLambda = 0.0f;
	}

	/// Check if constraint is active
	inline bool					IsActive() const
	{
		return mEffectiveMass != 0.0f;
	}

	/// Must be called from the WarmStartVelocityConstraint call to apply the previous frame's impulses
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inWarmStartImpulseRatio Ratio of new step to old time step (dt_new / dt_old) for scaling the lagrange multiplier of the previous frame
	inline void					WarmStart(Body &ioBody1, Body &ioBody2, float inWarmStartImpulseRatio)
	{
		mTotalLambda *= inWarmStartImpulseRatio;
		ApplyVelocityStep(ioBody1, ioBody2, mTotalLambda);
	}

	/// Iteratively update the velocity constraint. Makes sure d/dt C(...) = 0, where C is the constraint equation.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inWorldSpaceHingeAxis The axis around which body 1 rotates
	/// @param inWorldSpaceSliderAxis The axis along which body 2 slides
	/// @param inRatio The ratio between rotation and translation
	inline bool					SolveVelocityConstraint(Body &ioBody1, Vec3Arg inWorldSpaceHingeAxis, Body &ioBody2, Vec3Arg inWorldSpaceSliderAxis, float inRatio)
	{
		// Lagrange multiplier is:
		//
		// lambda = -K^-1 (J v + b)
		float lambda = mEffectiveMass * (inRatio * inWorldSpaceSliderAxis.Dot(ioBody2.GetLinearVelocity()) - inWorldSpaceHingeAxis.Dot(ioBody1.GetAngularVelocity()));
		mTotalLambda += lambda; // Store accumulated impulse

		return ApplyVelocityStep(ioBody1, ioBody2, lambda);
	}

	/// Return lagrange multiplier
	float						GetTotalLambda() const
	{
		return mTotalLambda;
	}

	/// Iteratively update the position constraint. Makes sure C(...) == 0.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inC Value of the constraint equation (C)
	/// @param inBaumgarte Baumgarte constant (fraction of the error to correct)
	inline bool					SolvePositionConstraint(Body &ioBody1, Body &ioBody2, float inC, float inBaumgarte) const
	{
		// Only apply position constraint when the constraint is hard, otherwise the velocity bias will fix the constraint
		if (inC != 0.0f)
		{
			// Calculate lagrange multiplier (lambda) for Baumgarte stabilization:
			//
			// lambda = -K^-1 * beta / dt * C
			//
			// We should divide by inDeltaTime, but we should multiply by inDeltaTime in the Euler step below so they're cancelled out
			float lambda = -mEffectiveMass * inBaumgarte * inC;

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
				ioBody1.AddRotationStep(lambda * mInvI1_A);
			if (ioBody2.IsDynamic())
				ioBody2.SubPositionStep(lambda * mRatio_InvM2_B);
			return true;
		}

		return false;
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
	Vec3						mInvI1_A;
	Vec3						mRatio_InvM2_B;
	float						mEffectiveMass = 0.0f;
	float						mTotalLambda = 0.0f;
};

JPH_NAMESPACE_END
