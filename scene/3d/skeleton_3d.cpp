/**************************************************************************/
/*  skeleton_3d.cpp                                                       */
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

#include "skeleton_3d.h"
#include "skeleton_3d.compat.inc"

#include "scene/3d/skeleton_modifier_3d.h"
#ifndef DISABLE_DEPRECATED
#include "scene/3d/physical_bone_simulator_3d.h"
#endif // _DISABLE_DEPRECATED

void SkinReference::_skin_changed() {
	if (skeleton_node) {
		skeleton_node->_make_dirty();
	}
	skeleton_version = 0;
}

void SkinReference::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_skeleton"), &SkinReference::get_skeleton);
	ClassDB::bind_method(D_METHOD("get_skin"), &SkinReference::get_skin);
}

RID SkinReference::get_skeleton() const {
	return skeleton;
}

Ref<Skin> SkinReference::get_skin() const {
	return skin;
}

SkinReference::~SkinReference() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	if (skeleton_node) {
		skeleton_node->skin_bindings.erase(this);
	}
	RS::get_singleton()->free(skeleton);
}

///////////////////////////////////////

bool Skeleton3D::_set(const StringName &p_path, const Variant &p_value) {
#ifndef DISABLE_DEPRECATED
	if (p_path == SNAME("animate_physical_bones")) {
		set_animate_physical_bones(p_value);
		return true;
	}
#endif
	String path = p_path;

	if (!path.begins_with("bones/")) {
		return false;
	}

	uint32_t which = path.get_slicec('/', 1).to_int();
	String what = path.get_slicec('/', 2);

	if (which == bones.size() && what == "name") {
		add_bone(p_value);
		return true;
	}

	ERR_FAIL_UNSIGNED_INDEX_V(which, bones.size(), false);

	if (what == "parent") {
		set_bone_parent(which, p_value);
	} else if (what == "rest") {
		set_bone_rest(which, p_value);
	} else if (what == "enabled") {
		set_bone_enabled(which, p_value);
	} else if (what == "position") {
		set_bone_pose_position(which, p_value);
	} else if (what == "rotation") {
		set_bone_pose_rotation(which, p_value);
	} else if (what == "scale") {
		set_bone_pose_scale(which, p_value);
	} else if (what == "bone_meta") {
		set_bone_meta(which, path.get_slicec('/', 3), p_value);
#ifndef DISABLE_DEPRECATED
	} else if (what == "pose" || what == "bound_children") {
		// Kept for compatibility from 3.x to 4.x.
		WARN_DEPRECATED_MSG("Skeleton uses old pose format, which is deprecated (and loads slower). Consider re-importing or re-saving the scene." +
				(is_inside_tree() ? vformat(" Path: \"%s\"", get_path()) : String()));
		if (what == "pose") {
			// Old Skeleton poses were relative to rest, new ones are absolute, so we need to recompute the pose.
			// Skeleton3D nodes were always written with rest before pose, so this *SHOULD* work...
			Transform3D rest = get_bone_rest(which);
			Transform3D pose = rest * (Transform3D)p_value;
			set_bone_pose_position(which, pose.origin);
			set_bone_pose_rotation(which, pose.basis.get_rotation_quaternion());
			set_bone_pose_scale(which, pose.basis.get_scale());
		} else { // bound_children
			// This handles the case where the pose was set to the rest position; the pose property would == Transform() and would not be saved to the scene by default.
			// However, the bound_children property was always saved regardless of value, and it was always saved after both pose and rest.
			// We don't do anything else with bound_children, as it's not present on Skeleton3D.
			Vector3 pos = get_bone_pose_position(which);
			Quaternion rot = get_bone_pose_rotation(which);
			Vector3 scale = get_bone_pose_scale(which);
			Transform3D rest = get_bone_rest(which);
			if (rest != Transform3D() && pos == Vector3() && rot == Quaternion() && scale == Vector3(1, 1, 1)) {
				set_bone_pose_position(which, rest.origin);
				set_bone_pose_rotation(which, rest.basis.get_rotation_quaternion());
				set_bone_pose_scale(which, rest.basis.get_scale());
			}
		}
#endif
	} else {
		return false;
	}

	return true;
}

bool Skeleton3D::_get(const StringName &p_path, Variant &r_ret) const {
#ifndef DISABLE_DEPRECATED
	if (p_path == SNAME("animate_physical_bones")) {
		r_ret = get_animate_physical_bones();
		return true;
	}
#endif
	String path = p_path;

	if (!path.begins_with("bones/")) {
		return false;
	}

	uint32_t which = path.get_slicec('/', 1).to_int();
	String what = path.get_slicec('/', 2);

	ERR_FAIL_UNSIGNED_INDEX_V(which, bones.size(), false);

	if (what == "name") {
		r_ret = get_bone_name(which);
	} else if (what == "parent") {
		r_ret = get_bone_parent(which);
	} else if (what == "rest") {
		r_ret = get_bone_rest(which);
	} else if (what == "enabled") {
		r_ret = is_bone_enabled(which);
	} else if (what == "position") {
		r_ret = get_bone_pose_position(which);
	} else if (what == "rotation") {
		r_ret = get_bone_pose_rotation(which);
	} else if (what == "scale") {
		r_ret = get_bone_pose_scale(which);
	} else if (what == "bone_meta") {
		r_ret = get_bone_meta(which, path.get_slicec('/', 3));
	} else {
		return false;
	}

	return true;
}

