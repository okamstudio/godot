/**************************************************************************/
/*  display_server_web.cpp                                                */
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

#include "display_server_web.h"

#include "dom_keys.inc"
#include "godot_js.h"
#include "os_web.h"

#include "core/config/project_settings.h"
#include "core/object/callable_method_pointer.h"
#include "servers/rendering/dummy/rasterizer_dummy.h"

#ifdef GLES3_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"
#endif

#include <emscripten.h>
#include <png.h>

#define DOM_BUTTON_LEFT 0
#define DOM_BUTTON_MIDDLE 1
#define DOM_BUTTON_RIGHT 2
#define DOM_BUTTON_XBUTTON1 3
#define DOM_BUTTON_XBUTTON2 4

DisplayServerWeb *DisplayServerWeb::get_singleton() {
	return static_cast<DisplayServerWeb *>(DisplayServer::get_singleton());
}

// Window (canvas)
bool DisplayServerWeb::check_size_force_redraw() {
	bool size_changed = godot_js_display_size_update() != 0;
	if (size_changed && rect_changed_callback.is_valid()) {
		Size2i window_size = window_get_size();
		Variant size = Rect2i(Point2i(), window_size); // TODO use window_get_position if implemented.
		rect_changed_callback.call(size);
		emscripten_set_canvas_element_size(canvas_id, window_size.x, window_size.y);
	}
	return size_changed;
}

void DisplayServerWeb::fullscreen_change_callback(int p_fullscreen) {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_fullscreen_change_callback).call_deferred(p_fullscreen);
		return;
	}
#endif

	_fullscreen_change_callback(p_fullscreen);
}

void DisplayServerWeb::_fullscreen_change_callback(int p_fullscreen) {
	DisplayServerWeb *display = get_singleton();
	if (p_fullscreen) {
		display->window_mode = WINDOW_MODE_FULLSCREEN;
	} else {
		display->window_mode = WINDOW_MODE_WINDOWED;
	}
}

// Drag and drop callback.
void DisplayServerWeb::drop_files_js_callback(const char **p_filev, int p_filec) {
	Vector<String> files;
	for (int i = 0; i < p_filec; i++) {
		files.push_back(String::utf8(p_filev[i]));
	}

#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_drop_files_js_callback).call_deferred(files);
		return;
	}
#endif

	_drop_files_js_callback(files);
}

void DisplayServerWeb::_drop_files_js_callback(const Vector<String> &p_files) {
	DisplayServerWeb *ds = get_singleton();
	if (!ds) {
		ERR_FAIL_MSG("Unable to drop files because the DisplayServer is not active");
	}
	if (!ds->drop_files_callback.is_valid()) {
		return;
	}
	Variant v_files = p_files;
	const Variant *v_args[1] = { &v_files };
	Variant ret;
	Callable::CallError ce;
	ds->drop_files_callback.callp((const Variant **)&v_args, 1, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_PRINT(vformat("Failed to execute drop files callback: %s.", Variant::get_callable_error_text(ds->drop_files_callback, v_args, 1, ce)));
	}
}

// Web quit request callback.
void DisplayServerWeb::request_quit_callback() {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_request_quit_callback).call_deferred();
		return;
	}
#endif

	_request_quit_callback();
}

void DisplayServerWeb::_request_quit_callback() {
	DisplayServerWeb *ds = get_singleton();
	if (ds && ds->window_event_callback.is_valid()) {
		Variant event = int(DisplayServer::WINDOW_EVENT_CLOSE_REQUEST);
		ds->window_event_callback.call(event);
	}
}

// Keys

void DisplayServerWeb::dom2godot_mod(Ref<InputEventWithModifiers> ev, int p_mod, Key p_keycode) {
	if (p_keycode != Key::SHIFT) {
		ev->set_shift_pressed(p_mod & 1);
	}
	if (p_keycode != Key::ALT) {
		ev->set_alt_pressed(p_mod & 2);
	}
	if (p_keycode != Key::CTRL) {
		ev->set_ctrl_pressed(p_mod & 4);
	}
	if (p_keycode != Key::META) {
		ev->set_meta_pressed(p_mod & 8);
	}
}

void DisplayServerWeb::key_callback(int p_pressed, int p_repeat, int p_modifiers) {
	DisplayServerWeb *ds = get_singleton();
	JSKeyEvent &key_event = ds->key_event;

	const String code = String::utf8(key_event.code);
	const String key = String::utf8(key_event.key);

#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_key_callback).call_deferred(code, key, p_pressed, p_repeat, p_modifiers);
		return;
	}
#endif

	_key_callback(code, key, p_pressed, p_repeat, p_modifiers);
}

void DisplayServerWeb::_key_callback(const String &p_key_event_code, const String &p_key_event_key, int p_pressed, int p_repeat, int p_modifiers) {
	// Resume audio context after input in case autoplay was denied.
	OS_Web::get_singleton()->resume_audio();

	DisplayServerWeb *ds = get_singleton();
	if (ds->ime_started) {
		return;
	}

	char32_t c = 0x00;
	String unicode = p_key_event_key;
	if (unicode.length() == 1) {
		c = unicode[0];
	}

	Key keycode = dom_code2godot_scancode(p_key_event_code.utf8().get_data(), p_key_event_key.utf8().get_data(), false);
	Key scancode = dom_code2godot_scancode(p_key_event_code.utf8().get_data(), p_key_event_key.utf8().get_data(), true);
	KeyLocation location = dom_code2godot_key_location(p_key_event_code.utf8().get_data());

	DisplayServerWeb::KeyEvent ke;

	ke.pressed = p_pressed;
	ke.echo = p_repeat;
	ke.raw = true;
	ke.keycode = fix_keycode(c, keycode);
	ke.physical_keycode = scancode;
	ke.key_label = fix_key_label(c, keycode);
	ke.unicode = fix_unicode(c);
	ke.location = location;
	ke.mod = p_modifiers;

	if (ds->key_event_pos >= ds->key_event_buffer.size()) {
		ds->key_event_buffer.resize(1 + ds->key_event_pos);
	}
	ds->key_event_buffer.write[ds->key_event_pos++] = ke;

	// Make sure to flush all events so we can call restricted APIs inside the event.
	Input::get_singleton()->flush_buffered_events();
}

