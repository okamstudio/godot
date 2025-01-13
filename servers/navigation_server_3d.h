/**************************************************************************/
/*  navigation_server_3d.h                                                */
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

#ifndef NAVIGATION_SERVER_3D_H
#define NAVIGATION_SERVER_3D_H

#include "core/object/class_db.h"
#include "core/templates/rid.h"

#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "scene/resources/navigation_mesh.h"
#include "servers/navigation/navigation_path_query_parameters_3d.h"
#include "servers/navigation/navigation_path_query_result_3d.h"

/// This server uses the concept of internal mutability.
/// All the constant functions can be called in multithread because internally
/// the server takes care to schedule the functions access.
///
/// Note: All the `set` functions are commands executed during the `sync` phase,
/// don't expect that a change is immediately propagated.

class NavigationServer3D : public Object {
	GDCLASS(NavigationServer3D, Object);

	static NavigationServer3D *singleton;

protected:
	static void _bind_methods();

public:
	/// Thread safe, can be used across many threads.
	static NavigationServer3D *get_singleton();

	virtual TypedArray<RID> get_maps() const = 0;

	/// Create a new map.
	virtual RID map_create() = 0;

	/// Set map active.
	virtual void map_set_active(RID p_map, bool p_active) = 0;

	/// Returns true if the map is active.
	virtual bool map_is_active(RID p_map) const = 0;

	/// Set the map UP direction.
	virtual void map_set_up(RID p_map, Vector3 p_up) = 0;

	/// Returns the map UP direction.
	virtual Vector3 map_get_up(RID p_map) const = 0;

	/// Set the map cell size used to weld the navigation mesh polygons.
	virtual void map_set_cell_size(RID p_map, real_t p_cell_size) = 0;

	/// Returns the map cell size.
	virtual real_t map_get_cell_size(RID p_map) const = 0;

	virtual void map_set_cell_height(RID p_map, real_t p_height) = 0;
	virtual real_t map_get_cell_height(RID p_map) const = 0;

	virtual void map_set_merge_rasterizer_cell_scale(RID p_map, float p_value) = 0;
	virtual float map_get_merge_rasterizer_cell_scale(RID p_map) const = 0;

	virtual void map_set_use_edge_connections(RID p_map, bool p_enabled) = 0;
	virtual bool map_get_use_edge_connections(RID p_map) const = 0;

	/// Set the map edge connection margin used to weld the compatible region edges.
	virtual void map_set_edge_connection_margin(RID p_map, real_t p_connection_margin) = 0;

	/// Returns the edge connection margin of this map.
	virtual real_t map_get_edge_connection_margin(RID p_map) const = 0;

	/// Set the map link connection radius used to attach links to the nav mesh.
	virtual void map_set_link_connection_radius(RID p_map, real_t p_connection_radius) = 0;

	/// Returns the link connection radius of this map.
	virtual real_t map_get_link_connection_radius(RID p_map) const = 0;

	/// Returns the navigation path to reach the destination from the origin.
	virtual Vector<Vector3> map_get_path(RID p_map, Vector3 p_origin, Vector3 p_destination, bool p_optimize, uint32_t p_navigation_layers = 1) = 0;

	virtual Vector3 map_get_closest_point_to_segment(RID p_map, const Vector3 &p_from, const Vector3 &p_to, const bool p_use_collision = false) const = 0;
	virtual Vector3 map_get_closest_point(RID p_map, const Vector3 &p_point) const = 0;
	virtual Vector3 map_get_closest_point_normal(RID p_map, const Vector3 &p_point) const = 0;
	virtual RID map_get_closest_point_owner(RID p_map, const Vector3 &p_point) const = 0;

	virtual TypedArray<RID> map_get_links(RID p_map) const = 0;
	virtual TypedArray<RID> map_get_regions(RID p_map) const = 0;
	virtual TypedArray<RID> map_get_agents(RID p_map) const = 0;
	virtual TypedArray<RID> map_get_obstacles(RID p_map) const = 0;

	virtual void map_force_update(RID p_map) = 0;
	virtual uint32_t map_get_iteration_id(RID p_map) const = 0;

