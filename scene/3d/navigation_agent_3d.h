/**************************************************************************/
/*  navigation_agent_3d.h                                                 */
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
#include "servers/navigation/navigation_path_query_parameters_3d.h"
#include "servers/navigation/navigation_path_query_result_3d.h"

class Node3D;

class NavigationAgent3D : public Node {
	GDCLASS(NavigationAgent3D, Node);

	Node3D *agent_parent = nullptr;

	RID agent;
	RID map_override;

	bool avoidance_enabled = false;
	bool use_3d_avoidance = false;
	uint32_t avoidance_layers = 1;
	uint32_t avoidance_mask = 1;
	real_t avoidance_priority = 1.0;
	uint32_t navigation_layers = 1;
	NavigationPathQueryParameters3D::PathfindingAlgorithm pathfinding_algorithm = NavigationPathQueryParameters3D::PathfindingAlgorithm::PATHFINDING_ALGORITHM_ASTAR;
	NavigationPathQueryParameters3D::PathPostProcessing path_postprocessing = NavigationPathQueryParameters3D::PathPostProcessing::PATH_POSTPROCESSING_CORRIDORFUNNEL;
	BitField<NavigationPathQueryParameters3D::PathMetadataFlags> path_metadata_flags = NavigationPathQueryParameters3D::PathMetadataFlags::PATH_METADATA_INCLUDE_ALL;

	real_t path_desired_distance = 1.0;
	real_t target_desired_distance = 1.0;
	real_t height = 1.0;
	real_t radius = 0.5;
	real_t path_height_offset = 0.0;
	real_t neighbor_distance = 50.0;
	int max_neighbors = 10;
	real_t time_horizon_agents = 1.0;
	real_t time_horizon_obstacles = 0.0;
	real_t max_speed = 10.0;
	real_t path_max_distance = 5.0;
	bool simplify_path = false;
	real_t simplify_epsilon = 0.0;

	Vector3 target_position;

	Ref<NavigationPathQueryParameters3D> navigation_query;
	Ref<NavigationPathQueryResult3D> navigation_result;
	int navigation_path_index = 0;

	// the velocity result of the avoidance simulation step
	Vector3 safe_velocity;

	/// The submitted target velocity, sets the "wanted" rvo agent velocity on the next update
	// this velocity is not guaranteed, the simulation will try to fulfill it if possible
	// if other agents or obstacles interfere it will be changed accordingly
	Vector3 velocity;
	bool velocity_submitted = false;

	/// The submitted forced velocity, overrides the rvo agent velocity on the next update
	// should only be used very intentionally and not every frame as it interferes with the simulation stability
	Vector3 velocity_forced;
	bool velocity_forced_submitted = false;

	// 2D avoidance has no y-axis. This stores and reapplies the y-axis velocity to the agent before and after the avoidance step.
	// While not perfect it at least looks way better than agent's that clip through everything that is not a flat surface
	bool keep_y_velocity = true;
	float stored_y_velocity = 0.0;

	bool target_position_submitted = false;
	bool target_reached = false;
	bool navigation_finished = true;
	bool last_waypoint_reached = false;

	// Debug properties for exposed bindings
	bool debug_enabled = false;
	float debug_path_custom_point_size = 4.0;
	bool debug_use_custom = false;
	Color debug_path_custom_color = Color(1.0, 1.0, 1.0, 1.0);
#ifdef DEBUG_ENABLED
	// Debug properties internal only
	bool debug_path_dirty = true;
	RID debug_path_instance;
	Ref<ArrayMesh> debug_path_mesh;
	Ref<StandardMaterial3D> debug_agent_path_line_custom_material;
	Ref<StandardMaterial3D> debug_agent_path_point_custom_material;
#endif // DEBUG_ENABLED

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
#endif // DISABLE_DEPRECATED

public:
	NavigationAgent3D();
	virtual ~NavigationAgent3D();

	RID get_rid() const { return agent; }

	void set_avoidance_enabled(bool p_enabled);
	bool get_avoidance_enabled() const;

	void set_agent_parent(Node *p_agent_parent);

	void set_navigation_layers(uint32_t p_navigation_layers);
	uint32_t get_navigation_layers() const;

	void set_navigation_layer_value(int p_layer_number, bool p_value);
	bool get_navigation_layer_value(int p_layer_number) const;

	void set_pathfinding_algorithm(const NavigationPathQueryParameters3D::PathfindingAlgorithm p_pathfinding_algorithm);
	NavigationPathQueryParameters3D::PathfindingAlgorithm get_pathfinding_algorithm() const {
		return pathfinding_algorithm;
	}

	void set_path_postprocessing(const NavigationPathQueryParameters3D::PathPostProcessing p_path_postprocessing);
	NavigationPathQueryParameters3D::PathPostProcessing get_path_postprocessing() const {
		return path_postprocessing;
	}

	void set_path_metadata_flags(BitField<NavigationPathQueryParameters3D::PathMetadataFlags> p_flags);
	BitField<NavigationPathQueryParameters3D::PathMetadataFlags> get_path_metadata_flags() const {
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

	void set_height(real_t p_height);
	real_t get_height() const { return height; }

	void set_path_height_offset(real_t p_path_height_offset);
	real_t get_path_height_offset() const { return path_height_offset; }

	void set_use_3d_avoidance(bool p_use_3d_avoidance);
	bool get_use_3d_avoidance() const { return use_3d_avoidance; }

	void set_keep_y_velocity(bool p_enabled);
	bool get_keep_y_velocity() const;

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

	void set_target_position(Vector3 p_position);
	Vector3 get_target_position() const;

	void set_simplify_path(bool p_enabled);
	bool get_simplify_path() const;

	void set_simplify_epsilon(real_t p_epsilon);
	real_t get_simplify_epsilon() const;

	Vector3 get_next_path_position();

	Ref<NavigationPathQueryResult3D> get_current_navigation_result() const { return navigation_result; }

	const Vector<Vector3> &get_current_navigation_path() const { return navigation_result->get_path(); }

	int get_current_navigation_path_index() const { return navigation_path_index; }

	real_t distance_to_target() const;
	bool is_target_reached() const;
	bool is_target_reachable();
	bool is_navigation_finished();
	Vector3 get_final_position();

	void set_velocity(const Vector3 p_velocity);
	Vector3 get_velocity() { return velocity; }

	void set_velocity_forced(const Vector3 p_velocity);

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

private:
	bool _is_target_reachable() const;
	Vector3 _get_final_position() const;

	void _update_navigation();
	void _advance_waypoints(const Vector3 &p_origin);
	void _request_repath();

	bool _is_last_waypoint() const;
	void _move_to_next_waypoint();
	bool _is_within_waypoint_distance(const Vector3 &p_origin) const;
	bool _is_within_target_distance(const Vector3 &p_origin) const;

	void _trigger_waypoint_reached();
	void _transition_to_navigation_finished();
	void _transition_to_target_reached();

#ifdef DEBUG_ENABLED
	void _navigation_debug_changed();
	void _update_debug_path();
#endif // DEBUG_ENABLED
};
