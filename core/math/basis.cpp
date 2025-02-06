/**************************************************************************/
/*  basis.cpp                                                             */
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

#include "basis.h"

#include "core/math/math_funcs.h"
#include "core/string/ustring.h"

#define cofac(row1, col1, row2, col2) \
	(rows[row1][col1] * rows[row2][col2] - rows[row1][col2] * rows[row2][col1])

void Basis::invert() {
	real_t co[3] = {
		cofac(1, 1, 2, 2), cofac(1, 2, 2, 0), cofac(1, 0, 2, 1)
	};
	real_t det = rows[0][0] * co[0] +
			rows[0][1] * co[1] +
			rows[0][2] * co[2];
#ifdef MATH_CHECKS
	ERR_FAIL_COND(det == 0);
#endif
	real_t s = 1.0f / det;

	set(co[0] * s, cofac(0, 2, 2, 1) * s, cofac(0, 1, 1, 2) * s,
			co[1] * s, cofac(0, 0, 2, 2) * s, cofac(0, 2, 1, 0) * s,
			co[2] * s, cofac(0, 1, 2, 0) * s, cofac(0, 0, 1, 1) * s);
}

void Basis::orthonormalize() {
	// Gram-Schmidt Process

	Vector3 x = get_column(0);
	Vector3 y = get_column(1);
	Vector3 z = get_column(2);

	x.normalize();
	y = (y - x * (x.dot(y)));
	y.normalize();
	z = (z - x * (x.dot(z)) - y * (y.dot(z)));
	z.normalize();

	set_column(0, x);
	set_column(1, y);
	set_column(2, z);
}

Basis Basis::orthonormalized() const {
	Basis c = *this;
	c.orthonormalize();
	return c;
}

void Basis::orthogonalize() {
	Vector3 scl = get_scale();
	orthonormalize();
	scale_local(scl);
}

Basis Basis::orthogonalized() const {
	Basis c = *this;
	c.orthogonalize();
	return c;
}

// Returns true if the basis vectors are orthogonal (perpendicular), so it has no skew or shear, and can be decomposed into rotation and scale.
// See https://en.wikipedia.org/wiki/Orthogonal_basis
bool Basis::is_orthogonal() const {
	const Vector3 x = get_column(0);
	const Vector3 y = get_column(1);
	const Vector3 z = get_column(2);
	return Math::is_zero_approx(x.dot(y)) && Math::is_zero_approx(x.dot(z)) && Math::is_zero_approx(y.dot(z));
}

// Returns true if the basis vectors are orthonormal (orthogonal and normalized), so it has no scale, skew, or shear.
// See https://en.wikipedia.org/wiki/Orthonormal_basis
bool Basis::is_orthonormal() const {
	const Vector3 x = get_column(0);
	const Vector3 y = get_column(1);
	const Vector3 z = get_column(2);
	return Math::is_equal_approx(x.length_squared(), 1) && Math::is_equal_approx(y.length_squared(), 1) && Math::is_equal_approx(z.length_squared(), 1) && Math::is_zero_approx(x.dot(y)) && Math::is_zero_approx(x.dot(z)) && Math::is_zero_approx(y.dot(z));
}

// Returns true if the basis is conformal (orthogonal, uniform scale, preserves angles and distance ratios).
// See https://en.wikipedia.org/wiki/Conformal_linear_transformation
bool Basis::is_conformal() const {
	const Vector3 x = get_column(0);
	const Vector3 y = get_column(1);
	const Vector3 z = get_column(2);
	const real_t x_len_sq = x.length_squared();
	return Math::is_equal_approx(x_len_sq, y.length_squared()) && Math::is_equal_approx(x_len_sq, z.length_squared()) && Math::is_zero_approx(x.dot(y)) && Math::is_zero_approx(x.dot(z)) && Math::is_zero_approx(y.dot(z));
}

// Returns true if the basis only has diagonal elements, so it may only have scale or flip, but no rotation, skew, or shear.
bool Basis::is_diagonal() const {
	return (
			Math::is_zero_approx(rows[0][1]) && Math::is_zero_approx(rows[0][2]) &&
			Math::is_zero_approx(rows[1][0]) && Math::is_zero_approx(rows[1][2]) &&
			Math::is_zero_approx(rows[2][0]) && Math::is_zero_approx(rows[2][1]));
}

// Returns true if the basis is a pure rotation matrix, so it has no scale, skew, shear, or flip.
bool Basis::is_rotation() const {
	return is_conformal() && Math::is_equal_approx(determinant(), 1, (real_t)UNIT_EPSILON);
}

#ifdef MATH_CHECKS
// This method is only used once, in diagonalize. If it's desired elsewhere, feel free to remove the #ifdef.
bool Basis::is_symmetric() const {
	if (!Math::is_equal_approx(rows[0][1], rows[1][0])) {
		return false;
	}
	if (!Math::is_equal_approx(rows[0][2], rows[2][0])) {
		return false;
	}
	if (!Math::is_equal_approx(rows[1][2], rows[2][1])) {
		return false;
	}

	return true;
}
#endif

