/**************************************************************************/
/*  physical_bone_simulator_3d.cpp                                        */
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

#include "physical_bone_simulator_3d.h"

#include "scene/3d/physics/physical_bone_3d.h"

void PhysicalBoneSimulator3D::_skeleton_changed(Skeleton3D *p_old, Skeleton3D *p_new) {
	if (p_old) {
		if (p_old->is_connected(SNAME("bone_list_changed"), callable_mp(this, &PhysicalBoneSimulator3D::_bone_list_changed))) {
			p_old->disconnect(SNAME("bone_list_changed"), callable_mp(this, &PhysicalBoneSimulator3D::_bone_list_changed));
		}
		if (p_old->is_connected(SceneStringName(pose_updated), callable_mp(this, &PhysicalBoneSimulator3D::_pose_updated))) {
			p_old->disconnect(SceneStringName(pose_updated), callable_mp(this, &PhysicalBoneSimulator3D::_pose_updated));
		}
	}
	if (p_new) {
		if (!p_new->is_connected(SNAME("bone_list_changed"), callable_mp(this, &PhysicalBoneSimulator3D::_bone_list_changed))) {
			p_new->connect(SNAME("bone_list_changed"), callable_mp(this, &PhysicalBoneSimulator3D::_bone_list_changed));
		}
		if (!p_new->is_connected(SceneStringName(pose_updated), callable_mp(this, &PhysicalBoneSimulator3D::_pose_updated))) {
			p_new->connect(SceneStringName(pose_updated), callable_mp(this, &PhysicalBoneSimulator3D::_pose_updated));
		}
	}
	_bone_list_changed();
}

void PhysicalBoneSimulator3D::_bone_list_changed() {
	bones.clear();
	Skeleton3D *skeleton = get_skeleton();
	if (!skeleton) {
		return;
	}
	for (int i = 0; i < skeleton->get_bone_count(); i++) {
		SimulatedBone sb;
		sb.parent = skeleton->get_bone_parent(i);
		sb.child_bones = skeleton->get_bone_children(i);
		bones.push_back(sb);
	}
	_rebuild_physical_bones_cache();
	_pose_updated();
}

void PhysicalBoneSimulator3D::_pose_updated() {
	Skeleton3D *skeleton = get_skeleton();
	if (!skeleton || simulating) {
		return;
	}
	ERR_FAIL_COND(skeleton->get_bone_count() != bones.size());
	for (int i = 0; i < skeleton->get_bone_count(); i++) {
		_bone_pose_updated(skeleton, i);
	}
}

void PhysicalBoneSimulator3D::_bone_pose_updated(Skeleton3D *p_skeleton, int p_bone_id) {
	ERR_FAIL_INDEX(p_bone_id, bones.size());
	bones.write[p_bone_id].global_pose = p_skeleton->get_bone_global_pose(p_bone_id);
}

void PhysicalBoneSimulator3D::_set_active(bool p_active) {
	if (!Engine::get_singleton()->is_editor_hint()) {
		_reset_physical_bones_state();
	}
}

void PhysicalBoneSimulator3D::_reset_physical_bones_state() {
	for (int i = 0; i < bones.size(); i += 1) {
		if (bones[i].physical_bone) {
			bones[i].physical_bone->reset_physics_simulation_state();
		}
	}
}

bool PhysicalBoneSimulator3D::is_simulating_physics() const {
	return simulating;
}

int PhysicalBoneSimulator3D::find_bone(const String &p_name) const {
	Skeleton3D *skeleton = get_skeleton();
	if (!skeleton) {
		return -1;
	}
	return skeleton->find_bone(p_name);
}

String PhysicalBoneSimulator3D::get_bone_name(int p_bone) const {
	Skeleton3D *skeleton = get_skeleton();
	if (!skeleton) {
		return String();
	}
	return skeleton->get_bone_name(p_bone);
}

int PhysicalBoneSimulator3D::get_bone_count() const {
	return bones.size();
}

bool PhysicalBoneSimulator3D::is_bone_parent_of(int p_bone, int p_parent_bone_id) const {
	Skeleton3D *skeleton = get_skeleton();
	if (!skeleton) {
		return false;
	}
	return skeleton->is_bone_parent_of(p_bone, p_parent_bone_id);
}

void PhysicalBoneSimulator3D::bind_physical_bone_to_bone(int p_bone, PhysicalBone3D *p_physical_bone) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);
	ERR_FAIL_COND(bones[p_bone].physical_bone);
	ERR_FAIL_NULL(p_physical_bone);
	bones.write[p_bone].physical_bone = p_physical_bone;

	_rebuild_physical_bones_cache();
}

