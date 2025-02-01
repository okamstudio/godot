/**************************************************************************/
/*  basis.h                                                               */
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

#include "core/math/quaternion.h"
#include "core/math/vector3.h"

struct [[nodiscard]] Basis {
	Vector3 rows[3] = {
		Vector3(1, 0, 0),
		Vector3(0, 1, 0),
		Vector3(0, 0, 1)
	};

	_FORCE_INLINE_ const Vector3 &operator[](int p_row) const {
		return rows[p_row];
	}
	_FORCE_INLINE_ Vector3 &operator[](int p_row) {
		return rows[p_row];
	}

	void invert();
	void transpose();

	Basis inverse() const;
	Basis transposed() const;

	_FORCE_INLINE_ real_t determinant() const;

	void rotate(const Vector3 &p_axis, real_t p_angle);
	Basis rotated(const Vector3 &p_axis, real_t p_angle) const;

	void rotate_local(const Vector3 &p_axis, real_t p_angle);
	Basis rotated_local(const Vector3 &p_axis, real_t p_angle) const;

	void rotate(const Vector3 &p_euler, EulerOrder p_order = EulerOrder::YXZ);
	Basis rotated(const Vector3 &p_euler, EulerOrder p_order = EulerOrder::YXZ) const;

	void rotate(const Quaternion &p_quaternion);
	Basis rotated(const Quaternion &p_quaternion) const;

	Vector3 get_euler_normalized(EulerOrder p_order = EulerOrder::YXZ) const;
	void get_rotation_axis_angle(Vector3 &p_axis, real_t &p_angle) const;
	void get_rotation_axis_angle_local(Vector3 &p_axis, real_t &p_angle) const;
	Quaternion get_rotation_quaternion() const;

	void rotate_to_align(Vector3 p_start_direction, Vector3 p_end_direction);

	Vector3 rotref_posscale_decomposition(Basis &rotref) const;

	Vector3 get_euler(EulerOrder p_order = EulerOrder::YXZ) const;
	void set_euler(const Vector3 &p_euler, EulerOrder p_order = EulerOrder::YXZ);
	static Basis from_euler(const Vector3 &p_euler, EulerOrder p_order = EulerOrder::YXZ) {
		Basis b;
		b.set_euler(p_euler, p_order);
		return b;
	}

	Quaternion get_quaternion() const;
	void set_quaternion(const Quaternion &p_quaternion);

	void get_axis_angle(Vector3 &r_axis, real_t &r_angle) const;
	void set_axis_angle(const Vector3 &p_axis, real_t p_angle);

	void scale(const Vector3 &p_scale);
	Basis scaled(const Vector3 &p_scale) const;

	void scale_local(const Vector3 &p_scale);
	Basis scaled_local(const Vector3 &p_scale) const;

	void scale_orthogonal(const Vector3 &p_scale);
	Basis scaled_orthogonal(const Vector3 &p_scale) const;
	real_t get_uniform_scale() const;

	Vector3 get_scale() const;
	Vector3 get_scale_abs() const;
	Vector3 get_scale_global() const;

	void set_axis_angle_scale(const Vector3 &p_axis, real_t p_angle, const Vector3 &p_scale);
	void set_euler_scale(const Vector3 &p_euler, const Vector3 &p_scale, EulerOrder p_order = EulerOrder::YXZ);
	void set_quaternion_scale(const Quaternion &p_quaternion, const Vector3 &p_scale);

	// transposed dot products
	_FORCE_INLINE_ real_t tdotx(const Vector3 &p_v) const {
		return rows[0][0] * p_v[0] + rows[1][0] * p_v[1] + rows[2][0] * p_v[2];
	}
	_FORCE_INLINE_ real_t tdoty(const Vector3 &p_v) const {
		return rows[0][1] * p_v[0] + rows[1][1] * p_v[1] + rows[2][1] * p_v[2];
	}
	_FORCE_INLINE_ real_t tdotz(const Vector3 &p_v) const {
		return rows[0][2] * p_v[0] + rows[1][2] * p_v[1] + rows[2][2] * p_v[2];
	}

	bool is_equal_approx(const Basis &p_basis) const;
	bool is_finite() const;

	bool operator==(const Basis &p_matrix) const;
	bool operator!=(const Basis &p_matrix) const;

