/**************************************************************************/
/*  sort_array.h                                                          */
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
#include "core/typedefs.h"

#define ERR_BAD_COMPARE(cond)                                         \
	if (unlikely(cond)) {                                             \
		ERR_PRINT("bad comparison function; sorting will be broken"); \
		break;                                                        \
	}

template <typename T>
struct _DefaultComparator {
	_FORCE_INLINE_ bool operator()(const T &a, const T &b) const { return (a < b); }
};

#ifdef DEBUG_ENABLED
#define SORT_ARRAY_VALIDATE_ENABLED true
#else
#define SORT_ARRAY_VALIDATE_ENABLED false
#endif

template <typename T, typename Comparator = _DefaultComparator<T>, bool Validate = SORT_ARRAY_VALIDATE_ENABLED>
class SortArray {
	enum {
		INTROSORT_THRESHOLD = 16
	};

public:
	Comparator compare;

	inline const T &median_of_3(const T &a, const T &b, const T &c) const {
		if (compare(a, b)) {
			if (compare(b, c)) {
				return b;
			} else if (compare(a, c)) {
				return c;
			} else {
				return a;
			}
		} else if (compare(a, c)) {
			return a;
		} else if (compare(b, c)) {
			return c;
		} else {
			return b;
		}
	}

	inline int64_t bitlog(int64_t n) const {
		int64_t k;
		for (k = 0; n != 1; n >>= 1) {
			++k;
		}
		return k;
	}

	/* Heap / Heapsort functions */

	inline void push_heap(int64_t p_first, int64_t p_hole_idx, int64_t p_top_index, T p_value, T *p_array) const {
		int64_t parent = (p_hole_idx - 1) / 2;
		while (p_hole_idx > p_top_index && compare(p_array[p_first + parent], p_value)) {
			p_array[p_first + p_hole_idx] = p_array[p_first + parent];
			p_hole_idx = parent;
			parent = (p_hole_idx - 1) / 2;
		}
		p_array[p_first + p_hole_idx] = p_value;
	}

	inline void pop_heap(int64_t p_first, int64_t p_last, int64_t p_result, T p_value, T *p_array) const {
		p_array[p_result] = p_array[p_first];
		adjust_heap(p_first, 0, p_last - p_first, p_value, p_array);
	}
	inline void pop_heap(int64_t p_first, int64_t p_last, T *p_array) const {
		pop_heap(p_first, p_last - 1, p_last - 1, p_array[p_last - 1], p_array);
	}

	inline void adjust_heap(int64_t p_first, int64_t p_hole_idx, int64_t p_len, T p_value, T *p_array) const {
		int64_t top_index = p_hole_idx;
		int64_t second_child = 2 * p_hole_idx + 2;

		while (second_child < p_len) {
			if (compare(p_array[p_first + second_child], p_array[p_first + (second_child - 1)])) {
				second_child--;
			}

			p_array[p_first + p_hole_idx] = p_array[p_first + second_child];
			p_hole_idx = second_child;
			second_child = 2 * (second_child + 1);
		}

		if (second_child == p_len) {
			p_array[p_first + p_hole_idx] = p_array[p_first + (second_child - 1)];
			p_hole_idx = second_child - 1;
		}
		push_heap(p_first, p_hole_idx, top_index, p_value, p_array);
	}

	inline void sort_heap(int64_t p_first, int64_t p_last, T *p_array) const {
		while (p_last - p_first > 1) {
			pop_heap(p_first, p_last--, p_array);
		}
	}

	inline void make_heap(int64_t p_first, int64_t p_last, T *p_array) const {
		if (p_last - p_first < 2) {
			return;
		}
		int64_t len = p_last - p_first;
		int64_t parent = (len - 2) / 2;

		while (true) {
			adjust_heap(p_first, parent, len, p_array[p_first + parent], p_array);
			if (parent == 0) {
				return;
			}
			parent--;
		}
	}

	inline void partial_sort(int64_t p_first, int64_t p_last, int64_t p_middle, T *p_array) const {
		make_heap(p_first, p_middle, p_array);
		for (int64_t i = p_middle; i < p_last; i++) {
			if (compare(p_array[i], p_array[p_first])) {
				pop_heap(p_first, p_middle, i, p_array[i], p_array);
			}
		}
		sort_heap(p_first, p_middle, p_array);
	}

	inline void partial_select(int64_t p_first, int64_t p_last, int64_t p_middle, T *p_array) const {
		make_heap(p_first, p_middle, p_array);
		for (int64_t i = p_middle; i < p_last; i++) {
			if (compare(p_array[i], p_array[p_first])) {
				pop_heap(p_first, p_middle, i, p_array[i], p_array);
			}
		}
	}

