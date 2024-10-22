/**************************************************************************/
/*  event_listener_line_edit.cpp                                          */
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

#include "editor/event_listener_line_edit.h"

#include "core/input/input_map.h"

// Maps to 2*axis if value is neg, or 2*axis+1 if value is pos.
static const char *_joy_axis_descriptions[(size_t)JoyAxis::MAX * 2] = {
	TTRC("Left Stick Left, Joystick 0 Left"),
	TTRC("Left Stick Right, Joystick 0 Right"),
	TTRC("Left Stick Up, Joystick 0 Up"),
	TTRC("Left Stick Down, Joystick 0 Down"),
	TTRC("Right Stick Left, Joystick 1 Left"),
	TTRC("Right Stick Right, Joystick 1 Right"),
	TTRC("Right Stick Up, Joystick 1 Up"),
	TTRC("Right Stick Down, Joystick 1 Down"),
	TTRC("Joystick 2 Left"),
	TTRC("Left Trigger, Sony L2, Xbox LT, Joystick 2 Right"),
	TTRC("Joystick 2 Up"),
	TTRC("Right Trigger, Sony R2, Xbox RT, Joystick 2 Down"),
	TTRC("Joystick 3 Left"),
	TTRC("Joystick 3 Right"),
	TTRC("Joystick 3 Up"),
	TTRC("Joystick 3 Down"),
	TTRC("Joystick 4 Left"),
	TTRC("Joystick 4 Right"),
	TTRC("Joystick 4 Up"),
	TTRC("Joystick 4 Down"),
};

String EventListenerLineEdit::get_event_text(const Ref<InputEvent> &p_event, bool p_include_device) {
	ERR_FAIL_COND_V_MSG(p_event.is_null(), String(), "Provided event is not a valid instance of InputEvent");

	String text;
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		String mods_text = key->InputEventWithModifiers::as_text();
		mods_text = mods_text.is_empty() ? mods_text : mods_text + "+";
		if (key->is_command_or_control_autoremap()) {
			if (OS::get_singleton()->has_feature("macos") || OS::get_singleton()->has_feature("web_macos") || OS::get_singleton()->has_feature("web_ios")) {
				mods_text = mods_text.replace("Command", "Command/Ctrl");
			} else {
				mods_text = mods_text.replace("Ctrl", "Command/Ctrl");
			}
		}

		if (key->get_keycode() != Key::NONE) {
			text += mods_text + keycode_get_string(key->get_keycode());
		}
		if (key->get_physical_keycode() != Key::NONE) {
			if (!text.is_empty()) {
				text += " " + TTR("or") + " ";
			}
			text += mods_text + keycode_get_string(key->get_physical_keycode()) + " (" + TTR("Physical");
			if (key->get_location() != KeyLocation::UNSPECIFIED) {
				text += " " + key->as_text_location();
			}
			text += ")";
		}
		if (key->get_key_label() != Key::NONE) {
			if (!text.is_empty()) {
				text += " " + TTR("or") + " ";
			}
			text += mods_text + keycode_get_string(key->get_key_label()) + " (" + TTR("Unicode") + ")";
		}

		if (text.is_empty()) {
			text = "(" + TTR("Unset") + ")";
		}
	} else {
		text = p_event->as_text();
	}

	Ref<InputEventMouse> mouse = p_event;
	Ref<InputEventJoypadMotion> jp_motion = p_event;
	Ref<InputEventJoypadButton> jp_button = p_event;
	if (jp_motion.is_valid()) {
		// Joypad motion events will display slightly differently than what the event->as_text() provides. See #43660.
		String desc = TTR("Unknown Joypad Axis");
		if (jp_motion->get_axis() < JoyAxis::MAX) {
			desc = TTR(_joy_axis_descriptions[2 * (size_t)jp_motion->get_axis() + (jp_motion->get_axis_value() < 0 ? 0 : 1)]);
		}

		// TRANSLATORS: %d is the axis number, the first %s is either "-" or "+", and the second %s is the description of the axis.
		text = vformat(TTR("Joypad Axis %d %s (%s)"), (int64_t)jp_motion->get_axis(), jp_motion->get_axis_value() < 0 ? "-" : "+", desc);
	}
	if (p_include_device && (mouse.is_valid() || jp_button.is_valid() || jp_motion.is_valid())) {
		String device_string = get_device_string(p_event->get_device());
		text += vformat(" - %s", device_string);
	}

	return text;
}

