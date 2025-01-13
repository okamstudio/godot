/**************************************************************************/
/*  nav_map.cpp                                                           */
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

#include "nav_map.h"

#include "nav_agent.h"
#include "nav_link.h"
#include "nav_obstacle.h"
#include "nav_region.h"

#include "core/config/project_settings.h"
#include "core/object/worker_thread_pool.h"

#include <Obstacle2d.h>

#ifdef DEBUG_ENABLED
#define NAVMAP_ITERATION_ZERO_ERROR_MSG() \
	ERR_PRINT_ONCE("NavigationServer navigation map query failed because it was made before first map synchronization.\n\
	NavigationServer 'map_changed' signal can be used to receive update notifications.\n\
	NavigationServer 'map_get_iteration_id()' can be used to check if a map has finished its newest iteration.");
#else
#define NAVMAP_ITERATION_ZERO_ERROR_MSG()
#endif // DEBUG_ENABLED

void NavMap::set_up(Vector3 p_up) {
	if (up == p_up) {
		return;
	}
	up = p_up;
	map_settings_dirty = true;
}

void NavMap::set_cell_size(real_t p_cell_size) {
	if (cell_size == p_cell_size) {
		return;
	}
	cell_size = p_cell_size;
	_update_merge_rasterizer_cell_dimensions();
	map_settings_dirty = true;
}

void NavMap::set_cell_height(real_t p_cell_height) {
	if (cell_height == p_cell_height) {
		return;
	}
	cell_height = p_cell_height;
	_update_merge_rasterizer_cell_dimensions();
	map_settings_dirty = true;
}

void NavMap::set_merge_rasterizer_cell_scale(float p_value) {
	if (merge_rasterizer_cell_scale == p_value) {
		return;
	}
	merge_rasterizer_cell_scale = p_value;
	_update_merge_rasterizer_cell_dimensions();
	map_settings_dirty = true;
}

void NavMap::set_use_edge_connections(bool p_enabled) {
	if (use_edge_connections == p_enabled) {
		return;
	}
	use_edge_connections = p_enabled;
	iteration_dirty = true;
}

void NavMap::set_edge_connection_margin(real_t p_edge_connection_margin) {
	if (edge_connection_margin == p_edge_connection_margin) {
		return;
	}
	edge_connection_margin = p_edge_connection_margin;
	iteration_dirty = true;
}

void NavMap::set_link_connection_radius(real_t p_link_connection_radius) {
	if (link_connection_radius == p_link_connection_radius) {
		return;
	}
	link_connection_radius = p_link_connection_radius;
	iteration_dirty = true;
}

gd::PointKey NavMap::get_point_key(const Vector3 &p_pos) const {
	const int x = static_cast<int>(Math::floor(p_pos.x / merge_rasterizer_cell_size));
	const int y = static_cast<int>(Math::floor(p_pos.y / merge_rasterizer_cell_height));
	const int z = static_cast<int>(Math::floor(p_pos.z / merge_rasterizer_cell_size));

	gd::PointKey p;
	p.key = 0;
	p.x = x;
	p.y = y;
	p.z = z;
	return p;
}

void NavMap::query_path(NavMeshQueries3D::NavMeshPathQueryTask3D &p_query_task) {
	RWLockRead read_lock(map_rwlock);
	if (iteration_id == 0) {
		return;
	}

	path_query_slots_semaphore.wait();

	path_query_slots_mutex.lock();
	for (NavMeshQueries3D::PathQuerySlot &p_path_query_slot : path_query_slots) {
		if (!p_path_query_slot.in_use) {
			p_path_query_slot.in_use = true;
			p_query_task.path_query_slot = &p_path_query_slot;
			break;
		}
	}
	path_query_slots_mutex.unlock();

	if (p_query_task.path_query_slot == nullptr) {
		path_query_slots_semaphore.post();
		ERR_FAIL_NULL_MSG(p_query_task.path_query_slot, "No unused NavMap path query slot found! This should never happen :(.");
	}

	p_query_task.map_up = get_up();

	NavMeshQueries3D::query_task_polygons_get_path(p_query_task, polygons, up, link_polygons.size());

	path_query_slots_mutex.lock();
	uint32_t used_slot_index = p_query_task.path_query_slot->slot_index;
	path_query_slots[used_slot_index].in_use = false;
	p_query_task.path_query_slot = nullptr;
	path_query_slots_mutex.unlock();

	path_query_slots_semaphore.post();
}

Vector3 NavMap::get_closest_point_to_segment(const Vector3 &p_from, const Vector3 &p_to, const bool p_use_collision) const {
	RWLockRead read_lock(map_rwlock);
	if (iteration_id == 0) {
		NAVMAP_ITERATION_ZERO_ERROR_MSG();
		return Vector3();
	}

	return NavMeshQueries3D::polygons_get_closest_point_to_segment(polygons, p_from, p_to, p_use_collision);
}

