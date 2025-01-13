// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Core/STLTempAllocator.h>

JPH_NAMESPACE_BEGIN

class CharacterVirtual;
class CollideShapeSettings;

/// Contains the configuration of a character
class JPH_EXPORT CharacterVirtualSettings : public CharacterBaseSettings
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Character mass (kg). Used to push down objects with gravity when the character is standing on top.
	float								mMass = 70.0f;

	/// Maximum force with which the character can push other bodies (N).
	float								mMaxStrength = 100.0f;

	/// An extra offset applied to the shape in local space. This allows applying an extra offset to the shape in local space.
	Vec3								mShapeOffset = Vec3::sZero();

	///@name Movement settings
	EBackFaceMode						mBackFaceMode = EBackFaceMode::CollideWithBackFaces;	///< When colliding with back faces, the character will not be able to move through back facing triangles. Use this if you have triangles that need to collide on both sides.
	float								mPredictiveContactDistance = 0.1f;						///< How far to scan outside of the shape for predictive contacts. A value of 0 will most likely cause the character to get stuck as it cannot properly calculate a sliding direction anymore. A value that's too high will cause ghost collisions.
	uint								mMaxCollisionIterations = 5;							///< Max amount of collision loops
	uint								mMaxConstraintIterations = 15;							///< How often to try stepping in the constraint solving
	float								mMinTimeRemaining = 1.0e-4f;							///< Early out condition: If this much time is left to simulate we are done
	float								mCollisionTolerance = 1.0e-3f;							///< How far we're willing to penetrate geometry
	float								mCharacterPadding = 0.02f;								///< How far we try to stay away from the geometry, this ensures that the sweep will hit as little as possible lowering the collision cost and reducing the risk of getting stuck
	uint								mMaxNumHits = 256;										///< Max num hits to collect in order to avoid excess of contact points collection
	float								mHitReductionCosMaxAngle = 0.999f;						///< Cos(angle) where angle is the maximum angle between two hits contact normals that are allowed to be merged during hit reduction. Default is around 2.5 degrees. Set to -1 to turn off.
	float								mPenetrationRecoverySpeed = 1.0f;						///< This value governs how fast a penetration will be resolved, 0 = nothing is resolved, 1 = everything in one update

	/// This character can optionally have an inner rigid body. This rigid body can be used to give the character presence in the world. When set it means that:
	/// - Regular collision checks (e.g. NarrowPhaseQuery::CastRay) will collide with the rigid body (they cannot collide with CharacterVirtual since it is not added to the broad phase)
	/// - Regular contact callbacks will be called through the ContactListener (next to the ones that will be passed to the CharacterContactListener)
	/// - Fast moving objects of motion quality LinearCast will not be able to pass through the CharacterVirtual in 1 time step
	RefConst<Shape>						mInnerBodyShape;

	/// Layer that the inner rigid body will be added to
	ObjectLayer							mInnerBodyLayer = 0;
};

/// This class contains settings that allow you to override the behavior of a character's collision response
class CharacterContactSettings
{
public:
	/// True when the object can push the virtual character.
	bool								mCanPushCharacter = true;

	/// True when the virtual character can apply impulses (push) the body.
	/// Note that this only works against rigid bodies. Other CharacterVirtual objects can only be moved in their own update,
	/// so you must ensure that in their OnCharacterContactAdded mCanPushCharacter is true.
	bool								mCanReceiveImpulses = true;
};

/// This class receives callbacks when a virtual character hits something.
class JPH_EXPORT CharacterContactListener
{
public:
	/// Destructor
	virtual								~CharacterContactListener() = default;

	/// Callback to adjust the velocity of a body as seen by the character. Can be adjusted to e.g. implement a conveyor belt or an inertial dampener system of a sci-fi space ship.
	/// Note that inBody2 is locked during the callback so you can read its properties freely.
	virtual void						OnAdjustBodyVelocity(const CharacterVirtual *inCharacter, const Body &inBody2, Vec3 &ioLinearVelocity, Vec3 &ioAngularVelocity) { /* Do nothing, the linear and angular velocity are already filled in */ }

	/// Checks if a character can collide with specified body. Return true if the contact is valid.
	virtual bool						OnContactValidate(const CharacterVirtual *inCharacter, const BodyID &inBodyID2, const SubShapeID &inSubShapeID2) { return true; }

	/// Same as OnContactValidate but when colliding with a CharacterVirtual
	virtual bool						OnCharacterContactValidate(const CharacterVirtual *inCharacter, const CharacterVirtual *inOtherCharacter, const SubShapeID &inSubShapeID2) { return true; }

	/// Called whenever the character collides with a body.
	/// @param inCharacter Character that is being solved
	/// @param inBodyID2 Body ID of body that is being hit
	/// @param inSubShapeID2 Sub shape ID of shape that is being hit
	/// @param inContactPosition World space contact position
	/// @param inContactNormal World space contact normal
	/// @param ioSettings Settings returned by the contact callback to indicate how the character should behave
	virtual void						OnContactAdded(const CharacterVirtual *inCharacter, const BodyID &inBodyID2, const SubShapeID &inSubShapeID2, RVec3Arg inContactPosition, Vec3Arg inContactNormal, CharacterContactSettings &ioSettings) { /* Default do nothing */ }

