/**************************************************************************/
/*  bone_map.cpp                                                          */
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

#include "bone_map.h"

bool BoneMap::_set(const StringName &p_path, const Variant &p_value) {
	String path = p_path;
	if (path.begins_with("bone_map/")) {
		String which = path.get_slicec('/', 1);
		set_skeleton_bone_name(which, p_value);
		return true;
	}
	return false;
}

bool BoneMap::_get(const StringName &p_path, Variant &r_ret) const {
	String path = p_path;
	if (path.begins_with("bone_map/")) {
		String which = path.get_slicec('/', 1);
		r_ret = get_skeleton_bone_name(which);
		return true;
	}
	return false;
}

void BoneMap::_get_property_list(List<PropertyInfo> *p_list) const {
	HashMap<StringName, StringName>::ConstIterator E = bone_map.begin();
	while (E) {
		p_list->push_back(PropertyInfo(Variant::STRING_NAME, "bone_map/" + E->key, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		++E;
	}
}

Ref<SkeletonProfile> BoneMap::get_profile() const {
	return profile;
}

void BoneMap::set_profile(const Ref<SkeletonProfile> &p_profile) {
	bool is_changed = profile != p_profile;
	if (is_changed) {
		if (profile.is_valid() && profile->is_connected("profile_updated", callable_mp(this, &BoneMap::_update_profile))) {
			profile->disconnect("profile_updated", callable_mp(this, &BoneMap::_update_profile));
		}
		profile = p_profile;
		if (profile.is_valid()) {
			profile->connect("profile_updated", callable_mp(this, &BoneMap::_update_profile));
		}
		_update_profile();
	}
	notify_property_list_changed();
}

StringName BoneMap::get_skeleton_bone_name(const StringName &p_profile_bone_name) const {
	ERR_FAIL_COND_V(!bone_map.has(p_profile_bone_name), StringName());
	return bone_map.get(p_profile_bone_name);
}

void BoneMap::_set_skeleton_bone_name(const StringName &p_profile_bone_name, const StringName &p_skeleton_bone_name) {
	ERR_FAIL_COND(!bone_map.has(p_profile_bone_name));
	bone_map.insert(p_profile_bone_name, p_skeleton_bone_name);
}

void BoneMap::set_skeleton_bone_name(const StringName &p_profile_bone_name, const StringName &p_skeleton_bone_name) {
	_set_skeleton_bone_name(p_profile_bone_name, p_skeleton_bone_name);
	emit_signal("bone_map_updated");
}

StringName BoneMap::find_profile_bone_name(const StringName &p_skeleton_bone_name) const {
	StringName profile_bone_name;
	HashMap<StringName, StringName>::ConstIterator E = bone_map.begin();
	while (E) {
		if (E->value == p_skeleton_bone_name) {
			profile_bone_name = E->key;
			break;
		}
		++E;
	}
	return profile_bone_name;
}

int BoneMap::get_skeleton_bone_name_count(const StringName &p_skeleton_bone_name) const {
	int count = 0;
	HashMap<StringName, StringName>::ConstIterator E = bone_map.begin();
	while (E) {
		if (E->value == p_skeleton_bone_name) {
			++count;
		}
		++E;
	}
	return count;
}

void BoneMap::_update_profile() {
	_validate_bone_map();
	emit_signal("profile_updated");
}

void BoneMap::_validate_bone_map() {
	Ref<SkeletonProfile> current_profile = get_profile();
	if (current_profile.is_valid()) {
		// Insert missing profile bones into bone map.
		int len = current_profile->get_bone_size();
		StringName profile_bone_name;
		for (int i = 0; i < len; i++) {
			profile_bone_name = current_profile->get_bone_name(i);
			if (!bone_map.has(profile_bone_name)) {
				bone_map.insert(profile_bone_name, StringName());
			}
		}
		// Remove bones that do not exist in the profile from the map.
		Vector<StringName> delete_bones;
		StringName k;
		HashMap<StringName, StringName>::ConstIterator E = bone_map.begin();
		while (E) {
			k = E->key;
			if (!current_profile->has_bone(k)) {
				delete_bones.push_back(k);
			}
			++E;
		}
		len = delete_bones.size();
		for (int i = 0; i < len; i++) {
			bone_map.erase(delete_bones[i]);
		}
	} else {
		bone_map.clear();
	}
}

void BoneMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_profile"), &BoneMap::get_profile);
	ClassDB::bind_method(D_METHOD("set_profile", "profile"), &BoneMap::set_profile);

	ClassDB::bind_method(D_METHOD("get_skeleton_bone_name", "profile_bone_name"), &BoneMap::get_skeleton_bone_name);
	ClassDB::bind_method(D_METHOD("set_skeleton_bone_name", "profile_bone_name", "skeleton_bone_name"), &BoneMap::set_skeleton_bone_name);

	ClassDB::bind_method(D_METHOD("find_profile_bone_name", "skeleton_bone_name"), &BoneMap::find_profile_bone_name);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "profile", PROPERTY_HINT_RESOURCE_TYPE, "SkeletonProfile"), "set_profile", "get_profile");
	ADD_ARRAY("bonemap", "bonemap");

	ADD_SIGNAL(MethodInfo("bone_map_updated"));
	ADD_SIGNAL(MethodInfo("profile_updated"));
}

void BoneMap::_validate_property(PropertyInfo &property) const {
	if (property.name == "bonemap" || property.name == "profile") {
		property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

BoneMap::BoneMap() {
	_validate_bone_map();
}

BoneMap::~BoneMap() {
}

//////////////////////////////////////
