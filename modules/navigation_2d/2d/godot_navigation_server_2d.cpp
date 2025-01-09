/**************************************************************************/
/*  godot_navigation_server_2d.cpp                                        */
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

#include "godot_navigation_server_2d.h"

#include "core/os/mutex.h"
#include "scene/main/node.h"

#ifdef CLIPPER2_ENABLED
#include "nav_mesh_generator_2d.h"
#endif // CLIPPER2_ENABLED

#define COMMAND_1(F_NAME, T_0, D_0)                                     \
	struct MERGE(F_NAME, _command_2d) : public SetCommand2D {           \
		T_0 d_0;                                                        \
		MERGE(F_NAME, _command_2d)                                      \
		(T_0 p_d_0) :                                                   \
				d_0(p_d_0) {}                                           \
		virtual void exec(GodotNavigationServer2D *p_server) override { \
			p_server->MERGE(_cmd_, F_NAME)(d_0);                        \
		}                                                               \
	};                                                                  \
	void GodotNavigationServer2D::F_NAME(T_0 D_0) {                     \
		auto cmd = memnew(MERGE(F_NAME, _command_2d)(                   \
				D_0));                                                  \
		add_command(cmd);                                               \
	}                                                                   \
	void GodotNavigationServer2D::MERGE(_cmd_, F_NAME)(T_0 D_0)

#define COMMAND_2(F_NAME, T_0, D_0, T_1, D_1)                           \
	struct MERGE(F_NAME, _command_2d) : public SetCommand2D {           \
		T_0 d_0;                                                        \
		T_1 d_1;                                                        \
		MERGE(F_NAME, _command_2d)                                      \
		(                                                               \
				T_0 p_d_0,                                              \
				T_1 p_d_1) :                                            \
				d_0(p_d_0),                                             \
				d_1(p_d_1) {}                                           \
		virtual void exec(GodotNavigationServer2D *p_server) override { \
			p_server->MERGE(_cmd_, F_NAME)(d_0, d_1);                   \
		}                                                               \
	};                                                                  \
	void GodotNavigationServer2D::F_NAME(T_0 D_0, T_1 D_1) {            \
		auto cmd = memnew(MERGE(F_NAME, _command_2d)(                   \
				D_0,                                                    \
				D_1));                                                  \
		add_command(cmd);                                               \
	}                                                                   \
	void GodotNavigationServer2D::MERGE(_cmd_, F_NAME)(T_0 D_0, T_1 D_1)

void GodotNavigationServer2D::init() {
#ifdef CLIPPER2_ENABLED
	navmesh_generator_2d = memnew(NavMeshGenerator2D);
	ERR_FAIL_NULL_MSG(navmesh_generator_2d, "Failed to init NavMeshGenerator2D.");
	RWLockRead read_lock(geometry_parser_rwlock);
	navmesh_generator_2d->set_generator_parsers(generator_parsers);
#endif // CLIPPER2_ENABLED
	// TODO
}

void GodotNavigationServer2D::sync() {
#ifdef CLIPPER2_ENABLED
	if (navmesh_generator_2d) {
		navmesh_generator_2d->sync();
	}
#endif // CLIPPER2_ENABLED
	// TODO
}

void GodotNavigationServer2D::finish() {
#ifdef CLIPPER2_ENABLED
	if (navmesh_generator_2d) {
		navmesh_generator_2d->finish();
		memdelete(navmesh_generator_2d);
		navmesh_generator_2d = nullptr;
	}
#endif // CLIPPER2_ENABLED
	// TODO
}

void GodotNavigationServer2D::parse_source_geometry_data(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, Node *p_root_node, const Callable &p_callback) {
	ERR_FAIL_COND_MSG(!Thread::is_main_thread(), "The SceneTree can only be parsed on the main thread. Call this function from the main thread or use call_deferred().");
	ERR_FAIL_COND_MSG(p_navigation_mesh.is_null(), "Invalid navigation polygon.");
	ERR_FAIL_NULL_MSG(p_root_node, "No parsing root node specified.");
	ERR_FAIL_COND_MSG(!p_root_node->is_inside_tree(), "The root node needs to be inside the SceneTree.");

#ifdef CLIPPER2_ENABLED
	ERR_FAIL_NULL(NavMeshGenerator2D::get_singleton());
	NavMeshGenerator2D::get_singleton()->parse_source_geometry_data(p_navigation_mesh, p_source_geometry_data, p_root_node, p_callback);
#endif // CLIPPER2_ENABLED
}

