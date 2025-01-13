// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

using namespace literals;

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(PulleyConstraintSettings)
{
	JPH_ADD_BASE_CLASS(PulleyConstraintSettings, TwoBodyConstraintSettings)

	JPH_ADD_ENUM_ATTRIBUTE(PulleyConstraintSettings, mSpace)
	JPH_ADD_ATTRIBUTE(PulleyConstraintSettings, mBodyPoint1)
	JPH_ADD_ATTRIBUTE(PulleyConstraintSettings, mFixedPoint1)
	JPH_ADD_ATTRIBUTE(PulleyConstraintSettings, mBodyPoint2)
	JPH_ADD_ATTRIBUTE(PulleyConstraintSettings, mFixedPoint2)
	JPH_ADD_ATTRIBUTE(PulleyConstraintSettings, mRatio)
	JPH_ADD_ATTRIBUTE(PulleyConstraintSettings, mMinLength)
	JPH_ADD_ATTRIBUTE(PulleyConstraintSettings, mMaxLength)
}

void PulleyConstraintSettings::SaveBinaryState(StreamOut &inStream) const
{
	ConstraintSettings::SaveBinaryState(inStream);

	inStream.Write(mSpace);
	inStream.Write(mBodyPoint1);
	inStream.Write(mFixedPoint1);
	inStream.Write(mBodyPoint2);
	inStream.Write(mFixedPoint2);
	inStream.Write(mRatio);
	inStream.Write(mMinLength);
	inStream.Write(mMaxLength);
}

void PulleyConstraintSettings::RestoreBinaryState(StreamIn &inStream)
{
	ConstraintSettings::RestoreBinaryState(inStream);

	inStream.Read(mSpace);
	inStream.Read(mBodyPoint1);
	inStream.Read(mFixedPoint1);
	inStream.Read(mBodyPoint2);
	inStream.Read(mFixedPoint2);
	inStream.Read(mRatio);
	inStream.Read(mMinLength);
	inStream.Read(mMaxLength);
}

TwoBodyConstraint *PulleyConstraintSettings::Create(Body &inBody1, Body &inBody2) const
{
	return new PulleyConstraint(inBody1, inBody2, *this);
}

PulleyConstraint::PulleyConstraint(Body &inBody1, Body &inBody2, const PulleyConstraintSettings &inSettings) :
	TwoBodyConstraint(inBody1, inBody2, inSettings),
	mFixedPosition1(inSettings.mFixedPoint1),
	mFixedPosition2(inSettings.mFixedPoint2),
	mRatio(inSettings.mRatio),
	mMinLength(inSettings.mMinLength),
	mMaxLength(inSettings.mMaxLength)
{
	if (inSettings.mSpace == EConstraintSpace::WorldSpace)
	{
		// If all properties were specified in world space, take them to local space now
		mLocalSpacePosition1 = Vec3(inBody1.GetInverseCenterOfMassTransform() * inSettings.mBodyPoint1);
		mLocalSpacePosition2 = Vec3(inBody2.GetInverseCenterOfMassTransform() * inSettings.mBodyPoint2);
		mWorldSpacePosition1 = inSettings.mBodyPoint1;
		mWorldSpacePosition2 = inSettings.mBodyPoint2;
	}
	else
	{
		// If properties were specified in local space, we need to calculate world space positions
		mLocalSpacePosition1 = Vec3(inSettings.mBodyPoint1);
		mLocalSpacePosition2 = Vec3(inSettings.mBodyPoint2);
		mWorldSpacePosition1 = inBody1.GetCenterOfMassTransform() * inSettings.mBodyPoint1;
		mWorldSpacePosition2 = inBody2.GetCenterOfMassTransform() * inSettings.mBodyPoint2;
	}

	// Calculate min/max length if it was not provided
	float current_length = GetCurrentLength();
	if (mMinLength < 0.0f)
		mMinLength = current_length;
	if (mMaxLength < 0.0f)
		mMaxLength = current_length;

	// Initialize the normals to a likely valid axis in case the fixed points overlap with the attachment points (most likely the fixed points are above both bodies)
	mWorldSpaceNormal1 = mWorldSpaceNormal2 = -Vec3::sAxisY();
}

void PulleyConstraint::NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM)
{
	if (mBody1->GetID() == inBodyID)
		mLocalSpacePosition1 -= inDeltaCOM;
	else if (mBody2->GetID() == inBodyID)
		mLocalSpacePosition2 -= inDeltaCOM;
}

float PulleyConstraint::CalculatePositionsNormalsAndLength()
{
	// Update world space positions (the bodies may have moved)
	mWorldSpacePosition1 = mBody1->GetCenterOfMassTransform() * mLocalSpacePosition1;
	mWorldSpacePosition2 = mBody2->GetCenterOfMassTransform() * mLocalSpacePosition2;

	// Calculate world space normals
	Vec3 delta1 = Vec3(mWorldSpacePosition1 - mFixedPosition1);
	float delta1_len = delta1.Length();
	if (delta1_len > 0.0f)
		mWorldSpaceNormal1 = delta1 / delta1_len;

	Vec3 delta2 = Vec3(mWorldSpacePosition2 - mFixedPosition2);
	float delta2_len = delta2.Length();
	if (delta2_len > 0.0f)
		mWorldSpaceNormal2 = delta2 / delta2_len;

	// Calculate length
	return delta1_len + mRatio * delta2_len;
}

