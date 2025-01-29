/**************************************************************************/
/*  navigation_server_2d_dummy.h                                          */
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

#ifndef NAVIGATION_SERVER_2D_DUMMY_H
#define NAVIGATION_SERVER_2D_DUMMY_H

#include "servers/navigation_server_2d.h"

class NavigationServer2DDummy : public NavigationServer2D {
	GDCLASS(NavigationServer2DDummy, NavigationServer2D);

public:
	TypedArray<RID> get_maps() const override { return TypedArray<RID>(); }

	RID map_create() override { return RID(); }
	void map_set_active(RID p_map, bool p_active) override {}
	bool map_is_active(RID p_map) const override { return false; }
	void map_set_cell_size(RID p_map, real_t p_cell_size) override {}
	real_t map_get_cell_size(RID p_map) const override { return 0; }
	void map_set_use_edge_connections(RID p_map, bool p_enabled) override {}
	bool map_get_use_edge_connections(RID p_map) const override { return false; }
	void map_set_edge_connection_margin(RID p_map, real_t p_connection_margin) override {}
	real_t map_get_edge_connection_margin(RID p_map) const override { return 0; }
	void map_set_link_connection_radius(RID p_map, real_t p_connection_radius) override {}
	real_t map_get_link_connection_radius(RID p_map) const override { return 0; }
	Vector<Vector2> map_get_path(RID p_map, Vector2 p_origin, Vector2 p_destination, bool p_optimize, uint32_t p_navigation_layers = 1) override { return Vector<Vector2>(); }
	Vector2 map_get_closest_point(RID p_map, const Vector2 &p_point) const override { return Vector2(); }
	RID map_get_closest_point_owner(RID p_map, const Vector2 &p_point) const override { return RID(); }
	TypedArray<RID> map_get_links(RID p_map) const override { return TypedArray<RID>(); }
	TypedArray<RID> map_get_regions(RID p_map) const override { return TypedArray<RID>(); }
	TypedArray<RID> map_get_agents(RID p_map) const override { return TypedArray<RID>(); }
	TypedArray<RID> map_get_obstacles(RID p_map) const override { return TypedArray<RID>(); }
	void map_force_update(RID p_map) override {}
	Vector2 map_get_random_point(RID p_map, uint32_t p_naviation_layers, bool p_uniformly) const override { return Vector2(); }
	uint32_t map_get_iteration_id(RID p_map) const override { return 0; }
	void map_set_use_async_iterations(RID p_map, bool p_enabled) override {}
	bool map_get_use_async_iterations(RID p_map) const override { return false; }

	RID region_create() override { return RID(); }
	void region_set_enabled(RID p_region, bool p_enabled) override {}
	bool region_get_enabled(RID p_region) const override { return false; }
	void region_set_use_edge_connections(RID p_region, bool p_enabled) override {}
	bool region_get_use_edge_connections(RID p_region) const override { return false; }
	void region_set_enter_cost(RID p_region, real_t p_enter_cost) override {}
	real_t region_get_enter_cost(RID p_region) const override { return 0; }
	void region_set_travel_cost(RID p_region, real_t p_travel_cost) override {}
	real_t region_get_travel_cost(RID p_region) const override { return 0; }
	void region_set_owner_id(RID p_region, ObjectID p_owner_id) override {}
	ObjectID region_get_owner_id(RID p_region) const override { return ObjectID(); }
	bool region_owns_point(RID p_region, const Vector2 &p_point) const override { return false; }
	void region_set_map(RID p_region, RID p_map) override {}
	RID region_get_map(RID p_region) const override { return RID(); }
	void region_set_navigation_layers(RID p_region, uint32_t p_navigation_layers) override {}
	uint32_t region_get_navigation_layers(RID p_region) const override { return 0; }
	void region_set_transform(RID p_region, Transform2D p_transform) override {}
	Transform2D region_get_transform(RID p_region) const override { return Transform2D(); }
	void region_set_navigation_polygon(RID p_region, Ref<NavigationPolygon> p_navigation_polygon) override {}
	int region_get_connections_count(RID p_region) const override { return 0; }
	Vector2 region_get_connection_pathway_start(RID p_region, int p_connection_id) const override { return Vector2(); }
	Vector2 region_get_connection_pathway_end(RID p_region, int p_connection_id) const override { return Vector2(); }
	Vector2 region_get_closest_point(RID p_region, const Vector2 &p_point) const override { return Vector2(); }
	Vector2 region_get_random_point(RID p_region, uint32_t p_navigation_layers, bool p_uniformly) const override { return Vector2(); }
	Rect2 region_get_bounds(RID p_region) const override { return Rect2(); }

	RID link_create() override { return RID(); }
	void link_set_map(RID p_link, RID p_map) override {}
	RID link_get_map(RID p_link) const override { return RID(); }
	void link_set_enabled(RID p_link, bool p_enabled) override {}
	bool link_get_enabled(RID p_link) const override { return false; }
	void link_set_bidirectional(RID p_link, bool p_bidirectional) override {}
	bool link_is_bidirectional(RID p_link) const override { return false; }
	void link_set_navigation_layers(RID p_link, uint32_t p_navigation_layers) override {}
	uint32_t link_get_navigation_layers(RID p_link) const override { return 0; }
	void link_set_start_position(RID p_link, Vector2 p_position) override {}
	Vector2 link_get_start_position(RID p_link) const override { return Vector2(); }
	void link_set_end_position(RID p_link, Vector2 p_position) override {}
	Vector2 link_get_end_position(RID p_link) const override { return Vector2(); }
	void link_set_enter_cost(RID p_link, real_t p_enter_cost) override {}
	real_t link_get_enter_cost(RID p_link) const override { return 0; }
	void link_set_travel_cost(RID p_link, real_t p_travel_cost) override {}
	real_t link_get_travel_cost(RID p_link) const override { return 0; }
	void link_set_owner_id(RID p_link, ObjectID p_owner_id) override {}
	ObjectID link_get_owner_id(RID p_link) const override { return ObjectID(); }

