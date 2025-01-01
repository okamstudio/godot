/**************************************************************************/
/*  physics_server_3d.h                                                   */
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

#ifndef PHYSICS_SERVER_3D_H
#define PHYSICS_SERVER_3D_H

#ifndef _3D_DISABLED

#include "core/io/resource.h"
#include "core/object/gdvirtual.gen.inc"

class PhysicsDirectSpaceState3D;
template <typename T>
class TypedArray;

class PhysicsDirectBodyState3D : public Object {
	GDCLASS(PhysicsDirectBodyState3D, Object);

protected:
	static void _bind_methods();

public:
	virtual Vector3 get_total_gravity() const = 0;
	virtual real_t get_total_angular_damp() const = 0;
	virtual real_t get_total_linear_damp() const = 0;

	virtual Vector3 get_center_of_mass() const = 0;
	virtual Vector3 get_center_of_mass_local() const = 0;
	virtual Basis get_principal_inertia_axes() const = 0;
	virtual real_t get_inverse_mass() const = 0; // get the mass
	virtual Vector3 get_inverse_inertia() const = 0; // get density of this body space
	virtual Basis get_inverse_inertia_tensor() const = 0; // get density of this body space

	virtual void set_linear_velocity(const Vector3 &p_velocity) = 0;
	virtual Vector3 get_linear_velocity() const = 0;

	virtual void set_angular_velocity(const Vector3 &p_velocity) = 0;
	virtual Vector3 get_angular_velocity() const = 0;

	virtual void set_transform(const Transform3D &p_transform) = 0;
	virtual Transform3D get_transform() const = 0;

	virtual Vector3 get_velocity_at_local_position(const Vector3 &p_position) const = 0;

	virtual void apply_central_impulse(const Vector3 &p_impulse) = 0;
	virtual void apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position = Vector3()) = 0;
	virtual void apply_torque_impulse(const Vector3 &p_impulse) = 0;

	virtual void apply_central_force(const Vector3 &p_force) = 0;
	virtual void apply_force(const Vector3 &p_force, const Vector3 &p_position = Vector3()) = 0;
	virtual void apply_torque(const Vector3 &p_torque) = 0;

	virtual void add_constant_central_force(const Vector3 &p_force) = 0;
	virtual void add_constant_force(const Vector3 &p_force, const Vector3 &p_position = Vector3()) = 0;
	virtual void add_constant_torque(const Vector3 &p_torque) = 0;

	virtual void set_constant_force(const Vector3 &p_force) = 0;
	virtual Vector3 get_constant_force() const = 0;

	virtual void set_constant_torque(const Vector3 &p_torque) = 0;
	virtual Vector3 get_constant_torque() const = 0;

	virtual void set_sleep_state(bool p_sleep) = 0;
	virtual bool is_sleeping() const = 0;

	virtual int get_contact_count() const = 0;

	virtual Vector3 get_contact_local_position(int p_contact_idx) const = 0;
	virtual Vector3 get_contact_local_normal(int p_contact_idx) const = 0;
	virtual Vector3 get_contact_impulse(int p_contact_idx) const = 0;
	virtual int get_contact_local_shape(int p_contact_idx) const = 0;
	virtual Vector3 get_contact_local_velocity_at_position(int p_contact_idx) const = 0;

	virtual RID get_contact_collider(int p_contact_idx) const = 0;
	virtual Vector3 get_contact_collider_position(int p_contact_idx) const = 0;
	virtual ObjectID get_contact_collider_id(int p_contact_idx) const = 0;
	virtual Object *get_contact_collider_object(int p_contact_idx) const;
	virtual int get_contact_collider_shape(int p_contact_idx) const = 0;
	virtual Vector3 get_contact_collider_velocity_at_position(int p_contact_idx) const = 0;

	virtual real_t get_step() const = 0;
	virtual void integrate_forces();

	virtual PhysicsDirectSpaceState3D *get_space_state() = 0;

	PhysicsDirectBodyState3D();
};

class PhysicsRayQueryParameters3D;
class PhysicsPointQueryParameters3D;
class PhysicsShapeQueryParameters3D;

class PhysicsDirectSpaceState3D : public Object {
	GDCLASS(PhysicsDirectSpaceState3D, Object);

private:
	Dictionary _intersect_ray(const Ref<PhysicsRayQueryParameters3D> &p_ray_query);
	TypedArray<Dictionary> _intersect_point(const Ref<PhysicsPointQueryParameters3D> &p_point_query, int p_max_results = 32);
	TypedArray<Dictionary> _intersect_shape(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query, int p_max_results = 32);
	Vector<real_t> _cast_motion(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query);
	TypedArray<Vector3> _collide_shape(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query, int p_max_results = 32);
	Dictionary _get_rest_info(const Ref<PhysicsShapeQueryParameters3D> &p_shape_query);

protected:
	static void _bind_methods();

public:
	struct RayParameters {
		Vector3 from;
		Vector3 to;
		HashSet<RID> exclude;
		uint32_t collision_mask = UINT32_MAX;

		bool collide_with_bodies = true;
		bool collide_with_areas = false;

		bool hit_from_inside = false;
		bool hit_back_faces = true;

		bool pick_ray = false;
	};