// Mouse

int DisplayServerWeb::mouse_button_callback(int p_pressed, int p_button, double p_x, double p_y, int p_modifiers) {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_mouse_button_callback).call_deferred(p_pressed, p_button, p_x, p_y, p_modifiers);
		return true;
	}
#endif

	return _mouse_button_callback(p_pressed, p_button, p_x, p_y, p_modifiers);
}

int DisplayServerWeb::_mouse_button_callback(int p_pressed, int p_button, double p_x, double p_y, int p_modifiers) {
	DisplayServerWeb *ds = get_singleton();

	Point2 pos(p_x, p_y);
	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	ev->set_position(pos);
	ev->set_global_position(pos);
	ev->set_pressed(p_pressed);
	dom2godot_mod(ev, p_modifiers, Key::NONE);

	switch (p_button) {
		case DOM_BUTTON_LEFT:
			ev->set_button_index(MouseButton::LEFT);
			break;
		case DOM_BUTTON_MIDDLE:
			ev->set_button_index(MouseButton::MIDDLE);
			break;
		case DOM_BUTTON_RIGHT:
			ev->set_button_index(MouseButton::RIGHT);
			break;
		case DOM_BUTTON_XBUTTON1:
			ev->set_button_index(MouseButton::MB_XBUTTON1);
			break;
		case DOM_BUTTON_XBUTTON2:
			ev->set_button_index(MouseButton::MB_XBUTTON2);
			break;
		default:
			return false;
	}

	if (p_pressed) {
		uint64_t diff = (OS::get_singleton()->get_ticks_usec() / 1000) - ds->last_click_ms;

		if (ev->get_button_index() == ds->last_click_button_index) {
			if (diff < 400 && Point2(ds->last_click_pos).distance_to(ev->get_position()) < 5) {
				ds->last_click_ms = 0;
				ds->last_click_pos = Point2(-100, -100);
				ds->last_click_button_index = MouseButton::NONE;
				ev->set_double_click(true);
			}

		} else {
			ds->last_click_button_index = ev->get_button_index();
		}

		if (!ev->is_double_click()) {
			ds->last_click_ms += diff;
			ds->last_click_pos = ev->get_position();
		}
	}

	BitField<MouseButtonMask> mask = Input::get_singleton()->get_mouse_button_mask();
	MouseButtonMask button_flag = mouse_button_to_mask(ev->get_button_index());
	if (ev->is_pressed()) {
		mask.set_flag(button_flag);
	} else if (mask.has_flag(button_flag)) {
		mask.clear_flag(button_flag);
	} else {
		// Received release event, but press was outside the canvas, so ignore.
		return false;
	}
	ev->set_button_mask(mask);

	Input::get_singleton()->parse_input_event(ev);
	// Resume audio context after input in case autoplay was denied.
	OS_Web::get_singleton()->resume_audio();

	// Make sure to flush all events so we can call restricted APIs inside the event.
	Input::get_singleton()->flush_buffered_events();

	// Prevent multi-click text selection and wheel-click scrolling anchor.
	// Context menu is prevented through contextmenu event.
	return true;
}

void DisplayServerWeb::mouse_move_callback(double p_x, double p_y, double p_rel_x, double p_rel_y, int p_modifiers) {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_mouse_move_callback).call_deferred(p_x, p_y, p_rel_x, p_rel_y, p_modifiers);
		return;
	}
#endif

	_mouse_move_callback(p_x, p_y, p_rel_x, p_rel_y, p_modifiers);
}

void DisplayServerWeb::_mouse_move_callback(double p_x, double p_y, double p_rel_x, double p_rel_y, int p_modifiers) {
	BitField<MouseButtonMask> input_mask = Input::get_singleton()->get_mouse_button_mask();
	// For motion outside the canvas, only read mouse movement if dragging
	// started inside the canvas; imitating desktop app behavior.
	if (!get_singleton()->cursor_inside_canvas && input_mask.is_empty()) {
		return;
	}

	Point2 pos(p_x, p_y);
	Ref<InputEventMouseMotion> ev;
	ev.instantiate();
	dom2godot_mod(ev, p_modifiers, Key::NONE);
	ev->set_button_mask(input_mask);

	ev->set_position(pos);
	ev->set_global_position(pos);

	ev->set_relative(Vector2(p_rel_x, p_rel_y));
	ev->set_relative_screen_position(ev->get_relative());
	ev->set_velocity(Input::get_singleton()->get_last_mouse_velocity());
	ev->set_screen_velocity(ev->get_velocity());

	Input::get_singleton()->parse_input_event(ev);
}

// Cursor
const char *DisplayServerWeb::godot2dom_cursor(DisplayServer::CursorShape p_shape) {
	switch (p_shape) {
		case DisplayServer::CURSOR_ARROW:
			return "default";
		case DisplayServer::CURSOR_IBEAM:
			return "text";
		case DisplayServer::CURSOR_POINTING_HAND:
			return "pointer";
		case DisplayServer::CURSOR_CROSS:
			return "crosshair";
		case DisplayServer::CURSOR_WAIT:
			return "wait";
		case DisplayServer::CURSOR_BUSY:
			return "progress";
		case DisplayServer::CURSOR_DRAG:
			return "grab";
		case DisplayServer::CURSOR_CAN_DROP:
			return "grabbing";
		case DisplayServer::CURSOR_FORBIDDEN:
			return "no-drop";
		case DisplayServer::CURSOR_VSIZE:
			return "ns-resize";
		case DisplayServer::CURSOR_HSIZE:
			return "ew-resize";
		case DisplayServer::CURSOR_BDIAGSIZE:
			return "nesw-resize";
		case DisplayServer::CURSOR_FDIAGSIZE:
			return "nwse-resize";
		case DisplayServer::CURSOR_MOVE:
			return "move";
		case DisplayServer::CURSOR_VSPLIT:
			return "row-resize";
		case DisplayServer::CURSOR_HSPLIT:
			return "col-resize";
		case DisplayServer::CURSOR_HELP:
			return "help";
		default:
			return "default";
	}
}

