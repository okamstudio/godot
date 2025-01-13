// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/Reference.h>
#include <Jolt/Core/Color.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/NonCopyable.h>
#include <Jolt/Core/StaticArray.h>
#include <Jolt/Core/Atomics.h>

JPH_NAMESPACE_BEGIN

/// A class that allows units of work (Jobs) to be scheduled across multiple threads.
/// It allows dependencies between the jobs so that the jobs form a graph.
///
/// The pattern for using this class is:
///
///		// Create job system
///		JobSystem *job_system = new JobSystemThreadPool(...);
///
///		// Create some jobs
///		JobHandle second_job = job_system->CreateJob("SecondJob", Color::sRed, []() { ... }, 1); // Create a job with 1 dependency
///		JobHandle first_job = job_system->CreateJob("FirstJob", Color::sGreen, [second_job]() { ....; second_job.RemoveDependency(); }, 0); // Job can start immediately, will start second job when it's done
///		JobHandle third_job = job_system->CreateJob("ThirdJob", Color::sBlue, []() { ... }, 0); // This job can run immediately as well and can run in parallel to job 1 and 2
///
///		// Add the jobs to the barrier so that we can execute them while we're waiting
///		Barrier *barrier = job_system->CreateBarrier();
///		barrier->AddJob(first_job);
///		barrier->AddJob(second_job);
///		barrier->AddJob(third_job);
///		job_system->WaitForJobs(barrier);
///
///		// Clean up
///		job_system->DestroyBarrier(barrier);
///		delete job_system;
///
///	Jobs are guaranteed to be started in the order that their dependency counter becomes zero (in case they're scheduled on a background thread)
///	or in the order they're added to the barrier (when dependency count is zero and when executing on the thread that calls WaitForJobs).
///
/// If you want to implement your own job system, inherit from JobSystem and implement:
///
/// * JobSystem::GetMaxConcurrency - This should return the maximum number of jobs that can run in parallel.
/// * JobSystem::CreateJob - This should create a Job object and return it to the caller.
/// * JobSystem::FreeJob - This should free the memory associated with the job object. It is called by the Job destructor when it is Release()-ed for the last time.
/// * JobSystem::QueueJob/QueueJobs - These should store the job pointer in an internal queue to run immediately (dependencies are tracked internally, this function is called when the job can run).
/// The Job objects are reference counted and are guaranteed to stay alive during the QueueJob(s) call. If you store the job in your own data structure you need to call AddRef() to take a reference.
/// After the job has been executed you need to call Release() to release the reference. Make sure you no longer dereference the job pointer after calling Release().
///
/// JobSystem::Barrier is used to track the completion of a set of jobs. Jobs will be created by other jobs and added to the barrier while it is being waited on. This means that you cannot
/// create a dependency graph beforehand as the graph changes while jobs are running. Implement the following functions:
///
/// * Barrier::AddJob/AddJobs - Add a job to the barrier, any call to WaitForJobs will now also wait for this job to complete.
/// If you store the job in a data structure in the Barrier you need to call AddRef() on the job to keep it alive and Release() after you're done with it.
/// * Barrier::OnJobFinished - This function is called when a job has finished executing, you can use this to track completion and remove the job from the list of jobs to wait on.
///
/// The functions on JobSystem that need to be implemented to support barriers are:
///
/// * JobSystem::CreateBarrier - Create a new barrier.
/// * JobSystem::DestroyBarrier - Destroy a barrier.
/// * JobSystem::WaitForJobs - This is the main function that is used to wait for all jobs that have been added to a Barrier. WaitForJobs can execute jobs that have
/// been added to the barrier while waiting. It is not wise to execute other jobs that touch physics structures as this can cause race conditions and deadlocks. Please keep in mind that the barrier is
/// only intended to wait on the completion of the Jolt jobs added to it, if you scheduled any jobs in your engine's job system to execute the Jolt jobs as part of QueueJob/QueueJobs, you might still need
/// to wait for these in this function after the barrier is finished waiting.
///
/// An example implementation is JobSystemThreadPool. If you don't want to write the Barrier class you can also inherit from JobSystemWithBarrier.
class JPH_EXPORT JobSystem : public NonCopyable
{
protected:
	class Job;

public:
	JPH_OVERRIDE_NEW_DELETE

	/// A job handle contains a reference to a job. The job will be deleted as soon as there are no JobHandles.
	/// referring to the job and when it is not in the job queue / being processed.
	class JobHandle : private Ref<Job>
	{
	public:
		/// Constructor
		inline				JobHandle()									= default;
		inline				JobHandle(const JobHandle &inHandle)		= default;
		inline				JobHandle(JobHandle &&inHandle) noexcept	: Ref<Job>(std::move(inHandle)) { }

