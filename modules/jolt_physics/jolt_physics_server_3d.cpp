/**************************************************************************/
/*  jolt_physics_server_3d.cpp                                            */
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

#include "jolt_physics_server_3d.h"

#include "joints/jolt_cone_twist_joint_3d.h"
#include "joints/jolt_generic_6dof_joint_3d.h"
#include "joints/jolt_hinge_joint_3d.h"
#include "joints/jolt_joint_3d.h"
#include "joints/jolt_pin_joint_3d.h"
#include "joints/jolt_slider_joint_3d.h"
#include "objects/jolt_area_3d.h"
#include "objects/jolt_body_3d.h"
#include "objects/jolt_soft_body_3d.h"
#include "servers/physics_server_3d_wrap_mt.h"
#include "shapes/jolt_box_shape_3d.h"
#include "shapes/jolt_capsule_shape_3d.h"
#include "shapes/jolt_concave_polygon_shape_3d.h"
#include "shapes/jolt_convex_polygon_shape_3d.h"
#include "shapes/jolt_cylinder_shape_3d.h"
#include "shapes/jolt_height_map_shape_3d.h"
#include "shapes/jolt_separation_ray_shape_3d.h"
#include "shapes/jolt_sphere_shape_3d.h"
#include "shapes/jolt_world_boundary_shape_3d.h"
#include "spaces/jolt_job_system.h"
#include "spaces/jolt_physics_direct_space_state_3d.h"
#include "spaces/jolt_space_3d.h"

JoltPhysicsServer3D::JoltPhysicsServer3D(bool p_on_separate_thread) :
		on_separate_thread(p_on_separate_thread) {
	singleton = this;
}

JoltPhysicsServer3D::~JoltPhysicsServer3D() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

RID JoltPhysicsServer3D::world_boundary_shape_create() {
	JoltShape3D *shape = memnew(JoltWorldBoundaryShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::separation_ray_shape_create() {
	JoltShape3D *shape = memnew(JoltSeparationRayShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::sphere_shape_create() {
	JoltShape3D *shape = memnew(JoltSphereShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::box_shape_create() {
	JoltShape3D *shape = memnew(JoltBoxShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::capsule_shape_create() {
	JoltShape3D *shape = memnew(JoltCapsuleShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::cylinder_shape_create() {
	JoltShape3D *shape = memnew(JoltCylinderShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::convex_polygon_shape_create() {
	JoltShape3D *shape = memnew(JoltConvexPolygonShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::concave_polygon_shape_create() {
	JoltShape3D *shape = memnew(JoltConcavePolygonShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::heightmap_shape_create() {
	JoltShape3D *shape = memnew(JoltHeightMapShape3D);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID JoltPhysicsServer3D::custom_shape_create() {
	ERR_FAIL_V_MSG(RID(), "Custom shapes are not supported.");
}

void JoltPhysicsServer3D::shape_set_data(RID p_shape, const Variant &p_data) {
	JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	shape->set_data(p_data);
}

Variant JoltPhysicsServer3D::shape_get_data(RID p_shape) const {
	const JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, Variant());

	return shape->get_data();
}

void JoltPhysicsServer3D::shape_set_custom_solver_bias(RID p_shape, real_t p_bias) {
	JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	shape->set_solver_bias((float)p_bias);
}

PhysicsServer3D::ShapeType JoltPhysicsServer3D::shape_get_type(RID p_shape) const {
	const JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, SHAPE_CUSTOM);

	return shape->get_type();
}

void JoltPhysicsServer3D::shape_set_margin(RID p_shape, real_t p_margin) {
	JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	shape->set_margin((float)p_margin);
}

real_t JoltPhysicsServer3D::shape_get_margin(RID p_shape) const {
	const JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, 0.0);

	return (real_t)shape->get_margin();
}

real_t JoltPhysicsServer3D::shape_get_custom_solver_bias(RID p_shape) const {
	const JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, 0.0);

	return (real_t)shape->get_solver_bias();
}

RID JoltPhysicsServer3D::space_create() {
	JoltSpace3D *space = memnew(JoltSpace3D(job_system));
	RID rid = space_owner.make_rid(space);
	space->set_rid(rid);

	const RID default_area_rid = area_create();
	JoltArea3D *default_area = area_owner.get_or_null(default_area_rid);
	ERR_FAIL_NULL_V(default_area, RID());
	space->set_default_area(default_area);
	default_area->set_space(space);

	return rid;
}

void JoltPhysicsServer3D::space_set_active(RID p_space, bool p_active) {
	JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);

	if (p_active) {
		space->set_active(true);
		active_spaces.insert(space);
	} else {
		space->set_active(false);
		active_spaces.erase(space);
	}
}

bool JoltPhysicsServer3D::space_is_active(RID p_space) const {
	JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, false);

	return active_spaces.has(space);
}

void JoltPhysicsServer3D::space_set_param(RID p_space, SpaceParameter p_param, real_t p_value) {
	JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);

	space->set_param(p_param, (double)p_value);
}

real_t JoltPhysicsServer3D::space_get_param(RID p_space, SpaceParameter p_param) const {
	const JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, 0.0);

	return (real_t)space->get_param(p_param);
}

PhysicsDirectSpaceState3D *JoltPhysicsServer3D::space_get_direct_state(RID p_space) {
	JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, nullptr);
	ERR_FAIL_COND_V_MSG((on_separate_thread && !doing_sync) || space->is_stepping(), nullptr, "Space state is inaccessible right now, wait for iteration or physics process notification.");

	return space->get_direct_state();
}

void JoltPhysicsServer3D::space_set_debug_contacts(RID p_space, int p_max_contacts) {
#ifdef DEBUG_ENABLED
	JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);

	space->set_max_debug_contacts(p_max_contacts);
#endif
}

PackedVector3Array JoltPhysicsServer3D::space_get_contacts(RID p_space) const {
#ifdef DEBUG_ENABLED
	JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, PackedVector3Array());

	return space->get_debug_contacts();
#else
	return PackedVector3Array();
#endif
}

int JoltPhysicsServer3D::space_get_contact_count(RID p_space) const {
#ifdef DEBUG_ENABLED
	JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, 0);

	return space->get_debug_contact_count();
#else
	return 0;
#endif
}

RID JoltPhysicsServer3D::area_create() {
	JoltArea3D *area = memnew(JoltArea3D);
	RID rid = area_owner.make_rid(area);
	area->set_rid(rid);
	return rid;
}

void JoltPhysicsServer3D::area_set_space(RID p_area, RID p_space) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	JoltSpace3D *space = nullptr;

	if (p_space.is_valid()) {
		space = space_owner.get_or_null(p_space);
		ERR_FAIL_NULL(space);
	}

	area->set_space(space);
}

RID JoltPhysicsServer3D::area_get_space(RID p_area) const {
	const JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, RID());

	const JoltSpace3D *space = area->get_space();

	if (space == nullptr) {
		return RID();
	}

	return space->get_rid();
}

void JoltPhysicsServer3D::area_add_shape(RID p_area, RID p_shape, const Transform3D &p_transform, bool p_disabled) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	area->add_shape(shape, p_transform, p_disabled);
}

void JoltPhysicsServer3D::area_set_shape(RID p_area, int p_shape_idx, RID p_shape) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	area->set_shape(p_shape_idx, shape);
}

RID JoltPhysicsServer3D::area_get_shape(RID p_area, int p_shape_idx) const {
	const JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, RID());

	const JoltShape3D *shape = area->get_shape(p_shape_idx);
	ERR_FAIL_NULL_V(shape, RID());

	return shape->get_rid();
}

