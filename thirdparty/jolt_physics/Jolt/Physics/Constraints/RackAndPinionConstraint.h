// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/ConstraintPart/RackAndPinionConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Rack and pinion constraint (slider & gear) settings
class JPH_EXPORT RackAndPinionConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, RackAndPinionConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint.
	/// Body1 should be the pinion (gear) and body 2 the rack (slider).
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// Defines the ratio between the rotation of the pinion and the translation of the rack.
	/// The ratio is defined as: PinionRotation(t) = ratio * RackTranslation(t)
	/// @param inNumTeethRack Number of teeth that the rack has
	/// @param inRackLength Length of the rack
	/// @param inNumTeethPinion Number of teeth the pinion has
	void						SetRatio(int inNumTeethRack, float inRackLength, int inNumTeethPinion)
	{
		mRatio = 2.0f * JPH_PI * inNumTeethRack / (inRackLength * inNumTeethPinion);
	}

	/// This determines in which space the constraint is setup, all properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	/// Body 1 (pinion) constraint reference frame (space determined by mSpace).
	Vec3						mHingeAxis = Vec3::sAxisX();

	/// Body 2 (rack) constraint reference frame (space determined by mSpace)
	Vec3						mSliderAxis = Vec3::sAxisX();

	/// Ratio between the rack and pinion, see SetRatio.
	float						mRatio = 1.0f;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// A rack and pinion constraint constrains the rotation of body1 to the translation of body 2.
/// Note that this constraint needs to be used in conjunction with a hinge constraint for body 1 and a slider constraint for body 2.
class JPH_EXPORT RackAndPinionConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct gear constraint
								RackAndPinionConstraint(Body &inBody1, Body &inBody2, const RackAndPinionConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override												{ return EConstraintSubType::RackAndPinion; }
	virtual void				NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM) override { /* Nothing */ }
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
	virtual Mat44				GetConstraintToBody1Matrix() const override;
	virtual Mat44				GetConstraintToBody2Matrix() const override;

	/// The constraints that constrain the rack and pinion (a slider and a hinge), optional and used to calculate the position error and fix numerical drift.
	void						SetConstraints(const Constraint *inPinion, const Constraint *inRack)	{ mPinionConstraint = inPinion; mRackConstraint = inRack; }

	///@name Get Lagrange multiplier from last physics update (the linear/angular impulse applied to satisfy the constraint)
	inline float				GetTotalLambda() const													{ return mRackAndPinionConstraintPart.GetTotalLambda(); }

private:
	// Internal helper function to calculate the values below
	void						CalculateConstraintProperties(Mat44Arg inRotation1, Mat44Arg inRotation2);

	// CONFIGURATION PROPERTIES FOLLOW

	// Local space hinge axis
	Vec3						mLocalSpaceHingeAxis;

	// Local space sliding direction
	Vec3						mLocalSpaceSliderAxis;

	// Ratio between rack and pinion
	float						mRatio;

	// The constraints that constrain the rack and pinion (a slider and a hinge), optional and used to calculate the position error and fix numerical drift.
	RefConst<Constraint>		mPinionConstraint;
	RefConst<Constraint>		mRackConstraint;

	// RUN TIME PROPERTIES FOLLOW

	// World space hinge axis
	Vec3						mWorldSpaceHingeAxis;

	// World space sliding direction
	Vec3						mWorldSpaceSliderAxis;

	// The constraint parts
	RackAndPinionConstraintPart	mRackAndPinionConstraintPart;
};

JPH_NAMESPACE_END
