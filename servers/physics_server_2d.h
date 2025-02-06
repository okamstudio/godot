/**************************************************************************/
/*  physics_server_2d.h                                                   */
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

#include "core/io/resource.h"
#include "core/object/class_db.h"
#include "core/object/ref_counted.h"

class PhysicsDirectSpaceState2D;
template <typename T>
class TypedArray;

class PhysicsDirectBodyState2D : public Object {
	GDCLASS(PhysicsDirectBodyState2D, Object);

protected:
	static void _bind_methods();

public:
	virtual Vector2 get_total_gravity() const = 0; // get gravity vector working on this body space/area
	virtual real_t get_total_linear_damp() const = 0; // get density of this body space/area
	virtual real_t get_total_angular_damp() const = 0; // get density of this body space/area

	virtual Vector2 get_center_of_mass() const = 0;
	virtual Vector2 get_center_of_mass_local() const = 0;
	virtual real_t get_inverse_mass() const = 0; // get the mass
	virtual real_t get_inverse_inertia() const = 0; // get density of this body space

	virtual void set_linear_velocity(const Vector2 &p_velocity) = 0;
	virtual Vector2 get_linear_velocity() const = 0;

	virtual void set_angular_velocity(real_t p_velocity) = 0;
	virtual real_t get_angular_velocity() const = 0;

	virtual void set_transform(const Transform2D &p_transform) = 0;
	virtual Transform2D get_transform() const = 0;

	virtual Vector2 get_velocity_at_local_position(const Vector2 &p_position) const = 0;

	virtual void apply_central_impulse(const Vector2 &p_impulse) = 0;
	virtual void apply_torque_impulse(real_t p_torque) = 0;
	virtual void apply_impulse(const Vector2 &p_impulse, const Vector2 &p_position = Vector2()) = 0;

	virtual void apply_central_force(const Vector2 &p_force) = 0;
	virtual void apply_force(const Vector2 &p_force, const Vector2 &p_position = Vector2()) = 0;
	virtual void apply_torque(real_t p_torque) = 0;

	virtual void add_constant_central_force(const Vector2 &p_force) = 0;
	virtual void add_constant_force(const Vector2 &p_force, const Vector2 &p_position = Vector2()) = 0;
	virtual void add_constant_torque(real_t p_torque) = 0;

	virtual void set_constant_force(const Vector2 &p_force) = 0;
	virtual Vector2 get_constant_force() const = 0;

	virtual void set_constant_torque(real_t p_torque) = 0;
	virtual real_t get_constant_torque() const = 0;

	virtual void set_sleep_state(bool p_enable) = 0;
	virtual bool is_sleeping() const = 0;

	virtual int get_contact_count() const = 0;

	virtual Vector2 get_contact_local_position(int p_contact_idx) const = 0;
	virtual Vector2 get_contact_local_normal(int p_contact_idx) const = 0;
	virtual int get_contact_local_shape(int p_contact_idx) const = 0;
	virtual Vector2 get_contact_local_velocity_at_position(int p_contact_idx) const = 0;

	virtual RID get_contact_collider(int p_contact_idx) const = 0;
	virtual Vector2 get_contact_collider_position(int p_contact_idx) const = 0;
	virtual ObjectID get_contact_collider_id(int p_contact_idx) const = 0;
	virtual Object *get_contact_collider_object(int p_contact_idx) const;
	virtual int get_contact_collider_shape(int p_contact_idx) const = 0;
	virtual Vector2 get_contact_collider_velocity_at_position(int p_contact_idx) const = 0;
	virtual Vector2 get_contact_impulse(int p_contact_idx) const = 0;

	virtual real_t get_step() const = 0;
	virtual void integrate_forces();

	virtual PhysicsDirectSpaceState2D *get_space_state() = 0;

	PhysicsDirectBodyState2D();
};

class PhysicsRayQueryParameters2D;
class PhysicsPointQueryParameters2D;
class PhysicsShapeQueryParameters2D;

class PhysicsDirectSpaceState2D : public Object {
	GDCLASS(PhysicsDirectSpaceState2D, Object);

	Dictionary _intersect_ray(const Ref<PhysicsRayQueryParameters2D> &p_ray_query);
	TypedArray<Dictionary> _intersect_point(const Ref<PhysicsPointQueryParameters2D> &p_point_query, int p_max_results = 32);
	TypedArray<Dictionary> _intersect_shape(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query, int p_max_results = 32);
	Vector<real_t> _cast_motion(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query);
	TypedArray<Vector2> _collide_shape(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query, int p_max_results = 32);
	Dictionary _get_rest_info(const Ref<PhysicsShapeQueryParameters2D> &p_shape_query);

protected:
	static void _bind_methods();

public:
	struct RayParameters {
		Vector2 from;
		Vector2 to;
		HashSet<RID> exclude;
		uint32_t collision_mask = UINT32_MAX;

