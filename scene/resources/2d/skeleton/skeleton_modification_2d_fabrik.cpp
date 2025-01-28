/**************************************************************************/
/*  skeleton_modification_2d_fabrik.cpp                                   */
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

#include "skeleton_modification_2d_fabrik.h"
#include "scene/2d/skeleton_2d.h"

bool SkeletonModification2DFABRIK::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;

	if (path.begins_with("joint_data/")) {
		int which = path.get_slicec('/', 1).to_int();
		String what = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, fabrik_data_chain.size(), false);

		if (what == "bone2d_node") {
			set_fabrik_joint_bone2d_node(which, p_value);
		} else if (what == "bone_index") {
			set_fabrik_joint_bone_index(which, p_value);
		} else if (what == "magnet_position") {
			set_fabrik_joint_magnet_position(which, p_value);
		} else if (what == "use_target_rotation") {
			set_fabrik_joint_use_target_rotation(which, p_value);
		} else {
			return false;
		}
	} else {
		return false;
	}

	return true;
}

bool SkeletonModification2DFABRIK::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;

	if (path.begins_with("joint_data/")) {
		int which = path.get_slicec('/', 1).to_int();
		String what = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, fabrik_data_chain.size(), false);

		if (what == "bone2d_node") {
			r_ret = get_fabrik_joint_bone2d_node(which);
		} else if (what == "bone_index") {
			r_ret = get_fabrik_joint_bone_index(which);
		} else if (what == "magnet_position") {
			r_ret = get_fabrik_joint_magnet_position(which);
		} else if (what == "use_target_rotation") {
			r_ret = get_fabrik_joint_use_target_rotation(which);
		} else {
			return false;
		}
	} else {
		return false;
	}
	return true;
}

