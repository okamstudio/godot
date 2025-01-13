// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Reference.h>
#include <Jolt/ObjectStream/SerializableObject.h>
#include <Jolt/Physics/Constraints/SpringSettings.h>

JPH_NAMESPACE_BEGIN

class StreamIn;
class StreamOut;

enum class EMotorState
{
	Off,																///< Motor is off
	Velocity,															///< Motor will drive to target velocity
	Position															///< Motor will drive to target position
};

/// Class that contains the settings for a constraint motor.
/// See the main page of the API documentation for more information on how to configure a motor.
class JPH_EXPORT MotorSettings
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, MotorSettings)

public:
	/// Constructor
							MotorSettings() = default;
							MotorSettings(const MotorSettings &) = default;
	MotorSettings &			operator = (const MotorSettings &) = default;
							MotorSettings(float inFrequency, float inDamping) : mSpringSettings(ESpringMode::FrequencyAndDamping, inFrequency, inDamping) { JPH_ASSERT(IsValid()); }
							MotorSettings(float inFrequency, float inDamping, float inForceLimit, float inTorqueLimit) : mSpringSettings(ESpringMode::FrequencyAndDamping, inFrequency, inDamping), mMinForceLimit(-inForceLimit), mMaxForceLimit(inForceLimit), mMinTorqueLimit(-inTorqueLimit), mMaxTorqueLimit(inTorqueLimit) { JPH_ASSERT(IsValid()); }

	/// Set asymmetric force limits
	void					SetForceLimits(float inMin, float inMax)	{ JPH_ASSERT(inMin <= inMax); mMinForceLimit = inMin; mMaxForceLimit = inMax; }

	/// Set asymmetric torque limits
	void					SetTorqueLimits(float inMin, float inMax)	{ JPH_ASSERT(inMin <= inMax); mMinTorqueLimit = inMin; mMaxTorqueLimit = inMax; }

	/// Set symmetric force limits
	void					SetForceLimit(float inLimit)				{ mMinForceLimit = -inLimit; mMaxForceLimit = inLimit; }

	/// Set symmetric torque limits
	void					SetTorqueLimit(float inLimit)				{ mMinTorqueLimit = -inLimit; mMaxTorqueLimit = inLimit; }

	/// Check if settings are valid
	bool					IsValid() const								{ return mSpringSettings.mFrequency >= 0.0f && mSpringSettings.mDamping >= 0.0f && mMinForceLimit <= mMaxForceLimit && mMinTorqueLimit <= mMaxTorqueLimit; }

	/// Saves the contents of the motor settings in binary form to inStream.
	void					SaveBinaryState(StreamOut &inStream) const;

	/// Restores contents from the binary stream inStream.
	void					RestoreBinaryState(StreamIn &inStream);

	// Settings
	SpringSettings			mSpringSettings { ESpringMode::FrequencyAndDamping, 2.0f, 1.0f }; ///< Settings for the spring that is used to drive to the position target (not used when motor is a velocity motor).
	float					mMinForceLimit = -FLT_MAX;					///< Minimum force to apply in case of a linear constraint (N). Usually this is -mMaxForceLimit unless you want a motor that can e.g. push but not pull. Not used when motor is an angular motor.
	float					mMaxForceLimit = FLT_MAX;					///< Maximum force to apply in case of a linear constraint (N). Not used when motor is an angular motor.
	float					mMinTorqueLimit = -FLT_MAX;					///< Minimum torque to apply in case of a angular constraint (N m). Usually this is -mMaxTorqueLimit unless you want a motor that can e.g. push but not pull. Not used when motor is a position motor.
	float					mMaxTorqueLimit = FLT_MAX;					///< Maximum torque to apply in case of a angular constraint (N m). Not used when motor is a position motor.
};

JPH_NAMESPACE_END
