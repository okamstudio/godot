/**************************************************************************/
/*  projection.cpp                                                        */
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

#include "projection.h"

#include "core/math/aabb.h"
#include "core/math/math_funcs.h"
#include "core/math/plane.h"
#include "core/math/rect2.h"
#include "core/math/transform_3d.h"
#include "core/string/ustring.h"

real_t Projection::determinant() const {
	return columns[0][3] * columns[1][2] * columns[2][1] * columns[3][0] - columns[0][2] * columns[1][3] * columns[2][1] * columns[3][0] -
			columns[0][3] * columns[1][1] * columns[2][2] * columns[3][0] + columns[0][1] * columns[1][3] * columns[2][2] * columns[3][0] +
			columns[0][2] * columns[1][1] * columns[2][3] * columns[3][0] - columns[0][1] * columns[1][2] * columns[2][3] * columns[3][0] -
			columns[0][3] * columns[1][2] * columns[2][0] * columns[3][1] + columns[0][2] * columns[1][3] * columns[2][0] * columns[3][1] +
			columns[0][3] * columns[1][0] * columns[2][2] * columns[3][1] - columns[0][0] * columns[1][3] * columns[2][2] * columns[3][1] -
			columns[0][2] * columns[1][0] * columns[2][3] * columns[3][1] + columns[0][0] * columns[1][2] * columns[2][3] * columns[3][1] +
			columns[0][3] * columns[1][1] * columns[2][0] * columns[3][2] - columns[0][1] * columns[1][3] * columns[2][0] * columns[3][2] -
			columns[0][3] * columns[1][0] * columns[2][1] * columns[3][2] + columns[0][0] * columns[1][3] * columns[2][1] * columns[3][2] +
			columns[0][1] * columns[1][0] * columns[2][3] * columns[3][2] - columns[0][0] * columns[1][1] * columns[2][3] * columns[3][2] -
			columns[0][2] * columns[1][1] * columns[2][0] * columns[3][3] + columns[0][1] * columns[1][2] * columns[2][0] * columns[3][3] +
			columns[0][2] * columns[1][0] * columns[2][1] * columns[3][3] - columns[0][0] * columns[1][2] * columns[2][1] * columns[3][3] -
			columns[0][1] * columns[1][0] * columns[2][2] * columns[3][3] + columns[0][0] * columns[1][1] * columns[2][2] * columns[3][3];
}

void Projection::set_identity() {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			columns[i][j] = (i == j) ? 1 : 0;
		}
	}
}

void Projection::set_zero() {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			columns[i][j] = 0;
		}
	}
}

Plane Projection::xform4(const Plane &p_vec4) const {
	Plane ret;

	ret.normal.x = columns[0][0] * p_vec4.normal.x + columns[1][0] * p_vec4.normal.y + columns[2][0] * p_vec4.normal.z + columns[3][0] * p_vec4.d;
	ret.normal.y = columns[0][1] * p_vec4.normal.x + columns[1][1] * p_vec4.normal.y + columns[2][1] * p_vec4.normal.z + columns[3][1] * p_vec4.d;
	ret.normal.z = columns[0][2] * p_vec4.normal.x + columns[1][2] * p_vec4.normal.y + columns[2][2] * p_vec4.normal.z + columns[3][2] * p_vec4.d;
	ret.d = columns[0][3] * p_vec4.normal.x + columns[1][3] * p_vec4.normal.y + columns[2][3] * p_vec4.normal.z + columns[3][3] * p_vec4.d;
	return ret;
}

Vector4 Projection::xform(const Vector4 &p_vec4) const {
	return Vector4(
			columns[0][0] * p_vec4.x + columns[1][0] * p_vec4.y + columns[2][0] * p_vec4.z + columns[3][0] * p_vec4.w,
			columns[0][1] * p_vec4.x + columns[1][1] * p_vec4.y + columns[2][1] * p_vec4.z + columns[3][1] * p_vec4.w,
			columns[0][2] * p_vec4.x + columns[1][2] * p_vec4.y + columns[2][2] * p_vec4.z + columns[3][2] * p_vec4.w,
			columns[0][3] * p_vec4.x + columns[1][3] * p_vec4.y + columns[2][3] * p_vec4.z + columns[3][3] * p_vec4.w);
}
Vector4 Projection::xform_inv(const Vector4 &p_vec4) const {
	return Vector4(
			columns[0][0] * p_vec4.x + columns[0][1] * p_vec4.y + columns[0][2] * p_vec4.z + columns[0][3] * p_vec4.w,
			columns[1][0] * p_vec4.x + columns[1][1] * p_vec4.y + columns[1][2] * p_vec4.z + columns[1][3] * p_vec4.w,
			columns[2][0] * p_vec4.x + columns[2][1] * p_vec4.y + columns[2][2] * p_vec4.z + columns[2][3] * p_vec4.w,
			columns[3][0] * p_vec4.x + columns[3][1] * p_vec4.y + columns[3][2] * p_vec4.z + columns[3][3] * p_vec4.w);
}

