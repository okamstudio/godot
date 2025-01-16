/**************************************************************************/
/*  test_span.h                                                           */
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

#ifndef TEST_SPAN_H
#define TEST_SPAN_H

#include "core/templates/span.h"

#include "tests/test_macros.h"

namespace TestSpan {

TEST_CASE("[Span] Constexpr Validators") {
	constexpr Span<uint16_t> span_empty;
	static_assert(span_empty.ptr() == nullptr);
	static_assert(span_empty.size() == 0);
	static_assert(span_empty.is_empty());

	constexpr static uint16_t value = 5;
	constexpr Span<uint16_t> span_value(&value, 1);
	static_assert(span_value.ptr() == &value);
	static_assert(span_value.size() == 1);
	static_assert(!span_value.is_empty());

	constexpr static char32_t array[] = U"122345";
	constexpr Span<char32_t> span_array(array, strlen(array));
	static_assert(span_array.ptr() == &array[0]);
	static_assert(span_array.size() == 6);
	static_assert(!span_array.is_empty());
	static_assert(span_array[0] == U'1');
	static_assert(span_array[span_array.size() - 1] == U'5');
}

} // namespace TestSpan

#endif // TEST_SPAN_H
