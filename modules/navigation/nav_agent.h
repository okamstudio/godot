/**************************************************************************/
/*  nav_agent.h                                                           */
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

#ifndef NAV_AGENT_H
#define NAV_AGENT_H

#include "nav_rid.h"

#include "core/object/class_db.h"
#include "core/templates/self_list.h"

#include <Agent2d.h>
#include <Agent3d.h>

class NavMap;

class NavAgent : public NavRid {
	Vector3 position;
	Vector3 target_position;
	Vector3 velocity;
	Vector3 velocity_forced;
	real_t height = 1.0;
	real_t radius = 1.0;
	real_t max_speed = 1.0;
	real_t time_horizon_agents = 1.0;
	real_t time_horizon_obstacles = 0.0;
	int max_neighbors = 5;
	real_t neighbor_distance = 5.0;
	Vector3 safe_velocity;
	bool clamp_speed = true; // Experimental, clamps velocity to max_speed.

	NavMap *map = nullptr;

	RVO2D::Agent2D rvo_agent_2d;
	RVO3D::Agent3D rvo_agent_3d;
	bool use_3d_avoidance = false;
	bool avoidance_enabled = false;

	uint32_t avoidance_layers = 1;
	uint32_t avoidance_mask = 1;
	real_t avoidance_priority = 1.0;

	Callable avoidance_callback;

	bool agent_dirty = true;

	uint32_t last_map_iteration_id = 0;
	bool paused = false;

	SelfList<NavAgent> sync_dirty_request_list_element;

public:
	NavAgent();
	~NavAgent();

	void set_avoidance_enabled(bool p_enabled);
	bool is_avoidance_enabled() { return avoidance_enabled; }

	void set_use_3d_avoidance(bool p_enabled);
	bool get_use_3d_avoidance() { return use_3d_avoidance; }

	void set_map(NavMap *p_map);
	NavMap *get_map() { return map; }

	bool is_map_changed();

	RVO2D::Agent2D *get_rvo_agent_2d() { return &rvo_agent_2d; }
	RVO3D::Agent3D *get_rvo_agent_3d() { return &rvo_agent_3d; }

	void set_avoidance_callback(Callable p_callback);
	bool has_avoidance_callback() const;

	void dispatch_avoidance_callback();

	void set_neighbor_distance(real_t p_neighbor_distance);
	real_t get_neighbor_distance() const { return neighbor_distance; }

	void set_max_neighbors(int p_max_neighbors);
	int get_max_neighbors() const { return max_neighbors; }

	void set_time_horizon_agents(real_t p_time_horizon);
	real_t get_time_horizon_agents() const { return time_horizon_agents; }

	void set_time_horizon_obstacles(real_t p_time_horizon);
	real_t get_time_horizon_obstacles() const { return time_horizon_obstacles; }

	void set_radius(real_t p_radius);
	real_t get_radius() const { return radius; }

	void set_height(real_t p_height);
	real_t get_height() const { return height; }

	void set_max_speed(real_t p_max_speed);
	real_t get_max_speed() const { return max_speed; }

	void set_position(const Vector3 p_position);
	const Vector3 &get_position() const { return position; }

	void set_target_position(const Vector3 p_target_position);
	const Vector3 &get_target_position() const { return target_position; }

	void set_velocity(const Vector3 p_velocity);
	const Vector3 &get_velocity() const { return velocity; }

	void set_velocity_forced(const Vector3 p_velocity);
	const Vector3 &get_velocity_forced() const { return velocity_forced; }

	void set_avoidance_layers(uint32_t p_layers);
	uint32_t get_avoidance_layers() const { return avoidance_layers; }

	void set_avoidance_mask(uint32_t p_mask);
	uint32_t get_avoidance_mask() const { return avoidance_mask; }

	void set_avoidance_priority(real_t p_priority);
	real_t get_avoidance_priority() const { return avoidance_priority; }

	void set_paused(bool p_paused);
	bool get_paused() const;

	bool is_dirty() const;
	void sync();
	void request_sync();
	void cancel_sync_request();

	// Updates this agent with rvo data after the rvo simulation avoidance step.
	void update();

	// RVO debug data from the last frame update.
	const Dictionary get_avoidance_data() const;

private:
	void _update_rvo_agent_properties();
};

#endif // NAV_AGENT_H
