/**************************************************************************/
/*  test_basis.h                                                          */
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

#include "core/math/basis.h"
#include "core/math/random_number_generator.h"

#include "tests/test_macros.h"

namespace TestBasis {

Vector3 deg_to_rad(const Vector3 &p_rotation) {
	return p_rotation / 180.0 * Math_PI;
}

Vector3 rad2deg(const Vector3 &p_rotation) {
	return p_rotation / Math_PI * 180.0;
}

String get_rot_order_name(EulerOrder ro) {
	switch (ro) {
		case EulerOrder::XYZ:
			return "XYZ";
		case EulerOrder::XZY:
			return "XZY";
		case EulerOrder::YZX:
			return "YZX";
		case EulerOrder::YXZ:
			return "YXZ";
		case EulerOrder::ZXY:
			return "ZXY";
		case EulerOrder::ZYX:
			return "ZYX";
		default:
			return "[Not supported]";
	}
}

void test_rotation(Vector3 deg_original_euler, EulerOrder rot_order) {
	// This test:
	// 1. Converts the rotation vector from deg to rad.
	// 2. Converts euler to basis.
	// 3. Converts the above basis back into euler.
	// 4. Converts the above euler into basis again.
	// 5. Compares the basis obtained in step 2 with the basis of step 4
	//
	// The conversion "basis to euler", done in the step 3, may be different from
	// the original euler, even if the final rotation are the same.
	// This happens because there are more ways to represents the same rotation,
	// both valid, using eulers.
	// For this reason is necessary to convert that euler back to basis and finally
	// compares it.
	//
	// In this way we can assert that both functions: basis to euler / euler to basis
	// are correct.

	// Euler to rotation
	const Vector3 original_euler = deg_to_rad(deg_original_euler);
	const Basis to_rotation = Basis::from_euler(original_euler, rot_order);

	// Euler from rotation
	const Vector3 euler_from_rotation = to_rotation.get_euler(rot_order);
	const Basis rotation_from_computed_euler = Basis::from_euler(euler_from_rotation, rot_order);

	Basis res = to_rotation.inverse() * rotation_from_computed_euler;

	CHECK_MESSAGE((res.get_column(0) - Vector3(1.0, 0.0, 0.0)).length() <= 0.001, vformat("Fail due to X %s\n", String(res.get_column(0))));
	CHECK_MESSAGE((res.get_column(1) - Vector3(0.0, 1.0, 0.0)).length() <= 0.001, vformat("Fail due to Y %s\n", String(res.get_column(1))));
	CHECK_MESSAGE((res.get_column(2) - Vector3(0.0, 0.0, 1.0)).length() <= 0.001, vformat("Fail due to Z %s\n", String(res.get_column(2))));

	// Double check `to_rotation` decomposing with XYZ rotation order.
	const Vector3 euler_xyz_from_rotation = to_rotation.get_euler(EulerOrder::XYZ);
	Basis rotation_from_xyz_computed_euler = Basis::from_euler(euler_xyz_from_rotation, EulerOrder::XYZ);

	res = to_rotation.inverse() * rotation_from_xyz_computed_euler;

	CHECK_MESSAGE((res.get_column(0) - Vector3(1.0, 0.0, 0.0)).length() <= 0.001, vformat("Double check with XYZ rot order failed, due to X %s\n", String(res.get_column(0))));
	CHECK_MESSAGE((res.get_column(1) - Vector3(0.0, 1.0, 0.0)).length() <= 0.001, vformat("Double check with XYZ rot order failed, due to Y %s\n", String(res.get_column(1))));
	CHECK_MESSAGE((res.get_column(2) - Vector3(0.0, 0.0, 1.0)).length() <= 0.001, vformat("Double check with XYZ rot order failed, due to Z %s\n", String(res.get_column(2))));

	INFO(vformat("Rotation order: %s\n.", get_rot_order_name(rot_order)));
	INFO(vformat("Original Rotation: %s\n", String(deg_original_euler)));
	INFO(vformat("Quaternion to rotation order: %s\n", String(rad2deg(euler_from_rotation))));
}

TEST_CASE("[Basis] Euler conversions") {
	Vector<EulerOrder> euler_order_to_test;
	euler_order_to_test.push_back(EulerOrder::XYZ);
	euler_order_to_test.push_back(EulerOrder::XZY);
	euler_order_to_test.push_back(EulerOrder::YZX);
	euler_order_to_test.push_back(EulerOrder::YXZ);
	euler_order_to_test.push_back(EulerOrder::ZXY);
	euler_order_to_test.push_back(EulerOrder::ZYX);

	Vector<Vector3> vectors_to_test;

	// Test the special cases.
	vectors_to_test.push_back(Vector3(0.0, 0.0, 0.0));
	vectors_to_test.push_back(Vector3(0.5, 0.5, 0.5));
	vectors_to_test.push_back(Vector3(-0.5, -0.5, -0.5));
	vectors_to_test.push_back(Vector3(40.0, 40.0, 40.0));
	vectors_to_test.push_back(Vector3(-40.0, -40.0, -40.0));
	vectors_to_test.push_back(Vector3(0.0, 0.0, -90.0));
	vectors_to_test.push_back(Vector3(0.0, -90.0, 0.0));
	vectors_to_test.push_back(Vector3(-90.0, 0.0, 0.0));
	vectors_to_test.push_back(Vector3(0.0, 0.0, 90.0));
	vectors_to_test.push_back(Vector3(0.0, 90.0, 0.0));
	vectors_to_test.push_back(Vector3(90.0, 0.0, 0.0));
	vectors_to_test.push_back(Vector3(0.0, 0.0, -30.0));
	vectors_to_test.push_back(Vector3(0.0, -30.0, 0.0));
	vectors_to_test.push_back(Vector3(-30.0, 0.0, 0.0));
	vectors_to_test.push_back(Vector3(0.0, 0.0, 30.0));
	vectors_to_test.push_back(Vector3(0.0, 30.0, 0.0));
	vectors_to_test.push_back(Vector3(30.0, 0.0, 0.0));
	vectors_to_test.push_back(Vector3(0.5, 50.0, 20.0));
	vectors_to_test.push_back(Vector3(-0.5, -50.0, -20.0));
	vectors_to_test.push_back(Vector3(0.5, 0.0, 90.0));
	vectors_to_test.push_back(Vector3(0.5, 0.0, -90.0));
	vectors_to_test.push_back(Vector3(360.0, 360.0, 360.0));
	vectors_to_test.push_back(Vector3(-360.0, -360.0, -360.0));
	vectors_to_test.push_back(Vector3(-90.0, 60.0, -90.0));
	vectors_to_test.push_back(Vector3(90.0, 60.0, -90.0));
	vectors_to_test.push_back(Vector3(90.0, -60.0, -90.0));
	vectors_to_test.push_back(Vector3(-90.0, -60.0, -90.0));
	vectors_to_test.push_back(Vector3(-90.0, 60.0, 90.0));
	vectors_to_test.push_back(Vector3(90.0, 60.0, 90.0));
	vectors_to_test.push_back(Vector3(90.0, -60.0, 90.0));
	vectors_to_test.push_back(Vector3(-90.0, -60.0, 90.0));
	vectors_to_test.push_back(Vector3(60.0, 90.0, -40.0));
	vectors_to_test.push_back(Vector3(60.0, -90.0, -40.0));
	vectors_to_test.push_back(Vector3(-60.0, -90.0, -40.0));
	vectors_to_test.push_back(Vector3(-60.0, 90.0, 40.0));
	vectors_to_test.push_back(Vector3(60.0, 90.0, 40.0));
	vectors_to_test.push_back(Vector3(60.0, -90.0, 40.0));
	vectors_to_test.push_back(Vector3(-60.0, -90.0, 40.0));
	vectors_to_test.push_back(Vector3(-90.0, 90.0, -90.0));
	vectors_to_test.push_back(Vector3(90.0, 90.0, -90.0));
	vectors_to_test.push_back(Vector3(90.0, -90.0, -90.0));
	vectors_to_test.push_back(Vector3(-90.0, -90.0, -90.0));
	vectors_to_test.push_back(Vector3(-90.0, 90.0, 90.0));
	vectors_to_test.push_back(Vector3(90.0, 90.0, 90.0));
	vectors_to_test.push_back(Vector3(90.0, -90.0, 90.0));
	vectors_to_test.push_back(Vector3(20.0, 150.0, 30.0));
	vectors_to_test.push_back(Vector3(20.0, -150.0, 30.0));
	vectors_to_test.push_back(Vector3(-120.0, -150.0, 30.0));
	vectors_to_test.push_back(Vector3(-120.0, -150.0, -130.0));
	vectors_to_test.push_back(Vector3(120.0, -150.0, -130.0));
	vectors_to_test.push_back(Vector3(120.0, 150.0, -130.0));
	vectors_to_test.push_back(Vector3(120.0, 150.0, 130.0));
	vectors_to_test.push_back(Vector3(89.9, 0.0, 0.0));
	vectors_to_test.push_back(Vector3(-89.9, 0.0, 0.0));
	vectors_to_test.push_back(Vector3(0.0, 89.9, 0.0));
	vectors_to_test.push_back(Vector3(0.0, -89.9, 0.0));
	vectors_to_test.push_back(Vector3(0.0, 0.0, 89.9));
	vectors_to_test.push_back(Vector3(0.0, 0.0, -89.9));

	for (int h = 0; h < euler_order_to_test.size(); h += 1) {
		for (int i = 0; i < vectors_to_test.size(); i += 1) {
			test_rotation(vectors_to_test[i], euler_order_to_test[h]);
		}
	}
}

TEST_CASE("[Stress][Basis] Euler conversions") {
	Vector<EulerOrder> euler_order_to_test;
	euler_order_to_test.push_back(EulerOrder::XYZ);
	euler_order_to_test.push_back(EulerOrder::XZY);
	euler_order_to_test.push_back(EulerOrder::YZX);
	euler_order_to_test.push_back(EulerOrder::YXZ);
	euler_order_to_test.push_back(EulerOrder::ZXY);
	euler_order_to_test.push_back(EulerOrder::ZYX);

	Vector<Vector3> vectors_to_test;
	// Add 1000 random vectors with weirds numbers.
	RandomNumberGenerator rng;
	for (int _ = 0; _ < 1000; _ += 1) {
		vectors_to_test.push_back(Vector3(
				rng.randf_range(-1800, 1800),
				rng.randf_range(-1800, 1800),
				rng.randf_range(-1800, 1800)));
	}

	for (int h = 0; h < euler_order_to_test.size(); h += 1) {
		for (int i = 0; i < vectors_to_test.size(); i += 1) {
			test_rotation(vectors_to_test[i], euler_order_to_test[h]);
		}
	}
}

TEST_CASE("[Basis] Set axis angle") {
	Vector3 axis;
	real_t angle;
	real_t pi = (real_t)Math_PI;

	// Testing the singularity when the angle is 0°.
	Basis identity(1, 0, 0, 0, 1, 0, 0, 0, 1);
	identity.get_axis_angle(axis, angle);
	CHECK(angle == 0);

	// Testing the singularity when the angle is 180°.
	Basis singularityPi(-1, 0, 0, 0, 1, 0, 0, 0, -1);
	singularityPi.get_axis_angle(axis, angle);
	CHECK(angle == doctest::Approx(pi));

	// Testing reversing the an axis (of an 30° angle).
	float cos30deg = Math::cos(Math::deg_to_rad((real_t)30.0));
	Basis z_positive(cos30deg, -0.5, 0, 0.5, cos30deg, 0, 0, 0, 1);
	Basis z_negative(cos30deg, 0.5, 0, -0.5, cos30deg, 0, 0, 0, 1);

	z_positive.get_axis_angle(axis, angle);
	CHECK(angle == doctest::Approx(Math::deg_to_rad((real_t)30.0)));
	CHECK(axis == Vector3(0, 0, 1));

	z_negative.get_axis_angle(axis, angle);
	CHECK(angle == doctest::Approx(Math::deg_to_rad((real_t)30.0)));
	CHECK(axis == Vector3(0, 0, -1));

	// Testing a rotation of 90° on x-y-z.
	Basis x90deg(1, 0, 0, 0, 0, -1, 0, 1, 0);
	x90deg.get_axis_angle(axis, angle);
	CHECK(angle == doctest::Approx(pi / (real_t)2));
	CHECK(axis == Vector3(1, 0, 0));

	Basis y90deg(0, 0, 1, 0, 1, 0, -1, 0, 0);
	y90deg.get_axis_angle(axis, angle);
	CHECK(axis == Vector3(0, 1, 0));

	Basis z90deg(0, -1, 0, 1, 0, 0, 0, 0, 1);
	z90deg.get_axis_angle(axis, angle);
	CHECK(axis == Vector3(0, 0, 1));

	// Regression test: checks that the method returns a small angle (not 0).
	Basis tiny(1, 0, 0, 0, 0.9999995, -0.001, 0, 001, 0.9999995); // The min angle possible with float is 0.001rad.
	tiny.get_axis_angle(axis, angle);
	CHECK(angle == doctest::Approx(0.001).epsilon(0.0001));

	// Regression test: checks that the method returns an angle which is a number (not NaN)
	Basis bugNan(1.00000024, 0, 0.000100001693, 0, 1, 0, -0.000100009143, 0, 1.00000024);
	bugNan.get_axis_angle(axis, angle);
	CHECK(!Math::is_nan(angle));
}

TEST_CASE("[Basis] Finite number checks") {
	const Vector3 x(0, 1, 2);
	const Vector3 infinite(NAN, NAN, NAN);

	CHECK_MESSAGE(
			Basis(x, x, x).is_finite(),
			"Basis with all components finite should be finite");

	CHECK_FALSE_MESSAGE(
			Basis(infinite, x, x).is_finite(),
			"Basis with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Basis(x, infinite, x).is_finite(),
			"Basis with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Basis(x, x, infinite).is_finite(),
			"Basis with one component infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Basis(infinite, infinite, x).is_finite(),
			"Basis with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Basis(infinite, x, infinite).is_finite(),
			"Basis with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Basis(x, infinite, infinite).is_finite(),
			"Basis with two components infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Basis(infinite, infinite, infinite).is_finite(),
			"Basis with three components infinite should not be finite.");
}

TEST_CASE("[Basis] Is conformal checks") {
	CHECK_MESSAGE(
			Basis().is_conformal(),
			"Identity Basis should be conformal.");

	CHECK_MESSAGE(
			Basis::from_euler(Vector3(1.2, 3.4, 5.6)).is_conformal(),
			"Basis with only rotation should be conformal.");

	CHECK_MESSAGE(
			Basis::from_scale(Vector3(-1, -1, -1)).is_conformal(),
			"Basis with only a flip should be conformal.");

	CHECK_MESSAGE(
			Basis::from_scale(Vector3(1.2, 1.2, 1.2)).is_conformal(),
			"Basis with only uniform scale should be conformal.");

	CHECK_MESSAGE(
			Basis(Vector3(3, 4, 0), Vector3(4, -3, 0.0), Vector3(0, 0, 5)).is_conformal(),
			"Basis with a flip, rotation, and uniform scale should be conformal.");

	CHECK_FALSE_MESSAGE(
			Basis::from_scale(Vector3(1.2, 3.4, 5.6)).is_conformal(),
			"Basis with non-uniform scale should not be conformal.");

	CHECK_FALSE_MESSAGE(
			Basis(Vector3(Math_SQRT12, Math_SQRT12, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)).is_conformal(),
			"Basis with the X axis skewed 45 degrees should not be conformal.");

	CHECK_MESSAGE(
			Basis(0, 0, 0, 0, 0, 0, 0, 0, 0).is_conformal(),
			"Edge case: Basis with all zeroes should return true for is_conformal (because a 0 scale is uniform).");
}

TEST_CASE("[Basis] Is orthogonal checks") {
	CHECK_MESSAGE(
			Basis().is_orthogonal(),
			"Identity Basis should be orthogonal.");

	CHECK_MESSAGE(
			Basis::from_euler(Vector3(1.2, 3.4, 5.6)).is_orthogonal(),
			"Basis with only rotation should be orthogonal.");

	CHECK_MESSAGE(
			Basis::from_scale(Vector3(-1, -1, -1)).is_orthogonal(),
			"Basis with only a flip should be orthogonal.");

	CHECK_MESSAGE(
			Basis::from_scale(Vector3(1.2, 3.4, 5.6)).is_orthogonal(),
			"Basis with only scale should be orthogonal.");

	CHECK_MESSAGE(
			Basis(Vector3(3, 4, 0), Vector3(4, -3, 0), Vector3(0, 0, 5)).is_orthogonal(),
			"Basis with a flip, rotation, and uniform scale should be orthogonal.");

	CHECK_FALSE_MESSAGE(
			Basis(Vector3(Math_SQRT12, Math_SQRT12, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)).is_orthogonal(),
			"Basis with the X axis skewed 45 degrees should not be orthogonal.");

	CHECK_MESSAGE(
			Basis(0, 0, 0, 0, 0, 0, 0, 0, 0).is_orthogonal(),
			"Edge case: Basis with all zeroes should return true for is_orthogonal, since zero vectors are orthogonal to all vectors.");
}

TEST_CASE("[Basis] Is orthonormal checks") {
	CHECK_MESSAGE(
			Basis().is_orthonormal(),
			"Identity Basis should be orthonormal.");

	CHECK_MESSAGE(
			Basis::from_euler(Vector3(1.2, 3.4, 5.6)).is_orthonormal(),
			"Basis with only rotation should be orthonormal.");

	CHECK_MESSAGE(
			Basis::from_scale(Vector3(-1, -1, -1)).is_orthonormal(),
			"Basis with only a flip should be orthonormal.");

	CHECK_FALSE_MESSAGE(
			Basis::from_scale(Vector3(1.2, 3.4, 5.6)).is_orthonormal(),
			"Basis with only scale should not be orthonormal.");

	CHECK_FALSE_MESSAGE(
			Basis(Vector3(3, 4, 0), Vector3(4, -3, 0), Vector3(0, 0, 5)).is_orthonormal(),
			"Basis with a flip, rotation, and uniform scale should not be orthonormal.");

	CHECK_FALSE_MESSAGE(
			Basis(Vector3(Math_SQRT12, Math_SQRT12, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)).is_orthonormal(),
			"Basis with the X axis skewed 45 degrees should not be orthonormal.");

	CHECK_FALSE_MESSAGE(
			Basis(0, 0, 0, 0, 0, 0, 0, 0, 0).is_orthonormal(),
			"Edge case: Basis with all zeroes should return false for is_orthonormal, since the vectors do not have a length of 1.");
}

TEST_CASE("[Basis] Is rotation checks") {
	CHECK_MESSAGE(
			Basis().is_rotation(),
			"Identity Basis should be a rotation (a rotation of zero).");

	CHECK_MESSAGE(
			Basis::from_euler(Vector3(1.2, 3.4, 5.6)).is_rotation(),
			"Basis with only rotation should be a rotation.");

	CHECK_FALSE_MESSAGE(
			Basis::from_scale(Vector3(-1, -1, -1)).is_rotation(),
			"Basis with only a flip should not be a rotation.");

	CHECK_FALSE_MESSAGE(
			Basis::from_scale(Vector3(1.2, 3.4, 5.6)).is_rotation(),
			"Basis with only scale should not be a rotation.");

	CHECK_FALSE_MESSAGE(
			Basis(Vector3(2, 0, 0), Vector3(0, 0.5, 0), Vector3(0, 0, 1)).is_rotation(),
			"Basis with a squeeze should not be a rotation.");

	CHECK_FALSE_MESSAGE(
			Basis(Vector3(Math_SQRT12, Math_SQRT12, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)).is_rotation(),
			"Basis with the X axis skewed 45 degrees should not be a rotation.");

	CHECK_FALSE_MESSAGE(
			Basis(0, 0, 0, 0, 0, 0, 0, 0, 0).is_rotation(),
			"Edge case: Basis with all zeroes should return false for is_rotation, because it is not just a rotation (has a scale of 0).");
}

} // namespace TestBasis