void GodotNavigationServer2D::bake_from_source_geometry_data(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, const Callable &p_callback) {
	ERR_FAIL_COND_MSG(p_navigation_mesh.is_null(), "Invalid navigation polygon.");
	ERR_FAIL_COND_MSG(p_source_geometry_data.is_null(), "Invalid NavigationMeshSourceGeometryData2D.");

#ifdef CLIPPER2_ENABLED
	ERR_FAIL_NULL(NavMeshGenerator2D::get_singleton());
	NavMeshGenerator2D::get_singleton()->bake_from_source_geometry_data(p_navigation_mesh, p_source_geometry_data, p_callback);
#endif // CLIPPER2_ENABLED
}

void GodotNavigationServer2D::bake_from_source_geometry_data_async(const Ref<NavigationPolygon> &p_navigation_mesh, const Ref<NavigationMeshSourceGeometryData2D> &p_source_geometry_data, const Callable &p_callback) {
	ERR_FAIL_COND_MSG(p_navigation_mesh.is_null(), "Invalid navigation mesh.");
	ERR_FAIL_COND_MSG(p_source_geometry_data.is_null(), "Invalid NavigationMeshSourceGeometryData2D.");

#ifdef CLIPPER2_ENABLED
	ERR_FAIL_NULL(NavMeshGenerator2D::get_singleton());
	NavMeshGenerator2D::get_singleton()->bake_from_source_geometry_data_async(p_navigation_mesh, p_source_geometry_data, p_callback);
#endif // CLIPPER2_ENABLED
}

bool GodotNavigationServer2D::is_baking_navigation_polygon(Ref<NavigationPolygon> p_navigation_polygon) const {
#ifdef CLIPPER2_ENABLED
	return NavMeshGenerator2D::get_singleton()->is_baking(p_navigation_polygon);
#else
	return false;
#endif
}

Vector<Vector2> GodotNavigationServer2D::simplify_path(const Vector<Vector2> &p_path, real_t p_epsilon) {
	if (p_path.size() <= 2) {
		return p_path;
	}

	p_epsilon = MAX(0.0, p_epsilon);

	LocalVector<Vector2> source_path;
	{
		source_path.resize(p_path.size());
		const Vector2 *r = p_path.ptr();
		for (uint32_t i = 0; i < p_path.size(); i++) {
			source_path[i] = r[i];
		}
	}

	LocalVector<uint32_t> simplified_path_indices = NavMeshQueries2D::get_simplified_path_indices(source_path, p_epsilon);

	uint32_t index_count = simplified_path_indices.size();

	Vector<Vector2> simplified_path;
	{
		simplified_path.resize(index_count);
		Vector2 *w = simplified_path.ptrw();
		const Vector2 *r = source_path.ptr();
		for (uint32_t i = 0; i < index_count; i++) {
			w[i] = r[simplified_path_indices[i]];
		}
	}

	return simplified_path;
}

GodotNavigationServer2D::GodotNavigationServer2D() {}

GodotNavigationServer2D::~GodotNavigationServer2D() {
	flush_queries();
}

void GodotNavigationServer2D::add_command(SetCommand2D *p_command) {
	MutexLock lock(commands_mutex);

	commands.push_back(p_command);
}

TypedArray<RID> GodotNavigationServer2D::get_maps() const {
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

TypedArray<RID> GodotNavigationServer2D::map_get_links(RID p_map) const {
	TypedArray<RID> link_rids;
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, link_rids);

	const LocalVector<NavLink2D *> &links = map->get_links();
	link_rids.resize(links.size());

	for (uint32_t i = 0; i < links.size(); i++) {
		link_rids[i] = links[i]->get_self();
	}
	return link_rids;
}

TypedArray<RID> GodotNavigationServer2D::map_get_regions(RID p_map) const {
	TypedArray<RID> regions_rids;
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, regions_rids);

	const LocalVector<NavRegion2D *> &regions = map->get_regions();
	regions_rids.resize(regions.size());

	for (uint32_t i = 0; i < regions.size(); i++) {
		regions_rids[i] = regions[i]->get_self();
	}
	return regions_rids;
}

