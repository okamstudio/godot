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

#ifndef VECTOR3_H
#define VECTOR3_H

#include "core/math/math_funcs.h"
#include "core/ustring.h"

class Basis;

struct _NO_DISCARD_CLASS_ Vector3 {
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

		real_t coord[3];
	};

	_FORCE_INLINE_ const real_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 3);
		return coord[p_axis];
	}

	_FORCE_INLINE_ real_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 3);
		return coord[p_axis];
	}

	void set_axis(int p_axis, real_t p_value);
	real_t get_axis(int p_axis) const;

	_FORCE_INLINE_ void set_all(real_t p_value) {
		x = y = z = p_value;
	}

	_FORCE_INLINE_ int min_axis() const {
		return x < y ? (x < z ? 0 : 2) : (y < z ? 1 : 2);
	}

	_FORCE_INLINE_ int max_axis() const {
		return x < y ? (y < z ? 2 : 1) : (x < z ? 2 : 0);
	}

	_FORCE_INLINE_ real_t length() const;
	_FORCE_INLINE_ real_t length_squared() const;

	bool normalize() { return normalize_and_get_length(); }
	inline bool normalize_and_get_length(real_t *r_length = nullptr);
	_FORCE_INLINE_ Vector3 normalized() const;
	_FORCE_INLINE_ bool is_normalized() const;
	_FORCE_INLINE_ Vector3 inverse() const;
	Vector3 limit_length(real_t p_len = 1.0) const;

	_FORCE_INLINE_ void zero();

	void snap(const Vector3 &p_val);
	Vector3 snapped(const Vector3 &p_val) const;

	void rotate(const Vector3 &p_axis, real_t p_angle);
	Vector3 rotated(const Vector3 &p_axis, real_t p_angle) const;

	/* Static Methods between 2 vector3s */

	_FORCE_INLINE_ Vector3 linear_interpolate(const Vector3 &p_to, real_t p_weight) const;
	_FORCE_INLINE_ Vector3 slerp(const Vector3 &p_to, real_t p_weight) const;
	Vector3 cubic_interpolate(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, real_t p_weight) const;
	Vector3 cubic_interpolaten(const Vector3 &p_b, const Vector3 &p_pre_a, const Vector3 &p_post_b, real_t p_weight) const;
	Vector3 move_toward(const Vector3 &p_to, real_t p_delta) const;

	_FORCE_INLINE_ Vector3 cross(const Vector3 &p_b) const;
	_FORCE_INLINE_ real_t dot(const Vector3 &p_b) const;
	Basis outer(const Vector3 &p_b) const;
	Basis to_diagonal_matrix() const;

	_FORCE_INLINE_ Vector3 abs() const;
	_FORCE_INLINE_ Vector3 floor() const;
	_FORCE_INLINE_ Vector3 sign() const;
	_FORCE_INLINE_ Vector3 ceil() const;
	_FORCE_INLINE_ Vector3 round() const;

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
	inline bool is_equal_approx(const Vector3 &p_v, real_t p_tolerance) const;
	bool is_zero_approx() const;

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

	_FORCE_INLINE_ Vector3(real_t p_x, real_t p_y, real_t p_z) {
		x = p_x;
		y = p_y;
		z = p_z;
	}
	_FORCE_INLINE_ Vector3() { x = y = z = 0; }
};

Vector3 Vector3::cross(const Vector3 &p_b) const {
	Vector3 ret(
			(y * p_b.z) - (z * p_b.y),
			(z * p_b.x) - (x * p_b.z),
			(x * p_b.y) - (y * p_b.x));

	return ret;
}

real_t Vector3::dot(const Vector3 &p_b) const {
	return x * p_b.x + y * p_b.y + z * p_b.z;
}

Vector3 Vector3::abs() const {
	return Vector3(Math::abs(x), Math::abs(y), Math::abs(z));
}

Vector3 Vector3::sign() const {
	return Vector3(SGN(x), SGN(y), SGN(z));
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

Vector3 Vector3::linear_interpolate(const Vector3 &p_to, real_t p_weight) const {
	return Vector3(
			x + (p_weight * (p_to.x - x)),
			y + (p_weight * (p_to.y - y)),
			z + (p_weight * (p_to.z - z)));
}

Vector3 Vector3::slerp(const Vector3 &p_to, real_t p_weight) const {
	real_t theta = angle_to(p_to);
	return rotated(cross(p_to).normalized(), theta * p_weight);
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

_FORCE_INLINE_ Vector3 operator*(real_t p_scalar, const Vector3 &p_vec) {
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
		} else {
			return y < p_v.y;
		}
	} else {
		return x < p_v.x;
	}
}

bool Vector3::operator>(const Vector3 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			return z > p_v.z;
		} else {
			return y > p_v.y;
		}
	} else {
		return x > p_v.x;
	}
}

bool Vector3::operator<=(const Vector3 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			return z <= p_v.z;
		} else {
			return y < p_v.y;
		}
	} else {
		return x < p_v.x;
	}
}

bool Vector3::operator>=(const Vector3 &p_v) const {
	if (x == p_v.x) {
		if (y == p_v.y) {
			return z >= p_v.z;
		} else {
			return y > p_v.y;
		}
	} else {
		return x > p_v.x;
	}
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

bool Vector3::normalize_and_get_length(real_t *r_length) {
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

Vector3 Vector3::normalized() const {
	Vector3 v = *this;
	if (v.normalize_and_get_length()) {
		return v;
	}
	return Vector3();
}

bool Vector3::is_normalized() const {
	// use length_squared() instead of length() to avoid sqrt(), makes it more stringent.
	return Math::is_equal_approx(length_squared(), 1, (real_t)UNIT_EPSILON);
}

Vector3 Vector3::inverse() const {
	return Vector3(1 / x, 1 / y, 1 / z);
}

void Vector3::zero() {
	x = y = z = 0;
}

// slide returns the component of the vector along the given plane, specified by its normal vector.
Vector3 Vector3::slide(const Vector3 &p_normal) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector3(), "The normal Vector3 must be normalized.");
#endif
	return *this - p_normal * this->dot(p_normal);
}

Vector3 Vector3::bounce(const Vector3 &p_normal) const {
	return -reflect(p_normal);
}

Vector3 Vector3::reflect(const Vector3 &p_normal) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!p_normal.is_normalized(), Vector3(), "The normal Vector3 must be normalized.");
#endif
	return 2 * p_normal * this->dot(p_normal) - *this;
}

bool Vector3::is_equal_approx(const Vector3 &p_v, real_t p_tolerance) const {
	return Math::is_equal_approx(x, p_v.x, p_tolerance) && Math::is_equal_approx(y, p_v.y, p_tolerance) && Math::is_equal_approx(z, p_v.z, p_tolerance);
}

#endif // VECTOR3_H
