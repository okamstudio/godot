/**************************************************************************/
/*  godot_navigation_server_3d.cpp                                        */
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

#include "godot_navigation_server_3d.h"

#include "core/os/mutex.h"
#include "scene/main/node.h"

#ifndef _3D_DISABLED
#include "nav_mesh_generator_3d.h"
#endif // _3D_DISABLED

using namespace NavigationUtilities;

/// Creates a struct for each function and a function that once called creates
/// an instance of that struct with the submitted parameters.
/// Then, that struct is stored in an array; the `sync` function consume that array.

#define COMMAND_1(F_NAME, T_0, D_0)                                   \
	struct MERGE(F_NAME, _command_3d) : public SetCommand3D {         \
		T_0 d_0;                                                      \
		MERGE(F_NAME, _command_3d)                                    \
		(T_0 p_d_0) :                                                 \
				d_0(p_d_0) {}                                         \
		virtual void exec(GodotNavigationServer3D *server) override { \
			server->MERGE(_cmd_, F_NAME)(d_0);                        \
		}                                                             \
	};                                                                \
	void GodotNavigationServer3D::F_NAME(T_0 D_0) {                   \
		auto cmd = memnew(MERGE(F_NAME, _command_3d)(                 \
				D_0));                                                \
		add_command(cmd);                                             \
	}                                                                 \
	void GodotNavigationServer3D::MERGE(_cmd_, F_NAME)(T_0 D_0)

#define COMMAND_2(F_NAME, T_0, D_0, T_1, D_1)                         \
	struct MERGE(F_NAME, _command_3d) : public SetCommand3D {         \
		T_0 d_0;                                                      \
		T_1 d_1;                                                      \
		MERGE(F_NAME, _command_3d)                                    \
		(                                                             \
				T_0 p_d_0,                                            \
				T_1 p_d_1) :                                          \
				d_0(p_d_0),                                           \
				d_1(p_d_1) {}                                         \
		virtual void exec(GodotNavigationServer3D *server) override { \
			server->MERGE(_cmd_, F_NAME)(d_0, d_1);                   \
		}                                                             \
	};                                                                \
	void GodotNavigationServer3D::F_NAME(T_0 D_0, T_1 D_1) {          \
		auto cmd = memnew(MERGE(F_NAME, _command_3d)(                 \
				D_0,                                                  \
				D_1));                                                \
		add_command(cmd);                                             \
	}                                                                 \
	void GodotNavigationServer3D::MERGE(_cmd_, F_NAME)(T_0 D_0, T_1 D_1)

GodotNavigationServer3D::GodotNavigationServer3D() {}

GodotNavigationServer3D::~GodotNavigationServer3D() {
	flush_queries();
}

void GodotNavigationServer3D::add_command(SetCommand3D *command) {
	MutexLock lock(commands_mutex);

	commands.push_back(command);
}

TypedArray<RID> GodotNavigationServer3D::get_maps() const {
	TypedArray<RID> all_map_rids;
	List<RID> maps_owned;
	map_owner.get_owned_list(&maps_owned);
	if (maps_owned.size()) {
		for (const RID &E : maps_owned) {
			all_map_rids.push_back(E);
		}
	}
	return all_map_rids;
}

RID GodotNavigationServer3D::map_create() {
	MutexLock lock(operations_mutex);

	RID rid = map_owner.make_rid();
	NavMap3D *map = map_owner.get_or_null(rid);
	map->set_self(rid);
	return rid;
}

COMMAND_2(map_set_active, RID, p_map, bool, p_active) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	if (p_active) {
		if (!map_is_active(p_map)) {
			active_maps.push_back(map);
			active_maps_iteration_id.push_back(map->get_iteration_id());
		}
	} else {
		int map_index = active_maps.find(map);
		ERR_FAIL_COND(map_index < 0);
		active_maps.remove_at(map_index);
		active_maps_iteration_id.remove_at(map_index);
	}
}

bool GodotNavigationServer3D::map_is_active(RID p_map) const {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, false);

	return active_maps.has(map);
}

COMMAND_2(map_set_up, RID, p_map, Vector3, p_up) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_up(p_up);
}

Vector3 GodotNavigationServer3D::map_get_up(RID p_map) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector3());

	return map->get_up();
}

COMMAND_2(map_set_cell_size, RID, p_map, real_t, p_cell_size) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_cell_size(p_cell_size);
}

real_t GodotNavigationServer3D::map_get_cell_size(RID p_map) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_cell_size();
}

COMMAND_2(map_set_cell_height, RID, p_map, real_t, p_cell_height) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_cell_height(p_cell_height);
}

real_t GodotNavigationServer3D::map_get_cell_height(RID p_map) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_cell_height();
}

COMMAND_2(map_set_merge_rasterizer_cell_scale, RID, p_map, float, p_value) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_merge_rasterizer_cell_scale(p_value);
}

float GodotNavigationServer3D::map_get_merge_rasterizer_cell_scale(RID p_map) const {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, false);

	return map->get_merge_rasterizer_cell_scale();
}

COMMAND_2(map_set_use_edge_connections, RID, p_map, bool, p_enabled) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_use_edge_connections(p_enabled);
}

