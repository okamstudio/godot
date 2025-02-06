/**************************************************************************/
/*  vector_iterator.h                                                     */
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

#ifndef VECTOR_ITERATOR_H
#define VECTOR_ITERATOR_H

#include <cstddef>

/**
 * @class VectorIterator
 * Iterator for Vector and LocalVector.
 */

template <class T>
struct ConstVectorIterator {
	using value_type = T;
	using difference_type = ptrdiff_t;
	using pointer = const value_type *;
	using reference = const value_type &;

	reference operator*() const { return *elem_ptr; }
	pointer operator->() const { return elem_ptr; }

	ConstVectorIterator &operator++() {
		++elem_ptr;
		return *this;
	}
	ConstVectorIterator operator++(int) {
		ConstVectorIterator tmp = *this;
		++*this;
		return tmp;
	}

	ConstVectorIterator &operator--() {
		--elem_ptr;
		return *this;
	}
	ConstVectorIterator operator--(int) {
		ConstVectorIterator tmp = *this;
		--*this;
		return tmp;
	}

	bool operator==(const ConstVectorIterator &p_it) const { return elem_ptr == p_it.elem_ptr; }
	bool operator!=(const ConstVectorIterator &p_it) const { return elem_ptr != p_it.elem_ptr; }

	bool operator<(const ConstVectorIterator &p_it) const { return this->elem_ptr < p_it.elem_ptr; }
	bool operator>(const ConstVectorIterator &p_it) const { return this->elem_ptr > p_it.elem_ptr; }
	bool operator>=(const ConstVectorIterator &p_it) const { return !(this->elem_ptr < p_it.elem_ptr); }
	bool operator<=(const ConstVectorIterator &p_it) const { return !(this->elem_ptr > p_it.elem_ptr); }

	difference_type operator-(const ConstVectorIterator &p_it) const { return elem_ptr - p_it.elem_ptr; }

	ConstVectorIterator operator+(const difference_type &p_diff) const { return ConstVectorIterator(elem_ptr + p_diff); }
	ConstVectorIterator operator-(const difference_type &p_diff) const { return ConstVectorIterator(elem_ptr - p_diff); }

	reference operator[](const difference_type &p_offset) const { return *(*this + p_offset); }

	ConstVectorIterator(const T *p_ptr) :
			elem_ptr{ const_cast<T *>(p_ptr) } {}
	ConstVectorIterator(T *p_ptr) :
			elem_ptr{ p_ptr } {}
	ConstVectorIterator() {}
	ConstVectorIterator(const ConstVectorIterator &p_it) = default;

protected:
	T *elem_ptr = nullptr;
};

template <class T>
struct VectorIterator : ConstVectorIterator<T> {
	using _base = ConstVectorIterator<T>;
	using _base::_base;
	using _base::elem_ptr;

	using value_type = T;
	using difference_type = ptrdiff_t;
	using pointer = value_type *;
	using reference = value_type &;

	reference operator*() const { return *elem_ptr; }
	pointer operator->() const { return elem_ptr; }

	VectorIterator &operator++() {
		++elem_ptr;
		return *this;
	}
	VectorIterator operator++(int) {
		VectorIterator tmp = *this;
		++*this;
		return tmp;
	}

	VectorIterator &operator--() {
		--elem_ptr;
		return *this;
	}
	VectorIterator operator--(int) {
		VectorIterator tmp = *this;
		--*this;
		return tmp;
	}

	bool operator==(const VectorIterator &p_it) const { return elem_ptr == p_it.elem_ptr; }
	bool operator!=(const VectorIterator &p_it) const { return elem_ptr != p_it.elem_ptr; }

	bool operator<(const VectorIterator &p_it) const { return this->elem_ptr < p_it.elem_ptr; }
	bool operator>(const VectorIterator &p_it) const { return this->elem_ptr > p_it.elem_ptr; }
	bool operator>=(const VectorIterator &p_it) const { return !(this->elem_ptr < p_it.elem_ptr); }
	bool operator<=(const VectorIterator &p_it) const { return !(this->elem_ptr > p_it.elem_ptr); }

	difference_type operator-(const VectorIterator &p_it) const { return elem_ptr - p_it.elem_ptr; }

	VectorIterator operator+(const difference_type &p_diff) const { return VectorIterator(elem_ptr + p_diff); }
	VectorIterator operator-(const difference_type &p_diff) const { return VectorIterator(elem_ptr - p_diff); }

	reference operator[](const difference_type &p_offset) const { return *(*this + p_offset); }

	VectorIterator(const T *p_ptr) = delete;
};

#endif // VECTOR_ITERATOR_H
