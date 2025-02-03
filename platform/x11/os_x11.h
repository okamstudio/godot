/**************************************************************************/
/*  os_x11.h                                                              */
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

#ifndef OS_X11_H
#define OS_X11_H

#include "context_gl_x11.h"
#include "core/local_vector.h"
#include "core/os/input.h"
#include "crash_handler_x11.h"
#include "drivers/alsa/audio_driver_alsa.h"
#include "drivers/alsamidi/midi_driver_alsamidi.h"
#include "drivers/pulseaudio/audio_driver_pulseaudio.h"
#include "drivers/unix/os_unix.h"
#include "joypad_linux.h"
#include "main/input_default.h"
#include "power_x11.h"
#include "servers/audio_server.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual_server.h"

#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>

#if defined(SPEECHD_ENABLED)
#include "tts_linux.h"
#endif

// Hints for X11 fullscreen
typedef struct {
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long inputMode;
	unsigned long status;
} Hints;

typedef struct _xrr_monitor_info {
	Atom name;
	Bool primary;
	Bool automatic;
	int noutput;
	int x;
	int y;
	int width;
	int height;
	int mwidth;
	int mheight;
	RROutput *outputs;
} xrr_monitor_info;

#undef CursorShape

class OS_X11 : public OS_Unix {
	Atom wm_delete;
	Atom xdnd_enter;
	Atom xdnd_position;
	Atom xdnd_status;
	Atom xdnd_action_copy;
	Atom xdnd_drop;
	Atom xdnd_finished;
	Atom xdnd_selection;
	Atom requested;

	int xdnd_version;

#if defined(OPENGL_ENABLED)
	ContextGL_X11 *context_gl;
#endif
	//Rasterizer *rasterizer;
	VisualServer *visual_server;
	VideoMode current_videomode;
	List<String> args;
	Window x11_window;
	Window xdnd_source_window;
	MainLoop *main_loop;
	::Display *x11_display;
	int context;
	char *xmbstring;
	int xmblen;
	unsigned long last_timestamp;
	::Time last_keyrelease_time;
	::XIC xic;
	::XIM xim;
	::XIMStyle xim_style;
	static void xim_destroy_callback(::XIM im, ::XPointer client_data,
			::XPointer call_data);

	// IME
	bool im_active;
	Vector2 im_position;
	Vector2 last_position_before_fs;

	Size2 min_size;
	Size2 max_size;

	Point2 last_mouse_pos;
	bool last_mouse_pos_valid;
	Point2i last_click_pos;
	uint64_t last_click_ms;
	int last_click_button_index;
	uint32_t last_button_state;

	struct {
		int opcode;
		Vector<int> touch_devices;
		Map<int, Vector2> absolute_devices;
		Map<int, Vector2> pen_pressure_range;
		Map<int, Vector2> pen_tilt_x_range;
		Map<int, Vector2> pen_tilt_y_range;
		Map<int, bool> pen_inverted_devices;
		XIEventMask all_event_mask;
		XIEventMask all_master_event_mask;
		Map<int, Vector2> state;
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

	bool refresh_device_info();

	unsigned int get_mouse_button_state(unsigned int p_x11_button, int p_x11_type);
	void get_key_modifier_state(unsigned int p_x11_state, Ref<InputEventWithModifiers> state);
	void flush_mouse_motion();

	MouseMode mouse_mode;
	Point2i center;

	void _handle_key_event(XKeyEvent *p_event, LocalVector<XEvent> &p_events, uint32_t &p_event_index, bool p_echo = false);

	Atom _process_selection_request_target(Atom p_target, Window p_requestor, Atom p_property, Atom p_selection) const;
	void _handle_selection_request_event(XSelectionRequestEvent *p_event) const;

	String _get_clipboard_impl(Atom p_source, Window x11_window, Atom target) const;
	String _get_clipboard(Atom p_source, Window x11_window) const;
	void _clipboard_transfer_ownership(Atom p_source, Window x11_window) const;