bool DisplayServerWeb::tts_is_speaking() const {
	ERR_FAIL_COND_V_MSG(!tts, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	return godot_js_tts_is_speaking();
}

bool DisplayServerWeb::tts_is_paused() const {
	ERR_FAIL_COND_V_MSG(!tts, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	return godot_js_tts_is_paused();
}

void DisplayServerWeb::update_voices_callback(int p_size, const char **p_voice) {
	Vector<String> voices;
	for (int i = 0; i < p_size; i++) {
		voices.append(String::utf8(p_voice[i]));
	}

#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_update_voices_callback).call_deferred(voices);
		return;
	}
#endif

	_update_voices_callback(voices);
}

void DisplayServerWeb::_update_voices_callback(const Vector<String> &p_voices) {
	get_singleton()->voices.clear();
	for (int i = 0; i < p_voices.size(); i++) {
		Vector<String> tokens = p_voices[i].split(";", true, 2);
		if (tokens.size() == 2) {
			Dictionary voice_d;
			voice_d["name"] = tokens[1];
			voice_d["id"] = tokens[1];
			voice_d["language"] = tokens[0];
			get_singleton()->voices.push_back(voice_d);
		}
	}
}

TypedArray<Dictionary> DisplayServerWeb::tts_get_voices() const {
	ERR_FAIL_COND_V_MSG(!tts, TypedArray<Dictionary>(), "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	godot_js_tts_get_voices(update_voices_callback);
	return voices;
}

void DisplayServerWeb::tts_speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	ERR_FAIL_COND_MSG(!tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	if (p_interrupt) {
		tts_stop();
	}

	if (p_text.is_empty()) {
		tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, p_utterance_id);
		return;
	}

	CharString string = p_text.utf8();
	utterance_ids[p_utterance_id] = string;

	godot_js_tts_speak(string.get_data(), p_voice.utf8().get_data(), CLAMP(p_volume, 0, 100), CLAMP(p_pitch, 0.f, 2.f), CLAMP(p_rate, 0.1f, 10.f), p_utterance_id, DisplayServerWeb::js_utterance_callback);
}

void DisplayServerWeb::tts_pause() {
	ERR_FAIL_COND_MSG(!tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	godot_js_tts_pause();
}

void DisplayServerWeb::tts_resume() {
	ERR_FAIL_COND_MSG(!tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	godot_js_tts_resume();
}

void DisplayServerWeb::tts_stop() {
	ERR_FAIL_COND_MSG(!tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	for (const KeyValue<int, CharString> &E : utterance_ids) {
		tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, E.key);
	}
	utterance_ids.clear();
	godot_js_tts_stop();
}

void DisplayServerWeb::js_utterance_callback(int p_event, int p_id, int p_pos) {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_js_utterance_callback).call_deferred(p_event, p_id, p_pos);
		return;
	}
#endif

	_js_utterance_callback(p_event, p_id, p_pos);
}

void DisplayServerWeb::_js_utterance_callback(int p_event, int p_id, int p_pos) {
	DisplayServerWeb *ds = (DisplayServerWeb *)DisplayServer::get_singleton();
	if (ds->utterance_ids.has(p_id)) {
		int pos = 0;
		if ((TTSUtteranceEvent)p_event == DisplayServer::TTS_UTTERANCE_BOUNDARY) {
			// Convert position from UTF-8 to UTF-32.
			const CharString &string = ds->utterance_ids[p_id];
			for (int i = 0; i < MIN(p_pos, string.length()); i++) {
				uint8_t c = string[i];
				if ((c & 0xe0) == 0xc0) {
					i += 1;
				} else if ((c & 0xf0) == 0xe0) {
					i += 2;
				} else if ((c & 0xf8) == 0xf0) {
					i += 3;
				}
				pos++;
			}
		} else if ((TTSUtteranceEvent)p_event != DisplayServer::TTS_UTTERANCE_STARTED) {
			ds->utterance_ids.erase(p_id);
		}
		ds->tts_post_utterance_event((TTSUtteranceEvent)p_event, p_id, pos);
	}
}

void DisplayServerWeb::cursor_set_shape(CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	if (cursor_shape == p_shape) {
		return;
	}
	cursor_shape = p_shape;
	godot_js_display_cursor_set_shape(godot2dom_cursor(cursor_shape));
}

DisplayServer::CursorShape DisplayServerWeb::cursor_get_shape() const {
	return cursor_shape;
}

void DisplayServerWeb::cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	if (p_cursor.is_valid()) {
		Ref<Image> image = _get_cursor_image_from_resource(p_cursor, p_hotspot);
		ERR_FAIL_COND(image.is_null());
		Vector2i texture_size = image->get_size();

		if (image->get_format() != Image::FORMAT_RGBA8) {
			image->convert(Image::FORMAT_RGBA8);
		}

		png_image png_meta;
		memset(&png_meta, 0, sizeof png_meta);
		png_meta.version = PNG_IMAGE_VERSION;
		png_meta.width = texture_size.width;
		png_meta.height = texture_size.height;
		png_meta.format = PNG_FORMAT_RGBA;

		PackedByteArray png;
		size_t len;
		PackedByteArray data = image->get_data();
		ERR_FAIL_COND(!png_image_write_get_memory_size(png_meta, len, 0, data.ptr(), 0, nullptr));

		png.resize(len);
		ERR_FAIL_COND(!png_image_write_to_memory(&png_meta, png.ptrw(), &len, 0, data.ptr(), 0, nullptr));

		godot_js_display_cursor_set_custom_shape(godot2dom_cursor(p_shape), png.ptr(), len, p_hotspot.x, p_hotspot.y);

	} else {
		godot_js_display_cursor_set_custom_shape(godot2dom_cursor(p_shape), nullptr, 0, 0, 0);
	}

	cursor_set_shape(cursor_shape);
}

