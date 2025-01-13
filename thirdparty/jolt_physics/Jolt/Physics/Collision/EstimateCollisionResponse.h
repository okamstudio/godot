// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/ContactListener.h>

JPH_NAMESPACE_BEGIN

/// A structure that contains the estimated contact and friction impulses and the resulting body velocities
struct CollisionEstimationResult
{
	Vec3			mLinearVelocity1;				///< The estimated linear velocity of body 1 after collision
	Vec3			mAngularVelocity1;				///< The estimated angular velocity of body 1 after collision
	Vec3			mLinearVelocity2;				///< The estimated linear velocity of body 2 after collision
	Vec3			mAngularVelocity2;				///< The estimated angular velocity of body 2 after collision

	Vec3			mTangent1;						///< Normalized tangent of contact normal
	Vec3			mTangent2;						///< Second normalized tangent of contact normal (forms a basis with mTangent1 and mWorldSpaceNormal)

	struct Impulse
	{
		float		mContactImpulse;				///< Estimated contact impulses (kg m / s)
		float		mFrictionImpulse1;				///< Estimated friction impulses in the direction of tangent 1 (kg m / s)
		float		mFrictionImpulse2;				///< Estimated friction impulses in the direction of tangent 2 (kg m / s)
	};

	using Impulses = StaticArray<Impulse, ContactPoints::Capacity>;

	Impulses		mImpulses;
};

/// This function estimates the contact impulses and body velocity changes as a result of a collision.
/// It can be used in the ContactListener::OnContactAdded to determine the strength of the collision to e.g. play a sound or trigger a particle system.
/// This function is accurate when two bodies collide but will not be accurate when more than 2 bodies collide at the same time as it does not know about these other collisions.
///
/// @param inBody1 Colliding body 1
/// @param inBody2 Colliding body 2
/// @param inManifold The collision manifold
/// @param outResult A structure that contains the estimated contact and friction impulses and the resulting body velocities
/// @param inCombinedFriction The combined friction of body 1 and body 2 (see ContactSettings::mCombinedFriction)
/// @param inCombinedRestitution The combined restitution of body 1 and body 2 (see ContactSettings::mCombinedRestitution)
/// @param inMinVelocityForRestitution Minimal velocity required for restitution to be applied (see PhysicsSettings::mMinVelocityForRestitution)
/// @param inNumIterations Number of iterations to use for the impulse estimation (see PhysicsSettings::mNumVelocitySteps, note you can probably use a lower number for a decent estimate). If you set the number of iterations to 1 then no friction will be calculated.
JPH_EXPORT void EstimateCollisionResponse(const Body &inBody1, const Body &inBody2, const ContactManifold &inManifold, CollisionEstimationResult &outResult, float inCombinedFriction, float inCombinedRestitution, float inMinVelocityForRestitution = 1.0f, uint inNumIterations = 10);

JPH_NAMESPACE_END
