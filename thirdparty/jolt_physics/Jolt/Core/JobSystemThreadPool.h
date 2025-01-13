// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/Semaphore.h>

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <thread>
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

// Things we're using from STL
using std::thread;

/// Implementation of a JobSystem using a thread pool
///
/// Note that this is considered an example implementation. It is expected that when you integrate
/// the physics engine into your own project that you'll provide your own implementation of the
/// JobSystem built on top of whatever job system your project uses.
class JPH_EXPORT JobSystemThreadPool final : public JobSystemWithBarrier
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Creates a thread pool.
	/// @see JobSystemThreadPool::Init
							JobSystemThreadPool(uint inMaxJobs, uint inMaxBarriers, int inNumThreads = -1);
							JobSystemThreadPool() = default;
	virtual					~JobSystemThreadPool() override;

	/// Functions to call when a thread is initialized or exits, must be set before calling Init()
	using InitExitFunction = function<void(int)>;
	void					SetThreadInitFunction(const InitExitFunction &inInitFunction)	{ mThreadInitFunction = inInitFunction; }
	void					SetThreadExitFunction(const InitExitFunction &inExitFunction)	{ mThreadExitFunction = inExitFunction; }

	/// Initialize the thread pool
	/// @param inMaxJobs Max number of jobs that can be allocated at any time
	/// @param inMaxBarriers Max number of barriers that can be allocated at any time
	/// @param inNumThreads Number of threads to start (the number of concurrent jobs is 1 more because the main thread will also run jobs while waiting for a barrier to complete). Use -1 to auto detect the amount of CPU's.
	void					Init(uint inMaxJobs, uint inMaxBarriers, int inNumThreads = -1);

	// See JobSystem
	virtual int				GetMaxConcurrency() const override				{ return int(mThreads.size()) + 1; }
	virtual JobHandle		CreateJob(const char *inName, ColorArg inColor, const JobFunction &inJobFunction, uint32 inNumDependencies = 0) override;

	/// Change the max concurrency after initialization
	void					SetNumThreads(int inNumThreads)					{ StopThreads(); StartThreads(inNumThreads); }

protected:
	// See JobSystem
	virtual void			QueueJob(Job *inJob) override;
	virtual void			QueueJobs(Job **inJobs, uint inNumJobs) override;
	virtual void			FreeJob(Job *inJob) override;

private:
	/// Start/stop the worker threads
	void					StartThreads(int inNumThreads);
	void					StopThreads();

	/// Entry point for a thread
	void					ThreadMain(int inThreadIndex);

	/// Get the head of the thread that has processed the least amount of jobs
	inline uint				GetHead() const;

	/// Internal helper function to queue a job
	inline void				QueueJobInternal(Job *inJob);

	/// Functions to call when initializing or exiting a thread
	InitExitFunction		mThreadInitFunction = [](int) { };
	InitExitFunction		mThreadExitFunction = [](int) { };

	/// Array of jobs (fixed size)
	using AvailableJobs = FixedSizeFreeList<Job>;
	AvailableJobs			mJobs;

	/// Threads running jobs
	Array<thread>			mThreads;

	// The job queue
	static constexpr uint32 cQueueLength = 1024;
	static_assert(IsPowerOf2(cQueueLength));								// We do bit operations and require queue length to be a power of 2
	atomic<Job *>			mQueue[cQueueLength];

	// Head and tail of the queue, do this value modulo cQueueLength - 1 to get the element in the mQueue array
	atomic<uint> *			mHeads = nullptr;								///< Per executing thread the head of the current queue
	alignas(JPH_CACHE_LINE_SIZE) atomic<uint> mTail = 0;					///< Tail (write end) of the queue

	// Semaphore used to signal worker threads that there is new work
	Semaphore				mSemaphore;

	/// Boolean to indicate that we want to stop the job system
	atomic<bool>			mQuit = false;
};

JPH_NAMESPACE_END
