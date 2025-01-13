// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleEngine.h>
#include <Jolt/Physics/Vehicle/VehicleTransmission.h>
#include <Jolt/Physics/Vehicle/VehicleDifferential.h>
#include <Jolt/Core/LinearCurve.h>

JPH_NAMESPACE_BEGIN

class PhysicsSystem;

/// WheelSettings object specifically for WheeledVehicleController
class JPH_EXPORT WheelSettingsWV : public WheelSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, WheelSettingsWV)

public:
	/// Constructor
								WheelSettingsWV();

	// See: WheelSettings
	virtual void				SaveBinaryState(StreamOut &inStream) const override;
	virtual void				RestoreBinaryState(StreamIn &inStream) override;

	float						mInertia = 0.9f;							///< Moment of inertia (kg m^2), for a cylinder this would be 0.5 * M * R^2 which is 0.9 for a wheel with a mass of 20 kg and radius 0.3 m
	float						mAngularDamping = 0.2f;						///< Angular damping factor of the wheel: dw/dt = -c * w
	float						mMaxSteerAngle = DegreesToRadians(70.0f);	///< How much this wheel can steer (radians)
	LinearCurve					mLongitudinalFriction;						///< On the Y-axis: friction in the forward direction of the tire. Friction is normally between 0 (no friction) and 1 (full friction) although friction can be a little bit higher than 1 because of the profile of a tire. On the X-axis: the slip ratio (fraction) defined as (omega_wheel * r_wheel - v_longitudinal) / |v_longitudinal|. You can see slip ratio as the amount the wheel is spinning relative to the floor: 0 means the wheel has full traction and is rolling perfectly in sync with the ground, 1 is for example when the wheel is locked and sliding over the ground.
	LinearCurve					mLateralFriction;							///< On the Y-axis: friction in the sideways direction of the tire. Friction is normally between 0 (no friction) and 1 (full friction) although friction can be a little bit higher than 1 because of the profile of a tire. On the X-axis: the slip angle (degrees) defined as angle between relative contact velocity and tire direction.
	float						mMaxBrakeTorque = 1500.0f;					///< How much torque (Nm) the brakes can apply to this wheel
	float						mMaxHandBrakeTorque = 4000.0f;				///< How much torque (Nm) the hand brake can apply to this wheel (usually only applied to the rear wheels)
};

/// Wheel object specifically for WheeledVehicleController
class JPH_EXPORT WheelWV : public Wheel
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit					WheelWV(const WheelSettingsWV &inWheel);

	/// Override GetSettings and cast to the correct class
	const WheelSettingsWV *		GetSettings() const							{ return StaticCast<WheelSettingsWV>(mSettings); }

	/// Apply a torque (N m) to the wheel for a particular delta time
	void						ApplyTorque(float inTorque, float inDeltaTime)
	{
		mAngularVelocity += inTorque * inDeltaTime / GetSettings()->mInertia;
	}

	/// Update the wheel rotation based on the current angular velocity
	void						Update(uint inWheelIndex, float inDeltaTime, const VehicleConstraint &inConstraint);

	float						mLongitudinalSlip = 0.0f;					///< Velocity difference between ground and wheel relative to ground velocity
	float						mLateralSlip = 0.0f;						///< Angular difference (in radians) between ground and wheel relative to ground velocity
	float						mCombinedLongitudinalFriction = 0.0f;		///< Combined friction coefficient in longitudinal direction (combines terrain and tires)
	float						mCombinedLateralFriction = 0.0f;			///< Combined friction coefficient in lateral direction (combines terrain and tires)
	float						mBrakeImpulse = 0.0f;						///< Amount of impulse that the brakes can apply to the floor (excluding friction)
};

/// Settings of a vehicle with regular wheels
///
/// The properties in this controller are largely based on "Car Physics for Games" by Marco Monster.
/// See: https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html
class JPH_EXPORT WheeledVehicleControllerSettings : public VehicleControllerSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, WheeledVehicleControllerSettings)

public:
	// See: VehicleControllerSettings
	virtual VehicleController *	ConstructController(VehicleConstraint &inConstraint) const override;
	virtual void				SaveBinaryState(StreamOut &inStream) const override;
	virtual void				RestoreBinaryState(StreamIn &inStream) override;

	VehicleEngineSettings		mEngine;									///< The properties of the engine
	VehicleTransmissionSettings	mTransmission;								///< The properties of the transmission (aka gear box)
	Array<VehicleDifferentialSettings> mDifferentials;						///< List of differentials and their properties
	float						mDifferentialLimitedSlipRatio = 1.4f;		///< Ratio max / min average wheel speed of each differential (measured at the clutch). When the ratio is exceeded all torque gets distributed to the differential with the minimal average velocity. This allows implementing a limited slip differential between differentials. Set to FLT_MAX for an open differential. Value should be > 1.
};

