/**************************************************************************/
/*  test_variant.h                                                        */
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

#include "core/variant/variant.h"
#include "core/variant/variant_parser.h"

#include "tests/test_macros.h"

namespace TestVariant {

static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}
static inline Dictionary build_dictionary() {
	return Dictionary();
}
template <typename... Targs>
static inline Dictionary build_dictionary(Variant key, Variant item, Targs... Fargs) {
	Dictionary d = build_dictionary(Fargs...);
	d[key] = item;
	return d;
}

TEST_CASE("[Variant] Writer and parser integer") {
	int64_t a32 = 2147483648; // 2^31, so out of bounds for 32-bit signed int [-2^31, +2^31-1].
	String a32_str;
	VariantWriter::write_to_string(a32, a32_str);

	CHECK_MESSAGE(a32_str != "-2147483648", "Should not wrap around");

	int64_t b64 = 9223372036854775807; // 2^63-1, upper bound for signed 64-bit int.
	String b64_str;
	VariantWriter::write_to_string(b64, b64_str);

	CHECK_MESSAGE(b64_str == "9223372036854775807", "Should not wrap around.");

	VariantParser::StreamString ss;
	String errs;
	int line;
	Variant b64_parsed;
	int64_t b64_int_parsed;

	ss.s = b64_str;
	VariantParser::parse(&ss, b64_parsed, errs, line);
	b64_int_parsed = b64_parsed;

	CHECK_MESSAGE(b64_int_parsed == 9223372036854775807, "Should parse back.");

	ss.s = "9223372036854775808"; // Overflowed by one.
	VariantParser::parse(&ss, b64_parsed, errs, line);
	b64_int_parsed = b64_parsed;

	CHECK_MESSAGE(b64_int_parsed == 9223372036854775807, "The result should be clamped to max value.");

	ss.s = "1e100"; // Googol! Scientific notation.
	VariantParser::parse(&ss, b64_parsed, errs, line);
	b64_int_parsed = b64_parsed;

	CHECK_MESSAGE(b64_int_parsed == 9223372036854775807, "The result should be clamped to max value.");
}

TEST_CASE("[Variant] Writer and parser Variant::FLOAT") {
	// Variant::FLOAT is always 64-bit (C++ double).
	// This is the maximum non-infinity double-precision float.
	double a64 = 179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0;
	String a64_str;
	VariantWriter::write_to_string(a64, a64_str);

	CHECK_MESSAGE(a64_str == "1.79769e+308", "Writes in scientific notation.");
	CHECK_MESSAGE(a64_str != "inf", "Should not overflow.");
	CHECK_MESSAGE(a64_str != "nan", "The result should be defined.");

	String errs;
	int line;
	Variant variant_parsed;
	double float_parsed;

	VariantParser::StreamString bss;
	bss.s = a64_str;
	VariantParser::parse(&bss, variant_parsed, errs, line);
	float_parsed = variant_parsed;
	// Loses precision, but that's alright.
	CHECK_MESSAGE(float_parsed == 1.79769e+308, "Should parse back.");

	// Approximation of Googol with a double-precision float.
	VariantParser::StreamString css;
	css.s = "1.0e+100";
	VariantParser::parse(&css, variant_parsed, errs, line);
	float_parsed = variant_parsed;
	CHECK_MESSAGE(float_parsed == 1.0e+100, "Should match the double literal.");
}

TEST_CASE("[Variant] Assignment To Bool from Int,Float,String,Vec2,Vec2i,Vec3,Vec3i,Vec4,Vec4i,Rect2,Rect2i,Trans2d,Trans3d,Color,Call,Plane,Basis,AABB,Quant,Proj,RID,and Object") {
	Variant int_v = 0;
	Variant bool_v = true;
	int_v = bool_v; // int_v is now a bool
	CHECK(int_v == Variant(true));
	bool_v = false;
	int_v = bool_v;
	CHECK(int_v.get_type() == Variant::BOOL);

	Variant float_v = 0.0f;
	bool_v = true;
	float_v = bool_v;
	CHECK(float_v == Variant(true));
	bool_v = false;
	float_v = bool_v;
	CHECK(float_v.get_type() == Variant::BOOL);

	Variant string_v = "";
	bool_v = true;
	string_v = bool_v;
	CHECK(string_v == Variant(true));
	bool_v = false;
	string_v = bool_v;
	CHECK(string_v.get_type() == Variant::BOOL);

	Variant vec2_v = Vector2(0, 0);
	bool_v = true;
	vec2_v = bool_v;
	CHECK(vec2_v == Variant(true));
	bool_v = false;
	vec2_v = bool_v;
	CHECK(vec2_v.get_type() == Variant::BOOL);

	Variant vec2i_v = Vector2i(0, 0);
	bool_v = true;
	vec2i_v = bool_v;
	CHECK(vec2i_v == Variant(true));
	bool_v = false;
	vec2i_v = bool_v;
	CHECK(vec2i_v.get_type() == Variant::BOOL);

	Variant vec3_v = Vector3(0, 0, 0);
	bool_v = true;
	vec3_v = bool_v;
	CHECK(vec3_v == Variant(true));
	bool_v = false;
	vec3_v = bool_v;
	CHECK(vec3_v.get_type() == Variant::BOOL);

	Variant vec3i_v = Vector3i(0, 0, 0);
	bool_v = true;
	vec3i_v = bool_v;
	CHECK(vec3i_v == Variant(true));
	bool_v = false;
	vec3i_v = bool_v;
	CHECK(vec3i_v.get_type() == Variant::BOOL);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	bool_v = true;
	vec4_v = bool_v;
	CHECK(vec4_v == Variant(true));
	bool_v = false;
	vec4_v = bool_v;
	CHECK(vec4_v.get_type() == Variant::BOOL);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	bool_v = true;
	vec4i_v = bool_v;
	CHECK(vec4i_v == Variant(true));
	bool_v = false;
	vec4i_v = bool_v;
	CHECK(vec4i_v.get_type() == Variant::BOOL);

	Variant rect2_v = Rect2();
	bool_v = true;
	rect2_v = bool_v;
	CHECK(rect2_v == Variant(true));
	bool_v = false;
	rect2_v = bool_v;
	CHECK(rect2_v.get_type() == Variant::BOOL);

	Variant rect2i_v = Rect2i();
	bool_v = true;
	rect2i_v = bool_v;
	CHECK(rect2i_v == Variant(true));
	bool_v = false;
	rect2i_v = bool_v;
	CHECK(rect2i_v.get_type() == Variant::BOOL);

	Variant transform2d_v = Transform2D();
	bool_v = true;
	transform2d_v = bool_v;
	CHECK(transform2d_v == Variant(true));
	bool_v = false;
	transform2d_v = bool_v;
	CHECK(transform2d_v.get_type() == Variant::BOOL);

	Variant transform3d_v = Transform3D();
	bool_v = true;
	transform3d_v = bool_v;
	CHECK(transform3d_v == Variant(true));
	bool_v = false;
	transform3d_v = bool_v;
	CHECK(transform3d_v.get_type() == Variant::BOOL);

	Variant col_v = Color(0.5f, 0.2f, 0.75f);
	bool_v = true;
	col_v = bool_v;
	CHECK(col_v == Variant(true));
	bool_v = false;
	col_v = bool_v;
	CHECK(col_v.get_type() == Variant::BOOL);

	Variant call_v = Callable();
	bool_v = true;
	call_v = bool_v;
	CHECK(call_v == Variant(true));
	bool_v = false;
	call_v = bool_v;
	CHECK(call_v.get_type() == Variant::BOOL);

	Variant plane_v = Plane();
	bool_v = true;
	plane_v = bool_v;
	CHECK(plane_v == Variant(true));
	bool_v = false;
	plane_v = bool_v;
	CHECK(plane_v.get_type() == Variant::BOOL);

	Variant basis_v = Basis();
	bool_v = true;
	basis_v = bool_v;
	CHECK(basis_v == Variant(true));
	bool_v = false;
	basis_v = bool_v;
	CHECK(basis_v.get_type() == Variant::BOOL);

	Variant aabb_v = AABB();
	bool_v = true;
	aabb_v = bool_v;
	CHECK(aabb_v == Variant(true));
	bool_v = false;
	aabb_v = bool_v;
	CHECK(aabb_v.get_type() == Variant::BOOL);

	Variant quaternion_v = Quaternion();
	bool_v = true;
	quaternion_v = bool_v;
	CHECK(quaternion_v == Variant(true));
	bool_v = false;
	quaternion_v = bool_v;
	CHECK(quaternion_v.get_type() == Variant::BOOL);

	Variant projection_v = Projection();
	bool_v = true;
	projection_v = bool_v;
	CHECK(projection_v == Variant(true));
	bool_v = false;
	projection_v = bool_v;
	CHECK(projection_v.get_type() == Variant::BOOL);

	Variant rid_v = RID();
	bool_v = true;
	rid_v = bool_v;
	CHECK(rid_v == Variant(true));
	bool_v = false;
	rid_v = bool_v;
	CHECK(rid_v.get_type() == Variant::BOOL);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	bool_v = true;
	object_v = bool_v;
	CHECK(object_v == Variant(true));
	bool_v = false;
	object_v = bool_v;
	CHECK(object_v.get_type() == Variant::BOOL);
}

