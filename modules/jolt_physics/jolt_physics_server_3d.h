/**************************************************************************/
/*  jolt_physics_server_3d.h                                              */
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

#ifndef JOLT_PHYSICS_SERVER_3D_H
#define JOLT_PHYSICS_SERVER_3D_H

#include "core/templates/rid_owner.h"
#include "servers/physics_server_3d.h"

class JoltArea3D;
class JoltBody3D;
class JoltJobSystem;
class JoltJoint3D;
class JoltShape3D;
class JoltSoftBody3D;
class JoltSpace3D;

class JoltPhysicsServer3D final : public PhysicsServer3D {
	GDCLASS(JoltPhysicsServer3D, PhysicsServer3D)

	inline static JoltPhysicsServer3D *singleton = nullptr;

	mutable RID_PtrOwner<JoltSpace3D, true> space_owner;
	mutable RID_PtrOwner<JoltArea3D, true> area_owner;
	mutable RID_PtrOwner<JoltBody3D, true> body_owner;
	mutable RID_PtrOwner<JoltSoftBody3D, true> soft_body_owner;
	mutable RID_PtrOwner<JoltShape3D, true> shape_owner;
	mutable RID_PtrOwner<JoltJoint3D, true> joint_owner;

	HashSet<JoltSpace3D *> active_spaces;

	JoltJobSystem *job_system = nullptr;

	bool on_separate_thread = false;
	bool active = true;
	bool flushing_queries = false;
	bool doing_sync = false;

public:
	enum HingeJointParamJolt {
		HINGE_JOINT_LIMIT_SPRING_FREQUENCY = 100,
		HINGE_JOINT_LIMIT_SPRING_DAMPING,
		HINGE_JOINT_MOTOR_MAX_TORQUE,
	};

	enum HingeJointFlagJolt {
		HINGE_JOINT_FLAG_USE_LIMIT_SPRING = 100,
	};

	enum SliderJointParamJolt {
		SLIDER_JOINT_LIMIT_SPRING_FREQUENCY = 100,
		SLIDER_JOINT_LIMIT_SPRING_DAMPING,
		SLIDER_JOINT_MOTOR_TARGET_VELOCITY,
		SLIDER_JOINT_MOTOR_MAX_FORCE,
	};

	enum SliderJointFlagJolt {
		SLIDER_JOINT_FLAG_USE_LIMIT = 100,
		SLIDER_JOINT_FLAG_USE_LIMIT_SPRING,
		SLIDER_JOINT_FLAG_ENABLE_MOTOR,
	};

	enum ConeTwistJointParamJolt {
		CONE_TWIST_JOINT_SWING_MOTOR_TARGET_VELOCITY_Y = 100,
		CONE_TWIST_JOINT_SWING_MOTOR_TARGET_VELOCITY_Z,
		CONE_TWIST_JOINT_TWIST_MOTOR_TARGET_VELOCITY,
		CONE_TWIST_JOINT_SWING_MOTOR_MAX_TORQUE,
		CONE_TWIST_JOINT_TWIST_MOTOR_MAX_TORQUE,
	};

	enum ConeTwistJointFlagJolt {
		CONE_TWIST_JOINT_FLAG_USE_SWING_LIMIT = 100,
		CONE_TWIST_JOINT_FLAG_USE_TWIST_LIMIT,
		CONE_TWIST_JOINT_FLAG_ENABLE_SWING_MOTOR,
		CONE_TWIST_JOINT_FLAG_ENABLE_TWIST_MOTOR,
	};

	enum G6DOFJointAxisParamJolt {
		G6DOF_JOINT_LINEAR_SPRING_FREQUENCY = 100,
		G6DOF_JOINT_LINEAR_LIMIT_SPRING_FREQUENCY,
		G6DOF_JOINT_LINEAR_LIMIT_SPRING_DAMPING,
		G6DOF_JOINT_ANGULAR_SPRING_FREQUENCY,
		G6DOF_JOINT_LINEAR_SPRING_MAX_FORCE,
		G6DOF_JOINT_ANGULAR_SPRING_MAX_TORQUE,
	};

