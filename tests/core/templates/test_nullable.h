/**************************************************************************/
/*  test_nullable.h                                                       */
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

#ifndef TEST_NULLABLE_H
#define TEST_NULLABLE_H

#include "core/templates/nullable.h"

#include "tests/test_macros.h"

namespace TestNullable {

TEST_CASE("[Nullable] Initialize") {
	Nullable<int> nullable;
	CHECK(nullable.is_null());
	CHECK(nullable == nullptr);

	nullable = 0;
	CHECK(nullable.has_value());
	CHECK(nullable == 0);

	nullable = nullptr;
	CHECK(nullable.is_null());
	CHECK(nullable == nullptr);
}

TEST_CASE("[Nullable] Coalesse") {
	Nullable<int> nullable;
	CHECK(nullable.is_null());
	CHECK(nullable == nullptr);

	nullable <<= nullptr;
	CHECK(nullable.is_null());
	CHECK(nullable == nullptr);

	nullable <<= 123;
	CHECK(nullable.has_value());
	CHECK(nullable == 123);

	nullable <<= nullptr;
	CHECK(nullable.has_value());
	CHECK(nullable == 123);

	nullable <<= 999;
	CHECK(nullable.has_value());
	CHECK(nullable == 123);
}

TEST_CASE("[Nullable] Equality operators") {
	Nullable<int> nullable = 123;
	CHECK(nullable == 123.0);
	CHECK_FALSE(nullable == nullptr);

	Nullable<Vector2> nullable_v2 = Vector2(1, 1);
	CHECK(nullable_v2 == Vector2i(1, 1));
	CHECK_FALSE(nullable_v2 == nullptr);

	Nullable<Vector3> nullable_v3 = Vector3(1, 1, 1);
	CHECK(nullable_v3 == Vector3i(1, 1, 1));
	CHECK_FALSE(nullable_v3 == nullptr);
}

} // namespace TestNullable

#endif // TEST_NULLABLE_H
