/**************************************************************************/
/*  jolt_project_settings.h                                               */
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

#ifndef JOLT_PROJECT_SETTINGS_H
#define JOLT_PROJECT_SETTINGS_H

#include <stdint.h>

class JoltProjectSettings {
public:
	static void register_settings();

	static int get_simulation_velocity_steps();
	static int get_simulation_position_steps();
	static bool use_enhanced_internal_edge_removal_for_bodies();
	static bool areas_detect_static_bodies();
	static bool should_generate_all_kinematic_contacts();
	static float get_penetration_slop();
	static float get_speculative_contact_distance();
	static float get_baumgarte_stabilization_factor();
	static float get_soft_body_point_radius();
	static float get_bounce_velocity_threshold();
	static bool is_sleep_allowed();
	static float get_sleep_velocity_threshold();
	static float get_sleep_time_threshold();
	static float get_ccd_movement_threshold();
	static float get_ccd_max_penetration();
	static bool is_body_pair_contact_cache_enabled();
	static float get_body_pair_cache_distance_sq();
	static float get_body_pair_cache_angle_cos_div2();

	static bool use_enhanced_internal_edge_removal_for_queries();
	static bool enable_ray_cast_face_index();

	static bool use_enhanced_internal_edge_removal_for_motion_queries();
	static int get_motion_query_recovery_iterations();
	static float get_motion_query_recovery_amount();

	static float get_collision_margin_fraction();
	static float get_active_edge_threshold();

	static bool use_joint_world_node_a();

	static int get_temp_memory_mib();
	static int64_t get_temp_memory_b();
	static float get_world_boundary_shape_size();
	static float get_max_linear_velocity();
	static float get_max_angular_velocity();
	static int get_max_bodies();
	static int get_max_pairs();
	static int get_max_contact_constraints();
};

#endif // JOLT_PROJECT_SETTINGS_H
