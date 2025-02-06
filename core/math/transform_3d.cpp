/**************************************************************************/
/*  transform_3d.cpp                                                      */
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

#include "transform_3d.h"

#include "core/string/ustring.h"

void Transform3D::affine_invert() {
	basis.invert();
	origin = basis.xform(-origin);
}

Transform3D Transform3D::affine_inverse() const {
	Transform3D ret = *this;
	ret.affine_invert();
	return ret;
}

void Transform3D::invert() {
	basis.transpose();
	origin = basis.xform(-origin);
}

Transform3D Transform3D::inverse() const {
	// FIXME: this function assumes the basis is a rotation matrix, with no scaling.
	// Transform3D::affine_inverse can handle matrices with scaling, so GDScript should eventually use that.
	Transform3D ret = *this;
	ret.invert();
	return ret;
}

void Transform3D::rotate(const Vector3 &p_axis, real_t p_angle) {
	*this = rotated(p_axis, p_angle);
}

Transform3D Transform3D::rotated(const Vector3 &p_axis, real_t p_angle) const {
	// Equivalent to left multiplication
	Basis p_basis(p_axis, p_angle);
	return Transform3D(p_basis * basis, p_basis.xform(origin));
}

Transform3D Transform3D::rotated_local(const Vector3 &p_axis, real_t p_angle) const {
	// Equivalent to right multiplication
	Basis p_basis(p_axis, p_angle);
	return Transform3D(basis * p_basis, origin);
}

void Transform3D::rotate_basis(const Vector3 &p_axis, real_t p_angle) {
	basis.rotate(p_axis, p_angle);
}

Transform3D Transform3D::looking_at(const Vector3 &p_target, const Vector3 &p_up, bool p_use_model_front) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(origin.is_equal_approx(p_target), Transform3D(), "The transform's origin and target can't be equal.");
#endif
	Transform3D t = *this;
	t.basis = Basis::looking_at(p_target - origin, p_up, p_use_model_front);
	return t;
}

void Transform3D::set_look_at(const Vector3 &p_eye, const Vector3 &p_target, const Vector3 &p_up, bool p_use_model_front) {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_MSG(p_eye.is_equal_approx(p_target), "The eye and target vectors can't be equal.");
#endif
	basis = Basis::looking_at(p_target - p_eye, p_up, p_use_model_front);
	origin = p_eye;
}

Transform3D Transform3D::interpolate_with(const Transform3D &p_transform, real_t p_c) const {
	Transform3D interp;

	Vector3 src_scale = basis.get_scale();
	Quaternion src_rot = basis.get_rotation_quaternion();
	Vector3 src_loc = origin;

	Vector3 dst_scale = p_transform.basis.get_scale();
	Quaternion dst_rot = p_transform.basis.get_rotation_quaternion();
	Vector3 dst_loc = p_transform.origin;

	interp.basis.set_quaternion_scale(src_rot.slerp(dst_rot, p_c).normalized(), src_scale.lerp(dst_scale, p_c));
	interp.origin = src_loc.lerp(dst_loc, p_c);

	return interp;
}

void Transform3D::scale(const Vector3 &p_scale) {
	basis.scale(p_scale);
	origin *= p_scale;
}

Transform3D Transform3D::scaled(const Vector3 &p_scale) const {
	// Equivalent to left multiplication
	return Transform3D(basis.scaled(p_scale), origin * p_scale);
}

Transform3D Transform3D::scaled_local(const Vector3 &p_scale) const {
	// Equivalent to right multiplication
	return Transform3D(basis.scaled_local(p_scale), origin);
}

void Transform3D::scale_basis(const Vector3 &p_scale) {
	basis.scale(p_scale);
}

void Transform3D::translate_local(real_t p_tx, real_t p_ty, real_t p_tz) {
	translate_local(Vector3(p_tx, p_ty, p_tz));
}

void Transform3D::translate_local(const Vector3 &p_translation) {
	for (int i = 0; i < 3; i++) {
		origin[i] += basis[i].dot(p_translation);
	}
}

Transform3D Transform3D::translated(const Vector3 &p_translation) const {
	// Equivalent to left multiplication
	return Transform3D(basis, origin + p_translation);
}

Transform3D Transform3D::translated_local(const Vector3 &p_translation) const {
	// Equivalent to right multiplication
	return Transform3D(basis, origin + basis.xform(p_translation));
}

void Transform3D::orthonormalize() {
	basis.orthonormalize();
}

Transform3D Transform3D::orthonormalized() const {
	Transform3D _copy = *this;
	_copy.orthonormalize();
	return _copy;
}

void Transform3D::orthogonalize() {
	basis.orthogonalize();
}

Transform3D Transform3D::orthogonalized() const {
	Transform3D _copy = *this;
	_copy.orthogonalize();
	return _copy;
}

bool Transform3D::is_equal_approx(const Transform3D &p_transform) const {
	return basis.is_equal_approx(p_transform.basis) && origin.is_equal_approx(p_transform.origin);
}

bool Transform3D::is_same(const Transform3D &p_transform) const {
	return basis.is_same(p_transform.basis) && origin.is_same(p_transform.origin);
}

bool Transform3D::is_finite() const {
	return basis.is_finite() && origin.is_finite();
}

bool Transform3D::operator==(const Transform3D &p_transform) const {
	return (basis == p_transform.basis && origin == p_transform.origin);
}

bool Transform3D::operator!=(const Transform3D &p_transform) const {
	return (basis != p_transform.basis || origin != p_transform.origin);
}

void Transform3D::operator*=(const Transform3D &p_transform) {
	origin = xform(p_transform.origin);
	basis *= p_transform.basis;
}

Transform3D Transform3D::operator*(const Transform3D &p_transform) const {
	Transform3D t = *this;
	t *= p_transform;
	return t;
}

void Transform3D::operator*=(real_t p_val) {
	origin *= p_val;
	basis *= p_val;
}

Transform3D Transform3D::operator*(real_t p_val) const {
	Transform3D ret(*this);
	ret *= p_val;
	return ret;
}

void Transform3D::operator/=(real_t p_val) {
	basis /= p_val;
	origin /= p_val;
}

Transform3D Transform3D::operator/(real_t p_val) const {
	Transform3D ret(*this);
	ret /= p_val;
	return ret;
}

Transform3D::operator String() const {
	return "[X: " + basis.get_column(0).operator String() +
			", Y: " + basis.get_column(1).operator String() +
			", Z: " + basis.get_column(2).operator String() +
			", O: " + origin.operator String() + "]";
}

Transform3D::Transform3D(const Basis &p_basis, const Vector3 &p_origin) :
		basis(p_basis),
		origin(p_origin) {
}

Transform3D::Transform3D(const Vector3 &p_x, const Vector3 &p_y, const Vector3 &p_z, const Vector3 &p_origin) :
		origin(p_origin) {
	basis.set_column(0, p_x);
	basis.set_column(1, p_y);
	basis.set_column(2, p_z);
}

Transform3D::Transform3D(real_t p_xx, real_t p_xy, real_t p_xz, real_t p_yx, real_t p_yy, real_t p_yz, real_t p_zx, real_t p_zy, real_t p_zz, real_t p_ox, real_t p_oy, real_t p_oz) {
	basis = Basis(p_xx, p_xy, p_xz, p_yx, p_yy, p_yz, p_zx, p_zy, p_zz);
	origin = Vector3(p_ox, p_oy, p_oz);
}
