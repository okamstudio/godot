/**************************************************************************/
/*  gltf_object_model_property.h                                          */
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

#include "core/math/expression.h"
#include "core/variant/typed_array.h"
#include "gltf_accessor.h"

// Object model: https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/ObjectModel.adoc
// KHR_animation_pointer: https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_animation_pointer

class GLTFObjectModelProperty : public RefCounted {
	GDCLASS(GLTFObjectModelProperty, RefCounted);

public:
	enum GLTFObjectModelType {
		GLTF_OBJECT_MODEL_TYPE_UNKNOWN,
		GLTF_OBJECT_MODEL_TYPE_BOOL,
		GLTF_OBJECT_MODEL_TYPE_FLOAT,
		GLTF_OBJECT_MODEL_TYPE_FLOAT_ARRAY,
		GLTF_OBJECT_MODEL_TYPE_FLOAT2,
		GLTF_OBJECT_MODEL_TYPE_FLOAT3,
		GLTF_OBJECT_MODEL_TYPE_FLOAT4,
		GLTF_OBJECT_MODEL_TYPE_FLOAT2X2,
		GLTF_OBJECT_MODEL_TYPE_FLOAT3X3,
		GLTF_OBJECT_MODEL_TYPE_FLOAT4X4,
		GLTF_OBJECT_MODEL_TYPE_INT,
	};

private:
	Ref<Expression> gltf_to_godot_expr;
	Ref<Expression> godot_to_gltf_expr;
	TypedArray<NodePath> node_paths;
	GLTFObjectModelType object_model_type = GLTF_OBJECT_MODEL_TYPE_UNKNOWN;
	Vector<PackedStringArray> json_pointers;
	Variant::Type variant_type = Variant::NIL;

protected:
	static void _bind_methods();

public:
	void append_node_path(const NodePath &p_node_path);
	void append_path_to_property(const NodePath &p_node_path, const StringName &p_prop_name);

	GLTFAccessor::GLTFAccessorType get_accessor_type() const;

	Ref<Expression> get_gltf_to_godot_expression() const;
	void set_gltf_to_godot_expression(Ref<Expression> p_gltf_to_godot_expr);

	Ref<Expression> get_godot_to_gltf_expression() const;
	void set_godot_to_gltf_expression(Ref<Expression> p_godot_to_gltf_expr);

	TypedArray<NodePath> get_node_paths() const;
	bool has_node_paths() const;
	void set_node_paths(TypedArray<NodePath> p_node_paths);

	GLTFObjectModelType get_object_model_type() const;
	void set_object_model_type(GLTFObjectModelType p_type);

	Vector<PackedStringArray> get_json_pointers() const;
	bool has_json_pointers() const;
	void set_json_pointers(const Vector<PackedStringArray> &p_json_pointers);

	TypedArray<PackedStringArray> get_json_pointers_bind() const;
	void set_json_pointers_bind(const TypedArray<PackedStringArray> &p_json_pointers);

	Variant::Type get_variant_type() const;
	void set_variant_type(Variant::Type p_variant_type);

	void set_types(Variant::Type p_variant_type, GLTFObjectModelType p_obj_model_type);
};

VARIANT_ENUM_CAST(GLTFObjectModelProperty::GLTFObjectModelType);
