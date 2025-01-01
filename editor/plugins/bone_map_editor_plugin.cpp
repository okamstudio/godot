/**************************************************************************/
/*  bone_map_editor_plugin.cpp                                            */
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

#include "bone_map_editor_plugin.h"

#include "editor/editor_settings.h"
#include "editor/import/3d/post_import_plugin_skeleton_renamer.h"
#include "editor/import/3d/post_import_plugin_skeleton_rest_fixer.h"
#include "editor/import/3d/post_import_plugin_skeleton_track_organizer.h"
#include "editor/import/3d/scene_import_settings.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/aspect_ratio_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/texture_rect.h"

void BoneMapperButton::fetch_textures() {
	if (selected) {
		set_texture_normal(get_editor_theme_icon(SNAME("BoneMapperHandleSelected")));
	} else {
		set_texture_normal(get_editor_theme_icon(SNAME("BoneMapperHandle")));
	}
	set_offset(SIDE_LEFT, 0);
	set_offset(SIDE_RIGHT, 0);
	set_offset(SIDE_TOP, 0);
	set_offset(SIDE_BOTTOM, 0);

	// Hack to avoid handle color darkening...
	set_modulate(EditorThemeManager::is_dark_theme() ? Color(1, 1, 1) : Color(4.25, 4.25, 4.25));

	circle = memnew(TextureRect);
	circle->set_texture(get_editor_theme_icon(SNAME("BoneMapperHandleCircle")));
	add_child(circle);
	set_state(BONE_MAP_STATE_UNSET);
}

StringName BoneMapperButton::get_profile_bone_name() const {
	return profile_bone_name;
}

void BoneMapperButton::set_state(BoneMapState p_state) {
	switch (p_state) {
		case BONE_MAP_STATE_UNSET: {
			circle->set_modulate(EDITOR_GET("editors/bone_mapper/handle_colors/unset"));
		} break;
		case BONE_MAP_STATE_SET: {
			circle->set_modulate(EDITOR_GET("editors/bone_mapper/handle_colors/set"));
		} break;
		case BONE_MAP_STATE_MISSING: {
			circle->set_modulate(EDITOR_GET("editors/bone_mapper/handle_colors/missing"));
		} break;
		case BONE_MAP_STATE_ERROR: {
			circle->set_modulate(EDITOR_GET("editors/bone_mapper/handle_colors/error"));
		} break;
		default: {
		} break;
	}
}

bool BoneMapperButton::is_require() const {
	return require;
}

void BoneMapperButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			fetch_textures();
		} break;
	}
}

BoneMapperButton::BoneMapperButton(const StringName &p_profile_bone_name, bool p_require, bool p_selected) {
	profile_bone_name = p_profile_bone_name;
	require = p_require;
	selected = p_selected;
}

BoneMapperButton::~BoneMapperButton() {
}

void BoneMapperItem::create_editor() {
	HBoxContainer *hbox = memnew(HBoxContainer);
	add_child(hbox);

	skeleton_bone_selector = memnew(EditorPropertyText);
	skeleton_bone_selector->set_label(profile_bone_name);
	skeleton_bone_selector->set_selectable(false);
	skeleton_bone_selector->set_h_size_flags(SIZE_EXPAND_FILL);
	skeleton_bone_selector->set_object_and_property(bone_map.ptr(), "bone_map/" + String(profile_bone_name));
	skeleton_bone_selector->update_property();
	skeleton_bone_selector->connect("property_changed", callable_mp(this, &BoneMapperItem::_value_changed));
	hbox->add_child(skeleton_bone_selector);

	picker_button = memnew(Button);
	picker_button->set_button_icon(get_editor_theme_icon(SNAME("ClassList")));
	picker_button->connect(SceneStringName(pressed), callable_mp(this, &BoneMapperItem::_open_picker));
	hbox->add_child(picker_button);

	add_child(memnew(HSeparator));
}

void BoneMapperItem::_update_property() {
	if (skeleton_bone_selector->get_edited_object() && skeleton_bone_selector->get_edited_property()) {
		skeleton_bone_selector->update_property();
	}
}

void BoneMapperItem::_open_picker() {
	emit_signal(SNAME("pick"), profile_bone_name);
}

void BoneMapperItem::_value_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing) {
	bone_map->set(p_property, p_value);
}

void BoneMapperItem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			create_editor();
			bone_map->connect("bone_map_updated", callable_mp(this, &BoneMapperItem::_update_property));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (bone_map.is_valid() && bone_map->is_connected("bone_map_updated", callable_mp(this, &BoneMapperItem::_update_property))) {
				bone_map->disconnect("bone_map_updated", callable_mp(this, &BoneMapperItem::_update_property));
			}
		} break;
	}
}

void BoneMapperItem::_bind_methods() {
	ADD_SIGNAL(MethodInfo("pick", PropertyInfo(Variant::STRING_NAME, "profile_bone_name")));
}

BoneMapperItem::BoneMapperItem(Ref<BoneMap> &p_bone_map, const StringName &p_profile_bone_name) {
	bone_map = p_bone_map;
	profile_bone_name = p_profile_bone_name;
}

BoneMapperItem::~BoneMapperItem() {
}

void BonePicker::create_editors() {
	set_title(TTR("Bone Picker:"));

	VBoxContainer *vbox = memnew(VBoxContainer);
	add_child(vbox);

	bones = memnew(Tree);
	bones->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	bones->set_select_mode(Tree::SELECT_SINGLE);
	bones->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	bones->set_hide_root(true);
	bones->connect("item_activated", callable_mp(this, &BonePicker::_confirm));
	vbox->add_child(bones);

	create_bones_tree(skeleton);
}

void BonePicker::create_bones_tree(Skeleton3D *p_skeleton) {
	bones->clear();

	if (!p_skeleton) {
		return;
	}

	TreeItem *root = bones->create_item();

	HashMap<int, TreeItem *> items;

	items.insert(-1, root);

	Ref<Texture> bone_icon = get_editor_theme_icon(SNAME("Bone"));

	Vector<int> bones_to_process = p_skeleton->get_parentless_bones();
	bool is_first = true;
	while (bones_to_process.size() > 0) {
		int current_bone_idx = bones_to_process[0];
		bones_to_process.erase(current_bone_idx);

		Vector<int> current_bone_child_bones = p_skeleton->get_bone_children(current_bone_idx);
		int child_bone_size = current_bone_child_bones.size();
		for (int i = 0; i < child_bone_size; i++) {
			bones_to_process.push_back(current_bone_child_bones[i]);
		}

		const int parent_idx = p_skeleton->get_bone_parent(current_bone_idx);
		TreeItem *parent_item = items.find(parent_idx)->value;

		TreeItem *joint_item = bones->create_item(parent_item);
		items.insert(current_bone_idx, joint_item);

		joint_item->set_text(0, p_skeleton->get_bone_name(current_bone_idx));
		joint_item->set_icon(0, bone_icon);
		joint_item->set_selectable(0, true);
		joint_item->set_metadata(0, "bones/" + itos(current_bone_idx));
		if (is_first) {
			is_first = false;
		} else {
			joint_item->set_collapsed(true);
		}
	}
}

