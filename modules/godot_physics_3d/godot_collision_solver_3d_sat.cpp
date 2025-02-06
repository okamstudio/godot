/**************************************************************************/
/*  godot_collision_solver_3d_sat.cpp                                     */
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

#include "godot_collision_solver_3d_sat.h"

#include "gjk_epa.h"

#include "core/math/geometry_3d.h"

#define fallback_collision_solver gjk_epa_calculate_penetration

#define _BACKFACE_NORMAL_THRESHOLD -0.0002

// Cylinder SAT analytic methods and face-circle contact points for cylinder-trimesh and cylinder-box collision are based on ODE colliders.

/*
 *	Cylinder-trimesh and Cylinder-box colliders by Alen Ladavac
 *   Ported to ODE by Nguyen Binh
 */

/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001-2003 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

struct _CollectorCallback {
	GodotCollisionSolver3D::CallbackResult callback = nullptr;
	void *userdata = nullptr;
	bool swap = false;
	bool collided = false;
	Vector3 normal;
	Vector3 *prev_axis = nullptr;

	_FORCE_INLINE_ void call(const Vector3 &p_point_A, const Vector3 &p_point_B, Vector3 p_normal) {
		if (p_normal.dot(p_point_B - p_point_A) < 0) {
			p_normal = -p_normal;
		}
		if (swap) {
			callback(p_point_B, 0, p_point_A, 0, -p_normal, userdata);
		} else {
			callback(p_point_A, 0, p_point_B, 0, p_normal, userdata);
		}
	}
};

using GenerateContactsFunc = void (*)(const Vector3 *, int, const Vector3 *, int, _CollectorCallback *);

static void _generate_contacts_point_point(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A != 1);
	ERR_FAIL_COND(p_point_count_B != 1);
#endif

	p_callback->call(*p_points_A, *p_points_B, p_callback->normal);
}

static void _generate_contacts_point_edge(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A != 1);
	ERR_FAIL_COND(p_point_count_B != 2);
#endif

	Vector3 closest_B = Geometry3D::get_closest_point_to_segment_uncapped(*p_points_A, p_points_B);
	p_callback->call(*p_points_A, closest_B, p_callback->normal);
}

static void _generate_contacts_point_face(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A != 1);
	ERR_FAIL_COND(p_point_count_B < 3);
#endif

	Plane plane(p_points_B[0], p_points_B[1], p_points_B[2]);
	Vector3 closest_B = plane.project(*p_points_A);
	p_callback->call(*p_points_A, closest_B, plane.get_normal());
}

static void _generate_contacts_point_circle(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A != 1);
	ERR_FAIL_COND(p_point_count_B != 3);
#endif

	Plane plane(p_points_B[0], p_points_B[1], p_points_B[2]);
	Vector3 closest_B = plane.project(*p_points_A);
	p_callback->call(*p_points_A, closest_B, plane.get_normal());
}

static void _generate_contacts_edge_edge(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A != 2);
	ERR_FAIL_COND(p_point_count_B != 2); // circle is actually a 4x3 matrix
#endif

	Vector3 rel_A = p_points_A[1] - p_points_A[0];
	Vector3 rel_B = p_points_B[1] - p_points_B[0];

	Vector3 c = rel_A.cross(rel_B).cross(rel_B);

	if (Math::is_zero_approx(rel_A.dot(c))) {
		// should handle somehow..
		//ERR_PRINT("TODO FIX");
		//return;

		Vector3 axis = rel_A.normalized(); //make an axis
		Vector3 base_A = p_points_A[0] - axis * axis.dot(p_points_A[0]);
		Vector3 base_B = p_points_B[0] - axis * axis.dot(p_points_B[0]);

		//sort all 4 points in axis
		real_t dvec[4] = { axis.dot(p_points_A[0]), axis.dot(p_points_A[1]), axis.dot(p_points_B[0]), axis.dot(p_points_B[1]) };

		SortArray<real_t> sa;
		sa.sort(dvec, 4);

		//use the middle ones as contacts
		p_callback->call(base_A + axis * dvec[1], base_B + axis * dvec[1], p_callback->normal);
		p_callback->call(base_A + axis * dvec[2], base_B + axis * dvec[2], p_callback->normal);

		return;
	}

	real_t d = (c.dot(p_points_B[0]) - p_points_A[0].dot(c)) / rel_A.dot(c);

	if (d < 0.0) {
		d = 0.0;
	} else if (d > 1.0) {
		d = 1.0;
	}

	Vector3 closest_A = p_points_A[0] + rel_A * d;
	Vector3 closest_B = Geometry3D::get_closest_point_to_segment_uncapped(closest_A, p_points_B);
	// The normal should be perpendicular to both edges.
	Vector3 normal = rel_A.cross(rel_B);
	real_t normal_len = normal.length();
	if (normal_len > 1e-3) {
		normal /= normal_len;
	} else {
		normal = p_callback->normal;
	}
	p_callback->call(closest_A, closest_B, normal);
}

static void _generate_contacts_edge_circle(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A != 2);
	ERR_FAIL_COND(p_point_count_B != 3);
#endif

	const Vector3 &circle_B_pos = p_points_B[0];
	Vector3 circle_B_line_1 = p_points_B[1] - circle_B_pos;
	Vector3 circle_B_line_2 = p_points_B[2] - circle_B_pos;

	real_t circle_B_radius = circle_B_line_1.length();
	Vector3 circle_B_normal = circle_B_line_1.cross(circle_B_line_2).normalized();

	Plane circle_plane(circle_B_normal, circle_B_pos);

	static const int max_clip = 2;
	Vector3 contact_points[max_clip];
	int num_points = 0;

	// Project edge point in circle plane.
	const Vector3 &edge_A_1 = p_points_A[0];
	Vector3 proj_point_1 = circle_plane.project(edge_A_1);

	Vector3 dist_vec = proj_point_1 - circle_B_pos;
	real_t dist_sq = dist_vec.length_squared();

	// Point 1 is inside disk, add as contact point.
	if (dist_sq <= circle_B_radius * circle_B_radius) {
		contact_points[num_points] = edge_A_1;
		++num_points;
	}

	const Vector3 &edge_A_2 = p_points_A[1];
	Vector3 proj_point_2 = circle_plane.project(edge_A_2);

	Vector3 dist_vec_2 = proj_point_2 - circle_B_pos;
	real_t dist_sq_2 = dist_vec_2.length_squared();

	// Point 2 is inside disk, add as contact point.
	if (dist_sq_2 <= circle_B_radius * circle_B_radius) {
		contact_points[num_points] = edge_A_2;
		++num_points;
	}

	if (num_points < 2) {
		Vector3 line_vec = proj_point_2 - proj_point_1;
		real_t line_length_sq = line_vec.length_squared();

		// Create a quadratic formula of the form ax^2 + bx + c = 0
		real_t a, b, c;

		a = line_length_sq;
		b = 2.0 * dist_vec.dot(line_vec);
		c = dist_sq - circle_B_radius * circle_B_radius;

		// Solve for t.
		real_t sqrtterm = b * b - 4.0 * a * c;

		// If the term we intend to square root is less than 0 then the answer won't be real,
		// so the line doesn't intersect.
		if (sqrtterm >= 0) {
			sqrtterm = Math::sqrt(sqrtterm);

			Vector3 edge_dir = edge_A_2 - edge_A_1;

			real_t fraction_1 = (-b - sqrtterm) / (2.0 * a);
			if ((fraction_1 > 0.0) && (fraction_1 < 1.0)) {
				Vector3 face_point_1 = edge_A_1 + fraction_1 * edge_dir;
				ERR_FAIL_COND(num_points >= max_clip);
				contact_points[num_points] = face_point_1;
				++num_points;
			}

			real_t fraction_2 = (-b + sqrtterm) / (2.0 * a);
			if ((fraction_2 > 0.0) && (fraction_2 < 1.0) && !Math::is_equal_approx(fraction_1, fraction_2)) {
				Vector3 face_point_2 = edge_A_1 + fraction_2 * edge_dir;
				ERR_FAIL_COND(num_points >= max_clip);
				contact_points[num_points] = face_point_2;
				++num_points;
			}
		}
	}

	// Generate contact points.
	for (int i = 0; i < num_points; i++) {
		const Vector3 &contact_point_A = contact_points[i];

		real_t d = circle_plane.distance_to(contact_point_A);
		Vector3 closest_B = contact_point_A - circle_plane.normal * d;

		if (p_callback->normal.dot(contact_point_A) >= p_callback->normal.dot(closest_B)) {
			continue;
		}

		p_callback->call(contact_point_A, closest_B, circle_plane.get_normal());
	}
}

static void _generate_contacts_face_face(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A < 2);
	ERR_FAIL_COND(p_point_count_B < 3);
#endif

	static const int max_clip = 32;

	Vector3 _clipbuf1[max_clip];
	Vector3 _clipbuf2[max_clip];
	Vector3 *clipbuf_src = _clipbuf1;
	Vector3 *clipbuf_dst = _clipbuf2;
	int clipbuf_len = p_point_count_A;

	// copy A points to clipbuf_src
	for (int i = 0; i < p_point_count_A; i++) {
		clipbuf_src[i] = p_points_A[i];
	}

	Plane plane_B(p_points_B[0], p_points_B[1], p_points_B[2]);

	// go through all of B points
	for (int i = 0; i < p_point_count_B; i++) {
		int i_n = (i + 1) % p_point_count_B;

		Vector3 edge0_B = p_points_B[i];
		Vector3 edge1_B = p_points_B[i_n];

		Vector3 clip_normal = (edge0_B - edge1_B).cross(plane_B.normal).normalized();
		// make a clip plane

		Plane clip(clip_normal, edge0_B);
		// avoid double clip if A is edge
		int dst_idx = 0;
		bool edge = clipbuf_len == 2;
		for (int j = 0; j < clipbuf_len; j++) {
			int j_n = (j + 1) % clipbuf_len;

			Vector3 edge0_A = clipbuf_src[j];
			Vector3 edge1_A = clipbuf_src[j_n];

			real_t dist0 = clip.distance_to(edge0_A);
			real_t dist1 = clip.distance_to(edge1_A);

			if (dist0 <= 0) { // behind plane

				ERR_FAIL_COND(dst_idx >= max_clip);
				clipbuf_dst[dst_idx++] = clipbuf_src[j];
			}

			// check for different sides and non coplanar
			//if ( (dist0*dist1) < -CMP_EPSILON && !(edge && j)) {
			if ((dist0 * dist1) < 0 && !(edge && j)) {
				// calculate intersection
				Vector3 rel = edge1_A - edge0_A;
				real_t den = clip.normal.dot(rel);
				real_t dist = -(clip.normal.dot(edge0_A) - clip.d) / den;
				Vector3 inters = edge0_A + rel * dist;

				ERR_FAIL_COND(dst_idx >= max_clip);
				clipbuf_dst[dst_idx] = inters;
				dst_idx++;
			}
		}

		clipbuf_len = dst_idx;
		SWAP(clipbuf_src, clipbuf_dst);
	}

	// generate contacts
	//Plane plane_A(p_points_A[0],p_points_A[1],p_points_A[2]);

	for (int i = 0; i < clipbuf_len; i++) {
		real_t d = plane_B.distance_to(clipbuf_src[i]);

		Vector3 closest_B = clipbuf_src[i] - plane_B.normal * d;

		if (p_callback->normal.dot(clipbuf_src[i]) >= p_callback->normal.dot(closest_B)) {
			continue;
		}

		p_callback->call(clipbuf_src[i], closest_B, plane_B.get_normal());
	}
}