	/// Same as OnContactAdded but when colliding with a CharacterVirtual
	virtual void						OnCharacterContactAdded(const CharacterVirtual *inCharacter, const CharacterVirtual *inOtherCharacter, const SubShapeID &inSubShapeID2, RVec3Arg inContactPosition, Vec3Arg inContactNormal, CharacterContactSettings &ioSettings) { /* Default do nothing */ }

	/// Called whenever a contact is being used by the solver. Allows the listener to override the resulting character velocity (e.g. by preventing sliding along certain surfaces).
	/// @param inCharacter Character that is being solved
	/// @param inBodyID2 Body ID of body that is being hit
	/// @param inSubShapeID2 Sub shape ID of shape that is being hit
	/// @param inContactPosition World space contact position
	/// @param inContactNormal World space contact normal
	/// @param inContactVelocity World space velocity of contact point (e.g. for a moving platform)
	/// @param inContactMaterial Material of contact point
	/// @param inCharacterVelocity World space velocity of the character prior to hitting this contact
	/// @param ioNewCharacterVelocity Contains the calculated world space velocity of the character after hitting this contact, this velocity slides along the surface of the contact. Can be modified by the listener to provide an alternative velocity.
	virtual void						OnContactSolve(const CharacterVirtual *inCharacter, const BodyID &inBodyID2, const SubShapeID &inSubShapeID2, RVec3Arg inContactPosition, Vec3Arg inContactNormal, Vec3Arg inContactVelocity, const PhysicsMaterial *inContactMaterial, Vec3Arg inCharacterVelocity, Vec3 &ioNewCharacterVelocity) { /* Default do nothing */ }

	/// Same as OnContactSolve but when colliding with a CharacterVirtual
	virtual void						OnCharacterContactSolve(const CharacterVirtual *inCharacter, const CharacterVirtual *inOtherCharacter, const SubShapeID &inSubShapeID2, RVec3Arg inContactPosition, Vec3Arg inContactNormal, Vec3Arg inContactVelocity, const PhysicsMaterial *inContactMaterial, Vec3Arg inCharacterVelocity, Vec3 &ioNewCharacterVelocity) { /* Default do nothing */ }
};

/// Interface class that allows a CharacterVirtual to check collision with other CharacterVirtual instances.
/// Since CharacterVirtual instances are not registered anywhere, it is up to the application to test collision against relevant characters.
/// The characters could be stored in a tree structure to make this more efficient.
class JPH_EXPORT CharacterVsCharacterCollision : public NonCopyable
{
public:
	virtual								~CharacterVsCharacterCollision() = default;

	/// Collide a character against other CharacterVirtuals.
	/// @param inCharacter The character to collide.
	/// @param inCenterOfMassTransform Center of mass transform for this character.
	/// @param inCollideShapeSettings Settings for the collision check.
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. GetPosition() since floats are most accurate near the origin
	/// @param ioCollector Collision collector that receives the collision results.
	virtual void						CollideCharacter(const CharacterVirtual *inCharacter, RMat44Arg inCenterOfMassTransform, const CollideShapeSettings &inCollideShapeSettings, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector) const = 0;

	/// Cast a character against other CharacterVirtuals.
	/// @param inCharacter The character to cast.
	/// @param inCenterOfMassTransform Center of mass transform for this character.
	/// @param inDirection Direction and length to cast in.
	/// @param inShapeCastSettings Settings for the shape cast.
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. GetPosition() since floats are most accurate near the origin
	/// @param ioCollector Collision collector that receives the collision results.
	virtual void						CastCharacter(const CharacterVirtual *inCharacter, RMat44Arg inCenterOfMassTransform, Vec3Arg inDirection, const ShapeCastSettings &inShapeCastSettings, RVec3Arg inBaseOffset, CastShapeCollector &ioCollector) const = 0;
};

/// Simple collision checker that loops over all registered characters.
/// Note that this is not thread safe, so make sure that only one CharacterVirtual is checking collision at a time.
class JPH_EXPORT CharacterVsCharacterCollisionSimple : public CharacterVsCharacterCollision
{
public:
	/// Add a character to the list of characters to check collision against.
	void								Add(CharacterVirtual *inCharacter)						{ mCharacters.push_back(inCharacter); }

	/// Remove a character from the list of characters to check collision against.
	void								Remove(const CharacterVirtual *inCharacter);

	// See: CharacterVsCharacterCollision
	virtual void						CollideCharacter(const CharacterVirtual *inCharacter, RMat44Arg inCenterOfMassTransform, const CollideShapeSettings &inCollideShapeSettings, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector) const override;
	virtual void						CastCharacter(const CharacterVirtual *inCharacter, RMat44Arg inCenterOfMassTransform, Vec3Arg inDirection, const ShapeCastSettings &inShapeCastSettings, RVec3Arg inBaseOffset, CastShapeCollector &ioCollector) const override;

	Array<CharacterVirtual *>			mCharacters;											///< The list of characters to check collision against
};

