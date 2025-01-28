/**************************************************************************/
/*  android_input_handler.cpp                                             */
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

#include "android_input_handler.h"

#include "android_keys_utils.h"
#include "display_server_android.h"

void AndroidInputHandler::process_joy_event(AndroidInputHandler::JoypadEvent p_event) {
	switch (p_event.type) {
		case JOY_EVENT_BUTTON:
			Input::get_singleton()->joy_button(p_event.device, (JoyButton)p_event.index, p_event.pressed);
			break;
		case JOY_EVENT_AXIS:
			Input::get_singleton()->joy_axis(p_event.device, (JoyAxis)p_event.index, p_event.value);
			break;
		case JOY_EVENT_HAT:
			Input::get_singleton()->joy_hat(p_event.device, p_event.hat);
			break;
		default:
			return;
	}
}

void AndroidInputHandler::_set_key_modifier_state(Ref<InputEventWithModifiers> ev, Key p_keycode) {
	if (p_keycode != Key::SHIFT) {
		ev->set_shift_pressed(shift_mem);
	}
	if (p_keycode != Key::ALT) {
		ev->set_alt_pressed(alt_mem);
	}
	if (p_keycode != Key::META) {
		ev->set_meta_pressed(meta_mem);
	}
	if (p_keycode != Key::CTRL) {
		ev->set_ctrl_pressed(control_mem);
	}
}

void AndroidInputHandler::process_key_event(int p_physical_keycode, int p_unicode, int p_key_label, bool p_pressed, bool p_echo) {
	static char32_t prev_wc = 0;
	char32_t unicode = p_unicode;
	if ((p_unicode & 0xfffffc00) == 0xd800) {
		if (prev_wc != 0) {
			ERR_PRINT("invalid utf16 surrogate input");
		}
		prev_wc = unicode;
		return; // Skip surrogate.
	} else if ((unicode & 0xfffffc00) == 0xdc00) {
		if (prev_wc == 0) {
			ERR_PRINT("invalid utf16 surrogate input");
			return; // Skip invalid surrogate.
		}
		unicode = (prev_wc << 10UL) + unicode - ((0xd800 << 10UL) + 0xdc00 - 0x10000);
		prev_wc = 0;
	} else {
		prev_wc = 0;
	}

	Ref<InputEventKey> ev;
	ev.instantiate();

	Key physical_keycode = godot_code_from_android_code(p_physical_keycode);
	Key keycode;
	if (unicode == '\b') { // 0x08
		keycode = Key::BACKSPACE;
	} else if (unicode == '\t') { // 0x09
		keycode = Key::TAB;
	} else if (unicode == '\n') { // 0x0A
		keycode = Key::ENTER;
	} else if (unicode == 0x1B) {
		keycode = Key::ESCAPE;
	} else if (unicode == 0x1F) {
		keycode = Key::KEY_DELETE;
	} else {
		keycode = fix_keycode(unicode, physical_keycode);
	}

	switch (physical_keycode) {
		case Key::SHIFT: {
			shift_mem = p_pressed;
		} break;
		case Key::ALT: {
			alt_mem = p_pressed;
		} break;
		case Key::CTRL: {
			control_mem = p_pressed;
		} break;
		case Key::META: {
			meta_mem = p_pressed;
		} break;
		default:
			break;
	}

	ev->set_keycode(keycode);
	ev->set_physical_keycode(physical_keycode);
	ev->set_key_label(fix_key_label(p_key_label, keycode));
	ev->set_unicode(fix_unicode(unicode));
	ev->set_location(godot_location_from_android_code(p_physical_keycode));
	ev->set_pressed(p_pressed);
	ev->set_echo(p_echo);

	_set_key_modifier_state(ev, keycode);

	if (p_physical_keycode == AKEYCODE_BACK && p_pressed) {
		if (DisplayServerAndroid *dsa = Object::cast_to<DisplayServerAndroid>(DisplayServer::get_singleton())) {
			dsa->send_window_event(DisplayServer::WINDOW_EVENT_GO_BACK_REQUEST, true);
		}
	}

	Input::get_singleton()->parse_input_event(ev);
}

void AndroidInputHandler::_cancel_all_touch() {
	_parse_all_touch(false, true);
	touch.clear();
}

