/**************************************************************************/
/*  physics_server_3d_extension.h                                         */
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

#ifndef PHYSICS_SERVER_3D_EXTENSION_H
#define PHYSICS_SERVER_3D_EXTENSION_H

#include "core/extension/ext_wrappers.gen.inc"
#include "core/object/script_language.h"
#include "core/variant/native_ptr.h"
#include "core/variant/typed_array.h"
#include "servers/physics_server_3d.h"

class PhysicsDirectBodyState3DExtension : public PhysicsDirectBodyState3D {
	GDCLASS(PhysicsDirectBodyState3DExtension, PhysicsDirectBodyState3D);

protected:
	static void _bind_methods();

public:
	// The warning is valid, but unavoidable. If the function is not overridden it will error anyway.

	EXBIND0RC(Vector3, get_total_gravity)
	EXBIND0RC(real_t, get_total_angular_damp)
	EXBIND0RC(real_t, get_total_linear_damp)

	EXBIND0RC(Vector3, get_center_of_mass)
	EXBIND0RC(Vector3, get_center_of_mass_local)
	EXBIND0RC(Basis, get_principal_inertia_axes)
	EXBIND0RC(real_t, get_inverse_mass)
	EXBIND0RC(Vector3, get_inverse_inertia)
	EXBIND0RC(Basis, get_inverse_inertia_tensor)

	EXBIND1(set_linear_velocity, const Vector3 &)
	EXBIND0RC(Vector3, get_linear_velocity)

	EXBIND1(set_angular_velocity, const Vector3 &)
	EXBIND0RC(Vector3, get_angular_velocity)

	EXBIND1(set_transform, const Transform3D &)
	EXBIND0RC(Transform3D, get_transform)

	EXBIND1RC(Vector3, get_velocity_at_local_position, const Vector3 &)

	EXBIND1(apply_central_impulse, const Vector3 &)
	EXBIND2(apply_impulse, const Vector3 &, const Vector3 &)
	EXBIND1(apply_torque_impulse, const Vector3 &)

	EXBIND1(apply_central_force, const Vector3 &)
	EXBIND2(apply_force, const Vector3 &, const Vector3 &)
	EXBIND1(apply_torque, const Vector3 &)

	EXBIND1(add_constant_central_force, const Vector3 &)
	EXBIND2(add_constant_force, const Vector3 &, const Vector3 &)
	EXBIND1(add_constant_torque, const Vector3 &)

	EXBIND1(set_constant_force, const Vector3 &)
	EXBIND0RC(Vector3, get_constant_force)

	EXBIND1(set_constant_torque, const Vector3 &)
	EXBIND0RC(Vector3, get_constant_torque)

	EXBIND1(set_sleep_state, bool)
	EXBIND0RC(bool, is_sleeping)

	EXBIND0RC(int, get_contact_count)

	EXBIND1RC(Vector3, get_contact_local_position, int)
	EXBIND1RC(Vector3, get_contact_local_normal, int)
	EXBIND1RC(Vector3, get_contact_impulse, int)
	EXBIND1RC(int, get_contact_local_shape, int)
	EXBIND1RC(Vector3, get_contact_local_velocity_at_position, int)
	EXBIND1RC(RID, get_contact_collider, int)
	EXBIND1RC(Vector3, get_contact_collider_position, int)
	EXBIND1RC(ObjectID, get_contact_collider_id, int)
	EXBIND1RC(Object *, get_contact_collider_object, int)
	EXBIND1RC(int, get_contact_collider_shape, int)
	EXBIND1RC(Vector3, get_contact_collider_velocity_at_position, int)

	EXBIND0RC(real_t, get_step)

	EXBIND0(integrate_forces)
	EXBIND0R(PhysicsDirectSpaceState3D *, get_space_state)

	PhysicsDirectBodyState3DExtension();
};

typedef PhysicsDirectSpaceState3D::RayResult PhysicsServer3DExtensionRayResult;
typedef PhysicsDirectSpaceState3D::ShapeResult PhysicsServer3DExtensionShapeResult;
typedef PhysicsDirectSpaceState3D::ShapeRestInfo PhysicsServer3DExtensionShapeRestInfo;

GDVIRTUAL_NATIVE_PTR(PhysicsServer3DExtensionRayResult)
GDVIRTUAL_NATIVE_PTR(PhysicsServer3DExtensionShapeResult)
GDVIRTUAL_NATIVE_PTR(PhysicsServer3DExtensionShapeRestInfo)