void JoltPhysicsServer3D::area_set_shape_transform(RID p_area, int p_shape_idx, const Transform3D &p_transform) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_shape_transform(p_shape_idx, p_transform);
}

Transform3D JoltPhysicsServer3D::area_get_shape_transform(RID p_area, int p_shape_idx) const {
	const JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, Transform3D());

	return area->get_shape_transform_scaled(p_shape_idx);
}

int JoltPhysicsServer3D::area_get_shape_count(RID p_area) const {
	const JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);

	return area->get_shape_count();
}

void JoltPhysicsServer3D::area_remove_shape(RID p_area, int p_shape_idx) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->remove_shape(p_shape_idx);
}

void JoltPhysicsServer3D::area_clear_shapes(RID p_area) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->clear_shapes();
}

void JoltPhysicsServer3D::area_set_shape_disabled(RID p_area, int p_shape_idx, bool p_disabled) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_shape_disabled(p_shape_idx, p_disabled);
}

void JoltPhysicsServer3D::area_attach_object_instance_id(RID p_area, ObjectID p_id) {
	RID area_rid = p_area;

	if (space_owner.owns(area_rid)) {
		const JoltSpace3D *space = space_owner.get_or_null(area_rid);
		area_rid = space->get_default_area()->get_rid();
	}

	JoltArea3D *area = area_owner.get_or_null(area_rid);
	ERR_FAIL_NULL(area);

	area->set_instance_id(p_id);
}

ObjectID JoltPhysicsServer3D::area_get_object_instance_id(RID p_area) const {
	RID area_rid = p_area;

	if (space_owner.owns(area_rid)) {
		const JoltSpace3D *space = space_owner.get_or_null(area_rid);
		area_rid = space->get_default_area()->get_rid();
	}

	JoltArea3D *area = area_owner.get_or_null(area_rid);
	ERR_FAIL_NULL_V(area, ObjectID());

	return area->get_instance_id();
}

void JoltPhysicsServer3D::area_set_param(RID p_area, AreaParameter p_param, const Variant &p_value) {
	RID area_rid = p_area;

	if (space_owner.owns(area_rid)) {
		const JoltSpace3D *space = space_owner.get_or_null(area_rid);
		area_rid = space->get_default_area()->get_rid();
	}

	JoltArea3D *area = area_owner.get_or_null(area_rid);
	ERR_FAIL_NULL(area);

	area->set_param(p_param, p_value);
}

Variant JoltPhysicsServer3D::area_get_param(RID p_area, AreaParameter p_param) const {
	RID area_rid = p_area;

	if (space_owner.owns(area_rid)) {
		const JoltSpace3D *space = space_owner.get_or_null(area_rid);
		area_rid = space->get_default_area()->get_rid();
	}

	JoltArea3D *area = area_owner.get_or_null(area_rid);
	ERR_FAIL_NULL_V(area, Variant());

	return area->get_param(p_param);
}

void JoltPhysicsServer3D::area_set_transform(RID p_area, const Transform3D &p_transform) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	return area->set_transform(p_transform);
}

Transform3D JoltPhysicsServer3D::area_get_transform(RID p_area) const {
	const JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, Transform3D());

	return area->get_transform_scaled();
}

void JoltPhysicsServer3D::area_set_collision_mask(RID p_area, uint32_t p_mask) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_collision_mask(p_mask);
}

uint32_t JoltPhysicsServer3D::area_get_collision_mask(RID p_area) const {
	const JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);

	return area->get_collision_mask();
}

void JoltPhysicsServer3D::area_set_collision_layer(RID p_area, uint32_t p_layer) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_collision_layer(p_layer);
}

uint32_t JoltPhysicsServer3D::area_get_collision_layer(RID p_area) const {
	const JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);

	return area->get_collision_layer();
}

void JoltPhysicsServer3D::area_set_monitorable(RID p_area, bool p_monitorable) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_monitorable(p_monitorable);
}

void JoltPhysicsServer3D::area_set_monitor_callback(RID p_area, const Callable &p_callback) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_body_monitor_callback(p_callback);
}

void JoltPhysicsServer3D::area_set_area_monitor_callback(RID p_area, const Callable &p_callback) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_area_monitor_callback(p_callback);
}

void JoltPhysicsServer3D::area_set_ray_pickable(RID p_area, bool p_enable) {
	JoltArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);

	area->set_pickable(p_enable);
}

RID JoltPhysicsServer3D::body_create() {
	JoltBody3D *body = memnew(JoltBody3D);
	RID rid = body_owner.make_rid(body);
	body->set_rid(rid);
	return rid;
}

void JoltPhysicsServer3D::body_set_space(RID p_body, RID p_space) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	JoltSpace3D *space = nullptr;

	if (p_space.is_valid()) {
		space = space_owner.get_or_null(p_space);
		ERR_FAIL_NULL(space);
	}

	body->set_space(space);
}

