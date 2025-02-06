/**************************************************************************/
/*  test_aabb.h                                                           */
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

#include "core/math/aabb.h"

#include "tests/test_macros.h"

namespace TestAABB {

TEST_CASE("[AABB] Constructor methods") {
	const AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	const AABB aabb_copy = AABB(aabb);

	CHECK_MESSAGE(
			aabb == aabb_copy,
			"AABBs created with the same dimensions but by different methods should be equal.");
}

TEST_CASE("[AABB] String conversion") {
	CHECK_MESSAGE(
			String(AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6))) == "[P: (-1.5, 2.0, -2.5), S: (4.0, 5.0, 6.0)]",
			"The string representation should match the expected value.");
}

TEST_CASE("[AABB] Basic getters") {
	const AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.get_position().is_equal_approx(Vector3(-1.5, 2, -2.5)),
			"get_position() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_size().is_equal_approx(Vector3(4, 5, 6)),
			"get_size() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_end().is_equal_approx(Vector3(2.5, 7, 3.5)),
			"get_end() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_center().is_equal_approx(Vector3(0.5, 4.5, 0.5)),
			"get_center() should return the expected value.");
}

TEST_CASE("[AABB] Basic setters") {
	AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	aabb.set_end(Vector3(100, 0, 100));
	CHECK_MESSAGE(
			aabb.is_equal_approx(AABB(Vector3(-1.5, 2, -2.5), Vector3(101.5, -2, 102.5))),
			"set_end() should result in the expected AABB.");

	aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	aabb.set_position(Vector3(-1000, -2000, -3000));
	CHECK_MESSAGE(
			aabb.is_equal_approx(AABB(Vector3(-1000, -2000, -3000), Vector3(4, 5, 6))),
			"set_position() should result in the expected AABB.");

	aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	aabb.set_size(Vector3(0, 0, -50));
	CHECK_MESSAGE(
			aabb.is_equal_approx(AABB(Vector3(-1.5, 2, -2.5), Vector3(0, 0, -50))),
			"set_size() should result in the expected AABB.");
}

TEST_CASE("[AABB] Volume getters") {
	AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.get_volume() == doctest::Approx(120),
			"get_volume() should return the expected value with positive size.");
	CHECK_MESSAGE(
			aabb.has_volume(),
			"Non-empty volumetric AABB should have a volume.");

	aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(-4, 5, 6));
	CHECK_MESSAGE(
			aabb.get_volume() == doctest::Approx(-120),
			"get_volume() should return the expected value with negative size (1 component).");

	aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(-4, -5, 6));
	CHECK_MESSAGE(
			aabb.get_volume() == doctest::Approx(120),
			"get_volume() should return the expected value with negative size (2 components).");

	aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(-4, -5, -6));
	CHECK_MESSAGE(
			aabb.get_volume() == doctest::Approx(-120),
			"get_volume() should return the expected value with negative size (3 components).");

	aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 0, 6));
	CHECK_MESSAGE(
			!aabb.has_volume(),
			"Non-empty flat AABB should not have a volume.");

	CHECK_MESSAGE(
			!AABB().has_volume(),
			"Empty AABB should not have a volume.");
}

TEST_CASE("[AABB] Surface getters") {
	AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.has_surface(),
			"Non-empty volumetric AABB should have an surface.");

	aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 0, 6));
	CHECK_MESSAGE(
			aabb.has_surface(),
			"Non-empty flat AABB should have a surface.");

	aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 0, 0));
	CHECK_MESSAGE(
			aabb.has_surface(),
			"Non-empty linear AABB should have a surface.");

	CHECK_MESSAGE(
			!AABB().has_surface(),
			"Empty AABB should not have an surface.");
}