// Mouse mode
void DisplayServerWeb::mouse_set_mode(MouseMode p_mode) {
	ERR_FAIL_COND_MSG(p_mode == MOUSE_MODE_CONFINED || p_mode == MOUSE_MODE_CONFINED_HIDDEN, "MOUSE_MODE_CONFINED is not supported for the Web platform.");
	if (p_mode == mouse_get_mode()) {
		return;
	}

	if (p_mode == MOUSE_MODE_VISIBLE) {
		godot_js_display_cursor_set_visible(1);
		godot_js_display_cursor_lock_set(0);

	} else if (p_mode == MOUSE_MODE_HIDDEN) {
		godot_js_display_cursor_set_visible(0);
		godot_js_display_cursor_lock_set(0);

	} else if (p_mode == MOUSE_MODE_CAPTURED) {
		godot_js_display_cursor_set_visible(1);
		godot_js_display_cursor_lock_set(1);
	}
}

DisplayServer::MouseMode DisplayServerWeb::mouse_get_mode() const {
	if (godot_js_display_cursor_is_hidden()) {
		return MOUSE_MODE_HIDDEN;
	}

	if (godot_js_display_cursor_is_locked()) {
		return MOUSE_MODE_CAPTURED;
	}
	return MOUSE_MODE_VISIBLE;
}

Point2i DisplayServerWeb::mouse_get_position() const {
	return Input::get_singleton()->get_mouse_position();
}

// Wheel
int DisplayServerWeb::mouse_wheel_callback(double p_delta_x, double p_delta_y) {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_mouse_wheel_callback).call_deferred(p_delta_x, p_delta_y);
		return true;
	}
#endif

	return _mouse_wheel_callback(p_delta_x, p_delta_y);
}

int DisplayServerWeb::_mouse_wheel_callback(double p_delta_x, double p_delta_y) {
	if (!godot_js_display_canvas_is_focused() && !godot_js_is_ime_focused()) {
		if (get_singleton()->cursor_inside_canvas) {
			godot_js_display_canvas_focus();
		} else {
			return false;
		}
	}

	Input *input = Input::get_singleton();
	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	ev->set_position(input->get_mouse_position());
	ev->set_global_position(ev->get_position());

	ev->set_shift_pressed(input->is_key_pressed(Key::SHIFT));
	ev->set_alt_pressed(input->is_key_pressed(Key::ALT));
	ev->set_ctrl_pressed(input->is_key_pressed(Key::CTRL));
	ev->set_meta_pressed(input->is_key_pressed(Key::META));

	if (p_delta_y < 0) {
		ev->set_button_index(MouseButton::WHEEL_UP);
	} else if (p_delta_y > 0) {
		ev->set_button_index(MouseButton::WHEEL_DOWN);
	} else if (p_delta_x > 0) {
		ev->set_button_index(MouseButton::WHEEL_LEFT);
	} else if (p_delta_x < 0) {
		ev->set_button_index(MouseButton::WHEEL_RIGHT);
	} else {
		return false;
	}

	// Different browsers give wildly different delta values, and we can't
	// interpret deltaMode, so use default value for wheel events' factor.

	MouseButtonMask button_flag = mouse_button_to_mask(ev->get_button_index());
	BitField<MouseButtonMask> button_mask = input->get_mouse_button_mask();
	button_mask.set_flag(button_flag);

	ev->set_pressed(true);
	ev->set_button_mask(button_mask);
	input->parse_input_event(ev);

	Ref<InputEventMouseButton> release = ev->duplicate();
	release->set_pressed(false);
	release->set_button_mask(input->get_mouse_button_mask());
	input->parse_input_event(release);

	return true;
}

// Touch
void DisplayServerWeb::touch_callback(int p_type, int p_count) {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_touch_callback).call_deferred(p_type, p_count);
		return;
	}
#endif

	_touch_callback(p_type, p_count);
}

void DisplayServerWeb::_touch_callback(int p_type, int p_count) {
	DisplayServerWeb *ds = get_singleton();

	const JSTouchEvent &touch_event = ds->touch_event;
	for (int i = 0; i < p_count; i++) {
		Point2 point(touch_event.coords[i * 2], touch_event.coords[i * 2 + 1]);
		if (p_type == 2) {
			// touchmove
			Ref<InputEventScreenDrag> ev;
			ev.instantiate();
			ev->set_index(touch_event.identifier[i]);
			ev->set_position(point);

			Point2 &prev = ds->touches[i];
			ev->set_relative(ev->get_position() - prev);
			ev->set_relative_screen_position(ev->get_relative());
			prev = ev->get_position();

			Input::get_singleton()->parse_input_event(ev);
		} else {
			// touchstart/touchend
			Ref<InputEventScreenTouch> ev;

			// Resume audio context after input in case autoplay was denied.
			OS_Web::get_singleton()->resume_audio();

			ev.instantiate();
			ev->set_index(touch_event.identifier[i]);
			ev->set_position(point);
			ev->set_pressed(p_type == 0);
			ds->touches[i] = point;

			Input::get_singleton()->parse_input_event(ev);

			// Make sure to flush all events so we can call restricted APIs inside the event.
			Input::get_singleton()->flush_buffered_events();
		}
	}
}

bool DisplayServerWeb::is_touchscreen_available() const {
	return godot_js_display_touchscreen_is_available() || (Input::get_singleton() && Input::get_singleton()->is_emulating_touch_from_mouse());
}

