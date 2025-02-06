/**************************************************************************/
/*  vector3.h                                                             */
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
#include "core/string/ustring.h"

struct Basis;
struct Vector2;
struct Vector3i;

struct [[nodiscard]] Vector3 {
	static const int AXIS_COUNT = 3;

	enum Axis {
		AXIS_X,
		AXIS_Y,
		AXIS_Z,
	};

	union {
		struct {
			real_t x;
			real_t y;
			real_t z;
		};

		real_t coord[3] = { 0 };
	};

	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 3);
		return coord[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 3);
		return coord[p_axis];
	}

	_FORCE_INLINE_ Vector3::Axis min_axis_index() const {
		return x < y ? (x < z ? Vector3::AXIS_X : Vector3::AXIS_Z) : (y < z ? Vector3::AXIS_Y : Vector3::AXIS_Z);
	}

	_FORCE_INLINE_ Vector3::Axis max_axis_index() const {
		return x < y ? (y < z ? Vector3::AXIS_Z : Vector3::AXIS_Y) : (x < z ? Vector3::AXIS_Z : Vector3::AXIS_X);
	}

	Vector3 min(const Vector3 &p_vector3) const {
		return Vector3(MIN(x, p_vector3.x), MIN(y, p_vector3.y), MIN(z, p_vector3.z));
	}

	Vector3 minf(real_t p_scalar) const {
		return Vector3(MIN(x, p_scalar), MIN(y, p_scalar), MIN(z, p_scalar));
	}

	Vector3 max(const Vector3 &p_vector3) const {
		return Vector3(MAX(x, p_vector3.x), MAX(y, p_vector3.y), MAX(z, p_vector3.z));
	}

	Vector3 maxf(real_t p_scalar) const {
		return Vector3(MAX(x, p_scalar), MAX(y, p_scalar), MAX(z, p_scalar));
	}

	_FORCE_INLINE_ real_t length() const;
	_FORCE_INLINE_ real_t length_squared() const;

	_FORCE_INLINE_ void normalize();
	_FORCE_INLINE_ Vector3 normalized() const;
	_FORCE_INLINE_ bool is_normalized() const;
	_FORCE_INLINE_ Vector3 inverse() const;
	Vector3 limit_length(real_t p_len = 1.0) const;

	_FORCE_INLINE_ void zero();

	void snap(const Vector3 &p_step);
	void snapf(real_t p_step);
	Vector3 snapped(const Vector3 &p_step) const;
	Vector3 snappedf(real_t p_step) const;

	void rotate(const Vector3 &p_axis, real_t p_angle);
	Vector3 rotated(const Vector3 &p_axis, real_t p_angle) const;

	/* Static Methods between 2 vector3s */

	_FORCE_INLINE_ Vector3 lerp(const Vector3 &p_to, real_t p_weight) const;
	_FORCE_INLINE_ Vector3 slerp(const Vector3 &p_to, real_t p_weight) const;
	_FORCE_INLINE_ Vector3 cubic_interpolate(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, real_t p_weight) const;
	_FORCE_INLINE_ Vector3 cubic_interpolate_in_time(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, real_t p_weight, real_t p_b_t, real_t p_pre_a_t, real_t p_post_b_t) const;
	_FORCE_INLINE_ Vector3 bezier_interpolate(const Vector3 &p_control_1, const Vector3 &p_control_2, const Vector3 &p_end, real_t p_t) const;
	_FORCE_INLINE_ Vector3 bezier_derivative(const Vector3 &p_control_1, const Vector3 &p_control_2, const Vector3 &p_end, real_t p_t) const;

	Vector3 move_toward(const Vector3 &p_to, real_t p_delta) const;

	Vector2 octahedron_encode() const;
	static Vector3 octahedron_decode(const Vector2 &p_oct);
	Vector2 octahedron_tangent_encode(float p_sign) const;
	static Vector3 octahedron_tangent_decode(const Vector2 &p_oct, float *r_sign);

	_FORCE_INLINE_ Vector3 cross(const Vector3 &p_with) const;
	_FORCE_INLINE_ real_t dot(const Vector3 &p_with) const;
	Basis outer(const Vector3 &p_with) const;
	_FORCE_INLINE_ Vector3 get_any_perpendicular() const;

	_FORCE_INLINE_ Vector3 abs() const;
	_FORCE_INLINE_ Vector3 floor() const;
	_FORCE_INLINE_ Vector3 sign() const;
	_FORCE_INLINE_ Vector3 ceil() const;
	_FORCE_INLINE_ Vector3 round() const;
	Vector3 clamp(const Vector3 &p_min, const Vector3 &p_max) const;
	Vector3 clampf(real_t p_min, real_t p_max) const;

	_FORCE_INLINE_ real_t distance_to(const Vector3 &p_to) const;
	_FORCE_INLINE_ real_t distance_squared_to(const Vector3 &p_to) const;

	_FORCE_INLINE_ Vector3 posmod(real_t p_mod) const;
	_FORCE_INLINE_ Vector3 posmodv(const Vector3 &p_modv) const;
	_FORCE_INLINE_ Vector3 project(const Vector3 &p_to) const;

	_FORCE_INLINE_ real_t angle_to(const Vector3 &p_to) const;
	_FORCE_INLINE_ real_t signed_angle_to(const Vector3 &p_to, const Vector3 &p_axis) const;
	_FORCE_INLINE_ Vector3 direction_to(const Vector3 &p_to) const;

	_FORCE_INLINE_ Vector3 slide(const Vector3 &p_normal) const;
	_FORCE_INLINE_ Vector3 bounce(const Vector3 &p_normal) const;
	_FORCE_INLINE_ Vector3 reflect(const Vector3 &p_normal) const;

	bool is_equal_approx(const Vector3 &p_v) const;
	bool is_zero_approx() const;
	bool is_finite() const;

	/* Operators */

	_FORCE_INLINE_ Vector3 &operator+=(const Vector3 &p_v);
	_FORCE_INLINE_ Vector3 operator+(const Vector3 &p_v) const;
	_FORCE_INLINE_ Vector3 &operator-=(const Vector3 &p_v);
	_FORCE_INLINE_ Vector3 operator-(const Vector3 &p_v) const;
	_FORCE_INLINE_ Vector3 &operator*=(const Vector3 &p_v);
	_FORCE_INLINE_ Vector3 operator*(const Vector3 &p_v) const;
	_FORCE_INLINE_ Vector3 &operator/=(const Vector3 &p_v);
	_FORCE_INLINE_ Vector3 operator/(const Vector3 &p_v) const;

	_FORCE_INLINE_ Vector3 &operator*=(real_t p_scalar);
	_FORCE_INLINE_ Vector3 operator*(real_t p_scalar) const;
	_FORCE_INLINE_ Vector3 &operator/=(real_t p_scalar);
	_FORCE_INLINE_ Vector3 operator/(real_t p_scalar) const;

	_FORCE_INLINE_ Vector3 operator-() const;

	_FORCE_INLINE_ bool operator==(const Vector3 &p_v) const;
	_FORCE_INLINE_ bool operator!=(const Vector3 &p_v) const;
	_FORCE_INLINE_ bool operator<(const Vector3 &p_v) const;
	_FORCE_INLINE_ bool operator<=(const Vector3 &p_v) const;
	_FORCE_INLINE_ bool operator>(const Vector3 &p_v) const;
	_FORCE_INLINE_ bool operator>=(const Vector3 &p_v) const;

	operator String() const;
	operator Vector3i() const;

	_FORCE_INLINE_ Vector3() {}
	_FORCE_INLINE_ Vector3(real_t p_x, real_t p_y, real_t p_z) {
		x = p_x;
		y = p_y;
		z = p_z;
	}
};

