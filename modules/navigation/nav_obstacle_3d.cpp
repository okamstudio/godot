/**************************************************************************/
/*  nav_obstacle_3d.cpp                                                   */
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

#include "nav_obstacle_3d.h"

#include "nav_agent_3d.h"
#include "nav_map_3d.h"

void NavObstacle3D::set_agent(NavAgent3D *p_agent) {
	if (agent == p_agent) {
		return;
	}

	agent = p_agent;

	internal_update_agent();

	request_sync();
}

void NavObstacle3D::set_avoidance_enabled(bool p_enabled) {
	if (avoidance_enabled == p_enabled) {
		return;
	}

	avoidance_enabled = p_enabled;
	obstacle_dirty = true;

	internal_update_agent();

	request_sync();
}

void NavObstacle3D::set_use_3d_avoidance(bool p_enabled) {
	if (use_3d_avoidance == p_enabled) {
		return;
	}

	use_3d_avoidance = p_enabled;
	obstacle_dirty = true;

	if (agent) {
		agent->set_use_3d_avoidance(use_3d_avoidance);
	}

	request_sync();
}

void NavObstacle3D::set_map(NavMap3D *p_map) {
	if (map == p_map) {
		return;
	}

	cancel_sync_request();

	if (map) {
		map->remove_obstacle(this);
		if (agent) {
			agent->set_map(nullptr);
		}
	}

	map = p_map;
	obstacle_dirty = true;

	if (map) {
		map->add_obstacle(this);
		internal_update_agent();

		request_sync();
	}
}

void NavObstacle3D::set_position(const Vector3 p_position) {
	if (position == p_position) {
		return;
	}

	position = p_position;
	obstacle_dirty = true;

	if (agent) {
		agent->set_position(position);
	}

	request_sync();
}

void NavObstacle3D::set_radius(real_t p_radius) {
	if (radius == p_radius) {
		return;
	}

	radius = p_radius;

	if (agent) {
		agent->set_radius(radius);
	}
}

void NavObstacle3D::set_height(const real_t p_height) {
	if (height == p_height) {
		return;
	}

	height = p_height;
	obstacle_dirty = true;

	if (agent) {
		agent->set_height(height);
	}

	request_sync();
}

void NavObstacle3D::set_velocity(const Vector3 p_velocity) {
	velocity = p_velocity;

	if (agent) {
		agent->set_velocity(velocity);
	}
}

void NavObstacle3D::set_vertices(const Vector<Vector3> &p_vertices) {
	if (vertices == p_vertices) {
		return;
	}

	vertices = p_vertices;
	obstacle_dirty = true;

	request_sync();
}

bool NavObstacle3D::is_map_changed() {
	if (map) {
		bool is_changed = map->get_iteration_id() != last_map_iteration_id;
		last_map_iteration_id = map->get_iteration_id();
		return is_changed;
	} else {
		return false;
	}
}

void NavObstacle3D::set_avoidance_layers(uint32_t p_layers) {
	if (avoidance_layers == p_layers) {
		return;
	}

	avoidance_layers = p_layers;
	obstacle_dirty = true;

	if (agent) {
		agent->set_avoidance_layers(avoidance_layers);
	}

	request_sync();
}

bool NavObstacle3D::is_dirty() const {
	return obstacle_dirty;
}

void NavObstacle3D::sync() {
	obstacle_dirty = false;
}

void NavObstacle3D::internal_update_agent() {
	if (agent) {
		agent->set_neighbor_distance(0.0);
		agent->set_max_neighbors(0.0);
		agent->set_time_horizon_agents(0.0);
		agent->set_time_horizon_obstacles(0.0);
		agent->set_avoidance_mask(0.0);
		agent->set_neighbor_distance(0.0);
		agent->set_avoidance_priority(1.0);
		agent->set_map(map);
		agent->set_paused(paused);
		agent->set_radius(radius);
		agent->set_height(height);
		agent->set_position(position);
		agent->set_avoidance_layers(avoidance_layers);
		agent->set_avoidance_enabled(avoidance_enabled);
		agent->set_use_3d_avoidance(use_3d_avoidance);
	}
}

void NavObstacle3D::set_paused(bool p_paused) {
	if (paused == p_paused) {
		return;
	}

	paused = p_paused;

	if (map) {
		if (paused) {
			map->remove_obstacle(this);
		} else {
			map->add_obstacle(this);
		}
	}
	internal_update_agent();
}

bool NavObstacle3D::get_paused() const {
	return paused;
}

void NavObstacle3D::request_sync() {
	if (map && !sync_dirty_request_list_element.in_list()) {
		map->add_obstacle_sync_dirty_request(&sync_dirty_request_list_element);
	}
}

void NavObstacle3D::cancel_sync_request() {
	if (map && sync_dirty_request_list_element.in_list()) {
		map->remove_obstacle_sync_dirty_request(&sync_dirty_request_list_element);
	}
}

NavObstacle3D::NavObstacle3D() :
		sync_dirty_request_list_element(this) {
}

NavObstacle3D::~NavObstacle3D() {
	cancel_sync_request();
}
