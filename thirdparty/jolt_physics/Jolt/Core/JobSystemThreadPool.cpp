// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Profiler.h>
#include <Jolt/Core/FPException.h>

#ifdef JPH_PLATFORM_WINDOWS
	JPH_SUPPRESS_WARNING_PUSH
	JPH_MSVC_SUPPRESS_WARNING(5039) // winbase.h(13179): warning C5039: 'TpSetCallbackCleanupGroup': pointer or reference to potentially throwing function passed to 'extern "C"' function under -EHc. Undefined behavior may occur if this function throws an exception.
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
#ifndef JPH_COMPILER_MINGW
	#include <Windows.h>
#else
	#include <windows.h>
#endif

	JPH_SUPPRESS_WARNING_POP
#endif
#ifdef JPH_PLATFORM_LINUX
	#include <sys/prctl.h>
#endif

JPH_NAMESPACE_BEGIN

void JobSystemThreadPool::Init(uint inMaxJobs, uint inMaxBarriers, int inNumThreads)
{
	JobSystemWithBarrier::Init(inMaxBarriers);

	// Init freelist of jobs
	mJobs.Init(inMaxJobs, inMaxJobs);

	// Init queue
	for (atomic<Job *> &j : mQueue)
		j = nullptr;

	// Start the worker threads
	StartThreads(inNumThreads);
}

JobSystemThreadPool::JobSystemThreadPool(uint inMaxJobs, uint inMaxBarriers, int inNumThreads)
{
	Init(inMaxJobs, inMaxBarriers, inNumThreads);
}

void JobSystemThreadPool::StartThreads([[maybe_unused]] int inNumThreads)
{
#if !defined(JPH_CPU_WASM) || defined(__EMSCRIPTEN_PTHREADS__) // If we're running without threads support we cannot create threads and we ignore the inNumThreads parameter
	// Auto detect number of threads
	if (inNumThreads < 0)
		inNumThreads = thread::hardware_concurrency() - 1;

	// If no threads are requested we're done
	if (inNumThreads == 0)
		return;

	// Don't quit the threads
	mQuit = false;

	// Allocate heads
	mHeads = reinterpret_cast<atomic<uint> *>(Allocate(sizeof(atomic<uint>) * inNumThreads));
	for (int i = 0; i < inNumThreads; ++i)
		mHeads[i] = 0;

	// Start running threads
	JPH_ASSERT(mThreads.empty());
	mThreads.reserve(inNumThreads);
	for (int i = 0; i < inNumThreads; ++i)
		mThreads.emplace_back([this, i] { ThreadMain(i); });
#endif
}

JobSystemThreadPool::~JobSystemThreadPool()
{
	// Stop all worker threads
	StopThreads();
}

void JobSystemThreadPool::StopThreads()
{
	if (mThreads.empty())
		return;

	// Signal threads that we want to stop and wake them up
	mQuit = true;
	mSemaphore.Release((uint)mThreads.size());

	// Wait for all threads to finish
	for (thread &t : mThreads)
		if (t.joinable())
			t.join();

	// Delete all threads
	mThreads.clear();

	// Ensure that there are no lingering jobs in the queue
	for (uint head = 0; head != mTail; ++head)
	{
		// Fetch job
		Job *job_ptr = mQueue[head & (cQueueLength - 1)].exchange(nullptr);
		if (job_ptr != nullptr)
		{
			// And execute it
			job_ptr->Execute();
			job_ptr->Release();
		}
	}

	// Destroy heads and reset tail
	Free(mHeads);
	mHeads = nullptr;
	mTail = 0;
}