Vector3 NavMap::get_closest_point(const Vector3 &p_point) const {
	RWLockRead read_lock(map_rwlock);
	if (iteration_id == 0) {
		NAVMAP_ITERATION_ZERO_ERROR_MSG();
		return Vector3();
	}

	return NavMeshQueries3D::polygons_get_closest_point(polygons, p_point);
}

Vector3 NavMap::get_closest_point_normal(const Vector3 &p_point) const {
	RWLockRead read_lock(map_rwlock);
	if (iteration_id == 0) {
		NAVMAP_ITERATION_ZERO_ERROR_MSG();
		return Vector3();
	}

	return NavMeshQueries3D::polygons_get_closest_point_normal(polygons, p_point);
}

RID NavMap::get_closest_point_owner(const Vector3 &p_point) const {
	RWLockRead read_lock(map_rwlock);
	if (iteration_id == 0) {
		NAVMAP_ITERATION_ZERO_ERROR_MSG();
		return RID();
	}

	return NavMeshQueries3D::polygons_get_closest_point_owner(polygons, p_point);
}

gd::ClosestPointQueryResult NavMap::get_closest_point_info(const Vector3 &p_point) const {
	RWLockRead read_lock(map_rwlock);

	return NavMeshQueries3D::polygons_get_closest_point_info(polygons, p_point);
}

void NavMap::add_region(NavRegion *p_region) {
	regions.push_back(p_region);
	iteration_dirty = true;
}

void NavMap::remove_region(NavRegion *p_region) {
	int64_t region_index = regions.find(p_region);
	if (region_index >= 0) {
		regions.remove_at_unordered(region_index);
		iteration_dirty = true;
	}
}

void NavMap::add_link(NavLink *p_link) {
	links.push_back(p_link);
	iteration_dirty = true;
}

void NavMap::remove_link(NavLink *p_link) {
	int64_t link_index = links.find(p_link);
	if (link_index >= 0) {
		links.remove_at_unordered(link_index);
		iteration_dirty = true;
	}
}

bool NavMap::has_agent(NavAgent *agent) const {
	return agents.has(agent);
}

void NavMap::add_agent(NavAgent *agent) {
	if (!has_agent(agent)) {
		agents.push_back(agent);
		agents_dirty = true;
	}
}

void NavMap::remove_agent(NavAgent *agent) {
	remove_agent_as_controlled(agent);
	int64_t agent_index = agents.find(agent);
	if (agent_index >= 0) {
		agents.remove_at_unordered(agent_index);
		agents_dirty = true;
	}
}

bool NavMap::has_obstacle(NavObstacle *obstacle) const {
	return obstacles.has(obstacle);
}

void NavMap::add_obstacle(NavObstacle *obstacle) {
	if (obstacle->get_paused()) {
		// No point in adding a paused obstacle, it will add itself when unpaused again.
		return;
	}

	if (!has_obstacle(obstacle)) {
		obstacles.push_back(obstacle);
		obstacles_dirty = true;
	}
}

void NavMap::remove_obstacle(NavObstacle *obstacle) {
	int64_t obstacle_index = obstacles.find(obstacle);
	if (obstacle_index >= 0) {
		obstacles.remove_at_unordered(obstacle_index);
		obstacles_dirty = true;
	}
}

void NavMap::set_agent_as_controlled(NavAgent *agent) {
	remove_agent_as_controlled(agent);

	if (agent->get_paused()) {
		// No point in adding a paused agent, it will add itself when unpaused again.
		return;
	}

	if (agent->get_use_3d_avoidance()) {
		int64_t agent_3d_index = active_3d_avoidance_agents.find(agent);
		if (agent_3d_index < 0) {
			active_3d_avoidance_agents.push_back(agent);
			agents_dirty = true;
		}
	} else {
		int64_t agent_2d_index = active_2d_avoidance_agents.find(agent);
		if (agent_2d_index < 0) {
			active_2d_avoidance_agents.push_back(agent);
			agents_dirty = true;
		}
	}
}

void NavMap::remove_agent_as_controlled(NavAgent *agent) {
	int64_t agent_3d_index = active_3d_avoidance_agents.find(agent);
	if (agent_3d_index >= 0) {
		active_3d_avoidance_agents.remove_at_unordered(agent_3d_index);
		agents_dirty = true;
	}
	int64_t agent_2d_index = active_2d_avoidance_agents.find(agent);
	if (agent_2d_index >= 0) {
		active_2d_avoidance_agents.remove_at_unordered(agent_2d_index);
		agents_dirty = true;
	}
}

