/**************************************************************************/
/*  test_vector4.h                                                        */
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

#include "core/math/vector4.h"
#include "tests/test_macros.h"

#define Math_SQRT3 1.7320508075688772935274463415059

namespace TestVector4 {

TEST_CASE("[Vector4] Constructor methods") {
	const Vector4 vector_empty = Vector4();
	const Vector4 vector_zero = Vector4(0.0, 0.0, 0.0, 0.0);
	CHECK_MESSAGE(
			vector_empty == vector_zero,
			"Vector4 Constructor with no inputs should return a zero Vector4.");
}

TEST_CASE("[Vector4] Axis methods") {
	Vector4 vector = Vector4(1.2, 3.4, 5.6, -0.9);
	CHECK_MESSAGE(
			vector.max_axis_index() == Vector4::Axis::AXIS_Z,
			"Vector4 max_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector.min_axis_index() == Vector4::Axis::AXIS_W,
			"Vector4 min_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector[vector.max_axis_index()] == (real_t)5.6,
			"Vector4 array operator should work as expected.");
	CHECK_MESSAGE(
			vector[vector.min_axis_index()] == (real_t)-0.9,
			"Vector4 array operator should work as expected.");

	vector[Vector4::Axis::AXIS_Y] = 3.7;
	CHECK_MESSAGE(
			vector[Vector4::Axis::AXIS_Y] == (real_t)3.7,
			"Vector4 array operator setter should work as expected.");
}

TEST_CASE("[Vector4] Interpolation methods") {
	const Vector4 vector1 = Vector4(1, 2, 3, 4);
	const Vector4 vector2 = Vector4(4, 5, 6, 7);
	CHECK_MESSAGE(
			vector1.lerp(vector2, 0.5) == Vector4(2.5, 3.5, 4.5, 5.5),
			"Vector4 lerp should work as expected.");
	CHECK_MESSAGE(
			vector1.lerp(vector2, 1.0 / 3.0).is_equal_approx(Vector4(2, 3, 4, 5)),
			"Vector4 lerp should work as expected.");
	CHECK_MESSAGE(
			vector1.cubic_interpolate(vector2, Vector4(), Vector4(7, 7, 7, 7), 0.5) == Vector4(2.375, 3.5, 4.625, 5.75),
			"Vector4 cubic_interpolate should work as expected.");
	CHECK_MESSAGE(
			vector1.cubic_interpolate(vector2, Vector4(), Vector4(7, 7, 7, 7), 1.0 / 3.0).is_equal_approx(Vector4(1.851851940155029297, 2.962963104248046875, 4.074074268341064453, 5.185185185185)),
			"Vector4 cubic_interpolate should work as expected.");
}

TEST_CASE("[Vector4] Length methods") {
	const Vector4 vector1 = Vector4(10, 10, 10, 10);
	const Vector4 vector2 = Vector4(20, 30, 40, 50);
	CHECK_MESSAGE(
			vector1.length_squared() == 400,
			"Vector4 length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.length() == doctest::Approx(20),
			"Vector4 length should work as expected.");
	CHECK_MESSAGE(
			vector2.length_squared() == 5400,
			"Vector4 length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector2.length() == doctest::Approx((real_t)73.484692283495),
			"Vector4 length should work as expected.");
	CHECK_MESSAGE(
			vector1.distance_to(vector2) == doctest::Approx((real_t)54.772255750517),
			"Vector4 distance_to should work as expected.");
	CHECK_MESSAGE(
			vector1.distance_squared_to(vector2) == doctest::Approx(3000),
			"Vector4 distance_squared_to should work as expected.");
}

TEST_CASE("[Vector4] Limiting methods") {
	const Vector4 vector = Vector4(10, 10, 10, 10);
	CHECK_MESSAGE(
			Vector4(-5, 5, 15, -15).clamp(Vector4(), vector) == Vector4(0, 5, 10, 0),
			"Vector4 clamp should work as expected.");
	CHECK_MESSAGE(
			vector.clamp(Vector4(0, 10, 15, 18), Vector4(5, 10, 20, 25)) == Vector4(5, 10, 15, 18),
			"Vector4 clamp should work as expected.");
}

TEST_CASE("[Vector4] Normalization methods") {
	CHECK_MESSAGE(
			Vector4(1, 0, 0, 0).is_normalized() == true,
			"Vector4 is_normalized should return true for a normalized vector.");
	CHECK_MESSAGE(
			Vector4(1, 1, 1, 1).is_normalized() == false,
			"Vector4 is_normalized should return false for a non-normalized vector.");
	CHECK_MESSAGE(
			Vector4(1, 0, 0, 0).normalized() == Vector4(1, 0, 0, 0),
			"Vector4 normalized should return the same vector for a normalized vector.");
	CHECK_MESSAGE(
			Vector4(1, 1, 0, 0).normalized().is_equal_approx(Vector4(Math_SQRT12, Math_SQRT12, 0, 0)),
			"Vector4 normalized should work as expected.");
	CHECK_MESSAGE(
			Vector4(1, 1, 1, 1).normalized().is_equal_approx(Vector4(0.5, 0.5, 0.5, 0.5)),
			"Vector4 normalized should work as expected.");
}

TEST_CASE("[Vector4] Operators") {
	const Vector4 decimal1 = Vector4(2.3, 4.9, 7.8, 3.2);
	const Vector4 decimal2 = Vector4(1.2, 3.4, 5.6, 1.7);
	const Vector4 power1 = Vector4(0.75, 1.5, 0.625, 0.125);
	const Vector4 power2 = Vector4(0.5, 0.125, 0.25, 0.75);
	const Vector4 int1 = Vector4(4, 5, 9, 2);
	const Vector4 int2 = Vector4(1, 2, 3, 1);

	CHECK_MESSAGE(
			-decimal1 == Vector4(-2.3, -4.9, -7.8, -3.2),
			"Vector4 change of sign should work as expected.");
	CHECK_MESSAGE(
			(decimal1 + decimal2).is_equal_approx(Vector4(3.5, 8.3, 13.4, 4.9)),
			"Vector4 addition should behave as expected.");
	CHECK_MESSAGE(
			(power1 + power2) == Vector4(1.25, 1.625, 0.875, 0.875),
			"Vector4 addition with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 + int2) == Vector4(5, 7, 12, 3),
			"Vector4 addition with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 - decimal2).is_equal_approx(Vector4(1.1, 1.5, 2.2, 1.5)),
			"Vector4 subtraction should behave as expected.");
	CHECK_MESSAGE(
			(power1 - power2) == Vector4(0.25, 1.375, 0.375, -0.625),
			"Vector4 subtraction with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 - int2) == Vector4(3, 3, 6, 1),
			"Vector4 subtraction with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 * decimal2).is_equal_approx(Vector4(2.76, 16.66, 43.68, 5.44)),
			"Vector4 multiplication should behave as expected.");
	CHECK_MESSAGE(
			(power1 * power2) == Vector4(0.375, 0.1875, 0.15625, 0.09375),
			"Vector4 multiplication with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 * int2) == Vector4(4, 10, 27, 2),
			"Vector4 multiplication with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 / decimal2).is_equal_approx(Vector4(1.91666666666666666, 1.44117647058823529, 1.39285714285714286, 1.88235294118)),
			"Vector4 division should behave as expected.");
	CHECK_MESSAGE(
			(power1 / power2) == Vector4(1.5, 12.0, 2.5, 1.0 / 6.0),
			"Vector4 division with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 / int2) == Vector4(4, 2.5, 3, 2),
			"Vector4 division with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 * 2).is_equal_approx(Vector4(4.6, 9.8, 15.6, 6.4)),
			"Vector4 multiplication should behave as expected.");
	CHECK_MESSAGE(
			(power1 * 2) == Vector4(1.5, 3, 1.25, 0.25),
			"Vector4 multiplication with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 * 2) == Vector4(8, 10, 18, 4),
			"Vector4 multiplication with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 / 2).is_equal_approx(Vector4(1.15, 2.45, 3.9, 1.6)),
			"Vector4 division should behave as expected.");
	CHECK_MESSAGE(
			(power1 / 2) == Vector4(0.375, 0.75, 0.3125, 0.0625),
			"Vector4 division with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 / 2) == Vector4(2, 2.5, 4.5, 1),
			"Vector4 division with integers should give exact results.");

	CHECK_MESSAGE(
			((String)decimal1) == "(2.3, 4.9, 7.8, 3.2)",
			"Vector4 cast to String should work as expected.");
	CHECK_MESSAGE(
			((String)decimal2) == "(1.2, 3.4, 5.6, 1.7)",
			"Vector4 cast to String should work as expected.");
	CHECK_MESSAGE(
			((String)Vector4(9.7, 9.8, 9.9, -1.8)) == "(9.7, 9.8, 9.9, -1.8)",
			"Vector4 cast to String should work as expected.");
#ifdef REAL_T_IS_DOUBLE
	CHECK_MESSAGE(
			((String)Vector4(Math_E, Math_SQRT2, Math_SQRT3, Math_SQRT3)) == "(2.71828182845905, 1.4142135623731, 1.73205080756888, 1.73205080756888)",
			"Vector4 cast to String should print the correct amount of digits for real_t = double.");
#else
	CHECK_MESSAGE(
			((String)Vector4(Math_E, Math_SQRT2, Math_SQRT3, Math_SQRT3)) == "(2.718282, 1.414214, 1.732051, 1.732051)",
			"Vector4 cast to String should print the correct amount of digits for real_t = float.");
#endif // REAL_T_IS_DOUBLE
}

TEST_CASE("[Vector4] Other methods") {
	const Vector4 vector = Vector4(1.2, 3.4, 5.6, 1.6);
	CHECK_MESSAGE(
			vector.direction_to(Vector4()).is_equal_approx(-vector.normalized()),
			"Vector4 direction_to should work as expected.");
	CHECK_MESSAGE(
			Vector4(1, 1, 1, 1).direction_to(Vector4(2, 2, 2, 2)).is_equal_approx(Vector4(0.5, 0.5, 0.5, 0.5)),
			"Vector4 direction_to should work as expected.");
	CHECK_MESSAGE(
			vector.inverse().is_equal_approx(Vector4(1 / 1.2, 1 / 3.4, 1 / 5.6, 1 / 1.6)),
			"Vector4 inverse should work as expected.");
	CHECK_MESSAGE(
			vector.posmod(2).is_equal_approx(Vector4(1.2, 1.4, 1.6, 1.6)),
			"Vector4 posmod should work as expected.");
	CHECK_MESSAGE(
			(-vector).posmod(2).is_equal_approx(Vector4(0.8, 0.6, 0.4, 0.4)),
			"Vector4 posmod should work as expected.");
	CHECK_MESSAGE(
			vector.posmodv(Vector4(1, 2, 3, 4)).is_equal_approx(Vector4(0.2, 1.4, 2.6, 1.6)),
			"Vector4 posmodv should work as expected.");
	CHECK_MESSAGE(
			(-vector).posmodv(Vector4(2, 3, 4, 5)).is_equal_approx(Vector4(0.8, 2.6, 2.4, 3.4)),
			"Vector4 posmodv should work as expected.");
	CHECK_MESSAGE(
			vector.snapped(Vector4(1, 1, 1, 1)) == Vector4(1, 3, 6, 2),
			"Vector4 snapped to integers should be the same as rounding.");
	CHECK_MESSAGE(
			vector.snapped(Vector4(0.25, 0.25, 0.25, 0.25)) == Vector4(1.25, 3.5, 5.5, 1.5),
			"Vector4 snapped to 0.25 should give exact results.");

	CHECK_MESSAGE(
			Vector4(1.2, 2.5, 2.0, 1.6).is_equal_approx(vector.min(Vector4(3.0, 2.5, 2.0, 3.4))),
			"Vector4 min should return expected value.");

	CHECK_MESSAGE(
			Vector4(5.3, 3.4, 5.6, 4.2).is_equal_approx(vector.max(Vector4(5.3, 2.0, 3.0, 4.2))),
			"Vector4 max should return expected value.");
}

TEST_CASE("[Vector4] Rounding methods") {
	const Vector4 vector1 = Vector4(1.2, 3.4, 5.6, 1.6);
	const Vector4 vector2 = Vector4(1.2, -3.4, -5.6, -1.6);
	CHECK_MESSAGE(
			vector1.abs() == vector1,
			"Vector4 abs should work as expected.");
	CHECK_MESSAGE(
			vector2.abs() == vector1,
			"Vector4 abs should work as expected.");
	CHECK_MESSAGE(
			vector1.ceil() == Vector4(2, 4, 6, 2),
			"Vector4 ceil should work as expected.");
	CHECK_MESSAGE(
			vector2.ceil() == Vector4(2, -3, -5, -1),
			"Vector4 ceil should work as expected.");

	CHECK_MESSAGE(
			vector1.floor() == Vector4(1, 3, 5, 1),
			"Vector4 floor should work as expected.");
	CHECK_MESSAGE(
			vector2.floor() == Vector4(1, -4, -6, -2),
			"Vector4 floor should work as expected.");

	CHECK_MESSAGE(
			vector1.round() == Vector4(1, 3, 6, 2),
			"Vector4 round should work as expected.");
	CHECK_MESSAGE(
			vector2.round() == Vector4(1, -3, -6, -2),
			"Vector4 round should work as expected.");

	CHECK_MESSAGE(
			vector1.sign() == Vector4(1, 1, 1, 1),
			"Vector4 sign should work as expected.");
	CHECK_MESSAGE(
			vector2.sign() == Vector4(1, -1, -1, -1),
			"Vector4 sign should work as expected.");
}

TEST_CASE("[Vector4] Linear algebra methods") {
	const Vector4 vector_x = Vector4(1, 0, 0, 0);
	const Vector4 vector_y = Vector4(0, 1, 0, 0);
	const Vector4 vector1 = Vector4(1.7, 2.3, 1, 9.1);
	const Vector4 vector2 = Vector4(-8.2, -16, 3, 2.4);

	CHECK_MESSAGE(
			vector_x.dot(vector_y) == 0.0,
			"Vector4 dot product of perpendicular vectors should be zero.");
	CHECK_MESSAGE(
			vector_x.dot(vector_x) == 1.0,
			"Vector4 dot product of identical unit vectors should be one.");
	CHECK_MESSAGE(
			(vector_x * 10).dot(vector_x * 10) == 100.0,
			"Vector4 dot product of same direction vectors should behave as expected.");
	CHECK_MESSAGE(
			(vector1 * 2).dot(vector2 * 4) == doctest::Approx((real_t)-25.9 * 8),
			"Vector4 dot product should work as expected.");
}

TEST_CASE("[Vector4] Finite number checks") {
	const double infinite[] = { NAN, INFINITY, -INFINITY };

	CHECK_MESSAGE(
			Vector4(0, 1, 2, 3).is_finite(),
			"Vector4(0, 1, 2, 3) should be finite");

	for (double x : infinite) {
		CHECK_FALSE_MESSAGE(
				Vector4(x, 1, 2, 3).is_finite(),
				"Vector4 with one component infinite should not be finite.");
		CHECK_FALSE_MESSAGE(
				Vector4(0, x, 2, 3).is_finite(),
				"Vector4 with one component infinite should not be finite.");
		CHECK_FALSE_MESSAGE(
				Vector4(0, 1, x, 3).is_finite(),
				"Vector4 with one component infinite should not be finite.");
		CHECK_FALSE_MESSAGE(
				Vector4(0, 1, 2, x).is_finite(),
				"Vector4 with one component infinite should not be finite.");
	}

	for (double x : infinite) {
		for (double y : infinite) {
			CHECK_FALSE_MESSAGE(
					Vector4(x, y, 2, 3).is_finite(),
					"Vector4 with two components infinite should not be finite.");
			CHECK_FALSE_MESSAGE(
					Vector4(x, 1, y, 3).is_finite(),
					"Vector4 with two components infinite should not be finite.");
			CHECK_FALSE_MESSAGE(
					Vector4(x, 1, 2, y).is_finite(),
					"Vector4 with two components infinite should not be finite.");
			CHECK_FALSE_MESSAGE(
					Vector4(0, x, y, 3).is_finite(),
					"Vector4 with two components infinite should not be finite.");
			CHECK_FALSE_MESSAGE(
					Vector4(0, x, 2, y).is_finite(),
					"Vector4 with two components infinite should not be finite.");
			CHECK_FALSE_MESSAGE(
					Vector4(0, 1, x, y).is_finite(),
					"Vector4 with two components infinite should not be finite.");
		}
	}

	for (double x : infinite) {
		for (double y : infinite) {
			for (double z : infinite) {
				CHECK_FALSE_MESSAGE(
						Vector4(0, x, y, z).is_finite(),
						"Vector4 with three components infinite should not be finite.");
				CHECK_FALSE_MESSAGE(
						Vector4(x, 1, y, z).is_finite(),
						"Vector4 with three components infinite should not be finite.");
				CHECK_FALSE_MESSAGE(
						Vector4(x, y, 2, z).is_finite(),
						"Vector4 with three components infinite should not be finite.");
				CHECK_FALSE_MESSAGE(
						Vector4(x, y, z, 3).is_finite(),
						"Vector4 with three components infinite should not be finite.");
			}
		}
	}

	for (double x : infinite) {
		for (double y : infinite) {
			for (double z : infinite) {
				for (double w : infinite) {
					CHECK_FALSE_MESSAGE(
							Vector4(x, y, z, w).is_finite(),
							"Vector4 with four components infinite should not be finite.");
				}
			}
		}
	}
}

} // namespace TestVector4
