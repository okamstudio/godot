/**************************************************************************/
/*  nav_map_2d.cpp                                                        */
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

#include "nav_map_2d.h"

#include "2d/nav_map_builder_2d.h"
#include "2d/nav_mesh_queries_2d.h"
#include "2d/nav_region_iteration_2d.h"
#include "nav_agent_2d.h"
#include "nav_link_2d.h"
#include "nav_obstacle_2d.h"
#include "nav_region_2d.h"

#include "core/config/project_settings.h"
#include "core/object/worker_thread_pool.h"

#include <Obstacle2d.h>

using namespace nav_2d;

#ifdef DEBUG_ENABLED
#define NAVMAP_ITERATION_ZERO_ERROR_MSG() \
	ERR_PRINT_ONCE("NavigationServer navigation map query failed because it was made before first map synchronization.\n\
	NavigationServer 'map_changed' signal can be used to receive update notifications.\n\
	NavigationServer 'map_get_iteration_id()' can be used to check if a map has finished its newest iteration.");
#else
#define NAVMAP_ITERATION_ZERO_ERROR_MSG()
#endif // DEBUG_ENABLED

#define GET_MAP_ITERATION()                                                   \
	iteration_slot_rwlock.read_lock();                                        \
	NavMapIteration2D &map_iteration = iteration_slots[iteration_slot_index]; \
	NavMapIterationRead2D iteration_read_lock(map_iteration);                 \
	iteration_slot_rwlock.read_unlock();

#define GET_MAP_ITERATION_CONST()                                                   \
	iteration_slot_rwlock.read_lock();                                              \
	const NavMapIteration2D &map_iteration = iteration_slots[iteration_slot_index]; \
	NavMapIterationRead2D iteration_read_lock(map_iteration);                       \
	iteration_slot_rwlock.read_unlock();

void NavMap2D::set_cell_size(real_t p_cell_size) {
	if (cell_size == p_cell_size) {
		return;
	}
	cell_size = MAX(p_cell_size, NavigationDefaults2D::navmesh_cell_size_min);
	_update_merge_rasterizer_cell_dimensions();
	map_settings_dirty = true;
}

void NavMap2D::set_merge_rasterizer_cell_scale(float p_value) {
	if (merge_rasterizer_cell_scale == p_value) {
		return;
	}
	merge_rasterizer_cell_scale = MAX(p_value, NavigationDefaults2D::navmesh_cell_size_min);
	_update_merge_rasterizer_cell_dimensions();
	map_settings_dirty = true;
}

void NavMap2D::set_use_edge_connections(bool p_enabled) {
	if (use_edge_connections == p_enabled) {
		return;
	}
	use_edge_connections = p_enabled;
	iteration_dirty = true;
}

void NavMap2D::set_edge_connection_margin(real_t p_edge_connection_margin) {
	if (edge_connection_margin == p_edge_connection_margin) {
		return;
	}
	edge_connection_margin = p_edge_connection_margin;
	iteration_dirty = true;
}

void NavMap2D::set_link_connection_radius(real_t p_link_connection_radius) {
	if (link_connection_radius == p_link_connection_radius) {
		return;
	}
	link_connection_radius = p_link_connection_radius;
	iteration_dirty = true;
}

Vector2 NavMap2D::get_merge_rasterizer_cell_size() const {
	return merge_rasterizer_cell_size;
}

PointKey NavMap2D::get_point_key(const Vector2 &p_pos) const {
	const int x = static_cast<int>(Math::floor(p_pos.x / merge_rasterizer_cell_size.x));
	const int y = static_cast<int>(Math::floor(p_pos.y / merge_rasterizer_cell_size.y));

	PointKey p;
	p.key = 0;
	p.x = x;
	p.y = y;
	return p;
}