TEST_CASE("[AABB] Intersection") {
	const AABB aabb_big = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));

	AABB aabb_small = AABB(Vector3(-1.5, 2, -2.5), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.intersects(aabb_small),
			"intersects() with fully contained AABB (touching the edge) should return the expected result.");

	aabb_small = AABB(Vector3(0.5, 1.5, -2), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.intersects(aabb_small),
			"intersects() with partially contained AABB (overflowing on Y axis) should return the expected result.");

	aabb_small = AABB(Vector3(10, -10, -10), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			!aabb_big.intersects(aabb_small),
			"intersects() with non-contained AABB should return the expected result.");

	aabb_small = AABB(Vector3(-1.5, 2, -2.5), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.intersection(aabb_small).is_equal_approx(aabb_small),
			"intersection() with fully contained AABB (touching the edge) should return the expected result.");

	aabb_small = AABB(Vector3(0.5, 1.5, -2), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.intersection(aabb_small).is_equal_approx(AABB(Vector3(0.5, 2, -2), Vector3(1, 0.5, 1))),
			"intersection() with partially contained AABB (overflowing on Y axis) should return the expected result.");

	aabb_small = AABB(Vector3(10, -10, -10), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.intersection(aabb_small).is_equal_approx(AABB()),
			"intersection() with non-contained AABB should return the expected result.");

	CHECK_MESSAGE(
			aabb_big.intersects_plane(Plane(Vector3(0, 1, 0), 4)),
			"intersects_plane() should return the expected result.");
	CHECK_MESSAGE(
			aabb_big.intersects_plane(Plane(Vector3(0, -1, 0), -4)),
			"intersects_plane() should return the expected result.");
	CHECK_MESSAGE(
			!aabb_big.intersects_plane(Plane(Vector3(0, 1, 0), 200)),
			"intersects_plane() should return the expected result.");

	CHECK_MESSAGE(
			aabb_big.intersects_segment(Vector3(1, 3, 0), Vector3(0, 3, 0)),
			"intersects_segment() should return the expected result.");
	CHECK_MESSAGE(
			aabb_big.intersects_segment(Vector3(0, 3, 0), Vector3(0, -300, 0)),
			"intersects_segment() should return the expected result.");
	CHECK_MESSAGE(
			aabb_big.intersects_segment(Vector3(-50, 3, -50), Vector3(50, 3, 50)),
			"intersects_segment() should return the expected result.");
	CHECK_MESSAGE(
			!aabb_big.intersects_segment(Vector3(-50, 25, -50), Vector3(50, 25, 50)),
			"intersects_segment() should return the expected result.");
	CHECK_MESSAGE(
			aabb_big.intersects_segment(Vector3(0, 3, 0), Vector3(0, 3, 0)),
			"intersects_segment() should return the expected result with segment of length 0.");
	CHECK_MESSAGE(
			!aabb_big.intersects_segment(Vector3(0, 300, 0), Vector3(0, 300, 0)),
			"intersects_segment() should return the expected result with segment of length 0.");
	CHECK_MESSAGE( // Simple ray intersection test.
			aabb_big.intersects_ray(Vector3(-100, 3, 0), Vector3(1, 0, 0)),
			"intersects_ray() should return true when ray points directly to AABB from outside.");
	CHECK_MESSAGE( // Ray parallel to an edge.
			!aabb_big.intersects_ray(Vector3(10, 10, 0), Vector3(0, 1, 0)),
			"intersects_ray() should return false for ray parallel and outside of AABB.");
	CHECK_MESSAGE( // Ray origin inside aabb.
			aabb_big.intersects_ray(Vector3(1, 1, 1), Vector3(0, 1, 0)),
			"intersects_ray() should return true for rays originating inside the AABB.");
	CHECK_MESSAGE( // Ray pointing away from aabb.
			!aabb_big.intersects_ray(Vector3(-10, 0, 0), Vector3(-1, 0, 0)),
			"intersects_ray() should return false when ray points away from AABB.");
	CHECK_MESSAGE( // Ray along a diagonal of aabb.
			aabb_big.intersects_ray(Vector3(0, 0, 0), Vector3(1, 1, 1)),
			"intersects_ray() should return true for rays along the AABB diagonal.");
	CHECK_MESSAGE( // Ray originating at aabb edge.
			aabb_big.intersects_ray(aabb_big.position, Vector3(-1, 0, 0)),
			"intersects_ray() should return true for rays starting on AABB's edge.");
	CHECK_MESSAGE( // Ray with zero direction inside.
			aabb_big.intersects_ray(Vector3(-1, 3, -2), Vector3(0, 0, 0)),
			"intersects_ray() should return true because its inside.");
	CHECK_MESSAGE( // Ray with zero direction outside.
			!aabb_big.intersects_ray(Vector3(-1000, 3, -2), Vector3(0, 0, 0)),
			"intersects_ray() should return false for being outside.");

	// Finding ray intersections.
	const AABB aabb_simple = AABB(Vector3(), Vector3(1, 1, 1));
	bool inside = false;
	Vector3 intersection_point;
	Vector3 intersection_normal;

	// Borders.
	aabb_simple.find_intersects_ray(Vector3(0.5, 0, 0.5), Vector3(0, 1, 0), inside, &intersection_point, &intersection_normal);
	CHECK_MESSAGE(inside == false, "find_intersects_ray() should return outside on borders.");
	CHECK_MESSAGE(intersection_point.is_equal_approx(Vector3(0.5, 0, 0.5)), "find_intersects_ray() border intersection point incorrect.");
	CHECK_MESSAGE(intersection_normal.is_equal_approx(Vector3(0, -1, 0)), "find_intersects_ray() border intersection normal incorrect.");
	aabb_simple.find_intersects_ray(Vector3(0.5, 1, 0.5), Vector3(0, -1, 0), inside, &intersection_point, &intersection_normal);
	CHECK_MESSAGE(inside == false, "find_intersects_ray() should return outside on borders.");
	CHECK_MESSAGE(intersection_point.is_equal_approx(Vector3(0.5, 1, 0.5)), "find_intersects_ray() border intersection point incorrect.");
	CHECK_MESSAGE(intersection_normal.is_equal_approx(Vector3(0, 1, 0)), "find_intersects_ray() border intersection normal incorrect.");

	// Inside.
	aabb_simple.find_intersects_ray(Vector3(0.5, 0.1, 0.5), Vector3(0, 1, 0), inside, &intersection_point, &intersection_normal);
	CHECK_MESSAGE(inside == true, "find_intersects_ray() should return inside when inside.");
	CHECK_MESSAGE(intersection_point.is_equal_approx(Vector3(0.5, 0, 0.5)), "find_intersects_ray() inside backtracking intersection point incorrect.");
	CHECK_MESSAGE(intersection_normal.is_equal_approx(Vector3(0, -1, 0)), "find_intersects_ray() inside intersection normal incorrect.");

	// Zero sized AABB.
	const AABB aabb_zero = AABB(Vector3(), Vector3(1, 0, 1));
	aabb_zero.find_intersects_ray(Vector3(0.5, 0, 0.5), Vector3(0, 1, 0), inside, &intersection_point, &intersection_normal);
	CHECK_MESSAGE(inside == false, "find_intersects_ray() should return outside on borders of zero sized AABB.");
	CHECK_MESSAGE(intersection_point.is_equal_approx(Vector3(0.5, 0, 0.5)), "find_intersects_ray() border intersection point incorrect for zero sized AABB.");
	CHECK_MESSAGE(intersection_normal.is_equal_approx(Vector3(0, -1, 0)), "find_intersects_ray() border intersection normal incorrect for zero sized AABB.");
	aabb_zero.find_intersects_ray(Vector3(0.5, 0, 0.5), Vector3(0, -1, 0), inside, &intersection_point, &intersection_normal);
	CHECK_MESSAGE(inside == false, "find_intersects_ray() should return outside on borders of zero sized AABB.");
	CHECK_MESSAGE(intersection_point.is_equal_approx(Vector3(0.5, 0, 0.5)), "find_intersects_ray() border intersection point incorrect for zero sized AABB.");
	CHECK_MESSAGE(intersection_normal.is_equal_approx(Vector3(0, 1, 0)), "find_intersects_ray() border intersection normal incorrect for zero sized AABB.");
	aabb_zero.find_intersects_ray(Vector3(0.5, -1, 0.5), Vector3(0, 1, 0), inside, &intersection_point, &intersection_normal);
	CHECK_MESSAGE(inside == false, "find_intersects_ray() should return outside on borders of zero sized AABB.");
	CHECK_MESSAGE(intersection_point.is_equal_approx(Vector3(0.5, 0, 0.5)), "find_intersects_ray() border intersection point incorrect for zero sized AABB.");
	CHECK_MESSAGE(intersection_normal.is_equal_approx(Vector3(0, -1, 0)), "find_intersects_ray() border intersection normal incorrect for zero sized AABB.");
}

