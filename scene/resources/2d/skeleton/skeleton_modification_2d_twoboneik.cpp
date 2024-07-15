/**************************************************************************/
/*  skeleton_modification_2d_twoboneik.cpp                                */
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

#include "skeleton_modification_2d_twoboneik.h"
#include "scene/2d/skeleton_2d.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif // TOOLS_ENABLED

bool SkeletonModification2DTwoBoneIK::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;

	if (path == "joint_one_bone_idx") {
		set_joint_one_bone_idx(p_value);
	} else if (path == "joint_one_bone2d_node") {
		set_joint_one_bone2d_node(p_value);
	} else if (path == "joint_two_bone_idx") {
		set_joint_two_bone_idx(p_value);
	} else if (path == "joint_two_bone2d_node") {
		set_joint_two_bone2d_node(p_value);
	}
#ifdef TOOLS_ENABLED
	else if (path.begins_with("editor/draw_gizmo")) {
		set_editor_draw_gizmo(p_value);
	} else if (path.begins_with("editor/draw_min_max")) {
		set_editor_draw_min_max(p_value);
	}
#endif // TOOLS_ENABLED
	else {
		return false;
	}

	return true;
}

bool SkeletonModification2DTwoBoneIK::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;

	if (path == "joint_one_bone_idx") {
		r_ret = get_joint_one_bone_idx();
	} else if (path == "joint_one_bone2d_node") {
		r_ret = get_joint_one_bone2d_node();
	} else if (path == "joint_two_bone_idx") {
		r_ret = get_joint_two_bone_idx();
	} else if (path == "joint_two_bone2d_node") {
		r_ret = get_joint_two_bone2d_node();
	}
#ifdef TOOLS_ENABLED
	else if (path.begins_with("editor/draw_gizmo")) {
		r_ret = get_editor_draw_gizmo();
	} else if (path.begins_with("editor/draw_min_max")) {
		r_ret = get_editor_draw_min_max();
	}
#endif // TOOLS_ENABLED
	else {
		return false;
	}

	return true;
}