Vector3 NavMap::get_random_point(uint32_t p_navigation_layers, bool p_uniformly) const {
	RWLockRead read_lock(map_rwlock);

	const LocalVector<NavRegion *> map_regions = get_regions();

	if (map_regions.is_empty()) {
		return Vector3();
	}

	LocalVector<const NavRegion *> accessible_regions;

	for (const NavRegion *region : map_regions) {
		if (!region->get_enabled() || (p_navigation_layers & region->get_navigation_layers()) == 0) {
			continue;
		}
		accessible_regions.push_back(region);
	}

	if (accessible_regions.is_empty()) {
		// All existing region polygons are disabled.
		return Vector3();
	}

	if (p_uniformly) {
		real_t accumulated_region_surface_area = 0;
		RBMap<real_t, uint32_t> accessible_regions_area_map;

		for (uint32_t accessible_region_index = 0; accessible_region_index < accessible_regions.size(); accessible_region_index++) {
			const NavRegion *region = accessible_regions[accessible_region_index];

			real_t region_surface_area = region->get_surface_area();

			if (region_surface_area == 0.0f) {
				continue;
			}

			accessible_regions_area_map[accumulated_region_surface_area] = accessible_region_index;
			accumulated_region_surface_area += region_surface_area;
		}
		if (accessible_regions_area_map.is_empty() || accumulated_region_surface_area == 0) {
			// All faces have no real surface / no area.
			return Vector3();
		}

		real_t random_accessible_regions_area_map = Math::random(real_t(0), accumulated_region_surface_area);

		RBMap<real_t, uint32_t>::Iterator E = accessible_regions_area_map.find_closest(random_accessible_regions_area_map);
		ERR_FAIL_COND_V(!E, Vector3());
		uint32_t random_region_index = E->value;
		ERR_FAIL_UNSIGNED_INDEX_V(random_region_index, accessible_regions.size(), Vector3());

		const NavRegion *random_region = accessible_regions[random_region_index];
		ERR_FAIL_NULL_V(random_region, Vector3());

		return random_region->get_random_point(p_navigation_layers, p_uniformly);

	} else {
		uint32_t random_region_index = Math::random(int(0), accessible_regions.size() - 1);

		const NavRegion *random_region = accessible_regions[random_region_index];
		ERR_FAIL_NULL_V(random_region, Vector3());

		return random_region->get_random_point(p_navigation_layers, p_uniformly);
	}
}