void NavMap2D::query_path(NavMeshQueries2D::NavMeshPathQueryTask2D &p_query_task) {
	if (iteration_id == 0) {
		return;
	}

	GET_MAP_ITERATION();

	map_iteration.path_query_slots_semaphore.wait();

	map_iteration.path_query_slots_mutex.lock();
	for (NavMeshQueries2D::PathQuerySlot &p_path_query_slot : map_iteration.path_query_slots) {
		if (!p_path_query_slot.in_use) {
			p_path_query_slot.in_use = true;
			p_query_task.path_query_slot = &p_path_query_slot;
			break;
		}
	}
	map_iteration.path_query_slots_mutex.unlock();

	if (p_query_task.path_query_slot == nullptr) {
		map_iteration.path_query_slots_semaphore.post();
		ERR_FAIL_NULL_MSG(p_query_task.path_query_slot, "No unused NavMap2D path query slot found! This should never happen :(.");
	}

	NavMeshQueries2D::query_task_map_iteration_get_path(p_query_task, map_iteration);

	map_iteration.path_query_slots_mutex.lock();
	uint32_t used_slot_index = p_query_task.path_query_slot->slot_index;
	map_iteration.path_query_slots[used_slot_index].in_use = false;
	p_query_task.path_query_slot = nullptr;
	map_iteration.path_query_slots_mutex.unlock();

	map_iteration.path_query_slots_semaphore.post();
}

Vector2 NavMap2D::get_closest_point(const Vector2 &p_point) const {
	if (iteration_id == 0) {
		NAVMAP_ITERATION_ZERO_ERROR_MSG();
		return Vector2();
	}

	GET_MAP_ITERATION_CONST();

	return NavMeshQueries2D::map_iteration_get_closest_point(map_iteration, p_point);
}

RID NavMap2D::get_closest_point_owner(const Vector2 &p_point) const {
	if (iteration_id == 0) {
		NAVMAP_ITERATION_ZERO_ERROR_MSG();
		return RID();
	}

	GET_MAP_ITERATION_CONST();

	return NavMeshQueries2D::map_iteration_get_closest_point_owner(map_iteration, p_point);
}

ClosestPointQueryResult NavMap2D::get_closest_point_info(const Vector2 &p_point) const {
	GET_MAP_ITERATION_CONST();

	return NavMeshQueries2D::map_iteration_get_closest_point_info(map_iteration, p_point);
}

void NavMap2D::add_region(NavRegion2D *p_region) {
	regions.push_back(p_region);
	iteration_dirty = true;
}

void NavMap2D::remove_region(NavRegion2D *p_region) {
	int64_t region_index = regions.find(p_region);
	if (region_index >= 0) {
		regions.remove_at_unordered(region_index);
		iteration_dirty = true;
	}
}

void NavMap2D::add_link(NavLink2D *p_link) {
	links.push_back(p_link);
	iteration_dirty = true;
}

void NavMap2D::remove_link(NavLink2D *p_link) {
	int64_t link_index = links.find(p_link);
	if (link_index >= 0) {
		links.remove_at_unordered(link_index);
		iteration_dirty = true;
	}
}

bool NavMap2D::has_agent(NavAgent2D *p_agent) const {
	return agents.has(p_agent);
}

void NavMap2D::add_agent(NavAgent2D *p_agent) {
	if (!has_agent(p_agent)) {
		agents.push_back(p_agent);
		agents_dirty = true;
	}
}

void NavMap2D::remove_agent(NavAgent2D *p_agent) {
	remove_agent_as_controlled(p_agent);
	int64_t agent_index = agents.find(p_agent);
	if (agent_index >= 0) {
		agents.remove_at_unordered(agent_index);
		agents_dirty = true;
	}
}

bool NavMap2D::has_obstacle(NavObstacle2D *p_obstacle) const {
	return obstacles.has(p_obstacle);
}