Vector3 Vector3::cross(const Vector3 &p_with) const {
	Vector3 ret(
			(y * p_with.z) - (z * p_with.y),
			(z * p_with.x) - (x * p_with.z),
			(x * p_with.y) - (y * p_with.x));

	return ret;
}

real_t Vector3::dot(const Vector3 &p_with) const {
	return x * p_with.x + y * p_with.y + z * p_with.z;
}

Vector3 Vector3::abs() const {
	return Vector3(Math::abs(x), Math::abs(y), Math::abs(z));
}

Vector3 Vector3::sign() const {
	return Vector3(SIGN(x), SIGN(y), SIGN(z));
}

Vector3 Vector3::floor() const {
	return Vector3(Math::floor(x), Math::floor(y), Math::floor(z));
}

Vector3 Vector3::ceil() const {
	return Vector3(Math::ceil(x), Math::ceil(y), Math::ceil(z));
}

Vector3 Vector3::round() const {
	return Vector3(Math::round(x), Math::round(y), Math::round(z));
}

Vector3 Vector3::lerp(const Vector3 &p_to, real_t p_weight) const {
	Vector3 res = *this;
	res.x = Math::lerp(res.x, p_to.x, p_weight);
	res.y = Math::lerp(res.y, p_to.y, p_weight);
	res.z = Math::lerp(res.z, p_to.z, p_weight);
	return res;
}