		bool collide_with_bodies = true;
		bool collide_with_areas = false;

		bool hit_from_inside = false;
	};

	struct RayResult {
		Vector2 position;
		Vector2 normal;
		RID rid;
		ObjectID collider_id;
		Object *collider = nullptr;
		int shape = 0;
	};

	virtual bool intersect_ray(const RayParameters &p_parameters, RayResult &r_result) = 0;

	struct ShapeResult {
		RID rid;
		ObjectID collider_id;
		Object *collider = nullptr;
		int shape = 0;
	};

	struct PointParameters {
		Vector2 position;
		ObjectID canvas_instance_id;
		HashSet<RID> exclude;
		uint32_t collision_mask = UINT32_MAX;

		bool collide_with_bodies = true;
		bool collide_with_areas = false;

		bool pick_point = false;
	};

	virtual int intersect_point(const PointParameters &p_parameters, ShapeResult *r_results, int p_result_max) = 0;

	struct ShapeParameters {
		RID shape_rid;
		Transform2D transform;
		Vector2 motion;
		real_t margin = 0.0;
		HashSet<RID> exclude;
		uint32_t collision_mask = UINT32_MAX;

		bool collide_with_bodies = true;
		bool collide_with_areas = false;
	};

	struct ShapeRestInfo {
		Vector2 point;
		Vector2 normal;
		RID rid;
		ObjectID collider_id;
		int shape = 0;
		Vector2 linear_velocity; // Velocity at contact point.
	};

	virtual int intersect_shape(const ShapeParameters &p_parameters, ShapeResult *r_results, int p_result_max) = 0;
	virtual bool cast_motion(const ShapeParameters &p_parameters, real_t &p_closest_safe, real_t &p_closest_unsafe) = 0;
	virtual bool collide_shape(const ShapeParameters &p_parameters, Vector2 *r_results, int p_result_max, int &r_result_count) = 0;
	virtual bool rest_info(const ShapeParameters &p_parameters, ShapeRestInfo *r_info) = 0;

	PhysicsDirectSpaceState2D();
};

class PhysicsTestMotionParameters2D;
class PhysicsTestMotionResult2D;

class PhysicsServer2D : public Object {
	GDCLASS(PhysicsServer2D, Object);

	static PhysicsServer2D *singleton;

	virtual bool _body_test_motion(RID p_body, const Ref<PhysicsTestMotionParameters2D> &p_parameters, const Ref<PhysicsTestMotionResult2D> &p_result = Ref<PhysicsTestMotionResult2D>());

protected:
	static void _bind_methods();

public:
	static PhysicsServer2D *get_singleton();

	enum ShapeType {
		SHAPE_WORLD_BOUNDARY, ///< plane:"plane"
		SHAPE_SEPARATION_RAY, ///< float:"length"
		SHAPE_SEGMENT, ///< float:"length"
		SHAPE_CIRCLE, ///< float:"radius"
		SHAPE_RECTANGLE, ///< vec3:"extents"
		SHAPE_CAPSULE,
		SHAPE_CONVEX_POLYGON, ///< array of planes:"planes"
		SHAPE_CONCAVE_POLYGON, ///< Vector2 array:"triangles" , or Dictionary with "indices" (int array) and "triangles" (Vector2 array)
		SHAPE_CUSTOM, ///< Server-Implementation based custom shape, calling shape_create() with this value will result in an error
	};

	virtual RID world_boundary_shape_create() = 0;
	virtual RID separation_ray_shape_create() = 0;
	virtual RID segment_shape_create() = 0;
	virtual RID circle_shape_create() = 0;
	virtual RID rectangle_shape_create() = 0;
	virtual RID capsule_shape_create() = 0;
	virtual RID convex_polygon_shape_create() = 0;
	virtual RID concave_polygon_shape_create() = 0;

	virtual void shape_set_data(RID p_shape, const Variant &p_data) = 0;
	virtual void shape_set_custom_solver_bias(RID p_shape, real_t p_bias) = 0;