	inline int64_t partitioner(int64_t p_first, int64_t p_last, T p_pivot, T *p_array) const {
		const int64_t unmodified_first = p_first;
		const int64_t unmodified_last = p_last;

		while (true) {
			while (compare(p_array[p_first], p_pivot)) {
				if constexpr (Validate) {
					ERR_BAD_COMPARE(p_first == unmodified_last - 1);
				}
				p_first++;
			}
			p_last--;
			while (compare(p_pivot, p_array[p_last])) {
				if constexpr (Validate) {
					ERR_BAD_COMPARE(p_last == unmodified_first);
				}
				p_last--;
			}

			if (!(p_first < p_last)) {
				return p_first;
			}

			SWAP(p_array[p_first], p_array[p_last]);
			p_first++;
		}
	}

	inline void introsort(int64_t p_first, int64_t p_last, T *p_array, int64_t p_max_depth) const {
		while (p_last - p_first > INTROSORT_THRESHOLD) {
			if (p_max_depth == 0) {
				partial_sort(p_first, p_last, p_last, p_array);
				return;
			}

			p_max_depth--;

			int64_t cut = partitioner(
					p_first,
					p_last,
					median_of_3(
							p_array[p_first],
							p_array[p_first + (p_last - p_first) / 2],
							p_array[p_last - 1]),
					p_array);

			introsort(cut, p_last, p_array, p_max_depth);
			p_last = cut;
		}
	}

	inline void introselect(int64_t p_first, int64_t p_nth, int64_t p_last, T *p_array, int64_t p_max_depth) const {
		while (p_last - p_first > 3) {
			if (p_max_depth == 0) {
				partial_select(p_first, p_nth + 1, p_last, p_array);
				SWAP(p_first, p_nth);
				return;
			}

			p_max_depth--;

			int64_t cut = partitioner(
					p_first,
					p_last,
					median_of_3(
							p_array[p_first],
							p_array[p_first + (p_last - p_first) / 2],
							p_array[p_last - 1]),
					p_array);

			if (cut <= p_nth) {
				p_first = cut;
			} else {
				p_last = cut;
			}
		}

		insertion_sort(p_first, p_last, p_array);
	}

	inline void unguarded_linear_insert(int64_t p_last, T p_value, T *p_array) const {
		int64_t next = p_last - 1;
		while (compare(p_value, p_array[next])) {
			if constexpr (Validate) {
				ERR_BAD_COMPARE(next == 0);
			}
			p_array[p_last] = p_array[next];
			p_last = next;
			next--;
		}
		p_array[p_last] = p_value;
	}

	inline void linear_insert(int64_t p_first, int64_t p_last, T *p_array) const {
		T val = p_array[p_last];
		if (compare(val, p_array[p_first])) {
			for (int64_t i = p_last; i > p_first; i--) {
				p_array[i] = p_array[i - 1];
			}

			p_array[p_first] = val;
		} else {
			unguarded_linear_insert(p_last, val, p_array);
		}
	}

	inline void insertion_sort(int64_t p_first, int64_t p_last, T *p_array) const {
		if (p_first == p_last) {
			return;
		}
		for (int64_t i = p_first + 1; i != p_last; i++) {
			linear_insert(p_first, i, p_array);
		}
	}

	inline void unguarded_insertion_sort(int64_t p_first, int64_t p_last, T *p_array) const {
		for (int64_t i = p_first; i != p_last; i++) {
			unguarded_linear_insert(i, p_array[i], p_array);
		}
	}

	inline void final_insertion_sort(int64_t p_first, int64_t p_last, T *p_array) const {
		if (p_last - p_first > INTROSORT_THRESHOLD) {
			insertion_sort(p_first, p_first + INTROSORT_THRESHOLD, p_array);
			unguarded_insertion_sort(p_first + INTROSORT_THRESHOLD, p_last, p_array);
		} else {
			insertion_sort(p_first, p_last, p_array);
		}
	}

	inline void sort_range(int64_t p_first, int64_t p_last, T *p_array) const {
		if (p_first != p_last) {
			introsort(p_first, p_last, p_array, bitlog(p_last - p_first) * 2);
			final_insertion_sort(p_first, p_last, p_array);
		}
	}

	inline void sort(T *p_array, int64_t p_len) const {
		sort_range(0, p_len, p_array);
	}

	inline void nth_element(int64_t p_first, int64_t p_last, int64_t p_nth, T *p_array) const {
		if (p_first == p_last || p_nth == p_last) {
			return;
		}
		introselect(p_first, p_nth, p_last, p_array, bitlog(p_last - p_first) * 2);
	}
};
