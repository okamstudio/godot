/**************************************************************************/
/*  display_server_android.cpp                                            */
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

#include "display_server_android.h"

#include "java_godot_io_wrapper.h"
#include "java_godot_wrapper.h"
#include "os_android.h"
#include "tts_android.h"

#include "core/config/project_settings.h"

#if defined(RD_ENABLED)
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/rendering_device.h"

#if defined(VULKAN_ENABLED)
#include "rendering_context_driver_vulkan_android.h"
#endif
#endif

#ifdef GLES3_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"

#include <EGL/egl.h>
#endif

DisplayServerAndroid *DisplayServerAndroid::get_singleton() {
	return static_cast<DisplayServerAndroid *>(DisplayServer::get_singleton());
}

bool DisplayServerAndroid::has_feature(Feature p_feature) const {
	switch (p_feature) {
#ifndef DISABLE_DEPRECATED
		case FEATURE_GLOBAL_MENU: {
			return (native_menu && native_menu->has_feature(NativeMenu::FEATURE_GLOBAL_MENU));
		} break;
#endif
		case FEATURE_CURSOR_SHAPE:
		//case FEATURE_CUSTOM_CURSOR_SHAPE:
		//case FEATURE_HIDPI:
		//case FEATURE_ICON:
		//case FEATURE_IME:
		case FEATURE_MOUSE:
		//case FEATURE_MOUSE_WARP:
		//case FEATURE_NATIVE_DIALOG:
		case FEATURE_NATIVE_DIALOG_INPUT:
		case FEATURE_NATIVE_DIALOG_FILE:
		//case FEATURE_NATIVE_DIALOG_FILE_EXTRA:
		case FEATURE_NATIVE_DIALOG_FILE_MIME:
		//case FEATURE_NATIVE_ICON:
		//case FEATURE_WINDOW_TRANSPARENCY:
		case FEATURE_CLIPBOARD:
		case FEATURE_KEEP_SCREEN_ON:
		case FEATURE_ORIENTATION:
		case FEATURE_TOUCHSCREEN:
		case FEATURE_VIRTUAL_KEYBOARD:
		case FEATURE_TEXT_TO_SPEECH:
			return true;
		default:
			return false;
	}
}

String DisplayServerAndroid::get_name() const {
	return "Android";
}

bool DisplayServerAndroid::tts_is_speaking() const {
	return TTS_Android::is_speaking();
}

bool DisplayServerAndroid::tts_is_paused() const {
	return TTS_Android::is_paused();
}

TypedArray<Dictionary> DisplayServerAndroid::tts_get_voices() const {
	return TTS_Android::get_voices();
}

void DisplayServerAndroid::tts_speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	TTS_Android::speak(p_text, p_voice, p_volume, p_pitch, p_rate, p_utterance_id, p_interrupt);
}

void DisplayServerAndroid::tts_pause() {
	TTS_Android::pause();
}

void DisplayServerAndroid::tts_resume() {
	TTS_Android::resume();
}

void DisplayServerAndroid::tts_stop() {
	TTS_Android::stop();
}

bool DisplayServerAndroid::is_dark_mode_supported() const {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL_V(godot_java, false);

	return godot_java->is_dark_mode_supported();
}

bool DisplayServerAndroid::is_dark_mode() const {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL_V(godot_java, false);

	return godot_java->is_dark_mode();
}

void DisplayServerAndroid::set_system_theme_change_callback(const Callable &p_callable) {
	system_theme_changed = p_callable;
}

void DisplayServerAndroid::emit_system_theme_changed() {
	if (system_theme_changed.is_valid()) {
		system_theme_changed.call_deferred();
	}
}

void DisplayServerAndroid::clipboard_set(const String &p_text) {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL(godot_java);

	if (godot_java->has_set_clipboard()) {
		godot_java->set_clipboard(p_text);
	} else {
		DisplayServer::clipboard_set(p_text);
	}
}

