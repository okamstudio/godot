/**************************************************************************/
/*  variant.cpp                                                           */
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

#include "variant.h"

#include "core/debugger/engine_debugger.h"
#include "core/io/json.h"
#include "core/io/resource.h"
#include "core/math/math_funcs.h"
#include "core/variant/variant_parser.h"

PagedAllocator<Variant::Pools::BucketSmall, true> Variant::Pools::_bucket_small;
PagedAllocator<Variant::Pools::BucketMedium, true> Variant::Pools::_bucket_medium;
PagedAllocator<Variant::Pools::BucketLarge, true> Variant::Pools::_bucket_large;

String Variant::get_type_name(Variant::Type p_type) {
	switch (p_type) {
		case NIL: {
			return "Nil";
		}

		// Atomic types.
		case BOOL: {
			return "bool";
		}
		case INT: {
			return "int";
		}
		case FLOAT: {
			return "float";
		}
		case STRING: {
			return "String";
		}

		// Math types.
		case VECTOR2: {
			return "Vector2";
		}
		case VECTOR2I: {
			return "Vector2i";
		}
		case RECT2: {
			return "Rect2";
		}
		case RECT2I: {
			return "Rect2i";
		}
		case TRANSFORM2D: {
			return "Transform2D";
		}
		case VECTOR3: {
			return "Vector3";
		}
		case VECTOR3I: {
			return "Vector3i";
		}
		case VECTOR4: {
			return "Vector4";
		}
		case VECTOR4I: {
			return "Vector4i";
		}
		case PLANE: {
			return "Plane";
		}
		case AABB: {
			return "AABB";
		}
		case QUATERNION: {
			return "Quaternion";
		}
		case BASIS: {
			return "Basis";
		}
		case TRANSFORM3D: {
			return "Transform3D";
		}
		case PROJECTION: {
			return "Projection";
		}

		// Miscellaneous types.
		case COLOR: {
			return "Color";
		}
		case RID: {
			return "RID";
		}
		case OBJECT: {
			return "Object";
		}
		case CALLABLE: {
			return "Callable";
		}
		case SIGNAL: {
			return "Signal";
		}
		case STRING_NAME: {
			return "StringName";
		}
		case NODE_PATH: {
			return "NodePath";
		}
		case DICTIONARY: {
			return "Dictionary";
		}
		case ARRAY: {
			return "Array";
		}

		// Arrays.
		case PACKED_BYTE_ARRAY: {
			return "PackedByteArray";
		}
		case PACKED_INT32_ARRAY: {
			return "PackedInt32Array";
		}
		case PACKED_INT64_ARRAY: {
			return "PackedInt64Array";
		}
		case PACKED_FLOAT32_ARRAY: {
			return "PackedFloat32Array";
		}
		case PACKED_FLOAT64_ARRAY: {
			return "PackedFloat64Array";
		}
		case PACKED_STRING_ARRAY: {
			return "PackedStringArray";
		}
		case PACKED_VECTOR2_ARRAY: {
			return "PackedVector2Array";
		}
		case PACKED_VECTOR3_ARRAY: {
			return "PackedVector3Array";
		}
		case PACKED_COLOR_ARRAY: {
			return "PackedColorArray";
		}
		case PACKED_VECTOR4_ARRAY: {
			return "PackedVector4Array";
		}
		default: {
		}
	}

	return "";
}

Variant::Type Variant::get_type_by_name(const String &p_type_name) {
	static HashMap<String, Type> type_names;
	if (unlikely(type_names.is_empty())) {
		for (int i = 0; i < VARIANT_MAX; i++) {
			type_names[get_type_name((Type)i)] = (Type)i;
		}
	}

	const Type *ptr = type_names.getptr(p_type_name);
	return (ptr == nullptr) ? VARIANT_MAX : *ptr;
}

bool Variant::can_convert(Variant::Type p_type_from, Variant::Type p_type_to) {
	if (p_type_from == p_type_to) {
		return true;
	}
	if (p_type_to == NIL) { //nil can convert to anything
		return true;
	}

	if (p_type_from == NIL) {
		return (p_type_to == OBJECT);
	}

	const Type *valid_types = nullptr;
	const Type *invalid_types = nullptr;

	switch (p_type_to) {
		case BOOL: {
			static const Type valid[] = {
				INT,
				FLOAT,
				STRING,
				NIL,
			};

			valid_types = valid;
		} break;
		case INT: {
			static const Type valid[] = {
				BOOL,
				FLOAT,
				STRING,
				NIL,
			};

			valid_types = valid;

		} break;
		case FLOAT: {
			static const Type valid[] = {
				BOOL,
				INT,
				STRING,
				NIL,
			};

			valid_types = valid;

		} break;
		case STRING: {
			static const Type invalid[] = {
				OBJECT,
				NIL
			};

			invalid_types = invalid;
		} break;
		case VECTOR2: {
			static const Type valid[] = {
				VECTOR2I,
				NIL,
			};

			valid_types = valid;

		} break;
		case VECTOR2I: {
			static const Type valid[] = {
				VECTOR2,
				NIL,
			};

			valid_types = valid;

		} break;
		case RECT2: {
			static const Type valid[] = {
				RECT2I,
				NIL,
			};

			valid_types = valid;

		} break;
		case RECT2I: {
			static const Type valid[] = {
				RECT2,
				NIL,
			};

			valid_types = valid;

		} break;
		case TRANSFORM2D: {
			static const Type valid[] = {
				TRANSFORM3D,
				NIL
			};

			valid_types = valid;
		} break;
		case VECTOR3: {
			static const Type valid[] = {
				VECTOR3I,
				NIL,
			};

			valid_types = valid;

		} break;
		case VECTOR3I: {
			static const Type valid[] = {
				VECTOR3,
				NIL,
			};

			valid_types = valid;

		} break;
		case VECTOR4: {
			static const Type valid[] = {
				VECTOR4I,
				NIL,
			};

			valid_types = valid;

		} break;
		case VECTOR4I: {
			static const Type valid[] = {
				VECTOR4,
				NIL,
			};

			valid_types = valid;

		} break;

		case QUATERNION: {
			static const Type valid[] = {
				BASIS,
				NIL
			};

			valid_types = valid;

		} break;
		case BASIS: {
			static const Type valid[] = {
				QUATERNION,
				NIL
			};

			valid_types = valid;

		} break;
		case TRANSFORM3D: {
			static const Type valid[] = {
				TRANSFORM2D,
				QUATERNION,
				BASIS,
				PROJECTION,
				NIL
			};

			valid_types = valid;

		} break;
		case PROJECTION: {
			static const Type valid[] = {
				TRANSFORM3D,
				NIL
			};

			valid_types = valid;

		} break;

		case COLOR: {
			static const Type valid[] = {
				STRING,
				INT,
				NIL,
			};

			valid_types = valid;

		} break;

		case RID: {
			static const Type valid[] = {
				OBJECT,
				NIL
			};

			valid_types = valid;
		} break;
		case OBJECT: {
			static const Type valid[] = {
				NIL
			};

			valid_types = valid;
		} break;
		case STRING_NAME: {
			static const Type valid[] = {
				STRING,
				NIL
			};

			valid_types = valid;
		} break;
		case NODE_PATH: {
			static const Type valid[] = {
				STRING,
				NIL
			};

			valid_types = valid;
		} break;
		case ARRAY: {
			static const Type valid[] = {
				PACKED_BYTE_ARRAY,
				PACKED_INT32_ARRAY,
				PACKED_INT64_ARRAY,
				PACKED_FLOAT32_ARRAY,
				PACKED_FLOAT64_ARRAY,
				PACKED_STRING_ARRAY,
				PACKED_COLOR_ARRAY,
				PACKED_VECTOR2_ARRAY,
				PACKED_VECTOR3_ARRAY,
				PACKED_VECTOR4_ARRAY,
				NIL
			};

			valid_types = valid;
		} break;
		// arrays
		case PACKED_BYTE_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};

			valid_types = valid;
		} break;
		case PACKED_INT32_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;
		} break;
		case PACKED_INT64_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;
		} break;
		case PACKED_FLOAT32_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};

			valid_types = valid;
		} break;
		case PACKED_FLOAT64_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};

			valid_types = valid;
		} break;
		case PACKED_STRING_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;
		} break;
		case PACKED_VECTOR2_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;

		} break;
		case PACKED_VECTOR3_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;

		} break;
		case PACKED_COLOR_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};

			valid_types = valid;

		} break;
		case PACKED_VECTOR4_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;

		} break;
		default: {
		}
	}

	if (valid_types) {
		int i = 0;
		while (valid_types[i] != NIL) {
			if (p_type_from == valid_types[i]) {
				return true;
			}
			i++;
		}

	} else if (invalid_types) {
		int i = 0;
		while (invalid_types[i] != NIL) {
			if (p_type_from == invalid_types[i]) {
				return false;
			}
			i++;
		}

		return true;
	}

	return false;
}

bool Variant::can_convert_strict(Variant::Type p_type_from, Variant::Type p_type_to) {
	if (p_type_from == p_type_to) {
		return true;
	}
	if (p_type_to == NIL) { //nil can convert to anything
		return true;
	}

	if (p_type_from == NIL) {
		return (p_type_to == OBJECT);
	}

	const Type *valid_types = nullptr;

	switch (p_type_to) {
		case BOOL: {
			static const Type valid[] = {
				INT,
				FLOAT,
				//STRING,
				NIL,
			};

			valid_types = valid;
		} break;
		case INT: {
			static const Type valid[] = {
				BOOL,
				FLOAT,
				//STRING,
				NIL,
			};

			valid_types = valid;

		} break;
		case FLOAT: {
			static const Type valid[] = {
				BOOL,
				INT,
				//STRING,
				NIL,
			};

			valid_types = valid;

		} break;
		case STRING: {
			static const Type valid[] = {
				NODE_PATH,
				STRING_NAME,
				NIL
			};

			valid_types = valid;
		} break;
		case VECTOR2: {
			static const Type valid[] = {
				VECTOR2I,
				NIL,
			};

			valid_types = valid;

		} break;
		case VECTOR2I: {
			static const Type valid[] = {
				VECTOR2,
				NIL,
			};

			valid_types = valid;

		} break;
		case RECT2: {
			static const Type valid[] = {
				RECT2I,
				NIL,
			};

			valid_types = valid;

		} break;
		case RECT2I: {
			static const Type valid[] = {
				RECT2,
				NIL,
			};

			valid_types = valid;

		} break;
		case TRANSFORM2D: {
			static const Type valid[] = {
				TRANSFORM3D,
				NIL
			};

			valid_types = valid;
		} break;
		case VECTOR3: {
			static const Type valid[] = {
				VECTOR3I,
				NIL,
			};

			valid_types = valid;

		} break;
		case VECTOR3I: {
			static const Type valid[] = {
				VECTOR3,
				NIL,
			};

			valid_types = valid;

		} break;
		case VECTOR4: {
			static const Type valid[] = {
				VECTOR4I,
				NIL,
			};

			valid_types = valid;

		} break;
		case VECTOR4I: {
			static const Type valid[] = {
				VECTOR4,
				NIL,
			};

			valid_types = valid;

		} break;

		case QUATERNION: {
			static const Type valid[] = {
				BASIS,
				NIL
			};

			valid_types = valid;

		} break;
		case BASIS: {
			static const Type valid[] = {
				QUATERNION,
				NIL
			};

			valid_types = valid;

		} break;
		case TRANSFORM3D: {
			static const Type valid[] = {
				TRANSFORM2D,
				QUATERNION,
				BASIS,
				PROJECTION,
				NIL
			};

			valid_types = valid;

		} break;
		case PROJECTION: {
			static const Type valid[] = {
				TRANSFORM3D,
				NIL
			};

			valid_types = valid;

		} break;

		case COLOR: {
			static const Type valid[] = {
				STRING,
				INT,
				NIL,
			};

			valid_types = valid;

		} break;

		case RID: {
			static const Type valid[] = {
				OBJECT,
				NIL
			};

			valid_types = valid;
		} break;
		case OBJECT: {
			static const Type valid[] = {
				NIL
			};

			valid_types = valid;
		} break;
		case STRING_NAME: {
			static const Type valid[] = {
				STRING,
				NIL
			};

			valid_types = valid;
		} break;
		case NODE_PATH: {
			static const Type valid[] = {
				STRING,
				NIL
			};

			valid_types = valid;
		} break;
		case ARRAY: {
			static const Type valid[] = {
				PACKED_BYTE_ARRAY,
				PACKED_INT32_ARRAY,
				PACKED_INT64_ARRAY,
				PACKED_FLOAT32_ARRAY,
				PACKED_FLOAT64_ARRAY,
				PACKED_STRING_ARRAY,
				PACKED_COLOR_ARRAY,
				PACKED_VECTOR2_ARRAY,
				PACKED_VECTOR3_ARRAY,
				PACKED_VECTOR4_ARRAY,
				NIL
			};

			valid_types = valid;
		} break;
		// arrays
		case PACKED_BYTE_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};

			valid_types = valid;
		} break;
		case PACKED_INT32_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;
		} break;
		case PACKED_INT64_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;
		} break;
		case PACKED_FLOAT32_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};

			valid_types = valid;
		} break;
		case PACKED_FLOAT64_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};

			valid_types = valid;
		} break;
		case PACKED_STRING_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;
		} break;
		case PACKED_VECTOR2_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;

		} break;
		case PACKED_VECTOR3_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;

		} break;
		case PACKED_COLOR_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};

			valid_types = valid;

		} break;
		case PACKED_VECTOR4_ARRAY: {
			static const Type valid[] = {
				ARRAY,
				NIL
			};
			valid_types = valid;

		} break;
		default: {
		}
	}

	if (valid_types) {
		int i = 0;
		while (valid_types[i] != NIL) {
			if (p_type_from == valid_types[i]) {
				return true;
			}
			i++;
		}
	}

	return false;
}