void BonePicker::_confirm() {
	_ok_pressed();
}

void BonePicker::popup_bones_tree(const Size2i &p_minsize) {
	popup_centered(p_minsize);
}

bool BonePicker::has_selected_bone() {
	TreeItem *selected = bones->get_selected();
	if (!selected) {
		return false;
	}
	return true;
}

StringName BonePicker::get_selected_bone() {
	TreeItem *selected = bones->get_selected();
	if (!selected) {
		return StringName();
	}
	return selected->get_text(0);
}

void BonePicker::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			create_editors();
		} break;
	}
}

BonePicker::BonePicker(Skeleton3D *p_skeleton) {
	skeleton = p_skeleton;
}

BonePicker::~BonePicker() {
}

void BoneMapper::create_editor() {
	// Create Bone picker.
	picker = memnew(BonePicker(skeleton));
	picker->connect(SceneStringName(confirmed), callable_mp(this, &BoneMapper::_apply_picker_selection));
	add_child(picker, false, INTERNAL_MODE_FRONT);

	profile_selector = memnew(EditorPropertyResource);
	profile_selector->setup(bone_map.ptr(), "profile", "SkeletonProfile");
	profile_selector->set_label("Profile");
	profile_selector->set_selectable(false);
	profile_selector->set_object_and_property(bone_map.ptr(), "profile");
	profile_selector->update_property();
	profile_selector->connect("property_changed", callable_mp(this, &BoneMapper::_profile_changed));
	add_child(profile_selector);
	add_child(memnew(HSeparator));

	HBoxContainer *group_hbox = memnew(HBoxContainer);
	add_child(group_hbox);

	profile_group_selector = memnew(EditorPropertyEnum);
	profile_group_selector->set_label("Group");
	profile_group_selector->set_selectable(false);
	profile_group_selector->set_h_size_flags(SIZE_EXPAND_FILL);
	profile_group_selector->set_object_and_property(this, "current_group_idx");
	profile_group_selector->update_property();
	profile_group_selector->connect("property_changed", callable_mp(this, &BoneMapper::_value_changed));
	group_hbox->add_child(profile_group_selector);

	clear_mapping_button = memnew(Button);
	clear_mapping_button->set_button_icon(get_editor_theme_icon(SNAME("Clear")));
	clear_mapping_button->set_tooltip_text(TTR("Clear mappings in current group."));
	clear_mapping_button->connect(SceneStringName(pressed), callable_mp(this, &BoneMapper::_clear_mapping_current_group));
	group_hbox->add_child(clear_mapping_button);

	bone_mapper_field = memnew(AspectRatioContainer);
	bone_mapper_field->set_stretch_mode(AspectRatioContainer::STRETCH_FIT);
	bone_mapper_field->set_custom_minimum_size(Vector2(0, 256.0) * EDSCALE);
	bone_mapper_field->set_h_size_flags(Control::SIZE_FILL);
	add_child(bone_mapper_field);

	profile_bg = memnew(ColorRect);
	profile_bg->set_color(Color(0, 0, 0, 1));
	profile_bg->set_h_size_flags(Control::SIZE_FILL);
	profile_bg->set_v_size_flags(Control::SIZE_FILL);
	bone_mapper_field->add_child(profile_bg);

	profile_texture = memnew(TextureRect);
	profile_texture->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	profile_texture->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	profile_texture->set_h_size_flags(Control::SIZE_FILL);
	profile_texture->set_v_size_flags(Control::SIZE_FILL);
	bone_mapper_field->add_child(profile_texture);

	mapper_item_vbox = memnew(VBoxContainer);
	add_child(mapper_item_vbox);

	recreate_items();
}

void BoneMapper::update_group_idx() {
	if (bone_map->get_profile().is_null()) {
		return;
	}

	PackedStringArray group_names;
	int len = bone_map->get_profile()->get_group_size();
	for (int i = 0; i < len; i++) {
		group_names.push_back(bone_map->get_profile()->get_group_name(i));
	}
	if (current_group_idx >= len) {
		current_group_idx = 0;
	}
	if (len > 0) {
		profile_group_selector->setup(group_names);
		profile_group_selector->update_property();
		profile_group_selector->set_read_only(false);
	}
}

void BoneMapper::_pick_bone(const StringName &p_bone_name) {
	picker_key_name = p_bone_name;
	picker->popup_bones_tree(Size2(500, 500) * EDSCALE);
}

void BoneMapper::_apply_picker_selection() {
	if (!picker->has_selected_bone()) {
		return;
	}
	bone_map->set_skeleton_bone_name(picker_key_name, picker->get_selected_bone());
}

void BoneMapper::set_current_group_idx(int p_group_idx) {
	current_group_idx = p_group_idx;
	recreate_editor();
}

int BoneMapper::get_current_group_idx() const {
	return current_group_idx;
}

void BoneMapper::set_current_bone_idx(int p_bone_idx) {
	current_bone_idx = p_bone_idx;
	recreate_editor();
}

int BoneMapper::get_current_bone_idx() const {
	return current_bone_idx;
}

void BoneMapper::recreate_editor() {
	// Clear buttons.
	int len = bone_mapper_buttons.size();
	for (int i = 0; i < len; i++) {
		profile_texture->remove_child(bone_mapper_buttons[i]);
		memdelete(bone_mapper_buttons[i]);
	}
	bone_mapper_buttons.clear();

	// Organize mapper items.
	len = bone_mapper_items.size();
	for (int i = 0; i < len; i++) {
		bone_mapper_items[i]->set_visible(current_bone_idx == i);
	}

	Ref<SkeletonProfile> profile = bone_map->get_profile();
	if (profile.is_valid()) {
		SkeletonProfileHumanoid *hmn = Object::cast_to<SkeletonProfileHumanoid>(profile.ptr());
		if (hmn) {
			StringName hmn_group_name = profile->get_group_name(current_group_idx);
			if (hmn_group_name == "Body") {
				profile_texture->set_texture(get_editor_theme_icon(SNAME("BoneMapHumanBody")));
			} else if (hmn_group_name == "Face") {
				profile_texture->set_texture(get_editor_theme_icon(SNAME("BoneMapHumanFace")));
			} else if (hmn_group_name == "LeftHand") {
				profile_texture->set_texture(get_editor_theme_icon(SNAME("BoneMapHumanLeftHand")));
			} else if (hmn_group_name == "RightHand") {
				profile_texture->set_texture(get_editor_theme_icon(SNAME("BoneMapHumanRightHand")));
			}
		} else {
			profile_texture->set_texture(profile->get_texture(current_group_idx));
		}
	} else {
		profile_texture->set_texture(Ref<Texture2D>());
	}

	if (profile.is_null()) {
		return;
	}

	for (int i = 0; i < len; i++) {
		if (profile->get_group(i) == profile->get_group_name(current_group_idx)) {
			BoneMapperButton *mb = memnew(BoneMapperButton(profile->get_bone_name(i), profile->is_required(i), current_bone_idx == i));
			mb->connect(SceneStringName(pressed), callable_mp(this, &BoneMapper::set_current_bone_idx).bind(i), CONNECT_DEFERRED);
			mb->set_h_grow_direction(GROW_DIRECTION_BOTH);
			mb->set_v_grow_direction(GROW_DIRECTION_BOTH);
			Vector2 vc = profile->get_handle_offset(i);
			bone_mapper_buttons.push_back(mb);
			profile_texture->add_child(mb);
			mb->set_anchor(SIDE_LEFT, vc.x);
			mb->set_anchor(SIDE_RIGHT, vc.x);
			mb->set_anchor(SIDE_TOP, vc.y);
			mb->set_anchor(SIDE_BOTTOM, vc.y);
		}
	}

	_update_state();
}

