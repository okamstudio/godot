/**************************************************************************/
/*  jolt_inline_vector.h                                                  */
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

#ifndef JOLT_INLINE_VECTOR_H
#define JOLT_INLINE_VECTOR_H

#include "core/error/error_macros.h"

#include "Jolt/Jolt.h"

template <typename T, int TInlineCapacity>
class JoltInlineVector {
	alignas(T) uint8_t inline_buffer[sizeof(T) * TInlineCapacity];
	JPH::Array<T> dynamic_buffer;
	T *data = (T *)inline_buffer;
	int count = 0;
	int capacity = TInlineCapacity;

	void _switch_to_dynamic(int p_new_capacity) {
		dynamic_buffer.reserve(p_new_capacity);

		if constexpr (std::is_trivially_copyable_v<T>) {
			memcpy(dynamic_buffer.data(), data, sizeof(T) * count);
		} else {
			for (int i = 0; i < count; ++i) {
				new (dynamic_buffer.data() + i) T(std::move(data[i]));
				data[i].~T();
			}
		}

		data = dynamic_buffer.data();
		capacity = dynamic_buffer.capacity();
	}

public:
	JoltInlineVector() = default;

	JoltInlineVector(const JoltInlineVector &p_other) = delete;
	JoltInlineVector(JoltInlineVector &&p_other) = delete;

	JoltInlineVector &operator=(const JoltInlineVector &p_other) = delete;
	JoltInlineVector &operator=(JoltInlineVector &&p_other) = delete;

	~JoltInlineVector() { clear(); }

	int size() const { return count; }

	void resize(int p_size) {
		ERR_FAIL_COND(p_size < 0);

		if (p_size > capacity) {
			_switch_to_dynamic(p_size);
		}

		if (p_size > count) {
			for (int i = count; i < p_size; ++i) {
				new (data + i) T();
			}
		} else if (p_size < count) {
			for (int i = p_size; i < count; ++i) {
				data[i].~T();
			}
		}

		count = p_size;
	}

	void clear() { resize(0); }

	void insert(int p_index, T p_val) {
		ERR_FAIL_INDEX(p_index, count + 1);

		if (count == capacity) {
			_switch_to_dynamic(capacity + 1);
		}

		for (int i = count; i > p_index; i--) {
			new (data + i) T(std::move(data[i - 1]));
			data[i - 1].~T();
		}

		new (data + p_index) T(std::move(p_val));
		count++;
	}

	void push_back(T p_val) { insert(count, std::move(p_val)); }

	const T &operator[](int p_index) const {
		CRASH_BAD_INDEX(p_index, count);
		return data[p_index];
	}

	T &operator[](int p_index) {
		CRASH_BAD_INDEX(p_index, count);
		return data[p_index];
	}
};

#endif // JOLT_INLINE_VECTOR_H