		/// Constructor, only to be used by JobSystem
		inline explicit		JobHandle(Job *inJob)						: Ref<Job>(inJob) { }

		/// Assignment
		inline JobHandle &	operator = (const JobHandle &inHandle)		= default;
		inline JobHandle &	operator = (JobHandle &&inHandle) noexcept	= default;

		/// Check if this handle contains a job
		inline bool			IsValid() const								{ return GetPtr() != nullptr; }

		/// Check if this job has finished executing
		inline bool			IsDone() const								{ return GetPtr() != nullptr && GetPtr()->IsDone(); }

		/// Add to the dependency counter.
		inline void			AddDependency(int inCount = 1) const		{ GetPtr()->AddDependency(inCount); }

		/// Remove from the dependency counter. Job will start whenever the dependency counter reaches zero
		/// and if it does it is no longer valid to call the AddDependency/RemoveDependency functions.
		inline void			RemoveDependency(int inCount = 1) const		{ GetPtr()->RemoveDependencyAndQueue(inCount); }

		/// Remove a dependency from a batch of jobs at once, this can be more efficient than removing them one by one as it requires less locking
		static inline void	sRemoveDependencies(const JobHandle *inHandles, uint inNumHandles, int inCount = 1);

		/// Helper function to remove dependencies on a static array of job handles
		template <uint N>
		static inline void	sRemoveDependencies(StaticArray<JobHandle, N> &inHandles, int inCount = 1)
		{
			sRemoveDependencies(inHandles.data(), inHandles.size(), inCount);
		}

		/// Inherit the GetPtr function, only to be used by the JobSystem
		using Ref<Job>::GetPtr;
	};

	/// A job barrier keeps track of a number of jobs and allows waiting until they are all completed.
	class Barrier : public NonCopyable
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Add a job to this barrier
		/// Note that jobs can keep being added to the barrier while waiting for the barrier
		virtual void		AddJob(const JobHandle &inJob) = 0;

		/// Add multiple jobs to this barrier
		/// Note that jobs can keep being added to the barrier while waiting for the barrier
		virtual void		AddJobs(const JobHandle *inHandles, uint inNumHandles) = 0;

	protected:
		/// Job needs to be able to call OnJobFinished
		friend class Job;

		/// Destructor, you should call JobSystem::DestroyBarrier instead of destructing this object directly
		virtual				~Barrier() = default;

		/// Called by a Job to mark that it is finished
		virtual void		OnJobFinished(Job *inJob) = 0;
	};

	/// Main function of the job
	using JobFunction = function<void()>;

	/// Destructor
	virtual					~JobSystem() = default;

	/// Get maximum number of concurrently executing jobs
	virtual int				GetMaxConcurrency() const = 0;

	/// Create a new job, the job is started immediately if inNumDependencies == 0 otherwise it starts when
	/// RemoveDependency causes the dependency counter to reach 0.
	virtual JobHandle		CreateJob(const char *inName, ColorArg inColor, const JobFunction &inJobFunction, uint32 inNumDependencies = 0) = 0;

	/// Create a new barrier, used to wait on jobs
	virtual Barrier *		CreateBarrier() = 0;

	/// Destroy a barrier when it is no longer used. The barrier should be empty at this point.
	virtual void			DestroyBarrier(Barrier *inBarrier) = 0;

	/// Wait for a set of jobs to be finished, note that only 1 thread can be waiting on a barrier at a time
	virtual void			WaitForJobs(Barrier *inBarrier) = 0;

