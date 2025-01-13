/**************************************************************************/
/*  nav_mesh_queries_3d.cpp                                               */
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

#ifndef _3D_DISABLED

#include "nav_mesh_queries_3d.h"

#include "../nav_base.h"
#include "../nav_map.h"

#include "core/math/geometry_3d.h"
#include "servers/navigation/navigation_utilities.h"

#define THREE_POINTS_CROSS_PRODUCT(m_a, m_b, m_c) (((m_c) - (m_a)).cross((m_b) - (m_a)))

bool NavMeshQueries3D::emit_callback(const Callable &p_callback) {
	ERR_FAIL_COND_V(!p_callback.is_valid(), false);

	Callable::CallError ce;
	Variant result;
	p_callback.callp(nullptr, 0, result, ce);

	return ce.error == Callable::CallError::CALL_OK;
}

Vector3 NavMeshQueries3D::polygons_get_random_point(const LocalVector<gd::Polygon> &p_polygons, uint32_t p_navigation_layers, bool p_uniformly) {
	const LocalVector<gd::Polygon> &region_polygons = p_polygons;

	if (region_polygons.is_empty()) {
		return Vector3();
	}

	if (p_uniformly) {
		real_t accumulated_area = 0;
		RBMap<real_t, uint32_t> region_area_map;

		for (uint32_t rp_index = 0; rp_index < region_polygons.size(); rp_index++) {
			const gd::Polygon &region_polygon = region_polygons[rp_index];
			real_t polyon_area = region_polygon.surface_area;

			if (polyon_area == 0.0) {
				continue;
			}
			region_area_map[accumulated_area] = rp_index;
			accumulated_area += polyon_area;
		}
		if (region_area_map.is_empty() || accumulated_area == 0) {
			// All polygons have no real surface / no area.
			return Vector3();
		}

		real_t region_area_map_pos = Math::random(real_t(0), accumulated_area);

		RBMap<real_t, uint32_t>::Iterator region_E = region_area_map.find_closest(region_area_map_pos);
		ERR_FAIL_COND_V(!region_E, Vector3());
		uint32_t rrp_polygon_index = region_E->value;
		ERR_FAIL_UNSIGNED_INDEX_V(rrp_polygon_index, region_polygons.size(), Vector3());

		const gd::Polygon &rr_polygon = region_polygons[rrp_polygon_index];

		real_t accumulated_polygon_area = 0;
		RBMap<real_t, uint32_t> polygon_area_map;

		for (uint32_t rpp_index = 2; rpp_index < rr_polygon.points.size(); rpp_index++) {
			real_t face_area = Face3(rr_polygon.points[0].pos, rr_polygon.points[rpp_index - 1].pos, rr_polygon.points[rpp_index].pos).get_area();

			if (face_area == 0.0) {
				continue;
			}
			polygon_area_map[accumulated_polygon_area] = rpp_index;
			accumulated_polygon_area += face_area;
		}
		if (polygon_area_map.is_empty() || accumulated_polygon_area == 0) {
			// All faces have no real surface / no area.
			return Vector3();
		}

		real_t polygon_area_map_pos = Math::random(real_t(0), accumulated_polygon_area);

		RBMap<real_t, uint32_t>::Iterator polygon_E = polygon_area_map.find_closest(polygon_area_map_pos);
		ERR_FAIL_COND_V(!polygon_E, Vector3());
		uint32_t rrp_face_index = polygon_E->value;
		ERR_FAIL_UNSIGNED_INDEX_V(rrp_face_index, rr_polygon.points.size(), Vector3());

		const Face3 face(rr_polygon.points[0].pos, rr_polygon.points[rrp_face_index - 1].pos, rr_polygon.points[rrp_face_index].pos);

		Vector3 face_random_position = face.get_random_point_inside();
		return face_random_position;

	} else {
		uint32_t rrp_polygon_index = Math::random(int(0), region_polygons.size() - 1);

		const gd::Polygon &rr_polygon = region_polygons[rrp_polygon_index];

		uint32_t rrp_face_index = Math::random(int(2), rr_polygon.points.size() - 1);

		const Face3 face(rr_polygon.points[0].pos, rr_polygon.points[rrp_face_index - 1].pos, rr_polygon.points[rrp_face_index].pos);

		Vector3 face_random_position = face.get_random_point_inside();
		return face_random_position;
	}
}

void NavMeshQueries3D::_query_task_create_same_polygon_two_point_path(NavMeshPathQueryTask3D &p_query_task, const gd::Polygon *begin_poly, Vector3 begin_point, const gd::Polygon *end_poly, Vector3 end_point) {
	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_TYPES)) {
		p_query_task.path_meta_point_types.resize(2);
		p_query_task.path_meta_point_types[0] = begin_poly->owner->get_type();
		p_query_task.path_meta_point_types[1] = end_poly->owner->get_type();
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_RIDS)) {
		p_query_task.path_meta_point_rids.resize(2);
		p_query_task.path_meta_point_rids[0] = begin_poly->owner->get_self();
		p_query_task.path_meta_point_rids[1] = end_poly->owner->get_self();
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_OWNERS)) {
		p_query_task.path_meta_point_owners.resize(2);
		p_query_task.path_meta_point_owners[0] = begin_poly->owner->get_owner_id();
		p_query_task.path_meta_point_owners[1] = end_poly->owner->get_owner_id();
	}

	p_query_task.path_points.resize(2);
	p_query_task.path_points[0] = begin_point;
	p_query_task.path_points[1] = end_point;
}