	_FORCE_INLINE_ Vector3 xform(const Vector3 &p_vector) const;
	_FORCE_INLINE_ Vector3 xform_inv(const Vector3 &p_vector) const;
	_FORCE_INLINE_ void operator*=(const Basis &p_matrix);
	_FORCE_INLINE_ Basis operator*(const Basis &p_matrix) const;
	_FORCE_INLINE_ void operator+=(const Basis &p_matrix);
	_FORCE_INLINE_ Basis operator+(const Basis &p_matrix) const;
	_FORCE_INLINE_ void operator-=(const Basis &p_matrix);
	_FORCE_INLINE_ Basis operator-(const Basis &p_matrix) const;
	_FORCE_INLINE_ void operator*=(real_t p_val);
	_FORCE_INLINE_ Basis operator*(real_t p_val) const;
	_FORCE_INLINE_ void operator/=(real_t p_val);
	_FORCE_INLINE_ Basis operator/(real_t p_val) const;

	bool is_orthogonal() const;
	bool is_orthonormal() const;
	bool is_conformal() const;
	bool is_diagonal() const;
	bool is_rotation() const;

	Basis lerp(const Basis &p_to, real_t p_weight) const;
	Basis slerp(const Basis &p_to, real_t p_weight) const;
	void rotate_sh(real_t *p_values);

	operator String() const;

	/* create / set */

	_FORCE_INLINE_ void set(real_t p_xx, real_t p_xy, real_t p_xz, real_t p_yx, real_t p_yy, real_t p_yz, real_t p_zx, real_t p_zy, real_t p_zz) {
		rows[0][0] = p_xx;
		rows[0][1] = p_xy;
		rows[0][2] = p_xz;
		rows[1][0] = p_yx;
		rows[1][1] = p_yy;
		rows[1][2] = p_yz;
		rows[2][0] = p_zx;
		rows[2][1] = p_zy;
		rows[2][2] = p_zz;
	}
	_FORCE_INLINE_ void set_columns(const Vector3 &p_x, const Vector3 &p_y, const Vector3 &p_z) {
		set_column(0, p_x);
		set_column(1, p_y);
		set_column(2, p_z);
	}

	_FORCE_INLINE_ Vector3 get_column(int p_index) const {
		// Get actual basis axis column (we store transposed as rows for performance).
		return Vector3(rows[0][p_index], rows[1][p_index], rows[2][p_index]);
	}

	_FORCE_INLINE_ void set_column(int p_index, const Vector3 &p_value) {
		// Set actual basis axis column (we store transposed as rows for performance).
		rows[0][p_index] = p_value.x;
		rows[1][p_index] = p_value.y;
		rows[2][p_index] = p_value.z;
	}

	_FORCE_INLINE_ Vector3 get_main_diagonal() const {
		return Vector3(rows[0][0], rows[1][1], rows[2][2]);
	}

	_FORCE_INLINE_ void set_zero() {
		rows[0].zero();
		rows[1].zero();
		rows[2].zero();
	}

	_FORCE_INLINE_ Basis transpose_xform(const Basis &p_m) const {
		return Basis(
				rows[0].x * p_m[0].x + rows[1].x * p_m[1].x + rows[2].x * p_m[2].x,
				rows[0].x * p_m[0].y + rows[1].x * p_m[1].y + rows[2].x * p_m[2].y,
				rows[0].x * p_m[0].z + rows[1].x * p_m[1].z + rows[2].x * p_m[2].z,
				rows[0].y * p_m[0].x + rows[1].y * p_m[1].x + rows[2].y * p_m[2].x,
				rows[0].y * p_m[0].y + rows[1].y * p_m[1].y + rows[2].y * p_m[2].y,
				rows[0].y * p_m[0].z + rows[1].y * p_m[1].z + rows[2].y * p_m[2].z,
				rows[0].z * p_m[0].x + rows[1].z * p_m[1].x + rows[2].z * p_m[2].x,
				rows[0].z * p_m[0].y + rows[1].z * p_m[1].y + rows[2].z * p_m[2].y,
				rows[0].z * p_m[0].z + rows[1].z * p_m[1].z + rows[2].z * p_m[2].z);
	}
	Basis(real_t p_xx, real_t p_xy, real_t p_xz, real_t p_yx, real_t p_yy, real_t p_yz, real_t p_zx, real_t p_zy, real_t p_zz) {
		set(p_xx, p_xy, p_xz, p_yx, p_yy, p_yz, p_zx, p_zy, p_zz);
	}

	void orthonormalize();
	Basis orthonormalized() const;

	void orthogonalize();
	Basis orthogonalized() const;

#ifdef MATH_CHECKS
	bool is_symmetric() const;
#endif
	Basis diagonalize();

	operator Quaternion() const { return get_quaternion(); }

	static Basis looking_at(const Vector3 &p_target, const Vector3 &p_up = Vector3(0, 1, 0), bool p_use_model_front = false);

