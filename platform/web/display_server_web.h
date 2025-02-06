/**************************************************************************/
/*  display_server_web.h                                                  */
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

#include "servers/display_server.h"

#include "godot_js.h"

#include <emscripten.h>
#include <emscripten/html5.h>

class DisplayServerWeb : public DisplayServer {
	// No need to register with GDCLASS, it's platform-specific and nothing is added.

private:
	struct JSTouchEvent {
		uint32_t identifier[32] = { 0 };
		double coords[64] = { 0 };
	};
	JSTouchEvent touch_event;

	struct JSKeyEvent {
		char code[32] = { 0 };
		char key[32] = { 0 };
		uint8_t modifiers[4] = { 0 };
	};
	JSKeyEvent key_event;

#ifdef GLES3_ENABLED
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl_ctx = 0;
#endif

	HashMap<int, CharString> utterance_ids;

	WindowMode window_mode = WINDOW_MODE_WINDOWED;
	ObjectID window_attached_instance_id = {};

	Callable rect_changed_callback;
	Callable window_event_callback;
	Callable input_event_callback;
	Callable input_text_callback;
	Callable drop_files_callback;

	String clipboard;
	Point2 touches[32];

	Array voices;

	char canvas_id[256] = { 0 };
	bool cursor_inside_canvas = true;
	CursorShape cursor_shape = CURSOR_ARROW;
	Point2i last_click_pos = Point2(-100, -100); // TODO check this again.
	uint64_t last_click_ms = 0;
	MouseButton last_click_button_index = MouseButton::NONE;

	bool ime_active = false;
	bool ime_started = false;
	String ime_text;
	Vector2i ime_selection;

	struct KeyEvent {
		bool pressed = false;
		bool echo = false;
		bool raw = false;
		Key keycode = Key::NONE;
		Key physical_keycode = Key::NONE;
		Key key_label = Key::NONE;
		uint32_t unicode = 0;
		KeyLocation location = KeyLocation::UNSPECIFIED;
		int mod = 0;
	};

	Vector<KeyEvent> key_event_buffer;
	int key_event_pos = 0;

	bool swap_cancel_ok = false;
	bool tts = false;
	NativeMenu *native_menu = nullptr;

	MouseMode mouse_mode_base = MOUSE_MODE_VISIBLE;
	MouseMode mouse_mode_override = MOUSE_MODE_VISIBLE;
	bool mouse_mode_override_enabled = false;
	void _mouse_update_mode();

	// utilities
	static void dom2godot_mod(Ref<InputEventWithModifiers> ev, int p_mod, Key p_keycode);
	static const char *godot2dom_cursor(DisplayServer::CursorShape p_shape);

	// events
	WASM_EXPORT static void fullscreen_change_callback(int p_fullscreen);
	static void _fullscreen_change_callback(int p_fullscreen);
	WASM_EXPORT static int mouse_button_callback(int p_pressed, int p_button, double p_x, double p_y, int p_modifiers);
	static int _mouse_button_callback(int p_pressed, int p_button, double p_x, double p_y, int p_modifiers);
	WASM_EXPORT static void mouse_move_callback(double p_x, double p_y, double p_rel_x, double p_rel_y, int p_modifiers);
	static void _mouse_move_callback(double p_x, double p_y, double p_rel_x, double p_rel_y, int p_modifiers);
	WASM_EXPORT static int mouse_wheel_callback(double p_delta_x, double p_delta_y);
	static int _mouse_wheel_callback(double p_delta_x, double p_delta_y);
	WASM_EXPORT static void touch_callback(int p_type, int p_count);
	static void _touch_callback(int p_type, int p_count);
	WASM_EXPORT static void key_callback(int p_pressed, int p_repeat, int p_modifiers);
	static void _key_callback(const String &p_key_event_code, const String &p_key_event_key, int p_pressed, int p_repeat, int p_modifiers);
	WASM_EXPORT static void vk_input_text_callback(const char *p_text, int p_cursor);
	static void _vk_input_text_callback(const String &p_text, int p_cursor);
	WASM_EXPORT static void gamepad_callback(int p_index, int p_connected, const char *p_id, const char *p_guid);
	static void _gamepad_callback(int p_index, int p_connected, const String &p_id, const String &p_guid);
	WASM_EXPORT static void js_utterance_callback(int p_event, int p_id, int p_pos);
	static void _js_utterance_callback(int p_event, int p_id, int p_pos);
	WASM_EXPORT static void ime_callback(int p_type, const char *p_text);
	static void _ime_callback(int p_type, const String &p_text);
	WASM_EXPORT static void request_quit_callback();
	static void _request_quit_callback();
	WASM_EXPORT static void window_blur_callback();
	static void _window_blur_callback();
	WASM_EXPORT static void update_voices_callback(int p_size, const char **p_voice);
	static void _update_voices_callback(const Vector<String> &p_voices);
	WASM_EXPORT static void update_clipboard_callback(const char *p_text);
	static void _update_clipboard_callback(const String &p_text);
	WASM_EXPORT static void send_window_event_callback(int p_notification);
	static void _send_window_event_callback(int p_notification);
	WASM_EXPORT static void drop_files_js_callback(const char **p_filev, int p_filec);
	static void _drop_files_js_callback(const Vector<String> &p_files);

