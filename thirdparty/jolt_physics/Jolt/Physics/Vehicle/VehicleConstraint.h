// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AngleConstraintPart.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleAntiRollBar.h>
#include <Jolt/Physics/Vehicle/Wheel.h>
#include <Jolt/Physics/Vehicle/VehicleController.h>

JPH_NAMESPACE_BEGIN

class PhysicsSystem;

/// Configuration for constraint that simulates a wheeled vehicle.
///
/// The properties in this constraint are largely based on "Car Physics for Games" by Marco Monster.
/// See: https://www.asawicki.info/Mirror/Car%20Physics%20for%20Games/Car%20Physics%20for%20Games.html
class JPH_EXPORT VehicleConstraintSettings : public ConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, VehicleConstraintSettings)

public:
	/// Saves the contents of the constraint settings in binary form to inStream.
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	Vec3						mUp { 0, 1, 0 };							///< Vector indicating the up direction of the vehicle (in local space to the body)
	Vec3						mForward { 0, 0, 1 };						///< Vector indicating forward direction of the vehicle (in local space to the body)
	float						mMaxPitchRollAngle = JPH_PI;				///< Defines the maximum pitch/roll angle (rad), can be used to avoid the car from getting upside down. The vehicle up direction will stay within a cone centered around the up axis with half top angle mMaxPitchRollAngle, set to pi to turn off.
	Array<Ref<WheelSettings>>	mWheels;									///< List of wheels and their properties
	Array<VehicleAntiRollBar>	mAntiRollBars;								///< List of anti rollbars and their properties
	Ref<VehicleControllerSettings> mController;								///< Defines how the vehicle can accelerate / decelerate

protected:
	/// This function should not be called directly, it is used by sRestoreFromBinaryState.
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// Constraint that simulates a vehicle
/// Note: Don't forget to register the constraint as a StepListener with the PhysicsSystem!
///
/// When the vehicle drives over very light objects (rubble) you may see the car body dip down. This is a known issue and is an artifact of the iterative solver that Jolt is using.
/// Basically if a light object is sandwiched between two heavy objects (the static floor and the car body), the light object is not able to transfer enough force from the ground to
/// the car body to keep the car body up. You can see this effect in the HeavyOnLightTest sample, the boxes on the right have a lot of penetration because they're on top of light objects.
///
/// There are a couple of ways to improve this:
///
/// 1. You can increase the number of velocity steps (global settings PhysicsSettings::mNumVelocitySteps or if you only want to increase it on
/// the vehicle you can use VehicleConstraintSettings::mNumVelocityStepsOverride). E.g. going from 10 to 30 steps in the HeavyOnLightTest sample makes the penetration a lot less.
/// The number of position steps can also be increased (the first prevents the body from going down, the second corrects it if the problem did
/// occur which inevitably happens due to numerical drift). This solution costs CPU cycles.
///
/// 2. You can reduce the mass difference between the vehicle body and the rubble on the floor (by making the rubble heavier or the car lighter).
///
/// 3. You could filter out collisions between the vehicle collision test and the rubble completely. This would make the wheels ignore the rubble but would cause the vehicle to drive
/// through it as if nothing happened. You could create fake wheels (keyframed bodies) that move along with the vehicle and that only collide with rubble (and not the vehicle or the ground).
/// This would cause the vehicle to push away the rubble without the rubble being able to affect the vehicle (unless it hits the main body of course).
///
/// Note that when driving over rubble, you may see the wheel jump up and down quite quickly because one frame a collision is found and the next frame not.
/// To alleviate this, it may be needed to smooth the motion of the visual mesh for the wheel.
class JPH_EXPORT VehicleConstraint : public Constraint, public PhysicsStepListener
{
public:
	/// Constructor / destructor
								VehicleConstraint(Body &inVehicleBody, const VehicleConstraintSettings &inSettings);
	virtual						~VehicleConstraint() override;

	/// Get the type of a constraint
	virtual EConstraintSubType	GetSubType() const override					{ return EConstraintSubType::Vehicle; }