	RID agent_create() override { return RID(); }
	void agent_set_map(RID p_agent, RID p_map) override {}
	RID agent_get_map(RID p_agent) const override { return RID(); }
	void agent_set_paused(RID p_agent, bool p_paused) override {}
	bool agent_get_paused(RID p_agent) const override { return false; }
	void agent_set_avoidance_enabled(RID p_agent, bool p_enabled) override {}
	bool agent_get_avoidance_enabled(RID p_agent) const override { return false; }
	void agent_set_neighbor_distance(RID p_agent, real_t p_distance) override {}
	real_t agent_get_neighbor_distance(RID p_agent) const override { return 0; }
	void agent_set_max_neighbors(RID p_agent, int p_count) override {}
	int agent_get_max_neighbors(RID p_agent) const override { return 0; }
	void agent_set_time_horizon_agents(RID p_agent, real_t p_time_horizon) override {}
	real_t agent_get_time_horizon_agents(RID p_agent) const override { return 0; }
	void agent_set_time_horizon_obstacles(RID p_agent, real_t p_time_horizon) override {}
	real_t agent_get_time_horizon_obstacles(RID p_agent) const override { return 0; }
	void agent_set_radius(RID p_agent, real_t p_radius) override {}
	real_t agent_get_radius(RID p_agent) const override { return 0; }
	void agent_set_max_speed(RID p_agent, real_t p_max_speed) override {}
	real_t agent_get_max_speed(RID p_agent) const override { return 0; }
	void agent_set_velocity_forced(RID p_agent, Vector2 p_velocity) override {}
	void agent_set_velocity(RID p_agent, Vector2 p_velocity) override {}
	Vector2 agent_get_velocity(RID p_agent) const override { return Vector2(); }
	void agent_set_position(RID p_agent, Vector2 p_position) override {}
	Vector2 agent_get_position(RID p_agent) const override { return Vector2(); }
	bool agent_is_map_changed(RID p_agent) const override { return false; }
	void agent_set_avoidance_callback(RID p_agent, Callable p_callback) override {}
	bool agent_has_avoidance_callback(RID p_agent) const override { return false; }
	void agent_set_avoidance_layers(RID p_agent, uint32_t p_layers) override {}
	uint32_t agent_get_avoidance_layers(RID p_agent) const override { return 0; }
	void agent_set_avoidance_mask(RID p_agent, uint32_t p_mask) override {}
	uint32_t agent_get_avoidance_mask(RID p_agent) const override { return 0; }
	void agent_set_avoidance_priority(RID p_agent, real_t p_priority) override {}
	real_t agent_get_avoidance_priority(RID p_agent) const override { return 0; }

	RID obstacle_create() override { return RID(); }
	void obstacle_set_avoidance_enabled(RID p_obstacle, bool p_enabled) override {}
	bool obstacle_get_avoidance_enabled(RID p_obstacle) const override { return false; }
	void obstacle_set_map(RID p_obstacle, RID p_map) override {}
	RID obstacle_get_map(RID p_obstacle) const override { return RID(); }
	void obstacle_set_paused(RID p_obstacle, bool p_paused) override {}
	bool obstacle_get_paused(RID p_obstacle) const override { return false; }
	void obstacle_set_radius(RID p_obstacle, real_t p_radius) override {}
	real_t obstacle_get_radius(RID p_agent) const override { return 0; }
	void obstacle_set_velocity(RID p_obstacle, Vector2 p_velocity) override {}
	Vector2 obstacle_get_velocity(RID p_agent) const override { return Vector2(); }
	void obstacle_set_position(RID p_obstacle, Vector2 p_position) override {}
	Vector2 obstacle_get_position(RID p_agent) const override { return Vector2(); }
	void obstacle_set_vertices(RID p_obstacle, const Vector<Vector2> &p_vertices) override {}
	Vector<Vector2> obstacle_get_vertices(RID p_agent) const override { return Vector<Vector2>(); }
	void obstacle_set_avoidance_layers(RID p_obstacle, uint32_t p_layers) override {}
	uint32_t obstacle_get_avoidance_layers(RID p_agent) const override { return 0; }

	void query_path(const Ref<NavigationPathQueryParameters2D> &p_query_parameters, Ref<NavigationPathQueryResult2D> p_query_result, const Callable &p_callback = Callable()) override {}

	void init() override {}
	void sync() override {}
	void finish() override {}

	void free(RID p_object) override {}

	void parse_source_geometry_data(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, Node *p_root_node, const Callable &p_callback = Callable()) override {}
	void bake_from_source_geometry_data(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, const Callable &p_callback = Callable()) override {}
	void bake_from_source_geometry_data_async(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, const Callable &p_callback = Callable()) override {}
	bool is_baking_navigation_polygon(Ref<NavigationPolygon> p_navigation_polygon) const override { return false; }

	RID source_geometry_parser_create() override { return RID(); }
	void source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) override {}

	Vector<Vector2> simplify_path(const Vector<Vector2> &p_path, real_t p_epsilon) override { return Vector<Vector2>(); }

	void set_debug_enabled(bool p_enabled) {}
	bool get_debug_enabled() const { return false; }
};

#endif // NAVIGATION_SERVER_2D_DUMMY_H
