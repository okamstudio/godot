// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/EActivation.h>

JPH_NAMESPACE_BEGIN

/// Contains the configuration of a character
class JPH_EXPORT CharacterSettings : public CharacterBaseSettings
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Layer that this character will be added to
	ObjectLayer							mLayer = 0;

	/// Mass of the character
	float								mMass = 80.0f;

	/// Friction for the character
	float								mFriction = 0.2f;

	/// Value to multiply gravity with for this character
	float								mGravityFactor = 1.0f;
};

/// Runtime character object.
/// This object usually represents the player or a humanoid AI. It uses a single rigid body,
/// usually with a capsule shape to simulate movement and collision for the character.
/// The character is a keyframed object, the application controls it by setting the velocity.
class JPH_EXPORT Character : public CharacterBase
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	/// @param inSettings The settings for the character
	/// @param inPosition Initial position for the character
	/// @param inRotation Initial rotation for the character (usually only around Y)
	/// @param inUserData Application specific value
	/// @param inSystem Physics system that this character will be added to later
										Character(const CharacterSettings *inSettings, RVec3Arg inPosition, QuatArg inRotation, uint64 inUserData, PhysicsSystem *inSystem);

	/// Destructor
	virtual								~Character() override;

	/// Add bodies and constraints to the system and optionally activate the bodies
	void								AddToPhysicsSystem(EActivation inActivationMode = EActivation::Activate, bool inLockBodies = true);

	/// Remove bodies and constraints from the system
	void								RemoveFromPhysicsSystem(bool inLockBodies = true);

	/// Wake up the character
	void								Activate(bool inLockBodies = true);

	/// Needs to be called after every PhysicsSystem::Update
	/// @param inMaxSeparationDistance Max distance between the floor and the character to still consider the character standing on the floor
	/// @param inLockBodies If the collision query should use the locking body interface (true) or the non locking body interface (false)
	void								PostSimulation(float inMaxSeparationDistance, bool inLockBodies = true);

	/// Control the velocity of the character
	void								SetLinearAndAngularVelocity(Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity, bool inLockBodies = true);

	/// Get the linear velocity of the character (m / s)
	Vec3								GetLinearVelocity(bool inLockBodies = true) const;

	/// Set the linear velocity of the character (m / s)
	void								SetLinearVelocity(Vec3Arg inLinearVelocity, bool inLockBodies = true);

	/// Add world space linear velocity to current velocity (m / s)
	void								AddLinearVelocity(Vec3Arg inLinearVelocity, bool inLockBodies = true);

	/// Add impulse to the center of mass of the character
	void								AddImpulse(Vec3Arg inImpulse, bool inLockBodies = true);

	/// Get the body associated with this character
	BodyID								GetBodyID() const										{ return mBodyID; }

	/// Get position / rotation of the body
	void								GetPositionAndRotation(RVec3 &outPosition, Quat &outRotation, bool inLockBodies = true) const;

	/// Set the position / rotation of the body, optionally activating it.
	void								SetPositionAndRotation(RVec3Arg inPosition, QuatArg inRotation, EActivation inActivationMode = EActivation::Activate, bool inLockBodies = true) const;

	/// Get the position of the character
	RVec3								GetPosition(bool inLockBodies = true) const;

	/// Set the position of the character, optionally activating it.
	void								SetPosition(RVec3Arg inPosition, EActivation inActivationMode = EActivation::Activate, bool inLockBodies = true);

	/// Get the rotation of the character
	Quat								GetRotation(bool inLockBodies = true) const;

	/// Set the rotation of the character, optionally activating it.
	void								SetRotation(QuatArg inRotation, EActivation inActivationMode = EActivation::Activate, bool inLockBodies = true);

	/// Position of the center of mass of the underlying rigid body
	RVec3								GetCenterOfMassPosition(bool inLockBodies = true) const;

	/// Calculate the world transform of the character
	RMat44								GetWorldTransform(bool inLockBodies = true) const;

	/// Get the layer of the character
	ObjectLayer							GetLayer() const										{ return mLayer; }

	/// Update the layer of the character
	void								SetLayer(ObjectLayer inLayer, bool inLockBodies = true);

	/// Switch the shape of the character (e.g. for stance). When inMaxPenetrationDepth is not FLT_MAX, it checks
	/// if the new shape collides before switching shape. Returns true if the switch succeeded.
	bool								SetShape(const Shape *inShape, float inMaxPenetrationDepth, bool inLockBodies = true);

	/// Get the transformed shape that represents the volume of the character, can be used for collision checks.
	TransformedShape					GetTransformedShape(bool inLockBodies = true) const;

	/// @brief Get all contacts for the character at a particular location
	/// @param inPosition Position to test.
	/// @param inRotation Rotation at which to test the shape.
	/// @param inMovementDirection A hint in which direction the character is moving, will be used to calculate a proper normal.
	/// @param inMaxSeparationDistance How much distance around the character you want to report contacts in (can be 0 to match the character exactly).
	/// @param inShape Shape to test collision with.
	/// @param inBaseOffset All hit results will be returned relative to this offset, can be zero to get results in world position, but when you're testing far from the origin you get better precision by picking a position that's closer e.g. GetPosition() since floats are most accurate near the origin
	/// @param ioCollector Collision collector that receives the collision results.
	/// @param inLockBodies If the collision query should use the locking body interface (true) or the non locking body interface (false)
	void								CheckCollision(RVec3Arg inPosition, QuatArg inRotation, Vec3Arg inMovementDirection, float inMaxSeparationDistance, const Shape *inShape, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector, bool inLockBodies = true) const;

private:
	/// Check collisions between inShape and the world using the center of mass transform
	void								CheckCollision(RMat44Arg inCenterOfMassTransform, Vec3Arg inMovementDirection, float inMaxSeparationDistance, const Shape *inShape, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector, bool inLockBodies) const;

	/// Check collisions between inShape and the world using the current position / rotation of the character
	void								CheckCollision(const Shape *inShape, float inMaxSeparationDistance, RVec3Arg inBaseOffset, CollideShapeCollector &ioCollector, bool inLockBodies) const;

	/// The body of this character
	BodyID								mBodyID;

	/// The layer the body is in
	ObjectLayer							mLayer;
};

JPH_NAMESPACE_END
