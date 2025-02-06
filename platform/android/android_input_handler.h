/**************************************************************************/
/*  android_input_handler.h                                               */
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

#include "core/input/input.h"

// This class encapsulates all the handling of input events that come from the Android UI thread.
// Remarks:
// - It's not thread-safe by itself, so its functions must only be called on a single thread, which is the Android UI thread.
// - Its functions must only call thread-safe methods.
class AndroidInputHandler {
public:
	struct TouchPos {
		int id = 0;
		Point2 pos;
		float pressure = 0;
		Vector2 tilt;
	};

	struct MouseEventInfo {
		bool valid = false;
		Point2 pos;
	};

	enum {
		JOY_EVENT_BUTTON = 0,
		JOY_EVENT_AXIS = 1,
		JOY_EVENT_HAT = 2
	};

	struct JoypadEvent {
		int device = 0;
		int type = 0;
		int index = 0; // Can be either JoyAxis or JoyButton.
		bool pressed = false;
		float value = 0;
		BitField<HatMask> hat;
	};

private:
	bool alt_mem = false;
	bool shift_mem = false;
	bool control_mem = false;
	bool meta_mem = false;

	BitField<MouseButtonMask> buttons_state;

	Vector<TouchPos> touch;
	MouseEventInfo mouse_event_info;
	Point2 hover_prev_pos; // needed to calculate the relative position on hover events

	void _set_key_modifier_state(Ref<InputEventWithModifiers> ev, Key p_keycode);

	static MouseButton _button_index_from_mask(BitField<MouseButtonMask> button_mask);
	static BitField<MouseButtonMask> _android_button_mask_to_godot_button_mask(int android_button_mask);

	void _wheel_button_click(BitField<MouseButtonMask> event_buttons_mask, const Ref<InputEventMouseButton> &ev, MouseButton wheel_button, float factor);

	void _parse_mouse_event_info(BitField<MouseButtonMask> event_buttons_mask, bool p_pressed, bool p_canceled, bool p_double_click, bool p_source_mouse_relative);

	void _release_mouse_event_info(bool p_source_mouse_relative = false);

	void _cancel_mouse_event_info(bool p_source_mouse_relative = false);

	void _parse_all_touch(bool p_pressed, bool p_canceled = false, bool p_double_tap = false);

	void _release_all_touch();

	void _cancel_all_touch();

public:
	void process_mouse_event(int p_event_action, int p_event_android_buttons_mask, Point2 p_event_pos, Vector2 p_delta, bool p_double_click, bool p_source_mouse_relative, float p_pressure, Vector2 p_tilt);
	void process_touch_event(int p_event, int p_pointer, const Vector<TouchPos> &p_points, bool p_double_tap);
	void process_magnify(Point2 p_pos, float p_factor);
	void process_pan(Point2 p_pos, Vector2 p_delta);
	void process_joy_event(JoypadEvent p_event);
	void process_key_event(int p_physical_keycode, int p_unicode, int p_key_label, bool p_pressed, bool p_echo);
};
