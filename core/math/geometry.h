/**************************************************************************/
/*  geometry.h                                                            */
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

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "core/math/delaunay.h"
#include "core/math/face3.h"
#include "core/math/rect2.h"
#include "core/math/triangulate.h"
#include "core/math/vector3.h"
#include "core/object.h"
#include "core/pool_vector.h"
#include "core/print_string.h"
#include "core/vector.h"

class Geometry {
public:
	static real_t get_closest_points_between_segments(const Vector2 &p1, const Vector2 &q1, const Vector2 &p2, const Vector2 &q2, Vector2 &c1, Vector2 &c2) {
		Vector2 d1 = q1 - p1; // Direction vector of segment S1.
		Vector2 d2 = q2 - p2; // Direction vector of segment S2.
		Vector2 r = p1 - p2;
		real_t a = d1.dot(d1); // Squared length of segment S1, always nonnegative.
		real_t e = d2.dot(d2); // Squared length of segment S2, always nonnegative.
		real_t f = d2.dot(r);
		real_t s, t;
		// Check if either or both segments degenerate into points.
		if (a <= (real_t)CMP_EPSILON && e <= (real_t)CMP_EPSILON) {
			// Both segments degenerate into points.
			c1 = p1;
			c2 = p2;
			return Math::sqrt((c1 - c2).dot(c1 - c2));
		}
		if (a <= (real_t)CMP_EPSILON) {
			// First segment degenerates into a point.
			s = 0;
			t = f / e; // s = 0 => t = (b*s + f) / e = f / e
			t = CLAMP(t, 0, 1);
		} else {
			real_t c = d1.dot(r);
			if (e <= (real_t)CMP_EPSILON) {
				// Second segment degenerates into a point.
				t = 0;
				s = CLAMP(-c / a, 0, 1); // t = 0 => s = (b*t - c) / a = -c / a
			} else {
				// The general nondegenerate case starts here.
				real_t b = d1.dot(d2);
				real_t denom = a * e - b * b; // Always nonnegative.
				// If segments not parallel, compute closest point on L1 to L2 and
				// clamp to segment S1. Else pick arbitrary s (here 0).
				if (denom != 0) {
					s = CLAMP((b * f - c * e) / denom, 0, 1);
				} else {
					s = 0;
				}
				// Compute point on L2 closest to S1(s) using
				// t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
				t = (b * s + f) / e;

				//If t in [0,1] done. Else clamp t, recompute s for the new value
				// of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
				// and clamp s to [0, 1].
				if (t < 0) {
					t = 0;
					s = CLAMP(-c / a, 0, 1);
				} else if (t > 1) {
					t = 1;
					s = CLAMP((b - c) / a, 0, 1);
				}
			}
		}
		c1 = p1 + d1 * s;
		c2 = p2 + d2 * t;
		return Math::sqrt((c1 - c2).dot(c1 - c2));
	}

	static void get_closest_points_between_segments(const Vector3 &p_p0, const Vector3 &p_p1, const Vector3 &p_q0, const Vector3 &p_q1, Vector3 &r_ps, Vector3 &r_qt);
	static real_t get_closest_distance_between_segments(const Vector3 &p_p0, const Vector3 &p_p1, const Vector3 &p_q0, const Vector3 &p_q1);

	static inline bool ray_intersects_triangle(const Vector3 &p_from, const Vector3 &p_dir, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, Vector3 *r_res = nullptr) {
		Vector3 e1 = p_v1 - p_v0;
		Vector3 e2 = p_v2 - p_v0;
		Vector3 h = p_dir.cross(e2);
		real_t a = e1.dot(h);
		if (Math::is_zero_approx(a)) { // Parallel test.
			return false;
		}

		real_t f = 1 / a;

		Vector3 s = p_from - p_v0;
		real_t u = f * s.dot(h);

		if ((u < 0) || (u > 1)) {
			return false;
		}

		Vector3 q = s.cross(e1);

		real_t v = f * p_dir.dot(q);

		if ((v < 0) || (u + v > 1)) {
			return false;
		}

		// At this stage we can compute t to find out where
		// the intersection point is on the line.
		real_t t = f * e2.dot(q);

		if (t > 0.00001f) { // ray intersection
			if (r_res) {
				*r_res = p_from + p_dir * t;
			}
			return true;
		} else { // This means that there is a line intersection but not a ray intersection.
			return false;
		}
	}

