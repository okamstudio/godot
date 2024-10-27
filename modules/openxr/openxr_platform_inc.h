/**************************************************************************/
/*  openxr_platform_inc.h                                                 */
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

#ifndef OPENXR_PLATFORM_INC_H
#define OPENXR_PLATFORM_INC_H

// In various places we need to include platform definitions but we can't
// include these in our normal header files as we'll end up with issues.

#ifdef VULKAN_ENABLED
#define XR_USE_GRAPHICS_API_VULKAN
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#endif // VULKAN_ENABLED

#if defined(GLES3_ENABLED) && !defined(MACOS_ENABLED)
#ifdef ANDROID_ENABLED
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#define XR_USE_GRAPHICS_API_OPENGL
#endif // ANDROID_ENABLED
#if defined(LINUXBSD_ENABLED) && defined(EGL_ENABLED)
#ifdef GLAD_ENABLED
#include "thirdparty/glad/glad/egl.h"
#else
#include <EGL/egl.h>
#endif // GLAD_ENABLED
#endif // defined(LINUXBSD_ENABLED) && defined(EGL_ENABLED)
#ifdef X11_ENABLED
#define GL_GLEXT_PROTOTYPES 1
#define GL3_PROTOTYPES 1
#include "thirdparty/glad/glad/gl.h"
#include "thirdparty/glad/glad/glx.h"
#endif // X11_ENABLED
#endif // defined(GLES3_ENABLED) && !defined(MACOS_ENABLED)

#ifdef X11_ENABLED
#include <X11/Xlib.h>
#endif // X11_ENABLED

#ifdef WINDOWS_ENABLED
// Including windows.h here is absolutely evil, we shouldn't be doing this outside of platform
// however due to the way the openxr headers are put together, we have no choice.
#include <windows.h>
#endif // WINDOWS_ENABLED

#ifdef ANDROID_ENABLED
// The jobject type from jni.h is used by openxr_platform.h on Android.
#include <jni.h>
#endif // ANDROID_ENABLED

// Include platform dependent structs.
#include <openxr/openxr_platform.h>

#endif // OPENXR_PLATFORM_INC_H
