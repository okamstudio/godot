/**************************************************************************/
/*  bullet_physics_server.cpp                                             */
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

#include "bullet_physics_server.h"

#include "bullet_utilities.h"
#include "cone_twist_joint_bullet.h"
#include "core/class_db.h"
#include "core/error_macros.h"
#include "core/ustring.h"
#include "generic_6dof_joint_bullet.h"
#include "hinge_joint_bullet.h"
#include "pin_joint_bullet.h"
#include "shape_bullet.h"
#include "slider_joint_bullet.h"

#include <LinearMath/btVector3.h>

#include <assert.h>

/**
	@author AndreaCatania
*/

#define CreateThenReturnRID(owner, ridData) \
	RID rid = owner.make_rid(ridData);      \
	ridData->set_self(rid);                 \
	ridData->_set_physics_server(this);     \
	return rid;

// <--------------- Joint creation asserts
/// Assert the body is assigned to a space
#define JointAssertSpace(body, bIndex, ret)                                                          \
	if (!body->get_space()) {                                                                        \
		ERR_PRINT("Before create a joint the Body" + String(bIndex) + " must be added to a space!"); \
		return ret;                                                                                  \
	}

/// Assert the two bodies of joint are in the same space
#define JointAssertSameSpace(bodyA, bodyB, ret)                                                   \
	if (bodyA->get_space() != bodyB->get_space()) {                                               \
		ERR_PRINT("In order to create a joint the Body_A and Body_B must be in the same space!"); \
		return RID();                                                                             \
	}

#define AddJointToSpace(body, joint) \
	body->get_space()->add_constraint(joint, joint->is_disabled_collisions_between_bodies());
// <--------------- Joint creation asserts

void BulletPhysicsServer::_bind_methods() {
	//ClassDB::bind_method(D_METHOD("DoTest"), &BulletPhysicsServer::DoTest);
}

BulletPhysicsServer::BulletPhysicsServer() :
		PhysicsServer(),
		active(true),
		active_spaces_count(0) {}

BulletPhysicsServer::~BulletPhysicsServer() {}

RID BulletPhysicsServer::shape_create(ShapeType p_shape) {
	ShapeBullet *shape = nullptr;

	switch (p_shape) {
		case SHAPE_PLANE: {
			shape = bulletnew(PlaneShapeBullet);
		} break;
		case SHAPE_SPHERE: {
			shape = bulletnew(SphereShapeBullet);
		} break;
		case SHAPE_BOX: {
			shape = bulletnew(BoxShapeBullet);
		} break;
		case SHAPE_CAPSULE: {
			shape = bulletnew(CapsuleShapeBullet);
		} break;
		case SHAPE_CYLINDER: {
			shape = bulletnew(CylinderShapeBullet);
		} break;
		case SHAPE_CONVEX_POLYGON: {
			shape = bulletnew(ConvexPolygonShapeBullet);
		} break;
		case SHAPE_CONCAVE_POLYGON: {
			shape = bulletnew(ConcavePolygonShapeBullet);
		} break;
		case SHAPE_HEIGHTMAP: {
			shape = bulletnew(HeightMapShapeBullet);
		} break;
		case SHAPE_RAY: {
			shape = bulletnew(RayShapeBullet);
		} break;
		case SHAPE_CUSTOM:
		default:
			ERR_FAIL_V(RID());
			break;
	}

	CreateThenReturnRID(shape_owner, shape)
}

void BulletPhysicsServer::shape_set_data(RID p_shape, const Variant &p_data) {
	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND(!shape);
	shape->set_data(p_data);
}

void BulletPhysicsServer::shape_set_custom_solver_bias(RID p_shape, real_t p_bias) {
	//WARN_PRINT("Bias not supported by Bullet physics engine");
}

PhysicsServer::ShapeType BulletPhysicsServer::shape_get_type(RID p_shape) const {
	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND_V(!shape, PhysicsServer::SHAPE_CUSTOM);
	return shape->get_type();
}

Variant BulletPhysicsServer::shape_get_data(RID p_shape) const {
	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND_V(!shape, Variant());
	return shape->get_data();
}

void BulletPhysicsServer::shape_set_margin(RID p_shape, real_t p_margin) {
	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND(!shape);
	shape->set_margin(p_margin);
}

real_t BulletPhysicsServer::shape_get_margin(RID p_shape) const {
	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND_V(!shape, 0.0);
	return shape->get_margin();
}

real_t BulletPhysicsServer::shape_get_custom_solver_bias(RID p_shape) const {
	//WARN_PRINT("Bias not supported by Bullet physics engine");
	return 0.;
}

RID BulletPhysicsServer::space_create() {
	SpaceBullet *space = bulletnew(SpaceBullet);
	CreateThenReturnRID(space_owner, space);
}

void BulletPhysicsServer::space_set_active(RID p_space, bool p_active) {
	SpaceBullet *space = space_owner.get(p_space);
	ERR_FAIL_COND(!space);

	if (space_is_active(p_space) == p_active) {
		return;
	}

	if (p_active) {
		++active_spaces_count;
		active_spaces.push_back(space);
	} else {
		--active_spaces_count;
		active_spaces.erase(space);
	}
}

bool BulletPhysicsServer::space_is_active(RID p_space) const {
	SpaceBullet *space = space_owner.get(p_space);
	ERR_FAIL_COND_V(!space, false);

	return -1 != active_spaces.find(space);
}

void BulletPhysicsServer::space_set_param(RID p_space, SpaceParameter p_param, real_t p_value) {
	SpaceBullet *space = space_owner.get(p_space);
	ERR_FAIL_COND(!space);
	space->set_param(p_param, p_value);
}

real_t BulletPhysicsServer::space_get_param(RID p_space, SpaceParameter p_param) const {
	SpaceBullet *space = space_owner.get(p_space);
	ERR_FAIL_COND_V(!space, 0);
	return space->get_param(p_param);
}