void PhysicalBoneSimulator3D::unbind_physical_bone_from_bone(int p_bone) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);
	bones.write[p_bone].physical_bone = nullptr;

	_rebuild_physical_bones_cache();
}

PhysicalBone3D *PhysicalBoneSimulator3D::get_physical_bone(int p_bone) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, nullptr);

	return bones[p_bone].physical_bone;
}

PhysicalBone3D *PhysicalBoneSimulator3D::get_physical_bone_parent(int p_bone) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, nullptr);

	if (bones[p_bone].cache_parent_physical_bone) {
		return bones[p_bone].cache_parent_physical_bone;
	}

	return _get_physical_bone_parent(p_bone);
}

PhysicalBone3D *PhysicalBoneSimulator3D::_get_physical_bone_parent(int p_bone) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, nullptr);

	const int parent_bone = bones[p_bone].parent;
	if (parent_bone < 0) {
		return nullptr;
	}

	PhysicalBone3D *pb = bones[parent_bone].physical_bone;
	if (pb) {
		return pb;
	} else {
		return get_physical_bone_parent(parent_bone);
	}
}

void PhysicalBoneSimulator3D::_rebuild_physical_bones_cache() {
	const int b_size = bones.size();
	for (int i = 0; i < b_size; ++i) {
		PhysicalBone3D *parent_pb = _get_physical_bone_parent(i);
		if (parent_pb != bones[i].cache_parent_physical_bone) {
			bones.write[i].cache_parent_physical_bone = parent_pb;
			if (bones[i].physical_bone) {
				bones[i].physical_bone->_on_bone_parent_changed();
			}
		}
	}
}

#ifndef DISABLE_DEPRECATED
void _pb_stop_simulation_compat(Node *p_node) {
	PhysicalBoneSimulator3D *ps = Object::cast_to<PhysicalBoneSimulator3D>(p_node);
	if (ps) {
		return; // Prevent conflict.
	}
	for (int i = p_node->get_child_count() - 1; i >= 0; --i) {
		_pb_stop_simulation_compat(p_node->get_child(i));
	}
	PhysicalBone3D *pb = Object::cast_to<PhysicalBone3D>(p_node);
	if (pb) {
		pb->set_simulate_physics(false);
	}
}
#endif // _DISABLE_DEPRECATED

void _pb_stop_simulation(Node *p_node) {
	for (int i = p_node->get_child_count() - 1; i >= 0; --i) {
		PhysicalBone3D *pb = Object::cast_to<PhysicalBone3D>(p_node->get_child(i));
		if (!pb) {
			continue;
		}
		_pb_stop_simulation(pb);
	}
	PhysicalBone3D *pb = Object::cast_to<PhysicalBone3D>(p_node);
	if (pb) {
		pb->set_simulate_physics(false);
	}
}

void PhysicalBoneSimulator3D::physical_bones_stop_simulation() {
	simulating = false;
	_reset_physical_bones_state();
#ifndef DISABLE_DEPRECATED
	if (is_compat) {
		Skeleton3D *sk = get_skeleton();
		if (sk) {
			_pb_stop_simulation_compat(sk);
		}
	} else {
		_pb_stop_simulation(this);
	}
#else
	_pb_stop_simulation(this);
#endif // _DISABLE_DEPRECATED
}

#ifndef DISABLE_DEPRECATED
void _pb_start_simulation_compat(const PhysicalBoneSimulator3D *p_simulator, Node *p_node, const Vector<int> &p_sim_bones) {
	PhysicalBoneSimulator3D *ps = Object::cast_to<PhysicalBoneSimulator3D>(p_node);
	if (ps) {
		return; // Prevent conflict.
	}
	for (int i = p_node->get_child_count() - 1; i >= 0; --i) {
		_pb_start_simulation_compat(p_simulator, p_node->get_child(i), p_sim_bones);
	}
	PhysicalBone3D *pb = Object::cast_to<PhysicalBone3D>(p_node);
	if (pb) {
		if (p_sim_bones.is_empty()) { // If no bones are specified, activate ragdoll on full body.
			pb->set_simulate_physics(true);
		} else {
			for (int i = p_sim_bones.size() - 1; i >= 0; --i) {
				if (p_sim_bones[i] == pb->get_bone_id() || p_simulator->is_bone_parent_of(pb->get_bone_id(), p_sim_bones[i])) {
					pb->set_simulate_physics(true);
					break;
				}
			}
		}
	}
}
#endif // _DISABLE_DEPRECATED

