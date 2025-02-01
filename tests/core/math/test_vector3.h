/**************************************************************************/
/*  test_vector3.h                                                        */
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

#include "core/math/vector3.h"
#include "tests/test_macros.h"

#define Math_SQRT13 0.57735026918962576450914878050196
#define Math_SQRT3 1.7320508075688772935274463415059

namespace TestVector3 {

TEST_CASE("[Vector3] Constructor methods") {
	const Vector3 vector_empty = Vector3();
	const Vector3 vector_zero = Vector3(0.0, 0.0, 0.0);
	CHECK_MESSAGE(
			vector_empty == vector_zero,
			"Vector3 Constructor with no inputs should return a zero Vector3.");
}

TEST_CASE("[Vector3] Angle methods") {
	const Vector3 vector_x = Vector3(1, 0, 0);
	const Vector3 vector_y = Vector3(0, 1, 0);
	const Vector3 vector_yz = Vector3(0, 1, 1);
	CHECK_MESSAGE(
			vector_x.angle_to(vector_y) == doctest::Approx((real_t)Math_TAU / 4),
			"Vector3 angle_to should work as expected.");
	CHECK_MESSAGE(
			vector_x.angle_to(vector_yz) == doctest::Approx((real_t)Math_TAU / 4),
			"Vector3 angle_to should work as expected.");
	CHECK_MESSAGE(
			vector_yz.angle_to(vector_x) == doctest::Approx((real_t)Math_TAU / 4),
			"Vector3 angle_to should work as expected.");
	CHECK_MESSAGE(
			vector_y.angle_to(vector_yz) == doctest::Approx((real_t)Math_TAU / 8),
			"Vector3 angle_to should work as expected.");

	CHECK_MESSAGE(
			vector_x.signed_angle_to(vector_y, vector_y) == doctest::Approx((real_t)Math_TAU / 4),
			"Vector3 signed_angle_to edge case should be positive.");
	CHECK_MESSAGE(
			vector_x.signed_angle_to(vector_yz, vector_y) == doctest::Approx((real_t)Math_TAU / -4),
			"Vector3 signed_angle_to should work as expected.");
	CHECK_MESSAGE(
			vector_yz.signed_angle_to(vector_x, vector_y) == doctest::Approx((real_t)Math_TAU / 4),
			"Vector3 signed_angle_to should work as expected.");
}

TEST_CASE("[Vector3] Axis methods") {
	Vector3 vector = Vector3(1.2, 3.4, 5.6);
	CHECK_MESSAGE(
			vector.max_axis_index() == Vector3::Axis::AXIS_Z,
			"Vector3 max_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector.min_axis_index() == Vector3::Axis::AXIS_X,
			"Vector3 min_axis_index should work as expected.");
	CHECK_MESSAGE(
			vector[vector.max_axis_index()] == (real_t)5.6,
			"Vector3 array operator should work as expected.");
	CHECK_MESSAGE(
			vector[vector.min_axis_index()] == (real_t)1.2,
			"Vector3 array operator should work as expected.");

	vector[Vector3::Axis::AXIS_Y] = 3.7;
	CHECK_MESSAGE(
			vector[Vector3::Axis::AXIS_Y] == (real_t)3.7,
			"Vector3 array operator setter should work as expected.");
}

TEST_CASE("[Vector3] Interpolation methods") {
	const Vector3 vector1 = Vector3(1, 2, 3);
	const Vector3 vector2 = Vector3(4, 5, 6);
	CHECK_MESSAGE(
			vector1.lerp(vector2, 0.5) == Vector3(2.5, 3.5, 4.5),
			"Vector3 lerp should work as expected.");
	CHECK_MESSAGE(
			vector1.lerp(vector2, 1.0 / 3.0).is_equal_approx(Vector3(2, 3, 4)),
			"Vector3 lerp should work as expected.");
	CHECK_MESSAGE(
			vector1.normalized().slerp(vector2.normalized(), 0.5).is_equal_approx(Vector3(0.363866806030273438, 0.555698215961456299, 0.747529566287994385)),
			"Vector3 slerp should work as expected.");
	CHECK_MESSAGE(
			vector1.normalized().slerp(vector2.normalized(), 1.0 / 3.0).is_equal_approx(Vector3(0.332119762897491455, 0.549413740634918213, 0.766707837581634521)),
			"Vector3 slerp should work as expected.");
	CHECK_MESSAGE(
			Vector3(5, 0, 0).slerp(Vector3(0, 3, 4), 0.5).is_equal_approx(Vector3(3.535533905029296875, 2.121320486068725586, 2.828427314758300781)),
			"Vector3 slerp with non-normalized values should work as expected.");
	CHECK_MESSAGE(
			Vector3(1, 1, 1).slerp(Vector3(2, 2, 2), 0.5).is_equal_approx(Vector3(1.5, 1.5, 1.5)),
			"Vector3 slerp with colinear inputs should behave as expected.");
	CHECK_MESSAGE(
			Vector3().slerp(Vector3(), 0.5) == Vector3(),
			"Vector3 slerp with both inputs as zero vectors should return a zero vector.");
	CHECK_MESSAGE(
			Vector3().slerp(Vector3(1, 1, 1), 0.5) == Vector3(0.5, 0.5, 0.5),
			"Vector3 slerp with one input as zero should behave like a regular lerp.");
	CHECK_MESSAGE(
			Vector3(1, 1, 1).slerp(Vector3(), 0.5) == Vector3(0.5, 0.5, 0.5),
			"Vector3 slerp with one input as zero should behave like a regular lerp.");
	CHECK_MESSAGE(
			Vector3(4, 6, 2).slerp(Vector3(8, 10, 3), 0.5).is_equal_approx(Vector3(5.90194219811429941053, 8.06758688849378394534, 2.558307894718317120038)),
			"Vector3 slerp should work as expected.");
	CHECK_MESSAGE(
			vector1.slerp(vector2, 0.5).length() == doctest::Approx((real_t)6.25831088708303172),
			"Vector3 slerp with different length input should return a vector with an interpolated length.");
	CHECK_MESSAGE(
			vector1.angle_to(vector1.slerp(vector2, 0.5)) * 2 == doctest::Approx(vector1.angle_to(vector2)),
			"Vector3 slerp with different length input should return a vector with an interpolated angle.");
	CHECK_MESSAGE(
			vector1.cubic_interpolate(vector2, Vector3(), Vector3(7, 7, 7), 0.5) == Vector3(2.375, 3.5, 4.625),
			"Vector3 cubic_interpolate should work as expected.");
	CHECK_MESSAGE(
			vector1.cubic_interpolate(vector2, Vector3(), Vector3(7, 7, 7), 1.0 / 3.0).is_equal_approx(Vector3(1.851851940155029297, 2.962963104248046875, 4.074074268341064453)),
			"Vector3 cubic_interpolate should work as expected.");
	CHECK_MESSAGE(
			Vector3(1, 0, 0).move_toward(Vector3(10, 0, 0), 3) == Vector3(4, 0, 0),
			"Vector3 move_toward should work as expected.");
}

TEST_CASE("[Vector3] Length methods") {
	const Vector3 vector1 = Vector3(10, 10, 10);
	const Vector3 vector2 = Vector3(20, 30, 40);
	CHECK_MESSAGE(
			vector1.length_squared() == 300,
			"Vector3 length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.length() == doctest::Approx(10 * (real_t)Math_SQRT3),
			"Vector3 length should work as expected.");
	CHECK_MESSAGE(
			vector2.length_squared() == 2900,
			"Vector3 length_squared should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector2.length() == doctest::Approx((real_t)53.8516480713450403125),
			"Vector3 length should work as expected.");
	CHECK_MESSAGE(
			vector1.distance_squared_to(vector2) == 1400,
			"Vector3 distance_squared_to should work as expected and return exact result.");
	CHECK_MESSAGE(
			vector1.distance_to(vector2) == doctest::Approx((real_t)37.41657386773941385584),
			"Vector3 distance_to should work as expected.");
}

TEST_CASE("[Vector3] Limiting methods") {
	const Vector3 vector = Vector3(10, 10, 10);
	CHECK_MESSAGE(
			vector.limit_length().is_equal_approx(Vector3(Math_SQRT13, Math_SQRT13, Math_SQRT13)),
			"Vector3 limit_length should work as expected.");
	CHECK_MESSAGE(
			vector.limit_length(5).is_equal_approx(5 * Vector3(Math_SQRT13, Math_SQRT13, Math_SQRT13)),
			"Vector3 limit_length should work as expected.");

	CHECK_MESSAGE(
			Vector3(-5, 5, 15).clamp(Vector3(), vector) == Vector3(0, 5, 10),
			"Vector3 clamp should work as expected.");
	CHECK_MESSAGE(
			vector.clamp(Vector3(0, 10, 15), Vector3(5, 10, 20)) == Vector3(5, 10, 15),
			"Vector3 clamp should work as expected.");
}

TEST_CASE("[Vector3] Normalization methods") {
	CHECK_MESSAGE(
			Vector3(1, 0, 0).is_normalized() == true,
			"Vector3 is_normalized should return true for a normalized vector.");
	CHECK_MESSAGE(
			Vector3(1, 1, 1).is_normalized() == false,
			"Vector3 is_normalized should return false for a non-normalized vector.");
	CHECK_MESSAGE(
			Vector3(1, 0, 0).normalized() == Vector3(1, 0, 0),
			"Vector3 normalized should return the same vector for a normalized vector.");
	CHECK_MESSAGE(
			Vector3(1, 1, 0).normalized().is_equal_approx(Vector3(Math_SQRT12, Math_SQRT12, 0)),
			"Vector3 normalized should work as expected.");
	CHECK_MESSAGE(
			Vector3(1, 1, 1).normalized().is_equal_approx(Vector3(Math_SQRT13, Math_SQRT13, Math_SQRT13)),
			"Vector3 normalized should work as expected.");

	Vector3 vector = Vector3(3.2, -5.4, 6);
	vector.normalize();
	CHECK_MESSAGE(
			vector == Vector3(3.2, -5.4, 6).normalized(),
			"Vector3 normalize should convert same way as Vector3 normalized.");
	CHECK_MESSAGE(
			vector.is_equal_approx(Vector3(0.368522751763902980457, -0.621882143601586279522, 0.6909801595573180883585)),
			"Vector3 normalize should work as expected.");
}

TEST_CASE("[Vector3] Operators") {
	const Vector3 decimal1 = Vector3(2.3, 4.9, 7.8);
	const Vector3 decimal2 = Vector3(1.2, 3.4, 5.6);
	const Vector3 power1 = Vector3(0.75, 1.5, 0.625);
	const Vector3 power2 = Vector3(0.5, 0.125, 0.25);
	const Vector3 int1 = Vector3(4, 5, 9);
	const Vector3 int2 = Vector3(1, 2, 3);

	CHECK_MESSAGE(
			(decimal1 + decimal2).is_equal_approx(Vector3(3.5, 8.3, 13.4)),
			"Vector3 addition should behave as expected.");
	CHECK_MESSAGE(
			(power1 + power2) == Vector3(1.25, 1.625, 0.875),
			"Vector3 addition with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 + int2) == Vector3(5, 7, 12),
			"Vector3 addition with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 - decimal2).is_equal_approx(Vector3(1.1, 1.5, 2.2)),
			"Vector3 subtraction should behave as expected.");
	CHECK_MESSAGE(
			(power1 - power2) == Vector3(0.25, 1.375, 0.375),
			"Vector3 subtraction with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 - int2) == Vector3(3, 3, 6),
			"Vector3 subtraction with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 * decimal2).is_equal_approx(Vector3(2.76, 16.66, 43.68)),
			"Vector3 multiplication should behave as expected.");
	CHECK_MESSAGE(
			(power1 * power2) == Vector3(0.375, 0.1875, 0.15625),
			"Vector3 multiplication with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 * int2) == Vector3(4, 10, 27),
			"Vector3 multiplication with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 / decimal2).is_equal_approx(Vector3(1.91666666666666666, 1.44117647058823529, 1.39285714285714286)),
			"Vector3 division should behave as expected.");
	CHECK_MESSAGE(
			(power1 / power2) == Vector3(1.5, 12.0, 2.5),
			"Vector3 division with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 / int2) == Vector3(4, 2.5, 3),
			"Vector3 division with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 * 2).is_equal_approx(Vector3(4.6, 9.8, 15.6)),
			"Vector3 multiplication should behave as expected.");
	CHECK_MESSAGE(
			(power1 * 2) == Vector3(1.5, 3, 1.25),
			"Vector3 multiplication with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 * 2) == Vector3(8, 10, 18),
			"Vector3 multiplication with integers should give exact results.");

	CHECK_MESSAGE(
			(decimal1 / 2).is_equal_approx(Vector3(1.15, 2.45, 3.9)),
			"Vector3 division should behave as expected.");
	CHECK_MESSAGE(
			(power1 / 2) == Vector3(0.375, 0.75, 0.3125),
			"Vector3 division with powers of two should give exact results.");
	CHECK_MESSAGE(
			(int1 / 2) == Vector3(2, 2.5, 4.5),
			"Vector3 division with integers should give exact results.");

	CHECK_MESSAGE(
			((Vector3i)decimal1) == Vector3i(2, 4, 7),
			"Vector3 cast to Vector3i should work as expected.");
	CHECK_MESSAGE(
			((Vector3i)decimal2) == Vector3i(1, 3, 5),
			"Vector3 cast to Vector3i should work as expected.");
	CHECK_MESSAGE(
			Vector3(Vector3i(1, 2, 3)) == Vector3(1, 2, 3),
			"Vector3 constructed from Vector3i should work as expected.");

	CHECK_MESSAGE(
			((String)decimal1) == "(2.3, 4.9, 7.8)",
			"Vector3 cast to String should work as expected.");
	CHECK_MESSAGE(
			((String)decimal2) == "(1.2, 3.4, 5.6)",
			"Vector3 cast to String should work as expected.");
	CHECK_MESSAGE(
			((String)Vector3(9.7, 9.8, 9.9)) == "(9.7, 9.8, 9.9)",
			"Vector3 cast to String should work as expected.");
#ifdef REAL_T_IS_DOUBLE
	CHECK_MESSAGE(
			((String)Vector3(Math_E, Math_SQRT2, Math_SQRT3)) == "(2.71828182845905, 1.4142135623731, 1.73205080756888)",
			"Vector3 cast to String should print the correct amount of digits for real_t = double.");
#else
	CHECK_MESSAGE(
			((String)Vector3(Math_E, Math_SQRT2, Math_SQRT3)) == "(2.718282, 1.414214, 1.732051)",
			"Vector3 cast to String should print the correct amount of digits for real_t = float.");
#endif // REAL_T_IS_DOUBLE
}

TEST_CASE("[Vector3] Other methods") {
	const Vector3 vector = Vector3(1.2, 3.4, 5.6);
	CHECK_MESSAGE(
			vector.direction_to(Vector3()).is_equal_approx(-vector.normalized()),
			"Vector3 direction_to should work as expected.");
	CHECK_MESSAGE(
			Vector3(1, 1, 1).direction_to(Vector3(2, 2, 2)).is_equal_approx(Vector3(Math_SQRT13, Math_SQRT13, Math_SQRT13)),
			"Vector3 direction_to should work as expected.");
	CHECK_MESSAGE(
			vector.inverse().is_equal_approx(Vector3(1 / 1.2, 1 / 3.4, 1 / 5.6)),
			"Vector3 inverse should work as expected.");
	CHECK_MESSAGE(
			vector.posmod(2).is_equal_approx(Vector3(1.2, 1.4, 1.6)),
			"Vector3 posmod should work as expected.");
	CHECK_MESSAGE(
			(-vector).posmod(2).is_equal_approx(Vector3(0.8, 0.6, 0.4)),
			"Vector3 posmod should work as expected.");
	CHECK_MESSAGE(
			vector.posmodv(Vector3(1, 2, 3)).is_equal_approx(Vector3(0.2, 1.4, 2.6)),
			"Vector3 posmodv should work as expected.");
	CHECK_MESSAGE(
			(-vector).posmodv(Vector3(2, 3, 4)).is_equal_approx(Vector3(0.8, 2.6, 2.4)),
			"Vector3 posmodv should work as expected.");

	CHECK_MESSAGE(
			vector.rotated(Vector3(0, 1, 0), Math_TAU).is_equal_approx(vector),
			"Vector3 rotated should work as expected.");
	CHECK_MESSAGE(
			vector.rotated(Vector3(0, 1, 0), Math_TAU / 4).is_equal_approx(Vector3(5.6, 3.4, -1.2)),
			"Vector3 rotated should work as expected.");
	CHECK_MESSAGE(
			vector.rotated(Vector3(1, 0, 0), Math_TAU / 3).is_equal_approx(Vector3(1.2, -6.54974226119285642, 0.1444863728670914)),
			"Vector3 rotated should work as expected.");
	CHECK_MESSAGE(
			vector.rotated(Vector3(0, 0, 1), Math_TAU / 2).is_equal_approx(vector.rotated(Vector3(0, 0, 1), Math_TAU / -2)),
			"Vector3 rotated should work as expected.");

	CHECK_MESSAGE(
			vector.snapped(Vector3(1, 1, 1)) == Vector3(1, 3, 6),
			"Vector3 snapped to integers should be the same as rounding.");
	CHECK_MESSAGE(
			vector.snapped(Vector3(0.25, 0.25, 0.25)) == Vector3(1.25, 3.5, 5.5),
			"Vector3 snapped to 0.25 should give exact results.");

	CHECK_MESSAGE(
			Vector3(1.2, 2.5, 2.0).is_equal_approx(vector.min(Vector3(3.0, 2.5, 2.0))),
			"Vector3 min should return expected value.");

	CHECK_MESSAGE(
			Vector3(5.3, 3.4, 5.6).is_equal_approx(vector.max(Vector3(5.3, 2.0, 3.0))),
			"Vector3 max should return expected value.");
}

TEST_CASE("[Vector3] Plane methods") {
	const Vector3 vector = Vector3(1.2, 3.4, 5.6);
	const Vector3 vector_y = Vector3(0, 1, 0);
	const Vector3 vector_normal = Vector3(0.88763458893247992491, 0.26300284116517923701, 0.37806658417494515320);
	CHECK_MESSAGE(
			vector.bounce(vector_y) == Vector3(1.2, -3.4, 5.6),
			"Vector3 bounce on a plane with normal of the Y axis should.");
	CHECK_MESSAGE(
			vector.bounce(vector_normal).is_equal_approx(Vector3(-6.0369629829775736287, 1.25571467171034855444, 2.517589840583626047)),
			"Vector3 bounce with normal should return expected value.");
	CHECK_MESSAGE(
			vector.reflect(vector_y) == Vector3(-1.2, 3.4, -5.6),
			"Vector3 reflect on a plane with normal of the Y axis should.");
	CHECK_MESSAGE(
			vector.reflect(vector_normal).is_equal_approx(Vector3(6.0369629829775736287, -1.25571467171034855444, -2.517589840583626047)),
			"Vector3 reflect with normal should return expected value.");
	CHECK_MESSAGE(
			vector.project(vector_y) == Vector3(0, 3.4, 0),
			"Vector3 projected on the Y axis should only give the Y component.");
	CHECK_MESSAGE(
			vector.project(vector_normal).is_equal_approx(Vector3(3.61848149148878681437, 1.0721426641448257227776, 1.54120507970818697649)),
			"Vector3 projected on a normal should return expected value.");
	CHECK_MESSAGE(
			vector.slide(vector_y) == Vector3(1.2, 0, 5.6),
			"Vector3 slide on a plane with normal of the Y axis should set the Y to zero.");
	CHECK_MESSAGE(
			vector.slide(vector_normal).is_equal_approx(Vector3(-2.41848149148878681437, 2.32785733585517427722237, 4.0587949202918130235)),
			"Vector3 slide with normal should return expected value.");
	// There's probably a better way to test these ones?
#ifdef MATH_CHECKS
	const Vector3 vector_non_normal = Vector3(5.4, 1.6, 2.3);
	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			vector.bounce(vector_non_normal).is_equal_approx(Vector3()),
			"Vector3 bounce should return empty Vector3 with non-normalized input.");
	CHECK_MESSAGE(
			vector.reflect(vector_non_normal).is_equal_approx(Vector3()),
			"Vector3 reflect should return empty Vector3 with non-normalized input.");
	CHECK_MESSAGE(
			vector.slide(vector_non_normal).is_equal_approx(Vector3()),
			"Vector3 slide should return empty Vector3 with non-normalized input.");
	ERR_PRINT_ON;
#endif // MATH_CHECKS
}

