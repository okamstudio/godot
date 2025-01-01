/**************************************************************************/
/*  navigation_agent_3d.cpp                                               */
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

#include "navigation_agent_3d.h"

#include "scene/3d/navigation_link_3d.h"
#include "servers/navigation_server_3d.h"

void NavigationAgent3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rid"), &NavigationAgent3D::get_rid);

	ClassDB::bind_method(D_METHOD("set_avoidance_enabled", "enabled"), &NavigationAgent3D::set_avoidance_enabled);
	ClassDB::bind_method(D_METHOD("get_avoidance_enabled"), &NavigationAgent3D::get_avoidance_enabled);

	ClassDB::bind_method(D_METHOD("set_path_desired_distance", "desired_distance"), &NavigationAgent3D::set_path_desired_distance);
	ClassDB::bind_method(D_METHOD("get_path_desired_distance"), &NavigationAgent3D::get_path_desired_distance);

	ClassDB::bind_method(D_METHOD("set_target_desired_distance", "desired_distance"), &NavigationAgent3D::set_target_desired_distance);
	ClassDB::bind_method(D_METHOD("get_target_desired_distance"), &NavigationAgent3D::get_target_desired_distance);

	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &NavigationAgent3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &NavigationAgent3D::get_radius);

	ClassDB::bind_method(D_METHOD("set_height", "height"), &NavigationAgent3D::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &NavigationAgent3D::get_height);

	ClassDB::bind_method(D_METHOD("set_path_height_offset", "path_height_offset"), &NavigationAgent3D::set_path_height_offset);
	ClassDB::bind_method(D_METHOD("get_path_height_offset"), &NavigationAgent3D::get_path_height_offset);

	ClassDB::bind_method(D_METHOD("set_use_3d_avoidance", "enabled"), &NavigationAgent3D::set_use_3d_avoidance);
	ClassDB::bind_method(D_METHOD("get_use_3d_avoidance"), &NavigationAgent3D::get_use_3d_avoidance);

	ClassDB::bind_method(D_METHOD("set_keep_y_velocity", "enabled"), &NavigationAgent3D::set_keep_y_velocity);
	ClassDB::bind_method(D_METHOD("get_keep_y_velocity"), &NavigationAgent3D::get_keep_y_velocity);

	ClassDB::bind_method(D_METHOD("set_neighbor_distance", "neighbor_distance"), &NavigationAgent3D::set_neighbor_distance);
	ClassDB::bind_method(D_METHOD("get_neighbor_distance"), &NavigationAgent3D::get_neighbor_distance);

	ClassDB::bind_method(D_METHOD("set_max_neighbors", "max_neighbors"), &NavigationAgent3D::set_max_neighbors);
	ClassDB::bind_method(D_METHOD("get_max_neighbors"), &NavigationAgent3D::get_max_neighbors);

	ClassDB::bind_method(D_METHOD("set_time_horizon_agents", "time_horizon"), &NavigationAgent3D::set_time_horizon_agents);
	ClassDB::bind_method(D_METHOD("get_time_horizon_agents"), &NavigationAgent3D::get_time_horizon_agents);

	ClassDB::bind_method(D_METHOD("set_time_horizon_obstacles", "time_horizon"), &NavigationAgent3D::set_time_horizon_obstacles);
	ClassDB::bind_method(D_METHOD("get_time_horizon_obstacles"), &NavigationAgent3D::get_time_horizon_obstacles);

	ClassDB::bind_method(D_METHOD("set_max_speed", "max_speed"), &NavigationAgent3D::set_max_speed);
	ClassDB::bind_method(D_METHOD("get_max_speed"), &NavigationAgent3D::get_max_speed);

	ClassDB::bind_method(D_METHOD("set_path_max_distance", "max_speed"), &NavigationAgent3D::set_path_max_distance);
	ClassDB::bind_method(D_METHOD("get_path_max_distance"), &NavigationAgent3D::get_path_max_distance);

	ClassDB::bind_method(D_METHOD("set_navigation_layers", "navigation_layers"), &NavigationAgent3D::set_navigation_layers);
	ClassDB::bind_method(D_METHOD("get_navigation_layers"), &NavigationAgent3D::get_navigation_layers);

	ClassDB::bind_method(D_METHOD("set_navigation_layer_value", "layer_number", "value"), &NavigationAgent3D::set_navigation_layer_value);
	ClassDB::bind_method(D_METHOD("get_navigation_layer_value", "layer_number"), &NavigationAgent3D::get_navigation_layer_value);

	ClassDB::bind_method(D_METHOD("set_pathfinding_algorithm", "pathfinding_algorithm"), &NavigationAgent3D::set_pathfinding_algorithm);
	ClassDB::bind_method(D_METHOD("get_pathfinding_algorithm"), &NavigationAgent3D::get_pathfinding_algorithm);

	ClassDB::bind_method(D_METHOD("set_path_postprocessing", "path_postprocessing"), &NavigationAgent3D::set_path_postprocessing);
	ClassDB::bind_method(D_METHOD("get_path_postprocessing"), &NavigationAgent3D::get_path_postprocessing);

	ClassDB::bind_method(D_METHOD("set_path_metadata_flags", "flags"), &NavigationAgent3D::set_path_metadata_flags);
	ClassDB::bind_method(D_METHOD("get_path_metadata_flags"), &NavigationAgent3D::get_path_metadata_flags);

	ClassDB::bind_method(D_METHOD("set_navigation_map", "navigation_map"), &NavigationAgent3D::set_navigation_map);
	ClassDB::bind_method(D_METHOD("get_navigation_map"), &NavigationAgent3D::get_navigation_map);

	ClassDB::bind_method(D_METHOD("set_target_position", "position"), &NavigationAgent3D::set_target_position);
	ClassDB::bind_method(D_METHOD("get_target_position"), &NavigationAgent3D::get_target_position);

	ClassDB::bind_method(D_METHOD("set_simplify_path", "enabled"), &NavigationAgent3D::set_simplify_path);
	ClassDB::bind_method(D_METHOD("get_simplify_path"), &NavigationAgent3D::get_simplify_path);

	ClassDB::bind_method(D_METHOD("set_simplify_epsilon", "epsilon"), &NavigationAgent3D::set_simplify_epsilon);
	ClassDB::bind_method(D_METHOD("get_simplify_epsilon"), &NavigationAgent3D::get_simplify_epsilon);

	ClassDB::bind_method(D_METHOD("get_next_path_position"), &NavigationAgent3D::get_next_path_position);

	ClassDB::bind_method(D_METHOD("set_velocity_forced", "velocity"), &NavigationAgent3D::set_velocity_forced);

	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &NavigationAgent3D::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &NavigationAgent3D::get_velocity);

	ClassDB::bind_method(D_METHOD("distance_to_target"), &NavigationAgent3D::distance_to_target);

	ClassDB::bind_method(D_METHOD("get_current_navigation_result"), &NavigationAgent3D::get_current_navigation_result);

	ClassDB::bind_method(D_METHOD("get_current_navigation_path"), &NavigationAgent3D::get_current_navigation_path);
	ClassDB::bind_method(D_METHOD("get_current_navigation_path_index"), &NavigationAgent3D::get_current_navigation_path_index);
	ClassDB::bind_method(D_METHOD("is_target_reached"), &NavigationAgent3D::is_target_reached);
	ClassDB::bind_method(D_METHOD("is_target_reachable"), &NavigationAgent3D::is_target_reachable);
	ClassDB::bind_method(D_METHOD("is_navigation_finished"), &NavigationAgent3D::is_navigation_finished);
	ClassDB::bind_method(D_METHOD("get_final_position"), &NavigationAgent3D::get_final_position);

	ClassDB::bind_method(D_METHOD("_avoidance_done", "new_velocity"), &NavigationAgent3D::_avoidance_done);

	ClassDB::bind_method(D_METHOD("set_avoidance_layers", "layers"), &NavigationAgent3D::set_avoidance_layers);
	ClassDB::bind_method(D_METHOD("get_avoidance_layers"), &NavigationAgent3D::get_avoidance_layers);
	ClassDB::bind_method(D_METHOD("set_avoidance_mask", "mask"), &NavigationAgent3D::set_avoidance_mask);
	ClassDB::bind_method(D_METHOD("get_avoidance_mask"), &NavigationAgent3D::get_avoidance_mask);
	ClassDB::bind_method(D_METHOD("set_avoidance_layer_value", "layer_number", "value"), &NavigationAgent3D::set_avoidance_layer_value);
	ClassDB::bind_method(D_METHOD("get_avoidance_layer_value", "layer_number"), &NavigationAgent3D::get_avoidance_layer_value);
	ClassDB::bind_method(D_METHOD("set_avoidance_mask_value", "mask_number", "value"), &NavigationAgent3D::set_avoidance_mask_value);
	ClassDB::bind_method(D_METHOD("get_avoidance_mask_value", "mask_number"), &NavigationAgent3D::get_avoidance_mask_value);
	ClassDB::bind_method(D_METHOD("set_avoidance_priority", "priority"), &NavigationAgent3D::set_avoidance_priority);
	ClassDB::bind_method(D_METHOD("get_avoidance_priority"), &NavigationAgent3D::get_avoidance_priority);

	ADD_GROUP("Pathfinding", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "target_position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_target_position", "get_target_position");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_desired_distance", PROPERTY_HINT_RANGE, "0.1,100,0.01,or_greater,suffix:m"), "set_path_desired_distance", "get_path_desired_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "target_desired_distance", PROPERTY_HINT_RANGE, "0.1,100,0.01,or_greater,suffix:m"), "set_target_desired_distance", "get_target_desired_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_height_offset", PROPERTY_HINT_RANGE, "-100.0,100,0.01,or_greater,suffix:m"), "set_path_height_offset", "get_path_height_offset");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "path_max_distance", PROPERTY_HINT_RANGE, "0.01,100,0.1,or_greater,suffix:m"), "set_path_max_distance", "get_path_max_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "navigation_layers", PROPERTY_HINT_LAYERS_3D_NAVIGATION), "set_navigation_layers", "get_navigation_layers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pathfinding_algorithm", PROPERTY_HINT_ENUM, "AStar"), "set_pathfinding_algorithm", "get_pathfinding_algorithm");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "path_postprocessing", PROPERTY_HINT_ENUM, "Corridorfunnel,Edgecentered,None"), "set_path_postprocessing", "get_path_postprocessing");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "path_metadata_flags", PROPERTY_HINT_FLAGS, "Include Types,Include RIDs,Include Owners"), "set_path_metadata_flags", "get_path_metadata_flags");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simplify_path"), "set_simplify_path", "get_simplify_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "simplify_epsilon", PROPERTY_HINT_RANGE, "0.0,10.0,0.001,or_greater,suffix:m"), "set_simplify_epsilon", "get_simplify_epsilon");

	ADD_GROUP("Avoidance", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "avoidance_enabled"), "set_avoidance_enabled", "get_avoidance_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.01,100,0.01,or_greater,suffix:m"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.01,100,0.01,or_greater,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "neighbor_distance", PROPERTY_HINT_RANGE, "0.1,10000,0.01,or_greater,suffix:m"), "set_neighbor_distance", "get_neighbor_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_neighbors", PROPERTY_HINT_RANGE, "1,10000,1,or_greater"), "set_max_neighbors", "get_max_neighbors");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_horizon_agents", PROPERTY_HINT_RANGE, "0.0,10,0.01,or_greater,suffix:s"), "set_time_horizon_agents", "get_time_horizon_agents");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_horizon_obstacles", PROPERTY_HINT_RANGE, "0.0,10,0.01,or_greater,suffix:s"), "set_time_horizon_obstacles", "get_time_horizon_obstacles");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed", PROPERTY_HINT_RANGE, "0.01,10000,0.01,or_greater,suffix:m/s"), "set_max_speed", "get_max_speed");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_3d_avoidance"), "set_use_3d_avoidance", "get_use_3d_avoidance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "keep_y_velocity"), "set_keep_y_velocity", "get_keep_y_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "avoidance_layers", PROPERTY_HINT_LAYERS_AVOIDANCE), "set_avoidance_layers", "get_avoidance_layers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "avoidance_mask", PROPERTY_HINT_LAYERS_AVOIDANCE), "set_avoidance_mask", "get_avoidance_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "avoidance_priority", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_avoidance_priority", "get_avoidance_priority");

	ADD_SIGNAL(MethodInfo("path_changed"));
	ADD_SIGNAL(MethodInfo("target_reached"));
	ADD_SIGNAL(MethodInfo("waypoint_reached", PropertyInfo(Variant::DICTIONARY, "details")));
	ADD_SIGNAL(MethodInfo("link_reached", PropertyInfo(Variant::DICTIONARY, "details")));
	ADD_SIGNAL(MethodInfo("navigation_finished"));
	ADD_SIGNAL(MethodInfo("velocity_computed", PropertyInfo(Variant::VECTOR3, "safe_velocity")));

	ClassDB::bind_method(D_METHOD("set_debug_enabled", "enabled"), &NavigationAgent3D::set_debug_enabled);
	ClassDB::bind_method(D_METHOD("get_debug_enabled"), &NavigationAgent3D::get_debug_enabled);
	ClassDB::bind_method(D_METHOD("set_debug_use_custom", "enabled"), &NavigationAgent3D::set_debug_use_custom);
	ClassDB::bind_method(D_METHOD("get_debug_use_custom"), &NavigationAgent3D::get_debug_use_custom);
	ClassDB::bind_method(D_METHOD("set_debug_path_custom_color", "color"), &NavigationAgent3D::set_debug_path_custom_color);
	ClassDB::bind_method(D_METHOD("get_debug_path_custom_color"), &NavigationAgent3D::get_debug_path_custom_color);
	ClassDB::bind_method(D_METHOD("set_debug_path_custom_point_size", "point_size"), &NavigationAgent3D::set_debug_path_custom_point_size);
	ClassDB::bind_method(D_METHOD("get_debug_path_custom_point_size"), &NavigationAgent3D::get_debug_path_custom_point_size);

	ADD_GROUP("Debug", "debug_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_enabled"), "set_debug_enabled", "get_debug_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_use_custom"), "set_debug_use_custom", "get_debug_use_custom");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_path_custom_color"), "set_debug_path_custom_color", "get_debug_path_custom_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "debug_path_custom_point_size", PROPERTY_HINT_RANGE, "0,50,0.01,or_greater,suffix:px"), "set_debug_path_custom_point_size", "get_debug_path_custom_point_size");
}

