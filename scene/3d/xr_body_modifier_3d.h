/**************************************************************************/
/*  xr_body_modifier_3d.h                                                 */
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
#include "servers/xr/xr_body_tracker.h"

class Skeleton3D;

/**
	The XRBodyModifier3D node drives a body skeleton using body tracking
	data from an XRBodyTracker instance.
 */

class XRBodyModifier3D : public SkeletonModifier3D {
	GDCLASS(XRBodyModifier3D, SkeletonModifier3D);

public:
	enum BodyUpdate {
		BODY_UPDATE_UPPER_BODY = 1,
		BODY_UPDATE_LOWER_BODY = 2,
		BODY_UPDATE_HANDS = 4,
	};

	enum BoneUpdate {
		BONE_UPDATE_FULL,
		BONE_UPDATE_ROTATION_ONLY,
		BONE_UPDATE_MAX
	};

	void set_body_tracker(const StringName &p_tracker_name);
	StringName get_body_tracker() const;

	void set_body_update(BitField<BodyUpdate> p_body_update);
	BitField<BodyUpdate> get_body_update() const;

	void set_bone_update(BoneUpdate p_bone_update);
	BoneUpdate get_bone_update() const;

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

	StringName tracker_name = "/user/body_tracker";
	BitField<BodyUpdate> body_update = BODY_UPDATE_UPPER_BODY | BODY_UPDATE_LOWER_BODY | BODY_UPDATE_HANDS;
	BoneUpdate bone_update = BONE_UPDATE_FULL;
	JointData joints[XRBodyTracker::JOINT_MAX];

	void _get_joint_data();
	void _tracker_changed(const StringName &p_tracker_name, XRServer::TrackerType p_tracker_type);
};

VARIANT_BITFIELD_CAST(XRBodyModifier3D::BodyUpdate)
VARIANT_ENUM_CAST(XRBodyModifier3D::BoneUpdate)
