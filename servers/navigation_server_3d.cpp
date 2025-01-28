/**************************************************************************/
/*  navigation_server_3d.cpp                                              */
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

#include "navigation_server_3d.h"
#include "navigation_server_3d.compat.inc"

#include "core/config/project_settings.h"
#include "scene/main/node.h"
#include "servers/navigation/navigation_globals.h"
#include "servers/navigation_server_3d_dummy.h"

NavigationServer3D *NavigationServer3D::singleton = nullptr;

RWLock NavigationServer3D::geometry_parser_rwlock;
RID_Owner<NavMeshGeometryParser3D> NavigationServer3D::geometry_parser_owner;
LocalVector<NavMeshGeometryParser3D *> NavigationServer3D::generator_parsers;

void NavigationServer3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_maps"), &NavigationServer3D::get_maps);

	ClassDB::bind_method(D_METHOD("map_create"), &NavigationServer3D::map_create);
	ClassDB::bind_method(D_METHOD("map_set_active", "map", "active"), &NavigationServer3D::map_set_active);
	ClassDB::bind_method(D_METHOD("map_is_active", "map"), &NavigationServer3D::map_is_active);
	ClassDB::bind_method(D_METHOD("map_set_up", "map", "up"), &NavigationServer3D::map_set_up);
	ClassDB::bind_method(D_METHOD("map_get_up", "map"), &NavigationServer3D::map_get_up);
	ClassDB::bind_method(D_METHOD("map_set_cell_size", "map", "cell_size"), &NavigationServer3D::map_set_cell_size);
	ClassDB::bind_method(D_METHOD("map_get_cell_size", "map"), &NavigationServer3D::map_get_cell_size);
	ClassDB::bind_method(D_METHOD("map_set_cell_height", "map", "cell_height"), &NavigationServer3D::map_set_cell_height);
	ClassDB::bind_method(D_METHOD("map_get_cell_height", "map"), &NavigationServer3D::map_get_cell_height);
	ClassDB::bind_method(D_METHOD("map_set_merge_rasterizer_cell_scale", "map", "scale"), &NavigationServer3D::map_set_merge_rasterizer_cell_scale);
	ClassDB::bind_method(D_METHOD("map_get_merge_rasterizer_cell_scale", "map"), &NavigationServer3D::map_get_merge_rasterizer_cell_scale);
	ClassDB::bind_method(D_METHOD("map_set_use_edge_connections", "map", "enabled"), &NavigationServer3D::map_set_use_edge_connections);
	ClassDB::bind_method(D_METHOD("map_get_use_edge_connections", "map"), &NavigationServer3D::map_get_use_edge_connections);
	ClassDB::bind_method(D_METHOD("map_set_edge_connection_margin", "map", "margin"), &NavigationServer3D::map_set_edge_connection_margin);
	ClassDB::bind_method(D_METHOD("map_get_edge_connection_margin", "map"), &NavigationServer3D::map_get_edge_connection_margin);
	ClassDB::bind_method(D_METHOD("map_set_link_connection_radius", "map", "radius"), &NavigationServer3D::map_set_link_connection_radius);
	ClassDB::bind_method(D_METHOD("map_get_link_connection_radius", "map"), &NavigationServer3D::map_get_link_connection_radius);
	ClassDB::bind_method(D_METHOD("map_get_path", "map", "origin", "destination", "optimize", "navigation_layers"), &NavigationServer3D::map_get_path, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("map_get_closest_point_to_segment", "map", "start", "end", "use_collision"), &NavigationServer3D::map_get_closest_point_to_segment, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("map_get_closest_point", "map", "to_point"), &NavigationServer3D::map_get_closest_point);
	ClassDB::bind_method(D_METHOD("map_get_closest_point_normal", "map", "to_point"), &NavigationServer3D::map_get_closest_point_normal);
	ClassDB::bind_method(D_METHOD("map_get_closest_point_owner", "map", "to_point"), &NavigationServer3D::map_get_closest_point_owner);

	ClassDB::bind_method(D_METHOD("map_get_links", "map"), &NavigationServer3D::map_get_links);
	ClassDB::bind_method(D_METHOD("map_get_regions", "map"), &NavigationServer3D::map_get_regions);
	ClassDB::bind_method(D_METHOD("map_get_agents", "map"), &NavigationServer3D::map_get_agents);
	ClassDB::bind_method(D_METHOD("map_get_obstacles", "map"), &NavigationServer3D::map_get_obstacles);

	ClassDB::bind_method(D_METHOD("map_force_update", "map"), &NavigationServer3D::map_force_update);
	ClassDB::bind_method(D_METHOD("map_get_iteration_id", "map"), &NavigationServer3D::map_get_iteration_id);
	ClassDB::bind_method(D_METHOD("map_set_use_async_iterations", "map", "enabled"), &NavigationServer3D::map_set_use_async_iterations);
	ClassDB::bind_method(D_METHOD("map_get_use_async_iterations", "map"), &NavigationServer3D::map_get_use_async_iterations);

	ClassDB::bind_method(D_METHOD("map_get_random_point", "map", "navigation_layers", "uniformly"), &NavigationServer3D::map_get_random_point);

	ClassDB::bind_method(D_METHOD("query_path", "parameters", "result", "callback"), &NavigationServer3D::query_path, DEFVAL(Callable()));

	ClassDB::bind_method(D_METHOD("region_create"), &NavigationServer3D::region_create);
	ClassDB::bind_method(D_METHOD("region_set_enabled", "region", "enabled"), &NavigationServer3D::region_set_enabled);
	ClassDB::bind_method(D_METHOD("region_get_enabled", "region"), &NavigationServer3D::region_get_enabled);
	ClassDB::bind_method(D_METHOD("region_set_use_edge_connections", "region", "enabled"), &NavigationServer3D::region_set_use_edge_connections);
	ClassDB::bind_method(D_METHOD("region_get_use_edge_connections", "region"), &NavigationServer3D::region_get_use_edge_connections);
	ClassDB::bind_method(D_METHOD("region_set_enter_cost", "region", "enter_cost"), &NavigationServer3D::region_set_enter_cost);
	ClassDB::bind_method(D_METHOD("region_get_enter_cost", "region"), &NavigationServer3D::region_get_enter_cost);
	ClassDB::bind_method(D_METHOD("region_set_travel_cost", "region", "travel_cost"), &NavigationServer3D::region_set_travel_cost);
	ClassDB::bind_method(D_METHOD("region_get_travel_cost", "region"), &NavigationServer3D::region_get_travel_cost);
	ClassDB::bind_method(D_METHOD("region_set_owner_id", "region", "owner_id"), &NavigationServer3D::region_set_owner_id);
	ClassDB::bind_method(D_METHOD("region_get_owner_id", "region"), &NavigationServer3D::region_get_owner_id);
	ClassDB::bind_method(D_METHOD("region_owns_point", "region", "point"), &NavigationServer3D::region_owns_point);
	ClassDB::bind_method(D_METHOD("region_set_map", "region", "map"), &NavigationServer3D::region_set_map);
	ClassDB::bind_method(D_METHOD("region_get_map", "region"), &NavigationServer3D::region_get_map);
	ClassDB::bind_method(D_METHOD("region_set_navigation_layers", "region", "navigation_layers"), &NavigationServer3D::region_set_navigation_layers);
	ClassDB::bind_method(D_METHOD("region_get_navigation_layers", "region"), &NavigationServer3D::region_get_navigation_layers);
	ClassDB::bind_method(D_METHOD("region_set_transform", "region", "transform"), &NavigationServer3D::region_set_transform);
	ClassDB::bind_method(D_METHOD("region_get_transform", "region"), &NavigationServer3D::region_get_transform);
	ClassDB::bind_method(D_METHOD("region_set_navigation_mesh", "region", "navigation_mesh"), &NavigationServer3D::region_set_navigation_mesh);
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("region_bake_navigation_mesh", "navigation_mesh", "root_node"), &NavigationServer3D::region_bake_navigation_mesh);
#endif // DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("region_get_connections_count", "region"), &NavigationServer3D::region_get_connections_count);
	ClassDB::bind_method(D_METHOD("region_get_connection_pathway_start", "region", "connection"), &NavigationServer3D::region_get_connection_pathway_start);
	ClassDB::bind_method(D_METHOD("region_get_connection_pathway_end", "region", "connection"), &NavigationServer3D::region_get_connection_pathway_end);
	ClassDB::bind_method(D_METHOD("region_get_closest_point_to_segment", "region", "start", "end", "use_collision"), &NavigationServer3D::region_get_closest_point_to_segment, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("region_get_closest_point", "region", "to_point"), &NavigationServer3D::region_get_closest_point);
	ClassDB::bind_method(D_METHOD("region_get_closest_point_normal", "region", "to_point"), &NavigationServer3D::region_get_closest_point_normal);
	ClassDB::bind_method(D_METHOD("region_get_random_point", "region", "navigation_layers", "uniformly"), &NavigationServer3D::region_get_random_point);
	ClassDB::bind_method(D_METHOD("region_get_bounds", "region"), &NavigationServer3D::region_get_bounds);

	ClassDB::bind_method(D_METHOD("link_create"), &NavigationServer3D::link_create);
	ClassDB::bind_method(D_METHOD("link_set_map", "link", "map"), &NavigationServer3D::link_set_map);
	ClassDB::bind_method(D_METHOD("link_get_map", "link"), &NavigationServer3D::link_get_map);
	ClassDB::bind_method(D_METHOD("link_set_enabled", "link", "enabled"), &NavigationServer3D::link_set_enabled);
	ClassDB::bind_method(D_METHOD("link_get_enabled", "link"), &NavigationServer3D::link_get_enabled);
	ClassDB::bind_method(D_METHOD("link_set_bidirectional", "link", "bidirectional"), &NavigationServer3D::link_set_bidirectional);
	ClassDB::bind_method(D_METHOD("link_is_bidirectional", "link"), &NavigationServer3D::link_is_bidirectional);
	ClassDB::bind_method(D_METHOD("link_set_navigation_layers", "link", "navigation_layers"), &NavigationServer3D::link_set_navigation_layers);
	ClassDB::bind_method(D_METHOD("link_get_navigation_layers", "link"), &NavigationServer3D::link_get_navigation_layers);
	ClassDB::bind_method(D_METHOD("link_set_start_position", "link", "position"), &NavigationServer3D::link_set_start_position);
	ClassDB::bind_method(D_METHOD("link_get_start_position", "link"), &NavigationServer3D::link_get_start_position);
	ClassDB::bind_method(D_METHOD("link_set_end_position", "link", "position"), &NavigationServer3D::link_set_end_position);
	ClassDB::bind_method(D_METHOD("link_get_end_position", "link"), &NavigationServer3D::link_get_end_position);
	ClassDB::bind_method(D_METHOD("link_set_enter_cost", "link", "enter_cost"), &NavigationServer3D::link_set_enter_cost);
	ClassDB::bind_method(D_METHOD("link_get_enter_cost", "link"), &NavigationServer3D::link_get_enter_cost);
	ClassDB::bind_method(D_METHOD("link_set_travel_cost", "link", "travel_cost"), &NavigationServer3D::link_set_travel_cost);
	ClassDB::bind_method(D_METHOD("link_get_travel_cost", "link"), &NavigationServer3D::link_get_travel_cost);
	ClassDB::bind_method(D_METHOD("link_set_owner_id", "link", "owner_id"), &NavigationServer3D::link_set_owner_id);
	ClassDB::bind_method(D_METHOD("link_get_owner_id", "link"), &NavigationServer3D::link_get_owner_id);

	ClassDB::bind_method(D_METHOD("agent_create"), &NavigationServer3D::agent_create);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_enabled", "agent", "enabled"), &NavigationServer3D::agent_set_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("agent_get_avoidance_enabled", "agent"), &NavigationServer3D::agent_get_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("agent_set_use_3d_avoidance", "agent", "enabled"), &NavigationServer3D::agent_set_use_3d_avoidance);
	ClassDB::bind_method(D_METHOD("agent_get_use_3d_avoidance", "agent"), &NavigationServer3D::agent_get_use_3d_avoidance);

	ClassDB::bind_method(D_METHOD("agent_set_map", "agent", "map"), &NavigationServer3D::agent_set_map);
	ClassDB::bind_method(D_METHOD("agent_get_map", "agent"), &NavigationServer3D::agent_get_map);
	ClassDB::bind_method(D_METHOD("agent_set_paused", "agent", "paused"), &NavigationServer3D::agent_set_paused);
	ClassDB::bind_method(D_METHOD("agent_get_paused", "agent"), &NavigationServer3D::agent_get_paused);
	ClassDB::bind_method(D_METHOD("agent_set_neighbor_distance", "agent", "distance"), &NavigationServer3D::agent_set_neighbor_distance);
	ClassDB::bind_method(D_METHOD("agent_get_neighbor_distance", "agent"), &NavigationServer3D::agent_get_neighbor_distance);
	ClassDB::bind_method(D_METHOD("agent_set_max_neighbors", "agent", "count"), &NavigationServer3D::agent_set_max_neighbors);
	ClassDB::bind_method(D_METHOD("agent_get_max_neighbors", "agent"), &NavigationServer3D::agent_get_max_neighbors);
	ClassDB::bind_method(D_METHOD("agent_set_time_horizon_agents", "agent", "time_horizon"), &NavigationServer3D::agent_set_time_horizon_agents);
	ClassDB::bind_method(D_METHOD("agent_get_time_horizon_agents", "agent"), &NavigationServer3D::agent_get_time_horizon_agents);
	ClassDB::bind_method(D_METHOD("agent_set_time_horizon_obstacles", "agent", "time_horizon"), &NavigationServer3D::agent_set_time_horizon_obstacles);
	ClassDB::bind_method(D_METHOD("agent_get_time_horizon_obstacles", "agent"), &NavigationServer3D::agent_get_time_horizon_obstacles);
	ClassDB::bind_method(D_METHOD("agent_set_radius", "agent", "radius"), &NavigationServer3D::agent_set_radius);
	ClassDB::bind_method(D_METHOD("agent_get_radius", "agent"), &NavigationServer3D::agent_get_radius);
	ClassDB::bind_method(D_METHOD("agent_set_height", "agent", "height"), &NavigationServer3D::agent_set_height);
	ClassDB::bind_method(D_METHOD("agent_get_height", "agent"), &NavigationServer3D::agent_get_height);
	ClassDB::bind_method(D_METHOD("agent_set_max_speed", "agent", "max_speed"), &NavigationServer3D::agent_set_max_speed);
	ClassDB::bind_method(D_METHOD("agent_get_max_speed", "agent"), &NavigationServer3D::agent_get_max_speed);
	ClassDB::bind_method(D_METHOD("agent_set_velocity_forced", "agent", "velocity"), &NavigationServer3D::agent_set_velocity_forced);
	ClassDB::bind_method(D_METHOD("agent_set_velocity", "agent", "velocity"), &NavigationServer3D::agent_set_velocity);
	ClassDB::bind_method(D_METHOD("agent_get_velocity", "agent"), &NavigationServer3D::agent_get_velocity);
	ClassDB::bind_method(D_METHOD("agent_set_position", "agent", "position"), &NavigationServer3D::agent_set_position);
	ClassDB::bind_method(D_METHOD("agent_get_position", "agent"), &NavigationServer3D::agent_get_position);
	ClassDB::bind_method(D_METHOD("agent_is_map_changed", "agent"), &NavigationServer3D::agent_is_map_changed);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_callback", "agent", "callback"), &NavigationServer3D::agent_set_avoidance_callback);
	ClassDB::bind_method(D_METHOD("agent_has_avoidance_callback", "agent"), &NavigationServer3D::agent_has_avoidance_callback);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_layers", "agent", "layers"), &NavigationServer3D::agent_set_avoidance_layers);
	ClassDB::bind_method(D_METHOD("agent_get_avoidance_layers", "agent"), &NavigationServer3D::agent_get_avoidance_layers);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_mask", "agent", "mask"), &NavigationServer3D::agent_set_avoidance_mask);
	ClassDB::bind_method(D_METHOD("agent_get_avoidance_mask", "agent"), &NavigationServer3D::agent_get_avoidance_mask);
	ClassDB::bind_method(D_METHOD("agent_set_avoidance_priority", "agent", "priority"), &NavigationServer3D::agent_set_avoidance_priority);
	ClassDB::bind_method(D_METHOD("agent_get_avoidance_priority", "agent"), &NavigationServer3D::agent_get_avoidance_priority);

	ClassDB::bind_method(D_METHOD("obstacle_create"), &NavigationServer3D::obstacle_create);
	ClassDB::bind_method(D_METHOD("obstacle_set_avoidance_enabled", "obstacle", "enabled"), &NavigationServer3D::obstacle_set_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("obstacle_get_avoidance_enabled", "obstacle"), &NavigationServer3D::obstacle_get_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("obstacle_set_use_3d_avoidance", "obstacle", "enabled"), &NavigationServer3D::obstacle_set_use_3d_avoidance);
	ClassDB::bind_method(D_METHOD("obstacle_get_use_3d_avoidance", "obstacle"), &NavigationServer3D::obstacle_get_use_3d_avoidance);
	ClassDB::bind_method(D_METHOD("obstacle_set_map", "obstacle", "map"), &NavigationServer3D::obstacle_set_map);
	ClassDB::bind_method(D_METHOD("obstacle_get_map", "obstacle"), &NavigationServer3D::obstacle_get_map);
	ClassDB::bind_method(D_METHOD("obstacle_set_paused", "obstacle", "paused"), &NavigationServer3D::obstacle_set_paused);
	ClassDB::bind_method(D_METHOD("obstacle_get_paused", "obstacle"), &NavigationServer3D::obstacle_get_paused);
	ClassDB::bind_method(D_METHOD("obstacle_set_radius", "obstacle", "radius"), &NavigationServer3D::obstacle_set_radius);
	ClassDB::bind_method(D_METHOD("obstacle_get_radius", "obstacle"), &NavigationServer3D::obstacle_get_radius);
	ClassDB::bind_method(D_METHOD("obstacle_set_height", "obstacle", "height"), &NavigationServer3D::obstacle_set_height);
	ClassDB::bind_method(D_METHOD("obstacle_get_height", "obstacle"), &NavigationServer3D::obstacle_get_height);
	ClassDB::bind_method(D_METHOD("obstacle_set_velocity", "obstacle", "velocity"), &NavigationServer3D::obstacle_set_velocity);
	ClassDB::bind_method(D_METHOD("obstacle_get_velocity", "obstacle"), &NavigationServer3D::obstacle_get_velocity);
	ClassDB::bind_method(D_METHOD("obstacle_set_position", "obstacle", "position"), &NavigationServer3D::obstacle_set_position);
	ClassDB::bind_method(D_METHOD("obstacle_get_position", "obstacle"), &NavigationServer3D::obstacle_get_position);
	ClassDB::bind_method(D_METHOD("obstacle_set_vertices", "obstacle", "vertices"), &NavigationServer3D::obstacle_set_vertices);
	ClassDB::bind_method(D_METHOD("obstacle_get_vertices", "obstacle"), &NavigationServer3D::obstacle_get_vertices);
	ClassDB::bind_method(D_METHOD("obstacle_set_avoidance_layers", "obstacle", "layers"), &NavigationServer3D::obstacle_set_avoidance_layers);
	ClassDB::bind_method(D_METHOD("obstacle_get_avoidance_layers", "obstacle"), &NavigationServer3D::obstacle_get_avoidance_layers);

