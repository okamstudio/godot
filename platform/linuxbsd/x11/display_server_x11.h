/**************************************************************************/
/*  display_server_x11.h                                                  */
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

#ifndef DISPLAY_SERVER_X11_H
#define DISPLAY_SERVER_X11_H

#ifdef X11_ENABLED

#include "joypad_linux.h"

#include "core/input/input.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/local_vector.h"
#include "drivers/alsa/audio_driver_alsa.h"
#include "drivers/alsamidi/midi_driver_alsamidi.h"
#include "drivers/pulseaudio/audio_driver_pulseaudio.h"
#include "drivers/unix/os_unix.h"
#include "servers/audio_server.h"
#include "servers/display_server.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering_server.h"

#if defined(SPEECHD_ENABLED)
#include "tts_linux.h"
#endif

#if defined(GLES3_ENABLED)
#include "x11/gl_manager_x11.h"
#include "x11/gl_manager_x11_egl.h"
#endif

#if defined(RD_ENABLED)
#include "servers/rendering/rendering_device.h"

#if defined(VULKAN_ENABLED)
#include "x11/rendering_context_driver_vulkan_x11.h"
#endif
#endif

#if defined(DBUS_ENABLED)
#include "freedesktop_portal_desktop.h"
#include "freedesktop_screensaver.h"
#endif

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#ifdef SOWRAP_ENABLED
#include "x11/dynwrappers/xlib-so_wrap.h"

#include "x11/dynwrappers/xcursor-so_wrap.h"
#include "x11/dynwrappers/xext-so_wrap.h"
#include "x11/dynwrappers/xinerama-so_wrap.h"
#include "x11/dynwrappers/xinput2-so_wrap.h"
#include "x11/dynwrappers/xrandr-so_wrap.h"
#include "x11/dynwrappers/xrender-so_wrap.h"

#include "xkbcommon-so_wrap.h"
#else
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

#ifdef XKB_ENABLED
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>
#endif
#endif

typedef struct _xrr_monitor_info {
	Atom name;
	Bool primary = false;
	Bool automatic = false;
	int noutput = 0;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	int mwidth = 0;
	int mheight = 0;
	RROutput *outputs = nullptr;
} xrr_monitor_info;

#undef CursorShape

class DisplayServerX11 : public DisplayServer {
	// No need to register with GDCLASS, it's platform-specific and nothing is added.

	_THREAD_SAFE_CLASS_

	Atom wm_delete;
	Atom xdnd_enter;
	Atom xdnd_position;
	Atom xdnd_status;
	Atom xdnd_action_copy;
	Atom xdnd_drop;
	Atom xdnd_finished;
	Atom xdnd_selection;
	Atom xdnd_aware;
	Atom requested = None;
	int xdnd_version = 5;

#if defined(GLES3_ENABLED)
	GLManager_X11 *gl_manager = nullptr;
	GLManagerEGL_X11 *gl_manager_egl = nullptr;
#endif
#if defined(RD_ENABLED)
	RenderingContextDriver *rendering_context = nullptr;
	RenderingDevice *rendering_device = nullptr;
#endif

#if defined(DBUS_ENABLED)
	FreeDesktopScreenSaver *screensaver = nullptr;
	bool keep_screen_on = false;
#endif

#ifdef SPEECHD_ENABLED
	TTS_Linux *tts = nullptr;
#endif
	NativeMenu *native_menu = nullptr;

#if defined(DBUS_ENABLED)
	FreeDesktopPortalDesktop *portal_desktop = nullptr;
#endif

	struct WindowData {
		Window x11_window;
		Window x11_xim_window;
		Window parent;
		::XIC xic;
		bool ime_active = false;
		bool ime_in_progress = false;
		bool ime_suppress_next_keyup = false;
#ifdef XKB_ENABLED
		xkb_compose_state *xkb_state = nullptr;
#endif

		Size2i min_size;
		Size2i max_size;
		Point2i position;
		Size2i size;
		Callable rect_changed_callback;
		Callable event_callback;
		Callable input_event_callback;
		Callable input_text_callback;
		Callable drop_files_callback;

		Vector<Vector2> mpath;

		WindowID transient_parent = INVALID_WINDOW_ID;
		HashSet<WindowID> transient_children;

		ObjectID instance_id;

		bool no_focus = false;

