// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(PointConstraintSettings)
{
	JPH_ADD_BASE_CLASS(PointConstraintSettings, TwoBodyConstraintSettings)

	JPH_ADD_ENUM_ATTRIBUTE(PointConstraintSettings, mSpace)
	JPH_ADD_ATTRIBUTE(PointConstraintSettings, mPoint1)
	JPH_ADD_ATTRIBUTE(PointConstraintSettings, mPoint2)
}

void PointConstraintSettings::SaveBinaryState(StreamOut &inStream) const
{
	ConstraintSettings::SaveBinaryState(inStream);

	inStream.Write(mSpace);
	inStream.Write(mPoint1);
	inStream.Write(mPoint2);
}

void PointConstraintSettings::RestoreBinaryState(StreamIn &inStream)
{
	ConstraintSettings::RestoreBinaryState(inStream);

	inStream.Read(mSpace);
	inStream.Read(mPoint1);
	inStream.Read(mPoint2);
}

TwoBodyConstraint *PointConstraintSettings::Create(Body &inBody1, Body &inBody2) const
{
	return new PointConstraint(inBody1, inBody2, *this);
}

PointConstraint::PointConstraint(Body &inBody1, Body &inBody2, const PointConstraintSettings &inSettings) :
	TwoBodyConstraint(inBody1, inBody2, inSettings)
{
	if (inSettings.mSpace == EConstraintSpace::WorldSpace)
	{
		// If all properties were specified in world space, take them to local space now
		mLocalSpacePosition1 = Vec3(inBody1.GetInverseCenterOfMassTransform() * inSettings.mPoint1);
		mLocalSpacePosition2 = Vec3(inBody2.GetInverseCenterOfMassTransform() * inSettings.mPoint2);
	}
	else
	{
		mLocalSpacePosition1 = Vec3(inSettings.mPoint1);
		mLocalSpacePosition2 = Vec3(inSettings.mPoint2);
	}
}

void PointConstraint::NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM)
{
	if (mBody1->GetID() == inBodyID)
		mLocalSpacePosition1 -= inDeltaCOM;
	else if (mBody2->GetID() == inBodyID)
		mLocalSpacePosition2 -= inDeltaCOM;
}

void PointConstraint::SetPoint1(EConstraintSpace inSpace, RVec3Arg inPoint1)
{
	if (inSpace == EConstraintSpace::WorldSpace)
		mLocalSpacePosition1 = Vec3(mBody1->GetInverseCenterOfMassTransform() * inPoint1);
	else
		mLocalSpacePosition1 = Vec3(inPoint1);
}

void PointConstraint::SetPoint2(EConstraintSpace inSpace, RVec3Arg inPoint2)
{
	if (inSpace == EConstraintSpace::WorldSpace)
		mLocalSpacePosition2 = Vec3(mBody2->GetInverseCenterOfMassTransform() * inPoint2);
	else
		mLocalSpacePosition2 = Vec3(inPoint2);
}

void PointConstraint::CalculateConstraintProperties()
{
	mPointConstraintPart.CalculateConstraintProperties(*mBody1, Mat44::sRotation(mBody1->GetRotation()), mLocalSpacePosition1, *mBody2, Mat44::sRotation(mBody2->GetRotation()), mLocalSpacePosition2);
}

void PointConstraint::SetupVelocityConstraint(float inDeltaTime)
{
	CalculateConstraintProperties();
}

void PointConstraint::ResetWarmStart()
{
	mPointConstraintPart.Deactivate();
}

void PointConstraint::WarmStartVelocityConstraint(float inWarmStartImpulseRatio)
{
	// Warm starting: Apply previous frame impulse
	mPointConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
}

bool PointConstraint::SolveVelocityConstraint(float inDeltaTime)
{
	return mPointConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);
}

bool PointConstraint::SolvePositionConstraint(float inDeltaTime, float inBaumgarte)
{
	// Update constraint properties (bodies may have moved)
	CalculateConstraintProperties();

	return mPointConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, inBaumgarte);
}

#ifdef JPH_DEBUG_RENDERER
void PointConstraint::DrawConstraint(DebugRenderer *inRenderer) const
{
	// Draw constraint
	inRenderer->DrawMarker(mBody1->GetCenterOfMassTransform() * mLocalSpacePosition1, Color::sRed, 0.1f);
	inRenderer->DrawMarker(mBody2->GetCenterOfMassTransform() * mLocalSpacePosition2, Color::sGreen, 0.1f);
}
#endif // JPH_DEBUG_RENDERER

void PointConstraint::SaveState(StateRecorder &inStream) const
{
	TwoBodyConstraint::SaveState(inStream);

	mPointConstraintPart.SaveState(inStream);
}

void PointConstraint::RestoreState(StateRecorder &inStream)
{
	TwoBodyConstraint::RestoreState(inStream);

	mPointConstraintPart.RestoreState(inStream);
}

Ref<ConstraintSettings> PointConstraint::GetConstraintSettings() const
{
	PointConstraintSettings *settings = new PointConstraintSettings;
	ToConstraintSettings(*settings);
	settings->mSpace = EConstraintSpace::LocalToBodyCOM;
	settings->mPoint1 = RVec3(mLocalSpacePosition1);
	settings->mPoint2 = RVec3(mLocalSpacePosition2);
	return settings;
}

JPH_NAMESPACE_END
