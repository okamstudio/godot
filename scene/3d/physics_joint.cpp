/**************************************************************************/
/*  physics_joint.cpp                                                     */
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

#include "physics_joint.h"

#include "scene/scene_string_names.h"

void Joint::_disconnect_signals() {
	Node *node_a = get_node_or_null(a);
	PhysicsBody *body_a = Object::cast_to<PhysicsBody>(node_a);
	if (body_a) {
		body_a->disconnect(SceneStringNames::get_singleton()->tree_exiting, this, SceneStringNames::get_singleton()->_body_exit_tree);
	}

	Node *node_b = get_node_or_null(b);
	PhysicsBody *body_b = Object::cast_to<PhysicsBody>(node_b);
	if (body_b) {
		body_b->disconnect(SceneStringNames::get_singleton()->tree_exiting, this, SceneStringNames::get_singleton()->_body_exit_tree);
	}
}

void Joint::_body_exit_tree() {
	_disconnect_signals();
	_update_joint(true);
}

void Joint::_update_joint(bool p_only_free) {
	if (joint.is_valid()) {
		if (ba.is_valid() && bb.is_valid()) {
			PhysicsServer::get_singleton()->body_remove_collision_exception(ba, bb);
			PhysicsServer::get_singleton()->body_remove_collision_exception(bb, ba);
		}

		PhysicsServer::get_singleton()->free(joint);
		joint = RID();
		ba = RID();
		bb = RID();
	}

	if (p_only_free || !is_inside_tree()) {
		warning = String();
		return;
	}

	Node *node_a = get_node_or_null(a);
	Node *node_b = get_node_or_null(b);

	PhysicsBody *body_a = Object::cast_to<PhysicsBody>(node_a);
	PhysicsBody *body_b = Object::cast_to<PhysicsBody>(node_b);

	if (node_a && !body_a && node_b && !body_b) {
		warning = TTR("Node A and Node B must be PhysicsBodies");
		update_configuration_warning();
		return;
	}

	if (node_a && !body_a) {
		warning = TTR("Node A must be a PhysicsBody");
		update_configuration_warning();
		return;
	}

	if (node_b && !body_b) {
		warning = TTR("Node B must be a PhysicsBody");
		update_configuration_warning();
		return;
	}

	if (!body_a && !body_b) {
		warning = TTR("Joint is not connected to any PhysicsBodies");
		update_configuration_warning();
		return;
	}

	if (body_a == body_b) {
		warning = TTR("Node A and Node B must be different PhysicsBodies");
		update_configuration_warning();
		return;
	}

	warning = String();
	update_configuration_warning();

	if (body_a) {
		joint = _configure_joint(body_a, body_b);
	} else if (body_b) {
		joint = _configure_joint(body_b, nullptr);
	}

	ERR_FAIL_COND_MSG(!joint.is_valid(), "Failed to configure the joint.");

	PhysicsServer::get_singleton()->joint_set_solver_priority(joint, solver_priority);

	if (body_a) {
		ba = body_a->get_rid();
		body_a->connect(SceneStringNames::get_singleton()->tree_exiting, this, SceneStringNames::get_singleton()->_body_exit_tree);
	}

	if (body_b) {
		bb = body_b->get_rid();
		body_b->connect(SceneStringNames::get_singleton()->tree_exiting, this, SceneStringNames::get_singleton()->_body_exit_tree);
	}

	PhysicsServer::get_singleton()->joint_disable_collisions_between_bodies(joint, exclude_from_collision);
}

void Joint::set_node_a(const NodePath &p_node_a) {
	if (a == p_node_a) {
		return;
	}

	if (joint.is_valid()) {
		_disconnect_signals();
	}

	a = p_node_a;
	_update_joint();
}

NodePath Joint::get_node_a() const {
	return a;
}

void Joint::set_node_b(const NodePath &p_node_b) {
	if (b == p_node_b) {
		return;
	}

	if (joint.is_valid()) {
		_disconnect_signals();
	}

	b = p_node_b;
	_update_joint();
}

NodePath Joint::get_node_b() const {
	return b;
}

void Joint::set_solver_priority(int p_priority) {
	solver_priority = p_priority;
	if (joint.is_valid()) {
		PhysicsServer::get_singleton()->joint_set_solver_priority(joint, solver_priority);
	}
}

int Joint::get_solver_priority() const {
	return solver_priority;
}

void Joint::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			if (joint.is_valid()) {
				_disconnect_signals();
			}
			_update_joint();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (joint.is_valid()) {
				_disconnect_signals();
			}
			_update_joint(true);
		} break;
	}
}

void Joint::set_exclude_nodes_from_collision(bool p_enable) {
	if (exclude_from_collision == p_enable) {
		return;
	}
	if (joint.is_valid()) {
		_disconnect_signals();
	}
	_update_joint(true);
	exclude_from_collision = p_enable;
	_update_joint();
}

bool Joint::get_exclude_nodes_from_collision() const {
	return exclude_from_collision;
}

String Joint::get_configuration_warning() const {
	String node_warning = Node::get_configuration_warning();

	if (!warning.empty()) {
		if (!node_warning.empty()) {
			node_warning += "\n\n";
		}
		node_warning += warning;
	}

	return node_warning;
}

void Joint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_body_exit_tree"), &Joint::_body_exit_tree);

	ClassDB::bind_method(D_METHOD("set_node_a", "node"), &Joint::set_node_a);
	ClassDB::bind_method(D_METHOD("get_node_a"), &Joint::get_node_a);

	ClassDB::bind_method(D_METHOD("set_node_b", "node"), &Joint::set_node_b);
	ClassDB::bind_method(D_METHOD("get_node_b"), &Joint::get_node_b);

	ClassDB::bind_method(D_METHOD("set_solver_priority", "priority"), &Joint::set_solver_priority);
	ClassDB::bind_method(D_METHOD("get_solver_priority"), &Joint::get_solver_priority);

	ClassDB::bind_method(D_METHOD("set_exclude_nodes_from_collision", "enable"), &Joint::set_exclude_nodes_from_collision);
	ClassDB::bind_method(D_METHOD("get_exclude_nodes_from_collision"), &Joint::get_exclude_nodes_from_collision);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "nodes/node_a", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "PhysicsBody"), "set_node_a", "get_node_a");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "nodes/node_b", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "PhysicsBody"), "set_node_b", "get_node_b");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "solver/priority", PROPERTY_HINT_RANGE, "1,8,1"), "set_solver_priority", "get_solver_priority");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision/exclude_nodes"), "set_exclude_nodes_from_collision", "get_exclude_nodes_from_collision");
}

