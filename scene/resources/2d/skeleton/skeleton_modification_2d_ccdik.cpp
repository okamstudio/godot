/**************************************************************************/
/*  skeleton_modification_2d_ccdik.cpp                                    */
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

#include "skeleton_modification_2d_ccdik.h"
#include "scene/2d/skeleton_2d.h"

bool SkeletonModification2DCCDIK::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;

	if (path.begins_with("joint_data/")) {
		int which = path.get_slicec('/', 1).to_int();
		String what = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, ccdik_data_chain.size(), false);

		if (what == "bone2d_node") {
			set_ccdik_joint_bone2d_node(which, p_value);
		} else if (what == "bone_index") {
			set_ccdik_joint_bone_index(which, p_value);
		} else if (what == "rotate_from_joint") {
			set_ccdik_joint_rotate_from_joint(which, p_value);
		} else if (what == "enable_constraint") {
			set_ccdik_joint_enable_constraint(which, p_value);
		} else if (what == "constraint_angle_min") {
			set_ccdik_joint_constraint_angle_min(which, Math::deg_to_rad(float(p_value)));
		} else if (what == "constraint_angle_max") {
			set_ccdik_joint_constraint_angle_max(which, Math::deg_to_rad(float(p_value)));
		} else if (what == "constraint_angle_invert") {
			set_ccdik_joint_constraint_angle_invert(which, p_value);
		} else if (what == "constraint_in_localspace") {
			set_ccdik_joint_constraint_in_localspace(which, p_value);
		}
#ifdef TOOLS_ENABLED
		else if (what.begins_with("editor_draw_gizmo")) {
			set_ccdik_joint_editor_draw_gizmo(which, p_value);
		}
#endif // TOOLS_ENABLED
		else {
			return false;
		}
	}
#ifdef TOOLS_ENABLED
	else if (path.begins_with("editor/draw_gizmo")) {
		set_editor_draw_gizmo(p_value);
	}
#endif // TOOLS_ENABLED
	else {
		return false;
	}

	return true;
}

bool SkeletonModification2DCCDIK::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;

	if (path.begins_with("joint_data/")) {
		int which = path.get_slicec('/', 1).to_int();
		String what = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, ccdik_data_chain.size(), false);

		if (what == "bone2d_node") {
			r_ret = get_ccdik_joint_bone2d_node(which);
		} else if (what == "bone_index") {
			r_ret = get_ccdik_joint_bone_index(which);
		} else if (what == "rotate_from_joint") {
			r_ret = get_ccdik_joint_rotate_from_joint(which);
		} else if (what == "enable_constraint") {
			r_ret = get_ccdik_joint_enable_constraint(which);
		} else if (what == "constraint_angle_min") {
			r_ret = Math::rad_to_deg(get_ccdik_joint_constraint_angle_min(which));
		} else if (what == "constraint_angle_max") {
			r_ret = Math::rad_to_deg(get_ccdik_joint_constraint_angle_max(which));
		} else if (what == "constraint_angle_invert") {
			r_ret = get_ccdik_joint_constraint_angle_invert(which);
		} else if (what == "constraint_in_localspace") {
			r_ret = get_ccdik_joint_constraint_in_localspace(which);
		}
#ifdef TOOLS_ENABLED
		else if (what.begins_with("editor_draw_gizmo")) {
			r_ret = get_ccdik_joint_editor_draw_gizmo(which);
		}
#endif // TOOLS_ENABLED
		else {
			return false;
		}
	}
#ifdef TOOLS_ENABLED
	else if (path.begins_with("editor/draw_gizmo")) {
		r_ret = get_editor_draw_gizmo();
	}
#endif // TOOLS_ENABLED
	else {
		return false;
	}

	return true;
}

