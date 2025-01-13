/**************************************************************************/
/*  marshalls.cpp                                                         */
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

#include "marshalls.h"

#include "core/io/resource_loader.h"
#include "core/object/ref_counted.h"
#include "core/object/script_language.h"
#include "core/variant/container_type_validate.h"

#include <limits.h>
#include <stdio.h>

void EncodedObjectAsID::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_object_id", "id"), &EncodedObjectAsID::set_object_id);
	ClassDB::bind_method(D_METHOD("get_object_id"), &EncodedObjectAsID::get_object_id);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "object_id"), "set_object_id", "get_object_id");
}

void EncodedObjectAsID::set_object_id(ObjectID p_id) {
	id = p_id;
}

ObjectID EncodedObjectAsID::get_object_id() const {
	return id;
}

#define ERR_FAIL_ADD_OF(a, b, err) ERR_FAIL_COND_V(((int32_t)(b)) < 0 || ((int32_t)(a)) < 0 || ((int32_t)(a)) > INT_MAX - ((int32_t)(b)), err)
#define ERR_FAIL_MUL_OF(a, b, err) ERR_FAIL_COND_V(((int32_t)(a)) < 0 || ((int32_t)(b)) <= 0 || ((int32_t)(a)) > INT_MAX / ((int32_t)(b)), err)

// Byte 0: `Variant::Type`, byte 1: unused, bytes 2 and 3: additional data.
#define HEADER_TYPE_MASK 0xFF

// For `Variant::INT`, `Variant::FLOAT` and other math types.
#define HEADER_DATA_FLAG_64 (1 << 16)

// For `Variant::OBJECT`.
#define HEADER_DATA_FLAG_OBJECT_AS_ID (1 << 16)

// For `Variant::ARRAY`.
// Occupies bits 16 and 17.
#define HEADER_DATA_FIELD_TYPED_ARRAY_MASK (0b11 << 16)
#define HEADER_DATA_FIELD_TYPED_ARRAY_SHIFT 16

// For `Variant::DICTIONARY`.
// Occupies bits 16 and 17.
#define HEADER_DATA_FIELD_TYPED_DICTIONARY_KEY_MASK (0b11 << 16)
#define HEADER_DATA_FIELD_TYPED_DICTIONARY_KEY_SHIFT 16
// Occupies bits 18 and 19.
#define HEADER_DATA_FIELD_TYPED_DICTIONARY_VALUE_MASK (0b11 << 18)
#define HEADER_DATA_FIELD_TYPED_DICTIONARY_VALUE_SHIFT 18

enum ContainerTypeKind {
	CONTAINER_TYPE_KIND_NONE = 0b00,
	CONTAINER_TYPE_KIND_BUILTIN = 0b01,
	CONTAINER_TYPE_KIND_CLASS_NAME = 0b10,
	CONTAINER_TYPE_KIND_SCRIPT = 0b11,
};

