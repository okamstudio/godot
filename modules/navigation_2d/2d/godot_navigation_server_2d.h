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

#include "../nav_agent_2d.h"
#include "../nav_link_2d.h"
#include "../nav_map_2d.h"
#include "../nav_obstacle_2d.h"
#include "../nav_region_2d.h"

#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "core/templates/rid_owner.h"
#include "servers/navigation/navigation_path_query_parameters_2d.h"
#include "servers/navigation/navigation_path_query_result_2d.h"
#include "servers/navigation_server_2d.h"

/// The commands are functions executed during the `sync` phase.

#define MERGE_INTERNAL(A, B) A##B
#define MERGE(A, B) MERGE_INTERNAL(A, B)

#define COMMAND_1(F_NAME, T_0, D_0)        \
	virtual void F_NAME(T_0 D_0) override; \
	void MERGE(_cmd_, F_NAME)(T_0 D_0)

#define COMMAND_2(F_NAME, T_0, D_0, T_1, D_1)       \
	virtual void F_NAME(T_0 D_0, T_1 D_1) override; \
	void MERGE(_cmd_, F_NAME)(T_0 D_0, T_1 D_1)

class GodotNavigationServer2D;
#ifdef CLIPPER2_ENABLED
class NavMeshGenerator2D;
#endif // CLIPPER2_ENABLED

struct SetCommand2D {
	virtual ~SetCommand2D() {}
	virtual void exec(GodotNavigationServer2D *p_server) = 0;
};

// This server exposes the `NavigationServer3D` features in the 2D world.
class GodotNavigationServer2D : public NavigationServer2D {
	GDCLASS(GodotNavigationServer2D, NavigationServer2D);

	Mutex commands_mutex;
	/// Mutex used to make any operation threadsafe.
	Mutex operations_mutex;

	LocalVector<SetCommand2D *> commands;

	mutable RID_Owner<NavLink2D> link_owner;
	mutable RID_Owner<NavMap2D> map_owner;
	mutable RID_Owner<NavRegion2D> region_owner;
	mutable RID_Owner<NavAgent2D> agent_owner;
	mutable RID_Owner<NavObstacle2D> obstacle_owner;

	bool active = true;
	LocalVector<NavMap2D *> active_maps;
	LocalVector<uint32_t> active_maps_iteration_id;

#ifdef CLIPPER2_ENABLED
	NavMeshGenerator2D *navmesh_generator_2d = nullptr;
#endif // CLIPPER2_ENABLED

	// Performance Monitor.
	int pm_region_count = 0;
	int pm_agent_count = 0;
	int pm_link_count = 0;
	int pm_polygon_count = 0;
	int pm_edge_count = 0;
	int pm_edge_merge_count = 0;
	int pm_edge_connection_count = 0;
	int pm_edge_free_count = 0;
	int pm_obstacle_count = 0;

public:
	GodotNavigationServer2D();
	virtual ~GodotNavigationServer2D();

	void add_command(SetCommand2D *p_command);

	virtual TypedArray<RID> get_maps() const override;

	virtual RID map_create() override;
	COMMAND_2(map_set_active, RID, p_map, bool, p_active);
	virtual bool map_is_active(RID p_map) const override;

	COMMAND_2(map_set_cell_size, RID, p_map, real_t, p_cell_size);
	virtual real_t map_get_cell_size(RID p_map) const override;

	COMMAND_2(map_set_use_edge_connections, RID, p_map, bool, p_enabled);
	virtual bool map_get_use_edge_connections(RID p_map) const override;

	COMMAND_2(map_set_edge_connection_margin, RID, p_map, real_t, p_connection_margin);
	virtual real_t map_get_edge_connection_margin(RID p_map) const override;

	COMMAND_2(map_set_link_connection_radius, RID, p_map, real_t, p_connection_radius);
	virtual real_t map_get_link_connection_radius(RID p_map) const override;

