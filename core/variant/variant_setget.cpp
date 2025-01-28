/**************************************************************************/
/*  variant_setget.cpp                                                    */
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

#include "variant_setget.h"

#include "variant_callable.h"

struct VariantSetterGetterInfo {
	void (*setter)(Variant *base, const Variant *value, bool &valid);
	void (*getter)(const Variant *base, Variant *value);
	Variant::ValidatedSetter validated_setter;
	Variant::ValidatedGetter validated_getter;
	Variant::PTRSetter ptr_setter;
	Variant::PTRGetter ptr_getter;
	Variant::Type member_type;
};

static LocalVector<VariantSetterGetterInfo> variant_setters_getters[Variant::VARIANT_MAX];
static LocalVector<StringName> variant_setters_getters_names[Variant::VARIANT_MAX]; //one next to another to make it cache friendly

template <typename T>
static void register_member(Variant::Type p_type, const StringName &p_member) {
	VariantSetterGetterInfo sgi;
	sgi.setter = T::set;
	sgi.validated_setter = T::validated_set;
	sgi.ptr_setter = T::ptr_set;

	sgi.getter = T::get;
	sgi.validated_getter = T::validated_get;
	sgi.ptr_getter = T::ptr_get;

	sgi.member_type = T::get_type();

	variant_setters_getters[p_type].push_back(sgi);
	variant_setters_getters_names[p_type].push_back(p_member);
}

void register_named_setters_getters() {
#define REGISTER_MEMBER(m_base_type, m_member) register_member<VariantSetGet_##m_base_type##_##m_member>(GetTypeInfo<m_base_type>::VARIANT_TYPE, #m_member)

	REGISTER_MEMBER(Vector2, x);
	REGISTER_MEMBER(Vector2, y);

	REGISTER_MEMBER(Vector2i, x);
	REGISTER_MEMBER(Vector2i, y);

	REGISTER_MEMBER(Vector3, x);
	REGISTER_MEMBER(Vector3, y);
	REGISTER_MEMBER(Vector3, z);

	REGISTER_MEMBER(Vector3i, x);
	REGISTER_MEMBER(Vector3i, y);
	REGISTER_MEMBER(Vector3i, z);

	REGISTER_MEMBER(Vector4, x);
	REGISTER_MEMBER(Vector4, y);
	REGISTER_MEMBER(Vector4, z);
	REGISTER_MEMBER(Vector4, w);

	REGISTER_MEMBER(Vector4i, x);
	REGISTER_MEMBER(Vector4i, y);
	REGISTER_MEMBER(Vector4i, z);
	REGISTER_MEMBER(Vector4i, w);

	REGISTER_MEMBER(Rect2, position);
	REGISTER_MEMBER(Rect2, size);
	REGISTER_MEMBER(Rect2, end);

	REGISTER_MEMBER(Rect2i, position);
	REGISTER_MEMBER(Rect2i, size);
	REGISTER_MEMBER(Rect2i, end);

	REGISTER_MEMBER(AABB, position);
	REGISTER_MEMBER(AABB, size);
	REGISTER_MEMBER(AABB, end);

	REGISTER_MEMBER(Transform2D, x);
	REGISTER_MEMBER(Transform2D, y);
	REGISTER_MEMBER(Transform2D, origin);

	REGISTER_MEMBER(Plane, x);
	REGISTER_MEMBER(Plane, y);
	REGISTER_MEMBER(Plane, z);
	REGISTER_MEMBER(Plane, d);
	REGISTER_MEMBER(Plane, normal);

	REGISTER_MEMBER(Quaternion, x);
	REGISTER_MEMBER(Quaternion, y);
	REGISTER_MEMBER(Quaternion, z);
	REGISTER_MEMBER(Quaternion, w);

	REGISTER_MEMBER(Basis, x);
	REGISTER_MEMBER(Basis, y);
	REGISTER_MEMBER(Basis, z);

	REGISTER_MEMBER(Transform3D, basis);
	REGISTER_MEMBER(Transform3D, origin);

	REGISTER_MEMBER(Projection, x);
	REGISTER_MEMBER(Projection, y);
	REGISTER_MEMBER(Projection, z);
	REGISTER_MEMBER(Projection, w);

	REGISTER_MEMBER(Color, r);
	REGISTER_MEMBER(Color, g);
	REGISTER_MEMBER(Color, b);
	REGISTER_MEMBER(Color, a);

	REGISTER_MEMBER(Color, r8);
	REGISTER_MEMBER(Color, g8);
	REGISTER_MEMBER(Color, b8);
	REGISTER_MEMBER(Color, a8);

	REGISTER_MEMBER(Color, h);
	REGISTER_MEMBER(Color, s);
	REGISTER_MEMBER(Color, v);

	REGISTER_MEMBER(Color, ok_hsl_h);
	REGISTER_MEMBER(Color, ok_hsl_s);
	REGISTER_MEMBER(Color, ok_hsl_l);
}

void unregister_named_setters_getters() {
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		variant_setters_getters[i].clear();
		variant_setters_getters_names[i].clear();
	}
}

bool Variant::has_member(Variant::Type p_type, const StringName &p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);

	for (const StringName &member : variant_setters_getters_names[p_type]) {
		if (member == p_member) {
			return true;
		}
	}
	return false;
}

Variant::Type Variant::get_member_type(Variant::Type p_type, const StringName &p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::VARIANT_MAX);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].member_type;
		}
	}

	return Variant::NIL;
}

void Variant::get_member_list(Variant::Type p_type, List<StringName> *r_members) {
	for (const StringName &member : variant_setters_getters_names[p_type]) {
		r_members->push_back(member);
	}
}

int Variant::get_member_count(Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);
	return variant_setters_getters_names[p_type].size();
}

Variant::ValidatedSetter Variant::get_member_validated_setter(Variant::Type p_type, const StringName &p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].validated_setter;
		}
	}

	return nullptr;
}
Variant::ValidatedGetter Variant::get_member_validated_getter(Variant::Type p_type, const StringName &p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].validated_getter;
		}
	}

	return nullptr;
}

Variant::PTRSetter Variant::get_member_ptr_setter(Variant::Type p_type, const StringName &p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].ptr_setter;
		}
	}

	return nullptr;
}

Variant::PTRGetter Variant::get_member_ptr_getter(Variant::Type p_type, const StringName &p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].ptr_getter;
		}
	}

	return nullptr;
}

void Variant::set_named(const StringName &p_member, const Variant &p_value, bool &r_valid) {
	uint32_t s = variant_setters_getters[type].size();
	if (s) {
		for (uint32_t i = 0; i < s; i++) {
			if (variant_setters_getters_names[type][i] == p_member) {
				variant_setters_getters[type][i].setter(this, &p_value, r_valid);
				return;
			}
		}
		r_valid = false;

	} else if (type == Variant::OBJECT) {
		Object *obj = get_validated_object();
		if (!obj) {
			r_valid = false;
		} else {
			obj->set(p_member, p_value, &r_valid);
			return;
		}
	} else if (type == Variant::DICTIONARY) {
		Dictionary &dict = *VariantGetInternalPtr<Dictionary>::get_ptr(this);
		r_valid = dict.set(p_member, p_value);
	} else {
		r_valid = false;
	}
}

Variant Variant::get_named(const StringName &p_member, bool &r_valid) const {
	uint32_t s = variant_setters_getters[type].size();
	if (s) {
		for (uint32_t i = 0; i < s; i++) {
			if (variant_setters_getters_names[type][i] == p_member) {
				Variant ret;
				variant_setters_getters[type][i].getter(this, &ret);
				r_valid = true;
				return ret;
			}
		}
	}

	switch (type) {
		case Variant::OBJECT: {
			Object *obj = get_validated_object();
			if (!obj) {
				r_valid = false;
				return "Instance base is null.";
			} else {
				return obj->get(p_member, &r_valid);
			}
		} break;
		case Variant::DICTIONARY: {
			const Variant *v = VariantGetInternalPtr<Dictionary>::get_ptr(this)->getptr(p_member);
			if (v) {
				r_valid = true;
				return *v;
			}
		} break;
		default: {
			if (Variant::has_builtin_method(type, p_member)) {
				r_valid = true;
				return Callable(memnew(VariantCallable(*this, p_member)));
			}
		} break;
	}

	r_valid = false;
	return Variant();
}

