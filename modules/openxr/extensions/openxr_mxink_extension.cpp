/**************************************************************************/
/*  openxr_mxink_extension.cpp                                            */
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

#include "openxr_mxink_extension.h"

#include "../action_map/openxr_interaction_profile_metadata.h"

// Not in base XR libs needs def
#define XR_LOGITECH_MX_INK_STYLUS_INTERACTION_EXTENSION_NAME "XR_LOGITECH_mx_ink_stylus_interaction"

HashMap<String, bool *> OpenXRMxInkExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	request_extensions[XR_LOGITECH_MX_INK_STYLUS_INTERACTION_EXTENSION_NAME] = &available;

	return request_extensions;
}

bool OpenXRMxInkExtension::is_available() {
	return available;
}

void OpenXRMxInkExtension::on_register_metadata() {
	OpenXRInteractionProfileMetadata *openxr_metadata = OpenXRInteractionProfileMetadata::get_singleton();
	ERR_FAIL_NULL(openxr_metadata);

	// Logitech MX Ink Stylus
	openxr_metadata->register_interaction_profile("Logitech MX Ink Stylus", "/interaction_profiles/logitech/mx_ink_stylus_logitech", XR_LOGITECH_MX_INK_STYLUS_INTERACTION_EXTENSION_NAME);

	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Tip Force", "/user/hand/left", "/user/hand/left/input/tip_logitech/force", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Middle force", "/user/hand/left", "/user/hand/left/input/cluster_middle_logitech/force", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Front click", "/user/hand/left", "/user/hand/left/input/cluster_front_logitech/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Front double", "/user/hand/left", "/user/hand/left/input/cluster_front_logitech/double_tap_logitech", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Back click", "/user/hand/left", "/user/hand/left/input/cluster_back_logitech/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Back double", "/user/hand/left", "/user/hand/left/input/cluster_back_logitech/double_tap_logitech", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "System click", "/user/hand/left", "/user/hand/left/input/system/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Docked", "/user/hand/left", "/user/hand/left/input/dock_logitech/docked_logitech", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Grip pose", "/user/hand/left", "/user/hand/left/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Aim pose", "/user/hand/left", "/user/hand/left/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Tip pose", "/user/hand/left", "/user/hand/left/input/tip_logitech/pose", "", OpenXRAction::OPENXR_ACTION_POSE);

	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Tip Force", "/user/hand/right", "/user/hand/right/input/tip_logitech/force", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Middle force", "/user/hand/right", "/user/hand/right/input/cluster_middle_logitech/force", "", OpenXRAction::OPENXR_ACTION_FLOAT);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Front click", "/user/hand/right", "/user/hand/right/input/cluster_front_logitech/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Front double", "/user/hand/right", "/user/hand/right/input/cluster_front_logitech/double_tap_logitech", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Back click", "/user/hand/right", "/user/hand/right/input/cluster_back_logitech/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Back double", "/user/hand/right", "/user/hand/right/input/cluster_back_logitech/double_tap_logitech", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "System click", "/user/hand/right", "/user/hand/right/input/system/click", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Docked", "/user/hand/right", "/user/hand/right/input/dock_logitech/docked_logitech", "", OpenXRAction::OPENXR_ACTION_BOOL);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Grip pose", "/user/hand/right", "/user/hand/right/input/grip/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Aim pose", "/user/hand/right", "/user/hand/right/input/aim/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Tip pose", "/user/hand/right", "/user/hand/right/input/tip_logitech/pose", "", OpenXRAction::OPENXR_ACTION_POSE);

	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Haptic output", "/user/hand/left", "/user/hand/left/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
	openxr_metadata->register_io_path("/interaction_profiles/logitech/mx_ink_stylus_logitech", "Haptic output", "/user/hand/right", "/user/hand/right/output/haptic", "", OpenXRAction::OPENXR_ACTION_HAPTIC);
}