void NavMeshQueries3D::_query_task_push_back_point_with_metadata(NavMeshPathQueryTask3D &p_query_task, Vector3 p_point, const gd::Polygon *p_point_polygon) {
	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_TYPES)) {
		p_query_task.path_meta_point_types.push_back(p_point_polygon->owner->get_type());
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_RIDS)) {
		p_query_task.path_meta_point_rids.push_back(p_point_polygon->owner->get_self());
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_OWNERS)) {
		p_query_task.path_meta_point_owners.push_back(p_point_polygon->owner->get_owner_id());
	}

	p_query_task.path_points.push_back(p_point);
}

void NavMeshQueries3D::map_query_path(NavMap *map, const Ref<NavigationPathQueryParameters3D> &p_query_parameters, Ref<NavigationPathQueryResult3D> p_query_result, const Callable &p_callback) {
	ERR_FAIL_NULL(map);
	ERR_FAIL_COND(p_query_parameters.is_null());
	ERR_FAIL_COND(p_query_result.is_null());

	using namespace NavigationUtilities;

	NavMeshQueries3D::NavMeshPathQueryTask3D query_task;
	query_task.start_position = p_query_parameters->get_start_position();
	query_task.target_position = p_query_parameters->get_target_position();
	query_task.navigation_layers = p_query_parameters->get_navigation_layers();
	query_task.callback = p_callback;

	switch (p_query_parameters->get_pathfinding_algorithm()) {
		case NavigationPathQueryParameters3D::PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR: {
			query_task.pathfinding_algorithm = PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR;
		} break;
		default: {
			WARN_PRINT("No match for used PathfindingAlgorithm - fallback to default");
			query_task.pathfinding_algorithm = PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR;
		} break;
	}

	switch (p_query_parameters->get_path_postprocessing()) {
		case NavigationPathQueryParameters3D::PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL: {
			query_task.path_postprocessing = PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL;
		} break;
		case NavigationPathQueryParameters3D::PathPostProcessing::PATH_POSTPROCESSING_EDGECENTERED: {
			query_task.path_postprocessing = PathPostProcessing::PATH_POSTPROCESSING_EDGECENTERED;
		} break;
		case NavigationPathQueryParameters3D::PathPostProcessing::PATH_POSTPROCESSING_NONE: {
			query_task.path_postprocessing = PathPostProcessing::PATH_POSTPROCESSING_NONE;
		} break;
		default: {
			WARN_PRINT("No match for used PathPostProcessing - fallback to default");
			query_task.path_postprocessing = PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL;
		} break;
	}

	query_task.metadata_flags = (int64_t)p_query_parameters->get_metadata_flags();
	query_task.simplify_path = p_query_parameters->get_simplify_path();
	query_task.simplify_epsilon = p_query_parameters->get_simplify_epsilon();
	query_task.status = NavMeshPathQueryTask3D::TaskStatus::QUERY_STARTED;

	map->query_path(query_task);

	const uint32_t path_point_size = query_task.path_points.size();

	Vector<Vector3> path_points;
	Vector<int32_t> path_meta_point_types;
	TypedArray<RID> path_meta_point_rids;
	Vector<int64_t> path_meta_point_owners;

	{
		path_points.resize(path_point_size);
		Vector3 *w = path_points.ptrw();
		const Vector3 *r = query_task.path_points.ptr();
		for (uint32_t i = 0; i < path_point_size; i++) {
			w[i] = r[i];
		}
	}

	if (query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_TYPES)) {
		path_meta_point_types.resize(path_point_size);
		int32_t *w = path_meta_point_types.ptrw();
		const int32_t *r = query_task.path_meta_point_types.ptr();
		for (uint32_t i = 0; i < path_point_size; i++) {
			w[i] = r[i];
		}
	}
	if (query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_RIDS)) {
		path_meta_point_rids.resize(path_point_size);
		for (uint32_t i = 0; i < path_point_size; i++) {
			path_meta_point_rids[i] = query_task.path_meta_point_rids[i];
		}
	}
	if (query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_OWNERS)) {
		path_meta_point_owners.resize(path_point_size);
		int64_t *w = path_meta_point_owners.ptrw();
		const int64_t *r = query_task.path_meta_point_owners.ptr();
		for (uint32_t i = 0; i < path_point_size; i++) {
			w[i] = r[i];
		}
	}

	p_query_result->set_path(path_points);
	p_query_result->set_path_types(path_meta_point_types);
	p_query_result->set_path_rids(path_meta_point_rids);
	p_query_result->set_path_owner_ids(path_meta_point_owners);

	if (query_task.callback.is_valid()) {
		if (emit_callback(query_task.callback)) {
			query_task.status = NavMeshPathQueryTask3D::TaskStatus::CALLBACK_DISPATCHED;
		} else {
			query_task.status = NavMeshPathQueryTask3D::TaskStatus::CALLBACK_FAILED;
		}
	}
}

