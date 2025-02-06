/**************************************************************************/
/*  xr_nodes.h                                                            */
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

#include "scene/3d/camera_3d.h"
#include "servers/xr/xr_positional_tracker.h"

/*
	XRCamera is a subclass of camera which will register itself with its parent XROrigin and as a result is automatically positioned
*/

class XRCamera3D : public Camera3D {
	GDCLASS(XRCamera3D, Camera3D);

protected:
	// The name and pose for our HMD tracker is currently the only hardcoded bit.
	// If we ever are able to support multiple HMDs we may need to make this settable.
	StringName tracker_name = "head";
	StringName pose_name = SceneStringName(default_);
	Ref<XRPositionalTracker> tracker;

	void _bind_tracker();
	void _unbind_tracker();
	void _changed_tracker(const StringName &p_tracker_name, int p_tracker_type);
	void _removed_tracker(const StringName &p_tracker_name, int p_tracker_type);
	void _pose_changed(const Ref<XRPose> &p_pose);

public:
	PackedStringArray get_configuration_warnings() const override;

	virtual Vector3 project_local_ray_normal(const Point2 &p_pos) const override;
	virtual Point2 unproject_position(const Vector3 &p_pos) const override;
	virtual Vector3 project_position(const Point2 &p_point, real_t p_z_depth) const override;
	virtual Vector<Plane> get_frustum() const override;

	XRCamera3D();
	~XRCamera3D();
};

/*
	XRNode3D is a helper node that implements binding to a tracker.

	It must be a child node of our XROrigin node
*/

class XRNode3D : public Node3D {
	GDCLASS(XRNode3D, Node3D);

private:
	StringName tracker_name;
	StringName pose_name = SceneStringName(default_);
	bool has_tracking_data = false;
	bool show_when_tracked = false;

protected:
	Ref<XRPositionalTracker> tracker;

	static void _bind_methods();

	virtual void _bind_tracker();
	virtual void _unbind_tracker();
	void _changed_tracker(const StringName &p_tracker_name, int p_tracker_type);
	void _removed_tracker(const StringName &p_tracker_name, int p_tracker_type);

	void _pose_changed(const Ref<XRPose> &p_pose);
	void _pose_lost_tracking(const Ref<XRPose> &p_pose);
	void _set_has_tracking_data(bool p_has_tracking_data);

	void _update_visibility();

public:
	void _validate_property(PropertyInfo &p_property) const;
	void set_tracker(const StringName &p_tracker_name);
	StringName get_tracker() const;

	void set_pose_name(const StringName &p_pose);
	StringName get_pose_name() const;

	bool get_is_active() const;
	bool get_has_tracking_data() const;

	void set_show_when_tracked(bool p_show);
	bool get_show_when_tracked() const;

	void trigger_haptic_pulse(const String &p_action_name, double p_frequency, double p_amplitude, double p_duration_sec, double p_delay_sec = 0);

	Ref<XRPose> get_pose();

	PackedStringArray get_configuration_warnings() const override;

	XRNode3D();
	~XRNode3D();
};

/*
	XRController3D is a helper node that automatically updates its position based on tracker data.

	It must be a child node of our XROrigin node
*/

class XRController3D : public XRNode3D {
	GDCLASS(XRController3D, XRNode3D);

private:
protected:
	static void _bind_methods();

	virtual void _bind_tracker() override;
	virtual void _unbind_tracker() override;

	void _button_pressed(const String &p_name);
	void _button_released(const String &p_name);
	void _input_float_changed(const String &p_name, float p_value);
	void _input_vector2_changed(const String &p_name, Vector2 p_value);
	void _profile_changed(const String &p_role);

public:
	bool is_button_pressed(const StringName &p_name) const;
	Variant get_input(const StringName &p_name) const;
	float get_float(const StringName &p_name) const;
	Vector2 get_vector2(const StringName &p_name) const;

	XRPositionalTracker::TrackerHand get_tracker_hand() const;

	XRController3D() {}
	~XRController3D() {}
};

/*
	XRAnchor3D is a helper node that automatically updates its position based on anchor data, it represents a real world location.
	It must be a child node of our XROrigin3D node
*/

class XRAnchor3D : public XRNode3D {
	GDCLASS(XRAnchor3D, XRNode3D);

private:
	Vector3 size;

protected:
	static void _bind_methods();

public:
	Vector3 get_size() const;
	Plane get_plane() const;

	XRAnchor3D() {}
	~XRAnchor3D() {}
};

/*
	XROrigin3D is special spatial node that acts as our origin point mapping our real world center of our tracking volume into our virtual world.

	It is this point that you will move around the world as the player 'moves while standing still', i.e. the player moves through teleporting or controller inputs as opposed to physically moving.

	Our camera and controllers will always be child nodes and thus place relative to this origin point.
	This node will automatically locate any camera child nodes and update its position while our XRController3D node will handle tracked controllers.
*/

class XROrigin3D : public Node3D {
	GDCLASS(XROrigin3D, Node3D);

private:
	bool current = false;
	static Vector<XROrigin3D *> origin_nodes; // all origin nodes in tree

	void _set_current(bool p_enabled, bool p_update_others);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	PackedStringArray get_configuration_warnings() const override;

	real_t get_world_scale() const;
	void set_world_scale(real_t p_world_scale);

	void set_current(bool p_enabled);
	bool is_current() const;

	XROrigin3D() {}
	~XROrigin3D() {}
};
