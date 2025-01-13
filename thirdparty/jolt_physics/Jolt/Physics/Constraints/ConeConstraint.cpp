// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/ConeConstraint.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(ConeConstraintSettings)
{
	JPH_ADD_BASE_CLASS(ConeConstraintSettings, TwoBodyConstraintSettings)

	JPH_ADD_ENUM_ATTRIBUTE(ConeConstraintSettings, mSpace)
	JPH_ADD_ATTRIBUTE(ConeConstraintSettings, mPoint1)
	JPH_ADD_ATTRIBUTE(ConeConstraintSettings, mTwistAxis1)
	JPH_ADD_ATTRIBUTE(ConeConstraintSettings, mPoint2)
	JPH_ADD_ATTRIBUTE(ConeConstraintSettings, mTwistAxis2)
	JPH_ADD_ATTRIBUTE(ConeConstraintSettings, mHalfConeAngle)
}

void ConeConstraintSettings::SaveBinaryState(StreamOut &inStream) const
{
	ConstraintSettings::SaveBinaryState(inStream);

	inStream.Write(mSpace);
	inStream.Write(mPoint1);
	inStream.Write(mTwistAxis1);
	inStream.Write(mPoint2);
	inStream.Write(mTwistAxis2);
	inStream.Write(mHalfConeAngle);
}

void ConeConstraintSettings::RestoreBinaryState(StreamIn &inStream)
{
	ConstraintSettings::RestoreBinaryState(inStream);

	inStream.Read(mSpace);
	inStream.Read(mPoint1);
	inStream.Read(mTwistAxis1);
	inStream.Read(mPoint2);
	inStream.Read(mTwistAxis2);
	inStream.Read(mHalfConeAngle);
}

TwoBodyConstraint *ConeConstraintSettings::Create(Body &inBody1, Body &inBody2) const
{
	return new ConeConstraint(inBody1, inBody2, *this);
}

ConeConstraint::ConeConstraint(Body &inBody1, Body &inBody2, const ConeConstraintSettings &inSettings) :
	TwoBodyConstraint(inBody1, inBody2, inSettings)
{
	// Store limits
	SetHalfConeAngle(inSettings.mHalfConeAngle);

	// Initialize rotation axis to perpendicular of twist axis in case the angle between the twist axis is 0 in the first frame
	mWorldSpaceRotationAxis = inSettings.mTwistAxis1.GetNormalizedPerpendicular();

	if (inSettings.mSpace == EConstraintSpace::WorldSpace)
	{
		// If all properties were specified in world space, take them to local space now
		RMat44 inv_transform1 = inBody1.GetInverseCenterOfMassTransform();
		mLocalSpacePosition1 = Vec3(inv_transform1 * inSettings.mPoint1);
		mLocalSpaceTwistAxis1 = inv_transform1.Multiply3x3(inSettings.mTwistAxis1);

		RMat44 inv_transform2 = inBody2.GetInverseCenterOfMassTransform();
		mLocalSpacePosition2 = Vec3(inv_transform2 * inSettings.mPoint2);
		mLocalSpaceTwistAxis2 = inv_transform2.Multiply3x3(inSettings.mTwistAxis2);
	}
	else
	{
		// Properties already in local space
		mLocalSpacePosition1 = Vec3(inSettings.mPoint1);
		mLocalSpacePosition2 = Vec3(inSettings.mPoint2);
		mLocalSpaceTwistAxis1 = inSettings.mTwistAxis1;
		mLocalSpaceTwistAxis2 = inSettings.mTwistAxis2;

		// If they were in local space, we need to take the initial rotation axis to world space
		mWorldSpaceRotationAxis = inBody1.GetRotation() * mWorldSpaceRotationAxis;
	}
}

void ConeConstraint::NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM)
{
	if (mBody1->GetID() == inBodyID)
		mLocalSpacePosition1 -= inDeltaCOM;
	else if (mBody2->GetID() == inBodyID)
		mLocalSpacePosition2 -= inDeltaCOM;
}

void ConeConstraint::CalculateRotationConstraintProperties(Mat44Arg inRotation1, Mat44Arg inRotation2)
{
	// Rotation is along the cross product of both twist axis
	Vec3 twist1 = inRotation1.Multiply3x3(mLocalSpaceTwistAxis1);
	Vec3 twist2 = inRotation2.Multiply3x3(mLocalSpaceTwistAxis2);

	// Calculate dot product between twist axis, if it's smaller than the cone angle we need to correct
	mCosTheta = twist1.Dot(twist2);
	if (mCosTheta < mCosHalfConeAngle)
	{
		// Rotation axis is defined by the two twist axis
		Vec3 rot_axis = twist2.Cross(twist1);

		// If we can't find a rotation axis because the twist is too small, we'll use last frame's rotation axis
		float len = rot_axis.Length();
		if (len > 0.0f)
			mWorldSpaceRotationAxis = rot_axis / len;

		mAngleConstraintPart.CalculateConstraintProperties(*mBody1, *mBody2, mWorldSpaceRotationAxis);
	}
	else
		mAngleConstraintPart.Deactivate();
}

void ConeConstraint::SetupVelocityConstraint(float inDeltaTime)
{
	Mat44 rotation1 = Mat44::sRotation(mBody1->GetRotation());
	Mat44 rotation2 = Mat44::sRotation(mBody2->GetRotation());
	mPointConstraintPart.CalculateConstraintProperties(*mBody1, rotation1, mLocalSpacePosition1, *mBody2, rotation2, mLocalSpacePosition2);
	CalculateRotationConstraintProperties(rotation1, rotation2);
}

