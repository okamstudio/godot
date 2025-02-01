/**************************************************************************/
/*  variant_destruct.h                                                    */
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

#include "core/object/class_db.h"

template <typename T>
struct VariantDestruct {};

#define MAKE_PTRDESTRUCT(m_type)                               \
	template <>                                                \
	struct VariantDestruct<m_type> {                           \
		_FORCE_INLINE_ static void ptr_destruct(void *p_ptr) { \
			reinterpret_cast<m_type *>(p_ptr)->~m_type();      \
		}                                                      \
		_FORCE_INLINE_ static Variant::Type get_base_type() {  \
			return GetTypeInfo<m_type>::VARIANT_TYPE;          \
		}                                                      \
	}

MAKE_PTRDESTRUCT(String);
MAKE_PTRDESTRUCT(StringName);
MAKE_PTRDESTRUCT(NodePath);
MAKE_PTRDESTRUCT(Callable);
MAKE_PTRDESTRUCT(Signal);
MAKE_PTRDESTRUCT(Dictionary);
MAKE_PTRDESTRUCT(Array);
MAKE_PTRDESTRUCT(PackedByteArray);
MAKE_PTRDESTRUCT(PackedInt32Array);
MAKE_PTRDESTRUCT(PackedInt64Array);
MAKE_PTRDESTRUCT(PackedFloat32Array);
MAKE_PTRDESTRUCT(PackedFloat64Array);
MAKE_PTRDESTRUCT(PackedStringArray);
MAKE_PTRDESTRUCT(PackedVector2Array);
MAKE_PTRDESTRUCT(PackedVector3Array);
MAKE_PTRDESTRUCT(PackedColorArray);
MAKE_PTRDESTRUCT(PackedVector4Array);

#undef MAKE_PTRDESTRUCT
