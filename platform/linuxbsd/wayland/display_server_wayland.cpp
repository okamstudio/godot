/**************************************************************************/
/*  display_server_wayland.cpp                                            */
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

#include "display_server_wayland.h"

#ifdef WAYLAND_ENABLED

#define WAYLAND_DISPLAY_SERVER_DEBUG_LOGS_ENABLED
#ifdef WAYLAND_DISPLAY_SERVER_DEBUG_LOGS_ENABLED
#define DEBUG_LOG_WAYLAND(...) print_verbose(__VA_ARGS__)
#else
#define DEBUG_LOG_WAYLAND(...)
#endif

#ifdef VULKAN_ENABLED
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#endif

#ifdef GLES3_ENABLED
#include "detect_prime_egl.h"
#include "drivers/gles3/rasterizer_gles3.h"
#include "wayland/egl_manager_wayland.h"
#include "wayland/egl_manager_wayland_gles.h"
#endif

#define WAYLAND_MAX_FRAME_TIME_US (1'000'000)

String DisplayServerWayland::_get_app_id_from_context(Context p_context) {
	String app_id;

	switch (p_context) {
		case CONTEXT_EDITOR: {
			app_id = "org.godotengine.Editor";
		} break;

		case CONTEXT_PROJECTMAN: {
			app_id = "org.godotengine.ProjectManager";
		} break;

		case CONTEXT_ENGINE:
		default: {
			String config_name = GLOBAL_GET("application/config/name");
			if (config_name.length() != 0) {
				app_id = config_name;
			} else {
				app_id = "org.godotengine.Godot";
			}
		}
	}

	return app_id;
}

void DisplayServerWayland::_send_window_event(WindowEvent p_event, WindowID p_window_id) {
	ERR_FAIL_COND(!windows.has(p_window_id));

	WindowData &wd = windows[p_window_id];

	if (wd.window_event_callback.is_valid()) {
		Variant event = int(p_event);
		wd.window_event_callback.call(event);
	}
}

void DisplayServerWayland::dispatch_input_events(const Ref<InputEvent> &p_event) {
	static_cast<DisplayServerWayland *>(get_singleton())->_dispatch_input_event(p_event);
}

void DisplayServerWayland::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	Ref<InputEventFromWindow> event_from_window = p_event;

	if (event_from_window.is_valid()) {
		WindowID window_id = event_from_window->get_window_id();

		Ref<InputEventKey> key_event = p_event;
		if (!popup_menu_list.is_empty() && key_event.is_valid()) {
			// Redirect to the highest popup menu.
			window_id = popup_menu_list.back()->get();
		}

		// Send to a single window.
		if (windows.has(window_id)) {
			Callable callable = windows[window_id].input_event_callback;
			if (callable.is_valid()) {
				callable.call(p_event);
			}
		}
	}
}

void DisplayServerWayland::_update_window_rect(const Rect2i &p_rect, WindowID p_window_id) {
	ERR_FAIL_COND(!windows.has(p_window_id));

	WindowData &wd = windows[p_window_id];

	if (wd.rect == p_rect) {
		return;
	}

	wd.rect = p_rect;

#ifdef RD_ENABLED
	if (wd.visible && rendering_context) {
		rendering_context->window_set_size(p_window_id, wd.rect.size.width, wd.rect.size.height);
	}
#endif

#ifdef GLES3_ENABLED
	if (wd.visible && egl_manager) {
		wl_egl_window_resize(wd.wl_egl_window, wd.rect.size.width, wd.rect.size.height, 0, 0);
	}
#endif

	if (wd.rect_changed_callback.is_valid()) {
		wd.rect_changed_callback.call(wd.rect);
	}
}

// Interface methods.

bool DisplayServerWayland::has_feature(Feature p_feature) const {
	switch (p_feature) {
#ifndef DISABLE_DEPRECATED
		case FEATURE_GLOBAL_MENU: {
			return (native_menu && native_menu->has_feature(NativeMenu::FEATURE_GLOBAL_MENU));
		} break;
#endif
		case FEATURE_MOUSE:
		case FEATURE_MOUSE_WARP:
		case FEATURE_CLIPBOARD:
		case FEATURE_CURSOR_SHAPE:
		case FEATURE_CUSTOM_CURSOR_SHAPE:
		case FEATURE_WINDOW_TRANSPARENCY:
		case FEATURE_HIDPI:
		case FEATURE_SWAP_BUFFERS:
		case FEATURE_KEEP_SCREEN_ON:
		case FEATURE_IME:
		case FEATURE_WINDOW_DRAG:
		case FEATURE_CLIPBOARD_PRIMARY:
		case FEATURE_SUBWINDOWS:
		case FEATURE_SELF_FITTING_WINDOWS: {
			return true;
		} break;

		//case FEATURE_NATIVE_DIALOG:
		//case FEATURE_NATIVE_DIALOG_INPUT:
#ifdef DBUS_ENABLED
		case FEATURE_NATIVE_DIALOG_FILE:
		case FEATURE_NATIVE_DIALOG_FILE_EXTRA:
		case FEATURE_NATIVE_DIALOG_FILE_MIME: {
			return (portal_desktop && portal_desktop->is_supported() && portal_desktop->is_file_chooser_supported());
		} break;
#endif

#ifdef SPEECHD_ENABLED
		case FEATURE_TEXT_TO_SPEECH: {
			return true;
		} break;
#endif

		default: {
			return false;
		}
	}
}

String DisplayServerWayland::get_name() const {
	return "Wayland";
}

#ifdef SPEECHD_ENABLED

bool DisplayServerWayland::tts_is_speaking() const {
	ERR_FAIL_NULL_V(tts, false);
	return tts->is_speaking();
}

bool DisplayServerWayland::tts_is_paused() const {
	ERR_FAIL_NULL_V(tts, false);
	return tts->is_paused();
}

TypedArray<Dictionary> DisplayServerWayland::tts_get_voices() const {
	ERR_FAIL_NULL_V(tts, TypedArray<Dictionary>());
	return tts->get_voices();
}

void DisplayServerWayland::tts_speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	ERR_FAIL_NULL(tts);
	tts->speak(p_text, p_voice, p_volume, p_pitch, p_rate, p_utterance_id, p_interrupt);
}

void DisplayServerWayland::tts_pause() {
	ERR_FAIL_NULL(tts);
	tts->pause();
}

void DisplayServerWayland::tts_resume() {
	ERR_FAIL_NULL(tts);
	tts->resume();
}

void DisplayServerWayland::tts_stop() {
	ERR_FAIL_NULL(tts);
	tts->stop();
}

#endif

#ifdef DBUS_ENABLED

bool DisplayServerWayland::is_dark_mode_supported() const {
	return portal_desktop && portal_desktop->is_supported() && portal_desktop->is_settings_supported();
}

bool DisplayServerWayland::is_dark_mode() const {
	if (!is_dark_mode_supported()) {
		return false;
	}
	switch (portal_desktop->get_appearance_color_scheme()) {
		case 1:
			// Prefers dark theme.
			return true;
		case 2:
			// Prefers light theme.
			return false;
		default:
			// Preference unknown.
			return false;
	}
}

void DisplayServerWayland::set_system_theme_change_callback(const Callable &p_callable) {
	portal_desktop->set_system_theme_change_callback(p_callable);
}

Error DisplayServerWayland::file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback) {
	MutexLock mutex_lock(wayland_thread.mutex);

	WindowID window_id = wayland_thread.pointer_get_last_pointed_window_id();
	if (!windows.has(window_id)) {
		window_id = MAIN_WINDOW_ID;
	}

	WaylandThread::WindowState *ws = wayland_thread.window_get_state(window_id);
	ERR_FAIL_NULL_V(ws, ERR_BUG);

	return portal_desktop->file_dialog_show(window_id, (ws ? ws->exported_handle : String()), p_title, p_current_directory, String(), p_filename, p_mode, p_filters, TypedArray<Dictionary>(), p_callback, false);
}