TEST_CASE("[Variant] Assignment To Int from Bool,Float,String,Vec2,Vec2i,Vec3,Vec3i Vec4,Vec4i,Rect2,Rect2i,Trans2d,Trans3d,Color,Call,Plane,Basis,AABB,Quant,Proj,RID,and Object") {
	Variant bool_v = false;
	Variant int_v = 2;
	bool_v = int_v; // Now bool_v is int
	CHECK(bool_v == Variant(2));
	int_v = -3;
	bool_v = int_v;
	CHECK(bool_v.get_type() == Variant::INT);

	Variant float_v = 0.0f;
	int_v = 2;
	float_v = int_v;
	CHECK(float_v == Variant(2));
	int_v = -3;
	float_v = int_v;
	CHECK(float_v.get_type() == Variant::INT);

	Variant string_v = "";
	int_v = 2;
	string_v = int_v;
	CHECK(string_v == Variant(2));
	int_v = -3;
	string_v = int_v;
	CHECK(string_v.get_type() == Variant::INT);

	Variant vec2_v = Vector2(0, 0);
	int_v = 2;
	vec2_v = int_v;
	CHECK(vec2_v == Variant(2));
	int_v = -3;
	vec2_v = int_v;
	CHECK(vec2_v.get_type() == Variant::INT);

	Variant vec2i_v = Vector2i(0, 0);
	int_v = 2;
	vec2i_v = int_v;
	CHECK(vec2i_v == Variant(2));
	int_v = -3;
	vec2i_v = int_v;
	CHECK(vec2i_v.get_type() == Variant::INT);

	Variant vec3_v = Vector3(0, 0, 0);
	int_v = 2;
	vec3_v = int_v;
	CHECK(vec3_v == Variant(2));
	int_v = -3;
	vec3_v = int_v;
	CHECK(vec3_v.get_type() == Variant::INT);

	Variant vec3i_v = Vector3i(0, 0, 0);
	int_v = 2;
	vec3i_v = int_v;
	CHECK(vec3i_v == Variant(2));
	int_v = -3;
	vec3i_v = int_v;
	CHECK(vec3i_v.get_type() == Variant::INT);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	int_v = 2;
	vec4_v = int_v;
	CHECK(vec4_v == Variant(2));
	int_v = -3;
	vec4_v = int_v;
	CHECK(vec4_v.get_type() == Variant::INT);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	int_v = 2;
	vec4i_v = int_v;
	CHECK(vec4i_v == Variant(2));
	int_v = -3;
	vec4i_v = int_v;
	CHECK(vec4i_v.get_type() == Variant::INT);

	Variant rect2_v = Rect2();
	int_v = 2;
	rect2_v = int_v;
	CHECK(rect2_v == Variant(2));
	int_v = -3;
	rect2_v = int_v;
	CHECK(rect2_v.get_type() == Variant::INT);

	Variant rect2i_v = Rect2i();
	int_v = 2;
	rect2i_v = int_v;
	CHECK(rect2i_v == Variant(2));
	int_v = -3;
	rect2i_v = int_v;
	CHECK(rect2i_v.get_type() == Variant::INT);

	Variant transform2d_v = Transform2D();
	int_v = 2;
	transform2d_v = int_v;
	CHECK(transform2d_v == Variant(2));
	int_v = -3;
	transform2d_v = int_v;
	CHECK(transform2d_v.get_type() == Variant::INT);

	Variant transform3d_v = Transform3D();
	int_v = 2;
	transform3d_v = int_v;
	CHECK(transform3d_v == Variant(2));
	int_v = -3;
	transform3d_v = int_v;
	CHECK(transform3d_v.get_type() == Variant::INT);

	Variant col_v = Color(0.5f, 0.2f, 0.75f);
	int_v = 2;
	col_v = int_v;
	CHECK(col_v == Variant(2));
	int_v = -3;
	col_v = int_v;
	CHECK(col_v.get_type() == Variant::INT);

	Variant call_v = Callable();
	int_v = 2;
	call_v = int_v;
	CHECK(call_v == Variant(2));
	int_v = -3;
	call_v = int_v;
	CHECK(call_v.get_type() == Variant::INT);

	Variant plane_v = Plane();
	int_v = 2;
	plane_v = int_v;
	CHECK(plane_v == Variant(2));
	int_v = -3;
	plane_v = int_v;
	CHECK(plane_v.get_type() == Variant::INT);

	Variant basis_v = Basis();
	int_v = 2;
	basis_v = int_v;
	CHECK(basis_v == Variant(2));
	int_v = -3;
	basis_v = int_v;
	CHECK(basis_v.get_type() == Variant::INT);

	Variant aabb_v = AABB();
	int_v = 2;
	aabb_v = int_v;
	CHECK(aabb_v == Variant(2));
	int_v = -3;
	aabb_v = int_v;
	CHECK(aabb_v.get_type() == Variant::INT);

	Variant quaternion_v = Quaternion();
	int_v = 2;
	quaternion_v = int_v;
	CHECK(quaternion_v == Variant(2));
	int_v = -3;
	quaternion_v = int_v;
	CHECK(quaternion_v.get_type() == Variant::INT);

	Variant projection_v = Projection();
	int_v = 2;
	projection_v = int_v;
	CHECK(projection_v == Variant(2));
	int_v = -3;
	projection_v = int_v;
	CHECK(projection_v.get_type() == Variant::INT);

	Variant rid_v = RID();
	int_v = 2;
	rid_v = int_v;
	CHECK(rid_v == Variant(2));
	bool_v = -3;
	rid_v = int_v;
	CHECK(rid_v.get_type() == Variant::INT);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	int_v = 2;
	object_v = int_v;
	CHECK(object_v == Variant(2));
	int_v = -3;
	object_v = int_v;
	CHECK(object_v.get_type() == Variant::INT);
}

TEST_CASE("[Variant] Assignment To Float from Bool,Int,String,Vec2,Vec2i,Vec3,Vec3i,Vec4,Vec4i,Rect2,Rect2i,Trans2d,Trans3d,Color,Call,Plane,Basis,AABB,Quant,Proj,RID,and Object") {
	Variant bool_v = false;
	Variant float_v = 1.5f;
	bool_v = float_v; // Now bool_v is float
	CHECK(bool_v == Variant(1.5f));
	float_v = -4.6f;
	bool_v = float_v;
	CHECK(bool_v.get_type() == Variant::FLOAT);

	Variant int_v = 1;
	float_v = 1.5f;
	int_v = float_v;
	CHECK(int_v == Variant(1.5f));
	float_v = -4.6f;
	int_v = float_v;
	CHECK(int_v.get_type() == Variant::FLOAT);

	Variant string_v = "";
	float_v = 1.5f;
	string_v = float_v;
	CHECK(string_v == Variant(1.5f));
	float_v = -4.6f;
	string_v = float_v;
	CHECK(string_v.get_type() == Variant::FLOAT);

	Variant vec2_v = Vector2(0, 0);
	float_v = 1.5f;
	vec2_v = float_v;
	CHECK(vec2_v == Variant(1.5f));
	float_v = -4.6f;
	vec2_v = float_v;
	CHECK(vec2_v.get_type() == Variant::FLOAT);

	Variant vec2i_v = Vector2i(0, 0);
	float_v = 1.5f;
	vec2i_v = float_v;
	CHECK(vec2i_v == Variant(1.5f));
	float_v = -4.6f;
	vec2i_v = float_v;
	CHECK(vec2i_v.get_type() == Variant::FLOAT);

	Variant vec3_v = Vector3(0, 0, 0);
	float_v = 1.5f;
	vec3_v = float_v;
	CHECK(vec3_v == Variant(1.5f));
	float_v = -4.6f;
	vec3_v = float_v;
	CHECK(vec3_v.get_type() == Variant::FLOAT);

	Variant vec3i_v = Vector3i(0, 0, 0);
	float_v = 1.5f;
	vec3i_v = float_v;
	CHECK(vec3i_v == Variant(1.5f));
	float_v = -4.6f;
	vec3i_v = float_v;
	CHECK(vec3i_v.get_type() == Variant::FLOAT);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	float_v = 1.5f;
	vec4_v = float_v;
	CHECK(vec4_v == Variant(1.5f));
	float_v = -4.6f;
	vec4_v = float_v;
	CHECK(vec4_v.get_type() == Variant::FLOAT);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	float_v = 1.5f;
	vec4i_v = float_v;
	CHECK(vec4i_v == Variant(1.5f));
	float_v = -4.6f;
	vec4i_v = float_v;
	CHECK(vec4i_v.get_type() == Variant::FLOAT);

	Variant rect2_v = Rect2();
	float_v = 1.5f;
	rect2_v = float_v;
	CHECK(rect2_v == Variant(1.5f));
	float_v = -4.6f;
	rect2_v = float_v;
	CHECK(rect2_v.get_type() == Variant::FLOAT);

	Variant rect2i_v = Rect2i();
	float_v = 1.5f;
	rect2i_v = float_v;
	CHECK(rect2i_v == Variant(1.5f));
	float_v = -4.6f;
	rect2i_v = float_v;
	CHECK(rect2i_v.get_type() == Variant::FLOAT);

	Variant transform2d_v = Transform2D();
	float_v = 1.5f;
	transform2d_v = float_v;
	CHECK(transform2d_v == Variant(1.5f));
	float_v = -4.6f;
	transform2d_v = float_v;
	CHECK(transform2d_v.get_type() == Variant::FLOAT);

	Variant transform3d_v = Transform3D();
	float_v = 1.5f;
	transform3d_v = float_v;
	CHECK(transform3d_v == Variant(1.5f));
	float_v = -4.6f;
	transform3d_v = float_v;
	CHECK(transform2d_v.get_type() == Variant::FLOAT);

	Variant col_v = Color(0.5f, 0.2f, 0.75f);
	float_v = 1.5f;
	col_v = float_v;
	CHECK(col_v == Variant(1.5f));
	float_v = -4.6f;
	col_v = float_v;
	CHECK(col_v.get_type() == Variant::FLOAT);

	Variant call_v = Callable();
	float_v = 1.5f;
	call_v = float_v;
	CHECK(call_v == Variant(1.5f));
	float_v = -4.6f;
	call_v = float_v;
	CHECK(call_v.get_type() == Variant::FLOAT);

	Variant plane_v = Plane();
	float_v = 1.5f;
	plane_v = float_v;
	CHECK(plane_v == Variant(1.5f));
	float_v = -4.6f;
	plane_v = float_v;
	CHECK(plane_v.get_type() == Variant::FLOAT);

	Variant basis_v = Basis();
	float_v = 1.5f;
	basis_v = float_v;
	CHECK(basis_v == Variant(1.5f));
	float_v = -4.6f;
	basis_v = float_v;
	CHECK(basis_v.get_type() == Variant::FLOAT);

	Variant aabb_v = AABB();
	float_v = 1.5f;
	aabb_v = float_v;
	CHECK(aabb_v == Variant(1.5f));
	float_v = -4.6f;
	aabb_v = float_v;
	CHECK(aabb_v.get_type() == Variant::FLOAT);

	Variant quaternion_v = Quaternion();
	float_v = 1.5f;
	quaternion_v = float_v;
	CHECK(quaternion_v == Variant(1.5f));
	float_v = -4.6f;
	quaternion_v = float_v;
	CHECK(quaternion_v.get_type() == Variant::FLOAT);

	Variant projection_v = Projection();
	float_v = 1.5f;
	projection_v = float_v;
	CHECK(projection_v == Variant(1.5f));
	float_v = -4.6f;
	projection_v = float_v;
	CHECK(projection_v.get_type() == Variant::FLOAT);

	Variant rid_v = RID();
	float_v = 1.5f;
	rid_v = float_v;
	CHECK(rid_v == Variant(1.5f));
	float_v = -4.6f;
	rid_v = float_v;
	CHECK(rid_v.get_type() == Variant::FLOAT);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	float_v = 1.5f;
	object_v = float_v;
	CHECK(object_v == Variant(1.5f));
	float_v = -4.6f;
	object_v = float_v;
	CHECK(object_v.get_type() == Variant::FLOAT);
}