#ifndef DISABLE_DEPRECATED
// Compatibility with Godot 4.0 beta 10 or below.
// Functions in block below all renamed or replaced in 4.0 beta 1X avoidance rework.
bool NavigationAgent3D::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "time_horizon") {
		set_time_horizon_agents(p_value);
		return true;
	}
	if (p_name == "target_location") {
		set_target_position(p_value);
		return true;
	}
	if (p_name == "agent_height_offset") {
		set_path_height_offset(p_value);
		return true;
	}
	return false;
}

bool NavigationAgent3D::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "time_horizon") {
		r_ret = get_time_horizon_agents();
		return true;
	}
	if (p_name == "target_location") {
		r_ret = get_target_position();
		return true;
	}
	if (p_name == "agent_height_offset") {
		r_ret = get_path_height_offset();
		return true;
	}
	return false;
}
#endif // DISABLE_DEPRECATED

void NavigationAgent3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			// need to use POST_ENTER_TREE cause with normal ENTER_TREE not all required Nodes are ready.
			// cannot use READY as ready does not get called if Node is re-added to SceneTree
			set_agent_parent(get_parent());
			set_physics_process_internal(true);

			if (agent_parent && avoidance_enabled) {
				NavigationServer3D::get_singleton()->agent_set_position(agent, agent_parent->get_global_transform().origin);
			}

