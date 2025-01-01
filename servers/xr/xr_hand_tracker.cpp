/**************************************************************************/
/*  xr_hand_tracker.cpp                                                   */
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

#include "xr_hand_tracker.h"

void XRHandTracker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_has_tracking_data", "has_data"), &XRHandTracker::set_has_tracking_data);
	ClassDB::bind_method(D_METHOD("get_has_tracking_data"), &XRHandTracker::get_has_tracking_data);

	ClassDB::bind_method(D_METHOD("set_hand_tracking_source", "source"), &XRHandTracker::set_hand_tracking_source);
	ClassDB::bind_method(D_METHOD("get_hand_tracking_source"), &XRHandTracker::get_hand_tracking_source);

	ClassDB::bind_method(D_METHOD("set_hand_joint_flags", "joint", "flags"), &XRHandTracker::set_hand_joint_flags);
	ClassDB::bind_method(D_METHOD("get_hand_joint_flags", "joint"), &XRHandTracker::get_hand_joint_flags);

	ClassDB::bind_method(D_METHOD("set_hand_joint_transform", "joint", "transform"), &XRHandTracker::set_hand_joint_transform);
	ClassDB::bind_method(D_METHOD("get_hand_joint_transform", "joint"), &XRHandTracker::get_hand_joint_transform);

	ClassDB::bind_method(D_METHOD("set_hand_joint_radius", "joint", "radius"), &XRHandTracker::set_hand_joint_radius);
	ClassDB::bind_method(D_METHOD("get_hand_joint_radius", "joint"), &XRHandTracker::get_hand_joint_radius);

	ClassDB::bind_method(D_METHOD("set_hand_joint_linear_velocity", "joint", "linear_velocity"), &XRHandTracker::set_hand_joint_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_hand_joint_linear_velocity", "joint"), &XRHandTracker::get_hand_joint_linear_velocity);

	ClassDB::bind_method(D_METHOD("set_hand_joint_angular_velocity", "joint", "angular_velocity"), &XRHandTracker::set_hand_joint_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_hand_joint_angular_velocity", "joint"), &XRHandTracker::get_hand_joint_angular_velocity);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "has_tracking_data", PROPERTY_HINT_NONE), "set_has_tracking_data", "get_has_tracking_data");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "hand_tracking_source", PROPERTY_HINT_ENUM, "Unknown,Unobstructed,Controller"), "set_hand_tracking_source", "get_hand_tracking_source");

	BIND_ENUM_CONSTANT(HAND_TRACKING_SOURCE_UNKNOWN);
	BIND_ENUM_CONSTANT(HAND_TRACKING_SOURCE_UNOBSTRUCTED);
	BIND_ENUM_CONSTANT(HAND_TRACKING_SOURCE_CONTROLLER);
	BIND_ENUM_CONSTANT(HAND_TRACKING_SOURCE_NOT_TRACKED);
	BIND_ENUM_CONSTANT(HAND_TRACKING_SOURCE_MAX);

	BIND_ENUM_CONSTANT(HAND_JOINT_PALM);
	BIND_ENUM_CONSTANT(HAND_JOINT_WRIST);
	BIND_ENUM_CONSTANT(HAND_JOINT_THUMB_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_THUMB_PHALANX_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_THUMB_PHALANX_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_THUMB_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_FINGER_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_FINGER_PHALANX_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_FINGER_PHALANX_INTERMEDIATE);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_FINGER_PHALANX_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_FINGER_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_FINGER_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_FINGER_PHALANX_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_FINGER_PHALANX_INTERMEDIATE);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_FINGER_PHALANX_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_FINGER_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_FINGER_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_FINGER_PHALANX_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_FINGER_PHALANX_INTERMEDIATE);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_FINGER_PHALANX_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_FINGER_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_PINKY_FINGER_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_PINKY_FINGER_PHALANX_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_PINKY_FINGER_PHALANX_INTERMEDIATE);
	BIND_ENUM_CONSTANT(HAND_JOINT_PINKY_FINGER_PHALANX_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_PINKY_FINGER_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_MAX);

	BIND_BITFIELD_FLAG(HAND_JOINT_FLAG_ORIENTATION_VALID);
	BIND_BITFIELD_FLAG(HAND_JOINT_FLAG_ORIENTATION_TRACKED);
	BIND_BITFIELD_FLAG(HAND_JOINT_FLAG_POSITION_VALID);
	BIND_BITFIELD_FLAG(HAND_JOINT_FLAG_POSITION_TRACKED);
	BIND_BITFIELD_FLAG(HAND_JOINT_FLAG_LINEAR_VELOCITY_VALID);
	BIND_BITFIELD_FLAG(HAND_JOINT_FLAG_ANGULAR_VELOCITY_VALID);
}