Error DisplayServerWayland::file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback) {
	MutexLock mutex_lock(wayland_thread.mutex);

	WindowID window_id = wayland_thread.pointer_get_last_pointed_window_id();
	if (!windows.has(window_id)) {
		window_id = MAIN_WINDOW_ID;
	}

	WaylandThread::WindowState *ws = wayland_thread.window_get_state(window_id);
	ERR_FAIL_NULL_V(ws, ERR_BUG);

	return portal_desktop->file_dialog_show(window_id, (ws ? ws->exported_handle : String()), p_title, p_current_directory, p_root, p_filename, p_mode, p_filters, p_options, p_callback, true);
}

#endif

void DisplayServerWayland::beep() const {
	wayland_thread.beep();
}

void DisplayServerWayland::_mouse_update_mode() {
	MouseMode wanted_mouse_mode = mouse_mode_override_enabled
			? mouse_mode_override
			: mouse_mode_base;

	if (wanted_mouse_mode == mouse_mode) {
		return;
	}

	MutexLock mutex_lock(wayland_thread.mutex);

	bool show_cursor = (wanted_mouse_mode == MOUSE_MODE_VISIBLE || wanted_mouse_mode == MOUSE_MODE_CONFINED);

	wayland_thread.cursor_set_visible(show_cursor);

	WaylandThread::PointerConstraint constraint = WaylandThread::PointerConstraint::NONE;

	switch (wanted_mouse_mode) {
		case DisplayServer::MOUSE_MODE_CAPTURED: {
			constraint = WaylandThread::PointerConstraint::LOCKED;
		} break;

		case DisplayServer::MOUSE_MODE_CONFINED:
		case DisplayServer::MOUSE_MODE_CONFINED_HIDDEN: {
			constraint = WaylandThread::PointerConstraint::CONFINED;
		} break;

		default: {
		}
	}

	wayland_thread.pointer_set_constraint(constraint);

	if (wanted_mouse_mode == DisplayServer::MOUSE_MODE_CAPTURED) {
		WindowData *pointed_win = windows.getptr(wayland_thread.pointer_get_pointed_window_id());
		ERR_FAIL_NULL(pointed_win);
		wayland_thread.pointer_set_hint(pointed_win->rect.size / 2);
	}

	mouse_mode = wanted_mouse_mode;
}

void DisplayServerWayland::mouse_set_mode(MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, MouseMode::MOUSE_MODE_MAX);
	if (p_mode == mouse_mode_base) {
		return;
	}
	mouse_mode_base = p_mode;
	_mouse_update_mode();
}

DisplayServerWayland::MouseMode DisplayServerWayland::mouse_get_mode() const {
	return mouse_mode;
}

void DisplayServerWayland::mouse_set_mode_override(MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, MouseMode::MOUSE_MODE_MAX);
	if (p_mode == mouse_mode_override) {
		return;
	}
	mouse_mode_override = p_mode;
	_mouse_update_mode();
}

DisplayServerWayland::MouseMode DisplayServerWayland::mouse_get_mode_override() const {
	return mouse_mode_override;
}

void DisplayServerWayland::mouse_set_mode_override_enabled(bool p_override_enabled) {
	if (p_override_enabled == mouse_mode_override_enabled) {
		return;
	}
	mouse_mode_override_enabled = p_override_enabled;
	_mouse_update_mode();
}

bool DisplayServerWayland::mouse_is_mode_override_enabled() const {
	return mouse_mode_override_enabled;
}

// NOTE: This is hacked together (and not guaranteed to work in the first place)
// as for some reason the there's no proper way to ask the compositor to warp
// the pointer, although, at the time of writing, there's a proposal for a
// proper protocol for this. See:
// https://gitlab.freedesktop.org/wayland/wayland-protocols/-/issues/158
void DisplayServerWayland::warp_mouse(const Point2i &p_to) {
	MutexLock mutex_lock(wayland_thread.mutex);

	WaylandThread::PointerConstraint old_constraint = wayland_thread.pointer_get_constraint();

	wayland_thread.pointer_set_constraint(WaylandThread::PointerConstraint::LOCKED);
	wayland_thread.pointer_set_hint(p_to);

	wayland_thread.pointer_set_constraint(old_constraint);
}

Point2i DisplayServerWayland::mouse_get_position() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	WindowID pointed_id = wayland_thread.pointer_get_pointed_window_id();

	if (pointed_id != INVALID_WINDOW_ID) {
		return Input::get_singleton()->get_mouse_position() + windows[pointed_id].rect.position;
	}

	// We can't properly implement this method by design.
	// This is the best we can do unfortunately.
	return Input::get_singleton()->get_mouse_position();

	return Point2i();
}

BitField<MouseButtonMask> DisplayServerWayland::mouse_get_button_state() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	// Are we sure this is the only way? This seems sus.
	// TODO: Handle tablets properly.
	//mouse_button_mask.set_flag(MouseButtonMask((int64_t)wls.current_seat->tablet_tool_data.pressed_button_mask));

	return wayland_thread.pointer_get_button_mask();
}

// NOTE: According to the Wayland specification, this method will only do
// anything if the user has interacted with the application by sending a
// "recent enough" input event.
// TODO: Add this limitation to the documentation.
void DisplayServerWayland::clipboard_set(const String &p_text) {
	MutexLock mutex_lock(wayland_thread.mutex);

	wayland_thread.selection_set_text(p_text);
}

String DisplayServerWayland::clipboard_get() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	Vector<uint8_t> data;

	const String text_mimes[] = {
		"text/plain;charset=utf-8",
		"text/plain",
	};

	for (String mime : text_mimes) {
		if (wayland_thread.selection_has_mime(mime)) {
			print_verbose(vformat("Selecting media type \"%s\" from offered types.", mime));
			data = wayland_thread.selection_get_mime(mime);
			break;
		}
	}

	return String::utf8((const char *)data.ptr(), data.size());
}

Ref<Image> DisplayServerWayland::clipboard_get_image() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	Ref<Image> image;
	image.instantiate();

	Error err = OK;

	// TODO: Fallback to next media type on missing module or parse error.
	if (wayland_thread.selection_has_mime("image/png")) {
		err = image->load_png_from_buffer(wayland_thread.selection_get_mime("image/png"));
	} else if (wayland_thread.selection_has_mime("image/jpeg")) {
		err = image->load_jpg_from_buffer(wayland_thread.selection_get_mime("image/jpeg"));
	} else if (wayland_thread.selection_has_mime("image/webp")) {
		err = image->load_webp_from_buffer(wayland_thread.selection_get_mime("image/webp"));
	} else if (wayland_thread.selection_has_mime("image/svg+xml")) {
		err = image->load_svg_from_buffer(wayland_thread.selection_get_mime("image/svg+xml"));
	} else if (wayland_thread.selection_has_mime("image/bmp")) {
		err = image->load_bmp_from_buffer(wayland_thread.selection_get_mime("image/bmp"));
	} else if (wayland_thread.selection_has_mime("image/x-tga")) {
		err = image->load_tga_from_buffer(wayland_thread.selection_get_mime("image/x-tga"));
	} else if (wayland_thread.selection_has_mime("image/x-targa")) {
		err = image->load_tga_from_buffer(wayland_thread.selection_get_mime("image/x-targa"));
	} else if (wayland_thread.selection_has_mime("image/ktx")) {
		err = image->load_ktx_from_buffer(wayland_thread.selection_get_mime("image/ktx"));
	}

	ERR_FAIL_COND_V(err != OK, Ref<Image>());

	return image;
}

void DisplayServerWayland::clipboard_set_primary(const String &p_text) {
	MutexLock mutex_lock(wayland_thread.mutex);

	wayland_thread.primary_set_text(p_text);
}

String DisplayServerWayland::clipboard_get_primary() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	Vector<uint8_t> data;

	const String text_mimes[] = {
		"text/plain;charset=utf-8",
		"text/plain",
	};

	for (String mime : text_mimes) {
		if (wayland_thread.primary_has_mime(mime)) {
			print_verbose(vformat("Selecting media type \"%s\" from offered types.", mime));
			data = wayland_thread.primary_get_mime(mime);
			break;
		}
	}

	return String::utf8((const char *)data.ptr(), data.size());
}

int DisplayServerWayland::get_screen_count() const {
	MutexLock mutex_lock(wayland_thread.mutex);
	return wayland_thread.get_screen_count();
}

