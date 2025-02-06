/**************************************************************************/
/*  rid_owner.h                                                           */
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

#include "core/os/memory.h"
#include "core/os/mutex.h"
#include "core/string/print_string.h"
#include "core/templates/list.h"
#include "core/templates/rid.h"
#include "core/templates/safe_refcount.h"

#include <stdio.h>
#include <typeinfo>

#ifdef SANITIZERS_ENABLED
#ifdef __has_feature
#if __has_feature(thread_sanitizer)
#define TSAN_ENABLED
#endif
#elif defined(__SANITIZE_THREAD__)
#define TSAN_ENABLED
#endif
#endif

#ifdef TSAN_ENABLED
#include <sanitizer/tsan_interface.h>
#endif

// The following macros would need to be implemented somehow
// for purely weakly ordered architectures. There's a test case
// ("[RID_Owner] Thread safety") with potential to catch issues
// on such architectures if these primitives fail to be implemented.
// For now, they will be just markers about needs that may arise.
#define WEAK_MEMORY_ORDER 0
#if WEAK_MEMORY_ORDER
// Ideally, we'd have implementations that collaborate with the
// sync mechanism used (e.g., the mutex) so instead of some full
// memory barriers being issued, some acquire-release on the
// primitive itself. However, these implementations will at least
// provide correctness.
#define SYNC_ACQUIRE std::atomic_thread_fence(std::memory_order_acquire);
#define SYNC_RELEASE std::atomic_thread_fence(std::memory_order_release);
#else
// Compiler barriers are enough in this case.
#define SYNC_ACQUIRE std::atomic_signal_fence(std::memory_order_acquire);
#define SYNC_RELEASE std::atomic_signal_fence(std::memory_order_release);
#endif

class RID_AllocBase {
	static SafeNumeric<uint64_t> base_id;

protected:
	static RID _make_from_id(uint64_t p_id) {
		RID rid;
		rid._id = p_id;
		return rid;
	}

	static RID _gen_rid() {
		return _make_from_id(_gen_id());
	}

	friend struct VariantUtilityFunctions;

	static uint64_t _gen_id() {
		return base_id.increment();
	}

public:
	virtual ~RID_AllocBase() {}
};

template <typename T, bool THREAD_SAFE = false>
class RID_Alloc : public RID_AllocBase {
	struct Chunk {
		T data;
		uint32_t validator;
	};
	Chunk **chunks = nullptr;
	uint32_t **free_list_chunks = nullptr;

	uint32_t elements_in_chunk;
	uint32_t max_alloc = 0;
	uint32_t alloc_count = 0;
	uint32_t chunk_limit = 0;

	const char *description = nullptr;

	mutable Mutex mutex;

	_FORCE_INLINE_ RID _allocate_rid() {
		if constexpr (THREAD_SAFE) {
			mutex.lock();
		}

		if (alloc_count == max_alloc) {
			//allocate a new chunk
			uint32_t chunk_count = alloc_count == 0 ? 0 : (max_alloc / elements_in_chunk);
			if (THREAD_SAFE && chunk_count == chunk_limit) {
				mutex.unlock();
				if (description != nullptr) {
					ERR_FAIL_V_MSG(RID(), vformat("Element limit for RID of type '%s' reached.", String(description)));
				} else {
					ERR_FAIL_V_MSG(RID(), "Element limit reached.");
				}
			}

			//grow chunks
			if constexpr (!THREAD_SAFE) {
				chunks = (Chunk **)memrealloc(chunks, sizeof(Chunk *) * (chunk_count + 1));
			}
			chunks[chunk_count] = (Chunk *)memalloc(sizeof(Chunk) * elements_in_chunk); //but don't initialize
			//grow free lists
			if constexpr (!THREAD_SAFE) {
				free_list_chunks = (uint32_t **)memrealloc(free_list_chunks, sizeof(uint32_t *) * (chunk_count + 1));
			}
			free_list_chunks[chunk_count] = (uint32_t *)memalloc(sizeof(uint32_t) * elements_in_chunk);

			//initialize
			for (uint32_t i = 0; i < elements_in_chunk; i++) {
				// Don't initialize chunk.
				chunks[chunk_count][i].validator = 0xFFFFFFFF;
				free_list_chunks[chunk_count][i] = alloc_count + i;
			}

			if constexpr (THREAD_SAFE) {
				// Store atomically to avoid data race with the load in get_or_null().
				((std::atomic<uint32_t> *)&max_alloc)->store(max_alloc + elements_in_chunk, std::memory_order_relaxed);
			} else {
				max_alloc += elements_in_chunk;
			}
		}

		uint32_t free_index = free_list_chunks[alloc_count / elements_in_chunk][alloc_count % elements_in_chunk];

		uint32_t free_chunk = free_index / elements_in_chunk;
		uint32_t free_element = free_index % elements_in_chunk;

		uint32_t validator = (uint32_t)(_gen_id() & 0x7FFFFFFF);
		CRASH_COND_MSG(validator == 0x7FFFFFFF, "Overflow in RID validator");
		uint64_t id = validator;
		id <<= 32;
		id |= free_index;

		chunks[free_chunk][free_element].validator = validator;
		chunks[free_chunk][free_element].validator |= 0x80000000; //mark uninitialized bit

		alloc_count++;

		if constexpr (THREAD_SAFE) {
			mutex.unlock();
		}

		return _make_from_id(id);
	}

public:
	RID make_rid() {
		RID rid = _allocate_rid();
		initialize_rid(rid);
		return rid;
	}
	RID make_rid(const T &p_value) {
		RID rid = _allocate_rid();
		initialize_rid(rid, p_value);
		return rid;
	}

