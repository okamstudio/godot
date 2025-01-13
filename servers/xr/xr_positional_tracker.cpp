/**************************************************************************/
/*  xr_positional_tracker.cpp                                             */
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

#include "xr_positional_tracker.h"

#include "xr_controller_tracker.h"

void XRPositionalTracker::_bind_methods() {
	BIND_ENUM_CONSTANT(TRACKER_HAND_UNKNOWN);
	BIND_ENUM_CONSTANT(TRACKER_HAND_LEFT);
	BIND_ENUM_CONSTANT(TRACKER_HAND_RIGHT);
	BIND_ENUM_CONSTANT(TRACKER_HAND_MAX);

	ClassDB::bind_method(D_METHOD("get_tracker_profile"), &XRPositionalTracker::get_tracker_profile);
	ClassDB::bind_method(D_METHOD("set_tracker_profile", "profile"), &XRPositionalTracker::set_tracker_profile);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "profile"), "set_tracker_profile", "get_tracker_profile");

	ClassDB::bind_method(D_METHOD("get_tracker_hand"), &XRPositionalTracker::get_tracker_hand);
	ClassDB::bind_method(D_METHOD("set_tracker_hand", "hand"), &XRPositionalTracker::set_tracker_hand);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hand", PROPERTY_HINT_ENUM, "Unknown,Left,Right"), "set_tracker_hand", "get_tracker_hand");

	ClassDB::bind_method(D_METHOD("has_pose", "name"), &XRPositionalTracker::has_pose);
	ClassDB::bind_method(D_METHOD("get_pose", "name"), &XRPositionalTracker::get_pose);
	ClassDB::bind_method(D_METHOD("invalidate_pose", "name"), &XRPositionalTracker::invalidate_pose);
	ClassDB::bind_method(D_METHOD("set_pose", "name", "transform", "linear_velocity", "angular_velocity", "tracking_confidence"), &XRPositionalTracker::set_pose);
	ADD_SIGNAL(MethodInfo("pose_changed", PropertyInfo(Variant::OBJECT, "pose", PROPERTY_HINT_RESOURCE_TYPE, "XRPose")));
	ADD_SIGNAL(MethodInfo("pose_lost_tracking", PropertyInfo(Variant::OBJECT, "pose", PROPERTY_HINT_RESOURCE_TYPE, "XRPose")));

	ClassDB::bind_method(D_METHOD("get_input", "name"), &XRPositionalTracker::get_input);
	ClassDB::bind_method(D_METHOD("set_input", "name", "value"), &XRPositionalTracker::set_input);
	ADD_SIGNAL(MethodInfo("button_pressed", PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo("button_released", PropertyInfo(Variant::STRING, "name")));
	ADD_SIGNAL(MethodInfo("input_float_changed", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::FLOAT, "value")));
	ADD_SIGNAL(MethodInfo("input_vector2_changed", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::VECTOR2, "vector")));
	ADD_SIGNAL(MethodInfo("profile_changed", PropertyInfo(Variant::STRING, "role")));
}

void XRPositionalTracker::set_tracker_profile(const String &p_profile) {
	if (profile != p_profile) {
		profile = p_profile;

		emit_signal("profile_changed", profile);
	}
}

String XRPositionalTracker::get_tracker_profile() const {
	return profile;
}

XRPositionalTracker::TrackerHand XRPositionalTracker::get_tracker_hand() const {
	return tracker_hand;
}

void XRPositionalTracker::set_tracker_hand(const XRPositionalTracker::TrackerHand p_hand) {
	ERR_FAIL_INDEX(p_hand, TRACKER_HAND_MAX);
	tracker_hand = p_hand;
}

bool XRPositionalTracker::has_pose(const StringName &p_action_name) const {
	return poses.has(p_action_name);
}

Ref<XRPose> XRPositionalTracker::get_pose(const StringName &p_action_name) const {
	Ref<XRPose> pose;

	if (poses.has(p_action_name)) {
		pose = poses[p_action_name];
	}

	return pose;
}

void XRPositionalTracker::invalidate_pose(const StringName &p_action_name) {
	// only update this if we were tracking this pose
	if (poses.has(p_action_name)) {
		// We just set tracking data as invalid, we leave our current transform and velocity data as is so controllers don't suddenly jump to origin.
		Ref<XRPose> pose = poses[p_action_name];
		pose->set_has_tracking_data(false);

		emit_signal(SNAME("pose_lost_tracking"), pose);
	}
}

void XRPositionalTracker::set_pose(const StringName &p_action_name, const Transform3D &p_transform, const Vector3 &p_linear_velocity, const Vector3 &p_angular_velocity, const XRPose::TrackingConfidence p_tracking_confidence) {
	Ref<XRPose> new_pose;

	if (poses.has(p_action_name)) {
		new_pose = poses[p_action_name];
	} else {
		new_pose.instantiate();
		poses[p_action_name] = new_pose;
	}

	new_pose->set_name(p_action_name);
	new_pose->set_has_tracking_data(true);
	new_pose->set_transform(p_transform);
	new_pose->set_linear_velocity(p_linear_velocity);
	new_pose->set_angular_velocity(p_angular_velocity);
	new_pose->set_tracking_confidence(p_tracking_confidence);

	emit_signal(SNAME("pose_changed"), new_pose);

	// TODO discuss whether we also want to create and emit an InputEventXRPose event
}

Variant XRPositionalTracker::get_input(const StringName &p_action_name) const {
	// Complain if this method is called on a XRPositionalTracker instance.
	if (!dynamic_cast<const XRControllerTracker *>(this)) {
		WARN_DEPRECATED_MSG(R"*(The "get_input()" method is deprecated, use "XRControllerTracker" instead.)*");
	}

	if (inputs.has(p_action_name)) {
		return inputs[p_action_name];
	} else {
		return Variant();
	}
}

void XRPositionalTracker::set_input(const StringName &p_action_name, const Variant &p_value) {
	// Complain if this method is called on a XRPositionalTracker instance.
	if (!dynamic_cast<XRControllerTracker *>(this)) {
		WARN_DEPRECATED_MSG(R"*(The "set_input()" method is deprecated, use "XRControllerTracker" instead.)*");
	}

	// XR inputs
	bool changed;
	if (inputs.has(p_action_name)) {
		changed = inputs[p_action_name] != p_value;
	} else {
		changed = true;
	}

	if (changed) {
		// store the new value
		inputs[p_action_name] = p_value;

		// emit signals to let the rest of the world know
		switch (p_value.get_type()) {
			case Variant::BOOL: {
				bool pressed = p_value;
				if (pressed) {
					emit_signal(SNAME("button_pressed"), p_action_name);
				} else {
					emit_signal(SNAME("button_released"), p_action_name);
				}

				// TODO discuss whether we also want to create and emit an InputEventXRButton event
			} break;
			case Variant::FLOAT: {
				emit_signal(SNAME("input_float_changed"), p_action_name, p_value);

				// TODO discuss whether we also want to create and emit an InputEventXRValue event
			} break;
			case Variant::VECTOR2: {
				emit_signal(SNAME("input_vector2_changed"), p_action_name, p_value);

				// TODO discuss whether we also want to create and emit an InputEventXRAxis event
			} break;
			default: {
				// ???
			} break;
		}
	}
}