Vector3 Vector3::slerp(const Vector3 &p_to, real_t p_weight) const {
	// This method seems more complicated than it really is, since we write out
	// the internals of some methods for efficiency (mainly, checking length).
	real_t start_length_sq = length_squared();
	real_t end_length_sq = p_to.length_squared();
	if (unlikely(start_length_sq == 0.0f || end_length_sq == 0.0f)) {
		// Zero length vectors have no angle, so the best we can do is either lerp or throw an error.
		return lerp(p_to, p_weight);
	}
	Vector3 axis = cross(p_to);
	real_t axis_length_sq = axis.length_squared();
	if (unlikely(axis_length_sq == 0.0f)) {
		// Colinear vectors have no rotation axis or angle between them, so the best we can do is lerp.
		return lerp(p_to, p_weight);
	}
	axis /= Math::sqrt(axis_length_sq);
	real_t start_length = Math::sqrt(start_length_sq);
	real_t result_length = Math::lerp(start_length, Math::sqrt(end_length_sq), p_weight);
	real_t angle = angle_to(p_to);
	return rotated(axis, angle * p_weight) * (result_length / start_length);
}

Vector3 Vector3::cubic_interpolate(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, real_t p_weight) const {
	Vector3 res = *this;
	res.x = Math::cubic_interpolate(res.x, p_b.x, p_pre_a.x, p_post_b.x, p_weight);
	res.y = Math::cubic_interpolate(res.y, p_b.y, p_pre_a.y, p_post_b.y, p_weight);
	res.z = Math::cubic_interpolate(res.z, p_b.z, p_pre_a.z, p_post_b.z, p_weight);
	return res;
}

