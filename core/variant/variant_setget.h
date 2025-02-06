/**************************************************************************/
/*  variant_setget.h                                                      */
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

#ifndef VARIANT_SETGET_H
#define VARIANT_SETGET_H

#include "variant.h"

#include "core/debugger/engine_debugger.h"
#include "core/object/class_db.h"
#include "core/variant/variant_internal.h"

/**** NAMED SETTERS AND GETTERS ****/

#define SETGET_STRUCT(m_base_type, m_member_type, m_member)                                                                          \
	struct VariantSetGet_##m_base_type##_##m_member {                                                                                \
		static void get(const Variant *base, Variant *member) {                                                                      \
			VariantTypeAdjust<m_member_type>::adjust(member);                                                                        \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member;    \
		}                                                                                                                            \
		static inline void validated_get(const Variant *base, Variant *member) {                                                     \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member;    \
		}                                                                                                                            \
		static void ptr_get(const void *base, void *member) {                                                                        \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_member, member);                                  \
		}                                                                                                                            \
		static void set(Variant *base, const Variant *value, bool &valid) {                                                          \
			if (value->get_type() == GetTypeInfo<m_member_type>::VARIANT_TYPE) {                                                     \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member = *VariantGetInternalPtr<m_member_type>::get_ptr(value); \
				valid = true;                                                                                                        \
			} else {                                                                                                                 \
				valid = false;                                                                                                       \
			}                                                                                                                        \
		}                                                                                                                            \
		static inline void validated_set(Variant *base, const Variant *value) {                                                      \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member = *VariantGetInternalPtr<m_member_type>::get_ptr(value);     \
		}                                                                                                                            \
		static void ptr_set(void *base, const void *member) {                                                                        \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                                                    \
			b.m_member = PtrToArg<m_member_type>::convert(member);                                                                   \
			PtrToArg<m_base_type>::encode(b, base);                                                                                  \
		}                                                                                                                            \
		static Variant::Type get_type() {                                                                                            \
			return GetTypeInfo<m_member_type>::VARIANT_TYPE;                                                                         \
		}                                                                                                                            \
	};

#define SETGET_NUMBER_STRUCT(m_base_type, m_member_type, m_member)                                                                \
	struct VariantSetGet_##m_base_type##_##m_member {                                                                             \
		static void get(const Variant *base, Variant *member) {                                                                   \
			VariantTypeAdjust<m_member_type>::adjust(member);                                                                     \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member; \
		}                                                                                                                         \
		static inline void validated_get(const Variant *base, Variant *member) {                                                  \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member; \
		}                                                                                                                         \
		static void ptr_get(const void *base, void *member) {                                                                     \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_member, member);                               \
		}                                                                                                                         \
		static void set(Variant *base, const Variant *value, bool &valid) {                                                       \
			if (value->get_type() == Variant::FLOAT) {                                                                            \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member = *VariantGetInternalPtr<double>::get_ptr(value);     \
				valid = true;                                                                                                     \
			} else if (value->get_type() == Variant::INT) {                                                                       \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member = *VariantGetInternalPtr<int64_t>::get_ptr(value);    \
				valid = true;                                                                                                     \
			} else {                                                                                                              \
				valid = false;                                                                                                    \
			}                                                                                                                     \
		}                                                                                                                         \
		static inline void validated_set(Variant *base, const Variant *value) {                                                   \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_member = *VariantGetInternalPtr<m_member_type>::get_ptr(value);  \
		}                                                                                                                         \
		static void ptr_set(void *base, const void *member) {                                                                     \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                                                 \
			b.m_member = PtrToArg<m_member_type>::convert(member);                                                                \
			PtrToArg<m_base_type>::encode(b, base);                                                                               \
		}                                                                                                                         \
		static Variant::Type get_type() {                                                                                         \
			return GetTypeInfo<m_member_type>::VARIANT_TYPE;                                                                      \
		}                                                                                                                         \
	};

