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

#pragma once

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"

class String;
struct Vector2i;

struct [[nodiscard]] Vector2 {
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

		real_t coord[2] = { 0 };
	};

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 2);
		return coord[p_axis];
	}
	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 2);
		return coord[p_axis];
	}

	_FORCE_INLINE_ Vector2::Axis min_axis_index() const {
		return x < y ? Vector2::AXIS_X : Vector2::AXIS_Y;
	}

	_FORCE_INLINE_ Vector2::Axis max_axis_index() const {
		return x < y ? Vector2::AXIS_Y : Vector2::AXIS_X;
	}

	void normalize();
	Vector2 normalized() const;
	bool is_normalized() const;

	real_t length() const;
	real_t length_squared() const;
	Vector2 limit_length(real_t p_len = 1.0) const;

	Vector2 min(const Vector2 &p_vector2) const {
		return Vector2(MIN(x, p_vector2.x), MIN(y, p_vector2.y));
	}

	Vector2 minf(real_t p_scalar) const {
		return Vector2(MIN(x, p_scalar), MIN(y, p_scalar));
	}

	Vector2 max(const Vector2 &p_vector2) const {
		return Vector2(MAX(x, p_vector2.x), MAX(y, p_vector2.y));
	}

	Vector2 maxf(real_t p_scalar) const {
		return Vector2(MAX(x, p_scalar), MAX(y, p_scalar));
	}

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

	_FORCE_INLINE_ Vector2 lerp(const Vector2 &p_to, real_t p_weight) const;
	_FORCE_INLINE_ Vector2 slerp(const Vector2 &p_to, real_t p_weight) const;
	_FORCE_INLINE_ Vector2 cubic_interpolate(const Vector2 &p_b, const Vector2 &p_pre_a, const Vector2 &p_post_b, real_t p_weight) const;
	_FORCE_INLINE_ Vector2 cubic_interpolate_in_time(const Vector2 &p_b, const Vector2 &p_pre_a, const Vector2 &p_post_b, real_t p_weight, real_t p_b_t, real_t p_pre_a_t, real_t p_post_b_t) const;
	_FORCE_INLINE_ Vector2 bezier_interpolate(const Vector2 &p_control_1, const Vector2 &p_control_2, const Vector2 &p_end, real_t p_t) const;
	_FORCE_INLINE_ Vector2 bezier_derivative(const Vector2 &p_control_1, const Vector2 &p_control_2, const Vector2 &p_end, real_t p_t) const;

	Vector2 move_toward(const Vector2 &p_to, real_t p_delta) const;

	Vector2 slide(const Vector2 &p_normal) const;
	Vector2 bounce(const Vector2 &p_normal) const;
	Vector2 reflect(const Vector2 &p_normal) const;

	bool is_equal_approx(const Vector2 &p_v) const;
	bool is_zero_approx() const;
	bool is_finite() const;

	Vector2 operator+(const Vector2 &p_v) const;
	void operator+=(const Vector2 &p_v);
	Vector2 operator-(const Vector2 &p_v) const;
	void operator-=(const Vector2 &p_v);
	Vector2 operator*(const Vector2 &p_v1) const;

	Vector2 operator*(real_t p_rvalue) const;
	void operator*=(real_t p_rvalue);
	void operator*=(const Vector2 &p_rvalue) { *this = *this * p_rvalue; }

	Vector2 operator/(const Vector2 &p_v1) const;

	Vector2 operator/(real_t p_rvalue) const;

	void operator/=(real_t p_rvalue);
	void operator/=(const Vector2 &p_rvalue) { *this = *this / p_rvalue; }

	Vector2 operator-() const;

	bool operator==(const Vector2 &p_vec2) const;
	bool operator!=(const Vector2 &p_vec2) const;

	bool operator<(const Vector2 &p_vec2) const { return x == p_vec2.x ? (y < p_vec2.y) : (x < p_vec2.x); }
	bool operator>(const Vector2 &p_vec2) const { return x == p_vec2.x ? (y > p_vec2.y) : (x > p_vec2.x); }
	bool operator<=(const Vector2 &p_vec2) const { return x == p_vec2.x ? (y <= p_vec2.y) : (x < p_vec2.x); }
	bool operator>=(const Vector2 &p_vec2) const { return x == p_vec2.x ? (y >= p_vec2.y) : (x > p_vec2.x); }

	real_t angle() const;
	static Vector2 from_angle(real_t p_angle);

	_FORCE_INLINE_ Vector2 abs() const {
		return Vector2(Math::abs(x), Math::abs(y));
	}

	Vector2 rotated(real_t p_by) const;
	Vector2 orthogonal() const {
		return Vector2(y, -x);
	}

	Vector2 sign() const;
	Vector2 floor() const;
	Vector2 ceil() const;
	Vector2 round() const;
	Vector2 snapped(const Vector2 &p_by) const;
	Vector2 snappedf(real_t p_by) const;
	Vector2 clamp(const Vector2 &p_min, const Vector2 &p_max) const;
	Vector2 clampf(real_t p_min, real_t p_max) const;
	real_t aspect() const { return width / height; }

	operator String() const;
	operator Vector2i() const;

	_FORCE_INLINE_ Vector2() {}
	_FORCE_INLINE_ Vector2(real_t p_x, real_t p_y) {
		x = p_x;
		y = p_y;
	}
};

