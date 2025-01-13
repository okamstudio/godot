// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

CharacterBase::CharacterBase(const CharacterBaseSettings *inSettings, PhysicsSystem *inSystem) :
	mSystem(inSystem),
	mShape(inSettings->mShape),
	mUp(inSettings->mUp),
	mSupportingVolume(inSettings->mSupportingVolume)
{
	// Initialize max slope angle
	SetMaxSlopeAngle(inSettings->mMaxSlopeAngle);
}

const char *CharacterBase::sToString(EGroundState inState)
{
	switch (inState)
	{
	case EGroundState::OnGround:		return "OnGround";
	case EGroundState::OnSteepGround:	return "OnSteepGround";
	case EGroundState::NotSupported:	return "NotSupported";
	case EGroundState::InAir:			return "InAir";
	}

	JPH_ASSERT(false);
	return "Unknown";
}

void CharacterBase::SaveState(StateRecorder &inStream) const
{
	inStream.Write(mGroundState);
	inStream.Write(mGroundBodyID);
	inStream.Write(mGroundBodySubShapeID);
	inStream.Write(mGroundPosition);
	inStream.Write(mGroundNormal);
	inStream.Write(mGroundVelocity);
	// Can't save user data (may be a pointer) and material
}

void CharacterBase::RestoreState(StateRecorder &inStream)
{
	inStream.Read(mGroundState);
	inStream.Read(mGroundBodyID);
	inStream.Read(mGroundBodySubShapeID);
	inStream.Read(mGroundPosition);
	inStream.Read(mGroundNormal);
	inStream.Read(mGroundVelocity);
	mGroundUserData = 0; // Cannot restore user data
	mGroundMaterial = PhysicsMaterial::sDefault; // Cannot restore material
}

JPH_NAMESPACE_END