void Skeleton3D::_get_property_list(List<PropertyInfo> *p_list) const {
	for (uint32_t i = 0; i < bones.size(); i++) {
		const String prep = vformat("%s/%d/", "bones", i);
		p_list->push_back(PropertyInfo(Variant::STRING, prep + "name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		p_list->push_back(PropertyInfo(Variant::INT, prep + "parent", PROPERTY_HINT_RANGE, "-1," + itos(bones.size() - 1) + ",1", PROPERTY_USAGE_NO_EDITOR));
		p_list->push_back(PropertyInfo(Variant::TRANSFORM3D, prep + "rest", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		p_list->push_back(PropertyInfo(Variant::BOOL, prep + "enabled", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		p_list->push_back(PropertyInfo(Variant::VECTOR3, prep + "position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		p_list->push_back(PropertyInfo(Variant::QUATERNION, prep + "rotation", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		p_list->push_back(PropertyInfo(Variant::VECTOR3, prep + "scale", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));

		for (const KeyValue<StringName, Variant> &K : bones[i].metadata) {
			PropertyInfo pi = PropertyInfo(bones[i].metadata[K.key].get_type(), prep + "bone_meta/" + K.key, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR);
			p_list->push_back(pi);
		}
	}

	for (PropertyInfo &E : *p_list) {
		_validate_property(E);
	}
}

void Skeleton3D::_validate_property(PropertyInfo &p_property) const {
	PackedStringArray split = p_property.name.split("/");
	if (split.size() == 3 && split[0] == "bones") {
		if (split[2] == "rest") {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
		if (is_show_rest_only()) {
			if (split[2] == "enabled") {
				p_property.usage |= PROPERTY_USAGE_READ_ONLY;
			}
			if (split[2] == "position") {
				p_property.usage |= PROPERTY_USAGE_READ_ONLY;
			}
			if (split[2] == "rotation") {
				p_property.usage |= PROPERTY_USAGE_READ_ONLY;
			}
			if (split[2] == "scale") {
				p_property.usage |= PROPERTY_USAGE_READ_ONLY;
			}
		} else if (!is_bone_enabled(split[1].to_int())) {
			if (split[2] == "position") {
				p_property.usage |= PROPERTY_USAGE_READ_ONLY;
			}
			if (split[2] == "rotation") {
				p_property.usage |= PROPERTY_USAGE_READ_ONLY;
			}
			if (split[2] == "scale") {
				p_property.usage |= PROPERTY_USAGE_READ_ONLY;
			}
		}
	}
}

void Skeleton3D::_update_process_order() const {
	if (!process_order_dirty) {
		return;
	}

	Bone *bonesptr = bones.ptr();
	int len = bones.size();

	parentless_bones.clear();

	for (int i = 0; i < len; i++) {
		bonesptr[i].child_bones.clear();
	}

	for (int i = 0; i < len; i++) {
		if (bonesptr[i].parent >= len) {
			// Validate this just in case.
			ERR_PRINT("Bone " + itos(i) + " has invalid parent: " + itos(bonesptr[i].parent));
			bonesptr[i].parent = -1;
		}

		if (bonesptr[i].parent != -1) {
			int parent_bone_idx = bonesptr[i].parent;

			// Check to see if this node is already added to the parent.
			if (!bonesptr[parent_bone_idx].child_bones.has(i)) {
				// Add the child node.
				bonesptr[parent_bone_idx].child_bones.push_back(i);
			} else {
				ERR_PRINT("Skeleton3D parenthood graph is cyclic");
			}
		} else {
			parentless_bones.push_back(i);
		}
	}

	concatenated_bone_names = StringName();

	_update_bones_nested_set();

	process_order_dirty = false;

	const_cast<Skeleton3D *>(this)->emit_signal("bone_list_changed");
}

void Skeleton3D::_update_bone_names() const {
	String names;
	for (uint32_t i = 0; i < bones.size(); i++) {
		if (i > 0) {
			names += ",";
		}
		names += bones[i].name;
	}
	concatenated_bone_names = StringName(names);
}

StringName Skeleton3D::get_concatenated_bone_names() const {
	if (concatenated_bone_names == StringName()) {
		_update_bone_names();
	}
	return concatenated_bone_names;
}

#ifndef DISABLE_DEPRECATED
void Skeleton3D::setup_simulator() {
	if (simulator && simulator->get_parent() == this) {
		remove_child(simulator);
		simulator->queue_free();
	}
	PhysicalBoneSimulator3D *sim = memnew(PhysicalBoneSimulator3D);
	simulator = sim;
	sim->is_compat = true;
	sim->set_active(false); // Don't run unneeded process.
	add_child(simulator, false, INTERNAL_MODE_BACK);
	set_animate_physical_bones(animate_physical_bones);
}
#endif // _DISABLE_DEPRECATED

void Skeleton3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_process_changed();
			_make_dirty();
			_make_modifiers_dirty();
			force_update_all_dirty_bones();
#ifndef DISABLE_DEPRECATED
			setup_simulator();
#endif // _DISABLE_DEPRECATED
			update_flags = UPDATE_FLAG_POSE;
			_notification(NOTIFICATION_UPDATE_SKELETON);
		} break;
#ifdef TOOLS_ENABLED
		case NOTIFICATION_EDITOR_PRE_SAVE: {
			force_update_all_dirty_bones();
			emit_signal(SceneStringName(skeleton_updated));
		} break;
#endif // TOOLS_ENABLED
		case NOTIFICATION_UPDATE_SKELETON: {
			// Update bone transforms to apply unprocessed poses.
			force_update_all_dirty_bones();

			updating = true;

			Bone *bonesptr = bones.ptr();
			int len = bones.size();

			thread_local LocalVector<bool> bone_global_pose_dirty_backup;

			// Process modifiers.

			thread_local LocalVector<BonePoseBackup> bones_backup;
			_find_modifiers();
			if (!modifiers.is_empty()) {
				bones_backup.resize(bones.size());
				// Store unmodified bone poses.
				for (uint32_t i = 0; i < bones.size(); i++) {
					bones_backup[i].save(bonesptr[i]);
				}
				// Store dirty flags for global bone poses.
				bone_global_pose_dirty_backup = bone_global_pose_dirty;

				_process_modifiers();
			}

			// Abort if pose is not changed.
			if (!(update_flags & UPDATE_FLAG_POSE)) {
				updating = false;
				update_flags = UPDATE_FLAG_NONE;
				return;
			}

			emit_signal(SceneStringName(skeleton_updated));

			// Update skins.
			RenderingServer *rs = RenderingServer::get_singleton();
			for (SkinReference *E : skin_bindings) {
				const Skin *skin = E->skin.operator->();
				RID skeleton = E->skeleton;
				uint32_t bind_count = skin->get_bind_count();

				if (E->bind_count != bind_count) {
					RS::get_singleton()->skeleton_allocate_data(skeleton, bind_count);
					E->bind_count = bind_count;
					E->skin_bone_indices.resize(bind_count);
					E->skin_bone_indices_ptrs = E->skin_bone_indices.ptrw();
				}

				if (E->skeleton_version != version) {
					for (uint32_t i = 0; i < bind_count; i++) {
						StringName bind_name = skin->get_bind_name(i);

						if (bind_name != StringName()) {
							// Bind name used, use this.
							bool found = false;
							for (int j = 0; j < len; j++) {
								if (bonesptr[j].name == bind_name) {
									E->skin_bone_indices_ptrs[i] = j;
									found = true;
									break;
								}
							}

							if (!found) {
								ERR_PRINT("Skin bind #" + itos(i) + " contains named bind '" + String(bind_name) + "' but Skeleton3D has no bone by that name.");
								E->skin_bone_indices_ptrs[i] = 0;
							}
						} else if (skin->get_bind_bone(i) >= 0) {
							int bind_index = skin->get_bind_bone(i);
							if (bind_index >= len) {
								ERR_PRINT("Skin bind #" + itos(i) + " contains bone index bind: " + itos(bind_index) + " , which is greater than the skeleton bone count: " + itos(len) + ".");
								E->skin_bone_indices_ptrs[i] = 0;
							} else {
								E->skin_bone_indices_ptrs[i] = bind_index;
							}
						} else {
							ERR_PRINT("Skin bind #" + itos(i) + " does not contain a name nor a bone index.");
							E->skin_bone_indices_ptrs[i] = 0;
						}
					}

					E->skeleton_version = version;
				}

				for (uint32_t i = 0; i < bind_count; i++) {
					uint32_t bone_index = E->skin_bone_indices_ptrs[i];
					ERR_CONTINUE(bone_index >= (uint32_t)len);
					rs->skeleton_bone_set_transform(skeleton, i, bonesptr[bone_index].global_pose * skin->get_bind_pose(i));
				}
			}

			if (!modifiers.is_empty()) {
				// Restore unmodified bone poses.
				for (uint32_t i = 0; i < bones.size(); i++) {
					bones_backup[i].restore(bones[i]);
				}
				// Restore dirty flags for global bone poses.
				bone_global_pose_dirty = bone_global_pose_dirty_backup;
			}

			updating = false;
			update_flags = UPDATE_FLAG_NONE;
		} break;
		case NOTIFICATION_INTERNAL_PROCESS:
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			_find_modifiers();
			if (!modifiers.is_empty()) {
				_update_deferred(UPDATE_FLAG_MODIFIER);
			}
		} break;
	}
}

void Skeleton3D::set_modifier_callback_mode_process(Skeleton3D::ModifierCallbackModeProcess p_mode) {
	if (modifier_callback_mode_process == p_mode) {
		return;
	}
	modifier_callback_mode_process = p_mode;
	_process_changed();
}

Skeleton3D::ModifierCallbackModeProcess Skeleton3D::get_modifier_callback_mode_process() const {
	return modifier_callback_mode_process;
}

void Skeleton3D::_process_changed() {
	if (modifier_callback_mode_process == MODIFIER_CALLBACK_MODE_PROCESS_IDLE) {
		set_process_internal(true);
		set_physics_process_internal(false);
	} else if (modifier_callback_mode_process == MODIFIER_CALLBACK_MODE_PROCESS_PHYSICS) {
		set_process_internal(false);
		set_physics_process_internal(true);
	}
}

void Skeleton3D::_make_modifiers_dirty() {
	modifiers_dirty = true;
	_update_deferred(UPDATE_FLAG_MODIFIER);
}

void Skeleton3D::_update_bones_nested_set() const {
	nested_set_offset_to_bone_index.resize(bones.size());
	bone_global_pose_dirty.resize(bones.size());
	_make_bone_global_poses_dirty();

	int offset = 0;
	for (int bone : parentless_bones) {
		offset += _update_bone_nested_set(bone, offset);
	}
}

int Skeleton3D::_update_bone_nested_set(int p_bone, int p_offset) const {
	Bone &bone = bones[p_bone];
	int offset = p_offset + 1;
	int span = 1;

	for (int child_bone : bone.child_bones) {
		int subspan = _update_bone_nested_set(child_bone, offset);
		offset += subspan;
		span += subspan;
	}

	nested_set_offset_to_bone_index[p_offset] = p_bone;
	bone.nested_set_offset = p_offset;
	bone.nested_set_span = span;

	return span;
}

void Skeleton3D::_make_bone_global_poses_dirty() const {
	for (uint32_t i = 0; i < bone_global_pose_dirty.size(); i++) {
		bone_global_pose_dirty[i] = true;
	}
}

void Skeleton3D::_make_bone_global_pose_subtree_dirty(int p_bone) const {
	if (process_order_dirty) {
		return;
	}

	const Bone &bone = bones[p_bone];
	int span_offset = bone.nested_set_offset;
	// No need to make subtree dirty when bone is already dirty.
	if (bone_global_pose_dirty[span_offset]) {
		return;
	}

	// Make global poses of subtree dirty.
	int span_end = span_offset + bone.nested_set_span;
	for (int i = span_offset; i < span_end; i++) {
		bone_global_pose_dirty[i] = true;
	}
}

void Skeleton3D::_update_bone_global_pose(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	_update_process_order();

	// Global pose is already calculated.
	int nested_set_offset = bones[p_bone].nested_set_offset;
	if (!bone_global_pose_dirty[nested_set_offset]) {
		return;
	}

	thread_local LocalVector<int> bone_list;
	bone_list.clear();
	Transform3D global_pose;

	// Create list of parent bones for which the global pose needs to be recalculated.
	for (int bone = p_bone; bone >= 0; bone = bones[bone].parent) {
		int offset = bones[bone].nested_set_offset;
		// Stop searching when global pose is not dirty.
		if (!bone_global_pose_dirty[offset]) {
			global_pose = bones[bone].global_pose;
			break;
		}

		bone_list.push_back(bone);
	}

	// Calculate global poses for all parent bones and the current bone.
	for (int i = bone_list.size() - 1; i >= 0; i--) {
		int bone_idx = bone_list[i];
		Bone &bone = bones[bone_idx];
		bool bone_enabled = bone.enabled && !show_rest_only;
		Transform3D bone_pose = bone_enabled ? get_bone_pose(bone_idx) : get_bone_rest(bone_idx);

		global_pose *= bone_pose;
#ifndef DISABLE_DEPRECATED
		if (bone.global_pose_override_amount >= CMP_EPSILON) {
			global_pose = global_pose.interpolate_with(bone.global_pose_override, bone.global_pose_override_amount);
		}
#endif // _DISABLE_DEPRECATED

		bone.global_pose = global_pose;
		bone_global_pose_dirty[bone.nested_set_offset] = false;
	}
}

Transform3D Skeleton3D::get_bone_global_pose(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Transform3D());
	_update_bone_global_pose(p_bone);
	return bones[p_bone].global_pose;
}

void Skeleton3D::set_bone_global_pose(int p_bone, const Transform3D &p_pose) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	Transform3D pt;
	if (bones[p_bone].parent >= 0) {
		pt = get_bone_global_pose(bones[p_bone].parent);
	}
	Transform3D t = pt.affine_inverse() * p_pose;
	set_bone_pose(p_bone, t);
}

void Skeleton3D::set_motion_scale(float p_motion_scale) {
	if (p_motion_scale <= 0) {
		motion_scale = 1;
		ERR_FAIL_MSG("Motion scale must be larger than 0.");
	}
	motion_scale = p_motion_scale;
}

float Skeleton3D::get_motion_scale() const {
	ERR_FAIL_COND_V(motion_scale <= 0, 1);
	return motion_scale;
}

// Skeleton creation api

uint64_t Skeleton3D::get_version() const {
	return version;
}

int Skeleton3D::add_bone(const String &p_name) {
	ERR_FAIL_COND_V_MSG(p_name.is_empty() || p_name.contains_char(':') || p_name.contains_char('/'), -1, vformat("Bone name cannot be empty or contain ':' or '/'.", p_name));
	ERR_FAIL_COND_V_MSG(name_to_bone_index.has(p_name), -1, vformat("Skeleton3D \"%s\" already has a bone with name \"%s\".", to_string(), p_name));

	Bone b;
	b.name = p_name;
	bones.push_back(b);
	int new_idx = bones.size() - 1;
	name_to_bone_index.insert(p_name, new_idx);
	process_order_dirty = true;
	version++;
	rest_dirty = true;
	_make_dirty();
	update_gizmos();
	return new_idx;
}

int Skeleton3D::find_bone(const String &p_name) const {
	const int *bone_index_ptr = name_to_bone_index.getptr(p_name);
	return bone_index_ptr != nullptr ? *bone_index_ptr : -1;
}

String Skeleton3D::get_bone_name(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, "");
	return bones[p_bone].name;
}

void Skeleton3D::set_bone_name(int p_bone, const String &p_name) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	const int *bone_index_ptr = name_to_bone_index.getptr(p_name);
	if (bone_index_ptr != nullptr) {
		ERR_FAIL_COND_MSG(*bone_index_ptr != p_bone, "Skeleton3D: '" + get_name() + "', bone name:  '" + p_name + "' already exists.");
		return; // No need to rename, the bone already has the given name.
	}

	name_to_bone_index.erase(bones[p_bone].name);
	bones[p_bone].name = p_name;
	name_to_bone_index.insert(p_name, p_bone);

	version++;
}

Variant Skeleton3D::get_bone_meta(int p_bone, const StringName &p_key) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Variant());

	if (!bones[p_bone].metadata.has(p_key)) {
		return Variant();
	}
	return bones[p_bone].metadata[p_key];
}

