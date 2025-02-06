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

#pragma once

#include "core/typedefs.h"

#ifdef MINGW_ENABLED
#define MINGW_STDTHREAD_REDUNDANCY_WARNING
#include "thirdparty/mingw-std-threads/mingw.shared_mutex.h"
#define THREADING_NAMESPACE mingw_stdthread
#else
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
