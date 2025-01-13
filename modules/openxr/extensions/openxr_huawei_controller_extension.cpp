/**************************************************************************/
/*  openxr_huawei_controller_extension.cpp                                */
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

#include "openxr_huawei_controller_extension.h"

#include "../action_map/openxr_interaction_profile_metadata.h"

HashMap<String, bool *> OpenXRHuaweiControllerExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	request_extensions[XR_HUAWEI_CONTROLLER_INTERACTION_EXTENSION_NAME] = &available;

	return request_extensions;
}

bool OpenXRHuaweiControllerExtension::is_available() {
	return available;
}

void OpenXRHuaweiControllerExtension::on_register_metadata() {
	OpenXRInteractionProfileMetadata *metadata = OpenXRInteractionProfileMetadata::get_singleton();
	ERR_FAIL_NULL(metadata);

	{ // Huawei controller
		const String profile_path = "/interaction_profiles/huawei/controller";
		metadata->register_interaction_profile("Huawei controller", profile_path, XR_HUAWEI_CONTROLLER_INTERACTION_EXTENSION_NAME);
		for (const String user_path : { "/user/hand/left", "/user/hand/right" }) {
			metadata->register_io_path(profile_path, "Grip pose", user_path, user_path + "/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Aim pose", user_path, user_path + "/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
			metadata->register_io_path(profile_path, "Palm pose", user_path, user_path + "/input/palm_ext/pose", "", OpenXRAction::OPENXR_ACTION_POSE);

			metadata->register_io_path(profile_path, "Home click", user_path, user_path + "/input/home/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Back click", user_path, user_path + "/input/back/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Volume up click", user_path, user_path + "/input/volume_up/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
			metadata->register_io_path(profile_path, "Volume down click", user_path, user_path + "/input/volume_down/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

			metadata->register_io_path(profile_path, "Trigger", user_path, user_path + "/input/trigger/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
			metadata->register_io_path(profile_path, "Trigger click", user_path, user_path + "/input/trigger/click", "", OpenXRAction::OPENXR_ACTION_BOOL);

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
}