Basis Basis::diagonalize() {
// NOTE: only implemented for symmetric matrices
// with the Jacobi iterative method
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V(!is_symmetric(), Basis());
#endif
	const int ite_max = 1024;

	real_t off_matrix_norm_2 = rows[0][1] * rows[0][1] + rows[0][2] * rows[0][2] + rows[1][2] * rows[1][2];

	int ite = 0;
	Basis acc_rot;
	while (off_matrix_norm_2 > (real_t)CMP_EPSILON2 && ite++ < ite_max) {
		real_t el01_2 = rows[0][1] * rows[0][1];
		real_t el02_2 = rows[0][2] * rows[0][2];
		real_t el12_2 = rows[1][2] * rows[1][2];
		// Find the pivot element
		int i, j;
		if (el01_2 > el02_2) {
			if (el12_2 > el01_2) {
				i = 1;
				j = 2;
			} else {
				i = 0;
				j = 1;
			}
		} else {
			if (el12_2 > el02_2) {
				i = 1;
				j = 2;
			} else {
				i = 0;
				j = 2;
			}
		}

		// Compute the rotation angle
		real_t angle;
		if (Math::is_equal_approx(rows[j][j], rows[i][i])) {
			angle = Math_PI / 4;
		} else {
			angle = 0.5f * Math::atan(2 * rows[i][j] / (rows[j][j] - rows[i][i]));
		}

		// Compute the rotation matrix
		Basis rot;
		rot.rows[i][i] = rot.rows[j][j] = Math::cos(angle);
		rot.rows[i][j] = -(rot.rows[j][i] = Math::sin(angle));

		// Update the off matrix norm
		off_matrix_norm_2 -= rows[i][j] * rows[i][j];

		// Apply the rotation
		*this = rot * *this * rot.transposed();
		acc_rot = rot * acc_rot;
	}

	return acc_rot;
}

Basis Basis::inverse() const {
	Basis inv = *this;
	inv.invert();
	return inv;
}

void Basis::transpose() {
	SWAP(rows[0][1], rows[1][0]);
	SWAP(rows[0][2], rows[2][0]);
	SWAP(rows[1][2], rows[2][1]);
}

Basis Basis::transposed() const {
	Basis tr = *this;
	tr.transpose();
	return tr;
}

Basis Basis::from_scale(const Vector3 &p_scale) {
	return Basis(p_scale.x, 0, 0, 0, p_scale.y, 0, 0, 0, p_scale.z);
}

// Multiplies the matrix from left by the scaling matrix: M -> S.M
// See the comment for Basis::rotated for further explanation.
void Basis::scale(const Vector3 &p_scale) {
	rows[0][0] *= p_scale.x;
	rows[0][1] *= p_scale.x;
	rows[0][2] *= p_scale.x;
	rows[1][0] *= p_scale.y;
	rows[1][1] *= p_scale.y;
	rows[1][2] *= p_scale.y;
	rows[2][0] *= p_scale.z;
	rows[2][1] *= p_scale.z;
	rows[2][2] *= p_scale.z;
}

Basis Basis::scaled(const Vector3 &p_scale) const {
	Basis m = *this;
	m.scale(p_scale);
	return m;
}

void Basis::scale_local(const Vector3 &p_scale) {
	// performs a scaling in object-local coordinate system:
	// M -> (M.S.Minv).M = M.S.
	*this = scaled_local(p_scale);
}

void Basis::scale_orthogonal(const Vector3 &p_scale) {
	*this = scaled_orthogonal(p_scale);
}

Basis Basis::scaled_orthogonal(const Vector3 &p_scale) const {
	Basis m = *this;
	Vector3 s = Vector3(-1, -1, -1) + p_scale;
	bool sign = signbit(s.x + s.y + s.z);
	Basis b = m.orthonormalized();
	s = b.xform_inv(s);
	Vector3 dots;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			dots[j] += s[i] * abs(m.get_column(i).normalized().dot(b.get_column(j)));
		}
	}
	if (sign != signbit(dots.x + dots.y + dots.z)) {
		dots = -dots;
	}
	m.scale_local(Vector3(1, 1, 1) + dots);
	return m;
}

real_t Basis::get_uniform_scale() const {
	return (rows[0].length() + rows[1].length() + rows[2].length()) / 3.0f;
}

Basis Basis::scaled_local(const Vector3 &p_scale) const {
	return (*this) * Basis::from_scale(p_scale);
}

Vector3 Basis::get_scale_abs() const {
	return Vector3(
			Vector3(rows[0][0], rows[1][0], rows[2][0]).length(),
			Vector3(rows[0][1], rows[1][1], rows[2][1]).length(),
			Vector3(rows[0][2], rows[1][2], rows[2][2]).length());
}

Vector3 Basis::get_scale_global() const {
	real_t det_sign = SIGN(determinant());
	return det_sign * Vector3(rows[0].length(), rows[1].length(), rows[2].length());
}

