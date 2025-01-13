/**************************************************************************/
/*  nav_map_builder_3d.cpp                                                */
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

#include "nav_map_builder_3d.h"

#include "../nav_link.h"
#include "../nav_map.h"
#include "../nav_region.h"
#include "nav_map_iteration_3d.h"
#include "nav_region_iteration_3d.h"

gd::PointKey NavMapBuilder3D::get_point_key(const Vector3 &p_pos, const Vector3 &p_cell_size) {
	const int x = static_cast<int>(Math::floor(p_pos.x / p_cell_size.x));
	const int y = static_cast<int>(Math::floor(p_pos.y / p_cell_size.y));
	const int z = static_cast<int>(Math::floor(p_pos.z / p_cell_size.z));

	gd::PointKey p;
	p.key = 0;
	p.x = x;
	p.y = y;
	p.z = z;
	return p;
}

void NavMapBuilder3D::build_navmap_iteration(NavMapIterationBuild &r_build) {
	gd::PerformanceData &performance_data = r_build.performance_data;

	performance_data.pm_polygon_count = 0;
	performance_data.pm_edge_count = 0;
	performance_data.pm_edge_merge_count = 0;
	performance_data.pm_edge_connection_count = 0;
	performance_data.pm_edge_free_count = 0;

	_build_step_gather_region_polygons(r_build);

	_build_step_find_edge_connection_pairs(r_build);

	_build_step_merge_edge_connection_pairs(r_build);

	_build_step_edge_connection_margin_connections(r_build);

	_build_step_navlink_connections(r_build);

	_build_update_map_iteration(r_build);
}

void NavMapBuilder3D::_build_step_gather_region_polygons(NavMapIterationBuild &r_build) {
	gd::PerformanceData &performance_data = r_build.performance_data;
	NavMapIteration *map_iteration = r_build.map_iteration;

	LocalVector<NavRegionIteration> &regions = map_iteration->region_iterations;
	HashMap<uint32_t, LocalVector<gd::Edge::Connection>> &region_external_connections = map_iteration->external_region_connections;

	// Remove regions connections.
	region_external_connections.clear();
	for (const NavRegionIteration &region : regions) {
		region_external_connections[region.id] = LocalVector<gd::Edge::Connection>();
	}

	// Copy all region polygons in the map.
	int polygon_count = 0;
	for (NavRegionIteration &region : regions) {
		if (!region.get_enabled()) {
			continue;
		}
		LocalVector<gd::Polygon> &polygons_source = region.navmesh_polygons;
		for (uint32_t n = 0; n < polygons_source.size(); n++) {
			polygons_source[n].id = polygon_count;
			polygon_count++;
		}
	}

	performance_data.pm_polygon_count = polygon_count;
	r_build.polygon_count = polygon_count;
}