bool GodotNavigationServer3D::map_get_use_edge_connections(RID p_map) const {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, false);

	return map->get_use_edge_connections();
}

COMMAND_2(map_set_edge_connection_margin, RID, p_map, real_t, p_connection_margin) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_edge_connection_margin(p_connection_margin);
}

real_t GodotNavigationServer3D::map_get_edge_connection_margin(RID p_map) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_edge_connection_margin();
}

COMMAND_2(map_set_link_connection_radius, RID, p_map, real_t, p_connection_radius) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_link_connection_radius(p_connection_radius);
}

real_t GodotNavigationServer3D::map_get_link_connection_radius(RID p_map) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_link_connection_radius();
}

Vector<Vector3> GodotNavigationServer3D::map_get_path(RID p_map, Vector3 p_origin, Vector3 p_destination, bool p_optimize, uint32_t p_navigation_layers) {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector<Vector3>());

	Ref<NavigationPathQueryParameters3D> query_parameters;
	query_parameters.instantiate();

	query_parameters->set_map(p_map);
	query_parameters->set_start_position(p_origin);
	query_parameters->set_target_position(p_destination);
	query_parameters->set_navigation_layers(p_navigation_layers);
	query_parameters->set_pathfinding_algorithm(NavigationPathQueryParameters3D::PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR);
	query_parameters->set_path_postprocessing(NavigationPathQueryParameters3D::PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL);
	if (!p_optimize) {
		query_parameters->set_path_postprocessing(NavigationPathQueryParameters3D::PATH_POSTPROCESSING_EDGECENTERED);
	}

	Ref<NavigationPathQueryResult3D> query_result;
	query_result.instantiate();

	query_path(query_parameters, query_result);

	return query_result->get_path();
}

Vector3 GodotNavigationServer3D::map_get_closest_point_to_segment(RID p_map, const Vector3 &p_from, const Vector3 &p_to, const bool p_use_collision) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector3());

	return map->get_closest_point_to_segment(p_from, p_to, p_use_collision);
}

Vector3 GodotNavigationServer3D::map_get_closest_point(RID p_map, const Vector3 &p_point) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector3());

	return map->get_closest_point(p_point);
}

Vector3 GodotNavigationServer3D::map_get_closest_point_normal(RID p_map, const Vector3 &p_point) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector3());

	return map->get_closest_point_normal(p_point);
}

RID GodotNavigationServer3D::map_get_closest_point_owner(RID p_map, const Vector3 &p_point) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, RID());

	return map->get_closest_point_owner(p_point);
}

TypedArray<RID> GodotNavigationServer3D::map_get_links(RID p_map) const {
	TypedArray<RID> link_rids;
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, link_rids);

	const LocalVector<NavLink3D *> &links = map->get_links();
	link_rids.resize(links.size());

	for (uint32_t i = 0; i < links.size(); i++) {
		link_rids[i] = links[i]->get_self();
	}
	return link_rids;
}

TypedArray<RID> GodotNavigationServer3D::map_get_regions(RID p_map) const {
	TypedArray<RID> regions_rids;
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, regions_rids);

	const LocalVector<NavRegion3D *> &regions = map->get_regions();
	regions_rids.resize(regions.size());

	for (uint32_t i = 0; i < regions.size(); i++) {
		regions_rids[i] = regions[i]->get_self();
	}
	return regions_rids;
}

TypedArray<RID> GodotNavigationServer3D::map_get_agents(RID p_map) const {
	TypedArray<RID> agents_rids;
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, agents_rids);

	const LocalVector<NavAgent3D *> &agents = map->get_agents();
	agents_rids.resize(agents.size());

	for (uint32_t i = 0; i < agents.size(); i++) {
		agents_rids[i] = agents[i]->get_self();
	}
	return agents_rids;
}

TypedArray<RID> GodotNavigationServer3D::map_get_obstacles(RID p_map) const {
	TypedArray<RID> obstacles_rids;
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, obstacles_rids);
	const LocalVector<NavObstacle3D *> obstacles = map->get_obstacles();
	obstacles_rids.resize(obstacles.size());
	for (uint32_t i = 0; i < obstacles.size(); i++) {
		obstacles_rids[i] = obstacles[i]->get_self();
	}
	return obstacles_rids;
}

RID GodotNavigationServer3D::region_get_map(RID p_region) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, RID());

	if (region->get_map()) {
		return region->get_map()->get_self();
	}
	return RID();
}

RID GodotNavigationServer3D::agent_get_map(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, RID());

	if (agent->get_map()) {
		return agent->get_map()->get_self();
	}
	return RID();
}

COMMAND_2(map_set_use_async_iterations, RID, p_map, bool, p_enabled) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);
	map->set_use_async_iterations(p_enabled);
}

bool GodotNavigationServer3D::map_get_use_async_iterations(RID p_map) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, false);

	return map->get_use_async_iterations();
}

Vector3 GodotNavigationServer3D::map_get_random_point(RID p_map, uint32_t p_navigation_layers, bool p_uniformly) const {
	const NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector3());

	return map->get_random_point(p_navigation_layers, p_uniformly);
}

RID GodotNavigationServer3D::region_create() {
	MutexLock lock(operations_mutex);

	RID rid = region_owner.make_rid();
	NavRegion3D *reg = region_owner.get_or_null(rid);
	reg->set_self(rid);
	return rid;
}

