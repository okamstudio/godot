/**************************************************************************/
/*  godot_physics_server_3d.h                                             */
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

#ifndef GODOT_PHYSICS_SERVER_3D_H
#define GODOT_PHYSICS_SERVER_3D_H

#include "godot_joint_3d.h"
#include "godot_shape_3d.h"
#include "godot_space_3d.h"
#include "godot_step_3d.h"

#include "core/templates/rid_owner.h"
#include "servers/physics_server_3d.h"

class GodotPhysicsServer3D : public PhysicsServer3D {
	GDCLASS(GodotPhysicsServer3D, PhysicsServer3D);

	friend class GodotPhysicsDirectSpaceState3D;
	bool active = true;

	int island_count = 0;
	int active_objects = 0;
	int collision_pairs = 0;

	bool using_threads = false;
	bool doing_sync = false;
	bool flushing_queries = false;

	GodotStep3D *stepper = nullptr;
	HashSet<GodotSpace3D *> active_spaces;

	mutable RID_PtrOwner<GodotShape3D, true> shape_owner;
	mutable RID_PtrOwner<GodotSpace3D, true> space_owner;
	mutable RID_PtrOwner<GodotArea3D, true> area_owner;
	mutable RID_PtrOwner<GodotBody3D, true> body_owner;
	mutable RID_PtrOwner<GodotSoftBody3D, true> soft_body_owner;
	mutable RID_PtrOwner<GodotJoint3D, true> joint_owner;

	//void _clear_query(QuerySW *p_query);
	friend class GodotCollisionObject3D;
	SelfList<GodotCollisionObject3D>::List pending_shape_update_list;
	void _update_shapes();

	static GodotPhysicsServer3D *godot_singleton;

public:
	struct CollCbkData {
		int max;
		int amount;
		Vector3 *ptr = nullptr;
	};

	static void _shape_col_cbk(const Vector3 &p_point_A, int p_index_A, const Vector3 &p_point_B, int p_index_B, const Vector3 &normal, void *p_userdata);

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
	virtual void shape_set_custom_solver_bias(RID p_shape, real_t p_bias) override;

	virtual ShapeType shape_get_type(RID p_shape) const override;
	virtual Variant shape_get_data(RID p_shape) const override;

	virtual void shape_set_margin(RID p_shape, real_t p_margin) override;
	virtual real_t shape_get_margin(RID p_shape) const override;

	virtual real_t shape_get_custom_solver_bias(RID p_shape) const override;

	/* SPACE API */

	virtual RID space_create() override;
	virtual void space_set_active(RID p_space, bool p_active) override;
	virtual bool space_is_active(RID p_space) const override;

	virtual void space_set_param(RID p_space, SpaceParameter p_param, real_t p_value) override;
	virtual real_t space_get_param(RID p_space, SpaceParameter p_param) const override;

	// this function only works on physics process, errors and returns null otherwise
	virtual PhysicsDirectSpaceState3D *space_get_direct_state(RID p_space) override;

	virtual void space_set_debug_contacts(RID p_space, int p_max_contacts) override;
	virtual Vector<Vector3> space_get_contacts(RID p_space) const override;
	virtual int space_get_contact_count(RID p_space) const override;

	/* AREA API */

	virtual RID area_create() override;

	virtual void area_set_space(RID p_area, RID p_space) override;
	virtual RID area_get_space(RID p_area) const override;

	virtual void area_add_shape(RID p_area, RID p_shape, const Transform3D &p_transform = Transform3D(), bool p_disabled = false) override;
	virtual void area_set_shape(RID p_area, int p_shape_idx, RID p_shape) override;
	virtual void area_set_shape_transform(RID p_area, int p_shape_idx, const Transform3D &p_transform) override;

	virtual int area_get_shape_count(RID p_area) const override;
	virtual RID area_get_shape(RID p_area, int p_shape_idx) const override;
	virtual Transform3D area_get_shape_transform(RID p_area, int p_shape_idx) const override;

	virtual void area_remove_shape(RID p_area, int p_shape_idx) override;
	virtual void area_clear_shapes(RID p_area) override;

