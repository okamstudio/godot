// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

/// Constraint part to an AxisConstraintPart but both bodies have an independent axis on which the force is applied.
///
/// Constraint equation:
///
/// \f[C = (x_1 + r_1 - f_1) . n_1 + r (x_2 + r_2 - f_2) \cdot n_2\f]
///
/// Calculating the Jacobian:
///
/// \f[dC/dt = (v_1 + w_1 \times r_1) \cdot n_1 + (x_1 + r_1 - f_1) \cdot d n_1/dt + r (v_2 + w_2 \times r_2) \cdot n_2 + r (x_2 + r_2 - f_2) \cdot d n_2/dt\f]
///
/// Assuming that d n1/dt and d n2/dt are small this becomes:
///
/// \f[(v_1 + w_1 \times r_1) \cdot n_1 + r (v_2 + w_2 \times r_2) \cdot n_2\f]
/// \f[= v_1 \cdot n_1 + r_1 \times n_1 \cdot w_1 + r v_2 \cdot n_2 + r r_2 \times n_2 \cdot w_2\f]
///
/// Jacobian:
///
/// \f[J = \begin{bmatrix}n_1 & r_1 \times n_1 & r n_2 & r r_2 \times n_2\end{bmatrix}\f]
///
/// Effective mass:
///
/// \f[K = m_1^{-1} + r_1 \times n_1 I_1^{-1} r_1 \times n_1 + r^2 m_2^{-1} + r^2 r_2 \times n_2 I_2^{-1} r_2 \times n_2\f]
///
/// Used terms (here and below, everything in world space):\n
/// n1 = (x1 + r1 - f1) / |x1 + r1 - f1|, axis along which the force is applied for body 1\n
/// n2 = (x2 + r2 - f2) / |x2 + r2 - f2|, axis along which the force is applied for body 2\n
/// r = ratio how forces are applied between bodies.\n
/// x1, x2 = center of mass for the bodies.\n
/// v = [v1, w1, v2, w2].\n
/// v1, v2 = linear velocity of body 1 and 2.\n
/// w1, w2 = angular velocity of body 1 and 2.\n
/// M = mass matrix, a diagonal matrix of the mass and inertia with diagonal [m1, I1, m2, I2].\n
/// \f$K^{-1} = \left( J M^{-1} J^T \right)^{-1}\f$ = effective mass.\n
/// b = velocity bias.\n
/// \f$\beta\f$ = baumgarte constant.
class IndependentAxisConstraintPart
{
	/// Internal helper function to update velocities of bodies after Lagrange multiplier is calculated
	JPH_INLINE bool				ApplyVelocityStep(Body &ioBody1, Body &ioBody2, Vec3Arg inN1, Vec3Arg inN2, float inRatio, float inLambda) const
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
			if (ioBody1.IsDynamic())
			{
				MotionProperties *mp1 = ioBody1.GetMotionProperties();
				mp1->AddLinearVelocityStep((mp1->GetInverseMass() * inLambda) * inN1);
				mp1->AddAngularVelocityStep(mInvI1_R1xN1 * inLambda);
			}
			if (ioBody2.IsDynamic())
			{
				MotionProperties *mp2 = ioBody2.GetMotionProperties();
				mp2->AddLinearVelocityStep((inRatio * mp2->GetInverseMass() * inLambda) * inN2);
				mp2->AddAngularVelocityStep(mInvI2_RatioR2xN2 * inLambda);
			}
			return true;
		}

		return false;
	}

