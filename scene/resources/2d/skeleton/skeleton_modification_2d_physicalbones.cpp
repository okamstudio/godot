/**************************************************************************/
/*  skeleton_modification_2d_physicalbones.cpp                            */
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

#include "skeleton_modification_2d_physicalbones.h"
#include "scene/2d/physics/physical_bone_2d.h"
#include "scene/2d/skeleton_2d.h"

bool SkeletonModification2DPhysicalBones::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;

#ifdef TOOLS_ENABLED
	// Exposes a way to fetch the PhysicalBone2D nodes from the Godot editor.
	if (is_setup) {
		if (Engine::get_singleton()->is_editor_hint()) {
			if (path.begins_with("fetch_bones")) {
				fetch_physical_bones();
				notify_property_list_changed();
				return true;
			}
		}
	}
#endif //TOOLS_ENABLED

	if (path.begins_with("joint_")) {
		int which = path.get_slicec('_', 1).to_int();
		String what = path.get_slicec('_', 2);
		ERR_FAIL_INDEX_V(which, physical_bone_chain.size(), false);

		if (what == "nodepath") {
			set_physical_bone_node(which, p_value);
			return true;
		}
	}
	return false;
}

bool SkeletonModification2DPhysicalBones::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		if (path.begins_with("fetch_bones")) {
			return true; // Do nothing!
		}
	}
#endif //TOOLS_ENABLED

	if (path.begins_with("joint_")) {
		int which = path.get_slicec('_', 1).to_int();
		String what = path.get_slicec('_', 2);
		ERR_FAIL_INDEX_V(which, physical_bone_chain.size(), false);

		if (what == "nodepath") {
			r_ret = get_physical_bone_node(which);
			return true;
		}
	}
	return false;
}

void SkeletonModification2DPhysicalBones::_get_property_list(List<PropertyInfo> *p_list) const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		p_list->push_back(PropertyInfo(Variant::BOOL, "fetch_bones", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
	}
#endif //TOOLS_ENABLED

	for (int i = 0; i < physical_bone_chain.size(); i++) {
		String base_string = "joint_" + itos(i) + "_";

		p_list->push_back(PropertyInfo(Variant::NODE_PATH, base_string + "nodepath", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "PhysicalBone2D", PROPERTY_USAGE_DEFAULT));
	}
}

void SkeletonModification2DPhysicalBones::_execute(float p_delta) {
	ERR_FAIL_COND_MSG(!stack || !is_setup || stack->skeleton == nullptr,
			"Modification is not setup and therefore cannot execute!");
	if (!enabled) {
		return;
	}

	if (_simulation_state_dirty) {
		_update_simulation_state();
	}

	for (int i = 0; i < physical_bone_chain.size(); i++) {
		PhysicalBone_Data2D bone_data = physical_bone_chain[i];
		if (bone_data.physical_bone_node_cache.is_null()) {
			WARN_PRINT_ONCE("PhysicalBone2D cache " + itos(i) + " is out of date. Attempting to update...");
			_physical_bone_update_cache(i);
			continue;
		}

		PhysicalBone2D *physical_bone = Object::cast_to<PhysicalBone2D>(ObjectDB::get_instance(bone_data.physical_bone_node_cache));
		if (!physical_bone) {
			ERR_PRINT_ONCE("PhysicalBone2D not found at index " + itos(i) + "!");
			return;
		}
		if (physical_bone->get_bone2d_index() < 0 || physical_bone->get_bone2d_index() > stack->skeleton->get_bone_count()) {
			ERR_PRINT_ONCE("PhysicalBone2D at index " + itos(i) + " has invalid Bone2D!");
			return;
		}
		Bone2D *bone_2d = stack->skeleton->get_bone(physical_bone->get_bone2d_index());

		if (physical_bone->get_simulate_physics() && !physical_bone->get_follow_bone_when_simulating()) {
			bone_2d->set_global_transform(physical_bone->get_global_transform());
			stack->skeleton->set_bone_local_pose_override(physical_bone->get_bone2d_index(), bone_2d->get_transform(), stack->strength, true);
		}
	}
}

void SkeletonModification2DPhysicalBones::_setup_modification(SkeletonModificationStack2D *p_stack) {
	stack = p_stack;

	if (stack) {
		is_setup = true;

		if (stack->skeleton) {
			for (int i = 0; i < physical_bone_chain.size(); i++) {
				_physical_bone_update_cache(i);
			}
		}
	}
}

void SkeletonModification2DPhysicalBones::_physical_bone_update_cache(int p_joint_idx) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, physical_bone_chain.size(), "Cannot update PhysicalBone2D cache: joint index out of range!");
	if (!is_setup || !stack) {
		if (is_setup) {
			ERR_PRINT_ONCE("Cannot update PhysicalBone2D cache: modification is not properly setup!");
		}
		return;
	}

	physical_bone_chain.write[p_joint_idx].physical_bone_node_cache = ObjectID();
	if (stack->skeleton) {
		if (stack->skeleton->is_inside_tree()) {
			if (stack->skeleton->has_node(physical_bone_chain[p_joint_idx].physical_bone_node)) {
				Node *node = stack->skeleton->get_node(physical_bone_chain[p_joint_idx].physical_bone_node);
				ERR_FAIL_COND_MSG(!node || stack->skeleton == node,
						"Cannot update Physical Bone2D " + itos(p_joint_idx) + " cache: node is this modification's skeleton or cannot be found!");
				ERR_FAIL_COND_MSG(!node->is_inside_tree(),
						"Cannot update Physical Bone2D " + itos(p_joint_idx) + " cache: node is not in scene tree!");
				physical_bone_chain.write[p_joint_idx].physical_bone_node_cache = node->get_instance_id();
			}
		}
	}
}