void NavMeshQueries3D::query_task_polygons_get_path(NavMeshPathQueryTask3D &p_query_task, const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_map_up, uint32_t p_link_polygons_size) {
	p_query_task.path_points.clear();
	p_query_task.path_meta_point_types.clear();
	p_query_task.path_meta_point_rids.clear();
	p_query_task.path_meta_point_owners.clear();

	// Find begin polyon and begin position closest to start position and
	// end polyon and end position closest to target position on the map.
	const gd::Polygon *begin_poly = nullptr;
	const gd::Polygon *end_poly = nullptr;
	Vector3 begin_point;
	Vector3 end_point;

	_query_task_find_start_end_positions(p_query_task, p_polygons, &begin_poly, begin_point, &end_poly, end_point);

	// Check for trivial cases
	if (!begin_poly || !end_poly) {
		p_query_task.status = NavMeshPathQueryTask3D::TaskStatus::QUERY_FAILED;
		return;
	}

	if (begin_poly == end_poly) {
		_query_task_create_same_polygon_two_point_path(p_query_task, begin_poly, begin_point, end_poly, end_point);
		return;
	}

	_query_task_build_path_corridor(p_query_task, p_polygons, p_map_up, p_link_polygons_size, begin_poly, begin_point, end_poly, end_point);

	// Post-Process path.
	switch (p_query_task.path_postprocessing) {
		case PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL: {
			_path_corridor_post_process_corridorfunnel(p_query_task, p_query_task.least_cost_id, begin_poly, begin_point, end_poly, end_point, p_map_up);
		} break;
		case PathPostProcessing::PATH_POSTPROCESSING_EDGECENTERED: {
			_path_corridor_post_process_edgecentered(p_query_task, p_query_task.least_cost_id, begin_poly, begin_point, end_poly, end_point);
		} break;
		case PathPostProcessing::PATH_POSTPROCESSING_NONE: {
			_path_corridor_post_process_nopostprocessing(p_query_task, p_query_task.least_cost_id, begin_poly, begin_point, end_poly, end_point);
		} break;
		default: {
			WARN_PRINT("No match for used PathPostProcessing - fallback to default");
			_path_corridor_post_process_corridorfunnel(p_query_task, p_query_task.least_cost_id, begin_poly, begin_point, end_poly, end_point, p_map_up);
		} break;
	}

	p_query_task.path_points.invert();
	p_query_task.path_meta_point_types.invert();
	p_query_task.path_meta_point_rids.invert();
	p_query_task.path_meta_point_owners.invert();

	if (p_query_task.simplify_path) {
		_query_task_simplified_path_points(p_query_task);
	}

#ifdef DEBUG_ENABLED
	// Ensure post conditions as path meta arrays if used MUST match in array size with the path points.
	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_TYPES)) {
		DEV_ASSERT(p_query_task.path_points.size() == p_query_task.path_meta_point_types.size());
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_RIDS)) {
		DEV_ASSERT(p_query_task.path_points.size() == p_query_task.path_meta_point_rids.size());
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_OWNERS)) {
		DEV_ASSERT(p_query_task.path_points.size() == p_query_task.path_meta_point_owners.size());
	}
#endif // DEBUG_ENABLED

	p_query_task.status = NavMeshPathQueryTask3D::TaskStatus::QUERY_FINISHED;
}

