// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/ConstraintPart/RotationEulerConstraintPart.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(HingeConstraintSettings)
{
	JPH_ADD_BASE_CLASS(HingeConstraintSettings, TwoBodyConstraintSettings)

	JPH_ADD_ENUM_ATTRIBUTE(HingeConstraintSettings, mSpace)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mPoint1)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mHingeAxis1)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mNormalAxis1)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mPoint2)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mHingeAxis2)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mNormalAxis2)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mLimitsMin)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mLimitsMax)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mLimitsSpringSettings)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mMaxFrictionTorque)
	JPH_ADD_ATTRIBUTE(HingeConstraintSettings, mMotorSettings)
}

void HingeConstraintSettings::SaveBinaryState(StreamOut &inStream) const
{
	ConstraintSettings::SaveBinaryState(inStream);

	inStream.Write(mSpace);
	inStream.Write(mPoint1);
	inStream.Write(mHingeAxis1);
	inStream.Write(mNormalAxis1);
	inStream.Write(mPoint2);
	inStream.Write(mHingeAxis2);
	inStream.Write(mNormalAxis2);
	inStream.Write(mLimitsMin);
	inStream.Write(mLimitsMax);
	inStream.Write(mMaxFrictionTorque);
	mLimitsSpringSettings.SaveBinaryState(inStream);
	mMotorSettings.SaveBinaryState(inStream);
}

void HingeConstraintSettings::RestoreBinaryState(StreamIn &inStream)
{
	ConstraintSettings::RestoreBinaryState(inStream);

	inStream.Read(mSpace);
	inStream.Read(mPoint1);
	inStream.Read(mHingeAxis1);
	inStream.Read(mNormalAxis1);
	inStream.Read(mPoint2);
	inStream.Read(mHingeAxis2);
	inStream.Read(mNormalAxis2);
	inStream.Read(mLimitsMin);
	inStream.Read(mLimitsMax);
	inStream.Read(mMaxFrictionTorque);
	mLimitsSpringSettings.RestoreBinaryState(inStream);
	mMotorSettings.RestoreBinaryState(inStream);}

TwoBodyConstraint *HingeConstraintSettings::Create(Body &inBody1, Body &inBody2) const
{
	return new HingeConstraint(inBody1, inBody2, *this);
}

HingeConstraint::HingeConstraint(Body &inBody1, Body &inBody2, const HingeConstraintSettings &inSettings) :
	TwoBodyConstraint(inBody1, inBody2, inSettings),
	mMaxFrictionTorque(inSettings.mMaxFrictionTorque),
	mMotorSettings(inSettings.mMotorSettings)
{
	// Store limits
	JPH_ASSERT(inSettings.mLimitsMin != inSettings.mLimitsMax || inSettings.mLimitsSpringSettings.mFrequency > 0.0f, "Better use a fixed constraint in this case");
	SetLimits(inSettings.mLimitsMin, inSettings.mLimitsMax);

	// Store inverse of initial rotation from body 1 to body 2 in body 1 space
	mInvInitialOrientation = RotationEulerConstraintPart::sGetInvInitialOrientationXZ(inSettings.mNormalAxis1, inSettings.mHingeAxis1, inSettings.mNormalAxis2, inSettings.mHingeAxis2);

	if (inSettings.mSpace == EConstraintSpace::WorldSpace)
	{
		// If all properties were specified in world space, take them to local space now
		RMat44 inv_transform1 = inBody1.GetInverseCenterOfMassTransform();
		mLocalSpacePosition1 = Vec3(inv_transform1 * inSettings.mPoint1);
		mLocalSpaceHingeAxis1 = inv_transform1.Multiply3x3(inSettings.mHingeAxis1).Normalized();
		mLocalSpaceNormalAxis1 = inv_transform1.Multiply3x3(inSettings.mNormalAxis1).Normalized();

		RMat44 inv_transform2 = inBody2.GetInverseCenterOfMassTransform();
		mLocalSpacePosition2 = Vec3(inv_transform2 * inSettings.mPoint2);
		mLocalSpaceHingeAxis2 = inv_transform2.Multiply3x3(inSettings.mHingeAxis2).Normalized();
		mLocalSpaceNormalAxis2 = inv_transform2.Multiply3x3(inSettings.mNormalAxis2).Normalized();

		// Constraints were specified in world space, so we should have replaced c1 with q10^-1 c1 and c2 with q20^-1 c2
		// => r0^-1 = (q20^-1 c2) (q10^-1 c1)^1 = q20^-1 (c2 c1^-1) q10
		mInvInitialOrientation = inBody2.GetRotation().Conjugated() * mInvInitialOrientation * inBody1.GetRotation();
	}
	else
	{
		mLocalSpacePosition1 = Vec3(inSettings.mPoint1);
		mLocalSpaceHingeAxis1 = inSettings.mHingeAxis1;
		mLocalSpaceNormalAxis1 = inSettings.mNormalAxis1;

		mLocalSpacePosition2 = Vec3(inSettings.mPoint2);
		mLocalSpaceHingeAxis2 = inSettings.mHingeAxis2;
		mLocalSpaceNormalAxis2 = inSettings.mNormalAxis2;
	}

	// Store spring settings
	SetLimitsSpringSettings(inSettings.mLimitsSpringSettings);
}