PhysicsDirectSpaceState *BulletPhysicsServer::space_get_direct_state(RID p_space) {
	SpaceBullet *space = space_owner.get(p_space);
	ERR_FAIL_COND_V(!space, nullptr);

	return space->get_direct_state();
}

void BulletPhysicsServer::space_set_debug_contacts(RID p_space, int p_max_contacts) {
	SpaceBullet *space = space_owner.get(p_space);
	ERR_FAIL_COND(!space);

	space->set_debug_contacts(p_max_contacts);
}

Vector<Vector3> BulletPhysicsServer::space_get_contacts(RID p_space) const {
	SpaceBullet *space = space_owner.get(p_space);
	ERR_FAIL_COND_V(!space, Vector<Vector3>());

	return space->get_debug_contacts();
}

int BulletPhysicsServer::space_get_contact_count(RID p_space) const {
	SpaceBullet *space = space_owner.get(p_space);
	ERR_FAIL_COND_V(!space, 0);

	return space->get_debug_contact_count();
}

RID BulletPhysicsServer::area_create() {
	AreaBullet *area = bulletnew(AreaBullet);
	area->set_collision_layer(1);
	area->set_collision_mask(1);
	CreateThenReturnRID(area_owner, area)
}

void BulletPhysicsServer::area_set_space(RID p_area, RID p_space) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);
	SpaceBullet *space = nullptr;
	if (p_space.is_valid()) {
		space = space_owner.get(p_space);
		ERR_FAIL_COND(!space);
	}
	area->set_space(space);
}

RID BulletPhysicsServer::area_get_space(RID p_area) const {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND_V(!area, RID());
	SpaceBullet *space = area->get_space();
	if (!space) {
		return RID();
	}
	return space->get_self();
}

void BulletPhysicsServer::area_set_space_override_mode(RID p_area, AreaSpaceOverrideMode p_mode) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	area->set_spOv_mode(p_mode);
}

PhysicsServer::AreaSpaceOverrideMode BulletPhysicsServer::area_get_space_override_mode(RID p_area) const {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND_V(!area, PhysicsServer::AREA_SPACE_OVERRIDE_DISABLED);

	return area->get_spOv_mode();
}

void BulletPhysicsServer::area_add_shape(RID p_area, RID p_shape, const Transform &p_transform, bool p_disabled) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND(!shape);

	area->add_shape(shape, p_transform, p_disabled);
}

void BulletPhysicsServer::area_set_shape(RID p_area, int p_shape_idx, RID p_shape) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND(!shape);

	area->set_shape(p_shape_idx, shape);
}

void BulletPhysicsServer::area_set_shape_transform(RID p_area, int p_shape_idx, const Transform &p_transform) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	area->set_shape_transform(p_shape_idx, p_transform);
}

int BulletPhysicsServer::area_get_shape_count(RID p_area) const {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND_V(!area, 0);

	return area->get_shape_count();
}

RID BulletPhysicsServer::area_get_shape(RID p_area, int p_shape_idx) const {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND_V(!area, RID());

	return area->get_shape(p_shape_idx)->get_self();
}

Transform BulletPhysicsServer::area_get_shape_transform(RID p_area, int p_shape_idx) const {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND_V(!area, Transform());

	return area->get_shape_transform(p_shape_idx);
}

void BulletPhysicsServer::area_remove_shape(RID p_area, int p_shape_idx) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);
	return area->remove_shape_full(p_shape_idx);
}

void BulletPhysicsServer::area_clear_shapes(RID p_area) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	for (int i = area->get_shape_count(); 0 < i; --i) {
		area->remove_shape_full(0);
	}
}

void BulletPhysicsServer::area_set_shape_disabled(RID p_area, int p_shape_idx, bool p_disabled) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	area->set_shape_disabled(p_shape_idx, p_disabled);
}

void BulletPhysicsServer::area_attach_object_instance_id(RID p_area, ObjectID p_id) {
	if (space_owner.owns(p_area)) {
		return;
	}
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);
	area->set_instance_id(p_id);
}

ObjectID BulletPhysicsServer::area_get_object_instance_id(RID p_area) const {
	if (space_owner.owns(p_area)) {
		return 0;
	}
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND_V(!area, ObjectID());
	return area->get_instance_id();
}

void BulletPhysicsServer::area_set_param(RID p_area, AreaParameter p_param, const Variant &p_value) {
	if (space_owner.owns(p_area)) {
		SpaceBullet *space = space_owner.get(p_area);
		if (space) {
			space->set_param(p_param, p_value);
		}
	} else {
		AreaBullet *area = area_owner.get(p_area);
		ERR_FAIL_COND(!area);

		area->set_param(p_param, p_value);
	}
}

Variant BulletPhysicsServer::area_get_param(RID p_area, AreaParameter p_param) const {
	if (space_owner.owns(p_area)) {
		SpaceBullet *space = space_owner.get(p_area);
		return space->get_param(p_param);
	} else {
		AreaBullet *area = area_owner.get(p_area);
		ERR_FAIL_COND_V(!area, Variant());

		return area->get_param(p_param);
	}
}

void BulletPhysicsServer::area_set_transform(RID p_area, const Transform &p_transform) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);
	area->set_transform(p_transform);
}

Transform BulletPhysicsServer::area_get_transform(RID p_area) const {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND_V(!area, Transform());
	return area->get_transform();
}

void BulletPhysicsServer::area_set_collision_mask(RID p_area, uint32_t p_mask) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);
	area->set_collision_mask(p_mask);
}

void BulletPhysicsServer::area_set_collision_layer(RID p_area, uint32_t p_layer) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);
	area->set_collision_layer(p_layer);
}

void BulletPhysicsServer::area_set_monitorable(RID p_area, bool p_monitorable) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	area->set_monitorable(p_monitorable);
}

void BulletPhysicsServer::area_set_monitor_callback(RID p_area, Object *p_receiver, const StringName &p_method) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	area->set_event_callback(CollisionObjectBullet::TYPE_RIGID_BODY, p_receiver ? p_receiver->get_instance_id() : 0, p_method);
}

