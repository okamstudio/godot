// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Constraints/PathConstraint.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Core/StringTools.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(PathConstraintSettings)
{
	JPH_ADD_BASE_CLASS(PathConstraintSettings, TwoBodyConstraintSettings)

	JPH_ADD_ATTRIBUTE(PathConstraintSettings, mPath)
	JPH_ADD_ATTRIBUTE(PathConstraintSettings, mPathPosition)
	JPH_ADD_ATTRIBUTE(PathConstraintSettings, mPathRotation)
	JPH_ADD_ATTRIBUTE(PathConstraintSettings, mPathFraction)
	JPH_ADD_ATTRIBUTE(PathConstraintSettings, mMaxFrictionForce)
	JPH_ADD_ATTRIBUTE(PathConstraintSettings, mPositionMotorSettings)
	JPH_ADD_ENUM_ATTRIBUTE(PathConstraintSettings, mRotationConstraintType)
}

void PathConstraintSettings::SaveBinaryState(StreamOut &inStream) const
{
	ConstraintSettings::SaveBinaryState(inStream);

	mPath->SaveBinaryState(inStream);
	inStream.Write(mPathPosition);
	inStream.Write(mPathRotation);
	inStream.Write(mPathFraction);
	inStream.Write(mMaxFrictionForce);
	inStream.Write(mRotationConstraintType);
	mPositionMotorSettings.SaveBinaryState(inStream);
}

void PathConstraintSettings::RestoreBinaryState(StreamIn &inStream)
{
	ConstraintSettings::RestoreBinaryState(inStream);

	PathConstraintPath::PathResult result = PathConstraintPath::sRestoreFromBinaryState(inStream);
	if (!result.HasError())
		mPath = result.Get();
	inStream.Read(mPathPosition);
	inStream.Read(mPathRotation);
	inStream.Read(mPathFraction);
	inStream.Read(mMaxFrictionForce);
	inStream.Read(mRotationConstraintType);
	mPositionMotorSettings.RestoreBinaryState(inStream);
}

TwoBodyConstraint *PathConstraintSettings::Create(Body &inBody1, Body &inBody2) const
{
	return new PathConstraint(inBody1, inBody2, *this);
}

PathConstraint::PathConstraint(Body &inBody1, Body &inBody2, const PathConstraintSettings &inSettings) :
	TwoBodyConstraint(inBody1, inBody2, inSettings),
	mRotationConstraintType(inSettings.mRotationConstraintType),
	mMaxFrictionForce(inSettings.mMaxFrictionForce),
	mPositionMotorSettings(inSettings.mPositionMotorSettings)
{
	// Calculate transform that takes us from the path start to center of mass space of body 1
	mPathToBody1 = Mat44::sRotationTranslation(inSettings.mPathRotation, inSettings.mPathPosition - inBody1.GetShape()->GetCenterOfMass());

	SetPath(inSettings.mPath, inSettings.mPathFraction);
}

void PathConstraint::NotifyShapeChanged(const BodyID &inBodyID, Vec3Arg inDeltaCOM)
{
	if (mBody1->GetID() == inBodyID)
		mPathToBody1.SetTranslation(mPathToBody1.GetTranslation() - inDeltaCOM);
	else if (mBody2->GetID() == inBodyID)
		mPathToBody2.SetTranslation(mPathToBody2.GetTranslation() - inDeltaCOM);
}

void PathConstraint::SetPath(const PathConstraintPath *inPath, float inPathFraction)
{
	mPath = inPath;
	mPathFraction = inPathFraction;

	if (mPath != nullptr)
	{
		// Get the point on the path for this fraction
		Vec3 path_point, path_tangent, path_normal, path_binormal;
		mPath->GetPointOnPath(mPathFraction, path_point, path_tangent, path_normal, path_binormal);

		// Construct the matrix that takes us from the closest point on the path to body 2 center of mass space
		Mat44 closest_point_to_path(Vec4(path_tangent, 0), Vec4(path_binormal, 0), Vec4(path_normal, 0), Vec4(path_point, 1));
		Mat44 cp_to_body1 = mPathToBody1 * closest_point_to_path;
		mPathToBody2 = (mBody2->GetInverseCenterOfMassTransform() * mBody1->GetCenterOfMassTransform()).ToMat44() * cp_to_body1;

		// Calculate initial orientation
		if (mRotationConstraintType == EPathRotationConstraintType::FullyConstrained)
			mInvInitialOrientation = RotationEulerConstraintPart::sGetInvInitialOrientation(*mBody1, *mBody2);
	}
}

