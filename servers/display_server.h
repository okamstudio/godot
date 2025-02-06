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
class AccessibilityDriver;

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

	typedef DisplayServer *(*CreateFunction)(const String &, WindowMode, VSyncMode, uint32_t, const Point2i *, const Size2i &, int p_screen, Context, int64_t p_parent_window, Error &r_error);
	typedef Vector<String> (*GetRenderingDriversFunction)();

private:
	static void _input_set_mouse_mode(Input::MouseMode p_mode);
	static Input::MouseMode _input_get_mouse_mode();
	static void _input_set_mouse_mode_override(Input::MouseMode p_mode);
	static Input::MouseMode _input_get_mouse_mode_override();
	static void _input_set_mouse_mode_override_enabled(bool p_enabled);
	static bool _input_is_mouse_mode_override_enabled();
	static void _input_warp(const Vector2 &p_to_pos);
	static Input::CursorShape _input_get_current_cursor_shape();
	static void _input_set_custom_mouse_cursor_func(const Ref<Resource> &, Input::CursorShape, const Vector2 &p_hotspot);

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
		FEATURE_WINDOW_EMBEDDING,
		FEATURE_NATIVE_DIALOG_FILE_MIME,
		FEATURE_EMOJI_AND_SYMBOL_PICKER,
		FEATURE_ACCESSIBILITY_SCREEN_READER,
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
		MOUSE_MODE_VISIBLE = Input::MOUSE_MODE_VISIBLE,
		MOUSE_MODE_HIDDEN = Input::MOUSE_MODE_HIDDEN,
		MOUSE_MODE_CAPTURED = Input::MOUSE_MODE_CAPTURED,
		MOUSE_MODE_CONFINED = Input::MOUSE_MODE_CONFINED,
		MOUSE_MODE_CONFINED_HIDDEN = Input::MOUSE_MODE_CONFINED_HIDDEN,
		MOUSE_MODE_MAX = Input::MOUSE_MODE_MAX,
	};

	virtual void mouse_set_mode(MouseMode p_mode);
	virtual MouseMode mouse_get_mode() const;
	virtual void mouse_set_mode_override(MouseMode p_mode);
	virtual MouseMode mouse_get_mode_override() const;
	virtual void mouse_set_mode_override_enabled(bool p_override_enabled);
	virtual bool mouse_is_mode_override_enabled() const;

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

	enum WindowResizeEdge {
		WINDOW_EDGE_TOP_LEFT,
		WINDOW_EDGE_TOP,
		WINDOW_EDGE_TOP_RIGHT,
		WINDOW_EDGE_LEFT,
		WINDOW_EDGE_RIGHT,
		WINDOW_EDGE_BOTTOM_LEFT,
		WINDOW_EDGE_BOTTOM,
		WINDOW_EDGE_BOTTOM_RIGHT,
		WINDOW_EDGE_MAX,
	};

	virtual void window_start_resize(WindowResizeEdge p_edge, WindowID p_window = MAIN_WINDOW_ID) {}

	// Accessibility.

	enum AccessibilityMode {
		ACCESSIBILITY_AUTO,
		ACCESSIBILITY_ALWAYS,
		ACCESSIBILITY_DISABLED,
	};

protected:
	AccessibilityDriver *accessibility_driver = nullptr;
	static AccessibilityMode accessibility_mode;

