/**************************************************************************/
/*  camera_2d.cpp                                                         */
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

#include "camera_2d.h"

#include "core/engine.h"
#include "core/math/math_funcs.h"
#include "scene/scene_string_names.h"
#include "servers/visual_server.h"

void Camera2D::_update_scroll() {
	if (!is_inside_tree()) {
		return;
	}

	if (Engine::get_singleton()->is_editor_hint()) {
		update();
		// Only set viewport transform when not bound to the main viewport.
		if (get_viewport() == get_tree()->get_edited_scene_root()->get_viewport()) {
			return;
		}
	}

	if (!viewport) {
		return;
	}

	if (current) {
		ERR_FAIL_COND(custom_viewport && !ObjectDB::get_instance(custom_viewport_id));

		Transform2D xform;
		if (is_physics_interpolated_and_enabled()) {
			xform = _interpolation_data.xform_prev.interpolate_with(_interpolation_data.xform_curr, Engine::get_singleton()->get_physics_interpolation_fraction());
		} else {
			xform = get_camera_transform();
		}
		viewport->set_canvas_transform(xform);

		Size2 screen_size = viewport->get_visible_rect().size;
		Point2 screen_offset = (anchor_mode == ANCHOR_MODE_DRAG_CENTER ? (screen_size * 0.5) : Point2());

		get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, group_name, "_camera_moved", xform, screen_offset);
	};
}

void Camera2D::_update_process_mode() {
	if (is_physics_interpolated_and_enabled()) {
		set_process_internal(is_current());
		set_physics_process_internal(is_current());

#ifdef TOOLS_ENABLED
		if (process_mode == CAMERA2D_PROCESS_IDLE) {
			WARN_PRINT_ONCE("Camera2D overridden to physics process mode due to use of physics interpolation.");
		}
#endif
	} else {
		// Smoothing can be enabled in the editor but will never be active.
		if (process_mode == CAMERA2D_PROCESS_IDLE) {
			set_process_internal(smoothing_active);
			set_physics_process_internal(false);
		} else {
			set_process_internal(false);
			set_physics_process_internal(smoothing_active);
		}
	}
}

void Camera2D::_setup_viewport() {
	// Disconnect signal on previous viewport if there's one.
	if (viewport && viewport->is_connected("size_changed", this, "_update_scroll")) {
		viewport->disconnect("size_changed", this, "_update_scroll");
	}

	if (custom_viewport && ObjectDB::get_instance(custom_viewport_id)) {
		viewport = custom_viewport;
	} else {
		viewport = get_viewport();
	}

	RID vp = viewport->get_viewport_rid();
	group_name = "__cameras_" + itos(vp.get_id());
	canvas_group_name = "__cameras_c" + itos(canvas.get_id());
	add_to_group(group_name);
	add_to_group(canvas_group_name);

	viewport->connect("size_changed", this, "_update_scroll");
}

void Camera2D::set_zoom(const Vector2 &p_zoom) {
	// Setting zoom to zero causes 'affine_invert' issues
	ERR_FAIL_COND_MSG(Math::is_zero_approx(p_zoom.x) || Math::is_zero_approx(p_zoom.y), "Zoom level must be different from 0 (can be negative).");

	zoom = p_zoom;
	Point2 old_smoothed_camera_pos = smoothed_camera_pos;
	_update_scroll();
	smoothed_camera_pos = old_smoothed_camera_pos;
};

Vector2 Camera2D::get_zoom() const {
	return zoom;
};