Joint::Joint() {
	exclude_from_collision = true;
	solver_priority = 1;
	set_notify_transform(true);
}

///////////////////////////////////

void PinJoint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &PinJoint::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &PinJoint::get_param);

	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "params/bias", PROPERTY_HINT_RANGE, "0.01,0.99,0.01"), "set_param", "get_param", PARAM_BIAS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "params/damping", PROPERTY_HINT_RANGE, "0.01,8.0,0.01"), "set_param", "get_param", PARAM_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "params/impulse_clamp", PROPERTY_HINT_RANGE, "0.0,64.0,0.01"), "set_param", "get_param", PARAM_IMPULSE_CLAMP);

	BIND_ENUM_CONSTANT(PARAM_BIAS);
	BIND_ENUM_CONSTANT(PARAM_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_IMPULSE_CLAMP);
}

void PinJoint::set_param(Param p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, 3);
	params[p_param] = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->pin_joint_set_param(get_joint(), PhysicsServer::PinJointParam(p_param), p_value);
	}
}
float PinJoint::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, 3, 0);
	return params[p_param];
}

RID PinJoint::_configure_joint(PhysicsBody *body_a, PhysicsBody *body_b) {
	Vector3 pinpos = get_global_transform().origin;
	Vector3 local_a = body_a->get_global_transform().affine_inverse().xform(pinpos);
	Vector3 local_b;

	if (body_b) {
		local_b = body_b->get_global_transform().affine_inverse().xform(pinpos);
	} else {
		local_b = pinpos;
	}

	RID j = PhysicsServer::get_singleton()->joint_create_pin(body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < 3; i++) {
		PhysicsServer::get_singleton()->pin_joint_set_param(j, PhysicsServer::PinJointParam(i), params[i]);
	}
	return j;
}

PinJoint::PinJoint() {
	params[PARAM_BIAS] = 0.3;
	params[PARAM_DAMPING] = 1;
	params[PARAM_IMPULSE_CLAMP] = 0;
}

/////////////////////////////////////////////////

///////////////////////////////////

void HingeJoint::_set_upper_limit(float p_limit) {
	set_param(PARAM_LIMIT_UPPER, Math::deg2rad(p_limit));
}

float HingeJoint::_get_upper_limit() const {
	return Math::rad2deg(get_param(PARAM_LIMIT_UPPER));
}

void HingeJoint::_set_lower_limit(float p_limit) {
	set_param(PARAM_LIMIT_LOWER, Math::deg2rad(p_limit));
}

float HingeJoint::_get_lower_limit() const {
	return Math::rad2deg(get_param(PARAM_LIMIT_LOWER));
}

void HingeJoint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &HingeJoint::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &HingeJoint::get_param);

	ClassDB::bind_method(D_METHOD("set_flag", "flag", "enabled"), &HingeJoint::set_flag);
	ClassDB::bind_method(D_METHOD("get_flag", "flag"), &HingeJoint::get_flag);

	ClassDB::bind_method(D_METHOD("_set_upper_limit", "upper_limit"), &HingeJoint::_set_upper_limit);
	ClassDB::bind_method(D_METHOD("_get_upper_limit"), &HingeJoint::_get_upper_limit);

	ClassDB::bind_method(D_METHOD("_set_lower_limit", "lower_limit"), &HingeJoint::_set_lower_limit);
	ClassDB::bind_method(D_METHOD("_get_lower_limit"), &HingeJoint::_get_lower_limit);

	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "params/bias", PROPERTY_HINT_RANGE, "0.00,0.99,0.01"), "set_param", "get_param", PARAM_BIAS);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_limit/enable"), "set_flag", "get_flag", FLAG_USE_LIMIT);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit/upper", PROPERTY_HINT_RANGE, "-180,180,0.1"), "_set_upper_limit", "_get_upper_limit");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit/lower", PROPERTY_HINT_RANGE, "-180,180,0.1"), "_set_lower_limit", "_get_lower_limit");
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit/bias", PROPERTY_HINT_RANGE, "0.01,0.99,0.01"), "set_param", "get_param", PARAM_LIMIT_BIAS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param", "get_param", PARAM_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit/relaxation", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param", "get_param", PARAM_LIMIT_RELAXATION);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "motor/enable"), "set_flag", "get_flag", FLAG_ENABLE_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "motor/target_velocity", PROPERTY_HINT_RANGE, "-200,200,0.01,or_greater,or_lesser"), "set_param", "get_param", PARAM_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "motor/max_impulse", PROPERTY_HINT_RANGE, "0.01,1024,0.01"), "set_param", "get_param", PARAM_MOTOR_MAX_IMPULSE);

	BIND_ENUM_CONSTANT(PARAM_BIAS);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_BIAS);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LIMIT_RELAXATION);
	BIND_ENUM_CONSTANT(PARAM_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_MOTOR_MAX_IMPULSE);
	BIND_ENUM_CONSTANT(PARAM_MAX);

	BIND_ENUM_CONSTANT(FLAG_USE_LIMIT);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_MOTOR);
	BIND_ENUM_CONSTANT(FLAG_MAX);
}

void HingeJoint::set_param(Param p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params[p_param] = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->hinge_joint_set_param(get_joint(), PhysicsServer::HingeJointParam(p_param), p_value);
	}

	update_gizmo();
}
float HingeJoint::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params[p_param];
}

void HingeJoint::set_flag(Flag p_flag, bool p_value) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags[p_flag] = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->hinge_joint_set_flag(get_joint(), PhysicsServer::HingeJointFlag(p_flag), p_value);
	}

	update_gizmo();
}
bool HingeJoint::get_flag(Flag p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags[p_flag];
}

RID HingeJoint::_configure_joint(PhysicsBody *body_a, PhysicsBody *body_b) {
	Transform gt = get_global_transform();
	Transform ainv = body_a->get_global_transform().affine_inverse();

	Transform local_a = ainv * gt;
	local_a.orthonormalize();
	Transform local_b = gt;

	if (body_b) {
		Transform binv = body_b->get_global_transform().affine_inverse();
		local_b = binv * gt;
	}

	local_b.orthonormalize();

	RID j = RID_PRIME(PhysicsServer::get_singleton()->joint_create_hinge(body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b));
	for (int i = 0; i < PARAM_MAX; i++) {
		PhysicsServer::get_singleton()->hinge_joint_set_param(j, PhysicsServer::HingeJointParam(i), params[i]);
	}
	for (int i = 0; i < FLAG_MAX; i++) {
		set_flag(Flag(i), flags[i]);
		PhysicsServer::get_singleton()->hinge_joint_set_flag(j, PhysicsServer::HingeJointFlag(i), flags[i]);
	}
	return j;
}

