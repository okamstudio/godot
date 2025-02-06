/**************************************************************************/
/*  gl_manager_macos_legacy.h                                             */
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

#if defined(MACOS_ENABLED) && defined(GLES3_ENABLED)

#include "core/error/error_list.h"
#include "core/os/os.h"
#include "core/templates/local_vector.h"
#include "servers/display_server.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreVideo/CoreVideo.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations" // OpenGL is deprecated in macOS 10.14

typedef CGLError (*CGLEnablePtr)(CGLContextObj ctx, CGLContextEnable pname);
typedef CGLError (*CGLSetParameterPtr)(CGLContextObj ctx, CGLContextParameter pname, const GLint *params);
typedef CGLContextObj (*CGLGetCurrentContextPtr)(void);

class GLManagerLegacy_MacOS {
	struct GLWindow {
		id window_view = nullptr;
		NSOpenGLContext *context = nullptr;
	};

	RBMap<DisplayServer::WindowID, GLWindow> windows;

	NSOpenGLContext *shared_context = nullptr;
	DisplayServer::WindowID current_window = DisplayServer::INVALID_WINDOW_ID;

	Error create_context(GLWindow &win);

	bool framework_loaded = false;
	bool use_vsync = false;
	CGLEnablePtr CGLEnable = nullptr;
	CGLSetParameterPtr CGLSetParameter = nullptr;
	CGLGetCurrentContextPtr CGLGetCurrentContext = nullptr;

public:
	Error window_create(DisplayServer::WindowID p_window_id, id p_view, int p_width, int p_height);
	void window_destroy(DisplayServer::WindowID p_window_id);
	void window_resize(DisplayServer::WindowID p_window_id, int p_width, int p_height);

	void release_current();
	void swap_buffers();

	void window_make_current(DisplayServer::WindowID p_window_id);

	void window_set_per_pixel_transparency_enabled(DisplayServer::WindowID p_window_id, bool p_enabled);

	Error initialize();

	void set_use_vsync(bool p_use);
	bool is_using_vsync() const;

	NSOpenGLContext *get_context(DisplayServer::WindowID p_window_id);

	GLManagerLegacy_MacOS();
	~GLManagerLegacy_MacOS();
};

#pragma clang diagnostic push

#endif // MACOS_ENABLED && GLES3_ENABLED
