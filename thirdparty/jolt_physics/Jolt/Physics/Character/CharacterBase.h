// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Reference.h>
#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>

JPH_NAMESPACE_BEGIN

class PhysicsSystem;
class StateRecorder;

/// Base class for configuration of a character
class JPH_EXPORT CharacterBaseSettings : public RefTarget<CharacterBaseSettings>
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
										CharacterBaseSettings() = default;
										CharacterBaseSettings(const CharacterBaseSettings &inSettings) = default;
	CharacterBaseSettings &				operator = (const CharacterBaseSettings &inSettings) = default;

	/// Virtual destructor
	virtual								~CharacterBaseSettings() = default;

	/// Vector indicating the up direction of the character
	Vec3								mUp = Vec3::sAxisY();

	/// Plane, defined in local space relative to the character. Every contact behind this plane can support the
	/// character, every contact in front of this plane is treated as only colliding with the player.
	/// Default: Accept any contact.
	Plane								mSupportingVolume { Vec3::sAxisY(), -1.0e10f };

	/// Maximum angle of slope that character can still walk on (radians).
	float								mMaxSlopeAngle = DegreesToRadians(50.0f);

	/// Set to indicate that extra effort should be made to try to remove ghost contacts (collisions with internal edges of a mesh). This is more expensive but makes bodies move smoother over a mesh with convex edges.
	bool								mEnhancedInternalEdgeRemoval = false;

	/// Initial shape that represents the character's volume.
	/// Usually this is a capsule, make sure the shape is made so that the bottom of the shape is at (0, 0, 0).
	RefConst<Shape>						mShape;
};

/// Base class for character class
class JPH_EXPORT CharacterBase : public RefTarget<CharacterBase>, public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
										CharacterBase(const CharacterBaseSettings *inSettings, PhysicsSystem *inSystem);

	/// Destructor
	virtual								~CharacterBase() = default;

	/// Set the maximum angle of slope that character can still walk on (radians)
	void								SetMaxSlopeAngle(float inMaxSlopeAngle)					{ mCosMaxSlopeAngle = Cos(inMaxSlopeAngle); }
	float								GetCosMaxSlopeAngle() const								{ return mCosMaxSlopeAngle; }

	/// Set the up vector for the character
	void								SetUp(Vec3Arg inUp)										{ mUp = inUp; }
	Vec3								GetUp() const											{ return mUp; }

	/// Check if the normal of the ground surface is too steep to walk on
	bool								IsSlopeTooSteep(Vec3Arg inNormal) const
	{
		// If cos max slope angle is close to one the system is turned off,
		// otherwise check the angle between the up and normal vector
		return mCosMaxSlopeAngle < cNoMaxSlopeAngle && inNormal.Dot(mUp) < mCosMaxSlopeAngle;
	}

	/// Get the current shape that the character is using.
	const Shape *						GetShape() const										{ return mShape; }

	enum class EGroundState
	{
		OnGround,						///< Character is on the ground and can move freely.
		OnSteepGround,					///< Character is on a slope that is too steep and can't climb up any further. The caller should start applying downward velocity if sliding from the slope is desired.
		NotSupported,					///< Character is touching an object, but is not supported by it and should fall. The GetGroundXXX functions will return information about the touched object.
		InAir,							///< Character is in the air and is not touching anything.
	};

	/// Debug function to convert enum values to string
	static const char *					sToString(EGroundState inState);

	///@name Properties of the ground this character is standing on

	/// Current ground state
	EGroundState						GetGroundState() const									{ return mGroundState; }

	/// Returns true if the player is supported by normal or steep ground
	bool								IsSupported() const										{ return mGroundState == EGroundState::OnGround || mGroundState == EGroundState::OnSteepGround; }

	/// Get the contact point with the ground
	RVec3								GetGroundPosition() const								{ return mGroundPosition; }

	/// Get the contact normal with the ground
	Vec3								GetGroundNormal() const									{ return mGroundNormal; }

	/// Velocity in world space of ground
	Vec3								GetGroundVelocity() const								{ return mGroundVelocity; }

	/// Material that the character is standing on
	const PhysicsMaterial *				GetGroundMaterial() const								{ return mGroundMaterial; }

	/// BodyID of the object the character is standing on. Note may have been removed!
	BodyID								GetGroundBodyID() const									{ return mGroundBodyID; }

	/// Sub part of the body that we're standing on.
	SubShapeID							GetGroundSubShapeID() const								{ return mGroundBodySubShapeID; }

	/// User data value of the body that we're standing on
	uint64								GetGroundUserData() const								{ return mGroundUserData; }

	// Saving / restoring state for replay
	virtual void						SaveState(StateRecorder &inStream) const;
	virtual void						RestoreState(StateRecorder &inStream);

protected:
	// Cached physics system
	PhysicsSystem *						mSystem;

	// The shape that the body currently has
	RefConst<Shape>						mShape;

	// The character's world space up axis
	Vec3								mUp;

	// Every contact behind this plane can support the character
	Plane								mSupportingVolume;

	// Beyond this value there is no max slope
	static constexpr float				cNoMaxSlopeAngle = 0.9999f;

	// Cosine of the maximum angle of slope that character can still walk on
	float								mCosMaxSlopeAngle;

	// Ground properties
	EGroundState						mGroundState = EGroundState::InAir;
	BodyID								mGroundBodyID;
	SubShapeID							mGroundBodySubShapeID;
	RVec3								mGroundPosition = RVec3::sZero();
	Vec3								mGroundNormal = Vec3::sZero();
	Vec3								mGroundVelocity = Vec3::sZero();
	RefConst<PhysicsMaterial>			mGroundMaterial = PhysicsMaterial::sDefault;
	uint64								mGroundUserData = 0;
};

JPH_NAMESPACE_END