	virtual ShapeType shape_get_type(RID p_shape) const = 0;
	virtual Variant shape_get_data(RID p_shape) const = 0;
	virtual real_t shape_get_custom_solver_bias(RID p_shape) const = 0;

	//these work well, but should be used from the main thread only
	virtual bool shape_collide(RID p_shape_A, const Transform2D &p_xform_A, const Vector2 &p_motion_A, RID p_shape_B, const Transform2D &p_xform_B, const Vector2 &p_motion_B, Vector2 *r_results, int p_result_max, int &r_result_count) = 0;

	/* SPACE API */

	virtual RID space_create() = 0;
	virtual void space_set_active(RID p_space, bool p_active) = 0;
	virtual bool space_is_active(RID p_space) const = 0;

	enum SpaceParameter {
		SPACE_PARAM_CONTACT_RECYCLE_RADIUS,
		SPACE_PARAM_CONTACT_MAX_SEPARATION,
		SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION,
		SPACE_PARAM_CONTACT_DEFAULT_BIAS,
		SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD,
		SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_THRESHOLD,
		SPACE_PARAM_BODY_TIME_TO_SLEEP,
		SPACE_PARAM_CONSTRAINT_DEFAULT_BIAS,
		SPACE_PARAM_SOLVER_ITERATIONS,
	};

	virtual void space_set_param(RID p_space, SpaceParameter p_param, real_t p_value) = 0;
	virtual real_t space_get_param(RID p_space, SpaceParameter p_param) const = 0;

	// this function only works on physics process, errors and returns null otherwise
	virtual PhysicsDirectSpaceState2D *space_get_direct_state(RID p_space) = 0;

	virtual void space_set_debug_contacts(RID p_space, int p_max_contacts) = 0;
	virtual Vector<Vector2> space_get_contacts(RID p_space) const = 0;
	virtual int space_get_contact_count(RID p_space) const = 0;

	//missing space parameters

	/* AREA API */

	//missing attenuation? missing better override?

	enum AreaParameter {
		AREA_PARAM_GRAVITY_OVERRIDE_MODE,
		AREA_PARAM_GRAVITY,
		AREA_PARAM_GRAVITY_VECTOR,
		AREA_PARAM_GRAVITY_IS_POINT,
		AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE,
		AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE,
		AREA_PARAM_LINEAR_DAMP,
		AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE,
		AREA_PARAM_ANGULAR_DAMP,
		AREA_PARAM_PRIORITY
	};

	virtual RID area_create() = 0;

	virtual void area_set_space(RID p_area, RID p_space) = 0;
	virtual RID area_get_space(RID p_area) const = 0;

	enum AreaSpaceOverrideMode {
		AREA_SPACE_OVERRIDE_DISABLED,
		AREA_SPACE_OVERRIDE_COMBINE,
		AREA_SPACE_OVERRIDE_COMBINE_REPLACE, // Combines, then discards all subsequent calculations
		AREA_SPACE_OVERRIDE_REPLACE,
		AREA_SPACE_OVERRIDE_REPLACE_COMBINE // Discards all previous calculations, then keeps combining
	};

	virtual void area_add_shape(RID p_area, RID p_shape, const Transform2D &p_transform = Transform2D(), bool p_disabled = false) = 0;
	virtual void area_set_shape(RID p_area, int p_shape_idx, RID p_shape) = 0;
	virtual void area_set_shape_transform(RID p_area, int p_shape_idx, const Transform2D &p_transform) = 0;

	virtual int area_get_shape_count(RID p_area) const = 0;
	virtual RID area_get_shape(RID p_area, int p_shape_idx) const = 0;
	virtual Transform2D area_get_shape_transform(RID p_area, int p_shape_idx) const = 0;

	virtual void area_remove_shape(RID p_area, int p_shape_idx) = 0;
	virtual void area_clear_shapes(RID p_area) = 0;

	virtual void area_set_shape_disabled(RID p_area, int p_shape, bool p_disabled) = 0;

	virtual void area_attach_object_instance_id(RID p_area, ObjectID p_id) = 0;
	virtual ObjectID area_get_object_instance_id(RID p_area) const = 0;

	virtual void area_attach_canvas_instance_id(RID p_area, ObjectID p_id) = 0;
	virtual ObjectID area_get_canvas_instance_id(RID p_area) const = 0;

	virtual void area_set_param(RID p_area, AreaParameter p_param, const Variant &p_value) = 0;
	virtual void area_set_transform(RID p_area, const Transform2D &p_transform) = 0;