class PhysicsDirectSpaceState3DExtension : public PhysicsDirectSpaceState3D {
	GDCLASS(PhysicsDirectSpaceState3DExtension, PhysicsDirectSpaceState3D);

	thread_local static const HashSet<RID> *exclude;

protected:
	static void _bind_methods();
	bool is_body_excluded_from_query(const RID &p_body) const;

	GDVIRTUAL9R_REQUIRED(bool, _intersect_ray, const Vector3 &, const Vector3 &, uint32_t, bool, bool, bool, bool, bool, GDExtensionPtr<PhysicsServer3DExtensionRayResult>)
	GDVIRTUAL6R_REQUIRED(int, _intersect_point, const Vector3 &, uint32_t, bool, bool, GDExtensionPtr<PhysicsServer3DExtensionShapeResult>, int)
	GDVIRTUAL9R_REQUIRED(int, _intersect_shape, RID, const Transform3D &, const Vector3 &, real_t, uint32_t, bool, bool, GDExtensionPtr<PhysicsServer3DExtensionShapeResult>, int)
	GDVIRTUAL10R_REQUIRED(bool, _cast_motion, RID, const Transform3D &, const Vector3 &, real_t, uint32_t, bool, bool, GDExtensionPtr<real_t>, GDExtensionPtr<real_t>, GDExtensionPtr<PhysicsServer3DExtensionShapeRestInfo>)
	GDVIRTUAL10R_REQUIRED(bool, _collide_shape, RID, const Transform3D &, const Vector3 &, real_t, uint32_t, bool, bool, GDExtensionPtr<Vector3>, int, GDExtensionPtr<int>)
	GDVIRTUAL8R_REQUIRED(bool, _rest_info, RID, const Transform3D &, const Vector3 &, real_t, uint32_t, bool, bool, GDExtensionPtr<PhysicsServer3DExtensionShapeRestInfo>)
	GDVIRTUAL2RC_REQUIRED(Vector3, _get_closest_point_to_object_volume, RID, const Vector3 &)

public:
	virtual bool intersect_ray(const RayParameters &p_parameters, RayResult &r_result) override {
		exclude = &p_parameters.exclude;
		bool ret = false;
		GDVIRTUAL_CALL(_intersect_ray, p_parameters.from, p_parameters.to, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, p_parameters.hit_from_inside, p_parameters.hit_back_faces, p_parameters.pick_ray, &r_result, ret);
		exclude = nullptr;
		return ret;
	}
	virtual int intersect_point(const PointParameters &p_parameters, ShapeResult *r_results, int p_result_max) override {
		exclude = &p_parameters.exclude;
		int ret = false;
		GDVIRTUAL_CALL(_intersect_point, p_parameters.position, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, r_results, p_result_max, ret);
		exclude = nullptr;
		return ret;
	}
	virtual int intersect_shape(const ShapeParameters &p_parameters, ShapeResult *r_results, int p_result_max) override {
		exclude = &p_parameters.exclude;
		int ret = 0;
		GDVIRTUAL_CALL(_intersect_shape, p_parameters.shape_rid, p_parameters.transform, p_parameters.motion, p_parameters.margin, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, r_results, p_result_max, ret);
		exclude = nullptr;
		return ret;
	}
	virtual bool cast_motion(const ShapeParameters &p_parameters, real_t &p_closest_safe, real_t &p_closest_unsafe, ShapeRestInfo *r_info = nullptr) override {
		exclude = &p_parameters.exclude;
		bool ret = false;
		GDVIRTUAL_CALL(_cast_motion, p_parameters.shape_rid, p_parameters.transform, p_parameters.motion, p_parameters.margin, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, &p_closest_safe, &p_closest_unsafe, r_info, ret);
		exclude = nullptr;
		return ret;
	}
	virtual bool collide_shape(const ShapeParameters &p_parameters, Vector3 *r_results, int p_result_max, int &r_result_count) override {
		exclude = &p_parameters.exclude;
		bool ret = false;
		GDVIRTUAL_CALL(_collide_shape, p_parameters.shape_rid, p_parameters.transform, p_parameters.motion, p_parameters.margin, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, r_results, p_result_max, &r_result_count, ret);
		exclude = nullptr;
		return ret;
	}
	virtual bool rest_info(const ShapeParameters &p_parameters, ShapeRestInfo *r_info) override {
		exclude = &p_parameters.exclude;
		bool ret = false;
		GDVIRTUAL_CALL(_rest_info, p_parameters.shape_rid, p_parameters.transform, p_parameters.motion, p_parameters.margin, p_parameters.collision_mask, p_parameters.collide_with_bodies, p_parameters.collide_with_areas, r_info, ret);
		exclude = nullptr;
		return ret;
	}

