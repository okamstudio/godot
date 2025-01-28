/**************************************************************************/
/*  retarget_modifier_3d.cpp                                              */
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

#include "retarget_modifier_3d.h"

PackedStringArray RetargetModifier3D::get_configuration_warnings() const {
	PackedStringArray warnings = SkeletonModifier3D::get_configuration_warnings();
	if (child_skeletons.is_empty()) {
		warnings.push_back(RTR("There is no child Skeleton3D!"));
	}
	return warnings;
}

/// Caching

void RetargetModifier3D::_profile_changed(Ref<SkeletonProfile> p_old, Ref<SkeletonProfile> p_new) {
	if (p_old.is_valid() && p_old->is_connected(SNAME("profile_updated"), callable_mp(this, &RetargetModifier3D::cache_rests_with_reset))) {
		p_old->disconnect(SNAME("profile_updated"), callable_mp(this, &RetargetModifier3D::cache_rests_with_reset));
	}
	profile = p_new;
	if (p_new.is_valid() && !p_new->is_connected(SNAME("profile_updated"), callable_mp(this, &RetargetModifier3D::cache_rests_with_reset))) {
		p_new->connect(SNAME("profile_updated"), callable_mp(this, &RetargetModifier3D::cache_rests_with_reset));
	}
	cache_rests_with_reset();
}

void RetargetModifier3D::_skeleton_changed(Skeleton3D *p_old, Skeleton3D *p_new) {
	if (p_old && p_old->is_connected(SNAME("rest_updated"), callable_mp(this, &RetargetModifier3D::cache_rests))) {
		p_old->disconnect(SNAME("rest_updated"), callable_mp(this, &RetargetModifier3D::cache_rests));
	}
	if (p_new && !p_new->is_connected(SNAME("rest_updated"), callable_mp(this, &RetargetModifier3D::cache_rests))) {
		p_new->connect(SNAME("rest_updated"), callable_mp(this, &RetargetModifier3D::cache_rests));
	}
	cache_rests();
}

void RetargetModifier3D::cache_rests_with_reset() {
	_reset_child_skeleton_poses();
	cache_rests();
}

void RetargetModifier3D::cache_rests() {
	source_bone_ids.clear();

	Skeleton3D *source_skeleton = get_skeleton();
	if (profile.is_null() || !source_skeleton) {
		return;
	}

	PackedStringArray bone_names = profile->get_bone_names();
	for (const String &E : bone_names) {
		source_bone_ids.push_back(source_skeleton->find_bone(E));
	}

	for (int i = 0; i < child_skeletons.size(); i++) {
		_update_child_skeleton_rests(i);
	}
}

Vector<RetargetModifier3D::RetargetBoneInfo> RetargetModifier3D::cache_bone_global_rests(Skeleton3D *p_skeleton) {
	// Retarget global pose in model space:
	// tgt_global_pose.basis = src_global_pose.basis * src_rest.basis.inv * src_parent_global_rest.basis.inv * tgt_parent_global_rest.basis * tgt_rest.basis
	// tgt_global_pose.origin = src_global_pose.origin
	Skeleton3D *source_skeleton = get_skeleton();
	Vector<RetargetBoneInfo> bone_rests;
	if (profile.is_null() || !source_skeleton) {
		return bone_rests;
	}
	PackedStringArray bone_names = profile->get_bone_names();
	for (const String &E : bone_names) {
		RetargetBoneInfo rbi;
		int source_bone_id = source_skeleton->find_bone(E);
		if (source_bone_id >= 0) {
			Transform3D parent_global_rest;
			int bone_parent = source_skeleton->get_bone_parent(source_bone_id);
			if (bone_parent >= 0) {
				parent_global_rest = source_skeleton->get_bone_global_rest(bone_parent);
			}
			rbi.post_basis = source_skeleton->get_bone_rest(source_bone_id).basis.inverse() * parent_global_rest.basis.inverse();
		}
		int target_bone_id = p_skeleton->find_bone(E);
		rbi.bone_id = target_bone_id;
		if (target_bone_id >= 0) {
			Transform3D parent_global_rest;
			int bone_parent = p_skeleton->get_bone_parent(target_bone_id);
			if (bone_parent >= 0) {
				parent_global_rest = p_skeleton->get_bone_global_rest(bone_parent);
			}
			rbi.post_basis = rbi.post_basis * parent_global_rest.basis * p_skeleton->get_bone_rest(target_bone_id).basis;
		}
		bone_rests.push_back(rbi);
	}
	return bone_rests;
}