void NavMeshQueries3D::_query_task_build_path_corridor(NavMeshPathQueryTask3D &p_query_task, const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_map_up, uint32_t p_link_polygons_size, const gd::Polygon *begin_poly, Vector3 begin_point, const gd::Polygon *end_poly, Vector3 end_point) {
	// List of all reachable navigation polys.
	LocalVector<gd::NavigationPoly> &navigation_polys = p_query_task.path_query_slot->path_corridor;
	for (gd::NavigationPoly &polygon : navigation_polys) {
		polygon.reset();
	}

	DEV_ASSERT(navigation_polys.size() == p_polygons.size() + p_link_polygons_size);

	// Initialize the matching navigation polygon.
	gd::NavigationPoly &begin_navigation_poly = navigation_polys[begin_poly->id];
	begin_navigation_poly.poly = begin_poly;
	begin_navigation_poly.entry = begin_point;
	begin_navigation_poly.back_navigation_edge_pathway_start = begin_point;
	begin_navigation_poly.back_navigation_edge_pathway_end = begin_point;

	// Heap of polygons to travel next.
	gd::Heap<gd::NavigationPoly *, gd::NavPolyTravelCostGreaterThan, gd::NavPolyHeapIndexer>
			&traversable_polys = p_query_task.path_query_slot->traversable_polys;
	traversable_polys.clear();
	traversable_polys.reserve(p_polygons.size() * 0.25);

	// This is an implementation of the A* algorithm.
	p_query_task.least_cost_id = begin_poly->id;
	int prev_least_cost_id = -1;
	bool found_route = false;

	const gd::Polygon *reachable_end = nullptr;
	real_t distance_to_reachable_end = FLT_MAX;
	bool is_reachable = true;

	while (true) {
		// Takes the current least_cost_poly neighbors (iterating over its edges) and compute the traveled_distance.
		for (const gd::Edge &edge : navigation_polys[p_query_task.least_cost_id].poly->edges) {
			// Iterate over connections in this edge, then compute the new optimized travel distance assigned to this polygon.
			for (uint32_t connection_index = 0; connection_index < edge.connections.size(); connection_index++) {
				const gd::Edge::Connection &connection = edge.connections[connection_index];

				// Only consider the connection to another polygon if this polygon is in a region with compatible layers.
				if ((p_query_task.navigation_layers & connection.polygon->owner->get_navigation_layers()) == 0) {
					continue;
				}

				const gd::NavigationPoly &least_cost_poly = navigation_polys[p_query_task.least_cost_id];
				real_t poly_enter_cost = 0.0;
				real_t poly_travel_cost = least_cost_poly.poly->owner->get_travel_cost();

				if (prev_least_cost_id != -1 && navigation_polys[prev_least_cost_id].poly->owner->get_self() != least_cost_poly.poly->owner->get_self()) {
					poly_enter_cost = least_cost_poly.poly->owner->get_enter_cost();
				}
				prev_least_cost_id = p_query_task.least_cost_id;

				Vector3 pathway[2] = { connection.pathway_start, connection.pathway_end };
				const Vector3 new_entry = Geometry3D::get_closest_point_to_segment(least_cost_poly.entry, pathway);
				const real_t new_traveled_distance = least_cost_poly.entry.distance_to(new_entry) * poly_travel_cost + poly_enter_cost + least_cost_poly.traveled_distance;

				// Check if the neighbor polygon has already been processed.
				gd::NavigationPoly &neighbor_poly = navigation_polys[connection.polygon->id];
				if (neighbor_poly.poly != nullptr) {
					// If the neighbor polygon hasn't been traversed yet and the new path leading to
					// it is shorter, update the polygon.
					if (neighbor_poly.traversable_poly_index < traversable_polys.size() &&
							new_traveled_distance < neighbor_poly.traveled_distance) {
						neighbor_poly.back_navigation_poly_id = p_query_task.least_cost_id;
						neighbor_poly.back_navigation_edge = connection.edge;
						neighbor_poly.back_navigation_edge_pathway_start = connection.pathway_start;
						neighbor_poly.back_navigation_edge_pathway_end = connection.pathway_end;
						neighbor_poly.traveled_distance = new_traveled_distance;
						neighbor_poly.distance_to_destination =
								new_entry.distance_to(end_point) *
								neighbor_poly.poly->owner->get_travel_cost();
						neighbor_poly.entry = new_entry;

						// Update the priority of the polygon in the heap.
						traversable_polys.shift(neighbor_poly.traversable_poly_index);
					}
				} else {
					// Initialize the matching navigation polygon.
					neighbor_poly.poly = connection.polygon;
					neighbor_poly.back_navigation_poly_id = p_query_task.least_cost_id;
					neighbor_poly.back_navigation_edge = connection.edge;
					neighbor_poly.back_navigation_edge_pathway_start = connection.pathway_start;
					neighbor_poly.back_navigation_edge_pathway_end = connection.pathway_end;
					neighbor_poly.traveled_distance = new_traveled_distance;
					neighbor_poly.distance_to_destination =
							new_entry.distance_to(end_point) *
							neighbor_poly.poly->owner->get_travel_cost();
					neighbor_poly.entry = new_entry;

					// Add the polygon to the heap of polygons to traverse next.
					traversable_polys.push(&neighbor_poly);
				}
			}
		}

		// When the heap of traversable polygons is empty at this point it means the end polygon is
		// unreachable.
		if (traversable_polys.is_empty()) {
			// Thus use the further reachable polygon
			ERR_BREAK_MSG(is_reachable == false, "It's not expect to not find the most reachable polygons");
			is_reachable = false;
			if (reachable_end == nullptr) {
				// The path is not found and there is not a way out.
				break;
			}

			// Set as end point the furthest reachable point.
			end_poly = reachable_end;
			real_t end_d = FLT_MAX;
			for (size_t point_id = 2; point_id < end_poly->points.size(); point_id++) {
				Face3 f(end_poly->points[0].pos, end_poly->points[point_id - 1].pos, end_poly->points[point_id].pos);
				Vector3 spoint = f.get_closest_point_to(p_query_task.target_position);
				real_t dpoint = spoint.distance_to(p_query_task.target_position);
				if (dpoint < end_d) {
					end_point = spoint;
					end_d = dpoint;
				}
			}

			// Search all faces of start polygon as well.
			bool closest_point_on_start_poly = false;
			for (size_t point_id = 2; point_id < begin_poly->points.size(); point_id++) {
				Face3 f(begin_poly->points[0].pos, begin_poly->points[point_id - 1].pos, begin_poly->points[point_id].pos);
				Vector3 spoint = f.get_closest_point_to(p_query_task.target_position);
				real_t dpoint = spoint.distance_to(p_query_task.target_position);
				if (dpoint < end_d) {
					end_point = spoint;
					end_d = dpoint;
					closest_point_on_start_poly = true;
				}
			}

			if (closest_point_on_start_poly) {
				_query_task_create_same_polygon_two_point_path(p_query_task, begin_poly, begin_point, end_poly, end_point);
				return;
			}

			for (gd::NavigationPoly &nav_poly : navigation_polys) {
				nav_poly.poly = nullptr;
			}
			navigation_polys[begin_poly->id].poly = begin_poly;

			p_query_task.least_cost_id = begin_poly->id;
			prev_least_cost_id = -1;

			reachable_end = nullptr;

			continue;
		}

		// Pop the polygon with the lowest travel cost from the heap of traversable polygons.
		p_query_task.least_cost_id = traversable_polys.pop()->poly->id;

		// Store the farthest reachable end polygon in case our goal is not reachable.
		if (is_reachable) {
			real_t distance = navigation_polys[p_query_task.least_cost_id].entry.distance_to(p_query_task.target_position);
			if (distance_to_reachable_end > distance) {
				distance_to_reachable_end = distance;
				reachable_end = navigation_polys[p_query_task.least_cost_id].poly;
			}
		}

		// Check if we reached the end
		if (navigation_polys[p_query_task.least_cost_id].poly == end_poly) {
			found_route = true;
			break;
		}
	}

	// We did not find a route but we have both a start polygon and an end polygon at this point.
	// Usually this happens because there was not a single external or internal connected edge, e.g. our start polygon is an isolated, single convex polygon.
	if (!found_route) {
		real_t end_d = FLT_MAX;
		// Search all faces of the start polygon for the closest point to our target position.
		for (size_t point_id = 2; point_id < begin_poly->points.size(); point_id++) {
			Face3 f(begin_poly->points[0].pos, begin_poly->points[point_id - 1].pos, begin_poly->points[point_id].pos);
			Vector3 spoint = f.get_closest_point_to(p_query_task.target_position);
			real_t dpoint = spoint.distance_to(p_query_task.target_position);
			if (dpoint < end_d) {
				end_point = spoint;
				end_d = dpoint;
			}
		}
		_query_task_create_same_polygon_two_point_path(p_query_task, begin_poly, begin_point, begin_poly, end_point);
		return;
	}
}