	struct RayResult {
		Vector3 position;
		Vector3 normal;
		RID rid;
		ObjectID collider_id;
		Object *collider = nullptr;
		int shape = 0;
		int face_index = -1;
	};

	virtual bool intersect_ray(const RayParameters &p_parameters, RayResult &r_result) = 0;

	struct ShapeResult {
		RID rid;
		ObjectID collider_id;
		Object *collider = nullptr;
		int shape = 0;
	};

	struct PointParameters {
		Vector3 position;
		HashSet<RID> exclude;
		uint32_t collision_mask = UINT32_MAX;

		bool collide_with_bodies = true;
		bool collide_with_areas = false;
	};

	virtual int intersect_point(const PointParameters &p_parameters, ShapeResult *r_results, int p_result_max) = 0;

	struct ShapeParameters {
		RID shape_rid;
		Transform3D transform;
		Vector3 motion;
		real_t margin = 0.0;
		HashSet<RID> exclude;
		uint32_t collision_mask = UINT32_MAX;

		bool collide_with_bodies = true;
		bool collide_with_areas = false;
	};

	struct ShapeRestInfo {
		Vector3 point;
		Vector3 normal;
		RID rid;
		ObjectID collider_id;
		int shape = 0;
		Vector3 linear_velocity; // Velocity at contact point.
	};

	virtual int intersect_shape(const ShapeParameters &p_parameters, ShapeResult *r_results, int p_result_max) = 0;
	virtual bool cast_motion(const ShapeParameters &p_parameters, real_t &p_closest_safe, real_t &p_closest_unsafe, ShapeRestInfo *r_info = nullptr) = 0;
	virtual bool collide_shape(const ShapeParameters &p_parameters, Vector3 *r_results, int p_result_max, int &r_result_count) = 0;
	virtual bool rest_info(const ShapeParameters &p_parameters, ShapeRestInfo *r_info) = 0;

	virtual Vector3 get_closest_point_to_object_volume(RID p_object, const Vector3 p_point) const = 0;

	PhysicsDirectSpaceState3D();
};

class PhysicsServer3DRenderingServerHandler : public Object {
	GDCLASS(PhysicsServer3DRenderingServerHandler, Object)
protected:
	GDVIRTUAL2_REQUIRED(_set_vertex, int, const Vector3 &)
	GDVIRTUAL2_REQUIRED(_set_normal, int, const Vector3 &)
	GDVIRTUAL1_REQUIRED(_set_aabb, const AABB &)

	static void _bind_methods();

public:
	virtual void set_vertex(int p_vertex_id, const Vector3 &p_vertex);
	virtual void set_normal(int p_vertex_id, const Vector3 &p_normal);
	virtual void set_aabb(const AABB &p_aabb);

	virtual ~PhysicsServer3DRenderingServerHandler() {}
};

class PhysicsTestMotionParameters3D;
class PhysicsTestMotionResult3D;

class PhysicsServer3D : public Object {
	GDCLASS(PhysicsServer3D, Object);

	static PhysicsServer3D *singleton;

	virtual bool _body_test_motion(RID p_body, const Ref<PhysicsTestMotionParameters3D> &p_parameters, const Ref<PhysicsTestMotionResult3D> &p_result = Ref<PhysicsTestMotionResult3D>());

protected:
	static void _bind_methods();

public:
	static PhysicsServer3D *get_singleton();

	enum ShapeType {
		SHAPE_WORLD_BOUNDARY, ///< plane:"plane"
		SHAPE_SEPARATION_RAY, ///< float:"length"
		SHAPE_SPHERE, ///< float:"radius"
		SHAPE_BOX, ///< vec3:"extents"
		SHAPE_CAPSULE, ///< dict( float:"radius", float:"height"):capsule
		SHAPE_CYLINDER, ///< dict( float:"radius", float:"height"):cylinder
		SHAPE_CONVEX_POLYGON, ///< array of planes:"planes"
		SHAPE_CONCAVE_POLYGON, ///< vector3 array:"triangles" , or Dictionary with "indices" (int array) and "triangles" (Vector3 array)
		SHAPE_HEIGHTMAP, ///< dict( int:"width", int:"depth",float:"cell_size", float_array:"heights"
		SHAPE_SOFT_BODY, ///< Used internally, can't be created from the physics server.
		SHAPE_CUSTOM, ///< Server-Implementation based custom shape, calling shape_create() with this value will result in an error
	};

	RID shape_create(ShapeType p_shape);

	virtual RID world_boundary_shape_create() = 0;
	virtual RID separation_ray_shape_create() = 0;
	virtual RID sphere_shape_create() = 0;
	virtual RID box_shape_create() = 0;
	virtual RID capsule_shape_create() = 0;
	virtual RID cylinder_shape_create() = 0;
	virtual RID convex_polygon_shape_create() = 0;
	virtual RID concave_polygon_shape_create() = 0;
	virtual RID heightmap_shape_create() = 0;
	virtual RID custom_shape_create() = 0;

	virtual void shape_set_data(RID p_shape, const Variant &p_data) = 0;
	virtual void shape_set_custom_solver_bias(RID p_shape, real_t p_bias) = 0;

	virtual ShapeType shape_get_type(RID p_shape) const = 0;
	virtual Variant shape_get_data(RID p_shape) const = 0;