void SkeletonModification2DCCDIK::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < ccdik_data_chain.size(); i++) {
		String base_string = "joint_data/" + itos(i) + "/";

		p_list->push_back(PropertyInfo(Variant::INT, base_string + "bone_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		p_list->push_back(PropertyInfo(Variant::NODE_PATH, base_string + "bone2d_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Bone2D", PROPERTY_USAGE_DEFAULT));
		p_list->push_back(PropertyInfo(Variant::BOOL, base_string + "rotate_from_joint", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));

		p_list->push_back(PropertyInfo(Variant::BOOL, base_string + "enable_constraint", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		if (ccdik_data_chain[i].enable_constraint) {
			p_list->push_back(PropertyInfo(Variant::FLOAT, base_string + "constraint_angle_min", PROPERTY_HINT_RANGE, "-360, 360, 0.01", PROPERTY_USAGE_DEFAULT));
			p_list->push_back(PropertyInfo(Variant::FLOAT, base_string + "constraint_angle_max", PROPERTY_HINT_RANGE, "-360, 360, 0.01", PROPERTY_USAGE_DEFAULT));
			p_list->push_back(PropertyInfo(Variant::BOOL, base_string + "constraint_angle_invert", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
			p_list->push_back(PropertyInfo(Variant::BOOL, base_string + "constraint_in_localspace", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		}

#ifdef TOOLS_ENABLED
		if (Engine::get_singleton()->is_editor_hint()) {
			p_list->push_back(PropertyInfo(Variant::BOOL, base_string + "editor_draw_gizmo", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		}
#endif // TOOLS_ENABLED
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		p_list->push_back(PropertyInfo(Variant::BOOL, "editor/draw_gizmo", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
	}
#endif // TOOLS_ENABLED
}

void SkeletonModification2DCCDIK::_execute(float p_delta) {
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
	if (tip_node_cache.is_null()) {
		WARN_PRINT_ONCE("Tip cache is out of date. Attempting to update...");
		update_tip_cache();
		return;
	}

	Node2D *target = Object::cast_to<Node2D>(ObjectDB::get_instance(target_node_cache));
	if (!target || !target->is_inside_tree()) {
		ERR_PRINT_ONCE("Target node is not in the scene tree. Cannot execute modification!");
		return;
	}

	Node2D *tip = Object::cast_to<Node2D>(ObjectDB::get_instance(tip_node_cache));
	if (!tip || !tip->is_inside_tree()) {
		ERR_PRINT_ONCE("Tip node is not in the scene tree. Cannot execute modification!");
		return;
	}

	for (int i = 0; i < ccdik_data_chain.size(); i++) {
		_execute_ccdik_joint(i, target, tip);
	}
}

void SkeletonModification2DCCDIK::_execute_ccdik_joint(int p_joint_idx, Node2D *p_target, Node2D *p_tip) {
	CCDIK_Joint_Data2D ccdik_data = ccdik_data_chain[p_joint_idx];
	if (ccdik_data.bone_idx < 0 || ccdik_data.bone_idx > stack->skeleton->get_bone_count()) {
		ERR_PRINT_ONCE("2D CCDIK joint: bone index not found!");
		return;
	}

	Bone2D *operation_bone = stack->skeleton->get_bone(ccdik_data.bone_idx);
	Transform2D operation_transform = operation_bone->get_global_transform();

	if (ccdik_data.rotate_from_joint) {
		// To rotate from the joint, simply look at the target!
		operation_transform.set_rotation(
				operation_transform.looking_at(p_target->get_global_position()).get_rotation() - operation_bone->get_bone_angle());
	} else {
		// How to rotate from the tip: get the difference of rotation needed from the tip to the target, from the perspective of the joint.
		// Because we are only using the offset, we do not need to account for the bone angle of the Bone2D node.
		float joint_to_tip = p_tip->get_global_position().angle_to_point(operation_transform.get_origin());
		float joint_to_target = p_target->get_global_position().angle_to_point(operation_transform.get_origin());
		operation_transform.set_rotation(
				operation_transform.get_rotation() + (joint_to_target - joint_to_tip));
	}

	// Reset scale
	operation_transform.set_scale(operation_bone->get_global_scale());

	// Apply constraints in globalspace:
	if (ccdik_data.enable_constraint && !ccdik_data.constraint_in_localspace) {
		operation_transform.set_rotation(clamp_angle(operation_transform.get_rotation(), ccdik_data.constraint_angle_min, ccdik_data.constraint_angle_max, ccdik_data.constraint_angle_invert));
	}

	// Convert from a global transform to a delta and then apply the delta to the local transform.
	operation_bone->set_global_transform(operation_transform);
	operation_transform = operation_bone->get_transform();

	// Apply constraints in localspace:
	if (ccdik_data.enable_constraint && ccdik_data.constraint_in_localspace) {
		operation_transform.set_rotation(clamp_angle(operation_transform.get_rotation(), ccdik_data.constraint_angle_min, ccdik_data.constraint_angle_max, ccdik_data.constraint_angle_invert));
	}

	// Set the local pose override, and to make sure child bones are also updated, set the transform of the bone.
	stack->skeleton->set_bone_local_pose_override(ccdik_data.bone_idx, operation_transform, stack->strength, true);
	operation_bone->set_transform(operation_transform);
	operation_bone->notification(operation_bone->NOTIFICATION_TRANSFORM_CHANGED);
}

void SkeletonModification2DCCDIK::_setup_modification(SkeletonModificationStack2D *p_stack) {
	stack = p_stack;

	if (stack != nullptr) {
		is_setup = true;
		update_target_cache();
		update_tip_cache();
	}
}

void SkeletonModification2DCCDIK::_draw_editor_gizmo() {
	if (!enabled || !is_setup) {
		return;
	}

	for (int i = 0; i < ccdik_data_chain.size(); i++) {
		if (!ccdik_data_chain[i].editor_draw_gizmo) {
			continue;
		}

		Bone2D *operation_bone = stack->skeleton->get_bone(ccdik_data_chain[i].bone_idx);
		editor_draw_angle_constraints(operation_bone, ccdik_data_chain[i].constraint_angle_min, ccdik_data_chain[i].constraint_angle_max,
				ccdik_data_chain[i].enable_constraint, ccdik_data_chain[i].constraint_in_localspace, ccdik_data_chain[i].constraint_angle_invert);
	}
}

void SkeletonModification2DCCDIK::update_target_cache() {
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

void SkeletonModification2DCCDIK::update_tip_cache() {
	if (!is_setup || !stack) {
		if (is_setup) {
			ERR_PRINT_ONCE("Cannot update tip cache: modification is not properly setup!");
		}
		return;
	}

	tip_node_cache = ObjectID();
	if (stack->skeleton) {
		if (stack->skeleton->is_inside_tree()) {
			if (stack->skeleton->has_node(tip_node)) {
				Node *node = stack->skeleton->get_node(tip_node);
				ERR_FAIL_COND_MSG(!node || stack->skeleton == node,
						"Cannot update tip cache: node is this modification's skeleton or cannot be found!");
				ERR_FAIL_COND_MSG(!node->is_inside_tree(),
						"Cannot update tip cache: node is not in the scene tree!");
				tip_node_cache = node->get_instance_id();
			}
		}
	}
}

void SkeletonModification2DCCDIK::ccdik_joint_update_bone2d_cache(int p_joint_idx) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "Cannot update bone2d cache: joint index out of range!");
	if (!is_setup || !stack) {
		if (is_setup) {
			ERR_PRINT_ONCE("Cannot update CCDIK Bone2D cache: modification is not properly setup!");
		}
		return;
	}

	ccdik_data_chain.write[p_joint_idx].bone2d_node_cache = ObjectID();
	if (stack->skeleton) {
		if (stack->skeleton->is_inside_tree()) {
			if (stack->skeleton->has_node(ccdik_data_chain[p_joint_idx].bone2d_node)) {
				Node *node = stack->skeleton->get_node(ccdik_data_chain[p_joint_idx].bone2d_node);
				ERR_FAIL_COND_MSG(!node || stack->skeleton == node,
						"Cannot update CCDIK joint " + itos(p_joint_idx) + " Bone2D cache: node is this modification's skeleton or cannot be found!");
				ERR_FAIL_COND_MSG(!node->is_inside_tree(),
						"Cannot update CCDIK joint " + itos(p_joint_idx) + " Bone2D cache: node is not in the scene tree!");
				ccdik_data_chain.write[p_joint_idx].bone2d_node_cache = node->get_instance_id();

				Bone2D *bone = Object::cast_to<Bone2D>(node);
				if (bone) {
					ccdik_data_chain.write[p_joint_idx].bone_idx = bone->get_index_in_skeleton();
				} else {
					ERR_FAIL_MSG("CCDIK joint " + itos(p_joint_idx) + " Bone2D cache: Nodepath to Bone2D is not a Bone2D node!");
				}
			}
		}
	}
}

void SkeletonModification2DCCDIK::set_target_node(const NodePath &p_target_node) {
	target_node = p_target_node;
	update_target_cache();
}

NodePath SkeletonModification2DCCDIK::get_target_node() const {
	return target_node;
}

void SkeletonModification2DCCDIK::set_tip_node(const NodePath &p_tip_node) {
	tip_node = p_tip_node;
	update_tip_cache();
}

NodePath SkeletonModification2DCCDIK::get_tip_node() const {
	return tip_node;
}

void SkeletonModification2DCCDIK::set_ccdik_data_chain_length(int p_length) {
	ccdik_data_chain.resize(p_length);
	notify_property_list_changed();
}

int SkeletonModification2DCCDIK::get_ccdik_data_chain_length() {
	return ccdik_data_chain.size();
}

void SkeletonModification2DCCDIK::set_ccdik_joint_bone2d_node(int p_joint_idx, const NodePath &p_target_node) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCDIK joint out of range!");
	ccdik_data_chain.write[p_joint_idx].bone2d_node = p_target_node;
	ccdik_joint_update_bone2d_cache(p_joint_idx);

	notify_property_list_changed();
}

NodePath SkeletonModification2DCCDIK::get_ccdik_joint_bone2d_node(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), NodePath(), "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].bone2d_node;
}

void SkeletonModification2DCCDIK::set_ccdik_joint_bone_index(int p_joint_idx, int p_bone_idx) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCCDIK joint out of range!");
	ERR_FAIL_COND_MSG(p_bone_idx < 0, "Bone index is out of range: The index is too low!");

	if (is_setup) {
		if (stack->skeleton) {
			ERR_FAIL_INDEX_MSG(p_bone_idx, stack->skeleton->get_bone_count(), "Passed-in Bone index is out of range!");
			ccdik_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
			ccdik_data_chain.write[p_joint_idx].bone2d_node_cache = stack->skeleton->get_bone(p_bone_idx)->get_instance_id();
			ccdik_data_chain.write[p_joint_idx].bone2d_node = stack->skeleton->get_path_to(stack->skeleton->get_bone(p_bone_idx));
		} else {
			WARN_PRINT("Cannot verify the CCDIK joint " + itos(p_joint_idx) + " bone index for this modification...");
			ccdik_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
		}
	} else {
		ccdik_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
	}

	notify_property_list_changed();
}

int SkeletonModification2DCCDIK::get_ccdik_joint_bone_index(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), -1, "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].bone_idx;
}

void SkeletonModification2DCCDIK::set_ccdik_joint_rotate_from_joint(int p_joint_idx, bool p_rotate_from_joint) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCDIK joint out of range!");
	ccdik_data_chain.write[p_joint_idx].rotate_from_joint = p_rotate_from_joint;
}

bool SkeletonModification2DCCDIK::get_ccdik_joint_rotate_from_joint(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), false, "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].rotate_from_joint;
}

void SkeletonModification2DCCDIK::set_ccdik_joint_enable_constraint(int p_joint_idx, bool p_constraint) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCDIK joint out of range!");
	ccdik_data_chain.write[p_joint_idx].enable_constraint = p_constraint;
	notify_property_list_changed();

#ifdef TOOLS_ENABLED
	if (stack && is_setup) {
		stack->set_editor_gizmos_dirty(true);
	}
#endif // TOOLS_ENABLED
}

bool SkeletonModification2DCCDIK::get_ccdik_joint_enable_constraint(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), false, "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].enable_constraint;
}

void SkeletonModification2DCCDIK::set_ccdik_joint_constraint_angle_min(int p_joint_idx, float p_angle_min) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCDIK joint out of range!");
	ccdik_data_chain.write[p_joint_idx].constraint_angle_min = p_angle_min;

#ifdef TOOLS_ENABLED
	if (stack && is_setup) {
		stack->set_editor_gizmos_dirty(true);
	}
#endif // TOOLS_ENABLED
}

float SkeletonModification2DCCDIK::get_ccdik_joint_constraint_angle_min(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), 0.0, "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].constraint_angle_min;
}

