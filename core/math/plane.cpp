/**************************************************************************/
/*  plane.cpp                                                             */
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

#include "plane.h"

#include "core/math/math_funcs.h"
#include "core/variant/variant.h"

void Plane::set_normal(const Vector3 &p_normal) {
	normal = p_normal;
}

void Plane::normalize() {
	real_t l = normal.length();
	if (l == 0) {
		*this = Plane(0, 0, 0, 0);
		return;
	}
	normal /= l;
	d /= l;
}

Plane Plane::normalized() const {
	Plane p = *this;
	p.normalize();
	return p;
}

Vector3 Plane::get_any_perpendicular_normal() const {
	static const Vector3 p1 = Vector3(1, 0, 0);
	static const Vector3 p2 = Vector3(0, 1, 0);
	Vector3 p;

	if (ABS(normal.dot(p1)) > 0.99f) { // if too similar to p1
		p = p2; // use p2
	} else {
		p = p1; // use p1
	}

	p -= normal * normal.dot(p);
	p.normalize();

	return p;
}

/* intersections */

bool Plane::intersect_3(const Plane &p_plane1, const Plane &p_plane2, Vector3 *r_result) const {
	const Plane &p_plane0 = *this;
	Vector3 normal0 = p_plane0.normal;
	Vector3 normal1 = p_plane1.normal;
	Vector3 normal2 = p_plane2.normal;

	real_t denom = vec3_cross(normal0, normal1).dot(normal2);

	if (Math::is_zero_approx(denom)) {
		return false;
	}

	if (r_result) {
		*r_result = ((vec3_cross(normal1, normal2) * p_plane0.d) +
							(vec3_cross(normal2, normal0) * p_plane1.d) +
							(vec3_cross(normal0, normal1) * p_plane2.d)) /
				denom;
	}

	return true;
}

bool Plane::intersects_ray(const Vector3 &p_from, const Vector3 &p_dir, Vector3 *p_intersection) const {
	Vector3 segment = p_dir;
	real_t den = normal.dot(segment);

	if (Math::is_zero_approx(den)) {
		return false;
	}

	real_t dist = (normal.dot(p_from) - d) / den;

	if (dist > (real_t)CMP_EPSILON) { //this is a ray, before the emitting pos (p_from) doesn't exist

		return false;
	}

	dist = -dist;
	*p_intersection = p_from + segment * dist;

	return true;
}

bool Plane::intersects_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 *p_intersection) const {
	Vector3 segment = p_begin - p_end;
	real_t den = normal.dot(segment);

	if (Math::is_zero_approx(den)) {
		return false;
	}

	real_t dist = (normal.dot(p_begin) - d) / den;

	if (dist < (real_t)-CMP_EPSILON || dist > (1.0f + (real_t)CMP_EPSILON)) {
		return false;
	}

	dist = -dist;
	*p_intersection = p_begin + segment * dist;

	return true;
}

Variant Plane::intersect_3_bind(const Plane &p_plane1, const Plane &p_plane2) const {
	Vector3 inters;
	if (intersect_3(p_plane1, p_plane2, &inters)) {
		return inters;
	} else {
		return Variant();
	}
}

Variant Plane::intersects_ray_bind(const Vector3 &p_from, const Vector3 &p_dir) const {
	Vector3 inters;
	if (intersects_ray(p_from, p_dir, &inters)) {
		return inters;
	} else {
		return Variant();
	}
}

Variant Plane::intersects_segment_bind(const Vector3 &p_begin, const Vector3 &p_end) const {
	Vector3 inters;
	if (intersects_segment(p_begin, p_end, &inters)) {
		return inters;
	} else {
		return Variant();
	}
}

/* misc */

bool Plane::is_equal_approx_any_side(const Plane &p_plane) const {
	return (normal.is_equal_approx(p_plane.normal) && Math::is_equal_approx(d, p_plane.d)) || (normal.is_equal_approx(-p_plane.normal) && Math::is_equal_approx(d, -p_plane.d));
}

bool Plane::is_equal_approx(const Plane &p_plane) const {
	return normal.is_equal_approx(p_plane.normal) && Math::is_equal_approx(d, p_plane.d);
}

bool Plane::is_same(const Plane &p_plane) const {
	return normal.is_same(p_plane.normal) && Math::is_same(d, p_plane.d);
}

bool Plane::is_finite() const {
	return normal.is_finite() && Math::is_finite(d);
}

Plane::operator String() const {
	return "[N: " + normal.operator String() + ", D: " + String::num_real(d, false) + "]";
}
