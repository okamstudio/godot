/**************************************************************************/
/*  method_ptrcall.h                                                      */
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

#include "core/object/object_id.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"

template <typename T>
struct PtrToArg {};

#define MAKE_PTRARG(m_type)                                              \
	template <>                                                          \
	struct PtrToArg<m_type> {                                            \
		_FORCE_INLINE_ static const m_type &convert(const void *p_ptr) { \
			return *reinterpret_cast<const m_type *>(p_ptr);             \
		}                                                                \
		typedef m_type EncodeT;                                          \
		_FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) {   \
			*((m_type *)p_ptr) = p_val;                                  \
		}                                                                \
	};                                                                   \
	template <>                                                          \
	struct PtrToArg<const m_type &> {                                    \
		_FORCE_INLINE_ static const m_type &convert(const void *p_ptr) { \
			return *reinterpret_cast<const m_type *>(p_ptr);             \
		}                                                                \
		typedef m_type EncodeT;                                          \
		_FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) {   \
			*((m_type *)p_ptr) = p_val;                                  \
		}                                                                \
	}

#define MAKE_PTRARGCONV(m_type, m_conv)                                           \
	template <>                                                                   \
	struct PtrToArg<m_type> {                                                     \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {                 \
			return static_cast<m_type>(*reinterpret_cast<const m_conv *>(p_ptr)); \
		}                                                                         \
		typedef m_conv EncodeT;                                                   \
		_FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) {            \
			*((m_conv *)p_ptr) = static_cast<m_conv>(p_val);                      \
		}                                                                         \
	};                                                                            \
	template <>                                                                   \
	struct PtrToArg<const m_type &> {                                             \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {                 \
			return static_cast<m_type>(*reinterpret_cast<const m_conv *>(p_ptr)); \
		}                                                                         \
		typedef m_conv EncodeT;                                                   \
		_FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) {            \
			*((m_conv *)p_ptr) = static_cast<m_conv>(p_val);                      \
		}                                                                         \
	}

#define MAKE_PTRARG_BY_REFERENCE(m_type)                                      \
	template <>                                                               \
	struct PtrToArg<m_type> {                                                 \
		_FORCE_INLINE_ static const m_type &convert(const void *p_ptr) {      \
			return *reinterpret_cast<const m_type *>(p_ptr);                  \
		}                                                                     \
		typedef m_type EncodeT;                                               \
		_FORCE_INLINE_ static void encode(const m_type &p_val, void *p_ptr) { \
			*((m_type *)p_ptr) = p_val;                                       \
		}                                                                     \
	};                                                                        \
	template <>                                                               \
	struct PtrToArg<const m_type &> {                                         \
		_FORCE_INLINE_ static const m_type &convert(const void *p_ptr) {      \
			return *reinterpret_cast<const m_type *>(p_ptr);                  \
		}                                                                     \
		typedef m_type EncodeT;                                               \
		_FORCE_INLINE_ static void encode(const m_type &p_val, void *p_ptr) { \
			*((m_type *)p_ptr) = p_val;                                       \
		}                                                                     \
	}

MAKE_PTRARGCONV(bool, uint8_t);
// Integer types.
MAKE_PTRARGCONV(uint8_t, int64_t);
MAKE_PTRARGCONV(int8_t, int64_t);
MAKE_PTRARGCONV(uint16_t, int64_t);
MAKE_PTRARGCONV(int16_t, int64_t);
MAKE_PTRARGCONV(uint32_t, int64_t);
MAKE_PTRARGCONV(int32_t, int64_t);
MAKE_PTRARG(int64_t);
MAKE_PTRARG(uint64_t);
// Float types
MAKE_PTRARGCONV(float, double);
MAKE_PTRARG(double);