// get_scale works with get_rotation, use get_scale_abs if you need to enforce positive signature.
Vector3 Basis::get_scale() const {
	// FIXME: We are assuming M = R.S (R is rotation and S is scaling), and use polar decomposition to extract R and S.
	// A polar decomposition is M = O.P, where O is an orthogonal matrix (meaning rotation and reflection) and
	// P is a positive semi-definite matrix (meaning it contains absolute values of scaling along its diagonal).
	//
	// Despite being different from what we want to achieve, we can nevertheless make use of polar decomposition
	// here as follows. We can split O into a rotation and a reflection as O = R.Q, and obtain M = R.S where
	// we defined S = Q.P. Now, R is a proper rotation matrix and S is a (signed) scaling matrix,
	// which can involve negative scalings. However, there is a catch: unlike the polar decomposition of M = O.P,
	// the decomposition of O into a rotation and reflection matrix as O = R.Q is not unique.
	// Therefore, we are going to do this decomposition by sticking to a particular convention.
	// This may lead to confusion for some users though.
	//
	// The convention we use here is to absorb the sign flip into the scaling matrix.
	// The same convention is also used in other similar functions such as get_rotation_axis_angle, get_rotation, ...
	//
	// A proper way to get rid of this issue would be to store the scaling values (or at least their signs)
	// as a part of Basis. However, if we go that path, we need to disable direct (write) access to the
	// matrix elements.
	//
	// The rotation part of this decomposition is returned by get_rotation* functions.
	real_t det_sign = SIGN(determinant());
	return det_sign * get_scale_abs();
}

// Decomposes a Basis into a rotation-reflection matrix (an element of the group O(3)) and a positive scaling matrix as B = O.S.
// Returns the rotation-reflection matrix via reference argument, and scaling information is returned as a Vector3.
// This (internal) function is too specific and named too ugly to expose to users, and probably there's no need to do so.
Vector3 Basis::rotref_posscale_decomposition(Basis &rotref) const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V(determinant() == 0, Vector3());

	Basis m = transposed() * (*this);
	ERR_FAIL_COND_V(!m.is_diagonal(), Vector3());
#endif
	Vector3 scale = get_scale();
	Basis inv_scale = Basis().scaled(scale.inverse()); // this will also absorb the sign of scale
	rotref = (*this) * inv_scale;

#ifdef MATH_CHECKS
	ERR_FAIL_COND_V(!rotref.is_orthogonal(), Vector3());
#endif
	return scale.abs();
}

// Multiplies the matrix from left by the rotation matrix: M -> R.M
// Note that this does *not* rotate the matrix itself.
//
// The main use of Basis is as Transform.basis, which is used by the transformation matrix
// of 3D object. Rotate here refers to rotation of the object (which is R * (*this)),
// not the matrix itself (which is R * (*this) * R.transposed()).
Basis Basis::rotated(const Vector3 &p_axis, real_t p_angle) const {
	return Basis(p_axis, p_angle) * (*this);
}

void Basis::rotate(const Vector3 &p_axis, real_t p_angle) {
	*this = rotated(p_axis, p_angle);
}

void Basis::rotate_local(const Vector3 &p_axis, real_t p_angle) {
	// performs a rotation in object-local coordinate system:
	// M -> (M.R.Minv).M = M.R.
	*this = rotated_local(p_axis, p_angle);
}

Basis Basis::rotated_local(const Vector3 &p_axis, real_t p_angle) const {
	return (*this) * Basis(p_axis, p_angle);
}

Basis Basis::rotated(const Vector3 &p_euler, EulerOrder p_order) const {
	return Basis::from_euler(p_euler, p_order) * (*this);
}

void Basis::rotate(const Vector3 &p_euler, EulerOrder p_order) {
	*this = rotated(p_euler, p_order);
}

Basis Basis::rotated(const Quaternion &p_quaternion) const {
	return Basis(p_quaternion) * (*this);
}

void Basis::rotate(const Quaternion &p_quaternion) {
	*this = rotated(p_quaternion);
}

Vector3 Basis::get_euler_normalized(EulerOrder p_order) const {
	// Assumes that the matrix can be decomposed into a proper rotation and scaling matrix as M = R.S,
	// and returns the Euler angles corresponding to the rotation part, complementing get_scale().
	// See the comment in get_scale() for further information.
	Basis m = orthonormalized();
	real_t det = m.determinant();
	if (det < 0) {
		// Ensure that the determinant is 1, such that result is a proper rotation matrix which can be represented by Euler angles.
		m.scale(Vector3(-1, -1, -1));
	}

	return m.get_euler(p_order);
}

Quaternion Basis::get_rotation_quaternion() const {
	// Assumes that the matrix can be decomposed into a proper rotation and scaling matrix as M = R.S,
	// and returns the Euler angles corresponding to the rotation part, complementing get_scale().
	// See the comment in get_scale() for further information.
	Basis m = orthonormalized();
	real_t det = m.determinant();
	if (det < 0) {
		// Ensure that the determinant is 1, such that result is a proper rotation matrix which can be represented by Euler angles.
		m.scale(Vector3(-1, -1, -1));
	}

	return m.get_quaternion();
}

void Basis::rotate_to_align(Vector3 p_start_direction, Vector3 p_end_direction) {
	// Takes two vectors and rotates the basis from the first vector to the second vector.
	// Adopted from: https://gist.github.com/kevinmoran/b45980723e53edeb8a5a43c49f134724
	const Vector3 axis = p_start_direction.cross(p_end_direction).normalized();
	if (axis.length_squared() != 0) {
		real_t dot = p_start_direction.dot(p_end_direction);
		dot = CLAMP(dot, -1.0f, 1.0f);
		const real_t angle_rads = Math::acos(dot);
		*this = Basis(axis, angle_rads) * (*this);
	}
}

