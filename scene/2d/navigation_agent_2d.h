/**************************************************************************/
/*  navigation_agent_2d.h                                                 */
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

#pragma once

#include "scene/main/node.h"
#include "servers/navigation/navigation_path_query_parameters_2d.h"
#include "servers/navigation/navigation_path_query_result_2d.h"

class Node2D;

class NavigationAgent2D : public Node {
	GDCLASS(NavigationAgent2D, Node);

	Node2D *agent_parent = nullptr;

	RID agent;
	RID map_override;

	bool avoidance_enabled = false;
	uint32_t avoidance_layers = 1;
	uint32_t avoidance_mask = 1;
	real_t avoidance_priority = 1.0;
	uint32_t navigation_layers = 1;
	NavigationPathQueryParameters2D::PathfindingAlgorithm pathfinding_algorithm = NavigationPathQueryParameters2D::PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR;
	NavigationPathQueryParameters2D::PathPostProcessing path_postprocessing = NavigationPathQueryParameters2D::PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL;
	BitField<NavigationPathQueryParameters2D::PathMetadataFlags> path_metadata_flags = NavigationPathQueryParameters2D::PathMetadataFlags::PATH_METADATA_INCLUDE_ALL;

	real_t path_desired_distance = 20.0;
	real_t target_desired_distance = 10.0;
	real_t radius = 10.0;
	real_t neighbor_distance = 500.0;
	int max_neighbors = 10;
	real_t time_horizon_agents = 1.0;
	real_t time_horizon_obstacles = 0.0;
	real_t max_speed = 100.0;
	real_t path_max_distance = 100.0;
	bool simplify_path = false;
	real_t simplify_epsilon = 0.0;

	Vector2 target_position;

	Ref<NavigationPathQueryParameters2D> navigation_query;
	Ref<NavigationPathQueryResult2D> navigation_result;
	int navigation_path_index = 0;

	// the velocity result of the avoidance simulation step
	Vector2 safe_velocity;

	/// The submitted target velocity, sets the "wanted" rvo agent velocity on the next update
	// this velocity is not guaranteed, the simulation will try to fulfill it if possible
	// if other agents or obstacles interfere it will be changed accordingly
	Vector2 velocity;
	bool velocity_submitted = false;

	/// The submitted forced velocity, overrides the rvo agent velocity on the next update
	// should only be used very intentionally and not every frame as it interferes with the simulation stability
	Vector2 velocity_forced;
	bool velocity_forced_submitted = false;

	bool target_position_submitted = false;

	bool target_reached = false;
	bool navigation_finished = true;
	bool last_waypoint_reached = false;

	// Debug properties for exposed bindings
	bool debug_enabled = false;
	float debug_path_custom_point_size = 4.0;
	float debug_path_custom_line_width = -1.0;
	bool debug_use_custom = false;
	Color debug_path_custom_color = Color(1.0, 1.0, 1.0, 1.0);

#ifdef DEBUG_ENABLED
	// Debug properties internal only
	bool debug_path_dirty = true;
	RID debug_path_instance;
#endif // DEBUG_ENABLED

protected:
	static void _bind_methods();
	void _notification(int p_what);

#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
#endif // DISABLE_DEPRECATED

public:
	NavigationAgent2D();
	virtual ~NavigationAgent2D();

	RID get_rid() const { return agent; }

	void set_avoidance_enabled(bool p_enabled);
	bool get_avoidance_enabled() const;

	void set_agent_parent(Node *p_agent_parent);

	void set_navigation_layers(uint32_t p_navigation_layers);
	uint32_t get_navigation_layers() const;

	void set_navigation_layer_value(int p_layer_number, bool p_value);
	bool get_navigation_layer_value(int p_layer_number) const;

	void set_pathfinding_algorithm(const NavigationPathQueryParameters2D::PathfindingAlgorithm p_pathfinding_algorithm);
	NavigationPathQueryParameters2D::PathfindingAlgorithm get_pathfinding_algorithm() const {
		return pathfinding_algorithm;
	}

	void set_path_postprocessing(const NavigationPathQueryParameters2D::PathPostProcessing p_path_postprocessing);
	NavigationPathQueryParameters2D::PathPostProcessing get_path_postprocessing() const {
		return path_postprocessing;
	}

