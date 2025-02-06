/**************************************************************************/
/*  heap.h                                                                */
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

#ifndef HEAP_H
#define HEAP_H

#include "core/templates/local_vector.h"

template <typename T>
struct NoopIndexer {
	void operator()(const T &p_value, uint32_t p_index) {}
};

/**
 * A max-heap implementation that notifies of element index changes.
 */
template <typename T, typename LessThan = Comparator<T>, typename Indexer = NoopIndexer<T>>
class Heap {
	LocalVector<T> _buffer;

	LessThan _less_than;
	Indexer _indexer;

public:
	static constexpr uint32_t INVALID_INDEX = UINT32_MAX;
	void reserve(uint32_t p_size) {
		_buffer.reserve(p_size);
	}

	uint32_t size() const {
		return _buffer.size();
	}

	bool is_empty() const {
		return _buffer.is_empty();
	}

	void push(const T &p_element) {
		_buffer.push_back(p_element);
		_indexer(p_element, _buffer.size() - 1);
		_shift_up(_buffer.size() - 1);
	}

	T pop() {
		ERR_FAIL_COND_V_MSG(_buffer.is_empty(), T(), "Can't pop an empty heap.");
		T value = _buffer[0];
		_indexer(value, INVALID_INDEX);
		if (_buffer.size() > 1) {
			_buffer[0] = _buffer[_buffer.size() - 1];
			_indexer(_buffer[0], 0);
			_buffer.remove_at(_buffer.size() - 1);
			_shift_down(0);
		} else {
			_buffer.remove_at(_buffer.size() - 1);
		}
		return value;
	}

	/**
	 * Update the position of the element in the heap if necessary.
	 */
	void shift(uint32_t p_index) {
		ERR_FAIL_UNSIGNED_INDEX_MSG(p_index, _buffer.size(), "Heap element index is out of range.");
		if (!_shift_up(p_index)) {
			_shift_down(p_index);
		}
	}

	void clear() {
		for (const T &value : _buffer) {
			_indexer(value, INVALID_INDEX);
		}
		_buffer.clear();
	}

	Heap() {}

	Heap(const LessThan &p_less_than) :
			_less_than(p_less_than) {}

	Heap(const Indexer &p_indexer) :
			_indexer(p_indexer) {}

	Heap(const LessThan &p_less_than, const Indexer &p_indexer) :
			_less_than(p_less_than), _indexer(p_indexer) {}

private:
	bool _shift_up(uint32_t p_index) {
		T value = _buffer[p_index];
		uint32_t current_index = p_index;
		uint32_t parent_index = (current_index - 1) / 2;
		while (current_index > 0 && _less_than(_buffer[parent_index], value)) {
			_buffer[current_index] = _buffer[parent_index];
			_indexer(_buffer[current_index], current_index);
			current_index = parent_index;
			parent_index = (current_index - 1) / 2;
		}
		if (current_index != p_index) {
			_buffer[current_index] = value;
			_indexer(value, current_index);
			return true;
		} else {
			return false;
		}
	}

	bool _shift_down(uint32_t p_index) {
		T value = _buffer[p_index];
		uint32_t current_index = p_index;
		uint32_t child_index = 2 * current_index + 1;
		while (child_index < _buffer.size()) {
			if (child_index + 1 < _buffer.size() &&
					_less_than(_buffer[child_index], _buffer[child_index + 1])) {
				child_index++;
			}
			if (_less_than(_buffer[child_index], value)) {
				break;
			}
			_buffer[current_index] = _buffer[child_index];
			_indexer(_buffer[current_index], current_index);
			current_index = child_index;
			child_index = 2 * current_index + 1;
		}
		if (current_index != p_index) {
			_buffer[current_index] = value;
			_indexer(value, current_index);
			return true;
		} else {
			return false;
		}
	}
};

#endif // HEAP_H
