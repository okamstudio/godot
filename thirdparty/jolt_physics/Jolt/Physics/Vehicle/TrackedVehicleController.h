// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleEngine.h>
#include <Jolt/Physics/Vehicle/VehicleTransmission.h>
#include <Jolt/Physics/Vehicle/VehicleTrack.h>

JPH_NAMESPACE_BEGIN

class PhysicsSystem;

/// WheelSettings object specifically for TrackedVehicleController
class JPH_EXPORT WheelSettingsTV : public WheelSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, WheelSettingsTV)

public:
	// See: WheelSettings
	virtual void				SaveBinaryState(StreamOut &inStream) const override;
	virtual void				RestoreBinaryState(StreamIn &inStream) override;

	float						mLongitudinalFriction = 4.0f;				///< Friction in forward direction of tire
	float						mLateralFriction = 2.0f;					///< Friction in sideway direction of tire
};

/// Wheel object specifically for TrackedVehicleController
class JPH_EXPORT WheelTV : public Wheel
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit					WheelTV(const WheelSettingsTV &inWheel);

	/// Override GetSettings and cast to the correct class
	const WheelSettingsTV *		GetSettings() const							{ return StaticCast<WheelSettingsTV>(mSettings); }

	/// Update the angular velocity of the wheel based on the angular velocity of the track
	void						CalculateAngularVelocity(const VehicleConstraint &inConstraint);

	/// Update the wheel rotation based on the current angular velocity
	void						Update(uint inWheelIndex, float inDeltaTime, const VehicleConstraint &inConstraint);

	int							mTrackIndex = -1;							///< Index in mTracks to which this wheel is attached (calculated on initialization)
	float						mCombinedLongitudinalFriction = 0.0f;		///< Combined friction coefficient in longitudinal direction (combines terrain and track)
	float						mCombinedLateralFriction = 0.0f;			///< Combined friction coefficient in lateral direction (combines terrain and track)
	float						mBrakeImpulse = 0.0f;						///< Amount of impulse that the brakes can apply to the floor (excluding friction), spread out from brake impulse applied on track
};

/// Settings of a vehicle with tank tracks
///
/// Default settings are based around what I could find about the M1 Abrams tank.
/// Note to avoid issues with very heavy objects vs very light objects the mass of the tank should be a lot lower (say 10x) than that of a real tank. That means that the engine/brake torque is also 10x less.
class JPH_EXPORT TrackedVehicleControllerSettings : public VehicleControllerSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, TrackedVehicleControllerSettings)

public:
	// Constructor
								TrackedVehicleControllerSettings();

	// See: VehicleControllerSettings
	virtual VehicleController *	ConstructController(VehicleConstraint &inConstraint) const override;
	virtual void				SaveBinaryState(StreamOut &inStream) const override;
	virtual void				RestoreBinaryState(StreamIn &inStream) override;

	VehicleEngineSettings		mEngine;									///< The properties of the engine
	VehicleTransmissionSettings	mTransmission;								///< The properties of the transmission (aka gear box)
	VehicleTrackSettings		mTracks[(int)ETrackSide::Num];				///< List of tracks and their properties
};

