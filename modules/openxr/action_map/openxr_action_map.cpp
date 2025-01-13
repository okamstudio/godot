/**************************************************************************/
/*  openxr_action_map.cpp                                                 */
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

#include "openxr_action_map.h"

void OpenXRActionMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action_sets", "action_sets"), &OpenXRActionMap::set_action_sets);
	ClassDB::bind_method(D_METHOD("get_action_sets"), &OpenXRActionMap::get_action_sets);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "action_sets", PROPERTY_HINT_RESOURCE_TYPE, "OpenXRActionSet", PROPERTY_USAGE_NO_EDITOR), "set_action_sets", "get_action_sets");

	ClassDB::bind_method(D_METHOD("get_action_set_count"), &OpenXRActionMap::get_action_set_count);
	ClassDB::bind_method(D_METHOD("find_action_set", "name"), &OpenXRActionMap::find_action_set);
	ClassDB::bind_method(D_METHOD("get_action_set", "idx"), &OpenXRActionMap::get_action_set);
	ClassDB::bind_method(D_METHOD("add_action_set", "action_set"), &OpenXRActionMap::add_action_set);
	ClassDB::bind_method(D_METHOD("remove_action_set", "action_set"), &OpenXRActionMap::remove_action_set);

	ClassDB::bind_method(D_METHOD("set_interaction_profiles", "interaction_profiles"), &OpenXRActionMap::set_interaction_profiles);
	ClassDB::bind_method(D_METHOD("get_interaction_profiles"), &OpenXRActionMap::get_interaction_profiles);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "interaction_profiles", PROPERTY_HINT_RESOURCE_TYPE, "OpenXRInteractionProfile", PROPERTY_USAGE_NO_EDITOR), "set_interaction_profiles", "get_interaction_profiles");

	ClassDB::bind_method(D_METHOD("get_interaction_profile_count"), &OpenXRActionMap::get_interaction_profile_count);
	ClassDB::bind_method(D_METHOD("find_interaction_profile", "name"), &OpenXRActionMap::find_interaction_profile);
	ClassDB::bind_method(D_METHOD("get_interaction_profile", "idx"), &OpenXRActionMap::get_interaction_profile);
	ClassDB::bind_method(D_METHOD("add_interaction_profile", "interaction_profile"), &OpenXRActionMap::add_interaction_profile);
	ClassDB::bind_method(D_METHOD("remove_interaction_profile", "interaction_profile"), &OpenXRActionMap::remove_interaction_profile);

	ClassDB::bind_method(D_METHOD("create_default_action_sets"), &OpenXRActionMap::create_default_action_sets);
}

void OpenXRActionMap::set_action_sets(Array p_action_sets) {
	action_sets.clear();

	for (int i = 0; i < p_action_sets.size(); i++) {
		Ref<OpenXRActionSet> action_set = p_action_sets[i];
		if (action_set.is_valid() && !action_sets.has(action_set)) {
			action_sets.push_back(action_set);
		}
	}
}

Array OpenXRActionMap::get_action_sets() const {
	return action_sets;
}

int OpenXRActionMap::get_action_set_count() const {
	return action_sets.size();
}

Ref<OpenXRActionSet> OpenXRActionMap::find_action_set(String p_name) const {
	for (int i = 0; i < action_sets.size(); i++) {
		Ref<OpenXRActionSet> action_set = action_sets[i];
		if (action_set->get_name() == p_name) {
			return action_set;
		}
	}

	return Ref<OpenXRActionSet>();
}

Ref<OpenXRActionSet> OpenXRActionMap::get_action_set(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, action_sets.size(), Ref<OpenXRActionSet>());

	return action_sets[p_idx];
}

void OpenXRActionMap::add_action_set(Ref<OpenXRActionSet> p_action_set) {
	ERR_FAIL_COND(p_action_set.is_null());

	if (!action_sets.has(p_action_set)) {
		action_sets.push_back(p_action_set);
		emit_changed();
	}
}

