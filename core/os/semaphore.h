/**************************************************************************/
/*  semaphore.h                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include <cstdint>

#ifdef THREADS_ENABLED

#include "core/error/error_list.h"
#include "core/typedefs.h"
#ifdef DEBUG_ENABLED
#include "core/error/error_macros.h"
#endif

#ifdef MINGW_ENABLED
#define MINGW_STDTHREAD_REDUNDANCY_WARNING
#include "thirdparty/mingw-std-threads/mingw.condition_variable.h"
#include "thirdparty/mingw-std-threads/mingw.mutex.h"
#define THREADING_NAMESPACE mingw_stdthread
#else
#include <condition_variable>
#include <mutex>
#define THREADING_NAMESPACE std
#endif

class Semaphore {
private:
	mutable THREADING_NAMESPACE::mutex mutex;
	mutable THREADING_NAMESPACE::condition_variable condition;
	mutable uint32_t count = 0; // Initialized as locked.
#ifdef DEBUG_ENABLED
	mutable uint32_t awaiters = 0;
#endif

public:
	_ALWAYS_INLINE_ void post(uint32_t p_count = 1) const {
		std::lock_guard lock(mutex);
		count += p_count;
		for (uint32_t i = 0; i < p_count; ++i) {
			condition.notify_one();
		}
	}

	_ALWAYS_INLINE_ void wait() const {
		THREADING_NAMESPACE::unique_lock lock(mutex);
#ifdef DEBUG_ENABLED
		++awaiters;
#endif
		while (!count) { // Handle spurious wake-ups.
			condition.wait(lock);
		}
		--count;
#ifdef DEBUG_ENABLED
		--awaiters;
#endif
	}

	_ALWAYS_INLINE_ bool try_wait() const {
		std::lock_guard lock(mutex);
		if (count) {
			count--;
			return true;
		} else {
			return false;
		}
	}

#ifdef DEBUG_ENABLED
	~Semaphore() {
		// Destroying an std::condition_variable when not all threads waiting on it have been notified
		// invokes undefined behavior (e.g., it may be nicely destroyed or it may be awaited forever.)
		// That means other threads could still be running the body of std::condition_variable::wait()
		// but already past the safety checkpoint. That's the case for instance if that function is already
		// waiting to lock again.
		//
		// We will make the rule a bit more restrictive and simpler to understand at the same time: there
		// should not be any threads at any stage of the waiting by the time the semaphore is destroyed.
		//
		// We do so because of the following reasons:
		// - We have the guideline that threads must be awaited (i.e., completed), so the waiting thread
		//   must be completely done by the time the thread controlling it finally destroys the semaphore.
		//   Therefore, only a coding mistake could make the program run into such a attempt at premature
		//   destruction of the semaphore.
		// - In scripting, given that Semaphores are wrapped by RefCounted classes, in general it can't
		//   happen that a thread is trying to destroy a Semaphore while another is still doing whatever with
		//   it, so the simplification is mostly transparent to script writers.
		// - The redefined rule can be checked for failure to meet it, which is what this implementation does.
		//   This is useful to detect a few cases of potential misuse; namely:
		//   a) In scripting:
		//      * The coder is naughtily dealing with the reference count causing a semaphore to die prematurely.
		//      * The coder is letting the project reach its termination without having cleanly finished threads
		//        that await on semaphores (or at least, let the usual semaphore-controlled loop exit).
		//   b) In the native side, where Semaphore is not a ref-counted beast and certain coding mistakes can
		//      lead to its premature destruction as well.
		//
		// Let's let users know they are doing it wrong, but apply a, somewhat hacky, countermeasure against UB
		// in debug builds.
		std::lock_guard lock(mutex);
		if (awaiters) {
			WARN_PRINT(
					"A Semaphore object is being destroyed while one or more threads are still waiting on it.\n"
					"Please call post() on it as necessary to prevent such a situation and so ensure correct cleanup.");
			// And now, the hacky countermeasure (i.e., leak the condition variable).
			new (&condition) THREADING_NAMESPACE::condition_variable();
		}
	}
#endif
};

#else // No threads.

class Semaphore {
public:
	void post(uint32_t p_count = 1) const {}
	void wait() const {}
	bool try_wait() const {
		return true;
	}
};

#endif // THREADS_ENABLED
