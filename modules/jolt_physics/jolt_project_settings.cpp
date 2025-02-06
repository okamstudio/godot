/**************************************************************************/
/*  jolt_project_settings.cpp                                             */
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

#include "jolt_project_settings.h"

#include "core/config/project_settings.h"

void JoltProjectSettings::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/jolt_physics_3d/simulation/velocity_steps", PROPERTY_HINT_RANGE, U"2,16,or_greater"), 10);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/jolt_physics_3d/simulation/position_steps", PROPERTY_HINT_RANGE, U"1,16,or_greater"), 2);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/jolt_physics_3d/simulation/use_enhanced_internal_edge_removal"), true);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/jolt_physics_3d/simulation/areas_detect_static_bodies"), false);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/jolt_physics_3d/simulation/generate_all_kinematic_contacts"), false);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/penetration_slop", PROPERTY_HINT_RANGE, U"0,1,0.00001,or_greater,suffix:m"), 0.02f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/speculative_contact_distance", PROPERTY_HINT_RANGE, U"0,1,0.00001,or_greater,suffix:m"), 0.02f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/baumgarte_stabilization_factor", PROPERTY_HINT_RANGE, U"0,1,0.01"), 0.2f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/soft_body_point_radius", PROPERTY_HINT_RANGE, U"0,1,0.001,or_greater,suffix:m"), 0.01f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/bounce_velocity_threshold", PROPERTY_HINT_RANGE, U"0,1,0.001,or_greater,suffix:m/s"), 1.0f);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/jolt_physics_3d/simulation/allow_sleep"), true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/sleep_velocity_threshold", PROPERTY_HINT_RANGE, U"0,1,0.001,or_greater,suffix:m/s"), 0.03f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/sleep_time_threshold", PROPERTY_HINT_RANGE, U"0,5,0.01,or_greater,suffix:s"), 0.5f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/continuous_cd_movement_threshold", PROPERTY_HINT_RANGE, U"0,1,0.01"), 0.75f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/continuous_cd_max_penetration", PROPERTY_HINT_RANGE, U"0,1,0.01"), 0.25f);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/jolt_physics_3d/simulation/body_pair_contact_cache_enabled"), true);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/body_pair_contact_cache_distance_threshold", PROPERTY_HINT_RANGE, U"0,0.01,0.00001,or_greater,suffix:m"), 0.001f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/simulation/body_pair_contact_cache_angle_threshold", PROPERTY_HINT_RANGE, U"0,180,0.01,radians_as_degrees"), Math::deg_to_rad(2.0f));

	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/jolt_physics_3d/queries/use_enhanced_internal_edge_removal"), false);
	GLOBAL_DEF_RST(PropertyInfo(Variant::BOOL, "physics/jolt_physics_3d/queries/enable_ray_cast_face_index"), false);

	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/jolt_physics_3d/motion_queries/use_enhanced_internal_edge_removal"), true);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/jolt_physics_3d/motion_queries/recovery_iterations", PROPERTY_HINT_RANGE, U"1,8,or_greater"), 4);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/motion_queries/recovery_amount", PROPERTY_HINT_RANGE, U"0,1,0.01"), 0.4f);

	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/collisions/collision_margin_fraction", PROPERTY_HINT_RANGE, U"0,1,0.00001"), 0.08f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/collisions/active_edge_threshold", PROPERTY_HINT_RANGE, U"0,90,0.01,radians_as_degrees"), Math::deg_to_rad(50.0f));

	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/jolt_physics_3d/joints/world_node", PROPERTY_HINT_ENUM, U"Node A,Node B"), JOLT_JOINT_WORLD_NODE_A);

	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/jolt_physics_3d/limits/temporary_memory_buffer_size", PROPERTY_HINT_RANGE, U"1,32,or_greater,suffix:MiB"), 32);
	GLOBAL_DEF_RST(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/limits/world_boundary_shape_size", PROPERTY_HINT_RANGE, U"2,2000,0.1,or_greater,suffix:m"), 2000.0f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/limits/max_linear_velocity", PROPERTY_HINT_RANGE, U"0,500,0.01,or_greater,suffix:m/s"), 500.0f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/jolt_physics_3d/limits/max_angular_velocity", PROPERTY_HINT_RANGE, U"0,2700,0.01,or_greater,radians_as_degrees,suffix:°/s"), Math::deg_to_rad(2700.0f));
	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "physics/jolt_physics_3d/limits/max_bodies", PROPERTY_HINT_RANGE, U"1,10240,or_greater"), 10240);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/jolt_physics_3d/limits/max_body_pairs", PROPERTY_HINT_RANGE, U"8,65536,or_greater"), 65536);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/jolt_physics_3d/limits/max_contact_constraints", PROPERTY_HINT_RANGE, U"8,20480,or_greater"), 20480);

	read_settings();

	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp_static(JoltProjectSettings::read_settings));
}