		//better to guess on the fly, given WM can change it
		//WindowMode mode;
		bool fullscreen = false; //OS can't exit from this mode
		bool exclusive_fullscreen = false;
		bool on_top = false;
		bool borderless = false;
		bool resize_disabled = false;
		Vector2i last_position_before_fs;
		bool focused = true;
		bool minimized = false;
		bool maximized = false;
		bool is_popup = false;
		bool layered_window = false;
		bool mpass = false;

		Window embed_parent = 0;

		Rect2i parent_safe_rect;

		unsigned int focus_order = 0;
	};

	Point2i im_selection;
	String im_text;

#ifdef XKB_ENABLED
	bool xkb_loaded_v05p = false;
	bool xkb_loaded_v08p = false;
	xkb_context *xkb_ctx = nullptr;
	xkb_compose_table *dead_tbl = nullptr;
#endif

	HashMap<WindowID, WindowData> windows;

	unsigned int last_mouse_monitor_mask = 0;
	uint64_t time_since_popup = 0;

	List<WindowID> popup_list;

	WindowID window_mouseover_id = INVALID_WINDOW_ID;
	WindowID last_focused_window = INVALID_WINDOW_ID;

	WindowID window_id_counter = MAIN_WINDOW_ID;
	WindowID _create_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect, Window p_parent_window);

	String internal_clipboard;
	String internal_clipboard_primary;
	Window xdnd_source_window = 0;
	::Display *x11_display;
	char *xmbstring = nullptr;
	int xmblen = 0;
	unsigned long last_timestamp = 0;
	::Time last_keyrelease_time = 0;
	::XIM xim;
	::XIMStyle xim_style;

	static int _xim_preedit_start_callback(::XIM xim, ::XPointer client_data,
			::XPointer call_data);
	static void _xim_preedit_done_callback(::XIM xim, ::XPointer client_data,
			::XPointer call_data);
	static void _xim_preedit_draw_callback(::XIM xim, ::XPointer client_data,
			::XIMPreeditDrawCallbackStruct *call_data);
	static void _xim_preedit_caret_callback(::XIM xim, ::XPointer client_data,
			::XIMPreeditCaretCallbackStruct *call_data);
	static void _xim_destroy_callback(::XIM im, ::XPointer client_data,
			::XPointer call_data);

	Point2i last_mouse_pos;
	bool last_mouse_pos_valid = false;
	Point2i last_click_pos = Point2i(-100, -100);
	uint64_t last_click_ms = 0;
	MouseButton last_click_button_index = MouseButton::NONE;
	bool app_focused = false;
	uint64_t time_since_no_focus = 0;

	struct {
		int opcode;
		Vector<int> touch_devices;
		HashMap<int, Vector2> absolute_devices;
		HashMap<int, Vector2> pen_pressure_range;
		HashMap<int, Vector2> pen_tilt_x_range;
		HashMap<int, Vector2> pen_tilt_y_range;
		HashMap<int, bool> pen_inverted_devices;
		XIEventMask all_event_mask;
		HashMap<int, Vector2> state;
		double pressure;
		bool pressure_supported;
		bool pen_inverted;
		Vector2 tilt;
		Vector2 mouse_pos_to_filter;
		Vector2 relative_motion;
		Vector2 raw_pos;
		Vector2 old_raw_pos;
		::Time last_relative_time;
	} xi;

	bool _refresh_device_info();

	Rect2i _screen_get_rect(int p_screen) const;

	void _get_key_modifier_state(unsigned int p_x11_state, Ref<InputEventWithModifiers> state);
	void _flush_mouse_motion();

	MouseMode mouse_mode = MOUSE_MODE_VISIBLE;
	Point2i center;

	void _handle_key_event(WindowID p_window, XKeyEvent *p_event, LocalVector<XEvent> &p_events, uint32_t &p_event_index, bool p_echo = false);

	Atom _process_selection_request_target(Atom p_target, Window p_requestor, Atom p_property, Atom p_selection) const;
	void _handle_selection_request_event(XSelectionRequestEvent *p_event) const;
	void _update_window_mouse_passthrough(WindowID p_window);

	String _clipboard_get_impl(Atom p_source, Window x11_window, Atom target) const;
	String _clipboard_get(Atom p_source, Window x11_window) const;
	Atom _clipboard_get_image_target(Atom p_source, Window x11_window) const;
	void _clipboard_transfer_ownership(Atom p_source, Window x11_window) const;

	bool do_mouse_warp = false;

	const char *cursor_theme = nullptr;
	int cursor_size = 0;
	XcursorImage *cursor_img[CURSOR_MAX];
	Cursor cursors[CURSOR_MAX];
	Cursor null_cursor;
	CursorShape current_cursor = CURSOR_ARROW;
	HashMap<CursorShape, Vector<Variant>> cursors_cache;

	String rendering_driver;
	void set_wm_fullscreen(bool p_enabled);
	void set_wm_above(bool p_enabled);

	typedef xrr_monitor_info *(*xrr_get_monitors_t)(Display *dpy, Window window, Bool get_active, int *nmonitors);
	typedef void (*xrr_free_monitors_t)(xrr_monitor_info *monitors);
	xrr_get_monitors_t xrr_get_monitors = nullptr;
	xrr_free_monitors_t xrr_free_monitors = nullptr;
	void *xrandr_handle = nullptr;
	bool xrandr_ext_ok = true;
	bool xinerama_ext_ok = true;
	bool xshaped_ext_ok = true;
	bool xwayland = false;

	struct Property {
		unsigned char *data;
		int format, nitems;
		Atom type;
	};
	static Property _read_property(Display *p_display, Window p_window, Atom p_property);

	void _update_real_mouse_position(const WindowData &wd);
	bool _window_maximize_check(WindowID p_window, const char *p_atom_name) const;
	bool _window_fullscreen_check(WindowID p_window) const;
	bool _window_minimize_check(WindowID p_window) const;
	void _validate_mode_on_map(WindowID p_window);
	void _update_size_hints(WindowID p_window);
	void _set_wm_fullscreen(WindowID p_window, bool p_enabled, bool p_exclusive);
	void _set_wm_maximized(WindowID p_window, bool p_enabled);
	void _set_wm_minimized(WindowID p_window, bool p_enabled);

	void _update_context(WindowData &wd);

	Context context = CONTEXT_ENGINE;

	WindowID _get_focused_window_or_popup() const;
	bool _window_focus_check();

	void _send_window_event(const WindowData &wd, WindowEvent p_event);
	static void _dispatch_input_events(const Ref<InputEvent> &p_event);
	void _dispatch_input_event(const Ref<InputEvent> &p_event);
	void _set_input_focus(Window p_window, int p_revert_to);

	mutable Mutex events_mutex;
	Thread events_thread;
	SafeFlag events_thread_done;
	LocalVector<XEvent> polled_events;
	static void _poll_events_thread(void *ud);
	bool _wait_for_events() const;
	void _poll_events();
	void _check_pending_events(LocalVector<XEvent> &r_events);

	static Bool _predicate_all_events(Display *display, XEvent *event, XPointer arg);
	static Bool _predicate_clipboard_selection(Display *display, XEvent *event, XPointer arg);
	static Bool _predicate_clipboard_incr(Display *display, XEvent *event, XPointer arg);
	static Bool _predicate_clipboard_save_targets(Display *display, XEvent *event, XPointer arg);

	struct EmbeddedProcessData {
		Window process_window = 0;
		bool visible = true;
	};
	HashMap<OS::ProcessID, EmbeddedProcessData *> embedded_processes;

	Point2i _get_window_position(Window p_window) const;
	Rect2i _get_window_rect(Window p_window) const;
	void _set_external_window_settings(Window p_window, Window p_parent_transient, WindowMode p_mode, uint32_t p_flags, const Rect2i &p_rect);
	void _set_window_taskbar_pager_enabled(Window p_window, bool p_enabled);
	Rect2i _screens_get_full_rect() const;