int DisplayServerWayland::get_primary_screen() const {
	// AFAIK Wayland doesn't allow knowing (nor we care) about which screen is
	// primary.
	return 0;
}

Point2i DisplayServerWayland::screen_get_position(int p_screen) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	if (p_screen == SCREEN_OF_MAIN_WINDOW) {
		p_screen = window_get_current_screen();
	}

	return wayland_thread.screen_get_data(p_screen).position;
}

Size2i DisplayServerWayland::screen_get_size(int p_screen) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	if (p_screen == SCREEN_OF_MAIN_WINDOW) {
		p_screen = window_get_current_screen();
	}

	return wayland_thread.screen_get_data(p_screen).size;
}

Rect2i DisplayServerWayland::screen_get_usable_rect(int p_screen) const {
	// Unsupported on wayland.
	return Rect2i(Point2i(), screen_get_size(p_screen));
}

int DisplayServerWayland::screen_get_dpi(int p_screen) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	if (p_screen == SCREEN_OF_MAIN_WINDOW) {
		p_screen = window_get_current_screen();
	}

	const WaylandThread::ScreenData &data = wayland_thread.screen_get_data(p_screen);

	int width_mm = data.physical_size.width;
	int height_mm = data.physical_size.height;

	double xdpi = (width_mm ? data.size.width / (double)width_mm * 25.4 : 0);
	double ydpi = (height_mm ? data.size.height / (double)height_mm * 25.4 : 0);

	if (xdpi || ydpi) {
		return (xdpi + ydpi) / (xdpi && ydpi ? 2 : 1);
	}

	// Could not get DPI.
	return 96;
}

float DisplayServerWayland::screen_get_scale(int p_screen) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	if (p_screen == SCREEN_OF_MAIN_WINDOW) {
		// Wayland does not expose fractional scale factors at the screen-level, but
		// some code relies on it. Since this special screen is the default and a lot
		// of code relies on it, we'll return the window's scale, which is what we
		// really care about. After all, we have very little use of the actual screen
		// enumeration APIs and we're (for now) in single-window mode anyways.
		struct wl_surface *wl_surface = wayland_thread.window_get_wl_surface(MAIN_WINDOW_ID);
		WaylandThread::WindowState *ws = wayland_thread.wl_surface_get_window_state(wl_surface);

		return wayland_thread.window_state_get_scale_factor(ws);
	}

	return wayland_thread.screen_get_data(p_screen).scale;
}

float DisplayServerWayland::screen_get_refresh_rate(int p_screen) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	if (p_screen == SCREEN_OF_MAIN_WINDOW) {
		p_screen = window_get_current_screen();
	}

	return wayland_thread.screen_get_data(p_screen).refresh_rate;
}

void DisplayServerWayland::screen_set_keep_on(bool p_enable) {
	MutexLock mutex_lock(wayland_thread.mutex);

	// FIXME: For some reason this does not also windows from the wayland thread.

	if (screen_is_kept_on() == p_enable) {
		return;
	}

#ifdef DBUS_ENABLED
	if (screensaver) {
		if (p_enable) {
			screensaver->inhibit();
		} else {
			screensaver->uninhibit();
		}

		screensaver_inhibited = p_enable;
	}
#endif
}

bool DisplayServerWayland::screen_is_kept_on() const {
	// FIXME: Multiwindow support.
#ifdef DBUS_ENABLED
	return wayland_thread.window_get_idle_inhibition(MAIN_WINDOW_ID) || screensaver_inhibited;
#else
	return wayland_thread.window_get_idle_inhibition(MAIN_WINDOW_ID);
#endif
}

Vector<DisplayServer::WindowID> DisplayServerWayland::get_window_list() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	Vector<int> ret;
	for (const KeyValue<WindowID, WindowData> &E : windows) {
		ret.push_back(E.key);
	}
	return ret;
}

DisplayServer::WindowID DisplayServerWayland::create_sub_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect, bool p_exclusive, WindowID p_transient_parent) {
	WindowID id = ++window_id_counter;
	WindowData &wd = windows[id];

	wd.id = id;
	wd.mode = p_mode;
	wd.flags = p_flags;
	wd.vsync_mode = p_vsync_mode;

	// NOTE: Remember to clear its position if this window will be a toplevel. We
	// can only know once we show it.
	wd.rect = p_rect;

	wd.title = "Godot";
	wd.parent_id = p_transient_parent;
	return id;
}

void DisplayServerWayland::show_window(WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));

	WindowData &wd = windows[p_window_id];

	if (!wd.visible) {
		DEBUG_LOG_WAYLAND(vformat("Showing window %d", p_window_id));
		// Showing this window will reset its mode with whatever the compositor
		// reports. We'll save the mode beforehand so that we can reapply it later.
		// TODO: Fix/Port/Move/Whatever to `WaylandThread` APIs.
		WindowMode setup_mode = wd.mode;

		// Let's determine the closest toplevel. For toplevels it will be themselves,
		// for popups the first toplevel ancestor it finds.
		WindowID root_id = wd.id;
		while (root_id != INVALID_WINDOW_ID && window_get_flag(WINDOW_FLAG_POPUP_WM_HINT, root_id)) {
			root_id = windows[root_id].parent_id;
		}
		ERR_FAIL_COND(root_id == INVALID_WINDOW_ID);

		wd.root_id = root_id;

		if (!window_get_flag(WINDOW_FLAG_POPUP_WM_HINT, p_window_id)) {
			// NOTE: DO **NOT** KEEP THE POSITION SET FOR TOPLEVELS. Wayland does not
			// track them and we're gonna get our events transformed in unexpected ways.
			wd.rect.position = Point2i();

			DEBUG_LOG_WAYLAND(vformat("Creating regular window of size %s", wd.rect.size));
			wayland_thread.window_create(p_window_id, wd.rect.size.width, wd.rect.size.height);
			wayland_thread.window_set_min_size(p_window_id, wd.min_size);
			wayland_thread.window_set_max_size(p_window_id, wd.max_size);
			wayland_thread.window_set_app_id(p_window_id, _get_app_id_from_context(context));
			wayland_thread.window_set_borderless(p_window_id, window_get_flag(WINDOW_FLAG_BORDERLESS, p_window_id));

			if (wd.parent_id != INVALID_WINDOW_ID) {
				wayland_thread.window_set_parent(wd.id, wd.parent_id);
			}

			// Since it can't have a position. Let's tell the window node the news by
			// the actual rect to it.
			if (wd.rect_changed_callback.is_valid()) {
				wd.rect_changed_callback.call(wd.rect);
			}
		} else {
			DEBUG_LOG_WAYLAND("!!!!! Making popup !!!!!");

			windows[root_id].popup_stack.push_back(p_window_id);

			if (window_get_flag(WINDOW_FLAG_POPUP, p_window_id)) {
				// Reroutes all input to it.
				popup_menu_list.push_back(p_window_id);
			}

			wayland_thread.window_create_popup(p_window_id, wd.parent_id, wd.rect);
		}

		// NOTE: The XDG shell protocol is built in a way that causes the window to
		// be immediately shown as soon as a valid buffer is assigned to it. Hence,
		// the only acceptable way of implementing window showing is to move the
		// graphics context window creation logic here.
#ifdef RD_ENABLED
		if (rendering_context) {
			union {
#ifdef VULKAN_ENABLED
				RenderingContextDriverVulkanWayland::WindowPlatformData vulkan;
#endif
			} wpd;
#ifdef VULKAN_ENABLED
			if (rendering_driver == "vulkan") {
				wpd.vulkan.surface = wayland_thread.window_get_wl_surface(wd.id);
				wpd.vulkan.display = wayland_thread.get_wl_display();
			}
#endif
			Error err = rendering_context->window_create(wd.id, &wpd);
			ERR_FAIL_COND_MSG(err != OK, vformat("Can't create a %s window", rendering_driver));

			rendering_context->window_set_size(wd.id, wd.rect.size.width, wd.rect.size.height);

			// NOTE: Looks like we have to set the vsync mode before creating the screen
			// or it won't work. Resist any temptation.
			window_set_vsync_mode(wd.vsync_mode, p_window_id);
		}

		if (rendering_device) {
			rendering_device->screen_create(wd.id);
		}
#endif

#ifdef GLES3_ENABLED
		if (egl_manager) {
			struct wl_surface *wl_surface = wayland_thread.window_get_wl_surface(wd.id);
			wd.wl_egl_window = wl_egl_window_create(wl_surface, wd.rect.size.width, wd.rect.size.height);

			Error err = egl_manager->window_create(p_window_id, wayland_thread.get_wl_display(), wd.wl_egl_window, wd.rect.size.width, wd.rect.size.height);
			ERR_FAIL_COND_MSG(err == ERR_CANT_CREATE, "Can't show a GLES3 window.");

			window_set_vsync_mode(wd.vsync_mode, p_window_id);
		}
#endif

		// NOTE: The public window-handling methods might depend on this flag being
		// set. Ensure to not make any of these calls before this assignment.
		wd.visible = true;

		// Actually try to apply the window's mode now that it's visible.
		window_set_mode(setup_mode, wd.id);

		wayland_thread.window_set_title(p_window_id, wd.title);
	}
}