Transform2D Camera2D::get_camera_transform() {
	if (!get_tree() || !viewport) {
		return Transform2D();
	}

	ERR_FAIL_COND_V(custom_viewport && !ObjectDB::get_instance(custom_viewport_id), Transform2D());

	Size2 screen_size = viewport->get_visible_rect().size;

	Point2 new_camera_pos = get_global_transform().get_origin();
	Point2 ret_camera_pos;

	if (!first) {
		if (anchor_mode == ANCHOR_MODE_DRAG_CENTER) {
			if (h_drag_enabled && !Engine::get_singleton()->is_editor_hint() && !h_offset_changed) {
				camera_pos.x = MIN(camera_pos.x, (new_camera_pos.x + screen_size.x * 0.5 * zoom.x * drag_margin[MARGIN_LEFT]));
				camera_pos.x = MAX(camera_pos.x, (new_camera_pos.x - screen_size.x * 0.5 * zoom.x * drag_margin[MARGIN_RIGHT]));
			} else {
				if (h_ofs < 0) {
					camera_pos.x = new_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_RIGHT] * h_ofs;
				} else {
					camera_pos.x = new_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_LEFT] * h_ofs;
				}

				h_offset_changed = false;
			}

			if (v_drag_enabled && !Engine::get_singleton()->is_editor_hint() && !v_offset_changed) {
				camera_pos.y = MIN(camera_pos.y, (new_camera_pos.y + screen_size.y * 0.5 * zoom.y * drag_margin[MARGIN_TOP]));
				camera_pos.y = MAX(camera_pos.y, (new_camera_pos.y - screen_size.y * 0.5 * zoom.y * drag_margin[MARGIN_BOTTOM]));

			} else {
				if (v_ofs < 0) {
					camera_pos.y = new_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_BOTTOM] * v_ofs;
				} else {
					camera_pos.y = new_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_TOP] * v_ofs;
				}

				v_offset_changed = false;
			}

		} else if (anchor_mode == ANCHOR_MODE_FIXED_TOP_LEFT) {
			camera_pos = new_camera_pos;
		}

		Point2 screen_offset = (anchor_mode == ANCHOR_MODE_DRAG_CENTER ? (screen_size * 0.5 * zoom) : Point2());
		Rect2 screen_rect(-screen_offset + camera_pos, screen_size * zoom);

		if (limit_smoothing_enabled) {
			if (screen_rect.position.x < limit[MARGIN_LEFT]) {
				camera_pos.x -= screen_rect.position.x - limit[MARGIN_LEFT];
			}

			if (screen_rect.position.x + screen_rect.size.x > limit[MARGIN_RIGHT]) {
				camera_pos.x -= screen_rect.position.x + screen_rect.size.x - limit[MARGIN_RIGHT];
			}

			if (screen_rect.position.y + screen_rect.size.y > limit[MARGIN_BOTTOM]) {
				camera_pos.y -= screen_rect.position.y + screen_rect.size.y - limit[MARGIN_BOTTOM];
			}

			if (screen_rect.position.y < limit[MARGIN_TOP]) {
				camera_pos.y -= screen_rect.position.y - limit[MARGIN_TOP];
			}
		}

		// TODO ..
		// There is a bug here.
		// Smoothing occurs rather confusingly
		// during the call to get_camera_transform().
		// It may be called MULTIPLE TIMES on certain frames,
		// therefore smoothing is not currently applied only once per frame / tick,
		// which will result in some haphazard results.
		if (smoothing_active) {
			// Note that if we are using physics interpolation,
			// processing will always be physics based (it ignores the process mode set in the UI).
			bool physics_process = (process_mode == CAMERA2D_PROCESS_PHYSICS) || is_physics_interpolated_and_enabled();
			float delta = physics_process ? get_physics_process_delta_time() : get_process_delta_time();
			float c = smoothing * delta;
			smoothed_camera_pos = ((camera_pos - smoothed_camera_pos) * c) + smoothed_camera_pos;
			ret_camera_pos = smoothed_camera_pos;
		} else {
			ret_camera_pos = smoothed_camera_pos = camera_pos;
		}

	} else {
		ret_camera_pos = smoothed_camera_pos = camera_pos = new_camera_pos;
		first = false;
	}

	Point2 screen_offset = (anchor_mode == ANCHOR_MODE_DRAG_CENTER ? (screen_size * 0.5 * zoom) : Point2());

	float angle = get_global_transform().get_rotation();
	if (rotating) {
		screen_offset = screen_offset.rotated(angle);
	}

	Rect2 screen_rect(-screen_offset + ret_camera_pos, screen_size * zoom);

	if (!smoothing_enabled || !limit_smoothing_enabled) {
		if (screen_rect.position.x < limit[MARGIN_LEFT]) {
			screen_rect.position.x = limit[MARGIN_LEFT];
		}

		if (screen_rect.position.x + screen_rect.size.x > limit[MARGIN_RIGHT]) {
			screen_rect.position.x = limit[MARGIN_RIGHT] - screen_rect.size.x;
		}

		if (screen_rect.position.y + screen_rect.size.y > limit[MARGIN_BOTTOM]) {
			screen_rect.position.y = limit[MARGIN_BOTTOM] - screen_rect.size.y;
		}

		if (screen_rect.position.y < limit[MARGIN_TOP]) {
			screen_rect.position.y = limit[MARGIN_TOP];
		}
	}

	if (offset != Vector2()) {
		screen_rect.position += offset;
	}

	camera_screen_center = screen_rect.position + screen_rect.size * 0.5;

	Transform2D xform;
	xform.scale_basis(zoom);
	if (rotating) {
		xform.set_rotation(angle);
	}
	xform.set_origin(screen_rect.position);

	return (xform).affine_inverse();
}