COMMAND_2(region_set_enabled, RID, p_region, bool, p_enabled) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_enabled(p_enabled);
}

bool GodotNavigationServer3D::region_get_enabled(RID p_region) const {
	const NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, false);

	return region->get_enabled();
}

COMMAND_2(region_set_use_edge_connections, RID, p_region, bool, p_enabled) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_use_edge_connections(p_enabled);
}

bool GodotNavigationServer3D::region_get_use_edge_connections(RID p_region) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, false);

	return region->get_use_edge_connections();
}

COMMAND_2(region_set_map, RID, p_region, RID, p_map) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	NavMap3D *map = map_owner.get_or_null(p_map);

	region->set_map(map);
}

COMMAND_2(region_set_transform, RID, p_region, Transform3D, p_transform) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_transform(p_transform);
}

Transform3D GodotNavigationServer3D::region_get_transform(RID p_region) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Transform3D());

	return region->get_transform();
}

COMMAND_2(region_set_enter_cost, RID, p_region, real_t, p_enter_cost) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);
	ERR_FAIL_COND(p_enter_cost < 0.0);

	region->set_enter_cost(p_enter_cost);
}

real_t GodotNavigationServer3D::region_get_enter_cost(RID p_region) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, 0);

	return region->get_enter_cost();
}

COMMAND_2(region_set_travel_cost, RID, p_region, real_t, p_travel_cost) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);
	ERR_FAIL_COND(p_travel_cost < 0.0);

	region->set_travel_cost(p_travel_cost);
}

real_t GodotNavigationServer3D::region_get_travel_cost(RID p_region) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, 0);

	return region->get_travel_cost();
}

COMMAND_2(region_set_owner_id, RID, p_region, ObjectID, p_owner_id) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_owner_id(p_owner_id);
}

ObjectID GodotNavigationServer3D::region_get_owner_id(RID p_region) const {
	const NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, ObjectID());

	return region->get_owner_id();
}

bool GodotNavigationServer3D::region_owns_point(RID p_region, const Vector3 &p_point) const {
	const NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, false);

	if (region->get_map()) {
		RID closest_point_owner = map_get_closest_point_owner(region->get_map()->get_self(), p_point);
		return closest_point_owner == region->get_self();
	}
	return false;
}

COMMAND_2(region_set_navigation_layers, RID, p_region, uint32_t, p_navigation_layers) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_navigation_layers(p_navigation_layers);
}

uint32_t GodotNavigationServer3D::region_get_navigation_layers(RID p_region) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, 0);

	return region->get_navigation_layers();
}

COMMAND_2(region_set_navigation_mesh, RID, p_region, Ref<NavigationMesh>, p_navigation_mesh) {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_navigation_mesh(p_navigation_mesh);
}

#ifndef DISABLE_DEPRECATED
void GodotNavigationServer3D::region_bake_navigation_mesh(Ref<NavigationMesh> p_navigation_mesh, Node *p_root_node) {
	ERR_FAIL_COND(p_navigation_mesh.is_null());
	ERR_FAIL_NULL(p_root_node);

	WARN_PRINT_ONCE("NavigationServer3D::region_bake_navigation_mesh() is deprecated due to core threading changes. To upgrade existing code, first create a NavigationMeshSourceGeometryData3D resource. Use this resource with method parse_source_geometry_data() to parse the SceneTree for nodes that should contribute to the navigation mesh baking. The SceneTree parsing needs to happen on the main thread. After the parsing is finished use the resource with method bake_from_source_geometry_data() to bake a navigation mesh..");

#ifndef _3D_DISABLED
	p_navigation_mesh->clear();
	Ref<NavigationMeshSourceGeometryData3D> source_geometry_data;
	source_geometry_data.instantiate();
	parse_source_geometry_data(p_navigation_mesh, source_geometry_data, p_root_node);
	bake_from_source_geometry_data(p_navigation_mesh, source_geometry_data);
#endif
}
#endif // DISABLE_DEPRECATED

int GodotNavigationServer3D::region_get_connections_count(RID p_region) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, 0);
	NavMap3D *map = region->get_map();
	if (map) {
		return map->get_region_connections_count(region);
	}
	return 0;
}

Vector3 GodotNavigationServer3D::region_get_connection_pathway_start(RID p_region, int p_connection_id) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector3());
	NavMap3D *map = region->get_map();
	if (map) {
		return map->get_region_connection_pathway_start(region, p_connection_id);
	}
	return Vector3();
}

Vector3 GodotNavigationServer3D::region_get_connection_pathway_end(RID p_region, int p_connection_id) const {
	NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector3());
	NavMap3D *map = region->get_map();
	if (map) {
		return map->get_region_connection_pathway_end(region, p_connection_id);
	}
	return Vector3();
}

Vector3 GodotNavigationServer3D::region_get_closest_point_to_segment(RID p_region, const Vector3 &p_from, const Vector3 &p_to, bool p_use_collision) const {
	const NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector3());

	return region->get_closest_point_to_segment(p_from, p_to, p_use_collision);
}

