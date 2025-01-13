// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/ObjectStream/SerializableObject.h>
#include <Jolt/Core/LinearCurve.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

/// On which side of the vehicle the track is located (for steering)
enum class ETrackSide : uint
{
	Left = 0,
	Right = 1,
	Num = 2
};

/// Generic properties for tank tracks
class JPH_EXPORT VehicleTrackSettings
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, VehicleTrackSettings)

public:
	/// Saves the contents in binary form to inStream.
	void					SaveBinaryState(StreamOut &inStream) const;

	/// Restores the contents in binary form to inStream.
	void					RestoreBinaryState(StreamIn &inStream);

	uint					mDrivenWheel;								///< Which wheel on the track is connected to the engine
	Array<uint>				mWheels;									///< Indices of wheels that are inside this track, should include the driven wheel too
	float					mInertia = 10.0f;							///< Moment of inertia (kg m^2) of the track and its wheels as seen on the driven wheel
	float					mAngularDamping = 0.5f;						///< Damping factor of track and its wheels: dw/dt = -c * w as seen on the driven wheel
	float					mMaxBrakeTorque = 15000.0f;					///< How much torque (Nm) the brakes can apply on the driven wheel
	float					mDifferentialRatio = 6.0f;					///< Ratio between rotation speed of gear box and driven wheel of track
};

/// Runtime data for tank tracks
class JPH_EXPORT VehicleTrack : public VehicleTrackSettings
{
public:
	/// Saving state for replay
	void					SaveState(StateRecorder &inStream) const;
	void					RestoreState(StateRecorder &inStream);

	float					mAngularVelocity = 0.0f;					///< Angular velocity of the driven wheel, will determine the speed of the entire track
};

using VehicleTracks = VehicleTrack[(int)ETrackSide::Num];

JPH_NAMESPACE_END