	virtual Vector3 get_closest_point_to_object_volume(RID p_object, const Vector3 p_point) const override {
		Vector3 ret;
		GDVIRTUAL_CALL(_get_closest_point_to_object_volume, p_object, p_point, ret);
		return ret;
	}

	PhysicsDirectSpaceState3DExtension();
};

typedef PhysicsServer3D::MotionCollision PhysicsServer3DExtensionMotionCollision;
typedef PhysicsServer3D::MotionResult PhysicsServer3DExtensionMotionResult;

GDVIRTUAL_NATIVE_PTR(PhysicsServer3DExtensionMotionCollision)
GDVIRTUAL_NATIVE_PTR(PhysicsServer3DExtensionMotionResult)

class PhysicsServer3DExtension : public PhysicsServer3D {
	GDCLASS(PhysicsServer3DExtension, PhysicsServer3D);

protected:
	static void _bind_methods();

public:
	// The warning is valid, but unavoidable. If the function is not overridden it will error anyway.

	/* SHAPE API */

	EXBIND0R(RID, world_boundary_shape_create)
	EXBIND0R(RID, separation_ray_shape_create)
	EXBIND0R(RID, sphere_shape_create)
	EXBIND0R(RID, box_shape_create)
	EXBIND0R(RID, capsule_shape_create)
	EXBIND0R(RID, cylinder_shape_create)
	EXBIND0R(RID, convex_polygon_shape_create)
	EXBIND0R(RID, concave_polygon_shape_create)
	EXBIND0R(RID, heightmap_shape_create)
	EXBIND0R(RID, custom_shape_create)

	EXBIND2(shape_set_data, RID, const Variant &)
	EXBIND2(shape_set_custom_solver_bias, RID, real_t)

	EXBIND2(shape_set_margin, RID, real_t)
	EXBIND1RC(real_t, shape_get_margin, RID)

	EXBIND1RC(ShapeType, shape_get_type, RID)
	EXBIND1RC(Variant, shape_get_data, RID)
	EXBIND1RC(real_t, shape_get_custom_solver_bias, RID)

	/* SPACE API */

	EXBIND0R(RID, space_create)
	EXBIND2(space_set_active, RID, bool)
	EXBIND1RC(bool, space_is_active, RID)

	EXBIND3(space_set_param, RID, SpaceParameter, real_t)
	EXBIND2RC(real_t, space_get_param, RID, SpaceParameter)

	EXBIND1R(PhysicsDirectSpaceState3D *, space_get_direct_state, RID)

	EXBIND2(space_set_debug_contacts, RID, int)
	EXBIND1RC(Vector<Vector3>, space_get_contacts, RID)
	EXBIND1RC(int, space_get_contact_count, RID)

	/* AREA API */

	//EXBIND0RID(area);
	EXBIND0R(RID, area_create)

	EXBIND2(area_set_space, RID, RID)
	EXBIND1RC(RID, area_get_space, RID)

	EXBIND4(area_add_shape, RID, RID, const Transform3D &, bool)
	EXBIND3(area_set_shape, RID, int, RID)
	EXBIND3(area_set_shape_transform, RID, int, const Transform3D &)
	EXBIND3(area_set_shape_disabled, RID, int, bool)

	EXBIND1RC(int, area_get_shape_count, RID)
	EXBIND2RC(RID, area_get_shape, RID, int)
	EXBIND2RC(Transform3D, area_get_shape_transform, RID, int)

	EXBIND2(area_remove_shape, RID, int)
	EXBIND1(area_clear_shapes, RID)

	EXBIND2(area_attach_object_instance_id, RID, ObjectID)
	EXBIND1RC(ObjectID, area_get_object_instance_id, RID)

	EXBIND3(area_set_param, RID, AreaParameter, const Variant &)
	EXBIND2(area_set_transform, RID, const Transform3D &)

	EXBIND2RC(Variant, area_get_param, RID, AreaParameter)
	EXBIND1RC(Transform3D, area_get_transform, RID)