TypedArray<RID> GodotNavigationServer2D::map_get_agents(RID p_map) const {
	TypedArray<RID> agents_rids;
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, agents_rids);

	const LocalVector<NavAgent2D *> &agents = map->get_agents();
	agents_rids.resize(agents.size());

	for (uint32_t i = 0; i < agents.size(); i++) {
		agents_rids[i] = agents[i]->get_self();
	}
	return agents_rids;
}

TypedArray<RID> GodotNavigationServer2D::map_get_obstacles(RID p_map) const {
	TypedArray<RID> obstacles_rids;
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, obstacles_rids);
	const LocalVector<NavObstacle2D *> obstacles = map->get_obstacles();
	obstacles_rids.resize(obstacles.size());
	for (uint32_t i = 0; i < obstacles.size(); i++) {
		obstacles_rids[i] = obstacles[i]->get_self();
	}
	return obstacles_rids;
}

RID GodotNavigationServer2D::region_get_map(RID p_region) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, RID());

	if (region->get_map()) {
		return region->get_map()->get_self();
	}
	return RID();
}

RID GodotNavigationServer2D::agent_get_map(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, RID());

	if (agent->get_map()) {
		return agent->get_map()->get_self();
	}
	return RID();
}

RID GodotNavigationServer2D::map_create() {
	MutexLock lock(operations_mutex);

	RID rid = map_owner.make_rid();
	NavMap2D *map = map_owner.get_or_null(rid);
	map->set_self(rid);
	return rid;
}

COMMAND_2(map_set_active, RID, p_map, bool, p_active) {
	NavMap2D *map = map_owner.get_or_null(p_map);
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

bool GodotNavigationServer2D::map_is_active(RID p_map) const {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, false);

	return active_maps.has(map);
}

void GodotNavigationServer2D::map_force_update(RID p_map) {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	flush_queries();

	map->sync();
}

uint32_t GodotNavigationServer2D::map_get_iteration_id(RID p_map) const {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_iteration_id();
}

COMMAND_2(map_set_use_async_iterations, RID, p_map, bool, p_enabled) {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);
	map->set_use_async_iterations(p_enabled);
}

bool GodotNavigationServer2D::map_get_use_async_iterations(RID p_map) const {
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, false);

	return map->get_use_async_iterations();
}

COMMAND_2(map_set_cell_size, RID, p_map, real_t, p_cell_size) {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_cell_size(p_cell_size);
}

real_t GodotNavigationServer2D::map_get_cell_size(RID p_map) const {
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_cell_size();
}

COMMAND_2(map_set_use_edge_connections, RID, p_map, bool, p_enabled) {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_use_edge_connections(p_enabled);
}

bool GodotNavigationServer2D::map_get_use_edge_connections(RID p_map) const {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, false);

	return map->get_use_edge_connections();
}

COMMAND_2(map_set_edge_connection_margin, RID, p_map, real_t, p_connection_margin) {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_edge_connection_margin(p_connection_margin);
}

real_t GodotNavigationServer2D::map_get_edge_connection_margin(RID p_map) const {
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_edge_connection_margin();
}

COMMAND_2(map_set_link_connection_radius, RID, p_map, real_t, p_connection_radius) {
	NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL(map);

	map->set_link_connection_radius(p_connection_radius);
}

real_t GodotNavigationServer2D::map_get_link_connection_radius(RID p_map) const {
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, 0);

	return map->get_link_connection_radius();
}

Vector<Vector2> GodotNavigationServer2D::map_get_path(RID p_map, Vector2 p_origin, Vector2 p_destination, bool p_optimize, uint32_t p_navigation_layers) {
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector<Vector2>());

	Ref<NavigationPathQueryParameters2D> query_parameters;
	query_parameters.instantiate();

	query_parameters->set_map(p_map);
	query_parameters->set_start_position(p_origin);
	query_parameters->set_target_position(p_destination);
	query_parameters->set_navigation_layers(p_navigation_layers);
	query_parameters->set_pathfinding_algorithm(NavigationPathQueryParameters2D::PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR);
	query_parameters->set_path_postprocessing(NavigationPathQueryParameters2D::PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL);
	if (!p_optimize) {
		query_parameters->set_path_postprocessing(NavigationPathQueryParameters2D::PATH_POSTPROCESSING_EDGECENTERED);
	}

	Ref<NavigationPathQueryResult2D> query_result;
	query_result.instantiate();

	query_path(query_parameters, query_result);

	return query_result->get_path();
}

