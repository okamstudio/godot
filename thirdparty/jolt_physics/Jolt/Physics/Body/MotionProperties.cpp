// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

void MotionProperties::SetMassProperties(EAllowedDOFs inAllowedDOFs, const MassProperties &inMassProperties)
{
	// Store allowed DOFs
	mAllowedDOFs = inAllowedDOFs;

	// Decompose DOFs
	uint allowed_translation_axis = uint(inAllowedDOFs) & 0b111;
	uint allowed_rotation_axis = (uint(inAllowedDOFs) >> 3) & 0b111;

	// Set inverse mass
	if (allowed_translation_axis == 0)
	{
		// No translation possible
		mInvMass = 0.0f;
	}
	else
	{
		JPH_ASSERT(inMassProperties.mMass > 0.0f, "Invalid mass. "
			"Some shapes like MeshShape or TriangleShape cannot calculate mass automatically, "
			"in this case you need to provide it by setting BodyCreationSettings::mOverrideMassProperties and mMassPropertiesOverride.");
		mInvMass = 1.0f / inMassProperties.mMass;
	}

	if (allowed_rotation_axis == 0)
	{
		// No rotation possible
		mInvInertiaDiagonal = Vec3::sZero();
		mInertiaRotation = Quat::sIdentity();
	}
	else
	{
		// Set inverse inertia
		Mat44 rotation;
		Vec3 diagonal;
		if (inMassProperties.DecomposePrincipalMomentsOfInertia(rotation, diagonal)
			&& !diagonal.IsNearZero())
		{
			mInvInertiaDiagonal = diagonal.Reciprocal();
			mInertiaRotation = rotation.GetQuaternion();
		}
		else
		{
			// Failed! Fall back to inertia tensor of sphere with radius 1.
			mInvInertiaDiagonal = Vec3::sReplicate(2.5f * mInvMass);
			mInertiaRotation = Quat::sIdentity();
		}
	}

	JPH_ASSERT(mInvMass != 0.0f || mInvInertiaDiagonal != Vec3::sZero(), "Can't lock all axes, use a static body for this. This will crash with a division by zero later!");
}

void MotionProperties::SaveState(StateRecorder &inStream) const
{
	// Only write properties that can change at runtime
	inStream.Write(mLinearVelocity);
	inStream.Write(mAngularVelocity);
	inStream.Write(mForce);
	inStream.Write(mTorque);
#ifdef JPH_DOUBLE_PRECISION
	inStream.Write(mSleepTestOffset);
#endif // JPH_DOUBLE_PRECISION
	inStream.Write(mSleepTestSpheres);
	inStream.Write(mSleepTestTimer);
	inStream.Write(mAllowSleeping);
}

void MotionProperties::RestoreState(StateRecorder &inStream)
{
	inStream.Read(mLinearVelocity);
	inStream.Read(mAngularVelocity);
	inStream.Read(mForce);
	inStream.Read(mTorque);
#ifdef JPH_DOUBLE_PRECISION
	inStream.Read(mSleepTestOffset);
#endif // JPH_DOUBLE_PRECISION
	inStream.Read(mSleepTestSpheres);
	inStream.Read(mSleepTestTimer);
	inStream.Read(mAllowSleeping);
}

JPH_NAMESPACE_END