static void _generate_contacts_face_circle(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A < 3);
	ERR_FAIL_COND(p_point_count_B != 3);
#endif

	const Vector3 &circle_B_pos = p_points_B[0];
	Vector3 circle_B_line_1 = p_points_B[1] - circle_B_pos;
	Vector3 circle_B_line_2 = p_points_B[2] - circle_B_pos;

	// Clip face with circle segments.
	static const int circle_segments = 8;
	Vector3 circle_points[circle_segments];

	real_t angle_delta = 2.0 * Math_PI / circle_segments;

	for (int i = 0; i < circle_segments; ++i) {
		Vector3 point_pos = circle_B_pos;
		point_pos += circle_B_line_1 * Math::cos(i * angle_delta);
		point_pos += circle_B_line_2 * Math::sin(i * angle_delta);
		circle_points[i] = point_pos;
	}

	_generate_contacts_face_face(p_points_A, p_point_count_A, circle_points, circle_segments, p_callback);

	// Clip face with circle plane.
	Vector3 circle_B_normal = circle_B_line_1.cross(circle_B_line_2).normalized();

	Plane circle_plane(circle_B_normal, circle_B_pos);

	static const int max_clip = 32;
	Vector3 contact_points[max_clip];
	int num_points = 0;

	for (int i = 0; i < p_point_count_A; i++) {
		int i_n = (i + 1) % p_point_count_A;

		const Vector3 &edge0_A = p_points_A[i];
		const Vector3 &edge1_A = p_points_A[i_n];

		real_t dist0 = circle_plane.distance_to(edge0_A);
		real_t dist1 = circle_plane.distance_to(edge1_A);

		// First point in front of plane, generate contact point.
		if (dist0 * circle_plane.d >= 0) {
			ERR_FAIL_COND(num_points >= max_clip);
			contact_points[num_points] = edge0_A;
			++num_points;
		}

		// Points on different sides, generate contact point.
		if (dist0 * dist1 < 0) {
			// calculate intersection
			Vector3 rel = edge1_A - edge0_A;
			real_t den = circle_plane.normal.dot(rel);
			real_t dist = -(circle_plane.normal.dot(edge0_A) - circle_plane.d) / den;
			Vector3 inters = edge0_A + rel * dist;

			ERR_FAIL_COND(num_points >= max_clip);
			contact_points[num_points] = inters;
			++num_points;
		}
	}

	// Generate contact points.
	for (int i = 0; i < num_points; i++) {
		const Vector3 &contact_point_A = contact_points[i];

		real_t d = circle_plane.distance_to(contact_point_A);
		Vector3 closest_B = contact_point_A - circle_plane.normal * d;

		if (p_callback->normal.dot(contact_point_A) >= p_callback->normal.dot(closest_B)) {
			continue;
		}

		p_callback->call(contact_point_A, closest_B, circle_plane.get_normal());
	}
}

static void _generate_contacts_circle_circle(const Vector3 *p_points_A, int p_point_count_A, const Vector3 *p_points_B, int p_point_count_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A != 3);
	ERR_FAIL_COND(p_point_count_B != 3);
#endif

	const Vector3 &circle_A_pos = p_points_A[0];
	Vector3 circle_A_line_1 = p_points_A[1] - circle_A_pos;
	Vector3 circle_A_line_2 = p_points_A[2] - circle_A_pos;

	real_t circle_A_radius = circle_A_line_1.length();
	Vector3 circle_A_normal = circle_A_line_1.cross(circle_A_line_2).normalized();

	const Vector3 &circle_B_pos = p_points_B[0];
	Vector3 circle_B_line_1 = p_points_B[1] - circle_B_pos;
	Vector3 circle_B_line_2 = p_points_B[2] - circle_B_pos;

	real_t circle_B_radius = circle_B_line_1.length();
	Vector3 circle_B_normal = circle_B_line_1.cross(circle_B_line_2).normalized();

	static const int max_clip = 4;
	Vector3 contact_points[max_clip];
	int num_points = 0;

	Vector3 centers_diff = circle_B_pos - circle_A_pos;
	Vector3 norm_proj = circle_A_normal.dot(centers_diff) * circle_A_normal;
	Vector3 comp_proj = centers_diff - norm_proj;
	real_t proj_dist = comp_proj.length();
	if (!Math::is_zero_approx(proj_dist)) {
		comp_proj /= proj_dist;
		if ((proj_dist > circle_A_radius - circle_B_radius) && (proj_dist > circle_B_radius - circle_A_radius)) {
			// Circles are overlapping, use the 2 points of intersection as contacts.
			real_t radius_a_sqr = circle_A_radius * circle_A_radius;
			real_t radius_b_sqr = circle_B_radius * circle_B_radius;
			real_t d_sqr = proj_dist * proj_dist;
			real_t s = (1.0 + (radius_a_sqr - radius_b_sqr) / d_sqr) * 0.5;
			real_t h = Math::sqrt(MAX(radius_a_sqr - d_sqr * s * s, 0.0));
			Vector3 midpoint = circle_A_pos + s * comp_proj * proj_dist;
			Vector3 h_vec = h * circle_A_normal.cross(comp_proj);

			Vector3 point_A = midpoint + h_vec;
			contact_points[num_points] = point_A;
			++num_points;

			point_A = midpoint - h_vec;
			contact_points[num_points] = point_A;
			++num_points;

			// Add 2 points from circle A and B along the line between the centers.
			point_A = circle_A_pos + comp_proj * circle_A_radius;
			contact_points[num_points] = point_A;
			++num_points;

			point_A = circle_B_pos - comp_proj * circle_B_radius - norm_proj;
			contact_points[num_points] = point_A;
			++num_points;
		} // Otherwise one circle is inside the other one, use 3 arbitrary equidistant points.
	} // Otherwise circles are concentric, use 3 arbitrary equidistant points.

	if (num_points == 0) {
		// Generate equidistant points.
		if (circle_A_radius < circle_B_radius) {
			// Circle A inside circle B.
			for (int i = 0; i < 3; ++i) {
				Vector3 circle_A_point = circle_A_pos;
				circle_A_point += circle_A_line_1 * Math::cos(2.0 * Math_PI * i / 3.0);
				circle_A_point += circle_A_line_2 * Math::sin(2.0 * Math_PI * i / 3.0);

				contact_points[num_points] = circle_A_point;
				++num_points;
			}
		} else {
			// Circle B inside circle A.
			for (int i = 0; i < 3; ++i) {
				Vector3 circle_B_point = circle_B_pos;
				circle_B_point += circle_B_line_1 * Math::cos(2.0 * Math_PI * i / 3.0);
				circle_B_point += circle_B_line_2 * Math::sin(2.0 * Math_PI * i / 3.0);

				Vector3 circle_A_point = circle_B_point - norm_proj;

				contact_points[num_points] = circle_A_point;
				++num_points;
			}
		}
	}

	Plane circle_B_plane(circle_B_normal, circle_B_pos);

	// Generate contact points.
	for (int i = 0; i < num_points; i++) {
		const Vector3 &contact_point_A = contact_points[i];

		real_t d = circle_B_plane.distance_to(contact_point_A);
		Vector3 closest_B = contact_point_A - circle_B_plane.normal * d;

		if (p_callback->normal.dot(contact_point_A) >= p_callback->normal.dot(closest_B)) {
			continue;
		}

		p_callback->call(contact_point_A, closest_B, circle_B_plane.get_normal());
	}
}

static void _generate_contacts_from_supports(const Vector3 *p_points_A, int p_point_count_A, GodotShape3D::FeatureType p_feature_type_A, const Vector3 *p_points_B, int p_point_count_B, GodotShape3D::FeatureType p_feature_type_B, _CollectorCallback *p_callback) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(p_point_count_A < 1);
	ERR_FAIL_COND(p_point_count_B < 1);
#endif

	static const GenerateContactsFunc generate_contacts_func_table[4][4] = {
		{
				_generate_contacts_point_point,
				_generate_contacts_point_edge,
				_generate_contacts_point_face,
				_generate_contacts_point_circle,
		},
		{
				nullptr,
				_generate_contacts_edge_edge,
				_generate_contacts_face_face,
				_generate_contacts_edge_circle,
		},
		{
				nullptr,
				nullptr,
				_generate_contacts_face_face,
				_generate_contacts_face_circle,
		},
		{
				nullptr,
				nullptr,
				nullptr,
				_generate_contacts_circle_circle,
		},
	};

	int pointcount_B;
	int pointcount_A;
	const Vector3 *points_A;
	const Vector3 *points_B;
	int version_A;
	int version_B;

	if (p_feature_type_A > p_feature_type_B) {
		//swap
		p_callback->swap = !p_callback->swap;
		p_callback->normal = -p_callback->normal;

		pointcount_B = p_point_count_A;
		pointcount_A = p_point_count_B;
		points_A = p_points_B;
		points_B = p_points_A;
		version_A = p_feature_type_B;
		version_B = p_feature_type_A;
	} else {
		pointcount_B = p_point_count_B;
		pointcount_A = p_point_count_A;
		points_A = p_points_A;
		points_B = p_points_B;
		version_A = p_feature_type_A;
		version_B = p_feature_type_B;
	}

	GenerateContactsFunc contacts_func = generate_contacts_func_table[version_A][version_B];
	ERR_FAIL_NULL(contacts_func);
	contacts_func(points_A, pointcount_A, points_B, pointcount_B, p_callback);
}