Vector2 GodotNavigationServer2D::map_get_closest_point(RID p_map, const Vector2 &p_point) const {
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector2());

	return map->get_closest_point(p_point);
}

RID GodotNavigationServer2D::map_get_closest_point_owner(RID p_map, const Vector2 &p_point) const {
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, RID());

	return map->get_closest_point_owner(p_point);
}

Vector2 GodotNavigationServer2D::map_get_random_point(RID p_map, uint32_t p_navigation_layers, bool p_uniformly) const {
	const NavMap2D *map = map_owner.get_or_null(p_map);
	ERR_FAIL_NULL_V(map, Vector2());

	return map->get_random_point(p_navigation_layers, p_uniformly);
}

RID GodotNavigationServer2D::region_create() {
	MutexLock lock(operations_mutex);

	RID rid = region_owner.make_rid();
	NavRegion2D *reg = region_owner.get_or_null(rid);
	reg->set_self(rid);
	return rid;
}

COMMAND_2(region_set_enabled, RID, p_region, bool, p_enabled) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_enabled(p_enabled);
}

bool GodotNavigationServer2D::region_get_enabled(RID p_region) const {
	const NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, false);

	return region->get_enabled();
}

COMMAND_2(region_set_use_edge_connections, RID, p_region, bool, p_enabled) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_use_edge_connections(p_enabled);
}

bool GodotNavigationServer2D::region_get_use_edge_connections(RID p_region) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, false);

	return region->get_use_edge_connections();
}

COMMAND_2(region_set_enter_cost, RID, p_region, real_t, p_enter_cost) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);
	ERR_FAIL_COND(p_enter_cost < 0.0);

	region->set_enter_cost(p_enter_cost);
}

real_t GodotNavigationServer2D::region_get_enter_cost(RID p_region) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, 0);

	return region->get_enter_cost();
}

COMMAND_2(region_set_travel_cost, RID, p_region, real_t, p_travel_cost) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);
	ERR_FAIL_COND(p_travel_cost < 0.0);

	region->set_travel_cost(p_travel_cost);
}

real_t GodotNavigationServer2D::region_get_travel_cost(RID p_region) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, 0);

	return region->get_travel_cost();
}

COMMAND_2(region_set_owner_id, RID, p_region, ObjectID, p_owner_id) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_owner_id(p_owner_id);
}

ObjectID GodotNavigationServer2D::region_get_owner_id(RID p_region) const {
	const NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, ObjectID());

	return region->get_owner_id();
}

bool GodotNavigationServer2D::region_owns_point(RID p_region, const Vector2 &p_point) const {
	const NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, false);

	if (region->get_map()) {
		RID closest_point_owner = map_get_closest_point_owner(region->get_map()->get_self(), p_point);
		return closest_point_owner == region->get_self();
	}
	return false;
}

COMMAND_2(region_set_map, RID, p_region, RID, p_map) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	NavMap2D *map = map_owner.get_or_null(p_map);

	region->set_map(map);
}

COMMAND_2(region_set_navigation_layers, RID, p_region, uint32_t, p_navigation_layers) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_navigation_layers(p_navigation_layers);
}

uint32_t GodotNavigationServer2D::region_get_navigation_layers(RID p_region) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, 0);

	return region->get_navigation_layers();
}

COMMAND_2(region_set_transform, RID, p_region, Transform2D, p_transform) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_transform(p_transform);
}

Transform2D GodotNavigationServer2D::region_get_transform(RID p_region) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Transform2D());

	return region->get_transform();
}

void GodotNavigationServer2D::region_set_navigation_polygon(RID p_region, Ref<NavigationPolygon> p_navigation_polygon) {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL(region);

	region->set_navigation_polygon(p_navigation_polygon);
}

int GodotNavigationServer2D::region_get_connections_count(RID p_region) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, 0);
	NavMap2D *map = region->get_map();
	if (map) {
		return map->get_region_connections_count(region);
	}
	return 0;
}