TEST_CASE("[AABB] Merging") {
	const AABB aabb_big = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));

	AABB aabb_small = AABB(Vector3(-1.5, 2, -2.5), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.merge(aabb_small).is_equal_approx(aabb_big),
			"merge() with fully contained AABB (touching the edge) should return the expected result.");

	aabb_small = AABB(Vector3(0.5, 1.5, -2), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.merge(aabb_small).is_equal_approx(AABB(Vector3(-1.5, 1.5, -2.5), Vector3(4, 5.5, 6))),
			"merge() with partially contained AABB (overflowing on Y axis) should return the expected result.");

	aabb_small = AABB(Vector3(10, -10, -10), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.merge(aabb_small).is_equal_approx(AABB(Vector3(-1.5, -10, -10), Vector3(12.5, 17, 13.5))),
			"merge() with non-contained AABB should return the expected result.");
}

TEST_CASE("[AABB] Encloses") {
	const AABB aabb_big = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));

	CHECK_MESSAGE(
			aabb_big.encloses(aabb_big),
			"encloses() with itself should return the expected result.");

	AABB aabb_small = AABB(Vector3(-1.5, 2, -2.5), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.encloses(aabb_small),
			"encloses() with fully contained AABB (touching the edge) should return the expected result.");

	aabb_small = AABB(Vector3(1.5, 6, 2.5), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			aabb_big.encloses(aabb_small),
			"encloses() with fully contained AABB (touching the edge) should return the expected result.");

	aabb_small = AABB(Vector3(0.5, 1.5, -2), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			!aabb_big.encloses(aabb_small),
			"encloses() with partially contained AABB (overflowing on Y axis) should return the expected result.");

	aabb_small = AABB(Vector3(10, -10, -10), Vector3(1, 1, 1));
	CHECK_MESSAGE(
			!aabb_big.encloses(aabb_small),
			"encloses() with non-contained AABB should return the expected result.");
}