MAKE_PTRARG(String);
MAKE_PTRARG(Vector2);
MAKE_PTRARG(Vector2i);
MAKE_PTRARG(Rect2);
MAKE_PTRARG(Rect2i);
MAKE_PTRARG_BY_REFERENCE(Vector3);
MAKE_PTRARG_BY_REFERENCE(Vector3i);
MAKE_PTRARG_BY_REFERENCE(Vector4);
MAKE_PTRARG_BY_REFERENCE(Vector4i);
MAKE_PTRARG(Transform2D);
MAKE_PTRARG(Projection);
MAKE_PTRARG_BY_REFERENCE(Plane);
MAKE_PTRARG(Quaternion);
MAKE_PTRARG_BY_REFERENCE(AABB);
MAKE_PTRARG_BY_REFERENCE(Basis);
MAKE_PTRARG_BY_REFERENCE(Transform3D);
MAKE_PTRARG_BY_REFERENCE(Color);
MAKE_PTRARG(StringName);
MAKE_PTRARG(NodePath);
MAKE_PTRARG(RID);
// Object doesn't need this.
MAKE_PTRARG(Callable);
MAKE_PTRARG(Signal);
MAKE_PTRARG(Dictionary);
MAKE_PTRARG(Array);
MAKE_PTRARG(PackedByteArray);
MAKE_PTRARG(PackedInt32Array);
MAKE_PTRARG(PackedInt64Array);
MAKE_PTRARG(PackedFloat32Array);
MAKE_PTRARG(PackedFloat64Array);
MAKE_PTRARG(PackedStringArray);
MAKE_PTRARG(PackedVector2Array);
MAKE_PTRARG(PackedVector3Array);
MAKE_PTRARG(PackedColorArray);
MAKE_PTRARG(PackedVector4Array);
MAKE_PTRARG_BY_REFERENCE(Variant);

// This is for Object.

template <typename T>
struct PtrToArg<T *> {
	_FORCE_INLINE_ static T *convert(const void *p_ptr) {
		return likely(p_ptr) ? *reinterpret_cast<T *const *>(p_ptr) : nullptr;
	}
	typedef Object *EncodeT;
	_FORCE_INLINE_ static void encode(T *p_var, void *p_ptr) {
		*((T **)p_ptr) = p_var;
	}
};

template <typename T>
struct PtrToArg<const T *> {
	_FORCE_INLINE_ static const T *convert(const void *p_ptr) {
		return likely(p_ptr) ? *reinterpret_cast<T *const *>(p_ptr) : nullptr;
	}
	typedef const Object *EncodeT;
	_FORCE_INLINE_ static void encode(T *p_var, void *p_ptr) {
		*((T **)p_ptr) = p_var;
	}
};

// This is for ObjectID.

template <>
struct PtrToArg<ObjectID> {
	_FORCE_INLINE_ static const ObjectID convert(const void *p_ptr) {
		return ObjectID(*reinterpret_cast<const uint64_t *>(p_ptr));
	}
	typedef uint64_t EncodeT;
	_FORCE_INLINE_ static void encode(const ObjectID &p_val, void *p_ptr) {
		*((uint64_t *)p_ptr) = p_val;
	}
};

// This is for the special cases used by Variant.

// No EncodeT because direct pointer conversion not possible.
#define MAKE_VECARG(m_type)                                                              \
	template <>                                                                          \
	struct PtrToArg<Vector<m_type>> {                                                    \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {                \
			const Vector<m_type> *dvs = reinterpret_cast<const Vector<m_type> *>(p_ptr); \
			Vector<m_type> ret;                                                          \
			int len = dvs->size();                                                       \
			ret.resize(len);                                                             \
			{                                                                            \
				const m_type *r = dvs->ptr();                                            \
				for (int i = 0; i < len; i++) {                                          \
					ret.write[i] = r[i];                                                 \
				}                                                                        \
			}                                                                            \
			return ret;                                                                  \
		}                                                                                \
		_FORCE_INLINE_ static void encode(const Vector<m_type> &p_vec, void *p_ptr) {    \
			Vector<m_type> *dv = reinterpret_cast<Vector<m_type> *>(p_ptr);              \
			int len = p_vec.size();                                                      \
			dv->resize(len);                                                             \
			{                                                                            \
				m_type *w = dv->ptrw();                                                  \
				for (int i = 0; i < len; i++) {                                          \
					w[i] = p_vec[i];                                                     \
				}                                                                        \
			}                                                                            \
		}                                                                                \
	};                                                                                   \
	template <>                                                                          \
	struct PtrToArg<const Vector<m_type> &> {                                            \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {                \
			const Vector<m_type> *dvs = reinterpret_cast<const Vector<m_type> *>(p_ptr); \
			Vector<m_type> ret;                                                          \
			int len = dvs->size();                                                       \
			ret.resize(len);                                                             \
			{                                                                            \
				const m_type *r = dvs->ptr();                                            \
				for (int i = 0; i < len; i++) {                                          \
					ret.write[i] = r[i];                                                 \
				}                                                                        \
			}                                                                            \
			return ret;                                                                  \
		}                                                                                \
	}

