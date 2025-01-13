// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(SwingTwistConstraintSettings)
{
	JPH_ADD_BASE_CLASS(SwingTwistConstraintSettings, TwoBodyConstraintSettings)

	JPH_ADD_ENUM_ATTRIBUTE(SwingTwistConstraintSettings, mSpace)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mPosition1)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mTwistAxis1)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mPlaneAxis1)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mPosition2)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mTwistAxis2)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mPlaneAxis2)
	JPH_ADD_ENUM_ATTRIBUTE(SwingTwistConstraintSettings, mSwingType)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mNormalHalfConeAngle)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mPlaneHalfConeAngle)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mTwistMinAngle)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mTwistMaxAngle)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mMaxFrictionTorque)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mSwingMotorSettings)
	JPH_ADD_ATTRIBUTE(SwingTwistConstraintSettings, mTwistMotorSettings)
}

void SwingTwistConstraintSettings::SaveBinaryState(StreamOut &inStream) const
{
	ConstraintSettings::SaveBinaryState(inStream);

	inStream.Write(mSpace);
	inStream.Write(mPosition1);
	inStream.Write(mTwistAxis1);
	inStream.Write(mPlaneAxis1);
	inStream.Write(mPosition2);
	inStream.Write(mTwistAxis2);
	inStream.Write(mPlaneAxis2);
	inStream.Write(mSwingType);
	inStream.Write(mNormalHalfConeAngle);
	inStream.Write(mPlaneHalfConeAngle);
	inStream.Write(mTwistMinAngle);
	inStream.Write(mTwistMaxAngle);
	inStream.Write(mMaxFrictionTorque);
	mSwingMotorSettings.SaveBinaryState(inStream);
	mTwistMotorSettings.SaveBinaryState(inStream);
}

void SwingTwistConstraintSettings::RestoreBinaryState(StreamIn &inStream)
{
	ConstraintSettings::RestoreBinaryState(inStream);

	inStream.Read(mSpace);
	inStream.Read(mPosition1);
	inStream.Read(mTwistAxis1);
	inStream.Read(mPlaneAxis1);
	inStream.Read(mPosition2);
	inStream.Read(mTwistAxis2);
	inStream.Read(mPlaneAxis2);
	inStream.Read(mSwingType);
	inStream.Read(mNormalHalfConeAngle);
	inStream.Read(mPlaneHalfConeAngle);
	inStream.Read(mTwistMinAngle);
	inStream.Read(mTwistMaxAngle);
	inStream.Read(mMaxFrictionTorque);
	mSwingMotorSettings.RestoreBinaryState(inStream);
	mTwistMotorSettings.RestoreBinaryState(inStream);
}

TwoBodyConstraint *SwingTwistConstraintSettings::Create(Body &inBody1, Body &inBody2) const
{
	return new SwingTwistConstraint(inBody1, inBody2, *this);
}

void SwingTwistConstraint::UpdateLimits()
{
	// Pass limits on to swing twist constraint part
	mSwingTwistConstraintPart.SetLimits(mTwistMinAngle, mTwistMaxAngle, -mPlaneHalfConeAngle, mPlaneHalfConeAngle, -mNormalHalfConeAngle, mNormalHalfConeAngle);
}