	virtual Vector<Vector2> map_get_path(RID p_map, Vector2 p_origin, Vector2 p_destination, bool p_optimize, uint32_t p_navigation_layers = 1) override;

	virtual Vector2 map_get_closest_point(RID p_map, const Vector2 &p_point) const override;

	virtual RID map_get_closest_point_owner(RID p_map, const Vector2 &p_point) const override;

	virtual TypedArray<RID> map_get_links(RID p_map) const override;
	virtual TypedArray<RID> map_get_regions(RID p_map) const override;
	virtual TypedArray<RID> map_get_agents(RID p_map) const override;
	virtual TypedArray<RID> map_get_obstacles(RID p_map) const override;

	virtual void map_force_update(RID p_map) override;
	virtual uint32_t map_get_iteration_id(RID p_map) const override;

	COMMAND_2(map_set_use_async_iterations, RID, p_map, bool, p_enabled);
	virtual bool map_get_use_async_iterations(RID p_map) const override;

	virtual Vector2 map_get_random_point(RID p_map, uint32_t p_navigation_layers, bool p_uniformly) const override;

	virtual RID region_create() override;

	COMMAND_2(region_set_enabled, RID, p_region, bool, p_enabled);
	virtual bool region_get_enabled(RID p_region) const override;

	COMMAND_2(region_set_use_edge_connections, RID, p_region, bool, p_enabled);
	virtual bool region_get_use_edge_connections(RID p_region) const override;

	COMMAND_2(region_set_enter_cost, RID, p_region, real_t, p_enter_cost);
	virtual real_t region_get_enter_cost(RID p_region) const override;
	COMMAND_2(region_set_travel_cost, RID, p_region, real_t, p_travel_cost);
	virtual real_t region_get_travel_cost(RID p_region) const override;

	COMMAND_2(region_set_owner_id, RID, p_region, ObjectID, p_owner_id);
	virtual ObjectID region_get_owner_id(RID p_region) const override;

	virtual bool region_owns_point(RID p_region, const Vector2 &p_point) const override;

	COMMAND_2(region_set_map, RID, p_region, RID, p_map);
	virtual RID region_get_map(RID p_region) const override;
	COMMAND_2(region_set_navigation_layers, RID, p_region, uint32_t, p_navigation_layers);
	virtual uint32_t region_get_navigation_layers(RID p_region) const override;
	COMMAND_2(region_set_transform, RID, p_region, Transform2D, p_transform);
	virtual Transform2D region_get_transform(RID p_region) const override;
	COMMAND_2(region_set_navigation_polygon, RID, p_region, Ref<NavigationPolygon>, p_navigation_polygon);
	virtual int region_get_connections_count(RID p_region) const override;
	virtual Vector2 region_get_connection_pathway_start(RID p_region, int p_connection_id) const override;
	virtual Vector2 region_get_connection_pathway_end(RID p_region, int p_connection_id) const override;
	virtual Vector2 region_get_closest_point(RID p_region, const Vector2 &p_point) const override;
	virtual Vector2 region_get_random_point(RID p_region, uint32_t p_navigation_layers, bool p_uniformly) const override;
	virtual Rect2 region_get_bounds(RID p_region) const override;

	virtual RID link_create() override;

	/// Set the map of this link.
	COMMAND_2(link_set_map, RID, p_link, RID, p_map);
	virtual RID link_get_map(RID p_link) const override;
	COMMAND_2(link_set_enabled, RID, p_link, bool, p_enabled);
	virtual bool link_get_enabled(RID p_link) const override;

	/// Set whether this link travels in both directions.
	COMMAND_2(link_set_bidirectional, RID, p_link, bool, p_bidirectional);
	virtual bool link_is_bidirectional(RID p_link) const override;

	/// Set the link's layers.
	COMMAND_2(link_set_navigation_layers, RID, p_link, uint32_t, p_navigation_layers);
	virtual uint32_t link_get_navigation_layers(RID p_link) const override;

	/// Set the start position of the link.
	COMMAND_2(link_set_start_position, RID, p_link, Vector2, p_position);
	virtual Vector2 link_get_start_position(RID p_link) const override;