void NavMeshQueries3D::_query_task_simplified_path_points(NavMeshPathQueryTask3D &p_query_task) {
	if (!p_query_task.simplify_path || p_query_task.path_points.size() <= 2) {
		return;
	}

	const LocalVector<uint32_t> &simplified_path_indices = NavMeshQueries3D::get_simplified_path_indices(p_query_task.path_points, p_query_task.simplify_epsilon);

	uint32_t index_count = simplified_path_indices.size();

	{
		Vector3 *points_ptr = p_query_task.path_points.ptr();
		for (uint32_t i = 0; i < index_count; i++) {
			points_ptr[i] = points_ptr[simplified_path_indices[i]];
		}
		p_query_task.path_points.resize(index_count);
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_TYPES)) {
		int32_t *types_ptr = p_query_task.path_meta_point_types.ptr();
		for (uint32_t i = 0; i < index_count; i++) {
			types_ptr[i] = types_ptr[simplified_path_indices[i]];
		}
		p_query_task.path_meta_point_types.resize(index_count);
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_RIDS)) {
		RID *rids_ptr = p_query_task.path_meta_point_rids.ptr();
		for (uint32_t i = 0; i < index_count; i++) {
			rids_ptr[i] = rids_ptr[simplified_path_indices[i]];
		}
		p_query_task.path_meta_point_rids.resize(index_count);
	}

	if (p_query_task.metadata_flags.has_flag(PathMetadataFlags::PATH_INCLUDE_OWNERS)) {
		int64_t *owners_ptr = p_query_task.path_meta_point_owners.ptr();
		for (uint32_t i = 0; i < index_count; i++) {
			owners_ptr[i] = owners_ptr[simplified_path_indices[i]];
		}
		p_query_task.path_meta_point_owners.resize(index_count);
	}
}