SwingTwistConstraint::SwingTwistConstraint(Body &inBody1, Body &inBody2, const SwingTwistConstraintSettings &inSettings) :
	TwoBodyConstraint(inBody1, inBody2, inSettings),
	mNormalHalfConeAngle(inSettings.mNormalHalfConeAngle),
	mPlaneHalfConeAngle(inSettings.mPlaneHalfConeAngle),
	mTwistMinAngle(inSettings.mTwistMinAngle),
	mTwistMaxAngle(inSettings.mTwistMaxAngle),
	mMaxFrictionTorque(inSettings.mMaxFrictionTorque),
	mSwingMotorSettings(inSettings.mSwingMotorSettings),
	mTwistMotorSettings(inSettings.mTwistMotorSettings)
{
	// Override swing type
	mSwingTwistConstraintPart.SetSwingType(inSettings.mSwingType);

	// Calculate rotation needed to go from constraint space to body1 local space
	Vec3 normal_axis1 = inSettings.mPlaneAxis1.Cross(inSettings.mTwistAxis1);
	Mat44 c_to_b1(Vec4(inSettings.mTwistAxis1, 0), Vec4(normal_axis1, 0), Vec4(inSettings.mPlaneAxis1, 0), Vec4(0, 0, 0, 1));
	mConstraintToBody1 = c_to_b1.GetQuaternion();

	// Calculate rotation needed to go from constraint space to body2 local space
	Vec3 normal_axis2 = inSettings.mPlaneAxis2.Cross(inSettings.mTwistAxis2);
	Mat44 c_to_b2(Vec4(inSettings.mTwistAxis2, 0), Vec4(normal_axis2, 0), Vec4(inSettings.mPlaneAxis2, 0), Vec4(0, 0, 0, 1));
	mConstraintToBody2 = c_to_b2.GetQuaternion();

	if (inSettings.mSpace == EConstraintSpace::WorldSpace)
	{
		// If all properties were specified in world space, take them to local space now
		mLocalSpacePosition1 = Vec3(inBody1.GetInverseCenterOfMassTransform() * inSettings.mPosition1);
		mConstraintToBody1 = inBody1.GetRotation().Conjugated() * mConstraintToBody1;

		mLocalSpacePosition2 = Vec3(inBody2.GetInverseCenterOfMassTransform() * inSettings.mPosition2);
		mConstraintToBody2 = inBody2.GetRotation().Conjugated() * mConstraintToBody2;
	}
	else
	{
		mLocalSpacePosition1 = Vec3(inSettings.mPosition1);
		mLocalSpacePosition2 = Vec3(inSettings.mPosition2);
	}

	UpdateLimits();
}

void SwingTwistConstraint::NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM)
{
	if (mBody1->GetID() == inBodyID)
		mLocalSpacePosition1 -= inDeltaCOM;
	else if (mBody2->GetID() == inBodyID)
		mLocalSpacePosition2 -= inDeltaCOM;
}

Quat SwingTwistConstraint::GetRotationInConstraintSpace() const
{
	// Let b1, b2 be the center of mass transform of body1 and body2 (For body1 this is mBody1->GetCenterOfMassTransform())
	// Let c1, c2 be the transform that takes a vector from constraint space to local space of body1 and body2 (For body1 this is Mat44::sRotationTranslation(mConstraintToBody1, mLocalSpacePosition1))
	// Let q be the rotation of the constraint in constraint space
	// b2 takes a vector from the local space of body2 to world space
	// To express this in terms of b1: b2 = b1 * c1 * q * c2^-1
	// c2^-1 goes from local body 2 space to constraint space
	// q rotates the constraint
	// c1 goes from constraint space to body 1 local space
	// b1 goes from body 1 local space to world space
	// So when the body rotations are given, q = (b1 * c1)^-1 * b2 c2
	// Or: q = (q1 * c1)^-1 * (q2 * c2) if we're only interested in rotations
	Quat constraint_body1_to_world = mBody1->GetRotation() * mConstraintToBody1;
	Quat constraint_body2_to_world = mBody2->GetRotation() * mConstraintToBody2;
	return constraint_body1_to_world.Conjugated() * constraint_body2_to_world;
}

void SwingTwistConstraint::SetSwingMotorState(EMotorState inState)
{
	JPH_ASSERT(inState == EMotorState::Off || mSwingMotorSettings.IsValid());

	if (mSwingMotorState != inState)
	{
		mSwingMotorState = inState;

		// Ensure that warm starting next frame doesn't apply any impulses (motor parts are repurposed for different modes)
		for (AngleConstraintPart &c : mMotorConstraintPart)
			c.Deactivate();
	}
}

void SwingTwistConstraint::SetTwistMotorState(EMotorState inState)
{
	JPH_ASSERT(inState == EMotorState::Off || mTwistMotorSettings.IsValid());

	if (mTwistMotorState != inState)
	{
		mTwistMotorState = inState;

		// Ensure that warm starting next frame doesn't apply any impulses (motor parts are repurposed for different modes)
		mMotorConstraintPart[0].Deactivate();
	}
}

