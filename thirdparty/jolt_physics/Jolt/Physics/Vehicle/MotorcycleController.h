// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

JPH_NAMESPACE_BEGIN

/// Settings of a two wheeled motorcycle (adds a spring to balance the motorcycle)
/// Note: The motor cycle controller is still in development and may need a lot of tweaks/hacks to work properly!
class JPH_EXPORT MotorcycleControllerSettings : public WheeledVehicleControllerSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, MotorcycleControllerSettings)

public:
	// See: VehicleControllerSettings
	virtual VehicleController *	ConstructController(VehicleConstraint &inConstraint) const override;
	virtual void				SaveBinaryState(StreamOut &inStream) const override;
	virtual void				RestoreBinaryState(StreamIn &inStream) override;

	/// How far we're willing to make the bike lean over in turns (in radians)
	float						mMaxLeanAngle = DegreesToRadians(45.0f);

	/// Spring constant for the lean spring
	float						mLeanSpringConstant = 5000.0f;

	/// Spring damping constant for the lean spring
	float						mLeanSpringDamping = 1000.0f;

	/// The lean spring applies an additional force equal to this coefficient * Integral(delta angle, 0, t), this effectively makes the lean spring a PID controller
	float						mLeanSpringIntegrationCoefficient = 0.0f;

	/// How much to decay the angle integral when the wheels are not touching the floor: new_value = e^(-decay * t) * initial_value
	float						mLeanSpringIntegrationCoefficientDecay = 4.0f;

	/// How much to smooth the lean angle (0 = no smoothing, 1 = lean angle never changes)
	/// Note that this is frame rate dependent because the formula is: smoothing_factor * previous + (1 - smoothing_factor) * current
	float						mLeanSmoothingFactor = 0.8f;
};

/// Runtime controller class
class JPH_EXPORT MotorcycleController : public WheeledVehicleController
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
								MotorcycleController(const MotorcycleControllerSettings &inSettings, VehicleConstraint &inConstraint);

	/// Get the distance between the front and back wheels
	float						GetWheelBase() const;

	/// Enable or disable the lean spring. This allows you to temporarily disable the lean spring to allow the motorcycle to fall over.
	void						EnableLeanController(bool inEnable)					{ mEnableLeanController = inEnable; }

	/// Check if the lean spring is enabled.
	bool						IsLeanControllerEnabled() const						{ return mEnableLeanController; }

	/// Enable or disable the lean steering limit. When enabled (default) the steering angle is limited based on the vehicle speed to prevent steering that would cause an inertial force that causes the motorcycle to topple over.
	void						EnableLeanSteeringLimit(bool inEnable)				{ mEnableLeanSteeringLimit = inEnable; }
	bool						IsLeanSteeringLimitEnabled() const					{ return mEnableLeanSteeringLimit; }

	/// Spring constant for the lean spring
	void						SetLeanSpringConstant(float inConstant)				{ mLeanSpringConstant = inConstant; }
	float						GetLeanSpringConstant() const						{ return mLeanSpringConstant; }

	/// Spring damping constant for the lean spring
	void						SetLeanSpringDamping(float inDamping)				{ mLeanSpringDamping = inDamping; }
	float						GetLeanSpringDamping() const						{ return mLeanSpringDamping; }

	/// The lean spring applies an additional force equal to this coefficient * Integral(delta angle, 0, t), this effectively makes the lean spring a PID controller
	void						SetLeanSpringIntegrationCoefficient(float inCoefficient) { mLeanSpringIntegrationCoefficient = inCoefficient; }
	float						GetLeanSpringIntegrationCoefficient() const			{ return mLeanSpringIntegrationCoefficient; }

	/// How much to decay the angle integral when the wheels are not touching the floor: new_value = e^(-decay * t) * initial_value
	void						SetLeanSpringIntegrationCoefficientDecay(float inDecay) { mLeanSpringIntegrationCoefficientDecay = inDecay; }
	float						GetLeanSpringIntegrationCoefficientDecay() const	{ return mLeanSpringIntegrationCoefficientDecay; }

	/// How much to smooth the lean angle (0 = no smoothing, 1 = lean angle never changes)
	/// Note that this is frame rate dependent because the formula is: smoothing_factor * previous + (1 - smoothing_factor) * current
	void						SetLeanSmoothingFactor(float inFactor)				{ mLeanSmoothingFactor = inFactor; }
	float						GetLeanSmoothingFactor() const						{ return mLeanSmoothingFactor; }

protected:
	// See: VehicleController
	virtual void				PreCollide(float inDeltaTime, PhysicsSystem &inPhysicsSystem) override;
	virtual bool				SolveLongitudinalAndLateralConstraints(float inDeltaTime) override;
	virtual void				SaveState(StateRecorder &inStream) const override;
	virtual void				RestoreState(StateRecorder &inStream) override;
#ifdef JPH_DEBUG_RENDERER
	virtual void				Draw(DebugRenderer *inRenderer) const override;
#endif // JPH_DEBUG_RENDERER

	// Configuration properties
	bool						mEnableLeanController = true;
	bool						mEnableLeanSteeringLimit = true;
	float						mMaxLeanAngle;
	float						mLeanSpringConstant;
	float						mLeanSpringDamping;
	float						mLeanSpringIntegrationCoefficient;
	float						mLeanSpringIntegrationCoefficientDecay;
	float						mLeanSmoothingFactor;

	// Run-time calculated target lean vector
	Vec3						mTargetLean = Vec3::sZero();

	// Integrated error for the lean spring
	float						mLeanSpringIntegratedDeltaAngle = 0.0f;

	// Run-time total angular impulse applied to turn the cycle towards the target lean angle
	float						mAppliedImpulse = 0.0f;
};

JPH_NAMESPACE_END
