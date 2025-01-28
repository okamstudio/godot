/**************************************************************************/
/*  retarget_modifier_3d.h                                                */
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

#ifndef RETARGET_MODIFIER_3D_H
#define RETARGET_MODIFIER_3D_H

#include "scene/3d/skeleton_modifier_3d.h"
#include "scene/resources/skeleton_profile.h"

class RetargetModifier3D : public SkeletonModifier3D {
	GDCLASS(RetargetModifier3D, SkeletonModifier3D);

public:
	enum TransformFlag {
		TRANSFORM_FLAG_POSITION = 1,
		TRANSFORM_FLAG_ROTATION = 2,
		TRANSFORM_FLAG_SCALE = 4,
		TRANSFORM_FLAG_ALL = TRANSFORM_FLAG_POSITION | TRANSFORM_FLAG_ROTATION | TRANSFORM_FLAG_SCALE,
	};

private:
	Ref<SkeletonProfile> profile;

	bool use_global_pose = false;
	BitField<TransformFlag> enable_flags = TRANSFORM_FLAG_ALL;

	struct RetargetBoneInfo {
		int bone_id = -1;
		Basis pre_basis;
		Basis post_basis;
	};

	struct RetargetInfo {
		ObjectID skeleton_id;
		Vector<RetargetBoneInfo> humanoid_bone_rests;
	};

	Vector<RetargetInfo> child_skeletons;
	Vector<int> source_bone_ids;

	void _update_child_skeleton_rests(int p_child_skeleton_idx);
	void _update_child_skeletons();
	void _reset_child_skeleton_poses();
	void _reset_child_skeletons();

#ifdef TOOLS_ENABLED
	void _force_update_child_skeletons();
#endif // TOOLS_ENABLED

	void cache_rests_with_reset();
	void cache_rests();
	Vector<RetargetBoneInfo> cache_bone_global_rests(Skeleton3D *p_skeleton);
	Vector<RetargetBoneInfo> cache_bone_rests(Skeleton3D *p_skeleton);
	Vector<RetargetBoneInfo> get_humanoid_bone_rests(Skeleton3D *p_skeleton);

	void _retarget_global_pose();
	void _retarget_pose();

protected:
	virtual void _skeleton_changed(Skeleton3D *p_old, Skeleton3D *p_new) override;
	void _profile_changed(Ref<SkeletonProfile> p_old, Ref<SkeletonProfile> p_new);

	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();
	virtual void _notification(int p_what);

	virtual void add_child_notify(Node *p_child) override;
	virtual void move_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;

	virtual void _set_active(bool p_active) override;
	virtual void _process_modification() override;

public:
	virtual PackedStringArray get_configuration_warnings() const override;

	void set_use_global_pose(bool p_use_global_pose);
	bool is_using_global_pose() const;
	void set_enable_flags(BitField<TransformFlag> p_enable_flags);
	BitField<TransformFlag> get_enable_flags() const;

	void set_position_enabled(bool p_enabled);
	bool is_position_enabled() const;
	void set_rotation_enabled(bool p_enabled);
	bool is_rotation_enabled() const;
	void set_scale_enabled(bool p_enabled);
	bool is_scale_enabled() const;

	void set_profile(Ref<SkeletonProfile> p_profile);
	Ref<SkeletonProfile> get_profile() const;

	RetargetModifier3D();
	virtual ~RetargetModifier3D();
};

VARIANT_BITFIELD_CAST(RetargetModifier3D::TransformFlag);

#endif // RETARGET_MODIFIER_3D_H
