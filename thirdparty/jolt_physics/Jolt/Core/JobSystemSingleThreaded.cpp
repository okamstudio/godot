// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemSingleThreaded.h>

JPH_NAMESPACE_BEGIN

void JobSystemSingleThreaded::Init(uint inMaxJobs)
{
	mJobs.Init(inMaxJobs, inMaxJobs);
}

JobHandle JobSystemSingleThreaded::CreateJob(const char *inJobName, ColorArg inColor, const JobFunction &inJobFunction, uint32 inNumDependencies)
{
	// Construct an object
	uint32 index = mJobs.ConstructObject(inJobName, inColor, this, inJobFunction, inNumDependencies);
	JPH_ASSERT(index != AvailableJobs::cInvalidObjectIndex);
	Job *job = &mJobs.Get(index);

	// Construct handle to keep a reference, the job is queued below and will immediately complete
	JobHandle handle(job);

	// If there are no dependencies, queue the job now
	if (inNumDependencies == 0)
		QueueJob(job);

	// Return the handle
	return handle;
}

void JobSystemSingleThreaded::FreeJob(Job *inJob)
{
	mJobs.DestructObject(inJob);
}

void JobSystemSingleThreaded::QueueJob(Job *inJob)
{
	inJob->Execute();
}

void JobSystemSingleThreaded::QueueJobs(Job **inJobs, uint inNumJobs)
{
	for (uint i = 0; i < inNumJobs; ++i)
		QueueJob(inJobs[i]);
}

JobSystem::Barrier *JobSystemSingleThreaded::CreateBarrier()
{
	return &mDummyBarrier;
}

void JobSystemSingleThreaded::DestroyBarrier(Barrier *inBarrier)
{
	// There's nothing to do here, the barrier is just a dummy
}

void JobSystemSingleThreaded::WaitForJobs(Barrier *inBarrier)
{
	// There's nothing to do here, the barrier is just a dummy, we just execute the jobs immediately
}

JPH_NAMESPACE_END
