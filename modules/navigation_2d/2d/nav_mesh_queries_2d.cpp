/**************************************************************************/
/*  nav_mesh_queries_2d.cpp                                               */
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

#include "nav_mesh_queries_2d.h"

#include "../nav_base_2d.h"
#include "../nav_map_2d.h"
#include "../triangle2.h"
#include "nav_region_iteration_2d.h"

#include "core/math/geometry_2d.h"
#include "servers/navigation/navigation_utilities.h"

using namespace nav_2d;

#define THREE_POINTS_CROSS_PRODUCT(m_a, m_b, m_c) (((m_c) - (m_a)).cross((m_b) - (m_a)))

bool NavMeshQueries2D::emit_callback(const Callable &p_callback) {
	ERR_FAIL_COND_V(!p_callback.is_valid(), false);

	Callable::CallError ce;
	Variant result;
	p_callback.callp(nullptr, 0, result, ce);

	return ce.error == Callable::CallError::CALL_OK;
}

Vector2 NavMeshQueries2D::polygons_get_random_point(const LocalVector<Polygon> &p_polygons, uint32_t p_navigation_layers, bool p_uniformly) {
	const LocalVector<Polygon> &region_polygons = p_polygons;

	if (region_polygons.is_empty()) {
		return Vector2();
	}

	if (p_uniformly) {
		real_t accumulated_area = 0;
		RBMap<real_t, uint32_t> region_area_map;

		for (uint32_t rp_index = 0; rp_index < region_polygons.size(); rp_index++) {
			const Polygon &region_polygon = region_polygons[rp_index];
			real_t polyon_area = region_polygon.surface_area;

			if (polyon_area == 0.0) {
				continue;
			}
			region_area_map[accumulated_area] = rp_index;
			accumulated_area += polyon_area;
		}
		if (region_area_map.is_empty() || accumulated_area == 0) {
			// All polygons have no real surface / no area.
			return Vector2();
		}

		real_t region_area_map_pos = Math::random(real_t(0), accumulated_area);

		RBMap<real_t, uint32_t>::Iterator region_E = region_area_map.find_closest(region_area_map_pos);
		ERR_FAIL_COND_V(!region_E, Vector2());
		uint32_t rrp_polygon_index = region_E->value;
		ERR_FAIL_UNSIGNED_INDEX_V(rrp_polygon_index, region_polygons.size(), Vector2());

		const Polygon &rr_polygon = region_polygons[rrp_polygon_index];

		real_t accumulated_polygon_area = 0;
		RBMap<real_t, uint32_t> polygon_area_map;

		for (uint32_t rpp_index = 2; rpp_index < rr_polygon.points.size(); rpp_index++) {
			real_t triangle_area = Triangle2(rr_polygon.points[0].pos, rr_polygon.points[rpp_index - 1].pos, rr_polygon.points[rpp_index].pos).get_area();

			if (triangle_area == 0.0) {
				continue;
			}
			polygon_area_map[accumulated_polygon_area] = rpp_index;
			accumulated_polygon_area += triangle_area;
		}
		if (polygon_area_map.is_empty() || accumulated_polygon_area == 0) {
			// All faces have no real surface / no area.
			return Vector2();
		}

		real_t polygon_area_map_pos = Math::random(real_t(0), accumulated_polygon_area);

		RBMap<real_t, uint32_t>::Iterator polygon_E = polygon_area_map.find_closest(polygon_area_map_pos);
		ERR_FAIL_COND_V(!polygon_E, Vector2());
		uint32_t rrp_face_index = polygon_E->value;
		ERR_FAIL_UNSIGNED_INDEX_V(rrp_face_index, rr_polygon.points.size(), Vector2());

		const Triangle2 triangle(rr_polygon.points[0].pos, rr_polygon.points[rrp_face_index - 1].pos, rr_polygon.points[rrp_face_index].pos);

		Vector2 triangle_random_position = triangle.get_random_point_inside();
		return triangle_random_position;

	} else {
		uint32_t rrp_polygon_index = Math::random(int(0), region_polygons.size() - 1);

		const Polygon &rr_polygon = region_polygons[rrp_polygon_index];

		uint32_t rrp_face_index = Math::random(int(2), rr_polygon.points.size() - 1);

		const Triangle2 triangle(rr_polygon.points[0].pos, rr_polygon.points[rrp_face_index - 1].pos, rr_polygon.points[rrp_face_index].pos);

		Vector2 triangle_random_position = triangle.get_random_point_inside();
		return triangle_random_position;
	}
}

