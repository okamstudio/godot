/**************************************************************************/
/*  godot_shape_3d.cpp                                                    */
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

#include "godot_shape_3d.h"

#include "core/io/image.h"
#include "core/math/convex_hull.h"
#include "core/math/geometry_3d.h"
#include "core/templates/sort_array.h"

// GodotHeightMapShape3D is based on Bullet btHeightfieldTerrainShape.

/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2009 Erwin Coumans  http://bulletphysics.org

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it freely,
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

const double edge_support_threshold = 0.99999998;
const double edge_support_threshold_lower = Math::sqrt(1.0 - edge_support_threshold * edge_support_threshold);
// For a unit normal vector n, the horizontality condition
//     sqrt(n.x * n.x + n.z * n.z) > edge_support_threshold
// is equivalent to the condition
//     abs(n.y) < edge_support_threshold_lower,
// which is cheaper to test.
const double face_support_threshold = 0.9998;

const double cylinder_edge_support_threshold = 0.999998;
const double cylinder_edge_support_threshold_lower = Math::sqrt(1.0 - cylinder_edge_support_threshold * cylinder_edge_support_threshold);
const double cylinder_face_support_threshold = 0.999;

void GodotShape3D::configure(const AABB &p_aabb) {
	aabb = p_aabb;
	configured = true;
	for (const KeyValue<GodotShapeOwner3D *, int> &E : owners) {
		GodotShapeOwner3D *co = E.key;
		co->_shape_changed();
	}
}

Vector3 GodotShape3D::get_support(const Vector3 &p_normal) const {
	Vector3 res;
	int amnt;
	FeatureType type;
	get_supports(p_normal, 1, &res, amnt, type);
	return res;
}

void GodotShape3D::add_owner(GodotShapeOwner3D *p_owner) {
	HashMap<GodotShapeOwner3D *, int>::Iterator E = owners.find(p_owner);
	if (E) {
		E->value++;
	} else {
		owners[p_owner] = 1;
	}
}

void GodotShape3D::remove_owner(GodotShapeOwner3D *p_owner) {
	HashMap<GodotShapeOwner3D *, int>::Iterator E = owners.find(p_owner);
	ERR_FAIL_COND(!E);
	E->value--;
	if (E->value == 0) {
		owners.remove(E);
	}
}

bool GodotShape3D::is_owner(GodotShapeOwner3D *p_owner) const {
	return owners.has(p_owner);
}

const HashMap<GodotShapeOwner3D *, int> &GodotShape3D::get_owners() const {
	return owners;
}

GodotShape3D::~GodotShape3D() {
	ERR_FAIL_COND(owners.size());
}

Plane GodotWorldBoundaryShape3D::get_plane() const {
	return plane;
}

void GodotWorldBoundaryShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	// gibberish, a plane is infinity
	r_min = -1e7;
	r_max = 1e7;
}

Vector3 GodotWorldBoundaryShape3D::get_support(const Vector3 &p_normal) const {
	return p_normal * 1e15;
}

bool GodotWorldBoundaryShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	bool inters = plane.intersects_segment(p_begin, p_end, &r_result);
	if (inters) {
		r_normal = plane.normal;
	}
	return inters;
}

bool GodotWorldBoundaryShape3D::intersect_point(const Vector3 &p_point) const {
	return plane.distance_to(p_point) < 0;
}

Vector3 GodotWorldBoundaryShape3D::get_closest_point_to(const Vector3 &p_point) const {
	if (plane.is_point_over(p_point)) {
		return plane.project(p_point);
	} else {
		return p_point;
	}
}

Vector3 GodotWorldBoundaryShape3D::get_moment_of_inertia(real_t p_mass) const {
	return Vector3(); // not applicable.
}

void GodotWorldBoundaryShape3D::_setup(const Plane &p_plane) {
	plane = p_plane;
	configure(AABB(Vector3(-1e15, -1e15, -1e15), Vector3(1e15 * 2, 1e15 * 2, 1e15 * 2)));
}

void GodotWorldBoundaryShape3D::set_data(const Variant &p_data) {
	_setup(p_data);
}

Variant GodotWorldBoundaryShape3D::get_data() const {
	return plane;
}

GodotWorldBoundaryShape3D::GodotWorldBoundaryShape3D() {
}

//

real_t GodotSeparationRayShape3D::get_length() const {
	return length;
}

bool GodotSeparationRayShape3D::get_slide_on_slope() const {
	return slide_on_slope;
}

void GodotSeparationRayShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	// don't think this will be even used
	r_min = 0;
	r_max = 1;
}

Vector3 GodotSeparationRayShape3D::get_support(const Vector3 &p_normal) const {
	if (p_normal.z > 0) {
		return Vector3(0, 0, length);
	} else {
		return Vector3(0, 0, 0);
	}
}

void GodotSeparationRayShape3D::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount, FeatureType &r_type) const {
	if (Math::abs(p_normal.z) < edge_support_threshold_lower) {
		r_amount = 2;
		r_type = FEATURE_EDGE;
		r_supports[0] = Vector3(0, 0, 0);
		r_supports[1] = Vector3(0, 0, length);
	} else if (p_normal.z > 0) {
		r_amount = 1;
		r_type = FEATURE_POINT;
		*r_supports = Vector3(0, 0, length);
	} else {
		r_amount = 1;
		r_type = FEATURE_POINT;
		*r_supports = Vector3(0, 0, 0);
	}
}

bool GodotSeparationRayShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	return false; //simply not possible
}

bool GodotSeparationRayShape3D::intersect_point(const Vector3 &p_point) const {
	return false; //simply not possible
}

Vector3 GodotSeparationRayShape3D::get_closest_point_to(const Vector3 &p_point) const {
	Vector3 s[2] = {
		Vector3(0, 0, 0),
		Vector3(0, 0, length)
	};

	return Geometry3D::get_closest_point_to_segment(p_point, s);
}

Vector3 GodotSeparationRayShape3D::get_moment_of_inertia(real_t p_mass) const {
	return Vector3();
}

void GodotSeparationRayShape3D::_setup(real_t p_length, bool p_slide_on_slope) {
	length = p_length;
	slide_on_slope = p_slide_on_slope;
	configure(AABB(Vector3(0, 0, 0), Vector3(0.1, 0.1, length)));
}

void GodotSeparationRayShape3D::set_data(const Variant &p_data) {
	Dictionary d = p_data;
	_setup(d["length"], d["slide_on_slope"]);
}

Variant GodotSeparationRayShape3D::get_data() const {
	Dictionary d;
	d["length"] = length;
	d["slide_on_slope"] = slide_on_slope;
	return d;
}

GodotSeparationRayShape3D::GodotSeparationRayShape3D() {}

/********** SPHERE *************/

real_t GodotSphereShape3D::get_radius() const {
	return radius;
}

void GodotSphereShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	real_t d = p_normal.dot(p_transform.origin);

	// figure out scale at point
	Vector3 local_normal = p_transform.basis.xform_inv(p_normal);
	real_t scale = local_normal.length();

	r_min = d - (radius)*scale;
	r_max = d + (radius)*scale;
}

Vector3 GodotSphereShape3D::get_support(const Vector3 &p_normal) const {
	return p_normal * radius;
}

void GodotSphereShape3D::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount, FeatureType &r_type) const {
	*r_supports = p_normal * radius;
	r_amount = 1;
	r_type = FEATURE_POINT;
}

bool GodotSphereShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	return Geometry3D::segment_intersects_sphere(p_begin, p_end, Vector3(), radius, &r_result, &r_normal);
}

bool GodotSphereShape3D::intersect_point(const Vector3 &p_point) const {
	return p_point.length() < radius;
}

Vector3 GodotSphereShape3D::get_closest_point_to(const Vector3 &p_point) const {
	Vector3 p = p_point;
	real_t l = p.length();
	if (l < radius) {
		return p_point;
	}
	return (p / l) * radius;
}

Vector3 GodotSphereShape3D::get_moment_of_inertia(real_t p_mass) const {
	real_t s = 0.4 * p_mass * radius * radius;
	return Vector3(s, s, s);
}

void GodotSphereShape3D::_setup(real_t p_radius) {
	radius = p_radius;
	configure(AABB(Vector3(-radius, -radius, -radius), Vector3(radius * 2.0, radius * 2.0, radius * 2.0)));
}

void GodotSphereShape3D::set_data(const Variant &p_data) {
	_setup(p_data);
}

Variant GodotSphereShape3D::get_data() const {
	return radius;
}

GodotSphereShape3D::GodotSphereShape3D() {}

/********** BOX *************/

void GodotBoxShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	// no matter the angle, the box is mirrored anyway
	Vector3 local_normal = p_transform.basis.xform_inv(p_normal);

	real_t length = local_normal.abs().dot(half_extents);
	real_t distance = p_normal.dot(p_transform.origin);

	r_min = distance - length;
	r_max = distance + length;
}