// Virtual Keyboard
void DisplayServerWeb::vk_input_text_callback(const char *p_text, int p_cursor) {
	String text = String::utf8(p_text);

#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_vk_input_text_callback).call_deferred(text, p_cursor);
		return;
	}
#endif

	_vk_input_text_callback(text, p_cursor);
}

void DisplayServerWeb::_vk_input_text_callback(const String &p_text, int p_cursor) {
	DisplayServerWeb *ds = DisplayServerWeb::get_singleton();
	if (!ds || !ds->input_text_callback.is_valid()) {
		return;
	}
	// Call input_text
	ds->input_text_callback.call(p_text);
	// Insert key right to reach position.
	Input *input = Input::get_singleton();
	Ref<InputEventKey> k;
	for (int i = 0; i < p_cursor; i++) {
		k.instantiate();
		k->set_pressed(true);
		k->set_echo(false);
		k->set_keycode(Key::RIGHT);
		input->parse_input_event(k);
		k.instantiate();
		k->set_pressed(false);
		k->set_echo(false);
		k->set_keycode(Key::RIGHT);
		input->parse_input_event(k);
	}
}

void DisplayServerWeb::virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect, VirtualKeyboardType p_type, int p_max_input_length, int p_cursor_start, int p_cursor_end) {
	godot_js_display_vk_show(p_existing_text.utf8().get_data(), p_type, p_cursor_start, p_cursor_end);
}

void DisplayServerWeb::virtual_keyboard_hide() {
	godot_js_display_vk_hide();
}

void DisplayServerWeb::window_blur_callback() {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_window_blur_callback).call_deferred();
		return;
	}
#endif

	_window_blur_callback();
}

void DisplayServerWeb::_window_blur_callback() {
	Input::get_singleton()->release_pressed_events();
}

// Gamepad
void DisplayServerWeb::gamepad_callback(int p_index, int p_connected, const char *p_id, const char *p_guid) {
	String id = p_id;
	String guid = p_guid;

#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_gamepad_callback).call_deferred(p_index, p_connected, id, guid);
		return;
	}
#endif

	_gamepad_callback(p_index, p_connected, id, guid);
}

void DisplayServerWeb::_gamepad_callback(int p_index, int p_connected, const String &p_id, const String &p_guid) {
	Input *input = Input::get_singleton();
	if (p_connected) {
		input->joy_connection_changed(p_index, true, p_id, p_guid);
	} else {
		input->joy_connection_changed(p_index, false, "");
	}
}

// IME.
void DisplayServerWeb::ime_callback(int p_type, const char *p_text) {
	String text = String::utf8(p_text);

#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_ime_callback).call_deferred(p_type, text);
		return;
	}
#endif

	_ime_callback(p_type, text);
}

void DisplayServerWeb::_ime_callback(int p_type, const String &p_text) {
	DisplayServerWeb *ds = get_singleton();
	// Resume audio context after input in case autoplay was denied.
	OS_Web::get_singleton()->resume_audio();

	switch (p_type) {
		case 0: {
			// IME start.
			ds->ime_text = String();
			ds->ime_selection = Vector2i();
			for (int i = ds->key_event_pos - 1; i >= 0; i--) {
				// Delete last raw keydown event from query.
				if (ds->key_event_buffer[i].pressed && ds->key_event_buffer[i].raw) {
					ds->key_event_buffer.remove_at(i);
					ds->key_event_pos--;
					break;
				}
			}
			OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
			ds->ime_started = true;
		} break;
		case 1: {
			// IME update.
			if (ds->ime_active && ds->ime_started) {
				ds->ime_text = p_text;
				ds->ime_selection = Vector2i(ds->ime_text.length(), ds->ime_text.length());
				OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
			}
		} break;
		case 2: {
			// IME commit.
			if (ds->ime_active && ds->ime_started) {
				ds->ime_started = false;

				ds->ime_text = String();
				ds->ime_selection = Vector2i();
				OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);

				String text = p_text;
				for (int i = 0; i < text.length(); i++) {
					DisplayServerWeb::KeyEvent ke;

					ke.pressed = true;
					ke.echo = false;
					ke.raw = false;
					ke.keycode = Key::NONE;
					ke.physical_keycode = Key::NONE;
					ke.key_label = Key::NONE;
					ke.unicode = text[i];
					ke.mod = 0;

					if (ds->key_event_pos >= ds->key_event_buffer.size()) {
						ds->key_event_buffer.resize(1 + ds->key_event_pos);
					}
					ds->key_event_buffer.write[ds->key_event_pos++] = ke;
				}
			}
		} break;
		default:
			break;
	}

	ds->process_keys();
	Input::get_singleton()->flush_buffered_events();
}

void DisplayServerWeb::window_set_ime_active(const bool p_active, WindowID p_window) {
	ime_active = p_active;
	godot_js_set_ime_active(p_active);
}

void DisplayServerWeb::window_set_ime_position(const Point2i &p_pos, WindowID p_window) {
	godot_js_set_ime_position(p_pos.x, p_pos.y);
}

Point2i DisplayServerWeb::ime_get_selection() const {
	return ime_selection;
}

String DisplayServerWeb::ime_get_text() const {
	return ime_text;
}

void DisplayServerWeb::process_joypads() {
	Input *input = Input::get_singleton();
	int32_t pads = godot_js_input_gamepad_sample_count();
	int32_t s_btns_num = 0;
	int32_t s_axes_num = 0;
	int32_t s_standard = 0;
	float s_btns[16];
	float s_axes[10];
	for (int idx = 0; idx < pads; idx++) {
		int err = godot_js_input_gamepad_sample_get(idx, s_btns, &s_btns_num, s_axes, &s_axes_num, &s_standard);
		if (err) {
			continue;
		}
		for (int b = 0; b < s_btns_num; b++) {
			// Buttons 6 and 7 in the standard mapping need to be
			// axis to be handled as JoyAxis::TRIGGER by Godot.
			if (s_standard && (b == 6)) {
				input->joy_axis(idx, JoyAxis::TRIGGER_LEFT, s_btns[b]);
			} else if (s_standard && (b == 7)) {
				input->joy_axis(idx, JoyAxis::TRIGGER_RIGHT, s_btns[b]);
			} else {
				input->joy_button(idx, (JoyButton)b, s_btns[b]);
			}
		}
		for (int a = 0; a < s_axes_num; a++) {
			input->joy_axis(idx, (JoyAxis)a, s_axes[a]);
		}
	}
}