void BoneMapper::clear_items() {
	// Clear items.
	int len = bone_mapper_items.size();
	for (int i = 0; i < len; i++) {
		bone_mapper_items[i]->disconnect("pick", callable_mp(this, &BoneMapper::_pick_bone));
		mapper_item_vbox->remove_child(bone_mapper_items[i]);
		memdelete(bone_mapper_items[i]);
	}
	bone_mapper_items.clear();
}

void BoneMapper::recreate_items() {
	clear_items();
	// Create items by profile.
	Ref<SkeletonProfile> profile = bone_map->get_profile();
	if (profile.is_valid()) {
		int len = profile->get_bone_size();
		for (int i = 0; i < len; i++) {
			StringName bn = profile->get_bone_name(i);
			bone_mapper_items.append(memnew(BoneMapperItem(bone_map, bn)));
			bone_mapper_items[i]->connect("pick", callable_mp(this, &BoneMapper::_pick_bone), CONNECT_DEFERRED);
			mapper_item_vbox->add_child(bone_mapper_items[i]);
		}
	}

	update_group_idx();
	recreate_editor();
}

void BoneMapper::_update_state() {
	int len = bone_mapper_buttons.size();
	for (int i = 0; i < len; i++) {
		StringName pbn = bone_mapper_buttons[i]->get_profile_bone_name();
		StringName sbn = bone_map->get_skeleton_bone_name(pbn);
		int bone_idx = skeleton->find_bone(sbn);
		if (bone_idx >= 0) {
			if (bone_map->get_skeleton_bone_name_count(sbn) == 1) {
				Ref<SkeletonProfile> prof = bone_map->get_profile();

				StringName parent_name = prof->get_bone_parent(prof->find_bone(pbn));
				Vector<int> prof_parent_bones;
				while (parent_name != StringName()) {
					prof_parent_bones.push_back(skeleton->find_bone(bone_map->get_skeleton_bone_name(parent_name)));
					if (prof->find_bone(parent_name) == -1) {
						break;
					}
					parent_name = prof->get_bone_parent(prof->find_bone(parent_name));
				}

				int parent_id = skeleton->get_bone_parent(bone_idx);
				Vector<int> skel_parent_bones;
				while (parent_id >= 0) {
					skel_parent_bones.push_back(parent_id);
					parent_id = skeleton->get_bone_parent(parent_id);
				}

				bool is_broken = false;
				for (int j = 0; j < prof_parent_bones.size(); j++) {
					if (prof_parent_bones[j] != -1 && !skel_parent_bones.has(prof_parent_bones[j])) {
						is_broken = true;
					}
				}

				if (is_broken) {
					bone_mapper_buttons[i]->set_state(BoneMapperButton::BONE_MAP_STATE_ERROR);
				} else {
					bone_mapper_buttons[i]->set_state(BoneMapperButton::BONE_MAP_STATE_SET);
				}
			} else {
				bone_mapper_buttons[i]->set_state(BoneMapperButton::BONE_MAP_STATE_ERROR);
			}
		} else {
			if (bone_mapper_buttons[i]->is_require()) {
				bone_mapper_buttons[i]->set_state(BoneMapperButton::BONE_MAP_STATE_MISSING);
			} else {
				bone_mapper_buttons[i]->set_state(BoneMapperButton::BONE_MAP_STATE_UNSET);
			}
		}
	}
}

void BoneMapper::_clear_mapping_current_group() {
	if (bone_map.is_valid()) {
		Ref<SkeletonProfile> profile = bone_map->get_profile();
		if (profile.is_valid() && profile->get_group_size() > 0) {
			int len = profile->get_bone_size();
			for (int i = 0; i < len; i++) {
				if (profile->get_group(i) == profile->get_group_name(current_group_idx)) {
					bone_map->_set_skeleton_bone_name(profile->get_bone_name(i), StringName());
				}
			}
			recreate_items();
		}
	}
}

#ifdef MODULE_REGEX_ENABLED
bool BoneMapper::is_match_with_bone_name(const String &p_bone_name, const String &p_word) {
	RegEx re = RegEx(p_word);
	return re.search(p_bone_name.to_lower()).is_valid();
}

int BoneMapper::search_bone_by_name(Skeleton3D *p_skeleton, const Vector<String> &p_picklist, BoneSegregation p_segregation, int p_parent, int p_child, int p_children_count) {
	// There may be multiple candidates hit by existing the subsidiary bone.
	// The one with the shortest name is probably the original.
	LocalVector<String> hit_list;
	String shortest = "";

	for (int word_idx = 0; word_idx < p_picklist.size(); word_idx++) {
		if (p_child == -1) {
			Vector<int> bones_to_process = p_parent == -1 ? p_skeleton->get_parentless_bones() : p_skeleton->get_bone_children(p_parent);
			while (bones_to_process.size() > 0) {
				int idx = bones_to_process[0];
				bones_to_process.erase(idx);
				Vector<int> children = p_skeleton->get_bone_children(idx);
				for (int i = 0; i < children.size(); i++) {
					bones_to_process.push_back(children[i]);
				}

				if (p_children_count == 0 && children.size() > 0) {
					continue;
				}
				if (p_children_count > 0 && children.size() < p_children_count) {
					continue;
				}

				String bn = skeleton->get_bone_name(idx);
				if (is_match_with_bone_name(bn, p_picklist[word_idx]) && guess_bone_segregation(bn) == p_segregation) {
					hit_list.push_back(bn);
				}
			}

			if (hit_list.size() > 0) {
				shortest = hit_list[0];
				for (const String &hit : hit_list) {
					if (hit.length() < shortest.length()) {
						shortest = hit; // Prioritize parent.
					}
				}
			}
		} else {
			int idx = skeleton->get_bone_parent(p_child);
			while (idx != p_parent && idx >= 0) {
				Vector<int> children = p_skeleton->get_bone_children(idx);
				if (p_children_count == 0 && children.size() > 0) {
					continue;
				}
				if (p_children_count > 0 && children.size() < p_children_count) {
					continue;
				}

				String bn = skeleton->get_bone_name(idx);
				if (is_match_with_bone_name(bn, p_picklist[word_idx]) && guess_bone_segregation(bn) == p_segregation) {
					hit_list.push_back(bn);
				}
				idx = skeleton->get_bone_parent(idx);
			}

			if (hit_list.size() > 0) {
				shortest = hit_list[0];
				for (const String &hit : hit_list) {
					if (hit.length() <= shortest.length()) {
						shortest = hit; // Prioritize parent.
					}
				}
			}
		}

		if (shortest != "") {
			break;
		}
	}

	if (shortest == "") {
		return -1;
	}

	return skeleton->find_bone(shortest);
}

