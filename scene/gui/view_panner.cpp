/**************************************************************************/
/*  view_panner.cpp                                                       */
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

#include "view_panner.h"

#include "core/input/input.h"
#include "core/input/shortcut.h"
#include "core/os/keyboard.h"
#include "scene/main/viewport.h"

bool ViewPanner::gui_input(const Ref<InputEvent> &p_event, Rect2 p_canvas_rect) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		Vector2 scroll_vec = Vector2((mb->get_button_index() == MouseButton::WHEEL_RIGHT) - (mb->get_button_index() == MouseButton::WHEEL_LEFT), (mb->get_button_index() == MouseButton::WHEEL_DOWN) - (mb->get_button_index() == MouseButton::WHEEL_UP));
		// Moving the scroll wheel sends two events: one with pressed as true,
		// and one with pressed as false. Make sure we only process one of them.
		if (scroll_vec != Vector2() && mb->is_pressed()) {
			if (control_scheme == SCROLL_PANS) {
				if (mb->is_ctrl_pressed()) {
					if (scroll_vec.y != 0) {
						// Compute the zoom factor.
						float zoom_factor = mb->get_factor() <= 0 ? 1.0 : mb->get_factor();
						zoom_factor = ((scroll_zoom_factor - 1.0) * zoom_factor) + 1.0;
						float zoom = scroll_vec.y > 0 ? 1.0 / scroll_zoom_factor : scroll_zoom_factor;
						zoom_callback.call(zoom, mb->get_position(), p_event);
						return true;
					}
				} else {
					Vector2 panning = scroll_vec * mb->get_factor();
					if (pan_axis == PAN_AXIS_HORIZONTAL) {
						panning = Vector2(panning.x + panning.y, 0);
					} else if (pan_axis == PAN_AXIS_VERTICAL) {
						panning = Vector2(0, panning.x + panning.y);
					} else if (mb->is_shift_pressed()) {
						panning = Vector2(panning.y, panning.x);
					}
					pan_callback.call(-panning * scroll_speed, p_event);
					return true;
				}
			} else {
				if (mb->is_ctrl_pressed()) {
					Vector2 panning = scroll_vec * mb->get_factor();
					if (pan_axis == PAN_AXIS_HORIZONTAL) {
						panning = Vector2(panning.x + panning.y, 0);
					} else if (pan_axis == PAN_AXIS_VERTICAL) {
						panning = Vector2(0, panning.x + panning.y);
					} else if (mb->is_shift_pressed()) {
						panning = Vector2(panning.y, panning.x);
					}
					pan_callback.call(-panning * scroll_speed, p_event);
					return true;
				} else if (!mb->is_shift_pressed() && scroll_vec.y != 0) {
					// Compute the zoom factor.
					float zoom_factor = mb->get_factor() <= 0 ? 1.0 : mb->get_factor();
					zoom_factor = ((scroll_zoom_factor - 1.0) * zoom_factor) + 1.0;
					float zoom = scroll_vec.y > 0 ? 1.0 / scroll_zoom_factor : scroll_zoom_factor;
					zoom_callback.call(zoom, mb->get_position(), p_event);
					return true;
				}
			}
		}

		// Alt is not used for button presses, so ignore it.
		if (mb->is_alt_pressed()) {
			return false;
		}

		bool is_drag_event = mb->get_button_index() == MouseButton::MIDDLE ||
				(enable_rmb && mb->get_button_index() == MouseButton::RIGHT) ||
				(!simple_panning_enabled && mb->get_button_index() == MouseButton::LEFT && is_panning()) ||
				(force_drag && mb->get_button_index() == MouseButton::LEFT);

		if (is_drag_event) {
			if (mb->is_pressed()) {
				is_dragging = true;
			} else {
				is_dragging = false;
			}
			return mb->get_button_index() != MouseButton::LEFT || mb->is_pressed(); // Don't consume LMB release events (it fixes some selection problems).
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (is_dragging) {
			if (warped_panning_viewport && p_canvas_rect.has_area()) {
				pan_callback.call(warped_panning_viewport->wrap_mouse_in_rect(mm->get_relative(), p_canvas_rect), p_event);
			} else {
				pan_callback.call(mm->get_relative(), p_event);
			}
			return true;
		}
	}

	Ref<InputEventMagnifyGesture> magnify_gesture = p_event;
	if (magnify_gesture.is_valid()) {
		// Zoom gesture
		zoom_callback.call(magnify_gesture->get_factor(), magnify_gesture->get_position(), p_event);
		return true;
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		if (pan_gesture->is_ctrl_pressed()) {
			// Zoom gesture.
			float pan_zoom_factor = 1.02f;
			float zoom_direction = pan_gesture->get_delta().x - pan_gesture->get_delta().y;
			if (zoom_direction == 0.f) {
				return true;
			}
			float zoom = zoom_direction < 0 ? 1.0 / pan_zoom_factor : pan_zoom_factor;
			zoom_callback.call(zoom, pan_gesture->get_position(), p_event);
			return true;
		}
		pan_callback.call(-pan_gesture->get_delta() * scroll_speed, p_event);
	}

	Ref<InputEventScreenDrag> screen_drag = p_event;
	if (screen_drag.is_valid()) {
		if (Input::get_singleton()->is_emulating_mouse_from_touch() || Input::get_singleton()->is_emulating_touch_from_mouse()) {
			// This set of events also generates/is generated by
			// InputEventMouseButton/InputEventMouseMotion events which will be processed instead.
		} else {
			pan_callback.call(screen_drag->get_relative(), p_event);
		}
	}

	Ref<InputEventKey> k = p_event;
	if (k.is_valid()) {
		if (pan_view_shortcut.is_valid() && pan_view_shortcut->matches_event(k)) {
			pan_key_pressed = k->is_pressed();
			if (simple_panning_enabled || Input::get_singleton()->get_mouse_button_mask().has_flag(MouseButtonMask::LEFT)) {
				is_dragging = pan_key_pressed;
			}
			return true;
		}
	}

	return false;
}

