/**************************************************************************/
/*  static_block_allocator.cpp                                            */
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

#include "static_block_allocator.h"

#include "core/os/block_allocator.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/oa_hash_map.h"

typedef OAHashMap<int64_t, int64_t> SizeToIdMap;

struct alignas(Thread::CACHE_LINE_BYTES) ThreadData {
	LocalVector<BlockAllocator> allocs;
	BinaryMutex mtx;
	SizeToIdMap size_to_id = SizeToIdMap(32);
};

uint32_t total_num_allocators;
int physics_threads_count;
ThreadData *threads_data = nullptr;
bool is_set_num_threads_by_OS = false;

int32_t StaticBlockAllocator::_get_thread_id() {
	return (Thread::get_caller_id() - Thread::MAIN_ID) % physics_threads_count;
}

int64_t StaticBlockAllocator::_create_id(int32_t p_size_pos, int32_t p_thread_id) {
	union {
		int64_t id;
		struct {
			int32_t pos;
			int32_t thread_id;
		};
	} id_data;
	id_data.pos = p_size_pos;
	id_data.thread_id = p_thread_id;
	return id_data.id;
}

void StaticBlockAllocator::_get_from_id(int64_t p_id, int32_t &r_size_pos, int32_t &r_thread_id) {
	union {
		int64_t id;
		struct {
			int32_t pos;
			int32_t thread_id;
		};
	} id_data;
	id_data.id = p_id;
	r_size_pos = id_data.pos;
	r_thread_id = id_data.thread_id;
}

void StaticBlockAllocator::_init() {
	physics_threads_count = 1;
	threads_data = (ThreadData *)Memory::alloc_aligned_static(sizeof(ThreadData), Thread::CACHE_LINE_BYTES);
	memnew_placement(threads_data, ThreadData);
}

int64_t StaticBlockAllocator::_get_allocator_id_for_size(size_t p_size) {
	if (unlikely(!is_set_num_threads_by_OS)) {
		if (threads_data == nullptr) {
			_init();
		}
		if (OS::get_singleton() != nullptr) {
			physics_threads_count = OS::get_singleton()->get_processor_count();
			if (physics_threads_count > 1) {
				threads_data = (ThreadData *)Memory::realloc_aligned_static(threads_data, physics_threads_count * sizeof(ThreadData), sizeof(ThreadData), Thread::CACHE_LINE_BYTES);
				for (int i = 1; i < physics_threads_count; i++) {
					memnew_placement(&threads_data[i], ThreadData);
				}
			}
			is_set_num_threads_by_OS = true;
		}
	}
	int32_t thread_id = _get_thread_id();
	ThreadData &t = threads_data[thread_id];
	t.mtx.lock();
	BlockAllocator *ptr = t.allocs.ptr();
	int64_t pos = -1;
	t.size_to_id.lookup(p_size, pos);

	if (unlikely(pos == -1)) {
		((SafeNumeric<uint32_t> *)(&total_num_allocators))->increment();
		pos = t.allocs.size();
		t.allocs.push_back(BlockAllocator());
		t.allocs[pos].init(p_size, 8);
		t.size_to_id.insert(p_size, pos);
	} else if (!ptr[pos].is_initialized()) {
		((SafeNumeric<uint32_t> *)(&total_num_allocators))->increment();
		ptr[pos].init(p_size, 1);
	}
	int64_t id = _create_id(pos, thread_id);
	t.mtx.unlock();
	return id;
}

int64_t StaticBlockAllocator::get_allocator_id_for_size(size_t p_size) {
	uint64_t pos = _get_allocator_id_for_size(p_size);
	return pos;
}

void *StaticBlockAllocator::allocate_by_id(int64_t p_id) {
	int32_t pos, thread_id;
	_get_from_id(p_id, pos, thread_id);
	ThreadData &t = threads_data[thread_id];
	BlockAllocator &b_allocator = t.allocs.ptr()[pos];
	t.mtx.lock();
	if (unlikely(!b_allocator.is_initialized())) {
		((SafeNumeric<uint32_t> *)(&total_num_allocators))->increment();
		b_allocator.init(b_allocator.get_structure_size(), 1);
	}
	void *ret = b_allocator.alloc();
	t.mtx.unlock();
	return ret;
}

void StaticBlockAllocator::free_by_id(int64_t p_id, void *p_ptr) {
	int32_t pos, thread_id;
	_get_from_id(p_id, pos, thread_id);
	ThreadData &t = threads_data[thread_id];
	t.mtx.lock();
	DEV_ASSERT(!(pos >= (int32_t)t.allocs.size() || !t.allocs.ptr()[pos].is_initialized()));
	BlockAllocator &b_allocator = t.allocs.ptr()[pos];
	b_allocator.free(p_ptr);

	if (likely(b_allocator.get_total_elements() != 0)) {
		t.mtx.unlock();
		return;
	}
	b_allocator.reset();
	if (unlikely(((SafeNumeric<uint32_t> *)(&total_num_allocators))->decrement() == 0)) {
		t.mtx.unlock();
		for (int i = 0; i < physics_threads_count; i++) {
			threads_data[i].~ThreadData();
		}
		Memory::free_aligned_static(threads_data);
		threads_data = nullptr;
		is_set_num_threads_by_OS = false;
		return;
	}
	t.mtx.unlock();
}