void PathConstraint::CalculateConstraintProperties(float inDeltaTime)
{
	// Get transforms of body 1 and 2
	RMat44 transform1 = mBody1->GetCenterOfMassTransform();
	RMat44 transform2 = mBody2->GetCenterOfMassTransform();

	// Get the transform of the path transform as seen from body 1 in world space
	RMat44 path_to_world_1 = transform1 * mPathToBody1;

	// Get the transform of from the point on path that body 2 is attached to in world space
	RMat44 path_to_world_2 = transform2 * mPathToBody2;

	// Calculate new closest point on path
	RVec3 position2 = path_to_world_2.GetTranslation();
	Vec3 position2_local_to_path = Vec3(path_to_world_1.InversedRotationTranslation() * position2);
	mPathFraction = mPath->GetClosestPoint(position2_local_to_path, mPathFraction);

	// Get the point on the path for this fraction
	Vec3 path_point, path_tangent, path_normal, path_binormal;
	mPath->GetPointOnPath(mPathFraction, path_point, path_tangent, path_normal, path_binormal);

	// Calculate R1 and R2
	RVec3 path_point_ws = path_to_world_1 * path_point;
	mR1 = Vec3(path_point_ws - mBody1->GetCenterOfMassPosition());
	mR2 = Vec3(position2 - mBody2->GetCenterOfMassPosition());

	// Calculate U = X2 + R2 - X1 - R1
	mU = Vec3(position2 - path_point_ws);

	// Calculate world space normals
	mPathNormal = path_to_world_1.Multiply3x3(path_normal);
	mPathBinormal = path_to_world_1.Multiply3x3(path_binormal);

	// Calculate slide axis
	mPathTangent = path_to_world_1.Multiply3x3(path_tangent);

	// Prepare constraint part for position constraint to slide along the path
	mPositionConstraintPart.CalculateConstraintProperties(*mBody1, transform1.GetRotation(), mR1 + mU, *mBody2, transform2.GetRotation(), mR2, mPathNormal, mPathBinormal);

	// Check if closest point is on the boundary of the path and if so apply limit
	if (!mPath->IsLooping() && (mPathFraction <= 0.0f || mPathFraction >= mPath->GetPathMaxFraction()))
		mPositionLimitsConstraintPart.CalculateConstraintProperties(*mBody1, mR1 + mU, *mBody2, mR2, mPathTangent);
	else
		mPositionLimitsConstraintPart.Deactivate();

	// Prepare rotation constraint part
	switch (mRotationConstraintType)
	{
	case EPathRotationConstraintType::Free:
		// No rotational limits
		break;

	case EPathRotationConstraintType::ConstrainAroundTangent:
		mHingeConstraintPart.CalculateConstraintProperties(*mBody1, transform1.GetRotation(), mPathTangent, *mBody2, transform2.GetRotation(), path_to_world_2.GetAxisX());
		break;

	case EPathRotationConstraintType::ConstrainAroundNormal:
		mHingeConstraintPart.CalculateConstraintProperties(*mBody1, transform1.GetRotation(), mPathNormal, *mBody2, transform2.GetRotation(), path_to_world_2.GetAxisZ());
		break;

	case EPathRotationConstraintType::ConstrainAroundBinormal:
		mHingeConstraintPart.CalculateConstraintProperties(*mBody1, transform1.GetRotation(), mPathBinormal, *mBody2, transform2.GetRotation(), path_to_world_2.GetAxisY());
		break;

	case EPathRotationConstraintType::ConstrainToPath:
		// We need to calculate the inverse of the rotation from body 1 to body 2 for the current path position (see: RotationEulerConstraintPart::sGetInvInitialOrientation)
		// RotationBody2 = RotationBody1 * InitialOrientation <=> InitialOrientation^-1 = RotationBody2^-1 * RotationBody1
		// We can express RotationBody2 in terms of RotationBody1: RotationBody2 = RotationBody1 * PathToBody1 * RotationClosestPointOnPath * PathToBody2^-1
		// Combining these two: InitialOrientation^-1 = PathToBody2 * (PathToBody1 * RotationClosestPointOnPath)^-1
		mInvInitialOrientation = mPathToBody2.Multiply3x3RightTransposed(mPathToBody1.Multiply3x3(Mat44(Vec4(path_tangent, 0), Vec4(path_binormal, 0), Vec4(path_normal, 0), Vec4::sZero()))).GetQuaternion();
		[[fallthrough]];

	case EPathRotationConstraintType::FullyConstrained:
		mRotationConstraintPart.CalculateConstraintProperties(*mBody1, transform1.GetRotation(), *mBody2, transform2.GetRotation());
		break;
	}

	// Motor properties
	switch (mPositionMotorState)
	{
	case EMotorState::Off:
		if (mMaxFrictionForce > 0.0f)
			mPositionMotorConstraintPart.CalculateConstraintProperties(*mBody1, mR1 + mU, *mBody2, mR2, mPathTangent);
		else
			mPositionMotorConstraintPart.Deactivate();
		break;

	case EMotorState::Velocity:
		mPositionMotorConstraintPart.CalculateConstraintProperties(*mBody1, mR1 + mU, *mBody2, mR2, mPathTangent, -mTargetVelocity);
		break;

	case EMotorState::Position:
		if (mPositionMotorSettings.mSpringSettings.HasStiffness())
		{
			// Calculate constraint value to drive to
			float c;
			if (mPath->IsLooping())
			{
				float max_fraction = mPath->GetPathMaxFraction();
				c = fmod(mPathFraction - mTargetPathFraction, max_fraction);
				float half_max_fraction = 0.5f * max_fraction;
				if (c > half_max_fraction)
					c -= max_fraction;
				else if (c < -half_max_fraction)
					c += max_fraction;
			}
			else
				c = mPathFraction - mTargetPathFraction;
			mPositionMotorConstraintPart.CalculateConstraintPropertiesWithSettings(inDeltaTime, *mBody1, mR1 + mU, *mBody2, mR2, mPathTangent, 0.0f, c, mPositionMotorSettings.mSpringSettings);
		}
		else
			mPositionMotorConstraintPart.Deactivate();
		break;
	}
}