bool Variant::operator==(const Variant &p_variant) const {
	return hash_compare(p_variant);
}

bool Variant::operator!=(const Variant &p_variant) const {
	// Don't use `!hash_compare(p_variant)` given it makes use of OP_EQUAL
	if (type != p_variant.type) { //evaluation of operator== needs to be more strict
		return true;
	}
	bool v;
	Variant r;
	evaluate(OP_NOT_EQUAL, *this, p_variant, r, v);
	return r;
}

bool Variant::operator<(const Variant &p_variant) const {
	if (type != p_variant.type) { //if types differ, then order by type first
		return type < p_variant.type;
	}
	bool v;
	Variant r;
	evaluate(OP_LESS, *this, p_variant, r, v);
	return r;
}

bool Variant::is_zero() const {
	switch (type) {
		case NIL: {
			return true;
		}

		// Atomic types.
		case BOOL: {
			return !(_data._bool);
		}
		case INT: {
			return _data._int == 0;
		}
		case FLOAT: {
			return _data._float == 0;
		}
		case STRING: {
			return *reinterpret_cast<const String *>(_data._mem) == String();
		}

		// Math types.
		case VECTOR2: {
			return *reinterpret_cast<const Vector2 *>(_data._mem) == Vector2();
		}
		case VECTOR2I: {
			return *reinterpret_cast<const Vector2i *>(_data._mem) == Vector2i();
		}
		case RECT2: {
			return *reinterpret_cast<const Rect2 *>(_data._mem) == Rect2();
		}
		case RECT2I: {
			return *reinterpret_cast<const Rect2i *>(_data._mem) == Rect2i();
		}
		case TRANSFORM2D: {
			return *_data._transform2d == Transform2D();
		}
		case VECTOR3: {
			return *reinterpret_cast<const Vector3 *>(_data._mem) == Vector3();
		}
		case VECTOR3I: {
			return *reinterpret_cast<const Vector3i *>(_data._mem) == Vector3i();
		}
		case VECTOR4: {
			return *reinterpret_cast<const Vector4 *>(_data._mem) == Vector4();
		}
		case VECTOR4I: {
			return *reinterpret_cast<const Vector4i *>(_data._mem) == Vector4i();
		}
		case PLANE: {
			return *reinterpret_cast<const Plane *>(_data._mem) == Plane();
		}
		case AABB: {
			return *_data._aabb == ::AABB();
		}
		case QUATERNION: {
			return *reinterpret_cast<const Quaternion *>(_data._mem) == Quaternion();
		}
		case BASIS: {
			return *_data._basis == Basis();
		}
		case TRANSFORM3D: {
			return *_data._transform3d == Transform3D();
		}
		case PROJECTION: {
			return *_data._projection == Projection();
		}

		// Miscellaneous types.
		case COLOR: {
			return *reinterpret_cast<const Color *>(_data._mem) == Color();
		}
		case RID: {
			return *reinterpret_cast<const ::RID *>(_data._mem) == ::RID();
		}
		case OBJECT: {
			return get_validated_object() == nullptr;
		}
		case CALLABLE: {
			return reinterpret_cast<const Callable *>(_data._mem)->is_null();
		}
		case SIGNAL: {
			return reinterpret_cast<const Signal *>(_data._mem)->is_null();
		}
		case STRING_NAME: {
			return *reinterpret_cast<const StringName *>(_data._mem) == StringName();
		}
		case NODE_PATH: {
			return reinterpret_cast<const NodePath *>(_data._mem)->is_empty();
		}
		case DICTIONARY: {
			return reinterpret_cast<const Dictionary *>(_data._mem)->is_empty();
		}
		case ARRAY: {
			return reinterpret_cast<const Array *>(_data._mem)->is_empty();
		}

		// Arrays.
		case PACKED_BYTE_ARRAY: {
			return PackedArrayRef<uint8_t>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_INT32_ARRAY: {
			return PackedArrayRef<int32_t>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_INT64_ARRAY: {
			return PackedArrayRef<int64_t>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_FLOAT32_ARRAY: {
			return PackedArrayRef<float>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_FLOAT64_ARRAY: {
			return PackedArrayRef<double>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_STRING_ARRAY: {
			return PackedArrayRef<String>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_VECTOR2_ARRAY: {
			return PackedArrayRef<Vector2>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_VECTOR3_ARRAY: {
			return PackedArrayRef<Vector3>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_COLOR_ARRAY: {
			return PackedArrayRef<Color>::get_array(_data.packed_array).size() == 0;
		}
		case PACKED_VECTOR4_ARRAY: {
			return PackedArrayRef<Vector4>::get_array(_data.packed_array).size() == 0;
		}
		default: {
		}
	}

	return false;
}

bool Variant::is_one() const {
	switch (type) {
		case NIL: {
			return true;
		}

		case BOOL: {
			return _data._bool;
		}
		case INT: {
			return _data._int == 1;
		}
		case FLOAT: {
			return _data._float == 1;
		}

		case VECTOR2: {
			return *reinterpret_cast<const Vector2 *>(_data._mem) == Vector2(1, 1);
		}
		case VECTOR2I: {
			return *reinterpret_cast<const Vector2i *>(_data._mem) == Vector2i(1, 1);
		}
		case RECT2: {
			return *reinterpret_cast<const Rect2 *>(_data._mem) == Rect2(1, 1, 1, 1);
		}
		case RECT2I: {
			return *reinterpret_cast<const Rect2i *>(_data._mem) == Rect2i(1, 1, 1, 1);
		}
		case VECTOR3: {
			return *reinterpret_cast<const Vector3 *>(_data._mem) == Vector3(1, 1, 1);
		}
		case VECTOR3I: {
			return *reinterpret_cast<const Vector3i *>(_data._mem) == Vector3i(1, 1, 1);
		}
		case VECTOR4: {
			return *reinterpret_cast<const Vector4 *>(_data._mem) == Vector4(1, 1, 1, 1);
		}
		case VECTOR4I: {
			return *reinterpret_cast<const Vector4i *>(_data._mem) == Vector4i(1, 1, 1, 1);
		}
		case PLANE: {
			return *reinterpret_cast<const Plane *>(_data._mem) == Plane(1, 1, 1, 1);
		}

		case COLOR: {
			return *reinterpret_cast<const Color *>(_data._mem) == Color(1, 1, 1, 1);
		}

		default: {
			return !is_zero();
		}
	}
}

bool Variant::is_null() const {
	if (type == OBJECT && _get_obj().obj) {
		return false;
	} else {
		return true;
	}
}

void Variant::ObjData::ref(const ObjData &p_from) {
	// Mirrors Ref::ref in refcounted.h
	if (p_from.id == id) {
		return;
	}

	ObjData cleanup_ref = *this;

	*this = p_from;
	if (id.is_ref_counted()) {
		RefCounted *reference = static_cast<RefCounted *>(obj);
		// Assuming reference is not null because id.is_ref_counted() was true.
		if (!reference->reference()) {
			*this = ObjData();
		}
	}

	cleanup_ref.unref();
}

void Variant::ObjData::ref_pointer(Object *p_object) {
	// Mirrors Ref::ref_pointer in refcounted.h
	if (p_object == obj) {
		return;
	}

	ObjData cleanup_ref = *this;

	if (p_object) {
		*this = ObjData{ p_object->get_instance_id(), p_object };
		if (p_object->is_ref_counted()) {
			RefCounted *reference = static_cast<RefCounted *>(p_object);
			if (!reference->init_ref()) {
				*this = ObjData();
			}
		}
	} else {
		*this = ObjData();
	}

	cleanup_ref.unref();
}

void Variant::ObjData::unref() {
	// Mirrors Ref::unref in refcounted.h
	if (id.is_ref_counted()) {
		RefCounted *reference = static_cast<RefCounted *>(obj);
		// Assuming reference is not null because id.is_ref_counted() was true.
		if (reference->unreference()) {
			memdelete(reference);
		}
	}
	*this = ObjData();
}

void Variant::reference(const Variant &p_variant) {
	if (type == OBJECT && p_variant.type == OBJECT) {
		_get_obj().ref(p_variant._get_obj());
		return;
	}

	clear();

	type = p_variant.type;

	switch (p_variant.type) {
		case NIL: {
			// None.
		} break;

		// Atomic types.
		case BOOL: {
			_data._bool = p_variant._data._bool;
		} break;
		case INT: {
			_data._int = p_variant._data._int;
		} break;
		case FLOAT: {
			_data._float = p_variant._data._float;
		} break;
		case STRING: {
			memnew_placement(_data._mem, String(*reinterpret_cast<const String *>(p_variant._data._mem)));
		} break;

		// Math types.
		case VECTOR2: {
			memnew_placement(_data._mem, Vector2(*reinterpret_cast<const Vector2 *>(p_variant._data._mem)));
		} break;
		case VECTOR2I: {
			memnew_placement(_data._mem, Vector2i(*reinterpret_cast<const Vector2i *>(p_variant._data._mem)));
		} break;
		case RECT2: {
			memnew_placement(_data._mem, Rect2(*reinterpret_cast<const Rect2 *>(p_variant._data._mem)));
		} break;
		case RECT2I: {
			memnew_placement(_data._mem, Rect2i(*reinterpret_cast<const Rect2i *>(p_variant._data._mem)));
		} break;
		case TRANSFORM2D: {
			_data._transform2d = (Transform2D *)Pools::_bucket_small.alloc();
			memnew_placement(_data._transform2d, Transform2D(*p_variant._data._transform2d));
		} break;
		case VECTOR3: {
			memnew_placement(_data._mem, Vector3(*reinterpret_cast<const Vector3 *>(p_variant._data._mem)));
		} break;
		case VECTOR3I: {
			memnew_placement(_data._mem, Vector3i(*reinterpret_cast<const Vector3i *>(p_variant._data._mem)));
		} break;
		case VECTOR4: {
			memnew_placement(_data._mem, Vector4(*reinterpret_cast<const Vector4 *>(p_variant._data._mem)));
		} break;
		case VECTOR4I: {
			memnew_placement(_data._mem, Vector4i(*reinterpret_cast<const Vector4i *>(p_variant._data._mem)));
		} break;
		case PLANE: {
			memnew_placement(_data._mem, Plane(*reinterpret_cast<const Plane *>(p_variant._data._mem)));
		} break;
		case AABB: {
			_data._aabb = (::AABB *)Pools::_bucket_small.alloc();
			memnew_placement(_data._aabb, ::AABB(*p_variant._data._aabb));
		} break;
		case QUATERNION: {
			memnew_placement(_data._mem, Quaternion(*reinterpret_cast<const Quaternion *>(p_variant._data._mem)));
		} break;
		case BASIS: {
			_data._basis = (Basis *)Pools::_bucket_medium.alloc();
			memnew_placement(_data._basis, Basis(*p_variant._data._basis));
		} break;
		case TRANSFORM3D: {
			_data._transform3d = (Transform3D *)Pools::_bucket_medium.alloc();
			memnew_placement(_data._transform3d, Transform3D(*p_variant._data._transform3d));
		} break;
		case PROJECTION: {
			_data._projection = (Projection *)Pools::_bucket_large.alloc();
			memnew_placement(_data._projection, Projection(*p_variant._data._projection));
		} break;

		// Miscellaneous types.
		case COLOR: {
			memnew_placement(_data._mem, Color(*reinterpret_cast<const Color *>(p_variant._data._mem)));
		} break;
		case RID: {
			memnew_placement(_data._mem, ::RID(*reinterpret_cast<const ::RID *>(p_variant._data._mem)));
		} break;
		case OBJECT: {
			memnew_placement(_data._mem, ObjData);
			_get_obj().ref(p_variant._get_obj());
		} break;
		case CALLABLE: {
			memnew_placement(_data._mem, Callable(*reinterpret_cast<const Callable *>(p_variant._data._mem)));
		} break;
		case SIGNAL: {
			memnew_placement(_data._mem, Signal(*reinterpret_cast<const Signal *>(p_variant._data._mem)));
		} break;
		case STRING_NAME: {
			memnew_placement(_data._mem, StringName(*reinterpret_cast<const StringName *>(p_variant._data._mem)));
		} break;
		case NODE_PATH: {
			memnew_placement(_data._mem, NodePath(*reinterpret_cast<const NodePath *>(p_variant._data._mem)));
		} break;
		case DICTIONARY: {
			memnew_placement(_data._mem, Dictionary(*reinterpret_cast<const Dictionary *>(p_variant._data._mem)));
		} break;
		case ARRAY: {
			memnew_placement(_data._mem, Array(*reinterpret_cast<const Array *>(p_variant._data._mem)));
		} break;

		// Arrays.
		case PACKED_BYTE_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<uint8_t> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<uint8_t>::create();
			}
		} break;
		case PACKED_INT32_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<int32_t> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<int32_t>::create();
			}
		} break;
		case PACKED_INT64_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<int64_t> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<int64_t>::create();
			}
		} break;
		case PACKED_FLOAT32_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<float> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<float>::create();
			}
		} break;
		case PACKED_FLOAT64_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<double> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<double>::create();
			}
		} break;
		case PACKED_STRING_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<String> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<String>::create();
			}
		} break;
		case PACKED_VECTOR2_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<Vector2> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<Vector2>::create();
			}
		} break;
		case PACKED_VECTOR3_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<Vector3> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<Vector3>::create();
			}
		} break;
		case PACKED_COLOR_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<Color> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<Color>::create();
			}
		} break;
		case PACKED_VECTOR4_ARRAY: {
			_data.packed_array = static_cast<PackedArrayRef<Vector4> *>(p_variant._data.packed_array)->reference();
			if (!_data.packed_array) {
				_data.packed_array = PackedArrayRef<Vector4>::create();
			}
		} break;
		default: {
		}
	}
}

