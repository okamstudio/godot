/**************************************************************************/
/*  test_vector4i.h                                                       */
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

#include "core/math/vector4i.h"
#include "tests/test_macros.h"

namespace TestVector4i {

TEST_CASE("[Vector4i] Constructor methods") {
	const Vector4i vector_empty = Vector4i();
	const Vector4i vector_zero = Vector4i(0, 0, 0, 0);
	CHECK_MESSAGE(
			vector_empty == vector_zero,
			"Vector4i Constructor with no inputs should return a zero Vector4i.");
}

TEST_CASE("[Vector4i] Axis methods") {
	Vector4i vector = Vector4i(1, 2, 3, 4);
	CHECK_MESSAGE(
			vector.max_axis_index() == Vector4i::Axis::AXIS_W,
			"Vector4i max_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector.min_axis_index() == Vector4i::Axis::AXIS_X,
			"Vector4i min_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector[vector.max_axis_index()] == 4,
			"Vector4i array operator should work as expected.");
	CHECK_MESSAGE(
			vector[vector.min_axis_index()] == 1,
			"Vector4i array operator should work as expected.");

	vector[Vector4i::Axis::AXIS_Y] = 5;
	CHECK_MESSAGE(
			vector[Vector4i::Axis::AXIS_Y] == 5,
			"Vector4i array operator setter should work as expected.");
}

TEST_CASE("[Vector4i] Clamp method") {
	const Vector4i vector = Vector4i(10, 10, 10, 10);
	CHECK_MESSAGE(
			Vector4i(-5, 5, 15, INT_MAX).clamp(Vector4i(), vector) == Vector4i(0, 5, 10, 10),
			"Vector4i clamp should work as expected.");
	CHECK_MESSAGE(
			vector.clamp(Vector4i(0, 10, 15, -10), Vector4i(5, 10, 20, -5)) == Vector4i(5, 10, 15, -5),
			"Vector4i clamp should work as expected.");
}

TEST_CASE("[Vector4i] Length methods") {
	const Vector4i vector1 = Vector4i(10, 10, 10, 10);
	const Vector4i vector2 = Vector4i(20, 30, 40, 50);
	CHECK_MESSAGE(
			vector1.length_squared() == 400,
			"Vector4i length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.length() == doctest::Approx(20),
			"Vector4i length should work as expected.");
	CHECK_MESSAGE(
			vector2.length_squared() == 5400,
			"Vector4i length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector2.length() == doctest::Approx(73.4846922835),
			"Vector4i length should work as expected.");
	CHECK_MESSAGE(
			vector1.distance_squared_to(vector2) == 3000,
			"Vector4i distance_squared_to should work as expected.");
	CHECK_MESSAGE(
			vector1.distance_to(vector2) == doctest::Approx(54.772255750517),
			"Vector4i distance_to should work as expected.");
}

TEST_CASE("[Vector4i] Operators") {
	const Vector4i vector1 = Vector4i(4, 5, 9, 2);
	const Vector4i vector2 = Vector4i(1, 2, 3, 4);

	CHECK_MESSAGE(
			-vector1 == Vector4i(-4, -5, -9, -2),
			"Vector4i change of sign should work as expected.");
	CHECK_MESSAGE(
			(vector1 + vector2) == Vector4i(5, 7, 12, 6),
			"Vector4i addition with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 - vector2) == Vector4i(3, 3, 6, -2),
			"Vector4i subtraction with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 * vector2) == Vector4i(4, 10, 27, 8),
			"Vector4i multiplication with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 / vector2) == Vector4i(4, 2, 3, 0),
			"Vector4i division with integers should give exact results.");

	CHECK_MESSAGE(
			(vector1 * 2) == Vector4i(8, 10, 18, 4),
			"Vector4i multiplication with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 / 2) == Vector4i(2, 2, 4, 1),
			"Vector4i division with integers should give exact results.");

	CHECK_MESSAGE(
			((Vector4)vector1) == Vector4(4, 5, 9, 2),
			"Vector4i cast to Vector4 should work as expected.");
	CHECK_MESSAGE(
			((Vector4)vector2) == Vector4(1, 2, 3, 4),
			"Vector4i cast to Vector4 should work as expected.");
	CHECK_MESSAGE(
			Vector4i(Vector4(1.1, 2.9, 3.9, 100.5)) == Vector4i(1, 2, 3, 100),
			"Vector4i constructed from Vector4 should work as expected.");
}

TEST_CASE("[Vector3i] Other methods") {
	const Vector4i vector = Vector4i(1, 3, -7, 13);

	CHECK_MESSAGE(
			vector.min(Vector4i(3, 2, 5, 8)) == Vector4i(1, 2, -7, 8),
			"Vector4i min should return expected value.");

	CHECK_MESSAGE(
			vector.max(Vector4i(5, 2, 4, 8)) == Vector4i(5, 3, 4, 13),
			"Vector4i max should return expected value.");

	CHECK_MESSAGE(
			vector.snapped(Vector4i(4, 2, 5, 8)) == Vector4i(0, 4, -5, 16),
			"Vector4i snapped should work as expected.");
}

TEST_CASE("[Vector4i] Abs and sign methods") {
	const Vector4i vector1 = Vector4i(1, 3, 5, 7);
	const Vector4i vector2 = Vector4i(1, -3, -5, 7);
	CHECK_MESSAGE(
			vector1.abs() == vector1,
			"Vector4i abs should work as expected.");
	CHECK_MESSAGE(
			vector2.abs() == vector1,
			"Vector4i abs should work as expected.");

	CHECK_MESSAGE(
			vector1.sign() == Vector4i(1, 1, 1, 1),
			"Vector4i sign should work as expected.");
	CHECK_MESSAGE(
			vector2.sign() == Vector4i(1, -1, -1, 1),
			"Vector4i sign should work as expected.");
}
} // namespace TestVector4i
