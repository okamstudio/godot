/**************************************************************************/
/*  egl_manager.cpp                                                       */
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

#include "egl_manager.h"

#include "drivers/gles3/rasterizer_gles3.h"

#ifdef EGL_ENABLED

#if defined(EGL_STATIC)

#define GLAD_EGL_VERSION_1_5 true

#ifdef EGL_EXT_platform_base
#define GLAD_EGL_EXT_platform_base 1
#endif

#define KHRONOS_STATIC 1
extern "C" EGLAPI void EGLAPIENTRY eglSetBlobCacheFuncsANDROID(EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get);
extern "C" EGLAPI EGLDisplay EGLAPIENTRY eglGetPlatformDisplayEXT(EGLenum platform, void *native_display, const EGLint *attrib_list);
#undef KHRONOS_STATIC

#endif // defined(EGL_STATIC)

#ifndef EGL_EXT_platform_base
#define GLAD_EGL_EXT_platform_base 0
#endif

#ifdef WINDOWS_ENABLED
// Unofficial ANGLE extension: EGL_ANGLE_surface_orientation
#ifndef EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE
#define EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE 0x33A7
#define EGL_SURFACE_ORIENTATION_ANGLE 0x33A8
#define EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE 0x0001
#define EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE 0x0002
#endif
#endif

// Creates and caches a GLDisplay. Returns -1 on error.
int EGLManager::_get_gldisplay_id(void *p_display) {
	// Look for a cached GLDisplay.
	for (unsigned int i = 0; i < displays.size(); i++) {
		if (displays[i].display == p_display) {
			return i;
		}
	}

	// We didn't find any, so we'll have to create one, along with its own
	// EGLDisplay and EGLContext.
	GLDisplay new_gldisplay;
	new_gldisplay.display = p_display;

	if (GLAD_EGL_VERSION_1_5) {
		Vector<EGLAttrib> attribs = _get_platform_display_attributes();
		new_gldisplay.egl_display = eglGetPlatformDisplay(_get_platform_extension_enum(), new_gldisplay.display, (attribs.size() > 0) ? attribs.ptr() : nullptr);
	} else if (GLAD_EGL_EXT_platform_base) {
#ifdef EGL_EXT_platform_base
		// eglGetPlatformDisplayEXT wants its attributes as EGLint, so we'll truncate
		// what we already have. It's a bit naughty but I'm really not sure what else
		// we could do here.
		Vector<EGLint> attribs;
		for (const EGLAttrib &attrib : _get_platform_display_attributes()) {
			attribs.push_back((EGLint)attrib);
		}

		new_gldisplay.egl_display = eglGetPlatformDisplayEXT(_get_platform_extension_enum(), new_gldisplay.display, (attribs.size() > 0) ? attribs.ptr() : nullptr);
#endif // EGL_EXT_platform_base
	} else {
		NativeDisplayType *native_display_type = (NativeDisplayType *)new_gldisplay.display;
		new_gldisplay.egl_display = eglGetDisplay(*native_display_type);
	}

	ERR_FAIL_COND_V(eglGetError() != EGL_SUCCESS, -1);

	ERR_FAIL_COND_V_MSG(new_gldisplay.egl_display == EGL_NO_DISPLAY, -1, "Can't create an EGL display.");

	if (!eglInitialize(new_gldisplay.egl_display, nullptr, nullptr)) {
		ERR_FAIL_V_MSG(-1, "Can't initialize an EGL display.");
	}

	if (!eglBindAPI(_get_platform_api_enum())) {
		ERR_FAIL_V_MSG(-1, "OpenGL not supported.");
	}

	Error err = _gldisplay_create_context(new_gldisplay);

	if (err != OK) {
		eglTerminate(new_gldisplay.egl_display);
		ERR_FAIL_V(-1);
	}

#ifdef EGL_ANDROID_blob_cache
#if defined(EGL_STATIC)
	bool has_blob_cache = true;
#else
	bool has_blob_cache = (eglSetBlobCacheFuncsANDROID != nullptr);
#endif
	if (has_blob_cache && !shader_cache_dir.is_empty()) {
		eglSetBlobCacheFuncsANDROID(new_gldisplay.egl_display, &EGLManager::_set_cache, &EGLManager::_get_cache);
	}
#endif

#ifdef WINDOWS_ENABLED
	String client_extensions_string = eglQueryString(new_gldisplay.egl_display, EGL_EXTENSIONS);
	if (eglGetError() == EGL_SUCCESS) {
		Vector<String> egl_extensions = client_extensions_string.split(" ");

		if (egl_extensions.has("EGL_ANGLE_surface_orientation")) {
			new_gldisplay.has_EGL_ANGLE_surface_orientation = true;
			print_verbose("EGL: EGL_ANGLE_surface_orientation is supported.");
		}
	}
#endif

	displays.push_back(new_gldisplay);

	// Return the new GLDisplay's ID.
	return displays.size() - 1;
}

