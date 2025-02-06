/**************************************************************************/
/*  joypad_helper.cpp                                                     */
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

#include "joypad_helper.h"

#include "core/input/input.h"
#include "scene/main/node.h"

bool JoypadHelper::_check_initial_action(const Ref<InputEvent> &p_event, const StringName &p_action_name) {
	if (!p_event->is_pressed()) {
		return false;
	}

	if (!p_event->is_action(p_action_name, true)) {
		return false;
	}

	if (!Input::get_singleton()->is_action_just_pressed(p_action_name, true)) {
		return false;
	}
	return true;
}

void JoypadHelper::_set_active(bool p_active) {
	active = p_active;
	if (toggle_process) {
		owner->set_process_internal(active);
	}
	if (!active) {
		gamepad_event_delay_ms = DEFAULT_GAMEPAD_EVENT_DELAY_MS;
	}
}

void JoypadHelper::setup(Node *p_owner, bool p_use_horizontal_axis, bool p_use_vertical_axis) {
	owner = p_owner;
	use_horizontal_axis = p_use_horizontal_axis;
	use_vertical_axis = p_use_vertical_axis;
}

void JoypadHelper::set_move_callback(const Callable &p_callback) {
	move_callback = p_callback;
}

void JoypadHelper::disable_process_toggle() {
	toggle_process = false;
}

bool JoypadHelper::process_event(const Ref<InputEvent> &p_event) {
	if (!Object::cast_to<InputEventJoypadMotion>(p_event.ptr()) && !Object::cast_to<InputEventJoypadButton>(p_event.ptr())) {
		return false;
	}

	if (use_horizontal_axis) {
		if (_check_initial_action(p_event, SNAME("ui_left")) || _check_initial_action(p_event, SNAME("ui_right"))) {
			_set_active(true);
			return false;
		}
	}

	if (use_vertical_axis) {
		if (_check_initial_action(p_event, SNAME("ui_down")) || _check_initial_action(p_event, SNAME("ui_up"))) {
			_set_active(true);
			return false;
		}
	}
	return true;
}

void JoypadHelper::process_internal(double p_delta) {
	if (!active) {
		return;
	}

	Input *input = Input::get_singleton();
	if (use_horizontal_axis) {
		if (input->is_action_just_released(SNAME("ui_right")) || input->is_action_just_released(SNAME("ui_left"))) {
			_set_active(false);
			return;
		}
	}

	if (use_vertical_axis) {
		if (input->is_action_just_released(SNAME("ui_down")) || input->is_action_just_released(SNAME("ui_up"))) {
			_set_active(false);
			return;
		}
	}
	gamepad_event_delay_ms -= p_delta;

	if (gamepad_event_delay_ms <= 0) {
		if (use_horizontal_axis) {
			if (input->is_action_pressed(SNAME("ui_right"))) {
				gamepad_event_delay_ms = GAMEPAD_EVENT_REPEAT_RATE_MS + gamepad_event_delay_ms;
				move_callback.call(Vector2i(1, 0));
			}

			if (input->is_action_pressed(SNAME("ui_left"))) {
				gamepad_event_delay_ms = GAMEPAD_EVENT_REPEAT_RATE_MS + gamepad_event_delay_ms;
				move_callback.call(Vector2i(-1, 0));
			}
		}

		if (use_vertical_axis) {
			if (input->is_action_pressed(SNAME("ui_down"))) {
				gamepad_event_delay_ms = GAMEPAD_EVENT_REPEAT_RATE_MS + gamepad_event_delay_ms;
				move_callback.call(Vector2i(0, 1));
			}

			if (input->is_action_pressed(SNAME("ui_up"))) {
				gamepad_event_delay_ms = GAMEPAD_EVENT_REPEAT_RATE_MS + gamepad_event_delay_ms;
				move_callback.call(Vector2i(0, -1));
			}
		}
	}
}
