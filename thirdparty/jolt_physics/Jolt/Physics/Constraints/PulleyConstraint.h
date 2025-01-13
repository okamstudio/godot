// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Constraints/ConstraintPart/IndependentAxisConstraintPart.h>

JPH_NAMESPACE_BEGIN

/// Pulley constraint settings, used to create a pulley constraint.
/// A pulley connects two bodies via two fixed world points to each other similar to a distance constraint.
/// We define Length1 = |BodyPoint1 - FixedPoint1| where Body1 is a point on body 1 in world space and FixedPoint1 a fixed point in world space
/// Length2 = |BodyPoint2 - FixedPoint2|
/// The constraint keeps the two line segments constrained so that
/// MinDistance <= Length1 + Ratio * Length2 <= MaxDistance
class JPH_EXPORT PulleyConstraintSettings final : public TwoBodyConstraintSettings
{
	JPH_DECLARE_SERIALIZABLE_VIRTUAL(JPH_EXPORT, PulleyConstraintSettings)

public:
	// See: ConstraintSettings::SaveBinaryState
	virtual void				SaveBinaryState(StreamOut &inStream) const override;

	/// Create an instance of this constraint
	virtual TwoBodyConstraint *	Create(Body &inBody1, Body &inBody2) const override;

	/// This determines in which space the constraint is setup, specified properties below should be in the specified space
	EConstraintSpace			mSpace = EConstraintSpace::WorldSpace;

	/// Body 1 constraint attachment point (space determined by mSpace).
	RVec3						mBodyPoint1 = RVec3::sZero();

	/// Fixed world point to which body 1 is connected (always world space)
	RVec3						mFixedPoint1 = RVec3::sZero();

	/// Body 2 constraint attachment point (space determined by mSpace)
	RVec3						mBodyPoint2 = RVec3::sZero();

	/// Fixed world point to which body 2 is connected (always world space)
	RVec3						mFixedPoint2 = RVec3::sZero();

	/// Ratio between the two line segments (see formula above), can be used to create a block and tackle
	float						mRatio = 1.0f;

	/// The minimum length of the line segments (see formula above), use -1 to calculate the length based on the positions of the objects when the constraint is created.
	float						mMinLength = 0.0f;

	/// The maximum length of the line segments (see formula above), use -1 to calculate the length based on the positions of the objects when the constraint is created.
	float						mMaxLength = -1.0f;

protected:
	// See: ConstraintSettings::RestoreBinaryState
	virtual void				RestoreBinaryState(StreamIn &inStream) override;
};

/// A pulley constraint.
class JPH_EXPORT PulleyConstraint final : public TwoBodyConstraint
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Construct distance constraint
								PulleyConstraint(Body &inBody1, Body &inBody2, const PulleyConstraintSettings &inSettings);

	// Generic interface of a constraint
	virtual EConstraintSubType	GetSubType() const override									{ return EConstraintSubType::Pulley; }
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

	/// Update the minimum and maximum length for the constraint
	void						SetLength(float inMinLength, float inMaxLength)				{ JPH_ASSERT(inMinLength >= 0.0f && inMinLength <= inMaxLength); mMinLength = inMinLength; mMaxLength = inMaxLength; }
	float						GetMinLength() const										{ return mMinLength; }
	float						GetMaxLength() const										{ return mMaxLength; }

	/// Get the current length of both segments (multiplied by the ratio for segment 2)
	float						GetCurrentLength() const									{ return Vec3(mWorldSpacePosition1 - mFixedPosition1).Length() + mRatio * Vec3(mWorldSpacePosition2 - mFixedPosition2).Length(); }

	///@name Get Lagrange multiplier from last physics update (the linear impulse applied to satisfy the constraint)
	inline float				GetTotalLambdaPosition() const								{ return mIndependentAxisConstraintPart.GetTotalLambda(); }

private:
	// Calculates world positions and normals and returns current length
	float						CalculatePositionsNormalsAndLength();

	// Internal helper function to calculate the values below
	void						CalculateConstraintProperties();

	// CONFIGURATION PROPERTIES FOLLOW

	// Local space constraint positions on the bodies
	Vec3						mLocalSpacePosition1;
	Vec3						mLocalSpacePosition2;

	// World space fixed positions
	RVec3						mFixedPosition1;
	RVec3						mFixedPosition2;

	/// Ratio between the two line segments
	float						mRatio;

	// The minimum/maximum length of the line segments
	float						mMinLength;
	float						mMaxLength;

	// RUN TIME PROPERTIES FOLLOW

	// World space positions and normal
	RVec3						mWorldSpacePosition1;
	RVec3						mWorldSpacePosition2;
	Vec3						mWorldSpaceNormal1;
	Vec3						mWorldSpaceNormal2;

	// Depending on if the length < min or length > max we can apply forces to prevent further violations
	float						mMinLambda;
	float						mMaxLambda;

	// The constraint part
	IndependentAxisConstraintPart mIndependentAxisConstraintPart;
};

JPH_NAMESPACE_END