TEST_CASE("[Vector3] Rounding methods") {
	const Vector3 vector1 = Vector3(1.2, 3.4, 5.6);
	const Vector3 vector2 = Vector3(1.2, -3.4, -5.6);
	CHECK_MESSAGE(
			vector1.abs() == vector1,
			"Vector3 abs should work as expected.");
	CHECK_MESSAGE(
			vector2.abs() == vector1,
			"Vector3 abs should work as expected.");

	CHECK_MESSAGE(
			vector1.ceil() == Vector3(2, 4, 6),
			"Vector3 ceil should work as expected.");
	CHECK_MESSAGE(
			vector2.ceil() == Vector3(2, -3, -5),
			"Vector3 ceil should work as expected.");

	CHECK_MESSAGE(
			vector1.floor() == Vector3(1, 3, 5),
			"Vector3 floor should work as expected.");
	CHECK_MESSAGE(
			vector2.floor() == Vector3(1, -4, -6),
			"Vector3 floor should work as expected.");

	CHECK_MESSAGE(
			vector1.round() == Vector3(1, 3, 6),
			"Vector3 round should work as expected.");
	CHECK_MESSAGE(
			vector2.round() == Vector3(1, -3, -6),
			"Vector3 round should work as expected.");

	CHECK_MESSAGE(
			vector1.sign() == Vector3(1, 1, 1),
			"Vector3 sign should work as expected.");
	CHECK_MESSAGE(
			vector2.sign() == Vector3(1, -1, -1),
			"Vector3 sign should work as expected.");
}

