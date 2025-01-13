// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2023 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <atomic>
#include <mutex>
#include <condition_variable>
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

// Things we're using from STL
using std::atomic;
using std::mutex;
using std::condition_variable;

/// Implements a semaphore
/// When we switch to C++20 we can use counting_semaphore to unify this
class JPH_EXPORT Semaphore
{
public:
	/// Constructor
						Semaphore();
						~Semaphore();

	/// Release the semaphore, signaling the thread waiting on the barrier that there may be work
	void				Release(uint inNumber = 1);

	/// Acquire the semaphore inNumber times
	void				Acquire(uint inNumber = 1);

	/// Get the current value of the semaphore
	inline int			GetValue() const								{ return mCount.load(std::memory_order_relaxed); }

private:
#ifdef JPH_PLATFORM_WINDOWS
	// On windows we use a semaphore object since it is more efficient than a lock and a condition variable
	alignas(JPH_CACHE_LINE_SIZE) atomic<int> mCount { 0 };				///< We increment mCount for every release, to acquire we decrement the count. If the count is negative we know that we are waiting on the actual semaphore.
	void *				mSemaphore;										///< The semaphore is an expensive construct so we only acquire/release it if we know that we need to wait/have waiting threads
#else
	// Other platforms: Emulate a semaphore using a mutex, condition variable and count
	mutex				mLock;
	condition_variable	mWaitVariable;
	atomic<int>			mCount { 0 };
#endif
};

JPH_NAMESPACE_END
