/**************************************************************************/
/*  mutex.h                                                               */
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

#include "core/typedefs.h"

#ifdef MINGW_ENABLED
#define MINGW_STDTHREAD_REDUNDANCY_WARNING
#include "thirdparty/mingw-std-threads/mingw.mutex.h"
#define THREADING_NAMESPACE mingw_stdthread
#else
#include <mutex>
#define THREADING_NAMESPACE std
#endif

#ifdef THREADS_ENABLED

template <typename MutexT>
class MutexLock;

template <typename StdMutexT>
class MutexImpl {
	friend class MutexLock<MutexImpl<StdMutexT>>;

	using StdMutexType = StdMutexT;

	mutable StdMutexT mutex;

public:
	_ALWAYS_INLINE_ void lock() const {
		mutex.lock();
	}

	_ALWAYS_INLINE_ void unlock() const {
		mutex.unlock();
	}

	_ALWAYS_INLINE_ bool try_lock() const {
		return mutex.try_lock();
	}
};

template <typename MutexT>
class MutexLock {
	mutable THREADING_NAMESPACE::unique_lock<typename MutexT::StdMutexType> lock;

public:
	explicit MutexLock(const MutexT &p_mutex) :
			lock(p_mutex.mutex) {}

	// Clarification: all the funny syntax is needed so this function exists only for binary mutexes.
	template <typename T = MutexT>
	_ALWAYS_INLINE_ THREADING_NAMESPACE::unique_lock<THREADING_NAMESPACE::mutex> &_get_lock(
			typename std::enable_if<std::is_same<T, THREADING_NAMESPACE::mutex>::value> * = nullptr) const {
		return lock;
	}

	_ALWAYS_INLINE_ void temp_relock() const {
		lock.lock();
	}

	_ALWAYS_INLINE_ void temp_unlock() const {
		lock.unlock();
	}

	// TODO: Implement a `try_temp_relock` if needed (will also need a dummy method below).
};

using Mutex = MutexImpl<THREADING_NAMESPACE::recursive_mutex>; // Recursive, for general use
using BinaryMutex = MutexImpl<THREADING_NAMESPACE::mutex>; // Non-recursive, handle with care

extern template class MutexImpl<THREADING_NAMESPACE::recursive_mutex>;
extern template class MutexImpl<THREADING_NAMESPACE::mutex>;
extern template class MutexLock<MutexImpl<THREADING_NAMESPACE::recursive_mutex>>;
extern template class MutexLock<MutexImpl<THREADING_NAMESPACE::mutex>>;

#else // No threads.

class MutexImpl {
	mutable THREADING_NAMESPACE::mutex mutex;

public:
	void lock() const {}
	void unlock() const {}
	bool try_lock() const { return true; }
};

template <typename MutexT>
class MutexLock {
public:
	MutexLock(const MutexT &p_mutex) {}

	void temp_relock() const {}
	void temp_unlock() const {}
};

using Mutex = MutexImpl;
using BinaryMutex = MutexImpl;

#endif // THREADS_ENABLED