BoneMapper::BoneSegregation BoneMapper::guess_bone_segregation(const String &p_bone_name) {
	String fixed_bn = p_bone_name.to_snake_case();

	LocalVector<String> left_words;
	left_words.push_back("(?<![a-zA-Z])left");
	left_words.push_back("(?<![a-zA-Z0-9])l(?![a-zA-Z0-9])");

	LocalVector<String> right_words;
	right_words.push_back("(?<![a-zA-Z])right");
	right_words.push_back("(?<![a-zA-Z0-9])r(?![a-zA-Z0-9])");

	for (uint32_t i = 0; i < left_words.size(); i++) {
		RegEx re_l = RegEx(left_words[i]);
		if (re_l.search(fixed_bn).is_valid()) {
			return BONE_SEGREGATION_LEFT;
		}
		RegEx re_r = RegEx(right_words[i]);
		if (re_r.search(fixed_bn).is_valid()) {
			return BONE_SEGREGATION_RIGHT;
		}
	}

	return BONE_SEGREGATION_NONE;
}

void BoneMapper::_run_auto_mapping() {
	auto_mapping_process(bone_map);
	recreate_items();
}

void BoneMapper::auto_mapping_process(Ref<BoneMap> &p_bone_map) {
	WARN_PRINT("Run auto mapping.");

	int bone_idx = -1;
	Vector<String> picklist; // Use Vector<String> because match words have priority.
	Vector<int> search_path;

	// 1. Guess Hips
	picklist.push_back("hip");
	picklist.push_back("pelvis");
	picklist.push_back("waist");
	picklist.push_back("torso");
	picklist.push_back("spine");
	int hips = search_bone_by_name(skeleton, picklist);
	if (hips == -1) {
		WARN_PRINT("Auto Mapping couldn't guess Hips. Abort auto mapping.");
		return; // If there is no Hips, we cannot guess bone after then.
	} else {
		p_bone_map->_set_skeleton_bone_name("Hips", skeleton->get_bone_name(hips));
	}
	picklist.clear();

	// 2. Guess Root
	bone_idx = skeleton->get_bone_parent(hips);
	while (bone_idx >= 0) {
		search_path.push_back(bone_idx);
		bone_idx = skeleton->get_bone_parent(bone_idx);
	}
	if (search_path.size() == 0) {
		bone_idx = -1;
	} else if (search_path.size() == 1) {
		bone_idx = search_path[0]; // It is only one bone which can be root.
	} else {
		bool found = false;
		for (int i = 0; i < search_path.size(); i++) {
			RegEx re = RegEx("root");
			if (re.search(skeleton->get_bone_name(search_path[i]).to_lower()).is_valid()) {
				bone_idx = search_path[i]; // Name match is preferred.
				found = true;
				break;
			}
		}
		if (!found) {
			for (int i = 0; i < search_path.size(); i++) {
				if (skeleton->get_bone_global_rest(search_path[i]).origin.is_zero_approx()) {
					bone_idx = search_path[i]; // The bone existing at the origin is appropriate as a root.
					found = true;
					break;
				}
			}
		}
		if (!found) {
			bone_idx = search_path[search_path.size() - 1]; // Ambiguous, but most parental bone selected.
		}
	}
	if (bone_idx == -1) {
		WARN_PRINT("Auto Mapping couldn't guess Root."); // Root is not required, so continue.
	} else {
		p_bone_map->_set_skeleton_bone_name("Root", skeleton->get_bone_name(bone_idx));
	}
	bone_idx = -1;
	search_path.clear();

	// 3. Guess Foots
	picklist.push_back("foot");
	picklist.push_back("ankle");
	int left_foot = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, hips);
	if (left_foot == -1) {
		WARN_PRINT("Auto Mapping couldn't guess LeftFoot.");
	} else {
		p_bone_map->_set_skeleton_bone_name("LeftFoot", skeleton->get_bone_name(left_foot));
	}
	int right_foot = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, hips);
	if (right_foot == -1) {
		WARN_PRINT("Auto Mapping couldn't guess RightFoot.");
	} else {
		p_bone_map->_set_skeleton_bone_name("RightFoot", skeleton->get_bone_name(right_foot));
	}
	picklist.clear();

	// 3-1. Guess LowerLegs
	picklist.push_back("(low|under).*leg");
	picklist.push_back("knee");
	picklist.push_back("shin");
	picklist.push_back("calf");
	picklist.push_back("leg");
	int left_lower_leg = -1;
	if (left_foot != -1) {
		left_lower_leg = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, hips, left_foot);
	}
	if (left_lower_leg == -1) {
		WARN_PRINT("Auto Mapping couldn't guess LeftLowerLeg.");
	} else {
		p_bone_map->_set_skeleton_bone_name("LeftLowerLeg", skeleton->get_bone_name(left_lower_leg));
	}
	int right_lower_leg = -1;
	if (right_foot != -1) {
		right_lower_leg = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, hips, right_foot);
	}
	if (right_lower_leg == -1) {
		WARN_PRINT("Auto Mapping couldn't guess RightLowerLeg.");
	} else {
		p_bone_map->_set_skeleton_bone_name("RightLowerLeg", skeleton->get_bone_name(right_lower_leg));
	}
	picklist.clear();

	// 3-2. Guess UpperLegs
	picklist.push_back("up.*leg");
	picklist.push_back("thigh");
	picklist.push_back("leg");
	if (left_lower_leg != -1) {
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, hips, left_lower_leg);
	}
	if (bone_idx == -1) {
		WARN_PRINT("Auto Mapping couldn't guess LeftUpperLeg.");
	} else {
		p_bone_map->_set_skeleton_bone_name("LeftUpperLeg", skeleton->get_bone_name(bone_idx));
	}
	bone_idx = -1;
	if (right_lower_leg != -1) {
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, hips, right_lower_leg);
	}
	if (bone_idx == -1) {
		WARN_PRINT("Auto Mapping couldn't guess RightUpperLeg.");
	} else {
		p_bone_map->_set_skeleton_bone_name("RightUpperLeg", skeleton->get_bone_name(bone_idx));
	}
	bone_idx = -1;
	picklist.clear();

	// 3-3. Guess Toes
	picklist.push_back("toe");
	picklist.push_back("ball");
	if (left_foot != -1) {
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, left_foot);
		if (bone_idx == -1) {
			search_path = skeleton->get_bone_children(left_foot);
			if (search_path.size() == 1) {
				bone_idx = search_path[0]; // Maybe only one child of the Foot is Toes.
			}
			search_path.clear();
		}
	}
	if (bone_idx == -1) {
		WARN_PRINT("Auto Mapping couldn't guess LeftToes.");
	} else {
		p_bone_map->_set_skeleton_bone_name("LeftToes", skeleton->get_bone_name(bone_idx));
	}
	bone_idx = -1;
	if (right_foot != -1) {
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, right_foot);
		if (bone_idx == -1) {
			search_path = skeleton->get_bone_children(right_foot);
			if (search_path.size() == 1) {
				bone_idx = search_path[0]; // Maybe only one child of the Foot is Toes.
			}
			search_path.clear();
		}
	}
	if (bone_idx == -1) {
		WARN_PRINT("Auto Mapping couldn't guess RightToes.");
	} else {
		p_bone_map->_set_skeleton_bone_name("RightToes", skeleton->get_bone_name(bone_idx));
	}
	bone_idx = -1;
	picklist.clear();

	// 4. Guess Hands
	picklist.push_back("hand");
	picklist.push_back("wrist");
	picklist.push_back("palm");
	picklist.push_back("fingers");
	int left_hand_or_palm = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, hips, -1, 5);
	if (left_hand_or_palm == -1) {
		// Ambiguous, but try again for fewer finger models.
		left_hand_or_palm = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, hips);
	}
	int left_hand = left_hand_or_palm; // Check for the presence of a wrist, since bones with five children may be palmar.
	while (left_hand != -1) {
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, hips, left_hand);
		if (bone_idx == -1) {
			break;
		}
		left_hand = bone_idx;
	}
	if (left_hand == -1) {
		WARN_PRINT("Auto Mapping couldn't guess LeftHand.");
	} else {
		p_bone_map->_set_skeleton_bone_name("LeftHand", skeleton->get_bone_name(left_hand));
	}
	bone_idx = -1;
	int right_hand_or_palm = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, hips, -1, 5);
	if (right_hand_or_palm == -1) {
		// Ambiguous, but try again for fewer finger models.
		right_hand_or_palm = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, hips);
	}
	int right_hand = right_hand_or_palm;
	while (right_hand != -1) {
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, hips, right_hand);
		if (bone_idx == -1) {
			break;
		}
		right_hand = bone_idx;
	}
	if (right_hand == -1) {
		WARN_PRINT("Auto Mapping couldn't guess RightHand.");
	} else {
		p_bone_map->_set_skeleton_bone_name("RightHand", skeleton->get_bone_name(right_hand));
	}
	bone_idx = -1;
	picklist.clear();

	// 4-1. Guess Finger
	int tips_index = -1;
	bool thumb_tips_size = false;
	bool named_finger_is_found = false;
	LocalVector<String> fingers;
	fingers.push_back("thumb|pollex");
	fingers.push_back("index|fore");
	fingers.push_back("middle");
	fingers.push_back("ring");
	fingers.push_back("little|pinkie|pinky");
	if (left_hand_or_palm != -1) {
		LocalVector<LocalVector<String>> left_fingers_map;
		left_fingers_map.resize(5);
		left_fingers_map[0].push_back("LeftThumbMetacarpal");
		left_fingers_map[0].push_back("LeftThumbProximal");
		left_fingers_map[0].push_back("LeftThumbDistal");
		left_fingers_map[1].push_back("LeftIndexProximal");
		left_fingers_map[1].push_back("LeftIndexIntermediate");
		left_fingers_map[1].push_back("LeftIndexDistal");
		left_fingers_map[2].push_back("LeftMiddleProximal");
		left_fingers_map[2].push_back("LeftMiddleIntermediate");
		left_fingers_map[2].push_back("LeftMiddleDistal");
		left_fingers_map[3].push_back("LeftRingProximal");
		left_fingers_map[3].push_back("LeftRingIntermediate");
		left_fingers_map[3].push_back("LeftRingDistal");
		left_fingers_map[4].push_back("LeftLittleProximal");
		left_fingers_map[4].push_back("LeftLittleIntermediate");
		left_fingers_map[4].push_back("LeftLittleDistal");
		for (int i = 0; i < 5; i++) {
			picklist.push_back(fingers[i]);
			int finger = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, left_hand_or_palm, -1, 0);
			if (finger != -1) {
				while (finger != left_hand_or_palm && finger >= 0) {
					search_path.push_back(finger);
					finger = skeleton->get_bone_parent(finger);
				}
				// Tips detection by name matching with "distal" from root.
				for (int j = search_path.size() - 1; j >= 0; j--) {
					if (RegEx("distal").search(skeleton->get_bone_name(search_path[j]).to_lower()).is_valid()) {
						tips_index = j - 1;
						break;
					}
				}
				// Tips detection by name matching with "tip|leaf" from end.
				if (tips_index < 0) {
					for (int j = 0; j < search_path.size(); j++) {
						if (RegEx("tip|leaf").search(skeleton->get_bone_name(search_path[j]).to_lower()).is_valid()) {
							tips_index = j;
							break;
						}
					}
				}
				// Tips detection by thumb children size.
				if (tips_index < 0) {
					if (i == 0) {
						thumb_tips_size = MAX(0, search_path.size() - 3);
					}
					tips_index = thumb_tips_size - 1;
				}
				// Remove tips.
				for (int j = 0; j <= tips_index; j++) {
					search_path.remove_at(0);
				}
				search_path.reverse();
				if (search_path.size() == 1) {
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					named_finger_is_found = true;
				} else if (search_path.size() == 2) {
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][1], skeleton->get_bone_name(search_path[1]));
					named_finger_is_found = true;
				} else if (search_path.size() >= 3) {
					search_path = search_path.slice(-3); // Eliminate the possibility of carpal bone.
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][1], skeleton->get_bone_name(search_path[1]));
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][2], skeleton->get_bone_name(search_path[2]));
					named_finger_is_found = true;
				}
			}
			picklist.clear();
			search_path.clear();
		}

		// It is a bit corner case, but possibly the finger names are sequentially numbered...
		if (!named_finger_is_found) {
			picklist.push_back("finger");
			RegEx finger_re = RegEx("finger");
			search_path = skeleton->get_bone_children(left_hand_or_palm);
			Vector<String> finger_names;
			for (int i = 0; i < search_path.size(); i++) {
				String bn = skeleton->get_bone_name(search_path[i]);
				if (finger_re.search(bn.to_lower()).is_valid()) {
					finger_names.push_back(bn);
				}
			}
			finger_names.sort(); // Order by lexicographic, normal use cases never have more than 10 fingers in one hand.
			search_path.clear();
			for (int i = 0; i < finger_names.size(); i++) {
				if (i >= 5) {
					break;
				}
				int finger_root = skeleton->find_bone(finger_names[i]);
				int finger = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, finger_root, -1, 0);
				if (finger != -1) {
					while (finger != finger_root && finger >= 0) {
						search_path.push_back(finger);
						finger = skeleton->get_bone_parent(finger);
					}
				}
				search_path.push_back(finger_root);
				// Tips detection by thumb children size.
				if (i == 0) {
					thumb_tips_size = MAX(0, search_path.size() - 3);
				}
				tips_index = thumb_tips_size - 1;
				for (int j = 0; j <= tips_index; j++) {
					search_path.remove_at(0);
				}
				search_path.reverse();
				if (search_path.size() == 1) {
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
				} else if (search_path.size() == 2) {
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][1], skeleton->get_bone_name(search_path[1]));
				} else if (search_path.size() >= 3) {
					search_path = search_path.slice(-3); // Eliminate the possibility of carpal bone.
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][1], skeleton->get_bone_name(search_path[1]));
					p_bone_map->_set_skeleton_bone_name(left_fingers_map[i][2], skeleton->get_bone_name(search_path[2]));
				}
				search_path.clear();
			}
			picklist.clear();
		}
	}

	tips_index = -1;
	thumb_tips_size = false;
	named_finger_is_found = false;
	if (right_hand_or_palm != -1) {
		LocalVector<LocalVector<String>> right_fingers_map;
		right_fingers_map.resize(5);
		right_fingers_map[0].push_back("RightThumbMetacarpal");
		right_fingers_map[0].push_back("RightThumbProximal");
		right_fingers_map[0].push_back("RightThumbDistal");
		right_fingers_map[1].push_back("RightIndexProximal");
		right_fingers_map[1].push_back("RightIndexIntermediate");
		right_fingers_map[1].push_back("RightIndexDistal");
		right_fingers_map[2].push_back("RightMiddleProximal");
		right_fingers_map[2].push_back("RightMiddleIntermediate");
		right_fingers_map[2].push_back("RightMiddleDistal");
		right_fingers_map[3].push_back("RightRingProximal");
		right_fingers_map[3].push_back("RightRingIntermediate");
		right_fingers_map[3].push_back("RightRingDistal");
		right_fingers_map[4].push_back("RightLittleProximal");
		right_fingers_map[4].push_back("RightLittleIntermediate");
		right_fingers_map[4].push_back("RightLittleDistal");
		for (int i = 0; i < 5; i++) {
			picklist.push_back(fingers[i]);
			int finger = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, right_hand_or_palm, -1, 0);
			if (finger != -1) {
				while (finger != right_hand_or_palm && finger >= 0) {
					search_path.push_back(finger);
					finger = skeleton->get_bone_parent(finger);
				}
				// Tips detection by name matching with "distal" from root.
				for (int j = search_path.size() - 1; j >= 0; j--) {
					if (RegEx("distal").search(skeleton->get_bone_name(search_path[j]).to_lower()).is_valid()) {
						tips_index = j - 1;
						break;
					}
				}
				// Tips detection by name matching with "tip|leaf" from end.
				if (tips_index < 0) {
					for (int j = 0; j < search_path.size(); j++) {
						if (RegEx("tip|leaf").search(skeleton->get_bone_name(search_path[j]).to_lower()).is_valid()) {
							tips_index = j;
							break;
						}
					}
				}
				// Tips detection by thumb children size.
				if (tips_index < 0) {
					if (i == 0) {
						thumb_tips_size = MAX(0, search_path.size() - 3);
					}
					tips_index = thumb_tips_size - 1;
				}
				// Remove tips.
				for (int j = 0; j <= tips_index; j++) {
					search_path.remove_at(0);
				}
				search_path.reverse();
				if (search_path.size() == 1) {
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					named_finger_is_found = true;
				} else if (search_path.size() == 2) {
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][1], skeleton->get_bone_name(search_path[1]));
					named_finger_is_found = true;
				} else if (search_path.size() >= 3) {
					search_path = search_path.slice(-3); // Eliminate the possibility of carpal bone.
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][1], skeleton->get_bone_name(search_path[1]));
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][2], skeleton->get_bone_name(search_path[2]));
					named_finger_is_found = true;
				}
			}
			picklist.clear();
			search_path.clear();
		}

		// It is a bit corner case, but possibly the finger names are sequentially numbered...
		if (!named_finger_is_found) {
			picklist.push_back("finger");
			RegEx finger_re = RegEx("finger");
			search_path = skeleton->get_bone_children(right_hand_or_palm);
			Vector<String> finger_names;
			for (int i = 0; i < search_path.size(); i++) {
				String bn = skeleton->get_bone_name(search_path[i]);
				if (finger_re.search(bn.to_lower()).is_valid()) {
					finger_names.push_back(bn);
				}
			}
			finger_names.sort(); // Order by lexicographic, normal use cases never have more than 10 fingers in one hand.
			search_path.clear();
			for (int i = 0; i < finger_names.size(); i++) {
				if (i >= 5) {
					break;
				}
				int finger_root = skeleton->find_bone(finger_names[i]);
				int finger = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, finger_root, -1, 0);
				if (finger != -1) {
					while (finger != finger_root && finger >= 0) {
						search_path.push_back(finger);
						finger = skeleton->get_bone_parent(finger);
					}
				}
				search_path.push_back(finger_root);
				// Tips detection by thumb children size.
				if (i == 0) {
					thumb_tips_size = MAX(0, search_path.size() - 3);
				}
				tips_index = thumb_tips_size - 1;
				for (int j = 0; j <= tips_index; j++) {
					search_path.remove_at(0);
				}
				search_path.reverse();
				if (search_path.size() == 1) {
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
				} else if (search_path.size() == 2) {
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][1], skeleton->get_bone_name(search_path[1]));
				} else if (search_path.size() >= 3) {
					search_path = search_path.slice(-3); // Eliminate the possibility of carpal bone.
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][0], skeleton->get_bone_name(search_path[0]));
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][1], skeleton->get_bone_name(search_path[1]));
					p_bone_map->_set_skeleton_bone_name(right_fingers_map[i][2], skeleton->get_bone_name(search_path[2]));
				}
				search_path.clear();
			}
			picklist.clear();
		}
	}

	// 5. Guess Arms
	picklist.push_back("shoulder");
	picklist.push_back("clavicle");
	picklist.push_back("collar");
	int left_shoulder = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, hips);
	if (left_shoulder == -1) {
		WARN_PRINT("Auto Mapping couldn't guess LeftShoulder.");
	} else {
		p_bone_map->_set_skeleton_bone_name("LeftShoulder", skeleton->get_bone_name(left_shoulder));
	}
	int right_shoulder = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, hips);
	if (right_shoulder == -1) {
		WARN_PRINT("Auto Mapping couldn't guess RightShoulder.");
	} else {
		p_bone_map->_set_skeleton_bone_name("RightShoulder", skeleton->get_bone_name(right_shoulder));
	}
	picklist.clear();

	// 5-1. Guess LowerArms
	picklist.push_back("(low|fore).*arm");
	picklist.push_back("elbow");
	picklist.push_back("arm");
	int left_lower_arm = -1;
	if (left_shoulder != -1 && left_hand_or_palm != -1) {
		left_lower_arm = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, left_shoulder, left_hand_or_palm);
	}
	if (left_lower_arm == -1) {
		WARN_PRINT("Auto Mapping couldn't guess LeftLowerArm.");
	} else {
		p_bone_map->_set_skeleton_bone_name("LeftLowerArm", skeleton->get_bone_name(left_lower_arm));
	}
	int right_lower_arm = -1;
	if (right_shoulder != -1 && right_hand_or_palm != -1) {
		right_lower_arm = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, right_shoulder, right_hand_or_palm);
	}
	if (right_lower_arm == -1) {
		WARN_PRINT("Auto Mapping couldn't guess RightLowerArm.");
	} else {
		p_bone_map->_set_skeleton_bone_name("RightLowerArm", skeleton->get_bone_name(right_lower_arm));
	}
	picklist.clear();

	// 5-2. Guess UpperArms
	picklist.push_back("up.*arm");
	picklist.push_back("arm");
	if (left_shoulder != -1 && left_lower_arm != -1) {
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, left_shoulder, left_lower_arm);
	}
	if (bone_idx == -1) {
		WARN_PRINT("Auto Mapping couldn't guess LeftUpperArm.");
	} else {
		p_bone_map->_set_skeleton_bone_name("LeftUpperArm", skeleton->get_bone_name(bone_idx));
	}
	bone_idx = -1;
	if (right_shoulder != -1 && right_lower_arm != -1) {
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, right_shoulder, right_lower_arm);
	}
	if (bone_idx == -1) {
		WARN_PRINT("Auto Mapping couldn't guess RightUpperArm.");
	} else {
		p_bone_map->_set_skeleton_bone_name("RightUpperArm", skeleton->get_bone_name(bone_idx));
	}
	bone_idx = -1;
	picklist.clear();

	// 6. Guess Neck
	picklist.push_back("neck");
	picklist.push_back("head"); // For no neck model.
	picklist.push_back("face"); // Same above.
	int neck = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_NONE, hips);
	picklist.clear();
	if (neck == -1) {
		// If it can't expect by name, search child spine of where the right and left shoulders (or hands) cross.
		int ls_idx = left_shoulder != -1 ? left_shoulder : (left_hand_or_palm != -1 ? left_hand_or_palm : -1);
		int rs_idx = right_shoulder != -1 ? right_shoulder : (right_hand_or_palm != -1 ? right_hand_or_palm : -1);
		if (ls_idx != -1 && rs_idx != -1) {
			bool detect = false;
			while (ls_idx != hips && ls_idx >= 0 && rs_idx != hips && rs_idx >= 0) {
				ls_idx = skeleton->get_bone_parent(ls_idx);
				rs_idx = skeleton->get_bone_parent(rs_idx);
				if (ls_idx == rs_idx) {
					detect = true;
					break;
				}
			}
			if (detect) {
				Vector<int> children = skeleton->get_bone_children(ls_idx);
				children.erase(ls_idx);
				children.erase(rs_idx);
				String word = "spine"; // It would be better to limit the search with "spine" because it could be mistaken with breast, wing and etc...
				for (int i = 0; i < children.size(); i++) {
					bone_idx = children[i];
					if (is_match_with_bone_name(skeleton->get_bone_name(bone_idx), word)) {
						neck = bone_idx;
						break;
					};
				}
				bone_idx = -1;
			}
		}
	}

	// 7. Guess Head
	picklist.push_back("head");
	picklist.push_back("face");
	int head = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_NONE, neck);
	if (head == -1) {
		if (neck != -1) {
			search_path = skeleton->get_bone_children(neck);
			if (search_path.size() == 1) {
				head = search_path[0]; // Maybe only one child of the Neck is Head.
			}
		}
	}
	if (head == -1) {
		if (neck != -1) {
			head = neck; // The head animation should have more movement.
			neck = -1;
			p_bone_map->_set_skeleton_bone_name("Head", skeleton->get_bone_name(head));
		} else {
			WARN_PRINT("Auto Mapping couldn't guess Neck or Head."); // Continued for guessing on the other bones. But abort when guessing spines step.
		}
	} else {
		p_bone_map->_set_skeleton_bone_name("Neck", skeleton->get_bone_name(neck));
		p_bone_map->_set_skeleton_bone_name("Head", skeleton->get_bone_name(head));
	}
	picklist.clear();
	search_path.clear();

	int neck_or_head = neck != -1 ? neck : (head != -1 ? head : -1);
	if (neck_or_head != -1) {
		// 7-1. Guess Eyes
		picklist.push_back("eye(?!.*(brow|lash|lid))");
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_LEFT, neck_or_head);
		if (bone_idx == -1) {
			WARN_PRINT("Auto Mapping couldn't guess LeftEye.");
		} else {
			p_bone_map->_set_skeleton_bone_name("LeftEye", skeleton->get_bone_name(bone_idx));
		}

		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_RIGHT, neck_or_head);
		if (bone_idx == -1) {
			WARN_PRINT("Auto Mapping couldn't guess RightEye.");
		} else {
			p_bone_map->_set_skeleton_bone_name("RightEye", skeleton->get_bone_name(bone_idx));
		}
		picklist.clear();

		// 7-2. Guess Jaw
		picklist.push_back("jaw");
		bone_idx = search_bone_by_name(skeleton, picklist, BONE_SEGREGATION_NONE, neck_or_head);
		if (bone_idx == -1) {
			WARN_PRINT("Auto Mapping couldn't guess Jaw.");
		} else {
			p_bone_map->_set_skeleton_bone_name("Jaw", skeleton->get_bone_name(bone_idx));
		}
		bone_idx = -1;
		picklist.clear();
	}

	// 8. Guess UpperChest or Chest
	if (neck_or_head == -1) {
		return; // Abort.
	}
	int chest_or_upper_chest = skeleton->get_bone_parent(neck_or_head);
	bool is_appropriate = true;
	if (left_shoulder != -1) {
		bone_idx = skeleton->get_bone_parent(left_shoulder);
		bool detect = false;
		while (bone_idx != hips && bone_idx >= 0) {
			if (bone_idx == chest_or_upper_chest) {
				detect = true;
				break;
			}
			bone_idx = skeleton->get_bone_parent(bone_idx);
		}
		if (!detect) {
			is_appropriate = false;
		}
		bone_idx = -1;
	}
	if (right_shoulder != -1) {
		bone_idx = skeleton->get_bone_parent(right_shoulder);
		bool detect = false;
		while (bone_idx != hips && bone_idx >= 0) {
			if (bone_idx == chest_or_upper_chest) {
				detect = true;
				break;
			}
			bone_idx = skeleton->get_bone_parent(bone_idx);
		}
		if (!detect) {
			is_appropriate = false;
		}
		bone_idx = -1;
	}
	if (!is_appropriate) {
		if (skeleton->get_bone_parent(left_shoulder) == skeleton->get_bone_parent(right_shoulder)) {
			chest_or_upper_chest = skeleton->get_bone_parent(left_shoulder);
		} else {
			chest_or_upper_chest = -1;
		}
	}
	if (chest_or_upper_chest == -1) {
		WARN_PRINT("Auto Mapping couldn't guess Chest or UpperChest. Abort auto mapping.");
		return; // Will be not able to guess Spines.
	}

	// 9. Guess Spines
	bone_idx = skeleton->get_bone_parent(chest_or_upper_chest);
	while (bone_idx != hips && bone_idx >= 0) {
		search_path.push_back(bone_idx);
		bone_idx = skeleton->get_bone_parent(bone_idx);
	}
	search_path.reverse();
	if (search_path.size() == 0) {
		p_bone_map->_set_skeleton_bone_name("Spine", skeleton->get_bone_name(chest_or_upper_chest)); // Maybe chibi model...?
	} else if (search_path.size() == 1) {
		p_bone_map->_set_skeleton_bone_name("Spine", skeleton->get_bone_name(search_path[0]));
		p_bone_map->_set_skeleton_bone_name("Chest", skeleton->get_bone_name(chest_or_upper_chest));
	} else if (search_path.size() >= 2) {
		p_bone_map->_set_skeleton_bone_name("Spine", skeleton->get_bone_name(search_path[0]));
		p_bone_map->_set_skeleton_bone_name("Chest", skeleton->get_bone_name(search_path[search_path.size() - 1])); // Probably UppeChest's parent is appropriate.
		p_bone_map->_set_skeleton_bone_name("UpperChest", skeleton->get_bone_name(chest_or_upper_chest));
	}
	bone_idx = -1;
	search_path.clear();

	WARN_PRINT("Finish auto mapping.");
}
#endif // MODULE_REGEX_ENABLED