HingeJoint::HingeJoint() {
	params[PARAM_BIAS] = 0.3;
	params[PARAM_LIMIT_UPPER] = Math_PI * 0.5;
	params[PARAM_LIMIT_LOWER] = -Math_PI * 0.5;
	params[PARAM_LIMIT_BIAS] = 0.3;
	params[PARAM_LIMIT_SOFTNESS] = 0.9;
	params[PARAM_LIMIT_RELAXATION] = 1.0;
	params[PARAM_MOTOR_TARGET_VELOCITY] = 1;
	params[PARAM_MOTOR_MAX_IMPULSE] = 1;

	flags[FLAG_USE_LIMIT] = false;
	flags[FLAG_ENABLE_MOTOR] = false;
}

/////////////////////////////////////////////////

//////////////////////////////////

void SliderJoint::_set_upper_limit_angular(float p_limit_angular) {
	set_param(PARAM_ANGULAR_LIMIT_UPPER, Math::deg2rad(p_limit_angular));
}

float SliderJoint::_get_upper_limit_angular() const {
	return Math::rad2deg(get_param(PARAM_ANGULAR_LIMIT_UPPER));
}

void SliderJoint::_set_lower_limit_angular(float p_limit_angular) {
	set_param(PARAM_ANGULAR_LIMIT_LOWER, Math::deg2rad(p_limit_angular));
}

float SliderJoint::_get_lower_limit_angular() const {
	return Math::rad2deg(get_param(PARAM_ANGULAR_LIMIT_LOWER));
}

void SliderJoint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &SliderJoint::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &SliderJoint::get_param);

	ClassDB::bind_method(D_METHOD("_set_upper_limit_angular", "upper_limit_angular"), &SliderJoint::_set_upper_limit_angular);
	ClassDB::bind_method(D_METHOD("_get_upper_limit_angular"), &SliderJoint::_get_upper_limit_angular);

	ClassDB::bind_method(D_METHOD("_set_lower_limit_angular", "lower_limit_angular"), &SliderJoint::_set_lower_limit_angular);
	ClassDB::bind_method(D_METHOD("_get_lower_limit_angular"), &SliderJoint::_get_lower_limit_angular);

	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit/upper_distance", PROPERTY_HINT_RANGE, "-1024,1024,0.01"), "set_param", "get_param", PARAM_LINEAR_LIMIT_UPPER);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit/lower_distance", PROPERTY_HINT_RANGE, "-1024,1024,0.01"), "set_param", "get_param", PARAM_LINEAR_LIMIT_LOWER);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit/softness", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_LIMIT_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit/damping", PROPERTY_HINT_RANGE, "0,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_LIMIT_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motion/softness", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_MOTION_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motion/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_MOTION_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motion/damping", PROPERTY_HINT_RANGE, "0,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_MOTION_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_ortho/softness", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_ORTHOGONAL_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_ortho/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_ORTHOGONAL_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_ortho/damping", PROPERTY_HINT_RANGE, "0,16.0,0.01"), "set_param", "get_param", PARAM_LINEAR_ORTHOGONAL_DAMPING);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit/upper_angle", PROPERTY_HINT_RANGE, "-180,180,0.1"), "_set_upper_limit_angular", "_get_upper_limit_angular");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit/lower_angle", PROPERTY_HINT_RANGE, "-180,180,0.1"), "_set_lower_limit_angular", "_get_lower_limit_angular");
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit/softness", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_LIMIT_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit/damping", PROPERTY_HINT_RANGE, "0,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_LIMIT_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motion/softness", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_MOTION_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motion/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_MOTION_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motion/damping", PROPERTY_HINT_RANGE, "0,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_MOTION_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_ortho/softness", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_ORTHOGONAL_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_ortho/restitution", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_ORTHOGONAL_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_ortho/damping", PROPERTY_HINT_RANGE, "0,16.0,0.01"), "set_param", "get_param", PARAM_ANGULAR_ORTHOGONAL_DAMPING);

	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTION_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTION_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTION_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_ORTHOGONAL_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_ORTHOGONAL_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_ORTHOGONAL_DAMPING);

	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_UPPER);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_LOWER);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTION_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTION_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTION_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_ORTHOGONAL_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_ORTHOGONAL_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_ORTHOGONAL_DAMPING);

	BIND_ENUM_CONSTANT(PARAM_MAX);
}

void SliderJoint::set_param(Param p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params[p_param] = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->slider_joint_set_param(get_joint(), PhysicsServer::SliderJointParam(p_param), p_value);
	}
	update_gizmo();
}
float SliderJoint::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params[p_param];
}

RID SliderJoint::_configure_joint(PhysicsBody *body_a, PhysicsBody *body_b) {
	Transform gt = get_global_transform();
	Transform ainv = body_a->get_global_transform().affine_inverse();

	Transform local_a = ainv * gt;
	local_a.orthonormalize();
	Transform local_b = gt;

	if (body_b) {
		Transform binv = body_b->get_global_transform().affine_inverse();
		local_b = binv * gt;
	}

	local_b.orthonormalize();

	RID j = PhysicsServer::get_singleton()->joint_create_slider(body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < PARAM_MAX; i++) {
		PhysicsServer::get_singleton()->slider_joint_set_param(j, PhysicsServer::SliderJointParam(i), params[i]);
	}

	return j;
}

SliderJoint::SliderJoint() {
	params[PARAM_LINEAR_LIMIT_UPPER] = 1.0;
	params[PARAM_LINEAR_LIMIT_LOWER] = -1.0;
	params[PARAM_LINEAR_LIMIT_SOFTNESS] = 1.0;
	params[PARAM_LINEAR_LIMIT_RESTITUTION] = 0.7;
	params[PARAM_LINEAR_LIMIT_DAMPING] = 1.0;
	params[PARAM_LINEAR_MOTION_SOFTNESS] = 1.0;
	params[PARAM_LINEAR_MOTION_RESTITUTION] = 0.7;
	params[PARAM_LINEAR_MOTION_DAMPING] = 0; //1.0;
	params[PARAM_LINEAR_ORTHOGONAL_SOFTNESS] = 1.0;
	params[PARAM_LINEAR_ORTHOGONAL_RESTITUTION] = 0.7;
	params[PARAM_LINEAR_ORTHOGONAL_DAMPING] = 1.0;

	params[PARAM_ANGULAR_LIMIT_UPPER] = 0;
	params[PARAM_ANGULAR_LIMIT_LOWER] = 0;
	params[PARAM_ANGULAR_LIMIT_SOFTNESS] = 1.0;
	params[PARAM_ANGULAR_LIMIT_RESTITUTION] = 0.7;
	params[PARAM_ANGULAR_LIMIT_DAMPING] = 0; //1.0;
	params[PARAM_ANGULAR_MOTION_SOFTNESS] = 1.0;
	params[PARAM_ANGULAR_MOTION_RESTITUTION] = 0.7;
	params[PARAM_ANGULAR_MOTION_DAMPING] = 1.0;
	params[PARAM_ANGULAR_ORTHOGONAL_SOFTNESS] = 1.0;
	params[PARAM_ANGULAR_ORTHOGONAL_RESTITUTION] = 0.7;
	params[PARAM_ANGULAR_ORTHOGONAL_DAMPING] = 1.0;
}

