/**************************************************************************/
/*  openxr_pico_controller_extension.cpp                                  */
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

#include "openxr_pico_controller_extension.h"

#include "../action_map/openxr_interaction_profile_metadata.h"

HashMap<String, bool *> OpenXRPicoControllerExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	// Note, this used to be XR_PICO_controller_interaction but that has since been retired
	// and was never part of the OpenXX specification.
	// All PICO devices should be updated to an OS supporting the official extension.

	request_extensions[XR_BD_CONTROLLER_INTERACTION_EXTENSION_NAME] = &available;

	return request_extensions;
}

bool OpenXRPicoControllerExtension::is_available() {
	return available;
}

void OpenXRPicoControllerExtension::on_register_metadata() {
	OpenXRInteractionProfileMetadata *openxr_metadata = OpenXRInteractionProfileMetadata::get_singleton();
	ERR_FAIL_NULL(openxr_metadata);

	// Make sure we switch to our new name.
	openxr_metadata->register_profile_rename("/interaction_profiles/pico/neo3_controller", "/interaction_profiles/bytedance/pico_neo3_controller");

	{ // Pico neo 3 controller.
		const String profile_path = "/interaction_profiles/bytedance/pico_neo3_controller";
		openxr_metadata->register_interaction_profile("Pico Neo3 controller", profile_path, XR_BD_CONTROLLER_INTERACTION_EXTENSION_NAME);
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			openxr_metadata->register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			openxr_metadata->register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			openxr_metadata->register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			openxr_metadata->register_io_path(profile_path, "Menu click", user_path, user_path + "/input/menu/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "System click", user_path, user_path + "/input/system/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			openxr_metadata->register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			openxr_metadata->register_io_path(profile_path, "Trigger touch", user_path, user_path + "/input/trigger/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);

			openxr_metadata->register_io_path(profile_path, "Squeeze", user_path, user_path + "/input/squeeze/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);

			openxr_metadata->register_io_path(profile_path, "Thumbstick", user_path, user_path + "/input/thumbstick", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			openxr_metadata->register_io_path(profile_path, "Thumbstick click", user_path, user_path + "/input/thumbstick/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick touch", user_path, user_path + "/input/thumbstick/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick Dpad Up", user_path, user_path + "/input/thumbstick/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick Dpad Down", user_path, user_path + "/input/thumbstick/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick Dpad Left", user_path, user_path + "/input/thumbstick/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick Dpad Right", user_path, user_path + "/input/thumbstick/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			openxr_metadata->register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}

		openxr_metadata->register_io_path(profile_path, "X click", "/user/hand/left", "/user/hand/left/input/x/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "X touch", "/user/hand/left", "/user/hand/left/input/x/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "Y click", "/user/hand/left", "/user/hand/left/input/y/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "Y touch", "/user/hand/left", "/user/hand/left/input/y/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "A click", "/user/hand/right", "/user/hand/right/input/a/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "A touch", "/user/hand/right", "/user/hand/right/input/a/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "B click", "/user/hand/right", "/user/hand/right/input/b/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "B touch", "/user/hand/right", "/user/hand/right/input/b/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
	}

	{ // Pico 4 controller.
		const String profile_path = "/interaction_profiles/bytedance/pico4_controller";
		openxr_metadata->register_interaction_profile("Pico 4 controller", profile_path, XR_BD_CONTROLLER_INTERACTION_EXTENSION_NAME);
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			openxr_metadata->register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			openxr_metadata->register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			openxr_metadata->register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

			openxr_metadata->register_io_path(profile_path, "System click", user_path, user_path + "/input/system/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			openxr_metadata->register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			openxr_metadata->register_io_path(profile_path, "Trigger touch", user_path, user_path + "/input/trigger/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);

			openxr_metadata->register_io_path(profile_path, "Squeeze", user_path, user_path + "/input/squeeze/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);

			openxr_metadata->register_io_path(profile_path, "Thumbstick", user_path, user_path + "/input/thumbstick", "", OpenXRAction::OPENXR_ACTION_VECTOR2);
			openxr_metadata->register_io_path(profile_path, "Thumbstick click", user_path, user_path + "/input/thumbstick/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick touch", user_path, user_path + "/input/thumbstick/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick Dpad Up", user_path, user_path + "/input/thumbstick/dpad_up", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick Dpad Down", user_path, user_path + "/input/thumbstick/dpad_down", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick Dpad Left", user_path, user_path + "/input/thumbstick/dpad_left", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);
			openxr_metadata->register_io_path(profile_path, "Thumbstick Dpad Right", user_path, user_path + "/input/thumbstick/dpad_right", "XR_EXT_dpad_binding", OpenXRAction::OPENXR_ACTION_BOOL);

			openxr_metadata->register_io_path(profile_path, "Haptic output", user_path, user_path + "/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
		}

		openxr_metadata->register_io_path(profile_path, "Menu click", "/user/hand/left", "/user/hand/left/input/menu/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		// Note, no menu on right controller!

		openxr_metadata->register_io_path(profile_path, "X click", "/user/hand/left", "/user/hand/left/input/x/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "X touch", "/user/hand/left", "/user/hand/left/input/x/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "Y click", "/user/hand/left", "/user/hand/left/input/y/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "Y touch", "/user/hand/left", "/user/hand/left/input/y/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "A click", "/user/hand/right", "/user/hand/right/input/a/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "A touch", "/user/hand/right", "/user/hand/right/input/a/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "B click", "/user/hand/right", "/user/hand/right/input/b/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
		openxr_metadata->register_io_path(profile_path, "B touch", "/user/hand/right", "/user/hand/right/input/b/touch", "", OpenXRAction::OPENXR_ACTION_BOOL);
	}
}
