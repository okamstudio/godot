// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Core/Semaphore.h>

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

JPH_NAMESPACE_BEGIN

Semaphore::Semaphore()
{
#ifdef JPH_PLATFORM_WINDOWS
	mSemaphore = CreateSemaphore(nullptr, 0, INT_MAX, nullptr);
#endif
}

Semaphore::~Semaphore()
{
#ifdef JPH_PLATFORM_WINDOWS
	CloseHandle(mSemaphore);
#endif
}

void Semaphore::Release(uint inNumber)
{
	JPH_ASSERT(inNumber > 0);

#ifdef JPH_PLATFORM_WINDOWS
	int old_value = mCount.fetch_add(inNumber);
	if (old_value < 0)
	{
		int new_value = old_value + (int)inNumber;
		int num_to_release = min(new_value, 0) - old_value;
		::ReleaseSemaphore(mSemaphore, num_to_release, nullptr);
	}
#else
	std::lock_guard lock(mLock);
	mCount.fetch_add(inNumber, std::memory_order_relaxed);
	if (inNumber > 1)
		mWaitVariable.notify_all();
	else
		mWaitVariable.notify_one();
#endif
}

void Semaphore::Acquire(uint inNumber)
{
	JPH_ASSERT(inNumber > 0);

#ifdef JPH_PLATFORM_WINDOWS
	int old_value = mCount.fetch_sub(inNumber);
	int new_value = old_value - (int)inNumber;
	if (new_value < 0)
	{
		int num_to_acquire = min(old_value, 0) - new_value;
		for (int i = 0; i < num_to_acquire; ++i)
			WaitForSingleObject(mSemaphore, INFINITE);
	}
#else
	std::unique_lock lock(mLock);
	mCount.fetch_sub(inNumber, std::memory_order_relaxed);
	mWaitVariable.wait(lock, [this]() { return mCount >= 0; });
#endif
}

JPH_NAMESPACE_END