void DisplayServerWayland::delete_sub_window(WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));
	WindowData &wd = windows[p_window_id];

	ERR_FAIL_COND(!windows.has(wd.root_id));
	WindowData &root_wd = windows[wd.root_id];

	// NOTE: By the time the Wayland thread will send a `WINDOW_EVENT_MOUSE_EXIT`
	// the window will be gone and the message will be discarded, confusing the
	// engine. We thus have to send it ourselves.
	if (wayland_thread.pointer_get_pointed_window_id() == p_window_id) {
		_send_window_event(WINDOW_EVENT_MOUSE_EXIT, p_window_id);
	}

	// The XDG shell specification requires us to clear all popups in reverse order.
	while (!root_wd.popup_stack.is_empty() && root_wd.popup_stack.back()->get() != p_window_id) {
		_send_window_event(WINDOW_EVENT_FORCE_CLOSE, root_wd.popup_stack.back()->get());
	}

	if (root_wd.popup_stack.back() && root_wd.popup_stack.back()->get() == p_window_id) {
		root_wd.popup_stack.pop_back();
	}

	if (popup_menu_list.back() && popup_menu_list.back()->get() == p_window_id) {
		popup_menu_list.pop_back();
	}

	if (wd.visible) {
#ifdef VULKAN_ENABLED
		if (rendering_device) {
			rendering_device->screen_free(p_window_id);
		}

		if (rendering_context) {
			rendering_context->window_destroy(p_window_id);
		}
#endif

#ifdef GLES3_ENABLED
		if (egl_manager) {
			egl_manager->window_destroy(p_window_id);
		}
#endif

		wayland_thread.window_destroy(p_window_id);
	}

	windows.erase(p_window_id);

	DEBUG_LOG_WAYLAND(vformat("Destroyed window %d", p_window_id));
}

DisplayServer::WindowID DisplayServerWayland::window_get_active_popup() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	if (!popup_menu_list.is_empty()) {
		return popup_menu_list.back()->get();
	}

	return INVALID_WINDOW_ID;
}

void DisplayServerWayland::window_set_popup_safe_rect(WindowID p_window, const Rect2i &p_rect) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window));

	windows[p_window].safe_rect = p_rect;
}

Rect2i DisplayServerWayland::window_get_popup_safe_rect(WindowID p_window) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND_V(!windows.has(p_window), Rect2i());

	return windows[p_window].safe_rect;
}

int64_t DisplayServerWayland::window_get_native_handle(HandleType p_handle_type, WindowID p_window) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	switch (p_handle_type) {
		case DISPLAY_HANDLE: {
			return (int64_t)wayland_thread.get_wl_display();
		} break;

		case WINDOW_HANDLE: {
			return (int64_t)wayland_thread.window_get_wl_surface(p_window);
		} break;

		case WINDOW_VIEW: {
			return 0; // Not supported.
		} break;

#ifdef GLES3_ENABLED
		case OPENGL_CONTEXT: {
			if (egl_manager) {
				return (int64_t)egl_manager->get_context(p_window);
			}
			return 0;
		} break;
		case EGL_DISPLAY: {
			if (egl_manager) {
				return (int64_t)egl_manager->get_display(p_window);
			}
			return 0;
		}
		case EGL_CONFIG: {
			if (egl_manager) {
				return (int64_t)egl_manager->get_config(p_window);
			}
			return 0;
		}
#endif // GLES3_ENABLED

		default: {
			return 0;
		} break;
	}
}

DisplayServer::WindowID DisplayServerWayland::get_window_at_screen_position(const Point2i &p_position) const {
	// Standard Wayland APIs don't support this.
	return MAIN_WINDOW_ID;
}

void DisplayServerWayland::window_attach_instance_id(ObjectID p_instance, WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));

	windows[p_window_id].instance_id = p_instance;
}

ObjectID DisplayServerWayland::window_get_attached_instance_id(WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND_V(!windows.has(p_window_id), ObjectID());

	return windows[p_window_id].instance_id;
}

void DisplayServerWayland::window_set_title(const String &p_title, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));

	WindowData &wd = windows[p_window_id];

	wd.title = p_title;

	if (wd.visible) {
		wayland_thread.window_set_title(p_window_id, wd.title);
	}
}

void DisplayServerWayland::window_set_mouse_passthrough(const Vector<Vector2> &p_region, DisplayServer::WindowID p_window_id) {
	// TODO
	DEBUG_LOG_WAYLAND(vformat("wayland stub window_set_mouse_passthrough region %s", p_region));
}

void DisplayServerWayland::window_set_rect_changed_callback(const Callable &p_callable, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));

	windows[p_window_id].rect_changed_callback = p_callable;
}

void DisplayServerWayland::window_set_window_event_callback(const Callable &p_callable, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));

	windows[p_window_id].window_event_callback = p_callable;
}

void DisplayServerWayland::window_set_input_event_callback(const Callable &p_callable, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));

	windows[p_window_id].input_event_callback = p_callable;
}

void DisplayServerWayland::window_set_input_text_callback(const Callable &p_callable, WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));

	windows[p_window_id].input_text_callback = p_callable;
}

void DisplayServerWayland::window_set_drop_files_callback(const Callable &p_callable, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));

	windows[p_window_id].drop_files_callback = p_callable;
}

int DisplayServerWayland::window_get_current_screen(DisplayServer::WindowID p_window_id) const {
	// Standard Wayland APIs don't support getting the screen of a window.
	return 0;
}

void DisplayServerWayland::window_set_current_screen(int p_screen, DisplayServer::WindowID p_window_id) {
	// Standard Wayland APIs don't support setting the screen of a window.
}

Point2i DisplayServerWayland::window_get_position(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return windows[p_window_id].rect.position;
}

Point2i DisplayServerWayland::window_get_position_with_decorations(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return windows[p_window_id].rect.position;
}

void DisplayServerWayland::window_set_position(const Point2i &p_position, DisplayServer::WindowID p_window_id) {
	// Unsupported with toplevels.
}

void DisplayServerWayland::window_set_max_size(const Size2i p_size, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	DEBUG_LOG_WAYLAND(vformat("window max size set to %s", p_size));

	if (p_size.x < 0 || p_size.y < 0) {
		ERR_FAIL_MSG("Maximum window size can't be negative!");
	}

	ERR_FAIL_COND(!windows.has(p_window_id));
	WindowData &wd = windows[p_window_id];

	// FIXME: Is `p_size.x < wd.min_size.x || p_size.y < wd.min_size.y` == `p_size < wd.min_size`?
	if ((p_size != Size2i()) && ((p_size.x < wd.min_size.x) || (p_size.y < wd.min_size.y))) {
		ERR_PRINT("Maximum window size can't be smaller than minimum window size!");
		return;
	}

	wd.max_size = p_size;

	if (wd.visible) {
		wayland_thread.window_set_max_size(p_window_id, p_size);
	}
}

Size2i DisplayServerWayland::window_get_max_size(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND_V(!windows.has(p_window_id), Size2i());
	return windows[p_window_id].max_size;
}

void DisplayServerWayland::gl_window_make_current(DisplayServer::WindowID p_window_id) {
#ifdef GLES3_ENABLED
	if (egl_manager) {
		egl_manager->window_make_current(p_window_id);
	}
#endif
}