	EXBIND2(area_set_collision_layer, RID, uint32_t)
	EXBIND1RC(uint32_t, area_get_collision_layer, RID)

	EXBIND2(area_set_collision_mask, RID, uint32_t)
	EXBIND1RC(uint32_t, area_get_collision_mask, RID)

	EXBIND2(area_set_monitorable, RID, bool)
	EXBIND2(area_set_ray_pickable, RID, bool)

	EXBIND2(area_set_monitor_callback, RID, const Callable &)
	EXBIND2(area_set_area_monitor_callback, RID, const Callable &)

	/* BODY API */

	//EXBIND2RID(body,BodyMode,bool);
	EXBIND0R(RID, body_create)

	EXBIND2(body_set_space, RID, RID)
	EXBIND1RC(RID, body_get_space, RID)

	EXBIND2(body_set_mode, RID, BodyMode)
	EXBIND1RC(BodyMode, body_get_mode, RID)

	EXBIND4(body_add_shape, RID, RID, const Transform3D &, bool)
	EXBIND3(body_set_shape, RID, int, RID)
	EXBIND3(body_set_shape_transform, RID, int, const Transform3D &)
	EXBIND3(body_set_shape_disabled, RID, int, bool)

	EXBIND1RC(int, body_get_shape_count, RID)
	EXBIND2RC(RID, body_get_shape, RID, int)
	EXBIND2RC(Transform3D, body_get_shape_transform, RID, int)

	EXBIND2(body_remove_shape, RID, int)
	EXBIND1(body_clear_shapes, RID)

	EXBIND2(body_attach_object_instance_id, RID, ObjectID)
	EXBIND1RC(ObjectID, body_get_object_instance_id, RID)

	EXBIND2(body_set_enable_continuous_collision_detection, RID, bool)
	EXBIND1RC(bool, body_is_continuous_collision_detection_enabled, RID)

	EXBIND2(body_set_collision_layer, RID, uint32_t)
	EXBIND1RC(uint32_t, body_get_collision_layer, RID)

	EXBIND2(body_set_collision_mask, RID, uint32_t)
	EXBIND1RC(uint32_t, body_get_collision_mask, RID)

	EXBIND2(body_set_collision_priority, RID, real_t)
	EXBIND1RC(real_t, body_get_collision_priority, RID)

	EXBIND2(body_set_user_flags, RID, uint32_t)
	EXBIND1RC(uint32_t, body_get_user_flags, RID)

	EXBIND3(body_set_param, RID, BodyParameter, const Variant &)
	EXBIND2RC(Variant, body_get_param, RID, BodyParameter)

	EXBIND1(body_reset_mass_properties, RID)

	EXBIND3(body_set_state, RID, BodyState, const Variant &)
	EXBIND2RC(Variant, body_get_state, RID, BodyState)

	EXBIND2(body_apply_central_impulse, RID, const Vector3 &)
	EXBIND3(body_apply_impulse, RID, const Vector3 &, const Vector3 &)
	EXBIND2(body_apply_torque_impulse, RID, const Vector3 &)

	EXBIND2(body_apply_central_force, RID, const Vector3 &)
	EXBIND3(body_apply_force, RID, const Vector3 &, const Vector3 &)
	EXBIND2(body_apply_torque, RID, const Vector3 &)

	EXBIND2(body_add_constant_central_force, RID, const Vector3 &)
	EXBIND3(body_add_constant_force, RID, const Vector3 &, const Vector3 &)
	EXBIND2(body_add_constant_torque, RID, const Vector3 &)

	EXBIND2(body_set_constant_force, RID, const Vector3 &)
	EXBIND1RC(Vector3, body_get_constant_force, RID)

	EXBIND2(body_set_constant_torque, RID, const Vector3 &)
	EXBIND1RC(Vector3, body_get_constant_torque, RID)

	EXBIND2(body_set_axis_velocity, RID, const Vector3 &)

	EXBIND3(body_set_axis_lock, RID, BodyAxis, bool)
	EXBIND2RC(bool, body_is_axis_locked, RID, BodyAxis)

	EXBIND2(body_add_collision_exception, RID, RID)
	EXBIND2(body_remove_collision_exception, RID, RID)

	GDVIRTUAL1RC_REQUIRED(TypedArray<RID>, _body_get_collision_exceptions, RID)

