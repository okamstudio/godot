/**************************************************************************/
/*  extension_api_dump.cpp                                                */
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

#include "extension_api_dump.h"

#include "core/config/engine.h"
#include "core/core_constants.h"
#include "core/extension/gdextension_compat_hashes.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/templates/pair.h"
#include "core/version.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_help.h"

static String get_builtin_or_variant_type_name(const Variant::Type p_type) {
	if (p_type == Variant::NIL) {
		return "Variant";
	} else {
		return Variant::get_type_name(p_type);
	}
}

static String get_property_info_type_name(const PropertyInfo &p_info) {
	if (p_info.type == Variant::INT && (p_info.hint == PROPERTY_HINT_INT_IS_POINTER)) {
		if (p_info.hint_string.is_empty()) {
			return "void*";
		} else {
			return p_info.hint_string + "*";
		}
	}
	if (p_info.type == Variant::ARRAY && (p_info.hint == PROPERTY_HINT_ARRAY_TYPE)) {
		return String("typedarray::") + p_info.hint_string;
	}
	if (p_info.type == Variant::DICTIONARY && (p_info.hint == PROPERTY_HINT_DICTIONARY_TYPE)) {
		return String("typeddictionary::") + p_info.hint_string;
	}
	if (p_info.type == Variant::INT && (p_info.usage & (PROPERTY_USAGE_CLASS_IS_ENUM))) {
		return String("enum::") + String(p_info.class_name);
	}
	if (p_info.type == Variant::INT && (p_info.usage & (PROPERTY_USAGE_CLASS_IS_BITFIELD))) {
		return String("bitfield::") + String(p_info.class_name);
	}
	if (p_info.type == Variant::INT && (p_info.usage & PROPERTY_USAGE_ARRAY)) {
		return "int";
	}
	if (p_info.class_name != StringName()) {
		return p_info.class_name;
	}
	if (p_info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
		return p_info.hint_string;
	}
	if (p_info.type == Variant::NIL && (p_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT)) {
		return "Variant";
	}
	if (p_info.type == Variant::NIL) {
		return "void";
	}
	return get_builtin_or_variant_type_name(p_info.type);
}

static String get_type_meta_name(const GodotTypeInfo::Metadata metadata) {
	static const char *argmeta[13] = { "none", "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64", "float", "double", "char16", "char32" };
	return argmeta[metadata];
}

static String fix_doc_description(const String &p_bbcode) {
	// Based on what EditorHelp does.

	return p_bbcode.dedent()
			.replace("\t", "")
			.replace("\r", "")
			.strip_edges();
}