Vector<String> DisplayServerWeb::get_rendering_drivers_func() {
	Vector<String> drivers;
#ifdef GLES3_ENABLED
	drivers.push_back("opengl3");
#endif
	return drivers;
}

// Clipboard
void DisplayServerWeb::update_clipboard_callback(const char *p_text) {
	String text = p_text;

#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_update_clipboard_callback).call_deferred(text);
		return;
	}
#endif

	_update_clipboard_callback(text);
}

void DisplayServerWeb::_update_clipboard_callback(const String &p_text) {
	get_singleton()->clipboard = p_text;
}

void DisplayServerWeb::clipboard_set(const String &p_text) {
	clipboard = p_text;
	int err = godot_js_display_clipboard_set(p_text.utf8().get_data());
	ERR_FAIL_COND_MSG(err, "Clipboard API is not supported.");
}

String DisplayServerWeb::clipboard_get() const {
	godot_js_display_clipboard_get(update_clipboard_callback);
	return clipboard;
}

void DisplayServerWeb::send_window_event_callback(int p_notification) {
#ifdef PROXY_TO_PTHREAD_ENABLED
	if (!Thread::is_main_thread()) {
		callable_mp_static(DisplayServerWeb::_send_window_event_callback).call_deferred(p_notification);
		return;
	}
#endif

	_send_window_event_callback(p_notification);
}

void DisplayServerWeb::_send_window_event_callback(int p_notification) {
	DisplayServerWeb *ds = get_singleton();
	if (!ds) {
		return;
	}
	if (p_notification == DisplayServer::WINDOW_EVENT_MOUSE_ENTER || p_notification == DisplayServer::WINDOW_EVENT_MOUSE_EXIT) {
		ds->cursor_inside_canvas = p_notification == DisplayServer::WINDOW_EVENT_MOUSE_ENTER;
	}
	if (godot_js_is_ime_focused() && (p_notification == DisplayServer::WINDOW_EVENT_FOCUS_IN || p_notification == DisplayServer::WINDOW_EVENT_FOCUS_OUT)) {
		return;
	}
	if (ds->window_event_callback.is_valid()) {
		Variant event = int(p_notification);
		ds->window_event_callback.call(event);
	}
}

void DisplayServerWeb::set_icon(const Ref<Image> &p_icon) {
	if (p_icon.is_valid()) {
		ERR_FAIL_COND(p_icon->get_width() <= 0 || p_icon->get_height() <= 0);

		Ref<Image> icon = p_icon;
		if (icon->is_compressed()) {
			icon = icon->duplicate();
			ERR_FAIL_COND(icon->decompress() != OK);
		}
		if (icon->get_format() != Image::FORMAT_RGBA8) {
			if (icon == p_icon) {
				icon = icon->duplicate();
			}
			icon->convert(Image::FORMAT_RGBA8);
		}

		png_image png_meta;
		memset(&png_meta, 0, sizeof png_meta);
		png_meta.version = PNG_IMAGE_VERSION;
		png_meta.width = icon->get_width();
		png_meta.height = icon->get_height();
		png_meta.format = PNG_FORMAT_RGBA;

		PackedByteArray png;
		size_t len;
		PackedByteArray data = icon->get_data();
		ERR_FAIL_COND(!png_image_write_get_memory_size(png_meta, len, 0, data.ptr(), 0, nullptr));

		png.resize(len);
		ERR_FAIL_COND(!png_image_write_to_memory(&png_meta, png.ptrw(), &len, 0, data.ptr(), 0, nullptr));

		godot_js_display_window_icon_set(png.ptr(), len);
	} else {
		godot_js_display_window_icon_set(nullptr, 0);
	}
}

void DisplayServerWeb::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	Callable cb = get_singleton()->input_event_callback;
	if (cb.is_valid()) {
		cb.call(p_event);
	}
}

DisplayServer *DisplayServerWeb::create_func(const String &p_rendering_driver, WindowMode p_window_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Point2i *p_position, const Size2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	return memnew(DisplayServerWeb(p_rendering_driver, p_window_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, p_context, p_parent_window, r_error));
}

