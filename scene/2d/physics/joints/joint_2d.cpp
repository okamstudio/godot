/**************************************************************************/
/*  joint_2d.cpp                                                          */
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

#include "joint_2d.h"

#include "scene/2d/physics/physics_body_2d.h"

void Joint2D::_disconnect_signals() {
	Node *node_a = get_node_or_null(a);
	PhysicsBody2D *body_a = Object::cast_to<PhysicsBody2D>(node_a);
	if (body_a) {
		body_a->disconnect(SceneStringName(tree_exiting), callable_mp(this, &Joint2D::_body_exit_tree));
	}

	Node *node_b = get_node_or_null(b);
	PhysicsBody2D *body_b = Object::cast_to<PhysicsBody2D>(node_b);
	if (body_b) {
		body_b->disconnect(SceneStringName(tree_exiting), callable_mp(this, &Joint2D::_body_exit_tree));
	}
}

void Joint2D::_body_exit_tree() {
	_disconnect_signals();
	_update_joint(true);
	update_configuration_warnings();
}

void Joint2D::_update_joint(bool p_only_free) {
	if (ba.is_valid() && bb.is_valid() && exclude_from_collision) {
		PhysicsServer2D::get_singleton()->joint_disable_collisions_between_bodies(joint, false);
	}

	ba = RID();
	bb = RID();
	configured = false;

	if (p_only_free || !is_inside_tree()) {
		PhysicsServer2D::get_singleton()->joint_clear(joint);
		warning = String();
		return;
	}

	Node *node_a = get_node_or_null(a);
	Node *node_b = get_node_or_null(b);

	PhysicsBody2D *body_a = Object::cast_to<PhysicsBody2D>(node_a);
	PhysicsBody2D *body_b = Object::cast_to<PhysicsBody2D>(node_b);

	bool valid = false;

	if (node_a && !body_a && node_b && !body_b) {
		warning = RTR("Node A and Node B must be PhysicsBody2Ds");
	} else if (node_a && !body_a) {
		warning = RTR("Node A must be a PhysicsBody2D");
	} else if (node_b && !body_b) {
		warning = RTR("Node B must be a PhysicsBody2D");
	} else if (!body_a || !body_b) {
		warning = RTR("Joint is not connected to two PhysicsBody2Ds");
	} else if (body_a == body_b) {
		warning = RTR("Node A and Node B must be different PhysicsBody2Ds");
	} else {
		warning = String();
		valid = true;
	}

	update_configuration_warnings();

	if (!valid) {
		PhysicsServer2D::get_singleton()->joint_clear(joint);
		return;
	}

	if (body_a) {
		body_a->force_update_transform();
	}

	if (body_b) {
		body_b->force_update_transform();
	}

	configured = true;

	_configure_joint(joint, body_a, body_b);

	ERR_FAIL_COND_MSG(!joint.is_valid(), "Failed to configure the joint.");

	PhysicsServer2D::get_singleton()->joint_set_param(joint, PhysicsServer2D::JOINT_PARAM_BIAS, bias);

	ba = body_a->get_rid();
	bb = body_b->get_rid();

	if (!body_a->is_connected(SceneStringName(tree_exiting), callable_mp(this, &Joint2D::_body_exit_tree))) {
		body_a->connect(SceneStringName(tree_exiting), callable_mp(this, &Joint2D::_body_exit_tree));
	}
	if (!body_b->is_connected(SceneStringName(tree_exiting), callable_mp(this, &Joint2D::_body_exit_tree))) {
		body_b->connect(SceneStringName(tree_exiting), callable_mp(this, &Joint2D::_body_exit_tree));
	}

	PhysicsServer2D::get_singleton()->joint_disable_collisions_between_bodies(joint, exclude_from_collision);
}

void Joint2D::set_node_a(const NodePath &p_node_a) {
	if (a == p_node_a) {
		return;
	}

	if (is_configured()) {
		_disconnect_signals();
	}

	a = p_node_a;
	if (Engine::get_singleton()->is_editor_hint()) {
		// When in editor, the setter may be called as a result of node rename.
		// It happens before the node actually changes its name, which triggers false warning.
		callable_mp(this, &Joint2D::_update_joint).call_deferred(false);
	} else {
		_update_joint();
	}
}

NodePath Joint2D::get_node_a() const {
	return a;
}

void Joint2D::set_node_b(const NodePath &p_node_b) {
	if (b == p_node_b) {
		return;
	}

	if (is_configured()) {
		_disconnect_signals();
	}

	b = p_node_b;
	if (Engine::get_singleton()->is_editor_hint()) {
		callable_mp(this, &Joint2D::_update_joint).call_deferred(false);
	} else {
		_update_joint();
	}
}

NodePath Joint2D::get_node_b() const {
	return b;
}

void Joint2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POST_ENTER_TREE: {
			if (is_configured()) {
				_disconnect_signals();
			}
			_update_joint();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (is_configured()) {
				_disconnect_signals();
			}
			_update_joint(true);
		} break;
	}
}

void Joint2D::set_bias(real_t p_bias) {
	bias = p_bias;
	if (joint.is_valid()) {
		PhysicsServer2D::get_singleton()->joint_set_param(joint, PhysicsServer2D::JOINT_PARAM_BIAS, bias);
	}
}

real_t Joint2D::get_bias() const {
	return bias;
}