void NavMeshQueries2D::_query_task_push_back_point_with_metadata(NavMeshPathQueryTask2D &p_query_task, const Vector2 &p_point, const Polygon *p_point_polygon) {
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

void NavMeshQueries2D::map_query_path(NavMap2D *p_map, const Ref<NavigationPathQueryParameters2D> &p_query_parameters, Ref<NavigationPathQueryResult2D> p_query_result, const Callable &p_callback) {
	ERR_FAIL_NULL(p_map);
	ERR_FAIL_COND(p_query_parameters.is_null());
	ERR_FAIL_COND(p_query_result.is_null());

	using namespace NavigationUtilities;

	NavMeshQueries2D::NavMeshPathQueryTask2D query_task;
	query_task.start_position = p_query_parameters->get_start_position();
	query_task.target_position = p_query_parameters->get_target_position();
	query_task.navigation_layers = p_query_parameters->get_navigation_layers();
	query_task.callback = p_callback;

	switch (p_query_parameters->get_pathfinding_algorithm()) {
		case NavigationPathQueryParameters2D::PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR: {
			query_task.pathfinding_algorithm = PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR;
		} break;
		default: {
			WARN_PRINT("No match for used PathfindingAlgorithm - fallback to default");
			query_task.pathfinding_algorithm = PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR;
		} break;
	}

	switch (p_query_parameters->get_path_postprocessing()) {
		case NavigationPathQueryParameters2D::PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL: {
			query_task.path_postprocessing = PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL;
		} break;
		case NavigationPathQueryParameters2D::PathPostProcessing::PATH_POSTPROCESSING_EDGECENTERED: {
			query_task.path_postprocessing = PathPostProcessing::PATH_POSTPROCESSING_EDGECENTERED;
		} break;
		case NavigationPathQueryParameters2D::PathPostProcessing::PATH_POSTPROCESSING_NONE: {
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
	query_task.status = NavMeshPathQueryTask2D::TaskStatus::QUERY_STARTED;

	p_map->query_path(query_task);

	p_query_result->set_data(
			query_task.path_points,
			query_task.path_meta_point_types,
			query_task.path_meta_point_rids,
			query_task.path_meta_point_owners);

	if (query_task.callback.is_valid()) {
		if (emit_callback(query_task.callback)) {
			query_task.status = NavMeshPathQueryTask2D::TaskStatus::CALLBACK_DISPATCHED;
		} else {
			query_task.status = NavMeshPathQueryTask2D::TaskStatus::CALLBACK_FAILED;
		}
	}
}

void NavMeshQueries2D::_query_task_find_start_end_positions(NavMeshPathQueryTask2D &p_query_task, const NavMapIteration2D &p_map_iteration) {
	real_t begin_d = FLT_MAX;
	real_t end_d = FLT_MAX;

	const LocalVector<NavRegionIteration2D> &regions = p_map_iteration.region_iterations;

	for (const NavRegionIteration2D &region : regions) {
		if (!region.get_enabled()) {
			continue;
		}

		// Find the initial poly and the end poly on this map.
		for (const Polygon &p : region.get_navmesh_polygons()) {
			// Only consider the polygon if it in a region with compatible layers.
			if ((p_query_task.navigation_layers & p.owner->get_navigation_layers()) == 0) {
				continue;
			}

			// For each triangle check the distance between the origin/destination.
			for (size_t point_id = 2; point_id < p.points.size(); point_id++) {
				const Triangle2 triangle(p.points[0].pos, p.points[point_id - 1].pos, p.points[point_id].pos);

				Vector2 point = triangle.get_closest_point_to(p_query_task.start_position);
				real_t distance_to_point = point.distance_to(p_query_task.start_position);
				if (distance_to_point < begin_d) {
					begin_d = distance_to_point;
					p_query_task.begin_polygon = &p;
					p_query_task.begin_position = point;
				}

				point = triangle.get_closest_point_to(p_query_task.target_position);
				distance_to_point = point.distance_to(p_query_task.target_position);
				if (distance_to_point < end_d) {
					end_d = distance_to_point;
					p_query_task.end_polygon = &p;
					p_query_task.end_position = point;
				}
			}
		}
	}
}

void NavMeshQueries2D::_query_task_build_path_corridor(NavMeshPathQueryTask2D &p_query_task) {
	const Vector2 p_target_position = p_query_task.target_position;
	const uint32_t p_navigation_layers = p_query_task.navigation_layers;
	const Polygon *begin_poly = p_query_task.begin_polygon;
	const Polygon *end_poly = p_query_task.end_polygon;
	Vector2 begin_point = p_query_task.begin_position;
	Vector2 end_point = p_query_task.end_position;

	// Heap of polygons to travel next.
	Heap<NavigationPoly *, NavPolyTravelCostGreaterThan, NavPolyHeapIndexer>
			&traversable_polys = p_query_task.path_query_slot->traversable_polys;
	traversable_polys.clear();

	LocalVector<NavigationPoly> &navigation_polys = p_query_task.path_query_slot->path_corridor;
	for (NavigationPoly &polygon : navigation_polys) {
		polygon.reset();
	}

	// Initialize the matching navigation polygon.
	NavigationPoly &begin_navigation_poly = navigation_polys[begin_poly->id];
	begin_navigation_poly.poly = begin_poly;
	begin_navigation_poly.entry = begin_point;
	begin_navigation_poly.back_navigation_edge_pathway_start = begin_point;
	begin_navigation_poly.back_navigation_edge_pathway_end = begin_point;
	begin_navigation_poly.traveled_distance = 0.0;

	// This is an implementation of the A* algorithm.
	uint32_t least_cost_id = begin_poly->id;
	bool found_route = false;

	const Polygon *reachable_end = nullptr;
	real_t distance_to_reachable_end = FLT_MAX;
	bool is_reachable = true;
	real_t poly_enter_cost = 0.0;

	while (true) {
		const NavigationPoly &least_cost_poly = navigation_polys[least_cost_id];
		real_t poly_travel_cost = least_cost_poly.poly->owner->get_travel_cost();

		// Takes the current least_cost_poly neighbors (iterating over its edges) and compute the traveled_distance.
		for (const Edge &edge : least_cost_poly.poly->edges) {
			// Iterate over connections in this edge, then compute the new optimized travel distance assigned to this polygon.
			for (uint32_t connection_index = 0; connection_index < edge.connections.size(); connection_index++) {
				const Edge::Connection &connection = edge.connections[connection_index];

				// Only consider the connection to another polygon if this polygon is in a region with compatible layers.
				const NavBaseIteration2D *owner = connection.polygon->owner;
				if ((p_navigation_layers & owner->get_navigation_layers()) != 0) {
					Vector2 pathway[2] = { connection.pathway_start, connection.pathway_end };
					const Vector2 new_entry = Geometry2D::get_closest_point_to_segment(least_cost_poly.entry, pathway);
					const real_t new_traveled_distance = least_cost_poly.entry.distance_squared_to(new_entry) * poly_travel_cost + poly_enter_cost + least_cost_poly.traveled_distance;

					// Check if the neighbor polygon has already been processed.
					NavigationPoly &neighbor_poly = navigation_polys[connection.polygon->id];
					if (new_traveled_distance < neighbor_poly.traveled_distance) {
						// Add the polygon to the heap of polygons to traverse next.
						neighbor_poly.back_navigation_poly_id = least_cost_id;
						neighbor_poly.back_navigation_edge = connection.edge;
						neighbor_poly.back_navigation_edge_pathway_start = connection.pathway_start;
						neighbor_poly.back_navigation_edge_pathway_end = connection.pathway_end;
						neighbor_poly.traveled_distance = new_traveled_distance;
						neighbor_poly.distance_to_destination =
								new_entry.distance_to(end_point) *
								owner->get_travel_cost();
						neighbor_poly.entry = new_entry;

						if (neighbor_poly.traversable_poly_index != traversable_polys.INVALID_INDEX) {
							traversable_polys.shift(neighbor_poly.traversable_poly_index);
						} else {
							neighbor_poly.poly = connection.polygon;
							traversable_polys.push(&neighbor_poly);
						}
					}
				}
			}
		}

		poly_enter_cost = 0;
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
				Triangle2 t(end_poly->points[0].pos, end_poly->points[point_id - 1].pos, end_poly->points[point_id].pos);
				Vector2 spoint = t.get_closest_point_to(p_target_position);
				real_t dpoint = spoint.distance_squared_to(p_target_position);
				if (dpoint < end_d) {
					end_point = spoint;
					end_d = dpoint;
				}
			}

			// Search all faces of start polygon as well.
			bool closest_point_on_start_poly = false;
			for (size_t point_id = 2; point_id < begin_poly->points.size(); point_id++) {
				Triangle2 t(begin_poly->points[0].pos, begin_poly->points[point_id - 1].pos, begin_poly->points[point_id].pos);
				Vector2 spoint = t.get_closest_point_to(p_target_position);
				real_t dpoint = spoint.distance_squared_to(p_target_position);
				if (dpoint < end_d) {
					end_point = spoint;
					end_d = dpoint;
					closest_point_on_start_poly = true;
				}
			}

			if (closest_point_on_start_poly) {
				// No point to run PostProcessing when start and end convex polygon is the same.
				p_query_task.path_clear();

				_query_task_push_back_point_with_metadata(p_query_task, begin_point, begin_poly);
				_query_task_push_back_point_with_metadata(p_query_task, end_point, begin_poly);
				p_query_task.status = NavMeshPathQueryTask2D::TaskStatus::QUERY_FINISHED;
				return;
			}

			for (NavigationPoly &nav_poly : navigation_polys) {
				nav_poly.poly = nullptr;
				nav_poly.traveled_distance = FLT_MAX;
			}
			navigation_polys[begin_poly->id].poly = begin_poly;
			navigation_polys[begin_poly->id].traveled_distance = 0;
			least_cost_id = begin_poly->id;
			reachable_end = nullptr;
		} else {
			// Pop the polygon with the lowest travel cost from the heap of traversable polygons.
			least_cost_id = traversable_polys.pop()->poly->id;

			// Store the farthest reachable end polygon in case our goal is not reachable.
			if (is_reachable) {
				real_t distance = navigation_polys[least_cost_id].entry.distance_squared_to(p_target_position);
				if (distance_to_reachable_end > distance) {
					distance_to_reachable_end = distance;
					reachable_end = navigation_polys[least_cost_id].poly;
				}
			}

			// Check if we reached the end
			if (navigation_polys[least_cost_id].poly == end_poly) {
				found_route = true;
				break;
			}

			if (navigation_polys[least_cost_id].poly->owner->get_self() != least_cost_poly.poly->owner->get_self()) {
				poly_enter_cost = least_cost_poly.poly->owner->get_enter_cost();
			}
		}
	}

	// We did not find a route but we have both a start polygon and an end polygon at this point.
	// Usually this happens because there was not a single external or internal connected edge, e.g. our start polygon is an isolated, single convex polygon.
	if (!found_route) {
		real_t end_d = FLT_MAX;
		// Search all faces of the start polygon for the closest point to our target position.
		for (size_t point_id = 2; point_id < begin_poly->points.size(); point_id++) {
			Triangle2 t(begin_poly->points[0].pos, begin_poly->points[point_id - 1].pos, begin_poly->points[point_id].pos);
			Vector2 spoint = t.get_closest_point_to(p_target_position);
			real_t dpoint = spoint.distance_squared_to(p_target_position);
			if (dpoint < end_d) {
				end_point = spoint;
				end_d = dpoint;
			}
		}

		p_query_task.path_clear();

		_query_task_push_back_point_with_metadata(p_query_task, begin_point, begin_poly);
		_query_task_push_back_point_with_metadata(p_query_task, end_point, begin_poly);
		p_query_task.status = NavMeshPathQueryTask2D::TaskStatus::QUERY_FINISHED;
	} else {
		p_query_task.end_position = end_point;
		p_query_task.end_polygon = end_poly;
		p_query_task.begin_position = begin_point;
		p_query_task.begin_polygon = begin_poly;
		p_query_task.least_cost_id = least_cost_id;
	}
}

void NavMeshQueries2D::query_task_map_iteration_get_path(NavMeshPathQueryTask2D &p_query_task, const NavMapIteration2D &p_map_iteration) {
	p_query_task.path_clear();

	_query_task_find_start_end_positions(p_query_task, p_map_iteration);

	// Check for trivial cases.
	if (!p_query_task.begin_polygon || !p_query_task.end_polygon) {
		p_query_task.status = NavMeshPathQueryTask2D::TaskStatus::QUERY_FINISHED;
		return;
	}
	if (p_query_task.begin_polygon == p_query_task.end_polygon) {
		p_query_task.path_clear();
		_query_task_push_back_point_with_metadata(p_query_task, p_query_task.begin_position, p_query_task.begin_polygon);
		_query_task_push_back_point_with_metadata(p_query_task, p_query_task.end_position, p_query_task.end_polygon);
		p_query_task.status = NavMeshPathQueryTask2D::TaskStatus::QUERY_FINISHED;
		return;
	}

	_query_task_build_path_corridor(p_query_task);

	if (p_query_task.status == NavMeshPathQueryTask2D::TaskStatus::QUERY_FINISHED || p_query_task.status == NavMeshPathQueryTask2D::TaskStatus::QUERY_FAILED) {
		return;
	}

	// Post-Process path.
	switch (p_query_task.path_postprocessing) {
		case PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL: {
			_query_task_post_process_corridorfunnel(p_query_task);
		} break;
		case PathPostProcessing::PATH_POSTPROCESSING_EDGECENTERED: {
			_query_task_post_process_edgecentered(p_query_task);
		} break;
		case PathPostProcessing::PATH_POSTPROCESSING_NONE: {
			_query_task_post_process_nopostprocessing(p_query_task);
		} break;
		default: {
			WARN_PRINT("No match for used PathPostProcessing - fallback to default");
			_query_task_post_process_corridorfunnel(p_query_task);
		} break;
	}

	p_query_task.path_reverse();

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

	p_query_task.status = NavMeshPathQueryTask2D::TaskStatus::QUERY_FINISHED;
}

void NavMeshQueries2D::_query_task_simplified_path_points(NavMeshPathQueryTask2D &p_query_task) {
	if (!p_query_task.simplify_path || p_query_task.path_points.size() <= 2) {
		return;
	}

	const LocalVector<uint32_t> &simplified_path_indices = NavMeshQueries2D::get_simplified_path_indices(p_query_task.path_points, p_query_task.simplify_epsilon);

	uint32_t index_count = simplified_path_indices.size();

	{
		Vector2 *points_ptr = p_query_task.path_points.ptr();
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

void NavMeshQueries2D::_query_task_post_process_corridorfunnel(NavMeshPathQueryTask2D &p_query_task) {
	Vector2 end_point = p_query_task.end_position;
	const Polygon *end_poly = p_query_task.end_polygon;
	Vector2 begin_point = p_query_task.begin_position;
	const Polygon *begin_poly = p_query_task.begin_polygon;
	uint32_t least_cost_id = p_query_task.least_cost_id;
	LocalVector<NavigationPoly> &navigation_polys = p_query_task.path_query_slot->path_corridor;

	// Set the apex poly/point to the end point.
	NavigationPoly *apex_poly = &navigation_polys[least_cost_id];

	Vector2 back_pathway[2] = { apex_poly->back_navigation_edge_pathway_start, apex_poly->back_navigation_edge_pathway_end };
	const Vector2 back_edge_closest_point = Geometry2D::get_closest_point_to_segment(end_point, back_pathway);
	if (end_point.is_equal_approx(back_edge_closest_point)) {
		// The end point is basically on top of the last crossed edge, funneling around the corners would at best do nothing.
		// At worst it would add an unwanted path point before the last point due to precision issues so skip to the next polygon.
		if (apex_poly->back_navigation_poly_id != -1) {
			apex_poly = &navigation_polys[apex_poly->back_navigation_poly_id];
		}
	}

	Vector2 apex_point = end_point;

	NavigationPoly *left_poly = apex_poly;
	Vector2 left_portal = apex_point;
	NavigationPoly *right_poly = apex_poly;
	Vector2 right_portal = apex_point;

	NavigationPoly *p = apex_poly;

	_query_task_push_back_point_with_metadata(p_query_task, end_point, end_poly);

	while (p) {
		// Set left and right points of the pathway between polygons.
		Vector2 left = p->back_navigation_edge_pathway_start;
		Vector2 right = p->back_navigation_edge_pathway_end;
		if (THREE_POINTS_CROSS_PRODUCT(apex_point, left, right) < 0) {
			SWAP(left, right);
		}

		bool skip = false;
		if (THREE_POINTS_CROSS_PRODUCT(apex_point, left_portal, left) >= 0) {
			// Process.
			if (left_portal == apex_point || THREE_POINTS_CROSS_PRODUCT(apex_point, left, right_portal) > 0) {
				left_poly = p;
				left_portal = left;
			} else {
				_query_task_clip_path(p_query_task, apex_poly, right_portal, right_poly);

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

		if (!skip && THREE_POINTS_CROSS_PRODUCT(apex_point, right_portal, right) <= 0) {
			// Process.
			if (right_portal == apex_point || THREE_POINTS_CROSS_PRODUCT(apex_point, right, left_portal) < 0) {
				right_poly = p;
				right_portal = right;
			} else {
				_query_task_clip_path(p_query_task, apex_poly, left_portal, left_poly);

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
			p = &navigation_polys[p->back_navigation_poly_id];
		} else {
			// The end
			p = nullptr;
		}
	}

	// If the last point is not the begin point, add it to the list.
	if (p_query_task.path_points[p_query_task.path_points.size() - 1] != begin_point) {
		_query_task_push_back_point_with_metadata(p_query_task, begin_point, begin_poly);
	}
}

void NavMeshQueries2D::_query_task_post_process_edgecentered(NavMeshPathQueryTask2D &p_query_task) {
	Vector2 end_point = p_query_task.end_position;
	const Polygon *end_poly = p_query_task.end_polygon;
	Vector2 begin_point = p_query_task.begin_position;
	const Polygon *begin_poly = p_query_task.begin_polygon;
	uint32_t least_cost_id = p_query_task.least_cost_id;
	LocalVector<NavigationPoly> &navigation_polys = p_query_task.path_query_slot->path_corridor;

	_query_task_push_back_point_with_metadata(p_query_task, end_point, end_poly);

	// Add mid points.
	int np_id = least_cost_id;
	while (np_id != -1 && navigation_polys[np_id].back_navigation_poly_id != -1) {
		if (navigation_polys[np_id].back_navigation_edge != -1) {
			int prev = navigation_polys[np_id].back_navigation_edge;
			int prev_n = (navigation_polys[np_id].back_navigation_edge + 1) % navigation_polys[np_id].poly->points.size();
			Vector2 point = (navigation_polys[np_id].poly->points[prev].pos + navigation_polys[np_id].poly->points[prev_n].pos) * 0.5;

			_query_task_push_back_point_with_metadata(p_query_task, point, navigation_polys[np_id].poly);
		} else {
			_query_task_push_back_point_with_metadata(p_query_task, navigation_polys[np_id].entry, navigation_polys[np_id].poly);
		}

		np_id = navigation_polys[np_id].back_navigation_poly_id;
	}

	_query_task_push_back_point_with_metadata(p_query_task, begin_point, begin_poly);
}

void NavMeshQueries2D::_query_task_post_process_nopostprocessing(NavMeshPathQueryTask2D &p_query_task) {
	Vector2 end_point = p_query_task.end_position;
	const Polygon *end_poly = p_query_task.end_polygon;
	Vector2 begin_point = p_query_task.begin_position;
	const Polygon *begin_poly = p_query_task.begin_polygon;
	uint32_t least_cost_id = p_query_task.least_cost_id;
	LocalVector<NavigationPoly> &navigation_polys = p_query_task.path_query_slot->path_corridor;

	_query_task_push_back_point_with_metadata(p_query_task, end_point, end_poly);

	// Add mid points.
	int np_id = least_cost_id;
	while (np_id != -1 && navigation_polys[np_id].back_navigation_poly_id != -1) {
		_query_task_push_back_point_with_metadata(p_query_task, navigation_polys[np_id].entry, navigation_polys[np_id].poly);

		np_id = navigation_polys[np_id].back_navigation_poly_id;
	}

	_query_task_push_back_point_with_metadata(p_query_task, begin_point, begin_poly);
}

Vector2 NavMeshQueries2D::map_iteration_get_closest_point(const NavMapIteration2D &p_map_iteration, const Vector2 &p_point) {
	ClosestPointQueryResult cp = map_iteration_get_closest_point_info(p_map_iteration, p_point);
	return cp.point;
}

RID NavMeshQueries2D::map_iteration_get_closest_point_owner(const NavMapIteration2D &p_map_iteration, const Vector2 &p_point) {
	ClosestPointQueryResult cp = map_iteration_get_closest_point_info(p_map_iteration, p_point);
	return cp.owner;
}

ClosestPointQueryResult NavMeshQueries2D::map_iteration_get_closest_point_info(const NavMapIteration2D &p_map_iteration, const Vector2 &p_point) {
	ClosestPointQueryResult result;
	real_t closest_point_distance_squared = FLT_MAX;

	// TODO: Check for further 2D improvements.

	const LocalVector<NavRegionIteration2D> &regions = p_map_iteration.region_iterations;
	for (const NavRegionIteration2D &region : regions) {
		for (const Polygon &polygon : region.get_navmesh_polygons()) {
			real_t cross = (polygon.points[1].pos - polygon.points[0].pos).cross(polygon.points[2].pos - polygon.points[0].pos);
			Vector2 closest_on_polygon;
			real_t closest = FLT_MAX;
			bool inside = true;
			Vector2 previous = polygon.points[polygon.points.size() - 1].pos;
			for (size_t point_id = 0; point_id < polygon.points.size(); ++point_id) {
				Vector2 edge = polygon.points[point_id].pos - previous;
				Vector2 to_point = p_point - previous;
				real_t edge_to_point_cross = edge.cross(to_point);
				bool clockwise = (edge_to_point_cross * cross) > 0;
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
					} else if (point_projected_on_edge < 0.0) {
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
				closest_point_distance_squared = 0.0;
				result.point = p_point;
				result.owner = polygon.owner->get_self();

				break;
			} else {
				real_t distance = closest_on_polygon.distance_squared_to(p_point);
				if (distance < closest_point_distance_squared) {
					closest_point_distance_squared = distance;
					result.point = closest_on_polygon;
					result.owner = polygon.owner->get_self();
				}
			}
		}
	}

	return result;
}

Vector2 NavMeshQueries2D::map_iteration_get_random_point(const NavMapIteration2D &p_map_iteration, uint32_t p_navigation_layers, bool p_uniformly) {
	if (p_map_iteration.region_iterations.is_empty()) {
		return Vector2();
	}

	LocalVector<uint32_t> accessible_regions;
	accessible_regions.reserve(p_map_iteration.region_iterations.size());

	for (uint32_t i = 0; i < p_map_iteration.region_iterations.size(); i++) {
		const NavRegionIteration2D &region = p_map_iteration.region_iterations[i];
		if (!region.enabled || (p_navigation_layers & region.navigation_layers) == 0) {
			continue;
		}
		accessible_regions.push_back(i);
	}

	if (accessible_regions.is_empty()) {
		// All existing region polygons are disabled.
		return Vector2();
	}

	if (p_uniformly) {
		real_t accumulated_region_surface_area = 0;
		RBMap<real_t, uint32_t> accessible_regions_area_map;

		for (uint32_t accessible_region_index = 0; accessible_region_index < accessible_regions.size(); accessible_region_index++) {
			const NavRegionIteration2D &region = p_map_iteration.region_iterations[accessible_regions[accessible_region_index]];

			real_t region_surface_area = region.surface_area;

			if (region_surface_area == 0.0f) {
				continue;
			}

			accessible_regions_area_map[accumulated_region_surface_area] = accessible_region_index;
			accumulated_region_surface_area += region_surface_area;
		}
		if (accessible_regions_area_map.is_empty() || accumulated_region_surface_area == 0) {
			// All faces have no real surface / no area.
			return Vector2();
		}

		real_t random_accessible_regions_area_map = Math::random(real_t(0), accumulated_region_surface_area);

		RBMap<real_t, uint32_t>::Iterator E = accessible_regions_area_map.find_closest(random_accessible_regions_area_map);
		ERR_FAIL_COND_V(!E, Vector2());
		uint32_t random_region_index = E->value;
		ERR_FAIL_UNSIGNED_INDEX_V(random_region_index, accessible_regions.size(), Vector2());

		const NavRegionIteration2D &random_region = p_map_iteration.region_iterations[accessible_regions[random_region_index]];

		return NavMeshQueries2D::polygons_get_random_point(random_region.navmesh_polygons, p_navigation_layers, p_uniformly);

	} else {
		uint32_t random_region_index = Math::random(int(0), accessible_regions.size() - 1);

		const NavRegionIteration2D &random_region = p_map_iteration.region_iterations[accessible_regions[random_region_index]];

		return NavMeshQueries2D::polygons_get_random_point(random_region.navmesh_polygons, p_navigation_layers, p_uniformly);
	}
}

Vector2 NavMeshQueries2D::polygons_get_closest_point(const LocalVector<Polygon> &p_polygons, const Vector2 &p_point) {
	ClosestPointQueryResult cp = polygons_get_closest_point_info(p_polygons, p_point);
	return cp.point;
}

ClosestPointQueryResult NavMeshQueries2D::polygons_get_closest_point_info(const LocalVector<Polygon> &p_polygons, const Vector2 &p_point) {
	ClosestPointQueryResult result;
	real_t closest_point_distance_squared = FLT_MAX;

	// TODO: Check for further 2D improvements.

	for (const Polygon &polygon : p_polygons) {
		real_t cross = (polygon.points[1].pos - polygon.points[0].pos).cross(polygon.points[2].pos - polygon.points[0].pos);
		Vector2 closest_on_polygon;
		real_t closest = FLT_MAX;
		bool inside = true;
		Vector2 previous = polygon.points[polygon.points.size() - 1].pos;
		for (size_t point_id = 0; point_id < polygon.points.size(); ++point_id) {
			Vector2 edge = polygon.points[point_id].pos - previous;
			Vector2 to_point = p_point - previous;
			real_t edge_to_point_cross = edge.cross(to_point);
			bool clockwise = (edge_to_point_cross * cross) > 0;
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
				} else if (point_projected_on_edge < 0.0) {
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
			closest_point_distance_squared = 0.0;
			result.point = p_point;
			result.owner = polygon.owner->get_self();
			break;
		} else {
			real_t distance = closest_on_polygon.distance_squared_to(p_point);
			if (distance < closest_point_distance_squared) {
				closest_point_distance_squared = distance;
				result.point = closest_on_polygon;
				result.owner = polygon.owner->get_self();
			}
		}
	}

	return result;
}

RID NavMeshQueries2D::polygons_get_closest_point_owner(const LocalVector<Polygon> &p_polygons, const Vector2 &p_point) {
	ClosestPointQueryResult cp = polygons_get_closest_point_info(p_polygons, p_point);
	return cp.owner;
}

static bool _line_intersects_segment(const Vector2 &p_line_normal, real_t p_line_d, const Vector2 &p_segment_begin, const Vector2 &p_segment_end, Vector2 &r_intersection) {
	Vector2 segment = p_segment_begin - p_segment_end;
	real_t den = p_line_normal.dot(segment);

	if (Math::is_zero_approx(den)) {
		return false;
	}

	real_t dist = (p_line_normal.dot(p_segment_begin) - p_line_d) / den;

	if (dist < (real_t)-CMP_EPSILON || dist > (1.0 + (real_t)CMP_EPSILON)) {
		return false;
	}

	r_intersection = p_segment_begin - segment * dist;
	return true;
}

void NavMeshQueries2D::_query_task_clip_path(NavMeshPathQueryTask2D &p_query_task, const NavigationPoly *p_from_poly, const Vector2 &p_to_point, const NavigationPoly *p_to_poly) {
	Vector2 from = p_query_task.path_points[p_query_task.path_points.size() - 1];
	const LocalVector<NavigationPoly> &p_navigation_polys = p_query_task.path_query_slot->path_corridor;

	if (from.is_equal_approx(p_to_point)) {
		return;
	}

	// Compute line parameters (equivalent to the Plane case in 3D).
	const Vector2 normal = -(from - p_to_point).orthogonal().normalized();
	const real_t d = normal.dot(from);

	while (p_from_poly != p_to_poly) {
		Vector2 pathway_start = p_from_poly->back_navigation_edge_pathway_start;
		Vector2 pathway_end = p_from_poly->back_navigation_edge_pathway_end;

		ERR_FAIL_COND(p_from_poly->back_navigation_poly_id == -1);
		p_from_poly = &p_navigation_polys[p_from_poly->back_navigation_poly_id];

		if (!pathway_start.is_equal_approx(pathway_end)) {
			Vector2 inters;
			if (_line_intersects_segment(normal, d, pathway_start, pathway_end, inters)) {
				if (!inters.is_equal_approx(p_to_point) && !inters.is_equal_approx(p_query_task.path_points[p_query_task.path_points.size() - 1])) {
					_query_task_push_back_point_with_metadata(p_query_task, inters, p_from_poly->poly);
				}
			}
		}
	}
}

LocalVector<uint32_t> NavMeshQueries2D::get_simplified_path_indices(const LocalVector<Vector2> &p_path, real_t p_epsilon) {
	p_epsilon = MAX(0.0, p_epsilon);
	real_t squared_epsilon = p_epsilon * p_epsilon;

	LocalVector<uint32_t> simplified_path_indices;
	simplified_path_indices.reserve(p_path.size());
	simplified_path_indices.push_back(0);
	simplify_path_segment(0, p_path.size() - 1, p_path, squared_epsilon, simplified_path_indices);
	simplified_path_indices.push_back(p_path.size() - 1);

	return simplified_path_indices;
}

void NavMeshQueries2D::simplify_path_segment(int p_start_inx, int p_end_inx, const LocalVector<Vector2> &p_points, real_t p_epsilon, LocalVector<uint32_t> &r_simplified_path_indices) {
	Vector2 path_segment[2] = { p_points[p_start_inx], p_points[p_end_inx] };

	real_t point_max_distance = 0.0;
	int point_max_index = 0;

	for (int i = p_start_inx; i < p_end_inx; i++) {
		const Vector2 &checked_point = p_points[i];

		const Vector2 closest_point = Geometry2D::get_closest_point_to_segment(checked_point, path_segment);
		real_t distance_squared = closest_point.distance_squared_to(checked_point);

		if (distance_squared > point_max_distance) {
			point_max_index = i;
			point_max_distance = distance_squared;
		}
	}

	if (point_max_distance > p_epsilon) {
		simplify_path_segment(p_start_inx, point_max_index, p_points, p_epsilon, r_simplified_path_indices);
		r_simplified_path_indices.push_back(point_max_index);
		simplify_path_segment(point_max_index, p_end_inx, p_points, p_epsilon, r_simplified_path_indices);
	}
}
