/**************************************************************************/
/*  rw_lock.cpp                                                           */
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

#include "rw_lock.h"

#include "core/os/memory.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/typedefs.h"

int RWLock::threads_number = -1;

struct RWLock::ThreadMutex {
	uint8_t _offset[64];
	BinaryMutex mtx;
};

int RWLock::get_thread_pos() {
	return Thread::get_caller_id() % threads_number;
}

void RWLock::init() const {
	if (unlikely(threads_number == -1)) {
		if (OS::get_singleton() != nullptr) {
			threads_number = OS::get_singleton()->get_processor_count();
		} else {
			threads_number = THREADING_NAMESPACE::thread::hardware_concurrency();
		}

		if (threads_number < 1) {
			threads_number = 1;
		}
	}
	threads_data = (ThreadMutex *)memalloc(sizeof(ThreadMutex) * threads_number);
	for (int i = 0; i < threads_number; i++) {
		memnew_placement(&threads_data[i], ThreadMutex());
	}
}

void RWLock::read_lock() const {
	if (unlikely(threads_data == nullptr)) {
		return;
	}
	threads_data[get_thread_pos()].mtx.lock();
}

void RWLock::read_unlock() const {
	if (unlikely(threads_data == nullptr)) {
		return;
	}

	DEV_ASSERT(threads_data != nullptr);
	threads_data[get_thread_pos()].mtx.unlock();
}

void RWLock::write_lock() {
	if (unlikely(threads_data == nullptr)) {
		return;
	}

	for (int i = 0; i < threads_number; i++) {
		threads_data[i].mtx.lock();
	}
}

void RWLock::write_unlock() {
	if (unlikely(threads_data == nullptr)) {
		return;
	}

	DEV_ASSERT(threads_data != nullptr);
	for (int i = 0; i < threads_number; i++) {
		threads_data[i].mtx.unlock();
	}
}

RWLock::RWLock() {
	if (threads_data == nullptr) {
		init();
	}
}

RWLock::~RWLock() {
	if (threads_data != nullptr) {
		for (int i = 0; i < threads_number; i++) {
			threads_data[i].~ThreadMutex();
		}
		memfree(threads_data);
		threads_data = nullptr;
	}
}
