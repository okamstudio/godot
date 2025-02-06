/**************************************************************************/
/*  detect_prime_x11.cpp                                                  */
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

#if defined(X11_ENABLED) && defined(GLES3_ENABLED)

#include "detect_prime_x11.h"

#include "core/string/print_string.h"
#include "core/string/ustring.h"

#include "thirdparty/glad/glad/gl.h"
#include "thirdparty/glad/glad/glx.h"

#ifdef SOWRAP_ENABLED
#include "x11/dynwrappers/xlib-so_wrap.h"
#else
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

using GLXCREATECONTEXTATTRIBSARBPROC = GLXContext (*)(Display *, GLXFBConfig, GLXContext, Bool, const int *);

// To prevent shadowing warnings
#undef glGetString

struct vendor {
	const char *glxvendor = nullptr;
	int priority = 0;
};

vendor vendormap[] = {
	{ "Advanced Micro Devices, Inc.", 30 },
	{ "AMD", 30 },
	{ "NVIDIA Corporation", 30 },
	{ "X.Org", 30 },
	{ "Intel Open Source Technology Center", 20 },
	{ "Intel", 20 },
	{ "nouveau", 10 },
	{ "Mesa Project", 0 },
	{ nullptr, 0 }
};

// Runs inside a child. Exiting will not quit the engine.
void create_context() {
	Display *x11_display = XOpenDisplay(nullptr);
	Window x11_window;
	GLXContext glx_context;

	static int visual_attribs[] = {
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_DOUBLEBUFFER, true,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DEPTH_SIZE, 24,
		None
	};

	if (gladLoaderLoadGLX(x11_display, XScreenNumberOfScreen(XDefaultScreenOfDisplay(x11_display))) == 0) {
		print_verbose("Unable to load GLX, GPU detection skipped.");
		quick_exit(1);
	}
	int fbcount;
	GLXFBConfig fbconfig = nullptr;
	XVisualInfo *vi = nullptr;

	XSetWindowAttributes swa;
	swa.event_mask = StructureNotifyMask;
	swa.border_pixel = 0;
	unsigned long valuemask = CWBorderPixel | CWColormap | CWEventMask;

	GLXFBConfig *fbc = glXChooseFBConfig(x11_display, DefaultScreen(x11_display), visual_attribs, &fbcount);
	if (!fbc) {
		quick_exit(1);
	}

	vi = glXGetVisualFromFBConfig(x11_display, fbc[0]);

	fbconfig = fbc[0];

	static int context_attribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 3,
		GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
		GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		None
	};

	glx_context = glXCreateContextAttribsARB(x11_display, fbconfig, nullptr, true, context_attribs);

	swa.colormap = XCreateColormap(x11_display, RootWindow(x11_display, vi->screen), vi->visual, AllocNone);
	x11_window = XCreateWindow(x11_display, RootWindow(x11_display, vi->screen), 0, 0, 10, 10, 0, vi->depth, InputOutput, vi->visual, valuemask, &swa);

	if (!x11_window) {
		quick_exit(1);
	}

	glXMakeCurrent(x11_display, x11_window, glx_context);
	XFree(vi);
}

int silent_error_handler(Display *display, XErrorEvent *error) {
	static char message[1024];
	XGetErrorText(display, error->error_code, message, sizeof(message));
	print_verbose(vformat("XServer error: %s"
						  "\n   Major opcode of failed request: %d"
						  "\n   Serial number of failed request: %d"
						  "\n   Current serial number in output stream: %d",
			String::utf8(message), (uint64_t)error->request_code, (uint64_t)error->minor_code, (uint64_t)error->serial));

	quick_exit(1);
	return 0;
}

int detect_prime() {
	pid_t p;
	int priorities[2] = {};
	String vendors[2];
	String renderers[2];

	vendors[0] = "Unknown";
	vendors[1] = "Unknown";
	renderers[0] = "Unknown";
	renderers[1] = "Unknown";

	for (int i = 0; i < 2; ++i) {
		int fdset[2];

		if (pipe(fdset) == -1) {
			print_verbose("Failed to pipe(), using default GPU");
			return 0;
		}

		// Fork so the driver initialization can crash without taking down the engine.
		p = fork();

		if (p > 0) {
			// Main thread

			int stat_loc = 0;
			char string[201];
			string[200] = '\0';

			close(fdset[1]);

			waitpid(p, &stat_loc, 0);

			if (!stat_loc) {
				// No need to do anything complicated here. Anything less than
				// PIPE_BUF will be delivered in one read() call.
				// Leave it 'Unknown' otherwise.
				if (read(fdset[0], string, sizeof(string) - 1) > 0) {
					vendors[i] = string;
					renderers[i] = string + strlen(string) + 1;
				}
			}

			close(fdset[0]);

		} else {
			// In child, exit() here will not quit the engine.

			// Prevent false leak reports as we will not be properly
			// cleaning up these processes, and fork() makes a copy
			// of all globals.
			CoreGlobals::leak_reporting_enabled = false;
			XSetErrorHandler(&silent_error_handler);

			char string[201];

			close(fdset[0]);

			if (i) {
				setenv("DRI_PRIME", "1", 1);
			}

			create_context();

			PFNGLGETSTRINGPROC glGetString = (PFNGLGETSTRINGPROC)glXGetProcAddressARB((GLubyte *)"glGetString");
			if (!glGetString) {
				print_verbose("Unable to get glGetString, GPU detection skipped.");
				quick_exit(1);
			}

			const char *vendor = (const char *)glGetString(GL_VENDOR);
			const char *renderer = (const char *)glGetString(GL_RENDERER);

			unsigned int vendor_len = strlen(vendor) + 1;
			unsigned int renderer_len = strlen(renderer) + 1;

			if (vendor_len + renderer_len >= sizeof(string)) {
				renderer_len = 200 - vendor_len;
			}

			memcpy(&string, vendor, vendor_len);
			memcpy(&string[vendor_len], renderer, renderer_len);

			if (write(fdset[1], string, vendor_len + renderer_len) == -1) {
				print_verbose("Couldn't write vendor/renderer string.");
			}
			close(fdset[1]);

			// The function quick_exit() is used because exit() will call destructors on static objects copied by fork().
			// These objects will be freed anyway when the process finishes execution.
			quick_exit(0);
		}
	}

	int preferred = 0;
	int priority = 0;

	if (vendors[0] == vendors[1]) {
		print_verbose("Only one GPU found, using default.");
		return 0;
	}

	for (int i = 1; i >= 0; --i) {
		vendor *v = vendormap;
		while (v->glxvendor) {
			if (v->glxvendor == vendors[i]) {
				priorities[i] = v->priority;

				if (v->priority >= priority) {
					priority = v->priority;
					preferred = i;
				}
			}
			++v;
		}
	}

	print_verbose("Found renderers:");
	for (int i = 0; i < 2; ++i) {
		print_verbose("Renderer " + itos(i) + ": " + renderers[i] + " with priority: " + itos(priorities[i]));
	}

	print_verbose("Using renderer: " + renderers[preferred]);
	return preferred;
}

#endif // X11_ENABLED && GLES3_ENABLED
