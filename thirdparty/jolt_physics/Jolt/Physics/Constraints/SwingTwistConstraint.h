// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Constraints/ConstraintPart/PointConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AngleConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/SwingTwistConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Swing twist constraint settings, used to create a swing twist constraint
/// All values in this structure are copied to the swing twist constraint and the settings object is no longer needed afterwards.
///
/// This image describes the limit settings:
/// @image html Docs/SwingTwistConstraint.png
class JPH_EXPORT SwingTwistConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, SwingTwistConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// This determines in which space the constraint is setup, all properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	///@name Body 1 constraint reference frame (space determined by mSpace)
	RVec3						mPosition1 = RVec3::sZero();
	Vec3						mTwistAxis1 = Vec3::sAxisX();
	Vec3						mPlaneAxis1 = Vec3::sAxisY();

	///@name Body 2 constraint reference frame (space determined by mSpace)
	RVec3						mPosition2 = RVec3::sZero();
	Vec3						mTwistAxis2 = Vec3::sAxisX();
	Vec3						mPlaneAxis2 = Vec3::sAxisY();

	/// The type of swing constraint that we want to use.
	ESwingType					mSwingType = ESwingType::Cone;

	///@name Swing rotation limits
	float						mNormalHalfConeAngle = 0.0f;								///< See image at Detailed Description. Angle in radians.
	float						mPlaneHalfConeAngle = 0.0f;									///< See image at Detailed Description. Angle in radians.

	///@name Twist rotation limits
	float						mTwistMinAngle = 0.0f;										///< See image at Detailed Description. Angle in radians. Should be \f$\in [-\pi, \pi]\f$.
	float						mTwistMaxAngle = 0.0f;										///< See image at Detailed Description. Angle in radians. Should be \f$\in [-\pi, \pi]\f$.

	///@name Friction
	float						mMaxFrictionTorque = 0.0f;									///< Maximum amount of torque (N m) to apply as friction when the constraint is not powered by a motor

	///@name In case the constraint is powered, this determines the motor settings around the swing and twist axis
	MotorSettings				mSwingMotorSettings;
	MotorSettings				mTwistMotorSettings;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// A swing twist constraint is a specialized constraint for humanoid ragdolls that allows limited rotation only