	virtual Vector3 map_get_random_point(RID p_map, uint32_t p_navigation_layers, bool p_uniformly) const = 0;

	/// Creates a new region.
	virtual RID region_create() = 0;

	virtual void region_set_enabled(RID p_region, bool p_enabled) = 0;
	virtual bool region_get_enabled(RID p_region) const = 0;

	virtual void region_set_use_edge_connections(RID p_region, bool p_enabled) = 0;
	virtual bool region_get_use_edge_connections(RID p_region) const = 0;

	/// Set the enter_cost of a region
	virtual void region_set_enter_cost(RID p_region, real_t p_enter_cost) = 0;
	virtual real_t region_get_enter_cost(RID p_region) const = 0;

	/// Set the travel_cost of a region
	virtual void region_set_travel_cost(RID p_region, real_t p_travel_cost) = 0;
	virtual real_t region_get_travel_cost(RID p_region) const = 0;

	/// Set the node which manages this region.
	virtual void region_set_owner_id(RID p_region, ObjectID p_owner_id) = 0;
	virtual ObjectID region_get_owner_id(RID p_region) const = 0;

	virtual bool region_owns_point(RID p_region, const Vector3 &p_point) const = 0;

	/// Set the map of this region.
	virtual void region_set_map(RID p_region, RID p_map) = 0;
	virtual RID region_get_map(RID p_region) const = 0;

	/// Set the region's layers
	virtual void region_set_navigation_layers(RID p_region, uint32_t p_navigation_layers) = 0;
	virtual uint32_t region_get_navigation_layers(RID p_region) const = 0;

	/// Set the global transformation of this region.
	virtual void region_set_transform(RID p_region, Transform3D p_transform) = 0;
	virtual Transform3D region_get_transform(RID p_region) const = 0;

	/// Set the navigation mesh of this region.
	virtual void region_set_navigation_mesh(RID p_region, Ref<NavigationMesh> p_navigation_mesh) = 0;

#ifndef DISABLE_DEPRECATED
	/// Bake the navigation mesh.
	virtual void region_bake_navigation_mesh(Ref<NavigationMesh> p_navigation_mesh, Node *p_root_node) = 0;
#endif // DISABLE_DEPRECATED

	/// Get a list of a region's connection to other regions.
	virtual int region_get_connections_count(RID p_region) const = 0;
	virtual Vector3 region_get_connection_pathway_start(RID p_region, int p_connection_id) const = 0;
	virtual Vector3 region_get_connection_pathway_end(RID p_region, int p_connection_id) const = 0;

	virtual Vector3 region_get_closest_point_to_segment(RID p_region, const Vector3 &p_from, const Vector3 &p_to, bool p_use_collision = false) const = 0;
	virtual Vector3 region_get_closest_point(RID p_region, const Vector3 &p_point) const = 0;
	virtual Vector3 region_get_closest_point_normal(RID p_region, const Vector3 &p_point) const = 0;
	virtual Vector3 region_get_random_point(RID p_region, uint32_t p_navigation_layers, bool p_uniformly) const = 0;

	/// Creates a new link between positions in the nav map.
	virtual RID link_create() = 0;

	/// Set the map of this link.
	virtual void link_set_map(RID p_link, RID p_map) = 0;
	virtual RID link_get_map(RID p_link) const = 0;

	virtual void link_set_enabled(RID p_link, bool p_enabled) = 0;
	virtual bool link_get_enabled(RID p_link) const = 0;

	/// Set whether this link travels in both directions.
	virtual void link_set_bidirectional(RID p_link, bool p_bidirectional) = 0;
	virtual bool link_is_bidirectional(RID p_link) const = 0;

	/// Set the link's layers.
	virtual void link_set_navigation_layers(RID p_link, uint32_t p_navigation_layers) = 0;
	virtual uint32_t link_get_navigation_layers(RID p_link) const = 0;

	/// Set the start position of the link.
	virtual void link_set_start_position(RID p_link, Vector3 p_position) = 0;
	virtual Vector3 link_get_start_position(RID p_link) const = 0;