// No EncodeT because direct pointer conversion not possible.
#define MAKE_VECARG_ALT(m_type, m_type_alt)                                               \
	template <>                                                                           \
	struct PtrToArg<Vector<m_type_alt>> {                                                 \
		_FORCE_INLINE_ static Vector<m_type_alt> convert(const void *p_ptr) {             \
			const Vector<m_type> *dvs = reinterpret_cast<const Vector<m_type> *>(p_ptr);  \
			Vector<m_type_alt> ret;                                                       \
			int len = dvs->size();                                                        \
			ret.resize(len);                                                              \
			{                                                                             \
				const m_type *r = dvs->ptr();                                             \
				for (int i = 0; i < len; i++) {                                           \
					ret.write[i] = r[i];                                                  \
				}                                                                         \
			}                                                                             \
			return ret;                                                                   \
		}                                                                                 \
		_FORCE_INLINE_ static void encode(const Vector<m_type_alt> &p_vec, void *p_ptr) { \
			Vector<m_type> *dv = reinterpret_cast<Vector<m_type> *>(p_ptr);               \
			int len = p_vec.size();                                                       \
			dv->resize(len);                                                              \
			{                                                                             \
				m_type *w = dv->ptrw();                                                   \
				for (int i = 0; i < len; i++) {                                           \
					w[i] = p_vec[i];                                                      \
				}                                                                         \
			}                                                                             \
		}                                                                                 \
	};                                                                                    \
	template <>                                                                           \
	struct PtrToArg<const Vector<m_type_alt> &> {                                         \
		_FORCE_INLINE_ static Vector<m_type_alt> convert(const void *p_ptr) {             \
			const Vector<m_type> *dvs = reinterpret_cast<const Vector<m_type> *>(p_ptr);  \
			Vector<m_type_alt> ret;                                                       \
			int len = dvs->size();                                                        \
			ret.resize(len);                                                              \
			{                                                                             \
				const m_type *r = dvs->ptr();                                             \
				for (int i = 0; i < len; i++) {                                           \
					ret.write[i] = r[i];                                                  \
				}                                                                         \
			}                                                                             \
			return ret;                                                                   \
		}                                                                                 \
	}

MAKE_VECARG_ALT(String, StringName);

// For stuff that gets converted to Array vectors.

// No EncodeT because direct pointer conversion not possible.
#define MAKE_VECARR(m_type)                                                           \
	template <>                                                                       \
	struct PtrToArg<Vector<m_type>> {                                                 \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {             \
			const Array *arr = reinterpret_cast<const Array *>(p_ptr);                \
			Vector<m_type> ret;                                                       \
			int len = arr->size();                                                    \
			ret.resize(len);                                                          \
			for (int i = 0; i < len; i++) {                                           \
				ret.write[i] = (*arr)[i];                                             \
			}                                                                         \
			return ret;                                                               \
		}                                                                             \
		_FORCE_INLINE_ static void encode(const Vector<m_type> &p_vec, void *p_ptr) { \
			Array *arr = reinterpret_cast<Array *>(p_ptr);                            \
			int len = p_vec.size();                                                   \
			arr->resize(len);                                                         \
			for (int i = 0; i < len; i++) {                                           \
				(*arr)[i] = p_vec[i];                                                 \
			}                                                                         \
		}                                                                             \
	};                                                                                \
	template <>                                                                       \
	struct PtrToArg<const Vector<m_type> &> {                                         \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {             \
			const Array *arr = reinterpret_cast<const Array *>(p_ptr);                \
			Vector<m_type> ret;                                                       \
			int len = arr->size();                                                    \
			ret.resize(len);                                                          \
			for (int i = 0; i < len; i++) {                                           \
				ret.write[i] = (*arr)[i];                                             \
			}                                                                         \
			return ret;                                                               \
		}                                                                             \
	}

MAKE_VECARR(Variant);
MAKE_VECARR(RID);
MAKE_VECARR(Plane);