void NavMap2D::add_obstacle(NavObstacle2D *p_obstacle) {
	if (p_obstacle->get_paused()) {
		// No point in adding a paused obstacle, it will add itself when unpaused again.
		return;
	}

	if (!has_obstacle(p_obstacle)) {
		obstacles.push_back(p_obstacle);
		obstacles_dirty = true;
	}
}

void NavMap2D::remove_obstacle(NavObstacle2D *p_obstacle) {
	int64_t obstacle_index = obstacles.find(p_obstacle);
	if (obstacle_index >= 0) {
		obstacles.remove_at_unordered(obstacle_index);
		obstacles_dirty = true;
	}
}

void NavMap2D::set_agent_as_controlled(NavAgent2D *p_agent) {
	remove_agent_as_controlled(p_agent);

	if (p_agent->get_paused()) {
		// No point in adding a paused agent, it will add itself when unpaused again.
		return;
	}

	int64_t agent_index = active_avoidance_agents.find(p_agent);
	if (agent_index < 0) {
		active_avoidance_agents.push_back(p_agent);
		agents_dirty = true;
	}
}

void NavMap2D::remove_agent_as_controlled(NavAgent2D *p_agent) {
	int64_t agent_index = active_avoidance_agents.find(p_agent);
	if (agent_index >= 0) {
		active_avoidance_agents.remove_at_unordered(agent_index);
		agents_dirty = true;
	}
}

Vector2 NavMap2D::get_random_point(uint32_t p_navigation_layers, bool p_uniformly) const {
	GET_MAP_ITERATION_CONST();

	return NavMeshQueries2D::map_iteration_get_random_point(map_iteration, p_navigation_layers, p_uniformly);
}

void NavMap2D::_build_iteration() {
	if (!iteration_dirty || iteration_building || iteration_ready) {
		return;
	}

	// Get the next free iteration slot that should be potentially unused.
	iteration_slot_rwlock.read_lock();
	NavMapIteration2D &next_map_iteration = iteration_slots[(iteration_slot_index + 1) % 2];
	// Check if the iteration slot is truly free or still used by an external thread.
	bool iteration_is_free = next_map_iteration.users.get() == 0;
	iteration_slot_rwlock.read_unlock();

	if (!iteration_is_free) {
		// A long running pathfinding thread or something is still reading
		// from this older iteration and needs to finish first.
		// Return and wait for the next sync cycle to check again.
		return;
	}

	// Iteration slot is free and no longer used by anything, let's build.

	iteration_dirty = false;
	iteration_building = true;
	iteration_ready = false;

	// We don't need to hold any lock because at this point nothing else can touch it.
	// All new queries are already forwarded to the other iteration slot.

	iteration_build.reset();

	iteration_build.merge_rasterizer_cell_size = get_merge_rasterizer_cell_size();
	iteration_build.use_edge_connections = get_use_edge_connections();
	iteration_build.edge_connection_margin = get_edge_connection_margin();
	iteration_build.link_connection_radius = get_link_connection_radius();

	uint32_t enabled_region_count = 0;
	uint32_t enabled_link_count = 0;

	for (NavRegion2D *region : regions) {
		if (!region->get_enabled()) {
			continue;
		}
		enabled_region_count++;
	}
	for (NavLink2D *link : links) {
		if (!link->get_enabled()) {
			continue;
		}
		enabled_link_count++;
	}

	next_map_iteration.region_ptr_to_region_id.clear();

	next_map_iteration.region_iterations.clear();
	next_map_iteration.link_iterations.clear();

	next_map_iteration.region_iterations.resize(enabled_region_count);
	next_map_iteration.link_iterations.resize(enabled_link_count);

	uint32_t region_id_count = 0;
	uint32_t link_id_count = 0;

	for (NavRegion2D *region : regions) {
		if (!region->get_enabled()) {
			continue;
		}
		NavRegionIteration2D &region_iteration = next_map_iteration.region_iterations[region_id_count];
		region_iteration.id = region_id_count++;
		region->get_iteration_update(region_iteration);
		next_map_iteration.region_ptr_to_region_id[region] = (uint32_t)region_iteration.id;
	}
	for (NavLink2D *link : links) {
		if (!link->get_enabled()) {
			continue;
		}
		NavLinkIteration2D &link_iteration = next_map_iteration.link_iterations[link_id_count];
		link_iteration.id = link_id_count++;
		link->get_iteration_update(link_iteration);
	}

	iteration_build.map_iteration = &next_map_iteration;

	if (use_async_iterations) {
		iteration_build_thread_task_id = WorkerThreadPool::get_singleton()->add_native_task(&NavMap2D::_build_iteration_threaded, &iteration_build, true, SNAME("NavMapBuilder2D"));
	} else {
		NavMapBuilder2D::build_navmap_iteration(iteration_build);

		iteration_building = false;
		iteration_ready = true;
	}
}

