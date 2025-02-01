/**************************************************************************/
/*  transform_3d.h                                                        */
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

#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/math/plane.h"
#include "core/templates/vector.h"

struct [[nodiscard]] Transform3D {
	Basis basis;
	Vector3 origin;

	void invert();
	Transform3D inverse() const;

	void affine_invert();
	Transform3D affine_inverse() const;

	Transform3D rotated(const Vector3 &p_axis, real_t p_angle) const;
	Transform3D rotated_local(const Vector3 &p_axis, real_t p_angle) const;

	void rotate(const Vector3 &p_axis, real_t p_angle);
	void rotate_basis(const Vector3 &p_axis, real_t p_angle);

	void set_look_at(const Vector3 &p_eye, const Vector3 &p_target, const Vector3 &p_up = Vector3(0, 1, 0), bool p_use_model_front = false);
	Transform3D looking_at(const Vector3 &p_target, const Vector3 &p_up = Vector3(0, 1, 0), bool p_use_model_front = false) const;

	void scale(const Vector3 &p_scale);
	Transform3D scaled(const Vector3 &p_scale) const;
	Transform3D scaled_local(const Vector3 &p_scale) const;
	void scale_basis(const Vector3 &p_scale);
	void translate_local(real_t p_tx, real_t p_ty, real_t p_tz);
	void translate_local(const Vector3 &p_translation);
	Transform3D translated(const Vector3 &p_translation) const;
	Transform3D translated_local(const Vector3 &p_translation) const;

	const Basis &get_basis() const { return basis; }
	void set_basis(const Basis &p_basis) { basis = p_basis; }

	const Vector3 &get_origin() const { return origin; }
	void set_origin(const Vector3 &p_origin) { origin = p_origin; }

	void orthonormalize();
	Transform3D orthonormalized() const;
	void orthogonalize();
	Transform3D orthogonalized() const;
	bool is_equal_approx(const Transform3D &p_transform) const;
	bool is_finite() const;

	bool operator==(const Transform3D &p_transform) const;
	bool operator!=(const Transform3D &p_transform) const;

	_FORCE_INLINE_ Vector3 xform(const Vector3 &p_vector) const;
	_FORCE_INLINE_ AABB xform(const AABB &p_aabb) const;
	_FORCE_INLINE_ Vector<Vector3> xform(const Vector<Vector3> &p_array) const;

	// NOTE: These are UNSAFE with non-uniform scaling, and will produce incorrect results.
	// They use the transpose.
	// For safe inverse transforms, xform by the affine_inverse.
	_FORCE_INLINE_ Vector3 xform_inv(const Vector3 &p_vector) const;
	_FORCE_INLINE_ AABB xform_inv(const AABB &p_aabb) const;
	_FORCE_INLINE_ Vector<Vector3> xform_inv(const Vector<Vector3> &p_array) const;

	// Safe with non-uniform scaling (uses affine_inverse).
	_FORCE_INLINE_ Plane xform(const Plane &p_plane) const;
	_FORCE_INLINE_ Plane xform_inv(const Plane &p_plane) const;

	// These fast versions use precomputed affine inverse, and should be used in bottleneck areas where
	// multiple planes are to be transformed.
	_FORCE_INLINE_ Plane xform_fast(const Plane &p_plane, const Basis &p_basis_inverse_transpose) const;
	static _FORCE_INLINE_ Plane xform_inv_fast(const Plane &p_plane, const Transform3D &p_inverse, const Basis &p_basis_transpose);

	void operator*=(const Transform3D &p_transform);
	Transform3D operator*(const Transform3D &p_transform) const;
	void operator*=(real_t p_val);
	Transform3D operator*(real_t p_val) const;
	void operator/=(real_t p_val);
	Transform3D operator/(real_t p_val) const;

	Transform3D interpolate_with(const Transform3D &p_transform, real_t p_c) const;

	_FORCE_INLINE_ Transform3D inverse_xform(const Transform3D &t) const {
		Vector3 v = t.origin - origin;
		return Transform3D(basis.transpose_xform(t.basis),
				basis.xform(v));
	}

	void set(real_t p_xx, real_t p_xy, real_t p_xz, real_t p_yx, real_t p_yy, real_t p_yz, real_t p_zx, real_t p_zy, real_t p_zz, real_t p_tx, real_t p_ty, real_t p_tz) {
		basis.set(p_xx, p_xy, p_xz, p_yx, p_yy, p_yz, p_zx, p_zy, p_zz);
		origin.x = p_tx;
		origin.y = p_ty;
		origin.z = p_tz;
	}

	operator String() const;

	Transform3D() {}
	Transform3D(const Basis &p_basis, const Vector3 &p_origin = Vector3());
	Transform3D(const Vector3 &p_x, const Vector3 &p_y, const Vector3 &p_z, const Vector3 &p_origin);
	Transform3D(real_t p_xx, real_t p_xy, real_t p_xz, real_t p_yx, real_t p_yy, real_t p_yz, real_t p_zx, real_t p_zy, real_t p_zz, real_t p_ox, real_t p_oy, real_t p_oz);
};

_FORCE_INLINE_ Vector3 Transform3D::xform(const Vector3 &p_vector) const {
	return Vector3(
			basis[0].dot(p_vector) + origin.x,
			basis[1].dot(p_vector) + origin.y,
			basis[2].dot(p_vector) + origin.z);
}

