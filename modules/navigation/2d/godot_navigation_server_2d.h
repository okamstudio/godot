/**************************************************************************/
/*  godot_navigation_server_2d.h                                          */
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

#ifndef GODOT_NAVIGATION_SERVER_2D_H
#define GODOT_NAVIGATION_SERVER_2D_H

#include "../nav_agent.h"
#include "../nav_link.h"
#include "../nav_map.h"
#include "../nav_obstacle.h"
#include "../nav_region.h"

#include "servers/navigation_server_2d.h"

#ifdef CLIPPER2_ENABLED
class NavMeshGenerator2D;
#endif // CLIPPER2_ENABLED

// This server exposes the `NavigationServer3D` features in the 2D world.
class GodotNavigationServer2D : public NavigationServer2D {
	GDCLASS(GodotNavigationServer2D, NavigationServer2D);

#ifdef CLIPPER2_ENABLED
	NavMeshGenerator2D *navmesh_generator_2d = nullptr;
#endif // CLIPPER2_ENABLED

public:
	GodotNavigationServer2D();
	virtual ~GodotNavigationServer2D();

	virtual TypedArray<RID> get_maps() const override;

	virtual RID map_create() override;
	virtual void map_set_active(RID p_map, bool p_active) override;
	virtual bool map_is_active(RID p_map) const override;
	virtual void map_set_cell_size(RID p_map, real_t p_cell_size) override;
	virtual real_t map_get_cell_size(RID p_map) const override;
	virtual void map_set_use_edge_connections(RID p_map, bool p_enabled) override;
	virtual bool map_get_use_edge_connections(RID p_map) const override;
	virtual void map_set_edge_connection_margin(RID p_map, real_t p_connection_margin) override;
	virtual real_t map_get_edge_connection_margin(RID p_map) const override;
	virtual void map_set_link_connection_radius(RID p_map, real_t p_connection_radius) override;
	virtual real_t map_get_link_connection_radius(RID p_map) const override;
	virtual Vector<Vector2> map_get_path(RID p_map, Vector2 p_origin, Vector2 p_destination, bool p_optimize, uint32_t p_navigation_layers = 1) override;
	virtual Vector2 map_get_closest_point(RID p_map, const Vector2 &p_point) const override;
	virtual RID map_get_closest_point_owner(RID p_map, const Vector2 &p_point) const override;
	virtual TypedArray<RID> map_get_links(RID p_map) const override;
	virtual TypedArray<RID> map_get_regions(RID p_map) const override;
	virtual TypedArray<RID> map_get_agents(RID p_map) const override;
	virtual TypedArray<RID> map_get_obstacles(RID p_map) const override;
	virtual void map_force_update(RID p_map) override;
	virtual Vector2 map_get_random_point(RID p_map, uint32_t p_navigation_layers, bool p_uniformly) const override;
	virtual uint32_t map_get_iteration_id(RID p_map) const override;

	virtual RID region_create() override;
	virtual void region_set_enabled(RID p_region, bool p_enabled) override;
	virtual bool region_get_enabled(RID p_region) const override;
	virtual void region_set_use_edge_connections(RID p_region, bool p_enabled) override;
	virtual bool region_get_use_edge_connections(RID p_region) const override;
	virtual void region_set_enter_cost(RID p_region, real_t p_enter_cost) override;
	virtual real_t region_get_enter_cost(RID p_region) const override;
	virtual void region_set_travel_cost(RID p_region, real_t p_travel_cost) override;
	virtual real_t region_get_travel_cost(RID p_region) const override;
	virtual void region_set_owner_id(RID p_region, ObjectID p_owner_id) override;
	virtual ObjectID region_get_owner_id(RID p_region) const override;
	virtual bool region_owns_point(RID p_region, const Vector2 &p_point) const override;
	virtual void region_set_map(RID p_region, RID p_map) override;
	virtual RID region_get_map(RID p_region) const override;
	virtual void region_set_navigation_layers(RID p_region, uint32_t p_navigation_layers) override;
	virtual uint32_t region_get_navigation_layers(RID p_region) const override;
	virtual void region_set_transform(RID p_region, Transform2D p_transform) override;
	virtual Transform2D region_get_transform(RID p_region) const override;
	virtual void region_set_navigation_polygon(RID p_region, Ref<NavigationPolygon> p_navigation_polygon) override;
	virtual int region_get_connections_count(RID p_region) const override;
	virtual Vector2 region_get_connection_pathway_start(RID p_region, int p_connection_id) const override;
	virtual Vector2 region_get_connection_pathway_end(RID p_region, int p_connection_id) const override;
	virtual Vector2 region_get_closest_point(RID p_region, const Vector2 &p_point) const override;
	virtual Vector2 region_get_random_point(RID p_region, uint32_t p_navigation_layers, bool p_uniformly) const override;

