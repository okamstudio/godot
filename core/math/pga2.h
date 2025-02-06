/**************************************************************************/
/*  pga2.h                                                                */
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

#ifndef PGA2_H
#define PGA2_H

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"
#include "core/string/ustring.h"

struct _NO_DISCARD_ PGAVector2 {
	static const int AXIS_COUNT = 8;

	enum GradeMask {
		GRADE_MASK_0 = 0b0001,
		GRADE_MASK_1 = 0b0010,
		GRADE_MASK_2 = 0b0100,
		GRADE_MASK_3 = 0b1000,

		// Presets
		GRADE_MASK_SCALAR = GRADE_MASK_0,
		GRADE_MASK_LINE = GRADE_MASK_1,
		GRADE_MASK_POINT = GRADE_MASK_2,
		GRADE_MASK_MOTOR = GRADE_MASK_0 | GRADE_MASK_2
	};

	union {
		struct {
			real_t e;
			real_t e0;
			real_t e1;
			real_t e2;
			real_t e01;
			real_t e02;
			real_t e12;
			real_t e012;
		};

		real_t coord[AXIS_COUNT] = { 0 };
	};

	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < AXIS_COUNT);
		return coord[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < AXIS_COUNT);
		return coord[p_axis];
	}

	// Mathematical operations
	_FORCE_INLINE_ PGAVector2 geometric_product(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 outer_product(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 inner_product(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 regressive_product(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 commutator_product(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 sandwich_product(const PGAVector2 &p_mv) const;

	_FORCE_INLINE_ PGAVector2 reverse() const;
	_FORCE_INLINE_ PGAVector2 dual() const;
	_FORCE_INLINE_ PGAVector2 normalize() const;
	_FORCE_INLINE_ PGAVector2 grade(GradeMask mask) const;
	_FORCE_INLINE_ real_t norm_square() const;
	_FORCE_INLINE_ real_t norm() const;

	_FORCE_INLINE_ void reverse_in_place();
	_FORCE_INLINE_ void dual_in_place();
	_FORCE_INLINE_ void normalize_in_place();
	_FORCE_INLINE_ void grade_in_place(GradeMask mask);

	// C++ Operators
	_FORCE_INLINE_ PGAVector2 operator-() const;
	_FORCE_INLINE_ PGAVector2 operator~() const;
	_FORCE_INLINE_ PGAVector2 operator!() const;

	_FORCE_INLINE_ void operator+=(const PGAVector2 &p_mv);
	_FORCE_INLINE_ void operator-=(const PGAVector2 &p_mv);
	_FORCE_INLINE_ void operator*=(const real_t p_s);
	_FORCE_INLINE_ void operator/=(const real_t p_s);

	_FORCE_INLINE_ PGAVector2 operator+(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 operator-(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 operator*(const real_t p_s) const;
	_FORCE_INLINE_ PGAVector2 operator/(const real_t p_s) const;

	_FORCE_INLINE_ PGAVector2 operator*(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 operator^(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 operator|(const PGAVector2 &p_mv) const;
	_FORCE_INLINE_ PGAVector2 operator&(const PGAVector2 &p_mv) const;

	// Convertor to subtypes
	PGALine2 to_line() const;
	PGAPoint2 to_point() const;
	PGAMotor2 to_motor() const;

	// Convertor from subtypes
	PGAVector2() {}
	PGAVector2(const PGALine2 &p_line);
	PGAVector2(const PGAPoint2 &p_point);
	PGAVector2(const PGAMotor2 &p_motor);
	PGAVector2(const real_t p_s);
};

struct _NO_DISCARD_ PGALine2 {
	static const int AXIS_COUNT = 3;

	union {
		struct {
			real_t e0;
			real_t e1;
			real_t e2;
		};

		real_t coord[AXIS_COUNT] = { 0 };
	};

	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < AXIS_COUNT);
		return coord[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < AXIS_COUNT);
		return coord[p_axis];
	}

	// Mathematical operations
	_FORCE_INLINE_ PGAMotor2 geometric_product(const PGALine2 &p_line) const;
	_FORCE_INLINE_ PGAPoint2 outer_product(const PGALine2 &p_line) const;
	_FORCE_INLINE_ PGALine2 inner_product(const PGAPoint2 &p_point) const;
	_FORCE_INLINE_ real_t inner_product(const PGALine2 &p_line) const;
	_FORCE_INLINE_ PGALine2 sandwich_product(const PGALine2 &p_line) const;
	_FORCE_INLINE_ PGAPoint2 sandwich_product(const PGAPoint2 &p_point) const;

	_FORCE_INLINE_ PGALine2 reverse() const;
	_FORCE_INLINE_ PGAPoint2 dual() const;
	_FORCE_INLINE_ PGALine2 normalize() const;
	_FORCE_INLINE_ real_t norm_square() const;
	_FORCE_INLINE_ real_t norm() const;

	_FORCE_INLINE_ void reverse_in_place() {}
	_FORCE_INLINE_ void normalize_in_place();

	// C++ Operators
	_FORCE_INLINE_ PGALine2 operator-() const;
	_FORCE_INLINE_ PGALine2 operator~() const;
	_FORCE_INLINE_ PGAPoint2 operator!() const;

	_FORCE_INLINE_ void operator+=(const PGALine2 &p_line);
	_FORCE_INLINE_ void operator-=(const PGALine2 &p_line);
	_FORCE_INLINE_ void operator*=(const real_t p_s);
	_FORCE_INLINE_ void operator/=(const real_t p_s);

	_FORCE_INLINE_ PGALine2 operator+(const PGALine2 &p_line) const;
	_FORCE_INLINE_ PGALine2 operator-(const PGALine2 &p_line) const;
	_FORCE_INLINE_ PGALine2 operator*(const real_t p_s) const;
	_FORCE_INLINE_ PGALine2 operator/(const real_t p_s) const;

	_FORCE_INLINE_ PGAMotor2 operator*(const PGALine2 &p_line) const;
	_FORCE_INLINE_ PGAPoint2 operator^(const PGALine2 &p_line) const;
	_FORCE_INLINE_ PGALine2 operator|(const PGAPoint2 &p_point) const;
	_FORCE_INLINE_ real_t operator|(const PGALine2 &p_line) const;

	// Alias
	_FORCE_INLINE_ PGAPoint2 join(const PGALine2 &p_line) const { return outer_product(p_line); }
	_FORCE_INLINE_ PGALine2 apply(const PGALine2 &p_line) const { return sandwich_product(p_line); }
	_FORCE_INLINE_ PGAPoint2 reflect(const PGAPoint2 &p_point) const { return sandwich_product(p_point); }
	_FORCE_INLINE_ PGALine2 ortho(const PGAPoint2 &p_point) const { return inner_product(p_point); }
};

struct _NO_DISCARD_ PGAPoint2 {
	static const int AXIS_COUNT = 3;

	union {
		struct {
			real_t e01;
			real_t e02;
			real_t e12;
		};

		real_t coord[AXIS_COUNT] = { 0 };
	};

	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < AXIS_COUNT);
		return coord[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < AXIS_COUNT);
		return coord[p_axis];
	}

	// Mathematical operations
	_FORCE_INLINE_ PGALine2 regressive_product(const PGAPoint2 &p_point) const;
	_FORCE_INLINE_ PGALine2 inner_product(const PGALine2 &p_point) const;

	_FORCE_INLINE_ PGAPoint2 reverse() const;
	_FORCE_INLINE_ PGALine2 dual() const;
	_FORCE_INLINE_ PGAVector2 normalize() const;
	_FORCE_INLINE_ real_t norm_square() const;
	_FORCE_INLINE_ real_t norm() const;

	_FORCE_INLINE_ void normalize_in_place();
	_FORCE_INLINE_ void reverse_in_place() {}

	_FORCE_INLINE_ PGAMotor2 exp(real_t &p_s);

	// C++ Operators
	_FORCE_INLINE_ PGAPoint2 operator-() const;
	_FORCE_INLINE_ PGAPoint2 operator~() const;
	_FORCE_INLINE_ PGALine2 operator!() const;

	_FORCE_INLINE_ void operator+=(const PGAPoint2 &p_point);
	_FORCE_INLINE_ void operator-=(const PGAPoint2 &p_point);
	_FORCE_INLINE_ void operator*=(const real_t p_s);
	_FORCE_INLINE_ void operator/=(const real_t p_s);

	_FORCE_INLINE_ PGAPoint2 operator+(const PGAPoint2 &p_point) const;
	_FORCE_INLINE_ PGAPoint2 operator-(const PGAPoint2 &p_point) const;
	_FORCE_INLINE_ PGAPoint2 operator*(const real_t p_s) const;
	_FORCE_INLINE_ PGAPoint2 operator/(const real_t p_s) const;

	_FORCE_INLINE_ PGALine2 operator&(const PGAPoint2 &p_point) const;

	// Alias
	_FORCE_INLINE_ PGALine2 meet(const PGAPoint2 &p_point) const { return regressive_product(p_point); }
	_FORCE_INLINE_ PGALine2 ortho(const PGALine2 &p_line) const { return inner_product(p_line); }
};

struct _NO_DISCARD_ PGAMotor2 {
	static const int AXIS_COUNT = 4;

	union {
		struct {
			real_t e;
			real_t e01;
			real_t e02;
			real_t e12;
		};

		real_t coord[AXIS_COUNT] = { 0 };
	};

	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < AXIS_COUNT);
		return coord[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < AXIS_COUNT);
		return coord[p_axis];
	}

	// Mathematical operations
	_FORCE_INLINE_ PGAMotor2 geometric_product(const PGAMotor2 &p_motor) const;
	_FORCE_INLINE_ PGAPoint2 sandwich_product(const PGAPoint2 &p_point) const;
	_FORCE_INLINE_ PGALine2 sandwich_product(const PGALine2 &p_line) const;

	_FORCE_INLINE_ PGAMotor2 reverse() const;
	_FORCE_INLINE_ PGAMotor2 normalize() const;
	_FORCE_INLINE_ real_t norm_square() const;
	_FORCE_INLINE_ real_t norm() const;

	_FORCE_INLINE_ void reverse_in_place();
	_FORCE_INLINE_ void normalize_in_place();

	// C++ Operators
	_FORCE_INLINE_ PGAMotor2 operator-() const;
	_FORCE_INLINE_ PGAMotor2 operator~() const;

	_FORCE_INLINE_ void operator+=(const PGAMotor2 &p_motor);
	_FORCE_INLINE_ void operator-=(const PGAMotor2 &p_motor);
	_FORCE_INLINE_ void operator*=(const real_t p_s);
	_FORCE_INLINE_ void operator/=(const real_t p_s);

	_FORCE_INLINE_ PGAMotor2 operator+(const PGAMotor2 &p_motor) const;
	_FORCE_INLINE_ PGAMotor2 operator-(const PGAMotor2 &p_motor) const;
	_FORCE_INLINE_ PGAMotor2 operator*(const real_t p_s) const;
	_FORCE_INLINE_ PGAMotor2 operator/(const real_t p_s) const;

	_FORCE_INLINE_ PGAMotor2 operator*(const PGAMotor2 &p_motor) const;

	// Alias
	_FORCE_INLINE_ PGAMotor2 compose(const PGAMotor2 &p_motor) const { return geometric_product(p_motor); }
	_FORCE_INLINE_ PGAPoint2 apply(const PGAPoint2 &p_point) const { return sandwich_product(p_point); }
	_FORCE_INLINE_ PGALine2 apply(const PGALine2 &p_line) const { return sandwich_product(p_line); }
};

#endif // PGA2_H