Vector3 Vector3::cubic_interpolate_in_time(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, real_t p_weight, real_t p_b_t, real_t p_pre_a_t, real_t p_post_b_t) const {
	Vector3 res = *this;
	res.x = Math::cubic_interpolate_in_time(res.x, p_b.x, p_pre_a.x, p_post_b.x, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
	res.y = Math::cubic_interpolate_in_time(res.y, p_b.y, p_pre_a.y, p_post_b.y, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
	res.z = Math::cubic_interpolate_in_time(res.z, p_b.z, p_pre_a.z, p_post_b.z, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
	return res;
}

Vector3 Vector3::bezier_interpolate(const Vector3 &p_control_1, const Vector3 &p_control_2, const Vector3 &p_end, real_t p_t) const {
	Vector3 res = *this;
	res.x = Math::bezier_interpolate(res.x, p_control_1.x, p_control_2.x, p_end.x, p_t);
	res.y = Math::bezier_interpolate(res.y, p_control_1.y, p_control_2.y, p_end.y, p_t);
	res.z = Math::bezier_interpolate(res.z, p_control_1.z, p_control_2.z, p_end.z, p_t);
	return res;
}

Vector3 Vector3::bezier_derivative(const Vector3 &p_control_1, const Vector3 &p_control_2, const Vector3 &p_end, real_t p_t) const {
	Vector3 res = *this;
	res.x = Math::bezier_derivative(res.x, p_control_1.x, p_control_2.x, p_end.x, p_t);
	res.y = Math::bezier_derivative(res.y, p_control_1.y, p_control_2.y, p_end.y, p_t);
	res.z = Math::bezier_derivative(res.z, p_control_1.z, p_control_2.z, p_end.z, p_t);
	return res;
}

real_t Vector3::distance_to(const Vector3 &p_to) const {
	return (p_to - *this).length();
}

real_t Vector3::distance_squared_to(const Vector3 &p_to) const {
	return (p_to - *this).length_squared();
}

Vector3 Vector3::posmod(real_t p_mod) const {
	return Vector3(Math::fposmod(x, p_mod), Math::fposmod(y, p_mod), Math::fposmod(z, p_mod));
}

Vector3 Vector3::posmodv(const Vector3 &p_modv) const {
	return Vector3(Math::fposmod(x, p_modv.x), Math::fposmod(y, p_modv.y), Math::fposmod(z, p_modv.z));
}

Vector3 Vector3::project(const Vector3 &p_to) const {
	return p_to * (dot(p_to) / p_to.length_squared());
}

real_t Vector3::angle_to(const Vector3 &p_to) const {
	return Math::atan2(cross(p_to).length(), dot(p_to));
}

real_t Vector3::signed_angle_to(const Vector3 &p_to, const Vector3 &p_axis) const {
	Vector3 cross_to = cross(p_to);
	real_t unsigned_angle = Math::atan2(cross_to.length(), dot(p_to));
	real_t sign = cross_to.dot(p_axis);
	return (sign < 0) ? -unsigned_angle : unsigned_angle;
}

Vector3 Vector3::direction_to(const Vector3 &p_to) const {
	Vector3 ret(p_to.x - x, p_to.y - y, p_to.z - z);
	ret.normalize();
	return ret;
}

Vector3 Vector3::get_any_perpendicular() const {
	// Return the any perpendicular vector by cross product with the Vector3.RIGHT or Vector3.UP,
	// whichever has the greater angle to the current vector with the sign of each element positive.
	// The only essence is "to avoid being parallel to the current vector", and there is no mathematical basis for using Vector3.RIGHT and Vector3.UP,
	// since it could be a different vector depending on the prior branching code Math::abs(x) <= Math::abs(y) && Math::abs(x) <= Math::abs(z).
	// However, it would be reasonable to use any of the axes of the basis, as it is simpler to calculate.
	ERR_FAIL_COND_V_MSG(is_zero_approx(), Vector3(0, 0, 0), "The Vector3 must not be zero.");
	return cross((Math::abs(x) <= Math::abs(y) && Math::abs(x) <= Math::abs(z)) ? Vector3(1, 0, 0) : Vector3(0, 1, 0)).normalized();
}

/* Operators */

Vector3 &Vector3::operator+=(const Vector3 &p_v) {
	x += p_v.x;
	y += p_v.y;
	z += p_v.z;
	return *this;
}

Vector3 Vector3::operator+(const Vector3 &p_v) const {
	return Vector3(x + p_v.x, y + p_v.y, z + p_v.z);
}

Vector3 &Vector3::operator-=(const Vector3 &p_v) {
	x -= p_v.x;
	y -= p_v.y;
	z -= p_v.z;
	return *this;
}

Vector3 Vector3::operator-(const Vector3 &p_v) const {
	return Vector3(x - p_v.x, y - p_v.y, z - p_v.z);
}

Vector3 &Vector3::operator*=(const Vector3 &p_v) {
	x *= p_v.x;
	y *= p_v.y;
	z *= p_v.z;
	return *this;
}

Vector3 Vector3::operator*(const Vector3 &p_v) const {
	return Vector3(x * p_v.x, y * p_v.y, z * p_v.z);
}

Vector3 &Vector3::operator/=(const Vector3 &p_v) {
	x /= p_v.x;
	y /= p_v.y;
	z /= p_v.z;
	return *this;
}

Vector3 Vector3::operator/(const Vector3 &p_v) const {
	return Vector3(x / p_v.x, y / p_v.y, z / p_v.z);
}

Vector3 &Vector3::operator*=(real_t p_scalar) {
	x *= p_scalar;
	y *= p_scalar;
	z *= p_scalar;
	return *this;
}

// Multiplication operators required to workaround issues with LLVM using implicit conversion
// to Vector3i instead for integers where it should not.

_FORCE_INLINE_ Vector3 operator*(float p_scalar, const Vector3 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector3 operator*(double p_scalar, const Vector3 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector3 operator*(int32_t p_scalar, const Vector3 &p_vec) {
	return p_vec * p_scalar;
}

_FORCE_INLINE_ Vector3 operator*(int64_t p_scalar, const Vector3 &p_vec) {
	return p_vec * p_scalar;
}

Vector3 Vector3::operator*(real_t p_scalar) const {
	return Vector3(x * p_scalar, y * p_scalar, z * p_scalar);
}

Vector3 &Vector3::operator/=(real_t p_scalar) {
	x /= p_scalar;
	y /= p_scalar;
	z /= p_scalar;
	return *this;
}

Vector3 Vector3::operator/(real_t p_scalar) const {
	return Vector3(x / p_scalar, y / p_scalar, z / p_scalar);
}

Vector3 Vector3::operator-() const {
	return Vector3(-x, -y, -z);
}

bool Vector3::operator==(const Vector3 &p_v) const {
	return x == p_v.x && y == p_v.y && z == p_v.z;
}

bool Vector3::operator!=(const Vector3 &p_v) const {
	return x != p_v.x || y != p_v.y || z != p_v.z;
}

bool Vector3::operator<(const Vector3 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			return z < p_v.z;
		}
		return y < p_v.y;
	}
	return x < p_v.x;
}

bool Vector3::operator>(const Vector3 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			return z > p_v.z;
		}
		return y > p_v.y;
	}
	return x > p_v.x;
}

bool Vector3::operator<=(const Vector3 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			return z <= p_v.z;
		}
		return y < p_v.y;
	}
	return x < p_v.x;
}

