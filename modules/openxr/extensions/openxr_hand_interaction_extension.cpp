/**************************************************************************/
/*  openxr_hand_interaction_extension.cpp                                 */
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

#include "openxr_hand_interaction_extension.h"

#include "../action_map/openxr_interaction_profile_metadata.h"
#include "core/config/project_settings.h"

OpenXRHandInteractionExtension *OpenXRHandInteractionExtension::singleton = nullptr;

OpenXRHandInteractionExtension *OpenXRHandInteractionExtension::get_singleton() {
	return singleton;
}

OpenXRHandInteractionExtension::OpenXRHandInteractionExtension() {
	singleton = this;
}

OpenXRHandInteractionExtension::~OpenXRHandInteractionExtension() {
	singleton = nullptr;
}

HashMap<String, bool *> OpenXRHandInteractionExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	// Only enable this extension when requested.
	// We still register our meta data or the action map editor will fail.
	if (GLOBAL_GET("xr/openxr/extensions/hand_interaction_profile")) {
		request_extensions[XR_EXT_HAND_INTERACTION_EXTENSION_NAME] = &available;
	}

	return request_extensions;
}

bool OpenXRHandInteractionExtension::is_available() {
	return available;
}

void OpenXRHandInteractionExtension::on_register_metadata() {
	OpenXRInteractionProfileMetadata *openxr_metadata = OpenXRInteractionProfileMetadata::get_singleton();
	ERR_FAIL_NULL(openxr_metadata);

	// Hand interaction profile
	openxr_metadata->register_interaction_profile("Hand interaction", "/interaction_profiles/ext/hand_interaction_ext", XR_EXT_HAND_INTERACTION_EXTENSION_NAME);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Grip pose", "/user/hand/left", "/user/hand/left/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Grip pose", "/user/hand/right", "/user/hand/right/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Aim pose", "/user/hand/left", "/user/hand/left/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Aim pose", "/user/hand/right", "/user/hand/right/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Pinch pose", "/user/hand/left", "/user/hand/left/input/pinch_ext/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Pinch pose", "/user/hand/right", "/user/hand/right/input/pinch_ext/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Poke pose", "/user/hand/left", "/user/hand/left/input/poke_ext/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Poke pose", "/user/hand/right", "/user/hand/right/input/poke_ext/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Palm pose", "/user/hand/left", "/user/hand/left/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Palm pose", "/user/hand/right", "/user/hand/right/input/palm_ext/pose", XR_EXT_PALM_POSE_EXTENSION_NAME, OpenXRAction::OPENXR_ACTION_POSE);

	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Pinch", "/user/hand/left", "/user/hand/left/input/pinch_ext/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Pinch", "/user/hand/right", "/user/hand/right/input/pinch_ext/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Pinch ready", "/user/hand/left", "/user/hand/left/input/pinch_ext/ready_ext", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Pinch ready", "/user/hand/right", "/user/hand/right/input/pinch_ext/ready_ext", "", OpenXRAction::OPENXR_ACTION_BOOL);

	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Aim activate", "/user/hand/left", "/user/hand/left/input/aim_activate_ext/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Aim activate", "/user/hand/right", "/user/hand/right/input/aim_activate_ext/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Aim activate ready", "/user/hand/left", "/user/hand/left/input/aim_activate_ext/ready_ext", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Aim activate ready", "/user/hand/right", "/user/hand/right/input/aim_activate_ext/ready_ext", "", OpenXRAction::OPENXR_ACTION_BOOL);

	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Grasp", "/user/hand/left", "/user/hand/left/input/grasp_ext/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Grasp", "/user/hand/right", "/user/hand/right/input/grasp_ext/value", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Grasp ready", "/user/hand/left", "/user/hand/left/input/grasp_ext/ready_ext", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/ext/hand_interaction_ext", "Grasp ready", "/user/hand/right", "/user/hand/right/input/grasp_ext/ready_ext", "", OpenXRAction::OPENXR_ACTION_BOOL);
}
