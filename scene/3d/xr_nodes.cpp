/**************************************************************************/
/*  xr_nodes.cpp                                                          */
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

#include "xr_nodes.h"

#include "core/config/project_settings.h"
#include "scene/main/viewport.h"
#include "servers/xr/xr_interface.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

void XRCamera3D::_bind_tracker() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	tracker = xr_server->get_tracker(tracker_name);
	if (tracker.is_valid()) {
		tracker->connect("pose_changed", callable_mp(this, &XRCamera3D::_pose_changed));

		Ref<XRPose> pose = tracker->get_pose(pose_name);
		if (pose.is_valid()) {
			set_transform(pose->get_adjusted_transform());
		}
	}
}

void XRCamera3D::_unbind_tracker() {
	if (tracker.is_valid()) {
		tracker->disconnect("pose_changed", callable_mp(this, &XRCamera3D::_pose_changed));
	}
	tracker.unref();
}

void XRCamera3D::_changed_tracker(const StringName &p_tracker_name, int p_tracker_type) {
	if (p_tracker_name == tracker_name) {
		_bind_tracker();
	}
}

void XRCamera3D::_removed_tracker(const StringName &p_tracker_name, int p_tracker_type) {
	if (p_tracker_name == tracker_name) {
		_unbind_tracker();
	}
}

void XRCamera3D::_pose_changed(const Ref<XRPose> &p_pose) {
	if (p_pose->get_name() == pose_name) {
		set_transform(p_pose->get_adjusted_transform());
	}
}

PackedStringArray XRCamera3D::get_configuration_warnings() const {
	PackedStringArray warnings = Camera3D::get_configuration_warnings();

	if (is_visible() && is_inside_tree()) {
		// Warn if the node has a parent which isn't an XROrigin3D!
		Node *parent = get_parent();
		XROrigin3D *origin = Object::cast_to<XROrigin3D>(parent);
		if (parent && origin == nullptr) {
			warnings.push_back(RTR("XRCamera3D may not function as expected without an XROrigin3D node as its parent."));
		};
	}

	return warnings;
}

Vector3 XRCamera3D::project_local_ray_normal(const Point2 &p_pos) const {
	// get our XRServer
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, Vector3());

	Ref<XRInterface> xr_interface = xr_server->get_primary_interface();
	if (xr_interface.is_null()) {
		// we might be in the editor or have VR turned off, just call superclass
		return Camera3D::project_local_ray_normal(p_pos);
	}

	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector3(), "Camera is not inside scene.");

	Size2 viewport_size = get_viewport()->get_camera_rect_size();
	Vector2 cpos = get_viewport()->get_camera_coords(p_pos);
	Vector3 ray;

	// Just use the first view, if multiple views are supported this function has no good result
	Projection cm = xr_interface->get_projection_for_view(0, viewport_size.aspect(), get_near(), get_far());
	Vector2 screen_he = cm.get_viewport_half_extents();
	ray = Vector3(((cpos.x / viewport_size.width) * 2.0 - 1.0) * screen_he.x, ((1.0 - (cpos.y / viewport_size.height)) * 2.0 - 1.0) * screen_he.y, -get_near()).normalized();

	return ray;
}

Point2 XRCamera3D::unproject_position(const Vector3 &p_pos) const {
	// get our XRServer
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, Vector2());

	Ref<XRInterface> xr_interface = xr_server->get_primary_interface();
	if (xr_interface.is_null()) {
		// we might be in the editor or have VR turned off, just call superclass
		return Camera3D::unproject_position(p_pos);
	}

	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector2(), "Camera is not inside scene.");

	Size2 viewport_size = get_viewport()->get_visible_rect().size;

	// Just use the first view, if multiple views are supported this function has no good result
	Projection cm = xr_interface->get_projection_for_view(0, viewport_size.aspect(), get_near(), get_far());

	Plane p(get_camera_transform().xform_inv(p_pos), 1.0);

	p = cm.xform4(p);
	p.normal /= p.d;

	Point2 res;
	res.x = (p.normal.x * 0.5 + 0.5) * viewport_size.x;
	res.y = (-p.normal.y * 0.5 + 0.5) * viewport_size.y;

	return res;
}