Vector3 GodotBoxShape3D::get_support(const Vector3 &p_normal) const {
	Vector3 point(
			(p_normal.x < 0) ? -half_extents.x : half_extents.x,
			(p_normal.y < 0) ? -half_extents.y : half_extents.y,
			(p_normal.z < 0) ? -half_extents.z : half_extents.z);

	return point;
}

void GodotBoxShape3D::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount, FeatureType &r_type) const {
	static const int next[3] = { 1, 2, 0 };
	static const int next2[3] = { 2, 0, 1 };

	for (int i = 0; i < 3; i++) {
		Vector3 axis;
		axis[i] = 1.0;
		real_t dot = p_normal.dot(axis);
		if (Math::abs(dot) > face_support_threshold) {
			//Vector3 axis_b;

			bool neg = dot < 0;
			r_amount = 4;
			r_type = FEATURE_FACE;

			Vector3 point;
			point[i] = half_extents[i];

			int i_n = next[i];
			int i_n2 = next2[i];

			static const real_t sign[4][2] = {
				{ -1.0, 1.0 },
				{ 1.0, 1.0 },
				{ 1.0, -1.0 },
				{ -1.0, -1.0 },
			};

			for (int j = 0; j < 4; j++) {
				point[i_n] = sign[j][0] * half_extents[i_n];
				point[i_n2] = sign[j][1] * half_extents[i_n2];
				r_supports[j] = neg ? -point : point;
			}

			if (neg) {
				SWAP(r_supports[1], r_supports[2]);
				SWAP(r_supports[0], r_supports[3]);
			}

			return;
		}

		r_amount = 0;
	}

	for (int i = 0; i < 3; i++) {
		Vector3 axis;
		axis[i] = 1.0;

		if (Math::abs(p_normal.dot(axis)) < edge_support_threshold_lower) {
			r_amount = 2;
			r_type = FEATURE_EDGE;

			int i_n = next[i];
			int i_n2 = next2[i];

			Vector3 point = half_extents;

			if (p_normal[i_n] < 0) {
				point[i_n] = -point[i_n];
			}
			if (p_normal[i_n2] < 0) {
				point[i_n2] = -point[i_n2];
			}

			r_supports[0] = point;
			point[i] = -point[i];
			r_supports[1] = point;
			return;
		}
	}
	/* USE POINT */

	Vector3 point(
			(p_normal.x < 0) ? -half_extents.x : half_extents.x,
			(p_normal.y < 0) ? -half_extents.y : half_extents.y,
			(p_normal.z < 0) ? -half_extents.z : half_extents.z);

	r_amount = 1;
	r_type = FEATURE_POINT;
	r_supports[0] = point;
}

bool GodotBoxShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	AABB aabb_ext(-half_extents, half_extents * 2.0);

	return aabb_ext.intersects_segment(p_begin, p_end, &r_result, &r_normal);
}

bool GodotBoxShape3D::intersect_point(const Vector3 &p_point) const {
	return (Math::abs(p_point.x) < half_extents.x && Math::abs(p_point.y) < half_extents.y && Math::abs(p_point.z) < half_extents.z);
}

Vector3 GodotBoxShape3D::get_closest_point_to(const Vector3 &p_point) const {
	int outside = 0;
	Vector3 min_point;

	for (int i = 0; i < 3; i++) {
		if (Math::abs(p_point[i]) > half_extents[i]) {
			outside++;
			if (outside == 1) {
				//use plane if only one side matches
				Vector3 n;
				n[i] = SIGN(p_point[i]);

				Plane p(n, half_extents[i]);
				min_point = p.project(p_point);
			}
		}
	}

	if (!outside) {
		return p_point; //it's inside, don't do anything else
	}

	if (outside == 1) { //if only above one plane, this plane clearly wins
		return min_point;
	}

	//check segments
	real_t min_distance = 1e20;
	Vector3 closest_vertex = half_extents * p_point.sign();
	Vector3 s[2] = {
		closest_vertex,
		closest_vertex
	};

	for (int i = 0; i < 3; i++) {
		s[1] = closest_vertex;
		s[1][i] = -s[1][i]; //edge

		Vector3 closest_edge = Geometry3D::get_closest_point_to_segment(p_point, s);

		real_t d = p_point.distance_to(closest_edge);
		if (d < min_distance) {
			min_point = closest_edge;
			min_distance = d;
		}
	}

	return min_point;
}

Vector3 GodotBoxShape3D::get_moment_of_inertia(real_t p_mass) const {
	real_t lx = half_extents.x;
	real_t ly = half_extents.y;
	real_t lz = half_extents.z;

	return Vector3((p_mass / 3.0) * (ly * ly + lz * lz), (p_mass / 3.0) * (lx * lx + lz * lz), (p_mass / 3.0) * (lx * lx + ly * ly));
}

void GodotBoxShape3D::_setup(const Vector3 &p_half_extents) {
	half_extents = p_half_extents.abs();

	configure(AABB(-half_extents, half_extents * 2));
}

void GodotBoxShape3D::set_data(const Variant &p_data) {
	_setup(p_data);
}

Variant GodotBoxShape3D::get_data() const {
	return half_extents;
}

GodotBoxShape3D::GodotBoxShape3D() {}

/********** CAPSULE *************/

void GodotCapsuleShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	Vector3 n = p_transform.basis.xform_inv(p_normal).normalized();
	real_t h = height * 0.5 - radius;

	n *= radius;
	n.y += (n.y > 0) ? h : -h;

	r_max = p_normal.dot(p_transform.xform(n));
	r_min = p_normal.dot(p_transform.xform(-n));
}

Vector3 GodotCapsuleShape3D::get_support(const Vector3 &p_normal) const {
	Vector3 n = p_normal;

	real_t h = height * 0.5 - radius;

	n *= radius;
	n.y += (n.y > 0) ? h : -h;
	return n;
}

void GodotCapsuleShape3D::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount, FeatureType &r_type) const {
	Vector3 n = p_normal;

	real_t d = n.y;
	real_t h = height * 0.5 - radius; // half-height of the cylinder part

	if (h > 0 && Math::abs(d) < edge_support_threshold_lower) {
		// make it flat
		n.y = 0.0;
		n.normalize();
		n *= radius;

		r_amount = 2;
		r_type = FEATURE_EDGE;
		r_supports[0] = n;
		r_supports[0].y += h;
		r_supports[1] = n;
		r_supports[1].y -= h;
	} else {
		n *= radius;
		n.y += (d > 0) ? h : -h;
		r_amount = 1;
		r_type = FEATURE_POINT;
		*r_supports = n;
	}
}

bool GodotCapsuleShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	Vector3 norm = (p_end - p_begin).normalized();
	real_t min_d = 1e20;

	Vector3 res, n;
	bool collision = false;

	Vector3 auxres, auxn;
	bool collided;

	// test against cylinder and spheres :-|

	collided = Geometry3D::segment_intersects_cylinder(p_begin, p_end, height - radius * 2.0, radius, &auxres, &auxn, 1);

	if (collided) {
		real_t d = norm.dot(auxres);
		if (d < min_d) {
			min_d = d;
			res = auxres;
			n = auxn;
			collision = true;
		}
	}

	collided = Geometry3D::segment_intersects_sphere(p_begin, p_end, Vector3(0, height * 0.5 - radius, 0), radius, &auxres, &auxn);

	if (collided) {
		real_t d = norm.dot(auxres);
		if (d < min_d) {
			min_d = d;
			res = auxres;
			n = auxn;
			collision = true;
		}
	}

	collided = Geometry3D::segment_intersects_sphere(p_begin, p_end, Vector3(0, height * -0.5 + radius, 0), radius, &auxres, &auxn);

	if (collided) {
		real_t d = norm.dot(auxres);

		if (d < min_d) {
			min_d = d;
			res = auxres;
			n = auxn;
			collision = true;
		}
	}

	if (collision) {
		r_result = res;
		r_normal = n;
	}
	return collision;
}

bool GodotCapsuleShape3D::intersect_point(const Vector3 &p_point) const {
	if (Math::abs(p_point.y) < height * 0.5 - radius) {
		return Vector3(p_point.x, 0, p_point.z).length() < radius;
	} else {
		Vector3 p = p_point;
		p.y = Math::abs(p.y) - height * 0.5 + radius;
		return p.length() < radius;
	}
}

Vector3 GodotCapsuleShape3D::get_closest_point_to(const Vector3 &p_point) const {
	Vector3 s[2] = {
		Vector3(0, -height * 0.5 + radius, 0),
		Vector3(0, height * 0.5 - radius, 0),
	};

	Vector3 p = Geometry3D::get_closest_point_to_segment(p_point, s);

	if (p.distance_to(p_point) < radius) {
		return p_point;
	}

	return p + (p_point - p).normalized() * radius;
}

