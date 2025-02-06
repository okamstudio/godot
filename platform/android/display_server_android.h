/**************************************************************************/
/*  display_server_android.h                                              */
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

#ifndef DISPLAY_SERVER_ANDROID_H
#define DISPLAY_SERVER_ANDROID_H

#include "servers/display_server.h"

#if defined(RD_ENABLED)
class RenderingContextDriver;
class RenderingDevice;
#endif

class DisplayServerAndroid : public DisplayServer {
	// No need to register with GDCLASS, it's platform-specific and nothing is added.

	String rendering_driver;

	// https://developer.android.com/reference/android/view/PointerIcon
	// mapping between Godot's cursor shape to Android's'
	int android_cursors[CURSOR_MAX] = {
		1000, //CURSOR_ARROW
		1008, //CURSOR_IBEAM
		1002, //CURSOR_POINTIN
		1007, //CURSOR_CROSS
		1004, //CURSOR_WAIT
		1004, //CURSOR_BUSY
		1021, //CURSOR_DRAG
		1021, //CURSOR_CAN_DRO
		1000, //CURSOR_FORBIDD (no corresponding icon in Android's icon so fallback to default)
		1015, //CURSOR_VSIZE
		1014, //CURSOR_HSIZE
		1017, //CURSOR_BDIAGSI
		1016, //CURSOR_FDIAGSI
		1020, //CURSOR_MOVE
		1015, //CURSOR_VSPLIT
		1014, //CURSOR_HSPLIT
		1003, //CURSOR_HELP
	};
	const int CURSOR_TYPE_NULL = 0;
	MouseMode mouse_mode = MouseMode::MOUSE_MODE_VISIBLE;
	MouseMode mouse_mode_base = MouseMode::MOUSE_MODE_VISIBLE;
	MouseMode mouse_mode_override = MouseMode::MOUSE_MODE_VISIBLE;
	bool mouse_mode_override_enabled = false;
	void _mouse_update_mode();

	bool keep_screen_on;
	bool swap_buffers_flag;

	CursorShape cursor_shape = CursorShape::CURSOR_ARROW;

#if defined(RD_ENABLED)
	RenderingContextDriver *rendering_context = nullptr;
	RenderingDevice *rendering_device = nullptr;
#endif
	NativeMenu *native_menu = nullptr;

	ObjectID window_attached_instance_id;

	Callable window_event_callback;
	Callable input_event_callback;
	Callable input_text_callback;
	Callable rect_changed_callback;

	Callable system_theme_changed;

	Callable dialog_callback;
	Callable input_dialog_callback;

	Callable file_picker_callback;

	void _window_callback(const Callable &p_callable, const Variant &p_arg, bool p_deferred = false) const;

	static void _dispatch_input_events(const Ref<InputEvent> &p_event);

public:
	static DisplayServerAndroid *get_singleton();

	virtual bool has_feature(Feature p_feature) const override;
	virtual String get_name() const override;

	virtual bool tts_is_speaking() const override;
	virtual bool tts_is_paused() const override;
	virtual TypedArray<Dictionary> tts_get_voices() const override;

	virtual void tts_speak(const String &p_text, const String &p_voice, int p_volume = 50, float p_pitch = 1.f, float p_rate = 1.f, int p_utterance_id = 0, bool p_interrupt = false) override;
	virtual void tts_pause() override;
	virtual void tts_resume() override;
	virtual void tts_stop() override;

	void emit_system_theme_changed();

	virtual bool is_dark_mode_supported() const override;
	virtual bool is_dark_mode() const override;
	virtual void set_system_theme_change_callback(const Callable &p_callable) override;

	virtual void clipboard_set(const String &p_text) override;
	virtual String clipboard_get() const override;
	virtual bool clipboard_has() const override;

	virtual Error dialog_show(String p_title, String p_description, Vector<String> p_buttons, const Callable &p_callback) override;
	void emit_dialog_callback(int p_button_index);