void BulletPhysicsServer::area_set_area_monitor_callback(RID p_area, Object *p_receiver, const StringName &p_method) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);

	area->set_event_callback(CollisionObjectBullet::TYPE_AREA, p_receiver ? p_receiver->get_instance_id() : 0, p_method);
}

void BulletPhysicsServer::area_set_ray_pickable(RID p_area, bool p_enable) {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND(!area);
	area->set_ray_pickable(p_enable);
}

bool BulletPhysicsServer::area_is_ray_pickable(RID p_area) const {
	AreaBullet *area = area_owner.get(p_area);
	ERR_FAIL_COND_V(!area, false);
	return area->is_ray_pickable();
}

RID BulletPhysicsServer::body_create(BodyMode p_mode, bool p_init_sleeping) {
	RigidBodyBullet *body = bulletnew(RigidBodyBullet);
	body->set_mode(p_mode);
	body->set_collision_layer(1);
	body->set_collision_mask(1);
	if (p_init_sleeping) {
		body->set_state(BODY_STATE_SLEEPING, p_init_sleeping);
	}
	CreateThenReturnRID(rigid_body_owner, body);
}

void BulletPhysicsServer::body_set_space(RID p_body, RID p_space) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	SpaceBullet *space = nullptr;

	if (p_space.is_valid()) {
		space = space_owner.get(p_space);
		ERR_FAIL_COND(!space);
	}

	if (body->get_space() == space) {
		return; //pointles
	}

	body->set_space(space);
}

RID BulletPhysicsServer::body_get_space(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, RID());

	SpaceBullet *space = body->get_space();
	if (!space) {
		return RID();
	}
	return space->get_self();
}

void BulletPhysicsServer::body_set_mode(RID p_body, PhysicsServer::BodyMode p_mode) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_mode(p_mode);
}

PhysicsServer::BodyMode BulletPhysicsServer::body_get_mode(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, BODY_MODE_STATIC);
	return body->get_mode();
}

void BulletPhysicsServer::body_add_shape(RID p_body, RID p_shape, const Transform &p_transform, bool p_disabled) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND(!shape);

	body->add_shape(shape, p_transform, p_disabled);
}

void BulletPhysicsServer::body_set_shape(RID p_body, int p_shape_idx, RID p_shape) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	ShapeBullet *shape = shape_owner.get(p_shape);
	ERR_FAIL_COND(!shape);

	body->set_shape(p_shape_idx, shape);
}

void BulletPhysicsServer::body_set_shape_transform(RID p_body, int p_shape_idx, const Transform &p_transform) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_shape_transform(p_shape_idx, p_transform);
}

int BulletPhysicsServer::body_get_shape_count(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);
	return body->get_shape_count();
}

RID BulletPhysicsServer::body_get_shape(RID p_body, int p_shape_idx) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, RID());

	ShapeBullet *shape = body->get_shape(p_shape_idx);
	ERR_FAIL_COND_V(!shape, RID());

	return shape->get_self();
}

Transform BulletPhysicsServer::body_get_shape_transform(RID p_body, int p_shape_idx) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, Transform());
	return body->get_shape_transform(p_shape_idx);
}

void BulletPhysicsServer::body_set_shape_disabled(RID p_body, int p_shape_idx, bool p_disabled) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_shape_disabled(p_shape_idx, p_disabled);
}

void BulletPhysicsServer::body_remove_shape(RID p_body, int p_shape_idx) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->remove_shape_full(p_shape_idx);
}

void BulletPhysicsServer::body_clear_shapes(RID p_body) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->remove_all_shapes();
}

void BulletPhysicsServer::body_attach_object_instance_id(RID p_body, uint32_t p_id) {
	CollisionObjectBullet *body = get_collision_object(p_body);
	ERR_FAIL_COND(!body);

	body->set_instance_id(p_id);
}

uint32_t BulletPhysicsServer::body_get_object_instance_id(RID p_body) const {
	CollisionObjectBullet *body = get_collision_object(p_body);
	ERR_FAIL_COND_V(!body, 0);

	return body->get_instance_id();
}

void BulletPhysicsServer::body_set_enable_continuous_collision_detection(RID p_body, bool p_enable) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_continuous_collision_detection(p_enable);
}

bool BulletPhysicsServer::body_is_continuous_collision_detection_enabled(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, false);

	return body->is_continuous_collision_detection_enabled();
}

void BulletPhysicsServer::body_set_collision_layer(RID p_body, uint32_t p_layer) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_collision_layer(p_layer);
}

uint32_t BulletPhysicsServer::body_get_collision_layer(RID p_body) const {
	const RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);

	return body->get_collision_layer();
}

void BulletPhysicsServer::body_set_collision_mask(RID p_body, uint32_t p_mask) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_collision_mask(p_mask);
}

uint32_t BulletPhysicsServer::body_get_collision_mask(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);

	return body->get_collision_mask();
}

void BulletPhysicsServer::body_set_user_flags(RID p_body, uint32_t p_flags) {
	// This function si not currently supported
}

uint32_t BulletPhysicsServer::body_get_user_flags(RID p_body) const {
	// This function si not currently supported
	return 0;
}

void BulletPhysicsServer::body_set_param(RID p_body, BodyParameter p_param, float p_value) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_param(p_param, p_value);
}

float BulletPhysicsServer::body_get_param(RID p_body, BodyParameter p_param) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);

	return body->get_param(p_param);
}

void BulletPhysicsServer::body_set_kinematic_safe_margin(RID p_body, real_t p_margin) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	if (body->get_kinematic_utilities()) {
		body->get_kinematic_utilities()->setSafeMargin(p_margin);
	}
}

real_t BulletPhysicsServer::body_get_kinematic_safe_margin(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);

	if (body->get_kinematic_utilities()) {
		return body->get_kinematic_utilities()->safe_margin;
	}

	return 0;
}

void BulletPhysicsServer::body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_state(p_state, p_variant);
}

Variant BulletPhysicsServer::body_get_state(RID p_body, BodyState p_state) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, Variant());

	return body->get_state(p_state);
}