	enum G6DOFJointAxisFlagJolt {
		G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT_SPRING = 100,
		G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING_FREQUENCY,
		G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING_FREQUENCY,
	};

private:
	static void _bind_methods() {}

public:
	explicit JoltPhysicsServer3D(bool p_on_separate_thread);
	~JoltPhysicsServer3D();

	static JoltPhysicsServer3D *get_singleton() { return singleton; }

	virtual RID world_boundary_shape_create() override;
	virtual RID separation_ray_shape_create() override;
	virtual RID sphere_shape_create() override;
	virtual RID box_shape_create() override;
	virtual RID capsule_shape_create() override;
	virtual RID cylinder_shape_create() override;
	virtual RID convex_polygon_shape_create() override;
	virtual RID concave_polygon_shape_create() override;
	virtual RID heightmap_shape_create() override;
	virtual RID custom_shape_create() override;

	virtual void shape_set_data(RID p_shape, const Variant &p_data) override;
	virtual Variant shape_get_data(RID p_shape) const override;

	virtual void shape_set_custom_solver_bias(RID p_shape, real_t p_bias) override;

	virtual void shape_set_margin(RID p_shape, real_t p_margin) override;
	virtual real_t shape_get_margin(RID p_shape) const override;

	virtual PhysicsServer3D::ShapeType shape_get_type(RID p_shape) const override;

	virtual real_t shape_get_custom_solver_bias(RID p_shape) const override;

	virtual RID space_create() override;

	virtual void space_set_active(RID p_space, bool p_active) override;
	virtual bool space_is_active(RID p_space) const override;

	virtual void space_set_param(RID p_space, PhysicsServer3D::SpaceParameter p_param, real_t p_value) override;
	virtual real_t space_get_param(RID p_space, PhysicsServer3D::SpaceParameter p_param) const override;

	virtual PhysicsDirectSpaceState3D *space_get_direct_state(RID p_space) override;

	virtual void space_set_debug_contacts(RID p_space, int p_max_contacts) override;
	virtual PackedVector3Array space_get_contacts(RID p_space) const override;
	virtual int space_get_contact_count(RID p_space) const override;

	virtual RID area_create() override;

	virtual void area_set_space(RID p_area, RID p_space) override;
	virtual RID area_get_space(RID p_area) const override;

	virtual void area_add_shape(RID p_area, RID p_shape, const Transform3D &p_transform, bool p_disabled) override;

	virtual void area_set_shape(RID p_area, int p_shape_idx, RID p_shape) override;
	virtual RID area_get_shape(RID p_area, int p_shape_idx) const override;

	virtual void area_set_shape_transform(RID p_area, int p_shape_idx, const Transform3D &p_transform) override;
	virtual Transform3D area_get_shape_transform(RID p_area, int p_shape_idx) const override;

	virtual void area_set_shape_disabled(RID p_area, int p_shape_idx, bool p_disabled) override;

	virtual int area_get_shape_count(RID p_area) const override;

	virtual void area_remove_shape(RID p_area, int p_shape_idx) override;
	virtual void area_clear_shapes(RID p_area) override;

	virtual void area_attach_object_instance_id(RID p_area, ObjectID p_id) override;
	virtual ObjectID area_get_object_instance_id(RID p_area) const override;

	virtual void area_set_param(RID p_area, PhysicsServer3D::AreaParameter p_param, const Variant &p_value) override;
	virtual Variant area_get_param(RID p_area, PhysicsServer3D::AreaParameter p_param) const override;

	virtual void area_set_transform(RID p_area, const Transform3D &p_transform) override;
	virtual Transform3D area_get_transform(RID p_area) const override;

	virtual void area_set_collision_layer(RID p_area, uint32_t p_layer) override;
	virtual uint32_t area_get_collision_layer(RID p_area) const override;

	virtual void area_set_collision_mask(RID p_area, uint32_t p_mask) override;
	virtual uint32_t area_get_collision_mask(RID p_area) const override;

	virtual void area_set_monitorable(RID p_area, bool p_monitorable) override;

	virtual void area_set_ray_pickable(RID p_area, bool p_enable) override;