	static inline bool segment_intersects_triangle(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, Vector3 *r_res = nullptr) {
		Vector3 rel = p_to - p_from;
		Vector3 e1 = p_v1 - p_v0;
		Vector3 e2 = p_v2 - p_v0;
		Vector3 h = rel.cross(e2);
		real_t a = e1.dot(h);
		if (Math::is_zero_approx(a)) { // Parallel test.
			return false;
		}

		real_t f = 1 / a;

		Vector3 s = p_from - p_v0;
		real_t u = f * s.dot(h);

		if ((u < 0) || (u > 1)) {
			return false;
		}

		Vector3 q = s.cross(e1);

		real_t v = f * rel.dot(q);

		if ((v < 0) || (u + v > 1)) {
			return false;
		}

		// At this stage we can compute t to find out where
		// the intersection point is on the line.
		real_t t = f * e2.dot(q);

		if (t > (real_t)CMP_EPSILON && t <= 1) { // Ray intersection.
			if (r_res) {
				*r_res = p_from + rel * t;
			}
			return true;
		} else { // This means that there is a line intersection but not a ray intersection.
			return false;
		}
	}

	static inline bool segment_intersects_sphere(const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_sphere_pos, real_t p_sphere_radius, Vector3 *r_res = nullptr, Vector3 *r_norm = nullptr) {
		Vector3 sphere_pos = p_sphere_pos - p_from;
		Vector3 rel = (p_to - p_from);
		real_t rel_l = rel.length();
		if (rel_l < (real_t)CMP_EPSILON) {
			return false; // Both points are the same.
		}
		Vector3 normal = rel / rel_l;

		real_t sphere_d = normal.dot(sphere_pos);

		real_t ray_distance = sphere_pos.distance_to(normal * sphere_d);

		if (ray_distance >= p_sphere_radius) {
			return false;
		}

		real_t inters_d2 = p_sphere_radius * p_sphere_radius - ray_distance * ray_distance;
		real_t inters_d = sphere_d;

		if (inters_d2 >= (real_t)CMP_EPSILON) {
			inters_d -= Math::sqrt(inters_d2);
		}

		// Check in segment.
		if (inters_d < 0 || inters_d > rel_l) {
			return false;
		}

		Vector3 result = p_from + normal * inters_d;

		if (r_res) {
			*r_res = result;
		}
		if (r_norm) {
			*r_norm = (result - p_sphere_pos).normalized();
		}

		return true;
	}

	static inline bool segment_intersects_cylinder(const Vector3 &p_from, const Vector3 &p_to, real_t p_height, real_t p_radius, Vector3 *r_res = nullptr, Vector3 *r_norm = nullptr, int p_cylinder_axis = 2) {
		Vector3 rel = (p_to - p_from);
		real_t rel_l = rel.length();
		if (rel_l < (real_t)CMP_EPSILON) {
			return false; // Both points are the same.
		}

		ERR_FAIL_COND_V(p_cylinder_axis < 0, false);
		ERR_FAIL_COND_V(p_cylinder_axis > 2, false);
		Vector3 cylinder_axis;
		cylinder_axis[p_cylinder_axis] = 1;

		// First check if they are parallel.
		Vector3 normal = (rel / rel_l);
		Vector3 crs = normal.cross(cylinder_axis);
		real_t crs_l = crs.length();

		Vector3 axis_dir;

		if (crs_l < (real_t)CMP_EPSILON) {
			Vector3 side_axis;
			side_axis[(p_cylinder_axis + 1) % 3] = 1; // Any side axis OK.
			axis_dir = side_axis;
		} else {
			axis_dir = crs / crs_l;
		}

		real_t dist = axis_dir.dot(p_from);

		if (dist >= p_radius) {
			return false; // Too far away.
		}

		// Convert to 2D.
		real_t w2 = p_radius * p_radius - dist * dist;
		if (w2 < (real_t)CMP_EPSILON) {
			return false; // Avoid numerical error.
		}
		Size2 size(Math::sqrt(w2), p_height * 0.5f);

		Vector3 side_dir = axis_dir.cross(cylinder_axis).normalized();

		Vector2 from2D(side_dir.dot(p_from), p_from[p_cylinder_axis]);
		Vector2 to2D(side_dir.dot(p_to), p_to[p_cylinder_axis]);

		real_t min = 0, max = 1;

		int axis = -1;

		for (int i = 0; i < 2; i++) {
			real_t seg_from = from2D[i];
			real_t seg_to = to2D[i];
			real_t box_begin = -size[i];
			real_t box_end = size[i];
			real_t cmin, cmax;

			if (seg_from < seg_to) {
				if (seg_from > box_end || seg_to < box_begin) {
					return false;
				}
				real_t length = seg_to - seg_from;
				cmin = (seg_from < box_begin) ? ((box_begin - seg_from) / length) : 0;
				cmax = (seg_to > box_end) ? ((box_end - seg_from) / length) : 1;

			} else {
				if (seg_to > box_end || seg_from < box_begin) {
					return false;
				}
				real_t length = seg_to - seg_from;
				cmin = (seg_from > box_end) ? (box_end - seg_from) / length : 0;
				cmax = (seg_to < box_begin) ? (box_begin - seg_from) / length : 1;
			}

			if (cmin > min) {
				min = cmin;
				axis = i;
			}
			if (cmax < max) {
				max = cmax;
			}
			if (max < min) {
				return false;
			}
		}

		// Convert to 3D again.
		Vector3 result = p_from + (rel * min);
		Vector3 res_normal = result;

		if (axis == 0) {
			res_normal[p_cylinder_axis] = 0;
		} else {
			int axis_side = (p_cylinder_axis + 1) % 3;
			res_normal[axis_side] = 0;
			axis_side = (axis_side + 1) % 3;
			res_normal[axis_side] = 0;
		}

		res_normal.normalize();

		if (r_res) {
			*r_res = result;
		}
		if (r_norm) {
			*r_norm = res_normal;
		}

		return true;
	}