/**** INDEXED SETTERS AND GETTERS ****/

#ifdef DEBUG_ENABLED

#define OOB_TEST(m_idx, m_v) \
	ERR_FAIL_INDEX(m_idx, m_v)

#else

#define OOB_TEST(m_idx, m_v)

#endif

#ifdef DEBUG_ENABLED

#define NULL_TEST(m_key) \
	ERR_FAIL_NULL(m_key)

#else

#define NULL_TEST(m_key)

#endif

#define INDEXED_SETGET_STRUCT_TYPED(m_base_type, m_elem_type)                                                                        \
	struct VariantIndexedSetGet_##m_base_type {                                                                                      \
		static void get(const Variant *base, int64_t index, Variant *value, bool *oob) {                                             \
			int64_t size = VariantGetInternalPtr<m_base_type>::get_ptr(base)->size();                                                \
			if (index < 0) {                                                                                                         \
				index += size;                                                                                                       \
			}                                                                                                                        \
			if (index < 0 || index >= size) {                                                                                        \
				*oob = true;                                                                                                         \
				return;                                                                                                              \
			}                                                                                                                        \
			VariantTypeAdjust<m_elem_type>::adjust(value);                                                                           \
			*VariantGetInternalPtr<m_elem_type>::get_ptr(value) = (*VariantGetInternalPtr<m_base_type>::get_ptr(base))[index];       \
			*oob = false;                                                                                                            \
		}                                                                                                                            \
		static void ptr_get(const void *base, int64_t index, void *member) {                                                         \
			/* avoid ptrconvert for performance*/                                                                                    \
			const m_base_type &v = *reinterpret_cast<const m_base_type *>(base);                                                     \
			if (index < 0)                                                                                                           \
				index += v.size();                                                                                                   \
			OOB_TEST(index, v.size());                                                                                               \
			PtrToArg<m_elem_type>::encode(v[index], member);                                                                         \
		}                                                                                                                            \
		static void set(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) {                                \
			if (value->get_type() != GetTypeInfo<m_elem_type>::VARIANT_TYPE) {                                                       \
				*oob = false;                                                                                                        \
				*valid = false;                                                                                                      \
				return;                                                                                                              \
			}                                                                                                                        \
			int64_t size = VariantGetInternalPtr<m_base_type>::get_ptr(base)->size();                                                \
			if (index < 0) {                                                                                                         \
				index += size;                                                                                                       \
			}                                                                                                                        \
			if (index < 0 || index >= size) {                                                                                        \
				*oob = true;                                                                                                         \
				*valid = false;                                                                                                      \
				return;                                                                                                              \
			}                                                                                                                        \
			(*VariantGetInternalPtr<m_base_type>::get_ptr(base)).write[index] = *VariantGetInternalPtr<m_elem_type>::get_ptr(value); \
			*oob = false;                                                                                                            \
			*valid = true;                                                                                                           \
		}                                                                                                                            \
		static void validated_set(Variant *base, int64_t index, const Variant *value, bool *oob) {                                   \
			int64_t size = VariantGetInternalPtr<m_base_type>::get_ptr(base)->size();                                                \
			if (index < 0) {                                                                                                         \
				index += size;                                                                                                       \
			}                                                                                                                        \
			if (index < 0 || index >= size) {                                                                                        \
				*oob = true;                                                                                                         \
				return;                                                                                                              \
			}                                                                                                                        \
			(*VariantGetInternalPtr<m_base_type>::get_ptr(base)).write[index] = *VariantGetInternalPtr<m_elem_type>::get_ptr(value); \
			*oob = false;                                                                                                            \
		}                                                                                                                            \
		static void ptr_set(void *base, int64_t index, const void *member) {                                                         \
			/* avoid ptrconvert for performance*/                                                                                    \
			m_base_type &v = *reinterpret_cast<m_base_type *>(base);                                                                 \
			if (index < 0)                                                                                                           \
				index += v.size();                                                                                                   \
			OOB_TEST(index, v.size());                                                                                               \
			v.write[index] = PtrToArg<m_elem_type>::convert(member);                                                                 \
		}                                                                                                                            \
		static Variant::Type get_index_type() {                                                                                      \
			return GetTypeInfo<m_elem_type>::VARIANT_TYPE;                                                                           \
		}                                                                                                                            \
		static uint32_t get_index_usage() {                                                                                          \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                                                 \
		}                                                                                                                            \
		static uint64_t get_indexed_size(const Variant *base) {                                                                      \
			return VariantGetInternalPtr<m_base_type>::get_ptr(base)->size();                                                        \
		}                                                                                                                            \
	};

#define INDEXED_SETGET_STRUCT_TYPED_NUMERIC(m_base_type, m_elem_type, m_assign_type)                                                 \
	struct VariantIndexedSetGet_##m_base_type {                                                                                      \
		static void get(const Variant *base, int64_t index, Variant *value, bool *oob) {                                             \
			int64_t size = VariantGetInternalPtr<m_base_type>::get_ptr(base)->size();                                                \
			if (index < 0) {                                                                                                         \
				index += size;                                                                                                       \
			}                                                                                                                        \
			if (index < 0 || index >= size) {                                                                                        \
				*oob = true;                                                                                                         \
				return;                                                                                                              \
			}                                                                                                                        \
			VariantTypeAdjust<m_elem_type>::adjust(value);                                                                           \
			*VariantGetInternalPtr<m_elem_type>::get_ptr(value) = (*VariantGetInternalPtr<m_base_type>::get_ptr(base))[index];       \
			*oob = false;                                                                                                            \
		}                                                                                                                            \
		static void ptr_get(const void *base, int64_t index, void *member) {                                                         \
			/* avoid ptrconvert for performance*/                                                                                    \
			const m_base_type &v = *reinterpret_cast<const m_base_type *>(base);                                                     \
			if (index < 0)                                                                                                           \
				index += v.size();                                                                                                   \
			OOB_TEST(index, v.size());                                                                                               \
			PtrToArg<m_elem_type>::encode(v[index], member);                                                                         \
		}                                                                                                                            \
		static void set(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) {                                \
			int64_t size = VariantGetInternalPtr<m_base_type>::get_ptr(base)->size();                                                \
			if (index < 0) {                                                                                                         \
				index += size;                                                                                                       \
			}                                                                                                                        \
			if (index < 0 || index >= size) {                                                                                        \
				*oob = true;                                                                                                         \
				*valid = false;                                                                                                      \
				return;                                                                                                              \
			}                                                                                                                        \
			m_assign_type num;                                                                                                       \
			if (value->get_type() == Variant::INT) {                                                                                 \
				num = (m_assign_type) * VariantGetInternalPtr<int64_t>::get_ptr(value);                                              \
			} else if (value->get_type() == Variant::FLOAT) {                                                                        \
				num = (m_assign_type) * VariantGetInternalPtr<double>::get_ptr(value);                                               \
			} else {                                                                                                                 \
				*oob = false;                                                                                                        \
				*valid = false;                                                                                                      \
				return;                                                                                                              \
			}                                                                                                                        \
			(*VariantGetInternalPtr<m_base_type>::get_ptr(base)).write[index] = num;                                                 \
			*oob = false;                                                                                                            \
			*valid = true;                                                                                                           \
		}                                                                                                                            \
		static void validated_set(Variant *base, int64_t index, const Variant *value, bool *oob) {                                   \
			int64_t size = VariantGetInternalPtr<m_base_type>::get_ptr(base)->size();                                                \
			if (index < 0) {                                                                                                         \
				index += size;                                                                                                       \
			}                                                                                                                        \
			if (index < 0 || index >= size) {                                                                                        \
				*oob = true;                                                                                                         \
				return;                                                                                                              \
			}                                                                                                                        \
			(*VariantGetInternalPtr<m_base_type>::get_ptr(base)).write[index] = *VariantGetInternalPtr<m_elem_type>::get_ptr(value); \
			*oob = false;                                                                                                            \
		}                                                                                                                            \
		static void ptr_set(void *base, int64_t index, const void *member) {                                                         \
			/* avoid ptrconvert for performance*/                                                                                    \
			m_base_type &v = *reinterpret_cast<m_base_type *>(base);                                                                 \
			if (index < 0)                                                                                                           \
				index += v.size();                                                                                                   \
			OOB_TEST(index, v.size());                                                                                               \
			v.write[index] = PtrToArg<m_elem_type>::convert(member);                                                                 \
		}                                                                                                                            \
		static Variant::Type get_index_type() {                                                                                      \
			return GetTypeInfo<m_elem_type>::VARIANT_TYPE;                                                                           \
		}                                                                                                                            \
		static uint32_t get_index_usage() {                                                                                          \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                                                 \
		}                                                                                                                            \
		static uint64_t get_indexed_size(const Variant *base) {                                                                      \
			return VariantGetInternalPtr<m_base_type>::get_ptr(base)->size();                                                        \
		}                                                                                                                            \
	};

