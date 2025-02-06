/**************************************************************************/
/*  test_vector2i.h                                                       */
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

#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "tests/test_macros.h"

namespace TestVector2i {

TEST_CASE("[Vector2i] Constructor methods") {
	const Vector2i vector_empty = Vector2i();
	const Vector2i vector_zero = Vector2i(0, 0);
	CHECK_MESSAGE(
			vector_empty == vector_zero,
			"Vector2i Constructor with no inputs should return a zero Vector2i.");
}

TEST_CASE("[Vector2i] Axis methods") {
	Vector2i vector = Vector2i(2, 3);
	CHECK_MESSAGE(
			vector.max_axis_index() == Vector2i::Axis::AXIS_Y,
			"Vector2i max_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector.min_axis_index() == Vector2i::Axis::AXIS_X,
			"Vector2i min_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector[vector.min_axis_index()] == 2,
			"Vector2i array operator should work as expected.");
	vector[Vector2i::Axis::AXIS_Y] = 5;
	CHECK_MESSAGE(
			vector[Vector2i::Axis::AXIS_Y] == 5,
			"Vector2i array operator setter should work as expected.");
}

TEST_CASE("[Vector2i] Clamp method") {
	const Vector2i vector = Vector2i(10, 10);
	CHECK_MESSAGE(
			Vector2i(-5, 15).clamp(Vector2i(), vector) == Vector2i(0, 10),
			"Vector2i clamp should work as expected.");
	CHECK_MESSAGE(
			vector.clamp(Vector2i(0, 15), Vector2i(5, 20)) == Vector2i(5, 15),
			"Vector2i clamp should work as expected.");
}

TEST_CASE("[Vector2i] Length methods") {
	const Vector2i vector1 = Vector2i(10, 10);
	const Vector2i vector2 = Vector2i(20, 30);
	CHECK_MESSAGE(
			vector1.length_squared() == 200,
			"Vector2i length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.length() == doctest::Approx(10 * Math_SQRT2),
			"Vector2i length should work as expected.");
	CHECK_MESSAGE(
			vector2.length_squared() == 1300,
			"Vector2i length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector2.length() == doctest::Approx(36.05551275463989293119),
			"Vector2i length should work as expected.");
	CHECK_MESSAGE(
			vector1.distance_squared_to(vector2) == 500,
			"Vector2i distance_squared_to should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.distance_to(vector2) == doctest::Approx(22.36067977499789696409),
			"Vector2i distance_to should work as expected.");
}

TEST_CASE("[Vector2i] Operators") {
	const Vector2i vector1 = Vector2i(5, 9);
	const Vector2i vector2 = Vector2i(2, 3);

	CHECK_MESSAGE(
			(vector1 + vector2) == Vector2i(7, 12),
			"Vector2i addition with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 - vector2) == Vector2i(3, 6),
			"Vector2i subtraction with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 * vector2) == Vector2i(10, 27),
			"Vector2i multiplication with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 / vector2) == Vector2i(2, 3),
			"Vector2i division with integers should give exact results.");

	CHECK_MESSAGE(
			(vector1 * 2) == Vector2i(10, 18),
			"Vector2i multiplication with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 / 2) == Vector2i(2, 4),
			"Vector2i division with integers should give exact results.");

	CHECK_MESSAGE(
			((Vector2)vector1) == Vector2(5, 9),
			"Vector2i cast to Vector2 should work as expected.");
	CHECK_MESSAGE(
			((Vector2)vector2) == Vector2(2, 3),
			"Vector2i cast to Vector2 should work as expected.");
	CHECK_MESSAGE(
			Vector2i(Vector2(1.1, 2.9)) == Vector2i(1, 2),
			"Vector2i constructed from Vector2 should work as expected.");
}

TEST_CASE("[Vector2i] Other methods") {
	const Vector2i vector = Vector2i(1, 3);
	CHECK_MESSAGE(
			vector.aspect() == doctest::Approx((real_t)1.0 / (real_t)3.0),
			"Vector2i aspect should work as expected.");

	CHECK_MESSAGE(
			vector.min(Vector2i(3, 2)) == Vector2i(1, 2),
			"Vector2i min should return expected value.");

	CHECK_MESSAGE(
			vector.max(Vector2i(5, 2)) == Vector2i(5, 3),
			"Vector2i max should return expected value.");

	CHECK_MESSAGE(
			vector.snapped(Vector2i(4, 2)) == Vector2i(0, 4),
			"Vector2i snapped should work as expected.");
}

TEST_CASE("[Vector2i] Abs and sign methods") {
	const Vector2i vector1 = Vector2i(1, 3);
	const Vector2i vector2 = Vector2i(1, -3);
	CHECK_MESSAGE(
			vector1.abs() == vector1,
			"Vector2i abs should work as expected.");
	CHECK_MESSAGE(
			vector2.abs() == vector1,
			"Vector2i abs should work as expected.");

	CHECK_MESSAGE(
			vector1.sign() == Vector2i(1, 1),
			"Vector2i sign should work as expected.");
	CHECK_MESSAGE(
			vector2.sign() == Vector2i(1, -1),
			"Vector2i sign should work as expected.");
}
} // namespace TestVector2i
