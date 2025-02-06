/**************************************************************************/
/*  xr_body_tracker.h                                                     */
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

#pragma once

#include "servers/xr/xr_positional_tracker.h"

class XRBodyTracker : public XRPositionalTracker {
	GDCLASS(XRBodyTracker, XRPositionalTracker);
	_THREAD_SAFE_CLASS_

public:
	enum BodyFlags {
		BODY_FLAG_UPPER_BODY_SUPPORTED = 1,
		BODY_FLAG_LOWER_BODY_SUPPORTED = 2,
		BODY_FLAG_HANDS_SUPPORTED = 4,
	};

	enum Joint {
		JOINT_ROOT,

		// Upper Body Joints
		JOINT_HIPS,
		JOINT_SPINE,
		JOINT_CHEST,
		JOINT_UPPER_CHEST,
		JOINT_NECK,
		JOINT_HEAD,
		JOINT_HEAD_TIP,
		JOINT_LEFT_SHOULDER,
		JOINT_LEFT_UPPER_ARM,
		JOINT_LEFT_LOWER_ARM,
		JOINT_RIGHT_SHOULDER,
		JOINT_RIGHT_UPPER_ARM,
		JOINT_RIGHT_LOWER_ARM,

		// Lower Body Joints
		JOINT_LEFT_UPPER_LEG,
		JOINT_LEFT_LOWER_LEG,
		JOINT_LEFT_FOOT,
		JOINT_LEFT_TOES,
		JOINT_RIGHT_UPPER_LEG,
		JOINT_RIGHT_LOWER_LEG,
		JOINT_RIGHT_FOOT,
		JOINT_RIGHT_TOES,

		// Left Hand Joints
		JOINT_LEFT_HAND,
		JOINT_LEFT_PALM,
		JOINT_LEFT_WRIST,
		JOINT_LEFT_THUMB_METACARPAL,
		JOINT_LEFT_THUMB_PHALANX_PROXIMAL,
		JOINT_LEFT_THUMB_PHALANX_DISTAL,
		JOINT_LEFT_THUMB_TIP,
		JOINT_LEFT_INDEX_FINGER_METACARPAL,
		JOINT_LEFT_INDEX_FINGER_PHALANX_PROXIMAL,
		JOINT_LEFT_INDEX_FINGER_PHALANX_INTERMEDIATE,
		JOINT_LEFT_INDEX_FINGER_PHALANX_DISTAL,
		JOINT_LEFT_INDEX_FINGER_TIP,
		JOINT_LEFT_MIDDLE_FINGER_METACARPAL,
		JOINT_LEFT_MIDDLE_FINGER_PHALANX_PROXIMAL,
		JOINT_LEFT_MIDDLE_FINGER_PHALANX_INTERMEDIATE,
		JOINT_LEFT_MIDDLE_FINGER_PHALANX_DISTAL,
		JOINT_LEFT_MIDDLE_FINGER_TIP,
		JOINT_LEFT_RING_FINGER_METACARPAL,
		JOINT_LEFT_RING_FINGER_PHALANX_PROXIMAL,
		JOINT_LEFT_RING_FINGER_PHALANX_INTERMEDIATE,
		JOINT_LEFT_RING_FINGER_PHALANX_DISTAL,
		JOINT_LEFT_RING_FINGER_TIP,
		JOINT_LEFT_PINKY_FINGER_METACARPAL,
		JOINT_LEFT_PINKY_FINGER_PHALANX_PROXIMAL,
		JOINT_LEFT_PINKY_FINGER_PHALANX_INTERMEDIATE,
		JOINT_LEFT_PINKY_FINGER_PHALANX_DISTAL,
		JOINT_LEFT_PINKY_FINGER_TIP,

		// Right Hand Joints
		JOINT_RIGHT_HAND,
		JOINT_RIGHT_PALM,
		JOINT_RIGHT_WRIST,
		JOINT_RIGHT_THUMB_METACARPAL,
		JOINT_RIGHT_THUMB_PHALANX_PROXIMAL,
		JOINT_RIGHT_THUMB_PHALANX_DISTAL,
		JOINT_RIGHT_THUMB_TIP,
		JOINT_RIGHT_INDEX_FINGER_METACARPAL,
		JOINT_RIGHT_INDEX_FINGER_PHALANX_PROXIMAL,
		JOINT_RIGHT_INDEX_FINGER_PHALANX_INTERMEDIATE,
		JOINT_RIGHT_INDEX_FINGER_PHALANX_DISTAL,
		JOINT_RIGHT_INDEX_FINGER_TIP,
		JOINT_RIGHT_MIDDLE_FINGER_METACARPAL,
		JOINT_RIGHT_MIDDLE_FINGER_PHALANX_PROXIMAL,
		JOINT_RIGHT_MIDDLE_FINGER_PHALANX_INTERMEDIATE,
		JOINT_RIGHT_MIDDLE_FINGER_PHALANX_DISTAL,
		JOINT_RIGHT_MIDDLE_FINGER_TIP,
		JOINT_RIGHT_RING_FINGER_METACARPAL,
		JOINT_RIGHT_RING_FINGER_PHALANX_PROXIMAL,
		JOINT_RIGHT_RING_FINGER_PHALANX_INTERMEDIATE,
		JOINT_RIGHT_RING_FINGER_PHALANX_DISTAL,
		JOINT_RIGHT_RING_FINGER_TIP,
		JOINT_RIGHT_PINKY_FINGER_METACARPAL,
		JOINT_RIGHT_PINKY_FINGER_PHALANX_PROXIMAL,
		JOINT_RIGHT_PINKY_FINGER_PHALANX_INTERMEDIATE,
		JOINT_RIGHT_PINKY_FINGER_PHALANX_DISTAL,
		JOINT_RIGHT_PINKY_FINGER_TIP,

		JOINT_MAX,
	};

	enum JointFlags {
		JOINT_FLAG_ORIENTATION_VALID = 1,
		JOINT_FLAG_ORIENTATION_TRACKED = 2,
		JOINT_FLAG_POSITION_VALID = 4,
		JOINT_FLAG_POSITION_TRACKED = 8,
	};

	void set_tracker_type(XRServer::TrackerType p_type) override;
	void set_tracker_hand(const XRPositionalTracker::TrackerHand p_hand) override;

	void set_has_tracking_data(bool p_has_tracking_data);
	bool get_has_tracking_data() const;

	void set_body_flags(BitField<BodyFlags> p_body_flags);
	BitField<BodyFlags> get_body_flags() const;

	void set_joint_flags(Joint p_joint, BitField<JointFlags> p_flags);
	BitField<JointFlags> get_joint_flags(Joint p_joint) const;

	void set_joint_transform(Joint p_joint, const Transform3D &p_transform);
	Transform3D get_joint_transform(Joint p_joint) const;

	XRBodyTracker();

protected:
	static void _bind_methods();

private:
	bool has_tracking_data = false;
	BitField<BodyFlags> body_flags;

	BitField<JointFlags> joint_flags[JOINT_MAX];
	Transform3D joint_transforms[JOINT_MAX];
};

VARIANT_BITFIELD_CAST(XRBodyTracker::BodyFlags)
VARIANT_ENUM_CAST(XRBodyTracker::Joint)
VARIANT_BITFIELD_CAST(XRBodyTracker::JointFlags)