#define INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(m_base_type, m_elem_type, m_assign_type, m_max)                                  \
	struct VariantIndexedSetGet_##m_base_type {                                                                                \
		static void get(const Variant *base, int64_t index, Variant *value, bool *oob) {                                       \
			if (index < 0 || index >= m_max) {                                                                                 \
				*oob = true;                                                                                                   \
				return;                                                                                                        \
			}                                                                                                                  \
			VariantTypeAdjust<m_elem_type>::adjust(value);                                                                     \
			*VariantGetInternalPtr<m_elem_type>::get_ptr(value) = (*VariantGetInternalPtr<m_base_type>::get_ptr(base))[index]; \
			*oob = false;                                                                                                      \
		}                                                                                                                      \
		static void ptr_get(const void *base, int64_t index, void *member) {                                                   \
			/* avoid ptrconvert for performance*/                                                                              \
			const m_base_type &v = *reinterpret_cast<const m_base_type *>(base);                                               \
			OOB_TEST(index, m_max);                                                                                            \
			PtrToArg<m_elem_type>::encode(v[index], member);                                                                   \
		}                                                                                                                      \
		static void set(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) {                          \
			if (index < 0 || index >= m_max) {                                                                                 \
				*oob = true;                                                                                                   \
				*valid = false;                                                                                                \
				return;                                                                                                        \
			}                                                                                                                  \
			m_assign_type num;                                                                                                 \
			if (value->get_type() == Variant::INT) {                                                                           \
				num = (m_assign_type) * VariantGetInternalPtr<int64_t>::get_ptr(value);                                        \
			} else if (value->get_type() == Variant::FLOAT) {                                                                  \
				num = (m_assign_type) * VariantGetInternalPtr<double>::get_ptr(value);                                         \
			} else {                                                                                                           \
				*oob = false;                                                                                                  \
				*valid = false;                                                                                                \
				return;                                                                                                        \
			}                                                                                                                  \
			(*VariantGetInternalPtr<m_base_type>::get_ptr(base))[index] = num;                                                 \
			*oob = false;                                                                                                      \
			*valid = true;                                                                                                     \
		}                                                                                                                      \
		static void validated_set(Variant *base, int64_t index, const Variant *value, bool *oob) {                             \
			if (index < 0 || index >= m_max) {                                                                                 \
				*oob = true;                                                                                                   \
				return;                                                                                                        \
			}                                                                                                                  \
			(*VariantGetInternalPtr<m_base_type>::get_ptr(base))[index] = *VariantGetInternalPtr<m_elem_type>::get_ptr(value); \
			*oob = false;                                                                                                      \
		}                                                                                                                      \
		static void ptr_set(void *base, int64_t index, const void *member) {                                                   \
			/* avoid ptrconvert for performance*/                                                                              \
			m_base_type &v = *reinterpret_cast<m_base_type *>(base);                                                           \
			OOB_TEST(index, m_max);                                                                                            \
			v[index] = PtrToArg<m_elem_type>::convert(member);                                                                 \
		}                                                                                                                      \
		static Variant::Type get_index_type() {                                                                                \
			return GetTypeInfo<m_elem_type>::VARIANT_TYPE;                                                                     \
		}                                                                                                                      \
		static uint32_t get_index_usage() {                                                                                    \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                                           \
		}                                                                                                                      \
		static uint64_t get_indexed_size(const Variant *base) {                                                                \
			return m_max;                                                                                                      \
		}                                                                                                                      \
	};

#define INDEXED_SETGET_STRUCT_BUILTIN_ACCESSOR(m_base_type, m_elem_type, m_accessor, m_max)                                               \
	struct VariantIndexedSetGet_##m_base_type {                                                                                           \
		static void get(const Variant *base, int64_t index, Variant *value, bool *oob) {                                                  \
			if (index < 0 || index >= m_max) {                                                                                            \
				*oob = true;                                                                                                              \
				return;                                                                                                                   \
			}                                                                                                                             \
			VariantTypeAdjust<m_elem_type>::adjust(value);                                                                                \
			*VariantGetInternalPtr<m_elem_type>::get_ptr(value) = (*VariantGetInternalPtr<m_base_type>::get_ptr(base))m_accessor[index];  \
			*oob = false;                                                                                                                 \
		}                                                                                                                                 \
		static void ptr_get(const void *base, int64_t index, void *member) {                                                              \
			/* avoid ptrconvert for performance*/                                                                                         \
			const m_base_type &v = *reinterpret_cast<const m_base_type *>(base);                                                          \
			OOB_TEST(index, m_max);                                                                                                       \
			PtrToArg<m_elem_type>::encode(v m_accessor[index], member);                                                                   \
		}                                                                                                                                 \
		static void set(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) {                                     \
			if (value->get_type() != GetTypeInfo<m_elem_type>::VARIANT_TYPE) {                                                            \
				*oob = false;                                                                                                             \
				*valid = false;                                                                                                           \
			}                                                                                                                             \
			if (index < 0 || index >= m_max) {                                                                                            \
				*oob = true;                                                                                                              \
				*valid = false;                                                                                                           \
				return;                                                                                                                   \
			}                                                                                                                             \
			(*VariantGetInternalPtr<m_base_type>::get_ptr(base)) m_accessor[index] = *VariantGetInternalPtr<m_elem_type>::get_ptr(value); \
			*oob = false;                                                                                                                 \
			*valid = true;                                                                                                                \
		}                                                                                                                                 \
		static void validated_set(Variant *base, int64_t index, const Variant *value, bool *oob) {                                        \
			if (index < 0 || index >= m_max) {                                                                                            \
				*oob = true;                                                                                                              \
				return;                                                                                                                   \
			}                                                                                                                             \
			(*VariantGetInternalPtr<m_base_type>::get_ptr(base)) m_accessor[index] = *VariantGetInternalPtr<m_elem_type>::get_ptr(value); \
			*oob = false;                                                                                                                 \
		}                                                                                                                                 \
		static void ptr_set(void *base, int64_t index, const void *member) {                                                              \
			/* avoid ptrconvert for performance*/                                                                                         \
			m_base_type &v = *reinterpret_cast<m_base_type *>(base);                                                                      \
			OOB_TEST(index, m_max);                                                                                                       \
			v m_accessor[index] = PtrToArg<m_elem_type>::convert(member);                                                                 \
		}                                                                                                                                 \
		static Variant::Type get_index_type() {                                                                                           \
			return GetTypeInfo<m_elem_type>::VARIANT_TYPE;                                                                                \
		}                                                                                                                                 \
		static uint32_t get_index_usage() {                                                                                               \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                                                      \
		}                                                                                                                                 \
		static uint64_t get_indexed_size(const Variant *base) {                                                                           \
			return m_max;                                                                                                                 \
		}                                                                                                                                 \
	};

