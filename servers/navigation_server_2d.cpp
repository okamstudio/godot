/**************************************************************************/
/*  navigation_server_2d.cpp                                              */
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

#include "navigation_server_2d.h"
#include "navigation_server_2d.compat.inc"

#include "core/config/project_settings.h"
#include "scene/main/node.h"
#include "servers/navigation/navigation_globals.h"
#include "servers/navigation_server_2d_dummy.h"

NavigationServer2D *NavigationServer2D::singleton = nullptr;

RWLock NavigationServer2D::geometry_parser_rwlock;
RID_Owner<NavMeshGeometryParser2D> NavigationServer2D::geometry_parser_owner;
LocalVector<NavMeshGeometryParser2D *> NavigationServer2D::generator_parsers;

void NavigationServer2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_maps"), &NavigationServer2D::get_maps);

	ClassDB::bind_method(D_METHOD("map_create"), &NavigationServer2D::map_create);
	ClassDB::bind_method(D_METHOD("map_set_active", "map", "active"), &NavigationServer2D::map_set_active);
	ClassDB::bind_method(D_METHOD("map_is_active", "map"), &NavigationServer2D::map_is_active);
	ClassDB::bind_method(D_METHOD("map_set_cell_size", "map", "cell_size"), &NavigationServer2D::map_set_cell_size);
	ClassDB::bind_method(D_METHOD("map_get_cell_size", "map"), &NavigationServer2D::map_get_cell_size);
	ClassDB::bind_method(D_METHOD("map_set_use_edge_connections", "map", "enabled"), &NavigationServer2D::map_set_use_edge_connections);
	ClassDB::bind_method(D_METHOD("map_get_use_edge_connections", "map"), &NavigationServer2D::map_get_use_edge_connections);
	ClassDB::bind_method(D_METHOD("map_set_edge_connection_margin", "map", "margin"), &NavigationServer2D::map_set_edge_connection_margin);
	ClassDB::bind_method(D_METHOD("map_get_edge_connection_margin", "map"), &NavigationServer2D::map_get_edge_connection_margin);
	ClassDB::bind_method(D_METHOD("map_set_link_connection_radius", "map", "radius"), &NavigationServer2D::map_set_link_connection_radius);
	ClassDB::bind_method(D_METHOD("map_get_link_connection_radius", "map"), &NavigationServer2D::map_get_link_connection_radius);
	ClassDB::bind_method(D_METHOD("map_get_path", "map", "origin", "destination", "optimize", "navigation_layers"), &NavigationServer2D::map_get_path, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("map_get_closest_point", "map", "to_point"), &NavigationServer2D::map_get_closest_point);
	ClassDB::bind_method(D_METHOD("map_get_closest_point_owner", "map", "to_point"), &NavigationServer2D::map_get_closest_point_owner);

	ClassDB::bind_method(D_METHOD("map_get_links", "map"), &NavigationServer2D::map_get_links);
	ClassDB::bind_method(D_METHOD("map_get_regions", "map"), &NavigationServer2D::map_get_regions);
	ClassDB::bind_method(D_METHOD("map_get_agents", "map"), &NavigationServer2D::map_get_agents);
	ClassDB::bind_method(D_METHOD("map_get_obstacles", "map"), &NavigationServer2D::map_get_obstacles);

	ClassDB::bind_method(D_METHOD("map_force_update", "map"), &NavigationServer2D::map_force_update);
	ClassDB::bind_method(D_METHOD("map_get_iteration_id", "map"), &NavigationServer2D::map_get_iteration_id);
	ClassDB::bind_method(D_METHOD("map_set_use_async_iterations", "map", "enabled"), &NavigationServer2D::map_set_use_async_iterations);
	ClassDB::bind_method(D_METHOD("map_get_use_async_iterations", "map"), &NavigationServer2D::map_get_use_async_iterations);

	ClassDB::bind_method(D_METHOD("map_get_random_point", "map", "navigation_layers", "uniformly"), &NavigationServer2D::map_get_random_point);

	ClassDB::bind_method(D_METHOD("query_path", "parameters", "result", "callback"), &NavigationServer2D::query_path, DEFVAL(Callable()));

	ClassDB::bind_method(D_METHOD("region_create"), &NavigationServer2D::region_create);
	ClassDB::bind_method(D_METHOD("region_set_enabled", "region", "enabled"), &NavigationServer2D::region_set_enabled);
	ClassDB::bind_method(D_METHOD("region_get_enabled", "region"), &NavigationServer2D::region_get_enabled);
	ClassDB::bind_method(D_METHOD("region_set_use_edge_connections", "region", "enabled"), &NavigationServer2D::region_set_use_edge_connections);
	ClassDB::bind_method(D_METHOD("region_get_use_edge_connections", "region"), &NavigationServer2D::region_get_use_edge_connections);
	ClassDB::bind_method(D_METHOD("region_set_enter_cost", "region", "enter_cost"), &NavigationServer2D::region_set_enter_cost);
	ClassDB::bind_method(D_METHOD("region_get_enter_cost", "region"), &NavigationServer2D::region_get_enter_cost);
	ClassDB::bind_method(D_METHOD("region_set_travel_cost", "region", "travel_cost"), &NavigationServer2D::region_set_travel_cost);
	ClassDB::bind_method(D_METHOD("region_get_travel_cost", "region"), &NavigationServer2D::region_get_travel_cost);
	ClassDB::bind_method(D_METHOD("region_set_owner_id", "region", "owner_id"), &NavigationServer2D::region_set_owner_id);
	ClassDB::bind_method(D_METHOD("region_get_owner_id", "region"), &NavigationServer2D::region_get_owner_id);
	ClassDB::bind_method(D_METHOD("region_owns_point", "region", "point"), &NavigationServer2D::region_owns_point);
	ClassDB::bind_method(D_METHOD("region_set_map", "region", "map"), &NavigationServer2D::region_set_map);
	ClassDB::bind_method(D_METHOD("region_get_map", "region"), &NavigationServer2D::region_get_map);
	ClassDB::bind_method(D_METHOD("region_set_navigation_layers", "region", "navigation_layers"), &NavigationServer2D::region_set_navigation_layers);
	ClassDB::bind_method(D_METHOD("region_get_navigation_layers", "region"), &NavigationServer2D::region_get_navigation_layers);
	ClassDB::bind_method(D_METHOD("region_set_transform", "region", "transform"), &NavigationServer2D::region_set_transform);
	ClassDB::bind_method(D_METHOD("region_get_transform", "region"), &NavigationServer2D::region_get_transform);
	ClassDB::bind_method(D_METHOD("region_set_navigation_polygon", "region", "navigation_polygon"), &NavigationServer2D::region_set_navigation_polygon);
	ClassDB::bind_method(D_METHOD("region_get_connections_count", "region"), &NavigationServer2D::region_get_connections_count);
	ClassDB::bind_method(D_METHOD("region_get_connection_pathway_start", "region", "connection"), &NavigationServer2D::region_get_connection_pathway_start);
	ClassDB::bind_method(D_METHOD("region_get_connection_pathway_end", "region", "connection"), &NavigationServer2D::region_get_connection_pathway_end);
	ClassDB::bind_method(D_METHOD("region_get_closest_point", "region", "to_point"), &NavigationServer2D::region_get_closest_point);
	ClassDB::bind_method(D_METHOD("region_get_random_point", "region", "navigation_layers", "uniformly"), &NavigationServer2D::region_get_random_point);
	ClassDB::bind_method(D_METHOD("region_get_bounds", "region"), &NavigationServer2D::region_get_bounds);

	ClassDB::bind_method(D_METHOD("link_create"), &NavigationServer2D::link_create);
	ClassDB::bind_method(D_METHOD("link_set_map", "link", "map"), &NavigationServer2D::link_set_map);
	ClassDB::bind_method(D_METHOD("link_get_map", "link"), &NavigationServer2D::link_get_map);
	ClassDB::bind_method(D_METHOD("link_set_enabled", "link", "enabled"), &NavigationServer2D::link_set_enabled);
	ClassDB::bind_method(D_METHOD("link_get_enabled", "link"), &NavigationServer2D::link_get_enabled);
	ClassDB::bind_method(D_METHOD("link_set_bidirectional", "link", "bidirectional"), &NavigationServer2D::link_set_bidirectional);
	ClassDB::bind_method(D_METHOD("link_is_bidirectional", "link"), &NavigationServer2D::link_is_bidirectional);
	ClassDB::bind_method(D_METHOD("link_set_navigation_layers", "link", "navigation_layers"), &NavigationServer2D::link_set_navigation_layers);
	ClassDB::bind_method(D_METHOD("link_get_navigation_layers", "link"), &NavigationServer2D::link_get_navigation_layers);
	ClassDB::bind_method(D_METHOD("link_set_start_position", "link", "position"), &NavigationServer2D::link_set_start_position);
	ClassDB::bind_method(D_METHOD("link_get_start_position", "link"), &NavigationServer2D::link_get_start_position);
	ClassDB::bind_method(D_METHOD("link_set_end_position", "link", "position"), &NavigationServer2D::link_set_end_position);
	ClassDB::bind_method(D_METHOD("link_get_end_position", "link"), &NavigationServer2D::link_get_end_position);
	ClassDB::bind_method(D_METHOD("link_set_enter_cost", "link", "enter_cost"), &NavigationServer2D::link_set_enter_cost);
	ClassDB::bind_method(D_METHOD("link_get_enter_cost", "link"), &NavigationServer2D::link_get_enter_cost);
	ClassDB::bind_method(D_METHOD("link_set_travel_cost", "link", "travel_cost"), &NavigationServer2D::link_set_travel_cost);
	ClassDB::bind_method(D_METHOD("link_get_travel_cost", "link"), &NavigationServer2D::link_get_travel_cost);
	ClassDB::bind_method(D_METHOD("link_set_owner_id", "link", "owner_id"), &NavigationServer2D::link_set_owner_id);
	ClassDB::bind_method(D_METHOD("link_get_owner_id", "link"), &NavigationServer2D::link_get_owner_id);

	ClassDB::bind_method(D_METHOD("agent_create"), &NavigationServer2D::agent_create);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_enabled", "agent", "enabled"), &NavigationServer2D::agent_set_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("agent_get_avoidance_enabled", "agent"), &NavigationServer2D::agent_get_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("agent_set_map", "agent", "map"), &NavigationServer2D::agent_set_map);
	ClassDB::bind_method(D_METHOD("agent_get_map", "agent"), &NavigationServer2D::agent_get_map);
	ClassDB::bind_method(D_METHOD("agent_set_paused", "agent", "paused"), &NavigationServer2D::agent_set_paused);
	ClassDB::bind_method(D_METHOD("agent_get_paused", "agent"), &NavigationServer2D::agent_get_paused);
	ClassDB::bind_method(D_METHOD("agent_set_neighbor_distance", "agent", "distance"), &NavigationServer2D::agent_set_neighbor_distance);
	ClassDB::bind_method(D_METHOD("agent_get_neighbor_distance", "agent"), &NavigationServer2D::agent_get_neighbor_distance);
	ClassDB::bind_method(D_METHOD("agent_set_max_neighbors", "agent", "count"), &NavigationServer2D::agent_set_max_neighbors);
	ClassDB::bind_method(D_METHOD("agent_get_max_neighbors", "agent"), &NavigationServer2D::agent_get_max_neighbors);
	ClassDB::bind_method(D_METHOD("agent_set_time_horizon_agents", "agent", "time_horizon"), &NavigationServer2D::agent_set_time_horizon_agents);
	ClassDB::bind_method(D_METHOD("agent_get_time_horizon_agents", "agent"), &NavigationServer2D::agent_get_time_horizon_agents);
	ClassDB::bind_method(D_METHOD("agent_set_time_horizon_obstacles", "agent", "time_horizon"), &NavigationServer2D::agent_set_time_horizon_obstacles);
	ClassDB::bind_method(D_METHOD("agent_get_time_horizon_obstacles", "agent"), &NavigationServer2D::agent_get_time_horizon_obstacles);
	ClassDB::bind_method(D_METHOD("agent_set_radius", "agent", "radius"), &NavigationServer2D::agent_set_radius);
	ClassDB::bind_method(D_METHOD("agent_get_radius", "agent"), &NavigationServer2D::agent_get_radius);
	ClassDB::bind_method(D_METHOD("agent_set_max_speed", "agent", "max_speed"), &NavigationServer2D::agent_set_max_speed);
	ClassDB::bind_method(D_METHOD("agent_get_max_speed", "agent"), &NavigationServer2D::agent_get_max_speed);
	ClassDB::bind_method(D_METHOD("agent_set_velocity_forced", "agent", "velocity"), &NavigationServer2D::agent_set_velocity_forced);
	ClassDB::bind_method(D_METHOD("agent_set_velocity", "agent", "velocity"), &NavigationServer2D::agent_set_velocity);
	ClassDB::bind_method(D_METHOD("agent_get_velocity", "agent"), &NavigationServer2D::agent_get_velocity);
	ClassDB::bind_method(D_METHOD("agent_set_position", "agent", "position"), &NavigationServer2D::agent_set_position);
	ClassDB::bind_method(D_METHOD("agent_get_position", "agent"), &NavigationServer2D::agent_get_position);
	ClassDB::bind_method(D_METHOD("agent_is_map_changed", "agent"), &NavigationServer2D::agent_is_map_changed);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_callback", "agent", "callback"), &NavigationServer2D::agent_set_avoidance_callback);
	ClassDB::bind_method(D_METHOD("agent_has_avoidance_callback", "agent"), &NavigationServer2D::agent_has_avoidance_callback);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_layers", "agent", "layers"), &NavigationServer2D::agent_set_avoidance_layers);
	ClassDB::bind_method(D_METHOD("agent_get_avoidance_layers", "agent"), &NavigationServer2D::agent_get_avoidance_layers);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_mask", "agent", "mask"), &NavigationServer2D::agent_set_avoidance_mask);
	ClassDB::bind_method(D_METHOD("agent_get_avoidance_mask", "agent"), &NavigationServer2D::agent_get_avoidance_mask);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_priority", "agent", "priority"), &NavigationServer2D::agent_set_avoidance_priority);
	ClassDB::bind_method(D_METHOD("agent_get_avoidance_priority", "agent"), &NavigationServer2D::agent_get_avoidance_priority);

	ClassDB::bind_method(D_METHOD("obstacle_create"), &NavigationServer2D::obstacle_create);
	ClassDB::bind_method(D_METHOD("obstacle_set_avoidance_enabled", "obstacle", "enabled"), &NavigationServer2D::obstacle_set_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("obstacle_get_avoidance_enabled", "obstacle"), &NavigationServer2D::obstacle_get_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("obstacle_set_map", "obstacle", "map"), &NavigationServer2D::obstacle_set_map);
	ClassDB::bind_method(D_METHOD("obstacle_get_map", "obstacle"), &NavigationServer2D::obstacle_get_map);
	ClassDB::bind_method(D_METHOD("obstacle_set_paused", "obstacle", "paused"), &NavigationServer2D::obstacle_set_paused);
	ClassDB::bind_method(D_METHOD("obstacle_get_paused", "obstacle"), &NavigationServer2D::obstacle_get_paused);
	ClassDB::bind_method(D_METHOD("obstacle_set_radius", "obstacle", "radius"), &NavigationServer2D::obstacle_set_radius);
	ClassDB::bind_method(D_METHOD("obstacle_get_radius", "obstacle"), &NavigationServer2D::obstacle_get_radius);
	ClassDB::bind_method(D_METHOD("obstacle_set_velocity", "obstacle", "velocity"), &NavigationServer2D::obstacle_set_velocity);
	ClassDB::bind_method(D_METHOD("obstacle_get_velocity", "obstacle"), &NavigationServer2D::obstacle_get_velocity);
	ClassDB::bind_method(D_METHOD("obstacle_set_position", "obstacle", "position"), &NavigationServer2D::obstacle_set_position);
	ClassDB::bind_method(D_METHOD("obstacle_get_position", "obstacle"), &NavigationServer2D::obstacle_get_position);
	ClassDB::bind_method(D_METHOD("obstacle_set_vertices", "obstacle", "vertices"), &NavigationServer2D::obstacle_set_vertices);
	ClassDB::bind_method(D_METHOD("obstacle_get_vertices", "obstacle"), &NavigationServer2D::obstacle_get_vertices);
	ClassDB::bind_method(D_METHOD("obstacle_set_avoidance_layers", "obstacle", "layers"), &NavigationServer2D::obstacle_set_avoidance_layers);
	ClassDB::bind_method(D_METHOD("obstacle_get_avoidance_layers", "obstacle"), &NavigationServer2D::obstacle_get_avoidance_layers);

	ClassDB::bind_method(D_METHOD("parse_source_geometry_data", "navigation_polygon", "source_geometry_data", "root_node", "callback"), &NavigationServer2D::parse_source_geometry_data, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("bake_from_source_geometry_data", "navigation_polygon", "source_geometry_data", "callback"), &NavigationServer2D::bake_from_source_geometry_data, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("bake_from_source_geometry_data_async", "navigation_polygon", "source_geometry_data", "callback"), &NavigationServer2D::bake_from_source_geometry_data_async, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("is_baking_navigation_polygon", "navigation_polygon"), &NavigationServer2D::is_baking_navigation_polygon);

	ClassDB::bind_method(D_METHOD("source_geometry_parser_create"), &NavigationServer2D::source_geometry_parser_create);
	ClassDB::bind_method(D_METHOD("source_geometry_parser_set_callback", "parser", "callback"), &NavigationServer2D::source_geometry_parser_set_callback);

	ClassDB::bind_method(D_METHOD("simplify_path", "path", "epsilon"), &NavigationServer2D::simplify_path);

	ClassDB::bind_method(D_METHOD("free_rid", "rid"), &NavigationServer2D::free);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &NavigationServer2D::set_active);

	ClassDB::bind_method(D_METHOD("set_debug_enabled", "enabled"), &NavigationServer2D::set_debug_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_enabled"), &NavigationServer2D::get_debug_enabled);

	ADD_SIGNAL(MethodInfo("map_changed", PropertyInfo(Variant::RID, "map")));

	ADD_SIGNAL(MethodInfo("navigation_debug_changed"));
	ADD_SIGNAL(MethodInfo("avoidance_debug_changed"));

	ClassDB::bind_method(D_METHOD("get_process_info", "process_info"), &NavigationServer2D::get_process_info);

	BIND_ENUM_CONSTANT(INFO_ACTIVE_MAPS);
	BIND_ENUM_CONSTANT(INFO_REGION_COUNT);
	BIND_ENUM_CONSTANT(INFO_AGENT_COUNT);
	BIND_ENUM_CONSTANT(INFO_LINK_COUNT);
	BIND_ENUM_CONSTANT(INFO_POLYGON_COUNT);
	BIND_ENUM_CONSTANT(INFO_EDGE_COUNT);
	BIND_ENUM_CONSTANT(INFO_EDGE_MERGE_COUNT);
	BIND_ENUM_CONSTANT(INFO_EDGE_CONNECTION_COUNT);
	BIND_ENUM_CONSTANT(INFO_EDGE_FREE_COUNT);
	BIND_ENUM_CONSTANT(INFO_OBSTACLE_COUNT);
}

