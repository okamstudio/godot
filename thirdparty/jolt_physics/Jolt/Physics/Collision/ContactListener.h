// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Collision/Shape/SubShapeIDPair.h>
#include <Jolt/Core/StaticArray.h>

JPH_NAMESPACE_BEGIN

class Body;
class CollideShapeResult;

/// Array of contact points
using ContactPoints = StaticArray<Vec3, 64>;

/// Manifold class, describes the contact surface between two bodies
class ContactManifold
{
public:
	/// Swaps shape 1 and 2
	ContactManifold			SwapShapes() const					{ return { mBaseOffset, -mWorldSpaceNormal, mPenetrationDepth, mSubShapeID2, mSubShapeID1, mRelativeContactPointsOn2, mRelativeContactPointsOn1 }; }

	/// Access to the world space contact positions
	inline RVec3			GetWorldSpaceContactPointOn1(uint inIndex) const { return mBaseOffset + mRelativeContactPointsOn1[inIndex]; }
	inline RVec3			GetWorldSpaceContactPointOn2(uint inIndex) const { return mBaseOffset + mRelativeContactPointsOn2[inIndex]; }

	RVec3					mBaseOffset;						///< Offset to which all the contact points are relative
	Vec3					mWorldSpaceNormal;					///< Normal for this manifold, direction along which to move body 2 out of collision along the shortest path
	float					mPenetrationDepth;					///< Penetration depth (move shape 2 by this distance to resolve the collision). If this value is negative, this is a speculative contact point and may not actually result in a velocity change as during solving the bodies may not actually collide.
	SubShapeID				mSubShapeID1;						///< Sub shapes that formed this manifold (note that when multiple manifolds are combined because they're coplanar, we lose some information here because we only keep track of one sub shape pair that we encounter, see description at Body::SetUseManifoldReduction)
	SubShapeID				mSubShapeID2;
	ContactPoints			mRelativeContactPointsOn1;			///< Contact points on the surface of shape 1 relative to mBaseOffset.
	ContactPoints			mRelativeContactPointsOn2;			///< Contact points on the surface of shape 2 relative to mBaseOffset. If there's no penetration, this will be the same as mRelativeContactPointsOn1. If there is penetration they will be different.
};

/// When a contact point is added or persisted, the callback gets a chance to override certain properties of the contact constraint.
/// The values are filled in with their defaults by the system so the callback doesn't need to modify anything, but it can if it wants to.
class ContactSettings
{
public:
	float					mCombinedFriction;					///< Combined friction for the body pair (see: PhysicsSystem::SetCombineFriction)
	float					mCombinedRestitution;				///< Combined restitution for the body pair (see: PhysicsSystem::SetCombineRestitution)
	float					mInvMassScale1 = 1.0f;				///< Scale factor for the inverse mass of body 1 (0 = infinite mass, 1 = use original mass, 2 = body has half the mass). For the same contact pair, you should strive to keep the value the same over time.
	float					mInvInertiaScale1 = 1.0f;			///< Scale factor for the inverse inertia of body 1 (usually same as mInvMassScale1)
	float					mInvMassScale2 = 1.0f;				///< Scale factor for the inverse mass of body 2 (0 = infinite mass, 1 = use original mass, 2 = body has half the mass). For the same contact pair, you should strive to keep the value the same over time.
	float					mInvInertiaScale2 = 1.0f;			///< Scale factor for the inverse inertia of body 2 (usually same as mInvMassScale2)
	bool					mIsSensor;							///< If the contact should be treated as a sensor vs body contact (no collision response)
	Vec3					mRelativeLinearSurfaceVelocity = Vec3::sZero(); ///< Relative linear surface velocity between the bodies (world space surface velocity of body 2 - world space surface velocity of body 1), can be used to create a conveyor belt effect
	Vec3					mRelativeAngularSurfaceVelocity = Vec3::sZero(); ///< Relative angular surface velocity between the bodies (world space angular surface velocity of body 2 - world space angular surface velocity of body 1). Note that this angular velocity is relative to the center of mass of body 1, so if you want it relative to body 2's center of mass you need to add body 2 angular velocity x (body 1 world space center of mass - body 2 world space center of mass) to mRelativeLinearSurfaceVelocity.
};

/// Return value for the OnContactValidate callback. Determines if the contact is being processed or not.
/// Results are ordered so that the strongest accept has the lowest number and the strongest reject the highest number (which allows for easy combining of results)
enum class ValidateResult
{
	AcceptAllContactsForThisBodyPair,							///< Accept this and any further contact points for this body pair
	AcceptContact,												///< Accept this contact only (and continue calling this callback for every contact manifold for the same body pair)
	RejectContact,												///< Reject this contact only (but process any other contact manifolds for the same body pair)
	RejectAllContactsForThisBodyPair							///< Rejects this and any further contact points for this body pair
};