	virtual void shape_set_margin(RID p_shape, real_t p_margin) = 0;
	virtual real_t shape_get_margin(RID p_shape) const = 0;

	virtual real_t shape_get_custom_solver_bias(RID p_shape) const = 0;

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
		SPACE_PARAM_SOLVER_ITERATIONS,
	};

	virtual void space_set_param(RID p_space, SpaceParameter p_param, real_t p_value) = 0;
	virtual real_t space_get_param(RID p_space, SpaceParameter p_param) const = 0;

	// this function only works on physics process, errors and returns null otherwise
	virtual PhysicsDirectSpaceState3D *space_get_direct_state(RID p_space) = 0;

	virtual void space_set_debug_contacts(RID p_space, int p_max_contacts) = 0;
	virtual Vector<Vector3> space_get_contacts(RID p_space) const = 0;
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
		AREA_PARAM_PRIORITY,
		AREA_PARAM_WIND_FORCE_MAGNITUDE,
		AREA_PARAM_WIND_SOURCE,
		AREA_PARAM_WIND_DIRECTION,
		AREA_PARAM_WIND_ATTENUATION_FACTOR,
	};

	virtual RID area_create() = 0;

	virtual void area_set_space(RID p_area, RID p_space) = 0;
	virtual RID area_get_space(RID p_area) const = 0;

	enum AreaSpaceOverrideMode {
		AREA_SPACE_OVERRIDE_DISABLED,
		AREA_SPACE_OVERRIDE_COMBINE,
		AREA_SPACE_OVERRIDE_COMBINE_REPLACE,
		AREA_SPACE_OVERRIDE_REPLACE,
		AREA_SPACE_OVERRIDE_REPLACE_COMBINE
	};

	virtual void area_add_shape(RID p_area, RID p_shape, const Transform3D &p_transform = Transform3D(), bool p_disabled = false) = 0;
	virtual void area_set_shape(RID p_area, int p_shape_idx, RID p_shape) = 0;
	virtual void area_set_shape_transform(RID p_area, int p_shape_idx, const Transform3D &p_transform) = 0;

	virtual int area_get_shape_count(RID p_area) const = 0;
	virtual RID area_get_shape(RID p_area, int p_shape_idx) const = 0;
	virtual Transform3D area_get_shape_transform(RID p_area, int p_shape_idx) const = 0;

	virtual void area_remove_shape(RID p_area, int p_shape_idx) = 0;
	virtual void area_clear_shapes(RID p_area) = 0;

	virtual void area_set_shape_disabled(RID p_area, int p_shape_idx, bool p_disabled) = 0;

	virtual void area_attach_object_instance_id(RID p_area, ObjectID p_id) = 0;
	virtual ObjectID area_get_object_instance_id(RID p_area) const = 0;

	virtual void area_set_param(RID p_area, AreaParameter p_param, const Variant &p_value) = 0;
	virtual void area_set_transform(RID p_area, const Transform3D &p_transform) = 0;

	virtual Variant area_get_param(RID p_parea, AreaParameter p_param) const = 0;
	virtual Transform3D area_get_transform(RID p_area) const = 0;

	virtual void area_set_collision_layer(RID p_area, uint32_t p_layer) = 0;
	virtual uint32_t area_get_collision_layer(RID p_area) const = 0;

	virtual void area_set_collision_mask(RID p_area, uint32_t p_mask) = 0;
	virtual uint32_t area_get_collision_mask(RID p_area) const = 0;

	virtual void area_set_monitorable(RID p_area, bool p_monitorable) = 0;

	virtual void area_set_monitor_callback(RID p_area, const Callable &p_callback) = 0;
	virtual void area_set_area_monitor_callback(RID p_area, const Callable &p_callback) = 0;

	virtual void area_set_ray_pickable(RID p_area, bool p_enable) = 0;

	/* BODY API */

	//missing ccd?

	enum BodyMode {
		BODY_MODE_STATIC,
		BODY_MODE_KINEMATIC,
		BODY_MODE_RIGID,
		BODY_MODE_RIGID_LINEAR,
	};

	enum BodyDampMode {
		BODY_DAMP_MODE_COMBINE,
		BODY_DAMP_MODE_REPLACE,
	};

	virtual RID body_create() = 0;

	virtual void body_set_space(RID p_body, RID p_space) = 0;
	virtual RID body_get_space(RID p_body) const = 0;

	virtual void body_set_mode(RID p_body, BodyMode p_mode) = 0;
	virtual BodyMode body_get_mode(RID p_body) const = 0;

	virtual void body_add_shape(RID p_body, RID p_shape, const Transform3D &p_transform = Transform3D(), bool p_disabled = false) = 0;
	virtual void body_set_shape(RID p_body, int p_shape_idx, RID p_shape) = 0;
	virtual void body_set_shape_transform(RID p_body, int p_shape_idx, const Transform3D &p_transform) = 0;

	virtual int body_get_shape_count(RID p_body) const = 0;
	virtual RID body_get_shape(RID p_body, int p_shape_idx) const = 0;
	virtual Transform3D body_get_shape_transform(RID p_body, int p_shape_idx) const = 0;

	virtual void body_remove_shape(RID p_body, int p_shape_idx) = 0;
	virtual void body_clear_shapes(RID p_body) = 0;

	virtual void body_set_shape_disabled(RID p_body, int p_shape_idx, bool p_disabled) = 0;

	virtual void body_attach_object_instance_id(RID p_body, ObjectID p_id) = 0;
	virtual ObjectID body_get_object_instance_id(RID p_body) const = 0;

	virtual void body_set_enable_continuous_collision_detection(RID p_body, bool p_enable) = 0;
	virtual bool body_is_continuous_collision_detection_enabled(RID p_body) const = 0;

	virtual void body_set_collision_layer(RID p_body, uint32_t p_layer) = 0;
	virtual uint32_t body_get_collision_layer(RID p_body) const = 0;

	virtual void body_set_collision_mask(RID p_body, uint32_t p_mask) = 0;
	virtual uint32_t body_get_collision_mask(RID p_body) const = 0;

	virtual void body_set_collision_priority(RID p_body, real_t p_priority) = 0;
	virtual real_t body_get_collision_priority(RID p_body) const = 0;

	virtual void body_set_user_flags(RID p_body, uint32_t p_flags) = 0;
	virtual uint32_t body_get_user_flags(RID p_body) const = 0;

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

	virtual void body_set_param(RID p_body, BodyParameter p_param, const Variant &p_value) = 0;
	virtual Variant body_get_param(RID p_body, BodyParameter p_param) const = 0;

	virtual void body_reset_mass_properties(RID p_body) = 0;

	//state
	enum BodyState {
		BODY_STATE_TRANSFORM,
		BODY_STATE_LINEAR_VELOCITY,
		BODY_STATE_ANGULAR_VELOCITY,
		BODY_STATE_SLEEPING,
		BODY_STATE_CAN_SLEEP
	};

	virtual void body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) = 0;
	virtual Variant body_get_state(RID p_body, BodyState p_state) const = 0;

	virtual void body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) = 0;
	virtual void body_apply_impulse(RID p_body, const Vector3 &p_impulse, const Vector3 &p_position = Vector3()) = 0;
	virtual void body_apply_torque_impulse(RID p_body, const Vector3 &p_impulse) = 0;

	virtual void body_apply_central_force(RID p_body, const Vector3 &p_force) = 0;
	virtual void body_apply_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position = Vector3()) = 0;
	virtual void body_apply_torque(RID p_body, const Vector3 &p_torque) = 0;

	virtual void body_add_constant_central_force(RID p_body, const Vector3 &p_force) = 0;
	virtual void body_add_constant_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position = Vector3()) = 0;
	virtual void body_add_constant_torque(RID p_body, const Vector3 &p_torque) = 0;

	virtual void body_set_constant_force(RID p_body, const Vector3 &p_force) = 0;
	virtual Vector3 body_get_constant_force(RID p_body) const = 0;

	virtual void body_set_constant_torque(RID p_body, const Vector3 &p_torque) = 0;
	virtual Vector3 body_get_constant_torque(RID p_body) const = 0;

	virtual void body_set_axis_velocity(RID p_body, const Vector3 &p_axis_velocity) = 0;

	enum BodyAxis {
		BODY_AXIS_LINEAR_X = 1 << 0,
		BODY_AXIS_LINEAR_Y = 1 << 1,
		BODY_AXIS_LINEAR_Z = 1 << 2,
		BODY_AXIS_ANGULAR_X = 1 << 3,
		BODY_AXIS_ANGULAR_Y = 1 << 4,
		BODY_AXIS_ANGULAR_Z = 1 << 5
	};

	virtual void body_set_axis_lock(RID p_body, BodyAxis p_axis, bool p_lock) = 0;
	virtual bool body_is_axis_locked(RID p_body, BodyAxis p_axis) const = 0;

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

	virtual void body_set_ray_pickable(RID p_body, bool p_enable) = 0;

	// this function only works on physics process, errors and returns null otherwise
	virtual PhysicsDirectBodyState3D *body_get_direct_state(RID p_body) = 0;

	struct MotionParameters {
		Transform3D from;
		Vector3 motion;
		real_t margin = 0.001;
		int max_collisions = 1;
		bool collide_separation_ray = false;
		HashSet<RID> exclude_bodies;
		HashSet<ObjectID> exclude_objects;
		bool recovery_as_collision = false;

		MotionParameters() {}

		MotionParameters(const Transform3D &p_from, const Vector3 &p_motion, real_t p_margin = 0.001) :
				from(p_from),
				motion(p_motion),
				margin(p_margin) {}
	};

	struct MotionCollision {
		Vector3 position;
		Vector3 normal;
		Vector3 collider_velocity;
		Vector3 collider_angular_velocity;
		real_t depth = 0.0;
		int local_shape = 0;
		ObjectID collider_id;
		RID collider;
		int collider_shape = 0;

		real_t get_angle(Vector3 p_up_direction) const {
			return Math::acos(normal.dot(p_up_direction));
		}
	};

	struct MotionResult {
		Vector3 travel;
		Vector3 remainder;
		real_t collision_depth = 0.0;
		real_t collision_safe_fraction = 0.0;
		real_t collision_unsafe_fraction = 0.0;

		static const int MAX_COLLISIONS = 32;
		MotionCollision collisions[MAX_COLLISIONS];
		int collision_count = 0;
	};

	virtual bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result = nullptr) = 0;

	/* SOFT BODY */

	virtual RID soft_body_create() = 0;

	virtual void soft_body_update_rendering_server(RID p_body, PhysicsServer3DRenderingServerHandler *p_rendering_server_handler) = 0;

	virtual void soft_body_set_space(RID p_body, RID p_space) = 0;
	virtual RID soft_body_get_space(RID p_body) const = 0;

	virtual void soft_body_set_mesh(RID p_body, RID p_mesh) = 0;

	virtual AABB soft_body_get_bounds(RID p_body) const = 0;

	virtual void soft_body_set_collision_layer(RID p_body, uint32_t p_layer) = 0;
	virtual uint32_t soft_body_get_collision_layer(RID p_body) const = 0;

	virtual void soft_body_set_collision_mask(RID p_body, uint32_t p_mask) = 0;
	virtual uint32_t soft_body_get_collision_mask(RID p_body) const = 0;

	virtual void soft_body_add_collision_exception(RID p_body, RID p_body_b) = 0;
	virtual void soft_body_remove_collision_exception(RID p_body, RID p_body_b) = 0;
	virtual void soft_body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) = 0;

	virtual void soft_body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) = 0;
	virtual Variant soft_body_get_state(RID p_body, BodyState p_state) const = 0;

	virtual void soft_body_set_transform(RID p_body, const Transform3D &p_transform) = 0;

	virtual void soft_body_set_ray_pickable(RID p_body, bool p_enable) = 0;

	virtual void soft_body_set_simulation_precision(RID p_body, int p_simulation_precision) = 0;
	virtual int soft_body_get_simulation_precision(RID p_body) const = 0;

	virtual void soft_body_set_total_mass(RID p_body, real_t p_total_mass) = 0;
	virtual real_t soft_body_get_total_mass(RID p_body) const = 0;

	virtual void soft_body_set_linear_stiffness(RID p_body, real_t p_stiffness) = 0;
	virtual real_t soft_body_get_linear_stiffness(RID p_body) const = 0;

	virtual void soft_body_set_pressure_coefficient(RID p_body, real_t p_pressure_coefficient) = 0;
	virtual real_t soft_body_get_pressure_coefficient(RID p_body) const = 0;

	virtual void soft_body_set_damping_coefficient(RID p_body, real_t p_damping_coefficient) = 0;
	virtual real_t soft_body_get_damping_coefficient(RID p_body) const = 0;

	virtual void soft_body_set_drag_coefficient(RID p_body, real_t p_drag_coefficient) = 0;
	virtual real_t soft_body_get_drag_coefficient(RID p_body) const = 0;

	virtual void soft_body_move_point(RID p_body, int p_point_index, const Vector3 &p_global_position) = 0;
	virtual Vector3 soft_body_get_point_global_position(RID p_body, int p_point_index) const = 0;

	virtual void soft_body_remove_all_pinned_points(RID p_body) = 0;
	virtual void soft_body_pin_point(RID p_body, int p_point_index, bool p_pin) = 0;
	virtual bool soft_body_is_point_pinned(RID p_body, int p_point_index) const = 0;

	/* JOINT API */

	enum JointType {
		JOINT_TYPE_PIN,
		JOINT_TYPE_HINGE,
		JOINT_TYPE_SLIDER,
		JOINT_TYPE_CONE_TWIST,
		JOINT_TYPE_6DOF,
		JOINT_TYPE_MAX,

	};

	virtual RID joint_create() = 0;

	virtual void joint_clear(RID p_joint) = 0;

	virtual JointType joint_get_type(RID p_joint) const = 0;

	virtual void joint_set_solver_priority(RID p_joint, int p_priority) = 0;
	virtual int joint_get_solver_priority(RID p_joint) const = 0;

	virtual void joint_disable_collisions_between_bodies(RID p_joint, bool p_disable) = 0;
	virtual bool joint_is_disabled_collisions_between_bodies(RID p_joint) const = 0;

	virtual void joint_make_pin(RID p_joint, RID p_body_A, const Vector3 &p_local_A, RID p_body_B, const Vector3 &p_local_B) = 0;

	enum PinJointParam {
		PIN_JOINT_BIAS,
		PIN_JOINT_DAMPING,
		PIN_JOINT_IMPULSE_CLAMP
	};

	virtual void pin_joint_set_param(RID p_joint, PinJointParam p_param, real_t p_value) = 0;
	virtual real_t pin_joint_get_param(RID p_joint, PinJointParam p_param) const = 0;

	virtual void pin_joint_set_local_a(RID p_joint, const Vector3 &p_A) = 0;
	virtual Vector3 pin_joint_get_local_a(RID p_joint) const = 0;

	virtual void pin_joint_set_local_b(RID p_joint, const Vector3 &p_B) = 0;
	virtual Vector3 pin_joint_get_local_b(RID p_joint) const = 0;

	enum HingeJointParam {
		HINGE_JOINT_BIAS,
		HINGE_JOINT_LIMIT_UPPER,
		HINGE_JOINT_LIMIT_LOWER,
		HINGE_JOINT_LIMIT_BIAS,
		HINGE_JOINT_LIMIT_SOFTNESS,
		HINGE_JOINT_LIMIT_RELAXATION,
		HINGE_JOINT_MOTOR_TARGET_VELOCITY,
		HINGE_JOINT_MOTOR_MAX_IMPULSE,
		HINGE_JOINT_MAX
	};

	enum HingeJointFlag {
		HINGE_JOINT_FLAG_USE_LIMIT,
		HINGE_JOINT_FLAG_ENABLE_MOTOR,
		HINGE_JOINT_FLAG_MAX
	};

	virtual void joint_make_hinge(RID p_joint, RID p_body_A, const Transform3D &p_hinge_A, RID p_body_B, const Transform3D &p_hinge_B) = 0;
	virtual void joint_make_hinge_simple(RID p_joint, RID p_body_A, const Vector3 &p_pivot_A, const Vector3 &p_axis_A, RID p_body_B, const Vector3 &p_pivot_B, const Vector3 &p_axis_B) = 0;

	virtual void hinge_joint_set_param(RID p_joint, HingeJointParam p_param, real_t p_value) = 0;
	virtual real_t hinge_joint_get_param(RID p_joint, HingeJointParam p_param) const = 0;

	virtual void hinge_joint_set_flag(RID p_joint, HingeJointFlag p_flag, bool p_enabled) = 0;
	virtual bool hinge_joint_get_flag(RID p_joint, HingeJointFlag p_flag) const = 0;

	enum SliderJointParam {
		SLIDER_JOINT_LINEAR_LIMIT_UPPER,
		SLIDER_JOINT_LINEAR_LIMIT_LOWER,
		SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS,
		SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION,
		SLIDER_JOINT_LINEAR_LIMIT_DAMPING,
		SLIDER_JOINT_LINEAR_MOTION_SOFTNESS,
		SLIDER_JOINT_LINEAR_MOTION_RESTITUTION,
		SLIDER_JOINT_LINEAR_MOTION_DAMPING,
		SLIDER_JOINT_LINEAR_ORTHOGONAL_SOFTNESS,
		SLIDER_JOINT_LINEAR_ORTHOGONAL_RESTITUTION,
		SLIDER_JOINT_LINEAR_ORTHOGONAL_DAMPING,

		SLIDER_JOINT_ANGULAR_LIMIT_UPPER,
		SLIDER_JOINT_ANGULAR_LIMIT_LOWER,
		SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS,
		SLIDER_JOINT_ANGULAR_LIMIT_RESTITUTION,
		SLIDER_JOINT_ANGULAR_LIMIT_DAMPING,
		SLIDER_JOINT_ANGULAR_MOTION_SOFTNESS,
		SLIDER_JOINT_ANGULAR_MOTION_RESTITUTION,
		SLIDER_JOINT_ANGULAR_MOTION_DAMPING,
		SLIDER_JOINT_ANGULAR_ORTHOGONAL_SOFTNESS,
		SLIDER_JOINT_ANGULAR_ORTHOGONAL_RESTITUTION,
		SLIDER_JOINT_ANGULAR_ORTHOGONAL_DAMPING,
		SLIDER_JOINT_MAX

	};

	virtual void joint_make_slider(RID p_joint, RID p_body_A, const Transform3D &p_local_frame_A, RID p_body_B, const Transform3D &p_local_frame_B) = 0; //reference frame is A

	virtual void slider_joint_set_param(RID p_joint, SliderJointParam p_param, real_t p_value) = 0;
	virtual real_t slider_joint_get_param(RID p_joint, SliderJointParam p_param) const = 0;

	enum ConeTwistJointParam {
		CONE_TWIST_JOINT_SWING_SPAN,
		CONE_TWIST_JOINT_TWIST_SPAN,
		CONE_TWIST_JOINT_BIAS,
		CONE_TWIST_JOINT_SOFTNESS,
		CONE_TWIST_JOINT_RELAXATION,
		CONE_TWIST_MAX
	};

	virtual void joint_make_cone_twist(RID p_joint, RID p_body_A, const Transform3D &p_local_frame_A, RID p_body_B, const Transform3D &p_local_frame_B) = 0; //reference frame is A

	virtual void cone_twist_joint_set_param(RID p_joint, ConeTwistJointParam p_param, real_t p_value) = 0;
	virtual real_t cone_twist_joint_get_param(RID p_joint, ConeTwistJointParam p_param) const = 0;

	enum G6DOFJointAxisParam {
		G6DOF_JOINT_LINEAR_LOWER_LIMIT,
		G6DOF_JOINT_LINEAR_UPPER_LIMIT,
		G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS,
		G6DOF_JOINT_LINEAR_RESTITUTION,
		G6DOF_JOINT_LINEAR_DAMPING,
		G6DOF_JOINT_LINEAR_MOTOR_TARGET_VELOCITY,
		G6DOF_JOINT_LINEAR_MOTOR_FORCE_LIMIT,
		G6DOF_JOINT_LINEAR_SPRING_STIFFNESS,
		G6DOF_JOINT_LINEAR_SPRING_DAMPING,
		G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT,
		G6DOF_JOINT_ANGULAR_LOWER_LIMIT,
		G6DOF_JOINT_ANGULAR_UPPER_LIMIT,
		G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS,
		G6DOF_JOINT_ANGULAR_DAMPING,
		G6DOF_JOINT_ANGULAR_RESTITUTION,
		G6DOF_JOINT_ANGULAR_FORCE_LIMIT,
		G6DOF_JOINT_ANGULAR_ERP,
		G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY,
		G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT,
		G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS,
		G6DOF_JOINT_ANGULAR_SPRING_DAMPING,
		G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT,
		G6DOF_JOINT_MAX
	};

	enum G6DOFJointAxisFlag {
		G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT,
		G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT,
		G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING,
		G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING,
		G6DOF_JOINT_FLAG_ENABLE_MOTOR,
		G6DOF_JOINT_FLAG_ENABLE_LINEAR_MOTOR,
		G6DOF_JOINT_FLAG_MAX
	};

	virtual void joint_make_generic_6dof(RID p_joint, RID p_body_A, const Transform3D &p_local_frame_A, RID p_body_B, const Transform3D &p_local_frame_B) = 0; //reference frame is A

	virtual void generic_6dof_joint_set_param(RID p_joint, Vector3::Axis, G6DOFJointAxisParam p_param, real_t p_value) = 0;
	virtual real_t generic_6dof_joint_get_param(RID p_joint, Vector3::Axis, G6DOFJointAxisParam p_param) const = 0;

	virtual void generic_6dof_joint_set_flag(RID p_joint, Vector3::Axis, G6DOFJointAxisFlag p_flag, bool p_enable) = 0;
	virtual bool generic_6dof_joint_get_flag(RID p_joint, Vector3::Axis, G6DOFJointAxisFlag p_flag) const = 0;

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

	PhysicsServer3D();
	~PhysicsServer3D();
};