Vector3 GodotNavigationServer3D::region_get_closest_point(RID p_region, const Vector3 &p_point) const {
	const NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector3());

	return region->get_closest_point_info(p_point).point;
}

Vector3 GodotNavigationServer3D::region_get_closest_point_normal(RID p_region, const Vector3 &p_point) const {
	const NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector3());

	return region->get_closest_point_info(p_point).normal;
}

Vector3 GodotNavigationServer3D::region_get_random_point(RID p_region, uint32_t p_navigation_layers, bool p_uniformly) const {
	const NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector3());

	return region->get_random_point(p_navigation_layers, p_uniformly);
}

AABB GodotNavigationServer3D::region_get_bounds(RID p_region) const {
	const NavRegion3D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, AABB());

	return region->get_bounds();
}

RID GodotNavigationServer3D::link_create() {
	MutexLock lock(operations_mutex);

	RID rid = link_owner.make_rid();
	NavLink3D *link = link_owner.get_or_null(rid);
	link->set_self(rid);
	return rid;
}

COMMAND_2(link_set_map, RID, p_link, RID, p_map) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	NavMap3D *map = map_owner.get_or_null(p_map);

	link->set_map(map);
}

RID GodotNavigationServer3D::link_get_map(const RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, RID());

	if (link->get_map()) {
		return link->get_map()->get_self();
	}
	return RID();
}

COMMAND_2(link_set_enabled, RID, p_link, bool, p_enabled) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_enabled(p_enabled);
}

bool GodotNavigationServer3D::link_get_enabled(RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, false);

	return link->get_enabled();
}

COMMAND_2(link_set_bidirectional, RID, p_link, bool, p_bidirectional) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_bidirectional(p_bidirectional);
}

bool GodotNavigationServer3D::link_is_bidirectional(RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, false);

	return link->is_bidirectional();
}

COMMAND_2(link_set_navigation_layers, RID, p_link, uint32_t, p_navigation_layers) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_navigation_layers(p_navigation_layers);
}

uint32_t GodotNavigationServer3D::link_get_navigation_layers(const RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, 0);

	return link->get_navigation_layers();
}

COMMAND_2(link_set_start_position, RID, p_link, Vector3, p_position) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_start_position(p_position);
}

Vector3 GodotNavigationServer3D::link_get_start_position(RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, Vector3());

	return link->get_start_position();
}

COMMAND_2(link_set_end_position, RID, p_link, Vector3, p_position) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_end_position(p_position);
}

Vector3 GodotNavigationServer3D::link_get_end_position(RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, Vector3());

	return link->get_end_position();
}

COMMAND_2(link_set_enter_cost, RID, p_link, real_t, p_enter_cost) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_enter_cost(p_enter_cost);
}

real_t GodotNavigationServer3D::link_get_enter_cost(const RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, 0);

	return link->get_enter_cost();
}

COMMAND_2(link_set_travel_cost, RID, p_link, real_t, p_travel_cost) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_travel_cost(p_travel_cost);
}

real_t GodotNavigationServer3D::link_get_travel_cost(const RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, 0);

	return link->get_travel_cost();
}

COMMAND_2(link_set_owner_id, RID, p_link, ObjectID, p_owner_id) {
	NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_owner_id(p_owner_id);
}

ObjectID GodotNavigationServer3D::link_get_owner_id(RID p_link) const {
	const NavLink3D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, ObjectID());

	return link->get_owner_id();
}

RID GodotNavigationServer3D::agent_create() {
	MutexLock lock(operations_mutex);

	RID rid = agent_owner.make_rid();
	NavAgent3D *agent = agent_owner.get_or_null(rid);
	agent->set_self(rid);
	return rid;
}

COMMAND_2(agent_set_avoidance_enabled, RID, p_agent, bool, p_enabled) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_avoidance_enabled(p_enabled);
}

bool GodotNavigationServer3D::agent_get_avoidance_enabled(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->is_avoidance_enabled();
}

COMMAND_2(agent_set_use_3d_avoidance, RID, p_agent, bool, p_enabled) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_use_3d_avoidance(p_enabled);
}

bool GodotNavigationServer3D::agent_get_use_3d_avoidance(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->get_use_3d_avoidance();
}

COMMAND_2(agent_set_map, RID, p_agent, RID, p_map) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	NavMap3D *map = map_owner.get_or_null(p_map);

	agent->set_map(map);
}

COMMAND_2(agent_set_paused, RID, p_agent, bool, p_paused) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_paused(p_paused);
}

bool GodotNavigationServer3D::agent_get_paused(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->get_paused();
}

COMMAND_2(agent_set_neighbor_distance, RID, p_agent, real_t, p_distance) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_neighbor_distance(p_distance);
}

real_t GodotNavigationServer3D::agent_get_neighbor_distance(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_neighbor_distance();
}

COMMAND_2(agent_set_max_neighbors, RID, p_agent, int, p_count) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_max_neighbors(p_count);
}

int GodotNavigationServer3D::agent_get_max_neighbors(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_max_neighbors();
}

COMMAND_2(agent_set_time_horizon_agents, RID, p_agent, real_t, p_time_horizon) {
	ERR_FAIL_COND_MSG(p_time_horizon < 0.0, "Time horizon must be positive.");
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_time_horizon_agents(p_time_horizon);
}