#define GET_CONTAINER_TYPE_KIND(m_header, m_field) \
	((ContainerTypeKind)(((m_header) & HEADER_DATA_FIELD_##m_field##_MASK) >> HEADER_DATA_FIELD_##m_field##_SHIFT))

static Error _decode_string(const uint8_t *&buf, int &len, int *r_len, String &r_string) {
	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	int32_t strlen = decode_uint32(buf);
	int32_t pad = 0;

	// Handle padding.
	if (strlen % 4) {
		pad = 4 - strlen % 4;
	}

	buf += 4;
	len -= 4;

	// Ensure buffer is big enough.
	ERR_FAIL_ADD_OF(strlen, pad, ERR_FILE_EOF);
	ERR_FAIL_COND_V(strlen < 0 || strlen + pad > len, ERR_FILE_EOF);

	String str;
	ERR_FAIL_COND_V(str.parse_utf8((const char *)buf, strlen) != OK, ERR_INVALID_DATA);
	r_string = str;

	// Add padding.
	strlen += pad;

	// Update buffer pos, left data count, and return size.
	buf += strlen;
	len -= strlen;
	if (r_len) {
		(*r_len) += 4 + strlen;
	}

	return OK;
}

static Error _decode_container_type(const uint8_t *&buf, int &len, int *r_len, bool p_allow_objects, ContainerTypeKind p_type_kind, ContainerType &r_type) {
	switch (p_type_kind) {
		case CONTAINER_TYPE_KIND_NONE: {
			return OK;
		} break;
		case CONTAINER_TYPE_KIND_BUILTIN: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

			int32_t bt = decode_uint32(buf);
			buf += 4;
			len -= 4;
			if (r_len) {
				(*r_len) += 4;
			}

			ERR_FAIL_INDEX_V(bt, Variant::VARIANT_MAX, ERR_INVALID_DATA);
			r_type.builtin_type = (Variant::Type)bt;
			if (!p_allow_objects && r_type.builtin_type == Variant::OBJECT) {
				r_type.class_name = EncodedObjectAsID::get_class_static();
			}
			return OK;
		} break;
		case CONTAINER_TYPE_KIND_CLASS_NAME: {
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}

			r_type.builtin_type = Variant::OBJECT;
			if (p_allow_objects) {
				r_type.class_name = str;
			} else {
				r_type.class_name = EncodedObjectAsID::get_class_static();
			}
			return OK;
		} break;
		case CONTAINER_TYPE_KIND_SCRIPT: {
			String path;
			Error err = _decode_string(buf, len, r_len, path);
			if (err) {
				return err;
			}

			r_type.builtin_type = Variant::OBJECT;
			if (p_allow_objects) {
				ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://") || !ResourceLoader::exists(path, "Script"), ERR_INVALID_DATA, vformat("Invalid script path \"%s\".", path));
				r_type.script = ResourceLoader::load(path, "Script");
				ERR_FAIL_COND_V_MSG(r_type.script.is_null(), ERR_INVALID_DATA, vformat("Can't load script at path \"%s\".", path));
				r_type.class_name = r_type.script->get_instance_base_type();
			} else {
				r_type.class_name = EncodedObjectAsID::get_class_static();
			}
			return OK;
		} break;
	}
	ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid container type kind."); // Future proofing.
}

Error decode_variant(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_objects, int p_depth) {
	ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ERR_OUT_OF_MEMORY, "Variant is too deep. Bailing.");
	const uint8_t *buf = p_buffer;
	int len = p_len;

	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	uint32_t header = decode_uint32(buf);

	ERR_FAIL_COND_V((header & HEADER_TYPE_MASK) >= Variant::VARIANT_MAX, ERR_INVALID_DATA);

	buf += 4;
	len -= 4;
	if (r_len) {
		*r_len = 4;
	}

	// NOTE: We cannot use `sizeof(real_t)` for decoding, in case a different size is encoded.
	// Decoding math types always checks for the encoded size, while encoding always uses compilation setting.
	// This does lead to some code duplication for decoding, but compatibility is the priority.
	switch (header & HEADER_TYPE_MASK) {
		case Variant::NIL: {
			r_variant = Variant();
		} break;
		case Variant::BOOL: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			bool val = decode_uint32(buf);
			r_variant = val;
			if (r_len) {
				(*r_len) += 4;
			}
		} break;
		case Variant::INT: {
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				int64_t val = decode_uint64(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}

			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				int32_t val = decode_uint32(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case Variant::FLOAT: {
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double), ERR_INVALID_DATA);
				double val = decode_double(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += sizeof(double);
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float), ERR_INVALID_DATA);
				float val = decode_float(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += sizeof(float);
				}
			}

		} break;
		case Variant::STRING: {
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}
			r_variant = str;

		} break;

		// Math types.
		case Variant::VECTOR2: {
			Vector2 val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 2, ERR_INVALID_DATA);
				val.x = decode_double(&buf[0]);
				val.y = decode_double(&buf[sizeof(double)]);

				if (r_len) {
					(*r_len) += sizeof(double) * 2;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 2, ERR_INVALID_DATA);
				val.x = decode_float(&buf[0]);
				val.y = decode_float(&buf[sizeof(float)]);

				if (r_len) {
					(*r_len) += sizeof(float) * 2;
				}
			}
			r_variant = val;

		} break;
		case Variant::VECTOR2I: {
			ERR_FAIL_COND_V(len < 4 * 2, ERR_INVALID_DATA);
			Vector2i val;
			val.x = decode_uint32(&buf[0]);
			val.y = decode_uint32(&buf[4]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 2;
			}

		} break;
		case Variant::RECT2: {
			Rect2 val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 4, ERR_INVALID_DATA);
				val.position.x = decode_double(&buf[0]);
				val.position.y = decode_double(&buf[sizeof(double)]);
				val.size.x = decode_double(&buf[sizeof(double) * 2]);
				val.size.y = decode_double(&buf[sizeof(double) * 3]);

				if (r_len) {
					(*r_len) += sizeof(double) * 4;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 4, ERR_INVALID_DATA);
				val.position.x = decode_float(&buf[0]);
				val.position.y = decode_float(&buf[sizeof(float)]);
				val.size.x = decode_float(&buf[sizeof(float) * 2]);
				val.size.y = decode_float(&buf[sizeof(float) * 3]);

				if (r_len) {
					(*r_len) += sizeof(float) * 4;
				}
			}
			r_variant = val;

		} break;
		case Variant::RECT2I: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Rect2i val;
			val.position.x = decode_uint32(&buf[0]);
			val.position.y = decode_uint32(&buf[4]);
			val.size.x = decode_uint32(&buf[8]);
			val.size.y = decode_uint32(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case Variant::VECTOR3: {
			Vector3 val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 3, ERR_INVALID_DATA);
				val.x = decode_double(&buf[0]);
				val.y = decode_double(&buf[sizeof(double)]);
				val.z = decode_double(&buf[sizeof(double) * 2]);

				if (r_len) {
					(*r_len) += sizeof(double) * 3;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 3, ERR_INVALID_DATA);
				val.x = decode_float(&buf[0]);
				val.y = decode_float(&buf[sizeof(float)]);
				val.z = decode_float(&buf[sizeof(float) * 2]);

				if (r_len) {
					(*r_len) += sizeof(float) * 3;
				}
			}
			r_variant = val;

		} break;
		case Variant::VECTOR3I: {
			ERR_FAIL_COND_V(len < 4 * 3, ERR_INVALID_DATA);
			Vector3i val;
			val.x = decode_uint32(&buf[0]);
			val.y = decode_uint32(&buf[4]);
			val.z = decode_uint32(&buf[8]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 3;
			}

		} break;
		case Variant::VECTOR4: {
			Vector4 val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 4, ERR_INVALID_DATA);
				val.x = decode_double(&buf[0]);
				val.y = decode_double(&buf[sizeof(double)]);
				val.z = decode_double(&buf[sizeof(double) * 2]);
				val.w = decode_double(&buf[sizeof(double) * 3]);

				if (r_len) {
					(*r_len) += sizeof(double) * 4;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 4, ERR_INVALID_DATA);
				val.x = decode_float(&buf[0]);
				val.y = decode_float(&buf[sizeof(float)]);
				val.z = decode_float(&buf[sizeof(float) * 2]);
				val.w = decode_float(&buf[sizeof(float) * 3]);

				if (r_len) {
					(*r_len) += sizeof(float) * 4;
				}
			}
			r_variant = val;

		} break;
		case Variant::VECTOR4I: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Vector4i val;
			val.x = decode_uint32(&buf[0]);
			val.y = decode_uint32(&buf[4]);
			val.z = decode_uint32(&buf[8]);
			val.w = decode_uint32(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case Variant::TRANSFORM2D: {
			Transform2D val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 6, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						val.columns[i][j] = decode_double(&buf[(i * 2 + j) * sizeof(double)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(double) * 6;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 6, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						val.columns[i][j] = decode_float(&buf[(i * 2 + j) * sizeof(float)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(float) * 6;
				}
			}
			r_variant = val;

		} break;
		case Variant::PLANE: {
			Plane val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 4, ERR_INVALID_DATA);
				val.normal.x = decode_double(&buf[0]);
				val.normal.y = decode_double(&buf[sizeof(double)]);
				val.normal.z = decode_double(&buf[sizeof(double) * 2]);
				val.d = decode_double(&buf[sizeof(double) * 3]);

				if (r_len) {
					(*r_len) += sizeof(double) * 4;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 4, ERR_INVALID_DATA);
				val.normal.x = decode_float(&buf[0]);
				val.normal.y = decode_float(&buf[sizeof(float)]);
				val.normal.z = decode_float(&buf[sizeof(float) * 2]);
				val.d = decode_float(&buf[sizeof(float) * 3]);

				if (r_len) {
					(*r_len) += sizeof(float) * 4;
				}
			}
			r_variant = val;

		} break;
		case Variant::QUATERNION: {
			Quaternion val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 4, ERR_INVALID_DATA);
				val.x = decode_double(&buf[0]);
				val.y = decode_double(&buf[sizeof(double)]);
				val.z = decode_double(&buf[sizeof(double) * 2]);
				val.w = decode_double(&buf[sizeof(double) * 3]);

				if (r_len) {
					(*r_len) += sizeof(double) * 4;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 4, ERR_INVALID_DATA);
				val.x = decode_float(&buf[0]);
				val.y = decode_float(&buf[sizeof(float)]);
				val.z = decode_float(&buf[sizeof(float) * 2]);
				val.w = decode_float(&buf[sizeof(float) * 3]);

				if (r_len) {
					(*r_len) += sizeof(float) * 4;
				}
			}
			r_variant = val;

		} break;
		case Variant::AABB: {
			AABB val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 6, ERR_INVALID_DATA);
				val.position.x = decode_double(&buf[0]);
				val.position.y = decode_double(&buf[sizeof(double)]);
				val.position.z = decode_double(&buf[sizeof(double) * 2]);
				val.size.x = decode_double(&buf[sizeof(double) * 3]);
				val.size.y = decode_double(&buf[sizeof(double) * 4]);
				val.size.z = decode_double(&buf[sizeof(double) * 5]);

				if (r_len) {
					(*r_len) += sizeof(double) * 6;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 6, ERR_INVALID_DATA);
				val.position.x = decode_float(&buf[0]);
				val.position.y = decode_float(&buf[sizeof(float)]);
				val.position.z = decode_float(&buf[sizeof(float) * 2]);
				val.size.x = decode_float(&buf[sizeof(float) * 3]);
				val.size.y = decode_float(&buf[sizeof(float) * 4]);
				val.size.z = decode_float(&buf[sizeof(float) * 5]);

				if (r_len) {
					(*r_len) += sizeof(float) * 6;
				}
			}
			r_variant = val;

		} break;
		case Variant::BASIS: {
			Basis val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 9, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						val.rows[i][j] = decode_double(&buf[(i * 3 + j) * sizeof(double)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(double) * 9;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 9, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						val.rows[i][j] = decode_float(&buf[(i * 3 + j) * sizeof(float)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(float) * 9;
				}
			}
			r_variant = val;

		} break;
		case Variant::TRANSFORM3D: {
			Transform3D val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 12, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						val.basis.rows[i][j] = decode_double(&buf[(i * 3 + j) * sizeof(double)]);
					}
				}
				val.origin[0] = decode_double(&buf[sizeof(double) * 9]);
				val.origin[1] = decode_double(&buf[sizeof(double) * 10]);
				val.origin[2] = decode_double(&buf[sizeof(double) * 11]);

				if (r_len) {
					(*r_len) += sizeof(double) * 12;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 12, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						val.basis.rows[i][j] = decode_float(&buf[(i * 3 + j) * sizeof(float)]);
					}
				}
				val.origin[0] = decode_float(&buf[sizeof(float) * 9]);
				val.origin[1] = decode_float(&buf[sizeof(float) * 10]);
				val.origin[2] = decode_float(&buf[sizeof(float) * 11]);

				if (r_len) {
					(*r_len) += sizeof(float) * 12;
				}
			}
			r_variant = val;

		} break;
		case Variant::PROJECTION: {
			Projection val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 16, ERR_INVALID_DATA);
				for (int i = 0; i < 4; i++) {
					for (int j = 0; j < 4; j++) {
						val.columns[i][j] = decode_double(&buf[(i * 4 + j) * sizeof(double)]);
					}
				}
				if (r_len) {
					(*r_len) += sizeof(double) * 16;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 16, ERR_INVALID_DATA);
				for (int i = 0; i < 4; i++) {
					for (int j = 0; j < 4; j++) {
						val.columns[i][j] = decode_float(&buf[(i * 4 + j) * sizeof(float)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(float) * 16;
				}
			}
			r_variant = val;

		} break;

		// Misc types.
		case Variant::COLOR: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Color val;
			val.r = decode_float(&buf[0]);
			val.g = decode_float(&buf[4]);
			val.b = decode_float(&buf[8]);
			val.a = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4; // Colors should always be in single-precision.
			}
		} break;
		case Variant::STRING_NAME: {
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}
			r_variant = StringName(str);

		} break;

		case Variant::NODE_PATH: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t strlen = decode_uint32(buf);

			if (strlen & 0x80000000) {
				// New format.
				ERR_FAIL_COND_V(len < 12, ERR_INVALID_DATA);
				Vector<StringName> names;
				Vector<StringName> subnames;

				uint32_t namecount = strlen &= 0x7FFFFFFF;
				uint32_t subnamecount = decode_uint32(buf + 4);
				uint32_t np_flags = decode_uint32(buf + 8);

				len -= 12;
				buf += 12;

				if (np_flags & 2) { // Obsolete format with property separate from subpath.
					subnamecount++;
				}

				uint32_t total = namecount + subnamecount;

				if (r_len) {
					(*r_len) += 12;
				}

				for (uint32_t i = 0; i < total; i++) {
					String str;
					Error err = _decode_string(buf, len, r_len, str);
					if (err) {
						return err;
					}

					if (i < namecount) {
						names.push_back(str);
					} else {
						subnames.push_back(str);
					}
				}

				r_variant = NodePath(names, subnames, np_flags & 1);

			} else {
				// Old format, just a string.
				ERR_FAIL_V(ERR_INVALID_DATA);
			}

		} break;
		case Variant::RID: {
			ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
			uint64_t id = decode_uint64(buf);
			if (r_len) {
				(*r_len) += 8;
			}

			r_variant = RID::from_uint64(id);
		} break;
		case Variant::OBJECT: {
			if (header & HEADER_DATA_FLAG_OBJECT_AS_ID) {
				// This _is_ allowed.
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				ObjectID val = ObjectID(decode_uint64(buf));
				if (r_len) {
					(*r_len) += 8;
				}

				if (val.is_null()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<EncodedObjectAsID> obj_as_id;
					obj_as_id.instantiate();
					obj_as_id->set_object_id(val);

					r_variant = obj_as_id;
				}
			} else {
				ERR_FAIL_COND_V(!p_allow_objects, ERR_UNAUTHORIZED);

				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				if (str.is_empty()) {
					r_variant = (Object *)nullptr;
				} else {
					ERR_FAIL_COND_V(!ClassDB::can_instantiate(str), ERR_INVALID_DATA);

					Object *obj = ClassDB::instantiate(str);
					ERR_FAIL_NULL_V(obj, ERR_UNAVAILABLE);

					// Avoid premature free `RefCounted`. This must be done before properties are initialized,
					// since script functions (setters, implicit initializer) may be called. See GH-68666.
					Variant variant;
					if (Object::cast_to<RefCounted>(obj)) {
						Ref<RefCounted> ref = Ref<RefCounted>(Object::cast_to<RefCounted>(obj));
						variant = ref;
					} else {
						variant = obj;
					}

					ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
					int32_t count = decode_uint32(buf);
					buf += 4;
					len -= 4;
					if (r_len) {
						(*r_len) += 4; // Size of count number.
					}

					for (int i = 0; i < count; i++) {
						str = String();
						err = _decode_string(buf, len, r_len, str);
						if (err) {
							return err;
						}

						Variant value;
						int used;
						err = decode_variant(value, buf, len, &used, p_allow_objects, p_depth + 1);
						if (err) {
							return err;
						}

						buf += used;
						len -= used;
						if (r_len) {
							(*r_len) += used;
						}

						if (str == "script" && value.get_type() != Variant::NIL) {
							ERR_FAIL_COND_V_MSG(value.get_type() != Variant::STRING, ERR_INVALID_DATA, "Invalid value for \"script\" property, expected script path as String.");
							String path = value;
							ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://") || !ResourceLoader::exists(path, "Script"), ERR_INVALID_DATA, vformat("Invalid script path \"%s\".", path));
							Ref<Script> script = ResourceLoader::load(path, "Script");
							ERR_FAIL_COND_V_MSG(script.is_null(), ERR_INVALID_DATA, vformat("Can't load script at path \"%s\".", path));
							obj->set_script(script);
						} else {
							obj->set(str, value);
						}
					}

					r_variant = variant;
				}
			}

		} break;
		case Variant::CALLABLE: {
			r_variant = Callable();
		} break;
		case Variant::SIGNAL: {
			String name;
			Error err = _decode_string(buf, len, r_len, name);
			if (err) {
				return err;
			}

			ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
			ObjectID id = ObjectID(decode_uint64(buf));
			if (r_len) {
				(*r_len) += 8;
			}

			r_variant = Signal(id, StringName(name));
		} break;
		case Variant::DICTIONARY: {
			ContainerType key_type;

			{
				ContainerTypeKind key_type_kind = GET_CONTAINER_TYPE_KIND(header, TYPED_DICTIONARY_KEY);
				Error err = _decode_container_type(buf, len, r_len, p_allow_objects, key_type_kind, key_type);
				if (err) {
					return err;
				}
			}

			ContainerType value_type;

			{
				ContainerTypeKind value_type_kind = GET_CONTAINER_TYPE_KIND(header, TYPED_DICTIONARY_VALUE);
				Error err = _decode_container_type(buf, len, r_len, p_allow_objects, value_type_kind, value_type);
				if (err) {
					return err;
				}
			}

			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

			int32_t count = decode_uint32(buf);
			//bool shared = count & 0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4; // Size of count number.
			}

			Dictionary dict;
			if (key_type.builtin_type != Variant::NIL || value_type.builtin_type != Variant::NIL) {
				dict.set_typed(key_type, value_type);
			}

			for (int i = 0; i < count; i++) {
				Variant key, value;

				int used;
				Error err = decode_variant(key, buf, len, &used, p_allow_objects, p_depth + 1);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				err = decode_variant(value, buf, len, &used, p_allow_objects, p_depth + 1);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				dict[key] = value;
			}

			r_variant = dict;

		} break;
		case Variant::ARRAY: {
			ContainerType type;

			{
				ContainerTypeKind type_kind = GET_CONTAINER_TYPE_KIND(header, TYPED_ARRAY);
				Error err = _decode_container_type(buf, len, r_len, p_allow_objects, type_kind, type);
				if (err) {
					return err;
				}
			}

			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

			int32_t count = decode_uint32(buf);
			//bool shared = count & 0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4; // Size of count number.
			}

			Array array;
			if (type.builtin_type != Variant::NIL) {
				array.set_typed(type);
			}

			for (int i = 0; i < count; i++) {
				int used = 0;
				Variant elem;
				Error err = decode_variant(elem, buf, len, &used, p_allow_objects, p_depth + 1);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");
				buf += used;
				len -= used;
				array.push_back(elem);
				if (r_len) {
					(*r_len) += used;
				}
			}

			r_variant = array;

		} break;

		// Packed arrays.
		case Variant::PACKED_BYTE_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_COND_V(count < 0 || count > len, ERR_INVALID_DATA);

			Vector<uint8_t> data;

			if (count) {
				data.resize(count);
				uint8_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = buf[i];
				}
			}

			r_variant = data;

			if (r_len) {
				if (count % 4) {
					(*r_len) += 4 - count % 4;
				}
				(*r_len) += 4 + count;
			}

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<int32_t> data;

			if (count) {
				//const int *rbuf = (const int *)buf;
				data.resize(count);
				int32_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_uint32(&buf[i * 4]);
				}
			}
			r_variant = Variant(data);
			if (r_len) {
				(*r_len) += 4 + count * sizeof(int32_t);
			}

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 8, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 8 > len, ERR_INVALID_DATA);

			Vector<int64_t> data;

			if (count) {
				//const int *rbuf = (const int *)buf;
				data.resize(count);
				int64_t *w = data.ptrw();
				for (int64_t i = 0; i < count; i++) {
					w[i] = decode_uint64(&buf[i * 8]);
				}
			}
			r_variant = Variant(data);
			if (r_len) {
				(*r_len) += 4 + count * sizeof(int64_t);
			}

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<float> data;

			if (count) {
				//const float *rbuf = (const float *)buf;
				data.resize(count);
				float *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_float(&buf[i * 4]);
				}
			}
			r_variant = data;

			if (r_len) {
				(*r_len) += 4 + count * sizeof(float);
			}

		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 8, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 8 > len, ERR_INVALID_DATA);

			Vector<double> data;

			if (count) {
				data.resize(count);
				double *w = data.ptrw();
				for (int64_t i = 0; i < count; i++) {
					w[i] = decode_double(&buf[i * 8]);
				}
			}
			r_variant = data;

			if (r_len) {
				(*r_len) += 4 + count * sizeof(double);
			}

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);

			Vector<String> strings;
			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4; // Size of count number.
			}

			for (int32_t i = 0; i < count; i++) {
				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				strings.push_back(str);
			}

			r_variant = strings;

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			Vector<Vector2> varray;

			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_MUL_OF(count, sizeof(double) * 2, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(double) * 2 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector2 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_double(buf + i * sizeof(double) * 2 + sizeof(double) * 0);
						w[i].y = decode_double(buf + i * sizeof(double) * 2 + sizeof(double) * 1);
					}

					int adv = sizeof(double) * 2 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			} else {
				ERR_FAIL_MUL_OF(count, sizeof(float) * 2, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(float) * 2 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector2 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_float(buf + i * sizeof(float) * 2 + sizeof(float) * 0);
						w[i].y = decode_float(buf + i * sizeof(float) * 2 + sizeof(float) * 1);
					}

					int adv = sizeof(float) * 2 * count;

					if (r_len) {
						(*r_len) += adv;
					}
				}
			}
			r_variant = varray;

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			Vector<Vector3> varray;

			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_MUL_OF(count, sizeof(double) * 3, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(double) * 3 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector3 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_double(buf + i * sizeof(double) * 3 + sizeof(double) * 0);
						w[i].y = decode_double(buf + i * sizeof(double) * 3 + sizeof(double) * 1);
						w[i].z = decode_double(buf + i * sizeof(double) * 3 + sizeof(double) * 2);
					}

					int adv = sizeof(double) * 3 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			} else {
				ERR_FAIL_MUL_OF(count, sizeof(float) * 3, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(float) * 3 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector3 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_float(buf + i * sizeof(float) * 3 + sizeof(float) * 0);
						w[i].y = decode_float(buf + i * sizeof(float) * 3 + sizeof(float) * 1);
						w[i].z = decode_float(buf + i * sizeof(float) * 3 + sizeof(float) * 2);
					}

					int adv = sizeof(float) * 3 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			}
			r_variant = varray;

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 4 > len, ERR_INVALID_DATA);

			Vector<Color> carray;

			if (r_len) {
				(*r_len) += 4; // Size of count number.
			}

			if (count) {
				carray.resize(count);
				Color *w = carray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					// Colors should always be in single-precision.
					w[i].r = decode_float(buf + i * 4 * 4 + 4 * 0);
					w[i].g = decode_float(buf + i * 4 * 4 + 4 * 1);
					w[i].b = decode_float(buf + i * 4 * 4 + 4 * 2);
					w[i].a = decode_float(buf + i * 4 * 4 + 4 * 3);
				}

				int adv = 4 * 4 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = carray;

		} break;

		case Variant::PACKED_VECTOR4_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			Vector<Vector4> varray;

			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_MUL_OF(count, sizeof(double) * 4, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(double) * 4 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector4 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_double(buf + i * sizeof(double) * 4 + sizeof(double) * 0);
						w[i].y = decode_double(buf + i * sizeof(double) * 4 + sizeof(double) * 1);
						w[i].z = decode_double(buf + i * sizeof(double) * 4 + sizeof(double) * 2);
						w[i].w = decode_double(buf + i * sizeof(double) * 4 + sizeof(double) * 3);
					}

					int adv = sizeof(double) * 4 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			} else {
				ERR_FAIL_MUL_OF(count, sizeof(float) * 4, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(float) * 4 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector4 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_float(buf + i * sizeof(float) * 4 + sizeof(float) * 0);
						w[i].y = decode_float(buf + i * sizeof(float) * 4 + sizeof(float) * 1);
						w[i].z = decode_float(buf + i * sizeof(float) * 4 + sizeof(float) * 2);
						w[i].w = decode_float(buf + i * sizeof(float) * 4 + sizeof(float) * 3);
					}

					int adv = sizeof(float) * 4 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			}
			r_variant = varray;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