Vector3 GodotCapsuleShape3D::get_moment_of_inertia(real_t p_mass) const {
	// use bad AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.y * extents.y));
}

void GodotCapsuleShape3D::_setup(real_t p_height, real_t p_radius) {
	height = p_height;
	radius = p_radius;
	configure(AABB(Vector3(-radius, -height * 0.5, -radius), Vector3(radius * 2, height, radius * 2)));
}

void GodotCapsuleShape3D::set_data(const Variant &p_data) {
	Dictionary d = p_data;
	ERR_FAIL_COND(!d.has("radius"));
	ERR_FAIL_COND(!d.has("height"));
	_setup(d["height"], d["radius"]);
}

Variant GodotCapsuleShape3D::get_data() const {
	Dictionary d;
	d["radius"] = radius;
	d["height"] = height;
	return d;
}

GodotCapsuleShape3D::GodotCapsuleShape3D() {}

/********** CYLINDER *************/

void GodotCylinderShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	Vector3 cylinder_axis = p_transform.basis.get_column(1).normalized();
	real_t axis_dot = cylinder_axis.dot(p_normal);

	Vector3 local_normal = p_transform.basis.xform_inv(p_normal);
	real_t scale = local_normal.length();
	real_t scaled_radius = radius * scale;
	real_t scaled_height = height * scale;

	real_t length;
	if (Math::abs(axis_dot) > 1.0) {
		length = scaled_height * 0.5;
	} else {
		length = Math::abs(axis_dot * scaled_height * 0.5) + scaled_radius * Math::sqrt(1.0 - axis_dot * axis_dot);
	}

	real_t distance = p_normal.dot(p_transform.origin);

	r_min = distance - length;
	r_max = distance + length;
}

Vector3 GodotCylinderShape3D::get_support(const Vector3 &p_normal) const {
	Vector3 n = p_normal;
	real_t h = (n.y > 0) ? height : -height;
	real_t s = Math::sqrt(n.x * n.x + n.z * n.z);
	if (Math::is_zero_approx(s)) {
		n.x = radius;
		n.y = h * 0.5;
		n.z = 0.0;
	} else {
		real_t d = radius / s;
		n.x = n.x * d;
		n.y = h * 0.5;
		n.z = n.z * d;
	}

	return n;
}

void GodotCylinderShape3D::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount, FeatureType &r_type) const {
	real_t d = p_normal.y;
	if (Math::abs(d) > cylinder_face_support_threshold) {
		real_t h = (d > 0) ? height : -height;

		Vector3 n = p_normal;
		n.x = 0.0;
		n.z = 0.0;
		n.y = h * 0.5;

		r_amount = 3;
		r_type = FEATURE_CIRCLE;
		r_supports[0] = n;
		r_supports[1] = n;
		r_supports[1].x += radius;
		r_supports[2] = n;
		r_supports[2].z += radius;
	} else if (Math::abs(d) < cylinder_edge_support_threshold_lower) {
		// make it flat
		Vector3 n = p_normal;
		n.y = 0.0;
		n.normalize();
		n *= radius;

		r_amount = 2;
		r_type = FEATURE_EDGE;
		r_supports[0] = n;
		r_supports[0].y += height * 0.5;
		r_supports[1] = n;
		r_supports[1].y -= height * 0.5;
	} else {
		r_amount = 1;
		r_type = FEATURE_POINT;
		r_supports[0] = get_support(p_normal);
	}
}

bool GodotCylinderShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	return Geometry3D::segment_intersects_cylinder(p_begin, p_end, height, radius, &r_result, &r_normal, 1);
}

bool GodotCylinderShape3D::intersect_point(const Vector3 &p_point) const {
	if (Math::abs(p_point.y) < height * 0.5) {
		return Vector3(p_point.x, 0, p_point.z).length() < radius;
	}
	return false;
}

Vector3 GodotCylinderShape3D::get_closest_point_to(const Vector3 &p_point) const {
	if (Math::absf(p_point.y) > height * 0.5) {
		// Project point to top disk.
		real_t dir = p_point.y > 0.0 ? 1.0 : -1.0;
		Vector3 circle_pos(0.0, dir * height * 0.5, 0.0);
		Plane circle_plane(Vector3(0.0, dir, 0.0), circle_pos);
		Vector3 proj_point = circle_plane.project(p_point);

		// Clip position.
		Vector3 delta_point_1 = proj_point - circle_pos;
		real_t dist_point_1 = delta_point_1.length_squared();
		if (!Math::is_zero_approx(dist_point_1)) {
			dist_point_1 = Math::sqrt(dist_point_1);
			proj_point = circle_pos + delta_point_1 * MIN(dist_point_1, radius) / dist_point_1;
		}

		return proj_point;
	} else {
		Vector3 s[2] = {
			Vector3(0, -height * 0.5, 0),
			Vector3(0, height * 0.5, 0),
		};

		Vector3 p = Geometry3D::get_closest_point_to_segment(p_point, s);

		if (p.distance_to(p_point) < radius) {
			return p_point;
		}

		return p + (p_point - p).normalized() * radius;
	}
}

Vector3 GodotCylinderShape3D::get_moment_of_inertia(real_t p_mass) const {
	// use bad AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.y * extents.y));
}

void GodotCylinderShape3D::_setup(real_t p_height, real_t p_radius) {
	height = p_height;
	radius = p_radius;
	configure(AABB(Vector3(-radius, -height * 0.5, -radius), Vector3(radius * 2.0, height, radius * 2.0)));
}

void GodotCylinderShape3D::set_data(const Variant &p_data) {
	Dictionary d = p_data;
	ERR_FAIL_COND(!d.has("radius"));
	ERR_FAIL_COND(!d.has("height"));
	_setup(d["height"], d["radius"]);
}

Variant GodotCylinderShape3D::get_data() const {
	Dictionary d;
	d["radius"] = radius;
	d["height"] = height;
	return d;
}

GodotCylinderShape3D::GodotCylinderShape3D() {}

/********** CONVEX POLYGON *************/

void GodotConvexPolygonShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	uint32_t vertex_count = mesh.vertices.size();
	if (vertex_count == 0) {
		return;
	}

	const Vector3 *vrts = &mesh.vertices[0];

	if (vertex_count > 3 * extreme_vertices.size()) {
		// For a large mesh, two calls to get_support() is faster than a full
		// scan over all vertices.

		Vector3 n = p_transform.basis.xform_inv(p_normal).normalized();
		r_min = p_normal.dot(p_transform.xform(get_support(-n)));
		r_max = p_normal.dot(p_transform.xform(get_support(n)));
	} else {
		for (uint32_t i = 0; i < vertex_count; i++) {
			real_t d = p_normal.dot(p_transform.xform(vrts[i]));

			if (i == 0 || d > r_max) {
				r_max = d;
			}
			if (i == 0 || d < r_min) {
				r_min = d;
			}
		}
	}
}

Vector3 GodotConvexPolygonShape3D::get_support(const Vector3 &p_normal) const {
	// Skip if there are no vertices in the mesh
	if (mesh.vertices.size() == 0) {
		return Vector3();
	}

	// Get the array of vertices
	const Vector3 *const vertices_array = mesh.vertices.ptr();

	// Start with an initial assumption of the first extreme vertex.
	int best_vertex = extreme_vertices[0];
	real_t max_support = p_normal.dot(vertices_array[best_vertex]);

	// Check the remaining extreme vertices for a better vertex.
	for (const int &vert : extreme_vertices) {
		real_t s = p_normal.dot(vertices_array[vert]);
		if (s > max_support) {
			best_vertex = vert;
			max_support = s;
		}
	}

	// If we checked all vertices in the mesh then we're done.
	if (extreme_vertices.size() == mesh.vertices.size()) {
		return vertices_array[best_vertex];
	}

	// Move along the surface until we reach the true support vertex.
	int last_vertex = -1;
	while (true) {
		int next_vertex = -1;

		// Iterate over all the neighbors checking for a better vertex.
		for (const int &vert : vertex_neighbors[best_vertex]) {
			if (vert != last_vertex) {
				real_t s = p_normal.dot(vertices_array[vert]);
				if (s > max_support) {
					next_vertex = vert;
					max_support = s;
					break;
				}
			}
		}

		// No better vertex found, we have the best
		if (next_vertex == -1) {
			return vertices_array[best_vertex];
		}

		// Move to the better vertex and try again
		last_vertex = best_vertex;
		best_vertex = next_vertex;
	}
}

