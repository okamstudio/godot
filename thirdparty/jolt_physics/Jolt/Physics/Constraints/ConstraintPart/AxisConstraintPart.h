// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Constraints/ConstraintPart/SpringPart.h>
#include <Jolt/Physics/Constraints/SpringSettings.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/Physics/DeterminismLog.h>

JPH_NAMESPACE_BEGIN

/// Constraint that constrains motion along 1 axis
///
/// @see "Constraints Derivation for Rigid Body Simulation in 3D" - Daniel Chappuis, section 2.1.1
/// (we're not using the approximation of eq 27 but instead add the U term as in eq 55)
///
/// Constraint equation (eq 25):
///
/// \f[C = (p_2 - p_1) \cdot n\f]
///
/// Jacobian (eq 28):
///
/// \f[J = \begin{bmatrix} -n^T & (-(r_1 + u) \times n)^T & n^T & (r_2 \times n)^T \end{bmatrix}\f]
///
/// Used terms (here and below, everything in world space):\n
/// n = constraint axis (normalized).\n
/// p1, p2 = constraint points.\n
/// r1 = p1 - x1.\n
/// r2 = p2 - x2.\n
/// u = x2 + r2 - x1 - r1 = p2 - p1.\n
/// x1, x2 = center of mass for the bodies.\n
/// v = [v1, w1, v2, w2].\n
/// v1, v2 = linear velocity of body 1 and 2.\n
/// w1, w2 = angular velocity of body 1 and 2.\n
/// M = mass matrix, a diagonal matrix of the mass and inertia with diagonal [m1, I1, m2, I2].\n
/// \f$K^{-1} = \left( J M^{-1} J^T \right)^{-1}\f$ = effective mass.\n
/// b = velocity bias.\n
/// \f$\beta\f$ = baumgarte constant.
class AxisConstraintPart
{
	/// Internal helper function to update velocities of bodies after Lagrange multiplier is calculated
	template <EMotionType Type1, EMotionType Type2>
	JPH_INLINE bool				ApplyVelocityStep(MotionProperties *ioMotionProperties1, float inInvMass1, MotionProperties *ioMotionProperties2, float inInvMass2, Vec3Arg inWorldSpaceAxis, float inLambda) const
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
			if constexpr (Type1 == EMotionType::Dynamic)
			{
				ioMotionProperties1->SubLinearVelocityStep((inLambda * inInvMass1) * inWorldSpaceAxis);
				ioMotionProperties1->SubAngularVelocityStep(inLambda * Vec3::sLoadFloat3Unsafe(mInvI1_R1PlusUxAxis));
			}
			if constexpr (Type2 == EMotionType::Dynamic)
			{
				ioMotionProperties2->AddLinearVelocityStep((inLambda * inInvMass2) * inWorldSpaceAxis);
				ioMotionProperties2->AddAngularVelocityStep(inLambda * Vec3::sLoadFloat3Unsafe(mInvI2_R2xAxis));
			}
			return true;
		}

		return false;
	}

	/// Internal helper function to calculate the inverse effective mass
	template <EMotionType Type1, EMotionType Type2>
	JPH_INLINE float			TemplatedCalculateInverseEffectiveMass(float inInvMass1, Mat44Arg inInvI1, Vec3Arg inR1PlusU, float inInvMass2, Mat44Arg inInvI2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis)
	{
		JPH_ASSERT(inWorldSpaceAxis.IsNormalized(1.0e-5f));

		// Calculate properties used below
		Vec3 r1_plus_u_x_axis;
		if constexpr (Type1 != EMotionType::Static)
		{
			r1_plus_u_x_axis = inR1PlusU.Cross(inWorldSpaceAxis);
			r1_plus_u_x_axis.StoreFloat3(&mR1PlusUxAxis);
		}
		else
		{
		#ifdef JPH_DEBUG
			Vec3::sNaN().StoreFloat3(&mR1PlusUxAxis);
		#endif
		}

		Vec3 r2_x_axis;
		if constexpr (Type2 != EMotionType::Static)
		{
			r2_x_axis = inR2.Cross(inWorldSpaceAxis);
			r2_x_axis.StoreFloat3(&mR2xAxis);
		}
		else
		{
		#ifdef JPH_DEBUG
			Vec3::sNaN().StoreFloat3(&mR2xAxis);
		#endif
		}

		// Calculate inverse effective mass: K = J M^-1 J^T
		float inv_effective_mass;

		if constexpr (Type1 == EMotionType::Dynamic)
		{
			Vec3 invi1_r1_plus_u_x_axis = inInvI1.Multiply3x3(r1_plus_u_x_axis);
			invi1_r1_plus_u_x_axis.StoreFloat3(&mInvI1_R1PlusUxAxis);
			inv_effective_mass = inInvMass1 + invi1_r1_plus_u_x_axis.Dot(r1_plus_u_x_axis);
		}
		else
		{
			(void)r1_plus_u_x_axis; // Fix compiler warning: Not using this (it's not calculated either)
			JPH_IF_DEBUG(Vec3::sNaN().StoreFloat3(&mInvI1_R1PlusUxAxis);)
			inv_effective_mass = 0.0f;
		}

		if constexpr (Type2 == EMotionType::Dynamic)
		{
			Vec3 invi2_r2_x_axis = inInvI2.Multiply3x3(r2_x_axis);
			invi2_r2_x_axis.StoreFloat3(&mInvI2_R2xAxis);
			inv_effective_mass += inInvMass2 + invi2_r2_x_axis.Dot(r2_x_axis);
		}
		else
		{
			(void)r2_x_axis; // Fix compiler warning: Not using this (it's not calculated either)
			JPH_IF_DEBUG(Vec3::sNaN().StoreFloat3(&mInvI2_R2xAxis);)
		}

		return inv_effective_mass;
	}

	/// Internal helper function to calculate the inverse effective mass
	JPH_INLINE float			CalculateInverseEffectiveMass(const Body &inBody1, Vec3Arg inR1PlusU, const Body &inBody2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis)
	{
		// Dispatch to the correct templated form
		switch (inBody1.GetMotionType())
		{
		case EMotionType::Dynamic:
			{
				const MotionProperties *mp1 = inBody1.GetMotionPropertiesUnchecked();
				float inv_m1 = mp1->GetInverseMass();
				Mat44 inv_i1 = inBody1.GetInverseInertia();
				switch (inBody2.GetMotionType())
				{
				case EMotionType::Dynamic:
					return TemplatedCalculateInverseEffectiveMass<EMotionType::Dynamic, EMotionType::Dynamic>(inv_m1, inv_i1, inR1PlusU, inBody2.GetMotionPropertiesUnchecked()->GetInverseMass(), inBody2.GetInverseInertia(), inR2, inWorldSpaceAxis);

				case EMotionType::Kinematic:
					return TemplatedCalculateInverseEffectiveMass<EMotionType::Dynamic, EMotionType::Kinematic>(inv_m1, inv_i1, inR1PlusU, 0 /* Will not be used */, Mat44() /* Will not be used */, inR2, inWorldSpaceAxis);

				case EMotionType::Static:
					return TemplatedCalculateInverseEffectiveMass<EMotionType::Dynamic, EMotionType::Static>(inv_m1, inv_i1, inR1PlusU, 0 /* Will not be used */, Mat44() /* Will not be used */, inR2, inWorldSpaceAxis);

				default:
					break;
				}
				break;
			}

		case EMotionType::Kinematic:
			JPH_ASSERT(inBody2.IsDynamic());
			return TemplatedCalculateInverseEffectiveMass<EMotionType::Kinematic, EMotionType::Dynamic>(0 /* Will not be used */, Mat44() /* Will not be used */, inR1PlusU, inBody2.GetMotionPropertiesUnchecked()->GetInverseMass(), inBody2.GetInverseInertia(), inR2, inWorldSpaceAxis);

		case EMotionType::Static:
			JPH_ASSERT(inBody2.IsDynamic());
			return TemplatedCalculateInverseEffectiveMass<EMotionType::Static, EMotionType::Dynamic>(0 /* Will not be used */, Mat44() /* Will not be used */, inR1PlusU, inBody2.GetMotionPropertiesUnchecked()->GetInverseMass(), inBody2.GetInverseInertia(), inR2, inWorldSpaceAxis);

		default:
			break;
		}

		JPH_ASSERT(false);
		return 0.0f;
	}

	/// Internal helper function to calculate the inverse effective mass, version that supports mass scaling
	JPH_INLINE float			CalculateInverseEffectiveMassWithMassOverride(const Body &inBody1, float inInvMass1, float inInvInertiaScale1, Vec3Arg inR1PlusU, const Body &inBody2, float inInvMass2, float inInvInertiaScale2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis)
	{
		// Dispatch to the correct templated form
		switch (inBody1.GetMotionType())
		{
		case EMotionType::Dynamic:
			{
				Mat44 inv_i1 = inInvInertiaScale1 * inBody1.GetInverseInertia();
				switch (inBody2.GetMotionType())
				{
				case EMotionType::Dynamic:
					return TemplatedCalculateInverseEffectiveMass<EMotionType::Dynamic, EMotionType::Dynamic>(inInvMass1, inv_i1, inR1PlusU, inInvMass2, inInvInertiaScale2 * inBody2.GetInverseInertia(), inR2, inWorldSpaceAxis);

				case EMotionType::Kinematic:
					return TemplatedCalculateInverseEffectiveMass<EMotionType::Dynamic, EMotionType::Kinematic>(inInvMass1, inv_i1, inR1PlusU, 0 /* Will not be used */, Mat44() /* Will not be used */, inR2, inWorldSpaceAxis);

				case EMotionType::Static:
					return TemplatedCalculateInverseEffectiveMass<EMotionType::Dynamic, EMotionType::Static>(inInvMass1, inv_i1, inR1PlusU, 0 /* Will not be used */, Mat44() /* Will not be used */, inR2, inWorldSpaceAxis);

				default:
					break;
				}
				break;
			}

		case EMotionType::Kinematic:
			JPH_ASSERT(inBody2.IsDynamic());
			return TemplatedCalculateInverseEffectiveMass<EMotionType::Kinematic, EMotionType::Dynamic>(0 /* Will not be used */, Mat44() /* Will not be used */, inR1PlusU, inInvMass2, inInvInertiaScale2 * inBody2.GetInverseInertia(), inR2, inWorldSpaceAxis);

		case EMotionType::Static:
			JPH_ASSERT(inBody2.IsDynamic());
			return TemplatedCalculateInverseEffectiveMass<EMotionType::Static, EMotionType::Dynamic>(0 /* Will not be used */, Mat44() /* Will not be used */, inR1PlusU, inInvMass2, inInvInertiaScale2 * inBody2.GetInverseInertia(), inR2, inWorldSpaceAxis);

		default:
			break;
		}

		JPH_ASSERT(false);
		return 0.0f;
	}