	virtual void area_set_shape_disabled(RID p_area, int p_shape_idx, bool p_disabled) override;

	virtual void area_attach_object_instance_id(RID p_area, ObjectID p_id) override;
	virtual ObjectID area_get_object_instance_id(RID p_area) const override;

	virtual void area_set_param(RID p_area, AreaParameter p_param, const Variant &p_value) override;
	virtual void area_set_transform(RID p_area, const Transform3D &p_transform) override;

	virtual Variant area_get_param(RID p_area, AreaParameter p_param) const override;
	virtual Transform3D area_get_transform(RID p_area) const override;

	virtual void area_set_ray_pickable(RID p_area, bool p_enable) override;

	virtual void area_set_collision_layer(RID p_area, uint32_t p_layer) override;
	virtual uint32_t area_get_collision_layer(RID p_area) const override;

	virtual void area_set_collision_mask(RID p_area, uint32_t p_mask) override;
	virtual uint32_t area_get_collision_mask(RID p_area) const override;

	virtual void area_set_monitorable(RID p_area, bool p_monitorable) override;

	virtual void area_set_monitor_callback(RID p_area, const Callable &p_callback) override;
	virtual void area_set_area_monitor_callback(RID p_area, const Callable &p_callback) override;

	/* BODY API */

	// create a body of a given type
	virtual RID body_create() override;

	virtual void body_set_space(RID p_body, RID p_space) override;
	virtual RID body_get_space(RID p_body) const override;

	virtual void body_set_mode(RID p_body, BodyMode p_mode) override;
	virtual BodyMode body_get_mode(RID p_body) const override;

	virtual void body_add_shape(RID p_body, RID p_shape, const Transform3D &p_transform = Transform3D(), bool p_disabled = false) override;
	virtual void body_set_shape(RID p_body, int p_shape_idx, RID p_shape) override;
	virtual void body_set_shape_transform(RID p_body, int p_shape_idx, const Transform3D &p_transform) override;

	virtual int body_get_shape_count(RID p_body) const override;
	virtual RID body_get_shape(RID p_body, int p_shape_idx) const override;
	virtual Transform3D body_get_shape_transform(RID p_body, int p_shape_idx) const override;

	virtual void body_set_shape_disabled(RID p_body, int p_shape_idx, bool p_disabled) override;

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

	virtual void body_set_param(RID p_body, BodyParameter p_param, const Variant &p_value) override;
	virtual Variant body_get_param(RID p_body, BodyParameter p_param) const override;

	virtual void body_reset_mass_properties(RID p_body) override;