RID JoltPhysicsServer3D::body_get_space(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, RID());

	const JoltSpace3D *space = body->get_space();

	if (space == nullptr) {
		return RID();
	}

	return space->get_rid();
}

void JoltPhysicsServer3D::body_set_mode(RID p_body, BodyMode p_mode) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_mode(p_mode);
}

PhysicsServer3D::BodyMode JoltPhysicsServer3D::body_get_mode(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, BODY_MODE_STATIC);

	return body->get_mode();
}

void JoltPhysicsServer3D::body_add_shape(RID p_body, RID p_shape, const Transform3D &p_transform, bool p_disabled) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	body->add_shape(shape, p_transform, p_disabled);
}

void JoltPhysicsServer3D::body_set_shape(RID p_body, int p_shape_idx, RID p_shape) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	JoltShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	body->set_shape(p_shape_idx, shape);
}

RID JoltPhysicsServer3D::body_get_shape(RID p_body, int p_shape_idx) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, RID());

	const JoltShape3D *shape = body->get_shape(p_shape_idx);
	ERR_FAIL_NULL_V(shape, RID());

	return shape->get_rid();
}

void JoltPhysicsServer3D::body_set_shape_transform(RID p_body, int p_shape_idx, const Transform3D &p_transform) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_shape_transform(p_shape_idx, p_transform);
}

Transform3D JoltPhysicsServer3D::body_get_shape_transform(RID p_body, int p_shape_idx) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Transform3D());

	return body->get_shape_transform_scaled(p_shape_idx);
}

int JoltPhysicsServer3D::body_get_shape_count(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);

	return body->get_shape_count();
}

void JoltPhysicsServer3D::body_remove_shape(RID p_body, int p_shape_idx) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->remove_shape(p_shape_idx);
}

void JoltPhysicsServer3D::body_clear_shapes(RID p_body) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->clear_shapes();
}

void JoltPhysicsServer3D::body_set_shape_disabled(RID p_body, int p_shape_idx, bool p_disabled) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_shape_disabled(p_shape_idx, p_disabled);
}

void JoltPhysicsServer3D::body_attach_object_instance_id(RID p_body, ObjectID p_id) {
	if (JoltBody3D *body = body_owner.get_or_null(p_body)) {
		body->set_instance_id(p_id);
	} else if (JoltSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body)) {
		soft_body->set_instance_id(p_id);
	} else {
		ERR_FAIL();
	}
}

ObjectID JoltPhysicsServer3D::body_get_object_instance_id(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, ObjectID());

	return body->get_instance_id();
}

void JoltPhysicsServer3D::body_set_enable_continuous_collision_detection(RID p_body, bool p_enable) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_ccd_enabled(p_enable);
}

bool JoltPhysicsServer3D::body_is_continuous_collision_detection_enabled(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);

	return body->is_ccd_enabled();
}

void JoltPhysicsServer3D::body_set_collision_layer(RID p_body, uint32_t p_layer) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_collision_layer(p_layer);
}

uint32_t JoltPhysicsServer3D::body_get_collision_layer(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);

	return body->get_collision_layer();
}

void JoltPhysicsServer3D::body_set_collision_mask(RID p_body, uint32_t p_mask) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_collision_mask(p_mask);
}

uint32_t JoltPhysicsServer3D::body_get_collision_mask(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);

	return body->get_collision_mask();
}

void JoltPhysicsServer3D::body_set_collision_priority(RID p_body, real_t p_priority) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_collision_priority((float)p_priority);
}

real_t JoltPhysicsServer3D::body_get_collision_priority(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0.0);

	return (real_t)body->get_collision_priority();
}

void JoltPhysicsServer3D::body_set_user_flags(RID p_body, uint32_t p_flags) {
	WARN_PRINT("Body user flags are not supported. Any such value will be ignored.");
}

uint32_t JoltPhysicsServer3D::body_get_user_flags(RID p_body) const {
	return 0;
}

void JoltPhysicsServer3D::body_set_param(RID p_body, BodyParameter p_param, const Variant &p_value) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_param(p_param, p_value);
}

Variant JoltPhysicsServer3D::body_get_param(RID p_body, BodyParameter p_param) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Variant());

	return body->get_param(p_param);
}

void JoltPhysicsServer3D::body_reset_mass_properties(RID p_body) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->reset_mass_properties();
}

void JoltPhysicsServer3D::body_set_state(RID p_body, BodyState p_state, const Variant &p_value) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_state(p_state, p_value);
}

Variant JoltPhysicsServer3D::body_get_state(RID p_body, BodyState p_state) const {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Variant());

	return body->get_state(p_state);
}

void JoltPhysicsServer3D::body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->apply_central_impulse(p_impulse);
}

void JoltPhysicsServer3D::body_apply_impulse(RID p_body, const Vector3 &p_impulse, const Vector3 &p_position) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->apply_impulse(p_impulse, p_position);
}

void JoltPhysicsServer3D::body_apply_torque_impulse(RID p_body, const Vector3 &p_impulse) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->apply_torque_impulse(p_impulse);
}

void JoltPhysicsServer3D::body_apply_central_force(RID p_body, const Vector3 &p_force) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->apply_central_force(p_force);
}

void JoltPhysicsServer3D::body_apply_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->apply_force(p_force, p_position);
}

void JoltPhysicsServer3D::body_apply_torque(RID p_body, const Vector3 &p_torque) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->apply_torque(p_torque);
}

void JoltPhysicsServer3D::body_add_constant_central_force(RID p_body, const Vector3 &p_force) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->add_constant_central_force(p_force);
}

void JoltPhysicsServer3D::body_add_constant_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->add_constant_force(p_force, p_position);
}

void JoltPhysicsServer3D::body_add_constant_torque(RID p_body, const Vector3 &p_torque) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->add_constant_torque(p_torque);
}

void JoltPhysicsServer3D::body_set_constant_force(RID p_body, const Vector3 &p_force) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_constant_force(p_force);
}

Vector3 JoltPhysicsServer3D::body_get_constant_force(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Vector3());

	return body->get_constant_force();
}

void JoltPhysicsServer3D::body_set_constant_torque(RID p_body, const Vector3 &p_torque) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_constant_torque(p_torque);
}

Vector3 JoltPhysicsServer3D::body_get_constant_torque(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Vector3());

	return body->get_constant_torque();
}

