/**************************************************************************/
/*  test_validate_testing.h                                               */
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

#include "core/core_globals.h"
#include "core/os/os.h"

#include "tests/test_macros.h"
#include "tests/test_tools.h"

TEST_SUITE("Validate tests") {
	TEST_CASE("Always pass") {
		CHECK(true);
	}
	TEST_CASE_PENDING("Pending tests are skipped") {
		if (!doctest::getContextOptions()->no_skip) { // Normal run.
			FAIL("This should be skipped if `--no-skip` is NOT set (missing `doctest::skip()` decorator?)");
		} else {
			CHECK_MESSAGE(true, "Pending test is run with `--no-skip`");
		}
	}
	TEST_CASE("Muting Godot error messages") {
		ERR_PRINT_OFF;
		CHECK_MESSAGE(!CoreGlobals::print_error_enabled, "Error printing should be disabled.");
		ERR_PRINT("Still waiting for Godot!"); // This should never get printed!
		ERR_PRINT_ON;
		CHECK_MESSAGE(CoreGlobals::print_error_enabled, "Error printing should be re-enabled.");
	}
	TEST_CASE("Stringify Variant types") {
		Variant var;
		INFO(var);

		String string("Godot is finally here!");
		INFO(string);

		Vector2 vec2(0.5, 1.0);
		INFO(vec2);

		Vector2i vec2i(1, 2);
		INFO(vec2i);

		Rect2 rect2(0.5, 0.5, 100.5, 100.5);
		INFO(rect2);

		Rect2i rect2i(0, 0, 100, 100);
		INFO(rect2i);

		Vector3 vec3(0.5, 1.0, 2.0);
		INFO(vec3);

		Vector3i vec3i(1, 2, 3);
		INFO(vec3i);

		Transform2D trans2d(0.5, Vector2(100, 100));
		INFO(trans2d);

		Plane plane(Vector3(1, 1, 1), 1.0);
		INFO(plane);

		Quaternion quat = Quaternion::from_euler(Vector3(0.5, 1.0, 2.0));
		INFO(quat);

		AABB aabb(Vector3(), Vector3(100, 100, 100));
		INFO(aabb);

		Basis basis(quat);
		INFO(basis);

		Transform3D trans(basis);
		INFO(trans);

		Color color(1, 0.5, 0.2, 0.3);
		INFO(color);

		StringName string_name("has_method");
		INFO(string_name);

		NodePath node_path("godot/sprite");
		INFO(node_path);

		INFO(RID());

		Object *obj = memnew(Object);
		INFO(obj);

		Callable callable(obj, "has_method");
		INFO(callable);

		Signal signal(obj, "script_changed");
		INFO(signal);

		memdelete(obj);

		Dictionary dict;
		dict["string"] = string;
		dict["color"] = color;
		INFO(dict);

		Array arr;
		arr.push_back(string);
		arr.push_back(color);
		INFO(arr);

		PackedByteArray byte_arr;
		byte_arr.push_back(0);
		byte_arr.push_back(1);
		byte_arr.push_back(2);
		INFO(byte_arr);

		PackedInt32Array int32_arr;
		int32_arr.push_back(0);
		int32_arr.push_back(1);
		int32_arr.push_back(2);
		INFO(int32_arr);

		PackedInt64Array int64_arr;
		int64_arr.push_back(0);
		int64_arr.push_back(1);
		int64_arr.push_back(2);
		INFO(int64_arr);

		PackedFloat32Array float32_arr;
		float32_arr.push_back(0.5);
		float32_arr.push_back(1.5);
		float32_arr.push_back(2.5);
		INFO(float32_arr);

		PackedFloat64Array float64_arr;
		float64_arr.push_back(0.5);
		float64_arr.push_back(1.5);
		float64_arr.push_back(2.5);
		INFO(float64_arr);

		PackedStringArray str_arr = string.split(" ");
		INFO(str_arr);

		PackedVector2Array vec2_arr;
		vec2_arr.push_back(Vector2(0, 0));
		vec2_arr.push_back(Vector2(1, 1));
		vec2_arr.push_back(Vector2(2, 2));
		INFO(vec2_arr);

		PackedVector3Array vec3_arr;
		vec3_arr.push_back(Vector3(0, 0, 0));
		vec3_arr.push_back(Vector3(1, 1, 1));
		vec3_arr.push_back(Vector3(2, 2, 2));
		INFO(vec3_arr);

		PackedColorArray color_arr;
		color_arr.push_back(Color(0, 0, 0));
		color_arr.push_back(Color(1, 1, 1));
		color_arr.push_back(Color(2, 2, 2));
		INFO(color_arr);

		PackedVector4Array vec4_arr;
		vec4_arr.push_back(Vector4(0, 0, 0, 0));
		vec4_arr.push_back(Vector4(1, 1, 1, 1));
		vec4_arr.push_back(Vector4(2, 2, 2, 2));
		vec4_arr.push_back(Vector4(3, 3, 3, 3));
		INFO(vec4_arr);

		// doctest string concatenation.
		CHECK_MESSAGE(true, var, " ", vec2, " ", rect2, " ", color);
	}
	TEST_CASE("Detect error messages") {
		ErrorDetector ed;

		REQUIRE_FALSE(ed.has_error);

		ERR_PRINT_OFF;
		ERR_PRINT("Still waiting for Godot!");
		ERR_PRINT_ON;

		REQUIRE(ed.has_error);
	}
}