NavigationServer2D *NavigationServer2D::get_singleton() {
	return singleton;
}

NavigationServer2D::NavigationServer2D() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/2d/default_cell_size", PROPERTY_HINT_RANGE, NavigationDefaults2D::navmesh_cell_size_hint), NavigationDefaults2D::navmesh_cell_size);
	GLOBAL_DEF("navigation/2d/use_edge_connections", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/2d/default_edge_connection_margin", PROPERTY_HINT_RANGE, "0.01,10,0.001,or_greater"), NavigationDefaults2D::edge_connection_margin);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/2d/default_link_connection_radius", PROPERTY_HINT_RANGE, "0.01,10,0.001,or_greater"), NavigationDefaults2D::link_connection_radius);

#ifdef DEBUG_ENABLED
	debug_navigation_edge_connection_color = GLOBAL_GET("debug/shapes/navigation/edge_connection_color");
	debug_navigation_geometry_edge_color = GLOBAL_GET("debug/shapes/navigation/geometry_edge_color");
	debug_navigation_geometry_face_color = GLOBAL_GET("debug/shapes/navigation/geometry_face_color");
	debug_navigation_geometry_edge_disabled_color = GLOBAL_GET("debug/shapes/navigation/geometry_edge_disabled_color");
	debug_navigation_geometry_face_disabled_color = GLOBAL_GET("debug/shapes/navigation/geometry_face_disabled_color");
	debug_navigation_link_connection_color = GLOBAL_GET("debug/shapes/navigation/link_connection_color");
	debug_navigation_link_connection_disabled_color = GLOBAL_GET("debug/shapes/navigation/link_connection_disabled_color");
	debug_navigation_agent_path_color = GLOBAL_GET("debug/shapes/navigation/agent_path_color");

	debug_navigation_enable_edge_connections = GLOBAL_GET("debug/shapes/navigation/enable_edge_connections");
	debug_navigation_enable_edge_lines = GLOBAL_GET("debug/shapes/navigation/enable_edge_lines");
	debug_navigation_enable_geometry_face_random_color = GLOBAL_GET("debug/shapes/navigation/enable_geometry_face_random_color");
	debug_navigation_enable_link_connections = GLOBAL_GET("debug/shapes/navigation/enable_link_connections");

	debug_navigation_enable_agent_paths = GLOBAL_GET("debug/shapes/navigation/enable_agent_paths");
	debug_navigation_agent_path_point_size = GLOBAL_GET("debug/shapes/navigation/agent_path_point_size");

	debug_navigation_avoidance_agents_radius_color = GLOBAL_GET("debug/shapes/avoidance/agents_radius_color");
	debug_navigation_avoidance_obstacles_radius_color = GLOBAL_GET("debug/shapes/avoidance/obstacles_radius_color");
	debug_navigation_avoidance_static_obstacle_pushin_face_color = GLOBAL_GET("debug/shapes/avoidance/obstacles_static_face_pushin_color");
	debug_navigation_avoidance_static_obstacle_pushin_edge_color = GLOBAL_GET("debug/shapes/avoidance/obstacles_static_edge_pushin_color");
	debug_navigation_avoidance_static_obstacle_pushout_face_color = GLOBAL_GET("debug/shapes/avoidance/obstacles_static_face_pushout_color");
	debug_navigation_avoidance_static_obstacle_pushout_edge_color = GLOBAL_GET("debug/shapes/avoidance/obstacles_static_edge_pushout_color");
	debug_navigation_avoidance_enable_agents_radius = GLOBAL_GET("debug/shapes/avoidance/enable_agents_radius");
	debug_navigation_avoidance_enable_obstacles_radius = GLOBAL_GET("debug/shapes/avoidance/enable_obstacles_radius");
	debug_navigation_avoidance_enable_obstacles_static = GLOBAL_GET("debug/shapes/avoidance/enable_obstacles_static");

	if (Engine::get_singleton()->is_editor_hint()) {
		// enable NavigationServer3D when in Editor or else navigation mesh edge connections are invisible
		// on runtime tests SceneTree has "Visible Navigation" set and main iteration takes care of this.
		set_debug_enabled(true);
		set_debug_navigation_enabled(true);
		set_debug_avoidance_enabled(true);
	}