void SkeletonModification2DTwoBoneIK::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::INT, "joint_one_bone_idx", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
	p_list->push_back(PropertyInfo(Variant::NODE_PATH, "joint_one_bone2d_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Bone2D", PROPERTY_USAGE_DEFAULT));

	p_list->push_back(PropertyInfo(Variant::INT, "joint_two_bone_idx", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
	p_list->push_back(PropertyInfo(Variant::NODE_PATH, "joint_two_bone2d_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Bone2D", PROPERTY_USAGE_DEFAULT));

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		p_list->push_back(PropertyInfo(Variant::BOOL, "editor/draw_gizmo", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		p_list->push_back(PropertyInfo(Variant::BOOL, "editor/draw_min_max", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
	}
#endif // TOOLS_ENABLED
}

void SkeletonModification2DTwoBoneIK::_execute(float p_delta) {
	ERR_FAIL_COND_MSG(!stack || !is_setup || stack->skeleton == nullptr,
			"Modification is not setup and therefore cannot execute!");
	if (!enabled) {
		return;
	}

	if (target_node_cache.is_null()) {
		WARN_PRINT_ONCE("Target cache is out of date. Attempting to update...");
		update_target_cache();
		return;
	}

	if (joint_one_bone2d_node_cache.is_null() && !joint_one_bone2d_node.is_empty()) {
		WARN_PRINT_ONCE("Joint one Bone2D node cache is out of date. Attempting to update...");
		update_joint_one_bone2d_cache();
	}
	if (joint_two_bone2d_node_cache.is_null() && !joint_two_bone2d_node.is_empty()) {
		WARN_PRINT_ONCE("Joint two Bone2D node cache is out of date. Attempting to update...");
		update_joint_two_bone2d_cache();
	}

	Node2D *target = Object::cast_to<Node2D>(ObjectDB::get_instance(target_node_cache));
	if (!target || !target->is_inside_tree()) {
		ERR_PRINT_ONCE("Target node is not in the scene tree. Cannot execute modification!");
		return;
	}

	Bone2D *joint_one_bone = stack->skeleton->get_bone(joint_one_bone_idx);
	if (joint_one_bone == nullptr) {
		ERR_PRINT_ONCE("Joint one bone_idx does not point to a valid bone! Cannot execute modification!");
		return;
	}

	Bone2D *joint_two_bone = stack->skeleton->get_bone(joint_two_bone_idx);
	if (joint_two_bone == nullptr) {
		ERR_PRINT_ONCE("Joint two bone_idx does not point to a valid bone! Cannot execute modification!");
		return;
	}

	// Adopted from the links below:
	// http://theorangeduck.com/page/simple-two-joint
	// https://www.alanzucconi.com/2018/05/02/ik-2d-2/
	// With modifications by TwistedTwigleg
	Vector2 target_difference = target->get_global_position() - joint_one_bone->get_global_position();
	float joint_one_to_target = target_difference.length();
	float angle_atan = target_difference.angle();

	float bone_one_length = joint_one_bone->get_length() * MIN(joint_one_bone->get_global_scale().x, joint_one_bone->get_global_scale().y);
	float bone_two_length = joint_two_bone->get_length() * MIN(joint_two_bone->get_global_scale().x, joint_two_bone->get_global_scale().y);
	bool override_angles_due_to_out_of_range = false;

	if (joint_one_to_target < target_minimum_distance) {
		joint_one_to_target = target_minimum_distance;
	}
	if (joint_one_to_target > target_maximum_distance && target_maximum_distance > 0.0) {
		joint_one_to_target = target_maximum_distance;
	}

	if (bone_one_length + bone_two_length < joint_one_to_target) {
		override_angles_due_to_out_of_range = true;
	}

	if (!override_angles_due_to_out_of_range) {
		float angle_0 = Math::acos(((joint_one_to_target * joint_one_to_target) + (bone_one_length * bone_one_length) - (bone_two_length * bone_two_length)) / (2.0 * joint_one_to_target * bone_one_length));
		float angle_1 = Math::acos(((bone_two_length * bone_two_length) + (bone_one_length * bone_one_length) - (joint_one_to_target * joint_one_to_target)) / (2.0 * bone_two_length * bone_one_length));

		if (flip_bend_direction) {
			angle_0 = -angle_0;
			angle_1 = -angle_1;
		}

		if (Math::is_nan(angle_0) || Math::is_nan(angle_1)) {
			// We cannot solve for this angle! Do nothing to avoid setting the rotation (and scale) to NaN.
		} else {
			joint_one_bone->set_global_rotation(angle_atan - angle_0 - joint_one_bone->get_bone_angle());
			joint_two_bone->set_rotation(-Math_PI - angle_1 - joint_two_bone->get_bone_angle() + joint_one_bone->get_bone_angle());
		}
	} else {
		joint_one_bone->set_global_rotation(angle_atan - joint_one_bone->get_bone_angle());
		joint_two_bone->set_global_rotation(angle_atan - joint_two_bone->get_bone_angle());
	}

	stack->skeleton->set_bone_local_pose_override(joint_one_bone_idx, joint_one_bone->get_transform(), stack->strength, true);
	stack->skeleton->set_bone_local_pose_override(joint_two_bone_idx, joint_two_bone->get_transform(), stack->strength, true);
}

void SkeletonModification2DTwoBoneIK::_setup_modification(SkeletonModificationStack2D *p_stack) {
	stack = p_stack;

	if (stack) {
		is_setup = true;
		update_target_cache();
		update_joint_one_bone2d_cache();
		update_joint_two_bone2d_cache();
	}
}

void SkeletonModification2DTwoBoneIK::_draw_editor_gizmo() {
	if (!enabled || !is_setup) {
		return;
	}

	Bone2D *operation_bone_one = stack->skeleton->get_bone(joint_one_bone_idx);
	if (!operation_bone_one) {
		return;
	}
	stack->skeleton->draw_set_transform(
			stack->skeleton->to_local(operation_bone_one->get_global_position()),
			operation_bone_one->get_global_rotation() - stack->skeleton->get_global_rotation());

	Color bone_ik_color = Color(1.0, 0.65, 0.0, 0.4);
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		bone_ik_color = EDITOR_GET("editors/2d/bone_ik_color");
	}
#endif // TOOLS_ENABLED

	if (flip_bend_direction) {
		float angle = -(Math_PI * 0.5) + operation_bone_one->get_bone_angle();
		stack->skeleton->draw_line(Vector2(0, 0), Vector2(Math::cos(angle), Math::sin(angle)) * (operation_bone_one->get_length() * 0.5), bone_ik_color, 2.0);
	} else {
		float angle = (Math_PI * 0.5) + operation_bone_one->get_bone_angle();
		stack->skeleton->draw_line(Vector2(0, 0), Vector2(Math::cos(angle), Math::sin(angle)) * (operation_bone_one->get_length() * 0.5), bone_ik_color, 2.0);
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		if (editor_draw_min_max) {
			if (target_maximum_distance != 0.0 || target_minimum_distance != 0.0) {
				Vector2 target_direction = Vector2(0, 1);
				if (target_node_cache.is_valid()) {
					stack->skeleton->draw_set_transform(Vector2(0, 0), 0.0);
					Node2D *target = Object::cast_to<Node2D>(ObjectDB::get_instance(target_node_cache));
					target_direction = operation_bone_one->get_global_position().direction_to(target->get_global_position());
				}

				stack->skeleton->draw_circle(target_direction * target_minimum_distance, 8, bone_ik_color);
				stack->skeleton->draw_circle(target_direction * target_maximum_distance, 8, bone_ik_color);
				stack->skeleton->draw_line(target_direction * target_minimum_distance, target_direction * target_maximum_distance, bone_ik_color, 2.0);
			}
		}
	}
#endif // TOOLS_ENABLED
}

void SkeletonModification2DTwoBoneIK::update_target_cache() {
	if (!is_setup || !stack) {
		if (is_setup) {
			ERR_PRINT_ONCE("Cannot update target cache: modification is not properly setup!");
		}
		return;
	}

	target_node_cache = ObjectID();
	if (stack->skeleton) {
		if (stack->skeleton->is_inside_tree()) {
			if (stack->skeleton->has_node(target_node)) {
				Node *node = stack->skeleton->get_node(target_node);
				ERR_FAIL_COND_MSG(!node || stack->skeleton == node,
						"Cannot update target cache: node is this modification's skeleton or cannot be found!");
				ERR_FAIL_COND_MSG(!node->is_inside_tree(),
						"Cannot update target cache: node is not in the scene tree!");
				target_node_cache = node->get_instance_id();
			}
		}
	}
}

void SkeletonModification2DTwoBoneIK::update_joint_one_bone2d_cache() {
	if (!is_setup || !stack) {
		if (is_setup) {
			ERR_PRINT_ONCE("Cannot update joint one Bone2D cache: modification is not properly setup!");
		}
		return;
	}

	joint_one_bone2d_node_cache = ObjectID();
	if (stack->skeleton) {
		if (stack->skeleton->is_inside_tree()) {
			if (stack->skeleton->has_node(joint_one_bone2d_node)) {
				Node *node = stack->skeleton->get_node(joint_one_bone2d_node);
				ERR_FAIL_COND_MSG(!node || stack->skeleton == node,
						"Cannot update joint one Bone2D cache: node is this modification's skeleton or cannot be found!");
				ERR_FAIL_COND_MSG(!node->is_inside_tree(),
						"Cannot update joint one Bone2D cache: node is not in the scene tree!");
				joint_one_bone2d_node_cache = node->get_instance_id();

				Bone2D *bone = Object::cast_to<Bone2D>(node);
				if (bone) {
					joint_one_bone_idx = bone->get_index_in_skeleton();
				} else {
					ERR_FAIL_MSG("Update joint one Bone2D cache: Nodepath to Bone2D is not a Bone2D node!");
				}
			}
		}
	}
}

void SkeletonModification2DTwoBoneIK::update_joint_two_bone2d_cache() {
	if (!is_setup || !stack) {
		if (is_setup) {
			ERR_PRINT_ONCE("Cannot update joint two Bone2D cache: modification is not properly setup!");
		}
		return;
	}

	joint_two_bone2d_node_cache = ObjectID();
	if (stack->skeleton) {
		if (stack->skeleton->is_inside_tree()) {
			if (stack->skeleton->has_node(joint_two_bone2d_node)) {
				Node *node = stack->skeleton->get_node(joint_two_bone2d_node);
				ERR_FAIL_COND_MSG(!node || stack->skeleton == node,
						"Cannot update joint two Bone2D cache: node is this modification's skeleton or cannot be found!");
				ERR_FAIL_COND_MSG(!node->is_inside_tree(),
						"Cannot update joint two Bone2D cache: node is not in scene tree!");
				joint_two_bone2d_node_cache = node->get_instance_id();

				Bone2D *bone = Object::cast_to<Bone2D>(node);
				if (bone) {
					joint_two_bone_idx = bone->get_index_in_skeleton();
				} else {
					ERR_FAIL_MSG("Update joint two Bone2D cache: Nodepath to Bone2D is not a Bone2D node!");
				}
			}
		}
	}
}

void SkeletonModification2DTwoBoneIK::set_target_node(const NodePath &p_target_node) {
	target_node = p_target_node;
	update_target_cache();
}

NodePath SkeletonModification2DTwoBoneIK::get_target_node() const {
	return target_node;
}

void SkeletonModification2DTwoBoneIK::set_joint_one_bone2d_node(const NodePath &p_target_node) {
	joint_one_bone2d_node = p_target_node;
	update_joint_one_bone2d_cache();
	notify_property_list_changed();
}

void SkeletonModification2DTwoBoneIK::set_target_minimum_distance(float p_distance) {
	ERR_FAIL_COND_MSG(p_distance < 0, "Target minimum distance cannot be less than zero!");
	target_minimum_distance = p_distance;
}

float SkeletonModification2DTwoBoneIK::get_target_minimum_distance() const {
	return target_minimum_distance;
}

void SkeletonModification2DTwoBoneIK::set_target_maximum_distance(float p_distance) {
	ERR_FAIL_COND_MSG(p_distance < 0, "Target maximum distance cannot be less than zero!");
	target_maximum_distance = p_distance;
}

float SkeletonModification2DTwoBoneIK::get_target_maximum_distance() const {
	return target_maximum_distance;
}

void SkeletonModification2DTwoBoneIK::set_flip_bend_direction(bool p_flip_direction) {
	flip_bend_direction = p_flip_direction;

#ifdef TOOLS_ENABLED
	if (stack && is_setup) {
		stack->set_editor_gizmos_dirty(true);
	}
#endif // TOOLS_ENABLED
}

bool SkeletonModification2DTwoBoneIK::get_flip_bend_direction() const {
	return flip_bend_direction;
}

NodePath SkeletonModification2DTwoBoneIK::get_joint_one_bone2d_node() const {
	return joint_one_bone2d_node;
}

void SkeletonModification2DTwoBoneIK::set_joint_two_bone2d_node(const NodePath &p_target_node) {
	joint_two_bone2d_node = p_target_node;
	update_joint_two_bone2d_cache();
	notify_property_list_changed();
}

NodePath SkeletonModification2DTwoBoneIK::get_joint_two_bone2d_node() const {
	return joint_two_bone2d_node;
}

void SkeletonModification2DTwoBoneIK::set_joint_one_bone_idx(int p_bone_idx) {
	ERR_FAIL_COND_MSG(p_bone_idx < 0, "Bone index is out of range: The index is too low!");

	if (is_setup) {
		if (stack->skeleton) {
			ERR_FAIL_INDEX_MSG(p_bone_idx, stack->skeleton->get_bone_count(), "Passed-in Bone index is out of range!");
			joint_one_bone_idx = p_bone_idx;
			joint_one_bone2d_node_cache = stack->skeleton->get_bone(p_bone_idx)->get_instance_id();
			joint_one_bone2d_node = stack->skeleton->get_path_to(stack->skeleton->get_bone(p_bone_idx));
		} else {
			WARN_PRINT("TwoBoneIK: Cannot verify the joint bone index for joint one...");
			joint_one_bone_idx = p_bone_idx;
		}
	} else {
		joint_one_bone_idx = p_bone_idx;
	}

	notify_property_list_changed();
}

int SkeletonModification2DTwoBoneIK::get_joint_one_bone_idx() const {
	return joint_one_bone_idx;
}

void SkeletonModification2DTwoBoneIK::set_joint_two_bone_idx(int p_bone_idx) {
	ERR_FAIL_COND_MSG(p_bone_idx < 0, "Bone index is out of range: The index is too low!");

	if (is_setup) {
		if (stack->skeleton) {
			ERR_FAIL_INDEX_MSG(p_bone_idx, stack->skeleton->get_bone_count(), "Passed-in Bone index is out of range!");
			joint_two_bone_idx = p_bone_idx;
			joint_two_bone2d_node_cache = stack->skeleton->get_bone(p_bone_idx)->get_instance_id();
			joint_two_bone2d_node = stack->skeleton->get_path_to(stack->skeleton->get_bone(p_bone_idx));
		} else {
			WARN_PRINT("TwoBoneIK: Cannot verify the joint bone index for joint two...");
			joint_two_bone_idx = p_bone_idx;
		}
	} else {
		joint_two_bone_idx = p_bone_idx;
	}

	notify_property_list_changed();
}

int SkeletonModification2DTwoBoneIK::get_joint_two_bone_idx() const {
	return joint_two_bone_idx;
}

#ifdef TOOLS_ENABLED
void SkeletonModification2DTwoBoneIK::set_editor_draw_min_max(bool p_draw) {
	editor_draw_min_max = p_draw;
}

bool SkeletonModification2DTwoBoneIK::get_editor_draw_min_max() const {
	return editor_draw_min_max;
}
#endif // TOOLS_ENABLED

void SkeletonModification2DTwoBoneIK::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_node", "target_nodepath"), &SkeletonModification2DTwoBoneIK::set_target_node);
	ClassDB::bind_method(D_METHOD("get_target_node"), &SkeletonModification2DTwoBoneIK::get_target_node);

	ClassDB::bind_method(D_METHOD("set_target_minimum_distance", "minimum_distance"), &SkeletonModification2DTwoBoneIK::set_target_minimum_distance);
	ClassDB::bind_method(D_METHOD("get_target_minimum_distance"), &SkeletonModification2DTwoBoneIK::get_target_minimum_distance);
	ClassDB::bind_method(D_METHOD("set_target_maximum_distance", "maximum_distance"), &SkeletonModification2DTwoBoneIK::set_target_maximum_distance);
	ClassDB::bind_method(D_METHOD("get_target_maximum_distance"), &SkeletonModification2DTwoBoneIK::get_target_maximum_distance);
	ClassDB::bind_method(D_METHOD("set_flip_bend_direction", "flip_direction"), &SkeletonModification2DTwoBoneIK::set_flip_bend_direction);
	ClassDB::bind_method(D_METHOD("get_flip_bend_direction"), &SkeletonModification2DTwoBoneIK::get_flip_bend_direction);

	ClassDB::bind_method(D_METHOD("set_joint_one_bone2d_node", "bone2d_node"), &SkeletonModification2DTwoBoneIK::set_joint_one_bone2d_node);
	ClassDB::bind_method(D_METHOD("get_joint_one_bone2d_node"), &SkeletonModification2DTwoBoneIK::get_joint_one_bone2d_node);
	ClassDB::bind_method(D_METHOD("set_joint_one_bone_idx", "bone_idx"), &SkeletonModification2DTwoBoneIK::set_joint_one_bone_idx);
	ClassDB::bind_method(D_METHOD("get_joint_one_bone_idx"), &SkeletonModification2DTwoBoneIK::get_joint_one_bone_idx);

	ClassDB::bind_method(D_METHOD("set_joint_two_bone2d_node", "bone2d_node"), &SkeletonModification2DTwoBoneIK::set_joint_two_bone2d_node);
	ClassDB::bind_method(D_METHOD("get_joint_two_bone2d_node"), &SkeletonModification2DTwoBoneIK::get_joint_two_bone2d_node);
	ClassDB::bind_method(D_METHOD("set_joint_two_bone_idx", "bone_idx"), &SkeletonModification2DTwoBoneIK::set_joint_two_bone_idx);
	ClassDB::bind_method(D_METHOD("get_joint_two_bone_idx"), &SkeletonModification2DTwoBoneIK::get_joint_two_bone_idx);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_nodepath", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node2D"), "set_target_node", "get_target_node");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "target_minimum_distance", PROPERTY_HINT_RANGE, "0,100000000,0.01,suffix:px"), "set_target_minimum_distance", "get_target_minimum_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "target_maximum_distance", PROPERTY_HINT_NONE, "0,100000000,0.01,suffix:px"), "set_target_maximum_distance", "get_target_maximum_distance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_bend_direction", PROPERTY_HINT_NONE, ""), "set_flip_bend_direction", "get_flip_bend_direction");
	ADD_GROUP("", "");
}

SkeletonModification2DTwoBoneIK::SkeletonModification2DTwoBoneIK() {
	stack = nullptr;
	is_setup = false;
	enabled = true;
	editor_draw_gizmo = true;
}

SkeletonModification2DTwoBoneIK::~SkeletonModification2DTwoBoneIK() {
}