	virtual Variant area_get_param(RID p_parea, AreaParameter p_param) const = 0;
	virtual Transform2D area_get_transform(RID p_area) const = 0;

	virtual void area_set_collision_layer(RID p_area, uint32_t p_layer) = 0;
	virtual uint32_t area_get_collision_layer(RID p_area) const = 0;

	virtual void area_set_collision_mask(RID p_area, uint32_t p_mask) = 0;
	virtual uint32_t area_get_collision_mask(RID p_area) const = 0;

	virtual void area_set_monitorable(RID p_area, bool p_monitorable) = 0;
	virtual void area_set_pickable(RID p_area, bool p_pickable) = 0;

	virtual void area_set_monitor_callback(RID p_area, const Callable &p_callback) = 0;
	virtual void area_set_area_monitor_callback(RID p_area, const Callable &p_callback) = 0;

	/* BODY API */

	//missing ccd?

	enum BodyMode {
		BODY_MODE_STATIC,
		BODY_MODE_KINEMATIC,
		BODY_MODE_RIGID,
		BODY_MODE_RIGID_LINEAR,
	};

	virtual RID body_create() = 0;

	virtual void body_set_space(RID p_body, RID p_space) = 0;
	virtual RID body_get_space(RID p_body) const = 0;

	virtual void body_set_mode(RID p_body, BodyMode p_mode) = 0;
	virtual BodyMode body_get_mode(RID p_body) const = 0;

	virtual void body_add_shape(RID p_body, RID p_shape, const Transform2D &p_transform = Transform2D(), bool p_disabled = false) = 0;
	virtual void body_set_shape(RID p_body, int p_shape_idx, RID p_shape) = 0;
	virtual void body_set_shape_transform(RID p_body, int p_shape_idx, const Transform2D &p_transform) = 0;

	virtual int body_get_shape_count(RID p_body) const = 0;
	virtual RID body_get_shape(RID p_body, int p_shape_idx) const = 0;
	virtual Transform2D body_get_shape_transform(RID p_body, int p_shape_idx) const = 0;

	virtual void body_set_shape_disabled(RID p_body, int p_shape, bool p_disabled) = 0;
	virtual void body_set_shape_as_one_way_collision(RID p_body, int p_shape, bool p_enabled, real_t p_margin = 0) = 0;

	virtual void body_remove_shape(RID p_body, int p_shape_idx) = 0;
	virtual void body_clear_shapes(RID p_body) = 0;

	virtual void body_attach_object_instance_id(RID p_body, ObjectID p_id) = 0;
	virtual ObjectID body_get_object_instance_id(RID p_body) const = 0;

	virtual void body_attach_canvas_instance_id(RID p_body, ObjectID p_id) = 0;
	virtual ObjectID body_get_canvas_instance_id(RID p_body) const = 0;

	enum CCDMode {
		CCD_MODE_DISABLED,
		CCD_MODE_CAST_RAY,
		CCD_MODE_CAST_SHAPE,
	};

	virtual void body_set_continuous_collision_detection_mode(RID p_body, CCDMode p_mode) = 0;
	virtual CCDMode body_get_continuous_collision_detection_mode(RID p_body) const = 0;

	virtual void body_set_collision_layer(RID p_body, uint32_t p_layer) = 0;
	virtual uint32_t body_get_collision_layer(RID p_body) const = 0;

	virtual void body_set_collision_mask(RID p_body, uint32_t p_mask) = 0;
	virtual uint32_t body_get_collision_mask(RID p_body) const = 0;

	virtual void body_set_collision_priority(RID p_body, real_t p_priority) = 0;
	virtual real_t body_get_collision_priority(RID p_body) const = 0;

	// common body variables
	enum BodyParameter {
		BODY_PARAM_BOUNCE,
		BODY_PARAM_FRICTION,
		BODY_PARAM_MASS, ///< unused for static, always infinite
		BODY_PARAM_INERTIA,
		BODY_PARAM_CENTER_OF_MASS,
		BODY_PARAM_GRAVITY_SCALE,
		BODY_PARAM_LINEAR_DAMP_MODE,
		BODY_PARAM_ANGULAR_DAMP_MODE,
		BODY_PARAM_LINEAR_DAMP,
		BODY_PARAM_ANGULAR_DAMP,
		BODY_PARAM_MAX,
	};

	enum BodyDampMode {
		BODY_DAMP_MODE_COMBINE,
		BODY_DAMP_MODE_REPLACE,
	};