#ifdef EGL_ANDROID_blob_cache
String EGLManager::shader_cache_dir;

void EGLManager::_set_cache(const void *p_key, EGLsizeiANDROID p_key_size, const void *p_value, EGLsizeiANDROID p_value_size) {
	String name = CryptoCore::b64_encode_str((const uint8_t *)p_key, p_key_size).replace("/", "_");
	String path = shader_cache_dir.path_join(name) + ".cache";

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
	if (err != OK) {
		return;
	}
	file->store_buffer((const uint8_t *)p_value, p_value_size);
}

EGLsizeiANDROID EGLManager::_get_cache(const void *p_key, EGLsizeiANDROID p_key_size, void *p_value, EGLsizeiANDROID p_value_size) {
	String name = CryptoCore::b64_encode_str((const uint8_t *)p_key, p_key_size).replace("/", "_");
	String path = shader_cache_dir.path_join(name) + ".cache";

	Error err = OK;
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ, &err);
	if (err != OK) {
		return 0;
	}
	EGLsizeiANDROID len = file->get_length();
	if (len <= p_value_size) {
		file->get_buffer((uint8_t *)p_value, len);
	}
	return len;
}
#endif

Error EGLManager::_gldisplay_create_context(GLDisplay &p_gldisplay) {
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

	EGLint attribs_layered[] = {
		EGL_RED_SIZE,
		8,
		EGL_GREEN_SIZE,
		8,
		EGL_GREEN_SIZE,
		8,
		EGL_ALPHA_SIZE,
		8,
		EGL_DEPTH_SIZE,
		24,
		EGL_NONE,
	};

	EGLint config_count = 0;

	if (OS::get_singleton()->is_layered_allowed()) {
		eglChooseConfig(p_gldisplay.egl_display, attribs_layered, &p_gldisplay.egl_config, 1, &config_count);
	} else {
		eglChooseConfig(p_gldisplay.egl_display, attribs, &p_gldisplay.egl_config, 1, &config_count);
	}

	ERR_FAIL_COND_V(eglGetError() != EGL_SUCCESS, ERR_BUG);
	ERR_FAIL_COND_V(config_count == 0, ERR_UNCONFIGURED);

	Vector<EGLint> context_attribs = _get_platform_context_attribs();
	p_gldisplay.egl_context = eglCreateContext(p_gldisplay.egl_display, p_gldisplay.egl_config, EGL_NO_CONTEXT, (context_attribs.size() > 0) ? context_attribs.ptr() : nullptr);
	ERR_FAIL_COND_V_MSG(p_gldisplay.egl_context == EGL_NO_CONTEXT, ERR_CANT_CREATE, vformat("Can't create an EGL context. Error code: %d", eglGetError()));

	return OK;
}

Error EGLManager::open_display(void *p_display) {
	int gldisplay_id = _get_gldisplay_id(p_display);
	if (gldisplay_id < 0) {
		return ERR_CANT_CREATE;
	} else {
		return OK;
	}
}

int EGLManager::display_get_native_visual_id(void *p_display) {
	int gldisplay_id = _get_gldisplay_id(p_display);
	ERR_FAIL_COND_V(gldisplay_id < 0, ERR_CANT_CREATE);

	GLDisplay gldisplay = displays[gldisplay_id];

	EGLint native_visual_id = -1;

	if (!eglGetConfigAttrib(gldisplay.egl_display, gldisplay.egl_config, EGL_NATIVE_VISUAL_ID, &native_visual_id)) {
		ERR_FAIL_V(-1);
	}

	return native_visual_id;
}

