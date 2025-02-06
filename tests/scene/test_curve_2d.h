/**************************************************************************/
/*  test_curve_2d.h                                                       */
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

#include "core/math/math_funcs.h"
#include "scene/resources/curve.h"

#include "tests/test_macros.h"

namespace TestCurve2D {

void add_sample_curve_points(Ref<Curve2D> &curve) {
	Vector2 p0 = Vector2(0, 0);
	Vector2 p1 = Vector2(50, 0);
	Vector2 p2 = Vector2(50, 50);
	Vector2 p3 = Vector2(0, 50);

	Vector2 control0 = p1 - p0;
	Vector2 control1 = p3 - p2;

	curve->add_point(p0, Vector2(), control0);
	curve->add_point(p3, control1, Vector2());
}

TEST_CASE("[Curve2D] Default curve is empty") {
	const Ref<Curve2D> curve = memnew(Curve2D);
	CHECK(curve->get_point_count() == 0);
}

TEST_CASE("[Curve2D] Point management") {
	Ref<Curve2D> curve = memnew(Curve2D);

	SUBCASE("Functions for adding/removing points should behave as expected") {
		curve->set_point_count(2);
		CHECK(curve->get_point_count() == 2);

		curve->remove_point(0);
		CHECK(curve->get_point_count() == 1);

		curve->add_point(Vector2());
		CHECK(curve->get_point_count() == 2);

		curve->clear_points();
		CHECK(curve->get_point_count() == 0);
	}

	SUBCASE("Functions for changing single point properties should behave as expected") {
		Vector2 new_in = Vector2(1, 1);
		Vector2 new_out = Vector2(1, 1);
		Vector2 new_pos = Vector2(1, 1);

		curve->add_point(Vector2());

		CHECK(curve->get_point_in(0) != new_in);
		curve->set_point_in(0, new_in);
		CHECK(curve->get_point_in(0) == new_in);

		CHECK(curve->get_point_out(0) != new_out);
		curve->set_point_out(0, new_out);
		CHECK(curve->get_point_out(0) == new_out);

		CHECK(curve->get_point_position(0) != new_pos);
		curve->set_point_position(0, new_pos);
		CHECK(curve->get_point_position(0) == new_pos);
	}
}

TEST_CASE("[Curve2D] Baked") {
	Ref<Curve2D> curve = memnew(Curve2D);

	SUBCASE("Single Point") {
		curve->add_point(Vector2());

		CHECK(curve->get_baked_length() == 0);
		CHECK(curve->get_baked_points().size() == 1);
	}

	SUBCASE("Straight line") {
		curve->add_point(Vector2());
		curve->add_point(Vector2(0, 50));

		CHECK(Math::is_equal_approx(curve->get_baked_length(), 50));
		CHECK(curve->get_baked_points().size() == 15);
	}

	SUBCASE("Beziér Curve") {
		add_sample_curve_points(curve);

		real_t len = curve->get_baked_length();
		real_t n_points = curve->get_baked_points().size();
		// Curve length should be bigger than a straight between points
		CHECK(len > 50);

		SUBCASE("Increase bake interval") {
			curve->set_bake_interval(10.0);
			// Lower resolution should imply less points and smaller length
			CHECK(curve->get_baked_length() < len);
			CHECK(curve->get_baked_points().size() < n_points);
		}
	}
}

TEST_CASE("[Curve2D] Sampling") {
	// Sampling over a simple straight line to make assertions simpler
	Ref<Curve2D> curve = memnew(Curve2D);
	curve->add_point(Vector2());
	curve->add_point(Vector2(0, 50));

	SUBCASE("sample") {
		CHECK(curve->sample(0, 0) == Vector2(0, 0));
		CHECK(curve->sample(0, 0.5) == Vector2(0, 25));
		CHECK(curve->sample(0, 1) == Vector2(0, 50));
	}

	SUBCASE("samplef") {
		CHECK(curve->samplef(0) == Vector2(0, 0));
		CHECK(curve->samplef(0.5) == Vector2(0, 25));
		CHECK(curve->samplef(1) == Vector2(0, 50));
	}

	SUBCASE("sample_baked, cubic = false") {
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector2(0, 0))) == Vector2(0, 0));
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector2(0, 25))) == Vector2(0, 25));
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector2(0, 50))) == Vector2(0, 50));
	}

	SUBCASE("sample_baked, cubic = true") {
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector2(0, 0)), true) == Vector2(0, 0));
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector2(0, 25)), true) == Vector2(0, 25));
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector2(0, 50)), true) == Vector2(0, 50));
	}

	SUBCASE("sample_baked_with_rotation, cubic = false") {
		const real_t pi = 3.14159;
		const real_t half_pi = pi * 0.5;
		Ref<Curve2D> rot_curve = memnew(Curve2D);
		Transform2D t;

		rot_curve->clear_points();
		rot_curve->add_point(Vector2());
		rot_curve->add_point(Vector2(50, 0));
		t = rot_curve->sample_baked_with_rotation(25);
		CHECK(t.get_origin() == Vector2(25, 0));
		CHECK(Math::is_equal_approx(t.get_rotation(), 0));

		rot_curve->clear_points();
		rot_curve->add_point(Vector2());
		rot_curve->add_point(Vector2(0, 50));
		t = rot_curve->sample_baked_with_rotation(25);
		CHECK(t.get_origin() == Vector2(0, 25));
		CHECK(Math::is_equal_approx(t.get_rotation(), half_pi));

		rot_curve->clear_points();
		rot_curve->add_point(Vector2());
		rot_curve->add_point(Vector2(-50, 0));
		t = rot_curve->sample_baked_with_rotation(25);
		CHECK(t.get_origin() == Vector2(-25, 0));
		CHECK(Math::is_equal_approx(t.get_rotation(), pi));

		rot_curve->clear_points();
		rot_curve->add_point(Vector2());
		rot_curve->add_point(Vector2(0, -50));
		t = rot_curve->sample_baked_with_rotation(25);
		CHECK(t.get_origin() == Vector2(0, -25));
		CHECK(Math::is_equal_approx(t.get_rotation(), -half_pi));
	}

	SUBCASE("sample_baked_with_rotation, cubic = true") {
		const real_t pi = 3.14159;
		const real_t half_pi = pi * 0.5;
		Ref<Curve2D> rot_curve = memnew(Curve2D);
		Transform2D t;

		rot_curve->clear_points();
		rot_curve->add_point(Vector2());
		rot_curve->add_point(Vector2(50, 0));
		t = rot_curve->sample_baked_with_rotation(25, true);
		CHECK(t.get_origin() == Vector2(25, 0));
		CHECK(Math::is_equal_approx(t.get_rotation(), 0));

		rot_curve->clear_points();
		rot_curve->add_point(Vector2());
		rot_curve->add_point(Vector2(0, 50));
		t = rot_curve->sample_baked_with_rotation(25, true);
		CHECK(t.get_origin() == Vector2(0, 25));
		CHECK(Math::is_equal_approx(t.get_rotation(), half_pi));

		rot_curve->clear_points();
		rot_curve->add_point(Vector2());
		rot_curve->add_point(Vector2(-50, 0));
		t = rot_curve->sample_baked_with_rotation(25, true);
		CHECK(t.get_origin() == Vector2(-25, 0));
		CHECK(Math::is_equal_approx(t.get_rotation(), pi));

		rot_curve->clear_points();
		rot_curve->add_point(Vector2());
		rot_curve->add_point(Vector2(0, -50));
		t = rot_curve->sample_baked_with_rotation(25, true);
		CHECK(t.get_origin() == Vector2(0, -25));
		CHECK(Math::is_equal_approx(t.get_rotation(), -half_pi));
	}

	SUBCASE("get_closest_point") {
		CHECK(curve->get_closest_point(Vector2(0, 0)) == Vector2(0, 0));
		CHECK(curve->get_closest_point(Vector2(0, 25)) == Vector2(0, 25));
		CHECK(curve->get_closest_point(Vector2(50, 25)) == Vector2(0, 25));
		CHECK(curve->get_closest_point(Vector2(0, 50)) == Vector2(0, 50));
		CHECK(curve->get_closest_point(Vector2(50, 50)) == Vector2(0, 50));
		CHECK(curve->get_closest_point(Vector2(0, 100)) == Vector2(0, 50));
	}
}

