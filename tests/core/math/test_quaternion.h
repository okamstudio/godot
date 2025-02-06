/**************************************************************************/
/*  test_quaternion.h                                                     */
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

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/math/quaternion.h"
#include "core/math/vector3.h"

#include "tests/test_macros.h"

namespace TestQuaternion {

Quaternion quat_euler_yxz_deg(Vector3 angle) {
	double yaw = Math::deg_to_rad(angle[1]);
	double pitch = Math::deg_to_rad(angle[0]);
	double roll = Math::deg_to_rad(angle[2]);

	// Generate YXZ (Z-then-X-then-Y) Quaternion using single-axis Euler
	// constructor and quaternion product, both tested separately.
	Quaternion q_y = Quaternion::from_euler(Vector3(0.0, yaw, 0.0));
	Quaternion q_p = Quaternion::from_euler(Vector3(pitch, 0.0, 0.0));
	Quaternion q_r = Quaternion::from_euler(Vector3(0.0, 0.0, roll));
	// Roll-Z is followed by Pitch-X, then Yaw-Y.
	Quaternion q_yxz = q_y * q_p * q_r;

	return q_yxz;
}

TEST_CASE("[Quaternion] Default Construct") {
	Quaternion q;

	CHECK(q[0] == 0.0);
	CHECK(q[1] == 0.0);
	CHECK(q[2] == 0.0);
	CHECK(q[3] == 1.0);
}

TEST_CASE("[Quaternion] Construct x,y,z,w") {
	// Values are taken from actual use in another project & are valid (except roundoff error).
	Quaternion q(0.2391, 0.099, 0.3696, 0.8924);

	CHECK(q[0] == doctest::Approx(0.2391));
	CHECK(q[1] == doctest::Approx(0.099));
	CHECK(q[2] == doctest::Approx(0.3696));
	CHECK(q[3] == doctest::Approx(0.8924));
}

TEST_CASE("[Quaternion] Construct AxisAngle 1") {
	// Easy to visualize: 120 deg about X-axis.
	Quaternion q(Vector3(1.0, 0.0, 0.0), Math::deg_to_rad(120.0));

	// 0.866 isn't close enough; doctest::Approx doesn't cut much slack!
	CHECK(q[0] == doctest::Approx(0.866025)); // Sine of half the angle.
	CHECK(q[1] == doctest::Approx(0.0));
	CHECK(q[2] == doctest::Approx(0.0));
	CHECK(q[3] == doctest::Approx(0.5)); // Cosine of half the angle.
}

TEST_CASE("[Quaternion] Construct AxisAngle 2") {
	// Easy to visualize: 30 deg about Y-axis.
	Quaternion q(Vector3(0.0, 1.0, 0.0), Math::deg_to_rad(30.0));

	CHECK(q[0] == doctest::Approx(0.0));
	CHECK(q[1] == doctest::Approx(0.258819)); // Sine of half the angle.
	CHECK(q[2] == doctest::Approx(0.0));
	CHECK(q[3] == doctest::Approx(0.965926)); // Cosine of half the angle.
}

TEST_CASE("[Quaternion] Construct AxisAngle 3") {
	// Easy to visualize: 60 deg about Z-axis.
	Quaternion q(Vector3(0.0, 0.0, 1.0), Math::deg_to_rad(60.0));

	CHECK(q[0] == doctest::Approx(0.0));
	CHECK(q[1] == doctest::Approx(0.0));
	CHECK(q[2] == doctest::Approx(0.5)); // Sine of half the angle.
	CHECK(q[3] == doctest::Approx(0.866025)); // Cosine of half the angle.
}

TEST_CASE("[Quaternion] Construct AxisAngle 4") {
	// More complex & hard to visualize, so test w/ data from online calculator.
	Vector3 axis(1.0, 2.0, 0.5);
	Quaternion q(axis.normalized(), Math::deg_to_rad(35.0));

	CHECK(q[0] == doctest::Approx(0.131239));
	CHECK(q[1] == doctest::Approx(0.262478));
	CHECK(q[2] == doctest::Approx(0.0656194));
	CHECK(q[3] == doctest::Approx(0.953717));
}

TEST_CASE("[Quaternion] Construct from Quaternion") {
	Vector3 axis(1.0, 2.0, 0.5);
	Quaternion q_src(axis.normalized(), Math::deg_to_rad(35.0));
	Quaternion q(q_src);

	CHECK(q[0] == doctest::Approx(0.131239));
	CHECK(q[1] == doctest::Approx(0.262478));
	CHECK(q[2] == doctest::Approx(0.0656194));
	CHECK(q[3] == doctest::Approx(0.953717));
}

TEST_CASE("[Quaternion] Construct Euler SingleAxis") {
	double yaw = Math::deg_to_rad(45.0);
	double pitch = Math::deg_to_rad(30.0);
	double roll = Math::deg_to_rad(10.0);

	Vector3 euler_y(0.0, yaw, 0.0);
	Quaternion q_y = Quaternion::from_euler(euler_y);
	CHECK(q_y[0] == doctest::Approx(0.0));
	CHECK(q_y[1] == doctest::Approx(0.382684));
	CHECK(q_y[2] == doctest::Approx(0.0));
	CHECK(q_y[3] == doctest::Approx(0.923879));

	Vector3 euler_p(pitch, 0.0, 0.0);
	Quaternion q_p = Quaternion::from_euler(euler_p);
	CHECK(q_p[0] == doctest::Approx(0.258819));
	CHECK(q_p[1] == doctest::Approx(0.0));
	CHECK(q_p[2] == doctest::Approx(0.0));
	CHECK(q_p[3] == doctest::Approx(0.965926));

	Vector3 euler_r(0.0, 0.0, roll);
	Quaternion q_r = Quaternion::from_euler(euler_r);
	CHECK(q_r[0] == doctest::Approx(0.0));
	CHECK(q_r[1] == doctest::Approx(0.0));
	CHECK(q_r[2] == doctest::Approx(0.0871558));
	CHECK(q_r[3] == doctest::Approx(0.996195));
}

TEST_CASE("[Quaternion] Construct Euler YXZ dynamic axes") {
	double yaw = Math::deg_to_rad(45.0);
	double pitch = Math::deg_to_rad(30.0);
	double roll = Math::deg_to_rad(10.0);

	// Generate YXZ comparison data (Z-then-X-then-Y) using single-axis Euler
	// constructor and quaternion product, both tested separately.
	Vector3 euler_y(0.0, yaw, 0.0);
	Quaternion q_y = Quaternion::from_euler(euler_y);
	Vector3 euler_p(pitch, 0.0, 0.0);
	Quaternion q_p = Quaternion::from_euler(euler_p);
	Vector3 euler_r(0.0, 0.0, roll);
	Quaternion q_r = Quaternion::from_euler(euler_r);

	// Instrinsically, Yaw-Y then Pitch-X then Roll-Z.
	// Extrinsically, Roll-Z is followed by Pitch-X, then Yaw-Y.
	Quaternion check_yxz = q_y * q_p * q_r;

	// Test construction from YXZ Euler angles.
	Vector3 euler_yxz(pitch, yaw, roll);
	Quaternion q = Quaternion::from_euler(euler_yxz);
	CHECK(q[0] == doctest::Approx(check_yxz[0]));
	CHECK(q[1] == doctest::Approx(check_yxz[1]));
	CHECK(q[2] == doctest::Approx(check_yxz[2]));
	CHECK(q[3] == doctest::Approx(check_yxz[3]));

	CHECK(q.is_equal_approx(check_yxz));
	CHECK(q.get_euler().is_equal_approx(euler_yxz));
	CHECK(check_yxz.get_euler().is_equal_approx(euler_yxz));
}

TEST_CASE("[Quaternion] Construct Basis Euler") {
	double yaw = Math::deg_to_rad(45.0);
	double pitch = Math::deg_to_rad(30.0);
	double roll = Math::deg_to_rad(10.0);
	Vector3 euler_yxz(pitch, yaw, roll);
	Quaternion q_yxz = Quaternion::from_euler(euler_yxz);
	Basis basis_axes = Basis::from_euler(euler_yxz);
	Quaternion q(basis_axes);
	CHECK(q.is_equal_approx(q_yxz));
}

TEST_CASE("[Quaternion] Construct Basis Axes") {
	// Arbitrary Euler angles.
	Vector3 euler_yxz(Math::deg_to_rad(31.41), Math::deg_to_rad(-49.16), Math::deg_to_rad(12.34));
	// Basis vectors from online calculation of rotation matrix.
	Vector3 i_unit(0.5545787, 0.1823950, 0.8118957);
	Vector3 j_unit(-0.5249245, 0.8337420, 0.1712555);
	Vector3 k_unit(-0.6456754, -0.5211586, 0.5581192);
	// Quaternion from online calculation.
	Quaternion q_calc(0.2016913, -0.4245716, 0.206033, 0.8582598);
	// Quaternion from local calculation.
	Quaternion q_local = quat_euler_yxz_deg(Vector3(31.41, -49.16, 12.34));
	// Quaternion from Euler angles constructor.
	Quaternion q_euler = Quaternion::from_euler(euler_yxz);
	CHECK(q_calc.is_equal_approx(q_local));
	CHECK(q_local.is_equal_approx(q_euler));

	// Calculate Basis and construct Quaternion.
	// When this is written, C++ Basis class does not construct from basis vectors.
	// This is by design, but may be subject to change.
	// Workaround by constructing Basis from Euler angles.
	// basis_axes = Basis(i_unit, j_unit, k_unit);
	Basis basis_axes = Basis::from_euler(euler_yxz);
	Quaternion q(basis_axes);

	CHECK(basis_axes.get_column(0).is_equal_approx(i_unit));
	CHECK(basis_axes.get_column(1).is_equal_approx(j_unit));
	CHECK(basis_axes.get_column(2).is_equal_approx(k_unit));

	CHECK(q.is_equal_approx(q_calc));
	CHECK_FALSE(q.inverse().is_equal_approx(q_calc));
	CHECK(q.is_equal_approx(q_local));
	CHECK(q.is_equal_approx(q_euler));
	CHECK(q[0] == doctest::Approx(0.2016913));
	CHECK(q[1] == doctest::Approx(-0.4245716));
	CHECK(q[2] == doctest::Approx(0.206033));
	CHECK(q[3] == doctest::Approx(0.8582598));
}

TEST_CASE("[Quaternion] Construct Shortest Arc For 180 Degree Arc") {
	Vector3 up(0, 1, 0);
	Vector3 down(0, -1, 0);
	Vector3 left(-1, 0, 0);
	Vector3 right(1, 0, 0);
	Vector3 forward(0, 0, -1);
	Vector3 back(0, 0, 1);

	// When we have a 180 degree rotation quaternion which was defined as
	// A to B, logically when we transform A we expect to get B.
	Quaternion left_to_right(left, right);
	Quaternion right_to_left(right, left);
	CHECK(left_to_right.xform(left).is_equal_approx(right));
	CHECK(Quaternion(right, left).xform(right).is_equal_approx(left));
	CHECK(Quaternion(up, down).xform(up).is_equal_approx(down));
	CHECK(Quaternion(down, up).xform(down).is_equal_approx(up));
	CHECK(Quaternion(forward, back).xform(forward).is_equal_approx(back));
	CHECK(Quaternion(back, forward).xform(back).is_equal_approx(forward));

	// With (arbitrary) opposite vectors that are not axis-aligned as parameters.
	Vector3 diagonal_up = Vector3(1.2, 2.3, 4.5).normalized();
	Vector3 diagonal_down = -diagonal_up;
	Quaternion q1(diagonal_up, diagonal_down);
	CHECK(q1.xform(diagonal_down).is_equal_approx(diagonal_up));
	CHECK(q1.xform(diagonal_up).is_equal_approx(diagonal_down));

	// For the consistency of the rotation direction, they should be symmetrical to the plane.
	CHECK(left_to_right.is_equal_approx(right_to_left.inverse()));

	// If vectors are same, no rotation.
	CHECK(Quaternion(diagonal_up, diagonal_up).is_equal_approx(Quaternion()));
}

TEST_CASE("[Quaternion] Get Euler Orders") {
	double x = Math::deg_to_rad(30.0);
	double y = Math::deg_to_rad(45.0);
	double z = Math::deg_to_rad(10.0);
	Vector3 euler(x, y, z);
	for (int i = 0; i < 6; i++) {
		EulerOrder order = (EulerOrder)i;
		Basis basis = Basis::from_euler(euler, order);
		Quaternion q = Quaternion(basis);
		Vector3 check = q.get_euler(order);
		CHECK_MESSAGE(check.is_equal_approx(euler),
				"Quaternion get_euler method should return the original angles.");
		CHECK_MESSAGE(check.is_equal_approx(basis.get_euler(order)),
				"Quaternion get_euler method should behave the same as Basis get_euler.");
	}
}

TEST_CASE("[Quaternion] Product (book)") {
	// Example from "Quaternions and Rotation Sequences" by Jack Kuipers, p. 108.
	Quaternion p(1.0, -2.0, 1.0, 3.0);
	Quaternion q(-1.0, 2.0, 3.0, 2.0);

	Quaternion pq = p * q;
	CHECK(pq[0] == doctest::Approx(-9.0));
	CHECK(pq[1] == doctest::Approx(-2.0));
	CHECK(pq[2] == doctest::Approx(11.0));
	CHECK(pq[3] == doctest::Approx(8.0));
}

TEST_CASE("[Quaternion] Product") {
	double yaw = Math::deg_to_rad(45.0);
	double pitch = Math::deg_to_rad(30.0);
	double roll = Math::deg_to_rad(10.0);

	Vector3 euler_y(0.0, yaw, 0.0);
	Quaternion q_y = Quaternion::from_euler(euler_y);
	CHECK(q_y[0] == doctest::Approx(0.0));
	CHECK(q_y[1] == doctest::Approx(0.382684));
	CHECK(q_y[2] == doctest::Approx(0.0));
	CHECK(q_y[3] == doctest::Approx(0.923879));

	Vector3 euler_p(pitch, 0.0, 0.0);
	Quaternion q_p = Quaternion::from_euler(euler_p);
	CHECK(q_p[0] == doctest::Approx(0.258819));
	CHECK(q_p[1] == doctest::Approx(0.0));
	CHECK(q_p[2] == doctest::Approx(0.0));
	CHECK(q_p[3] == doctest::Approx(0.965926));

	Vector3 euler_r(0.0, 0.0, roll);
	Quaternion q_r = Quaternion::from_euler(euler_r);
	CHECK(q_r[0] == doctest::Approx(0.0));
	CHECK(q_r[1] == doctest::Approx(0.0));
	CHECK(q_r[2] == doctest::Approx(0.0871558));
	CHECK(q_r[3] == doctest::Approx(0.996195));

	// Test ZYX dynamic-axes since test data is available online.
	// Rotate first about X axis, then new Y axis, then new Z axis.
	// (Godot uses YXZ Yaw-Pitch-Roll order).
	Quaternion q_yp = q_y * q_p;
	CHECK(q_yp[0] == doctest::Approx(0.239118));
	CHECK(q_yp[1] == doctest::Approx(0.369644));
	CHECK(q_yp[2] == doctest::Approx(-0.099046));
	CHECK(q_yp[3] == doctest::Approx(0.892399));

	Quaternion q_ryp = q_r * q_yp;
	CHECK(q_ryp[0] == doctest::Approx(0.205991));
	CHECK(q_ryp[1] == doctest::Approx(0.389078));
	CHECK(q_ryp[2] == doctest::Approx(-0.0208912));
	CHECK(q_ryp[3] == doctest::Approx(0.897636));
}

TEST_CASE("[Quaternion] xform unit vectors") {
	// Easy to visualize: 120 deg about X-axis.
	// Transform the i, j, & k unit vectors.
	Quaternion q(Vector3(1.0, 0.0, 0.0), Math::deg_to_rad(120.0));
	Vector3 i_t = q.xform(Vector3(1.0, 0.0, 0.0));
	Vector3 j_t = q.xform(Vector3(0.0, 1.0, 0.0));
	Vector3 k_t = q.xform(Vector3(0.0, 0.0, 1.0));
	//
	CHECK(i_t.is_equal_approx(Vector3(1.0, 0.0, 0.0)));
	CHECK(j_t.is_equal_approx(Vector3(0.0, -0.5, 0.866025)));
	CHECK(k_t.is_equal_approx(Vector3(0.0, -0.866025, -0.5)));
	CHECK(i_t.length_squared() == doctest::Approx(1.0));
	CHECK(j_t.length_squared() == doctest::Approx(1.0));
	CHECK(k_t.length_squared() == doctest::Approx(1.0));

	// Easy to visualize: 30 deg about Y-axis.
	q = Quaternion(Vector3(0.0, 1.0, 0.0), Math::deg_to_rad(30.0));
	i_t = q.xform(Vector3(1.0, 0.0, 0.0));
	j_t = q.xform(Vector3(0.0, 1.0, 0.0));
	k_t = q.xform(Vector3(0.0, 0.0, 1.0));
	//
	CHECK(i_t.is_equal_approx(Vector3(0.866025, 0.0, -0.5)));
	CHECK(j_t.is_equal_approx(Vector3(0.0, 1.0, 0.0)));
	CHECK(k_t.is_equal_approx(Vector3(0.5, 0.0, 0.866025)));
	CHECK(i_t.length_squared() == doctest::Approx(1.0));
	CHECK(j_t.length_squared() == doctest::Approx(1.0));
	CHECK(k_t.length_squared() == doctest::Approx(1.0));

	// Easy to visualize: 60 deg about Z-axis.
	q = Quaternion(Vector3(0.0, 0.0, 1.0), Math::deg_to_rad(60.0));
	i_t = q.xform(Vector3(1.0, 0.0, 0.0));
	j_t = q.xform(Vector3(0.0, 1.0, 0.0));
	k_t = q.xform(Vector3(0.0, 0.0, 1.0));
	//
	CHECK(i_t.is_equal_approx(Vector3(0.5, 0.866025, 0.0)));
	CHECK(j_t.is_equal_approx(Vector3(-0.866025, 0.5, 0.0)));
	CHECK(k_t.is_equal_approx(Vector3(0.0, 0.0, 1.0)));
	CHECK(i_t.length_squared() == doctest::Approx(1.0));
	CHECK(j_t.length_squared() == doctest::Approx(1.0));
	CHECK(k_t.length_squared() == doctest::Approx(1.0));
}

TEST_CASE("[Quaternion] xform vector") {
	// Arbitrary quaternion rotates an arbitrary vector.
	Vector3 euler_yzx(Math::deg_to_rad(31.41), Math::deg_to_rad(-49.16), Math::deg_to_rad(12.34));
	Basis basis_axes = Basis::from_euler(euler_yzx);
	Quaternion q(basis_axes);

	Vector3 v_arb(3.0, 4.0, 5.0);
	Vector3 v_rot = q.xform(v_arb);
	Vector3 v_compare = basis_axes.xform(v_arb);

	CHECK(v_rot.length_squared() == doctest::Approx(v_arb.length_squared()));
	CHECK(v_rot.is_equal_approx(v_compare));
}

// Test vector xform for a single combination of Quaternion and Vector.
void test_quat_vec_rotate(Vector3 euler_yzx, Vector3 v_in) {
	Basis basis_axes = Basis::from_euler(euler_yzx);
	Quaternion q(basis_axes);

	Vector3 v_rot = q.xform(v_in);
	Vector3 v_compare = basis_axes.xform(v_in);

	CHECK(v_rot.length_squared() == doctest::Approx(v_in.length_squared()));
	CHECK(v_rot.is_equal_approx(v_compare));
}

TEST_CASE("[Stress][Quaternion] Many vector xforms") {
	// Many arbitrary quaternions rotate many arbitrary vectors.
	// For each trial, check that rotation by Quaternion yields same result as
	// rotation by Basis.
	const int STEPS = 100; // Number of test steps in each dimension
	const double delta = 2.0 * Math_PI / STEPS; // Angle increment per step
	const double delta_vec = 20.0 / STEPS; // Vector increment per step
	Vector3 vec_arb(1.0, 1.0, 1.0);
	double x_angle = -Math_PI;
	double y_angle = -Math_PI;
	double z_angle = -Math_PI;
	for (double i = 0; i < STEPS; ++i) {
		vec_arb[0] = -10.0 + i * delta_vec;
		x_angle = i * delta - Math_PI;
		for (double j = 0; j < STEPS; ++j) {
			vec_arb[1] = -10.0 + j * delta_vec;
			y_angle = j * delta - Math_PI;
			for (double k = 0; k < STEPS; ++k) {
				vec_arb[2] = -10.0 + k * delta_vec;
				z_angle = k * delta - Math_PI;
				Vector3 euler_yzx(x_angle, y_angle, z_angle);
				test_quat_vec_rotate(euler_yzx, vec_arb);
			}
		}
	}
}

TEST_CASE("[Quaternion] Finite number checks") {
	const real_t x = NAN;

	CHECK_MESSAGE(
			Quaternion(0, 1, 2, 3).is_finite(),
			"Quaternion with all components finite should be finite");

	CHECK_FALSE_MESSAGE(
			Quaternion(x, 1, 2, 3).is_finite(),
			"Quaternion with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(0, x, 2, 3).is_finite(),
			"Quaternion with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(0, 1, x, 3).is_finite(),
			"Quaternion with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(0, 1, 2, x).is_finite(),
			"Quaternion with one component infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Quaternion(x, x, 2, 3).is_finite(),
			"Quaternion with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(x, 1, x, 3).is_finite(),
			"Quaternion with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(x, 1, 2, x).is_finite(),
			"Quaternion with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(0, x, x, 3).is_finite(),
			"Quaternion with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(0, x, 2, x).is_finite(),
			"Quaternion with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(0, 1, x, x).is_finite(),
			"Quaternion with two components infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Quaternion(0, x, x, x).is_finite(),
			"Quaternion with three components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(x, 1, x, x).is_finite(),
			"Quaternion with three components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(x, x, 2, x).is_finite(),
			"Quaternion with three components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Quaternion(x, x, x, 3).is_finite(),
			"Quaternion with three components infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Quaternion(x, x, x, x).is_finite(),
			"Quaternion with four components infinite should not be finite.");
}

} // namespace TestQuaternion
