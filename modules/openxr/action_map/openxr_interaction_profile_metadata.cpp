/**************************************************************************/
/*  openxr_interaction_profile_metadata.cpp                               */
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

#include "openxr_interaction_profile_metadata.h"

#include "../openxr_api.h"

OpenXRInteractionProfileMetadata *OpenXRInteractionProfileMetadata::singleton = nullptr;

OpenXRInteractionProfileMetadata::OpenXRInteractionProfileMetadata() {
	singleton = this;

	_register_core_metadata();
	OpenXRAPI::register_extension_metadata();
}

OpenXRInteractionProfileMetadata::~OpenXRInteractionProfileMetadata() {
	singleton = nullptr;
}

void OpenXRInteractionProfileMetadata::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_profile_rename", "old_name", "new_name"), &OpenXRInteractionProfileMetadata::register_profile_rename);
	ClassDB::bind_method(D_METHOD("register_top_level_path", "display_name", "openxr_path", "openxr_extension_name"), &OpenXRInteractionProfileMetadata::register_top_level_path);
	ClassDB::bind_method(D_METHOD("register_interaction_profile", "display_name", "openxr_path", "openxr_extension_name"), &OpenXRInteractionProfileMetadata::register_interaction_profile);
	ClassDB::bind_method(D_METHOD("register_io_path", "interaction_profile", "display_name", "toplevel_path", "openxr_path", "openxr_extension_name", "action_type"), &OpenXRInteractionProfileMetadata::register_io_path);
}

void OpenXRInteractionProfileMetadata::register_profile_rename(const String &p_old_name, const String &p_new_name) {
	ERR_FAIL_COND(profile_renames.has(p_old_name));

	profile_renames[p_old_name] = p_new_name;
}

String OpenXRInteractionProfileMetadata::check_profile_name(const String &p_name) const {
	if (profile_renames.has(p_name)) {
		return profile_renames[p_name];
	}

	return p_name;
}

void OpenXRInteractionProfileMetadata::register_top_level_path(const String &p_display_name, const String &p_openxr_path, const String &p_openxr_extension_name) {
	ERR_FAIL_COND_MSG(has_top_level_path(p_openxr_path), p_openxr_path + " had already been registered");

	TopLevelPath new_toplevel_path = {
		p_display_name,
		p_openxr_path,
		p_openxr_extension_name
	};

	top_level_paths.push_back(new_toplevel_path);
}

void OpenXRInteractionProfileMetadata::register_interaction_profile(const String &p_display_name, const String &p_openxr_path, const String &p_openxr_extension_name) {
	ERR_FAIL_COND_MSG(has_interaction_profile(p_openxr_path), p_openxr_path + " has already been registered");

	InteractionProfile new_profile;
	new_profile.display_name = p_display_name;
	new_profile.openxr_path = p_openxr_path;
	new_profile.openxr_extension_name = p_openxr_extension_name;

	interaction_profiles.push_back(new_profile);
}

void OpenXRInteractionProfileMetadata::register_io_path(const String &p_interaction_profile, const String &p_display_name, const String &p_toplevel_path, const String &p_openxr_path, const String &p_openxr_extension_name, OpenXRAction::ActionType p_action_type) {
	ERR_FAIL_COND_MSG(!has_interaction_profile(p_interaction_profile), "Unknown interaction profile " + p_interaction_profile);
	ERR_FAIL_COND_MSG(!has_top_level_path(p_toplevel_path), "Unknown top level path " + p_toplevel_path);

	for (InteractionProfile &interaction_profile : interaction_profiles) {
		if (interaction_profile.openxr_path == p_interaction_profile) {
			ERR_FAIL_COND_MSG(interaction_profile.has_io_path(p_openxr_path), p_interaction_profile + " already has io path " + p_openxr_path + " registered!");

			IOPath new_io_path = {
				p_display_name,
				p_toplevel_path,
				p_openxr_path,
				p_openxr_extension_name,
				p_action_type
			};

			interaction_profile.io_paths.push_back(new_io_path);
		}
	}
}

bool OpenXRInteractionProfileMetadata::has_top_level_path(const String p_openxr_path) const {
	for (int i = 0; i < top_level_paths.size(); i++) {
		if (top_level_paths[i].openxr_path == p_openxr_path) {
			return true;
		}
	}

	return false;
}

String OpenXRInteractionProfileMetadata::get_top_level_name(const String p_openxr_path) const {
	for (int i = 0; i < top_level_paths.size(); i++) {
		if (top_level_paths[i].openxr_path == p_openxr_path) {
			return top_level_paths[i].display_name;
		}
	}

	return String();
}

String OpenXRInteractionProfileMetadata::get_top_level_extension(const String p_openxr_path) const {
	for (int i = 0; i < top_level_paths.size(); i++) {
		if (top_level_paths[i].openxr_path == p_openxr_path) {
			return top_level_paths[i].openxr_extension_name;
		}
	}

	return XR_PATH_UNSUPPORTED_NAME;
}

