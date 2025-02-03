/**************************************************************************/
/*  context_gl_x11.cpp                                                    */
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

#include "context_gl_x11.h"

#include "core/project_settings.h"

#ifdef X11_ENABLED
#if defined(OPENGL_ENABLED)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/glxext.h>

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

typedef GLXContext (*GLXCREATECONTEXTATTRIBSARBPROC)(Display *, GLXFBConfig, GLXContext, Bool, const int *);

struct ContextGL_X11_Private {
	::GLXContext glx_context;
	::GLXContext glx_context_offscreen;
};

void ContextGL_X11::release_current() {
	glXMakeCurrent(x11_display, None, nullptr);
}

void ContextGL_X11::make_current() {
	glXMakeCurrent(x11_display, x11_window, p->glx_context);
}

bool ContextGL_X11::is_offscreen_available() const {
	return p->glx_context_offscreen;
}

void ContextGL_X11::make_offscreen_current() {
	glXMakeCurrent(x11_display, x11_window, p->glx_context_offscreen);
}

void ContextGL_X11::release_offscreen_current() {
	glXMakeCurrent(x11_display, None, NULL);
}

void ContextGL_X11::swap_buffers() {
	glXSwapBuffers(x11_display, x11_window);
}

static bool ctxErrorOccurred = false;
static int ctxErrorHandler(Display *dpy, XErrorEvent *ev) {
	ctxErrorOccurred = true;
	return 0;
}

static void set_class_hint(Display *p_display, Window p_window, int p_context) {
	XClassHint *classHint = XAllocClassHint();

	if (classHint) {
		CharString name_str;
		switch (p_context) {
			case OS::CONTEXT_EDITOR:
				name_str = "Godot_Editor";
				break;
			case OS::CONTEXT_PROJECTMAN:
				name_str = "Godot_ProjectList";
				break;
			case OS::CONTEXT_ENGINE:
				name_str = "Godot_Engine";
				break;
		}

		CharString class_str;
		if (p_context == OS::CONTEXT_ENGINE) {
			String config_name = GLOBAL_GET("application/config/name");
			if (config_name.length() == 0) {
				class_str = "Godot_Engine";
			} else {
				class_str = config_name.utf8();
			}
		} else {
			class_str = "Godot";
		}

		classHint->res_class = class_str.ptrw();
		classHint->res_name = name_str.ptrw();

		XSetClassHint(p_display, p_window, classHint);
		XFree(classHint);
	}
}