	virtual void body_set_param(RID p_body, BodyParameter p_param, const Variant &p_value) = 0;
	virtual Variant body_get_param(RID p_body, BodyParameter p_param) const = 0;

	virtual void body_reset_mass_properties(RID p_body) = 0;

	//state
	enum BodyState {
		BODY_STATE_TRANSFORM,
		BODY_STATE_LINEAR_VELOCITY,
		BODY_STATE_ANGULAR_VELOCITY,
		BODY_STATE_SLEEPING,
		BODY_STATE_CAN_SLEEP,
	};

	virtual void body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) = 0;
	virtual Variant body_get_state(RID p_body, BodyState p_state) const = 0;

	virtual void body_apply_central_impulse(RID p_body, const Vector2 &p_impulse) = 0;
	virtual void body_apply_torque_impulse(RID p_body, real_t p_torque) = 0;
	virtual void body_apply_impulse(RID p_body, const Vector2 &p_impulse, const Vector2 &p_position = Vector2()) = 0;

	virtual void body_apply_central_force(RID p_body, const Vector2 &p_force) = 0;
	virtual void body_apply_force(RID p_body, const Vector2 &p_force, const Vector2 &p_position = Vector2()) = 0;
	virtual void body_apply_torque(RID p_body, real_t p_torque) = 0;

	virtual void body_add_constant_central_force(RID p_body, const Vector2 &p_force) = 0;
	virtual void body_add_constant_force(RID p_body, const Vector2 &p_force, const Vector2 &p_position = Vector2()) = 0;
	virtual void body_add_constant_torque(RID p_body, real_t p_torque) = 0;

	virtual void body_set_constant_force(RID p_body, const Vector2 &p_force) = 0;
	virtual Vector2 body_get_constant_force(RID p_body) const = 0;

	virtual void body_set_constant_torque(RID p_body, real_t p_torque) = 0;
	virtual real_t body_get_constant_torque(RID p_body) const = 0;

	virtual void body_set_axis_velocity(RID p_body, const Vector2 &p_axis_velocity) = 0;

	//fix
	virtual void body_add_collision_exception(RID p_body, RID p_body_b) = 0;
	virtual void body_remove_collision_exception(RID p_body, RID p_body_b) = 0;
	virtual void body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) = 0;

	virtual void body_set_max_contacts_reported(RID p_body, int p_contacts) = 0;
	virtual int body_get_max_contacts_reported(RID p_body) const = 0;

	//missing remove
	virtual void body_set_contacts_reported_depth_threshold(RID p_body, real_t p_threshold) = 0;
	virtual real_t body_get_contacts_reported_depth_threshold(RID p_body) const = 0;

	virtual void body_set_omit_force_integration(RID p_body, bool p_omit) = 0;
	virtual bool body_is_omitting_force_integration(RID p_body) const = 0;

	virtual void body_set_state_sync_callback(RID p_body, const Callable &p_callable) = 0;
	virtual void body_set_force_integration_callback(RID p_body, const Callable &p_callable, const Variant &p_udata = Variant()) = 0;

	virtual bool body_collide_shape(RID p_body, int p_body_shape, RID p_shape, const Transform2D &p_shape_xform, const Vector2 &p_motion, Vector2 *r_results, int p_result_max, int &r_result_count) = 0;

	virtual void body_set_pickable(RID p_body, bool p_pickable) = 0;

	// this function only works on physics process, errors and returns null otherwise
	virtual PhysicsDirectBodyState2D *body_get_direct_state(RID p_body) = 0;

	struct MotionParameters {
		Transform2D from;
		Vector2 motion;
		real_t margin = 0.08;
		bool collide_separation_ray = false;
		HashSet<RID> exclude_bodies;
		HashSet<ObjectID> exclude_objects;
		bool recovery_as_collision = false;

		MotionParameters() {}

		MotionParameters(const Transform2D &p_from, const Vector2 &p_motion, real_t p_margin = 0.08) :
				from(p_from),
				motion(p_motion),
				margin(p_margin) {}
	};

	struct MotionResult {
		Vector2 travel;
		Vector2 remainder;

		Vector2 collision_point;
		Vector2 collision_normal;
		Vector2 collider_velocity;
		real_t collision_depth = 0.0;
		real_t collision_safe_fraction = 0.0;
		real_t collision_unsafe_fraction = 0.0;
		int collision_local_shape = 0;
		ObjectID collider_id;
		RID collider;
		int collider_shape = 0;

		real_t get_angle(Vector2 p_up_direction) const {
			return Math::acos(collision_normal.dot(p_up_direction));
		}
	};

	virtual bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) = 0;

	/* JOINT API */

	virtual RID joint_create() = 0;

	virtual void joint_clear(RID p_joint) = 0;

	enum JointType {
		JOINT_TYPE_PIN,
		JOINT_TYPE_GROOVE,
		JOINT_TYPE_DAMPED_SPRING,
		JOINT_TYPE_MAX
	};

	enum JointParam {
		JOINT_PARAM_BIAS,
		JOINT_PARAM_MAX_BIAS,
		JOINT_PARAM_MAX_FORCE,
	};

	virtual void joint_set_param(RID p_joint, JointParam p_param, real_t p_value) = 0;
	virtual real_t joint_get_param(RID p_joint, JointParam p_param) const = 0;

	virtual void joint_disable_collisions_between_bodies(RID p_joint, const bool p_disable) = 0;
	virtual bool joint_is_disabled_collisions_between_bodies(RID p_joint) const = 0;

	virtual void joint_make_pin(RID p_joint, const Vector2 &p_anchor, RID p_body_a, RID p_body_b = RID()) = 0;
	virtual void joint_make_groove(RID p_joint, const Vector2 &p_a_groove1, const Vector2 &p_a_groove2, const Vector2 &p_b_anchor, RID p_body_a, RID p_body_b) = 0;
	virtual void joint_make_damped_spring(RID p_joint, const Vector2 &p_anchor_a, const Vector2 &p_anchor_b, RID p_body_a, RID p_body_b = RID()) = 0;

	enum PinJointParam {
		PIN_JOINT_SOFTNESS,
		PIN_JOINT_LIMIT_UPPER,
		PIN_JOINT_LIMIT_LOWER,
		PIN_JOINT_MOTOR_TARGET_VELOCITY
	};

	virtual void pin_joint_set_param(RID p_joint, PinJointParam p_param, real_t p_value) = 0;
	virtual real_t pin_joint_get_param(RID p_joint, PinJointParam p_param) const = 0;

	enum PinJointFlag {
		PIN_JOINT_FLAG_ANGULAR_LIMIT_ENABLED,
		PIN_JOINT_FLAG_MOTOR_ENABLED
	};

	virtual void pin_joint_set_flag(RID p_joint, PinJointFlag p_flag, bool p_enabled) = 0;
	virtual bool pin_joint_get_flag(RID p_joint, PinJointFlag p_flag) const = 0;

	enum DampedSpringParam {
		DAMPED_SPRING_REST_LENGTH,
		DAMPED_SPRING_STIFFNESS,
		DAMPED_SPRING_DAMPING
	};
	virtual void damped_spring_joint_set_param(RID p_joint, DampedSpringParam p_param, real_t p_value) = 0;
	virtual real_t damped_spring_joint_get_param(RID p_joint, DampedSpringParam p_param) const = 0;

	virtual JointType joint_get_type(RID p_joint) const = 0;

	/* QUERY API */

	enum AreaBodyStatus {
		AREA_BODY_ADDED,
		AREA_BODY_REMOVED
	};

	/* MISC */

	virtual void free(RID p_rid) = 0;

	virtual void set_active(bool p_active) = 0;
	virtual void init() = 0;
	virtual void step(real_t p_step) = 0;
	virtual void sync() = 0;
	virtual void flush_queries() = 0;
	virtual void end_sync() = 0;
	virtual void finish() = 0;

	virtual bool is_flushing_queries() const = 0;

	enum ProcessInfo {
		INFO_ACTIVE_OBJECTS,
		INFO_COLLISION_PAIRS,
		INFO_ISLAND_COUNT
	};

	virtual int get_process_info(ProcessInfo p_info) = 0;

	PhysicsServer2D();
	~PhysicsServer2D();
};

