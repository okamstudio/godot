/**************************************************************************/
/*  bvh_abb.h                                                             */
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

// special optimized version of axis aligned bounding box
template <typename BOUNDS = AABB, typename POINT = Vector3>
struct BVH_ABB {
	struct ConvexHull {
		// convex hulls (optional)
		const Plane *planes;
		int num_planes;
		const Vector3 *points;
		int num_points;
	};

	struct Segment {
		POINT from;
		POINT to;
	};

	enum IntersectResult {
		IR_MISS = 0,
		IR_PARTIAL,
		IR_FULL,
	};

	// we store mins with a negative value in order to test them with SIMD
	POINT min;
	POINT neg_max;

	bool operator==(const BVH_ABB &o) const { return (min == o.min) && (neg_max == o.neg_max); }
	bool operator!=(const BVH_ABB &o) const { return (*this == o) == false; }

	void set(const POINT &_min, const POINT &_max) {
		min = _min;
		neg_max = -_max;
	}

	// to and from standard AABB
	void from(const BOUNDS &p_aabb) {
		min = p_aabb.position;
		neg_max = -(p_aabb.position + p_aabb.size);
	}

	void to(BOUNDS &r_aabb) const {
		r_aabb.position = min;
		r_aabb.size = calculate_size();
	}

	void merge(const BVH_ABB &p_o) {
		for (int axis = 0; axis < POINT::AXIS_COUNT; ++axis) {
			neg_max[axis] = MIN(neg_max[axis], p_o.neg_max[axis]);
			min[axis] = MIN(min[axis], p_o.min[axis]);
		}
	}

	POINT calculate_size() const {
		return -neg_max - min;
	}

	POINT calculate_center() const {
		return POINT((calculate_size() * 0.5) + min);
	}

	real_t get_proximity_to(const BVH_ABB &p_b) const {
		const POINT d = (min - neg_max) - (p_b.min - p_b.neg_max);
		real_t proximity = 0.0;
		for (int axis = 0; axis < POINT::AXIS_COUNT; ++axis) {
			proximity += Math::abs(d[axis]);
		}
		return proximity;
	}

	int select_by_proximity(const BVH_ABB &p_a, const BVH_ABB &p_b) const {
		return (get_proximity_to(p_a) < get_proximity_to(p_b) ? 0 : 1);
	}

	uint32_t find_cutting_planes(const typename BVH_ABB::ConvexHull &p_hull, uint32_t *p_plane_ids) const {
		uint32_t count = 0;

		for (int n = 0; n < p_hull.num_planes; n++) {
			const Plane &p = p_hull.planes[n];
			if (intersects_plane(p)) {
				p_plane_ids[count++] = n;
			}
		}

		return count;
	}

	bool intersects_plane(const Plane &p_p) const {
		Vector3 size = calculate_size();
		Vector3 half_extents = size * 0.5;
		Vector3 ofs = min + half_extents;

		// forward side of plane?
		Vector3 point_offset(
				(p_p.normal.x < 0) ? -half_extents.x : half_extents.x,
				(p_p.normal.y < 0) ? -half_extents.y : half_extents.y,
				(p_p.normal.z < 0) ? -half_extents.z : half_extents.z);
		Vector3 point = point_offset + ofs;

		if (!p_p.is_point_over(point)) {
			return false;
		}

		point = -point_offset + ofs;
		if (p_p.is_point_over(point)) {
			return false;
		}

		return true;
	}

	bool intersects_convex_optimized(const ConvexHull &p_hull, const uint32_t *p_plane_ids, uint32_t p_num_planes) const {
		Vector3 size = calculate_size();
		Vector3 half_extents = size * 0.5;
		Vector3 ofs = min + half_extents;

		for (unsigned int i = 0; i < p_num_planes; i++) {
			const Plane &p = p_hull.planes[p_plane_ids[i]];
			Vector3 point(
					(p.normal.x > 0) ? -half_extents.x : half_extents.x,
					(p.normal.y > 0) ? -half_extents.y : half_extents.y,
					(p.normal.z > 0) ? -half_extents.z : half_extents.z);
			point += ofs;
			if (p.is_point_over(point)) {
				return false;
			}
		}

		return true;
	}

	bool intersects_convex_partial(const ConvexHull &p_hull) const {
		BOUNDS bb;
		to(bb);
		return bb.intersects_convex_shape(p_hull.planes, p_hull.num_planes, p_hull.points, p_hull.num_points);
	}

	IntersectResult intersects_convex(const ConvexHull &p_hull) const {
		if (intersects_convex_partial(p_hull)) {
			// fully within? very important for tree checks
			if (is_within_convex(p_hull)) {
				return IR_FULL;
			}

			return IR_PARTIAL;
		}

		return IR_MISS;
	}

	bool is_within_convex(const ConvexHull &p_hull) const {
		// use half extents routine
		BOUNDS bb;
		to(bb);
		return bb.inside_convex_shape(p_hull.planes, p_hull.num_planes);
	}

	bool is_point_within_hull(const ConvexHull &p_hull, const Vector3 &p_pt) const {
		for (int n = 0; n < p_hull.num_planes; n++) {
			if (p_hull.planes[n].distance_to(p_pt) > 0.0f) {
				return false;
			}
		}
		return true;
	}

	bool intersects_segment(const Segment &p_s) const {
		BOUNDS bb;
		to(bb);
		return bb.intersects_segment(p_s.from, p_s.to);
	}

	bool intersects_point(const POINT &p_pt) const {
		if (_any_lessthan(-p_pt, neg_max)) {
			return false;
		}
		if (_any_lessthan(p_pt, min)) {
			return false;
		}
		return true;
	}

	// Very hot in profiling, make sure optimized
	bool intersects(const BVH_ABB &p_o) const {
		if (_any_morethan(p_o.min, -neg_max)) {
			return false;
		}
		if (_any_morethan(min, -p_o.neg_max)) {
			return false;
		}
		return true;
	}

	// for pre-swizzled tester (this object)
	bool intersects_swizzled(const BVH_ABB &p_o) const {
		if (_any_lessthan(min, p_o.min)) {
			return false;
		}
		if (_any_lessthan(neg_max, p_o.neg_max)) {
			return false;
		}
		return true;
	}

	bool is_other_within(const BVH_ABB &p_o) const {
		if (_any_lessthan(p_o.neg_max, neg_max)) {
			return false;
		}
		if (_any_lessthan(p_o.min, min)) {
			return false;
		}
		return true;
	}

	void grow(const POINT &p_change) {
		neg_max -= p_change;
		min -= p_change;
	}

	void expand(real_t p_change) {
		POINT change;
		for (int axis = 0; axis < POINT::AXIS_COUNT; ++axis) {
			change[axis] = p_change;
		}
		grow(change);
	}

	// Actually surface area metric.
	real_t get_area() const {
		POINT d = calculate_size();
		return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
	}

	void set_to_max_opposite_extents() {
		for (int axis = 0; axis < POINT::AXIS_COUNT; ++axis) {
			neg_max[axis] = FLT_MAX;
		}
		min = neg_max;
	}

	bool _any_morethan(const POINT &p_a, const POINT &p_b) const {
		for (int axis = 0; axis < POINT::AXIS_COUNT; ++axis) {
			if (p_a[axis] > p_b[axis]) {
				return true;
			}
		}
		return false;
	}

	bool _any_lessthan(const POINT &p_a, const POINT &p_b) const {
		for (int axis = 0; axis < POINT::AXIS_COUNT; ++axis) {
			if (p_a[axis] < p_b[axis]) {
				return true;
			}
		}
		return false;
	}
};