#define SETGET_STRUCT_CUSTOM(m_base_type, m_member_type, m_member, m_custom)                                                         \
	struct VariantSetGet_##m_base_type##_##m_member {                                                                                \
		static void get(const Variant *base, Variant *member) {                                                                      \
			VariantTypeAdjust<m_member_type>::adjust(member);                                                                        \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom;    \
		}                                                                                                                            \
		static inline void validated_get(const Variant *base, Variant *member) {                                                     \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom;    \
		}                                                                                                                            \
		static void ptr_get(const void *base, void *member) {                                                                        \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_custom, member);                                  \
		}                                                                                                                            \
		static void set(Variant *base, const Variant *value, bool &valid) {                                                          \
			if (value->get_type() == GetTypeInfo<m_member_type>::VARIANT_TYPE) {                                                     \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom = *VariantGetInternalPtr<m_member_type>::get_ptr(value); \
				valid = true;                                                                                                        \
			} else {                                                                                                                 \
				valid = false;                                                                                                       \
			}                                                                                                                        \
		}                                                                                                                            \
		static inline void validated_set(Variant *base, const Variant *value) {                                                      \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom = *VariantGetInternalPtr<m_member_type>::get_ptr(value);     \
		}                                                                                                                            \
		static void ptr_set(void *base, const void *member) {                                                                        \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                                                    \
			b.m_custom = PtrToArg<m_member_type>::convert(member);                                                                   \
			PtrToArg<m_base_type>::encode(b, base);                                                                                  \
		}                                                                                                                            \
		static Variant::Type get_type() {                                                                                            \
			return GetTypeInfo<m_member_type>::VARIANT_TYPE;                                                                         \
		}                                                                                                                            \
	};

#define SETGET_NUMBER_STRUCT_CUSTOM(m_base_type, m_member_type, m_member, m_custom)                                               \
	struct VariantSetGet_##m_base_type##_##m_member {                                                                             \
		static void get(const Variant *base, Variant *member) {                                                                   \
			VariantTypeAdjust<m_member_type>::adjust(member);                                                                     \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom; \
		}                                                                                                                         \
		static inline void validated_get(const Variant *base, Variant *member) {                                                  \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom; \
		}                                                                                                                         \
		static void ptr_get(const void *base, void *member) {                                                                     \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_custom, member);                               \
		}                                                                                                                         \
		static void set(Variant *base, const Variant *value, bool &valid) {                                                       \
			if (value->get_type() == Variant::FLOAT) {                                                                            \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom = *VariantGetInternalPtr<double>::get_ptr(value);     \
				valid = true;                                                                                                     \
			} else if (value->get_type() == Variant::INT) {                                                                       \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom = *VariantGetInternalPtr<int64_t>::get_ptr(value);    \
				valid = true;                                                                                                     \
			} else {                                                                                                              \
				valid = false;                                                                                                    \
			}                                                                                                                     \
		}                                                                                                                         \
		static inline void validated_set(Variant *base, const Variant *value) {                                                   \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_custom = *VariantGetInternalPtr<m_member_type>::get_ptr(value);  \
		}                                                                                                                         \
		static void ptr_set(void *base, const void *member) {                                                                     \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                                                 \
			b.m_custom = PtrToArg<m_member_type>::convert(member);                                                                \
			PtrToArg<m_base_type>::encode(b, base);                                                                               \
		}                                                                                                                         \
		static Variant::Type get_type() {                                                                                         \
			return GetTypeInfo<m_member_type>::VARIANT_TYPE;                                                                      \
		}                                                                                                                         \
	};

#define SETGET_STRUCT_FUNC(m_base_type, m_member_type, m_member, m_setter, m_getter)                                                \
	struct VariantSetGet_##m_base_type##_##m_member {                                                                               \
		static void get(const Variant *base, Variant *member) {                                                                     \
			VariantTypeAdjust<m_member_type>::adjust(member);                                                                       \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_getter(); \
		}                                                                                                                           \
		static inline void validated_get(const Variant *base, Variant *member) {                                                    \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_getter(); \
		}                                                                                                                           \
		static void ptr_get(const void *base, void *member) {                                                                       \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_getter(), member);                               \
		}                                                                                                                           \
		static void set(Variant *base, const Variant *value, bool &valid) {                                                         \
			if (value->get_type() == GetTypeInfo<m_member_type>::VARIANT_TYPE) {                                                    \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_setter(*VariantGetInternalPtr<m_member_type>::get_ptr(value)); \
				valid = true;                                                                                                       \
			} else {                                                                                                                \
				valid = false;                                                                                                      \
			}                                                                                                                       \
		}                                                                                                                           \
		static inline void validated_set(Variant *base, const Variant *value) {                                                     \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_setter(*VariantGetInternalPtr<m_member_type>::get_ptr(value));     \
		}                                                                                                                           \
		static void ptr_set(void *base, const void *member) {                                                                       \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                                                   \
			b.m_setter(PtrToArg<m_member_type>::convert(member));                                                                   \
			PtrToArg<m_base_type>::encode(b, base);                                                                                 \
		}                                                                                                                           \
		static Variant::Type get_type() {                                                                                           \
			return GetTypeInfo<m_member_type>::VARIANT_TYPE;                                                                        \
		}                                                                                                                           \
	};