void Basis::get_rotation_axis_angle(Vector3 &p_axis, real_t &p_angle) const {
	// Assumes that the matrix can be decomposed into a proper rotation and scaling matrix as M = R.S,
	// and returns the Euler angles corresponding to the rotation part, complementing get_scale().
	// See the comment in get_scale() for further information.
	Basis m = orthonormalized();
	real_t det = m.determinant();
	if (det < 0) {
		// Ensure that the determinant is 1, such that result is a proper rotation matrix which can be represented by Euler angles.
		m.scale(Vector3(-1, -1, -1));
	}

	m.get_axis_angle(p_axis, p_angle);
}

void Basis::get_rotation_axis_angle_local(Vector3 &p_axis, real_t &p_angle) const {
	// Assumes that the matrix can be decomposed into a proper rotation and scaling matrix as M = R.S,
	// and returns the Euler angles corresponding to the rotation part, complementing get_scale().
	// See the comment in get_scale() for further information.
	Basis m = transposed();
	m.orthonormalize();
	real_t det = m.determinant();
	if (det < 0) {
		// Ensure that the determinant is 1, such that result is a proper rotation matrix which can be represented by Euler angles.
		m.scale(Vector3(-1, -1, -1));
	}

	m.get_axis_angle(p_axis, p_angle);
	p_angle = -p_angle;
}

Vector3 Basis::get_euler(EulerOrder p_order) const {
	// This epsilon value results in angles within a +/- 0.04 degree range being simplified/truncated.
	// Based on testing, this is the largest the epsilon can be without the angle truncation becoming
	// visually noticeable.
	const real_t epsilon = 0.00000025;

	switch (p_order) {
		case EulerOrder::XYZ: {
			// Euler angles in XYZ convention.
			// See https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
			//
			// rot =  cy*cz          -cy*sz           sy
			//        cz*sx*sy+cx*sz  cx*cz-sx*sy*sz -cy*sx
			//       -cx*cz*sy+sx*sz  cz*sx+cx*sy*sz  cx*cy

			Vector3 euler;
			real_t sy = rows[0][2];
			if (sy < (1.0f - epsilon)) {
				if (sy > -(1.0f - epsilon)) {
					// is this a pure Y rotation?
					if (rows[1][0] == 0 && rows[0][1] == 0 && rows[1][2] == 0 && rows[2][1] == 0 && rows[1][1] == 1) {
						// return the simplest form (human friendlier in editor and scripts)
						euler.x = 0;
						euler.y = atan2(rows[0][2], rows[0][0]);
						euler.z = 0;
					} else {
						euler.x = Math::atan2(-rows[1][2], rows[2][2]);
						euler.y = Math::asin(sy);
						euler.z = Math::atan2(-rows[0][1], rows[0][0]);
					}
				} else {
					euler.x = Math::atan2(rows[2][1], rows[1][1]);
					euler.y = -Math_PI / 2.0f;
					euler.z = 0.0f;
				}
			} else {
				euler.x = Math::atan2(rows[2][1], rows[1][1]);
				euler.y = Math_PI / 2.0f;
				euler.z = 0.0f;
			}
			return euler;
		}
		case EulerOrder::XZY: {
			// Euler angles in XZY convention.
			// See https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
			//
			// rot =  cz*cy             -sz             cz*sy
			//        sx*sy+cx*cy*sz    cx*cz           cx*sz*sy-cy*sx
			//        cy*sx*sz          cz*sx           cx*cy+sx*sz*sy

			Vector3 euler;
			real_t sz = rows[0][1];
			if (sz < (1.0f - epsilon)) {
				if (sz > -(1.0f - epsilon)) {
					euler.x = Math::atan2(rows[2][1], rows[1][1]);
					euler.y = Math::atan2(rows[0][2], rows[0][0]);
					euler.z = Math::asin(-sz);
				} else {
					// It's -1
					euler.x = -Math::atan2(rows[1][2], rows[2][2]);
					euler.y = 0.0f;
					euler.z = Math_PI / 2.0f;
				}
			} else {
				// It's 1
				euler.x = -Math::atan2(rows[1][2], rows[2][2]);
				euler.y = 0.0f;
				euler.z = -Math_PI / 2.0f;
			}
			return euler;
		}
		case EulerOrder::YXZ: {
			// Euler angles in YXZ convention.
			// See https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
			//
			// rot =  cy*cz+sy*sx*sz    cz*sy*sx-cy*sz        cx*sy
			//        cx*sz             cx*cz                 -sx
			//        cy*sx*sz-cz*sy    cy*cz*sx+sy*sz        cy*cx

			Vector3 euler;

			real_t m12 = rows[1][2];

			if (m12 < (1 - epsilon)) {
				if (m12 > -(1 - epsilon)) {
					// is this a pure X rotation?
					if (rows[1][0] == 0 && rows[0][1] == 0 && rows[0][2] == 0 && rows[2][0] == 0 && rows[0][0] == 1) {
						// return the simplest form (human friendlier in editor and scripts)
						euler.x = atan2(-m12, rows[1][1]);
						euler.y = 0;
						euler.z = 0;
					} else {
						euler.x = asin(-m12);
						euler.y = atan2(rows[0][2], rows[2][2]);
						euler.z = atan2(rows[1][0], rows[1][1]);
					}
				} else { // m12 == -1
					euler.x = Math_PI * 0.5f;
					euler.y = atan2(rows[0][1], rows[0][0]);
					euler.z = 0;
				}
			} else { // m12 == 1
				euler.x = -Math_PI * 0.5f;
				euler.y = -atan2(rows[0][1], rows[0][0]);
				euler.z = 0;
			}

			return euler;
		}
		case EulerOrder::YZX: {
			// Euler angles in YZX convention.
			// See https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
			//
			// rot =  cy*cz             sy*sx-cy*cx*sz     cx*sy+cy*sz*sx
			//        sz                cz*cx              -cz*sx
			//        -cz*sy            cy*sx+cx*sy*sz     cy*cx-sy*sz*sx

			Vector3 euler;
			real_t sz = rows[1][0];
			if (sz < (1.0f - epsilon)) {
				if (sz > -(1.0f - epsilon)) {
					euler.x = Math::atan2(-rows[1][2], rows[1][1]);
					euler.y = Math::atan2(-rows[2][0], rows[0][0]);
					euler.z = Math::asin(sz);
				} else {
					// It's -1
					euler.x = Math::atan2(rows[2][1], rows[2][2]);
					euler.y = 0.0f;
					euler.z = -Math_PI / 2.0f;
				}
			} else {
				// It's 1
				euler.x = Math::atan2(rows[2][1], rows[2][2]);
				euler.y = 0.0f;
				euler.z = Math_PI / 2.0f;
			}
			return euler;
		} break;
		case EulerOrder::ZXY: {
			// Euler angles in ZXY convention.
			// See https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
			//
			// rot =  cz*cy-sz*sx*sy    -cx*sz                cz*sy+cy*sz*sx
			//        cy*sz+cz*sx*sy    cz*cx                 sz*sy-cz*cy*sx
			//        -cx*sy            sx                    cx*cy
			Vector3 euler;
			real_t sx = rows[2][1];
			if (sx < (1.0f - epsilon)) {
				if (sx > -(1.0f - epsilon)) {
					euler.x = Math::asin(sx);
					euler.y = Math::atan2(-rows[2][0], rows[2][2]);
					euler.z = Math::atan2(-rows[0][1], rows[1][1]);
				} else {
					// It's -1
					euler.x = -Math_PI / 2.0f;
					euler.y = Math::atan2(rows[0][2], rows[0][0]);
					euler.z = 0;
				}
			} else {
				// It's 1
				euler.x = Math_PI / 2.0f;
				euler.y = Math::atan2(rows[0][2], rows[0][0]);
				euler.z = 0;
			}
			return euler;
		} break;
		case EulerOrder::ZYX: {
			// Euler angles in ZYX convention.
			// See https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
			//
			// rot =  cz*cy             cz*sy*sx-cx*sz        sz*sx+cz*cx*cy
			//        cy*sz             cz*cx+sz*sy*sx        cx*sz*sy-cz*sx
			//        -sy               cy*sx                 cy*cx
			Vector3 euler;
			real_t sy = rows[2][0];
			if (sy < (1.0f - epsilon)) {
				if (sy > -(1.0f - epsilon)) {
					euler.x = Math::atan2(rows[2][1], rows[2][2]);
					euler.y = Math::asin(-sy);
					euler.z = Math::atan2(rows[1][0], rows[0][0]);
				} else {
					// It's -1
					euler.x = 0;
					euler.y = Math_PI / 2.0f;
					euler.z = -Math::atan2(rows[0][1], rows[1][1]);
				}
			} else {
				// It's 1
				euler.x = 0;
				euler.y = -Math_PI / 2.0f;
				euler.z = -Math::atan2(rows[0][1], rows[1][1]);
			}
			return euler;
		}
		default: {
			ERR_FAIL_V_MSG(Vector3(), "Invalid parameter for get_euler(order)");
		}
	}
	return Vector3();
}

