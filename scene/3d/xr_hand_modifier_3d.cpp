/**************************************************************************/
/*  xr_hand_modifier_3d.cpp                                               */
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

#include "xr_hand_modifier_3d.h"

#include "core/config/project_settings.h"
#include "servers/xr_server.h"

void XRHandModifier3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_hand_tracker", "tracker_name"), &XRHandModifier3D::set_hand_tracker);
	ClassDB::bind_method(D_METHOD("get_hand_tracker"), &XRHandModifier3D::get_hand_tracker);

	ClassDB::bind_method(D_METHOD("set_bone_update", "bone_update"), &XRHandModifier3D::set_bone_update);
	ClassDB::bind_method(D_METHOD("get_bone_update"), &XRHandModifier3D::get_bone_update);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "hand_tracker", PROPERTY_HINT_ENUM_SUGGESTION, "/user/hand_tracker/left,/user/hand_tracker/right"), "set_hand_tracker", "get_hand_tracker");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bone_update", PROPERTY_HINT_ENUM, "Full,Rotation Only"), "set_bone_update", "get_bone_update");

	BIND_ENUM_CONSTANT(BONE_UPDATE_FULL);
	BIND_ENUM_CONSTANT(BONE_UPDATE_ROTATION_ONLY);
	BIND_ENUM_CONSTANT(BONE_UPDATE_MAX);
}

void XRHandModifier3D::set_hand_tracker(const StringName &p_tracker_name) {
	tracker_name = p_tracker_name;
}

StringName XRHandModifier3D::get_hand_tracker() const {
	return tracker_name;
}

void XRHandModifier3D::set_bone_update(BoneUpdate p_bone_update) {
	ERR_FAIL_INDEX(p_bone_update, BONE_UPDATE_MAX);
	bone_update = p_bone_update;
}

XRHandModifier3D::BoneUpdate XRHandModifier3D::get_bone_update() const {
	return bone_update;
}

void XRHandModifier3D::_get_joint_data() {
	if (!is_inside_tree()) {
		return;
	}

	if (has_stored_previous_transforms) {
		previous_relative_transforms.clear();
		has_stored_previous_transforms = false;
	}

	// Table of bone names for different rig types.
	static const String bone_names[XRHandTracker::HAND_JOINT_MAX] = {
		"Palm",
		"Hand",
		"ThumbMetacarpal",
		"ThumbProximal",
		"ThumbDistal",
		"ThumbTip",
		"IndexMetacarpal",
		"IndexProximal",
		"IndexIntermediate",
		"IndexDistal",
		"IndexTip",
		"MiddleMetacarpal",
		"MiddleProximal",
		"MiddleIntermediate",
		"MiddleDistal",
		"MiddleTip",
		"RingMetacarpal",
		"RingProximal",
		"RingIntermediate",
		"RingDistal",
		"RingTip",
		"LittleMetacarpal",
		"LittleProximal",
		"LittleIntermediate",
		"LittleDistal",
		"LittleTip",
	};

	static const String bone_name_format[2] = {
		"Left<bone>",
		"Right<bone>",
	};

	// reset JIC
	for (int i = 0; i < XRHandTracker::HAND_JOINT_MAX; i++) {
		joints[i].bone = -1;
		joints[i].parent_joint = -1;
	}

	const Skeleton3D *skeleton = get_skeleton();
	if (!skeleton) {
		return;
	}

	const XRServer *xr_server = XRServer::get_singleton();
	if (!xr_server) {
		return;
	}

	const Ref<XRHandTracker> tracker = xr_server->get_tracker(tracker_name);
	if (tracker.is_null()) {
		return;
	}

	// Verify we have a left or right hand tracker.
	const XRPositionalTracker::TrackerHand tracker_hand = tracker->get_tracker_hand();
	if (tracker_hand != XRPositionalTracker::TRACKER_HAND_LEFT &&
			tracker_hand != XRPositionalTracker::TRACKER_HAND_RIGHT) {
		return;
	}

	// Get the hand index (0 = left, 1 = right).
	const int hand = tracker_hand == XRPositionalTracker::TRACKER_HAND_LEFT ? 0 : 1;

	// Find the skeleton-bones associated with each joint.
	int bones[XRHandTracker::HAND_JOINT_MAX];
	for (int i = 0; i < XRHandTracker::HAND_JOINT_MAX; i++) {
		// Construct the expected bone name.
		String bone_name = bone_name_format[hand].replace("<bone>", bone_names[i]);

		// Find the skeleton bone.
		bones[i] = skeleton->find_bone(bone_name);
		if (bones[i] == -1) {
			WARN_PRINT(vformat("Couldn't obtain bone for %s", bone_name));
		}
	}

	// Assemble the joint relationship to the available skeleton bones.
	for (int i = 0; i < XRHandTracker::HAND_JOINT_MAX; i++) {
		// Get the skeleton bone (skip if not found).
		const int bone = bones[i];
		if (bone == -1) {
			continue;
		}

		// Find the parent skeleton-bone.
		const int parent_bone = skeleton->get_bone_parent(bone);
		if (parent_bone == -1) {
			// If no parent skeleton-bone exists then drive this relative to palm joint.
			joints[i].bone = bone;
			joints[i].parent_joint = XRHandTracker::HAND_JOINT_PALM;
			continue;
		}

		// Find the joint associated with the parent skeleton-bone.
		for (int j = 0; j < XRHandTracker::HAND_JOINT_MAX; ++j) {
			if (bones[j] == parent_bone) {
				// If a parent joint is found then drive this bone relative to it.
				joints[i].bone = bone;
				joints[i].parent_joint = j;
				break;
			}
		}
	}
}