void SkeletonModification2DCCDIK::set_ccdik_joint_constraint_angle_max(int p_joint_idx, float p_angle_max) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCDIK joint out of range!");
	ccdik_data_chain.write[p_joint_idx].constraint_angle_max = p_angle_max;

#ifdef TOOLS_ENABLED
	if (stack && is_setup) {
		stack->set_editor_gizmos_dirty(true);
	}
#endif // TOOLS_ENABLED
}

float SkeletonModification2DCCDIK::get_ccdik_joint_constraint_angle_max(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), 0.0, "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].constraint_angle_max;
}

void SkeletonModification2DCCDIK::set_ccdik_joint_constraint_angle_invert(int p_joint_idx, bool p_invert) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCDIK joint out of range!");
	ccdik_data_chain.write[p_joint_idx].constraint_angle_invert = p_invert;

#ifdef TOOLS_ENABLED
	if (stack && is_setup) {
		stack->set_editor_gizmos_dirty(true);
	}
#endif // TOOLS_ENABLED
}

bool SkeletonModification2DCCDIK::get_ccdik_joint_constraint_angle_invert(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), false, "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].constraint_angle_invert;
}

void SkeletonModification2DCCDIK::set_ccdik_joint_constraint_in_localspace(int p_joint_idx, bool p_constraint_in_localspace) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCDIK joint out of range!");
	ccdik_data_chain.write[p_joint_idx].constraint_in_localspace = p_constraint_in_localspace;