class PhysicsRayQueryParameters3D : public RefCounted {
	GDCLASS(PhysicsRayQueryParameters3D, RefCounted);

	PhysicsDirectSpaceState3D::RayParameters parameters;

protected:
	static void _bind_methods();

public:
	static Ref<PhysicsRayQueryParameters3D> create(Vector3 p_from, Vector3 p_to, uint32_t p_mask, const TypedArray<RID> &p_exclude);
	const PhysicsDirectSpaceState3D::RayParameters &get_parameters() const { return parameters; }

	void set_from(const Vector3 &p_from) { parameters.from = p_from; }
	const Vector3 &get_from() const { return parameters.from; }

	void set_to(const Vector3 &p_to) { parameters.to = p_to; }
	const Vector3 &get_to() const { return parameters.to; }

	void set_collision_mask(uint32_t p_mask) { parameters.collision_mask = p_mask; }
	uint32_t get_collision_mask() const { return parameters.collision_mask; }

	void set_collide_with_bodies(bool p_enable) { parameters.collide_with_bodies = p_enable; }
	bool is_collide_with_bodies_enabled() const { return parameters.collide_with_bodies; }

	void set_collide_with_areas(bool p_enable) { parameters.collide_with_areas = p_enable; }
	bool is_collide_with_areas_enabled() const { return parameters.collide_with_areas; }