DisplayServerWeb::DisplayServerWeb(const String &p_rendering_driver, WindowMode p_window_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Point2i *p_position, const Size2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	r_error = OK; // Always succeeds for now.

	tts = GLOBAL_GET("audio/general/text_to_speech");
	native_menu = memnew(NativeMenu); // Dummy native menu.

	// Ensure the canvas ID.
	godot_js_config_canvas_id_get(canvas_id, 256);

	// Handle contextmenu, webglcontextlost
	godot_js_display_setup_canvas(p_resolution.x, p_resolution.y, (p_window_mode == WINDOW_MODE_FULLSCREEN || p_window_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN), OS::get_singleton()->is_hidpi_allowed() ? 1 : 0);

	// Check if it's windows.
	swap_cancel_ok = godot_js_display_is_swap_ok_cancel() == 1;

	// Expose method for requesting quit.
	godot_js_os_request_quit_cb(request_quit_callback);

#ifdef GLES3_ENABLED
	bool webgl2_inited = false;
	if (godot_js_display_has_webgl(2)) {
		EmscriptenWebGLContextAttributes attributes;
		emscripten_webgl_init_context_attributes(&attributes);
		attributes.alpha = OS::get_singleton()->is_layered_allowed();
		attributes.antialias = false;
		attributes.majorVersion = 2;
		attributes.explicitSwapControl = true;

		webgl_ctx = emscripten_webgl_create_context(canvas_id, &attributes);
		webgl2_inited = webgl_ctx && emscripten_webgl_make_context_current(webgl_ctx) == EMSCRIPTEN_RESULT_SUCCESS;
	}
	if (webgl2_inited) {
		if (!emscripten_webgl_enable_extension(webgl_ctx, "OVR_multiview2")) {
			print_verbose("Failed to enable WebXR extension.");
		}
		RasterizerGLES3::make_current(false);

	} else {
		OS::get_singleton()->alert(
				"Your browser seems not to support WebGL 2.\n\n"
				"If possible, consider updating your browser version and video card drivers.",
				"Unable to initialize WebGL 2 video driver");
		RasterizerDummy::make_current();
	}
#else
	RasterizerDummy::make_current();
#endif

	// JS Input interface (js/libs/library_godot_input.js)
	godot_js_input_mouse_button_cb(&DisplayServerWeb::mouse_button_callback);
	godot_js_input_mouse_move_cb(&DisplayServerWeb::mouse_move_callback);
	godot_js_input_mouse_wheel_cb(&DisplayServerWeb::mouse_wheel_callback);
	godot_js_input_touch_cb(&DisplayServerWeb::touch_callback, touch_event.identifier, touch_event.coords);
	godot_js_input_key_cb(&DisplayServerWeb::key_callback, key_event.code, key_event.key);
	godot_js_input_paste_cb(&DisplayServerWeb::update_clipboard_callback);
	godot_js_input_drop_files_cb(&DisplayServerWeb::drop_files_js_callback);
	godot_js_input_gamepad_cb(&DisplayServerWeb::gamepad_callback);
	godot_js_set_ime_cb(&DisplayServerWeb::ime_callback, &DisplayServerWeb::key_callback, key_event.code, key_event.key);

	// JS Display interface (js/libs/library_godot_display.js)
	godot_js_display_fullscreen_cb(&DisplayServerWeb::fullscreen_change_callback);
	godot_js_display_window_blur_cb(&DisplayServerWeb::window_blur_callback);
	godot_js_display_notification_cb(&DisplayServerWeb::send_window_event_callback,
			WINDOW_EVENT_MOUSE_ENTER,
			WINDOW_EVENT_MOUSE_EXIT,
			WINDOW_EVENT_FOCUS_IN,
			WINDOW_EVENT_FOCUS_OUT);
	godot_js_display_vk_cb(&DisplayServerWeb::vk_input_text_callback);

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_event);
}

DisplayServerWeb::~DisplayServerWeb() {
	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}
#ifdef GLES3_ENABLED
	if (webgl_ctx) {
		emscripten_webgl_commit_frame();
		emscripten_webgl_destroy_context(webgl_ctx);
	}
#endif
}

bool DisplayServerWeb::has_feature(Feature p_feature) const {
	switch (p_feature) {
#ifndef DISABLE_DEPRECATED
		case FEATURE_GLOBAL_MENU: {
			return (native_menu && native_menu->has_feature(NativeMenu::FEATURE_GLOBAL_MENU));
		} break;
#endif
		//case FEATURE_HIDPI:
		case FEATURE_ICON:
		case FEATURE_CLIPBOARD:
		case FEATURE_CURSOR_SHAPE:
		case FEATURE_CUSTOM_CURSOR_SHAPE:
		case FEATURE_MOUSE:
		case FEATURE_TOUCHSCREEN:
			return true;
		//case FEATURE_MOUSE_WARP:
		//case FEATURE_NATIVE_DIALOG:
		//case FEATURE_NATIVE_DIALOG_INPUT:
		//case FEATURE_NATIVE_DIALOG_FILE:
		//case FEATURE_NATIVE_DIALOG_FILE_EXTRA:
		//case FEATURE_NATIVE_ICON:
		//case FEATURE_WINDOW_TRANSPARENCY:
		//case FEATURE_KEEP_SCREEN_ON:
		//case FEATURE_ORIENTATION:
		case FEATURE_IME:
			// IME does not work with experimental VK support.
			return godot_js_display_vk_available() == 0;
		case FEATURE_VIRTUAL_KEYBOARD:
			return godot_js_display_vk_available() != 0;
		case FEATURE_TEXT_TO_SPEECH:
			return tts && (godot_js_display_tts_available() != 0);
		default:
			return false;
	}
}

void DisplayServerWeb::register_web_driver() {
	register_create_function("web", create_func, get_rendering_drivers_func);
}

String DisplayServerWeb::get_name() const {
	return "web";
}

int DisplayServerWeb::get_screen_count() const {
	return 1;
}

int DisplayServerWeb::get_primary_screen() const {
	return 0;
}

Point2i DisplayServerWeb::screen_get_position(int p_screen) const {
	return Point2i(); // TODO offsetX/Y?
}

Size2i DisplayServerWeb::screen_get_size(int p_screen) const {
	int size[2];
	godot_js_display_screen_size_get(size, size + 1);
	return Size2(size[0], size[1]);
}

Rect2i DisplayServerWeb::screen_get_usable_rect(int p_screen) const {
	int size[2];
	godot_js_display_window_size_get(size, size + 1);
	return Rect2i(0, 0, size[0], size[1]);
}

int DisplayServerWeb::screen_get_dpi(int p_screen) const {
	return godot_js_display_screen_dpi_get();
}

float DisplayServerWeb::screen_get_scale(int p_screen) const {
	return godot_js_display_pixel_ratio_get();
}

float DisplayServerWeb::screen_get_refresh_rate(int p_screen) const {
	return SCREEN_REFRESH_RATE_FALLBACK; // Web doesn't have much of a need for the screen refresh rate, and there's no native way to do so.
}

Vector<DisplayServer::WindowID> DisplayServerWeb::get_window_list() const {
	Vector<WindowID> ret;
	ret.push_back(MAIN_WINDOW_ID);
	return ret;
}

