/**************************************************************************/
/*  test_vector3i.h                                                       */
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

#include "core/math/vector3i.h"
#include "tests/test_macros.h"

namespace TestVector3i {

TEST_CASE("[Vector3i] Constructor methods") {
	const Vector3i vector_empty = Vector3i();
	const Vector3i vector_zero = Vector3i(0, 0, 0);
	CHECK_MESSAGE(
			vector_empty == vector_zero,
			"Vector3i Constructor with no inputs should return a zero Vector3i.");
}

TEST_CASE("[Vector3i] Axis methods") {
	Vector3i vector = Vector3i(1, 2, 3);
	CHECK_MESSAGE(
			vector.max_axis_index() == Vector3i::Axis::AXIS_Z,
			"Vector3i max_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector.min_axis_index() == Vector3i::Axis::AXIS_X,
			"Vector3i min_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector[vector.max_axis_index()] == 3,
			"Vector3i array operator should work as expected.");
	CHECK_MESSAGE(
			vector[vector.min_axis_index()] == 1,
			"Vector3i array operator should work as expected.");

	vector[Vector3i::Axis::AXIS_Y] = 5;
	CHECK_MESSAGE(
			vector[Vector3i::Axis::AXIS_Y] == 5,
			"Vector3i array operator setter should work as expected.");
}

TEST_CASE("[Vector3i] Clamp method") {
	const Vector3i vector = Vector3i(10, 10, 10);
	CHECK_MESSAGE(
			Vector3i(-5, 5, 15).clamp(Vector3i(), vector) == Vector3i(0, 5, 10),
			"Vector3i clamp should work as expected.");
	CHECK_MESSAGE(
			vector.clamp(Vector3i(0, 10, 15), Vector3i(5, 10, 20)) == Vector3i(5, 10, 15),
			"Vector3i clamp should work as expected.");
}

TEST_CASE("[Vector3i] Length methods") {
	const Vector3i vector1 = Vector3i(10, 10, 10);
	const Vector3i vector2 = Vector3i(20, 30, 40);
	CHECK_MESSAGE(
			vector1.length_squared() == 300,
			"Vector3i length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.length() == doctest::Approx(10 * Math_SQRT3),
			"Vector3i length should work as expected.");
	CHECK_MESSAGE(
			vector2.length_squared() == 2900,
			"Vector3i length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector2.length() == doctest::Approx(53.8516480713450403125),
			"Vector3i length should work as expected.");
	CHECK_MESSAGE(
			vector1.distance_squared_to(vector2) == 1400,
			"Vector3i distance_squared_to should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.distance_to(vector2) == doctest::Approx(37.41657386773941385584),
			"Vector3i distance_to should work as expected.");
}

TEST_CASE("[Vector3i] Operators") {
	const Vector3i vector1 = Vector3i(4, 5, 9);
	const Vector3i vector2 = Vector3i(1, 2, 3);

	CHECK_MESSAGE(
			(vector1 + vector2) == Vector3i(5, 7, 12),
			"Vector3i addition with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 - vector2) == Vector3i(3, 3, 6),
			"Vector3i subtraction with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 * vector2) == Vector3i(4, 10, 27),
			"Vector3i multiplication with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 / vector2) == Vector3i(4, 2, 3),
			"Vector3i division with integers should give exact results.");

	CHECK_MESSAGE(
			(vector1 * 2) == Vector3i(8, 10, 18),
			"Vector3i multiplication with integers should give exact results.");
	CHECK_MESSAGE(
			(vector1 / 2) == Vector3i(2, 2, 4),
			"Vector3i division with integers should give exact results.");

	CHECK_MESSAGE(
			((Vector3)vector1) == Vector3(4, 5, 9),
			"Vector3i cast to Vector3 should work as expected.");
	CHECK_MESSAGE(
			((Vector3)vector2) == Vector3(1, 2, 3),
			"Vector3i cast to Vector3 should work as expected.");
	CHECK_MESSAGE(
			Vector3i(Vector3(1.1, 2.9, 3.9)) == Vector3i(1, 2, 3),
			"Vector3i constructed from Vector3 should work as expected.");
}

TEST_CASE("[Vector3i] Other methods") {
	const Vector3i vector = Vector3i(1, 3, -7);

	CHECK_MESSAGE(
			vector.min(Vector3i(3, 2, 5)) == Vector3i(1, 2, -7),
			"Vector3i min should return expected value.");
	CHECK_MESSAGE(
			vector.max(Vector3i(5, 2, 4)) == Vector3i(5, 3, 4),
			"Vector3i max should return expected value.");

	CHECK_MESSAGE(
			vector.snapped(Vector3i(4, 2, 5)) == Vector3i(0, 4, -5),
			"Vector3i snapped should work as expected.");
}

TEST_CASE("[Vector3i] Abs and sign methods") {
	const Vector3i vector1 = Vector3i(1, 3, 5);
	const Vector3i vector2 = Vector3i(1, -3, -5);
	CHECK_MESSAGE(
			vector1.abs() == vector1,
			"Vector3i abs should work as expected.");
	CHECK_MESSAGE(
			vector2.abs() == vector1,
			"Vector3i abs should work as expected.");

	CHECK_MESSAGE(
			vector1.sign() == Vector3i(1, 1, 1),
			"Vector3i sign should work as expected.");
	CHECK_MESSAGE(
			vector2.sign() == Vector3i(1, -1, -1),
			"Vector3i sign should work as expected.");
}
} // namespace TestVector3i