void SkeletonModification2DFABRIK::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < fabrik_data_chain.size(); i++) {
		String base_string = "joint_data/" + itos(i) + "/";

		p_list->push_back(PropertyInfo(Variant::INT, base_string + "bone_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		p_list->push_back(PropertyInfo(Variant::NODE_PATH, base_string + "bone2d_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Bone2D", PROPERTY_USAGE_DEFAULT));

		if (i > 0) {
			p_list->push_back(PropertyInfo(Variant::VECTOR2, base_string + "magnet_position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		}
		if (i == fabrik_data_chain.size() - 1) {
			p_list->push_back(PropertyInfo(Variant::BOOL, base_string + "use_target_rotation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		}
	}
}

void SkeletonModification2DFABRIK::_execute(float p_delta) {
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

	if (fabrik_data_chain.size() <= 1) {
		ERR_PRINT_ONCE("FABRIK requires at least two joints to operate! Cannot execute modification!");
		return;
	}

	Node2D *target = Object::cast_to<Node2D>(ObjectDB::get_instance(target_node_cache));
	if (!target || !target->is_inside_tree()) {
		ERR_PRINT_ONCE("Target node is not in the scene tree. Cannot execute modification!");
		return;
	}
	target_global_pose = target->get_global_transform();

	if (fabrik_data_chain[0].bone2d_node_cache.is_null() && !fabrik_data_chain[0].bone2d_node.is_empty()) {
		fabrik_joint_update_bone2d_cache(0);
		WARN_PRINT("Bone2D cache for origin joint is out of date. Updating...");
	}

	Bone2D *origin_bone2d_node = Object::cast_to<Bone2D>(ObjectDB::get_instance(fabrik_data_chain[0].bone2d_node_cache));
	if (!origin_bone2d_node || !origin_bone2d_node->is_inside_tree()) {
		ERR_PRINT_ONCE("Origin joint's Bone2D node is not in the scene tree. Cannot execute modification!");
		return;
	}

	origin_global_pose = origin_bone2d_node->get_global_transform();

	if (fabrik_transform_chain.size() != fabrik_data_chain.size()) {
		fabrik_transform_chain.resize(fabrik_data_chain.size());
	}

	for (int i = 0; i < fabrik_data_chain.size(); i++) {
		// Update the transform chain
		if (fabrik_data_chain[i].bone2d_node_cache.is_null() && !fabrik_data_chain[i].bone2d_node.is_empty()) {
			WARN_PRINT_ONCE("Bone2D cache for joint " + itos(i) + " is out of date.. Attempting to update...");
			fabrik_joint_update_bone2d_cache(i);
		}
		Bone2D *joint_bone2d_node = Object::cast_to<Bone2D>(ObjectDB::get_instance(fabrik_data_chain[i].bone2d_node_cache));
		if (!joint_bone2d_node) {
			ERR_PRINT_ONCE("FABRIK Joint " + itos(i) + " does not have a Bone2D node set! Cannot execute modification!");
			return;
		}
		fabrik_transform_chain.write[i] = joint_bone2d_node->get_global_transform();
	}

	Bone2D *final_bone2d_node = Object::cast_to<Bone2D>(ObjectDB::get_instance(fabrik_data_chain[fabrik_data_chain.size() - 1].bone2d_node_cache));
	float final_bone2d_angle = final_bone2d_node->get_global_rotation();
	if (fabrik_data_chain[fabrik_data_chain.size() - 1].use_target_rotation) {
		final_bone2d_angle = target_global_pose.get_rotation();
	}
	Vector2 final_bone2d_direction = Vector2(Math::cos(final_bone2d_angle), Math::sin(final_bone2d_angle));
	float final_bone2d_length = final_bone2d_node->get_length() * MIN(final_bone2d_node->get_global_scale().x, final_bone2d_node->get_global_scale().y);
	float target_distance = (final_bone2d_node->get_global_position() + (final_bone2d_direction * final_bone2d_length)).distance_to(target->get_global_position());
	chain_iterations = 0;

	while (target_distance > chain_tolarance) {
		chain_backwards();
		chain_forwards();

		final_bone2d_angle = final_bone2d_node->get_global_rotation();
		if (fabrik_data_chain[fabrik_data_chain.size() - 1].use_target_rotation) {
			final_bone2d_angle = target_global_pose.get_rotation();
		}
		final_bone2d_direction = Vector2(Math::cos(final_bone2d_angle), Math::sin(final_bone2d_angle));
		target_distance = (final_bone2d_node->get_global_position() + (final_bone2d_direction * final_bone2d_length)).distance_to(target->get_global_position());

		chain_iterations += 1;
		if (chain_iterations >= chain_max_iterations) {
			break;
		}
	}

	// Apply all of the saved transforms to the Bone2D nodes
	for (int i = 0; i < fabrik_data_chain.size(); i++) {
		Bone2D *joint_bone2d_node = Object::cast_to<Bone2D>(ObjectDB::get_instance(fabrik_data_chain[i].bone2d_node_cache));
		if (!joint_bone2d_node) {
			ERR_PRINT_ONCE("FABRIK Joint " + itos(i) + " does not have a Bone2D node set!");
			continue;
		}
		Transform2D chain_trans = fabrik_transform_chain[i];

		// Apply rotation
		if (i + 1 < fabrik_data_chain.size()) {
			chain_trans = chain_trans.looking_at(fabrik_transform_chain[i + 1].get_origin());
		} else {
			if (fabrik_data_chain[i].use_target_rotation) {
				chain_trans.set_rotation(target_global_pose.get_rotation());
			} else {
				chain_trans = chain_trans.looking_at(target_global_pose.get_origin());
			}
		}
		// Adjust for the bone angle
		chain_trans.set_rotation(chain_trans.get_rotation() - joint_bone2d_node->get_bone_angle());

		// Reset scale
		chain_trans.set_scale(joint_bone2d_node->get_global_scale());

		// Apply to the bone, and to the override
		joint_bone2d_node->set_global_transform(chain_trans);
		stack->skeleton->set_bone_local_pose_override(fabrik_data_chain[i].bone_idx, joint_bone2d_node->get_transform(), stack->strength, true);
	}
}

void SkeletonModification2DFABRIK::chain_backwards() {
	int final_joint_index = fabrik_data_chain.size() - 1;
	Bone2D *final_bone2d_node = Object::cast_to<Bone2D>(ObjectDB::get_instance(fabrik_data_chain[final_joint_index].bone2d_node_cache));
	Transform2D final_bone2d_trans = fabrik_transform_chain[final_joint_index];

	// Apply magnet position
	if (final_joint_index != 0) {
		final_bone2d_trans.set_origin(final_bone2d_trans.get_origin() + fabrik_data_chain[final_joint_index].magnet_position);
	}

	// Set the rotation of the tip bone
	final_bone2d_trans = final_bone2d_trans.looking_at(target_global_pose.get_origin());

	// Set the position of the tip bone
	float final_bone2d_angle = final_bone2d_trans.get_rotation();
	if (fabrik_data_chain[final_joint_index].use_target_rotation) {
		final_bone2d_angle = target_global_pose.get_rotation();
	}
	Vector2 final_bone2d_direction = Vector2(Math::cos(final_bone2d_angle), Math::sin(final_bone2d_angle));
	float final_bone2d_length = final_bone2d_node->get_length() * MIN(final_bone2d_node->get_global_scale().x, final_bone2d_node->get_global_scale().y);
	final_bone2d_trans.set_origin(target_global_pose.get_origin() - (final_bone2d_direction * final_bone2d_length));

	// Save the transform
	fabrik_transform_chain.write[final_joint_index] = final_bone2d_trans;

	int i = final_joint_index;
	while (i >= 1) {
		Transform2D previous_pose = fabrik_transform_chain[i];
		i -= 1;
		Bone2D *current_bone2d_node = Object::cast_to<Bone2D>(ObjectDB::get_instance(fabrik_data_chain[i].bone2d_node_cache));
		Transform2D current_pose = fabrik_transform_chain[i];

		// Apply magnet position
		if (i != 0) {
			current_pose.set_origin(current_pose.get_origin() + fabrik_data_chain[i].magnet_position);
		}

		float current_bone2d_node_length = current_bone2d_node->get_length() * MIN(current_bone2d_node->get_global_scale().x, current_bone2d_node->get_global_scale().y);
		float length = current_bone2d_node_length / (current_pose.get_origin().distance_to(previous_pose.get_origin()));
		Vector2 finish_position = previous_pose.get_origin().lerp(current_pose.get_origin(), length);
		current_pose.set_origin(finish_position);

		// Save the transform
		fabrik_transform_chain.write[i] = current_pose;
	}
}

void SkeletonModification2DFABRIK::chain_forwards() {
	Transform2D origin_bone2d_trans = fabrik_transform_chain[0];
	origin_bone2d_trans.set_origin(origin_global_pose.get_origin());
	// Save the position
	fabrik_transform_chain.write[0] = origin_bone2d_trans;

	for (int i = 0; i < fabrik_data_chain.size() - 1; i++) {
		Bone2D *current_bone2d_node = Object::cast_to<Bone2D>(ObjectDB::get_instance(fabrik_data_chain[i].bone2d_node_cache));
		Transform2D current_pose = fabrik_transform_chain[i];
		Transform2D next_pose = fabrik_transform_chain[i + 1];

		float current_bone2d_node_length = current_bone2d_node->get_length() * MIN(current_bone2d_node->get_global_scale().x, current_bone2d_node->get_global_scale().y);
		float length = current_bone2d_node_length / (next_pose.get_origin().distance_to(current_pose.get_origin()));
		Vector2 finish_position = current_pose.get_origin().lerp(next_pose.get_origin(), length);
		current_pose.set_origin(finish_position);

		// Apply to the bone
		fabrik_transform_chain.write[i + 1] = current_pose;
	}
}

void SkeletonModification2DFABRIK::_setup_modification(SkeletonModificationStack2D *p_stack) {
	stack = p_stack;

	if (stack != nullptr) {
		is_setup = true;

		if (stack->skeleton) {
			for (int i = 0; i < fabrik_data_chain.size(); i++) {
				fabrik_joint_update_bone2d_cache(i);
			}
		}
		update_target_cache();
	}
}

void SkeletonModification2DFABRIK::update_target_cache() {
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
						"Cannot update target cache: node is not in scene tree!");
				target_node_cache = node->get_instance_id();
			}
		}
	}
}

void SkeletonModification2DFABRIK::fabrik_joint_update_bone2d_cache(int p_joint_idx) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, fabrik_data_chain.size(), "Cannot update bone2d cache: joint index out of range!");
	if (!is_setup || !stack) {
		if (is_setup) {
			ERR_PRINT_ONCE("Cannot update FABRIK Bone2D cache: modification is not properly setup!");
		}
		return;
	}

	fabrik_data_chain.write[p_joint_idx].bone2d_node_cache = ObjectID();
	if (stack->skeleton) {
		if (stack->skeleton->is_inside_tree()) {
			if (stack->skeleton->has_node(fabrik_data_chain[p_joint_idx].bone2d_node)) {
				Node *node = stack->skeleton->get_node(fabrik_data_chain[p_joint_idx].bone2d_node);
				ERR_FAIL_COND_MSG(!node || stack->skeleton == node,
						"Cannot update FABRIK joint " + itos(p_joint_idx) + " Bone2D cache: node is this modification's skeleton or cannot be found!");
				ERR_FAIL_COND_MSG(!node->is_inside_tree(),
						"Cannot update FABRIK joint " + itos(p_joint_idx) + " Bone2D cache: node is not in scene tree!");
				fabrik_data_chain.write[p_joint_idx].bone2d_node_cache = node->get_instance_id();

				Bone2D *bone = Object::cast_to<Bone2D>(node);
				if (bone) {
					fabrik_data_chain.write[p_joint_idx].bone_idx = bone->get_index_in_skeleton();
				} else {
					ERR_FAIL_MSG("FABRIK joint " + itos(p_joint_idx) + " Bone2D cache: Nodepath to Bone2D is not a Bone2D node!");
				}
			}
		}
	}
}

void SkeletonModification2DFABRIK::set_target_node(const NodePath &p_target_node) {
	target_node = p_target_node;
	update_target_cache();
}

NodePath SkeletonModification2DFABRIK::get_target_node() const {
	return target_node;
}

void SkeletonModification2DFABRIK::set_fabrik_data_chain_length(int p_length) {
	fabrik_data_chain.resize(p_length);
	notify_property_list_changed();
}

int SkeletonModification2DFABRIK::get_fabrik_data_chain_length() {
	return fabrik_data_chain.size();
}

void SkeletonModification2DFABRIK::set_fabrik_joint_bone2d_node(int p_joint_idx, const NodePath &p_target_node) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, fabrik_data_chain.size(), "FABRIK joint out of range!");
	fabrik_data_chain.write[p_joint_idx].bone2d_node = p_target_node;
	fabrik_joint_update_bone2d_cache(p_joint_idx);

	notify_property_list_changed();
}