void NavMap::sync() {
	RWLockWrite write_lock(map_rwlock);

	performance_data.pm_region_count = regions.size();
	performance_data.pm_agent_count = agents.size();
	performance_data.pm_link_count = links.size();
	performance_data.pm_obstacle_count = obstacles.size();

	_sync_dirty_map_update_requests();

	if (iteration_dirty) {
		performance_data.pm_polygon_count = 0;
		performance_data.pm_edge_count = 0;
		performance_data.pm_edge_merge_count = 0;
		performance_data.pm_edge_connection_count = 0;
		performance_data.pm_edge_free_count = 0;

		// Remove regions connections.
		region_external_connections.clear();
		for (NavRegion *region : regions) {
			region_external_connections[region] = LocalVector<gd::Edge::Connection>();
		}

		// Resize the polygon count.
		int polygon_count = 0;
		for (const NavRegion *region : regions) {
			if (!region->get_enabled()) {
				continue;
			}
			polygon_count += region->get_polygons().size();
		}
		polygons.resize(polygon_count);

		// Copy all region polygons in the map.
		polygon_count = 0;
		for (const NavRegion *region : regions) {
			if (!region->get_enabled()) {
				continue;
			}
			const LocalVector<gd::Polygon> &polygons_source = region->get_polygons();
			for (uint32_t n = 0; n < polygons_source.size(); n++) {
				polygons[polygon_count] = polygons_source[n];
				polygons[polygon_count].id = polygon_count;
				polygon_count++;
			}
		}

		performance_data.pm_polygon_count = polygon_count;

		// Group all edges per key.
		connection_pairs_map.clear();
		connection_pairs_map.reserve(polygons.size());
		int free_edges_count = 0; // How many ConnectionPairs have only one Connection.

		for (gd::Polygon &poly : polygons) {
			for (uint32_t p = 0; p < poly.points.size(); p++) {
				const int next_point = (p + 1) % poly.points.size();
				const gd::EdgeKey ek(poly.points[p].key, poly.points[next_point].key);

				HashMap<gd::EdgeKey, ConnectionPair, gd::EdgeKey>::Iterator pair_it = connection_pairs_map.find(ek);
				if (!pair_it) {
					pair_it = connection_pairs_map.insert(ek, ConnectionPair());
					performance_data.pm_edge_count += 1;
					++free_edges_count;
				}
				ConnectionPair &pair = pair_it->value;
				if (pair.size < 2) {
					// Add the polygon/edge tuple to this key.
					gd::Edge::Connection new_connection;
					new_connection.polygon = &poly;
					new_connection.edge = p;
					new_connection.pathway_start = poly.points[p].pos;
					new_connection.pathway_end = poly.points[next_point].pos;

					pair.connections[pair.size] = new_connection;
					++pair.size;
					if (pair.size == 2) {
						--free_edges_count;
					}

				} else {
					// The edge is already connected with another edge, skip.
					ERR_PRINT_ONCE("Navigation map synchronization error. Attempted to merge a navigation mesh polygon edge with another already-merged edge. This is usually caused by crossing edges, overlapping polygons, or a mismatch of the NavigationMesh / NavigationPolygon baked 'cell_size' and navigation map 'cell_size'. If you're certain none of above is the case, change 'navigation/3d/merge_rasterizer_cell_scale' to 0.001.");
				}
			}
		}

		free_edges.clear();
		free_edges.reserve(free_edges_count);

		for (const KeyValue<gd::EdgeKey, ConnectionPair> &pair_it : connection_pairs_map) {
			const ConnectionPair &pair = pair_it.value;
			if (pair.size == 2) {
				// Connect edge that are shared in different polygons.
				const gd::Edge::Connection &c1 = pair.connections[0];
				const gd::Edge::Connection &c2 = pair.connections[1];
				c1.polygon->edges[c1.edge].connections.push_back(c2);
				c2.polygon->edges[c2.edge].connections.push_back(c1);
				// Note: The pathway_start/end are full for those connection and do not need to be modified.
				performance_data.pm_edge_merge_count += 1;
			} else {
				CRASH_COND_MSG(pair.size != 1, vformat("Number of connection != 1. Found: %d", pair.size));
				if (use_edge_connections && pair.connections[0].polygon->owner->get_use_edge_connections()) {
					free_edges.push_back(pair.connections[0]);
				}
			}
		}

		// Find the compatible near edges.
		//
		// Note:
		// Considering that the edges must be compatible (for obvious reasons)
		// to be connected, create new polygons to remove that small gap is
		// not really useful and would result in wasteful computation during
		// connection, integration and path finding.
		performance_data.pm_edge_free_count = free_edges.size();

		const real_t edge_connection_margin_squared = edge_connection_margin * edge_connection_margin;

		for (uint32_t i = 0; i < free_edges.size(); i++) {
			const gd::Edge::Connection &free_edge = free_edges[i];
			Vector3 edge_p1 = free_edge.polygon->points[free_edge.edge].pos;
			Vector3 edge_p2 = free_edge.polygon->points[(free_edge.edge + 1) % free_edge.polygon->points.size()].pos;

			for (uint32_t j = 0; j < free_edges.size(); j++) {
				const gd::Edge::Connection &other_edge = free_edges[j];
				if (i == j || free_edge.polygon->owner == other_edge.polygon->owner) {
					continue;
				}

				Vector3 other_edge_p1 = other_edge.polygon->points[other_edge.edge].pos;
				Vector3 other_edge_p2 = other_edge.polygon->points[(other_edge.edge + 1) % other_edge.polygon->points.size()].pos;

				// Compute the projection of the opposite edge on the current one
				Vector3 edge_vector = edge_p2 - edge_p1;
				real_t projected_p1_ratio = edge_vector.dot(other_edge_p1 - edge_p1) / (edge_vector.length_squared());
				real_t projected_p2_ratio = edge_vector.dot(other_edge_p2 - edge_p1) / (edge_vector.length_squared());
				if ((projected_p1_ratio < 0.0 && projected_p2_ratio < 0.0) || (projected_p1_ratio > 1.0 && projected_p2_ratio > 1.0)) {
					continue;
				}

				// Check if the two edges are close to each other enough and compute a pathway between the two regions.
				Vector3 self1 = edge_vector * CLAMP(projected_p1_ratio, 0.0, 1.0) + edge_p1;
				Vector3 other1;
				if (projected_p1_ratio >= 0.0 && projected_p1_ratio <= 1.0) {
					other1 = other_edge_p1;
				} else {
					other1 = other_edge_p1.lerp(other_edge_p2, (1.0 - projected_p1_ratio) / (projected_p2_ratio - projected_p1_ratio));
				}
				if (other1.distance_squared_to(self1) > edge_connection_margin_squared) {
					continue;
				}

				Vector3 self2 = edge_vector * CLAMP(projected_p2_ratio, 0.0, 1.0) + edge_p1;
				Vector3 other2;
				if (projected_p2_ratio >= 0.0 && projected_p2_ratio <= 1.0) {
					other2 = other_edge_p2;
				} else {
					other2 = other_edge_p1.lerp(other_edge_p2, (0.0 - projected_p1_ratio) / (projected_p2_ratio - projected_p1_ratio));
				}
				if (other2.distance_squared_to(self2) > edge_connection_margin_squared) {
					continue;
				}

				// The edges can now be connected.
				gd::Edge::Connection new_connection = other_edge;
				new_connection.pathway_start = (self1 + other1) / 2.0;
				new_connection.pathway_end = (self2 + other2) / 2.0;
				free_edge.polygon->edges[free_edge.edge].connections.push_back(new_connection);

				// Add the connection to the region_connection map.
				region_external_connections[(NavRegion *)free_edge.polygon->owner].push_back(new_connection);
				performance_data.pm_edge_connection_count += 1;
			}
		}

		uint32_t link_poly_idx = 0;
		link_polygons.resize(links.size());

		// Search for polygons within range of a nav link.
		for (const NavLink *link : links) {
			if (!link->get_enabled()) {
				continue;
			}
			const Vector3 start = link->get_start_position();
			const Vector3 end = link->get_end_position();

			gd::Polygon *closest_start_polygon = nullptr;
			real_t closest_start_sqr_dist = link_connection_radius * link_connection_radius;
			Vector3 closest_start_point;

			gd::Polygon *closest_end_polygon = nullptr;
			real_t closest_end_sqr_dist = link_connection_radius * link_connection_radius;
			Vector3 closest_end_point;

			// Create link to any polygons within the search radius of the start point.
			for (uint32_t start_index = 0; start_index < polygons.size(); start_index++) {
				gd::Polygon &start_poly = polygons[start_index];

				// For each face check the distance to the start
				for (uint32_t start_point_id = 2; start_point_id < start_poly.points.size(); start_point_id += 1) {
					const Face3 start_face(start_poly.points[0].pos, start_poly.points[start_point_id - 1].pos, start_poly.points[start_point_id].pos);
					const Vector3 start_point = start_face.get_closest_point_to(start);
					const real_t sqr_dist = start_point.distance_squared_to(start);

					// Pick the polygon that is within our radius and is closer than anything we've seen yet.
					if (sqr_dist < closest_start_sqr_dist) {
						closest_start_sqr_dist = sqr_dist;
						closest_start_point = start_point;
						closest_start_polygon = &start_poly;
					}
				}
			}

			// Find any polygons within the search radius of the end point.
			for (gd::Polygon &end_poly : polygons) {
				// For each face check the distance to the end
				for (uint32_t end_point_id = 2; end_point_id < end_poly.points.size(); end_point_id += 1) {
					const Face3 end_face(end_poly.points[0].pos, end_poly.points[end_point_id - 1].pos, end_poly.points[end_point_id].pos);
					const Vector3 end_point = end_face.get_closest_point_to(end);
					const real_t sqr_dist = end_point.distance_squared_to(end);

					// Pick the polygon that is within our radius and is closer than anything we've seen yet.
					if (sqr_dist < closest_end_sqr_dist) {
						closest_end_sqr_dist = sqr_dist;
						closest_end_point = end_point;
						closest_end_polygon = &end_poly;
					}
				}
			}

			// If we have both a start and end point, then create a synthetic polygon to route through.
			if (closest_start_polygon && closest_end_polygon) {
				gd::Polygon &new_polygon = link_polygons[link_poly_idx++];
				new_polygon.id = polygon_count++;
				new_polygon.owner = link;

				new_polygon.edges.clear();
				new_polygon.edges.resize(4);
				new_polygon.points.clear();
				new_polygon.points.reserve(4);

				// Build a set of vertices that create a thin polygon going from the start to the end point.
				new_polygon.points.push_back({ closest_start_point, get_point_key(closest_start_point) });
				new_polygon.points.push_back({ closest_start_point, get_point_key(closest_start_point) });
				new_polygon.points.push_back({ closest_end_point, get_point_key(closest_end_point) });
				new_polygon.points.push_back({ closest_end_point, get_point_key(closest_end_point) });

				// Setup connections to go forward in the link.
				{
					gd::Edge::Connection entry_connection;
					entry_connection.polygon = &new_polygon;
					entry_connection.edge = -1;
					entry_connection.pathway_start = new_polygon.points[0].pos;
					entry_connection.pathway_end = new_polygon.points[1].pos;
					closest_start_polygon->edges[0].connections.push_back(entry_connection);

					gd::Edge::Connection exit_connection;
					exit_connection.polygon = closest_end_polygon;
					exit_connection.edge = -1;
					exit_connection.pathway_start = new_polygon.points[2].pos;
					exit_connection.pathway_end = new_polygon.points[3].pos;
					new_polygon.edges[2].connections.push_back(exit_connection);
				}

				// If the link is bi-directional, create connections from the end to the start.
				if (link->is_bidirectional()) {
					gd::Edge::Connection entry_connection;
					entry_connection.polygon = &new_polygon;
					entry_connection.edge = -1;
					entry_connection.pathway_start = new_polygon.points[2].pos;
					entry_connection.pathway_end = new_polygon.points[3].pos;
					closest_end_polygon->edges[0].connections.push_back(entry_connection);

					gd::Edge::Connection exit_connection;
					exit_connection.polygon = closest_start_polygon;
					exit_connection.edge = -1;
					exit_connection.pathway_start = new_polygon.points[0].pos;
					exit_connection.pathway_end = new_polygon.points[1].pos;
					new_polygon.edges[0].connections.push_back(exit_connection);
				}
			}
		}

		// Some code treats 0 as a failure case, so we avoid returning 0 and modulo wrap UINT32_MAX manually.
		iteration_id = iteration_id % UINT32_MAX + 1;

		path_query_slots_mutex.lock();
		for (NavMeshQueries3D::PathQuerySlot &p_path_query_slot : path_query_slots) {
			p_path_query_slot.path_corridor.clear();
			p_path_query_slot.path_corridor.resize(polygons.size() + link_polygons.size());
			p_path_query_slot.traversable_polys.clear();
			p_path_query_slot.traversable_polys.reserve(polygons.size() * 0.25);
		}
		path_query_slots_mutex.unlock();
	}

	map_settings_dirty = false;
	iteration_dirty = false;

	_sync_avoidance();
}

