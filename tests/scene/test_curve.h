/**************************************************************************/
/*  test_curve.h                                                          */
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

#ifndef TEST_CURVE_H
#define TEST_CURVE_H

#include "core/math/math_funcs.h"
#include "scene/resources/curve.h"

#include "tests/test_macros.h"

namespace TestCurve {

TEST_CASE("[Curve] Default curve") {
	const Ref<Curve> curve = memnew(Curve);

	CHECK_MESSAGE(
			curve->get_point_count() == 0,
			"Default curve should contain the expected number of points.");
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample(0)),
			"Default curve should return the expected value at offset 0.0.");
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample(0.5)),
			"Default curve should return the expected value at offset 0.5.");
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample(1)),
			"Default curve should return the expected value at offset 1.0.");
}

TEST_CASE("[Curve] Custom unit curve with free tangents") {
	Ref<Curve> curve = memnew(Curve);
	// "Sawtooth" curve with an open ending towards the 1.0 offset.
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(0.25, 1));
	curve->add_point(Vector2(0.5, 0));
	curve->add_point(Vector2(0.75, 1));
	curve->set_bake_resolution(11);

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->get_point_left_tangent(0)),
			"get_point_left_tangent() should return the expected value for point index 0.");
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->get_point_right_tangent(0)),
			"get_point_right_tangent() should return the expected value for point index 0.");
	CHECK_MESSAGE(
			curve->get_point_left_mode(0) == Curve::TangentMode::TANGENT_FREE,
			"get_point_left_mode() should return the expected value for point index 0.");
	CHECK_MESSAGE(
			curve->get_point_right_mode(0) == Curve::TangentMode::TANGENT_FREE,
			"get_point_right_mode() should return the expected value for point index 0.");

	CHECK_MESSAGE(
			curve->get_point_count() == 4,
			"Custom free curve should contain the expected number of points.");

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample(-0.1)),
			"Custom free curve should return the expected value at offset -0.1.");
	CHECK_MESSAGE(
			curve->sample(0.1) == doctest::Approx((real_t)0.352),
			"Custom free curve should return the expected value at offset 0.1.");
	CHECK_MESSAGE(
			curve->sample(0.4) == doctest::Approx((real_t)0.352),
			"Custom free curve should return the expected value at offset 0.4.");
	CHECK_MESSAGE(
			curve->sample(0.7) == doctest::Approx((real_t)0.896),
			"Custom free curve should return the expected value at offset 0.7.");
	CHECK_MESSAGE(
			curve->sample(1) == doctest::Approx(1),
			"Custom free curve should return the expected value at offset 1.");
	CHECK_MESSAGE(
			curve->sample(2) == doctest::Approx(1),
			"Custom free curve should return the expected value at offset 2.");

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample_baked(-0.1)),
			"Custom free curve should return the expected baked value at offset -0.1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.1) == doctest::Approx((real_t)0.352),
			"Custom free curve should return the expected baked value at offset 0.1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.4) == doctest::Approx((real_t)0.352),
			"Custom free curve should return the expected baked value at offset 0.4.");
	CHECK_MESSAGE(
			curve->sample_baked(0.7) == doctest::Approx((real_t)0.896),
			"Custom free curve should return the expected baked value at offset 0.7.");
	CHECK_MESSAGE(
			curve->sample_baked(1) == doctest::Approx(1),
			"Custom free curve should return the expected baked value at offset 1.");
	CHECK_MESSAGE(
			curve->sample_baked(2) == doctest::Approx(1),
			"Custom free curve should return the expected baked value at offset 2.");

	curve->remove_point(1);
	CHECK_MESSAGE(
			curve->sample(0.1) == doctest::Approx(0),
			"Custom free curve should return the expected value at offset 0.1 after removing point at index 1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.1) == doctest::Approx(0),
			"Custom free curve should return the expected baked value at offset 0.1 after removing point at index 1.");

	curve->clear_points();
	CHECK_MESSAGE(
			curve->sample(0.6) == doctest::Approx(0),
			"Custom free curve should return the expected value at offset 0.6 after clearing all points.");
	CHECK_MESSAGE(
			curve->sample_baked(0.6) == doctest::Approx(0),
			"Custom free curve should return the expected baked value at offset 0.6 after clearing all points.");
}