//////////////////////////////////

void ConeTwistJoint::_set_swing_span(float p_limit_angular) {
	set_param(PARAM_SWING_SPAN, Math::deg2rad(p_limit_angular));
}

float ConeTwistJoint::_get_swing_span() const {
	return Math::rad2deg(get_param(PARAM_SWING_SPAN));
}

void ConeTwistJoint::_set_twist_span(float p_limit_angular) {
	set_param(PARAM_TWIST_SPAN, Math::deg2rad(p_limit_angular));
}

float ConeTwistJoint::_get_twist_span() const {
	return Math::rad2deg(get_param(PARAM_TWIST_SPAN));
}

void ConeTwistJoint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &ConeTwistJoint::set_param);
	ClassDB::bind_method(D_METHOD("get_param", "param"), &ConeTwistJoint::get_param);

	ClassDB::bind_method(D_METHOD("_set_swing_span", "swing_span"), &ConeTwistJoint::_set_swing_span);
	ClassDB::bind_method(D_METHOD("_get_swing_span"), &ConeTwistJoint::_get_swing_span);

	ClassDB::bind_method(D_METHOD("_set_twist_span", "twist_span"), &ConeTwistJoint::_set_twist_span);
	ClassDB::bind_method(D_METHOD("_get_twist_span"), &ConeTwistJoint::_get_twist_span);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "swing_span", PROPERTY_HINT_RANGE, "-180,180,0.1"), "_set_swing_span", "_get_swing_span");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "twist_span", PROPERTY_HINT_RANGE, "-40000,40000,0.1"), "_set_twist_span", "_get_twist_span");

	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "bias", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_BIAS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "softness", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "relaxation", PROPERTY_HINT_RANGE, "0.01,16.0,0.01"), "set_param", "get_param", PARAM_RELAXATION);

	BIND_ENUM_CONSTANT(PARAM_SWING_SPAN);
	BIND_ENUM_CONSTANT(PARAM_TWIST_SPAN);
	BIND_ENUM_CONSTANT(PARAM_BIAS);
	BIND_ENUM_CONSTANT(PARAM_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_RELAXATION);
	BIND_ENUM_CONSTANT(PARAM_MAX);
}

void ConeTwistJoint::set_param(Param p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params[p_param] = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->cone_twist_joint_set_param(get_joint(), PhysicsServer::ConeTwistJointParam(p_param), p_value);
	}

	update_gizmo();
}
float ConeTwistJoint::get_param(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params[p_param];
}

RID ConeTwistJoint::_configure_joint(PhysicsBody *body_a, PhysicsBody *body_b) {
	Transform gt = get_global_transform();
	//Vector3 cone_twistpos = gt.origin;
	//Vector3 cone_twistdir = gt.basis.get_axis(2);

	Transform ainv = body_a->get_global_transform().affine_inverse();

	Transform local_a = ainv * gt;
	local_a.orthonormalize();
	Transform local_b = gt;

	if (body_b) {
		Transform binv = body_b->get_global_transform().affine_inverse();
		local_b = binv * gt;
	}

	local_b.orthonormalize();

	RID j = PhysicsServer::get_singleton()->joint_create_cone_twist(body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < PARAM_MAX; i++) {
		PhysicsServer::get_singleton()->cone_twist_joint_set_param(j, PhysicsServer::ConeTwistJointParam(i), params[i]);
	}

	return j;
}

ConeTwistJoint::ConeTwistJoint() {
	params[PARAM_SWING_SPAN] = Math_PI * 0.25;
	params[PARAM_TWIST_SPAN] = Math_PI;
	params[PARAM_BIAS] = 0.3;
	params[PARAM_SOFTNESS] = 0.8;
	params[PARAM_RELAXATION] = 1.0;
}

/////////////////////////////////////////////////////////////////////

void Generic6DOFJoint::_set_angular_hi_limit_x(float p_limit_angular) {
	set_param_x(PARAM_ANGULAR_UPPER_LIMIT, Math::deg2rad(p_limit_angular));
}

float Generic6DOFJoint::_get_angular_hi_limit_x() const {
	return Math::rad2deg(get_param_x(PARAM_ANGULAR_UPPER_LIMIT));
}

void Generic6DOFJoint::_set_angular_lo_limit_x(float p_limit_angular) {
	set_param_x(PARAM_ANGULAR_LOWER_LIMIT, Math::deg2rad(p_limit_angular));
}

float Generic6DOFJoint::_get_angular_lo_limit_x() const {
	return Math::rad2deg(get_param_x(PARAM_ANGULAR_LOWER_LIMIT));
}

void Generic6DOFJoint::_set_angular_hi_limit_y(float p_limit_angular) {
	set_param_y(PARAM_ANGULAR_UPPER_LIMIT, Math::deg2rad(p_limit_angular));
}

float Generic6DOFJoint::_get_angular_hi_limit_y() const {
	return Math::rad2deg(get_param_y(PARAM_ANGULAR_UPPER_LIMIT));
}

void Generic6DOFJoint::_set_angular_lo_limit_y(float p_limit_angular) {
	set_param_y(PARAM_ANGULAR_LOWER_LIMIT, Math::deg2rad(p_limit_angular));
}

float Generic6DOFJoint::_get_angular_lo_limit_y() const {
	return Math::rad2deg(get_param_y(PARAM_ANGULAR_LOWER_LIMIT));
}

void Generic6DOFJoint::_set_angular_hi_limit_z(float p_limit_angular) {
	set_param_z(PARAM_ANGULAR_UPPER_LIMIT, Math::deg2rad(p_limit_angular));
}

float Generic6DOFJoint::_get_angular_hi_limit_z() const {
	return Math::rad2deg(get_param_z(PARAM_ANGULAR_UPPER_LIMIT));
}

void Generic6DOFJoint::_set_angular_lo_limit_z(float p_limit_angular) {
	set_param_z(PARAM_ANGULAR_LOWER_LIMIT, Math::deg2rad(p_limit_angular));
}