void DisplayServerWayland::window_set_transient(WindowID p_window_id, WindowID p_parent) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));
	WindowData &wd = windows[p_window_id];

	ERR_FAIL_COND(wd.parent_id == p_parent);

	if (p_parent != INVALID_WINDOW_ID) {
		ERR_FAIL_COND(!windows.has(p_parent));
		ERR_FAIL_COND_MSG(wd.parent_id != INVALID_WINDOW_ID, "Window already has a transient parent");
		wd.parent_id = p_parent;

		// NOTE: Looks like live unparenting is not really practical unfortunately.
		// See WaylandThread::window_set_parent for more info.
		if (wd.visible) {
			wayland_thread.window_set_parent(p_window_id, p_parent);
		}
	}
}

void DisplayServerWayland::window_set_min_size(const Size2i p_size, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	DEBUG_LOG_WAYLAND(vformat("window minsize set to %s", p_size));

	ERR_FAIL_COND(!windows.has(p_window_id));
	WindowData &wd = windows[p_window_id];

	if (p_size.x < 0 || p_size.y < 0) {
		ERR_FAIL_MSG("Minimum window size can't be negative!");
	}

	// FIXME: Is `p_size.x > wd.max_size.x || p_size.y > wd.max_size.y` == `p_size > wd.max_size`?
	if ((p_size != Size2i()) && (wd.max_size != Size2i()) && ((p_size.x > wd.max_size.x) || (p_size.y > wd.max_size.y))) {
		ERR_PRINT("Minimum window size can't be larger than maximum window size!");
		return;
	}

	wd.min_size = p_size;

	if (wd.visible) {
		wayland_thread.window_set_min_size(p_window_id, p_size);
	}
}

Size2i DisplayServerWayland::window_get_min_size(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND_V(!windows.has(p_window_id), Size2i());
	return windows[p_window_id].min_size;
}

void DisplayServerWayland::window_set_size(const Size2i p_size, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));
	WindowData &wd = windows[p_window_id];

	// The XDG spec doesn't allow non-interactive resizes. Let's update the
	// window's internal representation to account for that.
	if (wd.rect_changed_callback.is_valid()) {
		wd.rect_changed_callback.call(wd.rect);
	}
}

Size2i DisplayServerWayland::window_get_size(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND_V(!windows.has(p_window_id), Size2i());
	return windows[p_window_id].rect.size;
}

Size2i DisplayServerWayland::window_get_size_with_decorations(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	// I don't think there's a way of actually knowing the size of the window
	// decoration in Wayland, at least in the case of SSDs, nor that it would be
	// that useful in this case. We'll just return the main window's size.
	ERR_FAIL_COND_V(!windows.has(p_window_id), Size2i());
	return windows[p_window_id].rect.size;
}

void DisplayServerWayland::window_set_mode(WindowMode p_mode, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));
	WindowData &wd = windows[p_window_id];

	if (!wd.visible) {
		return;
	}

	wayland_thread.window_try_set_mode(p_window_id, p_mode);
}

DisplayServer::WindowMode DisplayServerWayland::window_get_mode(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND_V(!windows.has(p_window_id), WINDOW_MODE_WINDOWED);
	const WindowData &wd = windows[p_window_id];

	if (!wd.visible) {
		return WINDOW_MODE_WINDOWED;
	}

	return wayland_thread.window_get_mode(p_window_id);
}

bool DisplayServerWayland::window_is_maximize_allowed(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return wayland_thread.window_can_set_mode(p_window_id, WINDOW_MODE_MAXIMIZED);
}

void DisplayServerWayland::window_set_flag(WindowFlags p_flag, bool p_enabled, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND(!windows.has(p_window_id));
	WindowData &wd = windows[p_window_id];

	DEBUG_LOG_WAYLAND(vformat("Window set flag %d", p_flag));

	switch (p_flag) {
		case WINDOW_FLAG_BORDERLESS: {
			wayland_thread.window_set_borderless(p_window_id, p_enabled);
		} break;

		case WINDOW_FLAG_POPUP: {
			ERR_FAIL_COND_MSG(p_window_id == MAIN_WINDOW_ID, "Main window can't be popup.");
			ERR_FAIL_COND_MSG(wd.visible, "Popup flag can't changed while window is opened.");
		} break;

		case WINDOW_FLAG_POPUP_WM_HINT: {
			ERR_FAIL_COND_MSG(p_window_id == MAIN_WINDOW_ID, "Main window can't have popup hint.");
			ERR_FAIL_COND_MSG(wd.visible, "Popup hint can't changed while window is opened.");
		} break;

		default: {
		}
	}

	if (p_enabled) {
		wd.flags |= 1 << p_flag;
	} else {
		wd.flags &= ~(1 << p_flag);
	}
}

bool DisplayServerWayland::window_get_flag(WindowFlags p_flag, DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_COND_V(!windows.has(p_window_id), false);
	return windows[p_window_id].flags & (1 << p_flag);
}

void DisplayServerWayland::window_request_attention(DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	DEBUG_LOG_WAYLAND("Requested attention.");

	wayland_thread.window_request_attention(p_window_id);
}

void DisplayServerWayland::window_move_to_foreground(DisplayServer::WindowID p_window_id) {
	// Standard Wayland APIs don't support this.
}

bool DisplayServerWayland::window_is_focused(WindowID p_window_id) const {
	return wayland_thread.pointer_get_pointed_window_id() == p_window_id;
}

bool DisplayServerWayland::window_can_draw(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	uint64_t last_frame_time = wayland_thread.window_get_last_frame_time(p_window_id);
	uint64_t time_since_frame = OS::get_singleton()->get_ticks_usec() - last_frame_time;

	if (time_since_frame > WAYLAND_MAX_FRAME_TIME_US) {
		return false;
	}

	if (wayland_thread.window_is_suspended(p_window_id)) {
		return false;
	}

	return suspend_state == SuspendState::NONE;
}

bool DisplayServerWayland::can_any_window_draw() const {
	return suspend_state == SuspendState::NONE;
}

void DisplayServerWayland::window_set_ime_active(const bool p_active, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	wayland_thread.window_set_ime_active(p_active, p_window_id);
}

void DisplayServerWayland::window_set_ime_position(const Point2i &p_pos, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	wayland_thread.window_set_ime_position(p_pos, p_window_id);
}

Point2i DisplayServerWayland::ime_get_selection() const {
	return ime_selection;
}

String DisplayServerWayland::ime_get_text() const {
	return ime_text;
}

// NOTE: While Wayland is supposed to be tear-free, wayland-protocols version
// 1.30 added a protocol for allowing async flips which is supposed to be
// handled by drivers such as Vulkan. We can then just ask to disable v-sync and
// hope for the best. See: https://gitlab.freedesktop.org/wayland/wayland-protocols/-/commit/6394f0b4f3be151076f10a845a2fb131eeb56706
void DisplayServerWayland::window_set_vsync_mode(DisplayServer::VSyncMode p_vsync_mode, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	WindowData &wd = windows[p_window_id];

#ifdef RD_ENABLED
	if (rendering_context) {
		rendering_context->window_set_vsync_mode(p_window_id, p_vsync_mode);

		wd.emulate_vsync = (rendering_context->window_get_vsync_mode(p_window_id) == DisplayServer::VSYNC_ENABLED);

		if (wd.emulate_vsync) {
			print_verbose("VSYNC: manually throttling frames using MAILBOX.");
			rendering_context->window_set_vsync_mode(p_window_id, DisplayServer::VSYNC_MAILBOX);
		}
	}
#endif // VULKAN_ENABLED

#ifdef GLES3_ENABLED
	if (egl_manager) {
		egl_manager->set_use_vsync(p_vsync_mode != DisplayServer::VSYNC_DISABLED);

		wd.emulate_vsync = egl_manager->is_using_vsync();

		if (wd.emulate_vsync) {
			print_verbose("VSYNC: manually throttling frames with swap delay 0.");
			egl_manager->set_use_vsync(false);
		}
	}
#endif // GLES3_ENABLED
}