Vector2 GodotNavigationServer2D::region_get_connection_pathway_start(RID p_region, int p_connection_id) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector2());
	NavMap2D *map = region->get_map();
	if (map) {
		return map->get_region_connection_pathway_start(region, p_connection_id);
	}
	return Vector2();
}

Vector2 GodotNavigationServer2D::region_get_connection_pathway_end(RID p_region, int p_connection_id) const {
	NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector2());
	NavMap2D *map = region->get_map();
	if (map) {
		return map->get_region_connection_pathway_end(region, p_connection_id);
	}
	return Vector2();
}

Vector2 GodotNavigationServer2D::region_get_closest_point(RID p_region, const Vector2 &p_point) const {
	const NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector2());

	return region->get_closest_point_info(p_point).point;
}

Vector2 GodotNavigationServer2D::region_get_random_point(RID p_region, uint32_t p_navigation_layers, bool p_uniformly) const {
	const NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Vector2());

	return region->get_random_point(p_navigation_layers, p_uniformly);
}

Rect2 GodotNavigationServer2D::region_get_bounds(RID p_region) const {
	const NavRegion2D *region = region_owner.get_or_null(p_region);
	ERR_FAIL_NULL_V(region, Rect2());

	return region->get_bounds();
}

RID GodotNavigationServer2D::link_create() {
	MutexLock lock(operations_mutex);

	RID rid = link_owner.make_rid();
	NavLink2D *link = link_owner.get_or_null(rid);
	link->set_self(rid);
	return rid;
}

COMMAND_2(link_set_map, RID, p_link, RID, p_map) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	NavMap2D *map = map_owner.get_or_null(p_map);

	link->set_map(map);
}

RID GodotNavigationServer2D::link_get_map(const RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, RID());

	if (link->get_map()) {
		return link->get_map()->get_self();
	}
	return RID();
}

COMMAND_2(link_set_enabled, RID, p_link, bool, p_enabled) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_enabled(p_enabled);
}

bool GodotNavigationServer2D::link_get_enabled(RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, false);

	return link->get_enabled();
}

COMMAND_2(link_set_bidirectional, RID, p_link, bool, p_bidirectional) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_bidirectional(p_bidirectional);
}

bool GodotNavigationServer2D::link_is_bidirectional(RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, false);

	return link->is_bidirectional();
}

COMMAND_2(link_set_navigation_layers, RID, p_link, uint32_t, p_navigation_layers) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_navigation_layers(p_navigation_layers);
}

uint32_t GodotNavigationServer2D::link_get_navigation_layers(const RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, 0);

	return link->get_navigation_layers();
}

COMMAND_2(link_set_start_position, RID, p_link, Vector2, p_position) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_start_position(p_position);
}

Vector2 GodotNavigationServer2D::link_get_start_position(RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, Vector2());

	return link->get_start_position();
}

COMMAND_2(link_set_end_position, RID, p_link, Vector2, p_position) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_end_position(p_position);
}

Vector2 GodotNavigationServer2D::link_get_end_position(RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, Vector2());

	return link->get_end_position();
}

COMMAND_2(link_set_enter_cost, RID, p_link, real_t, p_enter_cost) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_enter_cost(p_enter_cost);
}

real_t GodotNavigationServer2D::link_get_enter_cost(const RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, 0);

	return link->get_enter_cost();
}

COMMAND_2(link_set_travel_cost, RID, p_link, real_t, p_travel_cost) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_travel_cost(p_travel_cost);
}

real_t GodotNavigationServer2D::link_get_travel_cost(const RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, 0);

	return link->get_travel_cost();
}

COMMAND_2(link_set_owner_id, RID, p_link, ObjectID, p_owner_id) {
	NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL(link);

	link->set_owner_id(p_owner_id);
}

ObjectID GodotNavigationServer2D::link_get_owner_id(RID p_link) const {
	const NavLink2D *link = link_owner.get_or_null(p_link);
	ERR_FAIL_NULL_V(link, ObjectID());

	return link->get_owner_id();
}

RID GodotNavigationServer2D::agent_create() {
	MutexLock lock(operations_mutex);

	RID rid = agent_owner.make_rid();
	NavAgent2D *agent = agent_owner.get_or_null(rid);
	agent->set_self(rid);
	return rid;
}