int SkeletonModification2DPhysicalBones::get_physical_bone_chain_length() {
	return physical_bone_chain.size();
}

void SkeletonModification2DPhysicalBones::set_physical_bone_chain_length(int p_length) {
	ERR_FAIL_COND(p_length < 0);
	physical_bone_chain.resize(p_length);
	notify_property_list_changed();
}

void SkeletonModification2DPhysicalBones::fetch_physical_bones() {
	ERR_FAIL_NULL_MSG(stack, "No modification stack found! Cannot fetch physical bones!");
	ERR_FAIL_NULL_MSG(stack->skeleton, "No skeleton found! Cannot fetch physical bones!");

	physical_bone_chain.clear();

	List<Node *> node_queue = List<Node *>();
	node_queue.push_back(stack->skeleton);

	while (node_queue.size() > 0) {
		Node *node_to_process = node_queue.get_front();
		node_queue.pop_front();

		if (node_to_process != nullptr) {
			PhysicalBone2D *potential_bone = Object::cast_to<PhysicalBone2D>(node_to_process);
			if (potential_bone) {
				PhysicalBone_Data2D new_data = PhysicalBone_Data2D();
				new_data.physical_bone_node = stack->skeleton->get_path_to(potential_bone);
				new_data.physical_bone_node_cache = potential_bone->get_instance_id();
				physical_bone_chain.push_back(new_data);
			}
			for (int i = 0; i < node_to_process->get_child_count(); i++) {
				node_queue.push_back(node_to_process->get_child(i));
			}
		}
	}
}

void SkeletonModification2DPhysicalBones::start_simulation(const TypedArray<StringName> &p_bones) {
	_simulation_state_dirty = true;
	_simulation_state_dirty_names = p_bones;
	_simulation_state_dirty_process = true;

	if (is_setup) {
		_update_simulation_state();
	}
}

void SkeletonModification2DPhysicalBones::stop_simulation(const TypedArray<StringName> &p_bones) {
	_simulation_state_dirty = true;
	_simulation_state_dirty_names = p_bones;
	_simulation_state_dirty_process = false;

	if (is_setup) {
		_update_simulation_state();
	}
}

void SkeletonModification2DPhysicalBones::_update_simulation_state() {
	if (!_simulation_state_dirty) {
		return;
	}
	_simulation_state_dirty = false;

	if (_simulation_state_dirty_names.size() <= 0) {
		for (int i = 0; i < physical_bone_chain.size(); i++) {
			PhysicalBone2D *physical_bone = Object::cast_to<PhysicalBone2D>(stack->skeleton->get_node(physical_bone_chain[i].physical_bone_node));
			if (!physical_bone) {
				continue;
			}

			physical_bone->set_simulate_physics(_simulation_state_dirty_process);
		}
	} else {
		for (int i = 0; i < physical_bone_chain.size(); i++) {
			PhysicalBone2D *physical_bone = Object::cast_to<PhysicalBone2D>(ObjectDB::get_instance(physical_bone_chain[i].physical_bone_node_cache));
			if (!physical_bone) {
				continue;
			}
			if (_simulation_state_dirty_names.has(physical_bone->get_name())) {
				physical_bone->set_simulate_physics(_simulation_state_dirty_process);
			}
		}
	}
}

void SkeletonModification2DPhysicalBones::set_physical_bone_node(int p_joint_idx, const NodePath &p_nodepath) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, physical_bone_chain.size(), "Joint index out of range!");
	physical_bone_chain.write[p_joint_idx].physical_bone_node = p_nodepath;
	_physical_bone_update_cache(p_joint_idx);
}

NodePath SkeletonModification2DPhysicalBones::get_physical_bone_node(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, physical_bone_chain.size(), NodePath(), "Joint index out of range!");
	return physical_bone_chain[p_joint_idx].physical_bone_node;
}

void SkeletonModification2DPhysicalBones::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_physical_bone_chain_length", "length"), &SkeletonModification2DPhysicalBones::set_physical_bone_chain_length);
	ClassDB::bind_method(D_METHOD("get_physical_bone_chain_length"), &SkeletonModification2DPhysicalBones::get_physical_bone_chain_length);

	ClassDB::bind_method(D_METHOD("set_physical_bone_node", "joint_idx", "physicalbone2d_node"), &SkeletonModification2DPhysicalBones::set_physical_bone_node);
	ClassDB::bind_method(D_METHOD("get_physical_bone_node", "joint_idx"), &SkeletonModification2DPhysicalBones::get_physical_bone_node);

	ClassDB::bind_method(D_METHOD("fetch_physical_bones"), &SkeletonModification2DPhysicalBones::fetch_physical_bones);
	ClassDB::bind_method(D_METHOD("start_simulation", "bones"), &SkeletonModification2DPhysicalBones::start_simulation, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("stop_simulation", "bones"), &SkeletonModification2DPhysicalBones::stop_simulation, DEFVAL(Array()));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "physical_bone_chain_length", PROPERTY_HINT_RANGE, "0,100,1"), "set_physical_bone_chain_length", "get_physical_bone_chain_length");
}

SkeletonModification2DPhysicalBones::SkeletonModification2DPhysicalBones() {
	stack = nullptr;
	is_setup = false;
	physical_bone_chain = Vector<PhysicalBone_Data2D>();
	enabled = true;
	editor_draw_gizmo = false; // Nothing to really show in a gizmo right now.
}

SkeletonModification2DPhysicalBones::~SkeletonModification2DPhysicalBones() {
}