void BulletPhysicsServer::body_set_applied_force(RID p_body, const Vector3 &p_force) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_applied_force(p_force);
}

Vector3 BulletPhysicsServer::body_get_applied_force(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, Vector3());
	return body->get_applied_force();
}

void BulletPhysicsServer::body_set_applied_torque(RID p_body, const Vector3 &p_torque) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_applied_torque(p_torque);
}

Vector3 BulletPhysicsServer::body_get_applied_torque(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, Vector3());

	return body->get_applied_torque();
}

void BulletPhysicsServer::body_add_central_force(RID p_body, const Vector3 &p_force) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->apply_central_force(p_force);
}

void BulletPhysicsServer::body_add_force(RID p_body, const Vector3 &p_force, const Vector3 &p_pos) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->apply_force(p_force, p_pos);
}

void BulletPhysicsServer::body_add_torque(RID p_body, const Vector3 &p_torque) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->apply_torque(p_torque);
}

void BulletPhysicsServer::body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->apply_central_impulse(p_impulse);
}

void BulletPhysicsServer::body_apply_impulse(RID p_body, const Vector3 &p_pos, const Vector3 &p_impulse) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->apply_impulse(p_pos, p_impulse);
}

void BulletPhysicsServer::body_apply_torque_impulse(RID p_body, const Vector3 &p_impulse) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->apply_torque_impulse(p_impulse);
}

void BulletPhysicsServer::body_set_axis_velocity(RID p_body, const Vector3 &p_axis_velocity) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	Vector3 v = body->get_linear_velocity();
	Vector3 axis = p_axis_velocity.normalized();
	v -= axis * axis.dot(v);
	v += p_axis_velocity;
	body->set_linear_velocity(v);
}

void BulletPhysicsServer::body_set_axis_lock(RID p_body, BodyAxis p_axis, bool p_lock) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_axis_lock(p_axis, p_lock);
}

bool BulletPhysicsServer::body_is_axis_locked(RID p_body, BodyAxis p_axis) const {
	const RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);
	return body->is_axis_locked(p_axis);
}

void BulletPhysicsServer::body_add_collision_exception(RID p_body, RID p_body_b) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	RigidBodyBullet *other_body = rigid_body_owner.get(p_body_b);
	ERR_FAIL_COND(!other_body);

	body->add_collision_exception(other_body);
}

void BulletPhysicsServer::body_remove_collision_exception(RID p_body, RID p_body_b) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	RigidBodyBullet *other_body = rigid_body_owner.get(p_body_b);
	ERR_FAIL_COND(!other_body);

	body->remove_collision_exception(other_body);
}

void BulletPhysicsServer::body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	for (int i = 0; i < body->get_exceptions().size(); i++) {
		p_exceptions->push_back(body->get_exceptions()[i]);
	}
}

void BulletPhysicsServer::body_set_max_contacts_reported(RID p_body, int p_contacts) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_max_collisions_detection(p_contacts);
}

int BulletPhysicsServer::body_get_max_contacts_reported(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);

	return body->get_max_collisions_detection();
}

void BulletPhysicsServer::body_set_contacts_reported_depth_threshold(RID p_body, float p_threshold) {
	// Not supported by bullet and even Godot
}

float BulletPhysicsServer::body_get_contacts_reported_depth_threshold(RID p_body) const {
	// Not supported by bullet and even Godot
	return 0.;
}

void BulletPhysicsServer::body_set_omit_force_integration(RID p_body, bool p_omit) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_omit_forces_integration(p_omit);
}

bool BulletPhysicsServer::body_is_omitting_force_integration(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, false);

	return body->get_omit_forces_integration();
}

void BulletPhysicsServer::body_set_force_integration_callback(RID p_body, Object *p_receiver, const StringName &p_method, const Variant &p_udata) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_force_integration_callback(p_receiver ? p_receiver->get_instance_id() : ObjectID(0), p_method, p_udata);
}

void BulletPhysicsServer::body_set_ray_pickable(RID p_body, bool p_enable) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_ray_pickable(p_enable);
}

bool BulletPhysicsServer::body_is_ray_pickable(RID p_body) const {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, false);
	return body->is_ray_pickable();
}

PhysicsDirectBodyState *BulletPhysicsServer::body_get_direct_state(RID p_body) {
	if (!rigid_body_owner.owns(p_body)) {
		return nullptr;
	}

	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V_MSG(!body, nullptr, "Body with RID " + itos(p_body.get_id()) + " not owned by this server.");

	if (!body->get_space()) {
		return nullptr;
	}

	return body->get_direct_state();
}

bool BulletPhysicsServer::body_test_motion(RID p_body, const Transform &p_from, const Vector3 &p_motion, bool p_infinite_inertia, MotionResult *r_result, bool p_exclude_raycast_shapes, const Set<RID> &p_exclude) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, false);
	ERR_FAIL_COND_V(!body->get_space(), false);

	return body->get_space()->test_body_motion(body, p_from, p_motion, p_infinite_inertia, r_result, p_exclude_raycast_shapes, p_exclude);
}

int BulletPhysicsServer::body_test_ray_separation(RID p_body, const Transform &p_transform, bool p_infinite_inertia, Vector3 &r_recover_motion, SeparationResult *r_results, int p_result_max, float p_margin) {
	RigidBodyBullet *body = rigid_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);
	ERR_FAIL_COND_V(!body->get_space(), 0);

	return body->get_space()->test_ray_separation(body, p_transform, p_infinite_inertia, r_recover_motion, r_results, p_result_max, p_margin);
}

RID BulletPhysicsServer::soft_body_create(bool p_init_sleeping) {
	SoftBodyBullet *body = bulletnew(SoftBodyBullet);
	body->set_collision_layer(1);
	body->set_collision_mask(1);
	if (p_init_sleeping) {
		body->set_activation_state(false);
	}
	CreateThenReturnRID(soft_body_owner, body);
}

void BulletPhysicsServer::soft_body_update_visual_server(RID p_body, class SoftBodyVisualServerHandler *p_visual_server_handler) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->update_visual_server(p_visual_server_handler);
}