void Variant::zero() {
	switch (type) {
		case NIL:
			break;
		case BOOL:
			_data._bool = false;
			break;
		case INT:
			_data._int = 0;
			break;
		case FLOAT:
			_data._float = 0;
			break;

		case VECTOR2:
			*reinterpret_cast<Vector2 *>(_data._mem) = Vector2();
			break;
		case VECTOR2I:
			*reinterpret_cast<Vector2i *>(_data._mem) = Vector2i();
			break;
		case RECT2:
			*reinterpret_cast<Rect2 *>(_data._mem) = Rect2();
			break;
		case RECT2I:
			*reinterpret_cast<Rect2i *>(_data._mem) = Rect2i();
			break;
		case VECTOR3:
			*reinterpret_cast<Vector3 *>(_data._mem) = Vector3();
			break;
		case VECTOR3I:
			*reinterpret_cast<Vector3i *>(_data._mem) = Vector3i();
			break;
		case VECTOR4:
			*reinterpret_cast<Vector4 *>(_data._mem) = Vector4();
			break;
		case VECTOR4I:
			*reinterpret_cast<Vector4i *>(_data._mem) = Vector4i();
			break;
		case PLANE:
			*reinterpret_cast<Plane *>(_data._mem) = Plane();
			break;
		case QUATERNION:
			*reinterpret_cast<Quaternion *>(_data._mem) = Quaternion();
			break;

		case COLOR:
			*reinterpret_cast<Color *>(_data._mem) = Color();
			break;

		default:
			Type prev_type = type;
			clear();
			if (type != prev_type) {
				// clear() changes type to NIL, so it needs to be restored.
				Callable::CallError ce;
				Variant::construct(prev_type, *this, nullptr, 0, ce);
			}
			break;
	}
}

void Variant::_clear_internal() {
	switch (type) {
		case STRING: {
			reinterpret_cast<String *>(_data._mem)->~String();
		} break;

		// Math types.
		case TRANSFORM2D: {
			if (_data._transform2d) {
				_data._transform2d->~Transform2D();
				Pools::_bucket_small.free((Pools::BucketSmall *)_data._transform2d);
				_data._transform2d = nullptr;
			}
		} break;
		case AABB: {
			if (_data._aabb) {
				_data._aabb->~AABB();
				Pools::_bucket_small.free((Pools::BucketSmall *)_data._aabb);
				_data._aabb = nullptr;
			}
		} break;
		case BASIS: {
			if (_data._basis) {
				_data._basis->~Basis();
				Pools::_bucket_medium.free((Pools::BucketMedium *)_data._basis);
				_data._basis = nullptr;
			}
		} break;
		case TRANSFORM3D: {
			if (_data._transform3d) {
				_data._transform3d->~Transform3D();
				Pools::_bucket_medium.free((Pools::BucketMedium *)_data._transform3d);
				_data._transform3d = nullptr;
			}
		} break;
		case PROJECTION: {
			if (_data._projection) {
				_data._projection->~Projection();
				Pools::_bucket_large.free((Pools::BucketLarge *)_data._projection);
				_data._projection = nullptr;
			}
		} break;

		// Miscellaneous types.
		case STRING_NAME: {
			reinterpret_cast<StringName *>(_data._mem)->~StringName();
		} break;
		case NODE_PATH: {
			reinterpret_cast<NodePath *>(_data._mem)->~NodePath();
		} break;
		case OBJECT: {
			_get_obj().unref();
		} break;
		case RID: {
			// Not much need probably.
			// HACK: Can't seem to use destructor + scoping operator, so hack.
			typedef ::RID RID_Class;
			reinterpret_cast<RID_Class *>(_data._mem)->~RID_Class();
		} break;
		case CALLABLE: {
			reinterpret_cast<Callable *>(_data._mem)->~Callable();
		} break;
		case SIGNAL: {
			reinterpret_cast<Signal *>(_data._mem)->~Signal();
		} break;
		case DICTIONARY: {
			reinterpret_cast<Dictionary *>(_data._mem)->~Dictionary();
		} break;
		case ARRAY: {
			reinterpret_cast<Array *>(_data._mem)->~Array();
		} break;

		// Arrays.
		case PACKED_BYTE_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_INT32_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_INT64_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_FLOAT32_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_FLOAT64_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_STRING_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_VECTOR2_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_VECTOR3_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_COLOR_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		case PACKED_VECTOR4_ARRAY: {
			PackedArrayRefBase::destroy(_data.packed_array);
		} break;
		default: {
			// Not needed, there is no point. The following do not allocate memory:
			// VECTOR2, VECTOR3, VECTOR4, RECT2, PLANE, QUATERNION, COLOR.
		}
	}
}

Variant::operator int64_t() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1 : 0;
		case INT:
			return int64_t(_data._int);
		case FLOAT:
			return int64_t(_data._float);
		case STRING:
			return int64_t(operator String().to_int());
		default: {
			return 0;
		}
	}
}

Variant::operator int32_t() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1 : 0;
		case INT:
			return int32_t(_data._int);
		case FLOAT:
			return int32_t(_data._float);
		case STRING:
			return int32_t(operator String().to_int());
		default: {
			return 0;
		}
	}
}

Variant::operator int16_t() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1 : 0;
		case INT:
			return int16_t(_data._int);
		case FLOAT:
			return int16_t(_data._float);
		case STRING:
			return int16_t(operator String().to_int());
		default: {
			return 0;
		}
	}
}

Variant::operator int8_t() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1 : 0;
		case INT:
			return int8_t(_data._int);
		case FLOAT:
			return int8_t(_data._float);
		case STRING:
			return int8_t(operator String().to_int());
		default: {
			return 0;
		}
	}
}

Variant::operator uint64_t() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1 : 0;
		case INT:
			return uint64_t(_data._int);
		case FLOAT:
			return uint64_t(_data._float);
		case STRING:
			return uint64_t(operator String().to_int());
		default: {
			return 0;
		}
	}
}

Variant::operator uint32_t() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1 : 0;
		case INT:
			return uint32_t(_data._int);
		case FLOAT:
			return uint32_t(_data._float);
		case STRING:
			return uint32_t(operator String().to_int());
		default: {
			return 0;
		}
	}
}

Variant::operator uint16_t() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1 : 0;
		case INT:
			return uint16_t(_data._int);
		case FLOAT:
			return uint16_t(_data._float);
		case STRING:
			return uint16_t(operator String().to_int());
		default: {
			return 0;
		}
	}
}

Variant::operator uint8_t() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1 : 0;
		case INT:
			return uint8_t(_data._int);
		case FLOAT:
			return uint8_t(_data._float);
		case STRING:
			return uint8_t(operator String().to_int());
		default: {
			return 0;
		}
	}
}

Variant::operator ObjectID() const {
	if (type == INT) {
		return ObjectID(_data._int);
	} else if (type == OBJECT) {
		return _get_obj().id;
	} else {
		return ObjectID();
	}
}

Variant::operator char32_t() const {
	return operator uint32_t();
}

Variant::operator float() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1.0 : 0.0;
		case INT:
			return (float)_data._int;
		case FLOAT:
			return _data._float;
		case STRING:
			return operator String().to_float();
		default: {
			return 0;
		}
	}
}

Variant::operator double() const {
	switch (type) {
		case NIL:
			return 0;
		case BOOL:
			return _data._bool ? 1.0 : 0.0;
		case INT:
			return (double)_data._int;
		case FLOAT:
			return _data._float;
		case STRING:
			return operator String().to_float();
		default: {
			return 0;
		}
	}
}

Variant::operator StringName() const {
	if (type == STRING_NAME) {
		return *reinterpret_cast<const StringName *>(_data._mem);
	} else if (type == STRING) {
		return *reinterpret_cast<const String *>(_data._mem);
	}

	return StringName();
}

struct _VariantStrPair {
	String key;
	String value;

	bool operator<(const _VariantStrPair &p) const {
		return key < p.key;
	}
};

Variant::operator String() const {
	return stringify(0);
}

String stringify_variant_clean(const Variant &p_variant, int recursion_count) {
	String s = p_variant.stringify(recursion_count);

	// Wrap strings in quotes to avoid ambiguity.
	switch (p_variant.get_type()) {
		case Variant::STRING: {
			s = s.c_escape().quote();
		} break;
		case Variant::STRING_NAME: {
			s = "&" + s.c_escape().quote();
		} break;
		case Variant::NODE_PATH: {
			s = "^" + s.c_escape().quote();
		} break;
		default: {
		} break;
	}

	return s;
}

template <typename T>
String stringify_vector(const T &vec, int recursion_count) {
	String str("[");
	for (int i = 0; i < vec.size(); i++) {
		if (i > 0) {
			str += ", ";
		}

		str += stringify_variant_clean(vec[i], recursion_count);
	}
	str += "]";
	return str;
}

String Variant::stringify(int recursion_count) const {
	switch (type) {
		case NIL:
			return "<null>";
		case BOOL:
			return _data._bool ? "true" : "false";
		case INT:
			return itos(_data._int);
		case FLOAT:
			return String::num_real(_data._float, true);
		case STRING:
			return *reinterpret_cast<const String *>(_data._mem);
		case VECTOR2:
			return operator Vector2();
		case VECTOR2I:
			return operator Vector2i();
		case RECT2:
			return operator Rect2();
		case RECT2I:
			return operator Rect2i();
		case TRANSFORM2D:
			return operator Transform2D();
		case VECTOR3:
			return operator Vector3();
		case VECTOR3I:
			return operator Vector3i();
		case VECTOR4:
			return operator Vector4();
		case VECTOR4I:
			return operator Vector4i();
		case PLANE:
			return operator Plane();
		case AABB:
			return operator ::AABB();
		case QUATERNION:
			return operator Quaternion();
		case BASIS:
			return operator Basis();
		case TRANSFORM3D:
			return operator Transform3D();
		case PROJECTION:
			return operator Projection();
		case STRING_NAME:
			return operator StringName();
		case NODE_PATH:
			return operator NodePath();
		case COLOR:
			return operator Color();
		case DICTIONARY: {
			ERR_FAIL_COND_V_MSG(recursion_count > MAX_RECURSION, "{ ... }", "Maximum dictionary recursion reached!");
			recursion_count++;

			const Dictionary &d = *reinterpret_cast<const Dictionary *>(_data._mem);

			// Add leading and trailing space to Dictionary printing. This distinguishes it
			// from array printing on fonts that have similar-looking {} and [] characters.
			String str("{ ");
			List<Variant> keys;
			d.get_key_list(&keys);

			Vector<_VariantStrPair> pairs;

			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
				_VariantStrPair sp;
				sp.key = stringify_variant_clean(E->get(), recursion_count);
				sp.value = stringify_variant_clean(d[E->get()], recursion_count);

				pairs.push_back(sp);
			}

			for (int i = 0; i < pairs.size(); i++) {
				if (i > 0) {
					str += ", ";
				}
				str += pairs[i].key + ": " + pairs[i].value;
			}
			str += " }";

			return str;
		}
		// Packed arrays cannot contain recursive structures, the recursion_count increment is not needed.
		case PACKED_VECTOR2_ARRAY: {
			return stringify_vector(operator PackedVector2Array(), recursion_count);
		}
		case PACKED_VECTOR3_ARRAY: {
			return stringify_vector(operator PackedVector3Array(), recursion_count);
		}
		case PACKED_COLOR_ARRAY: {
			return stringify_vector(operator PackedColorArray(), recursion_count);
		}
		case PACKED_VECTOR4_ARRAY: {
			return stringify_vector(operator PackedVector4Array(), recursion_count);
		}
		case PACKED_STRING_ARRAY: {
			return stringify_vector(operator PackedStringArray(), recursion_count);
		}
		case PACKED_BYTE_ARRAY: {
			return stringify_vector(operator PackedByteArray(), recursion_count);
		}
		case PACKED_INT32_ARRAY: {
			return stringify_vector(operator PackedInt32Array(), recursion_count);
		}
		case PACKED_INT64_ARRAY: {
			return stringify_vector(operator PackedInt64Array(), recursion_count);
		}
		case PACKED_FLOAT32_ARRAY: {
			return stringify_vector(operator PackedFloat32Array(), recursion_count);
		}
		case PACKED_FLOAT64_ARRAY: {
			return stringify_vector(operator PackedFloat64Array(), recursion_count);
		}
		case ARRAY: {
			ERR_FAIL_COND_V_MSG(recursion_count > MAX_RECURSION, "[...]", "Maximum array recursion reached!");
			recursion_count++;

			return stringify_vector(operator Array(), recursion_count);
		}
		case OBJECT: {
			if (_get_obj().obj) {
				if (!_get_obj().id.is_ref_counted() && ObjectDB::get_instance(_get_obj().id) == nullptr) {
					return "<Freed Object>";
				}

				return _get_obj().obj->to_string();
			} else {
				return "<Object#null>";
			}
		}
		case CALLABLE: {
			const Callable &c = *reinterpret_cast<const Callable *>(_data._mem);
			return c;
		}
		case SIGNAL: {
			const Signal &s = *reinterpret_cast<const Signal *>(_data._mem);
			return s;
		}
		case RID: {
			const ::RID &s = *reinterpret_cast<const ::RID *>(_data._mem);
			return "RID(" + itos(s.get_id()) + ")";
		}
		default: {
			return "<" + get_type_name(type) + ">";
		}
	}
}