	//allocate but don't initialize, use initialize_rid afterwards
	RID allocate_rid() {
		return _allocate_rid();
	}

	_FORCE_INLINE_ T *get_or_null(const RID &p_rid, bool p_initialize = false) {
		if (p_rid == RID()) {
			return nullptr;
		}

		if constexpr (THREAD_SAFE) {
			SYNC_ACQUIRE;
		}

		uint64_t id = p_rid.get_id();
		uint32_t idx = uint32_t(id & 0xFFFFFFFF);
		uint32_t ma;
		if constexpr (THREAD_SAFE) { // Read atomically to avoid data race with the store in _allocate_rid().
			ma = ((std::atomic<uint32_t> *)&max_alloc)->load(std::memory_order_relaxed);
		} else {
			ma = max_alloc;
		}
		if (unlikely(idx >= ma)) {
			return nullptr;
		}

		uint32_t idx_chunk = idx / elements_in_chunk;
		uint32_t idx_element = idx % elements_in_chunk;

		uint32_t validator = uint32_t(id >> 32);

		if constexpr (THREAD_SAFE) {
#ifdef TSAN_ENABLED
			__tsan_acquire(&chunks[idx_chunk]); // We know not a race in practice.
			__tsan_acquire(&chunks[idx_chunk][idx_element]); // We know not a race in practice.
#endif
		}

		Chunk &c = chunks[idx_chunk][idx_element];

		if constexpr (THREAD_SAFE) {
#ifdef TSAN_ENABLED
			__tsan_release(&chunks[idx_chunk]);
			__tsan_release(&chunks[idx_chunk][idx_element]);
			__tsan_acquire(&c.validator); // We know not a race in practice.
#endif
		}

		if (unlikely(p_initialize)) {
			if (unlikely(!(c.validator & 0x80000000))) {
				ERR_FAIL_V_MSG(nullptr, "Initializing already initialized RID");
			}

			if (unlikely((c.validator & 0x7FFFFFFF) != validator)) {
				ERR_FAIL_V_MSG(nullptr, "Attempting to initialize the wrong RID");
			}

			c.validator &= 0x7FFFFFFF; //initialized

		} else if (unlikely(c.validator != validator)) {
			if ((c.validator & 0x80000000) && c.validator != 0xFFFFFFFF) {
				ERR_FAIL_V_MSG(nullptr, "Attempting to use an uninitialized RID");
			}
			return nullptr;
		}

		if constexpr (THREAD_SAFE) {
#ifdef TSAN_ENABLED
			__tsan_release(&c.validator);
#endif
		}

		T *ptr = &c.data;

		return ptr;
	}
	void initialize_rid(RID p_rid) {
		T *mem = get_or_null(p_rid, true);
		ERR_FAIL_NULL(mem);

		if constexpr (THREAD_SAFE) {
#ifdef TSAN_ENABLED
			__tsan_acquire(mem); // We know not a race in practice.
#endif
		}

		memnew_placement(mem, T);

		if constexpr (THREAD_SAFE) {
#ifdef TSAN_ENABLED
			__tsan_release(mem);
#endif
			SYNC_RELEASE;
		}
	}

	void initialize_rid(RID p_rid, const T &p_value) {
		T *mem = get_or_null(p_rid, true);
		ERR_FAIL_NULL(mem);

		if constexpr (THREAD_SAFE) {
#ifdef TSAN_ENABLED
			__tsan_acquire(mem); // We know not a race in practice.
#endif
		}

		memnew_placement(mem, T(p_value));

		if constexpr (THREAD_SAFE) {
#ifdef TSAN_ENABLED
			__tsan_release(mem);
#endif
			SYNC_RELEASE;
		}
	}