	static bool segment_intersects_convex(const Vector3 &p_from, const Vector3 &p_to, const Plane *p_planes, int p_plane_count, Vector3 *p_res, Vector3 *p_norm) {
		real_t min = -1e20, max = 1e20;

		Vector3 rel = p_to - p_from;
		real_t rel_l = rel.length();

		if (rel_l < (real_t)CMP_EPSILON) {
			return false;
		}

		Vector3 dir = rel / rel_l;

		int min_index = -1;

		for (int i = 0; i < p_plane_count; i++) {
			const Plane &p = p_planes[i];

			real_t den = p.normal.dot(dir);

			if (Math::abs(den) <= (real_t)CMP_EPSILON) {
				continue; // Ignore parallel plane.
			}

			real_t dist = -p.distance_to(p_from) / den;

			if (den > 0) {
				// Backwards facing plane.
				if (dist < max) {
					max = dist;
				}
			} else {
				// Front facing plane.
				if (dist > min) {
					min = dist;
					min_index = i;
				}
			}
		}

		if (max <= min || min < 0 || min > rel_l || min_index == -1) { // Exit conditions.
			return false; // No intersection.
		}

		if (p_res) {
			*p_res = p_from + dir * min;
		}
		if (p_norm) {
			*p_norm = p_planes[min_index].normal;
		}

		return true;
	}

	static Vector3 get_closest_point_to_segment(const Vector3 &p_point, const Vector3 *p_segment) {
		Vector3 p = p_point - p_segment[0];
		Vector3 n = p_segment[1] - p_segment[0];
		real_t l2 = n.length_squared();
		if (l2 < 1e-20f) {
			return p_segment[0]; // Both points are the same, just give any.
		}

		real_t d = n.dot(p) / l2;

		if (d <= 0) {
			return p_segment[0]; // Before first point.
		} else if (d >= 1.0) {
			return p_segment[1]; // After first point.
		} else {
			return p_segment[0] + n * d; // Inside.
		}
	}

	static Vector3 get_closest_point_to_segment_uncapped(const Vector3 &p_point, const Vector3 *p_segment) {
		Vector3 p = p_point - p_segment[0];
		Vector3 n = p_segment[1] - p_segment[0];
		real_t l2 = n.length_squared();
		if (l2 < 1e-20f) {
			return p_segment[0]; // Both points are the same, just give any.
		}

		real_t d = n.dot(p) / l2;

		return p_segment[0] + n * d; // Inside.
	}

	static Vector2 get_closest_point_to_segment_2d(const Vector2 &p_point, const Vector2 *p_segment) {
		Vector2 p = p_point - p_segment[0];
		Vector2 n = p_segment[1] - p_segment[0];
		real_t l2 = n.length_squared();
		if (l2 < 1e-20f) {
			return p_segment[0]; // Both points are the same, just give any.
		}

		real_t d = n.dot(p) / l2;

		if (d <= 0) {
			return p_segment[0]; // Before first point.
		} else if (d >= 1) {
			return p_segment[1]; // After first point.
		} else {
			return p_segment[0] + n * d; // Inside.
		}
	}

	static bool is_point_in_triangle(const Vector2 &s, const Vector2 &a, const Vector2 &b, const Vector2 &c) {
		Vector2 an = a - s;
		Vector2 bn = b - s;
		Vector2 cn = c - s;

		bool orientation = an.cross(bn) > 0;

		if ((bn.cross(cn) > 0) != orientation) {
			return false;
		}

		return (cn.cross(an) > 0) == orientation;
	}