	virtual Error dialog_input_text(String p_title, String p_description, String p_partial, const Callable &p_callback) override;
	void emit_input_dialog_callback(String p_text);

	virtual Error file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, const FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback, WindowID p_window_id) override;
	void emit_file_picker_callback(bool p_ok, const Vector<String> &p_selected_paths);

	virtual Color get_accent_color() const override;
	virtual Color get_base_color() const override;

	virtual TypedArray<Rect2> get_display_cutouts() const override;
	virtual Rect2i get_display_safe_area() const override;

	virtual void screen_set_keep_on(bool p_enable) override;
	virtual bool screen_is_kept_on() const override;

	virtual void screen_set_orientation(ScreenOrientation p_orientation, int p_screen = SCREEN_OF_MAIN_WINDOW) override;
	virtual ScreenOrientation screen_get_orientation(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;

	virtual int get_screen_count() const override;
	virtual int get_primary_screen() const override;
	virtual Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_scale(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual bool is_touchscreen_available() const override;

	virtual void virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect = Rect2(), VirtualKeyboardType p_type = KEYBOARD_TYPE_DEFAULT, int p_max_length = -1, int p_cursor_start = -1, int p_cursor_end = -1) override;
	virtual void virtual_keyboard_hide() override;
	virtual int virtual_keyboard_get_height() const override;
	virtual bool has_hardware_keyboard() const override;

	virtual void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	void send_window_event(WindowEvent p_event, bool p_deferred = false) const;
	void send_input_event(const Ref<InputEvent> &p_event) const;
	void send_input_text(const String &p_text) const;

	virtual Vector<WindowID> get_window_list() const override;
	virtual WindowID get_window_at_screen_position(const Point2i &p_position) const override;

	virtual int64_t window_get_native_handle(HandleType p_handle_type, WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const override;
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

	virtual void window_set_vsync_mode(DisplayServer::VSyncMode p_vsync_mode, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual DisplayServer::VSyncMode window_get_vsync_mode(WindowID p_vsync_mode) const override;

	virtual void process_events() override;

	void process_accelerometer(const Vector3 &p_accelerometer);
	void process_gravity(const Vector3 &p_gravity);
	void process_magnetometer(const Vector3 &p_magnetometer);
	void process_gyroscope(const Vector3 &p_gyroscope);

	void _cursor_set_shape_helper(CursorShape p_shape, bool force = false);
	virtual void cursor_set_shape(CursorShape p_shape) override;
	virtual CursorShape cursor_get_shape() const override;
	virtual void cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape = CURSOR_ARROW, const Vector2 &p_hotspot = Vector2()) override;

	virtual void mouse_set_mode(MouseMode p_mode) override;
	virtual MouseMode mouse_get_mode() const override;
	virtual void mouse_set_mode_override(MouseMode p_mode) override;
	virtual MouseMode mouse_get_mode_override() const override;
	virtual void mouse_set_mode_override_enabled(bool p_override_enabled) override;
	virtual bool mouse_is_mode_override_enabled() const override;

	static DisplayServer *create_func(const String &p_rendering_driver, WindowMode p_mode, DisplayServer::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);
	static Vector<String> get_rendering_drivers_func();
	static void register_android_driver();

	void reset_window();
	void notify_surface_changed(int p_width, int p_height);

	virtual Point2i mouse_get_position() const override;
	virtual BitField<MouseButtonMask> mouse_get_button_state() const override;

	void reset_swap_buffers_flag();
	bool should_swap_buffers() const;
	virtual void swap_buffers() override;

	virtual void set_native_icon(const String &p_filename) override;
	virtual void set_icon(const Ref<Image> &p_icon) override;

	DisplayServerAndroid(const String &p_rendering_driver, WindowMode p_mode, DisplayServer::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);
	~DisplayServerAndroid();
};

#endif // DISPLAY_SERVER_ANDROID_H