#endif // DEBUG_ENABLED
}

NavigationServer2D::~NavigationServer2D() {
	singleton = nullptr;

	RWLockWrite write_lock(geometry_parser_rwlock);
	for (NavMeshGeometryParser2D *parser : generator_parsers) {
		geometry_parser_owner.free(parser->self);
	}
	generator_parsers.clear();
}

RID NavigationServer2D::source_geometry_parser_create() {
	RWLockWrite write_lock(geometry_parser_rwlock);

	RID rid = geometry_parser_owner.make_rid();

	NavMeshGeometryParser2D *parser = geometry_parser_owner.get_or_null(rid);
	parser->self = rid;

	generator_parsers.push_back(parser);

	return rid;
}

void NavigationServer2D::free(RID p_object) {
	if (!geometry_parser_owner.owns(p_object)) {
		return;
	}
	RWLockWrite write_lock(geometry_parser_rwlock);

	NavMeshGeometryParser2D *parser = geometry_parser_owner.get_or_null(p_object);
	ERR_FAIL_NULL(parser);

	generator_parsers.erase(parser);
	geometry_parser_owner.free(parser->self);
}

void NavigationServer2D::source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) {
	RWLockWrite write_lock(geometry_parser_rwlock);

	NavMeshGeometryParser2D *parser = geometry_parser_owner.get_or_null(p_parser);
	ERR_FAIL_NULL(parser);

	parser->callback = p_callback;
}