void BoneMapper::_value_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing) {
	set(p_property, p_value);
	recreate_editor();
}

void BoneMapper::_profile_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing) {
	bone_map->set(p_property, p_value);

	// Run auto mapping when setting SkeletonProfileHumanoid by GUI Editor.
	Ref<SkeletonProfile> profile = bone_map->get_profile();
	if (profile.is_valid()) {
		SkeletonProfileHumanoid *hmn = Object::cast_to<SkeletonProfileHumanoid>(profile.ptr());
		if (hmn) {
#ifdef MODULE_REGEX_ENABLED
			_run_auto_mapping();
#endif // MODULE_REGEX_ENABLED
		}
	}
}

void BoneMapper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_current_group_idx", "current_group_idx"), &BoneMapper::set_current_group_idx);
	ClassDB::bind_method(D_METHOD("get_current_group_idx"), &BoneMapper::get_current_group_idx);
	ClassDB::bind_method(D_METHOD("set_current_bone_idx", "current_bone_idx"), &BoneMapper::set_current_bone_idx);
	ClassDB::bind_method(D_METHOD("get_current_bone_idx"), &BoneMapper::get_current_bone_idx);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_group_idx"), "set_current_group_idx", "get_current_group_idx");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_bone_idx"), "set_current_bone_idx", "get_current_bone_idx");
}