void PulleyConstraint::CalculateConstraintProperties()
{
	// Calculate attachment points relative to COM
	Vec3 r1 = Vec3(mWorldSpacePosition1 - mBody1->GetCenterOfMassPosition());
	Vec3 r2 = Vec3(mWorldSpacePosition2 - mBody2->GetCenterOfMassPosition());

	mIndependentAxisConstraintPart.CalculateConstraintProperties(*mBody1, *mBody2, r1, mWorldSpaceNormal1, r2, mWorldSpaceNormal2, mRatio);
}

void PulleyConstraint::SetupVelocityConstraint(float inDeltaTime)
{
	// Determine if the constraint is active
	float current_length = CalculatePositionsNormalsAndLength();
	bool min_length_violation = current_length <= mMinLength;
	bool max_length_violation = current_length >= mMaxLength;
	if (min_length_violation || max_length_violation)
	{
		// Determine max lambda based on if the length is too big or small
		mMinLambda = max_length_violation? -FLT_MAX : 0.0f;
		mMaxLambda = min_length_violation? FLT_MAX : 0.0f;

		CalculateConstraintProperties();
	}
	else
		mIndependentAxisConstraintPart.Deactivate();
}

void PulleyConstraint::ResetWarmStart()
{
	mIndependentAxisConstraintPart.Deactivate();
}

void PulleyConstraint::WarmStartVelocityConstraint(float inWarmStartImpulseRatio)
{
	mIndependentAxisConstraintPart.WarmStart(*mBody1, *mBody2, mWorldSpaceNormal1, mWorldSpaceNormal2, mRatio, inWarmStartImpulseRatio);
}

bool PulleyConstraint::SolveVelocityConstraint(float inDeltaTime)
{
	if (mIndependentAxisConstraintPart.IsActive())
		return mIndependentAxisConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mWorldSpaceNormal1, mWorldSpaceNormal2, mRatio, mMinLambda, mMaxLambda);
	else
		return false;
}

bool PulleyConstraint::SolvePositionConstraint(float inDeltaTime, float inBaumgarte)
{
	// Calculate new length (bodies may have changed)
	float current_length = CalculatePositionsNormalsAndLength();

	float position_error = 0.0f;
	if (current_length < mMinLength)
		position_error = current_length - mMinLength;
	else if (current_length > mMaxLength)
		position_error = current_length - mMaxLength;

	if (position_error != 0.0f)
	{
		// Update constraint properties (bodies may have moved)
		CalculateConstraintProperties();

		return mIndependentAxisConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, mWorldSpaceNormal1, mWorldSpaceNormal2, mRatio, position_error, inBaumgarte);
	}

	return false;
}

#ifdef JPH_DEBUG_RENDERER
void PulleyConstraint::DrawConstraint(DebugRenderer *inRenderer) const
{
	// Color according to length vs min/max length
	float current_length = GetCurrentLength();
	Color color = Color::sGreen;
	if (current_length < mMinLength)
		color = Color::sYellow;
	else if (current_length > mMaxLength)
		color = Color::sRed;

	// Draw constraint
	inRenderer->DrawLine(mWorldSpacePosition1, mFixedPosition1, color);
	inRenderer->DrawLine(mFixedPosition1, mFixedPosition2, color);
	inRenderer->DrawLine(mFixedPosition2, mWorldSpacePosition2, color);

	// Draw current length
	inRenderer->DrawText3D(0.5_r * (mFixedPosition1 + mFixedPosition2), StringFormat("%.2f", (double)current_length));
}
#endif // JPH_DEBUG_RENDERER

void PulleyConstraint::SaveState(StateRecorder &inStream) const
{
	TwoBodyConstraint::SaveState(inStream);

	mIndependentAxisConstraintPart.SaveState(inStream);
	inStream.Write(mWorldSpaceNormal1); // When distance to fixed point = 0, the normal is used from last frame so we need to store it
	inStream.Write(mWorldSpaceNormal2);
}

void PulleyConstraint::RestoreState(StateRecorder &inStream)
{
	TwoBodyConstraint::RestoreState(inStream);

	mIndependentAxisConstraintPart.RestoreState(inStream);
	inStream.Read(mWorldSpaceNormal1);
	inStream.Read(mWorldSpaceNormal2);
}

Ref<ConstraintSettings> PulleyConstraint::GetConstraintSettings() const
{
	PulleyConstraintSettings *settings = new PulleyConstraintSettings;
	ToConstraintSettings(*settings);
	settings->mSpace = EConstraintSpace::LocalToBodyCOM;
	settings->mBodyPoint1 = RVec3(mLocalSpacePosition1);
	settings->mFixedPoint1 = mFixedPosition1;
	settings->mBodyPoint2 = RVec3(mLocalSpacePosition2);
	settings->mFixedPoint2 = mFixedPosition2;
	settings->mRatio = mRatio;
	settings->mMinLength = mMinLength;
	settings->mMaxLength = mMaxLength;
	return settings;
}

JPH_NAMESPACE_END
