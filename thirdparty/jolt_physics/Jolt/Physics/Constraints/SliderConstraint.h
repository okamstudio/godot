// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/ConstraintPart/DualAxisConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/RotationEulerConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AxisConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Slider constraint settings, used to create a slider constraint
class JPH_EXPORT SliderConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, SliderConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint.
	/// Note that the rotation constraint will be solved from body 1. This means that if body 1 and body 2 have different masses / inertias (kinematic body = infinite mass / inertia), body 1 should be the heaviest body.
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// Simple way of setting the slider and normal axis in world space (assumes the bodies are already oriented correctly when the constraint is created)
	void						SetSliderAxis(Vec3Arg inSliderAxis);

	/// This determines in which space the constraint is setup, all properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	/// When mSpace is WorldSpace mPoint1 and mPoint2 can be automatically calculated based on the positions of the bodies when the constraint is created (the current relative position/orientation is chosen as the '0' position). Set this to false if you want to supply the attachment points yourself.
	bool						mAutoDetectPoint = false;

	/// Body 1 constraint reference frame (space determined by mSpace).
	/// Slider axis is the axis along which movement is possible (direction), normal axis is a perpendicular vector to define the frame.
	RVec3						mPoint1 = RVec3::sZero();
	Vec3						mSliderAxis1 = Vec3::sAxisX();
	Vec3						mNormalAxis1 = Vec3::sAxisY();

	/// Body 2 constraint reference frame (space determined by mSpace)
	RVec3						mPoint2 = RVec3::sZero();
	Vec3						mSliderAxis2 = Vec3::sAxisX();
	Vec3						mNormalAxis2 = Vec3::sAxisY();

	/// When the bodies move so that mPoint1 coincides with mPoint2 the slider position is defined to be 0, movement will be limited between [mLimitsMin, mLimitsMax] where mLimitsMin e [-inf, 0] and mLimitsMax e [0, inf]
	float						mLimitsMin = -FLT_MAX;
	float						mLimitsMax = FLT_MAX;

	/// When enabled, this makes the limits soft. When the constraint exceeds the limits, a spring force will pull it back.
	SpringSettings				mLimitsSpringSettings;

	/// Maximum amount of friction force to apply (N) when not driven by a motor.
	float						mMaxFrictionForce = 0.0f;

	/// In case the constraint is powered, this determines the motor settings around the sliding axis
	MotorSettings				mMotorSettings;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// A slider constraint allows movement in only 1 axis (and no rotation). Also known as a prismatic constraint.
class JPH_EXPORT SliderConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct slider constraint
								SliderConstraint(Body &inBody1, Body &inBody2, const SliderConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override								{ return EConstraintSubType::Slider; }
	virtual void				NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM) override;
	virtual void				SetupVelocityConstraint(float inDeltaTime) override;
	virtual void				ResetWarmStart() override;
	virtual void				WarmStartVelocityConstraint(float inWarmStartImpulseRatio) override;
	virtual bool				SolveVelocityConstraint(float inDeltaTime) override;
	virtual bool				SolvePositionConstraint(float inDeltaTime, float inBaumgarte) override;
#ifdef JPH_DEBUG_RENDERER
	virtual void				DrawConstraint(DebugRenderer *inRenderer) const override;
	virtual void				DrawConstraintLimits(DebugRenderer *inRenderer) const override;