#ifdef DEBUG_ENABLED
			if (NavigationServer3D::get_singleton()->get_debug_enabled()) {
				debug_path_dirty = true;
			}
#endif // DEBUG_ENABLED

		} break;

		case NOTIFICATION_PARENTED: {
			if (is_inside_tree() && (get_parent() != agent_parent)) {
				// only react to PARENTED notifications when already inside_tree and parent changed, e.g. users switch nodes around
				// PARENTED notification fires also when Node is added in scripts to a parent
				// this would spam transforms fails and world fails while Node is outside SceneTree
				// when node gets reparented when joining the tree POST_ENTER_TREE takes care of this
				set_agent_parent(get_parent());
				set_physics_process_internal(true);
			}
		} break;

		case NOTIFICATION_UNPARENTED: {
			// if agent has no parent no point in processing it until reparented
			set_agent_parent(nullptr);
			set_physics_process_internal(false);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			set_agent_parent(nullptr);
			set_physics_process_internal(false);

#ifdef DEBUG_ENABLED
			if (debug_path_instance.is_valid()) {
				RS::get_singleton()->instance_set_visible(debug_path_instance, false);
			}
#endif // DEBUG_ENABLED
		} break;

		case NOTIFICATION_SUSPENDED:
		case NOTIFICATION_PAUSED: {
			if (agent_parent) {
				NavigationServer3D::get_singleton()->agent_set_paused(get_rid(), !agent_parent->can_process());
			}
		} break;

		case NOTIFICATION_UNSUSPENDED: {
			if (get_tree()->is_paused()) {
				break;
			}
			[[fallthrough]];
		}

		case NOTIFICATION_UNPAUSED: {
			if (agent_parent) {
				NavigationServer3D::get_singleton()->agent_set_paused(get_rid(), !agent_parent->can_process());
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (agent_parent && avoidance_enabled) {
				NavigationServer3D::get_singleton()->agent_set_position(agent, agent_parent->get_global_position());
			}
			if (agent_parent && target_position_submitted) {
				if (velocity_submitted) {
					velocity_submitted = false;
					if (avoidance_enabled) {
						if (!use_3d_avoidance) {
							if (keep_y_velocity) {
								stored_y_velocity = velocity.y;
							}
							velocity.y = 0.0;
						}
						NavigationServer3D::get_singleton()->agent_set_velocity(agent, velocity);
					}
				}
				if (velocity_forced_submitted) {
					velocity_forced_submitted = false;
					if (avoidance_enabled) {
						NavigationServer3D::get_singleton()->agent_set_velocity_forced(agent, velocity_forced);
					}
				}
			}
#ifdef DEBUG_ENABLED
			if (debug_path_dirty) {
				_update_debug_path();
			}
#endif // DEBUG_ENABLED
		} break;
	}
}