void NavigationServer2D::set_debug_enabled(bool p_enabled) {
#ifdef DEBUG_ENABLED
	if (debug_enabled != p_enabled) {
		debug_dirty = true;
	}

	debug_enabled = p_enabled;

	if (debug_dirty) {
		navigation_debug_dirty = true;
		callable_mp(this, &NavigationServer2D::_emit_navigation_debug_changed_signal).call_deferred();

		avoidance_debug_dirty = true;
		callable_mp(this, &NavigationServer2D::_emit_avoidance_debug_changed_signal).call_deferred();
	}
#endif // DEBUG_ENABLED
}

bool NavigationServer2D::get_debug_enabled() const {
	return debug_enabled;
}

#ifdef DEBUG_ENABLED
void NavigationServer2D::_emit_navigation_debug_changed_signal() {
	if (navigation_debug_dirty) {
		navigation_debug_dirty = false;
		emit_signal(SNAME("navigation_debug_changed"));
	}
}

void NavigationServer2D::_emit_avoidance_debug_changed_signal() {
	if (avoidance_debug_dirty) {
		avoidance_debug_dirty = false;
		emit_signal(SNAME("avoidance_debug_changed"));
	}
}
#endif // DEBUG_ENABLED

#ifdef DEBUG_ENABLED
void NavigationServer2D::set_debug_navigation_enabled(bool p_enabled) {
	debug_navigation_enabled = p_enabled;
	navigation_debug_dirty = true;
	callable_mp(this, &NavigationServer2D::_emit_navigation_debug_changed_signal).call_deferred();
}