public:
	enum AccessibilityRole {
		ROLE_UNKNOWN,
		ROLE_DEFAULT_BUTTON,
		ROLE_AUDIO,
		ROLE_VIDEO,
		ROLE_STATIC_TEXT,
		ROLE_CONTAINER,
		ROLE_PANEL,
		ROLE_BUTTON,
		ROLE_LINK,
		ROLE_CHECK_BOX,
		ROLE_RADIO_BUTTON,
		ROLE_CHECK_BUTTON,
		ROLE_SCROLL_BAR,
		ROLE_SCROLL_VIEW,
		ROLE_SPLITTER,
		ROLE_SLIDER,
		ROLE_SPIN_BUTTON,
		ROLE_PROGRESS_INDICATOR,
		ROLE_TEXT_FIELD,
		ROLE_MULTILINE_TEXT_FIELD,
		ROLE_COLOR_PICKER,
		ROLE_TABLE,
		ROLE_CELL,
		ROLE_ROW,
		ROLE_ROW_GROUP,
		ROLE_ROW_HEADER,
		ROLE_COLUMN_HEADER,
		ROLE_TREE,
		ROLE_TREE_ITEM,
		ROLE_LIST,
		ROLE_LIST_ITEM,
		ROLE_LIST_BOX,
		ROLE_LIST_BOX_OPTION,
		ROLE_TAB_BAR,
		ROLE_TAB,
		ROLE_TAB_PANEL,
		ROLE_MENU_BAR,
		ROLE_MENU,
		ROLE_MENU_ITEM,
		ROLE_MENU_ITEM_CHECK_BOX,
		ROLE_MENU_ITEM_RADIO,
		ROLE_IMAGE,
		ROLE_WINDOW,
		ROLE_TITLE_BAR,
		ROLE_DIALOG,
		ROLE_TOOLTIP,
	};

	enum AccessibilityPopupType {
		POPUP_UNKNOWN,
		POPUP_MENU,
		POPUP_LIST,
		POPUP_TREE,
		POPUP_DIALOG,
	};

	enum AccessibilityFlags {
		FLAG_HIDDEN,
		FLAG_LINKED,
		FLAG_MULTISELECTABLE,
		FLAG_REQUIRED,
		FLAG_VISITED,
		FLAG_BUSY,
		FLAG_MODAL,
		FLAG_TOUCH_PASSTHROUGH,
		FLAG_READONLY,
		FLAG_DISABLED,
		FLAG_CLIPS_CHILDREN,
	};

	enum AccessibilityAction {
		ACTION_CLICK,
		ACTION_FOCUS,
		ACTION_BLUR,
		ACTION_COLLAPSE,
		ACTION_EXPAND,
		ACTION_DECREMENT,
		ACTION_INCREMENT,
		ACTION_HIDE_TOOLTIP,
		ACTION_SHOW_TOOLTIP,
		ACTION_SET_TEXT_SELECTION,
		ACTION_REPLACE_SELECTED_TEXT,
		ACTION_SCROLL_BACKWARD,
		ACTION_SCROLL_DOWN,
		ACTION_SCROLL_FORWARD,
		ACTION_SCROLL_LEFT,
		ACTION_SCROLL_RIGHT,
		ACTION_SCROLL_UP,
		ACTION_SCROLL_INTO_VIEW,
		ACTION_SCROLL_TO_POINT,
		ACTION_SET_SCROLL_OFFSET,
		ACTION_SET_VALUE,
		ACTION_SHOW_CONTEXT_MENU,
		ACTION_CUSTOM,
	};

	enum AccessibilityLiveMode {
		LIVE_OFF,
		LIVE_POLITE,
		LIVE_ASSERTIVE,
	};

	static AccessibilityMode accessibility_get_mode() { return accessibility_mode; }
	static void accessibility_set_mode(AccessibilityMode p_mode) { accessibility_mode = p_mode; }

	virtual int accessibility_should_increase_contrast() const { return -1; }
	virtual int accessibility_should_reduce_animation() const { return -1; }
	virtual int accessibility_should_reduce_transparency() const { return -1; }
	virtual int accessibility_screen_reader_active() const { return -1; }

	virtual RID accessibility_create_element(WindowID p_window_id, DisplayServer::AccessibilityRole p_role);
	virtual RID accessibility_create_sub_element(const RID &p_parent_rid, DisplayServer::AccessibilityRole p_role, int p_insert_pos = -1);
	virtual RID accessibility_create_sub_text_edit_elements(const RID &p_parent_rid, const RID &p_shaped_text, float p_min_height, int p_insert_pos = -1);
	virtual bool accessibility_has_element(const RID &p_id) const;
	virtual void accessibility_free_element(const RID &p_id);

	virtual void accessibility_element_set_meta(const RID &p_id, const Variant &p_meta);
	virtual Variant accessibility_element_get_meta(const RID &p_id) const;

	virtual void accessibility_update_if_active(const Callable &p_callable);

	virtual void accessibility_update_set_focus(const RID &p_id);
	virtual RID accessibility_get_window_root(DisplayServer::WindowID p_window_id) const;

	virtual void accessibility_set_window_rect(DisplayServer::WindowID p_window_id, const Rect2 &p_rect_out, const Rect2 &p_rect_in);
	virtual void accessibility_set_window_focused(DisplayServer::WindowID p_window_id, bool p_focused);

	virtual void accessibility_update_set_role(const RID &p_id, DisplayServer::AccessibilityRole p_role);
	virtual void accessibility_update_set_name(const RID &p_id, const String &p_name);
	virtual void accessibility_update_set_extra_info(const RID &p_id, const String &p_name_extra_info);
	virtual void accessibility_update_set_description(const RID &p_id, const String &p_description);
	virtual void accessibility_update_set_value(const RID &p_id, const String &p_value);
	virtual void accessibility_update_set_tooltip(const RID &p_id, const String &p_tooltip);
	virtual void accessibility_update_set_bounds(const RID &p_id, const Rect2 &p_rect);
	virtual void accessibility_update_set_transform(const RID &p_id, const Transform2D &p_transform);
	virtual void accessibility_update_add_child(const RID &p_id, const RID &p_child_id);
	virtual void accessibility_update_add_related_controls(const RID &p_id, const RID &p_related_id);
	virtual void accessibility_update_add_related_details(const RID &p_id, const RID &p_related_id);
	virtual void accessibility_update_add_related_described_by(const RID &p_id, const RID &p_related_id);
	virtual void accessibility_update_add_related_flow_to(const RID &p_id, const RID &p_related_id);
	virtual void accessibility_update_add_related_labeled_by(const RID &p_id, const RID &p_related_id);
	virtual void accessibility_update_add_related_radio_group(const RID &p_id, const RID &p_related_id);
	virtual void accessibility_update_set_active_descendant(const RID &p_id, const RID &p_other_id);
	virtual void accessibility_update_set_next_on_line(const RID &p_id, const RID &p_other_id);
	virtual void accessibility_update_set_previous_on_line(const RID &p_id, const RID &p_other_id);
	virtual void accessibility_update_set_member_of(const RID &p_id, const RID &p_other_id);
	virtual void accessibility_update_set_in_page_link_target(const RID &p_id, const RID &p_other_id);
	virtual void accessibility_update_set_error_message(const RID &p_id, const RID &p_other_id);
	virtual void accessibility_update_set_live(const RID &p_id, DisplayServer::AccessibilityLiveMode p_live);
	virtual void accessibility_update_add_action(const RID &p_id, DisplayServer::AccessibilityAction p_action, const Callable &p_callable);
	virtual void accessibility_update_add_custom_action(const RID &p_id, int p_action_id, const String &p_action_description);
	virtual void accessibility_update_set_table_row_count(const RID &p_id, int p_count);
	virtual void accessibility_update_set_table_column_count(const RID &p_id, int p_count);
	virtual void accessibility_update_set_table_row_index(const RID &p_id, int p_index);
	virtual void accessibility_update_set_table_column_index(const RID &p_id, int p_index);
	virtual void accessibility_update_set_table_cell_position(const RID &p_id, int p_row_index, int p_column_index);
	virtual void accessibility_update_set_table_cell_span(const RID &p_id, int p_row_span, int p_column_span);
	virtual void accessibility_update_set_list_item_count(const RID &p_id, int p_size);
	virtual void accessibility_update_set_list_item_index(const RID &p_id, int p_index);
	virtual void accessibility_update_set_list_item_level(const RID &p_id, int p_level);
	virtual void accessibility_update_set_list_item_selected(const RID &p_id, bool p_selected);
	virtual void accessibility_update_set_list_item_expanded(const RID &p_id, bool p_expanded);
	virtual void accessibility_update_set_popup_type(const RID &p_id, DisplayServer::AccessibilityPopupType p_popup);
	virtual void accessibility_update_set_checked(const RID &p_id, bool p_checekd);
	virtual void accessibility_update_set_num_value(const RID &p_id, double p_position);
	virtual void accessibility_update_set_num_range(const RID &p_id, double p_min, double p_max);
	virtual void accessibility_update_set_num_step(const RID &p_id, double p_step);
	virtual void accessibility_update_set_num_jump(const RID &p_id, double p_jump);
	virtual void accessibility_update_set_scroll_x(const RID &p_id, double p_position);
	virtual void accessibility_update_set_scroll_x_range(const RID &p_id, double p_min, double p_max);
	virtual void accessibility_update_set_scroll_y(const RID &p_id, double p_position);
	virtual void accessibility_update_set_scroll_y_range(const RID &p_id, double p_min, double p_max);
	virtual void accessibility_update_set_text_decorations(const RID &p_id, bool p_underline, bool p_strikethrough, bool p_overline);
	virtual void accessibility_update_set_text_align(const RID &p_id, HorizontalAlignment p_align);
	virtual void accessibility_update_set_text_selection(const RID &p_id, const RID &p_text_start_id, int p_start_char, const RID &p_text_end_id, int p_end_char);
	virtual void accessibility_update_set_flag(const RID &p_id, DisplayServer::AccessibilityFlags p_flag, bool p_value);
	virtual void accessibility_update_set_classname(const RID &p_id, const String &p_classname);
	virtual void accessibility_update_set_placeholder(const RID &p_id, const String &p_placeholder);
	virtual void accessibility_update_set_language(const RID &p_id, const String &p_language);
	virtual void accessibility_update_set_text_orientation(const RID &p_id, bool p_vertical);
	virtual void accessibility_update_set_list_orientation(const RID &p_id, bool p_vertical);
	virtual void accessibility_update_set_shortcut(const RID &p_id, const String &p_shortcut);
	virtual void accessibility_update_set_url(const RID &p_id, const String &p_url);
	virtual void accessibility_update_set_role_description(const RID &p_id, const String &p_description);
	virtual void accessibility_update_set_state_description(const RID &p_id, const String &p_description);
	virtual void accessibility_update_set_color_value(const RID &p_id, const Color &p_color);
	virtual void accessibility_update_set_background_color(const RID &p_id, const Color &p_color);
	virtual void accessibility_update_set_foreground_color(const RID &p_id, const Color &p_color);

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

	virtual Error embed_process(WindowID p_window, OS::ProcessID p_pid, const Rect2i &p_rect, bool p_visible, bool p_grab_focus);
	virtual Error remove_embedded_process(OS::ProcessID p_pid);
	virtual OS::ProcessID get_focused_process_id();

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
	virtual void show_emoji_and_symbol_picker() const;

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
	static DisplayServer *create(int p_index, const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);

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

