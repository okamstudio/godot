/**************************************************************************/
/*  gl_manager_windows_native.h                                           */
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

#if defined(WINDOWS_ENABLED) && defined(GLES3_ENABLED)

#include "core/error/error_list.h"
#include "core/os/os.h"
#include "core/templates/local_vector.h"
#include "servers/display_server.h"

#include <windows.h>

typedef bool(APIENTRY *PFNWGLSWAPINTERVALEXTPROC)(int interval);
typedef int(APIENTRY *PFNWGLGETSWAPINTERVALEXTPROC)(void);

class GLManagerNative_Windows {
private:
	// any data specific to the window
	struct GLWindow {
		bool use_vsync = false;

		// windows specific
		HDC hDC;
		HWND hwnd;

		int gldisplay_id = 0;
	};

	struct GLDisplay {
		// windows specific
		HGLRC hRC;
	};

	RBMap<DisplayServer::WindowID, GLWindow> _windows;
	LocalVector<GLDisplay> _displays;

	GLWindow *_current_window = nullptr;

	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;

	GLWindow &get_window(unsigned int id) { return _windows[id]; }
	const GLWindow &get_window(unsigned int id) const { return _windows[id]; }

	const GLDisplay &get_current_display() const { return _displays[_current_window->gldisplay_id]; }
	const GLDisplay &get_display(unsigned int id) { return _displays[id]; }

	bool direct_render;
	int glx_minor, glx_major;

private:
	void _nvapi_setup_profile();
	int _find_or_create_display(GLWindow &win);
	Error _create_context(GLWindow &win, GLDisplay &gl_display);

public:
	Error window_create(DisplayServer::WindowID p_window_id, HWND p_hwnd, HINSTANCE p_hinstance, int p_width, int p_height);
	void window_destroy(DisplayServer::WindowID p_window_id);
	void window_resize(DisplayServer::WindowID p_window_id, int p_width, int p_height) {}

	void release_current();
	void swap_buffers();

	void window_make_current(DisplayServer::WindowID p_window_id);

	Error initialize();

	void set_use_vsync(DisplayServer::WindowID p_window_id, bool p_use);
	bool is_using_vsync(DisplayServer::WindowID p_window_id) const;

	HDC get_hdc(DisplayServer::WindowID p_window_id);
	HGLRC get_hglrc(DisplayServer::WindowID p_window_id);

	GLManagerNative_Windows();
	~GLManagerNative_Windows();
};

#endif // WINDOWS_ENABLED && GLES3_ENABLED
