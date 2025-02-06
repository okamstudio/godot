/**************************************************************************/
/*  container_converters.h                                                */
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

#ifndef CONTAINER_CONVERTERS_H
#define CONTAINER_CONVERTERS_H

#include "core/variant/typed_array.h"

namespace converters {

template <typename R, typename T>
Vector<R> vector_to_vector(const Vector<T> &p_vector) {
	Vector<R> ret;
	ret.resize(p_vector.size());

	R *write = ret.ptrw();
	for (const T &E : p_vector) {
		*write = E;
		write++;
	}
	return ret;
}

template <typename T>
TypedArray<T> vector_to_typed_array(const Vector<T> &p_vector) {
	TypedArray<T> ret;
	ret.resize(p_vector.size());

	Array::Iterator itr = ret.begin();
	for (const T &E : p_vector) {
		*itr = E;
		++itr;
	}
	return ret;
}

template <typename T>
List<T> vector_to_list(const Vector<T> &p_vector) {
	List<T> ret;

	for (const T &E : p_vector) {
		ret.push_back(E);
	}
	return ret;
}

template <typename T>
Vector<T> list_to_vector(const List<T> &p_list) {
	Vector<T> ret;
	// FIXME: resize() can cause unnecessary initialization of elements if they are not trivially destructible.
	// Something like LocalVector's reserve() would be better, but Vector does not support it yet.
	ret.resize(p_list.size());

	T *write = ret.ptrw();
	for (const T &E : p_list) {
		*write = E;
		write++;
	}
	return ret;
}

template <typename R, typename T>
TypedArray<R> list_to_typed_array(const List<T> &p_list) {
	TypedArray<R> ret;
	ret.resize(p_list.size());

	Array::Iterator itr = ret.begin();
	for (const T &E : p_list) {
		const R value = E; // *itr uses Variant, so this allows better conversion.
		*itr = value;
		++itr;
	}
	return ret;
}

} //namespace converters

#endif // CONTAINER_CONVERTERS_H