void NavigationAgent3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "keep_y_velocity" && use_3d_avoidance) {
		p_property.usage = PROPERTY_USAGE_NONE;
		return;
	}
}

NavigationAgent3D::NavigationAgent3D() {
	agent = NavigationServer3D::get_singleton()->agent_create();

	NavigationServer3D::get_singleton()->agent_set_neighbor_distance(agent, neighbor_distance);
	NavigationServer3D::get_singleton()->agent_set_max_neighbors(agent, max_neighbors);
	NavigationServer3D::get_singleton()->agent_set_time_horizon_agents(agent, time_horizon_agents);
	NavigationServer3D::get_singleton()->agent_set_time_horizon_obstacles(agent, time_horizon_obstacles);
	NavigationServer3D::get_singleton()->agent_set_radius(agent, radius);
	NavigationServer3D::get_singleton()->agent_set_height(agent, height);
	NavigationServer3D::get_singleton()->agent_set_max_speed(agent, max_speed);
	NavigationServer3D::get_singleton()->agent_set_avoidance_layers(agent, avoidance_layers);
	NavigationServer3D::get_singleton()->agent_set_avoidance_mask(agent, avoidance_mask);
	NavigationServer3D::get_singleton()->agent_set_avoidance_priority(agent, avoidance_priority);
	NavigationServer3D::get_singleton()->agent_set_use_3d_avoidance(agent, use_3d_avoidance);
	NavigationServer3D::get_singleton()->agent_set_avoidance_enabled(agent, avoidance_enabled);
	if (avoidance_enabled) {
		NavigationServer3D::get_singleton()->agent_set_avoidance_callback(agent, callable_mp(this, &NavigationAgent3D::_avoidance_done));
	}

	// Preallocate query and result objects to improve performance.
	navigation_query = Ref<NavigationPathQueryParameters3D>();
	navigation_query.instantiate();

	navigation_result = Ref<NavigationPathQueryResult3D>();
	navigation_result.instantiate();

#ifdef DEBUG_ENABLED
	NavigationServer3D::get_singleton()->connect(SNAME("navigation_debug_changed"), callable_mp(this, &NavigationAgent3D::_navigation_debug_changed));
#endif // DEBUG_ENABLED
}

NavigationAgent3D::~NavigationAgent3D() {
	ERR_FAIL_NULL(NavigationServer3D::get_singleton());
	NavigationServer3D::get_singleton()->free(agent);
	agent = RID(); // Pointless

#ifdef DEBUG_ENABLED
	NavigationServer3D::get_singleton()->disconnect(SNAME("navigation_debug_changed"), callable_mp(this, &NavigationAgent3D::_navigation_debug_changed));

	ERR_FAIL_NULL(RenderingServer::get_singleton());
	if (debug_path_instance.is_valid()) {
		RenderingServer::get_singleton()->free(debug_path_instance);
	}
	if (debug_path_mesh.is_valid()) {
		RenderingServer::get_singleton()->free(debug_path_mesh->get_rid());
	}
#endif // DEBUG_ENABLED
}

void NavigationAgent3D::set_avoidance_enabled(bool p_enabled) {
	if (avoidance_enabled == p_enabled) {
		return;
	}

	avoidance_enabled = p_enabled;

	if (avoidance_enabled) {
		NavigationServer3D::get_singleton()->agent_set_avoidance_enabled(agent, true);
		NavigationServer3D::get_singleton()->agent_set_avoidance_callback(agent, callable_mp(this, &NavigationAgent3D::_avoidance_done));
	} else {
		NavigationServer3D::get_singleton()->agent_set_avoidance_enabled(agent, false);
		NavigationServer3D::get_singleton()->agent_set_avoidance_callback(agent, Callable());
	}
}

bool NavigationAgent3D::get_avoidance_enabled() const {
	return avoidance_enabled;
}

void NavigationAgent3D::set_agent_parent(Node *p_agent_parent) {
	if (agent_parent == p_agent_parent) {
		return;
	}

	// remove agent from any avoidance map before changing parent or there will be leftovers on the RVO map
	NavigationServer3D::get_singleton()->agent_set_avoidance_callback(agent, Callable());

	if (Object::cast_to<Node3D>(p_agent_parent) != nullptr) {
		// place agent on navigation map first or else the RVO agent callback creation fails silently later
		agent_parent = Object::cast_to<Node3D>(p_agent_parent);
		if (map_override.is_valid()) {
			NavigationServer3D::get_singleton()->agent_set_map(get_rid(), map_override);
		} else {
			NavigationServer3D::get_singleton()->agent_set_map(get_rid(), agent_parent->get_world_3d()->get_navigation_map());
		}

		// create new avoidance callback if enabled
		if (avoidance_enabled) {
			NavigationServer3D::get_singleton()->agent_set_avoidance_callback(agent, callable_mp(this, &NavigationAgent3D::_avoidance_done));
		}
	} else {
		agent_parent = nullptr;
		NavigationServer3D::get_singleton()->agent_set_map(get_rid(), RID());
	}
}

void NavigationAgent3D::set_navigation_layers(uint32_t p_navigation_layers) {
	if (navigation_layers == p_navigation_layers) {
		return;
	}

	navigation_layers = p_navigation_layers;

	_request_repath();
}

uint32_t NavigationAgent3D::get_navigation_layers() const {
	return navigation_layers;
}

void NavigationAgent3D::set_navigation_layer_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Navigation layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Navigation layer number must be between 1 and 32 inclusive.");
	uint32_t _navigation_layers = get_navigation_layers();
	if (p_value) {
		_navigation_layers |= 1 << (p_layer_number - 1);
	} else {
		_navigation_layers &= ~(1 << (p_layer_number - 1));
	}
	set_navigation_layers(_navigation_layers);
}

bool NavigationAgent3D::get_navigation_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Navigation layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Navigation layer number must be between 1 and 32 inclusive.");
	return get_navigation_layers() & (1 << (p_layer_number - 1));
}

void NavigationAgent3D::set_pathfinding_algorithm(const NavigationPathQueryParameters3D::PathfindingAlgorithm p_pathfinding_algorithm) {
	if (pathfinding_algorithm == p_pathfinding_algorithm) {
		return;
	}

	pathfinding_algorithm = p_pathfinding_algorithm;

	navigation_query->set_pathfinding_algorithm(pathfinding_algorithm);
}