bool NavigationServer2D::get_debug_navigation_enabled() const {
	return debug_navigation_enabled;
}

void NavigationServer2D::set_debug_avoidance_enabled(bool p_enabled) {
	debug_avoidance_enabled = p_enabled;
	avoidance_debug_dirty = true;
	callable_mp(this, &NavigationServer2D::_emit_avoidance_debug_changed_signal).call_deferred();
}

bool NavigationServer2D::get_debug_avoidance_enabled() const {
	return debug_avoidance_enabled;
}

void NavigationServer2D::set_debug_navigation_edge_connection_color(const Color &p_color) {
	debug_navigation_edge_connection_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_edge_connection_color() const {
	return debug_navigation_edge_connection_color;
}

void NavigationServer2D::set_debug_navigation_geometry_face_color(const Color &p_color) {
	debug_navigation_geometry_face_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_geometry_face_color() const {
	return debug_navigation_geometry_face_color;
}

void NavigationServer2D::set_debug_navigation_geometry_face_disabled_color(const Color &p_color) {
	debug_navigation_geometry_face_disabled_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_geometry_face_disabled_color() const {
	return debug_navigation_geometry_face_disabled_color;
}

void NavigationServer2D::set_debug_navigation_link_connection_color(const Color &p_color) {
	debug_navigation_link_connection_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_link_connection_color() const {
	return debug_navigation_link_connection_color;
}

void NavigationServer2D::set_debug_navigation_link_connection_disabled_color(const Color &p_color) {
	debug_navigation_link_connection_disabled_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_link_connection_disabled_color() const {
	return debug_navigation_link_connection_disabled_color;
}

void NavigationServer2D::set_debug_navigation_geometry_edge_color(const Color &p_color) {
	debug_navigation_geometry_edge_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_geometry_edge_color() const {
	return debug_navigation_geometry_edge_color;
}

void NavigationServer2D::set_debug_navigation_geometry_edge_disabled_color(const Color &p_color) {
	debug_navigation_geometry_edge_disabled_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_geometry_edge_disabled_color() const {
	return debug_navigation_geometry_edge_disabled_color;
}

void NavigationServer2D::set_debug_navigation_enable_edge_connections(const bool p_value) {
	debug_navigation_enable_edge_connections = p_value;
}

bool NavigationServer2D::get_debug_navigation_enable_edge_connections() const {
	return debug_navigation_enable_edge_connections;
}

void NavigationServer2D::set_debug_navigation_enable_geometry_face_random_color(const bool p_value) {
	debug_navigation_enable_geometry_face_random_color = p_value;
}

bool NavigationServer2D::get_debug_navigation_enable_geometry_face_random_color() const {
	return debug_navigation_enable_geometry_face_random_color;
}

void NavigationServer2D::set_debug_navigation_enable_edge_lines(const bool p_value) {
	debug_navigation_enable_edge_lines = p_value;
}

bool NavigationServer2D::get_debug_navigation_enable_edge_lines() const {
	return debug_navigation_enable_edge_lines;
}

void NavigationServer2D::set_debug_navigation_agent_path_color(const Color &p_color) {
	debug_navigation_agent_path_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_agent_path_color() const {
	return debug_navigation_agent_path_color;
}

void NavigationServer2D::set_debug_navigation_enable_agent_paths(const bool p_value) {
	debug_navigation_enable_agent_paths = p_value;
}

bool NavigationServer2D::get_debug_navigation_enable_agent_paths() const {
	return debug_navigation_enable_agent_paths;
}

void NavigationServer2D::set_debug_navigation_agent_path_point_size(real_t p_point_size) {
	debug_navigation_agent_path_point_size = p_point_size;
}

real_t NavigationServer2D::get_debug_navigation_agent_path_point_size() const {
	return debug_navigation_agent_path_point_size;
}

void NavigationServer2D::set_debug_navigation_avoidance_enable_agents_radius(const bool p_value) {
	debug_navigation_avoidance_enable_agents_radius = p_value;
}

bool NavigationServer2D::get_debug_navigation_avoidance_enable_agents_radius() const {
	return debug_navigation_avoidance_enable_agents_radius;
}

void NavigationServer2D::set_debug_navigation_avoidance_enable_obstacles_radius(const bool p_value) {
	debug_navigation_avoidance_enable_obstacles_radius = p_value;
}

bool NavigationServer2D::get_debug_navigation_avoidance_enable_obstacles_radius() const {
	return debug_navigation_avoidance_enable_obstacles_radius;
}

void NavigationServer2D::set_debug_navigation_avoidance_agents_radius_color(const Color &p_color) {
	debug_navigation_avoidance_agents_radius_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_avoidance_agents_radius_color() const {
	return debug_navigation_avoidance_agents_radius_color;
}

void NavigationServer2D::set_debug_navigation_avoidance_obstacles_radius_color(const Color &p_color) {
	debug_navigation_avoidance_obstacles_radius_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_avoidance_obstacles_radius_color() const {
	return debug_navigation_avoidance_obstacles_radius_color;
}

void NavigationServer2D::set_debug_navigation_avoidance_static_obstacle_pushin_face_color(const Color &p_color) {
	debug_navigation_avoidance_static_obstacle_pushin_face_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_avoidance_static_obstacle_pushin_face_color() const {
	return debug_navigation_avoidance_static_obstacle_pushin_face_color;
}

void NavigationServer2D::set_debug_navigation_avoidance_static_obstacle_pushout_face_color(const Color &p_color) {
	debug_navigation_avoidance_static_obstacle_pushout_face_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_avoidance_static_obstacle_pushout_face_color() const {
	return debug_navigation_avoidance_static_obstacle_pushout_face_color;
}

void NavigationServer2D::set_debug_navigation_avoidance_static_obstacle_pushin_edge_color(const Color &p_color) {
	debug_navigation_avoidance_static_obstacle_pushin_edge_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_avoidance_static_obstacle_pushin_edge_color() const {
	return debug_navigation_avoidance_static_obstacle_pushin_edge_color;
}

void NavigationServer2D::set_debug_navigation_avoidance_static_obstacle_pushout_edge_color(const Color &p_color) {
	debug_navigation_avoidance_static_obstacle_pushout_edge_color = p_color;
}

Color NavigationServer2D::get_debug_navigation_avoidance_static_obstacle_pushout_edge_color() const {
	return debug_navigation_avoidance_static_obstacle_pushout_edge_color;
}

void NavigationServer2D::set_debug_navigation_avoidance_enable_obstacles_static(const bool p_value) {
	debug_navigation_avoidance_enable_obstacles_static = p_value;
}

bool NavigationServer2D::get_debug_navigation_avoidance_enable_obstacles_static() const {
	return debug_navigation_avoidance_enable_obstacles_static;
}
#endif // DEBUG_ENABLED

///////////////////////////////////////////////////////

static NavigationServer2D *navigation_server_2d = nullptr;

NavigationServer2DCallback NavigationServer2DManager::create_callback = nullptr;

void NavigationServer2DManager::set_default_server(NavigationServer2DCallback p_callback) {
	create_callback = p_callback;
}

NavigationServer2D *NavigationServer2DManager::new_default_server() {
	if (create_callback == nullptr) {
		return nullptr;
	}

	return create_callback();
}

void NavigationServer2DManager::initialize_server() {
	ERR_FAIL_COND(navigation_server_2d != nullptr);

	// Init 2D Navigation Server
	navigation_server_2d = NavigationServer2DManager::new_default_server();
	if (!navigation_server_2d) {
		WARN_VERBOSE("Failed to initialize NavigationServer2D. Fall back to dummy server.");
		navigation_server_2d = memnew(NavigationServer2DDummy);
	}

	ERR_FAIL_NULL_MSG(navigation_server_2d, "Failed to initialize NavigationServer2D.");
	navigation_server_2d->init();
}

void NavigationServer2DManager::finalize_server() {
	ERR_FAIL_NULL(navigation_server_2d);
	navigation_server_2d->finish();
	memdelete(navigation_server_2d);
	navigation_server_2d = nullptr;
}