	/// Set the end position of the link.
	virtual void link_set_end_position(RID p_link, Vector3 p_position) = 0;
	virtual Vector3 link_get_end_position(RID p_link) const = 0;

	/// Set the enter cost of the link.
	virtual void link_set_enter_cost(RID p_link, real_t p_enter_cost) = 0;
	virtual real_t link_get_enter_cost(RID p_link) const = 0;

	/// Set the travel cost of the link.
	virtual void link_set_travel_cost(RID p_link, real_t p_travel_cost) = 0;
	virtual real_t link_get_travel_cost(RID p_link) const = 0;

	/// Set the node which manages this link.
	virtual void link_set_owner_id(RID p_link, ObjectID p_owner_id) = 0;
	virtual ObjectID link_get_owner_id(RID p_link) const = 0;

	/// Creates the agent.
	virtual RID agent_create() = 0;

	/// Put the agent in the map.
	virtual void agent_set_map(RID p_agent, RID p_map) = 0;
	virtual RID agent_get_map(RID p_agent) const = 0;

	virtual void agent_set_paused(RID p_agent, bool p_paused) = 0;
	virtual bool agent_get_paused(RID p_agent) const = 0;

	virtual void agent_set_avoidance_enabled(RID p_agent, bool p_enabled) = 0;
	virtual bool agent_get_avoidance_enabled(RID p_agent) const = 0;

	virtual void agent_set_use_3d_avoidance(RID p_agent, bool p_enabled) = 0;
	virtual bool agent_get_use_3d_avoidance(RID p_agent) const = 0;

	/// The maximum distance (center point to
	/// center point) to other agents this agent
	/// takes into account in the navigation. The
	/// larger this number, the longer the running
	/// time of the simulation. If the number is too
	/// low, the simulation will not be safe.
	/// Must be non-negative.
	virtual void agent_set_neighbor_distance(RID p_agent, real_t p_distance) = 0;
	virtual real_t agent_get_neighbor_distance(RID p_agent) const = 0;

	/// The maximum number of other agents this
	/// agent takes into account in the navigation.
	/// The larger this number, the longer the
	/// running time of the simulation. If the
	/// number is too low, the simulation will not
	/// be safe.
	virtual void agent_set_max_neighbors(RID p_agent, int p_count) = 0;
	virtual int agent_get_max_neighbors(RID p_agent) const = 0;

	// Sets the minimum amount of time in seconds that an agent's
	// must be able to stay on the calculated velocity while still avoiding collisions with agent's
	// if this value is set to high an agent will often fall back to using a very low velocity just to be safe
	virtual void agent_set_time_horizon_agents(RID p_agent, real_t p_time_horizon) = 0;
	virtual real_t agent_get_time_horizon_agents(RID p_agent) const = 0;

	/// Sets the minimum amount of time in seconds that an agent's
	// must be able to stay on the calculated velocity while still avoiding collisions with obstacle's
	// if this value is set to high an agent will often fall back to using a very low velocity just to be safe
	virtual void agent_set_time_horizon_obstacles(RID p_agent, real_t p_time_horizon) = 0;
	virtual real_t agent_get_time_horizon_obstacles(RID p_agent) const = 0;

	/// The radius of this agent.
	/// Must be non-negative.
	virtual void agent_set_radius(RID p_agent, real_t p_radius) = 0;
	virtual real_t agent_get_radius(RID p_agent) const = 0;

	virtual void agent_set_height(RID p_agent, real_t p_height) = 0;
	virtual real_t agent_get_height(RID p_agent) const = 0;

	/// The maximum speed of this agent.
	/// Must be non-negative.
	virtual void agent_set_max_speed(RID p_agent, real_t p_max_speed) = 0;
	virtual real_t agent_get_max_speed(RID p_agent) const = 0;

	/// forces and agent velocity change in the avoidance simulation, adds simulation instability if done recklessly
	virtual void agent_set_velocity_forced(RID p_agent, Vector3 p_velocity) = 0;

	/// The wanted velocity for the agent as a "suggestion" to the avoidance simulation.
	/// The simulation will try to fulfill this velocity wish if possible but may change the velocity depending on other agent's and obstacles'.
	virtual void agent_set_velocity(RID p_agent, Vector3 p_velocity) = 0;
	virtual Vector3 agent_get_velocity(RID p_agent) const = 0;

