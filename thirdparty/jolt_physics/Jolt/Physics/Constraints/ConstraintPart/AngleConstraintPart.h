// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Constraints/ConstraintPart/SpringPart.h>
#include <Jolt/Physics/Constraints/SpringSettings.h>
#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

/// Constraint that constrains rotation along 1 axis
///
/// Based on: "Constraints Derivation for Rigid Body Simulation in 3D" - Daniel Chappuis, see section 2.4.5
///
/// Constraint equation (eq 108):
///
/// \f[C = \theta(t) - \theta_{min}\f]
///
/// Jacobian (eq 109):
///
/// \f[J = \begin{bmatrix}0 & -a^T & 0 & a^T\end{bmatrix}\f]
///
/// Used terms (here and below, everything in world space):\n
/// a = axis around which rotation is constrained (normalized).\n
/// x1, x2 = center of mass for the bodies.\n
/// v = [v1, w1, v2, w2].\n
/// v1, v2 = linear velocity of body 1 and 2.\n
/// w1, w2 = angular velocity of body 1 and 2.\n
/// M = mass matrix, a diagonal matrix of the mass and inertia with diagonal [m1, I1, m2, I2].\n
/// \f$K^{-1} = \left( J M^{-1} J^T \right)^{-1}\f$ = effective mass.\n
/// b = velocity bias.\n
/// \f$\beta\f$ = baumgarte constant.
class AngleConstraintPart
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
			if (ioBody1.IsDynamic())
				ioBody1.GetMotionProperties()->SubAngularVelocityStep(inLambda * mInvI1_Axis);
			if (ioBody2.IsDynamic())
				ioBody2.GetMotionProperties()->AddAngularVelocityStep(inLambda * mInvI2_Axis);
			return true;
		}

		return false;
	}

	/// Internal helper function to calculate the inverse effective mass
	JPH_INLINE float				CalculateInverseEffectiveMass(const Body &inBody1, const Body &inBody2, Vec3Arg inWorldSpaceAxis)
	{
		JPH_ASSERT(inWorldSpaceAxis.IsNormalized(1.0e-4f));

		// Calculate properties used below
		mInvI1_Axis = inBody1.IsDynamic()? inBody1.GetMotionProperties()->MultiplyWorldSpaceInverseInertiaByVector(inBody1.GetRotation(), inWorldSpaceAxis) : Vec3::sZero();
		mInvI2_Axis = inBody2.IsDynamic()? inBody2.GetMotionProperties()->MultiplyWorldSpaceInverseInertiaByVector(inBody2.GetRotation(), inWorldSpaceAxis) : Vec3::sZero();

		// Calculate inverse effective mass: K = J M^-1 J^T
		return inWorldSpaceAxis.Dot(mInvI1_Axis + mInvI2_Axis);
	}