float Generic6DOFJoint::_get_angular_lo_limit_z() const {
	return Math::rad2deg(get_param_z(PARAM_ANGULAR_LOWER_LIMIT));
}

void Generic6DOFJoint::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "angular_spring/quaternion_rotation_equilibrium") {
		p_property.usage = PROPERTY_USAGE_DEFAULT;
		if (!using_quaternion_rotation_equilibrium) {
			p_property.usage = 0;
		}
	}
}

void Generic6DOFJoint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_set_angular_hi_limit_x", "angle"), &Generic6DOFJoint::_set_angular_hi_limit_x);
	ClassDB::bind_method(D_METHOD("_get_angular_hi_limit_x"), &Generic6DOFJoint::_get_angular_hi_limit_x);

	ClassDB::bind_method(D_METHOD("_set_angular_lo_limit_x", "angle"), &Generic6DOFJoint::_set_angular_lo_limit_x);
	ClassDB::bind_method(D_METHOD("_get_angular_lo_limit_x"), &Generic6DOFJoint::_get_angular_lo_limit_x);

	ClassDB::bind_method(D_METHOD("_set_angular_hi_limit_y", "angle"), &Generic6DOFJoint::_set_angular_hi_limit_y);
	ClassDB::bind_method(D_METHOD("_get_angular_hi_limit_y"), &Generic6DOFJoint::_get_angular_hi_limit_y);

	ClassDB::bind_method(D_METHOD("_set_angular_lo_limit_y", "angle"), &Generic6DOFJoint::_set_angular_lo_limit_y);
	ClassDB::bind_method(D_METHOD("_get_angular_lo_limit_y"), &Generic6DOFJoint::_get_angular_lo_limit_y);

	ClassDB::bind_method(D_METHOD("_set_angular_hi_limit_z", "angle"), &Generic6DOFJoint::_set_angular_hi_limit_z);
	ClassDB::bind_method(D_METHOD("_get_angular_hi_limit_z"), &Generic6DOFJoint::_get_angular_hi_limit_z);

	ClassDB::bind_method(D_METHOD("_set_angular_lo_limit_z", "angle"), &Generic6DOFJoint::_set_angular_lo_limit_z);
	ClassDB::bind_method(D_METHOD("_get_angular_lo_limit_z"), &Generic6DOFJoint::_get_angular_lo_limit_z);

	ClassDB::bind_method(D_METHOD("set_param_x", "param", "value"), &Generic6DOFJoint::set_param_x);
	ClassDB::bind_method(D_METHOD("get_param_x", "param"), &Generic6DOFJoint::get_param_x);

	ClassDB::bind_method(D_METHOD("set_param_y", "param", "value"), &Generic6DOFJoint::set_param_y);
	ClassDB::bind_method(D_METHOD("get_param_y", "param"), &Generic6DOFJoint::get_param_y);

	ClassDB::bind_method(D_METHOD("set_param_z", "param", "value"), &Generic6DOFJoint::set_param_z);
	ClassDB::bind_method(D_METHOD("get_param_z", "param"), &Generic6DOFJoint::get_param_z);

	ClassDB::bind_method(D_METHOD("set_flag_x", "flag", "value"), &Generic6DOFJoint::set_flag_x);
	ClassDB::bind_method(D_METHOD("get_flag_x", "flag"), &Generic6DOFJoint::get_flag_x);

	ClassDB::bind_method(D_METHOD("set_flag_y", "flag", "value"), &Generic6DOFJoint::set_flag_y);
	ClassDB::bind_method(D_METHOD("get_flag_y", "flag"), &Generic6DOFJoint::get_flag_y);

	ClassDB::bind_method(D_METHOD("set_flag_z", "flag", "value"), &Generic6DOFJoint::set_flag_z);
	ClassDB::bind_method(D_METHOD("get_flag_z", "flag"), &Generic6DOFJoint::get_flag_z);

	ClassDB::bind_method(D_METHOD("set_use_global_rotation", "enable"), &Generic6DOFJoint::set_use_global_rotation);
	ClassDB::bind_method(D_METHOD("get_use_global_rotation"), &Generic6DOFJoint::get_use_global_rotation);

	ClassDB::bind_method(D_METHOD("set_use_quaternion_rotation_equilibrium", "enable"), &Generic6DOFJoint::set_use_quaternion_rotation_equilibrium);
	ClassDB::bind_method(D_METHOD("get_use_quaternion_rotation_equilibrium"), &Generic6DOFJoint::get_use_quaternion_rotation_equilibrium);

	ClassDB::bind_method(D_METHOD("set_quaternion_rotation_equilibrium", "value"), &Generic6DOFJoint::set_quaternion_rotation_equilibrium);
	ClassDB::bind_method(D_METHOD("get_quaternion_rotation_equilibrium"), &Generic6DOFJoint::get_quaternion_rotation_equilibrium);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "angular_spring/use_global_rotation"), "set_use_global_rotation", "get_use_global_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "angular_spring/use_quaternion_rotation_equilibrium"), "set_use_quaternion_rotation_equilibrium", "get_use_quaternion_rotation_equilibrium");
	ADD_PROPERTY(PropertyInfo(Variant::QUAT, "angular_spring/quaternion_rotation_equilibrium"), "set_quaternion_rotation_equilibrium", "get_quaternion_rotation_equilibrium");
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_limit_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_LINEAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_x/upper_distance"), "set_param_x", "get_param_x", PARAM_LINEAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_x/lower_distance"), "set_param_x", "get_param_x", PARAM_LINEAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_x/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_LINEAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_x/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_LINEAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_x/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_LINEAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_motor_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_LINEAR_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motor_x/target_velocity"), "set_param_x", "get_param_x", PARAM_LINEAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motor_x/force_limit"), "set_param_x", "get_param_x", PARAM_LINEAR_MOTOR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_spring_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_LINEAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_x/stiffness"), "set_param_x", "get_param_x", PARAM_LINEAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_x/damping"), "set_param_x", "get_param_x", PARAM_LINEAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_x/equilibrium_point"), "set_param_x", "get_param_x", PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_limit_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_ANGULAR_LIMIT);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit_x/upper_angle", PROPERTY_HINT_RANGE, "-180,180,0.01"), "_set_angular_hi_limit_x", "_get_angular_hi_limit_x");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit_x/lower_angle", PROPERTY_HINT_RANGE, "-180,180,0.01"), "_set_angular_lo_limit_x", "_get_angular_lo_limit_x");
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_x/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_ANGULAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_x/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_ANGULAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_x/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_x", "get_param_x", PARAM_ANGULAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_x/force_limit"), "set_param_x", "get_param_x", PARAM_ANGULAR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_x/erp"), "set_param_x", "get_param_x", PARAM_ANGULAR_ERP);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_motor_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motor_x/target_velocity"), "set_param_x", "get_param_x", PARAM_ANGULAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motor_x/force_limit"), "set_param_x", "get_param_x", PARAM_ANGULAR_MOTOR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_spring_x/enabled"), "set_flag_x", "get_flag_x", FLAG_ENABLE_ANGULAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_x/stiffness"), "set_param_x", "get_param_x", PARAM_ANGULAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_x/damping"), "set_param_x", "get_param_x", PARAM_ANGULAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_x/equilibrium_point"), "set_param_x", "get_param_x", PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_limit_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_LINEAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_y/upper_distance"), "set_param_y", "get_param_y", PARAM_LINEAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_y/lower_distance"), "set_param_y", "get_param_y", PARAM_LINEAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_y/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_LINEAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_y/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_LINEAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_y/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_LINEAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_motor_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_LINEAR_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motor_y/target_velocity"), "set_param_y", "get_param_y", PARAM_LINEAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motor_y/force_limit"), "set_param_y", "get_param_y", PARAM_LINEAR_MOTOR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_spring_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_LINEAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_y/stiffness"), "set_param_y", "get_param_y", PARAM_LINEAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_y/damping"), "set_param_y", "get_param_y", PARAM_LINEAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_y/equilibrium_point"), "set_param_y", "get_param_y", PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_limit_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_ANGULAR_LIMIT);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit_y/upper_angle", PROPERTY_HINT_RANGE, "-180,180,0.01"), "_set_angular_hi_limit_y", "_get_angular_hi_limit_y");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit_y/lower_angle", PROPERTY_HINT_RANGE, "-180,180,0.01"), "_set_angular_lo_limit_y", "_get_angular_lo_limit_y");
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_y/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_ANGULAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_y/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_ANGULAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_y/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_y", "get_param_y", PARAM_ANGULAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_y/force_limit"), "set_param_y", "get_param_y", PARAM_ANGULAR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_y/erp"), "set_param_y", "get_param_y", PARAM_ANGULAR_ERP);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_motor_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motor_y/target_velocity"), "set_param_y", "get_param_y", PARAM_ANGULAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motor_y/force_limit"), "set_param_y", "get_param_y", PARAM_ANGULAR_MOTOR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_spring_y/enabled"), "set_flag_y", "get_flag_y", FLAG_ENABLE_ANGULAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_y/stiffness"), "set_param_y", "get_param_y", PARAM_ANGULAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_y/damping"), "set_param_y", "get_param_y", PARAM_ANGULAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_y/equilibrium_point"), "set_param_y", "get_param_y", PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT);

	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_limit_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_LINEAR_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_z/upper_distance"), "set_param_z", "get_param_z", PARAM_LINEAR_UPPER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_z/lower_distance"), "set_param_z", "get_param_z", PARAM_LINEAR_LOWER_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_z/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_LINEAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_z/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_LINEAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_limit_z/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_LINEAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_motor_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_LINEAR_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motor_z/target_velocity"), "set_param_z", "get_param_z", PARAM_LINEAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_motor_z/force_limit"), "set_param_z", "get_param_z", PARAM_LINEAR_MOTOR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "linear_spring_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_LINEAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_z/stiffness"), "set_param_z", "get_param_z", PARAM_LINEAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_z/damping"), "set_param_z", "get_param_z", PARAM_LINEAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "linear_spring_z/equilibrium_point"), "set_param_z", "get_param_z", PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_limit_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_ANGULAR_LIMIT);
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit_z/upper_angle", PROPERTY_HINT_RANGE, "-180,180,0.01"), "_set_angular_hi_limit_z", "_get_angular_hi_limit_z");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_limit_z/lower_angle", PROPERTY_HINT_RANGE, "-180,180,0.01"), "_set_angular_lo_limit_z", "_get_angular_lo_limit_z");
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_z/softness", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_ANGULAR_LIMIT_SOFTNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_z/restitution", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_ANGULAR_RESTITUTION);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_z/damping", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_param_z", "get_param_z", PARAM_ANGULAR_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_z/force_limit"), "set_param_z", "get_param_z", PARAM_ANGULAR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_limit_z/erp"), "set_param_z", "get_param_z", PARAM_ANGULAR_ERP);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_motor_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_MOTOR);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motor_z/target_velocity"), "set_param_z", "get_param_z", PARAM_ANGULAR_MOTOR_TARGET_VELOCITY);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_motor_z/force_limit"), "set_param_z", "get_param_z", PARAM_ANGULAR_MOTOR_FORCE_LIMIT);
	ADD_PROPERTYI(PropertyInfo(Variant::BOOL, "angular_spring_z/enabled"), "set_flag_z", "get_flag_z", FLAG_ENABLE_ANGULAR_SPRING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_z/stiffness"), "set_param_z", "get_param_z", PARAM_ANGULAR_SPRING_STIFFNESS);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_z/damping"), "set_param_z", "get_param_z", PARAM_ANGULAR_SPRING_DAMPING);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "angular_spring_z/equilibrium_point"), "set_param_z", "get_param_z", PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT);

	BIND_ENUM_CONSTANT(PARAM_LINEAR_LOWER_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_UPPER_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_MOTOR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_SPRING_STIFFNESS);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_SPRING_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LOWER_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_UPPER_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_LIMIT_SOFTNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_RESTITUTION);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_ERP);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTOR_TARGET_VELOCITY);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_MOTOR_FORCE_LIMIT);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_SPRING_STIFFNESS);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_SPRING_DAMPING);
	BIND_ENUM_CONSTANT(PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT);
	BIND_ENUM_CONSTANT(PARAM_MAX);

	BIND_ENUM_CONSTANT(FLAG_ENABLE_LINEAR_LIMIT);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_ANGULAR_LIMIT);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_LINEAR_SPRING);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_ANGULAR_SPRING);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_MOTOR);
	BIND_ENUM_CONSTANT(FLAG_ENABLE_LINEAR_MOTOR);
	BIND_ENUM_CONSTANT(FLAG_MAX);
}