public:
	/// Calculate properties used during the functions below
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inR1 The position on which the constraint operates on body 1 relative to COM
	/// @param inN1 The world space normal in which the constraint operates for body 1
	/// @param inR2 The position on which the constraint operates on body 1 relative to COM
	/// @param inN2 The world space normal in which the constraint operates for body 2
	/// @param inRatio The ratio how forces are applied between bodies
	inline void					CalculateConstraintProperties(const Body &inBody1, const Body &inBody2, Vec3Arg inR1, Vec3Arg inN1, Vec3Arg inR2, Vec3Arg inN2, float inRatio)
	{
		JPH_ASSERT(inN1.IsNormalized(1.0e-4f) && inN2.IsNormalized(1.0e-4f));

		float inv_effective_mass = 0.0f;

		if (!inBody1.IsStatic())
		{
			const MotionProperties *mp1 = inBody1.GetMotionProperties();

			mR1xN1 = inR1.Cross(inN1);
			mInvI1_R1xN1 = mp1->MultiplyWorldSpaceInverseInertiaByVector(inBody1.GetRotation(), mR1xN1);

			inv_effective_mass += mp1->GetInverseMass() + mInvI1_R1xN1.Dot(mR1xN1);
		}

		if (!inBody2.IsStatic())
		{
			const MotionProperties *mp2 = inBody2.GetMotionProperties();

			mRatioR2xN2 = inRatio * inR2.Cross(inN2);
			mInvI2_RatioR2xN2 = mp2->MultiplyWorldSpaceInverseInertiaByVector(inBody2.GetRotation(), mRatioR2xN2);

			inv_effective_mass += Square(inRatio) * mp2->GetInverseMass() + mInvI2_RatioR2xN2.Dot(mRatioR2xN2);
		}

		// Calculate inverse effective mass: K = J M^-1 J^T
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
	/// @param inN1 The world space normal in which the constraint operates for body 1
	/// @param inN2 The world space normal in which the constraint operates for body 2
	/// @param inRatio The ratio how forces are applied between bodies
	/// @param inWarmStartImpulseRatio Ratio of new step to old time step (dt_new / dt_old) for scaling the lagrange multiplier of the previous frame
	inline void					WarmStart(Body &ioBody1, Body &ioBody2, Vec3Arg inN1, Vec3Arg inN2, float inRatio, float inWarmStartImpulseRatio)
	{
		mTotalLambda *= inWarmStartImpulseRatio;
		ApplyVelocityStep(ioBody1, ioBody2, inN1, inN2, inRatio, mTotalLambda);
	}

	/// Iteratively update the velocity constraint. Makes sure d/dt C(...) = 0, where C is the constraint equation.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inN1 The world space normal in which the constraint operates for body 1
	/// @param inN2 The world space normal in which the constraint operates for body 2
	/// @param inRatio The ratio how forces are applied between bodies
	/// @param inMinLambda Minimum angular impulse to apply (N m s)
	/// @param inMaxLambda Maximum angular impulse to apply (N m s)
	inline bool					SolveVelocityConstraint(Body &ioBody1, Body &ioBody2, Vec3Arg inN1, Vec3Arg inN2, float inRatio, float inMinLambda, float inMaxLambda)
	{
		// Lagrange multiplier is:
		//
		// lambda = -K^-1 (J v + b)
		float lambda = -mEffectiveMass * (inN1.Dot(ioBody1.GetLinearVelocity()) + mR1xN1.Dot(ioBody1.GetAngularVelocity()) + inRatio * inN2.Dot(ioBody2.GetLinearVelocity()) + mRatioR2xN2.Dot(ioBody2.GetAngularVelocity()));
		float new_lambda = Clamp(mTotalLambda + lambda, inMinLambda, inMaxLambda); // Clamp impulse
		lambda = new_lambda - mTotalLambda; // Lambda potentially got clamped, calculate the new impulse to apply
		mTotalLambda = new_lambda; // Store accumulated impulse

		return ApplyVelocityStep(ioBody1, ioBody2, inN1, inN2, inRatio, lambda);
	}

	/// Return lagrange multiplier
	float						GetTotalLambda() const
	{
		return mTotalLambda;
	}

	/// Iteratively update the position constraint. Makes sure C(...) == 0.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inN1 The world space normal in which the constraint operates for body 1
	/// @param inN2 The world space normal in which the constraint operates for body 2
	/// @param inRatio The ratio how forces are applied between bodies
	/// @param inC Value of the constraint equation (C)
	/// @param inBaumgarte Baumgarte constant (fraction of the error to correct)
	inline bool					SolvePositionConstraint(Body &ioBody1, Body &ioBody2, Vec3Arg inN1, Vec3Arg inN2, float inRatio, float inC, float inBaumgarte) const
	{
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
			{
				ioBody1.AddPositionStep((lambda * ioBody1.GetMotionPropertiesUnchecked()->GetInverseMass()) * inN1);
				ioBody1.AddRotationStep(lambda * mInvI1_R1xN1);
			}
			if (ioBody2.IsDynamic())
			{
				ioBody2.AddPositionStep((lambda * inRatio * ioBody2.GetMotionPropertiesUnchecked()->GetInverseMass()) * inN2);
				ioBody2.AddRotationStep(lambda * mInvI2_RatioR2xN2);
			}
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
	Vec3						mR1xN1;
	Vec3						mInvI1_R1xN1;
	Vec3						mRatioR2xN2;
	Vec3						mInvI2_RatioR2xN2;
	float						mEffectiveMass = 0.0f;
	float						mTotalLambda = 0.0f;
};

JPH_NAMESPACE_END