_FORCE_INLINE_ Vector2 Vector2::plane_project(real_t p_d, const Vector2 &p_vec) const {
	return p_vec - *this * (dot(p_vec) - p_d);
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

_FORCE_INLINE_ Vector2 Vector2::operator*(const Vector2 &p_v1) const {
	return Vector2(x * p_v1.x, y * p_v1.y);
}

_FORCE_INLINE_ Vector2 Vector2::operator*(real_t p_rvalue) const {
	return Vector2(x * p_rvalue, y * p_rvalue);
}

_FORCE_INLINE_ void Vector2::operator*=(real_t p_rvalue) {
	x *= p_rvalue;
	y *= p_rvalue;
}

_FORCE_INLINE_ Vector2 Vector2::operator/(const Vector2 &p_v1) const {
	return Vector2(x / p_v1.x, y / p_v1.y);
}

_FORCE_INLINE_ Vector2 Vector2::operator/(real_t p_rvalue) const {
	return Vector2(x / p_rvalue, y / p_rvalue);
}

_FORCE_INLINE_ void Vector2::operator/=(real_t p_rvalue) {
	x /= p_rvalue;
	y /= p_rvalue;
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

Vector2 Vector2::lerp(const Vector2 &p_to, real_t p_weight) const {
	Vector2 res = *this;
	res.x = Math::lerp(res.x, p_to.x, p_weight);
	res.y = Math::lerp(res.y, p_to.y, p_weight);
	return res;
}

Vector2 Vector2::slerp(const Vector2 &p_to, real_t p_weight) const {
	real_t start_length_sq = length_squared();
	real_t end_length_sq = p_to.length_squared();
	if (unlikely(start_length_sq == 0.0f || end_length_sq == 0.0f)) {
		// Zero length vectors have no angle, so the best we can do is either lerp or throw an error.
		return lerp(p_to, p_weight);
	}
	real_t start_length = Math::sqrt(start_length_sq);
	real_t result_length = Math::lerp(start_length, Math::sqrt(end_length_sq), p_weight);
	real_t angle = angle_to(p_to);
	return rotated(angle * p_weight) * (result_length / start_length);
}

Vector2 Vector2::cubic_interpolate(const Vector2 &p_b, const Vector2 &p_pre_a, const Vector2 &p_post_b, real_t p_weight) const {
	Vector2 res = *this;
	res.x = Math::cubic_interpolate(res.x, p_b.x, p_pre_a.x, p_post_b.x, p_weight);
	res.y = Math::cubic_interpolate(res.y, p_b.y, p_pre_a.y, p_post_b.y, p_weight);
	return res;
}

Vector2 Vector2::cubic_interpolate_in_time(const Vector2 &p_b, const Vector2 &p_pre_a, const Vector2 &p_post_b, real_t p_weight, real_t p_b_t, real_t p_pre_a_t, real_t p_post_b_t) const {
	Vector2 res = *this;
	res.x = Math::cubic_interpolate_in_time(res.x, p_b.x, p_pre_a.x, p_post_b.x, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
	res.y = Math::cubic_interpolate_in_time(res.y, p_b.y, p_pre_a.y, p_post_b.y, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
	return res;
}

Vector2 Vector2::bezier_interpolate(const Vector2 &p_control_1, const Vector2 &p_control_2, const Vector2 &p_end, real_t p_t) const {
	Vector2 res = *this;
	res.x = Math::bezier_interpolate(res.x, p_control_1.x, p_control_2.x, p_end.x, p_t);
	res.y = Math::bezier_interpolate(res.y, p_control_1.y, p_control_2.y, p_end.y, p_t);
	return res;
}

Vector2 Vector2::bezier_derivative(const Vector2 &p_control_1, const Vector2 &p_control_2, const Vector2 &p_end, real_t p_t) const {
	Vector2 res = *this;
	res.x = Math::bezier_derivative(res.x, p_control_1.x, p_control_2.x, p_end.x, p_t);
	res.y = Math::bezier_derivative(res.y, p_control_1.y, p_control_2.y, p_end.y, p_t);
	return res;
}

Vector2 Vector2::direction_to(const Vector2 &p_to) const {
	Vector2 ret(p_to.x - x, p_to.y - y);
	ret.normalize();
	return ret;
}

// Multiplication operators required to workaround issues with LLVM using implicit conversion
// to Vector2i instead for integers where it should not.

_FORCE_INLINE_ Vector2 operator*(float p_scalar, const Vector2 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector2 operator*(double p_scalar, const Vector2 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector2 operator*(int32_t p_scalar, const Vector2 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector2 operator*(int64_t p_scalar, const Vector2 &p_vec) {
	return p_vec * p_scalar;
}

typedef Vector2 Size2;
typedef Vector2 Point2;
