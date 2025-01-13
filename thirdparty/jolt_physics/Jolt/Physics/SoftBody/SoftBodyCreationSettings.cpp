// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(SoftBodyCreationSettings)
{
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mSettings)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mPosition)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mRotation)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mUserData)
	JPH_ADD_ENUM_ATTRIBUTE(SoftBodyCreationSettings, mObjectLayer)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mCollisionGroup)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mNumIterations)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mLinearDamping)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mMaxLinearVelocity)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mRestitution)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mFriction)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mPressure)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mGravityFactor)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mUpdatePosition)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mMakeRotationIdentity)
	JPH_ADD_ATTRIBUTE(SoftBodyCreationSettings, mAllowSleeping)
}

void SoftBodyCreationSettings::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mPosition);
	inStream.Write(mRotation);
	inStream.Write(mUserData);
	inStream.Write(mObjectLayer);
	mCollisionGroup.SaveBinaryState(inStream);
	inStream.Write(mNumIterations);
	inStream.Write(mLinearDamping);
	inStream.Write(mMaxLinearVelocity);
	inStream.Write(mRestitution);
	inStream.Write(mFriction);
	inStream.Write(mPressure);
	inStream.Write(mGravityFactor);
	inStream.Write(mUpdatePosition);
	inStream.Write(mMakeRotationIdentity);
	inStream.Write(mAllowSleeping);
}

void SoftBodyCreationSettings::RestoreBinaryState(StreamIn &inStream)
{
	inStream.Read(mPosition);
	inStream.Read(mRotation);
	inStream.Read(mUserData);
	inStream.Read(mObjectLayer);
	mCollisionGroup.RestoreBinaryState(inStream);
	inStream.Read(mNumIterations);
	inStream.Read(mLinearDamping);
	inStream.Read(mMaxLinearVelocity);
	inStream.Read(mRestitution);
	inStream.Read(mFriction);
	inStream.Read(mPressure);
	inStream.Read(mGravityFactor);
	inStream.Read(mUpdatePosition);
	inStream.Read(mMakeRotationIdentity);
	inStream.Read(mAllowSleeping);
}

void SoftBodyCreationSettings::SaveWithChildren(StreamOut &inStream, SharedSettingsToIDMap *ioSharedSettingsMap, MaterialToIDMap *ioMaterialMap, GroupFilterToIDMap *ioGroupFilterMap) const
{
	// Save creation settings
	SaveBinaryState(inStream);

	// Save shared settings
	if (ioSharedSettingsMap != nullptr && ioMaterialMap != nullptr)
		mSettings->SaveWithMaterials(inStream, *ioSharedSettingsMap, *ioMaterialMap);
	else
		inStream.Write(~uint32(0));

	// Save group filter
	StreamUtils::SaveObjectReference(inStream, mCollisionGroup.GetGroupFilter(), ioGroupFilterMap);
}

SoftBodyCreationSettings::SBCSResult SoftBodyCreationSettings::sRestoreWithChildren(StreamIn &inStream, IDToSharedSettingsMap &ioSharedSettingsMap, IDToMaterialMap &ioMaterialMap, IDToGroupFilterMap &ioGroupFilterMap)
{
	SBCSResult result;

	// Read creation settings
	SoftBodyCreationSettings settings;
	settings.RestoreBinaryState(inStream);
	if (inStream.IsEOF() || inStream.IsFailed())
	{
		result.SetError("Error reading body creation settings");
		return result;
	}

	// Read shared settings
	SoftBodySharedSettings::SettingsResult settings_result = SoftBodySharedSettings::sRestoreWithMaterials(inStream, ioSharedSettingsMap, ioMaterialMap);
	if (settings_result.HasError())
	{
		result.SetError(settings_result.GetError());
		return result;
	}
	settings.mSettings = settings_result.Get();

	// Read group filter
	Result gfresult = StreamUtils::RestoreObjectReference(inStream, ioGroupFilterMap);
	if (gfresult.HasError())
	{
		result.SetError(gfresult.GetError());
		return result;
	}
	settings.mCollisionGroup.SetGroupFilter(gfresult.Get());

	result.Set(settings);
	return result;
}

JPH_NAMESPACE_END