	static Vector3 barycentric_coordinates_2d(const Vector2 &s, const Vector2 &a, const Vector2 &b, const Vector2 &c) {
		// http://www.blackpawn.com/texts/pointinpoly/
		Vector2 v0 = c - a;
		Vector2 v1 = b - a;
		Vector2 v2 = s - a;

		// Compute dot products
		double dot00 = v0.dot(v0);
		double dot01 = v0.dot(v1);
		double dot02 = v0.dot(v2);
		double dot11 = v1.dot(v1);
		double dot12 = v1.dot(v2);

		// Check for divide by zero
		double denom = dot00 * dot11 - dot01 * dot01;
		if (denom == 0.0) {
			return Vector3(0.0, 0.0, 0.0);
		}

		// Compute barycentric coordinates
		double invDenom = 1.0 / denom;
		double b2 = (dot11 * dot02 - dot01 * dot12) * invDenom;
		double b1 = (dot00 * dot12 - dot01 * dot02) * invDenom;
		double b0 = 1.0 - b2 - b1;
		return Vector3(b0, b1, b2);
	}

	static Vector2 get_closest_point_to_segment_uncapped_2d(const Vector2 &p_point, const Vector2 *p_segment) {
		Vector2 p = p_point - p_segment[0];
		Vector2 n = p_segment[1] - p_segment[0];
		real_t l2 = n.length_squared();
		if (l2 < 1e-20f) {
			return p_segment[0]; // Both points are the same, just give any.
		}

		real_t d = n.dot(p) / l2;

		return p_segment[0] + n * d; // Inside.
	}

	static bool line_intersects_line_2d(const Vector2 &p_from_a, const Vector2 &p_dir_a, const Vector2 &p_from_b, const Vector2 &p_dir_b, Vector2 &r_result) {
		// See http://paulbourke.net/geometry/pointlineplane/

		const real_t denom = p_dir_b.y * p_dir_a.x - p_dir_b.x * p_dir_a.y;
		if (Math::is_zero_approx(denom)) { // Parallel?
			return false;
		}

		const Vector2 v = p_from_a - p_from_b;
		const real_t t = (p_dir_b.x * v.y - p_dir_b.y * v.x) / denom;
		r_result = p_from_a + t * p_dir_a;
		return true;
	}

	static bool segment_intersects_segment_2d(const Vector2 &p_from_a, const Vector2 &p_to_a, const Vector2 &p_from_b, const Vector2 &p_to_b, Vector2 *r_result) {
		Vector2 B = p_to_a - p_from_a;
		Vector2 C = p_from_b - p_from_a;
		Vector2 D = p_to_b - p_from_a;

		real_t ABlen = B.dot(B);
		if (ABlen <= 0) {
			return false;
		}
		Vector2 Bn = B / ABlen;
		C = Vector2(C.x * Bn.x + C.y * Bn.y, C.y * Bn.x - C.x * Bn.y);
		D = Vector2(D.x * Bn.x + D.y * Bn.y, D.y * Bn.x - D.x * Bn.y);

		if ((C.y < 0 && D.y < 0) || (C.y >= 0 && D.y >= 0)) {
			return false;
		}

		real_t ABpos = D.x + (C.x - D.x) * D.y / (D.y - C.y);

		// Fail if segment C-D crosses line A-B outside of segment A-B.
		if ((ABpos < 0) || (ABpos > 1)) {
			return false;
		}

		// (4) Apply the discovered position to line A-B in the original coordinate system.
		if (r_result) {
			*r_result = p_from_a + B * ABpos;
		}

		return true;
	}

	static inline bool point_in_projected_triangle(const Vector3 &p_point, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3) {
		Vector3 face_n = (p_v1 - p_v3).cross(p_v1 - p_v2);

		Vector3 n1 = (p_point - p_v3).cross(p_point - p_v2);

		if (face_n.dot(n1) < 0) {
			return false;
		}

		Vector3 n2 = (p_v1 - p_v3).cross(p_v1 - p_point);

		if (face_n.dot(n2) < 0) {
			return false;
		}

		Vector3 n3 = (p_v1 - p_point).cross(p_v1 - p_v2);

		if (face_n.dot(n3) < 0) {
			return false;
		}

		return true;
	}