static void _encode_string(const String &p_string, uint8_t *&buf, int &r_len) {
	CharString utf8 = p_string.utf8();

	if (buf) {
		encode_uint32(utf8.length(), buf);
		buf += 4;
		memcpy(buf, utf8.get_data(), utf8.length());
		buf += utf8.length();
	}

	r_len += 4 + utf8.length();
	while (r_len % 4) {
		r_len++; // Pad.
		if (buf) {
			*(buf++) = 0;
		}
	}
}

static void _encode_container_type_header(const ContainerType &p_type, uint32_t &header, uint32_t p_shift, bool p_full_objects) {
	if (p_type.builtin_type != Variant::NIL) {
		if (p_type.script.is_valid()) {
			header |= (p_full_objects ? CONTAINER_TYPE_KIND_SCRIPT : CONTAINER_TYPE_KIND_CLASS_NAME) << p_shift;
		} else if (p_type.class_name != StringName()) {
			header |= CONTAINER_TYPE_KIND_CLASS_NAME << p_shift;
		} else {
			// No need to check `p_full_objects` since `class_name` should be non-empty for `builtin_type == Variant::OBJECT`.
			header |= CONTAINER_TYPE_KIND_BUILTIN << p_shift;
		}
	}
}

static Error _encode_container_type(const ContainerType &p_type, uint8_t *&buf, int &r_len, bool p_full_objects) {
	if (p_type.builtin_type != Variant::NIL) {
		if (p_type.script.is_valid()) {
			if (p_full_objects) {
				String path = p_type.script->get_path();
				ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://"), ERR_UNAVAILABLE, "Failed to encode a path to a custom script for a container type.");
				_encode_string(path, buf, r_len);
			} else {
				_encode_string(EncodedObjectAsID::get_class_static(), buf, r_len);
			}
		} else if (p_type.class_name != StringName()) {
			_encode_string(p_full_objects ? p_type.class_name.operator String() : EncodedObjectAsID::get_class_static(), buf, r_len);
		} else {
			// No need to check `p_full_objects` since `class_name` should be non-empty for `builtin_type == Variant::OBJECT`.
			if (buf) {
				encode_uint32(p_type.builtin_type, buf);
				buf += 4;
			}
			r_len += 4;
		}
	}
	return OK;
}