#define SETGET_NUMBER_STRUCT_FUNC(m_base_type, m_member_type, m_member, m_setter, m_getter)                                         \
	struct VariantSetGet_##m_base_type##_##m_member {                                                                               \
		static void get(const Variant *base, Variant *member) {                                                                     \
			VariantTypeAdjust<m_member_type>::adjust(member);                                                                       \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_getter(); \
		}                                                                                                                           \
		static inline void validated_get(const Variant *base, Variant *member) {                                                    \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_getter(); \
		}                                                                                                                           \
		static void ptr_get(const void *base, void *member) {                                                                       \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_getter(), member);                               \
		}                                                                                                                           \
		static void set(Variant *base, const Variant *value, bool &valid) {                                                         \
			if (value->get_type() == Variant::FLOAT) {                                                                              \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_setter(*VariantGetInternalPtr<double>::get_ptr(value));        \
				valid = true;                                                                                                       \
			} else if (value->get_type() == Variant::INT) {                                                                         \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_setter(*VariantGetInternalPtr<int64_t>::get_ptr(value));       \
				valid = true;                                                                                                       \
			} else {                                                                                                                \
				valid = false;                                                                                                      \
			}                                                                                                                       \
		}                                                                                                                           \
		static inline void validated_set(Variant *base, const Variant *value) {                                                     \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_setter(*VariantGetInternalPtr<m_member_type>::get_ptr(value));     \
		}                                                                                                                           \
		static void ptr_set(void *base, const void *member) {                                                                       \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                                                   \
			b.m_setter(PtrToArg<m_member_type>::convert(member));                                                                   \
			PtrToArg<m_base_type>::encode(b, base);                                                                                 \
		}                                                                                                                           \
		static Variant::Type get_type() {                                                                                           \
			return GetTypeInfo<m_member_type>::VARIANT_TYPE;                                                                        \
		}                                                                                                                           \
	};

#define SETGET_STRUCT_FUNC_INDEX(m_base_type, m_member_type, m_member, m_setter, m_getter, m_index)                                          \
	struct VariantSetGet_##m_base_type##_##m_member {                                                                                        \
		static void get(const Variant *base, Variant *member) {                                                                              \
			VariantTypeAdjust<m_member_type>::adjust(member);                                                                                \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_getter(m_index);   \
		}                                                                                                                                    \
		static inline void validated_get(const Variant *base, Variant *member) {                                                             \
			*VariantGetInternalPtr<m_member_type>::get_ptr(member) = VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_getter(m_index);   \
		}                                                                                                                                    \
		static void ptr_get(const void *base, void *member) {                                                                                \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_getter(m_index), member);                                 \
		}                                                                                                                                    \
		static void set(Variant *base, const Variant *value, bool &valid) {                                                                  \
			if (value->get_type() == GetTypeInfo<m_member_type>::VARIANT_TYPE) {                                                             \
				VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_setter(m_index, *VariantGetInternalPtr<m_member_type>::get_ptr(value)); \
				valid = true;                                                                                                                \
			} else {                                                                                                                         \
				valid = false;                                                                                                               \
			}                                                                                                                                \
		}                                                                                                                                    \
		static inline void validated_set(Variant *base, const Variant *value) {                                                              \
			VariantGetInternalPtr<m_base_type>::get_ptr(base)->m_setter(m_index, *VariantGetInternalPtr<m_member_type>::get_ptr(value));     \
		}                                                                                                                                    \
		static void ptr_set(void *base, const void *member) {                                                                                \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                                                            \
			b.m_setter(m_index, PtrToArg<m_member_type>::convert(member));                                                                   \
			PtrToArg<m_base_type>::encode(b, base);                                                                                          \
		}                                                                                                                                    \
		static Variant::Type get_type() {                                                                                                    \
			return GetTypeInfo<m_member_type>::VARIANT_TYPE;                                                                                 \
		}                                                                                                                                    \
	};