	void set_hit_from_inside(bool p_enable) { parameters.hit_from_inside = p_enable; }
	bool is_hit_from_inside_enabled() const { return parameters.hit_from_inside; }

	void set_hit_back_faces(bool p_enable) { parameters.hit_back_faces = p_enable; }
	bool is_hit_back_faces_enabled() const { return parameters.hit_back_faces; }

	void set_exclude(const TypedArray<RID> &p_exclude);
	TypedArray<RID> get_exclude() const;
};

class PhysicsPointQueryParameters3D : public RefCounted {
	GDCLASS(PhysicsPointQueryParameters3D, RefCounted);

	PhysicsDirectSpaceState3D::PointParameters parameters;

protected:
	static void _bind_methods();

public:
	const PhysicsDirectSpaceState3D::PointParameters &get_parameters() const { return parameters; }

	void set_position(const Vector3 &p_position) { parameters.position = p_position; }
	const Vector3 &get_position() const { return parameters.position; }

	void set_collision_mask(uint32_t p_mask) { parameters.collision_mask = p_mask; }
	uint32_t get_collision_mask() const { return parameters.collision_mask; }

	void set_collide_with_bodies(bool p_enable) { parameters.collide_with_bodies = p_enable; }
	bool is_collide_with_bodies_enabled() const { return parameters.collide_with_bodies; }

