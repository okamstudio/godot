/**************************************************************************/
/*  skeleton_modification_2d_jiggle.cpp                                   */
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

#include "skeleton_modification_2d_jiggle.h"

#include "scene/2d/skeleton_2d.h"
#include "scene/resources/world_2d.h"

bool SkeletonModification2DJiggle::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;

	if (path.begins_with("joint_data/")) {
		int which = path.get_slicec('/', 1).to_int();
		String what = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, jiggle_data_chain.size(), false);

		if (what == "bone2d_node") {
			set_jiggle_joint_bone2d_node(which, p_value);
		} else if (what == "bone_index") {
			set_jiggle_joint_bone_index(which, p_value);
		} else if (what == "override_defaults") {
			set_jiggle_joint_override(which, p_value);
		} else if (what == "stiffness") {
			set_jiggle_joint_stiffness(which, p_value);
		} else if (what == "mass") {
			set_jiggle_joint_mass(which, p_value);
		} else if (what == "damping") {
			set_jiggle_joint_damping(which, p_value);
		} else if (what == "use_gravity") {
			set_jiggle_joint_use_gravity(which, p_value);
		} else if (what == "gravity") {
			set_jiggle_joint_gravity(which, p_value);
		} else {
			return false;
		}
	} else if (path == "use_colliders") {
		set_use_colliders(p_value);
	} else if (path == "collision_mask") {
		set_collision_mask(p_value);
	} else {
		return false;
	}
	return true;
}

bool SkeletonModification2DJiggle::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;

	if (path.begins_with("joint_data/")) {
		int which = path.get_slicec('/', 1).to_int();
		String what = path.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(which, jiggle_data_chain.size(), false);

		if (what == "bone2d_node") {
			r_ret = get_jiggle_joint_bone2d_node(which);
		} else if (what == "bone_index") {
			r_ret = get_jiggle_joint_bone_index(which);
		} else if (what == "override_defaults") {
			r_ret = get_jiggle_joint_override(which);
		} else if (what == "stiffness") {
			r_ret = get_jiggle_joint_stiffness(which);
		} else if (what == "mass") {
			r_ret = get_jiggle_joint_mass(which);
		} else if (what == "damping") {
			r_ret = get_jiggle_joint_damping(which);
		} else if (what == "use_gravity") {
			r_ret = get_jiggle_joint_use_gravity(which);
		} else if (what == "gravity") {
			r_ret = get_jiggle_joint_gravity(which);
		} else {
			return false;
		}
	} else if (path == "use_colliders") {
		r_ret = get_use_colliders();
	} else if (path == "collision_mask") {
		r_ret = get_collision_mask();
	} else {
		return false;
	}
	return true;
}

