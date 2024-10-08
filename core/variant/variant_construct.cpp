/**************************************************************************/
/*  variant_construct.cpp                                                 */
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

#include "variant_construct.h"

struct VariantConstructData {
	void (*construct)(Variant &r_base, const Variant **p_args, Callable::CallError &r_error) = nullptr;
	Variant::ValidatedConstructor validated_construct = nullptr;
	Variant::PTRConstructor ptr_construct = nullptr;
	Variant::Type (*get_argument_type)(int) = nullptr;
	int argument_count = 0;
	Vector<String> arg_names;
};

static LocalVector<VariantConstructData> construct_data[Variant::VARIANT_MAX];

template <typename T>
static void add_constructor(const Vector<String> &arg_names) {
	ERR_FAIL_COND_MSG(arg_names.size() != T::get_argument_count(), "Argument names size mismatch for " + Variant::get_type_name(T::get_base_type()) + ".");

	VariantConstructData cd;
	cd.construct = T::construct;
	cd.validated_construct = T::validated_construct;
	cd.ptr_construct = T::ptr_construct;
	cd.get_argument_type = T::get_argument_type;
	cd.argument_count = T::get_argument_count();
	cd.arg_names = arg_names;
	construct_data[T::get_base_type()].push_back(cd);
}

void Variant::_register_variant_constructors() {
	add_constructor<VariantConstructNoArgsNil>(sarray());
	add_constructor<VariantConstructorNil>(sarray("from"));

	add_constructor<VariantConstructNoArgs<bool>>(sarray());
	add_constructor<VariantConstructor<bool, bool>>(sarray("from"));
	add_constructor<VariantConstructor<bool, int64_t>>(sarray("from"));
	add_constructor<VariantConstructor<bool, double>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<int64_t>>(sarray());
	add_constructor<VariantConstructor<int64_t, int64_t>>(sarray("from"));
	add_constructor<VariantConstructor<int64_t, double>>(sarray("from"));
	add_constructor<VariantConstructor<int64_t, bool>>(sarray("from"));
	add_constructor<VariantConstructorFromString<int64_t>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<double>>(sarray());
	add_constructor<VariantConstructor<double, double>>(sarray("from"));
	add_constructor<VariantConstructor<double, int64_t>>(sarray("from"));
	add_constructor<VariantConstructor<double, bool>>(sarray("from"));
	add_constructor<VariantConstructorFromString<double>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<String>>(sarray());
	add_constructor<VariantConstructor<String, String>>(sarray("from"));
	add_constructor<VariantConstructor<String, StringName>>(sarray("from"));
	add_constructor<VariantConstructor<String, NodePath>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<Vector2>>(sarray());
	add_constructor<VariantConstructor<Vector2, Vector2>>(sarray("from"));
	add_constructor<VariantConstructor<Vector2, Vector2i>>(sarray("from"));
	add_constructor<VariantConstructor<Vector2, double, double>>(sarray("x", "y"));

	add_constructor<VariantConstructNoArgs<Vector2i>>(sarray());
	add_constructor<VariantConstructor<Vector2i, Vector2i>>(sarray("from"));
	add_constructor<VariantConstructor<Vector2i, Vector2>>(sarray("from"));
	add_constructor<VariantConstructor<Vector2i, int64_t, int64_t>>(sarray("x", "y"));

	add_constructor<VariantConstructNoArgs<Rect2>>(sarray());
	add_constructor<VariantConstructor<Rect2, Rect2>>(sarray("from"));
	add_constructor<VariantConstructor<Rect2, Rect2i>>(sarray("from"));
	add_constructor<VariantConstructor<Rect2, Vector2, Vector2>>(sarray("position", "size"));
	add_constructor<VariantConstructor<Rect2, double, double, double, double>>(sarray("x", "y", "width", "height"));

	add_constructor<VariantConstructNoArgs<Rect2i>>(sarray());
	add_constructor<VariantConstructor<Rect2i, Rect2i>>(sarray("from"));
	add_constructor<VariantConstructor<Rect2i, Rect2>>(sarray("from"));
	add_constructor<VariantConstructor<Rect2i, Vector2i, Vector2i>>(sarray("position", "size"));
	add_constructor<VariantConstructor<Rect2i, int64_t, int64_t, int64_t, int64_t>>(sarray("x", "y", "width", "height"));

	add_constructor<VariantConstructNoArgs<Vector3>>(sarray());
	add_constructor<VariantConstructor<Vector3, Vector3>>(sarray("from"));
	add_constructor<VariantConstructor<Vector3, Vector3i>>(sarray("from"));
	add_constructor<VariantConstructor<Vector3, double, double, double>>(sarray("x", "y", "z"));

	add_constructor<VariantConstructNoArgs<Vector3i>>(sarray());
	add_constructor<VariantConstructor<Vector3i, Vector3i>>(sarray("from"));
	add_constructor<VariantConstructor<Vector3i, Vector3>>(sarray("from"));
	add_constructor<VariantConstructor<Vector3i, int64_t, int64_t, int64_t>>(sarray("x", "y", "z"));

	add_constructor<VariantConstructNoArgs<Vector4>>(sarray());
	add_constructor<VariantConstructor<Vector4, Vector4>>(sarray("from"));
	add_constructor<VariantConstructor<Vector4, Vector4i>>(sarray("from"));
	add_constructor<VariantConstructor<Vector4, double, double, double, double>>(sarray("x", "y", "z", "w"));

	add_constructor<VariantConstructNoArgs<Vector4i>>(sarray());
	add_constructor<VariantConstructor<Vector4i, Vector4i>>(sarray("from"));
	add_constructor<VariantConstructor<Vector4i, Vector4>>(sarray("from"));
	add_constructor<VariantConstructor<Vector4i, int64_t, int64_t, int64_t, int64_t>>(sarray("x", "y", "z", "w"));

	add_constructor<VariantConstructNoArgs<Transform2D>>(sarray());
	add_constructor<VariantConstructor<Transform2D, Transform2D>>(sarray("from"));
	add_constructor<VariantConstructor<Transform2D, float, Vector2>>(sarray("rotation", "position"));
	add_constructor<VariantConstructor<Transform2D, float, Size2, float, Vector2>>(sarray("rotation", "scale", "skew", "position"));
	add_constructor<VariantConstructor<Transform2D, Vector2, Vector2, Vector2>>(sarray("x_axis", "y_axis", "origin"));

	add_constructor<VariantConstructNoArgs<Plane>>(sarray());
	add_constructor<VariantConstructor<Plane, Plane>>(sarray("from"));
	add_constructor<VariantConstructor<Plane, Vector3>>(sarray("normal"));
	add_constructor<VariantConstructor<Plane, Vector3, double>>(sarray("normal", "d"));
	add_constructor<VariantConstructor<Plane, Vector3, Vector3>>(sarray("normal", "point"));
	add_constructor<VariantConstructor<Plane, Vector3, Vector3, Vector3>>(sarray("point1", "point2", "point3"));
	add_constructor<VariantConstructor<Plane, double, double, double, double>>(sarray("a", "b", "c", "d"));

	add_constructor<VariantConstructNoArgs<Quaternion>>(sarray());
	add_constructor<VariantConstructor<Quaternion, Quaternion>>(sarray("from"));
	add_constructor<VariantConstructor<Quaternion, Basis>>(sarray("from"));
	add_constructor<VariantConstructor<Quaternion, Vector3, double>>(sarray("axis", "angle"));
	add_constructor<VariantConstructor<Quaternion, Vector3, Vector3>>(sarray("arc_from", "arc_to"));
	add_constructor<VariantConstructor<Quaternion, double, double, double, double>>(sarray("x", "y", "z", "w"));

	add_constructor<VariantConstructNoArgs<::AABB>>(sarray());
	add_constructor<VariantConstructor<::AABB, ::AABB>>(sarray("from"));
	add_constructor<VariantConstructor<::AABB, Vector3, Vector3>>(sarray("position", "size"));

	add_constructor<VariantConstructNoArgs<Basis>>(sarray());
	add_constructor<VariantConstructor<Basis, Basis>>(sarray("from"));
	add_constructor<VariantConstructor<Basis, Quaternion>>(sarray("from"));
	add_constructor<VariantConstructor<Basis, Vector3, double>>(sarray("axis", "angle"));
	add_constructor<VariantConstructor<Basis, Vector3, Vector3, Vector3>>(sarray("x_axis", "y_axis", "z_axis"));

	add_constructor<VariantConstructNoArgs<Transform3D>>(sarray());
	add_constructor<VariantConstructor<Transform3D, Transform3D>>(sarray("from"));
	add_constructor<VariantConstructor<Transform3D, Basis, Vector3>>(sarray("basis", "origin"));
	add_constructor<VariantConstructor<Transform3D, Vector3, Vector3, Vector3, Vector3>>(sarray("x_axis", "y_axis", "z_axis", "origin"));
	add_constructor<VariantConstructor<Transform3D, Projection>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<Projection>>(sarray());
	add_constructor<VariantConstructor<Projection, Projection>>(sarray("from"));
	add_constructor<VariantConstructor<Projection, Transform3D>>(sarray("from"));
	add_constructor<VariantConstructor<Projection, Vector4, Vector4, Vector4, Vector4>>(sarray("x_axis", "y_axis", "z_axis", "w_axis"));

	add_constructor<VariantConstructNoArgs<Color>>(sarray());
	add_constructor<VariantConstructor<Color, Color>>(sarray("from"));
	add_constructor<VariantConstructor<Color, Color, double>>(sarray("from", "alpha"));
	add_constructor<VariantConstructor<Color, double, double, double>>(sarray("r", "g", "b"));
	add_constructor<VariantConstructor<Color, double, double, double, double>>(sarray("r", "g", "b", "a"));
	add_constructor<VariantConstructor<Color, String>>(sarray("code"));
	add_constructor<VariantConstructor<Color, String, double>>(sarray("code", "alpha"));

	add_constructor<VariantConstructNoArgs<StringName>>(sarray());
	add_constructor<VariantConstructor<StringName, StringName>>(sarray("from"));
	add_constructor<VariantConstructor<StringName, String>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<NodePath>>(sarray());
	add_constructor<VariantConstructor<NodePath, NodePath>>(sarray("from"));
	add_constructor<VariantConstructor<NodePath, String>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<::RID>>(sarray());
	add_constructor<VariantConstructor<::RID, ::RID>>(sarray("from"));

	add_constructor<VariantConstructNoArgsObject>(sarray());
	add_constructor<VariantConstructorObject>(sarray("from"));
	add_constructor<VariantConstructorNilObject>(sarray("from"));

	add_constructor<VariantConstructNoArgs<Callable>>(sarray());
	add_constructor<VariantConstructor<Callable, Callable>>(sarray("from"));
	add_constructor<VariantConstructorCallableArgs>(sarray("object", "method"));

	add_constructor<VariantConstructNoArgs<Signal>>(sarray());
	add_constructor<VariantConstructor<Signal, Signal>>(sarray("from"));
	add_constructor<VariantConstructorSignalArgs>(sarray("object", "signal"));

	add_constructor<VariantConstructNoArgs<Dictionary>>(sarray());
	add_constructor<VariantConstructor<Dictionary, Dictionary>>(sarray("from"));
	add_constructor<VariantConstructorTypedDictionary>(sarray("base", "key_type", "key_class_name", "key_script", "value_type", "value_class_name", "value_script"));

	add_constructor<VariantConstructNoArgs<Array>>(sarray());
	add_constructor<VariantConstructor<Array, Array>>(sarray("from"));
	add_constructor<VariantConstructorTypedArray>(sarray("base", "type", "class_name", "script"));
	add_constructor<VariantConstructorToArray<PackedByteArray>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedInt32Array>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedInt64Array>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedFloat32Array>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedFloat64Array>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedStringArray>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedVector2Array>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedVector3Array>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedColorArray>>(sarray("from"));
	add_constructor<VariantConstructorToArray<PackedVector4Array>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedByteArray>>(sarray());
	add_constructor<VariantConstructor<PackedByteArray, PackedByteArray>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedByteArray>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedInt32Array>>(sarray());
	add_constructor<VariantConstructor<PackedInt32Array, PackedInt32Array>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedInt32Array>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedInt64Array>>(sarray());
	add_constructor<VariantConstructor<PackedInt64Array, PackedInt64Array>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedInt64Array>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedFloat32Array>>(sarray());
	add_constructor<VariantConstructor<PackedFloat32Array, PackedFloat32Array>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedFloat32Array>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedFloat64Array>>(sarray());
	add_constructor<VariantConstructor<PackedFloat64Array, PackedFloat64Array>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedFloat64Array>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedStringArray>>(sarray());
	add_constructor<VariantConstructor<PackedStringArray, PackedStringArray>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedStringArray>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedVector2Array>>(sarray());
	add_constructor<VariantConstructor<PackedVector2Array, PackedVector2Array>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedVector2Array>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedVector3Array>>(sarray());
	add_constructor<VariantConstructor<PackedVector3Array, PackedVector3Array>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedVector3Array>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedColorArray>>(sarray());
	add_constructor<VariantConstructor<PackedColorArray, PackedColorArray>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedColorArray>>(sarray("from"));

	add_constructor<VariantConstructNoArgs<PackedVector4Array>>(sarray());
	add_constructor<VariantConstructor<PackedVector4Array, PackedVector4Array>>(sarray("from"));
	add_constructor<VariantConstructorFromArray<PackedVector4Array>>(sarray("from"));
}

