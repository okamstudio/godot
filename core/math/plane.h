/**************************************************************************/
/*  plane.h                                                               */
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

class Variant;

struct [[nodiscard]] Plane {
	Vector3 normal;
	real_t d = 0;

	void set_normal(const Vector3 &p_normal);
	_FORCE_INLINE_ Vector3 get_normal() const { return normal; }

	void normalize();
	Plane normalized() const;

	/* Plane-Point operations */

	_FORCE_INLINE_ Vector3 get_center() const { return normal * d; }
	Vector3 get_any_perpendicular_normal() const;

	_FORCE_INLINE_ bool is_point_over(const Vector3 &p_point) const; ///< Point is over plane
	_FORCE_INLINE_ real_t distance_to(const Vector3 &p_point) const;
	_FORCE_INLINE_ bool has_point(const Vector3 &p_point, real_t p_tolerance = CMP_EPSILON) const;

	/* intersections */

	bool intersect_3(const Plane &p_plane1, const Plane &p_plane2, Vector3 *r_result = nullptr) const;
	bool intersects_ray(const Vector3 &p_from, const Vector3 &p_dir, Vector3 *p_intersection) const;
	bool intersects_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 *p_intersection) const;

	// For Variant bindings.
	Variant intersect_3_bind(const Plane &p_plane1, const Plane &p_plane2) const;
	Variant intersects_ray_bind(const Vector3 &p_from, const Vector3 &p_dir) const;
	Variant intersects_segment_bind(const Vector3 &p_begin, const Vector3 &p_end) const;

	_FORCE_INLINE_ Vector3 project(const Vector3 &p_point) const {
		return p_point - normal * distance_to(p_point);
	}

	/* misc */

	Plane operator-() const { return Plane(-normal, -d); }
	bool is_equal_approx(const Plane &p_plane) const;
	bool is_equal_approx_any_side(const Plane &p_plane) const;
	bool is_finite() const;

	_FORCE_INLINE_ bool operator==(const Plane &p_plane) const;
	_FORCE_INLINE_ bool operator!=(const Plane &p_plane) const;
	operator String() const;

	_FORCE_INLINE_ Plane() {}
	_FORCE_INLINE_ Plane(real_t p_a, real_t p_b, real_t p_c, real_t p_d) :
			normal(p_a, p_b, p_c),
			d(p_d) {}

	_FORCE_INLINE_ Plane(const Vector3 &p_normal, real_t p_d = 0.0);
	_FORCE_INLINE_ Plane(const Vector3 &p_normal, const Vector3 &p_point);
	_FORCE_INLINE_ Plane(const Vector3 &p_point1, const Vector3 &p_point2, const Vector3 &p_point3, ClockDirection p_dir = CLOCKWISE);
};

bool Plane::is_point_over(const Vector3 &p_point) const {
	return (normal.dot(p_point) > d);
}

real_t Plane::distance_to(const Vector3 &p_point) const {
	return (normal.dot(p_point) - d);
}

bool Plane::has_point(const Vector3 &p_point, real_t p_tolerance) const {
	real_t dist = normal.dot(p_point) - d;
	dist = ABS(dist);
	return (dist <= p_tolerance);
}

Plane::Plane(const Vector3 &p_normal, real_t p_d) :
		normal(p_normal),
		d(p_d) {
}

Plane::Plane(const Vector3 &p_normal, const Vector3 &p_point) :
		normal(p_normal),
		d(p_normal.dot(p_point)) {
}

Plane::Plane(const Vector3 &p_point1, const Vector3 &p_point2, const Vector3 &p_point3, ClockDirection p_dir) {
	if (p_dir == CLOCKWISE) {
		normal = (p_point1 - p_point3).cross(p_point1 - p_point2);
	} else {
		normal = (p_point1 - p_point2).cross(p_point1 - p_point3);
	}

	normal.normalize();
	d = normal.dot(p_point1);
}

bool Plane::operator==(const Plane &p_plane) const {
	return normal == p_plane.normal && d == p_plane.d;
}

bool Plane::operator!=(const Plane &p_plane) const {
	return normal != p_plane.normal || d != p_plane.d;
}
