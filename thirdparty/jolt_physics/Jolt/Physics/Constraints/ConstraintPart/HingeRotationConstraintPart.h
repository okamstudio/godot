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
	Constrains rotation around 2 axis so that it only allows rotation around 1 axis

	Based on: "Constraints Derivation for Rigid Body Simulation in 3D" - Daniel Chappuis, section 2.4.1

	Constraint equation (eq 87):

	\f[C = \begin{bmatrix}a_1 \cdot b_2 \\ a_1 \cdot c_2\end{bmatrix}\f]

	Jacobian (eq 90):

	\f[J = \begin{bmatrix}
	0	& -b_2 \times a_1	& 0		& b_2 \times a_1	\\
	0	& -c_2 \times a_1	& 0		& c2 \times a_1
	\end{bmatrix}\f]

	Used terms (here and below, everything in world space):\n
	a1 = hinge axis on body 1.\n
	b2, c2 = axis perpendicular to hinge axis on body 2.\n
	x1, x2 = center of mass for the bodies.\n
	v = [v1, w1, v2, w2].\n
	v1, v2 = linear velocity of body 1 and 2.\n
	w1, w2 = angular velocity of body 1 and 2.\n
	M = mass matrix, a diagonal matrix of the mass and inertia with diagonal [m1, I1, m2, I2].\n
	\f$K^{-1} = \left( J M^{-1} J^T \right)^{-1}\f$ = effective mass.\n
	b = velocity bias.\n
	\f$\beta\f$ = baumgarte constant.\n
	E = identity matrix.
**/
class HingeRotationConstraintPart
{
public:
	using Vec2 = Vector<2>;
	using Mat22 = Matrix<2, 2>;

private:
	/// Internal helper function to update velocities of bodies after Lagrange multiplier is calculated
	JPH_INLINE bool				ApplyVelocityStep(Body &ioBody1, Body &ioBody2, const Vec2 &inLambda) const
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
			Vec3 impulse = mB2xA1 * inLambda[0] + mC2xA1 * inLambda[1];
			if (ioBody1.IsDynamic())
				ioBody1.GetMotionProperties()->SubAngularVelocityStep(mInvI1.Multiply3x3(impulse));
			if (ioBody2.IsDynamic())
				ioBody2.GetMotionProperties()->AddAngularVelocityStep(mInvI2.Multiply3x3(impulse));
			return true;
		}

		return false;
	}

public:
	/// Calculate properties used during the functions below
	inline void					CalculateConstraintProperties(const Body &inBody1, Mat44Arg inRotation1, Vec3Arg inWorldSpaceHingeAxis1, const Body &inBody2, Mat44Arg inRotation2, Vec3Arg inWorldSpaceHingeAxis2)
	{
		JPH_ASSERT(inWorldSpaceHingeAxis1.IsNormalized(1.0e-5f));
		JPH_ASSERT(inWorldSpaceHingeAxis2.IsNormalized(1.0e-5f));

		// Calculate hinge axis in world space
		mA1 = inWorldSpaceHingeAxis1;
		Vec3 a2 = inWorldSpaceHingeAxis2;
		float dot = mA1.Dot(a2);
		if (dot <= 1.0e-3f)
		{
			// World space axes are more than 90 degrees apart, get a perpendicular vector in the plane formed by mA1 and a2 as hinge axis until the rotation is less than 90 degrees
			Vec3 perp = a2 - dot * mA1;
			if (perp.LengthSq() < 1.0e-6f)
			{
				// mA1 ~ -a2, take random perpendicular
				perp = mA1.GetNormalizedPerpendicular();
			}

			// Blend in a little bit from mA1 so we're less than 90 degrees apart
			a2 = (0.99f * perp.Normalized() + 0.01f * mA1).Normalized();
		}
		mB2 = a2.GetNormalizedPerpendicular();
		mC2 = a2.Cross(mB2);

		// Calculate properties used during constraint solving
		mInvI1 = inBody1.IsDynamic()? inBody1.GetMotionProperties()->GetInverseInertiaForRotation(inRotation1) : Mat44::sZero();
		mInvI2 = inBody2.IsDynamic()? inBody2.GetMotionProperties()->GetInverseInertiaForRotation(inRotation2) : Mat44::sZero();
		mB2xA1 = mB2.Cross(mA1);
		mC2xA1 = mC2.Cross(mA1);

		// Calculate effective mass: K^-1 = (J M^-1 J^T)^-1
		Mat44 summed_inv_inertia = mInvI1 + mInvI2;
		Mat22 inv_effective_mass;
		inv_effective_mass(0, 0) = mB2xA1.Dot(summed_inv_inertia.Multiply3x3(mB2xA1));
		inv_effective_mass(0, 1) = mB2xA1.Dot(summed_inv_inertia.Multiply3x3(mC2xA1));
		inv_effective_mass(1, 0) = mC2xA1.Dot(summed_inv_inertia.Multiply3x3(mB2xA1));
		inv_effective_mass(1, 1) = mC2xA1.Dot(summed_inv_inertia.Multiply3x3(mC2xA1));
		if (!mEffectiveMass.SetInversed(inv_effective_mass))
			Deactivate();
	}

	/// Deactivate this constraint
	inline void					Deactivate()
	{
		mEffectiveMass.SetZero();
		mTotalLambda.SetZero();
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
		Vec3 delta_ang = ioBody1.GetAngularVelocity() - ioBody2.GetAngularVelocity();
		Vec2 jv;
		jv[0] = mB2xA1.Dot(delta_ang);
		jv[1] = mC2xA1.Dot(delta_ang);
		Vec2 lambda = mEffectiveMass * jv;

		// Store accumulated lambda
		mTotalLambda += lambda;

		return ApplyVelocityStep(ioBody1, ioBody2, lambda);
	}

	/// Iteratively update the position constraint. Makes sure C(...) = 0.
	inline bool					SolvePositionConstraint(Body &ioBody1, Body &ioBody2, float inBaumgarte) const
	{
		// Constraint needs Axis of body 1 perpendicular to both B and C from body 2 (which are both perpendicular to the Axis of body 2)
		Vec2 c;
		c[0] = mA1.Dot(mB2);
		c[1] = mA1.Dot(mC2);
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
			Vec3 impulse = mB2xA1 * lambda[0] + mC2xA1 * lambda[1];
			if (ioBody1.IsDynamic())
				ioBody1.SubRotationStep(mInvI1.Multiply3x3(impulse));
			if (ioBody2.IsDynamic())
				ioBody2.AddRotationStep(mInvI2.Multiply3x3(impulse));
			return true;
		}

		return false;
	}

	/// Return lagrange multiplier
	const Vec2 &				GetTotalLambda() const
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
	Vec3						mA1;						///< World space hinge axis for body 1
	Vec3						mB2;						///< World space perpendiculars of hinge axis for body 2
	Vec3						mC2;
	Mat44						mInvI1;
	Mat44						mInvI2;
	Vec3						mB2xA1;
	Vec3						mC2xA1;
	Mat22						mEffectiveMass;
	Vec2						mTotalLambda { Vec2::sZero() };
};

JPH_NAMESPACE_END