Error ContextGL_X11::initialize(int p_context) {
	//const char *extensions = glXQueryExtensionsString(x11_display, DefaultScreen(x11_display));

	GLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = (GLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");

	ERR_FAIL_COND_V(!glXCreateContextAttribsARB, ERR_UNCONFIGURED);

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

	static int visual_attribs_layered[] = {
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_DOUBLEBUFFER, true,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		None
	};

	int fbcount;
	GLXFBConfig fbconfig = nullptr;
	XVisualInfo *vi = nullptr;

	XSetWindowAttributes swa;
	swa.event_mask = StructureNotifyMask;
	swa.border_pixel = 0;
	unsigned long valuemask = CWBorderPixel | CWColormap | CWEventMask;

	if (OS::get_singleton()->is_layered_allowed()) {
		GLXFBConfig *fbc = glXChooseFBConfig(x11_display, DefaultScreen(x11_display), visual_attribs_layered, &fbcount);
		ERR_FAIL_COND_V(!fbc, ERR_UNCONFIGURED);

		for (int i = 0; i < fbcount; i++) {
			vi = (XVisualInfo *)glXGetVisualFromFBConfig(x11_display, fbc[i]);
			if (!vi) {
				continue;
			}

			XRenderPictFormat *pict_format = XRenderFindVisualFormat(x11_display, vi->visual);
			if (!pict_format) {
				XFree(vi);
				vi = nullptr;
				continue;
			}

			fbconfig = fbc[i];
			if (pict_format->direct.alphaMask > 0) {
				break;
			}
		}
		XFree(fbc);
		ERR_FAIL_COND_V(!fbconfig, ERR_UNCONFIGURED);

		swa.background_pixmap = None;
		swa.background_pixel = 0;
		swa.border_pixmap = None;
		valuemask |= CWBackPixel;

	} else {
		GLXFBConfig *fbc = glXChooseFBConfig(x11_display, DefaultScreen(x11_display), visual_attribs, &fbcount);
		ERR_FAIL_COND_V(!fbc, ERR_UNCONFIGURED);

		vi = glXGetVisualFromFBConfig(x11_display, fbc[0]);

		fbconfig = fbc[0];
		XFree(fbc);
	}

	int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(&ctxErrorHandler);

	switch (context_type) {
		case OLDSTYLE: {
			p->glx_context = glXCreateContext(x11_display, vi, nullptr, GL_TRUE);
			ERR_FAIL_COND_V(!p->glx_context, ERR_UNCONFIGURED);
		} break;
		case GLES_2_0_COMPATIBLE: {
			p->glx_context = glXCreateNewContext(x11_display, fbconfig, GLX_RGBA_TYPE, nullptr, true);
			ERR_FAIL_COND_V(!p->glx_context, ERR_UNCONFIGURED);
		} break;
		case GLES_3_0_COMPATIBLE: {
			static int context_attribs[] = {
				GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
				GLX_CONTEXT_MINOR_VERSION_ARB, 3,
				GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
				GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB /*|GLX_CONTEXT_DEBUG_BIT_ARB*/,
				None
			};

			p->glx_context = glXCreateContextAttribsARB(x11_display, fbconfig, nullptr, true, context_attribs);
			ERR_FAIL_COND_V(ctxErrorOccurred || !p->glx_context, ERR_UNCONFIGURED);
			p->glx_context_offscreen = glXCreateContextAttribsARB(x11_display, fbconfig, nullptr, true, context_attribs);
		} break;
	}

	swa.colormap = XCreateColormap(x11_display, RootWindow(x11_display, vi->screen), vi->visual, AllocNone);
	x11_window = XCreateWindow(x11_display, RootWindow(x11_display, vi->screen), 0, 0, OS::get_singleton()->get_video_mode().width, OS::get_singleton()->get_video_mode().height, 0, vi->depth, InputOutput, vi->visual, valuemask, &swa);
	XStoreName(x11_display, x11_window, "Godot Engine");

	ERR_FAIL_COND_V(!x11_window, ERR_UNCONFIGURED);
	set_class_hint(x11_display, x11_window, p_context);

	if (!OS::get_singleton()->is_no_window_mode_enabled()) {
		XMapWindow(x11_display, x11_window);
	}

	XSync(x11_display, False);
	XSetErrorHandler(oldHandler);

	glXMakeCurrent(x11_display, x11_window, p->glx_context);

	XFree(vi);

	return OK;
}

int ContextGL_X11::get_window_width() {
	XWindowAttributes xwa;
	XGetWindowAttributes(x11_display, x11_window, &xwa);

	return xwa.width;
}

int ContextGL_X11::get_window_height() {
	XWindowAttributes xwa;
	XGetWindowAttributes(x11_display, x11_window, &xwa);

	return xwa.height;
}

void *ContextGL_X11::get_glx_context() {
	if (p != nullptr) {
		return p->glx_context;
	} else {
		return nullptr;
	}
}

void ContextGL_X11::set_use_vsync(bool p_use) {
	static bool setup = false;
	static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = nullptr;
	static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalMESA = nullptr;
	static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = nullptr;

	if (!setup) {
		setup = true;
		String extensions = glXQueryExtensionsString(x11_display, DefaultScreen(x11_display));
		if (extensions.find("GLX_EXT_swap_control") != -1) {
			glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
		}
		if (extensions.find("GLX_MESA_swap_control") != -1) {
			glXSwapIntervalMESA = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalMESA");
		}
		if (extensions.find("GLX_SGI_swap_control") != -1) {
			glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
		}
	}
	int val = p_use ? 1 : 0;
	if (glXSwapIntervalMESA) {
		glXSwapIntervalMESA(val);
	} else if (glXSwapIntervalSGI) {
		glXSwapIntervalSGI(val);
	} else if (glXSwapIntervalEXT) {
		GLXDrawable drawable = glXGetCurrentDrawable();
		glXSwapIntervalEXT(x11_display, drawable, val);
	} else {
		return;
	}
	use_vsync = p_use;
}
bool ContextGL_X11::is_using_vsync() const {
	return use_vsync;
}

ContextGL_X11::ContextGL_X11(::Display *p_x11_display, ::Window &p_x11_window, const OS::VideoMode &p_default_video_mode, ContextType p_context_type) :
		x11_window(p_x11_window) {
	default_video_mode = p_default_video_mode;
	x11_display = p_x11_display;

	context_type = p_context_type;

	double_buffer = false;
	direct_render = false;
	glx_minor = glx_major = 0;
	p = memnew(ContextGL_X11_Private);
	p->glx_context = nullptr;
	p->glx_context_offscreen = nullptr;
	use_vsync = false;
}

ContextGL_X11::~ContextGL_X11() {
	release_current();
	glXDestroyContext(x11_display, p->glx_context);
	if (p->glx_context_offscreen) {
		glXDestroyContext(x11_display, p->glx_context_offscreen);
	}
	memdelete(p);
}

#endif
#endif