void XRHandTracker::set_tracker_type(XRServer::TrackerType p_type) {
	ERR_FAIL_COND_MSG(p_type != XRServer::TRACKER_HAND, "XRHandTracker must be of type TRACKER_HAND.");
}

void XRHandTracker::set_tracker_hand(const XRPositionalTracker::TrackerHand p_hand) {
	ERR_FAIL_COND_MSG(p_hand != TRACKER_HAND_LEFT && p_hand != TRACKER_HAND_RIGHT, "XRHandTracker must specify hand.");
	tracker_hand = p_hand;
}

void XRHandTracker::set_has_tracking_data(bool p_has_tracking_data) {
	has_tracking_data = p_has_tracking_data;
}

bool XRHandTracker::get_has_tracking_data() const {
	return has_tracking_data;
}

void XRHandTracker::set_hand_tracking_source(XRHandTracker::HandTrackingSource p_source) {
	hand_tracking_source = p_source;
}

XRHandTracker::HandTrackingSource XRHandTracker::get_hand_tracking_source() const {
	return hand_tracking_source;
}

void XRHandTracker::set_hand_joint_flags(XRHandTracker::HandJoint p_joint, BitField<XRHandTracker::HandJointFlags> p_flags) {
	ERR_FAIL_INDEX(p_joint, HAND_JOINT_MAX);
	hand_joint_flags[p_joint] = p_flags;
}

BitField<XRHandTracker::HandJointFlags> XRHandTracker::get_hand_joint_flags(XRHandTracker::HandJoint p_joint) const {
	ERR_FAIL_INDEX_V(p_joint, HAND_JOINT_MAX, BitField<HandJointFlags>());
	return hand_joint_flags[p_joint];
}

void XRHandTracker::set_hand_joint_transform(XRHandTracker::HandJoint p_joint, const Transform3D &p_transform) {
	ERR_FAIL_INDEX(p_joint, HAND_JOINT_MAX);
	hand_joint_transforms[p_joint] = p_transform;
}

Transform3D XRHandTracker::get_hand_joint_transform(XRHandTracker::HandJoint p_joint) const {
	ERR_FAIL_INDEX_V(p_joint, HAND_JOINT_MAX, Transform3D());
	return hand_joint_transforms[p_joint];
}

void XRHandTracker::set_hand_joint_radius(XRHandTracker::HandJoint p_joint, float p_radius) {
	ERR_FAIL_INDEX(p_joint, HAND_JOINT_MAX);
	hand_joint_radii[p_joint] = p_radius;
}

float XRHandTracker::get_hand_joint_radius(XRHandTracker::HandJoint p_joint) const {
	ERR_FAIL_INDEX_V(p_joint, HAND_JOINT_MAX, 0.0);
	return hand_joint_radii[p_joint];
}

void XRHandTracker::set_hand_joint_linear_velocity(XRHandTracker::HandJoint p_joint, const Vector3 &p_velocity) {
	ERR_FAIL_INDEX(p_joint, HAND_JOINT_MAX);
	hand_joint_linear_velocities[p_joint] = p_velocity;
}

Vector3 XRHandTracker::get_hand_joint_linear_velocity(XRHandTracker::HandJoint p_joint) const {
	ERR_FAIL_INDEX_V(p_joint, HAND_JOINT_MAX, Vector3());
	return hand_joint_linear_velocities[p_joint];
}

void XRHandTracker::set_hand_joint_angular_velocity(XRHandTracker::HandJoint p_joint, const Vector3 &p_velocity) {
	ERR_FAIL_INDEX(p_joint, HAND_JOINT_MAX);
	hand_joint_angular_velocities[p_joint] = p_velocity;
}

Vector3 XRHandTracker::get_hand_joint_angular_velocity(XRHandTracker::HandJoint p_joint) const {
	ERR_FAIL_INDEX_V(p_joint, HAND_JOINT_MAX, Vector3());
	return hand_joint_angular_velocities[p_joint];
}

XRHandTracker::XRHandTracker() {
	type = XRServer::TRACKER_HAND;
	tracker_hand = TRACKER_HAND_LEFT;
}