void GodotConvexPolygonShape3D::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount, FeatureType &r_type) const {
	const Geometry3D::MeshData::Face *faces = mesh.faces.ptr();
	int fc = mesh.faces.size();

	const Geometry3D::MeshData::Edge *edges = mesh.edges.ptr();
	int ec = mesh.edges.size();

	const Vector3 *vertices = mesh.vertices.ptr();
	int vc = mesh.vertices.size();

	r_amount = 0;
	ERR_FAIL_COND_MSG(vc == 0, "Convex polygon shape has no vertices.");

	//find vertex first
	real_t max = 0;
	int vtx = 0;

	for (int i = 0; i < vc; i++) {
		real_t d = p_normal.dot(vertices[i]);

		if (i == 0 || d > max) {
			max = d;
			vtx = i;
		}
	}

	for (int i = 0; i < fc; i++) {
		if (faces[i].plane.normal.dot(p_normal) > face_support_threshold) {
			int ic = faces[i].indices.size();
			const int *ind = faces[i].indices.ptr();

			bool valid = false;
			for (int j = 0; j < ic; j++) {
				if (ind[j] == vtx) {
					valid = true;
					break;
				}
			}

			if (!valid) {
				continue;
			}

			int m = MIN(p_max, ic);
			for (int j = 0; j < m; j++) {
				r_supports[j] = vertices[ind[j]];
			}
			r_amount = m;
			r_type = FEATURE_FACE;
			return;
		}
	}

	for (int i = 0; i < ec; i++) {
		real_t dot = (vertices[edges[i].vertex_a] - vertices[edges[i].vertex_b]).normalized().dot(p_normal);
		dot = ABS(dot);
		if (dot < edge_support_threshold_lower && (edges[i].vertex_a == vtx || edges[i].vertex_b == vtx)) {
			r_amount = 2;
			r_type = FEATURE_EDGE;
			r_supports[0] = vertices[edges[i].vertex_a];
			r_supports[1] = vertices[edges[i].vertex_b];
			return;
		}
	}

	r_supports[0] = vertices[vtx];
	r_amount = 1;
	r_type = FEATURE_POINT;
}

bool GodotConvexPolygonShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	const Geometry3D::MeshData::Face *faces = mesh.faces.ptr();
	int fc = mesh.faces.size();

	const Vector3 *vertices = mesh.vertices.ptr();

	Vector3 n = p_end - p_begin;
	real_t min = 1e20;
	bool col = false;

	for (int i = 0; i < fc; i++) {
		if (faces[i].plane.normal.dot(n) > 0) {
			continue; //opposing face
		}

		int ic = faces[i].indices.size();
		const int *ind = faces[i].indices.ptr();

		for (int j = 1; j < ic - 1; j++) {
			Face3 f(vertices[ind[0]], vertices[ind[j]], vertices[ind[j + 1]]);
			Vector3 result;
			if (f.intersects_segment(p_begin, p_end, &result)) {
				real_t d = n.dot(result);
				if (d < min) {
					min = d;
					r_result = result;
					r_normal = faces[i].plane.normal;
					col = true;
				}

				break;
			}
		}
	}

	return col;
}

bool GodotConvexPolygonShape3D::intersect_point(const Vector3 &p_point) const {
	const Geometry3D::MeshData::Face *faces = mesh.faces.ptr();
	int fc = mesh.faces.size();

	for (int i = 0; i < fc; i++) {
		if (faces[i].plane.distance_to(p_point) >= 0) {
			return false;
		}
	}

	return true;
}

Vector3 GodotConvexPolygonShape3D::get_closest_point_to(const Vector3 &p_point) const {
	const Geometry3D::MeshData::Face *faces = mesh.faces.ptr();
	int fc = mesh.faces.size();
	const Vector3 *vertices = mesh.vertices.ptr();

	bool all_inside = true;
	for (int i = 0; i < fc; i++) {
		if (!faces[i].plane.is_point_over(p_point)) {
			continue;
		}

		all_inside = false;
		bool is_inside = true;
		int ic = faces[i].indices.size();
		const int *indices = faces[i].indices.ptr();

		for (int j = 0; j < ic; j++) {
			Vector3 a = vertices[indices[j]];
			Vector3 b = vertices[indices[(j + 1) % ic]];
			Vector3 n = (a - b).cross(faces[i].plane.normal).normalized();
			if (Plane(n, a).is_point_over(p_point)) {
				is_inside = false;
				break;
			}
		}

		if (is_inside) {
			return faces[i].plane.project(p_point);
		}
	}

	if (all_inside) {
		return p_point;
	}

	real_t min_distance = 1e20;
	Vector3 min_point;

	//check edges
	const Geometry3D::MeshData::Edge *edges = mesh.edges.ptr();
	int ec = mesh.edges.size();
	for (int i = 0; i < ec; i++) {
		Vector3 s[2] = {
			vertices[edges[i].vertex_a],
			vertices[edges[i].vertex_b]
		};

		Vector3 closest = Geometry3D::get_closest_point_to_segment(p_point, s);
		real_t d = closest.distance_to(p_point);
		if (d < min_distance) {
			min_distance = d;
			min_point = closest;
		}
	}

	return min_point;
}

Vector3 GodotConvexPolygonShape3D::get_moment_of_inertia(real_t p_mass) const {
	// use bad AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.y * extents.y));
}

void GodotConvexPolygonShape3D::_setup(const Vector<Vector3> &p_vertices) {
	Error err = ConvexHullComputer::convex_hull(p_vertices, mesh);
	if (err != OK) {
		ERR_PRINT("Failed to build convex hull");
	}
	extreme_vertices.resize(0);
	vertex_neighbors.resize(0);

	AABB _aabb;

	for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
		if (i == 0) {
			_aabb.position = mesh.vertices[i];
		} else {
			_aabb.expand_to(mesh.vertices[i]);
		}
	}

	configure(_aabb);

	// Pre-compute the extreme vertices in 26 directions.  This will be used
	// to speed up get_support() by letting us quickly get a good guess for
	// the support vertex.

	for (int x = -1; x < 2; x++) {
		for (int y = -1; y < 2; y++) {
			for (int z = -1; z < 2; z++) {
				if (x != 0 || y != 0 || z != 0) {
					Vector3 dir(x, y, z);
					dir.normalize();
					real_t max_support = 0.0;
					int best_vertex = -1;
					for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
						real_t s = dir.dot(mesh.vertices[i]);
						if (best_vertex == -1 || s > max_support) {
							best_vertex = i;
							max_support = s;
						}
					}
					if (!extreme_vertices.has(best_vertex)) {
						extreme_vertices.push_back(best_vertex);
					}
				}
			}
		}
	}

	// Record all the neighbors of each vertex.  This is used in get_support().

	if (extreme_vertices.size() < mesh.vertices.size()) {
		vertex_neighbors.resize(mesh.vertices.size());
		for (Geometry3D::MeshData::Edge &edge : mesh.edges) {
			vertex_neighbors[edge.vertex_a].push_back(edge.vertex_b);
			vertex_neighbors[edge.vertex_b].push_back(edge.vertex_a);
		}
	}
}

void GodotConvexPolygonShape3D::set_data(const Variant &p_data) {
	_setup(p_data);
}

Variant GodotConvexPolygonShape3D::get_data() const {
	Vector<Vector3> vertices;
	vertices.resize(mesh.vertices.size());
	for (uint32_t i = 0; i < mesh.vertices.size(); i++) {
		vertices.write[i] = mesh.vertices[i];
	}
	return vertices;
}

GodotConvexPolygonShape3D::GodotConvexPolygonShape3D() {
}

/********** FACE POLYGON *************/

void GodotFaceShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	for (int i = 0; i < 3; i++) {
		Vector3 v = p_transform.xform(vertex[i]);
		real_t d = p_normal.dot(v);

		if (i == 0 || d > r_max) {
			r_max = d;
		}

		if (i == 0 || d < r_min) {
			r_min = d;
		}
	}
}

Vector3 GodotFaceShape3D::get_support(const Vector3 &p_normal) const {
	int vert_support_idx = -1;
	real_t support_max = 0;

	for (int i = 0; i < 3; i++) {
		real_t d = p_normal.dot(vertex[i]);

		if (i == 0 || d > support_max) {
			support_max = d;
			vert_support_idx = i;
		}
	}

	return vertex[vert_support_idx];
}