#define INDEXED_SETGET_STRUCT_BUILTIN_FUNC(m_base_type, m_elem_type, m_set, m_get, m_max)                                          \
	struct VariantIndexedSetGet_##m_base_type {                                                                                    \
		static void get(const Variant *base, int64_t index, Variant *value, bool *oob) {                                           \
			if (index < 0 || index >= m_max) {                                                                                     \
				*oob = true;                                                                                                       \
				return;                                                                                                            \
			}                                                                                                                      \
			VariantTypeAdjust<m_elem_type>::adjust(value);                                                                         \
			*VariantGetInternalPtr<m_elem_type>::get_ptr(value) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_get(index); \
			*oob = false;                                                                                                          \
		}                                                                                                                          \
		static void ptr_get(const void *base, int64_t index, void *member) {                                                       \
			/* avoid ptrconvert for performance*/                                                                                  \
			const m_base_type &v = *reinterpret_cast<const m_base_type *>(base);                                                   \
			OOB_TEST(index, m_max);                                                                                                \
			PtrToArg<m_elem_type>::encode(v.m_get(index), member);                                                                 \
		}                                                                                                                          \
		static void set(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) {                              \
			if (value->get_type() != GetTypeInfo<m_elem_type>::VARIANT_TYPE) {                                                     \
				*oob = false;                                                                                                      \
				*valid = false;                                                                                                    \
			}                                                                                                                      \
			if (index < 0 || index >= m_max) {                                                                                     \
				*oob = true;                                                                                                       \
				*valid = false;                                                                                                    \
				return;                                                                                                            \
			}                                                                                                                      \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_set(index, *VariantGetInternalPtr<m_elem_type>::get_ptr(value));  \
			*oob = false;                                                                                                          \
			*valid = true;                                                                                                         \
		}                                                                                                                          \
		static void validated_set(Variant *base, int64_t index, const Variant *value, bool *oob) {                                 \
			if (index < 0 || index >= m_max) {                                                                                     \
				*oob = true;                                                                                                       \
				return;                                                                                                            \
			}                                                                                                                      \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_set(index, *VariantGetInternalPtr<m_elem_type>::get_ptr(value));  \
			*oob = false;                                                                                                          \
		}                                                                                                                          \
		static void ptr_set(void *base, int64_t index, const void *member) {                                                       \
			/* avoid ptrconvert for performance*/                                                                                  \
			m_base_type &v = *reinterpret_cast<m_base_type *>(base);                                                               \
			OOB_TEST(index, m_max);                                                                                                \
			v.m_set(index, PtrToArg<m_elem_type>::convert(member));                                                                \
		}                                                                                                                          \
		static Variant::Type get_index_type() {                                                                                    \
			return GetTypeInfo<m_elem_type>::VARIANT_TYPE;                                                                         \
		}                                                                                                                          \
		static uint32_t get_index_usage() {                                                                                        \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                                               \
		}                                                                                                                          \
		static uint64_t get_indexed_size(const Variant *base) {                                                                    \
			return m_max;                                                                                                          \
		}                                                                                                                          \
	};

struct VariantIndexedSetGet_Array {
	static void get(const Variant *base, int64_t index, Variant *value, bool *oob) {
		int64_t size = VariantGetInternalPtr<Array>::get_ptr(base)->size();
		if (index < 0) {
			index += size;
		}
		if (index < 0 || index >= size) {
			*oob = true;
			return;
		}
		*value = (*VariantGetInternalPtr<Array>::get_ptr(base))[index];
		*oob = false;
	}
	static void ptr_get(const void *base, int64_t index, void *member) {
		/* avoid ptrconvert for performance*/
		const Array &v = *reinterpret_cast<const Array *>(base);
		if (index < 0) {
			index += v.size();
		}
		OOB_TEST(index, v.size());
		PtrToArg<Variant>::encode(v[index], member);
	}
	static void set(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) {
		if (VariantGetInternalPtr<Array>::get_ptr(base)->is_read_only()) {
			*valid = false;
			*oob = true;
			return;
		}
		int64_t size = VariantGetInternalPtr<Array>::get_ptr(base)->size();
		if (index < 0) {
			index += size;
		}
		if (index < 0 || index >= size) {
			*oob = true;
			*valid = false;
			return;
		}
		VariantGetInternalPtr<Array>::get_ptr(base)->set(index, *value);
		*oob = false;
		*valid = true;
	}
	static void validated_set(Variant *base, int64_t index, const Variant *value, bool *oob) {
		if (VariantGetInternalPtr<Array>::get_ptr(base)->is_read_only()) {
			*oob = true;
			return;
		}
		int64_t size = VariantGetInternalPtr<Array>::get_ptr(base)->size();
		if (index < 0) {
			index += size;
		}
		if (index < 0 || index >= size) {
			*oob = true;
			return;
		}
		VariantGetInternalPtr<Array>::get_ptr(base)->set(index, *value);
		*oob = false;
	}
	static void ptr_set(void *base, int64_t index, const void *member) {
		/* avoid ptrconvert for performance*/
		Array &v = *reinterpret_cast<Array *>(base);
		if (index < 0) {
			index += v.size();
		}
		OOB_TEST(index, v.size());
		v.set(index, PtrToArg<Variant>::convert(member));
	}
	static Variant::Type get_index_type() { return Variant::NIL; }
	static uint32_t get_index_usage() { return PROPERTY_USAGE_NIL_IS_VARIANT; }
	static uint64_t get_indexed_size(const Variant *base) { return 0; }
};

struct VariantIndexedSetGet_Dictionary {
	static void get(const Variant *base, int64_t index, Variant *value, bool *oob) {
		const Variant *ptr = VariantGetInternalPtr<Dictionary>::get_ptr(base)->getptr(index);
		if (!ptr) {
			*oob = true;
			return;
		}
		*value = *ptr;
		*oob = false;
	}
	static void ptr_get(const void *base, int64_t index, void *member) {
		// Avoid ptrconvert for performance.
		const Dictionary &v = *reinterpret_cast<const Dictionary *>(base);
		const Variant *ptr = v.getptr(index);
		NULL_TEST(ptr);
		PtrToArg<Variant>::encode(*ptr, member);
	}
	static void set(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) {
		*valid = VariantGetInternalPtr<Dictionary>::get_ptr(base)->set(index, *value);
		*oob = VariantGetInternalPtr<Dictionary>::get_ptr(base)->is_read_only();
	}
	static void validated_set(Variant *base, int64_t index, const Variant *value, bool *oob) {
		VariantGetInternalPtr<Dictionary>::get_ptr(base)->set(index, *value);
		*oob = VariantGetInternalPtr<Dictionary>::get_ptr(base)->is_read_only();
	}
	static void ptr_set(void *base, int64_t index, const void *member) {
		Dictionary &v = *reinterpret_cast<Dictionary *>(base);
		v.set(index, PtrToArg<Variant>::convert(member));
	}
	static Variant::Type get_index_type() { return Variant::NIL; }
	static uint32_t get_index_usage() { return PROPERTY_USAGE_DEFAULT; }
	static uint64_t get_indexed_size(const Variant *base) { return VariantGetInternalPtr<Dictionary>::get_ptr(base)->size(); }
};

struct VariantIndexedSetGet_String {
	static void get(const Variant *base, int64_t index, Variant *value, bool *oob) {
		int64_t length = VariantGetInternalPtr<String>::get_ptr(base)->length();
		if (index < 0) {
			index += length;
		}
		if (index < 0 || index >= length) {
			*oob = true;
			return;
		}
		char32_t result = (*VariantGetInternalPtr<String>::get_ptr(base))[index];
		*value = String(&result, 1);
		*oob = false;
	}
	static void ptr_get(const void *base, int64_t index, void *member) {
		/* avoid ptrconvert for performance*/
		const String &v = *reinterpret_cast<const String *>(base);
		if (index < 0) {
			index += v.length();
		}
		OOB_TEST(index, v.length());
		char32_t c = v[index];
		PtrToArg<String>::encode(String(&c, 1), member);
	}
	static void set(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) {
		if (value->get_type() != Variant::STRING) {
			*oob = false;
			*valid = false;
			return;
		}
		int64_t length = VariantGetInternalPtr<String>::get_ptr(base)->length();
		if (index < 0) {
			index += length;
		}
		if (index < 0 || index >= length) {
			*oob = true;
			*valid = false;
			return;
		}
		String *b = VariantGetInternalPtr<String>::get_ptr(base);
		const String *v = VariantInternal::get_string(value);
		if (v->length() == 0) {
			b->remove_at(index);
		} else {
			b->set(index, v->get(0));
		}
		*oob = false;
		*valid = true;
	}
	static void validated_set(Variant *base, int64_t index, const Variant *value, bool *oob) {
		int64_t length = VariantGetInternalPtr<String>::get_ptr(base)->length();
		if (index < 0) {
			index += length;
		}
		if (index < 0 || index >= length) {
			*oob = true;
			return;
		}
		String *b = VariantGetInternalPtr<String>::get_ptr(base);
		const String *v = VariantInternal::get_string(value);
		if (v->length() == 0) {
			b->remove_at(index);
		} else {
			b->set(index, v->get(0));
		}
		*oob = false;
	}
	static void ptr_set(void *base, int64_t index, const void *member) {
		/* avoid ptrconvert for performance*/
		String &v = *reinterpret_cast<String *>(base);
		if (index < 0) {
			index += v.length();
		}
		OOB_TEST(index, v.length());
		const String &m = *reinterpret_cast<const String *>(member);
		if (unlikely(m.length() == 0)) {
			v.remove_at(index);
		} else {
			v.set(index, m.unicode_at(0));
		}
	}
	static Variant::Type get_index_type() { return Variant::STRING; }
	static uint32_t get_index_usage() { return PROPERTY_USAGE_DEFAULT; }
	static uint64_t get_indexed_size(const Variant *base) { return VariantInternal::get_string(base)->length(); }
};

INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector2, double, real_t, 2)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector2i, int64_t, int32_t, 2)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector3, double, real_t, 3)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector3i, int64_t, int32_t, 3)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector4, double, real_t, 4)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector4i, int64_t, int32_t, 4)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Quaternion, double, real_t, 4)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Color, double, float, 4)

INDEXED_SETGET_STRUCT_BUILTIN_ACCESSOR(Transform2D, Vector2, .columns, 3)
INDEXED_SETGET_STRUCT_BUILTIN_FUNC(Basis, Vector3, set_column, get_column, 3)
INDEXED_SETGET_STRUCT_BUILTIN_ACCESSOR(Projection, Vector4, .columns, 4)

INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedByteArray, int64_t, uint8_t)
INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedInt32Array, int64_t, int32_t)
INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedInt64Array, int64_t, int64_t)
INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedFloat32Array, double, float)
INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedFloat64Array, double, double)
INDEXED_SETGET_STRUCT_TYPED(PackedVector2Array, Vector2)
INDEXED_SETGET_STRUCT_TYPED(PackedVector3Array, Vector3)
INDEXED_SETGET_STRUCT_TYPED(PackedStringArray, String)
INDEXED_SETGET_STRUCT_TYPED(PackedColorArray, Color)
INDEXED_SETGET_STRUCT_TYPED(PackedVector4Array, Vector4)

struct VariantIndexedSetterGetterInfo {
	void (*setter)(Variant *base, int64_t index, const Variant *value, bool *valid, bool *oob) = nullptr;
	void (*getter)(const Variant *base, int64_t index, Variant *value, bool *oob) = nullptr;

	Variant::ValidatedIndexedSetter validated_setter = nullptr;
	Variant::ValidatedIndexedGetter validated_getter = nullptr;

	Variant::PTRIndexedSetter ptr_setter = nullptr;
	Variant::PTRIndexedGetter ptr_getter = nullptr;

	uint64_t (*get_indexed_size)(const Variant *base) = nullptr;

	Variant::Type index_type = Variant::NIL;
	uint32_t index_usage = PROPERTY_USAGE_DEFAULT;

	bool valid = false;
};

static VariantIndexedSetterGetterInfo variant_indexed_setters_getters[Variant::VARIANT_MAX];

template <typename T>
static void register_indexed_member(Variant::Type p_type) {
	VariantIndexedSetterGetterInfo &sgi = variant_indexed_setters_getters[p_type];

	sgi.setter = T::set;
	sgi.validated_setter = T::validated_set;
	sgi.ptr_setter = T::ptr_set;

	sgi.getter = T::get;
	sgi.validated_getter = T::get;
	sgi.ptr_getter = T::ptr_get;

	sgi.index_type = T::get_index_type();
	sgi.index_usage = T::get_index_usage();
	sgi.get_indexed_size = T::get_indexed_size;

	sgi.valid = true;
}

void register_indexed_setters_getters() {
#define REGISTER_INDEXED_MEMBER(m_base_type) register_indexed_member<VariantIndexedSetGet_##m_base_type>(GetTypeInfo<m_base_type>::VARIANT_TYPE)

	REGISTER_INDEXED_MEMBER(String);
	REGISTER_INDEXED_MEMBER(Vector2);
	REGISTER_INDEXED_MEMBER(Vector2i);
	REGISTER_INDEXED_MEMBER(Vector3);
	REGISTER_INDEXED_MEMBER(Vector3i);
	REGISTER_INDEXED_MEMBER(Vector4);
	REGISTER_INDEXED_MEMBER(Vector4i);
	REGISTER_INDEXED_MEMBER(Quaternion);
	REGISTER_INDEXED_MEMBER(Color);
	REGISTER_INDEXED_MEMBER(Transform2D);
	REGISTER_INDEXED_MEMBER(Basis);
	REGISTER_INDEXED_MEMBER(Projection);

	REGISTER_INDEXED_MEMBER(PackedByteArray);
	REGISTER_INDEXED_MEMBER(PackedInt32Array);
	REGISTER_INDEXED_MEMBER(PackedInt64Array);
	REGISTER_INDEXED_MEMBER(PackedFloat32Array);
	REGISTER_INDEXED_MEMBER(PackedFloat64Array);
	REGISTER_INDEXED_MEMBER(PackedVector2Array);
	REGISTER_INDEXED_MEMBER(PackedVector3Array);
	REGISTER_INDEXED_MEMBER(PackedStringArray);
	REGISTER_INDEXED_MEMBER(PackedColorArray);
	REGISTER_INDEXED_MEMBER(PackedVector4Array);

	REGISTER_INDEXED_MEMBER(Array);
	REGISTER_INDEXED_MEMBER(Dictionary);
}

static void unregister_indexed_setters_getters() {
}

bool Variant::has_indexing(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	return variant_indexed_setters_getters[p_type].valid;
}

Variant::Type Variant::get_indexed_element_type(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::VARIANT_MAX);
	return variant_indexed_setters_getters[p_type].index_type;
}

uint32_t Variant::get_indexed_element_usage(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, PROPERTY_USAGE_DEFAULT);
	return variant_indexed_setters_getters[p_type].index_usage;
}

Variant::ValidatedIndexedSetter Variant::get_member_validated_indexed_setter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return variant_indexed_setters_getters[p_type].validated_setter;
}
Variant::ValidatedIndexedGetter Variant::get_member_validated_indexed_getter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return variant_indexed_setters_getters[p_type].validated_getter;
}

Variant::PTRIndexedSetter Variant::get_member_ptr_indexed_setter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return variant_indexed_setters_getters[p_type].ptr_setter;
}
Variant::PTRIndexedGetter Variant::get_member_ptr_indexed_getter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return variant_indexed_setters_getters[p_type].ptr_getter;
}

void Variant::set_indexed(int64_t p_index, const Variant &p_value, bool &r_valid, bool &r_oob) {
	if (likely(variant_indexed_setters_getters[type].valid)) {
		variant_indexed_setters_getters[type].setter(this, p_index, &p_value, &r_valid, &r_oob);
	} else {
		r_valid = false;
		r_oob = false;
	}
}
Variant Variant::get_indexed(int64_t p_index, bool &r_valid, bool &r_oob) const {
	if (likely(variant_indexed_setters_getters[type].valid)) {
		Variant ret;
		variant_indexed_setters_getters[type].getter(this, p_index, &ret, &r_oob);
		r_valid = !r_oob;
		return ret;
	} else {
		r_valid = false;
		r_oob = false;
		return Variant();
	}
}

uint64_t Variant::get_indexed_size() const {
	if (likely(variant_indexed_setters_getters[type].valid && variant_indexed_setters_getters[type].get_indexed_size)) {
		return variant_indexed_setters_getters[type].get_indexed_size(this);
	} else {
		return 0;
	}
}