void Camera2D::_ensure_update_interpolation_data() {
	// The curr -> previous update can either occur
	// on the INTERNAL_PHYSICS_PROCESS OR
	// on NOTIFICATION_TRANSFORM_CHANGED,
	// if NOTIFICATION_TRANSFORM_CHANGED takes place
	// earlier than INTERNAL_PHYSICS_PROCESS on a tick.
	// This is to ensure that the data keeps flowing, but the new data
	// doesn't overwrite before prev has been set.

	// Keep the data flowing.
	uint64_t tick = Engine::get_singleton()->get_physics_frames();
	if (_interpolation_data.last_update_physics_tick != tick) {
		_interpolation_data.xform_prev = _interpolation_data.xform_curr;
		_interpolation_data.last_update_physics_tick = tick;
	}
}

void Camera2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS: {
			_update_scroll();
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (is_physics_interpolated_and_enabled()) {
				_ensure_update_interpolation_data();
				_interpolation_data.xform_curr = get_camera_transform();
			} else {
				_update_scroll();
			}
		} break;
		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			// Force the limits etc to update.
			_interpolation_data.xform_curr = get_camera_transform();
			_interpolation_data.xform_prev = _interpolation_data.xform_curr;
		} break;
		case NOTIFICATION_PAUSED: {
			if (is_physics_interpolated_and_enabled()) {
				_update_scroll();
			}
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (!smoothing_active && !is_physics_interpolated_and_enabled()) {
				_update_scroll();
			}
			if (is_physics_interpolated_and_enabled()) {
				_ensure_update_interpolation_data();
				if (Engine::get_singleton()->is_in_physics_frame()) {
					_interpolation_data.xform_curr = get_camera_transform();
				} else {
#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
					PHYSICS_INTERPOLATION_NODE_WARNING(get_instance_id(), "Interpolated Camera2D transformed from outside physics process");
#endif
				}
			}

		} break;
		case NOTIFICATION_ENTER_TREE: {
			ERR_FAIL_COND(!is_inside_tree());
			canvas = get_canvas();

			_setup_viewport();
			_update_process_mode();

			// if a camera enters the tree that is set to current,
			// it should take over as the current camera, and mark
			// all other cameras as non current
			first = true;
			_set_current(current);

			// Note that NOTIFICATION_RESET_PHYSICS_INTERPOLATION
			// is automatically called before this because Camera2D is inherited
			// from CanvasItem. However, the camera transform is not up to date
			// until this point, so we do an extra manual reset.
			if (is_physics_interpolated_and_enabled()) {
				_interpolation_data.xform_curr = get_camera_transform();
				_interpolation_data.xform_prev = _interpolation_data.xform_curr;
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			const bool viewport_valid = !custom_viewport || ObjectDB::get_instance(custom_viewport_id);
			if (is_current()) {
				if (viewport && viewport_valid) {
					viewport->set_canvas_transform(Transform2D());
				}
			}
			if (viewport && viewport_valid) {
				viewport->disconnect("size_changed", this, "_update_scroll");
			}
			remove_from_group(group_name);
			remove_from_group(canvas_group_name);
			viewport = nullptr;

		} break;
#ifdef TOOLS_ENABLED
		case NOTIFICATION_DRAW: {
			if (!is_inside_tree() || !Engine::get_singleton()->is_editor_hint()) {
				break;
			}

			if (screen_drawing_enabled) {
				Color area_axis_color(1, 0.4, 1, 0.63);
				float area_axis_width = 1;
				if (is_current()) {
					area_axis_width = 3;
				}

				Transform2D inv_camera_transform = get_camera_transform().affine_inverse();
				Size2 screen_size = get_viewport_rect().size;

				Vector2 screen_endpoints[4] = {
					inv_camera_transform.xform(Vector2(0, 0)),
					inv_camera_transform.xform(Vector2(screen_size.width, 0)),
					inv_camera_transform.xform(Vector2(screen_size.width, screen_size.height)),
					inv_camera_transform.xform(Vector2(0, screen_size.height))
				};

				Transform2D inv_transform = get_global_transform().affine_inverse(); // undo global space

				for (int i = 0; i < 4; i++) {
					draw_line(inv_transform.xform(screen_endpoints[i]), inv_transform.xform(screen_endpoints[(i + 1) % 4]), area_axis_color, area_axis_width);
				}
			}

			if (limit_drawing_enabled) {
				Color limit_drawing_color(1, 1, 0.25, 0.63);
				float limit_drawing_width = 1;
				if (is_current()) {
					limit_drawing_width = 3;
				}

				Vector2 camera_origin = get_global_transform().get_origin();
				Vector2 camera_scale = get_global_transform().get_scale().abs();
				Vector2 limit_points[4] = {
					(Vector2(limit[MARGIN_LEFT], limit[MARGIN_TOP]) - camera_origin) / camera_scale,
					(Vector2(limit[MARGIN_RIGHT], limit[MARGIN_TOP]) - camera_origin) / camera_scale,
					(Vector2(limit[MARGIN_RIGHT], limit[MARGIN_BOTTOM]) - camera_origin) / camera_scale,
					(Vector2(limit[MARGIN_LEFT], limit[MARGIN_BOTTOM]) - camera_origin) / camera_scale
				};

				for (int i = 0; i < 4; i++) {
					draw_line(limit_points[i], limit_points[(i + 1) % 4], limit_drawing_color, limit_drawing_width);
				}
			}

			if (margin_drawing_enabled) {
				Color margin_drawing_color(0.25, 1, 1, 0.63);
				float margin_drawing_width = 1;
				if (is_current()) {
					margin_drawing_width = 3;
				}

				Transform2D inv_camera_transform = get_camera_transform().affine_inverse();
				Size2 screen_size = get_viewport_rect().size;

				Vector2 margin_endpoints[4] = {
					inv_camera_transform.xform(Vector2((screen_size.width / 2) - ((screen_size.width / 2) * drag_margin[MARGIN_LEFT]), (screen_size.height / 2) - ((screen_size.height / 2) * drag_margin[MARGIN_TOP]))),
					inv_camera_transform.xform(Vector2((screen_size.width / 2) + ((screen_size.width / 2) * drag_margin[MARGIN_RIGHT]), (screen_size.height / 2) - ((screen_size.height / 2) * drag_margin[MARGIN_TOP]))),
					inv_camera_transform.xform(Vector2((screen_size.width / 2) + ((screen_size.width / 2) * drag_margin[MARGIN_RIGHT]), (screen_size.height / 2) + ((screen_size.height / 2) * drag_margin[MARGIN_BOTTOM]))),
					inv_camera_transform.xform(Vector2((screen_size.width / 2) - ((screen_size.width / 2) * drag_margin[MARGIN_LEFT]), (screen_size.height / 2) + ((screen_size.height / 2) * drag_margin[MARGIN_BOTTOM])))
				};

				Transform2D inv_transform = get_global_transform().affine_inverse(); // undo global space

				for (int i = 0; i < 4; i++) {
					draw_line(inv_transform.xform(margin_endpoints[i]), inv_transform.xform(margin_endpoints[(i + 1) % 4]), margin_drawing_color, margin_drawing_width);
				}
			}
		} break;
#endif
	}
}