	static inline bool triangle_sphere_intersection_test(const Vector3 *p_triangle, const Vector3 &p_normal, const Vector3 &p_sphere_pos, real_t p_sphere_radius, Vector3 &r_triangle_contact, Vector3 &r_sphere_contact) {
		real_t d = p_normal.dot(p_sphere_pos) - p_normal.dot(p_triangle[0]);

		if (d > p_sphere_radius || d < -p_sphere_radius) { // Not touching the plane of the face, return.
			return false;
		}

		Vector3 contact = p_sphere_pos - (p_normal * d);

		/** 2nd) TEST INSIDE TRIANGLE **/

		if (Geometry::point_in_projected_triangle(contact, p_triangle[0], p_triangle[1], p_triangle[2])) {
			r_triangle_contact = contact;
			r_sphere_contact = p_sphere_pos - p_normal * p_sphere_radius;
			//printf("solved inside triangle\n");
			return true;
		}

		/** 3rd TEST INSIDE EDGE CYLINDERS **/

		const Vector3 verts[4] = { p_triangle[0], p_triangle[1], p_triangle[2], p_triangle[0] }; // for() friendly

		for (int i = 0; i < 3; i++) {
			// Check edge cylinder.

			Vector3 n1 = verts[i] - verts[i + 1];
			Vector3 n2 = p_sphere_pos - verts[i + 1];

			///@TODO Maybe discard by range here to make the algorithm quicker.

			// Check point within cylinder radius.
			Vector3 axis = n1.cross(n2).cross(n1);
			axis.normalize();

			real_t ad = axis.dot(n2);

			if (ABS(ad) > p_sphere_radius) {
				// No chance with this edge, too far away.
				continue;
			}

			// Check point within edge capsule cylinder.
			/** 4th TEST INSIDE EDGE POINTS **/

			real_t sphere_at = n1.dot(n2);

			if (sphere_at >= 0 && sphere_at < n1.dot(n1)) {
				r_triangle_contact = p_sphere_pos - axis * (axis.dot(n2));
				r_sphere_contact = p_sphere_pos - axis * p_sphere_radius;
				// Point inside here.
				return true;
			}

			real_t r2 = p_sphere_radius * p_sphere_radius;

			if (n2.length_squared() < r2) {
				Vector3 n = (p_sphere_pos - verts[i + 1]).normalized();

				r_triangle_contact = verts[i + 1];
				r_sphere_contact = p_sphere_pos - n * p_sphere_radius;
				return true;
			}

			if (n2.distance_squared_to(n1) < r2) {
				Vector3 n = (p_sphere_pos - verts[i]).normalized();

				r_triangle_contact = verts[i];
				r_sphere_contact = p_sphere_pos - n * p_sphere_radius;
				return true;
			}

			break; // It's pointless to continue at this point, so save some CPU cycles.
		}

		return false;
	}

	static inline bool is_point_in_circle(const Vector2 &p_point, const Vector2 &p_circle_pos, real_t p_circle_radius) {
		return p_point.distance_squared_to(p_circle_pos) <= p_circle_radius * p_circle_radius;
	}

	static real_t segment_intersects_circle(const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_circle_pos, real_t p_circle_radius) {
		Vector2 line_vec = p_to - p_from;
		Vector2 vec_to_line = p_from - p_circle_pos;

		// Create a quadratic formula of the form ax^2 + bx + c = 0
		real_t a, b, c;

		a = line_vec.dot(line_vec);
		b = 2 * vec_to_line.dot(line_vec);
		c = vec_to_line.dot(vec_to_line) - p_circle_radius * p_circle_radius;

		// Solve for t.
		real_t sqrtterm = b * b - 4 * a * c;

		// If the term we intend to square root is less than 0 then the answer won't be real,
		// so it definitely won't be t in the range 0 to 1.
		if (sqrtterm < 0) {
			return -1;
		}

		// If we can assume that the line segment starts outside the circle (e.g. for continuous time collision detection)
		// then the following can be skipped and we can just return the equivalent of res1.
		sqrtterm = Math::sqrt(sqrtterm);
		real_t res1 = (-b - sqrtterm) / (2 * a);
		real_t res2 = (-b + sqrtterm) / (2 * a);

		if (res1 >= 0 && res1 <= 1) {
			return res1;
		}
		if (res2 >= 0 && res2 <= 1) {
			return res2;
		}
		return -1;
	}