String EventListenerLineEdit::get_device_string(int p_device) {
	if (p_device == InputEvent::DEVICE_ID_ALL_DEVICES) {
		return TTR("All Devices");
	}
	return TTR("Device") + " " + itos(p_device);
}

bool EventListenerLineEdit::_is_event_allowed(const Ref<InputEvent> &p_event) const {
	const Ref<InputEventMouseButton> mb = p_event;
	const Ref<InputEventKey> k = p_event;
	const Ref<InputEventJoypadButton> jb = p_event;
	const Ref<InputEventJoypadMotion> jm = p_event;

	return (mb.is_valid() && (allowed_input_types & INPUT_MOUSE_BUTTON)) ||
			(k.is_valid() && (allowed_input_types & INPUT_KEY)) ||
			(jb.is_valid() && (allowed_input_types & INPUT_JOY_BUTTON)) ||
			(jm.is_valid() && (allowed_input_types & INPUT_JOY_MOTION));
}

void EventListenerLineEdit::gui_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		LineEdit::gui_input(p_event);
		return;
	}

	// Allow mouse button click on the clear button without being treated as an event.
	const Ref<InputEventMouseButton> b = p_event;
	if (b.is_valid() && _is_over_clear_button(b->get_position())) {
		LineEdit::gui_input(p_event);
		return;
	}

	// First event will be an event which is used to focus this control - i.e. a mouse click, or a tab press.
	// Ignore the first one so that clicking into the LineEdit does not override the current event.
	// Ignore is reset to true when the control is unfocused.
	// This class also specially handles grab_focus() calls.
	if (ignore_next_event) {
		ignore_next_event = false;
		return;
	}

	accept_event();
	if (!p_event->is_pressed() || p_event->is_echo() || p_event->is_match(event) || !_is_event_allowed(p_event)) {
		return;
	}

	event = p_event;
	set_text(get_event_text(event, false));
	emit_signal("event_changed", event);
}

void EventListenerLineEdit::_on_text_changed(const String &p_text) {
	if (p_text.is_empty()) {
		clear_event();
	}
}

void EventListenerLineEdit::_on_focus() {
	set_placeholder(TTR("Listening for Input"));
}

void EventListenerLineEdit::_on_unfocus() {
	ignore_next_event = true;
	set_placeholder(TTR("Filter by Event"));
}

Ref<InputEvent> EventListenerLineEdit::get_event() const {
	return event;
}

void EventListenerLineEdit::clear_event() {
	if (event.is_valid()) {
		event = Ref<InputEvent>();
		set_text("");
		emit_signal("event_changed", event);
	}
}

void EventListenerLineEdit::set_allowed_input_types(int p_type_masks) {
	allowed_input_types = p_type_masks;
}

int EventListenerLineEdit::get_allowed_input_types() const {
	return allowed_input_types;
}

void EventListenerLineEdit::grab_focus() {
	// If we grab focus through code, we don't need to ignore the first event!
	ignore_next_event = false;
	Control::grab_focus();
}

void EventListenerLineEdit::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			connect(SceneStringName(text_changed), callable_mp(this, &EventListenerLineEdit::_on_text_changed));
			connect(SceneStringName(focus_entered), callable_mp(this, &EventListenerLineEdit::_on_focus));
			connect(SceneStringName(focus_exited), callable_mp(this, &EventListenerLineEdit::_on_unfocus));
			set_right_icon(get_editor_theme_icon(SNAME("Keyboard")));
			set_clear_button_enabled(true);
		} break;
	}
}

void EventListenerLineEdit::_bind_methods() {
	ADD_SIGNAL(MethodInfo("event_changed", PropertyInfo(Variant::OBJECT, "event", PROPERTY_HINT_RESOURCE_TYPE, "InputEvent")));
}

EventListenerLineEdit::EventListenerLineEdit() {
	set_caret_blink_enabled(false);
	set_placeholder(TTR("Filter by Event"));
}