SETGET_NUMBER_STRUCT(Vector2, double, x)
SETGET_NUMBER_STRUCT(Vector2, double, y)
#define SETGET_VEC2D(v, a, b) SETGET_STRUCT_FUNC(v, v, a##b, set_##a##b, get_##a##b)
#define SETGET_VEC2DV(v, w, a, b) SETGET_STRUCT_FUNC(v, w, a##b, set_##a##b, get_##a##b)
#define SETGET_VEC2DM(v, a, b) SETGET_VEC2D(v, a, b) SETGET_VEC2D(v, b, a)
#define SETGET_VEC2DMV(v, w, a, b) SETGET_VEC2DV(v, w, a, b) SETGET_VEC2DV(v, w, b, a)
#define SETGET_VEC2DP(v, a, b) SETGET_VEC2DM(v, a, b) SETGET_VEC2D(v, a, a) SETGET_VEC2D(v, b, b)
SETGET_VEC2DP(Vector2, x, y)

SETGET_NUMBER_STRUCT(Vector2i, int64_t, x)
SETGET_NUMBER_STRUCT(Vector2i, int64_t, y)
SETGET_VEC2DP(Vector2i, x, y)

SETGET_NUMBER_STRUCT(Vector3, double, x)
SETGET_NUMBER_STRUCT(Vector3, double, y)
SETGET_NUMBER_STRUCT(Vector3, double, z)
#define SETGET_VEC3D(v, a, b, c) SETGET_STRUCT_FUNC(v, v, a##b##c, set_##a##b##c, get_##a##b##c)
#define SETGET_VEC3DP(v, a, b, c) \
	SETGET_VEC3D(v, a, a, a);     \
	SETGET_VEC3D(v, a, a, b);     \
	SETGET_VEC3D(v, a, a, c);     \
	SETGET_VEC3D(v, a, b, a);     \
	SETGET_VEC3D(v, a, b, b);     \
	SETGET_VEC3D(v, a, b, c);     \
	SETGET_VEC3D(v, a, c, a);     \
	SETGET_VEC3D(v, a, c, b);     \
	SETGET_VEC3D(v, a, c, c);     \
	SETGET_VEC3D(v, b, a, a);     \
	SETGET_VEC3D(v, b, a, b);     \
	SETGET_VEC3D(v, b, a, c);     \
	SETGET_VEC3D(v, b, b, a);     \
	SETGET_VEC3D(v, b, b, b);     \
	SETGET_VEC3D(v, b, b, c);     \
	SETGET_VEC3D(v, b, c, a);     \
	SETGET_VEC3D(v, b, c, b);     \
	SETGET_VEC3D(v, b, c, c);     \
	SETGET_VEC3D(v, c, a, a);     \
	SETGET_VEC3D(v, c, a, b);     \
	SETGET_VEC3D(v, c, a, c);     \
	SETGET_VEC3D(v, c, b, a);     \
	SETGET_VEC3D(v, c, b, b);     \
	SETGET_VEC3D(v, c, b, c);     \
	SETGET_VEC3D(v, c, c, a);     \
	SETGET_VEC3D(v, c, c, b);     \
	SETGET_VEC3D(v, c, c, c)
SETGET_VEC2DMV(Vector3, Vector2, x, y)
SETGET_VEC2DMV(Vector3, Vector2, x, z)
SETGET_VEC2DMV(Vector3, Vector2, y, z)
SETGET_VEC2DV(Vector3, Vector2, x, x)
SETGET_VEC2DV(Vector3, Vector2, y, y)
SETGET_VEC2DV(Vector3, Vector2, z, z)
SETGET_VEC3DP(Vector3, x, y, z)

SETGET_NUMBER_STRUCT(Vector3i, int64_t, x)
SETGET_NUMBER_STRUCT(Vector3i, int64_t, y)
SETGET_NUMBER_STRUCT(Vector3i, int64_t, z)
SETGET_VEC2DMV(Vector3i, Vector2i, x, y)
SETGET_VEC2DMV(Vector3i, Vector2i, x, z)
SETGET_VEC2DMV(Vector3i, Vector2i, y, z)
SETGET_VEC2DV(Vector3i, Vector2i, x, x)
SETGET_VEC2DV(Vector3i, Vector2i, y, y)
SETGET_VEC2DV(Vector3i, Vector2i, z, z)
SETGET_VEC3DP(Vector3i, x, y, z)

