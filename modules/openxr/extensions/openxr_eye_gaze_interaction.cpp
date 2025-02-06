/**************************************************************************/
/*  openxr_eye_gaze_interaction.cpp                                       */
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

#include "openxr_eye_gaze_interaction.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"

#include "../action_map/openxr_interaction_profile_metadata.h"
#include "../openxr_api.h"

OpenXREyeGazeInteractionExtension *OpenXREyeGazeInteractionExtension::singleton = nullptr;

OpenXREyeGazeInteractionExtension *OpenXREyeGazeInteractionExtension::get_singleton() {
	ERR_FAIL_NULL_V(singleton, nullptr);
	return singleton;
}

OpenXREyeGazeInteractionExtension::OpenXREyeGazeInteractionExtension() {
	singleton = this;
}

OpenXREyeGazeInteractionExtension::~OpenXREyeGazeInteractionExtension() {
	singleton = nullptr;
}

HashMap<String, bool *> OpenXREyeGazeInteractionExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	// Only enable this extension when requested.
	// We still register our meta data or the action map editor will fail.
	if (GLOBAL_GET("xr/openxr/extensions/eye_gaze_interaction") && (!OS::get_singleton()->has_feature("mobile") || OS::get_singleton()->has_feature(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME))) {
		request_extensions[XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME] = &available;
	}

	return request_extensions;
}

void *OpenXREyeGazeInteractionExtension::set_system_properties_and_get_next_pointer(void *p_next_pointer) {
	if (!available) {
		return p_next_pointer;
	}

	properties.type = XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT;
	properties.next = p_next_pointer;
	properties.supportsEyeGazeInteraction = false;

	return &properties;
}

PackedStringArray OpenXREyeGazeInteractionExtension::get_suggested_tracker_names() {
	PackedStringArray arr = { "/user/eyes_ext" };
	return arr;
}

bool OpenXREyeGazeInteractionExtension::is_available() {
	return available;
}

bool OpenXREyeGazeInteractionExtension::supports_eye_gaze_interaction() {
	// The extension being available only means that the OpenXR Runtime supports the extension.
	// The `supportsEyeGazeInteraction` is set to true if the device also supports this.
	// Thus both need to be true.
	// In addition, on mobile runtimes, the proper permission needs to be granted.
	if (available && properties.supportsEyeGazeInteraction) {
		return !OS::get_singleton()->has_feature("mobile") || OS::get_singleton()->has_feature("PERMISSION_XR_EXT_eye_gaze_interaction");
	}

	return false;
}

void OpenXREyeGazeInteractionExtension::on_register_metadata() {
	OpenXRInteractionProfileMetadata *openxr_metadata = OpenXRInteractionProfileMetadata::get_singleton();
	ERR_FAIL_NULL(openxr_metadata);

	// Eyes top path
	openxr_metadata->register_top_level_path("Eye gaze tracker", "/user/eyes_ext", XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);

	// Eye gaze interaction
	openxr_metadata->register_interaction_profile("Eye gaze", "/interaction_profiles/ext/eye_gaze_interaction", XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME);
	openxr_metadata->register_io_path("/interaction_profiles/ext/eye_gaze_interaction", "Gaze pose", "/user/eyes_ext", "/user/eyes_ext/input/gaze_ext/pose", "", OpenXRAction::OPENXR_ACTION_POSE);
}

bool OpenXREyeGazeInteractionExtension::get_eye_gaze_pose(double p_dist, Vector3 &r_eye_pose) {
	OpenXRAPI *openxr_api = OpenXRAPI::get_singleton();
	ERR_FAIL_NULL_V(openxr_api, false);

	if (!init_eye_gaze_pose) {
		init_eye_gaze_pose = true;

		eye_tracker = openxr_api->find_tracker("/user/eyes_ext");
		if (eye_tracker.is_null()) {
			WARN_PRINT("Couldn't obtain eye tracker");
		}

		eye_action = openxr_api->find_action("eye_gaze_pose");
		if (eye_action.is_null()) {
			WARN_PRINT("Couldn't obtain pose action for `eye_gaze_pose`, make sure to add this to your action map.");
		}
	}

	if (eye_tracker.is_null() || eye_action.is_null()) {
		return false;
	}

	Transform3D eye_transform;
	Vector3 linear_velocity;
	Vector3 angular_velocity;
	XRPose::TrackingConfidence confidence = openxr_api->get_action_pose(eye_action, eye_tracker, eye_transform, linear_velocity, angular_velocity);
	if (confidence == XRPose::XR_TRACKING_CONFIDENCE_NONE) {
		return false;
	}

	r_eye_pose = eye_transform.origin + eye_transform.basis[2] * p_dist;

	return true;
}