void JoltPhysicsServer3D::body_set_axis_velocity(RID p_body, const Vector3 &p_axis_velocity) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_axis_velocity(p_axis_velocity);
}

void JoltPhysicsServer3D::body_set_axis_lock(RID p_body, BodyAxis p_axis, bool p_lock) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_axis_lock(p_axis, p_lock);
}

bool JoltPhysicsServer3D::body_is_axis_locked(RID p_body, BodyAxis p_axis) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);

	return body->is_axis_locked(p_axis);
}

void JoltPhysicsServer3D::body_add_collision_exception(RID p_body, RID p_excepted_body) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->add_collision_exception(p_excepted_body);
}

void JoltPhysicsServer3D::body_remove_collision_exception(RID p_body, RID p_excepted_body) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->remove_collision_exception(p_excepted_body);
}

void JoltPhysicsServer3D::body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	for (const RID &exception : body->get_collision_exceptions()) {
		p_exceptions->push_back(exception);
	}
}

void JoltPhysicsServer3D::body_set_max_contacts_reported(RID p_body, int p_amount) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_max_contacts_reported(p_amount);
}

int JoltPhysicsServer3D::body_get_max_contacts_reported(RID p_body) const {
	const JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);

	return body->get_max_contacts_reported();
}

void JoltPhysicsServer3D::body_set_contacts_reported_depth_threshold(RID p_body, real_t p_threshold) {
	WARN_PRINT("Per-body contact depth threshold is not supported. Any such value will be ignored.");
}

real_t JoltPhysicsServer3D::body_get_contacts_reported_depth_threshold(RID p_body) const {
	return 0.0;
}

void JoltPhysicsServer3D::body_set_omit_force_integration(RID p_body, bool p_enable) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_custom_integrator(p_enable);
}

bool JoltPhysicsServer3D::body_is_omitting_force_integration(RID p_body) const {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);

	return body->has_custom_integrator();
}

void JoltPhysicsServer3D::body_set_state_sync_callback(RID p_body, const Callable &p_callable) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_state_sync_callback(p_callable);
}

void JoltPhysicsServer3D::body_set_force_integration_callback(RID p_body, const Callable &p_callable, const Variant &p_userdata) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_custom_integration_callback(p_callable, p_userdata);
}

void JoltPhysicsServer3D::body_set_ray_pickable(RID p_body, bool p_enable) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_pickable(p_enable);
}

bool JoltPhysicsServer3D::body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result) {
	JoltBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);

	JoltSpace3D *space = body->get_space();
	ERR_FAIL_NULL_V(space, false);

	return space->get_direct_state()->body_test_motion(*body, p_parameters, r_result);
}

PhysicsDirectBodyState3D *JoltPhysicsServer3D::body_get_direct_state(RID p_body) {
	ERR_FAIL_COND_V_MSG((on_separate_thread && !doing_sync), nullptr, "Body state is inaccessible right now, wait for iteration or physics process notification.");

	JoltBody3D *body = body_owner.get_or_null(p_body);
	if (unlikely(body == nullptr || body->get_space() == nullptr)) {
		return nullptr;
	}

	ERR_FAIL_COND_V_MSG(body->get_space()->is_stepping(), nullptr, "Body state is inaccessible right now, wait for iteration or physics process notification.");

	return body->get_direct_state();
}

RID JoltPhysicsServer3D::soft_body_create() {
	JoltSoftBody3D *body = memnew(JoltSoftBody3D);
	RID rid = soft_body_owner.make_rid(body);
	body->set_rid(rid);
	return rid;
}

void JoltPhysicsServer3D::soft_body_update_rendering_server(RID p_body, PhysicsServer3DRenderingServerHandler *p_rendering_server_handler) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->update_rendering_server(p_rendering_server_handler);
}

void JoltPhysicsServer3D::soft_body_set_space(RID p_body, RID p_space) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	JoltSpace3D *space = nullptr;

	if (p_space.is_valid()) {
		space = space_owner.get_or_null(p_space);
		ERR_FAIL_NULL(space);
	}

	body->set_space(space);
}

RID JoltPhysicsServer3D::soft_body_get_space(RID p_body) const {
	const JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, RID());

	const JoltSpace3D *space = body->get_space();

	if (space == nullptr) {
		return RID();
	}

	return space->get_rid();
}

void JoltPhysicsServer3D::soft_body_set_mesh(RID p_body, RID p_mesh) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_mesh(p_mesh);
}

AABB JoltPhysicsServer3D::soft_body_get_bounds(RID p_body) const {
	const JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, AABB());

	return body->get_bounds();
}

void JoltPhysicsServer3D::soft_body_set_collision_layer(RID p_body, uint32_t p_layer) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_collision_layer(p_layer);
}

uint32_t JoltPhysicsServer3D::soft_body_get_collision_layer(RID p_body) const {
	const JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);

	return body->get_collision_layer();
}

void JoltPhysicsServer3D::soft_body_set_collision_mask(RID p_body, uint32_t p_mask) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_collision_mask(p_mask);
}

uint32_t JoltPhysicsServer3D::soft_body_get_collision_mask(RID p_body) const {
	const JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);

	return body->get_collision_mask();
}

void JoltPhysicsServer3D::soft_body_add_collision_exception(RID p_body, RID p_excepted_body) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->add_collision_exception(p_excepted_body);
}

void JoltPhysicsServer3D::soft_body_remove_collision_exception(RID p_body, RID p_excepted_body) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->remove_collision_exception(p_excepted_body);
}

void JoltPhysicsServer3D::soft_body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) {
	const JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	for (const RID &exception : body->get_collision_exceptions()) {
		p_exceptions->push_back(exception);
	}
}

void JoltPhysicsServer3D::soft_body_set_state(RID p_body, BodyState p_state, const Variant &p_value) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_state(p_state, p_value);
}

Variant JoltPhysicsServer3D::soft_body_get_state(RID p_body, BodyState p_state) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Variant());

	return body->get_state(p_state);
}

void JoltPhysicsServer3D::soft_body_set_transform(RID p_body, const Transform3D &p_transform) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_transform(p_transform);
}

void JoltPhysicsServer3D::soft_body_set_ray_pickable(RID p_body, bool p_enable) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_pickable(p_enable);
}