String DisplayServerAndroid::clipboard_get() const {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL_V(godot_java, String());

	if (godot_java->has_get_clipboard()) {
		return godot_java->get_clipboard();
	} else {
		return DisplayServer::clipboard_get();
	}
}

bool DisplayServerAndroid::clipboard_has() const {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL_V(godot_java, false);

	if (godot_java->has_has_clipboard()) {
		return godot_java->has_clipboard();
	} else {
		return DisplayServer::clipboard_has();
	}
}

Error DisplayServerAndroid::dialog_input_text(String p_title, String p_description, String p_partial, const Callable &p_callback) {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL_V(godot_java, FAILED);
	input_dialog_callback = p_callback;
	return godot_java->show_input_dialog(p_title, p_description, p_partial);
}

void DisplayServerAndroid::emit_input_dialog_callback(String p_text) {
	if (input_dialog_callback.is_valid()) {
		input_dialog_callback.call_deferred(p_text);
	}
}

Error DisplayServerAndroid::file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback) {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL_V(godot_java, FAILED);
	file_picker_callback = p_callback;
	return godot_java->show_file_picker(p_current_directory, p_filename, p_mode, p_filters);
}

void DisplayServerAndroid::emit_file_picker_callback(bool p_ok, const Vector<String> &p_selected_paths) {
	if (file_picker_callback.is_valid()) {
		file_picker_callback.call_deferred(p_ok, p_selected_paths, 0);
	}
}

Color DisplayServerAndroid::get_accent_color() const {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL_V(godot_java, Color(0, 0, 0, 0));
	return godot_java->get_accent_color();
}

Color DisplayServerAndroid::get_base_color() const {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL_V(godot_java, Color(0, 0, 0, 0));
	return godot_java->get_base_color();
}

TypedArray<Rect2> DisplayServerAndroid::get_display_cutouts() const {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL_V(godot_io_java, Array());
	return godot_io_java->get_display_cutouts();
}

Rect2i DisplayServerAndroid::get_display_safe_area() const {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL_V(godot_io_java, Rect2i());
	return godot_io_java->get_display_safe_area();
}

void DisplayServerAndroid::screen_set_keep_on(bool p_enable) {
	GodotJavaWrapper *godot_java = OS_Android::get_singleton()->get_godot_java();
	ERR_FAIL_NULL(godot_java);

	godot_java->set_keep_screen_on(p_enable);
	keep_screen_on = p_enable;
}

bool DisplayServerAndroid::screen_is_kept_on() const {
	return keep_screen_on;
}

void DisplayServerAndroid::screen_set_orientation(DisplayServer::ScreenOrientation p_orientation, int p_screen) {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL(godot_io_java);

	godot_io_java->set_screen_orientation(p_orientation);
}

DisplayServer::ScreenOrientation DisplayServerAndroid::screen_get_orientation(int p_screen) const {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL_V(godot_io_java, SCREEN_LANDSCAPE);

	const int orientation = godot_io_java->get_screen_orientation();
	ERR_FAIL_INDEX_V_MSG(orientation, 7, SCREEN_LANDSCAPE, "Unrecognized screen orientation");
	return (ScreenOrientation)orientation;
}

int DisplayServerAndroid::get_screen_count() const {
	return 1;
}

int DisplayServerAndroid::get_primary_screen() const {
	return 0;
}

Point2i DisplayServerAndroid::screen_get_position(int p_screen) const {
	return Point2i(0, 0);
}

Size2i DisplayServerAndroid::screen_get_size(int p_screen) const {
	return OS_Android::get_singleton()->get_display_size();
}

Rect2i DisplayServerAndroid::screen_get_usable_rect(int p_screen) const {
	Size2i display_size = OS_Android::get_singleton()->get_display_size();
	return Rect2i(0, 0, display_size.width, display_size.height);
}

int DisplayServerAndroid::screen_get_dpi(int p_screen) const {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL_V(godot_io_java, 0);

	return godot_io_java->get_screen_dpi();
}