	mutable Mutex events_mutex;
	Thread events_thread;
	bool events_thread_done = false;
	LocalVector<XEvent> polled_events;
	static void _poll_events_thread(void *ud);
	bool _wait_for_events() const;
	void _poll_events();
	void _check_pending_events(LocalVector<XEvent> &r_events);

	static Bool _predicate_all_events(Display *display, XEvent *event, XPointer arg);
	static Bool _predicate_clipboard_selection(Display *display, XEvent *event, XPointer arg);
	static Bool _predicate_clipboard_incr(Display *display, XEvent *event, XPointer arg);
	static Bool _predicate_clipboard_save_targets(Display *display, XEvent *event, XPointer arg);

	void process_xevents();
	virtual void delete_main_loop();

	bool force_quit;
	bool minimized;
	bool window_has_focus;
	bool do_mouse_warp;

	const char *cursor_theme;
	int cursor_size;
	XcursorImage *img[CURSOR_MAX];
	Cursor cursors[CURSOR_MAX];
	Cursor null_cursor;
	CursorShape current_cursor;
	Map<CursorShape, Vector<Variant>> cursors_cache;

	InputDefault *input;

#ifdef JOYDEV_ENABLED
	JoypadLinux *joypad;
#endif

#ifdef ALSA_ENABLED
	AudioDriverALSA driver_alsa;
#endif

#ifdef ALSAMIDI_ENABLED
	MIDIDriverALSAMidi driver_alsamidi;
#endif

#ifdef PULSEAUDIO_ENABLED
	AudioDriverPulseAudio driver_pulseaudio;
#endif

#ifdef SPEECHD_ENABLED
	TTS_Linux *tts = nullptr;
#endif

	PowerX11 *power_manager;

	bool layered_window;

	CrashHandler crash_handler;

	int video_driver_index;
	bool maximized;
	bool window_focused;
	//void set_wm_border(bool p_enabled);
	void set_wm_fullscreen(bool p_enabled);
	void set_wm_above(bool p_enabled);

	typedef xrr_monitor_info *(*xrr_get_monitors_t)(Display *dpy, Window window, Bool get_active, int *nmonitors);
	typedef void (*xrr_free_monitors_t)(xrr_monitor_info *monitors);
	xrr_get_monitors_t xrr_get_monitors;
	xrr_free_monitors_t xrr_free_monitors;
	void *xrandr_handle;
	Bool xrandr_ext_ok;

protected:
	virtual int get_current_video_driver() const;

	virtual void initialize_core();
	virtual Error initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver);
	virtual void finalize();

	virtual bool is_offscreen_gl_available() const;
	virtual void set_offscreen_gl_current(bool p_current);

	virtual void set_main_loop(MainLoop *p_main_loop);

	void _window_changed(XEvent *event);

	bool window_maximize_check(const char *p_atom_name) const;
	bool window_fullscreen_check() const;
	bool is_window_maximize_allowed() const;

public:
	virtual String get_name() const;

#ifdef SPEECHD_ENABLED
	virtual bool tts_is_speaking() const;
	virtual bool tts_is_paused() const;
	virtual Array tts_get_voices() const;

	virtual void tts_speak(const String &p_text, const String &p_voice, int p_volume = 50, float p_pitch = 1.f, float p_rate = 1.f, int p_utterance_id = 0, bool p_interrupt = false);
	virtual void tts_pause();
	virtual void tts_resume();
	virtual void tts_stop();
#endif