///
/// @see SwingTwistConstraintSettings for a description of the limits
class JPH_EXPORT SwingTwistConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct swing twist constraint
								SwingTwistConstraint(Body &inBody1, Body &inBody2, const SwingTwistConstraintSettings &inSettings);

	///@name Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override									{ return EConstraintSubType::SwingTwist; }
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
	virtual Mat44				GetConstraintToBody1Matrix() const override					{ return Mat44::sRotationTranslation(mConstraintToBody1, mLocalSpacePosition1); }
	virtual Mat44				GetConstraintToBody2Matrix() const override					{ return Mat44::sRotationTranslation(mConstraintToBody2, mLocalSpacePosition2); }

	///@name Constraint reference frame
	inline Vec3					GetLocalSpacePosition1() const								{ return mLocalSpacePosition1; }
	inline Vec3					GetLocalSpacePosition2() const								{ return mLocalSpacePosition2; }
	inline Quat					GetConstraintToBody1() const								{ return mConstraintToBody1; }
	inline Quat					GetConstraintToBody2() const								{ return mConstraintToBody2; }

	///@name Constraint limits
	inline float				GetNormalHalfConeAngle() const								{ return mNormalHalfConeAngle; }
	inline void					SetNormalHalfConeAngle(float inAngle)						{ mNormalHalfConeAngle = inAngle; UpdateLimits(); }
	inline float				GetPlaneHalfConeAngle() const								{ return mPlaneHalfConeAngle; }
	inline void					SetPlaneHalfConeAngle(float inAngle)						{ mPlaneHalfConeAngle = inAngle; UpdateLimits(); }
	inline float				GetTwistMinAngle() const									{ return mTwistMinAngle; }
	inline void					SetTwistMinAngle(float inAngle)								{ mTwistMinAngle = inAngle; UpdateLimits(); }
	inline float				GetTwistMaxAngle() const									{ return mTwistMaxAngle; }
	inline void					SetTwistMaxAngle(float inAngle)								{ mTwistMaxAngle = inAngle; UpdateLimits(); }

	///@name Motor settings
	const MotorSettings &		GetSwingMotorSettings() const								{ return mSwingMotorSettings; }
	MotorSettings &				GetSwingMotorSettings()										{ return mSwingMotorSettings; }
	const MotorSettings &		GetTwistMotorSettings() const								{ return mTwistMotorSettings; }
	MotorSettings &				GetTwistMotorSettings()										{ return mTwistMotorSettings; }

	///@name Friction control
	void						SetMaxFrictionTorque(float inFrictionTorque)				{ mMaxFrictionTorque = inFrictionTorque; }
	float						GetMaxFrictionTorque() const								{ return mMaxFrictionTorque; }

	///@name Motor controls

	/// Controls if the motors are on or off
	void						SetSwingMotorState(EMotorState inState);
	EMotorState					GetSwingMotorState() const									{ return mSwingMotorState; }
	void						SetTwistMotorState(EMotorState inState);
	EMotorState					GetTwistMotorState() const									{ return mTwistMotorState; }

	/// Set the target angular velocity of body 2 in constraint space of body 2
	void						SetTargetAngularVelocityCS(Vec3Arg inAngularVelocity)		{ mTargetAngularVelocity = inAngularVelocity; }
	Vec3						GetTargetAngularVelocityCS() const							{ return mTargetAngularVelocity; }

	/// Set the target orientation in constraint space (drives constraint to: GetRotationInConstraintSpace() == inOrientation)
	void						SetTargetOrientationCS(QuatArg inOrientation);
	Quat						GetTargetOrientationCS() const								{ return mTargetOrientation; }

	/// Set the target orientation in body space (R2 = R1 * inOrientation, where R1 and R2 are the world space rotations for body 1 and 2).
	/// Solve: R2 * ConstraintToBody2 = R1 * ConstraintToBody1 * q (see SwingTwistConstraint::GetSwingTwist) and R2 = R1 * inOrientation for q.
	void						SetTargetOrientationBS(QuatArg inOrientation)				{ SetTargetOrientationCS(mConstraintToBody1.Conjugated() * inOrientation * mConstraintToBody2); }

	/// Get current rotation of constraint in constraint space.
	/// Solve: R2 * ConstraintToBody2 = R1 * ConstraintToBody1 * q for q.
	Quat						GetRotationInConstraintSpace() const;

	///@name Get Lagrange multiplier from last physics update (the linear/angular impulse applied to satisfy the constraint)
	inline Vec3					GetTotalLambdaPosition() const								{ return mPointConstraintPart.GetTotalLambda(); }
	inline float				GetTotalLambdaTwist() const									{ return mSwingTwistConstraintPart.GetTotalTwistLambda(); }
	inline float				GetTotalLambdaSwingY() const								{ return mSwingTwistConstraintPart.GetTotalSwingYLambda(); }
	inline float				GetTotalLambdaSwingZ() const								{ return mSwingTwistConstraintPart.GetTotalSwingZLambda(); }
	inline Vec3					GetTotalLambdaMotor() const									{ return Vec3(mMotorConstraintPart[0].GetTotalLambda(), mMotorConstraintPart[1].GetTotalLambda(), mMotorConstraintPart[2].GetTotalLambda()); }

private:
	// Update the limits in the swing twist constraint part
	void						UpdateLimits();

	// CONFIGURATION PROPERTIES FOLLOW

	// Local space constraint positions
	Vec3						mLocalSpacePosition1;
	Vec3						mLocalSpacePosition2;

	// Transforms from constraint space to body space
	Quat						mConstraintToBody1;
	Quat						mConstraintToBody2;

	// Limits
	float						mNormalHalfConeAngle;
	float						mPlaneHalfConeAngle;
	float						mTwistMinAngle;
	float						mTwistMaxAngle;

	// Friction
	float						mMaxFrictionTorque;

	// Motor controls
	MotorSettings				mSwingMotorSettings;
	MotorSettings				mTwistMotorSettings;
	EMotorState					mSwingMotorState = EMotorState::Off;
	EMotorState					mTwistMotorState = EMotorState::Off;
	Vec3						mTargetAngularVelocity = Vec3::sZero();
	Quat						mTargetOrientation = Quat::sIdentity();

	// RUN TIME PROPERTIES FOLLOW

	// Rotation axis for motor constraint parts
	Vec3						mWorldSpaceMotorAxis[3];

	// The constraint parts
	PointConstraintPart			mPointConstraintPart;
	SwingTwistConstraintPart	mSwingTwistConstraintPart;
	AngleConstraintPart			mMotorConstraintPart[3];
};

JPH_NAMESPACE_END