Vector3 XRCamera3D::project_position(const Point2 &p_point, real_t p_z_depth) const {
	// get our XRServer
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, Vector3());

	Ref<XRInterface> xr_interface = xr_server->get_primary_interface();
	if (xr_interface.is_null()) {
		// we might be in the editor or have VR turned off, just call superclass
		return Camera3D::project_position(p_point, p_z_depth);
	}

	ERR_FAIL_COND_V_MSG(!is_inside_tree(), Vector3(), "Camera is not inside scene.");

	Size2 viewport_size = get_viewport()->get_visible_rect().size;

	// Just use the first view, if multiple views are supported this function has no good result
	Projection cm = xr_interface->get_projection_for_view(0, viewport_size.aspect(), get_near(), get_far());

	Vector2 vp_he = cm.get_viewport_half_extents();

	Vector2 point;
	point.x = (p_point.x / viewport_size.x) * 2.0 - 1.0;
	point.y = (1.0 - (p_point.y / viewport_size.y)) * 2.0 - 1.0;
	point *= vp_he;

	Vector3 p(point.x, point.y, -p_z_depth);

	return get_camera_transform().xform(p);
}

Vector<Plane> XRCamera3D::get_frustum() const {
	// get our XRServer
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, Vector<Plane>());

	Ref<XRInterface> xr_interface = xr_server->get_primary_interface();
	if (xr_interface.is_null()) {
		// we might be in the editor or have VR turned off, just call superclass
		return Camera3D::get_frustum();
	}

	ERR_FAIL_COND_V(!is_inside_world(), Vector<Plane>());

	Size2 viewport_size = get_viewport()->get_visible_rect().size;
	// TODO Just use the first view for now, this is mostly for debugging so we may look into using our combined projection here.
	Projection cm = xr_interface->get_projection_for_view(0, viewport_size.aspect(), get_near(), get_far());
	return cm.get_projection_planes(get_camera_transform());
}

XRCamera3D::XRCamera3D() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	xr_server->connect("tracker_added", callable_mp(this, &XRCamera3D::_changed_tracker));
	xr_server->connect("tracker_updated", callable_mp(this, &XRCamera3D::_changed_tracker));
	xr_server->connect("tracker_removed", callable_mp(this, &XRCamera3D::_removed_tracker));

	// check if our tracker already exists and if so, bind it...
	_bind_tracker();
}

XRCamera3D::~XRCamera3D() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	xr_server->disconnect("tracker_added", callable_mp(this, &XRCamera3D::_changed_tracker));
	xr_server->disconnect("tracker_updated", callable_mp(this, &XRCamera3D::_changed_tracker));
	xr_server->disconnect("tracker_removed", callable_mp(this, &XRCamera3D::_removed_tracker));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// XRNode3D is a node that has it's transform updated by an XRPositionalTracker.
// Note that trackers are only available in runtime and only after an XRInterface registers one.
// So we bind by name and as long as a tracker isn't available, our node remains inactive.

void XRNode3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tracker", "tracker_name"), &XRNode3D::set_tracker);
	ClassDB::bind_method(D_METHOD("get_tracker"), &XRNode3D::get_tracker);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tracker", PROPERTY_HINT_ENUM_SUGGESTION), "set_tracker", "get_tracker");

	ClassDB::bind_method(D_METHOD("set_pose_name", "pose"), &XRNode3D::set_pose_name);
	ClassDB::bind_method(D_METHOD("get_pose_name"), &XRNode3D::get_pose_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "pose", PROPERTY_HINT_ENUM_SUGGESTION), "set_pose_name", "get_pose_name");

	ClassDB::bind_method(D_METHOD("set_show_when_tracked", "show"), &XRNode3D::set_show_when_tracked);
	ClassDB::bind_method(D_METHOD("get_show_when_tracked"), &XRNode3D::get_show_when_tracked);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_when_tracked"), "set_show_when_tracked", "get_show_when_tracked");

	ClassDB::bind_method(D_METHOD("get_is_active"), &XRNode3D::get_is_active);
	ClassDB::bind_method(D_METHOD("get_has_tracking_data"), &XRNode3D::get_has_tracking_data);
	ClassDB::bind_method(D_METHOD("get_pose"), &XRNode3D::get_pose);
	ClassDB::bind_method(D_METHOD("trigger_haptic_pulse", "action_name", "frequency", "amplitude", "duration_sec", "delay_sec"), &XRNode3D::trigger_haptic_pulse);

	ADD_SIGNAL(MethodInfo("tracking_changed", PropertyInfo(Variant::BOOL, "tracking")));
}