Vector<RetargetModifier3D::RetargetBoneInfo> RetargetModifier3D::cache_bone_rests(Skeleton3D *p_skeleton) {
	// Retarget pose in model space:
	// tgt_pose.basis = tgt_parent_global_rest.basis.inv * src_parent_global_rest.basis * src_pose.basis * src_rest.basis.inv * src_parent_global_rest.basis.inv * tgt_parent_global_rest.basis * tgt_rest.basis
	// tgt_pose.origin = tgt_parent_global_rest.basis.inv.xform(src_parent_global_rest.basis.xform(src_pose.origin - src_rest.origin)) + tgt_rest.origin
	Skeleton3D *source_skeleton = get_skeleton();
	Vector<RetargetBoneInfo> bone_rests;
	if (profile.is_null() || !source_skeleton) {
		return bone_rests;
	}
	PackedStringArray bone_names = profile->get_bone_names();
	for (const String &E : bone_names) {
		RetargetBoneInfo rbi;
		int source_bone_id = source_skeleton->find_bone(E);
		if (source_bone_id >= 0) {
			Transform3D parent_global_rest;
			int bone_parent = source_skeleton->get_bone_parent(source_bone_id);
			if (bone_parent >= 0) {
				parent_global_rest = source_skeleton->get_bone_global_rest(bone_parent);
			}
			rbi.pre_basis = parent_global_rest.basis;
			rbi.post_basis = source_skeleton->get_bone_rest(source_bone_id).basis.inverse() * parent_global_rest.basis.inverse();
		}

		int target_bone_id = p_skeleton->find_bone(E);
		rbi.bone_id = target_bone_id;
		if (target_bone_id >= 0) {
			Transform3D parent_global_rest;
			int bone_parent = p_skeleton->get_bone_parent(target_bone_id);
			if (bone_parent >= 0) {
				parent_global_rest = p_skeleton->get_bone_global_rest(bone_parent);
			}
			rbi.pre_basis = parent_global_rest.basis.inverse() * rbi.pre_basis;
			rbi.post_basis = rbi.post_basis * parent_global_rest.basis * p_skeleton->get_bone_rest(target_bone_id).basis;
		}
		bone_rests.push_back(rbi);
	}
	return bone_rests;
}

void RetargetModifier3D::_update_child_skeleton_rests(int p_child_skeleton_idx) {
	ERR_FAIL_INDEX(p_child_skeleton_idx, child_skeletons.size());
	Skeleton3D *c = Object::cast_to<Skeleton3D>(ObjectDB::get_instance(child_skeletons[p_child_skeleton_idx].skeleton_id));
	if (!c) {
		return;
	}
	if (use_global_pose) {
		child_skeletons.write[p_child_skeleton_idx].humanoid_bone_rests = cache_bone_global_rests(c);
	} else {
		child_skeletons.write[p_child_skeleton_idx].humanoid_bone_rests = cache_bone_rests(c);
	}
}

void RetargetModifier3D::_update_child_skeletons() {
	_reset_child_skeletons();

	for (int i = 0; i < get_child_count(); i++) {
		RetargetInfo ri;
		Skeleton3D *c = Object::cast_to<Skeleton3D>(get_child(i));
		if (c) {
			int id = child_skeletons.size();
			ri.skeleton_id = c->get_instance_id();
			child_skeletons.push_back(ri);
			c->connect(SNAME("rest_updated"), callable_mp(this, &RetargetModifier3D::_update_child_skeleton_rests).bind(id));
		}
	}

	cache_rests();
	update_configuration_warnings();
}