String Variant::to_json_string() const {
	return JSON::stringify(*this);
}

Variant::operator Vector2() const {
	if (type == VECTOR2) {
		return *reinterpret_cast<const Vector2 *>(_data._mem);
	} else if (type == VECTOR2I) {
		return *reinterpret_cast<const Vector2i *>(_data._mem);
	} else if (type == VECTOR3) {
		return Vector2(reinterpret_cast<const Vector3 *>(_data._mem)->x, reinterpret_cast<const Vector3 *>(_data._mem)->y);
	} else if (type == VECTOR3I) {
		return Vector2(reinterpret_cast<const Vector3i *>(_data._mem)->x, reinterpret_cast<const Vector3i *>(_data._mem)->y);
	} else if (type == VECTOR4) {
		return Vector2(reinterpret_cast<const Vector4 *>(_data._mem)->x, reinterpret_cast<const Vector4 *>(_data._mem)->y);
	} else if (type == VECTOR4I) {
		return Vector2(reinterpret_cast<const Vector4i *>(_data._mem)->x, reinterpret_cast<const Vector4i *>(_data._mem)->y);
	} else {
		return Vector2();
	}
}

Variant::operator Vector2i() const {
	if (type == VECTOR2I) {
		return *reinterpret_cast<const Vector2i *>(_data._mem);
	} else if (type == VECTOR2) {
		return *reinterpret_cast<const Vector2 *>(_data._mem);
	} else if (type == VECTOR3) {
		return Vector2(reinterpret_cast<const Vector3 *>(_data._mem)->x, reinterpret_cast<const Vector3 *>(_data._mem)->y);
	} else if (type == VECTOR3I) {
		return Vector2(reinterpret_cast<const Vector3i *>(_data._mem)->x, reinterpret_cast<const Vector3i *>(_data._mem)->y);
	} else if (type == VECTOR4) {
		return Vector2(reinterpret_cast<const Vector4 *>(_data._mem)->x, reinterpret_cast<const Vector4 *>(_data._mem)->y);
	} else if (type == VECTOR4I) {
		return Vector2(reinterpret_cast<const Vector4i *>(_data._mem)->x, reinterpret_cast<const Vector4i *>(_data._mem)->y);
	} else {
		return Vector2i();
	}
}

Variant::operator Rect2() const {
	if (type == RECT2) {
		return *reinterpret_cast<const Rect2 *>(_data._mem);
	} else if (type == RECT2I) {
		return *reinterpret_cast<const Rect2i *>(_data._mem);
	} else {
		return Rect2();
	}
}

Variant::operator Rect2i() const {
	if (type == RECT2I) {
		return *reinterpret_cast<const Rect2i *>(_data._mem);
	} else if (type == RECT2) {
		return *reinterpret_cast<const Rect2 *>(_data._mem);
	} else {
		return Rect2i();
	}
}

Variant::operator Vector3() const {
	if (type == VECTOR3) {
		return *reinterpret_cast<const Vector3 *>(_data._mem);
	} else if (type == VECTOR3I) {
		return *reinterpret_cast<const Vector3i *>(_data._mem);
	} else if (type == VECTOR2) {
		return Vector3(reinterpret_cast<const Vector2 *>(_data._mem)->x, reinterpret_cast<const Vector2 *>(_data._mem)->y, 0.0);
	} else if (type == VECTOR2I) {
		return Vector3(reinterpret_cast<const Vector2i *>(_data._mem)->x, reinterpret_cast<const Vector2i *>(_data._mem)->y, 0.0);
	} else if (type == VECTOR4) {
		return Vector3(reinterpret_cast<const Vector4 *>(_data._mem)->x, reinterpret_cast<const Vector4 *>(_data._mem)->y, reinterpret_cast<const Vector4 *>(_data._mem)->z);
	} else if (type == VECTOR4I) {
		return Vector3(reinterpret_cast<const Vector4i *>(_data._mem)->x, reinterpret_cast<const Vector4i *>(_data._mem)->y, reinterpret_cast<const Vector4i *>(_data._mem)->z);
	} else {
		return Vector3();
	}
}

Variant::operator Vector3i() const {
	if (type == VECTOR3I) {
		return *reinterpret_cast<const Vector3i *>(_data._mem);
	} else if (type == VECTOR3) {
		return *reinterpret_cast<const Vector3 *>(_data._mem);
	} else if (type == VECTOR2) {
		return Vector3i(reinterpret_cast<const Vector2 *>(_data._mem)->x, reinterpret_cast<const Vector2 *>(_data._mem)->y, 0.0);
	} else if (type == VECTOR2I) {
		return Vector3i(reinterpret_cast<const Vector2i *>(_data._mem)->x, reinterpret_cast<const Vector2i *>(_data._mem)->y, 0.0);
	} else if (type == VECTOR4) {
		return Vector3i(reinterpret_cast<const Vector4 *>(_data._mem)->x, reinterpret_cast<const Vector4 *>(_data._mem)->y, reinterpret_cast<const Vector4 *>(_data._mem)->z);
	} else if (type == VECTOR4I) {
		return Vector3i(reinterpret_cast<const Vector4i *>(_data._mem)->x, reinterpret_cast<const Vector4i *>(_data._mem)->y, reinterpret_cast<const Vector4i *>(_data._mem)->z);
	} else {
		return Vector3i();
	}
}

Variant::operator Vector4() const {
	if (type == VECTOR4) {
		return *reinterpret_cast<const Vector4 *>(_data._mem);
	} else if (type == VECTOR4I) {
		return *reinterpret_cast<const Vector4i *>(_data._mem);
	} else if (type == VECTOR2) {
		return Vector4(reinterpret_cast<const Vector2 *>(_data._mem)->x, reinterpret_cast<const Vector2 *>(_data._mem)->y, 0.0, 0.0);
	} else if (type == VECTOR2I) {
		return Vector4(reinterpret_cast<const Vector2i *>(_data._mem)->x, reinterpret_cast<const Vector2i *>(_data._mem)->y, 0.0, 0.0);
	} else if (type == VECTOR3) {
		return Vector4(reinterpret_cast<const Vector3 *>(_data._mem)->x, reinterpret_cast<const Vector3 *>(_data._mem)->y, reinterpret_cast<const Vector3 *>(_data._mem)->z, 0.0);
	} else if (type == VECTOR3I) {
		return Vector4(reinterpret_cast<const Vector3i *>(_data._mem)->x, reinterpret_cast<const Vector3i *>(_data._mem)->y, reinterpret_cast<const Vector3i *>(_data._mem)->z, 0.0);
	} else {
		return Vector4();
	}
}

Variant::operator Vector4i() const {
	if (type == VECTOR4I) {
		return *reinterpret_cast<const Vector4i *>(_data._mem);
	} else if (type == VECTOR4) {
		const Vector4 &v4 = *reinterpret_cast<const Vector4 *>(_data._mem);
		return Vector4i(v4.x, v4.y, v4.z, v4.w);
	} else if (type == VECTOR2) {
		return Vector4i(reinterpret_cast<const Vector2 *>(_data._mem)->x, reinterpret_cast<const Vector2 *>(_data._mem)->y, 0.0, 0.0);
	} else if (type == VECTOR2I) {
		return Vector4i(reinterpret_cast<const Vector2i *>(_data._mem)->x, reinterpret_cast<const Vector2i *>(_data._mem)->y, 0.0, 0.0);
	} else if (type == VECTOR3) {
		return Vector4i(reinterpret_cast<const Vector3 *>(_data._mem)->x, reinterpret_cast<const Vector3 *>(_data._mem)->y, reinterpret_cast<const Vector3 *>(_data._mem)->z, 0.0);
	} else if (type == VECTOR3I) {
		return Vector4i(reinterpret_cast<const Vector3i *>(_data._mem)->x, reinterpret_cast<const Vector3i *>(_data._mem)->y, reinterpret_cast<const Vector3i *>(_data._mem)->z, 0.0);
	} else {
		return Vector4i();
	}
}

Variant::operator Plane() const {
	if (type == PLANE) {
		return *reinterpret_cast<const Plane *>(_data._mem);
	} else {
		return Plane();
	}
}

Variant::operator ::AABB() const {
	if (type == AABB) {
		return *_data._aabb;
	} else {
		return ::AABB();
	}
}

Variant::operator Basis() const {
	if (type == BASIS) {
		return *_data._basis;
	} else if (type == QUATERNION) {
		return *reinterpret_cast<const Quaternion *>(_data._mem);
	} else if (type == TRANSFORM3D) { // unexposed in Variant::can_convert?
		return _data._transform3d->basis;
	} else {
		return Basis();
	}
}

Variant::operator Quaternion() const {
	if (type == QUATERNION) {
		return *reinterpret_cast<const Quaternion *>(_data._mem);
	} else if (type == BASIS) {
		return *_data._basis;
	} else if (type == TRANSFORM3D) {
		return _data._transform3d->basis;
	} else {
		return Quaternion();
	}
}

Variant::operator Transform3D() const {
	if (type == TRANSFORM3D) {
		return *_data._transform3d;
	} else if (type == BASIS) {
		return Transform3D(*_data._basis, Vector3());
	} else if (type == QUATERNION) {
		return Transform3D(Basis(*reinterpret_cast<const Quaternion *>(_data._mem)), Vector3());
	} else if (type == TRANSFORM2D) {
		const Transform2D &t = *_data._transform2d;
		Transform3D m;
		m.basis.rows[0][0] = t.columns[0][0];
		m.basis.rows[1][0] = t.columns[0][1];
		m.basis.rows[0][1] = t.columns[1][0];
		m.basis.rows[1][1] = t.columns[1][1];
		m.origin[0] = t.columns[2][0];
		m.origin[1] = t.columns[2][1];
		return m;
	} else if (type == PROJECTION) {
		return *_data._projection;
	} else {
		return Transform3D();
	}
}

Variant::operator Projection() const {
	if (type == TRANSFORM3D) {
		return *_data._transform3d;
	} else if (type == BASIS) {
		return Transform3D(*_data._basis, Vector3());
	} else if (type == QUATERNION) {
		return Transform3D(Basis(*reinterpret_cast<const Quaternion *>(_data._mem)), Vector3());
	} else if (type == TRANSFORM2D) {
		const Transform2D &t = *_data._transform2d;
		Transform3D m;
		m.basis.rows[0][0] = t.columns[0][0];
		m.basis.rows[1][0] = t.columns[0][1];
		m.basis.rows[0][1] = t.columns[1][0];
		m.basis.rows[1][1] = t.columns[1][1];
		m.origin[0] = t.columns[2][0];
		m.origin[1] = t.columns[2][1];
		return m;
	} else if (type == PROJECTION) {
		return *_data._projection;
	} else {
		return Projection();
	}
}

Variant::operator Transform2D() const {
	if (type == TRANSFORM2D) {
		return *_data._transform2d;
	} else if (type == TRANSFORM3D) {
		const Transform3D &t = *_data._transform3d;
		Transform2D m;
		m.columns[0][0] = t.basis.rows[0][0];
		m.columns[0][1] = t.basis.rows[1][0];
		m.columns[1][0] = t.basis.rows[0][1];
		m.columns[1][1] = t.basis.rows[1][1];
		m.columns[2][0] = t.origin[0];
		m.columns[2][1] = t.origin[1];
		return m;
	} else {
		return Transform2D();
	}
}

Variant::operator Color() const {
	if (type == COLOR) {
		return *reinterpret_cast<const Color *>(_data._mem);
	} else if (type == STRING) {
		return Color(operator String());
	} else if (type == INT) {
		return Color::hex(operator int());
	} else {
		return Color();
	}
}

Variant::operator NodePath() const {
	if (type == NODE_PATH) {
		return *reinterpret_cast<const NodePath *>(_data._mem);
	} else if (type == STRING) {
		return NodePath(operator String());
	} else {
		return NodePath();
	}
}

