// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/Math/Vector.h>
#include <Jolt/Math/Matrix.h>

JPH_NAMESPACE_BEGIN

/**
	Constrains movement on 2 axis

	@see "Constraints Derivation for Rigid Body Simulation in 3D" - Daniel Chappuis, section 2.3.1

	Constraint equation (eq 51):

	\f[C = \begin{bmatrix} (p_2 - p_1) \cdot n_1 \\ (p_2 - p_1) \cdot n_2\end{bmatrix}\f]

	Jacobian (transposed) (eq 55):

	\f[J^T = \begin{bmatrix}
	-n_1					& -n_2					\\
	-(r_1 + u) \times n_1	& -(r_1 + u) \times n_2	\\
	n_1						& n_2					\\
	r_2 \times n_1			& r_2 \times n_2
	\end{bmatrix}\f]

	Used terms (here and below, everything in world space):\n
	n1, n2 = constraint axis (normalized).\n
	p1, p2 = constraint points.\n
	r1 = p1 - x1.\n
	r2 = p2 - x2.\n
	u = x2 + r2 - x1 - r1 = p2 - p1.\n
	x1, x2 = center of mass for the bodies.\n
	v = [v1, w1, v2, w2].\n
	v1, v2 = linear velocity of body 1 and 2.\n
	w1, w2 = angular velocity of body 1 and 2.\n
	M = mass matrix, a diagonal matrix of the mass and inertia with diagonal [m1, I1, m2, I2].\n
	\f$K^{-1} = \left( J M^{-1} J^T \right)^{-1}\f$ = effective mass.\n
	b = velocity bias.\n
	\f$\beta\f$ = baumgarte constant.
**/
class DualAxisConstraintPart
{
public:
	using Vec2 = Vector<2>;
	using Mat22 = Matrix<2, 2>;

private:
	/// Internal helper function to update velocities of bodies after Lagrange multiplier is calculated
	JPH_INLINE bool				ApplyVelocityStep(Body &ioBody1, Body &ioBody2, Vec3Arg inN1, Vec3Arg inN2, const Vec2 &inLambda) const
	{
		// Apply impulse if delta is not zero
		if (!inLambda.IsZero())
		{
			// Calculate velocity change due to constraint
			//
			// Impulse:
			// P = J^T lambda
			//
			// Euler velocity integration:
			// v' = v + M^-1 P
			Vec3 impulse = inN1 * inLambda[0] + inN2 * inLambda[1];
			if (ioBody1.IsDynamic())
			{
				MotionProperties *mp1 = ioBody1.GetMotionProperties();
				mp1->SubLinearVelocityStep(mp1->GetInverseMass() * impulse);
				mp1->SubAngularVelocityStep(mInvI1_R1PlusUxN1 * inLambda[0] + mInvI1_R1PlusUxN2 * inLambda[1]);
			}
			if (ioBody2.IsDynamic())
			{
				MotionProperties *mp2 = ioBody2.GetMotionProperties();
				mp2->AddLinearVelocityStep(mp2->GetInverseMass() * impulse);
				mp2->AddAngularVelocityStep(mInvI2_R2xN1 * inLambda[0] + mInvI2_R2xN2 * inLambda[1]);
			}
			return true;
		}

		return false;
	}

	/// Internal helper function to calculate the lagrange multiplier
	inline void					CalculateLagrangeMultiplier(const Body &inBody1, const Body &inBody2, Vec3Arg inN1, Vec3Arg inN2, Vec2 &outLambda) const
	{
		// Calculate lagrange multiplier:
		//
		// lambda = -K^-1 (J v + b)
		Vec3 delta_lin = inBody1.GetLinearVelocity() - inBody2.GetLinearVelocity();
		Vec2 jv;
		jv[0] = inN1.Dot(delta_lin) + mR1PlusUxN1.Dot(inBody1.GetAngularVelocity()) - mR2xN1.Dot(inBody2.GetAngularVelocity());
		jv[1] = inN2.Dot(delta_lin) + mR1PlusUxN2.Dot(inBody1.GetAngularVelocity()) - mR2xN2.Dot(inBody2.GetAngularVelocity());
		outLambda = mEffectiveMass * jv;
	}

public:
	/// Calculate properties used during the functions below
	/// All input vectors are in world space
	inline void					CalculateConstraintProperties(const Body &inBody1, Mat44Arg inRotation1, Vec3Arg inR1PlusU, const Body &inBody2, Mat44Arg inRotation2, Vec3Arg inR2, Vec3Arg inN1, Vec3Arg inN2)
	{
		JPH_ASSERT(inN1.IsNormalized(1.0e-5f));
		JPH_ASSERT(inN2.IsNormalized(1.0e-5f));

		// Calculate properties used during constraint solving
		mR1PlusUxN1 = inR1PlusU.Cross(inN1);
		mR1PlusUxN2 = inR1PlusU.Cross(inN2);
		mR2xN1 = inR2.Cross(inN1);
		mR2xN2 = inR2.Cross(inN2);

		// Calculate effective mass: K^-1 = (J M^-1 J^T)^-1, eq 59
		Mat22 inv_effective_mass;
		if (inBody1.IsDynamic())
		{
			const MotionProperties *mp1 = inBody1.GetMotionProperties();
			Mat44 inv_i1 = mp1->GetInverseInertiaForRotation(inRotation1);
			mInvI1_R1PlusUxN1 = inv_i1.Multiply3x3(mR1PlusUxN1);
			mInvI1_R1PlusUxN2 = inv_i1.Multiply3x3(mR1PlusUxN2);

			inv_effective_mass(0, 0) = mp1->GetInverseMass() + mR1PlusUxN1.Dot(mInvI1_R1PlusUxN1);
			inv_effective_mass(0, 1) = mR1PlusUxN1.Dot(mInvI1_R1PlusUxN2);
			inv_effective_mass(1, 0) = mR1PlusUxN2.Dot(mInvI1_R1PlusUxN1);
			inv_effective_mass(1, 1) = mp1->GetInverseMass() + mR1PlusUxN2.Dot(mInvI1_R1PlusUxN2);
		}
		else
		{
			JPH_IF_DEBUG(mInvI1_R1PlusUxN1 = Vec3::sNaN();)
			JPH_IF_DEBUG(mInvI1_R1PlusUxN2 = Vec3::sNaN();)

			inv_effective_mass = Mat22::sZero();
		}

		if (inBody2.IsDynamic())
		{
			const MotionProperties *mp2 = inBody2.GetMotionProperties();
			Mat44 inv_i2 = mp2->GetInverseInertiaForRotation(inRotation2);
			mInvI2_R2xN1 = inv_i2.Multiply3x3(mR2xN1);
			mInvI2_R2xN2 = inv_i2.Multiply3x3(mR2xN2);

			inv_effective_mass(0, 0) += mp2->GetInverseMass() + mR2xN1.Dot(mInvI2_R2xN1);
			inv_effective_mass(0, 1) += mR2xN1.Dot(mInvI2_R2xN2);
			inv_effective_mass(1, 0) += mR2xN2.Dot(mInvI2_R2xN1);
			inv_effective_mass(1, 1) += mp2->GetInverseMass() + mR2xN2.Dot(mInvI2_R2xN2);
		}
		else
		{
			JPH_IF_DEBUG(mInvI2_R2xN1 = Vec3::sNaN();)
			JPH_IF_DEBUG(mInvI2_R2xN2 = Vec3::sNaN();)
		}

		if (!mEffectiveMass.SetInversed(inv_effective_mass))
			Deactivate();
	}