void BulletPhysicsServer::soft_body_set_space(RID p_body, RID p_space) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	SpaceBullet *space = nullptr;

	if (p_space.is_valid()) {
		space = space_owner.get(p_space);
		ERR_FAIL_COND(!space);
	}

	if (body->get_space() == space) {
		return; //pointles
	}

	body->set_space(space);
}

RID BulletPhysicsServer::soft_body_get_space(RID p_body) const {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, RID());

	SpaceBullet *space = body->get_space();
	if (!space) {
		return RID();
	}
	return space->get_self();
}

void BulletPhysicsServer::soft_body_set_mesh(RID p_body, const REF &p_mesh) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_soft_mesh(p_mesh);
}

void BulletPhysicsServer::soft_body_set_collision_layer(RID p_body, uint32_t p_layer) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_collision_layer(p_layer);
}

uint32_t BulletPhysicsServer::soft_body_get_collision_layer(RID p_body) const {
	const SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);

	return body->get_collision_layer();
}

void BulletPhysicsServer::soft_body_set_collision_mask(RID p_body, uint32_t p_mask) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_collision_mask(p_mask);
}

uint32_t BulletPhysicsServer::soft_body_get_collision_mask(RID p_body) const {
	const SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0);

	return body->get_collision_mask();
}

void BulletPhysicsServer::soft_body_add_collision_exception(RID p_body, RID p_body_b) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	CollisionObjectBullet *other_body = rigid_body_owner.get(p_body_b);
	if (!other_body) {
		other_body = soft_body_owner.get(p_body_b);
	}
	ERR_FAIL_COND(!other_body);

	body->add_collision_exception(other_body);
}

void BulletPhysicsServer::soft_body_remove_collision_exception(RID p_body, RID p_body_b) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	CollisionObjectBullet *other_body = rigid_body_owner.get(p_body_b);
	if (!other_body) {
		other_body = soft_body_owner.get(p_body_b);
	}
	ERR_FAIL_COND(!other_body);

	body->remove_collision_exception(other_body);
}

void BulletPhysicsServer::soft_body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	for (int i = 0; i < body->get_exceptions().size(); i++) {
		p_exceptions->push_back(body->get_exceptions()[i]);
	}
}

void BulletPhysicsServer::soft_body_set_state(RID p_body, BodyState p_state, const Variant &p_variant) {
	// FIXME: Must be implemented.
	WARN_PRINT("soft_body_state is not implemented yet in Bullet backend.");
}

Variant BulletPhysicsServer::soft_body_get_state(RID p_body, BodyState p_state) const {
	// FIXME: Must be implemented.
	WARN_PRINT("soft_body_state is not implemented yet in Bullet backend.");
	return Variant();
}

void BulletPhysicsServer::soft_body_set_transform(RID p_body, const Transform &p_transform) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);

	body->set_soft_transform(p_transform);
}

Vector3 BulletPhysicsServer::soft_body_get_vertex_position(RID p_body, int vertex_index) const {
	const SoftBodyBullet *body = soft_body_owner.get(p_body);
	Vector3 pos;
	ERR_FAIL_COND_V(!body, pos);

	body->get_node_position(vertex_index, pos);
	return pos;
}

void BulletPhysicsServer::soft_body_set_ray_pickable(RID p_body, bool p_enable) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_ray_pickable(p_enable);
}

bool BulletPhysicsServer::soft_body_is_ray_pickable(RID p_body) const {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, false);
	return body->is_ray_pickable();
}

void BulletPhysicsServer::soft_body_set_simulation_precision(RID p_body, int p_simulation_precision) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_simulation_precision(p_simulation_precision);
}

int BulletPhysicsServer::soft_body_get_simulation_precision(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_simulation_precision();
}

void BulletPhysicsServer::soft_body_set_total_mass(RID p_body, real_t p_total_mass) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_total_mass(p_total_mass);
}

real_t BulletPhysicsServer::soft_body_get_total_mass(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_total_mass();
}

void BulletPhysicsServer::soft_body_set_linear_stiffness(RID p_body, real_t p_stiffness) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_linear_stiffness(p_stiffness);
}

real_t BulletPhysicsServer::soft_body_get_linear_stiffness(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_linear_stiffness();
}

void BulletPhysicsServer::soft_body_set_areaAngular_stiffness(RID p_body, real_t p_stiffness) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_areaAngular_stiffness(p_stiffness);
}

real_t BulletPhysicsServer::soft_body_get_areaAngular_stiffness(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_areaAngular_stiffness();
}

void BulletPhysicsServer::soft_body_set_volume_stiffness(RID p_body, real_t p_stiffness) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_volume_stiffness(p_stiffness);
}

real_t BulletPhysicsServer::soft_body_get_volume_stiffness(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_volume_stiffness();
}

void BulletPhysicsServer::soft_body_set_pressure_coefficient(RID p_body, real_t p_pressure_coefficient) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_pressure_coefficient(p_pressure_coefficient);
}

real_t BulletPhysicsServer::soft_body_get_pressure_coefficient(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_pressure_coefficient();
}

void BulletPhysicsServer::soft_body_set_pose_matching_coefficient(RID p_body, real_t p_pose_matching_coefficient) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	return body->set_pose_matching_coefficient(p_pose_matching_coefficient);
}

real_t BulletPhysicsServer::soft_body_get_pose_matching_coefficient(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_pose_matching_coefficient();
}

void BulletPhysicsServer::soft_body_set_damping_coefficient(RID p_body, real_t p_damping_coefficient) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_damping_coefficient(p_damping_coefficient);
}

real_t BulletPhysicsServer::soft_body_get_damping_coefficient(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_damping_coefficient();
}

void BulletPhysicsServer::soft_body_set_drag_coefficient(RID p_body, real_t p_drag_coefficient) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_drag_coefficient(p_drag_coefficient);
}

real_t BulletPhysicsServer::soft_body_get_drag_coefficient(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_drag_coefficient();
}