TypedArray<StringName> Skeleton3D::_get_bone_meta_list_bind(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, TypedArray<StringName>());

	TypedArray<StringName> _metaret;
	for (const KeyValue<StringName, Variant> &K : bones[p_bone].metadata) {
		_metaret.push_back(K.key);
	}
	return _metaret;
}

void Skeleton3D::get_bone_meta_list(int p_bone, List<StringName> *p_list) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	for (const KeyValue<StringName, Variant> &K : bones[p_bone].metadata) {
		p_list->push_back(K.key);
	}
}

bool Skeleton3D::has_bone_meta(int p_bone, const StringName &p_key) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, false);

	return bones[p_bone].metadata.has(p_key);
}

void Skeleton3D::set_bone_meta(int p_bone, const StringName &p_key, const Variant &p_value) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	if (p_value.get_type() == Variant::NIL) {
		if (bones[p_bone].metadata.has(p_key)) {
			bones[p_bone].metadata.erase(p_key);
		}
		return;
	}

	bones[p_bone].metadata.insert(p_key, p_value, false);
}

bool Skeleton3D::is_bone_parent_of(int p_bone, int p_parent_bone_id) const {
	int parent_of_bone = get_bone_parent(p_bone);

	if (-1 == parent_of_bone) {
		return false;
	}

	if (parent_of_bone == p_parent_bone_id) {
		return true;
	}

	return is_bone_parent_of(parent_of_bone, p_parent_bone_id);
}

