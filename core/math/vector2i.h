/**************************************************************************/
/*  vector2i.h                                                            */
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

#ifndef VECTOR2I_H
#define VECTOR2I_H

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"

class String;
struct Vector2;

struct [[nodiscard]] Vector2i {
	static const int AXIS_COUNT = 2;

	enum Axis {
		AXIS_X,
		AXIS_Y,
	};

	union {
		// NOLINTBEGIN(modernize-use-default-member-init)
		struct {
			int32_t x;
			int32_t y;
		};

		struct {
			int32_t width;
			int32_t height;
		};

		int32_t coord[2] = { 0 };
		// NOLINTEND(modernize-use-default-member-init)
	};

	_FORCE_INLINE_ int32_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 2);
		return coord[p_axis];
	}
	_FORCE_INLINE_ const int32_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 2);
		return coord[p_axis];
	}

	_FORCE_INLINE_ Vector2i::Axis min_axis_index() const {
		return x < y ? Vector2i::AXIS_X : Vector2i::AXIS_Y;
	}

	_FORCE_INLINE_ Vector2i::Axis max_axis_index() const {
		return x < y ? Vector2i::AXIS_Y : Vector2i::AXIS_X;
	}

	Vector2i min(const Vector2i &p_vector2i) const {
		return Vector2i(MIN(x, p_vector2i.x), MIN(y, p_vector2i.y));
	}

	Vector2i mini(int32_t p_scalar) const {
		return Vector2i(MIN(x, p_scalar), MIN(y, p_scalar));
	}

	Vector2i max(const Vector2i &p_vector2i) const {
		return Vector2i(MAX(x, p_vector2i.x), MAX(y, p_vector2i.y));
	}

	Vector2i maxi(int32_t p_scalar) const {
		return Vector2i(MAX(x, p_scalar), MAX(y, p_scalar));
	}

	double distance_to(const Vector2i &p_to) const {
		return (p_to - *this).length();
	}

	int64_t distance_squared_to(const Vector2i &p_to) const {
		return (p_to - *this).length_squared();
	}

	constexpr Vector2i operator+(const Vector2i &p_v) const;
	constexpr void operator+=(const Vector2i &p_v);
	constexpr Vector2i operator-(const Vector2i &p_v) const;
	constexpr void operator-=(const Vector2i &p_v);
	constexpr Vector2i operator*(const Vector2i &p_v1) const;

	constexpr Vector2i operator*(int32_t p_rvalue) const;
	constexpr void operator*=(int32_t p_rvalue);

	constexpr Vector2i operator/(const Vector2i &p_v1) const;
	constexpr Vector2i operator/(int32_t p_rvalue) const;
	constexpr void operator/=(int32_t p_rvalue);

	constexpr Vector2i operator%(const Vector2i &p_v1) const;
	constexpr Vector2i operator%(int32_t p_rvalue) const;
	constexpr void operator%=(int32_t p_rvalue);

	constexpr Vector2i operator-() const;
	constexpr bool operator<(const Vector2i &p_vec2) const { return (x == p_vec2.x) ? (y < p_vec2.y) : (x < p_vec2.x); }
	constexpr bool operator>(const Vector2i &p_vec2) const { return (x == p_vec2.x) ? (y > p_vec2.y) : (x > p_vec2.x); }

	constexpr bool operator<=(const Vector2i &p_vec2) const { return x == p_vec2.x ? (y <= p_vec2.y) : (x < p_vec2.x); }
	constexpr bool operator>=(const Vector2i &p_vec2) const { return x == p_vec2.x ? (y >= p_vec2.y) : (x > p_vec2.x); }

	constexpr bool operator==(const Vector2i &p_vec2) const;
	constexpr bool operator!=(const Vector2i &p_vec2) const;

	int64_t length_squared() const;
	double length() const;

	real_t aspect() const { return width / (real_t)height; }
	Vector2i sign() const { return Vector2i(SIGN(x), SIGN(y)); }
	Vector2i abs() const { return Vector2i(Math::abs(x), Math::abs(y)); }
	Vector2i clamp(const Vector2i &p_min, const Vector2i &p_max) const;
	Vector2i clampi(int32_t p_min, int32_t p_max) const;
	Vector2i snapped(const Vector2i &p_step) const;
	Vector2i snappedi(int32_t p_step) const;

	operator String() const;
	operator Vector2() const;

	// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
	constexpr Vector2i() :
			x(0), y(0) {}
	constexpr Vector2i(int32_t p_x, int32_t p_y) :
			x(p_x), y(p_y) {}
	// NOLINTEND(cppcoreguidelines-pro-type-member-init)
};

constexpr Vector2i Vector2i::operator+(const Vector2i &p_v) const {
	return Vector2i(x + p_v.x, y + p_v.y);
}

constexpr void Vector2i::operator+=(const Vector2i &p_v) {
	x += p_v.x;
	y += p_v.y;
}

constexpr Vector2i Vector2i::operator-(const Vector2i &p_v) const {
	return Vector2i(x - p_v.x, y - p_v.y);
}

constexpr void Vector2i::operator-=(const Vector2i &p_v) {
	x -= p_v.x;
	y -= p_v.y;
}

constexpr Vector2i Vector2i::operator*(const Vector2i &p_v1) const {
	return Vector2i(x * p_v1.x, y * p_v1.y);
}

constexpr Vector2i Vector2i::operator*(int32_t p_rvalue) const {
	return Vector2i(x * p_rvalue, y * p_rvalue);
}

constexpr void Vector2i::operator*=(int32_t p_rvalue) {
	x *= p_rvalue;
	y *= p_rvalue;
}

constexpr Vector2i Vector2i::operator/(const Vector2i &p_v1) const {
	return Vector2i(x / p_v1.x, y / p_v1.y);
}

constexpr Vector2i Vector2i::operator/(int32_t p_rvalue) const {
	return Vector2i(x / p_rvalue, y / p_rvalue);
}

constexpr void Vector2i::operator/=(int32_t p_rvalue) {
	x /= p_rvalue;
	y /= p_rvalue;
}

constexpr Vector2i Vector2i::operator%(const Vector2i &p_v1) const {
	return Vector2i(x % p_v1.x, y % p_v1.y);
}

constexpr Vector2i Vector2i::operator%(int32_t p_rvalue) const {
	return Vector2i(x % p_rvalue, y % p_rvalue);
}

constexpr void Vector2i::operator%=(int32_t p_rvalue) {
	x %= p_rvalue;
	y %= p_rvalue;
}

constexpr Vector2i Vector2i::operator-() const {
	return Vector2i(-x, -y);
}

constexpr bool Vector2i::operator==(const Vector2i &p_vec2) const {
	return x == p_vec2.x && y == p_vec2.y;
}

constexpr bool Vector2i::operator!=(const Vector2i &p_vec2) const {
	return x != p_vec2.x || y != p_vec2.y;
}

// Multiplication operators required to workaround issues with LLVM using implicit conversion.

constexpr Vector2i operator*(int32_t p_scalar, const Vector2i &p_vector) {
	return p_vector * p_scalar;
}

constexpr Vector2i operator*(int64_t p_scalar, const Vector2i &p_vector) {
	return p_vector * p_scalar;
}

constexpr Vector2i operator*(float p_scalar, const Vector2i &p_vector) {
	return p_vector * p_scalar;
}

constexpr Vector2i operator*(double p_scalar, const Vector2i &p_vector) {
	return p_vector * p_scalar;
}

typedef Vector2i Size2i;
typedef Vector2i Point2i;

#endif // VECTOR2I_H
