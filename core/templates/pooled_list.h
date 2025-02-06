/**************************************************************************/
/*  pooled_list.h                                                         */
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

// Simple template to provide a pool with O(1) allocate and free.
// The freelist could alternatively be a linked list placed within the unused elements
// to use less memory, however a separate freelist is probably more cache friendly.

// NOTE : Take great care when using this with non POD types. The construction and destruction
// is done in the LocalVector, NOT as part of the pool. So requesting a new item does not guarantee
// a constructor is run, and free does not guarantee a destructor.
// You should generally handle clearing
// an item explicitly after a request, as it may contain 'leftovers'.
// This is by design for fastest use in the BVH. If you want a more general pool
// that does call constructors / destructors on request / free, this should probably be
// a separate template.

// The zero_on_first_request feature is optional and is useful for e.g. pools of handles,
// which may use a ref count which we want to be initialized to zero the first time a handle is created,
// but left alone on subsequent allocations (as will typically be incremented).

// Note that there is no function to compact the pool - this would
// invalidate any existing pool IDs held externally.
// Compaction can be done but would rely on a more complex method
// of preferentially giving out lower IDs in the freelist first.

#include "core/templates/local_vector.h"

template <typename T, typename U = uint32_t, bool force_trivial = false, bool zero_on_first_request = false>
class PooledList {
	LocalVector<T, U, force_trivial> list;
	LocalVector<U, U, true> freelist;

	// not all list members are necessarily used
	U _used_size;

public:
	PooledList() {
		_used_size = 0;
	}

	// Use with care, in most cases you should make sure to
	// free all elements first (i.e. _used_size would be zero),
	// although it could also be used without this as an optimization
	// in some cases.
	void clear() {
		list.clear();
		freelist.clear();
		_used_size = 0;
	}

	uint64_t estimate_memory_use() const {
		return ((uint64_t)list.size() * sizeof(T)) + ((uint64_t)freelist.size() * sizeof(U));
	}

	const T &operator[](U p_index) const {
		return list[p_index];
	}
	T &operator[](U p_index) {
		return list[p_index];
	}

	// To be explicit in a pool there is a distinction
	// between the number of elements that are currently
	// in use, and the number of elements that have been reserved.
	// Using size() would be vague.
	U used_size() const { return _used_size; }
	U reserved_size() const { return list.size(); }

	T *request(U &r_id) {
		_used_size++;

		if (freelist.size()) {
			// pop from freelist
			int new_size = freelist.size() - 1;
			r_id = freelist[new_size];
			freelist.resize(new_size);

			return &list[r_id];
		}

		r_id = list.size();
		list.resize(r_id + 1);

		static_assert((!zero_on_first_request) || (__is_pod(T)), "zero_on_first_request requires trivial type");
		if constexpr (zero_on_first_request && __is_pod(T)) {
			list[r_id] = {};
		}

		return &list[r_id];
	}
	void free(const U &p_id) {
		// should not be on free list already
		ERR_FAIL_UNSIGNED_INDEX(p_id, list.size());
		freelist.push_back(p_id);
		ERR_FAIL_COND_MSG(!_used_size, "_used_size has become out of sync, have you double freed an item?");
		_used_size--;
	}
};

// a pooled list which automatically keeps a list of the active members
template <typename T, typename U = uint32_t, bool force_trivial = false, bool zero_on_first_request = false>
class TrackedPooledList {
public:
	U pool_used_size() const { return _pool.used_size(); }
	U pool_reserved_size() const { return _pool.reserved_size(); }
	U active_size() const { return _active_list.size(); }

	// use with care, see the earlier notes in the PooledList clear()
	void clear() {
		_pool.clear();
		_active_list.clear();
		_active_map.clear();
	}

	U get_active_id(U p_index) const {
		return _active_list[p_index];
	}

	const T &get_active(U p_index) const {
		return _pool[get_active_id(p_index)];
	}

	T &get_active(U p_index) {
		return _pool[get_active_id(p_index)];
	}

	const T &operator[](U p_index) const {
		return _pool[p_index];
	}
	T &operator[](U p_index) {
		return _pool[p_index];
	}

	T *request(U &r_id) {
		T *item = _pool.request(r_id);

		// add to the active list
		U active_list_id = _active_list.size();
		_active_list.push_back(r_id);

		// expand the active map (this should be in sync with the pool list
		if (_pool.used_size() > _active_map.size()) {
			_active_map.resize(_pool.used_size());
		}

		// store in the active map
		_active_map[r_id] = active_list_id;

		return item;
	}

	void free(const U &p_id) {
		_pool.free(p_id);

		// remove from the active list.
		U list_id = _active_map[p_id];

		// zero the _active map to detect bugs (only in debug?)
		_active_map[p_id] = -1;

		_active_list.remove_unordered(list_id);

		// keep the replacement in sync with the correct list Id
		if (list_id < _active_list.size()) {
			// which pool id has been replaced in the active list
			U replacement_id = _active_list[list_id];

			// keep that replacements map up to date with the new position
			_active_map[replacement_id] = list_id;
		}
	}

	const LocalVector<U, U> &get_active_list() const { return _active_list; }

private:
	PooledList<T, U, force_trivial, zero_on_first_request> _pool;
	LocalVector<U, U> _active_map;
	LocalVector<U, U> _active_list;
};
