// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/ConstraintPart/GearConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Gear constraint settings
class JPH_EXPORT GearConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, GearConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint.
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// Defines the ratio between the rotation of both gears
	/// The ratio is defined as: Gear1Rotation(t) = -ratio * Gear2Rotation(t)
	/// @param inNumTeethGear1 Number of teeth that body 1 has
	/// @param inNumTeethGear2 Number of teeth that body 2 has
	void						SetRatio(int inNumTeethGear1, int inNumTeethGear2)
	{
		mRatio = float(inNumTeethGear2) / float(inNumTeethGear1);
	}

	/// This determines in which space the constraint is setup, all properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	/// Body 1 constraint reference frame (space determined by mSpace).
	Vec3						mHingeAxis1 = Vec3::sAxisX();

	/// Body 2 constraint reference frame (space determined by mSpace)
	Vec3						mHingeAxis2 = Vec3::sAxisX();

	/// Ratio between both gears, see SetRatio.
	float						mRatio = 1.0f;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// A gear constraint constrains the rotation of body1 to the rotation of body 2 using a gear.
/// Note that this constraint needs to be used in conjunction with a two hinge constraints.
class JPH_EXPORT GearConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct gear constraint
								GearConstraint(Body &inBody1, Body &inBody2, const GearConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override								{ return EConstraintSubType::Gear; }
	virtual void				NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM) override { /* Do nothing */ }
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

	/// The constraints that constrain both gears (2 hinges), optional and used to calculate the rotation error and fix numerical drift.
	void						SetConstraints(const Constraint *inGear1, const Constraint *inGear2)	{ mGear1Constraint = inGear1; mGear2Constraint = inGear2; }

	///@name Get Lagrange multiplier from last physics update (the angular impulse applied to satisfy the constraint)
	inline float				GetTotalLambda() const									{ return mGearConstraintPart.GetTotalLambda(); }

private:
	// Internal helper function to calculate the values below
	void						CalculateConstraintProperties(Mat44Arg inRotation1, Mat44Arg inRotation2);

	// CONFIGURATION PROPERTIES FOLLOW

	// Local space hinge axis for body 1
	Vec3						mLocalSpaceHingeAxis1;

	// Local space hinge axis for body 2
	Vec3						mLocalSpaceHingeAxis2;

	// Ratio between gear 1 and 2
	float						mRatio;

	// The constraints that constrain both gears (2 hinges), optional and used to calculate the rotation error and fix numerical drift.
	RefConst<Constraint>		mGear1Constraint;
	RefConst<Constraint>		mGear2Constraint;

	// RUN TIME PROPERTIES FOLLOW

	// World space hinge axis for body 1
	Vec3						mWorldSpaceHingeAxis1;

	// World space hinge axis for body 2
	Vec3						mWorldSpaceHingeAxis2;

	// The constraint parts
	GearConstraintPart			mGearConstraintPart;
};

JPH_NAMESPACE_END
