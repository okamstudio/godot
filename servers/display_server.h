/**************************************************************************/
/*  display_server.h                                                      */
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

#ifndef DISPLAY_SERVER_H
#define DISPLAY_SERVER_H

#include "core/input/input.h"
#include "core/io/image.h"
#include "core/io/resource.h"
#include "core/os/os.h"
#include "core/variant/callable.h"

#include "display/native_menu.h"

class Texture2D;

class DisplayServer : public Object {
	GDCLASS(DisplayServer, Object)

	static DisplayServer *singleton;
	static bool hidpi_allowed;

#ifndef DISABLE_DEPRECATED
	mutable HashMap<String, RID> menu_names;

	RID _get_rid_from_name(NativeMenu *p_nmenu, const String &p_menu_root) const;
#endif

	LocalVector<ObjectID> additional_outputs;

public:
	_FORCE_INLINE_ static DisplayServer *get_singleton() {
		return singleton;
	}

	enum WindowMode {
		WINDOW_MODE_WINDOWED,
		WINDOW_MODE_MINIMIZED,
		WINDOW_MODE_MAXIMIZED,
		WINDOW_MODE_FULLSCREEN,
		WINDOW_MODE_EXCLUSIVE_FULLSCREEN,
	};

	// Keep the VSyncMode enum values in sync with the `display/window/vsync/vsync_mode`
	// project setting hint.
	enum VSyncMode {
		VSYNC_DISABLED,
		VSYNC_ENABLED,
		VSYNC_ADAPTIVE,
		VSYNC_MAILBOX
	};

	enum HandleType {
		DISPLAY_HANDLE,
		WINDOW_HANDLE,
		WINDOW_VIEW,
		OPENGL_CONTEXT,
		EGL_DISPLAY,
		EGL_CONFIG,
	};

	enum Context {
		CONTEXT_EDITOR,
		CONTEXT_PROJECTMAN,
		CONTEXT_ENGINE,
	};

	typedef DisplayServer *(*CreateFunction)(const String &, WindowMode, VSyncMode, uint32_t, const Point2i *, const Size2i &, int p_screen, Context, Error &r_error);
	typedef Vector<String> (*GetRenderingDriversFunction)();

private:
	static void _input_set_mouse_mode(Input::MouseMode p_mode);
	static Input::MouseMode _input_get_mouse_mode();
	static void _input_warp(const Vector2 &p_to_pos);
	static Input::CursorShape _input_get_current_cursor_shape();
	static void _input_set_custom_mouse_cursor_func(const Ref<Resource> &, Input::CursorShape, const Vector2 &p_hostspot);

protected:
	static void _bind_methods();

	static Ref<Image> _get_cursor_image_from_resource(const Ref<Resource> &p_cursor, const Vector2 &p_hotspot);

	enum {
		MAX_SERVERS = 64
	};

	struct DisplayServerCreate {
		const char *name;
		CreateFunction create_function;
		GetRenderingDriversFunction get_rendering_drivers_function;
	};

	static DisplayServerCreate server_create_functions[MAX_SERVERS];
	static int server_create_count;

	friend class RendererViewport;

public:
	enum Feature {
#ifndef DISABLE_DEPRECATED
		FEATURE_GLOBAL_MENU,
#endif
		FEATURE_SUBWINDOWS,
		FEATURE_TOUCHSCREEN,
		FEATURE_MOUSE,
		FEATURE_MOUSE_WARP,
		FEATURE_CLIPBOARD,
		FEATURE_VIRTUAL_KEYBOARD,
		FEATURE_CURSOR_SHAPE,
		FEATURE_CUSTOM_CURSOR_SHAPE,
		FEATURE_NATIVE_DIALOG,
		FEATURE_IME,
		FEATURE_WINDOW_TRANSPARENCY,
		FEATURE_HIDPI,
		FEATURE_ICON,
		FEATURE_NATIVE_ICON,
		FEATURE_ORIENTATION,
		FEATURE_SWAP_BUFFERS,
		FEATURE_KEEP_SCREEN_ON,
		FEATURE_CLIPBOARD_PRIMARY,
		FEATURE_TEXT_TO_SPEECH,
		FEATURE_EXTEND_TO_TITLE,
		FEATURE_SCREEN_CAPTURE,
		FEATURE_STATUS_INDICATOR,
		FEATURE_NATIVE_HELP,
		FEATURE_NATIVE_DIALOG_INPUT,
		FEATURE_NATIVE_DIALOG_FILE,
		FEATURE_NATIVE_DIALOG_FILE_EXTRA,
		FEATURE_WINDOW_DRAG,
		FEATURE_SCREEN_EXCLUDE_FROM_CAPTURE,
	};

