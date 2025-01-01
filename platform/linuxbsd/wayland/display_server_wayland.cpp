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

void DisplayServerWayland::_send_window_event(WindowEvent p_event) {
	WindowData &wd = main_window;

	if (wd.window_event_callback.is_valid()) {
		Variant event = int(p_event);
		wd.window_event_callback.call(event);
	}
}

void DisplayServerWayland::dispatch_input_events(const Ref<InputEvent> &p_event) {
	((DisplayServerWayland *)(get_singleton()))->_dispatch_input_event(p_event);
}

void DisplayServerWayland::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	Callable callable = main_window.input_event_callback;
	if (callable.is_valid()) {
		callable.call(p_event);
	}
}

void DisplayServerWayland::_resize_window(const Size2i &p_size) {
	WindowData &wd = main_window;

	wd.rect.size = p_size;

#ifdef RD_ENABLED
	if (wd.visible && rendering_context) {
		rendering_context->window_set_size(MAIN_WINDOW_ID, wd.rect.size.width, wd.rect.size.height);
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

void DisplayServerWayland::_show_window() {
	MutexLock mutex_lock(wayland_thread.mutex);

	WindowData &wd = main_window;

	if (!wd.visible) {
		DEBUG_LOG_WAYLAND("Showing window.");

		// Showing this window will reset its mode with whatever the compositor
		// reports. We'll save the mode beforehand so that we can reapply it later.
		// TODO: Fix/Port/Move/Whatever to `WaylandThread` APIs.
		WindowMode setup_mode = wd.mode;

		wayland_thread.window_create(MAIN_WINDOW_ID, wd.rect.size.width, wd.rect.size.height);
		wayland_thread.window_set_min_size(MAIN_WINDOW_ID, wd.min_size);
		wayland_thread.window_set_max_size(MAIN_WINDOW_ID, wd.max_size);
		wayland_thread.window_set_app_id(MAIN_WINDOW_ID, _get_app_id_from_context(context));
		wayland_thread.window_set_borderless(MAIN_WINDOW_ID, window_get_flag(WINDOW_FLAG_BORDERLESS));

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
			rendering_context->window_set_vsync_mode(wd.id, wd.vsync_mode);

			emulate_vsync = (rendering_context->window_get_vsync_mode(wd.id) == DisplayServer::VSYNC_ENABLED);

			if (emulate_vsync) {
				print_verbose("VSYNC: manually throttling frames using MAILBOX.");
				rendering_context->window_set_vsync_mode(wd.id, DisplayServer::VSYNC_MAILBOX);
			}
		}
#endif

#ifdef GLES3_ENABLED
		if (egl_manager) {
			struct wl_surface *wl_surface = wayland_thread.window_get_wl_surface(wd.id);
			wd.wl_egl_window = wl_egl_window_create(wl_surface, wd.rect.size.width, wd.rect.size.height);

			Error err = egl_manager->window_create(MAIN_WINDOW_ID, wayland_thread.get_wl_display(), wd.wl_egl_window, wd.rect.size.width, wd.rect.size.height);
			ERR_FAIL_COND_MSG(err == ERR_CANT_CREATE, "Can't show a GLES3 window.");

			window_set_vsync_mode(wd.vsync_mode, MAIN_WINDOW_ID);
		}
#endif
		// NOTE: The public window-handling methods might depend on this flag being
		// set. Ensure to not make any of these calls before this assignment.
		wd.visible = true;

		// Actually try to apply the window's mode now that it's visible.
		window_set_mode(setup_mode);

		wayland_thread.window_set_title(MAIN_WINDOW_ID, wd.title);
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
		case FEATURE_CLIPBOARD_PRIMARY: {
			return true;
		} break;

		//case FEATURE_NATIVE_DIALOG:
		//case FEATURE_NATIVE_DIALOG_INPUT:
#ifdef DBUS_ENABLED
		case FEATURE_NATIVE_DIALOG_FILE:
		case FEATURE_NATIVE_DIALOG_FILE_EXTRA: {
			return true;
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
	return portal_desktop->is_supported();
}

bool DisplayServerWayland::is_dark_mode() const {
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
	WindowID window_id = MAIN_WINDOW_ID;
	// TODO: Use window IDs for multiwindow support.

	WaylandThread::WindowState *ws = wayland_thread.wl_surface_get_window_state(wayland_thread.window_get_wl_surface(window_id));
	return portal_desktop->file_dialog_show(window_id, (ws ? ws->exported_handle : String()), p_title, p_current_directory, String(), p_filename, p_mode, p_filters, TypedArray<Dictionary>(), p_callback, false);
}

Error DisplayServerWayland::file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback) {
	WindowID window_id = MAIN_WINDOW_ID;
	// TODO: Use window IDs for multiwindow support.

	WaylandThread::WindowState *ws = wayland_thread.wl_surface_get_window_state(wayland_thread.window_get_wl_surface(window_id));
	return portal_desktop->file_dialog_show(window_id, (ws ? ws->exported_handle : String()), p_title, p_current_directory, p_root, p_filename, p_mode, p_filters, p_options, p_callback, true);
}

#endif

void DisplayServerWayland::beep() const {
	wayland_thread.beep();
}

void DisplayServerWayland::mouse_set_mode(MouseMode p_mode) {
	if (p_mode == mouse_mode) {
		return;
	}

	MutexLock mutex_lock(wayland_thread.mutex);

	bool show_cursor = (p_mode == MOUSE_MODE_VISIBLE || p_mode == MOUSE_MODE_CONFINED);

	wayland_thread.cursor_set_visible(show_cursor);

	WaylandThread::PointerConstraint constraint = WaylandThread::PointerConstraint::NONE;

	switch (p_mode) {
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

	mouse_mode = p_mode;
}

DisplayServerWayland::MouseMode DisplayServerWayland::mouse_get_mode() const {
	return mouse_mode;
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
#ifdef DBUS_ENABLED
	return wayland_thread.window_get_idle_inhibition(MAIN_WINDOW_ID) || screensaver_inhibited;
#else
	return wayland_thread.window_get_idle_inhibition(MAIN_WINDOW_ID);
#endif
}

Vector<DisplayServer::WindowID> DisplayServerWayland::get_window_list() const {
	MutexLock mutex_lock(wayland_thread.mutex);

	Vector<int> ret;
	ret.push_back(MAIN_WINDOW_ID);

	return ret;
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

	main_window.instance_id = p_instance;
}

ObjectID DisplayServerWayland::window_get_attached_instance_id(WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return main_window.instance_id;
}

void DisplayServerWayland::window_set_title(const String &p_title, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	WindowData &wd = main_window;

	wd.title = p_title;

	wayland_thread.window_set_title(MAIN_WINDOW_ID, wd.title);
}

void DisplayServerWayland::window_set_mouse_passthrough(const Vector<Vector2> &p_region, DisplayServer::WindowID p_window_id) {
	// TODO
	DEBUG_LOG_WAYLAND(vformat("wayland stub window_set_mouse_passthrough region %s", p_region));
}

void DisplayServerWayland::window_set_rect_changed_callback(const Callable &p_callable, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	main_window.rect_changed_callback = p_callable;
}

void DisplayServerWayland::window_set_window_event_callback(const Callable &p_callable, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	main_window.window_event_callback = p_callable;
}

void DisplayServerWayland::window_set_input_event_callback(const Callable &p_callable, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	main_window.input_event_callback = p_callable;
}

void DisplayServerWayland::window_set_input_text_callback(const Callable &p_callable, WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	main_window.input_text_callback = p_callable;
}

void DisplayServerWayland::window_set_drop_files_callback(const Callable &p_callable, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	main_window.drop_files_callback = p_callable;
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

	// We can't know the position of toplevels with the standard protocol.
	return Point2i();
}

Point2i DisplayServerWayland::window_get_position_with_decorations(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	// We can't know the position of toplevels with the standard protocol, nor can
	// we get information about the decorations, at least with SSDs.
	return Point2i();
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

	WindowData &wd = main_window;

	// FIXME: Is `p_size.x < wd.min_size.x || p_size.y < wd.min_size.y` == `p_size < wd.min_size`?
	if ((p_size != Size2i()) && ((p_size.x < wd.min_size.x) || (p_size.y < wd.min_size.y))) {
		ERR_PRINT("Maximum window size can't be smaller than minimum window size!");
		return;
	}

	wd.max_size = p_size;

	wayland_thread.window_set_max_size(MAIN_WINDOW_ID, p_size);
}

Size2i DisplayServerWayland::window_get_max_size(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return main_window.max_size;
}

void DisplayServerWayland::gl_window_make_current(DisplayServer::WindowID p_window_id) {
#ifdef GLES3_ENABLED
	if (egl_manager) {
		egl_manager->window_make_current(MAIN_WINDOW_ID);
	}
#endif
}

void DisplayServerWayland::window_set_transient(WindowID p_window_id, WindowID p_parent) {
	// Currently unsupported.
}

void DisplayServerWayland::window_set_min_size(const Size2i p_size, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	DEBUG_LOG_WAYLAND(vformat("window minsize set to %s", p_size));

	WindowData &wd = main_window;

	if (p_size.x < 0 || p_size.y < 0) {
		ERR_FAIL_MSG("Minimum window size can't be negative!");
	}

	// FIXME: Is `p_size.x > wd.max_size.x || p_size.y > wd.max_size.y` == `p_size > wd.max_size`?
	if ((p_size != Size2i()) && (wd.max_size != Size2i()) && ((p_size.x > wd.max_size.x) || (p_size.y > wd.max_size.y))) {
		ERR_PRINT("Minimum window size can't be larger than maximum window size!");
		return;
	}

	wd.min_size = p_size;

	wayland_thread.window_set_min_size(MAIN_WINDOW_ID, p_size);
}

Size2i DisplayServerWayland::window_get_min_size(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return main_window.min_size;
}

void DisplayServerWayland::window_set_size(const Size2i p_size, DisplayServer::WindowID p_window_id) {
	// The XDG spec doesn't allow non-interactive resizes.
}

Size2i DisplayServerWayland::window_get_size(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	return main_window.rect.size;
}

Size2i DisplayServerWayland::window_get_size_with_decorations(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	// I don't think there's a way of actually knowing the size of the window
	// decoration in Wayland, at least in the case of SSDs, nor that it would be
	// that useful in this case. We'll just return the main window's size.
	return main_window.rect.size;
}

void DisplayServerWayland::window_set_mode(WindowMode p_mode, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	WindowData &wd = main_window;

	if (!wd.visible) {
		return;
	}

	wayland_thread.window_try_set_mode(p_window_id, p_mode);
}

DisplayServer::WindowMode DisplayServerWayland::window_get_mode(DisplayServer::WindowID p_window_id) const {
	MutexLock mutex_lock(wayland_thread.mutex);

	const WindowData &wd = main_window;

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

	WindowData &wd = main_window;

	DEBUG_LOG_WAYLAND(vformat("Window set flag %d", p_flag));

	switch (p_flag) {
		case WINDOW_FLAG_BORDERLESS: {
			wayland_thread.window_set_borderless(MAIN_WINDOW_ID, p_enabled);
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

	return main_window.flags & (1 << p_flag);
}

void DisplayServerWayland::window_request_attention(DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	DEBUG_LOG_WAYLAND("Requested attention.");

	wayland_thread.window_request_attention(MAIN_WINDOW_ID);
}

void DisplayServerWayland::window_move_to_foreground(DisplayServer::WindowID p_window_id) {
	// Standard Wayland APIs don't support this.
}

bool DisplayServerWayland::window_is_focused(WindowID p_window_id) const {
	return wayland_thread.pointer_get_pointed_window_id() == p_window_id;
}

bool DisplayServerWayland::window_can_draw(DisplayServer::WindowID p_window_id) const {
	return !suspended;
}

bool DisplayServerWayland::can_any_window_draw() const {
	return !suspended;
}

void DisplayServerWayland::window_set_ime_active(const bool p_active, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	wayland_thread.window_set_ime_active(p_active, MAIN_WINDOW_ID);
}

void DisplayServerWayland::window_set_ime_position(const Point2i &p_pos, DisplayServer::WindowID p_window_id) {
	MutexLock mutex_lock(wayland_thread.mutex);

	wayland_thread.window_set_ime_position(p_pos, MAIN_WINDOW_ID);
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

#ifdef RD_ENABLED
	if (rendering_context) {
		rendering_context->window_set_vsync_mode(p_window_id, p_vsync_mode);

		emulate_vsync = (rendering_context->window_get_vsync_mode(p_window_id) == DisplayServer::VSYNC_ENABLED);

		if (emulate_vsync) {
			print_verbose("VSYNC: manually throttling frames using MAILBOX.");
			rendering_context->window_set_vsync_mode(p_window_id, DisplayServer::VSYNC_MAILBOX);
		}
	}
#endif // VULKAN_ENABLED

#ifdef GLES3_ENABLED
	if (egl_manager) {
		egl_manager->set_use_vsync(p_vsync_mode != DisplayServer::VSYNC_DISABLED);

		emulate_vsync = egl_manager->is_using_vsync();

		if (emulate_vsync) {
			print_verbose("VSYNC: manually throttling frames with swap delay 0.");
			egl_manager->set_use_vsync(false);
		}
	}
#endif // GLES3_ENABLED
}

DisplayServer::VSyncMode DisplayServerWayland::window_get_vsync_mode(DisplayServer::WindowID p_window_id) const {
	if (emulate_vsync) {
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
			if (cursor_c->value.rid == p_cursor->get_rid() && cursor_c->value.hotspot == p_hotspot) {
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

		cursor.rid = p_cursor->get_rid();
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
	// Due to various reasons, we manually handle display synchronization by
	// waiting for a frame event (request to draw) or, if available, the actual
	// window's suspend status. When a window is suspended, we can avoid drawing
	// altogether, either because the compositor told us that we don't need to or
	// because the pace of the frame events became unreliable.
	if (emulate_vsync) {
		bool frame = wayland_thread.wait_frame_suspend_ms(1000);
		if (!frame) {
			suspended = true;
		}
	} else {
		if (wayland_thread.is_suspended()) {
			suspended = true;
		}
	}

	if (suspended) {
		DEBUG_LOG_WAYLAND("Window suspended.");
	}
}

void DisplayServerWayland::process_events() {
	wayland_thread.mutex.lock();

	while (wayland_thread.has_message()) {
		Ref<WaylandThread::Message> msg = wayland_thread.pop_message();

		Ref<WaylandThread::WindowRectMessage> winrect_msg = msg;
		if (winrect_msg.is_valid()) {
			_resize_window(winrect_msg->rect.size);
		}

		Ref<WaylandThread::WindowEventMessage> winev_msg = msg;
		if (winev_msg.is_valid()) {
			_send_window_event(winev_msg->event);

			if (winev_msg->event == WINDOW_EVENT_FOCUS_IN) {
				if (OS::get_singleton()->get_main_loop()) {
					OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN);
				}
			} else if (winev_msg->event == WINDOW_EVENT_FOCUS_OUT) {
				if (OS::get_singleton()->get_main_loop()) {
					OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT);
				}
			}
		}

		Ref<WaylandThread::InputEventMessage> inputev_msg = msg;
		if (inputev_msg.is_valid()) {
			Input::get_singleton()->parse_input_event(inputev_msg->event);
		}

		Ref<WaylandThread::DropFilesEventMessage> dropfiles_msg = msg;
		if (dropfiles_msg.is_valid()) {
			WindowData wd = main_window;

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
		}

		Ref<WaylandThread::IMECommitEventMessage> ime_commit_msg = msg;
		if (ime_commit_msg.is_valid()) {
			for (int i = 0; i < ime_commit_msg->text.length(); i++) {
				const char32_t codepoint = ime_commit_msg->text[i];

				Ref<InputEventKey> ke;
				ke.instantiate();
				ke->set_window_id(MAIN_WINDOW_ID);
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
		}

		Ref<WaylandThread::IMEUpdateEventMessage> ime_update_msg = msg;
		if (ime_update_msg.is_valid()) {
			ime_text = ime_update_msg->text;
			ime_selection = ime_update_msg->selection;

			OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
		}
	}

	wayland_thread.keyboard_echo_keys();

	if (!suspended) {
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
	} else if (!wayland_thread.is_suspended() || wayland_thread.get_reset_frame()) {
		// At last, a sign of life! We're no longer suspended.
		suspended = false;
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

	WindowData &wd = main_window;

	wd.id = MAIN_WINDOW_ID;
	wd.mode = p_mode;
	wd.flags = p_flags;
	wd.vsync_mode = p_vsync_mode;
	wd.rect.size = p_resolution;
	wd.title = "Godot";

	_show_window();

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
	// TODO: Multiwindow support.

	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}

	if (main_window.visible) {
#ifdef VULKAN_ENABLED
		if (rendering_device) {
			rendering_device->screen_free(MAIN_WINDOW_ID);
		}

		if (rendering_context) {
			rendering_context->window_destroy(MAIN_WINDOW_ID);
		}
#endif

#ifdef GLES3_ENABLED
		if (egl_manager) {
			egl_manager->window_destroy(MAIN_WINDOW_ID);
		}
#endif
	}

#ifdef GLES3_ENABLED
	if (main_window.wl_egl_window) {
		wl_egl_window_destroy(main_window.wl_egl_window);
	}
#endif

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
