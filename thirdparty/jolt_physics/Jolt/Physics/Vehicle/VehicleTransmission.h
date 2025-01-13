// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/ObjectStream/SerializableObject.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

/// How gears are shifted
enum class ETransmissionMode : uint8
{
	Auto,																///< Automatically shift gear up and down
	Manual,																///< Manual gear shift (call SetTransmissionInput)
};

/// Configuration for the transmission of a vehicle (gear box)
class JPH_EXPORT VehicleTransmissionSettings
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, VehicleTransmissionSettings)

public:
	/// Saves the contents in binary form to inStream.
	void					SaveBinaryState(StreamOut &inStream) const;

	/// Restores the contents in binary form to inStream.
	void					RestoreBinaryState(StreamIn &inStream);

	ETransmissionMode		mMode = ETransmissionMode::Auto;			///< How to switch gears
	Array<float>			mGearRatios { 2.66f, 1.78f, 1.3f, 1.0f, 0.74f }; ///< Ratio in rotation rate between engine and gear box, first element is 1st gear, 2nd element 2nd gear etc.
	Array<float>			mReverseGearRatios { -2.90f };				///< Ratio in rotation rate between engine and gear box when driving in reverse
	float					mSwitchTime = 0.5f;							///< How long it takes to switch gears (s), only used in auto mode
	float					mClutchReleaseTime = 0.3f;					///< How long it takes to release the clutch (go to full friction), only used in auto mode
	float					mSwitchLatency = 0.5f;						///< How long to wait after releasing the clutch before another switch is attempted (s), only used in auto mode
	float					mShiftUpRPM = 4000.0f;						///< If RPM of engine is bigger then this we will shift a gear up, only used in auto mode
	float					mShiftDownRPM = 2000.0f;					///< If RPM of engine is smaller then this we will shift a gear down, only used in auto mode
	float					mClutchStrength = 10.0f;					///< Strength of the clutch when fully engaged. Total torque a clutch applies is Torque = ClutchStrength * (Velocity Engine - Avg Velocity Wheels At Clutch) (units: k m^2 s^-1)
};

/// Runtime data for transmission
class JPH_EXPORT VehicleTransmission : public VehicleTransmissionSettings
{
public:
	/// Set input from driver regarding the transmission (only relevant when transmission is set to manual mode)
	/// @param inCurrentGear Current gear, -1 = reverse, 0 = neutral, 1 = 1st gear etc.
	/// @param inClutchFriction Value between 0 and 1 indicating how much friction the clutch gives (0 = no friction, 1 = full friction)
	void					Set(int inCurrentGear, float inClutchFriction) { mCurrentGear = inCurrentGear; mClutchFriction = inClutchFriction; }

	/// Update the current gear and clutch friction if the transmission is in auto mode
	/// @param inDeltaTime Time step delta time in s
	/// @param inCurrentRPM Current RPM for engine
	/// @param inForwardInput Hint if the user wants to drive forward (> 0) or backwards (< 0)
	/// @param inCanShiftUp Indicates if we want to allow the transmission to shift up (e.g. pass false if wheels are slipping)
	void					Update(float inDeltaTime, float inCurrentRPM, float inForwardInput, bool inCanShiftUp);

	/// Current gear, -1 = reverse, 0 = neutral, 1 = 1st gear etc.
	int						GetCurrentGear() const						{ return mCurrentGear; }

	/// Value between 0 and 1 indicating how much friction the clutch gives (0 = no friction, 1 = full friction)
	float					GetClutchFriction() const					{ return mClutchFriction; }

	/// If the auto box is currently switching gears
	bool					IsSwitchingGear() const						{ return mGearSwitchTimeLeft > 0.0f; }

	/// Return the transmission ratio based on the current gear (ratio between engine and differential)
	float					GetCurrentRatio() const;

	/// Only allow sleeping when the transmission is idle
	bool					AllowSleep() const							{ return mGearSwitchTimeLeft <= 0.0f && mClutchReleaseTimeLeft <= 0.0f && mGearSwitchLatencyTimeLeft <= 0.0f; }

	/// Saving state for replay
	void					SaveState(StateRecorder &inStream) const;
	void					RestoreState(StateRecorder &inStream);

private:
	int						mCurrentGear = 0;							///< Current gear, -1 = reverse, 0 = neutral, 1 = 1st gear etc.
	float					mClutchFriction = 1.0f;						///< Value between 0 and 1 indicating how much friction the clutch gives (0 = no friction, 1 = full friction)
	float					mGearSwitchTimeLeft = 0.0f;					///< When switching gears this will be > 0 and will cause the engine to not provide any torque to the wheels for a short time (used for automatic gear switching only)
	float					mClutchReleaseTimeLeft = 0.0f;				///< After switching gears this will be > 0 and will cause the clutch friction to go from 0 to 1 (used for automatic gear switching only)
	float					mGearSwitchLatencyTimeLeft = 0.0f;			///< After releasing the clutch this will be > 0 and will prevent another gear switch (used for automatic gear switching only)
};

JPH_NAMESPACE_END