void SkeletonModification2DJiggle::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::BOOL, "use_colliders", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
	if (use_colliders) {
		p_list->push_back(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_2D_PHYSICS, "", PROPERTY_USAGE_DEFAULT));
	}

	for (int i = 0; i < jiggle_data_chain.size(); i++) {
		String base_string = "joint_data/" + itos(i) + "/";

		p_list->push_back(PropertyInfo(Variant::INT, base_string + "bone_index", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
		p_list->push_back(PropertyInfo(Variant::NODE_PATH, base_string + "bone2d_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Bone2D", PROPERTY_USAGE_DEFAULT));
		p_list->push_back(PropertyInfo(Variant::BOOL, base_string + "override_defaults", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));

		if (jiggle_data_chain[i].override_defaults) {
			p_list->push_back(PropertyInfo(Variant::FLOAT, base_string + "stiffness", PROPERTY_HINT_RANGE, "0, 1000, 0.01", PROPERTY_USAGE_DEFAULT));
			p_list->push_back(PropertyInfo(Variant::FLOAT, base_string + "mass", PROPERTY_HINT_RANGE, "0, 1000, 0.01", PROPERTY_USAGE_DEFAULT));
			p_list->push_back(PropertyInfo(Variant::FLOAT, base_string + "damping", PROPERTY_HINT_RANGE, "0, 1, 0.01", PROPERTY_USAGE_DEFAULT));
			p_list->push_back(PropertyInfo(Variant::BOOL, base_string + "use_gravity", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
			if (jiggle_data_chain[i].use_gravity) {
				p_list->push_back(PropertyInfo(Variant::VECTOR2, base_string + "gravity", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT));
			}
		}
	}
}

void SkeletonModification2DJiggle::_execute(float p_delta) {
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
	Node2D *target = target_node_cache.get_object<Node2D>();
	if (!target || !target->is_inside_tree()) {
		ERR_PRINT_ONCE("Target node is not in the scene tree. Cannot execute modification!");
		return;
	}

	for (int i = 0; i < jiggle_data_chain.size(); i++) {
		_execute_jiggle_joint(i, target, p_delta);
	}
}

void SkeletonModification2DJiggle::_execute_jiggle_joint(int p_joint_idx, Node2D *p_target, float p_delta) {
	// Adopted from: https://wiki.unity3d.com/index.php/JiggleBone
	// With modifications by TwistedTwigleg.

	if (jiggle_data_chain[p_joint_idx].bone_idx <= -1 || jiggle_data_chain[p_joint_idx].bone_idx > stack->skeleton->get_bone_count()) {
		ERR_PRINT_ONCE("Jiggle joint " + itos(p_joint_idx) + " bone index is invalid. Cannot execute modification on joint...");
		return;
	}

	if (jiggle_data_chain[p_joint_idx].bone2d_node_cache.is_null() && !jiggle_data_chain[p_joint_idx].bone2d_node.is_empty()) {
		WARN_PRINT_ONCE("Bone2D cache for joint " + itos(p_joint_idx) + " is out of date. Updating...");
		jiggle_joint_update_bone2d_cache(p_joint_idx);
	}

	Bone2D *operation_bone = stack->skeleton->get_bone(jiggle_data_chain[p_joint_idx].bone_idx);
	if (!operation_bone) {
		ERR_PRINT_ONCE("Jiggle joint " + itos(p_joint_idx) + " does not have a Bone2D node or it cannot be found!");
		return;
	}

	Transform2D operation_bone_trans = operation_bone->get_global_transform();
	Vector2 target_position = p_target->get_global_position();

	jiggle_data_chain.write[p_joint_idx].force = (target_position - jiggle_data_chain[p_joint_idx].dynamic_position) * jiggle_data_chain[p_joint_idx].stiffness * p_delta;

	if (jiggle_data_chain[p_joint_idx].use_gravity) {
		jiggle_data_chain.write[p_joint_idx].force += jiggle_data_chain[p_joint_idx].gravity * p_delta;
	}

	jiggle_data_chain.write[p_joint_idx].acceleration = jiggle_data_chain[p_joint_idx].force / jiggle_data_chain[p_joint_idx].mass;
	jiggle_data_chain.write[p_joint_idx].velocity += jiggle_data_chain[p_joint_idx].acceleration * (1 - jiggle_data_chain[p_joint_idx].damping);

	jiggle_data_chain.write[p_joint_idx].dynamic_position += jiggle_data_chain[p_joint_idx].velocity + jiggle_data_chain[p_joint_idx].force;
	jiggle_data_chain.write[p_joint_idx].dynamic_position += operation_bone_trans.get_origin() - jiggle_data_chain[p_joint_idx].last_position;
	jiggle_data_chain.write[p_joint_idx].last_position = operation_bone_trans.get_origin();

	// Collision detection/response
	if (use_colliders) {
		if (execution_mode == SkeletonModificationStack2D::EXECUTION_MODE::execution_mode_physics_process) {
			Ref<World2D> world_2d = stack->skeleton->get_world_2d();
			ERR_FAIL_COND(world_2d.is_null());
			PhysicsDirectSpaceState2D *space_state = PhysicsServer2D::get_singleton()->space_get_direct_state(world_2d->get_space());
			PhysicsDirectSpaceState2D::RayResult ray_result;

			PhysicsDirectSpaceState2D::RayParameters ray_params;
			ray_params.from = operation_bone_trans.get_origin();
			ray_params.to = jiggle_data_chain[p_joint_idx].dynamic_position;
			ray_params.collision_mask = collision_mask;

			// Add exception support?
			bool ray_hit = space_state->intersect_ray(ray_params, ray_result);

			if (ray_hit) {
				jiggle_data_chain.write[p_joint_idx].dynamic_position = jiggle_data_chain[p_joint_idx].last_noncollision_position;
				jiggle_data_chain.write[p_joint_idx].acceleration = Vector2(0, 0);
				jiggle_data_chain.write[p_joint_idx].velocity = Vector2(0, 0);
			} else {
				jiggle_data_chain.write[p_joint_idx].last_noncollision_position = jiggle_data_chain[p_joint_idx].dynamic_position;
			}
		} else {
			WARN_PRINT_ONCE("Jiggle 2D modifier: You cannot detect colliders without the stack mode being set to _physics_process!");
		}
	}

	// Rotate the bone using the dynamic position!
	operation_bone_trans = operation_bone_trans.looking_at(jiggle_data_chain[p_joint_idx].dynamic_position);
	operation_bone_trans.set_rotation(operation_bone_trans.get_rotation() - operation_bone->get_bone_angle());

	// Reset scale
	operation_bone_trans.set_scale(operation_bone->get_global_scale());

	operation_bone->set_global_transform(operation_bone_trans);
	stack->skeleton->set_bone_local_pose_override(jiggle_data_chain[p_joint_idx].bone_idx, operation_bone->get_transform(), stack->strength, true);
}

void SkeletonModification2DJiggle::_update_jiggle_joint_data() {
	for (int i = 0; i < jiggle_data_chain.size(); i++) {
		if (!jiggle_data_chain[i].override_defaults) {
			set_jiggle_joint_stiffness(i, stiffness);
			set_jiggle_joint_mass(i, mass);
			set_jiggle_joint_damping(i, damping);
			set_jiggle_joint_use_gravity(i, use_gravity);
			set_jiggle_joint_gravity(i, gravity);
		}
	}
}

void SkeletonModification2DJiggle::_setup_modification(SkeletonModificationStack2D *p_stack) {
	stack = p_stack;

	if (stack) {
		is_setup = true;

		if (stack->skeleton) {
			for (int i = 0; i < jiggle_data_chain.size(); i++) {
				int bone_idx = jiggle_data_chain[i].bone_idx;
				if (bone_idx > 0 && bone_idx < stack->skeleton->get_bone_count()) {
					Bone2D *bone2d_node = stack->skeleton->get_bone(bone_idx);
					jiggle_data_chain.write[i].dynamic_position = bone2d_node->get_global_position();
				}

				jiggle_joint_update_bone2d_cache(i);
			}
		}

		update_target_cache();
	}
}

void SkeletonModification2DJiggle::update_target_cache() {
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

void SkeletonModification2DJiggle::jiggle_joint_update_bone2d_cache(int p_joint_idx) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, jiggle_data_chain.size(), "Cannot update bone2d cache: joint index out of range!");
	if (!is_setup || !stack) {
		if (is_setup) {
			ERR_PRINT_ONCE("Cannot update Jiggle " + itos(p_joint_idx) + " Bone2D cache: modification is not properly setup!");
		}
		return;
	}

	jiggle_data_chain.write[p_joint_idx].bone2d_node_cache = ObjectID();
	if (stack->skeleton) {
		if (stack->skeleton->is_inside_tree()) {
			if (stack->skeleton->has_node(jiggle_data_chain[p_joint_idx].bone2d_node)) {
				Node *node = stack->skeleton->get_node(jiggle_data_chain[p_joint_idx].bone2d_node);
				ERR_FAIL_COND_MSG(!node || stack->skeleton == node,
						"Cannot update Jiggle joint " + itos(p_joint_idx) + " Bone2D cache: node is this modification's skeleton or cannot be found!");
				ERR_FAIL_COND_MSG(!node->is_inside_tree(),
						"Cannot update Jiggle joint " + itos(p_joint_idx) + " Bone2D cache: node is not in scene tree!");
				jiggle_data_chain.write[p_joint_idx].bone2d_node_cache = node->get_instance_id();

				Bone2D *bone = Object::cast_to<Bone2D>(node);
				if (bone) {
					jiggle_data_chain.write[p_joint_idx].bone_idx = bone->get_index_in_skeleton();
				} else {
					ERR_FAIL_MSG("Jiggle joint " + itos(p_joint_idx) + " Bone2D cache: Nodepath to Bone2D is not a Bone2D node!");
				}
			}
		}
	}
}

void SkeletonModification2DJiggle::set_target_node(const NodePath &p_target_node) {
	target_node = p_target_node;
	update_target_cache();
}

NodePath SkeletonModification2DJiggle::get_target_node() const {
	return target_node;
}

void SkeletonModification2DJiggle::set_stiffness(float p_stiffness) {
	ERR_FAIL_COND_MSG(p_stiffness < 0, "Stiffness cannot be set to a negative value!");
	stiffness = p_stiffness;
	_update_jiggle_joint_data();
}

float SkeletonModification2DJiggle::get_stiffness() const {
	return stiffness;
}

void SkeletonModification2DJiggle::set_mass(float p_mass) {
	ERR_FAIL_COND_MSG(p_mass < 0, "Mass cannot be set to a negative value!");
	mass = p_mass;
	_update_jiggle_joint_data();
}

float SkeletonModification2DJiggle::get_mass() const {
	return mass;
}

void SkeletonModification2DJiggle::set_damping(float p_damping) {
	ERR_FAIL_COND_MSG(p_damping < 0, "Damping cannot be set to a negative value!");
	ERR_FAIL_COND_MSG(p_damping > 1, "Damping cannot be more than one!");
	damping = p_damping;
	_update_jiggle_joint_data();
}

float SkeletonModification2DJiggle::get_damping() const {
	return damping;
}

void SkeletonModification2DJiggle::set_use_gravity(bool p_use_gravity) {
	use_gravity = p_use_gravity;
	_update_jiggle_joint_data();
}

bool SkeletonModification2DJiggle::get_use_gravity() const {
	return use_gravity;
}

void SkeletonModification2DJiggle::set_gravity(Vector2 p_gravity) {
	gravity = p_gravity;
	_update_jiggle_joint_data();
}

Vector2 SkeletonModification2DJiggle::get_gravity() const {
	return gravity;
}

void SkeletonModification2DJiggle::set_use_colliders(bool p_use_colliders) {
	use_colliders = p_use_colliders;
	notify_property_list_changed();
}

bool SkeletonModification2DJiggle::get_use_colliders() const {
	return use_colliders;
}

void SkeletonModification2DJiggle::set_collision_mask(int p_mask) {
	collision_mask = p_mask;
}

int SkeletonModification2DJiggle::get_collision_mask() const {
	return collision_mask;
}

// Jiggle joint data functions
int SkeletonModification2DJiggle::get_jiggle_data_chain_length() {
	return jiggle_data_chain.size();
}

void SkeletonModification2DJiggle::set_jiggle_data_chain_length(int p_length) {
	ERR_FAIL_COND(p_length < 0);
	jiggle_data_chain.resize(p_length);
	notify_property_list_changed();
}

void SkeletonModification2DJiggle::set_jiggle_joint_bone2d_node(int p_joint_idx, const NodePath &p_target_node) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, jiggle_data_chain.size(), "Jiggle joint out of range!");
	jiggle_data_chain.write[p_joint_idx].bone2d_node = p_target_node;
	jiggle_joint_update_bone2d_cache(p_joint_idx);

	notify_property_list_changed();
}