float DisplayServerAndroid::screen_get_scale(int p_screen) const {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL_V(godot_io_java, 1.0f);

	float screen_scale = godot_io_java->get_scaled_density();

	// Update the scale to avoid cropping.
	Size2i screen_size = screen_get_size(p_screen);
	if (screen_size != Size2i()) {
		float width_scale = screen_size.width / (float)OS_Android::DEFAULT_WINDOW_WIDTH;
		float height_scale = screen_size.height / (float)OS_Android::DEFAULT_WINDOW_HEIGHT;
		screen_scale = MIN(screen_scale, MIN(width_scale, height_scale));
	}

	return screen_scale;
}

float DisplayServerAndroid::screen_get_refresh_rate(int p_screen) const {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	if (!godot_io_java) {
		ERR_PRINT("An error occurred while trying to get the screen refresh rate.");
		return SCREEN_REFRESH_RATE_FALLBACK;
	}

	return godot_io_java->get_screen_refresh_rate(SCREEN_REFRESH_RATE_FALLBACK);
}

bool DisplayServerAndroid::is_touchscreen_available() const {
	return true;
}

void DisplayServerAndroid::virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect, VirtualKeyboardType p_type, int p_max_length, int p_cursor_start, int p_cursor_end) {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL(godot_io_java);

	if (godot_io_java->has_vk()) {
		godot_io_java->show_vk(p_existing_text, (int)p_type, p_max_length, p_cursor_start, p_cursor_end);
	} else {
		ERR_PRINT("Virtual keyboard not available");
	}
}

void DisplayServerAndroid::virtual_keyboard_hide() {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL(godot_io_java);

	if (godot_io_java->has_vk()) {
		godot_io_java->hide_vk();
	} else {
		ERR_PRINT("Virtual keyboard not available");
	}
}

int DisplayServerAndroid::virtual_keyboard_get_height() const {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL_V(godot_io_java, 0);

	return godot_io_java->get_vk_height();
}

bool DisplayServerAndroid::has_hardware_keyboard() const {
	GodotIOJavaWrapper *godot_io_java = OS_Android::get_singleton()->get_godot_io_java();
	ERR_FAIL_NULL_V(godot_io_java, false);

	return godot_io_java->has_hardware_keyboard();
}

void DisplayServerAndroid::window_set_window_event_callback(const Callable &p_callable, DisplayServer::WindowID p_window) {
	window_event_callback = p_callable;
}