TEST_CASE("[Vector3] Linear algebra methods") {
	const Vector3 vector_x = Vector3(1, 0, 0);
	const Vector3 vector_y = Vector3(0, 1, 0);
	const Vector3 vector_z = Vector3(0, 0, 1);
	const Vector3 a = Vector3(3.5, 8.5, 2.3);
	const Vector3 b = Vector3(5.2, 4.6, 7.8);
	CHECK_MESSAGE(
			vector_x.cross(vector_y) == vector_z,
			"Vector3 cross product of X and Y should give Z.");
	CHECK_MESSAGE(
			vector_y.cross(vector_x) == -vector_z,
			"Vector3 cross product of Y and X should give negative Z.");
	CHECK_MESSAGE(
			vector_y.cross(vector_z) == vector_x,
			"Vector3 cross product of Y and Z should give X.");
	CHECK_MESSAGE(
			vector_z.cross(vector_x) == vector_y,
			"Vector3 cross product of Z and X should give Y.");
	CHECK_MESSAGE(
			a.cross(b).is_equal_approx(Vector3(55.72, -15.34, -28.1)),
			"Vector3 cross should return expected value.");
	CHECK_MESSAGE(
			Vector3(-a.x, a.y, -a.z).cross(Vector3(b.x, -b.y, b.z)).is_equal_approx(Vector3(55.72, 15.34, -28.1)),
			"Vector2 cross should return expected value.");

	CHECK_MESSAGE(
			vector_x.dot(vector_y) == 0.0,
			"Vector3 dot product of perpendicular vectors should be zero.");
	CHECK_MESSAGE(
			vector_x.dot(vector_x) == 1.0,
			"Vector3 dot product of identical unit vectors should be one.");
	CHECK_MESSAGE(
			(vector_x * 10).dot(vector_x * 10) == 100.0,
			"Vector3 dot product of same direction vectors should behave as expected.");
	CHECK_MESSAGE(
			a.dot(b) == doctest::Approx((real_t)75.24),
			"Vector3 dot should return expected value.");
	CHECK_MESSAGE(
			Vector3(-a.x, a.y, -a.z).dot(Vector3(b.x, -b.y, b.z)) == doctest::Approx((real_t)-75.24),
			"Vector3 dot should return expected value.");
}