	virtual RID link_create() override;

	/// Set the map of this link.
	virtual void link_set_map(RID p_link, RID p_map) override;
	virtual RID link_get_map(RID p_link) const override;

	virtual void link_set_enabled(RID p_link, bool p_enabled) override;
	virtual bool link_get_enabled(RID p_link) const override;

	/// Set whether this link travels in both directions.
	virtual void link_set_bidirectional(RID p_link, bool p_bidirectional) override;
	virtual bool link_is_bidirectional(RID p_link) const override;

	/// Set the link's layers.
	virtual void link_set_navigation_layers(RID p_link, uint32_t p_navigation_layers) override;
	virtual uint32_t link_get_navigation_layers(RID p_link) const override;

	/// Set the start position of the link.
	virtual void link_set_start_position(RID p_link, Vector2 p_position) override;
	virtual Vector2 link_get_start_position(RID p_link) const override;

	/// Set the end position of the link.
	virtual void link_set_end_position(RID p_link, Vector2 p_position) override;
	virtual Vector2 link_get_end_position(RID p_link) const override;

	/// Set the enter cost of the link.
	virtual void link_set_enter_cost(RID p_link, real_t p_enter_cost) override;
	virtual real_t link_get_enter_cost(RID p_link) const override;

	/// Set the travel cost of the link.
	virtual void link_set_travel_cost(RID p_link, real_t p_travel_cost) override;
	virtual real_t link_get_travel_cost(RID p_link) const override;

	/// Set the node which manages this link.
	virtual void link_set_owner_id(RID p_link, ObjectID p_owner_id) override;
	virtual ObjectID link_get_owner_id(RID p_link) const override;

	/// Creates the agent.
	virtual RID agent_create() override;

	/// Put the agent in the map.
	virtual void agent_set_map(RID p_agent, RID p_map) override;
	virtual RID agent_get_map(RID p_agent) const override;

	virtual void agent_set_paused(RID p_agent, bool p_paused) override;
	virtual bool agent_get_paused(RID p_agent) const override;

	virtual void agent_set_avoidance_enabled(RID p_agent, bool p_enabled) override;
	virtual bool agent_get_avoidance_enabled(RID p_agent) const override;

	/// The maximum distance (center point to
	/// center point) to other agents this agent
	/// takes into account in the navigation. The
	/// larger this number, the longer the running
	/// time of the simulation. If the number is too
	/// low, the simulation will not be safe.
	/// Must be non-negative.
	virtual void agent_set_neighbor_distance(RID p_agent, real_t p_distance) override;
	virtual real_t agent_get_neighbor_distance(RID p_agent) const override;

	/// The maximum number of other agents this
	/// agent takes into account in the navigation.
	/// The larger this number, the longer the
	/// running time of the simulation. If the
	/// number is too low, the simulation will not
	/// be safe.
	virtual void agent_set_max_neighbors(RID p_agent, int p_count) override;
	virtual int agent_get_max_neighbors(RID p_agent) const override;

	/// The minimal amount of time for which this
	/// agent's velocities that are computed by the
	/// simulation are safe with respect to other
	/// agents. The larger this number, the sooner
	/// this agent will respond to the presence of
	/// other agents, but the less freedom this
	/// agent has in choosing its velocities.
	/// Must be positive.
	virtual void agent_set_time_horizon_agents(RID p_agent, real_t p_time_horizon) override;
	virtual real_t agent_get_time_horizon_agents(RID p_agent) const override;
	virtual void agent_set_time_horizon_obstacles(RID p_agent, real_t p_time_horizon) override;
	virtual real_t agent_get_time_horizon_obstacles(RID p_agent) const override;

	/// The radius of this agent.
	/// Must be non-negative.
	virtual void agent_set_radius(RID p_agent, real_t p_radius) override;
	virtual real_t agent_get_radius(RID p_agent) const override;