void NavigationAgent3D::set_path_postprocessing(const NavigationPathQueryParameters3D::PathPostProcessing p_path_postprocessing) {
	if (path_postprocessing == p_path_postprocessing) {
		return;
	}

	path_postprocessing = p_path_postprocessing;

	navigation_query->set_path_postprocessing(path_postprocessing);
}

void NavigationAgent3D::set_simplify_path(bool p_enabled) {
	simplify_path = p_enabled;
	navigation_query->set_simplify_path(simplify_path);
}

bool NavigationAgent3D::get_simplify_path() const {
	return simplify_path;
}

void NavigationAgent3D::set_simplify_epsilon(real_t p_epsilon) {
	simplify_epsilon = MAX(0.0, p_epsilon);
	navigation_query->set_simplify_epsilon(simplify_epsilon);
}

real_t NavigationAgent3D::get_simplify_epsilon() const {
	return simplify_epsilon;
}

void NavigationAgent3D::set_path_metadata_flags(BitField<NavigationPathQueryParameters3D::PathMetadataFlags> p_path_metadata_flags) {
	if (path_metadata_flags == p_path_metadata_flags) {
		return;
	}

	path_metadata_flags = p_path_metadata_flags;
}

void NavigationAgent3D::set_navigation_map(RID p_navigation_map) {
	if (map_override == p_navigation_map) {
		return;
	}

	map_override = p_navigation_map;

	NavigationServer3D::get_singleton()->agent_set_map(agent, map_override);
	_request_repath();
}

RID NavigationAgent3D::get_navigation_map() const {
	if (map_override.is_valid()) {
		return map_override;
	} else if (agent_parent != nullptr) {
		return agent_parent->get_world_3d()->get_navigation_map();
	}
	return RID();
}

void NavigationAgent3D::set_path_desired_distance(real_t p_path_desired_distance) {
	if (Math::is_equal_approx(path_desired_distance, p_path_desired_distance)) {
		return;
	}

	path_desired_distance = p_path_desired_distance;
}

void NavigationAgent3D::set_target_desired_distance(real_t p_target_desired_distance) {
	if (Math::is_equal_approx(target_desired_distance, p_target_desired_distance)) {
		return;
	}

	target_desired_distance = p_target_desired_distance;
}

void NavigationAgent3D::set_radius(real_t p_radius) {
	ERR_FAIL_COND_MSG(p_radius < 0.0, "Radius must be positive.");
	if (Math::is_equal_approx(radius, p_radius)) {
		return;
	}
	radius = p_radius;

	NavigationServer3D::get_singleton()->agent_set_radius(agent, radius);
}

void NavigationAgent3D::set_height(real_t p_height) {
	ERR_FAIL_COND_MSG(p_height < 0.0, "Height must be positive.");
	if (Math::is_equal_approx(height, p_height)) {
		return;
	}
	height = p_height;
	NavigationServer3D::get_singleton()->agent_set_height(agent, height);
}

void NavigationAgent3D::set_path_height_offset(real_t p_path_height_offset) {
	path_height_offset = p_path_height_offset;
}

void NavigationAgent3D::set_use_3d_avoidance(bool p_use_3d_avoidance) {
	use_3d_avoidance = p_use_3d_avoidance;
	NavigationServer3D::get_singleton()->agent_set_use_3d_avoidance(agent, use_3d_avoidance);
	notify_property_list_changed();
}

void NavigationAgent3D::set_keep_y_velocity(bool p_enabled) {
	keep_y_velocity = p_enabled;
	stored_y_velocity = 0.0;
}

bool NavigationAgent3D::get_keep_y_velocity() const {
	return keep_y_velocity;
}

void NavigationAgent3D::set_neighbor_distance(real_t p_distance) {
	if (Math::is_equal_approx(neighbor_distance, p_distance)) {
		return;
	}

	neighbor_distance = p_distance;

	NavigationServer3D::get_singleton()->agent_set_neighbor_distance(agent, neighbor_distance);
}

void NavigationAgent3D::set_max_neighbors(int p_count) {
	if (max_neighbors == p_count) {
		return;
	}

	max_neighbors = p_count;

	NavigationServer3D::get_singleton()->agent_set_max_neighbors(agent, max_neighbors);
}

void NavigationAgent3D::set_time_horizon_agents(real_t p_time_horizon) {
	ERR_FAIL_COND_MSG(p_time_horizon < 0.0, "Time horizon must be positive.");
	if (Math::is_equal_approx(time_horizon_agents, p_time_horizon)) {
		return;
	}
	time_horizon_agents = p_time_horizon;
	NavigationServer3D::get_singleton()->agent_set_time_horizon_agents(agent, time_horizon_agents);
}

void NavigationAgent3D::set_time_horizon_obstacles(real_t p_time_horizon) {
	ERR_FAIL_COND_MSG(p_time_horizon < 0.0, "Time horizon must be positive.");
	if (Math::is_equal_approx(time_horizon_obstacles, p_time_horizon)) {
		return;
	}
	time_horizon_obstacles = p_time_horizon;
	NavigationServer3D::get_singleton()->agent_set_time_horizon_obstacles(agent, time_horizon_obstacles);
}

void NavigationAgent3D::set_max_speed(real_t p_max_speed) {
	ERR_FAIL_COND_MSG(p_max_speed < 0.0, "Max speed must be positive.");
	if (Math::is_equal_approx(max_speed, p_max_speed)) {
		return;
	}
	max_speed = p_max_speed;

	NavigationServer3D::get_singleton()->agent_set_max_speed(agent, max_speed);
}

void NavigationAgent3D::set_path_max_distance(real_t p_path_max_distance) {
	if (Math::is_equal_approx(path_max_distance, p_path_max_distance)) {
		return;
	}

	path_max_distance = p_path_max_distance;
}

real_t NavigationAgent3D::get_path_max_distance() {
	return path_max_distance;
}

void NavigationAgent3D::set_target_position(Vector3 p_position) {
	// Intentionally not checking for equality of the parameter, as we want to update the path even if the target position is the same in case the world changed.
	// Revisit later when the navigation server can update the path without requesting a new path.

	target_position = p_position;
	target_position_submitted = true;

	_request_repath();
}

