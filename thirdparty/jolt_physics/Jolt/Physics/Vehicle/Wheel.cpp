// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Vehicle/Wheel.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(WheelSettings)
{
	JPH_ADD_ATTRIBUTE(WheelSettings, mSuspensionForcePoint)
	JPH_ADD_ATTRIBUTE(WheelSettings, mPosition)
	JPH_ADD_ATTRIBUTE(WheelSettings, mSuspensionDirection)
	JPH_ADD_ATTRIBUTE(WheelSettings, mSteeringAxis)
	JPH_ADD_ATTRIBUTE(WheelSettings, mWheelForward)
	JPH_ADD_ATTRIBUTE(WheelSettings, mWheelUp)
	JPH_ADD_ATTRIBUTE(WheelSettings, mSuspensionMinLength)
	JPH_ADD_ATTRIBUTE(WheelSettings, mSuspensionMaxLength)
	JPH_ADD_ATTRIBUTE(WheelSettings, mSuspensionPreloadLength)
	JPH_ADD_ENUM_ATTRIBUTE_WITH_ALIAS(WheelSettings, mSuspensionSpring.mMode, "mSuspensionSpringMode")
	JPH_ADD_ATTRIBUTE_WITH_ALIAS(WheelSettings, mSuspensionSpring.mFrequency, "mSuspensionFrequency") // Renaming attributes to stay compatible with old versions of the library
	JPH_ADD_ATTRIBUTE_WITH_ALIAS(WheelSettings, mSuspensionSpring.mDamping, "mSuspensionDamping")
	JPH_ADD_ATTRIBUTE(WheelSettings, mRadius)
	JPH_ADD_ATTRIBUTE(WheelSettings, mWidth)
	JPH_ADD_ATTRIBUTE(WheelSettings, mEnableSuspensionForcePoint)
}

void WheelSettings::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mSuspensionForcePoint);
	inStream.Write(mPosition);
	inStream.Write(mSuspensionDirection);
	inStream.Write(mSteeringAxis);
	inStream.Write(mWheelForward);
	inStream.Write(mWheelUp);
	inStream.Write(mSuspensionMinLength);
	inStream.Write(mSuspensionMaxLength);
	inStream.Write(mSuspensionPreloadLength);
	mSuspensionSpring.SaveBinaryState(inStream);
	inStream.Write(mRadius);
	inStream.Write(mWidth);
	inStream.Write(mEnableSuspensionForcePoint);
}

void WheelSettings::RestoreBinaryState(StreamIn &inStream)
{
	inStream.Read(mSuspensionForcePoint);
	inStream.Read(mPosition);
	inStream.Read(mSuspensionDirection);
	inStream.Read(mSteeringAxis);
	inStream.Read(mWheelForward);
	inStream.Read(mWheelUp);
	inStream.Read(mSuspensionMinLength);
	inStream.Read(mSuspensionMaxLength);
	inStream.Read(mSuspensionPreloadLength);
	mSuspensionSpring.RestoreBinaryState(inStream);
	inStream.Read(mRadius);
	inStream.Read(mWidth);
	inStream.Read(mEnableSuspensionForcePoint);
}

Wheel::Wheel(const WheelSettings &inSettings) :
	mSettings(&inSettings),
	mSuspensionLength(inSettings.mSuspensionMaxLength)
{
	JPH_ASSERT(inSettings.mSuspensionDirection.IsNormalized());
	JPH_ASSERT(inSettings.mSteeringAxis.IsNormalized());
	JPH_ASSERT(inSettings.mWheelForward.IsNormalized());
	JPH_ASSERT(inSettings.mWheelUp.IsNormalized());
	JPH_ASSERT(inSettings.mSuspensionMinLength >= 0.0f);
	JPH_ASSERT(inSettings.mSuspensionMaxLength >= inSettings.mSuspensionMinLength);
	JPH_ASSERT(inSettings.mSuspensionPreloadLength >= 0.0f);
	JPH_ASSERT(inSettings.mSuspensionSpring.mFrequency > 0.0f);
	JPH_ASSERT(inSettings.mSuspensionSpring.mDamping >= 0.0f);
	JPH_ASSERT(inSettings.mRadius > 0.0f);
	JPH_ASSERT(inSettings.mWidth >= 0.0f);
}

bool Wheel::SolveLongitudinalConstraintPart(const VehicleConstraint &inConstraint, float inMinImpulse, float inMaxImpulse)
{
	return mLongitudinalPart.SolveVelocityConstraint(*inConstraint.GetVehicleBody(), *mContactBody, -mContactLongitudinal, inMinImpulse, inMaxImpulse);
}

bool Wheel::SolveLateralConstraintPart(const VehicleConstraint &inConstraint, float inMinImpulse, float inMaxImpulse)
{
	return mLateralPart.SolveVelocityConstraint(*inConstraint.GetVehicleBody(), *mContactBody, -mContactLateral, inMinImpulse, inMaxImpulse);
}

JPH_NAMESPACE_END