	void set_collide_with_areas(bool p_enable) { parameters.collide_with_areas = p_enable; }
	bool is_collide_with_areas_enabled() const { return parameters.collide_with_areas; }

	void set_exclude(const TypedArray<RID> &p_exclude);
	TypedArray<RID> get_exclude() const;
};

class PhysicsShapeQueryParameters3D : public RefCounted {
	GDCLASS(PhysicsShapeQueryParameters3D, RefCounted);

	PhysicsDirectSpaceState3D::ShapeParameters parameters;

	Ref<Resource> shape_ref;

protected:
	static void _bind_methods();

public:
	const PhysicsDirectSpaceState3D::ShapeParameters &get_parameters() const { return parameters; }

	void set_shape(const Ref<Resource> &p_shape_ref);
	Ref<Resource> get_shape() const { return shape_ref; }

	void set_shape_rid(const RID &p_shape);
	RID get_shape_rid() const { return parameters.shape_rid; }

	void set_transform(const Transform3D &p_transform) { parameters.transform = p_transform; }
	const Transform3D &get_transform() const { return parameters.transform; }

	void set_motion(const Vector3 &p_motion) { parameters.motion = p_motion; }
	const Vector3 &get_motion() const { return parameters.motion; }

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

class PhysicsTestMotionParameters3D : public RefCounted {
	GDCLASS(PhysicsTestMotionParameters3D, RefCounted);