void PathConstraint::SetupVelocityConstraint(float inDeltaTime)
{
	CalculateConstraintProperties(inDeltaTime);
}

void PathConstraint::ResetWarmStart()
{
	mPositionMotorConstraintPart.Deactivate();
	mPositionConstraintPart.Deactivate();
	mPositionLimitsConstraintPart.Deactivate();
	mHingeConstraintPart.Deactivate();
	mRotationConstraintPart.Deactivate();
}

void PathConstraint::WarmStartVelocityConstraint(float inWarmStartImpulseRatio)
{
	// Warm starting: Apply previous frame impulse
	mPositionMotorConstraintPart.WarmStart(*mBody1, *mBody2, mPathTangent, inWarmStartImpulseRatio);
	mPositionConstraintPart.WarmStart(*mBody1, *mBody2, mPathNormal, mPathBinormal, inWarmStartImpulseRatio);
	mPositionLimitsConstraintPart.WarmStart(*mBody1, *mBody2, mPathTangent, inWarmStartImpulseRatio);

	switch (mRotationConstraintType)
	{
	case EPathRotationConstraintType::Free:
		// No rotational limits
		break;

	case EPathRotationConstraintType::ConstrainAroundTangent:
	case EPathRotationConstraintType::ConstrainAroundNormal:
	case EPathRotationConstraintType::ConstrainAroundBinormal:
		mHingeConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
		break;

	case EPathRotationConstraintType::ConstrainToPath:
	case EPathRotationConstraintType::FullyConstrained:
		mRotationConstraintPart.WarmStart(*mBody1, *mBody2, inWarmStartImpulseRatio);
		break;
	}
}