void BoneMapper::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			create_editor();
			bone_map->connect("bone_map_updated", callable_mp(this, &BoneMapper::_update_state));
			bone_map->connect("profile_updated", callable_mp(this, &BoneMapper::recreate_items));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			clear_items();
			if (bone_map.is_valid()) {
				if (bone_map->is_connected("bone_map_updated", callable_mp(this, &BoneMapper::_update_state))) {
					bone_map->disconnect("bone_map_updated", callable_mp(this, &BoneMapper::_update_state));
				}
				if (bone_map->is_connected("profile_updated", callable_mp(this, &BoneMapper::recreate_items))) {
					bone_map->disconnect("profile_updated", callable_mp(this, &BoneMapper::recreate_items));
				}
			}
		}
	}
}

BoneMapper::BoneMapper(Skeleton3D *p_skeleton, Ref<BoneMap> &p_bone_map) {
	skeleton = p_skeleton;
	bone_map = p_bone_map;
}

BoneMapper::~BoneMapper() {
}

void BoneMapEditor::create_editors() {
	if (!skeleton) {
		return;
	}
	bone_mapper = memnew(BoneMapper(skeleton, bone_map));
	add_child(bone_mapper);
}

void BoneMapEditor::fetch_objects() {
	skeleton = nullptr;
	// Hackey... but it may be the easiest way to get a selected object from "ImporterScene".
	SceneImportSettingsDialog *si = SceneImportSettingsDialog::get_singleton();
	if (!si) {
		return;
	}
	if (!si->is_visible()) {
		return;
	}
	Node *selected = si->get_selected_node();
	if (selected) {
		Skeleton3D *sk = Object::cast_to<Skeleton3D>(selected);
		if (!sk) {
			return;
		}
		skeleton = sk;
	} else {
		// Editor should not exist.
		skeleton = nullptr;
	}
}

void BoneMapEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			fetch_objects();
			create_editors();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			skeleton = nullptr;
		} break;
	}
}

BoneMapEditor::BoneMapEditor(Ref<BoneMap> &p_bone_map) {
	bone_map = p_bone_map;
}

BoneMapEditor::~BoneMapEditor() {
}

bool EditorInspectorPluginBoneMap::can_handle(Object *p_object) {
	return Object::cast_to<BoneMap>(p_object) != nullptr;
}

void EditorInspectorPluginBoneMap::parse_begin(Object *p_object) {
	BoneMap *bm = Object::cast_to<BoneMap>(p_object);
	if (!bm) {
		return;
	}
	Ref<BoneMap> r(bm);
	editor = memnew(BoneMapEditor(r));
	add_custom_control(editor);
}

BoneMapEditorPlugin::BoneMapEditorPlugin() {
	Ref<EditorInspectorPluginBoneMap> inspector_plugin;
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);

	Ref<PostImportPluginSkeletonTrackOrganizer> post_import_plugin_track_organizer;
	post_import_plugin_track_organizer.instantiate();
	add_scene_post_import_plugin(post_import_plugin_track_organizer);

	Ref<PostImportPluginSkeletonRenamer> post_import_plugin_renamer;
	post_import_plugin_renamer.instantiate();
	add_scene_post_import_plugin(post_import_plugin_renamer);

	Ref<PostImportPluginSkeletonRestFixer> post_import_plugin_rest_fixer;
	post_import_plugin_rest_fixer.instantiate();
	add_scene_post_import_plugin(post_import_plugin_rest_fixer);
}