void NavMap2D::_build_iteration_threaded(void *p_arg) {
	NavMapIterationBuild2D *_iteration_build = static_cast<NavMapIterationBuild2D *>(p_arg);

	NavMapBuilder2D::build_navmap_iteration(*_iteration_build);
}

void NavMap2D::_sync_iteration() {
	if (iteration_building || !iteration_ready) {
		return;
	}

	performance_data.pm_polygon_count = iteration_build.performance_data.pm_polygon_count;
	performance_data.pm_edge_count = iteration_build.performance_data.pm_edge_count;
	performance_data.pm_edge_merge_count = iteration_build.performance_data.pm_edge_merge_count;
	performance_data.pm_edge_connection_count = iteration_build.performance_data.pm_edge_connection_count;
	performance_data.pm_edge_free_count = iteration_build.performance_data.pm_edge_free_count;

	iteration_id = iteration_id % UINT32_MAX + 1;

	// Finally ping-pong switch the iteration slot.
	iteration_slot_rwlock.write_lock();
	uint32_t next_iteration_slot_index = (iteration_slot_index + 1) % 2;
	iteration_slot_index = next_iteration_slot_index;
	iteration_slot_rwlock.write_unlock();

	iteration_ready = false;
}

void NavMap2D::sync() {
	// Performance Monitor.
	performance_data.pm_region_count = regions.size();
	performance_data.pm_agent_count = agents.size();
	performance_data.pm_link_count = links.size();
	performance_data.pm_obstacle_count = obstacles.size();

	_sync_dirty_map_update_requests();

	if (iteration_dirty && !iteration_building && !iteration_ready) {
		_build_iteration();
	}
	if (use_async_iterations && iteration_build_thread_task_id != WorkerThreadPool::INVALID_TASK_ID) {
		if (WorkerThreadPool::get_singleton()->is_task_completed(iteration_build_thread_task_id)) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(iteration_build_thread_task_id);

			iteration_build_thread_task_id = WorkerThreadPool::INVALID_TASK_ID;
			iteration_building = false;
			iteration_ready = true;
		}
	}
	if (iteration_ready) {
		_sync_iteration();
	}

	map_settings_dirty = false;

	_sync_avoidance();
}

void NavMap2D::_sync_avoidance() {
	_sync_dirty_avoidance_update_requests();

	if (obstacles_dirty || agents_dirty) {
		_update_rvo_simulation();
	}

	obstacles_dirty = false;
	agents_dirty = false;
}

