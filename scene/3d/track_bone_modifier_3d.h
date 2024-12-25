/**************************************************************************/
/*  track_bone_modifier_3d.h                                              */
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

#ifndef TRACK_BONE_MODIFIER_3D_H
#define TRACK_BONE_MODIFIER_3D_H

#include "scene/3d/bone_constraint_3d.h"
#include "scene/3d/look_at_modifier_3d.h"

class TrackBoneModifier3D : public BoneConstraint3D {
	GDCLASS(TrackBoneModifier3D, BoneConstraint3D);

public:
	struct TrackBone3DSetting : public BoneConstraint3DSetting {
		BoneAxis forward_axis = BONE_AXIS_PLUS_Y;
		bool use_euler = false;
		Vector3::Axis primary_rotation_axis = Vector3::AXIS_X;
		bool use_secondary_rotation = true;
	};

protected:
	bool _get(const StringName &p_path, Variant &r_ret) const;
	bool _set(const StringName &p_path, const Variant &p_value);
	virtual PackedStringArray get_configuration_warnings() const override;
	void _validate_property(PropertyInfo &p_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

	virtual void _process_constraint(int p_index, Skeleton3D *p_skeleton, int p_apply_bone, int p_target_bone, float p_amount) override;
	virtual void _validate_setting(int p_index) override;

public:
	void set_forward_axis(int p_index, BoneAxis p_axis);
	BoneAxis get_forward_axis(int p_index) const;
	void set_use_euler(int p_index, bool p_enabled);
	bool is_using_euler(int p_index) const;
	void set_primary_rotation_axis(int p_index, Vector3::Axis p_axis);
	Vector3::Axis get_primary_rotation_axis(int p_index) const;
	void set_use_secondary_rotation(int p_index, bool p_enabled);
	bool is_using_secondary_rotation(int p_index) const;
};

#endif // TRACK_BONE_MODIFIER_3D_H
