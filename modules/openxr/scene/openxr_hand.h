/**************************************************************************/
/*  openxr_hand.h                                                         */
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

#include "scene/3d/node_3d.h"
#include "scene/3d/skeleton_3d.h"

#include <openxr/openxr.h>

class OpenXRAPI;
class OpenXRHandTrackingExtension;

class OpenXRHand : public Node3D {
	GDCLASS(OpenXRHand, Node3D);

public:
	enum Hands { // Deprecated, need to change this to OpenXRInterface::Hands.
		HAND_LEFT,
		HAND_RIGHT,
		HAND_MAX
	};

	enum MotionRange { // Deprecated, need to change this to OpenXRInterface::HandMotionRange.
		MOTION_RANGE_UNOBSTRUCTED,
		MOTION_RANGE_CONFORM_TO_CONTROLLER,
		MOTION_RANGE_MAX
	};

	enum SkeletonRig {
		SKELETON_RIG_OPENXR,
		SKELETON_RIG_HUMANOID,
		SKELETON_RIG_MAX
	};

	enum BoneUpdate {
		BONE_UPDATE_FULL,
		BONE_UPDATE_ROTATION_ONLY,
		BONE_UPDATE_MAX
	};

private:
	struct JointData {
		int bone = -1;
		int parent_joint = -1;
	};

	OpenXRAPI *openxr_api = nullptr;
	OpenXRHandTrackingExtension *hand_tracking_ext = nullptr;

	Hands hand = HAND_LEFT;
	MotionRange motion_range = MOTION_RANGE_UNOBSTRUCTED;
	NodePath hand_skeleton;
	SkeletonRig skeleton_rig = SKELETON_RIG_OPENXR;
	BoneUpdate bone_update = BONE_UPDATE_FULL;

	JointData joints[XR_HAND_JOINT_COUNT_EXT];

	void _set_motion_range();

	Skeleton3D *get_skeleton();
	void _get_joint_data();
	void _update_skeleton();

protected:
	static void _bind_methods();

public:
	OpenXRHand();

	void set_hand(Hands p_hand);
	Hands get_hand() const;

	void set_motion_range(MotionRange p_motion_range);
	MotionRange get_motion_range() const;

	void set_hand_skeleton(const NodePath &p_hand_skeleton);
	NodePath get_hand_skeleton() const;

	void set_skeleton_rig(SkeletonRig p_skeleton_rig);
	SkeletonRig get_skeleton_rig() const;

	void set_bone_update(BoneUpdate p_bone_update);
	BoneUpdate get_bone_update() const;

	void _notification(int p_what);
};

VARIANT_ENUM_CAST(OpenXRHand::Hands)
VARIANT_ENUM_CAST(OpenXRHand::MotionRange)
VARIANT_ENUM_CAST(OpenXRHand::SkeletonRig)
VARIANT_ENUM_CAST(OpenXRHand::BoneUpdate)
