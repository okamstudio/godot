/**************************************************************************/
/*  projection.h                                                          */
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

#include "core/math/vector3.h"
#include "core/math/vector4.h"

template <typename T>
class Vector;

struct AABB;
struct Plane;
struct Rect2;
struct Transform3D;
struct Vector2;

struct [[nodiscard]] Projection {
	enum Planes {
		PLANE_NEAR,
		PLANE_FAR,
		PLANE_LEFT,
		PLANE_TOP,
		PLANE_RIGHT,
		PLANE_BOTTOM
	};

	Vector4 columns[4];

	_FORCE_INLINE_ const Vector4 &operator[](int p_axis) const {
		DEV_ASSERT((unsigned int)p_axis < 4);
		return columns[p_axis];
	}

	_FORCE_INLINE_ Vector4 &operator[](int p_axis) {
		DEV_ASSERT((unsigned int)p_axis < 4);
		return columns[p_axis];
	}

	real_t determinant() const;
	void set_identity();
	void set_zero();
	void set_light_bias();
	void set_depth_correction(bool p_flip_y = true, bool p_reverse_z = true, bool p_remap_z = true);

	void set_light_atlas_rect(const Rect2 &p_rect);
	void set_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov = false);
	void set_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov, int p_eye, real_t p_intraocular_dist, real_t p_convergence_dist);
	void set_for_hmd(int p_eye, real_t p_aspect, real_t p_intraocular_dist, real_t p_display_width, real_t p_display_to_lens, real_t p_oversample, real_t p_z_near, real_t p_z_far);
	void set_orthogonal(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_znear, real_t p_zfar);
	void set_orthogonal(real_t p_size, real_t p_aspect, real_t p_znear, real_t p_zfar, bool p_flip_fov = false);
	void set_frustum(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_near, real_t p_far);
	void set_frustum(real_t p_size, real_t p_aspect, Vector2 p_offset, real_t p_near, real_t p_far, bool p_flip_fov = false);
	void adjust_perspective_znear(real_t p_new_znear);

	static Projection create_depth_correction(bool p_flip_y);
	static Projection create_light_atlas_rect(const Rect2 &p_rect);
	static Projection create_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov = false);
	static Projection create_perspective_hmd(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov, int p_eye, real_t p_intraocular_dist, real_t p_convergence_dist);
	static Projection create_for_hmd(int p_eye, real_t p_aspect, real_t p_intraocular_dist, real_t p_display_width, real_t p_display_to_lens, real_t p_oversample, real_t p_z_near, real_t p_z_far);
	static Projection create_orthogonal(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_znear, real_t p_zfar);
	static Projection create_orthogonal_aspect(real_t p_size, real_t p_aspect, real_t p_znear, real_t p_zfar, bool p_flip_fov = false);
	static Projection create_frustum(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_near, real_t p_far);
	static Projection create_frustum_aspect(real_t p_size, real_t p_aspect, Vector2 p_offset, real_t p_near, real_t p_far, bool p_flip_fov = false);
	static Projection create_fit_aabb(const AABB &p_aabb);
	Projection perspective_znear_adjusted(real_t p_new_znear) const;
	Plane get_projection_plane(Planes p_plane) const;
	Projection flipped_y() const;
	Projection jitter_offseted(const Vector2 &p_offset) const;

	static real_t get_fovy(real_t p_fovx, real_t p_aspect) {
		return Math::rad_to_deg(Math::atan(p_aspect * Math::tan(Math::deg_to_rad(p_fovx) * 0.5)) * 2.0);
	}

	real_t get_z_far() const;
	real_t get_z_near() const;
	real_t get_aspect() const;
	real_t get_fov() const;
	bool is_orthogonal() const;

	Vector<Plane> get_projection_planes(const Transform3D &p_transform) const;

	bool get_endpoints(const Transform3D &p_transform, Vector3 *p_8points) const;
	Vector2 get_viewport_half_extents() const;
	Vector2 get_far_plane_half_extents() const;

	void invert();
	Projection inverse() const;

	Projection operator*(const Projection &p_matrix) const;

	Plane xform4(const Plane &p_vec4) const;
	_FORCE_INLINE_ Vector3 xform(const Vector3 &p_vec3) const;

	Vector4 xform(const Vector4 &p_vec4) const;
	Vector4 xform_inv(const Vector4 &p_vec4) const;

	operator String() const;

	void scale_translate_to_fit(const AABB &p_aabb);
	void add_jitter_offset(const Vector2 &p_offset);
	void make_scale(const Vector3 &p_scale);
	int get_pixels_per_meter(int p_for_pixel_width) const;
	operator Transform3D() const;

	void flip_y();

	bool operator==(const Projection &p_cam) const {
		for (uint32_t i = 0; i < 4; i++) {
			for (uint32_t j = 0; j < 4; j++) {
				if (columns[i][j] != p_cam.columns[i][j]) {
					return false;
				}
			}
		}
		return true;
	}

	bool operator!=(const Projection &p_cam) const {
		return !(*this == p_cam);
	}

	real_t get_lod_multiplier() const;

	Projection();
	Projection(const Vector4 &p_x, const Vector4 &p_y, const Vector4 &p_z, const Vector4 &p_w);
	Projection(const Transform3D &p_transform);
	~Projection();
};

Vector3 Projection::xform(const Vector3 &p_vec3) const {
	Vector3 ret;
	ret.x = columns[0][0] * p_vec3.x + columns[1][0] * p_vec3.y + columns[2][0] * p_vec3.z + columns[3][0];
	ret.y = columns[0][1] * p_vec3.x + columns[1][1] * p_vec3.y + columns[2][1] * p_vec3.z + columns[3][1];
	ret.z = columns[0][2] * p_vec3.x + columns[1][2] * p_vec3.y + columns[2][2] * p_vec3.z + columns[3][2];
	real_t w = columns[0][3] * p_vec3.x + columns[1][3] * p_vec3.y + columns[2][3] * p_vec3.z + columns[3][3];
	return ret / w;
}
