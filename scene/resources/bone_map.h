/**************************************************************************/
/*  bone_map.h                                                            */
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

#include "skeleton_profile.h"

class BoneMap : public Resource {
	GDCLASS(BoneMap, Resource);

	Ref<SkeletonProfile> profile;
	HashMap<StringName, StringName> bone_map;

	void _update_profile();
	void _validate_bone_map();

protected:
	bool _get(const StringName &p_path, Variant &r_ret) const;
	bool _set(const StringName &p_path, const Variant &p_value);
	void _validate_property(PropertyInfo &p_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	Ref<SkeletonProfile> get_profile() const;
	void set_profile(const Ref<SkeletonProfile> &p_profile);

	int get_skeleton_bone_name_count(const StringName &p_skeleton_bone_name) const;

	StringName get_skeleton_bone_name(const StringName &p_profile_bone_name) const;
	void set_skeleton_bone_name(const StringName &p_profile_bone_name, const StringName &p_skeleton_bone_name);
	void _set_skeleton_bone_name(const StringName &p_profile_bone_name, const StringName &p_skeleton_bone_name); // Avoid to emit signal for editor.

	StringName find_profile_bone_name(const StringName &p_skeleton_bone_name) const;

	BoneMap();
	~BoneMap();
};
