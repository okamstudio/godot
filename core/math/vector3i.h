/**************************************************************************/
/*  vector3i.h                                                            */
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
struct Vector3;

struct [[nodiscard]] Vector3i {
	static const int AXIS_COUNT = 3;

	enum Axis {
		AXIS_X,
		AXIS_Y,
		AXIS_Z,
	};

	union {
		struct {
			int32_t x;
			int32_t y;
			int32_t z;
		};

		int32_t coord[3] = { 0 };
	};

	_FORCE_INLINE_ const int32_t &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 3);
		return coord[p_axis];
	}

	_FORCE_INLINE_ int32_t &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 3);
		return coord[p_axis];
	}

	Vector3i::Axis min_axis_index() const;
	Vector3i::Axis max_axis_index() const;

	Vector3i min(const Vector3i &p_vector3i) const {
		return Vector3i(MIN(x, p_vector3i.x), MIN(y, p_vector3i.y), MIN(z, p_vector3i.z));
	}

	Vector3i mini(int32_t p_scalar) const {
		return Vector3i(MIN(x, p_scalar), MIN(y, p_scalar), MIN(z, p_scalar));
	}

	Vector3i max(const Vector3i &p_vector3i) const {
		return Vector3i(MAX(x, p_vector3i.x), MAX(y, p_vector3i.y), MAX(z, p_vector3i.z));
	}

	Vector3i maxi(int32_t p_scalar) const {
		return Vector3i(MAX(x, p_scalar), MAX(y, p_scalar), MAX(z, p_scalar));
	}

	_FORCE_INLINE_ int64_t length_squared() const;
	_FORCE_INLINE_ double length() const;

	_FORCE_INLINE_ void zero();

	_FORCE_INLINE_ Vector3i abs() const;
	_FORCE_INLINE_ Vector3i sign() const;
	Vector3i clamp(const Vector3i &p_min, const Vector3i &p_max) const;
	Vector3i clampi(int32_t p_min, int32_t p_max) const;
	Vector3i snapped(const Vector3i &p_step) const;
	Vector3i snappedi(int32_t p_step) const;

	_FORCE_INLINE_ double distance_to(const Vector3i &p_to) const;
	_FORCE_INLINE_ int64_t distance_squared_to(const Vector3i &p_to) const;

	/* Operators */

	_FORCE_INLINE_ Vector3i &operator+=(const Vector3i &p_v);
	_FORCE_INLINE_ Vector3i operator+(const Vector3i &p_v) const;
	_FORCE_INLINE_ Vector3i &operator-=(const Vector3i &p_v);
	_FORCE_INLINE_ Vector3i operator-(const Vector3i &p_v) const;
	_FORCE_INLINE_ Vector3i &operator*=(const Vector3i &p_v);
	_FORCE_INLINE_ Vector3i operator*(const Vector3i &p_v) const;
	_FORCE_INLINE_ Vector3i &operator/=(const Vector3i &p_v);
	_FORCE_INLINE_ Vector3i operator/(const Vector3i &p_v) const;
	_FORCE_INLINE_ Vector3i &operator%=(const Vector3i &p_v);
	_FORCE_INLINE_ Vector3i operator%(const Vector3i &p_v) const;

	_FORCE_INLINE_ Vector3i &operator*=(int32_t p_scalar);
	_FORCE_INLINE_ Vector3i operator*(int32_t p_scalar) const;
	_FORCE_INLINE_ Vector3i &operator/=(int32_t p_scalar);
	_FORCE_INLINE_ Vector3i operator/(int32_t p_scalar) const;
	_FORCE_INLINE_ Vector3i &operator%=(int32_t p_scalar);
	_FORCE_INLINE_ Vector3i operator%(int32_t p_scalar) const;

	_FORCE_INLINE_ Vector3i operator-() const;

	_FORCE_INLINE_ bool operator==(const Vector3i &p_v) const;
	_FORCE_INLINE_ bool operator!=(const Vector3i &p_v) const;
	_FORCE_INLINE_ bool operator<(const Vector3i &p_v) const;
	_FORCE_INLINE_ bool operator<=(const Vector3i &p_v) const;
	_FORCE_INLINE_ bool operator>(const Vector3i &p_v) const;
	_FORCE_INLINE_ bool operator>=(const Vector3i &p_v) const;

	operator String() const;
	operator Vector3() const;

	_FORCE_INLINE_ Vector3i() {}
	_FORCE_INLINE_ Vector3i(int32_t p_x, int32_t p_y, int32_t p_z) {
		x = p_x;
		y = p_y;
		z = p_z;
	}
};

int64_t Vector3i::length_squared() const {
	return x * (int64_t)x + y * (int64_t)y + z * (int64_t)z;
}

