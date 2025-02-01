/**************************************************************************/
/*  test_vector2.h                                                        */
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

namespace TestVector2 {

TEST_CASE("[Vector2] Constructor methods") {
	const Vector2 vector_empty = Vector2();
	const Vector2 vector_zero = Vector2(0.0, 0.0);
	CHECK_MESSAGE(
			vector_empty == vector_zero,
			"Vector2 Constructor with no inputs should return a zero Vector2.");
}

TEST_CASE("[Vector2] Angle methods") {
	const Vector2 vector_x = Vector2(1, 0);
	const Vector2 vector_y = Vector2(0, 1);
	CHECK_MESSAGE(
			vector_x.angle_to(vector_y) == doctest::Approx((real_t)Math_TAU / 4),
			"Vector2 angle_to should work as expected.");
	CHECK_MESSAGE(
			vector_y.angle_to(vector_x) == doctest::Approx((real_t)-Math_TAU / 4),
			"Vector2 angle_to should work as expected.");
	CHECK_MESSAGE(
			vector_x.angle_to_point(vector_y) == doctest::Approx((real_t)Math_TAU * 3 / 8),
			"Vector2 angle_to_point should work as expected.");
	CHECK_MESSAGE(
			vector_y.angle_to_point(vector_x) == doctest::Approx((real_t)-Math_TAU / 8),
			"Vector2 angle_to_point should work as expected.");
}

TEST_CASE("[Vector2] Axis methods") {
	Vector2 vector = Vector2(1.2, 3.4);
	CHECK_MESSAGE(
			vector.max_axis_index() == Vector2::Axis::AXIS_Y,
			"Vector2 max_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector.min_axis_index() == Vector2::Axis::AXIS_X,
			"Vector2 min_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector[vector.min_axis_index()] == (real_t)1.2,
			"Vector2 array operator should work as expected.");
	vector[Vector2::Axis::AXIS_Y] = 3.7;
	CHECK_MESSAGE(
			vector[Vector2::Axis::AXIS_Y] == (real_t)3.7,
			"Vector2 array operator setter should work as expected.");
}

TEST_CASE("[Vector2] Interpolation methods") {
	const Vector2 vector1 = Vector2(1, 2);
	const Vector2 vector2 = Vector2(4, 5);
	CHECK_MESSAGE(
			vector1.lerp(vector2, 0.5) == Vector2(2.5, 3.5),
			"Vector2 lerp should work as expected.");
	CHECK_MESSAGE(
			vector1.lerp(vector2, 1.0 / 3.0).is_equal_approx(Vector2(2, 3)),
			"Vector2 lerp should work as expected.");
	CHECK_MESSAGE(
			vector1.normalized().slerp(vector2.normalized(), 0.5).is_equal_approx(Vector2(0.538953602313995361, 0.84233558177947998)),
			"Vector2 slerp should work as expected.");
	CHECK_MESSAGE(
			vector1.normalized().slerp(vector2.normalized(), 1.0 / 3.0).is_equal_approx(Vector2(0.508990883827209473, 0.860771894454956055)),
			"Vector2 slerp should work as expected.");
	CHECK_MESSAGE(
			Vector2(5, 0).slerp(Vector2(0, 5), 0.5).is_equal_approx(Vector2(5, 5) * Math_SQRT12),
			"Vector2 slerp with non-normalized values should work as expected.");
	CHECK_MESSAGE(
			Vector2(1, 1).slerp(Vector2(2, 2), 0.5).is_equal_approx(Vector2(1.5, 1.5)),
			"Vector2 slerp with colinear inputs should behave as expected.");
	CHECK_MESSAGE(
			Vector2().slerp(Vector2(), 0.5) == Vector2(),
			"Vector2 slerp with both inputs as zero vectors should return a zero vector.");
	CHECK_MESSAGE(
			Vector2().slerp(Vector2(1, 1), 0.5) == Vector2(0.5, 0.5),
			"Vector2 slerp with one input as zero should behave like a regular lerp.");
	CHECK_MESSAGE(
			Vector2(1, 1).slerp(Vector2(), 0.5) == Vector2(0.5, 0.5),
			"Vector2 slerp with one input as zero should behave like a regular lerp.");
	CHECK_MESSAGE(
			Vector2(4, 6).slerp(Vector2(8, 10), 0.5).is_equal_approx(Vector2(5.9076470794008017626, 8.07918879020090480697)),
			"Vector2 slerp should work as expected.");
	CHECK_MESSAGE(
			vector1.slerp(vector2, 0.5).length() == doctest::Approx((real_t)4.31959610746631919),
			"Vector2 slerp with different length input should return a vector with an interpolated length.");
	CHECK_MESSAGE(
			vector1.angle_to(vector1.slerp(vector2, 0.5)) * 2 == doctest::Approx(vector1.angle_to(vector2)),
			"Vector2 slerp with different length input should return a vector with an interpolated angle.");
	CHECK_MESSAGE(
			vector1.cubic_interpolate(vector2, Vector2(), Vector2(7, 7), 0.5) == Vector2(2.375, 3.5),
			"Vector2 cubic_interpolate should work as expected.");
	CHECK_MESSAGE(
			vector1.cubic_interpolate(vector2, Vector2(), Vector2(7, 7), 1.0 / 3.0).is_equal_approx(Vector2(1.851851940155029297, 2.962963104248046875)),
			"Vector2 cubic_interpolate should work as expected.");
	CHECK_MESSAGE(
			Vector2(1, 0).move_toward(Vector2(10, 0), 3) == Vector2(4, 0),
			"Vector2 move_toward should work as expected.");
}

TEST_CASE("[Vector2] Length methods") {
	const Vector2 vector1 = Vector2(10, 10);
	const Vector2 vector2 = Vector2(20, 30);
	CHECK_MESSAGE(
			vector1.length_squared() == 200,
			"Vector2 length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.length() == doctest::Approx(10 * (real_t)Math_SQRT2),
			"Vector2 length should work as expected.");
	CHECK_MESSAGE(
			vector2.length_squared() == 1300,
			"Vector2 length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector2.length() == doctest::Approx((real_t)36.05551275463989293119),
			"Vector2 length should work as expected.");
	CHECK_MESSAGE(
			vector1.distance_squared_to(vector2) == 500,
			"Vector2 distance_squared_to should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.distance_to(vector2) == doctest::Approx((real_t)22.36067977499789696409),
			"Vector2 distance_to should work as expected.");
}

TEST_CASE("[Vector2] Limiting methods") {
	const Vector2 vector = Vector2(10, 10);
	CHECK_MESSAGE(
			vector.limit_length().is_equal_approx(Vector2(Math_SQRT12, Math_SQRT12)),
			"Vector2 limit_length should work as expected.");
	CHECK_MESSAGE(
			vector.limit_length(5).is_equal_approx(5 * Vector2(Math_SQRT12, Math_SQRT12)),
			"Vector2 limit_length should work as expected.");

	CHECK_MESSAGE(
			Vector2(-5, 15).clamp(Vector2(), vector).is_equal_approx(Vector2(0, 10)),
			"Vector2 clamp should work as expected.");
	CHECK_MESSAGE(
			vector.clamp(Vector2(0, 15), Vector2(5, 20)).is_equal_approx(Vector2(5, 15)),
			"Vector2 clamp should work as expected.");
}

TEST_CASE("[Vector2] Normalization methods") {
	CHECK_MESSAGE(
			Vector2(1, 0).is_normalized() == true,
			"Vector2 is_normalized should return true for a normalized vector.");
	CHECK_MESSAGE(
			Vector2(1, 1).is_normalized() == false,
			"Vector2 is_normalized should return false for a non-normalized vector.");
	CHECK_MESSAGE(
			Vector2(1, 0).normalized() == Vector2(1, 0),
			"Vector2 normalized should return the same vector for a normalized vector.");
	CHECK_MESSAGE(
			Vector2(1, 1).normalized().is_equal_approx(Vector2(Math_SQRT12, Math_SQRT12)),
			"Vector2 normalized should work as expected.");

	Vector2 vector = Vector2(3.2, -5.4);
	vector.normalize();
	CHECK_MESSAGE(
			vector == Vector2(3.2, -5.4).normalized(),
			"Vector2 normalize should convert same way as Vector2 normalized.");
	CHECK_MESSAGE(
			vector.is_equal_approx(Vector2(0.509802390301732898898, -0.860291533634174266891)),
			"Vector2 normalize should work as expected.");
}

TEST_CASE("[Vector2] Operators") {
	const Vector2 decimal1 = Vector2(2.3, 4.9);
	const Vector2 decimal2 = Vector2(1.2, 3.4);
	const Vector2 power1 = Vector2(0.75, 1.5);
	const Vector2 power2 = Vector2(0.5, 0.125);
	const Vector2 int1 = Vector2(4, 5);
	const Vector2 int2 = Vector2(1, 2);

	CHECK_MESSAGE(
			(decimal1 + decimal2).is_equal_approx(Vector2(3.5, 8.3)),
			"Vector2 addition should behave as expected.");
	CHECK_MESSAGE(
			(power1 + power2) == Vector2(1.25, 1.625),
			"Vector2 addition with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 + int2) == Vector2(5, 7),
			"Vector2 addition with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 - decimal2).is_equal_approx(Vector2(1.1, 1.5)),
			"Vector2 subtraction should behave as expected.");
	CHECK_MESSAGE(
			(power1 - power2) == Vector2(0.25, 1.375),
			"Vector2 subtraction with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 - int2) == Vector2(3, 3),
			"Vector2 subtraction with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 * decimal2).is_equal_approx(Vector2(2.76, 16.66)),
			"Vector2 multiplication should behave as expected.");
	CHECK_MESSAGE(
			(power1 * power2) == Vector2(0.375, 0.1875),
			"Vector2 multiplication with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 * int2) == Vector2(4, 10),
			"Vector2 multiplication with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 / decimal2).is_equal_approx(Vector2(1.91666666666666666, 1.44117647058823529)),
			"Vector2 division should behave as expected.");
	CHECK_MESSAGE(
			(power1 / power2) == Vector2(1.5, 12.0),
			"Vector2 division with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 / int2) == Vector2(4, 2.5),
			"Vector2 division with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 * 2).is_equal_approx(Vector2(4.6, 9.8)),
			"Vector2 multiplication should behave as expected.");
	CHECK_MESSAGE(
			(power1 * 2) == Vector2(1.5, 3),
			"Vector2 multiplication with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 * 2) == Vector2(8, 10),
			"Vector2 multiplication with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 / 2).is_equal_approx(Vector2(1.15, 2.45)),
			"Vector2 division should behave as expected.");
	CHECK_MESSAGE(
			(power1 / 2) == Vector2(0.375, 0.75),
			"Vector2 division with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 / 2) == Vector2(2, 2.5),
			"Vector2 division with integers should give exact results.");

	CHECK_MESSAGE(
			((Vector2i)decimal1) == Vector2i(2, 4),
			"Vector2 cast to Vector2i should work as expected.");
	CHECK_MESSAGE(
			((Vector2i)decimal2) == Vector2i(1, 3),
			"Vector2 cast to Vector2i should work as expected.");
	CHECK_MESSAGE(
			Vector2(Vector2i(1, 2)) == Vector2(1, 2),
			"Vector2 constructed from Vector2i should work as expected.");

	CHECK_MESSAGE(
			((String)decimal1) == "(2.3, 4.9)",
			"Vector2 cast to String should work as expected.");
	CHECK_MESSAGE(
			((String)decimal2) == "(1.2, 3.4)",
			"Vector2 cast to String should work as expected.");
	CHECK_MESSAGE(
			((String)Vector2(9.8, 9.9)) == "(9.8, 9.9)",
			"Vector2 cast to String should work as expected.");
#ifdef REAL_T_IS_DOUBLE
	CHECK_MESSAGE(
			((String)Vector2(Math_PI, Math_TAU)) == "(3.14159265358979, 6.28318530717959)",
			"Vector2 cast to String should print the correct amount of digits for real_t = double.");
#else
	CHECK_MESSAGE(
			((String)Vector2(Math_PI, Math_TAU)) == "(3.141593, 6.283185)",
			"Vector2 cast to String should print the correct amount of digits for real_t = float.");
#endif // REAL_T_IS_DOUBLE
}

TEST_CASE("[Vector2] Other methods") {
	const Vector2 vector = Vector2(1.2, 3.4);
	CHECK_MESSAGE(
			vector.aspect() == doctest::Approx((real_t)1.2 / (real_t)3.4),
			"Vector2 aspect should work as expected.");

	CHECK_MESSAGE(
			vector.direction_to(Vector2()).is_equal_approx(-vector.normalized()),
			"Vector2 direction_to should work as expected.");
	CHECK_MESSAGE(
			Vector2(1, 1).direction_to(Vector2(2, 2)).is_equal_approx(Vector2(Math_SQRT12, Math_SQRT12)),
			"Vector2 direction_to should work as expected.");

	CHECK_MESSAGE(
			vector.posmod(2).is_equal_approx(Vector2(1.2, 1.4)),
			"Vector2 posmod should work as expected.");
	CHECK_MESSAGE(
			(-vector).posmod(2).is_equal_approx(Vector2(0.8, 0.6)),
			"Vector2 posmod should work as expected.");
	CHECK_MESSAGE(
			vector.posmodv(Vector2(1, 2)).is_equal_approx(Vector2(0.2, 1.4)),
			"Vector2 posmodv should work as expected.");
	CHECK_MESSAGE(
			(-vector).posmodv(Vector2(2, 3)).is_equal_approx(Vector2(0.8, 2.6)),
			"Vector2 posmodv should work as expected.");

	CHECK_MESSAGE(
			vector.rotated(Math_TAU).is_equal_approx(Vector2(1.2, 3.4)),
			"Vector2 rotated should work as expected.");
	CHECK_MESSAGE(
			vector.rotated(Math_TAU / 4).is_equal_approx(Vector2(-3.4, 1.2)),
			"Vector2 rotated should work as expected.");
	CHECK_MESSAGE(
			vector.rotated(Math_TAU / 3).is_equal_approx(Vector2(-3.544486372867091398996, -0.660769515458673623883)),
			"Vector2 rotated should work as expected.");
	CHECK_MESSAGE(
			vector.rotated(Math_TAU / 2).is_equal_approx(vector.rotated(Math_TAU / -2)),
			"Vector2 rotated should work as expected.");

	CHECK_MESSAGE(
			vector.snapped(Vector2(1, 1)) == Vector2(1, 3),
			"Vector2 snapped to integers should be the same as rounding.");
	CHECK_MESSAGE(
			Vector2(3.4, 5.6).snapped(Vector2(1, 1)) == Vector2(3, 6),
			"Vector2 snapped to integers should be the same as rounding.");
	CHECK_MESSAGE(
			vector.snapped(Vector2(0.25, 0.25)) == Vector2(1.25, 3.5),
			"Vector2 snapped to 0.25 should give exact results.");

	CHECK_MESSAGE(
			Vector2(1.2, 2.5).is_equal_approx(vector.min(Vector2(3.0, 2.5))),
			"Vector2 min should return expected value.");

	CHECK_MESSAGE(
			Vector2(5.3, 3.4).is_equal_approx(vector.max(Vector2(5.3, 2.0))),
			"Vector2 max should return expected value.");
}

TEST_CASE("[Vector2] Plane methods") {
	const Vector2 vector = Vector2(1.2, 3.4);
	const Vector2 vector_y = Vector2(0, 1);
	const Vector2 vector_normal = Vector2(0.95879811270838721622267, 0.2840883296913739899919);
	const real_t p_d = 99.1;
	CHECK_MESSAGE(
			vector.bounce(vector_y) == Vector2(1.2, -3.4),
			"Vector2 bounce on a plane with normal of the Y axis should.");
	CHECK_MESSAGE(
			vector.bounce(vector_normal).is_equal_approx(Vector2(-2.85851197982345523329, 2.197477931904161412358)),
			"Vector2 bounce with normal should return expected value.");
	CHECK_MESSAGE(
			vector.reflect(vector_y) == Vector2(-1.2, 3.4),
			"Vector2 reflect on a plane with normal of the Y axis should.");
	CHECK_MESSAGE(
			vector.reflect(vector_normal).is_equal_approx(Vector2(2.85851197982345523329, -2.197477931904161412358)),
			"Vector2 reflect with normal should return expected value.");
	CHECK_MESSAGE(
			vector.project(vector_y) == Vector2(0, 3.4),
			"Vector2 projected on the Y axis should only give the Y component.");
	CHECK_MESSAGE(
			vector.project(vector_normal).is_equal_approx(Vector2(2.0292559899117276166, 0.60126103404791929382)),
			"Vector2 projected on a normal should return expected value.");
	CHECK_MESSAGE(
			vector_normal.plane_project(p_d, vector).is_equal_approx(Vector2(94.187635516479631, 30.951892004882851)),
			"Vector2 plane_project should return expected value.");
	CHECK_MESSAGE(
			vector.slide(vector_y) == Vector2(1.2, 0),
			"Vector2 slide on a plane with normal of the Y axis should set the Y to zero.");
	CHECK_MESSAGE(
			vector.slide(vector_normal).is_equal_approx(Vector2(-0.8292559899117276166456, 2.798738965952080706179)),
			"Vector2 slide with normal should return expected value.");
	// There's probably a better way to test these ones?
#ifdef MATH_CHECKS
	const Vector2 vector_non_normal = Vector2(5.4, 1.6);
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			vector.bounce(vector_non_normal).is_equal_approx(Vector2()),
			"Vector2 bounce should return empty Vector2 with non-normalized input.");
	CHECK_MESSAGE(
			vector.reflect(vector_non_normal).is_equal_approx(Vector2()),
			"Vector2 reflect should return empty Vector2 with non-normalized input.");
	CHECK_MESSAGE(
			vector.slide(vector_non_normal).is_equal_approx(Vector2()),
			"Vector2 slide should return empty Vector2 with non-normalized input.");
	ERR_PRINT_ON;
#endif // MATH_CHECKS
}