class PhysicsRayQueryParameters2D : public RefCounted {
	GDCLASS(PhysicsRayQueryParameters2D, RefCounted);

	PhysicsDirectSpaceState2D::RayParameters parameters;

protected:
	static void _bind_methods();

public:
	static Ref<PhysicsRayQueryParameters2D> create(Vector2 p_from, Vector2 p_to, uint32_t p_mask, const TypedArray<RID> &p_exclude);
	const PhysicsDirectSpaceState2D::RayParameters &get_parameters() const { return parameters; }

	void set_from(const Vector2 &p_from) { parameters.from = p_from; }
	const Vector2 &get_from() const { return parameters.from; }

	void set_to(const Vector2 &p_to) { parameters.to = p_to; }
	const Vector2 &get_to() const { return parameters.to; }

	void set_collision_mask(uint32_t p_mask) { parameters.collision_mask = p_mask; }
	uint32_t get_collision_mask() const { return parameters.collision_mask; }

	void set_collide_with_bodies(bool p_enable) { parameters.collide_with_bodies = p_enable; }
	bool is_collide_with_bodies_enabled() const { return parameters.collide_with_bodies; }

	void set_collide_with_areas(bool p_enable) { parameters.collide_with_areas = p_enable; }
	bool is_collide_with_areas_enabled() const { return parameters.collide_with_areas; }