	/// Position of the agent in world space.
	virtual void agent_set_position(RID p_agent, Vector3 p_position) = 0;
	virtual Vector3 agent_get_position(RID p_agent) const = 0;

	/// Returns true if the map got changed the previous frame.
	virtual bool agent_is_map_changed(RID p_agent) const = 0;

	/// Callback called at the end of the RVO process
	virtual void agent_set_avoidance_callback(RID p_agent, Callable p_callback) = 0;
	virtual bool agent_has_avoidance_callback(RID p_agent) const = 0;

	virtual void agent_set_avoidance_layers(RID p_agent, uint32_t p_layers) = 0;
	virtual uint32_t agent_get_avoidance_layers(RID p_agent) const = 0;

	virtual void agent_set_avoidance_mask(RID p_agent, uint32_t p_mask) = 0;
	virtual uint32_t agent_get_avoidance_mask(RID p_agent) const = 0;

	virtual void agent_set_avoidance_priority(RID p_agent, real_t p_priority) = 0;
	virtual real_t agent_get_avoidance_priority(RID p_agent) const = 0;

	/// Creates the obstacle.
	virtual RID obstacle_create() = 0;

	virtual void obstacle_set_map(RID p_obstacle, RID p_map) = 0;
	virtual RID obstacle_get_map(RID p_obstacle) const = 0;

	virtual void obstacle_set_paused(RID p_obstacle, bool p_paused) = 0;
	virtual bool obstacle_get_paused(RID p_obstacle) const = 0;

	virtual void obstacle_set_avoidance_enabled(RID p_obstacle, bool p_enabled) = 0;
	virtual bool obstacle_get_avoidance_enabled(RID p_obstacle) const = 0;

	virtual void obstacle_set_use_3d_avoidance(RID p_obstacle, bool p_enabled) = 0;
	virtual bool obstacle_get_use_3d_avoidance(RID p_obstacle) const = 0;

	virtual void obstacle_set_radius(RID p_obstacle, real_t p_radius) = 0;
	virtual real_t obstacle_get_radius(RID p_obstacle) const = 0;
	virtual void obstacle_set_height(RID p_obstacle, real_t p_height) = 0;
	virtual real_t obstacle_get_height(RID p_obstacle) const = 0;
	virtual void obstacle_set_velocity(RID p_obstacle, Vector3 p_velocity) = 0;
	virtual Vector3 obstacle_get_velocity(RID p_obstacle) const = 0;
	virtual void obstacle_set_position(RID p_obstacle, Vector3 p_position) = 0;
	virtual Vector3 obstacle_get_position(RID p_obstacle) const = 0;
	virtual void obstacle_set_vertices(RID p_obstacle, const Vector<Vector3> &p_vertices) = 0;
	virtual Vector<Vector3> obstacle_get_vertices(RID p_obstacle) const = 0;
	virtual void obstacle_set_avoidance_layers(RID p_obstacle, uint32_t p_layers) = 0;
	virtual uint32_t obstacle_get_avoidance_layers(RID p_obstacle) const = 0;

	/// Destroy the `RID`
	virtual void free(RID p_object) = 0;

	/// Control activation of this server.
	virtual void set_active(bool p_active) = 0;

	/// Process the collision avoidance agents.
	/// The result of this process is needed by the physics server,
	/// so this must be called in the main thread.
	/// Note: This function is not thread safe.
	virtual void process(real_t delta_time) = 0;
	virtual void init() = 0;
	virtual void sync() = 0;
	virtual void finish() = 0;

