// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Skeleton/SkeletalAnimation.h>
#include <Jolt/Skeleton/SkeletonPose.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(SkeletalAnimation::JointState)
{
	JPH_ADD_ATTRIBUTE(JointState, mRotation)
	JPH_ADD_ATTRIBUTE(JointState, mTranslation)
}

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(SkeletalAnimation::Keyframe)
{
	JPH_ADD_BASE_CLASS(Keyframe, JointState)

	JPH_ADD_ATTRIBUTE(Keyframe, mTime)
}

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(SkeletalAnimation::AnimatedJoint)
{
	JPH_ADD_ATTRIBUTE(AnimatedJoint, mJointName)
	JPH_ADD_ATTRIBUTE(AnimatedJoint, mKeyframes)
}

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(SkeletalAnimation)
{
	JPH_ADD_ATTRIBUTE(SkeletalAnimation, mAnimatedJoints)
	JPH_ADD_ATTRIBUTE(SkeletalAnimation, mIsLooping)
}


void SkeletalAnimation::JointState::FromMatrix(Mat44Arg inMatrix)
{
	mRotation = inMatrix.GetQuaternion();
	mTranslation = inMatrix.GetTranslation();
}

float SkeletalAnimation::GetDuration() const
{
	if (!mAnimatedJoints.empty() && !mAnimatedJoints[0].mKeyframes.empty())
		return mAnimatedJoints[0].mKeyframes.back().mTime;
	else
		return 0.0f;
}

void SkeletalAnimation::ScaleJoints(float inScale)
{
	for (SkeletalAnimation::AnimatedJoint &j : mAnimatedJoints)
		for (SkeletalAnimation::Keyframe &k : j.mKeyframes)
			k.mTranslation *= inScale;
}

void SkeletalAnimation::Sample(float inTime, SkeletonPose &ioPose) const
{
	// Correct time when animation is looping
	JPH_ASSERT(inTime >= 0.0f);
	float duration = GetDuration();
	float time = duration > 0.0f && mIsLooping? fmod(inTime, duration) : inTime;

	for (const AnimatedJoint &aj : mAnimatedJoints)
	{
		// Do binary search for keyframe
		int high = (int)aj.mKeyframes.size(), low = -1;
		while (high - low > 1)
		{
			int probe = (high + low) / 2;
			if (aj.mKeyframes[probe].mTime < time)
				low = probe;
			else
				high = probe;
		}

		JointState &state = ioPose.GetJoint(ioPose.GetSkeleton()->GetJointIndex(aj.mJointName));

		if (low == -1)
		{
			// Before first key, return first key
			state = static_cast<const JointState &>(aj.mKeyframes.front());
		}
		else if (high == (int)aj.mKeyframes.size())
		{
			// Beyond last key, return last key
			state = static_cast<const JointState &>(aj.mKeyframes.back());
		}
		else
		{
			// Interpolate
			const Keyframe &s1 = aj.mKeyframes[low];
			const Keyframe &s2 = aj.mKeyframes[low + 1];

			float fraction = (time - s1.mTime) / (s2.mTime - s1.mTime);
			JPH_ASSERT(fraction >= 0.0f && fraction <= 1.0f);

			state.mTranslation = (1.0f - fraction) * s1.mTranslation + fraction * s2.mTranslation;
			JPH_ASSERT(s1.mRotation.IsNormalized());
			JPH_ASSERT(s2.mRotation.IsNormalized());
			state.mRotation = s1.mRotation.SLERP(s2.mRotation, fraction);
			JPH_ASSERT(state.mRotation.IsNormalized());
		}
	}
}

JPH_NAMESPACE_END
