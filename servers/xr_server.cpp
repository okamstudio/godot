/**************************************************************************/
/*  xr_server.cpp                                                         */
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

#include "xr_server.h"
#include "core/config/project_settings.h"
#include "xr/xr_body_tracker.h"
#include "xr/xr_face_tracker.h"
#include "xr/xr_hand_tracker.h"
#include "xr/xr_interface.h"
#include "xr/xr_positional_tracker.h"
#include "xr_server.compat.inc"

XRServer::XRMode XRServer::xr_mode = XRMODE_DEFAULT;

XRServer::XRMode XRServer::get_xr_mode() {
	return xr_mode;
}

void XRServer::set_xr_mode(XRServer::XRMode p_mode) {
	xr_mode = p_mode;
}

XRServer *XRServer::singleton = nullptr;

XRServer *XRServer::get_singleton() {
	return singleton;
}

void XRServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_world_scale"), &XRServer::get_world_scale);
	ClassDB::bind_method(D_METHOD("set_world_scale", "scale"), &XRServer::set_world_scale);
	ClassDB::bind_method(D_METHOD("get_world_origin"), &XRServer::get_world_origin);
	ClassDB::bind_method(D_METHOD("set_world_origin", "world_origin"), &XRServer::set_world_origin);
	ClassDB::bind_method(D_METHOD("get_reference_frame"), &XRServer::get_reference_frame);
	ClassDB::bind_method(D_METHOD("clear_reference_frame"), &XRServer::clear_reference_frame);
	ClassDB::bind_method(D_METHOD("center_on_hmd", "rotation_mode", "keep_height"), &XRServer::center_on_hmd);
	ClassDB::bind_method(D_METHOD("get_hmd_transform"), &XRServer::get_hmd_transform);
	ClassDB::bind_method(D_METHOD("set_camera_locked_to_origin", "enabled"), &XRServer::set_camera_locked_to_origin);
	ClassDB::bind_method(D_METHOD("is_camera_locked_to_origin"), &XRServer::is_camera_locked_to_origin);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "world_scale"), "set_world_scale", "get_world_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "world_origin"), "set_world_origin", "get_world_origin");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "camera_locked_to_origin"), "set_camera_locked_to_origin", "is_camera_locked_to_origin");

	ClassDB::bind_method(D_METHOD("add_interface", "interface"), &XRServer::add_interface);
	ClassDB::bind_method(D_METHOD("get_interface_count"), &XRServer::get_interface_count);
	ClassDB::bind_method(D_METHOD("remove_interface", "interface"), &XRServer::remove_interface);
	ClassDB::bind_method(D_METHOD("get_interface", "idx"), &XRServer::get_interface);
	ClassDB::bind_method(D_METHOD("get_interfaces"), &XRServer::get_interfaces);
	ClassDB::bind_method(D_METHOD("find_interface", "name"), &XRServer::find_interface);

	ClassDB::bind_method(D_METHOD("add_tracker", "tracker"), &XRServer::add_tracker);
	ClassDB::bind_method(D_METHOD("remove_tracker", "tracker"), &XRServer::remove_tracker);
	ClassDB::bind_method(D_METHOD("get_trackers", "tracker_types"), &XRServer::get_trackers);
	ClassDB::bind_method(D_METHOD("get_tracker", "tracker_name"), &XRServer::get_tracker);

	ClassDB::bind_method(D_METHOD("get_primary_interface"), &XRServer::get_primary_interface);
	ClassDB::bind_method(D_METHOD("set_primary_interface", "interface"), &XRServer::set_primary_interface);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "primary_interface"), "set_primary_interface", "get_primary_interface");

	BIND_ENUM_CONSTANT(TRACKER_HEAD);
	BIND_ENUM_CONSTANT(TRACKER_CONTROLLER);
	BIND_ENUM_CONSTANT(TRACKER_BASESTATION);
	BIND_ENUM_CONSTANT(TRACKER_ANCHOR);
	BIND_ENUM_CONSTANT(TRACKER_HAND);
	BIND_ENUM_CONSTANT(TRACKER_BODY);
	BIND_ENUM_CONSTANT(TRACKER_FACE);
	BIND_ENUM_CONSTANT(TRACKER_ANY_KNOWN);
	BIND_ENUM_CONSTANT(TRACKER_UNKNOWN);
	BIND_ENUM_CONSTANT(TRACKER_ANY);

	BIND_ENUM_CONSTANT(RESET_FULL_ROTATION);
	BIND_ENUM_CONSTANT(RESET_BUT_KEEP_TILT);
	BIND_ENUM_CONSTANT(DONT_RESET_ROTATION);

	ADD_SIGNAL(MethodInfo("reference_frame_changed"));

	ADD_SIGNAL(MethodInfo("interface_added", PropertyInfo(Variant::STRING_NAME, "interface_name")));
	ADD_SIGNAL(MethodInfo("interface_removed", PropertyInfo(Variant::STRING_NAME, "interface_name")));

	ADD_SIGNAL(MethodInfo("tracker_added", PropertyInfo(Variant::STRING_NAME, "tracker_name"), PropertyInfo(Variant::INT, "type")));
	ADD_SIGNAL(MethodInfo("tracker_updated", PropertyInfo(Variant::STRING_NAME, "tracker_name"), PropertyInfo(Variant::INT, "type")));
	ADD_SIGNAL(MethodInfo("tracker_removed", PropertyInfo(Variant::STRING_NAME, "tracker_name"), PropertyInfo(Variant::INT, "type")));
}