	/// Defines the maximum pitch/roll angle (rad), can be used to avoid the car from getting upside down. The vehicle up direction will stay within a cone centered around the up axis with half top angle mMaxPitchRollAngle, set to pi to turn off.
	void						SetMaxPitchRollAngle(float inMaxPitchRollAngle) { mCosMaxPitchRollAngle = Cos(inMaxPitchRollAngle); }

	/// Set the interface that tests collision between wheel and ground
	void						SetVehicleCollisionTester(const VehicleCollisionTester *inTester) { mVehicleCollisionTester = inTester; }

	/// Callback function to combine the friction of a tire with the friction of the body it is colliding with.
	/// On input ioLongitudinalFriction and ioLateralFriction contain the friction of the tire, on output they should contain the combined friction with inBody2.
	using CombineFunction = function<void(uint inWheelIndex, float &ioLongitudinalFriction, float &ioLateralFriction, const Body &inBody2, const SubShapeID &inSubShapeID2)>;

	/// Set the function that combines the friction of two bodies and returns it
	/// Default method is the geometric mean: sqrt(friction1 * friction2).
	void						SetCombineFriction(const CombineFunction &inCombineFriction) { mCombineFriction = inCombineFriction; }
	const CombineFunction &		GetCombineFriction() const					{ return mCombineFriction; }

	/// Callback function to notify of current stage in PhysicsStepListener::OnStep.
	using StepCallback = function<void(VehicleConstraint &inVehicle, const PhysicsStepListenerContext &inContext)>;

	/// Callback function to notify that PhysicsStepListener::OnStep has started for this vehicle. Default is to do nothing.
	/// Can be used to allow higher-level code to e.g. control steering. This is the last moment that the position/orientation of the vehicle can be changed.
	/// Wheel collision checks have not been performed yet.
	const StepCallback &		GetPreStepCallback() const					{ return mPreStepCallback; }
	void						SetPreStepCallback(const StepCallback &inPreStepCallback) { mPreStepCallback = inPreStepCallback; }

	/// Callback function to notify that PhysicsStepListener::OnStep has just completed wheel collision checks. Default is to do nothing.
	/// Can be used to allow higher-level code to e.g. detect tire contact or to modify the velocity of the vehicle based on the wheel contacts.
	/// You should not change the position of the vehicle in this callback as the wheel collision checks have already been performed.
	const StepCallback &		GetPostCollideCallback() const				{ return mPostCollideCallback; }
	void						SetPostCollideCallback(const StepCallback &inPostCollideCallback) { mPostCollideCallback = inPostCollideCallback; }

	/// Callback function to notify that PhysicsStepListener::OnStep has completed for this vehicle. Default is to do nothing.
	/// Can be used to allow higher-level code to e.g. control the vehicle in the air.
	/// You should not change the position of the vehicle in this callback as the wheel collision checks have already been performed.
	const StepCallback &		GetPostStepCallback() const					{ return mPostStepCallback; }
	void						SetPostStepCallback(const StepCallback &inPostStepCallback) { mPostStepCallback = inPostStepCallback; }

	/// Override gravity for this vehicle. Note that overriding gravity will set the gravity factor of the vehicle body to 0 and apply gravity in the PhysicsStepListener instead.
	void						OverrideGravity(Vec3Arg inGravity)			{ mGravityOverride = inGravity; mIsGravityOverridden = true; }
	bool						IsGravityOverridden() const					{ return mIsGravityOverridden; }
	Vec3						GetGravityOverride() const					{ return mGravityOverride; }
	void						ResetGravityOverride()						{ mIsGravityOverridden = false; mBody->GetMotionProperties()->SetGravityFactor(1.0f); } ///< Note that resetting the gravity override will restore the gravity factor of the vehicle body to 1.

	/// Get the local space forward vector of the vehicle
	Vec3						GetLocalForward() const						{ return mForward; }

	/// Get the local space up vector of the vehicle
	Vec3						GetLocalUp() const							{ return mUp; }

	/// Vector indicating the world space up direction (used to limit vehicle pitch/roll), calculated every frame by inverting gravity
	Vec3						GetWorldUp() const							{ return mWorldUp; }

	/// Access to the vehicle body
	Body *						GetVehicleBody() const						{ return mBody; }