#endif // JPH_DEBUG_RENDERER
	virtual void				SaveState(StateRecorder &inStream) const override;
	virtual void				RestoreState(StateRecorder &inStream) override;
	virtual Ref<ConstraintSettings> GetConstraintSettings() const override;

	// See: TwoBodyConstraint
	virtual Mat44				GetConstraintToBody1Matrix() const override;
	virtual Mat44				GetConstraintToBody2Matrix() const override;

	/// Get the current distance from the rest position
	float						GetCurrentPosition() const;

	/// Friction control
	void						SetMaxFrictionForce(float inFrictionForce)				{ mMaxFrictionForce = inFrictionForce; }
	float						GetMaxFrictionForce() const								{ return mMaxFrictionForce; }

	/// Motor settings
	MotorSettings &				GetMotorSettings()										{ return mMotorSettings; }
	const MotorSettings &		GetMotorSettings() const								{ return mMotorSettings; }

	// Motor controls
	void						SetMotorState(EMotorState inState)						{ JPH_ASSERT(inState == EMotorState::Off || mMotorSettings.IsValid()); mMotorState = inState; }
	EMotorState					GetMotorState() const									{ return mMotorState; }
	void						SetTargetVelocity(float inVelocity)						{ mTargetVelocity = inVelocity; }
	float						GetTargetVelocity() const								{ return mTargetVelocity; }
	void						SetTargetPosition(float inPosition)						{ mTargetPosition = mHasLimits? Clamp(inPosition, mLimitsMin, mLimitsMax) : inPosition; }
	float						GetTargetPosition() const								{ return mTargetPosition; }

	/// Update the limits of the slider constraint (see SliderConstraintSettings)
	void						SetLimits(float inLimitsMin, float inLimitsMax);
	float						GetLimitsMin() const									{ return mLimitsMin; }
	float						GetLimitsMax() const									{ return mLimitsMax; }
	bool						HasLimits() const										{ return mHasLimits; }

	/// Update the limits spring settings
	const SpringSettings &		GetLimitsSpringSettings() const							{ return mLimitsSpringSettings; }
	SpringSettings &			GetLimitsSpringSettings()								{ return mLimitsSpringSettings; }
	void						SetLimitsSpringSettings(const SpringSettings &inLimitsSpringSettings) { mLimitsSpringSettings = inLimitsSpringSettings; }

	///@name Get Lagrange multiplier from last physics update (the linear/angular impulse applied to satisfy the constraint)
	inline Vector<2>			GetTotalLambdaPosition() const							{ return mPositionConstraintPart.GetTotalLambda(); }
	inline float				GetTotalLambdaPositionLimits() const					{ return mPositionLimitsConstraintPart.GetTotalLambda(); }
	inline Vec3					GetTotalLambdaRotation() const							{ return mRotationConstraintPart.GetTotalLambda(); }
	inline float				GetTotalLambdaMotor() const								{ return mMotorConstraintPart.GetTotalLambda(); }

private:
	// Internal helper function to calculate the values below
	void						CalculateR1R2U(Mat44Arg inRotation1, Mat44Arg inRotation2);
	void						CalculateSlidingAxisAndPosition(Mat44Arg inRotation1);
	void						CalculatePositionConstraintProperties(Mat44Arg inRotation1, Mat44Arg inRotation2);
	void						CalculatePositionLimitsConstraintProperties(float inDeltaTime);
	void						CalculateMotorConstraintProperties(float inDeltaTime);

	// CONFIGURATION PROPERTIES FOLLOW

	// Local space constraint positions
	Vec3						mLocalSpacePosition1;
	Vec3						mLocalSpacePosition2;

	// Local space sliding direction
	Vec3						mLocalSpaceSliderAxis1;

	// Local space normals to the sliding direction (in body 1 space)
	Vec3						mLocalSpaceNormal1;
	Vec3						mLocalSpaceNormal2;

	// Inverse of initial rotation from body 1 to body 2 in body 1 space
	Quat						mInvInitialOrientation;

	// Slider limits
	bool						mHasLimits;
	float						mLimitsMin;
	float						mLimitsMax;

	// Soft constraint limits
	SpringSettings				mLimitsSpringSettings;

	// Friction
	float						mMaxFrictionForce;

	// Motor controls
	MotorSettings				mMotorSettings;
	EMotorState					mMotorState = EMotorState::Off;
	float						mTargetVelocity = 0.0f;
	float						mTargetPosition = 0.0f;

	// RUN TIME PROPERTIES FOLLOW

	// Positions where the point constraint acts on (middle point between center of masses)
	Vec3						mR1;
	Vec3						mR2;

	// X2 + R2 - X1 - R1
	Vec3						mU;

	// World space sliding direction
	Vec3						mWorldSpaceSliderAxis;

	// Normals to the slider axis
	Vec3						mN1;
	Vec3						mN2;

	// Distance along the slide axis
	float						mD = 0.0f;

	// The constraint parts
	DualAxisConstraintPart		mPositionConstraintPart;
	RotationEulerConstraintPart	mRotationConstraintPart;
	AxisConstraintPart			mPositionLimitsConstraintPart;
	AxisConstraintPart			mMotorConstraintPart;
};

JPH_NAMESPACE_END
