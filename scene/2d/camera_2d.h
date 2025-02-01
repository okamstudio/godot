/**************************************************************************/
/*  camera_2d.h                                                           */
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

#include "scene/2d/node_2d.h"

class Camera2D : public Node2D {
	GDCLASS(Camera2D, Node2D);

public:
	enum AnchorMode {
		ANCHOR_MODE_FIXED_TOP_LEFT,
		ANCHOR_MODE_DRAG_CENTER
	};

	enum Camera2DProcessCallback {
		CAMERA2D_PROCESS_PHYSICS,
		CAMERA2D_PROCESS_IDLE
	};

protected:
	Point2 camera_pos;
	Point2 smoothed_camera_pos;
	bool first = true;
	bool just_exited_tree = false;

	ObjectID custom_viewport_id; // to check validity
	Viewport *custom_viewport = nullptr;
	Viewport *viewport = nullptr;

	StringName group_name;
	StringName canvas_group_name;
	RID canvas;
	Vector2 offset;
	Vector2 zoom = Vector2(1, 1);
	Vector2 zoom_scale = Vector2(1, 1);
	AnchorMode anchor_mode = ANCHOR_MODE_DRAG_CENTER;
	bool ignore_rotation = true;
	bool enabled = true;
	real_t position_smoothing_speed = 5.0;
	bool position_smoothing_enabled = false;

	real_t camera_angle = 0.0;
	real_t rotation_smoothing_speed = 5.0;
	bool rotation_smoothing_enabled = false;

	int limit[4];
	bool limit_smoothing_enabled = false;

	real_t drag_margin[4];
	bool drag_horizontal_enabled = false;
	bool drag_vertical_enabled = false;
	real_t drag_horizontal_offset = 0.0;
	real_t drag_vertical_offset = 0.0;
	bool drag_horizontal_offset_changed = false;
	bool drag_vertical_offset_changed = false;

	Point2 camera_screen_center;
	bool _is_editing_in_editor() const;
	void _update_process_callback();
	void _update_scroll();
#ifdef TOOLS_ENABLED
	void _project_settings_changed();
#endif

	void _make_current(Object *p_which);
	void _reset_just_exited() { just_exited_tree = false; }

	void _set_old_smoothing(real_t p_enable);

	void _update_process_internal_for_smoothing();

	bool screen_drawing_enabled = true;
	bool limit_drawing_enabled = false;
	bool margin_drawing_enabled = false;

	Camera2DProcessCallback process_callback = CAMERA2D_PROCESS_IDLE;

	struct InterpolationData {
		Transform2D xform_curr;
		Transform2D xform_prev;
		uint32_t last_update_physics_tick = UINT32_MAX; // Ensure tick 0 is detected as a change.
	} _interpolation_data;

	void _ensure_update_interpolation_data();

	Size2 _get_camera_screen_size() const;

protected:
	virtual Transform2D get_camera_transform();

	void _notification(int p_what);
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_offset(const Vector2 &p_offset);
	Vector2 get_offset() const;

	void set_anchor_mode(AnchorMode p_anchor_mode);
	AnchorMode get_anchor_mode() const;

	void set_ignore_rotation(bool p_ignore);
	bool is_ignoring_rotation() const;

	void set_limit(Side p_side, int p_limit);
	int get_limit(Side p_side) const;

	void set_limit_smoothing_enabled(bool enable);
	bool is_limit_smoothing_enabled() const;

	void set_drag_horizontal_enabled(bool p_enabled);
	bool is_drag_horizontal_enabled() const;

	void set_drag_vertical_enabled(bool p_enabled);
	bool is_drag_vertical_enabled() const;

	void set_drag_margin(Side p_side, real_t p_drag_margin);
	real_t get_drag_margin(Side p_side) const;

	void set_drag_horizontal_offset(real_t p_offset);
	real_t get_drag_horizontal_offset() const;

	void set_drag_vertical_offset(real_t p_offset);
	real_t get_drag_vertical_offset() const;

	void set_position_smoothing_enabled(bool p_enabled);
	bool is_position_smoothing_enabled() const;

	void set_position_smoothing_speed(real_t p_speed);
	real_t get_position_smoothing_speed() const;

	void set_rotation_smoothing_speed(real_t p_speed);
	real_t get_rotation_smoothing_speed() const;

	void set_rotation_smoothing_enabled(bool p_enabled);
	bool is_rotation_smoothing_enabled() const;

	void set_process_callback(Camera2DProcessCallback p_mode);
	Camera2DProcessCallback get_process_callback() const;

	void set_enabled(bool p_enabled);
	bool is_enabled() const;

	void make_current();
	void clear_current();
	bool is_current() const;

	void set_zoom(const Vector2 &p_zoom);
	Vector2 get_zoom() const;

	Point2 get_camera_screen_center() const;

	void set_custom_viewport(Node *p_viewport);
	Node *get_custom_viewport() const;

	Vector2 get_camera_position() const;
	void force_update_scroll();
	void reset_smoothing();
	void align();

	void set_screen_drawing_enabled(bool enable);
	bool is_screen_drawing_enabled() const;

	void set_limit_drawing_enabled(bool enable);
	bool is_limit_drawing_enabled() const;

	void set_margin_drawing_enabled(bool enable);
	bool is_margin_drawing_enabled() const;

	Camera2D();
};

VARIANT_ENUM_CAST(Camera2D::AnchorMode);
VARIANT_ENUM_CAST(Camera2D::Camera2DProcessCallback);