TEST_CASE("[Vector3] Finite number checks") {
	const double infinite[] = { NAN, INFINITY, -INFINITY };

	CHECK_MESSAGE(
			Vector3(0, 1, 2).is_finite(),
			"Vector3(0, 1, 2) should be finite");

	for (double x : infinite) {
		CHECK_FALSE_MESSAGE(
				Vector3(x, 1, 2).is_finite(),
				"Vector3 with one component infinite should not be finite.");
		CHECK_FALSE_MESSAGE(
				Vector3(0, x, 2).is_finite(),
				"Vector3 with one component infinite should not be finite.");
		CHECK_FALSE_MESSAGE(
				Vector3(0, 1, x).is_finite(),
				"Vector3 with one component infinite should not be finite.");
	}

	for (double x : infinite) {
		for (double y : infinite) {
			CHECK_FALSE_MESSAGE(
					Vector3(x, y, 2).is_finite(),
					"Vector3 with two components infinite should not be finite.");
			CHECK_FALSE_MESSAGE(
					Vector3(x, 1, y).is_finite(),
					"Vector3 with two components infinite should not be finite.");
			CHECK_FALSE_MESSAGE(
					Vector3(0, x, y).is_finite(),
					"Vector3 with two components infinite should not be finite.");
		}
	}

	for (double x : infinite) {
		for (double y : infinite) {
			for (double z : infinite) {
				CHECK_FALSE_MESSAGE(
						Vector3(x, y, z).is_finite(),
						"Vector3 with three components infinite should not be finite.");
			}
		}
	}
}

} // namespace TestVector3
