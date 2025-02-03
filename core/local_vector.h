/**************************************************************************/
/*  local_vector.h                                                        */
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

#ifndef LOCAL_VECTOR_H
#define LOCAL_VECTOR_H

#include "core/error_macros.h"
#include "core/os/memory.h"
#include "core/pool_vector.h"
#include "core/sort_array.h"
#include "core/vector.h"

#include <type_traits>
#include <utility>

template <class T, class U = uint32_t, bool force_trivial = false>
class LocalVector {
protected:
	U count = 0;
	U capacity = 0;
	T *data = nullptr;

public:
	T *ptr() {
		return data;
	}

	const T *ptr() const {
		return data;
	}

	_FORCE_INLINE_ void push_back(T p_elem) {
		if (unlikely(count == capacity)) {
			if (capacity == 0) {
				capacity = 1;
			} else {
				capacity <<= 1;
			}
			data = (T *)memrealloc(data, capacity * sizeof(T));
			CRASH_COND_MSG(!data, "Out of memory");
		}

		if (!std::is_trivially_constructible<T>::value && !force_trivial) {
			memnew_placement(&data[count++], T(std::move(p_elem)));
		} else {
			data[count++] = std::move(p_elem);
		}
	}

	void remove(U p_index) {
		ERR_FAIL_UNSIGNED_INDEX(p_index, count);
		count--;
		for (U i = p_index; i < count; i++) {
			data[i] = std::move(data[i + 1]);
		}
		if (!std::is_trivially_destructible<T>::value && !force_trivial) {
			data[count].~T();
		}
	}

	// Removes the item copying the last value into the position of the one to
	// remove. It's generally faster than `remove`.
	void remove_unordered(U p_index) {
		ERR_FAIL_INDEX(p_index, count);
		count--;
		if (count > p_index) {
			data[p_index] = std::move(data[count]);
		}
		if (!std::is_trivially_destructible<T>::value && !force_trivial) {
			data[count].~T();
		}
	}

	void erase(const T &p_val) {
		int64_t idx = find(p_val);
		if (idx >= 0) {
			remove(idx);
		}
	}

	U erase_multiple_unordered(const T &p_val) {
		U from = 0;
		U count = 0;
		while (true) {
			int64_t idx = find(p_val, from);

			if (idx == -1) {
				break;
			}
			remove_unordered(idx);
			from = idx;
			count++;
		}
		return count;
	}

	void invert() {
		for (U i = 0; i < count / 2; i++) {
			SWAP(data[i], data[count - i - 1]);
		}
	}

	_FORCE_INLINE_ void clear() { resize(0); }
	_FORCE_INLINE_ void reset() {
		clear();
		if (data) {
			memfree(data);
			data = nullptr;
			capacity = 0;
		}
	}
	_FORCE_INLINE_ bool empty() const { return count == 0; }
	_FORCE_INLINE_ U get_capacity() const { return capacity; }

	_FORCE_INLINE_ void reserve(U p_size, bool p_allow_shrink = false) {
		p_size = nearest_power_of_2_templated(p_size);
		if (!p_allow_shrink ? p_size > capacity : ((p_size >= count) && (p_size != capacity))) {
			capacity = p_size;
			data = (T *)memrealloc(data, capacity * sizeof(T));
			CRASH_COND_MSG(!data, "Out of memory");
		}
	}

	_FORCE_INLINE_ U size() const { return count; }
	void resize(U p_size) {
		if (p_size < count) {
			if (!std::is_trivially_destructible<T>::value && !force_trivial) {
				for (U i = p_size; i < count; i++) {
					data[i].~T();
				}
			}
			count = p_size;
		} else if (p_size > count) {
			if (unlikely(p_size > capacity)) {
				if (capacity == 0) {
					capacity = 1;
				}
				while (capacity < p_size) {
					capacity <<= 1;
				}
				data = (T *)memrealloc(data, capacity * sizeof(T));
				CRASH_COND_MSG(!data, "Out of memory");
			}
			if (!std::is_trivially_constructible<T>::value && !force_trivial) {
				for (U i = count; i < p_size; i++) {
					memnew_placement(&data[i], T);
				}
			}
			count = p_size;
		}
	}
	_FORCE_INLINE_ const T &operator[](U p_index) const {
		CRASH_BAD_UNSIGNED_INDEX(p_index, count);
		return data[p_index];
	}
	_FORCE_INLINE_ T &operator[](U p_index) {
		CRASH_BAD_UNSIGNED_INDEX(p_index, count);
		return data[p_index];
	}

