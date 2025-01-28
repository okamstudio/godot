// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Reference.h>
#include <Jolt/Core/Result.h>
#include <Jolt/Core/StreamUtils.h>
#include <Jolt/ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class SkeletonPose;

/// Resource for a skinned animation
class JPH_EXPORT SkeletalAnimation : public RefTarget<SkeletalAnimation>
{
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, SkeletalAnimation)

public:
	/// Contains the current state of a joint, a local space transformation relative to its parent joint
	class JointState
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, JointState)

	public:
		/// Convert from a local space matrix
		void							FromMatrix(Mat44Arg inMatrix);

		/// Convert to matrix representation
		inline Mat44					ToMatrix() const									{ return Mat44::sRotationTranslation(mRotation, mTranslation); }

		Quat							mRotation = Quat::sIdentity();						///< Local space rotation of the joint
		Vec3							mTranslation = Vec3::sZero();						///< Local space translation of the joint
	};

	/// Contains the state of a single joint at a particular time
	class Keyframe : public JointState
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, Keyframe)

	public:
		float							mTime = 0.0f;										///< Time of keyframe in seconds
	};

	using KeyframeVector = Array<Keyframe>;

	/// Contains the animation for a single joint
	class AnimatedJoint
	{
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(JPH_EXPORT, AnimatedJoint)

	public:
		String							mJointName;											///< Name of the joint
		KeyframeVector					mKeyframes;											///< List of keyframes over time
	};

	using AnimatedJointVector = Array<AnimatedJoint>;

	/// Get the length (in seconds) of this animation
	float								GetDuration() const;

	/// Scale the size of all joints by inScale
	void								ScaleJoints(float inScale);

	/// If the animation is looping or not. If an animation is looping, the animation will continue playing after completion
	void 								SetIsLooping(bool inIsLooping)						{ mIsLooping = inIsLooping; }
	bool								IsLooping() const									{ return mIsLooping; }

	/// Get the (interpolated) joint transforms at time inTime
	void								Sample(float inTime, SkeletonPose &ioPose) const;

	/// Get joint samples
	const AnimatedJointVector &			GetAnimatedJoints() const							{ return mAnimatedJoints; }
	AnimatedJointVector &				GetAnimatedJoints()									{ return mAnimatedJoints; }

	/// Saves the state of this animation in binary form to inStream.
	void								SaveBinaryState(StreamOut &inStream) const;

	using AnimationResult = Result<Ref<SkeletalAnimation>>;

	/// Restore a saved ragdoll from inStream
	static AnimationResult				sRestoreFromBinaryState(StreamIn &inStream);

private:
	AnimatedJointVector					mAnimatedJoints;									///< List of joints and keyframes
	bool								mIsLooping = true;									///< If this animation loops back to start
};

JPH_NAMESPACE_END