void JoltPhysicsServer3D::soft_body_set_simulation_precision(RID p_body, int p_precision) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_simulation_precision(p_precision);
}

int JoltPhysicsServer3D::soft_body_get_simulation_precision(RID p_body) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);

	return body->get_simulation_precision();
}

void JoltPhysicsServer3D::soft_body_set_total_mass(RID p_body, real_t p_total_mass) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_mass((float)p_total_mass);
}

real_t JoltPhysicsServer3D::soft_body_get_total_mass(RID p_body) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0.0);

	return (real_t)body->get_mass();
}

void JoltPhysicsServer3D::soft_body_set_linear_stiffness(RID p_body, real_t p_coefficient) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_stiffness_coefficient((float)p_coefficient);
}

real_t JoltPhysicsServer3D::soft_body_get_linear_stiffness(RID p_body) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0.0);

	return (real_t)body->get_stiffness_coefficient();
}

void JoltPhysicsServer3D::soft_body_set_pressure_coefficient(RID p_body, real_t p_coefficient) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_pressure((float)p_coefficient);
}

real_t JoltPhysicsServer3D::soft_body_get_pressure_coefficient(RID p_body) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0.0);

	return (real_t)body->get_pressure();
}

void JoltPhysicsServer3D::soft_body_set_damping_coefficient(RID p_body, real_t p_coefficient) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_linear_damping((float)p_coefficient);
}

real_t JoltPhysicsServer3D::soft_body_get_damping_coefficient(RID p_body) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0.0);

	return (real_t)body->get_linear_damping();
}

void JoltPhysicsServer3D::soft_body_set_drag_coefficient(RID p_body, real_t p_coefficient) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	return body->set_drag((float)p_coefficient);
}

real_t JoltPhysicsServer3D::soft_body_get_drag_coefficient(RID p_body) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0.0);

	return (real_t)body->get_drag();
}

void JoltPhysicsServer3D::soft_body_move_point(RID p_body, int p_point_index, const Vector3 &p_global_position) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->set_vertex_position(p_point_index, p_global_position);
}

Vector3 JoltPhysicsServer3D::soft_body_get_point_global_position(RID p_body, int p_point_index) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Vector3());

	return body->get_vertex_position(p_point_index);
}

void JoltPhysicsServer3D::soft_body_remove_all_pinned_points(RID p_body) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	body->unpin_all_vertices();
}

void JoltPhysicsServer3D::soft_body_pin_point(RID p_body, int p_point_index, bool p_pin) {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	if (p_pin) {
		body->pin_vertex(p_point_index);
	} else {
		body->unpin_vertex(p_point_index);
	}
}

bool JoltPhysicsServer3D::soft_body_is_point_pinned(RID p_body, int p_point_index) const {
	JoltSoftBody3D *body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);

	return body->is_vertex_pinned(p_point_index);
}

RID JoltPhysicsServer3D::joint_create() {
	JoltJoint3D *joint = memnew(JoltJoint3D);
	RID rid = joint_owner.make_rid(joint);
	joint->set_rid(rid);
	return rid;
}

void JoltPhysicsServer3D::joint_clear(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	if (joint->get_type() != JOINT_TYPE_MAX) {
		JoltJoint3D *empty_joint = memnew(JoltJoint3D);
		empty_joint->set_rid(joint->get_rid());

		memdelete(joint);
		joint = nullptr;

		joint_owner.replace(p_joint, empty_joint);
	}
}

void JoltPhysicsServer3D::joint_make_pin(RID p_joint, RID p_body_a, const Vector3 &p_local_a, RID p_body_b, const Vector3 &p_local_b) {
	JoltJoint3D *old_joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(old_joint);

	JoltBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL(body_a);

	JoltBody3D *body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_COND(body_a == body_b);

	JoltJoint3D *new_joint = memnew(JoltPinJoint3D(*old_joint, body_a, body_b, p_local_a, p_local_b));

	memdelete(old_joint);
	old_joint = nullptr;

	joint_owner.replace(p_joint, new_joint);
}

void JoltPhysicsServer3D::pin_joint_set_param(RID p_joint, PinJointParam p_param, real_t p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_PIN);
	JoltPinJoint3D *pin_joint = static_cast<JoltPinJoint3D *>(joint);

	pin_joint->set_param(p_param, (double)p_value);
}

real_t JoltPhysicsServer3D::pin_joint_get_param(RID p_joint, PinJointParam p_param) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_PIN, 0.0);
	const JoltPinJoint3D *pin_joint = static_cast<const JoltPinJoint3D *>(joint);

	return (real_t)pin_joint->get_param(p_param);
}

void JoltPhysicsServer3D::pin_joint_set_local_a(RID p_joint, const Vector3 &p_local_a) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_PIN);
	JoltPinJoint3D *pin_joint = static_cast<JoltPinJoint3D *>(joint);

	pin_joint->set_local_a(p_local_a);
}

Vector3 JoltPhysicsServer3D::pin_joint_get_local_a(RID p_joint) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, Vector3());

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_PIN, Vector3());
	const JoltPinJoint3D *pin_joint = static_cast<const JoltPinJoint3D *>(joint);

	return pin_joint->get_local_a();
}

void JoltPhysicsServer3D::pin_joint_set_local_b(RID p_joint, const Vector3 &p_local_b) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_PIN);
	JoltPinJoint3D *pin_joint = static_cast<JoltPinJoint3D *>(joint);

	pin_joint->set_local_b(p_local_b);
}

Vector3 JoltPhysicsServer3D::pin_joint_get_local_b(RID p_joint) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, Vector3());

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_PIN, Vector3());
	const JoltPinJoint3D *pin_joint = static_cast<const JoltPinJoint3D *>(joint);

	return pin_joint->get_local_b();
}

void JoltPhysicsServer3D::joint_make_hinge(RID p_joint, RID p_body_a, const Transform3D &p_hinge_a, RID p_body_b, const Transform3D &p_hinge_b) {
	JoltJoint3D *old_joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(old_joint);

	JoltBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL(body_a);

	JoltBody3D *body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_COND(body_a == body_b);

	JoltJoint3D *new_joint = memnew(JoltHingeJoint3D(*old_joint, body_a, body_b, p_hinge_a, p_hinge_b));

	memdelete(old_joint);
	old_joint = nullptr;

	joint_owner.replace(p_joint, new_joint);
}