COMMAND_2(agent_set_avoidance_enabled, RID, p_agent, bool, p_enabled) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_avoidance_enabled(p_enabled);
}

bool GodotNavigationServer2D::agent_get_avoidance_enabled(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->is_avoidance_enabled();
}

COMMAND_2(agent_set_map, RID, p_agent, RID, p_map) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	NavMap2D *map = map_owner.get_or_null(p_map);

	agent->set_map(map);
}

COMMAND_2(agent_set_neighbor_distance, RID, p_agent, real_t, p_distance) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_neighbor_distance(p_distance);
}

real_t GodotNavigationServer2D::agent_get_neighbor_distance(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_neighbor_distance();
}

COMMAND_2(agent_set_max_neighbors, RID, p_agent, int, p_count) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_max_neighbors(p_count);
}

int GodotNavigationServer2D::agent_get_max_neighbors(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_max_neighbors();
}

COMMAND_2(agent_set_time_horizon_agents, RID, p_agent, real_t, p_time_horizon) {
	ERR_FAIL_COND_MSG(p_time_horizon < 0.0, "Time horizon must be positive.");
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_time_horizon_agents(p_time_horizon);
}

real_t GodotNavigationServer2D::agent_get_time_horizon_agents(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_time_horizon_agents();
}

COMMAND_2(agent_set_time_horizon_obstacles, RID, p_agent, real_t, p_time_horizon) {
	ERR_FAIL_COND_MSG(p_time_horizon < 0.0, "Time horizon must be positive.");
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_time_horizon_obstacles(p_time_horizon);
}

real_t GodotNavigationServer2D::agent_get_time_horizon_obstacles(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_time_horizon_obstacles();
}

COMMAND_2(agent_set_radius, RID, p_agent, real_t, p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0.0, "Radius must be positive.");
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_radius(p_radius);
}

real_t GodotNavigationServer2D::agent_get_radius(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_radius();
}

COMMAND_2(agent_set_max_speed, RID, p_agent, real_t, p_max_speed) {
	ERR_FAIL_COND_MSG(p_max_speed < 0.0, "Max speed must be positive.");
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_max_speed(p_max_speed);
}

real_t GodotNavigationServer2D::agent_get_max_speed(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_max_speed();
}

COMMAND_2(agent_set_velocity_forced, RID, p_agent, Vector2, p_velocity) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_velocity_forced(p_velocity);
}

COMMAND_2(agent_set_velocity, RID, p_agent, Vector2, p_velocity) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_velocity(p_velocity);
}

Vector2 GodotNavigationServer2D::agent_get_velocity(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, Vector2());

	return agent->get_velocity();
}

COMMAND_2(agent_set_position, RID, p_agent, Vector2, p_position) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_position(p_position);
}

Vector2 GodotNavigationServer2D::agent_get_position(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, Vector2());

	return agent->get_position();
}

bool GodotNavigationServer2D::agent_is_map_changed(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->is_map_changed();
}

COMMAND_2(agent_set_avoidance_callback, RID, p_agent, Callable, p_callback) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
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

bool GodotNavigationServer2D::agent_has_avoidance_callback(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->has_avoidance_callback();
}

COMMAND_2(agent_set_avoidance_layers, RID, p_agent, uint32_t, p_layers) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);
	agent->set_avoidance_layers(p_layers);
}

uint32_t GodotNavigationServer2D::agent_get_avoidance_layers(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_avoidance_layers();
}

COMMAND_2(agent_set_avoidance_mask, RID, p_agent, uint32_t, p_mask) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);
	agent->set_avoidance_mask(p_mask);
}

uint32_t GodotNavigationServer2D::agent_get_avoidance_mask(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_avoidance_mask();
}

COMMAND_2(agent_set_avoidance_priority, RID, p_agent, real_t, p_priority) {
	ERR_FAIL_COND_MSG(p_priority < 0.0, "Avoidance priority must be between 0.0 and 1.0 inclusive.");
	ERR_FAIL_COND_MSG(p_priority > 1.0, "Avoidance priority must be between 0.0 and 1.0 inclusive.");
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);
	agent->set_avoidance_priority(p_priority);
}