/// Runtime character object.
/// This object usually represents the player. Contrary to the Character class it doesn't use a rigid body but moves doing collision checks only (hence the name virtual).
/// The advantage of this is that you can determine when the character moves in the frame (usually this has to happen at a very particular point in the frame)
/// but the downside is that other objects don't see this virtual character. In order to make this work it is recommended to pair a CharacterVirtual with a Character that
/// moves along. This Character should be keyframed (or at least have no gravity) and move along with the CharacterVirtual so that other rigid bodies can collide with it.
class JPH_EXPORT CharacterVirtual : public CharacterBase
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	/// @param inSettings The settings for the character
	/// @param inPosition Initial position for the character
	/// @param inRotation Initial rotation for the character (usually only around the up-axis)
	/// @param inUserData Application specific value
	/// @param inSystem Physics system that this character will be added to
										CharacterVirtual(const CharacterVirtualSettings *inSettings, RVec3Arg inPosition, QuatArg inRotation, uint64 inUserData, PhysicsSystem *inSystem);

	/// Constructor without user data
										CharacterVirtual(const CharacterVirtualSettings *inSettings, RVec3Arg inPosition, QuatArg inRotation, PhysicsSystem *inSystem) : CharacterVirtual(inSettings, inPosition, inRotation, 0, inSystem) { }

	/// Destructor
	virtual								~CharacterVirtual() override;

	/// Set the contact listener
	void								SetListener(CharacterContactListener *inListener)		{ mListener = inListener; }

	/// Get the current contact listener
	CharacterContactListener *			GetListener() const										{ return mListener; }

	/// Set the character vs character collision interface
	void								SetCharacterVsCharacterCollision(CharacterVsCharacterCollision *inCharacterVsCharacterCollision) { mCharacterVsCharacterCollision = inCharacterVsCharacterCollision; }

	/// Get the linear velocity of the character (m / s)
	Vec3								GetLinearVelocity() const								{ return mLinearVelocity; }

	/// Set the linear velocity of the character (m / s)
	void								SetLinearVelocity(Vec3Arg inLinearVelocity)				{ mLinearVelocity = inLinearVelocity; }

	/// Get the position of the character
	RVec3								GetPosition() const										{ return mPosition; }

	/// Set the position of the character
	void								SetPosition(RVec3Arg inPosition)						{ mPosition = inPosition; UpdateInnerBodyTransform(); }

	/// Get the rotation of the character
	Quat								GetRotation() const										{ return mRotation; }

	/// Set the rotation of the character
	void								SetRotation(QuatArg inRotation)							{ mRotation = inRotation; UpdateInnerBodyTransform(); }

	// Get the center of mass position of the shape
	inline RVec3						GetCenterOfMassPosition() const							{ return mPosition + (mRotation * (mShapeOffset + mShape->GetCenterOfMass()) + mCharacterPadding * mUp); }

	/// Calculate the world transform of the character
	RMat44								GetWorldTransform() const								{ return RMat44::sRotationTranslation(mRotation, mPosition); }

	/// Calculates the transform for this character's center of mass
	RMat44								GetCenterOfMassTransform() const						{ return GetCenterOfMassTransform(mPosition, mRotation, mShape); }

	/// Character mass (kg)
	float								GetMass() const											{ return mMass; }
	void								SetMass(float inMass)									{ mMass = inMass; }

	/// Maximum force with which the character can push other bodies (N)
	float								GetMaxStrength() const									{ return mMaxStrength; }
	void								SetMaxStrength(float inMaxStrength)						{ mMaxStrength = inMaxStrength; }

	/// This value governs how fast a penetration will be resolved, 0 = nothing is resolved, 1 = everything in one update
	float								GetPenetrationRecoverySpeed() const						{ return mPenetrationRecoverySpeed; }
	void								SetPenetrationRecoverySpeed(float inSpeed)				{ mPenetrationRecoverySpeed = inSpeed; }

	/// Set to indicate that extra effort should be made to try to remove ghost contacts (collisions with internal edges of a mesh). This is more expensive but makes bodies move smoother over a mesh with convex edges.
	bool								GetEnhancedInternalEdgeRemoval() const					{ return mEnhancedInternalEdgeRemoval; }
	void								SetEnhancedInternalEdgeRemoval(bool inApply)			{ mEnhancedInternalEdgeRemoval = inApply; }

	/// Character padding
	float								GetCharacterPadding() const								{ return mCharacterPadding; }

	/// Max num hits to collect in order to avoid excess of contact points collection
	uint								GetMaxNumHits() const									{ return mMaxNumHits; }
	void								SetMaxNumHits(uint inMaxHits)							{ mMaxNumHits = inMaxHits; }

	/// Cos(angle) where angle is the maximum angle between two hits contact normals that are allowed to be merged during hit reduction. Default is around 2.5 degrees. Set to -1 to turn off.
	float								GetHitReductionCosMaxAngle() const						{ return mHitReductionCosMaxAngle; }
	void								SetHitReductionCosMaxAngle(float inCosMaxAngle)			{ mHitReductionCosMaxAngle = inCosMaxAngle; }

	/// Returns if we exceeded the maximum number of hits during the last collision check and had to discard hits based on distance.
	/// This can be used to find areas that have too complex geometry for the character to navigate properly.
	/// To solve you can either increase the max number of hits or simplify the geometry. Note that the character simulation will
	/// try to do its best to select the most relevant contacts to avoid the character from getting stuck.
	bool								GetMaxHitsExceeded() const								{ return mMaxHitsExceeded; }

	/// An extra offset applied to the shape in local space. This allows applying an extra offset to the shape in local space. Note that setting it on the fly can cause the shape to teleport into collision.
	Vec3								GetShapeOffset() const									{ return mShapeOffset; }
	void								SetShapeOffset(Vec3Arg inShapeOffset)					{ mShapeOffset = inShapeOffset; UpdateInnerBodyTransform(); }

	/// Access to the user data, can be used for anything by the application
	uint64								GetUserData() const										{ return mUserData; }
	void								SetUserData(uint64 inUserData);

	/// Optional inner rigid body that proxies the character in the world. Can be used to update body properties.
	BodyID								GetInnerBodyID() const									{ return mInnerBodyID; }

	/// This function can be called prior to calling Update() to convert a desired velocity into a velocity that won't make the character move further onto steep slopes.
	/// This velocity can then be set on the character using SetLinearVelocity()
	/// @param inDesiredVelocity Velocity to clamp against steep walls
	/// @return A new velocity vector that won't make the character move up steep slopes
	Vec3								CancelVelocityTowardsSteepSlopes(Vec3Arg inDesiredVelocity) const;

	/// This is the main update function. It moves the character according to its current velocity (the character is similar to a kinematic body in the sense
	/// that you set the velocity and the character will follow unless collision is blocking the way). Note it's your own responsibility to apply gravity to the character velocity!
	/// Different surface materials (like ice) can be emulated by getting the ground material and adjusting the velocity and/or the max slope angle accordingly every frame.
	/// @param inDeltaTime Time step to simulate.
	/// @param inGravity Gravity vector (m/s^2). This gravity vector is only used when the character is standing on top of another object to apply downward force.
	/// @param inBroadPhaseLayerFilter Filter that is used to check if the character collides with something in the broadphase.
	/// @param inObjectLayerFilter Filter that is used to check if a character collides with a layer.
	/// @param inBodyFilter Filter that is used to check if a character collides with a body.
	/// @param inShapeFilter Filter that is used to check if a character collides with a subshape.
	/// @param inAllocator An allocator for temporary allocations. All memory will be freed by the time this function returns.
	void								Update(float inDeltaTime, Vec3Arg inGravity, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator);

	/// This function will return true if the character has moved into a slope that is too steep (e.g. a vertical wall).
	/// You would call WalkStairs to attempt to step up stairs.
	/// @param inLinearVelocity The linear velocity that the player desired. This is used to determine if we're pushing into a step.
	bool								CanWalkStairs(Vec3Arg inLinearVelocity) const;

	/// When stair walking is needed, you can call the WalkStairs function to cast up, forward and down again to try to find a valid position
	/// @param inDeltaTime Time step to simulate.
	/// @param inStepUp The direction and distance to step up (this corresponds to the max step height)
	/// @param inStepForward The direction and distance to step forward after the step up
	/// @param inStepForwardTest When running at a high frequency, inStepForward can be very small and it's likely that you hit the side of the stairs on the way down. This could produce a normal that violates the max slope angle. If this happens, we test again using this distance from the up position to see if we find a valid slope.
	/// @param inStepDownExtra An additional translation that is added when stepping down at the end. Allows you to step further down than up. Set to zero if you don't want this. Should be in the opposite direction of up.
	/// @param inBroadPhaseLayerFilter Filter that is used to check if the character collides with something in the broadphase.
	/// @param inObjectLayerFilter Filter that is used to check if a character collides with a layer.
	/// @param inBodyFilter Filter that is used to check if a character collides with a body.
	/// @param inShapeFilter Filter that is used to check if a character collides with a subshape.
	/// @param inAllocator An allocator for temporary allocations. All memory will be freed by the time this function returns.
	/// @return true if the stair walk was successful
	bool								WalkStairs(float inDeltaTime, Vec3Arg inStepUp, Vec3Arg inStepForward, Vec3Arg inStepForwardTest, Vec3Arg inStepDownExtra, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator);

	/// This function can be used to artificially keep the character to the floor. Normally when a character is on a small step and starts moving horizontally, the character will
	/// lose contact with the floor because the initial vertical velocity is zero while the horizontal velocity is quite high. To prevent the character from losing contact with the floor,
	/// we do an additional collision check downwards and if we find the floor within a certain distance, we project the character onto the floor.
	/// @param inStepDown Max amount to project the character downwards (if no floor is found within this distance, the function will return false)
	/// @param inBroadPhaseLayerFilter Filter that is used to check if the character collides with something in the broadphase.
	/// @param inObjectLayerFilter Filter that is used to check if a character collides with a layer.
	/// @param inBodyFilter Filter that is used to check if a character collides with a body.
	/// @param inShapeFilter Filter that is used to check if a character collides with a subshape.
	/// @param inAllocator An allocator for temporary allocations. All memory will be freed by the time this function returns.
	/// @return True if the character was successfully projected onto the floor.
	bool								StickToFloor(Vec3Arg inStepDown, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator);

	/// Settings struct with settings for ExtendedUpdate
	struct ExtendedUpdateSettings
	{
		Vec3							mStickToFloorStepDown { 0, -0.5f, 0 };									///< See StickToFloor inStepDown parameter. Can be zero to turn off.
		Vec3							mWalkStairsStepUp { 0, 0.4f, 0 };										///< See WalkStairs inStepUp parameter. Can be zero to turn off.
		float							mWalkStairsMinStepForward { 0.02f };									///< See WalkStairs inStepForward parameter. Note that the parameter only indicates a magnitude, direction is taken from current velocity.
		float							mWalkStairsStepForwardTest { 0.15f };									///< See WalkStairs inStepForwardTest parameter. Note that the parameter only indicates a magnitude, direction is taken from current velocity.
		float							mWalkStairsCosAngleForwardContact { Cos(DegreesToRadians(75.0f)) };		///< Cos(angle) where angle is the maximum angle between the ground normal in the horizontal plane and the character forward vector where we're willing to adjust the step forward test towards the contact normal.
		Vec3							mWalkStairsStepDownExtra { Vec3::sZero() };								///< See WalkStairs inStepDownExtra
	};

	/// This function combines Update, StickToFloor and WalkStairs. This function serves as an example of how these functions could be combined.
	/// Before calling, call SetLinearVelocity to update the horizontal/vertical speed of the character, typically this is:
	/// - When on OnGround and not moving away from ground: velocity = GetGroundVelocity() + horizontal speed as input by player + optional vertical jump velocity + delta time * gravity
	/// - Else: velocity = current vertical velocity + horizontal speed as input by player + delta time * gravity
	/// @param inDeltaTime Time step to simulate.
	/// @param inGravity Gravity vector (m/s^2). This gravity vector is only used when the character is standing on top of another object to apply downward force.
	/// @param inSettings A structure containing settings for the algorithm.
	/// @param inBroadPhaseLayerFilter Filter that is used to check if the character collides with something in the broadphase.
	/// @param inObjectLayerFilter Filter that is used to check if a character collides with a layer.
	/// @param inBodyFilter Filter that is used to check if a character collides with a body.
	/// @param inShapeFilter Filter that is used to check if a character collides with a subshape.
	/// @param inAllocator An allocator for temporary allocations. All memory will be freed by the time this function returns.
	void								ExtendedUpdate(float inDeltaTime, Vec3Arg inGravity, const ExtendedUpdateSettings &inSettings, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator);

	/// This function can be used after a character has teleported to determine the new contacts with the world.
	void								RefreshContacts(const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator);

	/// Use the ground body ID to get an updated estimate of the ground velocity. This function can be used if the ground body has moved / changed velocity and you want a new estimate of the ground velocity.
	/// It will not perform collision detection, so is less accurate than RefreshContacts but a lot faster.
	void								UpdateGroundVelocity();

	/// Switch the shape of the character (e.g. for stance).
	/// @param inShape The shape to switch to.
	/// @param inMaxPenetrationDepth When inMaxPenetrationDepth is not FLT_MAX, it checks if the new shape collides before switching shape. This is the max penetration we're willing to accept after the switch.
	/// @param inBroadPhaseLayerFilter Filter that is used to check if the character collides with something in the broadphase.
	/// @param inObjectLayerFilter Filter that is used to check if a character collides with a layer.
	/// @param inBodyFilter Filter that is used to check if a character collides with a body.
	/// @param inShapeFilter Filter that is used to check if a character collides with a subshape.
	/// @param inAllocator An allocator for temporary allocations. All memory will be freed by the time this function returns.
	/// @return Returns true if the switch succeeded.
	bool								SetShape(const Shape *inShape, float inMaxPenetrationDepth, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator);

	/// Updates the shape of the inner rigid body. Should be called after a successful call to SetShape.
	void								SetInnerBodyShape(const Shape *inShape);

	/// Get the transformed shape that represents the volume of the character, can be used for collision checks.
	TransformedShape					GetTransformedShape() const								{ return TransformedShape(GetCenterOfMassPosition(), mRotation, mShape, mInnerBodyID); }

	/// @brief Get all contacts for the character at a particular location.
	/// When colliding with another character virtual, this pointer will be provided through CollideShapeCollector::SetUserContext before adding a hit.
	/// @param inPosition Position to test, note that this position will be corrected for the character padding.
	/// @param inRotation Rotation at which to test the shape.
	/// @param inMovementDirection A hint in which direction the character is moving, will be used to calculate a proper normal.
	/// @param inMaxSeparationDistance How much distance around the character you want to report contacts in (can be 0 to match the character exactly).
	/// @param inShape Shape to test collision with.
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. GetPosition() since floats are most accurate near the origin
	/// @param ioCollector Collision collector that receives the collision results.
	/// @param inBroadPhaseLayerFilter Filter that is used to check if the character collides with something in the broadphase.
	/// @param inObjectLayerFilter Filter that is used to check if a character collides with a layer.
	/// @param inBodyFilter Filter that is used to check if a character collides with a body.
	/// @param inShapeFilter Filter that is used to check if a character collides with a subshape.
	void								CheckCollision(RVec3Arg inPosition, QuatArg inRotation, Vec3Arg inMovementDirection, float inMaxSeparationDistance, const Shape *inShape, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter) const;

	// Saving / restoring state for replay
	virtual void						SaveState(StateRecorder &inStream) const override;
	virtual void						RestoreState(StateRecorder &inStream) override;