	/// Set the end position of the link.
	COMMAND_2(link_set_end_position, RID, p_link, Vector2, p_position);
	virtual Vector2 link_get_end_position(RID p_link) const override;

	/// Set the enter cost of the link.
	COMMAND_2(link_set_enter_cost, RID, p_link, real_t, p_enter_cost);
	virtual real_t link_get_enter_cost(RID p_link) const override;

	/// Set the travel cost of the link.
	COMMAND_2(link_set_travel_cost, RID, p_link, real_t, p_travel_cost);
	virtual real_t link_get_travel_cost(RID p_link) const override;

	/// Set the node which manages this link.
	COMMAND_2(link_set_owner_id, RID, p_link, ObjectID, p_owner_id);
	virtual ObjectID link_get_owner_id(RID p_link) const override;

	/// Creates the agent.
	virtual RID agent_create() override;

	/// Put the agent in the map.
	COMMAND_2(agent_set_map, RID, p_agent, RID, p_map);
	virtual RID agent_get_map(RID p_agent) const override;

	COMMAND_2(agent_set_paused, RID, p_agent, bool, p_paused);
	virtual bool agent_get_paused(RID p_agent) const override;

	COMMAND_2(agent_set_avoidance_enabled, RID, p_agent, bool, p_enabled);
	virtual bool agent_get_avoidance_enabled(RID p_agent) const override;

	/// The maximum distance (center point to
	/// center point) to other agents this agent
	/// takes into account in the navigation. The
	/// larger this number, the longer the running
	/// time of the simulation. If the number is too
	/// low, the simulation will not be safe.
	/// Must be non-negative.
	COMMAND_2(agent_set_neighbor_distance, RID, p_agent, real_t, p_distance);
	virtual real_t agent_get_neighbor_distance(RID p_agent) const override;

	/// The maximum number of other agents this
	/// agent takes into account in the navigation.
	/// The larger this number, the longer the
	/// running time of the simulation. If the
	/// number is too low, the simulation will not
	/// be safe.
	COMMAND_2(agent_set_max_neighbors, RID, p_agent, int, p_count);
	virtual int agent_get_max_neighbors(RID p_agent) const override;

	/// The minimal amount of time for which this
	/// agent's velocities that are computed by the
	/// simulation are safe with respect to other
	/// agents. The larger this number, the sooner
	/// this agent will respond to the presence of
	/// other agents, but the less freedom this
	/// agent has in choosing its velocities.
	/// Must be positive.
	COMMAND_2(agent_set_time_horizon_agents, RID, p_agent, real_t, p_time_horizon);
	virtual real_t agent_get_time_horizon_agents(RID p_agent) const override;
	COMMAND_2(agent_set_time_horizon_obstacles, RID, p_agent, real_t, p_time_horizon);
	virtual real_t agent_get_time_horizon_obstacles(RID p_agent) const override;

	/// The radius of this agent.
	/// Must be non-negative.
	COMMAND_2(agent_set_radius, RID, p_agent, real_t, p_radius);
	virtual real_t agent_get_radius(RID p_agent) const override;

	/// The maximum speed of this agent.
	/// Must be non-negative.
	COMMAND_2(agent_set_max_speed, RID, p_agent, real_t, p_max_speed);
	virtual real_t agent_get_max_speed(RID p_agent) const override;

	/// forces and agent velocity change in the avoidance simulation, adds simulation instability if done recklessly
	COMMAND_2(agent_set_velocity_forced, RID, p_agent, Vector2, p_velocity);

	/// The wanted velocity for the agent as a "suggestion" to the avoidance simulation.
	/// The simulation will try to fulfill this velocity wish if possible but may change the velocity depending on other agent's and obstacles'.
	COMMAND_2(agent_set_velocity, RID, p_agent, Vector2, p_velocity);
	virtual Vector2 agent_get_velocity(RID p_agent) const override;

