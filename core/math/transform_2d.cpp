/**************************************************************************/
/*  transform_2d.cpp                                                      */
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

#include "transform_2d.h"

#include "core/string/ustring.h"

void Transform2D::invert() {
	// FIXME: this function assumes the basis is a rotation matrix, with no scaling.
	// Transform2D::affine_inverse can handle matrices with scaling, so GDScript should eventually use that.
	SWAP(columns[0][1], columns[1][0]);
	columns[2] = basis_xform(-columns[2]);
}

Transform2D Transform2D::inverse() const {
	Transform2D inv = *this;
	inv.invert();
	return inv;
}

void Transform2D::affine_invert() {
	real_t det = determinant();
#ifdef MATH_CHECKS
	ERR_FAIL_COND(det == 0);
#endif
	real_t idet = 1.0f / det;

	SWAP(columns[0][0], columns[1][1]);
	columns[0] *= Vector2(idet, -idet);
	columns[1] *= Vector2(-idet, idet);

	columns[2] = basis_xform(-columns[2]);
}

Transform2D Transform2D::affine_inverse() const {
	Transform2D inv = *this;
	inv.affine_invert();
	return inv;
}

void Transform2D::rotate(real_t p_angle) {
	*this = Transform2D(p_angle, Vector2()) * (*this);
}

real_t Transform2D::get_skew() const {
	real_t det = determinant();
	return Math::acos(columns[0].normalized().dot(SIGN(det) * columns[1].normalized())) - (real_t)Math_PI * 0.5f;
}

void Transform2D::set_skew(real_t p_angle) {
	real_t det = determinant();
	columns[1] = SIGN(det) * columns[0].rotated(((real_t)Math_PI * 0.5f + p_angle)).normalized() * columns[1].length();
}

real_t Transform2D::get_rotation() const {
	return Math::atan2(columns[0].y, columns[0].x);
}

void Transform2D::set_rotation(real_t p_rot) {
	Size2 scale = get_scale();
	real_t cr = Math::cos(p_rot);
	real_t sr = Math::sin(p_rot);
	columns[0][0] = cr;
	columns[0][1] = sr;
	columns[1][0] = -sr;
	columns[1][1] = cr;
	set_scale(scale);
}

Transform2D::Transform2D(real_t p_rot, const Vector2 &p_pos) {
	real_t cr = Math::cos(p_rot);
	real_t sr = Math::sin(p_rot);
	columns[0][0] = cr;
	columns[0][1] = sr;
	columns[1][0] = -sr;
	columns[1][1] = cr;
	columns[2] = p_pos;
}

Transform2D::Transform2D(real_t p_rot, const Size2 &p_scale, real_t p_skew, const Vector2 &p_pos) {
	columns[0][0] = Math::cos(p_rot) * p_scale.x;
	columns[1][1] = Math::cos(p_rot + p_skew) * p_scale.y;
	columns[1][0] = -Math::sin(p_rot + p_skew) * p_scale.y;
	columns[0][1] = Math::sin(p_rot) * p_scale.x;
	columns[2] = p_pos;
}

Size2 Transform2D::get_scale() const {
	real_t det_sign = SIGN(determinant());
	return Size2(columns[0].length(), det_sign * columns[1].length());
}

void Transform2D::set_scale(const Size2 &p_scale) {
	columns[0].normalize();
	columns[1].normalize();
	columns[0] *= p_scale.x;
	columns[1] *= p_scale.y;
}

void Transform2D::scale(const Size2 &p_scale) {
	scale_basis(p_scale);
	columns[2] *= p_scale;
}

void Transform2D::scale_basis(const Size2 &p_scale) {
	columns[0][0] *= p_scale.x;
	columns[0][1] *= p_scale.y;
	columns[1][0] *= p_scale.x;
	columns[1][1] *= p_scale.y;
}

void Transform2D::translate_local(real_t p_tx, real_t p_ty) {
	translate_local(Vector2(p_tx, p_ty));
}

void Transform2D::translate_local(const Vector2 &p_translation) {
	columns[2] += basis_xform(p_translation);
}

void Transform2D::orthonormalize() {
	// Gram-Schmidt Process

	Vector2 x = columns[0];
	Vector2 y = columns[1];

	x.normalize();
	y = y - x * x.dot(y);
	y.normalize();

	columns[0] = x;
	columns[1] = y;
}

Transform2D Transform2D::orthonormalized() const {
	Transform2D ortho = *this;
	ortho.orthonormalize();
	return ortho;
}

bool Transform2D::is_conformal() const {
	// Non-flipped case.
	if (Math::is_equal_approx(columns[0][0], columns[1][1]) && Math::is_equal_approx(columns[0][1], -columns[1][0])) {
		return true;
	}
	// Flipped case.
	if (Math::is_equal_approx(columns[0][0], -columns[1][1]) && Math::is_equal_approx(columns[0][1], columns[1][0])) {
		return true;
	}
	return false;
}