double Vector3i::length() const {
	return Math::sqrt((double)length_squared());
}

Vector3i Vector3i::abs() const {
	return Vector3i(Math::abs(x), Math::abs(y), Math::abs(z));
}

Vector3i Vector3i::sign() const {
	return Vector3i(SIGN(x), SIGN(y), SIGN(z));
}

double Vector3i::distance_to(const Vector3i &p_to) const {
	return (p_to - *this).length();
}

int64_t Vector3i::distance_squared_to(const Vector3i &p_to) const {
	return (p_to - *this).length_squared();
}

/* Operators */

Vector3i &Vector3i::operator+=(const Vector3i &p_v) {
	x += p_v.x;
	y += p_v.y;
	z += p_v.z;
	return *this;
}

Vector3i Vector3i::operator+(const Vector3i &p_v) const {
	return Vector3i(x + p_v.x, y + p_v.y, z + p_v.z);
}

Vector3i &Vector3i::operator-=(const Vector3i &p_v) {
	x -= p_v.x;
	y -= p_v.y;
	z -= p_v.z;
	return *this;
}

Vector3i Vector3i::operator-(const Vector3i &p_v) const {
	return Vector3i(x - p_v.x, y - p_v.y, z - p_v.z);
}

Vector3i &Vector3i::operator*=(const Vector3i &p_v) {
	x *= p_v.x;
	y *= p_v.y;
	z *= p_v.z;
	return *this;
}

Vector3i Vector3i::operator*(const Vector3i &p_v) const {
	return Vector3i(x * p_v.x, y * p_v.y, z * p_v.z);
}

Vector3i &Vector3i::operator/=(const Vector3i &p_v) {
	x /= p_v.x;
	y /= p_v.y;
	z /= p_v.z;
	return *this;
}

Vector3i Vector3i::operator/(const Vector3i &p_v) const {
	return Vector3i(x / p_v.x, y / p_v.y, z / p_v.z);
}

Vector3i &Vector3i::operator%=(const Vector3i &p_v) {
	x %= p_v.x;
	y %= p_v.y;
	z %= p_v.z;
	return *this;
}

Vector3i Vector3i::operator%(const Vector3i &p_v) const {
	return Vector3i(x % p_v.x, y % p_v.y, z % p_v.z);
}

Vector3i &Vector3i::operator*=(int32_t p_scalar) {
	x *= p_scalar;
	y *= p_scalar;
	z *= p_scalar;
	return *this;
}

Vector3i Vector3i::operator*(int32_t p_scalar) const {
	return Vector3i(x * p_scalar, y * p_scalar, z * p_scalar);
}

// Multiplication operators required to workaround issues with LLVM using implicit conversion.

_FORCE_INLINE_ Vector3i operator*(int32_t p_scalar, const Vector3i &p_vector) {
	return p_vector * p_scalar;
}

_FORCE_INLINE_ Vector3i operator*(int64_t p_scalar, const Vector3i &p_vector) {
	return p_vector * p_scalar;
}

_FORCE_INLINE_ Vector3i operator*(float p_scalar, const Vector3i &p_vector) {
	return p_vector * p_scalar;
}

_FORCE_INLINE_ Vector3i operator*(double p_scalar, const Vector3i &p_vector) {
	return p_vector * p_scalar;
}

Vector3i &Vector3i::operator/=(int32_t p_scalar) {
	x /= p_scalar;
	y /= p_scalar;
	z /= p_scalar;
	return *this;
}

Vector3i Vector3i::operator/(int32_t p_scalar) const {
	return Vector3i(x / p_scalar, y / p_scalar, z / p_scalar);
}

Vector3i &Vector3i::operator%=(int32_t p_scalar) {
	x %= p_scalar;
	y %= p_scalar;
	z %= p_scalar;
	return *this;
}

Vector3i Vector3i::operator%(int32_t p_scalar) const {
	return Vector3i(x % p_scalar, y % p_scalar, z % p_scalar);
}

Vector3i Vector3i::operator-() const {
	return Vector3i(-x, -y, -z);
}

bool Vector3i::operator==(const Vector3i &p_v) const {
	return (x == p_v.x && y == p_v.y && z == p_v.z);
}

bool Vector3i::operator!=(const Vector3i &p_v) const {
	return (x != p_v.x || y != p_v.y || z != p_v.z);
}

bool Vector3i::operator<(const Vector3i &p_v) const {
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

bool Vector3i::operator>(const Vector3i &p_v) const {
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

bool Vector3i::operator<=(const Vector3i &p_v) const {
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

bool Vector3i::operator>=(const Vector3i &p_v) const {
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

void Vector3i::zero() {
	x = y = z = 0;
}