#ifndef _3D_DISABLED
	ClassDB::bind_method(D_METHOD("parse_source_geometry_data", "navigation_mesh", "source_geometry_data", "root_node", "callback"), &NavigationServer3D::parse_source_geometry_data, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("bake_from_source_geometry_data", "navigation_mesh", "source_geometry_data", "callback"), &NavigationServer3D::bake_from_source_geometry_data, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("bake_from_source_geometry_data_async", "navigation_mesh", "source_geometry_data", "callback"), &NavigationServer3D::bake_from_source_geometry_data_async, DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("is_baking_navigation_mesh", "navigation_mesh"), &NavigationServer3D::is_baking_navigation_mesh);
#endif // _3D_DISABLED

	ClassDB::bind_method(D_METHOD("source_geometry_parser_create"), &NavigationServer3D::source_geometry_parser_create);
	ClassDB::bind_method(D_METHOD("source_geometry_parser_set_callback", "parser", "callback"), &NavigationServer3D::source_geometry_parser_set_callback);

	ClassDB::bind_method(D_METHOD("simplify_path", "path", "epsilon"), &NavigationServer3D::simplify_path);

	ClassDB::bind_method(D_METHOD("free_rid", "rid"), &NavigationServer3D::free);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &NavigationServer3D::set_active);

	ClassDB::bind_method(D_METHOD("set_debug_enabled", "enabled"), &NavigationServer3D::set_debug_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_enabled"), &NavigationServer3D::get_debug_enabled);

	ADD_SIGNAL(MethodInfo("map_changed", PropertyInfo(Variant::RID, "map")));

	ADD_SIGNAL(MethodInfo("navigation_debug_changed"));
	ADD_SIGNAL(MethodInfo("avoidance_debug_changed"));

	ClassDB::bind_method(D_METHOD("get_process_info", "process_info"), &NavigationServer3D::get_process_info);

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

NavigationServer3D *NavigationServer3D::get_singleton() {
	return singleton;
}

NavigationServer3D::NavigationServer3D() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/2d/default_cell_size", PROPERTY_HINT_RANGE, NavigationDefaults2D::navmesh_cell_size_hint), NavigationDefaults2D::navmesh_cell_size);
	GLOBAL_DEF("navigation/2d/use_edge_connections", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/2d/default_edge_connection_margin", PROPERTY_HINT_RANGE, "0.01,10,0.001,or_greater"), NavigationDefaults2D::edge_connection_margin);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/2d/default_link_connection_radius", PROPERTY_HINT_RANGE, "0.01,10,0.001,or_greater"), NavigationDefaults2D::link_connection_radius);

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/3d/default_cell_size", PROPERTY_HINT_RANGE, NavigationDefaults3D::navmesh_cell_size_hint), NavigationDefaults3D::navmesh_cell_size);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/3d/default_cell_height", PROPERTY_HINT_RANGE, "0.001,100,0.001,or_greater"), NavigationDefaults3D::navmesh_cell_height);
	GLOBAL_DEF("navigation/3d/default_up", Vector3(0, 1, 0));
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "navigation/3d/merge_rasterizer_cell_scale", PROPERTY_HINT_RANGE, "0.001,1,0.001,or_greater"), 1.0);
	GLOBAL_DEF("navigation/3d/use_edge_connections", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/3d/default_edge_connection_margin", PROPERTY_HINT_RANGE, "0.01,10,0.001,or_greater"), NavigationDefaults3D::edge_connection_margin);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::FLOAT, "navigation/3d/default_link_connection_radius", PROPERTY_HINT_RANGE, "0.01,10,0.001,or_greater"), NavigationDefaults3D::link_connection_radius);

	GLOBAL_DEF("navigation/world/map_use_async_iterations", true);

	GLOBAL_DEF("navigation/avoidance/thread_model/avoidance_use_multiple_threads", true);
	GLOBAL_DEF("navigation/avoidance/thread_model/avoidance_use_high_priority_threads", true);

	GLOBAL_DEF("navigation/pathfinding/max_threads", 4);

	GLOBAL_DEF("navigation/baking/use_crash_prevention_checks", true);
	GLOBAL_DEF("navigation/baking/thread_model/baking_use_multiple_threads", true);
	GLOBAL_DEF("navigation/baking/thread_model/baking_use_high_priority_threads", true);

#ifdef DEBUG_ENABLED
	debug_navigation_edge_connection_color = GLOBAL_DEF("debug/shapes/navigation/edge_connection_color", Color(1.0, 0.0, 1.0, 1.0));
	debug_navigation_geometry_edge_color = GLOBAL_DEF("debug/shapes/navigation/geometry_edge_color", Color(0.5, 1.0, 1.0, 1.0));
	debug_navigation_geometry_face_color = GLOBAL_DEF("debug/shapes/navigation/geometry_face_color", Color(0.5, 1.0, 1.0, 0.4));
	debug_navigation_geometry_edge_disabled_color = GLOBAL_DEF("debug/shapes/navigation/geometry_edge_disabled_color", Color(0.5, 0.5, 0.5, 1.0));
	debug_navigation_geometry_face_disabled_color = GLOBAL_DEF("debug/shapes/navigation/geometry_face_disabled_color", Color(0.5, 0.5, 0.5, 0.4));
	debug_navigation_link_connection_color = GLOBAL_DEF("debug/shapes/navigation/link_connection_color", Color(1.0, 0.5, 1.0, 1.0));
	debug_navigation_link_connection_disabled_color = GLOBAL_DEF("debug/shapes/navigation/link_connection_disabled_color", Color(0.5, 0.5, 0.5, 1.0));
	debug_navigation_agent_path_color = GLOBAL_DEF("debug/shapes/navigation/agent_path_color", Color(1.0, 0.0, 0.0, 1.0));

	debug_navigation_enable_edge_connections = GLOBAL_DEF("debug/shapes/navigation/enable_edge_connections", true);
	debug_navigation_enable_edge_connections_xray = GLOBAL_DEF("debug/shapes/navigation/enable_edge_connections_xray", true);
	debug_navigation_enable_edge_lines = GLOBAL_DEF("debug/shapes/navigation/enable_edge_lines", true);
	debug_navigation_enable_edge_lines_xray = GLOBAL_DEF("debug/shapes/navigation/enable_edge_lines_xray", true);
	debug_navigation_enable_geometry_face_random_color = GLOBAL_DEF("debug/shapes/navigation/enable_geometry_face_random_color", true);
	debug_navigation_enable_link_connections = GLOBAL_DEF("debug/shapes/navigation/enable_link_connections", true);
	debug_navigation_enable_link_connections_xray = GLOBAL_DEF("debug/shapes/navigation/enable_link_connections_xray", true);

	debug_navigation_enable_agent_paths = GLOBAL_DEF("debug/shapes/navigation/enable_agent_paths", true);
	debug_navigation_enable_agent_paths_xray = GLOBAL_DEF("debug/shapes/navigation/enable_agent_paths_xray", true);
	debug_navigation_agent_path_point_size = GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "debug/shapes/navigation/agent_path_point_size", PROPERTY_HINT_RANGE, "0.01,10,0.001,or_greater"), 4.0);

	debug_navigation_avoidance_agents_radius_color = GLOBAL_DEF("debug/shapes/avoidance/agents_radius_color", Color(1.0, 1.0, 0.0, 0.25));
	debug_navigation_avoidance_obstacles_radius_color = GLOBAL_DEF("debug/shapes/avoidance/obstacles_radius_color", Color(1.0, 0.5, 0.0, 0.25));
	debug_navigation_avoidance_static_obstacle_pushin_face_color = GLOBAL_DEF("debug/shapes/avoidance/obstacles_static_face_pushin_color", Color(1.0, 0.0, 0.0, 0.0));
	debug_navigation_avoidance_static_obstacle_pushin_edge_color = GLOBAL_DEF("debug/shapes/avoidance/obstacles_static_edge_pushin_color", Color(1.0, 0.0, 0.0, 1.0));
	debug_navigation_avoidance_static_obstacle_pushout_face_color = GLOBAL_DEF("debug/shapes/avoidance/obstacles_static_face_pushout_color", Color(1.0, 1.0, 0.0, 0.5));
	debug_navigation_avoidance_static_obstacle_pushout_edge_color = GLOBAL_DEF("debug/shapes/avoidance/obstacles_static_edge_pushout_color", Color(1.0, 1.0, 0.0, 1.0));
	debug_navigation_avoidance_enable_agents_radius = GLOBAL_DEF("debug/shapes/avoidance/enable_agents_radius", true);
	debug_navigation_avoidance_enable_obstacles_radius = GLOBAL_DEF("debug/shapes/avoidance/enable_obstacles_radius", true);
	debug_navigation_avoidance_enable_obstacles_static = GLOBAL_DEF("debug/shapes/avoidance/enable_obstacles_static", true);

	if (Engine::get_singleton()->is_editor_hint()) {
		// enable NavigationServer3D when in Editor or else navigation mesh edge connections are invisible
		// on runtime tests SceneTree has "Visible Navigation" set and main iteration takes care of this
		set_debug_enabled(true);
		set_debug_navigation_enabled(true);
		set_debug_avoidance_enabled(true);
	}