TEST_CASE("[Variant] Assignment To String from Bool,Int,Float,Vec2,Vec2i,Vec3,Vec3i,Vec4,Vec4i,Rect2,Rect2i,Trans2d,Trans3d,Color,Call,Plane,Basis,AABB,Quant,Proj,RID,and Object") {
	Variant bool_v = false;
	Variant string_v = "Hello";
	bool_v = string_v; // Now bool_v is string
	CHECK(bool_v == Variant("Hello"));
	string_v = "Hello there";
	bool_v = string_v;
	CHECK(bool_v.get_type() == Variant::STRING);

	Variant int_v = 0;
	string_v = "Hello";
	int_v = string_v;
	CHECK(int_v == Variant("Hello"));
	string_v = "Hello there";
	int_v = string_v;
	CHECK(int_v.get_type() == Variant::STRING);

	Variant float_v = 0.0f;
	string_v = "Hello";
	float_v = string_v;
	CHECK(float_v == Variant("Hello"));
	string_v = "Hello there";
	float_v = string_v;
	CHECK(float_v.get_type() == Variant::STRING);

	Variant vec2_v = Vector2(0, 0);
	string_v = "Hello";
	vec2_v = string_v;
	CHECK(vec2_v == Variant("Hello"));
	string_v = "Hello there";
	vec2_v = string_v;
	CHECK(vec2_v.get_type() == Variant::STRING);

	Variant vec2i_v = Vector2i(0, 0);
	string_v = "Hello";
	vec2i_v = string_v;
	CHECK(vec2i_v == Variant("Hello"));
	string_v = "Hello there";
	vec2i_v = string_v;
	CHECK(vec2i_v.get_type() == Variant::STRING);

	Variant vec3_v = Vector3(0, 0, 0);
	string_v = "Hello";
	vec3_v = string_v;
	CHECK(vec3_v == Variant("Hello"));
	string_v = "Hello there";
	vec3_v = string_v;
	CHECK(vec3_v.get_type() == Variant::STRING);

	Variant vec3i_v = Vector3i(0, 0, 0);
	string_v = "Hello";
	vec3i_v = string_v;
	CHECK(vec3i_v == Variant("Hello"));
	string_v = "Hello there";
	vec3i_v = string_v;
	CHECK(vec3i_v.get_type() == Variant::STRING);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	string_v = "Hello";
	vec4_v = string_v;
	CHECK(vec4_v == Variant("Hello"));
	string_v = "Hello there";
	vec4_v = string_v;
	CHECK(vec4_v.get_type() == Variant::STRING);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	string_v = "Hello";
	vec4i_v = string_v;
	CHECK(vec4i_v == Variant("Hello"));
	string_v = "Hello there";
	vec4i_v = string_v;
	CHECK(vec4i_v.get_type() == Variant::STRING);

	Variant rect2_v = Rect2();
	string_v = "Hello";
	rect2_v = string_v;
	CHECK(rect2_v == Variant("Hello"));
	string_v = "Hello there";
	rect2_v = string_v;
	CHECK(rect2_v.get_type() == Variant::STRING);

	Variant rect2i_v = Rect2i();
	string_v = "Hello";
	rect2i_v = string_v;
	CHECK(rect2i_v == Variant("Hello"));
	string_v = "Hello there";
	rect2i_v = string_v;
	CHECK(rect2i_v.get_type() == Variant::STRING);

	Variant transform2d_v = Transform2D();
	string_v = "Hello";
	transform2d_v = string_v;
	CHECK(transform2d_v == Variant("Hello"));
	string_v = "Hello there";
	transform2d_v = string_v;
	CHECK(transform2d_v.get_type() == Variant::STRING);

	Variant transform3d_v = Transform3D();
	string_v = "Hello";
	transform3d_v = string_v;
	CHECK(transform3d_v == Variant("Hello"));
	string_v = "Hello there";
	transform3d_v = string_v;
	CHECK(transform3d_v.get_type() == Variant::STRING);

	Variant col_v = Color(0.5f, 0.2f, 0.75f);
	string_v = "Hello";
	col_v = string_v;
	CHECK(col_v == Variant("Hello"));
	string_v = "Hello there";
	col_v = string_v;
	CHECK(col_v.get_type() == Variant::STRING);

	Variant call_v = Callable();
	string_v = "Hello";
	call_v = string_v;
	CHECK(call_v == Variant("Hello"));
	string_v = "Hello there";
	call_v = string_v;
	CHECK(call_v.get_type() == Variant::STRING);

	Variant plane_v = Plane();
	string_v = "Hello";
	plane_v = string_v;
	CHECK(plane_v == Variant("Hello"));
	string_v = "Hello there";
	plane_v = string_v;
	CHECK(plane_v.get_type() == Variant::STRING);

	Variant basis_v = Basis();
	string_v = "Hello";
	basis_v = string_v;
	CHECK(basis_v == Variant("Hello"));
	string_v = "Hello there";
	basis_v = string_v;
	CHECK(basis_v.get_type() == Variant::STRING);

	Variant aabb_v = AABB();
	string_v = "Hello";
	aabb_v = string_v;
	CHECK(aabb_v == Variant("Hello"));
	string_v = "Hello there";
	aabb_v = string_v;
	CHECK(aabb_v.get_type() == Variant::STRING);

	Variant quaternion_v = Quaternion();
	string_v = "Hello";
	quaternion_v = string_v;
	CHECK(quaternion_v == Variant("Hello"));
	string_v = "Hello there";
	quaternion_v = string_v;
	CHECK(quaternion_v.get_type() == Variant::STRING);

	Variant projection_v = Projection();
	string_v = "Hello";
	projection_v = string_v;
	CHECK(projection_v == Variant("Hello"));
	string_v = "Hello there";
	projection_v = string_v;
	CHECK(projection_v.get_type() == Variant::STRING);

	Variant rid_v = RID();
	string_v = "Hello";
	rid_v = string_v;
	CHECK(rid_v == Variant("Hello"));
	string_v = "Hello there";
	rid_v = string_v;
	CHECK(rid_v.get_type() == Variant::STRING);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	string_v = "Hello";
	object_v = string_v;
	CHECK(object_v == Variant("Hello"));
	string_v = "Hello there";
	object_v = string_v;
	CHECK(object_v.get_type() == Variant::STRING);
}