Dictionary GDExtensionAPIDump::generate_extension_api(bool p_include_docs) {
	Dictionary api_dump;

	{
		//header
		Dictionary header;
		header["version_major"] = VERSION_MAJOR;
		header["version_minor"] = VERSION_MINOR;
#if VERSION_PATCH
		header["version_patch"] = VERSION_PATCH;
#else
		header["version_patch"] = 0;
#endif
		header["version_status"] = VERSION_STATUS;
		header["version_build"] = VERSION_BUILD;
		header["version_full_name"] = VERSION_FULL_NAME;

		api_dump["header"] = header;
	}

	const uint32_t vec3_elems = 3;
	const uint32_t vec4_elems = 4;
	const uint32_t ptrsize_32 = 4;
	const uint32_t ptrsize_64 = 8;
	static const char *build_config_name[4] = { "float_32", "float_64", "double_32", "double_64" };

	{
		//type sizes
		constexpr struct {
			Variant::Type type;
			uint32_t size_32_bits_real_float;
			uint32_t size_64_bits_real_float;
			uint32_t size_32_bits_real_double;
			uint32_t size_64_bits_real_double;

			// For compile-time size check.
			constexpr uint32_t operator[](int index) const {
				switch (index) {
#ifndef REAL_T_IS_DOUBLE
					case sizeof(uint32_t):
						return size_32_bits_real_float;
					case sizeof(uint64_t):
						return size_64_bits_real_float;
#else // REAL_T_IS_DOUBLE
					case sizeof(uint32_t):
						return size_32_bits_real_double;
					case sizeof(uint64_t):
						return size_64_bits_real_double;
#endif
				}
				return -1;
			}
		} type_size_array[Variant::VARIANT_MAX + 1] = {
			{ Variant::NIL, 0, 0, 0, 0 },
			{ Variant::BOOL, sizeof(uint8_t), sizeof(uint8_t), sizeof(uint8_t), sizeof(uint8_t) },
			{ Variant::INT, sizeof(int64_t), sizeof(int64_t), sizeof(int64_t), sizeof(int64_t) },
			{ Variant::FLOAT, sizeof(double), sizeof(double), sizeof(double), sizeof(double) },
			{ Variant::STRING, ptrsize_32, ptrsize_64, ptrsize_32, ptrsize_64 },
			{ Variant::VECTOR2, 2 * sizeof(float), 2 * sizeof(float), 2 * sizeof(double), 2 * sizeof(double) },
			{ Variant::VECTOR2I, 2 * sizeof(int32_t), 2 * sizeof(int32_t), 2 * sizeof(int32_t), 2 * sizeof(int32_t) },
			{ Variant::RECT2, 4 * sizeof(float), 4 * sizeof(float), 4 * sizeof(double), 4 * sizeof(double) },
			{ Variant::RECT2I, 4 * sizeof(int32_t), 4 * sizeof(int32_t), 4 * sizeof(int32_t), 4 * sizeof(int32_t) },
			{ Variant::VECTOR3, vec3_elems * sizeof(float), vec3_elems * sizeof(float), vec3_elems * sizeof(double), vec3_elems * sizeof(double) },
			{ Variant::VECTOR3I, 3 * sizeof(int32_t), 3 * sizeof(int32_t), 3 * sizeof(int32_t), 3 * sizeof(int32_t) },
			{ Variant::TRANSFORM2D, 6 * sizeof(float), 6 * sizeof(float), 6 * sizeof(double), 6 * sizeof(double) },
			{ Variant::VECTOR4, 4 * sizeof(float), 4 * sizeof(float), 4 * sizeof(double), 4 * sizeof(double) },
			{ Variant::VECTOR4I, 4 * sizeof(int32_t), 4 * sizeof(int32_t), 4 * sizeof(int32_t), 4 * sizeof(int32_t) },
			{ Variant::PLANE, (vec3_elems + 1) * sizeof(float), (vec3_elems + 1) * sizeof(float), (vec3_elems + 1) * sizeof(double), (vec3_elems + 1) * sizeof(double) },
			{ Variant::QUATERNION, 4 * sizeof(float), 4 * sizeof(float), 4 * sizeof(double), 4 * sizeof(double) },
			{ Variant::AABB, (vec3_elems * 2) * sizeof(float), (vec3_elems * 2) * sizeof(float), (vec3_elems * 2) * sizeof(double), (vec3_elems * 2) * sizeof(double) },
			{ Variant::BASIS, (vec3_elems * 3) * sizeof(float), (vec3_elems * 3) * sizeof(float), (vec3_elems * 3) * sizeof(double), (vec3_elems * 3) * sizeof(double) },
			{ Variant::TRANSFORM3D, (vec3_elems * 4) * sizeof(float), (vec3_elems * 4) * sizeof(float), (vec3_elems * 4) * sizeof(double), (vec3_elems * 4) * sizeof(double) },
			{ Variant::PROJECTION, (vec4_elems * 4) * sizeof(float), (vec4_elems * 4) * sizeof(float), (vec4_elems * 4) * sizeof(double), (vec4_elems * 4) * sizeof(double) },
			{ Variant::COLOR, 4 * sizeof(float), 4 * sizeof(float), 4 * sizeof(float), 4 * sizeof(float) },
			{ Variant::STRING_NAME, ptrsize_32, ptrsize_64, ptrsize_32, ptrsize_64 },
			{ Variant::NODE_PATH, ptrsize_32, ptrsize_64, ptrsize_32, ptrsize_64 },
			{ Variant::RID, sizeof(uint64_t), sizeof(uint64_t), sizeof(uint64_t), sizeof(uint64_t) },
			{ Variant::OBJECT, ptrsize_32, ptrsize_64, ptrsize_32, ptrsize_64 },
			{ Variant::CALLABLE, sizeof(Callable), sizeof(Callable), sizeof(Callable), sizeof(Callable) }, // Hardcoded align.
			{ Variant::SIGNAL, sizeof(Signal), sizeof(Signal), sizeof(Signal), sizeof(Signal) }, // Hardcoded align.
			{ Variant::DICTIONARY, ptrsize_32, ptrsize_64, ptrsize_32, ptrsize_64 },
			{ Variant::ARRAY, ptrsize_32, ptrsize_64, ptrsize_32, ptrsize_64 },
			{ Variant::PACKED_BYTE_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_INT32_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_INT64_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_FLOAT32_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_FLOAT64_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_STRING_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_VECTOR2_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_VECTOR3_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_COLOR_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::PACKED_VECTOR4_ARRAY, ptrsize_32 * 2, ptrsize_64 * 2, ptrsize_32 * 2, ptrsize_64 * 2 },
			{ Variant::VARIANT_MAX, sizeof(uint64_t) + sizeof(float) * 4, sizeof(uint64_t) + sizeof(float) * 4, sizeof(uint64_t) + sizeof(double) * 4, sizeof(uint64_t) + sizeof(double) * 4 },
		};

		// Validate sizes at compile time for the current build configuration.
		static_assert(type_size_array[Variant::BOOL][sizeof(void *)] == sizeof(GDExtensionBool), "Size of bool mismatch");
		static_assert(type_size_array[Variant::INT][sizeof(void *)] == sizeof(GDExtensionInt), "Size of int mismatch");
		static_assert(type_size_array[Variant::FLOAT][sizeof(void *)] == sizeof(double), "Size of float mismatch");
		static_assert(type_size_array[Variant::STRING][sizeof(void *)] == sizeof(String), "Size of String mismatch");
		static_assert(type_size_array[Variant::VECTOR2][sizeof(void *)] == sizeof(Vector2), "Size of Vector2 mismatch");
		static_assert(type_size_array[Variant::VECTOR2I][sizeof(void *)] == sizeof(Vector2i), "Size of Vector2i mismatch");
		static_assert(type_size_array[Variant::RECT2][sizeof(void *)] == sizeof(Rect2), "Size of Rect2 mismatch");
		static_assert(type_size_array[Variant::RECT2I][sizeof(void *)] == sizeof(Rect2i), "Size of Rect2i mismatch");
		static_assert(type_size_array[Variant::VECTOR3][sizeof(void *)] == sizeof(Vector3), "Size of Vector3 mismatch");
		static_assert(type_size_array[Variant::VECTOR3I][sizeof(void *)] == sizeof(Vector3i), "Size of Vector3i mismatch");
		static_assert(type_size_array[Variant::TRANSFORM2D][sizeof(void *)] == sizeof(Transform2D), "Size of Transform2D mismatch");
		static_assert(type_size_array[Variant::VECTOR4][sizeof(void *)] == sizeof(Vector4), "Size of Vector4 mismatch");
		static_assert(type_size_array[Variant::VECTOR4I][sizeof(void *)] == sizeof(Vector4i), "Size of Vector4i mismatch");
		static_assert(type_size_array[Variant::PLANE][sizeof(void *)] == sizeof(Plane), "Size of Plane mismatch");
		static_assert(type_size_array[Variant::QUATERNION][sizeof(void *)] == sizeof(Quaternion), "Size of Quaternion mismatch");
		static_assert(type_size_array[Variant::AABB][sizeof(void *)] == sizeof(AABB), "Size of AABB mismatch");
		static_assert(type_size_array[Variant::BASIS][sizeof(void *)] == sizeof(Basis), "Size of Basis mismatch");
		static_assert(type_size_array[Variant::TRANSFORM3D][sizeof(void *)] == sizeof(Transform3D), "Size of Transform3D mismatch");
		static_assert(type_size_array[Variant::PROJECTION][sizeof(void *)] == sizeof(Projection), "Size of Projection mismatch");
		static_assert(type_size_array[Variant::COLOR][sizeof(void *)] == sizeof(Color), "Size of Color mismatch");
		static_assert(type_size_array[Variant::STRING_NAME][sizeof(void *)] == sizeof(StringName), "Size of StringName mismatch");
		static_assert(type_size_array[Variant::NODE_PATH][sizeof(void *)] == sizeof(NodePath), "Size of NodePath mismatch");
		static_assert(type_size_array[Variant::RID][sizeof(void *)] == sizeof(RID), "Size of RID mismatch");
		static_assert(type_size_array[Variant::OBJECT][sizeof(void *)] == sizeof(Object *), "Size of Object mismatch");
		static_assert(type_size_array[Variant::CALLABLE][sizeof(void *)] == sizeof(Callable), "Size of Callable mismatch");
		static_assert(type_size_array[Variant::SIGNAL][sizeof(void *)] == sizeof(Signal), "Size of Signal mismatch");
		static_assert(type_size_array[Variant::DICTIONARY][sizeof(void *)] == sizeof(Dictionary), "Size of Dictionary mismatch");
		static_assert(type_size_array[Variant::ARRAY][sizeof(void *)] == sizeof(Array), "Size of Array mismatch");
		static_assert(type_size_array[Variant::PACKED_BYTE_ARRAY][sizeof(void *)] == sizeof(PackedByteArray), "Size of PackedByteArray mismatch");
		static_assert(type_size_array[Variant::PACKED_INT32_ARRAY][sizeof(void *)] == sizeof(PackedInt32Array), "Size of PackedInt32Array mismatch");
		static_assert(type_size_array[Variant::PACKED_INT64_ARRAY][sizeof(void *)] == sizeof(PackedInt64Array), "Size of PackedInt64Array mismatch");
		static_assert(type_size_array[Variant::PACKED_FLOAT32_ARRAY][sizeof(void *)] == sizeof(PackedFloat32Array), "Size of PackedFloat32Array mismatch");
		static_assert(type_size_array[Variant::PACKED_FLOAT64_ARRAY][sizeof(void *)] == sizeof(PackedFloat64Array), "Size of PackedFloat64Array mismatch");
		static_assert(type_size_array[Variant::PACKED_STRING_ARRAY][sizeof(void *)] == sizeof(PackedStringArray), "Size of PackedStringArray mismatch");
		static_assert(type_size_array[Variant::PACKED_VECTOR2_ARRAY][sizeof(void *)] == sizeof(PackedVector2Array), "Size of PackedVector2Array mismatch");
		static_assert(type_size_array[Variant::PACKED_VECTOR3_ARRAY][sizeof(void *)] == sizeof(PackedVector3Array), "Size of PackedVector3Array mismatch");
		static_assert(type_size_array[Variant::PACKED_COLOR_ARRAY][sizeof(void *)] == sizeof(PackedColorArray), "Size of PackedColorArray mismatch");
		static_assert(type_size_array[Variant::PACKED_VECTOR4_ARRAY][sizeof(void *)] == sizeof(PackedVector4Array), "Size of PackedVector4Array mismatch");
		static_assert(type_size_array[Variant::VARIANT_MAX][sizeof(void *)] == sizeof(Variant), "Size of Variant mismatch");

		Array core_type_sizes;

		for (int i = 0; i < 4; i++) {
			Dictionary d;
			d["build_configuration"] = build_config_name[i];
			Array sizes;
			for (int j = 0; j <= Variant::VARIANT_MAX; j++) {
				Variant::Type t = type_size_array[j].type;
				String name = t == Variant::VARIANT_MAX ? String("Variant") : Variant::get_type_name(t);
				Dictionary d2;
				d2["name"] = name;
				uint32_t size = 0;
				switch (i) {
					case 0:
						size = type_size_array[j].size_32_bits_real_float;
						break;
					case 1:
						size = type_size_array[j].size_64_bits_real_float;
						break;
					case 2:
						size = type_size_array[j].size_32_bits_real_double;
						break;
					case 3:
						size = type_size_array[j].size_64_bits_real_double;
						break;
				}
				d2["size"] = size;
				sizes.push_back(d2);
			}
			d["sizes"] = sizes;
			core_type_sizes.push_back(d);
		}
		api_dump["builtin_class_sizes"] = core_type_sizes;
	}

	{
		// Member offsets, meta types and sizes.

#define REAL_MEMBER_OFFSET(type, member) \
	{                                    \
		type,                            \
				member,                  \
				"float",                 \
				sizeof(float),           \
				"float",                 \
				sizeof(float),           \
				"double",                \
				sizeof(double),          \
				"double",                \
				sizeof(double),          \
	}

#define INT32_MEMBER_OFFSET(type, member) \
	{                                     \
		type,                             \
				member,                   \
				"int32",                  \
				sizeof(int32_t),          \
				"int32",                  \
				sizeof(int32_t),          \
				"int32",                  \
				sizeof(int32_t),          \
				"int32",                  \
				sizeof(int32_t),          \
	}

#define INT32_BASED_BUILTIN_MEMBER_OFFSET(type, member, member_type, member_elems) \
	{                                                                              \
		type,                                                                      \
				member,                                                            \
				member_type,                                                       \
				sizeof(int32_t) * member_elems,                                    \
				member_type,                                                       \
				sizeof(int32_t) * member_elems,                                    \
				member_type,                                                       \
				sizeof(int32_t) * member_elems,                                    \
				member_type,                                                       \
				sizeof(int32_t) * member_elems,                                    \
	}

#define REAL_BASED_BUILTIN_MEMBER_OFFSET(type, member, member_type, member_elems) \
	{                                                                             \
		type,                                                                     \
				member,                                                           \
				member_type,                                                      \
				sizeof(float) * member_elems,                                     \
				member_type,                                                      \
				sizeof(float) * member_elems,                                     \
				member_type,                                                      \
				sizeof(double) * member_elems,                                    \
				member_type,                                                      \
				sizeof(double) * member_elems,                                    \
	}

		struct {
			Variant::Type type;
			const char *member;
			const char *member_meta_32_bits_real_float;
			const uint32_t member_size_32_bits_real_float;
			const char *member_meta_64_bits_real_float;
			const uint32_t member_size_64_bits_real_float;
			const char *member_meta_32_bits_real_double;
			const uint32_t member_size_32_bits_real_double;
			const char *member_meta_64_bits_real_double;
			const uint32_t member_size_64_bits_real_double;
		} member_offset_array[] = {
			// Vector2
			REAL_MEMBER_OFFSET(Variant::VECTOR2, "x"),
			REAL_MEMBER_OFFSET(Variant::VECTOR2, "y"),
			// Vector2i
			INT32_MEMBER_OFFSET(Variant::VECTOR2I, "x"),
			INT32_MEMBER_OFFSET(Variant::VECTOR2I, "y"),
			// Rect2
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::RECT2, "position", "Vector2", 2),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::RECT2, "size", "Vector2", 2),
			// Rect2i
			INT32_BASED_BUILTIN_MEMBER_OFFSET(Variant::RECT2I, "position", "Vector2i", 2),
			INT32_BASED_BUILTIN_MEMBER_OFFSET(Variant::RECT2I, "size", "Vector2i", 2),
			// Vector3
			REAL_MEMBER_OFFSET(Variant::VECTOR3, "x"),
			REAL_MEMBER_OFFSET(Variant::VECTOR3, "y"),
			REAL_MEMBER_OFFSET(Variant::VECTOR3, "z"),
			// Vector3i
			INT32_MEMBER_OFFSET(Variant::VECTOR3I, "x"),
			INT32_MEMBER_OFFSET(Variant::VECTOR3I, "y"),
			INT32_MEMBER_OFFSET(Variant::VECTOR3I, "z"),
			// Transform2D
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::TRANSFORM2D, "x", "Vector2", 2),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::TRANSFORM2D, "y", "Vector2", 2),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::TRANSFORM2D, "origin", "Vector2", 2),
			// Vector4
			REAL_MEMBER_OFFSET(Variant::VECTOR4, "x"),
			REAL_MEMBER_OFFSET(Variant::VECTOR4, "y"),
			REAL_MEMBER_OFFSET(Variant::VECTOR4, "z"),
			REAL_MEMBER_OFFSET(Variant::VECTOR4, "w"),
			// Vector4i
			INT32_MEMBER_OFFSET(Variant::VECTOR4I, "x"),
			INT32_MEMBER_OFFSET(Variant::VECTOR4I, "y"),
			INT32_MEMBER_OFFSET(Variant::VECTOR4I, "z"),
			INT32_MEMBER_OFFSET(Variant::VECTOR4I, "w"),
			// Plane
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::PLANE, "normal", "Vector3", vec3_elems),
			REAL_MEMBER_OFFSET(Variant::PLANE, "d"),
			// Quaternion
			REAL_MEMBER_OFFSET(Variant::QUATERNION, "x"),
			REAL_MEMBER_OFFSET(Variant::QUATERNION, "y"),
			REAL_MEMBER_OFFSET(Variant::QUATERNION, "z"),
			REAL_MEMBER_OFFSET(Variant::QUATERNION, "w"),
			// AABB
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::AABB, "position", "Vector3", vec3_elems),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::AABB, "size", "Vector3", vec3_elems),
			// Basis (remember that basis vectors are flipped!)
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::BASIS, "x", "Vector3", vec3_elems),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::BASIS, "y", "Vector3", vec3_elems),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::BASIS, "z", "Vector3", vec3_elems),
			// Transform3D
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::TRANSFORM3D, "basis", "Basis", vec3_elems * 3),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::TRANSFORM3D, "origin", "Vector3", vec3_elems),
			// Projection
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::PROJECTION, "x", "Vector4", vec4_elems),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::PROJECTION, "y", "Vector4", vec4_elems),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::PROJECTION, "z", "Vector4", vec4_elems),
			REAL_BASED_BUILTIN_MEMBER_OFFSET(Variant::PROJECTION, "w", "Vector4", vec4_elems),
			// Color (always composed of 4bytes floats)
			{ Variant::COLOR, "r", "float", sizeof(float), "float", sizeof(float), "float", sizeof(float), "float", sizeof(float) },
			{ Variant::COLOR, "g", "float", sizeof(float), "float", sizeof(float), "float", sizeof(float), "float", sizeof(float) },
			{ Variant::COLOR, "b", "float", sizeof(float), "float", sizeof(float), "float", sizeof(float), "float", sizeof(float) },
			{ Variant::COLOR, "a", "float", sizeof(float), "float", sizeof(float), "float", sizeof(float), "float", sizeof(float) },
			// End marker, must stay last
			{ Variant::NIL, nullptr, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0 },
		};

		Array core_type_member_offsets;

		for (int i = 0; i < 4; i++) {
			Dictionary d;
			d["build_configuration"] = build_config_name[i];
			Array type_offsets;
			uint32_t idx = 0;

			Variant::Type previous_type = Variant::NIL;

			Dictionary d2;
			Array members;
			uint32_t offset = 0;

			while (true) {
				Variant::Type t = member_offset_array[idx].type;
				if (t != previous_type) {
					if (previous_type != Variant::NIL) {
						d2["members"] = members;
						type_offsets.push_back(d2);
					}
					if (t == Variant::NIL) {
						break;
					}

					String name = t == Variant::VARIANT_MAX ? String("Variant") : Variant::get_type_name(t);
					d2 = Dictionary();
					members = Array();
					offset = 0;
					d2["name"] = name;
					previous_type = t;
				}
				Dictionary d3;
				const char *member_meta = nullptr;
				uint32_t member_size = 0;
				switch (i) {
					case 0:
						member_meta = member_offset_array[idx].member_meta_32_bits_real_float;
						member_size = member_offset_array[idx].member_size_32_bits_real_float;
						break;
					case 1:
						member_meta = member_offset_array[idx].member_meta_64_bits_real_float;
						member_size = member_offset_array[idx].member_size_64_bits_real_float;
						break;
					case 2:
						member_meta = member_offset_array[idx].member_meta_32_bits_real_double;
						member_size = member_offset_array[idx].member_size_32_bits_real_double;
						break;
					case 3:
						member_meta = member_offset_array[idx].member_meta_64_bits_real_double;
						member_size = member_offset_array[idx].member_size_64_bits_real_double;
						break;
				}
				d3["member"] = member_offset_array[idx].member;
				d3["offset"] = offset;
				d3["meta"] = member_meta;
				offset += member_size;
				members.push_back(d3);
				idx++;
			}
			d["classes"] = type_offsets;
			core_type_member_offsets.push_back(d);
		}
		api_dump["builtin_class_member_offsets"] = core_type_member_offsets;
	}

	if (p_include_docs) {
		EditorHelp::generate_doc(false);
	}

	{
		// Global enums and constants.
		Array constants;
		HashMap<String, List<Pair<String, int64_t>>> enum_list;
		HashMap<String, bool> enum_is_bitfield;

		const DocData::ClassDoc *global_scope_doc = nullptr;
		if (p_include_docs) {
			global_scope_doc = EditorHelp::get_doc_data()->class_list.getptr("@GlobalScope");
			CRASH_COND_MSG(!global_scope_doc, "Could not find '@GlobalScope' in DocData.");
		}

		for (int i = 0; i < CoreConstants::get_global_constant_count(); i++) {
			int64_t value = CoreConstants::get_global_constant_value(i);
			String enum_name = CoreConstants::get_global_constant_enum(i);
			String name = CoreConstants::get_global_constant_name(i);
			bool bitfield = CoreConstants::is_global_constant_bitfield(i);
			if (!enum_name.is_empty()) {
				enum_list[enum_name].push_back(Pair<String, int64_t>(name, value));
				enum_is_bitfield[enum_name] = bitfield;
			} else {
				Dictionary d;
				d["name"] = name;
				d["value"] = value;
				d["is_bitfield"] = bitfield;
				if (p_include_docs) {
					for (const DocData::ConstantDoc &constant_doc : global_scope_doc->constants) {
						if (constant_doc.name == name) {
							d["description"] = fix_doc_description(constant_doc.description);
							break;
						}
					}
				}
				constants.push_back(d);
			}
		}

		api_dump["global_constants"] = constants;

		Array enums;
		for (const KeyValue<String, List<Pair<String, int64_t>>> &E : enum_list) {
			Dictionary d1;
			d1["name"] = E.key;
			d1["is_bitfield"] = enum_is_bitfield[E.key];
			if (p_include_docs) {
				const DocData::EnumDoc *enum_doc = global_scope_doc->enums.getptr(E.key);
				if (enum_doc) {
					d1["description"] = fix_doc_description(enum_doc->description);
				}
			}
			Array values;
			for (const Pair<String, int64_t> &F : E.value) {
				Dictionary d2;
				d2["name"] = F.first;
				d2["value"] = F.second;
				if (p_include_docs) {
					for (const DocData::ConstantDoc &constant_doc : global_scope_doc->constants) {
						if (constant_doc.name == F.first) {
							d2["description"] = fix_doc_description(constant_doc.description);
							break;
						}
					}
				}
				values.push_back(d2);
			}
			d1["values"] = values;
			enums.push_back(d1);
		}

		api_dump["global_enums"] = enums;
	}
	{
		Array utility_funcs;

		List<StringName> utility_func_names;
		Variant::get_utility_function_list(&utility_func_names);

		const DocData::ClassDoc *global_scope_doc = nullptr;
		if (p_include_docs) {
			global_scope_doc = EditorHelp::get_doc_data()->class_list.getptr("@GlobalScope");
			CRASH_COND_MSG(!global_scope_doc, "Could not find '@GlobalScope' in DocData.");
		}

		for (const StringName &name : utility_func_names) {
			Dictionary func;
			func["name"] = String(name);
			if (Variant::has_utility_function_return_value(name)) {
				Variant::Type rt = Variant::get_utility_function_return_type(name);
				func["return_type"] = rt == Variant::NIL ? String("Variant") : Variant::get_type_name(rt);
			}
			switch (Variant::get_utility_function_type(name)) {
				case Variant::UTILITY_FUNC_TYPE_MATH:
					func["category"] = "math";
					break;
				case Variant::UTILITY_FUNC_TYPE_RANDOM:
					func["category"] = "random";
					break;
				case Variant::UTILITY_FUNC_TYPE_GENERAL:
					func["category"] = "general";
					break;
			}
			bool vararg = Variant::is_utility_function_vararg(name);
			func["is_vararg"] = Variant::is_utility_function_vararg(name);
			func["hash"] = Variant::get_utility_function_hash(name);
			Array arguments;
			int argcount = Variant::get_utility_function_argument_count(name);
			for (int i = 0; i < argcount; i++) {
				Dictionary arg;
				String argname = vararg ? "arg" + itos(i + 1) : Variant::get_utility_function_argument_name(name, i);
				arg["name"] = argname;
				arg["type"] = get_builtin_or_variant_type_name(Variant::get_utility_function_argument_type(name, i));
				//no default value support in utility functions
				arguments.push_back(arg);
			}

			if (arguments.size()) {
				func["arguments"] = arguments;
			}

			if (p_include_docs) {
				for (const DocData::MethodDoc &method_doc : global_scope_doc->methods) {
					if (method_doc.name == name) {
						func["description"] = fix_doc_description(method_doc.description);
						break;
					}
				}
			}

			utility_funcs.push_back(func);
		}

		api_dump["utility_functions"] = utility_funcs;
	}

	{
		// builtin types

		Array builtins;

		for (int i = 0; i < Variant::VARIANT_MAX; i++) {
			if (i == Variant::OBJECT) {
				continue;
			}

			Variant::Type type = Variant::Type(i);

			Dictionary d;
			d["name"] = Variant::get_type_name(type);
			if (Variant::has_indexing(type)) {
				d["indexing_return_type"] = get_builtin_or_variant_type_name(Variant::get_indexed_element_type(type));
			}

			d["is_keyed"] = Variant::is_keyed(type);

			DocData::ClassDoc *builtin_doc = nullptr;
			if (p_include_docs && d["name"] != "Nil") {
				builtin_doc = EditorHelp::get_doc_data()->class_list.getptr(d["name"]);
				CRASH_COND_MSG(!builtin_doc, vformat("Could not find '%s' in DocData.", d["name"]));
			}

			{
				//members
				Array members;

				List<StringName> member_names;
				Variant::get_member_list(type, &member_names);
				for (const StringName &member_name : member_names) {
					Dictionary d2;
					d2["name"] = String(member_name);
					d2["type"] = get_builtin_or_variant_type_name(Variant::get_member_type(type, member_name));
					if (p_include_docs) {
						for (const DocData::PropertyDoc &property_doc : builtin_doc->properties) {
							if (property_doc.name == member_name) {
								d2["description"] = fix_doc_description(property_doc.description);
								break;
							}
						}
					}
					members.push_back(d2);
				}
				if (members.size()) {
					d["members"] = members;
				}
			}
			{
				//constants
				Array constants;

				List<StringName> constant_names;
				Variant::get_constants_for_type(type, &constant_names);
				for (const StringName &constant_name : constant_names) {
					Dictionary d2;
					d2["name"] = String(constant_name);
					Variant constant = Variant::get_constant_value(type, constant_name);
					d2["type"] = get_builtin_or_variant_type_name(constant.get_type());
					d2["value"] = constant.get_construct_string();
					if (p_include_docs) {
						for (const DocData::ConstantDoc &constant_doc : builtin_doc->constants) {
							if (constant_doc.name == constant_name) {
								d2["description"] = fix_doc_description(constant_doc.description);
								break;
							}
						}
					}
					constants.push_back(d2);
				}
				if (constants.size()) {
					d["constants"] = constants;
				}
			}
			{
				//enums
				Array enums;

				List<StringName> enum_names;
				Variant::get_enums_for_type(type, &enum_names);
				for (const StringName &enum_name : enum_names) {
					Dictionary enum_dict;
					enum_dict["name"] = String(enum_name);

					List<StringName> enumeration_names;
					Variant::get_enumerations_for_enum(type, enum_name, &enumeration_names);

					Array values;

					for (const StringName &enumeration : enumeration_names) {
						Dictionary values_dict;
						values_dict["name"] = String(enumeration);
						values_dict["value"] = Variant::get_enum_value(type, enum_name, enumeration);
						if (p_include_docs) {
							for (const DocData::ConstantDoc &constant_doc : builtin_doc->constants) {
								if (constant_doc.name == enumeration) {
									values_dict["description"] = fix_doc_description(constant_doc.description);
									break;
								}
							}
						}
						values.push_back(values_dict);
					}

					if (p_include_docs) {
						const DocData::EnumDoc *enum_doc = builtin_doc->enums.getptr(enum_name);
						if (enum_doc) {
							enum_dict["description"] = fix_doc_description(enum_doc->description);
						}
					}

					if (values.size()) {
						enum_dict["values"] = values;
					}
					enums.push_back(enum_dict);
				}

				if (enums.size()) {
					d["enums"] = enums;
				}
			}
			{
				//operators
				Array operators;

				for (int j = 0; j < Variant::VARIANT_MAX; j++) {
					for (int k = 0; k < Variant::OP_MAX; k++) {
						Variant::Type rt = Variant::get_operator_return_type(Variant::Operator(k), type, Variant::Type(j));
						if (rt != Variant::NIL) {
							Dictionary d2;
							String operator_name = Variant::get_operator_name(Variant::Operator(k));
							d2["name"] = operator_name;

							String right_type_name = get_builtin_or_variant_type_name(Variant::Type(j));
							bool is_unary = k == Variant::OP_NEGATE || k == Variant::OP_POSITIVE || k == Variant::OP_NOT || k == Variant::OP_BIT_NEGATE;
							if (!is_unary) {
								d2["right_type"] = right_type_name;
							}

							d2["return_type"] = get_builtin_or_variant_type_name(Variant::get_operator_return_type(Variant::Operator(k), type, Variant::Type(j)));

							if (p_include_docs && builtin_doc != nullptr) {
								for (const DocData::MethodDoc &operator_doc : builtin_doc->operators) {
									if (operator_doc.name == "operator " + operator_name &&
											(is_unary || operator_doc.arguments[0].type == right_type_name)) {
										d2["description"] = fix_doc_description(operator_doc.description);
										break;
									}
								}
							}

							operators.push_back(d2);
						}
					}
				}
				if (operators.size()) {
					d["operators"] = operators;
				}
			}
			{
				//methods
				Array methods;

				List<StringName> method_names;
				Variant::get_builtin_method_list(type, &method_names);
				for (const StringName &method_name : method_names) {
					Dictionary d2;
					d2["name"] = String(method_name);
					if (Variant::has_builtin_method_return_value(type, method_name)) {
						Variant::Type ret_type = Variant::get_builtin_method_return_type(type, method_name);
						d2["return_type"] = ret_type == Variant::NIL ? String("Variant") : Variant::get_type_name(ret_type);
					}
					d2["is_vararg"] = Variant::is_builtin_method_vararg(type, method_name);
					d2["is_const"] = Variant::is_builtin_method_const(type, method_name);
					d2["is_static"] = Variant::is_builtin_method_static(type, method_name);
					d2["hash"] = Variant::get_builtin_method_hash(type, method_name);

					Vector<Variant> default_args = Variant::get_builtin_method_default_arguments(type, method_name);

					Array arguments;
					int argcount = Variant::get_builtin_method_argument_count(type, method_name);
					for (int j = 0; j < argcount; j++) {
						Dictionary d3;
						d3["name"] = Variant::get_builtin_method_argument_name(type, method_name, j);
						d3["type"] = get_builtin_or_variant_type_name(Variant::get_builtin_method_argument_type(type, method_name, j));

						if (j >= (argcount - default_args.size())) {
							int dargidx = j - (argcount - default_args.size());
							d3["default_value"] = default_args[dargidx].get_construct_string();
						}
						arguments.push_back(d3);
					}

					if (arguments.size()) {
						d2["arguments"] = arguments;
					}

					if (p_include_docs) {
						for (const DocData::MethodDoc &method_doc : builtin_doc->methods) {
							if (method_doc.name == method_name) {
								d2["description"] = fix_doc_description(method_doc.description);
								break;
							}
						}
					}

					methods.push_back(d2);
				}
				if (methods.size()) {
					d["methods"] = methods;
				}
			}
			{
				//constructors
				Array constructors;

				for (int j = 0; j < Variant::get_constructor_count(type); j++) {
					Dictionary d2;
					d2["index"] = j;

					Array arguments;
					int argcount = Variant::get_constructor_argument_count(type, j);
					for (int k = 0; k < argcount; k++) {
						Dictionary d3;
						d3["name"] = Variant::get_constructor_argument_name(type, j, k);
						d3["type"] = get_builtin_or_variant_type_name(Variant::get_constructor_argument_type(type, j, k));
						arguments.push_back(d3);
					}
					if (arguments.size()) {
						d2["arguments"] = arguments;
					}

					if (p_include_docs && builtin_doc) {
						for (const DocData::MethodDoc &constructor_doc : builtin_doc->constructors) {
							if (constructor_doc.arguments.size() != argcount) {
								continue;
							}
							bool constructor_found = true;
							for (int k = 0; k < argcount; k++) {
								const DocData::ArgumentDoc &argument_doc = constructor_doc.arguments[k];
								const Dictionary &argument_dict = arguments[k];
								const String &argument_string = argument_dict["type"];
								if (argument_doc.type != argument_string) {
									constructor_found = false;
									break;
								}
							}
							if (constructor_found) {
								d2["description"] = fix_doc_description(constructor_doc.description);
							}
						}
					}

					constructors.push_back(d2);
				}

				if (constructors.size()) {
					d["constructors"] = constructors;
				}
			}
			{
				//destructor
				d["has_destructor"] = Variant::has_destructor(type);
			}

			if (p_include_docs && builtin_doc != nullptr) {
				d["brief_description"] = fix_doc_description(builtin_doc->brief_description);
				d["description"] = fix_doc_description(builtin_doc->description);
			}

			builtins.push_back(d);
		}

		api_dump["builtin_classes"] = builtins;
	}

	{
		// classes
		Array classes;

		List<StringName> class_list;

		ClassDB::get_class_list(&class_list);

		class_list.sort_custom<StringName::AlphCompare>();

		for (const StringName &class_name : class_list) {
			if (!ClassDB::is_class_exposed(class_name)) {
				continue;
			}
			Dictionary d;
			d["name"] = String(class_name);
			d["is_refcounted"] = ClassDB::is_parent_class(class_name, "RefCounted");
			d["is_instantiable"] = ClassDB::can_instantiate(class_name);
			StringName parent_class = ClassDB::get_parent_class(class_name);
			if (parent_class != StringName()) {
				d["inherits"] = String(parent_class);
			}

			DocData::ClassDoc *class_doc = nullptr;
			if (p_include_docs) {
				class_doc = EditorHelp::get_doc_data()->class_list.getptr(class_name);
				CRASH_COND_MSG(!class_doc, vformat("Could not find '%s' in DocData.", class_name));
			}

			{
				ClassDB::APIType api = ClassDB::get_api_type(class_name);
				static const char *api_type[5] = { "core", "editor", "extension", "editor_extension" };
				d["api_type"] = api_type[api];
			}

			{
				//constants
				Array constants;
				List<String> constant_list;
				ClassDB::get_integer_constant_list(class_name, &constant_list, true);
				for (const String &F : constant_list) {
					StringName enum_name = ClassDB::get_integer_constant_enum(class_name, F);
					if (enum_name != StringName()) {
						continue; //enums will be handled on their own
					}

					Dictionary d2;
					d2["name"] = String(F);
					d2["value"] = ClassDB::get_integer_constant(class_name, F);

					if (p_include_docs) {
						for (const DocData::ConstantDoc &constant_doc : class_doc->constants) {
							if (constant_doc.name == F) {
								d2["description"] = fix_doc_description(constant_doc.description);
								break;
							}
						}
					}

					constants.push_back(d2);
				}

				if (constants.size()) {
					d["constants"] = constants;
				}
			}
			{
				//enum
				Array enums;
				List<StringName> enum_list;
				ClassDB::get_enum_list(class_name, &enum_list, true);
				for (const StringName &F : enum_list) {
					Dictionary d2;
					d2["name"] = String(F);
					d2["is_bitfield"] = ClassDB::is_enum_bitfield(class_name, F);

					Array values;
					List<StringName> enum_constant_list;
					ClassDB::get_enum_constants(class_name, F, &enum_constant_list, true);
					for (List<StringName>::Element *G = enum_constant_list.front(); G; G = G->next()) {
						Dictionary d3;
						d3["name"] = String(G->get());
						d3["value"] = ClassDB::get_integer_constant(class_name, G->get());

						if (p_include_docs) {
							for (const DocData::ConstantDoc &constant_doc : class_doc->constants) {
								if (constant_doc.name == G->get()) {
									d3["description"] = fix_doc_description(constant_doc.description);
									break;
								}
							}
						}

						values.push_back(d3);
					}

					d2["values"] = values;

					if (p_include_docs) {
						const DocData::EnumDoc *enum_doc = class_doc->enums.getptr(F);
						if (enum_doc) {
							d2["description"] = fix_doc_description(enum_doc->description);
						}
					}

					enums.push_back(d2);
				}

				if (enums.size()) {
					d["enums"] = enums;
				}
			}
			{
				//methods
				Array methods;
				List<MethodInfo> method_list;
				ClassDB::get_method_list(class_name, &method_list, true);
				for (const MethodInfo &F : method_list) {
					StringName method_name = F.name;
					if ((F.flags & METHOD_FLAG_VIRTUAL) && !(F.flags & METHOD_FLAG_OBJECT_CORE)) {
						//virtual method
						const MethodInfo &mi = F;
						Dictionary d2;
						d2["name"] = String(method_name);
						d2["is_const"] = (F.flags & METHOD_FLAG_CONST) ? true : false;
						d2["is_static"] = (F.flags & METHOD_FLAG_STATIC) ? true : false;
						d2["is_required"] = (F.flags & METHOD_FLAG_VIRTUAL_REQUIRED) ? true : false;
						d2["is_vararg"] = false;
						d2["is_virtual"] = true;
						// virtual functions have no hash since no MethodBind is involved
						bool has_return = mi.return_val.type != Variant::NIL || (mi.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT);
						if (has_return) {
							PropertyInfo pinfo = mi.return_val;
							Dictionary d3;

							d3["type"] = get_property_info_type_name(pinfo);

							if (mi.get_argument_meta(-1) > 0) {
								d3["meta"] = get_type_meta_name((GodotTypeInfo::Metadata)mi.get_argument_meta(-1));
							}

							d2["return_value"] = d3;
						}

						Array arguments;
						int i = 0;
						for (List<PropertyInfo>::ConstIterator itr = mi.arguments.begin(); itr != mi.arguments.end(); ++itr, ++i) {
							const PropertyInfo &pinfo = *itr;
							Dictionary d3;

							d3["name"] = pinfo.name;

							d3["type"] = get_property_info_type_name(pinfo);

							if (mi.get_argument_meta(i) > 0) {
								d3["meta"] = get_type_meta_name((GodotTypeInfo::Metadata)mi.get_argument_meta(i));
							}

							arguments.push_back(d3);
						}

						if (arguments.size()) {
							d2["arguments"] = arguments;
						}

						if (p_include_docs) {
							for (const DocData::MethodDoc &method_doc : class_doc->methods) {
								if (method_doc.name == method_name) {
									d2["description"] = fix_doc_description(method_doc.description);
									break;
								}
							}
						}

						methods.push_back(d2);

					} else if (F.name.begins_with("_")) {
						//hidden method, ignore

					} else {
						Dictionary d2;
						d2["name"] = String(method_name);

						MethodBind *method = ClassDB::get_method(class_name, method_name);
						if (!method) {
							continue;
						}

						d2["is_const"] = method->is_const();
						d2["is_vararg"] = method->is_vararg();
						d2["is_static"] = method->is_static();
						d2["is_virtual"] = false;
						d2["hash"] = method->get_hash();

						Vector<uint32_t> compat_hashes = ClassDB::get_method_compatibility_hashes(class_name, method_name);
						Array compatibility;
						if (compat_hashes.size()) {
							for (int i = 0; i < compat_hashes.size(); i++) {
								compatibility.push_back(compat_hashes[i]);
							}
						}

#ifndef DISABLE_DEPRECATED
						GDExtensionCompatHashes::get_legacy_hashes(class_name, method_name, compatibility);
#endif

						if (compatibility.size() > 0) {
							d2["hash_compatibility"] = compatibility;
						}

						Vector<Variant> default_args = method->get_default_arguments();

						Array arguments;
						for (int i = (method->has_return() ? -1 : 0); i < method->get_argument_count(); i++) {
							PropertyInfo pinfo = i == -1 ? method->get_return_info() : method->get_argument_info(i);
							Dictionary d3;

							if (i >= 0) {
								d3["name"] = pinfo.name;
							}
							d3["type"] = get_property_info_type_name(pinfo);

							if (method->get_argument_meta(i) > 0) {
								d3["meta"] = get_type_meta_name(method->get_argument_meta(i));
							}

							if (i >= 0 && i >= (method->get_argument_count() - default_args.size())) {
								int dargidx = i - (method->get_argument_count() - default_args.size());
								d3["default_value"] = default_args[dargidx].get_construct_string();
							}

							if (i == -1) {
								d2["return_value"] = d3;
							} else {
								arguments.push_back(d3);
							}
						}

						if (arguments.size()) {
							d2["arguments"] = arguments;
						}

						if (p_include_docs) {
							for (const DocData::MethodDoc &method_doc : class_doc->methods) {
								if (method_doc.name == method_name) {
									d2["description"] = fix_doc_description(method_doc.description);
									break;
								}
							}
						}

						methods.push_back(d2);
					}
				}

				if (methods.size()) {
					d["methods"] = methods;
				}
			}

			{
				//signals
				Array signals;
				List<MethodInfo> signal_list;
				ClassDB::get_signal_list(class_name, &signal_list, true);
				for (const MethodInfo &F : signal_list) {
					StringName signal_name = F.name;
					Dictionary d2;
					d2["name"] = String(signal_name);

					Array arguments;

					int i = 0;
					for (List<PropertyInfo>::ConstIterator itr = F.arguments.begin(); itr != F.arguments.end(); ++itr, ++i) {
						Dictionary d3;
						d3["name"] = itr->name;
						d3["type"] = get_property_info_type_name(*itr);
						if (F.get_argument_meta(i) > 0) {
							d3["meta"] = get_type_meta_name((GodotTypeInfo::Metadata)F.get_argument_meta(i));
						}
						arguments.push_back(d3);
					}
					if (arguments.size()) {
						d2["arguments"] = arguments;
					}

					if (p_include_docs) {
						for (const DocData::MethodDoc &signal_doc : class_doc->signals) {
							if (signal_doc.name == signal_name) {
								d2["description"] = fix_doc_description(signal_doc.description);
								break;
							}
						}
					}

					signals.push_back(d2);
				}

				if (signals.size()) {
					d["signals"] = signals;
				}
			}
			{
				//properties
				Array properties;
				List<PropertyInfo> property_list;
				ClassDB::get_property_list(class_name, &property_list, true);
				for (const PropertyInfo &F : property_list) {
					if (F.usage & PROPERTY_USAGE_CATEGORY || F.usage & PROPERTY_USAGE_GROUP || F.usage & PROPERTY_USAGE_SUBGROUP || (F.type == Variant::NIL && F.usage & PROPERTY_USAGE_ARRAY)) {
						continue; //not real properties
					}
					if (F.name.begins_with("_")) {
						continue; //hidden property
					}
					if (F.name.contains_char('/')) {
						// Ignore properties with '/' (slash) in the name. These are only meant for use in the inspector.
						continue;
					}
					StringName property_name = F.name;
					Dictionary d2;
					d2["type"] = get_property_info_type_name(F);
					d2["name"] = String(property_name);
					StringName setter = ClassDB::get_property_setter(class_name, F.name);
					if (!(setter == "")) {
						d2["setter"] = setter;
					}
					StringName getter = ClassDB::get_property_getter(class_name, F.name);
					if (!(getter == "")) {
						d2["getter"] = getter;
					}
					int index = ClassDB::get_property_index(class_name, F.name);
					if (index != -1) {
						d2["index"] = index;
					}

					if (p_include_docs) {
						for (const DocData::PropertyDoc &property_doc : class_doc->properties) {
							if (property_doc.name == property_name) {
								d2["description"] = fix_doc_description(property_doc.description);
								break;
							}
						}
					}

					properties.push_back(d2);
				}

				if (properties.size()) {
					d["properties"] = properties;
				}
			}

			if (p_include_docs && class_doc != nullptr) {
				d["brief_description"] = fix_doc_description(class_doc->brief_description);
				d["description"] = fix_doc_description(class_doc->description);
			}

			classes.push_back(d);
		}

		api_dump["classes"] = classes;
	}

	{
		// singletons

		Array singletons;
		List<Engine::Singleton> singleton_list;
		Engine::get_singleton()->get_singletons(&singleton_list);

		for (const Engine::Singleton &s : singleton_list) {
			Dictionary d;
			d["name"] = s.name;
			if (s.class_name != StringName()) {
				d["type"] = String(s.class_name);
			} else {
				d["type"] = String(s.ptr->get_class());
			}
			singletons.push_back(d);
		}

		if (singletons.size()) {
			api_dump["singletons"] = singletons;
		}
	}

	{
		Array native_structures;

		List<StringName> native_structs;
		ClassDB::get_native_struct_list(&native_structs);
		native_structs.sort_custom<StringName::AlphCompare>();

		for (const StringName &E : native_structs) {
			String code = ClassDB::get_native_struct_code(E);

			Dictionary d;
			d["name"] = String(E);
			d["format"] = code;

			native_structures.push_back(d);
		}

		api_dump["native_structures"] = native_structures;
	}

	return api_dump;
}