NodePath SkeletonModification2DFABRIK::get_fabrik_joint_bone2d_node(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, fabrik_data_chain.size(), NodePath(), "FABRIK joint out of range!");
	return fabrik_data_chain[p_joint_idx].bone2d_node;
}

void SkeletonModification2DFABRIK::set_fabrik_joint_bone_index(int p_joint_idx, int p_bone_idx) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, fabrik_data_chain.size(), "FABRIK joint out of range!");
	ERR_FAIL_COND_MSG(p_bone_idx < 0, "Bone index is out of range: The index is too low!");

	if (is_setup) {
		if (stack->skeleton) {
			ERR_FAIL_INDEX_MSG(p_bone_idx, stack->skeleton->get_bone_count(), "Passed-in Bone index is out of range!");
			fabrik_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
			fabrik_data_chain.write[p_joint_idx].bone2d_node_cache = stack->skeleton->get_bone(p_bone_idx)->get_instance_id();
			fabrik_data_chain.write[p_joint_idx].bone2d_node = stack->skeleton->get_path_to(stack->skeleton->get_bone(p_bone_idx));
		} else {
			WARN_PRINT("Cannot verify the FABRIK joint " + itos(p_joint_idx) + " bone index for this modification...");
			fabrik_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
		}
	} else {
		fabrik_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
	}

	notify_property_list_changed();
}