int Skeleton3D::get_bone_count() const {
	return bones.size();
}

void Skeleton3D::set_bone_parent(int p_bone, int p_parent) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);
	ERR_FAIL_COND(p_parent != -1 && (p_parent < 0));
	ERR_FAIL_COND(p_bone == p_parent);

	bones[p_bone].parent = p_parent;
	process_order_dirty = true;
	rest_dirty = true;
	_make_dirty();
}

void Skeleton3D::unparent_bone_and_rest(int p_bone) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	_update_process_order();

	int parent = bones[p_bone].parent;
	while (parent >= 0) {
		bones[p_bone].rest = bones[parent].rest * bones[p_bone].rest;
		parent = bones[parent].parent;
	}

	bones[p_bone].parent = -1;
	process_order_dirty = true;

	rest_dirty = true;
	_make_dirty();
}

int Skeleton3D::get_bone_parent(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, -1);
	if (process_order_dirty) {
		_update_process_order();
	}
	return bones[p_bone].parent;
}

Vector<int> Skeleton3D::get_bone_children(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Vector<int>());
	if (process_order_dirty) {
		_update_process_order();
	}
	return bones[p_bone].child_bones;
}

Vector<int> Skeleton3D::get_parentless_bones() const {
	if (process_order_dirty) {
		_update_process_order();
	}
	return parentless_bones;
}

