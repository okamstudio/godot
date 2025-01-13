// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/ConstraintPart/PointConstraintPart.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AngleConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Cone constraint settings, used to create a cone constraint
class JPH_EXPORT ConeConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, ConeConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// This determines in which space the constraint is setup, all properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	/// Body 1 constraint reference frame (space determined by mSpace)
	RVec3						mPoint1 = RVec3::sZero();
	Vec3						mTwistAxis1 = Vec3::sAxisX();

	/// Body 2 constraint reference frame (space determined by mSpace)
	RVec3						mPoint2 = RVec3::sZero();
	Vec3						mTwistAxis2 = Vec3::sAxisX();

	/// Half of maximum angle between twist axis of body 1 and 2
	float						mHalfConeAngle = 0.0f;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// A cone constraint constraints 2 bodies to a single point and limits the swing between the twist axis within a cone:
///
/// t1 . t2 <= cos(theta)
///
/// Where:
///
/// t1 = twist axis of body 1.
/// t2 = twist axis of body 2.
/// theta = half cone angle (angle from the principal axis of the cone to the edge).
///
/// Calculating the Jacobian:
///
/// Constraint equation:
///
/// C = t1 . t2 - cos(theta)
///
/// Derivative:
///
/// d/dt C = d/dt (t1 . t2) = (d/dt t1) . t2 + t1 . (d/dt t2) = (w1 x t1) . t2 + t1 . (w2 x t2) = (t1 x t2) . w1 + (t2 x t1) . w2
///
/// d/dt C = J v = [0, -t2 x t1, 0, t2 x t1] [v1, w1, v2, w2]
///
/// Where J is the Jacobian.
///
/// Note that this is the exact same equation as used in AngleConstraintPart if we use t2 x t1 as the world space axis
class JPH_EXPORT ConeConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct cone constraint
								ConeConstraint(Body &inBody1, Body &inBody2, const ConeConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override					{ return EConstraintSubType::Cone; }
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

	/// Update maximum angle between body 1 and 2 (see ConeConstraintSettings)
	void						SetHalfConeAngle(float inHalfConeAngle)		{ JPH_ASSERT(inHalfConeAngle >= 0.0f && inHalfConeAngle <= JPH_PI); mCosHalfConeAngle = Cos(inHalfConeAngle); }
	float						GetCosHalfConeAngle() const					{ return mCosHalfConeAngle; }

	///@name Get Lagrange multiplier from last physics update (the linear/angular impulse applied to satisfy the constraint)
	inline Vec3					GetTotalLambdaPosition() const				{ return mPointConstraintPart.GetTotalLambda(); }
	inline float				GetTotalLambdaRotation() const				{ return mAngleConstraintPart.GetTotalLambda(); }

private:
	// Internal helper function to calculate the values below
	void						CalculateRotationConstraintProperties(Mat44Arg inRotation1, Mat44Arg inRotation2);

	// CONFIGURATION PROPERTIES FOLLOW

	// Local space constraint positions
	Vec3						mLocalSpacePosition1;
	Vec3						mLocalSpacePosition2;

	// Local space constraint axis
	Vec3						mLocalSpaceTwistAxis1;
	Vec3						mLocalSpaceTwistAxis2;

	// Angular limits
	float						mCosHalfConeAngle;

	// RUN TIME PROPERTIES FOLLOW

	// Axis and angle of rotation between the two bodies
	Vec3						mWorldSpaceRotationAxis;
	float						mCosTheta;

	// The constraint parts
	PointConstraintPart			mPointConstraintPart;
	AngleConstraintPart			mAngleConstraintPart;
};

JPH_NAMESPACE_END