	static inline Vector<Vector3> clip_polygon(const Vector<Vector3> &polygon, const Plane &p_plane) {
		enum LocationCache {
			LOC_INSIDE = 1,
			LOC_BOUNDARY = 0,
			LOC_OUTSIDE = -1
		};

		if (polygon.size() == 0) {
			return polygon;
		}

		int *location_cache = (int *)alloca(sizeof(int) * polygon.size());
		int inside_count = 0;
		int outside_count = 0;

		for (int a = 0; a < polygon.size(); a++) {
			real_t dist = p_plane.distance_to(polygon[a]);
			if (dist < (real_t)-CMP_POINT_IN_PLANE_EPSILON) {
				location_cache[a] = LOC_INSIDE;
				inside_count++;
			} else {
				if (dist > (real_t)CMP_POINT_IN_PLANE_EPSILON) {
					location_cache[a] = LOC_OUTSIDE;
					outside_count++;
				} else {
					location_cache[a] = LOC_BOUNDARY;
				}
			}
		}

		if (outside_count == 0) {
			return polygon; // No changes.

		} else if (inside_count == 0) {
			return Vector<Vector3>(); // Empty.
		}

		long previous = polygon.size() - 1;
		Vector<Vector3> clipped;

		for (int index = 0; index < polygon.size(); index++) {
			int loc = location_cache[index];
			if (loc == LOC_OUTSIDE) {
				if (location_cache[previous] == LOC_INSIDE) {
					const Vector3 &v1 = polygon[previous];
					const Vector3 &v2 = polygon[index];

					Vector3 segment = v1 - v2;
					real_t den = p_plane.normal.dot(segment);
					real_t dist = p_plane.distance_to(v1) / den;
					dist = -dist;
					clipped.push_back(v1 + segment * dist);
				}
			} else {
				const Vector3 &v1 = polygon[index];
				if ((loc == LOC_INSIDE) && (location_cache[previous] == LOC_OUTSIDE)) {
					const Vector3 &v2 = polygon[previous];
					Vector3 segment = v1 - v2;
					real_t den = p_plane.normal.dot(segment);
					real_t dist = p_plane.distance_to(v1) / den;
					dist = -dist;
					clipped.push_back(v1 + segment * dist);
				}

				clipped.push_back(v1);
			}

			previous = index;
		}

		return clipped;
	}

	enum PolyBooleanOperation {
		OPERATION_UNION,
		OPERATION_DIFFERENCE,
		OPERATION_INTERSECTION,
		OPERATION_XOR
	};
	enum PolyJoinType {
		JOIN_SQUARE,
		JOIN_ROUND,
		JOIN_MITER
	};
	enum PolyEndType {
		END_POLYGON,
		END_JOINED,
		END_BUTT,
		END_SQUARE,
		END_ROUND
	};

	static Vector<Vector<Point2>> merge_polygons_2d(const Vector<Point2> &p_polygon_a, const Vector<Point2> &p_polygon_b) {
		return _polypaths_do_operation(OPERATION_UNION, p_polygon_a, p_polygon_b);
	}

	static Vector<Vector<Point2>> clip_polygons_2d(const Vector<Point2> &p_polygon_a, const Vector<Point2> &p_polygon_b) {
		return _polypaths_do_operation(OPERATION_DIFFERENCE, p_polygon_a, p_polygon_b);
	}

	static Vector<Vector<Point2>> intersect_polygons_2d(const Vector<Point2> &p_polygon_a, const Vector<Point2> &p_polygon_b) {
		return _polypaths_do_operation(OPERATION_INTERSECTION, p_polygon_a, p_polygon_b);
	}

	static Vector<Vector<Point2>> exclude_polygons_2d(const Vector<Point2> &p_polygon_a, const Vector<Point2> &p_polygon_b) {
		return _polypaths_do_operation(OPERATION_XOR, p_polygon_a, p_polygon_b);
	}