void SwingTwistConstraint::SetTargetOrientationCS(QuatArg inOrientation)
{
	Quat q_swing, q_twist;
	inOrientation.GetSwingTwist(q_swing, q_twist);

	uint clamped_axis;
	mSwingTwistConstraintPart.ClampSwingTwist(q_swing, q_twist, clamped_axis);

	if (clamped_axis != 0)
		mTargetOrientation = q_swing * q_twist;
	else
		mTargetOrientation = inOrientation;
}

void SwingTwistConstraint::SetupVelocityConstraint(float inDeltaTime)
{
	// Setup point constraint
	Mat44 rotation1 = Mat44::sRotation(mBody1->GetRotation());
	Mat44 rotation2 = Mat44::sRotation(mBody2->GetRotation());
	mPointConstraintPart.CalculateConstraintProperties(*mBody1, rotation1, mLocalSpacePosition1, *mBody2, rotation2, mLocalSpacePosition2);

	// GetRotationInConstraintSpace written out since we reuse the sub expressions
	Quat constraint_body1_to_world = mBody1->GetRotation() * mConstraintToBody1;
	Quat constraint_body2_to_world = mBody2->GetRotation() * mConstraintToBody2;
	Quat q = constraint_body1_to_world.Conjugated() * constraint_body2_to_world;

	// Calculate constraint properties for the swing twist limit
	mSwingTwistConstraintPart.CalculateConstraintProperties(*mBody1, *mBody2, q, constraint_body1_to_world);

	if (mSwingMotorState != EMotorState::Off || mTwistMotorState != EMotorState::Off || mMaxFrictionTorque > 0.0f)
	{
		// Calculate rotation motor axis
		Mat44 ws_axis = Mat44::sRotation(constraint_body2_to_world);
		for (int i = 0; i < 3; ++i)
			mWorldSpaceMotorAxis[i] = ws_axis.GetColumn3(i);

		Vec3 rotation_error;
		if (mSwingMotorState == EMotorState::Position || mTwistMotorState == EMotorState::Position)
		{
			// Get target orientation along the shortest path from q
			Quat target_orientation = q.Dot(mTargetOrientation) > 0.0f? mTargetOrientation : -mTargetOrientation;

			// The definition of the constraint rotation q:
			// R2 * ConstraintToBody2 = R1 * ConstraintToBody1 * q (1)
			//
			// R2' is the rotation of body 2 when reaching the target_orientation:
			// R2' * ConstraintToBody2 = R1 * ConstraintToBody1 * target_orientation (2)
			//
			// The difference in body 2 space:
			// R2' = R2 * diff_body2 (3)
			//
			// We want to specify the difference in the constraint space of body 2:
			// diff_body2 = ConstraintToBody2 * diff * ConstraintToBody2^* (4)
			//
			// Extracting R2' from 2: R2' = R1 * ConstraintToBody1 * target_orientation * ConstraintToBody2^* (5)
			// Combining 3 & 4: R2' = R2 * ConstraintToBody2 * diff * ConstraintToBody2^* (6)
			// Combining 1 & 6: R2' = R1 * ConstraintToBody1 * q * diff * ConstraintToBody2^* (7)
			// Combining 5 & 7: R1 * ConstraintToBody1 * target_orientation * ConstraintToBody2^* = R1 * ConstraintToBody1 * q * diff * ConstraintToBody2^*
			// <=> target_orientation = q * diff
			// <=> diff = q^* * target_orientation
			Quat diff = q.Conjugated() * target_orientation;

			// Approximate error angles
			// The imaginary part of a quaternion is rotation_axis * sin(angle / 2)
			// If angle is small, sin(x) = x so angle[i] ~ 2.0f * rotation_axis[i]
			// We'll be making small time steps, so if the angle is not small at least the sign will be correct and we'll move in the right direction
			rotation_error = -2.0f * diff.GetXYZ();
		}

		// Swing motor
		switch (mSwingMotorState)
		{
		case EMotorState::Off:
			if (mMaxFrictionTorque > 0.0f)
			{
				// Enable friction
				for (int i = 1; i < 3; ++i)
					mMotorConstraintPart[i].CalculateConstraintProperties(*mBody1, *mBody2, mWorldSpaceMotorAxis[i], 0.0f);
			}
			else
			{
				// Disable friction
				for (AngleConstraintPart &c : mMotorConstraintPart)
					c.Deactivate();
			}
			break;

		case EMotorState::Velocity:
			// Use motor to create angular velocity around desired axis
			for (int i = 1; i < 3; ++i)
				mMotorConstraintPart[i].CalculateConstraintProperties(*mBody1, *mBody2, mWorldSpaceMotorAxis[i], -mTargetAngularVelocity[i]);
			break;

		case EMotorState::Position:
			// Use motor to drive rotation error to zero
			if (mSwingMotorSettings.mSpringSettings.HasStiffness())
			{
				for (int i = 1; i < 3; ++i)
					mMotorConstraintPart[i].CalculateConstraintPropertiesWithSettings(inDeltaTime, *mBody1, *mBody2, mWorldSpaceMotorAxis[i], 0.0f, rotation_error[i], mSwingMotorSettings.mSpringSettings);
			}
			else
			{
				for (int i = 1; i < 3; ++i)
					mMotorConstraintPart[i].Deactivate();
			}
			break;
		}

		// Twist motor
		switch (mTwistMotorState)
		{
		case EMotorState::Off:
			if (mMaxFrictionTorque > 0.0f)
			{
				// Enable friction
				mMotorConstraintPart[0].CalculateConstraintProperties(*mBody1, *mBody2, mWorldSpaceMotorAxis[0], 0.0f);
			}
			else
			{
				// Disable friction
				mMotorConstraintPart[0].Deactivate();
			}
			break;

		case EMotorState::Velocity:
			// Use motor to create angular velocity around desired axis
			mMotorConstraintPart[0].CalculateConstraintProperties(*mBody1, *mBody2, mWorldSpaceMotorAxis[0], -mTargetAngularVelocity[0]);
			break;

		case EMotorState::Position:
			// Use motor to drive rotation error to zero
			if (mTwistMotorSettings.mSpringSettings.HasStiffness())
				mMotorConstraintPart[0].CalculateConstraintPropertiesWithSettings(inDeltaTime, *mBody1, *mBody2, mWorldSpaceMotorAxis[0], 0.0f, rotation_error[0], mTwistMotorSettings.mSpringSettings);
			else
				mMotorConstraintPart[0].Deactivate();
			break;
		}
	}
	else
	{
		// Disable rotation motor
		for (AngleConstraintPart &c : mMotorConstraintPart)
			c.Deactivate();
	}
}