void Camera2D::set_offset(const Vector2 &p_offset) {
	offset = p_offset;
	Point2 old_smoothed_camera_pos = smoothed_camera_pos;
	_update_scroll();
	smoothed_camera_pos = old_smoothed_camera_pos;
}

Vector2 Camera2D::get_offset() const {
	return offset;
}

void Camera2D::set_anchor_mode(AnchorMode p_anchor_mode) {
	anchor_mode = p_anchor_mode;
	_update_scroll();
}

Camera2D::AnchorMode Camera2D::get_anchor_mode() const {
	return anchor_mode;
}

void Camera2D::set_rotating(bool p_rotating) {
	rotating = p_rotating;
	Point2 old_smoothed_camera_pos = smoothed_camera_pos;
	_update_scroll();
	smoothed_camera_pos = old_smoothed_camera_pos;
}

bool Camera2D::is_rotating() const {
	return rotating;
}

void Camera2D::set_process_mode(Camera2DProcessMode p_mode) {
	if (process_mode == p_mode) {
		return;
	}

	process_mode = p_mode;
	_update_process_mode();
}

Camera2D::Camera2DProcessMode Camera2D::get_process_mode() const {
	return process_mode;
}

void Camera2D::_make_current(Object *p_which) {
	bool new_current = false;

	if (p_which == this) {
		new_current = true;
	}

	if (new_current != current) {
		current = new_current;
		_update_process_mode();
	}
}