void BulletPhysicsServer::soft_body_move_point(RID p_body, int p_point_index, const Vector3 &p_global_position) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_node_position(p_point_index, p_global_position);
}

Vector3 BulletPhysicsServer::soft_body_get_point_global_position(RID p_body, int p_point_index) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, Vector3(0., 0., 0.));
	Vector3 pos;
	body->get_node_position(p_point_index, pos);
	return pos;
}

Vector3 BulletPhysicsServer::soft_body_get_point_offset(RID p_body, int p_point_index) const {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, Vector3());
	Vector3 res;
	body->get_node_offset(p_point_index, res);
	return res;
}

void BulletPhysicsServer::soft_body_remove_all_pinned_points(RID p_body) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->reset_all_node_mass();
}

void BulletPhysicsServer::soft_body_pin_point(RID p_body, int p_point_index, bool p_pin) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND(!body);
	body->set_node_mass(p_point_index, p_pin ? 0 : 1);
}

bool BulletPhysicsServer::soft_body_is_point_pinned(RID p_body, int p_point_index) {
	SoftBodyBullet *body = soft_body_owner.get(p_body);
	ERR_FAIL_COND_V(!body, 0.f);
	return body->get_node_mass(p_point_index);
}

PhysicsServer::JointType BulletPhysicsServer::joint_get_type(RID p_joint) const {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, JOINT_PIN);
	return joint->get_type();
}

void BulletPhysicsServer::joint_set_solver_priority(RID p_joint, int p_priority) {
	// Joint priority not supported by bullet
}

int BulletPhysicsServer::joint_get_solver_priority(RID p_joint) const {
	// Joint priority not supported by bullet
	return 0;
}

void BulletPhysicsServer::joint_disable_collisions_between_bodies(RID p_joint, const bool p_disable) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);

	joint->disable_collisions_between_bodies(p_disable);
}

bool BulletPhysicsServer::joint_is_disabled_collisions_between_bodies(RID p_joint) const {
	JointBullet *joint(joint_owner.get(p_joint));
	ERR_FAIL_COND_V(!joint, false);

	return joint->is_disabled_collisions_between_bodies();
}

RID BulletPhysicsServer::joint_create_pin(RID p_body_A, const Vector3 &p_local_A, RID p_body_B, const Vector3 &p_local_B) {
	RigidBodyBullet *body_A = rigid_body_owner.get(p_body_A);
	ERR_FAIL_COND_V(!body_A, RID());

	JointAssertSpace(body_A, "A", RID());

	RigidBodyBullet *body_B = nullptr;
	if (p_body_B.is_valid()) {
		body_B = rigid_body_owner.get(p_body_B);
		JointAssertSpace(body_B, "B", RID());
		JointAssertSameSpace(body_A, body_B, RID());
	}

	ERR_FAIL_COND_V(body_A == body_B, RID());

	JointBullet *joint = bulletnew(PinJointBullet(body_A, p_local_A, body_B, p_local_B));
	AddJointToSpace(body_A, joint);

	CreateThenReturnRID(joint_owner, joint);
}

void BulletPhysicsServer::pin_joint_set_param(RID p_joint, PinJointParam p_param, float p_value) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_PIN);
	PinJointBullet *pin_joint = static_cast<PinJointBullet *>(joint);
	pin_joint->set_param(p_param, p_value);
}

float BulletPhysicsServer::pin_joint_get_param(RID p_joint, PinJointParam p_param) const {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, 0);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_PIN, 0);
	PinJointBullet *pin_joint = static_cast<PinJointBullet *>(joint);
	return pin_joint->get_param(p_param);
}

void BulletPhysicsServer::pin_joint_set_local_a(RID p_joint, const Vector3 &p_A) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_PIN);
	PinJointBullet *pin_joint = static_cast<PinJointBullet *>(joint);
	pin_joint->setPivotInA(p_A);
}

Vector3 BulletPhysicsServer::pin_joint_get_local_a(RID p_joint) const {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, Vector3());
	ERR_FAIL_COND_V(joint->get_type() != JOINT_PIN, Vector3());
	PinJointBullet *pin_joint = static_cast<PinJointBullet *>(joint);
	return pin_joint->getPivotInA();
}

void BulletPhysicsServer::pin_joint_set_local_b(RID p_joint, const Vector3 &p_B) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_PIN);
	PinJointBullet *pin_joint = static_cast<PinJointBullet *>(joint);
	pin_joint->setPivotInB(p_B);
}

Vector3 BulletPhysicsServer::pin_joint_get_local_b(RID p_joint) const {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, Vector3());
	ERR_FAIL_COND_V(joint->get_type() != JOINT_PIN, Vector3());
	PinJointBullet *pin_joint = static_cast<PinJointBullet *>(joint);
	return pin_joint->getPivotInB();
}

RID BulletPhysicsServer::joint_create_hinge(RID p_body_A, const Transform &p_hinge_A, RID p_body_B, const Transform &p_hinge_B) {
	RigidBodyBullet *body_A = rigid_body_owner.get(p_body_A);
	ERR_FAIL_COND_V(!body_A, RID());
	JointAssertSpace(body_A, "A", RID());

	RigidBodyBullet *body_B = nullptr;
	if (p_body_B.is_valid()) {
		body_B = rigid_body_owner.get(p_body_B);
		JointAssertSpace(body_B, "B", RID());
		JointAssertSameSpace(body_A, body_B, RID());
	}

	ERR_FAIL_COND_V(body_A == body_B, RID());

	JointBullet *joint = bulletnew(HingeJointBullet(body_A, body_B, p_hinge_A, p_hinge_B));
	AddJointToSpace(body_A, joint);

	CreateThenReturnRID(joint_owner, joint);
}