struct VariantKeyedSetGetDictionary {
	static void get(const Variant *base, const Variant *key, Variant *value, bool *r_valid) {
		const Variant *ptr = VariantGetInternalPtr<Dictionary>::get_ptr(base)->getptr(*key);
		if (!ptr) {
			*r_valid = false;
			return;
		}
		*value = *ptr;
		*r_valid = true;
	}
	static void ptr_get(const void *base, const void *key, void *value) {
		/* avoid ptrconvert for performance*/
		const Dictionary &v = *reinterpret_cast<const Dictionary *>(base);
		const Variant *ptr = v.getptr(PtrToArg<Variant>::convert(key));
		NULL_TEST(ptr);
		PtrToArg<Variant>::encode(*ptr, value);
	}
	static void set(Variant *base, const Variant *key, const Variant *value, bool *r_valid) {
		*r_valid = VariantGetInternalPtr<Dictionary>::get_ptr(base)->set(*key, *value);
	}
	static void ptr_set(void *base, const void *key, const void *value) {
		Dictionary &v = *reinterpret_cast<Dictionary *>(base);
		v.set(PtrToArg<Variant>::convert(key), PtrToArg<Variant>::convert(value));
	}

	static bool has(const Variant *base, const Variant *key, bool *r_valid) {
		*r_valid = true;
		return VariantGetInternalPtr<Dictionary>::get_ptr(base)->has(*key);
	}
	static uint32_t ptr_has(const void *base, const void *key) {
		/* avoid ptrconvert for performance*/
		const Dictionary &v = *reinterpret_cast<const Dictionary *>(base);
		return v.has(PtrToArg<Variant>::convert(key));
	}
};

struct VariantKeyedSetGetObject {
	static void get(const Variant *base, const Variant *key, Variant *value, bool *r_valid) {
		Object *obj = base->get_validated_object();

		if (!obj) {
			*r_valid = false;
			*value = Variant();
			return;
		}
		*value = obj->getvar(*key, r_valid);
	}
	static void ptr_get(const void *base, const void *key, void *value) {
		const Object *obj = PtrToArg<Object *>::convert(base);
		NULL_TEST(obj);
		Variant v = obj->getvar(PtrToArg<Variant>::convert(key));
		PtrToArg<Variant>::encode(v, value);
	}
	static void set(Variant *base, const Variant *key, const Variant *value, bool *r_valid) {
		Object *obj = base->get_validated_object();

		if (!obj) {
			*r_valid = false;
			return;
		}
		obj->setvar(*key, *value, r_valid);
	}
	static void ptr_set(void *base, const void *key, const void *value) {
		Object *obj = PtrToArg<Object *>::convert(base);
		NULL_TEST(obj);
		obj->setvar(PtrToArg<Variant>::convert(key), PtrToArg<Variant>::convert(value));
	}

	static bool has(const Variant *base, const Variant *key, bool *r_valid) {
		Object *obj = base->get_validated_object();
		if (!obj) {
			*r_valid = false;
			return false;
		}
		*r_valid = true;
		bool exists;
		obj->getvar(*key, &exists);
		return exists;
	}
	static uint32_t ptr_has(const void *base, const void *key) {
		const Object *obj = PtrToArg<Object *>::convert(base);
		ERR_FAIL_NULL_V(obj, false);
		bool valid;
		obj->getvar(PtrToArg<Variant>::convert(key), &valid);
		return valid;
	}
};

struct VariantKeyedSetterGetterInfo {
	Variant::ValidatedKeyedSetter validated_setter = nullptr;
	Variant::ValidatedKeyedGetter validated_getter = nullptr;
	Variant::ValidatedKeyedChecker validated_checker = nullptr;

	Variant::PTRKeyedSetter ptr_setter = nullptr;
	Variant::PTRKeyedGetter ptr_getter = nullptr;
	Variant::PTRKeyedChecker ptr_checker = nullptr;

	bool valid = false;
};

static VariantKeyedSetterGetterInfo variant_keyed_setters_getters[Variant::VARIANT_MAX];

template <typename T>
static void register_keyed_member(Variant::Type p_type) {
	VariantKeyedSetterGetterInfo &sgi = variant_keyed_setters_getters[p_type];

	sgi.validated_setter = T::set;
	sgi.ptr_setter = T::ptr_set;

	sgi.validated_getter = T::get;
	sgi.ptr_getter = T::ptr_get;

	sgi.validated_checker = T::has;
	sgi.ptr_checker = T::ptr_has;

	sgi.valid = true;
}

static void register_keyed_setters_getters() {
	register_keyed_member<VariantKeyedSetGetDictionary>(Variant::DICTIONARY);
	register_keyed_member<VariantKeyedSetGetObject>(Variant::OBJECT);
}
bool Variant::is_keyed(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, VARIANT_MAX, false);
	return variant_keyed_setters_getters[p_type].valid;
}

Variant::ValidatedKeyedSetter Variant::get_member_validated_keyed_setter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].validated_setter;
}
Variant::ValidatedKeyedGetter Variant::get_member_validated_keyed_getter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].validated_getter;
}
Variant::ValidatedKeyedChecker Variant::get_member_validated_keyed_checker(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].validated_checker;
}

Variant::PTRKeyedSetter Variant::get_member_ptr_keyed_setter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].ptr_setter;
}
Variant::PTRKeyedGetter Variant::get_member_ptr_keyed_getter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].ptr_getter;
}
Variant::PTRKeyedChecker Variant::get_member_ptr_keyed_checker(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].ptr_checker;
}

void Variant::set_keyed(const Variant &p_key, const Variant &p_value, bool &r_valid) {
	if (likely(variant_keyed_setters_getters[type].valid)) {
		variant_keyed_setters_getters[type].validated_setter(this, &p_key, &p_value, &r_valid);
	} else {
		r_valid = false;
	}
}
Variant Variant::get_keyed(const Variant &p_key, bool &r_valid) const {
	if (likely(variant_keyed_setters_getters[type].valid)) {
		Variant ret;
		variant_keyed_setters_getters[type].validated_getter(this, &p_key, &ret, &r_valid);
		return ret;
	} else {
		r_valid = false;
		return Variant();
	}
}
bool Variant::has_key(const Variant &p_key, bool &r_valid) const {
	if (likely(variant_keyed_setters_getters[type].valid)) {
		return variant_keyed_setters_getters[type].validated_checker(this, &p_key, &r_valid);
	} else {
		r_valid = false;
		return false;
	}
}

void Variant::set(const Variant &p_index, const Variant &p_value, bool *r_valid, VariantSetError *err_code) {
	if (err_code) {
		*err_code = VariantSetError::SET_OK;
	}
	if (type == DICTIONARY || type == OBJECT) {
		bool valid;
		set_keyed(p_index, p_value, valid);
		if (r_valid) {
			*r_valid = valid;
			if (!valid && err_code) {
				*err_code = VariantSetError::SET_KEYED_ERR;
			}
		}
	} else {
		bool valid = false;
		if (p_index.get_type() == STRING_NAME) {
			set_named(*VariantGetInternalPtr<StringName>::get_ptr(&p_index), p_value, valid);
			if (!valid && err_code) {
				*err_code = VariantSetError::SET_NAMED_ERR;
			}
		} else if (p_index.get_type() == INT) {
			bool obb;
			set_indexed(*VariantGetInternalPtr<int64_t>::get_ptr(&p_index), p_value, valid, obb);
			if (obb) {
				valid = false;
				if (err_code) {
					*err_code = VariantSetError::SET_INDEXED_ERR;
				}
			}
		} else if (p_index.get_type() == STRING) { // less efficient version of named
			set_named(*VariantGetInternalPtr<String>::get_ptr(&p_index), p_value, valid);
			if (!valid && err_code) {
				*err_code = VariantSetError::SET_NAMED_ERR;
			}
		} else if (p_index.get_type() == FLOAT) { // less efficient version of indexed
			bool obb;
			set_indexed(*VariantGetInternalPtr<double>::get_ptr(&p_index), p_value, valid, obb);
			if (obb) {
				valid = false;
				if (err_code) {
					*err_code = VariantSetError::SET_INDEXED_ERR;
				}
			}
		}
		if (r_valid) {
			*r_valid = valid;
		}
	}
}