real_t GodotNavigationServer2D::agent_get_avoidance_priority(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, 0);

	return agent->get_avoidance_priority();
}

COMMAND_2(agent_set_paused, RID, p_agent, bool, p_paused) {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL(agent);

	agent->set_paused(p_paused);
}

bool GodotNavigationServer2D::agent_get_paused(RID p_agent) const {
	NavAgent2D *agent = agent_owner.get_or_null(p_agent);
	ERR_FAIL_NULL_V(agent, false);

	return agent->get_paused();
}

RID GodotNavigationServer2D::obstacle_create() {
	MutexLock lock(operations_mutex);

	RID rid = obstacle_owner.make_rid();
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(rid);
	obstacle->set_self(rid);

	RID agent_rid = agent_owner.make_rid();
	NavAgent2D *agent = agent_owner.get_or_null(agent_rid);
	agent->set_self(agent_rid);

	obstacle->set_agent(agent);

	return rid;
}

COMMAND_2(obstacle_set_avoidance_enabled, RID, p_obstacle, bool, p_enabled) {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_avoidance_enabled(p_enabled);
}

bool GodotNavigationServer2D::obstacle_get_avoidance_enabled(RID p_obstacle) const {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, false);

	return obstacle->is_avoidance_enabled();
}

COMMAND_2(obstacle_set_map, RID, p_obstacle, RID, p_map) {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	NavMap2D *map = map_owner.get_or_null(p_map);

	obstacle->set_map(map);
}

RID GodotNavigationServer2D::obstacle_get_map(RID p_obstacle) const {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, RID());
	if (obstacle->get_map()) {
		return obstacle->get_map()->get_self();
	}
	return RID();
}

COMMAND_2(obstacle_set_paused, RID, p_obstacle, bool, p_paused) {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_paused(p_paused);
}

bool GodotNavigationServer2D::obstacle_get_paused(RID p_obstacle) const {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, false);

	return obstacle->get_paused();
}

COMMAND_2(obstacle_set_radius, RID, p_obstacle, real_t, p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0.0, "Radius must be positive.");
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_radius(p_radius);
}

real_t GodotNavigationServer2D::obstacle_get_radius(RID p_obstacle) const {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, 0);

	return obstacle->get_radius();
}

COMMAND_2(obstacle_set_velocity, RID, p_obstacle, Vector2, p_velocity) {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);

	obstacle->set_velocity(p_velocity);
}

Vector2 GodotNavigationServer2D::obstacle_get_velocity(RID p_obstacle) const {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, Vector2());

	return obstacle->get_velocity();
}

COMMAND_2(obstacle_set_position, RID, p_obstacle, Vector2, p_position) {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);
	obstacle->set_position(p_position);
}

Vector2 GodotNavigationServer2D::obstacle_get_position(RID p_obstacle) const {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, Vector2());

	return obstacle->get_position();
}

COMMAND_2(obstacle_set_avoidance_layers, RID, p_obstacle, uint32_t, p_layers) {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);
	obstacle->set_avoidance_layers(p_layers);
}

uint32_t GodotNavigationServer2D::obstacle_get_avoidance_layers(RID p_obstacle) const {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, 0);

	return obstacle->get_avoidance_layers();
}

void GodotNavigationServer2D::obstacle_set_vertices(RID p_obstacle, const Vector<Vector2> &p_vertices) {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL(obstacle);
	obstacle->set_vertices(p_vertices);
}

Vector<Vector2> GodotNavigationServer2D::obstacle_get_vertices(RID p_obstacle) const {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_obstacle);
	ERR_FAIL_NULL_V(obstacle, Vector<Vector2>());

	return obstacle->get_vertices();
}

void GodotNavigationServer2D::flush_queries() {
	// In c++ we can't be sure that this is performed in the main thread
	// even with mutable functions.
	MutexLock lock(commands_mutex);
	MutexLock lock2(operations_mutex);

	for (SetCommand2D *command : commands) {
		command->exec(this);
		memdelete(command);
	}
	commands.clear();
}

