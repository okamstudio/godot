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

namespace {

enum JoltJointWorldNode : int {
	JOLT_JOINT_WORLD_NODE_A,
	JOLT_JOINT_WORLD_NODE_B,
};

} // namespace

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
}

int JoltProjectSettings::get_simulation_velocity_steps() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/velocity_steps");
}

int JoltProjectSettings::get_simulation_position_steps() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/position_steps");
}

bool JoltProjectSettings::use_enhanced_internal_edge_removal_for_bodies() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/use_enhanced_internal_edge_removal");
}

bool JoltProjectSettings::areas_detect_static_bodies() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/areas_detect_static_bodies");
}

bool JoltProjectSettings::should_generate_all_kinematic_contacts() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/generate_all_kinematic_contacts");
}

float JoltProjectSettings::get_penetration_slop() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/penetration_slop");
}

float JoltProjectSettings::get_speculative_contact_distance() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/speculative_contact_distance");
}

float JoltProjectSettings::get_baumgarte_stabilization_factor() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/baumgarte_stabilization_factor");
}

float JoltProjectSettings::get_soft_body_point_radius() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/soft_body_point_radius");
}

float JoltProjectSettings::get_bounce_velocity_threshold() {
	static const float value = GLOBAL_GET("physics/jolt_physics_3d/simulation/bounce_velocity_threshold");
	return value;
}

bool JoltProjectSettings::is_sleep_allowed() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/allow_sleep");
}

float JoltProjectSettings::get_sleep_velocity_threshold() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/sleep_velocity_threshold");
}

float JoltProjectSettings::get_sleep_time_threshold() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/sleep_time_threshold");
}

float JoltProjectSettings::get_ccd_movement_threshold() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/continuous_cd_movement_threshold");
}

float JoltProjectSettings::get_ccd_max_penetration() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/continuous_cd_max_penetration");
}

bool JoltProjectSettings::is_body_pair_contact_cache_enabled() {
	return GLOBAL_GET("physics/jolt_physics_3d/simulation/body_pair_contact_cache_enabled");
}

float JoltProjectSettings::get_body_pair_cache_distance_sq() {
	const float value = GLOBAL_GET("physics/jolt_physics_3d/simulation/body_pair_contact_cache_distance_threshold");
	return value * value;
}

float JoltProjectSettings::get_body_pair_cache_angle_cos_div2() {
	return Math::cos((float)GLOBAL_GET("physics/jolt_physics_3d/simulation/body_pair_contact_cache_angle_threshold") / 2.0f);
}

bool JoltProjectSettings::use_enhanced_internal_edge_removal_for_queries() {
	static const bool value = GLOBAL_GET("physics/jolt_physics_3d/queries/use_enhanced_internal_edge_removal");
	return value;
}

bool JoltProjectSettings::enable_ray_cast_face_index() {
	static const bool value = GLOBAL_GET("physics/jolt_physics_3d/queries/enable_ray_cast_face_index");
	return value;
}

bool JoltProjectSettings::use_enhanced_internal_edge_removal_for_motion_queries() {
	static const bool value = GLOBAL_GET("physics/jolt_physics_3d/motion_queries/use_enhanced_internal_edge_removal");
	return value;
}

int JoltProjectSettings::get_motion_query_recovery_iterations() {
	static const int value = GLOBAL_GET("physics/jolt_physics_3d/motion_queries/recovery_iterations");
	return value;
}

float JoltProjectSettings::get_motion_query_recovery_amount() {
	static const float value = GLOBAL_GET("physics/jolt_physics_3d/motion_queries/recovery_amount");
	return value;
}

float JoltProjectSettings::get_collision_margin_fraction() {
	static const float value = GLOBAL_GET("physics/jolt_physics_3d/collisions/collision_margin_fraction");
	return value;
}

float JoltProjectSettings::get_active_edge_threshold() {
	static const float value = Math::cos((float)GLOBAL_GET("physics/jolt_physics_3d/collisions/active_edge_threshold"));
	return value;
}

bool JoltProjectSettings::use_joint_world_node_a() {
	return (int)GLOBAL_GET("physics/jolt_physics_3d/joints/world_node") == JOLT_JOINT_WORLD_NODE_A;
}

int JoltProjectSettings::get_temp_memory_mib() {
	return GLOBAL_GET("physics/jolt_physics_3d/limits/temporary_memory_buffer_size");
}

int64_t JoltProjectSettings::get_temp_memory_b() {
	return get_temp_memory_mib() * 1024 * 1024;
}

float JoltProjectSettings::get_world_boundary_shape_size() {
	return GLOBAL_GET("physics/jolt_physics_3d/limits/world_boundary_shape_size");
}

float JoltProjectSettings::get_max_linear_velocity() {
	return GLOBAL_GET("physics/jolt_physics_3d/limits/max_linear_velocity");
}

float JoltProjectSettings::get_max_angular_velocity() {
	return GLOBAL_GET("physics/jolt_physics_3d/limits/max_angular_velocity");
}

int JoltProjectSettings::get_max_bodies() {
	return GLOBAL_GET("physics/jolt_physics_3d/limits/max_bodies");
}

int JoltProjectSettings::get_max_pairs() {
	return GLOBAL_GET("physics/jolt_physics_3d/limits/max_body_pairs");
}

int JoltProjectSettings::get_max_contact_constraints() {
	return GLOBAL_GET("physics/jolt_physics_3d/limits/max_contact_constraints");
}