Variant::operator ::RID() const {
	if (type == RID) {
		return *reinterpret_cast<const ::RID *>(_data._mem);
	} else if (type == OBJECT && _get_obj().obj == nullptr) {
		return ::RID();
	} else if (type == OBJECT && _get_obj().obj) {
#ifdef DEBUG_ENABLED
		if (EngineDebugger::is_active()) {
			ERR_FAIL_NULL_V_MSG(ObjectDB::get_instance(_get_obj().id), ::RID(), "Invalid pointer (object was freed).");
		}
#endif
		Callable::CallError ce;
		const Variant ret = _get_obj().obj->callp(CoreStringName(get_rid), nullptr, 0, ce);
		if (ce.error == Callable::CallError::CALL_OK && ret.get_type() == Variant::RID) {
			return ret;
		}
		return ::RID();
	} else {
		return ::RID();
	}
}

Variant::operator Object *() const {
	if (type == OBJECT) {
		return _get_obj().obj;
	} else {
		return nullptr;
	}
}

Object *Variant::get_validated_object_with_check(bool &r_previously_freed) const {
	if (type == OBJECT) {
		Object *instance = ObjectDB::get_instance(_get_obj().id);
		r_previously_freed = !instance && _get_obj().id != ObjectID();
		return instance;
	} else {
		r_previously_freed = false;
		return nullptr;
	}
}

Object *Variant::get_validated_object() const {
	if (type == OBJECT) {
		return ObjectDB::get_instance(_get_obj().id);
	} else {
		return nullptr;
	}
}

Variant::operator Dictionary() const {
	if (type == DICTIONARY) {
		return *reinterpret_cast<const Dictionary *>(_data._mem);
	} else {
		return Dictionary();
	}
}

Variant::operator Callable() const {
	if (type == CALLABLE) {
		return *reinterpret_cast<const Callable *>(_data._mem);
	} else {
		return Callable();
	}
}

Variant::operator Signal() const {
	if (type == SIGNAL) {
		return *reinterpret_cast<const Signal *>(_data._mem);
	} else {
		return Signal();
	}
}

template <typename DA, typename SA>
inline DA _convert_array(const SA &p_array) {
	DA da;
	da.resize(p_array.size());

	for (int i = 0; i < p_array.size(); i++) {
		da.set(i, Variant(p_array.get(i)));
	}

	return da;
}

template <typename DA>
inline DA _convert_array_from_variant(const Variant &p_variant) {
	switch (p_variant.get_type()) {
		case Variant::ARRAY: {
			return _convert_array<DA, Array>(p_variant.operator Array());
		}
		case Variant::PACKED_BYTE_ARRAY: {
			return _convert_array<DA, PackedByteArray>(p_variant.operator PackedByteArray());
		}
		case Variant::PACKED_INT32_ARRAY: {
			return _convert_array<DA, PackedInt32Array>(p_variant.operator PackedInt32Array());
		}
		case Variant::PACKED_INT64_ARRAY: {
			return _convert_array<DA, PackedInt64Array>(p_variant.operator PackedInt64Array());
		}
		case Variant::PACKED_FLOAT32_ARRAY: {
			return _convert_array<DA, PackedFloat32Array>(p_variant.operator PackedFloat32Array());
		}
		case Variant::PACKED_FLOAT64_ARRAY: {
			return _convert_array<DA, PackedFloat64Array>(p_variant.operator PackedFloat64Array());
		}
		case Variant::PACKED_STRING_ARRAY: {
			return _convert_array<DA, PackedStringArray>(p_variant.operator PackedStringArray());
		}
		case Variant::PACKED_VECTOR2_ARRAY: {
			return _convert_array<DA, PackedVector2Array>(p_variant.operator PackedVector2Array());
		}
		case Variant::PACKED_VECTOR3_ARRAY: {
			return _convert_array<DA, PackedVector3Array>(p_variant.operator PackedVector3Array());
		}
		case Variant::PACKED_COLOR_ARRAY: {
			return _convert_array<DA, PackedColorArray>(p_variant.operator PackedColorArray());
		}
		case Variant::PACKED_VECTOR4_ARRAY: {
			return _convert_array<DA, PackedVector4Array>(p_variant.operator PackedVector4Array());
		}
		default: {
			return DA();
		}
	}
}

Variant::operator Array() const {
	if (type == ARRAY) {
		return *reinterpret_cast<const Array *>(_data._mem);
	} else {
		return _convert_array_from_variant<Array>(*this);
	}
}

Variant::operator PackedByteArray() const {
	if (type == PACKED_BYTE_ARRAY) {
		return static_cast<PackedArrayRef<uint8_t> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedByteArray>(*this);
	}
}

Variant::operator PackedInt32Array() const {
	if (type == PACKED_INT32_ARRAY) {
		return static_cast<PackedArrayRef<int32_t> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedInt32Array>(*this);
	}
}

Variant::operator PackedInt64Array() const {
	if (type == PACKED_INT64_ARRAY) {
		return static_cast<PackedArrayRef<int64_t> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedInt64Array>(*this);
	}
}

Variant::operator PackedFloat32Array() const {
	if (type == PACKED_FLOAT32_ARRAY) {
		return static_cast<PackedArrayRef<float> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedFloat32Array>(*this);
	}
}

Variant::operator PackedFloat64Array() const {
	if (type == PACKED_FLOAT64_ARRAY) {
		return static_cast<PackedArrayRef<double> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedFloat64Array>(*this);
	}
}

Variant::operator PackedStringArray() const {
	if (type == PACKED_STRING_ARRAY) {
		return static_cast<PackedArrayRef<String> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedStringArray>(*this);
	}
}

Variant::operator PackedVector2Array() const {
	if (type == PACKED_VECTOR2_ARRAY) {
		return static_cast<PackedArrayRef<Vector2> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedVector2Array>(*this);
	}
}

Variant::operator PackedVector3Array() const {
	if (type == PACKED_VECTOR3_ARRAY) {
		return static_cast<PackedArrayRef<Vector3> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedVector3Array>(*this);
	}
}

Variant::operator PackedColorArray() const {
	if (type == PACKED_COLOR_ARRAY) {
		return static_cast<PackedArrayRef<Color> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedColorArray>(*this);
	}
}

Variant::operator PackedVector4Array() const {
	if (type == PACKED_VECTOR4_ARRAY) {
		return static_cast<PackedArrayRef<Vector4> *>(_data.packed_array)->array;
	} else {
		return _convert_array_from_variant<PackedVector4Array>(*this);
	}
}

/* helpers */

Variant::operator Vector<::RID>() const {
	Array va = operator Array();
	Vector<::RID> rids;
	rids.resize(va.size());
	for (int i = 0; i < rids.size(); i++) {
		rids.write[i] = va[i];
	}
	return rids;
}

Variant::operator Vector<Plane>() const {
	Array va = operator Array();
	Vector<Plane> planes;
	int va_size = va.size();
	if (va_size == 0) {
		return planes;
	}

	planes.resize(va_size);
	Plane *w = planes.ptrw();

	for (int i = 0; i < va_size; i++) {
		w[i] = va[i];
	}

	return planes;
}

Variant::operator Vector<Face3>() const {
	PackedVector3Array va = operator PackedVector3Array();
	Vector<Face3> faces;
	int va_size = va.size();
	if (va_size == 0) {
		return faces;
	}

	faces.resize(va_size / 3);
	Face3 *w = faces.ptrw();
	const Vector3 *r = va.ptr();

	for (int i = 0; i < va_size; i++) {
		w[i / 3].vertex[i % 3] = r[i];
	}

	return faces;
}

Variant::operator Vector<Variant>() const {
	Array va = operator Array();
	Vector<Variant> variants;
	int va_size = va.size();
	if (va_size == 0) {
		return variants;
	}

	variants.resize(va_size);
	Variant *w = variants.ptrw();
	for (int i = 0; i < va_size; i++) {
		w[i] = va[i];
	}

	return variants;
}

Variant::operator Vector<StringName>() const {
	PackedStringArray from = operator PackedStringArray();
	Vector<StringName> to;
	int len = from.size();
	to.resize(len);
	for (int i = 0; i < len; i++) {
		to.write[i] = from[i];
	}
	return to;
}

Variant::operator Side() const {
	return (Side) operator int();
}

Variant::operator Orientation() const {
	return (Orientation) operator int();
}

Variant::operator IPAddress() const {
	if (type == PACKED_FLOAT32_ARRAY || type == PACKED_INT32_ARRAY || type == PACKED_FLOAT64_ARRAY || type == PACKED_INT64_ARRAY || type == PACKED_BYTE_ARRAY) {
		Vector<int> addr = operator Vector<int>();
		if (addr.size() == 4) {
			return IPAddress(addr.get(0), addr.get(1), addr.get(2), addr.get(3));
		}
	}

	return IPAddress(operator String());
}

Variant::Variant(bool p_bool) :
		type(BOOL) {
	_data._bool = p_bool;
}

Variant::Variant(int64_t p_int64) :
		type(INT) {
	_data._int = p_int64;
}

Variant::Variant(int32_t p_int32) :
		type(INT) {
	_data._int = p_int32;
}

Variant::Variant(int16_t p_int16) :
		type(INT) {
	_data._int = p_int16;
}

Variant::Variant(int8_t p_int8) :
		type(INT) {
	_data._int = p_int8;
}

Variant::Variant(uint64_t p_uint64) :
		type(INT) {
	_data._int = int64_t(p_uint64);
}

Variant::Variant(uint32_t p_uint32) :
		type(INT) {
	_data._int = int64_t(p_uint32);
}

Variant::Variant(uint16_t p_uint16) :
		type(INT) {
	_data._int = int64_t(p_uint16);
}

Variant::Variant(uint8_t p_uint8) :
		type(INT) {
	_data._int = int64_t(p_uint8);
}

Variant::Variant(float p_float) :
		type(FLOAT) {
	_data._float = p_float;
}

Variant::Variant(double p_double) :
		type(FLOAT) {
	_data._float = p_double;
}

Variant::Variant(const ObjectID &p_id) :
		type(INT) {
	_data._int = int64_t(p_id);
}

Variant::Variant(const StringName &p_string) :
		type(STRING_NAME) {
	memnew_placement(_data._mem, StringName(p_string));
}

Variant::Variant(const String &p_string) :
		type(STRING) {
	memnew_placement(_data._mem, String(p_string));
}

Variant::Variant(const char *const p_cstring) :
		type(STRING) {
	memnew_placement(_data._mem, String((const char *)p_cstring));
}

Variant::Variant(const char32_t *p_wstring) :
		type(STRING) {
	memnew_placement(_data._mem, String(p_wstring));
}

Variant::Variant(const Vector3 &p_vector3) :
		type(VECTOR3) {
	memnew_placement(_data._mem, Vector3(p_vector3));
}

Variant::Variant(const Vector3i &p_vector3i) :
		type(VECTOR3I) {
	memnew_placement(_data._mem, Vector3i(p_vector3i));
}

Variant::Variant(const Vector4 &p_vector4) :
		type(VECTOR4) {
	memnew_placement(_data._mem, Vector4(p_vector4));
}

Variant::Variant(const Vector4i &p_vector4i) :
		type(VECTOR4I) {
	memnew_placement(_data._mem, Vector4i(p_vector4i));
}

Variant::Variant(const Vector2 &p_vector2) :
		type(VECTOR2) {
	memnew_placement(_data._mem, Vector2(p_vector2));
}

Variant::Variant(const Vector2i &p_vector2i) :
		type(VECTOR2I) {
	memnew_placement(_data._mem, Vector2i(p_vector2i));
}

Variant::Variant(const Rect2 &p_rect2) :
		type(RECT2) {
	memnew_placement(_data._mem, Rect2(p_rect2));
}

Variant::Variant(const Rect2i &p_rect2i) :
		type(RECT2I) {
	memnew_placement(_data._mem, Rect2i(p_rect2i));
}

Variant::Variant(const Plane &p_plane) :
		type(PLANE) {
	memnew_placement(_data._mem, Plane(p_plane));
}

Variant::Variant(const ::AABB &p_aabb) :
		type(AABB) {
	_data._aabb = (::AABB *)Pools::_bucket_small.alloc();
	memnew_placement(_data._aabb, ::AABB(p_aabb));
}

Variant::Variant(const Basis &p_matrix) :
		type(BASIS) {
	_data._basis = (Basis *)Pools::_bucket_medium.alloc();
	memnew_placement(_data._basis, Basis(p_matrix));
}

Variant::Variant(const Quaternion &p_quaternion) :
		type(QUATERNION) {
	memnew_placement(_data._mem, Quaternion(p_quaternion));
}

Variant::Variant(const Transform3D &p_transform) :
		type(TRANSFORM3D) {
	_data._transform3d = (Transform3D *)Pools::_bucket_medium.alloc();
	memnew_placement(_data._transform3d, Transform3D(p_transform));
}

Variant::Variant(const Projection &pp_projection) :
		type(PROJECTION) {
	_data._projection = (Projection *)Pools::_bucket_large.alloc();
	memnew_placement(_data._projection, Projection(pp_projection));
}

Variant::Variant(const Transform2D &p_transform) :
		type(TRANSFORM2D) {
	_data._transform2d = (Transform2D *)Pools::_bucket_small.alloc();
	memnew_placement(_data._transform2d, Transform2D(p_transform));
}

Variant::Variant(const Color &p_color) :
		type(COLOR) {
	memnew_placement(_data._mem, Color(p_color));
}

Variant::Variant(const NodePath &p_node_path) :
		type(NODE_PATH) {
	memnew_placement(_data._mem, NodePath(p_node_path));
}

Variant::Variant(const ::RID &p_rid) :
		type(RID) {
	memnew_placement(_data._mem, ::RID(p_rid));
}