Vector3 NavigationAgent3D::get_target_position() const {
	return target_position;
}

Vector3 NavigationAgent3D::get_next_path_position() {
	_update_navigation();

	const Vector<Vector3> &navigation_path = navigation_result->get_path();
	if (navigation_path.size() == 0) {
		ERR_FAIL_NULL_V_MSG(agent_parent, Vector3(), "The agent has no parent.");
		return agent_parent->get_global_position();
	} else {
		return navigation_path[navigation_path_index] - Vector3(0, path_height_offset, 0);
	}
}

real_t NavigationAgent3D::distance_to_target() const {
	ERR_FAIL_NULL_V_MSG(agent_parent, 0.0, "The agent has no parent.");
	return agent_parent->get_global_position().distance_to(target_position);
}

bool NavigationAgent3D::is_target_reached() const {
	return target_reached;
}

bool NavigationAgent3D::is_target_reachable() {
	_update_navigation();
	return _is_target_reachable();
}

bool NavigationAgent3D::_is_target_reachable() const {
	return target_desired_distance >= _get_final_position().distance_to(target_position);
}

bool NavigationAgent3D::is_navigation_finished() {
	_update_navigation();
	return navigation_finished;
}

Vector3 NavigationAgent3D::get_final_position() {
	_update_navigation();
	return _get_final_position();
}

Vector3 NavigationAgent3D::_get_final_position() const {
	const Vector<Vector3> &navigation_path = navigation_result->get_path();
	if (navigation_path.size() == 0) {
		return Vector3();
	}
	return navigation_path[navigation_path.size() - 1] - Vector3(0, path_height_offset, 0);
}

void NavigationAgent3D::set_velocity_forced(Vector3 p_velocity) {
	// Intentionally not checking for equality of the parameter.
	// We need to always submit the velocity to the navigation server, even when it is the same, in order to run avoidance every frame.
	// Revisit later when the navigation server can update avoidance without users resubmitting the velocity.

	velocity_forced = p_velocity;
	velocity_forced_submitted = true;
}

void NavigationAgent3D::set_velocity(const Vector3 p_velocity) {
	velocity = p_velocity;
	velocity_submitted = true;
}

void NavigationAgent3D::_avoidance_done(Vector3 p_new_velocity) {
	safe_velocity = p_new_velocity;
	if (!use_3d_avoidance) {
		safe_velocity.y = stored_y_velocity;
	}
	emit_signal(SNAME("velocity_computed"), safe_velocity);
}

PackedStringArray NavigationAgent3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (!Object::cast_to<Node3D>(get_parent())) {
		warnings.push_back(RTR("The NavigationAgent3D can be used only under a Node3D inheriting parent node."));
	}

	return warnings;
}

void NavigationAgent3D::_update_navigation() {
	if (agent_parent == nullptr) {
		return;
	}
	if (!agent_parent->is_inside_tree()) {
		return;
	}
	if (!target_position_submitted) {
		return;
	}

	Vector3 origin = agent_parent->get_global_position();

	bool reload_path = false;

	if (NavigationServer3D::get_singleton()->agent_is_map_changed(agent)) {
		reload_path = true;
	} else if (navigation_result->get_path().size() == 0) {
		reload_path = true;
	} else {
		// Check if too far from the navigation path
		if (navigation_path_index > 0) {
			const Vector<Vector3> &navigation_path = navigation_result->get_path();

			Vector3 segment[2];
			segment[0] = navigation_path[navigation_path_index - 1];
			segment[1] = navigation_path[navigation_path_index];
			segment[0].y -= path_height_offset;
			segment[1].y -= path_height_offset;
			Vector3 p = Geometry3D::get_closest_point_to_segment(origin, segment);
			if (origin.distance_to(p) >= path_max_distance) {
				// To faraway, reload path
				reload_path = true;
			}
		}
	}

	if (reload_path) {
		navigation_query->set_start_position(origin);
		navigation_query->set_target_position(target_position);
		navigation_query->set_navigation_layers(navigation_layers);
		navigation_query->set_metadata_flags(path_metadata_flags);

		if (map_override.is_valid()) {
			navigation_query->set_map(map_override);
		} else {
			navigation_query->set_map(agent_parent->get_world_3d()->get_navigation_map());
		}

		NavigationServer3D::get_singleton()->query_path(navigation_query, navigation_result);
#ifdef DEBUG_ENABLED
		debug_path_dirty = true;
#endif // DEBUG_ENABLED
		navigation_finished = false;
		last_waypoint_reached = false;
		navigation_path_index = 0;
		emit_signal(SNAME("path_changed"));
	}

	if (navigation_result->get_path().size() == 0) {
		return;
	}

	// Check if the navigation has already finished.
	if (navigation_finished) {
		return;
	}

	// Check if we reached the target.
	if (_is_within_target_distance(origin)) {
		// Emit waypoint_reached in case we also moved within distance of a waypoint.
		_advance_waypoints(origin);
		_transition_to_target_reached();
		_transition_to_navigation_finished();
	} else {
		// Advance waypoints if possible.
		_advance_waypoints(origin);
		// Keep navigation running even after reaching the last waypoint if the target is reachable.
		if (last_waypoint_reached && !_is_target_reachable()) {
			_transition_to_navigation_finished();
		}
	}
}

void NavigationAgent3D::_advance_waypoints(const Vector3 &p_origin) {
	if (last_waypoint_reached) {
		return;
	}

	// Advance to the farthest possible waypoint.
	while (_is_within_waypoint_distance(p_origin)) {
		_trigger_waypoint_reached();

		if (_is_last_waypoint()) {
			last_waypoint_reached = true;
			break;
		}

		_move_to_next_waypoint();
	}
}

void NavigationAgent3D::_request_repath() {
	navigation_result->reset();
	target_reached = false;
	navigation_finished = false;
	last_waypoint_reached = false;
}

bool NavigationAgent3D::_is_last_waypoint() const {
	return navigation_path_index == navigation_result->get_path().size() - 1;
}

void NavigationAgent3D::_move_to_next_waypoint() {
	navigation_path_index += 1;
}

bool NavigationAgent3D::_is_within_waypoint_distance(const Vector3 &p_origin) const {
	const Vector<Vector3> &navigation_path = navigation_result->get_path();
	Vector3 waypoint = navigation_path[navigation_path_index] - Vector3(0, path_height_offset, 0);
	return p_origin.distance_to(waypoint) < path_desired_distance;
}