_FORCE_INLINE_ Vector3 Transform3D::xform_inv(const Vector3 &p_vector) const {
	Vector3 v = p_vector - origin;

	return Vector3(
			(basis.rows[0][0] * v.x) + (basis.rows[1][0] * v.y) + (basis.rows[2][0] * v.z),
			(basis.rows[0][1] * v.x) + (basis.rows[1][1] * v.y) + (basis.rows[2][1] * v.z),
			(basis.rows[0][2] * v.x) + (basis.rows[1][2] * v.y) + (basis.rows[2][2] * v.z));
}

// Neither the plane regular xform or xform_inv are particularly efficient,
// as they do a basis inverse. For xforming a large number
// of planes it is better to pre-calculate the inverse transpose basis once
// and reuse it for each plane, by using the 'fast' version of the functions.
_FORCE_INLINE_ Plane Transform3D::xform(const Plane &p_plane) const {
	Basis b = basis.inverse();
	b.transpose();
	return xform_fast(p_plane, b);
}

_FORCE_INLINE_ Plane Transform3D::xform_inv(const Plane &p_plane) const {
	Transform3D inv = affine_inverse();
	Basis basis_transpose = basis.transposed();
	return xform_inv_fast(p_plane, inv, basis_transpose);
}

_FORCE_INLINE_ AABB Transform3D::xform(const AABB &p_aabb) const {
	/* https://dev.theomader.com/transform-bounding-boxes/ */
	Vector3 min = p_aabb.position;
	Vector3 max = p_aabb.position + p_aabb.size;
	Vector3 tmin, tmax;
	for (int i = 0; i < 3; i++) {
		tmin[i] = tmax[i] = origin[i];
		for (int j = 0; j < 3; j++) {
			real_t e = basis[i][j] * min[j];
			real_t f = basis[i][j] * max[j];
			if (e < f) {
				tmin[i] += e;
				tmax[i] += f;
			} else {
				tmin[i] += f;
				tmax[i] += e;
			}
		}
	}
	AABB r_aabb;
	r_aabb.position = tmin;
	r_aabb.size = tmax - tmin;
	return r_aabb;
}

_FORCE_INLINE_ AABB Transform3D::xform_inv(const AABB &p_aabb) const {
	/* define vertices */
	Vector3 vertices[8] = {
		Vector3(p_aabb.position.x + p_aabb.size.x, p_aabb.position.y + p_aabb.size.y, p_aabb.position.z + p_aabb.size.z),
		Vector3(p_aabb.position.x + p_aabb.size.x, p_aabb.position.y + p_aabb.size.y, p_aabb.position.z),
		Vector3(p_aabb.position.x + p_aabb.size.x, p_aabb.position.y, p_aabb.position.z + p_aabb.size.z),
		Vector3(p_aabb.position.x + p_aabb.size.x, p_aabb.position.y, p_aabb.position.z),
		Vector3(p_aabb.position.x, p_aabb.position.y + p_aabb.size.y, p_aabb.position.z + p_aabb.size.z),
		Vector3(p_aabb.position.x, p_aabb.position.y + p_aabb.size.y, p_aabb.position.z),
		Vector3(p_aabb.position.x, p_aabb.position.y, p_aabb.position.z + p_aabb.size.z),
		Vector3(p_aabb.position.x, p_aabb.position.y, p_aabb.position.z)
	};

	AABB ret;

	ret.position = xform_inv(vertices[0]);

	for (int i = 1; i < 8; i++) {
		ret.expand_to(xform_inv(vertices[i]));
	}

	return ret;
}

Vector<Vector3> Transform3D::xform(const Vector<Vector3> &p_array) const {
	Vector<Vector3> array;
	array.resize(p_array.size());

	const Vector3 *r = p_array.ptr();
	Vector3 *w = array.ptrw();

	for (int i = 0; i < p_array.size(); ++i) {
		w[i] = xform(r[i]);
	}
	return array;
}

Vector<Vector3> Transform3D::xform_inv(const Vector<Vector3> &p_array) const {
	Vector<Vector3> array;
	array.resize(p_array.size());

	const Vector3 *r = p_array.ptr();
	Vector3 *w = array.ptrw();

	for (int i = 0; i < p_array.size(); ++i) {
		w[i] = xform_inv(r[i]);
	}
	return array;
}

_FORCE_INLINE_ Plane Transform3D::xform_fast(const Plane &p_plane, const Basis &p_basis_inverse_transpose) const {
	// Transform a single point on the plane.
	Vector3 point = p_plane.normal * p_plane.d;
	point = xform(point);

	// Use inverse transpose for correct normals with non-uniform scaling.
	Vector3 normal = p_basis_inverse_transpose.xform(p_plane.normal);
	normal.normalize();

	real_t d = normal.dot(point);
	return Plane(normal, d);
}

_FORCE_INLINE_ Plane Transform3D::xform_inv_fast(const Plane &p_plane, const Transform3D &p_inverse, const Basis &p_basis_transpose) {
	// Transform a single point on the plane.
	Vector3 point = p_plane.normal * p_plane.d;
	point = p_inverse.xform(point);

	// Note that instead of precalculating the transpose, an alternative
	// would be to use the transpose for the basis transform.
	// However that would be less SIMD friendly (requiring a swizzle).
	// So the cost is one extra precalced value in the calling code.
	// This is probably worth it, as this could be used in bottleneck areas. And
	// where it is not a bottleneck, the non-fast method is fine.

	// Use transpose for correct normals with non-uniform scaling.
	Vector3 normal = p_basis_transpose.xform(p_plane.normal);
	normal.normalize();

	real_t d = normal.dot(point);
	return Plane(normal, d);
}
