/**************************************************************************/
/*  bone_constraint_3d.h                                                  */
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

#ifndef BONE_CONSTRAINT_3D_H
#define BONE_CONSTRAINT_3D_H

#include "scene/3d/skeleton_modifier_3d.h"

class BoneConstraint3D : public SkeletonModifier3D {
	GDCLASS(BoneConstraint3D, SkeletonModifier3D);

public:
	struct BoneConstraint3DSetting {
		float amount = 1.0;

		String apply_bone_name;
		int apply_bone = -1;

		String target_bone_name;
		int target_bone = -1;
	};

protected:
	Vector<BoneConstraint3DSetting *> settings;

	bool _get(const StringName &p_path, Variant &r_ret) const;
	bool _set(const StringName &p_path, const Variant &p_value);

	// Define get_property_list() instead of _get_property_list()
	// to merge child class properties into parent class array inspector.
	void get_property_list(List<PropertyInfo> *p_list) const; // Will be called by child classes.

	static void _bind_methods();

	virtual void _process_modification() override;

	virtual void _process_constraint(int p_index, Skeleton3D *p_skeleton, int p_apply_bone, int p_target_bone, float p_amount);
	virtual void _validate_setting(int p_index);

public:
	void set_amount(int p_index, float p_amount);
	float get_amount(int p_index) const;

	void set_apply_bone_name(int p_index, const String &p_bone_name);
	String get_apply_bone_name(int p_index) const;
	void set_apply_bone(int p_index, int p_bone);
	int get_apply_bone(int p_index) const;

	void set_target_bone_name(int p_index, const String &p_bone_name);
	String get_target_bone_name(int p_index) const;
	void set_target_bone(int p_index, int p_bone);
	int get_target_bone(int p_index) const;

	void set_setting_count(int p_count);
	int get_setting_count() const;

	void clear_settings();
};

#endif // BONE_CONSTRAINT_3D_H