RID BulletPhysicsServer::joint_create_hinge_simple(RID p_body_A, const Vector3 &p_pivot_A, const Vector3 &p_axis_A, RID p_body_B, const Vector3 &p_pivot_B, const Vector3 &p_axis_B) {
	RigidBodyBullet *body_A = rigid_body_owner.get(p_body_A);
	ERR_FAIL_COND_V(!body_A, RID());
	JointAssertSpace(body_A, "A", RID());

	RigidBodyBullet *body_B = nullptr;
	if (p_body_B.is_valid()) {
		body_B = rigid_body_owner.get(p_body_B);
		JointAssertSpace(body_B, "B", RID());
		JointAssertSameSpace(body_A, body_B, RID());
	}

	ERR_FAIL_COND_V(body_A == body_B, RID());

	JointBullet *joint = bulletnew(HingeJointBullet(body_A, body_B, p_pivot_A, p_pivot_B, p_axis_A, p_axis_B));
	AddJointToSpace(body_A, joint);

	CreateThenReturnRID(joint_owner, joint);
}

void BulletPhysicsServer::hinge_joint_set_param(RID p_joint, HingeJointParam p_param, float p_value) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_HINGE);
	HingeJointBullet *hinge_joint = static_cast<HingeJointBullet *>(joint);
	hinge_joint->set_param(p_param, p_value);
}

float BulletPhysicsServer::hinge_joint_get_param(RID p_joint, HingeJointParam p_param) const {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, 0);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_HINGE, 0);
	HingeJointBullet *hinge_joint = static_cast<HingeJointBullet *>(joint);
	return hinge_joint->get_param(p_param);
}

void BulletPhysicsServer::hinge_joint_set_flag(RID p_joint, HingeJointFlag p_flag, bool p_value) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_HINGE);
	HingeJointBullet *hinge_joint = static_cast<HingeJointBullet *>(joint);
	hinge_joint->set_flag(p_flag, p_value);
}

bool BulletPhysicsServer::hinge_joint_get_flag(RID p_joint, HingeJointFlag p_flag) const {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, false);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_HINGE, false);
	HingeJointBullet *hinge_joint = static_cast<HingeJointBullet *>(joint);
	return hinge_joint->get_flag(p_flag);
}

RID BulletPhysicsServer::joint_create_slider(RID p_body_A, const Transform &p_local_frame_A, RID p_body_B, const Transform &p_local_frame_B) {
	RigidBodyBullet *body_A = rigid_body_owner.get(p_body_A);
	ERR_FAIL_COND_V(!body_A, RID());
	JointAssertSpace(body_A, "A", RID());

	RigidBodyBullet *body_B = nullptr;
	if (p_body_B.is_valid()) {
		body_B = rigid_body_owner.get(p_body_B);
		JointAssertSpace(body_B, "B", RID());
		JointAssertSameSpace(body_A, body_B, RID());
	}

	ERR_FAIL_COND_V(body_A == body_B, RID());

	JointBullet *joint = bulletnew(SliderJointBullet(body_A, body_B, p_local_frame_A, p_local_frame_B));
	AddJointToSpace(body_A, joint);

	CreateThenReturnRID(joint_owner, joint);
}

void BulletPhysicsServer::slider_joint_set_param(RID p_joint, SliderJointParam p_param, float p_value) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_SLIDER);
	SliderJointBullet *slider_joint = static_cast<SliderJointBullet *>(joint);
	slider_joint->set_param(p_param, p_value);
}

float BulletPhysicsServer::slider_joint_get_param(RID p_joint, SliderJointParam p_param) const {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, 0);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_SLIDER, 0);
	SliderJointBullet *slider_joint = static_cast<SliderJointBullet *>(joint);
	return slider_joint->get_param(p_param);
}

RID BulletPhysicsServer::joint_create_cone_twist(RID p_body_A, const Transform &p_local_frame_A, RID p_body_B, const Transform &p_local_frame_B) {
	RigidBodyBullet *body_A = rigid_body_owner.get(p_body_A);
	ERR_FAIL_COND_V(!body_A, RID());
	JointAssertSpace(body_A, "A", RID());

	RigidBodyBullet *body_B = nullptr;
	if (p_body_B.is_valid()) {
		body_B = rigid_body_owner.get(p_body_B);
		JointAssertSpace(body_B, "B", RID());
		JointAssertSameSpace(body_A, body_B, RID());
	}

	JointBullet *joint = bulletnew(ConeTwistJointBullet(body_A, body_B, p_local_frame_A, p_local_frame_B));
	AddJointToSpace(body_A, joint);

	CreateThenReturnRID(joint_owner, joint);
}

void BulletPhysicsServer::cone_twist_joint_set_param(RID p_joint, ConeTwistJointParam p_param, float p_value) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_CONE_TWIST);
	ConeTwistJointBullet *coneTwist_joint = static_cast<ConeTwistJointBullet *>(joint);
	coneTwist_joint->set_param(p_param, p_value);
}

float BulletPhysicsServer::cone_twist_joint_get_param(RID p_joint, ConeTwistJointParam p_param) const {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, 0.);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_CONE_TWIST, 0.);
	ConeTwistJointBullet *coneTwist_joint = static_cast<ConeTwistJointBullet *>(joint);
	return coneTwist_joint->get_param(p_param);
}

RID BulletPhysicsServer::joint_create_generic_6dof(RID p_body_A, const Transform &p_local_frame_A, RID p_body_B, const Transform &p_local_frame_B) {
	RigidBodyBullet *body_A = rigid_body_owner.get(p_body_A);
	ERR_FAIL_COND_V(!body_A, RID());
	JointAssertSpace(body_A, "A", RID());

	RigidBodyBullet *body_B = nullptr;
	if (p_body_B.is_valid()) {
		body_B = rigid_body_owner.get(p_body_B);
		JointAssertSpace(body_B, "B", RID());
		JointAssertSameSpace(body_A, body_B, RID());
	}

	ERR_FAIL_COND_V(body_A == body_B, RID());

	JointBullet *joint = bulletnew(Generic6DOFJointBullet(body_A, body_B, p_local_frame_A, p_local_frame_B));
	AddJointToSpace(body_A, joint);

	CreateThenReturnRID(joint_owner, joint);
}

void BulletPhysicsServer::generic_6dof_joint_set_param(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisParam p_param, float p_value) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_6DOF);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	generic_6dof_joint->set_param(p_axis, p_param, p_value);
}