void Basis::set_euler(const Vector3 &p_euler, EulerOrder p_order) {
	real_t c, s;

	c = Math::cos(p_euler.x);
	s = Math::sin(p_euler.x);
	Basis xmat(1, 0, 0, 0, c, -s, 0, s, c);

	c = Math::cos(p_euler.y);
	s = Math::sin(p_euler.y);
	Basis ymat(c, 0, s, 0, 1, 0, -s, 0, c);

	c = Math::cos(p_euler.z);
	s = Math::sin(p_euler.z);
	Basis zmat(c, -s, 0, s, c, 0, 0, 0, 1);

	switch (p_order) {
		case EulerOrder::XYZ: {
			*this = xmat * (ymat * zmat);
		} break;
		case EulerOrder::XZY: {
			*this = xmat * zmat * ymat;
		} break;
		case EulerOrder::YXZ: {
			*this = ymat * xmat * zmat;
		} break;
		case EulerOrder::YZX: {
			*this = ymat * zmat * xmat;
		} break;
		case EulerOrder::ZXY: {
			*this = zmat * xmat * ymat;
		} break;
		case EulerOrder::ZYX: {
			*this = zmat * ymat * xmat;
		} break;
		default: {
			ERR_FAIL_MSG("Invalid Euler order parameter.");
		}
	}
}

bool Basis::is_equal_approx(const Basis &p_basis) const {
	return rows[0].is_equal_approx(p_basis.rows[0]) && rows[1].is_equal_approx(p_basis.rows[1]) && rows[2].is_equal_approx(p_basis.rows[2]);
}

