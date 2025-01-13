// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/EstimateCollisionResponse.h>
#include <Jolt/Physics/Body/Body.h>

JPH_NAMESPACE_BEGIN

void EstimateCollisionResponse(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, CollisionEstimationResult &outResult, float inCombinedFriction, float inCombinedRestitution, float inMinVelocityForRestitution, uint inNumIterations)
{
	// Note this code is based on AxisConstraintPart, see that class for more comments on the math

	ContactPoints::size_type num_points = inManifold.mRelativeContactPointsOn1.size();
	JPH_ASSERT(num_points == inManifold.mRelativeContactPointsOn2.size());

	// Start with zero impulses
	outResult.mImpulses.resize(num_points);
	memset(outResult.mImpulses.data(), 0, num_points * sizeof(CollisionEstimationResult::Impulse));

	// Calculate friction directions
	outResult.mTangent1 = inManifold.mWorldSpaceNormal.GetNormalizedPerpendicular();
	outResult.mTangent2 = inManifold.mWorldSpaceNormal.Cross(outResult.mTangent1);

	// Get body velocities
	EMotionType motion_type1 = inBody1.GetMotionType();
	const MotionProperties *motion_properties1 = inBody1.GetMotionPropertiesUnchecked();
	if (motion_type1 != EMotionType::Static)
	{
		outResult.mLinearVelocity1 = motion_properties1->GetLinearVelocity();
		outResult.mAngularVelocity1 = motion_properties1->GetAngularVelocity();
	}
	else
		outResult.mLinearVelocity1 = outResult.mAngularVelocity1 = Vec3::sZero();

	EMotionType motion_type2 = inBody2.GetMotionType();
	const MotionProperties *motion_properties2 = inBody2.GetMotionPropertiesUnchecked();
	if (motion_type2 != EMotionType::Static)
	{
		outResult.mLinearVelocity2 = motion_properties2->GetLinearVelocity();
		outResult.mAngularVelocity2 = motion_properties2->GetAngularVelocity();
	}
	else
		outResult.mLinearVelocity2 = outResult.mAngularVelocity2 = Vec3::sZero();

	// Get inverse mass and inertia
	float inv_m1, inv_m2;
	Mat44 inv_i1, inv_i2;
	if (motion_type1 == EMotionType::Dynamic)
	{
		inv_m1 = motion_properties1->GetInverseMass();
		inv_i1 = inBody1.GetInverseInertia();
	}
	else
	{
		inv_m1 = 0.0f;
		inv_i1 = Mat44::sZero();
	}

	if (motion_type2 == EMotionType::Dynamic)
	{
		inv_m2 = motion_properties2->GetInverseMass();
		inv_i2 = inBody2.GetInverseInertia();
	}
	else
	{
		inv_m2 = 0.0f;
		inv_i2 = Mat44::sZero();
	}

	// Get center of masses relative to the base offset
	Vec3 com1 = Vec3(inBody1.GetCenterOfMassPosition() - inManifold.mBaseOffset);
	Vec3 com2 = Vec3(inBody2.GetCenterOfMassPosition() - inManifold.mBaseOffset);

	struct AxisConstraint
	{
		inline void		Initialize(Vec3Arg inR1, Vec3Arg inR2, Vec3Arg inWorldSpaceNormal, float inInvM1, float inInvM2, Mat44Arg inInvI1, Mat44Arg inInvI2)
		{
			// Calculate effective mass: K^-1 = (J M^-1 J^T)^-1
			mR1PlusUxAxis = inR1.Cross(inWorldSpaceNormal);
			mR2xAxis = inR2.Cross(inWorldSpaceNormal);
			mInvI1_R1PlusUxAxis = inInvI1.Multiply3x3(mR1PlusUxAxis);
			mInvI2_R2xAxis = inInvI2.Multiply3x3(mR2xAxis);
			mEffectiveMass = 1.0f / (inInvM1 + mInvI1_R1PlusUxAxis.Dot(mR1PlusUxAxis) + inInvM2 + mInvI2_R2xAxis.Dot(mR2xAxis));
			mBias = 0.0f;
		}

		inline float	SolveGetLambda(Vec3Arg inWorldSpaceNormal, const CollisionEstimationResult &inResult) const
		{
			// Calculate jacobian multiplied by linear/angular velocity
			float jv = inWorldSpaceNormal.Dot(inResult.mLinearVelocity1 - inResult.mLinearVelocity2) + mR1PlusUxAxis.Dot(inResult.mAngularVelocity1) - mR2xAxis.Dot(inResult.mAngularVelocity2);

			// Lagrange multiplier is:
			//
			// lambda = -K^-1 (J v + b)
			return mEffectiveMass * (jv - mBias);
		}

		inline void		SolveApplyLambda(Vec3Arg inWorldSpaceNormal, float inInvM1, float inInvM2, float inLambda, CollisionEstimationResult &ioResult) const
		{
			// Apply impulse to body velocities
			ioResult.mLinearVelocity1 -= (inLambda * inInvM1) * inWorldSpaceNormal;
			ioResult.mAngularVelocity1 -= inLambda * mInvI1_R1PlusUxAxis;
			ioResult.mLinearVelocity2 += (inLambda * inInvM2) * inWorldSpaceNormal;
			ioResult.mAngularVelocity2 += inLambda * mInvI2_R2xAxis;
		}

		inline void		Solve(Vec3Arg inWorldSpaceNormal, float inInvM1, float inInvM2, float inMinLambda, float inMaxLambda, float &ioTotalLambda, CollisionEstimationResult &ioResult) const
		{
			// Calculate new total lambda
			float total_lambda = ioTotalLambda + SolveGetLambda(inWorldSpaceNormal, ioResult);

			// Clamp impulse
			total_lambda = Clamp(total_lambda, inMinLambda, inMaxLambda);

			SolveApplyLambda(inWorldSpaceNormal, inInvM1, inInvM2, total_lambda - ioTotalLambda, ioResult);

			ioTotalLambda = total_lambda;
		}

		Vec3			mR1PlusUxAxis;
		Vec3			mR2xAxis;
		Vec3			mInvI1_R1PlusUxAxis;
		Vec3			mInvI2_R2xAxis;
		float			mEffectiveMass;
		float			mBias;
	};

	struct Constraint
	{
		AxisConstraint	mContact;
		AxisConstraint	mFriction1;
		AxisConstraint	mFriction2;
	};

	// Initialize the constraint properties
	Constraint constraints[ContactPoints::Capacity];
	for (uint c = 0; c < num_points; ++c)
	{
		Constraint &constraint = constraints[c];

		// Calculate contact points relative to body 1 and 2
		Vec3 p = 0.5f * (inManifold.mRelativeContactPointsOn1[c] + inManifold.mRelativeContactPointsOn2[c]);
		Vec3 r1 = p - com1;
		Vec3 r2 = p - com2;

		// Initialize contact constraint
		constraint.mContact.Initialize(r1, r2, inManifold.mWorldSpaceNormal, inv_m1, inv_m2, inv_i1, inv_i2);

		// Handle elastic collisions
		if (inCombinedRestitution > 0.0f)
		{
			// Calculate velocity of contact point
			Vec3 relative_velocity = outResult.mLinearVelocity2 + outResult.mAngularVelocity2.Cross(r2) - outResult.mLinearVelocity1 - outResult.mAngularVelocity1.Cross(r1);
			float normal_velocity = relative_velocity.Dot(inManifold.mWorldSpaceNormal);

			// If it is big enough, apply restitution
			if (normal_velocity < -inMinVelocityForRestitution)
				constraint.mContact.mBias = inCombinedRestitution * normal_velocity;
		}

		if (inCombinedFriction > 0.0f)
		{
			// Initialize friction constraints
			constraint.mFriction1.Initialize(r1, r2, outResult.mTangent1, inv_m1, inv_m2, inv_i1, inv_i2);
			constraint.mFriction2.Initialize(r1, r2, outResult.mTangent2, inv_m1, inv_m2, inv_i1, inv_i2);
		}
	}

	// If there's only 1 contact point, we only need 1 iteration
	int num_iterations = inCombinedFriction <= 0.0f && num_points == 1? 1 : inNumIterations;

	// Solve iteratively
	for (int iteration = 0; iteration < num_iterations; ++iteration)
	{
		// Solve friction constraints first
		if (inCombinedFriction > 0.0f && iteration > 0) // For first iteration the contact impulse is zero so there's no point in applying friction
			for (uint c = 0; c < num_points; ++c)
			{
				const Constraint &constraint = constraints[c];
				CollisionEstimationResult::Impulse &impulse = outResult.mImpulses[c];

				float lambda1 = impulse.mFrictionImpulse1 + constraint.mFriction1.SolveGetLambda(outResult.mTangent1, outResult);
				float lambda2 = impulse.mFrictionImpulse2 + constraint.mFriction2.SolveGetLambda(outResult.mTangent2, outResult);

				// Calculate max impulse based on contact impulse
				float max_impulse = inCombinedFriction * impulse.mContactImpulse;

				// If the total lambda that we will apply is too large, scale it back
				float total_lambda_sq = Square(lambda1) + Square(lambda2);
				if (total_lambda_sq > Square(max_impulse))
				{
					float scale = max_impulse / sqrt(total_lambda_sq);
					lambda1 *= scale;
					lambda2 *= scale;
				}

				constraint.mFriction1.SolveApplyLambda(outResult.mTangent1, inv_m1, inv_m2, lambda1 - impulse.mFrictionImpulse1, outResult);
				constraint.mFriction2.SolveApplyLambda(outResult.mTangent2, inv_m1, inv_m2, lambda2 - impulse.mFrictionImpulse2, outResult);

				impulse.mFrictionImpulse1 = lambda1;
				impulse.mFrictionImpulse2 = lambda2;
			}

		// Solve contact constraints last
		for (uint c = 0; c < num_points; ++c)
			constraints[c].mContact.Solve(inManifold.mWorldSpaceNormal, inv_m1, inv_m2, 0.0f, FLT_MAX, outResult.mImpulses[c].mContactImpulse, outResult);
	}
}

JPH_NAMESPACE_END