#ifdef JPH_DEBUG_RENDERER
	static inline bool					sDrawConstraints = false;								///< Draw the current state of the constraints for iteration 0 when creating them
	static inline bool					sDrawWalkStairs = false;								///< Draw the state of the walk stairs algorithm
	static inline bool					sDrawStickToFloor = false;								///< Draw the state of the stick to floor algorithm
#endif

	// Encapsulates a collision contact
	struct Contact
	{
		// Saving / restoring state for replay
		void							SaveState(StateRecorder &inStream) const;
		void							RestoreState(StateRecorder &inStream);

		// Checks if two contacts refer to the same body (or virtual character)
		inline bool						IsSameBody(const Contact &inOther) const				{ return mBodyB == inOther.mBodyB && mCharacterB == inOther.mCharacterB; }

		RVec3							mPosition;												///< Position where the character makes contact
		Vec3							mLinearVelocity;										///< Velocity of the contact point
		Vec3							mContactNormal;											///< Contact normal, pointing towards the character
		Vec3							mSurfaceNormal;											///< Surface normal of the contact
		float							mDistance;												///< Distance to the contact <= 0 means that it is an actual contact, > 0 means predictive
		float							mFraction;												///< Fraction along the path where this contact takes place
		BodyID							mBodyB;													///< ID of body we're colliding with (if not invalid)
		CharacterVirtual *				mCharacterB = nullptr;									///< Character we're colliding with (if not null)
		SubShapeID						mSubShapeIDB;											///< Sub shape ID of body we're colliding with
		EMotionType						mMotionTypeB;											///< Motion type of B, used to determine the priority of the contact
		bool							mIsSensorB;												///< If B is a sensor
		uint64							mUserData;												///< User data of B
		const PhysicsMaterial *			mMaterial;												///< Material of B
		bool							mHadCollision = false;									///< If the character actually collided with the contact (can be false if a predictive contact never becomes a real one)
		bool							mWasDiscarded = false;									///< If the contact validate callback chose to discard this contact
		bool							mCanPushCharacter = true;								///< When true, the velocity of the contact point can push the character
	};

	using TempContactList = Array<Contact, STLTempAllocator<Contact>>;
	using ContactList = Array<Contact>;

	/// Access to the internal list of contacts that the character has found.
	const ContactList &					GetActiveContacts() const								{ return mActiveContacts; }

	/// Check if the character is currently in contact with or has collided with another body in the last time step
	bool								HasCollidedWith(const BodyID &inBody) const
	{
		for (const CharacterVirtual::Contact &c : mActiveContacts)
			if (c.mHadCollision && c.mBodyB == inBody)
				return true;
		return false;
	}

	/// Check if the character is currently in contact with or has collided with another character in the last time step
	bool								HasCollidedWith(const CharacterVirtual *inCharacter) const
	{
		for (const CharacterVirtual::Contact &c : mActiveContacts)
			if (c.mHadCollision && c.mCharacterB == inCharacter)
				return true;
		return false;
	}