bool OpenXRInteractionProfileMetadata::has_interaction_profile(const String p_openxr_path) const {
	for (const InteractionProfile &interaction_profile : interaction_profiles) {
		if (interaction_profile.openxr_path == p_openxr_path) {
			return true;
		}
	}

	return false;
}

String OpenXRInteractionProfileMetadata::get_interaction_profile_extension(const String p_openxr_path) const {
	for (const InteractionProfile &interaction_profile : interaction_profiles) {
		if (interaction_profile.openxr_path == p_openxr_path) {
			return interaction_profile.openxr_extension_name;
		}
	}

	return XR_PATH_UNSUPPORTED_NAME;
}

const OpenXRInteractionProfileMetadata::InteractionProfile *OpenXRInteractionProfileMetadata::get_profile(const String p_openxr_path) const {
	for (const InteractionProfile &interaction_profile : interaction_profiles) {
		if (interaction_profile.openxr_path == p_openxr_path) {
			return &interaction_profile;
		}
	}

	return nullptr;
}

bool OpenXRInteractionProfileMetadata::InteractionProfile::has_io_path(const String p_io_path) const {
	for (int i = 0; i < io_paths.size(); i++) {
		if (io_paths[i].openxr_path == p_io_path) {
			return true;
		}
	}

	return false;
}

const OpenXRInteractionProfileMetadata::IOPath *OpenXRInteractionProfileMetadata::InteractionProfile::get_io_path(const String p_io_path) const {
	for (int i = 0; i < io_paths.size(); i++) {
		if (io_paths[i].openxr_path == p_io_path) {
			return &io_paths[i];
		}
	}

	return nullptr;
}

const OpenXRInteractionProfileMetadata::IOPath *OpenXRInteractionProfileMetadata::get_io_path(const String p_interaction_profile, const String p_io_path) const {
	const OpenXRInteractionProfileMetadata::InteractionProfile *profile = get_profile(p_interaction_profile);
	if (profile != nullptr) {
		return profile->get_io_path(p_io_path);
	}

	return nullptr;
}

PackedStringArray OpenXRInteractionProfileMetadata::get_interaction_profile_paths() const {
	PackedStringArray arr;

	for (const InteractionProfile &interaction_profile : interaction_profiles) {
		arr.push_back(interaction_profile.openxr_path);
	}

	return arr;
}