bool Basis::is_same(const Basis &p_basis) const {
	return rows[0].is_same(p_basis.rows[0]) && rows[1].is_same(p_basis.rows[1]) && rows[2].is_same(p_basis.rows[2]);
}

bool Basis::is_finite() const {
	return rows[0].is_finite() && rows[1].is_finite() && rows[2].is_finite();
}

bool Basis::operator==(const Basis &p_matrix) const {
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (rows[i][j] != p_matrix.rows[i][j]) {
				return false;
			}
		}
	}

	return true;
}

bool Basis::operator!=(const Basis &p_matrix) const {
	return (!(*this == p_matrix));
}

Basis::operator String() const {
	return "[X: " + get_column(0).operator String() +
			", Y: " + get_column(1).operator String() +
			", Z: " + get_column(2).operator String() + "]";
}

Quaternion Basis::get_quaternion() const {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!is_rotation(), Quaternion(), "Basis " + operator String() + " must be normalized in order to be casted to a Quaternion. Use get_rotation_quaternion() or call orthonormalized() if the Basis contains linearly independent vectors.");
#endif
	/* Allow getting a quaternion from an unnormalized transform */
	Basis m = *this;
	real_t trace = m.rows[0][0] + m.rows[1][1] + m.rows[2][2];
	real_t temp[4];

	if (trace > 0.0f) {
		real_t s = Math::sqrt(trace + 1.0f);
		temp[3] = (s * 0.5f);
		s = 0.5f / s;

		temp[0] = ((m.rows[2][1] - m.rows[1][2]) * s);
		temp[1] = ((m.rows[0][2] - m.rows[2][0]) * s);
		temp[2] = ((m.rows[1][0] - m.rows[0][1]) * s);
	} else {
		int i = m.rows[0][0] < m.rows[1][1]
				? (m.rows[1][1] < m.rows[2][2] ? 2 : 1)
				: (m.rows[0][0] < m.rows[2][2] ? 2 : 0);
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;

		real_t s = Math::sqrt(m.rows[i][i] - m.rows[j][j] - m.rows[k][k] + 1.0f);
		temp[i] = s * 0.5f;
		s = 0.5f / s;

		temp[3] = (m.rows[k][j] - m.rows[j][k]) * s;
		temp[j] = (m.rows[j][i] + m.rows[i][j]) * s;
		temp[k] = (m.rows[k][i] + m.rows[i][k]) * s;
	}

	return Quaternion(temp[0], temp[1], temp[2], temp[3]);
}

void Basis::get_axis_angle(Vector3 &r_axis, real_t &r_angle) const {
	/* checking this is a bad idea, because obtaining from scaled transform is a valid use case
#ifdef MATH_CHECKS
	ERR_FAIL_COND(!is_rotation());
#endif
	*/

	// https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToAngle/index.htm
	real_t x, y, z; // Variables for result.
	if (Math::is_zero_approx(rows[0][1] - rows[1][0]) && Math::is_zero_approx(rows[0][2] - rows[2][0]) && Math::is_zero_approx(rows[1][2] - rows[2][1])) {
		// Singularity found.
		// First check for identity matrix which must have +1 for all terms in leading diagonal and zero in other terms.
		if (is_diagonal() && (Math::abs(rows[0][0] + rows[1][1] + rows[2][2] - 3) < 3 * CMP_EPSILON)) {
			// This singularity is identity matrix so angle = 0.
			r_axis = Vector3(0, 1, 0);
			r_angle = 0;
			return;
		}
		// Otherwise this singularity is angle = 180.
		real_t xx = (rows[0][0] + 1) / 2;
		real_t yy = (rows[1][1] + 1) / 2;
		real_t zz = (rows[2][2] + 1) / 2;
		real_t xy = (rows[0][1] + rows[1][0]) / 4;
		real_t xz = (rows[0][2] + rows[2][0]) / 4;
		real_t yz = (rows[1][2] + rows[2][1]) / 4;

		if ((xx > yy) && (xx > zz)) { // rows[0][0] is the largest diagonal term.
			if (xx < CMP_EPSILON) {
				x = 0;
				y = Math_SQRT12;
				z = Math_SQRT12;
			} else {
				x = Math::sqrt(xx);
				y = xy / x;
				z = xz / x;
			}
		} else if (yy > zz) { // rows[1][1] is the largest diagonal term.
			if (yy < CMP_EPSILON) {
				x = Math_SQRT12;
				y = 0;
				z = Math_SQRT12;
			} else {
				y = Math::sqrt(yy);
				x = xy / y;
				z = yz / y;
			}
		} else { // rows[2][2] is the largest diagonal term so base result on this.
			if (zz < CMP_EPSILON) {
				x = Math_SQRT12;
				y = Math_SQRT12;
				z = 0;
			} else {
				z = Math::sqrt(zz);
				x = xz / z;
				y = yz / z;
			}
		}
		r_axis = Vector3(x, y, z);
		r_angle = Math_PI;
		return;
	}
	// As we have reached here there are no singularities so we can handle normally.
	double s = Math::sqrt((rows[2][1] - rows[1][2]) * (rows[2][1] - rows[1][2]) + (rows[0][2] - rows[2][0]) * (rows[0][2] - rows[2][0]) + (rows[1][0] - rows[0][1]) * (rows[1][0] - rows[0][1])); // Used to normalize.

	if (Math::abs(s) < CMP_EPSILON) {
		// Prevent divide by zero, should not happen if matrix is orthogonal and should be caught by singularity test above.
		s = 1;
	}

	x = (rows[2][1] - rows[1][2]) / s;
	y = (rows[0][2] - rows[2][0]) / s;
	z = (rows[1][0] - rows[0][1]) / s;

	r_axis = Vector3(x, y, z);
	// acos does clamping.
	r_angle = Math::acos((rows[0][0] + rows[1][1] + rows[2][2] - 1) / 2);
}