DisplayServer::VSyncMode DisplayServerWayland::window_get_vsync_mode(DisplayServer::WindowID p_window_id) const {
	const WindowData &wd = windows[p_window_id];
	if (wd.emulate_vsync) {
		return DisplayServer::VSYNC_ENABLED;
	}

#ifdef VULKAN_ENABLED
	if (rendering_context) {
		return rendering_context->window_get_vsync_mode(p_window_id);
	}
#endif // VULKAN_ENABLED

#ifdef GLES3_ENABLED
	if (egl_manager) {
		return egl_manager->is_using_vsync() ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED;
	}
#endif // GLES3_ENABLED

	return DisplayServer::VSYNC_ENABLED;
}

void DisplayServerWayland::window_start_drag(WindowID p_window) {
	MutexLock mutex_lock(wayland_thread.mutex);

	wayland_thread.window_start_drag(p_window);
}

void DisplayServerWayland::window_start_resize(WindowResizeEdge p_edge, WindowID p_window) {
	MutexLock mutex_lock(wayland_thread.mutex);

	ERR_FAIL_INDEX(int(p_edge), WINDOW_EDGE_MAX);
	wayland_thread.window_start_resize(p_edge, p_window);
}

void DisplayServerWayland::cursor_set_shape(CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);

	MutexLock mutex_lock(wayland_thread.mutex);

	if (p_shape == cursor_shape) {
		return;
	}

	cursor_shape = p_shape;

	if (mouse_mode != MOUSE_MODE_VISIBLE && mouse_mode != MOUSE_MODE_CONFINED) {
		// Hidden.
		return;
	}

	wayland_thread.cursor_set_shape(p_shape);
}

DisplayServerWayland::CursorShape DisplayServerWayland::cursor_get_shape() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return cursor_shape;
}

void DisplayServerWayland::cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	MutexLock mutex_lock(wayland_thread.mutex);

	if (p_cursor.is_valid()) {
		HashMap<CursorShape, CustomCursor>::Iterator cursor_c = custom_cursors.find(p_shape);

		if (cursor_c) {
			if (cursor_c->value.resource == p_cursor && cursor_c->value.hotspot == p_hotspot) {
				// We have a cached cursor. Nice.
				wayland_thread.cursor_set_shape(p_shape);
				return;
			}

			// We're changing this cursor; we'll have to rebuild it.
			custom_cursors.erase(p_shape);
			wayland_thread.cursor_shape_clear_custom_image(p_shape);
		}

		Ref<Image> image = _get_cursor_image_from_resource(p_cursor, p_hotspot);
		ERR_FAIL_COND(image.is_null());

		CustomCursor &cursor = custom_cursors[p_shape];

		cursor.resource = p_cursor;
		cursor.hotspot = p_hotspot;

		wayland_thread.cursor_shape_set_custom_image(p_shape, image, p_hotspot);

		wayland_thread.cursor_set_shape(p_shape);
	} else {
		// Clear cache and reset to default system cursor.
		wayland_thread.cursor_shape_clear_custom_image(p_shape);

		if (cursor_shape == p_shape) {
			wayland_thread.cursor_set_shape(p_shape);
		}

		if (custom_cursors.has(p_shape)) {
			custom_cursors.erase(p_shape);
		}
	}
}

int DisplayServerWayland::keyboard_get_layout_count() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return wayland_thread.keyboard_get_layout_count();
}

int DisplayServerWayland::keyboard_get_current_layout() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return wayland_thread.keyboard_get_current_layout_index();
}

void DisplayServerWayland::keyboard_set_current_layout(int p_index) {
	MutexLock mutex_lock(wayland_thread.mutex);

	wayland_thread.keyboard_set_current_layout_index(p_index);
}

String DisplayServerWayland::keyboard_get_layout_language(int p_index) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	// xkbcommon exposes only the layout's name, which looks like it overlaps with
	// its language.
	return wayland_thread.keyboard_get_layout_name(p_index);
}

String DisplayServerWayland::keyboard_get_layout_name(int p_index) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return wayland_thread.keyboard_get_layout_name(p_index);
}

Key DisplayServerWayland::keyboard_get_keycode_from_physical(Key p_keycode) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	Key key = wayland_thread.keyboard_get_key_from_physical(p_keycode);

	// If not found, fallback to QWERTY.
	// This should match the behavior of the event pump.
	if (key == Key::NONE) {
		return p_keycode;
	}

	if (key >= Key::A + 32 && key <= Key::Z + 32) {
		key -= 'a' - 'A';
	}

	// Make it consistent with the keys returned by `Input`.
	if (key == Key::BACKTAB) {
		key = Key::TAB;
	}

	return key;
}

void DisplayServerWayland::try_suspend() {
	bool must_emulate = false;

	for (KeyValue<DisplayServer::WindowID, WindowData> &pair : windows) {
		if (pair.value.emulate_vsync) {
			must_emulate = true;
			break;
		}
	}

	// Due to various reasons, we manually handle display synchronization by
	// waiting for a frame event (request to draw) or, if available, the actual
	// window's suspend status. When a window is suspended, we can avoid drawing
	// altogether, either because the compositor told us that we don't need to or
	// because the pace of the frame events became unreliable.
	if (must_emulate) {
		bool frame = wayland_thread.wait_frame_suspend_ms(WAYLAND_MAX_FRAME_TIME_US / 1000);
		if (!frame) {
			suspend_state = SuspendState::TIMEOUT;
		}
	}

	// If we suspended by capability, we'll know with this check. We must do this
	// after `wait_frame_suspend_ms` as it progressively dispatches the event queue
	// during the "timeout".
	if (wayland_thread.is_suspended()) {
		suspend_state = SuspendState::CAPABILITY;
	}

	if (suspend_state == SuspendState::TIMEOUT) {
		DEBUG_LOG_WAYLAND("Suspending. Reason: timeout.");
	} else if (suspend_state == SuspendState::CAPABILITY) {
		DEBUG_LOG_WAYLAND("Suspending. Reason: capability.");
	}
}