void ConeConstraint::ResetWarmStart()
{
	mPointConstraintPart.Deactivate();
	mAngleConstraintPart.Deactivate();
}

void ConeConstraint::WarmStartVelocityConstraint(float inWarmStartImpulseRatio)
{
	// Warm starting: Apply previous frame impulse
	mPointConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
	mAngleConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
}

bool ConeConstraint::SolveVelocityConstraint(float inDeltaTime)
{
	bool pos = mPointConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);

	bool rot = false;
	if (mAngleConstraintPart.IsActive())
		rot = mAngleConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mWorldSpaceRotationAxis, 0, FLT_MAX);

	return pos || rot;
}

bool ConeConstraint::SolvePositionConstraint(float inDeltaTime, float inBaumgarte)
{
	mPointConstraintPart.CalculateConstraintProperties(*mBody1, Mat44::sRotation(mBody1->GetRotation()), mLocalSpacePosition1, *mBody2, Mat44::sRotation(mBody2->GetRotation()), mLocalSpacePosition2);
	bool pos = mPointConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, inBaumgarte);

	bool rot = false;
	CalculateRotationConstraintProperties(Mat44::sRotation(mBody1->GetRotation()), Mat44::sRotation(mBody2->GetRotation()));
	if (mAngleConstraintPart.IsActive())
		rot = mAngleConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, mCosTheta - mCosHalfConeAngle, inBaumgarte);

	return pos || rot;
}

#ifdef JPH_DEBUG_RENDERER
void ConeConstraint::DrawConstraint(DebugRenderer *inRenderer) const
{
	RMat44 transform1 = mBody1->GetCenterOfMassTransform();
	RMat44 transform2 = mBody2->GetCenterOfMassTransform();

	RVec3 p1 = transform1 * mLocalSpacePosition1;
	RVec3 p2 = transform2 * mLocalSpacePosition2;

	// Draw constraint
	inRenderer->DrawMarker(p1, Color::sRed, 0.1f);
	inRenderer->DrawMarker(p2, Color::sGreen, 0.1f);

	// Draw twist axis
	inRenderer->DrawLine(p1, p1 + mDrawConstraintSize * transform1.Multiply3x3(mLocalSpaceTwistAxis1), Color::sRed);
	inRenderer->DrawLine(p2, p2 + mDrawConstraintSize * transform2.Multiply3x3(mLocalSpaceTwistAxis2), Color::sGreen);
}

void ConeConstraint::DrawConstraintLimits(DebugRenderer *inRenderer) const
{
	// Get constraint properties in world space
	RMat44 transform1 = mBody1->GetCenterOfMassTransform();
	RVec3 position1 = transform1 * mLocalSpacePosition1;
	Vec3 twist_axis1 = transform1.Multiply3x3(mLocalSpaceTwistAxis1);
	Vec3 normal_axis1 = transform1.Multiply3x3(mLocalSpaceTwistAxis1.GetNormalizedPerpendicular());

	inRenderer->DrawOpenCone(position1, twist_axis1, normal_axis1, ACos(mCosHalfConeAngle), mDrawConstraintSize * mCosHalfConeAngle, Color::sPurple, DebugRenderer::ECastShadow::Off);
}
#endif // JPH_DEBUG_RENDERER

void ConeConstraint::SaveState(StateRecorder &inStream) const
{
	TwoBodyConstraint::SaveState(inStream);

	mPointConstraintPart.SaveState(inStream);
	mAngleConstraintPart.SaveState(inStream);
	inStream.Write(mWorldSpaceRotationAxis); // When twist is too small, the rotation is used from last frame so we need to store it
}

void ConeConstraint::RestoreState(StateRecorder &inStream)
{
	TwoBodyConstraint::RestoreState(inStream);

	mPointConstraintPart.RestoreState(inStream);
	mAngleConstraintPart.RestoreState(inStream);
	inStream.Read(mWorldSpaceRotationAxis);
}

Ref<ConstraintSettings> ConeConstraint::GetConstraintSettings() const
{
	ConeConstraintSettings *settings = new ConeConstraintSettings;
	ToConstraintSettings(*settings);
	settings->mSpace = EConstraintSpace::LocalToBodyCOM;
	settings->mPoint1 = RVec3(mLocalSpacePosition1);
	settings->mTwistAxis1 = mLocalSpaceTwistAxis1;
	settings->mPoint2 = RVec3(mLocalSpacePosition2);
	settings->mTwistAxis2 = mLocalSpaceTwistAxis2;
	settings->mHalfConeAngle = ACos(mCosHalfConeAngle);
	return settings;
}

Mat44 ConeConstraint::GetConstraintToBody1Matrix() const
{
	Vec3 perp = mLocalSpaceTwistAxis1.GetNormalizedPerpendicular();
	Vec3 perp2 = mLocalSpaceTwistAxis1.Cross(perp);
	return Mat44(Vec4(mLocalSpaceTwistAxis1, 0), Vec4(perp, 0), Vec4(perp2, 0), Vec4(mLocalSpacePosition1, 1));
}

Mat44 ConeConstraint::GetConstraintToBody2Matrix() const
{
	// Note: Incorrect in rotation around the twist axis (the perpendicular does not match that of body 1),
	// this should not matter as we're not limiting rotation around the twist axis.
	Vec3 perp = mLocalSpaceTwistAxis2.GetNormalizedPerpendicular();
	Vec3 perp2 = mLocalSpaceTwistAxis2.Cross(perp);
	return Mat44(Vec4(mLocalSpaceTwistAxis2, 0), Vec4(perp, 0), Vec4(perp2, 0), Vec4(mLocalSpacePosition2, 1));
}

JPH_NAMESPACE_END