void HingeConstraint::NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM)
{
	if (mBody1->GetID() == inBodyID)
		mLocalSpacePosition1 -= inDeltaCOM;
	else if (mBody2->GetID() == inBodyID)
		mLocalSpacePosition2 -= inDeltaCOM;
}

float HingeConstraint::GetCurrentAngle() const
{
	// See: CalculateA1AndTheta
	Quat rotation1 = mBody1->GetRotation();
	Quat diff = mBody2->GetRotation() * mInvInitialOrientation * rotation1.Conjugated();
	return diff.GetRotationAngle(rotation1 * mLocalSpaceHingeAxis1);
}

void HingeConstraint::SetLimits(float inLimitsMin, float inLimitsMax)
{
	JPH_ASSERT(inLimitsMin <= 0.0f && inLimitsMin >= -JPH_PI);
	JPH_ASSERT(inLimitsMax >= 0.0f && inLimitsMax <= JPH_PI);
	mLimitsMin = inLimitsMin;
	mLimitsMax = inLimitsMax;
	mHasLimits = mLimitsMin > -JPH_PI && mLimitsMax < JPH_PI;
}

void HingeConstraint::CalculateA1AndTheta()
{
	if (mHasLimits || mMotorState != EMotorState::Off || mMaxFrictionTorque > 0.0f)
	{
		Quat rotation1 = mBody1->GetRotation();

		// Calculate relative rotation in world space
		//
		// The rest rotation is:
		//
		// q2 = q1 r0
		//
		// But the actual rotation is
		//
		// q2 = diff q1 r0
		// <=> diff = q2 r0^-1 q1^-1
		//
		// Where:
		// q1 = current rotation of body 1
		// q2 = current rotation of body 2
		// diff = relative rotation in world space
		Quat diff = mBody2->GetRotation() * mInvInitialOrientation * rotation1.Conjugated();

		// Calculate hinge axis in world space
		mA1 = rotation1 * mLocalSpaceHingeAxis1;

		// Get rotation angle around the hinge axis
		mTheta = diff.GetRotationAngle(mA1);
	}
}

void HingeConstraint::CalculateRotationLimitsConstraintProperties(float inDeltaTime)
{
	// Apply constraint if outside of limits
	if (mHasLimits && (mTheta <= mLimitsMin || mTheta >= mLimitsMax))
		mRotationLimitsConstraintPart.CalculateConstraintPropertiesWithSettings(inDeltaTime, *mBody1, *mBody2, mA1, 0.0f, GetSmallestAngleToLimit(), mLimitsSpringSettings);
	else
		mRotationLimitsConstraintPart.Deactivate();
}

void HingeConstraint::CalculateMotorConstraintProperties(float inDeltaTime)
{
	switch (mMotorState)
	{
	case EMotorState::Off:
		if (mMaxFrictionTorque > 0.0f)
			mMotorConstraintPart.CalculateConstraintProperties(*mBody1, *mBody2, mA1);
		else
			mMotorConstraintPart.Deactivate();
		break;

	case EMotorState::Velocity:
		mMotorConstraintPart.CalculateConstraintProperties(*mBody1, *mBody2, mA1, -mTargetAngularVelocity);
		break;

	case EMotorState::Position:
		if (mMotorSettings.mSpringSettings.HasStiffness())
			mMotorConstraintPart.CalculateConstraintPropertiesWithSettings(inDeltaTime, *mBody1, *mBody2, mA1, 0.0f, CenterAngleAroundZero(mTheta - mTargetAngle), mMotorSettings.mSpringSettings);
		else
			mMotorConstraintPart.Deactivate();
		break;
	}
}

void HingeConstraint::SetupVelocityConstraint(float inDeltaTime)
{
	// Cache constraint values that are valid until the bodies move
	Mat44 rotation1 = Mat44::sRotation(mBody1->GetRotation());
	Mat44 rotation2 = Mat44::sRotation(mBody2->GetRotation());
	mPointConstraintPart.CalculateConstraintProperties(*mBody1, rotation1, mLocalSpacePosition1, *mBody2, rotation2, mLocalSpacePosition2);
	mRotationConstraintPart.CalculateConstraintProperties(*mBody1, rotation1, rotation1.Multiply3x3(mLocalSpaceHingeAxis1), *mBody2, rotation2, rotation2.Multiply3x3(mLocalSpaceHingeAxis2));
	CalculateA1AndTheta();
	CalculateRotationLimitsConstraintProperties(inDeltaTime);
	CalculateMotorConstraintProperties(inDeltaTime);
}