void DisplayServerWayland::process_events() {
	wayland_thread.mutex.lock();

	while (wayland_thread.has_message()) {
		Ref<WaylandThread::Message> msg = wayland_thread.pop_message();

		// Generic check. Not actual message handling.
		Ref<WaylandThread::WindowMessage> win_msg = msg;
		if (win_msg.is_valid()) {
			ERR_CONTINUE_MSG(win_msg->id == INVALID_WINDOW_ID, "Invalid window ID received from Wayland thread.");

			if (!windows.has(win_msg->id)) {
				// Window got probably deleted.
				continue;
			}
		}

		Ref<WaylandThread::WindowRectMessage> winrect_msg = msg;
		if (winrect_msg.is_valid()) {
			_update_window_rect(winrect_msg->rect, winrect_msg->id);
			continue;
		}

		Ref<WaylandThread::WindowEventMessage> winev_msg = msg;
		if (winev_msg.is_valid()) {
			_send_window_event(winev_msg->event, winev_msg->id);

			if (winev_msg->event == WINDOW_EVENT_FOCUS_IN) {
				if (OS::get_singleton()->get_main_loop()) {
					OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN);
				}
			} else if (winev_msg->event == WINDOW_EVENT_FOCUS_OUT) {
				if (OS::get_singleton()->get_main_loop()) {
					OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT);
				}
				Input::get_singleton()->release_pressed_events();
			}
			continue;
		}

		Ref<WaylandThread::InputEventMessage> inputev_msg = msg;
		if (inputev_msg.is_valid()) {
			Ref<InputEventMouseButton> mb = inputev_msg->event;

			bool handled = false;
			if (!popup_menu_list.is_empty() && mb.is_valid()) {
				// Popup menu handling.

				BitField<MouseButtonMask> mouse_mask = mb->get_button_mask();
				if (mouse_mask != last_mouse_monitor_mask && mb->is_pressed()) {
					List<WindowID>::Element *E = popup_menu_list.back();
					List<WindowID>::Element *C = nullptr;

					// Looking for the oldest popup to close.
					while (E) {
						WindowData &wd = windows[E->get()];
						Point2 global_pos = mb->get_position() + window_get_position(mb->get_window_id());
						if (wd.rect.has_point(global_pos)) {
							break;
						} else if (wd.safe_rect.has_point(global_pos)) {
							break;
						}

						C = E;
						E = E->prev();
					}

					if (C) {
						handled = true;
						_send_window_event(WINDOW_EVENT_CLOSE_REQUEST, C->get());
					}
				}

				last_mouse_monitor_mask = mouse_mask;
			}

			if (!handled) {
				Input::get_singleton()->parse_input_event(inputev_msg->event);
			}
			continue;
		}

		Ref<WaylandThread::DropFilesEventMessage> dropfiles_msg = msg;
		if (dropfiles_msg.is_valid()) {
			WindowData wd = windows[dropfiles_msg->id];

			if (wd.drop_files_callback.is_valid()) {
				Variant v_files = dropfiles_msg->files;
				const Variant *v_args[1] = { &v_files };
				Variant ret;
				Callable::CallError ce;
				wd.drop_files_callback.callp((const Variant **)&v_args, 1, ret, ce);
				if (ce.error != Callable::CallError::CALL_OK) {
					ERR_PRINT(vformat("Failed to execute drop files callback: %s.", Variant::get_callable_error_text(wd.drop_files_callback, v_args, 1, ce)));
				}
			}
			continue;
		}

		Ref<WaylandThread::IMECommitEventMessage> ime_commit_msg = msg;
		if (ime_commit_msg.is_valid()) {
			for (int i = 0; i < ime_commit_msg->text.length(); i++) {
				const char32_t codepoint = ime_commit_msg->text[i];

				Ref<InputEventKey> ke;
				ke.instantiate();
				ke->set_window_id(ime_commit_msg->id);
				ke->set_pressed(true);
				ke->set_echo(false);
				ke->set_keycode(Key::NONE);
				ke->set_physical_keycode(Key::NONE);
				ke->set_key_label(Key::NONE);
				ke->set_unicode(codepoint);

				Input::get_singleton()->parse_input_event(ke);
			}
			ime_text = String();
			ime_selection = Vector2i();

			OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
			continue;
		}

		Ref<WaylandThread::IMEUpdateEventMessage> ime_update_msg = msg;
		if (ime_update_msg.is_valid()) {
			if (ime_text != ime_update_msg->text || ime_selection != ime_update_msg->selection) {
				ime_text = ime_update_msg->text;
				ime_selection = ime_update_msg->selection;

				OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
			}
			continue;
		}
	}

	wayland_thread.keyboard_echo_keys();

	if (suspend_state == SuspendState::NONE) {
		// Due to the way legacy suspension works, we have to treat low processor
		// usage mode very differently than the regular one.
		if (OS::get_singleton()->is_in_low_processor_usage_mode()) {
			// NOTE: We must avoid committing a surface if we expect a new frame, as we
			// might otherwise commit some inconsistent data (e.g. buffer scale). Note
			// that if a new frame is expected it's going to be committed by the renderer
			// soon anyways.
			if (!RenderingServer::get_singleton()->has_changed()) {
				// We _can't_ commit in a different thread (such as in the frame callback
				// itself) because we would risk to step on the renderer's feet, which would
				// cause subtle but severe issues, such as crashes on setups with explicit
				// sync. This isn't normally a problem, as the renderer commits at every
				// frame (which is what we need for atomic surface updates anyways), but in
				// low processor usage mode that expectation is broken. When it's on, our
				// frame rate stops being constant. This also reflects in the frame
				// information we use for legacy suspension. In order to avoid issues, let's
				// manually commit all surfaces, so that we can get fresh frame data.
				wayland_thread.commit_surfaces();
				try_suspend();
			}
		} else {
			try_suspend();
		}
	} else {
		if (suspend_state == SuspendState::CAPABILITY) {
			// If we suspended by capability we can assume that it will be reset when
			// the compositor wants us to repaint.
			if (!wayland_thread.is_suspended()) {
				suspend_state = SuspendState::NONE;
				DEBUG_LOG_WAYLAND("Unsuspending from capability.");
			}
		} else if (suspend_state == SuspendState::TIMEOUT) {
			// Certain compositors might not report the "suspended" wm_capability flag.
			// Because of this we'll wake up at the next frame event, indicating the
			// desire for the compositor to let us repaint.
			if (wayland_thread.get_reset_frame()) {
				suspend_state = SuspendState::NONE;
				DEBUG_LOG_WAYLAND("Unsuspending from timeout.");
			}
		}
		wayland_thread.commit_surfaces();
	}

#ifdef DBUS_ENABLED
	if (portal_desktop) {
		portal_desktop->process_file_dialog_callbacks();
	}
#endif

	wayland_thread.mutex.unlock();

	Input::get_singleton()->flush_buffered_events();
}

void DisplayServerWayland::release_rendering_thread() {
#ifdef GLES3_ENABLED
	if (egl_manager) {
		egl_manager->release_current();
	}
#endif
}

void DisplayServerWayland::swap_buffers() {
#ifdef GLES3_ENABLED
	if (egl_manager) {
		egl_manager->swap_buffers();
	}
#endif
}

void DisplayServerWayland::set_context(Context p_context) {
	MutexLock mutex_lock(wayland_thread.mutex);

	DEBUG_LOG_WAYLAND(vformat("Setting context %d.", p_context));

	context = p_context;

	String app_id = _get_app_id_from_context(p_context);
	wayland_thread.window_set_app_id(MAIN_WINDOW_ID, app_id);
}

bool DisplayServerWayland::is_window_transparency_available() const {
#if defined(RD_ENABLED)
	if (rendering_device && !rendering_device->is_composite_alpha_supported()) {
		return false;
	}
#endif
	return OS::get_singleton()->is_layered_allowed();
}

Vector<String> DisplayServerWayland::get_rendering_drivers_func() {
	Vector<String> drivers;

#ifdef VULKAN_ENABLED
	drivers.push_back("vulkan");
#endif

#ifdef GLES3_ENABLED
	drivers.push_back("opengl3");
	drivers.push_back("opengl3_es");
#endif

	return drivers;
}

DisplayServer *DisplayServerWayland::create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Point2i *p_position, const Size2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	DisplayServer *ds = memnew(DisplayServerWayland(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_resolution, p_context, p_parent_window, r_error));
	if (r_error != OK) {
		ERR_PRINT("Can't create the Wayland display server.");
		memdelete(ds);

		return nullptr;
	}
	return ds;
}

