/**************************************************************************/
/*  transform_2d.h                                                        */
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

#include "core/math/math_funcs.h"
#include "core/math/rect2.h"
#include "core/math/vector2.h"
#include "core/templates/vector.h"

class String;

struct [[nodiscard]] Transform2D {
	// WARNING: The basis of Transform2D is stored differently from Basis.
	// In terms of columns array, the basis matrix looks like "on paper":
	// M = (columns[0][0] columns[1][0])
	//     (columns[0][1] columns[1][1])
	// This is such that the columns, which can be interpreted as basis vectors
	// of the coordinate system "painted" on the object, can be accessed as columns[i].
	// NOTE: This is the opposite of the indices in mathematical texts,
	// meaning: $M_{12}$ in a math book corresponds to columns[1][0] here.
	// This requires additional care when working with explicit indices.
	// See https://en.wikipedia.org/wiki/Row-_and_column-major_order for further reading.

	// WARNING: Be aware that unlike 3D code, 2D code uses a left-handed coordinate system:
	// Y-axis points down, and angle is measure from +X to +Y in a clockwise-fashion.

	Vector2 columns[3];

	_FORCE_INLINE_ real_t tdotx(const Vector2 &p_v) const { return columns[0][0] * p_v.x + columns[1][0] * p_v.y; }
	_FORCE_INLINE_ real_t tdoty(const Vector2 &p_v) const { return columns[0][1] * p_v.x + columns[1][1] * p_v.y; }

	const Vector2 &operator[](int p_idx) const { return columns[p_idx]; }
	Vector2 &operator[](int p_idx) { return columns[p_idx]; }

	void invert();
	Transform2D inverse() const;

	void affine_invert();
	Transform2D affine_inverse() const;

	void set_rotation(real_t p_rot);
	real_t get_rotation() const;
	real_t get_skew() const;
	void set_skew(real_t p_angle);
	_FORCE_INLINE_ void set_rotation_and_scale(real_t p_rot, const Size2 &p_scale);
	_FORCE_INLINE_ void set_rotation_scale_and_skew(real_t p_rot, const Size2 &p_scale, real_t p_skew);
	void rotate(real_t p_angle);

	void scale(const Size2 &p_scale);
	void scale_basis(const Size2 &p_scale);
	void translate_local(real_t p_tx, real_t p_ty);
	void translate_local(const Vector2 &p_translation);

	real_t determinant() const;

	Size2 get_scale() const;
	void set_scale(const Size2 &p_scale);

	_FORCE_INLINE_ const Vector2 &get_origin() const { return columns[2]; }
	_FORCE_INLINE_ void set_origin(const Vector2 &p_origin) { columns[2] = p_origin; }

	Transform2D scaled(const Size2 &p_scale) const;
	Transform2D scaled_local(const Size2 &p_scale) const;
	Transform2D translated(const Vector2 &p_offset) const;
	Transform2D translated_local(const Vector2 &p_offset) const;
	Transform2D rotated(real_t p_angle) const;
	Transform2D rotated_local(real_t p_angle) const;

	Transform2D untranslated() const;

	void orthonormalize();
	Transform2D orthonormalized() const;
	bool is_conformal() const;
	bool is_equal_approx(const Transform2D &p_transform) const;
	bool is_finite() const;

	Transform2D looking_at(const Vector2 &p_target) const;

	bool operator==(const Transform2D &p_transform) const;
	bool operator!=(const Transform2D &p_transform) const;

	void operator*=(const Transform2D &p_transform);
	Transform2D operator*(const Transform2D &p_transform) const;
	void operator*=(real_t p_val);
	Transform2D operator*(real_t p_val) const;
	void operator/=(real_t p_val);
	Transform2D operator/(real_t p_val) const;

	Transform2D interpolate_with(const Transform2D &p_transform, real_t p_c) const;

	_FORCE_INLINE_ Vector2 basis_xform(const Vector2 &p_vec) const;
	_FORCE_INLINE_ Vector2 basis_xform_inv(const Vector2 &p_vec) const;
	_FORCE_INLINE_ Vector2 xform(const Vector2 &p_vec) const;
	_FORCE_INLINE_ Vector2 xform_inv(const Vector2 &p_vec) const;
	_FORCE_INLINE_ Rect2 xform(const Rect2 &p_rect) const;
	_FORCE_INLINE_ Rect2 xform_inv(const Rect2 &p_rect) const;
	_FORCE_INLINE_ Vector<Vector2> xform(const Vector<Vector2> &p_array) const;
	_FORCE_INLINE_ Vector<Vector2> xform_inv(const Vector<Vector2> &p_array) const;

