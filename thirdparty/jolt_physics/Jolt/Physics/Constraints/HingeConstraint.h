// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/ConstraintPart/PointConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/HingeRotationConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AngleConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Hinge constraint settings, used to create a hinge constraint
class JPH_EXPORT HingeConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, HingeConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// This determines in which space the constraint is setup, all properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	/// Body 1 constraint reference frame (space determined by mSpace).
	/// Hinge axis is the axis where rotation is allowed.
	/// When the normal axis of both bodies align in world space, the hinge angle is defined to be 0.
	/// mHingeAxis1 and mNormalAxis1 should be perpendicular. mHingeAxis2 and mNormalAxis2 should also be perpendicular.
	/// If you configure the joint in world space and create both bodies with a relative rotation you want to be defined as zero,
	/// you can simply set mHingeAxis1 = mHingeAxis2 and mNormalAxis1 = mNormalAxis2.
	RVec3						mPoint1 = RVec3::sZero();
	Vec3						mHingeAxis1 = Vec3::sAxisY();
	Vec3						mNormalAxis1 = Vec3::sAxisX();

	/// Body 2 constraint reference frame (space determined by mSpace)
	RVec3						mPoint2 = RVec3::sZero();
	Vec3						mHingeAxis2 = Vec3::sAxisY();
	Vec3						mNormalAxis2 = Vec3::sAxisX();

	/// Rotation around the hinge axis will be limited between [mLimitsMin, mLimitsMax] where mLimitsMin e [-pi, 0] and mLimitsMax e [0, pi].
	/// Both angles are in radians.
	float						mLimitsMin = -JPH_PI;
	float						mLimitsMax = JPH_PI;

	/// When enabled, this makes the limits soft. When the constraint exceeds the limits, a spring force will pull it back.
	SpringSettings				mLimitsSpringSettings;

	/// Maximum amount of torque (N m) to apply as friction when the constraint is not powered by a motor
	float						mMaxFrictionTorque = 0.0f;

	/// In case the constraint is powered, this determines the motor settings around the hinge axis
	MotorSettings				mMotorSettings;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// A hinge constraint constrains 2 bodies on a single point and allows only a single axis of rotation