void NavMeshQueries3D::_path_corridor_post_process_corridorfunnel(NavMeshPathQueryTask3D &p_query_task, int p_least_cost_id, const gd::Polygon *p_begin_poly, Vector3 p_begin_point, const gd::Polygon *p_end_polygon, Vector3 p_end_point, const Vector3 &p_map_up) {
	LocalVector<gd::NavigationPoly> &p_path_corridor = p_query_task.path_query_slot->path_corridor;

	// Set the apex poly/point to the end point
	gd::NavigationPoly *apex_poly = &p_path_corridor[p_least_cost_id];

	Vector3 back_pathway[2] = { apex_poly->back_navigation_edge_pathway_start, apex_poly->back_navigation_edge_pathway_end };
	const Vector3 back_edge_closest_point = Geometry3D::get_closest_point_to_segment(p_end_point, back_pathway);
	if (p_end_point.is_equal_approx(back_edge_closest_point)) {
		// The end point is basically on top of the last crossed edge, funneling around the corners would at best do nothing.
		// At worst it would add an unwanted path point before the last point due to precision issues so skip to the next polygon.
		if (apex_poly->back_navigation_poly_id != -1) {
			apex_poly = &p_path_corridor[apex_poly->back_navigation_poly_id];
		}
	}

	Vector3 apex_point = p_end_point;

	gd::NavigationPoly *left_poly = apex_poly;
	Vector3 left_portal = apex_point;
	gd::NavigationPoly *right_poly = apex_poly;
	Vector3 right_portal = apex_point;

	gd::NavigationPoly *p = apex_poly;

	_query_task_push_back_point_with_metadata(p_query_task, p_end_point, p_end_polygon);

	while (p) {
		// Set left and right points of the pathway between polygons.
		Vector3 left = p->back_navigation_edge_pathway_start;
		Vector3 right = p->back_navigation_edge_pathway_end;
		if (THREE_POINTS_CROSS_PRODUCT(apex_point, left, right).dot(p_map_up) < 0) {
			SWAP(left, right);
		}

		bool skip = false;
		if (THREE_POINTS_CROSS_PRODUCT(apex_point, left_portal, left).dot(p_map_up) >= 0) {
			//process
			if (left_portal == apex_point || THREE_POINTS_CROSS_PRODUCT(apex_point, left, right_portal).dot(p_map_up) > 0) {
				left_poly = p;
				left_portal = left;
			} else {
				clip_path(p_query_task, p_path_corridor, apex_poly, right_portal, right_poly, p_map_up);

				apex_point = right_portal;
				p = right_poly;
				left_poly = p;
				apex_poly = p;
				left_portal = apex_point;
				right_portal = apex_point;

				_query_task_push_back_point_with_metadata(p_query_task, apex_point, apex_poly->poly);

				skip = true;
			}
		}

		if (!skip && THREE_POINTS_CROSS_PRODUCT(apex_point, right_portal, right).dot(p_map_up) <= 0) {
			//process
			if (right_portal == apex_point || THREE_POINTS_CROSS_PRODUCT(apex_point, right, left_portal).dot(p_map_up) < 0) {
				right_poly = p;
				right_portal = right;
			} else {
				clip_path(p_query_task, p_path_corridor, apex_poly, left_portal, left_poly, p_map_up);

				apex_point = left_portal;
				p = left_poly;
				right_poly = p;
				apex_poly = p;
				right_portal = apex_point;
				left_portal = apex_point;

				_query_task_push_back_point_with_metadata(p_query_task, apex_point, apex_poly->poly);
			}
		}

		// Go to the previous polygon.
		if (p->back_navigation_poly_id != -1) {
			p = &p_path_corridor[p->back_navigation_poly_id];
		} else {
			// The end
			p = nullptr;
		}
	}

	// If the last point is not the begin point, add it to the list.
	if (p_query_task.path_points[p_query_task.path_points.size() - 1] != p_begin_point) {
		_query_task_push_back_point_with_metadata(p_query_task, p_begin_point, p_begin_poly);
	}
}

void NavMeshQueries3D::_path_corridor_post_process_edgecentered(NavMeshPathQueryTask3D &p_query_task, int p_least_cost_id, const gd::Polygon *p_begin_poly, Vector3 p_begin_point, const gd::Polygon *p_end_polygon, Vector3 p_end_point) {
	LocalVector<gd::NavigationPoly> &p_path_corridor = p_query_task.path_query_slot->path_corridor;

	_query_task_push_back_point_with_metadata(p_query_task, p_end_point, p_end_polygon);

	// Add mid points.
	int np_id = p_least_cost_id;
	while (np_id != -1 && p_path_corridor[np_id].back_navigation_poly_id != -1) {
		if (p_path_corridor[np_id].back_navigation_edge != -1) {
			int prev = p_path_corridor[np_id].back_navigation_edge;
			int prev_n = (p_path_corridor[np_id].back_navigation_edge + 1) % p_path_corridor[np_id].poly->points.size();
			Vector3 point = (p_path_corridor[np_id].poly->points[prev].pos + p_path_corridor[np_id].poly->points[prev_n].pos) * 0.5;

			_query_task_push_back_point_with_metadata(p_query_task, point, p_path_corridor[np_id].poly);
		} else {
			_query_task_push_back_point_with_metadata(p_query_task, p_path_corridor[np_id].entry, p_path_corridor[np_id].poly);
		}

		np_id = p_path_corridor[np_id].back_navigation_poly_id;
	}

	_query_task_push_back_point_with_metadata(p_query_task, p_begin_point, p_begin_poly);
}

void NavMeshQueries3D::_path_corridor_post_process_nopostprocessing(NavMeshPathQueryTask3D &p_query_task, int p_least_cost_id, const gd::Polygon *p_begin_poly, Vector3 p_begin_point, const gd::Polygon *p_end_polygon, Vector3 p_end_point) {
	LocalVector<gd::NavigationPoly> &p_path_corridor = p_query_task.path_query_slot->path_corridor;

	_query_task_push_back_point_with_metadata(p_query_task, p_end_point, p_end_polygon);

	// Add mid points.
	int np_id = p_least_cost_id;
	while (np_id != -1 && p_path_corridor[np_id].back_navigation_poly_id != -1) {
		_query_task_push_back_point_with_metadata(p_query_task, p_path_corridor[np_id].entry, p_path_corridor[np_id].poly);

		np_id = p_path_corridor[np_id].back_navigation_poly_id;
	}

	_query_task_push_back_point_with_metadata(p_query_task, p_begin_point, p_begin_poly);
}

