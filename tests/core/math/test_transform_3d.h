/**************************************************************************/
/*  test_transform_3d.h                                                   */
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

#include "core/math/transform_3d.h"

#include "tests/test_macros.h"

namespace TestTransform3D {

Transform3D create_dummy_transform() {
	return Transform3D(Basis(Vector3(1, 2, 3), Vector3(4, 5, 6), Vector3(7, 8, 9)), Vector3(10, 11, 12));
}

Transform3D identity() {
	return Transform3D();
}

TEST_CASE("[Transform3D] translation") {
	Vector3 offset = Vector3(1, 2, 3);

	// Both versions should give the same result applied to identity.
	CHECK(identity().translated(offset) == identity().translated_local(offset));

	// Check both versions against left and right multiplications.
	Transform3D orig = create_dummy_transform();
	Transform3D T = identity().translated(offset);
	CHECK(orig.translated(offset) == T * orig);
	CHECK(orig.translated_local(offset) == orig * T);
}

TEST_CASE("[Transform3D] scaling") {
	Vector3 scaling = Vector3(1, 2, 3);

	// Both versions should give the same result applied to identity.
	CHECK(identity().scaled(scaling) == identity().scaled_local(scaling));

	// Check both versions against left and right multiplications.
	Transform3D orig = create_dummy_transform();
	Transform3D S = identity().scaled(scaling);
	CHECK(orig.scaled(scaling) == S * orig);
	CHECK(orig.scaled_local(scaling) == orig * S);
}

TEST_CASE("[Transform3D] rotation") {
	Vector3 axis = Vector3(1, 2, 3).normalized();
	real_t phi = 1.0;

	// Both versions should give the same result applied to identity.
	CHECK(identity().rotated(axis, phi) == identity().rotated_local(axis, phi));

	// Check both versions against left and right multiplications.
	Transform3D orig = create_dummy_transform();
	Transform3D R = identity().rotated(axis, phi);
	CHECK(orig.rotated(axis, phi) == R * orig);
	CHECK(orig.rotated_local(axis, phi) == orig * R);
}

TEST_CASE("[Transform3D] Finite number checks") {
	const Vector3 y(0, 1, 2);
	const Vector3 infinite_vec(NAN, NAN, NAN);
	const Basis x(y, y, y);
	const Basis infinite_basis(infinite_vec, infinite_vec, infinite_vec);

	CHECK_MESSAGE(
			Transform3D(x, y).is_finite(),
			"Transform3D with all components finite should be finite");

	CHECK_FALSE_MESSAGE(
			Transform3D(x, infinite_vec).is_finite(),
			"Transform3D with one component infinite should not be finite.");
	CHECK_FALSE_MESSAGE(
			Transform3D(infinite_basis, y).is_finite(),
			"Transform3D with one component infinite should not be finite.");

	CHECK_FALSE_MESSAGE(
			Transform3D(infinite_basis, infinite_vec).is_finite(),
			"Transform3D with two components infinite should not be finite.");
}

TEST_CASE("[Transform3D] Rotate around global origin") {
	// Start with the default orientation, but not centered on the origin.
	// Rotating should rotate both our basis and the origin.
	Transform3D transform = Transform3D();
	transform.origin = Vector3(0, 0, 1);

	Transform3D expected = Transform3D();
	expected.origin = Vector3(0, 0, -1);
	expected.basis[0] = Vector3(-1, 0, 0);
	expected.basis[2] = Vector3(0, 0, -1);

	const Transform3D rotated_transform = transform.rotated(Vector3(0, 1, 0), Math_PI);
	CHECK_MESSAGE(rotated_transform.is_equal_approx(expected), "The rotated transform should have a new orientation and basis.");
}

TEST_CASE("[Transform3D] Rotate in-place (local rotation)") {
	// Start with the default orientation.
	// Local rotation should not change the origin, only the basis.
	Transform3D transform = Transform3D();
	transform.origin = Vector3(1, 2, 3);

	Transform3D expected = Transform3D();
	expected.origin = Vector3(1, 2, 3);
	expected.basis[0] = Vector3(-1, 0, 0);
	expected.basis[2] = Vector3(0, 0, -1);

	const Transform3D rotated_transform = Transform3D(transform.rotated_local(Vector3(0, 1, 0), Math_PI));
	CHECK_MESSAGE(rotated_transform.is_equal_approx(expected), "The rotated transform should have a new orientation but still be based on the same origin.");
}
} // namespace TestTransform3D
