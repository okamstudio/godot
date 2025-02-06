/**************************************************************************/
/*  bin_sorted_array.h                                                    */
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

#include "core/templates/local_vector.h"
#include "core/templates/paged_array.h"

template <typename T>
class BinSortedArray {
	PagedArray<T> array;
	LocalVector<uint64_t> bin_limits;

	// Implement if elements need to keep track of their own index in the array.
	_FORCE_INLINE_ virtual void _update_idx(T &r_element, uint64_t p_idx) {}

	_FORCE_INLINE_ void _swap(uint64_t p_a, uint64_t p_b) {
		SWAP(array[p_a], array[p_b]);
		_update_idx(array[p_a], p_a);
		_update_idx(array[p_b], p_b);
	}

public:
	uint64_t insert(T &p_element, uint64_t p_bin) {
		array.push_back(p_element);
		uint64_t new_idx = array.size() - 1;
		_update_idx(p_element, new_idx);
		bin_limits[0] = new_idx;
		if (p_bin != 0) {
			new_idx = move(new_idx, p_bin);
		}
		return new_idx;
	}

	uint64_t move(uint64_t p_idx, uint64_t p_bin) {
		ERR_FAIL_UNSIGNED_INDEX_V(p_idx, array.size(), -1);

		uint64_t current_bin = bin_limits.size() - 1;
		while (p_idx > bin_limits[current_bin]) {
			current_bin--;
		}

		if (p_bin == current_bin) {
			return p_idx;
		}

		uint64_t current_idx = p_idx;
		if (p_bin > current_bin) {
			while (p_bin > current_bin) {
				uint64_t swap_idx = 0;

				if (current_bin == bin_limits.size() - 1) {
					bin_limits.push_back(0);
				} else {
					bin_limits[current_bin + 1]++;
					swap_idx = bin_limits[current_bin + 1];
				}

				if (current_idx != swap_idx) {
					_swap(current_idx, swap_idx);
					current_idx = swap_idx;
				}

				current_bin++;
			}
		} else {
			while (p_bin < current_bin) {
				uint64_t swap_idx = bin_limits[current_bin];

				if (current_idx != swap_idx) {
					_swap(current_idx, swap_idx);
				}

				if (current_bin == bin_limits.size() - 1 && bin_limits[current_bin] == 0) {
					bin_limits.resize(bin_limits.size() - 1);
				} else {
					bin_limits[current_bin]--;
				}
				current_idx = swap_idx;
				current_bin--;
			}
		}

		return current_idx;
	}

	void remove_at(uint64_t p_idx) {
		ERR_FAIL_UNSIGNED_INDEX(p_idx, array.size());
		uint64_t new_idx = move(p_idx, 0);
		uint64_t swap_idx = array.size() - 1;

		if (new_idx != swap_idx) {
			_swap(new_idx, swap_idx);
		}

		if (bin_limits[0] > 0) {
			bin_limits[0]--;
		}

		array.pop_back();
	}

	void set_page_pool(PagedArrayPool<T> *p_page_pool) {
		array.set_page_pool(p_page_pool);
	}

	_FORCE_INLINE_ const T &operator[](uint64_t p_index) const {
		return array[p_index];
	}

	_FORCE_INLINE_ T &operator[](uint64_t p_index) {
		return array[p_index];
	}

	int get_bin_count() {
		if (array.size() == 0) {
			return 0;
		}
		return bin_limits.size();
	}

	int get_bin_start(int p_bin) {
		ERR_FAIL_COND_V(p_bin >= get_bin_count(), ~0U);
		if ((unsigned int)p_bin == bin_limits.size() - 1) {
			return 0;
		}
		return bin_limits[p_bin + 1] + 1;
	}

	int get_bin_size(int p_bin) {
		ERR_FAIL_COND_V(p_bin >= get_bin_count(), 0);
		if ((unsigned int)p_bin == bin_limits.size() - 1) {
			return bin_limits[p_bin] + 1;
		}
		return bin_limits[p_bin] - bin_limits[p_bin + 1];
	}

	void reset() {
		array.reset();
		bin_limits.clear();
		bin_limits.push_back(0);
	}

	BinSortedArray() {
		bin_limits.push_back(0);
	}

	virtual ~BinSortedArray() {
		reset();
	}
};