TEST_CASE("[Variant] Assignment To Vec2 from Bool,Int,Float,String,Vec2i,Vec3,Vec3i,Vec4,Vec4i,Rect2,Rect2i,Trans2d,Trans3d,Color,Call,Plane,Basis,AABB,Quant,Proj,RID,and Object") {
	Variant bool_v = false;
	Variant vec2_v = Vector2(2.2f, 3.5f);
	bool_v = vec2_v; // Now bool_v is Vector2
	CHECK(bool_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	bool_v = vec2_v;
	CHECK(bool_v.get_type() == Variant::VECTOR2);

	Variant int_v = 0;
	vec2_v = Vector2(2.2f, 3.5f);
	int_v = vec2_v;
	CHECK(int_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	int_v = vec2_v;
	CHECK(int_v.get_type() == Variant::VECTOR2);

	Variant float_v = 0.0f;
	vec2_v = Vector2(2.2f, 3.5f);
	float_v = vec2_v;
	CHECK(float_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	float_v = vec2_v;
	CHECK(float_v.get_type() == Variant::VECTOR2);

	Variant string_v = "";
	vec2_v = Vector2(2.2f, 3.5f);
	string_v = vec2_v;
	CHECK(string_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	string_v = vec2_v;
	CHECK(string_v.get_type() == Variant::VECTOR2);

	Variant vec2i_v = Vector2i(0, 0);
	vec2_v = Vector2(2.2f, 3.5f);
	vec2i_v = vec2_v;
	CHECK(vec2i_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	vec2i_v = vec2_v;
	CHECK(vec2i_v.get_type() == Variant::VECTOR2);

	Variant vec3_v = Vector3(0, 0, 0);
	vec2_v = Vector2(2.2f, 3.5f);
	vec3_v = vec2_v;
	CHECK(vec3_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	vec3_v = vec2_v;
	CHECK(vec3_v.get_type() == Variant::VECTOR2);

	Variant vec3i_v = Vector3i(0, 0, 0);
	vec2_v = Vector2(2.2f, 3.5f);
	vec3i_v = vec2_v;
	CHECK(vec3i_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	vec3i_v = vec2_v;
	CHECK(vec3i_v.get_type() == Variant::VECTOR2);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	vec2_v = Vector2(2.2f, 3.5f);
	vec4_v = vec2_v;
	CHECK(vec4_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	vec4_v = vec2_v;
	CHECK(vec4_v.get_type() == Variant::VECTOR2);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	vec2_v = Vector2(2.2f, 3.5f);
	vec4i_v = vec2_v;
	CHECK(vec4i_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	vec4i_v = vec2_v;
	CHECK(vec4i_v.get_type() == Variant::VECTOR2);

	Variant rect2_v = Rect2();
	vec2_v = Vector2(2.2f, 3.5f);
	rect2_v = vec2_v;
	CHECK(rect2_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	rect2_v = vec2_v;
	CHECK(rect2_v.get_type() == Variant::VECTOR2);

	Variant rect2i_v = Rect2i();
	vec2_v = Vector2(2.2f, 3.5f);
	rect2i_v = vec2_v;
	CHECK(rect2i_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	rect2i_v = vec2_v;
	CHECK(rect2i_v.get_type() == Variant::VECTOR2);

	Variant transform2d_v = Transform2D();
	vec2_v = Vector2(2.2f, 3.5f);
	transform2d_v = vec2_v;
	CHECK(transform2d_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	transform2d_v = vec2_v;
	CHECK(transform2d_v.get_type() == Variant::VECTOR2);

	Variant transform3d_v = Transform3D();
	vec2_v = Vector2(2.2f, 3.5f);
	transform3d_v = vec2_v;
	CHECK(transform3d_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	transform3d_v = vec2_v;
	CHECK(transform3d_v.get_type() == Variant::VECTOR2);

	Variant col_v = Color(0.5f, 0.2f, 0.75f);
	vec2_v = Vector2(2.2f, 3.5f);
	col_v = vec2_v;
	CHECK(col_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	col_v = vec2_v;
	CHECK(col_v.get_type() == Variant::VECTOR2);

	Variant call_v = Callable();
	vec2_v = Vector2(2.2f, 3.5f);
	call_v = vec2_v;
	CHECK(call_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	call_v = vec2_v;
	CHECK(call_v.get_type() == Variant::VECTOR2);

	Variant plane_v = Plane();
	vec2_v = Vector2(2.2f, 3.5f);
	plane_v = vec2_v;
	CHECK(plane_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	plane_v = vec2_v;
	CHECK(plane_v.get_type() == Variant::VECTOR2);

	Variant basis_v = Basis();
	vec2_v = Vector2(2.2f, 3.5f);
	basis_v = vec2_v;
	CHECK(basis_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	basis_v = vec2_v;
	CHECK(basis_v.get_type() == Variant::VECTOR2);

	Variant aabb_v = AABB();
	vec2_v = Vector2(2.2f, 3.5f);
	aabb_v = vec2_v;
	CHECK(aabb_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	aabb_v = vec2_v;
	CHECK(aabb_v.get_type() == Variant::VECTOR2);

	Variant quaternion_v = Quaternion();
	vec2_v = Vector2(2.2f, 3.5f);
	quaternion_v = vec2_v;
	CHECK(quaternion_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	quaternion_v = vec2_v;
	CHECK(quaternion_v.get_type() == Variant::VECTOR2);

	Variant projection_v = Projection();
	vec2_v = Vector2(2.2f, 3.5f);
	projection_v = vec2_v;
	CHECK(projection_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	projection_v = vec2_v;
	CHECK(projection_v.get_type() == Variant::VECTOR2);

	Variant rid_v = RID();
	vec2_v = Vector2(2.2f, 3.5f);
	rid_v = vec2_v;
	CHECK(rid_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	rid_v = vec2_v;
	CHECK(rid_v.get_type() == Variant::VECTOR2);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	vec2_v = Vector2(2.2f, 3.5f);
	object_v = vec2_v;
	CHECK(object_v == Variant(Vector2(2.2f, 3.5f)));
	vec2_v = Vector2(-5.4f, -7.9f);
	object_v = vec2_v;
	CHECK(object_v.get_type() == Variant::VECTOR2);
}

TEST_CASE("[Variant] Assignment To Vec2i from Bool,Int,Float,String,Vec2,Vec3,Vec3i,Vec4,Vec4i,Rect2,Rect2i,Trans2d,Trans3d,Color,Call,Plane,Basis,AABB,Quant,Proj,RID,and Object") {
	Variant bool_v = false;
	Variant vec2i_v = Vector2i(2, 3);
	bool_v = vec2i_v; // Now bool_v is Vector2i
	CHECK(bool_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	bool_v = vec2i_v;
	CHECK(bool_v.get_type() == Variant::VECTOR2I);

	Variant int_v = 0;
	vec2i_v = Vector2i(2, 3);
	int_v = vec2i_v;
	CHECK(int_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	int_v = vec2i_v;
	CHECK(int_v.get_type() == Variant::VECTOR2I);

	Variant float_v = 0.0f;
	vec2i_v = Vector2i(2, 3);
	float_v = vec2i_v;
	CHECK(float_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	float_v = vec2i_v;
	CHECK(float_v.get_type() == Variant::VECTOR2I);

	Variant string_v = "";
	vec2i_v = Vector2i(2, 3);
	string_v = vec2i_v;
	CHECK(string_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	string_v = vec2i_v;
	CHECK(string_v.get_type() == Variant::VECTOR2I);

	Variant vec2_v = Vector2(0, 0);
	vec2i_v = Vector2i(2, 3);
	vec2_v = vec2i_v;
	CHECK(vec2_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	vec2_v = vec2i_v;
	CHECK(vec2i_v.get_type() == Variant::VECTOR2I);

	Variant vec3_v = Vector3(0, 0, 0);
	vec2i_v = Vector2i(2, 3);
	vec3_v = vec2i_v;
	CHECK(vec3_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	vec3_v = vec2i_v;
	CHECK(vec3_v.get_type() == Variant::VECTOR2I);

	Variant vec3i_v = Vector3i(0, 0, 0);
	vec2i_v = Vector2i(2, 3);
	vec3i_v = vec2i_v;
	CHECK(vec3i_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	vec3i_v = vec2i_v;
	CHECK(vec3i_v.get_type() == Variant::VECTOR2I);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	vec2i_v = Vector2i(2, 3);
	vec4_v = vec2i_v;
	CHECK(vec4_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	vec4_v = vec2i_v;
	CHECK(vec4_v.get_type() == Variant::VECTOR2I);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	vec2i_v = Vector2i(2, 3);
	vec4i_v = vec2i_v;
	CHECK(vec4i_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	vec4i_v = vec2i_v;
	CHECK(vec4i_v.get_type() == Variant::VECTOR2I);

	Variant rect2_v = Rect2();
	vec2i_v = Vector2i(2, 3);
	rect2_v = vec2i_v;
	CHECK(rect2_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	rect2_v = vec2i_v;
	CHECK(rect2_v.get_type() == Variant::VECTOR2I);

	Variant rect2i_v = Rect2i();
	vec2i_v = Vector2i(2, 3);
	rect2i_v = vec2i_v;
	CHECK(rect2i_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	rect2i_v = vec2i_v;
	CHECK(rect2i_v.get_type() == Variant::VECTOR2I);

	Variant transform2d_v = Transform2D();
	vec2i_v = Vector2i(2, 3);
	transform2d_v = vec2i_v;
	CHECK(transform2d_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	transform2d_v = vec2i_v;
	CHECK(transform2d_v.get_type() == Variant::VECTOR2I);

	Variant transform3d_v = Transform3D();
	vec2i_v = Vector2i(2, 3);
	transform3d_v = vec2i_v;
	CHECK(transform3d_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	transform3d_v = vec2i_v;
	CHECK(transform3d_v.get_type() == Variant::VECTOR2I);

	Variant col_v = Color(0.5f, 0.2f, 0.75f);
	vec2i_v = Vector2i(2, 3);
	col_v = vec2i_v;
	CHECK(col_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	col_v = vec2i_v;
	CHECK(col_v.get_type() == Variant::VECTOR2I);

	Variant call_v = Callable();
	vec2i_v = Vector2i(2, 3);
	call_v = vec2i_v;
	CHECK(call_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	call_v = vec2i_v;
	CHECK(call_v.get_type() == Variant::VECTOR2I);

	Variant plane_v = Plane();
	vec2i_v = Vector2i(2, 3);
	plane_v = vec2i_v;
	CHECK(plane_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	plane_v = vec2i_v;
	CHECK(plane_v.get_type() == Variant::VECTOR2I);

	Variant basis_v = Basis();
	vec2i_v = Vector2i(2, 3);
	basis_v = vec2i_v;
	CHECK(basis_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	basis_v = vec2i_v;
	CHECK(basis_v.get_type() == Variant::VECTOR2I);

	Variant aabb_v = AABB();
	vec2i_v = Vector2i(2, 3);
	aabb_v = vec2i_v;
	CHECK(aabb_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	aabb_v = vec2i_v;
	CHECK(aabb_v.get_type() == Variant::VECTOR2I);

	Variant quaternion_v = Quaternion();
	vec2i_v = Vector2i(2, 3);
	quaternion_v = vec2i_v;
	CHECK(quaternion_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	quaternion_v = vec2i_v;
	CHECK(quaternion_v.get_type() == Variant::VECTOR2I);

	Variant projection_v = Projection();
	vec2i_v = Vector2i(2, 3);
	projection_v = vec2i_v;
	CHECK(projection_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	projection_v = vec2i_v;
	CHECK(projection_v.get_type() == Variant::VECTOR2I);

	Variant rid_v = RID();
	vec2i_v = Vector2i(2, 3);
	rid_v = vec2i_v;
	CHECK(rid_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	rid_v = vec2i_v;
	CHECK(rid_v.get_type() == Variant::VECTOR2I);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	vec2i_v = Vector2i(2, 3);
	object_v = vec2i_v;
	CHECK(object_v == Variant(Vector2i(2, 3)));
	vec2i_v = Vector2i(-5, -7);
	object_v = vec2i_v;
	CHECK(object_v.get_type() == Variant::VECTOR2I);
}

TEST_CASE("[Variant] Assignment To Vec3 from Bool,Int,Float,String,Vec2,Vec2i,Vec3i,Vec4,Vec4i,Rect2,Rect2i,Trans2d,Trans3d,Color,Call,Plane,Basis,AABB,Quant,Proj,RID,and Object") {
	Variant bool_v = false;
	Variant vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	bool_v = vec3_v; // Now bool_v is Vector3
	CHECK(bool_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	bool_v = vec3_v;
	CHECK(bool_v.get_type() == Variant::VECTOR3);

	Variant int_v = 0;
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	int_v = vec3_v;
	CHECK(int_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	int_v = vec3_v;
	CHECK(int_v.get_type() == Variant::VECTOR3);

	Variant float_v = 0.0f;
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	float_v = vec3_v;
	CHECK(float_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	float_v = vec3_v;
	CHECK(float_v.get_type() == Variant::VECTOR3);

	Variant string_v = "";
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	string_v = vec3_v;
	CHECK(string_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	string_v = vec3_v;
	CHECK(string_v.get_type() == Variant::VECTOR3);

	Variant vec2_v = Vector2(0, 0);
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	vec2_v = vec3_v;
	CHECK(vec2_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	vec2_v = vec3_v;
	CHECK(vec2_v.get_type() == Variant::VECTOR3);

	Variant vec2i_v = Vector2i(0, 0);
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	vec2i_v = vec3_v;
	CHECK(vec2i_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	vec2i_v = vec3_v;
	CHECK(vec2i_v.get_type() == Variant::VECTOR3);

	Variant vec3i_v = Vector3i(0, 0, 0);
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	vec3i_v = vec3_v;
	CHECK(vec3i_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	vec3i_v = vec3_v;
	CHECK(vec3i_v.get_type() == Variant::VECTOR3);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	vec4_v = vec3_v;
	CHECK(vec4_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	vec4_v = vec3_v;
	CHECK(vec4_v.get_type() == Variant::VECTOR3);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	vec4i_v = vec3_v;
	CHECK(vec4i_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	vec4i_v = vec3_v;
	CHECK(vec4i_v.get_type() == Variant::VECTOR3);

	Variant rect2_v = Rect2();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	rect2_v = vec3_v;
	CHECK(rect2_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	rect2_v = vec3_v;
	CHECK(rect2_v.get_type() == Variant::VECTOR3);

	Variant rect2i_v = Rect2i();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	rect2i_v = vec3_v;
	CHECK(rect2i_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	rect2i_v = vec3_v;
	CHECK(rect2i_v.get_type() == Variant::VECTOR3);

	Variant transform2d_v = Transform2D();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	transform2d_v = vec3_v;
	CHECK(transform2d_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	transform2d_v = vec3_v;
	CHECK(transform2d_v.get_type() == Variant::VECTOR3);

	Variant transform3d_v = Transform3D();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	transform3d_v = vec3_v;
	CHECK(transform3d_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	transform3d_v = vec3_v;
	CHECK(transform3d_v.get_type() == Variant::VECTOR3);

	Variant col_v = Color(0.5f, 0.2f, 0.75f);
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	col_v = vec3_v;
	CHECK(col_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	col_v = vec3_v;
	CHECK(col_v.get_type() == Variant::VECTOR3);

	Variant call_v = Callable();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	call_v = vec3_v;
	CHECK(call_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	call_v = vec3_v;
	CHECK(call_v.get_type() == Variant::VECTOR3);

	Variant plane_v = Plane();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	plane_v = vec3_v;
	CHECK(plane_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	plane_v = vec3_v;
	CHECK(plane_v.get_type() == Variant::VECTOR3);

	Variant basis_v = Basis();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	basis_v = vec3_v;
	CHECK(basis_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	basis_v = vec3_v;
	CHECK(basis_v.get_type() == Variant::VECTOR3);

	Variant aabb_v = AABB();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	aabb_v = vec3_v;
	CHECK(aabb_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	aabb_v = vec3_v;
	CHECK(aabb_v.get_type() == Variant::VECTOR3);

	Variant quaternion_v = Quaternion();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	quaternion_v = vec3_v;
	CHECK(quaternion_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	quaternion_v = vec3_v;
	CHECK(quaternion_v.get_type() == Variant::VECTOR3);

	Variant projection_v = Projection();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	quaternion_v = vec3_v;
	CHECK(quaternion_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	quaternion_v = vec3_v;
	CHECK(quaternion_v.get_type() == Variant::VECTOR3);

	Variant rid_v = RID();
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	rid_v = vec3_v;
	CHECK(rid_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	rid_v = vec3_v;
	CHECK(rid_v.get_type() == Variant::VECTOR3);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	vec3_v = Vector3(2.2f, 3.5f, 5.3f);
	object_v = vec3_v;
	CHECK(object_v == Variant(Vector3(2.2f, 3.5f, 5.3f)));
	vec3_v = Vector3(-5.4f, -7.9f, -2.1f);
	object_v = vec3_v;
	CHECK(object_v.get_type() == Variant::VECTOR3);
}

TEST_CASE("[Variant] Assignment To Vec3i from Bool,Int,Float,String,Vec2,Vec2i,Vec3 and Color") {
	Variant bool_v = false;
	Variant vec3i_v = Vector3i(2, 3, 5);
	bool_v = vec3i_v; // Now bool_v is Vector3i
	CHECK(bool_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	bool_v = vec3i_v;
	CHECK(bool_v.get_type() == Variant::VECTOR3I);

	Variant int_v = 0;
	vec3i_v = Vector3i(2, 3, 5);
	int_v = vec3i_v;
	CHECK(int_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	int_v = vec3i_v;
	CHECK(int_v.get_type() == Variant::VECTOR3I);

	Variant float_v = 0.0f;
	vec3i_v = Vector3i(2, 3, 5);
	float_v = vec3i_v;
	CHECK(float_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	float_v = vec3i_v;
	CHECK(float_v.get_type() == Variant::VECTOR3I);

	Variant string_v = "";
	vec3i_v = Vector3i(2, 3, 5);
	string_v = vec3i_v;
	CHECK(string_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	string_v = vec3i_v;
	CHECK(string_v.get_type() == Variant::VECTOR3I);

	Variant vec2_v = Vector2(0, 0);
	vec3i_v = Vector3i(2, 3, 5);
	vec2_v = vec3i_v;
	CHECK(vec2_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	vec2_v = vec3i_v;
	CHECK(vec2_v.get_type() == Variant::VECTOR3I);

	Variant vec2i_v = Vector2i(0, 0);
	vec3i_v = Vector3i(2, 3, 5);
	vec2i_v = vec3i_v;
	CHECK(vec2i_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	vec2i_v = vec3i_v;
	CHECK(vec2i_v.get_type() == Variant::VECTOR3I);

	Variant vec3_v = Vector3(0, 0, 0);
	vec3i_v = Vector3i(2, 3, 5);
	vec3_v = vec3i_v;
	CHECK(vec3_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	vec3_v = vec3i_v;
	CHECK(vec3_v.get_type() == Variant::VECTOR3I);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	vec3i_v = Vector3i(2, 3, 5);
	vec4_v = vec3i_v;
	CHECK(vec4_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	vec4_v = vec3i_v;
	CHECK(vec4_v.get_type() == Variant::VECTOR3I);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	vec3i_v = Vector3i(2, 3, 5);
	vec4i_v = vec3i_v;
	CHECK(vec4i_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	vec4i_v = vec3i_v;
	CHECK(vec4i_v.get_type() == Variant::VECTOR3I);

	Variant rect2_v = Rect2();
	vec3i_v = Vector3i(2, 3, 5);
	rect2_v = vec3i_v;
	CHECK(rect2_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	rect2_v = vec3i_v;
	CHECK(rect2_v.get_type() == Variant::VECTOR3I);

	Variant rect2i_v = Rect2i();
	vec3i_v = Vector3i(2, 3, 5);
	rect2i_v = vec3i_v;
	CHECK(rect2i_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	rect2i_v = vec3i_v;
	CHECK(rect2i_v.get_type() == Variant::VECTOR3I);

	Variant transform2d_v = Transform2D();
	vec3i_v = Vector3i(2, 3, 5);
	transform2d_v = vec3i_v;
	CHECK(transform2d_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	transform2d_v = vec3i_v;
	CHECK(transform2d_v.get_type() == Variant::VECTOR3I);

	Variant transform3d_v = Transform3D();
	vec3i_v = Vector3i(2, 3, 5);
	transform3d_v = vec3i_v;
	CHECK(transform3d_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	transform3d_v = vec3i_v;
	CHECK(transform3d_v.get_type() == Variant::VECTOR3I);

	Variant col_v = Color(0.5f, 0.2f, 0.75f);
	vec3i_v = Vector3i(2, 3, 5);
	col_v = vec3i_v;
	CHECK(col_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	col_v = vec3i_v;
	CHECK(col_v.get_type() == Variant::VECTOR3I);

	Variant call_v = Callable();
	vec3i_v = Vector3i(2, 3, 5);
	call_v = vec3i_v;
	CHECK(call_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	call_v = vec3i_v;
	CHECK(call_v.get_type() == Variant::VECTOR3I);

	Variant plane_v = Plane();
	vec3i_v = Vector3i(2, 3, 5);
	plane_v = vec3i_v;
	CHECK(plane_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	plane_v = vec3i_v;
	CHECK(plane_v.get_type() == Variant::VECTOR3I);

	Variant basis_v = Basis();
	vec3i_v = Vector3i(2, 3, 5);
	basis_v = vec3i_v;
	CHECK(basis_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	basis_v = vec3i_v;
	CHECK(basis_v.get_type() == Variant::VECTOR3I);

	Variant aabb_v = AABB();
	vec3i_v = Vector3i(2, 3, 5);
	aabb_v = vec3i_v;
	CHECK(aabb_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	aabb_v = vec3i_v;
	CHECK(aabb_v.get_type() == Variant::VECTOR3I);

	Variant quaternion_v = Quaternion();
	vec3i_v = Vector3i(2, 3, 5);
	quaternion_v = vec3i_v;
	CHECK(quaternion_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	quaternion_v = vec3i_v;
	CHECK(quaternion_v.get_type() == Variant::VECTOR3I);

	Variant projection_v = Projection();
	vec3i_v = Vector3i(2, 3, 5);
	projection_v = vec3i_v;
	CHECK(projection_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	projection_v = vec3i_v;
	CHECK(projection_v.get_type() == Variant::VECTOR3I);

	Variant rid_v = RID();
	vec3i_v = Vector3i(2, 3, 5);
	rid_v = vec3i_v;
	CHECK(rid_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	rid_v = vec3i_v;
	CHECK(rid_v.get_type() == Variant::VECTOR3I);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	vec3i_v = Vector3i(2, 3, 5);
	object_v = vec3i_v;
	CHECK(object_v == Variant(Vector3i(2, 3, 5)));
	vec3i_v = Vector3i(-5, -7, -2);
	object_v = vec3i_v;
	CHECK(object_v.get_type() == Variant::VECTOR3I);
}

TEST_CASE("[Variant] Assignment To Color from Bool,Int,Float,String,Vec2,Vec2i,Vec3,Vec3i,Vec4,Vec4i,Rect2,Rect2i,Trans2d,Trans3d,Color,Call,Plane,Basis,AABB,Quant,Proj,RID,and Object") {
	Variant bool_v = false;
	Variant col_v = Color(0.25f, 0.4f, 0.78f);
	bool_v = col_v; // Now bool_v is Color
	CHECK(bool_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	bool_v = col_v;
	CHECK(bool_v.get_type() == Variant::COLOR);

	Variant int_v = 0;
	col_v = Color(0.25f, 0.4f, 0.78f);
	int_v = col_v;
	CHECK(int_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	int_v = col_v;
	CHECK(int_v.get_type() == Variant::COLOR);

	Variant float_v = 0.0f;
	col_v = Color(0.25f, 0.4f, 0.78f);
	float_v = col_v;
	CHECK(float_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	float_v = col_v;
	CHECK(float_v.get_type() == Variant::COLOR);

	Variant string_v = "";
	col_v = Color(0.25f, 0.4f, 0.78f);
	string_v = col_v;
	CHECK(string_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	string_v = col_v;
	CHECK(string_v.get_type() == Variant::COLOR);

	Variant vec2_v = Vector2(0, 0);
	col_v = Color(0.25f, 0.4f, 0.78f);
	vec2_v = col_v;
	CHECK(vec2_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	vec2_v = col_v;
	CHECK(vec2_v.get_type() == Variant::COLOR);

	Variant vec2i_v = Vector2i(0, 0);
	col_v = Color(0.25f, 0.4f, 0.78f);
	vec2i_v = col_v;
	CHECK(vec2i_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	vec2i_v = col_v;
	CHECK(vec2i_v.get_type() == Variant::COLOR);

	Variant vec3_v = Vector3(0, 0, 0);
	col_v = Color(0.25f, 0.4f, 0.78f);
	vec3_v = col_v;
	CHECK(vec3_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	vec3_v = col_v;
	CHECK(vec3_v.get_type() == Variant::COLOR);

	Variant vec3i_v = Vector3i(0, 0, 0);
	col_v = Color(0.25f, 0.4f, 0.78f);
	vec3i_v = col_v;
	CHECK(vec3i_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	vec3i_v = col_v;
	CHECK(vec3i_v.get_type() == Variant::COLOR);

	Variant vec4_v = Vector4(0, 0, 0, 0);
	col_v = Color(0.25f, 0.4f, 0.78f);
	vec4_v = col_v;
	CHECK(vec4_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	vec4_v = col_v;
	CHECK(vec4_v.get_type() == Variant::COLOR);

	Variant vec4i_v = Vector4i(0, 0, 0, 0);
	col_v = Color(0.25f, 0.4f, 0.78f);
	vec4i_v = col_v;
	CHECK(vec4i_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	vec4i_v = col_v;
	CHECK(vec4i_v.get_type() == Variant::COLOR);

	Variant rect2_v = Rect2();
	col_v = Color(0.25f, 0.4f, 0.78f);
	rect2_v = col_v;
	CHECK(rect2_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	rect2_v = col_v;
	CHECK(rect2_v.get_type() == Variant::COLOR);

	Variant rect2i_v = Rect2i();
	col_v = Color(0.25f, 0.4f, 0.78f);
	rect2i_v = col_v;
	CHECK(rect2i_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	rect2i_v = col_v;
	CHECK(rect2i_v.get_type() == Variant::COLOR);

	Variant transform2d_v = Transform2D();
	col_v = Color(0.25f, 0.4f, 0.78f);
	transform2d_v = col_v;
	CHECK(transform2d_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	transform2d_v = col_v;
	CHECK(transform2d_v.get_type() == Variant::COLOR);

	Variant transform3d_v = Transform3D();
	col_v = Color(0.25f, 0.4f, 0.78f);
	transform3d_v = col_v;
	CHECK(transform3d_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	transform3d_v = col_v;
	CHECK(transform3d_v.get_type() == Variant::COLOR);

	Variant call_v = Callable();
	col_v = Color(0.25f, 0.4f, 0.78f);
	call_v = col_v;
	CHECK(call_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	call_v = col_v;
	CHECK(call_v.get_type() == Variant::COLOR);

	Variant plane_v = Plane();
	col_v = Color(0.25f, 0.4f, 0.78f);
	plane_v = col_v;
	CHECK(plane_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	plane_v = col_v;
	CHECK(plane_v.get_type() == Variant::COLOR);

	Variant basis_v = Basis();
	col_v = Color(0.25f, 0.4f, 0.78f);
	basis_v = col_v;
	CHECK(basis_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	basis_v = col_v;
	CHECK(basis_v.get_type() == Variant::COLOR);

	Variant aabb_v = AABB();
	col_v = Color(0.25f, 0.4f, 0.78f);
	aabb_v = col_v;
	CHECK(aabb_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	aabb_v = col_v;
	CHECK(aabb_v.get_type() == Variant::COLOR);

	Variant quaternion_v = Quaternion();
	col_v = Color(0.25f, 0.4f, 0.78f);
	quaternion_v = col_v;
	CHECK(quaternion_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	quaternion_v = col_v;
	CHECK(quaternion_v.get_type() == Variant::COLOR);

	Variant projection_v = Projection();
	col_v = Color(0.25f, 0.4f, 0.78f);
	projection_v = col_v;
	CHECK(projection_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	projection_v = col_v;
	CHECK(projection_v.get_type() == Variant::COLOR);

	Variant rid_v = RID();
	col_v = Color(0.25f, 0.4f, 0.78f);
	rid_v = col_v;
	CHECK(rid_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	rid_v = col_v;
	CHECK(rid_v.get_type() == Variant::COLOR);

	Object obj_one = Object();
	Variant object_v = &obj_one;
	col_v = Color(0.25f, 0.4f, 0.78f);
	object_v = col_v;
	CHECK(object_v == Variant(Color(0.25f, 0.4f, 0.78f)));
	col_v = Color(0.33f, 0.75f, 0.21f);
	object_v = col_v;
	CHECK(object_v.get_type() == Variant::COLOR);
}

TEST_CASE("[Variant] Writer and parser array") {
	Array a = build_array(1, String("hello"), build_array(Variant()));
	String a_str;
	VariantWriter::write_to_string(a, a_str);

	CHECK_EQ(a_str, "[1, \"hello\", [null]]");

	VariantParser::StreamString ss;
	String errs;
	int line;
	Variant a_parsed;

	ss.s = a_str;
	VariantParser::parse(&ss, a_parsed, errs, line);

	CHECK_MESSAGE(a_parsed == Variant(a), "Should parse back.");
}

TEST_CASE("[Variant] Writer recursive array") {
	// There is no way to accurately represent a recursive array,
	// the only thing we can do is make sure the writer doesn't blow up

	// Self recursive
	Array a;
	a.push_back(a);

	// Writer should it recursion limit while visiting the array
	ERR_PRINT_OFF;
	String a_str;
	VariantWriter::write_to_string(a, a_str);
	ERR_PRINT_ON;

	// Nested recursive
	Array a1;
	Array a2;
	a1.push_back(a2);
	a2.push_back(a1);

	// Writer should it recursion limit while visiting the array
	ERR_PRINT_OFF;
	String a1_str;
	VariantWriter::write_to_string(a1, a1_str);
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary tearndown will leak memory
	a.clear();
	a1.clear();
	a2.clear();
}

TEST_CASE("[Variant] Writer and parser dictionary") {
	// d = {{1: 2}: 3, 4: "hello", 5: {null: []}}
	Dictionary d = build_dictionary(build_dictionary(1, 2), 3, 4, String("hello"), 5, build_dictionary(Variant(), build_array()));
	String d_str;
	VariantWriter::write_to_string(d, d_str);

	CHECK_EQ(d_str, "{\n4: \"hello\",\n5: {\nnull: []\n},\n{\n1: 2\n}: 3\n}");

	VariantParser::StreamString ss;
	String errs;
	int line;
	Variant d_parsed;

	ss.s = d_str;
	VariantParser::parse(&ss, d_parsed, errs, line);

	CHECK_MESSAGE(d_parsed == Variant(d), "Should parse back.");
}

TEST_CASE("[Variant] Writer key sorting") {
	Dictionary d = build_dictionary(StringName("C"), 3, "A", 1, StringName("B"), 2, "D", 4);
	String d_str;
	VariantWriter::write_to_string(d, d_str);

	CHECK_EQ(d_str, "{\n\"A\": 1,\n&\"B\": 2,\n&\"C\": 3,\n\"D\": 4\n}");
}

TEST_CASE("[Variant] Writer recursive dictionary") {
	// There is no way to accurately represent a recursive dictionary,
	// the only thing we can do is make sure the writer doesn't blow up

	// Self recursive
	Dictionary d;
	d[1] = d;

	// Writer should it recursion limit while visiting the dictionary
	ERR_PRINT_OFF;
	String d_str;
	VariantWriter::write_to_string(d, d_str);
	ERR_PRINT_ON;

	// Nested recursive
	Dictionary d1;
	Dictionary d2;
	d1[2] = d2;
	d2[1] = d1;

	// Writer should it recursion limit while visiting the dictionary
	ERR_PRINT_OFF;
	String d1_str;
	VariantWriter::write_to_string(d1, d1_str);
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary tearndown will leak memory
	d.clear();
	d1.clear();
	d2.clear();
}

#if 0 // TODO: recursion in dict key is currently buggy
TEST_CASE("[Variant] Writer recursive dictionary on keys") {
	// There is no way to accurately represent a recursive dictionary,
	// the only thing we can do is make sure the writer doesn't blow up

	// Self recursive
	Dictionary d;
	d[d] = 1;

	// Writer should it recursion limit while visiting the dictionary
	ERR_PRINT_OFF;
	String d_str;
	VariantWriter::write_to_string(d, d_str);
	ERR_PRINT_ON;

	// Nested recursive
	Dictionary d1;
	Dictionary d2;
	d1[d2] = 2;
	d2[d1] = 1;

	// Writer should it recursion limit while visiting the dictionary
	ERR_PRINT_OFF;
	String d1_str;
	VariantWriter::write_to_string(d1, d1_str);
	ERR_PRINT_ON;

	// Break the recursivity otherwise Dictionary tearndown will leak memory
	d.clear();
	d1.clear();
	d2.clear();
}
#endif

TEST_CASE("[Variant] Basic comparison") {
	CHECK_EQ(Variant(1), Variant(1));
	CHECK_FALSE(Variant(1) != Variant(1));
	CHECK_NE(Variant(1), Variant(2));
	CHECK_EQ(Variant(String("foo")), Variant(String("foo")));
	CHECK_NE(Variant(String("foo")), Variant(String("bar")));
	// Check "empty" version of different types are not equivalents
	CHECK_NE(Variant(0), Variant());
	CHECK_NE(Variant(String()), Variant());
	CHECK_NE(Variant(Array()), Variant());
	CHECK_NE(Variant(Dictionary()), Variant());
}

TEST_CASE("[Variant] Identity comparison") {
	// Value types are compared by value
	Variant aabb = AABB();
	CHECK(aabb.identity_compare(aabb));
	CHECK(aabb.identity_compare(AABB()));
	CHECK_FALSE(aabb.identity_compare(AABB(Vector3(1, 2, 3), Vector3(1, 2, 3))));

	Variant basis = Basis();
	CHECK(basis.identity_compare(basis));
	CHECK(basis.identity_compare(Basis()));
	CHECK_FALSE(basis.identity_compare(Basis(Quaternion(Vector3(1, 2, 3).normalized(), 45))));

	Variant bool_var = true;
	CHECK(bool_var.identity_compare(bool_var));
	CHECK(bool_var.identity_compare(true));
	CHECK_FALSE(bool_var.identity_compare(false));

	Variant callable = Callable();
	CHECK(callable.identity_compare(callable));
	CHECK(callable.identity_compare(Callable()));
	CHECK_FALSE(callable.identity_compare(Callable(ObjectID(), StringName("lambda"))));

	Variant color = Color();
	CHECK(color.identity_compare(color));
	CHECK(color.identity_compare(Color()));
	CHECK_FALSE(color.identity_compare(Color(255, 0, 255)));

	Variant float_var = 1.0;
	CHECK(float_var.identity_compare(float_var));
	CHECK(float_var.identity_compare(1.0));
	CHECK_FALSE(float_var.identity_compare(2.0));

	Variant int_var = 1;
	CHECK(int_var.identity_compare(int_var));
	CHECK(int_var.identity_compare(1));
	CHECK_FALSE(int_var.identity_compare(2));

	Variant nil = Variant();
	CHECK(nil.identity_compare(nil));
	CHECK(nil.identity_compare(Variant()));
	CHECK_FALSE(nil.identity_compare(true));

	Variant node_path = NodePath("godot");
	CHECK(node_path.identity_compare(node_path));
	CHECK(node_path.identity_compare(NodePath("godot")));
	CHECK_FALSE(node_path.identity_compare(NodePath("waiting")));

	Variant plane = Plane();
	CHECK(plane.identity_compare(plane));
	CHECK(plane.identity_compare(Plane()));
	CHECK_FALSE(plane.identity_compare(Plane(Vector3(1, 2, 3), 42)));

	Variant projection = Projection();
	CHECK(projection.identity_compare(projection));
	CHECK(projection.identity_compare(Projection()));
	CHECK_FALSE(projection.identity_compare(Projection(Transform3D(Basis(Vector3(1, 2, 3).normalized(), 45), Vector3(1, 2, 3)))));

	Variant quaternion = Quaternion();
	CHECK(quaternion.identity_compare(quaternion));
	CHECK(quaternion.identity_compare(Quaternion()));
	CHECK_FALSE(quaternion.identity_compare(Quaternion(Vector3(1, 2, 3).normalized(), 45)));

	Variant rect2 = Rect2();
	CHECK(rect2.identity_compare(rect2));
	CHECK(rect2.identity_compare(Rect2()));
	CHECK_FALSE(rect2.identity_compare(Rect2(Point2(Vector2(1, 2)), Size2(Vector2(1, 2)))));

	Variant rect2i = Rect2i();
	CHECK(rect2i.identity_compare(rect2i));
	CHECK(rect2i.identity_compare(Rect2i()));
	CHECK_FALSE(rect2i.identity_compare(Rect2i(Point2i(Vector2i(1, 2)), Size2i(Vector2i(1, 2)))));

	Variant rid = RID();
	CHECK(rid.identity_compare(rid));
	CHECK(rid.identity_compare(RID()));
	CHECK_FALSE(rid.identity_compare(RID::from_uint64(123)));

	Variant signal = Signal();
	CHECK(signal.identity_compare(signal));
	CHECK(signal.identity_compare(Signal()));
	CHECK_FALSE(signal.identity_compare(Signal(ObjectID(), StringName("lambda"))));

	Variant str = "godot";
	CHECK(str.identity_compare(str));
	CHECK(str.identity_compare("godot"));
	CHECK_FALSE(str.identity_compare("waiting"));

	Variant str_name = StringName("godot");
	CHECK(str_name.identity_compare(str_name));
	CHECK(str_name.identity_compare(StringName("godot")));
	CHECK_FALSE(str_name.identity_compare(StringName("waiting")));

	Variant transform2d = Transform2D();
	CHECK(transform2d.identity_compare(transform2d));
	CHECK(transform2d.identity_compare(Transform2D()));
	CHECK_FALSE(transform2d.identity_compare(Transform2D(45, Vector2(1, 2))));

	Variant transform3d = Transform3D();
	CHECK(transform3d.identity_compare(transform3d));
	CHECK(transform3d.identity_compare(Transform3D()));
	CHECK_FALSE(transform3d.identity_compare(Transform3D(Basis(Quaternion(Vector3(1, 2, 3).normalized(), 45)), Vector3(1, 2, 3))));

	Variant vect2 = Vector2();
	CHECK(vect2.identity_compare(vect2));
	CHECK(vect2.identity_compare(Vector2()));
	CHECK_FALSE(vect2.identity_compare(Vector2(1, 2)));

	Variant vect2i = Vector2i();
	CHECK(vect2i.identity_compare(vect2i));
	CHECK(vect2i.identity_compare(Vector2i()));
	CHECK_FALSE(vect2i.identity_compare(Vector2i(1, 2)));

	Variant vect3 = Vector3();
	CHECK(vect3.identity_compare(vect3));
	CHECK(vect3.identity_compare(Vector3()));
	CHECK_FALSE(vect3.identity_compare(Vector3(1, 2, 3)));

	Variant vect3i = Vector3i();
	CHECK(vect3i.identity_compare(vect3i));
	CHECK(vect3i.identity_compare(Vector3i()));
	CHECK_FALSE(vect3i.identity_compare(Vector3i(1, 2, 3)));

	Variant vect4 = Vector4();
	CHECK(vect4.identity_compare(vect4));
	CHECK(vect4.identity_compare(Vector4()));
	CHECK_FALSE(vect4.identity_compare(Vector4(1, 2, 3, 4)));

	Variant vect4i = Vector4i();
	CHECK(vect4i.identity_compare(vect4i));
	CHECK(vect4i.identity_compare(Vector4i()));
	CHECK_FALSE(vect4i.identity_compare(Vector4i(1, 2, 3, 4)));

	// Reference types are compared by reference
	Variant array = Array();
	CHECK(array.identity_compare(array));
	CHECK_FALSE(array.identity_compare(Array()));

	Variant dictionary = Dictionary();
	CHECK(dictionary.identity_compare(dictionary));
	CHECK_FALSE(dictionary.identity_compare(Dictionary()));

	Variant packed_byte_array = PackedByteArray();
	CHECK(packed_byte_array.identity_compare(packed_byte_array));
	CHECK_FALSE(packed_byte_array.identity_compare(PackedByteArray()));

	Variant packed_color_array = PackedColorArray();
	CHECK(packed_color_array.identity_compare(packed_color_array));
	CHECK_FALSE(packed_color_array.identity_compare(PackedColorArray()));

	Variant packed_vector4_array = PackedVector4Array();
	CHECK(packed_vector4_array.identity_compare(packed_vector4_array));
	CHECK_FALSE(packed_vector4_array.identity_compare(PackedVector4Array()));

	Variant packed_float32_array = PackedFloat32Array();
	CHECK(packed_float32_array.identity_compare(packed_float32_array));
	CHECK_FALSE(packed_float32_array.identity_compare(PackedFloat32Array()));

	Variant packed_float64_array = PackedFloat64Array();
	CHECK(packed_float64_array.identity_compare(packed_float64_array));
	CHECK_FALSE(packed_float64_array.identity_compare(PackedFloat64Array()));

	Variant packed_int32_array = PackedInt32Array();
	CHECK(packed_int32_array.identity_compare(packed_int32_array));
	CHECK_FALSE(packed_int32_array.identity_compare(PackedInt32Array()));

	Variant packed_int64_array = PackedInt64Array();
	CHECK(packed_int64_array.identity_compare(packed_int64_array));
	CHECK_FALSE(packed_int64_array.identity_compare(PackedInt64Array()));

	Variant packed_string_array = PackedStringArray();
	CHECK(packed_string_array.identity_compare(packed_string_array));
	CHECK_FALSE(packed_string_array.identity_compare(PackedStringArray()));

	Variant packed_vector2_array = PackedVector2Array();
	CHECK(packed_vector2_array.identity_compare(packed_vector2_array));
	CHECK_FALSE(packed_vector2_array.identity_compare(PackedVector2Array()));

	Variant packed_vector3_array = PackedVector3Array();
	CHECK(packed_vector3_array.identity_compare(packed_vector3_array));
	CHECK_FALSE(packed_vector3_array.identity_compare(PackedVector3Array()));

	Object obj_one = Object();
	Variant obj_one_var = &obj_one;
	Object obj_two = Object();
	Variant obj_two_var = &obj_two;
	CHECK(obj_one_var.identity_compare(obj_one_var));
	CHECK_FALSE(obj_one_var.identity_compare(obj_two_var));

	Variant obj_null_one_var = Variant((Object *)nullptr);
	Variant obj_null_two_var = Variant((Object *)nullptr);
	CHECK(obj_null_one_var.identity_compare(obj_null_one_var));
	CHECK(obj_null_one_var.identity_compare(obj_null_two_var));

	Object *freed_one = new Object();
	Variant freed_one_var = freed_one;
	delete freed_one;
	Object *freed_two = new Object();
	Variant freed_two_var = freed_two;
	delete freed_two;
	CHECK_FALSE(freed_one_var.identity_compare(freed_two_var));
}

TEST_CASE("[Variant] Nested array comparison") {
	Array a1 = build_array(1, build_array(2, 3));
	Array a2 = build_array(1, build_array(2, 3));
	Array a_other = build_array(1, build_array(2, 4));
	Variant v_a1 = a1;
	Variant v_a1_ref2 = a1;
	Variant v_a2 = a2;
	Variant v_a_other = a_other;

	// test both operator== and operator!=
	CHECK_EQ(v_a1, v_a1);
	CHECK_FALSE(v_a1 != v_a1);
	CHECK_EQ(v_a1, v_a1_ref2);
	CHECK_FALSE(v_a1 != v_a1_ref2);
	CHECK_EQ(v_a1, v_a2);
	CHECK_FALSE(v_a1 != v_a2);
	CHECK_NE(v_a1, v_a_other);
	CHECK_FALSE(v_a1 == v_a_other);
}

TEST_CASE("[Variant] Nested dictionary comparison") {
	Dictionary d1 = build_dictionary(build_dictionary(1, 2), build_dictionary(3, 4));
	Dictionary d2 = build_dictionary(build_dictionary(1, 2), build_dictionary(3, 4));
	Dictionary d_other_key = build_dictionary(build_dictionary(1, 0), build_dictionary(3, 4));
	Dictionary d_other_val = build_dictionary(build_dictionary(1, 2), build_dictionary(3, 0));
	Variant v_d1 = d1;
	Variant v_d1_ref2 = d1;
	Variant v_d2 = d2;
	Variant v_d_other_key = d_other_key;
	Variant v_d_other_val = d_other_val;

	// test both operator== and operator!=
	CHECK_EQ(v_d1, v_d1);
	CHECK_FALSE(v_d1 != v_d1);
	CHECK_EQ(v_d1, v_d1_ref2);
	CHECK_FALSE(v_d1 != v_d1_ref2);
	CHECK_EQ(v_d1, v_d2);
	CHECK_FALSE(v_d1 != v_d2);
	CHECK_NE(v_d1, v_d_other_key);
	CHECK_FALSE(v_d1 == v_d_other_key);
	CHECK_NE(v_d1, v_d_other_val);
	CHECK_FALSE(v_d1 == v_d_other_val);
}

struct ArgumentData {
	Variant::Type type;
	String name;
	bool has_defval = false;
	Variant defval;
	int position;
};

struct MethodData {
	StringName name;
	Variant::Type return_type;
	List<ArgumentData> arguments;
	bool is_virtual = false;
	bool is_vararg = false;
};

TEST_CASE("[Variant] Utility functions") {
	List<MethodData> functions;

	List<StringName> function_names;
	Variant::get_utility_function_list(&function_names);
	function_names.sort_custom<StringName::AlphCompare>();

	for (const StringName &E : function_names) {
		MethodData md;
		md.name = E;

		// Utility function's return type.
		if (Variant::has_utility_function_return_value(E)) {
			md.return_type = Variant::get_utility_function_return_type(E);
		}

		// Utility function's arguments.
		if (Variant::is_utility_function_vararg(E)) {
			md.is_vararg = true;
		} else {
			for (int i = 0; i < Variant::get_utility_function_argument_count(E); i++) {
				ArgumentData arg;
				arg.type = Variant::get_utility_function_argument_type(E, i);
				arg.name = Variant::get_utility_function_argument_name(E, i);
				arg.position = i;

				md.arguments.push_back(arg);
			}
		}

		functions.push_back(md);
	}

	SUBCASE("[Variant] Validate utility functions") {
		for (const MethodData &E : functions) {
			for (const ArgumentData &F : E.arguments) {
				const ArgumentData &arg = F;

				TEST_COND((arg.name.is_empty() || arg.name.begins_with("_unnamed_arg")),
						vformat("Unnamed argument in position %d of function '%s'.", arg.position, E.name));
			}
		}
	}
}

TEST_CASE("[Variant] Operator NOT") {
	// Verify that operator NOT works for all types and is consistent with booleanize().
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		Variant value;
		Callable::CallError err;
		Variant::construct((Variant::Type)i, value, nullptr, 0, err);

		REQUIRE_EQ(err.error, Callable::CallError::CALL_OK);

		Variant result = Variant::evaluate(Variant::OP_NOT, value, Variant());

		REQUIRE_EQ(result.get_type(), Variant::BOOL);
		CHECK_EQ(!value.booleanize(), result.operator bool());
	}
}

} // namespace TestVariant