bool PathConstraint::SolveVelocityConstraint(float inDeltaTime)
{
	// Solve motor
	bool motor = false;
	if (mPositionMotorConstraintPart.IsActive())
	{
		switch (mPositionMotorState)
		{
		case EMotorState::Off:
			{
				float max_lambda = mMaxFrictionForce * inDeltaTime;
				motor = mPositionMotorConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mPathTangent, -max_lambda, max_lambda);
				break;
			}

		case EMotorState::Velocity:
		case EMotorState::Position:
			motor = mPositionMotorConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mPathTangent, inDeltaTime * mPositionMotorSettings.mMinForceLimit, inDeltaTime * mPositionMotorSettings.mMaxForceLimit);
			break;
		}
	}

	// Solve position constraint along 2 axis
	bool pos = mPositionConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mPathNormal, mPathBinormal);

	// Solve limits along path axis
	bool limit = false;
	if (mPositionLimitsConstraintPart.IsActive())
	{
		if (mPathFraction <= 0.0f)
			limit = mPositionLimitsConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mPathTangent, 0, FLT_MAX);
		else
		{
			JPH_ASSERT(mPathFraction >= mPath->GetPathMaxFraction());
			limit = mPositionLimitsConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2, mPathTangent, -FLT_MAX, 0);
		}
	}

	// Solve rotational constraint
	// Note, this is not entirely correct, we should apply a velocity constraint so that the body will actually follow the path
	// by looking at the derivative of the tangent, normal or binormal but we don't. This means the position constraint solver
	// will need to correct the orientation error that builds up, which in turn means that the simulation is not physically correct.
	bool rot = false;
	switch (mRotationConstraintType)
	{
	case EPathRotationConstraintType::Free:
		// No rotational limits
		break;

	case EPathRotationConstraintType::ConstrainAroundTangent:
	case EPathRotationConstraintType::ConstrainAroundNormal:
	case EPathRotationConstraintType::ConstrainAroundBinormal:
		rot = mHingeConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);
		break;

	case EPathRotationConstraintType::ConstrainToPath:
	case EPathRotationConstraintType::FullyConstrained:
		rot = mRotationConstraintPart.SolveVelocityConstraint(*mBody1, *mBody2);
		break;
	}

	return motor || pos || limit || rot;
}

bool PathConstraint::SolvePositionConstraint(float inDeltaTime, float inBaumgarte)
{
	// Update constraint properties (bodies may have moved)
	CalculateConstraintProperties(inDeltaTime);

	// Solve position constraint along 2 axis
	bool pos = mPositionConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, mU, mPathNormal, mPathBinormal, inBaumgarte);

	// Solve limits along path axis
	bool limit = false;
	if (mPositionLimitsConstraintPart.IsActive())
	{
		if (mPathFraction <= 0.0f)
			limit = mPositionLimitsConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, mPathTangent, mU.Dot(mPathTangent), inBaumgarte);
		else
		{
			JPH_ASSERT(mPathFraction >= mPath->GetPathMaxFraction());
			limit = mPositionLimitsConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, mPathTangent, mU.Dot(mPathTangent), inBaumgarte);
		}
	}

	// Solve rotational constraint
	bool rot = false;
	switch (mRotationConstraintType)
	{
	case EPathRotationConstraintType::Free:
		// No rotational limits
		break;

	case EPathRotationConstraintType::ConstrainAroundTangent:
	case EPathRotationConstraintType::ConstrainAroundNormal:
	case EPathRotationConstraintType::ConstrainAroundBinormal:
		rot = mHingeConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, inBaumgarte);
		break;

	case EPathRotationConstraintType::ConstrainToPath:
	case EPathRotationConstraintType::FullyConstrained:
		rot = mRotationConstraintPart.SolvePositionConstraint(*mBody1, *mBody2, mInvInitialOrientation, inBaumgarte);
		break;
	}

	return pos || limit || rot;
}