void AndroidInputHandler::_parse_all_touch(bool p_pressed, bool p_canceled, bool p_double_tap) {
	if (touch.size()) {
		//end all if exist
		for (int i = 0; i < touch.size(); i++) {
			Ref<InputEventScreenTouch> ev;
			ev.instantiate();
			ev->set_index(touch[i].id);
			ev->set_pressed(p_pressed);
			ev->set_canceled(p_canceled);
			ev->set_position(touch[i].pos);
			ev->set_double_tap(p_double_tap);
			Input::get_singleton()->parse_input_event(ev);
		}
	}
}

void AndroidInputHandler::_release_all_touch() {
	_parse_all_touch(false, false);
	touch.clear();
}

void AndroidInputHandler::process_touch_event(int p_event, int p_pointer, const Vector<TouchPos> &p_points, bool p_double_tap) {
	switch (p_event) {
		case AMOTION_EVENT_ACTION_DOWN: { //gesture begin
			// Release any remaining touches or mouse event
			_release_mouse_event_info();
			_release_all_touch();

			touch.resize(p_points.size());
			for (int i = 0; i < p_points.size(); i++) {
				touch.write[i].id = p_points[i].id;
				touch.write[i].pos = p_points[i].pos;
				touch.write[i].pressure = p_points[i].pressure;
				touch.write[i].tilt = p_points[i].tilt;
			}

			//send touch
			_parse_all_touch(true, false, p_double_tap);

		} break;
		case AMOTION_EVENT_ACTION_MOVE: { //motion
			if (touch.size() != p_points.size()) {
				return;
			}

			for (int i = 0; i < touch.size(); i++) {
				int idx = -1;
				for (int j = 0; j < p_points.size(); j++) {
					if (touch[i].id == p_points[j].id) {
						idx = j;
						break;
					}
				}

				ERR_CONTINUE(idx == -1);

				if (touch[i].pos == p_points[idx].pos) {
					continue; // Don't move unnecessarily.
				}

				Ref<InputEventScreenDrag> ev;
				ev.instantiate();
				ev->set_index(touch[i].id);
				ev->set_position(p_points[idx].pos);
				ev->set_relative(p_points[idx].pos - touch[i].pos);
				ev->set_relative_screen_position(ev->get_relative());
				ev->set_pressure(p_points[idx].pressure);
				ev->set_tilt(p_points[idx].tilt);
				Input::get_singleton()->parse_input_event(ev);
				touch.write[i].pos = p_points[idx].pos;
			}

		} break;
		case AMOTION_EVENT_ACTION_CANCEL: {
			_cancel_all_touch();
		} break;
		case AMOTION_EVENT_ACTION_UP: { //release
			_release_all_touch();
		} break;
		case AMOTION_EVENT_ACTION_POINTER_DOWN: { // add touch
			for (int i = 0; i < p_points.size(); i++) {
				if (p_points[i].id == p_pointer) {
					TouchPos tp = p_points[i];
					touch.push_back(tp);

					Ref<InputEventScreenTouch> ev;
					ev.instantiate();

					ev->set_index(tp.id);
					ev->set_pressed(true);
					ev->set_position(tp.pos);
					Input::get_singleton()->parse_input_event(ev);

					break;
				}
			}
		} break;
		case AMOTION_EVENT_ACTION_POINTER_UP: { // remove touch
			for (int i = 0; i < touch.size(); i++) {
				if (touch[i].id == p_pointer) {
					Ref<InputEventScreenTouch> ev;
					ev.instantiate();
					ev->set_index(touch[i].id);
					ev->set_pressed(false);
					ev->set_position(touch[i].pos);
					Input::get_singleton()->parse_input_event(ev);
					touch.remove_at(i);

					break;
				}
			}
		} break;
	}
}

void AndroidInputHandler::_cancel_mouse_event_info(bool p_source_mouse_relative) {
	buttons_state = BitField<MouseButtonMask>();
	_parse_mouse_event_info(BitField<MouseButtonMask>(), false, true, false, p_source_mouse_relative);
	mouse_event_info.valid = false;
}