	virtual void area_set_monitor_callback(RID p_area, const Callable &p_callback) override;
	virtual void area_set_area_monitor_callback(RID p_area, const Callable &p_callback) override;

	virtual RID body_create() override;

	virtual void body_set_space(RID p_body, RID p_space) override;
	virtual RID body_get_space(RID p_body) const override;

	virtual void body_set_mode(RID p_body, PhysicsServer3D::BodyMode p_mode) override;
	virtual PhysicsServer3D::BodyMode body_get_mode(RID p_body) const override;

	virtual void body_add_shape(RID p_body, RID p_shape, const Transform3D &p_transform, bool p_disabled) override;

	virtual void body_set_shape(RID p_body, int p_shape_idx, RID p_shape) override;
	virtual RID body_get_shape(RID p_body, int p_shape_idx) const override;

	virtual void body_set_shape_transform(RID p_body, int p_shape_idx, const Transform3D &p_transform) override;
	virtual Transform3D body_get_shape_transform(RID p_body, int p_shape_idx) const override;

	virtual void body_set_shape_disabled(RID p_body, int p_shape_idx, bool p_disabled) override;

	virtual int body_get_shape_count(RID p_body) const override;

	virtual void body_remove_shape(RID p_body, int p_shape_idx) override;
	virtual void body_clear_shapes(RID p_body) override;

	virtual void body_attach_object_instance_id(RID p_body, ObjectID p_id) override;
	virtual ObjectID body_get_object_instance_id(RID p_body) const override;

	virtual void body_set_enable_continuous_collision_detection(RID p_body, bool p_enable) override;
	virtual bool body_is_continuous_collision_detection_enabled(RID p_body) const override;

	virtual void body_set_collision_layer(RID p_body, uint32_t p_layer) override;
	virtual uint32_t body_get_collision_layer(RID p_body) const override;

	virtual void body_set_collision_mask(RID p_body, uint32_t p_mask) override;
	virtual uint32_t body_get_collision_mask(RID p_body) const override;

	virtual void body_set_collision_priority(RID p_body, real_t p_priority) override;
	virtual real_t body_get_collision_priority(RID p_body) const override;

	virtual void body_set_user_flags(RID p_body, uint32_t p_flags) override;
	virtual uint32_t body_get_user_flags(RID p_body) const override;

	virtual void body_set_param(RID p_body, PhysicsServer3D::BodyParameter p_param, const Variant &p_value) override;
	virtual Variant body_get_param(RID p_body, PhysicsServer3D::BodyParameter p_param) const override;

	virtual void body_reset_mass_properties(RID p_body) override;

	virtual void body_set_state(RID p_body, PhysicsServer3D::BodyState p_state, const Variant &p_value) override;
	virtual Variant body_get_state(RID p_body, PhysicsServer3D::BodyState p_state) const override;