	/// Returns a customized navigation path using a query parameters object
	virtual void query_path(const Ref<NavigationPathQueryParameters3D> &p_query_parameters, Ref<NavigationPathQueryResult3D> p_query_result, const Callable &p_callback = Callable()) = 0;

#ifndef _3D_DISABLED
	virtual void parse_source_geometry_data(const Ref<NavigationMesh> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData3D> &p_source_geometry_data, Node *p_root_node, const Callable &p_callback = Callable()) = 0;
	virtual void bake_from_source_geometry_data(const Ref<NavigationMesh> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData3D> &p_source_geometry_data, const Callable &p_callback = Callable()) = 0;
	virtual void bake_from_source_geometry_data_async(const Ref<NavigationMesh> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData3D> &p_source_geometry_data, const Callable &p_callback = Callable()) = 0;
	virtual bool is_baking_navigation_mesh(Ref<NavigationMesh> p_navigation_mesh) const = 0;
#endif // _3D_DISABLED

	virtual RID source_geometry_parser_create() = 0;
	virtual void source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) = 0;

	virtual Vector<Vector3> simplify_path(const Vector<Vector3> &p_path, real_t p_epsilon) = 0;

	NavigationServer3D();
	~NavigationServer3D() override;

	enum ProcessInfo {
		INFO_ACTIVE_MAPS,
		INFO_REGION_COUNT,
		INFO_AGENT_COUNT,
		INFO_LINK_COUNT,
		INFO_POLYGON_COUNT,
		INFO_EDGE_COUNT,
		INFO_EDGE_MERGE_COUNT,
		INFO_EDGE_CONNECTION_COUNT,
		INFO_EDGE_FREE_COUNT,
		INFO_OBSTACLE_COUNT,
	};

	virtual int get_process_info(ProcessInfo p_info) const = 0;

	void set_debug_enabled(bool p_enabled);
	bool get_debug_enabled() const;

protected:
#ifndef DISABLE_DEPRECATED
	Vector<Vector3> _map_get_path_bind_compat_100129(RID p_map, Vector3 p_origin, Vector3 p_destination, bool p_optimize, uint32_t p_navigation_layers = 1) const;
	void _query_path_bind_compat_100129(const Ref<NavigationPathQueryParameters3D> &p_query_parameters, Ref<NavigationPathQueryResult3D> p_query_result) const;
	static void _bind_compatibility_methods();
#endif

private:
	bool debug_enabled = false;

#ifdef DEBUG_ENABLED
	bool debug_dirty = true;

	bool debug_navigation_enabled = false;
	bool navigation_debug_dirty = true;
	void _emit_navigation_debug_changed_signal();

	bool debug_avoidance_enabled = false;
	bool avoidance_debug_dirty = true;
	void _emit_avoidance_debug_changed_signal();

	Color debug_navigation_edge_connection_color = Color(1.0, 0.0, 1.0, 1.0);
	Color debug_navigation_geometry_edge_color = Color(0.5, 1.0, 1.0, 1.0);
	Color debug_navigation_geometry_face_color = Color(0.5, 1.0, 1.0, 0.4);
	Color debug_navigation_geometry_edge_disabled_color = Color(0.5, 0.5, 0.5, 1.0);
	Color debug_navigation_geometry_face_disabled_color = Color(0.5, 0.5, 0.5, 0.4);
	Color debug_navigation_link_connection_color = Color(1.0, 0.5, 1.0, 1.0);
	Color debug_navigation_link_connection_disabled_color = Color(0.5, 0.5, 0.5, 1.0);
	Color debug_navigation_agent_path_color = Color(1.0, 0.0, 0.0, 1.0);

	real_t debug_navigation_agent_path_point_size = 4.0;

	Color debug_navigation_avoidance_agents_radius_color = Color(1.0, 1.0, 0.0, 0.25);
	Color debug_navigation_avoidance_obstacles_radius_color = Color(1.0, 0.5, 0.0, 0.25);

	Color debug_navigation_avoidance_static_obstacle_pushin_face_color = Color(1.0, 0.0, 0.0, 0.0);
	Color debug_navigation_avoidance_static_obstacle_pushout_face_color = Color(1.0, 1.0, 0.0, 0.5);
	Color debug_navigation_avoidance_static_obstacle_pushin_edge_color = Color(1.0, 0.0, 0.0, 1.0);
	Color debug_navigation_avoidance_static_obstacle_pushout_edge_color = Color(1.0, 1.0, 0.0, 1.0);

	bool debug_navigation_enable_edge_connections = true;
	bool debug_navigation_enable_edge_connections_xray = true;
	bool debug_navigation_enable_edge_lines = true;
	bool debug_navigation_enable_edge_lines_xray = true;
	bool debug_navigation_enable_geometry_face_random_color = true;
	bool debug_navigation_enable_link_connections = true;
	bool debug_navigation_enable_link_connections_xray = true;
	bool debug_navigation_enable_agent_paths = true;
	bool debug_navigation_enable_agent_paths_xray = true;

	bool debug_navigation_avoidance_enable_agents_radius = true;
	bool debug_navigation_avoidance_enable_obstacles_radius = true;
	bool debug_navigation_avoidance_enable_obstacles_static = true;

	Ref<StandardMaterial3D> debug_navigation_geometry_edge_material;
	Ref<StandardMaterial3D> debug_navigation_geometry_face_material;
	Ref<StandardMaterial3D> debug_navigation_geometry_edge_disabled_material;
	Ref<StandardMaterial3D> debug_navigation_geometry_face_disabled_material;
	Ref<StandardMaterial3D> debug_navigation_edge_connections_material;
	Ref<StandardMaterial3D> debug_navigation_link_connections_material;
	Ref<StandardMaterial3D> debug_navigation_link_connections_disabled_material;
	Ref<StandardMaterial3D> debug_navigation_avoidance_agents_radius_material;
	Ref<StandardMaterial3D> debug_navigation_avoidance_obstacles_radius_material;

	Ref<StandardMaterial3D> debug_navigation_avoidance_static_obstacle_pushin_face_material;
	Ref<StandardMaterial3D> debug_navigation_avoidance_static_obstacle_pushout_face_material;
	Ref<StandardMaterial3D> debug_navigation_avoidance_static_obstacle_pushin_edge_material;
	Ref<StandardMaterial3D> debug_navigation_avoidance_static_obstacle_pushout_edge_material;

	Ref<StandardMaterial3D> debug_navigation_agent_path_line_material;
	Ref<StandardMaterial3D> debug_navigation_agent_path_point_material;

