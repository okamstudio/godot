/**************************************************************************/
/*  physics_server_2d_dummy.h                                             */
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

#include "physics_server_2d.h"

class PhysicsDirectBodyState2DDummy : public PhysicsDirectBodyState2D {
	GDCLASS(PhysicsDirectBodyState2DDummy, PhysicsDirectBodyState2D);

	PhysicsDirectSpaceState2D *space_state_dummy = nullptr;

public:
	virtual Vector2 get_total_gravity() const override { return Vector2(); }
	virtual real_t get_total_linear_damp() const override { return 0; }
	virtual real_t get_total_angular_damp() const override { return 0; }

	virtual Vector2 get_center_of_mass() const override { return Vector2(); }
	virtual Vector2 get_center_of_mass_local() const override { return Vector2(); }
	virtual real_t get_inverse_mass() const override { return 0; }
	virtual real_t get_inverse_inertia() const override { return 0; }

	virtual void set_linear_velocity(const Vector2 &p_velocity) override {}
	virtual Vector2 get_linear_velocity() const override { return Vector2(); }

	virtual void set_angular_velocity(real_t p_velocity) override {}
	virtual real_t get_angular_velocity() const override { return 0; }

	virtual void set_transform(const Transform2D &p_transform) override {}
	virtual Transform2D get_transform() const override { return Transform2D(); }

	virtual Vector2 get_velocity_at_local_position(const Vector2 &p_position) const override { return Vector2(); }

	virtual void apply_central_impulse(const Vector2 &p_impulse) override {}
	virtual void apply_torque_impulse(real_t p_torque) override {}
	virtual void apply_impulse(const Vector2 &p_impulse, const Vector2 &p_position = Vector2()) override {}

	virtual void apply_central_force(const Vector2 &p_force) override {}
	virtual void apply_force(const Vector2 &p_force, const Vector2 &p_position = Vector2()) override {}
	virtual void apply_torque(real_t p_torque) override {}

	virtual void add_constant_central_force(const Vector2 &p_force) override {}
	virtual void add_constant_force(const Vector2 &p_force, const Vector2 &p_position = Vector2()) override {}
	virtual void add_constant_torque(real_t p_torque) override {}

	virtual void set_constant_force(const Vector2 &p_force) override {}
	virtual Vector2 get_constant_force() const override { return Vector2(); }

	virtual void set_constant_torque(real_t p_torque) override {}
	virtual real_t get_constant_torque() const override { return 0; }

	virtual void set_sleep_state(bool p_enable) override {}
	virtual bool is_sleeping() const override { return false; }

	virtual int get_contact_count() const override { return 0; }

	virtual Vector2 get_contact_local_position(int p_contact_idx) const override { return Vector2(); }
	virtual Vector2 get_contact_local_normal(int p_contact_idx) const override { return Vector2(); }
	virtual int get_contact_local_shape(int p_contact_idx) const override { return 0; }
	virtual Vector2 get_contact_local_velocity_at_position(int p_contact_idx) const override { return Vector2(); }

	virtual RID get_contact_collider(int p_contact_idx) const override { return RID(); }
	virtual Vector2 get_contact_collider_position(int p_contact_idx) const override { return Vector2(); }
	virtual ObjectID get_contact_collider_id(int p_contact_idx) const override { return ObjectID(); }
	virtual Object *get_contact_collider_object(int p_contact_idx) const override { return nullptr; }
	virtual int get_contact_collider_shape(int p_contact_idx) const override { return 0; }
	virtual Vector2 get_contact_collider_velocity_at_position(int p_contact_idx) const override { return Vector2(); }
	virtual Vector2 get_contact_impulse(int p_contact_idx) const override { return Vector2(); }

	virtual real_t get_step() const override { return 0; }
	virtual void integrate_forces() override {}

	virtual PhysicsDirectSpaceState2D *get_space_state() override { return space_state_dummy; }

	PhysicsDirectBodyState2DDummy(PhysicsDirectSpaceState2D *p_space_state_dummy) {
		space_state_dummy = p_space_state_dummy;
	}
};

class PhysicsDirectSpaceState2DDummy : public PhysicsDirectSpaceState2D {
	GDCLASS(PhysicsDirectSpaceState2DDummy, PhysicsDirectSpaceState2D);

public:
	virtual bool intersect_ray(const RayParameters &p_parameters, RayResult &r_result) override { return false; }

	virtual int intersect_point(const PointParameters &p_parameters, ShapeResult *r_results, int p_result_max) override { return 0; }

