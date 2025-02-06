/**************************************************************************/
/*  slab_allocator.h                                                      */
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

#ifndef SLAB_ALLOCATOR_H
#define SLAB_ALLOCATOR_H

#include "core/core_globals.h"
#include "core/os/spin_lock.h"
#include "core/string/ustring.h"
#include "core/typedefs.h"

#include <assert.h>
#include <stdio.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>
#include <typeinfo>

#if defined(__GNUC__) || defined(__clang__)
#if (__has_builtin(__builtin_popcountll))
#define builtin_popcountll
static_assert(sizeof(unsigned long long) == sizeof(uint64_t), "uint64_t and unsigned long long must have the same size");
static_assert((uint64_t)(-1) == (unsigned long long)(-1), "uint64_t and unsigned long long must have the same representation");
#endif
#if (__has_builtin(__builtin_ffsll))
#define builtin_ffsll
static_assert(sizeof(long long) == sizeof(int64_t), "int64_t and long long must have the same size");
static_assert((int64_t)(-1) == (long long)(-1), "int64_t and long long must have the same representation");
#endif
#endif

#if defined(builtin_popcountll)
_ALWAYS_INLINE_ static int popcount64(uint64_t p_word) {
	return __builtin_popcountll(p_word);
}
#else
// Hamming weight calculation version of popcount.
_ALWAYS_INLINE_ static int popcount64(uint64_t p_word) {
	p_word = p_word - ((p_word >> 1) & 0x5555555555555555);
	p_word = (p_word & 0x3333333333333333) + ((p_word >> 2) & 0x3333333333333333);
	p_word = (p_word + (p_word >> 4)) & 0x0F0F0F0F0F0F0F0F;
	p_word = p_word + (p_word >> 8);
	p_word = p_word + (p_word >> 16);
	p_word = p_word + (p_word >> 32);
	return p_word & 0x7F;
}
#endif

#if defined(builtin_ffsll)
_ALWAYS_INLINE_ static int ffs64(uint64_t p_word) {
	return __builtin_ffsll(int64_t(p_word));
}
#else
_ALWAYS_INLINE_ static int ffs64(uint64_t p_word) {
	if (p_word == 0) {
		return 0;
	}

	int pos = 1;

	if ((p_word & 0xFFFFFFFF) == 0) {
		pos += 32;
		p_word >>= 32;
	}
	if ((p_word & 0xFFFF) == 0) {
		pos += 16;
		p_word >>= 16;
	}
	if ((p_word & 0xFF) == 0) {
		pos += 8;
		p_word >>= 8;
	}
	if ((p_word & 0xF) == 0) {
		pos += 4;
		p_word >>= 4;
	}
	if ((p_word & 0x3) == 0) {
		pos += 2;
		p_word >>= 2;
	}
	if ((p_word & 0x1) == 0) {
		pos += 1;
	}

	return pos;
}
#endif

_ALWAYS_INLINE_ static uint32_t count_set_bits64(uint64_t p_word) {
	return popcount64(p_word);
}

_ALWAYS_INLINE_ static uint32_t find_first_trailing_set_bit64(uint64_t p_word) {
	return ffs64(p_word) - 1;
}

static constexpr size_t OBJECTS_PER_SLAB = 64;

template <typename T>
class ThreadSafeSlabAllocator {
	static_assert(sizeof(T) <= 512, "Size of class too big for ThreadSafeSlabAllocator, use PagedAllocator");

	static constexpr uint8_t REUSE_LOW_WATERMARK = sizeof(T) > 128 ? 32 : 16;

	struct Item {
		alignas(T) char object[sizeof(T)];
		uint8_t index;
	};

	struct alignas(64) Slab {
		enum slabstate {
			IN_USE,
			UNUSED,
			IN_USABLE,
		};

		Item objects[OBJECTS_PER_SLAB];
		uint64_t bitmap = UINT64_MAX;
		SpinLock lock;
		Slab *next = nullptr;
		Slab *next_available = nullptr;
		slabstate state = IN_USE;