#endif // DEBUG_ENABLED
}

NavigationServer3D::~NavigationServer3D() {
	singleton = nullptr;

	RWLockWrite write_lock(geometry_parser_rwlock);
	for (NavMeshGeometryParser3D *parser : generator_parsers) {
		geometry_parser_owner.free(parser->self);
	}
	generator_parsers.clear();
}

RID NavigationServer3D::source_geometry_parser_create() {
	RWLockWrite write_lock(geometry_parser_rwlock);

	RID rid = geometry_parser_owner.make_rid();

	NavMeshGeometryParser3D *parser = geometry_parser_owner.get_or_null(rid);
	parser->self = rid;

	generator_parsers.push_back(parser);

	return rid;
}

void NavigationServer3D::free(RID p_object) {
	if (!geometry_parser_owner.owns(p_object)) {
		return;
	}
	RWLockWrite write_lock(geometry_parser_rwlock);

	NavMeshGeometryParser3D *parser = geometry_parser_owner.get_or_null(p_object);
	ERR_FAIL_NULL(parser);

	generator_parsers.erase(parser);
	geometry_parser_owner.free(parser->self);
}

void NavigationServer3D::source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) {
	RWLockWrite write_lock(geometry_parser_rwlock);

	NavMeshGeometryParser3D *parser = geometry_parser_owner.get_or_null(p_parser);
	ERR_FAIL_NULL(parser);

	parser->callback = p_callback;
}