NodePath SkeletonModification2DJiggle::get_jiggle_joint_bone2d_node(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, jiggle_data_chain.size(), NodePath(), "Jiggle joint out of range!");
	return jiggle_data_chain[p_joint_idx].bone2d_node;
}

void SkeletonModification2DJiggle::set_jiggle_joint_bone_index(int p_joint_idx, int p_bone_idx) {
	ERR_FAIL_INDEX_MSG(p_joint_idx, jiggle_data_chain.size(), "Jiggle joint out of range!");
	ERR_FAIL_COND_MSG(p_bone_idx < 0, "Bone index is out of range: The index is too low!");

	if (is_setup) {
		if (stack->skeleton) {
			ERR_FAIL_INDEX_MSG(p_bone_idx, stack->skeleton->get_bone_count(), "Passed-in Bone index is out of range!");
			jiggle_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
			jiggle_data_chain.write[p_joint_idx].bone2d_node_cache = stack->skeleton->get_bone(p_bone_idx)->get_instance_id();
			jiggle_data_chain.write[p_joint_idx].bone2d_node = stack->skeleton->get_path_to(stack->skeleton->get_bone(p_bone_idx));
		} else {
			WARN_PRINT("Cannot verify the Jiggle joint " + itos(p_joint_idx) + " bone index for this modification...");
			jiggle_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
		}
	} else {
		jiggle_data_chain.write[p_joint_idx].bone_idx = p_bone_idx;
	}

	notify_property_list_changed();
}