	void fill(T p_val) {
		for (U i = 0; i < count; i++) {
			data[i] = p_val;
		}
	}

	void insert(U p_pos, T p_val) {
		ERR_FAIL_UNSIGNED_INDEX(p_pos, count + 1);
		if (p_pos == count) {
			push_back(std::move(p_val));
		} else {
			resize(count + 1);
			for (U i = count - 1; i > p_pos; i--) {
				data[i] = std::move(data[i - 1]);
			}
			data[p_pos] = std::move(p_val);
		}
	}

	int64_t find(const T &p_val, U p_from = 0) const {
		for (U i = p_from; i < count; i++) {
			if (data[i] == p_val) {
				return int64_t(i);
			}
		}
		return -1;
	}

	template <class C>
	void sort_custom() {
		U len = count;
		if (len == 0) {
			return;
		}

		SortArray<T, C> sorter;
		sorter.sort(data, len);
	}

	void sort() {
		sort_custom<_DefaultComparator<T>>();
	}

	void ordered_insert(T p_val) {
		U i;
		for (i = 0; i < count; i++) {
			if (p_val < data[i]) {
				break;
			}
		}
		insert(i, p_val);
	}

	operator Vector<T>() const {
		Vector<T> ret;
		ret.resize(size());
		T *w = ret.ptrw();
		memcpy(w, data, sizeof(T) * count);
		return ret;
	}

	operator PoolVector<T>() const {
		PoolVector<T> pl;
		if (size()) {
			pl.resize(size());
			typename PoolVector<T>::Write w = pl.write();
			T *dest = w.ptr();
			memcpy(dest, data, sizeof(T) * count);
		}
		return pl;
	}

	Vector<uint8_t> to_byte_array() const { //useful to pass stuff to gpu or variant
		Vector<uint8_t> ret;
		ret.resize(count * sizeof(T));
		uint8_t *w = ret.ptrw();
		memcpy(w, data, sizeof(T) * count);
		return ret;
	}

	_FORCE_INLINE_ LocalVector() {}
	_FORCE_INLINE_ LocalVector(const LocalVector &p_from) {
		resize(p_from.size());
		for (U i = 0; i < p_from.count; i++) {
			data[i] = p_from.data[i];
		}
	}
	LocalVector(const Vector<T> &p_from) {
		resize(p_from.size());
		for (U i = 0; i < count; i++) {
			data[i] = p_from[i];
		}
	}
	LocalVector(const PoolVector<T> &p_from) {
		resize(p_from.size());
		typename PoolVector<T>::Read r = p_from.read();
		for (U i = 0; i < count; i++) {
			data[i] = r[i];
		}
	}

	LocalVector(LocalVector &&p_from) {
		data = p_from.data;
		count = p_from.count;
		capacity = p_from.capacity;

		p_from.data = nullptr;
		p_from.count = 0;
		p_from.capacity = 0;
	}

	inline LocalVector &operator=(const LocalVector &p_from) {
		resize(p_from.size());
		for (U i = 0; i < p_from.count; i++) {
			data[i] = p_from.data[i];
		}
		return *this;
	}

	inline void operator=(LocalVector &&p_from) {
		if (unlikely(this == &p_from)) {
			return;
		}
		reset();

		data = p_from.data;
		count = p_from.count;
		capacity = p_from.capacity;

		p_from.data = nullptr;
		p_from.count = 0;
		p_from.capacity = 0;
	}

	inline LocalVector &operator=(const Vector<T> &p_from) {
		resize(p_from.size());
		for (U i = 0; i < count; i++) {
			data[i] = p_from[i];
		}
		return *this;
	}

	inline void operator=(Vector<T> &&p_from) {
		resize(p_from.size());
		for (U i = 0; i < count; i++) {
			data[i] = std::move(p_from[i]);
		}
	}

	inline LocalVector &operator=(const PoolVector<T> &p_from) {
		resize(p_from.size());
		typename PoolVector<T>::Read r = p_from.read();
		for (U i = 0; i < count; i++) {
			data[i] = r[i];
		}
		return *this;
	}

	_FORCE_INLINE_ ~LocalVector() {
		if (data) {
			reset();
		}
	}
};

// Integer default version
template <class T, class I = int32_t, bool force_trivial = false>
class LocalVectori : public LocalVector<T, I, force_trivial> {
};

#endif // LOCAL_VECTOR_H
