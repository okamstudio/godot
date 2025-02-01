/**************************************************************************/
/*  physics_server_3d_wrap_mt.h                                           */
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

#include "core/config/project_settings.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/thread.h"
#include "core/templates/command_queue_mt.h"
#include "servers/physics_server_3d.h"

#ifdef DEBUG_SYNC
#define SYNC_DEBUG print_line("sync on: " + String(__FUNCTION__));
#else
#define SYNC_DEBUG
#endif

#ifdef DEBUG_ENABLED
#ifdef DEV_ENABLED
#define MAIN_THREAD_SYNC_WARN WARN_PRINT("Call to " + String(__FUNCTION__) + " causing PhysicsServer3D synchronizations on every frame. This significantly affects performance.");
#else
#define MAIN_THREAD_SYNC_WARN
#endif
#endif

class PhysicsServer3DWrapMT : public PhysicsServer3D {
	mutable PhysicsServer3D *physics_server_3d = nullptr;

	mutable CommandQueueMT command_queue;

	Thread::ID server_thread = Thread::UNASSIGNED_ID;
	WorkerThreadPool::TaskID server_task_id = WorkerThreadPool::INVALID_TASK_ID;
	bool exit = false;
	bool create_thread = false;

	void _assign_mt_ids(WorkerThreadPool::TaskID p_pump_task_id);
	void _thread_exit();
	void _thread_step(real_t p_delta);
	void _thread_loop();

public:
#define ServerName PhysicsServer3D
#define ServerNameWrapMT PhysicsServer3DWrapMT
#define server_name physics_server_3d
#define WRITE_ACTION

#include "servers/server_wrap_mt_common.h"

	//FUNC1RID(shape,ShapeType); todo fix
	FUNCRID(world_boundary_shape)
	FUNCRID(separation_ray_shape)
	FUNCRID(sphere_shape)
	FUNCRID(box_shape)
	FUNCRID(capsule_shape)
	FUNCRID(cylinder_shape)
	FUNCRID(convex_polygon_shape)
	FUNCRID(concave_polygon_shape)
	FUNCRID(heightmap_shape)
	FUNCRID(custom_shape)

	FUNC2(shape_set_data, RID, const Variant &);
	FUNC2(shape_set_custom_solver_bias, RID, real_t);

	FUNC2(shape_set_margin, RID, real_t)
	FUNC1RC(real_t, shape_get_margin, RID)

	FUNC1RC(ShapeType, shape_get_type, RID);
	FUNC1RC(Variant, shape_get_data, RID);
	FUNC1RC(real_t, shape_get_custom_solver_bias, RID);
#if 0
	//these work well, but should be used from the main thread only
	bool shape_collide(RID p_shape_A, const Transform &p_xform_A, const Vector3 &p_motion_A, RID p_shape_B, const Transform &p_xform_B, const Vector3 &p_motion_B, Vector3 *r_results, int p_result_max, int &r_result_count) {
		ERR_FAIL_COND_V(!Thread::is_main_thread(), false);
		return physics_server_3d->shape_collide(p_shape_A, p_xform_A, p_motion_A, p_shape_B, p_xform_B, p_motion_B, r_results, p_result_max, r_result_count);
	}
#endif
	/* SPACE API */

	FUNCRID(space);
	FUNC2(space_set_active, RID, bool);
	FUNC1RC(bool, space_is_active, RID);

	FUNC3(space_set_param, RID, SpaceParameter, real_t);
	FUNC2RC(real_t, space_get_param, RID, SpaceParameter);

	// this function only works on physics process, errors and returns null otherwise
	PhysicsDirectSpaceState3D *space_get_direct_state(RID p_space) override {
		ERR_FAIL_COND_V(!Thread::is_main_thread(), nullptr);
		return physics_server_3d->space_get_direct_state(p_space);
	}

	FUNC2(space_set_debug_contacts, RID, int);
	virtual Vector<Vector3> space_get_contacts(RID p_space) const override {
		ERR_FAIL_COND_V(!Thread::is_main_thread(), Vector<Vector3>());
		return physics_server_3d->space_get_contacts(p_space);
	}

	virtual int space_get_contact_count(RID p_space) const override {
		ERR_FAIL_COND_V(!Thread::is_main_thread(), 0);
		return physics_server_3d->space_get_contact_count(p_space);
	}