bool Transform2D::is_equal_approx(const Transform2D &p_transform) const {
	return columns[0].is_equal_approx(p_transform.columns[0]) && columns[1].is_equal_approx(p_transform.columns[1]) && columns[2].is_equal_approx(p_transform.columns[2]);
}

bool Transform2D::is_same(const Transform2D &p_transform) const {
	return columns[0].is_same(p_transform.columns[0]) && columns[1].is_same(p_transform.columns[1]) && columns[2].is_same(p_transform.columns[2]);
}

bool Transform2D::is_finite() const {
	return columns[0].is_finite() && columns[1].is_finite() && columns[2].is_finite();
}

Transform2D Transform2D::looking_at(const Vector2 &p_target) const {
	Transform2D return_trans = Transform2D(get_rotation(), get_origin());
	Vector2 target_position = affine_inverse().xform(p_target);
	return_trans.set_rotation(return_trans.get_rotation() + (target_position * get_scale()).angle());
	return return_trans;
}

bool Transform2D::operator==(const Transform2D &p_transform) const {
	for (int i = 0; i < 3; i++) {
		if (columns[i] != p_transform.columns[i]) {
			return false;
		}
	}

	return true;
}

bool Transform2D::operator!=(const Transform2D &p_transform) const {
	for (int i = 0; i < 3; i++) {
		if (columns[i] != p_transform.columns[i]) {
			return true;
		}
	}

	return false;
}

void Transform2D::operator*=(const Transform2D &p_transform) {
	columns[2] = xform(p_transform.columns[2]);

	real_t x0, x1, y0, y1;

	x0 = tdotx(p_transform.columns[0]);
	x1 = tdoty(p_transform.columns[0]);
	y0 = tdotx(p_transform.columns[1]);
	y1 = tdoty(p_transform.columns[1]);

	columns[0][0] = x0;
	columns[0][1] = x1;
	columns[1][0] = y0;
	columns[1][1] = y1;
}

Transform2D Transform2D::operator*(const Transform2D &p_transform) const {
	Transform2D t = *this;
	t *= p_transform;
	return t;
}

Transform2D Transform2D::scaled(const Size2 &p_scale) const {
	// Equivalent to left multiplication
	Transform2D copy = *this;
	copy.scale(p_scale);
	return copy;
}

Transform2D Transform2D::scaled_local(const Size2 &p_scale) const {
	// Equivalent to right multiplication
	return Transform2D(columns[0] * p_scale.x, columns[1] * p_scale.y, columns[2]);
}

Transform2D Transform2D::untranslated() const {
	Transform2D copy = *this;
	copy.columns[2] = Vector2();
	return copy;
}

Transform2D Transform2D::translated(const Vector2 &p_offset) const {
	// Equivalent to left multiplication
	return Transform2D(columns[0], columns[1], columns[2] + p_offset);
}

Transform2D Transform2D::translated_local(const Vector2 &p_offset) const {
	// Equivalent to right multiplication
	return Transform2D(columns[0], columns[1], columns[2] + basis_xform(p_offset));
}

Transform2D Transform2D::rotated(real_t p_angle) const {
	// Equivalent to left multiplication
	return Transform2D(p_angle, Vector2()) * (*this);
}

Transform2D Transform2D::rotated_local(real_t p_angle) const {
	// Equivalent to right multiplication
	return (*this) * Transform2D(p_angle, Vector2()); // Could be optimized, because origin transform can be skipped.
}

real_t Transform2D::determinant() const {
	return columns[0].x * columns[1].y - columns[0].y * columns[1].x;
}

Transform2D Transform2D::interpolate_with(const Transform2D &p_transform, real_t p_weight) const {
	return Transform2D(
			Math::lerp_angle(get_rotation(), p_transform.get_rotation(), p_weight),
			get_scale().lerp(p_transform.get_scale(), p_weight),
			Math::lerp_angle(get_skew(), p_transform.get_skew(), p_weight),
			get_origin().lerp(p_transform.get_origin(), p_weight));
}

void Transform2D::operator*=(real_t p_val) {
	columns[0] *= p_val;
	columns[1] *= p_val;
	columns[2] *= p_val;
}

Transform2D Transform2D::operator*(real_t p_val) const {
	Transform2D ret(*this);
	ret *= p_val;
	return ret;
}

void Transform2D::operator/=(real_t p_val) {
	columns[0] /= p_val;
	columns[1] /= p_val;
	columns[2] /= p_val;
}

Transform2D Transform2D::operator/(real_t p_val) const {
	Transform2D ret(*this);
	ret /= p_val;
	return ret;
}

Transform2D::operator String() const {
	return "[X: " + columns[0].operator String() +
			", Y: " + columns[1].operator String() +
			", O: " + columns[2].operator String() + "]";
}
