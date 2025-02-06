/**************************************************************************/
/*  xr_hand_modifier_3d.h                                                 */
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

#include "scene/3d/skeleton_modifier_3d.h"
#include "servers/xr/xr_hand_tracker.h"

/**
	The XRHandModifier3D node drives a hand skeleton using hand tracking
	data from an XRHandTracking instance.
 */

class XRHandModifier3D : public SkeletonModifier3D {
	GDCLASS(XRHandModifier3D, SkeletonModifier3D);

public:
	enum BoneUpdate {
		BONE_UPDATE_FULL,
		BONE_UPDATE_ROTATION_ONLY,
		BONE_UPDATE_MAX
	};

	void set_hand_tracker(const StringName &p_tracker_name);
	StringName get_hand_tracker() const;

	void set_bone_update(BoneUpdate p_bone_update);
	BoneUpdate get_bone_update() const;

	PackedStringArray get_configuration_warnings() const override;

	void _notification(int p_what);

protected:
	static void _bind_methods();

	virtual void _skeleton_changed(Skeleton3D *p_old, Skeleton3D *p_new) override;
	virtual void _process_modification() override;

private:
	struct JointData {
		int bone = -1;
		int parent_joint = -1;
	};

	StringName tracker_name = "/user/hand_tracker/left";
	BoneUpdate bone_update = BONE_UPDATE_FULL;
	JointData joints[XRHandTracker::HAND_JOINT_MAX];

	bool has_stored_previous_transforms = false;
	Vector<Transform3D> previous_relative_transforms;

	void _get_joint_data();
	void _tracker_changed(StringName p_tracker_name, XRServer::TrackerType p_tracker_type);
};

VARIANT_ENUM_CAST(XRHandModifier3D::BoneUpdate)