protected:
	/// A class that contains information for a single unit of work
	class Job
	{
	public:
		JPH_OVERRIDE_NEW_DELETE

		/// Constructor
							Job([[maybe_unused]] const char *inJobName, [[maybe_unused]] ColorArg inColor, JobSystem *inJobSystem, const JobFunction &inJobFunction, uint32 inNumDependencies) :
		#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
			mJobName(inJobName),
			mColor(inColor),
		#endif // defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
			mJobSystem(inJobSystem),
			mJobFunction(inJobFunction),
			mNumDependencies(inNumDependencies)
		{
		}

		/// Get the jobs system to which this job belongs
		inline JobSystem *	GetJobSystem()								{ return mJobSystem; }

		/// Add or release a reference to this object
		inline void			AddRef()
		{
			// Adding a reference can use relaxed memory ordering
			mReferenceCount.fetch_add(1, memory_order_relaxed);
		}
		inline void			Release()
		{
		#ifndef JPH_TSAN_ENABLED
			// Releasing a reference must use release semantics...
			if (mReferenceCount.fetch_sub(1, memory_order_release) == 1)
			{
				// ... so that we can use acquire to ensure that we see any updates from other threads that released a ref before freeing the job
				atomic_thread_fence(memory_order_acquire);
				mJobSystem->FreeJob(this);
			}
		#else
			// But under TSAN, we cannot use atomic_thread_fence, so we use an acq_rel operation unconditionally instead
			if (mReferenceCount.fetch_sub(1, memory_order_acq_rel) == 1)
				mJobSystem->FreeJob(this);
		#endif
		}

		/// Add to the dependency counter.
		inline void			AddDependency(int inCount);

		/// Remove from the dependency counter. Returns true whenever the dependency counter reaches zero
		/// and if it does it is no longer valid to call the AddDependency/RemoveDependency functions.
		inline bool			RemoveDependency(int inCount);

		/// Remove from the dependency counter. Job will be queued whenever the dependency counter reaches zero
		/// and if it does it is no longer valid to call the AddDependency/RemoveDependency functions.
		inline void			RemoveDependencyAndQueue(int inCount);

		/// Set the job barrier that this job belongs to and returns false if this was not possible because the job already finished
		inline bool			SetBarrier(Barrier *inBarrier)
		{
			intptr_t barrier = 0;
			if (mBarrier.compare_exchange_strong(barrier, reinterpret_cast<intptr_t>(inBarrier), memory_order_relaxed))
				return true;
			JPH_ASSERT(barrier == cBarrierDoneState, "A job can only belong to 1 barrier");
			return false;
		}

		/// Run the job function, returns the number of dependencies that this job still has or cExecutingState or cDoneState
		inline uint32		Execute()
		{
			// Transition job to executing state
			uint32 state = 0; // We can only start running with a dependency counter of 0
			if (!mNumDependencies.compare_exchange_strong(state, cExecutingState, memory_order_acquire))
				return state; // state is updated by compare_exchange_strong to the current value

			// Run the job function
			{
				JPH_PROFILE(mJobName, mColor.GetUInt32());
				mJobFunction();
			}

			// Fetch the barrier pointer and exchange it for the done state, so we're sure that no barrier gets set after we want to call the callback
			intptr_t barrier = mBarrier.load(memory_order_relaxed);
			for (;;)
			{
				if (mBarrier.compare_exchange_weak(barrier, cBarrierDoneState, memory_order_relaxed))
					break;
			}
			JPH_ASSERT(barrier != cBarrierDoneState);

			// Mark job as done
			state = cExecutingState;
			mNumDependencies.compare_exchange_strong(state, cDoneState, memory_order_relaxed);
			JPH_ASSERT(state == cExecutingState);

			// Notify the barrier after we've changed the job to the done state so that any thread reading the state after receiving the callback will see that the job has finished
			if (barrier != 0)
				reinterpret_cast<Barrier *>(barrier)->OnJobFinished(this);

			return cDoneState;
		}

		/// Test if the job can be executed
		inline bool			CanBeExecuted() const						{ return mNumDependencies.load(memory_order_relaxed) == 0; }

		/// Test if the job finished executing
		inline bool			IsDone() const								{ return mNumDependencies.load(memory_order_relaxed) == cDoneState; }

	#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		/// Get the name of the job
		const char *		GetName() const								{ return mJobName; }
	#endif // defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)

		static constexpr uint32 cExecutingState = 0xe0e0e0e0;			///< Value of mNumDependencies when job is executing
		static constexpr uint32 cDoneState		= 0xd0d0d0d0;			///< Value of mNumDependencies when job is done executing

		static constexpr intptr_t cBarrierDoneState = ~intptr_t(0);		///< Value to use when the barrier has been triggered

private:
	#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		const char *		mJobName;									///< Name of the job
		Color				mColor;										///< Color of the job in the profiler
	#endif // defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		JobSystem *			mJobSystem;									///< The job system we belong to
		atomic<intptr_t>	mBarrier = 0;								///< Barrier that this job is associated with (is a Barrier pointer)
		JobFunction			mJobFunction;								///< Main job function
		atomic<uint32>		mReferenceCount = 0;						///< Amount of JobHandles pointing to this job
		atomic<uint32>		mNumDependencies;							///< Amount of jobs that need to complete before this job can run
	};

	/// Adds a job to the job queue
	virtual void			QueueJob(Job *inJob) = 0;

	/// Adds a number of jobs at once to the job queue
	virtual void			QueueJobs(Job **inJobs, uint inNumJobs) = 0;

	/// Frees a job
	virtual void			FreeJob(Job *inJob) = 0;
};

using JobHandle = JobSystem::JobHandle;

JPH_NAMESPACE_END

#include "JobSystem.inl"