void RetargetModifier3D::_reset_child_skeleton_poses() {
	for (const RetargetInfo &E : child_skeletons) {
		Skeleton3D *c = Object::cast_to<Skeleton3D>(ObjectDB::get_instance(E.skeleton_id));
		if (!c) {
			continue;
		}
		if (c->is_connected(SNAME("rest_updated"), callable_mp(this, &RetargetModifier3D::_update_child_skeleton_rests))) {
			c->disconnect(SNAME("rest_updated"), callable_mp(this, &RetargetModifier3D::_update_child_skeleton_rests));
		}
		for (const RetargetBoneInfo &F : E.humanoid_bone_rests) {
			if (F.bone_id < 0) {
				continue;
			}
			c->reset_bone_pose(F.bone_id);
		}
	}
}

void RetargetModifier3D::_reset_child_skeletons() {
	_reset_child_skeleton_poses();
	child_skeletons.clear();
}

#ifdef TOOLS_ENABLED
void RetargetModifier3D::_force_update_child_skeletons() {
	for (const RetargetInfo &E : child_skeletons) {
		Skeleton3D *c = Object::cast_to<Skeleton3D>(ObjectDB::get_instance(E.skeleton_id));
		if (!c) {
			continue;
		}
		c->force_update_all_dirty_bones();
		c->emit_signal(SceneStringName(skeleton_updated));
	}
}
#endif // TOOLS_ENABLED

/// General functions

void RetargetModifier3D::add_child_notify(Node *p_child) {
	if (Object::cast_to<Skeleton3D>(p_child)) {
		_update_child_skeletons();
	}
}

void RetargetModifier3D::move_child_notify(Node *p_child) {
	if (Object::cast_to<Skeleton3D>(p_child)) {
		_update_child_skeletons();
	}
}

void RetargetModifier3D::remove_child_notify(Node *p_child) {
	if (Object::cast_to<Skeleton3D>(p_child)) {
		// Reset after process.
		callable_mp(this, &RetargetModifier3D::_update_child_skeletons).call_deferred();
	}
}

void RetargetModifier3D::_validate_property(PropertyInfo &p_property) const {
	if (use_global_pose) {
		if (p_property.name == "enable_flags") {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}
}

void RetargetModifier3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_profile", "profile"), &RetargetModifier3D::set_profile);
	ClassDB::bind_method(D_METHOD("get_profile"), &RetargetModifier3D::get_profile);
	ClassDB::bind_method(D_METHOD("set_use_global_pose", "use_global_pose"), &RetargetModifier3D::set_use_global_pose);
	ClassDB::bind_method(D_METHOD("is_using_global_pose"), &RetargetModifier3D::is_using_global_pose);
	ClassDB::bind_method(D_METHOD("set_enable_flags", "enable_flags"), &RetargetModifier3D::set_enable_flags);
	ClassDB::bind_method(D_METHOD("get_enable_flags"), &RetargetModifier3D::get_enable_flags);

	ClassDB::bind_method(D_METHOD("set_position_enabled", "enabled"), &RetargetModifier3D::set_position_enabled);
	ClassDB::bind_method(D_METHOD("is_position_enabled"), &RetargetModifier3D::is_position_enabled);
	ClassDB::bind_method(D_METHOD("set_rotation_enabled", "enabled"), &RetargetModifier3D::set_rotation_enabled);
	ClassDB::bind_method(D_METHOD("is_rotation_enabled"), &RetargetModifier3D::is_rotation_enabled);
	ClassDB::bind_method(D_METHOD("set_scale_enabled", "enabled"), &RetargetModifier3D::set_scale_enabled);
	ClassDB::bind_method(D_METHOD("is_scale_enabled"), &RetargetModifier3D::is_scale_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "profile", PROPERTY_HINT_RESOURCE_TYPE, "SkeletonProfile"), "set_profile", "get_profile");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_global_pose"), "set_use_global_pose", "is_using_global_pose");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "enable", PROPERTY_HINT_FLAGS, "Position,Rotation,Scale"), "set_enable_flags", "get_enable_flags");

	BIND_BITFIELD_FLAG(TRANSFORM_FLAG_POSITION);
	BIND_BITFIELD_FLAG(TRANSFORM_FLAG_ROTATION);
	BIND_BITFIELD_FLAG(TRANSFORM_FLAG_SCALE);
	BIND_BITFIELD_FLAG(TRANSFORM_FLAG_ALL);
}