void DisplayServerAndroid::window_set_input_event_callback(const Callable &p_callable, DisplayServer::WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerAndroid::window_set_input_text_callback(const Callable &p_callable, DisplayServer::WindowID p_window) {
	input_text_callback = p_callable;
}

void DisplayServerAndroid::window_set_rect_changed_callback(const Callable &p_callable, DisplayServer::WindowID p_window) {
	rect_changed_callback = p_callable;
}

void DisplayServerAndroid::window_set_drop_files_callback(const Callable &p_callable, DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

void DisplayServerAndroid::_window_callback(const Callable &p_callable, const Variant &p_arg, bool p_deferred) const {
	if (p_callable.is_valid()) {
		if (p_deferred) {
			p_callable.call_deferred(p_arg);
		} else {
			p_callable.call(p_arg);
		}
	}
}

void DisplayServerAndroid::send_window_event(DisplayServer::WindowEvent p_event, bool p_deferred) const {
	_window_callback(window_event_callback, int(p_event), p_deferred);
}

void DisplayServerAndroid::send_input_event(const Ref<InputEvent> &p_event) const {
	_window_callback(input_event_callback, p_event);
}

void DisplayServerAndroid::send_input_text(const String &p_text) const {
	_window_callback(input_text_callback, p_text);
}

void DisplayServerAndroid::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	DisplayServerAndroid::get_singleton()->send_input_event(p_event);
}

Vector<DisplayServer::WindowID> DisplayServerAndroid::get_window_list() const {
	Vector<WindowID> ret;
	ret.push_back(MAIN_WINDOW_ID);
	return ret;
}

DisplayServer::WindowID DisplayServerAndroid::get_window_at_screen_position(const Point2i &p_position) const {
	return MAIN_WINDOW_ID;
}

int64_t DisplayServerAndroid::window_get_native_handle(HandleType p_handle_type, WindowID p_window) const {
	ERR_FAIL_COND_V(p_window != MAIN_WINDOW_ID, 0);
	switch (p_handle_type) {
		case WINDOW_HANDLE: {
			return reinterpret_cast<int64_t>(static_cast<OS_Android *>(OS::get_singleton())->get_godot_java()->get_activity());
		}
		case WINDOW_VIEW: {
			return 0; // Not supported.
		}
#ifdef GLES3_ENABLED
		case DISPLAY_HANDLE: {
			if (rendering_driver == "opengl3") {
				return reinterpret_cast<int64_t>(eglGetCurrentDisplay());
			}
			return 0;
		}
		case OPENGL_CONTEXT: {
			if (rendering_driver == "opengl3") {
				return reinterpret_cast<int64_t>(eglGetCurrentContext());
			}
			return 0;
		}
		case EGL_DISPLAY: {
			// @todo Find a way to get this from the Java side.
			return 0;
		}
		case EGL_CONFIG: {
			// @todo Find a way to get this from the Java side.
			return 0;
		}
#endif
		default: {
			return 0;
		}
	}
}

void DisplayServerAndroid::window_attach_instance_id(ObjectID p_instance, DisplayServer::WindowID p_window) {
	window_attached_instance_id = p_instance;
}

ObjectID DisplayServerAndroid::window_get_attached_instance_id(DisplayServer::WindowID p_window) const {
	return window_attached_instance_id;
}

void DisplayServerAndroid::window_set_title(const String &p_title, DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

int DisplayServerAndroid::window_get_current_screen(DisplayServer::WindowID p_window) const {
	return SCREEN_OF_MAIN_WINDOW;
}

void DisplayServerAndroid::window_set_current_screen(int p_screen, DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

Point2i DisplayServerAndroid::window_get_position(DisplayServer::WindowID p_window) const {
	return Point2i();
}

Point2i DisplayServerAndroid::window_get_position_with_decorations(DisplayServer::WindowID p_window) const {
	return Point2i();
}

void DisplayServerAndroid::window_set_position(const Point2i &p_position, DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

void DisplayServerAndroid::window_set_transient(DisplayServer::WindowID p_window, DisplayServer::WindowID p_parent) {
	// Not supported on Android.
}

void DisplayServerAndroid::window_set_max_size(const Size2i p_size, DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

Size2i DisplayServerAndroid::window_get_max_size(DisplayServer::WindowID p_window) const {
	return Size2i();
}

void DisplayServerAndroid::window_set_min_size(const Size2i p_size, DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

Size2i DisplayServerAndroid::window_get_min_size(DisplayServer::WindowID p_window) const {
	return Size2i();
}

void DisplayServerAndroid::window_set_size(const Size2i p_size, DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

Size2i DisplayServerAndroid::window_get_size(DisplayServer::WindowID p_window) const {
	return OS_Android::get_singleton()->get_display_size();
}

Size2i DisplayServerAndroid::window_get_size_with_decorations(DisplayServer::WindowID p_window) const {
	return OS_Android::get_singleton()->get_display_size();
}

void DisplayServerAndroid::window_set_mode(DisplayServer::WindowMode p_mode, DisplayServer::WindowID p_window) {
	OS_Android::get_singleton()->get_godot_java()->enable_immersive_mode(p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN);
}

DisplayServer::WindowMode DisplayServerAndroid::window_get_mode(DisplayServer::WindowID p_window) const {
	if (OS_Android::get_singleton()->get_godot_java()->is_in_immersive_mode()) {
		return WINDOW_MODE_FULLSCREEN;
	} else {
		return WINDOW_MODE_MAXIMIZED;
	}
}

bool DisplayServerAndroid::window_is_maximize_allowed(DisplayServer::WindowID p_window) const {
	return false;
}

void DisplayServerAndroid::window_set_flag(DisplayServer::WindowFlags p_flag, bool p_enabled, DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

bool DisplayServerAndroid::window_get_flag(DisplayServer::WindowFlags p_flag, DisplayServer::WindowID p_window) const {
	return false;
}

void DisplayServerAndroid::window_request_attention(DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

void DisplayServerAndroid::window_move_to_foreground(DisplayServer::WindowID p_window) {
	// Not supported on Android.
}

bool DisplayServerAndroid::window_is_focused(WindowID p_window) const {
	return true;
}

bool DisplayServerAndroid::window_can_draw(DisplayServer::WindowID p_window) const {
	return true;
}

bool DisplayServerAndroid::can_any_window_draw() const {
	return true;
}

void DisplayServerAndroid::process_events() {
	Input::get_singleton()->flush_buffered_events();
}

Vector<String> DisplayServerAndroid::get_rendering_drivers_func() {
	Vector<String> drivers;

#ifdef GLES3_ENABLED
	drivers.push_back("opengl3");
#endif
#ifdef VULKAN_ENABLED
	drivers.push_back("vulkan");
#endif

	return drivers;
}

DisplayServer *DisplayServerAndroid::create_func(const String &p_rendering_driver, DisplayServer::WindowMode p_mode, DisplayServer::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	DisplayServer *ds = memnew(DisplayServerAndroid(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, p_context, p_parent_window, r_error));
	if (r_error != OK) {
		if (p_rendering_driver == "vulkan") {
			OS::get_singleton()->alert(
					"Your device seems not to support the required Vulkan version.\n\n"
					"Please try exporting your game using the 'gl_compatibility' renderer.",
					"Unable to initialize Vulkan video driver");
		} else {
			OS::get_singleton()->alert(
					"Your device seems not to support the required OpenGL ES 3.0 version.",
					"Unable to initialize OpenGL video driver");
		}
	}
	return ds;
}

void DisplayServerAndroid::register_android_driver() {
	register_create_function("android", create_func, get_rendering_drivers_func);
}

void DisplayServerAndroid::reset_window() {
#if defined(RD_ENABLED)
	if (rendering_context) {
		if (rendering_device) {
			rendering_device->screen_free(MAIN_WINDOW_ID);
		}

		VSyncMode last_vsync_mode = rendering_context->window_get_vsync_mode(MAIN_WINDOW_ID);
		rendering_context->window_destroy(MAIN_WINDOW_ID);

		union {
#ifdef VULKAN_ENABLED
			RenderingContextDriverVulkanAndroid::WindowPlatformData vulkan;
#endif
		} wpd;
#ifdef VULKAN_ENABLED
		if (rendering_driver == "vulkan") {
			ANativeWindow *native_window = OS_Android::get_singleton()->get_native_window();
			ERR_FAIL_NULL(native_window);
			wpd.vulkan.window = native_window;
		}
#endif

		if (rendering_context->window_create(MAIN_WINDOW_ID, &wpd) != OK) {
			ERR_PRINT(vformat("Failed to reset %s window.", rendering_driver));
			memdelete(rendering_context);
			rendering_context = nullptr;
			return;
		}

		Size2i display_size = OS_Android::get_singleton()->get_display_size();
		rendering_context->window_set_size(MAIN_WINDOW_ID, display_size.width, display_size.height);
		rendering_context->window_set_vsync_mode(MAIN_WINDOW_ID, last_vsync_mode);

		if (rendering_device) {
			rendering_device->screen_create(MAIN_WINDOW_ID);
		}
	}
#endif
}

void DisplayServerAndroid::notify_surface_changed(int p_width, int p_height) {
	if (rect_changed_callback.is_valid()) {
		rect_changed_callback.call(Rect2i(0, 0, p_width, p_height));
	}
}

DisplayServerAndroid::DisplayServerAndroid(const String &p_rendering_driver, DisplayServer::WindowMode p_mode, DisplayServer::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	rendering_driver = p_rendering_driver;

	keep_screen_on = GLOBAL_GET("display/window/energy_saving/keep_screen_on");

	native_menu = memnew(NativeMenu);

#if defined(RD_ENABLED)
	rendering_context = nullptr;
	rendering_device = nullptr;

#if defined(VULKAN_ENABLED)
	if (rendering_driver == "vulkan") {
		rendering_context = memnew(RenderingContextDriverVulkanAndroid);
	}
#endif

	if (rendering_context) {
		if (rendering_context->initialize() != OK) {
			memdelete(rendering_context);
			rendering_context = nullptr;
#if defined(GLES3_ENABLED)
			bool fallback_to_opengl3 = GLOBAL_GET("rendering/rendering_device/fallback_to_opengl3");
			if (fallback_to_opengl3 && rendering_driver != "opengl3") {
				WARN_PRINT("Your device seem not to support Vulkan, switching to OpenGL 3.");
				rendering_driver = "opengl3";
				OS::get_singleton()->set_current_rendering_method("gl_compatibility");
				OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
			} else
#endif
			{
				ERR_PRINT(vformat("Failed to initialize %s context", rendering_driver));
				r_error = ERR_UNAVAILABLE;
				return;
			}
		}
	}

	if (rendering_context) {
		union {
#ifdef VULKAN_ENABLED
			RenderingContextDriverVulkanAndroid::WindowPlatformData vulkan;
#endif
		} wpd;
#ifdef VULKAN_ENABLED
		if (rendering_driver == "vulkan") {
			ANativeWindow *native_window = OS_Android::get_singleton()->get_native_window();
			ERR_FAIL_NULL(native_window);
			wpd.vulkan.window = native_window;
		}
#endif

		if (rendering_context->window_create(MAIN_WINDOW_ID, &wpd) != OK) {
			ERR_PRINT(vformat("Failed to create %s window.", rendering_driver));
			memdelete(rendering_context);
			rendering_context = nullptr;
			r_error = ERR_UNAVAILABLE;
			return;
		}

		Size2i display_size = OS_Android::get_singleton()->get_display_size();
		rendering_context->window_set_size(MAIN_WINDOW_ID, display_size.width, display_size.height);
		rendering_context->window_set_vsync_mode(MAIN_WINDOW_ID, p_vsync_mode);

		rendering_device = memnew(RenderingDevice);
		if (rendering_device->initialize(rendering_context, MAIN_WINDOW_ID) != OK) {
			rendering_device = nullptr;
			memdelete(rendering_context);
			rendering_context = nullptr;
			r_error = ERR_UNAVAILABLE;
			return;
		}
		rendering_device->screen_create(MAIN_WINDOW_ID);

		RendererCompositorRD::make_current();
	}
#endif

#if defined(GLES3_ENABLED)
	if (rendering_driver == "opengl3") {
		RasterizerGLES3::make_current(false);
	}
#endif

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);

	r_error = OK;
}

DisplayServerAndroid::~DisplayServerAndroid() {
	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}

#if defined(RD_ENABLED)
	if (rendering_device) {
		memdelete(rendering_device);
	}
	if (rendering_context) {
		memdelete(rendering_context);
	}
#endif
}

void DisplayServerAndroid::process_accelerometer(const Vector3 &p_accelerometer) {
	Input::get_singleton()->set_accelerometer(p_accelerometer);
}

void DisplayServerAndroid::process_gravity(const Vector3 &p_gravity) {
	Input::get_singleton()->set_gravity(p_gravity);
}

void DisplayServerAndroid::process_magnetometer(const Vector3 &p_magnetometer) {
	Input::get_singleton()->set_magnetometer(p_magnetometer);
}

void DisplayServerAndroid::process_gyroscope(const Vector3 &p_gyroscope) {
	Input::get_singleton()->set_gyroscope(p_gyroscope);
}

void DisplayServerAndroid::mouse_set_mode(MouseMode p_mode) {
	if (!OS_Android::get_singleton()->get_godot_java()->get_godot_view()->can_update_pointer_icon() || !OS_Android::get_singleton()->get_godot_java()->get_godot_view()->can_capture_pointer()) {
		return;
	}
	if (mouse_mode == p_mode) {
		return;
	}

	if (p_mode == MouseMode::MOUSE_MODE_HIDDEN) {
		OS_Android::get_singleton()->get_godot_java()->get_godot_view()->set_pointer_icon(CURSOR_TYPE_NULL);
	} else {
		cursor_set_shape(cursor_shape);
	}

	if (p_mode == MouseMode::MOUSE_MODE_CAPTURED) {
		OS_Android::get_singleton()->get_godot_java()->get_godot_view()->request_pointer_capture();
	} else {
		OS_Android::get_singleton()->get_godot_java()->get_godot_view()->release_pointer_capture();
	}

	mouse_mode = p_mode;
}

DisplayServer::MouseMode DisplayServerAndroid::mouse_get_mode() const {
	return mouse_mode;
}

Point2i DisplayServerAndroid::mouse_get_position() const {
	return Input::get_singleton()->get_mouse_position();
}

BitField<MouseButtonMask> DisplayServerAndroid::mouse_get_button_state() const {
	return Input::get_singleton()->get_mouse_button_mask();
}

void DisplayServerAndroid::_cursor_set_shape_helper(CursorShape p_shape, bool force) {
	if (!OS_Android::get_singleton()->get_godot_java()->get_godot_view()->can_update_pointer_icon()) {
		return;
	}
	if (cursor_shape == p_shape && !force) {
		return;
	}

	cursor_shape = p_shape;

	if (mouse_mode == MouseMode::MOUSE_MODE_VISIBLE || mouse_mode == MouseMode::MOUSE_MODE_CONFINED) {
		OS_Android::get_singleton()->get_godot_java()->get_godot_view()->set_pointer_icon(android_cursors[cursor_shape]);
	}
}

void DisplayServerAndroid::cursor_set_shape(DisplayServer::CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	_cursor_set_shape_helper(p_shape);
}

DisplayServer::CursorShape DisplayServerAndroid::cursor_get_shape() const {
	return cursor_shape;
}

void DisplayServerAndroid::cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);
	String cursor_path = p_cursor.is_valid() ? p_cursor->get_path() : "";
	if (!cursor_path.is_empty()) {
		cursor_path = ProjectSettings::get_singleton()->globalize_path(cursor_path);
	}
	OS_Android::get_singleton()->get_godot_java()->get_godot_view()->configure_pointer_icon(android_cursors[cursor_shape], cursor_path, p_hotspot);
	_cursor_set_shape_helper(p_shape, true);
}

void DisplayServerAndroid::window_set_vsync_mode(DisplayServer::VSyncMode p_vsync_mode, WindowID p_window) {
#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_context->window_set_vsync_mode(p_window, p_vsync_mode);
	}
#endif
}

DisplayServer::VSyncMode DisplayServerAndroid::window_get_vsync_mode(WindowID p_window) const {
#if defined(RD_ENABLED)
	if (rendering_context) {
		return rendering_context->window_get_vsync_mode(p_window);
	}
#endif
	return DisplayServer::VSYNC_ENABLED;
}

void DisplayServerAndroid::reset_swap_buffers_flag() {
	swap_buffers_flag = false;
}

bool DisplayServerAndroid::should_swap_buffers() const {
	return swap_buffers_flag;
}

void DisplayServerAndroid::swap_buffers() {
	swap_buffers_flag = true;
}

void DisplayServerAndroid::set_native_icon(const String &p_filename) {
	// NOT SUPPORTED
}

void DisplayServerAndroid::set_icon(const Ref<Image> &p_icon) {
	// NOT SUPPORTED
}