/// Runtime controller class
class JPH_EXPORT WheeledVehicleController : public VehicleController
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
								WheeledVehicleController(const WheeledVehicleControllerSettings &inSettings, VehicleConstraint &inConstraint);

	/// Typedefs
	using Differentials = Array<VehicleDifferentialSettings>;

	/// Set input from driver
	/// @param inForward Value between -1 and 1 for auto transmission and value between 0 and 1 indicating desired driving direction and amount the gas pedal is pressed
	/// @param inRight Value between -1 and 1 indicating desired steering angle (1 = right)
	/// @param inBrake Value between 0 and 1 indicating how strong the brake pedal is pressed
	/// @param inHandBrake Value between 0 and 1 indicating how strong the hand brake is pulled
	void						SetDriverInput(float inForward, float inRight, float inBrake, float inHandBrake) { mForwardInput = inForward; mRightInput = inRight; mBrakeInput = inBrake; mHandBrakeInput = inHandBrake; }

	/// Value between -1 and 1 for auto transmission and value between 0 and 1 indicating desired driving direction and amount the gas pedal is pressed
	void						SetForwardInput(float inForward)			{ mForwardInput = inForward; }
	float						GetForwardInput() const						{ return mForwardInput; }

	/// Value between -1 and 1 indicating desired steering angle (1 = right)
	void						SetRightInput(float inRight)				{ mRightInput = inRight; }
	float						GetRightInput() const						{ return mRightInput; }

	/// Value between 0 and 1 indicating how strong the brake pedal is pressed
	void						SetBrakeInput(float inBrake)				{ mBrakeInput = inBrake; }
	float						GetBrakeInput() const						{ return mBrakeInput; }

	/// Value between 0 and 1 indicating how strong the hand brake is pulled
	void						SetHandBrakeInput(float inHandBrake)		{ mHandBrakeInput = inHandBrake; }
	float						GetHandBrakeInput() const					{ return mHandBrakeInput; }

	/// Get current engine state
	const VehicleEngine &		GetEngine() const							{ return mEngine; }

	/// Get current engine state (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	VehicleEngine &				GetEngine()									{ return mEngine; }

	/// Get current transmission state
	const VehicleTransmission &	GetTransmission() const						{ return mTransmission; }

	/// Get current transmission state (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	VehicleTransmission &		GetTransmission()							{ return mTransmission; }

	/// Get the differentials this vehicle has
	const Differentials &		GetDifferentials() const					{ return mDifferentials; }

	/// Get the differentials this vehicle has (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	Differentials &				GetDifferentials()							{ return mDifferentials; }

	/// Ratio max / min average wheel speed of each differential (measured at the clutch).
	float						GetDifferentialLimitedSlipRatio() const		{ return mDifferentialLimitedSlipRatio; }
	void						SetDifferentialLimitedSlipRatio(float inV)	{ mDifferentialLimitedSlipRatio = inV; }

	/// Get the average wheel speed of all driven wheels (measured at the clutch)
	float						GetWheelSpeedAtClutch() const;

	/// Calculate max tire impulses by combining friction, slip, and suspension impulse. Note that the actual applied impulse may be lower (e.g. when the vehicle is stationary on a horizontal surface the actual impulse applied will be 0).
	using TireMaxImpulseCallback = function<void(uint inWheelIndex, float &outLongitudinalImpulse, float &outLateralImpulse, float inSuspensionImpulse, float inLongitudinalFriction, float inLateralFriction, float inLongitudinalSlip, float inLateralSlip, float inDeltaTime)>;
	const TireMaxImpulseCallback&GetTireMaxImpulseCallback() const			{ return mTireMaxImpulseCallback; }
	void						SetTireMaxImpulseCallback(const TireMaxImpulseCallback &inTireMaxImpulseCallback)	{ mTireMaxImpulseCallback = inTireMaxImpulseCallback; }

#ifdef JPH_DEBUG_RENDERER
	/// Debug drawing of RPM meter
	void						SetRPMMeter(Vec3Arg inPosition, float inSize) { mRPMMeterPosition = inPosition; mRPMMeterSize = inSize; }
#endif // JPH_DEBUG_RENDERER

protected:
	// See: VehicleController
	virtual Wheel *				ConstructWheel(const WheelSettings &inWheel) const override { JPH_ASSERT(IsKindOf(&inWheel, JPH_RTTI(WheelSettingsWV))); return new WheelWV(static_cast<const WheelSettingsWV &>(inWheel)); }
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
	float						mRightInput = 0.0f;							///< Value between -1 and 1 indicating desired steering angle
	float						mBrakeInput = 0.0f;							///< Value between 0 and 1 indicating how strong the brake pedal is pressed
	float						mHandBrakeInput = 0.0f;						///< Value between 0 and 1 indicating how strong the hand brake is pulled

	// Simulation information
	VehicleEngine				mEngine;									///< Engine state of the vehicle
	VehicleTransmission			mTransmission;								///< Transmission state of the vehicle
	Differentials				mDifferentials;								///< Differential states of the vehicle
	float						mDifferentialLimitedSlipRatio;				///< Ratio max / min average wheel speed of each differential (measured at the clutch).
	float						mPreviousDeltaTime = 0.0f;					///< Delta time of the last step

	// Callback that calculates the max impulse that the tire can apply to the ground
	TireMaxImpulseCallback		mTireMaxImpulseCallback =
		[](uint, float &outLongitudinalImpulse, float &outLateralImpulse, float inSuspensionImpulse, float inLongitudinalFriction, float inLateralFriction, float, float, float)
		{
			outLongitudinalImpulse = inLongitudinalFriction * inSuspensionImpulse;
			outLateralImpulse = inLateralFriction * inSuspensionImpulse;
		};

#ifdef JPH_DEBUG_RENDERER
	// Debug settings
	Vec3						mRPMMeterPosition { 0, 1, 0 };				///< Position (in local space of the body) of the RPM meter when drawing the constraint
	float						mRPMMeterSize = 0.5f;						///< Size of the RPM meter when drawing the constraint
#endif // JPH_DEBUG_RENDERER
};

JPH_NAMESPACE_END
