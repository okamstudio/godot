/**************************************************************************/
/*  vector2.h                                                             */
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

#ifndef VECTOR2_H
#define VECTOR2_H

#include "core/math/math_funcs.h"
#include "core/ustring.h"

struct Vector2i;

struct _NO_DISCARD_CLASS_ Vector2 {
	static const int AXIS_COUNT = 2;

	enum Axis {
		AXIS_X,
		AXIS_Y,
	};

	union {
		struct {
			union {
				real_t x;
				real_t width;
			};
			union {
				real_t y;
				real_t height;
			};
		};

		real_t coord[2];
	};

	_FORCE_INLINE_ real_t &operator[](int p_idx) {
		DEV_ASSERT((unsigned int)p_idx < 2);
		return coord[p_idx];
	}
	_FORCE_INLINE_ const real_t &operator[](int p_idx) const {
		DEV_ASSERT((unsigned int)p_idx < 2);
		return coord[p_idx];
	}

	_FORCE_INLINE_ void set_all(real_t p_value) {
		x = y = p_value;
	}

	_FORCE_INLINE_ int min_axis() const {
		return x < y ? 0 : 1;
	}

	_FORCE_INLINE_ int max_axis() const {
		return x < y ? 1 : 0;
	}

	bool normalize() { return normalize_and_get_length(); }
	inline bool normalize_and_get_length(real_t *r_length = nullptr);
	Vector2 normalized() const;
	bool is_normalized() const;

	real_t length() const;
	real_t length_squared() const;

	real_t distance_to(const Vector2 &p_vector2) const;
	real_t distance_squared_to(const Vector2 &p_vector2) const;
	real_t angle_to(const Vector2 &p_vector2) const;
	real_t angle_to_point(const Vector2 &p_vector2) const;
	_FORCE_INLINE_ Vector2 direction_to(const Vector2 &p_to) const;

	real_t dot(const Vector2 &p_other) const;
	real_t cross(const Vector2 &p_other) const;
	Vector2 posmod(real_t p_mod) const;
	Vector2 posmodv(const Vector2 &p_modv) const;
	Vector2 project(const Vector2 &p_to) const;

	Vector2 plane_project(real_t p_d, const Vector2 &p_vec) const;

	Vector2 clamped(real_t p_len) const;
	Vector2 limit_length(real_t p_len = 1.0) const;

	_FORCE_INLINE_ static Vector2 linear_interpolate(const Vector2 &p_a, const Vector2 &p_b, real_t p_weight);
	_FORCE_INLINE_ Vector2 linear_interpolate(const Vector2 &p_to, real_t p_weight) const;
	_FORCE_INLINE_ Vector2 slerp(const Vector2 &p_to, real_t p_weight) const;
	Vector2 cubic_interpolate(const Vector2 &p_b, const Vector2 &p_pre_a, const Vector2 &p_post_b, real_t p_weight) const;
	Vector2 move_toward(const Vector2 &p_to, real_t p_delta) const;

	Vector2 slide(const Vector2 &p_normal) const;
	Vector2 bounce(const Vector2 &p_normal) const;
	Vector2 reflect(const Vector2 &p_normal) const;

	bool is_equal_approx(const Vector2 &p_v) const;
	bool is_zero_approx() const;

	Vector2 operator+(const Vector2 &p_v) const;
	void operator+=(const Vector2 &p_v);
	Vector2 operator-(const Vector2 &p_v) const;
	void operator-=(const Vector2 &p_v);
	Vector2 operator*(const Vector2 &p_v) const;

	Vector2 operator*(real_t p_scalar) const;
	void operator*=(real_t p_scalar);
	void operator*=(const Vector2 &p_v) { *this = *this * p_v; }

	Vector2 operator/(const Vector2 &p_v) const;

	Vector2 operator/(real_t p_scalar) const;

	void operator/=(real_t p_scalar);
	void operator/=(const Vector2 &p_v) { *this = *this / p_v; }

	Vector2 operator-() const;

	bool operator==(const Vector2 &p_vec2) const;
	bool operator!=(const Vector2 &p_vec2) const;