void SwingTwistConstraint::ResetWarmStart()
{
	for (AngleConstraintPart &c : mMotorConstraintPart)
		c.Deactivate();
	mSwingTwistConstraintPart.Deactivate();
	mPointConstraintPart.Deactivate();
}

void SwingTwistConstraint::WarmStartVelocityConstraint(float inWarmStartImpulseRatio)
{
	// Warm starting: Apply previous frame impulse
	for (AngleConstraintPart &c : mMotorConstraintPart)
		c.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
	mSwingTwistConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
	mPointConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
}

bool SwingTwistConstraint::SolveVelocityConstraint(float inDeltaTime)
{
	bool impulse = false;

	// Solve twist rotation motor
	if (mMotorConstraintPart[0].IsActive())
	{
		// Twist limits
		float min_twist_limit, max_twist_limit;
		if (mTwistMotorState == EMotorState::Off)
		{
			max_twist_limit = inDeltaTime * mMaxFrictionTorque;
			min_twist_limit = -max_twist_limit;
		}
		else
		{
			min_twist_limit = inDeltaTime * mTwistMotorSettings.mMinTorqueLimit;
			max_twist_limit = inDeltaTime * mTwistMotorSettings.mMaxTorqueLimit;
		}

		impulse |= mMotorConstraintPart[0].SolveVelocityConstraint(*mBody1, *mBody2, mWorldSpaceMotorAxis[0], min_twist_limit, max_twist_limit);
	}

	// Solve swing rotation motor
	if (mMotorConstraintPart[1].IsActive())
	{
		// Swing parts should turn on / off together
		JPH_ASSERT(mMotorConstraintPart[2].IsActive());

		// Swing limits
		float min_swing_limit, max_swing_limit;
		if (mSwingMotorState == EMotorState::Off)
		{
			max_swing_limit = inDeltaTime * mMaxFrictionTorque;
			min_swing_limit = -max_swing_limit;
		}
		else
		{
			min_swing_limit = inDeltaTime * mSwingMotorSettings.mMinTorqueLimit;
			max_swing_limit = inDeltaTime * mSwingMotorSettings.mMaxTorqueLimit;
		}

		for (int i = 1; i < 3; ++i)
			impulse |= mMotorConstraintPart[i].SolveVelocityConstraint(*mBody1, *mBody2, mWorldSpaceMotorAxis[i], min_swing_limit, max_swing_limit);
	}
	else
	{
		// Swing parts should turn on / off together
		JPH_ASSERT(!mMotorConstraintPart[2].IsActive());
	}

	// Solve rotation limits
	impulse |= mSwingTwistConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);

	// Solve position constraint
	impulse |= mPointConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);

	return impulse;
}

