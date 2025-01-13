/**************************************************************************/
/*  test_plane.h                                                          */
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

#ifndef TEST_PLANE_H
#define TEST_PLANE_H

#include "core/math/plane.h"

#include "thirdparty/doctest/doctest.h"

namespace TestPlane {

// Plane

TEST_CASE("[Plane] Constructor methods") {
	const Plane plane = Plane(32, 22, 16, 3);
	const Plane plane_vector = Plane(Vector3(32, 22, 16), 3);
	const Plane plane_copy_plane = Plane(plane);

	CHECK_MESSAGE(
			plane == plane_vector,
			"Planes created with same values but different methods should be equal.");

	CHECK_MESSAGE(
			plane == plane_copy_plane,
			"Planes created with same values but different methods should be equal.");
}

TEST_CASE("[Plane] Basic getters") {
	const Plane plane = Plane(32, 22, 16, 3);
	const Plane plane_normalized = Plane(32.0 / 42, 22.0 / 42, 16.0 / 42, 3.0 / 42);

	CHECK_MESSAGE(
			plane.get_normal().is_equal_approx(Vector3(32, 22, 16)),
			"get_normal() should return the expected value.");

	CHECK_MESSAGE(
			plane.normalized().is_equal_approx(plane_normalized),
			"normalized() should return a copy of the normalized value.");
}

TEST_CASE("[Plane] Basic setters") {
	Plane plane = Plane(32, 22, 16, 3);
	plane.set_normal(Vector3(4, 2, 3));

	CHECK_MESSAGE(
			plane.is_equal_approx(Plane(4, 2, 3, 3)),
			"set_normal() should result in the expected plane.");

	plane = Plane(32, 22, 16, 3);
	plane.normalize();

	CHECK_MESSAGE(
			plane.is_equal_approx(Plane(32.0 / 42, 22.0 / 42, 16.0 / 42, 3.0 / 42)),
			"normalize() should result in the expected plane.");
}

TEST_CASE("[Plane] Plane-point operations") {
	const Plane plane = Plane(32, 22, 16, 3);
	const Plane y_facing_plane = Plane(0, 1, 0, 4);

	CHECK_MESSAGE(
			plane.get_center().is_equal_approx(Vector3(32 * 3, 22 * 3, 16 * 3)),
			"get_center() should return a vector pointing to the center of the plane.");

	CHECK_MESSAGE(
			y_facing_plane.is_point_over(Vector3(0, 5, 0)),
			"is_point_over() should return the expected result.");

	CHECK_MESSAGE(
			y_facing_plane.get_any_perpendicular_normal().is_equal_approx(Vector3(1, 0, 0)),
			"get_any_perpendicular_normal() should return the expected result.");

	// TODO distance_to()
}

TEST_CASE("[Plane] Has point") {
	const Plane x_facing_plane = Plane(1, 0, 0, 0);
	const Plane y_facing_plane = Plane(0, 1, 0, 0);
	const Plane z_facing_plane = Plane(0, 0, 1, 0);

	const Vector3 x_axis_point = Vector3(10, 0, 0);
	const Vector3 y_axis_point = Vector3(0, 10, 0);
	const Vector3 z_axis_point = Vector3(0, 0, 10);

	const Plane x_facing_plane_with_d_offset = Plane(1, 0, 0, 1);
	const Vector3 y_axis_point_with_d_offset = Vector3(1, 10, 0);

	CHECK_MESSAGE(
			x_facing_plane.has_point(y_axis_point),
			"has_point() with contained Vector3 should return the expected result.");
	CHECK_MESSAGE(
			x_facing_plane.has_point(z_axis_point),
			"has_point() with contained Vector3 should return the expected result.");

	CHECK_MESSAGE(
			y_facing_plane.has_point(x_axis_point),
			"has_point() with contained Vector3 should return the expected result.");
	CHECK_MESSAGE(
			y_facing_plane.has_point(z_axis_point),
			"has_point() with contained Vector3 should return the expected result.");

	CHECK_MESSAGE(
			z_facing_plane.has_point(y_axis_point),
			"has_point() with contained Vector3 should return the expected result.");
	CHECK_MESSAGE(
			z_facing_plane.has_point(x_axis_point),
			"has_point() with contained Vector3 should return the expected result.");

	CHECK_MESSAGE(
			x_facing_plane_with_d_offset.has_point(y_axis_point_with_d_offset),
			"has_point() with passed Vector3 should return the expected result.");
}

TEST_CASE("[Plane] Intersection") {
	const Plane x_facing_plane = Plane(1, 0, 0, 1);
	const Plane y_facing_plane = Plane(0, 1, 0, 2);
	const Plane z_facing_plane = Plane(0, 0, 1, 3);

	Vector3 vec_out;

	CHECK_MESSAGE(
			x_facing_plane.intersect_3(y_facing_plane, z_facing_plane, &vec_out),
			"intersect_3() should return the expected result.");
	CHECK_MESSAGE(
			vec_out.is_equal_approx(Vector3(1, 2, 3)),
			"intersect_3() should modify vec_out to the expected result.");

	CHECK_MESSAGE(
			x_facing_plane.intersects_ray(Vector3(0, 1, 1), Vector3(2, 0, 0), &vec_out),
			"intersects_ray() should return the expected result.");
	CHECK_MESSAGE(
			vec_out.is_equal_approx(Vector3(1, 1, 1)),
			"intersects_ray() should modify vec_out to the expected result.");

	CHECK_MESSAGE(
			x_facing_plane.intersects_segment(Vector3(0, 1, 1), Vector3(2, 1, 1), &vec_out),
			"intersects_segment() should return the expected result.");
	CHECK_MESSAGE(
			vec_out.is_equal_approx(Vector3(1, 1, 1)),
			"intersects_segment() should modify vec_out to the expected result.");
}

TEST_CASE("[Plane] Finite number checks") {
	const Vector3 x(0, 1, 2);
	const Vector3 infinite_vec(NAN, NAN, NAN);
	const real_t y = 0;
	const real_t infinite_y = NAN;

	CHECK_MESSAGE(
			Plane(x, y).is_finite(),
			"Plane with all components finite should be finite");

	CHECK_FALSE_MESSAGE(
			Plane(x, infinite_y).is_finite(),
			"Plane with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Plane(infinite_vec, y).is_finite(),
			"Plane with one component infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Plane(infinite_vec, infinite_y).is_finite(),
			"Plane with two components infinite should not be finite.");
}

} // namespace TestPlane

#endif // TEST_PLANE_H