TEST_CASE("[Curve] Custom non-unit curve with free tangents") {
	Ref<Curve> curve = memnew(Curve);
	curve->set_min_domain(-100.0);
	curve->set_max_domain(100.0);
	// "Sawtooth" curve with an open ending towards the 100 offset.
	curve->add_point(Vector2(-100, 0));
	curve->add_point(Vector2(-50, 1));
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(50, 1));
	curve->set_bake_resolution(11);

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->get_point_left_tangent(0)),
			"get_point_left_tangent() should return the expected value for point index 0.");
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->get_point_right_tangent(0)),
			"get_point_right_tangent() should return the expected value for point index 0.");
	CHECK_MESSAGE(
			curve->get_point_left_mode(0) == Curve::TangentMode::TANGENT_FREE,
			"get_point_left_mode() should return the expected value for point index 0.");
	CHECK_MESSAGE(
			curve->get_point_right_mode(0) == Curve::TangentMode::TANGENT_FREE,
			"get_point_right_mode() should return the expected value for point index 0.");

	CHECK_MESSAGE(
			curve->get_point_count() == 4,
			"Custom free curve should contain the expected number of points.");

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample(-200)),
			"Custom free curve should return the expected value at offset -200.");
	CHECK_MESSAGE(
			curve->sample(0.1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.352),
			"Custom free curve should return the expected value at offset equivalent to a unit curve's 0.1.");
	CHECK_MESSAGE(
			curve->sample(0.4 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.352),
			"Custom free curve should return the expected value at offset equivalent to a unit curve's 0.4.");
	CHECK_MESSAGE(
			curve->sample(0.7 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.896),
			"Custom free curve should return the expected value at offset equivalent to a unit curve's 0.7.");
	CHECK_MESSAGE(
			curve->sample(1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(1),
			"Custom free curve should return the expected value at offset equivalent to a unit curve's 1.");
	CHECK_MESSAGE(
			curve->sample(2 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(1),
			"Custom free curve should return the expected value at offset equivalent to a unit curve's 2.");

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample_baked(-200)),
			"Custom free curve should return the expected baked value at offset equivalent to a unit curve's -0.1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.352),
			"Custom free curve should return the expected baked value at offset equivalent to a unit curve's 0.1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.4 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.352),
			"Custom free curve should return the expected baked value at offset equivalent to a unit curve's 0.4.");
	CHECK_MESSAGE(
			curve->sample_baked(0.7 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.896),
			"Custom free curve should return the expected baked value at offset equivalent to a unit curve's 0.7.");
	CHECK_MESSAGE(
			curve->sample_baked(1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(1),
			"Custom free curve should return the expected baked value at offset equivalent to a unit curve's 1.");
	CHECK_MESSAGE(
			curve->sample_baked(2 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(1),
			"Custom free curve should return the expected baked value at offset equivalent to a unit curve's 2.");

	curve->remove_point(1);
	CHECK_MESSAGE(
			curve->sample(0.1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(0),
			"Custom free curve should return the expected value at offset equivalent to a unit curve's 0.1 after removing point at index 1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(0),
			"Custom free curve should return the expected baked value at offset equivalent to a unit curve's 0.1 after removing point at index 1.");

	curve->clear_points();
	CHECK_MESSAGE(
			curve->sample(0.6 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(0),
			"Custom free curve should return the expected value at offset 0.6 after clearing all points.");
	CHECK_MESSAGE(
			curve->sample_baked(0.6 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(0),
			"Custom free curve should return the expected baked value at offset 0.6 after clearing all points.");
}

TEST_CASE("[Curve] Custom unit curve with linear tangents") {
	Ref<Curve> curve = memnew(Curve);
	// "Sawtooth" curve with an open ending towards the 1.0 offset.
	curve->add_point(Vector2(0, 0), 0, 0, Curve::TangentMode::TANGENT_LINEAR, Curve::TangentMode::TANGENT_LINEAR);
	curve->add_point(Vector2(0.25, 1), 0, 0, Curve::TangentMode::TANGENT_LINEAR, Curve::TangentMode::TANGENT_LINEAR);
	curve->add_point(Vector2(0.5, 0), 0, 0, Curve::TangentMode::TANGENT_LINEAR, Curve::TangentMode::TANGENT_LINEAR);
	curve->add_point(Vector2(0.75, 1), 0, 0, Curve::TangentMode::TANGENT_LINEAR, Curve::TangentMode::TANGENT_LINEAR);

	CHECK_MESSAGE(
			curve->get_point_left_tangent(3) == doctest::Approx(4),
			"get_point_left_tangent() should return the expected value for point index 3.");
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->get_point_right_tangent(3)),
			"get_point_right_tangent() should return the expected value for point index 3.");
	CHECK_MESSAGE(
			curve->get_point_left_mode(3) == Curve::TangentMode::TANGENT_LINEAR,
			"get_point_left_mode() should return the expected value for point index 3.");
	CHECK_MESSAGE(
			curve->get_point_right_mode(3) == Curve::TangentMode::TANGENT_LINEAR,
			"get_point_right_mode() should return the expected value for point index 3.");

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->get_point_right_tangent(300)),
			"get_point_right_tangent() should return the expected value for invalid point index 300.");
	CHECK_MESSAGE(
			curve->get_point_left_mode(-12345) == Curve::TangentMode::TANGENT_FREE,
			"get_point_left_mode() should return the expected value for invalid point index -12345.");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			curve->get_point_count() == 4,
			"Custom linear curve should contain the expected number of points.");

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample(-0.1)),
			"Custom linear curve should return the expected value at offset -0.1.");
	CHECK_MESSAGE(
			curve->sample(0.1) == doctest::Approx((real_t)0.4),
			"Custom linear curve should return the expected value at offset 0.1.");
	CHECK_MESSAGE(
			curve->sample(0.4) == doctest::Approx((real_t)0.4),
			"Custom linear curve should return the expected value at offset 0.4.");
	CHECK_MESSAGE(
			curve->sample(0.7) == doctest::Approx((real_t)0.8),
			"Custom linear curve should return the expected value at offset 0.7.");
	CHECK_MESSAGE(
			curve->sample(1) == doctest::Approx(1),
			"Custom linear curve should return the expected value at offset 1.0.");
	CHECK_MESSAGE(
			curve->sample(2) == doctest::Approx(1),
			"Custom linear curve should return the expected value at offset 2.0.");

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample_baked(-0.1)),
			"Custom linear curve should return the expected baked value at offset -0.1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.1) == doctest::Approx((real_t)0.4),
			"Custom linear curve should return the expected baked value at offset 0.1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.4) == doctest::Approx((real_t)0.4),
			"Custom linear curve should return the expected baked value at offset 0.4.");
	CHECK_MESSAGE(
			curve->sample_baked(0.7) == doctest::Approx((real_t)0.8),
			"Custom linear curve should return the expected baked value at offset 0.7.");
	CHECK_MESSAGE(
			curve->sample_baked(1) == doctest::Approx(1),
			"Custom linear curve should return the expected baked value at offset 1.0.");
	CHECK_MESSAGE(
			curve->sample_baked(2) == doctest::Approx(1),
			"Custom linear curve should return the expected baked value at offset 2.0.");

	ERR_PRINT_OFF;
	curve->remove_point(10);
	ERR_PRINT_ON;
	CHECK_MESSAGE(
			curve->sample(0.7) == doctest::Approx((real_t)0.8),
			"Custom free curve should return the expected value at offset 0.7 after removing point at invalid index 10.");
	CHECK_MESSAGE(
			curve->sample_baked(0.7) == doctest::Approx((real_t)0.8),
			"Custom free curve should return the expected baked value at offset 0.7 after removing point at invalid index 10.");
}