int SkeletonModification2DJiggle::get_jiggle_joint_bone_index(int p_joint_idx) const {
	ERR_FAIL_INDEX_V_MSG(p_joint_idx, jiggle_data_chain.size(), -1, "Jiggle joint out of range!");
	return jiggle_data_chain[p_joint_idx].bone_idx;
}

void SkeletonModification2DJiggle::set_jiggle_joint_override(int p_joint_idx, bool p_override) {
	ERR_FAIL_INDEX(p_joint_idx, jiggle_data_chain.size());
	jiggle_data_chain.write[p_joint_idx].override_defaults = p_override;
	_update_jiggle_joint_data();
	notify_property_list_changed();
}

bool SkeletonModification2DJiggle::get_jiggle_joint_override(int p_joint_idx) const {
	ERR_FAIL_INDEX_V(p_joint_idx, jiggle_data_chain.size(), false);
	return jiggle_data_chain[p_joint_idx].override_defaults;
}

void SkeletonModification2DJiggle::set_jiggle_joint_stiffness(int p_joint_idx, float p_stiffness) {
	ERR_FAIL_COND_MSG(p_stiffness < 0, "Stiffness cannot be set to a negative value!");
	ERR_FAIL_INDEX(p_joint_idx, jiggle_data_chain.size());
	jiggle_data_chain.write[p_joint_idx].stiffness = p_stiffness;
}