	virtual void body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) override;
	virtual void body_apply_impulse(RID p_body, const Vector3 &p_impulse, const Vector3 &p_position) override;
	virtual void body_apply_torque_impulse(RID p_body, const Vector3 &p_impulse) override;

	virtual void body_apply_central_force(RID p_body, const Vector3 &p_force) override;
	virtual void body_apply_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position) override;
	virtual void body_apply_torque(RID p_body, const Vector3 &p_torque) override;

	virtual void body_add_constant_central_force(RID p_body, const Vector3 &p_force) override;
	virtual void body_add_constant_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position) override;
	virtual void body_add_constant_torque(RID p_body, const Vector3 &p_torque) override;

	virtual void body_set_constant_force(RID p_body, const Vector3 &p_force) override;
	virtual Vector3 body_get_constant_force(RID p_body) const override;

	virtual void body_set_constant_torque(RID p_body, const Vector3 &p_torque) override;
	virtual Vector3 body_get_constant_torque(RID p_body) const override;

	virtual void body_set_axis_velocity(RID p_body, const Vector3 &p_axis_velocity) override;

	virtual void body_set_axis_lock(RID p_body, PhysicsServer3D::BodyAxis p_axis, bool p_lock) override;
	virtual bool body_is_axis_locked(RID p_body, PhysicsServer3D::BodyAxis p_axis) const override;

	virtual void body_add_collision_exception(RID p_body, RID p_excepted_body) override;
	virtual void body_remove_collision_exception(RID p_body, RID p_excepted_body) override;
	virtual void body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) override;

	virtual void body_set_max_contacts_reported(RID p_body, int p_amount) override;
	virtual int body_get_max_contacts_reported(RID p_body) const override;

	virtual void body_set_contacts_reported_depth_threshold(RID p_body, real_t p_threshold) override;
	virtual real_t body_get_contacts_reported_depth_threshold(RID p_body) const override;

	virtual void body_set_omit_force_integration(RID p_body, bool p_enable) override;
	virtual bool body_is_omitting_force_integration(RID p_body) const override;

	virtual void body_set_state_sync_callback(RID p_body, const Callable &p_callable) override;
	virtual void body_set_force_integration_callback(RID p_body, const Callable &p_callable, const Variant &p_userdata) override;

	virtual void body_set_ray_pickable(RID p_body, bool p_enable) override;

	virtual bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result) override;

	virtual PhysicsDirectBodyState3D *body_get_direct_state(RID p_body) override;

	virtual RID soft_body_create() override;

	virtual void soft_body_update_rendering_server(RID p_body, PhysicsServer3DRenderingServerHandler *p_rendering_server_handler) override;

	virtual void soft_body_set_space(RID p_body, RID p_space) override;
	virtual RID soft_body_get_space(RID p_body) const override;

	virtual void soft_body_set_ray_pickable(RID p_body, bool p_enable) override;

	virtual void soft_body_set_collision_layer(RID p_body, uint32_t p_layer) override;
	virtual uint32_t soft_body_get_collision_layer(RID p_body) const override;

	virtual void soft_body_set_collision_mask(RID p_body, uint32_t p_mask) override;
	virtual uint32_t soft_body_get_collision_mask(RID p_body) const override;

	virtual void soft_body_add_collision_exception(RID p_body, RID p_excepted_body) override;
	virtual void soft_body_remove_collision_exception(RID p_body, RID p_excepted_body) override;
	virtual void soft_body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) override;

	virtual void soft_body_set_state(RID p_body, PhysicsServer3D::BodyState p_state, const Variant &p_value) override;
	virtual Variant soft_body_get_state(RID p_body, PhysicsServer3D::BodyState p_state) const override;

	virtual void soft_body_set_transform(RID p_body, const Transform3D &p_transform) override;

	virtual void soft_body_set_simulation_precision(RID p_body, int p_precision) override;
	virtual int soft_body_get_simulation_precision(RID p_body) const override;

	virtual void soft_body_set_total_mass(RID p_body, real_t p_total_mass) override;
	virtual real_t soft_body_get_total_mass(RID p_body) const override;

	virtual void soft_body_set_linear_stiffness(RID p_body, real_t p_coefficient) override;
	virtual real_t soft_body_get_linear_stiffness(RID p_body) const override;

	virtual void soft_body_set_pressure_coefficient(RID p_body, real_t p_coefficient) override;
	virtual real_t soft_body_get_pressure_coefficient(RID p_body) const override;

	virtual void soft_body_set_damping_coefficient(RID p_body, real_t p_coefficient) override;
	virtual real_t soft_body_get_damping_coefficient(RID p_body) const override;

	virtual void soft_body_set_drag_coefficient(RID p_body, real_t p_coefficient) override;
	virtual real_t soft_body_get_drag_coefficient(RID p_body) const override;

	virtual void soft_body_set_mesh(RID p_body, RID p_mesh) override;

	virtual AABB soft_body_get_bounds(RID p_body) const override;

	virtual void soft_body_move_point(RID p_body, int p_point_index, const Vector3 &p_global_position) override;

	virtual Vector3 soft_body_get_point_global_position(RID p_body, int p_point_index) const override;

	virtual void soft_body_remove_all_pinned_points(RID p_body) override;

	virtual void soft_body_pin_point(RID p_body, int p_point_index, bool p_pin) override;
	virtual bool soft_body_is_point_pinned(RID p_body, int p_point_index) const override;

	virtual RID joint_create() override;
	virtual void joint_clear(RID p_joint) override;

	virtual void joint_make_pin(RID p_joint, RID p_body_a, const Vector3 &p_local_a, RID p_body_b, const Vector3 &p_local_b) override;

	virtual void pin_joint_set_param(RID p_joint, PhysicsServer3D::PinJointParam p_param, real_t p_value) override;
	virtual real_t pin_joint_get_param(RID p_joint, PhysicsServer3D::PinJointParam p_param) const override;

	virtual void pin_joint_set_local_a(RID p_joint, const Vector3 &p_local_a) override;
	virtual Vector3 pin_joint_get_local_a(RID p_joint) const override;

	virtual void pin_joint_set_local_b(RID p_joint, const Vector3 &p_local_b) override;
	virtual Vector3 pin_joint_get_local_b(RID p_joint) const override;

	virtual void joint_make_hinge(RID p_joint, RID p_body_a, const Transform3D &p_hinge_a, RID p_body_b, const Transform3D &p_hinge_b) override;

	virtual void joint_make_hinge_simple(RID p_joint, RID p_body_a, const Vector3 &p_pivot_a, const Vector3 &p_axis_a, RID p_body_b, const Vector3 &p_pivot_b, const Vector3 &p_axis_b) override;

	virtual void hinge_joint_set_param(RID p_joint, PhysicsServer3D::HingeJointParam p_param, real_t p_value) override;
	virtual real_t hinge_joint_get_param(RID p_joint, PhysicsServer3D::HingeJointParam p_param) const override;

	virtual void hinge_joint_set_flag(RID p_joint, PhysicsServer3D::HingeJointFlag p_flag, bool p_enabled) override;
	virtual bool hinge_joint_get_flag(RID p_joint, PhysicsServer3D::HingeJointFlag p_flag) const override;

	virtual void joint_make_slider(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) override;

	virtual void slider_joint_set_param(RID p_joint, PhysicsServer3D::SliderJointParam p_param, real_t p_value) override;
	virtual real_t slider_joint_get_param(RID p_joint, PhysicsServer3D::SliderJointParam p_param) const override;

	virtual void joint_make_cone_twist(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) override;

	virtual void cone_twist_joint_set_param(RID p_joint, PhysicsServer3D::ConeTwistJointParam p_param, real_t p_value) override;
	virtual real_t cone_twist_joint_get_param(RID p_joint, PhysicsServer3D::ConeTwistJointParam p_param) const override;

	virtual void joint_make_generic_6dof(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) override;

	virtual void generic_6dof_joint_set_param(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param, real_t p_value) override;
	virtual real_t generic_6dof_joint_get_param(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param) const override;

	virtual void generic_6dof_joint_set_flag(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag, bool p_enable) override;
	virtual bool generic_6dof_joint_get_flag(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag) const override;

	virtual PhysicsServer3D::JointType joint_get_type(RID p_joint) const override;

	virtual void joint_set_solver_priority(RID p_joint, int p_priority) override;
	virtual int joint_get_solver_priority(RID p_joint) const override;

	virtual void joint_disable_collisions_between_bodies(RID p_joint, bool p_disable) override;
	virtual bool joint_is_disabled_collisions_between_bodies(RID p_joint) const override;

	virtual void free(RID p_rid) override;

	virtual void set_active(bool p_active) override;

	virtual void init() override;
	virtual void finish() override;

	virtual void step(real_t p_step) override;

	virtual void sync() override;
	virtual void end_sync() override;

	virtual void flush_queries() override;
	virtual bool is_flushing_queries() const override;

	virtual int get_process_info(PhysicsServer3D::ProcessInfo p_process_info) override;

	bool is_active() const { return active; }

	void free_space(JoltSpace3D *p_space);
	void free_area(JoltArea3D *p_area);
	void free_body(JoltBody3D *p_body);
	void free_soft_body(JoltSoftBody3D *p_body);
	void free_shape(JoltShape3D *p_shape);
	void free_joint(JoltJoint3D *p_joint);

	JoltSpace3D *get_space(RID p_rid) const { return space_owner.get_or_null(p_rid); }
	JoltArea3D *get_area(RID p_rid) const { return area_owner.get_or_null(p_rid); }
	JoltBody3D *get_body(RID p_rid) const { return body_owner.get_or_null(p_rid); }
	JoltShape3D *get_shape(RID p_rid) const { return shape_owner.get_or_null(p_rid); }
	JoltJoint3D *get_joint(RID p_rid) const { return joint_owner.get_or_null(p_rid); }

