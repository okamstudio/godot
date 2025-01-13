/**************************************************************************/
/*  skeleton_3d.h                                                         */
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

#ifndef SKELETON_3D_H
#define SKELETON_3D_H

#include "core/templates/a_hash_map.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/3d/skin.h"

typedef int BoneId;

class Skeleton3D;
class SkeletonModifier3D;

class SkinReference : public RefCounted {
	GDCLASS(SkinReference, RefCounted)
	friend class Skeleton3D;

	Skeleton3D *skeleton_node = nullptr;
	RID skeleton;
	Ref<Skin> skin;
	uint32_t bind_count = 0;
	uint64_t skeleton_version = 0;
	Vector<uint32_t> skin_bone_indices;
	uint32_t *skin_bone_indices_ptrs = nullptr;

protected:
	static void _bind_methods();

public:
	// Public for use with callable_mp.
	void _skin_changed();

	RID get_skeleton() const;
	Ref<Skin> get_skin() const;
	~SkinReference();
};

class Skeleton3D : public Node3D {
	GDCLASS(Skeleton3D, Node3D);

#ifndef DISABLE_DEPRECATED
	bool animate_physical_bones = true;
	Node *simulator = nullptr;
	void setup_simulator();
#endif // _DISABLE_DEPRECATED

public:
	enum ModifierCallbackModeProcess {
		MODIFIER_CALLBACK_MODE_PROCESS_PHYSICS,
		MODIFIER_CALLBACK_MODE_PROCESS_IDLE,
	};

private:
	friend class SkinReference;

	enum UpdateFlag {
		UPDATE_FLAG_NONE = 1,
		UPDATE_FLAG_MODIFIER = 2,
		UPDATE_FLAG_POSE = 4,
	};

	void _update_deferred(UpdateFlag p_update_flag = UPDATE_FLAG_POSE);
	uint8_t update_flags = UPDATE_FLAG_NONE;
	bool updating = false; // Is updating now?

	struct Bone {
		String name;

		int parent = -1;
		Vector<int> child_bones;

		Transform3D rest;
		Transform3D global_rest;

		bool enabled = true;
		bool pose_cache_dirty = true;
		Transform3D pose_cache;
		Vector3 pose_position;
		Quaternion pose_rotation;
		Vector3 pose_scale = Vector3(1, 1, 1);
		Transform3D global_pose;
		int nested_set_offset = 0; // Offset in nested set of bone hierarchy.
		int nested_set_span = 0; // Subtree span in nested set of bone hierarchy.

		void update_pose_cache() {
			if (pose_cache_dirty) {
				pose_cache.basis.set_quaternion_scale(pose_rotation, pose_scale);
				pose_cache.origin = pose_position;
				pose_cache_dirty = false;
			}
		}

		HashMap<StringName, Variant> metadata;

#ifndef DISABLE_DEPRECATED
		Transform3D pose_global_no_override;
		real_t global_pose_override_amount = 0.0;
		bool global_pose_override_reset = false;
		Transform3D global_pose_override;
#endif // _DISABLE_DEPRECATED
	};

	struct BonePoseBackup {
		Transform3D pose_cache;
		Vector3 pose_position;
		Quaternion pose_rotation;
		Vector3 pose_scale = Vector3(1, 1, 1);
		Transform3D global_pose;

		void save(const Bone &p_bone) {
			pose_cache = p_bone.pose_cache;
			pose_position = p_bone.pose_position;
			pose_rotation = p_bone.pose_rotation;
			pose_scale = p_bone.pose_scale;
			global_pose = p_bone.global_pose;
		}

		void restore(Bone &r_bone) {
			r_bone.pose_cache = pose_cache;
			r_bone.pose_position = pose_position;
			r_bone.pose_rotation = pose_rotation;
			r_bone.pose_scale = pose_scale;
			r_bone.global_pose = global_pose;
		}
	};

	HashSet<SkinReference *> skin_bindings;
	void _skin_changed();

	mutable LocalVector<Bone> bones;
	mutable bool process_order_dirty = false;

	mutable Vector<int> parentless_bones;
	AHashMap<String, int> name_to_bone_index;

	mutable StringName concatenated_bone_names;
	void _update_bone_names() const;

	void _make_dirty();
	mutable bool dirty = false;
	mutable bool rest_dirty = false;

	bool show_rest_only = false;
	float motion_scale = 1.0;

	uint64_t version = 1;

	void _update_process_order() const;

	// To process modifiers.
	ModifierCallbackModeProcess modifier_callback_mode_process = MODIFIER_CALLBACK_MODE_PROCESS_IDLE;
	LocalVector<ObjectID> modifiers;
	bool modifiers_dirty = false;
	void _find_modifiers();
	void _process_modifiers();
	void _process_changed();
	void _make_modifiers_dirty();
	mutable LocalVector<BonePoseBackup> bones_backup;