// No EncodeT because direct pointer conversion not possible.
#define MAKE_DVECARR(m_type)                                                          \
	template <>                                                                       \
	struct PtrToArg<Vector<m_type>> {                                                 \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {             \
			const Array *arr = reinterpret_cast<const Array *>(p_ptr);                \
			Vector<m_type> ret;                                                       \
			int len = arr->size();                                                    \
			ret.resize(len);                                                          \
			{                                                                         \
				m_type *w = ret.ptrw();                                               \
				for (int i = 0; i < len; i++) {                                       \
					w[i] = (*arr)[i];                                                 \
				}                                                                     \
			}                                                                         \
			return ret;                                                               \
		}                                                                             \
		_FORCE_INLINE_ static void encode(const Vector<m_type> &p_vec, void *p_ptr) { \
			Array *arr = reinterpret_cast<Array *>(p_ptr);                            \
			int len = p_vec.size();                                                   \
			arr->resize(len);                                                         \
			{                                                                         \
				const m_type *r = p_vec.ptr();                                        \
				for (int i = 0; i < len; i++) {                                       \
					(*arr)[i] = r[i];                                                 \
				}                                                                     \
			}                                                                         \
		}                                                                             \
	};                                                                                \
	template <>                                                                       \
	struct PtrToArg<const Vector<m_type> &> {                                         \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {             \
			const Array *arr = reinterpret_cast<const Array *>(p_ptr);                \
			Vector<m_type> ret;                                                       \
			int len = arr->size();                                                    \
			ret.resize(len);                                                          \
			{                                                                         \
				m_type *w = ret.ptrw();                                               \
				for (int i = 0; i < len; i++) {                                       \
					w[i] = (*arr)[i];                                                 \
				}                                                                     \
			}                                                                         \
			return ret;                                                               \
		}                                                                             \
	}

// Special case for IPAddress.

// No EncodeT because direct pointer conversion not possible.
#define MAKE_STRINGCONV_BY_REFERENCE(m_type)                                  \
	template <>                                                               \
	struct PtrToArg<m_type> {                                                 \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {             \
			m_type s = *reinterpret_cast<const String *>(p_ptr);              \
			return s;                                                         \
		}                                                                     \
		_FORCE_INLINE_ static void encode(const m_type &p_vec, void *p_ptr) { \
			String *arr = reinterpret_cast<String *>(p_ptr);                  \
			*arr = p_vec;                                                     \
		}                                                                     \
	};                                                                        \
                                                                              \
	template <>                                                               \
	struct PtrToArg<const m_type &> {                                         \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {             \
			m_type s = *reinterpret_cast<const String *>(p_ptr);              \
			return s;                                                         \
		}                                                                     \
	}

MAKE_STRINGCONV_BY_REFERENCE(IPAddress);

// No EncodeT because direct pointer conversion not possible.
template <>
struct PtrToArg<Vector<Face3>> {
	_FORCE_INLINE_ static Vector<Face3> convert(const void *p_ptr) {
		const Vector<Vector3> *dvs = reinterpret_cast<const Vector<Vector3> *>(p_ptr);
		Vector<Face3> ret;
		int len = dvs->size() / 3;
		ret.resize(len);
		{
			const Vector3 *r = dvs->ptr();
			Face3 *w = ret.ptrw();
			for (int i = 0; i < len; i++) {
				w[i].vertex[0] = r[i * 3 + 0];
				w[i].vertex[1] = r[i * 3 + 1];
				w[i].vertex[2] = r[i * 3 + 2];
			}
		}
		return ret;
	}
	_FORCE_INLINE_ static void encode(const Vector<Face3> &p_vec, void *p_ptr) {
		Vector<Vector3> *arr = reinterpret_cast<Vector<Vector3> *>(p_ptr);
		int len = p_vec.size();
		arr->resize(len * 3);
		{
			const Face3 *r = p_vec.ptr();
			Vector3 *w = arr->ptrw();
			for (int i = 0; i < len; i++) {
				w[i * 3 + 0] = r[i].vertex[0];
				w[i * 3 + 1] = r[i].vertex[1];
				w[i * 3 + 2] = r[i].vertex[2];
			}
		}
	}
};

// No EncodeT because direct pointer conversion not possible.
template <>
struct PtrToArg<const Vector<Face3> &> {
	_FORCE_INLINE_ static Vector<Face3> convert(const void *p_ptr) {
		const Vector<Vector3> *dvs = reinterpret_cast<const Vector<Vector3> *>(p_ptr);
		Vector<Face3> ret;
		int len = dvs->size() / 3;
		ret.resize(len);
		{
			const Vector3 *r = dvs->ptr();
			Face3 *w = ret.ptrw();
			for (int i = 0; i < len; i++) {
				w[i].vertex[0] = r[i * 3 + 0];
				w[i].vertex[1] = r[i * 3 + 1];
				w[i].vertex[2] = r[i * 3 + 2];
			}
		}
		return ret;
	}
};