void GDExtensionAPIDump::generate_extension_json_file(const String &p_path, bool p_include_docs) {
	Dictionary api = generate_extension_api(p_include_docs);
	Ref<JSON> json;
	json.instantiate();

	String text = json->stringify(api, "\t", false) + "\n";
	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_MSG(fa.is_null(), vformat("Cannot open file '%s' for writing.", p_path));
	fa->store_string(text);
}

static bool compare_value(const String &p_path, const String &p_field, const Variant &p_old_value, const Variant &p_new_value, bool p_allow_name_change) {
	bool failed = false;
	String path = p_path + "/" + p_field;
	if (p_old_value.get_type() == Variant::ARRAY && p_new_value.get_type() == Variant::ARRAY) {
		Array old_array = p_old_value;
		Array new_array = p_new_value;
		if (!compare_value(path, "size", old_array.size(), new_array.size(), p_allow_name_change)) {
			failed = true;
		}
		for (int i = 0; i < old_array.size() && i < new_array.size(); i++) {
			if (!compare_value(path, itos(i), old_array[i], new_array[i], p_allow_name_change)) {
				failed = true;
			}
		}
	} else if (p_old_value.get_type() == Variant::DICTIONARY && p_new_value.get_type() == Variant::DICTIONARY) {
		Dictionary old_dict = p_old_value;
		Dictionary new_dict = p_new_value;
		for (const Variant &key : old_dict.keys()) {
			if (!new_dict.has(key)) {
				failed = true;
				print_error(vformat("Validate extension JSON: Error: Field '%s': %s was removed.", p_path, key));
				continue;
			}
			if (p_allow_name_change && key == "name") {
				continue;
			}
			if (!compare_value(path, key, old_dict[key], new_dict[key], p_allow_name_change)) {
				failed = true;
			}
		}
		for (const Variant &key : old_dict.keys()) {
			if (!old_dict.has(key)) {
				failed = true;
				print_error(vformat("Validate extension JSON: Error: Field '%s': %s was added with value %s.", p_path, key, new_dict[key]));
			}
		}
	} else {
		bool equal = Variant::evaluate(Variant::OP_EQUAL, p_old_value, p_new_value);
		if (!equal) {
			print_error(vformat("Validate extension JSON: Error: Field '%s': %s changed value in new API, from %s to %s.", p_path, p_field, p_old_value.get_construct_string(), p_new_value.get_construct_string()));
			return false;
		}
	}
	return !failed;
}