	virtual bool has_feature(Feature p_feature) const = 0;
	virtual String get_name() const = 0;

	virtual void help_set_search_callbacks(const Callable &p_search_callback = Callable(), const Callable &p_action_callback = Callable());

#ifndef DISABLE_DEPRECATED
	virtual void global_menu_set_popup_callbacks(const String &p_menu_root, const Callable &p_open_callback = Callable(), const Callable &p_close_callback = Callable());

	virtual int global_menu_add_submenu_item(const String &p_menu_root, const String &p_label, const String &p_submenu, int p_index = -1);
	virtual int global_menu_add_item(const String &p_menu_root, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int global_menu_add_check_item(const String &p_menu_root, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int global_menu_add_icon_item(const String &p_menu_root, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int global_menu_add_icon_check_item(const String &p_menu_root, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int global_menu_add_radio_check_item(const String &p_menu_root, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int global_menu_add_icon_radio_check_item(const String &p_menu_root, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int global_menu_add_multistate_item(const String &p_menu_root, const String &p_label, int p_max_states, int p_default_state, const Callable &p_callback = Callable(), const Callable &p_key_callback = Callable(), const Variant &p_tag = Variant(), Key p_accel = Key::NONE, int p_index = -1);
	virtual int global_menu_add_separator(const String &p_menu_root, int p_index = -1);

	virtual int global_menu_get_item_index_from_text(const String &p_menu_root, const String &p_text) const;
	virtual int global_menu_get_item_index_from_tag(const String &p_menu_root, const Variant &p_tag) const;

	virtual bool global_menu_is_item_checked(const String &p_menu_root, int p_idx) const;
	virtual bool global_menu_is_item_checkable(const String &p_menu_root, int p_idx) const;
	virtual bool global_menu_is_item_radio_checkable(const String &p_menu_root, int p_idx) const;
	virtual Callable global_menu_get_item_callback(const String &p_menu_root, int p_idx) const;
	virtual Callable global_menu_get_item_key_callback(const String &p_menu_root, int p_idx) const;
	virtual Variant global_menu_get_item_tag(const String &p_menu_root, int p_idx) const;
	virtual String global_menu_get_item_text(const String &p_menu_root, int p_idx) const;
	virtual String global_menu_get_item_submenu(const String &p_menu_root, int p_idx) const;
	virtual Key global_menu_get_item_accelerator(const String &p_menu_root, int p_idx) const;
	virtual bool global_menu_is_item_disabled(const String &p_menu_root, int p_idx) const;
	virtual bool global_menu_is_item_hidden(const String &p_menu_root, int p_idx) const;
	virtual String global_menu_get_item_tooltip(const String &p_menu_root, int p_idx) const;
	virtual int global_menu_get_item_state(const String &p_menu_root, int p_idx) const;
	virtual int global_menu_get_item_max_states(const String &p_menu_root, int p_idx) const;
	virtual Ref<Texture2D> global_menu_get_item_icon(const String &p_menu_root, int p_idx) const;
	virtual int global_menu_get_item_indentation_level(const String &p_menu_root, int p_idx) const;

	virtual void global_menu_set_item_checked(const String &p_menu_root, int p_idx, bool p_checked);
	virtual void global_menu_set_item_checkable(const String &p_menu_root, int p_idx, bool p_checkable);
	virtual void global_menu_set_item_radio_checkable(const String &p_menu_root, int p_idx, bool p_checkable);
	virtual void global_menu_set_item_callback(const String &p_menu_root, int p_idx, const Callable &p_callback);
	virtual void global_menu_set_item_key_callback(const String &p_menu_root, int p_idx, const Callable &p_key_callback);
	virtual void global_menu_set_item_hover_callbacks(const String &p_menu_root, int p_idx, const Callable &p_callback);
	virtual void global_menu_set_item_tag(const String &p_menu_root, int p_idx, const Variant &p_tag);
	virtual void global_menu_set_item_text(const String &p_menu_root, int p_idx, const String &p_text);
	virtual void global_menu_set_item_submenu(const String &p_menu_root, int p_idx, const String &p_submenu);
	virtual void global_menu_set_item_accelerator(const String &p_menu_root, int p_idx, Key p_keycode);
	virtual void global_menu_set_item_disabled(const String &p_menu_root, int p_idx, bool p_disabled);
	virtual void global_menu_set_item_hidden(const String &p_menu_root, int p_idx, bool p_hidden);
	virtual void global_menu_set_item_tooltip(const String &p_menu_root, int p_idx, const String &p_tooltip);
	virtual void global_menu_set_item_state(const String &p_menu_root, int p_idx, int p_state);
	virtual void global_menu_set_item_max_states(const String &p_menu_root, int p_idx, int p_max_states);
	virtual void global_menu_set_item_icon(const String &p_menu_root, int p_idx, const Ref<Texture2D> &p_icon);
	virtual void global_menu_set_item_indentation_level(const String &p_menu_root, int p_idx, int p_level);

	virtual int global_menu_get_item_count(const String &p_menu_root) const;

	virtual void global_menu_remove_item(const String &p_menu_root, int p_idx);
	virtual void global_menu_clear(const String &p_menu_root);

	virtual Dictionary global_menu_get_system_menu_roots() const;
#endif

	struct TTSUtterance {
		String text;
		String voice;
		int volume = 50;
		float pitch = 1.f;
		float rate = 1.f;
		int id = 0;
	};

	enum TTSUtteranceEvent {
		TTS_UTTERANCE_STARTED,
		TTS_UTTERANCE_ENDED,
		TTS_UTTERANCE_CANCELED,
		TTS_UTTERANCE_BOUNDARY,
		TTS_UTTERANCE_MAX,
	};

private:
	Callable utterance_callback[TTS_UTTERANCE_MAX];

public:
	virtual bool tts_is_speaking() const;
	virtual bool tts_is_paused() const;
	virtual TypedArray<Dictionary> tts_get_voices() const;
	virtual PackedStringArray tts_get_voices_for_language(const String &p_language) const;

	virtual void tts_speak(const String &p_text, const String &p_voice, int p_volume = 50, float p_pitch = 1.f, float p_rate = 1.f, int p_utterance_id = 0, bool p_interrupt = false);
	virtual void tts_pause();
	virtual void tts_resume();
	virtual void tts_stop();

	virtual void tts_set_utterance_callback(TTSUtteranceEvent p_event, const Callable &p_callable);
	virtual void tts_post_utterance_event(TTSUtteranceEvent p_event, int p_id, int p_pos = 0);

	virtual bool is_dark_mode_supported() const { return false; }
	virtual bool is_dark_mode() const { return false; }
	virtual Color get_accent_color() const { return Color(0, 0, 0, 0); }
	virtual Color get_base_color() const { return Color(0, 0, 0, 0); }
	virtual void set_system_theme_change_callback(const Callable &p_callable) {}

private:
	static bool window_early_clear_override_enabled;
	static Color window_early_clear_override_color;

protected:
	static bool _get_window_early_clear_override(Color &r_color);

public:
	static void set_early_window_clear_color_override(bool p_enabled, Color p_color = Color(0, 0, 0, 0));

	enum MouseMode {
		MOUSE_MODE_VISIBLE,
		MOUSE_MODE_HIDDEN,
		MOUSE_MODE_CAPTURED,
		MOUSE_MODE_CONFINED,
		MOUSE_MODE_CONFINED_HIDDEN,
	};

	virtual void mouse_set_mode(MouseMode p_mode);
	virtual MouseMode mouse_get_mode() const;

	virtual void warp_mouse(const Point2i &p_position);
	virtual Point2i mouse_get_position() const;
	virtual BitField<MouseButtonMask> mouse_get_button_state() const;

	virtual void clipboard_set(const String &p_text);
	virtual String clipboard_get() const;
	virtual Ref<Image> clipboard_get_image() const;
	virtual bool clipboard_has() const;
	virtual bool clipboard_has_image() const;
	virtual void clipboard_set_primary(const String &p_text);
	virtual String clipboard_get_primary() const;

	virtual TypedArray<Rect2> get_display_cutouts() const { return TypedArray<Rect2>(); }
	virtual Rect2i get_display_safe_area() const { return screen_get_usable_rect(); }

	enum {
		SCREEN_WITH_MOUSE_FOCUS = -4,
		SCREEN_WITH_KEYBOARD_FOCUS = -3,
		SCREEN_PRIMARY = -2,
		SCREEN_OF_MAIN_WINDOW = -1, // Note: for the main window, determine screen from position.
	};

	const float SCREEN_REFRESH_RATE_FALLBACK = -1.0; // Returned by screen_get_refresh_rate if the method fails.

	int _get_screen_index(int p_screen) const {
		switch (p_screen) {
			case SCREEN_WITH_MOUSE_FOCUS: {
				const Rect2i rect = Rect2i(mouse_get_position(), Vector2i(1, 1));
				return get_screen_from_rect(rect);
			} break;
			case SCREEN_WITH_KEYBOARD_FOCUS: {
				return get_keyboard_focus_screen();
			} break;
			case SCREEN_PRIMARY: {
				return get_primary_screen();
			} break;
			case SCREEN_OF_MAIN_WINDOW: {
				return window_get_current_screen(MAIN_WINDOW_ID);
			} break;
			default: {
				return p_screen;
			} break;
		}
	}

	virtual int get_screen_count() const = 0;
	virtual int get_primary_screen() const = 0;
	virtual int get_keyboard_focus_screen() const { return get_primary_screen(); }
	virtual int get_screen_from_rect(const Rect2 &p_rect) const;
	virtual Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const = 0;
	virtual Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const = 0;
	virtual Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const = 0;
	virtual int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const = 0;
	virtual float screen_get_scale(int p_screen = SCREEN_OF_MAIN_WINDOW) const;
	virtual float screen_get_max_scale() const {
		float scale = 1.f;
		int screen_count = get_screen_count();
		for (int i = 0; i < screen_count; i++) {
			scale = fmax(scale, screen_get_scale(i));
		}
		return scale;
	}
	virtual float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const = 0;
	virtual Color screen_get_pixel(const Point2i &p_position) const { return Color(); }
	virtual Ref<Image> screen_get_image(int p_screen = SCREEN_OF_MAIN_WINDOW) const { return Ref<Image>(); }
	virtual Ref<Image> screen_get_image_rect(const Rect2i &p_rect) const { return Ref<Image>(); }
	virtual bool is_touchscreen_available() const;

	// Keep the ScreenOrientation enum values in sync with the `display/window/handheld/orientation`
	// project setting hint.
	enum ScreenOrientation {
		SCREEN_LANDSCAPE,
		SCREEN_PORTRAIT,
		SCREEN_REVERSE_LANDSCAPE,
		SCREEN_REVERSE_PORTRAIT,
		SCREEN_SENSOR_LANDSCAPE,
		SCREEN_SENSOR_PORTRAIT,
		SCREEN_SENSOR,
	};

	virtual void screen_set_orientation(ScreenOrientation p_orientation, int p_screen = SCREEN_OF_MAIN_WINDOW);
	virtual ScreenOrientation screen_get_orientation(int p_screen = SCREEN_OF_MAIN_WINDOW) const;

	virtual void screen_set_keep_on(bool p_enable); //disable screensaver
	virtual bool screen_is_kept_on() const;
	enum {
		MAIN_WINDOW_ID = 0,
		INVALID_WINDOW_ID = -1,
		INVALID_INDICATOR_ID = -1
	};

public:
	typedef int WindowID;
	typedef int IndicatorID;

	virtual Vector<DisplayServer::WindowID> get_window_list() const = 0;

	enum WindowFlags {
		WINDOW_FLAG_RESIZE_DISABLED,
		WINDOW_FLAG_BORDERLESS,
		WINDOW_FLAG_ALWAYS_ON_TOP,
		WINDOW_FLAG_TRANSPARENT,
		WINDOW_FLAG_NO_FOCUS,
		WINDOW_FLAG_POPUP,
		WINDOW_FLAG_EXTEND_TO_TITLE,
		WINDOW_FLAG_MOUSE_PASSTHROUGH,
		WINDOW_FLAG_SHARP_CORNERS,
		WINDOW_FLAG_EXCLUDE_FROM_CAPTURE,
		WINDOW_FLAG_MAX,
	};

	// Separate enum otherwise we get warnings in switches not handling all values.
	enum WindowFlagsBit {
		WINDOW_FLAG_RESIZE_DISABLED_BIT = (1 << WINDOW_FLAG_RESIZE_DISABLED),
		WINDOW_FLAG_BORDERLESS_BIT = (1 << WINDOW_FLAG_BORDERLESS),
		WINDOW_FLAG_ALWAYS_ON_TOP_BIT = (1 << WINDOW_FLAG_ALWAYS_ON_TOP),
		WINDOW_FLAG_TRANSPARENT_BIT = (1 << WINDOW_FLAG_TRANSPARENT),
		WINDOW_FLAG_NO_FOCUS_BIT = (1 << WINDOW_FLAG_NO_FOCUS),
		WINDOW_FLAG_POPUP_BIT = (1 << WINDOW_FLAG_POPUP),
		WINDOW_FLAG_EXTEND_TO_TITLE_BIT = (1 << WINDOW_FLAG_EXTEND_TO_TITLE),
		WINDOW_FLAG_MOUSE_PASSTHROUGH_BIT = (1 << WINDOW_FLAG_MOUSE_PASSTHROUGH),
		WINDOW_FLAG_SHARP_CORNERS_BIT = (1 << WINDOW_FLAG_SHARP_CORNERS),
		WINDOW_FLAG_EXCLUDE_FROM_CAPTURE_BIT = (1 << WINDOW_FLAG_EXCLUDE_FROM_CAPTURE),
	};

	virtual WindowID create_sub_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect = Rect2i(), bool p_exclusive = false, WindowID p_transient_parent = INVALID_WINDOW_ID);
	virtual void show_window(WindowID p_id);
	virtual void delete_sub_window(WindowID p_id);

	virtual WindowID window_get_active_popup() const { return INVALID_WINDOW_ID; }
	virtual void window_set_popup_safe_rect(WindowID p_window, const Rect2i &p_rect) {}
	virtual Rect2i window_get_popup_safe_rect(WindowID p_window) const { return Rect2i(); }

	virtual int64_t window_get_native_handle(HandleType p_handle_type, WindowID p_window = MAIN_WINDOW_ID) const;

	virtual WindowID get_window_at_screen_position(const Point2i &p_position) const = 0;

	virtual void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) = 0;

	enum WindowEvent {
		WINDOW_EVENT_MOUSE_ENTER,
		WINDOW_EVENT_MOUSE_EXIT,
		WINDOW_EVENT_FOCUS_IN,
		WINDOW_EVENT_FOCUS_OUT,
		WINDOW_EVENT_CLOSE_REQUEST,
		WINDOW_EVENT_GO_BACK_REQUEST,
		WINDOW_EVENT_DPI_CHANGE,
		WINDOW_EVENT_TITLEBAR_CHANGE,
	};
	virtual void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) = 0;

	virtual void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) = 0;

	virtual void window_set_title(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual Size2i window_get_title_size(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) const { return Size2i(); }

	virtual void window_set_mouse_passthrough(const Vector<Vector2> &p_region, WindowID p_window = MAIN_WINDOW_ID);

	virtual int window_get_current_screen(WindowID p_window = MAIN_WINDOW_ID) const = 0;
	virtual void window_set_current_screen(int p_screen, WindowID p_window = MAIN_WINDOW_ID) = 0;

	virtual Point2i window_get_position(WindowID p_window = MAIN_WINDOW_ID) const = 0;
	virtual Point2i window_get_position_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const = 0;
	virtual void window_set_position(const Point2i &p_position, WindowID p_window = MAIN_WINDOW_ID) = 0;

	virtual void window_set_transient(WindowID p_window, WindowID p_parent) = 0;
	virtual void window_set_exclusive(WindowID p_window, bool p_exclusive);

	virtual void window_set_max_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual Size2i window_get_max_size(WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual void window_set_min_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual Size2i window_get_min_size(WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual void window_set_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual Size2i window_get_size(WindowID p_window = MAIN_WINDOW_ID) const = 0;
	virtual Size2i window_get_size_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual void window_set_mode(WindowMode p_mode, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual WindowMode window_get_mode(WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual void window_set_vsync_mode(VSyncMode p_vsync_mode, WindowID p_window = MAIN_WINDOW_ID);
	virtual VSyncMode window_get_vsync_mode(WindowID p_window) const;

	virtual bool window_is_maximize_allowed(WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual bool window_get_flag(WindowFlags p_flag, WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual void window_request_attention(WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual void window_move_to_foreground(WindowID p_window = MAIN_WINDOW_ID) = 0;
	virtual bool window_is_focused(WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual WindowID get_focused_window() const;

	virtual void window_set_window_buttons_offset(const Vector2i &p_offset, WindowID p_window = MAIN_WINDOW_ID) {}
	virtual Vector3i window_get_safe_title_margins(WindowID p_window = MAIN_WINDOW_ID) const { return Vector3i(); }

	virtual bool window_can_draw(WindowID p_window = MAIN_WINDOW_ID) const = 0;

	virtual bool can_any_window_draw() const = 0;

	virtual void window_set_ime_active(const bool p_active, WindowID p_window = MAIN_WINDOW_ID);
	virtual void window_set_ime_position(const Point2i &p_pos, WindowID p_window = MAIN_WINDOW_ID);

	virtual bool window_maximize_on_title_dbl_click() const { return false; }
	virtual bool window_minimize_on_title_dbl_click() const { return false; }

	virtual void window_start_drag(WindowID p_window = MAIN_WINDOW_ID) {}

	// necessary for GL focus, may be able to use one of the existing functions for this, not sure yet
	virtual void gl_window_make_current(DisplayServer::WindowID p_window_id);

	virtual Point2i ime_get_selection() const;
	virtual String ime_get_text() const;

	enum VirtualKeyboardType {
		KEYBOARD_TYPE_DEFAULT,
		KEYBOARD_TYPE_MULTILINE,
		KEYBOARD_TYPE_NUMBER,
		KEYBOARD_TYPE_NUMBER_DECIMAL,
		KEYBOARD_TYPE_PHONE,
		KEYBOARD_TYPE_EMAIL_ADDRESS,
		KEYBOARD_TYPE_PASSWORD,
		KEYBOARD_TYPE_URL
	};

	virtual void virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect = Rect2(), VirtualKeyboardType p_type = KEYBOARD_TYPE_DEFAULT, int p_max_length = -1, int p_cursor_start = -1, int p_cursor_end = -1);
	virtual void virtual_keyboard_hide();

	// returns height of the currently shown virtual keyboard (0 if keyboard is hidden)
	virtual int virtual_keyboard_get_height() const;

	virtual bool has_hardware_keyboard() const;

	enum CursorShape {
		CURSOR_ARROW,
		CURSOR_IBEAM,
		CURSOR_POINTING_HAND,
		CURSOR_CROSS,
		CURSOR_WAIT,
		CURSOR_BUSY,
		CURSOR_DRAG,
		CURSOR_CAN_DROP,
		CURSOR_FORBIDDEN,
		CURSOR_VSIZE,
		CURSOR_HSIZE,
		CURSOR_BDIAGSIZE,
		CURSOR_FDIAGSIZE,
		CURSOR_MOVE,
		CURSOR_VSPLIT,
		CURSOR_HSPLIT,
		CURSOR_HELP,
		CURSOR_MAX
	};
	virtual void cursor_set_shape(CursorShape p_shape);
	virtual CursorShape cursor_get_shape() const;
	virtual void cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape = CURSOR_ARROW, const Vector2 &p_hotspot = Vector2());

	virtual bool get_swap_cancel_ok();

	virtual void enable_for_stealing_focus(OS::ProcessID pid);

	virtual Error dialog_show(String p_title, String p_description, Vector<String> p_buttons, const Callable &p_callback);
	virtual Error dialog_input_text(String p_title, String p_description, String p_partial, const Callable &p_callback);

	enum FileDialogMode {
		FILE_DIALOG_MODE_OPEN_FILE,
		FILE_DIALOG_MODE_OPEN_FILES,
		FILE_DIALOG_MODE_OPEN_DIR,
		FILE_DIALOG_MODE_OPEN_ANY,
		FILE_DIALOG_MODE_SAVE_FILE,
		FILE_DIALOG_MODE_SAVE_MAX
	};
	virtual Error file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback);
	virtual Error file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback);

	virtual void beep() const;

	virtual int keyboard_get_layout_count() const;
	virtual int keyboard_get_current_layout() const;
	virtual void keyboard_set_current_layout(int p_index);
	virtual String keyboard_get_layout_language(int p_index) const;
	virtual String keyboard_get_layout_name(int p_index) const;
	virtual Key keyboard_get_keycode_from_physical(Key p_keycode) const;
	virtual Key keyboard_get_label_from_physical(Key p_keycode) const;

	virtual int tablet_get_driver_count() const { return 1; }
	virtual String tablet_get_driver_name(int p_driver) const { return "default"; }
	virtual String tablet_get_current_driver() const { return "default"; }
	virtual void tablet_set_current_driver(const String &p_driver) {}

	virtual void process_events() = 0;

	virtual void force_process_and_drop_events();

	virtual void release_rendering_thread();
	virtual void swap_buffers();

	virtual void set_native_icon(const String &p_filename);
	virtual void set_icon(const Ref<Image> &p_icon);

	virtual IndicatorID create_status_indicator(const Ref<Texture2D> &p_icon, const String &p_tooltip, const Callable &p_callback);
	virtual void status_indicator_set_icon(IndicatorID p_id, const Ref<Texture2D> &p_icon);
	virtual void status_indicator_set_tooltip(IndicatorID p_id, const String &p_tooltip);
	virtual void status_indicator_set_menu(IndicatorID p_id, const RID &p_menu_rid);
	virtual void status_indicator_set_callback(IndicatorID p_id, const Callable &p_callback);
	virtual Rect2 status_indicator_get_rect(IndicatorID p_id) const;
	virtual void delete_status_indicator(IndicatorID p_id);

	virtual void set_context(Context p_context);

	virtual bool is_window_transparency_available() const { return false; }

	void register_additional_output(Object *p_output);
	void unregister_additional_output(Object *p_output);
	bool has_additional_outputs() const { return additional_outputs.size() > 0; }

	static void register_create_function(const char *p_name, CreateFunction p_function, GetRenderingDriversFunction p_get_drivers);
	static int get_create_function_count();
	static const char *get_create_function_name(int p_index);
	static Vector<String> get_create_function_rendering_drivers(int p_index);
	static DisplayServer *create(int p_index, const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, Error &r_error);

	enum RenderingDeviceCreationStatus {
		UNKNOWN,
		SUCCESS,
		FAILURE,
	};

	// Used to cache the result of `can_create_rendering_device()` when RenderingDevice isn't currently being used.
	// This is done as creating a RenderingDevice is quite slow.
	static inline RenderingDeviceCreationStatus created_rendering_device = RenderingDeviceCreationStatus::UNKNOWN;

	static bool can_create_rendering_device();

	DisplayServer();
	~DisplayServer();
};

VARIANT_ENUM_CAST(DisplayServer::WindowEvent)
VARIANT_ENUM_CAST(DisplayServer::Feature)
VARIANT_ENUM_CAST(DisplayServer::MouseMode)
VARIANT_ENUM_CAST(DisplayServer::ScreenOrientation)
VARIANT_ENUM_CAST(DisplayServer::WindowMode)
VARIANT_ENUM_CAST(DisplayServer::WindowFlags)
VARIANT_ENUM_CAST(DisplayServer::HandleType)
VARIANT_ENUM_CAST(DisplayServer::VirtualKeyboardType);
VARIANT_ENUM_CAST(DisplayServer::CursorShape)
VARIANT_ENUM_CAST(DisplayServer::VSyncMode)
VARIANT_ENUM_CAST(DisplayServer::TTSUtteranceEvent)
VARIANT_ENUM_CAST(DisplayServer::FileDialogMode)

#endif // DISPLAY_SERVER_H
