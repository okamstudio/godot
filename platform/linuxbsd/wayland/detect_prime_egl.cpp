/**************************************************************************/
/*  detect_prime_egl.cpp                                                  */
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

#ifdef GLES3_ENABLED
#ifdef EGL_ENABLED

#include "detect_prime_egl.h"

#include "core/string/print_string.h"
#include "core/string/ustring.h"

#include <stdlib.h>

#ifdef __OpenBSD__
// Map `quick_exit` to `_Exit`
// See reason in: GH-92130#issuecomment-2127135908
#define quick_exit _Exit
#endif

#include <cstring>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// To prevent shadowing warnings.
#undef glGetString

// Runs inside a child. Exiting will not quit the engine.
void DetectPrimeEGL::create_context(EGLenum p_platform_enum) {
#if defined(GLAD_ENABLED)
	if (!gladLoaderLoadEGL(nullptr)) {
		print_verbose("Unable to load EGL, GPU detection skipped.");
		quick_exit(1);
	}
#endif

	EGLDisplay egl_display = EGL_NO_DISPLAY;

	if (GLAD_EGL_VERSION_1_5) {
		egl_display = eglGetPlatformDisplay(p_platform_enum, nullptr, nullptr);
	} else if (GLAD_EGL_EXT_platform_base) {
#ifdef EGL_EXT_platform_base
		egl_display = eglGetPlatformDisplayEXT(p_platform_enum, nullptr, nullptr);
#endif
	} else {
		egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	}

	EGLConfig egl_config;
	EGLContext egl_context = EGL_NO_CONTEXT;

	eglInitialize(egl_display, nullptr, nullptr);

#if defined(GLAD_ENABLED)
	if (!gladLoaderLoadEGL(egl_display)) {
		print_verbose("Unable to load EGL, GPU detection skipped.");
		quick_exit(1);
	}
#endif

	eglBindAPI(EGL_OPENGL_API);

	EGLint attribs[] = {
		EGL_RED_SIZE,
		1,
		EGL_BLUE_SIZE,
		1,
		EGL_GREEN_SIZE,
		1,
		EGL_DEPTH_SIZE,
		24,
		EGL_NONE,
	};

	EGLint config_count = 0;
	eglChooseConfig(egl_display, attribs, &egl_config, 1, &config_count);

	EGLint context_attribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 3,
		EGL_NONE
	};

	egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attribs);
	if (egl_context == EGL_NO_CONTEXT) {
		print_verbose("Unable to create an EGL context, GPU detection skipped.");
		quick_exit(1);
	}

	eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
}

int DetectPrimeEGL::detect_prime(EGLenum p_platform_enum) {
	pid_t p;
	int priorities[4] = {};
	String vendors[4];
	String renderers[4];

	for (int i = 0; i < 4; ++i) {
		vendors[i] = "Unknown";
		renderers[i] = "Unknown";
	}

	for (int i = 0; i < 4; ++i) {
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

			char string[201];

			close(fdset[0]);

			setenv("DRI_PRIME", itos(i).utf8().ptr(), 1);

			create_context(p_platform_enum);

			PFNGLGETSTRINGPROC glGetString = (PFNGLGETSTRINGPROC)eglGetProcAddress("glGetString");
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

	for (int i = 3; i >= 0; --i) {
		const Vendor *v = vendor_map;
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
	for (int i = 0; i < 4; ++i) {
		print_verbose("Renderer " + itos(i) + ": " + renderers[i] + " with priority: " + itos(priorities[i]));
	}

	print_verbose("Using renderer: " + renderers[preferred]);
	return preferred;
}

#endif // EGL_ENABLED
#endif // GLES3_ENABLED