void Skeleton3D::set_bone_rest(int p_bone, const Transform3D &p_rest) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	bones[p_bone].rest = p_rest;
	rest_dirty = true;
	_make_dirty();
	_make_bone_global_pose_subtree_dirty(p_bone);
}
Transform3D Skeleton3D::get_bone_rest(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Transform3D());

	return bones[p_bone].rest;
}
Transform3D Skeleton3D::get_bone_global_rest(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Transform3D());
	if (rest_dirty) {
		_force_update_all_bone_transforms();
	}
	return bones[p_bone].global_rest;
}

void Skeleton3D::set_bone_enabled(int p_bone, bool p_enabled) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	bones[p_bone].enabled = p_enabled;
	emit_signal(SceneStringName(bone_enabled_changed), p_bone);
	_make_dirty();
	_make_bone_global_pose_subtree_dirty(p_bone);
}

bool Skeleton3D::is_bone_enabled(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, false);
	return bones[p_bone].enabled;
}

void Skeleton3D::set_show_rest_only(bool p_enabled) {
	show_rest_only = p_enabled;
	emit_signal(SceneStringName(show_rest_only_changed));
	_make_dirty();
	_make_bone_global_poses_dirty();
}

bool Skeleton3D::is_show_rest_only() const {
	return show_rest_only;
}

void Skeleton3D::clear_bones() {
	bones.clear();
	name_to_bone_index.clear();
	process_order_dirty = true;
	version++;
	_make_dirty();
}

// Posing api

void Skeleton3D::set_bone_pose(int p_bone, const Transform3D &p_pose) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	bones[p_bone].pose_position = p_pose.origin;
	bones[p_bone].pose_rotation = p_pose.basis.get_rotation_quaternion();
	bones[p_bone].pose_scale = p_pose.basis.get_scale();
	bones[p_bone].pose_cache_dirty = true;
	if (is_inside_tree()) {
		_make_dirty();
		_make_bone_global_pose_subtree_dirty(p_bone);
	}
}

void Skeleton3D::set_bone_pose_position(int p_bone, const Vector3 &p_position) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	bones[p_bone].pose_position = p_position;
	bones[p_bone].pose_cache_dirty = true;
	if (is_inside_tree()) {
		_make_dirty();
		_make_bone_global_pose_subtree_dirty(p_bone);
	}
}
void Skeleton3D::set_bone_pose_rotation(int p_bone, const Quaternion &p_rotation) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	bones[p_bone].pose_rotation = p_rotation;
	bones[p_bone].pose_cache_dirty = true;
	if (is_inside_tree()) {
		_make_dirty();
		_make_bone_global_pose_subtree_dirty(p_bone);
	}
}
void Skeleton3D::set_bone_pose_scale(int p_bone, const Vector3 &p_scale) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);

	bones[p_bone].pose_scale = p_scale;
	bones[p_bone].pose_cache_dirty = true;
	if (is_inside_tree()) {
		_make_dirty();
		_make_bone_global_pose_subtree_dirty(p_bone);
	}
}

Vector3 Skeleton3D::get_bone_pose_position(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Vector3());
	return bones[p_bone].pose_position;
}

Quaternion Skeleton3D::get_bone_pose_rotation(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Quaternion());
	return bones[p_bone].pose_rotation;
}

Vector3 Skeleton3D::get_bone_pose_scale(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Vector3());
	return bones[p_bone].pose_scale;
}

void Skeleton3D::reset_bone_pose(int p_bone) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);
	set_bone_pose_position(p_bone, bones[p_bone].rest.origin);
	set_bone_pose_rotation(p_bone, bones[p_bone].rest.basis.get_rotation_quaternion());
	set_bone_pose_scale(p_bone, bones[p_bone].rest.basis.get_scale());
}

void Skeleton3D::reset_bone_poses() {
	for (uint32_t i = 0; i < bones.size(); i++) {
		reset_bone_pose(i);
	}
}

Transform3D Skeleton3D::get_bone_pose(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Transform3D());
	bones[p_bone].update_pose_cache();
	return bones[p_bone].pose_cache;
}

void Skeleton3D::_make_dirty() {
	if (dirty) {
		return;
	}
	dirty = true;
	_update_deferred();
}

void Skeleton3D::_update_deferred(UpdateFlag p_update_flag) {
	if (is_inside_tree()) {
		if (update_flags == UPDATE_FLAG_NONE && !updating) {
			notify_deferred_thread_group(NOTIFICATION_UPDATE_SKELETON); // It must never be called more than once in a single frame.
		}
		update_flags |= p_update_flag;
	}
}

void Skeleton3D::localize_rests() {
	Vector<int> bones_to_process = get_parentless_bones();
	while (bones_to_process.size() > 0) {
		int current_bone_idx = bones_to_process[0];
		bones_to_process.erase(current_bone_idx);

		if (bones[current_bone_idx].parent >= 0) {
			set_bone_rest(current_bone_idx, bones[bones[current_bone_idx].parent].rest.affine_inverse() * bones[current_bone_idx].rest);
		}

		// Add the bone's children to the list of bones to be processed.
		int child_bone_size = bones[current_bone_idx].child_bones.size();
		for (int i = 0; i < child_bone_size; i++) {
			bones_to_process.push_back(bones[current_bone_idx].child_bones[i]);
		}
	}
}

void Skeleton3D::_skin_changed() {
	_make_dirty();
}

Ref<Skin> Skeleton3D::create_skin_from_rest_transforms() {
	Ref<Skin> skin;

	skin.instantiate();
	skin->set_bind_count(bones.size());

	// Pose changed, rebuild cache of inverses.
	const Bone *bonesptr = bones.ptr();
	uint32_t len = bones.size();

	// Calculate global rests and invert them.
	LocalVector<int> bones_to_process;
	bones_to_process = get_parentless_bones();
	while (bones_to_process.size() > 0) {
		int current_bone_idx = bones_to_process[0];
		const Bone &b = bonesptr[current_bone_idx];
		bones_to_process.erase(current_bone_idx);
		LocalVector<int> child_bones_vector;
		child_bones_vector = get_bone_children(current_bone_idx);
		int child_bones_size = child_bones_vector.size();
		if (b.parent < 0) {
			skin->set_bind_pose(current_bone_idx, b.rest);
		}
		for (int i = 0; i < child_bones_size; i++) {
			int child_bone_idx = child_bones_vector[i];
			const Bone &cb = bonesptr[child_bone_idx];
			skin->set_bind_pose(child_bone_idx, skin->get_bind_pose(current_bone_idx) * cb.rest);
			// Add the bone's children to the list of bones to be processed.
			bones_to_process.push_back(child_bones_vector[i]);
		}
	}

	for (uint32_t i = 0; i < len; i++) {
		// The inverse is what is actually required.
		skin->set_bind_bone(i, i);
		skin->set_bind_pose(i, skin->get_bind_pose(i).affine_inverse());
	}

	return skin;
}