SETGET_NUMBER_STRUCT(Vector4, double, x)
SETGET_NUMBER_STRUCT(Vector4, double, y)
SETGET_NUMBER_STRUCT(Vector4, double, z)
SETGET_NUMBER_STRUCT(Vector4, double, w)

SETGET_NUMBER_STRUCT(Vector4i, int64_t, x)
SETGET_NUMBER_STRUCT(Vector4i, int64_t, y)
SETGET_NUMBER_STRUCT(Vector4i, int64_t, z)
SETGET_NUMBER_STRUCT(Vector4i, int64_t, w)

SETGET_STRUCT(Rect2, Vector2, position)
SETGET_STRUCT(Rect2, Vector2, size)
SETGET_STRUCT_FUNC(Rect2, Vector2, end, set_end, get_end)

SETGET_STRUCT(Rect2i, Vector2i, position)
SETGET_STRUCT(Rect2i, Vector2i, size)
SETGET_STRUCT_FUNC(Rect2i, Vector2i, end, set_end, get_end)

SETGET_STRUCT(AABB, Vector3, position)
SETGET_STRUCT(AABB, Vector3, size)
SETGET_STRUCT_FUNC(AABB, Vector3, end, set_end, get_end)

SETGET_STRUCT_CUSTOM(Transform2D, Vector2, x, columns[0])
SETGET_STRUCT_CUSTOM(Transform2D, Vector2, y, columns[1])
SETGET_STRUCT_CUSTOM(Transform2D, Vector2, origin, columns[2])

SETGET_NUMBER_STRUCT_CUSTOM(Plane, double, x, normal.x)
SETGET_NUMBER_STRUCT_CUSTOM(Plane, double, y, normal.y)
SETGET_NUMBER_STRUCT_CUSTOM(Plane, double, z, normal.z)
SETGET_STRUCT(Plane, Vector3, normal)
SETGET_NUMBER_STRUCT(Plane, double, d)

SETGET_NUMBER_STRUCT(Quaternion, double, x)
SETGET_NUMBER_STRUCT(Quaternion, double, y)
SETGET_NUMBER_STRUCT(Quaternion, double, z)
SETGET_NUMBER_STRUCT(Quaternion, double, w)

SETGET_STRUCT_FUNC_INDEX(Basis, Vector3, x, set_column, get_column, 0)
SETGET_STRUCT_FUNC_INDEX(Basis, Vector3, y, set_column, get_column, 1)
SETGET_STRUCT_FUNC_INDEX(Basis, Vector3, z, set_column, get_column, 2)

SETGET_STRUCT(Transform3D, Basis, basis)
SETGET_STRUCT(Transform3D, Vector3, origin)

SETGET_STRUCT_CUSTOM(Projection, Vector4, x, columns[0])
SETGET_STRUCT_CUSTOM(Projection, Vector4, y, columns[1])
SETGET_STRUCT_CUSTOM(Projection, Vector4, z, columns[2])
SETGET_STRUCT_CUSTOM(Projection, Vector4, w, columns[3])

SETGET_NUMBER_STRUCT(Color, double, r)
SETGET_NUMBER_STRUCT(Color, double, g)
SETGET_NUMBER_STRUCT(Color, double, b)
SETGET_NUMBER_STRUCT(Color, double, a)

SETGET_NUMBER_STRUCT_FUNC(Color, int64_t, r8, set_r8, get_r8)
SETGET_NUMBER_STRUCT_FUNC(Color, int64_t, g8, set_g8, get_g8)
SETGET_NUMBER_STRUCT_FUNC(Color, int64_t, b8, set_b8, get_b8)
SETGET_NUMBER_STRUCT_FUNC(Color, int64_t, a8, set_a8, get_a8)

SETGET_NUMBER_STRUCT_FUNC(Color, double, h, set_h, get_h)
SETGET_NUMBER_STRUCT_FUNC(Color, double, s, set_s, get_s)
SETGET_NUMBER_STRUCT_FUNC(Color, double, v, set_v, get_v)

SETGET_NUMBER_STRUCT_FUNC(Color, double, ok_hsl_h, set_ok_hsl_h, get_ok_hsl_h)
SETGET_NUMBER_STRUCT_FUNC(Color, double, ok_hsl_s, set_ok_hsl_s, get_ok_hsl_s)
SETGET_NUMBER_STRUCT_FUNC(Color, double, ok_hsl_l, set_ok_hsl_l, get_ok_hsl_l)

#endif // VARIANT_SETGET_H
