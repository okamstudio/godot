// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/Profiler.h>

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <thread>
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

JobSystemWithBarrier::BarrierImpl::BarrierImpl()
{
	for (atomic<Job *> &j : mJobs)
		j = nullptr;
}

JobSystemWithBarrier::BarrierImpl::~BarrierImpl()
{
	JPH_ASSERT(IsEmpty());
}

void JobSystemWithBarrier::BarrierImpl::AddJob(const JobHandle &inJob)
{
	JPH_PROFILE_FUNCTION();

	bool release_semaphore = false;

	// Set the barrier on the job, this returns true if the barrier was successfully set (otherwise the job is already done and we don't need to add it to our list)
	Job *job = inJob.GetPtr();
	if (job->SetBarrier(this))
	{
		// If the job can be executed we want to release the semaphore an extra time to allow the waiting thread to start executing it
		mNumToAcquire++;
		if (job->CanBeExecuted())
		{
			release_semaphore = true;
			mNumToAcquire++;
		}

		// Add the job to our job list
		job->AddRef();
		uint write_index = mJobWriteIndex++;
		while (write_index - mJobReadIndex >= cMaxJobs)
		{
			JPH_ASSERT(false, "Barrier full, stalling!");
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		mJobs[write_index & (cMaxJobs - 1)] = job;
	}

	// Notify waiting thread that a new executable job is available
	if (release_semaphore)
		mSemaphore.Release();
}

void JobSystemWithBarrier::BarrierImpl::AddJobs(const JobHandle *inHandles, uint inNumHandles)
{
	JPH_PROFILE_FUNCTION();

	bool release_semaphore = false;

	for (const JobHandle *handle = inHandles, *handles_end = inHandles + inNumHandles; handle < handles_end; ++handle)
	{
		// Set the barrier on the job, this returns true if the barrier was successfully set (otherwise the job is already done and we don't need to add it to our list)
		Job *job = handle->GetPtr();
		if (job->SetBarrier(this))
		{
			// If the job can be executed we want to release the semaphore an extra time to allow the waiting thread to start executing it
			mNumToAcquire++;
			if (!release_semaphore && job->CanBeExecuted())
			{
				release_semaphore = true;
				mNumToAcquire++;
			}

			// Add the job to our job list
			job->AddRef();
			uint write_index = mJobWriteIndex++;
			while (write_index - mJobReadIndex >= cMaxJobs)
			{
				JPH_ASSERT(false, "Barrier full, stalling!");
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
			mJobs[write_index & (cMaxJobs - 1)] = job;
		}
	}

	// Notify waiting thread that a new executable job is available
	if (release_semaphore)
		mSemaphore.Release();
}

void JobSystemWithBarrier::BarrierImpl::OnJobFinished(Job *inJob)
{
	JPH_PROFILE_FUNCTION();

	mSemaphore.Release();
}

void JobSystemWithBarrier::BarrierImpl::Wait()
{
	while (mNumToAcquire > 0)
	{
		{
			JPH_PROFILE("Execute Jobs");

			// Go through all jobs
			bool has_executed;
			do
			{
				has_executed = false;

				// Loop through the jobs and erase jobs from the beginning of the list that are done
				while (mJobReadIndex < mJobWriteIndex)
				{
					atomic<Job *> &job = mJobs[mJobReadIndex & (cMaxJobs - 1)];
					Job *job_ptr = job.load();
					if (job_ptr == nullptr || !job_ptr->IsDone())
						break;

					// Job is finished, release it
					job_ptr->Release();
					job = nullptr;
					++mJobReadIndex;
				}

				// Loop through the jobs and execute the first executable job
				for (uint index = mJobReadIndex; index < mJobWriteIndex; ++index)
				{
					const atomic<Job *> &job = mJobs[index & (cMaxJobs - 1)];
					Job *job_ptr = job.load();
					if (job_ptr != nullptr && job_ptr->CanBeExecuted())
					{
						// This will only execute the job if it has not already executed
						job_ptr->Execute();
						has_executed = true;
						break;
					}
				}

			} while (has_executed);
		}

		// Wait for another thread to wake us when either there is more work to do or when all jobs have completed.
		// When there have been multiple releases, we acquire them all at the same time to avoid needlessly spinning on executing jobs.
		// Note that using GetValue is inherently unsafe since we can read a stale value, but this is not an issue here as this is the only
		// place where we acquire the semaphore. Other threads only release it, so we can only read a value that is lower or equal to the actual value.
		int num_to_acquire = max(1, mSemaphore.GetValue());
		mSemaphore.Acquire(num_to_acquire);
		mNumToAcquire -= num_to_acquire;
	}

	// All jobs should be done now, release them
	while (mJobReadIndex < mJobWriteIndex)
	{
		atomic<Job *> &job = mJobs[mJobReadIndex & (cMaxJobs - 1)];
		Job *job_ptr = job.load();
		JPH_ASSERT(job_ptr != nullptr && job_ptr->IsDone());
		job_ptr->Release();
		job = nullptr;
		++mJobReadIndex;
	}
}

void JobSystemWithBarrier::Init(uint inMaxBarriers)
{
	JPH_ASSERT(mBarriers == nullptr); // Already initialized?

	// Init freelist of barriers
	mMaxBarriers = inMaxBarriers;
	mBarriers = new BarrierImpl [inMaxBarriers];
}

JobSystemWithBarrier::JobSystemWithBarrier(uint inMaxBarriers)
{
	Init(inMaxBarriers);
}

JobSystemWithBarrier::~JobSystemWithBarrier()
{
	// Ensure that none of the barriers are used
#ifdef JPH_ENABLE_ASSERTS
	for (const BarrierImpl *b = mBarriers, *b_end = mBarriers + mMaxBarriers; b < b_end; ++b)
		JPH_ASSERT(!b->mInUse);
#endif // JPH_ENABLE_ASSERTS
	delete [] mBarriers;
}

JobSystem::Barrier *JobSystemWithBarrier::CreateBarrier()
{
	JPH_PROFILE_FUNCTION();

	// Find the first unused barrier
	for (uint32 index = 0; index < mMaxBarriers; ++index)
	{
		bool expected = false;
		if (mBarriers[index].mInUse.compare_exchange_strong(expected, true))
			return &mBarriers[index];
	}

	return nullptr;
}

void JobSystemWithBarrier::DestroyBarrier(Barrier *inBarrier)
{
	JPH_PROFILE_FUNCTION();

	// Check that no jobs are in the barrier
	JPH_ASSERT(static_cast<BarrierImpl *>(inBarrier)->IsEmpty());

	// Flag the barrier as unused
	bool expected = true;
	static_cast<BarrierImpl *>(inBarrier)->mInUse.compare_exchange_strong(expected, false);
	JPH_ASSERT(expected);
}

void JobSystemWithBarrier::WaitForJobs(Barrier *inBarrier)
{
	JPH_PROFILE_FUNCTION();

	// Let our barrier implementation wait for the jobs
	static_cast<BarrierImpl *>(inBarrier)->Wait();
}

JPH_NAMESPACE_END