	PhysicsServer3D::MotionParameters parameters;

protected:
	static void _bind_methods();

public:
	const PhysicsServer3D::MotionParameters &get_parameters() const { return parameters; }

	const Transform3D &get_from() const { return parameters.from; }
	void set_from(const Transform3D &p_from) { parameters.from = p_from; }

	const Vector3 &get_motion() const { return parameters.motion; }
	void set_motion(const Vector3 &p_motion) { parameters.motion = p_motion; }

	real_t get_margin() const { return parameters.margin; }
	void set_margin(real_t p_margin) { parameters.margin = p_margin; }

	int get_max_collisions() const { return parameters.max_collisions; }
	void set_max_collisions(int p_max_collisions) { parameters.max_collisions = p_max_collisions; }

	bool is_collide_separation_ray_enabled() const { return parameters.collide_separation_ray; }
	void set_collide_separation_ray_enabled(bool p_enabled) { parameters.collide_separation_ray = p_enabled; }

	TypedArray<RID> get_exclude_bodies() const;
	void set_exclude_bodies(const TypedArray<RID> &p_exclude);

	TypedArray<uint64_t> get_exclude_objects() const;
	void set_exclude_objects(const TypedArray<uint64_t> &p_exclude);

	bool is_recovery_as_collision_enabled() const { return parameters.recovery_as_collision; }
	void set_recovery_as_collision_enabled(bool p_enabled) { parameters.recovery_as_collision = p_enabled; }
};

class PhysicsTestMotionResult3D : public RefCounted {
	GDCLASS(PhysicsTestMotionResult3D, RefCounted);