float BulletPhysicsServer::generic_6dof_joint_get_param(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisParam p_param) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, 0);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_6DOF, 0);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	return generic_6dof_joint->get_param(p_axis, p_param);
}

void BulletPhysicsServer::generic_6dof_joint_set_flag(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisFlag p_flag, bool p_enable) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_6DOF);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	generic_6dof_joint->set_flag(p_axis, p_flag, p_enable);
}

bool BulletPhysicsServer::generic_6dof_joint_get_flag(RID p_joint, Vector3::Axis p_axis, G6DOFJointAxisFlag p_flag) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, false);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_6DOF, false);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	return generic_6dof_joint->get_flag(p_axis, p_flag);
}

void BulletPhysicsServer::generic_6dof_joint_set_use_global_rotation(RID p_joint, bool p_enable) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_6DOF);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	generic_6dof_joint->set_use_global_rotation(p_enable);
}

bool BulletPhysicsServer::generic_6dof_joint_get_use_global_rotation(RID p_joint) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, false);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_6DOF, false);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	return generic_6dof_joint->get_use_global_rotation();
}

void BulletPhysicsServer::generic_6dof_joint_set_use_quaternion_rotation_equilibrium(RID p_joint, bool p_enable) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_6DOF);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	generic_6dof_joint->set_use_quaternion_rotation_equilibrium(p_enable);
}
bool BulletPhysicsServer::generic_6dof_joint_get_use_quaternion_rotation_equilibrium(RID p_joint) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, false);
	ERR_FAIL_COND_V(joint->get_type() != JOINT_6DOF, false);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	return generic_6dof_joint->get_use_quaternion_rotation_equilibrium();
}
void BulletPhysicsServer::generic_6dof_joint_set_quaternion_rotation_equilibrium(RID p_joint, Quat p_value) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND(!joint);
	ERR_FAIL_COND(joint->get_type() != JOINT_6DOF);
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	generic_6dof_joint->set_quaternion_rotation_equilibrium(p_value);
}
Quat BulletPhysicsServer::generic_6dof_joint_get_quaternion_rotation_equilibrium(RID p_joint) {
	JointBullet *joint = joint_owner.get(p_joint);
	ERR_FAIL_COND_V(!joint, Quat());
	ERR_FAIL_COND_V(joint->get_type() != JOINT_6DOF, Quat());
	Generic6DOFJointBullet *generic_6dof_joint = static_cast<Generic6DOFJointBullet *>(joint);
	return generic_6dof_joint->get_quaternion_rotation_equilibrium();
}

void BulletPhysicsServer::free(RID p_rid) {
	if (!p_rid.is_valid()) {
		ERR_FAIL_MSG("PhysicsServer attempted to free a NULL RID.");
		return;
	}

	if (shape_owner.owns(p_rid)) {
		ShapeBullet *shape = shape_owner.get(p_rid);

		// Notify the shape is configured
		for (Map<ShapeOwnerBullet *, int>::Element *element = shape->get_owners().front(); element; element = element->next()) {
			static_cast<ShapeOwnerBullet *>(element->key())->remove_shape_full(shape);
		}

		shape_owner.free(p_rid);
		bulletdelete(shape);

	} else if (rigid_body_owner.owns(p_rid)) {
		RigidBodyBullet *body = rigid_body_owner.get(p_rid);

		body->set_space(nullptr);

		body->remove_all_shapes(true, true);

		rigid_body_owner.free(p_rid);
		bulletdelete(body);

	} else if (soft_body_owner.owns(p_rid)) {
		SoftBodyBullet *body = soft_body_owner.get(p_rid);

		body->set_space(nullptr);

		soft_body_owner.free(p_rid);
		bulletdelete(body);

	} else if (area_owner.owns(p_rid)) {
		AreaBullet *area = area_owner.get(p_rid);

		area->set_space(nullptr);

		area->remove_all_shapes(true, true);

		area_owner.free(p_rid);
		bulletdelete(area);

	} else if (joint_owner.owns(p_rid)) {
		JointBullet *joint = joint_owner.get(p_rid);
		joint->destroy_internal_constraint();
		joint_owner.free(p_rid);
		bulletdelete(joint);

	} else if (space_owner.owns(p_rid)) {
		SpaceBullet *space = space_owner.get(p_rid);

		space->remove_all_collision_objects();

		space_set_active(p_rid, false);
		space_owner.free(p_rid);
		bulletdelete(space);

	} else {
		ERR_FAIL_MSG("RID not found by PhysicsServer.");
	}
}

void BulletPhysicsServer::init() {
}

void BulletPhysicsServer::step(float p_deltaTime) {
	if (!active) {
		return;
	}

	for (int i = 0; i < active_spaces_count; ++i) {
		active_spaces[i]->step(p_deltaTime);
	}
}

void BulletPhysicsServer::flush_queries() {
}

void BulletPhysicsServer::finish() {
}

void BulletPhysicsServer::set_collision_iterations(int p_iterations) {
	WARN_PRINT("Changing the number of 3D physics collision iterations is only supported when using GodotPhysics, not Bullet.");
}

int BulletPhysicsServer::get_process_info(ProcessInfo p_info) {
	return 0;
}

CollisionObjectBullet *BulletPhysicsServer::get_collision_object(RID p_object) const {
	if (rigid_body_owner.owns(p_object)) {
		return rigid_body_owner.getornull(p_object);
	}
	if (area_owner.owns(p_object)) {
		return area_owner.getornull(p_object);
	}
	if (soft_body_owner.owns(p_object)) {
		return soft_body_owner.getornull(p_object);
	}
	return nullptr;
}

RigidCollisionObjectBullet *BulletPhysicsServer::get_rigid_collision_object(RID p_object) const {
	if (rigid_body_owner.owns(p_object)) {
		return rigid_body_owner.getornull(p_object);
	}
	if (area_owner.owns(p_object)) {
		return area_owner.getornull(p_object);
	}
	return nullptr;
}