		template <typename... Args>
		T *allocate(Args &&...p_args) {
			lock.lock();
			// Under lock, nobody else is writing. Locking/unlocking is a barrier.
			if (likely(bitmap != 0)) {
				uint8_t index = find_first_trailing_set_bit64(bitmap);
				bitmap &= ~(1ULL << index);
				lock.unlock();
				objects[index].index = index;
				if constexpr (!std::is_trivially_constructible<T>()) {
					new (&(objects[index].object)) T(std::forward<Args>(p_args)...);
				}
				return (T *)&(objects[index].object);
			}
			// We're about to replace our slab.
			state = UNUSED;
			lock.unlock();
			return nullptr;
		}

		void deallocate(Item *p_ptr) {
			if constexpr (!std::is_trivially_destructible<T>()) {
				(*((T *)&p_ptr->object)).~T();
			}
			lock.lock();
			bitmap |= (1ULL << p_ptr->index);
			// state cannot change while we're under lock.
			if (unlikely(state == UNUSED && count_set_bits64(bitmap) <= REUSE_LOW_WATERMARK)) {
				state = IN_USABLE;
				// It is safe to unlock now. We won't be reconsidered for adding to the usable pool.
				// And we can't yet be taken out of the free pool because the usable pool's head has.
				// not yet been changed.
				lock.unlock();

				usable_spin_lock.lock();
				// Under lock, nobody else is writing. Locking/unlocking is a barrier.
				Slab *global = global_usable_slabs;
				next_available = global;
				global_usable_slabs = this;
				usable_spin_lock.unlock();

				// We could now be reconsidered for allocation.
				return;
			}

			lock.unlock();
		}

		void claim() {
			lock.lock();
			state = IN_USE;
			lock.unlock();
		}
	};

	inline static thread_local Slab *local_slab = nullptr;
	inline static Slab *global_slabs = nullptr;
	inline static Slab *global_usable_slabs = nullptr;
	inline static SpinLock alloc_spin_lock;
	inline static SpinLock usable_spin_lock;
	inline static std::atomic<size_t> allocator_count = 0;

	Slab *allocate_slab() {
		// Not under lock, guessing.
		if (global_usable_slabs) {
			usable_spin_lock.lock();
			// Under lock, nobody else is writing. Locking/unlocking is a barrier.
			if (global_usable_slabs) {
				Slab *new_slab = global_usable_slabs;
				Slab *next_global_usable = global_usable_slabs->next_available;
				global_usable_slabs = next_global_usable;
				usable_spin_lock.unlock();

				new_slab->claim();
				return new_slab;
			}
			usable_spin_lock.unlock();
		}

		void *raw = std::malloc(sizeof(Slab));
		Slab *slab = new (raw) Slab();

		alloc_spin_lock.lock();
		slab->next = global_slabs;
		global_slabs = slab;
		alloc_spin_lock.unlock();

		return slab;
	}

	bool _check_used() {
		Slab *current = global_slabs;
		while (current) {
			Slab *next = current->next;
			if (current->bitmap != UINT64_MAX) {
				return true;
			}
			current = next;
		}

		return false;
	}

public:
	template <typename... Args>
	T *alloc(Args &&...p_args) {
		if (unlikely(!local_slab)) {
			local_slab = allocate_slab();
		}

		while (true) {
			T *result = local_slab->allocate(std::forward<Args>(p_args)...);
			if (likely(result)) {
				return result;
			}

			Slab *new_slab = allocate_slab();
			local_slab = new_slab;
		}
	}

	template <typename... Args>
	T *new_allocation(Args &&...p_args) { return alloc(std::forward<Args>(p_args)...); }
	void delete_allocation(T *p_mem) { free(p_mem); }

	void free(T *p_mem) {
		Item *item = reinterpret_cast<Item *>(p_mem);
		Slab *slab = reinterpret_cast<Slab *>(reinterpret_cast<char *>(item) - (item->index * sizeof(Item)) - offsetof(Slab, objects));

		slab->deallocate(item);
	}

	ThreadSafeSlabAllocator() {
		allocator_count.fetch_add(1, std::memory_order_relaxed);
	}