void JoltPhysicsServer3D::joint_make_hinge_simple(RID p_joint, RID p_body_a, const Vector3 &p_pivot_a, const Vector3 &p_axis_a, RID p_body_b, const Vector3 &p_pivot_b, const Vector3 &p_axis_b) {
	ERR_FAIL_MSG("Simple hinge joints are not supported when using Jolt Physics.");
}

void JoltPhysicsServer3D::hinge_joint_set_param(RID p_joint, HingeJointParam p_param, real_t p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_HINGE);
	JoltHingeJoint3D *hinge_joint = static_cast<JoltHingeJoint3D *>(joint);

	return hinge_joint->set_param(p_param, (double)p_value);
}

real_t JoltPhysicsServer3D::hinge_joint_get_param(RID p_joint, HingeJointParam p_param) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_HINGE, 0.0);
	const JoltHingeJoint3D *hinge_joint = static_cast<const JoltHingeJoint3D *>(joint);

	return (real_t)hinge_joint->get_param(p_param);
}

void JoltPhysicsServer3D::hinge_joint_set_flag(RID p_joint, HingeJointFlag p_flag, bool p_enabled) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_HINGE);
	JoltHingeJoint3D *hinge_joint = static_cast<JoltHingeJoint3D *>(joint);

	return hinge_joint->set_flag(p_flag, p_enabled);
}

bool JoltPhysicsServer3D::hinge_joint_get_flag(RID p_joint, HingeJointFlag p_flag) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_HINGE, false);
	const JoltHingeJoint3D *hinge_joint = static_cast<const JoltHingeJoint3D *>(joint);

	return hinge_joint->get_flag(p_flag);
}

void JoltPhysicsServer3D::joint_make_slider(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) {
	JoltJoint3D *old_joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(old_joint);

	JoltBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL(body_a);

	JoltBody3D *body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_COND(body_a == body_b);

	JoltJoint3D *new_joint = memnew(JoltSliderJoint3D(*old_joint, body_a, body_b, p_local_ref_a, p_local_ref_b));

	memdelete(old_joint);
	old_joint = nullptr;

	joint_owner.replace(p_joint, new_joint);
}

void JoltPhysicsServer3D::slider_joint_set_param(RID p_joint, SliderJointParam p_param, real_t p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_SLIDER);
	JoltSliderJoint3D *slider_joint = static_cast<JoltSliderJoint3D *>(joint);

	return slider_joint->set_param(p_param, (real_t)p_value);
}

real_t JoltPhysicsServer3D::slider_joint_get_param(RID p_joint, SliderJointParam p_param) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_SLIDER, 0.0);
	const JoltSliderJoint3D *slider_joint = static_cast<const JoltSliderJoint3D *>(joint);

	return slider_joint->get_param(p_param);
}

void JoltPhysicsServer3D::joint_make_cone_twist(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) {
	JoltJoint3D *old_joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(old_joint);

	JoltBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL(body_a);

	JoltBody3D *body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_COND(body_a == body_b);

	JoltJoint3D *new_joint = memnew(JoltConeTwistJoint3D(*old_joint, body_a, body_b, p_local_ref_a, p_local_ref_b));

	memdelete(old_joint);
	old_joint = nullptr;

	joint_owner.replace(p_joint, new_joint);
}

void JoltPhysicsServer3D::cone_twist_joint_set_param(RID p_joint, ConeTwistJointParam p_param, real_t p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_CONE_TWIST);
	JoltConeTwistJoint3D *cone_twist_joint = static_cast<JoltConeTwistJoint3D *>(joint);

	return cone_twist_joint->set_param(p_param, (double)p_value);
}

real_t JoltPhysicsServer3D::cone_twist_joint_get_param(RID p_joint, ConeTwistJointParam p_param) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_CONE_TWIST, 0.0);
	const JoltConeTwistJoint3D *cone_twist_joint = static_cast<const JoltConeTwistJoint3D *>(joint);

	return (real_t)cone_twist_joint->get_param(p_param);
}

void JoltPhysicsServer3D::joint_make_generic_6dof(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) {
	JoltJoint3D *old_joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(old_joint);

	JoltBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL(body_a);

	JoltBody3D *body_b = body_owner.get_or_null(p_body_b);
	ERR_FAIL_COND(body_a == body_b);

	JoltJoint3D *new_joint = memnew(JoltGeneric6DOFJoint3D(*old_joint, body_a, body_b, p_local_ref_a, p_local_ref_b));

	memdelete(old_joint);
	old_joint = nullptr;

	joint_owner.replace(p_joint, new_joint);
}

void JoltPhysicsServer3D::generic_6dof_joint_set_param(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param, real_t p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_6DOF);
	JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->set_param(p_axis, p_param, (double)p_value);
}

real_t JoltPhysicsServer3D::generic_6dof_joint_get_param(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_6DOF, 0.0);
	const JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<const JoltGeneric6DOFJoint3D *>(joint);

	return (real_t)g6dof_joint->get_param(p_axis, p_param);
}

void JoltPhysicsServer3D::generic_6dof_joint_set_flag(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag, bool p_enable) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_6DOF);
	JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->set_flag(p_axis, p_flag, p_enable);
}

bool JoltPhysicsServer3D::generic_6dof_joint_get_flag(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_6DOF, false);
	const JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<const JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->get_flag(p_axis, p_flag);
}

PhysicsServer3D::JointType JoltPhysicsServer3D::joint_get_type(RID p_joint) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, JOINT_TYPE_PIN);

	return joint->get_type();
}

void JoltPhysicsServer3D::joint_set_solver_priority(RID p_joint, int p_priority) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	joint->set_solver_priority(p_priority);
}

int JoltPhysicsServer3D::joint_get_solver_priority(RID p_joint) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0);

	return joint->get_solver_priority();
}

void JoltPhysicsServer3D::joint_disable_collisions_between_bodies(RID p_joint, bool p_disable) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	joint->set_collision_disabled(p_disable);
}

bool JoltPhysicsServer3D::joint_is_disabled_collisions_between_bodies(RID p_joint) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);

	return joint->is_collision_disabled();
}