void RetargetModifier3D::_set_active(bool p_active) {
	if (!p_active) {
		_reset_child_skeleton_poses();
	}
}

void RetargetModifier3D::_retarget_global_pose() {
	Skeleton3D *source_skeleton = get_skeleton();
	if (profile.is_null() || !source_skeleton) {
		return;
	}

	LocalVector<Transform3D> source_poses;
	if (influence < 1.0) {
		for (int source_bone_id : source_bone_ids) {
			source_poses.push_back(source_bone_id < 0 ? Transform3D() : source_skeleton->get_bone_global_rest(source_bone_id).interpolate_with(source_skeleton->get_bone_global_pose(source_bone_id), influence));
		}
	} else {
		for (int source_bone_id : source_bone_ids) {
			source_poses.push_back(source_bone_id < 0 ? Transform3D() : source_skeleton->get_bone_global_pose(source_bone_id));
		}
	}

	for (const RetargetInfo &E : child_skeletons) {
		Skeleton3D *target_skeleton = Object::cast_to<Skeleton3D>(ObjectDB::get_instance(E.skeleton_id));
		if (!target_skeleton) {
			continue;
		}
		for (int i = 0; i < source_bone_ids.size(); i++) {
			int target_bone_id = E.humanoid_bone_rests[i].bone_id;
			if (target_bone_id < 0) {
				continue;
			}
			Transform3D retarget_pose = source_poses[i];
			retarget_pose.basis = retarget_pose.basis * E.humanoid_bone_rests[i].post_basis;
			target_skeleton->set_bone_global_pose(target_bone_id, retarget_pose);
		}
	}
}

void RetargetModifier3D::_retarget_pose() {
	Skeleton3D *source_skeleton = get_skeleton();
	if (profile.is_null() || !source_skeleton) {
		return;
	}

	LocalVector<Transform3D> source_poses;
	if (influence < 1.0) {
		for (int source_bone_id : source_bone_ids) {
			source_poses.push_back(source_bone_id < 0 ? Transform3D() : source_skeleton->get_bone_rest(source_bone_id).interpolate_with(source_skeleton->get_bone_pose(source_bone_id), influence));
		}
	} else {
		for (int source_bone_id : source_bone_ids) {
			source_poses.push_back(source_bone_id < 0 ? Transform3D() : source_skeleton->get_bone_pose(source_bone_id));
		}
	}

	for (const RetargetInfo &E : child_skeletons) {
		Skeleton3D *target_skeleton = Object::cast_to<Skeleton3D>(ObjectDB::get_instance(E.skeleton_id));
		if (!target_skeleton) {
			continue;
		}
		float motion_scale_ratio = target_skeleton->get_motion_scale() / source_skeleton->get_motion_scale();
		for (int i = 0; i < source_bone_ids.size(); i++) {
			int target_bone_id = E.humanoid_bone_rests[i].bone_id;
			if (target_bone_id < 0) {
				continue;
			}
			int source_bone_id = source_bone_ids[i];
			if (source_bone_id < 0) {
				continue;
			}

			Transform3D extracted_transform = source_poses[i];
			extracted_transform.basis = E.humanoid_bone_rests[i].pre_basis * extracted_transform.basis * E.humanoid_bone_rests[i].post_basis;
			extracted_transform.origin = E.humanoid_bone_rests[i].pre_basis.xform((extracted_transform.origin - source_skeleton->get_bone_rest(source_bone_id).origin) * motion_scale_ratio) + target_skeleton->get_bone_rest(target_bone_id).origin;

			if (enable_flags.has_flag(TRANSFORM_FLAG_POSITION)) {
				target_skeleton->set_bone_pose_position(target_bone_id, extracted_transform.origin);
			}
			if (enable_flags.has_flag(TRANSFORM_FLAG_ROTATION)) {
				target_skeleton->set_bone_pose_rotation(target_bone_id, extracted_transform.basis.get_rotation_quaternion());
			}
			if (enable_flags.has_flag(TRANSFORM_FLAG_SCALE)) {
				target_skeleton->set_bone_pose_scale(target_bone_id, extracted_transform.basis.get_scale());
			}
		}
	}
}