float SkeletonModification2DJiggle::get_jiggle_joint_stiffness(int p_joint_idx) const {
	ERR_FAIL_INDEX_V(p_joint_idx, jiggle_data_chain.size(), -1);
	return jiggle_data_chain[p_joint_idx].stiffness;
}

void SkeletonModification2DJiggle::set_jiggle_joint_mass(int p_joint_idx, float p_mass) {
	ERR_FAIL_COND_MSG(p_mass < 0, "Mass cannot be set to a negative value!");
	ERR_FAIL_INDEX(p_joint_idx, jiggle_data_chain.size());
	jiggle_data_chain.write[p_joint_idx].mass = p_mass;
}

float SkeletonModification2DJiggle::get_jiggle_joint_mass(int p_joint_idx) const {
	ERR_FAIL_INDEX_V(p_joint_idx, jiggle_data_chain.size(), -1);
	return jiggle_data_chain[p_joint_idx].mass;
}

void SkeletonModification2DJiggle::set_jiggle_joint_damping(int p_joint_idx, float p_damping) {
	ERR_FAIL_COND_MSG(p_damping < 0, "Damping cannot be set to a negative value!");
	ERR_FAIL_INDEX(p_joint_idx, jiggle_data_chain.size());
	jiggle_data_chain.write[p_joint_idx].damping = p_damping;
}

