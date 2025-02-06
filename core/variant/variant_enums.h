/**************************************************************************/
/*  variant_enums.h                                                       */
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

#ifndef VARIANT_ENUMS_H
#define VARIANT_ENUMS_H

// HACK: By wrapping these enums in a namespace, we're able to hoist the enumeration itself into
// the global namespace while keeping the enumeration constants constrained. This has a similar
// effect to the behavior of scoped enums, where the constants will require the type preceding it,
// but does so without changing the existing type/conversion properties. This workaround can be
// removed if these enums ever become scoped (ie: enum class).

namespace godot {
namespace details {

// WARNING: If this changes, the table in variant_op must be updated.
enum VariantType {
	NIL,

	// Atomic types.
	BOOL,
	INT,
	FLOAT,
	STRING,

	// Math types.
	VECTOR2,
	VECTOR2I,
	RECT2,
	RECT2I,
	VECTOR3,
	VECTOR3I,
	TRANSFORM2D,
	VECTOR4,
	VECTOR4I,
	PLANE,
	QUATERNION,
	AABB,
	BASIS,
	TRANSFORM3D,
	PROJECTION,

	// Misc types.
	COLOR,
	STRING_NAME,
	NODE_PATH,
	RID,
	OBJECT,
	CALLABLE,
	SIGNAL,
	DICTIONARY,
	ARRAY,

	// Packed arrays.
	PACKED_BYTE_ARRAY,
	PACKED_INT32_ARRAY,
	PACKED_INT64_ARRAY,
	PACKED_FLOAT32_ARRAY,
	PACKED_FLOAT64_ARRAY,
	PACKED_STRING_ARRAY,
	PACKED_VECTOR2_ARRAY,
	PACKED_VECTOR3_ARRAY,
	PACKED_COLOR_ARRAY,
	PACKED_VECTOR4_ARRAY,

	VARIANT_MAX,
};

// WARNING: If this changes, the table in variant_op must be updated.
enum VariantOperator {
	// Comparison operators.
	OP_EQUAL,
	OP_NOT_EQUAL,
	OP_LESS,
	OP_LESS_EQUAL,
	OP_GREATER,
	OP_GREATER_EQUAL,

	// Mathematic operators.
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NEGATE,
	OP_POSITIVE,
	OP_MODULE,
	OP_POWER,

	// Bitwise operators.
	OP_SHIFT_LEFT,
	OP_SHIFT_RIGHT,
	OP_BIT_AND,
	OP_BIT_OR,
	OP_BIT_XOR,
	OP_BIT_NEGATE,

	// Logical operators.
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_NOT,

	// Containment operators.
	OP_IN,

	OP_MAX,
};

} // namespace details
} // namespace godot

using VariantType = godot::details::VariantType;
using VariantOperator = godot::details::VariantOperator;

#endif // VARIANT_ENUMS_H