void Variant::_unregister_variant_constructors() {
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		construct_data[i].clear();
	}
}

void Variant::construct(Variant::Type p_type, Variant &base, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);
	uint32_t s = construct_data[p_type].size();
	for (uint32_t i = 0; i < s; i++) {
		int argc = construct_data[p_type][i].argument_count;
		if (argc != p_argcount) {
			continue;
		}
		bool args_match = true;
		for (int j = 0; j < argc; j++) {
			if (!Variant::can_convert_strict(p_args[j]->get_type(), construct_data[p_type][i].get_argument_type(j))) {
				args_match = false;
				break;
			}
		}

		if (!args_match) {
			continue;
		}

		construct_data[p_type][i].construct(base, p_args, r_error);
		return;
	}

	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
}

int Variant::get_constructor_count(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);
	return construct_data[p_type].size();
}

Variant::ValidatedConstructor Variant::get_validated_constructor(Variant::Type p_type, int p_constructor) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	ERR_FAIL_INDEX_V(p_constructor, (int)construct_data[p_type].size(), nullptr);
	return construct_data[p_type][p_constructor].validated_construct;
}

Variant::PTRConstructor Variant::get_ptr_constructor(Variant::Type p_type, int p_constructor) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	ERR_FAIL_INDEX_V(p_constructor, (int)construct_data[p_type].size(), nullptr);
	return construct_data[p_type][p_constructor].ptr_construct;
}