void HingeConstraint::ResetWarmStart()
{
	mMotorConstraintPart.Deactivate();
	mPointConstraintPart.Deactivate();
	mRotationConstraintPart.Deactivate();
	mRotationLimitsConstraintPart.Deactivate();
}

void HingeConstraint::WarmStartVelocityConstraint(float inWarmStartImpulseRatio)
{
	// Warm starting: Apply previous frame impulse
	mMotorConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
	mPointConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
	mRotationConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
	mRotationLimitsConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
}

float HingeConstraint::GetSmallestAngleToLimit() const
{
	float dist_to_min = CenterAngleAroundZero(mTheta - mLimitsMin);
	float dist_to_max = CenterAngleAroundZero(mTheta - mLimitsMax);
	return abs(dist_to_min) < abs(dist_to_max)? dist_to_min : dist_to_max;
}

bool HingeConstraint::IsMinLimitClosest() const
{
	float dist_to_min = CenterAngleAroundZero(mTheta - mLimitsMin);
	float dist_to_max = CenterAngleAroundZero(mTheta - mLimitsMax);
	return abs(dist_to_min) < abs(dist_to_max);
}

bool HingeConstraint::SolveVelocityConstraint(float inDeltaTime)
{
	// Solve motor
	bool motor = false;
	if (mMotorConstraintPart.IsActive())
	{
		switch (mMotorState)
		{
		case EMotorState::Off:
			{
				float max_lambda = mMaxFrictionTorque * inDeltaTime;
				motor = mMotorConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mA1, -max_lambda, max_lambda);
				break;
			}

		case EMotorState::Velocity:
		case EMotorState::Position:
			motor = mMotorConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mA1, inDeltaTime * mMotorSettings.mMinTorqueLimit, inDeltaTime * mMotorSettings.mMaxTorqueLimit);
			break;
		}
	}

	// Solve point constraint
	bool pos = mPointConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);

	// Solve rotation constraint
	bool rot = mRotationConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);

	// Solve rotation limits
	bool limit = false;
	if (mRotationLimitsConstraintPart.IsActive())
	{
		float min_lambda, max_lambda;
		if (mLimitsMin == mLimitsMax)
		{
			min_lambda = -FLT_MAX;
			max_lambda = FLT_MAX;
		}
		else if (IsMinLimitClosest())
		{
			min_lambda = 0.0f;
			max_lambda = FLT_MAX;
		}
		else
		{
			min_lambda = -FLT_MAX;
			max_lambda = 0.0f;
		}
		limit = mRotationLimitsConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mA1, min_lambda, max_lambda);
	}

	return motor || pos || rot || limit;
}

bool HingeConstraint::SolvePositionConstraint(float inDeltaTime, float inBaumgarte)
{
	// Motor operates on velocities only, don't call SolvePositionConstraint

	// Solve point constraint
	mPointConstraintPart.CalculateConstraintProperties(*mBody1, Mat44::sRotation(mBody1->GetRotation()), mLocalSpacePosition1, *mBody2, Mat44::sRotation(mBody2->GetRotation()), mLocalSpacePosition2);
	bool pos = mPointConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, inBaumgarte);

	// Solve rotation constraint
	Mat44 rotation1 = Mat44::sRotation(mBody1->GetRotation()); // Note that previous call to GetRotation() is out of date since the rotation has changed
	Mat44 rotation2 = Mat44::sRotation(mBody2->GetRotation());
	mRotationConstraintPart.CalculateConstraintProperties(*mBody1, rotation1, rotation1.Multiply3x3(mLocalSpaceHingeAxis1), *mBody2, rotation2, rotation2.Multiply3x3(mLocalSpaceHingeAxis2));
	bool rot = mRotationConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, inBaumgarte);

	// Solve rotation limits
	bool limit = false;
	if (mHasLimits && mLimitsSpringSettings.mFrequency <= 0.0f)
	{
		CalculateA1AndTheta();
		CalculateRotationLimitsConstraintProperties(inDeltaTime);
		if (mRotationLimitsConstraintPart.IsActive())
			limit = mRotationLimitsConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, GetSmallestAngleToLimit(), inBaumgarte);
	}

	return pos || rot || limit;
}