static bool compare_dict_array(const Dictionary &p_old_api, const Dictionary &p_new_api, const String &p_base_array, const String &p_name_field, const Vector<String> &p_fields_to_compare, bool p_compare_hashes, const String &p_outer_class = String(), bool p_compare_operators = false, bool p_compare_enum_value = false) {
	String base_array = p_outer_class + p_base_array;
	if (!p_old_api.has(p_base_array)) {
		return true; // May just not have this array and its still good. Probably added recently.
	}
	bool failed = false;
	ERR_FAIL_COND_V_MSG(!p_new_api.has(p_base_array), false, vformat("New API lacks base array: %s", p_base_array));
	Array new_api = p_new_api[p_base_array];
	HashMap<String, Dictionary> new_api_assoc;

	for (const Variant &var : new_api) {
		Dictionary elem = var;
		ERR_FAIL_COND_V_MSG(!elem.has(p_name_field), false, vformat("Validate extension JSON: Element of base_array '%s' is missing field '%s'. This is a bug.", base_array, p_name_field));
		String name = elem[p_name_field];
		if (name.is_valid_float()) {
			name = name.trim_suffix(".0"); // Make "integers" stringified as integers.
		}
		if (p_compare_operators && elem.has("right_type")) {
			name += " " + String(elem["right_type"]);
		}
		new_api_assoc.insert(name, elem);
	}

	Array old_api = p_old_api[p_base_array];
	for (const Variant &var : old_api) {
		Dictionary old_elem = var;
		if (!old_elem.has(p_name_field)) {
			failed = true;
			print_error(vformat("Validate extension JSON: JSON file: element of base array '%s' is missing the field: '%s'.", base_array, p_name_field));
			continue;
		}
		String name = old_elem[p_name_field];
		if (name.is_valid_float()) {
			name = name.trim_suffix(".0"); // Make "integers" stringified as integers.
		}
		if (p_compare_operators && old_elem.has("right_type")) {
			name += " " + String(old_elem["right_type"]);
		}
		if (!new_api_assoc.has(name)) {
			failed = true;
			print_error(vformat("Validate extension JSON: API was removed: %s/%s", base_array, name));
			continue;
		}

		Dictionary new_elem = new_api_assoc[name];

		for (int j = 0; j < p_fields_to_compare.size(); j++) {
			String field = p_fields_to_compare[j];
			bool optional = field.begins_with("*");
			if (optional) {
				// This is an optional field, but if exists it has to exist in both.
				field = field.substr(1, field.length());
			}

			bool added = field.begins_with("+");
			if (added) {
				// Meaning this field must either exist or contents may not exist.
				field = field.substr(1, field.length());
			}

			bool enum_values = field.begins_with("$");
			if (enum_values) {
				// Meaning this field is a list of enum values.
				field = field.substr(1, field.length());
			}

			bool allow_name_change = field.begins_with("@");
			if (allow_name_change) {
				// Meaning that when structurally comparing the old and new value, the dictionary entry 'name' may change.
				field = field.substr(1, field.length());
			}

			Variant old_value;

			if (!old_elem.has(field)) {
				if (optional) {
					if (new_elem.has(field)) {
						failed = true;
						print_error(vformat("Validate extension JSON: JSON file: Field was added in a way that breaks compatibility '%s/%s': %s", base_array, name, field));
					}
				} else if (added && new_elem.has(field)) {
					// Should be ok, field now exists, should not be verified in prior versions where it does not.
				} else {
					failed = true;
					print_error(vformat("Validate extension JSON: JSON file: Missing field in '%s/%s': %s", base_array, name, field));
				}
				continue;
			} else {
				old_value = old_elem[field];
			}

			if (!new_elem.has(field)) {
				failed = true;
				ERR_PRINT(vformat("Validate extension JSON: Missing field in current API '%s/%s': %s. This is a bug.", base_array, name, field));
				continue;
			}

			Variant new_value = new_elem[field];

			if (p_compare_enum_value && name.ends_with("_MAX")) {
				if (static_cast<int64_t>(new_value) > static_cast<int64_t>(old_value)) {
					// Ignore the _MAX value of an enum increasing.
					continue;
				}
			}
			if (enum_values) {
				if (!compare_dict_array(old_elem, new_elem, field, "name", { "value" }, false, base_array + "/" + name + "/", false, true)) {
					failed = true;
				}
			} else if (!compare_value(base_array + "/" + name, field, old_value, new_value, allow_name_change)) {
				failed = true;
			}
		}

		if (p_compare_hashes) {
			if (!old_elem.has("hash")) {
				if (old_elem.has("is_virtual") && bool(old_elem["is_virtual"]) && !new_elem.has("hash")) {
					continue; // No hash for virtual methods, go on.
				}

				failed = true;
				print_error(vformat("Validate extension JSON: JSON file: element of base array '%s' is missing the field: 'hash'.", base_array));
				continue;
			}

			uint64_t old_hash = old_elem["hash"];

			if (!new_elem.has("hash")) {
				failed = true;
				print_error(vformat("Validate extension JSON: Error: Field '%s' is missing the field: 'hash'.", base_array));
				continue;
			}

			uint64_t new_hash = new_elem["hash"];
			bool hash_found = false;
			if (old_hash == new_hash) {
				hash_found = true;
			} else if (new_elem.has("hash_compatibility")) {
				Array compatibility = new_elem["hash_compatibility"];
				for (int j = 0; j < compatibility.size(); j++) {
					new_hash = compatibility[j];
					if (new_hash == old_hash) {
						hash_found = true;
						break;
					}
				}
			}

			if (!hash_found) {
				failed = true;
				print_error(vformat("Validate extension JSON: Error: Hash changed for '%s/%s', from %08X to %08X. This means that the function has changed and no compatibility function was provided.", base_array, name, old_hash, new_hash));
				continue;
			}
		}
	}

	return !failed;
}