void XRNode3D::_validate_property(PropertyInfo &p_property) const {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	if (p_property.name == "tracker") {
		PackedStringArray names = xr_server->get_suggested_tracker_names();
		String hint_string;
		for (const String &name : names) {
			hint_string += name + ",";
		}
		p_property.hint_string = hint_string;
	} else if (p_property.name == "pose") {
		PackedStringArray names = xr_server->get_suggested_pose_names(tracker_name);
		String hint_string;
		for (const String &name : names) {
			hint_string += name + ",";
		}
		p_property.hint_string = hint_string;
	}
}

void XRNode3D::set_tracker(const StringName &p_tracker_name) {
	if (tracker.is_valid() && tracker->get_tracker_name() == p_tracker_name) {
		// didn't change
		return;
	}

	// just in case
	_unbind_tracker();

	// copy the name
	tracker_name = p_tracker_name;
	pose_name = SceneStringName(default_);

	// see if it's already available
	_bind_tracker();

	update_configuration_warnings();
	notify_property_list_changed();
}

StringName XRNode3D::get_tracker() const {
	return tracker_name;
}

void XRNode3D::set_pose_name(const StringName &p_pose_name) {
	pose_name = p_pose_name;

	// Update pose if we are bound to a tracker with a valid pose
	Ref<XRPose> pose = get_pose();
	if (pose.is_valid()) {
		set_transform(pose->get_adjusted_transform());
	}
}

StringName XRNode3D::get_pose_name() const {
	return pose_name;
}

void XRNode3D::set_show_when_tracked(bool p_show) {
	show_when_tracked = p_show;

	_update_visibility();
}

bool XRNode3D::get_show_when_tracked() const {
	return show_when_tracked;
}

bool XRNode3D::get_is_active() const {
	if (tracker.is_null()) {
		return false;
	} else if (!tracker->has_pose(pose_name)) {
		return false;
	} else {
		return true;
	}
}

bool XRNode3D::get_has_tracking_data() const {
	return has_tracking_data;
}

void XRNode3D::trigger_haptic_pulse(const String &p_action_name, double p_frequency, double p_amplitude, double p_duration_sec, double p_delay_sec) {
	// TODO need to link trackers to the interface that registered them so we can call this on the correct interface.
	// For now this works fine as in 99% of the cases we only have our primary interface active
	XRServer *xr_server = XRServer::get_singleton();
	if (xr_server != nullptr) {
		Ref<XRInterface> xr_interface = xr_server->get_primary_interface();
		if (xr_interface.is_valid()) {
			xr_interface->trigger_haptic_pulse(p_action_name, tracker_name, p_frequency, p_amplitude, p_duration_sec, p_delay_sec);
		}
	}
}

Ref<XRPose> XRNode3D::get_pose() {
	if (tracker.is_valid()) {
		return tracker->get_pose(pose_name);
	} else {
		return Ref<XRPose>();
	}
}

void XRNode3D::_bind_tracker() {
	ERR_FAIL_COND_MSG(tracker.is_valid(), "Unbind the current tracker first");

	XRServer *xr_server = XRServer::get_singleton();
	if (xr_server != nullptr) {
		tracker = xr_server->get_tracker(tracker_name);
		if (tracker.is_null()) {
			// It is possible and valid if the tracker isn't available (yet), in this case we just exit
			return;
		}

		tracker->connect("pose_changed", callable_mp(this, &XRNode3D::_pose_changed));
		tracker->connect("pose_lost_tracking", callable_mp(this, &XRNode3D::_pose_lost_tracking));

		Ref<XRPose> pose = get_pose();
		if (pose.is_valid()) {
			set_transform(pose->get_adjusted_transform());
			_set_has_tracking_data(pose->get_has_tracking_data());
		} else {
			// Pose has been invalidated or was never set.
			_set_has_tracking_data(false);
		}
	}
}

void XRNode3D::_unbind_tracker() {
	if (tracker.is_valid()) {
		tracker->disconnect("pose_changed", callable_mp(this, &XRNode3D::_pose_changed));
		tracker->disconnect("pose_lost_tracking", callable_mp(this, &XRNode3D::_pose_lost_tracking));

		tracker.unref();

		_set_has_tracking_data(false);
	}
}