void Generic6DOFJoint::set_param_x(Param p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params_x[p_param] = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_param(get_joint(), Vector3::AXIS_X, PhysicsServer::G6DOFJointAxisParam(p_param), p_value);
	}

	update_gizmo();
}
float Generic6DOFJoint::get_param_x(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params_x[p_param];
}

void Generic6DOFJoint::set_param_y(Param p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params_y[p_param] = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_param(get_joint(), Vector3::AXIS_Y, PhysicsServer::G6DOFJointAxisParam(p_param), p_value);
	}
	update_gizmo();
}
float Generic6DOFJoint::get_param_y(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params_y[p_param];
}

void Generic6DOFJoint::set_param_z(Param p_param, float p_value) {
	ERR_FAIL_INDEX(p_param, PARAM_MAX);
	params_z[p_param] = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_param(get_joint(), Vector3::AXIS_Z, PhysicsServer::G6DOFJointAxisParam(p_param), p_value);
	}
	update_gizmo();
}
float Generic6DOFJoint::get_param_z(Param p_param) const {
	ERR_FAIL_INDEX_V(p_param, PARAM_MAX, 0);
	return params_z[p_param];
}

void Generic6DOFJoint::set_flag_x(Flag p_flag, bool p_enabled) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags_x[p_flag] = p_enabled;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_flag(get_joint(), Vector3::AXIS_X, PhysicsServer::G6DOFJointAxisFlag(p_flag), p_enabled);
	}
	update_gizmo();
}
bool Generic6DOFJoint::get_flag_x(Flag p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags_x[p_flag];
}