DisplayServerWeb::WindowID DisplayServerWeb::get_window_at_screen_position(const Point2i &p_position) const {
	return MAIN_WINDOW_ID;
}

void DisplayServerWeb::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {
	window_attached_instance_id = p_instance;
}

ObjectID DisplayServerWeb::window_get_attached_instance_id(WindowID p_window) const {
	return window_attached_instance_id;
}

void DisplayServerWeb::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	rect_changed_callback = p_callable;
}

void DisplayServerWeb::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {
	window_event_callback = p_callable;
}

void DisplayServerWeb::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerWeb::window_set_input_text_callback(const Callable &p_callable, WindowID p_window) {
	input_text_callback = p_callable;
}

void DisplayServerWeb::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {
	drop_files_callback = p_callable;
}

void DisplayServerWeb::window_set_title(const String &p_title, WindowID p_window) {
	godot_js_display_window_title_set(p_title.utf8().get_data());
}

int DisplayServerWeb::window_get_current_screen(WindowID p_window) const {
	return 1;
}

void DisplayServerWeb::window_set_current_screen(int p_screen, WindowID p_window) {
	// Not implemented.
}

Point2i DisplayServerWeb::window_get_position(WindowID p_window) const {
	return Point2i();
}

Point2i DisplayServerWeb::window_get_position_with_decorations(WindowID p_window) const {
	return Point2i();
}

void DisplayServerWeb::window_set_position(const Point2i &p_position, WindowID p_window) {
	// Not supported.
}

void DisplayServerWeb::window_set_transient(WindowID p_window, WindowID p_parent) {
	// Not supported.
}

void DisplayServerWeb::window_set_max_size(const Size2i p_size, WindowID p_window) {
	// Not supported.
}

Size2i DisplayServerWeb::window_get_max_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerWeb::window_set_min_size(const Size2i p_size, WindowID p_window) {
	// Not supported.
}

Size2i DisplayServerWeb::window_get_min_size(WindowID p_window) const {
	return Size2i();
}

void DisplayServerWeb::window_set_size(const Size2i p_size, WindowID p_window) {
	godot_js_display_desired_size_set(p_size.x, p_size.y);
}

Size2i DisplayServerWeb::window_get_size(WindowID p_window) const {
	int size[2];
	godot_js_display_window_size_get(size, size + 1);
	return Size2i(size[0], size[1]);
}

Size2i DisplayServerWeb::window_get_size_with_decorations(WindowID p_window) const {
	return window_get_size(p_window);
}

void DisplayServerWeb::window_set_mode(WindowMode p_mode, WindowID p_window) {
	if (window_mode == p_mode) {
		return;
	}

	switch (p_mode) {
		case WINDOW_MODE_WINDOWED: {
			if (window_mode == WINDOW_MODE_FULLSCREEN) {
				godot_js_display_fullscreen_exit();
			}
			window_mode = WINDOW_MODE_WINDOWED;
		} break;
		case WINDOW_MODE_EXCLUSIVE_FULLSCREEN:
		case WINDOW_MODE_FULLSCREEN: {
			int result = godot_js_display_fullscreen_request();
			ERR_FAIL_COND_MSG(result, "The request was denied. Remember that enabling fullscreen is only possible from an input callback for the Web platform.");
		} break;
		case WINDOW_MODE_MAXIMIZED:
		case WINDOW_MODE_MINIMIZED:
			// WindowMode MAXIMIZED and MINIMIZED are not supported in Web platform.
			break;
		default:
			break;
	}
}

DisplayServerWeb::WindowMode DisplayServerWeb::window_get_mode(WindowID p_window) const {
	return window_mode;
}

bool DisplayServerWeb::window_is_maximize_allowed(WindowID p_window) const {
	return false;
}

void DisplayServerWeb::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {
	// Not supported.
}

bool DisplayServerWeb::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	return false;
}

void DisplayServerWeb::window_request_attention(WindowID p_window) {
	// Not supported.
}

void DisplayServerWeb::window_move_to_foreground(WindowID p_window) {
	// Not supported.
}

bool DisplayServerWeb::window_is_focused(WindowID p_window) const {
	return true;
}

bool DisplayServerWeb::window_can_draw(WindowID p_window) const {
	return true;
}

bool DisplayServerWeb::can_any_window_draw() const {
	return true;
}

DisplayServer::VSyncMode DisplayServerWeb::window_get_vsync_mode(WindowID p_vsync_mode) const {
	return DisplayServer::VSYNC_ENABLED;
}

void DisplayServerWeb::process_events() {
	process_keys();
	Input::get_singleton()->flush_buffered_events();
	if (godot_js_input_gamepad_sample() == OK) {
		process_joypads();
	}
}

void DisplayServerWeb::process_keys() {
	for (int i = 0; i < key_event_pos; i++) {
		const DisplayServerWeb::KeyEvent &ke = key_event_buffer[i];

		Ref<InputEventKey> ev;
		ev.instantiate();
		ev->set_pressed(ke.pressed);
		ev->set_echo(ke.echo);
		ev->set_keycode(ke.keycode);
		ev->set_physical_keycode(ke.physical_keycode);
		ev->set_key_label(ke.key_label);
		ev->set_unicode(ke.unicode);
		ev->set_location(ke.location);
		if (ke.raw) {
			dom2godot_mod(ev, ke.mod, ke.keycode);
		}

		Input::get_singleton()->parse_input_event(ev);
	}
	key_event_pos = 0;
}

int DisplayServerWeb::get_current_video_driver() const {
	return 1;
}

bool DisplayServerWeb::get_swap_cancel_ok() {
	return swap_cancel_ok;
}

void DisplayServerWeb::swap_buffers() {
#ifdef GLES3_ENABLED
	if (webgl_ctx) {
		emscripten_webgl_commit_frame();
	}
#endif
}