/**************************************************************************/

class AccessibilityDriver {
public:
	virtual Error init() = 0;

	virtual bool window_create(DisplayServer::WindowID p_window_id, void *p_handle) = 0;
	virtual void window_destroy(DisplayServer::WindowID p_window_id) = 0;

	virtual RID accessibility_create_element(DisplayServer::WindowID p_window_id, DisplayServer::AccessibilityRole p_role) = 0;
	virtual RID accessibility_create_sub_element(const RID &p_parent_rid, DisplayServer::AccessibilityRole p_role, int p_insert_pos = -1) = 0;
	virtual RID accessibility_create_sub_text_edit_elements(const RID &p_parent_rid, const RID &p_shaped_text, float p_min_height, int p_insert_pos = -1) = 0;
	virtual bool accessibility_has_element(const RID &p_id) const = 0;
	virtual void accessibility_free_element(const RID &p_id) = 0;

	virtual void accessibility_element_set_meta(const RID &p_id, const Variant &p_meta) = 0;
	virtual Variant accessibility_element_get_meta(const RID &p_id) const = 0;

	virtual void accessibility_update_if_active(const Callable &p_callable) = 0;

	virtual RID accessibility_get_window_root(DisplayServer::WindowID p_window_id) const = 0;
	virtual void accessibility_update_set_focus(const RID &p_id) = 0;