void Projection::adjust_perspective_znear(real_t p_new_znear) {
	real_t zfar = get_z_far();
	real_t znear = p_new_znear;

	real_t deltaZ = zfar - znear;
	columns[2][2] = -(zfar + znear) / deltaZ;
	columns[3][2] = -2 * znear * zfar / deltaZ;
}

Projection Projection::create_depth_correction(bool p_flip_y) {
	Projection proj;
	proj.set_depth_correction(p_flip_y);
	return proj;
}

Projection Projection::create_light_atlas_rect(const Rect2 &p_rect) {
	Projection proj;
	proj.set_light_atlas_rect(p_rect);
	return proj;
}

Projection Projection::create_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov) {
	Projection proj;
	proj.set_perspective(p_fovy_degrees, p_aspect, p_z_near, p_z_far, p_flip_fov);
	return proj;
}

Projection Projection::create_perspective_hmd(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov, int p_eye, real_t p_intraocular_dist, real_t p_convergence_dist) {
	Projection proj;
	proj.set_perspective(p_fovy_degrees, p_aspect, p_z_near, p_z_far, p_flip_fov, p_eye, p_intraocular_dist, p_convergence_dist);
	return proj;
}

Projection Projection::create_for_hmd(int p_eye, real_t p_aspect, real_t p_intraocular_dist, real_t p_display_width, real_t p_display_to_lens, real_t p_oversample, real_t p_z_near, real_t p_z_far) {
	Projection proj;
	proj.set_for_hmd(p_eye, p_aspect, p_intraocular_dist, p_display_width, p_display_to_lens, p_oversample, p_z_near, p_z_far);
	return proj;
}

Projection Projection::create_orthogonal(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_znear, real_t p_zfar) {
	Projection proj;
	proj.set_orthogonal(p_left, p_right, p_bottom, p_top, p_znear, p_zfar);
	return proj;
}

Projection Projection::create_orthogonal_aspect(real_t p_size, real_t p_aspect, real_t p_znear, real_t p_zfar, bool p_flip_fov) {
	Projection proj;
	proj.set_orthogonal(p_size, p_aspect, p_znear, p_zfar, p_flip_fov);
	return proj;
}

Projection Projection::create_frustum(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_near, real_t p_far) {
	Projection proj;
	proj.set_frustum(p_left, p_right, p_bottom, p_top, p_near, p_far);
	return proj;
}

Projection Projection::create_frustum_aspect(real_t p_size, real_t p_aspect, Vector2 p_offset, real_t p_near, real_t p_far, bool p_flip_fov) {
	Projection proj;
	proj.set_frustum(p_size, p_aspect, p_offset, p_near, p_far, p_flip_fov);
	return proj;
}

Projection Projection::create_fit_aabb(const AABB &p_aabb) {
	Projection proj;
	proj.scale_translate_to_fit(p_aabb);
	return proj;
}

Projection Projection::perspective_znear_adjusted(real_t p_new_znear) const {
	Projection proj = *this;
	proj.adjust_perspective_znear(p_new_znear);
	return proj;
}

