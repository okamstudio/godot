/**************************************************************************/
/*  egl_manager.h                                                         */
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

#ifdef EGL_ENABLED

// These must come first to avoid windows.h mess.
#include "platform_gl.h"

#include "core/templates/local_vector.h"
#include "servers/display_server.h"

class EGLManager {
private:
	// An EGL-side representation of a display with its own rendering
	// context.
	struct GLDisplay {
		void *display = nullptr;

		EGLDisplay egl_display = EGL_NO_DISPLAY;
		EGLContext egl_context = EGL_NO_CONTEXT;
		EGLConfig egl_config = nullptr;

#ifdef WINDOWS_ENABLED
		bool has_EGL_ANGLE_surface_orientation = false;
#endif
	};

	// EGL specific window data.
	struct GLWindow {
		bool initialized = false;
#ifdef WINDOWS_ENABLED
		bool flipped_y = false;
#endif

		// An handle to the GLDisplay associated with this window.
		int gldisplay_id = -1;

		EGLSurface egl_surface = EGL_NO_SURFACE;
	};

	LocalVector<GLDisplay> displays;
	LocalVector<GLWindow> windows;

	GLWindow *current_window = nullptr;

	// On EGL the default swap interval is 1 and thus vsync is on by default.
	bool use_vsync = true;

	virtual const char *_get_platform_extension_name() const = 0;
	virtual EGLenum _get_platform_extension_enum() const = 0;
	virtual EGLenum _get_platform_api_enum() const = 0;
	virtual Vector<EGLAttrib> _get_platform_display_attributes() const = 0;
	virtual Vector<EGLint> _get_platform_context_attribs() const = 0;

#ifdef EGL_ANDROID_blob_cache
	static String shader_cache_dir;

	static void _set_cache(const void *p_key, EGLsizeiANDROID p_key_size, const void *p_value, EGLsizeiANDROID p_value_size);
	static EGLsizeiANDROID _get_cache(const void *p_key, EGLsizeiANDROID p_key_size, void *p_value, EGLsizeiANDROID p_value_size);
#endif

	int _get_gldisplay_id(void *p_display);
	Error _gldisplay_create_context(GLDisplay &p_gldisplay);

public:
	int display_get_native_visual_id(void *p_display);

	Error open_display(void *p_display);
	Error window_create(DisplayServer::WindowID p_window_id, void *p_display, void *p_native_window, int p_width, int p_height);

	void window_destroy(DisplayServer::WindowID p_window_id);

	void release_current();
	void swap_buffers();

	void window_make_current(DisplayServer::WindowID p_window_id);

	void set_use_vsync(bool p_use);
	bool is_using_vsync() const;

	EGLContext get_context(DisplayServer::WindowID p_window_id);
	EGLDisplay get_display(DisplayServer::WindowID p_window_id);
	EGLConfig get_config(DisplayServer::WindowID p_window_id);

	Error initialize(void *p_native_display = nullptr);

	EGLManager();
	virtual ~EGLManager();
};

#endif // EGL_ENABLED