#ifdef TOOLS_ENABLED
	if (stack && is_setup) {
		stack->set_editor_gizmos_dirty(true);
	}
#endif // TOOLS_ENABLED
}

bool SkeletonModification2DCCDIK::get_ccdik_joint_constraint_in_localspace(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), false, "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].constraint_in_localspace;
}

void SkeletonModification2DCCDIK::set_ccdik_joint_editor_draw_gizmo(int p_joint_idx, bool p_draw_gizmo) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, ccdik_data_chain.size(), "CCDIK joint out of range!");
	ccdik_data_chain.write[p_joint_idx].editor_draw_gizmo = p_draw_gizmo;

#ifdef TOOLS_ENABLED
	if (stack && is_setup) {
		stack->set_editor_gizmos_dirty(true);
	}
#endif // TOOLS_ENABLED
}

bool SkeletonModification2DCCDIK::get_ccdik_joint_editor_draw_gizmo(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, ccdik_data_chain.size(), false, "CCDIK joint out of range!");
	return ccdik_data_chain[p_joint_idx].editor_draw_gizmo;
}

void SkeletonModification2DCCDIK::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_node", "target_nodepath"), &SkeletonModification2DCCDIK::set_target_node);
	ClassDB::bind_method(D_METHOD("get_target_node"), &SkeletonModification2DCCDIK::get_target_node);
	ClassDB::bind_method(D_METHOD("set_tip_node", "tip_nodepath"), &SkeletonModification2DCCDIK::set_tip_node);
	ClassDB::bind_method(D_METHOD("get_tip_node"), &SkeletonModification2DCCDIK::get_tip_node);

	ClassDB::bind_method(D_METHOD("set_ccdik_data_chain_length", "length"), &SkeletonModification2DCCDIK::set_ccdik_data_chain_length);
	ClassDB::bind_method(D_METHOD("get_ccdik_data_chain_length"), &SkeletonModification2DCCDIK::get_ccdik_data_chain_length);

	ClassDB::bind_method(D_METHOD("set_ccdik_joint_bone2d_node", "joint_idx", "bone2d_nodepath"), &SkeletonModification2DCCDIK::set_ccdik_joint_bone2d_node);
	ClassDB::bind_method(D_METHOD("get_ccdik_joint_bone2d_node", "joint_idx"), &SkeletonModification2DCCDIK::get_ccdik_joint_bone2d_node);
	ClassDB::bind_method(D_METHOD("set_ccdik_joint_bone_index", "joint_idx", "bone_idx"), &SkeletonModification2DCCDIK::set_ccdik_joint_bone_index);
	ClassDB::bind_method(D_METHOD("get_ccdik_joint_bone_index", "joint_idx"), &SkeletonModification2DCCDIK::get_ccdik_joint_bone_index);
	ClassDB::bind_method(D_METHOD("set_ccdik_joint_rotate_from_joint", "joint_idx", "rotate_from_joint"), &SkeletonModification2DCCDIK::set_ccdik_joint_rotate_from_joint);
	ClassDB::bind_method(D_METHOD("get_ccdik_joint_rotate_from_joint", "joint_idx"), &SkeletonModification2DCCDIK::get_ccdik_joint_rotate_from_joint);
	ClassDB::bind_method(D_METHOD("set_ccdik_joint_enable_constraint", "joint_idx", "enable_constraint"), &SkeletonModification2DCCDIK::set_ccdik_joint_enable_constraint);
	ClassDB::bind_method(D_METHOD("get_ccdik_joint_enable_constraint", "joint_idx"), &SkeletonModification2DCCDIK::get_ccdik_joint_enable_constraint);
	ClassDB::bind_method(D_METHOD("set_ccdik_joint_constraint_angle_min", "joint_idx", "angle_min"), &SkeletonModification2DCCDIK::set_ccdik_joint_constraint_angle_min);
	ClassDB::bind_method(D_METHOD("get_ccdik_joint_constraint_angle_min", "joint_idx"), &SkeletonModification2DCCDIK::get_ccdik_joint_constraint_angle_min);
	ClassDB::bind_method(D_METHOD("set_ccdik_joint_constraint_angle_max", "joint_idx", "angle_max"), &SkeletonModification2DCCDIK::set_ccdik_joint_constraint_angle_max);
	ClassDB::bind_method(D_METHOD("get_ccdik_joint_constraint_angle_max", "joint_idx"), &SkeletonModification2DCCDIK::get_ccdik_joint_constraint_angle_max);
	ClassDB::bind_method(D_METHOD("set_ccdik_joint_constraint_angle_invert", "joint_idx", "invert"), &SkeletonModification2DCCDIK::set_ccdik_joint_constraint_angle_invert);
	ClassDB::bind_method(D_METHOD("get_ccdik_joint_constraint_angle_invert", "joint_idx"), &SkeletonModification2DCCDIK::get_ccdik_joint_constraint_angle_invert);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_nodepath", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node2D"), "set_target_node", "get_target_node");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "tip_nodepath", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node2D"), "set_tip_node", "get_tip_node");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ccdik_data_chain_length", PROPERTY_HINT_RANGE, "0, 100, 1"), "set_ccdik_data_chain_length", "get_ccdik_data_chain_length");
}

SkeletonModification2DCCDIK::SkeletonModification2DCCDIK() {
	stack = nullptr;
	is_setup = false;
	enabled = true;
	editor_draw_gizmo = true;
}

SkeletonModification2DCCDIK::~SkeletonModification2DCCDIK() {
}
