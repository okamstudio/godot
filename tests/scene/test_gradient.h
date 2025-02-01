/**************************************************************************/
/*  test_gradient.h                                                       */
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

#include "scene/resources/gradient.h"

#include "thirdparty/doctest/doctest.h"

namespace TestGradient {

TEST_CASE("[Gradient] Default gradient") {
	// Black-white gradient.
	Ref<Gradient> gradient = memnew(Gradient);

	CHECK_MESSAGE(
			gradient->get_point_count() == 2,
			"Default gradient should contain the expected number of points.");

	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.0).is_equal_approx(Color(0, 0, 0)),
			"Default gradient should return the expected interpolated value at offset 0.0.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.4).is_equal_approx(Color(0.4, 0.4, 0.4)),
			"Default gradient should return the expected interpolated value at offset 0.4.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.8).is_equal_approx(Color(0.8, 0.8, 0.8)),
			"Default gradient should return the expected interpolated value at offset 0.8.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(1.0).is_equal_approx(Color(1, 1, 1)),
			"Default gradient should return the expected interpolated value at offset 1.0.");

	// Out of bounds checks.
	CHECK_MESSAGE(
			gradient->get_color_at_offset(-1.0).is_equal_approx(Color(0, 0, 0)),
			"Default gradient should return the expected interpolated value at offset -1.0.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(1234.0).is_equal_approx(Color(1, 1, 1)),
			"Default gradient should return the expected interpolated value at offset 1234.0.");
}

TEST_CASE("[Gradient] Custom gradient (points specified in order)") {
	// Red-yellow-green gradient (with overbright green).
	Ref<Gradient> gradient = memnew(Gradient);
	Vector<float> offsets = { 0.0, 0.5, 1.0 };
	Vector<Color> colors = { Color(1, 0, 0), Color(1, 1, 0), Color(0, 2, 0) };

	gradient->set_offsets(offsets);
	gradient->set_colors(colors);

	CHECK_MESSAGE(
			gradient->get_point_count() == 3,
			"Custom gradient should contain the expected number of points.");

	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.0).is_equal_approx(Color(1, 0, 0)),
			"Custom gradient should return the expected interpolated value at offset 0.0.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.25).is_equal_approx(Color(1, 0.5, 0)),
			"Custom gradient should return the expected interpolated value at offset 0.25.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.5).is_equal_approx(Color(1, 1, 0)),
			"Custom gradient should return the expected interpolated value at offset 0.5.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.75).is_equal_approx(Color(0.5, 1.5, 0)),
			"Custom gradient should return the expected interpolated value at offset 0.75.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(1.0).is_equal_approx(Color(0, 2, 0)),
			"Custom gradient should return the expected interpolated value at offset 1.0.");

	gradient->remove_point(1);
	CHECK_MESSAGE(
			gradient->get_point_count() == 2,
			"Custom gradient should contain the expected number of points after removing one point.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.5).is_equal_approx(Color(0.5, 1, 0)),
			"Custom gradient should return the expected interpolated value at offset 0.5 after removing point at index 1.");
}

TEST_CASE("[Gradient] Custom gradient (points specified out-of-order)") {
	// HSL rainbow with points specified out of order.
	// These should be sorted automatically when adding points.
	Ref<Gradient> gradient = memnew(Gradient);
	LocalVector<Gradient::Point> points;
	Vector<float> offsets = { 0.2, 0.0, 0.8, 0.4, 1.0, 0.6 };
	Vector<Color> colors = { Color(1, 0, 0), Color(1, 1, 0), Color(0, 1, 0), Color(0, 1, 1), Color(0, 0, 1), Color(1, 0, 1) };

	gradient->set_offsets(offsets);
	gradient->set_colors(colors);

	CHECK_MESSAGE(
			gradient->get_point_count() == 6,
			"Custom out-of-order gradient should contain the expected number of points.");

	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.0).is_equal_approx(Color(1, 1, 0)),
			"Custom out-of-order gradient should return the expected interpolated value at offset 0.0.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.3).is_equal_approx(Color(0.5, 0.5, 0.5)),
			"Custom out-of-order gradient should return the expected interpolated value at offset 0.3.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.6).is_equal_approx(Color(1, 0, 1)),
			"Custom out-of-order gradient should return the expected interpolated value at offset 0.6.");
	CHECK_MESSAGE(
			gradient->get_color_at_offset(1.0).is_equal_approx(Color(0, 0, 1)),
			"Custom out-of-order gradient should return the expected interpolated value at offset 1.0.");

	gradient->remove_point(0);
	CHECK_MESSAGE(
			gradient->get_point_count() == 5,
			"Custom out-of-order gradient should contain the expected number of points after removing one point.");
	// The color will be clamped to the nearest point (which is at offset 0.2).
	CHECK_MESSAGE(
			gradient->get_color_at_offset(0.1).is_equal_approx(Color(1, 0, 0)),
			"Custom out-of-order gradient should return the expected interpolated value at offset 0.1 after removing point at index 0.");
}
} // namespace TestGradient