void Basis::set_quaternion(const Quaternion &p_quaternion) {
	real_t d = p_quaternion.length_squared();
	real_t s = 2.0f / d;
	real_t xs = p_quaternion.x * s, ys = p_quaternion.y * s, zs = p_quaternion.z * s;
	real_t wx = p_quaternion.w * xs, wy = p_quaternion.w * ys, wz = p_quaternion.w * zs;
	real_t xx = p_quaternion.x * xs, xy = p_quaternion.x * ys, xz = p_quaternion.x * zs;
	real_t yy = p_quaternion.y * ys, yz = p_quaternion.y * zs, zz = p_quaternion.z * zs;
	set(1.0f - (yy + zz), xy - wz, xz + wy,
			xy + wz, 1.0f - (xx + zz), yz - wx,
			xz - wy, yz + wx, 1.0f - (xx + yy));
}

void Basis::set_axis_angle(const Vector3 &p_axis, real_t p_angle) {
// Rotation matrix from axis and angle, see https://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_angle
#ifdef MATH_CHECKS
	ERR_FAIL_COND_MSG(!p_axis.is_normalized(), "The axis Vector3 " + p_axis.operator String() + " must be normalized.");
#endif
	Vector3 axis_sq(p_axis.x * p_axis.x, p_axis.y * p_axis.y, p_axis.z * p_axis.z);
	real_t cosine = Math::cos(p_angle);
	rows[0][0] = axis_sq.x + cosine * (1.0f - axis_sq.x);
	rows[1][1] = axis_sq.y + cosine * (1.0f - axis_sq.y);
	rows[2][2] = axis_sq.z + cosine * (1.0f - axis_sq.z);

	real_t sine = Math::sin(p_angle);
	real_t t = 1 - cosine;

	real_t xyzt = p_axis.x * p_axis.y * t;
	real_t zyxs = p_axis.z * sine;
	rows[0][1] = xyzt - zyxs;
	rows[1][0] = xyzt + zyxs;

	xyzt = p_axis.x * p_axis.z * t;
	zyxs = p_axis.y * sine;
	rows[0][2] = xyzt + zyxs;
	rows[2][0] = xyzt - zyxs;

	xyzt = p_axis.y * p_axis.z * t;
	zyxs = p_axis.x * sine;
	rows[1][2] = xyzt - zyxs;
	rows[2][1] = xyzt + zyxs;
}

void Basis::set_axis_angle_scale(const Vector3 &p_axis, real_t p_angle, const Vector3 &p_scale) {
	_set_diagonal(p_scale);
	rotate(p_axis, p_angle);
}

void Basis::set_euler_scale(const Vector3 &p_euler, const Vector3 &p_scale, EulerOrder p_order) {
	_set_diagonal(p_scale);
	rotate(p_euler, p_order);
}

void Basis::set_quaternion_scale(const Quaternion &p_quaternion, const Vector3 &p_scale) {
	_set_diagonal(p_scale);
	rotate(p_quaternion);
}

// This also sets the non-diagonal elements to 0, which is misleading from the
// name, so we want this method to be private. Use `from_scale` externally.
void Basis::_set_diagonal(const Vector3 &p_diag) {
	rows[0][0] = p_diag.x;
	rows[0][1] = 0;
	rows[0][2] = 0;

	rows[1][0] = 0;
	rows[1][1] = p_diag.y;
	rows[1][2] = 0;

	rows[2][0] = 0;
	rows[2][1] = 0;
	rows[2][2] = p_diag.z;
}

Basis Basis::lerp(const Basis &p_to, real_t p_weight) const {
	Basis b;
	b.rows[0] = rows[0].lerp(p_to.rows[0], p_weight);
	b.rows[1] = rows[1].lerp(p_to.rows[1], p_weight);
	b.rows[2] = rows[2].lerp(p_to.rows[2], p_weight);

	return b;
}

Basis Basis::slerp(const Basis &p_to, real_t p_weight) const {
	//consider scale
	Quaternion from(*this);
	Quaternion to(p_to);

	Basis b(from.slerp(to, p_weight));
	b.rows[0] *= Math::lerp(rows[0].length(), p_to.rows[0].length(), p_weight);
	b.rows[1] *= Math::lerp(rows[1].length(), p_to.rows[1].length(), p_weight);
	b.rows[2] *= Math::lerp(rows[2].length(), p_to.rows[2].length(), p_weight);

	return b;
}

