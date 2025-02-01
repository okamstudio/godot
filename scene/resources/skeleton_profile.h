/**************************************************************************/
/*  skeleton_profile.h                                                    */
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

#pragma once

#include "texture.h"

class SkeletonProfile : public Resource {
	GDCLASS(SkeletonProfile, Resource);

public:
	enum TailDirection {
		TAIL_DIRECTION_AVERAGE_CHILDREN,
		TAIL_DIRECTION_SPECIFIC_CHILD,
		TAIL_DIRECTION_END
	};

protected:
	// Note: SkeletonProfileHumanoid which extends SkeletonProfile exists to unify standard bone names.
	// That is what is_read_only is for, so don't make it public.
	bool is_read_only = false;

	struct SkeletonProfileGroup {
		StringName group_name;
		Ref<Texture2D> texture;
	};

	struct SkeletonProfileBone {
		StringName bone_name;
		StringName bone_parent;
		TailDirection tail_direction = TAIL_DIRECTION_AVERAGE_CHILDREN;
		StringName bone_tail;
		Transform3D reference_pose;
		Vector2 handle_offset;
		StringName group;
		bool required = false;
	};

	StringName root_bone;
	StringName scale_base_bone;

	Vector<SkeletonProfileGroup> groups;
	Vector<SkeletonProfileBone> bones;

	bool _get(const StringName &p_path, Variant &r_ret) const;
	bool _set(const StringName &p_path, const Variant &p_value);
	void _validate_property(PropertyInfo &p_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	StringName get_root_bone();
	void set_root_bone(const StringName &p_bone_name);

	StringName get_scale_base_bone();
	void set_scale_base_bone(const StringName &p_bone_name);

	int get_group_size();
	void set_group_size(int p_size);

	StringName get_group_name(int p_group_idx) const;
	void set_group_name(int p_group_idx, const StringName &p_group_name);

	Ref<Texture2D> get_texture(int p_group_idx) const;
	void set_texture(int p_group_idx, const Ref<Texture2D> &p_texture);

	int get_bone_size();
	void set_bone_size(int p_size);

	int find_bone(const StringName &p_bone_name) const;

	PackedStringArray get_bone_names();
	StringName get_bone_name(int p_bone_idx) const;
	void set_bone_name(int p_bone_idx, const StringName &p_bone_name);

	StringName get_bone_parent(int p_bone_idx) const;
	void set_bone_parent(int p_bone_idx, const StringName &p_bone_parent);

	TailDirection get_tail_direction(int p_bone_idx) const;
	void set_tail_direction(int p_bone_idx, TailDirection p_tail_direction);

	StringName get_bone_tail(int p_bone_idx) const;
	void set_bone_tail(int p_bone_idx, const StringName &p_bone_tail);

	Transform3D get_reference_pose(int p_bone_idx) const;
	void set_reference_pose(int p_bone_idx, const Transform3D &p_reference_pose);

	Vector2 get_handle_offset(int p_bone_idx) const;
	void set_handle_offset(int p_bone_idx, const Vector2 &p_handle_offset);

	StringName get_group(int p_bone_idx) const;
	void set_group(int p_bone_idx, const StringName &p_group);

	bool is_required(int p_bone_idx) const;
	void set_required(int p_bone_idx, bool p_required);

	bool has_bone(const StringName &p_bone_name);

	SkeletonProfile();
	~SkeletonProfile();
};

class SkeletonProfileHumanoid : public SkeletonProfile {
	GDCLASS(SkeletonProfileHumanoid, SkeletonProfile);

public:
	SkeletonProfileHumanoid();
	~SkeletonProfileHumanoid();
};

VARIANT_ENUM_CAST(SkeletonProfile::TailDirection);