bool NavigationAgent3D::_is_within_target_distance(const Vector3 &p_origin) const {
	return p_origin.distance_to(target_position) < target_desired_distance;
}

void NavigationAgent3D::_trigger_waypoint_reached() {
	const Vector<Vector3> &navigation_path = navigation_result->get_path();
	const Vector<int32_t> &navigation_path_types = navigation_result->get_path_types();
	const TypedArray<RID> &navigation_path_rids = navigation_result->get_path_rids();
	const Vector<int64_t> &navigation_path_owners = navigation_result->get_path_owner_ids();

	Dictionary details;

	const Vector3 waypoint = navigation_path[navigation_path_index];
	details[CoreStringName(position)] = waypoint;

	int waypoint_type = -1;
	if (path_metadata_flags.has_flag(NavigationPathQueryParameters3D::PathMetadataFlags::PATH_METADATA_INCLUDE_TYPES)) {
		const NavigationPathQueryResult3D::PathSegmentType type = NavigationPathQueryResult3D::PathSegmentType(navigation_path_types[navigation_path_index]);

		details[SNAME("type")] = type;
		waypoint_type = type;
	}

	if (path_metadata_flags.has_flag(NavigationPathQueryParameters3D::PathMetadataFlags::PATH_METADATA_INCLUDE_RIDS)) {
		details[SNAME("rid")] = navigation_path_rids[navigation_path_index];
	}

	if (path_metadata_flags.has_flag(NavigationPathQueryParameters3D::PathMetadataFlags::PATH_METADATA_INCLUDE_OWNERS)) {
		const ObjectID waypoint_owner_id = ObjectID(navigation_path_owners[navigation_path_index]);

		// Get a reference to the owning object.
		Object *owner = nullptr;
		if (waypoint_owner_id.is_valid()) {
			owner = ObjectDB::get_instance(waypoint_owner_id);
		}

		details[SNAME("owner")] = owner;

		if (waypoint_type == NavigationPathQueryResult3D::PATH_SEGMENT_TYPE_LINK) {
			const NavigationLink3D *navlink = Object::cast_to<NavigationLink3D>(owner);
			if (navlink) {
				Vector3 link_global_start_position = navlink->get_global_start_position();
				Vector3 link_global_end_position = navlink->get_global_end_position();
				if (waypoint.distance_to(link_global_start_position) < waypoint.distance_to(link_global_end_position)) {
					details[SNAME("link_entry_position")] = link_global_start_position;
					details[SNAME("link_exit_position")] = link_global_end_position;
				} else {
					details[SNAME("link_entry_position")] = link_global_end_position;
					details[SNAME("link_exit_position")] = link_global_start_position;
				}
			}
		}
	}

	// Emit a signal for the waypoint.
	emit_signal(SNAME("waypoint_reached"), details);

	// Emit a signal if we've reached a navigation link.
	if (waypoint_type == NavigationPathQueryResult3D::PATH_SEGMENT_TYPE_LINK) {
		emit_signal(SNAME("link_reached"), details);
	}
}

void NavigationAgent3D::_transition_to_navigation_finished() {
	navigation_finished = true;
	target_position_submitted = false;

	if (avoidance_enabled) {
		NavigationServer3D::get_singleton()->agent_set_position(agent, agent_parent->get_global_transform().origin);
		NavigationServer3D::get_singleton()->agent_set_velocity(agent, Vector3(0.0, 0.0, 0.0));
		NavigationServer3D::get_singleton()->agent_set_velocity_forced(agent, Vector3(0.0, 0.0, 0.0));
		stored_y_velocity = 0.0;
	}

	emit_signal(SNAME("navigation_finished"));
}

void NavigationAgent3D::_transition_to_target_reached() {
	target_reached = true;
	emit_signal(SNAME("target_reached"));
}

void NavigationAgent3D::set_avoidance_layers(uint32_t p_layers) {
	avoidance_layers = p_layers;
	NavigationServer3D::get_singleton()->agent_set_avoidance_layers(get_rid(), avoidance_layers);
}

uint32_t NavigationAgent3D::get_avoidance_layers() const {
	return avoidance_layers;
}

void NavigationAgent3D::set_avoidance_mask(uint32_t p_mask) {
	avoidance_mask = p_mask;
	NavigationServer3D::get_singleton()->agent_set_avoidance_mask(get_rid(), avoidance_mask);
}

uint32_t NavigationAgent3D::get_avoidance_mask() const {
	return avoidance_mask;
}

void NavigationAgent3D::set_avoidance_layer_value(int p_layer_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_layer_number < 1, "Avoidance layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_layer_number > 32, "Avoidance layer number must be between 1 and 32 inclusive.");
	uint32_t avoidance_layers_new = get_avoidance_layers();
	if (p_value) {
		avoidance_layers_new |= 1 << (p_layer_number - 1);
	} else {
		avoidance_layers_new &= ~(1 << (p_layer_number - 1));
	}
	set_avoidance_layers(avoidance_layers_new);
}