void Basis::rotate_sh(real_t *p_values) {
	// code by John Hable
	// http://filmicworlds.com/blog/simple-and-fast-spherical-harmonic-rotation/
	// this code is Public Domain

	const static real_t s_c3 = 0.94617469575; // (3*sqrt(5))/(4*sqrt(pi))
	const static real_t s_c4 = -0.31539156525; // (-sqrt(5))/(4*sqrt(pi))
	const static real_t s_c5 = 0.54627421529; // (sqrt(15))/(4*sqrt(pi))

	const static real_t s_c_scale = 1.0 / 0.91529123286551084;
	const static real_t s_c_scale_inv = 0.91529123286551084;

	const static real_t s_rc2 = 1.5853309190550713 * s_c_scale;
	const static real_t s_c4_div_c3 = s_c4 / s_c3;
	const static real_t s_c4_div_c3_x2 = (s_c4 / s_c3) * 2.0;

	const static real_t s_scale_dst2 = s_c3 * s_c_scale_inv;
	const static real_t s_scale_dst4 = s_c5 * s_c_scale_inv;

	const real_t src[9] = { p_values[0], p_values[1], p_values[2], p_values[3], p_values[4], p_values[5], p_values[6], p_values[7], p_values[8] };

	real_t m00 = rows[0][0];
	real_t m01 = rows[0][1];
	real_t m02 = rows[0][2];
	real_t m10 = rows[1][0];
	real_t m11 = rows[1][1];
	real_t m12 = rows[1][2];
	real_t m20 = rows[2][0];
	real_t m21 = rows[2][1];
	real_t m22 = rows[2][2];

	p_values[0] = src[0];
	p_values[1] = m11 * src[1] - m12 * src[2] + m10 * src[3];
	p_values[2] = -m21 * src[1] + m22 * src[2] - m20 * src[3];
	p_values[3] = m01 * src[1] - m02 * src[2] + m00 * src[3];

	real_t sh0 = src[7] + src[8] + src[8] - src[5];
	real_t sh1 = src[4] + s_rc2 * src[6] + src[7] + src[8];
	real_t sh2 = src[4];
	real_t sh3 = -src[7];
	real_t sh4 = -src[5];

	// Rotations.  R0 and R1 just use the raw matrix columns
	real_t r2x = m00 + m01;
	real_t r2y = m10 + m11;
	real_t r2z = m20 + m21;

	real_t r3x = m00 + m02;
	real_t r3y = m10 + m12;
	real_t r3z = m20 + m22;

	real_t r4x = m01 + m02;
	real_t r4y = m11 + m12;
	real_t r4z = m21 + m22;

	// dense matrix multiplication one column at a time

	// column 0
	real_t sh0_x = sh0 * m00;
	real_t sh0_y = sh0 * m10;
	real_t d0 = sh0_x * m10;
	real_t d1 = sh0_y * m20;
	real_t d2 = sh0 * (m20 * m20 + s_c4_div_c3);
	real_t d3 = sh0_x * m20;
	real_t d4 = sh0_x * m00 - sh0_y * m10;

	// column 1
	real_t sh1_x = sh1 * m02;
	real_t sh1_y = sh1 * m12;
	d0 += sh1_x * m12;
	d1 += sh1_y * m22;
	d2 += sh1 * (m22 * m22 + s_c4_div_c3);
	d3 += sh1_x * m22;
	d4 += sh1_x * m02 - sh1_y * m12;

	// column 2
	real_t sh2_x = sh2 * r2x;
	real_t sh2_y = sh2 * r2y;
	d0 += sh2_x * r2y;
	d1 += sh2_y * r2z;
	d2 += sh2 * (r2z * r2z + s_c4_div_c3_x2);
	d3 += sh2_x * r2z;
	d4 += sh2_x * r2x - sh2_y * r2y;

	// column 3
	real_t sh3_x = sh3 * r3x;
	real_t sh3_y = sh3 * r3y;
	d0 += sh3_x * r3y;
	d1 += sh3_y * r3z;
	d2 += sh3 * (r3z * r3z + s_c4_div_c3_x2);
	d3 += sh3_x * r3z;
	d4 += sh3_x * r3x - sh3_y * r3y;

	// column 4
	real_t sh4_x = sh4 * r4x;
	real_t sh4_y = sh4 * r4y;
	d0 += sh4_x * r4y;
	d1 += sh4_y * r4z;
	d2 += sh4 * (r4z * r4z + s_c4_div_c3_x2);
	d3 += sh4_x * r4z;
	d4 += sh4_x * r4x - sh4_y * r4y;

	// extra multipliers
	p_values[4] = d0;
	p_values[5] = -d1;
	p_values[6] = d2 * s_scale_dst2;
	p_values[7] = -d3;
	p_values[8] = d4 * s_scale_dst4;
}

Basis Basis::looking_at(const Vector3 &p_target, const Vector3 &p_up, bool p_use_model_front) {
#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(p_target.is_zero_approx(), Basis(), "The target vector can't be zero.");
	ERR_FAIL_COND_V_MSG(p_up.is_zero_approx(), Basis(), "The up vector can't be zero.");
#endif
	Vector3 v_z = p_target.normalized();
	if (!p_use_model_front) {
		v_z = -v_z;
	}
	Vector3 v_x = p_up.cross(v_z);
	if (v_x.is_zero_approx()) {
		WARN_PRINT("Target and up vectors are colinear. This is not advised as it may cause unwanted rotation around local Z axis.");
		v_x = p_up.get_any_perpendicular(); // Vectors are almost parallel.
	}
	v_x.normalize();
	Vector3 v_y = v_z.cross(v_x);

	Basis basis;
	basis.set_columns(v_x, v_y, v_z);
	return basis;
}