Ref<SkinReference> Skeleton3D::register_skin(const Ref<Skin> &p_skin) {
	ERR_FAIL_COND_V(p_skin.is_null(), Ref<SkinReference>());

	for (const SkinReference *E : skin_bindings) {
		if (E->skin == p_skin) {
			return Ref<SkinReference>(E);
		}
	}

	Ref<SkinReference> skin_ref;
	skin_ref.instantiate();

	skin_ref->skeleton_node = this;
	skin_ref->bind_count = 0;
	skin_ref->skeleton = RenderingServer::get_singleton()->skeleton_create();
	skin_ref->skeleton_node = this;
	skin_ref->skin = p_skin;

	skin_bindings.insert(skin_ref.operator->());

	skin_ref->skin->connect_changed(callable_mp(skin_ref.operator->(), &SkinReference::_skin_changed));

	_make_dirty(); // Skin needs to be updated, so update skeleton.

	return skin_ref;
}

void Skeleton3D::force_update_deferred() {
	_make_dirty();
}

void Skeleton3D::force_update_all_dirty_bones() {
	_force_update_all_dirty_bones();
}

void Skeleton3D::_force_update_all_dirty_bones() const {
	if (!dirty) {
		return;
	}
	_force_update_all_bone_transforms();
}

void Skeleton3D::force_update_all_bone_transforms() {
	_force_update_all_bone_transforms();
}

void Skeleton3D::_force_update_all_bone_transforms() const {
	_update_process_order();
	for (int i = 0; i < parentless_bones.size(); i++) {
		_force_update_bone_children_transforms(parentless_bones[i]);
	}
	if (rest_dirty) {
		rest_dirty = false;
		const_cast<Skeleton3D *>(this)->emit_signal(SNAME("rest_updated"));
	} else {
		rest_dirty = false;
	}
	dirty = false;
	if (updating) {
		return;
	}
	const_cast<Skeleton3D *>(this)->emit_signal(SceneStringName(pose_updated));
}

void Skeleton3D::force_update_bone_children_transforms(int p_bone_idx) {
	_force_update_bone_children_transforms(p_bone_idx);
}

void Skeleton3D::_force_update_bone_children_transforms(int p_bone_idx) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone_idx, bone_size);

	Bone *bonesptr = bones.ptr();

	// Loop through nested set.
	for (int offset = 0; offset < bone_size; offset++) {
		if (!bone_global_pose_dirty[offset]) {
			continue;
		}

		int current_bone_idx = nested_set_offset_to_bone_index[offset];
		Bone &b = bonesptr[current_bone_idx];
		bool bone_enabled = b.enabled && !show_rest_only;

		if (bone_enabled) {
			b.update_pose_cache();
			Transform3D pose = b.pose_cache;

			if (b.parent >= 0) {
				b.global_pose = bonesptr[b.parent].global_pose * pose;
			} else {
				b.global_pose = pose;
			}
		} else {
			if (b.parent >= 0) {
				b.global_pose = bonesptr[b.parent].global_pose * b.rest;
			} else {
				b.global_pose = b.rest;
			}
		}
		if (rest_dirty) {
			b.global_rest = b.parent >= 0 ? bonesptr[b.parent].global_rest * b.rest : b.rest;
		}

#ifndef DISABLE_DEPRECATED
		if (bone_enabled) {
			Transform3D pose = b.pose_cache;
			if (b.parent >= 0) {
				b.pose_global_no_override = bonesptr[b.parent].pose_global_no_override * pose;
			} else {
				b.pose_global_no_override = pose;
			}
		} else {
			if (b.parent >= 0) {
				b.pose_global_no_override = bonesptr[b.parent].pose_global_no_override * b.rest;
			} else {
				b.pose_global_no_override = b.rest;
			}
		}
		if (b.global_pose_override_amount >= CMP_EPSILON) {
			b.global_pose = b.global_pose.interpolate_with(b.global_pose_override, b.global_pose_override_amount);
		}
		if (b.global_pose_override_reset) {
			b.global_pose_override_amount = 0.0;
		}
#endif // _DISABLE_DEPRECATED

		bone_global_pose_dirty[offset] = false;
	}
}

void Skeleton3D::_find_modifiers() {
	if (!modifiers_dirty) {
		return;
	}
	modifiers.clear();
	for (int i = 0; i < get_child_count(); i++) {
		SkeletonModifier3D *c = Object::cast_to<SkeletonModifier3D>(get_child(i));
		if (c) {
			modifiers.push_back(c->get_instance_id());
		}
	}
	modifiers_dirty = false;
}

void Skeleton3D::_process_modifiers() {
	for (const ObjectID &oid : modifiers) {
		Object *t_obj = ObjectDB::get_instance(oid);
		if (!t_obj) {
			continue;
		}
		SkeletonModifier3D *mod = cast_to<SkeletonModifier3D>(t_obj);
		if (!mod) {
			continue;
		}
		real_t influence = mod->get_influence();
		if (influence < 1.0) {
			LocalVector<Transform3D> old_poses;
			for (int i = 0; i < get_bone_count(); i++) {
				old_poses.push_back(get_bone_pose(i));
			}
			mod->process_modification();
			LocalVector<Transform3D> new_poses;
			for (int i = 0; i < get_bone_count(); i++) {
				new_poses.push_back(get_bone_pose(i));
			}
			for (int i = 0; i < get_bone_count(); i++) {
				if (old_poses[i] == new_poses[i]) {
					continue; // Avoid unneeded calculation.
				}
				set_bone_pose(i, old_poses[i].interpolate_with(new_poses[i], influence));
			}
		} else {
			mod->process_modification();
		}
		force_update_all_dirty_bones();
	}
}