void NavMap::_sync_avoidance() {
	_sync_dirty_avoidance_update_requests();

	if (obstacles_dirty || agents_dirty) {
		_update_rvo_simulation();
	}

	obstacles_dirty = false;
	agents_dirty = false;
}

void NavMap::_update_rvo_obstacles_tree_2d() {
	int obstacle_vertex_count = 0;
	for (NavObstacle *obstacle : obstacles) {
		obstacle_vertex_count += obstacle->get_vertices().size();
	}

	// Cleaning old obstacles.
	for (size_t i = 0; i < rvo_simulation_2d.obstacles_.size(); ++i) {
		delete rvo_simulation_2d.obstacles_[i];
	}
	rvo_simulation_2d.obstacles_.clear();

	// Cannot use LocalVector here as RVO library expects std::vector to build KdTree
	std::vector<RVO2D::Obstacle2D *> &raw_obstacles = rvo_simulation_2d.obstacles_;
	raw_obstacles.reserve(obstacle_vertex_count);

	// The following block is modified copy from RVO2D::AddObstacle()
	// Obstacles are linked and depend on all other obstacles.
	for (NavObstacle *obstacle : obstacles) {
		const Vector3 &_obstacle_position = obstacle->get_position();
		const Vector<Vector3> &_obstacle_vertices = obstacle->get_vertices();

		if (_obstacle_vertices.size() < 2) {
			continue;
		}

		std::vector<RVO2D::Vector2> rvo_2d_vertices;
		rvo_2d_vertices.reserve(_obstacle_vertices.size());

		uint32_t _obstacle_avoidance_layers = obstacle->get_avoidance_layers();
		real_t _obstacle_height = obstacle->get_height();

		for (const Vector3 &_obstacle_vertex : _obstacle_vertices) {
#ifdef TOOLS_ENABLED
			if (_obstacle_vertex.y != 0) {
				WARN_PRINT_ONCE("Y coordinates of static obstacle vertices are ignored. Please use obstacle position Y to change elevation of obstacle.");
			}
#endif
			rvo_2d_vertices.push_back(RVO2D::Vector2(_obstacle_vertex.x + _obstacle_position.x, _obstacle_vertex.z + _obstacle_position.z));
		}

		const size_t obstacleNo = raw_obstacles.size();

		for (size_t i = 0; i < rvo_2d_vertices.size(); i++) {
			RVO2D::Obstacle2D *rvo_2d_obstacle = new RVO2D::Obstacle2D();
			rvo_2d_obstacle->point_ = rvo_2d_vertices[i];
			rvo_2d_obstacle->height_ = _obstacle_height;
			rvo_2d_obstacle->elevation_ = _obstacle_position.y;

			rvo_2d_obstacle->avoidance_layers_ = _obstacle_avoidance_layers;

			if (i != 0) {
				rvo_2d_obstacle->prevObstacle_ = raw_obstacles.back();
				rvo_2d_obstacle->prevObstacle_->nextObstacle_ = rvo_2d_obstacle;
			}

			if (i == rvo_2d_vertices.size() - 1) {
				rvo_2d_obstacle->nextObstacle_ = raw_obstacles[obstacleNo];
				rvo_2d_obstacle->nextObstacle_->prevObstacle_ = rvo_2d_obstacle;
			}

			rvo_2d_obstacle->unitDir_ = normalize(rvo_2d_vertices[(i == rvo_2d_vertices.size() - 1 ? 0 : i + 1)] - rvo_2d_vertices[i]);

			if (rvo_2d_vertices.size() == 2) {
				rvo_2d_obstacle->isConvex_ = true;
			} else {
				rvo_2d_obstacle->isConvex_ = (leftOf(rvo_2d_vertices[(i == 0 ? rvo_2d_vertices.size() - 1 : i - 1)], rvo_2d_vertices[i], rvo_2d_vertices[(i == rvo_2d_vertices.size() - 1 ? 0 : i + 1)]) >= 0.0f);
			}

			rvo_2d_obstacle->id_ = raw_obstacles.size();

			raw_obstacles.push_back(rvo_2d_obstacle);
		}
	}

	rvo_simulation_2d.kdTree_->buildObstacleTree(raw_obstacles);
}