float SkeletonModification2DJiggle::get_jiggle_joint_damping(int p_joint_idx) const {
	ERR_FAIL_INDEX_V(p_joint_idx, jiggle_data_chain.size(), -1);
	return jiggle_data_chain[p_joint_idx].damping;
}

void SkeletonModification2DJiggle::set_jiggle_joint_use_gravity(int p_joint_idx, bool p_use_gravity) {
	ERR_FAIL_INDEX(p_joint_idx, jiggle_data_chain.size());
	jiggle_data_chain.write[p_joint_idx].use_gravity = p_use_gravity;
	notify_property_list_changed();
}

bool SkeletonModification2DJiggle::get_jiggle_joint_use_gravity(int p_joint_idx) const {
	ERR_FAIL_INDEX_V(p_joint_idx, jiggle_data_chain.size(), false);
	return jiggle_data_chain[p_joint_idx].use_gravity;
}

void SkeletonModification2DJiggle::set_jiggle_joint_gravity(int p_joint_idx, Vector2 p_gravity) {
	ERR_FAIL_INDEX(p_joint_idx, jiggle_data_chain.size());
	jiggle_data_chain.write[p_joint_idx].gravity = p_gravity;
}

Vector2 SkeletonModification2DJiggle::get_jiggle_joint_gravity(int p_joint_idx) const {
	ERR_FAIL_INDEX_V(p_joint_idx, jiggle_data_chain.size(), Vector2(0, 0));
	return jiggle_data_chain[p_joint_idx].gravity;
}