real_t GodotNavigationServer3D::agent_get_time_horizon_agents(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_time_horizon_agents();
}

COMMAND_2(agent_set_time_horizon_obstacles, RID, p_agent, real_t, p_time_horizon) {
	ERR_FAIL_COND_MSG(p_time_horizon < 0.0, "Time horizon must be positive.");
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_time_horizon_obstacles(p_time_horizon);
}

real_t GodotNavigationServer3D::agent_get_time_horizon_obstacles(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_time_horizon_obstacles();
}

COMMAND_2(agent_set_radius, RID, p_agent, real_t, p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0.0, "Radius must be positive.");
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_radius(p_radius);
}

real_t GodotNavigationServer3D::agent_get_radius(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_radius();
}

COMMAND_2(agent_set_height, RID, p_agent, real_t, p_height) {
	ERR_FAIL_COND_MSG(p_height < 0.0, "Height must be positive.");
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_height(p_height);
}

real_t GodotNavigationServer3D::agent_get_height(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_height();
}

COMMAND_2(agent_set_max_speed, RID, p_agent, real_t, p_max_speed) {
	ERR_FAIL_COND_MSG(p_max_speed < 0.0, "Max speed must be positive.");
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_max_speed(p_max_speed);
}

real_t GodotNavigationServer3D::agent_get_max_speed(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_max_speed();
}

COMMAND_2(agent_set_velocity, RID, p_agent, Vector3, p_velocity) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_velocity(p_velocity);
}

Vector3 GodotNavigationServer3D::agent_get_velocity(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, Vector3());

	return agent->get_velocity();
}

COMMAND_2(agent_set_velocity_forced, RID, p_agent, Vector3, p_velocity) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_velocity_forced(p_velocity);
}

COMMAND_2(agent_set_position, RID, p_agent, Vector3, p_position) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_position(p_position);
}

Vector3 GodotNavigationServer3D::agent_get_position(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, Vector3());

	return agent->get_position();
}

bool GodotNavigationServer3D::agent_is_map_changed(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->is_map_changed();
}

COMMAND_2(agent_set_avoidance_callback, RID, p_agent, Callable, p_callback) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_avoidance_callback(p_callback);

	if (agent->get_map()) {
		if (p_callback.is_valid()) {
			agent->get_map()->set_agent_as_controlled(agent);
		} else {
			agent->get_map()->remove_agent_as_controlled(agent);
		}
	}
}

bool GodotNavigationServer3D::agent_has_avoidance_callback(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->has_avoidance_callback();
}

COMMAND_2(agent_set_avoidance_layers, RID, p_agent, uint32_t, p_layers) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);
	agent->set_avoidance_layers(p_layers);
}

uint32_t GodotNavigationServer3D::agent_get_avoidance_layers(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_avoidance_layers();
}

COMMAND_2(agent_set_avoidance_mask, RID, p_agent, uint32_t, p_mask) {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);
	agent->set_avoidance_mask(p_mask);
}

uint32_t GodotNavigationServer3D::agent_get_avoidance_mask(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_avoidance_mask();
}

COMMAND_2(agent_set_avoidance_priority, RID, p_agent, real_t, p_priority) {
	ERR_FAIL_COND_MSG(p_priority < 0.0, "Avoidance priority must be between 0.0 and 1.0 inclusive.");
	ERR_FAIL_COND_MSG(p_priority > 1.0, "Avoidance priority must be between 0.0 and 1.0 inclusive.");
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);
	agent->set_avoidance_priority(p_priority);
}

real_t GodotNavigationServer3D::agent_get_avoidance_priority(RID p_agent) const {
	NavAgent3D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_avoidance_priority();
}

RID GodotNavigationServer3D::obstacle_create() {
	MutexLock lock(operations_mutex);

	RID rid = obstacle_owner.make_rid();
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(rid);
	obstacle->set_self(rid);

	RID agent_rid = agent_owner.make_rid();
	NavAgent3D *agent = agent_owner.get_or_null(agent_rid);
	agent->set_self(agent_rid);

	obstacle->set_agent(agent);

	return rid;
}

COMMAND_2(obstacle_set_avoidance_enabled, RID, p_obstacle, bool, p_enabled) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_avoidance_enabled(p_enabled);
}

bool GodotNavigationServer3D::obstacle_get_avoidance_enabled(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, false);

	return obstacle->is_avoidance_enabled();
}

COMMAND_2(obstacle_set_use_3d_avoidance, RID, p_obstacle, bool, p_enabled) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_use_3d_avoidance(p_enabled);
}

bool GodotNavigationServer3D::obstacle_get_use_3d_avoidance(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, false);

	return obstacle->get_use_3d_avoidance();
}

COMMAND_2(obstacle_set_map, RID, p_obstacle, RID, p_map) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	NavMap3D *map = map_owner.get_or_null(p_map);

	obstacle->set_map(map);
}

RID GodotNavigationServer3D::obstacle_get_map(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, RID());
	if (obstacle->get_map()) {
		return obstacle->get_map()->get_self();
	}
	return RID();
}

COMMAND_2(obstacle_set_paused, RID, p_obstacle, bool, p_paused) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_paused(p_paused);
}