#ifdef JPH_DEBUG_RENDERER
void PathConstraint::DrawConstraint(DebugRenderer *inRenderer) const
{
	if (mPath != nullptr)
	{
		// Draw the path in world space
		RMat44 path_to_world = mBody1->GetCenterOfMassTransform() * mPathToBody1;
		mPath->DrawPath(inRenderer, path_to_world);

		// Draw anchor point of both bodies in world space
		RVec3 x1 = mBody1->GetCenterOfMassPosition() + mR1;
		RVec3 x2 = mBody2->GetCenterOfMassPosition() + mR2;
		inRenderer->DrawMarker(x1, Color::sYellow, 0.1f);
		inRenderer->DrawMarker(x2, Color::sYellow, 0.1f);
		inRenderer->DrawArrow(x1, x1 + mPathTangent, Color::sBlue, 0.1f);
		inRenderer->DrawArrow(x1, x1 + mPathNormal, Color::sRed, 0.1f);
		inRenderer->DrawArrow(x1, x1 + mPathBinormal, Color::sGreen, 0.1f);
		inRenderer->DrawText3D(x1, StringFormat("%.1f", (double)mPathFraction));

		// Draw motor
		switch (mPositionMotorState)
		{
		case EMotorState::Position:
			{
				// Draw target marker
				Vec3 position, tangent, normal, binormal;
				mPath->GetPointOnPath(mTargetPathFraction, position, tangent, normal, binormal);
				inRenderer->DrawMarker(path_to_world * position, Color::sYellow, 1.0f);
				break;
			}

		case EMotorState::Velocity:
			{
				RVec3 position = mBody2->GetCenterOfMassPosition() + mR2;
				inRenderer->DrawArrow(position, position + mPathTangent * mTargetVelocity, Color::sRed, 0.1f);
				break;
			}

		case EMotorState::Off:
			break;
		}
	}
}
#endif // JPH_DEBUG_RENDERER

void PathConstraint::SaveState(StateRecorder &inStream) const
{
	TwoBodyConstraint::SaveState(inStream);

	mPositionConstraintPart.SaveState(inStream);
	mPositionLimitsConstraintPart.SaveState(inStream);
	mPositionMotorConstraintPart.SaveState(inStream);
	mHingeConstraintPart.SaveState(inStream);
	mRotationConstraintPart.SaveState(inStream);

	inStream.Write(mMaxFrictionForce);
	inStream.Write(mPositionMotorSettings);
	inStream.Write(mPositionMotorState);
	inStream.Write(mTargetVelocity);
	inStream.Write(mTargetPathFraction);
	inStream.Write(mPathFraction);
}

void PathConstraint::RestoreState(StateRecorder &inStream)
{
	TwoBodyConstraint::RestoreState(inStream);

	mPositionConstraintPart.RestoreState(inStream);
	mPositionLimitsConstraintPart.RestoreState(inStream);
	mPositionMotorConstraintPart.RestoreState(inStream);
	mHingeConstraintPart.RestoreState(inStream);
	mRotationConstraintPart.RestoreState(inStream);

	inStream.Read(mMaxFrictionForce);
	inStream.Read(mPositionMotorSettings);
	inStream.Read(mPositionMotorState);
	inStream.Read(mTargetVelocity);
	inStream.Read(mTargetPathFraction);
	inStream.Read(mPathFraction);
}

Ref<ConstraintSettings> PathConstraint::GetConstraintSettings() const
{
	JPH_ASSERT(false); // Not implemented yet
	return nullptr;
}

JPH_NAMESPACE_END