Variant::Variant(const Object *p_object) :
		type(OBJECT) {
	_get_obj() = ObjData();
	_get_obj().ref_pointer(const_cast<Object *>(p_object));
}

Variant::Variant(const Callable &p_callable) :
		type(CALLABLE) {
	memnew_placement(_data._mem, Callable(p_callable));
}

Variant::Variant(const Signal &p_callable) :
		type(SIGNAL) {
	memnew_placement(_data._mem, Signal(p_callable));
}

Variant::Variant(const Dictionary &p_dictionary) :
		type(DICTIONARY) {
	memnew_placement(_data._mem, Dictionary(p_dictionary));
}

Variant::Variant(const Array &p_array) :
		type(ARRAY) {
	memnew_placement(_data._mem, Array(p_array));
}

Variant::Variant(const PackedByteArray &p_byte_array) :
		type(PACKED_BYTE_ARRAY) {
	_data.packed_array = PackedArrayRef<uint8_t>::create(p_byte_array);
}

Variant::Variant(const PackedInt32Array &p_int32_array) :
		type(PACKED_INT32_ARRAY) {
	_data.packed_array = PackedArrayRef<int32_t>::create(p_int32_array);
}

Variant::Variant(const PackedInt64Array &p_int64_array) :
		type(PACKED_INT64_ARRAY) {
	_data.packed_array = PackedArrayRef<int64_t>::create(p_int64_array);
}

Variant::Variant(const PackedFloat32Array &p_float32_array) :
		type(PACKED_FLOAT32_ARRAY) {
	_data.packed_array = PackedArrayRef<float>::create(p_float32_array);
}

Variant::Variant(const PackedFloat64Array &p_float64_array) :
		type(PACKED_FLOAT64_ARRAY) {
	_data.packed_array = PackedArrayRef<double>::create(p_float64_array);
}

Variant::Variant(const PackedStringArray &p_string_array) :
		type(PACKED_STRING_ARRAY) {
	_data.packed_array = PackedArrayRef<String>::create(p_string_array);
}

Variant::Variant(const PackedVector2Array &p_vector2_array) :
		type(PACKED_VECTOR2_ARRAY) {
	_data.packed_array = PackedArrayRef<Vector2>::create(p_vector2_array);
}

Variant::Variant(const PackedVector3Array &p_vector3_array) :
		type(PACKED_VECTOR3_ARRAY) {
	_data.packed_array = PackedArrayRef<Vector3>::create(p_vector3_array);
}

Variant::Variant(const PackedColorArray &p_color_array) :
		type(PACKED_COLOR_ARRAY) {
	_data.packed_array = PackedArrayRef<Color>::create(p_color_array);
}

Variant::Variant(const PackedVector4Array &p_vector4_array) :
		type(PACKED_VECTOR4_ARRAY) {
	_data.packed_array = PackedArrayRef<Vector4>::create(p_vector4_array);
}

/* helpers */
Variant::Variant(const Vector<::RID> &p_array) :
		type(ARRAY) {
	Array *rid_array = memnew_placement(_data._mem, Array);

	rid_array->resize(p_array.size());

	for (int i = 0; i < p_array.size(); i++) {
		rid_array->set(i, Variant(p_array[i]));
	}
}

Variant::Variant(const Vector<Plane> &p_array) :
		type(ARRAY) {
	Array *plane_array = memnew_placement(_data._mem, Array);

	plane_array->resize(p_array.size());

	for (int i = 0; i < p_array.size(); i++) {
		plane_array->operator[](i) = Variant(p_array[i]);
	}
}

Variant::Variant(const Vector<Face3> &p_face_array) {
	PackedVector3Array vertices;
	int face_count = p_face_array.size();
	vertices.resize(face_count * 3);

	if (face_count) {
		const Face3 *r = p_face_array.ptr();
		Vector3 *w = vertices.ptrw();

		for (int i = 0; i < face_count; i++) {
			for (int j = 0; j < 3; j++) {
				w[i * 3 + j] = r[i].vertex[j];
			}
		}
	}

	*this = vertices;
}

Variant::Variant(const Vector<Variant> &p_array) {
	Array arr;
	arr.resize(p_array.size());
	for (int i = 0; i < p_array.size(); i++) {
		arr[i] = p_array[i];
	}
	*this = arr;
}

Variant::Variant(const Vector<StringName> &p_array) {
	PackedStringArray v;
	int len = p_array.size();
	v.resize(len);
	for (int i = 0; i < len; i++) {
		v.set(i, p_array[i]);
	}
	*this = v;
}

