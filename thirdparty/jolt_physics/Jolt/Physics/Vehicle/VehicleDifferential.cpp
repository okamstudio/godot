// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Vehicle/VehicleDifferential.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(VehicleDifferentialSettings)
{
	JPH_ADD_ATTRIBUTE(VehicleDifferentialSettings, mLeftWheel)
	JPH_ADD_ATTRIBUTE(VehicleDifferentialSettings, mRightWheel)
	JPH_ADD_ATTRIBUTE(VehicleDifferentialSettings, mDifferentialRatio)
	JPH_ADD_ATTRIBUTE(VehicleDifferentialSettings, mLeftRightSplit)
	JPH_ADD_ATTRIBUTE(VehicleDifferentialSettings, mLimitedSlipRatio)
	JPH_ADD_ATTRIBUTE(VehicleDifferentialSettings, mEngineTorqueRatio)
}

void VehicleDifferentialSettings::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mLeftWheel);
	inStream.Write(mRightWheel);
	inStream.Write(mDifferentialRatio);
	inStream.Write(mLeftRightSplit);
	inStream.Write(mLimitedSlipRatio);
	inStream.Write(mEngineTorqueRatio);
}

void VehicleDifferentialSettings::RestoreBinaryState(StreamIn &inStream)
{
	inStream.Read(mLeftWheel);
	inStream.Read(mRightWheel);
	inStream.Read(mDifferentialRatio);
	inStream.Read(mLeftRightSplit);
	inStream.Read(mLimitedSlipRatio);
	inStream.Read(mEngineTorqueRatio);
}

void VehicleDifferentialSettings::CalculateTorqueRatio(float inLeftAngularVelocity, float inRightAngularVelocity, float &outLeftTorqueFraction, float &outRightTorqueFraction) const
{
	// Start with the default torque ratio
	outLeftTorqueFraction = 1.0f - mLeftRightSplit;
	outRightTorqueFraction = mLeftRightSplit;

	if (mLimitedSlipRatio < FLT_MAX)
	{
		JPH_ASSERT(mLimitedSlipRatio > 1.0f);

		// This is a limited slip differential, adjust torque ratios according to wheel speeds
		float omega_l = max(1.0e-3f, abs(inLeftAngularVelocity)); // prevent div by zero by setting a minimum velocity and ignoring that the wheels may be rotating in different directions
		float omega_r = max(1.0e-3f, abs(inRightAngularVelocity));
		float omega_min = min(omega_l, omega_r);
		float omega_max = max(omega_l, omega_r);

		// Map into a value that is 0 when the wheels are turning at an equal rate and 1 when the wheels are turning at mLimitedSlipRotationRatio
		float alpha = min((omega_max / omega_min - 1.0f) / (mLimitedSlipRatio - 1.0f), 1.0f);
		JPH_ASSERT(alpha >= 0.0f);
		float one_min_alpha = 1.0f - alpha;

		if (omega_l < omega_r)
		{
			// Redirect more power to the left wheel
			outLeftTorqueFraction = outLeftTorqueFraction * one_min_alpha + alpha;
			outRightTorqueFraction = outRightTorqueFraction * one_min_alpha;
		}
		else
		{
			// Redirect more power to the right wheel
			outLeftTorqueFraction = outLeftTorqueFraction * one_min_alpha;
			outRightTorqueFraction = outRightTorqueFraction * one_min_alpha + alpha;
		}
	}

	// Assert the values add up to 1
	JPH_ASSERT(abs(outLeftTorqueFraction + outRightTorqueFraction - 1.0f) < 1.0e-6f);
}

JPH_NAMESPACE_END
