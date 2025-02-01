/**************************************************************************/
/*  test_curve_3d.h                                                       */
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

namespace TestCurve3D {

void add_sample_curve_points(Ref<Curve3D> &curve) {
	Vector3 p0 = Vector3(0, 0, 0);
	Vector3 p1 = Vector3(50, 0, 0);
	Vector3 p2 = Vector3(50, 50, 50);
	Vector3 p3 = Vector3(0, 50, 0);

	Vector3 control0 = p1 - p0;
	Vector3 control1 = p3 - p2;

	curve->add_point(p0, Vector3(), control0);
	curve->add_point(p3, control1, Vector3());
}

TEST_CASE("[Curve3D] Default curve is empty") {
	const Ref<Curve3D> curve = memnew(Curve3D);
	CHECK(curve->get_point_count() == 0);
}

TEST_CASE("[Curve3D] Point management") {
	Ref<Curve3D> curve = memnew(Curve3D);

	SUBCASE("Functions for adding/removing points should behave as expected") {
		curve->set_point_count(2);
		CHECK(curve->get_point_count() == 2);

		curve->remove_point(0);
		CHECK(curve->get_point_count() == 1);

		curve->add_point(Vector3());
		CHECK(curve->get_point_count() == 2);

		curve->clear_points();
		CHECK(curve->get_point_count() == 0);
	}

	SUBCASE("Functions for changing single point properties should behave as expected") {
		Vector3 new_in = Vector3(1, 1, 1);
		Vector3 new_out = Vector3(1, 1, 1);
		Vector3 new_pos = Vector3(1, 1, 1);
		real_t new_tilt = 1;

		curve->add_point(Vector3());

		CHECK(curve->get_point_in(0) != new_in);
		curve->set_point_in(0, new_in);
		CHECK(curve->get_point_in(0) == new_in);

		CHECK(curve->get_point_out(0) != new_out);
		curve->set_point_out(0, new_out);
		CHECK(curve->get_point_out(0) == new_out);

		CHECK(curve->get_point_position(0) != new_pos);
		curve->set_point_position(0, new_pos);
		CHECK(curve->get_point_position(0) == new_pos);

		CHECK(curve->get_point_tilt(0) != new_tilt);
		curve->set_point_tilt(0, new_tilt);
		CHECK(curve->get_point_tilt(0) == new_tilt);
	}
}

TEST_CASE("[Curve3D] Baked") {
	Ref<Curve3D> curve = memnew(Curve3D);

	SUBCASE("Single Point") {
		curve->add_point(Vector3());

		CHECK(curve->get_baked_length() == 0);
		CHECK(curve->get_baked_points().size() == 1);
		CHECK(curve->get_baked_tilts().size() == 1);
		CHECK(curve->get_baked_up_vectors().size() == 1);
	}

	SUBCASE("Straight line") {
		curve->add_point(Vector3());
		curve->add_point(Vector3(0, 50, 0));

		CHECK(Math::is_equal_approx(curve->get_baked_length(), 50));
		CHECK(curve->get_baked_points().size() == 369);
		CHECK(curve->get_baked_tilts().size() == 369);
		CHECK(curve->get_baked_up_vectors().size() == 369);
	}

	SUBCASE("Beziér Curve") {
		add_sample_curve_points(curve);

		real_t len = curve->get_baked_length();
		real_t n_points = curve->get_baked_points().size();
		// Curve length should be bigger than a straight line between points
		CHECK(len > 50);

		SUBCASE("Increase bake interval") {
			curve->set_bake_interval(10.0);
			CHECK(curve->get_bake_interval() == 10.0);
			// Lower resolution should imply less points and smaller length
			CHECK(curve->get_baked_length() < len);
			CHECK(curve->get_baked_points().size() < n_points);
			CHECK(curve->get_baked_tilts().size() < n_points);
			CHECK(curve->get_baked_up_vectors().size() < n_points);
		}

		SUBCASE("Disable up vectors") {
			curve->set_up_vector_enabled(false);
			CHECK(curve->is_up_vector_enabled() == false);
			CHECK(curve->get_baked_up_vectors().size() == 0);
		}
	}
}

TEST_CASE("[Curve3D] Sampling") {
	// Sampling over a simple straight line to make assertions simpler
	Ref<Curve3D> curve = memnew(Curve3D);
	curve->add_point(Vector3());
	curve->add_point(Vector3(0, 50, 0));

	SUBCASE("sample") {
		CHECK(curve->sample(0, 0) == Vector3(0, 0, 0));
		CHECK(curve->sample(0, 0.5) == Vector3(0, 25, 0));
		CHECK(curve->sample(0, 1) == Vector3(0, 50, 0));
	}

	SUBCASE("samplef") {
		CHECK(curve->samplef(0) == Vector3(0, 0, 0));
		CHECK(curve->samplef(0.5) == Vector3(0, 25, 0));
		CHECK(curve->samplef(1) == Vector3(0, 50, 0));
	}

	SUBCASE("sample_baked, cubic = false") {
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector3(0, 0, 0))) == Vector3(0, 0, 0));
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector3(0, 25, 0))) == Vector3(0, 25, 0));
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector3(0, 50, 0))) == Vector3(0, 50, 0));
	}

	SUBCASE("sample_baked, cubic = true") {
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector3(0, 0, 0)), true) == Vector3(0, 0, 0));
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector3(0, 25, 0)), true) == Vector3(0, 25, 0));
		CHECK(curve->sample_baked(curve->get_closest_offset(Vector3(0, 50, 0)), true) == Vector3(0, 50, 0));
	}

	SUBCASE("sample_baked_with_rotation, cubic = false, p_apply_tilt = false") {
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 0, 0))) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 0, 0)));
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 25, 0))) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 25, 0)));
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 50, 0))) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 50, 0)));
	}

	SUBCASE("sample_baked_with_rotation, cubic = false, p_apply_tilt = true") {
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 0, 0)), false, true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 0, 0)));
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 25, 0)), false, true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 25, 0)));
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 50, 0)), false, true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 50, 0)));
	}

	SUBCASE("sample_baked_with_rotation, cubic = true, p_apply_tilt = false") {
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 0, 0)), true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 0, 0)));
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 25, 0)), true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 25, 0)));
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 50, 0)), true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 50, 0)));
	}

	SUBCASE("sample_baked_with_rotation, cubic = true, p_apply_tilt = true") {
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 0, 0)), true, true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 0, 0)));
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 25, 0)), true, true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 25, 0)));
		CHECK(curve->sample_baked_with_rotation(curve->get_closest_offset(Vector3(0, 50, 0)), true, true) == Transform3D(Basis(Vector3(0, 0, -1), Vector3(1, 0, 0), Vector3(0, -1, 0)), Vector3(0, 50, 0)));
	}

	SUBCASE("sample_baked_tilt") {
		CHECK(curve->sample_baked_tilt(curve->get_closest_offset(Vector3(0, 0, 0))) == 0);
		CHECK(curve->sample_baked_tilt(curve->get_closest_offset(Vector3(0, 25, 0))) == 0);
		CHECK(curve->sample_baked_tilt(curve->get_closest_offset(Vector3(0, 50, 0))) == 0);
	}

	SUBCASE("sample_baked_up_vector, p_apply_tilt = false") {
		CHECK(curve->sample_baked_up_vector(curve->get_closest_offset(Vector3(0, 0, 0))) == Vector3(1, 0, 0));
		CHECK(curve->sample_baked_up_vector(curve->get_closest_offset(Vector3(0, 25, 0))) == Vector3(1, 0, 0));
		CHECK(curve->sample_baked_up_vector(curve->get_closest_offset(Vector3(0, 50, 0))) == Vector3(1, 0, 0));
	}

	SUBCASE("sample_baked_up_vector, p_apply_tilt = true") {
		CHECK(curve->sample_baked_up_vector(curve->get_closest_offset(Vector3(0, 0, 0)), true) == Vector3(1, 0, 0));
		CHECK(curve->sample_baked_up_vector(curve->get_closest_offset(Vector3(0, 25, 0)), true) == Vector3(1, 0, 0));
		CHECK(curve->sample_baked_up_vector(curve->get_closest_offset(Vector3(0, 50, 0)), true) == Vector3(1, 0, 0));
	}

	SUBCASE("get_closest_point") {
		CHECK(curve->get_closest_point(Vector3(0, 0, 0)) == Vector3(0, 0, 0));
		CHECK(curve->get_closest_point(Vector3(0, 25, 0)) == Vector3(0, 25, 0));
		CHECK(curve->get_closest_point(Vector3(50, 25, 0)) == Vector3(0, 25, 0));
		CHECK(curve->get_closest_point(Vector3(0, 50, 0)) == Vector3(0, 50, 0));
		CHECK(curve->get_closest_point(Vector3(50, 50, 0)) == Vector3(0, 50, 0));
		CHECK(curve->get_closest_point(Vector3(0, 100, 0)) == Vector3(0, 50, 0));
	}

	SUBCASE("sample_baked_up_vector, off-axis") {
		// Regression test for issue #81879
		Ref<Curve3D> c = memnew(Curve3D);
		c->add_point(Vector3());
		c->add_point(Vector3(0, .1, 1));
		CHECK_LT((c->sample_baked_up_vector(c->get_closest_offset(Vector3(0, 0, .9))) - Vector3(0, 0.995037, -0.099504)).length(), 0.01);
	}
}

TEST_CASE("[Curve3D] Tessellation") {
	Ref<Curve3D> curve = memnew(Curve3D);
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
		curve->add_point(Vector3(0, 100, 0));
		PackedVector3Array tes = curve->tessellate();
		CHECK(tes.size() == default_size + 1);
		CHECK(tes[tes.size() - 1] == Vector3(0, 100, 0));
		CHECK(tes[tes.size() - 2] == Vector3(0, 50, 0));
	}
}

TEST_CASE("[Curve3D] Even length tessellation") {
	Ref<Curve3D> curve = memnew(Curve3D);
	add_sample_curve_points(curve);

	const int default_size = curve->tessellate_even_length().size();

	// Default tessellate_even_length tolerance_length is 20.0, by adding a 100 units
	// straight, we expect the total size to be increased by more than 5,
	// that is, the algo will pick a length < 20.0 and will divide the straight as
	// well as the curve as opposed to tessellate() which only adds the final point.
	curve->add_point(Vector3(0, 150, 0));
	CHECK(curve->tessellate_even_length().size() > default_size + 5);
}

} // namespace TestCurve3D
