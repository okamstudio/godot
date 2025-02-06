/**************************************************************************/
/*  physics_server_2d_extension.cpp                                       */
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

#include "physics_server_2d_extension.h"

bool PhysicsDirectSpaceState2DExtension::is_body_excluded_from_query(const RID &p_body) const {
	return exclude && exclude->has(p_body);
}

thread_local const HashSet<RID> *PhysicsDirectSpaceState2DExtension::exclude = nullptr;

void PhysicsDirectSpaceState2DExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_body_excluded_from_query", "body"), &PhysicsDirectSpaceState2DExtension::is_body_excluded_from_query);

	GDVIRTUAL_BIND(_intersect_ray, "from", "to", "collision_mask", "collide_with_bodies", "collide_with_areas", "hit_from_inside", "result");
	GDVIRTUAL_BIND(_intersect_point, "position", "canvas_instance_id", "collision_mask", "collide_with_bodies", "collide_with_areas", "results", "max_results");
	GDVIRTUAL_BIND(_intersect_shape, "shape_rid", "transform", "motion", "margin", "collision_mask", "collide_with_bodies", "collide_with_areas", "result", "max_results");
	GDVIRTUAL_BIND(_cast_motion, "shape_rid", "transform", "motion", "margin", "collision_mask", "collide_with_bodies", "collide_with_areas", "closest_safe", "closest_unsafe");
	GDVIRTUAL_BIND(_collide_shape, "shape_rid", "transform", "motion", "margin", "collision_mask", "collide_with_bodies", "collide_with_areas", "results", "max_results", "result_count");
	GDVIRTUAL_BIND(_rest_info, "shape_rid", "transform", "motion", "margin", "collision_mask", "collide_with_bodies", "collide_with_areas", "rest_info");
}

PhysicsDirectSpaceState2DExtension::PhysicsDirectSpaceState2DExtension() {
}

void PhysicsDirectBodyState2DExtension::_bind_methods() {
	GDVIRTUAL_BIND(_get_total_gravity);
	GDVIRTUAL_BIND(_get_total_linear_damp);
	GDVIRTUAL_BIND(_get_total_angular_damp);

	GDVIRTUAL_BIND(_get_center_of_mass);
	GDVIRTUAL_BIND(_get_center_of_mass_local);
	GDVIRTUAL_BIND(_get_inverse_mass);
	GDVIRTUAL_BIND(_get_inverse_inertia);

	GDVIRTUAL_BIND(_set_linear_velocity, "velocity");
	GDVIRTUAL_BIND(_get_linear_velocity);

	GDVIRTUAL_BIND(_set_angular_velocity, "velocity");
	GDVIRTUAL_BIND(_get_angular_velocity);

	GDVIRTUAL_BIND(_set_transform, "transform");
	GDVIRTUAL_BIND(_get_transform);

	GDVIRTUAL_BIND(_get_velocity_at_local_position, "local_position");

	GDVIRTUAL_BIND(_apply_central_impulse, "impulse");
	GDVIRTUAL_BIND(_apply_impulse, "impulse", "position");
	GDVIRTUAL_BIND(_apply_torque_impulse, "impulse");

	GDVIRTUAL_BIND(_apply_central_force, "force");
	GDVIRTUAL_BIND(_apply_force, "force", "position");
	GDVIRTUAL_BIND(_apply_torque, "torque");

	GDVIRTUAL_BIND(_add_constant_central_force, "force");
	GDVIRTUAL_BIND(_add_constant_force, "force", "position");
	GDVIRTUAL_BIND(_add_constant_torque, "torque");

	GDVIRTUAL_BIND(_set_constant_force, "force");
	GDVIRTUAL_BIND(_get_constant_force);

	GDVIRTUAL_BIND(_set_constant_torque, "torque");
	GDVIRTUAL_BIND(_get_constant_torque);

	GDVIRTUAL_BIND(_set_sleep_state, "enabled");
	GDVIRTUAL_BIND(_is_sleeping);

	GDVIRTUAL_BIND(_get_contact_count);

	GDVIRTUAL_BIND(_get_contact_local_position, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_local_normal, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_local_shape, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_local_velocity_at_position, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_collider, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_collider_position, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_collider_id, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_collider_object, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_collider_shape, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_collider_velocity_at_position, "contact_idx");
	GDVIRTUAL_BIND(_get_contact_impulse, "contact_idx");

	GDVIRTUAL_BIND(_get_step);
	GDVIRTUAL_BIND(_integrate_forces);

	GDVIRTUAL_BIND(_get_space_state);
}