void GodotFaceShape3D::get_supports(const Vector3 &p_normal, int p_max, Vector3 *r_supports, int &r_amount, FeatureType &r_type) const {
	Vector3 n = p_normal;

	/** TEST FACE AS SUPPORT **/
	if (Math::abs(normal.dot(n)) > face_support_threshold) {
		r_amount = 3;
		r_type = FEATURE_FACE;
		for (int i = 0; i < 3; i++) {
			r_supports[i] = vertex[i];
		}
		return;
	}

	/** FIND SUPPORT VERTEX **/

	int vert_support_idx = -1;
	real_t support_max = 0;

	for (int i = 0; i < 3; i++) {
		real_t d = n.dot(vertex[i]);

		if (i == 0 || d > support_max) {
			support_max = d;
			vert_support_idx = i;
		}
	}

	/** TEST EDGES AS SUPPORT **/

	for (int i = 0; i < 3; i++) {
		int nx = (i + 1) % 3;
		if (i != vert_support_idx && nx != vert_support_idx) {
			continue;
		}

		// check if edge is valid as a support
		real_t dot = (vertex[i] - vertex[nx]).normalized().dot(n);
		dot = ABS(dot);
		if (dot < edge_support_threshold_lower) {
			r_amount = 2;
			r_type = FEATURE_EDGE;
			r_supports[0] = vertex[i];
			r_supports[1] = vertex[nx];
			return;
		}
	}

	r_amount = 1;
	r_type = FEATURE_POINT;
	r_supports[0] = vertex[vert_support_idx];
}

bool GodotFaceShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	bool c = Geometry3D::segment_intersects_triangle(p_begin, p_end, vertex[0], vertex[1], vertex[2], &r_result);
	if (c) {
		r_normal = Plane(vertex[0], vertex[1], vertex[2]).normal;
		if (r_normal.dot(p_end - p_begin) > 0) {
			if (backface_collision && p_hit_back_faces) {
				r_normal = -r_normal;
			} else {
				c = false;
			}
		}
	}

	return c;
}

bool GodotFaceShape3D::intersect_point(const Vector3 &p_point) const {
	return false; //face is flat
}

Vector3 GodotFaceShape3D::get_closest_point_to(const Vector3 &p_point) const {
	return Face3(vertex[0], vertex[1], vertex[2]).get_closest_point_to(p_point);
}

Vector3 GodotFaceShape3D::get_moment_of_inertia(real_t p_mass) const {
	return Vector3(); // Sorry, but i don't think anyone cares, FaceShape!
}

GodotFaceShape3D::GodotFaceShape3D() {
	configure(AABB());
}

Vector<Vector3> GodotConcavePolygonShape3D::get_faces() const {
	Vector<Vector3> rfaces;
	rfaces.resize(faces.size() * 3);

	for (int i = 0; i < faces.size(); i++) {
		Face f = faces.get(i);

		for (int j = 0; j < 3; j++) {
			rfaces.set(i * 3 + j, vertices.get(f.indices[j]));
		}
	}

	return rfaces;
}

void GodotConcavePolygonShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	int count = vertices.size();
	if (count == 0) {
		r_min = 0;
		r_max = 0;
		return;
	}
	const Vector3 *vptr = vertices.ptr();

	for (int i = 0; i < count; i++) {
		real_t d = p_normal.dot(p_transform.xform(vptr[i]));

		if (i == 0 || d > r_max) {
			r_max = d;
		}
		if (i == 0 || d < r_min) {
			r_min = d;
		}
	}
}

Vector3 GodotConcavePolygonShape3D::get_support(const Vector3 &p_normal) const {
	int count = vertices.size();
	if (count == 0) {
		return Vector3();
	}

	const Vector3 *vptr = vertices.ptr();

	Vector3 n = p_normal;

	int vert_support_idx = -1;
	real_t support_max = 0;

	for (int i = 0; i < count; i++) {
		real_t d = n.dot(vptr[i]);

		if (i == 0 || d > support_max) {
			support_max = d;
			vert_support_idx = i;
		}
	}

	return vptr[vert_support_idx];
}

void GodotConcavePolygonShape3D::_cull_segment(int p_idx, _SegmentCullParams *p_params) const {
	const BVH *params_bvh = &p_params->bvh[p_idx];

	if (!params_bvh->aabb.intersects_segment(p_params->from, p_params->to)) {
		return;
	}

	if (params_bvh->face_index >= 0) {
		const Face *f = &p_params->faces[params_bvh->face_index];
		GodotFaceShape3D *face = p_params->face;
		face->normal = f->normal;
		face->vertex[0] = p_params->vertices[f->indices[0]];
		face->vertex[1] = p_params->vertices[f->indices[1]];
		face->vertex[2] = p_params->vertices[f->indices[2]];

		Vector3 res;
		Vector3 normal;
		int face_index = params_bvh->face_index;
		if (face->intersect_segment(p_params->from, p_params->to, res, normal, face_index, true)) {
			real_t d = p_params->dir.dot(res) - p_params->dir.dot(p_params->from);
			if ((d > 0) && (d < p_params->min_d)) {
				p_params->min_d = d;
				p_params->result = res;
				p_params->normal = normal;
				p_params->face_index = face_index;
				p_params->collisions++;
			}
		}
	} else {
		if (params_bvh->left >= 0) {
			_cull_segment(params_bvh->left, p_params);
		}
		if (params_bvh->right >= 0) {
			_cull_segment(params_bvh->right, p_params);
		}
	}
}

bool GodotConcavePolygonShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_result, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	if (faces.size() == 0) {
		return false;
	}

	// unlock data
	const Face *fr = faces.ptr();
	const Vector3 *vr = vertices.ptr();
	const BVH *br = bvh.ptr();

	GodotFaceShape3D face;
	face.backface_collision = backface_collision && p_hit_back_faces;

	_SegmentCullParams params;
	params.from = p_begin;
	params.to = p_end;
	params.dir = (p_end - p_begin).normalized();

	params.faces = fr;
	params.vertices = vr;
	params.bvh = br;

	params.face = &face;

	// cull
	_cull_segment(0, &params);

	if (params.collisions > 0) {
		r_result = params.result;
		r_normal = params.normal;
		r_face_index = params.face_index;
		return true;
	} else {
		return false;
	}
}

bool GodotConcavePolygonShape3D::intersect_point(const Vector3 &p_point) const {
	return false; //face is flat
}

Vector3 GodotConcavePolygonShape3D::get_closest_point_to(const Vector3 &p_point) const {
	return Vector3();
}

bool GodotConcavePolygonShape3D::_cull(int p_idx, _CullParams *p_params) const {
	const BVH *params_bvh = &p_params->bvh[p_idx];

	if (!p_params->aabb.intersects(params_bvh->aabb)) {
		return false;
	}

	if (params_bvh->face_index >= 0) {
		const Face *f = &p_params->faces[params_bvh->face_index];
		GodotFaceShape3D *face = p_params->face;
		face->normal = f->normal;
		face->vertex[0] = p_params->vertices[f->indices[0]];
		face->vertex[1] = p_params->vertices[f->indices[1]];
		face->vertex[2] = p_params->vertices[f->indices[2]];
		if (p_params->callback(p_params->userdata, face)) {
			return true;
		}
	} else {
		if (params_bvh->left >= 0) {
			if (_cull(params_bvh->left, p_params)) {
				return true;
			}
		}

		if (params_bvh->right >= 0) {
			if (_cull(params_bvh->right, p_params)) {
				return true;
			}
		}
	}

	return false;
}

void GodotConcavePolygonShape3D::cull(const AABB &p_local_aabb, QueryCallback p_callback, void *p_userdata, bool p_invert_backface_collision) const {
	// make matrix local to concave
	if (faces.size() == 0) {
		return;
	}

	AABB local_aabb = p_local_aabb;

	// unlock data
	const Face *fr = faces.ptr();
	const Vector3 *vr = vertices.ptr();
	const BVH *br = bvh.ptr();

	GodotFaceShape3D face; // use this to send in the callback
	face.backface_collision = backface_collision;
	face.invert_backface_collision = p_invert_backface_collision;

	_CullParams params;
	params.aabb = local_aabb;
	params.face = &face;
	params.faces = fr;
	params.vertices = vr;
	params.bvh = br;
	params.callback = p_callback;
	params.userdata = p_userdata;

	// cull
	_cull(0, &params);
}

Vector3 GodotConcavePolygonShape3D::get_moment_of_inertia(real_t p_mass) const {
	// use bad AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.y * extents.y));
}

struct _Volume_BVH_Element {
	AABB aabb;
	Vector3 center;
	int face_index = 0;
};

struct _Volume_BVH_CompareX {
	_FORCE_INLINE_ bool operator()(const _Volume_BVH_Element &a, const _Volume_BVH_Element &b) const {
		return a.center.x < b.center.x;
	}
};

struct _Volume_BVH_CompareY {
	_FORCE_INLINE_ bool operator()(const _Volume_BVH_Element &a, const _Volume_BVH_Element &b) const {
		return a.center.y < b.center.y;
	}
};

struct _Volume_BVH_CompareZ {
	_FORCE_INLINE_ bool operator()(const _Volume_BVH_Element &a, const _Volume_BVH_Element &b) const {
		return a.center.z < b.center.z;
	}
};

struct _Volume_BVH {
	AABB aabb;
	_Volume_BVH *left = nullptr;
	_Volume_BVH *right = nullptr;

	int face_index = 0;
};