void AndroidInputHandler::_parse_mouse_event_info(BitField<MouseButtonMask> event_buttons_mask, bool p_pressed, bool p_canceled, bool p_double_click, bool p_source_mouse_relative) {
	if (!mouse_event_info.valid) {
		return;
	}

	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	_set_key_modifier_state(ev, Key::NONE);
	if (p_source_mouse_relative) {
		ev->set_position(hover_prev_pos);
		ev->set_global_position(hover_prev_pos);
	} else {
		ev->set_position(mouse_event_info.pos);
		ev->set_global_position(mouse_event_info.pos);
		hover_prev_pos = mouse_event_info.pos;
	}
	ev->set_pressed(p_pressed);
	ev->set_canceled(p_canceled);
	BitField<MouseButtonMask> changed_button_mask = BitField<MouseButtonMask>(buttons_state.operator int64_t() ^ event_buttons_mask.operator int64_t());

	buttons_state = event_buttons_mask;

	ev->set_button_index(_button_index_from_mask(changed_button_mask));
	ev->set_button_mask(event_buttons_mask);
	ev->set_double_click(p_double_click);
	Input::get_singleton()->parse_input_event(ev);
}

void AndroidInputHandler::_release_mouse_event_info(bool p_source_mouse_relative) {
	_parse_mouse_event_info(BitField<MouseButtonMask>(), false, false, false, p_source_mouse_relative);
	mouse_event_info.valid = false;
}

void AndroidInputHandler::process_mouse_event(int p_event_action, int p_event_android_buttons_mask, Point2 p_event_pos, Vector2 p_delta, bool p_double_click, bool p_source_mouse_relative, float p_pressure, Vector2 p_tilt) {
	BitField<MouseButtonMask> event_buttons_mask = _android_button_mask_to_godot_button_mask(p_event_android_buttons_mask);
	switch (p_event_action) {
		case AMOTION_EVENT_ACTION_HOVER_MOVE: // hover move
		case AMOTION_EVENT_ACTION_HOVER_ENTER: // hover enter
		case AMOTION_EVENT_ACTION_HOVER_EXIT: { // hover exit
			// https://developer.android.com/reference/android/view/MotionEvent.html#ACTION_HOVER_ENTER
			Ref<InputEventMouseMotion> ev;
			ev.instantiate();
			_set_key_modifier_state(ev, Key::NONE);
			ev->set_position(p_event_pos);
			ev->set_global_position(p_event_pos);
			ev->set_relative(p_event_pos - hover_prev_pos);
			ev->set_relative_screen_position(ev->get_relative());
			Input::get_singleton()->parse_input_event(ev);
			hover_prev_pos = p_event_pos;
		} break;

		case AMOTION_EVENT_ACTION_DOWN:
		case AMOTION_EVENT_ACTION_BUTTON_PRESS: {
			// Release any remaining touches or mouse event
			_release_mouse_event_info();
			_release_all_touch();

			mouse_event_info.valid = true;
			mouse_event_info.pos = p_event_pos;
			_parse_mouse_event_info(event_buttons_mask, true, false, p_double_click, p_source_mouse_relative);
		} break;

		case AMOTION_EVENT_ACTION_CANCEL: {
			_cancel_mouse_event_info(p_source_mouse_relative);
		} break;

		case AMOTION_EVENT_ACTION_UP:
		case AMOTION_EVENT_ACTION_BUTTON_RELEASE: {
			_release_mouse_event_info(p_source_mouse_relative);
		} break;

		case AMOTION_EVENT_ACTION_MOVE: {
			if (!mouse_event_info.valid) {
				return;
			}

			Ref<InputEventMouseMotion> ev;
			ev.instantiate();
			_set_key_modifier_state(ev, Key::NONE);
			if (p_source_mouse_relative) {
				ev->set_position(hover_prev_pos);
				ev->set_global_position(hover_prev_pos);
				ev->set_relative(p_event_pos);
				ev->set_relative_screen_position(p_event_pos);
			} else {
				ev->set_position(p_event_pos);
				ev->set_global_position(p_event_pos);
				ev->set_relative(p_event_pos - hover_prev_pos);
				ev->set_relative_screen_position(ev->get_relative());
				mouse_event_info.pos = p_event_pos;
				hover_prev_pos = p_event_pos;
			}
			ev->set_button_mask(event_buttons_mask);
			ev->set_pressure(p_pressure);
			ev->set_tilt(p_tilt);
			Input::get_singleton()->parse_input_event(ev);
		} break;

		case AMOTION_EVENT_ACTION_SCROLL: {
			Ref<InputEventMouseButton> ev;
			ev.instantiate();
			_set_key_modifier_state(ev, Key::NONE);
			if (p_source_mouse_relative) {
				ev->set_position(hover_prev_pos);
				ev->set_global_position(hover_prev_pos);
			} else {
				ev->set_position(p_event_pos);
				ev->set_global_position(p_event_pos);
			}
			ev->set_pressed(true);
			buttons_state = event_buttons_mask;
			if (p_delta.y > 0) {
				_wheel_button_click(event_buttons_mask, ev, MouseButton::WHEEL_UP, p_delta.y);
			} else if (p_delta.y < 0) {
				_wheel_button_click(event_buttons_mask, ev, MouseButton::WHEEL_DOWN, -p_delta.y);
			}

			if (p_delta.x > 0) {
				_wheel_button_click(event_buttons_mask, ev, MouseButton::WHEEL_RIGHT, p_delta.x);
			} else if (p_delta.x < 0) {
				_wheel_button_click(event_buttons_mask, ev, MouseButton::WHEEL_LEFT, -p_delta.x);
			}
		} break;
	}
}