bool GodotNavigationServer3D::obstacle_get_paused(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, false);

	return obstacle->get_paused();
}

COMMAND_2(obstacle_set_radius, RID, p_obstacle, real_t, p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0.0, "Radius must be positive.");
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_radius(p_radius);
}

real_t GodotNavigationServer3D::obstacle_get_radius(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, 0);

	return obstacle->get_radius();
}

COMMAND_2(obstacle_set_height, RID, p_obstacle, real_t, p_height) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);
	obstacle->set_height(p_height);
}

real_t GodotNavigationServer3D::obstacle_get_height(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, 0);

	return obstacle->get_height();
}

COMMAND_2(obstacle_set_velocity, RID, p_obstacle, Vector3, p_velocity) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_velocity(p_velocity);
}

Vector3 GodotNavigationServer3D::obstacle_get_velocity(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, Vector3());

	return obstacle->get_velocity();
}

COMMAND_2(obstacle_set_position, RID, p_obstacle, Vector3, p_position) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);
	obstacle->set_position(p_position);
}

Vector3 GodotNavigationServer3D::obstacle_get_position(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, Vector3());

	return obstacle->get_position();
}

void GodotNavigationServer3D::obstacle_set_vertices(RID p_obstacle, const Vector<Vector3> &p_vertices) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);
	obstacle->set_vertices(p_vertices);
}

Vector<Vector3> GodotNavigationServer3D::obstacle_get_vertices(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, Vector<Vector3>());

	return obstacle->get_vertices();
}

COMMAND_2(obstacle_set_avoidance_layers, RID, p_obstacle, uint32_t, p_layers) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);
	obstacle->set_avoidance_layers(p_layers);
}

uint32_t GodotNavigationServer3D::obstacle_get_avoidance_layers(RID p_obstacle) const {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, 0);

	return obstacle->get_avoidance_layers();
}

void GodotNavigationServer3D::parse_source_geometry_data(const Ref<NavigationMesh> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData3D> &p_source_geometry_data, Node *p_root_node, const Callable &p_callback) {
#ifndef _3D_DISABLED
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "The SceneTree can only be parsed on the main thread. Call this function from the main thread or use call_deferred().");
	ERR_FAIL_COND_MSG(p_navigation_mesh.is_null(), "Invalid navigation mesh.");
	ERR_FAIL_NULL_MSG(p_root_node, "No parsing root node specified.");
	ERR_FAIL_COND_MSG(!p_root_node->is_inside_tree(), "The root node needs to be inside the SceneTree.");

	ERR_FAIL_NULL(NavMeshGenerator3D::get_singleton());
	NavMeshGenerator3D::get_singleton()->parse_source_geometry_data(p_navigation_mesh, p_source_geometry_data, p_root_node, p_callback);
#endif // _3D_DISABLED
}

void GodotNavigationServer3D::bake_from_source_geometry_data(const Ref<NavigationMesh> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData3D> &p_source_geometry_data, const Callable &p_callback) {
#ifndef _3D_DISABLED
	ERR_FAIL_COND_MSG(p_navigation_mesh.is_null(), "Invalid navigation mesh.");
	ERR_FAIL_COND_MSG(p_source_geometry_data.is_null(), "Invalid NavigationMeshSourceGeometryData3D.");

	ERR_FAIL_NULL(NavMeshGenerator3D::get_singleton());
	NavMeshGenerator3D::get_singleton()->bake_from_source_geometry_data(p_navigation_mesh, p_source_geometry_data, p_callback);
#endif // _3D_DISABLED
}

void GodotNavigationServer3D::bake_from_source_geometry_data_async(const Ref<NavigationMesh> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData3D> &p_source_geometry_data, const Callable &p_callback) {
#ifndef _3D_DISABLED
	ERR_FAIL_COND_MSG(p_navigation_mesh.is_null(), "Invalid navigation mesh.");
	ERR_FAIL_COND_MSG(p_source_geometry_data.is_null(), "Invalid NavigationMeshSourceGeometryData3D.");

	ERR_FAIL_NULL(NavMeshGenerator3D::get_singleton());
	NavMeshGenerator3D::get_singleton()->bake_from_source_geometry_data_async(p_navigation_mesh, p_source_geometry_data, p_callback);
#endif // _3D_DISABLED
}

bool GodotNavigationServer3D::is_baking_navigation_mesh(Ref<NavigationMesh> p_navigation_mesh) const {
#ifdef _3D_DISABLED
	return false;
#else
	return NavMeshGenerator3D::get_singleton()->is_baking(p_navigation_mesh);
#endif // _3D_DISABLED
}