void RetargetModifier3D::_process_modification() {
	if (use_global_pose) {
		_retarget_global_pose();
	} else {
		_retarget_pose();
	}
}

void RetargetModifier3D::set_profile(Ref<SkeletonProfile> p_profile) {
	if (profile == p_profile) {
		return;
	}
	_profile_changed(profile, p_profile);
}

Ref<SkeletonProfile> RetargetModifier3D::get_profile() const {
	return profile;
}

void RetargetModifier3D::set_use_global_pose(bool p_use_global_pose) {
	if (use_global_pose == p_use_global_pose) {
		return;
	}

	use_global_pose = p_use_global_pose;
	cache_rests_with_reset();

	notify_property_list_changed();
}

bool RetargetModifier3D::is_using_global_pose() const {
	return use_global_pose;
}

void RetargetModifier3D::set_enable_flags(BitField<TransformFlag> p_enable_flag) {
	if (enable_flags != p_enable_flag) {
		_reset_child_skeleton_poses();
	}
	enable_flags = p_enable_flag;
}

BitField<RetargetModifier3D::TransformFlag> RetargetModifier3D::get_enable_flags() const {
	return enable_flags;
}

void RetargetModifier3D::set_position_enabled(bool p_enabled) {
	if (enable_flags.has_flag(TRANSFORM_FLAG_POSITION) != p_enabled) {
		_reset_child_skeleton_poses();
	}
	if (p_enabled) {
		enable_flags.set_flag(TRANSFORM_FLAG_POSITION);
	} else {
		enable_flags.clear_flag(TRANSFORM_FLAG_POSITION);
	}
}

bool RetargetModifier3D::is_position_enabled() const {
	return enable_flags.has_flag(TRANSFORM_FLAG_POSITION);
}

void RetargetModifier3D::set_rotation_enabled(bool p_enabled) {
	if (enable_flags.has_flag(TRANSFORM_FLAG_ROTATION) != p_enabled) {
		_reset_child_skeleton_poses();
	}
	if (p_enabled) {
		enable_flags.set_flag(TRANSFORM_FLAG_ROTATION);
	} else {
		enable_flags.clear_flag(TRANSFORM_FLAG_ROTATION);
	}
}

bool RetargetModifier3D::is_rotation_enabled() const {
	return enable_flags.has_flag(TRANSFORM_FLAG_ROTATION);
}

void RetargetModifier3D::set_scale_enabled(bool p_enabled) {
	if (enable_flags.has_flag(TRANSFORM_FLAG_SCALE) != p_enabled) {
		_reset_child_skeleton_poses();
	}
	if (p_enabled) {
		enable_flags.set_flag(TRANSFORM_FLAG_SCALE);
	} else {
		enable_flags.clear_flag(TRANSFORM_FLAG_SCALE);
	}
}

bool RetargetModifier3D::is_scale_enabled() const {
	return enable_flags.has_flag(TRANSFORM_FLAG_SCALE);
}

void RetargetModifier3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_child_skeletons();
		} break;
#ifdef TOOLS_ENABLED
		case NOTIFICATION_EDITOR_PRE_SAVE: {
			_reset_child_skeleton_poses();
			_force_update_child_skeletons();
		} break;
#endif // TOOLS_ENABLED
		case NOTIFICATION_EXIT_TREE: {
			_reset_child_skeletons();
		} break;
	}
}

RetargetModifier3D::RetargetModifier3D() {
}

RetargetModifier3D::~RetargetModifier3D() {
}