Error EGLManager::window_create(DisplayServer::WindowID p_window_id, void *p_display, void *p_native_window, int p_width, int p_height) {
	int gldisplay_id = _get_gldisplay_id(p_display);
	ERR_FAIL_COND_V(gldisplay_id < 0, ERR_CANT_CREATE);

	GLDisplay &gldisplay = displays[gldisplay_id];

	// In order to ensure a fast lookup, make sure we got enough elements in the
	// windows local vector to use the window id as an index.
	if (p_window_id >= (int)windows.size()) {
		windows.resize(p_window_id + 1);
	}

	GLWindow &glwindow = windows[p_window_id];
	glwindow.gldisplay_id = gldisplay_id;

	Vector<EGLAttrib> egl_attribs;

#ifdef WINDOWS_ENABLED
	if (gldisplay.has_EGL_ANGLE_surface_orientation) {
		EGLint optimal_orientation;
		if (eglGetConfigAttrib(gldisplay.egl_display, gldisplay.egl_config, EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE, &optimal_orientation)) {
			// We only need to support inverting Y for optimizing ANGLE on D3D11.
			if (optimal_orientation & EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE && !(optimal_orientation & EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE)) {
				egl_attribs.push_back(EGL_SURFACE_ORIENTATION_ANGLE);
				egl_attribs.push_back(EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE);
			}
		} else {
			ERR_PRINT(vformat("Failed to get EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE, error: 0x%08X", eglGetError()));
		}
	}

	if (!egl_attribs.is_empty()) {
		egl_attribs.push_back(EGL_NONE);
	}
#endif

	if (GLAD_EGL_VERSION_1_5) {
		glwindow.egl_surface = eglCreatePlatformWindowSurface(gldisplay.egl_display, gldisplay.egl_config, p_native_window, egl_attribs.ptr());
	} else {
		EGLNativeWindowType *native_window_type = (EGLNativeWindowType *)p_native_window;
		glwindow.egl_surface = eglCreateWindowSurface(gldisplay.egl_display, gldisplay.egl_config, *native_window_type, nullptr);
	}

	if (glwindow.egl_surface == EGL_NO_SURFACE) {
		return ERR_CANT_CREATE;
	}

	glwindow.initialized = true;

#ifdef WINDOWS_ENABLED
	if (gldisplay.has_EGL_ANGLE_surface_orientation) {
		EGLint orientation;
		if (eglQuerySurface(gldisplay.egl_display, glwindow.egl_surface, EGL_SURFACE_ORIENTATION_ANGLE, &orientation)) {
			if (orientation & EGL_SURFACE_ORIENTATION_INVERT_Y_ANGLE && !(orientation & EGL_SURFACE_ORIENTATION_INVERT_X_ANGLE)) {
				glwindow.flipped_y = true;
				print_verbose("EGL: Using optimal surface orientation: Invert Y");
			}
		} else {
			ERR_PRINT(vformat("Failed to get EGL_SURFACE_ORIENTATION_ANGLE, error: 0x%08X", eglGetError()));
		}
	}
#endif

	window_make_current(p_window_id);

	return OK;
}

void EGLManager::window_destroy(DisplayServer::WindowID p_window_id) {
	ERR_FAIL_INDEX(p_window_id, (int)windows.size());

	GLWindow &glwindow = windows[p_window_id];

	if (!glwindow.initialized) {
		return;
	}

	glwindow.initialized = false;

	ERR_FAIL_INDEX(glwindow.gldisplay_id, (int)displays.size());
	GLDisplay &gldisplay = displays[glwindow.gldisplay_id];

	if (glwindow.egl_surface != EGL_NO_SURFACE) {
		eglDestroySurface(gldisplay.egl_display, glwindow.egl_surface);
		glwindow.egl_surface = nullptr;
	}
}