void ViewPanner::release_pan_key() {
	pan_key_pressed = false;
	is_dragging = false;
}

void ViewPanner::set_callbacks(Callable p_pan_callback, Callable p_zoom_callback) {
	pan_callback = p_pan_callback;
	zoom_callback = p_zoom_callback;
}

void ViewPanner::set_control_scheme(ControlScheme p_scheme) {
	control_scheme = p_scheme;
}

void ViewPanner::set_enable_rmb(bool p_enable) {
	enable_rmb = p_enable;
}

void ViewPanner::set_pan_shortcut(Ref<Shortcut> p_shortcut) {
	pan_view_shortcut = p_shortcut;
	pan_key_pressed = false;
}

void ViewPanner::set_simple_panning_enabled(bool p_enabled) {
	simple_panning_enabled = p_enabled;
}

void ViewPanner::set_scroll_speed(int p_scroll_speed) {
	ERR_FAIL_COND(p_scroll_speed <= 0);
	scroll_speed = p_scroll_speed;
}

void ViewPanner::set_scroll_zoom_factor(float p_scroll_zoom_factor) {
	ERR_FAIL_COND(p_scroll_zoom_factor <= 1.0);
	scroll_zoom_factor = p_scroll_zoom_factor;
}

void ViewPanner::set_pan_axis(PanAxis p_pan_axis) {
	pan_axis = p_pan_axis;
}

void ViewPanner::setup(ControlScheme p_scheme, Ref<Shortcut> p_shortcut, bool p_simple_panning) {
	set_control_scheme(p_scheme);
	set_pan_shortcut(p_shortcut);
	set_simple_panning_enabled(p_simple_panning);
}

void ViewPanner::setup_warped_panning(Viewport *p_viewport, bool p_allowed) {
	warped_panning_viewport = p_allowed ? p_viewport : nullptr;
}

bool ViewPanner::is_panning() const {
	return is_dragging || pan_key_pressed;
}

void ViewPanner::set_force_drag(bool p_force) {
	force_drag = p_force;
}

ViewPanner::ViewPanner() {
	Array inputs;
	inputs.append(InputEventKey::create_reference(Key::SPACE));

	pan_view_shortcut.instantiate();
	pan_view_shortcut->set_events(inputs);
}
