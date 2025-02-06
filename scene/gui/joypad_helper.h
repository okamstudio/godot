/**************************************************************************/
/*  joypad_helper.h                                                       */
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

#ifndef JOYPAD_HELPER_H
#define JOYPAD_HELPER_H

#include "core/object/ref_counted.h"

class InputEvent;
class Node;

class JoypadHelper : public RefCounted {
	GDCLASS(JoypadHelper, RefCounted);

private:
	const float DEFAULT_GAMEPAD_EVENT_DELAY_MS = 0.5;
	const float GAMEPAD_EVENT_REPEAT_RATE_MS = 1.0 / 20;
	float gamepad_event_delay_ms = DEFAULT_GAMEPAD_EVENT_DELAY_MS;

	Node *owner = nullptr;
	bool use_horizontal_axis = false;
	bool use_vertical_axis = false;

	Callable move_callback;
	bool toggle_process = true;
	bool active = false;

	bool _check_initial_action(const Ref<InputEvent> &p_event, const StringName &p_action_name);
	void _set_active(bool p_active);

public:
	void setup(Node *p_owner, bool p_use_horizontal_axis, bool p_use_vertical_axis);
	void set_move_callback(const Callable &p_callback);
	void disable_process_toggle();

	bool process_event(const Ref<InputEvent> &p_event);
	void process_internal(double p_delta);
};

#endif // JOYPAD_HELPER_H