void Variant::operator=(const Variant &p_variant) {
	if (unlikely(this == &p_variant)) {
		return;
	}

	if (unlikely(type != p_variant.type)) {
		reference(p_variant);
		return;
	}

	switch (p_variant.type) {
		case NIL: {
			// none
		} break;

		// atomic types
		case BOOL: {
			_data._bool = p_variant._data._bool;
		} break;
		case INT: {
			_data._int = p_variant._data._int;
		} break;
		case FLOAT: {
			_data._float = p_variant._data._float;
		} break;
		case STRING: {
			*reinterpret_cast<String *>(_data._mem) = *reinterpret_cast<const String *>(p_variant._data._mem);
		} break;

		// math types
		case VECTOR2: {
			*reinterpret_cast<Vector2 *>(_data._mem) = *reinterpret_cast<const Vector2 *>(p_variant._data._mem);
		} break;
		case VECTOR2I: {
			*reinterpret_cast<Vector2i *>(_data._mem) = *reinterpret_cast<const Vector2i *>(p_variant._data._mem);
		} break;
		case RECT2: {
			*reinterpret_cast<Rect2 *>(_data._mem) = *reinterpret_cast<const Rect2 *>(p_variant._data._mem);
		} break;
		case RECT2I: {
			*reinterpret_cast<Rect2i *>(_data._mem) = *reinterpret_cast<const Rect2i *>(p_variant._data._mem);
		} break;
		case TRANSFORM2D: {
			*_data._transform2d = *(p_variant._data._transform2d);
		} break;
		case VECTOR3: {
			*reinterpret_cast<Vector3 *>(_data._mem) = *reinterpret_cast<const Vector3 *>(p_variant._data._mem);
		} break;
		case VECTOR3I: {
			*reinterpret_cast<Vector3i *>(_data._mem) = *reinterpret_cast<const Vector3i *>(p_variant._data._mem);
		} break;
		case VECTOR4: {
			*reinterpret_cast<Vector4 *>(_data._mem) = *reinterpret_cast<const Vector4 *>(p_variant._data._mem);
		} break;
		case VECTOR4I: {
			*reinterpret_cast<Vector4i *>(_data._mem) = *reinterpret_cast<const Vector4i *>(p_variant._data._mem);
		} break;
		case PLANE: {
			*reinterpret_cast<Plane *>(_data._mem) = *reinterpret_cast<const Plane *>(p_variant._data._mem);
		} break;

		case AABB: {
			*_data._aabb = *(p_variant._data._aabb);
		} break;
		case QUATERNION: {
			*reinterpret_cast<Quaternion *>(_data._mem) = *reinterpret_cast<const Quaternion *>(p_variant._data._mem);
		} break;
		case BASIS: {
			*_data._basis = *(p_variant._data._basis);
		} break;
		case TRANSFORM3D: {
			*_data._transform3d = *(p_variant._data._transform3d);
		} break;
		case PROJECTION: {
			*_data._projection = *(p_variant._data._projection);
		} break;

		// misc types
		case COLOR: {
			*reinterpret_cast<Color *>(_data._mem) = *reinterpret_cast<const Color *>(p_variant._data._mem);
		} break;
		case RID: {
			*reinterpret_cast<::RID *>(_data._mem) = *reinterpret_cast<const ::RID *>(p_variant._data._mem);
		} break;
		case OBJECT: {
			_get_obj().ref(p_variant._get_obj());
		} break;
		case CALLABLE: {
			*reinterpret_cast<Callable *>(_data._mem) = *reinterpret_cast<const Callable *>(p_variant._data._mem);
		} break;
		case SIGNAL: {
			*reinterpret_cast<Signal *>(_data._mem) = *reinterpret_cast<const Signal *>(p_variant._data._mem);
		} break;

		case STRING_NAME: {
			*reinterpret_cast<StringName *>(_data._mem) = *reinterpret_cast<const StringName *>(p_variant._data._mem);
		} break;
		case NODE_PATH: {
			*reinterpret_cast<NodePath *>(_data._mem) = *reinterpret_cast<const NodePath *>(p_variant._data._mem);
		} break;
		case DICTIONARY: {
			*reinterpret_cast<Dictionary *>(_data._mem) = *reinterpret_cast<const Dictionary *>(p_variant._data._mem);
		} break;
		case ARRAY: {
			*reinterpret_cast<Array *>(_data._mem) = *reinterpret_cast<const Array *>(p_variant._data._mem);
		} break;

		// arrays
		case PACKED_BYTE_ARRAY: {
			_data.packed_array = PackedArrayRef<uint8_t>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_INT32_ARRAY: {
			_data.packed_array = PackedArrayRef<int32_t>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_INT64_ARRAY: {
			_data.packed_array = PackedArrayRef<int64_t>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_FLOAT32_ARRAY: {
			_data.packed_array = PackedArrayRef<float>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_FLOAT64_ARRAY: {
			_data.packed_array = PackedArrayRef<double>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_STRING_ARRAY: {
			_data.packed_array = PackedArrayRef<String>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_VECTOR2_ARRAY: {
			_data.packed_array = PackedArrayRef<Vector2>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_VECTOR3_ARRAY: {
			_data.packed_array = PackedArrayRef<Vector3>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_COLOR_ARRAY: {
			_data.packed_array = PackedArrayRef<Color>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		case PACKED_VECTOR4_ARRAY: {
			_data.packed_array = PackedArrayRef<Vector4>::reference_from(_data.packed_array, p_variant._data.packed_array);
		} break;
		default: {
		}
	}
}

Variant::Variant(const IPAddress &p_address) :
		type(STRING) {
	memnew_placement(_data._mem, String(p_address));
}

Variant::Variant(const Variant &p_variant) {
	reference(p_variant);
}

uint32_t Variant::hash() const {
	return recursive_hash(0);
}

uint32_t Variant::recursive_hash(int recursion_count) const {
	switch (type) {
		case NIL: {
			return 0;
		} break;
		case BOOL: {
			return _data._bool ? 1 : 0;
		} break;
		case INT: {
			return hash_one_uint64((uint64_t)_data._int);
		} break;
		case FLOAT: {
			return hash_murmur3_one_double(_data._float);
		} break;
		case STRING: {
			return reinterpret_cast<const String *>(_data._mem)->hash();
		} break;

		// math types
		case VECTOR2: {
			return HashMapHasherDefault::hash(*reinterpret_cast<const Vector2 *>(_data._mem));
		} break;
		case VECTOR2I: {
			return HashMapHasherDefault::hash(*reinterpret_cast<const Vector2i *>(_data._mem));
		} break;
		case RECT2: {
			return HashMapHasherDefault::hash(*reinterpret_cast<const Rect2 *>(_data._mem));
		} break;
		case RECT2I: {
			return HashMapHasherDefault::hash(*reinterpret_cast<const Rect2i *>(_data._mem));
		} break;
		case TRANSFORM2D: {
			uint32_t h = HASH_MURMUR3_SEED;
			const Transform2D &t = *_data._transform2d;
			h = hash_murmur3_one_real(t[0].x, h);
			h = hash_murmur3_one_real(t[0].y, h);
			h = hash_murmur3_one_real(t[1].x, h);
			h = hash_murmur3_one_real(t[1].y, h);
			h = hash_murmur3_one_real(t[2].x, h);
			h = hash_murmur3_one_real(t[2].y, h);

			return hash_fmix32(h);
		} break;
		case VECTOR3: {
			return HashMapHasherDefault::hash(*reinterpret_cast<const Vector3 *>(_data._mem));
		} break;
		case VECTOR3I: {
			return HashMapHasherDefault::hash(*reinterpret_cast<const Vector3i *>(_data._mem));
		} break;
		case VECTOR4: {
			return HashMapHasherDefault::hash(*reinterpret_cast<const Vector4 *>(_data._mem));
		} break;
		case VECTOR4I: {
			return HashMapHasherDefault::hash(*reinterpret_cast<const Vector4i *>(_data._mem));
		} break;
		case PLANE: {
			uint32_t h = HASH_MURMUR3_SEED;
			const Plane &p = *reinterpret_cast<const Plane *>(_data._mem);
			h = hash_murmur3_one_real(p.normal.x, h);
			h = hash_murmur3_one_real(p.normal.y, h);
			h = hash_murmur3_one_real(p.normal.z, h);
			h = hash_murmur3_one_real(p.d, h);
			return hash_fmix32(h);
		} break;
		case AABB: {
			return HashMapHasherDefault::hash(*_data._aabb);
		} break;
		case QUATERNION: {
			uint32_t h = HASH_MURMUR3_SEED;
			const Quaternion &q = *reinterpret_cast<const Quaternion *>(_data._mem);
			h = hash_murmur3_one_real(q.x, h);
			h = hash_murmur3_one_real(q.y, h);
			h = hash_murmur3_one_real(q.z, h);
			h = hash_murmur3_one_real(q.w, h);
			return hash_fmix32(h);
		} break;
		case BASIS: {
			uint32_t h = HASH_MURMUR3_SEED;
			const Basis &b = *_data._basis;
			h = hash_murmur3_one_real(b[0].x, h);
			h = hash_murmur3_one_real(b[0].y, h);
			h = hash_murmur3_one_real(b[0].z, h);
			h = hash_murmur3_one_real(b[1].x, h);
			h = hash_murmur3_one_real(b[1].y, h);
			h = hash_murmur3_one_real(b[1].z, h);
			h = hash_murmur3_one_real(b[2].x, h);
			h = hash_murmur3_one_real(b[2].y, h);
			h = hash_murmur3_one_real(b[2].z, h);
			return hash_fmix32(h);
		} break;
		case TRANSFORM3D: {
			uint32_t h = HASH_MURMUR3_SEED;
			const Transform3D &t = *_data._transform3d;
			h = hash_murmur3_one_real(t.basis[0].x, h);
			h = hash_murmur3_one_real(t.basis[0].y, h);
			h = hash_murmur3_one_real(t.basis[0].z, h);
			h = hash_murmur3_one_real(t.basis[1].x, h);
			h = hash_murmur3_one_real(t.basis[1].y, h);
			h = hash_murmur3_one_real(t.basis[1].z, h);
			h = hash_murmur3_one_real(t.basis[2].x, h);
			h = hash_murmur3_one_real(t.basis[2].y, h);
			h = hash_murmur3_one_real(t.basis[2].z, h);
			h = hash_murmur3_one_real(t.origin.x, h);
			h = hash_murmur3_one_real(t.origin.y, h);
			h = hash_murmur3_one_real(t.origin.z, h);
			return hash_fmix32(h);
		} break;
		case PROJECTION: {
			uint32_t h = HASH_MURMUR3_SEED;
			const Projection &t = *_data._projection;
			h = hash_murmur3_one_real(t.columns[0].x, h);
			h = hash_murmur3_one_real(t.columns[0].y, h);
			h = hash_murmur3_one_real(t.columns[0].z, h);
			h = hash_murmur3_one_real(t.columns[0].w, h);
			h = hash_murmur3_one_real(t.columns[1].x, h);
			h = hash_murmur3_one_real(t.columns[1].y, h);
			h = hash_murmur3_one_real(t.columns[1].z, h);
			h = hash_murmur3_one_real(t.columns[1].w, h);
			h = hash_murmur3_one_real(t.columns[2].x, h);
			h = hash_murmur3_one_real(t.columns[2].y, h);
			h = hash_murmur3_one_real(t.columns[2].z, h);
			h = hash_murmur3_one_real(t.columns[2].w, h);
			h = hash_murmur3_one_real(t.columns[3].x, h);
			h = hash_murmur3_one_real(t.columns[3].y, h);
			h = hash_murmur3_one_real(t.columns[3].z, h);
			h = hash_murmur3_one_real(t.columns[3].w, h);
			return hash_fmix32(h);
		} break;
		// misc types
		case COLOR: {
			uint32_t h = HASH_MURMUR3_SEED;
			const Color &c = *reinterpret_cast<const Color *>(_data._mem);
			h = hash_murmur3_one_float(c.r, h);
			h = hash_murmur3_one_float(c.g, h);
			h = hash_murmur3_one_float(c.b, h);
			h = hash_murmur3_one_float(c.a, h);
			return hash_fmix32(h);
		} break;
		case RID: {
			return hash_one_uint64(reinterpret_cast<const ::RID *>(_data._mem)->get_id());
		} break;
		case OBJECT: {
			return hash_one_uint64(hash_make_uint64_t(_get_obj().obj));
		} break;
		case STRING_NAME: {
			return reinterpret_cast<const StringName *>(_data._mem)->hash();
		} break;
		case NODE_PATH: {
			return reinterpret_cast<const NodePath *>(_data._mem)->hash();
		} break;
		case DICTIONARY: {
			return reinterpret_cast<const Dictionary *>(_data._mem)->recursive_hash(recursion_count);

		} break;
		case CALLABLE: {
			return reinterpret_cast<const Callable *>(_data._mem)->hash();

		} break;
		case SIGNAL: {
			const Signal &s = *reinterpret_cast<const Signal *>(_data._mem);
			uint32_t hash = s.get_name().hash();
			return hash_murmur3_one_64(s.get_object_id(), hash);
		} break;
		case ARRAY: {
			const Array &arr = *reinterpret_cast<const Array *>(_data._mem);
			return arr.recursive_hash(recursion_count);

		} break;
		case PACKED_BYTE_ARRAY: {
			const PackedByteArray &arr = PackedArrayRef<uint8_t>::get_array(_data.packed_array);
			int len = arr.size();
			if (likely(len)) {
				const uint8_t *r = arr.ptr();
				return hash_murmur3_buffer((uint8_t *)&r[0], len);
			} else {
				return hash_murmur3_one_64(0);
			}

		} break;
		case PACKED_INT32_ARRAY: {
			const PackedInt32Array &arr = PackedArrayRef<int32_t>::get_array(_data.packed_array);
			int len = arr.size();
			if (likely(len)) {
				const int32_t *r = arr.ptr();
				return hash_murmur3_buffer((uint8_t *)&r[0], len * sizeof(int32_t));
			} else {
				return hash_murmur3_one_64(0);
			}

		} break;
		case PACKED_INT64_ARRAY: {
			const PackedInt64Array &arr = PackedArrayRef<int64_t>::get_array(_data.packed_array);
			int len = arr.size();
			if (likely(len)) {
				const int64_t *r = arr.ptr();
				return hash_murmur3_buffer((uint8_t *)&r[0], len * sizeof(int64_t));
			} else {
				return hash_murmur3_one_64(0);
			}

		} break;
		case PACKED_FLOAT32_ARRAY: {
			const PackedFloat32Array &arr = PackedArrayRef<float>::get_array(_data.packed_array);
			int len = arr.size();

			if (likely(len)) {
				const float *r = arr.ptr();
				uint32_t h = HASH_MURMUR3_SEED;
				for (int32_t i = 0; i < len; i++) {
					h = hash_murmur3_one_float(r[i], h);
				}
				return hash_fmix32(h);
			} else {
				return hash_murmur3_one_float(0.0);
			}

		} break;
		case PACKED_FLOAT64_ARRAY: {
			const PackedFloat64Array &arr = PackedArrayRef<double>::get_array(_data.packed_array);
			int len = arr.size();

			if (likely(len)) {
				const double *r = arr.ptr();
				uint32_t h = HASH_MURMUR3_SEED;
				for (int32_t i = 0; i < len; i++) {
					h = hash_murmur3_one_double(r[i], h);
				}
				return hash_fmix32(h);
			} else {
				return hash_murmur3_one_double(0.0);
			}

		} break;
		case PACKED_STRING_ARRAY: {
			uint32_t hash = HASH_MURMUR3_SEED;
			const PackedStringArray &arr = PackedArrayRef<String>::get_array(_data.packed_array);
			int len = arr.size();

			if (likely(len)) {
				const String *r = arr.ptr();

				for (int i = 0; i < len; i++) {
					hash = hash_murmur3_one_32(r[i].hash(), hash);
				}
				hash = hash_fmix32(hash);
			}

			return hash;
		} break;
		case PACKED_VECTOR2_ARRAY: {
			uint32_t hash = HASH_MURMUR3_SEED;
			const PackedVector2Array &arr = PackedArrayRef<Vector2>::get_array(_data.packed_array);
			int len = arr.size();

			if (likely(len)) {
				const Vector2 *r = arr.ptr();

				for (int i = 0; i < len; i++) {
					hash = hash_murmur3_one_real(r[i].x, hash);
					hash = hash_murmur3_one_real(r[i].y, hash);
				}
				hash = hash_fmix32(hash);
			}

			return hash;
		} break;
		case PACKED_VECTOR3_ARRAY: {
			uint32_t hash = HASH_MURMUR3_SEED;
			const PackedVector3Array &arr = PackedArrayRef<Vector3>::get_array(_data.packed_array);
			int len = arr.size();

			if (likely(len)) {
				const Vector3 *r = arr.ptr();

				for (int i = 0; i < len; i++) {
					hash = hash_murmur3_one_real(r[i].x, hash);
					hash = hash_murmur3_one_real(r[i].y, hash);
					hash = hash_murmur3_one_real(r[i].z, hash);
				}
				hash = hash_fmix32(hash);
			}

			return hash;
		} break;
		case PACKED_COLOR_ARRAY: {
			uint32_t hash = HASH_MURMUR3_SEED;
			const PackedColorArray &arr = PackedArrayRef<Color>::get_array(_data.packed_array);
			int len = arr.size();

			if (likely(len)) {
				const Color *r = arr.ptr();

				for (int i = 0; i < len; i++) {
					hash = hash_murmur3_one_float(r[i].r, hash);
					hash = hash_murmur3_one_float(r[i].g, hash);
					hash = hash_murmur3_one_float(r[i].b, hash);
					hash = hash_murmur3_one_float(r[i].a, hash);
				}
				hash = hash_fmix32(hash);
			}

			return hash;
		} break;
		case PACKED_VECTOR4_ARRAY: {
			uint32_t hash = HASH_MURMUR3_SEED;
			const PackedVector4Array &arr = PackedArrayRef<Vector4>::get_array(_data.packed_array);
			int len = arr.size();

			if (likely(len)) {
				const Vector4 *r = arr.ptr();

				for (int i = 0; i < len; i++) {
					hash = hash_murmur3_one_real(r[i].x, hash);
					hash = hash_murmur3_one_real(r[i].y, hash);
					hash = hash_murmur3_one_real(r[i].z, hash);
					hash = hash_murmur3_one_real(r[i].w, hash);
				}
				hash = hash_fmix32(hash);
			}

			return hash;
		} break;
		default: {
		}
	}

	return 0;
}

#define hash_compare_scalar_base(p_lhs, p_rhs, semantic_comparison) \
	(((p_lhs) == (p_rhs)) || (semantic_comparison && Math::is_nan(p_lhs) && Math::is_nan(p_rhs)))

#define hash_compare_scalar(p_lhs, p_rhs) \
	(hash_compare_scalar_base(p_lhs, p_rhs, true))

#define hash_compare_vector2(p_lhs, p_rhs)        \
	(hash_compare_scalar((p_lhs).x, (p_rhs).x) && \
			hash_compare_scalar((p_lhs).y, (p_rhs).y))

#define hash_compare_vector3(p_lhs, p_rhs)               \
	(hash_compare_scalar((p_lhs).x, (p_rhs).x) &&        \
			hash_compare_scalar((p_lhs).y, (p_rhs).y) && \
			hash_compare_scalar((p_lhs).z, (p_rhs).z))

#define hash_compare_vector4(p_lhs, p_rhs)               \
	(hash_compare_scalar((p_lhs).x, (p_rhs).x) &&        \
			hash_compare_scalar((p_lhs).y, (p_rhs).y) && \
			hash_compare_scalar((p_lhs).z, (p_rhs).z) && \
			hash_compare_scalar((p_lhs).w, (p_rhs).w))

#define hash_compare_quaternion(p_lhs, p_rhs)            \
	(hash_compare_scalar((p_lhs).x, (p_rhs).x) &&        \
			hash_compare_scalar((p_lhs).y, (p_rhs).y) && \
			hash_compare_scalar((p_lhs).z, (p_rhs).z) && \
			hash_compare_scalar((p_lhs).w, (p_rhs).w))

#define hash_compare_color(p_lhs, p_rhs)                 \
	(hash_compare_scalar((p_lhs).r, (p_rhs).r) &&        \
			hash_compare_scalar((p_lhs).g, (p_rhs).g) && \
			hash_compare_scalar((p_lhs).b, (p_rhs).b) && \
			hash_compare_scalar((p_lhs).a, (p_rhs).a))

#define hash_compare_packed_array(p_lhs, p_rhs, p_type, p_compare_func) \
	const Vector<p_type> &l = PackedArrayRef<p_type>::get_array(p_lhs); \
	const Vector<p_type> &r = PackedArrayRef<p_type>::get_array(p_rhs); \
                                                                        \
	if (l.size() != r.size())                                           \
		return false;                                                   \
                                                                        \
	const p_type *lr = l.ptr();                                         \
	const p_type *rr = r.ptr();                                         \
                                                                        \
	for (int i = 0; i < l.size(); ++i) {                                \
		if (!p_compare_func((lr[i]), (rr[i])))                          \
			return false;                                               \
	}                                                                   \
                                                                        \
	return true

bool Variant::hash_compare(const Variant &p_variant, int recursion_count, bool semantic_comparison) const {
	if (type != p_variant.type) {
		return false;
	}

	switch (type) {
		case INT: {
			return _data._int == p_variant._data._int;
		} break;

		case FLOAT: {
			return hash_compare_scalar_base(_data._float, p_variant._data._float, semantic_comparison);
		} break;

		case STRING: {
			return *reinterpret_cast<const String *>(_data._mem) == *reinterpret_cast<const String *>(p_variant._data._mem);
		} break;

		case STRING_NAME: {
			return *reinterpret_cast<const StringName *>(_data._mem) == *reinterpret_cast<const StringName *>(p_variant._data._mem);
		} break;

		case VECTOR2: {
			const Vector2 *l = reinterpret_cast<const Vector2 *>(_data._mem);
			const Vector2 *r = reinterpret_cast<const Vector2 *>(p_variant._data._mem);

			return hash_compare_vector2(*l, *r);
		} break;
		case VECTOR2I: {
			const Vector2i *l = reinterpret_cast<const Vector2i *>(_data._mem);
			const Vector2i *r = reinterpret_cast<const Vector2i *>(p_variant._data._mem);
			return *l == *r;
		} break;

		case RECT2: {
			const Rect2 *l = reinterpret_cast<const Rect2 *>(_data._mem);
			const Rect2 *r = reinterpret_cast<const Rect2 *>(p_variant._data._mem);

			return hash_compare_vector2(l->position, r->position) &&
					hash_compare_vector2(l->size, r->size);
		} break;
		case RECT2I: {
			const Rect2i *l = reinterpret_cast<const Rect2i *>(_data._mem);
			const Rect2i *r = reinterpret_cast<const Rect2i *>(p_variant._data._mem);

			return *l == *r;
		} break;

		case TRANSFORM2D: {
			Transform2D *l = _data._transform2d;
			Transform2D *r = p_variant._data._transform2d;

			for (int i = 0; i < 3; i++) {
				if (!hash_compare_vector2(l->columns[i], r->columns[i])) {
					return false;
				}
			}

			return true;
		} break;

		case VECTOR3: {
			const Vector3 *l = reinterpret_cast<const Vector3 *>(_data._mem);
			const Vector3 *r = reinterpret_cast<const Vector3 *>(p_variant._data._mem);

			return hash_compare_vector3(*l, *r);
		} break;
		case VECTOR3I: {
			const Vector3i *l = reinterpret_cast<const Vector3i *>(_data._mem);
			const Vector3i *r = reinterpret_cast<const Vector3i *>(p_variant._data._mem);

			return *l == *r;
		} break;
		case VECTOR4: {
			const Vector4 *l = reinterpret_cast<const Vector4 *>(_data._mem);
			const Vector4 *r = reinterpret_cast<const Vector4 *>(p_variant._data._mem);

			return hash_compare_vector4(*l, *r);
		} break;
		case VECTOR4I: {
			const Vector4i *l = reinterpret_cast<const Vector4i *>(_data._mem);
			const Vector4i *r = reinterpret_cast<const Vector4i *>(p_variant._data._mem);

			return *l == *r;
		} break;

		case PLANE: {
			const Plane *l = reinterpret_cast<const Plane *>(_data._mem);
			const Plane *r = reinterpret_cast<const Plane *>(p_variant._data._mem);

			return hash_compare_vector3(l->normal, r->normal) &&
					hash_compare_scalar(l->d, r->d);
		} break;

		case AABB: {
			const ::AABB *l = _data._aabb;
			const ::AABB *r = p_variant._data._aabb;

			return hash_compare_vector3(l->position, r->position) &&
					hash_compare_vector3(l->size, r->size);

		} break;

		case QUATERNION: {
			const Quaternion *l = reinterpret_cast<const Quaternion *>(_data._mem);
			const Quaternion *r = reinterpret_cast<const Quaternion *>(p_variant._data._mem);

			return hash_compare_quaternion(*l, *r);
		} break;

		case BASIS: {
			const Basis *l = _data._basis;
			const Basis *r = p_variant._data._basis;

			for (int i = 0; i < 3; i++) {
				if (!hash_compare_vector3(l->rows[i], r->rows[i])) {
					return false;
				}
			}

			return true;
		} break;

		case TRANSFORM3D: {
			const Transform3D *l = _data._transform3d;
			const Transform3D *r = p_variant._data._transform3d;

			for (int i = 0; i < 3; i++) {
				if (!hash_compare_vector3(l->basis.rows[i], r->basis.rows[i])) {
					return false;
				}
			}

			return hash_compare_vector3(l->origin, r->origin);
		} break;
		case PROJECTION: {
			const Projection *l = _data._projection;
			const Projection *r = p_variant._data._projection;

			for (int i = 0; i < 4; i++) {
				if (!hash_compare_vector4(l->columns[i], r->columns[i])) {
					return false;
				}
			}

			return true;
		} break;

		case COLOR: {
			const Color *l = reinterpret_cast<const Color *>(_data._mem);
			const Color *r = reinterpret_cast<const Color *>(p_variant._data._mem);

			return hash_compare_color(*l, *r);
		} break;

		case ARRAY: {
			const Array &l = *(reinterpret_cast<const Array *>(_data._mem));
			const Array &r = *(reinterpret_cast<const Array *>(p_variant._data._mem));

			if (!l.recursive_equal(r, recursion_count + 1)) {
				return false;
			}

			return true;
		} break;

		case DICTIONARY: {
			const Dictionary &l = *(reinterpret_cast<const Dictionary *>(_data._mem));
			const Dictionary &r = *(reinterpret_cast<const Dictionary *>(p_variant._data._mem));

			if (!l.recursive_equal(r, recursion_count + 1)) {
				return false;
			}

			return true;
		} break;

		// This is for floating point comparisons only.
		case PACKED_FLOAT32_ARRAY: {
			hash_compare_packed_array(_data.packed_array, p_variant._data.packed_array, float, hash_compare_scalar);
		} break;

		case PACKED_FLOAT64_ARRAY: {
			hash_compare_packed_array(_data.packed_array, p_variant._data.packed_array, double, hash_compare_scalar);
		} break;

		case PACKED_VECTOR2_ARRAY: {
			hash_compare_packed_array(_data.packed_array, p_variant._data.packed_array, Vector2, hash_compare_vector2);
		} break;

		case PACKED_VECTOR3_ARRAY: {
			hash_compare_packed_array(_data.packed_array, p_variant._data.packed_array, Vector3, hash_compare_vector3);
		} break;

		case PACKED_COLOR_ARRAY: {
			hash_compare_packed_array(_data.packed_array, p_variant._data.packed_array, Color, hash_compare_color);
		} break;

		case PACKED_VECTOR4_ARRAY: {
			hash_compare_packed_array(_data.packed_array, p_variant._data.packed_array, Vector4, hash_compare_vector4);
		} break;

		default:
			bool v;
			Variant r;
			evaluate(OP_EQUAL, *this, p_variant, r, v);
			return r;
	}
}

bool Variant::identity_compare(const Variant &p_variant) const {
	if (type != p_variant.type) {
		return false;
	}

	switch (type) {
		case OBJECT: {
			return _get_obj().id == p_variant._get_obj().id;
		} break;

		case DICTIONARY: {
			const Dictionary &l = *(reinterpret_cast<const Dictionary *>(_data._mem));
			const Dictionary &r = *(reinterpret_cast<const Dictionary *>(p_variant._data._mem));
			return l.id() == r.id();
		} break;

		case ARRAY: {
			const Array &l = *(reinterpret_cast<const Array *>(_data._mem));
			const Array &r = *(reinterpret_cast<const Array *>(p_variant._data._mem));
			return l.id() == r.id();
		} break;

		case PACKED_BYTE_ARRAY:
		case PACKED_INT32_ARRAY:
		case PACKED_INT64_ARRAY:
		case PACKED_FLOAT32_ARRAY:
		case PACKED_FLOAT64_ARRAY:
		case PACKED_STRING_ARRAY:
		case PACKED_VECTOR2_ARRAY:
		case PACKED_VECTOR3_ARRAY:
		case PACKED_COLOR_ARRAY:
		case PACKED_VECTOR4_ARRAY: {
			return _data.packed_array == p_variant._data.packed_array;
		} break;

		default: {
			return hash_compare(p_variant);
		}
	}
}

bool StringLikeVariantComparator::compare(const Variant &p_lhs, const Variant &p_rhs) {
	if (p_lhs.hash_compare(p_rhs)) {
		return true;
	}
	if (p_lhs.get_type() == Variant::STRING && p_rhs.get_type() == Variant::STRING_NAME) {
		return *VariantInternal::get_string(&p_lhs) == *VariantInternal::get_string_name(&p_rhs);
	}
	if (p_lhs.get_type() == Variant::STRING_NAME && p_rhs.get_type() == Variant::STRING) {
		return *VariantInternal::get_string_name(&p_lhs) == *VariantInternal::get_string(&p_rhs);
	}
	return false;
}

bool Variant::is_ref_counted() const {
	return type == OBJECT && _get_obj().id.is_ref_counted();
}

bool Variant::is_type_shared(Variant::Type p_type) {
	switch (p_type) {
		case OBJECT:
		case ARRAY:
		case DICTIONARY:
			return true;
		default: {
		}
	}

	return false;
}

bool Variant::is_shared() const {
	return is_type_shared(type);
}

bool Variant::is_read_only() const {
	switch (type) {
		case ARRAY:
			return reinterpret_cast<const Array *>(_data._mem)->is_read_only();
		case DICTIONARY:
			return reinterpret_cast<const Dictionary *>(_data._mem)->is_read_only();
		default:
			return false;
	}
}

void Variant::_variant_call_error(const String &p_method, Callable::CallError &error) {
	switch (error.error) {
		case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT: {
			String err = "Invalid type for argument #" + itos(error.argument) + ", expected '" + Variant::get_type_name(Variant::Type(error.expected)) + "'.";
			ERR_PRINT(err.utf8().get_data());

		} break;
		case Callable::CallError::CALL_ERROR_INVALID_METHOD: {
			String err = "Invalid method '" + p_method + "' for type '" + Variant::get_type_name(type) + "'.";
			ERR_PRINT(err.utf8().get_data());
		} break;
		case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS: {
			String err = "Too many arguments for method '" + p_method + "'";
			ERR_PRINT(err.utf8().get_data());
		} break;
		default: {
		}
	}
}

void Variant::construct_from_string(const String &p_string, Variant &r_value, ObjectConstruct p_obj_construct, void *p_construct_ud) {
	r_value = Variant();
}

String Variant::get_construct_string() const {
	String vars;
	VariantWriter::write_to_string(*this, vars);

	return vars;
}

String Variant::get_call_error_text(const StringName &p_method, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce) {
	return get_call_error_text(nullptr, p_method, p_argptrs, p_argcount, ce);
}

String Variant::get_call_error_text(Object *p_base, const StringName &p_method, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce) {
	String err_text;

	if (ce.error == Callable::CallError::CALL_ERROR_INVALID_ARGUMENT) {
		int errorarg = ce.argument;
		if (p_argptrs) {
			err_text = "Cannot convert argument " + itos(errorarg + 1) + " from " + Variant::get_type_name(p_argptrs[errorarg]->get_type()) + " to " + Variant::get_type_name(Variant::Type(ce.expected));
		} else {
			err_text = "Cannot convert argument " + itos(errorarg + 1) + " from [missing argptr, type unknown] to " + Variant::get_type_name(Variant::Type(ce.expected));
		}
	} else if (ce.error == Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS) {
		err_text = "Method expected " + itos(ce.expected) + " arguments, but called with " + itos(p_argcount);
	} else if (ce.error == Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS) {
		err_text = "Method expected " + itos(ce.expected) + " arguments, but called with " + itos(p_argcount);
	} else if (ce.error == Callable::CallError::CALL_ERROR_INVALID_METHOD) {
		err_text = "Method not found";
	} else if (ce.error == Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL) {
		err_text = "Instance is null";
	} else if (ce.error == Callable::CallError::CALL_ERROR_METHOD_NOT_CONST) {
		err_text = "Method not const in const instance";
	} else if (ce.error == Callable::CallError::CALL_OK) {
		return "Call OK";
	}

	String base_text;
	if (p_base) {
		base_text = p_base->get_class();
		Ref<Resource> script = p_base->get_script();
		if (script.is_valid() && script->get_path().is_resource_file()) {
			base_text += "(" + script->get_path().get_file() + ")";
		}
		base_text += "::";
	}
	return "'" + base_text + String(p_method) + "': " + err_text;
}

String Variant::get_callable_error_text(const Callable &p_callable, const Variant **p_argptrs, int p_argcount, const Callable::CallError &ce) {
	Vector<Variant> binds;
	p_callable.get_bound_arguments_ref(binds);

	int args_unbound = p_callable.get_unbound_arguments_count();

	if (p_argcount - args_unbound < 0) {
		return "Callable unbinds " + itos(args_unbound) + " arguments, but called with " + itos(p_argcount);
	} else {
		Vector<const Variant *> argptrs;
		argptrs.resize(p_argcount - args_unbound + binds.size());
		for (int i = 0; i < p_argcount - args_unbound; i++) {
			argptrs.write[i] = p_argptrs[i];
		}
		for (int i = 0; i < binds.size(); i++) {
			argptrs.write[i + p_argcount - args_unbound] = &binds[i];
		}
		return get_call_error_text(p_callable.get_object(), p_callable.get_method(), (const Variant **)argptrs.ptr(), argptrs.size(), ce);
	}
}

void Variant::register_types() {
	_register_variant_operators();
	_register_variant_methods();
	_register_variant_setters_getters();
	_register_variant_constructors();
	_register_variant_destructors();
	_register_variant_utility_functions();
}
void Variant::unregister_types() {
	_unregister_variant_operators();
	_unregister_variant_methods();
	_unregister_variant_setters_getters();
	_unregister_variant_destructors();
	_unregister_variant_utility_functions();
}