void _pb_start_simulation(const PhysicalBoneSimulator3D *p_simulator, Node *p_node, const Vector<int> &p_sim_bones) {
	for (int i = p_node->get_child_count() - 1; i >= 0; --i) {
		PhysicalBone3D *pb = Object::cast_to<PhysicalBone3D>(p_node->get_child(i));
		if (!pb) {
			continue;
		}
		_pb_start_simulation(p_simulator, pb, p_sim_bones);
	}
	PhysicalBone3D *pb = Object::cast_to<PhysicalBone3D>(p_node);
	if (pb) {
		if (p_sim_bones.is_empty()) { // If no bones are specified, activate ragdoll on full body.
			pb->set_simulate_physics(true);
		} else {
			for (int i = p_sim_bones.size() - 1; i >= 0; --i) {
				if (p_sim_bones[i] == pb->get_bone_id() || p_simulator->is_bone_parent_of(pb->get_bone_id(), p_sim_bones[i])) {
					pb->set_simulate_physics(true);
					break;
				}
			}
		}
	}
}

void PhysicalBoneSimulator3D::physical_bones_start_simulation_on(const TypedArray<StringName> &p_bones) {
	_pose_updated();

	simulating = true;
	_reset_physical_bones_state();

	Vector<int> sim_bones;
	if (p_bones.size() > 0) {
		sim_bones.resize(p_bones.size());
		int c = 0;
		for (int i = sim_bones.size() - 1; i >= 0; --i) {
			int bone_id = find_bone(p_bones[i]);
			if (bone_id != -1) {
				sim_bones.write[c++] = bone_id;
			}
		}
		sim_bones.resize(c);
	}

#ifndef DISABLE_DEPRECATED
	if (is_compat) {
		Skeleton3D *sk = get_skeleton();
		if (sk) {
			_pb_start_simulation_compat(this, sk, sim_bones);
		}
	} else {
		_pb_start_simulation(this, this, sim_bones);
	}
#else
	_pb_start_simulation(this, this, sim_bones);
#endif // _DISABLE_DEPRECATED
}

void _physical_bones_add_remove_collision_exception(bool p_add, Node *p_node, RID p_exception) {
	for (int i = p_node->get_child_count() - 1; i >= 0; --i) {
		_physical_bones_add_remove_collision_exception(p_add, p_node->get_child(i), p_exception);
	}

	CollisionObject3D *co = Object::cast_to<CollisionObject3D>(p_node);
	if (co) {
		if (p_add) {
			PhysicsServer3D::get_singleton()->body_add_collision_exception(co->get_rid(), p_exception);
		} else {
			PhysicsServer3D::get_singleton()->body_remove_collision_exception(co->get_rid(), p_exception);
		}
	}
}

void PhysicalBoneSimulator3D::physical_bones_add_collision_exception(RID p_exception) {
	_physical_bones_add_remove_collision_exception(true, this, p_exception);
}

void PhysicalBoneSimulator3D::physical_bones_remove_collision_exception(RID p_exception) {
	_physical_bones_add_remove_collision_exception(false, this, p_exception);
}

Transform3D PhysicalBoneSimulator3D::get_bone_global_pose(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Transform3D());
	return bones[p_bone].global_pose;
}

void PhysicalBoneSimulator3D::set_bone_global_pose(int p_bone, const Transform3D &p_pose) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);
	bones.write[p_bone].global_pose = p_pose;
}

void PhysicalBoneSimulator3D::_process_modification() {
	Skeleton3D *skeleton = get_skeleton();
	if (!skeleton) {
		return;
	}
	ERR_FAIL_COND(skeleton->get_bone_count() != bones.size());
	for (int i = 0; i < skeleton->get_bone_count(); i++) {
		if (!bones[i].physical_bone) {
			continue;
		}
		if (bones[i].physical_bone->is_simulating_physics() == false) {
			_bone_pose_updated(skeleton, i);
			bones[i].physical_bone->reset_to_rest_position();
		} else if (simulating) {
			skeleton->set_bone_global_pose(i, bones[i].global_pose);
		}
	}
}

void PhysicalBoneSimulator3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_simulating_physics"), &PhysicalBoneSimulator3D::is_simulating_physics);

	ClassDB::bind_method(D_METHOD("physical_bones_stop_simulation"), &PhysicalBoneSimulator3D::physical_bones_stop_simulation);
	ClassDB::bind_method(D_METHOD("physical_bones_start_simulation", "bones"), &PhysicalBoneSimulator3D::physical_bones_start_simulation_on, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("physical_bones_add_collision_exception", "exception"), &PhysicalBoneSimulator3D::physical_bones_add_collision_exception);
	ClassDB::bind_method(D_METHOD("physical_bones_remove_collision_exception", "exception"), &PhysicalBoneSimulator3D::physical_bones_remove_collision_exception);
}

PhysicalBoneSimulator3D::PhysicalBoneSimulator3D() {
}
