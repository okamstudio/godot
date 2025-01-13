// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Skeleton/SkeletonPose.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Jolt/Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

void SkeletonPose::SetSkeleton(const Skeleton *inSkeleton)
{
	mSkeleton = inSkeleton;

	mJoints.resize(mSkeleton->GetJointCount());
	mJointMatrices.resize(mSkeleton->GetJointCount());
}

void SkeletonPose::CalculateJointMatrices()
{
	for (int i = 0; i < (int)mJoints.size(); ++i)
	{
		mJointMatrices[i] = mJoints[i].ToMatrix();

		int parent = mSkeleton->GetJoint(i).mParentJointIndex;
		if (parent >= 0)
		{
			JPH_ASSERT(parent < i, "Joints must be ordered: parents first");
			mJointMatrices[i] = mJointMatrices[parent] * mJointMatrices[i];
		}
	}
}

void SkeletonPose::CalculateJointStates()
{
	for (int i = 0; i < (int)mJoints.size(); ++i)
	{
		Mat44 local_transform;
		int parent = mSkeleton->GetJoint(i).mParentJointIndex;
		if (parent >= 0)
			local_transform = mJointMatrices[parent].Inversed() * mJointMatrices[i];
		else
			local_transform = mJointMatrices[i];

		JointState &joint = mJoints[i];
		joint.mTranslation = local_transform.GetTranslation();
		joint.mRotation = local_transform.GetQuaternion();
	}
}

void SkeletonPose::CalculateLocalSpaceJointMatrices(Mat44 *outMatrices) const
{
	for (int i = 0; i < (int)mJoints.size(); ++i)
		outMatrices[i] = mJoints[i].ToMatrix();
}

#ifdef JPH_DEBUG_RENDERER
void SkeletonPose::Draw(const DrawSettings &inDrawSettings, DebugRenderer *inRenderer, RMat44Arg inOffset) const
{
	RMat44 offset = inOffset * RMat44::sTranslation(mRootOffset);

	const Skeleton::JointVector &joints = mSkeleton->GetJoints();

	for (int b = 0; b < mSkeleton->GetJointCount(); ++b)
	{
		RMat44 joint_transform = offset * mJointMatrices[b];

		if (inDrawSettings.mDrawJoints)
		{
			int parent = joints[b].mParentJointIndex;
			if (parent >= 0)
				inRenderer->DrawLine(offset * mJointMatrices[parent].GetTranslation(), joint_transform.GetTranslation(), Color::sGreen);
		}

		if (inDrawSettings.mDrawJointOrientations)
			inRenderer->DrawCoordinateSystem(joint_transform, 0.05f);

		if (inDrawSettings.mDrawJointNames)
			inRenderer->DrawText3D(joint_transform.GetTranslation(), joints[b].mName, Color::sWhite, 0.05f);
	}
}
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_END