	/* AREA API */

	//FUNC0RID(area);
	FUNCRID(area);

	FUNC2(area_set_space, RID, RID);
	FUNC1RC(RID, area_get_space, RID);

	FUNC4(area_add_shape, RID, RID, const Transform3D &, bool);
	FUNC3(area_set_shape, RID, int, RID);
	FUNC3(area_set_shape_transform, RID, int, const Transform3D &);
	FUNC3(area_set_shape_disabled, RID, int, bool);

	FUNC1RC(int, area_get_shape_count, RID);
	FUNC2RC(RID, area_get_shape, RID, int);
	FUNC2RC(Transform3D, area_get_shape_transform, RID, int);
	FUNC2(area_remove_shape, RID, int);
	FUNC1(area_clear_shapes, RID);

	FUNC2(area_attach_object_instance_id, RID, ObjectID);
	FUNC1RC(ObjectID, area_get_object_instance_id, RID);

	FUNC3(area_set_param, RID, AreaParameter, const Variant &);
	FUNC2(area_set_transform, RID, const Transform3D &);

	FUNC2RC(Variant, area_get_param, RID, AreaParameter);
	FUNC1RC(Transform3D, area_get_transform, RID);

	FUNC2(area_set_collision_layer, RID, uint32_t);
	FUNC1RC(uint32_t, area_get_collision_layer, RID);

	FUNC2(area_set_collision_mask, RID, uint32_t);
	FUNC1RC(uint32_t, area_get_collision_mask, RID);

	FUNC2(area_set_monitorable, RID, bool);
	FUNC2(area_set_ray_pickable, RID, bool);

	FUNC2(area_set_monitor_callback, RID, const Callable &);
	FUNC2(area_set_area_monitor_callback, RID, const Callable &);

	/* BODY API */

	//FUNC2RID(body,BodyMode,bool);
	FUNCRID(body)

	FUNC2(body_set_space, RID, RID);
	FUNC1RC(RID, body_get_space, RID);

	FUNC2(body_set_mode, RID, BodyMode);
	FUNC1RC(BodyMode, body_get_mode, RID);

	FUNC4(body_add_shape, RID, RID, const Transform3D &, bool);
	FUNC3(body_set_shape, RID, int, RID);
	FUNC3(body_set_shape_transform, RID, int, const Transform3D &);

	FUNC1RC(int, body_get_shape_count, RID);
	FUNC2RC(Transform3D, body_get_shape_transform, RID, int);
	FUNC2RC(RID, body_get_shape, RID, int);

	FUNC3(body_set_shape_disabled, RID, int, bool);

	FUNC2(body_remove_shape, RID, int);
	FUNC1(body_clear_shapes, RID);

	FUNC2(body_attach_object_instance_id, RID, ObjectID);
	FUNC1RC(ObjectID, body_get_object_instance_id, RID);

	FUNC2(body_set_enable_continuous_collision_detection, RID, bool);
	FUNC1RC(bool, body_is_continuous_collision_detection_enabled, RID);

	FUNC2(body_set_collision_layer, RID, uint32_t);
	FUNC1RC(uint32_t, body_get_collision_layer, RID);

	FUNC2(body_set_collision_mask, RID, uint32_t);
	FUNC1RC(uint32_t, body_get_collision_mask, RID);

	FUNC2(body_set_collision_priority, RID, real_t);
	FUNC1RC(real_t, body_get_collision_priority, RID);

	FUNC2(body_set_user_flags, RID, uint32_t);
	FUNC1RC(uint32_t, body_get_user_flags, RID);

	FUNC3(body_set_param, RID, BodyParameter, const Variant &);
	FUNC2RC(Variant, body_get_param, RID, BodyParameter);

	FUNC1(body_reset_mass_properties, RID);

	FUNC3(body_set_state, RID, BodyState, const Variant &);
	FUNC2RC(Variant, body_get_state, RID, BodyState);

	FUNC2(body_apply_torque_impulse, RID, const Vector3 &);
	FUNC2(body_apply_central_impulse, RID, const Vector3 &);
	FUNC3(body_apply_impulse, RID, const Vector3 &, const Vector3 &);

	FUNC2(body_apply_central_force, RID, const Vector3 &);
	FUNC3(body_apply_force, RID, const Vector3 &, const Vector3 &);
	FUNC2(body_apply_torque, RID, const Vector3 &);