void Skeleton3D::add_child_notify(Node *p_child) {
	if (Object::cast_to<SkeletonModifier3D>(p_child)) {
		_make_modifiers_dirty();
	}
}

void Skeleton3D::move_child_notify(Node *p_child) {
	if (Object::cast_to<SkeletonModifier3D>(p_child)) {
		_make_modifiers_dirty();
	}
}

void Skeleton3D::remove_child_notify(Node *p_child) {
	if (Object::cast_to<SkeletonModifier3D>(p_child)) {
		_make_modifiers_dirty();
	}
}

void Skeleton3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_bone", "name"), &Skeleton3D::add_bone);
	ClassDB::bind_method(D_METHOD("find_bone", "name"), &Skeleton3D::find_bone);
	ClassDB::bind_method(D_METHOD("get_bone_name", "bone_idx"), &Skeleton3D::get_bone_name);
	ClassDB::bind_method(D_METHOD("set_bone_name", "bone_idx", "name"), &Skeleton3D::set_bone_name);

	ClassDB::bind_method(D_METHOD("get_bone_meta", "bone_idx", "key"), &Skeleton3D::get_bone_meta);
	ClassDB::bind_method(D_METHOD("get_bone_meta_list", "bone_idx"), &Skeleton3D::_get_bone_meta_list_bind);
	ClassDB::bind_method(D_METHOD("has_bone_meta", "bone_idx", "key"), &Skeleton3D::has_bone_meta);
	ClassDB::bind_method(D_METHOD("set_bone_meta", "bone_idx", "key", "value"), &Skeleton3D::set_bone_meta);

	ClassDB::bind_method(D_METHOD("get_concatenated_bone_names"), &Skeleton3D::get_concatenated_bone_names);

	ClassDB::bind_method(D_METHOD("get_bone_parent", "bone_idx"), &Skeleton3D::get_bone_parent);
	ClassDB::bind_method(D_METHOD("set_bone_parent", "bone_idx", "parent_idx"), &Skeleton3D::set_bone_parent);

	ClassDB::bind_method(D_METHOD("get_bone_count"), &Skeleton3D::get_bone_count);
	ClassDB::bind_method(D_METHOD("get_version"), &Skeleton3D::get_version);

	ClassDB::bind_method(D_METHOD("unparent_bone_and_rest", "bone_idx"), &Skeleton3D::unparent_bone_and_rest);

	ClassDB::bind_method(D_METHOD("get_bone_children", "bone_idx"), &Skeleton3D::get_bone_children);

	ClassDB::bind_method(D_METHOD("get_parentless_bones"), &Skeleton3D::get_parentless_bones);

	ClassDB::bind_method(D_METHOD("get_bone_rest", "bone_idx"), &Skeleton3D::get_bone_rest);
	ClassDB::bind_method(D_METHOD("set_bone_rest", "bone_idx", "rest"), &Skeleton3D::set_bone_rest);
	ClassDB::bind_method(D_METHOD("get_bone_global_rest", "bone_idx"), &Skeleton3D::get_bone_global_rest);

	ClassDB::bind_method(D_METHOD("create_skin_from_rest_transforms"), &Skeleton3D::create_skin_from_rest_transforms);
	ClassDB::bind_method(D_METHOD("register_skin", "skin"), &Skeleton3D::register_skin);

	ClassDB::bind_method(D_METHOD("localize_rests"), &Skeleton3D::localize_rests);

	ClassDB::bind_method(D_METHOD("clear_bones"), &Skeleton3D::clear_bones);

	ClassDB::bind_method(D_METHOD("get_bone_pose", "bone_idx"), &Skeleton3D::get_bone_pose);
	ClassDB::bind_method(D_METHOD("set_bone_pose", "bone_idx", "pose"), &Skeleton3D::set_bone_pose);
	ClassDB::bind_method(D_METHOD("set_bone_pose_position", "bone_idx", "position"), &Skeleton3D::set_bone_pose_position);
	ClassDB::bind_method(D_METHOD("set_bone_pose_rotation", "bone_idx", "rotation"), &Skeleton3D::set_bone_pose_rotation);
	ClassDB::bind_method(D_METHOD("set_bone_pose_scale", "bone_idx", "scale"), &Skeleton3D::set_bone_pose_scale);

	ClassDB::bind_method(D_METHOD("get_bone_pose_position", "bone_idx"), &Skeleton3D::get_bone_pose_position);
	ClassDB::bind_method(D_METHOD("get_bone_pose_rotation", "bone_idx"), &Skeleton3D::get_bone_pose_rotation);
	ClassDB::bind_method(D_METHOD("get_bone_pose_scale", "bone_idx"), &Skeleton3D::get_bone_pose_scale);

	ClassDB::bind_method(D_METHOD("reset_bone_pose", "bone_idx"), &Skeleton3D::reset_bone_pose);
	ClassDB::bind_method(D_METHOD("reset_bone_poses"), &Skeleton3D::reset_bone_poses);

	ClassDB::bind_method(D_METHOD("is_bone_enabled", "bone_idx"), &Skeleton3D::is_bone_enabled);
	ClassDB::bind_method(D_METHOD("set_bone_enabled", "bone_idx", "enabled"), &Skeleton3D::set_bone_enabled, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("get_bone_global_pose", "bone_idx"), &Skeleton3D::get_bone_global_pose);
	ClassDB::bind_method(D_METHOD("set_bone_global_pose", "bone_idx", "pose"), &Skeleton3D::set_bone_global_pose);

	ClassDB::bind_method(D_METHOD("force_update_all_bone_transforms"), &Skeleton3D::force_update_all_bone_transforms);
	ClassDB::bind_method(D_METHOD("force_update_bone_child_transform", "bone_idx"), &Skeleton3D::force_update_bone_children_transforms);

	ClassDB::bind_method(D_METHOD("set_motion_scale", "motion_scale"), &Skeleton3D::set_motion_scale);
	ClassDB::bind_method(D_METHOD("get_motion_scale"), &Skeleton3D::get_motion_scale);

	ClassDB::bind_method(D_METHOD("set_show_rest_only", "enabled"), &Skeleton3D::set_show_rest_only);
	ClassDB::bind_method(D_METHOD("is_show_rest_only"), &Skeleton3D::is_show_rest_only);

	ClassDB::bind_method(D_METHOD("set_modifier_callback_mode_process", "mode"), &Skeleton3D::set_modifier_callback_mode_process);
	ClassDB::bind_method(D_METHOD("get_modifier_callback_mode_process"), &Skeleton3D::get_modifier_callback_mode_process);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "motion_scale", PROPERTY_HINT_RANGE, "0.001,10,0.001,or_greater"), "set_motion_scale", "get_motion_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_rest_only"), "set_show_rest_only", "is_show_rest_only");

	ADD_GROUP("Modifier", "modifier_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "modifier_callback_mode_process", PROPERTY_HINT_ENUM, "Physics,Idle"), "set_modifier_callback_mode_process", "get_modifier_callback_mode_process");

	ADD_SIGNAL(MethodInfo("rest_updated"));
	ADD_SIGNAL(MethodInfo("pose_updated"));
	ADD_SIGNAL(MethodInfo("skeleton_updated"));
	ADD_SIGNAL(MethodInfo("bone_enabled_changed", PropertyInfo(Variant::INT, "bone_idx")));
	ADD_SIGNAL(MethodInfo("bone_list_changed"));
	ADD_SIGNAL(MethodInfo("show_rest_only_changed"));

	BIND_CONSTANT(NOTIFICATION_UPDATE_SKELETON);
	BIND_ENUM_CONSTANT(MODIFIER_CALLBACK_MODE_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(MODIFIER_CALLBACK_MODE_PROCESS_IDLE);

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("clear_bones_global_pose_override"), &Skeleton3D::clear_bones_global_pose_override);
	ClassDB::bind_method(D_METHOD("set_bone_global_pose_override", "bone_idx", "pose", "amount", "persistent"), &Skeleton3D::set_bone_global_pose_override, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_bone_global_pose_override", "bone_idx"), &Skeleton3D::get_bone_global_pose_override);
	ClassDB::bind_method(D_METHOD("get_bone_global_pose_no_override", "bone_idx"), &Skeleton3D::get_bone_global_pose_no_override);

	ClassDB::bind_method(D_METHOD("set_animate_physical_bones", "enabled"), &Skeleton3D::set_animate_physical_bones);
	ClassDB::bind_method(D_METHOD("get_animate_physical_bones"), &Skeleton3D::get_animate_physical_bones);
	ClassDB::bind_method(D_METHOD("physical_bones_stop_simulation"), &Skeleton3D::physical_bones_stop_simulation);
	ClassDB::bind_method(D_METHOD("physical_bones_start_simulation", "bones"), &Skeleton3D::physical_bones_start_simulation_on, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("physical_bones_add_collision_exception", "exception"), &Skeleton3D::physical_bones_add_collision_exception);
	ClassDB::bind_method(D_METHOD("physical_bones_remove_collision_exception", "exception"), &Skeleton3D::physical_bones_remove_collision_exception);

	ADD_GROUP("Deprecated", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "animate_physical_bones"), "set_animate_physical_bones", "get_animate_physical_bones");
#endif // _DISABLE_DEPRECATED
}