void JoltPhysicsServer3D::free(RID p_rid) {
	if (JoltShape3D *shape = shape_owner.get_or_null(p_rid)) {
		free_shape(shape);
	} else if (JoltBody3D *body = body_owner.get_or_null(p_rid)) {
		free_body(body);
	} else if (JoltJoint3D *joint = joint_owner.get_or_null(p_rid)) {
		free_joint(joint);
	} else if (JoltArea3D *area = area_owner.get_or_null(p_rid)) {
		free_area(area);
	} else if (JoltSoftBody3D *soft_body = soft_body_owner.get_or_null(p_rid)) {
		free_soft_body(soft_body);
	} else if (JoltSpace3D *space = space_owner.get_or_null(p_rid)) {
		free_space(space);
	} else {
		ERR_FAIL_MSG("Failed to free RID: The specified RID has no owner.");
	}
}

void JoltPhysicsServer3D::set_active(bool p_active) {
	active = p_active;
}

void JoltPhysicsServer3D::init() {
	job_system = new JoltJobSystem();
}

void JoltPhysicsServer3D::finish() {
	if (job_system != nullptr) {
		delete job_system;
		job_system = nullptr;
	}
}

void JoltPhysicsServer3D::step(real_t p_step) {
	if (!active) {
		return;
	}

	for (JoltSpace3D *active_space : active_spaces) {
		job_system->pre_step();

		active_space->step((float)p_step);

		job_system->post_step();
	}
}

void JoltPhysicsServer3D::sync() {
	doing_sync = true;
}

void JoltPhysicsServer3D::end_sync() {
	doing_sync = false;
}

void JoltPhysicsServer3D::flush_queries() {
	if (!active) {
		return;
	}

	flushing_queries = true;

	for (JoltSpace3D *space : active_spaces) {
		space->call_queries();
	}

	flushing_queries = false;

#ifdef DEBUG_ENABLED
	job_system->flush_timings();
#endif
}

bool JoltPhysicsServer3D::is_flushing_queries() const {
	return flushing_queries;
}

int JoltPhysicsServer3D::get_process_info(ProcessInfo p_process_info) {
	return 0;
}

void JoltPhysicsServer3D::free_space(JoltSpace3D *p_space) {
	ERR_FAIL_NULL(p_space);

	free_area(p_space->get_default_area());
	space_set_active(p_space->get_rid(), false);
	space_owner.free(p_space->get_rid());
	memdelete(p_space);
}

void JoltPhysicsServer3D::free_area(JoltArea3D *p_area) {
	ERR_FAIL_NULL(p_area);

	p_area->set_space(nullptr);
	area_owner.free(p_area->get_rid());
	memdelete(p_area);
}

void JoltPhysicsServer3D::free_body(JoltBody3D *p_body) {
	ERR_FAIL_NULL(p_body);

	p_body->set_space(nullptr);
	body_owner.free(p_body->get_rid());
	memdelete(p_body);
}

void JoltPhysicsServer3D::free_soft_body(JoltSoftBody3D *p_body) {
	ERR_FAIL_NULL(p_body);

	p_body->set_space(nullptr);
	soft_body_owner.free(p_body->get_rid());
	memdelete(p_body);
}

void JoltPhysicsServer3D::free_shape(JoltShape3D *p_shape) {
	ERR_FAIL_NULL(p_shape);

	p_shape->remove_self();
	shape_owner.free(p_shape->get_rid());
	memdelete(p_shape);
}

void JoltPhysicsServer3D::free_joint(JoltJoint3D *p_joint) {
	ERR_FAIL_NULL(p_joint);

	joint_owner.free(p_joint->get_rid());
	memdelete(p_joint);
}

#ifdef DEBUG_ENABLED

void JoltPhysicsServer3D::dump_debug_snapshots(const String &p_dir) {
	for (JoltSpace3D *space : active_spaces) {
		space->dump_debug_snapshot(p_dir);
	}
}

void JoltPhysicsServer3D::space_dump_debug_snapshot(RID p_space, const String &p_dir) {
	JoltSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);

	space->dump_debug_snapshot(p_dir);
}

#endif

bool JoltPhysicsServer3D::joint_get_enabled(RID p_joint) const {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);

	return joint->is_enabled();
}

void JoltPhysicsServer3D::joint_set_enabled(RID p_joint, bool p_enabled) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	joint->set_enabled(p_enabled);
}

int JoltPhysicsServer3D::joint_get_solver_velocity_iterations(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0);

	return joint->get_solver_velocity_iterations();
}

void JoltPhysicsServer3D::joint_set_solver_velocity_iterations(RID p_joint, int p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	return joint->set_solver_velocity_iterations(p_value);
}

int JoltPhysicsServer3D::joint_get_solver_position_iterations(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0);

	return joint->get_solver_position_iterations();
}

void JoltPhysicsServer3D::joint_set_solver_position_iterations(RID p_joint, int p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	return joint->set_solver_position_iterations(p_value);
}

float JoltPhysicsServer3D::pin_joint_get_applied_force(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_PIN, 0.0);
	JoltPinJoint3D *pin_joint = static_cast<JoltPinJoint3D *>(joint);

	return pin_joint->get_applied_force();
}

double JoltPhysicsServer3D::hinge_joint_get_jolt_param(RID p_joint, HingeJointParamJolt p_param) const {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_HINGE, 0.0);
	JoltHingeJoint3D *hinge_joint = static_cast<JoltHingeJoint3D *>(joint);

	return hinge_joint->get_jolt_param(p_param);
}

void JoltPhysicsServer3D::hinge_joint_set_jolt_param(RID p_joint, HingeJointParamJolt p_param, double p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_HINGE);
	JoltHingeJoint3D *hinge_joint = static_cast<JoltHingeJoint3D *>(joint);

	return hinge_joint->set_jolt_param(p_param, p_value);
}

bool JoltPhysicsServer3D::hinge_joint_get_jolt_flag(RID p_joint, HingeJointFlagJolt p_flag) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_HINGE, false);
	const JoltHingeJoint3D *hinge_joint = static_cast<const JoltHingeJoint3D *>(joint);

	return hinge_joint->get_jolt_flag(p_flag);
}