TEST_CASE("[AABB] Get endpoints") {
	const AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.get_endpoint(0).is_equal_approx(Vector3(-1.5, 2, -2.5)),
			"The endpoint at index 0 should match the expected value.");
	CHECK_MESSAGE(
			aabb.get_endpoint(1).is_equal_approx(Vector3(-1.5, 2, 3.5)),
			"The endpoint at index 1 should match the expected value.");
	CHECK_MESSAGE(
			aabb.get_endpoint(2).is_equal_approx(Vector3(-1.5, 7, -2.5)),
			"The endpoint at index 2 should match the expected value.");
	CHECK_MESSAGE(
			aabb.get_endpoint(3).is_equal_approx(Vector3(-1.5, 7, 3.5)),
			"The endpoint at index 3 should match the expected value.");
	CHECK_MESSAGE(
			aabb.get_endpoint(4).is_equal_approx(Vector3(2.5, 2, -2.5)),
			"The endpoint at index 4 should match the expected value.");
	CHECK_MESSAGE(
			aabb.get_endpoint(5).is_equal_approx(Vector3(2.5, 2, 3.5)),
			"The endpoint at index 5 should match the expected value.");
	CHECK_MESSAGE(
			aabb.get_endpoint(6).is_equal_approx(Vector3(2.5, 7, -2.5)),
			"The endpoint at index 6 should match the expected value.");
	CHECK_MESSAGE(
			aabb.get_endpoint(7).is_equal_approx(Vector3(2.5, 7, 3.5)),
			"The endpoint at index 7 should match the expected value.");

	ERR_PRINT_OFF;
	CHECK_MESSAGE(
			aabb.get_endpoint(8).is_equal_approx(Vector3()),
			"The endpoint at invalid index 8 should match the expected value.");
	CHECK_MESSAGE(
			aabb.get_endpoint(-1).is_equal_approx(Vector3()),
			"The endpoint at invalid index -1 should match the expected value.");
	ERR_PRINT_ON;
}

TEST_CASE("[AABB] Get longest/shortest axis") {
	const AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.get_longest_axis() == Vector3(0, 0, 1),
			"get_longest_axis() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_longest_axis_index() == Vector3::AXIS_Z,
			"get_longest_axis_index() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_longest_axis_size() == 6,
			"get_longest_axis_size() should return the expected value.");

	CHECK_MESSAGE(
			aabb.get_shortest_axis() == Vector3(1, 0, 0),
			"get_shortest_axis() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_shortest_axis_index() == Vector3::AXIS_X,
			"get_shortest_axis_index() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_shortest_axis_size() == 4,
			"get_shortest_axis_size() should return the expected value.");
}