protected:
	void _window_changed(XEvent *event);

public:
	bool mouse_process_popups();
	void popup_open(WindowID p_window);
	void popup_close(WindowID p_window);

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

#if defined(DBUS_ENABLED)
	virtual bool is_dark_mode_supported() const override;
	virtual bool is_dark_mode() const override;
	virtual void set_system_theme_change_callback(const Callable &p_callable) override;

	virtual Error file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback) override;
	virtual Error file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback) override;
#endif

	virtual void beep() const override;

	virtual void mouse_set_mode(MouseMode p_mode) override;
	virtual MouseMode mouse_get_mode() const override;

	virtual void warp_mouse(const Point2i &p_position) override;
	virtual Point2i mouse_get_position() const override;
	virtual BitField<MouseButtonMask> mouse_get_button_state() const override;

	virtual void clipboard_set(const String &p_text) override;
	virtual String clipboard_get() const override;
	virtual Ref<Image> clipboard_get_image() const override;
	virtual bool clipboard_has_image() const override;
	virtual void clipboard_set_primary(const String &p_text) override;
	virtual String clipboard_get_primary() const override;

	virtual int get_screen_count() const override;
	virtual int get_primary_screen() const override;
	virtual int get_keyboard_focus_screen() const override;
	virtual Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Color screen_get_pixel(const Point2i &p_position) const override;
	virtual Ref<Image> screen_get_image(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;

#if defined(DBUS_ENABLED)
	virtual void screen_set_keep_on(bool p_enable) override;
	virtual bool screen_is_kept_on() const override;
#endif

	virtual Vector<DisplayServer::WindowID> get_window_list() const override;

	virtual WindowID create_sub_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect = Rect2i(), bool p_exclusive = false, WindowID p_transient_parent = INVALID_WINDOW_ID) override;
	virtual void show_window(WindowID p_id) override;
	virtual void delete_sub_window(WindowID p_id) override;

	virtual WindowID window_get_active_popup() const override;
	virtual void window_set_popup_safe_rect(WindowID p_window, const Rect2i &p_rect) override;
	virtual Rect2i window_get_popup_safe_rect(WindowID p_window) const override;

	virtual WindowID get_window_at_screen_position(const Point2i &p_position) const override;

	virtual int64_t window_get_native_handle(HandleType p_handle_type, WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_title(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_mouse_passthrough(const Vector<Vector2> &p_region, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual int window_get_current_screen(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_current_screen(int p_screen, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual Point2i window_get_position(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual Point2i window_get_position_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_position(const Point2i &p_position, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_max_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_max_size(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void gl_window_make_current(DisplayServer::WindowID p_window_id) override;

	virtual void window_set_transient(WindowID p_window, WindowID p_parent) override;

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

	virtual WindowID get_focused_window() const override;

	virtual bool window_can_draw(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool can_any_window_draw() const override;

	virtual void window_set_ime_active(const bool p_active, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_ime_position(const Point2i &p_pos, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual Point2i ime_get_selection() const override;
	virtual String ime_get_text() const override;

	virtual void window_set_vsync_mode(DisplayServer::VSyncMode p_vsync_mode, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual DisplayServer::VSyncMode window_get_vsync_mode(WindowID p_vsync_mode) const override;

	virtual void window_start_drag(WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_start_resize(WindowResizeEdge p_edge, WindowID p_window) override;

	virtual Error embed_process(WindowID p_window, OS::ProcessID p_pid, const Rect2i &p_rect, bool p_visible, bool p_grab_focus) override;
	virtual Error remove_embedded_process(OS::ProcessID p_pid) override;
	virtual OS::ProcessID get_focused_process_id() override;

	virtual void cursor_set_shape(CursorShape p_shape) override;
	virtual CursorShape cursor_get_shape() const override;
	virtual void cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) override;

	virtual int keyboard_get_layout_count() const override;
	virtual int keyboard_get_current_layout() const override;
	virtual void keyboard_set_current_layout(int p_index) override;
	virtual String keyboard_get_layout_language(int p_index) const override;
	virtual String keyboard_get_layout_name(int p_index) const override;
	virtual Key keyboard_get_keycode_from_physical(Key p_keycode) const override;
	virtual Key keyboard_get_label_from_physical(Key p_keycode) const override;

	virtual void process_events() override;

	virtual void release_rendering_thread() override;
	virtual void swap_buffers() override;

	virtual void set_context(Context p_context) override;

	virtual bool is_window_transparency_available() const override;

	virtual void set_native_icon(const String &p_filename) override;
	virtual void set_icon(const Ref<Image> &p_icon) override;

	static DisplayServer *create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);
	static Vector<String> get_rendering_drivers_func();

	static void register_x11_driver();

	DisplayServerX11(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);
	~DisplayServerX11();
};

#endif // X11_ENABLED

#endif // DISPLAY_SERVER_X11_H