_Volume_BVH *_volume_build_bvh(_Volume_BVH_Element *p_elements, int p_size, int &count) {
	_Volume_BVH *bvh = memnew(_Volume_BVH);

	if (p_size == 1) {
		//leaf
		bvh->aabb = p_elements[0].aabb;
		bvh->left = nullptr;
		bvh->right = nullptr;
		bvh->face_index = p_elements->face_index;
		count++;
		return bvh;
	} else {
		bvh->face_index = -1;
	}

	AABB aabb;
	for (int i = 0; i < p_size; i++) {
		if (i == 0) {
			aabb = p_elements[i].aabb;
		} else {
			aabb.merge_with(p_elements[i].aabb);
		}
	}
	bvh->aabb = aabb;
	switch (aabb.get_longest_axis_index()) {
		case 0: {
			SortArray<_Volume_BVH_Element, _Volume_BVH_CompareX> sort_x;
			sort_x.sort(p_elements, p_size);

		} break;
		case 1: {
			SortArray<_Volume_BVH_Element, _Volume_BVH_CompareY> sort_y;
			sort_y.sort(p_elements, p_size);
		} break;
		case 2: {
			SortArray<_Volume_BVH_Element, _Volume_BVH_CompareZ> sort_z;
			sort_z.sort(p_elements, p_size);
		} break;
	}

	int split = p_size / 2;
	bvh->left = _volume_build_bvh(p_elements, split, count);
	bvh->right = _volume_build_bvh(&p_elements[split], p_size - split, count);

	//printf("branch at %p - %i: %i\n",bvh,count,bvh->face_index);
	count++;
	return bvh;
}

void GodotConcavePolygonShape3D::_fill_bvh(_Volume_BVH *p_bvh_tree, BVH *p_bvh_array, int &p_idx) {
	int idx = p_idx;

	p_bvh_array[idx].aabb = p_bvh_tree->aabb;
	p_bvh_array[idx].face_index = p_bvh_tree->face_index;
	//printf("%p - %i: %i(%p)  -- %p:%p\n",%p_bvh_array[idx],p_idx,p_bvh_array[i]->face_index,&p_bvh_tree->face_index,p_bvh_tree->left,p_bvh_tree->right);

	if (p_bvh_tree->left) {
		p_bvh_array[idx].left = ++p_idx;
		_fill_bvh(p_bvh_tree->left, p_bvh_array, p_idx);

	} else {
		p_bvh_array[p_idx].left = -1;
	}

	if (p_bvh_tree->right) {
		p_bvh_array[idx].right = ++p_idx;
		_fill_bvh(p_bvh_tree->right, p_bvh_array, p_idx);

	} else {
		p_bvh_array[p_idx].right = -1;
	}

	memdelete(p_bvh_tree);
}

void GodotConcavePolygonShape3D::_setup(const Vector<Vector3> &p_faces, bool p_backface_collision) {
	int src_face_count = p_faces.size();
	if (src_face_count == 0) {
		configure(AABB());
		return;
	}
	ERR_FAIL_COND(src_face_count % 3);
	src_face_count /= 3;

	const Vector3 *facesr = p_faces.ptr();

	Vector<_Volume_BVH_Element> bvh_array;
	bvh_array.resize(src_face_count);

	_Volume_BVH_Element *bvh_arrayw = bvh_array.ptrw();

	faces.resize(src_face_count);
	Face *facesw = faces.ptrw();

	vertices.resize(src_face_count * 3);

	Vector3 *verticesw = vertices.ptrw();

	AABB _aabb;

	for (int i = 0; i < src_face_count; i++) {
		Face3 face(facesr[i * 3 + 0], facesr[i * 3 + 1], facesr[i * 3 + 2]);

		bvh_arrayw[i].aabb = face.get_aabb();
		bvh_arrayw[i].center = bvh_arrayw[i].aabb.get_center();
		bvh_arrayw[i].face_index = i;
		facesw[i].indices[0] = i * 3 + 0;
		facesw[i].indices[1] = i * 3 + 1;
		facesw[i].indices[2] = i * 3 + 2;
		facesw[i].normal = face.get_plane().normal;
		verticesw[i * 3 + 0] = face.vertex[0];
		verticesw[i * 3 + 1] = face.vertex[1];
		verticesw[i * 3 + 2] = face.vertex[2];
		if (i == 0) {
			_aabb = bvh_arrayw[i].aabb;
		} else {
			_aabb.merge_with(bvh_arrayw[i].aabb);
		}
	}

	int count = 0;
	_Volume_BVH *bvh_tree = _volume_build_bvh(bvh_arrayw, src_face_count, count);

	bvh.resize(count + 1);

	BVH *bvh_arrayw2 = bvh.ptrw();

	int idx = 0;
	_fill_bvh(bvh_tree, bvh_arrayw2, idx);

	backface_collision = p_backface_collision;

	configure(_aabb); // this type of shape has no margin
}

void GodotConcavePolygonShape3D::set_data(const Variant &p_data) {
	Dictionary d = p_data;
	ERR_FAIL_COND(!d.has("faces"));

	_setup(d["faces"], d["backface_collision"]);
}

Variant GodotConcavePolygonShape3D::get_data() const {
	Dictionary d;
	d["faces"] = get_faces();
	d["backface_collision"] = backface_collision;

	return d;
}

GodotConcavePolygonShape3D::GodotConcavePolygonShape3D() {
}

/* HEIGHT MAP SHAPE */

Vector<real_t> GodotHeightMapShape3D::get_heights() const {
	return heights;
}

int GodotHeightMapShape3D::get_width() const {
	return width;
}

int GodotHeightMapShape3D::get_depth() const {
	return depth;
}

void GodotHeightMapShape3D::project_range(const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) const {
	//not very useful, but not very used either
	p_transform.xform(get_aabb()).project_range_in_plane(Plane(p_normal), r_min, r_max);
}

Vector3 GodotHeightMapShape3D::get_support(const Vector3 &p_normal) const {
	//not very useful, but not very used either
	return get_aabb().get_support(p_normal);
}

struct _HeightmapSegmentCullParams {
	Vector3 from;
	Vector3 to;
	Vector3 dir;

	Vector3 result;
	Vector3 normal;

	const GodotHeightMapShape3D *heightmap = nullptr;
	GodotFaceShape3D *face = nullptr;
};

struct _HeightmapGridCullState {
	real_t length = 0.0;
	real_t length_flat = 0.0;

	real_t dist = 0.0;
	real_t prev_dist = 0.0;

	int x = 0;
	int z = 0;
};

_FORCE_INLINE_ bool _heightmap_face_cull_segment(_HeightmapSegmentCullParams &p_params) {
	Vector3 res;
	Vector3 normal;
	int fi = -1;
	if (p_params.face->intersect_segment(p_params.from, p_params.to, res, normal, fi, true)) {
		p_params.result = res;
		p_params.normal = normal;

		return true;
	}

	return false;
}

_FORCE_INLINE_ bool _heightmap_cell_cull_segment(_HeightmapSegmentCullParams &p_params, const _HeightmapGridCullState &p_state) {
	// First triangle.
	p_params.heightmap->_get_point(p_state.x, p_state.z, p_params.face->vertex[0]);
	p_params.heightmap->_get_point(p_state.x + 1, p_state.z, p_params.face->vertex[1]);
	p_params.heightmap->_get_point(p_state.x, p_state.z + 1, p_params.face->vertex[2]);
	p_params.face->normal = Plane(p_params.face->vertex[0], p_params.face->vertex[1], p_params.face->vertex[2]).normal;
	if (_heightmap_face_cull_segment(p_params)) {
		return true;
	}

	// Second triangle.
	p_params.face->vertex[0] = p_params.face->vertex[1];
	p_params.heightmap->_get_point(p_state.x + 1, p_state.z + 1, p_params.face->vertex[1]);
	p_params.face->normal = Plane(p_params.face->vertex[0], p_params.face->vertex[1], p_params.face->vertex[2]).normal;
	if (_heightmap_face_cull_segment(p_params)) {
		return true;
	}

	return false;
}