JobHandle JobSystemThreadPool::CreateJob(const char *inJobName, ColorArg inColor, const JobFunction &inJobFunction, uint32 inNumDependencies)
{
	JPH_PROFILE_FUNCTION();

	// Loop until we can get a job from the free list
	uint32 index;
	for (;;)
	{
		index = mJobs.ConstructObject(inJobName, inColor, this, inJobFunction, inNumDependencies);
		if (index != AvailableJobs::cInvalidObjectIndex)
			break;
		JPH_ASSERT(false, "No jobs available!");
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
	Job *job = &mJobs.Get(index);

	// Construct handle to keep a reference, the job is queued below and may immediately complete
	JobHandle handle(job);

	// If there are no dependencies, queue the job now
	if (inNumDependencies == 0)
		QueueJob(job);

	// Return the handle
	return handle;
}

void JobSystemThreadPool::FreeJob(Job *inJob)
{
	mJobs.DestructObject(inJob);
}

uint JobSystemThreadPool::GetHead() const
{
	// Find the minimal value across all threads
	uint head = mTail;
	for (size_t i = 0; i < mThreads.size(); ++i)
		head = min(head, mHeads[i].load());
	return head;
}

void JobSystemThreadPool::QueueJobInternal(Job *inJob)
{
	// Add reference to job because we're adding the job to the queue
	inJob->AddRef();

	// Need to read head first because otherwise the tail can already have passed the head
	// We read the head outside of the loop since it involves iterating over all threads and we only need to update
	// it if there's not enough space in the queue.
	uint head = GetHead();

	for (;;)
	{
		// Check if there's space in the queue
		uint old_value = mTail;
		if (old_value - head >= cQueueLength)
		{
			// We calculated the head outside of the loop, update head (and we also need to update tail to prevent it from passing head)
			head = GetHead();
			old_value = mTail;

			// Second check if there's space in the queue
			if (old_value - head >= cQueueLength)
			{
				// Wake up all threads in order to ensure that they can clear any nullptrs they may not have processed yet
				mSemaphore.Release((uint)mThreads.size());

				// Sleep a little (we have to wait for other threads to update their head pointer in order for us to be able to continue)
				std::this_thread::sleep_for(std::chrono::microseconds(100));
				continue;
			}
		}

		// Write the job pointer if the slot is empty
		Job *expected_job = nullptr;
		bool success = mQueue[old_value & (cQueueLength - 1)].compare_exchange_strong(expected_job, inJob);

		// Regardless of who wrote the slot, we will update the tail (if the successful thread got scheduled out
		// after writing the pointer we still want to be able to continue)
		mTail.compare_exchange_strong(old_value, old_value + 1);

		// If we successfully added our job we're done
		if (success)
			break;
	}
}

void JobSystemThreadPool::QueueJob(Job *inJob)
{
	JPH_PROFILE_FUNCTION();

	// If we have no worker threads, we can't queue the job either. We assume in this case that the job will be added to a barrier and that the barrier will execute the job when it's Wait() function is called.
	if (mThreads.empty())
		return;

	// Queue the job
	QueueJobInternal(inJob);

	// Wake up thread
	mSemaphore.Release();
}

void JobSystemThreadPool::QueueJobs(Job **inJobs, uint inNumJobs)
{
	JPH_PROFILE_FUNCTION();

	JPH_ASSERT(inNumJobs > 0);

	// If we have no worker threads, we can't queue the job either. We assume in this case that the job will be added to a barrier and that the barrier will execute the job when it's Wait() function is called.
	if (mThreads.empty())
		return;

	// Queue all jobs
	for (Job **job = inJobs, **job_end = inJobs + inNumJobs; job < job_end; ++job)
		QueueJobInternal(*job);

	// Wake up threads
	mSemaphore.Release(min(inNumJobs, (uint)mThreads.size()));
}

#if defined(JPH_PLATFORM_WINDOWS)

#if !defined(JPH_COMPILER_MINGW) // MinGW doesn't support __try/__except)
	// Sets the current thread name in MSVC debugger
	static void RaiseThreadNameException(const char *inName)
	{
		#pragma pack(push, 8)

		struct THREADNAME_INFO
		{
			DWORD	dwType;			// Must be 0x1000.
			LPCSTR	szName;			// Pointer to name (in user addr space).
			DWORD	dwThreadID;		// Thread ID (-1=caller thread).
			DWORD	dwFlags;		// Reserved for future use, must be zero.
		};

		#pragma pack(pop)

		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = inName;
		info.dwThreadID = (DWORD)-1;
		info.dwFlags = 0;

		__try
		{
			RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *)&info);
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}
#endif // !JPH_COMPILER_MINGW

	static void SetThreadName(const char* inName)
	{
		JPH_SUPPRESS_WARNING_PUSH

		// Suppress casting warning, it's fine here as GetProcAddress doesn't really return a FARPROC
		JPH_CLANG_SUPPRESS_WARNING("-Wcast-function-type") // error : cast from 'FARPROC' (aka 'long long (*)()') to 'SetThreadDescriptionFunc' (aka 'long (*)(void *, const wchar_t *)') converts to incompatible function type
		JPH_CLANG_SUPPRESS_WARNING("-Wcast-function-type-strict") // error : cast from 'FARPROC' (aka 'long long (*)()') to 'SetThreadDescriptionFunc' (aka 'long (*)(void *, const wchar_t *)') converts to incompatible function type
		JPH_MSVC_SUPPRESS_WARNING(4191) // reinterpret_cast' : unsafe conversion from 'FARPROC' to 'SetThreadDescriptionFunc'. Calling this function through the result pointer may cause your program to fail

		using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE hThread, PCWSTR lpThreadDescription);
		static SetThreadDescriptionFunc SetThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(GetProcAddress(GetModuleHandleW(L"Kernel32.dll"), "SetThreadDescription"));

		JPH_SUPPRESS_WARNING_POP

		if (SetThreadDescription)
		{
			wchar_t name_buffer[64] = { 0 };
			if (MultiByteToWideChar(CP_UTF8, 0, inName, -1, name_buffer, sizeof(name_buffer) / sizeof(wchar_t) - 1) == 0)
				return;

			SetThreadDescription(GetCurrentThread(), name_buffer);
		}