	void set_hit_from_inside(bool p_enable) { parameters.hit_from_inside = p_enable; }
	bool is_hit_from_inside_enabled() const { return parameters.hit_from_inside; }

	void set_exclude(const TypedArray<RID> &p_exclude);
	TypedArray<RID> get_exclude() const;
};

class PhysicsPointQueryParameters2D : public RefCounted {
	GDCLASS(PhysicsPointQueryParameters2D, RefCounted);

	PhysicsDirectSpaceState2D::PointParameters parameters;

protected:
	static void _bind_methods();

public:
	const PhysicsDirectSpaceState2D::PointParameters &get_parameters() const { return parameters; }

	void set_position(const Vector2 &p_position) { parameters.position = p_position; }
	const Vector2 &get_position() const { return parameters.position; }

	void set_canvas_instance_id(ObjectID p_canvas_instance_id) { parameters.canvas_instance_id = p_canvas_instance_id; }
	ObjectID get_canvas_instance_id() const { return parameters.canvas_instance_id; }

	void set_collision_mask(uint32_t p_mask) { parameters.collision_mask = p_mask; }
	uint32_t get_collision_mask() const { return parameters.collision_mask; }

	void set_collide_with_bodies(bool p_enable) { parameters.collide_with_bodies = p_enable; }
	bool is_collide_with_bodies_enabled() const { return parameters.collide_with_bodies; }

	void set_collide_with_areas(bool p_enable) { parameters.collide_with_areas = p_enable; }
	bool is_collide_with_areas_enabled() const { return parameters.collide_with_areas; }

	void set_exclude(const TypedArray<RID> &p_exclude);
	TypedArray<RID> get_exclude() const;
};

class PhysicsShapeQueryParameters2D : public RefCounted {
	GDCLASS(PhysicsShapeQueryParameters2D, RefCounted);

	PhysicsDirectSpaceState2D::ShapeParameters parameters;

	Ref<Resource> shape_ref;

protected:
	static void _bind_methods();

public:
	const PhysicsDirectSpaceState2D::ShapeParameters &get_parameters() const { return parameters; }

	void set_shape(const Ref<Resource> &p_shape_ref);
	Ref<Resource> get_shape() const { return shape_ref; }

	void set_shape_rid(const RID &p_shape);
	RID get_shape_rid() const { return parameters.shape_rid; }

	void set_transform(const Transform2D &p_transform) { parameters.transform = p_transform; }
	const Transform2D &get_transform() const { return parameters.transform; }

	void set_motion(const Vector2 &p_motion) { parameters.motion = p_motion; }
	const Vector2 &get_motion() const { return parameters.motion; }

	void set_margin(real_t p_margin) { parameters.margin = p_margin; }
	real_t get_margin() const { return parameters.margin; }

	void set_collision_mask(uint32_t p_mask) { parameters.collision_mask = p_mask; }
	uint32_t get_collision_mask() const { return parameters.collision_mask; }

	void set_collide_with_bodies(bool p_enable) { parameters.collide_with_bodies = p_enable; }
	bool is_collide_with_bodies_enabled() const { return parameters.collide_with_bodies; }

	void set_collide_with_areas(bool p_enable) { parameters.collide_with_areas = p_enable; }
	bool is_collide_with_areas_enabled() const { return parameters.collide_with_areas; }

	void set_exclude(const TypedArray<RID> &p_exclude);
	TypedArray<RID> get_exclude() const;
};

class PhysicsTestMotionParameters2D : public RefCounted {
	GDCLASS(PhysicsTestMotionParameters2D, RefCounted);

	PhysicsServer2D::MotionParameters parameters;

protected:
	static void _bind_methods();

public:
	const PhysicsServer2D::MotionParameters &get_parameters() const { return parameters; }

	const Transform2D &get_from() const { return parameters.from; }
	void set_from(const Transform2D &p_from) { parameters.from = p_from; }