	bool operator<(const Vector2 &p_vec2) const { return x == p_vec2.x ? (y < p_vec2.y) : (x < p_vec2.x); }
	bool operator>(const Vector2 &p_vec2) const { return x == p_vec2.x ? (y > p_vec2.y) : (x > p_vec2.x); }
	bool operator<=(const Vector2 &p_vec2) const { return x == p_vec2.x ? (y <= p_vec2.y) : (x < p_vec2.x); }
	bool operator>=(const Vector2 &p_vec2) const { return x == p_vec2.x ? (y >= p_vec2.y) : (x > p_vec2.x); }

	real_t angle() const;

	void set_rotation(real_t p_radians) {
		x = Math::cos(p_radians);
		y = Math::sin(p_radians);
	}

	_FORCE_INLINE_ Vector2 abs() const {
		return Vector2(Math::abs(x), Math::abs(y));
	}

	Vector2 rotated(real_t p_by) const;
	Vector2 tangent() const {
		return Vector2(y, -x);
	}

	Vector2 sign() const;
	Vector2 floor() const;
	Vector2 ceil() const;
	Vector2 round() const;
	Vector2 snapped(const Vector2 &p_by) const;
	real_t aspect() const { return width / height; }

	operator String() const { return String::num(x) + ", " + String::num(y); }

	_FORCE_INLINE_ Vector2(real_t p_x, real_t p_y) {
		x = p_x;
		y = p_y;
	}
	_FORCE_INLINE_ Vector2() { x = y = 0; }
};

_FORCE_INLINE_ Vector2 Vector2::plane_project(real_t p_d, const Vector2 &p_vec) const {
	return p_vec - *this * (dot(p_vec) - p_d);
}

_FORCE_INLINE_ Vector2 operator*(real_t p_scalar, const Vector2 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector2 Vector2::operator+(const Vector2 &p_v) const {
	return Vector2(x + p_v.x, y + p_v.y);
}
_FORCE_INLINE_ void Vector2::operator+=(const Vector2 &p_v) {
	x += p_v.x;
	y += p_v.y;
}
_FORCE_INLINE_ Vector2 Vector2::operator-(const Vector2 &p_v) const {
	return Vector2(x - p_v.x, y - p_v.y);
}
_FORCE_INLINE_ void Vector2::operator-=(const Vector2 &p_v) {
	x -= p_v.x;
	y -= p_v.y;
}

_FORCE_INLINE_ Vector2 Vector2::operator*(const Vector2 &p_v) const {
	return Vector2(x * p_v.x, y * p_v.y);
}

_FORCE_INLINE_ Vector2 Vector2::operator*(real_t p_scalar) const {
	return Vector2(x * p_scalar, y * p_scalar);
}
_FORCE_INLINE_ void Vector2::operator*=(real_t p_scalar) {
	x *= p_scalar;
	y *= p_scalar;
}

_FORCE_INLINE_ Vector2 Vector2::operator/(const Vector2 &p_v) const {
	return Vector2(x / p_v.x, y / p_v.y);
}

_FORCE_INLINE_ Vector2 Vector2::operator/(real_t p_scalar) const {
	return Vector2(x / p_scalar, y / p_scalar);
}

_FORCE_INLINE_ void Vector2::operator/=(real_t p_scalar) {
	x /= p_scalar;
	y /= p_scalar;
}

_FORCE_INLINE_ Vector2 Vector2::operator-() const {
	return Vector2(-x, -y);
}

_FORCE_INLINE_ bool Vector2::operator==(const Vector2 &p_vec2) const {
	return x == p_vec2.x && y == p_vec2.y;
}
_FORCE_INLINE_ bool Vector2::operator!=(const Vector2 &p_vec2) const {
	return x != p_vec2.x || y != p_vec2.y;
}

Vector2 Vector2::linear_interpolate(const Vector2 &p_to, real_t p_weight) const {
	Vector2 res = *this;

	res.x += (p_weight * (p_to.x - x));
	res.y += (p_weight * (p_to.y - y));

	return res;
}

bool Vector2::normalize_and_get_length(real_t *r_length) {
	real_t lengthsq = length_squared();

	// Long enough to guarantee unit length result.
	if (lengthsq >= (real_t)CMP_EPSILON2) {
		real_t length = Math::sqrt(lengthsq);
		*this /= length;
		if (r_length) {
			*r_length = length;
		}
		return true;
	}
	// Long enough to guarantee direction, but not unit length result.
	constexpr real_t epsilon = NORMALIZE_DIRECTION_EPSILON;
	if (lengthsq >= epsilon) {
		// Boost length prior to normalize
		// to guarantee unit length.
		constexpr real_t mult = (1.0 / NORMALIZE_DIRECTION_EPSILON_SQRT);
		*this *= mult;

		lengthsq = length_squared();

		real_t length = Math::sqrt(lengthsq);
		*this /= length;

		// The length has been boosted so we need to un-boost to get the original length.
		if (r_length) {
			*r_length = length * NORMALIZE_DIRECTION_EPSILON_SQRT;
		}
		return true;
	}
	// Not long enough to guarantee direction,
	// failed.

	return false;
}

Vector2 Vector2::slerp(const Vector2 &p_to, real_t p_weight) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!is_normalized(), Vector2(), "The start Vector2 must be normalized.");
#endif
	real_t theta = angle_to(p_to);
	return rotated(theta * p_weight);
}