void JoltPhysicsServer3D::hinge_joint_set_jolt_flag(RID p_joint, HingeJointFlagJolt p_flag, bool p_enabled) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_HINGE);
	JoltHingeJoint3D *hinge_joint = static_cast<JoltHingeJoint3D *>(joint);

	return hinge_joint->set_jolt_flag(p_flag, p_enabled);
}

float JoltPhysicsServer3D::hinge_joint_get_applied_force(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_HINGE, 0.0f);
	JoltHingeJoint3D *hinge_joint = static_cast<JoltHingeJoint3D *>(joint);

	return hinge_joint->get_applied_force();
}

float JoltPhysicsServer3D::hinge_joint_get_applied_torque(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_HINGE, 0.0f);
	JoltHingeJoint3D *hinge_joint = static_cast<JoltHingeJoint3D *>(joint);

	return hinge_joint->get_applied_torque();
}

double JoltPhysicsServer3D::slider_joint_get_jolt_param(RID p_joint, SliderJointParamJolt p_param) const {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_SLIDER, 0.0);
	JoltSliderJoint3D *slider_joint = static_cast<JoltSliderJoint3D *>(joint);

	return slider_joint->get_jolt_param(p_param);
}

void JoltPhysicsServer3D::slider_joint_set_jolt_param(RID p_joint, SliderJointParamJolt p_param, double p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_SLIDER);
	JoltSliderJoint3D *slider_joint = static_cast<JoltSliderJoint3D *>(joint);

	return slider_joint->set_jolt_param(p_param, p_value);
}

bool JoltPhysicsServer3D::slider_joint_get_jolt_flag(RID p_joint, SliderJointFlagJolt p_flag) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_SLIDER, false);
	const JoltSliderJoint3D *slider_joint = static_cast<const JoltSliderJoint3D *>(joint);

	return slider_joint->get_jolt_flag(p_flag);
}

void JoltPhysicsServer3D::slider_joint_set_jolt_flag(RID p_joint, SliderJointFlagJolt p_flag, bool p_enabled) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_SLIDER);
	JoltSliderJoint3D *slider_joint = static_cast<JoltSliderJoint3D *>(joint);

	return slider_joint->set_jolt_flag(p_flag, p_enabled);
}

float JoltPhysicsServer3D::slider_joint_get_applied_force(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_SLIDER, 0.0f);
	JoltSliderJoint3D *slider_joint = static_cast<JoltSliderJoint3D *>(joint);

	return slider_joint->get_applied_force();
}

float JoltPhysicsServer3D::slider_joint_get_applied_torque(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_SLIDER, 0.0f);
	JoltSliderJoint3D *slider_joint = static_cast<JoltSliderJoint3D *>(joint);

	return slider_joint->get_applied_torque();
}

double JoltPhysicsServer3D::cone_twist_joint_get_jolt_param(RID p_joint, ConeTwistJointParamJolt p_param) const {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_CONE_TWIST, 0.0);
	JoltConeTwistJoint3D *cone_twist_joint = static_cast<JoltConeTwistJoint3D *>(joint);

	return cone_twist_joint->get_jolt_param(p_param);
}

void JoltPhysicsServer3D::cone_twist_joint_set_jolt_param(RID p_joint, ConeTwistJointParamJolt p_param, double p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_CONE_TWIST);
	JoltConeTwistJoint3D *cone_twist_joint = static_cast<JoltConeTwistJoint3D *>(joint);

	return cone_twist_joint->set_jolt_param(p_param, p_value);
}

bool JoltPhysicsServer3D::cone_twist_joint_get_jolt_flag(RID p_joint, ConeTwistJointFlagJolt p_flag) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_CONE_TWIST, false);
	const JoltConeTwistJoint3D *cone_twist_joint = static_cast<const JoltConeTwistJoint3D *>(joint);

	return cone_twist_joint->get_jolt_flag(p_flag);
}

void JoltPhysicsServer3D::cone_twist_joint_set_jolt_flag(RID p_joint, ConeTwistJointFlagJolt p_flag, bool p_enabled) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_CONE_TWIST);
	JoltConeTwistJoint3D *cone_twist_joint = static_cast<JoltConeTwistJoint3D *>(joint);

	return cone_twist_joint->set_jolt_flag(p_flag, p_enabled);
}

float JoltPhysicsServer3D::cone_twist_joint_get_applied_force(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_CONE_TWIST, 0.0f);
	JoltConeTwistJoint3D *cone_twist_joint = static_cast<JoltConeTwistJoint3D *>(joint);

	return cone_twist_joint->get_applied_force();
}

float JoltPhysicsServer3D::cone_twist_joint_get_applied_torque(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_CONE_TWIST, 0.0f);
	JoltConeTwistJoint3D *cone_twist_joint = static_cast<JoltConeTwistJoint3D *>(joint);

	return cone_twist_joint->get_applied_torque();
}

double JoltPhysicsServer3D::generic_6dof_joint_get_jolt_param(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisParamJolt p_param) const {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_6DOF, 0.0);
	JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->get_jolt_param(p_axis, p_param);
}

void JoltPhysicsServer3D::generic_6dof_joint_set_jolt_param(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisParamJolt p_param, double p_value) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_6DOF);
	JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->set_jolt_param(p_axis, p_param, p_value);
}

bool JoltPhysicsServer3D::generic_6dof_joint_get_jolt_flag(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisFlagJolt p_flag) const {
	const JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_6DOF, false);
	const JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<const JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->get_jolt_flag(p_axis, p_flag);
}

void JoltPhysicsServer3D::generic_6dof_joint_set_jolt_flag(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisFlagJolt p_flag, bool p_enabled) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	ERR_FAIL_COND(joint->get_type() != JOINT_TYPE_6DOF);
	JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->set_jolt_flag(p_axis, p_flag, p_enabled);
}

float JoltPhysicsServer3D::generic_6dof_joint_get_applied_force(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_6DOF, 0.0f);
	JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->get_applied_force();
}

float JoltPhysicsServer3D::generic_6dof_joint_get_applied_torque(RID p_joint) {
	JoltJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0.0f);

	ERR_FAIL_COND_V(joint->get_type() != JOINT_TYPE_6DOF, 0.0f);
	JoltGeneric6DOFJoint3D *g6dof_joint = static_cast<JoltGeneric6DOFJoint3D *>(joint);

	return g6dof_joint->get_applied_torque();
}
