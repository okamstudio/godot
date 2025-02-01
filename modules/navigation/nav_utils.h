/**************************************************************************/
/*  nav_utils.h                                                           */
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
#include "core/templates/hash_map.h"
#include "core/templates/hashfuncs.h"
#include "core/templates/local_vector.h"
#include "servers/navigation/navigation_utilities.h"

struct NavBaseIteration;

namespace gd {
struct Polygon;

union PointKey {
	struct {
		int64_t x : 21;
		int64_t y : 22;
		int64_t z : 21;
	};

	uint64_t key = 0;
};

struct EdgeKey {
	PointKey a;
	PointKey b;

	static uint32_t hash(const EdgeKey &p_val) {
		return hash_one_uint64(p_val.a.key) ^ hash_one_uint64(p_val.b.key);
	}

	bool operator==(const EdgeKey &p_key) const {
		return (a.key == p_key.a.key) && (b.key == p_key.b.key);
	}

	EdgeKey(const PointKey &p_a = PointKey(), const PointKey &p_b = PointKey()) :
			a(p_a),
			b(p_b) {
		if (a.key > b.key) {
			SWAP(a, b);
		}
	}
};

struct Point {
	Vector3 pos;
	PointKey key;
};

struct Edge {
	/// The gateway in the edge, as, in some case, the whole edge might not be navigable.
	struct Connection {
		/// Polygon that this connection leads to.
		Polygon *polygon = nullptr;

		/// Edge of the source polygon where this connection starts from.
		int edge = -1;

		/// Point on the edge where the gateway leading to the poly starts.
		Vector3 pathway_start;

		/// Point on the edge where the gateway leading to the poly ends.
		Vector3 pathway_end;
	};

	/// Connections from this edge to other polygons.
	LocalVector<Connection> connections;
};

struct Polygon {
	/// Id of the polygon in the map.
	uint32_t id = UINT32_MAX;

	/// Navigation region or link that contains this polygon.
	const NavBaseIteration *owner = nullptr;

	/// The points of this `Polygon`
	LocalVector<Point> points;

	/// The edges of this `Polygon`
	LocalVector<Edge> edges;

	real_t surface_area = 0.0;
};

struct NavigationPoly {
	/// This poly.
	const Polygon *poly = nullptr;

	/// Index in the heap of traversable polygons.
	uint32_t traversable_poly_index = UINT32_MAX;

	/// Those 4 variables are used to travel the path backwards.
	int back_navigation_poly_id = -1;
	int back_navigation_edge = -1;
	Vector3 back_navigation_edge_pathway_start;
	Vector3 back_navigation_edge_pathway_end;

	/// The entry position of this poly.
	Vector3 entry;
	/// The distance traveled until now (g cost).
	real_t traveled_distance = 0.0;
	/// The distance to the destination (h cost).
	real_t distance_to_destination = 0.0;

	/// The total travel cost (f cost).
	real_t total_travel_cost() const {
		return traveled_distance + distance_to_destination;
	}

	bool operator==(const NavigationPoly &p_other) const {
		return poly == p_other.poly;
	}

	bool operator!=(const NavigationPoly &p_other) const {
		return !(*this == p_other);
	}

	void reset() {
		poly = nullptr;
		traversable_poly_index = UINT32_MAX;
		back_navigation_poly_id = -1;
		back_navigation_edge = -1;
		traveled_distance = FLT_MAX;
		distance_to_destination = 0.0;
	}
};

struct NavPolyTravelCostGreaterThan {
	// Returns `true` if the travel cost of `a` is higher than that of `b`.
	bool operator()(const NavigationPoly *p_poly_a, const NavigationPoly *p_poly_b) const {
		real_t f_cost_a = p_poly_a->total_travel_cost();
		real_t h_cost_a = p_poly_a->distance_to_destination;
		real_t f_cost_b = p_poly_b->total_travel_cost();
		real_t h_cost_b = p_poly_b->distance_to_destination;

		if (f_cost_a != f_cost_b) {
			return f_cost_a > f_cost_b;
		} else {
			return h_cost_a > h_cost_b;
		}
	}
};

struct NavPolyHeapIndexer {
	void operator()(NavigationPoly *p_poly, uint32_t p_heap_index) const {
		p_poly->traversable_poly_index = p_heap_index;
	}
};

struct ClosestPointQueryResult {
	Vector3 point;
	Vector3 normal;
	RID owner;
};

template <typename T>
struct NoopIndexer {
	void operator()(const T &p_value, uint32_t p_index) {}
};

/**
 * A max-heap implementation that notifies of element index changes.
 */
template <typename T, typename LessThan = Comparator<T>, typename Indexer = NoopIndexer<T>>
class Heap {
	LocalVector<T> _buffer;

