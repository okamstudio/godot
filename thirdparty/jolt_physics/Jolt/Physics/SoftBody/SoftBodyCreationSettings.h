// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/ObjectStream/SerializableObject.h>
#include <Jolt/Core/StreamUtils.h>

JPH_NAMESPACE_BEGIN

/// This class contains the information needed to create a soft body object
/// Note: Soft bodies are still in development and come with several caveats. Read the Architecture and API documentation for more information!
class JPH_EXPORT SoftBodyCreationSettings
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, SoftBodyCreationSettings)

public:
	/// Constructor
						SoftBodyCreationSettings() = default;
						SoftBodyCreationSettings(const SoftBodySharedSettings *inSettings, RVec3Arg inPosition, QuatArg inRotation, ObjectLayer inObjectLayer) : mSettings(inSettings), mPosition(inPosition), mRotation(inRotation), mObjectLayer(inObjectLayer) { }

	/// Saves the state of this object in binary form to inStream. Doesn't store the shared settings nor the group filter.
	void				SaveBinaryState(StreamOut &inStream) const;

	/// Restore the state of this object from inStream. Doesn't restore the shared settings nor the group filter.
	void				RestoreBinaryState(StreamIn &inStream);

	using GroupFilterToIDMap = StreamUtils::ObjectToIDMap<GroupFilter>;
	using IDToGroupFilterMap = StreamUtils::IDToObjectMap<GroupFilter>;
	using SharedSettingsToIDMap = SoftBodySharedSettings::SharedSettingsToIDMap;
	using IDToSharedSettingsMap = SoftBodySharedSettings::IDToSharedSettingsMap;
	using MaterialToIDMap = StreamUtils::ObjectToIDMap<PhysicsMaterial>;
	using IDToMaterialMap = StreamUtils::IDToObjectMap<PhysicsMaterial>;

	/// Save this body creation settings, its shared settings and group filter. Pass in an empty map in ioSharedSettingsMap / ioMaterialMap / ioGroupFilterMap or reuse the same map while saving multiple shapes to the same stream in order to avoid writing duplicates.
	/// Pass nullptr to ioSharedSettingsMap and ioMaterial map to skip saving shared settings and materials
	/// Pass nullptr to ioGroupFilterMap to skip saving group filters
	void				SaveWithChildren(StreamOut &inStream, SharedSettingsToIDMap *ioSharedSettingsMap, MaterialToIDMap *ioMaterialMap, GroupFilterToIDMap *ioGroupFilterMap) const;

	using SBCSResult = Result<SoftBodyCreationSettings>;

	/// Restore a shape, all its children and materials. Pass in an empty map in ioSharedSettingsMap / ioMaterialMap / ioGroupFilterMap or reuse the same map while reading multiple shapes from the same stream in order to restore duplicates.
	static SBCSResult	sRestoreWithChildren(StreamIn &inStream, IDToSharedSettingsMap &ioSharedSettingsMap, IDToMaterialMap &ioMaterialMap, IDToGroupFilterMap &ioGroupFilterMap);

	RefConst<SoftBodySharedSettings> mSettings;				///< Defines the configuration of this soft body

	RVec3				mPosition { RVec3::sZero() };		///< Initial position of the soft body
	Quat				mRotation { Quat::sIdentity() };	///< Initial rotation of the soft body

	/// User data value (can be used by application)
	uint64				mUserData = 0;

	///@name Collision settings
	ObjectLayer			mObjectLayer = 0;					///< The collision layer this body belongs to (determines if two objects can collide)
	CollisionGroup		mCollisionGroup;					///< The collision group this body belongs to (determines if two objects can collide)

	uint32				mNumIterations = 5;					///< Number of solver iterations
	float				mLinearDamping = 0.1f;				///< Linear damping: dv/dt = -mLinearDamping * v
	float				mMaxLinearVelocity = 500.0f;		///< Maximum linear velocity that a vertex can reach (m/s)
	float				mRestitution = 0.0f;				///< Restitution when colliding
	float				mFriction = 0.2f;					///< Friction coefficient when colliding
	float				mPressure = 0.0f;					///< n * R * T, amount of substance * ideal gas constant * absolute temperature, see https://en.wikipedia.org/wiki/Pressure
	float				mGravityFactor = 1.0f;				///< Value to multiply gravity with for this body
	bool				mUpdatePosition = true;				///< Update the position of the body while simulating (set to false for something that is attached to the static world)
	bool				mMakeRotationIdentity = true;		///< Bake specified mRotation in the vertices and set the body rotation to identity (simulation is slightly more accurate if the rotation of a soft body is kept to identity)
	bool				mAllowSleeping = true;				///< If this body can go to sleep or not
};

JPH_NAMESPACE_END