void NavMeshQueries3D::_query_task_find_start_end_positions(NavMeshPathQueryTask3D &p_query_task, const LocalVector<gd::Polygon> &p_polygons, const gd::Polygon **r_begin_poly, Vector3 &r_begin_point, const gd::Polygon **r_end_poly, Vector3 &r_end_point) {
	real_t begin_d = FLT_MAX;
	real_t end_d = FLT_MAX;

	// Find the initial poly and the end poly on this map.
	for (const gd::Polygon &p : p_polygons) {
		// Only consider the polygon if it in a region with compatible layers.
		if ((p_query_task.navigation_layers & p.owner->get_navigation_layers()) == 0) {
			continue;
		}

		// For each face check the distance between the origin/destination.
		for (size_t point_id = 2; point_id < p.points.size(); point_id++) {
			const Face3 face(p.points[0].pos, p.points[point_id - 1].pos, p.points[point_id].pos);

			Vector3 point = face.get_closest_point_to(p_query_task.start_position);
			real_t distance_to_point = point.distance_to(p_query_task.start_position);
			if (distance_to_point < begin_d) {
				begin_d = distance_to_point;
				*r_begin_poly = &p;
				r_begin_point = point;
			}

			point = face.get_closest_point_to(p_query_task.target_position);
			distance_to_point = point.distance_to(p_query_task.target_position);
			if (distance_to_point < end_d) {
				end_d = distance_to_point;
				*r_end_poly = &p;
				r_end_point = point;
			}
		}
	}
}

Vector3 NavMeshQueries3D::polygons_get_closest_point_to_segment(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_from, const Vector3 &p_to, const bool p_use_collision) {
	bool use_collision = p_use_collision;
	Vector3 closest_point;
	real_t closest_point_distance = FLT_MAX;

	for (const gd::Polygon &polygon : p_polygons) {
		// For each face check the distance to the segment.
		for (size_t point_id = 2; point_id < polygon.points.size(); point_id += 1) {
			const Face3 face(polygon.points[0].pos, polygon.points[point_id - 1].pos, polygon.points[point_id].pos);
			Vector3 intersection_point;
			if (face.intersects_segment(p_from, p_to, &intersection_point)) {
				const real_t d = p_from.distance_to(intersection_point);
				if (!use_collision) {
					closest_point = intersection_point;
					use_collision = true;
					closest_point_distance = d;
				} else if (closest_point_distance > d) {
					closest_point = intersection_point;
					closest_point_distance = d;
				}
			}
			// If segment does not itersect face, check the distance from segment's endpoints.
			else if (!use_collision) {
				const Vector3 p_from_closest = face.get_closest_point_to(p_from);
				const real_t d_p_from = p_from.distance_to(p_from_closest);
				if (closest_point_distance > d_p_from) {
					closest_point = p_from_closest;
					closest_point_distance = d_p_from;
				}

				const Vector3 p_to_closest = face.get_closest_point_to(p_to);
				const real_t d_p_to = p_to.distance_to(p_to_closest);
				if (closest_point_distance > d_p_to) {
					closest_point = p_to_closest;
					closest_point_distance = d_p_to;
				}
			}
		}
		// Finally, check for a case when shortest distance is between some point located on a face's edge and some point located on a line segment.
		if (!use_collision) {
			for (size_t point_id = 0; point_id < polygon.points.size(); point_id += 1) {
				Vector3 a, b;

				Geometry3D::get_closest_points_between_segments(
						p_from,
						p_to,
						polygon.points[point_id].pos,
						polygon.points[(point_id + 1) % polygon.points.size()].pos,
						a,
						b);

				const real_t d = a.distance_to(b);
				if (d < closest_point_distance) {
					closest_point_distance = d;
					closest_point = b;
				}
			}
		}
	}

	return closest_point;
}

Vector3 NavMeshQueries3D::polygons_get_closest_point(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_point) {
	gd::ClosestPointQueryResult cp = polygons_get_closest_point_info(p_polygons, p_point);
	return cp.point;
}

Vector3 NavMeshQueries3D::polygons_get_closest_point_normal(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_point) {
	gd::ClosestPointQueryResult cp = polygons_get_closest_point_info(p_polygons, p_point);
	return cp.normal;
}

gd::ClosestPointQueryResult NavMeshQueries3D::polygons_get_closest_point_info(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_point) {
	gd::ClosestPointQueryResult result;
	real_t closest_point_distance_squared = FLT_MAX;

	for (const gd::Polygon &polygon : p_polygons) {
		Vector3 plane_normal = (polygon.points[1].pos - polygon.points[0].pos).cross(polygon.points[2].pos - polygon.points[0].pos);
		Vector3 closest_on_polygon;
		real_t closest = FLT_MAX;
		bool inside = true;
		Vector3 previous = polygon.points[polygon.points.size() - 1].pos;
		for (size_t point_id = 0; point_id < polygon.points.size(); ++point_id) {
			Vector3 edge = polygon.points[point_id].pos - previous;
			Vector3 to_point = p_point - previous;
			Vector3 edge_to_point_pormal = edge.cross(to_point);
			bool clockwise = edge_to_point_pormal.dot(plane_normal) > 0;
			// If we are not clockwise, the point will never be inside the polygon and so the closest point will be on an edge.
			if (!clockwise) {
				inside = false;
				real_t point_projected_on_edge = edge.dot(to_point);
				real_t edge_square = edge.length_squared();

				if (point_projected_on_edge > edge_square) {
					real_t distance = polygon.points[point_id].pos.distance_squared_to(p_point);
					if (distance < closest) {
						closest_on_polygon = polygon.points[point_id].pos;
						closest = distance;
					}
				} else if (point_projected_on_edge < 0.f) {
					real_t distance = previous.distance_squared_to(p_point);
					if (distance < closest) {
						closest_on_polygon = previous;
						closest = distance;
					}
				} else {
					// If we project on this edge, this will be the closest point.
					real_t percent = point_projected_on_edge / edge_square;
					closest_on_polygon = previous + percent * edge;
					break;
				}
			}
			previous = polygon.points[point_id].pos;
		}

		if (inside) {
			Vector3 plane_normalized = plane_normal.normalized();
			real_t distance = plane_normalized.dot(p_point - polygon.points[0].pos);
			real_t distance_squared = distance * distance;
			if (distance_squared < closest_point_distance_squared) {
				closest_point_distance_squared = distance_squared;
				result.point = p_point - plane_normalized * distance;
				result.normal = plane_normal;
				result.owner = polygon.owner->get_self();

				if (Math::is_zero_approx(distance)) {
					break;
				}
			}
		} else {
			real_t distance = closest_on_polygon.distance_squared_to(p_point);
			if (distance < closest_point_distance_squared) {
				closest_point_distance_squared = distance;
				result.point = closest_on_polygon;
				result.normal = plane_normal;
				result.owner = polygon.owner->get_self();
			}
		}
	}

	return result;
}