void Generic6DOFJoint::set_flag_y(Flag p_flag, bool p_enabled) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags_y[p_flag] = p_enabled;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_flag(get_joint(), Vector3::AXIS_Y, PhysicsServer::G6DOFJointAxisFlag(p_flag), p_enabled);
	}
	update_gizmo();
}
bool Generic6DOFJoint::get_flag_y(Flag p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags_y[p_flag];
}

void Generic6DOFJoint::set_flag_z(Flag p_flag, bool p_enabled) {
	ERR_FAIL_INDEX(p_flag, FLAG_MAX);
	flags_z[p_flag] = p_enabled;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_flag(get_joint(), Vector3::AXIS_Z, PhysicsServer::G6DOFJointAxisFlag(p_flag), p_enabled);
	}
	update_gizmo();
}
bool Generic6DOFJoint::get_flag_z(Flag p_flag) const {
	ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
	return flags_z[p_flag];
}

void Generic6DOFJoint::set_use_global_rotation(bool p_enabled) {
	Generic6DOFJoint::using_global_rotation = p_enabled;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_use_global_rotation(get_joint(), p_enabled);
	}
	update_gizmo();
}

bool Generic6DOFJoint::get_use_global_rotation() const {
	if (get_joint().is_valid()) {
		return PhysicsServer::get_singleton()->generic_6dof_joint_get_use_global_rotation(get_joint());
	}
	return false;
}

void Generic6DOFJoint::set_use_quaternion_rotation_equilibrium(bool p_enabled) {
	Generic6DOFJoint::using_quaternion_rotation_equilibrium = p_enabled;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_use_quaternion_rotation_equilibrium(get_joint(), p_enabled);
	}
	update_gizmo();
	property_list_changed_notify();
}
bool Generic6DOFJoint::get_use_quaternion_rotation_equilibrium() const {
	if (get_joint().is_valid()) {
		return PhysicsServer::get_singleton()->generic_6dof_joint_get_use_quaternion_rotation_equilibrium(get_joint());
	}
	return false;
}

void Generic6DOFJoint::set_quaternion_rotation_equilibrium(Quat p_value) {
	Generic6DOFJoint::quaternion_rotation_equilibrium = p_value;
	if (get_joint().is_valid()) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_quaternion_rotation_equilibrium(get_joint(), p_value);
	}
	update_gizmo();
}
Quat Generic6DOFJoint::get_quaternion_rotation_equilibrium() const {
	return PhysicsServer::get_singleton()->generic_6dof_joint_get_quaternion_rotation_equilibrium(get_joint());
}

RID Generic6DOFJoint::_configure_joint(PhysicsBody *body_a, PhysicsBody *body_b) {
	Transform gt = get_global_transform();
	//Vector3 cone_twistpos = gt.origin;
	//Vector3 cone_twistdir = gt.basis.get_axis(2);

	Transform ainv = body_a->get_global_transform().affine_inverse();

	Transform local_a = ainv * gt;
	local_a.orthonormalize();
	Transform local_b = gt;

	if (body_b) {
		Transform binv = body_b->get_global_transform().affine_inverse();
		local_b = binv * gt;
	}

	local_b.orthonormalize();

	RID j = PhysicsServer::get_singleton()->joint_create_generic_6dof(body_a->get_rid(), local_a, body_b ? body_b->get_rid() : RID(), local_b);
	for (int i = 0; i < PARAM_MAX; i++) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_param(j, Vector3::AXIS_X, PhysicsServer::G6DOFJointAxisParam(i), params_x[i]);
		PhysicsServer::get_singleton()->generic_6dof_joint_set_param(j, Vector3::AXIS_Y, PhysicsServer::G6DOFJointAxisParam(i), params_y[i]);
		PhysicsServer::get_singleton()->generic_6dof_joint_set_param(j, Vector3::AXIS_Z, PhysicsServer::G6DOFJointAxisParam(i), params_z[i]);
	}
	for (int i = 0; i < FLAG_MAX; i++) {
		PhysicsServer::get_singleton()->generic_6dof_joint_set_flag(j, Vector3::AXIS_X, PhysicsServer::G6DOFJointAxisFlag(i), flags_x[i]);
		PhysicsServer::get_singleton()->generic_6dof_joint_set_flag(j, Vector3::AXIS_Y, PhysicsServer::G6DOFJointAxisFlag(i), flags_y[i]);
		PhysicsServer::get_singleton()->generic_6dof_joint_set_flag(j, Vector3::AXIS_Z, PhysicsServer::G6DOFJointAxisFlag(i), flags_z[i]);
	}
	PhysicsServer::get_singleton()->generic_6dof_joint_set_use_global_rotation(j, Generic6DOFJoint::using_global_rotation);
	PhysicsServer::get_singleton()->generic_6dof_joint_set_use_quaternion_rotation_equilibrium(j, Generic6DOFJoint::using_quaternion_rotation_equilibrium);
	PhysicsServer::get_singleton()->generic_6dof_joint_set_quaternion_rotation_equilibrium(j, Generic6DOFJoint::quaternion_rotation_equilibrium);
	return j;
}