void SkeletonModification2DJiggle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_node", "target_nodepath"), &SkeletonModification2DJiggle::set_target_node);
	ClassDB::bind_method(D_METHOD("get_target_node"), &SkeletonModification2DJiggle::get_target_node);

	ClassDB::bind_method(D_METHOD("set_jiggle_data_chain_length", "length"), &SkeletonModification2DJiggle::set_jiggle_data_chain_length);
	ClassDB::bind_method(D_METHOD("get_jiggle_data_chain_length"), &SkeletonModification2DJiggle::get_jiggle_data_chain_length);

	ClassDB::bind_method(D_METHOD("set_stiffness", "stiffness"), &SkeletonModification2DJiggle::set_stiffness);
	ClassDB::bind_method(D_METHOD("get_stiffness"), &SkeletonModification2DJiggle::get_stiffness);
	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &SkeletonModification2DJiggle::set_mass);
	ClassDB::bind_method(D_METHOD("get_mass"), &SkeletonModification2DJiggle::get_mass);
	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &SkeletonModification2DJiggle::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &SkeletonModification2DJiggle::get_damping);
	ClassDB::bind_method(D_METHOD("set_use_gravity", "use_gravity"), &SkeletonModification2DJiggle::set_use_gravity);
	ClassDB::bind_method(D_METHOD("get_use_gravity"), &SkeletonModification2DJiggle::get_use_gravity);
	ClassDB::bind_method(D_METHOD("set_gravity", "gravity"), &SkeletonModification2DJiggle::set_gravity);
	ClassDB::bind_method(D_METHOD("get_gravity"), &SkeletonModification2DJiggle::get_gravity);

	ClassDB::bind_method(D_METHOD("set_use_colliders", "use_colliders"), &SkeletonModification2DJiggle::set_use_colliders);
	ClassDB::bind_method(D_METHOD("get_use_colliders"), &SkeletonModification2DJiggle::get_use_colliders);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"), &SkeletonModification2DJiggle::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &SkeletonModification2DJiggle::get_collision_mask);

	// Jiggle joint data functions
	ClassDB::bind_method(D_METHOD("set_jiggle_joint_bone2d_node", "joint_idx", "bone2d_node"), &SkeletonModification2DJiggle::set_jiggle_joint_bone2d_node);
	ClassDB::bind_method(D_METHOD("get_jiggle_joint_bone2d_node", "joint_idx"), &SkeletonModification2DJiggle::get_jiggle_joint_bone2d_node);
	ClassDB::bind_method(D_METHOD("set_jiggle_joint_bone_index", "joint_idx", "bone_idx"), &SkeletonModification2DJiggle::set_jiggle_joint_bone_index);
	ClassDB::bind_method(D_METHOD("get_jiggle_joint_bone_index", "joint_idx"), &SkeletonModification2DJiggle::get_jiggle_joint_bone_index);
	ClassDB::bind_method(D_METHOD("set_jiggle_joint_override", "joint_idx", "override"), &SkeletonModification2DJiggle::set_jiggle_joint_override);
	ClassDB::bind_method(D_METHOD("get_jiggle_joint_override", "joint_idx"), &SkeletonModification2DJiggle::get_jiggle_joint_override);
	ClassDB::bind_method(D_METHOD("set_jiggle_joint_stiffness", "joint_idx", "stiffness"), &SkeletonModification2DJiggle::set_jiggle_joint_stiffness);
	ClassDB::bind_method(D_METHOD("get_jiggle_joint_stiffness", "joint_idx"), &SkeletonModification2DJiggle::get_jiggle_joint_stiffness);
	ClassDB::bind_method(D_METHOD("set_jiggle_joint_mass", "joint_idx", "mass"), &SkeletonModification2DJiggle::set_jiggle_joint_mass);
	ClassDB::bind_method(D_METHOD("get_jiggle_joint_mass", "joint_idx"), &SkeletonModification2DJiggle::get_jiggle_joint_mass);
	ClassDB::bind_method(D_METHOD("set_jiggle_joint_damping", "joint_idx", "damping"), &SkeletonModification2DJiggle::set_jiggle_joint_damping);
	ClassDB::bind_method(D_METHOD("get_jiggle_joint_damping", "joint_idx"), &SkeletonModification2DJiggle::get_jiggle_joint_damping);
	ClassDB::bind_method(D_METHOD("set_jiggle_joint_use_gravity", "joint_idx", "use_gravity"), &SkeletonModification2DJiggle::set_jiggle_joint_use_gravity);
	ClassDB::bind_method(D_METHOD("get_jiggle_joint_use_gravity", "joint_idx"), &SkeletonModification2DJiggle::get_jiggle_joint_use_gravity);
	ClassDB::bind_method(D_METHOD("set_jiggle_joint_gravity", "joint_idx", "gravity"), &SkeletonModification2DJiggle::set_jiggle_joint_gravity);
	ClassDB::bind_method(D_METHOD("get_jiggle_joint_gravity", "joint_idx"), &SkeletonModification2DJiggle::get_jiggle_joint_gravity);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_nodepath", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node2D"), "set_target_node", "get_target_node");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "jiggle_data_chain_length", PROPERTY_HINT_RANGE, "0,100,1"), "set_jiggle_data_chain_length", "get_jiggle_data_chain_length");
	ADD_GROUP("Default Joint Settings", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stiffness"), "set_stiffness", "get_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass"), "set_mass", "get_mass");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping", PROPERTY_HINT_RANGE, "0, 1, 0.01"), "set_damping", "get_damping");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_gravity"), "set_use_gravity", "get_use_gravity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "gravity"), "set_gravity", "get_gravity");
	ADD_GROUP("", "");
}

SkeletonModification2DJiggle::SkeletonModification2DJiggle() {
	stack = nullptr;
	is_setup = false;
	jiggle_data_chain = Vector<Jiggle_Joint_Data2D>();
	stiffness = 3;
	mass = 0.75;
	damping = 0.75;
	use_gravity = false;
	gravity = Vector2(0, 6.0);
	enabled = true;
	editor_draw_gizmo = false; // Nothing to really show in a gizmo right now.
}

SkeletonModification2DJiggle::~SkeletonModification2DJiggle() {
}