	PhysicsServer3D::MotionResult result;

protected:
	static void _bind_methods();

public:
	PhysicsServer3D::MotionResult *get_result_ptr() { return &result; }

	Vector3 get_travel() const;
	Vector3 get_remainder() const;
	real_t get_collision_safe_fraction() const;
	real_t get_collision_unsafe_fraction() const;

	int get_collision_count() const;

	Vector3 get_collision_point(int p_collision_index = 0) const;
	Vector3 get_collision_normal(int p_collision_index = 0) const;
	Vector3 get_collider_velocity(int p_collision_index = 0) const;
	ObjectID get_collider_id(int p_collision_index = 0) const;
	RID get_collider_rid(int p_collision_index = 0) const;
	Object *get_collider(int p_collision_index = 0) const;
	int get_collider_shape(int p_collision_index = 0) const;
	int get_collision_local_shape(int p_collision_index = 0) const;
	real_t get_collision_depth(int p_collision_index = 0) const;
};

class PhysicsServer3DManager : public Object {
	GDCLASS(PhysicsServer3DManager, Object);

	static PhysicsServer3DManager *singleton;

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

	Vector<ClassInfo> physics_servers;
	int default_server_id = -1;
	int default_server_priority = -1;

	void on_servers_changed();

protected:
	static void _bind_methods();

public:
	static const String setting_property_name;

	static PhysicsServer3DManager *get_singleton();

	void register_server(const String &p_name, const Callable &p_create_callback);
	void set_default_server(const String &p_name, int p_priority = 0);
	int find_server_id(const String &p_name);
	int get_servers_count();
	String get_server_name(int p_id);
	PhysicsServer3D *new_default_server();
	PhysicsServer3D *new_server(const String &p_name);

	PhysicsServer3DManager();
	~PhysicsServer3DManager();
};

VARIANT_ENUM_CAST(PhysicsServer3D::ShapeType);
VARIANT_ENUM_CAST(PhysicsServer3D::SpaceParameter);
VARIANT_ENUM_CAST(PhysicsServer3D::AreaParameter);
VARIANT_ENUM_CAST(PhysicsServer3D::AreaSpaceOverrideMode);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyMode);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyParameter);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyDampMode);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyState);
VARIANT_ENUM_CAST(PhysicsServer3D::BodyAxis);
VARIANT_ENUM_CAST(PhysicsServer3D::PinJointParam);
VARIANT_ENUM_CAST(PhysicsServer3D::JointType);
VARIANT_ENUM_CAST(PhysicsServer3D::HingeJointParam);
VARIANT_ENUM_CAST(PhysicsServer3D::HingeJointFlag);
VARIANT_ENUM_CAST(PhysicsServer3D::SliderJointParam);
VARIANT_ENUM_CAST(PhysicsServer3D::ConeTwistJointParam);
VARIANT_ENUM_CAST(PhysicsServer3D::G6DOFJointAxisParam);
VARIANT_ENUM_CAST(PhysicsServer3D::G6DOFJointAxisFlag);
VARIANT_ENUM_CAST(PhysicsServer3D::AreaBodyStatus);
VARIANT_ENUM_CAST(PhysicsServer3D::ProcessInfo);

#endif // _3D_DISABLED

#endif // PHYSICS_SERVER_3D_H