	void process_joypads();
	void process_keys();

	static Vector<String> get_rendering_drivers_func();
	static DisplayServer *create_func(const String &p_rendering_driver, WindowMode p_window_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);

	static void _dispatch_input_event(const Ref<InputEvent> &p_event);

protected:
	int get_current_video_driver() const;

public:
	// Override return type to make writing static callbacks less tedious.
	static DisplayServerWeb *get_singleton();

	// utilities
	bool check_size_force_redraw();

	// from DisplayServer
	virtual bool has_feature(Feature p_feature) const override;
	virtual String get_name() const override;

	// tts
	virtual bool tts_is_speaking() const override;
	virtual bool tts_is_paused() const override;
	virtual TypedArray<Dictionary> tts_get_voices() const override;

	virtual void tts_speak(const String &p_text, const String &p_voice, int p_volume = 50, float p_pitch = 1.f, float p_rate = 1.f, int p_utterance_id = 0, bool p_interrupt = false) override;
	virtual void tts_pause() override;
	virtual void tts_resume() override;
	virtual void tts_stop() override;

	// cursor
	virtual void cursor_set_shape(CursorShape p_shape) override;
	virtual CursorShape cursor_get_shape() const override;
	virtual void cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape = CURSOR_ARROW, const Vector2 &p_hotspot = Vector2()) override;

	// mouse
	virtual void mouse_set_mode(MouseMode p_mode) override;
	virtual MouseMode mouse_get_mode() const override;
	virtual void mouse_set_mode_override(MouseMode p_mode) override;
	virtual MouseMode mouse_get_mode_override() const override;
	virtual void mouse_set_mode_override_enabled(bool p_override_enabled) override;
	virtual bool mouse_is_mode_override_enabled() const override;

	virtual Point2i mouse_get_position() const override;

	// ime
	virtual void window_set_ime_active(const bool p_active, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_ime_position(const Point2i &p_pos, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual Point2i ime_get_selection() const override;
	virtual String ime_get_text() const override;

	// touch
	virtual bool is_touchscreen_available() const override;

	// clipboard
	virtual void clipboard_set(const String &p_text) override;
	virtual String clipboard_get() const override;

	// screen
	virtual int get_screen_count() const override;
	virtual int get_primary_screen() const override;
	virtual Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_scale(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual void screen_set_keep_on(bool p_enable) override {}

	virtual void virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect = Rect2(), VirtualKeyboardType p_type = KEYBOARD_TYPE_DEFAULT, int p_max_input_length = -1, int p_cursor_start = -1, int p_cursor_end = -1) override;
	virtual void virtual_keyboard_hide() override;

	// windows
	virtual Vector<DisplayServer::WindowID> get_window_list() const override;
	virtual WindowID get_window_at_screen_position(const Point2i &p_position) const override;

	virtual void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_title(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual int window_get_current_screen(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_current_screen(int p_screen, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual Point2i window_get_position(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual Point2i window_get_position_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_position(const Point2i &p_position, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_transient(WindowID p_window, WindowID p_parent) override;

	virtual void window_set_max_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_max_size(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_min_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_min_size(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_size(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual Size2i window_get_size_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_mode(WindowMode p_mode, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual WindowMode window_get_mode(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool window_is_maximize_allowed(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual bool window_get_flag(WindowFlags p_flag, WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_request_attention(WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_move_to_foreground(WindowID p_window = MAIN_WINDOW_ID) override;
	virtual bool window_is_focused(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool window_can_draw(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool can_any_window_draw() const override;

	virtual void window_set_vsync_mode(VSyncMode p_vsync_mode, WindowID p_window = MAIN_WINDOW_ID) override {}
	virtual DisplayServer::VSyncMode window_get_vsync_mode(WindowID p_vsync_mode) const override;

	// events
	virtual void process_events() override;

	// icon
	virtual void set_icon(const Ref<Image> &p_icon) override;

	// others
	virtual bool get_swap_cancel_ok() override;
	virtual void swap_buffers() override;

	static void register_web_driver();
	DisplayServerWeb(const String &p_rendering_driver, WindowMode p_window_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Point2i *p_position, const Size2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);
	~DisplayServerWeb();
};