#ifdef DEBUG_ENABLED
	void dump_debug_snapshots(const String &p_dir);

	void space_dump_debug_snapshot(RID p_space, const String &p_dir);
#endif

	bool joint_get_enabled(RID p_joint) const;
	void joint_set_enabled(RID p_joint, bool p_enabled);

	int joint_get_solver_velocity_iterations(RID p_joint);
	void joint_set_solver_velocity_iterations(RID p_joint, int p_value);

	int joint_get_solver_position_iterations(RID p_joint);
	void joint_set_solver_position_iterations(RID p_joint, int p_value);

	float pin_joint_get_applied_force(RID p_joint);

	double hinge_joint_get_jolt_param(RID p_joint, HingeJointParamJolt p_param) const;
	void hinge_joint_set_jolt_param(RID p_joint, HingeJointParamJolt p_param, double p_value);

	bool hinge_joint_get_jolt_flag(RID p_joint, HingeJointFlagJolt p_flag) const;
	void hinge_joint_set_jolt_flag(RID p_joint, HingeJointFlagJolt p_flag, bool p_enabled);

	float hinge_joint_get_applied_force(RID p_joint);
	float hinge_joint_get_applied_torque(RID p_joint);

	double slider_joint_get_jolt_param(RID p_joint, SliderJointParamJolt p_param) const;
	void slider_joint_set_jolt_param(RID p_joint, SliderJointParamJolt p_param, double p_value);

	bool slider_joint_get_jolt_flag(RID p_joint, SliderJointFlagJolt p_flag) const;
	void slider_joint_set_jolt_flag(RID p_joint, SliderJointFlagJolt p_flag, bool p_enabled);

	float slider_joint_get_applied_force(RID p_joint);
	float slider_joint_get_applied_torque(RID p_joint);

	double cone_twist_joint_get_jolt_param(RID p_joint, ConeTwistJointParamJolt p_param) const;
	void cone_twist_joint_set_jolt_param(RID p_joint, ConeTwistJointParamJolt p_param, double p_value);

	bool cone_twist_joint_get_jolt_flag(RID p_joint, ConeTwistJointFlagJolt p_flag) const;
	void cone_twist_joint_set_jolt_flag(RID p_joint, ConeTwistJointFlagJolt p_flag, bool p_enabled);

	float cone_twist_joint_get_applied_force(RID p_joint);
	float cone_twist_joint_get_applied_torque(RID p_joint);

	double generic_6dof_joint_get_jolt_param(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisParamJolt p_param) const;
	void generic_6dof_joint_set_jolt_param(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisParamJolt p_param, double p_value);

	bool generic_6dof_joint_get_jolt_flag(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisFlagJolt p_flag) const;
	void generic_6dof_joint_set_jolt_flag(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisFlagJolt p_flag, bool p_enabled);

	float generic_6dof_joint_get_applied_force(RID p_joint);
	float generic_6dof_joint_get_applied_torque(RID p_joint);
};

VARIANT_ENUM_CAST(JoltPhysicsServer3D::HingeJointParamJolt)
VARIANT_ENUM_CAST(JoltPhysicsServer3D::HingeJointFlagJolt)
VARIANT_ENUM_CAST(JoltPhysicsServer3D::SliderJointParamJolt)
VARIANT_ENUM_CAST(JoltPhysicsServer3D::SliderJointFlagJolt)
VARIANT_ENUM_CAST(JoltPhysicsServer3D::ConeTwistJointParamJolt)
VARIANT_ENUM_CAST(JoltPhysicsServer3D::ConeTwistJointFlagJolt)
VARIANT_ENUM_CAST(JoltPhysicsServer3D::G6DOFJointAxisParamJolt)
VARIANT_ENUM_CAST(JoltPhysicsServer3D::G6DOFJointAxisFlagJolt)

#endif // JOLT_PHYSICS_SERVER_3D_H