_FORCE_INLINE_ bool _heightmap_chunk_cull_segment(_HeightmapSegmentCullParams &p_params, const _HeightmapGridCullState &p_state) {
	const GodotHeightMapShape3D::Range &chunk = p_params.heightmap->_get_bounds_chunk(p_state.x, p_state.z);

	Vector3 enter_pos;
	Vector3 exit_pos;

	if (p_state.length_flat > CMP_EPSILON) {
		real_t flat_to_3d = p_state.length / p_state.length_flat;
		real_t enter_param = p_state.prev_dist * flat_to_3d;
		real_t exit_param = p_state.dist * flat_to_3d;
		enter_pos = p_params.from + p_params.dir * enter_param;
		exit_pos = p_params.from + p_params.dir * exit_param;
	} else {
		// Consider the ray vertical.
		// (though we shouldn't reach this often because there is an early check up-front)
		enter_pos = p_params.from;
		exit_pos = p_params.to;
	}

	// Transform positions to heightmap space.
	enter_pos *= GodotHeightMapShape3D::BOUNDS_CHUNK_SIZE;
	exit_pos *= GodotHeightMapShape3D::BOUNDS_CHUNK_SIZE;

	// We did enter the flat projection of the AABB,
	// but we have to check if we intersect it on the vertical axis.
	if ((enter_pos.y > chunk.max) && (exit_pos.y > chunk.max)) {
		return false;
	}
	if ((enter_pos.y < chunk.min) && (exit_pos.y < chunk.min)) {
		return false;
	}

	return p_params.heightmap->_intersect_grid_segment(_heightmap_cell_cull_segment, enter_pos, exit_pos, p_params.heightmap->width, p_params.heightmap->depth, p_params.heightmap->local_origin, p_params.result, p_params.normal);
}

template <typename ProcessFunction>
bool GodotHeightMapShape3D::_intersect_grid_segment(ProcessFunction &p_process, const Vector3 &p_begin, const Vector3 &p_end, int p_width, int p_depth, const Vector3 &offset, Vector3 &r_point, Vector3 &r_normal) const {
	Vector3 delta = (p_end - p_begin);
	real_t length = delta.length();

	if (length < CMP_EPSILON) {
		return false;
	}

	Vector3 local_begin = p_begin + offset;

	GodotFaceShape3D face;
	face.backface_collision = false;

	_HeightmapSegmentCullParams params;
	params.from = p_begin;
	params.to = p_end;
	params.dir = delta / length;
	params.heightmap = this;
	params.face = &face;

	_HeightmapGridCullState state;

	// Perform grid query from projected ray.
	Vector2 ray_dir_flat(delta.x, delta.z);
	state.length = length;
	state.length_flat = ray_dir_flat.length();

	if (state.length_flat < CMP_EPSILON) {
		ray_dir_flat = Vector2();
	} else {
		ray_dir_flat /= state.length_flat;
	}

	const int x_step = (ray_dir_flat.x > CMP_EPSILON) ? 1 : ((ray_dir_flat.x < -CMP_EPSILON) ? -1 : 0);
	const int z_step = (ray_dir_flat.y > CMP_EPSILON) ? 1 : ((ray_dir_flat.y < -CMP_EPSILON) ? -1 : 0);

	const real_t infinite = 1e20;
	const real_t delta_x = (x_step != 0) ? 1.f / Math::abs(ray_dir_flat.x) : infinite;
	const real_t delta_z = (z_step != 0) ? 1.f / Math::abs(ray_dir_flat.y) : infinite;

	real_t cross_x; // At which value of `param` we will cross a x-axis lane?
	real_t cross_z; // At which value of `param` we will cross a z-axis lane?

	// X initialization.
	if (x_step != 0) {
		if (x_step == 1) {
			cross_x = (Math::ceil(local_begin.x) - local_begin.x) * delta_x;
		} else {
			cross_x = (local_begin.x - Math::floor(local_begin.x)) * delta_x;
		}
	} else {
		cross_x = infinite; // Will never cross on X.
	}

	// Z initialization.
	if (z_step != 0) {
		if (z_step == 1) {
			cross_z = (Math::ceil(local_begin.z) - local_begin.z) * delta_z;
		} else {
			cross_z = (local_begin.z - Math::floor(local_begin.z)) * delta_z;
		}
	} else {
		cross_z = infinite; // Will never cross on Z.
	}

	int x = Math::floor(local_begin.x);
	int z = Math::floor(local_begin.z);

	// Workaround cases where the ray starts at an integer position.
	if (Math::is_zero_approx(cross_x)) {
		cross_x += delta_x;
		// If going backwards, we should ignore the position we would get by the above flooring,
		// because the ray is not heading in that direction.
		if (x_step == -1) {
			x -= 1;
		}
	}

	if (Math::is_zero_approx(cross_z)) {
		cross_z += delta_z;
		if (z_step == -1) {
			z -= 1;
		}
	}

	// Start inside the grid.
	int x_start = MAX(MIN(x, p_width - 2), 0);
	int z_start = MAX(MIN(z, p_depth - 2), 0);

	// Adjust initial cross values.
	cross_x += delta_x * x_step * (x_start - x);
	cross_z += delta_z * z_step * (z_start - z);

	x = x_start;
	z = z_start;

	while (true) {
		state.prev_dist = state.dist;
		state.x = x;
		state.z = z;

		if (cross_x < cross_z) {
			// X lane.
			x += x_step;
			// Assign before advancing the param,
			// to be in sync with the initialization step.
			state.dist = cross_x;
			cross_x += delta_x;
		} else {
			// Z lane.
			z += z_step;
			state.dist = cross_z;
			cross_z += delta_z;
		}

		if (state.dist > state.length_flat) {
			state.dist = state.length_flat;
			if (p_process(params, state)) {
				r_point = params.result;
				r_normal = params.normal;
				return true;
			}
			break;
		}

		if (p_process(params, state)) {
			r_point = params.result;
			r_normal = params.normal;
			return true;
		}

		// Stop when outside the grid.
		if ((x < 0) || (z < 0) || (x >= p_width - 1) || (z >= p_depth - 1)) {
			break;
		}
	}

	return false;
}

bool GodotHeightMapShape3D::intersect_segment(const Vector3 &p_begin, const Vector3 &p_end, Vector3 &r_point, Vector3 &r_normal, int &r_face_index, bool p_hit_back_faces) const {
	if (heights.is_empty()) {
		return false;
	}

	Vector3 local_begin = p_begin + local_origin;
	Vector3 local_end = p_end + local_origin;

	// Quantize the ray begin/end.
	int begin_x = Math::floor(local_begin.x);
	int begin_z = Math::floor(local_begin.z);
	int end_x = Math::floor(local_end.x);
	int end_z = Math::floor(local_end.z);

	if ((begin_x == end_x) && (begin_z == end_z)) {
		// Simple case for rays that don't traverse the grid horizontally.
		// Just perform a test on the given cell.
		GodotFaceShape3D face;
		face.backface_collision = p_hit_back_faces;

		_HeightmapSegmentCullParams params;
		params.from = p_begin;
		params.to = p_end;
		params.dir = (p_end - p_begin).normalized();

		params.heightmap = this;
		params.face = &face;

		_HeightmapGridCullState state;
		state.x = MAX(MIN(begin_x, width - 2), 0);
		state.z = MAX(MIN(begin_z, depth - 2), 0);
		if (_heightmap_cell_cull_segment(params, state)) {
			r_point = params.result;
			r_normal = params.normal;
			return true;
		}
	} else if (bounds_grid.is_empty()) {
		// Process all cells intersecting the flat projection of the ray.
		return _intersect_grid_segment(_heightmap_cell_cull_segment, p_begin, p_end, width, depth, local_origin, r_point, r_normal);
	} else {
		Vector3 ray_diff = (p_end - p_begin);
		real_t length_flat_sqr = ray_diff.x * ray_diff.x + ray_diff.z * ray_diff.z;
		if (length_flat_sqr < BOUNDS_CHUNK_SIZE * BOUNDS_CHUNK_SIZE) {
			// Don't use chunks, the ray is too short in the plane.
			return _intersect_grid_segment(_heightmap_cell_cull_segment, p_begin, p_end, width, depth, local_origin, r_point, r_normal);
		} else {
			// The ray is long, run raycast on a higher-level grid.
			Vector3 bounds_from = p_begin / BOUNDS_CHUNK_SIZE;
			Vector3 bounds_to = p_end / BOUNDS_CHUNK_SIZE;
			Vector3 bounds_offset = local_origin / BOUNDS_CHUNK_SIZE;
			// Plus 1 here to width and depth of the chunk because _intersect_grid_segment() is used by cell level as well,
			// and in _intersect_grid_segment() the loop will exit 1 early because for cell point triangle lookup, it dose x + 1, z + 1 etc for the vertex.
			int bounds_width = bounds_grid_width + 1;
			int bounds_depth = bounds_grid_depth + 1;
			return _intersect_grid_segment(_heightmap_chunk_cull_segment, bounds_from, bounds_to, bounds_width, bounds_depth, bounds_offset, r_point, r_normal);
		}
	}

	return false;
}

bool GodotHeightMapShape3D::intersect_point(const Vector3 &p_point) const {
	return false;
}

Vector3 GodotHeightMapShape3D::get_closest_point_to(const Vector3 &p_point) const {
	return Vector3();
}