void JoltProjectSettings::read_settings() {
	simulation_velocity_steps = GLOBAL_GET("physics/jolt_physics_3d/simulation/velocity_steps");
	simulation_position_steps = GLOBAL_GET("physics/jolt_physics_3d/simulation/position_steps");
	use_enhanced_internal_edge_removal_for_bodies = GLOBAL_GET("physics/jolt_physics_3d/simulation/use_enhanced_internal_edge_removal");
	areas_detect_static_bodies = GLOBAL_GET("physics/jolt_physics_3d/simulation/areas_detect_static_bodies");
	generate_all_kinematic_contacts = GLOBAL_GET("physics/jolt_physics_3d/simulation/generate_all_kinematic_contacts");
	penetration_slop = GLOBAL_GET("physics/jolt_physics_3d/simulation/penetration_slop");
	speculative_contact_distance = GLOBAL_GET("physics/jolt_physics_3d/simulation/speculative_contact_distance");
	baumgarte_stabilization_factor = GLOBAL_GET("physics/jolt_physics_3d/simulation/baumgarte_stabilization_factor");
	soft_body_point_radius = GLOBAL_GET("physics/jolt_physics_3d/simulation/soft_body_point_radius");
	bounce_velocity_threshold = GLOBAL_GET("physics/jolt_physics_3d/simulation/bounce_velocity_threshold");
	sleep_allowed = GLOBAL_GET("physics/jolt_physics_3d/simulation/allow_sleep");
	sleep_velocity_threshold = GLOBAL_GET("physics/jolt_physics_3d/simulation/sleep_velocity_threshold");
	sleep_time_threshold = GLOBAL_GET("physics/jolt_physics_3d/simulation/sleep_time_threshold");
	ccd_movement_threshold = GLOBAL_GET("physics/jolt_physics_3d/simulation/continuous_cd_movement_threshold");
	ccd_max_penetration = GLOBAL_GET("physics/jolt_physics_3d/simulation/continuous_cd_max_penetration");
	body_pair_contact_cache_enabled = GLOBAL_GET("physics/jolt_physics_3d/simulation/body_pair_contact_cache_enabled");
	float body_pair_cache_distance = GLOBAL_GET("physics/jolt_physics_3d/simulation/body_pair_contact_cache_distance_threshold");
	body_pair_cache_distance_sq = body_pair_cache_distance * body_pair_cache_distance;
	float body_pair_cache_angle = GLOBAL_GET("physics/jolt_physics_3d/simulation/body_pair_contact_cache_angle_threshold");
	body_pair_cache_angle_cos_div2 = Math::cos(body_pair_cache_angle / 2.0f);

	use_enhanced_internal_edge_removal_for_queries = GLOBAL_GET("physics/jolt_physics_3d/queries/use_enhanced_internal_edge_removal");
	enable_ray_cast_face_index = GLOBAL_GET("physics/jolt_physics_3d/queries/enable_ray_cast_face_index");

	use_enhanced_internal_edge_removal_for_motion_queries = GLOBAL_GET("physics/jolt_physics_3d/motion_queries/use_enhanced_internal_edge_removal");
	motion_query_recovery_iterations = GLOBAL_GET("physics/jolt_physics_3d/motion_queries/recovery_iterations");
	motion_query_recovery_amount = GLOBAL_GET("physics/jolt_physics_3d/motion_queries/recovery_amount");

	collision_margin_fraction = GLOBAL_GET("physics/jolt_physics_3d/collisions/collision_margin_fraction");
	float active_edge_threshold = GLOBAL_GET("physics/jolt_physics_3d/collisions/active_edge_threshold");
	active_edge_threshold_cos = Math::cos(active_edge_threshold);

	joint_world_node = (JoltJointWorldNode)(int)GLOBAL_GET("physics/jolt_physics_3d/joints/world_node");

	temp_memory_mib = GLOBAL_GET("physics/jolt_physics_3d/limits/temporary_memory_buffer_size");
	temp_memory_b = temp_memory_mib * 1024 * 1024;
	world_boundary_shape_size = GLOBAL_GET("physics/jolt_physics_3d/limits/world_boundary_shape_size");
	max_linear_velocity = GLOBAL_GET("physics/jolt_physics_3d/limits/max_linear_velocity");
	max_angular_velocity = GLOBAL_GET("physics/jolt_physics_3d/limits/max_angular_velocity");
	max_bodies = GLOBAL_GET("physics/jolt_physics_3d/limits/max_bodies");
	max_body_pairs = GLOBAL_GET("physics/jolt_physics_3d/limits/max_body_pairs");
	max_contact_constraints = GLOBAL_GET("physics/jolt_physics_3d/limits/max_contact_constraints");
}