void Camera2D::_set_current(bool p_current) {
	if (p_current) {
		make_current();
	}

	current = p_current;
	_update_process_mode();
	update();
}

bool Camera2D::is_current() const {
	return current;
}

void Camera2D::make_current() {
	if (!is_inside_tree()) {
		current = true;
	} else {
		get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, group_name, "_make_current", this);
	}
	_update_scroll();
	_update_process_mode();
}

void Camera2D::clear_current() {
	current = false;
	if (is_inside_tree()) {
		get_tree()->call_group_flags(SceneTree::GROUP_CALL_REALTIME, group_name, "_make_current", (Object *)nullptr);
	}
	_update_process_mode();
}

void Camera2D::set_limit(Margin p_margin, int p_limit) {
	ERR_FAIL_INDEX((int)p_margin, 4);
	limit[p_margin] = p_limit;
	Point2 old_smoothed_camera_pos = smoothed_camera_pos;
	_update_scroll();
	smoothed_camera_pos = old_smoothed_camera_pos;
}

int Camera2D::get_limit(Margin p_margin) const {
	ERR_FAIL_INDEX_V((int)p_margin, 4, 0);
	return limit[p_margin];
}

void Camera2D::set_limit_smoothing_enabled(bool enable) {
	limit_smoothing_enabled = enable;
	_update_scroll();
}

bool Camera2D::is_limit_smoothing_enabled() const {
	return limit_smoothing_enabled;
}

void Camera2D::set_drag_margin(Margin p_margin, float p_drag_margin) {
	ERR_FAIL_INDEX((int)p_margin, 4);
	drag_margin[p_margin] = p_drag_margin;
	update();
}

float Camera2D::get_drag_margin(Margin p_margin) const {
	ERR_FAIL_INDEX_V((int)p_margin, 4, 0);
	return drag_margin[p_margin];
}

Vector2 Camera2D::get_camera_position() const {
	return camera_pos;
}

void Camera2D::force_update_scroll() {
	_update_scroll();
}

void Camera2D::reset_smoothing() {
	_update_scroll();
	smoothed_camera_pos = camera_pos;
}

void Camera2D::align() {
	ERR_FAIL_COND(!is_inside_tree() || !viewport);
	ERR_FAIL_COND(custom_viewport && !ObjectDB::get_instance(custom_viewport_id));

	Size2 screen_size = viewport->get_visible_rect().size;

	Point2 current_camera_pos = get_global_transform().get_origin();
	if (anchor_mode == ANCHOR_MODE_DRAG_CENTER) {
		if (h_ofs < 0) {
			camera_pos.x = current_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_RIGHT] * h_ofs;
		} else {
			camera_pos.x = current_camera_pos.x + screen_size.x * 0.5 * drag_margin[MARGIN_LEFT] * h_ofs;
		}
		if (v_ofs < 0) {
			camera_pos.y = current_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_TOP] * v_ofs;
		} else {
			camera_pos.y = current_camera_pos.y + screen_size.y * 0.5 * drag_margin[MARGIN_BOTTOM] * v_ofs;
		}
	} else if (anchor_mode == ANCHOR_MODE_FIXED_TOP_LEFT) {
		camera_pos = current_camera_pos;
	}

	_update_scroll();
}

void Camera2D::set_follow_smoothing(float p_speed) {
	smoothing = p_speed;
}

float Camera2D::get_follow_smoothing() const {
	return smoothing;
}

Point2 Camera2D::get_camera_screen_center() const {
	return camera_screen_center;
}

void Camera2D::set_h_drag_enabled(bool p_enabled) {
	h_drag_enabled = p_enabled;
}

