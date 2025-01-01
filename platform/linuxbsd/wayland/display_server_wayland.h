/**************************************************************************/
/*  display_server_wayland.h                                              */
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

#ifndef DISPLAY_SERVER_WAYLAND_H
#define DISPLAY_SERVER_WAYLAND_H

#ifdef WAYLAND_ENABLED

#include "wayland/wayland_thread.h"

#ifdef RD_ENABLED
#include "servers/rendering/rendering_device.h"

#ifdef VULKAN_ENABLED
#include "wayland/rendering_context_driver_vulkan_wayland.h"
#endif

#endif //RD_ENABLED

#ifdef GLES3_ENABLED
#include "drivers/egl/egl_manager.h"
#endif

#if defined(SPEECHD_ENABLED)
#include "tts_linux.h"
#endif

#ifdef DBUS_ENABLED
#include "freedesktop_portal_desktop.h"
#include "freedesktop_screensaver.h"
#endif

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "servers/display_server.h"

#include <limits.h>
#include <stdio.h>

#undef CursorShape

class DisplayServerWayland : public DisplayServer {
	// No need to register with GDCLASS, it's platform-specific and nothing is added.
	struct WindowData {
		WindowID id;

		Rect2i rect;
		Size2i max_size;
		Size2i min_size;

		Rect2i safe_rect;

#ifdef GLES3_ENABLED
		struct wl_egl_window *wl_egl_window = nullptr;
#endif

		// Flags whether we have allocated a buffer through the video drivers.
		bool visible = false;

		DisplayServer::VSyncMode vsync_mode = VSYNC_ENABLED;

		uint32_t flags = 0;

		DisplayServer::WindowMode mode = WINDOW_MODE_WINDOWED;

		Callable rect_changed_callback;
		Callable window_event_callback;
		Callable input_event_callback;
		Callable drop_files_callback;
		Callable input_text_callback;

		String title;
		ObjectID instance_id;
	};

	struct CustomCursor {
		RID rid;
		Point2i hotspot;
	};

	CursorShape cursor_shape = CURSOR_ARROW;
	DisplayServer::MouseMode mouse_mode = DisplayServer::MOUSE_MODE_VISIBLE;

	HashMap<CursorShape, CustomCursor> custom_cursors;

	WindowData main_window;
	WaylandThread wayland_thread;

	Context context;

	String ime_text;
	Vector2i ime_selection;

	bool suspended = false;
	bool emulate_vsync = false;

	String rendering_driver;

#ifdef RD_ENABLED
	RenderingContextDriver *rendering_context = nullptr;
	RenderingDevice *rendering_device = nullptr;
#endif

#ifdef GLES3_ENABLED
	EGLManager *egl_manager = nullptr;
#endif

#ifdef SPEECHD_ENABLED
	TTS_Linux *tts = nullptr;
#endif
	NativeMenu *native_menu = nullptr;

#if DBUS_ENABLED
	FreeDesktopPortalDesktop *portal_desktop = nullptr;

	FreeDesktopScreenSaver *screensaver = nullptr;
	bool screensaver_inhibited = false;
#endif
	static String _get_app_id_from_context(Context p_context);

	void _send_window_event(WindowEvent p_event);

	static void dispatch_input_events(const Ref<InputEvent> &p_event);
	void _dispatch_input_event(const Ref<InputEvent> &p_event);

	void _resize_window(const Size2i &p_size);

	virtual void _show_window();

	void try_suspend();

public:
	virtual bool has_feature(Feature p_feature) const override;

	virtual String get_name() const override;

#ifdef SPEECHD_ENABLED
	virtual bool tts_is_speaking() const override;
	virtual bool tts_is_paused() const override;
	virtual TypedArray<Dictionary> tts_get_voices() const override;

	virtual void tts_speak(const String &p_text, const String &p_voice, int p_volume = 50, float p_pitch = 1.f, float p_rate = 1.f, int p_utterance_id = 0, bool p_interrupt = false) override;
	virtual void tts_pause() override;
	virtual void tts_resume() override;
	virtual void tts_stop() override;
#endif

#ifdef DBUS_ENABLED
	virtual bool is_dark_mode_supported() const override;
	virtual bool is_dark_mode() const override;
	virtual void set_system_theme_change_callback(const Callable &p_callable) override;

	virtual Error file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback) override;
	virtual Error file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback) override;