void NavMap::_update_rvo_agents_tree_2d() {
	// Cannot use LocalVector here as RVO library expects std::vector to build KdTree.
	std::vector<RVO2D::Agent2D *> raw_agents;
	raw_agents.reserve(active_2d_avoidance_agents.size());
	for (NavAgent *agent : active_2d_avoidance_agents) {
		raw_agents.push_back(agent->get_rvo_agent_2d());
	}
	rvo_simulation_2d.kdTree_->buildAgentTree(raw_agents);
}

void NavMap::_update_rvo_agents_tree_3d() {
	// Cannot use LocalVector here as RVO library expects std::vector to build KdTree.
	std::vector<RVO3D::Agent3D *> raw_agents;
	raw_agents.reserve(active_3d_avoidance_agents.size());
	for (NavAgent *agent : active_3d_avoidance_agents) {
		raw_agents.push_back(agent->get_rvo_agent_3d());
	}
	rvo_simulation_3d.kdTree_->buildAgentTree(raw_agents);
}

void NavMap::_update_rvo_simulation() {
	if (obstacles_dirty) {
		_update_rvo_obstacles_tree_2d();
	}
	if (agents_dirty) {
		_update_rvo_agents_tree_2d();
		_update_rvo_agents_tree_3d();
	}
}

void NavMap::compute_single_avoidance_step_2d(uint32_t index, NavAgent **agent) {
	(*(agent + index))->get_rvo_agent_2d()->computeNeighbors(&rvo_simulation_2d);
	(*(agent + index))->get_rvo_agent_2d()->computeNewVelocity(&rvo_simulation_2d);
	(*(agent + index))->get_rvo_agent_2d()->update(&rvo_simulation_2d);
	(*(agent + index))->update();
}

