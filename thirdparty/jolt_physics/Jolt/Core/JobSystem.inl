// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

JPH_NAMESPACE_BEGIN

void JobSystem::Job::AddDependency(int inCount)
{
	JPH_IF_ENABLE_ASSERTS(uint32 old_value =) mNumDependencies.fetch_add(inCount, memory_order_relaxed);
	JPH_ASSERT(old_value > 0 && old_value != cExecutingState && old_value != cDoneState, "Job is queued, running or done, it is not allowed to add a dependency to a running job");
}

bool JobSystem::Job::RemoveDependency(int inCount)
{
	uint32 old_value = mNumDependencies.fetch_sub(inCount, memory_order_release);
	JPH_ASSERT(old_value != cExecutingState && old_value != cDoneState, "Job is running or done, it is not allowed to add a dependency to a running job");
	uint32 new_value = old_value - inCount;
	JPH_ASSERT(old_value > new_value, "Test wrap around, this is a logic error");
	return new_value == 0;
}

void JobSystem::Job::RemoveDependencyAndQueue(int inCount)
{
	if (RemoveDependency(inCount))
		mJobSystem->QueueJob(this);
}

void JobSystem::JobHandle::sRemoveDependencies(const JobHandle *inHandles, uint inNumHandles, int inCount)
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inNumHandles > 0);

	// Get the job system, all jobs should be part of the same job system
	JobSystem *job_system = inHandles->GetPtr()->GetJobSystem();

	// Allocate a buffer to store the jobs that need to be queued
	Job **jobs_to_queue = (Job **)JPH_STACK_ALLOC(inNumHandles * sizeof(Job *));
	Job **next_job = jobs_to_queue;

	// Remove the dependencies on all jobs
	for (const JobHandle *handle = inHandles, *handle_end = inHandles + inNumHandles; handle < handle_end; ++handle)
	{
		Job *job = handle->GetPtr();
		JPH_ASSERT(job->GetJobSystem() == job_system); // All jobs should belong to the same job system
		if (job->RemoveDependency(inCount))
			*(next_job++) = job;
	}

	// If any jobs need to be scheduled, schedule them as a batch
	uint num_jobs_to_queue = uint(next_job - jobs_to_queue);
	if (num_jobs_to_queue != 0)
		job_system->QueueJobs(jobs_to_queue, num_jobs_to_queue);
}

JPH_NAMESPACE_END