	_FORCE_INLINE_ bool owns(const RID &p_rid) const {
		if constexpr (THREAD_SAFE) {
			mutex.lock();
		}

		uint64_t id = p_rid.get_id();
		uint32_t idx = uint32_t(id & 0xFFFFFFFF);
		if (unlikely(idx >= max_alloc)) {
			if constexpr (THREAD_SAFE) {
				mutex.unlock();
			}
			return false;
		}

		uint32_t idx_chunk = idx / elements_in_chunk;
		uint32_t idx_element = idx % elements_in_chunk;

		uint32_t validator = uint32_t(id >> 32);

		bool owned = (validator != 0x7FFFFFFF) && (chunks[idx_chunk][idx_element].validator & 0x7FFFFFFF) == validator;

		if constexpr (THREAD_SAFE) {
			mutex.unlock();
		}

		return owned;
	}

	_FORCE_INLINE_ void free(const RID &p_rid) {
		if constexpr (THREAD_SAFE) {
			mutex.lock();
		}

		uint64_t id = p_rid.get_id();
		uint32_t idx = uint32_t(id & 0xFFFFFFFF);
		if (unlikely(idx >= max_alloc)) {
			if constexpr (THREAD_SAFE) {
				mutex.unlock();
			}
			ERR_FAIL();
		}

		uint32_t idx_chunk = idx / elements_in_chunk;
		uint32_t idx_element = idx % elements_in_chunk;

		uint32_t validator = uint32_t(id >> 32);
		if (unlikely(chunks[idx_chunk][idx_element].validator & 0x80000000)) {
			if constexpr (THREAD_SAFE) {
				mutex.unlock();
			}
			ERR_FAIL_MSG("Attempted to free an uninitialized or invalid RID");
		} else if (unlikely(chunks[idx_chunk][idx_element].validator != validator)) {
			if constexpr (THREAD_SAFE) {
				mutex.unlock();
			}
			ERR_FAIL();
		}

		chunks[idx_chunk][idx_element].data.~T();
		chunks[idx_chunk][idx_element].validator = 0xFFFFFFFF; // go invalid

		alloc_count--;
		free_list_chunks[alloc_count / elements_in_chunk][alloc_count % elements_in_chunk] = idx;

		if constexpr (THREAD_SAFE) {
			mutex.unlock();
		}
	}

	_FORCE_INLINE_ uint32_t get_rid_count() const {
		return alloc_count;
	}
	void get_owned_list(List<RID> *p_owned) const {
		if constexpr (THREAD_SAFE) {
			mutex.lock();
		}
		for (size_t i = 0; i < max_alloc; i++) {
			uint64_t validator = chunks[i / elements_in_chunk][i % elements_in_chunk].validator;
			if (validator != 0xFFFFFFFF) {
				p_owned->push_back(_make_from_id((validator << 32) | i));
			}
		}
		if constexpr (THREAD_SAFE) {
			mutex.unlock();
		}
	}

	//used for fast iteration in the elements or RIDs
	void fill_owned_buffer(RID *p_rid_buffer) const {
		if constexpr (THREAD_SAFE) {
			mutex.lock();
		}
		uint32_t idx = 0;
		for (size_t i = 0; i < max_alloc; i++) {
			uint64_t validator = chunks[i / elements_in_chunk][i % elements_in_chunk].validator;
			if (validator != 0xFFFFFFFF) {
				p_rid_buffer[idx] = _make_from_id((validator << 32) | i);
				idx++;
			}
		}

		if constexpr (THREAD_SAFE) {
			mutex.unlock();
		}
	}

	void set_description(const char *p_description) {
		description = p_description;
	}

	RID_Alloc(uint32_t p_target_chunk_byte_size = 65536, uint32_t p_maximum_number_of_elements = 262144) {
		elements_in_chunk = sizeof(T) > p_target_chunk_byte_size ? 1 : (p_target_chunk_byte_size / sizeof(T));
		if constexpr (THREAD_SAFE) {
			chunk_limit = (p_maximum_number_of_elements / elements_in_chunk) + 1;
			chunks = (Chunk **)memalloc(sizeof(Chunk *) * chunk_limit);
			free_list_chunks = (uint32_t **)memalloc(sizeof(uint32_t *) * chunk_limit);
			SYNC_RELEASE;
		}
	}