void NavMap2D::_update_rvo_obstacles_tree() {
	int obstacle_vertex_count = 0;
	for (NavObstacle2D *obstacle : obstacles) {
		obstacle_vertex_count += obstacle->get_vertices().size();
	}

	// Cleaning old obstacles.
	for (size_t i = 0; i < rvo_simulation.obstacles_.size(); ++i) {
		delete rvo_simulation.obstacles_[i];
	}
	rvo_simulation.obstacles_.clear();

	// Cannot use LocalVector here as RVO library expects std::vector to build KdTree
	std::vector<RVO2D::Obstacle2D *> &raw_obstacles = rvo_simulation.obstacles_;
	raw_obstacles.reserve(obstacle_vertex_count);

	// The following block is modified copy from RVO2D::AddObstacle()
	// Obstacles are linked and depend on all other obstacles.
	for (NavObstacle2D *obstacle : obstacles) {
		const Vector2 &_obstacle_position = obstacle->get_position();
		const Vector<Vector2> &_obstacle_vertices = obstacle->get_vertices();

		if (_obstacle_vertices.size() < 2) {
			continue;
		}

		std::vector<RVO2D::Vector2> rvo_vertices;
		rvo_vertices.reserve(_obstacle_vertices.size());

		uint32_t _obstacle_avoidance_layers = obstacle->get_avoidance_layers();

		for (const Vector2 &_obstacle_vertex : _obstacle_vertices) {
			rvo_vertices.push_back(RVO2D::Vector2(_obstacle_vertex.x + _obstacle_position.x, _obstacle_vertex.y + _obstacle_position.y));
		}

		const size_t obstacleNo = raw_obstacles.size();

		for (size_t i = 0; i < rvo_vertices.size(); i++) {
			RVO2D::Obstacle2D *rvo_obstacle = new RVO2D::Obstacle2D();
			rvo_obstacle->point_ = rvo_vertices[i];

			rvo_obstacle->avoidance_layers_ = _obstacle_avoidance_layers;

			if (i != 0) {
				rvo_obstacle->prevObstacle_ = raw_obstacles.back();
				rvo_obstacle->prevObstacle_->nextObstacle_ = rvo_obstacle;
			}

			if (i == rvo_vertices.size() - 1) {
				rvo_obstacle->nextObstacle_ = raw_obstacles[obstacleNo];
				rvo_obstacle->nextObstacle_->prevObstacle_ = rvo_obstacle;
			}

			rvo_obstacle->unitDir_ = normalize(rvo_vertices[(i == rvo_vertices.size() - 1 ? 0 : i + 1)] - rvo_vertices[i]);

			if (rvo_vertices.size() == 2) {
				rvo_obstacle->isConvex_ = true;
			} else {
				rvo_obstacle->isConvex_ = (leftOf(rvo_vertices[(i == 0 ? rvo_vertices.size() - 1 : i - 1)], rvo_vertices[i], rvo_vertices[(i == rvo_vertices.size() - 1 ? 0 : i + 1)]) >= 0.0f);
			}

			rvo_obstacle->id_ = raw_obstacles.size();

			raw_obstacles.push_back(rvo_obstacle);
		}
	}

	rvo_simulation.kdTree_->buildObstacleTree(raw_obstacles);
}

void NavMap2D::_update_rvo_agents_tree() {
	// Cannot use LocalVector here as RVO library expects std::vector to build KdTree.
	std::vector<RVO2D::Agent2D *> raw_agents;
	raw_agents.reserve(active_avoidance_agents.size());
	for (NavAgent2D *agent : active_avoidance_agents) {
		raw_agents.push_back(agent->get_rvo_agent());
	}
	rvo_simulation.kdTree_->buildAgentTree(raw_agents);
}

void NavMap2D::_update_rvo_simulation() {
	if (obstacles_dirty) {
		_update_rvo_obstacles_tree();
	}
	if (agents_dirty) {
		_update_rvo_agents_tree();
	}
}

void NavMap2D::compute_single_avoidance_step(uint32_t p_index, NavAgent2D **p_agent) {
	(*(p_agent + p_index))->get_rvo_agent()->computeNeighbors(&rvo_simulation);
	(*(p_agent + p_index))->get_rvo_agent()->computeNewVelocity(&rvo_simulation);
	(*(p_agent + p_index))->get_rvo_agent()->update(&rvo_simulation);
	(*(p_agent + p_index))->update();
}