COMMAND_1(free, RID, p_object) {
	if (map_owner.owns(p_object)) {
		NavMap3D *map = map_owner.get_or_null(p_object);

		// Removes any assigned region
		for (NavRegion3D *region : map->get_regions()) {
			map->remove_region(region);
			region->set_map(nullptr);
		}

		// Removes any assigned links
		for (NavLink3D *link : map->get_links()) {
			map->remove_link(link);
			link->set_map(nullptr);
		}

		// Remove any assigned agent
		for (NavAgent3D *agent : map->get_agents()) {
			map->remove_agent(agent);
			agent->set_map(nullptr);
		}

		// Remove any assigned obstacles
		for (NavObstacle3D *obstacle : map->get_obstacles()) {
			map->remove_obstacle(obstacle);
			obstacle->set_map(nullptr);
		}

		int map_index = active_maps.find(map);
		if (map_index >= 0) {
			active_maps.remove_at(map_index);
			active_maps_iteration_id.remove_at(map_index);
		}
		map_owner.free(p_object);

	} else if (region_owner.owns(p_object)) {
		NavRegion3D *region = region_owner.get_or_null(p_object);

		// Removes this region from the map if assigned
		if (region->get_map() != nullptr) {
			region->get_map()->remove_region(region);
			region->set_map(nullptr);
		}

		region_owner.free(p_object);

	} else if (link_owner.owns(p_object)) {
		NavLink3D *link = link_owner.get_or_null(p_object);

		// Removes this link from the map if assigned
		if (link->get_map() != nullptr) {
			link->get_map()->remove_link(link);
			link->set_map(nullptr);
		}

		link_owner.free(p_object);

	} else if (agent_owner.owns(p_object)) {
		internal_free_agent(p_object);

	} else if (obstacle_owner.owns(p_object)) {
		internal_free_obstacle(p_object);

	} else if (geometry_parser_owner.owns(p_object)) {
		RWLockWrite write_lock(geometry_parser_rwlock);

		NavMeshGeometryParser3D *parser = geometry_parser_owner.get_or_null(p_object);
		ERR_FAIL_NULL(parser);

		generator_parsers.erase(parser);
#ifndef _3D_DISABLED
		NavMeshGenerator3D::get_singleton()->set_generator_parsers(generator_parsers);
#endif
		geometry_parser_owner.free(parser->self);

	} else {
		ERR_PRINT("Attempted to free a NavigationServer RID that did not exist (or was already freed).");
	}
}

void GodotNavigationServer3D::internal_free_agent(RID p_object) {
	NavAgent3D *agent = agent_owner.get_or_null(p_object);
	if (agent) {
		if (agent->get_map() != nullptr) {
			agent->get_map()->remove_agent(agent);
			agent->set_map(nullptr);
		}
		agent_owner.free(p_object);
	}
}

void GodotNavigationServer3D::internal_free_obstacle(RID p_object) {
	NavObstacle3D *obstacle = obstacle_owner.get_or_null(p_object);
	if (obstacle) {
		NavAgent3D *obstacle_agent = obstacle->get_agent();
		if (obstacle_agent) {
			RID _agent_rid = obstacle_agent->get_self();
			internal_free_agent(_agent_rid);
			obstacle->set_agent(nullptr);
		}
		if (obstacle->get_map() != nullptr) {
			obstacle->get_map()->remove_obstacle(obstacle);
			obstacle->set_map(nullptr);
		}
		obstacle_owner.free(p_object);
	}
}

void GodotNavigationServer3D::set_active(bool p_active) {
	MutexLock lock(operations_mutex);

	active = p_active;
}

void GodotNavigationServer3D::flush_queries() {
	// In c++ we can't be sure that this is performed in the main thread
	// even with mutable functions.
	MutexLock lock(commands_mutex);
	MutexLock lock2(operations_mutex);

	for (SetCommand3D *command : commands) {
		command->exec(this);
		memdelete(command);
	}
	commands.clear();
}

void GodotNavigationServer3D::map_force_update(RID p_map) {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	flush_queries();

	map->sync();
}

uint32_t GodotNavigationServer3D::map_get_iteration_id(RID p_map) const {
	NavMap3D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_iteration_id();
}

void GodotNavigationServer3D::sync() {
#ifndef _3D_DISABLED
	if (navmesh_generator_3d) {
		navmesh_generator_3d->sync();
	}
#endif // _3D_DISABLED
}

void GodotNavigationServer3D::process(real_t p_delta_time) {
	flush_queries();

	if (!active) {
		return;
	}

	int _new_pm_region_count = 0;
	int _new_pm_agent_count = 0;
	int _new_pm_link_count = 0;
	int _new_pm_polygon_count = 0;
	int _new_pm_edge_count = 0;
	int _new_pm_edge_merge_count = 0;
	int _new_pm_edge_connection_count = 0;
	int _new_pm_edge_free_count = 0;
	int _new_pm_obstacle_count = 0;

	// In c++ we can't be sure that this is performed in the main thread
	// even with mutable functions.
	MutexLock lock(operations_mutex);
	for (uint32_t i(0); i < active_maps.size(); i++) {
		active_maps[i]->sync();
		active_maps[i]->step(p_delta_time);
		active_maps[i]->dispatch_callbacks();

		_new_pm_region_count += active_maps[i]->get_pm_region_count();
		_new_pm_agent_count += active_maps[i]->get_pm_agent_count();
		_new_pm_link_count += active_maps[i]->get_pm_link_count();
		_new_pm_polygon_count += active_maps[i]->get_pm_polygon_count();
		_new_pm_edge_count += active_maps[i]->get_pm_edge_count();
		_new_pm_edge_merge_count += active_maps[i]->get_pm_edge_merge_count();
		_new_pm_edge_connection_count += active_maps[i]->get_pm_edge_connection_count();
		_new_pm_edge_free_count += active_maps[i]->get_pm_edge_free_count();
		_new_pm_obstacle_count += active_maps[i]->get_pm_obstacle_count();

		// Emit a signal if a map changed.
		const uint32_t new_map_iteration_id = active_maps[i]->get_iteration_id();
		if (new_map_iteration_id != active_maps_iteration_id[i]) {
			emit_signal(SNAME("map_changed"), active_maps[i]->get_self());
			active_maps_iteration_id[i] = new_map_iteration_id;
		}
	}

	pm_region_count = _new_pm_region_count;
	pm_agent_count = _new_pm_agent_count;
	pm_link_count = _new_pm_link_count;
	pm_polygon_count = _new_pm_polygon_count;
	pm_edge_count = _new_pm_edge_count;
	pm_edge_merge_count = _new_pm_edge_merge_count;
	pm_edge_connection_count = _new_pm_edge_connection_count;
	pm_edge_free_count = _new_pm_edge_free_count;
	pm_obstacle_count = _new_pm_obstacle_count;
}