	const Vector2 &get_motion() const { return parameters.motion; }
	void set_motion(const Vector2 &p_motion) { parameters.motion = p_motion; }

	real_t get_margin() const { return parameters.margin; }
	void set_margin(real_t p_margin) { parameters.margin = p_margin; }

	bool is_collide_separation_ray_enabled() const { return parameters.collide_separation_ray; }
	void set_collide_separation_ray_enabled(bool p_enabled) { parameters.collide_separation_ray = p_enabled; }

	TypedArray<RID> get_exclude_bodies() const;
	void set_exclude_bodies(const TypedArray<RID> &p_exclude);

	TypedArray<uint64_t> get_exclude_objects() const;
	void set_exclude_objects(const TypedArray<uint64_t> &p_exclude);

	bool is_recovery_as_collision_enabled() const { return parameters.recovery_as_collision; }
	void set_recovery_as_collision_enabled(bool p_enabled) { parameters.recovery_as_collision = p_enabled; }
};

class PhysicsTestMotionResult2D : public RefCounted {
	GDCLASS(PhysicsTestMotionResult2D, RefCounted);

	PhysicsServer2D::MotionResult result;

protected:
	static void _bind_methods();

public:
	PhysicsServer2D::MotionResult *get_result_ptr() { return &result; }

	Vector2 get_travel() const;
	Vector2 get_remainder() const;

	Vector2 get_collision_point() const;
	Vector2 get_collision_normal() const;
	Vector2 get_collider_velocity() const;
	ObjectID get_collider_id() const;
	RID get_collider_rid() const;
	Object *get_collider() const;
	int get_collider_shape() const;
	int get_collision_local_shape() const;
	real_t get_collision_depth() const;
	real_t get_collision_safe_fraction() const;
	real_t get_collision_unsafe_fraction() const;
};

class PhysicsServer2DManager : public Object {
	GDCLASS(PhysicsServer2DManager, Object);

	static PhysicsServer2DManager *singleton;

	struct ClassInfo {
		String name;
		Callable create_callback;

		ClassInfo() {}

		ClassInfo(String p_name, Callable p_create_callback) :
				name(p_name),
				create_callback(p_create_callback) {}

		ClassInfo(const ClassInfo &p_ci) :
				name(p_ci.name),
				create_callback(p_ci.create_callback) {}

		void operator=(const ClassInfo &p_ci) {
			name = p_ci.name;
			create_callback = p_ci.create_callback;
		}
	};

	Vector<ClassInfo> physics_2d_servers;
	int default_server_id = -1;
	int default_server_priority = -1;

	void on_servers_changed();

protected:
	static void _bind_methods();

public:
	static const String setting_property_name;

	static PhysicsServer2DManager *get_singleton();

	void register_server(const String &p_name, const Callable &p_create_callback);
	void set_default_server(const String &p_name, int p_priority = 0);
	int find_server_id(const String &p_name);
	int get_servers_count();
	String get_server_name(int p_id);
	PhysicsServer2D *new_default_server();
	PhysicsServer2D *new_server(const String &p_name);

	PhysicsServer2DManager();
	~PhysicsServer2DManager();
};

VARIANT_ENUM_CAST(PhysicsServer2D::ShapeType);
VARIANT_ENUM_CAST(PhysicsServer2D::SpaceParameter);
VARIANT_ENUM_CAST(PhysicsServer2D::AreaParameter);
VARIANT_ENUM_CAST(PhysicsServer2D::AreaSpaceOverrideMode);
VARIANT_ENUM_CAST(PhysicsServer2D::BodyMode);
VARIANT_ENUM_CAST(PhysicsServer2D::BodyParameter);
VARIANT_ENUM_CAST(PhysicsServer2D::BodyDampMode);
VARIANT_ENUM_CAST(PhysicsServer2D::BodyState);
VARIANT_ENUM_CAST(PhysicsServer2D::CCDMode);
VARIANT_ENUM_CAST(PhysicsServer2D::JointParam);
VARIANT_ENUM_CAST(PhysicsServer2D::JointType);
VARIANT_ENUM_CAST(PhysicsServer2D::PinJointParam);
VARIANT_ENUM_CAST(PhysicsServer2D::PinJointFlag);
VARIANT_ENUM_CAST(PhysicsServer2D::DampedSpringParam);
VARIANT_ENUM_CAST(PhysicsServer2D::AreaBodyStatus);
VARIANT_ENUM_CAST(PhysicsServer2D::ProcessInfo);