	LessThan _less_than;
	Indexer _indexer;

public:
	static constexpr uint32_t INVALID_INDEX = UINT32_MAX;
	void reserve(uint32_t p_size) {
		_buffer.reserve(p_size);
	}

	uint32_t size() const {
		return _buffer.size();
	}

	bool is_empty() const {
		return _buffer.is_empty();
	}

	void push(const T &p_element) {
		_buffer.push_back(p_element);
		_indexer(p_element, _buffer.size() - 1);
		_shift_up(_buffer.size() - 1);
	}

	T pop() {
		ERR_FAIL_COND_V_MSG(_buffer.is_empty(), T(), "Can't pop an empty heap.");
		T value = _buffer[0];
		_indexer(value, INVALID_INDEX);
		if (_buffer.size() > 1) {
			_buffer[0] = _buffer[_buffer.size() - 1];
			_indexer(_buffer[0], 0);
			_buffer.remove_at(_buffer.size() - 1);
			_shift_down(0);
		} else {
			_buffer.remove_at(_buffer.size() - 1);
		}
		return value;
	}

	/**
	 * Update the position of the element in the heap if necessary.
	 */
	void shift(uint32_t p_index) {
		ERR_FAIL_UNSIGNED_INDEX_MSG(p_index, _buffer.size(), "Heap element index is out of range.");
		if (!_shift_up(p_index)) {
			_shift_down(p_index);
		}
	}

	void clear() {
		for (const T &value : _buffer) {
			_indexer(value, INVALID_INDEX);
		}
		_buffer.clear();
	}

	Heap() {}

	Heap(const LessThan &p_less_than) :
			_less_than(p_less_than) {}

	Heap(const Indexer &p_indexer) :
			_indexer(p_indexer) {}

	Heap(const LessThan &p_less_than, const Indexer &p_indexer) :
			_less_than(p_less_than), _indexer(p_indexer) {}

private:
	bool _shift_up(uint32_t p_index) {
		T value = _buffer[p_index];
		uint32_t current_index = p_index;
		uint32_t parent_index = (current_index - 1) / 2;
		while (current_index > 0 && _less_than(_buffer[parent_index], value)) {
			_buffer[current_index] = _buffer[parent_index];
			_indexer(_buffer[current_index], current_index);
			current_index = parent_index;
			parent_index = (current_index - 1) / 2;
		}
		if (current_index != p_index) {
			_buffer[current_index] = value;
			_indexer(value, current_index);
			return true;
		} else {
			return false;
		}
	}

	bool _shift_down(uint32_t p_index) {
		T value = _buffer[p_index];
		uint32_t current_index = p_index;
		uint32_t child_index = 2 * current_index + 1;
		while (child_index < _buffer.size()) {
			if (child_index + 1 < _buffer.size() &&
					_less_than(_buffer[child_index], _buffer[child_index + 1])) {
				child_index++;
			}
			if (_less_than(_buffer[child_index], value)) {
				break;
			}
			_buffer[current_index] = _buffer[child_index];
			_indexer(_buffer[current_index], current_index);
			current_index = child_index;
			child_index = 2 * current_index + 1;
		}
		if (current_index != p_index) {
			_buffer[current_index] = value;
			_indexer(value, current_index);
			return true;
		} else {
			return false;
		}
	}
};

struct EdgeConnectionPair {
	gd::Edge::Connection connections[2];
	int size = 0;
};

struct PerformanceData {
	int pm_region_count = 0;
	int pm_agent_count = 0;
	int pm_link_count = 0;
	int pm_polygon_count = 0;
	int pm_edge_count = 0;
	int pm_edge_merge_count = 0;
	int pm_edge_connection_count = 0;
	int pm_edge_free_count = 0;
	int pm_obstacle_count = 0;

	void reset() {
		pm_region_count = 0;
		pm_agent_count = 0;
		pm_link_count = 0;
		pm_polygon_count = 0;
		pm_edge_count = 0;
		pm_edge_merge_count = 0;
		pm_edge_connection_count = 0;
		pm_edge_free_count = 0;
		pm_obstacle_count = 0;
	}
};

} // namespace gd