	// Global bone pose calculation.
	mutable LocalVector<int> nested_set_offset_to_bone_index; // Map from Bone::nested_set_offset to bone index.
	mutable LocalVector<bool> bone_global_pose_dirty; // Indexable with Bone::nested_set_offset.
	void _update_bones_nested_set() const;
	int _update_bone_nested_set(int p_bone, int p_offset) const;
	void _make_bone_global_poses_dirty() const;
	void _make_bone_global_pose_subtree_dirty(int p_bone) const;
	void _update_bone_global_pose(int p_bone) const;

#ifndef DISABLE_DEPRECATED
	void _add_bone_bind_compat_88791(const String &p_name);

	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED

protected:
	bool _get(const StringName &p_path, Variant &r_ret) const;
	bool _set(const StringName &p_path, const Variant &p_value);
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _validate_property(PropertyInfo &p_property) const;
	void _notification(int p_what);
	TypedArray<StringName> _get_bone_meta_list_bind(int p_bone) const;
	static void _bind_methods();

	virtual void add_child_notify(Node *p_child) override;
	virtual void move_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;

public:
	enum {
		NOTIFICATION_UPDATE_SKELETON = 50
	};

	// Skeleton creation API
	uint64_t get_version() const;
	int add_bone(const String &p_name);
	void remove_bone(int p_bone);
	int find_bone(const String &p_name) const;
	String get_bone_name(int p_bone) const;
	void set_bone_name(int p_bone, const String &p_name);
	StringName get_concatenated_bone_names() const;

	bool is_bone_parent_of(int p_bone_id, int p_parent_bone_id) const;

	void set_bone_parent(int p_bone, int p_parent);
	int get_bone_parent(int p_bone) const;

	void unparent_bone_and_rest(int p_bone);

	Vector<int> get_bone_children(int p_bone) const;
	Vector<int> get_parentless_bones() const;

	int get_bone_count() const;

	void set_bone_rest(int p_bone, const Transform3D &p_rest);
	Transform3D get_bone_rest(int p_bone) const;
	Transform3D get_bone_global_rest(int p_bone) const;

	void set_bone_enabled(int p_bone, bool p_enabled);
	bool is_bone_enabled(int p_bone) const;

	void set_show_rest_only(bool p_enabled);
	bool is_show_rest_only() const;
	void clear_bones();

	void set_motion_scale(float p_motion_scale);
	float get_motion_scale() const;

	// bone metadata
	Variant get_bone_meta(int p_bone, const StringName &p_key) const;
	void get_bone_meta_list(int p_bone, List<StringName> *p_list) const;
	bool has_bone_meta(int p_bone, const StringName &p_key) const;
	void set_bone_meta(int p_bone, const StringName &p_key, const Variant &p_value);

	// Posing API
	Transform3D get_bone_pose(int p_bone) const;
	Vector3 get_bone_pose_position(int p_bone) const;
	Quaternion get_bone_pose_rotation(int p_bone) const;
	Vector3 get_bone_pose_scale(int p_bone) const;
	void set_bone_pose(int p_bone, const Transform3D &p_pose);
	void set_bone_pose_position(int p_bone, const Vector3 &p_position);
	void set_bone_pose_rotation(int p_bone, const Quaternion &p_rotation);
	void set_bone_pose_scale(int p_bone, const Vector3 &p_scale);

	Transform3D get_bone_global_pose(int p_bone) const;
	void set_bone_global_pose(int p_bone, const Transform3D &p_pose);

	void reset_bone_pose(int p_bone);
	void reset_bone_poses();

	void localize_rests(); // Used for loaders and tools.

	Ref<Skin> create_skin_from_rest_transforms();

	Ref<SkinReference> register_skin(const Ref<Skin> &p_skin);

	void force_update_all_dirty_bones();
	void _force_update_all_dirty_bones() const;
	void force_update_all_bone_transforms();
	void _force_update_all_bone_transforms() const;
	void force_update_bone_children_transforms(int bone_idx);
	void _force_update_bone_children_transforms(int bone_idx) const;
	void force_update_deferred();

	void set_modifier_callback_mode_process(ModifierCallbackModeProcess p_mode);
	ModifierCallbackModeProcess get_modifier_callback_mode_process() const;

#ifndef DISABLE_DEPRECATED
	Transform3D get_bone_global_pose_no_override(int p_bone) const;
	void clear_bones_global_pose_override();
	Transform3D get_bone_global_pose_override(int p_bone) const;
	void set_bone_global_pose_override(int p_bone, const Transform3D &p_pose, real_t p_amount, bool p_persistent = false);

	Node *get_simulator();
	void set_animate_physical_bones(bool p_enabled);
	bool get_animate_physical_bones() const;
	void physical_bones_stop_simulation();
	void physical_bones_start_simulation_on(const TypedArray<StringName> &p_bones);
	void physical_bones_add_collision_exception(RID p_exception);
	void physical_bones_remove_collision_exception(RID p_exception);
#endif // _DISABLE_DEPRECATED

public:
	Skeleton3D();
	~Skeleton3D();
};

VARIANT_ENUM_CAST(Skeleton3D::ModifierCallbackModeProcess);

#endif // SKELETON_3D_H
