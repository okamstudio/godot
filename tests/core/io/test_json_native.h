/**************************************************************************/
/*  test_json_native.h                                                    */
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

#ifndef TEST_JSON_NATIVE_H
#define TEST_JSON_NATIVE_H

#include "core/io/json.h"

#include "core/variant/typed_array.h"
#include "core/variant/typed_dictionary.h"
#include "tests/test_macros.h"

namespace TestJSONNative {

String encode(const Variant &p_variant, bool p_full_objects = false) {
	return JSON::stringify(JSON::from_native(p_variant, p_full_objects), "", false);
}

Variant decode(const String &p_string, bool p_allow_objects = false) {
	return JSON::to_native(JSON::parse_string(p_string), p_allow_objects);
}

void test(const Variant &p_variant, const String &p_string, bool p_with_objects = false) {
	CHECK(encode(p_variant, p_with_objects) == p_string);
	CHECK(decode(p_string, p_with_objects).get_construct_string() == p_variant.get_construct_string());
}

TEST_CASE("[JSON][Native] Conversion between native and JSON formats") {
	// `Nil` and `bool` (represented as JSON keyword literals).
	test(Variant(), "null");
	test(false, "false");
	test(true, "true");

	// Numbers and strings (represented as JSON strings).
	test(1, R"("i:1")");
	test(1.0, R"("f:1.0")");
	test(String("abc"), R"("s:abc")");
	test(StringName("abc"), R"("sn:abc")");
	test(NodePath("abc"), R"("np:abc")");

	// Non-serializable types (always empty after deserialization).
	test(RID(), R"({"type":"RID"})");
	test(Callable(), R"({"type":"Callable"})");
	test(Signal(), R"({"type":"Signal"})");

	// Math types.

	test(Vector2(1, 2), R"({"type":"Vector2","args":[1.0,2.0]})");
	test(Vector2i(1, 2), R"({"type":"Vector2i","args":[1,2]})");
	test(Rect2(1, 2, 3, 4), R"({"type":"Rect2","args":[1.0,2.0,3.0,4.0]})");
	test(Rect2i(1, 2, 3, 4), R"({"type":"Rect2i","args":[1,2,3,4]})");
	test(Vector3(1, 2, 3), R"({"type":"Vector3","args":[1.0,2.0,3.0]})");
	test(Vector3i(1, 2, 3), R"({"type":"Vector3i","args":[1,2,3]})");
	test(Transform2D(1, 2, 3, 4, 5, 6), R"({"type":"Transform2D","args":[1.0,2.0,3.0,4.0,5.0,6.0]})");
	test(Vector4(1, 2, 3, 4), R"({"type":"Vector4","args":[1.0,2.0,3.0,4.0]})");
	test(Vector4i(1, 2, 3, 4), R"({"type":"Vector4i","args":[1,2,3,4]})");
	test(Plane(1, 2, 3, 4), R"({"type":"Plane","args":[1.0,2.0,3.0,4.0]})");
	test(Quaternion(1, 2, 3, 4), R"({"type":"Quaternion","args":[1.0,2.0,3.0,4.0]})");
	test(AABB(Vector3(1, 2, 3), Vector3(4, 5, 6)), R"({"type":"AABB","args":[1.0,2.0,3.0,4.0,5.0,6.0]})");

	const Basis b(Vector3(1, 2, 3), Vector3(4, 5, 6), Vector3(7, 8, 9));
	test(b, R"({"type":"Basis","args":[1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0]})");

	const Transform3D tr3d(Vector3(1, 2, 3), Vector3(4, 5, 6), Vector3(7, 8, 9), Vector3(10, 11, 12));
	test(tr3d, R"({"type":"Transform3D","args":[1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0]})");

	const Projection p(Vector4(1, 2, 3, 4), Vector4(5, 6, 7, 8), Vector4(9, 10, 11, 12), Vector4(13, 14, 15, 16));
	test(p, R"({"type":"Projection","args":[1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0,13.0,14.0,15.0,16.0]})");

	test(Color(1, 2, 3, 4), R"({"type":"Color","args":[1.0,2.0,3.0,4.0]})");

	// `Object`.

	Ref<Resource> res;
	res.instantiate();

	// The properties are stored in an array because the order in which they are assigned may be important during initialization.
	const String res_repr = R"({"type":"Resource","props":["resource_local_to_scene",false,"resource_name","s:","script",null]})";

	test(res, res_repr, true);
	ERR_PRINT_OFF;
	CHECK(encode(res) == "null");
	CHECK(decode(res_repr).get_type() == Variant::NIL);
	ERR_PRINT_ON;

	// `Dictionary`.

	Dictionary dict;
	dict[false] = true;
	dict[0] = 1;
	dict[0.0] = 1.0;

	// Godot dictionaries preserve insertion order, so an array is used for keys/values.
	test(dict, R"({"type":"Dictionary","args":[false,true,"i:0","i:1","f:0.0","f:1.0"]})");

	TypedDictionary<int64_t, int64_t> int_int_dict;
	int_int_dict[1] = 2;
	int_int_dict[3] = 4;

	test(int_int_dict, R"({"type":"Dictionary","key_type":"int","value_type":"int","args":["i:1","i:2","i:3","i:4"]})");

	TypedDictionary<int64_t, Variant> int_var_dict;
	int_var_dict[1] = "2";
	int_var_dict[3] = "4";

	test(int_var_dict, R"({"type":"Dictionary","key_type":"int","args":["i:1","s:2","i:3","s:4"]})");

	TypedDictionary<Variant, int64_t> var_int_dict;
	var_int_dict["1"] = 2;
	var_int_dict["3"] = 4;

	test(var_int_dict, R"({"type":"Dictionary","value_type":"int","args":["s:1","i:2","s:3","i:4"]})");

	Dictionary dict2;
	dict2["x"] = res;

	const String dict2_repr = vformat(R"({"type":"Dictionary","args":["s:x",%s]})", res_repr);

	test(dict2, dict2_repr, true);
	ERR_PRINT_OFF;
	CHECK(encode(dict2) == R"({"type":"Dictionary","args":["s:x",null]})");
	CHECK(decode(dict2_repr).get_construct_string() == "{\n\"x\": null\n}");
	ERR_PRINT_ON;

	TypedDictionary<String, Resource> res_dict;
	res_dict["x"] = res;

	const String res_dict_repr = vformat(R"({"type":"Dictionary","key_type":"String","value_type":"Resource","args":["s:x",%s]})", res_repr);

	test(res_dict, res_dict_repr, true);
	ERR_PRINT_OFF;
	CHECK(encode(res_dict) == "null");
	CHECK(decode(res_dict_repr).get_type() == Variant::NIL);
	ERR_PRINT_ON;

	// `Array`.

	Array arr;
	arr.push_back(true);
	arr.push_back(1);
	arr.push_back("abc");

	test(arr, R"([true,"i:1","s:abc"])");

	TypedArray<int64_t> int_arr;
	int_arr.push_back(1);
	int_arr.push_back(2);
	int_arr.push_back(3);

	test(int_arr, R"({"type":"Array","elem_type":"int","args":["i:1","i:2","i:3"]})");

	Array arr2;
	arr2.push_back(1);
	arr2.push_back(res);
	arr2.push_back(9);

	const String arr2_repr = vformat(R"(["i:1",%s,"i:9"])", res_repr);

	test(arr2, arr2_repr, true);
	ERR_PRINT_OFF;
	CHECK(encode(arr2) == R"(["i:1",null,"i:9"])");
	CHECK(decode(arr2_repr).get_construct_string() == "[1, null, 9]");
	ERR_PRINT_ON;

	TypedArray<Resource> res_arr;
	res_arr.push_back(res);

	const String res_arr_repr = vformat(R"({"type":"Array","elem_type":"Resource","args":[%s]})", res_repr);

	test(res_arr, res_arr_repr, true);
	ERR_PRINT_OFF;
	CHECK(encode(res_arr) == "null");
	CHECK(decode(res_arr_repr).get_type() == Variant::NIL);
	ERR_PRINT_ON;

	// Packed arrays.

	test(PackedByteArray({ 1, 2, 3 }), R"({"type":"PackedByteArray","args":[1,2,3]})");
	test(PackedInt32Array({ 1, 2, 3 }), R"({"type":"PackedInt32Array","args":[1,2,3]})");
	test(PackedInt64Array({ 1, 2, 3 }), R"({"type":"PackedInt64Array","args":[1,2,3]})");
	test(PackedFloat32Array({ 1, 2, 3 }), R"({"type":"PackedFloat32Array","args":[1.0,2.0,3.0]})");
	test(PackedFloat64Array({ 1, 2, 3 }), R"({"type":"PackedFloat64Array","args":[1.0,2.0,3.0]})");
	test(PackedStringArray({ "a", "b", "c" }), R"({"type":"PackedStringArray","args":["a","b","c"]})");

	const PackedVector2Array pv2arr({ Vector2(1, 2), Vector2(3, 4), Vector2(5, 6) });
	test(pv2arr, R"({"type":"PackedVector2Array","args":[1.0,2.0,3.0,4.0,5.0,6.0]})");

	const PackedVector3Array pv3arr({ Vector3(1, 2, 3), Vector3(4, 5, 6), Vector3(7, 8, 9) });
	test(pv3arr, R"({"type":"PackedVector3Array","args":[1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0]})");

	const PackedColorArray pcolarr({ Color(1, 2, 3, 4), Color(5, 6, 7, 8), Color(9, 10, 11, 12) });
	test(pcolarr, R"({"type":"PackedColorArray","args":[1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0]})");

	const PackedVector4Array pv4arr({ Vector4(1, 2, 3, 4), Vector4(5, 6, 7, 8), Vector4(9, 10, 11, 12) });
	test(pv4arr, R"({"type":"PackedVector4Array","args":[1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0]})");
}

} // namespace TestJSONNative

#endif // TEST_JSON_NATIVE_H