void OpenXRActionMap::remove_action_set(Ref<OpenXRActionSet> p_action_set) {
	int idx = action_sets.find(p_action_set);
	if (idx != -1) {
		action_sets.remove_at(idx);
		emit_changed();
	}
}

void OpenXRActionMap::clear_interaction_profiles() {
	if (interaction_profiles.is_empty()) {
		return;
	}

	// Interaction profiles held within our action map set should be released and destroyed but just in case they are still used some where else.
	for (Ref<OpenXRInteractionProfile> interaction_profile : interaction_profiles) {
		interaction_profile->action_map = nullptr;
	}
	interaction_profiles.clear();
	emit_changed();
}

void OpenXRActionMap::set_interaction_profiles(Array p_interaction_profiles) {
	clear_interaction_profiles();

	for (const Variant &interaction_profile : p_interaction_profiles) {
		// Add them anew so we verify our interaction profile pointer.
		add_interaction_profile(interaction_profile);
	}
}

Array OpenXRActionMap::get_interaction_profiles() const {
	return interaction_profiles;
}

int OpenXRActionMap::get_interaction_profile_count() const {
	return interaction_profiles.size();
}

Ref<OpenXRInteractionProfile> OpenXRActionMap::find_interaction_profile(String p_path) const {
	for (Ref<OpenXRInteractionProfile> interaction_profile : interaction_profiles) {
		if (interaction_profile->get_interaction_profile_path() == p_path) {
			return interaction_profile;
		}
	}

	return Ref<OpenXRInteractionProfile>();
}

Ref<OpenXRInteractionProfile> OpenXRActionMap::get_interaction_profile(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, interaction_profiles.size(), Ref<OpenXRInteractionProfile>());

	return interaction_profiles[p_idx];
}

void OpenXRActionMap::add_interaction_profile(Ref<OpenXRInteractionProfile> p_interaction_profile) {
	ERR_FAIL_COND(p_interaction_profile.is_null());

	if (!interaction_profiles.has(p_interaction_profile)) {
		if (p_interaction_profile->action_map && p_interaction_profile->action_map != this) {
			// Interaction profiles should only relate to our action map.
			p_interaction_profile->action_map->remove_interaction_profile(p_interaction_profile);
		}

		p_interaction_profile->action_map = this;

		interaction_profiles.push_back(p_interaction_profile);
		emit_changed();
	}
}

void OpenXRActionMap::remove_interaction_profile(Ref<OpenXRInteractionProfile> p_interaction_profile) {
	int idx = interaction_profiles.find(p_interaction_profile);
	if (idx != -1) {
		interaction_profiles.remove_at(idx);

		ERR_FAIL_COND_MSG(p_interaction_profile->action_map != this, "Removing interaction profile that belongs to this action map but had incorrect action map pointer."); // This should never happen!
		p_interaction_profile->action_map = nullptr;

		emit_changed();
	}
}