	static Vector<Vector<Point2>> clip_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon) {
		return _polypaths_do_operation(OPERATION_DIFFERENCE, p_polyline, p_polygon, true);
	}

	static Vector<Vector<Point2>> intersect_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon) {
		return _polypaths_do_operation(OPERATION_INTERSECTION, p_polyline, p_polygon, true);
	}

	static Vector<Vector<Point2>> offset_polygon_2d(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type) {
		return _polypath_offset(p_polygon, p_delta, p_join_type, END_POLYGON);
	}

	static Vector<Vector<Point2>> offset_polyline_2d(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type) {
		ERR_FAIL_COND_V_MSG(p_end_type == END_POLYGON, Vector<Vector<Point2>>(), "Attempt to offset a polyline like a polygon (use offset_polygon_2d instead).");

		return _polypath_offset(p_polygon, p_delta, p_join_type, p_end_type);
	}

	static Vector<Vector<Point2>> minkowski_sum_polygon_2d(const Vector<Vector2> &p_polygon_pattern, const Vector<Vector2> &p_polygon_path) {
		return _polypath_minkowski(p_polygon_pattern, p_polygon_path, true, true);
	}

	static Vector<Vector<Point2>> minkowski_diff_polygon_2d(const Vector<Vector2> &p_polygon_pattern, const Vector<Vector2> &p_polygon_path) {
		return _polypath_minkowski(p_polygon_pattern, p_polygon_path, false, true);
	}

	static Vector<Vector<Point2>> minkowski_sum_polyline_2d(const Vector<Vector2> &p_polygon_pattern, const Vector<Vector2> &p_polyline_path) {
		return _polypath_minkowski(p_polygon_pattern, p_polyline_path, true, false);
	}

	static Vector<int> triangulate_delaunay_2d(const Vector<Vector2> &p_points) {
		Vector<Delaunay2D::Triangle> tr = Delaunay2D::triangulate(p_points);
		Vector<int> triangles;

		for (int i = 0; i < tr.size(); i++) {
			triangles.push_back(tr[i].points[0]);
			triangles.push_back(tr[i].points[1]);
			triangles.push_back(tr[i].points[2]);
		}
		return triangles;
	}

	static Vector<int> triangulate_polygon(const Vector<Vector2> &p_polygon) {
		Vector<int> triangles;
		if (!Triangulate::triangulate(p_polygon, triangles)) {
			return Vector<int>(); //fail
		}
		return triangles;
	}

	static bool is_polygon_clockwise(const Vector<Vector2> &p_polygon) {
		int c = p_polygon.size();
		if (c < 3) {
			return false;
		}
		const Vector2 *p = p_polygon.ptr();
		real_t sum = 0;
		for (int i = 0; i < c; i++) {
			const Vector2 &v1 = p[i];
			const Vector2 &v2 = p[(i + 1) % c];
			sum += (v2.x - v1.x) * (v2.y + v1.y);
		}

		return sum > 0.0f;
	}

	// Alternate implementation that should be faster.
	static bool is_point_in_polygon(const Vector2 &p_point, const Vector<Vector2> &p_polygon) {
		int c = p_polygon.size();
		if (c < 3) {
			return false;
		}
		const Vector2 *p = p_polygon.ptr();
		Vector2 further_away(-1e20, -1e20);
		Vector2 further_away_opposite(1e20, 1e20);

		for (int i = 0; i < c; i++) {
			further_away.x = MAX(p[i].x, further_away.x);
			further_away.y = MAX(p[i].y, further_away.y);
			further_away_opposite.x = MIN(p[i].x, further_away_opposite.x);
			further_away_opposite.y = MIN(p[i].y, further_away_opposite.y);
		}

		// Make point outside that won't intersect with points in segment from p_point.
		further_away += (further_away - further_away_opposite) * Vector2(1.221313, 1.512312);

		int intersections = 0;
		for (int i = 0; i < c; i++) {
			const Vector2 &v1 = p[i];
			const Vector2 &v2 = p[(i + 1) % c];
			if (segment_intersects_segment_2d(v1, v2, p_point, further_away, nullptr)) {
				intersections++;
			}
		}

		return (intersections & 1);
	}

	static PoolVector<PoolVector<Face3>> separate_objects(PoolVector<Face3> p_array);

	// Create a "wrap" that encloses the given geometry.
	static PoolVector<Face3> wrap_geometry(PoolVector<Face3> p_array, real_t *p_error = nullptr);

	struct MeshData {
		struct Face {
			Plane plane;
			Vector<int> indices;
		};

		Vector<Face> faces;

		struct Edge {
			int a, b;
		};

		Vector<Edge> edges;

		Vector<Vector3> vertices;

		void optimize_vertices();
		void clear();
	};

	// Occluder Meshes contain convex faces which may contain 0 to many convex holes.
	// (holes are analogous to portals)
	struct OccluderMeshData {
		struct Hole {
			LocalVectori<uint32_t> indices;
		};
		struct Face {
			Plane plane;
			bool two_way = false;
			LocalVectori<uint32_t> indices;
			LocalVectori<Hole> holes;
		};
		LocalVectori<Face> faces;
		LocalVectori<Vector3> vertices;
		void clear();
	};

	_FORCE_INLINE_ static int get_uv84_normal_bit(const Vector3 &p_vector) {
		int lat = Math::fast_ftoi(Math::floor(Math::acos(p_vector.dot(Vector3(0, 1, 0))) * 4.0 / Math_PI + 0.5));

		if (lat == 0) {
			return 24;
		} else if (lat == 4) {
			return 25;
		}

		int lon = Math::fast_ftoi(Math::floor((Math_PI + Math::atan2(p_vector.x, p_vector.z)) * 8.0 / (Math_PI * 2.0) + 0.5)) % 8;

		return lon + (lat - 1) * 8;
	}

	_FORCE_INLINE_ static int get_uv84_normal_bit_neighbors(int p_idx) {
		if (p_idx == 24) {
			return 1 | 2 | 4 | 8;
		} else if (p_idx == 25) {
			return (1 << 23) | (1 << 22) | (1 << 21) | (1 << 20);
		} else {
			int ret = 0;
			if ((p_idx % 8) == 0) {
				ret |= (1 << (p_idx + 7));
			} else {
				ret |= (1 << (p_idx - 1));
			}
			if ((p_idx % 8) == 7) {
				ret |= (1 << (p_idx - 7));
			} else {
				ret |= (1 << (p_idx + 1));
			}

			int mask = ret | (1 << p_idx);
			if (p_idx < 8) {
				ret |= 24;
			} else {
				ret |= mask >> 8;
			}

			if (p_idx >= 16) {
				ret |= 25;
			} else {
				ret |= mask << 8;
			}

			return ret;
		}
	}

	static real_t vec2_cross(const Point2 &O, const Point2 &A, const Point2 &B) {
		return (real_t)(A.x - O.x) * (B.y - O.y) - (real_t)(A.y - O.y) * (B.x - O.x);
	}

	// Returns a list of points on the convex hull in counter-clockwise order.
	// Note: the last point in the returned list is the same as the first one.
	static Vector<Point2> convex_hull_2d(Vector<Point2> P) {
		int n = P.size(), k = 0;
		Vector<Point2> H;
		H.resize(2 * n);

		// Sort points lexicographically.
		P.sort();

		// Build lower hull.
		for (int i = 0; i < n; ++i) {
			while (k >= 2 && vec2_cross(H[k - 2], H[k - 1], P[i]) <= 0) {
				k--;
			}
			H.write[k++] = P[i];
		}

		// Build upper hull.
		for (int i = n - 2, t = k + 1; i >= 0; i--) {
			while (k >= t && vec2_cross(H[k - 2], H[k - 1], P[i]) <= 0) {
				k--;
			}
			H.write[k++] = P[i];
		}

		H.resize(k);
		return H;
	}
	static Vector<Vector<Vector2>> decompose_polygon_in_convex(Vector<Point2> polygon);

	static MeshData build_convex_mesh(const PoolVector<Plane> &p_planes);
	static PoolVector<Plane> build_sphere_planes(real_t p_radius, int p_lats, int p_lons, Vector3::Axis p_axis = Vector3::AXIS_Z);
	static PoolVector<Plane> build_box_planes(const Vector3 &p_extents);
	static PoolVector<Plane> build_cylinder_planes(real_t p_radius, real_t p_height, int p_sides, Vector3::Axis p_axis = Vector3::AXIS_Z);
	static PoolVector<Plane> build_capsule_planes(real_t p_radius, real_t p_height, int p_sides, int p_lats, Vector3::Axis p_axis = Vector3::AXIS_Z);
	static void sort_polygon_winding(Vector<Vector2> &r_verts, bool p_clockwise = true);
	static real_t find_polygon_area(const Vector3 *p_verts, int p_num_verts);

	static void make_atlas(const Vector<Size2i> &p_rects, Vector<Point2i> &r_result, Size2i &r_size);

	struct PackRectsResult {
		int x;
		int y;
		bool packed;
	};
	static Vector<PackRectsResult> partial_pack_rects(const Vector<Vector2i> &p_sizes, const Size2i &p_atlas_size);

	static Vector<Vector3> compute_convex_mesh_points(const Plane *p_planes, int p_plane_count, real_t p_epsilon = CMP_EPSILON);
	static bool convex_hull_intersects_convex_hull(const Plane *p_planes_a, int p_plane_count_a, const Plane *p_planes_b, int p_plane_count_b);
	static real_t calculate_convex_hull_volume(const Geometry::MeshData &p_md);
	static bool verify_indices(const int *p_indices, int p_num_indices, int p_num_vertices);

private:
	static Vector<Vector<Point2>> _polypaths_do_operation(PolyBooleanOperation p_op, const Vector<Point2> &p_polypath_a, const Vector<Point2> &p_polypath_b, bool is_a_open = false);
	static Vector<Vector<Point2>> _polypath_offset(const Vector<Point2> &p_polypath, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type);
	static Vector<Vector<Point2>> _polypath_minkowski(const Vector<Point2> &p_polypath_a, const Vector<Point2> &p_polypath_b, bool p_is_sum, bool p_is_closed);
};

#endif // GEOMETRY_H