static bool compare_sub_dict_array(HashSet<String> &r_removed_classes_registered, const String &p_outer, const String &p_outer_name, const Dictionary &p_old_api, const Dictionary &p_new_api, const String &p_base_array, const String &p_name_field, const Vector<String> &p_fields_to_compare, bool p_compare_hashes, bool p_compare_operators = false) {
	if (!p_old_api.has(p_outer)) {
		return true; // May just not have this array and its still good. Probably added recently or optional.
	}
	bool failed = false;
	ERR_FAIL_COND_V_MSG(!p_new_api.has(p_outer), false, vformat("New API lacks base array: %s", p_outer));
	Array new_api = p_new_api[p_outer];
	HashMap<String, Dictionary> new_api_assoc;

	for (const Variant &var : new_api) {
		Dictionary elem = var;
		ERR_FAIL_COND_V_MSG(!elem.has(p_outer_name), false, vformat("Validate extension JSON: Element of base_array '%s' is missing field '%s'. This is a bug.", p_outer, p_outer_name));
		new_api_assoc.insert(elem[p_outer_name], elem);
	}

	Array old_api = p_old_api[p_outer];

	for (const Variant &var : old_api) {
		Dictionary old_elem = var;
		if (!old_elem.has(p_outer_name)) {
			failed = true;
			print_error(vformat("Validate extension JSON: JSON file: element of base array '%s' is missing the field: '%s'.", p_outer, p_outer_name));
			continue;
		}
		String name = old_elem[p_outer_name];
		if (!new_api_assoc.has(name)) {
			failed = true;
			if (!r_removed_classes_registered.has(name)) {
				print_error(vformat("Validate extension JSON: API was removed: %s/%s", p_outer, name));
				r_removed_classes_registered.insert(name);
			}
			continue;
		}

		Dictionary new_elem = new_api_assoc[name];

		if (!compare_dict_array(old_elem, new_elem, p_base_array, p_name_field, p_fields_to_compare, p_compare_hashes, p_outer + "/" + name + "/", p_compare_operators)) {
			failed = true;
		}
	}

	return !failed;
}