void EGLManager::release_current() {
	if (!current_window) {
		return;
	}

	GLDisplay &current_display = displays[current_window->gldisplay_id];

	eglMakeCurrent(current_display.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void EGLManager::swap_buffers() {
	if (!current_window) {
		return;
	}

	if (!current_window->initialized) {
		WARN_PRINT("Current OpenGL window is uninitialized!");
		return;
	}

	GLDisplay &current_display = displays[current_window->gldisplay_id];

	eglSwapBuffers(current_display.egl_display, current_window->egl_surface);
}

void EGLManager::window_make_current(DisplayServer::WindowID p_window_id) {
	if (p_window_id == DisplayServer::INVALID_WINDOW_ID) {
		return;
	}

	GLWindow &glwindow = windows[p_window_id];

	if (&glwindow == current_window || !glwindow.initialized) {
		return;
	}

	current_window = &glwindow;

	GLDisplay &current_display = displays[current_window->gldisplay_id];

	eglMakeCurrent(current_display.egl_display, current_window->egl_surface, current_window->egl_surface, current_display.egl_context);

#ifdef WINDOWS_ENABLED
	RasterizerGLES3::set_screen_flipped_y(glwindow.flipped_y);
#endif
}

void EGLManager::set_use_vsync(bool p_use) {
	// We need an active window to get a display to set the vsync.
	if (!current_window) {
		return;
	}

	GLDisplay &disp = displays[current_window->gldisplay_id];

	int swap_interval = p_use ? 1 : 0;

	if (!eglSwapInterval(disp.egl_display, swap_interval)) {
		WARN_PRINT("Could not set V-Sync mode.");
	}

	use_vsync = p_use;
}

bool EGLManager::is_using_vsync() const {
	return use_vsync;
}

EGLContext EGLManager::get_context(DisplayServer::WindowID p_window_id) {
	GLWindow &glwindow = windows[p_window_id];

	if (!glwindow.initialized) {
		return EGL_NO_CONTEXT;
	}

	GLDisplay &display = displays[glwindow.gldisplay_id];

	return display.egl_context;
}

Error EGLManager::initialize(void *p_native_display) {
#if defined(GLAD_ENABLED) && !defined(EGL_STATIC)
	// Loading EGL with a new display gets us just the bare minimum API. We'll then
	// have to temporarily get a proper display and reload EGL once again to
	// initialize everything else.
	if (!gladLoaderLoadEGL(EGL_NO_DISPLAY)) {
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Can't load EGL dynamic library.");
	}

	EGLDisplay tmp_display = EGL_NO_DISPLAY;

	if (GLAD_EGL_EXT_platform_base) {
#ifdef EGL_EXT_platform_base
		// eglGetPlatformDisplayEXT wants its attributes as EGLint.
		Vector<EGLint> attribs;
		for (const EGLAttrib &attrib : _get_platform_display_attributes()) {
			attribs.push_back((EGLint)attrib);
		}
		tmp_display = eglGetPlatformDisplayEXT(_get_platform_extension_enum(), p_native_display, attribs.ptr());
#endif // EGL_EXT_platform_base
	} else {
		WARN_PRINT("EGL: EGL_EXT_platform_base not found during init, using default platform.");
		EGLNativeDisplayType *native_display_type = (EGLNativeDisplayType *)p_native_display;
		tmp_display = eglGetDisplay(*native_display_type);
	}

	if (tmp_display == EGL_NO_DISPLAY) {
		eglTerminate(tmp_display);
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Can't get a valid initial EGL display.");
	}

	eglInitialize(tmp_display, nullptr, nullptr);

	int version = gladLoaderLoadEGL(tmp_display);
	if (!version) {
		eglTerminate(tmp_display);
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Can't load EGL dynamic library.");
	}

	int major = GLAD_VERSION_MAJOR(version);
	int minor = GLAD_VERSION_MINOR(version);

	print_verbose(vformat("Loaded EGL %d.%d", major, minor));

	ERR_FAIL_COND_V_MSG(!GLAD_EGL_VERSION_1_4, ERR_UNAVAILABLE, vformat("EGL version is too old! %d.%d < 1.4", major, minor));

	eglTerminate(tmp_display);
#endif

#ifdef EGL_ANDROID_blob_cache
	shader_cache_dir = Engine::get_singleton()->get_shader_cache_path();
	if (shader_cache_dir.is_empty()) {
		shader_cache_dir = "user://";
	}
	Error err = OK;
	Ref<DirAccess> da = DirAccess::open(shader_cache_dir);
	if (da.is_null()) {
		ERR_PRINT("EGL: Can't create shader cache folder, no shader caching will happen: " + shader_cache_dir);
		shader_cache_dir = String();
	} else {
		err = da->change_dir(String("shader_cache").path_join("EGL"));
		if (err != OK) {
			err = da->make_dir_recursive(String("shader_cache").path_join("EGL"));
		}
		if (err != OK) {
			ERR_PRINT("EGL: Can't create shader cache folder, no shader caching will happen: " + shader_cache_dir);
			shader_cache_dir = String();
		} else {
			shader_cache_dir = shader_cache_dir.path_join(String("shader_cache").path_join("EGL"));
		}
	}
#endif

	String client_extensions_string = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

	// If the above method fails, we don't support client extensions, so there's nothing to check.
	if (eglGetError() == EGL_SUCCESS) {
		const char *platform = _get_platform_extension_name();
		if (!client_extensions_string.split(" ").has(platform)) {
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, vformat("EGL platform extension \"%s\" not found.", platform));
		}
	}

	return OK;
}

EGLManager::EGLManager() {
}

EGLManager::~EGLManager() {
	for (unsigned int i = 0; i < displays.size(); i++) {
		eglTerminate(displays[i].egl_display);
	}
}

#endif // EGL_ENABLED