/// Runtime controller class for vehicle with tank tracks
class JPH_EXPORT TrackedVehicleController : public VehicleController
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
								TrackedVehicleController(const TrackedVehicleControllerSettings &inSettings, VehicleConstraint &inConstraint);

	/// Set input from driver
	/// @param inForward Value between -1 and 1 for auto transmission and value between 0 and 1 indicating desired driving direction and amount the gas pedal is pressed
	/// @param inLeftRatio Value between -1 and 1 indicating an extra multiplier to the rotation rate of the left track (used for steering)
	/// @param inRightRatio Value between -1 and 1 indicating an extra multiplier to the rotation rate of the right track (used for steering)
	/// @param inBrake Value between 0 and 1 indicating how strong the brake pedal is pressed
	void						SetDriverInput(float inForward, float inLeftRatio, float inRightRatio, float inBrake) { JPH_ASSERT(inLeftRatio != 0.0f && inRightRatio != 0.0f); mForwardInput = inForward; mLeftRatio = inLeftRatio; mRightRatio = inRightRatio; mBrakeInput = inBrake; }

	/// Value between -1 and 1 for auto transmission and value between 0 and 1 indicating desired driving direction and amount the gas pedal is pressed
	void						SetForwardInput(float inForward)			{ mForwardInput = inForward; }
	float						GetForwardInput() const						{ return mForwardInput; }

	/// Value between -1 and 1 indicating an extra multiplier to the rotation rate of the left track (used for steering)
	void						SetLeftRatio(float inLeftRatio)				{ JPH_ASSERT(inLeftRatio != 0.0f); mLeftRatio = inLeftRatio; }
	float						GetLeftRatio() const						{ return mLeftRatio; }

	/// Value between -1 and 1 indicating an extra multiplier to the rotation rate of the right track (used for steering)
	void						SetRightRatio(float inRightRatio)			{ JPH_ASSERT(inRightRatio != 0.0f); mRightRatio = inRightRatio; }
	float						GetRightRatio() const						{ return mRightRatio; }

	/// Value between 0 and 1 indicating how strong the brake pedal is pressed
	void						SetBrakeInput(float inBrake)				{ mBrakeInput = inBrake; }
	float						GetBrakeInput() const						{ return mBrakeInput; }

	/// Get current engine state
	const VehicleEngine &		GetEngine() const							{ return mEngine; }

	/// Get current engine state (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	VehicleEngine &				GetEngine()									{ return mEngine; }

	/// Get current transmission state
	const VehicleTransmission &	GetTransmission() const						{ return mTransmission; }

	/// Get current transmission state (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	VehicleTransmission &		GetTransmission()							{ return mTransmission; }

	/// Get the tracks this vehicle has
	const VehicleTracks	&		GetTracks() const							{ return mTracks; }

	/// Get the tracks this vehicle has (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	VehicleTracks &				GetTracks()									{ return mTracks; }

#ifdef JPH_DEBUG_RENDERER
	/// Debug drawing of RPM meter
	void						SetRPMMeter(Vec3Arg inPosition, float inSize) { mRPMMeterPosition = inPosition; mRPMMeterSize = inSize; }
#endif // JPH_DEBUG_RENDERER

protected:
	/// Synchronize angular velocities of left and right tracks according to their ratios
	void						SyncLeftRightTracks();

	// See: VehicleController
	virtual Wheel *				ConstructWheel(const WheelSettings &inWheel) const override { JPH_ASSERT(IsKindOf(&inWheel, JPH_RTTI(WheelSettingsTV))); return new WheelTV(static_cast<const WheelSettingsTV &>(inWheel)); }
	virtual bool				AllowSleep() const override;
	virtual void				PreCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) override;
	virtual void				PostCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) override;
	virtual bool				SolveLongitudinalAndLateralConstraints(float inDeltaTime) override;
	virtual void				SaveState(StateRecorder &inStream) const override;
	virtual void				RestoreState(StateRecorder &inStream) override;
#ifdef JPH_DEBUG_RENDERER
	virtual void				Draw(DebugRenderer *inRenderer) const override;
#endif // JPH_DEBUG_RENDERER

	// Control information
	float						mForwardInput = 0.0f;						///< Value between -1 and 1 for auto transmission and value between 0 and 1 indicating desired driving direction and amount the gas pedal is pressed
	float						mLeftRatio = 1.0f;							///< Value between -1 and 1 indicating an extra multiplier to the rotation rate of the left track (used for steering)
	float						mRightRatio = 1.0f;							///< Value between -1 and 1 indicating an extra multiplier to the rotation rate of the right track (used for steering)
	float						mBrakeInput = 0.0f;							///< Value between 0 and 1 indicating how strong the brake pedal is pressed

	// Simulation information
	VehicleEngine				mEngine;									///< Engine state of the vehicle
	VehicleTransmission			mTransmission;								///< Transmission state of the vehicle
	VehicleTracks				mTracks;									///< Tracks of the vehicle

#ifdef JPH_DEBUG_RENDERER
	// Debug settings
	Vec3						mRPMMeterPosition { 0, 1, 0 };				///< Position (in local space of the body) of the RPM meter when drawing the constraint
	float						mRPMMeterSize = 0.5f;						///< Size of the RPM meter when drawing the constraint
#endif // JPH_DEBUG_RENDERER
};

JPH_NAMESPACE_END