TEST_CASE("[Curve] Custom non-unit curve with linear tangents") {
	Ref<Curve> curve = memnew(Curve);
	curve->set_min_domain(-100.0);
	curve->set_max_domain(100.0);
	// "Sawtooth" curve with an open ending towards the 100 offset.
	curve->add_point(Vector2(-100, 0), 0, 0, Curve::TangentMode::TANGENT_LINEAR, Curve::TangentMode::TANGENT_LINEAR);
	curve->add_point(Vector2(-50, 1), 0, 0, Curve::TangentMode::TANGENT_LINEAR, Curve::TangentMode::TANGENT_LINEAR);
	curve->add_point(Vector2(0, 0), 0, 0, Curve::TangentMode::TANGENT_LINEAR, Curve::TangentMode::TANGENT_LINEAR);
	curve->add_point(Vector2(50, 1), 0, 0, Curve::TangentMode::TANGENT_LINEAR, Curve::TangentMode::TANGENT_LINEAR);

	CHECK_MESSAGE(
			curve->get_point_left_tangent(3) == doctest::Approx(1.f / 50),
			"get_point_left_tangent() should return the expected value for point index 3.");
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->get_point_right_tangent(3)),
			"get_point_right_tangent() should return the expected value for point index 3.");
	CHECK_MESSAGE(
			curve->get_point_left_mode(3) == Curve::TangentMode::TANGENT_LINEAR,
			"get_point_left_mode() should return the expected value for point index 3.");
	CHECK_MESSAGE(
			curve->get_point_right_mode(3) == Curve::TangentMode::TANGENT_LINEAR,
			"get_point_right_mode() should return the expected value for point index 3.");

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			Math::is_zero_approx(curve->get_point_right_tangent(300)),
			"get_point_right_tangent() should return the expected value for invalid point index 300.");
	CHECK_MESSAGE(
			curve->get_point_left_mode(-12345) == Curve::TangentMode::TANGENT_FREE,
			"get_point_left_mode() should return the expected value for invalid point index -12345.");
	ERR_PRINT_ON;

	CHECK_MESSAGE(
			curve->get_point_count() == 4,
			"Custom linear unit curve should contain the expected number of points.");

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample(-0.1 * curve->get_domain_range() + curve->get_min_domain())),
			"Custom linear curve should return the expected value at offset equivalent to a unit curve's -0.1.");
	CHECK_MESSAGE(
			curve->sample(0.1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.4),
			"Custom linear curve should return the expected value at offset equivalent to a unit curve's 0.1.");
	CHECK_MESSAGE(
			curve->sample(0.4 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.4),
			"Custom linear curve should return the expected value at offset equivalent to a unit curve's 0.4.");
	CHECK_MESSAGE(
			curve->sample(0.7 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.8),
			"Custom linear curve should return the expected value at offset equivalent to a unit curve's 0.7.");
	CHECK_MESSAGE(
			curve->sample(1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(1),
			"Custom linear curve should return the expected value at offset equivalent to a unit curve's 1.0.");
	CHECK_MESSAGE(
			curve->sample(2 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(1),
			"Custom linear curve should return the expected value at offset equivalent to a unit curve's 2.0.");

	CHECK_MESSAGE(
			Math::is_zero_approx(curve->sample_baked(-0.1 * curve->get_domain_range() + curve->get_min_domain())),
			"Custom linear curve should return the expected baked value at offset equivalent to a unit curve's -0.1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.4),
			"Custom linear curve should return the expected baked value at offset equivalent to a unit curve's 0.1.");
	CHECK_MESSAGE(
			curve->sample_baked(0.4 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.4),
			"Custom linear curve should return the expected baked value at offset equivalent to a unit curve's 0.4.");
	CHECK_MESSAGE(
			curve->sample_baked(0.7 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.8),
			"Custom linear curve should return the expected baked value at offset equivalent to a unit curve's 0.7.");
	CHECK_MESSAGE(
			curve->sample_baked(1 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(1),
			"Custom linear curve should return the expected baked value at offset equivalent to a unit curve's 1.0.");
	CHECK_MESSAGE(
			curve->sample_baked(2 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx(1),
			"Custom linear curve should return the expected baked value at offset equivalent to a unit curve's 2.0.");

	ERR_PRINT_OFF;
	curve->remove_point(10);
	ERR_PRINT_ON;
	CHECK_MESSAGE(
			curve->sample(0.7 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.8),
			"Custom free curve should return the expected value at offset equivalent to a unit curve's 0.7 after removing point at invalid index 10.");
	CHECK_MESSAGE(
			curve->sample_baked(0.7 * curve->get_domain_range() + curve->get_min_domain()) == doctest::Approx((real_t)0.8),
			"Custom free curve should return the expected baked value at offset equivalent to a unit curve's 0.7 after removing point at invalid index 10.");
}

TEST_CASE("[Curve] Straight line offset test") {
	Ref<Curve> curve = memnew(Curve);
	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(1, 1));

	CHECK_MESSAGE(
			curve->sample_baked(1.0 - (0.5 / curve->get_bake_resolution())) != curve->sample_baked(1),
			"Straight line curve should return different baked values at offset 1 vs offset (1 - 0.5 / bake resolution) .");
}

TEST_CASE("[Curve2D] Linear sampling should return exact value") {
	Ref<Curve2D> curve = memnew(Curve2D);
	real_t len = 2048.0;

	curve->add_point(Vector2(0, 0));
	curve->add_point(Vector2(len, 0));

	real_t baked_length = curve->get_baked_length();
	CHECK(len == baked_length);

	for (int i = 0; i < len; i++) {
		Vector2 pos = curve->sample_baked(i);
		CHECK_MESSAGE(Math::is_equal_approx(pos.x, i), "sample_baked should return exact value");
	}
}

TEST_CASE("[Curve3D] Linear sampling should return exact value") {
	Ref<Curve3D> curve = memnew(Curve3D);
	real_t len = 2048.0;

	curve->add_point(Vector3(0, 0, 0));
	curve->add_point(Vector3(len, 0, 0));
	ERR_PRINT_OFF
	real_t baked_length = curve->get_baked_length();
	ERR_PRINT_ON
	CHECK(len == baked_length);

	for (int i = 0; i < len; i++) {
		Vector3 pos = curve->sample_baked(i);
		CHECK_MESSAGE(Math::is_equal_approx(pos.x, i), "sample_baked should return exact value");
	}
}

} // namespace TestCurve

#endif // TEST_CURVE_H