double XRServer::get_world_scale() const {
	RenderingServer *rendering_server = RenderingServer::get_singleton();

	if (rendering_server && rendering_server->is_on_render_thread()) {
		// Return the value with which we're currently rendering,
		// if we're on the render thread
		return render_state.world_scale;
	} else {
		// Return our current value
		return world_scale;
	}
}

void XRServer::set_world_scale(double p_world_scale) {
	if (p_world_scale < 0.01) {
		p_world_scale = 0.01;
	} else if (p_world_scale > 1000.0) {
		p_world_scale = 1000.0;
	}

	world_scale = p_world_scale;
	set_render_world_scale(world_scale);
}

void XRServer::_set_render_world_scale(double p_world_scale) {
	// Must be called from rendering thread!
	ERR_NOT_ON_RENDER_THREAD;

	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);
	xr_server->render_state.world_scale = p_world_scale;
}

Transform3D XRServer::get_world_origin() const {
	RenderingServer *rendering_server = RenderingServer::get_singleton();

	if (rendering_server && rendering_server->is_on_render_thread()) {
		// Return the value with which we're currently rendering,
		// if we're on the render thread
		return render_state.world_origin;
	} else {
		// Return our current value
		return world_origin;
	}
}

void XRServer::set_world_origin(const Transform3D &p_world_origin) {
	world_origin = p_world_origin;
	set_render_world_origin(world_origin);
}

void XRServer::_set_render_world_origin(const Transform3D &p_world_origin) {
	// Must be called from rendering thread!
	ERR_NOT_ON_RENDER_THREAD;

	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);
	xr_server->render_state.world_origin = p_world_origin;
}

Transform3D XRServer::get_reference_frame() const {
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL_V(rendering_server, reference_frame);

	if (rendering_server->is_on_render_thread()) {
		// Return the value with which we're currently rendering,
		// if we're on the render thread
		return render_state.reference_frame;
	} else {
		// Return our current value
		return reference_frame;
	}
}

void XRServer::center_on_hmd(RotationMode p_rotation_mode, bool p_keep_height) {
	if (primary_interface.is_null()) {
		return;
	}

	// clear our current reference frame or we'll end up double adjusting it
	reference_frame = Transform3D();

	// requesting our EYE_MONO transform should return our current HMD position
	Transform3D new_reference_frame = primary_interface->get_camera_transform();

	// remove our tilt
	if (p_rotation_mode == 1) {
		// take the Y out of our Z
		new_reference_frame.basis.set_column(2, Vector3(new_reference_frame.basis.rows[0][2], 0.0, new_reference_frame.basis.rows[2][2]).normalized());

		// Y is straight up
		new_reference_frame.basis.set_column(1, Vector3(0.0, 1.0, 0.0));

		// and X is our cross reference
		new_reference_frame.basis.set_column(0, new_reference_frame.basis.get_column(1).cross(new_reference_frame.basis.get_column(2)).normalized());
	} else if (p_rotation_mode == 2) {
		// remove our rotation, we're only interesting in centering on position
		new_reference_frame.basis = Basis();
	}

	// don't negate our height
	if (p_keep_height) {
		new_reference_frame.origin.y = 0.0;
	}

	reference_frame = new_reference_frame.inverse();
	set_render_reference_frame(reference_frame);
	emit_signal(SNAME("reference_frame_changed"));
}