	Basis(const Quaternion &p_quaternion) { set_quaternion(p_quaternion); }
	Basis(const Quaternion &p_quaternion, const Vector3 &p_scale) { set_quaternion_scale(p_quaternion, p_scale); }

	Basis(const Vector3 &p_axis, real_t p_angle) { set_axis_angle(p_axis, p_angle); }
	Basis(const Vector3 &p_axis, real_t p_angle, const Vector3 &p_scale) { set_axis_angle_scale(p_axis, p_angle, p_scale); }
	static Basis from_scale(const Vector3 &p_scale);

	_FORCE_INLINE_ Basis(const Vector3 &p_x_axis, const Vector3 &p_y_axis, const Vector3 &p_z_axis) {
		set_columns(p_x_axis, p_y_axis, p_z_axis);
	}

	_FORCE_INLINE_ Basis() {}

private:
	// Helper method.
	void _set_diagonal(const Vector3 &p_diag);
};

_FORCE_INLINE_ void Basis::operator*=(const Basis &p_matrix) {
	set(
			p_matrix.tdotx(rows[0]), p_matrix.tdoty(rows[0]), p_matrix.tdotz(rows[0]),
			p_matrix.tdotx(rows[1]), p_matrix.tdoty(rows[1]), p_matrix.tdotz(rows[1]),
			p_matrix.tdotx(rows[2]), p_matrix.tdoty(rows[2]), p_matrix.tdotz(rows[2]));
}

_FORCE_INLINE_ Basis Basis::operator*(const Basis &p_matrix) const {
	return Basis(
			p_matrix.tdotx(rows[0]), p_matrix.tdoty(rows[0]), p_matrix.tdotz(rows[0]),
			p_matrix.tdotx(rows[1]), p_matrix.tdoty(rows[1]), p_matrix.tdotz(rows[1]),
			p_matrix.tdotx(rows[2]), p_matrix.tdoty(rows[2]), p_matrix.tdotz(rows[2]));
}

_FORCE_INLINE_ void Basis::operator+=(const Basis &p_matrix) {
	rows[0] += p_matrix.rows[0];
	rows[1] += p_matrix.rows[1];
	rows[2] += p_matrix.rows[2];
}

_FORCE_INLINE_ Basis Basis::operator+(const Basis &p_matrix) const {
	Basis ret(*this);
	ret += p_matrix;
	return ret;
}

_FORCE_INLINE_ void Basis::operator-=(const Basis &p_matrix) {
	rows[0] -= p_matrix.rows[0];
	rows[1] -= p_matrix.rows[1];
	rows[2] -= p_matrix.rows[2];
}

_FORCE_INLINE_ Basis Basis::operator-(const Basis &p_matrix) const {
	Basis ret(*this);
	ret -= p_matrix;
	return ret;
}

_FORCE_INLINE_ void Basis::operator*=(real_t p_val) {
	rows[0] *= p_val;
	rows[1] *= p_val;
	rows[2] *= p_val;
}

_FORCE_INLINE_ Basis Basis::operator*(real_t p_val) const {
	Basis ret(*this);
	ret *= p_val;
	return ret;
}

_FORCE_INLINE_ void Basis::operator/=(real_t p_val) {
	rows[0] /= p_val;
	rows[1] /= p_val;
	rows[2] /= p_val;
}

_FORCE_INLINE_ Basis Basis::operator/(real_t p_val) const {
	Basis ret(*this);
	ret /= p_val;
	return ret;
}

Vector3 Basis::xform(const Vector3 &p_vector) const {
	return Vector3(
			rows[0].dot(p_vector),
			rows[1].dot(p_vector),
			rows[2].dot(p_vector));
}

Vector3 Basis::xform_inv(const Vector3 &p_vector) const {
	return Vector3(
			(rows[0][0] * p_vector.x) + (rows[1][0] * p_vector.y) + (rows[2][0] * p_vector.z),
			(rows[0][1] * p_vector.x) + (rows[1][1] * p_vector.y) + (rows[2][1] * p_vector.z),
			(rows[0][2] * p_vector.x) + (rows[1][2] * p_vector.y) + (rows[2][2] * p_vector.z));
}

real_t Basis::determinant() const {
	return rows[0][0] * (rows[1][1] * rows[2][2] - rows[2][1] * rows[1][2]) -
			rows[1][0] * (rows[0][1] * rows[2][2] - rows[2][1] * rows[0][2]) +
			rows[2][0] * (rows[0][1] * rows[1][2] - rows[1][1] * rows[0][2]);
}