public:
	void set_debug_navigation_enabled(bool p_enabled);
	bool get_debug_navigation_enabled() const;

	void set_debug_avoidance_enabled(bool p_enabled);
	bool get_debug_avoidance_enabled() const;

	void set_debug_navigation_edge_connection_color(const Color &p_color);
	Color get_debug_navigation_edge_connection_color() const;

	void set_debug_navigation_geometry_edge_color(const Color &p_color);
	Color get_debug_navigation_geometry_edge_color() const;

	void set_debug_navigation_geometry_face_color(const Color &p_color);
	Color get_debug_navigation_geometry_face_color() const;

	void set_debug_navigation_geometry_edge_disabled_color(const Color &p_color);
	Color get_debug_navigation_geometry_edge_disabled_color() const;

	void set_debug_navigation_geometry_face_disabled_color(const Color &p_color);
	Color get_debug_navigation_geometry_face_disabled_color() const;

	void set_debug_navigation_link_connection_color(const Color &p_color);
	Color get_debug_navigation_link_connection_color() const;

	void set_debug_navigation_link_connection_disabled_color(const Color &p_color);
	Color get_debug_navigation_link_connection_disabled_color() const;

	void set_debug_navigation_agent_path_color(const Color &p_color);
	Color get_debug_navigation_agent_path_color() const;

	void set_debug_navigation_avoidance_agents_radius_color(const Color &p_color);
	Color get_debug_navigation_avoidance_agents_radius_color() const;

	void set_debug_navigation_avoidance_obstacles_radius_color(const Color &p_color);
	Color get_debug_navigation_avoidance_obstacles_radius_color() const;

	void set_debug_navigation_avoidance_static_obstacle_pushin_face_color(const Color &p_color);
	Color get_debug_navigation_avoidance_static_obstacle_pushin_face_color() const;

	void set_debug_navigation_avoidance_static_obstacle_pushout_face_color(const Color &p_color);
	Color get_debug_navigation_avoidance_static_obstacle_pushout_face_color() const;

	void set_debug_navigation_avoidance_static_obstacle_pushin_edge_color(const Color &p_color);
	Color get_debug_navigation_avoidance_static_obstacle_pushin_edge_color() const;

	void set_debug_navigation_avoidance_static_obstacle_pushout_edge_color(const Color &p_color);
	Color get_debug_navigation_avoidance_static_obstacle_pushout_edge_color() const;

	void set_debug_navigation_enable_edge_connections(const bool p_value);
	bool get_debug_navigation_enable_edge_connections() const;

	void set_debug_navigation_enable_edge_connections_xray(const bool p_value);
	bool get_debug_navigation_enable_edge_connections_xray() const;

	void set_debug_navigation_enable_edge_lines(const bool p_value);
	bool get_debug_navigation_enable_edge_lines() const;

	void set_debug_navigation_enable_edge_lines_xray(const bool p_value);
	bool get_debug_navigation_enable_edge_lines_xray() const;

	void set_debug_navigation_enable_geometry_face_random_color(const bool p_value);
	bool get_debug_navigation_enable_geometry_face_random_color() const;

	void set_debug_navigation_enable_link_connections(const bool p_value);
	bool get_debug_navigation_enable_link_connections() const;

	void set_debug_navigation_enable_link_connections_xray(const bool p_value);
	bool get_debug_navigation_enable_link_connections_xray() const;

	void set_debug_navigation_enable_agent_paths(const bool p_value);
	bool get_debug_navigation_enable_agent_paths() const;

	void set_debug_navigation_enable_agent_paths_xray(const bool p_value);
	bool get_debug_navigation_enable_agent_paths_xray() const;

	void set_debug_navigation_agent_path_point_size(real_t p_point_size);
	real_t get_debug_navigation_agent_path_point_size() const;

	void set_debug_navigation_avoidance_enable_agents_radius(const bool p_value);
	bool get_debug_navigation_avoidance_enable_agents_radius() const;

	void set_debug_navigation_avoidance_enable_obstacles_radius(const bool p_value);
	bool get_debug_navigation_avoidance_enable_obstacles_radius() const;

	void set_debug_navigation_avoidance_enable_obstacles_static(const bool p_value);
	bool get_debug_navigation_avoidance_enable_obstacles_static() const;

	Ref<StandardMaterial3D> get_debug_navigation_geometry_face_material();
	Ref<StandardMaterial3D> get_debug_navigation_geometry_edge_material();
	Ref<StandardMaterial3D> get_debug_navigation_geometry_face_disabled_material();
	Ref<StandardMaterial3D> get_debug_navigation_geometry_edge_disabled_material();
	Ref<StandardMaterial3D> get_debug_navigation_edge_connections_material();
	Ref<StandardMaterial3D> get_debug_navigation_link_connections_material();
	Ref<StandardMaterial3D> get_debug_navigation_link_connections_disabled_material();

	Ref<StandardMaterial3D> get_debug_navigation_agent_path_line_material();
	Ref<StandardMaterial3D> get_debug_navigation_agent_path_point_material();

	Ref<StandardMaterial3D> get_debug_navigation_avoidance_agents_radius_material();
	Ref<StandardMaterial3D> get_debug_navigation_avoidance_obstacles_radius_material();

	Ref<StandardMaterial3D> get_debug_navigation_avoidance_static_obstacle_pushin_face_material();
	Ref<StandardMaterial3D> get_debug_navigation_avoidance_static_obstacle_pushout_face_material();
	Ref<StandardMaterial3D> get_debug_navigation_avoidance_static_obstacle_pushin_edge_material();
	Ref<StandardMaterial3D> get_debug_navigation_avoidance_static_obstacle_pushout_edge_material();
#endif // DEBUG_ENABLED
};

typedef NavigationServer3D *(*NavigationServer3DCallback)();

/// Manager used for the server singleton registration
class NavigationServer3DManager {
	static NavigationServer3DCallback create_callback;

public:
	static void set_default_server(NavigationServer3DCallback p_callback);
	static NavigationServer3D *new_default_server();
};

VARIANT_ENUM_CAST(NavigationServer3D::ProcessInfo);

#endif // NAVIGATION_SERVER_3D_H