void XRNode3D::_changed_tracker(const StringName &p_tracker_name, int p_tracker_type) {
	if (tracker_name == p_tracker_name) {
		// just in case unref our current tracker
		_unbind_tracker();

		// get our new tracker
		_bind_tracker();
	}
}

void XRNode3D::_removed_tracker(const StringName &p_tracker_name, int p_tracker_type) {
	if (tracker_name == p_tracker_name) {
		// unref our tracker, it's no longer available
		_unbind_tracker();
	}
}

void XRNode3D::_pose_changed(const Ref<XRPose> &p_pose) {
	if (p_pose.is_valid() && p_pose->get_name() == pose_name) {
		set_transform(p_pose->get_adjusted_transform());
		_set_has_tracking_data(p_pose->get_has_tracking_data());
	}
}

void XRNode3D::_pose_lost_tracking(const Ref<XRPose> &p_pose) {
	if (p_pose.is_valid() && p_pose->get_name() == pose_name) {
		_set_has_tracking_data(false);
	}
}

void XRNode3D::_set_has_tracking_data(bool p_has_tracking_data) {
	// Always update our visibility, we may have set our tracking data
	// when conditions weren't right.
	_update_visibility();

	// Ignore if the has_tracking_data state isn't changing.
	if (p_has_tracking_data == has_tracking_data) {
		return;
	}

	// Handle change of has_tracking_data.
	has_tracking_data = p_has_tracking_data;
	emit_signal(SNAME("tracking_changed"), has_tracking_data);
}

void XRNode3D::_update_visibility() {
	// If configured, show or hide the node based on tracking data.
	if (show_when_tracked) {
		// Only react to this if we have a primary interface.
		XRServer *xr_server = XRServer::get_singleton();
		if (xr_server != nullptr) {
			Ref<XRInterface> xr_interface = xr_server->get_primary_interface();
			if (xr_interface.is_valid()) {
				set_visible(has_tracking_data);
			}
		}
	}
}

XRNode3D::XRNode3D() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	xr_server->connect("tracker_added", callable_mp(this, &XRNode3D::_changed_tracker));
	xr_server->connect("tracker_updated", callable_mp(this, &XRNode3D::_changed_tracker));
	xr_server->connect("tracker_removed", callable_mp(this, &XRNode3D::_removed_tracker));
}

XRNode3D::~XRNode3D() {
	_unbind_tracker();

	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	xr_server->disconnect("tracker_added", callable_mp(this, &XRNode3D::_changed_tracker));
	xr_server->disconnect("tracker_updated", callable_mp(this, &XRNode3D::_changed_tracker));
	xr_server->disconnect("tracker_removed", callable_mp(this, &XRNode3D::_removed_tracker));
}

PackedStringArray XRNode3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (is_visible() && is_inside_tree()) {
		// Warn if the node has a parent which isn't an XROrigin3D!
		Node *parent = get_parent();
		XROrigin3D *origin = Object::cast_to<XROrigin3D>(parent);
		if (parent && origin == nullptr) {
			warnings.push_back(RTR("XRNode3D may not function as expected without an XROrigin3D node as its parent."));
		};

		if (tracker_name == "") {
			warnings.push_back(RTR("No tracker name is set."));
		}

		if (pose_name == "") {
			warnings.push_back(RTR("No pose is set."));
		}
	}

	return warnings;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void XRController3D::_bind_methods() {
	// passthroughs to information about our related joystick
	ClassDB::bind_method(D_METHOD("is_button_pressed", "name"), &XRController3D::is_button_pressed);
	ClassDB::bind_method(D_METHOD("get_input", "name"), &XRController3D::get_input);
	ClassDB::bind_method(D_METHOD("get_float", "name"), &XRController3D::get_float);
	ClassDB::bind_method(D_METHOD("get_vector2", "name"), &XRController3D::get_vector2);

	ClassDB::bind_method(D_METHOD("get_tracker_hand"), &XRController3D::get_tracker_hand);

	ADD_SIGNAL(MethodInfo("button_pressed", PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo("button_released", PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo("input_float_changed", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::FLOAT, "value")));
	ADD_SIGNAL(MethodInfo("input_vector2_changed", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::VECTOR2, "value")));
	ADD_SIGNAL(MethodInfo("profile_changed", PropertyInfo(Variant::STRING, "role")));
}