bool Camera2D::is_h_drag_enabled() const {
	return h_drag_enabled;
}

void Camera2D::set_v_drag_enabled(bool p_enabled) {
	v_drag_enabled = p_enabled;
}

bool Camera2D::is_v_drag_enabled() const {
	return v_drag_enabled;
}

void Camera2D::set_v_offset(float p_offset) {
	v_ofs = p_offset;
	v_offset_changed = true;
	Point2 old_smoothed_camera_pos = smoothed_camera_pos;
	_update_scroll();
	smoothed_camera_pos = old_smoothed_camera_pos;
}

float Camera2D::get_v_offset() const {
	return v_ofs;
}

void Camera2D::set_h_offset(float p_offset) {
	h_ofs = p_offset;
	h_offset_changed = true;
	Point2 old_smoothed_camera_pos = smoothed_camera_pos;
	_update_scroll();
	smoothed_camera_pos = old_smoothed_camera_pos;
}
float Camera2D::get_h_offset() const {
	return h_ofs;
}

void Camera2D::set_enable_follow_smoothing(bool p_enabled) {
	// watch for situation where an pre-enabled camera is added to the tree
	// processing must be resumed and bypass this noop check
	// (this currently works but is a possible future bug)
	if (smoothing_enabled == p_enabled) {
		return;
	}

	// Separate the logic between enabled and active, because the smoothing
	// cannot be active in the editor. This can be done without a separate flag
	// but is bug prone so this approach is easier to follow.
	smoothing_enabled = p_enabled;
	smoothing_active = smoothing_enabled && !Engine::get_singleton()->is_editor_hint();

	// keep the processing up to date after each change
	_update_process_mode();
}

bool Camera2D::is_follow_smoothing_enabled() const {
	return smoothing_enabled;
}

void Camera2D::set_custom_viewport(Node *p_viewport) {
	ERR_FAIL_NULL(p_viewport);
	if (is_inside_tree()) {
		remove_from_group(group_name);
		remove_from_group(canvas_group_name);
	}

	if (custom_viewport && !ObjectDB::get_instance(custom_viewport_id)) {
		viewport = nullptr;
	}

	custom_viewport = Object::cast_to<Viewport>(p_viewport);

	if (custom_viewport) {
		custom_viewport_id = custom_viewport->get_instance_id();
	} else {
		custom_viewport_id = 0;
	}

	if (is_inside_tree()) {
		_setup_viewport();
	}
}

Node *Camera2D::get_custom_viewport() const {
	return custom_viewport;
}

void Camera2D::set_screen_drawing_enabled(bool enable) {
	screen_drawing_enabled = enable;
#ifdef TOOLS_ENABLED
	update();
#endif
}

bool Camera2D::is_screen_drawing_enabled() const {
	return screen_drawing_enabled;
}

void Camera2D::set_limit_drawing_enabled(bool enable) {
	limit_drawing_enabled = enable;
#ifdef TOOLS_ENABLED
	update();
#endif
}

bool Camera2D::is_limit_drawing_enabled() const {
	return limit_drawing_enabled;
}

void Camera2D::set_margin_drawing_enabled(bool enable) {
	margin_drawing_enabled = enable;
#ifdef TOOLS_ENABLED
	update();
#endif
}

bool Camera2D::is_margin_drawing_enabled() const {
	return margin_drawing_enabled;
}