public:
	/// Templated form of CalculateConstraintProperties with the motion types baked in
	template <EMotionType Type1, EMotionType Type2>
	JPH_INLINE void				TemplatedCalculateConstraintProperties(float inInvMass1, Mat44Arg inInvI1, Vec3Arg inR1PlusU, float inInvMass2, Mat44Arg inInvI2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis, float inBias = 0.0f)
	{
		float inv_effective_mass = TemplatedCalculateInverseEffectiveMass<Type1, Type2>(inInvMass1, inInvI1, inR1PlusU, inInvMass2, inInvI2, inR2, inWorldSpaceAxis);

		if (inv_effective_mass == 0.0f)
			Deactivate();
		else
		{
			mEffectiveMass = 1.0f / inv_effective_mass;
			mSpringPart.CalculateSpringPropertiesWithBias(inBias);
		}

		JPH_DET_LOG("TemplatedCalculateConstraintProperties: invM1: " << inInvMass1 << " invI1: " << inInvI1 << " r1PlusU: " << inR1PlusU << " invM2: " << inInvMass2 << " invI2: " << inInvI2 << " r2: " << inR2 << " bias: " << inBias << " r1PlusUxAxis: " << mR1PlusUxAxis << " r2xAxis: " << mR2xAxis << " invI1_R1PlusUxAxis: " << mInvI1_R1PlusUxAxis << " invI2_R2xAxis: " << mInvI2_R2xAxis << " effectiveMass: " << mEffectiveMass << " totalLambda: " << mTotalLambda);
	}

	/// Calculate properties used during the functions below
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inR1PlusU See equations above (r1 + u)
	/// @param inR2 See equations above (r2)
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized, pointing from body 1 to 2)
	/// @param inBias Bias term (b) for the constraint impulse: lambda = J v + b
	inline void					CalculateConstraintProperties(const Body &inBody1, Vec3Arg inR1PlusU, const Body &inBody2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis, float inBias = 0.0f)
	{
		float inv_effective_mass = CalculateInverseEffectiveMass(inBody1, inR1PlusU, inBody2, inR2, inWorldSpaceAxis);

		if (inv_effective_mass == 0.0f)
			Deactivate();
		else
		{
			mEffectiveMass = 1.0f / inv_effective_mass;
			mSpringPart.CalculateSpringPropertiesWithBias(inBias);
		}
	}

	/// Calculate properties used during the functions below, version that supports mass scaling
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inInvMass1 The inverse mass of body 1 (only used when body 1 is dynamic)
	/// @param inInvMass2 The inverse mass of body 2 (only used when body 2 is dynamic)
	/// @param inInvInertiaScale1 Scale factor for the inverse inertia of body 1
	/// @param inInvInertiaScale2 Scale factor for the inverse inertia of body 2
	/// @param inR1PlusU See equations above (r1 + u)
	/// @param inR2 See equations above (r2)
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized, pointing from body 1 to 2)
	/// @param inBias Bias term (b) for the constraint impulse: lambda = J v + b
	inline void					CalculateConstraintPropertiesWithMassOverride(const Body &inBody1, float inInvMass1, float inInvInertiaScale1, Vec3Arg inR1PlusU, const Body &inBody2, float inInvMass2, float inInvInertiaScale2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis, float inBias = 0.0f)
	{
		float inv_effective_mass = CalculateInverseEffectiveMassWithMassOverride(inBody1, inInvMass1, inInvInertiaScale1, inR1PlusU, inBody2, inInvMass2, inInvInertiaScale2, inR2, inWorldSpaceAxis);

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
	/// @param inR1PlusU See equations above (r1 + u)
	/// @param inR2 See equations above (r2)
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized, pointing from body 1 to 2)
	/// @param inBias Bias term (b) for the constraint impulse: lambda = J v + b
	///	@param inC Value of the constraint equation (C).
	///	@param inFrequency Oscillation frequency (Hz).
	///	@param inDamping Damping factor (0 = no damping, 1 = critical damping).
	inline void					CalculateConstraintPropertiesWithFrequencyAndDamping(float inDeltaTime, const Body &inBody1, Vec3Arg inR1PlusU, const Body &inBody2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis, float inBias, float inC, float inFrequency, float inDamping)
	{
		float inv_effective_mass = CalculateInverseEffectiveMass(inBody1, inR1PlusU, inBody2, inR2, inWorldSpaceAxis);

		if (inv_effective_mass == 0.0f)
			Deactivate();
		else
			mSpringPart.CalculateSpringPropertiesWithFrequencyAndDamping(inDeltaTime, inv_effective_mass, inBias, inC, inFrequency, inDamping, mEffectiveMass);
	}

	/// Calculate properties used during the functions below
	/// @param inDeltaTime Time step
	/// @param inBody1 The first body that this constraint is attached to
	/// @param inBody2 The second body that this constraint is attached to
	/// @param inR1PlusU See equations above (r1 + u)
	/// @param inR2 See equations above (r2)
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized, pointing from body 1 to 2)
	/// @param inBias Bias term (b) for the constraint impulse: lambda = J v + b
	///	@param inC Value of the constraint equation (C).
	///	@param inStiffness Spring stiffness k.
	///	@param inDamping Spring damping coefficient c.
	inline void					CalculateConstraintPropertiesWithStiffnessAndDamping(float inDeltaTime, const Body &inBody1, Vec3Arg inR1PlusU, const Body &inBody2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis, float inBias, float inC, float inStiffness, float inDamping)
	{
		float inv_effective_mass = CalculateInverseEffectiveMass(inBody1, inR1PlusU, inBody2, inR2, inWorldSpaceAxis);

		if (inv_effective_mass == 0.0f)
			Deactivate();
		else
			mSpringPart.CalculateSpringPropertiesWithStiffnessAndDamping(inDeltaTime, inv_effective_mass, inBias, inC, inStiffness, inDamping, mEffectiveMass);
	}

	/// Selects one of the above functions based on the spring settings
	inline void					CalculateConstraintPropertiesWithSettings(float inDeltaTime, const Body &inBody1, Vec3Arg inR1PlusU, const Body &inBody2, Vec3Arg inR2, Vec3Arg inWorldSpaceAxis, float inBias, float inC, const SpringSettings &inSpringSettings)
	{
		float inv_effective_mass = CalculateInverseEffectiveMass(inBody1, inR1PlusU, inBody2, inR2, inWorldSpaceAxis);

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

	/// Templated form of WarmStart with the motion types baked in
	template <EMotionType Type1, EMotionType Type2>
	inline void					TemplatedWarmStart(MotionProperties *ioMotionProperties1, float inInvMass1, MotionProperties *ioMotionProperties2, float inInvMass2, Vec3Arg inWorldSpaceAxis, float inWarmStartImpulseRatio)
	{
		mTotalLambda *= inWarmStartImpulseRatio;

		ApplyVelocityStep<Type1, Type2>(ioMotionProperties1, inInvMass1, ioMotionProperties2, inInvMass2, inWorldSpaceAxis, mTotalLambda);
	}

	/// Must be called from the WarmStartVelocityConstraint call to apply the previous frame's impulses
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized)
	/// @param inWarmStartImpulseRatio Ratio of new step to old time step (dt_new / dt_old) for scaling the lagrange multiplier of the previous frame
	inline void					WarmStart(Body &ioBody1, Body &ioBody2, Vec3Arg inWorldSpaceAxis, float inWarmStartImpulseRatio)
	{
		EMotionType motion_type1 = ioBody1.GetMotionType();
		MotionProperties *motion_properties1 = ioBody1.GetMotionPropertiesUnchecked();

		EMotionType motion_type2 = ioBody2.GetMotionType();
		MotionProperties *motion_properties2 = ioBody2.GetMotionPropertiesUnchecked();

		// Dispatch to the correct templated form
		// Note: Warm starting doesn't differentiate between kinematic/static bodies so we handle both as static bodies
		if (motion_type1 == EMotionType::Dynamic)
		{
			if (motion_type2 == EMotionType::Dynamic)
				TemplatedWarmStart<EMotionType::Dynamic, EMotionType::Dynamic>(motion_properties1, motion_properties1->GetInverseMass(), motion_properties2, motion_properties2->GetInverseMass(), inWorldSpaceAxis, inWarmStartImpulseRatio);
			else
				TemplatedWarmStart<EMotionType::Dynamic, EMotionType::Static>(motion_properties1, motion_properties1->GetInverseMass(), motion_properties2, 0.0f /* Unused */, inWorldSpaceAxis, inWarmStartImpulseRatio);
		}
		else
		{
			JPH_ASSERT(motion_type2 == EMotionType::Dynamic);
			TemplatedWarmStart<EMotionType::Static, EMotionType::Dynamic>(motion_properties1, 0.0f /* Unused */, motion_properties2, motion_properties2->GetInverseMass(), inWorldSpaceAxis, inWarmStartImpulseRatio);
		}
	}

	/// Templated form of SolveVelocityConstraint with the motion types baked in, part 1: get the total lambda
	template <EMotionType Type1, EMotionType Type2>
	JPH_INLINE float			TemplatedSolveVelocityConstraintGetTotalLambda(const MotionProperties *ioMotionProperties1, const MotionProperties *ioMotionProperties2, Vec3Arg inWorldSpaceAxis) const
	{
		// Calculate jacobian multiplied by linear velocity
		float jv;
		if constexpr (Type1 != EMotionType::Static && Type2 != EMotionType::Static)
			jv = inWorldSpaceAxis.Dot(ioMotionProperties1->GetLinearVelocity() - ioMotionProperties2->GetLinearVelocity());
		else if constexpr (Type1 != EMotionType::Static)
			jv = inWorldSpaceAxis.Dot(ioMotionProperties1->GetLinearVelocity());
		else if constexpr (Type2 != EMotionType::Static)
			jv = inWorldSpaceAxis.Dot(-ioMotionProperties2->GetLinearVelocity());
		else
			JPH_ASSERT(false); // Static vs static is nonsensical!

		// Calculate jacobian multiplied by angular velocity
		if constexpr (Type1 != EMotionType::Static)
			jv += Vec3::sLoadFloat3Unsafe(mR1PlusUxAxis).Dot(ioMotionProperties1->GetAngularVelocity());
		if constexpr (Type2 != EMotionType::Static)
			jv -= Vec3::sLoadFloat3Unsafe(mR2xAxis).Dot(ioMotionProperties2->GetAngularVelocity());

		// Lagrange multiplier is:
		//
		// lambda = -K^-1 (J v + b)
		float lambda = mEffectiveMass * (jv - mSpringPart.GetBias(mTotalLambda));

		// Return the total accumulated lambda
		return mTotalLambda + lambda;
	}

	/// Templated form of SolveVelocityConstraint with the motion types baked in, part 2: apply new lambda
	template <EMotionType Type1, EMotionType Type2>
	JPH_INLINE bool				TemplatedSolveVelocityConstraintApplyLambda(MotionProperties *ioMotionProperties1, float inInvMass1, MotionProperties *ioMotionProperties2, float inInvMass2, Vec3Arg inWorldSpaceAxis, float inTotalLambda)
	{
		float delta_lambda = inTotalLambda - mTotalLambda; // Calculate change in lambda
		mTotalLambda = inTotalLambda; // Store accumulated impulse

		return ApplyVelocityStep<Type1, Type2>(ioMotionProperties1, inInvMass1, ioMotionProperties2, inInvMass2, inWorldSpaceAxis, delta_lambda);
	}

	/// Templated form of SolveVelocityConstraint with the motion types baked in
	template <EMotionType Type1, EMotionType Type2>
	inline bool					TemplatedSolveVelocityConstraint(MotionProperties *ioMotionProperties1, float inInvMass1, MotionProperties *ioMotionProperties2, float inInvMass2, Vec3Arg inWorldSpaceAxis, float inMinLambda, float inMaxLambda)
	{
		float total_lambda = TemplatedSolveVelocityConstraintGetTotalLambda<Type1, Type2>(ioMotionProperties1, ioMotionProperties2, inWorldSpaceAxis);

		// Clamp impulse to specified range
		total_lambda = Clamp(total_lambda, inMinLambda, inMaxLambda);

		return TemplatedSolveVelocityConstraintApplyLambda<Type1, Type2>(ioMotionProperties1, inInvMass1, ioMotionProperties2, inInvMass2, inWorldSpaceAxis, total_lambda);
	}

	/// Iteratively update the velocity constraint. Makes sure d/dt C(...) = 0, where C is the constraint equation.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized)
	/// @param inMinLambda Minimum value of constraint impulse to apply (N s)
	/// @param inMaxLambda Maximum value of constraint impulse to apply (N s)
	inline bool					SolveVelocityConstraint(Body &ioBody1, Body &ioBody2, Vec3Arg inWorldSpaceAxis, float inMinLambda, float inMaxLambda)
	{
		EMotionType motion_type1 = ioBody1.GetMotionType();
		MotionProperties *motion_properties1 = ioBody1.GetMotionPropertiesUnchecked();

		EMotionType motion_type2 = ioBody2.GetMotionType();
		MotionProperties *motion_properties2 = ioBody2.GetMotionPropertiesUnchecked();

		// Dispatch to the correct templated form
		switch (motion_type1)
		{
		case EMotionType::Dynamic:
			switch (motion_type2)
			{
			case EMotionType::Dynamic:
				return TemplatedSolveVelocityConstraint<EMotionType::Dynamic, EMotionType::Dynamic>(motion_properties1, motion_properties1->GetInverseMass(), motion_properties2, motion_properties2->GetInverseMass(), inWorldSpaceAxis, inMinLambda, inMaxLambda);

			case EMotionType::Kinematic:
				return TemplatedSolveVelocityConstraint<EMotionType::Dynamic, EMotionType::Kinematic>(motion_properties1, motion_properties1->GetInverseMass(), motion_properties2, 0.0f /* Unused */, inWorldSpaceAxis, inMinLambda, inMaxLambda);

			case EMotionType::Static:
				return TemplatedSolveVelocityConstraint<EMotionType::Dynamic, EMotionType::Static>(motion_properties1, motion_properties1->GetInverseMass(), motion_properties2, 0.0f /* Unused */, inWorldSpaceAxis, inMinLambda, inMaxLambda);

			default:
				JPH_ASSERT(false);
				break;
			}
			break;

		case EMotionType::Kinematic:
			JPH_ASSERT(motion_type2 == EMotionType::Dynamic);
			return TemplatedSolveVelocityConstraint<EMotionType::Kinematic, EMotionType::Dynamic>(motion_properties1, 0.0f /* Unused */, motion_properties2, motion_properties2->GetInverseMass(), inWorldSpaceAxis, inMinLambda, inMaxLambda);

		case EMotionType::Static:
			JPH_ASSERT(motion_type2 == EMotionType::Dynamic);
			return TemplatedSolveVelocityConstraint<EMotionType::Static, EMotionType::Dynamic>(motion_properties1, 0.0f /* Unused */, motion_properties2, motion_properties2->GetInverseMass(), inWorldSpaceAxis, inMinLambda, inMaxLambda);

		default:
			JPH_ASSERT(false);
			break;
		}

		return false;
	}

	/// Iteratively update the velocity constraint. Makes sure d/dt C(...) = 0, where C is the constraint equation.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inInvMass1 The inverse mass of body 1 (only used when body 1 is dynamic)
	/// @param inInvMass2 The inverse mass of body 2 (only used when body 2 is dynamic)
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized)
	/// @param inMinLambda Minimum value of constraint impulse to apply (N s)
	/// @param inMaxLambda Maximum value of constraint impulse to apply (N s)
	inline bool					SolveVelocityConstraintWithMassOverride(Body &ioBody1, float inInvMass1, Body &ioBody2, float inInvMass2, Vec3Arg inWorldSpaceAxis, float inMinLambda, float inMaxLambda)
	{
		EMotionType motion_type1 = ioBody1.GetMotionType();
		MotionProperties *motion_properties1 = ioBody1.GetMotionPropertiesUnchecked();

		EMotionType motion_type2 = ioBody2.GetMotionType();
		MotionProperties *motion_properties2 = ioBody2.GetMotionPropertiesUnchecked();

		// Dispatch to the correct templated form
		switch (motion_type1)
		{
		case EMotionType::Dynamic:
			switch (motion_type2)
			{
			case EMotionType::Dynamic:
				return TemplatedSolveVelocityConstraint<EMotionType::Dynamic, EMotionType::Dynamic>(motion_properties1, inInvMass1, motion_properties2, inInvMass2, inWorldSpaceAxis, inMinLambda, inMaxLambda);

			case EMotionType::Kinematic:
				return TemplatedSolveVelocityConstraint<EMotionType::Dynamic, EMotionType::Kinematic>(motion_properties1, inInvMass1, motion_properties2, 0.0f /* Unused */, inWorldSpaceAxis, inMinLambda, inMaxLambda);

			case EMotionType::Static:
				return TemplatedSolveVelocityConstraint<EMotionType::Dynamic, EMotionType::Static>(motion_properties1, inInvMass1, motion_properties2, 0.0f /* Unused */, inWorldSpaceAxis, inMinLambda, inMaxLambda);

			default:
				JPH_ASSERT(false);
				break;
			}
			break;

		case EMotionType::Kinematic:
			JPH_ASSERT(motion_type2 == EMotionType::Dynamic);
			return TemplatedSolveVelocityConstraint<EMotionType::Kinematic, EMotionType::Dynamic>(motion_properties1, 0.0f /* Unused */, motion_properties2, inInvMass2, inWorldSpaceAxis, inMinLambda, inMaxLambda);

		case EMotionType::Static:
			JPH_ASSERT(motion_type2 == EMotionType::Dynamic);
			return TemplatedSolveVelocityConstraint<EMotionType::Static, EMotionType::Dynamic>(motion_properties1, 0.0f /* Unused */, motion_properties2, inInvMass2, inWorldSpaceAxis, inMinLambda, inMaxLambda);

		default:
			JPH_ASSERT(false);
			break;
		}

		return false;
	}

	/// Iteratively update the position constraint. Makes sure C(...) = 0.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized)
	/// @param inC Value of the constraint equation (C)
	/// @param inBaumgarte Baumgarte constant (fraction of the error to correct)
	inline bool					SolvePositionConstraint(Body &ioBody1, Body &ioBody2, Vec3Arg inWorldSpaceAxis, float inC, float inBaumgarte) const
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
			{
				ioBody1.SubPositionStep((lambda * ioBody1.GetMotionProperties()->GetInverseMass()) * inWorldSpaceAxis);
				ioBody1.SubRotationStep(lambda * Vec3::sLoadFloat3Unsafe(mInvI1_R1PlusUxAxis));
			}
			if (ioBody2.IsDynamic())
			{
				ioBody2.AddPositionStep((lambda * ioBody2.GetMotionProperties()->GetInverseMass()) * inWorldSpaceAxis);
				ioBody2.AddRotationStep(lambda * Vec3::sLoadFloat3Unsafe(mInvI2_R2xAxis));
			}
			return true;
		}

		return false;
	}

	/// Iteratively update the position constraint. Makes sure C(...) = 0.
	/// @param ioBody1 The first body that this constraint is attached to
	/// @param ioBody2 The second body that this constraint is attached to
	/// @param inInvMass1 The inverse mass of body 1 (only used when body 1 is dynamic)
	/// @param inInvMass2 The inverse mass of body 2 (only used when body 2 is dynamic)
	/// @param inWorldSpaceAxis Axis along which the constraint acts (normalized)
	/// @param inC Value of the constraint equation (C)
	/// @param inBaumgarte Baumgarte constant (fraction of the error to correct)
	inline bool					SolvePositionConstraintWithMassOverride(Body &ioBody1, float inInvMass1, Body &ioBody2, float inInvMass2, Vec3Arg inWorldSpaceAxis, float inC, float inBaumgarte) const
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
			{
				ioBody1.SubPositionStep((lambda * inInvMass1) * inWorldSpaceAxis);
				ioBody1.SubRotationStep(lambda * Vec3::sLoadFloat3Unsafe(mInvI1_R1PlusUxAxis));
			}
			if (ioBody2.IsDynamic())
			{
				ioBody2.AddPositionStep((lambda * inInvMass2) * inWorldSpaceAxis);
				ioBody2.AddRotationStep(lambda * Vec3::sLoadFloat3Unsafe(mInvI2_R2xAxis));
			}
			return true;
		}

		return false;
	}

	/// Override total lagrange multiplier, can be used to set the initial value for warm starting
	inline void					SetTotalLambda(float inLambda)
	{
		mTotalLambda = inLambda;
	}

	/// Return lagrange multiplier
	inline float				GetTotalLambda() const
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
	Float3						mR1PlusUxAxis;
	Float3						mR2xAxis;
	Float3						mInvI1_R1PlusUxAxis;
	Float3						mInvI2_R2xAxis;
	float						mEffectiveMass = 0.0f;
	SpringPart					mSpringPart;
	float						mTotalLambda = 0.0f;
};

JPH_NAMESPACE_END