void XRController3D::_bind_tracker() {
	XRNode3D::_bind_tracker();
	if (tracker.is_valid()) {
		// bind to input signals
		tracker->connect("button_pressed", callable_mp(this, &XRController3D::_button_pressed));
		tracker->connect("button_released", callable_mp(this, &XRController3D::_button_released));
		tracker->connect("input_float_changed", callable_mp(this, &XRController3D::_input_float_changed));
		tracker->connect("input_vector2_changed", callable_mp(this, &XRController3D::_input_vector2_changed));
		tracker->connect("profile_changed", callable_mp(this, &XRController3D::_profile_changed));
	}
}

void XRController3D::_unbind_tracker() {
	if (tracker.is_valid()) {
		// unbind input signals
		tracker->disconnect("button_pressed", callable_mp(this, &XRController3D::_button_pressed));
		tracker->disconnect("button_released", callable_mp(this, &XRController3D::_button_released));
		tracker->disconnect("input_float_changed", callable_mp(this, &XRController3D::_input_float_changed));
		tracker->disconnect("input_vector2_changed", callable_mp(this, &XRController3D::_input_vector2_changed));
		tracker->disconnect("profile_changed", callable_mp(this, &XRController3D::_profile_changed));
	}

	XRNode3D::_unbind_tracker();
}

void XRController3D::_button_pressed(const String &p_name) {
	emit_signal(SNAME("button_pressed"), p_name);
}

void XRController3D::_button_released(const String &p_name) {
	emit_signal(SNAME("button_released"), p_name);
}

void XRController3D::_input_float_changed(const String &p_name, float p_value) {
	emit_signal(SNAME("input_float_changed"), p_name, p_value);
}

void XRController3D::_input_vector2_changed(const String &p_name, Vector2 p_value) {
	emit_signal(SNAME("input_vector2_changed"), p_name, p_value);
}

void XRController3D::_profile_changed(const String &p_role) {
	emit_signal(SNAME("profile_changed"), p_role);
}

bool XRController3D::is_button_pressed(const StringName &p_name) const {
	if (tracker.is_valid()) {
		// Inputs should already be of the correct type, our XR runtime handles conversions between raw input and the desired type
		bool pressed = tracker->get_input(p_name);
		return pressed;
	} else {
		return false;
	}
}

Variant XRController3D::get_input(const StringName &p_name) const {
	if (tracker.is_valid()) {
		return tracker->get_input(p_name);
	} else {
		return Variant();
	}
}

float XRController3D::get_float(const StringName &p_name) const {
	if (tracker.is_valid()) {
		// Inputs should already be of the correct type, our XR runtime handles conversions between raw input and the desired type, but just in case we convert
		Variant input = tracker->get_input(p_name);
		switch (input.get_type()) {
			case Variant::BOOL: {
				bool value = input;
				return value ? 1.0 : 0.0;
			} break;
			case Variant::FLOAT: {
				float value = input;
				return value;
			} break;
			default:
				return 0.0;
		};
	} else {
		return 0.0;
	}
}

Vector2 XRController3D::get_vector2(const StringName &p_name) const {
	if (tracker.is_valid()) {
		// Inputs should already be of the correct type, our XR runtime handles conversions between raw input and the desired type, but just in case we convert
		Variant input = tracker->get_input(p_name);
		switch (input.get_type()) {
			case Variant::BOOL: {
				bool value = input;
				return Vector2(value ? 1.0 : 0.0, 0.0);
			} break;
			case Variant::FLOAT: {
				float value = input;
				return Vector2(value, 0.0);
			} break;
			case Variant::VECTOR2: {
				Vector2 axis = input;
				return axis;
			}
			default:
				return Vector2();
		}
	} else {
		return Vector2();
	}
}

XRPositionalTracker::TrackerHand XRController3D::get_tracker_hand() const {
	// get our XRServer
	if (tracker.is_null()) {
		return XRPositionalTracker::TRACKER_HAND_UNKNOWN;
	}

	return tracker->get_tracker_hand();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void XRAnchor3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_size"), &XRAnchor3D::get_size);
	ClassDB::bind_method(D_METHOD("get_plane"), &XRAnchor3D::get_plane);
}

Vector3 XRAnchor3D::get_size() const {
	return size;
}