	virtual void set_cursor_shape(CursorShape p_shape);
	virtual CursorShape get_cursor_shape() const;
	virtual void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot);

	void set_mouse_mode(MouseMode p_mode);
	MouseMode get_mouse_mode() const;

	virtual void warp_mouse_position(const Point2 &p_to);
	virtual Point2 get_mouse_position() const;
	virtual int get_mouse_button_state() const;
	virtual void set_window_title(const String &p_title);
	virtual void set_window_mouse_passthrough(const PoolVector2Array &p_region);

	virtual void set_icon(const Ref<Image> &p_icon);

	virtual MainLoop *get_main_loop() const;

	virtual uint64_t get_embedded_pck_offset() const;

	virtual bool can_draw() const;

	virtual void set_clipboard(const String &p_text);
	virtual String get_clipboard() const;
	virtual void set_clipboard_primary(const String &p_text);
	virtual String get_clipboard_primary() const;

	virtual void release_rendering_thread();
	virtual void make_rendering_thread();
	virtual void swap_buffers();

	virtual String get_config_path() const;
	virtual String get_data_path() const;
	virtual String get_cache_path() const;

	virtual String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const;

	virtual Error shell_open(String p_uri);

	virtual void set_video_mode(const VideoMode &p_video_mode, int p_screen = 0);
	virtual VideoMode get_video_mode(int p_screen = 0) const;
	virtual void get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen = 0) const;

	virtual int get_screen_count() const;
	virtual int get_current_screen() const;
	virtual void set_current_screen(int p_screen);
	virtual Point2 get_screen_position(int p_screen = -1) const;
	virtual Size2 get_screen_size(int p_screen = -1) const;
	virtual int get_screen_dpi(int p_screen = -1) const;
	virtual float get_screen_refresh_rate(int p_screen = -1) const;
	virtual Point2 get_window_position() const;
	virtual void set_window_position(const Point2 &p_position);
	virtual Size2 get_window_size() const;
	virtual Size2 get_real_window_size() const;
	virtual Size2 get_max_window_size() const;
	virtual Size2 get_min_window_size() const;
	virtual void set_min_window_size(const Size2 p_size);
	virtual void set_max_window_size(const Size2 p_size);
	virtual void set_window_size(const Size2 p_size);
	virtual void set_window_fullscreen(bool p_enabled);
	virtual bool is_window_fullscreen() const;
	virtual void set_window_resizable(bool p_enabled);
	virtual bool is_window_resizable() const;
	virtual void set_window_minimized(bool p_enabled);
	virtual bool is_window_minimized() const;
	virtual void set_window_maximized(bool p_enabled);
	virtual bool is_window_maximized() const;
	virtual void set_window_always_on_top(bool p_enabled);
	virtual bool is_window_always_on_top() const;
	virtual bool is_window_focused() const;
	virtual void request_attention();
	virtual void *get_native_handle(int p_handle_type);

	virtual void set_borderless_window(bool p_borderless);
	virtual bool get_borderless_window();

	virtual bool get_window_per_pixel_transparency_enabled() const;
	virtual void set_window_per_pixel_transparency_enabled(bool p_enabled);

	virtual void set_ime_active(const bool p_active);
	virtual void set_ime_position(const Point2 &p_pos);

	virtual String get_unique_id() const;
	virtual String get_processor_name() const;

	virtual void move_window_to_foreground();
	virtual void alert(const String &p_alert, const String &p_title = "ALERT!");

	virtual bool is_joy_known(int p_device);
	virtual String get_joy_guid(int p_device) const;

	virtual void set_context(int p_context);

	virtual void _set_use_vsync(bool p_enable);
	//virtual bool is_vsync_enabled() const;

	virtual OS::PowerState get_power_state();
	virtual int get_power_seconds_left();
	virtual int get_power_percent_left();

	virtual bool _check_internal_feature_support(const String &p_feature);

	virtual void force_process_input();
	void run();

	void disable_crash_handler();
	bool is_disable_crash_handler() const;

	virtual Error move_to_trash(const String &p_path);

	virtual LatinKeyboardVariant get_latin_keyboard_variant() const;
	virtual int keyboard_get_layout_count() const;
	virtual int keyboard_get_current_layout() const;
	virtual void keyboard_set_current_layout(int p_index);
	virtual String keyboard_get_layout_language(int p_index) const;
	virtual String keyboard_get_layout_name(int p_index) const;
	virtual uint32_t keyboard_get_scancode_from_physical(uint32_t p_scancode) const;

	void update_real_mouse_position();
	OS_X11();
};

#endif // OS_X11_H