void NavMapBuilder3D::_build_step_find_edge_connection_pairs(NavMapIterationBuild &r_build) {
	gd::PerformanceData &performance_data = r_build.performance_data;
	NavMapIteration *map_iteration = r_build.map_iteration;
	int polygon_count = r_build.polygon_count;

	HashMap<gd::EdgeKey, gd::EdgeConnectionPair, gd::EdgeKey> &connection_pairs_map = r_build.iter_connection_pairs_map;

	// Group all edges per key.
	connection_pairs_map.clear();
	connection_pairs_map.reserve(polygon_count);
	int free_edges_count = 0; // How many ConnectionPairs have only one Connection.

	for (NavRegionIteration &region : map_iteration->region_iterations) {
		if (!region.get_enabled()) {
			continue;
		}

		for (gd::Polygon &poly : region.navmesh_polygons) {
			for (uint32_t p = 0; p < poly.points.size(); p++) {
				const int next_point = (p + 1) % poly.points.size();
				const gd::EdgeKey ek(poly.points[p].key, poly.points[next_point].key);

				HashMap<gd::EdgeKey, gd::EdgeConnectionPair, gd::EdgeKey>::Iterator pair_it = connection_pairs_map.find(ek);
				if (!pair_it) {
					pair_it = connection_pairs_map.insert(ek, gd::EdgeConnectionPair());
					performance_data.pm_edge_count += 1;
					++free_edges_count;
				}
				gd::EdgeConnectionPair &pair = pair_it->value;
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
	}

	r_build.free_edge_count = free_edges_count;
}

void NavMapBuilder3D::_build_step_merge_edge_connection_pairs(NavMapIterationBuild &r_build) {
	gd::PerformanceData &performance_data = r_build.performance_data;

	HashMap<gd::EdgeKey, gd::EdgeConnectionPair, gd::EdgeKey> &connection_pairs_map = r_build.iter_connection_pairs_map;
	LocalVector<gd::Edge::Connection> &free_edges = r_build.iter_free_edges;
	int free_edges_count = r_build.free_edge_count;
	bool use_edge_connections = r_build.use_edge_connections;

	free_edges.clear();
	free_edges.reserve(free_edges_count);

	for (const KeyValue<gd::EdgeKey, gd::EdgeConnectionPair> &pair_it : connection_pairs_map) {
		const gd::EdgeConnectionPair &pair = pair_it.value;
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
}

void NavMapBuilder3D::_build_step_edge_connection_margin_connections(NavMapIterationBuild &r_build) {
	gd::PerformanceData &performance_data = r_build.performance_data;
	NavMapIteration *map_iteration = r_build.map_iteration;

	real_t edge_connection_margin = r_build.edge_connection_margin;
	LocalVector<gd::Edge::Connection> &free_edges = r_build.iter_free_edges;
	HashMap<uint32_t, LocalVector<gd::Edge::Connection>> &region_external_connections = map_iteration->external_region_connections;

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
			region_external_connections[(uint32_t)free_edge.polygon->owner->id].push_back(new_connection);
			performance_data.pm_edge_connection_count += 1;
		}
	}
}

void NavMapBuilder3D::_build_step_navlink_connections(NavMapIterationBuild &r_build) {
	NavMapIteration *map_iteration = r_build.map_iteration;

	real_t link_connection_radius = r_build.link_connection_radius;
	Vector3 merge_rasterizer_cell_size = r_build.merge_rasterizer_cell_size;

	LocalVector<gd::Polygon> &link_polygons = map_iteration->link_polygons;
	LocalVector<NavLinkIteration> &links = map_iteration->link_iterations;
	int polygon_count = r_build.polygon_count;

	real_t link_connection_radius_sqr = link_connection_radius * link_connection_radius;
	uint32_t link_poly_idx = 0;
	link_polygons.resize(links.size());

	// Search for polygons within range of a nav link.
	for (const NavLinkIteration &link : links) {
		if (!link.get_enabled()) {
			continue;
		}
		const Vector3 link_start_pos = link.get_start_position();
		const Vector3 link_end_pos = link.get_end_position();

		gd::Polygon *closest_start_polygon = nullptr;
		real_t closest_start_sqr_dist = link_connection_radius_sqr;
		Vector3 closest_start_point;

		gd::Polygon *closest_end_polygon = nullptr;
		real_t closest_end_sqr_dist = link_connection_radius_sqr;
		Vector3 closest_end_point;

		for (NavRegionIteration &region : map_iteration->region_iterations) {
			if (!region.get_enabled()) {
				continue;
			}
			AABB region_bounds = region.get_bounds().grow(link_connection_radius);
			if (!region_bounds.has_point(link_start_pos) && !region_bounds.has_point(link_end_pos)) {
				continue;
			}

			for (gd::Polygon &polyon : region.navmesh_polygons) {
				//for (gd::Polygon &polyon : polygons) {
				for (uint32_t point_id = 2; point_id < polyon.points.size(); point_id += 1) {
					const Face3 face(polyon.points[0].pos, polyon.points[point_id - 1].pos, polyon.points[point_id].pos);

					{
						const Vector3 start_point = face.get_closest_point_to(link_start_pos);
						const real_t sqr_dist = start_point.distance_squared_to(link_start_pos);

						// Pick the polygon that is within our radius and is closer than anything we've seen yet.
						if (sqr_dist < closest_start_sqr_dist) {
							closest_start_sqr_dist = sqr_dist;
							closest_start_point = start_point;
							closest_start_polygon = &polyon;
						}
					}

					{
						const Vector3 end_point = face.get_closest_point_to(link_end_pos);
						const real_t sqr_dist = end_point.distance_squared_to(link_end_pos);

						// Pick the polygon that is within our radius and is closer than anything we've seen yet.
						if (sqr_dist < closest_end_sqr_dist) {
							closest_end_sqr_dist = sqr_dist;
							closest_end_point = end_point;
							closest_end_polygon = &polyon;
						}
					}
				}
			}
		}

		// If we have both a start and end point, then create a synthetic polygon to route through.
		if (closest_start_polygon && closest_end_polygon) {
			gd::Polygon &new_polygon = link_polygons[link_poly_idx++];
			new_polygon.id = polygon_count++;
			new_polygon.owner = &link;

			new_polygon.edges.clear();
			new_polygon.edges.resize(4);
			new_polygon.points.resize(4);

			// Build a set of vertices that create a thin polygon going from the start to the end point.
			new_polygon.points[0] = { closest_start_point, get_point_key(closest_start_point, merge_rasterizer_cell_size) };
			new_polygon.points[1] = { closest_start_point, get_point_key(closest_start_point, merge_rasterizer_cell_size) };
			new_polygon.points[2] = { closest_end_point, get_point_key(closest_end_point, merge_rasterizer_cell_size) };
			new_polygon.points[3] = { closest_end_point, get_point_key(closest_end_point, merge_rasterizer_cell_size) };

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
			if (link.is_bidirectional()) {
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
}

void NavMapBuilder3D::_build_update_map_iteration(NavMapIterationBuild &r_build) {
	NavMapIteration *map_iteration = r_build.map_iteration;

	LocalVector<gd::Polygon> &link_polygons = map_iteration->link_polygons;

	map_iteration->navmesh_polygon_count = r_build.polygon_count;
	map_iteration->link_polygon_count = link_polygons.size();

	map_iteration->path_query_slots_mutex.lock();
	for (NavMeshQueries3D::PathQuerySlot &p_path_query_slot : map_iteration->path_query_slots) {
		p_path_query_slot.traversable_polys.clear();
		p_path_query_slot.traversable_polys.reserve(map_iteration->navmesh_polygon_count * 0.25);
		p_path_query_slot.path_corridor.clear();
		p_path_query_slot.path_corridor.resize(map_iteration->navmesh_polygon_count + map_iteration->link_polygon_count);
	}
	map_iteration->path_query_slots_mutex.unlock();
}

#endif // _3D_DISABLED
