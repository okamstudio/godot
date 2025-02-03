/**************************************************************************/
/*  quat.h                                                                */
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

#ifndef QUAT_H
#define QUAT_H

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"
#include "core/math/vector3.h"
#include "core/ustring.h"

class _NO_DISCARD_CLASS_ Quat {
public:
	real_t x, y, z, w;

	_FORCE_INLINE_ real_t length_squared() const;
	bool is_equal_approx(const Quat &p_quat) const;
	real_t length() const;
	void normalize();
	Quat normalized() const;
	bool is_normalized() const;
	Quat inverse() const;
	_FORCE_INLINE_ real_t dot(const Quat &p_q) const;
	real_t angle_to(const Quat &p_to) const;

	void set_euler_xyz(const Vector3 &p_euler);
	Vector3 get_euler_xyz() const;
	void set_euler_yxz(const Vector3 &p_euler);
	Vector3 get_euler_yxz() const;

	void set_euler(const Vector3 &p_euler) { set_euler_yxz(p_euler); }
	Vector3 get_euler() const { return get_euler_yxz(); }

	Quat slerp(const Quat &p_to, real_t p_weight) const;
	Quat slerpni(const Quat &p_to, real_t p_weight) const;
	Quat cubic_slerp(const Quat &p_b, const Quat &p_pre_a, const Quat &p_post_b, real_t p_weight) const;

	void set_axis_angle(const Vector3 &p_axis, real_t p_angle);
	_FORCE_INLINE_ void get_axis_angle(Vector3 &r_axis, real_t &r_angle) const {
		r_angle = 2 * Math::acos(w);
		real_t r = ((real_t)1) / Math::sqrt(1 - w * w);
		r_axis.x = x * r;
		r_axis.y = y * r;
		r_axis.z = z * r;
	}

	void set_shortest_arc(const Vector3 &p_from, const Vector3 &p_to) {
		Vector3 c = p_from.cross(p_to);
		real_t d = p_from.dot(p_to);

		if (d < -1 + (real_t)CMP_EPSILON) {
			x = 0;
			y = 1;
			z = 0;
			w = 0;
		} else {
			real_t s = Math::sqrt((1 + d) * 2);
			real_t rs = 1 / s;

			x = c.x * rs;
			y = c.y * rs;
			z = c.z * rs;
			w = s * 0.5f;
		}
	}

	void operator*=(const Quat &p_q);
	Quat operator*(const Quat &p_q) const;

	Quat operator*(const Vector3 &p_v) const {
		return Quat(w * p_v.x + y * p_v.z - z * p_v.y,
				w * p_v.y + z * p_v.x - x * p_v.z,
				w * p_v.z + x * p_v.y - y * p_v.x,
				-x * p_v.x - y * p_v.y - z * p_v.z);
	}

	_FORCE_INLINE_ Vector3 xform(const Vector3 &p_v) const {
#ifdef MATH_CHECKS
		ERR_FAIL_COND_V_MSG(!is_normalized(), p_v, "The quaternion must be normalized.");
#endif
		Vector3 u(x, y, z);
		Vector3 uv = u.cross(p_v);
		return p_v + ((uv * w) + u.cross(uv)) * ((real_t)2);
	}

	_FORCE_INLINE_ void operator+=(const Quat &p_q);
	_FORCE_INLINE_ void operator-=(const Quat &p_q);
	_FORCE_INLINE_ void operator*=(real_t p_s);
	_FORCE_INLINE_ void operator/=(real_t p_s);
	_FORCE_INLINE_ Quat operator+(const Quat &p_q2) const;
	_FORCE_INLINE_ Quat operator-(const Quat &p_q2) const;
	_FORCE_INLINE_ Quat operator-() const;
	_FORCE_INLINE_ Quat operator*(real_t p_s) const;
	_FORCE_INLINE_ Quat operator/(real_t p_s) const;

	_FORCE_INLINE_ bool operator==(const Quat &p_quat) const;
	_FORCE_INLINE_ bool operator!=(const Quat &p_quat) const;

	operator String() const;

	inline void set(real_t p_x, real_t p_y, real_t p_z, real_t p_w) {
		x = p_x;
		y = p_y;
		z = p_z;
		w = p_w;
	}
	inline Quat(real_t p_x, real_t p_y, real_t p_z, real_t p_w) :
			x(p_x),
			y(p_y),
			z(p_z),
			w(p_w) {
	}
	Quat(const Vector3 &p_axis, real_t p_angle) { set_axis_angle(p_axis, p_angle); }

	Quat(const Vector3 &p_euler) { set_euler(p_euler); }
	Quat(const Quat &p_q) :
			x(p_q.x),
			y(p_q.y),
			z(p_q.z),
			w(p_q.w) {
	}

	Quat &operator=(const Quat &p_q) {
		x = p_q.x;
		y = p_q.y;
		z = p_q.z;
		w = p_q.w;
		return *this;
	}

	Quat(const Vector3 &p_v0, const Vector3 &p_v1) {
		set_shortest_arc(p_v0, p_v1);
	}

	inline Quat() :
			x(0),
			y(0),
			z(0),
			w(1) {
	}
};

real_t Quat::dot(const Quat &p_q) const {
	return x * p_q.x + y * p_q.y + z * p_q.z + w * p_q.w;
}

real_t Quat::length_squared() const {
	return dot(*this);
}

void Quat::operator+=(const Quat &p_q) {
	x += p_q.x;
	y += p_q.y;
	z += p_q.z;
	w += p_q.w;
}

void Quat::operator-=(const Quat &p_q) {
	x -= p_q.x;
	y -= p_q.y;
	z -= p_q.z;
	w -= p_q.w;
}

void Quat::operator*=(real_t p_s) {
	x *= p_s;
	y *= p_s;
	z *= p_s;
	w *= p_s;
}

void Quat::operator/=(real_t p_s) {
	*this *= 1 / p_s;
}

Quat Quat::operator+(const Quat &p_q2) const {
	const Quat &q1 = *this;
	return Quat(q1.x + p_q2.x, q1.y + p_q2.y, q1.z + p_q2.z, q1.w + p_q2.w);
}

Quat Quat::operator-(const Quat &p_q2) const {
	const Quat &q1 = *this;
	return Quat(q1.x - p_q2.x, q1.y - p_q2.y, q1.z - p_q2.z, q1.w - p_q2.w);
}

Quat Quat::operator-() const {
	const Quat &q2 = *this;
	return Quat(-q2.x, -q2.y, -q2.z, -q2.w);
}

Quat Quat::operator*(real_t p_s) const {
	return Quat(x * p_s, y * p_s, z * p_s, w * p_s);
}

Quat Quat::operator/(real_t p_s) const {
	return *this * (1 / p_s);
}

bool Quat::operator==(const Quat &p_quat) const {
	return x == p_quat.x && y == p_quat.y && z == p_quat.z && w == p_quat.w;
}

bool Quat::operator!=(const Quat &p_quat) const {
	return x != p_quat.x || y != p_quat.y || z != p_quat.z || w != p_quat.w;
}

#endif // QUAT_H