void AndroidInputHandler::_wheel_button_click(BitField<MouseButtonMask> event_buttons_mask, const Ref<InputEventMouseButton> &ev, MouseButton wheel_button, float factor) {
	Ref<InputEventMouseButton> evd = ev->duplicate();
	_set_key_modifier_state(evd, Key::NONE);
	evd->set_button_index(wheel_button);
	evd->set_button_mask(BitField<MouseButtonMask>(event_buttons_mask.operator int64_t() ^ int64_t(mouse_button_to_mask(wheel_button))));
	evd->set_factor(factor);
	Input::get_singleton()->parse_input_event(evd);
	Ref<InputEventMouseButton> evdd = evd->duplicate();
	evdd->set_pressed(false);
	evdd->set_button_mask(event_buttons_mask);
	Input::get_singleton()->parse_input_event(evdd);
}

void AndroidInputHandler::process_magnify(Point2 p_pos, float p_factor) {
	Ref<InputEventMagnifyGesture> magnify_event;
	magnify_event.instantiate();
	_set_key_modifier_state(magnify_event, Key::NONE);
	magnify_event->set_position(p_pos);
	magnify_event->set_factor(p_factor);
	Input::get_singleton()->parse_input_event(magnify_event);
}

void AndroidInputHandler::process_pan(Point2 p_pos, Vector2 p_delta) {
	Ref<InputEventPanGesture> pan_event;
	pan_event.instantiate();
	_set_key_modifier_state(pan_event, Key::NONE);
	pan_event->set_position(p_pos);
	pan_event->set_delta(p_delta);
	Input::get_singleton()->parse_input_event(pan_event);
}

MouseButton AndroidInputHandler::_button_index_from_mask(BitField<MouseButtonMask> button_mask) {
	switch (MouseButtonMask(button_mask.operator int64_t())) {
		case MouseButtonMask::LEFT:
			return MouseButton::LEFT;
		case MouseButtonMask::RIGHT:
			return MouseButton::RIGHT;
		case MouseButtonMask::MIDDLE:
			return MouseButton::MIDDLE;
		case MouseButtonMask::MB_XBUTTON1:
			return MouseButton::MB_XBUTTON1;
		case MouseButtonMask::MB_XBUTTON2:
			return MouseButton::MB_XBUTTON2;
		default:
			return MouseButton::NONE;
	}
}

BitField<MouseButtonMask> AndroidInputHandler::_android_button_mask_to_godot_button_mask(int android_button_mask) {
	BitField<MouseButtonMask> godot_button_mask;
	if (android_button_mask & AMOTION_EVENT_BUTTON_PRIMARY) {
		godot_button_mask.set_flag(MouseButtonMask::LEFT);
	}
	if (android_button_mask & AMOTION_EVENT_BUTTON_SECONDARY) {
		godot_button_mask.set_flag(MouseButtonMask::RIGHT);
	}
	if (android_button_mask & AMOTION_EVENT_BUTTON_TERTIARY) {
		godot_button_mask.set_flag(MouseButtonMask::MIDDLE);
	}
	if (android_button_mask & AMOTION_EVENT_BUTTON_BACK) {
		godot_button_mask.set_flag(MouseButtonMask::MB_XBUTTON1);
	}
	if (android_button_mask & AMOTION_EVENT_BUTTON_FORWARD) {
		godot_button_mask.set_flag(MouseButtonMask::MB_XBUTTON2);
	}

	return godot_button_mask;
}
