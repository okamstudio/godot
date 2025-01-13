// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/ConstraintPart/AxisConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Distance constraint settings, used to create a distance constraint
class JPH_EXPORT DistanceConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, DistanceConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// This determines in which space the constraint is setup, all properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	/// Body 1 constraint reference frame (space determined by mSpace).
	/// Constraint will keep mPoint1 (a point on body 1) and mPoint2 (a point on body 2) at the same distance.
	/// Note that this constraint can be used as a cheap PointConstraint by setting mPoint1 = mPoint2 (but this removes only 1 degree of freedom instead of 3).
	RVec3						mPoint1 = RVec3::sZero();

	/// Body 2 constraint reference frame (space determined by mSpace)
	RVec3						mPoint2 = RVec3::sZero();

	/// Ability to override the distance range at which the two points are kept apart. If the value is negative, it will be replaced by the distance between mPoint1 and mPoint2 (works only if mSpace is world space).
	float						mMinDistance = -1.0f;
	float						mMaxDistance = -1.0f;

	/// When enabled, this makes the limits soft. When the constraint exceeds the limits, a spring force will pull it back.
	SpringSettings				mLimitsSpringSettings;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// This constraint is a stiff spring that holds 2 points at a fixed distance from each other
class JPH_EXPORT DistanceConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct distance constraint
								DistanceConstraint(Body &inBody1, Body &inBody2, const DistanceConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override									{ return EConstraintSubType::Distance; }
	virtual void				NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM) override;
	virtual void				SetupVelocityConstraint(float inDeltaTime) override;
	virtual void				ResetWarmStart() override;
	virtual void				WarmStartVelocityConstraint(float inWarmStartImpulseRatio) override;
	virtual bool				SolveVelocityConstraint(float inDeltaTime) override;
	virtual bool				SolvePositionConstraint(float inDeltaTime, float inBaumgarte) override;
#ifdef JPH_DEBUG_RENDERER
	virtual void				DrawConstraint(DebugRenderer *inRenderer) const override;
#endif // JPH_DEBUG_RENDERER
	virtual void				SaveState(StateRecorder &inStream) const override;
	virtual void				RestoreState(StateRecorder &inStream) override;
	virtual Ref<ConstraintSettings> GetConstraintSettings() const override;

	// See: TwoBodyConstraint
	virtual Mat44				GetConstraintToBody1Matrix() const override					{ return Mat44::sTranslation(mLocalSpacePosition1); }
	virtual Mat44				GetConstraintToBody2Matrix() const override					{ return Mat44::sTranslation(mLocalSpacePosition2); } // Note: Incorrect rotation as we don't track the original rotation difference, should not matter though as the constraint is not limiting rotation.

	/// Update the minimum and maximum distance for the constraint
	void						SetDistance(float inMinDistance, float inMaxDistance)		{ JPH_ASSERT(inMinDistance <= inMaxDistance); mMinDistance = inMinDistance; mMaxDistance = inMaxDistance; }
	float						GetMinDistance() const										{ return mMinDistance; }
	float						GetMaxDistance() const										{ return mMaxDistance; }

	/// Update the limits spring settings
	const SpringSettings &		GetLimitsSpringSettings() const								{ return mLimitsSpringSettings; }
	SpringSettings &			GetLimitsSpringSettings()									{ return mLimitsSpringSettings; }
	void						SetLimitsSpringSettings(const SpringSettings &inLimitsSpringSettings) { mLimitsSpringSettings = inLimitsSpringSettings; }

	///@name Get Lagrange multiplier from last physics update (the linear impulse applied to satisfy the constraint)
	inline float				GetTotalLambdaPosition() const								{ return mAxisConstraint.GetTotalLambda(); }

private:
	// Internal helper function to calculate the values below
	void						CalculateConstraintProperties(float inDeltaTime);

	// CONFIGURATION PROPERTIES FOLLOW

	// Local space constraint positions
	Vec3						mLocalSpacePosition1;
	Vec3						mLocalSpacePosition2;

	// Min/max distance that must be kept between the world space points
	float						mMinDistance;
	float						mMaxDistance;

	// Soft constraint limits
	SpringSettings				mLimitsSpringSettings;

	// RUN TIME PROPERTIES FOLLOW

	// World space positions and normal
	RVec3						mWorldSpacePosition1;
	RVec3						mWorldSpacePosition2;
	Vec3						mWorldSpaceNormal;

	// Depending on if the distance < min or distance > max we can apply forces to prevent further violations
	float						mMinLambda;
	float						mMaxLambda;

	// The constraint part
	AxisConstraintPart			mAxisConstraint;
};

JPH_NAMESPACE_END