	virtual int intersect_shape(const ShapeParameters &p_parameters, ShapeResult *r_results, int p_result_max) override { return 0; }
	virtual bool cast_motion(const ShapeParameters &p_parameters, real_t &p_closest_safe, real_t &p_closest_unsafe) override { return false; }
	virtual bool collide_shape(const ShapeParameters &p_parameters, Vector2 *r_results, int p_result_max, int &r_result_count) override { return false; }
	virtual bool rest_info(const ShapeParameters &p_parameters, ShapeRestInfo *r_info) override { return false; }
};

class PhysicsServer2DDummy : public PhysicsServer2D {
	GDCLASS(PhysicsServer2DDummy, PhysicsServer2D);

	PhysicsDirectSpaceState2DDummy *space_state_dummy = nullptr;
	PhysicsDirectBodyState2DDummy *body_state_dummy = nullptr;

public:
	virtual RID world_boundary_shape_create() override { return RID(); }
	virtual RID separation_ray_shape_create() override { return RID(); }
	virtual RID segment_shape_create() override { return RID(); }
	virtual RID circle_shape_create() override { return RID(); }
	virtual RID rectangle_shape_create() override { return RID(); }
	virtual RID capsule_shape_create() override { return RID(); }
	virtual RID convex_polygon_shape_create() override { return RID(); }
	virtual RID concave_polygon_shape_create() override { return RID(); }

	virtual void shape_set_data(RID p_shape, const Variant &p_data) override {}
	virtual void shape_set_custom_solver_bias(RID p_shape, real_t p_bias) override {}

	virtual ShapeType shape_get_type(RID p_shape) const override { return ShapeType::SHAPE_CIRCLE; }
	virtual Variant shape_get_data(RID p_shape) const override { return Variant(); }
	virtual real_t shape_get_custom_solver_bias(RID p_shape) const override { return 0; }

	virtual bool shape_collide(RID p_shape_A, const Transform2D &p_xform_A, const Vector2 &p_motion_A, RID p_shape_B, const Transform2D &p_xform_B, const Vector2 &p_motion_B, Vector2 *r_results, int p_result_max, int &r_result_count) override { return false; }

	/* SPACE API */

	virtual RID space_create() override { return RID(); }
	virtual void space_set_active(RID p_space, bool p_active) override {}
	virtual bool space_is_active(RID p_space) const override { return false; }

	virtual void space_set_param(RID p_space, SpaceParameter p_param, real_t p_value) override {}
	virtual real_t space_get_param(RID p_space, SpaceParameter p_param) const override { return 0; }

	virtual PhysicsDirectSpaceState2D *space_get_direct_state(RID p_space) override { return space_state_dummy; }

	virtual void space_set_debug_contacts(RID p_space, int p_max_contacts) override {}
	virtual Vector<Vector2> space_get_contacts(RID p_space) const override { return Vector<Vector2>(); }
	virtual int space_get_contact_count(RID p_space) const override { return 0; }

	/* AREA API */

	virtual RID area_create() override { return RID(); }

	virtual void area_set_space(RID p_area, RID p_space) override {}
	virtual RID area_get_space(RID p_area) const override { return RID(); }

	virtual void area_add_shape(RID p_area, RID p_shape, const Transform2D &p_transform = Transform2D(), bool p_disabled = false) override {}
	virtual void area_set_shape(RID p_area, int p_shape_idx, RID p_shape) override {}
	virtual void area_set_shape_transform(RID p_area, int p_shape_idx, const Transform2D &p_transform) override {}

	virtual int area_get_shape_count(RID p_area) const override { return 0; }
	virtual RID area_get_shape(RID p_area, int p_shape_idx) const override { return RID(); }
	virtual Transform2D area_get_shape_transform(RID p_area, int p_shape_idx) const override { return Transform2D(); }

	virtual void area_remove_shape(RID p_area, int p_shape_idx) override {}
	virtual void area_clear_shapes(RID p_area) override {}

	virtual void area_set_shape_disabled(RID p_area, int p_shape, bool p_disabled) override {}

	virtual void area_attach_object_instance_id(RID p_area, ObjectID p_id) override {}
	virtual ObjectID area_get_object_instance_id(RID p_area) const override { return ObjectID(); }

	virtual void area_attach_canvas_instance_id(RID p_area, ObjectID p_id) override {}
	virtual ObjectID area_get_canvas_instance_id(RID p_area) const override { return ObjectID(); }

	virtual void area_set_param(RID p_area, AreaParameter p_param, const Variant &p_value) override {}
	virtual void area_set_transform(RID p_area, const Transform2D &p_transform) override {}