TEST_CASE("[AABB] Get support") {
	const AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.get_support(Vector3(1, 0, 0)) == Vector3(2.5, 2, -2.5),
			"get_support() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_support(Vector3(0.5, 1, 1)) == Vector3(2.5, 7, 3.5),
			"get_support() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_support(Vector3(0.5, 1, -400)) == Vector3(2.5, 7, -2.5),
			"get_support() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_support(Vector3(0, -1, 0)) == Vector3(-1.5, 2, -2.5),
			"get_support() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_support(Vector3(0, -0.1, 0)) == Vector3(-1.5, 2, -2.5),
			"get_support() should return the expected value.");
	CHECK_MESSAGE(
			aabb.get_support(Vector3()) == Vector3(-1.5, 2, -2.5),
			"get_support() should return the AABB position when given a zero vector.");
}

TEST_CASE("[AABB] Grow") {
	const AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.grow(0.25).is_equal_approx(AABB(Vector3(-1.75, 1.75, -2.75), Vector3(4.5, 5.5, 6.5))),
			"grow() with positive value should return the expected AABB.");
	CHECK_MESSAGE(
			aabb.grow(-0.25).is_equal_approx(AABB(Vector3(-1.25, 2.25, -2.25), Vector3(3.5, 4.5, 5.5))),
			"grow() with negative value should return the expected AABB.");
	CHECK_MESSAGE(
			aabb.grow(-10).is_equal_approx(AABB(Vector3(8.5, 12, 7.5), Vector3(-16, -15, -14))),
			"grow() with large negative value should return the expected AABB.");
}

TEST_CASE("[AABB] Has point") {
	const AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.has_point(Vector3(-1, 3, 0)),
			"has_point() with contained point should return the expected value.");
	CHECK_MESSAGE(
			aabb.has_point(Vector3(2, 3, 0)),
			"has_point() with contained point should return the expected value.");
	CHECK_MESSAGE(
			!aabb.has_point(Vector3(-20, 0, 0)),
			"has_point() with non-contained point should return the expected value.");

	CHECK_MESSAGE(
			aabb.has_point(Vector3(-1.5, 3, 0)),
			"has_point() with positive size should include point on near face (X axis).");
	CHECK_MESSAGE(
			aabb.has_point(Vector3(2.5, 3, 0)),
			"has_point() with positive size should include point on far face (X axis).");
	CHECK_MESSAGE(
			aabb.has_point(Vector3(0, 2, 0)),
			"has_point() with positive size should include point on near face (Y axis).");
	CHECK_MESSAGE(
			aabb.has_point(Vector3(0, 7, 0)),
			"has_point() with positive size should include point on far face (Y axis).");
	CHECK_MESSAGE(
			aabb.has_point(Vector3(0, 3, -2.5)),
			"has_point() with positive size should include point on near face (Z axis).");
	CHECK_MESSAGE(
			aabb.has_point(Vector3(0, 3, 3.5)),
			"has_point() with positive size should include point on far face (Z axis).");
}

TEST_CASE("[AABB] Expanding") {
	const AABB aabb = AABB(Vector3(-1.5, 2, -2.5), Vector3(4, 5, 6));
	CHECK_MESSAGE(
			aabb.expand(Vector3(-1, 3, 0)).is_equal_approx(aabb),
			"expand() with contained point should return the expected AABB.");
	CHECK_MESSAGE(
			aabb.expand(Vector3(2, 3, 0)).is_equal_approx(aabb),
			"expand() with contained point should return the expected AABB.");
	CHECK_MESSAGE(
			aabb.expand(Vector3(-1.5, 3, 0)).is_equal_approx(aabb),
			"expand() with contained point on negative edge should return the expected AABB.");
	CHECK_MESSAGE(
			aabb.expand(Vector3(2.5, 3, 0)).is_equal_approx(aabb),
			"expand() with contained point on positive edge should return the expected AABB.");
	CHECK_MESSAGE(
			aabb.expand(Vector3(-20, 0, 0)).is_equal_approx(AABB(Vector3(-20, 0, -2.5), Vector3(22.5, 7, 6))),
			"expand() with non-contained point should return the expected AABB.");
}

TEST_CASE("[AABB] Finite number checks") {
	const Vector3 x(0, 1, 2);
	const Vector3 infinite(NAN, NAN, NAN);

	CHECK_MESSAGE(
			AABB(x, x).is_finite(),
			"AABB with all components finite should be finite");

	CHECK_FALSE_MESSAGE(
			AABB(infinite, x).is_finite(),
			"AABB with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			AABB(x, infinite).is_finite(),
			"AABB with one component infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			AABB(infinite, infinite).is_finite(),
			"AABB with two components infinite should not be finite.");
}

} // namespace TestAABB