PhysicsDirectBodyState2DExtension::PhysicsDirectBodyState2DExtension() {
}

thread_local const HashSet<RID> *PhysicsServer2DExtension::exclude_bodies = nullptr;
thread_local const HashSet<ObjectID> *PhysicsServer2DExtension::exclude_objects = nullptr;

bool PhysicsServer2DExtension::body_test_motion_is_excluding_body(RID p_body) const {
	return exclude_bodies && exclude_bodies->has(p_body);
}

bool PhysicsServer2DExtension::body_test_motion_is_excluding_object(ObjectID p_object) const {
	return exclude_objects && exclude_objects->has(p_object);
}

void PhysicsServer2DExtension::_bind_methods() {
	/* SHAPE API */

	GDVIRTUAL_BIND(_world_boundary_shape_create);
	GDVIRTUAL_BIND(_separation_ray_shape_create);
	GDVIRTUAL_BIND(_segment_shape_create);
	GDVIRTUAL_BIND(_circle_shape_create);
	GDVIRTUAL_BIND(_rectangle_shape_create);
	GDVIRTUAL_BIND(_capsule_shape_create);
	GDVIRTUAL_BIND(_convex_polygon_shape_create);
	GDVIRTUAL_BIND(_concave_polygon_shape_create);

	GDVIRTUAL_BIND(_shape_set_data, "shape", "data");
	GDVIRTUAL_BIND(_shape_set_custom_solver_bias, "shape", "bias");

	GDVIRTUAL_BIND(_shape_get_type, "shape");
	GDVIRTUAL_BIND(_shape_get_data, "shape");
	GDVIRTUAL_BIND(_shape_get_custom_solver_bias, "shape");
	GDVIRTUAL_BIND(_shape_collide, "shape_A", "xform_A", "motion_A", "shape_B", "xform_B", "motion_B", "results", "result_max", "result_count");

	/* SPACE API */

	GDVIRTUAL_BIND(_space_create);
	GDVIRTUAL_BIND(_space_set_active, "space", "active");
	GDVIRTUAL_BIND(_space_is_active, "space");

	GDVIRTUAL_BIND(_space_set_param, "space", "param", "value");
	GDVIRTUAL_BIND(_space_get_param, "space", "param");

	GDVIRTUAL_BIND(_space_get_direct_state, "space");

	GDVIRTUAL_BIND(_space_set_debug_contacts, "space", "max_contacts");
	GDVIRTUAL_BIND(_space_get_contacts, "space");
	GDVIRTUAL_BIND(_space_get_contact_count, "space");

	/* AREA API */

	GDVIRTUAL_BIND(_area_create);

	GDVIRTUAL_BIND(_area_set_space, "area", "space");
	GDVIRTUAL_BIND(_area_get_space, "area");

	GDVIRTUAL_BIND(_area_add_shape, "area", "shape", "transform", "disabled");
	GDVIRTUAL_BIND(_area_set_shape, "area", "shape_idx", "shape");
	GDVIRTUAL_BIND(_area_set_shape_transform, "area", "shape_idx", "transform");
	GDVIRTUAL_BIND(_area_set_shape_disabled, "area", "shape_idx", "disabled");

	GDVIRTUAL_BIND(_area_get_shape_count, "area");
	GDVIRTUAL_BIND(_area_get_shape, "area", "shape_idx");
	GDVIRTUAL_BIND(_area_get_shape_transform, "area", "shape_idx");

	GDVIRTUAL_BIND(_area_remove_shape, "area", "shape_idx");
	GDVIRTUAL_BIND(_area_clear_shapes, "area");

	GDVIRTUAL_BIND(_area_attach_object_instance_id, "area", "id");
	GDVIRTUAL_BIND(_area_get_object_instance_id, "area");

	GDVIRTUAL_BIND(_area_attach_canvas_instance_id, "area", "id");
	GDVIRTUAL_BIND(_area_get_canvas_instance_id, "area");

	GDVIRTUAL_BIND(_area_set_param, "area", "param", "value");
	GDVIRTUAL_BIND(_area_set_transform, "area", "transform");

	GDVIRTUAL_BIND(_area_get_param, "area", "param");
	GDVIRTUAL_BIND(_area_get_transform, "area");

	GDVIRTUAL_BIND(_area_set_collision_layer, "area", "layer");
	GDVIRTUAL_BIND(_area_get_collision_layer, "area");

	GDVIRTUAL_BIND(_area_set_collision_mask, "area", "mask");
	GDVIRTUAL_BIND(_area_get_collision_mask, "area");

	GDVIRTUAL_BIND(_area_set_monitorable, "area", "monitorable");
	GDVIRTUAL_BIND(_area_set_pickable, "area", "pickable");

	GDVIRTUAL_BIND(_area_set_monitor_callback, "area", "callback");
	GDVIRTUAL_BIND(_area_set_area_monitor_callback, "area", "callback");

	/* BODY API */

	ClassDB::bind_method(D_METHOD("body_test_motion_is_excluding_body", "body"), &PhysicsServer2DExtension::body_test_motion_is_excluding_body);
	ClassDB::bind_method(D_METHOD("body_test_motion_is_excluding_object", "object"), &PhysicsServer2DExtension::body_test_motion_is_excluding_object);

	GDVIRTUAL_BIND(_body_create);

	GDVIRTUAL_BIND(_body_set_space, "body", "space");
	GDVIRTUAL_BIND(_body_get_space, "body");

	GDVIRTUAL_BIND(_body_set_mode, "body", "mode");
	GDVIRTUAL_BIND(_body_get_mode, "body");

	GDVIRTUAL_BIND(_body_add_shape, "body", "shape", "transform", "disabled");
	GDVIRTUAL_BIND(_body_set_shape, "body", "shape_idx", "shape");
	GDVIRTUAL_BIND(_body_set_shape_transform, "body", "shape_idx", "transform");

	GDVIRTUAL_BIND(_body_get_shape_count, "body");
	GDVIRTUAL_BIND(_body_get_shape, "body", "shape_idx");
	GDVIRTUAL_BIND(_body_get_shape_transform, "body", "shape_idx");

	GDVIRTUAL_BIND(_body_set_shape_disabled, "body", "shape_idx", "disabled");
	GDVIRTUAL_BIND(_body_set_shape_as_one_way_collision, "body", "shape_idx", "enable", "margin");

	GDVIRTUAL_BIND(_body_remove_shape, "body", "shape_idx");
	GDVIRTUAL_BIND(_body_clear_shapes, "body");

	GDVIRTUAL_BIND(_body_attach_object_instance_id, "body", "id");
	GDVIRTUAL_BIND(_body_get_object_instance_id, "body");

	GDVIRTUAL_BIND(_body_attach_canvas_instance_id, "body", "id");
	GDVIRTUAL_BIND(_body_get_canvas_instance_id, "body");

	GDVIRTUAL_BIND(_body_set_continuous_collision_detection_mode, "body", "mode");
	GDVIRTUAL_BIND(_body_get_continuous_collision_detection_mode, "body");

	GDVIRTUAL_BIND(_body_set_collision_layer, "body", "layer");
	GDVIRTUAL_BIND(_body_get_collision_layer, "body");

	GDVIRTUAL_BIND(_body_set_collision_mask, "body", "mask");
	GDVIRTUAL_BIND(_body_get_collision_mask, "body");

	GDVIRTUAL_BIND(_body_set_collision_priority, "body", "priority");
	GDVIRTUAL_BIND(_body_get_collision_priority, "body");

	GDVIRTUAL_BIND(_body_set_param, "body", "param", "value");
	GDVIRTUAL_BIND(_body_get_param, "body", "param");

	GDVIRTUAL_BIND(_body_reset_mass_properties, "body");

	GDVIRTUAL_BIND(_body_set_state, "body", "state", "value");
	GDVIRTUAL_BIND(_body_get_state, "body", "state");

	GDVIRTUAL_BIND(_body_apply_central_impulse, "body", "impulse");
	GDVIRTUAL_BIND(_body_apply_torque_impulse, "body", "impulse");
	GDVIRTUAL_BIND(_body_apply_impulse, "body", "impulse", "position");

	GDVIRTUAL_BIND(_body_apply_central_force, "body", "force");
	GDVIRTUAL_BIND(_body_apply_force, "body", "force", "position");
	GDVIRTUAL_BIND(_body_apply_torque, "body", "torque");

	GDVIRTUAL_BIND(_body_add_constant_central_force, "body", "force");
	GDVIRTUAL_BIND(_body_add_constant_force, "body", "force", "position");
	GDVIRTUAL_BIND(_body_add_constant_torque, "body", "torque");

	GDVIRTUAL_BIND(_body_set_constant_force, "body", "force");
	GDVIRTUAL_BIND(_body_get_constant_force, "body");

	GDVIRTUAL_BIND(_body_set_constant_torque, "body", "torque");
	GDVIRTUAL_BIND(_body_get_constant_torque, "body");

	GDVIRTUAL_BIND(_body_set_axis_velocity, "body", "axis_velocity");

	GDVIRTUAL_BIND(_body_add_collision_exception, "body", "excepted_body");
	GDVIRTUAL_BIND(_body_remove_collision_exception, "body", "excepted_body");
	GDVIRTUAL_BIND(_body_get_collision_exceptions, "body");

	GDVIRTUAL_BIND(_body_set_max_contacts_reported, "body", "amount");
	GDVIRTUAL_BIND(_body_get_max_contacts_reported, "body");

	GDVIRTUAL_BIND(_body_set_contacts_reported_depth_threshold, "body", "threshold");
	GDVIRTUAL_BIND(_body_get_contacts_reported_depth_threshold, "body");

	GDVIRTUAL_BIND(_body_set_omit_force_integration, "body", "enable");
	GDVIRTUAL_BIND(_body_is_omitting_force_integration, "body");

	GDVIRTUAL_BIND(_body_set_state_sync_callback, "body", "callable");
	GDVIRTUAL_BIND(_body_set_force_integration_callback, "body", "callable", "userdata");

	GDVIRTUAL_BIND(_body_collide_shape, "body", "body_shape", "shape", "shape_xform", "motion", "results", "result_max", "result_count");

	GDVIRTUAL_BIND(_body_set_pickable, "body", "pickable");

	GDVIRTUAL_BIND(_body_get_direct_state, "body");

	GDVIRTUAL_BIND(_body_test_motion, "body", "from", "motion", "margin", "collide_separation_ray", "recovery_as_collision", "result");

	/* JOINT API */

	GDVIRTUAL_BIND(_joint_create);
	GDVIRTUAL_BIND(_joint_clear, "joint");

	GDVIRTUAL_BIND(_joint_set_param, "joint", "param", "value");
	GDVIRTUAL_BIND(_joint_get_param, "joint", "param");

	GDVIRTUAL_BIND(_joint_set_flag, "joint", "flag", "enabled");
	GDVIRTUAL_BIND(_joint_get_flag, "joint", "flag");

	GDVIRTUAL_BIND(_joint_get_state, "joint", "state");

	GDVIRTUAL_BIND(_joint_disable_collisions_between_bodies, "joint", "disable");
	GDVIRTUAL_BIND(_joint_is_disabled_collisions_between_bodies, "joint");

	GDVIRTUAL_BIND(_joint_make_pin, "joint", "anchor", "body_a", "body_b");
	GDVIRTUAL_BIND(_joint_make_groove, "joint", "a_groove1", "a_groove2", "b_anchor", "body_a", "body_b");
	GDVIRTUAL_BIND(_joint_make_damped_spring, "joint", "anchor_a", "anchor_b", "body_a", "body_b");

	GDVIRTUAL_BIND(_pin_joint_set_flag, "joint", "flag", "enabled");
	GDVIRTUAL_BIND(_pin_joint_get_flag, "joint", "flag");

	GDVIRTUAL_BIND(_pin_joint_set_param, "joint", "param", "value");
	GDVIRTUAL_BIND(_pin_joint_get_param, "joint", "param");

	GDVIRTUAL_BIND(_damped_spring_joint_set_param, "joint", "param", "value");
	GDVIRTUAL_BIND(_damped_spring_joint_get_param, "joint", "param");

	GDVIRTUAL_BIND(_joint_get_type, "joint");

	/* MISC */

	GDVIRTUAL_BIND(_free_rid, "rid");

	GDVIRTUAL_BIND(_set_active, "active");

	GDVIRTUAL_BIND(_init);
	GDVIRTUAL_BIND(_step, "step");
	GDVIRTUAL_BIND(_sync);
	GDVIRTUAL_BIND(_flush_queries);
	GDVIRTUAL_BIND(_end_sync);
	GDVIRTUAL_BIND(_finish);

	GDVIRTUAL_BIND(_is_flushing_queries);
	GDVIRTUAL_BIND(_get_process_info, "process_info");
}

PhysicsServer2DExtension::PhysicsServer2DExtension() {
}

PhysicsServer2DExtension::~PhysicsServer2DExtension() {
}