#ifndef DISABLE_DEPRECATED
void Skeleton3D::clear_bones_global_pose_override() {
	for (uint32_t i = 0; i < bones.size(); i += 1) {
		bones[i].global_pose_override_amount = 0;
		bones[i].global_pose_override_reset = true;
	}
	_make_dirty();
	_make_bone_global_poses_dirty();
}

void Skeleton3D::set_bone_global_pose_override(int p_bone, const Transform3D &p_pose, real_t p_amount, bool p_persistent) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(p_bone, bone_size);
	bones[p_bone].global_pose_override_amount = p_amount;
	bones[p_bone].global_pose_override = p_pose;
	bones[p_bone].global_pose_override_reset = !p_persistent;
	_make_dirty();
	_make_bone_global_pose_subtree_dirty(p_bone);
}

Transform3D Skeleton3D::get_bone_global_pose_override(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Transform3D());
	return bones[p_bone].global_pose_override;
}

Transform3D Skeleton3D::get_bone_global_pose_no_override(int p_bone) const {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX_V(p_bone, bone_size, Transform3D());
	_force_update_all_dirty_bones();
	return bones[p_bone].pose_global_no_override;
}

Node *Skeleton3D::get_simulator() {
	return simulator;
}

void Skeleton3D::set_animate_physical_bones(bool p_enabled) {
	animate_physical_bones = p_enabled;
	PhysicalBoneSimulator3D *sim = cast_to<PhysicalBoneSimulator3D>(simulator);
	if (!sim) {
		return;
	}
	sim->set_active(animate_physical_bones || sim->is_simulating_physics());
}

bool Skeleton3D::get_animate_physical_bones() const {
	return animate_physical_bones;
}

void Skeleton3D::physical_bones_stop_simulation() {
	PhysicalBoneSimulator3D *sim = cast_to<PhysicalBoneSimulator3D>(simulator);
	if (!sim) {
		return;
	}
	sim->physical_bones_stop_simulation();
	sim->set_active(animate_physical_bones || sim->is_simulating_physics());
}

void Skeleton3D::physical_bones_start_simulation_on(const TypedArray<StringName> &p_bones) {
	PhysicalBoneSimulator3D *sim = cast_to<PhysicalBoneSimulator3D>(simulator);
	if (!sim) {
		return;
	}
	sim->set_active(true);
	sim->physical_bones_start_simulation_on(p_bones);
}

void Skeleton3D::physical_bones_add_collision_exception(RID p_exception) {
	PhysicalBoneSimulator3D *sim = cast_to<PhysicalBoneSimulator3D>(simulator);
	if (!sim) {
		return;
	}
	sim->physical_bones_add_collision_exception(p_exception);
}

void Skeleton3D::physical_bones_remove_collision_exception(RID p_exception) {
	PhysicalBoneSimulator3D *sim = cast_to<PhysicalBoneSimulator3D>(simulator);
	if (!sim) {
		return;
	}
	sim->physical_bones_remove_collision_exception(p_exception);
}
#endif // _DISABLE_DEPRECATED

Skeleton3D::Skeleton3D() {
}

Skeleton3D::~Skeleton3D() {
	// Some skins may remain bound.
	for (SkinReference *E : skin_bindings) {
		E->skeleton_node = nullptr;
	}
}