TEST_CASE("[Vector2] Rounding methods") {
	const Vector2 vector1 = Vector2(1.2, 5.6);
	const Vector2 vector2 = Vector2(1.2, -5.6);
	CHECK_MESSAGE(
			vector1.abs() == vector1,
			"Vector2 abs should work as expected.");
	CHECK_MESSAGE(
			vector2.abs() == vector1,
			"Vector2 abs should work as expected.");

	CHECK_MESSAGE(
			vector1.ceil() == Vector2(2, 6),
			"Vector2 ceil should work as expected.");
	CHECK_MESSAGE(
			vector2.ceil() == Vector2(2, -5),
			"Vector2 ceil should work as expected.");

	CHECK_MESSAGE(
			vector1.floor() == Vector2(1, 5),
			"Vector2 floor should work as expected.");
	CHECK_MESSAGE(
			vector2.floor() == Vector2(1, -6),
			"Vector2 floor should work as expected.");

	CHECK_MESSAGE(
			vector1.round() == Vector2(1, 6),
			"Vector2 round should work as expected.");
	CHECK_MESSAGE(
			vector2.round() == Vector2(1, -6),
			"Vector2 round should work as expected.");

	CHECK_MESSAGE(
			vector1.sign() == Vector2(1, 1),
			"Vector2 sign should work as expected.");
	CHECK_MESSAGE(
			vector2.sign() == Vector2(1, -1),
			"Vector2 sign should work as expected.");
}