void NavigationServer3D::set_debug_enabled(bool p_enabled) {
#ifdef DEBUG_ENABLED
	if (debug_enabled != p_enabled) {
		debug_dirty = true;
	}

	debug_enabled = p_enabled;

	if (debug_dirty) {
		navigation_debug_dirty = true;
		callable_mp(this, &NavigationServer3D::_emit_navigation_debug_changed_signal).call_deferred();

		avoidance_debug_dirty = true;
		callable_mp(this, &NavigationServer3D::_emit_avoidance_debug_changed_signal).call_deferred();
	}
#endif // DEBUG_ENABLED
}

bool NavigationServer3D::get_debug_enabled() const {
	return debug_enabled;
}

#ifdef DEBUG_ENABLED
void NavigationServer3D::_emit_navigation_debug_changed_signal() {
	if (navigation_debug_dirty) {
		navigation_debug_dirty = false;
		emit_signal(SNAME("navigation_debug_changed"));
	}
}

void NavigationServer3D::_emit_avoidance_debug_changed_signal() {
	if (avoidance_debug_dirty) {
		avoidance_debug_dirty = false;
		emit_signal(SNAME("avoidance_debug_changed"));
	}
}
#endif // DEBUG_ENABLED

#ifdef DEBUG_ENABLED
Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_geometry_face_material() {
	if (debug_navigation_geometry_face_material.is_valid()) {
		return debug_navigation_geometry_face_material;
	}

	bool enabled_geometry_face_random_color = get_debug_navigation_enable_geometry_face_random_color();

	Ref<StandardMaterial3D> face_material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	face_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	face_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	face_material->set_albedo(get_debug_navigation_geometry_face_color());
	face_material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
	face_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	if (enabled_geometry_face_random_color) {
		face_material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		face_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	}

	debug_navigation_geometry_face_material = face_material;

	return debug_navigation_geometry_face_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_geometry_edge_material() {
	if (debug_navigation_geometry_edge_material.is_valid()) {
		return debug_navigation_geometry_edge_material;
	}

	bool enabled_edge_lines_xray = get_debug_navigation_enable_edge_lines_xray();

	Ref<StandardMaterial3D> line_material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	line_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	line_material->set_albedo(get_debug_navigation_geometry_edge_color());
	line_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	if (enabled_edge_lines_xray) {
		line_material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	}

	debug_navigation_geometry_edge_material = line_material;

	return debug_navigation_geometry_edge_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_geometry_face_disabled_material() {
	if (debug_navigation_geometry_face_disabled_material.is_valid()) {
		return debug_navigation_geometry_face_disabled_material;
	}

	Ref<StandardMaterial3D> face_disabled_material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	face_disabled_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	face_disabled_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	face_disabled_material->set_albedo(get_debug_navigation_geometry_face_disabled_color());
	face_disabled_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);

	debug_navigation_geometry_face_disabled_material = face_disabled_material;

	return debug_navigation_geometry_face_disabled_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_geometry_edge_disabled_material() {
	if (debug_navigation_geometry_edge_disabled_material.is_valid()) {
		return debug_navigation_geometry_edge_disabled_material;
	}

	bool enabled_edge_lines_xray = get_debug_navigation_enable_edge_lines_xray();

	Ref<StandardMaterial3D> line_disabled_material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	line_disabled_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	line_disabled_material->set_albedo(get_debug_navigation_geometry_edge_disabled_color());
	line_disabled_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	if (enabled_edge_lines_xray) {
		line_disabled_material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	}

	debug_navigation_geometry_edge_disabled_material = line_disabled_material;

	return debug_navigation_geometry_edge_disabled_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_edge_connections_material() {
	if (debug_navigation_edge_connections_material.is_valid()) {
		return debug_navigation_edge_connections_material;
	}

	bool enabled_edge_connections_xray = get_debug_navigation_enable_edge_connections_xray();

	Ref<StandardMaterial3D> edge_connections_material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	edge_connections_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	edge_connections_material->set_albedo(get_debug_navigation_edge_connection_color());
	edge_connections_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	if (enabled_edge_connections_xray) {
		edge_connections_material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	}
	edge_connections_material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MAX - 2);

	debug_navigation_edge_connections_material = edge_connections_material;

	return debug_navigation_edge_connections_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_link_connections_material() {
	if (debug_navigation_link_connections_material.is_valid()) {
		return debug_navigation_link_connections_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	material->set_albedo(debug_navigation_link_connection_color);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	if (debug_navigation_enable_link_connections_xray) {
		material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	}
	material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MAX - 2);

	debug_navigation_link_connections_material = material;
	return debug_navigation_link_connections_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_link_connections_disabled_material() {
	if (debug_navigation_link_connections_disabled_material.is_valid()) {
		return debug_navigation_link_connections_disabled_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	material->set_albedo(debug_navigation_link_connection_disabled_color);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	if (debug_navigation_enable_link_connections_xray) {
		material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	}
	material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MAX - 2);

	debug_navigation_link_connections_disabled_material = material;
	return debug_navigation_link_connections_disabled_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_agent_path_line_material() {
	if (debug_navigation_agent_path_line_material.is_valid()) {
		return debug_navigation_agent_path_line_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);

	material->set_albedo(debug_navigation_agent_path_color);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	if (debug_navigation_enable_agent_paths_xray) {
		material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	}
	material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MAX - 2);

	debug_navigation_agent_path_line_material = material;
	return debug_navigation_agent_path_line_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_agent_path_point_material() {
	if (debug_navigation_agent_path_point_material.is_valid()) {
		return debug_navigation_agent_path_point_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_albedo(debug_navigation_agent_path_color);
	material->set_flag(StandardMaterial3D::FLAG_USE_POINT_SIZE, true);
	material->set_point_size(debug_navigation_agent_path_point_size);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	if (debug_navigation_enable_agent_paths_xray) {
		material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	}
	material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MAX - 2);

	debug_navigation_agent_path_point_material = material;
	return debug_navigation_agent_path_point_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_avoidance_agents_radius_material() {
	if (debug_navigation_avoidance_agents_radius_material.is_valid()) {
		return debug_navigation_avoidance_agents_radius_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
	material->set_albedo(debug_navigation_avoidance_agents_radius_color);
	material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 2);

	debug_navigation_avoidance_agents_radius_material = material;
	return debug_navigation_avoidance_agents_radius_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_avoidance_obstacles_radius_material() {
	if (debug_navigation_avoidance_obstacles_radius_material.is_valid()) {
		return debug_navigation_avoidance_obstacles_radius_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
	material->set_albedo(debug_navigation_avoidance_obstacles_radius_color);
	material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 2);

	debug_navigation_avoidance_obstacles_radius_material = material;
	return debug_navigation_avoidance_obstacles_radius_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_avoidance_static_obstacle_pushin_face_material() {
	if (debug_navigation_avoidance_static_obstacle_pushin_face_material.is_valid()) {
		return debug_navigation_avoidance_static_obstacle_pushin_face_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
	material->set_albedo(debug_navigation_avoidance_static_obstacle_pushin_face_color);
	material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 2);

	debug_navigation_avoidance_static_obstacle_pushin_face_material = material;
	return debug_navigation_avoidance_static_obstacle_pushin_face_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_avoidance_static_obstacle_pushout_face_material() {
	if (debug_navigation_avoidance_static_obstacle_pushout_face_material.is_valid()) {
		return debug_navigation_avoidance_static_obstacle_pushout_face_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
	material->set_albedo(debug_navigation_avoidance_static_obstacle_pushout_face_color);
	material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 2);

	debug_navigation_avoidance_static_obstacle_pushout_face_material = material;
	return debug_navigation_avoidance_static_obstacle_pushout_face_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_avoidance_static_obstacle_pushin_edge_material() {
	if (debug_navigation_avoidance_static_obstacle_pushin_edge_material.is_valid()) {
		return debug_navigation_avoidance_static_obstacle_pushin_edge_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	//material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	//material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
	material->set_albedo(debug_navigation_avoidance_static_obstacle_pushin_edge_color);
	//material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 2);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);

	debug_navigation_avoidance_static_obstacle_pushin_edge_material = material;
	return debug_navigation_avoidance_static_obstacle_pushin_edge_material;
}

Ref<StandardMaterial3D> NavigationServer3D::get_debug_navigation_avoidance_static_obstacle_pushout_edge_material() {
	if (debug_navigation_avoidance_static_obstacle_pushout_edge_material.is_valid()) {
		return debug_navigation_avoidance_static_obstacle_pushout_edge_material;
	}

	Ref<StandardMaterial3D> material = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
	material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	///material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	//material->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
	material->set_albedo(debug_navigation_avoidance_static_obstacle_pushout_edge_color);
	//material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 2);
	material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);

	debug_navigation_avoidance_static_obstacle_pushout_edge_material = material;
	return debug_navigation_avoidance_static_obstacle_pushout_edge_material;
}

void NavigationServer3D::set_debug_navigation_edge_connection_color(const Color &p_color) {
	debug_navigation_edge_connection_color = p_color;
	if (debug_navigation_edge_connections_material.is_valid()) {
		debug_navigation_edge_connections_material->set_albedo(debug_navigation_edge_connection_color);
	}
}

Color NavigationServer3D::get_debug_navigation_edge_connection_color() const {
	return debug_navigation_edge_connection_color;
}

void NavigationServer3D::set_debug_navigation_geometry_edge_color(const Color &p_color) {
	debug_navigation_geometry_edge_color = p_color;
	if (debug_navigation_geometry_edge_material.is_valid()) {
		debug_navigation_geometry_edge_material->set_albedo(debug_navigation_geometry_edge_color);
	}
}

Color NavigationServer3D::get_debug_navigation_geometry_edge_color() const {
	return debug_navigation_geometry_edge_color;
}

void NavigationServer3D::set_debug_navigation_geometry_face_color(const Color &p_color) {
	debug_navigation_geometry_face_color = p_color;
	if (debug_navigation_geometry_face_material.is_valid()) {
		debug_navigation_geometry_face_material->set_albedo(debug_navigation_geometry_face_color);
	}
}

Color NavigationServer3D::get_debug_navigation_geometry_face_color() const {
	return debug_navigation_geometry_face_color;
}

void NavigationServer3D::set_debug_navigation_geometry_edge_disabled_color(const Color &p_color) {
	debug_navigation_geometry_edge_disabled_color = p_color;
	if (debug_navigation_geometry_edge_disabled_material.is_valid()) {
		debug_navigation_geometry_edge_disabled_material->set_albedo(debug_navigation_geometry_edge_disabled_color);
	}
}

Color NavigationServer3D::get_debug_navigation_geometry_edge_disabled_color() const {
	return debug_navigation_geometry_edge_disabled_color;
}

void NavigationServer3D::set_debug_navigation_geometry_face_disabled_color(const Color &p_color) {
	debug_navigation_geometry_face_disabled_color = p_color;
	if (debug_navigation_geometry_face_disabled_material.is_valid()) {
		debug_navigation_geometry_face_disabled_material->set_albedo(debug_navigation_geometry_face_disabled_color);
	}
}

Color NavigationServer3D::get_debug_navigation_geometry_face_disabled_color() const {
	return debug_navigation_geometry_face_disabled_color;
}

void NavigationServer3D::set_debug_navigation_link_connection_color(const Color &p_color) {
	debug_navigation_link_connection_color = p_color;
	if (debug_navigation_link_connections_material.is_valid()) {
		debug_navigation_link_connections_material->set_albedo(debug_navigation_link_connection_color);
	}
}

Color NavigationServer3D::get_debug_navigation_link_connection_color() const {
	return debug_navigation_link_connection_color;
}

void NavigationServer3D::set_debug_navigation_link_connection_disabled_color(const Color &p_color) {
	debug_navigation_link_connection_disabled_color = p_color;
	if (debug_navigation_link_connections_disabled_material.is_valid()) {
		debug_navigation_link_connections_disabled_material->set_albedo(debug_navigation_link_connection_disabled_color);
	}
}

Color NavigationServer3D::get_debug_navigation_link_connection_disabled_color() const {
	return debug_navigation_link_connection_disabled_color;
}

void NavigationServer3D::set_debug_navigation_agent_path_point_size(real_t p_point_size) {
	debug_navigation_agent_path_point_size = MAX(0.1, p_point_size);
	if (debug_navigation_agent_path_point_material.is_valid()) {
		debug_navigation_agent_path_point_material->set_point_size(debug_navigation_agent_path_point_size);
	}
}

real_t NavigationServer3D::get_debug_navigation_agent_path_point_size() const {
	return debug_navigation_agent_path_point_size;
}

void NavigationServer3D::set_debug_navigation_agent_path_color(const Color &p_color) {
	debug_navigation_agent_path_color = p_color;
	if (debug_navigation_agent_path_line_material.is_valid()) {
		debug_navigation_agent_path_line_material->set_albedo(debug_navigation_agent_path_color);
	}
	if (debug_navigation_agent_path_point_material.is_valid()) {
		debug_navigation_agent_path_point_material->set_albedo(debug_navigation_agent_path_color);
	}
}

Color NavigationServer3D::get_debug_navigation_agent_path_color() const {
	return debug_navigation_agent_path_color;
}

void NavigationServer3D::set_debug_navigation_enable_edge_connections(const bool p_value) {
	debug_navigation_enable_edge_connections = p_value;
	navigation_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_navigation_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_navigation_enable_edge_connections() const {
	return debug_navigation_enable_edge_connections;
}

void NavigationServer3D::set_debug_navigation_enable_edge_connections_xray(const bool p_value) {
	debug_navigation_enable_edge_connections_xray = p_value;
	if (debug_navigation_edge_connections_material.is_valid()) {
		debug_navigation_edge_connections_material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, debug_navigation_enable_edge_connections_xray);
	}
}

bool NavigationServer3D::get_debug_navigation_enable_edge_connections_xray() const {
	return debug_navigation_enable_edge_connections_xray;
}

void NavigationServer3D::set_debug_navigation_enable_edge_lines(const bool p_value) {
	debug_navigation_enable_edge_lines = p_value;
	navigation_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_navigation_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_navigation_enable_edge_lines() const {
	return debug_navigation_enable_edge_lines;
}

void NavigationServer3D::set_debug_navigation_enable_edge_lines_xray(const bool p_value) {
	debug_navigation_enable_edge_lines_xray = p_value;
	if (debug_navigation_geometry_edge_material.is_valid()) {
		debug_navigation_geometry_edge_material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, debug_navigation_enable_edge_lines_xray);
	}
}

bool NavigationServer3D::get_debug_navigation_enable_edge_lines_xray() const {
	return debug_navigation_enable_edge_lines_xray;
}

void NavigationServer3D::set_debug_navigation_enable_geometry_face_random_color(const bool p_value) {
	debug_navigation_enable_geometry_face_random_color = p_value;
	navigation_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_navigation_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_navigation_enable_geometry_face_random_color() const {
	return debug_navigation_enable_geometry_face_random_color;
}

void NavigationServer3D::set_debug_navigation_enable_link_connections(const bool p_value) {
	debug_navigation_enable_link_connections = p_value;
	navigation_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_navigation_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_navigation_enable_link_connections() const {
	return debug_navigation_enable_link_connections;
}

void NavigationServer3D::set_debug_navigation_enable_link_connections_xray(const bool p_value) {
	debug_navigation_enable_link_connections_xray = p_value;
	if (debug_navigation_link_connections_material.is_valid()) {
		debug_navigation_link_connections_material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, debug_navigation_enable_link_connections_xray);
	}
}

bool NavigationServer3D::get_debug_navigation_enable_link_connections_xray() const {
	return debug_navigation_enable_link_connections_xray;
}

void NavigationServer3D::set_debug_navigation_avoidance_enable_agents_radius(const bool p_value) {
	debug_navigation_avoidance_enable_agents_radius = p_value;
	avoidance_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_avoidance_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_navigation_avoidance_enable_agents_radius() const {
	return debug_navigation_avoidance_enable_agents_radius;
}

void NavigationServer3D::set_debug_navigation_avoidance_enable_obstacles_radius(const bool p_value) {
	debug_navigation_avoidance_enable_obstacles_radius = p_value;
	avoidance_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_avoidance_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_navigation_avoidance_enable_obstacles_radius() const {
	return debug_navigation_avoidance_enable_obstacles_radius;
}

void NavigationServer3D::set_debug_navigation_avoidance_enable_obstacles_static(const bool p_value) {
	debug_navigation_avoidance_enable_obstacles_static = p_value;
	avoidance_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_avoidance_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_navigation_avoidance_enable_obstacles_static() const {
	return debug_navigation_avoidance_enable_obstacles_static;
}

void NavigationServer3D::set_debug_navigation_avoidance_agents_radius_color(const Color &p_color) {
	debug_navigation_avoidance_agents_radius_color = p_color;
	if (debug_navigation_avoidance_agents_radius_material.is_valid()) {
		debug_navigation_avoidance_agents_radius_material->set_albedo(debug_navigation_avoidance_agents_radius_color);
	}
}

Color NavigationServer3D::get_debug_navigation_avoidance_agents_radius_color() const {
	return debug_navigation_avoidance_agents_radius_color;
}

void NavigationServer3D::set_debug_navigation_avoidance_obstacles_radius_color(const Color &p_color) {
	debug_navigation_avoidance_obstacles_radius_color = p_color;
	if (debug_navigation_avoidance_obstacles_radius_material.is_valid()) {
		debug_navigation_avoidance_obstacles_radius_material->set_albedo(debug_navigation_avoidance_obstacles_radius_color);
	}
}

Color NavigationServer3D::get_debug_navigation_avoidance_obstacles_radius_color() const {
	return debug_navigation_avoidance_obstacles_radius_color;
}

void NavigationServer3D::set_debug_navigation_avoidance_static_obstacle_pushin_face_color(const Color &p_color) {
	debug_navigation_avoidance_static_obstacle_pushin_face_color = p_color;
	if (debug_navigation_avoidance_static_obstacle_pushin_face_material.is_valid()) {
		debug_navigation_avoidance_static_obstacle_pushin_face_material->set_albedo(debug_navigation_avoidance_static_obstacle_pushin_face_color);
	}
}

Color NavigationServer3D::get_debug_navigation_avoidance_static_obstacle_pushin_face_color() const {
	return debug_navigation_avoidance_static_obstacle_pushin_face_color;
}

void NavigationServer3D::set_debug_navigation_avoidance_static_obstacle_pushout_face_color(const Color &p_color) {
	debug_navigation_avoidance_static_obstacle_pushout_face_color = p_color;
	if (debug_navigation_avoidance_static_obstacle_pushout_face_material.is_valid()) {
		debug_navigation_avoidance_static_obstacle_pushout_face_material->set_albedo(debug_navigation_avoidance_static_obstacle_pushout_face_color);
	}
}

Color NavigationServer3D::get_debug_navigation_avoidance_static_obstacle_pushout_face_color() const {
	return debug_navigation_avoidance_static_obstacle_pushout_face_color;
}

void NavigationServer3D::set_debug_navigation_avoidance_static_obstacle_pushin_edge_color(const Color &p_color) {
	debug_navigation_avoidance_static_obstacle_pushin_edge_color = p_color;
	if (debug_navigation_avoidance_static_obstacle_pushin_edge_material.is_valid()) {
		debug_navigation_avoidance_static_obstacle_pushin_edge_material->set_albedo(debug_navigation_avoidance_static_obstacle_pushin_edge_color);
	}
}

Color NavigationServer3D::get_debug_navigation_avoidance_static_obstacle_pushin_edge_color() const {
	return debug_navigation_avoidance_static_obstacle_pushin_edge_color;
}

void NavigationServer3D::set_debug_navigation_avoidance_static_obstacle_pushout_edge_color(const Color &p_color) {
	debug_navigation_avoidance_static_obstacle_pushout_edge_color = p_color;
	if (debug_navigation_avoidance_static_obstacle_pushout_edge_material.is_valid()) {
		debug_navigation_avoidance_static_obstacle_pushout_edge_material->set_albedo(debug_navigation_avoidance_static_obstacle_pushout_edge_color);
	}
}

Color NavigationServer3D::get_debug_navigation_avoidance_static_obstacle_pushout_edge_color() const {
	return debug_navigation_avoidance_static_obstacle_pushout_edge_color;
}

void NavigationServer3D::set_debug_navigation_enable_agent_paths(const bool p_value) {
	if (debug_navigation_enable_agent_paths != p_value) {
		debug_dirty = true;
	}

	debug_navigation_enable_agent_paths = p_value;

	if (debug_dirty) {
		callable_mp(this, &NavigationServer3D::_emit_navigation_debug_changed_signal).call_deferred();
	}
}

bool NavigationServer3D::get_debug_navigation_enable_agent_paths() const {
	return debug_navigation_enable_agent_paths;
}

void NavigationServer3D::set_debug_navigation_enable_agent_paths_xray(const bool p_value) {
	debug_navigation_enable_agent_paths_xray = p_value;
	if (debug_navigation_agent_path_line_material.is_valid()) {
		debug_navigation_agent_path_line_material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, debug_navigation_enable_agent_paths_xray);
	}
	if (debug_navigation_agent_path_point_material.is_valid()) {
		debug_navigation_agent_path_point_material->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, debug_navigation_enable_agent_paths_xray);
	}
}

bool NavigationServer3D::get_debug_navigation_enable_agent_paths_xray() const {
	return debug_navigation_enable_agent_paths_xray;
}

void NavigationServer3D::set_debug_navigation_enabled(bool p_enabled) {
	debug_navigation_enabled = p_enabled;
	navigation_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_navigation_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_navigation_enabled() const {
	return debug_navigation_enabled;
}

void NavigationServer3D::set_debug_avoidance_enabled(bool p_enabled) {
	debug_avoidance_enabled = p_enabled;
	avoidance_debug_dirty = true;
	callable_mp(this, &NavigationServer3D::_emit_avoidance_debug_changed_signal).call_deferred();
}

bool NavigationServer3D::get_debug_avoidance_enabled() const {
	return debug_avoidance_enabled;
}

#endif // DEBUG_ENABLED

///////////////////////////////////////////////////////

static NavigationServer3D *navigation_server_3d = nullptr;

NavigationServer3DCallback NavigationServer3DManager::create_callback = nullptr;

void NavigationServer3DManager::set_default_server(NavigationServer3DCallback p_callback) {
	create_callback = p_callback;
}

NavigationServer3D *NavigationServer3DManager::new_default_server() {
	if (create_callback == nullptr) {
		return nullptr;
	}

	return create_callback();
}

void NavigationServer3DManager::initialize_server() {
	ERR_FAIL_COND(navigation_server_3d != nullptr);

	// Init 3D Navigation Server
	navigation_server_3d = NavigationServer3DManager::new_default_server();

	// Fall back to dummy if no default server has been registered.
	if (!navigation_server_3d) {
		WARN_VERBOSE("Failed to initialize NavigationServer3D. Fall back to dummy server.");
		navigation_server_3d = memnew(NavigationServer3DDummy);
	}

	// Should be impossible, but make sure it's not null.
	ERR_FAIL_NULL_MSG(navigation_server_3d, "Failed to initialize NavigationServer3D.");
	navigation_server_3d->init();
}

void NavigationServer3DManager::finalize_server() {
	ERR_FAIL_NULL(navigation_server_3d);
	navigation_server_3d->finish();
	memdelete(navigation_server_3d);
	navigation_server_3d = nullptr;
}