#endif

	virtual void beep() const override;

	virtual void mouse_set_mode(MouseMode p_mode) override;
	virtual MouseMode mouse_get_mode() const override;

	virtual void warp_mouse(const Point2i &p_to) override;
	virtual Point2i mouse_get_position() const override;
	virtual BitField<MouseButtonMask> mouse_get_button_state() const override;

	virtual void clipboard_set(const String &p_text) override;
	virtual String clipboard_get() const override;
	virtual Ref<Image> clipboard_get_image() const override;
	virtual void clipboard_set_primary(const String &p_text) override;
	virtual String clipboard_get_primary() const override;

	virtual int get_screen_count() const override;
	virtual int get_primary_screen() const override;
	virtual Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_scale(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;

	virtual void screen_set_keep_on(bool p_enable) override;
	virtual bool screen_is_kept_on() const override;

	virtual Vector<DisplayServer::WindowID> get_window_list() const override;

	virtual int64_t window_get_native_handle(HandleType p_handle_type, WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual WindowID get_window_at_screen_position(const Point2i &p_position) const override;

	virtual void window_attach_instance_id(ObjectID p_instance, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual ObjectID window_get_attached_instance_id(WindowID p_window_id = MAIN_WINDOW_ID) const override;

	virtual void window_set_title(const String &p_title, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual void window_set_mouse_passthrough(const Vector<Vector2> &p_region, WindowID p_window_id = MAIN_WINDOW_ID) override;

	virtual void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual void window_set_window_event_callback(const Callable &p_callable, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual void window_set_input_event_callback(const Callable &p_callable, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual void window_set_input_text_callback(const Callable &p_callable, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window_id = MAIN_WINDOW_ID) override;

	virtual int window_get_current_screen(WindowID p_window_id = MAIN_WINDOW_ID) const override;
	virtual void window_set_current_screen(int p_screen, WindowID p_window_id = MAIN_WINDOW_ID) override;

	virtual Point2i window_get_position(WindowID p_window_id = MAIN_WINDOW_ID) const override;
	virtual Point2i window_get_position_with_decorations(WindowID p_window_id = MAIN_WINDOW_ID) const override;
	virtual void window_set_position(const Point2i &p_position, WindowID p_window_id = MAIN_WINDOW_ID) override;

	virtual void window_set_max_size(const Size2i p_size, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_max_size(WindowID p_window_id = MAIN_WINDOW_ID) const override;
	virtual void gl_window_make_current(DisplayServer::WindowID p_window_id) override;

	virtual void window_set_transient(WindowID p_window_id, WindowID p_parent) override;

	virtual void window_set_min_size(const Size2i p_size, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_min_size(WindowID p_window_id = MAIN_WINDOW_ID) const override;

	virtual void window_set_size(const Size2i p_size, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_size(WindowID p_window_id = MAIN_WINDOW_ID) const override;
	virtual Size2i window_get_size_with_decorations(WindowID p_window_id = MAIN_WINDOW_ID) const override;

	virtual void window_set_mode(WindowMode p_mode, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual WindowMode window_get_mode(WindowID p_window_id = MAIN_WINDOW_ID) const override;

	virtual bool window_is_maximize_allowed(WindowID p_window_id = MAIN_WINDOW_ID) const override;

	virtual void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual bool window_get_flag(WindowFlags p_flag, WindowID p_window_id = MAIN_WINDOW_ID) const override;

	virtual void window_request_attention(WindowID p_window_id = MAIN_WINDOW_ID) override;

	virtual void window_move_to_foreground(WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual bool window_is_focused(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool window_can_draw(WindowID p_window_id = MAIN_WINDOW_ID) const override;

	virtual bool can_any_window_draw() const override;

	virtual void window_set_ime_active(const bool p_active, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual void window_set_ime_position(const Point2i &p_pos, WindowID p_window_id = MAIN_WINDOW_ID) override;

	virtual Point2i ime_get_selection() const override;
	virtual String ime_get_text() const override;

	virtual void window_set_vsync_mode(DisplayServer::VSyncMode p_vsync_mode, WindowID p_window_id = MAIN_WINDOW_ID) override;
	virtual DisplayServer::VSyncMode window_get_vsync_mode(WindowID p_window_id) const override;

	virtual void window_start_drag(WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void cursor_set_shape(CursorShape p_shape) override;
	virtual CursorShape cursor_get_shape() const override;
	virtual void cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) override;

	virtual int keyboard_get_layout_count() const override;
	virtual int keyboard_get_current_layout() const override;
	virtual void keyboard_set_current_layout(int p_index) override;
	virtual String keyboard_get_layout_language(int p_index) const override;
	virtual String keyboard_get_layout_name(int p_index) const override;
	virtual Key keyboard_get_keycode_from_physical(Key p_keycode) const override;

	virtual void process_events() override;

	virtual void release_rendering_thread() override;
	virtual void swap_buffers() override;

	virtual void set_context(Context p_context) override;

	virtual bool is_window_transparency_available() const override;

	static DisplayServer *create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Point2i *p_position, const Size2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);
	static Vector<String> get_rendering_drivers_func();

	static void register_wayland_driver();

	DisplayServerWayland(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i &p_resolution, Context p_context, int64_t p_parent_window, Error &r_error);
	~DisplayServerWayland();
};

#endif // WAYLAND_ENABLED

#endif // DISPLAY_SERVER_WAYLAND_H