RID NavMeshQueries3D::polygons_get_closest_point_owner(const LocalVector<gd::Polygon> &p_polygons, const Vector3 &p_point) {
	gd::ClosestPointQueryResult cp = polygons_get_closest_point_info(p_polygons, p_point);
	return cp.owner;
}

void NavMeshQueries3D::clip_path(NavMeshPathQueryTask3D &p_query_task, const LocalVector<gd::NavigationPoly> &p_navigation_polys, const gd::NavigationPoly *from_poly, const Vector3 &p_to_point, const gd::NavigationPoly *p_to_poly, const Vector3 &p_map_up) {
	Vector3 from = p_query_task.path_points[p_query_task.path_points.size() - 1];

	if (from.is_equal_approx(p_to_point)) {
		return;
	}

	Plane cut_plane;
	cut_plane.normal = (from - p_to_point).cross(p_map_up);
	if (cut_plane.normal == Vector3()) {
		return;
	}
	cut_plane.normal.normalize();
	cut_plane.d = cut_plane.normal.dot(from);

	while (from_poly != p_to_poly) {
		Vector3 pathway_start = from_poly->back_navigation_edge_pathway_start;
		Vector3 pathway_end = from_poly->back_navigation_edge_pathway_end;

		ERR_FAIL_COND(from_poly->back_navigation_poly_id == -1);
		from_poly = &p_navigation_polys[from_poly->back_navigation_poly_id];

		if (!pathway_start.is_equal_approx(pathway_end)) {
			Vector3 inters;
			if (cut_plane.intersects_segment(pathway_start, pathway_end, &inters)) {
				if (!inters.is_equal_approx(p_to_point) && !inters.is_equal_approx(p_query_task.path_points[p_query_task.path_points.size() - 1])) {
					_query_task_push_back_point_with_metadata(p_query_task, inters, from_poly->poly);
				}
			}
		}
	}
}

LocalVector<uint32_t> NavMeshQueries3D::get_simplified_path_indices(const LocalVector<Vector3> &p_path, real_t p_epsilon) {
	p_epsilon = MAX(0.0, p_epsilon);
	real_t squared_epsilon = p_epsilon * p_epsilon;

	LocalVector<bool> valid_points;
	valid_points.resize(p_path.size());
	for (uint32_t i = 0; i < valid_points.size(); i++) {
		valid_points[i] = false;
	}

	simplify_path_segment(0, p_path.size() - 1, p_path, squared_epsilon, valid_points);

	int valid_point_index = 0;

	for (bool valid : valid_points) {
		if (valid) {
			valid_point_index += 1;
		}
	}

	LocalVector<uint32_t> simplified_path_indices;
	simplified_path_indices.resize(valid_point_index);
	valid_point_index = 0;

	for (uint32_t i = 0; i < valid_points.size(); i++) {
		if (valid_points[i]) {
			simplified_path_indices[valid_point_index] = i;
			valid_point_index += 1;
		}
	}

	return simplified_path_indices;
}

void NavMeshQueries3D::simplify_path_segment(int p_start_inx, int p_end_inx, const LocalVector<Vector3> &p_points, real_t p_epsilon, LocalVector<bool> &r_valid_points) {
	r_valid_points[p_start_inx] = true;
	r_valid_points[p_end_inx] = true;

	Vector3 path_segment[2] = { p_points[p_start_inx], p_points[p_end_inx] };

	real_t point_max_distance = 0.0;
	int point_max_index = 0;

	for (int i = p_start_inx; i < p_end_inx; i++) {
		const Vector3 &checked_point = p_points[i];

		const Vector3 closest_point = Geometry3D::get_closest_point_to_segment(checked_point, path_segment);
		real_t distance_squared = closest_point.distance_squared_to(checked_point);

		if (distance_squared > point_max_distance) {
			point_max_index = i;
			point_max_distance = distance_squared;
		}
	}

	if (point_max_distance > p_epsilon) {
		simplify_path_segment(p_start_inx, point_max_index, p_points, p_epsilon, r_valid_points);
		simplify_path_segment(point_max_index, p_end_inx, p_points, p_epsilon, r_valid_points);
	}
}

#endif // _3D_DISABLED