TEST_CASE("[Curve2D] Tessellation") {
	Ref<Curve2D> curve = memnew(Curve2D);
	add_sample_curve_points(curve);

	const int default_size = curve->tessellate().size();

	SUBCASE("Increase to max stages should increase num of points") {
		CHECK(curve->tessellate(6).size() > default_size);
	}

	SUBCASE("Decrease to max stages should decrease num of points") {
		CHECK(curve->tessellate(4).size() < default_size);
	}

	SUBCASE("Increase to tolerance should decrease num of points") {
		CHECK(curve->tessellate(5, 5).size() < default_size);
	}

	SUBCASE("Decrease to tolerance should increase num of points") {
		CHECK(curve->tessellate(5, 3).size() > default_size);
	}

	SUBCASE("Adding a straight segment should only add the last point to tessellate return array") {
		curve->add_point(Vector2(0, 100));
		PackedVector2Array tes = curve->tessellate();
		CHECK(tes.size() == default_size + 1);
		CHECK(tes[tes.size() - 1] == Vector2(0, 100));
		CHECK(tes[tes.size() - 2] == Vector2(0, 50));
	}
}

TEST_CASE("[Curve2D] Even length tessellation") {
	Ref<Curve2D> curve = memnew(Curve2D);
	add_sample_curve_points(curve);

	const int default_size = curve->tessellate_even_length().size();

	// Default tessellate_even_length tolerance_length is 20.0, by adding a 100 units
	// straight, we expect the total size to be increased by more than 5,
	// that is, the algo will pick a length < 20.0 and will divide the straight as
	// well as the curve as opposed to tessellate() which only adds the final point
	curve->add_point(Vector2(0, 150));
	CHECK(curve->tessellate_even_length().size() > default_size + 5);
}

} // namespace TestCurve2D