void XRServer::clear_reference_frame() {
	reference_frame = Transform3D();
	set_render_reference_frame(reference_frame);
	emit_signal(SNAME("reference_frame_changed"));
}

void XRServer::_set_render_reference_frame(const Transform3D &p_reference_frame) {
	// Must be called from rendering thread!
	ERR_NOT_ON_RENDER_THREAD;

	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);
	xr_server->render_state.reference_frame = p_reference_frame;
}

Transform3D XRServer::get_hmd_transform() {
	Transform3D hmd_transform;
	if (primary_interface.is_valid()) {
		hmd_transform = primary_interface->get_camera_transform();
	}
	return hmd_transform;
}

void XRServer::set_camera_locked_to_origin(bool p_enable) {
	camera_locked_to_origin = p_enable;
}

void XRServer::add_interface(const Ref<XRInterface> &p_interface) {
	ERR_FAIL_COND(p_interface.is_null());

	for (int i = 0; i < interfaces.size(); i++) {
		if (interfaces[i] == p_interface) {
			ERR_PRINT("Interface was already added");
			return;
		}
	}

	interfaces.push_back(p_interface);
	emit_signal(SNAME("interface_added"), p_interface->get_name());
}

void XRServer::remove_interface(const Ref<XRInterface> &p_interface) {
	ERR_FAIL_COND(p_interface.is_null());

	int idx = -1;
	for (int i = 0; i < interfaces.size(); i++) {
		if (interfaces[i] == p_interface) {
			idx = i;
			break;
		}
	}

	ERR_FAIL_COND_MSG(idx == -1, "Interface not found.");
	print_verbose("XR: Removed interface \"" + p_interface->get_name() + "\"");
	emit_signal(SNAME("interface_removed"), p_interface->get_name());
	interfaces.remove_at(idx);
}

int XRServer::get_interface_count() const {
	return interfaces.size();
}

Ref<XRInterface> XRServer::get_interface(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, interfaces.size(), nullptr);

	return interfaces[p_index];
}

Ref<XRInterface> XRServer::find_interface(const String &p_name) const {
	for (int i = 0; i < interfaces.size(); i++) {
		if (interfaces[i]->get_name() == p_name) {
			return interfaces[i];
		}
	}
	return Ref<XRInterface>();
}

TypedArray<Dictionary> XRServer::get_interfaces() const {
	Array ret;

	for (int i = 0; i < interfaces.size(); i++) {
		Dictionary iface_info;

		iface_info["id"] = i;
		iface_info["name"] = interfaces[i]->get_name();

		ret.push_back(iface_info);
	}

	return ret;
}

Ref<XRInterface> XRServer::get_primary_interface() const {
	return primary_interface;
}

void XRServer::set_primary_interface(const Ref<XRInterface> &p_primary_interface) {
	if (p_primary_interface.is_null()) {
		print_verbose("XR: Clearing primary interface");
		primary_interface.unref();
	} else {
		primary_interface = p_primary_interface;

		print_verbose("XR: Primary interface set to: " + primary_interface->get_name());
	}
}

void XRServer::add_tracker(const Ref<XRTracker> &p_tracker) {
	ERR_FAIL_COND(p_tracker.is_null());

	StringName tracker_name = p_tracker->get_tracker_name();
	if (trackers.has(tracker_name)) {
		if (trackers[tracker_name] != p_tracker) {
			// We already have a tracker with this name, we're going to replace it
			trackers[tracker_name] = p_tracker;
			emit_signal(SNAME("tracker_updated"), tracker_name, p_tracker->get_tracker_type());
		}
	} else {
		trackers[tracker_name] = p_tracker;
		emit_signal(SNAME("tracker_added"), tracker_name, p_tracker->get_tracker_type());
	}
}

