/**************************************************************************/
/*  rw_lock.h                                                             */
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

#ifndef RW_LOCK_H
#define RW_LOCK_H

#include "core/error/error_macros.h"
#include "core/templates/vector.h"
#include "core/typedefs.h"
#include "mutex.h"
#include "thread.h"

#ifdef MINGW_ENABLED
#define MINGW_STDTHREAD_REDUNDANCY_WARNING
#include "thirdparty/mingw-std-threads/mingw.condition_variable.h"
#include "thirdparty/mingw-std-threads/mingw.mutex.h"
#include "thirdparty/mingw-std-threads/mingw.shared_mutex.h"
#define THREADING_NAMESPACE mingw_stdthread
#else
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#define THREADING_NAMESPACE std
#endif

class RWLock {
	mutable THREADING_NAMESPACE::shared_timed_mutex mutex;

public:
	// Lock the RWLock, block if locked by someone else.
	_ALWAYS_INLINE_ void read_lock() const {
		mutex.lock_shared();
	}

	// Unlock the RWLock, let other threads continue.
	_ALWAYS_INLINE_ void read_unlock() const {
		mutex.unlock_shared();
	}

	// Attempt to lock the RWLock for reading. True on success, false means it can't lock.
	_ALWAYS_INLINE_ bool read_try_lock() const {
		return mutex.try_lock_shared();
	}

	// Lock the RWLock, block if locked by someone else.
	_ALWAYS_INLINE_ void write_lock() {
		mutex.lock();
	}

	// Unlock the RWLock, let other threads continue.
	_ALWAYS_INLINE_ void write_unlock() {
		mutex.unlock();
	}

	// Attempt to lock the RWLock for writing. True on success, false means it can't lock.
	_ALWAYS_INLINE_ bool write_try_lock() {
		return mutex.try_lock();
	}
};

class RWLockRead {
	const RWLock &lock;

public:
	_ALWAYS_INLINE_ RWLockRead(const RWLock &p_lock) :
			lock(p_lock) {
		lock.read_lock();
	}
	_ALWAYS_INLINE_ ~RWLockRead() {
		lock.read_unlock();
	}
};

class RWLockWrite {
	RWLock &lock;

public:
	_ALWAYS_INLINE_ RWLockWrite(RWLock &p_lock) :
			lock(p_lock) {
		lock.write_lock();
	}
	_ALWAYS_INLINE_ ~RWLockWrite() {
		lock.write_unlock();
	}
};

#ifdef THREADS_ENABLED

class RecursiveRWLock {
	mutable THREADING_NAMESPACE::condition_variable cv;
	mutable THREADING_NAMESPACE::mutex cv_mtx;

	//These two are protected by mtx
	mutable Vector<Thread::ID> thread_ids;
	mutable Thread::ID waiting_thread_id;
	mutable BinaryMutex mtx;

	mutable Mutex exclusive_mutex;

	void lock_exclusive_mutex() const {
		if (!exclusive_mutex.try_lock()) { // The exclusive mutex lock is held by writing.
			int read_lock_times;
			{ // On fail, release all read locks and relock them to prevent deadlocks.
				mtx.lock();
				read_lock_times = thread_ids.count(waiting_thread_id);
				for (int i = 0; i < read_lock_times; i++) {
					thread_ids.erase(Thread::get_caller_id());
				}
				if (thread_ids.size() == thread_ids.count(waiting_thread_id)) {
					std::lock_guard lock(cv_mtx);
					cv.notify_all();
				}
				mtx.unlock();
			}

			exclusive_mutex.lock();
			mtx.lock();
			for (int i = 0; i < read_lock_times; i++) {
				thread_ids.push_back(Thread::get_caller_id());
			}
			mtx.unlock();
		}
	}

public:
	// Lock the RecursiveRWLock, block if locked by someone else.
	_ALWAYS_INLINE_ void read_lock() const {
		lock_exclusive_mutex();
		mtx.lock();
		exclusive_mutex.unlock();

		thread_ids.append(Thread::get_caller_id());
		mtx.unlock();
	}

	// Unlock the RecursiveRWLock, let other threads continue.
	_ALWAYS_INLINE_ void read_unlock() const {
		mtx.lock();
		Vector<Thread::ID>::Size index = thread_ids.rfind(Thread::get_caller_id());
		if (index != -1) {
			thread_ids.remove_at(index);
		} else {
			ERR_PRINT("Attempt to read_unlock RecursiveRWLock while the thread hasn't locked it!");
		}
		if (thread_ids.size() == thread_ids.count(waiting_thread_id)) {
			std::lock_guard lock(cv_mtx);
			cv.notify_all();
		}
		mtx.unlock();
	}

	// Attempt to lock the RecursiveRWLock for reading. True on success, false means it can't lock.
	_ALWAYS_INLINE_ bool read_try_lock() const {
		if (exclusive_mutex.try_lock()) {
			mtx.lock();
			exclusive_mutex.unlock();

			thread_ids.append(Thread::get_caller_id());
			mtx.unlock();

			return true;
		}
		return false;
	}

	// Lock the RecursiveRWLock, block if locked by someone else.
	_ALWAYS_INLINE_ void write_lock() {
		lock_exclusive_mutex();
		mtx.lock();

		waiting_thread_id = Thread::get_caller_id();

		while (thread_ids.size() > thread_ids.count(Thread::get_caller_id())) {
			std::unique_lock lock(cv_mtx);

			mtx.unlock();
			cv.wait(lock);
			mtx.lock();
		}
		mtx.unlock();
	}

	// Unlock the RecursiveRWLock, let other threads continue.
	_ALWAYS_INLINE_ void write_unlock() {
		exclusive_mutex.unlock();
	}

	// Attempt to lock the RecursiveRWLock for writing. True on success, false means it can't lock.
	_ALWAYS_INLINE_ bool write_try_lock() {
		if (exclusive_mutex.try_lock()) {
			mtx.lock();

			if (thread_ids.size() != thread_ids.count(Thread::get_caller_id())) {
				mtx.unlock();
				exclusive_mutex.unlock();
				return false;
			}

			mtx.unlock();
			return true;
		}
		return false;
	}
};
#else // No threads.

class RecursiveRWLock {
public:
	// Lock the RecursiveRWLock, block if locked by someone else.
	_ALWAYS_INLINE_ void read_lock() const {}

	// Unlock the RecursiveRWLock, let other threads continue.
	_ALWAYS_INLINE_ void read_unlock() const {}

	// Attempt to lock the RecursiveRWLock for reading. True on success, false means it can't lock.
	_ALWAYS_INLINE_ bool read_try_lock() const { return true; }

	// Lock the RecursiveRWLock, block if locked by someone else.
	_ALWAYS_INLINE_ void write_lock() {}

	// Unlock the RecursiveRWLock, let other threads continue.
	_ALWAYS_INLINE_ void write_unlock() {}

	// Attempt to lock the RecursiveRWLock for writing. True on success, false means it can't lock.
	_ALWAYS_INLINE_ bool write_try_lock() { return true; }
};
#endif

#endif // RW_LOCK_H