void Joint2D::set_break_force(real_t p_break_force) {
	if (break_force == p_break_force) {
		return;
	}
	break_force = p_break_force;
	if (joint.is_valid()) {
		PhysicsServer2D::get_singleton()->joint_set_param(joint, PhysicsServer2D::JOINT_PARAM_BREAK_FORCE, break_force);
	}
}

real_t Joint2D::get_break_force() const {
	return break_force;
}

void Joint2D::set_break_torque(real_t p_break_torque) {
	if (break_torque == p_break_torque) {
		return;
	}
	break_torque = p_break_torque;
	if (joint.is_valid()) {
		PhysicsServer2D::get_singleton()->joint_set_param(joint, PhysicsServer2D::JOINT_PARAM_BREAK_TORQUE, break_torque);
	}
}

real_t Joint2D::get_break_torque() const {
	return break_torque;
}

Vector2 Joint2D::get_reaction_force() const {
	if (joint.is_valid()) {
		return PhysicsServer2D::get_singleton()->joint_get_state(joint, PhysicsServer2D::JOINT_STATE_REACTION_FORCE);
	}
	return Vector2();
}

void Joint2D::set_break_enabled(bool p_break_enabled) {
	if (break_enabled == p_break_enabled) {
		return;
	}
	break_enabled = p_break_enabled;
	if (joint.is_valid()) {
		PhysicsServer2D::get_singleton()->joint_set_flag(joint, PhysicsServer2D::JOINT_FLAG_BREAK_ENABLED, break_enabled);
	}
}

bool Joint2D::is_break_enabled() const {
	return break_enabled;
}

real_t Joint2D::get_reaction_torque() const {
	if (joint.is_valid()) {
		return PhysicsServer2D::get_singleton()->joint_get_state(joint, PhysicsServer2D::JOINT_STATE_REACTION_TORQUE);
	}
	return 0.0;
}

void Joint2D::set_exclude_nodes_from_collision(bool p_enable) {
	if (exclude_from_collision == p_enable) {
		return;
	}
	if (is_configured()) {
		_disconnect_signals();
	}
	_update_joint(true);
	exclude_from_collision = p_enable;
	_update_joint();
}

bool Joint2D::get_exclude_nodes_from_collision() const {
	return exclude_from_collision;
}

PackedStringArray Joint2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();

	if (!warning.is_empty()) {
		warnings.push_back(warning);
	}

	return warnings;
}

void Joint2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_node_a", "node"), &Joint2D::set_node_a);
	ClassDB::bind_method(D_METHOD("get_node_a"), &Joint2D::get_node_a);

	ClassDB::bind_method(D_METHOD("set_node_b", "node"), &Joint2D::set_node_b);
	ClassDB::bind_method(D_METHOD("get_node_b"), &Joint2D::get_node_b);

	ClassDB::bind_method(D_METHOD("set_bias", "bias"), &Joint2D::set_bias);
	ClassDB::bind_method(D_METHOD("get_bias"), &Joint2D::get_bias);

	ClassDB::bind_method(D_METHOD("set_break_force", "break_force"), &Joint2D::set_break_force);
	ClassDB::bind_method(D_METHOD("get_break_force"), &Joint2D::get_break_force);

	ClassDB::bind_method(D_METHOD("set_break_enabled", "break_enabled"), &Joint2D::set_break_enabled);
	ClassDB::bind_method(D_METHOD("is_break_enabled"), &Joint2D::is_break_enabled);

	ClassDB::bind_method(D_METHOD("set_break_torque", "break_torque"), &Joint2D::set_break_torque);
	ClassDB::bind_method(D_METHOD("get_break_torque"), &Joint2D::get_break_torque);

	ClassDB::bind_method(D_METHOD("get_reaction_force"), &Joint2D::get_reaction_force);
	ClassDB::bind_method(D_METHOD("get_reaction_torque"), &Joint2D::get_reaction_torque);

	ClassDB::bind_method(D_METHOD("set_exclude_nodes_from_collision", "enable"), &Joint2D::set_exclude_nodes_from_collision);
	ClassDB::bind_method(D_METHOD("get_exclude_nodes_from_collision"), &Joint2D::get_exclude_nodes_from_collision);

	ClassDB::bind_method(D_METHOD("get_rid"), &Joint2D::get_rid);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "node_a", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "PhysicsBody2D"), "set_node_a", "get_node_a");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "node_b", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "PhysicsBody2D"), "set_node_b", "get_node_b");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bias", PROPERTY_HINT_RANGE, "0,0.9,0.001"), "set_bias", "get_bias");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "break_enabled"), "set_break_enabled", "is_break_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "break_force", PROPERTY_HINT_RANGE, "0,200,0.1"), "set_break_force", "get_break_force");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "break_torque", PROPERTY_HINT_RANGE, "0,200,0.1"), "set_break_torque", "get_break_torque");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_collision"), "set_exclude_nodes_from_collision", "get_exclude_nodes_from_collision");
}

Joint2D::Joint2D() {
	joint = PhysicsServer2D::get_singleton()->joint_create();
	set_hide_clip_children(true);
}

Joint2D::~Joint2D() {
	ERR_FAIL_NULL(PhysicsServer2D::get_singleton());
	PhysicsServer2D::get_singleton()->free(joint);
}