	/// Access to the vehicle controller interface (determines acceleration / deceleration)
	const VehicleController *	GetController() const						{ return mController; }

	/// Access to the vehicle controller interface (determines acceleration / deceleration)
	VehicleController *			GetController()								{ return mController; }

	/// Get the state of the wheels
	const Wheels &				GetWheels() const							{ return mWheels; }

	/// Get the state of a wheels (writable interface, allows you to make changes to the configuration which will take effect the next time step)
	Wheels &					GetWheels()									{ return mWheels; }

	/// Get the state of a wheel
	Wheel *						GetWheel(uint inIdx)						{ return mWheels[inIdx]; }
	const Wheel *				GetWheel(uint inIdx) const					{ return mWheels[inIdx]; }

	/// Get the basis vectors for the wheel in local space to the vehicle body (note: basis does not rotate when the wheel rotates around its axis)
	/// @param inWheel Wheel to fetch basis for
	/// @param outForward Forward vector for the wheel
	/// @param outUp Up vector for the wheel
	/// @param outRight Right vector for the wheel
	void						GetWheelLocalBasis(const Wheel *inWheel, Vec3 &outForward, Vec3 &outUp, Vec3 &outRight) const;

	/// Get the transform of a wheel in local space to the vehicle body, returns a matrix that transforms a cylinder aligned with the Y axis in body space (not COM space)
	/// @param inWheelIndex Index of the wheel to fetch
	/// @param inWheelRight Unit vector that indicates right in model space of the wheel (so if you only have 1 wheel model, you probably want to specify the opposite direction for the left and right wheels)
	/// @param inWheelUp Unit vector that indicates up in model space of the wheel
	Mat44						GetWheelLocalTransform(uint inWheelIndex, Vec3Arg inWheelRight, Vec3Arg inWheelUp) const;

	/// Get the transform of a wheel in world space, returns a matrix that transforms a cylinder aligned with the Y axis in world space
	/// @param inWheelIndex Index of the wheel to fetch
	/// @param inWheelRight Unit vector that indicates right in model space of the wheel (so if you only have 1 wheel model, you probably want to specify the opposite direction for the left and right wheels)
	/// @param inWheelUp Unit vector that indicates up in model space of the wheel
	RMat44						GetWheelWorldTransform(uint inWheelIndex, Vec3Arg inWheelRight, Vec3Arg inWheelUp) const;

	/// Number of simulation steps between wheel collision tests when the vehicle is active. Default is 1. 0 = never, 1 = every step, 2 = every other step, etc.
	/// Note that if a vehicle has multiple wheels and the number of steps > 1, the wheels will be tested in a round robin fashion.
	/// If there are multiple vehicles, the tests will be spread out based on the BodyID of the vehicle.
	/// If you set this to test less than every step, you may see simulation artifacts. This setting can be used to reduce the cost of simulating vehicles in the distance.
	void						SetNumStepsBetweenCollisionTestActive(uint inSteps) { mNumStepsBetweenCollisionTestActive = inSteps; }
	uint						GetNumStepsBetweenCollisionTestActive() const { return mNumStepsBetweenCollisionTestActive; }

	/// Number of simulation steps between wheel collision tests when the vehicle is inactive. Default is 1. 0 = never, 1 = every step, 2 = every other step, etc.
	/// Note that if a vehicle has multiple wheels and the number of steps > 1, the wheels will be tested in a round robin fashion.
	/// If there are multiple vehicles, the tests will be spread out based on the BodyID of the vehicle.
	/// This number can be lower than the number of steps when the vehicle is active as the only purpose of this test is
	/// to allow the vehicle to wake up in response to bodies moving into the wheels but not touching the body of the vehicle.
	void						SetNumStepsBetweenCollisionTestInactive(uint inSteps) { mNumStepsBetweenCollisionTestInactive = inSteps; }
	uint						GetNumStepsBetweenCollisionTestInactive() const { return mNumStepsBetweenCollisionTestInactive; }