template <typename ShapeA, typename ShapeB, bool withMargin = false>
class SeparatorAxisTest {
	const ShapeA *shape_A = nullptr;
	const ShapeB *shape_B = nullptr;
	const Transform3D *transform_A = nullptr;
	const Transform3D *transform_B = nullptr;
	real_t best_depth = 1e15;
	_CollectorCallback *callback = nullptr;
	real_t margin_A = 0.0;
	real_t margin_B = 0.0;
	Vector3 separator_axis;

public:
	Vector3 best_axis;

	_FORCE_INLINE_ bool test_previous_axis() {
		if (callback && callback->prev_axis && *callback->prev_axis != Vector3()) {
			return test_axis(*callback->prev_axis);
		} else {
			return true;
		}
	}

	_FORCE_INLINE_ bool test_axis(const Vector3 &p_axis) {
		Vector3 axis = p_axis;

		if (axis.is_zero_approx()) {
			// strange case, try an upwards separator
			axis = Vector3(0.0, 1.0, 0.0);
		}

		real_t min_A = 0.0, max_A = 0.0, min_B = 0.0, max_B = 0.0;

		shape_A->project_range(axis, *transform_A, min_A, max_A);
		shape_B->project_range(axis, *transform_B, min_B, max_B);

		if (withMargin) {
			min_A -= margin_A;
			max_A += margin_A;
			min_B -= margin_B;
			max_B += margin_B;
		}

		min_B -= (max_A - min_A) * 0.5;
		max_B += (max_A - min_A) * 0.5;

		min_B -= (min_A + max_A) * 0.5;
		max_B -= (min_A + max_A) * 0.5;

		if (min_B > 0.0 || max_B < 0.0) {
			separator_axis = axis;
			return false; // doesn't contain 0
		}

		//use the smallest depth

		if (min_B < 0.0) { // could be +0.0, we don't want it to become -0.0
			min_B = -min_B;
		}

		if (max_B < min_B) {
			if (max_B < best_depth) {
				best_depth = max_B;
				best_axis = axis;
			}
		} else {
			if (min_B < best_depth) {
				best_depth = min_B;
				best_axis = -axis; // keep it as A axis
			}
		}

		return true;
	}

	static _FORCE_INLINE_ void test_contact_points(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata) {
		SeparatorAxisTest<ShapeA, ShapeB, withMargin> *separator = (SeparatorAxisTest<ShapeA, ShapeB, withMargin> *)p_userdata;
		Vector3 axis = (p_point_B - p_point_A);
		real_t depth = axis.length();

		// Filter out bogus directions with a threshold and re-testing axis.
		if (separator->best_depth - depth > 0.001) {
			separator->test_axis(axis / depth);
		}
	}

	_FORCE_INLINE_ void generate_contacts() {
		// nothing to do, don't generate
		if (best_axis == Vector3(0.0, 0.0, 0.0)) {
			return;
		}

		if (!callback->callback) {
			//just was checking intersection?
			callback->collided = true;
			if (callback->prev_axis) {
				*callback->prev_axis = best_axis;
			}
			return;
		}

		static const int max_supports = 16;

		Vector3 supports_A[max_supports];
		int support_count_A;
		GodotShape3D::FeatureType support_type_A;
		shape_A->get_supports(transform_A->basis.xform_inv(-best_axis).normalized(), max_supports, supports_A, support_count_A, support_type_A);
		for (int i = 0; i < support_count_A; i++) {
			supports_A[i] = transform_A->xform(supports_A[i]);
		}

		if (withMargin) {
			for (int i = 0; i < support_count_A; i++) {
				supports_A[i] += -best_axis * margin_A;
			}
		}

		Vector3 supports_B[max_supports];
		int support_count_B;
		GodotShape3D::FeatureType support_type_B;
		shape_B->get_supports(transform_B->basis.xform_inv(best_axis).normalized(), max_supports, supports_B, support_count_B, support_type_B);
		for (int i = 0; i < support_count_B; i++) {
			supports_B[i] = transform_B->xform(supports_B[i]);
		}

		if (withMargin) {
			for (int i = 0; i < support_count_B; i++) {
				supports_B[i] += best_axis * margin_B;
			}
		}

		callback->normal = best_axis;
		if (callback->prev_axis) {
			*callback->prev_axis = best_axis;
		}
		_generate_contacts_from_supports(supports_A, support_count_A, support_type_A, supports_B, support_count_B, support_type_B, callback);

		callback->collided = true;
	}

	_FORCE_INLINE_ SeparatorAxisTest(const ShapeA *p_shape_A, const Transform3D &p_transform_A, const ShapeB *p_shape_B, const Transform3D &p_transform_B, _CollectorCallback *p_callback, real_t p_margin_A = 0, real_t p_margin_B = 0) {
		shape_A = p_shape_A;
		shape_B = p_shape_B;
		transform_A = &p_transform_A;
		transform_B = &p_transform_B;
		callback = p_callback;
		margin_A = p_margin_A;
		margin_B = p_margin_B;
	}
};

/****** SAT TESTS *******/

using CollisionFunc = void (*)(const GodotShape3D *, const Transform3D &, const GodotShape3D *, const Transform3D &, _CollectorCallback *p_callback, real_t, real_t);

// Perform analytic sphere-sphere collision and report results to collector
template <bool withMargin>
static void analytic_sphere_collision(const Vector3 &p_origin_a, real_t p_radius_a, const Vector3 &p_origin_b, real_t p_radius_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	// Expand the spheres by the margins if enabled
	if (withMargin) {
		p_radius_a += p_margin_a;
		p_radius_b += p_margin_b;
	}

	// Get the vector from sphere B to A
	Vector3 b_to_a = p_origin_a - p_origin_b;

	// Get the length from B to A
	real_t b_to_a_len = b_to_a.length();

	// Calculate the sphere overlap, and bail if not overlapping
	real_t overlap = p_radius_a + p_radius_b - b_to_a_len;
	if (overlap < 0) {
		return;
	}

	// Report collision
	p_collector->collided = true;

	// Bail if there is no callback to receive the A and B collision points.
	if (!p_collector->callback) {
		return;
	}

	// Normalize the B to A vector
	if (b_to_a_len < CMP_EPSILON) {
		b_to_a = Vector3(0, 1, 0); // Spheres coincident, use arbitrary direction
	} else {
		b_to_a /= b_to_a_len;
	}

	// Report collision points. The operations below are intended to minimize
	// floating-point precision errors. This is done by calculating the first
	// collision point from the smaller sphere, and then jumping across to
	// the larger spheres collision point using the overlap distance. This
	// jump is usually small even if the large sphere is massive, and so the
	// second point will not suffer from precision errors.
	if (p_radius_a < p_radius_b) {
		Vector3 point_a = p_origin_a - b_to_a * p_radius_a;
		Vector3 point_b = point_a + b_to_a * overlap;
		p_collector->call(point_a, point_b, b_to_a); // Consider adding b_to_a vector
	} else {
		Vector3 point_b = p_origin_b + b_to_a * p_radius_b;
		Vector3 point_a = point_b - b_to_a * overlap;
		p_collector->call(point_a, point_b, b_to_a); // Consider adding b_to_a vector
	}
}

template <bool withMargin>
static void _collision_sphere_sphere(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotSphereShape3D *sphere_A = static_cast<const GodotSphereShape3D *>(p_a);
	const GodotSphereShape3D *sphere_B = static_cast<const GodotSphereShape3D *>(p_b);

	// Perform an analytic sphere collision between the two spheres
	analytic_sphere_collision<withMargin>(
			p_transform_a.origin,
			sphere_A->get_radius() * p_transform_a.basis[0].length(),
			p_transform_b.origin,
			sphere_B->get_radius() * p_transform_b.basis[0].length(),
			p_collector,
			p_margin_a,
			p_margin_b);
}

template <bool withMargin>
static void _collision_sphere_box(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotSphereShape3D *sphere_A = static_cast<const GodotSphereShape3D *>(p_a);
	const GodotBoxShape3D *box_B = static_cast<const GodotBoxShape3D *>(p_b);

	// Find the point on the box nearest to the center of the sphere.

	Vector3 center = p_transform_b.affine_inverse().xform(p_transform_a.origin);
	Vector3 extents = box_B->get_half_extents();
	Vector3 nearest(MIN(MAX(center.x, -extents.x), extents.x),
			MIN(MAX(center.y, -extents.y), extents.y),
			MIN(MAX(center.z, -extents.z), extents.z));
	nearest = p_transform_b.xform(nearest);

	// See if it is inside the sphere.

	Vector3 delta = nearest - p_transform_a.origin;
	real_t length = delta.length();
	real_t radius = sphere_A->get_radius() * p_transform_a.basis[0].length();
	if (length > radius + p_margin_a + p_margin_b) {
		return;
	}
	p_collector->collided = true;
	if (!p_collector->callback) {
		return;
	}
	Vector3 axis;
	if (length == 0) {
		// The box passes through the sphere center.  Select an axis based on the box's center.
		axis = (p_transform_b.origin - nearest).normalized();
	} else {
		axis = delta / length;
	}
	Vector3 point_a = p_transform_a.origin + (radius + p_margin_a) * axis;
	Vector3 point_b = (withMargin ? nearest - p_margin_b * axis : nearest);
	p_collector->call(point_a, point_b, axis);
}

template <bool withMargin>
static void _collision_sphere_capsule(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotSphereShape3D *sphere_A = static_cast<const GodotSphereShape3D *>(p_a);
	const GodotCapsuleShape3D *capsule_B = static_cast<const GodotCapsuleShape3D *>(p_b);

	real_t scale_A = p_transform_a.basis[0].length();
	real_t scale_B = p_transform_b.basis[0].length();

	// Construct the capsule segment (ball-center to ball-center)
	Vector3 capsule_segment[2];
	Vector3 capsule_axis = p_transform_b.basis.get_column(1) * (capsule_B->get_height() * 0.5 - capsule_B->get_radius());
	capsule_segment[0] = p_transform_b.origin + capsule_axis;
	capsule_segment[1] = p_transform_b.origin - capsule_axis;

	// Get the capsules closest segment-point to the sphere
	Vector3 capsule_closest = Geometry3D::get_closest_point_to_segment(p_transform_a.origin, capsule_segment);

	// Perform an analytic sphere collision between the sphere and the sphere-collider in the capsule
	analytic_sphere_collision<withMargin>(
			p_transform_a.origin,
			sphere_A->get_radius() * scale_A,
			capsule_closest,
			capsule_B->get_radius() * scale_B,
			p_collector,
			p_margin_a,
			p_margin_b);
}