void XRServer::remove_tracker(const Ref<XRTracker> &p_tracker) {
	ERR_FAIL_COND(p_tracker.is_null());

	StringName tracker_name = p_tracker->get_tracker_name();
	if (trackers.has(tracker_name)) {
		// we send the signal right before removing it
		emit_signal(SNAME("tracker_removed"), p_tracker->get_tracker_name(), p_tracker->get_tracker_type());

		// and remove it
		trackers.erase(tracker_name);
	}
}

Dictionary XRServer::get_trackers(int p_tracker_types) {
	Dictionary res;

	for (int i = 0; i < trackers.size(); i++) {
		Ref<XRPositionalTracker> tracker = trackers.get_value_at_index(i);
		if (tracker.is_valid() && (tracker->get_tracker_type() & p_tracker_types) != 0) {
			res[tracker->get_tracker_name()] = tracker;
		}
	}

	return res;
}

Ref<XRTracker> XRServer::get_tracker(const StringName &p_name) const {
	if (trackers.has(p_name)) {
		return trackers[p_name];
	} else {
		// tracker hasn't been registered yet, which is fine, no need to spam the error log...
		return Ref<XRTracker>();
	}
}

PackedStringArray XRServer::get_suggested_tracker_names() const {
	PackedStringArray arr;

	for (int i = 0; i < interfaces.size(); i++) {
		Ref<XRInterface> interface = interfaces[i];
		PackedStringArray interface_arr = interface->get_suggested_tracker_names();
		for (int a = 0; a < interface_arr.size(); a++) {
			if (!arr.has(interface_arr[a])) {
				arr.push_back(interface_arr[a]);
			}
		}
	}

	if (arr.size() == 0) {
		// no suggestions from our tracker? include our defaults
		arr.push_back(String("head"));
		arr.push_back(String("left_hand"));
		arr.push_back(String("right_hand"));
	}

	return arr;
}

PackedStringArray XRServer::get_suggested_pose_names(const StringName &p_tracker_name) const {
	PackedStringArray arr;

	for (int i = 0; i < interfaces.size(); i++) {
		Ref<XRInterface> interface = interfaces[i];
		PackedStringArray interface_arr = interface->get_suggested_pose_names(p_tracker_name);
		for (int a = 0; a < interface_arr.size(); a++) {
			if (!arr.has(interface_arr[a])) {
				arr.push_back(interface_arr[a]);
			}
		}
	}

	if (arr.size() == 0) {
		// no suggestions from our tracker? include our defaults
		arr.push_back(String("default"));

		if ((p_tracker_name == "left_hand") || (p_tracker_name == "right_hand")) {
			arr.push_back(String("aim"));
			arr.push_back(String("grip"));
			arr.push_back(String("skeleton"));
		}
	}

	return arr;
}

void XRServer::_process() {
	// called from our main game loop before we handle physics and game logic
	// note that we can have multiple interfaces active if we have interfaces that purely handle tracking

	// process all active interfaces
	for (int i = 0; i < interfaces.size(); i++) {
		if (!interfaces[i].is_valid()) {
			// ignore, not a valid reference
		} else if (interfaces[i]->is_initialized()) {
			interfaces.write[i]->process();
		}
	}
}

void XRServer::pre_render() {
	// called from RendererViewport.draw_viewports right before we start drawing our viewports
	// note that we can have multiple interfaces active if we have interfaces that purely handle tracking

	// process all active interfaces
	for (int i = 0; i < interfaces.size(); i++) {
		if (!interfaces[i].is_valid()) {
			// ignore, not a valid reference
		} else if (interfaces[i]->is_initialized()) {
			interfaces.write[i]->pre_render();
		}
	}
}

void XRServer::end_frame() {
	// called from RenderingServerDefault after Vulkan queues have been submitted

	// process all active interfaces
	for (int i = 0; i < interfaces.size(); i++) {
		if (!interfaces[i].is_valid()) {
			// ignore, not a valid reference
		} else if (interfaces[i]->is_initialized()) {
			interfaces.write[i]->end_frame();
		}
	}
}

XRServer::XRServer() {
	singleton = this;
}

XRServer::~XRServer() {
	primary_interface.unref();

	interfaces.clear();
	trackers.clear();

	singleton = nullptr;
}