private:
	// Sorting predicate for making contact order deterministic
	struct ContactOrderingPredicate
	{
		inline bool						operator () (const Contact &inLHS, const Contact &inRHS) const
		{
			if (inLHS.mBodyB != inRHS.mBodyB)
				return inLHS.mBodyB < inRHS.mBodyB;

			return inLHS.mSubShapeIDB.GetValue() < inRHS.mSubShapeIDB.GetValue();
		}
	};

	// A contact that needs to be ignored
	struct IgnoredContact
	{
										IgnoredContact() = default;
										IgnoredContact(const BodyID &inBodyID, const SubShapeID &inSubShapeID) : mBodyID(inBodyID), mSubShapeID(inSubShapeID) { }

		BodyID							mBodyID;												///< ID of body we're colliding with
		SubShapeID						mSubShapeID;											///< Sub shape of body we're colliding with
	};

	using IgnoredContactList = Array<IgnoredContact, STLTempAllocator<IgnoredContact>>;

	// A constraint that limits the movement of the character
	struct Constraint
	{
		Contact *						mContact;												///< Contact that this constraint was generated from
		float							mTOI;													///< Calculated time of impact (can be negative if penetrating)
		float							mProjectedVelocity;										///< Velocity of the contact projected on the contact normal (negative if separating)
		Vec3							mLinearVelocity;										///< Velocity of the contact (can contain a corrective velocity to resolve penetration)
		Plane							mPlane;													///< Plane around the origin that describes how far we can displace (from the origin)
		bool							mIsSteepSlope = false;									///< If this constraint belongs to a steep slope
	};

	using ConstraintList = Array<Constraint, STLTempAllocator<Constraint>>;

	// Collision collector that collects hits for CollideShape
	class ContactCollector : public CollideShapeCollector
	{
	public:
										ContactCollector(PhysicsSystem *inSystem, const CharacterVirtual *inCharacter, uint inMaxHits, float inHitReductionCosMaxAngle, Vec3Arg inUp, RVec3Arg inBaseOffset, TempContactList &outContacts) : mBaseOffset(inBaseOffset), mUp(inUp), mSystem(inSystem), mCharacter(inCharacter), mContacts(outContacts), mMaxHits(inMaxHits), mHitReductionCosMaxAngle(inHitReductionCosMaxAngle) { }

		virtual void					SetUserData(uint64 inUserData) override					{ mOtherCharacter = reinterpret_cast<CharacterVirtual *>(inUserData); }

		virtual void					AddHit(const CollideShapeResult &inResult) override;

		RVec3							mBaseOffset;
		Vec3							mUp;
		PhysicsSystem *					mSystem;
		const CharacterVirtual *		mCharacter;
		CharacterVirtual *				mOtherCharacter = nullptr;
		TempContactList &				mContacts;
		uint							mMaxHits;
		float							mHitReductionCosMaxAngle;
		bool							mMaxHitsExceeded = false;
	};

	// A collision collector that collects hits for CastShape
	class ContactCastCollector : public CastShapeCollector
	{
	public:
										ContactCastCollector(PhysicsSystem *inSystem, const CharacterVirtual *inCharacter, Vec3Arg inDisplacement, Vec3Arg inUp, const IgnoredContactList &inIgnoredContacts, RVec3Arg inBaseOffset, Contact &outContact) : mBaseOffset(inBaseOffset), mDisplacement(inDisplacement), mUp(inUp), mSystem(inSystem), mCharacter(inCharacter), mIgnoredContacts(inIgnoredContacts), mContact(outContact) { }

		virtual void					SetUserData(uint64 inUserData) override					{ mOtherCharacter = reinterpret_cast<CharacterVirtual *>(inUserData); }

		virtual void					AddHit(const ShapeCastResult &inResult) override;

		RVec3							mBaseOffset;
		Vec3							mDisplacement;
		Vec3							mUp;
		PhysicsSystem *					mSystem;
		const CharacterVirtual *		mCharacter;
		CharacterVirtual *				mOtherCharacter = nullptr;
		const IgnoredContactList &		mIgnoredContacts;
		Contact &						mContact;
	};

	// Helper function to convert a Jolt collision result into a contact
	template <class taCollector>
	inline static void					sFillContactProperties(const CharacterVirtual *inCharacter, Contact &outContact, const Body &inBody, Vec3Arg inUp, RVec3Arg inBaseOffset, const taCollector &inCollector, const CollideShapeResult &inResult);
	inline static void					sFillCharacterContactProperties(Contact &outContact, CharacterVirtual *inOtherCharacter, RVec3Arg inBaseOffset, const CollideShapeResult &inResult);

	// Move the shape from ioPosition and try to displace it by inVelocity * inDeltaTime, this will try to slide the shape along the world geometry
	void								MoveShape(RVec3 &ioPosition, Vec3Arg inVelocity, float inDeltaTime, ContactList *outActiveContacts, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator
	#ifdef JPH_DEBUG_RENDERER
		, bool inDrawConstraints = false
	#endif // JPH_DEBUG_RENDERER
		) const;

	// Ask the callback if inContact is a valid contact point
	bool								ValidateContact(const Contact &inContact) const;

	// Trigger the contact callback for inContact and get the contact settings
	void								ContactAdded(const Contact &inContact, CharacterContactSettings &ioSettings) const;

	// Tests the shape for collision around inPosition
	void								GetContactsAtPosition(RVec3Arg inPosition, Vec3Arg inMovementDirection, const Shape *inShape, TempContactList &outContacts, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter) const;

	// Remove penetrating contacts with the same body that have conflicting normals, leaving these will make the character mover get stuck
	void								RemoveConflictingContacts(TempContactList &ioContacts, IgnoredContactList &outIgnoredContacts) const;

	// Convert contacts into constraints. The character is assumed to start at the origin and the constraints are planes around the origin that confine the movement of the character.
	void								DetermineConstraints(TempContactList &inContacts, float inDeltaTime, ConstraintList &outConstraints) const;

	// Use the constraints to solve the displacement of the character. This will slide the character on the planes around the origin for as far as possible.
	void								SolveConstraints(Vec3Arg inVelocity, float inDeltaTime, float inTimeRemaining, ConstraintList &ioConstraints, IgnoredContactList &ioIgnoredContacts, float &outTimeSimulated, Vec3 &outDisplacement, TempAllocator &inAllocator
	#ifdef JPH_DEBUG_RENDERER
		, bool inDrawConstraints = false
	#endif // JPH_DEBUG_RENDERER
		) const;

	// Get the velocity of a body adjusted by the contact listener
	void								GetAdjustedBodyVelocity(const Body& inBody, Vec3 &outLinearVelocity, Vec3 &outAngularVelocity) const;

	// Calculate the ground velocity of the character assuming it's standing on an object with specified linear and angular velocity and with specified center of mass.
	// Note that we don't just take the point velocity because a point on an object with angular velocity traces an arc,
	// so if you just take point velocity * delta time you get an error that accumulates over time
	Vec3								CalculateCharacterGroundVelocity(RVec3Arg inCenterOfMass, Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity, float inDeltaTime) const;

	// Handle contact with physics object that we're colliding against
	bool								HandleContact(Vec3Arg inVelocity, Constraint &ioConstraint, float inDeltaTime) const;

	// Does a swept test of the shape from inPosition with displacement inDisplacement, returns true if there was a collision
	bool								GetFirstContactForSweep(RVec3Arg inPosition, Vec3Arg inDisplacement, Contact &outContact, const IgnoredContactList &inIgnoredContacts, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter) const;

	// Store contacts so that we have proper ground information
	void								StoreActiveContacts(const TempContactList &inContacts, TempAllocator &inAllocator);

	// This function will determine which contacts are touching the character and will calculate the one that is supporting us
	void								UpdateSupportingContact(bool inSkipContactVelocityCheck, TempAllocator &inAllocator);

	/// This function can be called after moving the character to a new colliding position
	void								MoveToContact(RVec3Arg inPosition, const Contact &inContact, const BroadPhaseLayerFilter &inBroadPhaseLayerFilter, const ObjectLayerFilter &inObjectLayerFilter, const BodyFilter &inBodyFilter, const ShapeFilter &inShapeFilter, TempAllocator &inAllocator);

	// This function returns the actual center of mass of the shape, not corrected for the character padding
	inline RMat44						GetCenterOfMassTransform(RVec3Arg inPosition, QuatArg inRotation, const Shape *inShape) const
	{
		return RMat44::sRotationTranslation(inRotation, inPosition).PreTranslated(mShapeOffset + inShape->GetCenterOfMass()).PostTranslated(mCharacterPadding * mUp);
	}

	// This function returns the position of the inner rigid body
	inline RVec3						GetInnerBodyPosition() const
	{
		return mPosition + (mRotation * mShapeOffset + mCharacterPadding * mUp);
	}

	// Move the inner rigid body to the current position
	void								UpdateInnerBodyTransform();

	// Our main listener for contacts
	CharacterContactListener *			mListener = nullptr;

	// Interface to detect collision between characters
	CharacterVsCharacterCollision *		mCharacterVsCharacterCollision = nullptr;

	// Movement settings
	EBackFaceMode						mBackFaceMode;											// When colliding with back faces, the character will not be able to move through back facing triangles. Use this if you have triangles that need to collide on both sides.
	float								mPredictiveContactDistance;								// How far to scan outside of the shape for predictive contacts. A value of 0 will most likely cause the character to get stuck as it cannot properly calculate a sliding direction anymore. A value that's too high will cause ghost collisions.
	uint								mMaxCollisionIterations;								// Max amount of collision loops
	uint								mMaxConstraintIterations;								// How often to try stepping in the constraint solving
	float								mMinTimeRemaining;										// Early out condition: If this much time is left to simulate we are done
	float								mCollisionTolerance;									// How far we're willing to penetrate geometry
	float								mCharacterPadding;										// How far we try to stay away from the geometry, this ensures that the sweep will hit as little as possible lowering the collision cost and reducing the risk of getting stuck
	uint								mMaxNumHits;											// Max num hits to collect in order to avoid excess of contact points collection
	float								mHitReductionCosMaxAngle;								// Cos(angle) where angle is the maximum angle between two hits contact normals that are allowed to be merged during hit reduction. Default is around 2.5 degrees. Set to -1 to turn off.
	float								mPenetrationRecoverySpeed;								// This value governs how fast a penetration will be resolved, 0 = nothing is resolved, 1 = everything in one update
	bool								mEnhancedInternalEdgeRemoval;							// Set to indicate that extra effort should be made to try to remove ghost contacts (collisions with internal edges of a mesh). This is more expensive but makes bodies move smoother over a mesh with convex edges.

	// Character mass (kg)
	float								mMass;

	// Maximum force with which the character can push other bodies (N)
	float								mMaxStrength;

	// An extra offset applied to the shape in local space. This allows applying an extra offset to the shape in local space.
	Vec3								mShapeOffset = Vec3::sZero();

	// Current position (of the base, not the center of mass)
	RVec3								mPosition = RVec3::sZero();

	// Current rotation (of the base, not of the center of mass)
	Quat								mRotation = Quat::sIdentity();

	// Current linear velocity
	Vec3								mLinearVelocity = Vec3::sZero();

	// List of contacts that were active in the last frame
	ContactList							mActiveContacts;

	// Remembers the delta time of the last update
	float								mLastDeltaTime = 1.0f / 60.0f;

	// Remember if we exceeded the maximum number of hits and had to remove similar contacts
	mutable bool						mMaxHitsExceeded = false;

	// User data, can be used for anything by the application
	uint64								mUserData = 0;

	// The inner rigid body that proxies the character in the world
	BodyID								mInnerBodyID;
};

JPH_NAMESPACE_END