public:
	/// Calculate properties used during the functions below
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inWorldSpaceAxis The axis of rotation along which the constraint acts (normalized)
	/// Set the following terms to zero if you don't want to drive the constraint to zero with a spring:
	/// @param inBias Bias term (b) for the constraint impulse: lambda = J v + b
	inline void					CalculateConstraintProperties(const Body &inBody1, const Body &inBody2, Vec3Arg inWorldSpaceAxis, float inBias = 0.0f)
	{
		float inv_effective_mass = CalculateInverseEffectiveMass(inBody1, inBody2, inWorldSpaceAxis);

		if (inv_effective_mass == 0.0f)
			Deactivate();
		else
		{
			mEffectiveMass = 1.0f / inv_effective_mass;
			mSpringPart.CalculateSpringPropertiesWithBias(inBias);
		}
	}

	/// Calculate properties used during the functions below
	/// @param inDeltaTime Time step
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inWorldSpaceAxis The axis of rotation along which the constraint acts (normalized)
	/// Set the following terms to zero if you don't want to drive the constraint to zero with a spring:
	/// @param inBias Bias term (b) for the constraint impulse: lambda = J v + b
	///	@param inC Value of the constraint equation (C)
	///	@param inFrequency Oscillation frequency (Hz)
	///	@param inDamping Damping factor (0 = no damping, 1 = critical damping)
	inline void					CalculateConstraintPropertiesWithFrequencyAndDamping(float inDeltaTime, const Body &inBody1, const Body &inBody2, Vec3Arg inWorldSpaceAxis, float inBias, float inC, float inFrequency, float inDamping)
	{
		float inv_effective_mass = CalculateInverseEffectiveMass(inBody1, inBody2, inWorldSpaceAxis);

		if (inv_effective_mass == 0.0f)
			Deactivate();
		else
			mSpringPart.CalculateSpringPropertiesWithFrequencyAndDamping(inDeltaTime, inv_effective_mass, inBias, inC, inFrequency, inDamping, mEffectiveMass);
	}

	/// Calculate properties used during the functions below
	/// @param inDeltaTime Time step
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inWorldSpaceAxis The axis of rotation along which the constraint acts (normalized)
	/// Set the following terms to zero if you don't want to drive the constraint to zero with a spring:
	/// @param inBias Bias term (b) for the constraint impulse: lambda = J v + b
	///	@param inC Value of the constraint equation (C)
	///	@param inStiffness Spring stiffness k.
	///	@param inDamping Spring damping coefficient c.
	inline void					CalculateConstraintPropertiesWithStiffnessAndDamping(float inDeltaTime, const Body &inBody1, const Body &inBody2, Vec3Arg inWorldSpaceAxis, float inBias, float inC, float inStiffness, float inDamping)
	{
		float inv_effective_mass = CalculateInverseEffectiveMass(inBody1, inBody2, inWorldSpaceAxis);

		if (inv_effective_mass == 0.0f)
			Deactivate();
		else
			mSpringPart.CalculateSpringPropertiesWithStiffnessAndDamping(inDeltaTime, inv_effective_mass, inBias, inC, inStiffness, inDamping, mEffectiveMass);
	}

	/// Selects one of the above functions based on the spring settings
	inline void					CalculateConstraintPropertiesWithSettings(float inDeltaTime, const Body &inBody1, const Body &inBody2, Vec3Arg inWorldSpaceAxis, float inBias, float inC, const SpringSettings &inSpringSettings)
	{
		float inv_effective_mass = CalculateInverseEffectiveMass(inBody1, inBody2, inWorldSpaceAxis);

		if (inv_effective_mass == 0.0f)
			Deactivate();
		else if (inSpringSettings.mMode == ESpringMode::FrequencyAndDamping)
			mSpringPart.CalculateSpringPropertiesWithFrequencyAndDamping(inDeltaTime, inv_effective_mass, inBias, inC, inSpringSettings.mFrequency, inSpringSettings.mDamping, mEffectiveMass);
		else
			mSpringPart.CalculateSpringPropertiesWithStiffnessAndDamping(inDeltaTime, inv_effective_mass, inBias, inC, inSpringSettings.mStiffness, inSpringSettings.mDamping, mEffectiveMass);
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
	/// @param inWorldSpaceAxis The axis of rotation along which the constraint acts (normalized)
	/// @param inMinLambda Minimum angular impulse to apply (N m s)
	/// @param inMaxLambda Maximum angular impulse to apply (N m s)
	inline bool					SolveVelocityConstraint(Body &ioBody1, Body &ioBody2, Vec3Arg inWorldSpaceAxis, float inMinLambda, float inMaxLambda)
	{
		// Lagrange multiplier is:
		//
		// lambda = -K^-1 (J v + b)
		float lambda = mEffectiveMass * (inWorldSpaceAxis.Dot(ioBody1.GetAngularVelocity() - ioBody2.GetAngularVelocity()) - mSpringPart.GetBias(mTotalLambda));
		float new_lambda = Clamp(mTotalLambda + lambda, inMinLambda, inMaxLambda); // Clamp impulse
		lambda = new_lambda - mTotalLambda; // Lambda potentially got clamped, calculate the new impulse to apply
		mTotalLambda = new_lambda; // Store accumulated impulse

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
		if (inC != 0.0f && !mSpringPart.IsActive())
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
				ioBody1.SubRotationStep(lambda * mInvI1_Axis);
			if (ioBody2.IsDynamic())
				ioBody2.AddRotationStep(lambda * mInvI2_Axis);
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
	Vec3						mInvI1_Axis;
	Vec3						mInvI2_Axis;
	float						mEffectiveMass = 0.0f;
	SpringPart					mSpringPart;
	float						mTotalLambda = 0.0f;
};

JPH_NAMESPACE_END