Plane Projection::get_projection_plane(Planes p_plane) const {
	const real_t *matrix = (const real_t *)columns;

	switch (p_plane) {
		case PLANE_NEAR: {
			Plane new_plane = Plane(matrix[3] + matrix[2],
					matrix[7] + matrix[6],
					matrix[11] + matrix[10],
					matrix[15] + matrix[14]);

			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_FAR: {
			Plane new_plane = Plane(matrix[3] - matrix[2],
					matrix[7] - matrix[6],
					matrix[11] - matrix[10],
					matrix[15] - matrix[14]);

			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_LEFT: {
			Plane new_plane = Plane(matrix[3] + matrix[0],
					matrix[7] + matrix[4],
					matrix[11] + matrix[8],
					matrix[15] + matrix[12]);

			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_TOP: {
			Plane new_plane = Plane(matrix[3] - matrix[1],
					matrix[7] - matrix[5],
					matrix[11] - matrix[9],
					matrix[15] - matrix[13]);

			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_RIGHT: {
			Plane new_plane = Plane(matrix[3] - matrix[0],
					matrix[7] - matrix[4],
					matrix[11] - matrix[8],
					matrix[15] - matrix[12]);

			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_BOTTOM: {
			Plane new_plane = Plane(matrix[3] + matrix[1],
					matrix[7] + matrix[5],
					matrix[11] + matrix[9],
					matrix[15] + matrix[13]);

			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
	}

	return Plane();
}

Projection Projection::flipped_y() const {
	Projection proj = *this;
	proj.flip_y();
	return proj;
}

Projection Projection ::jitter_offseted(const Vector2 &p_offset) const {
	Projection proj = *this;
	proj.add_jitter_offset(p_offset);
	return proj;
}

void Projection::set_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov) {
	if (p_flip_fov) {
		p_fovy_degrees = get_fovy(p_fovy_degrees, 1.0 / p_aspect);
	}

	real_t sine, cotangent, deltaZ;
	real_t radians = Math::deg_to_rad(p_fovy_degrees / 2.0);

	deltaZ = p_z_far - p_z_near;
	sine = Math::sin(radians);

	if ((deltaZ == 0) || (sine == 0) || (p_aspect == 0)) {
		return;
	}
	cotangent = Math::cos(radians) / sine;

	set_identity();

	columns[0][0] = cotangent / p_aspect;
	columns[1][1] = cotangent;
	columns[2][2] = -(p_z_far + p_z_near) / deltaZ;
	columns[2][3] = -1;
	columns[3][2] = -2 * p_z_near * p_z_far / deltaZ;
	columns[3][3] = 0;
}

void Projection::set_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov, int p_eye, real_t p_intraocular_dist, real_t p_convergence_dist) {
	if (p_flip_fov) {
		p_fovy_degrees = get_fovy(p_fovy_degrees, 1.0 / p_aspect);
	}

	real_t left, right, modeltranslation, ymax, xmax, frustumshift;

	ymax = p_z_near * tan(Math::deg_to_rad(p_fovy_degrees / 2.0));
	xmax = ymax * p_aspect;
	frustumshift = (p_intraocular_dist / 2.0) * p_z_near / p_convergence_dist;

	switch (p_eye) {
		case 1: { // left eye
			left = -xmax + frustumshift;
			right = xmax + frustumshift;
			modeltranslation = p_intraocular_dist / 2.0;
		} break;
		case 2: { // right eye
			left = -xmax - frustumshift;
			right = xmax - frustumshift;
			modeltranslation = -p_intraocular_dist / 2.0;
		} break;
		default: { // mono, should give the same result as set_perspective(p_fovy_degrees,p_aspect,p_z_near,p_z_far,p_flip_fov)
			left = -xmax;
			right = xmax;
			modeltranslation = 0.0;
		} break;
	}

	set_frustum(left, right, -ymax, ymax, p_z_near, p_z_far);

	// translate matrix by (modeltranslation, 0.0, 0.0)
	Projection cm;
	cm.set_identity();
	cm.columns[3][0] = modeltranslation;
	*this = *this * cm;
}

void Projection::set_for_hmd(int p_eye, real_t p_aspect, real_t p_intraocular_dist, real_t p_display_width, real_t p_display_to_lens, real_t p_oversample, real_t p_z_near, real_t p_z_far) {
	// we first calculate our base frustum on our values without taking our lens magnification into account.
	real_t f1 = (p_intraocular_dist * 0.5) / p_display_to_lens;
	real_t f2 = ((p_display_width - p_intraocular_dist) * 0.5) / p_display_to_lens;
	real_t f3 = (p_display_width / 4.0) / p_display_to_lens;

	// now we apply our oversample factor to increase our FOV. how much we oversample is always a balance we strike between performance and how much
	// we're willing to sacrifice in FOV.
	real_t add = ((f1 + f2) * (p_oversample - 1.0)) / 2.0;
	f1 += add;
	f2 += add;
	f3 *= p_oversample;

	// always apply KEEP_WIDTH aspect ratio
	f3 /= p_aspect;

	switch (p_eye) {
		case 1: { // left eye
			set_frustum(-f2 * p_z_near, f1 * p_z_near, -f3 * p_z_near, f3 * p_z_near, p_z_near, p_z_far);
		} break;
		case 2: { // right eye
			set_frustum(-f1 * p_z_near, f2 * p_z_near, -f3 * p_z_near, f3 * p_z_near, p_z_near, p_z_far);
		} break;
		default: { // mono, does not apply here!
		} break;
	}
}

void Projection::set_orthogonal(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_znear, real_t p_zfar) {
	set_identity();

	columns[0][0] = 2.0 / (p_right - p_left);
	columns[3][0] = -((p_right + p_left) / (p_right - p_left));
	columns[1][1] = 2.0 / (p_top - p_bottom);
	columns[3][1] = -((p_top + p_bottom) / (p_top - p_bottom));
	columns[2][2] = -2.0 / (p_zfar - p_znear);
	columns[3][2] = -((p_zfar + p_znear) / (p_zfar - p_znear));
	columns[3][3] = 1.0;
}

void Projection::set_orthogonal(real_t p_size, real_t p_aspect, real_t p_znear, real_t p_zfar, bool p_flip_fov) {
	if (!p_flip_fov) {
		p_size *= p_aspect;
	}

	set_orthogonal(-p_size / 2, +p_size / 2, -p_size / p_aspect / 2, +p_size / p_aspect / 2, p_znear, p_zfar);
}

void Projection::set_frustum(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_near, real_t p_far) {
	ERR_FAIL_COND(p_right <= p_left);
	ERR_FAIL_COND(p_top <= p_bottom);
	ERR_FAIL_COND(p_far <= p_near);

	real_t *te = &columns[0][0];
	real_t x = 2 * p_near / (p_right - p_left);
	real_t y = 2 * p_near / (p_top - p_bottom);

	real_t a = (p_right + p_left) / (p_right - p_left);
	real_t b = (p_top + p_bottom) / (p_top - p_bottom);
	real_t c = -(p_far + p_near) / (p_far - p_near);
	real_t d = -2 * p_far * p_near / (p_far - p_near);

	te[0] = x;
	te[1] = 0;
	te[2] = 0;
	te[3] = 0;
	te[4] = 0;
	te[5] = y;
	te[6] = 0;
	te[7] = 0;
	te[8] = a;
	te[9] = b;
	te[10] = c;
	te[11] = -1;
	te[12] = 0;
	te[13] = 0;
	te[14] = d;
	te[15] = 0;
}

void Projection::set_frustum(real_t p_size, real_t p_aspect, Vector2 p_offset, real_t p_near, real_t p_far, bool p_flip_fov) {
	if (!p_flip_fov) {
		p_size *= p_aspect;
	}

	set_frustum(-p_size / 2 + p_offset.x, +p_size / 2 + p_offset.x, -p_size / p_aspect / 2 + p_offset.y, +p_size / p_aspect / 2 + p_offset.y, p_near, p_far);
}

real_t Projection::get_z_far() const {
	const real_t *matrix = (const real_t *)columns;
	Plane new_plane = Plane(matrix[3] - matrix[2],
			matrix[7] - matrix[6],
			matrix[11] - matrix[10],
			matrix[15] - matrix[14]);

	new_plane.normalize();

	return new_plane.d;
}

real_t Projection::get_z_near() const {
	const real_t *matrix = (const real_t *)columns;
	Plane new_plane = Plane(matrix[3] + matrix[2],
			matrix[7] + matrix[6],
			matrix[11] + matrix[10],
			-matrix[15] - matrix[14]);

	new_plane.normalize();
	return new_plane.d;
}

Vector2 Projection::get_viewport_half_extents() const {
	const real_t *matrix = (const real_t *)columns;
	///////--- Near Plane ---///////
	Plane near_plane = Plane(matrix[3] + matrix[2],
			matrix[7] + matrix[6],
			matrix[11] + matrix[10],
			-matrix[15] - matrix[14]);
	near_plane.normalize();

	///////--- Right Plane ---///////
	Plane right_plane = Plane(matrix[3] - matrix[0],
			matrix[7] - matrix[4],
			matrix[11] - matrix[8],
			-matrix[15] + matrix[12]);
	right_plane.normalize();

	Plane top_plane = Plane(matrix[3] - matrix[1],
			matrix[7] - matrix[5],
			matrix[11] - matrix[9],
			-matrix[15] + matrix[13]);
	top_plane.normalize();

	Vector3 res;
	near_plane.intersect_3(right_plane, top_plane, &res);

	return Vector2(res.x, res.y);
}

Vector2 Projection::get_far_plane_half_extents() const {
	const real_t *matrix = (const real_t *)columns;
	///////--- Far Plane ---///////
	Plane far_plane = Plane(matrix[3] - matrix[2],
			matrix[7] - matrix[6],
			matrix[11] - matrix[10],
			-matrix[15] + matrix[14]);
	far_plane.normalize();

	///////--- Right Plane ---///////
	Plane right_plane = Plane(matrix[3] - matrix[0],
			matrix[7] - matrix[4],
			matrix[11] - matrix[8],
			-matrix[15] + matrix[12]);
	right_plane.normalize();

	Plane top_plane = Plane(matrix[3] - matrix[1],
			matrix[7] - matrix[5],
			matrix[11] - matrix[9],
			-matrix[15] + matrix[13]);
	top_plane.normalize();

	Vector3 res;
	far_plane.intersect_3(right_plane, top_plane, &res);

	return Vector2(res.x, res.y);
}

bool Projection::get_endpoints(const Transform3D &p_transform, Vector3 *p_8points) const {
	Vector<Plane> planes = get_projection_planes(Transform3D());
	const Planes intersections[8][3] = {
		{ PLANE_FAR, PLANE_LEFT, PLANE_TOP },
		{ PLANE_FAR, PLANE_LEFT, PLANE_BOTTOM },
		{ PLANE_FAR, PLANE_RIGHT, PLANE_TOP },
		{ PLANE_FAR, PLANE_RIGHT, PLANE_BOTTOM },
		{ PLANE_NEAR, PLANE_LEFT, PLANE_TOP },
		{ PLANE_NEAR, PLANE_LEFT, PLANE_BOTTOM },
		{ PLANE_NEAR, PLANE_RIGHT, PLANE_TOP },
		{ PLANE_NEAR, PLANE_RIGHT, PLANE_BOTTOM },
	};

	for (int i = 0; i < 8; i++) {
		Vector3 point;
		Plane a = planes[intersections[i][0]];
		Plane b = planes[intersections[i][1]];
		Plane c = planes[intersections[i][2]];
		bool res = a.intersect_3(b, c, &point);
		ERR_FAIL_COND_V(!res, false);
		p_8points[i] = p_transform.xform(point);
	}

	return true;
}

Vector<Plane> Projection::get_projection_planes(const Transform3D &p_transform) const {
	/** Fast Plane Extraction from combined modelview/projection matrices.
	 * References:
	 * https://web.archive.org/web/20011221205252/https://www.markmorley.com/opengl/frustumculling.html
	 * https://web.archive.org/web/20061020020112/https://www2.ravensoft.com/users/ggribb/plane%20extraction.pdf
	 */

	Vector<Plane> planes;
	planes.resize(6);

	const real_t *matrix = (const real_t *)columns;

	Plane new_plane;

	///////--- Near Plane ---///////
	new_plane = Plane(matrix[3] + matrix[2],
			matrix[7] + matrix[6],
			matrix[11] + matrix[10],
			matrix[15] + matrix[14]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.write[0] = p_transform.xform(new_plane);

	///////--- Far Plane ---///////
	new_plane = Plane(matrix[3] - matrix[2],
			matrix[7] - matrix[6],
			matrix[11] - matrix[10],
			matrix[15] - matrix[14]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.write[1] = p_transform.xform(new_plane);

	///////--- Left Plane ---///////
	new_plane = Plane(matrix[3] + matrix[0],
			matrix[7] + matrix[4],
			matrix[11] + matrix[8],
			matrix[15] + matrix[12]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.write[2] = p_transform.xform(new_plane);

	///////--- Top Plane ---///////
	new_plane = Plane(matrix[3] - matrix[1],
			matrix[7] - matrix[5],
			matrix[11] - matrix[9],
			matrix[15] - matrix[13]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.write[3] = p_transform.xform(new_plane);

	///////--- Right Plane ---///////
	new_plane = Plane(matrix[3] - matrix[0],
			matrix[7] - matrix[4],
			matrix[11] - matrix[8],
			matrix[15] - matrix[12]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.write[4] = p_transform.xform(new_plane);

	///////--- Bottom Plane ---///////
	new_plane = Plane(matrix[3] + matrix[1],
			matrix[7] + matrix[5],
			matrix[11] + matrix[9],
			matrix[15] + matrix[13]);

	new_plane.normal = -new_plane.normal;
	new_plane.normalize();

	planes.write[5] = p_transform.xform(new_plane);

	return planes;
}

Projection Projection::inverse() const {
	Projection cm = *this;
	cm.invert();
	return cm;
}

void Projection::invert() {
	// Adapted from Mesa's `src/util/u_math.c` `util_invert_mat4x4`.
	// MIT licensed. Copyright 2008 VMware, Inc. Authored by Jacques Leroy.
	Projection temp;
	real_t *out = (real_t *)temp.columns;
	real_t *m = (real_t *)columns;

	real_t wtmp[4][8];
	real_t m0, m1, m2, m3, s;
	real_t *r0, *r1, *r2, *r3;

#define MAT(m, r, c) (m)[(c) * 4 + (r)]

	r0 = wtmp[0];
	r1 = wtmp[1];
	r2 = wtmp[2];
	r3 = wtmp[3];

	r0[0] = MAT(m, 0, 0);
	r0[1] = MAT(m, 0, 1);
	r0[2] = MAT(m, 0, 2);
	r0[3] = MAT(m, 0, 3);
	r0[4] = 1.0;
	r0[5] = 0.0;
	r0[6] = 0.0;
	r0[7] = 0.0;

	r1[0] = MAT(m, 1, 0);
	r1[1] = MAT(m, 1, 1);
	r1[2] = MAT(m, 1, 2);
	r1[3] = MAT(m, 1, 3);
	r1[5] = 1.0;
	r1[4] = 0.0;
	r1[6] = 0.0;
	r1[7] = 0.0;

	r2[0] = MAT(m, 2, 0);
	r2[1] = MAT(m, 2, 1);
	r2[2] = MAT(m, 2, 2);
	r2[3] = MAT(m, 2, 3);
	r2[6] = 1.0;
	r2[4] = 0.0;
	r2[5] = 0.0;
	r2[7] = 0.0;

	r3[0] = MAT(m, 3, 0);
	r3[1] = MAT(m, 3, 1);
	r3[2] = MAT(m, 3, 2);
	r3[3] = MAT(m, 3, 3);

	r3[7] = 1.0;
	r3[4] = 0.0;
	r3[5] = 0.0;
	r3[6] = 0.0;

	/* choose pivot - or die */
	if (Math::abs(r3[0]) > Math::abs(r2[0])) {
		SWAP(r3, r2);
	}
	if (Math::abs(r2[0]) > Math::abs(r1[0])) {
		SWAP(r2, r1);
	}
	if (Math::abs(r1[0]) > Math::abs(r0[0])) {
		SWAP(r1, r0);
	}
	ERR_FAIL_COND(0.0 == r0[0]);

	/* eliminate first variable     */
	m1 = r1[0] / r0[0];
	m2 = r2[0] / r0[0];
	m3 = r3[0] / r0[0];
	s = r0[1];
	r1[1] -= m1 * s;
	r2[1] -= m2 * s;
	r3[1] -= m3 * s;
	s = r0[2];
	r1[2] -= m1 * s;
	r2[2] -= m2 * s;
	r3[2] -= m3 * s;
	s = r0[3];
	r1[3] -= m1 * s;
	r2[3] -= m2 * s;
	r3[3] -= m3 * s;
	s = r0[4];
	if (s != 0.0) {
		r1[4] -= m1 * s;
		r2[4] -= m2 * s;
		r3[4] -= m3 * s;
	}
	s = r0[5];
	if (s != 0.0) {
		r1[5] -= m1 * s;
		r2[5] -= m2 * s;
		r3[5] -= m3 * s;
	}
	s = r0[6];
	if (s != 0.0) {
		r1[6] -= m1 * s;
		r2[6] -= m2 * s;
		r3[6] -= m3 * s;
	}
	s = r0[7];
	if (s != 0.0) {
		r1[7] -= m1 * s;
		r2[7] -= m2 * s;
		r3[7] -= m3 * s;
	}

	/* choose pivot - or die */
	if (Math::abs(r3[1]) > Math::abs(r2[1])) {
		SWAP(r3, r2);
	}
	if (Math::abs(r2[1]) > Math::abs(r1[1])) {
		SWAP(r2, r1);
	}
	ERR_FAIL_COND(0.0 == r1[1]);

	/* eliminate second variable */
	m2 = r2[1] / r1[1];
	m3 = r3[1] / r1[1];
	r2[2] -= m2 * r1[2];
	r3[2] -= m3 * r1[2];
	r2[3] -= m2 * r1[3];
	r3[3] -= m3 * r1[3];
	s = r1[4];
	if (0.0 != s) {
		r2[4] -= m2 * s;
		r3[4] -= m3 * s;
	}
	s = r1[5];
	if (0.0 != s) {
		r2[5] -= m2 * s;
		r3[5] -= m3 * s;
	}
	s = r1[6];
	if (0.0 != s) {
		r2[6] -= m2 * s;
		r3[6] -= m3 * s;
	}
	s = r1[7];
	if (0.0 != s) {
		r2[7] -= m2 * s;
		r3[7] -= m3 * s;
	}

	/* choose pivot - or die */
	if (Math::abs(r3[2]) > Math::abs(r2[2])) {
		SWAP(r3, r2);
	}
	ERR_FAIL_COND(0.0 == r2[2]);

	/* eliminate third variable */
	m3 = r3[2] / r2[2];
	r3[3] -= m3 * r2[3];
	r3[4] -= m3 * r2[4];
	r3[5] -= m3 * r2[5];
	r3[6] -= m3 * r2[6];
	r3[7] -= m3 * r2[7];

	/* last check */
	ERR_FAIL_COND(0.0 == r3[3]);

	s = 1.0 / r3[3]; /* now back substitute row 3 */
	r3[4] *= s;
	r3[5] *= s;
	r3[6] *= s;
	r3[7] *= s;

	m2 = r2[3]; /* now back substitute row 2 */
	s = 1.0 / r2[2];
	r2[4] = s * (r2[4] - r3[4] * m2);
	r2[5] = s * (r2[5] - r3[5] * m2);
	r2[6] = s * (r2[6] - r3[6] * m2);
	r2[7] = s * (r2[7] - r3[7] * m2);
	m1 = r1[3];
	r1[4] -= r3[4] * m1;
	r1[5] -= r3[5] * m1;
	r1[6] -= r3[6] * m1;
	r1[7] -= r3[7] * m1;
	m0 = r0[3];
	r0[4] -= r3[4] * m0;
	r0[5] -= r3[5] * m0;
	r0[6] -= r3[6] * m0;
	r0[7] -= r3[7] * m0;

	m1 = r1[2]; /* now back substitute row 1 */
	s = 1.0 / r1[1];
	r1[4] = s * (r1[4] - r2[4] * m1);
	r1[5] = s * (r1[5] - r2[5] * m1),
	r1[6] = s * (r1[6] - r2[6] * m1);
	r1[7] = s * (r1[7] - r2[7] * m1);
	m0 = r0[2];
	r0[4] -= r2[4] * m0;
	r0[5] -= r2[5] * m0;
	r0[6] -= r2[6] * m0;
	r0[7] -= r2[7] * m0;

	m0 = r0[1]; /* now back substitute row 0 */
	s = 1.0 / r0[0];
	r0[4] = s * (r0[4] - r1[4] * m0);
	r0[5] = s * (r0[5] - r1[5] * m0),
	r0[6] = s * (r0[6] - r1[6] * m0);
	r0[7] = s * (r0[7] - r1[7] * m0);

	MAT(out, 0, 0) = r0[4];
	MAT(out, 0, 1) = r0[5];
	MAT(out, 0, 2) = r0[6];
	MAT(out, 0, 3) = r0[7];
	MAT(out, 1, 0) = r1[4];
	MAT(out, 1, 1) = r1[5];
	MAT(out, 1, 2) = r1[6];
	MAT(out, 1, 3) = r1[7];
	MAT(out, 2, 0) = r2[4];
	MAT(out, 2, 1) = r2[5];
	MAT(out, 2, 2) = r2[6];
	MAT(out, 2, 3) = r2[7];
	MAT(out, 3, 0) = r3[4];
	MAT(out, 3, 1) = r3[5];
	MAT(out, 3, 2) = r3[6];
	MAT(out, 3, 3) = r3[7];

#undef MAT

	*this = temp;
}

void Projection::flip_y() {
	for (int i = 0; i < 4; i++) {
		columns[1][i] = -columns[1][i];
	}
}

Projection::Projection() {
	set_identity();
}

Projection Projection::operator*(const Projection &p_matrix) const {
	Projection new_matrix;

	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			real_t ab = 0;
			for (int k = 0; k < 4; k++) {
				ab += columns[k][i] * p_matrix.columns[j][k];
			}
			new_matrix.columns[j][i] = ab;
		}
	}

	return new_matrix;
}

void Projection::set_depth_correction(bool p_flip_y, bool p_reverse_z, bool p_remap_z) {
	// p_remap_z is used to convert from OpenGL-style clip space (-1 - 1) to Vulkan style (0 - 1).
	real_t *m = &columns[0][0];

	m[0] = 1;
	m[1] = 0.0;
	m[2] = 0.0;
	m[3] = 0.0;
	m[4] = 0.0;
	m[5] = p_flip_y ? -1 : 1;
	m[6] = 0.0;
	m[7] = 0.0;
	m[8] = 0.0;
	m[9] = 0.0;
	m[10] = p_remap_z ? (p_reverse_z ? -0.5 : 0.5) : (p_reverse_z ? -1.0 : 1.0);
	m[11] = 0.0;
	m[12] = 0.0;
	m[13] = 0.0;
	m[14] = p_remap_z ? 0.5 : 0.0;
	m[15] = 1.0;
}

void Projection::set_light_bias() {
	real_t *m = &columns[0][0];

	m[0] = 0.5;
	m[1] = 0.0;
	m[2] = 0.0;
	m[3] = 0.0;
	m[4] = 0.0;
	m[5] = 0.5;
	m[6] = 0.0;
	m[7] = 0.0;
	m[8] = 0.0;
	m[9] = 0.0;
	m[10] = 0.5;
	m[11] = 0.0;
	m[12] = 0.5;
	m[13] = 0.5;
	m[14] = 0.5;
	m[15] = 1.0;
}

void Projection::set_light_atlas_rect(const Rect2 &p_rect) {
	real_t *m = &columns[0][0];

	m[0] = p_rect.size.width;
	m[1] = 0.0;
	m[2] = 0.0;
	m[3] = 0.0;
	m[4] = 0.0;
	m[5] = p_rect.size.height;
	m[6] = 0.0;
	m[7] = 0.0;
	m[8] = 0.0;
	m[9] = 0.0;
	m[10] = 1.0;
	m[11] = 0.0;
	m[12] = p_rect.position.x;
	m[13] = p_rect.position.y;
	m[14] = 0.0;
	m[15] = 1.0;
}

Projection::operator String() const {
	return "[X: " + columns[0].operator String() +
			", Y: " + columns[1].operator String() +
			", Z: " + columns[2].operator String() +
			", W: " + columns[3].operator String() + "]";
}

real_t Projection::get_aspect() const {
	Vector2 vp_he = get_viewport_half_extents();
	return vp_he.x / vp_he.y;
}

int Projection::get_pixels_per_meter(int p_for_pixel_width) const {
	Vector3 result = xform(Vector3(1, 0, -1));

	return int((result.x * 0.5 + 0.5) * p_for_pixel_width);
}

bool Projection::is_orthogonal() const {
	return columns[3][3] == 1.0;
}

real_t Projection::get_fov() const {
	const real_t *matrix = (const real_t *)columns;

	Plane right_plane = Plane(matrix[3] - matrix[0],
			matrix[7] - matrix[4],
			matrix[11] - matrix[8],
			-matrix[15] + matrix[12]);
	right_plane.normalize();

	if ((matrix[8] == 0) && (matrix[9] == 0)) {
		return Math::rad_to_deg(Math::acos(Math::abs(right_plane.normal.x))) * 2.0;
	} else {
		// our frustum is asymmetrical need to calculate the left planes angle separately..
		Plane left_plane = Plane(matrix[3] + matrix[0],
				matrix[7] + matrix[4],
				matrix[11] + matrix[8],
				matrix[15] + matrix[12]);
		left_plane.normalize();

		return Math::rad_to_deg(Math::acos(Math::abs(left_plane.normal.x))) + Math::rad_to_deg(Math::acos(Math::abs(right_plane.normal.x)));
	}
}

real_t Projection::get_lod_multiplier() const {
	if (is_orthogonal()) {
		return get_viewport_half_extents().x;
	} else {
		const real_t zn = get_z_near();
		const real_t width = get_viewport_half_extents().x * 2.0f;
		return 1.0f / (zn / width);
	}

	// Usage is lod_size / (lod_distance * multiplier) < threshold
}

void Projection::make_scale(const Vector3 &p_scale) {
	set_identity();
	columns[0][0] = p_scale.x;
	columns[1][1] = p_scale.y;
	columns[2][2] = p_scale.z;
}

void Projection::scale_translate_to_fit(const AABB &p_aabb) {
	Vector3 min = p_aabb.position;
	Vector3 max = p_aabb.position + p_aabb.size;

	columns[0][0] = 2 / (max.x - min.x);
	columns[1][0] = 0;
	columns[2][0] = 0;
	columns[3][0] = -(max.x + min.x) / (max.x - min.x);

	columns[0][1] = 0;
	columns[1][1] = 2 / (max.y - min.y);
	columns[2][1] = 0;
	columns[3][1] = -(max.y + min.y) / (max.y - min.y);

	columns[0][2] = 0;
	columns[1][2] = 0;
	columns[2][2] = 2 / (max.z - min.z);
	columns[3][2] = -(max.z + min.z) / (max.z - min.z);

	columns[0][3] = 0;
	columns[1][3] = 0;
	columns[2][3] = 0;
	columns[3][3] = 1;
}

void Projection::add_jitter_offset(const Vector2 &p_offset) {
	columns[3][0] += p_offset.x;
	columns[3][1] += p_offset.y;
}

Projection::operator Transform3D() const {
	Transform3D tr;
	const real_t *m = &columns[0][0];

	tr.basis.rows[0][0] = m[0];
	tr.basis.rows[1][0] = m[1];
	tr.basis.rows[2][0] = m[2];

	tr.basis.rows[0][1] = m[4];
	tr.basis.rows[1][1] = m[5];
	tr.basis.rows[2][1] = m[6];

	tr.basis.rows[0][2] = m[8];
	tr.basis.rows[1][2] = m[9];
	tr.basis.rows[2][2] = m[10];

	tr.origin.x = m[12];
	tr.origin.y = m[13];
	tr.origin.z = m[14];

	return tr;
}

Projection::Projection(const Vector4 &p_x, const Vector4 &p_y, const Vector4 &p_z, const Vector4 &p_w) {
	columns[0] = p_x;
	columns[1] = p_y;
	columns[2] = p_z;
	columns[3] = p_w;
}

Projection::Projection(const Transform3D &p_transform) {
	const Transform3D &tr = p_transform;
	real_t *m = &columns[0][0];

	m[0] = tr.basis.rows[0][0];
	m[1] = tr.basis.rows[1][0];
	m[2] = tr.basis.rows[2][0];
	m[3] = 0.0;
	m[4] = tr.basis.rows[0][1];
	m[5] = tr.basis.rows[1][1];
	m[6] = tr.basis.rows[2][1];
	m[7] = 0.0;
	m[8] = tr.basis.rows[0][2];
	m[9] = tr.basis.rows[1][2];
	m[10] = tr.basis.rows[2][2];
	m[11] = 0.0;
	m[12] = tr.origin.x;
	m[13] = tr.origin.y;
	m[14] = tr.origin.z;
	m[15] = 1.0;
}

Projection::~Projection() {
}