#ifdef JPH_DEBUG_RENDERER
void HingeConstraint::DrawConstraint(DebugRenderer *inRenderer) const
{
	RMat44 transform1 = mBody1->GetCenterOfMassTransform();
	RMat44 transform2 = mBody2->GetCenterOfMassTransform();

	// Draw constraint
	RVec3 constraint_pos1 = transform1 * mLocalSpacePosition1;
	inRenderer->DrawMarker(constraint_pos1, Color::sRed, 0.1f);
	inRenderer->DrawLine(constraint_pos1, transform1 * (mLocalSpacePosition1 + mDrawConstraintSize * mLocalSpaceHingeAxis1), Color::sRed);

	RVec3 constraint_pos2 = transform2 * mLocalSpacePosition2;
	inRenderer->DrawMarker(constraint_pos2, Color::sGreen, 0.1f);
	inRenderer->DrawLine(constraint_pos2, transform2 * (mLocalSpacePosition2 + mDrawConstraintSize * mLocalSpaceHingeAxis2), Color::sGreen);
	inRenderer->DrawLine(constraint_pos2, transform2 * (mLocalSpacePosition2 + mDrawConstraintSize * mLocalSpaceNormalAxis2), Color::sWhite);
}

void HingeConstraint::DrawConstraintLimits(DebugRenderer *inRenderer) const
{
	if (mHasLimits && mLimitsMax > mLimitsMin)
	{
		// Get constraint properties in world space
		RMat44 transform1 = mBody1->GetCenterOfMassTransform();
		RVec3 position1 = transform1 * mLocalSpacePosition1;
		Vec3 hinge_axis1 = transform1.Multiply3x3(mLocalSpaceHingeAxis1);
		Vec3 normal_axis1 = transform1.Multiply3x3(mLocalSpaceNormalAxis1);

		inRenderer->DrawPie(position1, mDrawConstraintSize, hinge_axis1, normal_axis1, mLimitsMin, mLimitsMax, Color::sPurple, DebugRenderer::ECastShadow::Off);
	}
}
#endif // JPH_DEBUG_RENDERER

void HingeConstraint::SaveState(StateRecorder &inStream) const
{
	TwoBodyConstraint::SaveState(inStream);

	mMotorConstraintPart.SaveState(inStream);
	mRotationConstraintPart.SaveState(inStream);
	mPointConstraintPart.SaveState(inStream);
	mRotationLimitsConstraintPart.SaveState(inStream);

	inStream.Write(mMotorState);
	inStream.Write(mTargetAngularVelocity);
	inStream.Write(mTargetAngle);
}

void HingeConstraint::RestoreState(StateRecorder &inStream)
{
	TwoBodyConstraint::RestoreState(inStream);

	mMotorConstraintPart.RestoreState(inStream);
	mRotationConstraintPart.RestoreState(inStream);
	mPointConstraintPart.RestoreState(inStream);
	mRotationLimitsConstraintPart.RestoreState(inStream);

	inStream.Read(mMotorState);
	inStream.Read(mTargetAngularVelocity);
	inStream.Read(mTargetAngle);
}


Ref<ConstraintSettings> HingeConstraint::GetConstraintSettings() const
{
	HingeConstraintSettings *settings = new HingeConstraintSettings;
	ToConstraintSettings(*settings);
	settings->mSpace = EConstraintSpace::LocalToBodyCOM;
	settings->mPoint1 = RVec3(mLocalSpacePosition1);
	settings->mHingeAxis1 = mLocalSpaceHingeAxis1;
	settings->mNormalAxis1 = mLocalSpaceNormalAxis1;
	settings->mPoint2 = RVec3(mLocalSpacePosition2);
	settings->mHingeAxis2 = mLocalSpaceHingeAxis2;
	settings->mNormalAxis2 = mLocalSpaceNormalAxis2;
	settings->mLimitsMin = mLimitsMin;
	settings->mLimitsMax = mLimitsMax;
	settings->mLimitsSpringSettings = mLimitsSpringSettings;
	settings->mMaxFrictionTorque = mMaxFrictionTorque;
	settings->mMotorSettings = mMotorSettings;
	return settings;
}

Mat44 HingeConstraint::GetConstraintToBody1Matrix() const
{
	return Mat44(Vec4(mLocalSpaceHingeAxis1, 0), Vec4(mLocalSpaceNormalAxis1, 0), Vec4(mLocalSpaceHingeAxis1.Cross(mLocalSpaceNormalAxis1), 0), Vec4(mLocalSpacePosition1, 1));
}

Mat44 HingeConstraint::GetConstraintToBody2Matrix() const
{
	return Mat44(Vec4(mLocalSpaceHingeAxis2, 0), Vec4(mLocalSpaceNormalAxis2, 0), Vec4(mLocalSpaceHingeAxis2.Cross(mLocalSpaceNormalAxis2), 0), Vec4(mLocalSpacePosition2, 1));
}

JPH_NAMESPACE_END