Plane XRAnchor3D::get_plane() const {
	Vector3 location = get_position();
	Basis orientation = get_transform().basis;

	Plane plane(orientation.get_column(1).normalized(), location);

	return plane;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Vector<XROrigin3D *> XROrigin3D::origin_nodes;

PackedStringArray XROrigin3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (is_visible() && is_inside_tree()) {
		bool has_camera = false;
		for (int i = 0; !has_camera && i < get_child_count(); i++) {
			XRCamera3D *camera = Object::cast_to<XRCamera3D>(get_child(i));
			if (camera) {
				// found it!
				has_camera = true;
			}
		}

		if (!has_camera) {
			warnings.push_back(RTR("XROrigin3D requires an XRCamera3D child node."));
		}
	}

	bool xr_enabled = GLOBAL_GET("xr/shaders/enabled");
	if (!xr_enabled) {
		warnings.push_back(RTR("XR shaders are not enabled in project settings. Stereoscopic output is not supported unless they are enabled. Please enable `xr/shaders/enabled` to use stereoscopic output."));
	}

	return warnings;
}

void XROrigin3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_world_scale", "world_scale"), &XROrigin3D::set_world_scale);
	ClassDB::bind_method(D_METHOD("get_world_scale"), &XROrigin3D::get_world_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_scale"), "set_world_scale", "get_world_scale");

	ClassDB::bind_method(D_METHOD("set_current", "enabled"), &XROrigin3D::set_current);
	ClassDB::bind_method(D_METHOD("is_current"), &XROrigin3D::is_current);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "current"), "set_current", "is_current");
}

real_t XROrigin3D::get_world_scale() const {
	// get our XRServer
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, 1.0);

	return xr_server->get_world_scale();
}

void XROrigin3D::set_world_scale(real_t p_world_scale) {
	// get our XRServer
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	xr_server->set_world_scale(p_world_scale);
}

void XROrigin3D::_set_current(bool p_enabled, bool p_update_others) {
	// We run this logic even if current already equals p_enabled as we may have set this previously before we entered our tree.
	// This is then called a second time on NOTIFICATION_ENTER_TREE where we actually process activating this origin node.
	current = p_enabled;

	if (!is_inside_tree() || Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	// Notify us of any transform changes
	set_notify_local_transform(current);
	set_notify_transform(current);

	// update XRServer with our current position
	if (current) {
		XRServer *xr_server = XRServer::get_singleton();
		ERR_FAIL_NULL(xr_server);

		xr_server->set_world_origin(get_global_transform());
	}

	// Check if we need to update our other origin nodes accordingly
	if (p_update_others) {
		if (current) {
			for (int i = 0; i < origin_nodes.size(); i++) {
				if (origin_nodes[i] != this && origin_nodes[i]->current) {
					origin_nodes[i]->_set_current(false, false);
				}
			}
		} else {
			// We no longer have a current origin so find the first one we can make current
			for (int i = 0; i < origin_nodes.size(); i++) {
				if (origin_nodes[i] != this) {
					origin_nodes[i]->_set_current(true, false);
					return; // we are done.
				}
			}
		}
	}
}

void XROrigin3D::set_current(bool p_enabled) {
	_set_current(p_enabled, true);
}

bool XROrigin3D::is_current() const {
	if (Engine::get_singleton()->is_editor_hint()) {
		// return as is
		return current;
	} else {
		return current && is_inside_tree();
	}
}

void XROrigin3D::_notification(int p_what) {
	// get our XRServer
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				if (origin_nodes.is_empty()) {
					// first entry always becomes current
					current = true;
				}

				origin_nodes.push_back(this);

				if (current) {
					// set this again so we do whatever setup is needed.
					set_current(true);
				}
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				origin_nodes.erase(this);

				if (current) {
					// We are no longer current
					set_current(false);
				}
			}
		} break;

		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED:
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (current && !Engine::get_singleton()->is_editor_hint()) {
				xr_server->set_world_origin(get_global_transform());
			}
		} break;
	}

	if (current) {
		// send our notification to all active XE interfaces, they may need to react to it also
		for (int i = 0; i < xr_server->get_interface_count(); i++) {
			Ref<XRInterface> interface = xr_server->get_interface(i);
			if (interface.is_valid() && interface->is_initialized()) {
				interface->notification(p_what);
			}
		}
	}
}