void XRHandModifier3D::_process_modification() {
	Skeleton3D *skeleton = get_skeleton();
	if (!skeleton) {
		return;
	}

	const XRServer *xr_server = XRServer::get_singleton();
	if (!xr_server) {
		return;
	}

	const Ref<XRHandTracker> tracker = xr_server->get_tracker(tracker_name);
	if (tracker.is_null()) {
		return;
	}

	// Skip if no tracking data
	if (!tracker->get_has_tracking_data()) {
		if (!has_stored_previous_transforms) {
			return;
		}

		// Apply previous relative transforms if they are stored.
		for (int joint = 0; joint < XRHandTracker::HAND_JOINT_MAX; joint++) {
			const int bone = joints[joint].bone;
			if (bone == -1) {
				continue;
			}

			if (bone_update == BONE_UPDATE_FULL) {
				skeleton->set_bone_pose_position(joints[joint].bone, previous_relative_transforms[joint].origin);
			}

			skeleton->set_bone_pose_rotation(joints[joint].bone, Quaternion(previous_relative_transforms[joint].basis));
		}
		return;
	}

	// Get the world and skeleton scale.
	const float ss = skeleton->get_motion_scale();

	// We cache our transforms so we can quickly calculate local transforms.
	bool has_valid_data[XRHandTracker::HAND_JOINT_MAX];
	Transform3D transforms[XRHandTracker::HAND_JOINT_MAX];
	Transform3D inv_transforms[XRHandTracker::HAND_JOINT_MAX];

	for (int joint = 0; joint < XRHandTracker::HAND_JOINT_MAX; joint++) {
		BitField<XRHandTracker::HandJointFlags> flags = tracker->get_hand_joint_flags((XRHandTracker::HandJoint)joint);
		has_valid_data[joint] = flags.has_flag(XRHandTracker::HAND_JOINT_FLAG_ORIENTATION_VALID);

		if (has_valid_data[joint]) {
			transforms[joint] = tracker->get_hand_joint_transform((XRHandTracker::HandJoint)joint);
			transforms[joint].origin *= ss;
			inv_transforms[joint] = transforms[joint].inverse();
		}
	}

	// Skip if palm has no tracking data
	if (!has_valid_data[XRHandTracker::HAND_JOINT_PALM]) {
		return;
	}

	if (!has_stored_previous_transforms) {
		previous_relative_transforms.resize(XRHandTracker::HAND_JOINT_MAX);
		has_stored_previous_transforms = true;
	}
	Transform3D *previous_relative_transforms_ptr = previous_relative_transforms.ptrw();

	for (int joint = 0; joint < XRHandTracker::HAND_JOINT_MAX; joint++) {
		// Get the skeleton bone (skip if none).
		const int bone = joints[joint].bone;
		if (bone == -1) {
			continue;
		}

		// Calculate the relative relationship to the parent bone joint.
		const int parent_joint = joints[joint].parent_joint;
		const Transform3D relative_transform = inv_transforms[parent_joint] * transforms[joint];
		previous_relative_transforms_ptr[joint] = relative_transform;

		// Update the bone position if enabled by update mode.
		if (bone_update == BONE_UPDATE_FULL) {
			skeleton->set_bone_pose_position(joints[joint].bone, relative_transform.origin);
		}

		// Always update the bone rotation.
		skeleton->set_bone_pose_rotation(joints[joint].bone, Quaternion(relative_transform.basis));
	}
}

void XRHandModifier3D::_tracker_changed(StringName p_tracker_name, XRServer::TrackerType p_tracker_type) {
	if (tracker_name == p_tracker_name) {
		_get_joint_data();
	}
}

void XRHandModifier3D::_skeleton_changed(Skeleton3D *p_old, Skeleton3D *p_new) {
	_get_joint_data();
}

PackedStringArray XRHandModifier3D::get_configuration_warnings() const {
	PackedStringArray warnings = SkeletonModifier3D::get_configuration_warnings();

	// Detect OpenXR without the Hand Tracking extension.
	if (GLOBAL_GET("xr/openxr/enabled") && !GLOBAL_GET("xr/openxr/extensions/hand_tracking")) {
		warnings.push_back("XRHandModifier3D requires the OpenXR Hand Tracking extension to be enabled.");
	}

	return warnings;
}

void XRHandModifier3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			XRServer *xr_server = XRServer::get_singleton();
			if (xr_server) {
				xr_server->connect("tracker_added", callable_mp(this, &XRHandModifier3D::_tracker_changed));
				xr_server->connect("tracker_updated", callable_mp(this, &XRHandModifier3D::_tracker_changed));
				xr_server->connect("tracker_removed", callable_mp(this, &XRHandModifier3D::_tracker_changed));
			}

			_get_joint_data();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			XRServer *xr_server = XRServer::get_singleton();
			if (xr_server) {
				xr_server->disconnect("tracker_added", callable_mp(this, &XRHandModifier3D::_tracker_changed));
				xr_server->disconnect("tracker_updated", callable_mp(this, &XRHandModifier3D::_tracker_changed));
				xr_server->disconnect("tracker_removed", callable_mp(this, &XRHandModifier3D::_tracker_changed));
			}

			for (int i = 0; i < XRHandTracker::HAND_JOINT_MAX; i++) {
				joints[i].bone = -1;
				joints[i].parent_joint = -1;
			}
		} break;
		default: {
		} break;
	}
}