int Variant::get_constructor_argument_count(Variant::Type p_type, int p_constructor) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);
	ERR_FAIL_INDEX_V(p_constructor, (int)construct_data[p_type].size(), -1);
	return construct_data[p_type][p_constructor].argument_count;
}

Variant::Type Variant::get_constructor_argument_type(Variant::Type p_type, int p_constructor, int p_argument) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::VARIANT_MAX);
	ERR_FAIL_INDEX_V(p_constructor, (int)construct_data[p_type].size(), Variant::VARIANT_MAX);
	return construct_data[p_type][p_constructor].get_argument_type(p_argument);
}

String Variant::get_constructor_argument_name(Variant::Type p_type, int p_constructor, int p_argument) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, String());
	ERR_FAIL_INDEX_V(p_constructor, (int)construct_data[p_type].size(), String());
	return construct_data[p_type][p_constructor].arg_names[p_argument];
}

void Variant::get_constructor_list(Type p_type, List<MethodInfo> *r_list) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);

	MethodInfo mi;
	mi.return_val.type = p_type;
	mi.name = get_type_name(p_type);

	for (int i = 0; i < get_constructor_count(p_type); i++) {
		int ac = get_constructor_argument_count(p_type, i);
		mi.arguments.clear();
		for (int j = 0; j < ac; j++) {
			PropertyInfo arg;
			arg.name = get_constructor_argument_name(p_type, i, j);
			arg.type = get_constructor_argument_type(p_type, i, j);
			mi.arguments.push_back(arg);
		}
		r_list->push_back(mi);
	}
}
