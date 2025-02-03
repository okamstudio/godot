/**************************************************************************/
/*  rasterizer_array.h                                                    */
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

#ifndef RASTERIZER_ARRAY_H
#define RASTERIZER_ARRAY_H

/**
 * Fast single-threaded growable array for POD types.
 * For use in render drivers, not for general use.
 * TO BE REPLACED by local_vector.
 */

#include "core/os/memory.h"
#include "core/vector.h"

#include <string.h>

// very simple non-growable array, that keeps track of the size of a 'unit'
// which can be cast to whatever vertex format FVF required, and is initially
// created with enough memory to hold the biggest FVF.
// This allows multiple FVFs to use the same array.
class RasterizerUnitArrayGLES2 {
public:
	RasterizerUnitArrayGLES2() {
		_list = nullptr;
		free();
	}
	~RasterizerUnitArrayGLES2() { free(); }

	uint8_t *get_unit(unsigned int ui) { return &_list[ui * _unit_size_bytes]; }
	const uint8_t *get_unit(unsigned int ui) const { return &_list[ui * _unit_size_bytes]; }

	int size() const { return _size; }
	int max_size() const { return _max_size; }

	void free() {
		if (_list) {
			memdelete_arr(_list);
			_list = nullptr;
		}
		_size = 0;
		_max_size = 0;
		_max_size_bytes = 0;
		_unit_size_bytes = 0;
	}

	void create(int p_max_size_units, int p_max_unit_size_bytes) {
		free();

		_max_unit_size_bytes = p_max_unit_size_bytes;
		_max_size = p_max_size_units;
		_max_size_bytes = p_max_size_units * p_max_unit_size_bytes;

		if (_max_size_bytes) {
			_list = memnew_arr(uint8_t, _max_size_bytes);
		}
	}

	void prepare(int p_unit_size_bytes) {
		_unit_size_bytes = p_unit_size_bytes;
		_size = 0;
	}

	// several items at a time
	uint8_t *request(int p_num_items = 1) {
		int old_size = _size;
		_size += p_num_items;

		if (_size <= _max_size) {
			return get_unit(old_size);
		}

		// revert
		_size = old_size;
		return nullptr;
	}

private:
	uint8_t *_list;
	int _size; // in units
	int _max_size; // in units
	int _max_size_bytes;
	int _unit_size_bytes;
	int _max_unit_size_bytes;
};

template <class T>
class RasterizerArray {
public:
	RasterizerArray() {
		_list = nullptr;
		_size = 0;
		_max_size = 0;
	}
	~RasterizerArray() { free(); }

	T &operator[](unsigned int ui) { return _list[ui]; }
	const T &operator[](unsigned int ui) const { return _list[ui]; }

	void free() {
		if (_list) {
			memdelete_arr(_list);
			_list = nullptr;
		}
		_size = 0;
		_max_size = 0;
	}

	void create(int p_size) {
		free();
		if (p_size) {
			_list = memnew_arr(T, p_size);
		}
		_size = 0;
		_max_size = p_size;
	}

	void reset() { _size = 0; }

	T *request_with_grow() {
		T *p = request();
		if (!p) {
			grow();
			return request_with_grow();
		}
		return p;
	}

	T *request_with_grow(int p_num_items) {
		T *p = request(p_num_items);
		if (!p) {
			grow();
			return request_with_grow(p_num_items);
		}
		return p;
	}

	// none of that inefficient pass by value stuff here, thanks
	T *request() {
		if (_size < _max_size) {
			return &_list[_size++];
		}
		return nullptr;
	}

	// several items at a time
	T *request(int p_num_items) {
		int old_size = _size;
		_size += p_num_items;

		if (_size <= _max_size) {
			return &_list[old_size];
		}

		// revert
		_size = old_size;
		return nullptr;
	}

	int size() const { return _size; }
	int max_size() const { return _max_size; }
	const T *get_data() const { return _list; }

	bool copy_from(const RasterizerArray<T> &o) {
		// no resizing done here, it should be done manually
		if (o.size() > _max_size) {
			return false;
		}

		// pod types only please!
		memcpy(_list, o.get_data(), o.size() * sizeof(T));
		_size = o.size();
		return true;
	}

	// if you want this to be cheap, call reset before grow,
	// to ensure there is no data to copy
	void grow() {
		unsigned int new_max_size = _max_size * 2;
		if (!new_max_size) {
			new_max_size = 1;
		}

		T *new_list = memnew_arr(T, new_max_size);

		// copy .. pod types only
		if (_list) {
			memcpy(new_list, _list, _size * sizeof(T));
		}

		unsigned int new_size = size();
		free();
		_list = new_list;
		_size = new_size;
		_max_size = new_max_size;
	}

private:
	T *_list;
	int _size;
	int _max_size;
};

template <class T>
class RasterizerArray_non_pod {
public:
	RasterizerArray_non_pod() {
		_size = 0;
	}

	const T &operator[](unsigned int ui) const { return _list[ui]; }

	void create(int p_size) {
		_list.resize(p_size);
		_size = 0;
	}
	void reset() { _size = 0; }

	void push_back(const T &val) {
		while (true) {
			if (_size < max_size()) {
				_list.set(_size, val);
				_size++;
				return;
			}

			grow();
		}
	}

	int size() const { return _size; }
	int max_size() const { return _list.size(); }

private:
	void grow() {
		unsigned int new_max_size = _list.size() * 2;
		if (!new_max_size) {
			new_max_size = 1;
		}
		_list.resize(new_max_size);
	}

	Vector<T> _list;
	int _size;
};

// very simple non-growable array, that keeps track of the size of a 'unit'
// which can be cast to whatever vertex format FVF required, and is initially
// created with enough memory to hold the biggest FVF.
// This allows multiple FVFs to use the same array.
class RasterizerUnitArray {
public:
	RasterizerUnitArray() {
		_list = nullptr;
		free();
	}
	~RasterizerUnitArray() { free(); }

	uint8_t *get_unit(unsigned int ui) { return &_list[ui * _unit_size_bytes]; }
	const uint8_t *get_unit(unsigned int ui) const { return &_list[ui * _unit_size_bytes]; }

	int size() const { return _size; }
	int max_size() const { return _max_size; }
	int get_unit_size_bytes() const { return _unit_size_bytes; }

	void free() {
		if (_list) {
			memdelete_arr(_list);
			_list = nullptr;
		}
		_size = 0;
		_max_size = 0;
		_max_size_bytes = 0;
		_unit_size_bytes = 0;
	}

	void create(int p_max_size_units, int p_max_unit_size_bytes) {
		free();

		_max_unit_size_bytes = p_max_unit_size_bytes;
		_max_size = p_max_size_units;
		_max_size_bytes = p_max_size_units * p_max_unit_size_bytes;

		if (_max_size_bytes) {
			_list = memnew_arr(uint8_t, _max_size_bytes);
		}
	}

	void prepare(int p_unit_size_bytes) {
		_unit_size_bytes = p_unit_size_bytes;
		_size = 0;
	}

	// several items at a time
	uint8_t *request(int p_num_items = 1) {
		int old_size = _size;
		_size += p_num_items;

		if (_size <= _max_size) {
			return get_unit(old_size);
		}

		// revert
		_size = old_size;
		return nullptr;
	}

private:
	uint8_t *_list;
	int _size; // in units
	int _max_size; // in units
	int _max_size_bytes;
	int _unit_size_bytes;
	int _max_unit_size_bytes;
};

#endif // RASTERIZER_ARRAY_H
