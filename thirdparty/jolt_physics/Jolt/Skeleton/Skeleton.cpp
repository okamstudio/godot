// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Skeleton/Skeleton.h>
#include <Jolt/ObjectStream/TypeDeclarations.h>
#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(Skeleton::Joint)
{
	JPH_ADD_ATTRIBUTE(Joint, mName)
	JPH_ADD_ATTRIBUTE(Joint, mParentName)
}

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(Skeleton)
{
	JPH_ADD_ATTRIBUTE(Skeleton, mJoints)
}

int Skeleton::GetJointIndex(const string_view &inName) const
{
	for (int i = 0; i < (int)mJoints.size(); ++i)
		if (mJoints[i].mName == inName)
			return i;

	return -1;
}

void Skeleton::CalculateParentJointIndices()
{
	for (Joint &j : mJoints)
		j.mParentJointIndex = GetJointIndex(j.mParentName);
}

bool Skeleton::AreJointsCorrectlyOrdered() const
{
	for (int i = 0; i < (int)mJoints.size(); ++i)
		if (mJoints[i].mParentJointIndex >= i)
			return false;

	return true;
}

void Skeleton::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write((uint32)mJoints.size());
	for (const Joint &j : mJoints)
	{
		inStream.Write(j.mName);
		inStream.Write(j.mParentJointIndex);
		inStream.Write(j.mParentName);
	}
}

Skeleton::SkeletonResult Skeleton::sRestoreFromBinaryState(StreamIn &inStream)
{
	Ref<Skeleton> skeleton = new Skeleton;

	uint32 len = 0;
	inStream.Read(len);
	skeleton->mJoints.resize(len);
	for (Joint &j : skeleton->mJoints)
	{
		inStream.Read(j.mName);
		inStream.Read(j.mParentJointIndex);
		inStream.Read(j.mParentName);
	}

	SkeletonResult result;
	if (inStream.IsEOF() || inStream.IsFailed())
		result.SetError("Failed to read skeleton from stream");
	else
		result.Set(skeleton);
	return result;
}

JPH_NAMESPACE_END