bool SwingTwistConstraint::SolvePositionConstraint(float inDeltaTime, float inBaumgarte)
{
	bool impulse = false;

	// Solve rotation violations
	Quat q = GetRotationInConstraintSpace();
	impulse |= mSwingTwistConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, q, mConstraintToBody1, mConstraintToBody2, inBaumgarte);

	// Solve position violations
	mPointConstraintPart.CalculateConstraintProperties(*mBody1, Mat44::sRotation(mBody1->GetRotation()), mLocalSpacePosition1, *mBody2, Mat44::sRotation(mBody2->GetRotation()), mLocalSpacePosition2);
	impulse |= mPointConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, inBaumgarte);

	return impulse;
}

#ifdef JPH_DEBUG_RENDERER
void SwingTwistConstraint::DrawConstraint(DebugRenderer *inRenderer) const
{
	// Get constraint properties in world space
	RMat44 transform1 = mBody1->GetCenterOfMassTransform();
	RVec3 position1 = transform1 * mLocalSpacePosition1;
	Quat rotation1 = mBody1->GetRotation() * mConstraintToBody1;
	Quat rotation2 = mBody2->GetRotation() * mConstraintToBody2;

	// Draw constraint orientation
	inRenderer->DrawCoordinateSystem(RMat44::sRotationTranslation(rotation1, position1), mDrawConstraintSize);

	// Draw current swing and twist
	Quat q = GetRotationInConstraintSpace();
	Quat q_swing, q_twist;
	q.GetSwingTwist(q_swing, q_twist);
	inRenderer->DrawLine(position1, position1 + mDrawConstraintSize * (rotation1 * q_twist).RotateAxisY(), Color::sWhite);
	inRenderer->DrawLine(position1, position1 + mDrawConstraintSize * (rotation1 * q_swing).RotateAxisX(), Color::sWhite);

	if (mSwingMotorState == EMotorState::Velocity || mTwistMotorState == EMotorState::Velocity)
	{
		// Draw target angular velocity
		inRenderer->DrawArrow(position1, position1 + rotation2 * mTargetAngularVelocity, Color::sRed, 0.1f);
	}
	if (mSwingMotorState == EMotorState::Position || mTwistMotorState == EMotorState::Position)
	{
		// Draw motor swing and twist
		Quat swing, twist;
		mTargetOrientation.GetSwingTwist(swing, twist);
		inRenderer->DrawLine(position1, position1 + mDrawConstraintSize * (rotation1 * twist).RotateAxisY(), Color::sYellow);
		inRenderer->DrawLine(position1, position1 + mDrawConstraintSize * (rotation1 * swing).RotateAxisX(), Color::sCyan);
	}
}