void OpenXRActionMap::create_default_action_sets() {
	// Note:
	// - if you make changes here make sure to delete your default_action_map.tres file of it will load an old version.
	// - our palm pose is only available if the relevant extension is supported,
	//   we still want it to be part of our action map as we may deploy the same game to platforms that do and don't support it.
	// - the same applies for interaction profiles that are only supported if the relevant extension is supported.

	// Create our Godot action set.
	Ref<OpenXRActionSet> action_set = OpenXRActionSet::new_action_set("godot", "Godot action set");
	add_action_set(action_set);

	// Create our actions.
	Ref<OpenXRAction> trigger = action_set->add_new_action("trigger", "Trigger", OpenXRAction::OPENXR_ACTION_FLOAT, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> trigger_click = action_set->add_new_action("trigger_click", "Trigger click", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> trigger_touch = action_set->add_new_action("trigger_touch", "Trigger touching", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> grip = action_set->add_new_action("grip", "Grip", OpenXRAction::OPENXR_ACTION_FLOAT, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> grip_click = action_set->add_new_action("grip_click", "Grip click", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> grip_force = action_set->add_new_action("grip_force", "Grip force", OpenXRAction::OPENXR_ACTION_FLOAT, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> primary = action_set->add_new_action("primary", "Primary joystick/thumbstick/trackpad", OpenXRAction::OPENXR_ACTION_VECTOR2, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> primary_click = action_set->add_new_action("primary_click", "Primary joystick/thumbstick/trackpad click", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> primary_touch = action_set->add_new_action("primary_touch", "Primary joystick/thumbstick/trackpad touching", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> secondary = action_set->add_new_action("secondary", "Secondary joystick/thumbstick/trackpad", OpenXRAction::OPENXR_ACTION_VECTOR2, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> secondary_click = action_set->add_new_action("secondary_click", "Secondary joystick/thumbstick/trackpad click", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> secondary_touch = action_set->add_new_action("secondary_touch", "Secondary joystick/thumbstick/trackpad touching", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> menu_button = action_set->add_new_action("menu_button", "Menu button", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> select_button = action_set->add_new_action("select_button", "Select button", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> ax_button = action_set->add_new_action("ax_button", "A/X button", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> ax_touch = action_set->add_new_action("ax_touch", "A/X touching", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> by_button = action_set->add_new_action("by_button", "B/Y button", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> by_touch = action_set->add_new_action("by_touch", "B/Y touching", OpenXRAction::OPENXR_ACTION_BOOL, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> default_pose = action_set->add_new_action("default_pose", "Default pose", OpenXRAction::OPENXR_ACTION_POSE,
			"/user/hand/left,"
			"/user/hand/right,"
			// "/user/vive_tracker_htcx/role/handheld_object," <-- getting errors on this one.
			"/user/vive_tracker_htcx/role/left_foot,"
			"/user/vive_tracker_htcx/role/right_foot,"
			"/user/vive_tracker_htcx/role/left_shoulder,"
			"/user/vive_tracker_htcx/role/right_shoulder,"
			"/user/vive_tracker_htcx/role/left_elbow,"
			"/user/vive_tracker_htcx/role/right_elbow,"
			"/user/vive_tracker_htcx/role/left_knee,"
			"/user/vive_tracker_htcx/role/right_knee,"
			"/user/vive_tracker_htcx/role/waist,"
			"/user/vive_tracker_htcx/role/chest,"
			"/user/vive_tracker_htcx/role/camera,"
			"/user/vive_tracker_htcx/role/keyboard,"
			"/user/eyes_ext");
	Ref<OpenXRAction> aim_pose = action_set->add_new_action("aim_pose", "Aim pose", OpenXRAction::OPENXR_ACTION_POSE, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> grip_pose = action_set->add_new_action("grip_pose", "Grip pose", OpenXRAction::OPENXR_ACTION_POSE, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> palm_pose = action_set->add_new_action("palm_pose", "Palm pose", OpenXRAction::OPENXR_ACTION_POSE, "/user/hand/left,/user/hand/right");
	Ref<OpenXRAction> haptic = action_set->add_new_action("haptic", "Haptic", OpenXRAction::OPENXR_ACTION_HAPTIC,
			"/user/hand/left,"
			"/user/hand/right,"
			// "/user/vive_tracker_htcx/role/handheld_object," <-- getting errors on this one.
			"/user/vive_tracker_htcx/role/left_foot,"
			"/user/vive_tracker_htcx/role/right_foot,"
			"/user/vive_tracker_htcx/role/left_shoulder,"
			"/user/vive_tracker_htcx/role/right_shoulder,"
			"/user/vive_tracker_htcx/role/left_elbow,"
			"/user/vive_tracker_htcx/role/right_elbow,"
			"/user/vive_tracker_htcx/role/left_knee,"
			"/user/vive_tracker_htcx/role/right_knee,"
			"/user/vive_tracker_htcx/role/waist,"
			"/user/vive_tracker_htcx/role/chest,"
			"/user/vive_tracker_htcx/role/camera,"
			"/user/vive_tracker_htcx/role/keyboard");

	// Create our interaction profiles.
	Ref<OpenXRInteractionProfile> profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/khr/simple_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click,/user/hand/right/input/menu/click");
	profile->add_new_binding(select_button, "/user/hand/left/input/select/click,/user/hand/right/input/select/click");
	// generic has no support for triggers, grip, A/B buttons, nor joystick/trackpad inputs.
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our Vive controller profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/htc/vive_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click,/user/hand/right/input/menu/click");
	profile->add_new_binding(select_button, "/user/hand/left/input/system/click,/user/hand/right/input/system/click");
	// wmr controller has no a/b/x/y buttons.
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/click,/user/hand/right/input/trigger/click");
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click"); // OpenXR will convert bool to float.
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click");
	// primary on our vive controller is our trackpad.
	profile->add_new_binding(primary, "/user/hand/left/input/trackpad,/user/hand/right/input/trackpad");
	profile->add_new_binding(primary_click, "/user/hand/left/input/trackpad/click,/user/hand/right/input/trackpad/click");
	profile->add_new_binding(primary_touch, "/user/hand/left/input/trackpad/touch,/user/hand/right/input/trackpad/touch");
	// vive controllers have no secondary input.
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our WMR controller profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/microsoft/motion_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	// wmr controllers have no select button we can use.
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click,/user/hand/right/input/menu/click");
	// wmr controller has no a/b/x/y buttons.
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value"); // OpenXR will convert float to bool.
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click"); // OpenXR will convert bool to float.
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click");
	// primary on our wmr controller is our thumbstick, no touch.
	profile->add_new_binding(primary, "/user/hand/left/input/thumbstick,/user/hand/right/input/thumbstick");
	profile->add_new_binding(primary_click, "/user/hand/left/input/thumbstick/click,/user/hand/right/input/thumbstick/click");
	// secondary on our wmr controller is our trackpad.
	profile->add_new_binding(secondary, "/user/hand/left/input/trackpad,/user/hand/right/input/trackpad");
	profile->add_new_binding(secondary_click, "/user/hand/left/input/trackpad/click,/user/hand/right/input/trackpad/click");
	profile->add_new_binding(secondary_touch, "/user/hand/left/input/trackpad/touch,/user/hand/right/input/trackpad/touch");
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our Meta touch controller profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/oculus/touch_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	// touch controllers have no select button we can use.
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click,/user/hand/right/input/system/click"); // right hand system click may not be available.
	profile->add_new_binding(ax_button, "/user/hand/left/input/x/click,/user/hand/right/input/a/click"); // x on left hand, a on right hand.
	profile->add_new_binding(ax_touch, "/user/hand/left/input/x/touch,/user/hand/right/input/a/touch");
	profile->add_new_binding(by_button, "/user/hand/left/input/y/click,/user/hand/right/input/b/click"); // y on left hand, b on right hand.
	profile->add_new_binding(by_touch, "/user/hand/left/input/y/touch,/user/hand/right/input/b/touch");
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value"); // should be converted to boolean.
	profile->add_new_binding(trigger_touch, "/user/hand/left/input/trigger/touch,/user/hand/right/input/trigger/touch");
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/value,/user/hand/right/input/squeeze/value"); // should be converted to boolean.
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/value,/user/hand/right/input/squeeze/value");
	// primary on our touch controller is our thumbstick.
	profile->add_new_binding(primary, "/user/hand/left/input/thumbstick,/user/hand/right/input/thumbstick");
	profile->add_new_binding(primary_click, "/user/hand/left/input/thumbstick/click,/user/hand/right/input/thumbstick/click");
	profile->add_new_binding(primary_touch, "/user/hand/left/input/thumbstick/touch,/user/hand/right/input/thumbstick/touch");
	// touch controller has no secondary input.
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our Pico 4 controller profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/bytedance/pico4_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	profile->add_new_binding(select_button, "/user/hand/left/input/system/click,/user/hand/right/input/system/click"); // system click may not be available.
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click");
	profile->add_new_binding(ax_button, "/user/hand/left/input/x/click,/user/hand/right/input/a/click"); // x on left hand, a on right hand.
	profile->add_new_binding(ax_touch, "/user/hand/left/input/x/touch,/user/hand/right/input/a/touch");
	profile->add_new_binding(by_button, "/user/hand/left/input/y/click,/user/hand/right/input/b/click"); // y on left hand, b on right hand.
	profile->add_new_binding(by_touch, "/user/hand/left/input/y/touch,/user/hand/right/input/b/touch");
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value"); // should be converted to boolean.
	profile->add_new_binding(trigger_touch, "/user/hand/left/input/trigger/touch,/user/hand/right/input/trigger/touch");
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/value,/user/hand/right/input/squeeze/value"); // should be converted to boolean.
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/value,/user/hand/right/input/squeeze/value");
	// primary on our pico controller is our thumbstick.
	profile->add_new_binding(primary, "/user/hand/left/input/thumbstick,/user/hand/right/input/thumbstick");
	profile->add_new_binding(primary_click, "/user/hand/left/input/thumbstick/click,/user/hand/right/input/thumbstick/click");
	profile->add_new_binding(primary_touch, "/user/hand/left/input/thumbstick/touch,/user/hand/right/input/thumbstick/touch");
	// pico controller has no secondary input.
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our Valve index controller profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/valve/index_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	// index controllers have no select button we can use.
	profile->add_new_binding(menu_button, "/user/hand/left/input/system/click,/user/hand/right/input/system/click");
	profile->add_new_binding(ax_button, "/user/hand/left/input/a/click,/user/hand/right/input/a/click"); // a on both controllers.
	profile->add_new_binding(ax_touch, "/user/hand/left/input/a/touch,/user/hand/right/input/a/touch");
	profile->add_new_binding(by_button, "/user/hand/left/input/b/click,/user/hand/right/input/b/click"); // b on both controllers.
	profile->add_new_binding(by_touch, "/user/hand/left/input/b/touch,/user/hand/right/input/b/touch");
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/click,/user/hand/right/input/trigger/click");
	profile->add_new_binding(trigger_touch, "/user/hand/left/input/trigger/touch,/user/hand/right/input/trigger/touch");
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/value,/user/hand/right/input/squeeze/value");
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/value,/user/hand/right/input/squeeze/value"); // this should do a float to bool conversion.
	profile->add_new_binding(grip_force, "/user/hand/left/input/squeeze/force,/user/hand/right/input/squeeze/force"); // grip force seems to be unique to the Valve Index.
	// primary on our index controller is our thumbstick.
	profile->add_new_binding(primary, "/user/hand/left/input/thumbstick,/user/hand/right/input/thumbstick");
	profile->add_new_binding(primary_click, "/user/hand/left/input/thumbstick/click,/user/hand/right/input/thumbstick/click");
	profile->add_new_binding(primary_touch, "/user/hand/left/input/thumbstick/touch,/user/hand/right/input/thumbstick/touch");
	// secondary on our index controller is our trackpad.
	profile->add_new_binding(secondary, "/user/hand/left/input/trackpad,/user/hand/right/input/trackpad");
	profile->add_new_binding(secondary_click, "/user/hand/left/input/trackpad/force,/user/hand/right/input/trackpad/force"); // not sure if this will work but doesn't seem to support click...
	profile->add_new_binding(secondary_touch, "/user/hand/left/input/trackpad/touch,/user/hand/right/input/trackpad/touch");
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our HP MR controller profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/hp/mixed_reality_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	// hpmr controllers have no select button we can use.
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click,/user/hand/right/input/menu/click");
	// hpmr controllers only register click, not touch, on our a/b/x/y buttons.
	profile->add_new_binding(ax_button, "/user/hand/left/input/x/click,/user/hand/right/input/a/click"); // x on left hand, a on right hand.
	profile->add_new_binding(by_button, "/user/hand/left/input/y/click,/user/hand/right/input/b/click"); // y on left hand, b on right hand.
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/value,/user/hand/right/input/squeeze/value");
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/value,/user/hand/right/input/squeeze/value");
	// primary on our hpmr controller is our thumbstick.
	profile->add_new_binding(primary, "/user/hand/left/input/thumbstick,/user/hand/right/input/thumbstick");
	profile->add_new_binding(primary_click, "/user/hand/left/input/thumbstick/click,/user/hand/right/input/thumbstick/click");
	// No secondary on our hpmr controller.
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our Samsung Odyssey controller profile,
	// Note that this controller is only identified specifically on WMR, on SteamVR this is identified as a normal WMR controller.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/samsung/odyssey_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	// Odyssey controllers have no select button we can use.
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click,/user/hand/right/input/menu/click");
	// Odyssey controller has no a/b/x/y buttons.
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click");
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click");
	// primary on our Odyssey controller is our thumbstick, no touch.
	profile->add_new_binding(primary, "/user/hand/left/input/thumbstick,/user/hand/right/input/thumbstick");
	profile->add_new_binding(primary_click, "/user/hand/left/input/thumbstick/click,/user/hand/right/input/thumbstick/click");
	// secondary on our Odyssey controller is our trackpad.
	profile->add_new_binding(secondary, "/user/hand/left/input/trackpad,/user/hand/right/input/trackpad");
	profile->add_new_binding(secondary_click, "/user/hand/left/input/trackpad/click,/user/hand/right/input/trackpad/click");
	profile->add_new_binding(secondary_touch, "/user/hand/left/input/trackpad/touch,/user/hand/right/input/trackpad/touch");
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our Vive Cosmos controller.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/htc/vive_cosmos_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click");
	profile->add_new_binding(select_button, "/user/hand/right/input/system/click"); // we'll map system to select.
	profile->add_new_binding(ax_button, "/user/hand/left/input/x/click,/user/hand/right/input/a/click"); // x on left hand, a on right hand.
	profile->add_new_binding(by_button, "/user/hand/left/input/y/click,/user/hand/right/input/b/click"); // y on left hand, b on right hand.
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/click,/user/hand/right/input/trigger/click");
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click");
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click");
	// primary on our Cosmos controller is our thumbstick.
	profile->add_new_binding(primary, "/user/hand/left/input/thumbstick,/user/hand/right/input/thumbstick");
	profile->add_new_binding(primary_click, "/user/hand/left/input/thumbstick/click,/user/hand/right/input/thumbstick/click");
	profile->add_new_binding(primary_touch, "/user/hand/left/input/thumbstick/touch,/user/hand/right/input/thumbstick/touch");
	// No secondary on our cosmos controller.
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our Vive Focus 3 controller.
	// Note, Vive Focus 3 currently is not yet supported as a stand alone device
	// however HTC currently has a beta OpenXR runtime in testing we may support in the near future.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/htc/vive_focus3_controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	profile->add_new_binding(menu_button, "/user/hand/left/input/menu/click");
	profile->add_new_binding(select_button, "/user/hand/right/input/system/click"); // we'll map system to select.
	profile->add_new_binding(ax_button, "/user/hand/left/input/x/click,/user/hand/right/input/a/click"); // x on left hand, a on right hand.
	profile->add_new_binding(by_button, "/user/hand/left/input/y/click,/user/hand/right/input/b/click"); // y on left hand, b on right hand.
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/click,/user/hand/right/input/trigger/click");
	profile->add_new_binding(trigger_touch, "/user/hand/left/input/trigger/touch,/user/hand/right/input/trigger/touch");
	profile->add_new_binding(grip, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click");
	profile->add_new_binding(grip_click, "/user/hand/left/input/squeeze/click,/user/hand/right/input/squeeze/click");
	// primary on our Focus 3 controller is our thumbstick.
	profile->add_new_binding(primary, "/user/hand/left/input/thumbstick,/user/hand/right/input/thumbstick");
	profile->add_new_binding(primary_click, "/user/hand/left/input/thumbstick/click,/user/hand/right/input/thumbstick/click");
	profile->add_new_binding(primary_touch, "/user/hand/left/input/thumbstick/touch,/user/hand/right/input/thumbstick/touch");
	// We only have a thumb rest.
	profile->add_new_binding(secondary_touch, "/user/hand/left/input/thumbrest/touch,/user/hand/right/input/thumbrest/touch");
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our Huawei controller.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/huawei/controller");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");
	profile->add_new_binding(menu_button, "/user/hand/left/input/home/click,/user/hand/right/input/home/click");
	profile->add_new_binding(trigger, "/user/hand/left/input/trigger/value,/user/hand/right/input/trigger/value");
	profile->add_new_binding(trigger_click, "/user/hand/left/input/trigger/click,/user/hand/right/input/trigger/click");
	// primary on our Huawei controller is our trackpad.
	profile->add_new_binding(primary, "/user/hand/left/input/trackpad,/user/hand/right/input/trackpad");
	profile->add_new_binding(primary_click, "/user/hand/left/input/trackpad/click,/user/hand/right/input/trackpad/click");
	profile->add_new_binding(primary_touch, "/user/hand/left/input/trackpad/touch,/user/hand/right/input/trackpad/touch");
	profile->add_new_binding(haptic, "/user/hand/left/output/haptic,/user/hand/right/output/haptic");
	add_interaction_profile(profile);

	// Create our HTC Vive tracker profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/htc/vive_tracker_htcx");
	profile->add_new_binding(default_pose,
			// "/user/vive_tracker_htcx/role/handheld_object/input/grip/pose," <-- getting errors on this one.
			"/user/vive_tracker_htcx/role/left_foot/input/grip/pose,"
			"/user/vive_tracker_htcx/role/right_foot/input/grip/pose,"
			"/user/vive_tracker_htcx/role/left_shoulder/input/grip/pose,"
			"/user/vive_tracker_htcx/role/right_shoulder/input/grip/pose,"
			"/user/vive_tracker_htcx/role/left_elbow/input/grip/pose,"
			"/user/vive_tracker_htcx/role/right_elbow/input/grip/pose,"
			"/user/vive_tracker_htcx/role/left_knee/input/grip/pose,"
			"/user/vive_tracker_htcx/role/right_knee/input/grip/pose,"
			"/user/vive_tracker_htcx/role/waist/input/grip/pose,"
			"/user/vive_tracker_htcx/role/chest/input/grip/pose,"
			"/user/vive_tracker_htcx/role/camera/input/grip/pose,"
			"/user/vive_tracker_htcx/role/keyboard/input/grip/pose");
	profile->add_new_binding(haptic,
			// "/user/vive_tracker_htcx/role/handheld_object/output/haptic," <-- getting errors on this one.
			"/user/vive_tracker_htcx/role/left_foot/output/haptic,"
			"/user/vive_tracker_htcx/role/right_foot/output/haptic,"
			"/user/vive_tracker_htcx/role/left_shoulder/output/haptic,"
			"/user/vive_tracker_htcx/role/right_shoulder/output/haptic,"
			"/user/vive_tracker_htcx/role/left_elbow/output/haptic,"
			"/user/vive_tracker_htcx/role/right_elbow/output/haptic,"
			"/user/vive_tracker_htcx/role/left_knee/output/haptic,"
			"/user/vive_tracker_htcx/role/right_knee/output/haptic,"
			"/user/vive_tracker_htcx/role/waist/output/haptic,"
			"/user/vive_tracker_htcx/role/chest/output/haptic,"
			"/user/vive_tracker_htcx/role/camera/output/haptic,"
			"/user/vive_tracker_htcx/role/keyboard/output/haptic");
	add_interaction_profile(profile);

	// Create our eye gaze interaction profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/ext/eye_gaze_interaction");
	profile->add_new_binding(default_pose, "/user/eyes_ext/input/gaze_ext/pose");
	add_interaction_profile(profile);

	// Create our hand interaction profile.
	profile = OpenXRInteractionProfile::new_profile("/interaction_profiles/ext/hand_interaction_ext");
	profile->add_new_binding(default_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(aim_pose, "/user/hand/left/input/aim/pose,/user/hand/right/input/aim/pose");
	profile->add_new_binding(grip_pose, "/user/hand/left/input/grip/pose,/user/hand/right/input/grip/pose");
	profile->add_new_binding(palm_pose, "/user/hand/left/input/palm_ext/pose,/user/hand/right/input/palm_ext/pose");

	// Use pinch as primary.
	profile->add_new_binding(primary, "/user/hand/left/input/pinch_ext/value,/user/hand/right/input/pinch_ext/value");
	profile->add_new_binding(primary_click, "/user/hand/left/input/pinch_ext/ready_ext,/user/hand/right/input/pinch_ext/ready_ext");

	// Use activation as secondary.
	profile->add_new_binding(secondary, "/user/hand/left/input/aim_activate_ext/value,/user/hand/right/input/aim_activate_ext/value");
	profile->add_new_binding(secondary_click, "/user/hand/left/input/aim_activate_ext/ready_ext,/user/hand/right/input/aim_activate_ext/ready_ext");

	// We link grasp to our grip.
	profile->add_new_binding(grip, "/user/hand/left/input/grasp_ext/value,/user/hand/right/input/grasp_ext/value");
	profile->add_new_binding(grip_click, "/user/hand/left/input/grasp_ext/ready_ext,/user/hand/right/input/grasp_ext/ready_ext");
	add_interaction_profile(profile);
}

void OpenXRActionMap::create_editor_action_sets() {
	// TODO implement
}

Ref<OpenXRAction> OpenXRActionMap::get_action(const String p_path) const {
	PackedStringArray paths = p_path.split("/", false);
	ERR_FAIL_COND_V(paths.size() != 2, Ref<OpenXRAction>());

	Ref<OpenXRActionSet> action_set = find_action_set(paths[0]);
	if (action_set.is_valid()) {
		return action_set->get_action(paths[1]);
	}

	return Ref<OpenXRAction>();
}

void OpenXRActionMap::remove_action(const String p_path, bool p_remove_interaction_profiles) {
	Ref<OpenXRAction> action = get_action(p_path);
	if (action.is_valid()) {
		for (Ref<OpenXRInteractionProfile> interaction_profile : interaction_profiles) {
			if (p_remove_interaction_profiles) {
				// Remove any bindings for this action
				interaction_profile->remove_binding_for_action(action);
			} else {
				ERR_FAIL_COND(interaction_profile->has_binding_for_action(action));
			}
		}

		OpenXRActionSet *action_set = action->get_action_set();
		if (action_set != nullptr) {
			// Remove the action from this action set
			action_set->remove_action(action);
		}
	}
}

PackedStringArray OpenXRActionMap::get_top_level_paths(const Ref<OpenXRAction> p_action) {
	PackedStringArray arr;

	for (Ref<OpenXRInteractionProfile> ip : interaction_profiles) {
		const OpenXRInteractionProfileMetadata::InteractionProfile *profile = OpenXRInteractionProfileMetadata::get_singleton()->get_profile(ip->get_interaction_profile_path());

		if (profile != nullptr) {
			Vector<Ref<OpenXRIPBinding>> bindings = ip->get_bindings_for_action(p_action);
			for (const Ref<OpenXRIPBinding> &binding : bindings) {
				String binding_path = binding->get_binding_path();
				const OpenXRInteractionProfileMetadata::IOPath *io_path = profile->get_io_path(binding_path);
				if (io_path != nullptr) {
					String top_path = io_path->top_level_path;

					if (!arr.has(top_path)) {
						arr.push_back(top_path);
					}
				}
			}
		}
	}

	// print_line("Toplevel paths for", p_action->get_name_with_set(), "are", arr);

	return arr;
}

OpenXRActionMap::~OpenXRActionMap() {
	action_sets.clear();
	clear_interaction_profiles();
}
