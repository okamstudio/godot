/**************************************************************************/
/*  openxr_wmr_controller_extension.cpp                                   */
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

#include "openxr_wmr_controller_extension.h"

#include "../action_map/openxr_interaction_profile_metadata.h"

HashMap<String, bool *> OpenXRWMRControllerExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	// Note HP G2 is available on WMR and SteamVR, but Odessey is only available on WMR
	request_extensions[XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME] = &available[WMR_HPMR];
	request_extensions[XR_EXT_SAMSUNG_ODYSSEY_CONTROLLER_EXTENSION_NAME] = &available[WMR_SAMSUNG_ODESSY];
	request_extensions[XR_MSFT_HAND_INTERACTION_EXTENSION_NAME] = &available[WMR_HAND_INTERACTION];

	return request_extensions;
}

bool OpenXRWMRControllerExtension::is_available(WMRControllers p_type) {
	return available[p_type];
}

void OpenXRWMRControllerExtension::on_register_metadata() {
	OpenXRInteractionProfileMetadata *metadata = OpenXRInteractionProfileMetadata::get_singleton();
	ERR_FAIL_NULL(metadata);

	{ // HP MR controller (newer G2 controllers)
		const String profile_path = "/interaction_profiles/hp/mixed_reality_controller";
		metadata->register_interaction_profile("HPMR controller", profile_path, XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME);
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			metadata->register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			metadata->register_io_path(profile_path, "Menu click", user_path, user_path + "/input/menu/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			metadata->register_io_path(profile_path, "Trigger click", user_path, user_path + "/input/trigger/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Squeeze", user_path, user_path + "/input/squeeze/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);

			metadata->register_io_path(profile_path, "Thumbstick", user_path, user_path + "/input/thumbstick", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			metadata->register_io_path(profile_path, "Thumbstick click", user_path, user_path + "/input/thumbstick/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Thumbstick Dpad Up", user_path, user_path + "/input/thumbstick/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Thumbstick Dpad Down", user_path, user_path + "/input/thumbstick/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Thumbstick Dpad Left", user_path, user_path + "/input/thumbstick/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Thumbstick Dpad Right", user_path, user_path + "/input/thumbstick/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}

		metadata->register_io_path(profile_path, "X click", "/user/hand/left", "/user/hand/left/input/x/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		metadata->register_io_path(profile_path, "Y click", "/user/hand/left", "/user/hand/left/input/y/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		metadata->register_io_path(profile_path, "A click", "/user/hand/right", "/user/hand/right/input/a/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		metadata->register_io_path(profile_path, "B click", "/user/hand/right", "/user/hand/right/input/b/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
	}

	{ // Samsung Odyssey controller
		const String profile_path = "/interaction_profiles/samsung/odyssey_controller";
		metadata->register_interaction_profile("Samsung Odyssey controller", profile_path, XR_EXT_SAMSUNG_ODYSSEY_CONTROLLER_EXTENSION_NAME);
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			metadata->register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			metadata->register_io_path(profile_path, "Menu click", user_path, user_path + "/input/menu/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			metadata->register_io_path(profile_path, "Trigger click", user_path, user_path + "/input/trigger/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Squeeze click", user_path, user_path + "/input/squeeze/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Thumbstick", user_path, user_path + "/input/thumbstick", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			metadata->register_io_path(profile_path, "Thumbstick click", user_path, user_path + "/input/thumbstick/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Thumbstick Dpad Up", user_path, user_path + "/input/thumbstick/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Thumbstick Dpad Down", user_path, user_path + "/input/thumbstick/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Thumbstick Dpad Left", user_path, user_path + "/input/thumbstick/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Thumbstick Dpad Right", user_path, user_path + "/input/thumbstick/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Trackpad", user_path, user_path + "/input/trackpad", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			metadata->register_io_path(profile_path, "Trackpad click", user_path, user_path + "/input/trackpad/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Trackpad touch", user_path, user_path + "/input/trackpad/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Trackpad Dpad Up", user_path, user_path + "/input/trackpad/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Trackpad Dpad Down", user_path, user_path + "/input/trackpad/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Trackpad Dpad Left", user_path, user_path + "/input/trackpad/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Trackpad Dpad Right", user_path, user_path + "/input/trackpad/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Trackpad Dpad Center", user_path, user_path + "/input/trackpad/dpad_center", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}
	}

	{ // MSFT Hand interaction profile, also supported by other headsets
		const String profile_path = "/interaction_profiles/microsoft/hand_interaction";
		metadata->register_interaction_profile("MSFT Hand interaction", profile_path, XR_MSFT_HAND_INTERACTION_EXTENSION_NAME);
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			metadata->register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Pinch pose", user_path, user_path + "/input/pinch_ext/pose", XR_EXT_HAND_INTERACTION_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Poke pose", user_path, user_path + "/input/poke_ext/pose", XR_EXT_HAND_INTERACTION_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			metadata->register_io_path(profile_path, "Select (pinch)", user_path, user_path + "/input/select/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);

			metadata->register_io_path(profile_path, "Squeeze (grab)", user_path, user_path + "/input/squeeze/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
		}
	}
}