template <bool withMargin>
static void analytic_sphere_cylinder_collision(real_t p_radius_a, real_t p_radius_b, real_t p_height_b, const Transform3D &p_transform_a, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	// Find the point on the cylinder nearest to the center of the sphere.

	Vector3 center = p_transform_b.affine_inverse().xform(p_transform_a.origin);
	Vector3 nearest = center;
	real_t scale_A = p_transform_a.basis[0].length();
	real_t r = Math::sqrt(center.x * center.x + center.z * center.z);
	if (r > p_radius_b) {
		real_t scale = p_radius_b / r;
		nearest.x *= scale;
		nearest.z *= scale;
	}
	real_t half_height = p_height_b / 2;
	nearest.y = MIN(MAX(center.y, -half_height), half_height);
	nearest = p_transform_b.xform(nearest);

	// See if it is inside the sphere.

	Vector3 delta = nearest - p_transform_a.origin;
	real_t length = delta.length();
	if (length > p_radius_a * scale_A + p_margin_a + p_margin_b) {
		return;
	}
	p_collector->collided = true;
	if (!p_collector->callback) {
		return;
	}
	Vector3 axis;
	if (length == 0) {
		// The cylinder passes through the sphere center.  Select an axis based on the cylinder's center.
		axis = (p_transform_b.origin - nearest).normalized();
	} else {
		axis = delta / length;
	}
	Vector3 point_a = p_transform_a.origin + (p_radius_a * scale_A + p_margin_a) * axis;
	Vector3 point_b = (withMargin ? nearest - p_margin_b * axis : nearest);
	p_collector->call(point_a, point_b, axis);
}

template <bool withMargin>
static void _collision_sphere_cylinder(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotSphereShape3D *sphere_A = static_cast<const GodotSphereShape3D *>(p_a);
	const GodotCylinderShape3D *cylinder_B = static_cast<const GodotCylinderShape3D *>(p_b);

	analytic_sphere_cylinder_collision<withMargin>(sphere_A->get_radius(), cylinder_B->get_radius(), cylinder_B->get_height(), p_transform_a, p_transform_b, p_collector, p_margin_a, p_margin_b);
}