int SkeletonModification2DFABRIK::get_fabrik_joint_bone_index(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, fabrik_data_chain.size(), -1, "FABRIK joint out of range!");
	return fabrik_data_chain[p_joint_idx].bone_idx;
}

void SkeletonModification2DFABRIK::set_fabrik_joint_magnet_position(int p_joint_idx, Vector2 p_magnet_position) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, fabrik_data_chain.size(), "FABRIK joint out of range!");
	fabrik_data_chain.write[p_joint_idx].magnet_position = p_magnet_position;
}

Vector2 SkeletonModification2DFABRIK::get_fabrik_joint_magnet_position(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, fabrik_data_chain.size(), Vector2(), "FABRIK joint out of range!");
	return fabrik_data_chain[p_joint_idx].magnet_position;
}

void SkeletonModification2DFABRIK::set_fabrik_joint_use_target_rotation(int p_joint_idx, bool p_use_target_rotation) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, fabrik_data_chain.size(), "FABRIK joint out of range!");
	fabrik_data_chain.write[p_joint_idx].use_target_rotation = p_use_target_rotation;
}

bool SkeletonModification2DFABRIK::get_fabrik_joint_use_target_rotation(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, fabrik_data_chain.size(), false, "FABRIK joint out of range!");
	return fabrik_data_chain[p_joint_idx].use_target_rotation;
}