	virtual Variant area_get_param(RID p_parea, AreaParameter p_param) const override { return Variant(); }
	virtual Transform2D area_get_transform(RID p_area) const override { return Transform2D(); }

	virtual void area_set_collision_layer(RID p_area, uint32_t p_layer) override {}
	virtual uint32_t area_get_collision_layer(RID p_area) const override { return 0; }

	virtual void area_set_collision_mask(RID p_area, uint32_t p_mask) override {}
	virtual uint32_t area_get_collision_mask(RID p_area) const override { return 0; }

	virtual void area_set_monitorable(RID p_area, bool p_monitorable) override {}
	virtual void area_set_pickable(RID p_area, bool p_pickable) override {}

	virtual void area_set_monitor_callback(RID p_area, const Callable &p_callback) override {}
	virtual void area_set_area_monitor_callback(RID p_area, const Callable &p_callback) override {}

	/* BODY API */

	virtual RID body_create() override { return RID(); }

	virtual void body_set_space(RID p_body, RID p_space) override {}
	virtual RID body_get_space(RID p_body) const override { return RID(); }

	virtual void body_set_mode(RID p_body, BodyMode p_mode) override {}
	virtual BodyMode body_get_mode(RID p_body) const override { return BodyMode::BODY_MODE_STATIC; }

	virtual void body_add_shape(RID p_body, RID p_shape, const Transform2D &p_transform = Transform2D(), bool p_disabled = false) override {}
	virtual void body_set_shape(RID p_body, int p_shape_idx, RID p_shape) override {}
	virtual void body_set_shape_transform(RID p_body, int p_shape_idx, const Transform2D &p_transform) override {}

	virtual int body_get_shape_count(RID p_body) const override { return 0; }
	virtual RID body_get_shape(RID p_body, int p_shape_idx) const override { return RID(); }
	virtual Transform2D body_get_shape_transform(RID p_body, int p_shape_idx) const override { return Transform2D(); }

	virtual void body_set_shape_disabled(RID p_body, int p_shape, bool p_disabled) override {}
	virtual void body_set_shape_as_one_way_collision(RID p_body, int p_shape, bool p_enabled, real_t p_margin = 0) override {}

	virtual void body_remove_shape(RID p_body, int p_shape_idx) override {}
	virtual void body_clear_shapes(RID p_body) override {}

	virtual void body_attach_object_instance_id(RID p_body, ObjectID p_id) override {}
	virtual ObjectID body_get_object_instance_id(RID p_body) const override { return ObjectID(); }

	virtual void body_attach_canvas_instance_id(RID p_body, ObjectID p_id) override {}
	virtual ObjectID body_get_canvas_instance_id(RID p_body) const override { return ObjectID(); }

	virtual void body_set_continuous_collision_detection_mode(RID p_body, CCDMode p_mode) override {}
	virtual CCDMode body_get_continuous_collision_detection_mode(RID p_body) const override { return CCDMode::CCD_MODE_DISABLED; }

	virtual void body_set_collision_layer(RID p_body, uint32_t p_layer) override {}
	virtual uint32_t body_get_collision_layer(RID p_body) const override { return 0; }

	virtual void body_set_collision_mask(RID p_body, uint32_t p_mask) override {}
	virtual uint32_t body_get_collision_mask(RID p_body) const override { return 0; }

	virtual void body_set_collision_priority(RID p_body, real_t p_priority) override {}
	virtual real_t body_get_collision_priority(RID p_body) const override { return 0; }

	virtual void body_set_param(RID p_body, BodyParameter p_param, const Variant &p_value) override {}
	virtual Variant body_get_param(RID p_body, BodyParameter p_param) const override { return Variant(); }

	virtual void body_reset_mass_properties(RID p_body) override {}