template <bool withMargin>
static void _collision_sphere_convex_polygon(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotSphereShape3D *sphere_A = static_cast<const GodotSphereShape3D *>(p_a);
	const GodotConvexPolygonShape3D *convex_polygon_B = static_cast<const GodotConvexPolygonShape3D *>(p_b);

	SeparatorAxisTest<GodotSphereShape3D, GodotConvexPolygonShape3D, withMargin> separator(sphere_A, p_transform_a, convex_polygon_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	if (!separator.test_previous_axis()) {
		return;
	}

	const Geometry3D::MeshData &mesh = convex_polygon_B->get_mesh();

	const Geometry3D::MeshData::Face *faces = mesh.faces.ptr();
	int face_count = mesh.faces.size();
	const Geometry3D::MeshData::Edge *edges = mesh.edges.ptr();
	int edge_count = mesh.edges.size();
	const Vector3 *vertices = mesh.vertices.ptr();
	int vertex_count = mesh.vertices.size();

	// Precalculating this makes the transforms faster.
	Basis b_xform_normal = p_transform_b.basis.inverse().transposed();

	// faces of B
	for (int i = 0; i < face_count; i++) {
		Vector3 axis = b_xform_normal.xform(faces[i].plane.normal).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// edges of B
	for (int i = 0; i < edge_count; i++) {
		Vector3 v1 = p_transform_b.xform(vertices[edges[i].vertex_a]);
		Vector3 v2 = p_transform_b.xform(vertices[edges[i].vertex_b]);
		Vector3 v3 = p_transform_a.origin;

		Vector3 n1 = v2 - v1;
		Vector3 n2 = v2 - v3;

		Vector3 axis = n1.cross(n2).cross(n1).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// vertices of B
	for (int i = 0; i < vertex_count; i++) {
		Vector3 v1 = p_transform_b.xform(vertices[i]);
		Vector3 v2 = p_transform_a.origin;

		Vector3 axis = (v2 - v1).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_sphere_face(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotSphereShape3D *sphere_A = static_cast<const GodotSphereShape3D *>(p_a);
	const GodotFaceShape3D *face_B = static_cast<const GodotFaceShape3D *>(p_b);

	SeparatorAxisTest<GodotSphereShape3D, GodotFaceShape3D, withMargin> separator(sphere_A, p_transform_a, face_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	Vector3 vertex[3] = {
		p_transform_b.xform(face_B->vertex[0]),
		p_transform_b.xform(face_B->vertex[1]),
		p_transform_b.xform(face_B->vertex[2]),
	};

	Vector3 normal = (vertex[0] - vertex[2]).cross(vertex[0] - vertex[1]).normalized();

	if (!separator.test_axis(normal)) {
		return;
	}

	// edges and points of B
	for (int i = 0; i < 3; i++) {
		Vector3 n1 = vertex[i] - p_transform_a.origin;
		if (n1.dot(normal) < 0.0) {
			n1 *= -1.0;
		}

		if (!separator.test_axis(n1.normalized())) {
			return;
		}

		Vector3 n2 = vertex[(i + 1) % 3] - vertex[i];

		Vector3 axis = n1.cross(n2).cross(n2).normalized();
		if (axis.dot(normal) < 0.0) {
			axis *= -1.0;
		}

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	if (!face_B->backface_collision) {
		if (separator.best_axis.dot(normal) < _BACKFACE_NORMAL_THRESHOLD) {
			if (face_B->invert_backface_collision) {
				separator.best_axis = separator.best_axis.bounce(normal);
			} else {
				// Just ignore backface collision.
				return;
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_box_box(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotBoxShape3D *box_A = static_cast<const GodotBoxShape3D *>(p_a);
	const GodotBoxShape3D *box_B = static_cast<const GodotBoxShape3D *>(p_b);

	SeparatorAxisTest<GodotBoxShape3D, GodotBoxShape3D, withMargin> separator(box_A, p_transform_a, box_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	if (!separator.test_previous_axis()) {
		return;
	}

	// test faces of A

	for (int i = 0; i < 3; i++) {
		Vector3 axis = p_transform_a.basis.get_column(i).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// test faces of B

	for (int i = 0; i < 3; i++) {
		Vector3 axis = p_transform_b.basis.get_column(i).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// test combined edges
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			Vector3 axis = p_transform_a.basis.get_column(i).cross(p_transform_b.basis.get_column(j));

			if (Math::is_zero_approx(axis.length_squared())) {
				continue;
			}
			axis.normalize();

			if (!separator.test_axis(axis)) {
				return;
			}
		}
	}

	if (withMargin) {
		//add endpoint test between closest vertices and edges

		// calculate closest point to sphere

		Vector3 ab_vec = p_transform_b.origin - p_transform_a.origin;

		Vector3 cnormal_a = p_transform_a.basis.xform_inv(ab_vec);

		Vector3 support_a = p_transform_a.xform(Vector3(

				(cnormal_a.x < 0) ? -box_A->get_half_extents().x : box_A->get_half_extents().x,
				(cnormal_a.y < 0) ? -box_A->get_half_extents().y : box_A->get_half_extents().y,
				(cnormal_a.z < 0) ? -box_A->get_half_extents().z : box_A->get_half_extents().z));

		Vector3 cnormal_b = p_transform_b.basis.xform_inv(-ab_vec);

		Vector3 support_b = p_transform_b.xform(Vector3(

				(cnormal_b.x < 0) ? -box_B->get_half_extents().x : box_B->get_half_extents().x,
				(cnormal_b.y < 0) ? -box_B->get_half_extents().y : box_B->get_half_extents().y,
				(cnormal_b.z < 0) ? -box_B->get_half_extents().z : box_B->get_half_extents().z));

		Vector3 axis_ab = (support_a - support_b);

		if (!separator.test_axis(axis_ab.normalized())) {
			return;
		}

		//now try edges, which become cylinders!

		for (int i = 0; i < 3; i++) {
			//a ->b
			Vector3 axis_a = p_transform_a.basis.get_column(i);

			if (!separator.test_axis(axis_ab.cross(axis_a).cross(axis_a).normalized())) {
				return;
			}

			//b ->a
			Vector3 axis_b = p_transform_b.basis.get_column(i);

			if (!separator.test_axis(axis_ab.cross(axis_b).cross(axis_b).normalized())) {
				return;
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_box_capsule(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotBoxShape3D *box_A = static_cast<const GodotBoxShape3D *>(p_a);
	const GodotCapsuleShape3D *capsule_B = static_cast<const GodotCapsuleShape3D *>(p_b);

	SeparatorAxisTest<GodotBoxShape3D, GodotCapsuleShape3D, withMargin> separator(box_A, p_transform_a, capsule_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	if (!separator.test_previous_axis()) {
		return;
	}

	// faces of A
	for (int i = 0; i < 3; i++) {
		Vector3 axis = p_transform_a.basis.get_column(i).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	Vector3 cyl_axis = p_transform_b.basis.get_column(1).normalized();

	// edges of A, capsule cylinder

	for (int i = 0; i < 3; i++) {
		// cylinder
		Vector3 box_axis = p_transform_a.basis.get_column(i);
		Vector3 axis = box_axis.cross(cyl_axis);
		if (Math::is_zero_approx(axis.length_squared())) {
			continue;
		}

		if (!separator.test_axis(axis.normalized())) {
			return;
		}
	}

	// points of A, capsule cylinder
	// this sure could be made faster somehow..

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			for (int k = 0; k < 2; k++) {
				Vector3 he = box_A->get_half_extents();
				he.x *= (i * 2 - 1);
				he.y *= (j * 2 - 1);
				he.z *= (k * 2 - 1);
				Vector3 point = p_transform_a.origin;
				for (int l = 0; l < 3; l++) {
					point += p_transform_a.basis.get_column(l) * he[l];
				}

				//Vector3 axis = (point - cyl_axis * cyl_axis.dot(point)).normalized();
				Vector3 axis = Plane(cyl_axis).project(point).normalized();

				if (!separator.test_axis(axis)) {
					return;
				}
			}
		}
	}

	// capsule balls, edges of A

	for (int i = 0; i < 2; i++) {
		Vector3 capsule_axis = p_transform_b.basis.get_column(1) * (capsule_B->get_height() * 0.5 - capsule_B->get_radius());

		Vector3 sphere_pos = p_transform_b.origin + ((i == 0) ? capsule_axis : -capsule_axis);

		Vector3 cnormal = p_transform_a.xform_inv(sphere_pos);

		Vector3 cpoint = p_transform_a.xform(Vector3(

				(cnormal.x < 0) ? -box_A->get_half_extents().x : box_A->get_half_extents().x,
				(cnormal.y < 0) ? -box_A->get_half_extents().y : box_A->get_half_extents().y,
				(cnormal.z < 0) ? -box_A->get_half_extents().z : box_A->get_half_extents().z));

		// use point to test axis
		Vector3 point_axis = (sphere_pos - cpoint).normalized();

		if (!separator.test_axis(point_axis)) {
			return;
		}

		// test edges of A

		for (int j = 0; j < 3; j++) {
			Vector3 axis = point_axis.cross(p_transform_a.basis.get_column(j)).cross(p_transform_a.basis.get_column(j)).normalized();

			if (!separator.test_axis(axis)) {
				return;
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_box_cylinder(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotBoxShape3D *box_A = static_cast<const GodotBoxShape3D *>(p_a);
	const GodotCylinderShape3D *cylinder_B = static_cast<const GodotCylinderShape3D *>(p_b);

	SeparatorAxisTest<GodotBoxShape3D, GodotCylinderShape3D, withMargin> separator(box_A, p_transform_a, cylinder_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	if (!separator.test_previous_axis()) {
		return;
	}

	// Faces of A.
	for (int i = 0; i < 3; i++) {
		Vector3 axis = p_transform_a.basis.get_column(i).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	Vector3 cyl_axis = p_transform_b.basis.get_column(1).normalized();

	// Cylinder end caps.
	{
		if (!separator.test_axis(cyl_axis)) {
			return;
		}
	}

	// Edges of A, cylinder lateral surface.
	for (int i = 0; i < 3; i++) {
		Vector3 box_axis = p_transform_a.basis.get_column(i);
		Vector3 axis = box_axis.cross(cyl_axis);
		if (Math::is_zero_approx(axis.length_squared())) {
			continue;
		}

		if (!separator.test_axis(axis.normalized())) {
			return;
		}
	}

	// Gather points of A.
	Vector3 vertices_A[8];
	Vector3 box_extent = box_A->get_half_extents();
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			for (int k = 0; k < 2; k++) {
				Vector3 extent = box_extent;
				extent.x *= (i * 2 - 1);
				extent.y *= (j * 2 - 1);
				extent.z *= (k * 2 - 1);
				Vector3 &point = vertices_A[i * 2 * 2 + j * 2 + k];
				point = p_transform_a.origin;
				for (int l = 0; l < 3; l++) {
					point += p_transform_a.basis.get_column(l) * extent[l];
				}
			}
		}
	}

	// Points of A, cylinder lateral surface.
	for (int i = 0; i < 8; i++) {
		const Vector3 &point = vertices_A[i];
		Vector3 axis = Plane(cyl_axis).project(point).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// Edges of A, cylinder end caps rim.
	int edges_start_A[12] = { 0, 2, 4, 6, 0, 1, 4, 5, 0, 1, 2, 3 };
	int edges_end_A[12] = { 1, 3, 5, 7, 2, 3, 6, 7, 4, 5, 6, 7 };

	Vector3 cap_axis = cyl_axis * (cylinder_B->get_height() * 0.5);

	for (int i = 0; i < 2; i++) {
		Vector3 cap_pos = p_transform_b.origin + ((i == 0) ? cap_axis : -cap_axis);

		for (int e = 0; e < 12; e++) {
			const Vector3 &edge_start = vertices_A[edges_start_A[e]];
			const Vector3 &edge_end = vertices_A[edges_end_A[e]];

			Vector3 edge_dir = (edge_end - edge_start);
			edge_dir.normalize();

			real_t edge_dot = edge_dir.dot(cyl_axis);
			if (Math::is_zero_approx(edge_dot)) {
				// Edge is perpendicular to cylinder axis.
				continue;
			}

			// Calculate intersection between edge and circle plane.
			Vector3 edge_diff = cap_pos - edge_start;
			real_t diff_dot = edge_diff.dot(cyl_axis);
			Vector3 intersection = edge_start + edge_dir * diff_dot / edge_dot;

			// Calculate tangent that touches intersection.
			Vector3 tangent = (cap_pos - intersection).cross(cyl_axis);

			// Axis is orthogonal both to tangent and edge direction.
			Vector3 axis = tangent.cross(edge_dir);

			if (!separator.test_axis(axis.normalized())) {
				return;
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_box_convex_polygon(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotBoxShape3D *box_A = static_cast<const GodotBoxShape3D *>(p_a);
	const GodotConvexPolygonShape3D *convex_polygon_B = static_cast<const GodotConvexPolygonShape3D *>(p_b);

	SeparatorAxisTest<GodotBoxShape3D, GodotConvexPolygonShape3D, withMargin> separator(box_A, p_transform_a, convex_polygon_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	if (!separator.test_previous_axis()) {
		return;
	}

	const Geometry3D::MeshData &mesh = convex_polygon_B->get_mesh();

	const Geometry3D::MeshData::Face *faces = mesh.faces.ptr();
	int face_count = mesh.faces.size();
	const Geometry3D::MeshData::Edge *edges = mesh.edges.ptr();
	int edge_count = mesh.edges.size();
	const Vector3 *vertices = mesh.vertices.ptr();
	int vertex_count = mesh.vertices.size();

	// faces of A
	for (int i = 0; i < 3; i++) {
		Vector3 axis = p_transform_a.basis.get_column(i).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// Precalculating this makes the transforms faster.
	Basis b_xform_normal = p_transform_b.basis.inverse().transposed();

	// faces of B
	for (int i = 0; i < face_count; i++) {
		Vector3 axis = b_xform_normal.xform(faces[i].plane.normal).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// A<->B edges
	for (int i = 0; i < 3; i++) {
		Vector3 e1 = p_transform_a.basis.get_column(i);

		for (int j = 0; j < edge_count; j++) {
			Vector3 e2 = p_transform_b.basis.xform(vertices[edges[j].vertex_a]) - p_transform_b.basis.xform(vertices[edges[j].vertex_b]);

			Vector3 axis = e1.cross(e2).normalized();

			if (!separator.test_axis(axis)) {
				return;
			}
		}
	}

	if (withMargin) {
		// calculate closest points between vertices and box edges
		for (int v = 0; v < vertex_count; v++) {
			Vector3 vtxb = p_transform_b.xform(vertices[v]);
			Vector3 ab_vec = vtxb - p_transform_a.origin;

			Vector3 cnormal_a = p_transform_a.basis.xform_inv(ab_vec);

			Vector3 support_a = p_transform_a.xform(Vector3(

					(cnormal_a.x < 0) ? -box_A->get_half_extents().x : box_A->get_half_extents().x,
					(cnormal_a.y < 0) ? -box_A->get_half_extents().y : box_A->get_half_extents().y,
					(cnormal_a.z < 0) ? -box_A->get_half_extents().z : box_A->get_half_extents().z));

			Vector3 axis_ab = support_a - vtxb;

			if (!separator.test_axis(axis_ab.normalized())) {
				return;
			}

			//now try edges, which become cylinders!

			for (int i = 0; i < 3; i++) {
				//a ->b
				Vector3 axis_a = p_transform_a.basis.get_column(i);

				if (!separator.test_axis(axis_ab.cross(axis_a).cross(axis_a).normalized())) {
					return;
				}
			}
		}

		//convex edges and box points
		for (int i = 0; i < 2; i++) {
			for (int j = 0; j < 2; j++) {
				for (int k = 0; k < 2; k++) {
					Vector3 he = box_A->get_half_extents();
					he.x *= (i * 2 - 1);
					he.y *= (j * 2 - 1);
					he.z *= (k * 2 - 1);
					Vector3 point = p_transform_a.origin;
					for (int l = 0; l < 3; l++) {
						point += p_transform_a.basis.get_column(l) * he[l];
					}

					for (int e = 0; e < edge_count; e++) {
						Vector3 p1 = p_transform_b.xform(vertices[edges[e].vertex_a]);
						Vector3 p2 = p_transform_b.xform(vertices[edges[e].vertex_b]);
						Vector3 n = (p2 - p1);

						if (!separator.test_axis((point - p2).cross(n).cross(n).normalized())) {
							return;
						}
					}
				}
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_box_face(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotBoxShape3D *box_A = static_cast<const GodotBoxShape3D *>(p_a);
	const GodotFaceShape3D *face_B = static_cast<const GodotFaceShape3D *>(p_b);

	SeparatorAxisTest<GodotBoxShape3D, GodotFaceShape3D, withMargin> separator(box_A, p_transform_a, face_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	Vector3 vertex[3] = {
		p_transform_b.xform(face_B->vertex[0]),
		p_transform_b.xform(face_B->vertex[1]),
		p_transform_b.xform(face_B->vertex[2]),
	};

	Vector3 normal = (vertex[0] - vertex[2]).cross(vertex[0] - vertex[1]).normalized();

	if (!separator.test_axis(normal)) {
		return;
	}

	// faces of A
	for (int i = 0; i < 3; i++) {
		Vector3 axis = p_transform_a.basis.get_column(i).normalized();
		if (axis.dot(normal) < 0.0) {
			axis *= -1.0;
		}

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// combined edges

	for (int i = 0; i < 3; i++) {
		Vector3 e = vertex[i] - vertex[(i + 1) % 3];

		for (int j = 0; j < 3; j++) {
			Vector3 axis = e.cross(p_transform_a.basis.get_column(j)).normalized();
			if (axis.dot(normal) < 0.0) {
				axis *= -1.0;
			}

			if (!separator.test_axis(axis)) {
				return;
			}
		}
	}

	if (withMargin) {
		// calculate closest points between vertices and box edges
		for (int v = 0; v < 3; v++) {
			Vector3 ab_vec = vertex[v] - p_transform_a.origin;

			Vector3 cnormal_a = p_transform_a.basis.xform_inv(ab_vec);

			Vector3 support_a = p_transform_a.xform(Vector3(

					(cnormal_a.x < 0) ? -box_A->get_half_extents().x : box_A->get_half_extents().x,
					(cnormal_a.y < 0) ? -box_A->get_half_extents().y : box_A->get_half_extents().y,
					(cnormal_a.z < 0) ? -box_A->get_half_extents().z : box_A->get_half_extents().z));

			Vector3 axis_ab = support_a - vertex[v];
			if (axis_ab.dot(normal) < 0.0) {
				axis_ab *= -1.0;
			}

			if (!separator.test_axis(axis_ab.normalized())) {
				return;
			}

			//now try edges, which become cylinders!

			for (int i = 0; i < 3; i++) {
				//a ->b
				Vector3 axis_a = p_transform_a.basis.get_column(i);

				Vector3 axis = axis_ab.cross(axis_a).cross(axis_a).normalized();
				if (axis.dot(normal) < 0.0) {
					axis *= -1.0;
				}

				if (!separator.test_axis(axis)) {
					return;
				}
			}
		}

		//convex edges and box points, there has to be a way to speed up this (get closest point?)
		for (int i = 0; i < 2; i++) {
			for (int j = 0; j < 2; j++) {
				for (int k = 0; k < 2; k++) {
					Vector3 he = box_A->get_half_extents();
					he.x *= (i * 2 - 1);
					he.y *= (j * 2 - 1);
					he.z *= (k * 2 - 1);
					Vector3 point = p_transform_a.origin;
					for (int l = 0; l < 3; l++) {
						point += p_transform_a.basis.get_column(l) * he[l];
					}

					for (int e = 0; e < 3; e++) {
						Vector3 p1 = vertex[e];
						Vector3 p2 = vertex[(e + 1) % 3];

						Vector3 n = (p2 - p1);

						Vector3 axis = (point - p2).cross(n).cross(n).normalized();
						if (axis.dot(normal) < 0.0) {
							axis *= -1.0;
						}

						if (!separator.test_axis(axis)) {
							return;
						}
					}
				}
			}
		}
	}

	if (!face_B->backface_collision) {
		if (separator.best_axis.dot(normal) < _BACKFACE_NORMAL_THRESHOLD) {
			if (face_B->invert_backface_collision) {
				separator.best_axis = separator.best_axis.bounce(normal);
			} else {
				// Just ignore backface collision.
				return;
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_capsule_capsule(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotCapsuleShape3D *capsule_A = static_cast<const GodotCapsuleShape3D *>(p_a);
	const GodotCapsuleShape3D *capsule_B = static_cast<const GodotCapsuleShape3D *>(p_b);

	real_t scale_A = p_transform_a.basis[0].length();
	real_t scale_B = p_transform_b.basis[0].length();

	// Get the closest points between the capsule segments
	Vector3 capsule_A_closest;
	Vector3 capsule_B_closest;
	Vector3 capsule_A_axis = p_transform_a.basis.get_column(1) * (capsule_A->get_height() * 0.5 - capsule_A->get_radius());
	Vector3 capsule_B_axis = p_transform_b.basis.get_column(1) * (capsule_B->get_height() * 0.5 - capsule_B->get_radius());
	Geometry3D::get_closest_points_between_segments(
			p_transform_a.origin + capsule_A_axis,
			p_transform_a.origin - capsule_A_axis,
			p_transform_b.origin + capsule_B_axis,
			p_transform_b.origin - capsule_B_axis,
			capsule_A_closest,
			capsule_B_closest);

	// Perform the analytic collision between the two closest capsule spheres
	analytic_sphere_collision<withMargin>(
			capsule_A_closest,
			capsule_A->get_radius() * scale_A,
			capsule_B_closest,
			capsule_B->get_radius() * scale_B,
			p_collector,
			p_margin_a,
			p_margin_b);
}

template <bool withMargin>
static void _collision_capsule_cylinder(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotCapsuleShape3D *capsule_A = static_cast<const GodotCapsuleShape3D *>(p_a);
	const GodotCylinderShape3D *cylinder_B = static_cast<const GodotCylinderShape3D *>(p_b);

	// Find the closest points between the axes of the two objects.

	Vector3 capsule_A_closest;
	Vector3 cylinder_B_closest;
	Vector3 capsule_A_axis = p_transform_a.basis.get_column(1) * (capsule_A->get_height() * 0.5 - capsule_A->get_radius());
	Vector3 cylinder_B_axis = p_transform_b.basis.get_column(1) * (cylinder_B->get_height() * 0.5);
	Geometry3D::get_closest_points_between_segments(
			p_transform_a.origin + capsule_A_axis,
			p_transform_a.origin - capsule_A_axis,
			p_transform_b.origin + cylinder_B_axis,
			p_transform_b.origin - cylinder_B_axis,
			capsule_A_closest,
			cylinder_B_closest);

	// Perform the collision test between the cylinder and the nearest sphere on the capsule axis.

	Transform3D sphere_transform(p_transform_a.basis, capsule_A_closest);
	analytic_sphere_cylinder_collision<withMargin>(capsule_A->get_radius(), cylinder_B->get_radius(), cylinder_B->get_height(), sphere_transform, p_transform_b, p_collector, p_margin_a, p_margin_b);
}

template <bool withMargin>
static void _collision_capsule_convex_polygon(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotCapsuleShape3D *capsule_A = static_cast<const GodotCapsuleShape3D *>(p_a);
	const GodotConvexPolygonShape3D *convex_polygon_B = static_cast<const GodotConvexPolygonShape3D *>(p_b);

	SeparatorAxisTest<GodotCapsuleShape3D, GodotConvexPolygonShape3D, withMargin> separator(capsule_A, p_transform_a, convex_polygon_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	if (!separator.test_previous_axis()) {
		return;
	}

	const Geometry3D::MeshData &mesh = convex_polygon_B->get_mesh();

	const Geometry3D::MeshData::Face *faces = mesh.faces.ptr();
	int face_count = mesh.faces.size();
	const Geometry3D::MeshData::Edge *edges = mesh.edges.ptr();
	int edge_count = mesh.edges.size();
	const Vector3 *vertices = mesh.vertices.ptr();

	// Precalculating this makes the transforms faster.
	Basis b_xform_normal = p_transform_b.basis.inverse().transposed();

	// faces of B
	for (int i = 0; i < face_count; i++) {
		Vector3 axis = b_xform_normal.xform(faces[i].plane.normal).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// edges of B, capsule cylinder

	for (int i = 0; i < edge_count; i++) {
		// cylinder
		Vector3 edge_axis = p_transform_b.basis.xform(vertices[edges[i].vertex_a]) - p_transform_b.basis.xform(vertices[edges[i].vertex_b]);
		Vector3 axis = edge_axis.cross(p_transform_a.basis.get_column(1)).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// capsule balls, edges of B

	for (int i = 0; i < 2; i++) {
		// edges of B, capsule cylinder

		Vector3 capsule_axis = p_transform_a.basis.get_column(1) * (capsule_A->get_height() * 0.5 - capsule_A->get_radius());

		Vector3 sphere_pos = p_transform_a.origin + ((i == 0) ? capsule_axis : -capsule_axis);

		for (int j = 0; j < edge_count; j++) {
			Vector3 n1 = sphere_pos - p_transform_b.xform(vertices[edges[j].vertex_a]);
			Vector3 n2 = p_transform_b.basis.xform(vertices[edges[j].vertex_a]) - p_transform_b.basis.xform(vertices[edges[j].vertex_b]);

			Vector3 axis = n1.cross(n2).cross(n2).normalized();

			if (!separator.test_axis(axis)) {
				return;
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_capsule_face(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotCapsuleShape3D *capsule_A = static_cast<const GodotCapsuleShape3D *>(p_a);
	const GodotFaceShape3D *face_B = static_cast<const GodotFaceShape3D *>(p_b);

	SeparatorAxisTest<GodotCapsuleShape3D, GodotFaceShape3D, withMargin> separator(capsule_A, p_transform_a, face_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	Vector3 vertex[3] = {
		p_transform_b.xform(face_B->vertex[0]),
		p_transform_b.xform(face_B->vertex[1]),
		p_transform_b.xform(face_B->vertex[2]),
	};

	Vector3 normal = (vertex[0] - vertex[2]).cross(vertex[0] - vertex[1]).normalized();

	if (!separator.test_axis(normal)) {
		return;
	}

	// edges of B, capsule cylinder

	Vector3 capsule_axis = p_transform_a.basis.get_column(1) * (capsule_A->get_height() * 0.5 - capsule_A->get_radius());

	for (int i = 0; i < 3; i++) {
		// edge-cylinder
		Vector3 edge_axis = vertex[i] - vertex[(i + 1) % 3];

		Vector3 axis = edge_axis.cross(capsule_axis).normalized();
		if (axis.dot(normal) < 0.0) {
			axis *= -1.0;
		}

		if (!separator.test_axis(axis)) {
			return;
		}

		Vector3 dir_axis = (p_transform_a.origin - vertex[i]).cross(capsule_axis).cross(capsule_axis).normalized();
		if (dir_axis.dot(normal) < 0.0) {
			dir_axis *= -1.0;
		}

		if (!separator.test_axis(dir_axis)) {
			return;
		}

		for (int j = 0; j < 2; j++) {
			// point-spheres
			Vector3 sphere_pos = p_transform_a.origin + ((j == 0) ? capsule_axis : -capsule_axis);

			Vector3 n1 = sphere_pos - vertex[i];
			if (n1.dot(normal) < 0.0) {
				n1 *= -1.0;
			}

			if (!separator.test_axis(n1.normalized())) {
				return;
			}

			Vector3 n2 = edge_axis;

			axis = n1.cross(n2).cross(n2);
			if (axis.dot(normal) < 0.0) {
				axis *= -1.0;
			}

			if (!separator.test_axis(axis.normalized())) {
				return;
			}
		}
	}

	if (!face_B->backface_collision) {
		if (separator.best_axis.dot(normal) < _BACKFACE_NORMAL_THRESHOLD) {
			if (face_B->invert_backface_collision) {
				separator.best_axis = separator.best_axis.bounce(normal);
			} else {
				// Just ignore backface collision.
				return;
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_cylinder_cylinder(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotCylinderShape3D *cylinder_A = static_cast<const GodotCylinderShape3D *>(p_a);
	const GodotCylinderShape3D *cylinder_B = static_cast<const GodotCylinderShape3D *>(p_b);

	SeparatorAxisTest<GodotCylinderShape3D, GodotCylinderShape3D, withMargin> separator(cylinder_A, p_transform_a, cylinder_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	Vector3 cylinder_A_axis = p_transform_a.basis.get_column(1);
	Vector3 cylinder_B_axis = p_transform_b.basis.get_column(1);

	if (!separator.test_previous_axis()) {
		return;
	}

	// Cylinder A end caps.
	if (!separator.test_axis(cylinder_A_axis.normalized())) {
		return;
	}

	// Cylinder B end caps.
	if (!separator.test_axis(cylinder_B_axis.normalized())) {
		return;
	}

	Vector3 cylinder_diff = p_transform_b.origin - p_transform_a.origin;

	// Cylinder A lateral surface.
	if (!separator.test_axis(cylinder_A_axis.cross(cylinder_diff).cross(cylinder_A_axis).normalized())) {
		return;
	}

	// Cylinder B lateral surface.
	if (!separator.test_axis(cylinder_B_axis.cross(cylinder_diff).cross(cylinder_B_axis).normalized())) {
		return;
	}

	real_t proj = cylinder_A_axis.cross(cylinder_B_axis).cross(cylinder_B_axis).dot(cylinder_A_axis);
	if (Math::is_zero_approx(proj)) {
		// Parallel cylinders, handle with specific axes only.
		// Note: GJKEPA with no margin can lead to degenerate cases in this situation.
		separator.generate_contacts();
		return;
	}

	GodotCollisionSolver3D::CallbackResult callback = SeparatorAxisTest<GodotCylinderShape3D, GodotCylinderShape3D, withMargin>::test_contact_points;

	// Fallback to generic algorithm to find the best separating axis.
	if (!fallback_collision_solver(p_a, p_transform_a, p_b, p_transform_b, callback, &separator, false, p_margin_a, p_margin_b)) {
		return;
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_cylinder_convex_polygon(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotCylinderShape3D *cylinder_A = static_cast<const GodotCylinderShape3D *>(p_a);
	const GodotConvexPolygonShape3D *convex_polygon_B = static_cast<const GodotConvexPolygonShape3D *>(p_b);

	SeparatorAxisTest<GodotCylinderShape3D, GodotConvexPolygonShape3D, withMargin> separator(cylinder_A, p_transform_a, convex_polygon_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	GodotCollisionSolver3D::CallbackResult callback = SeparatorAxisTest<GodotCylinderShape3D, GodotConvexPolygonShape3D, withMargin>::test_contact_points;

	// Fallback to generic algorithm to find the best separating axis.
	if (!fallback_collision_solver(p_a, p_transform_a, p_b, p_transform_b, callback, &separator, false, p_margin_a, p_margin_b)) {
		return;
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_cylinder_face(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotCylinderShape3D *cylinder_A = static_cast<const GodotCylinderShape3D *>(p_a);
	const GodotFaceShape3D *face_B = static_cast<const GodotFaceShape3D *>(p_b);

	SeparatorAxisTest<GodotCylinderShape3D, GodotFaceShape3D, withMargin> separator(cylinder_A, p_transform_a, face_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	if (!separator.test_previous_axis()) {
		return;
	}

	Vector3 vertex[3] = {
		p_transform_b.xform(face_B->vertex[0]),
		p_transform_b.xform(face_B->vertex[1]),
		p_transform_b.xform(face_B->vertex[2]),
	};

	Vector3 normal = (vertex[0] - vertex[2]).cross(vertex[0] - vertex[1]).normalized();

	// Face B normal.
	if (!separator.test_axis(normal)) {
		return;
	}

	Vector3 cyl_axis = p_transform_a.basis.get_column(1).normalized();
	if (cyl_axis.dot(normal) < 0.0) {
		cyl_axis *= -1.0;
	}

	// Cylinder end caps.
	if (!separator.test_axis(cyl_axis)) {
		return;
	}

	// Edges of B, cylinder lateral surface.
	for (int i = 0; i < 3; i++) {
		Vector3 edge_axis = vertex[i] - vertex[(i + 1) % 3];
		Vector3 axis = edge_axis.cross(cyl_axis);
		if (Math::is_zero_approx(axis.length_squared())) {
			continue;
		}

		if (axis.dot(normal) < 0.0) {
			axis *= -1.0;
		}

		if (!separator.test_axis(axis.normalized())) {
			return;
		}
	}

	// Points of B, cylinder lateral surface.
	for (int i = 0; i < 3; i++) {
		const Vector3 point = vertex[i] - p_transform_a.origin;
		Vector3 axis = Plane(cyl_axis).project(point).normalized();
		if (axis.dot(normal) < 0.0) {
			axis *= -1.0;
		}

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// Edges of B, cylinder end caps rim.
	Vector3 cap_axis = cyl_axis * (cylinder_A->get_height() * 0.5);

	for (int i = 0; i < 2; i++) {
		Vector3 cap_pos = p_transform_a.origin + ((i == 0) ? cap_axis : -cap_axis);

		for (int j = 0; j < 3; j++) {
			const Vector3 &edge_start = vertex[j];
			const Vector3 &edge_end = vertex[(j + 1) % 3];
			Vector3 edge_dir = edge_end - edge_start;
			edge_dir.normalize();

			real_t edge_dot = edge_dir.dot(cyl_axis);
			if (Math::is_zero_approx(edge_dot)) {
				// Edge is perpendicular to cylinder axis.
				continue;
			}

			// Calculate intersection between edge and circle plane.
			Vector3 edge_diff = cap_pos - edge_start;
			real_t diff_dot = edge_diff.dot(cyl_axis);
			Vector3 intersection = edge_start + edge_dir * diff_dot / edge_dot;

			// Calculate tangent that touches intersection.
			Vector3 tangent = (cap_pos - intersection).cross(cyl_axis);

			// Axis is orthogonal both to tangent and edge direction.
			Vector3 axis = tangent.cross(edge_dir);
			if (axis.dot(normal) < 0.0) {
				axis *= -1.0;
			}

			if (!separator.test_axis(axis.normalized())) {
				return;
			}
		}
	}

	if (!face_B->backface_collision) {
		if (separator.best_axis.dot(normal) < _BACKFACE_NORMAL_THRESHOLD) {
			if (face_B->invert_backface_collision) {
				separator.best_axis = separator.best_axis.bounce(normal);
			} else {
				// Just ignore backface collision.
				return;
			}
		}
	}

	separator.generate_contacts();
}

static _FORCE_INLINE_ bool is_minkowski_face(const Vector3 &A, const Vector3 &B, const Vector3 &B_x_A, const Vector3 &C, const Vector3 &D, const Vector3 &D_x_C) {
	// Test if arcs AB and CD intersect on the unit sphere
	real_t CBA = C.dot(B_x_A);
	real_t DBA = D.dot(B_x_A);
	real_t ADC = A.dot(D_x_C);
	real_t BDC = B.dot(D_x_C);

	return (CBA * DBA < 0.0f) && (ADC * BDC < 0.0f) && (CBA * BDC > 0.0f);
}

template <bool withMargin>
static void _collision_convex_polygon_convex_polygon(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotConvexPolygonShape3D *convex_polygon_A = static_cast<const GodotConvexPolygonShape3D *>(p_a);
	const GodotConvexPolygonShape3D *convex_polygon_B = static_cast<const GodotConvexPolygonShape3D *>(p_b);

	SeparatorAxisTest<GodotConvexPolygonShape3D, GodotConvexPolygonShape3D, withMargin> separator(convex_polygon_A, p_transform_a, convex_polygon_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	if (!separator.test_previous_axis()) {
		return;
	}

	const Geometry3D::MeshData &mesh_A = convex_polygon_A->get_mesh();

	const Geometry3D::MeshData::Face *faces_A = mesh_A.faces.ptr();
	int face_count_A = mesh_A.faces.size();
	const Geometry3D::MeshData::Edge *edges_A = mesh_A.edges.ptr();
	int edge_count_A = mesh_A.edges.size();
	const Vector3 *vertices_A = mesh_A.vertices.ptr();
	int vertex_count_A = mesh_A.vertices.size();

	const Geometry3D::MeshData &mesh_B = convex_polygon_B->get_mesh();

	const Geometry3D::MeshData::Face *faces_B = mesh_B.faces.ptr();
	int face_count_B = mesh_B.faces.size();
	const Geometry3D::MeshData::Edge *edges_B = mesh_B.edges.ptr();
	int edge_count_B = mesh_B.edges.size();
	const Vector3 *vertices_B = mesh_B.vertices.ptr();
	int vertex_count_B = mesh_B.vertices.size();

	// Precalculating this makes the transforms faster.
	Basis a_xform_normal = p_transform_a.basis.inverse().transposed();

	// faces of A
	for (int i = 0; i < face_count_A; i++) {
		Vector3 axis = a_xform_normal.xform(faces_A[i].plane.normal).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// Precalculating this makes the transforms faster.
	Basis b_xform_normal = p_transform_b.basis.inverse().transposed();

	// faces of B
	for (int i = 0; i < face_count_B; i++) {
		Vector3 axis = b_xform_normal.xform(faces_B[i].plane.normal).normalized();

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// A<->B edges

	for (int i = 0; i < edge_count_A; i++) {
		Vector3 p1 = p_transform_a.xform(vertices_A[edges_A[i].vertex_a]);
		Vector3 q1 = p_transform_a.xform(vertices_A[edges_A[i].vertex_b]);
		Vector3 e1 = q1 - p1;
		Vector3 u1 = p_transform_a.basis.xform(faces_A[edges_A[i].face_a].plane.normal).normalized();
		Vector3 v1 = p_transform_a.basis.xform(faces_A[edges_A[i].face_b].plane.normal).normalized();

		for (int j = 0; j < edge_count_B; j++) {
			Vector3 p2 = p_transform_b.xform(vertices_B[edges_B[j].vertex_a]);
			Vector3 q2 = p_transform_b.xform(vertices_B[edges_B[j].vertex_b]);
			Vector3 e2 = q2 - p2;
			Vector3 u2 = p_transform_b.basis.xform(faces_B[edges_B[j].face_a].plane.normal).normalized();
			Vector3 v2 = p_transform_b.basis.xform(faces_B[edges_B[j].face_b].plane.normal).normalized();

			if (is_minkowski_face(u1, v1, -e1, -u2, -v2, -e2)) {
				Vector3 axis = e1.cross(e2).normalized();

				if (!separator.test_axis(axis)) {
					return;
				}
			}
		}
	}

	if (withMargin) {
		//vertex-vertex
		for (int i = 0; i < vertex_count_A; i++) {
			Vector3 va = p_transform_a.xform(vertices_A[i]);

			for (int j = 0; j < vertex_count_B; j++) {
				if (!separator.test_axis((va - p_transform_b.xform(vertices_B[j])).normalized())) {
					return;
				}
			}
		}
		//edge-vertex (shell)

		for (int i = 0; i < edge_count_A; i++) {
			Vector3 e1 = p_transform_a.basis.xform(vertices_A[edges_A[i].vertex_a]);
			Vector3 e2 = p_transform_a.basis.xform(vertices_A[edges_A[i].vertex_b]);
			Vector3 n = (e2 - e1);

			for (int j = 0; j < vertex_count_B; j++) {
				Vector3 e3 = p_transform_b.xform(vertices_B[j]);

				if (!separator.test_axis((e1 - e3).cross(n).cross(n).normalized())) {
					return;
				}
			}
		}

		for (int i = 0; i < edge_count_B; i++) {
			Vector3 e1 = p_transform_b.basis.xform(vertices_B[edges_B[i].vertex_a]);
			Vector3 e2 = p_transform_b.basis.xform(vertices_B[edges_B[i].vertex_b]);
			Vector3 n = (e2 - e1);

			for (int j = 0; j < vertex_count_A; j++) {
				Vector3 e3 = p_transform_a.xform(vertices_A[j]);

				if (!separator.test_axis((e1 - e3).cross(n).cross(n).normalized())) {
					return;
				}
			}
		}
	}

	separator.generate_contacts();
}

template <bool withMargin>
static void _collision_convex_polygon_face(const GodotShape3D *p_a, const Transform3D &p_transform_a, const GodotShape3D *p_b, const Transform3D &p_transform_b, _CollectorCallback *p_collector, real_t p_margin_a, real_t p_margin_b) {
	const GodotConvexPolygonShape3D *convex_polygon_A = static_cast<const GodotConvexPolygonShape3D *>(p_a);
	const GodotFaceShape3D *face_B = static_cast<const GodotFaceShape3D *>(p_b);

	SeparatorAxisTest<GodotConvexPolygonShape3D, GodotFaceShape3D, withMargin> separator(convex_polygon_A, p_transform_a, face_B, p_transform_b, p_collector, p_margin_a, p_margin_b);

	const Geometry3D::MeshData &mesh = convex_polygon_A->get_mesh();

	const Geometry3D::MeshData::Face *faces = mesh.faces.ptr();
	int face_count = mesh.faces.size();
	const Geometry3D::MeshData::Edge *edges = mesh.edges.ptr();
	int edge_count = mesh.edges.size();
	const Vector3 *vertices = mesh.vertices.ptr();
	int vertex_count = mesh.vertices.size();

	Vector3 vertex[3] = {
		p_transform_b.xform(face_B->vertex[0]),
		p_transform_b.xform(face_B->vertex[1]),
		p_transform_b.xform(face_B->vertex[2]),
	};

	Vector3 normal = (vertex[0] - vertex[2]).cross(vertex[0] - vertex[1]).normalized();

	if (!separator.test_axis(normal)) {
		return;
	}

	// faces of A
	for (int i = 0; i < face_count; i++) {
		//Vector3 axis = p_transform_a.xform( faces[i].plane ).normal;
		Vector3 axis = p_transform_a.basis.xform(faces[i].plane.normal).normalized();
		if (axis.dot(normal) < 0.0) {
			axis *= -1.0;
		}

		if (!separator.test_axis(axis)) {
			return;
		}
	}

	// A<->B edges
	for (int i = 0; i < edge_count; i++) {
		Vector3 e1 = p_transform_a.xform(vertices[edges[i].vertex_a]) - p_transform_a.xform(vertices[edges[i].vertex_b]);

		for (int j = 0; j < 3; j++) {
			Vector3 e2 = vertex[j] - vertex[(j + 1) % 3];

			Vector3 axis = e1.cross(e2).normalized();
			if (axis.dot(normal) < 0.0) {
				axis *= -1.0;
			}

			if (!separator.test_axis(axis)) {
				return;
			}
		}
	}

	if (withMargin) {
		//vertex-vertex
		for (int i = 0; i < vertex_count; i++) {
			Vector3 va = p_transform_a.xform(vertices[i]);

			for (int j = 0; j < 3; j++) {
				Vector3 axis = (va - vertex[j]).normalized();
				if (axis.dot(normal) < 0.0) {
					axis *= -1.0;
				}

				if (!separator.test_axis(axis)) {
					return;
				}
			}
		}
		//edge-vertex (shell)

		for (int i = 0; i < edge_count; i++) {
			Vector3 e1 = p_transform_a.basis.xform(vertices[edges[i].vertex_a]);
			Vector3 e2 = p_transform_a.basis.xform(vertices[edges[i].vertex_b]);
			Vector3 n = (e2 - e1);

			for (int j = 0; j < 3; j++) {
				Vector3 e3 = vertex[j];

				Vector3 axis = (e1 - e3).cross(n).cross(n).normalized();
				if (axis.dot(normal) < 0.0) {
					axis *= -1.0;
				}

				if (!separator.test_axis(axis)) {
					return;
				}
			}
		}

		for (int i = 0; i < 3; i++) {
			Vector3 e1 = vertex[i];
			Vector3 e2 = vertex[(i + 1) % 3];
			Vector3 n = (e2 - e1);

			for (int j = 0; j < vertex_count; j++) {
				Vector3 e3 = p_transform_a.xform(vertices[j]);

				Vector3 axis = (e1 - e3).cross(n).cross(n).normalized();
				if (axis.dot(normal) < 0.0) {
					axis *= -1.0;
				}

				if (!separator.test_axis(axis)) {
					return;
				}
			}
		}
	}

	if (!face_B->backface_collision) {
		if (separator.best_axis.dot(normal) < _BACKFACE_NORMAL_THRESHOLD) {
			if (face_B->invert_backface_collision) {
				separator.best_axis = separator.best_axis.bounce(normal);
			} else {
				// Just ignore backface collision.
				return;
			}
		}
	}

	separator.generate_contacts();
}

bool sat_calculate_penetration(const GodotShape3D *p_shape_A, const Transform3D &p_transform_A, const GodotShape3D *p_shape_B, const Transform3D &p_transform_B, GodotCollisionSolver3D::CallbackResult p_result_callback, void *p_userdata, bool p_swap, Vector3 *r_prev_axis, real_t p_margin_a, real_t p_margin_b) {
	PhysicsServer3D::ShapeType type_A = p_shape_A->get_type();

	ERR_FAIL_COND_V(type_A == PhysicsServer3D::SHAPE_WORLD_BOUNDARY, false);
	ERR_FAIL_COND_V(type_A == PhysicsServer3D::SHAPE_SEPARATION_RAY, false);
	ERR_FAIL_COND_V(p_shape_A->is_concave(), false);

	PhysicsServer3D::ShapeType type_B = p_shape_B->get_type();

	ERR_FAIL_COND_V(type_B == PhysicsServer3D::SHAPE_WORLD_BOUNDARY, false);
	ERR_FAIL_COND_V(type_B == PhysicsServer3D::SHAPE_SEPARATION_RAY, false);
	ERR_FAIL_COND_V(p_shape_B->is_concave(), false);

	static const CollisionFunc collision_table[6][6] = {
		{ _collision_sphere_sphere<false>,
				_collision_sphere_box<false>,
				_collision_sphere_capsule<false>,
				_collision_sphere_cylinder<false>,
				_collision_sphere_convex_polygon<false>,
				_collision_sphere_face<false> },
		{ nullptr,
				_collision_box_box<false>,
				_collision_box_capsule<false>,
				_collision_box_cylinder<false>,
				_collision_box_convex_polygon<false>,
				_collision_box_face<false> },
		{ nullptr,
				nullptr,
				_collision_capsule_capsule<false>,
				_collision_capsule_cylinder<false>,
				_collision_capsule_convex_polygon<false>,
				_collision_capsule_face<false> },
		{ nullptr,
				nullptr,
				nullptr,
				_collision_cylinder_cylinder<false>,
				_collision_cylinder_convex_polygon<false>,
				_collision_cylinder_face<false> },
		{ nullptr,
				nullptr,
				nullptr,
				nullptr,
				_collision_convex_polygon_convex_polygon<false>,
				_collision_convex_polygon_face<false> },
		{ nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr },
	};

	static const CollisionFunc collision_table_margin[6][6] = {
		{ _collision_sphere_sphere<true>,
				_collision_sphere_box<true>,
				_collision_sphere_capsule<true>,
				_collision_sphere_cylinder<true>,
				_collision_sphere_convex_polygon<true>,
				_collision_sphere_face<true> },
		{ nullptr,
				_collision_box_box<true>,
				_collision_box_capsule<true>,
				_collision_box_cylinder<true>,
				_collision_box_convex_polygon<true>,
				_collision_box_face<true> },
		{ nullptr,
				nullptr,
				_collision_capsule_capsule<true>,
				_collision_capsule_cylinder<true>,
				_collision_capsule_convex_polygon<true>,
				_collision_capsule_face<true> },
		{ nullptr,
				nullptr,
				nullptr,
				_collision_cylinder_cylinder<true>,
				_collision_cylinder_convex_polygon<true>,
				_collision_cylinder_face<true> },
		{ nullptr,
				nullptr,
				nullptr,
				nullptr,
				_collision_convex_polygon_convex_polygon<true>,
				_collision_convex_polygon_face<true> },
		{ nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				nullptr },
	};

	_CollectorCallback callback;
	callback.callback = p_result_callback;
	callback.swap = p_swap;
	callback.userdata = p_userdata;
	callback.collided = false;
	callback.prev_axis = r_prev_axis;

	const GodotShape3D *A = p_shape_A;
	const GodotShape3D *B = p_shape_B;
	const Transform3D *transform_A = &p_transform_A;
	const Transform3D *transform_B = &p_transform_B;
	real_t margin_A = p_margin_a;
	real_t margin_B = p_margin_b;

	if (type_A > type_B) {
		SWAP(A, B);
		SWAP(transform_A, transform_B);
		SWAP(type_A, type_B);
		SWAP(margin_A, margin_B);
		callback.swap = !callback.swap;
	}

	CollisionFunc collision_func;
	if (margin_A != 0.0 || margin_B != 0.0) {
		collision_func = collision_table_margin[type_A - 2][type_B - 2];

	} else {
		collision_func = collision_table[type_A - 2][type_B - 2];
	}
	ERR_FAIL_NULL_V(collision_func, false);

	collision_func(A, *transform_A, B, *transform_B, &callback, margin_A, margin_B);

	return callback.collided;
}