	/// Deactivate this constraint
	inline void					Deactivate()
	{
		mEffectiveMass.SetZero();
		mTotalLambda.SetZero();
	}

	/// Check if constraint is active
	inline bool					IsActive() const
	{
		return !mEffectiveMass.IsZero();
	}

	/// Must be called from the WarmStartVelocityConstraint call to apply the previous frame's impulses
	/// All input vectors are in world space
	inline void					WarmStart(Body &ioBody1, Body &ioBody2, Vec3Arg inN1, Vec3Arg inN2, float inWarmStartImpulseRatio)
	{
		mTotalLambda *= inWarmStartImpulseRatio;
		ApplyVelocityStep(ioBody1, ioBody2, inN1, inN2, mTotalLambda);
	}

	/// Iteratively update the velocity constraint. Makes sure d/dt C(...) = 0, where C is the constraint equation.
	/// All input vectors are in world space
	inline bool					SolveVelocityConstraint(Body &ioBody1, Body &ioBody2, Vec3Arg inN1, Vec3Arg inN2)
	{
		Vec2 lambda;
		CalculateLagrangeMultiplier(ioBody1, ioBody2, inN1, inN2, lambda);

		// Store accumulated lambda
		mTotalLambda += lambda;

		return ApplyVelocityStep(ioBody1, ioBody2, inN1, inN2, lambda);
	}

	/// Iteratively update the position constraint. Makes sure C(...) = 0.
	/// All input vectors are in world space
	inline bool					SolvePositionConstraint(Body &ioBody1, Body &ioBody2, Vec3Arg inU, Vec3Arg inN1, Vec3Arg inN2, float inBaumgarte) const
	{
		Vec2 c;
		c[0] = inU.Dot(inN1);
		c[1] = inU.Dot(inN2);
		if (!c.IsZero())
		{
			// Calculate lagrange multiplier (lambda) for Baumgarte stabilization:
			//
			// lambda = -K^-1 * beta / dt * C
			//
			// We should divide by inDeltaTime, but we should multiply by inDeltaTime in the Euler step below so they're cancelled out
			Vec2 lambda = -inBaumgarte * (mEffectiveMass * c);

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
			Vec3 impulse = inN1 * lambda[0] + inN2 * lambda[1];
			if (ioBody1.IsDynamic())
			{
				ioBody1.SubPositionStep(ioBody1.GetMotionProperties()->GetInverseMass() * impulse);
				ioBody1.SubRotationStep(mInvI1_R1PlusUxN1 * lambda[0] + mInvI1_R1PlusUxN2 * lambda[1]);
			}
			if (ioBody2.IsDynamic())
			{
				ioBody2.AddPositionStep(ioBody2.GetMotionProperties()->GetInverseMass() * impulse);
				ioBody2.AddRotationStep(mInvI2_R2xN1 * lambda[0] + mInvI2_R2xN2 * lambda[1]);
			}
			return true;
		}

		return false;
	}

	/// Override total lagrange multiplier, can be used to set the initial value for warm starting
	inline void					SetTotalLambda(const Vec2 &inLambda)
	{
		mTotalLambda = inLambda;
	}

	/// Return lagrange multiplier
	inline const Vec2 &			GetTotalLambda() const
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
	Vec3						mR1PlusUxN1;
	Vec3						mR1PlusUxN2;
	Vec3						mR2xN1;
	Vec3						mR2xN2;
	Vec3						mInvI1_R1PlusUxN1;
	Vec3						mInvI1_R1PlusUxN2;
	Vec3						mInvI2_R2xN1;
	Vec3						mInvI2_R2xN2;
	Mat22						mEffectiveMass;
	Vec2						mTotalLambda { Vec2::sZero() };
};

JPH_NAMESPACE_END
