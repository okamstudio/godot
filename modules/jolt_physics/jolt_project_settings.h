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

enum JoltJointWorldNode : int {
	JOLT_JOINT_WORLD_NODE_A,
	JOLT_JOINT_WORLD_NODE_B,
};

class JoltProjectSettings {
public:
	inline static int simulation_velocity_steps;
	inline static int simulation_position_steps;
	inline static bool use_enhanced_internal_edge_removal_for_bodies;
	inline static bool areas_detect_static_bodies;
	inline static bool generate_all_kinematic_contacts;
	inline static float penetration_slop;
	inline static float speculative_contact_distance;
	inline static float baumgarte_stabilization_factor;
	inline static float soft_body_point_radius;
	inline static float bounce_velocity_threshold;
	inline static bool sleep_allowed;
	inline static float sleep_velocity_threshold;
	inline static float sleep_time_threshold;
	inline static float ccd_movement_threshold;
	inline static float ccd_max_penetration;
	inline static bool body_pair_contact_cache_enabled;
	inline static float body_pair_cache_distance_sq;
	inline static float body_pair_cache_angle_cos_div2;

	inline static bool use_enhanced_internal_edge_removal_for_queries;
	inline static bool enable_ray_cast_face_index;

	inline static bool use_enhanced_internal_edge_removal_for_motion_queries;
	inline static int motion_query_recovery_iterations;
	inline static float motion_query_recovery_amount;

	inline static float collision_margin_fraction;
	inline static float active_edge_threshold_cos;

	inline static JoltJointWorldNode joint_world_node;

	inline static int temp_memory_mib;
	inline static int64_t temp_memory_b;
	inline static float world_boundary_shape_size;
	inline static float max_linear_velocity;
	inline static float max_angular_velocity;
	inline static int max_bodies;
	inline static int max_body_pairs;
	inline static int max_contact_constraints;

	static void register_settings();
	static void read_settings();
};

#endif // JOLT_PROJECT_SETTINGS_H
