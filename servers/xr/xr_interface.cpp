/**************************************************************************/
/*  xr_interface.cpp                                                      */
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

#include "xr_interface.h"

void XRInterface::_bind_methods() {
	ADD_SIGNAL(MethodInfo("play_area_changed", PropertyInfo(Variant::INT, "mode")));

	ClassDB::bind_method(D_METHOD("get_name"), &XRInterface::get_name);
	ClassDB::bind_method(D_METHOD("get_capabilities"), &XRInterface::get_capabilities);

	ClassDB::bind_method(D_METHOD("is_primary"), &XRInterface::is_primary);
	ClassDB::bind_method(D_METHOD("set_primary", "primary"), &XRInterface::set_primary);

	ClassDB::bind_method(D_METHOD("is_initialized"), &XRInterface::is_initialized);
	ClassDB::bind_method(D_METHOD("initialize"), &XRInterface::initialize);
	ClassDB::bind_method(D_METHOD("uninitialize"), &XRInterface::uninitialize);
	ClassDB::bind_method(D_METHOD("get_system_info"), &XRInterface::get_system_info);

	ClassDB::bind_method(D_METHOD("get_tracking_status"), &XRInterface::get_tracking_status);

	ClassDB::bind_method(D_METHOD("get_render_target_size"), &XRInterface::get_render_target_size);
	ClassDB::bind_method(D_METHOD("get_view_count"), &XRInterface::get_view_count);

	ClassDB::bind_method(D_METHOD("trigger_haptic_pulse", "action_name", "tracker_name", "frequency", "amplitude", "duration_sec", "delay_sec"), &XRInterface::trigger_haptic_pulse);

	ADD_GROUP("Interface", "interface_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "interface_is_primary"), "set_primary", "is_primary");

	// methods and properties specific to VR...
	ClassDB::bind_method(D_METHOD("supports_play_area_mode", "mode"), &XRInterface::supports_play_area_mode);
	ClassDB::bind_method(D_METHOD("get_play_area_mode"), &XRInterface::get_play_area_mode);
	ClassDB::bind_method(D_METHOD("set_play_area_mode", "mode"), &XRInterface::set_play_area_mode);
	ClassDB::bind_method(D_METHOD("get_play_area"), &XRInterface::get_play_area);

	ADD_GROUP("XR", "xr_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "xr_play_area_mode", PROPERTY_HINT_ENUM, "Unknown,3DOF,Sitting,Roomscale,Stage"), "set_play_area_mode", "get_play_area_mode");

	// methods and properties specific to AR....
	ClassDB::bind_method(D_METHOD("get_anchor_detection_is_enabled"), &XRInterface::get_anchor_detection_is_enabled);
	ClassDB::bind_method(D_METHOD("set_anchor_detection_is_enabled", "enable"), &XRInterface::set_anchor_detection_is_enabled);
	ClassDB::bind_method(D_METHOD("get_camera_feed_id"), &XRInterface::get_camera_feed_id);

	ClassDB::bind_method(D_METHOD("is_passthrough_supported"), &XRInterface::is_passthrough_supported);
	ClassDB::bind_method(D_METHOD("is_passthrough_enabled"), &XRInterface::is_passthrough_enabled);
	ClassDB::bind_method(D_METHOD("start_passthrough"), &XRInterface::start_passthrough);
	ClassDB::bind_method(D_METHOD("stop_passthrough"), &XRInterface::stop_passthrough);
	ClassDB::bind_method(D_METHOD("get_transform_for_view", "view", "cam_transform"), &XRInterface::get_transform_for_view);
	ClassDB::bind_method(D_METHOD("get_projection_for_view", "view", "aspect", "near", "far"), &XRInterface::get_projection_for_view);

	/** environment blend mode. */
	ClassDB::bind_method(D_METHOD("get_supported_environment_blend_modes"), &XRInterface::get_supported_environment_blend_modes);
	ClassDB::bind_method(D_METHOD("set_environment_blend_mode", "mode"), &XRInterface::set_environment_blend_mode);
	ClassDB::bind_method(D_METHOD("get_environment_blend_mode"), &XRInterface::get_environment_blend_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "environment_blend_mode"), "set_environment_blend_mode", "get_environment_blend_mode");

	ADD_GROUP("AR", "ar_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "ar_is_anchor_detection_enabled"), "set_anchor_detection_is_enabled", "get_anchor_detection_is_enabled");

	BIND_ENUM_CONSTANT(XR_NONE);
	BIND_ENUM_CONSTANT(XR_MONO);
	BIND_ENUM_CONSTANT(XR_STEREO);
	BIND_ENUM_CONSTANT(XR_QUAD);
	BIND_ENUM_CONSTANT(XR_VR);
	BIND_ENUM_CONSTANT(XR_AR);
	BIND_ENUM_CONSTANT(XR_EXTERNAL);

	BIND_ENUM_CONSTANT(XR_NORMAL_TRACKING);
	BIND_ENUM_CONSTANT(XR_EXCESSIVE_MOTION);
	BIND_ENUM_CONSTANT(XR_INSUFFICIENT_FEATURES);
	BIND_ENUM_CONSTANT(XR_UNKNOWN_TRACKING);
	BIND_ENUM_CONSTANT(XR_NOT_TRACKING);

	BIND_ENUM_CONSTANT(XR_PLAY_AREA_UNKNOWN);
	BIND_ENUM_CONSTANT(XR_PLAY_AREA_3DOF);
	BIND_ENUM_CONSTANT(XR_PLAY_AREA_SITTING);
	BIND_ENUM_CONSTANT(XR_PLAY_AREA_ROOMSCALE);
	BIND_ENUM_CONSTANT(XR_PLAY_AREA_STAGE);

	BIND_ENUM_CONSTANT(XR_ENV_BLEND_MODE_OPAQUE);
	BIND_ENUM_CONSTANT(XR_ENV_BLEND_MODE_ADDITIVE);
	BIND_ENUM_CONSTANT(XR_ENV_BLEND_MODE_ALPHA_BLEND);
}

bool XRInterface::is_primary() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, false);

	return xr_server->get_primary_interface() == this;
}