void NavMap::compute_single_avoidance_step_3d(uint32_t index, NavAgent **agent) {
	(*(agent + index))->get_rvo_agent_3d()->computeNeighbors(&rvo_simulation_3d);
	(*(agent + index))->get_rvo_agent_3d()->computeNewVelocity(&rvo_simulation_3d);
	(*(agent + index))->get_rvo_agent_3d()->update(&rvo_simulation_3d);
	(*(agent + index))->update();
}

void NavMap::step(real_t p_deltatime) {
	deltatime = p_deltatime;

	rvo_simulation_2d.setTimeStep(float(deltatime));
	rvo_simulation_3d.setTimeStep(float(deltatime));

	if (active_2d_avoidance_agents.size() > 0) {
		if (use_threads && avoidance_use_multiple_threads) {
			WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &NavMap::compute_single_avoidance_step_2d, active_2d_avoidance_agents.ptr(), active_2d_avoidance_agents.size(), -1, true, SNAME("RVOAvoidanceAgents2D"));
			WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);
		} else {
			for (NavAgent *agent : active_2d_avoidance_agents) {
				agent->get_rvo_agent_2d()->computeNeighbors(&rvo_simulation_2d);
				agent->get_rvo_agent_2d()->computeNewVelocity(&rvo_simulation_2d);
				agent->get_rvo_agent_2d()->update(&rvo_simulation_2d);
				agent->update();
			}
		}
	}

	if (active_3d_avoidance_agents.size() > 0) {
		if (use_threads && avoidance_use_multiple_threads) {
			WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &NavMap::compute_single_avoidance_step_3d, active_3d_avoidance_agents.ptr(), active_3d_avoidance_agents.size(), -1, true, SNAME("RVOAvoidanceAgents3D"));
			WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);
		} else {
			for (NavAgent *agent : active_3d_avoidance_agents) {
				agent->get_rvo_agent_3d()->computeNeighbors(&rvo_simulation_3d);
				agent->get_rvo_agent_3d()->computeNewVelocity(&rvo_simulation_3d);
				agent->get_rvo_agent_3d()->update(&rvo_simulation_3d);
				agent->update();
			}
		}
	}
}

void NavMap::dispatch_callbacks() {
	for (NavAgent *agent : active_2d_avoidance_agents) {
		agent->dispatch_avoidance_callback();
	}

	for (NavAgent *agent : active_3d_avoidance_agents) {
		agent->dispatch_avoidance_callback();
	}
}

void NavMap::_update_merge_rasterizer_cell_dimensions() {
	merge_rasterizer_cell_size = cell_size * merge_rasterizer_cell_scale;
	merge_rasterizer_cell_height = cell_height * merge_rasterizer_cell_scale;
}

int NavMap::get_region_connections_count(NavRegion *p_region) const {
	ERR_FAIL_NULL_V(p_region, 0);

	HashMap<NavRegion *, LocalVector<gd::Edge::Connection>>::ConstIterator found_connections = region_external_connections.find(p_region);
	if (found_connections) {
		return found_connections->value.size();
	}
	return 0;
}

