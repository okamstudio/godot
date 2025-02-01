/**************************************************************************/
/*  interop_types.h                                                       */
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

#include "core/math/math_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// This is taken from the old GDNative, which was removed.

#define GODOT_VARIANT_SIZE (sizeof(real_t) * 4 + sizeof(int64_t))

typedef struct {
	uint8_t _dont_touch_that[GODOT_VARIANT_SIZE];
} godot_variant;

#define GODOT_ARRAY_SIZE sizeof(void *)

typedef struct {
	uint8_t _dont_touch_that[GODOT_ARRAY_SIZE];
} godot_array;

#define GODOT_DICTIONARY_SIZE sizeof(void *)

typedef struct {
	uint8_t _dont_touch_that[GODOT_DICTIONARY_SIZE];
} godot_dictionary;

#define GODOT_STRING_SIZE sizeof(void *)

typedef struct {
	uint8_t _dont_touch_that[GODOT_STRING_SIZE];
} godot_string;

#define GODOT_STRING_NAME_SIZE sizeof(void *)

typedef struct {
	uint8_t _dont_touch_that[GODOT_STRING_NAME_SIZE];
} godot_string_name;

#define GODOT_PACKED_ARRAY_SIZE (2 * sizeof(void *))

typedef struct {
	uint8_t _dont_touch_that[GODOT_PACKED_ARRAY_SIZE];
} godot_packed_array;

#define GODOT_VECTOR2_SIZE (sizeof(real_t) * 2)

typedef struct {
	uint8_t _dont_touch_that[GODOT_VECTOR2_SIZE];
} godot_vector2;

#define GODOT_VECTOR2I_SIZE (sizeof(int32_t) * 2)

typedef struct {
	uint8_t _dont_touch_that[GODOT_VECTOR2I_SIZE];
} godot_vector2i;

#define GODOT_RECT2_SIZE (sizeof(real_t) * 4)

typedef struct godot_rect2 {
	uint8_t _dont_touch_that[GODOT_RECT2_SIZE];
} godot_rect2;

#define GODOT_RECT2I_SIZE (sizeof(int32_t) * 4)

typedef struct godot_rect2i {
	uint8_t _dont_touch_that[GODOT_RECT2I_SIZE];
} godot_rect2i;

#define GODOT_VECTOR3_SIZE (sizeof(real_t) * 3)

typedef struct {
	uint8_t _dont_touch_that[GODOT_VECTOR3_SIZE];
} godot_vector3;

#define GODOT_VECTOR3I_SIZE (sizeof(int32_t) * 3)

typedef struct {
	uint8_t _dont_touch_that[GODOT_VECTOR3I_SIZE];
} godot_vector3i;

#define GODOT_TRANSFORM2D_SIZE (sizeof(real_t) * 6)

typedef struct {
	uint8_t _dont_touch_that[GODOT_TRANSFORM2D_SIZE];
} godot_transform2d;

#define GODOT_VECTOR4_SIZE (sizeof(real_t) * 4)

typedef struct {
	uint8_t _dont_touch_that[GODOT_VECTOR4_SIZE];
} godot_vector4;

#define GODOT_VECTOR4I_SIZE (sizeof(int32_t) * 4)

typedef struct {
	uint8_t _dont_touch_that[GODOT_VECTOR4I_SIZE];
} godot_vector4i;

#define GODOT_PLANE_SIZE (sizeof(real_t) * 4)

typedef struct {
	uint8_t _dont_touch_that[GODOT_PLANE_SIZE];
} godot_plane;

#define GODOT_QUATERNION_SIZE (sizeof(real_t) * 4)

typedef struct {
	uint8_t _dont_touch_that[GODOT_QUATERNION_SIZE];
} godot_quaternion;

#define GODOT_AABB_SIZE (sizeof(real_t) * 6)

typedef struct {
	uint8_t _dont_touch_that[GODOT_AABB_SIZE];
} godot_aabb;

#define GODOT_BASIS_SIZE (sizeof(real_t) * 9)

typedef struct {
	uint8_t _dont_touch_that[GODOT_BASIS_SIZE];
} godot_basis;

#define GODOT_TRANSFORM3D_SIZE (sizeof(real_t) * 12)

typedef struct {
	uint8_t _dont_touch_that[GODOT_TRANSFORM3D_SIZE];
} godot_transform3d;

#define GODOT_PROJECTION_SIZE (sizeof(real_t) * 4 * 4)

typedef struct {
	uint8_t _dont_touch_that[GODOT_PROJECTION_SIZE];
} godot_projection;

// Colors should always use 32-bit floats, so don't use real_t here.
#define GODOT_COLOR_SIZE (sizeof(float) * 4)

typedef struct {
	uint8_t _dont_touch_that[GODOT_COLOR_SIZE];
} godot_color;

#define GODOT_NODE_PATH_SIZE sizeof(void *)

typedef struct {
	uint8_t _dont_touch_that[GODOT_NODE_PATH_SIZE];
} godot_node_path;

#define GODOT_RID_SIZE sizeof(uint64_t)

typedef struct {
	uint8_t _dont_touch_that[GODOT_RID_SIZE];
} godot_rid;

// Alignment hardcoded in `core/variant/callable.h`.
#define GODOT_CALLABLE_SIZE (16)

typedef struct {
	uint8_t _dont_touch_that[GODOT_CALLABLE_SIZE];
} godot_callable;

// Alignment hardcoded in `core/variant/callable.h`.
#define GODOT_SIGNAL_SIZE (16)

typedef struct {
	uint8_t _dont_touch_that[GODOT_SIGNAL_SIZE];
} godot_signal;

#ifdef __cplusplus
}
#endif