bool NavigationAgent3D::get_avoidance_layer_value(int p_layer_number) const {
	ERR_FAIL_COND_V_MSG(p_layer_number < 1, false, "Avoidance layer number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_layer_number > 32, false, "Avoidance layer number must be between 1 and 32 inclusive.");
	return get_avoidance_layers() & (1 << (p_layer_number - 1));
}

void NavigationAgent3D::set_avoidance_mask_value(int p_mask_number, bool p_value) {
	ERR_FAIL_COND_MSG(p_mask_number < 1, "Avoidance mask number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_MSG(p_mask_number > 32, "Avoidance mask number must be between 1 and 32 inclusive.");
	uint32_t mask = get_avoidance_mask();
	if (p_value) {
		mask |= 1 << (p_mask_number - 1);
	} else {
		mask &= ~(1 << (p_mask_number - 1));
	}
	set_avoidance_mask(mask);
}

bool NavigationAgent3D::get_avoidance_mask_value(int p_mask_number) const {
	ERR_FAIL_COND_V_MSG(p_mask_number < 1, false, "Avoidance mask number must be between 1 and 32 inclusive.");
	ERR_FAIL_COND_V_MSG(p_mask_number > 32, false, "Avoidance mask number must be between 1 and 32 inclusive.");
	return get_avoidance_mask() & (1 << (p_mask_number - 1));
}

void NavigationAgent3D::set_avoidance_priority(real_t p_priority) {
	ERR_FAIL_COND_MSG(p_priority < 0.0, "Avoidance priority must be between 0.0 and 1.0 inclusive.");
	ERR_FAIL_COND_MSG(p_priority > 1.0, "Avoidance priority must be between 0.0 and 1.0 inclusive.");
	avoidance_priority = p_priority;
	NavigationServer3D::get_singleton()->agent_set_avoidance_priority(get_rid(), p_priority);
}

real_t NavigationAgent3D::get_avoidance_priority() const {
	return avoidance_priority;
}

////////DEBUG////////////////////////////////////////////////////////////

void NavigationAgent3D::set_debug_enabled(bool p_enabled) {
#ifdef DEBUG_ENABLED
	if (debug_enabled == p_enabled) {
		return;
	}

	debug_enabled = p_enabled;
	debug_path_dirty = true;
#endif // DEBUG_ENABLED
}

bool NavigationAgent3D::get_debug_enabled() const {
	return debug_enabled;
}

void NavigationAgent3D::set_debug_use_custom(bool p_enabled) {
#ifdef DEBUG_ENABLED
	if (debug_use_custom == p_enabled) {
		return;
	}

	debug_use_custom = p_enabled;
	debug_path_dirty = true;
#endif // DEBUG_ENABLED
}

bool NavigationAgent3D::get_debug_use_custom() const {
	return debug_use_custom;
}

void NavigationAgent3D::set_debug_path_custom_color(Color p_color) {
#ifdef DEBUG_ENABLED
	if (debug_path_custom_color == p_color) {
		return;
	}

	debug_path_custom_color = p_color;
	debug_path_dirty = true;
#endif // DEBUG_ENABLED
}

Color NavigationAgent3D::get_debug_path_custom_color() const {
	return debug_path_custom_color;
}

void NavigationAgent3D::set_debug_path_custom_point_size(float p_point_size) {
#ifdef DEBUG_ENABLED
	if (Math::is_equal_approx(debug_path_custom_point_size, p_point_size)) {
		return;
	}

	debug_path_custom_point_size = MAX(0.0, p_point_size);
	debug_path_dirty = true;
#endif // DEBUG_ENABLED
}

float NavigationAgent3D::get_debug_path_custom_point_size() const {
	return debug_path_custom_point_size;
}

#ifdef DEBUG_ENABLED
void NavigationAgent3D::_navigation_debug_changed() {
	debug_path_dirty = true;
}

void NavigationAgent3D::_update_debug_path() {
	if (!debug_path_dirty) {
		return;
	}
	debug_path_dirty = false;

	if (!debug_path_instance.is_valid()) {
		debug_path_instance = RenderingServer::get_singleton()->instance_create();
	}

	if (debug_path_mesh.is_null()) {
		debug_path_mesh.instantiate();
	}

	debug_path_mesh->clear_surfaces();

	if (!(debug_enabled && NavigationServer3D::get_singleton()->get_debug_navigation_enable_agent_paths())) {
		return;
	}

	if (!(agent_parent && agent_parent->is_inside_tree())) {
		return;
	}

	const Vector<Vector3> &navigation_path = navigation_result->get_path();

	if (navigation_path.size() <= 1) {
		return;
	}

	Vector<Vector3> debug_path_lines_vertex_array;

	for (int i = 0; i < navigation_path.size() - 1; i++) {
		debug_path_lines_vertex_array.push_back(navigation_path[i]);
		debug_path_lines_vertex_array.push_back(navigation_path[i + 1]);
	}

	Array debug_path_lines_mesh_array;
	debug_path_lines_mesh_array.resize(Mesh::ARRAY_MAX);
	debug_path_lines_mesh_array[Mesh::ARRAY_VERTEX] = debug_path_lines_vertex_array;

	debug_path_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, debug_path_lines_mesh_array);

	Ref<StandardMaterial3D> debug_agent_path_line_material = NavigationServer3D::get_singleton()->get_debug_navigation_agent_path_line_material();
	if (debug_use_custom) {
		if (debug_agent_path_line_custom_material.is_null()) {
			debug_agent_path_line_custom_material = debug_agent_path_line_material->duplicate();
		}
		debug_agent_path_line_custom_material->set_albedo(debug_path_custom_color);
		debug_path_mesh->surface_set_material(0, debug_agent_path_line_custom_material);
	} else {
		debug_path_mesh->surface_set_material(0, debug_agent_path_line_material);
	}

	if (debug_path_custom_point_size > 0.0) {
		Vector<Vector3> debug_path_points_vertex_array;

		for (int i = 0; i < navigation_path.size(); i++) {
			debug_path_points_vertex_array.push_back(navigation_path[i]);
		}

		Array debug_path_points_mesh_array;
		debug_path_points_mesh_array.resize(Mesh::ARRAY_MAX);
		debug_path_points_mesh_array[Mesh::ARRAY_VERTEX] = debug_path_points_vertex_array;

		debug_path_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_POINTS, debug_path_points_mesh_array);

		Ref<StandardMaterial3D> debug_agent_path_point_material = NavigationServer3D::get_singleton()->get_debug_navigation_agent_path_point_material();
		if (debug_use_custom) {
			if (debug_agent_path_point_custom_material.is_null()) {
				debug_agent_path_point_custom_material = debug_agent_path_point_material->duplicate();
			}
			debug_agent_path_point_custom_material->set_albedo(debug_path_custom_color);
			debug_agent_path_point_custom_material->set_point_size(debug_path_custom_point_size);
			debug_path_mesh->surface_set_material(1, debug_agent_path_point_custom_material);
		} else {
			debug_path_mesh->surface_set_material(1, debug_agent_path_point_material);
		}
	}

	RS::get_singleton()->instance_set_base(debug_path_instance, debug_path_mesh->get_rid());
	RS::get_singleton()->instance_set_scenario(debug_path_instance, agent_parent->get_world_3d()->get_scenario());
	RS::get_singleton()->instance_set_visible(debug_path_instance, agent_parent->is_visible_in_tree());
}
#endif // DEBUG_ENABLED