	void body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) override {
		TypedArray<RID> ret;
		GDVIRTUAL_CALL(_body_get_collision_exceptions, p_body, ret);
		for (int i = 0; i < ret.size(); i++) {
			p_exceptions->push_back(ret[i]);
		}
	}

	EXBIND2(body_set_max_contacts_reported, RID, int)
	EXBIND1RC(int, body_get_max_contacts_reported, RID)

	EXBIND2(body_set_contacts_reported_depth_threshold, RID, real_t)
	EXBIND1RC(real_t, body_get_contacts_reported_depth_threshold, RID)

	EXBIND2(body_set_omit_force_integration, RID, bool)
	EXBIND1RC(bool, body_is_omitting_force_integration, RID)

	EXBIND2(body_set_state_sync_callback, RID, const Callable &)
	EXBIND3(body_set_force_integration_callback, RID, const Callable &, const Variant &)

	EXBIND2(body_set_ray_pickable, RID, bool)

	GDVIRTUAL8RC_REQUIRED(bool, _body_test_motion, RID, const Transform3D &, const Vector3 &, real_t, int, bool, bool, GDExtensionPtr<PhysicsServer3DExtensionMotionResult>)

	thread_local static const HashSet<RID> *exclude_bodies;
	thread_local static const HashSet<ObjectID> *exclude_objects;

	bool body_test_motion_is_excluding_body(RID p_body) const;
	bool body_test_motion_is_excluding_object(ObjectID p_object) const;

	bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) override {
		bool ret = false;
		exclude_bodies = &p_parameters.exclude_bodies;
		exclude_objects = &p_parameters.exclude_objects;
		GDVIRTUAL_CALL(_body_test_motion, p_body, p_parameters.from, p_parameters.motion, p_parameters.margin, p_parameters.max_collisions, p_parameters.collide_separation_ray, p_parameters.recovery_as_collision, r_result, ret);
		exclude_bodies = nullptr;
		exclude_objects = nullptr;
		return ret;
	}

	EXBIND1R(PhysicsDirectBodyState3D *, body_get_direct_state, RID)

	/* SOFT BODY API */

	EXBIND0R(RID, soft_body_create)

	EXBIND2(soft_body_update_rendering_server, RID, PhysicsServer3DRenderingServerHandler *)

	EXBIND2(soft_body_set_space, RID, RID)
	EXBIND1RC(RID, soft_body_get_space, RID)

	EXBIND2(soft_body_set_ray_pickable, RID, bool)

	EXBIND2(soft_body_set_collision_layer, RID, uint32_t)
	EXBIND1RC(uint32_t, soft_body_get_collision_layer, RID)

	EXBIND2(soft_body_set_collision_mask, RID, uint32_t)
	EXBIND1RC(uint32_t, soft_body_get_collision_mask, RID)

	EXBIND2(soft_body_add_collision_exception, RID, RID)
	EXBIND2(soft_body_remove_collision_exception, RID, RID)

	GDVIRTUAL1RC_REQUIRED(TypedArray<RID>, _soft_body_get_collision_exceptions, RID)

	void soft_body_get_collision_exceptions(RID p_soft_body, List<RID> *p_exceptions) override {
		TypedArray<RID> ret;
		GDVIRTUAL_CALL(_soft_body_get_collision_exceptions, p_soft_body, ret);
		for (int i = 0; i < ret.size(); i++) {
			p_exceptions->push_back(ret[i]);
		}
	}

	EXBIND3(soft_body_set_state, RID, BodyState, const Variant &)
	EXBIND2RC(Variant, soft_body_get_state, RID, BodyState)

	EXBIND2(soft_body_set_transform, RID, const Transform3D &)

	EXBIND2(soft_body_set_simulation_precision, RID, int)
	EXBIND1RC(int, soft_body_get_simulation_precision, RID)

	EXBIND2(soft_body_set_total_mass, RID, real_t)
	EXBIND1RC(real_t, soft_body_get_total_mass, RID)

	EXBIND2(soft_body_set_linear_stiffness, RID, real_t)
	EXBIND1RC(real_t, soft_body_get_linear_stiffness, RID)

	EXBIND2(soft_body_set_pressure_coefficient, RID, real_t)
	EXBIND1RC(real_t, soft_body_get_pressure_coefficient, RID)

	EXBIND2(soft_body_set_damping_coefficient, RID, real_t)
	EXBIND1RC(real_t, soft_body_get_damping_coefficient, RID)

	EXBIND2(soft_body_set_drag_coefficient, RID, real_t)
	EXBIND1RC(real_t, soft_body_get_drag_coefficient, RID)

	EXBIND2(soft_body_set_mesh, RID, RID)

	EXBIND1RC(AABB, soft_body_get_bounds, RID)

	EXBIND3(soft_body_move_point, RID, int, const Vector3 &)
	EXBIND2RC(Vector3, soft_body_get_point_global_position, RID, int)

	EXBIND1(soft_body_remove_all_pinned_points, RID)
	EXBIND3(soft_body_pin_point, RID, int, bool)
	EXBIND2RC(bool, soft_body_is_point_pinned, RID, int)

	/* JOINT API */

	EXBIND0R(RID, joint_create)
	EXBIND1(joint_clear, RID)

	EXBIND5(joint_make_pin, RID, RID, const Vector3 &, RID, const Vector3 &)

	EXBIND3(pin_joint_set_param, RID, PinJointParam, real_t)
	EXBIND2RC(real_t, pin_joint_get_param, RID, PinJointParam)

	EXBIND2(pin_joint_set_local_a, RID, const Vector3 &)
	EXBIND1RC(Vector3, pin_joint_get_local_a, RID)

	EXBIND2(pin_joint_set_local_b, RID, const Vector3 &)
	EXBIND1RC(Vector3, pin_joint_get_local_b, RID)

	EXBIND5(joint_make_hinge, RID, RID, const Transform3D &, RID, const Transform3D &)
	EXBIND7(joint_make_hinge_simple, RID, RID, const Vector3 &, const Vector3 &, RID, const Vector3 &, const Vector3 &)

	EXBIND3(hinge_joint_set_param, RID, HingeJointParam, real_t)
	EXBIND2RC(real_t, hinge_joint_get_param, RID, HingeJointParam)

	EXBIND3(hinge_joint_set_flag, RID, HingeJointFlag, bool)
	EXBIND2RC(bool, hinge_joint_get_flag, RID, HingeJointFlag)

	EXBIND5(joint_make_slider, RID, RID, const Transform3D &, RID, const Transform3D &)

	EXBIND3(slider_joint_set_param, RID, SliderJointParam, real_t)
	EXBIND2RC(real_t, slider_joint_get_param, RID, SliderJointParam)

	EXBIND5(joint_make_cone_twist, RID, RID, const Transform3D &, RID, const Transform3D &)

	EXBIND3(cone_twist_joint_set_param, RID, ConeTwistJointParam, real_t)
	EXBIND2RC(real_t, cone_twist_joint_get_param, RID, ConeTwistJointParam)

	EXBIND5(joint_make_generic_6dof, RID, RID, const Transform3D &, RID, const Transform3D &)

	EXBIND4(generic_6dof_joint_set_param, RID, Vector3::Axis, G6DOFJointAxisParam, real_t)
	EXBIND3RC(real_t, generic_6dof_joint_get_param, RID, Vector3::Axis, G6DOFJointAxisParam)

	EXBIND4(generic_6dof_joint_set_flag, RID, Vector3::Axis, G6DOFJointAxisFlag, bool)
	EXBIND3RC(bool, generic_6dof_joint_get_flag, RID, Vector3::Axis, G6DOFJointAxisFlag)

	EXBIND1RC(JointType, joint_get_type, RID)

	EXBIND2(joint_set_solver_priority, RID, int)
	EXBIND1RC(int, joint_get_solver_priority, RID)

	EXBIND2(joint_disable_collisions_between_bodies, RID, bool)
	EXBIND1RC(bool, joint_is_disabled_collisions_between_bodies, RID)

	/* MISC */

	GDVIRTUAL1_REQUIRED(_free_rid, RID)
	virtual void free(RID p_rid) override {
		GDVIRTUAL_CALL(_free_rid, p_rid);
	}

	EXBIND1(set_active, bool)

	EXBIND0(init)
	EXBIND1(step, real_t)
	EXBIND0(sync)
	EXBIND0(flush_queries)
	EXBIND0(end_sync)
	EXBIND0(finish)

	EXBIND0RC(bool, is_flushing_queries)
	EXBIND1R(int, get_process_info, ProcessInfo)

	PhysicsServer3DExtension();
	~PhysicsServer3DExtension();
};

#endif // PHYSICS_SERVER_3D_EXTENSION_H