	virtual void body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) override;
	virtual Variant body_get_state(RID p_body, BodyState p_state) const override;

	virtual void body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) override;
	virtual void body_apply_impulse(RID p_body, const Vector3 &p_impulse, const Vector3 &p_position = Vector3()) override;
	virtual void body_apply_torque_impulse(RID p_body, const Vector3 &p_impulse) override;

	virtual void body_apply_central_force(RID p_body, const Vector3 &p_force) override;
	virtual void body_apply_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position = Vector3()) override;
	virtual void body_apply_torque(RID p_body, const Vector3 &p_torque) override;

	virtual void body_add_constant_central_force(RID p_body, const Vector3 &p_force) override;
	virtual void body_add_constant_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position = Vector3()) override;
	virtual void body_add_constant_torque(RID p_body, const Vector3 &p_torque) override;

	virtual void body_set_constant_force(RID p_body, const Vector3 &p_force) override;
	virtual Vector3 body_get_constant_force(RID p_body) const override;

	virtual void body_set_constant_torque(RID p_body, const Vector3 &p_torque) override;
	virtual Vector3 body_get_constant_torque(RID p_body) const override;

	virtual void body_set_axis_velocity(RID p_body, const Vector3 &p_axis_velocity) override;

	virtual void body_set_axis_lock(RID p_body, BodyAxis p_axis, bool p_lock) override;
	virtual bool body_is_axis_locked(RID p_body, BodyAxis p_axis) const override;

	virtual void body_add_collision_exception(RID p_body, RID p_body_b) override;
	virtual void body_remove_collision_exception(RID p_body, RID p_body_b) override;
	virtual void body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) override;

	virtual void body_set_contacts_reported_depth_threshold(RID p_body, real_t p_threshold) override;
	virtual real_t body_get_contacts_reported_depth_threshold(RID p_body) const override;

	virtual void body_set_omit_force_integration(RID p_body, bool p_omit) override;
	virtual bool body_is_omitting_force_integration(RID p_body) const override;

	virtual void body_set_max_contacts_reported(RID p_body, int p_contacts) override;
	virtual int body_get_max_contacts_reported(RID p_body) const override;

	virtual void body_set_state_sync_callback(RID p_body, const Callable &p_callable) override;
	virtual void body_set_force_integration_callback(RID p_body, const Callable &p_callable, const Variant &p_udata = Variant()) override;

	virtual void body_set_ray_pickable(RID p_body, bool p_enable) override;

	virtual bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) override;

	// this function only works on physics process, errors and returns null otherwise
	virtual PhysicsDirectBodyState3D *body_get_direct_state(RID p_body) override;

	/* SOFT BODY */

	virtual RID soft_body_create() override;

	virtual void soft_body_update_rendering_server(RID p_body, PhysicsServer3DRenderingServerHandler *p_rendering_server_handler) override;

	virtual void soft_body_set_space(RID p_body, RID p_space) override;
	virtual RID soft_body_get_space(RID p_body) const override;

	virtual void soft_body_set_collision_layer(RID p_body, uint32_t p_layer) override;
	virtual uint32_t soft_body_get_collision_layer(RID p_body) const override;

	virtual void soft_body_set_collision_mask(RID p_body, uint32_t p_mask) override;
	virtual uint32_t soft_body_get_collision_mask(RID p_body) const override;

	virtual void soft_body_add_collision_exception(RID p_body, RID p_body_b) override;
	virtual void soft_body_remove_collision_exception(RID p_body, RID p_body_b) override;
	virtual void soft_body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) override;

	virtual void soft_body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) override;
	virtual Variant soft_body_get_state(RID p_body, BodyState p_state) const override;

	virtual void soft_body_set_transform(RID p_body, const Transform3D &p_transform) override;

	virtual void soft_body_set_ray_pickable(RID p_body, bool p_enable) override;

	virtual void soft_body_set_simulation_precision(RID p_body, int p_simulation_precision) override;
	virtual int soft_body_get_simulation_precision(RID p_body) const override;

	virtual void soft_body_set_total_mass(RID p_body, real_t p_total_mass) override;
	virtual real_t soft_body_get_total_mass(RID p_body) const override;

	virtual void soft_body_set_linear_stiffness(RID p_body, real_t p_stiffness) override;
	virtual real_t soft_body_get_linear_stiffness(RID p_body) const override;

	virtual void soft_body_set_pressure_coefficient(RID p_body, real_t p_pressure_coefficient) override;
	virtual real_t soft_body_get_pressure_coefficient(RID p_body) const override;

	virtual void soft_body_set_damping_coefficient(RID p_body, real_t p_damping_coefficient) override;
	virtual real_t soft_body_get_damping_coefficient(RID p_body) const override;

	virtual void soft_body_set_drag_coefficient(RID p_body, real_t p_drag_coefficient) override;
	virtual real_t soft_body_get_drag_coefficient(RID p_body) const override;

	virtual void soft_body_set_mesh(RID p_body, RID p_mesh) override;

	virtual AABB soft_body_get_bounds(RID p_body) const override;

	virtual void soft_body_move_point(RID p_body, int p_point_index, const Vector3 &p_global_position) override;
	virtual Vector3 soft_body_get_point_global_position(RID p_body, int p_point_index) const override;

	virtual void soft_body_remove_all_pinned_points(RID p_body) override;
	virtual void soft_body_pin_point(RID p_body, int p_point_index, bool p_pin) override;
	virtual bool soft_body_is_point_pinned(RID p_body, int p_point_index) const override;

	/* JOINT API */

	virtual RID joint_create() override;

	virtual void joint_clear(RID p_joint) override; //resets type

	virtual void joint_make_pin(RID p_joint, RID p_body_A, const Vector3 &p_local_A, RID p_body_B, const Vector3 &p_local_B) override;

	virtual void pin_joint_set_param(RID p_joint, PinJointParam p_param, real_t p_value) override;
	virtual real_t pin_joint_get_param(RID p_joint, PinJointParam p_param) const override;

	virtual void pin_joint_set_local_a(RID p_joint, const Vector3 &p_A) override;
	virtual Vector3 pin_joint_get_local_a(RID p_joint) const override;

	virtual void pin_joint_set_local_b(RID p_joint, const Vector3 &p_B) override;
	virtual Vector3 pin_joint_get_local_b(RID p_joint) const override;

	virtual void joint_make_hinge(RID p_joint, RID p_body_A, const Transform3D &p_frame_A, RID p_body_B, const Transform3D &p_frame_B) override;
	virtual void joint_make_hinge_simple(RID p_joint, RID p_body_A, const Vector3 &p_pivot_A, const Vector3 &p_axis_A, RID p_body_B, const Vector3 &p_pivot_B, const Vector3 &p_axis_B) override;

	virtual void hinge_joint_set_param(RID p_joint, HingeJointParam p_param, real_t p_value) override;
	virtual real_t hinge_joint_get_param(RID p_joint, HingeJointParam p_param) const override;

	virtual void hinge_joint_set_flag(RID p_joint, HingeJointFlag p_flag, bool p_value) override;
	virtual bool hinge_joint_get_flag(RID p_joint, HingeJointFlag p_flag) const override;

	virtual void joint_make_slider(RID p_joint, RID p_body_A, const Transform3D &p_local_frame_A, RID p_body_B, const Transform3D &p_local_frame_B) override; //reference frame is A

	virtual void slider_joint_set_param(RID p_joint, SliderJointParam p_param, real_t p_value) override;
	virtual real_t slider_joint_get_param(RID p_joint, SliderJointParam p_param) const override;

	virtual void joint_make_cone_twist(RID p_joint, RID p_body_A, const Transform3D &p_local_frame_A, RID p_body_B, const Transform3D &p_local_frame_B) override; //reference frame is A

	virtual void cone_twist_joint_set_param(RID p_joint, ConeTwistJointParam p_param, real_t p_value) override;
	virtual real_t cone_twist_joint_get_param(RID p_joint, ConeTwistJointParam p_param) const override;

	virtual void joint_make_generic_6dof(RID p_joint, RID p_body_A, const Transform3D &p_local_frame_A, RID p_body_B, const Transform3D &p_local_frame_B) override; //reference frame is A

	virtual void generic_6dof_joint_set_param(RID p_joint, Vector3::Axis, G6DOFJointAxisParam p_param, real_t p_value) override;
	virtual real_t generic_6dof_joint_get_param(RID p_joint, Vector3::Axis, G6DOFJointAxisParam p_param) const override;

	virtual void generic_6dof_joint_set_flag(RID p_joint, Vector3::Axis, G6DOFJointAxisFlag p_flag, bool p_enable) override;
	virtual bool generic_6dof_joint_get_flag(RID p_joint, Vector3::Axis, G6DOFJointAxisFlag p_flag) const override;

	virtual JointType joint_get_type(RID p_joint) const override;

	virtual void joint_set_solver_priority(RID p_joint, int p_priority) override;
	virtual int joint_get_solver_priority(RID p_joint) const override;

	virtual void joint_disable_collisions_between_bodies(RID p_joint, bool p_disable) override;
	virtual bool joint_is_disabled_collisions_between_bodies(RID p_joint) const override;

	/* MISC */

	virtual void free(RID p_rid) override;

	virtual void set_active(bool p_active) override;
	virtual void init() override;
	virtual void step(real_t p_step) override;
	virtual void sync() override;
	virtual void flush_queries() override;
	virtual void end_sync() override;
	virtual void finish() override;

	virtual bool is_flushing_queries() const override { return flushing_queries; }

	int get_process_info(ProcessInfo p_info) override;

	GodotPhysicsServer3D(bool p_using_threads = false);
	~GodotPhysicsServer3D() {}
};

#endif // GODOT_PHYSICS_SERVER_3D_H
