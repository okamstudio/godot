// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/ConstraintPart/PointConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Point constraint settings, used to create a point constraint
class JPH_EXPORT PointConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, PointConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// This determines in which space the constraint is setup, all properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	/// Body 1 constraint position (space determined by mSpace).
	RVec3						mPoint1 = RVec3::sZero();

	/// Body 2 constraint position (space determined by mSpace).
	/// Note: Normally you would set mPoint1 = mPoint2 if the bodies are already placed how you want to constrain them (if mSpace = world space).
	RVec3						mPoint2 = RVec3::sZero();

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// A point constraint constrains 2 bodies on a single point (removing 3 degrees of freedom)
class JPH_EXPORT PointConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct point constraint
								PointConstraint(Body &inBody1, Body &inBody2, const PointConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override									{ return EConstraintSubType::Point; }
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

	/// Update the attachment point for body 1
	void						SetPoint1(EConstraintSpace inSpace, RVec3Arg inPoint1);

	/// Update the attachment point for body 2
	void						SetPoint2(EConstraintSpace inSpace, RVec3Arg inPoint2);

	/// Get the attachment point for body 1 relative to body 1 COM (transform by Body::GetCenterOfMassTransform to take to world space)
	inline Vec3					GetLocalSpacePoint1() const									{ return mLocalSpacePosition1; }

	/// Get the attachment point for body 2 relative to body 2 COM (transform by Body::GetCenterOfMassTransform to take to world space)
	inline Vec3					GetLocalSpacePoint2() const									{ return mLocalSpacePosition2; }

	// See: TwoBodyConstraint
	virtual Mat44				GetConstraintToBody1Matrix() const override					{ return Mat44::sTranslation(mLocalSpacePosition1); }
	virtual Mat44				GetConstraintToBody2Matrix() const override					{ return Mat44::sTranslation(mLocalSpacePosition2); } // Note: Incorrect rotation as we don't track the original rotation difference, should not matter though as the constraint is not limiting rotation.

	///@name Get Lagrange multiplier from last physics update (the linear impulse applied to satisfy the constraint)
	inline Vec3					GetTotalLambdaPosition() const								{ return mPointConstraintPart.GetTotalLambda(); }

private:
	// Internal helper function to calculate the values below
	void						CalculateConstraintProperties();

	// Local space constraint positions
	Vec3						mLocalSpacePosition1;
	Vec3						mLocalSpacePosition2;

	// The constraint part
	PointConstraintPart			mPointConstraintPart;
};

JPH_NAMESPACE_END
