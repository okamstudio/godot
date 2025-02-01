/**************************************************************************/
/*  safe_binary_mutex.h                                                   */
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

#include "core/error/error_macros.h"
#include "core/os/mutex.h"
#include "core/typedefs.h"

#ifdef THREADS_ENABLED

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif

// A very special kind of mutex, used in scenarios where these
// requirements hold at the same time:
// - Must be used with a condition variable (only binary mutexes are suitable).
// - Must have recursive semnantics (or simulate, as this one does).
// The implementation keeps the lock count in TS. Therefore, only
// one object of each version of the template can exists; hence the Tag argument.
// Tags must be unique across the Godot codebase.
// Also, don't forget to declare the thread_local variable on each use.
template <int Tag>
class SafeBinaryMutex {
	friend class MutexLock<SafeBinaryMutex<Tag>>;

	using StdMutexType = THREADING_NAMESPACE::mutex;

	mutable THREADING_NAMESPACE::mutex mutex;

	struct TLSData {
		mutable THREADING_NAMESPACE::unique_lock<THREADING_NAMESPACE::mutex> lock;
		uint32_t count = 0;

		TLSData(SafeBinaryMutex<Tag> &p_mutex) :
				lock(p_mutex.mutex, THREADING_NAMESPACE::defer_lock) {}
	};
	static thread_local TLSData tls_data;

public:
	_ALWAYS_INLINE_ void lock() const {
		if (++tls_data.count == 1) {
			tls_data.lock.lock();
		}
	}

	_ALWAYS_INLINE_ void unlock() const {
		DEV_ASSERT(tls_data.count);
		if (--tls_data.count == 0) {
			tls_data.lock.unlock();
		}
	}

	_ALWAYS_INLINE_ THREADING_NAMESPACE::unique_lock<THREADING_NAMESPACE::mutex> &_get_lock() const {
		return const_cast<THREADING_NAMESPACE::unique_lock<THREADING_NAMESPACE::mutex> &>(tls_data.lock);
	}

	_ALWAYS_INLINE_ SafeBinaryMutex() {
	}

	_ALWAYS_INLINE_ ~SafeBinaryMutex() {
		DEV_ASSERT(!tls_data.count);
	}
};

template <int Tag>
class MutexLock<SafeBinaryMutex<Tag>> {
	friend class ConditionVariable;

	const SafeBinaryMutex<Tag> &mutex;

public:
	explicit MutexLock(const SafeBinaryMutex<Tag> &p_mutex) :
			mutex(p_mutex) {
		mutex.lock();
	}

	~MutexLock() {
		mutex.unlock();
	}

	_ALWAYS_INLINE_ void temp_relock() const {
		mutex.lock();
	}

	_ALWAYS_INLINE_ void temp_unlock() const {
		mutex.unlock();
	}

	// TODO: Implement a `try_temp_relock` if needed (will also need a dummy method below).
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else // No threads.

template <int Tag>
class SafeBinaryMutex {
	struct TLSData {
		TLSData(SafeBinaryMutex<Tag> &p_mutex) {}
	};
	static thread_local TLSData tls_data;

public:
	void lock() const {}
	void unlock() const {}
};

template <int Tag>
class MutexLock<SafeBinaryMutex<Tag>> {
public:
	MutexLock(const SafeBinaryMutex<Tag> &p_mutex) {}
	~MutexLock() {}

	void temp_relock() const {}
	void temp_unlock() const {}
};

#endif // THREADS_ENABLED
