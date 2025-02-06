/**************************************************************************/
/*  test_transform_2d.h                                                   */
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

#include "core/math/transform_2d.h"

#include "tests/test_macros.h"

namespace TestTransform2D {

Transform2D create_dummy_transform() {
	return Transform2D(Vector2(1, 2), Vector2(3, 4), Vector2(5, 6));
}

Transform2D identity() {
	return Transform2D();
}

TEST_CASE("[Transform2D] Default constructor") {
	Transform2D default_constructor = Transform2D();
	CHECK(default_constructor == Transform2D(Vector2(1, 0), Vector2(0, 1), Vector2(0, 0)));
}

TEST_CASE("[Transform2D] Copy constructor") {
	Transform2D T = create_dummy_transform();
	Transform2D copy_constructor = Transform2D(T);
	CHECK(T == copy_constructor);
}

TEST_CASE("[Transform2D] Constructor from angle and position") {
	constexpr float ROTATION = Math_PI / 4;
	const Vector2 TRANSLATION = Vector2(20, -20);

	const Transform2D test = Transform2D(ROTATION, TRANSLATION);
	const Transform2D expected = Transform2D().rotated(ROTATION).translated(TRANSLATION);
	CHECK(test == expected);
}

TEST_CASE("[Transform2D] Constructor from angle, scale, skew and position") {
	constexpr float ROTATION = Math_PI / 2;
	const Vector2 SCALE = Vector2(2, 0.5);
	constexpr float SKEW = Math_PI / 4;
	const Vector2 TRANSLATION = Vector2(30, 0);

	const Transform2D test = Transform2D(ROTATION, SCALE, SKEW, TRANSLATION);
	Transform2D expected = Transform2D().scaled(SCALE).rotated(ROTATION).translated(TRANSLATION);
	expected.set_skew(SKEW);

	CHECK(test.is_equal_approx(expected));
}

TEST_CASE("[Transform2D] Constructor from raw values") {
	const Transform2D test = Transform2D(1, 2, 3, 4, 5, 6);
	const Transform2D expected = Transform2D(Vector2(1, 2), Vector2(3, 4), Vector2(5, 6));
	CHECK(test == expected);
}

TEST_CASE("[Transform2D] xform") {
	const Vector2 v = Vector2(2, 3);
	const Transform2D T = Transform2D(Vector2(1, 2), Vector2(3, 4), Vector2(5, 6));
	const Vector2 expected = Vector2(1 * 2 + 3 * 3 + 5 * 1, 2 * 2 + 4 * 3 + 6 * 1);
	CHECK(T.xform(v) == expected);
}

TEST_CASE("[Transform2D] Basis xform") {
	const Vector2 v = Vector2(2, 2);
	const Transform2D T1 = Transform2D(Vector2(1, 2), Vector2(3, 4), Vector2(0, 0));

	// Both versions should be the same when the origin is (0,0).
	CHECK(T1.basis_xform(v) == T1.xform(v));

	const Transform2D T2 = Transform2D(Vector2(1, 2), Vector2(3, 4), Vector2(5, 6));

	// Each version should be different when the origin is not (0,0).
	CHECK_FALSE(T2.basis_xform(v) == T2.xform(v));
}

TEST_CASE("[Transform2D] Affine inverse") {
	const Transform2D orig = create_dummy_transform();
	const Transform2D affine_inverted = orig.affine_inverse();
	const Transform2D affine_inverted_again = affine_inverted.affine_inverse();
	CHECK(affine_inverted_again == orig);
}

TEST_CASE("[Transform2D] Orthonormalized") {
	const Transform2D T = create_dummy_transform();
	const Transform2D orthonormalized_T = T.orthonormalized();

	// Check each basis has length 1.
	CHECK(Math::is_equal_approx(orthonormalized_T[0].length_squared(), 1));
	CHECK(Math::is_equal_approx(orthonormalized_T[1].length_squared(), 1));

	const Vector2 vx = Vector2(orthonormalized_T[0].x, orthonormalized_T[1].x);
	const Vector2 vy = Vector2(orthonormalized_T[0].y, orthonormalized_T[1].y);

	// Check the basis are orthogonal.
	CHECK(Math::is_equal_approx(orthonormalized_T.tdotx(vx), 1));
	CHECK(Math::is_equal_approx(orthonormalized_T.tdotx(vy), 0));
	CHECK(Math::is_equal_approx(orthonormalized_T.tdoty(vx), 0));
	CHECK(Math::is_equal_approx(orthonormalized_T.tdoty(vy), 1));
}

TEST_CASE("[Transform2D] translation") {
	const Vector2 offset = Vector2(1, 2);

	// Both versions should give the same result applied to identity.
	CHECK(identity().translated(offset) == identity().translated_local(offset));

	// Check both versions against left and right multiplications.
	const Transform2D orig = create_dummy_transform();
	const Transform2D T = identity().translated(offset);
	CHECK(orig.translated(offset) == T * orig);
	CHECK(orig.translated_local(offset) == orig * T);
}

TEST_CASE("[Transform2D] scaling") {
	const Vector2 scaling = Vector2(1, 2);

	// Both versions should give the same result applied to identity.
	CHECK(identity().scaled(scaling) == identity().scaled_local(scaling));

	// Check both versions against left and right multiplications.
	const Transform2D orig = create_dummy_transform();
	const Transform2D S = identity().scaled(scaling);
	CHECK(orig.scaled(scaling) == S * orig);
	CHECK(orig.scaled_local(scaling) == orig * S);
}

TEST_CASE("[Transform2D] rotation") {
	constexpr real_t phi = 1.0;

	// Both versions should give the same result applied to identity.
	CHECK(identity().rotated(phi) == identity().rotated_local(phi));

	// Check both versions against left and right multiplications.
	const Transform2D orig = create_dummy_transform();
	const Transform2D R = identity().rotated(phi);
	CHECK(orig.rotated(phi) == R * orig);
	CHECK(orig.rotated_local(phi) == orig * R);
}

TEST_CASE("[Transform2D] Interpolation") {
	const Transform2D rotate_scale_skew_pos = Transform2D(Math::deg_to_rad(170.0), Vector2(3.6, 8.0), Math::deg_to_rad(20.0), Vector2(2.4, 6.8));
	const Transform2D rotate_scale_skew_pos_halfway = Transform2D(Math::deg_to_rad(85.0), Vector2(2.3, 4.5), Math::deg_to_rad(10.0), Vector2(1.2, 3.4));
	Transform2D interpolated = Transform2D().interpolate_with(rotate_scale_skew_pos, 0.5);
	CHECK(interpolated.get_origin().is_equal_approx(rotate_scale_skew_pos_halfway.get_origin()));
	CHECK(interpolated.get_rotation() == doctest::Approx(rotate_scale_skew_pos_halfway.get_rotation()));
	CHECK(interpolated.get_scale().is_equal_approx(rotate_scale_skew_pos_halfway.get_scale()));
	CHECK(interpolated.get_skew() == doctest::Approx(rotate_scale_skew_pos_halfway.get_skew()));
	CHECK(interpolated.is_equal_approx(rotate_scale_skew_pos_halfway));
	interpolated = rotate_scale_skew_pos.interpolate_with(Transform2D(), 0.5);
	CHECK(interpolated.is_equal_approx(rotate_scale_skew_pos_halfway));
}

TEST_CASE("[Transform2D] Finite number checks") {
	const Vector2 x = Vector2(0, 1);
	const Vector2 infinite = Vector2(NAN, NAN);

	CHECK_MESSAGE(
			Transform2D(x, x, x).is_finite(),
			"Transform2D with all components finite should be finite");

	CHECK_FALSE_MESSAGE(
			Transform2D(infinite, x, x).is_finite(),
			"Transform2D with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Transform2D(x, infinite, x).is_finite(),
			"Transform2D with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Transform2D(x, x, infinite).is_finite(),
			"Transform2D with one component infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Transform2D(infinite, infinite, x).is_finite(),
			"Transform2D with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Transform2D(infinite, x, infinite).is_finite(),
			"Transform2D with two components infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Transform2D(x, infinite, infinite).is_finite(),
			"Transform2D with two components infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Transform2D(infinite, infinite, infinite).is_finite(),
			"Transform2D with three components infinite should not be finite.");
}

TEST_CASE("[Transform2D] Is conformal checks") {
	CHECK_MESSAGE(
			Transform2D().is_conformal(),
			"Identity Transform2D should be conformal.");

	CHECK_MESSAGE(
			Transform2D(1.2, Vector2()).is_conformal(),
			"Transform2D with only rotation should be conformal.");

	CHECK_MESSAGE(
			Transform2D(Vector2(1, 0), Vector2(0, -1), Vector2()).is_conformal(),
			"Transform2D with only a flip should be conformal.");

	CHECK_MESSAGE(
			Transform2D(Vector2(1.2, 0), Vector2(0, 1.2), Vector2()).is_conformal(),
			"Transform2D with only uniform scale should be conformal.");

	CHECK_MESSAGE(
			Transform2D(Vector2(1.2, 3.4), Vector2(3.4, -1.2), Vector2()).is_conformal(),
			"Transform2D with a flip, rotation, and uniform scale should be conformal.");

	CHECK_FALSE_MESSAGE(
			Transform2D(Vector2(1.2, 0), Vector2(0, 3.4), Vector2()).is_conformal(),
			"Transform2D with non-uniform scale should not be conformal.");

	CHECK_FALSE_MESSAGE(
			Transform2D(Vector2(Math_SQRT12, Math_SQRT12), Vector2(0, 1), Vector2()).is_conformal(),
			"Transform2D with the X axis skewed 45 degrees should not be conformal.");
}

} // namespace TestTransform2D