	void set_path_metadata_flags(BitField<NavigationPathQueryParameters2D::PathMetadataFlags> p_flags);
	BitField<NavigationPathQueryParameters2D::PathMetadataFlags> get_path_metadata_flags() const {
		return path_metadata_flags;
	}

	void set_navigation_map(RID p_navigation_map);
	RID get_navigation_map() const;

	void set_path_desired_distance(real_t p_dd);
	real_t get_path_desired_distance() const { return path_desired_distance; }

	void set_target_desired_distance(real_t p_dd);
	real_t get_target_desired_distance() const { return target_desired_distance; }

	void set_radius(real_t p_radius);
	real_t get_radius() const { return radius; }

	void set_neighbor_distance(real_t p_distance);
	real_t get_neighbor_distance() const { return neighbor_distance; }

	void set_max_neighbors(int p_count);
	int get_max_neighbors() const { return max_neighbors; }

	void set_time_horizon_agents(real_t p_time_horizon);
	real_t get_time_horizon_agents() const { return time_horizon_agents; }

	void set_time_horizon_obstacles(real_t p_time_horizon);
	real_t get_time_horizon_obstacles() const { return time_horizon_obstacles; }

	void set_max_speed(real_t p_max_speed);
	real_t get_max_speed() const { return max_speed; }

	void set_path_max_distance(real_t p_pmd);
	real_t get_path_max_distance();

	void set_target_position(Vector2 p_position);
	Vector2 get_target_position() const;

	void set_simplify_path(bool p_enabled);
	bool get_simplify_path() const;

	void set_simplify_epsilon(real_t p_epsilon);
	real_t get_simplify_epsilon() const;

	Vector2 get_next_path_position();

	Ref<NavigationPathQueryResult2D> get_current_navigation_result() const { return navigation_result; }

	const Vector<Vector2> &get_current_navigation_path() const { return navigation_result->get_path(); }

	int get_current_navigation_path_index() const { return navigation_path_index; }

	real_t distance_to_target() const;
	bool is_target_reached() const;
	bool is_target_reachable();
	bool is_navigation_finished();
	Vector2 get_final_position();

	void set_velocity(const Vector2 p_velocity);
	Vector2 get_velocity() { return velocity; }

	void set_velocity_forced(const Vector2 p_velocity);

	void _avoidance_done(Vector3 p_new_velocity);

	PackedStringArray get_configuration_warnings() const override;

	void set_avoidance_layers(uint32_t p_layers);
	uint32_t get_avoidance_layers() const;

	void set_avoidance_mask(uint32_t p_mask);
	uint32_t get_avoidance_mask() const;

	void set_avoidance_layer_value(int p_layer_number, bool p_value);
	bool get_avoidance_layer_value(int p_layer_number) const;

	void set_avoidance_mask_value(int p_mask_number, bool p_value);
	bool get_avoidance_mask_value(int p_mask_number) const;

	void set_avoidance_priority(real_t p_priority);
	real_t get_avoidance_priority() const;

	void set_debug_enabled(bool p_enabled);
	bool get_debug_enabled() const;

	void set_debug_use_custom(bool p_enabled);
	bool get_debug_use_custom() const;

	void set_debug_path_custom_color(Color p_color);
	Color get_debug_path_custom_color() const;

	void set_debug_path_custom_point_size(float p_point_size);
	float get_debug_path_custom_point_size() const;

	void set_debug_path_custom_line_width(float p_line_width);
	float get_debug_path_custom_line_width() const;

private:
	bool _is_target_reachable() const;
	Vector2 _get_final_position() const;

	void _update_navigation();
	void _advance_waypoints(const Vector2 &p_origin);
	void _request_repath();

	bool _is_last_waypoint() const;
	void _move_to_next_waypoint();
	bool _is_within_waypoint_distance(const Vector2 &p_origin) const;
	bool _is_within_target_distance(const Vector2 &p_origin) const;

	void _trigger_waypoint_reached();
	void _transition_to_navigation_finished();
	void _transition_to_target_reached();

#ifdef DEBUG_ENABLED
	void _navigation_debug_changed();
	void _update_debug_path();
#endif // DEBUG_ENABLED
};