COMMAND_1(free, RID, p_object) {
	if (geometry_parser_owner.owns(p_object)) {
		RWLockWrite write_lock(geometry_parser_rwlock);

		NavMeshGeometryParser2D *parser = geometry_parser_owner.get_or_null(p_object);
		ERR_FAIL_NULL(parser);

		generator_parsers.erase(parser);
#ifndef CLIPPER2_ENABLED
		NavMeshGenerator2D::get_singleton()->set_generator_parsers(generator_parsers);
#endif
		geometry_parser_owner.free(parser->self);
		return;
	}
	if (map_owner.owns(p_object)) {
		NavMap2D *map = map_owner.get_or_null(p_object);

		// Removes any assigned region.
		for (NavRegion2D *region : map->get_regions()) {
			map->remove_region(region);
			region->set_map(nullptr);
		}

		// Removes any assigned links.
		for (NavLink2D *link : map->get_links()) {
			map->remove_link(link);
			link->set_map(nullptr);
		}

		// Remove any assigned agent.
		for (NavAgent2D *agent : map->get_agents()) {
			map->remove_agent(agent);
			agent->set_map(nullptr);
		}

		// Remove any assigned obstacles.
		for (NavObstacle2D *obstacle : map->get_obstacles()) {
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
		NavRegion2D *region = region_owner.get_or_null(p_object);

		// Removes this region from the map if assigned.
		if (region->get_map() != nullptr) {
			region->get_map()->remove_region(region);
			region->set_map(nullptr);
		}

		region_owner.free(p_object);

	} else if (link_owner.owns(p_object)) {
		NavLink2D *link = link_owner.get_or_null(p_object);

		// Removes this link from the map if assigned.
		if (link->get_map() != nullptr) {
			link->get_map()->remove_link(link);
			link->set_map(nullptr);
		}

		link_owner.free(p_object);

	} else if (agent_owner.owns(p_object)) {
		internal_free_agent(p_object);

	} else if (obstacle_owner.owns(p_object)) {
		internal_free_obstacle(p_object);

	} else {
		ERR_PRINT("Attempted to free a NavigationServer RID that did not exist (or was already freed).");
	}
}

void GodotNavigationServer2D::internal_free_agent(RID p_object) {
	NavAgent2D *agent = agent_owner.get_or_null(p_object);
	if (agent) {
		if (agent->get_map() != nullptr) {
			agent->get_map()->remove_agent(agent);
			agent->set_map(nullptr);
		}
		agent_owner.free(p_object);
	}
}

void GodotNavigationServer2D::internal_free_obstacle(RID p_object) {
	NavObstacle2D *obstacle = obstacle_owner.get_or_null(p_object);
	if (obstacle) {
		NavAgent2D *obstacle_agent = obstacle->get_agent();
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

void GodotNavigationServer2D::process(real_t p_delta_time) {
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

void GodotNavigationServer2D::set_active(bool p_active) {
	MutexLock lock(operations_mutex);

	active = p_active;
}

void GodotNavigationServer2D::query_path(const Ref<NavigationPathQueryParameters2D> &p_query_parameters, Ref<NavigationPathQueryResult2D> p_query_result, const Callable &p_callback) {
	ERR_FAIL_COND(p_query_parameters.is_null());
	ERR_FAIL_COND(p_query_result.is_null());

	NavMap2D *map = map_owner.get_or_null(p_query_parameters->get_map());
	ERR_FAIL_NULL(map);

	NavMeshQueries2D::map_query_path(map, p_query_parameters, p_query_result, p_callback);
}

RID GodotNavigationServer2D::source_geometry_parser_create() {
	RWLockWrite write_lock(geometry_parser_rwlock);

	RID rid = geometry_parser_owner.make_rid();

	NavMeshGeometryParser2D *parser = geometry_parser_owner.get_or_null(rid);
	parser->self = rid;

	generator_parsers.push_back(parser);
#ifdef CLIPPER2_ENABLED
	NavMeshGenerator2D::get_singleton()->set_generator_parsers(generator_parsers);
#endif
	return rid;
}

void GodotNavigationServer2D::source_geometry_parser_set_callback(RID p_parser, const Callable &p_callback) {
	RWLockWrite write_lock(geometry_parser_rwlock);

	NavMeshGeometryParser2D *parser = geometry_parser_owner.get_or_null(p_parser);
	ERR_FAIL_NULL(parser);

	parser->callback = p_callback;
}

int GodotNavigationServer2D::get_process_info(ProcessInfo p_info) const {
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