	// Generic interface of a constraint
	virtual bool				IsActive() const override					{ return mIsActive && Constraint::IsActive(); }
	virtual void				NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM) override { /* Do nothing */ }
	virtual void				SetupVelocityConstraint(float inDeltaTime) override;
	virtual void				ResetWarmStart() override;
	virtual void				WarmStartVelocityConstraint(float inWarmStartImpulseRatio) override;
	virtual bool				SolveVelocityConstraint(float inDeltaTime) override;
	virtual bool				SolvePositionConstraint(float inDeltaTime, float inBaumgarte) override;
	virtual void				BuildIslands(uint32 inConstraintIndex, IslandBuilder &ioBuilder, BodyManager &inBodyManager) override;
	virtual uint				BuildIslandSplits(LargeIslandSplitter &ioSplitter) const override;
#ifdef JPH_DEBUG_RENDERER
	virtual void				DrawConstraint(DebugRenderer *inRenderer) const override;
	virtual void				DrawConstraintLimits(DebugRenderer *inRenderer) const override;
#endif // JPH_DEBUG_RENDERER
	virtual void				SaveState(StateRecorder &inStream) const override;
	virtual void				RestoreState(StateRecorder &inStream) override;
	virtual Ref<ConstraintSettings> GetConstraintSettings() const override;

private:
	// See: PhysicsStepListener
	virtual void				OnStep(const PhysicsStepListenerContext &inContext) override;

	// Calculate the position where the suspension and traction forces should be applied in world space, relative to the center of mass of both bodies
	void						CalculateSuspensionForcePoint(const Wheel &inWheel, Vec3 &outR1PlusU, Vec3 &outR2) const;

	// Calculate the constraint properties for mPitchRollPart
	void						CalculatePitchRollConstraintProperties(RMat44Arg inBodyTransform);

	// Gravity override
	bool						mIsGravityOverridden = false;				///< If the gravity is currently overridden
	Vec3						mGravityOverride = Vec3::sZero();			///< Gravity override value, replaces PhysicsSystem::GetGravity() when mIsGravityOverridden is true

	// Simulation information
	Body *						mBody;										///< Body of the vehicle
	Vec3						mForward;									///< Local space forward vector for the vehicle
	Vec3						mUp;										///< Local space up vector for the vehicle
	Vec3						mWorldUp;									///< Vector indicating the world space up direction (used to limit vehicle pitch/roll)
	Wheels						mWheels;									///< Wheel states of the vehicle
	Array<VehicleAntiRollBar>	mAntiRollBars;								///< Anti rollbars of the vehicle
	VehicleController *			mController;								///< Controls the acceleration / deceleration of the vehicle
	bool						mIsActive = false;							///< If this constraint is active
	uint						mNumStepsBetweenCollisionTestActive = 1;	///< Number of simulation steps between wheel collision tests when the vehicle is active
	uint						mNumStepsBetweenCollisionTestInactive = 1;	///< Number of simulation steps between wheel collision tests when the vehicle is inactive
	uint						mCurrentStep = 0;							///< Current step number, used to determine when to test a wheel

	// Prevent vehicle from toppling over
	float						mCosMaxPitchRollAngle;						///< Cos of the max pitch/roll angle
	float						mCosPitchRollAngle;							///< Cos of the current pitch/roll angle
	Vec3						mPitchRollRotationAxis { 0, 1, 0 };			///< Current axis along which to apply torque to prevent the car from toppling over
	AngleConstraintPart			mPitchRollPart;								///< Constraint part that prevents the car from toppling over

	// Interfaces
	RefConst<VehicleCollisionTester> mVehicleCollisionTester;				///< Class that performs testing of collision for the wheels
	CombineFunction				mCombineFriction = [](uint, float &ioLongitudinalFriction, float &ioLateralFriction, const Body &inBody2, const SubShapeID &)
	{
		float body_friction = inBody2.GetFriction();

		ioLongitudinalFriction = sqrt(ioLongitudinalFriction * body_friction);
		ioLateralFriction = sqrt(ioLateralFriction * body_friction);
	};

	// Callbacks
	StepCallback				mPreStepCallback;
	StepCallback				mPostCollideCallback;
	StepCallback				mPostStepCallback;
};

JPH_NAMESPACE_END