Vector3 NavMap::get_region_connection_pathway_start(NavRegion *p_region, int p_connection_id) const {
	ERR_FAIL_NULL_V(p_region, Vector3());

	HashMap<NavRegion *, LocalVector<gd::Edge::Connection>>::ConstIterator found_connections = region_external_connections.find(p_region);
	if (found_connections) {
		ERR_FAIL_INDEX_V(p_connection_id, int(found_connections->value.size()), Vector3());
		return found_connections->value[p_connection_id].pathway_start;
	}

	return Vector3();
}

Vector3 NavMap::get_region_connection_pathway_end(NavRegion *p_region, int p_connection_id) const {
	ERR_FAIL_NULL_V(p_region, Vector3());

	HashMap<NavRegion *, LocalVector<gd::Edge::Connection>>::ConstIterator found_connections = region_external_connections.find(p_region);
	if (found_connections) {
		ERR_FAIL_INDEX_V(p_connection_id, int(found_connections->value.size()), Vector3());
		return found_connections->value[p_connection_id].pathway_end;
	}

	return Vector3();
}

void NavMap::add_region_sync_dirty_request(SelfList<NavRegion> *p_sync_request) {
	if (p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.regions.add(p_sync_request);
}

void NavMap::add_link_sync_dirty_request(SelfList<NavLink> *p_sync_request) {
	if (p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.links.add(p_sync_request);
}

void NavMap::add_agent_sync_dirty_request(SelfList<NavAgent> *p_sync_request) {
	if (p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.agents.add(p_sync_request);
}

void NavMap::add_obstacle_sync_dirty_request(SelfList<NavObstacle> *p_sync_request) {
	if (p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.obstacles.add(p_sync_request);
}

void NavMap::remove_region_sync_dirty_request(SelfList<NavRegion> *p_sync_request) {
	if (!p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.regions.remove(p_sync_request);
}

void NavMap::remove_link_sync_dirty_request(SelfList<NavLink> *p_sync_request) {
	if (!p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.links.remove(p_sync_request);
}

void NavMap::remove_agent_sync_dirty_request(SelfList<NavAgent> *p_sync_request) {
	if (!p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.agents.remove(p_sync_request);
}

void NavMap::remove_obstacle_sync_dirty_request(SelfList<NavObstacle> *p_sync_request) {
	if (!p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.obstacles.remove(p_sync_request);
}

void NavMap::_sync_dirty_map_update_requests() {
	// If entire map settings changed make all regions dirty.
	if (map_settings_dirty) {
		for (NavRegion *region : regions) {
			region->scratch_polygons();
		}
		iteration_dirty = true;
	}

	if (!iteration_dirty) {
		iteration_dirty = sync_dirty_requests.regions.first() || sync_dirty_requests.links.first();
	}

	// Sync NavRegions.
	for (SelfList<NavRegion> *element = sync_dirty_requests.regions.first(); element; element = element->next()) {
		element->self()->sync();
	}
	sync_dirty_requests.regions.clear();

	// Sync NavLinks.
	for (SelfList<NavLink> *element = sync_dirty_requests.links.first(); element; element = element->next()) {
		element->self()->sync();
	}
	sync_dirty_requests.links.clear();
}

void NavMap::_sync_dirty_avoidance_update_requests() {
	// Sync NavAgents.
	if (!agents_dirty) {
		agents_dirty = sync_dirty_requests.agents.first();
	}
	for (SelfList<NavAgent> *element = sync_dirty_requests.agents.first(); element; element = element->next()) {
		element->self()->sync();
	}
	sync_dirty_requests.agents.clear();

	// Sync NavObstacles.
	if (!obstacles_dirty) {
		obstacles_dirty = sync_dirty_requests.obstacles.first();
	}
	for (SelfList<NavObstacle> *element = sync_dirty_requests.obstacles.first(); element; element = element->next()) {
		element->self()->sync();
	}
	sync_dirty_requests.obstacles.clear();
}

NavMap::NavMap() {
	avoidance_use_multiple_threads = GLOBAL_GET("navigation/avoidance/thread_model/avoidance_use_multiple_threads");
	avoidance_use_high_priority_threads = GLOBAL_GET("navigation/avoidance/thread_model/avoidance_use_high_priority_threads");

	path_query_slots_max = GLOBAL_GET("navigation/pathfinding/max_threads");

	int processor_count = OS::get_singleton()->get_processor_count();
	if (path_query_slots_max < 0) {
		path_query_slots_max = processor_count;
	}
	if (processor_count < path_query_slots_max) {
		path_query_slots_max = processor_count;
	}
	if (path_query_slots_max < 1) {
		path_query_slots_max = 1;
	}

	path_query_slots.resize(path_query_slots_max);
	for (uint32_t i = 0; i < path_query_slots.size(); i++) {
		path_query_slots[i].slot_index = i;
	}

	path_query_slots_semaphore.post(path_query_slots_max);
}

NavMap::~NavMap() {
}
