/**************************************************************************/
/*  a_star.h                                                              */
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

#include "core/object/gdvirtual.gen.inc"
#include "core/object/ref_counted.h"
#include "core/templates/oa_hash_map.h"

/**
	A* pathfinding algorithm.
*/

class AStar3D : public RefCounted {
	GDCLASS(AStar3D, RefCounted);
	friend class AStar2D;

	struct Point {
		Point() {}

		int64_t id = 0;
		Vector3 pos;
		real_t weight_scale = 0;
		bool enabled = false;

		OAHashMap<int64_t, Point *> neighbors = 4u;
		OAHashMap<int64_t, Point *> unlinked_neighbours = 4u;

		// Used for pathfinding.
		Point *prev_point = nullptr;
		real_t g_score = 0;
		real_t f_score = 0;
		uint64_t open_pass = 0;
		uint64_t closed_pass = 0;

		// Used for getting closest_point_of_last_pathing_call.
		real_t abs_g_score = 0;
		real_t abs_f_score = 0;
	};

	struct SortPoints {
		_FORCE_INLINE_ bool operator()(const Point *A, const Point *B) const { // Returns true when the Point A is worse than Point B.
			if (A->f_score > B->f_score) {
				return true;
			} else if (A->f_score < B->f_score) {
				return false;
			} else {
				return A->g_score < B->g_score; // If the f_costs are the same then prioritize the points that are further away from the start.
			}
		}
	};

	struct Segment {
		Pair<int64_t, int64_t> key;

		enum {
			NONE = 0,
			FORWARD = 1,
			BACKWARD = 2,
			BIDIRECTIONAL = FORWARD | BACKWARD
		};
		unsigned char direction = NONE;

		static uint32_t hash(const Segment &p_seg) {
			return PairHash<int64_t, int64_t>().hash(p_seg.key);
		}
		bool operator==(const Segment &p_s) const { return key == p_s.key; }

		Segment() {}
		Segment(int64_t p_from, int64_t p_to) {
			if (p_from < p_to) {
				key.first = p_from;
				key.second = p_to;
				direction = FORWARD;
			} else {
				key.first = p_to;
				key.second = p_from;
				direction = BACKWARD;
			}
		}
	};

	mutable int64_t last_free_id = 0;
	uint64_t pass = 1;

	OAHashMap<int64_t, Point *> points;
	HashSet<Segment, Segment> segments;
	Point *last_closest_point = nullptr;

	bool _solve(Point *begin_point, Point *end_point, bool p_allow_partial_path);

protected:
	static void _bind_methods();

	virtual real_t _estimate_cost(int64_t p_from_id, int64_t p_end_id);
	virtual real_t _compute_cost(int64_t p_from_id, int64_t p_to_id);

	GDVIRTUAL2RC(real_t, _estimate_cost, int64_t, int64_t)
	GDVIRTUAL2RC(real_t, _compute_cost, int64_t, int64_t)

#ifndef DISABLE_DEPRECATED
	Vector<int64_t> _get_id_path_bind_compat_88047(int64_t p_from_id, int64_t p_to_id);
	Vector<Vector3> _get_point_path_bind_compat_88047(int64_t p_from_id, int64_t p_to_id);
	static void _bind_compatibility_methods();
#endif

public:
	int64_t get_available_point_id() const;

	void add_point(int64_t p_id, const Vector3 &p_pos, real_t p_weight_scale = 1);
	Vector3 get_point_position(int64_t p_id) const;
	void set_point_position(int64_t p_id, const Vector3 &p_pos);
	real_t get_point_weight_scale(int64_t p_id) const;
	void set_point_weight_scale(int64_t p_id, real_t p_weight_scale);
	void remove_point(int64_t p_id);
	bool has_point(int64_t p_id) const;
	Vector<int64_t> get_point_connections(int64_t p_id);
	PackedInt64Array get_point_ids();

	void set_point_disabled(int64_t p_id, bool p_disabled = true);
	bool is_point_disabled(int64_t p_id) const;

	void connect_points(int64_t p_id, int64_t p_with_id, bool bidirectional = true);
	void disconnect_points(int64_t p_id, int64_t p_with_id, bool bidirectional = true);
	bool are_points_connected(int64_t p_id, int64_t p_with_id, bool bidirectional = true) const;

	int64_t get_point_count() const;
	int64_t get_point_capacity() const;
	void reserve_space(int64_t p_num_nodes);
	void clear();

	int64_t get_closest_point(const Vector3 &p_point, bool p_include_disabled = false) const;
	Vector3 get_closest_position_in_segment(const Vector3 &p_point) const;

	Vector<Vector3> get_point_path(int64_t p_from_id, int64_t p_to_id, bool p_allow_partial_path = false);
	Vector<int64_t> get_id_path(int64_t p_from_id, int64_t p_to_id, bool p_allow_partial_path = false);

	AStar3D() {}
	~AStar3D();
};

class AStar2D : public RefCounted {
	GDCLASS(AStar2D, RefCounted);
	AStar3D astar;

	bool _solve(AStar3D::Point *begin_point, AStar3D::Point *end_point, bool p_allow_partial_path);

protected:
	static void _bind_methods();

	virtual real_t _estimate_cost(int64_t p_from_id, int64_t p_end_id);
	virtual real_t _compute_cost(int64_t p_from_id, int64_t p_to_id);

	GDVIRTUAL2RC(real_t, _estimate_cost, int64_t, int64_t)
	GDVIRTUAL2RC(real_t, _compute_cost, int64_t, int64_t)

#ifndef DISABLE_DEPRECATED
	Vector<int64_t> _get_id_path_bind_compat_88047(int64_t p_from_id, int64_t p_to_id);
	Vector<Vector2> _get_point_path_bind_compat_88047(int64_t p_from_id, int64_t p_to_id);
	static void _bind_compatibility_methods();
#endif

public:
	int64_t get_available_point_id() const;

	void add_point(int64_t p_id, const Vector2 &p_pos, real_t p_weight_scale = 1);
	Vector2 get_point_position(int64_t p_id) const;
	void set_point_position(int64_t p_id, const Vector2 &p_pos);
	real_t get_point_weight_scale(int64_t p_id) const;
	void set_point_weight_scale(int64_t p_id, real_t p_weight_scale);
	void remove_point(int64_t p_id);
	bool has_point(int64_t p_id) const;
	Vector<int64_t> get_point_connections(int64_t p_id);
	PackedInt64Array get_point_ids();

	void set_point_disabled(int64_t p_id, bool p_disabled = true);
	bool is_point_disabled(int64_t p_id) const;

	void connect_points(int64_t p_id, int64_t p_with_id, bool p_bidirectional = true);
	void disconnect_points(int64_t p_id, int64_t p_with_id, bool p_bidirectional = true);
	bool are_points_connected(int64_t p_id, int64_t p_with_id, bool p_bidirectional = true) const;

	int64_t get_point_count() const;
	int64_t get_point_capacity() const;
	void reserve_space(int64_t p_num_nodes);
	void clear();

	int64_t get_closest_point(const Vector2 &p_point, bool p_include_disabled = false) const;
	Vector2 get_closest_position_in_segment(const Vector2 &p_point) const;

	Vector<Vector2> get_point_path(int64_t p_from_id, int64_t p_to_id, bool p_allow_partial_path = false);
	Vector<int64_t> get_id_path(int64_t p_from_id, int64_t p_to_id, bool p_allow_partial_path = false);

	AStar2D() {}
	~AStar2D() {}
};
