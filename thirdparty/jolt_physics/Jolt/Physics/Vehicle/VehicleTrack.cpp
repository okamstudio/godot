// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Vehicle/VehicleTrack.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(VehicleTrackSettings)
{
	JPH_ADD_ATTRIBUTE(VehicleTrackSettings, mDrivenWheel)
	JPH_ADD_ATTRIBUTE(VehicleTrackSettings, mWheels)
	JPH_ADD_ATTRIBUTE(VehicleTrackSettings, mInertia)
	JPH_ADD_ATTRIBUTE(VehicleTrackSettings, mAngularDamping)
	JPH_ADD_ATTRIBUTE(VehicleTrackSettings, mMaxBrakeTorque)
	JPH_ADD_ATTRIBUTE(VehicleTrackSettings, mDifferentialRatio)
}

void VehicleTrackSettings::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mDrivenWheel);
	inStream.Write(mWheels);
	inStream.Write(mInertia);
	inStream.Write(mAngularDamping);
	inStream.Write(mMaxBrakeTorque);
	inStream.Write(mDifferentialRatio);
}

void VehicleTrackSettings::RestoreBinaryState(StreamIn &inStream)
{
	inStream.Read(mDrivenWheel);
	inStream.Read(mWheels);
	inStream.Read(mInertia);
	inStream.Read(mAngularDamping);
	inStream.Read(mMaxBrakeTorque);
	inStream.Read(mDifferentialRatio);
}

void VehicleTrack::SaveState(StateRecorder &inStream) const
{
	inStream.Write(mAngularVelocity);
}

void VehicleTrack::RestoreState(StateRecorder &inStream)
{
	inStream.Read(mAngularVelocity);
}

JPH_NAMESPACE_END