void Camera2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &Camera2D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &Camera2D::get_offset);

	ClassDB::bind_method(D_METHOD("set_anchor_mode", "anchor_mode"), &Camera2D::set_anchor_mode);
	ClassDB::bind_method(D_METHOD("get_anchor_mode"), &Camera2D::get_anchor_mode);

	ClassDB::bind_method(D_METHOD("set_rotating", "rotating"), &Camera2D::set_rotating);
	ClassDB::bind_method(D_METHOD("is_rotating"), &Camera2D::is_rotating);

	ClassDB::bind_method(D_METHOD("make_current"), &Camera2D::make_current);
	ClassDB::bind_method(D_METHOD("clear_current"), &Camera2D::clear_current);
	ClassDB::bind_method(D_METHOD("_make_current"), &Camera2D::_make_current);

	ClassDB::bind_method(D_METHOD("_update_scroll"), &Camera2D::_update_scroll);

	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &Camera2D::set_process_mode);
	ClassDB::bind_method(D_METHOD("get_process_mode"), &Camera2D::get_process_mode);

	ClassDB::bind_method(D_METHOD("_set_current", "current"), &Camera2D::_set_current);
	ClassDB::bind_method(D_METHOD("is_current"), &Camera2D::is_current);

	ClassDB::bind_method(D_METHOD("set_limit", "margin", "limit"), &Camera2D::set_limit);
	ClassDB::bind_method(D_METHOD("get_limit", "margin"), &Camera2D::get_limit);

	ClassDB::bind_method(D_METHOD("set_limit_smoothing_enabled", "limit_smoothing_enabled"), &Camera2D::set_limit_smoothing_enabled);
	ClassDB::bind_method(D_METHOD("is_limit_smoothing_enabled"), &Camera2D::is_limit_smoothing_enabled);

	ClassDB::bind_method(D_METHOD("set_v_drag_enabled", "enabled"), &Camera2D::set_v_drag_enabled);
	ClassDB::bind_method(D_METHOD("is_v_drag_enabled"), &Camera2D::is_v_drag_enabled);

	ClassDB::bind_method(D_METHOD("set_h_drag_enabled", "enabled"), &Camera2D::set_h_drag_enabled);
	ClassDB::bind_method(D_METHOD("is_h_drag_enabled"), &Camera2D::is_h_drag_enabled);

	ClassDB::bind_method(D_METHOD("set_v_offset", "ofs"), &Camera2D::set_v_offset);
	ClassDB::bind_method(D_METHOD("get_v_offset"), &Camera2D::get_v_offset);

	ClassDB::bind_method(D_METHOD("set_h_offset", "ofs"), &Camera2D::set_h_offset);
	ClassDB::bind_method(D_METHOD("get_h_offset"), &Camera2D::get_h_offset);

	ClassDB::bind_method(D_METHOD("set_drag_margin", "margin", "drag_margin"), &Camera2D::set_drag_margin);
	ClassDB::bind_method(D_METHOD("get_drag_margin", "margin"), &Camera2D::get_drag_margin);

	ClassDB::bind_method(D_METHOD("get_camera_position"), &Camera2D::get_camera_position);
	ClassDB::bind_method(D_METHOD("get_camera_screen_center"), &Camera2D::get_camera_screen_center);

	ClassDB::bind_method(D_METHOD("set_zoom", "zoom"), &Camera2D::set_zoom);
	ClassDB::bind_method(D_METHOD("get_zoom"), &Camera2D::get_zoom);

	ClassDB::bind_method(D_METHOD("set_custom_viewport", "viewport"), &Camera2D::set_custom_viewport);
	ClassDB::bind_method(D_METHOD("get_custom_viewport"), &Camera2D::get_custom_viewport);

	ClassDB::bind_method(D_METHOD("set_follow_smoothing", "follow_smoothing"), &Camera2D::set_follow_smoothing);
	ClassDB::bind_method(D_METHOD("get_follow_smoothing"), &Camera2D::get_follow_smoothing);

	ClassDB::bind_method(D_METHOD("set_enable_follow_smoothing", "follow_smoothing"), &Camera2D::set_enable_follow_smoothing);
	ClassDB::bind_method(D_METHOD("is_follow_smoothing_enabled"), &Camera2D::is_follow_smoothing_enabled);

	ClassDB::bind_method(D_METHOD("force_update_scroll"), &Camera2D::force_update_scroll);
	ClassDB::bind_method(D_METHOD("reset_smoothing"), &Camera2D::reset_smoothing);
	ClassDB::bind_method(D_METHOD("align"), &Camera2D::align);

	ClassDB::bind_method(D_METHOD("set_screen_drawing_enabled", "screen_drawing_enabled"), &Camera2D::set_screen_drawing_enabled);
	ClassDB::bind_method(D_METHOD("is_screen_drawing_enabled"), &Camera2D::is_screen_drawing_enabled);

	ClassDB::bind_method(D_METHOD("set_limit_drawing_enabled", "limit_drawing_enabled"), &Camera2D::set_limit_drawing_enabled);
	ClassDB::bind_method(D_METHOD("is_limit_drawing_enabled"), &Camera2D::is_limit_drawing_enabled);

	ClassDB::bind_method(D_METHOD("set_margin_drawing_enabled", "margin_drawing_enabled"), &Camera2D::set_margin_drawing_enabled);
	ClassDB::bind_method(D_METHOD("is_margin_drawing_enabled"), &Camera2D::is_margin_drawing_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "anchor_mode", PROPERTY_HINT_ENUM, "Fixed TopLeft,Drag Center"), "set_anchor_mode", "get_anchor_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "rotating"), "set_rotating", "is_rotating");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "current"), "_set_current", "is_current");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "zoom", PROPERTY_HINT_LINK), "set_zoom", "get_zoom");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "custom_viewport", PROPERTY_HINT_RESOURCE_TYPE, "Viewport", 0), "set_custom_viewport", "get_custom_viewport");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "process_mode", PROPERTY_HINT_ENUM, "Physics,Idle"), "set_process_mode", "get_process_mode");

	ADD_GROUP("Limit", "limit_");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "limit_left"), "set_limit", "get_limit", MARGIN_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "limit_top"), "set_limit", "get_limit", MARGIN_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "limit_right"), "set_limit", "get_limit", MARGIN_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "limit_bottom"), "set_limit", "get_limit", MARGIN_BOTTOM);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "limit_smoothed"), "set_limit_smoothing_enabled", "is_limit_smoothing_enabled");

	ADD_GROUP("Draw Margin", "draw_margin_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_margin_h_enabled"), "set_h_drag_enabled", "is_h_drag_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_margin_v_enabled"), "set_v_drag_enabled", "is_v_drag_enabled");

	ADD_GROUP("Smoothing", "smoothing_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "smoothing_enabled"), "set_enable_follow_smoothing", "is_follow_smoothing_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "smoothing_speed"), "set_follow_smoothing", "get_follow_smoothing");

	ADD_GROUP("Offset", "offset_");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "offset_h", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_h_offset", "get_h_offset");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "offset_v", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_v_offset", "get_v_offset");

	ADD_GROUP("Drag Margin", "drag_margin_");
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "drag_margin_left", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_drag_margin", "get_drag_margin", MARGIN_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "drag_margin_top", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_drag_margin", "get_drag_margin", MARGIN_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "drag_margin_right", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_drag_margin", "get_drag_margin", MARGIN_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::REAL, "drag_margin_bottom", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_drag_margin", "get_drag_margin", MARGIN_BOTTOM);

	ADD_GROUP("Editor", "editor_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_draw_screen"), "set_screen_drawing_enabled", "is_screen_drawing_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_draw_limits"), "set_limit_drawing_enabled", "is_limit_drawing_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_draw_drag_margin"), "set_margin_drawing_enabled", "is_margin_drawing_enabled");

	BIND_ENUM_CONSTANT(ANCHOR_MODE_FIXED_TOP_LEFT);
	BIND_ENUM_CONSTANT(ANCHOR_MODE_DRAG_CENTER);
	BIND_ENUM_CONSTANT(CAMERA2D_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(CAMERA2D_PROCESS_IDLE);
}

