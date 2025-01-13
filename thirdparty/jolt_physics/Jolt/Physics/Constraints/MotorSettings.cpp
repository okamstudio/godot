// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(MotorSettings)
{
	JPH_ADD_ENUM_ATTRIBUTE_WITH_ALIAS(MotorSettings, mSpringSettings.mMode, "mSpringMode")
	JPH_ADD_ATTRIBUTE_WITH_ALIAS(MotorSettings, mSpringSettings.mFrequency, "mFrequency") // Renaming attributes to stay compatible with old versions of the library
	JPH_ADD_ATTRIBUTE_WITH_ALIAS(MotorSettings, mSpringSettings.mDamping, "mDamping")
	JPH_ADD_ATTRIBUTE(MotorSettings, mMinForceLimit)
	JPH_ADD_ATTRIBUTE(MotorSettings, mMaxForceLimit)
	JPH_ADD_ATTRIBUTE(MotorSettings, mMinTorqueLimit)
	JPH_ADD_ATTRIBUTE(MotorSettings, mMaxTorqueLimit)
}

void MotorSettings::SaveBinaryState(StreamOut &inStream) const
{
	mSpringSettings.SaveBinaryState(inStream);
	inStream.Write(mMinForceLimit);
	inStream.Write(mMaxForceLimit);
	inStream.Write(mMinTorqueLimit);
	inStream.Write(mMaxTorqueLimit);
}

void MotorSettings::RestoreBinaryState(StreamIn &inStream)
{
	mSpringSettings.RestoreBinaryState(inStream);
	inStream.Read(mMinForceLimit);
	inStream.Read(mMaxForceLimit);
	inStream.Read(mMinTorqueLimit);
	inStream.Read(mMaxTorqueLimit);
}

JPH_NAMESPACE_END