void SwingTwistConstraint::DrawConstraintLimits(DebugRenderer *inRenderer) const
{
	// Get matrix that transforms from constraint space to world space
	RMat44 constraint_to_world = RMat44::sRotationTranslation(mBody1->GetRotation() * mConstraintToBody1, mBody1->GetCenterOfMassTransform() * mLocalSpacePosition1);

	// Draw limits
	if (mSwingTwistConstraintPart.GetSwingType() == ESwingType::Pyramid)
		inRenderer->DrawSwingPyramidLimits(constraint_to_world, -mPlaneHalfConeAngle, mPlaneHalfConeAngle, -mNormalHalfConeAngle, mNormalHalfConeAngle, mDrawConstraintSize, Color::sGreen, DebugRenderer::ECastShadow::Off);
	else
		inRenderer->DrawSwingConeLimits(constraint_to_world, mPlaneHalfConeAngle, mNormalHalfConeAngle, mDrawConstraintSize, Color::sGreen, DebugRenderer::ECastShadow::Off);
	inRenderer->DrawPie(constraint_to_world.GetTranslation(), mDrawConstraintSize, constraint_to_world.GetAxisX(), constraint_to_world.GetAxisY(), mTwistMinAngle, mTwistMaxAngle, Color::sPurple, DebugRenderer::ECastShadow::Off);
}
#endif // JPH_DEBUG_RENDERER

void SwingTwistConstraint::SaveState(StateRecorder &inStream) const
{
	TwoBodyConstraint::SaveState(inStream);

	mPointConstraintPart.SaveState(inStream);
	mSwingTwistConstraintPart.SaveState(inStream);
	for (const AngleConstraintPart &c : mMotorConstraintPart)
		c.SaveState(inStream);

	inStream.Write(mSwingMotorState);
	inStream.Write(mTwistMotorState);
	inStream.Write(mTargetAngularVelocity);
	inStream.Write(mTargetOrientation);
}

void SwingTwistConstraint::RestoreState(StateRecorder &inStream)
{
	TwoBodyConstraint::RestoreState(inStream);

	mPointConstraintPart.RestoreState(inStream);
	mSwingTwistConstraintPart.RestoreState(inStream);
	for (AngleConstraintPart &c : mMotorConstraintPart)
		c.RestoreState(inStream);

	inStream.Read(mSwingMotorState);
	inStream.Read(mTwistMotorState);
	inStream.Read(mTargetAngularVelocity);
	inStream.Read(mTargetOrientation);
}

Ref<ConstraintSettings> SwingTwistConstraint::GetConstraintSettings() const
{
	SwingTwistConstraintSettings *settings = new SwingTwistConstraintSettings;
	ToConstraintSettings(*settings);
	settings->mSpace = EConstraintSpace::LocalToBodyCOM;
	settings->mPosition1 = RVec3(mLocalSpacePosition1);
	settings->mTwistAxis1 = mConstraintToBody1.RotateAxisX();
	settings->mPlaneAxis1 = mConstraintToBody1.RotateAxisZ();
	settings->mPosition2 = RVec3(mLocalSpacePosition2);
	settings->mTwistAxis2 = mConstraintToBody2.RotateAxisX();
	settings->mPlaneAxis2 = mConstraintToBody2.RotateAxisZ();
	settings->mSwingType = mSwingTwistConstraintPart.GetSwingType();
	settings->mNormalHalfConeAngle = mNormalHalfConeAngle;
	settings->mPlaneHalfConeAngle = mPlaneHalfConeAngle;
	settings->mTwistMinAngle = mTwistMinAngle;
	settings->mTwistMaxAngle = mTwistMaxAngle;
	settings->mMaxFrictionTorque = mMaxFrictionTorque;
	settings->mSwingMotorSettings = mSwingMotorSettings;
	settings->mTwistMotorSettings = mTwistMotorSettings;
	return settings;
}

JPH_NAMESPACE_END