void SkeletonModification2DFABRIK::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_node", "target_nodepath"), &SkeletonModification2DFABRIK::set_target_node);
	ClassDB::bind_method(D_METHOD("get_target_node"), &SkeletonModification2DFABRIK::get_target_node);

	ClassDB::bind_method(D_METHOD("set_fabrik_data_chain_length", "length"), &SkeletonModification2DFABRIK::set_fabrik_data_chain_length);
	ClassDB::bind_method(D_METHOD("get_fabrik_data_chain_length"), &SkeletonModification2DFABRIK::get_fabrik_data_chain_length);

	ClassDB::bind_method(D_METHOD("set_fabrik_joint_bone2d_node", "joint_idx", "bone2d_nodepath"), &SkeletonModification2DFABRIK::set_fabrik_joint_bone2d_node);
	ClassDB::bind_method(D_METHOD("get_fabrik_joint_bone2d_node", "joint_idx"), &SkeletonModification2DFABRIK::get_fabrik_joint_bone2d_node);
	ClassDB::bind_method(D_METHOD("set_fabrik_joint_bone_index", "joint_idx", "bone_idx"), &SkeletonModification2DFABRIK::set_fabrik_joint_bone_index);
	ClassDB::bind_method(D_METHOD("get_fabrik_joint_bone_index", "joint_idx"), &SkeletonModification2DFABRIK::get_fabrik_joint_bone_index);
	ClassDB::bind_method(D_METHOD("set_fabrik_joint_magnet_position", "joint_idx", "magnet_position"), &SkeletonModification2DFABRIK::set_fabrik_joint_magnet_position);
	ClassDB::bind_method(D_METHOD("get_fabrik_joint_magnet_position", "joint_idx"), &SkeletonModification2DFABRIK::get_fabrik_joint_magnet_position);
	ClassDB::bind_method(D_METHOD("set_fabrik_joint_use_target_rotation", "joint_idx", "use_target_rotation"), &SkeletonModification2DFABRIK::set_fabrik_joint_use_target_rotation);
	ClassDB::bind_method(D_METHOD("get_fabrik_joint_use_target_rotation", "joint_idx"), &SkeletonModification2DFABRIK::get_fabrik_joint_use_target_rotation);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_nodepath", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node2D"), "set_target_node", "get_target_node");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fabrik_data_chain_length", PROPERTY_HINT_RANGE, "0, 100, 1"), "set_fabrik_data_chain_length", "get_fabrik_data_chain_length");
}

SkeletonModification2DFABRIK::SkeletonModification2DFABRIK() {
	stack = nullptr;
	is_setup = false;
	enabled = true;
	editor_draw_gizmo = false;
}

SkeletonModification2DFABRIK::~SkeletonModification2DFABRIK() {
}