Error encode_variant(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects, int p_depth) {
	ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ERR_OUT_OF_MEMORY, "Potential infinite recursion detected. Bailing.");
	uint8_t *buf = r_buffer;

	r_len = 0;

	uint32_t header = p_variant.get_type();

	switch (p_variant.get_type()) {
		case Variant::INT: {
			int64_t val = p_variant;
			if (val > (int64_t)INT_MAX || val < (int64_t)INT_MIN) {
				header |= HEADER_DATA_FLAG_64;
			}
		} break;
		case Variant::FLOAT: {
			double d = p_variant;
			float f = d;
			if (double(f) != d) {
				header |= HEADER_DATA_FLAG_64;
			}
		} break;
		case Variant::OBJECT: {
			// Test for potential wrong values sent by the debugger when it breaks.
			Object *obj = p_variant.get_validated_object();
			if (!obj) {
				// Object is invalid, send a nullptr instead.
				if (buf) {
					encode_uint32(Variant::NIL, buf);
				}
				r_len += 4;
				return OK;
			}

			if (!p_full_objects) {
				header |= HEADER_DATA_FLAG_OBJECT_AS_ID;
			}
		} break;
		case Variant::DICTIONARY: {
			const Dictionary dict = p_variant;
			_encode_container_type_header(dict.get_key_type(), header, HEADER_DATA_FIELD_TYPED_DICTIONARY_KEY_SHIFT, p_full_objects);
			_encode_container_type_header(dict.get_value_type(), header, HEADER_DATA_FIELD_TYPED_DICTIONARY_VALUE_SHIFT, p_full_objects);
		} break;
		case Variant::ARRAY: {
			const Array array = p_variant;
			_encode_container_type_header(array.get_element_type(), header, HEADER_DATA_FIELD_TYPED_ARRAY_SHIFT, p_full_objects);
		} break;
#ifdef REAL_T_IS_DOUBLE
		case Variant::VECTOR2:
		case Variant::VECTOR3:
		case Variant::VECTOR4:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_VECTOR4_ARRAY:
		case Variant::TRANSFORM2D:
		case Variant::TRANSFORM3D:
		case Variant::PROJECTION:
		case Variant::QUATERNION:
		case Variant::PLANE:
		case Variant::BASIS:
		case Variant::RECT2:
		case Variant::AABB: {
			header |= HEADER_DATA_FLAG_64;
		} break;
#endif // REAL_T_IS_DOUBLE
		default: {
			// Nothing to do at this stage.
		} break;
	}

	if (buf) {
		encode_uint32(header, buf);
		buf += 4;
	}
	r_len += 4;

	switch (p_variant.get_type()) {
		case Variant::NIL: {
			// Nothing to do.
		} break;
		case Variant::BOOL: {
			if (buf) {
				encode_uint32(p_variant.operator bool(), buf);
			}

			r_len += 4;

		} break;
		case Variant::INT: {
			if (header & HEADER_DATA_FLAG_64) {
				// 64 bits.
				if (buf) {
					encode_uint64(p_variant.operator int64_t(), buf);
				}

				r_len += 8;
			} else {
				if (buf) {
					encode_uint32(p_variant.operator int32_t(), buf);
				}

				r_len += 4;
			}
		} break;
		case Variant::FLOAT: {
			if (header & HEADER_DATA_FLAG_64) {
				if (buf) {
					encode_double(p_variant.operator double(), buf);
				}

				r_len += 8;

			} else {
				if (buf) {
					encode_float(p_variant.operator float(), buf);
				}

				r_len += 4;
			}

		} break;
		case Variant::NODE_PATH: {
			NodePath np = p_variant;
			if (buf) {
				encode_uint32(uint32_t(np.get_name_count()) | 0x80000000, buf); // For compatibility with the old format.
				encode_uint32(np.get_subname_count(), buf + 4);
				uint32_t np_flags = 0;
				if (np.is_absolute()) {
					np_flags |= 1;
				}

				encode_uint32(np_flags, buf + 8);

				buf += 12;
			}

			r_len += 12;

			int total = np.get_name_count() + np.get_subname_count();

			for (int i = 0; i < total; i++) {
				String str;

				if (i < np.get_name_count()) {
					str = np.get_name(i);
				} else {
					str = np.get_subname(i - np.get_name_count());
				}

				CharString utf8 = str.utf8();

				int pad = 0;

				if (utf8.length() % 4) {
					pad = 4 - utf8.length() % 4;
				}

				if (buf) {
					encode_uint32(utf8.length(), buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length());
					buf += pad + utf8.length();
				}

				r_len += 4 + utf8.length() + pad;
			}

		} break;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			_encode_string(p_variant, buf, r_len);

		} break;

		// Math types.
		case Variant::VECTOR2: {
			if (buf) {
				Vector2 v2 = p_variant;
				encode_real(v2.x, &buf[0]);
				encode_real(v2.y, &buf[sizeof(real_t)]);
			}

			r_len += 2 * sizeof(real_t);

		} break;
		case Variant::VECTOR2I: {
			if (buf) {
				Vector2i v2 = p_variant;
				encode_uint32(v2.x, &buf[0]);
				encode_uint32(v2.y, &buf[4]);
			}

			r_len += 2 * 4;

		} break;
		case Variant::RECT2: {
			if (buf) {
				Rect2 r2 = p_variant;
				encode_real(r2.position.x, &buf[0]);
				encode_real(r2.position.y, &buf[sizeof(real_t)]);
				encode_real(r2.size.x, &buf[sizeof(real_t) * 2]);
				encode_real(r2.size.y, &buf[sizeof(real_t) * 3]);
			}
			r_len += 4 * sizeof(real_t);

		} break;
		case Variant::RECT2I: {
			if (buf) {
				Rect2i r2 = p_variant;
				encode_uint32(r2.position.x, &buf[0]);
				encode_uint32(r2.position.y, &buf[4]);
				encode_uint32(r2.size.x, &buf[8]);
				encode_uint32(r2.size.y, &buf[12]);
			}
			r_len += 4 * 4;

		} break;
		case Variant::VECTOR3: {
			if (buf) {
				Vector3 v3 = p_variant;
				encode_real(v3.x, &buf[0]);
				encode_real(v3.y, &buf[sizeof(real_t)]);
				encode_real(v3.z, &buf[sizeof(real_t) * 2]);
			}

			r_len += 3 * sizeof(real_t);

		} break;
		case Variant::VECTOR3I: {
			if (buf) {
				Vector3i v3 = p_variant;
				encode_uint32(v3.x, &buf[0]);
				encode_uint32(v3.y, &buf[4]);
				encode_uint32(v3.z, &buf[8]);
			}

			r_len += 3 * 4;

		} break;
		case Variant::TRANSFORM2D: {
			if (buf) {
				Transform2D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						memcpy(&buf[(i * 2 + j) * sizeof(real_t)], &val.columns[i][j], sizeof(real_t));
					}
				}
			}

			r_len += 6 * sizeof(real_t);

		} break;
		case Variant::VECTOR4: {
			if (buf) {
				Vector4 v4 = p_variant;
				encode_real(v4.x, &buf[0]);
				encode_real(v4.y, &buf[sizeof(real_t)]);
				encode_real(v4.z, &buf[sizeof(real_t) * 2]);
				encode_real(v4.w, &buf[sizeof(real_t) * 3]);
			}

			r_len += 4 * sizeof(real_t);

		} break;
		case Variant::VECTOR4I: {
			if (buf) {
				Vector4i v4 = p_variant;
				encode_uint32(v4.x, &buf[0]);
				encode_uint32(v4.y, &buf[4]);
				encode_uint32(v4.z, &buf[8]);
				encode_uint32(v4.w, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		case Variant::PLANE: {
			if (buf) {
				Plane p = p_variant;
				encode_real(p.normal.x, &buf[0]);
				encode_real(p.normal.y, &buf[sizeof(real_t)]);
				encode_real(p.normal.z, &buf[sizeof(real_t) * 2]);
				encode_real(p.d, &buf[sizeof(real_t) * 3]);
			}

			r_len += 4 * sizeof(real_t);

		} break;
		case Variant::QUATERNION: {
			if (buf) {
				Quaternion q = p_variant;
				encode_real(q.x, &buf[0]);
				encode_real(q.y, &buf[sizeof(real_t)]);
				encode_real(q.z, &buf[sizeof(real_t) * 2]);
				encode_real(q.w, &buf[sizeof(real_t) * 3]);
			}

			r_len += 4 * sizeof(real_t);

		} break;
		case Variant::AABB: {
			if (buf) {
				AABB aabb = p_variant;
				encode_real(aabb.position.x, &buf[0]);
				encode_real(aabb.position.y, &buf[sizeof(real_t)]);
				encode_real(aabb.position.z, &buf[sizeof(real_t) * 2]);
				encode_real(aabb.size.x, &buf[sizeof(real_t) * 3]);
				encode_real(aabb.size.y, &buf[sizeof(real_t) * 4]);
				encode_real(aabb.size.z, &buf[sizeof(real_t) * 5]);
			}

			r_len += 6 * sizeof(real_t);

		} break;
		case Variant::BASIS: {
			if (buf) {
				Basis val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						memcpy(&buf[(i * 3 + j) * sizeof(real_t)], &val.rows[i][j], sizeof(real_t));
					}
				}
			}

			r_len += 9 * sizeof(real_t);

		} break;
		case Variant::TRANSFORM3D: {
			if (buf) {
				Transform3D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						memcpy(&buf[(i * 3 + j) * sizeof(real_t)], &val.basis.rows[i][j], sizeof(real_t));
					}
				}

				encode_real(val.origin.x, &buf[sizeof(real_t) * 9]);
				encode_real(val.origin.y, &buf[sizeof(real_t) * 10]);
				encode_real(val.origin.z, &buf[sizeof(real_t) * 11]);
			}

			r_len += 12 * sizeof(real_t);

		} break;
		case Variant::PROJECTION: {
			if (buf) {
				Projection val = p_variant;
				for (int i = 0; i < 4; i++) {
					for (int j = 0; j < 4; j++) {
						memcpy(&buf[(i * 4 + j) * sizeof(real_t)], &val.columns[i][j], sizeof(real_t));
					}
				}
			}

			r_len += 16 * sizeof(real_t);

		} break;

		// Misc types.
		case Variant::COLOR: {
			if (buf) {
				Color c = p_variant;
				encode_float(c.r, &buf[0]);
				encode_float(c.g, &buf[4]);
				encode_float(c.b, &buf[8]);
				encode_float(c.a, &buf[12]);
			}

			r_len += 4 * 4; // Colors should always be in single-precision.

		} break;
		case Variant::RID: {
			RID rid = p_variant;

			if (buf) {
				encode_uint64(rid.get_id(), buf);
			}
			r_len += 8;
		} break;
		case Variant::OBJECT: {
			if (p_full_objects) {
				Object *obj = p_variant;
				if (!obj) {
					if (buf) {
						encode_uint32(0, buf);
					}
					r_len += 4;

				} else {
					ERR_FAIL_COND_V(!ClassDB::can_instantiate(obj->get_class()), ERR_INVALID_PARAMETER);

					_encode_string(obj->get_class(), buf, r_len);

					List<PropertyInfo> props;
					obj->get_property_list(&props);

					int pc = 0;
					for (const PropertyInfo &E : props) {
						if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}
						pc++;
					}

					if (buf) {
						encode_uint32(pc, buf);
						buf += 4;
					}

					r_len += 4;

					for (const PropertyInfo &E : props) {
						if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}

						_encode_string(E.name, buf, r_len);

						Variant value;

						if (E.name == CoreStringName(script)) {
							Ref<Script> script = obj->get_script();
							if (script.is_valid()) {
								String path = script->get_path();
								ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://"), ERR_UNAVAILABLE, "Failed to encode a path to a custom script.");
								value = path;
							}
						} else {
							value = obj->get(E.name);
						}

						int len;
						Error err = encode_variant(value, buf, len, p_full_objects, p_depth + 1);
						ERR_FAIL_COND_V(err, err);
						ERR_FAIL_COND_V(len % 4, ERR_BUG);
						r_len += len;
						if (buf) {
							buf += len;
						}
					}
				}
			} else {
				if (buf) {
					Object *obj = p_variant.get_validated_object();
					ObjectID id;
					if (obj) {
						id = obj->get_instance_id();
					}

					encode_uint64(id, buf);
				}

				r_len += 8;
			}

		} break;
		case Variant::CALLABLE: {
		} break;
		case Variant::SIGNAL: {
			Signal signal = p_variant;

			_encode_string(signal.get_name(), buf, r_len);

			if (buf) {
				encode_uint64(signal.get_object_id(), buf);
			}
			r_len += 8;
		} break;
		case Variant::DICTIONARY: {
			const Dictionary dict = p_variant;

			{
				Error err = _encode_container_type(dict.get_key_type(), buf, r_len, p_full_objects);
				if (err) {
					return err;
				}
			}

			{
				Error err = _encode_container_type(dict.get_value_type(), buf, r_len, p_full_objects);
				if (err) {
					return err;
				}
			}

			if (buf) {
				encode_uint32(uint32_t(dict.size()), buf);
				buf += 4;
			}
			r_len += 4;

			List<Variant> keys;
			dict.get_key_list(&keys);

			for (const Variant &key : keys) {
				int len;
				Error err = encode_variant(key, buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
				const Variant *value = dict.getptr(key);
				ERR_FAIL_NULL_V(value, ERR_BUG);
				err = encode_variant(*value, buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
			}

		} break;
		case Variant::ARRAY: {
			const Array array = p_variant;

			{
				Error err = _encode_container_type(array.get_element_type(), buf, r_len, p_full_objects);
				if (err) {
					return err;
				}
			}

			if (buf) {
				encode_uint32(uint32_t(array.size()), buf);
				buf += 4;
			}
			r_len += 4;

			for (const Variant &elem : array) {
				int len;
				Error err = encode_variant(elem, buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				if (buf) {
					buf += len;
				}
				r_len += len;
			}

		} break;

		// Packed arrays.
		case Variant::PACKED_BYTE_ARRAY: {
			Vector<uint8_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(uint8_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const uint8_t *r = data.ptr();
				if (r) {
					memcpy(buf, &r[0], datalen * datasize);
					buf += datalen * datasize;
				}
			}

			r_len += 4 + datalen * datasize;
			while (r_len % 4) {
				r_len++;
				if (buf) {
					*(buf++) = 0;
				}
			}

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			Vector<int32_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int32_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const int32_t *r = data.ptr();
				for (int32_t i = 0; i < datalen; i++) {
					encode_uint32(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			Vector<int64_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int64_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const int64_t *r = data.ptr();
				for (int64_t i = 0; i < datalen; i++) {
					encode_uint64(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			Vector<float> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(float);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const float *r = data.ptr();
				for (int i = 0; i < datalen; i++) {
					encode_float(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			Vector<double> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(double);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const double *r = data.ptr();
				for (int i = 0; i < datalen; i++) {
					encode_double(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			Vector<String> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			for (int i = 0; i < len; i++) {
				CharString utf8 = data.get(i).utf8();

				if (buf) {
					encode_uint32(utf8.length() + 1, buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length() + 1);
					buf += utf8.length() + 1;
				}

				r_len += 4 + utf8.length() + 1;
				while (r_len % 4) {
					r_len++; // Pad.
					if (buf) {
						*(buf++) = 0;
					}
				}
			}

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			Vector<Vector2> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector2 v = data.get(i);

					encode_real(v.x, &buf[0]);
					encode_real(v.y, &buf[sizeof(real_t)]);
					buf += sizeof(real_t) * 2;
				}
			}

			r_len += sizeof(real_t) * 2 * len;

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			Vector<Vector3> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector3 v = data.get(i);

					encode_real(v.x, &buf[0]);
					encode_real(v.y, &buf[sizeof(real_t)]);
					encode_real(v.z, &buf[sizeof(real_t) * 2]);
					buf += sizeof(real_t) * 3;
				}
			}

			r_len += sizeof(real_t) * 3 * len;

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			Vector<Color> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Color c = data.get(i);

					encode_float(c.r, &buf[0]);
					encode_float(c.g, &buf[4]);
					encode_float(c.b, &buf[8]);
					encode_float(c.a, &buf[12]);
					buf += 4 * 4; // Colors should always be in single-precision.
				}
			}

			r_len += 4 * 4 * len;

		} break;
		case Variant::PACKED_VECTOR4_ARRAY: {
			Vector<Vector4> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector4 v = data.get(i);

					encode_real(v.x, &buf[0]);
					encode_real(v.y, &buf[sizeof(real_t)]);
					encode_real(v.z, &buf[sizeof(real_t) * 2]);
					encode_real(v.w, &buf[sizeof(real_t) * 3]);
					buf += sizeof(real_t) * 4;
				}
			}

			r_len += sizeof(real_t) * 4 * len;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

Vector<float> vector3_to_float32_array(const Vector3 *vecs, size_t count) {
	// We always allocate a new array, and we don't `memcpy()`.
	// We also don't consider returning a pointer to the passed vectors when `sizeof(real_t) == 4`.
	// One reason is that we could decide to put a 4th component in `Vector3` for SIMD/mobile performance,
	// which would cause trouble with these optimizations.
	Vector<float> floats;
	if (count == 0) {
		return floats;
	}
	floats.resize(count * 3);
	float *floats_w = floats.ptrw();
	for (size_t i = 0; i < count; ++i) {
		const Vector3 v = vecs[i];
		floats_w[0] = v.x;
		floats_w[1] = v.y;
		floats_w[2] = v.z;
		floats_w += 3;
	}
	return floats;
}