void NavMap2D::step(real_t p_deltatime) {
	deltatime = p_deltatime;

	rvo_simulation.setTimeStep(float(deltatime));

	if (active_avoidance_agents.size() > 0) {
		if (use_threads && avoidance_use_multiple_threads) {
			WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(this, &NavMap2D::compute_single_avoidance_step, active_avoidance_agents.ptr(), active_avoidance_agents.size(), -1, true, SNAME("RVOAvoidanceAgents2D"));
			WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);
		} else {
			for (NavAgent2D *agent : active_avoidance_agents) {
				agent->get_rvo_agent()->computeNeighbors(&rvo_simulation);
				agent->get_rvo_agent()->computeNewVelocity(&rvo_simulation);
				agent->get_rvo_agent()->update(&rvo_simulation);
				agent->update();
			}
		}
	}
}

void NavMap2D::dispatch_callbacks() {
	for (NavAgent2D *agent : active_avoidance_agents) {
		agent->dispatch_avoidance_callback();
	}
}

void NavMap2D::_update_merge_rasterizer_cell_dimensions() {
	merge_rasterizer_cell_size.x = cell_size * merge_rasterizer_cell_scale;
	merge_rasterizer_cell_size.y = cell_size * merge_rasterizer_cell_scale;
}

int NavMap2D::get_region_connections_count(NavRegion2D *p_region) const {
	ERR_FAIL_NULL_V(p_region, 0);

	GET_MAP_ITERATION_CONST();

	HashMap<NavRegion2D *, uint32_t>::ConstIterator found_id = map_iteration.region_ptr_to_region_id.find(p_region);
	if (found_id) {
		HashMap<uint32_t, LocalVector<Edge::Connection>>::ConstIterator found_connections = map_iteration.external_region_connections.find(found_id->value);
		if (found_connections) {
			return found_connections->value.size();
		}
	}

	return 0;
}

Vector2 NavMap2D::get_region_connection_pathway_start(NavRegion2D *p_region, int p_connection_id) const {
	ERR_FAIL_NULL_V(p_region, Vector2());

	GET_MAP_ITERATION_CONST();

	HashMap<NavRegion2D *, uint32_t>::ConstIterator found_id = map_iteration.region_ptr_to_region_id.find(p_region);
	if (found_id) {
		HashMap<uint32_t, LocalVector<Edge::Connection>>::ConstIterator found_connections = map_iteration.external_region_connections.find(found_id->value);
		if (found_connections) {
			ERR_FAIL_INDEX_V(p_connection_id, int(found_connections->value.size()), Vector2());
			return found_connections->value[p_connection_id].pathway_start;
		}
	}

	return Vector2();
}

Vector2 NavMap2D::get_region_connection_pathway_end(NavRegion2D *p_region, int p_connection_id) const {
	ERR_FAIL_NULL_V(p_region, Vector2());

	GET_MAP_ITERATION_CONST();

	HashMap<NavRegion2D *, uint32_t>::ConstIterator found_id = map_iteration.region_ptr_to_region_id.find(p_region);
	if (found_id) {
		HashMap<uint32_t, LocalVector<Edge::Connection>>::ConstIterator found_connections = map_iteration.external_region_connections.find(found_id->value);
		if (found_connections) {
			ERR_FAIL_INDEX_V(p_connection_id, int(found_connections->value.size()), Vector2());
			return found_connections->value[p_connection_id].pathway_end;
		}
	}

	return Vector2();
}