Error GDExtensionAPIDump::validate_extension_json_file(const String &p_path) {
	Error error;
	String text = FileAccess::get_file_as_string(p_path, &error);
	if (error != OK) {
		ERR_PRINT(vformat("Validate extension JSON: Could not open file '%s'.", p_path));
		return error;
	}

	Ref<JSON> json;
	json.instantiate();
	error = json->parse(text);
	if (error != OK) {
		ERR_PRINT(vformat("Validate extension JSON: Error parsing '%s' at line %d: %s", p_path, json->get_error_line(), json->get_error_message()));
		return error;
	}

	Dictionary old_api = json->get_data();
	Dictionary new_api = generate_extension_api();

	{ // Validate header:
		Dictionary header = old_api["header"];
		ERR_FAIL_COND_V(!header.has("version_major"), ERR_INVALID_DATA);
		ERR_FAIL_COND_V(!header.has("version_minor"), ERR_INVALID_DATA);
		int major = header["version_major"];
		int minor = header["version_minor"];

		ERR_FAIL_COND_V_MSG(major != VERSION_MAJOR, ERR_INVALID_DATA, vformat("JSON API dump is for a different engine version (%d) than this one (%d)", major, VERSION_MAJOR));
		ERR_FAIL_COND_V_MSG(minor > VERSION_MINOR, ERR_INVALID_DATA, vformat("JSON API dump is for a newer version of the engine: %d.%d", major, minor));
	}

	bool failed = false;

	HashSet<String> removed_classes_registered;

	if (!compare_dict_array(old_api, new_api, "global_constants", "name", Vector<String>({ "value", "is_bitfield" }), false)) {
		failed = true;
	}

	if (!compare_dict_array(old_api, new_api, "global_enums", "name", Vector<String>({ "$values", "is_bitfield" }), false)) {
		failed = true;
	}

	if (!compare_dict_array(old_api, new_api, "utility_functions", "name", Vector<String>({ "category", "is_vararg", "*return_type", "*@arguments" }), true)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "builtin_classes", "name", old_api, new_api, "members", "name", { "type" }, false)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "builtin_classes", "name", old_api, new_api, "constants", "name", { "type", "value" }, false)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "builtin_classes", "name", old_api, new_api, "operators", "name", { "return_type" }, false, true)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "builtin_classes", "name", old_api, new_api, "methods", "name", { "is_vararg", "is_static", "is_const", "*return_type", "*@arguments" }, true)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "builtin_classes", "name", old_api, new_api, "constructors", "index", { "*@arguments" }, false)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "classes", "name", old_api, new_api, "constants", "name", { "value" }, false)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "classes", "name", old_api, new_api, "enums", "name", { "is_bitfield", "$values" }, false)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "classes", "name", old_api, new_api, "methods", "name", { "is_virtual", "is_vararg", "is_static", "is_const", "*return_value", "*@arguments" }, true)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "classes", "name", old_api, new_api, "signals", "name", { "*@arguments" }, false)) {
		failed = true;
	}

	if (!compare_sub_dict_array(removed_classes_registered, "classes", "name", old_api, new_api, "properties", "name", { "type", "*setter", "*getter", "*index" }, false)) {
		failed = true;
	}

	if (!compare_dict_array(old_api, new_api, "singletons", "name", Vector<String>({ "type" }), false)) {
		failed = true;
	}

	if (!compare_dict_array(old_api, new_api, "native_structures", "name", Vector<String>({ "format" }), false)) {
		failed = true;
	}

	if (failed) {
		return ERR_INVALID_DATA;
	} else {
		return OK;
	}
}

#endif // TOOLS_ENABLED