	~ThreadSafeSlabAllocator() {
		if (allocator_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			alloc_spin_lock.lock();
			usable_spin_lock.lock();
			bool leaked = _check_used();
			if (leaked) {
				if (CoreGlobals::leak_reporting_enabled) {
					ERR_PRINT(String("Slabs in use at exit in ThreadSafeSlabAllocator: ") + String(typeid(T).name()));
				}
			} else {
				Slab *current = global_slabs;
				while (current) {
					if (current->bitmap) {
						leaked = true;
					}

					Slab *next = current->next;
					std::free(current);
					current = next;
				}
				global_slabs = nullptr;
				global_usable_slabs = nullptr;
			}
			local_slab = nullptr;
			usable_spin_lock.unlock();
			alloc_spin_lock.unlock();
		}
	}
};

template <typename T>
class SlabAllocator {
	static_assert(sizeof(T) <= 512, "Size of class too big for SlabAllocator, use PagedAllocator");

	static constexpr uint8_t REUSE_LOW_WATERMARK = sizeof(T) > 128 ? 32 : 16;

	struct Item {
		alignas(T) char object[sizeof(T)];
		uint8_t index;
	};

	struct alignas(64) Slab {
		enum slabstate {
			IN_USE,
			UNUSED,
			IN_USABLE,
		};

		Item objects[OBJECTS_PER_SLAB];
		uint64_t bitmap = UINT64_MAX;
		Slab *next = nullptr;
		Slab *next_available = nullptr;
		slabstate state = IN_USE;

		template <typename... Args>
		T *allocate(Args &&...p_args) {
			// Bitmap already checked in SlabAllocator::alloc().
			uint8_t index = find_first_trailing_set_bit64(bitmap);
			bitmap &= ~(1ULL << index);
			objects[index].index = index;
			if constexpr (!std::is_trivially_constructible<T>()) {
				new (&(objects[index].object)) T(std::forward<Args>(p_args)...);
			}
			return (T *)&(objects[index].object);
		}

		void deallocate(Item *p_ptr) {
			if constexpr (!std::is_trivially_destructible<T>()) {
				(*((T *)&p_ptr->object)).~T();
			}
			bitmap |= (1ULL << p_ptr->index);
		}
	};

	Slab *current_slab = nullptr;
	Slab *slabs = nullptr;
	Slab *usable_slabs = nullptr;

	Slab *allocate_slab() {
		if (usable_slabs) {
			Slab *new_slab = usable_slabs;
			Slab *next_global_usable = usable_slabs->next_available;
			usable_slabs = next_global_usable;

			new_slab->state = Slab::IN_USE;
			return new_slab;
		}

		void *raw = std::malloc(sizeof(Slab));
		Slab *slab = new (raw) Slab();

		slab->next = slabs;
		slabs = slab;

		return slab;
	}

	bool _check_used() {
		Slab *current = slabs;
		while (current) {
			Slab *next = current->next;
			if (current->bitmap != UINT64_MAX) {
				return true;
			}
			current = next;
		}

		return false;
	}

public:
	template <typename... Args>
	T *alloc(Args &&...p_args) {
		if (!current_slab->bitmap) {
			current_slab->state = Slab::UNUSED;
			current_slab = allocate_slab();
		}

		return current_slab->allocate(std::forward<Args>(p_args)...);
	}

	template <typename... Args>
	T *new_allocation(Args &&...p_args) { return alloc(std::forward<Args>(p_args)...); }
	void delete_allocation(T *p_mem) { free(p_mem); }

	void free(T *p_mem) {
		Item *item = reinterpret_cast<Item *>(p_mem);
		Slab *slab = reinterpret_cast<Slab *>(reinterpret_cast<char *>(item) - (item->index * sizeof(Item)) - offsetof(Slab, objects));

		slab->deallocate(item);

		if (slab->state == Slab::UNUSED && count_set_bits64(slab->bitmap) <= REUSE_LOW_WATERMARK) {
			slab->state = Slab::IN_USABLE;
			Slab *usable = usable_slabs;
			slab->next_available = usable;
			usable_slabs = slab;
		}
	}

	SlabAllocator() {
		current_slab = allocate_slab();
	}

	~SlabAllocator() {
		if (_check_used()) {
			if (CoreGlobals::leak_reporting_enabled) {
				ERR_PRINT(String("Slabs in use at exit in SlabAllocator: ") + String(typeid(T).name()));
			}
		}

		Slab *current = slabs;
		while (current) {
			Slab *next = current->next;
			std::free(current);
			current = next;
		}
	}
};

#endif // SLAB_ALLOCATOR_H