DisplayServerWayland::DisplayServerWayland(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i &p_resolution, Context p_context, int64_t p_parent_window, Error &r_error) {
#ifdef GLES3_ENABLED
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif // DEBUG_ENABLED
#endif // SOWRAP_ENABLED
#endif // GLES3_ENABLED

	r_error = ERR_UNAVAILABLE;
	context = p_context;

	Error thread_err = wayland_thread.init();

	if (thread_err != OK) {
		r_error = thread_err;
		ERR_FAIL_MSG("Could not initialize the Wayland thread.");
	}

	// Input.
	Input::get_singleton()->set_event_dispatch_function(dispatch_input_events);

	native_menu = memnew(NativeMenu);

#ifdef SPEECHD_ENABLED
	// Init TTS
	tts = memnew(TTS_Linux);
#endif

	rendering_driver = p_rendering_driver;

	bool driver_found = false;
	String executable_name = OS::get_singleton()->get_executable_path().get_file();

#ifdef RD_ENABLED
#ifdef VULKAN_ENABLED
	if (rendering_driver == "vulkan") {
		rendering_context = memnew(RenderingContextDriverVulkanWayland);
	}
#endif // VULKAN_ENABLED

	if (rendering_context) {
		if (rendering_context->initialize() != OK) {
			memdelete(rendering_context);
			rendering_context = nullptr;
#if defined(GLES3_ENABLED)
			bool fallback_to_opengl3 = GLOBAL_GET("rendering/rendering_device/fallback_to_opengl3");
			if (fallback_to_opengl3 && rendering_driver != "opengl3") {
				WARN_PRINT("Your video card drivers seem not to support the required Vulkan version, switching to OpenGL 3.");
				rendering_driver = "opengl3";
				OS::get_singleton()->set_current_rendering_method("gl_compatibility");
				OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
			} else
#endif // GLES3_ENABLED
			{
				r_error = ERR_CANT_CREATE;

				if (p_rendering_driver == "vulkan") {
					OS::get_singleton()->alert(
							vformat("Your video card drivers seem not to support the required Vulkan version.\n\n"
									"If possible, consider updating your video card drivers or using the OpenGL 3 driver.\n\n"
									"You can enable the OpenGL 3 driver by starting the engine from the\n"
									"command line with the command:\n\n    \"%s\" --rendering-driver opengl3\n\n"
									"If you recently updated your video card drivers, try rebooting.",
									executable_name),
							"Unable to initialize Vulkan video driver");
				}

				ERR_FAIL_MSG(vformat("Could not initialize %s", rendering_driver));
			}
		}

		driver_found = true;
	}
#endif // RD_ENABLED

#ifdef GLES3_ENABLED
	if (rendering_driver == "opengl3" || rendering_driver == "opengl3_es") {
#ifdef SOWRAP_ENABLED
		if (initialize_wayland_egl(dylibloader_verbose) != 0) {
			WARN_PRINT("Can't load the Wayland EGL library.");
			return;
		}
#endif // SOWRAP_ENABLED

		if (getenv("DRI_PRIME") == nullptr) {
			int prime_idx = -1;

			if (getenv("PRIMUS_DISPLAY") ||
					getenv("PRIMUS_libGLd") ||
					getenv("PRIMUS_libGLa") ||
					getenv("PRIMUS_libGL") ||
					getenv("PRIMUS_LOAD_GLOBAL") ||
					getenv("BUMBLEBEE_SOCKET") ||
					getenv("__NV_PRIME_RENDER_OFFLOAD")) {
				print_verbose("Optirun/primusrun detected. Skipping GPU detection");
				prime_idx = 0;
			}

			// Some tools use fake libGL libraries and have them override the real one using
			// LD_LIBRARY_PATH, so we skip them. *But* Steam also sets LD_LIBRARY_PATH for its
			// runtime and includes system `/lib` and `/lib64`... so ignore Steam.
			if (prime_idx == -1 && getenv("LD_LIBRARY_PATH") && !getenv("STEAM_RUNTIME_LIBRARY_PATH")) {
				String ld_library_path(getenv("LD_LIBRARY_PATH"));
				Vector<String> libraries = ld_library_path.split(":");

				for (int i = 0; i < libraries.size(); ++i) {
					if (FileAccess::exists(libraries[i] + "/libGL.so.1") ||
							FileAccess::exists(libraries[i] + "/libGL.so")) {
						print_verbose("Custom libGL override detected. Skipping GPU detection");
						prime_idx = 0;
					}
				}
			}

			if (prime_idx == -1) {
				print_verbose("Detecting GPUs, set DRI_PRIME in the environment to override GPU detection logic.");
				prime_idx = DetectPrimeEGL::detect_prime(EGL_PLATFORM_WAYLAND_KHR);
			}

			if (prime_idx) {
				print_line(vformat("Found discrete GPU, setting DRI_PRIME=%d to use it.", prime_idx));
				print_line("Note: Set DRI_PRIME=0 in the environment to disable Godot from using the discrete GPU.");
				setenv("DRI_PRIME", itos(prime_idx).utf8().ptr(), 1);
			}
		}

		if (rendering_driver == "opengl3") {
			egl_manager = memnew(EGLManagerWayland);

			if (egl_manager->initialize(wayland_thread.get_wl_display()) != OK || egl_manager->open_display(wayland_thread.get_wl_display()) != OK) {
				memdelete(egl_manager);
				egl_manager = nullptr;

				bool fallback = GLOBAL_GET("rendering/gl_compatibility/fallback_to_gles");
				if (fallback) {
					WARN_PRINT("Your video card drivers seem not to support the required OpenGL version, switching to OpenGLES.");
					rendering_driver = "opengl3_es";
					OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
				} else {
					r_error = ERR_UNAVAILABLE;

					OS::get_singleton()->alert(
							vformat("Your video card drivers seem not to support the required OpenGL 3.3 version.\n\n"
									"If possible, consider updating your video card drivers or using the Vulkan driver.\n\n"
									"You can enable the Vulkan driver by starting the engine from the\n"
									"command line with the command:\n\n    \"%s\" --rendering-driver vulkan\n\n"
									"If you recently updated your video card drivers, try rebooting.",
									executable_name),
							"Unable to initialize OpenGL video driver");

					ERR_FAIL_MSG("Could not initialize OpenGL.");
				}
			} else {
				RasterizerGLES3::make_current(true);
				driver_found = true;
			}
		}

		if (rendering_driver == "opengl3_es") {
			egl_manager = memnew(EGLManagerWaylandGLES);

			if (egl_manager->initialize(wayland_thread.get_wl_display()) != OK || egl_manager->open_display(wayland_thread.get_wl_display()) != OK) {
				memdelete(egl_manager);
				egl_manager = nullptr;
				r_error = ERR_CANT_CREATE;

				OS::get_singleton()->alert(
						vformat("Your video card drivers seem not to support the required OpenGL ES 3.0 version.\n\n"
								"If possible, consider updating your video card drivers or using the Vulkan driver.\n\n"
								"You can enable the Vulkan driver by starting the engine from the\n"
								"command line with the command:\n\n    \"%s\" --rendering-driver vulkan\n\n"
								"If you recently updated your video card drivers, try rebooting.",
								executable_name),
						"Unable to initialize OpenGL ES video driver");

				ERR_FAIL_MSG("Could not initialize OpenGL ES.");
			}

			RasterizerGLES3::make_current(false);
			driver_found = true;
		}
	}
#endif // GLES3_ENABLED

	if (!driver_found) {
		r_error = ERR_UNAVAILABLE;
		ERR_FAIL_MSG("Video driver not found.");
	}

	cursor_set_shape(CURSOR_BUSY);

	WindowData &wd = windows[MAIN_WINDOW_ID];

	wd.id = MAIN_WINDOW_ID;
	wd.mode = p_mode;
	wd.flags = p_flags;
	wd.vsync_mode = p_vsync_mode;
	wd.rect.size = p_resolution;
	wd.title = "Godot";

	show_window(MAIN_WINDOW_ID);

#ifdef RD_ENABLED
	if (rendering_context) {
		rendering_device = memnew(RenderingDevice);
		if (rendering_device->initialize(rendering_context, MAIN_WINDOW_ID) != OK) {
			memdelete(rendering_device);
			rendering_device = nullptr;
			memdelete(rendering_context);
			rendering_context = nullptr;
			r_error = ERR_UNAVAILABLE;
			return;
		}
		rendering_device->screen_create(MAIN_WINDOW_ID);

		RendererCompositorRD::make_current();
	}
#endif // RD_ENABLED

#ifdef DBUS_ENABLED
	portal_desktop = memnew(FreeDesktopPortalDesktop);
	screensaver = memnew(FreeDesktopScreenSaver);
#endif // DBUS_ENABLED

	screen_set_keep_on(GLOBAL_GET("display/window/energy_saving/keep_screen_on"));

	r_error = OK;
}

DisplayServerWayland::~DisplayServerWayland() {
	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}

	// Iterating on the window map while we delete stuff from it is a bit
	// uncomfortable, plus we can't even delete /all/ windows in an arbitrary order
	// (due to popups).
	List<WindowID> toplevels;

	for (const KeyValue<WindowID, WindowData> &pair : windows) {
		WindowID id = pair.key;

		if (!window_get_flag(WINDOW_FLAG_POPUP_WM_HINT, id)) {
			toplevels.push_back(id);
		}
	}

	for (WindowID &id : toplevels) {
		delete_sub_window(id);
	}

	wayland_thread.destroy();

	// Destroy all drivers.
#ifdef RD_ENABLED
	if (rendering_device) {
		memdelete(rendering_device);
	}

	if (rendering_context) {
		memdelete(rendering_context);
	}
#endif

#ifdef SPEECHD_ENABLED
	if (tts) {
		memdelete(tts);
	}
#endif

#ifdef DBUS_ENABLED
	if (portal_desktop) {
		memdelete(portal_desktop);
		memdelete(screensaver);
	}
#endif
}

void DisplayServerWayland::register_wayland_driver() {
	register_create_function("wayland", create_func, get_rendering_drivers_func);
}

#endif //WAYLAND_ENABLED