class JPH_EXPORT HingeConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct hinge constraint
								HingeConstraint(Body &inBody1, Body &inBody2, const HingeConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override								{ return EConstraintSubType::Hinge; }
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

	/// Get the attachment point for body 1 relative to body 1 COM (transform by Body::GetCenterOfMassTransform to take to world space)
	inline Vec3					GetLocalSpacePoint1() const								{ return mLocalSpacePosition1; }

	/// Get the attachment point for body 2 relative to body 2 COM (transform by Body::GetCenterOfMassTransform to take to world space)
	inline Vec3					GetLocalSpacePoint2() const								{ return mLocalSpacePosition2; }

	// Local space hinge directions (transform direction by Body::GetCenterOfMassTransform to take to world space)
	Vec3						GetLocalSpaceHingeAxis1() const							{ return mLocalSpaceHingeAxis1; }
	Vec3						GetLocalSpaceHingeAxis2() const							{ return mLocalSpaceHingeAxis2; }

	// Local space normal directions (transform direction by Body::GetCenterOfMassTransform to take to world space)
	Vec3						GetLocalSpaceNormalAxis1() const						{ return mLocalSpaceNormalAxis1; }
	Vec3						GetLocalSpaceNormalAxis2() const						{ return mLocalSpaceNormalAxis2; }

	/// Get the current rotation angle from the rest position
	float						GetCurrentAngle() const;

	// Friction control
	void						SetMaxFrictionTorque(float inFrictionTorque)			{ mMaxFrictionTorque = inFrictionTorque; }
	float						GetMaxFrictionTorque() const							{ return mMaxFrictionTorque; }

	// Motor settings
	MotorSettings &				GetMotorSettings()										{ return mMotorSettings; }
	const MotorSettings &		GetMotorSettings() const								{ return mMotorSettings; }

	// Motor controls
	void						SetMotorState(EMotorState inState)						{ JPH_ASSERT(inState == EMotorState::Off || mMotorSettings.IsValid()); mMotorState = inState; }
	EMotorState					GetMotorState() const									{ return mMotorState; }
	void						SetTargetAngularVelocity(float inAngularVelocity)		{ mTargetAngularVelocity = inAngularVelocity; } ///< rad/s
	float						GetTargetAngularVelocity() const						{ return mTargetAngularVelocity; }
	void						SetTargetAngle(float inAngle)							{ mTargetAngle = mHasLimits? Clamp(inAngle, mLimitsMin, mLimitsMax) : inAngle; } ///< rad
	float						GetTargetAngle() const									{ return mTargetAngle; }

	/// Update the rotation limits of the hinge, value in radians (see HingeConstraintSettings)
	void						SetLimits(float inLimitsMin, float inLimitsMax);
	float						GetLimitsMin() const									{ return mLimitsMin; }
	float						GetLimitsMax() const									{ return mLimitsMax; }
	bool						HasLimits() const										{ return mHasLimits; }

	/// Update the limits spring settings
	const SpringSettings &		GetLimitsSpringSettings() const							{ return mLimitsSpringSettings; }
	SpringSettings &			GetLimitsSpringSettings()								{ return mLimitsSpringSettings; }
	void						SetLimitsSpringSettings(const SpringSettings &inLimitsSpringSettings) { mLimitsSpringSettings = inLimitsSpringSettings; }

	///@name Get Lagrange multiplier from last physics update (the linear/angular impulse applied to satisfy the constraint)
	inline Vec3					GetTotalLambdaPosition() const							{ return mPointConstraintPart.GetTotalLambda(); }
	inline Vector<2>			GetTotalLambdaRotation() const							{ return mRotationConstraintPart.GetTotalLambda(); }
	inline float				GetTotalLambdaRotationLimits() const					{ return mRotationLimitsConstraintPart.GetTotalLambda(); }
	inline float				GetTotalLambdaMotor() const								{ return mMotorConstraintPart.GetTotalLambda(); }

private:
	// Internal helper function to calculate the values below
	void						CalculateA1AndTheta();
	void						CalculateRotationLimitsConstraintProperties(float inDeltaTime);
	void						CalculateMotorConstraintProperties(float inDeltaTime);
	inline float				GetSmallestAngleToLimit() const;
	inline bool					IsMinLimitClosest() const;

	// CONFIGURATION PROPERTIES FOLLOW

	// Local space constraint positions
	Vec3						mLocalSpacePosition1;
	Vec3						mLocalSpacePosition2;

	// Local space hinge directions
	Vec3						mLocalSpaceHingeAxis1;
	Vec3						mLocalSpaceHingeAxis2;

	// Local space normal direction (direction relative to which to draw constraint limits)
	Vec3						mLocalSpaceNormalAxis1;
	Vec3						mLocalSpaceNormalAxis2;

	// Inverse of initial relative orientation between bodies (which defines hinge angle = 0)
	Quat						mInvInitialOrientation;

	// Hinge limits
	bool						mHasLimits;
	float						mLimitsMin;
	float						mLimitsMax;

	// Soft constraint limits
	SpringSettings				mLimitsSpringSettings;

	// Friction
	float						mMaxFrictionTorque;

	// Motor controls
	MotorSettings				mMotorSettings;
	EMotorState					mMotorState = EMotorState::Off;
	float						mTargetAngularVelocity = 0.0f;
	float						mTargetAngle = 0.0f;

	// RUN TIME PROPERTIES FOLLOW

	// Current rotation around the hinge axis
	float						mTheta = 0.0f;

	// World space hinge axis for body 1
	Vec3						mA1;

	// The constraint parts
	PointConstraintPart			mPointConstraintPart;
	HingeRotationConstraintPart mRotationConstraintPart;
	AngleConstraintPart			mRotationLimitsConstraintPart;
	AngleConstraintPart			mMotorConstraintPart;
};

JPH_NAMESPACE_END