TEST_CASE("[Vector2] Linear algebra methods") {
	const Vector2 vector_x = Vector2(1, 0);
	const Vector2 vector_y = Vector2(0, 1);
	const Vector2 a = Vector2(3.5, 8.5);
	const Vector2 b = Vector2(5.2, 4.6);
	CHECK_MESSAGE(
			vector_x.cross(vector_y) == 1,
			"Vector2 cross product of X and Y should give 1.");
	CHECK_MESSAGE(
			vector_y.cross(vector_x) == -1,
			"Vector2 cross product of Y and X should give negative 1.");
	CHECK_MESSAGE(
			a.cross(b) == doctest::Approx((real_t)-28.1),
			"Vector2 cross should return expected value.");
	CHECK_MESSAGE(
			Vector2(-a.x, a.y).cross(Vector2(b.x, -b.y)) == doctest::Approx((real_t)-28.1),
			"Vector2 cross should return expected value.");

	CHECK_MESSAGE(
			vector_x.dot(vector_y) == 0.0,
			"Vector2 dot product of perpendicular vectors should be zero.");
	CHECK_MESSAGE(
			vector_x.dot(vector_x) == 1.0,
			"Vector2 dot product of identical unit vectors should be one.");
	CHECK_MESSAGE(
			(vector_x * 10).dot(vector_x * 10) == 100.0,
			"Vector2 dot product of same direction vectors should behave as expected.");
	CHECK_MESSAGE(
			a.dot(b) == doctest::Approx((real_t)57.3),
			"Vector2 dot should return expected value.");
	CHECK_MESSAGE(
			Vector2(-a.x, a.y).dot(Vector2(b.x, -b.y)) == doctest::Approx((real_t)-57.3),
			"Vector2 dot should return expected value.");
}

TEST_CASE("[Vector2] Finite number checks") {
	const double infinite[] = { NAN, INFINITY, -INFINITY };

	CHECK_MESSAGE(
			Vector2(0, 1).is_finite(),
			"Vector2(0, 1) should be finite");

	for (double x : infinite) {
		CHECK_FALSE_MESSAGE(
				Vector2(x, 1).is_finite(),
				"Vector2 with one component infinite should not be finite.");
		CHECK_FALSE_MESSAGE(
				Vector2(0, x).is_finite(),
				"Vector2 with one component infinite should not be finite.");
	}

	for (double x : infinite) {
		for (double y : infinite) {
			CHECK_FALSE_MESSAGE(
					Vector2(x, y).is_finite(),
					"Vector2 with two components infinite should not be finite.");
		}
	}
}

} // namespace TestVector2
