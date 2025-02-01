/**************************************************************************/
/*  math_defs.h                                                           */
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

#define CMP_EPSILON 0.00001
#define CMP_EPSILON2 (CMP_EPSILON * CMP_EPSILON)

#define CMP_NORMALIZE_TOLERANCE 0.000001
#define CMP_POINT_IN_PLANE_EPSILON 0.00001

#define Math_SQRT12 0.7071067811865475244008443621048490
#define Math_SQRT2 1.4142135623730950488016887242
#define Math_LN2 0.6931471805599453094172321215
#define Math_TAU 6.2831853071795864769252867666
#define Math_PI 3.1415926535897932384626433833
#define Math_E 2.7182818284590452353602874714

#ifdef DEBUG_ENABLED
#define MATH_CHECKS
#endif

//this epsilon is for values related to a unit size (scalar or vector len)
#ifdef PRECISE_MATH_CHECKS
#define UNIT_EPSILON 0.00001
#else
//tolerate some more floating point error normally
#define UNIT_EPSILON 0.001
#endif

#define USEC_TO_SEC(m_usec) ((m_usec) / 1000000.0)

enum ClockDirection {
	CLOCKWISE,
	COUNTERCLOCKWISE
};

enum Orientation {
	HORIZONTAL,
	VERTICAL
};

enum HorizontalAlignment {
	HORIZONTAL_ALIGNMENT_LEFT,
	HORIZONTAL_ALIGNMENT_CENTER,
	HORIZONTAL_ALIGNMENT_RIGHT,
	HORIZONTAL_ALIGNMENT_FILL,
};

enum VerticalAlignment {
	VERTICAL_ALIGNMENT_TOP,
	VERTICAL_ALIGNMENT_CENTER,
	VERTICAL_ALIGNMENT_BOTTOM,
	VERTICAL_ALIGNMENT_FILL,
};

enum InlineAlignment {
	// Image alignment points.
	INLINE_ALIGNMENT_TOP_TO = 0b0000,
	INLINE_ALIGNMENT_CENTER_TO = 0b0001,
	INLINE_ALIGNMENT_BASELINE_TO = 0b0011,
	INLINE_ALIGNMENT_BOTTOM_TO = 0b0010,
	INLINE_ALIGNMENT_IMAGE_MASK = 0b0011,

	// Text alignment points.
	INLINE_ALIGNMENT_TO_TOP = 0b0000,
	INLINE_ALIGNMENT_TO_CENTER = 0b0100,
	INLINE_ALIGNMENT_TO_BASELINE = 0b1000,
	INLINE_ALIGNMENT_TO_BOTTOM = 0b1100,
	INLINE_ALIGNMENT_TEXT_MASK = 0b1100,

	// Presets.
	INLINE_ALIGNMENT_TOP = INLINE_ALIGNMENT_TOP_TO | INLINE_ALIGNMENT_TO_TOP,
	INLINE_ALIGNMENT_CENTER = INLINE_ALIGNMENT_CENTER_TO | INLINE_ALIGNMENT_TO_CENTER,
	INLINE_ALIGNMENT_BOTTOM = INLINE_ALIGNMENT_BOTTOM_TO | INLINE_ALIGNMENT_TO_BOTTOM
};

enum Side {
	SIDE_LEFT,
	SIDE_TOP,
	SIDE_RIGHT,
	SIDE_BOTTOM
};

enum Corner {
	CORNER_TOP_LEFT,
	CORNER_TOP_RIGHT,
	CORNER_BOTTOM_RIGHT,
	CORNER_BOTTOM_LEFT
};

enum class EulerOrder {
	XYZ,
	XZY,
	YXZ,
	YZX,
	ZXY,
	ZYX
};

/**
 * The "Real" type is an abstract type used for real numbers, such as 1.5,
 * in contrast to integer numbers. Precision can be controlled with the
 * presence or absence of the REAL_T_IS_DOUBLE define.
 */
#ifdef REAL_T_IS_DOUBLE
typedef double real_t;
#else
typedef float real_t;
#endif