void OpenXRInteractionProfileMetadata::_register_core_metadata() {
	// Note, currently we add definitions that belong in extensions.
	// Extensions are registered when our OpenXRAPI is instantiated
	// however this does not happen in the editor.
	// We are changing this in another PR, once that is accepted we
	// can make the changes to move code into extensions where needed.

	// Note that we'll make an exception for XR_EXT_palm_pose, which is used everywhere

	// Our core toplevel paths
	register_top_level_path("Left hand controller", "/user/hand/left", "");
	register_top_level_path("Right hand controller", "/user/hand/right", "");
	register_top_level_path("Head", "/user/head", "");
	register_top_level_path("Gamepad", "/user/gamepad", "");
	register_top_level_path("Treadmill", "/user/treadmill", "");

	{ // Fallback Khronos simple controller
		const String profile_path = "/interaction_profiles/khr/simple_controller";
		register_interaction_profile("Simple controller", profile_path, "");
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			register_io_path(profile_path, "Menu click", user_path, user_path + "/input/menu/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Select click", user_path, user_path + "/input/select/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}
	}

	{ // Original HTC Vive wands
		const String profile_path = "/interaction_profiles/htc/vive_controller";
		register_interaction_profile("HTC Vive wand", profile_path, "");
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			register_io_path(profile_path, "Menu click", user_path, user_path + "/input/menu/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "System click", user_path, user_path + "/input/system/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			register_io_path(profile_path, "Trigger click", user_path, user_path + "/input/trigger/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Squeeze click", user_path, user_path + "/input/squeeze/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Trackpad", user_path, user_path + "/input/trackpad", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			register_io_path(profile_path, "Trackpad click", user_path, user_path + "/input/trackpad/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad touch", user_path, user_path + "/input/trackpad/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Up", user_path, user_path + "/input/trackpad/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Down", user_path, user_path + "/input/trackpad/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Left", user_path, user_path + "/input/trackpad/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Right", user_path, user_path + "/input/trackpad/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Center", user_path, user_path + "/input/trackpad/dpad_center", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}
	}

	{ // Microsoft motion controller (original WMR controllers)
		const String profile_path = "/interaction_profiles/microsoft/motion_controller";
		register_interaction_profile("MS Motion controller", profile_path, "");
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			register_io_path(profile_path, "Menu click", user_path, user_path + "/input/menu/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			register_io_path(profile_path, "Trigger click", user_path, user_path + "/input/trigger/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Squeeze click", user_path, user_path + "/input/squeeze/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Thumbstick", user_path, user_path + "/input/thumbstick", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			register_io_path(profile_path, "Thumbstick click", user_path, user_path + "/input/thumbstick/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Up", user_path, user_path + "/input/thumbstick/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Down", user_path, user_path + "/input/thumbstick/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Left", user_path, user_path + "/input/thumbstick/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Right", user_path, user_path + "/input/thumbstick/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Trackpad", user_path, user_path + "/input/trackpad", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			register_io_path(profile_path, "Trackpad click", user_path, user_path + "/input/trackpad/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad touch", user_path, user_path + "/input/trackpad/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Up", user_path, user_path + "/input/trackpad/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Down", user_path, user_path + "/input/trackpad/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Left", user_path, user_path + "/input/trackpad/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Right", user_path, user_path + "/input/trackpad/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Center", user_path, user_path + "/input/trackpad/dpad_center", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}
	}

	{ // Meta touch controller (original touch controllers, Quest 1 and Quest 2 controllers)
		const String profile_path = "/interaction_profiles/oculus/touch_controller";
		register_interaction_profile("Touch controller", profile_path, "");
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			register_io_path(profile_path, "Trigger touch", user_path, user_path + "/input/trigger/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Squeeze", user_path, user_path + "/input/squeeze/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);

			register_io_path(profile_path, "Thumbstick", user_path, user_path + "/input/thumbstick", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			register_io_path(profile_path, "Thumbstick click", user_path, user_path + "/input/thumbstick/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick touch", user_path, user_path + "/input/thumbstick/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Up", user_path, user_path + "/input/thumbstick/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Down", user_path, user_path + "/input/thumbstick/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Left", user_path, user_path + "/input/thumbstick/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Right", user_path, user_path + "/input/thumbstick/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Thumbrest touch", user_path, user_path + "/input/thumbrest/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}

		register_io_path(profile_path, "Menu click", "/user/hand/left", "/user/hand/left/input/menu/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		register_io_path(profile_path, "System click", "/user/hand/right", "/user/hand/right/input/system/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

		register_io_path(profile_path, "X click", "/user/hand/left", "/user/hand/left/input/x/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		register_io_path(profile_path, "X touch", "/user/hand/left", "/user/hand/left/input/x/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		register_io_path(profile_path, "Y click", "/user/hand/left", "/user/hand/left/input/y/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		register_io_path(profile_path, "Y touch", "/user/hand/left", "/user/hand/left/input/y/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		register_io_path(profile_path, "A click", "/user/hand/right", "/user/hand/right/input/a/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		register_io_path(profile_path, "A touch", "/user/hand/right", "/user/hand/right/input/a/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		register_io_path(profile_path, "B click", "/user/hand/right", "/user/hand/right/input/b/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		register_io_path(profile_path, "B touch", "/user/hand/right", "/user/hand/right/input/b/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
	}

	{ // Valve Index controller
		const String profile_path = "/interaction_profiles/valve/index_controller";
		register_interaction_profile("Index controller", profile_path, "");
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			register_io_path(profile_path, "System click", user_path, user_path + "/input/system/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "A click", user_path, user_path + "/input/a/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "A touch", user_path, user_path + "/input/a/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "B click", user_path, user_path + "/input/b/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "B touch", user_path, user_path + "/input/b/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			register_io_path(profile_path, "Trigger click", user_path, user_path + "/input/trigger/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trigger touch", user_path, user_path + "/input/trigger/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Squeeze", user_path, user_path + "/input/squeeze/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			register_io_path(profile_path, "Squeeze force", user_path, user_path + "/input/squeeze/force", "", OpenXRAction::OPENXR_ACTION_FLOAT);

			register_io_path(profile_path, "Thumbstick", user_path, user_path + "/input/thumbstick", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			register_io_path(profile_path, "Thumbstick click", user_path, user_path + "/input/thumbstick/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick touch", user_path, user_path + "/input/thumbstick/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Up", user_path, user_path + "/input/thumbstick/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Down", user_path, user_path + "/input/thumbstick/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Left", user_path, user_path + "/input/thumbstick/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Thumbstick Dpad Right", user_path, user_path + "/input/thumbstick/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Trackpad", user_path, user_path + "/input/trackpad", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			register_io_path(profile_path, "Trackpad force", user_path, user_path + "/input/trackpad/force", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			register_io_path(profile_path, "Trackpad touch", user_path, user_path + "/input/trackpad/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Up", user_path, user_path + "/input/trackpad/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Down", user_path, user_path + "/input/trackpad/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Left", user_path, user_path + "/input/trackpad/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Right", user_path, user_path + "/input/trackpad/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			register_io_path(profile_path, "Trackpad Dpad Center", user_path, user_path + "/input/trackpad/dpad_center", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}
	}
}