void XRInterface::set_primary(bool p_primary) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	if (p_primary) {
		ERR_FAIL_COND(!is_initialized());

		xr_server->set_primary_interface(this);
	} else if (xr_server->get_primary_interface() == this) {
		xr_server->set_primary_interface(nullptr);
	}
}

XRInterface::XRInterface() {}

XRInterface::~XRInterface() {}

// query if this interface supports this play area mode
bool XRInterface::supports_play_area_mode(XRInterface::PlayAreaMode p_mode) {
	return p_mode == XR_PLAY_AREA_UNKNOWN;
}

// get the current play area mode
XRInterface::PlayAreaMode XRInterface::get_play_area_mode() const {
	return XR_PLAY_AREA_UNKNOWN;
}

// change the play area mode, note that this should return false if the mode is not available
bool XRInterface::set_play_area_mode(XRInterface::PlayAreaMode p_mode) {
	return p_mode == XR_PLAY_AREA_UNKNOWN;
}

// if available, returns an array of vectors denoting the play area the player can move around in
PackedVector3Array XRInterface::get_play_area() const {
	// Return an empty array by default.
	// Note implementation is responsible for applying our reference frame and world scale to the raw data.
	// `play_area_changed` should be emitted if play area data is available and either the reference frame or world scale changes.
	return PackedVector3Array();
}

/** these will only be implemented on AR interfaces, so we want dummies for VR **/
bool XRInterface::get_anchor_detection_is_enabled() const {
	return false;
}

void XRInterface::set_anchor_detection_is_enabled(bool p_enable) {
}

int XRInterface::get_camera_feed_id() {
	return 0;
}

RID XRInterface::get_vrs_texture() {
	return RID();
}

/** these are optional, so we want dummies **/

RID XRInterface::get_color_texture() {
	return RID();
}

RID XRInterface::get_depth_texture() {
	return RID();
}

RID XRInterface::get_velocity_texture() {
	return RID();
}

RID XRInterface::get_velocity_depth_texture() {
	return RID();
}

Size2i XRInterface::get_velocity_target_size() {
	return Size2i();
}

PackedStringArray XRInterface::get_suggested_tracker_names() const {
	PackedStringArray arr;

	return arr;
}

PackedStringArray XRInterface::get_suggested_pose_names(const StringName &p_tracker_name) const {
	PackedStringArray arr;

	return arr;
}

XRInterface::TrackingStatus XRInterface::get_tracking_status() const {
	return XR_UNKNOWN_TRACKING;
}

void XRInterface::trigger_haptic_pulse(const String &p_action_name, const StringName &p_tracker_name, double p_frequency, double p_amplitude, double p_duration_sec, double p_delay_sec) {
}

Array XRInterface::get_supported_environment_blend_modes() {
	Array default_blend_modes;
	default_blend_modes.push_back(XR_ENV_BLEND_MODE_OPAQUE);
	return default_blend_modes;
}