	FUNC2(body_add_constant_central_force, RID, const Vector3 &);
	FUNC3(body_add_constant_force, RID, const Vector3 &, const Vector3 &);
	FUNC2(body_add_constant_torque, RID, const Vector3 &);

	FUNC2(body_set_constant_force, RID, const Vector3 &);
	FUNC1RC(Vector3, body_get_constant_force, RID);

	FUNC2(body_set_constant_torque, RID, const Vector3 &);
	FUNC1RC(Vector3, body_get_constant_torque, RID);

	FUNC2(body_set_axis_velocity, RID, const Vector3 &);

	FUNC3(body_set_axis_lock, RID, BodyAxis, bool);
	FUNC2RC(bool, body_is_axis_locked, RID, BodyAxis);

	FUNC2(body_add_collision_exception, RID, RID);
	FUNC2(body_remove_collision_exception, RID, RID);
	FUNC2S(body_get_collision_exceptions, RID, List<RID> *);

	FUNC2(body_set_max_contacts_reported, RID, int);
	FUNC1RC(int, body_get_max_contacts_reported, RID);

	FUNC2(body_set_contacts_reported_depth_threshold, RID, real_t);
	FUNC1RC(real_t, body_get_contacts_reported_depth_threshold, RID);

	FUNC2(body_set_omit_force_integration, RID, bool);
	FUNC1RC(bool, body_is_omitting_force_integration, RID);

	FUNC2(body_set_state_sync_callback, RID, const Callable &);
	FUNC3(body_set_force_integration_callback, RID, const Callable &, const Variant &);

	FUNC2(body_set_ray_pickable, RID, bool);

	bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) override {
		ERR_FAIL_COND_V(!Thread::is_main_thread(), false);
		return physics_server_3d->body_test_motion(p_body, p_parameters, r_result);
	}

	// this function only works on physics process, errors and returns null otherwise
	PhysicsDirectBodyState3D *body_get_direct_state(RID p_body) override {
		ERR_FAIL_COND_V(!Thread::is_main_thread(), nullptr);
		return physics_server_3d->body_get_direct_state(p_body);
	}

	/* SOFT BODY API */

	FUNCRID(soft_body)

	FUNC2(soft_body_update_rendering_server, RID, PhysicsServer3DRenderingServerHandler *)

	FUNC2(soft_body_set_space, RID, RID)
	FUNC1RC(RID, soft_body_get_space, RID)

	FUNC2(soft_body_set_ray_pickable, RID, bool);

	FUNC2(soft_body_set_collision_layer, RID, uint32_t)
	FUNC1RC(uint32_t, soft_body_get_collision_layer, RID)

	FUNC2(soft_body_set_collision_mask, RID, uint32_t)
	FUNC1RC(uint32_t, soft_body_get_collision_mask, RID)

	FUNC2(soft_body_add_collision_exception, RID, RID)
	FUNC2(soft_body_remove_collision_exception, RID, RID)
	FUNC2S(soft_body_get_collision_exceptions, RID, List<RID> *)

	FUNC3(soft_body_set_state, RID, BodyState, const Variant &);
	FUNC2RC(Variant, soft_body_get_state, RID, BodyState);

	FUNC2(soft_body_set_transform, RID, const Transform3D &);

	FUNC2(soft_body_set_simulation_precision, RID, int);
	FUNC1RC(int, soft_body_get_simulation_precision, RID);

	FUNC2(soft_body_set_total_mass, RID, real_t);
	FUNC1RC(real_t, soft_body_get_total_mass, RID);

	FUNC2(soft_body_set_linear_stiffness, RID, real_t);
	FUNC1RC(real_t, soft_body_get_linear_stiffness, RID);

	FUNC2(soft_body_set_pressure_coefficient, RID, real_t);
	FUNC1RC(real_t, soft_body_get_pressure_coefficient, RID);

	FUNC2(soft_body_set_damping_coefficient, RID, real_t);
	FUNC1RC(real_t, soft_body_get_damping_coefficient, RID);

	FUNC2(soft_body_set_drag_coefficient, RID, real_t);
	FUNC1RC(real_t, soft_body_get_drag_coefficient, RID);

	FUNC2(soft_body_set_mesh, RID, RID);

	FUNC1RC(AABB, soft_body_get_bounds, RID);

	FUNC3(soft_body_move_point, RID, int, const Vector3 &);
	FUNC2RC(Vector3, soft_body_get_point_global_position, RID, int);

	FUNC1(soft_body_remove_all_pinned_points, RID);
	FUNC3(soft_body_pin_point, RID, int, bool);
	FUNC2RC(bool, soft_body_is_point_pinned, RID, int);

	/* JOINT API */

	FUNCRID(joint)

	FUNC1(joint_clear, RID)

	FUNC5(joint_make_pin, RID, RID, const Vector3 &, RID, const Vector3 &)

	FUNC3(pin_joint_set_param, RID, PinJointParam, real_t)
	FUNC2RC(real_t, pin_joint_get_param, RID, PinJointParam)

	FUNC2(pin_joint_set_local_a, RID, const Vector3 &)
	FUNC1RC(Vector3, pin_joint_get_local_a, RID)

	FUNC2(pin_joint_set_local_b, RID, const Vector3 &)
	FUNC1RC(Vector3, pin_joint_get_local_b, RID)

	FUNC5(joint_make_hinge, RID, RID, const Transform3D &, RID, const Transform3D &)
	FUNC7(joint_make_hinge_simple, RID, RID, const Vector3 &, const Vector3 &, RID, const Vector3 &, const Vector3 &)

	FUNC3(hinge_joint_set_param, RID, HingeJointParam, real_t)
	FUNC2RC(real_t, hinge_joint_get_param, RID, HingeJointParam)

	FUNC3(hinge_joint_set_flag, RID, HingeJointFlag, bool)
	FUNC2RC(bool, hinge_joint_get_flag, RID, HingeJointFlag)

	FUNC5(joint_make_slider, RID, RID, const Transform3D &, RID, const Transform3D &)

	FUNC3(slider_joint_set_param, RID, SliderJointParam, real_t)
	FUNC2RC(real_t, slider_joint_get_param, RID, SliderJointParam)

	FUNC5(joint_make_cone_twist, RID, RID, const Transform3D &, RID, const Transform3D &)

	FUNC3(cone_twist_joint_set_param, RID, ConeTwistJointParam, real_t)
	FUNC2RC(real_t, cone_twist_joint_get_param, RID, ConeTwistJointParam)

	FUNC5(joint_make_generic_6dof, RID, RID, const Transform3D &, RID, const Transform3D &)

	FUNC4(generic_6dof_joint_set_param, RID, Vector3::Axis, G6DOFJointAxisParam, real_t)
	FUNC3RC(real_t, generic_6dof_joint_get_param, RID, Vector3::Axis, G6DOFJointAxisParam)

	FUNC4(generic_6dof_joint_set_flag, RID, Vector3::Axis, G6DOFJointAxisFlag, bool)
	FUNC3RC(bool, generic_6dof_joint_get_flag, RID, Vector3::Axis, G6DOFJointAxisFlag)

	FUNC1RC(JointType, joint_get_type, RID);

	FUNC2(joint_set_solver_priority, RID, int);
	FUNC1RC(int, joint_get_solver_priority, RID);

	FUNC2(joint_disable_collisions_between_bodies, RID, bool);
	FUNC1RC(bool, joint_is_disabled_collisions_between_bodies, RID);

	/* MISC */

	FUNC1(free, RID);
	FUNC1(set_active, bool);

	virtual void init() override;
	virtual void step(real_t p_step) override;
	virtual void sync() override;
	virtual void end_sync() override;
	virtual void flush_queries() override;
	virtual void finish() override;

	virtual bool is_flushing_queries() const override {
		return physics_server_3d->is_flushing_queries();
	}

	int get_process_info(ProcessInfo p_info) override {
		return physics_server_3d->get_process_info(p_info);
	}

	PhysicsServer3DWrapMT(PhysicsServer3D *p_contained, bool p_create_thread);
	~PhysicsServer3DWrapMT();

#undef ServerNameWrapMT
#undef ServerName
#undef server_name
#undef WRITE_ACTION
};

#ifdef DEBUG_SYNC
#undef DEBUG_SYNC
#endif
#undef SYNC_DEBUG

#ifdef DEBUG_ENABLED
#undef MAIN_THREAD_SYNC_WARN
#endif
