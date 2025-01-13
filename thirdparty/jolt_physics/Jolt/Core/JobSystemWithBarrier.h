// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/Semaphore.h>

JPH_NAMESPACE_BEGIN

/// Implementation of the Barrier class for a JobSystem
///
/// This class can be used to make it easier to create a new JobSystem implementation that integrates with your own job system.
/// It will implement all functionality relating to barriers, so the only functions that are left to be implemented are:
///
/// * JobSystem::GetMaxConcurrency
/// * JobSystem::CreateJob
/// * JobSystem::FreeJob
/// * JobSystem::QueueJob/QueueJobs
///
/// See instructions in JobSystem for more information on how to implement these.
class JPH_EXPORT JobSystemWithBarrier : public JobSystem
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructs barriers
	/// @see JobSystemWithBarrier::Init
	explicit				JobSystemWithBarrier(uint inMaxBarriers);
							JobSystemWithBarrier() = default;
	virtual					~JobSystemWithBarrier() override;

	/// Initialize the barriers
	/// @param inMaxBarriers Max number of barriers that can be allocated at any time
	void					Init(uint inMaxBarriers);

	// See JobSystem
	virtual Barrier *		CreateBarrier() override;
	virtual void			DestroyBarrier(Barrier *inBarrier) override;
	virtual void			WaitForJobs(Barrier *inBarrier) override;

private:
	class BarrierImpl : public Barrier
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Constructor
							BarrierImpl();
		virtual				~BarrierImpl() override;

		// See Barrier
		virtual void		AddJob(const JobHandle &inJob) override;
		virtual void		AddJobs(const JobHandle *inHandles, uint inNumHandles) override;

		/// Check if there are any jobs in the job barrier
		inline bool			IsEmpty() const									{ return mJobReadIndex == mJobWriteIndex; }

		/// Wait for all jobs in this job barrier, while waiting, execute jobs that are part of this barrier on the current thread
		void				Wait();

		/// Flag to indicate if a barrier has been handed out
		atomic<bool>		mInUse { false };

	protected:
		/// Called by a Job to mark that it is finished
		virtual void		OnJobFinished(Job *inJob) override;

		/// Jobs queue for the barrier
		static constexpr uint cMaxJobs = 2048;
		static_assert(IsPowerOf2(cMaxJobs));								// We do bit operations and require max jobs to be a power of 2
		atomic<Job *>		mJobs[cMaxJobs];								///< List of jobs that are part of this barrier, nullptrs for empty slots
		alignas(JPH_CACHE_LINE_SIZE) atomic<uint> mJobReadIndex { 0 };		///< First job that could be valid (modulo cMaxJobs), can be nullptr if other thread is still working on adding the job
		alignas(JPH_CACHE_LINE_SIZE) atomic<uint> mJobWriteIndex { 0 };		///< First job that can be written (modulo cMaxJobs)
		atomic<int>			mNumToAcquire { 0 };							///< Number of times the semaphore has been released, the barrier should acquire the semaphore this many times (written at the same time as mJobWriteIndex so ok to put in same cache line)
		Semaphore			mSemaphore;										///< Semaphore used by finishing jobs to signal the barrier that they're done
	};

	/// Array of barriers (we keep them constructed all the time since constructing a semaphore/mutex is not cheap)
	uint					mMaxBarriers = 0;								///< Max amount of barriers
	BarrierImpl *			mBarriers = nullptr;							///< List of the actual barriers
};

JPH_NAMESPACE_END