void NavMap2D::add_region_sync_dirty_request(SelfList<NavRegion2D> *p_sync_request) {
	if (p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.regions.add(p_sync_request);
}

void NavMap2D::add_link_sync_dirty_request(SelfList<NavLink2D> *p_sync_request) {
	if (p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.links.add(p_sync_request);
}

void NavMap2D::add_agent_sync_dirty_request(SelfList<NavAgent2D> *p_sync_request) {
	if (p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.agents.add(p_sync_request);
}

void NavMap2D::add_obstacle_sync_dirty_request(SelfList<NavObstacle2D> *p_sync_request) {
	if (p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.obstacles.add(p_sync_request);
}

void NavMap2D::remove_region_sync_dirty_request(SelfList<NavRegion2D> *p_sync_request) {
	if (!p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.regions.remove(p_sync_request);
}

void NavMap2D::remove_link_sync_dirty_request(SelfList<NavLink2D> *p_sync_request) {
	if (!p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.links.remove(p_sync_request);
}

void NavMap2D::remove_agent_sync_dirty_request(SelfList<NavAgent2D> *p_sync_request) {
	if (!p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.agents.remove(p_sync_request);
}

void NavMap2D::remove_obstacle_sync_dirty_request(SelfList<NavObstacle2D> *p_sync_request) {
	if (!p_sync_request->in_list()) {
		return;
	}
	sync_dirty_requests.obstacles.remove(p_sync_request);
}

void NavMap2D::_sync_dirty_map_update_requests() {
	// If entire map settings changed make all regions dirty.
	if (map_settings_dirty) {
		for (NavRegion2D *region : regions) {
			region->scratch_polygons();
		}
		iteration_dirty = true;
	}

	if (!iteration_dirty) {
		iteration_dirty = sync_dirty_requests.regions.first() || sync_dirty_requests.links.first();
	}

	// Sync NavRegions.
	for (SelfList<NavRegion2D> *element = sync_dirty_requests.regions.first(); element; element = element->next()) {
		element->self()->sync();
	}
	sync_dirty_requests.regions.clear();

	// Sync NavLinks.
	for (SelfList<NavLink2D> *element = sync_dirty_requests.links.first(); element; element = element->next()) {
		element->self()->sync();
	}
	sync_dirty_requests.links.clear();
}

void NavMap2D::_sync_dirty_avoidance_update_requests() {
	// Sync NavAgents.
	if (!agents_dirty) {
		agents_dirty = sync_dirty_requests.agents.first();
	}
	for (SelfList<NavAgent2D> *element = sync_dirty_requests.agents.first(); element; element = element->next()) {
		element->self()->sync();
	}
	sync_dirty_requests.agents.clear();

	// Sync NavObstacles.
	if (!obstacles_dirty) {
		obstacles_dirty = sync_dirty_requests.obstacles.first();
	}
	for (SelfList<NavObstacle2D> *element = sync_dirty_requests.obstacles.first(); element; element = element->next()) {
		element->self()->sync();
	}
	sync_dirty_requests.obstacles.clear();
}

void NavMap2D::set_use_async_iterations(bool p_enabled) {
	if (use_async_iterations == p_enabled) {
		return;
	}
#ifdef THREADS_ENABLED
	use_async_iterations = p_enabled;
#endif
}

bool NavMap2D::get_use_async_iterations() const {
	return use_async_iterations;
}

NavMap2D::NavMap2D() {
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

	iteration_slots.resize(2);

	for (NavMapIteration2D &iteration_slot : iteration_slots) {
		iteration_slot.path_query_slots.resize(path_query_slots_max);
		for (uint32_t i = 0; i < iteration_slot.path_query_slots.size(); i++) {
			iteration_slot.path_query_slots[i].slot_index = i;
		}
		iteration_slot.path_query_slots_semaphore.post(path_query_slots_max);
	}

#ifdef THREADS_ENABLED
	use_async_iterations = GLOBAL_GET("navigation/world/map_use_async_iterations");
#else
	use_async_iterations = false;
#endif
}

NavMap2D::~NavMap2D() {
	if (iteration_build_thread_task_id != WorkerThreadPool::INVALID_TASK_ID) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(iteration_build_thread_task_id);
		iteration_build_thread_task_id = WorkerThreadPool::INVALID_TASK_ID;
	}
}