#if !defined(JPH_COMPILER_MINGW)
		else if (IsDebuggerPresent())
			RaiseThreadNameException(inName);
#endif // !JPH_COMPILER_MINGW
	}
#elif defined(JPH_PLATFORM_LINUX)
	static void SetThreadName(const char *inName)
	{
		JPH_ASSERT(strlen(inName) < 16); // String will be truncated if it is longer
		prctl(PR_SET_NAME, inName, 0, 0, 0);
	}
#endif // JPH_PLATFORM_LINUX

void JobSystemThreadPool::ThreadMain(int inThreadIndex)
{
	// Name the thread
	char name[64];
	snprintf(name, sizeof(name), "Worker %d", int(inThreadIndex + 1));

#if defined(JPH_PLATFORM_WINDOWS) || defined(JPH_PLATFORM_LINUX)
	SetThreadName(name);
#endif // JPH_PLATFORM_WINDOWS && !JPH_COMPILER_MINGW

	// Enable floating point exceptions
	FPExceptionsEnable enable_exceptions;
	JPH_UNUSED(enable_exceptions);

	JPH_PROFILE_THREAD_START(name);

	// Call the thread init function
	mThreadInitFunction(inThreadIndex);

	atomic<uint> &head = mHeads[inThreadIndex];

	while (!mQuit)
	{
		// Wait for jobs
		mSemaphore.Acquire();

		{
			JPH_PROFILE("Executing Jobs");

			// Loop over the queue
			while (head != mTail)
			{
				// Exchange any job pointer we find with a nullptr
				atomic<Job *> &job = mQueue[head & (cQueueLength - 1)];
				if (job.load() != nullptr)
				{
					Job *job_ptr = job.exchange(nullptr);
					if (job_ptr != nullptr)
					{
						// And execute it
						job_ptr->Execute();
						job_ptr->Release();
					}
				}
				head++;
			}
		}
	}

	// Call the thread exit function
	mThreadExitFunction(inThreadIndex);

	JPH_PROFILE_THREAD_END();
}

JPH_NAMESPACE_END