Variant Variant::get(const Variant &p_index, bool *r_valid, VariantGetError *err_code) const {
	if (err_code) {
		*err_code = VariantGetError::GET_OK;
	}
	Variant ret;
	if (type == DICTIONARY || type == OBJECT) {
		bool valid;
		ret = get_keyed(p_index, valid);
		if (r_valid) {
			*r_valid = valid;
			if (!valid && err_code) {
				*err_code = VariantGetError::GET_KEYED_ERR;
			}
		}
	} else {
		bool valid = false;
		if (p_index.get_type() == STRING_NAME) {
			ret = get_named(*VariantGetInternalPtr<StringName>::get_ptr(&p_index), valid);
			if (!valid && err_code) {
				*err_code = VariantGetError::GET_NAMED_ERR;
			}
		} else if (p_index.get_type() == INT) {
			bool obb;
			ret = get_indexed(*VariantGetInternalPtr<int64_t>::get_ptr(&p_index), valid, obb);
			if (obb) {
				valid = false;
				if (err_code) {
					*err_code = VariantGetError::GET_INDEXED_ERR;
				}
			}
		} else if (p_index.get_type() == STRING) { // less efficient version of named
			ret = get_named(*VariantGetInternalPtr<String>::get_ptr(&p_index), valid);
			if (!valid && err_code) {
				*err_code = VariantGetError::GET_NAMED_ERR;
			}
		} else if (p_index.get_type() == FLOAT) { // less efficient version of indexed
			bool obb;
			ret = get_indexed(*VariantGetInternalPtr<double>::get_ptr(&p_index), valid, obb);
			if (obb) {
				valid = false;
				if (err_code) {
					*err_code = VariantGetError::GET_INDEXED_ERR;
				}
			}
		}
		if (r_valid) {
			*r_valid = valid;
		}
	}

	return ret;
}

void Variant::get_property_list(List<PropertyInfo> *p_list) const {
	if (type == DICTIONARY) {
		const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
		List<Variant> keys;
		dic->get_key_list(&keys);
		for (const Variant &E : keys) {
			if (E.is_string()) {
				p_list->push_back(PropertyInfo(dic->get_valid(E).get_type(), E));
			}
		}
	} else if (type == OBJECT) {
		Object *obj = get_validated_object();
		ERR_FAIL_NULL(obj);
		obj->get_property_list(p_list);

	} else {
		List<StringName> members;
		get_member_list(type, &members);
		for (const StringName &E : members) {
			PropertyInfo pi;
			pi.name = E;
			pi.type = get_member_type(type, E);
			p_list->push_back(pi);
		}
	}
}