	virtual void body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) override {}
	virtual Variant body_get_state(RID p_body, BodyState p_state) const override { return Variant(); }

	virtual void body_apply_central_impulse(RID p_body, const Vector2 &p_impulse) override {}
	virtual void body_apply_torque_impulse(RID p_body, real_t p_torque) override {}
	virtual void body_apply_impulse(RID p_body, const Vector2 &p_impulse, const Vector2 &p_position = Vector2()) override {}

	virtual void body_apply_central_force(RID p_body, const Vector2 &p_force) override {}
	virtual void body_apply_force(RID p_body, const Vector2 &p_force, const Vector2 &p_position = Vector2()) override {}
	virtual void body_apply_torque(RID p_body, real_t p_torque) override {}

	virtual void body_add_constant_central_force(RID p_body, const Vector2 &p_force) override {}
	virtual void body_add_constant_force(RID p_body, const Vector2 &p_force, const Vector2 &p_position = Vector2()) override {}
	virtual void body_add_constant_torque(RID p_body, real_t p_torque) override {}

	virtual void body_set_constant_force(RID p_body, const Vector2 &p_force) override {}
	virtual Vector2 body_get_constant_force(RID p_body) const override { return Vector2(); }

	virtual void body_set_constant_torque(RID p_body, real_t p_torque) override {}
	virtual real_t body_get_constant_torque(RID p_body) const override { return 0; }

	virtual void body_set_axis_velocity(RID p_body, const Vector2 &p_axis_velocity) override {}

	virtual void body_add_collision_exception(RID p_body, RID p_body_b) override {}
	virtual void body_remove_collision_exception(RID p_body, RID p_body_b) override {}
	virtual void body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) override {}

	virtual void body_set_max_contacts_reported(RID p_body, int p_contacts) override {}
	virtual int body_get_max_contacts_reported(RID p_body) const override { return 0; }

	virtual void body_set_contacts_reported_depth_threshold(RID p_body, real_t p_threshold) override {}
	virtual real_t body_get_contacts_reported_depth_threshold(RID p_body) const override { return 0; }

	virtual void body_set_omit_force_integration(RID p_body, bool p_omit) override {}
	virtual bool body_is_omitting_force_integration(RID p_body) const override { return false; }

	virtual void body_set_state_sync_callback(RID p_body, const Callable &p_callable) override {}
	virtual void body_set_force_integration_callback(RID p_body, const Callable &p_callable, const Variant &p_udata = Variant()) override {}

	virtual bool body_collide_shape(RID p_body, int p_body_shape, RID p_shape, const Transform2D &p_shape_xform, const Vector2 &p_motion, Vector2 *r_results, int p_result_max, int &r_result_count) override { return false; }

	virtual void body_set_pickable(RID p_body, bool p_pickable) override {}

	virtual PhysicsDirectBodyState2D *body_get_direct_state(RID p_body) override { return body_state_dummy; }

	virtual bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) override { return false; }

	/* JOINT API */

	virtual RID joint_create() override { return RID(); }

	virtual void joint_clear(RID p_joint) override {}

	virtual void joint_set_param(RID p_joint, JointParam p_param, real_t p_value) override {}
	virtual real_t joint_get_param(RID p_joint, JointParam p_param) const override { return 0; }

	virtual void joint_disable_collisions_between_bodies(RID p_joint, const bool p_disable) override {}
	virtual bool joint_is_disabled_collisions_between_bodies(RID p_joint) const override { return false; }

	virtual void joint_make_pin(RID p_joint, const Vector2 &p_anchor, RID p_body_a, RID p_body_b = RID()) override {}
	virtual void joint_make_groove(RID p_joint, const Vector2 &p_a_groove1, const Vector2 &p_a_groove2, const Vector2 &p_b_anchor, RID p_body_a, RID p_body_b) override {}
	virtual void joint_make_damped_spring(RID p_joint, const Vector2 &p_anchor_a, const Vector2 &p_anchor_b, RID p_body_a, RID p_body_b = RID()) override {}

	virtual void pin_joint_set_param(RID p_joint, PinJointParam p_param, real_t p_value) override {}
	virtual real_t pin_joint_get_param(RID p_joint, PinJointParam p_param) const override { return 0; }

	virtual void pin_joint_set_flag(RID p_joint, PinJointFlag p_flag, bool p_enabled) override {}
	virtual bool pin_joint_get_flag(RID p_joint, PinJointFlag p_flag) const override { return false; }

	virtual void damped_spring_joint_set_param(RID p_joint, DampedSpringParam p_param, real_t p_value) override {}
	virtual real_t damped_spring_joint_get_param(RID p_joint, DampedSpringParam p_param) const override { return 0; }

	virtual JointType joint_get_type(RID p_joint) const override { return JointType::JOINT_TYPE_PIN; }

	/* MISC */

	virtual void free(RID p_rid) override {}

	virtual void set_active(bool p_active) override {}
	virtual void init() override {
		space_state_dummy = memnew(PhysicsDirectSpaceState2DDummy);
		body_state_dummy = memnew(PhysicsDirectBodyState2DDummy(space_state_dummy));
	}
	virtual void step(real_t p_step) override {}
	virtual void sync() override {}
	virtual void flush_queries() override {}
	virtual void end_sync() override {}
	virtual void finish() override {
		memdelete(body_state_dummy);
		memdelete(space_state_dummy);
	}

	virtual bool is_flushing_queries() const override { return false; }

	virtual int get_process_info(ProcessInfo p_info) override { return 0; }
};