/// A listener class that receives collision contact events.
/// It can be registered with the ContactConstraintManager (or PhysicsSystem).
/// Note that contact listener callbacks are called from multiple threads at the same time when all bodies are locked, you're only allowed to read from the bodies and you can't change physics state.
/// During OnContactRemoved you cannot access the bodies at all, see the comments at that function.
class ContactListener
{
public:
	/// Ensure virtual destructor
	virtual					~ContactListener() = default;

	/// Called after detecting a collision between a body pair, but before calling OnContactAdded and before adding the contact constraint.
	/// If the function rejects the contact, the contact will not be added and any other contacts between this body pair will not be processed.
	/// This function will only be called once per PhysicsSystem::Update per body pair and may not be called again the next update
	/// if a contact persists and no new contact pairs between sub shapes are found.
	/// This is a rather expensive time to reject a contact point since a lot of the collision detection has happened already, make sure you
	/// filter out the majority of undesired body pairs through the ObjectLayerPairFilter that is registered on the PhysicsSystem.
	/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
	/// Body 1 will have a motion type that is larger or equal than body 2's motion type (order from large to small: dynamic -> kinematic -> static). When motion types are equal, they are ordered by BodyID.
	/// The collision result (inCollisionResult) is reported relative to inBaseOffset.
	virtual ValidateResult	OnContactValidate([[maybe_unused]] const Body &inBody1, [[maybe_unused]] const Body &inBody2, [[maybe_unused]] RVec3Arg inBaseOffset, [[maybe_unused]] const CollideShapeResult &inCollisionResult) { return ValidateResult::AcceptAllContactsForThisBodyPair; }

	/// Called whenever a new contact point is detected.
	/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
	/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
	/// Note that only active bodies will report contacts, as soon as a body goes to sleep the contacts between that body and all other
	/// bodies will receive an OnContactRemoved callback, if this is the case then Body::IsActive() will return false during the callback.
	/// When contacts are added, the constraint solver has not run yet, so the collision impulse is unknown at that point.
	/// The velocities of inBody1 and inBody2 are the velocities before the contact has been resolved, so you can use this to
	/// estimate the collision impulse to e.g. determine the volume of the impact sound to play (see: EstimateCollisionResponse).
	virtual void			OnContactAdded([[maybe_unused]] const Body &inBody1, [[maybe_unused]] const Body &inBody2, [[maybe_unused]] const ContactManifold &inManifold, [[maybe_unused]] ContactSettings &ioSettings) { /* Do nothing */ }

	/// Called whenever a contact is detected that was also detected last update.
	/// Note that this callback is called when all bodies are locked, so don't use any locking functions!
	/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
	/// If the structure of the shape of a body changes between simulation steps (e.g. by adding/removing a child shape of a compound shape),
	/// it is possible that the same sub shape ID used to identify the removed child shape is now reused for a different child shape. The physics
	/// system cannot detect this, so may send a 'contact persisted' callback even though the contact is now on a different child shape. You can
	/// detect this by keeping the old shape (before adding/removing a part) around until the next PhysicsSystem::Update (when the OnContactPersisted
	/// callbacks are triggered) and resolving the sub shape ID against both the old and new shape to see if they still refer to the same child shape.
	virtual void			OnContactPersisted([[maybe_unused]] const Body &inBody1, [[maybe_unused]] const Body &inBody2, [[maybe_unused]] const ContactManifold &inManifold, [[maybe_unused]] ContactSettings &ioSettings) { /* Do nothing */ }

	/// Called whenever a contact was detected last update but is not detected anymore.
	/// You cannot access the bodies at the time of this callback because:
	/// - All bodies are locked at the time of this callback.
	/// - Some properties of the bodies are being modified from another thread at the same time.
	/// - The body may have been removed and destroyed (you'll receive an OnContactRemoved callback in the PhysicsSystem::Update after the body has been removed).
	/// Cache what you need in the OnContactAdded and OnContactPersisted callbacks and store it in a separate structure to use during this callback.
	/// Alternatively, you could just record that the contact was removed and process it after PhysicsSimulation::Update.
	/// Body 1 and 2 will be sorted such that body 1 ID < body 2 ID, so body 1 may not be dynamic.
	/// The sub shape ID were created in the previous simulation step too, so if the structure of a shape changes (e.g. by adding/removing a child shape of a compound shape),
	/// the sub shape ID may not be valid / may not point to the same sub shape anymore.
	/// If you want to know if this is the last contact between the two bodies, use PhysicsSystem::WereBodiesInContact.
	virtual void			OnContactRemoved([[maybe_unused]] const SubShapeIDPair &inSubShapePair) { /* Do nothing */ }
};

JPH_NAMESPACE_END