	virtual void accessibility_set_window_rect(DisplayServer::WindowID p_window_id, const Rect2 &p_rect_out, const Rect2 &p_rect_in) = 0;
	virtual void accessibility_set_window_focused(DisplayServer::WindowID p_window_id, bool p_focused) = 0;

	virtual void accessibility_update_set_role(const RID &p_id, DisplayServer::AccessibilityRole p_role) = 0;
	virtual void accessibility_update_set_name(const RID &p_id, const String &p_name) = 0;
	virtual void accessibility_update_set_extra_info(const RID &p_id, const String &p_name_extra_info) = 0;
	virtual void accessibility_update_set_description(const RID &p_id, const String &p_description) = 0;
	virtual void accessibility_update_set_value(const RID &p_id, const String &p_value) = 0;
	virtual void accessibility_update_set_tooltip(const RID &p_id, const String &p_tooltip) = 0;
	virtual void accessibility_update_set_bounds(const RID &p_id, const Rect2 &p_rect) = 0;
	virtual void accessibility_update_set_transform(const RID &p_id, const Transform2D &p_transform) = 0;
	virtual void accessibility_update_add_child(const RID &p_id, const RID &p_child_id) = 0;
	virtual void accessibility_update_add_related_controls(const RID &p_id, const RID &p_related_id) = 0;
	virtual void accessibility_update_add_related_details(const RID &p_id, const RID &p_related_id) = 0;
	virtual void accessibility_update_add_related_described_by(const RID &p_id, const RID &p_related_id) = 0;
	virtual void accessibility_update_add_related_flow_to(const RID &p_id, const RID &p_related_id) = 0;
	virtual void accessibility_update_add_related_labeled_by(const RID &p_id, const RID &p_related_id) = 0;
	virtual void accessibility_update_add_related_radio_group(const RID &p_id, const RID &p_related_id) = 0;
	virtual void accessibility_update_set_active_descendant(const RID &p_id, const RID &p_other_id) = 0;
	virtual void accessibility_update_set_next_on_line(const RID &p_id, const RID &p_other_id) = 0;
	virtual void accessibility_update_set_previous_on_line(const RID &p_id, const RID &p_other_id) = 0;
	virtual void accessibility_update_set_member_of(const RID &p_id, const RID &p_other_id) = 0;
	virtual void accessibility_update_set_in_page_link_target(const RID &p_id, const RID &p_other_id) = 0;
	virtual void accessibility_update_set_error_message(const RID &p_id, const RID &p_other_id) = 0;
	virtual void accessibility_update_set_live(const RID &p_id, DisplayServer::AccessibilityLiveMode p_live) = 0;
	virtual void accessibility_update_add_action(const RID &p_id, DisplayServer::AccessibilityAction p_action, const Callable &p_callable) = 0;
	virtual void accessibility_update_add_custom_action(const RID &p_id, int p_action_id, const String &p_action_description) = 0;
	virtual void accessibility_update_set_table_row_count(const RID &p_id, int p_count) = 0;
	virtual void accessibility_update_set_table_column_count(const RID &p_id, int p_count) = 0;
	virtual void accessibility_update_set_table_row_index(const RID &p_id, int p_index) = 0;
	virtual void accessibility_update_set_table_column_index(const RID &p_id, int p_index) = 0;
	virtual void accessibility_update_set_table_cell_position(const RID &p_id, int p_row_index, int p_column_index) = 0;
	virtual void accessibility_update_set_table_cell_span(const RID &p_id, int p_row_span, int p_column_span) = 0;
	virtual void accessibility_update_set_list_item_count(const RID &p_id, int p_size) = 0;
	virtual void accessibility_update_set_list_item_index(const RID &p_id, int p_index) = 0;
	virtual void accessibility_update_set_list_item_level(const RID &p_id, int p_level) = 0;
	virtual void accessibility_update_set_list_item_selected(const RID &p_id, bool p_selected) = 0;
	virtual void accessibility_update_set_list_item_expanded(const RID &p_id, bool p_expanded) = 0;
	virtual void accessibility_update_set_popup_type(const RID &p_id, DisplayServer::AccessibilityPopupType p_popup) = 0;
	virtual void accessibility_update_set_checked(const RID &p_id, bool p_checekd) = 0;
	virtual void accessibility_update_set_num_value(const RID &p_id, double p_position) = 0;
	virtual void accessibility_update_set_num_range(const RID &p_id, double p_min, double p_max) = 0;
	virtual void accessibility_update_set_num_step(const RID &p_id, double p_step) = 0;
	virtual void accessibility_update_set_num_jump(const RID &p_id, double p_jump) = 0;
	virtual void accessibility_update_set_scroll_x(const RID &p_id, double p_position) = 0;
	virtual void accessibility_update_set_scroll_x_range(const RID &p_id, double p_min, double p_max) = 0;
	virtual void accessibility_update_set_scroll_y(const RID &p_id, double p_position) = 0;
	virtual void accessibility_update_set_scroll_y_range(const RID &p_id, double p_min, double p_max) = 0;
	virtual void accessibility_update_set_text_decorations(const RID &p_id, bool p_underline, bool p_strikethrough, bool p_overline) = 0;
	virtual void accessibility_update_set_text_align(const RID &p_id, HorizontalAlignment p_align) = 0;
	virtual void accessibility_update_set_text_selection(const RID &p_id, const RID &p_text_start_id, int p_start_char, const RID &p_text_end_id, int p_end_char) = 0;
	virtual void accessibility_update_set_flag(const RID &p_id, DisplayServer::AccessibilityFlags p_flag, bool p_value) = 0;
	virtual void accessibility_update_set_classname(const RID &p_id, const String &p_classname) = 0;
	virtual void accessibility_update_set_placeholder(const RID &p_id, const String &p_placeholder) = 0;
	virtual void accessibility_update_set_language(const RID &p_id, const String &p_language) = 0;
	virtual void accessibility_update_set_text_orientation(const RID &p_id, bool p_vertical) = 0;
	virtual void accessibility_update_set_list_orientation(const RID &p_id, bool p_vertical) = 0;
	virtual void accessibility_update_set_shortcut(const RID &p_id, const String &p_shortcut) = 0;
	virtual void accessibility_update_set_url(const RID &p_id, const String &p_url) = 0;
	virtual void accessibility_update_set_role_description(const RID &p_id, const String &p_description) = 0;
	virtual void accessibility_update_set_state_description(const RID &p_id, const String &p_description) = 0;
	virtual void accessibility_update_set_color_value(const RID &p_id, const Color &p_color) = 0;
	virtual void accessibility_update_set_background_color(const RID &p_id, const Color &p_color) = 0;
	virtual void accessibility_update_set_foreground_color(const RID &p_id, const Color &p_color) = 0;