Camera2D::Camera2D() {
	anchor_mode = ANCHOR_MODE_DRAG_CENTER;
	rotating = false;
	current = false;
	limit[MARGIN_LEFT] = -10000000;
	limit[MARGIN_TOP] = -10000000;
	limit[MARGIN_RIGHT] = 10000000;
	limit[MARGIN_BOTTOM] = 10000000;

	drag_margin[MARGIN_LEFT] = 0.2;
	drag_margin[MARGIN_TOP] = 0.2;
	drag_margin[MARGIN_RIGHT] = 0.2;
	drag_margin[MARGIN_BOTTOM] = 0.2;
	camera_pos = Vector2();
	first = true;
	smoothing_enabled = false;
	smoothing_active = false;
	limit_smoothing_enabled = false;

	viewport = nullptr;
	custom_viewport = nullptr;
	custom_viewport_id = 0;
	process_mode = CAMERA2D_PROCESS_IDLE;

	smoothing = 5.0;
	zoom = Vector2(1, 1);

	screen_drawing_enabled = true;
	limit_drawing_enabled = false;
	margin_drawing_enabled = false;

	h_drag_enabled = false;
	v_drag_enabled = false;
	h_ofs = 0;
	v_ofs = 0;
	h_offset_changed = false;
	v_offset_changed = false;

	set_notify_transform(true);
}