Vector2 Vector2::direction_to(const Vector2 &p_to) const {
	Vector2 ret(p_to.x - x, p_to.y - y);
	ret.normalize();
	return ret;
}

Vector2 Vector2::linear_interpolate(const Vector2 &p_a, const Vector2 &p_b, real_t p_weight) {
	Vector2 res = p_a;

	res.x += (p_weight * (p_b.x - p_a.x));
	res.y += (p_weight * (p_b.y - p_a.y));

	return res;
}

typedef Vector2 Size2;
typedef Vector2 Point2;

/* INTEGER STUFF */

struct _NO_DISCARD_CLASS_ Vector2i {
	enum Axis {
		AXIS_X,
		AXIS_Y,
	};

	union {
		struct {
			union {
				int x;
				int width;
			};
			union {
				int y;
				int height;
			};
		};

		int coord[2];
	};

	_FORCE_INLINE_ int &operator[](int p_idx) {
		DEV_ASSERT((unsigned int)p_idx < 2);
		return coord[p_idx];
	}
	_FORCE_INLINE_ const int &operator[](int p_idx) const {
		DEV_ASSERT((unsigned int)p_idx < 2);
		return coord[p_idx];
	}

	Vector2i operator+(const Vector2i &p_v) const;
	void operator+=(const Vector2i &p_v);
	Vector2i operator-(const Vector2i &p_v) const;
	void operator-=(const Vector2i &p_v);
	Vector2i operator*(const Vector2i &p_v) const;

	Vector2i operator*(int p_scalar) const;
	void operator*=(int p_scalar);

	Vector2i operator/(const Vector2i &p_v) const;

	Vector2i operator/(int p_scalar) const;

	void operator/=(int p_scalar);

	Vector2i operator-() const;
	bool operator<(const Vector2i &p_vec2) const { return (x == p_vec2.x) ? (y < p_vec2.y) : (x < p_vec2.x); }
	bool operator>(const Vector2i &p_vec2) const { return (x == p_vec2.x) ? (y > p_vec2.y) : (x > p_vec2.x); }

	bool operator==(const Vector2i &p_vec2) const;
	bool operator!=(const Vector2i &p_vec2) const;

	real_t get_aspect() const { return width / (real_t)height; }

	operator String() const { return String::num(x) + ", " + String::num(y); }

	operator Vector2() const { return Vector2(x, y); }
	inline Vector2i(const Vector2 &p_vec2) {
		x = (int)p_vec2.x;
		y = (int)p_vec2.y;
	}
	inline Vector2i(int p_x, int p_y) {
		x = p_x;
		y = p_y;
	}
	inline Vector2i() {
		x = 0;
		y = 0;
	}
};

typedef Vector2i Size2i;
typedef Vector2i Point2i;

#endif // VECTOR2_H
