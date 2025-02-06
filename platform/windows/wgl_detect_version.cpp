/**************************************************************************/
/*  wgl_detect_version.cpp                                                */
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

#if defined(WINDOWS_ENABLED) && defined(GLES3_ENABLED)

#include "wgl_detect_version.h"
#include "os_windows.h"

#include "core/string/print_string.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#include <windows.h>

#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_VENDOR 0x1F00
#define WGL_RENDERER 0x1F01
#define WGL_VERSION 0x1F02

#if defined(__GNUC__)
// Workaround GCC warning from -Wcast-function-type.
#define GetProcAddress (void *)GetProcAddress
#endif

using PFNWGLCREATECONTEXT = HGLRC(APIENTRY *)(HDC);
using PFNWGLDELETECONTEXT = BOOL(APIENTRY *)(HGLRC);
using PFNWGLMAKECURRENT = BOOL(APIENTRY *)(HDC, HGLRC);
using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC(APIENTRY *)(HDC, HGLRC, const int *);
using PFNWGLGETPROCADDRESS = void *(APIENTRY *)(LPCSTR);
using PFNWGLGETSTRINGPROC = const char *(APIENTRY *)(unsigned int);

Dictionary detect_wgl() {
	Dictionary gl_info;
	gl_info["version"] = 0;
	gl_info["vendor"] = String();
	gl_info["name"] = String();

	PFNWGLCREATECONTEXT gd_wglCreateContext;
	PFNWGLMAKECURRENT gd_wglMakeCurrent;
	PFNWGLDELETECONTEXT gd_wglDeleteContext;
	PFNWGLGETPROCADDRESS gd_wglGetProcAddress;

	HMODULE module = LoadLibraryW(L"opengl32.dll");
	if (!module) {
		return gl_info;
	}
	gd_wglCreateContext = (PFNWGLCREATECONTEXT)GetProcAddress(module, "wglCreateContext");
	gd_wglMakeCurrent = (PFNWGLMAKECURRENT)GetProcAddress(module, "wglMakeCurrent");
	gd_wglDeleteContext = (PFNWGLDELETECONTEXT)GetProcAddress(module, "wglDeleteContext");
	gd_wglGetProcAddress = (PFNWGLGETPROCADDRESS)GetProcAddress(module, "wglGetProcAddress");
	if (!gd_wglCreateContext || !gd_wglMakeCurrent || !gd_wglDeleteContext || !gd_wglGetProcAddress) {
		return gl_info;
	}

	LPCWSTR class_name = L"EngineWGLDetect";
	HINSTANCE hInstance = static_cast<OS_Windows *>(OS::get_singleton())->get_hinstance();
	WNDCLASSW wc = {};

	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = hInstance;
	wc.lpszClassName = class_name;

	RegisterClassW(&wc);

	HWND hWnd = CreateWindowExW(WS_EX_APPWINDOW, class_name, L"", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hInstance, nullptr);
	if (hWnd) {
		HDC hDC = GetDC(hWnd);
		if (hDC) {
			static PIXELFORMATDESCRIPTOR pfd = {
				sizeof(PIXELFORMATDESCRIPTOR), // Size Of This Pixel Format Descriptor
				1,
				PFD_DRAW_TO_WINDOW | // Format Must Support Window
						PFD_SUPPORT_OPENGL | // Format Must Support OpenGL
						PFD_DOUBLEBUFFER,
				(BYTE)PFD_TYPE_RGBA,
				(BYTE)(OS::get_singleton()->is_layered_allowed() ? 32 : 24),
				(BYTE)0, (BYTE)0, (BYTE)0, (BYTE)0, (BYTE)0, (BYTE)0, // Color Bits Ignored
				(BYTE)(OS::get_singleton()->is_layered_allowed() ? 8 : 0), // Alpha Buffer
				(BYTE)0, // Shift Bit Ignored
				(BYTE)0, // No Accumulation Buffer
				(BYTE)0, (BYTE)0, (BYTE)0, (BYTE)0, // Accumulation Bits Ignored
				(BYTE)24, // 24Bit Z-Buffer (Depth Buffer)
				(BYTE)0, // No Stencil Buffer
				(BYTE)0, // No Auxiliary Buffer
				(BYTE)PFD_MAIN_PLANE, // Main Drawing Layer
				(BYTE)0, // Reserved
				0, 0, 0 // Layer Masks Ignored
			};

			int pixel_format = ChoosePixelFormat(hDC, &pfd);
			SetPixelFormat(hDC, pixel_format, &pfd);

			HGLRC hRC = gd_wglCreateContext(hDC);
			if (hRC) {
				if (gd_wglMakeCurrent(hDC, hRC)) {
					int attribs[] = {
						WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
						WGL_CONTEXT_MINOR_VERSION_ARB, 3,
						WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
						WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
						0
					};

					PFNWGLCREATECONTEXTATTRIBSARBPROC gd_wglCreateContextAttribsARB = nullptr;
					gd_wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)gd_wglGetProcAddress("wglCreateContextAttribsARB");
					if (gd_wglCreateContextAttribsARB) {
						HGLRC new_hRC = gd_wglCreateContextAttribsARB(hDC, nullptr, attribs);
						if (new_hRC) {
							if (gd_wglMakeCurrent(hDC, new_hRC)) {
								PFNWGLGETSTRINGPROC gd_wglGetString = (PFNWGLGETSTRINGPROC)GetProcAddress(module, "glGetString");
								if (gd_wglGetString) {
									const char *prefixes[] = {
										"OpenGL ES-CM ",
										"OpenGL ES-CL ",
										"OpenGL ES ",
										"OpenGL SC ",
										nullptr
									};
									const char *version = (const char *)gd_wglGetString(WGL_VERSION);
									if (version) {
										const String device_vendor = String::utf8((const char *)gd_wglGetString(WGL_VENDOR)).strip_edges().trim_suffix(" Corporation");
										const String device_name = String::utf8((const char *)gd_wglGetString(WGL_RENDERER)).strip_edges().trim_suffix("/PCIe/SSE2");
										for (int i = 0; prefixes[i]; i++) {
											size_t length = strlen(prefixes[i]);
											if (strncmp(version, prefixes[i], length) == 0) {
												version += length;
												break;
											}
										}
										int major = 0;
										int minor = 0;
#ifdef _MSC_VER
										sscanf_s(version, "%d.%d", &major, &minor);
#else
										sscanf(version, "%d.%d", &major, &minor);
#endif
										print_verbose(vformat("Native OpenGL API detected: %d.%d: %s - %s", major, minor, device_vendor, device_name));
										gl_info["vendor"] = device_vendor;
										gl_info["name"] = device_name;
										gl_info["version"] = major * 10000 + minor;
									}
								}
							}
							gd_wglMakeCurrent(nullptr, nullptr);
							gd_wglDeleteContext(new_hRC);
						}
					}
				}
				gd_wglMakeCurrent(nullptr, nullptr);
				gd_wglDeleteContext(hRC);
			}
			ReleaseDC(hWnd, hDC);
		}
		DestroyWindow(hWnd);
	}
	UnregisterClassW(class_name, hInstance);

	return gl_info;
}

#endif // WINDOWS_ENABLED && GLES3_ENABLED