bool Vector3::operator>=(const Vector3 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			return z >= p_v.z;
		}
		return y > p_v.y;
	}
	return x > p_v.x;
}

_FORCE_INLINE_ Vector3 vec3_cross(const Vector3 &p_a, const Vector3 &p_b) {
	return p_a.cross(p_b);
}

_FORCE_INLINE_ real_t vec3_dot(const Vector3 &p_a, const Vector3 &p_b) {
	return p_a.dot(p_b);
}

real_t Vector3::length() const {
	real_t x2 = x * x;
	real_t y2 = y * y;
	real_t z2 = z * z;

	return Math::sqrt(x2 + y2 + z2);
}

real_t Vector3::length_squared() const {
	real_t x2 = x * x;
	real_t y2 = y * y;
	real_t z2 = z * z;

	return x2 + y2 + z2;
}

void Vector3::normalize() {
	real_t lengthsq = length_squared();
	if (lengthsq == 0) {
		x = y = z = 0;
	} else {
		real_t length = Math::sqrt(lengthsq);
		x /= length;
		y /= length;
		z /= length;
	}
}

Vector3 Vector3::normalized() const {
	Vector3 v = *this;
	v.normalize();
	return v;
}

bool Vector3::is_normalized() const {
	// use length_squared() instead of length() to avoid sqrt(), makes it more stringent.
	return Math::is_equal_approx(length_squared(), 1, (real_t)UNIT_EPSILON);
}

Vector3 Vector3::inverse() const {
	return Vector3(1.0f / x, 1.0f / y, 1.0f / z);
}

void Vector3::zero() {
	x = y = z = 0;
}

// slide returns the component of the vector along the given plane, specified by its normal vector.
Vector3 Vector3::slide(const Vector3 &p_normal) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector3(), "The normal Vector3 " + p_normal.operator String() + " must be normalized.");
#endif
	return *this - p_normal * dot(p_normal);
}

Vector3 Vector3::bounce(const Vector3 &p_normal) const {
	return -reflect(p_normal);
}

Vector3 Vector3::reflect(const Vector3 &p_normal) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector3(), "The normal Vector3 " + p_normal.operator String() + " must be normalized.");
#endif
	return 2.0f * p_normal * dot(p_normal) - *this;
}
