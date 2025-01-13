/**************************************************************************/
/*  tuple.h                                                               */
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

#ifndef TUPLE_H
#define TUPLE_H

// Simple recursive Tuple type that has no runtime overhead.
//
// The compile-time recursion works as follows:
// Assume the following: Tuple<int, float> my_tuple(42, 3.14f);
// This expands to a class hierarchy that inherits from the previous step.
// So in this case this leads to:
//  - struct Tuple<int> : Tuple<float>   <--- This contains the int value.
//  - struct Tuple<float>                <--- This contains the float value.
// where each of the classes has a single field of the type for that step in the
// recursion. So: float value;  int value; etc.
//
// This works by splitting up the parameter pack for each step in the recursion minus the first.
// so the the first step creates the "T value" from the first template parameter.
// any further template arguments end up in "Rest", which we then use to instantiate a new
// tuple, but now minus the first argument. To write this all out:
//
// Tuple<int, float>
// step 1: Tuple T = int, Rest = float. Results in a Tuple<int> : Tuple<float>
// step 2: Tuple T = float, no Rest. Results in a Tuple<float>
//
// tuple_get<I> works through a similar recursion, using the inheritance chain to walk to the right node.
// In order to tuple_get<1>(my_tuple), from the example tuple above:
//
// 1. We want tuple_get<1> to return the float, which is one level "up" from Tuple<int> : Tuple<float>,
//    (the real type of the Tuple "root").
// 2. Since index 1 > 0, it casts the tuple to its parent type (Tuple<float>). This works because
//    we cast to Tuple<Rest...> which in this case is just float.
// 3. Now we're looking for index 0 in Tuple<float>, which directly returns its value field. Note
//    how get<0> is a template specialization.
//
// At compile time, this gets fully resolved. The compiler sees get<1>(my_tuple) and:
// 1. Creates TupleGet<1, Tuple<int, float>>::tuple_get which contains the cast to Tuple<float>.
// 2. Creates TupleGet<0, Tuple<float>>::tuple_get which directly returns the value.
// 3. The compiler will then simply optimize all of this nonsense away and return the float directly.

#include "core/typedefs.h"

template <typename... Types>
struct Tuple;

template <>
struct Tuple<> {};

template <typename T, typename... Rest>
struct Tuple<T, Rest...> : Tuple<Rest...> {
	T value;

	Tuple() = default;

	template <typename F, typename... R>
	_FORCE_INLINE_ Tuple(F &&f, R &&...rest) :
			Tuple<Rest...>(std::forward<R>(rest)...),
			value(std::forward<F>(f)) {}
};

template <size_t I, typename Tuple>
struct TupleGet;

template <typename First, typename... Rest>
struct TupleGet<0, Tuple<First, Rest...>> {
	_FORCE_INLINE_ static First &tuple_get(Tuple<First, Rest...> &t) {
		return t.value;
	}
};

// Rationale for using auto here is that the alternative is writing a
// helper struct to create an otherwise useless type. we would have to write
// a second recursive template chain like: TupleGetType<I, Tuple<First, Rest...>>::type
// just to recover the type in the most baroque way possible.

template <size_t I, typename First, typename... Rest>
struct TupleGet<I, Tuple<First, Rest...>> {
	_FORCE_INLINE_ static auto &tuple_get(Tuple<First, Rest...> &t) {
		return TupleGet<I - 1, Tuple<Rest...>>::tuple_get(static_cast<Tuple<Rest...> &>(t));
	}
};

template <size_t I, typename... Types>
_FORCE_INLINE_ auto &tuple_get(Tuple<Types...> &t) {
	return TupleGet<I, Tuple<Types...>>::tuple_get(t);
}

template <size_t I, typename... Types>
_FORCE_INLINE_ const auto &tuple_get(const Tuple<Types...> &t) {
	return TupleGet<I, Tuple<Types...>>::tuple_get(t);
}

#endif // TUPLE_H
