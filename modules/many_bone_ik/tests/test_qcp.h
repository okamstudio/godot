/**************************************************************************/
/*  test_qcp.h                                                            */
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

#ifndef TEST_QCP_H
#define TEST_QCP_H

#include "core/math/quaternion.h"
#include "modules/many_bone_ik/src/math/qcp.h"
#include "tests/test_macros.h"

namespace TestQCP {

TEST_CASE("[Modules][QCP] Weighted Superpose") {
	double epsilon = CMP_EPSILON;
	QCP qcp(epsilon);

	Quaternion expected = Quaternion(0, 0, sqrt(2) / 2, sqrt(2) / 2);
	PackedVector3Array moved = { Vector3(4, 5, 6), Vector3(7, 8, 9), Vector3(1, 2, 3) };
	PackedVector3Array target = moved;
	for (Vector3 &element : target) {
		element = expected.xform(element);
	}
	Vector<double> weight = { 1.0, 1.0, 1.0 }; // Equal weights

	Quaternion result = qcp.weighted_superpose(moved, target, weight, false);
	CHECK(abs(result.x - expected.x) < epsilon);
	CHECK(abs(result.y - expected.y) < epsilon);
	CHECK(abs(result.z - expected.z) < epsilon);
	CHECK(abs(result.w - expected.w) < epsilon);
}

TEST_CASE("[Modules][QCP] Weighted Translation") {
	double epsilon = CMP_EPSILON;
	QCP qcp(epsilon);

	Quaternion expected;
	PackedVector3Array moved = { Vector3(4, 5, 6), Vector3(7, 8, 9), Vector3(1, 2, 3) };
	PackedVector3Array target = moved;
	Vector3 translation_vector = Vector3(1, 2, 3);
	for (Vector3 &element : target) {
		element = expected.xform(element + translation_vector);
	}
	Vector<double> weight = { 1.0, 1.0, 1.0 }; // Equal weights
	bool translate = true;

	Quaternion result = qcp.weighted_superpose(moved, target, weight, translate);
	CHECK(abs(result.x - expected.x) < epsilon);
	CHECK(abs(result.y - expected.y) < epsilon);
	CHECK(abs(result.z - expected.z) < epsilon);
	CHECK(abs(result.w - expected.w) < epsilon);

	// Check if translation occurred
	CHECK(translate);
	Vector3 translation_result = expected.xform_inv(qcp.get_translation());
	CHECK(abs(translation_result.x - translation_vector.x) < epsilon);
	CHECK(abs(translation_result.y - translation_vector.y) < epsilon);
	CHECK(abs(translation_result.z - translation_vector.z) < epsilon);
}

TEST_CASE("[Modules][QCP] Weighted Translation Shortest Path") {
	double epsilon = CMP_EPSILON;
	QCP qcp(epsilon);

	Quaternion expected = Quaternion(1, 2, 3, 4).normalized();
	PackedVector3Array moved = { Vector3(4, 5, 6), Vector3(7, 8, 9), Vector3(1, 2, 3) };
	PackedVector3Array target = moved;
	Vector3 translation_vector = Vector3(1, 2, 3);
	for (Vector3 &element : target) {
		element = expected.xform(element + translation_vector);
	}
	Vector<double> weight = { 1.0, 1.0, 1.0 }; // Equal weights
	bool translate = true;

	Quaternion result = qcp.weighted_superpose(moved, target, weight, translate);
	CHECK(abs(result.x - expected.x) > epsilon);
	CHECK(abs(result.y - expected.y) > epsilon);
	CHECK(abs(result.z - expected.z) > epsilon);
	CHECK(abs(result.w - expected.w) > epsilon);

	// Check if translation occurred
	CHECK(translate);
	Vector3 translation_result = expected.xform_inv(qcp.get_translation());
	CHECK(abs(translation_result.x - translation_vector.x) > epsilon);
	CHECK(abs(translation_result.y - translation_vector.y) > epsilon);
	CHECK(abs(translation_result.z - translation_vector.z) > epsilon);
}
} // namespace TestQCP

#endif // TEST_QCP_H
