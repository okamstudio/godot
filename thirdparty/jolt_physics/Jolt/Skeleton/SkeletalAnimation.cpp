// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Skeleton/SkeletalAnimation.h>
#include <Jolt/Skeleton/SkeletonPose.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

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

void SkeletalAnimation::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write((uint32)mAnimatedJoints.size());
	for (const AnimatedJoint &j : mAnimatedJoints)
	{
		// Write Joint name and number of keyframes
		inStream.Write(j.mJointName);
		inStream.Write((uint32)j.mKeyframes.size());
		for (const Keyframe &k : j.mKeyframes)
		{
			inStream.Write(k.mTime);
			inStream.Write(k.mRotation);
			inStream.Write(k.mTranslation);
		}
	}

	// Save additional parameters
	inStream.Write(mIsLooping);
}

SkeletalAnimation::AnimationResult SkeletalAnimation::sRestoreFromBinaryState(StreamIn &inStream)
{
	AnimationResult result;

	Ref<SkeletalAnimation> animation = new SkeletalAnimation;

	// Restore animated joints
	uint32 len = 0;
	inStream.Read(len);
	animation->mAnimatedJoints.resize(len);
	for (AnimatedJoint &j : animation->mAnimatedJoints)
	{
		// Read joint name
		inStream.Read(j.mJointName);

		// Read keyframes
		len = 0;
		inStream.Read(len);
		j.mKeyframes.resize(len);
		for (Keyframe &k : j.mKeyframes)
		{
			inStream.Read(k.mTime);
			inStream.Read(k.mRotation);
			inStream.Read(k.mTranslation);
		}
	}

	// Read additional parameters
	inStream.Read(animation->mIsLooping);
	result.Set(animation);
	return result;
}

JPH_NAMESPACE_END