void GodotHeightMapShape3D::_get_cell(const Vector3 &p_point, int &r_x, int &r_y, int &r_z) const {
	const AABB &shape_aabb = get_aabb();

	Vector3 pos_local = shape_aabb.position + local_origin;

	Vector3 clamped_point(p_point);
	clamped_point = p_point.clamp(pos_local, pos_local + shape_aabb.size);

	r_x = (clamped_point.x < 0.0) ? (clamped_point.x - 0.5) : (clamped_point.x + 0.5);
	r_y = (clamped_point.y < 0.0) ? (clamped_point.y - 0.5) : (clamped_point.y + 0.5);
	r_z = (clamped_point.z < 0.0) ? (clamped_point.z - 0.5) : (clamped_point.z + 0.5);
}

void GodotHeightMapShape3D::cull(const AABB &p_local_aabb, QueryCallback p_callback, void *p_userdata, bool p_invert_backface_collision) const {
	if (heights.is_empty()) {
		return;
	}

	AABB local_aabb = p_local_aabb;
	local_aabb.position += local_origin;

	// Quantize the aabb, and adjust the start/end ranges.
	int aabb_min[3];
	int aabb_max[3];
	_get_cell(local_aabb.position, aabb_min[0], aabb_min[1], aabb_min[2]);
	_get_cell(local_aabb.position + local_aabb.size, aabb_max[0], aabb_max[1], aabb_max[2]);

	// Expand the min/max quantized values.
	// This is to catch the case where the input aabb falls between grid points.
	for (int i = 0; i < 3; ++i) {
		aabb_min[i]--;
		aabb_max[i]++;
	}

	int start_x = MAX(0, aabb_min[0]);
	int end_x = MIN(width - 1, aabb_max[0]);
	int start_z = MAX(0, aabb_min[2]);
	int end_z = MIN(depth - 1, aabb_max[2]);

	GodotFaceShape3D face;
	face.backface_collision = !p_invert_backface_collision;
	face.invert_backface_collision = p_invert_backface_collision;

	for (int z = start_z; z < end_z; z++) {
		for (int x = start_x; x < end_x; x++) {
			// First triangle.
			_get_point(x, z, face.vertex[0]);
			_get_point(x + 1, z, face.vertex[1]);
			_get_point(x, z + 1, face.vertex[2]);
			face.normal = Plane(face.vertex[0], face.vertex[1], face.vertex[2]).normal;
			if (p_callback(p_userdata, &face)) {
				return;
			}

			// Second triangle.
			face.vertex[0] = face.vertex[1];
			_get_point(x + 1, z + 1, face.vertex[1]);
			face.normal = Plane(face.vertex[0], face.vertex[1], face.vertex[2]).normal;
			if (p_callback(p_userdata, &face)) {
				return;
			}
		}
	}
}

Vector3 GodotHeightMapShape3D::get_moment_of_inertia(real_t p_mass) const {
	// use bad AABB approximation
	Vector3 extents = get_aabb().size * 0.5;

	return Vector3(
			(p_mass / 3.0) * (extents.y * extents.y + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.z * extents.z),
			(p_mass / 3.0) * (extents.x * extents.x + extents.y * extents.y));
}

void GodotHeightMapShape3D::_build_accelerator() {
	bounds_grid.clear();

	bounds_grid_width = width / BOUNDS_CHUNK_SIZE;
	bounds_grid_depth = depth / BOUNDS_CHUNK_SIZE;

	if (width % BOUNDS_CHUNK_SIZE > 0) {
		++bounds_grid_width; // In case terrain size isn't dividable by chunk size.
	}

	if (depth % BOUNDS_CHUNK_SIZE > 0) {
		++bounds_grid_depth;
	}

	uint32_t bound_grid_size = (uint32_t)(bounds_grid_width * bounds_grid_depth);

	if (bound_grid_size < 2) {
		// Grid is empty or just one chunk.
		return;
	}

	bounds_grid.resize(bound_grid_size);

	// Compute min and max height for all chunks.
	for (int cz = 0; cz < bounds_grid_depth; ++cz) {
		int z0 = cz * BOUNDS_CHUNK_SIZE;

		for (int cx = 0; cx < bounds_grid_width; ++cx) {
			int x0 = cx * BOUNDS_CHUNK_SIZE;

			Range r;

			r.min = _get_height(x0, z0);
			r.max = r.min;

			// Compute min and max height for this chunk.
			// We have to include one extra cell to account for neighbors.
			// Here is why:
			// Say we have a flat terrain, and a plateau that fits a chunk perfectly.
			//
			//   Left        Right
			// 0---0---0---1---1---1
			// |   |   |   |   |   |
			// 0---0---0---1---1---1
			// |   |   |   |   |   |
			// 0---0---0---1---1---1
			//           x
			//
			// If the AABB for the Left chunk did not share vertices with the Right,
			// then we would fail collision tests at x due to a gap.
			//
			int z_max = MIN(z0 + BOUNDS_CHUNK_SIZE + 1, depth);
			int x_max = MIN(x0 + BOUNDS_CHUNK_SIZE + 1, width);
			for (int z = z0; z < z_max; ++z) {
				for (int x = x0; x < x_max; ++x) {
					real_t height = _get_height(x, z);
					if (height < r.min) {
						r.min = height;
					} else if (height > r.max) {
						r.max = height;
					}
				}
			}

			bounds_grid[cx + cz * bounds_grid_width] = r;
		}
	}
}

void GodotHeightMapShape3D::_setup(const Vector<real_t> &p_heights, int p_width, int p_depth, real_t p_min_height, real_t p_max_height) {
	heights = p_heights;
	width = p_width;
	depth = p_depth;

	// Initialize aabb.
	AABB aabb_new;
	aabb_new.position = Vector3(0.0, p_min_height, 0.0);
	aabb_new.size = Vector3(p_width - 1, p_max_height - p_min_height, p_depth - 1);

	// Initialize origin as the aabb center.
	local_origin = aabb_new.position + 0.5 * aabb_new.size;
	local_origin.y = 0.0;

	aabb_new.position -= local_origin;

	_build_accelerator();

	configure(aabb_new);
}

void GodotHeightMapShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	Dictionary d = p_data;
	ERR_FAIL_COND(!d.has("width"));
	ERR_FAIL_COND(!d.has("depth"));
	ERR_FAIL_COND(!d.has("heights"));

	int width_new = d["width"];
	int depth_new = d["depth"];

	ERR_FAIL_COND(width_new <= 0.0);
	ERR_FAIL_COND(depth_new <= 0.0);

	Variant heights_variant = d["heights"];
	Vector<real_t> heights_buffer;
#ifdef REAL_T_IS_DOUBLE
	if (heights_variant.get_type() == Variant::PACKED_FLOAT64_ARRAY) {
#else
	if (heights_variant.get_type() == Variant::PACKED_FLOAT32_ARRAY) {
#endif
		// Ready-to-use heights can be passed.
		heights_buffer = heights_variant;
	} else if (heights_variant.get_type() == Variant::OBJECT) {
		// If an image is passed, we have to convert it.
		// This would be expensive to do with a script, so it's nice to have it here.
		Ref<Image> image = heights_variant;
		ERR_FAIL_COND(image.is_null());
		ERR_FAIL_COND(image->get_format() != Image::FORMAT_RF);

		PackedByteArray im_data = image->get_data();
		heights_buffer.resize(image->get_width() * image->get_height());

		real_t *w = heights_buffer.ptrw();
		real_t *rp = (real_t *)im_data.ptr();
		for (int i = 0; i < heights_buffer.size(); ++i) {
			w[i] = rp[i];
		}
	} else {
#ifdef REAL_T_IS_DOUBLE
		ERR_FAIL_MSG("Expected PackedFloat64Array or float Image.");
#else
		ERR_FAIL_MSG("Expected PackedFloat32Array or float Image.");
#endif
	}

	// Compute min and max heights or use precomputed values.
	real_t min_height = 0.0;
	real_t max_height = 0.0;
	if (d.has("min_height") && d.has("max_height")) {
		min_height = d["min_height"];
		max_height = d["max_height"];
	} else {
		int heights_size = heights.size();
		for (int i = 0; i < heights_size; ++i) {
			real_t h = heights[i];
			if (h < min_height) {
				min_height = h;
			} else if (h > max_height) {
				max_height = h;
			}
		}
	}

	ERR_FAIL_COND(min_height > max_height);

	ERR_FAIL_COND(heights_buffer.size() != (width_new * depth_new));

	// If specified, min and max height will be used as precomputed values.
	_setup(heights_buffer, width_new, depth_new, min_height, max_height);
}

Variant GodotHeightMapShape3D::get_data() const {
	Dictionary d;
	d["width"] = width;
	d["depth"] = depth;

	const AABB &shape_aabb = get_aabb();
	d["min_height"] = shape_aabb.position.y;
	d["max_height"] = shape_aabb.position.y + shape_aabb.size.y;

	d["heights"] = heights;

	return d;
}

GodotHeightMapShape3D::GodotHeightMapShape3D() {
}