	/// Position of the agent in world space.
	COMMAND_2(agent_set_position, RID, p_agent, Vector2, p_position);
	virtual Vector2 agent_get_position(RID p_agent) const override;

	/// Returns true if the map got changed the previous frame.
	virtual bool agent_is_map_changed(RID p_agent) const override;

	/// Callback called at the end of the RVO process
	COMMAND_2(agent_set_avoidance_callback, RID, p_agent, Callable, p_callback);
	virtual bool agent_has_avoidance_callback(RID p_agent) const override;

	COMMAND_2(agent_set_avoidance_layers, RID, p_agent, uint32_t, p_layers);
	virtual uint32_t agent_get_avoidance_layers(RID p_agent) const override;

	COMMAND_2(agent_set_avoidance_mask, RID, p_agent, uint32_t, p_mask);
	virtual uint32_t agent_get_avoidance_mask(RID p_agent) const override;

	COMMAND_2(agent_set_avoidance_priority, RID, p_agent, real_t, p_priority);
	virtual real_t agent_get_avoidance_priority(RID p_agent) const override;

	virtual RID obstacle_create() override;
	COMMAND_2(obstacle_set_avoidance_enabled, RID, p_obstacle, bool, p_enabled);
	virtual bool obstacle_get_avoidance_enabled(RID p_obstacle) const override;
	COMMAND_2(obstacle_set_map, RID, p_obstacle, RID, p_map);
	virtual RID obstacle_get_map(RID p_obstacle) const override;
	COMMAND_2(obstacle_set_paused, RID, p_obstacle, bool, p_paused);
	virtual bool obstacle_get_paused(RID p_obstacle) const override;
	COMMAND_2(obstacle_set_radius, RID, p_obstacle, real_t, p_radius);
	virtual real_t obstacle_get_radius(RID p_obstacle) const override;
	COMMAND_2(obstacle_set_velocity, RID, p_obstacle, Vector2, p_velocity);
	virtual Vector2 obstacle_get_velocity(RID p_obstacle) const override;
	COMMAND_2(obstacle_set_position, RID, p_obstacle, Vector2, p_position);
	virtual Vector2 obstacle_get_position(RID p_obstacle) const override;
	COMMAND_2(obstacle_set_vertices, RID, p_obstacle, const Vector<Vector2> &, p_vertices);
	virtual Vector<Vector2> obstacle_get_vertices(RID p_obstacle) const override;
	COMMAND_2(obstacle_set_avoidance_layers, RID, p_obstacle, uint32_t, p_layers);
	virtual uint32_t obstacle_get_avoidance_layers(RID p_obstacle) const override;

	virtual void query_path(const Ref<NavigationPathQueryParameters2D> &p_query_parameters, Ref<NavigationPathQueryResult2D> p_query_result, const Callable &p_callback = Callable()) override;

	COMMAND_1(free, RID, p_object);

	virtual void set_active(bool p_active) override;

	void flush_queries();
	virtual void process(real_t p_delta_time) override;
	virtual void init() override;
	virtual void sync() override;
	virtual void finish() override;

	virtual int get_process_info(ProcessInfo p_info) const override;

	virtual void parse_source_geometry_data(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, Node *p_root_node, const Callable &p_callback = Callable()) override;
	virtual void bake_from_source_geometry_data(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, const Callable &p_callback = Callable()) override;
	virtual void bake_from_source_geometry_data_async(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, const Callable &p_callback = Callable()) override;
	virtual bool is_baking_navigation_polygon(Ref<NavigationPolygon> p_navigation_polygon) const override;

	virtual RID source_geometry_parser_create() override;
	virtual void source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) override;

	virtual Vector<Vector2> simplify_path(const Vector<Vector2> &p_path, real_t p_epsilon) override;

private:
	void internal_free_agent(RID p_object);
	void internal_free_obstacle(RID p_object);
};

#undef COMMAND_1
#undef COMMAND_2

#endif // GODOT_NAVIGATION_SERVER_2D_H