	/// The maximum speed of this agent.
	/// Must be non-negative.
	virtual void agent_set_max_speed(RID p_agent, real_t p_max_speed) override;
	virtual real_t agent_get_max_speed(RID p_agent) const override;

	/// forces and agent velocity change in the avoidance simulation, adds simulation instability if done recklessly
	virtual void agent_set_velocity_forced(RID p_agent, Vector2 p_velocity) override;

	/// The wanted velocity for the agent as a "suggestion" to the avoidance simulation.
	/// The simulation will try to fulfill this velocity wish if possible but may change the velocity depending on other agent's and obstacles'.
	virtual void agent_set_velocity(RID p_agent, Vector2 p_velocity) override;
	virtual Vector2 agent_get_velocity(RID p_agent) const override;

	/// Position of the agent in world space.
	virtual void agent_set_position(RID p_agent, Vector2 p_position) override;
	virtual Vector2 agent_get_position(RID p_agent) const override;

	/// Returns true if the map got changed the previous frame.
	virtual bool agent_is_map_changed(RID p_agent) const override;

	/// Callback called at the end of the RVO process
	virtual void agent_set_avoidance_callback(RID p_agent, Callable p_callback) override;
	virtual bool agent_has_avoidance_callback(RID p_agent) const override;

	virtual void agent_set_avoidance_layers(RID p_agent, uint32_t p_layers) override;
	virtual uint32_t agent_get_avoidance_layers(RID p_agent) const override;

	virtual void agent_set_avoidance_mask(RID p_agent, uint32_t p_mask) override;
	virtual uint32_t agent_get_avoidance_mask(RID p_agent) const override;

	virtual void agent_set_avoidance_priority(RID p_agent, real_t p_priority) override;
	virtual real_t agent_get_avoidance_priority(RID p_agent) const override;

	virtual RID obstacle_create() override;
	virtual void obstacle_set_avoidance_enabled(RID p_obstacle, bool p_enabled) override;
	virtual bool obstacle_get_avoidance_enabled(RID p_obstacle) const override;
	virtual void obstacle_set_map(RID p_obstacle, RID p_map) override;
	virtual RID obstacle_get_map(RID p_obstacle) const override;
	virtual void obstacle_set_paused(RID p_obstacle, bool p_paused) override;
	virtual bool obstacle_get_paused(RID p_obstacle) const override;
	virtual void obstacle_set_radius(RID p_obstacle, real_t p_radius) override;
	virtual real_t obstacle_get_radius(RID p_obstacle) const override;
	virtual void obstacle_set_velocity(RID p_obstacle, Vector2 p_velocity) override;
	virtual Vector2 obstacle_get_velocity(RID p_obstacle) const override;
	virtual void obstacle_set_position(RID p_obstacle, Vector2 p_position) override;
	virtual Vector2 obstacle_get_position(RID p_obstacle) const override;
	virtual void obstacle_set_vertices(RID p_obstacle, const Vector<Vector2> &p_vertices) override;
	virtual Vector<Vector2> obstacle_get_vertices(RID p_obstacle) const override;
	virtual void obstacle_set_avoidance_layers(RID p_obstacle, uint32_t p_layers) override;
	virtual uint32_t obstacle_get_avoidance_layers(RID p_obstacle) const override;

	virtual void query_path(const Ref<NavigationPathQueryParameters2D> &p_query_parameters, Ref<NavigationPathQueryResult2D> p_query_result, const Callable &p_callback) override;

	virtual void init() override;
	virtual void sync() override;
	virtual void finish() override;
	virtual void free(RID p_object) override;

	virtual void parse_source_geometry_data(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, Node *p_root_node, const Callable &p_callback = Callable()) override;
	virtual void bake_from_source_geometry_data(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, const Callable &p_callback = Callable()) override;
	virtual void bake_from_source_geometry_data_async(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, const Callable &p_callback = Callable()) override;
	virtual bool is_baking_navigation_polygon(Ref<NavigationPolygon> p_navigation_polygon) const override;

	virtual RID source_geometry_parser_create() override;
	virtual void source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) override;

	virtual Vector<Vector2> simplify_path(const Vector<Vector2> &p_path, real_t p_epsilon) override;
};

#endif // GODOT_NAVIGATION_SERVER_2D_H