void GodotNavigationServer3D::init() {
#ifndef _3D_DISABLED
	navmesh_generator_3d = memnew(NavMeshGenerator3D);
	RWLockRead read_lock(geometry_parser_rwlock);
	navmesh_generator_3d->set_generator_parsers(generator_parsers);
#endif // _3D_DISABLED
}

void GodotNavigationServer3D::finish() {
	flush_queries();
#ifndef _3D_DISABLED
	if (navmesh_generator_3d) {
		navmesh_generator_3d->finish();
		memdelete(navmesh_generator_3d);
		navmesh_generator_3d = nullptr;
	}
#endif // _3D_DISABLED
}

void GodotNavigationServer3D::query_path(const Ref<NavigationPathQueryParameters3D> &p_query_parameters, Ref<NavigationPathQueryResult3D> p_query_result, const Callable &p_callback) {
	ERR_FAIL_COND(p_query_parameters.is_null());
	ERR_FAIL_COND(p_query_result.is_null());

	NavMap3D *map = map_owner.get_or_null(p_query_parameters->get_map());
	ERR_FAIL_NULL(map);

	NavMeshQueries3D::map_query_path(map, p_query_parameters, p_query_result, p_callback);
}

RID GodotNavigationServer3D::source_geometry_parser_create() {
	RWLockWrite write_lock(geometry_parser_rwlock);

	RID rid = geometry_parser_owner.make_rid();

	NavMeshGeometryParser3D *parser = geometry_parser_owner.get_or_null(rid);
	parser->self = rid;

	generator_parsers.push_back(parser);
#ifndef _3D_DISABLED
	NavMeshGenerator3D::get_singleton()->set_generator_parsers(generator_parsers);
#endif
	return rid;
}

void GodotNavigationServer3D::source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) {
	RWLockWrite write_lock(geometry_parser_rwlock);

	NavMeshGeometryParser3D *parser = geometry_parser_owner.get_or_null(p_parser);
	ERR_FAIL_NULL(parser);

	parser->callback = p_callback;
}

Vector<Vector3> GodotNavigationServer3D::simplify_path(const Vector<Vector3> &p_path, real_t p_epsilon) {
	if (p_path.size() <= 2) {
		return p_path;
	}

	p_epsilon = MAX(0.0, p_epsilon);

	LocalVector<Vector3> source_path;
	{
		source_path.resize(p_path.size());
		const Vector3 *r = p_path.ptr();
		for (uint32_t i = 0; i < p_path.size(); i++) {
			source_path[i] = r[i];
		}
	}

	LocalVector<uint32_t> simplified_path_indices = NavMeshQueries3D::get_simplified_path_indices(source_path, p_epsilon);

	uint32_t index_count = simplified_path_indices.size();

	Vector<Vector3> simplified_path;
	{
		simplified_path.resize(index_count);
		Vector3 *w = simplified_path.ptrw();
		const Vector3 *r = source_path.ptr();
		for (uint32_t i = 0; i < index_count; i++) {
			w[i] = r[simplified_path_indices[i]];
		}
	}

	return simplified_path;
}

int GodotNavigationServer3D::get_process_info(ProcessInfo p_info) const {
	switch (p_info) {
		case INFO_ACTIVE_MAPS: {
			return active_maps.size();
		} break;
		case INFO_REGION_COUNT: {
			return pm_region_count;
		} break;
		case INFO_AGENT_COUNT: {
			return pm_agent_count;
		} break;
		case INFO_LINK_COUNT: {
			return pm_link_count;
		} break;
		case INFO_POLYGON_COUNT: {
			return pm_polygon_count;
		} break;
		case INFO_EDGE_COUNT: {
			return pm_edge_count;
		} break;
		case INFO_EDGE_MERGE_COUNT: {
			return pm_edge_merge_count;
		} break;
		case INFO_EDGE_CONNECTION_COUNT: {
			return pm_edge_connection_count;
		} break;
		case INFO_EDGE_FREE_COUNT: {
			return pm_edge_free_count;
		} break;
		case INFO_OBSTACLE_COUNT: {
			return pm_obstacle_count;
		} break;
	}

	return 0;
}

#undef COMMAND_1
#undef COMMAND_2