Generic6DOFJoint::Generic6DOFJoint() {
	set_param_x(PARAM_LINEAR_LOWER_LIMIT, 0);
	set_param_x(PARAM_LINEAR_UPPER_LIMIT, 0);
	set_param_x(PARAM_LINEAR_LIMIT_SOFTNESS, 0.7);
	set_param_x(PARAM_LINEAR_RESTITUTION, 0.5);
	set_param_x(PARAM_LINEAR_DAMPING, 1.0);
	set_param_x(PARAM_LINEAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_x(PARAM_LINEAR_MOTOR_FORCE_LIMIT, 0);
	set_param_x(PARAM_LINEAR_SPRING_STIFFNESS, 0.01);
	set_param_x(PARAM_LINEAR_SPRING_DAMPING, 0.01);
	set_param_x(PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT, 0.0);
	set_param_x(PARAM_ANGULAR_LOWER_LIMIT, 0);
	set_param_x(PARAM_ANGULAR_UPPER_LIMIT, 0);
	set_param_x(PARAM_ANGULAR_LIMIT_SOFTNESS, 0.5f);
	set_param_x(PARAM_ANGULAR_DAMPING, 1.0f);
	set_param_x(PARAM_ANGULAR_RESTITUTION, 0);
	set_param_x(PARAM_ANGULAR_FORCE_LIMIT, 0);
	set_param_x(PARAM_ANGULAR_ERP, 0.5);
	set_param_x(PARAM_ANGULAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_x(PARAM_ANGULAR_MOTOR_FORCE_LIMIT, 300);
	set_param_x(PARAM_ANGULAR_SPRING_STIFFNESS, 0);
	set_param_x(PARAM_ANGULAR_SPRING_DAMPING, 0);
	set_param_x(PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT, 0);

	set_flag_x(FLAG_ENABLE_ANGULAR_LIMIT, true);
	set_flag_x(FLAG_ENABLE_LINEAR_LIMIT, true);
	set_flag_x(FLAG_ENABLE_ANGULAR_SPRING, false);
	set_flag_x(FLAG_ENABLE_LINEAR_SPRING, false);
	set_flag_x(FLAG_ENABLE_MOTOR, false);
	set_flag_x(FLAG_ENABLE_LINEAR_MOTOR, false);

	set_param_y(PARAM_LINEAR_LOWER_LIMIT, 0);
	set_param_y(PARAM_LINEAR_UPPER_LIMIT, 0);
	set_param_y(PARAM_LINEAR_LIMIT_SOFTNESS, 0.7);
	set_param_y(PARAM_LINEAR_RESTITUTION, 0.5);
	set_param_y(PARAM_LINEAR_DAMPING, 1.0);
	set_param_y(PARAM_LINEAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_y(PARAM_LINEAR_MOTOR_FORCE_LIMIT, 0);
	set_param_y(PARAM_LINEAR_SPRING_STIFFNESS, 0.01);
	set_param_y(PARAM_LINEAR_SPRING_DAMPING, 0.01);
	set_param_y(PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT, 0.0);
	set_param_y(PARAM_ANGULAR_LOWER_LIMIT, 0);
	set_param_y(PARAM_ANGULAR_UPPER_LIMIT, 0);
	set_param_y(PARAM_ANGULAR_LIMIT_SOFTNESS, 0.5f);
	set_param_y(PARAM_ANGULAR_DAMPING, 1.0f);
	set_param_y(PARAM_ANGULAR_RESTITUTION, 0);
	set_param_y(PARAM_ANGULAR_FORCE_LIMIT, 0);
	set_param_y(PARAM_ANGULAR_ERP, 0.5);
	set_param_y(PARAM_ANGULAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_y(PARAM_ANGULAR_MOTOR_FORCE_LIMIT, 300);
	set_param_y(PARAM_ANGULAR_SPRING_STIFFNESS, 0);
	set_param_y(PARAM_ANGULAR_SPRING_DAMPING, 0);
	set_param_y(PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT, 0);

	set_flag_y(FLAG_ENABLE_ANGULAR_LIMIT, true);
	set_flag_y(FLAG_ENABLE_LINEAR_LIMIT, true);
	set_flag_y(FLAG_ENABLE_ANGULAR_SPRING, false);
	set_flag_y(FLAG_ENABLE_LINEAR_SPRING, false);
	set_flag_y(FLAG_ENABLE_MOTOR, false);
	set_flag_y(FLAG_ENABLE_LINEAR_MOTOR, false);

	set_param_z(PARAM_LINEAR_LOWER_LIMIT, 0);
	set_param_z(PARAM_LINEAR_UPPER_LIMIT, 0);
	set_param_z(PARAM_LINEAR_LIMIT_SOFTNESS, 0.7);
	set_param_z(PARAM_LINEAR_RESTITUTION, 0.5);
	set_param_z(PARAM_LINEAR_DAMPING, 1.0);
	set_param_z(PARAM_LINEAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_z(PARAM_LINEAR_MOTOR_FORCE_LIMIT, 0);
	set_param_z(PARAM_LINEAR_SPRING_STIFFNESS, 0.01);
	set_param_z(PARAM_LINEAR_SPRING_DAMPING, 0.01);
	set_param_z(PARAM_LINEAR_SPRING_EQUILIBRIUM_POINT, 0.0);
	set_param_z(PARAM_ANGULAR_LOWER_LIMIT, 0);
	set_param_z(PARAM_ANGULAR_UPPER_LIMIT, 0);
	set_param_z(PARAM_ANGULAR_LIMIT_SOFTNESS, 0.5f);
	set_param_z(PARAM_ANGULAR_DAMPING, 1.0f);
	set_param_z(PARAM_ANGULAR_RESTITUTION, 0);
	set_param_z(PARAM_ANGULAR_FORCE_LIMIT, 0);
	set_param_z(PARAM_ANGULAR_ERP, 0.5);
	set_param_z(PARAM_ANGULAR_MOTOR_TARGET_VELOCITY, 0);
	set_param_z(PARAM_ANGULAR_MOTOR_FORCE_LIMIT, 300);
	set_param_z(PARAM_ANGULAR_SPRING_STIFFNESS, 0);
	set_param_z(PARAM_ANGULAR_SPRING_DAMPING, 0);
	set_param_z(PARAM_ANGULAR_SPRING_EQUILIBRIUM_POINT, 0);

	set_flag_z(FLAG_ENABLE_ANGULAR_LIMIT, true);
	set_flag_z(FLAG_ENABLE_LINEAR_LIMIT, true);
	set_flag_z(FLAG_ENABLE_ANGULAR_SPRING, false);
	set_flag_z(FLAG_ENABLE_LINEAR_SPRING, false);
	set_flag_z(FLAG_ENABLE_MOTOR, false);
	set_flag_z(FLAG_ENABLE_LINEAR_MOTOR, false);

	set_use_global_rotation(false);
	set_use_quaternion_rotation_equilibrium(false);
	set_quaternion_rotation_equilibrium(Quat());
}