	AccessibilityDriver() {}
	virtual ~AccessibilityDriver() {}
};

VARIANT_ENUM_CAST(DisplayServer::AccessibilityAction)
VARIANT_ENUM_CAST(DisplayServer::AccessibilityFlags)
VARIANT_ENUM_CAST(DisplayServer::AccessibilityLiveMode)
VARIANT_ENUM_CAST(DisplayServer::AccessibilityPopupType)
VARIANT_ENUM_CAST(DisplayServer::AccessibilityRole)

VARIANT_ENUM_CAST(DisplayServer::WindowEvent)
VARIANT_ENUM_CAST(DisplayServer::Feature)
VARIANT_ENUM_CAST(DisplayServer::MouseMode)
VARIANT_ENUM_CAST(DisplayServer::ScreenOrientation)
VARIANT_ENUM_CAST(DisplayServer::WindowMode)
VARIANT_ENUM_CAST(DisplayServer::WindowFlags)
VARIANT_ENUM_CAST(DisplayServer::WindowResizeEdge)
VARIANT_ENUM_CAST(DisplayServer::HandleType)
VARIANT_ENUM_CAST(DisplayServer::VirtualKeyboardType);
VARIANT_ENUM_CAST(DisplayServer::CursorShape)
VARIANT_ENUM_CAST(DisplayServer::VSyncMode)
VARIANT_ENUM_CAST(DisplayServer::TTSUtteranceEvent)
VARIANT_ENUM_CAST(DisplayServer::FileDialogMode)

#endif // DISPLAY_SERVER_H