	~RID_Alloc() {
		if constexpr (THREAD_SAFE) {
			SYNC_ACQUIRE;
		}

		if (alloc_count) {
			print_error(vformat("ERROR: %d RID allocations of type '%s' were leaked at exit.",
					alloc_count, description ? description : typeid(T).name()));

			for (size_t i = 0; i < max_alloc; i++) {
				uint32_t validator = chunks[i / elements_in_chunk][i % elements_in_chunk].validator;
				if (validator & 0x80000000) {
					continue; //uninitialized
				}
				if (validator != 0xFFFFFFFF) {
					chunks[i / elements_in_chunk][i % elements_in_chunk].data.~T();
				}
			}
		}

		uint32_t chunk_count = max_alloc / elements_in_chunk;
		for (uint32_t i = 0; i < chunk_count; i++) {
			memfree(chunks[i]);
			memfree(free_list_chunks[i]);
		}

		if (chunks) {
			memfree(chunks);
			memfree(free_list_chunks);
		}
	}
};

template <typename T, bool THREAD_SAFE = false>
class RID_PtrOwner {
	RID_Alloc<T *, THREAD_SAFE> alloc;

public:
	_FORCE_INLINE_ RID make_rid(T *p_ptr) {
		return alloc.make_rid(p_ptr);
	}

	_FORCE_INLINE_ RID allocate_rid() {
		return alloc.allocate_rid();
	}

	_FORCE_INLINE_ void initialize_rid(RID p_rid, T *p_ptr) {
		alloc.initialize_rid(p_rid, p_ptr);
	}

	_FORCE_INLINE_ T *get_or_null(const RID &p_rid) {
		T **ptr = alloc.get_or_null(p_rid);
		if (unlikely(!ptr)) {
			return nullptr;
		}
		return *ptr;
	}

	_FORCE_INLINE_ void replace(const RID &p_rid, T *p_new_ptr) {
		T **ptr = alloc.get_or_null(p_rid);
		ERR_FAIL_NULL(ptr);
		*ptr = p_new_ptr;
	}

	_FORCE_INLINE_ bool owns(const RID &p_rid) const {
		return alloc.owns(p_rid);
	}

	_FORCE_INLINE_ void free(const RID &p_rid) {
		alloc.free(p_rid);
	}

	_FORCE_INLINE_ uint32_t get_rid_count() const {
		return alloc.get_rid_count();
	}

	_FORCE_INLINE_ void get_owned_list(List<RID> *p_owned) const {
		return alloc.get_owned_list(p_owned);
	}

	void fill_owned_buffer(RID *p_rid_buffer) const {
		alloc.fill_owned_buffer(p_rid_buffer);
	}

	void set_description(const char *p_description) {
		alloc.set_description(p_description);
	}

	RID_PtrOwner(uint32_t p_target_chunk_byte_size = 65536, uint32_t p_maximum_number_of_elements = 262144) :
			alloc(p_target_chunk_byte_size, p_maximum_number_of_elements) {}
};

template <typename T, bool THREAD_SAFE = false>
class RID_Owner {
	RID_Alloc<T, THREAD_SAFE> alloc;

public:
	_FORCE_INLINE_ RID make_rid() {
		return alloc.make_rid();
	}
	_FORCE_INLINE_ RID make_rid(const T &p_ptr) {
		return alloc.make_rid(p_ptr);
	}

	_FORCE_INLINE_ RID allocate_rid() {
		return alloc.allocate_rid();
	}

	_FORCE_INLINE_ void initialize_rid(RID p_rid) {
		alloc.initialize_rid(p_rid);
	}

	_FORCE_INLINE_ void initialize_rid(RID p_rid, const T &p_ptr) {
		alloc.initialize_rid(p_rid, p_ptr);
	}

	_FORCE_INLINE_ T *get_or_null(const RID &p_rid) {
		return alloc.get_or_null(p_rid);
	}

	_FORCE_INLINE_ bool owns(const RID &p_rid) const {
		return alloc.owns(p_rid);
	}

	_FORCE_INLINE_ void free(const RID &p_rid) {
		alloc.free(p_rid);
	}

	_FORCE_INLINE_ uint32_t get_rid_count() const {
		return alloc.get_rid_count();
	}

	_FORCE_INLINE_ void get_owned_list(List<RID> *p_owned) const {
		return alloc.get_owned_list(p_owned);
	}
	void fill_owned_buffer(RID *p_rid_buffer) const {
		alloc.fill_owned_buffer(p_rid_buffer);
	}

	void set_description(const char *p_description) {
		alloc.set_description(p_description);
	}
	RID_Owner(uint32_t p_target_chunk_byte_size = 65536, uint32_t p_maximum_number_of_elements = 262144) :
			alloc(p_target_chunk_byte_size, p_maximum_number_of_elements) {}
};