	operator String() const;

	Transform2D(real_t p_xx, real_t p_xy, real_t p_yx, real_t p_yy, real_t p_ox, real_t p_oy) {
		columns[0][0] = p_xx;
		columns[0][1] = p_xy;
		columns[1][0] = p_yx;
		columns[1][1] = p_yy;
		columns[2][0] = p_ox;
		columns[2][1] = p_oy;
	}

	Transform2D(const Vector2 &p_x, const Vector2 &p_y, const Vector2 &p_origin) {
		columns[0] = p_x;
		columns[1] = p_y;
		columns[2] = p_origin;
	}

	Transform2D(real_t p_rot, const Vector2 &p_pos);

	Transform2D(real_t p_rot, const Size2 &p_scale, real_t p_skew, const Vector2 &p_pos);

	Transform2D() {
		columns[0][0] = 1.0;
		columns[1][1] = 1.0;
	}
};

Vector2 Transform2D::basis_xform(const Vector2 &p_vec) const {
	return Vector2(
			tdotx(p_vec),
			tdoty(p_vec));
}

Vector2 Transform2D::basis_xform_inv(const Vector2 &p_vec) const {
	return Vector2(
			columns[0].dot(p_vec),
			columns[1].dot(p_vec));
}

Vector2 Transform2D::xform(const Vector2 &p_vec) const {
	return Vector2(
				   tdotx(p_vec),
				   tdoty(p_vec)) +
			columns[2];
}

Vector2 Transform2D::xform_inv(const Vector2 &p_vec) const {
	Vector2 v = p_vec - columns[2];

	return Vector2(
			columns[0].dot(v),
			columns[1].dot(v));
}

Rect2 Transform2D::xform(const Rect2 &p_rect) const {
	Vector2 x = columns[0] * p_rect.size.x;
	Vector2 y = columns[1] * p_rect.size.y;
	Vector2 pos = xform(p_rect.position);

	Rect2 new_rect;
	new_rect.position = pos;
	new_rect.expand_to(pos + x);
	new_rect.expand_to(pos + y);
	new_rect.expand_to(pos + x + y);
	return new_rect;
}

void Transform2D::set_rotation_and_scale(real_t p_rot, const Size2 &p_scale) {
	columns[0][0] = Math::cos(p_rot) * p_scale.x;
	columns[1][1] = Math::cos(p_rot) * p_scale.y;
	columns[1][0] = -Math::sin(p_rot) * p_scale.y;
	columns[0][1] = Math::sin(p_rot) * p_scale.x;
}

void Transform2D::set_rotation_scale_and_skew(real_t p_rot, const Size2 &p_scale, real_t p_skew) {
	columns[0][0] = Math::cos(p_rot) * p_scale.x;
	columns[1][1] = Math::cos(p_rot + p_skew) * p_scale.y;
	columns[1][0] = -Math::sin(p_rot + p_skew) * p_scale.y;
	columns[0][1] = Math::sin(p_rot) * p_scale.x;
}

Rect2 Transform2D::xform_inv(const Rect2 &p_rect) const {
	Vector2 ends[4] = {
		xform_inv(p_rect.position),
		xform_inv(Vector2(p_rect.position.x, p_rect.position.y + p_rect.size.y)),
		xform_inv(Vector2(p_rect.position.x + p_rect.size.x, p_rect.position.y + p_rect.size.y)),
		xform_inv(Vector2(p_rect.position.x + p_rect.size.x, p_rect.position.y))
	};

	Rect2 new_rect;
	new_rect.position = ends[0];
	new_rect.expand_to(ends[1]);
	new_rect.expand_to(ends[2]);
	new_rect.expand_to(ends[3]);

	return new_rect;
}

Vector<Vector2> Transform2D::xform(const Vector<Vector2> &p_array) const {
	Vector<Vector2> array;
	array.resize(p_array.size());

	const Vector2 *r = p_array.ptr();
	Vector2 *w = array.ptrw();

	for (int i = 0; i < p_array.size(); ++i) {
		w[i] = xform(r[i]);
	}
	return array;
}

Vector<Vector2> Transform2D::xform_inv(const Vector<Vector2> &p_array) const {
	Vector<Vector2> array;
	array.resize(p_array.size());

	const Vector2 *r = p_array.ptr();
	Vector2 *w = array.ptrw();

	for (int i = 0; i < p_array.size(); ++i) {
		w[i] = xform_inv(r[i]);
	}
	return array;
}