bool Variant::iter_init(Variant &r_iter, bool &valid) const {
	valid = true;
	switch (type) {
		case INT: {
			r_iter = 0;
			return _data._int > 0;
		} break;
		case FLOAT: {
			r_iter = 0.0;
			return _data._float > 0.0;
		} break;
		case VECTOR2: {
			double from = reinterpret_cast<const Vector2 *>(_data._mem)->x;
			double to = reinterpret_cast<const Vector2 *>(_data._mem)->y;

			r_iter = from;

			return from < to;
		} break;
		case VECTOR2I: {
			int64_t from = reinterpret_cast<const Vector2i *>(_data._mem)->x;
			int64_t to = reinterpret_cast<const Vector2i *>(_data._mem)->y;

			r_iter = from;

			return from < to;
		} break;
		case VECTOR3: {
			double from = reinterpret_cast<const Vector3 *>(_data._mem)->x;
			double to = reinterpret_cast<const Vector3 *>(_data._mem)->y;
			double step = reinterpret_cast<const Vector3 *>(_data._mem)->z;

			r_iter = from;

			if (from == to) {
				return false;
			} else if (from < to) {
				return step > 0;
			}
			return step < 0;
		} break;
		case VECTOR3I: {
			int64_t from = reinterpret_cast<const Vector3i *>(_data._mem)->x;
			int64_t to = reinterpret_cast<const Vector3i *>(_data._mem)->y;
			int64_t step = reinterpret_cast<const Vector3i *>(_data._mem)->z;

			r_iter = from;

			if (from == to) {
				return false;
			} else if (from < to) {
				return step > 0;
			}
			return step < 0;
		} break;
		case OBJECT: {
			if (!_get_obj().obj) {
				valid = false;
				return false;
			}

#ifdef DEBUG_ENABLED

			if (EngineDebugger::is_active() && !_get_obj().id.is_ref_counted() && ObjectDB::get_instance(_get_obj().id) == nullptr) {
				valid = false;
				return false;
			}

#endif
			Callable::CallError ce;
			ce.error = Callable::CallError::CALL_OK;
			Array ref;
			ref.push_back(r_iter);
			Variant vref = ref;
			const Variant *refp[] = { &vref };
			Variant ret = _get_obj().obj->callp(CoreStringName(_iter_init), refp, 1, ce);

			if (ref.size() != 1 || ce.error != Callable::CallError::CALL_OK) {
				valid = false;
				return false;
			}

			r_iter = ref[0];
			return ret;
		} break;

		case STRING: {
			const String *str = reinterpret_cast<const String *>(_data._mem);
			if (str->is_empty()) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case DICTIONARY: {
			const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
			if (dic->is_empty()) {
				return false;
			}

			const Variant *next = dic->next(nullptr);
			r_iter = *next;
			return true;

		} break;
		case ARRAY: {
			const Array *arr = reinterpret_cast<const Array *>(_data._mem);
			if (arr->is_empty()) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case PACKED_BYTE_ARRAY: {
			const Vector<uint8_t> *arr = &PackedArrayRef<uint8_t>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case PACKED_INT32_ARRAY: {
			const Vector<int32_t> *arr = &PackedArrayRef<int32_t>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case PACKED_INT64_ARRAY: {
			const Vector<int64_t> *arr = &PackedArrayRef<int64_t>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case PACKED_FLOAT32_ARRAY: {
			const Vector<float> *arr = &PackedArrayRef<float>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case PACKED_FLOAT64_ARRAY: {
			const Vector<double> *arr = &PackedArrayRef<double>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case PACKED_STRING_ARRAY: {
			const Vector<String> *arr = &PackedArrayRef<String>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case PACKED_VECTOR2_ARRAY: {
			const Vector<Vector2> *arr = &PackedArrayRef<Vector2>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case PACKED_VECTOR3_ARRAY: {
			const Vector<Vector3> *arr = &PackedArrayRef<Vector3>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		case PACKED_COLOR_ARRAY: {
			const Vector<Color> *arr = &PackedArrayRef<Color>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;

		} break;
		case PACKED_VECTOR4_ARRAY: {
			const Vector<Vector4> *arr = &PackedArrayRef<Vector4>::get_array(_data.packed_array);
			if (arr->size() == 0) {
				return false;
			}
			r_iter = 0;
			return true;
		} break;
		default: {
		}
	}

	valid = false;
	return false;
}

bool Variant::iter_next(Variant &r_iter, bool &valid) const {
	valid = true;
	switch (type) {
		case INT: {
			int64_t idx = r_iter;
			idx++;
			if (idx >= _data._int) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case FLOAT: {
			double idx = r_iter;
			idx++;
			if (idx >= _data._float) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case VECTOR2: {
			double to = reinterpret_cast<const Vector2 *>(_data._mem)->y;

			double idx = r_iter;
			idx++;

			if (idx >= to) {
				return false;
			}

			r_iter = idx;
			return true;
		} break;
		case VECTOR2I: {
			int64_t to = reinterpret_cast<const Vector2i *>(_data._mem)->y;

			int64_t idx = r_iter;
			idx++;

			if (idx >= to) {
				return false;
			}

			r_iter = idx;
			return true;
		} break;
		case VECTOR3: {
			double to = reinterpret_cast<const Vector3 *>(_data._mem)->y;
			double step = reinterpret_cast<const Vector3 *>(_data._mem)->z;

			double idx = r_iter;
			idx += step;

			if (step < 0 && idx <= to) {
				return false;
			}

			if (step > 0 && idx >= to) {
				return false;
			}

			r_iter = idx;
			return true;
		} break;
		case VECTOR3I: {
			int64_t to = reinterpret_cast<const Vector3i *>(_data._mem)->y;
			int64_t step = reinterpret_cast<const Vector3i *>(_data._mem)->z;

			int64_t idx = r_iter;
			idx += step;

			if (step < 0 && idx <= to) {
				return false;
			}

			if (step > 0 && idx >= to) {
				return false;
			}

			r_iter = idx;
			return true;
		} break;
		case OBJECT: {
			if (!_get_obj().obj) {
				valid = false;
				return false;
			}

#ifdef DEBUG_ENABLED

			if (EngineDebugger::is_active() && !_get_obj().id.is_ref_counted() && ObjectDB::get_instance(_get_obj().id) == nullptr) {
				valid = false;
				return false;
			}

#endif
			Callable::CallError ce;
			ce.error = Callable::CallError::CALL_OK;
			Array ref;
			ref.push_back(r_iter);
			Variant vref = ref;
			const Variant *refp[] = { &vref };
			Variant ret = _get_obj().obj->callp(CoreStringName(_iter_next), refp, 1, ce);

			if (ref.size() != 1 || ce.error != Callable::CallError::CALL_OK) {
				valid = false;
				return false;
			}

			r_iter = ref[0];

			return ret;
		} break;

		case STRING: {
			const String *str = reinterpret_cast<const String *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= str->length()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case DICTIONARY: {
			const Dictionary *dic = reinterpret_cast<const Dictionary *>(_data._mem);
			const Variant *next = dic->next(&r_iter);
			if (!next) {
				return false;
			}

			r_iter = *next;
			return true;

		} break;
		case ARRAY: {
			const Array *arr = reinterpret_cast<const Array *>(_data._mem);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case PACKED_BYTE_ARRAY: {
			const Vector<uint8_t> *arr = &PackedArrayRef<uint8_t>::get_array(_data.packed_array);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;

		} break;
		case PACKED_INT32_ARRAY: {
			const Vector<int32_t> *arr = &PackedArrayRef<int32_t>::get_array(_data.packed_array);
			int32_t idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;

		} break;
		case PACKED_INT64_ARRAY: {
			const Vector<int64_t> *arr = &PackedArrayRef<int64_t>::get_array(_data.packed_array);
			int64_t idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;

		} break;
		case PACKED_FLOAT32_ARRAY: {
			const Vector<float> *arr = &PackedArrayRef<float>::get_array(_data.packed_array);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;

		} break;
		case PACKED_FLOAT64_ARRAY: {
			const Vector<double> *arr = &PackedArrayRef<double>::get_array(_data.packed_array);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;

		} break;
		case PACKED_STRING_ARRAY: {
			const Vector<String> *arr = &PackedArrayRef<String>::get_array(_data.packed_array);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case PACKED_VECTOR2_ARRAY: {
			const Vector<Vector2> *arr = &PackedArrayRef<Vector2>::get_array(_data.packed_array);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case PACKED_VECTOR3_ARRAY: {
			const Vector<Vector3> *arr = &PackedArrayRef<Vector3>::get_array(_data.packed_array);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case PACKED_COLOR_ARRAY: {
			const Vector<Color> *arr = &PackedArrayRef<Color>::get_array(_data.packed_array);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		case PACKED_VECTOR4_ARRAY: {
			const Vector<Vector4> *arr = &PackedArrayRef<Vector4>::get_array(_data.packed_array);
			int idx = r_iter;
			idx++;
			if (idx >= arr->size()) {
				return false;
			}
			r_iter = idx;
			return true;
		} break;
		default: {
		}
	}

	valid = false;
	return false;
}

Variant Variant::iter_get(const Variant &r_iter, bool &r_valid) const {
	r_valid = true;
	switch (type) {
		case INT: {
			return r_iter;
		} break;
		case FLOAT: {
			return r_iter;
		} break;
		case VECTOR2: {
			return r_iter;
		} break;
		case VECTOR2I: {
			return r_iter;
		} break;
		case VECTOR3: {
			return r_iter;
		} break;
		case VECTOR3I: {
			return r_iter;
		} break;
		case OBJECT: {
			if (!_get_obj().obj) {
				r_valid = false;
				return Variant();
			}
#ifdef DEBUG_ENABLED
			if (EngineDebugger::is_active() && !_get_obj().id.is_ref_counted() && ObjectDB::get_instance(_get_obj().id) == nullptr) {
				r_valid = false;
				return Variant();
			}

#endif
			Callable::CallError ce;
			ce.error = Callable::CallError::CALL_OK;
			const Variant *refp[] = { &r_iter };
			Variant ret = _get_obj().obj->callp(CoreStringName(_iter_get), refp, 1, ce);

			if (ce.error != Callable::CallError::CALL_OK) {
				r_valid = false;
				return Variant();
			}

			//r_iter=ref[0];

			return ret;
		} break;

		case STRING: {
			const String *str = reinterpret_cast<const String *>(_data._mem);
			return str->substr(r_iter, 1);
		} break;
		case DICTIONARY: {
			return r_iter; //iterator is the same as the key

		} break;
		case ARRAY: {
			const Array *arr = reinterpret_cast<const Array *>(_data._mem);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_BYTE_ARRAY: {
			const Vector<uint8_t> *arr = &PackedArrayRef<uint8_t>::get_array(_data.packed_array);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_INT32_ARRAY: {
			const Vector<int32_t> *arr = &PackedArrayRef<int32_t>::get_array(_data.packed_array);
			int32_t idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_INT64_ARRAY: {
			const Vector<int64_t> *arr = &PackedArrayRef<int64_t>::get_array(_data.packed_array);
			int64_t idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_FLOAT32_ARRAY: {
			const Vector<float> *arr = &PackedArrayRef<float>::get_array(_data.packed_array);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_FLOAT64_ARRAY: {
			const Vector<double> *arr = &PackedArrayRef<double>::get_array(_data.packed_array);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_STRING_ARRAY: {
			const Vector<String> *arr = &PackedArrayRef<String>::get_array(_data.packed_array);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_VECTOR2_ARRAY: {
			const Vector<Vector2> *arr = &PackedArrayRef<Vector2>::get_array(_data.packed_array);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_VECTOR3_ARRAY: {
			const Vector<Vector3> *arr = &PackedArrayRef<Vector3>::get_array(_data.packed_array);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_COLOR_ARRAY: {
			const Vector<Color> *arr = &PackedArrayRef<Color>::get_array(_data.packed_array);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		case PACKED_VECTOR4_ARRAY: {
			const Vector<Vector4> *arr = &PackedArrayRef<Vector4>::get_array(_data.packed_array);
			int idx = r_iter;
#ifdef DEBUG_ENABLED
			if (idx < 0 || idx >= arr->size()) {
				r_valid = false;
				return Variant();
			}
#endif
			return arr->get(idx);
		} break;
		default: {
		}
	}

	r_valid = false;
	return Variant();
}

Variant Variant::duplicate(bool p_deep) const {
	return recursive_duplicate(p_deep, 0);
}

Variant Variant::recursive_duplicate(bool p_deep, int recursion_count) const {
	switch (type) {
		case OBJECT: {
			/*  breaks stuff :(
			if (p_deep && !_get_obj().ref.is_null()) {
				Ref<Resource> resource = _get_obj().ref;
				if (resource.is_valid()) {
					return resource->duplicate(true);
				}
			}
			*/
			return *this;
		} break;
		case DICTIONARY:
			return operator Dictionary().recursive_duplicate(p_deep, recursion_count);
		case ARRAY:
			return operator Array().recursive_duplicate(p_deep, recursion_count);
		case PACKED_BYTE_ARRAY:
			return operator Vector<uint8_t>().duplicate();
		case PACKED_INT32_ARRAY:
			return operator Vector<int32_t>().duplicate();
		case PACKED_INT64_ARRAY:
			return operator Vector<int64_t>().duplicate();
		case PACKED_FLOAT32_ARRAY:
			return operator Vector<float>().duplicate();
		case PACKED_FLOAT64_ARRAY:
			return operator Vector<double>().duplicate();
		case PACKED_STRING_ARRAY:
			return operator Vector<String>().duplicate();
		case PACKED_VECTOR2_ARRAY:
			return operator Vector<Vector2>().duplicate();
		case PACKED_VECTOR3_ARRAY:
			return operator Vector<Vector3>().duplicate();
		case PACKED_COLOR_ARRAY:
			return operator Vector<Color>().duplicate();
		case PACKED_VECTOR4_ARRAY:
			return operator Vector<Vector4>().duplicate();
		default:
			return *this;
	}
}

void Variant::_register_variant_setters_getters() {
	register_named_setters_getters();
	register_indexed_setters_getters();
	register_keyed_setters_getters();
}
void Variant::_unregister_variant_setters_getters() {
	unregister_named_setters_getters();
	unregister_indexed_setters_getters();
}
