/**************************************************************************/
/*  openxr_opengl_extension.cpp                                           */
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

#include "openxr_opengl_extension.h"

#ifdef GLES3_ENABLED

#include "../../openxr_util.h"

#include "drivers/gles3/effects/copy_effects.h"
#include "drivers/gles3/storage/texture_storage.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering_server.h"

// OpenXR requires us to submit sRGB textures so that it recognizes the content
// as being in sRGB color space. We do fall back on "normal" textures but this
// will likely result in incorrect colors as OpenXR will double the sRGB conversion.
// All major XR runtimes support sRGB textures.

// In OpenGL output of the fragment shader is assumed to be in the color space of
// the developers choice, however a linear to sRGB HW conversion can be enabled
// through enabling GL_FRAMEBUFFER_SRGB if an sRGB color attachment is used.
// This is a global setting.
// See: https://www.khronos.org/opengl/wiki/Framebuffer

// In OpenGLES output of the fragment shader is assumed to be in linear color space
// and will be converted by default to sRGB if an sRGB color attachment is used.
// The extension GL_EXT_sRGB_write_control was introduced to enable turning this
// feature off.
// See: https://registry.khronos.org/OpenGL/extensions/EXT/EXT_sRGB_write_control.txt

HashMap<String, bool *> OpenXROpenGLExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

#ifdef ANDROID_ENABLED
	request_extensions[XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME] = nullptr;
#else
	request_extensions[XR_KHR_OPENGL_ENABLE_EXTENSION_NAME] = nullptr;
#endif
#if defined(LINUXBSD_ENABLED) && defined(EGL_ENABLED) && defined(WAYLAND_ENABLED)
	request_extensions[XR_MNDX_EGL_ENABLE_EXTENSION_NAME] = &egl_extension_enabled;
#endif

	return request_extensions;
}

void OpenXROpenGLExtension::on_instance_created(const XrInstance p_instance) {
	// Obtain pointers to functions we're accessing here.
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

#ifdef ANDROID_ENABLED
	EXT_INIT_XR_FUNC(xrGetOpenGLESGraphicsRequirementsKHR);
#else
	EXT_INIT_XR_FUNC(xrGetOpenGLGraphicsRequirementsKHR);
#endif
	EXT_INIT_XR_FUNC(xrEnumerateSwapchainImages);
}

bool OpenXROpenGLExtension::check_graphics_api_support(XrVersion p_desired_version) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);

	XrSystemId system_id = OpenXRAPI::get_singleton()->get_system_id();
	XrInstance instance = OpenXRAPI::get_singleton()->get_instance();

#ifdef ANDROID_ENABLED
	XrGraphicsRequirementsOpenGLESKHR opengl_requirements;
	opengl_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
	opengl_requirements.next = nullptr;

	XrResult result = xrGetOpenGLESGraphicsRequirementsKHR(instance, system_id, &opengl_requirements);
	if (!OpenXRAPI::get_singleton()->xr_result(result, "Failed to get OpenGL graphics requirements!")) {
		return false;
	}
#else
	XrGraphicsRequirementsOpenGLKHR opengl_requirements;
	opengl_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
	opengl_requirements.next = nullptr;

	XrResult result = xrGetOpenGLGraphicsRequirementsKHR(instance, system_id, &opengl_requirements);
	if (!OpenXRAPI::get_singleton()->xr_result(result, "Failed to get OpenGL graphics requirements!")) {
		return false;
	}
#endif

	if (p_desired_version < opengl_requirements.minApiVersionSupported) {
		print_line("OpenXR: Requested OpenGL version does not meet the minimum version this runtime supports.");
		print_line("- desired_version ", OpenXRUtil::make_xr_version_string(p_desired_version));
		print_line("- minApiVersionSupported ", OpenXRUtil::make_xr_version_string(opengl_requirements.minApiVersionSupported));
		print_line("- maxApiVersionSupported ", OpenXRUtil::make_xr_version_string(opengl_requirements.maxApiVersionSupported));
		return false;
	}

	if (p_desired_version > opengl_requirements.maxApiVersionSupported) {
		print_line("OpenXR: Requested OpenGL version exceeds the maximum version this runtime has been tested on and is known to support.");
		print_line("- desired_version ", OpenXRUtil::make_xr_version_string(p_desired_version));
		print_line("- minApiVersionSupported ", OpenXRUtil::make_xr_version_string(opengl_requirements.minApiVersionSupported));
		print_line("- maxApiVersionSupported ", OpenXRUtil::make_xr_version_string(opengl_requirements.maxApiVersionSupported));
	}

	return true;
}

#ifdef WIN32
XrGraphicsBindingOpenGLWin32KHR OpenXROpenGLExtension::graphics_binding_gl;
#elif defined(ANDROID_ENABLED)
XrGraphicsBindingOpenGLESAndroidKHR OpenXROpenGLExtension::graphics_binding_gl;
#elif defined(LINUXBSD_ENABLED)
#ifdef X11_ENABLED
XrGraphicsBindingOpenGLXlibKHR OpenXROpenGLExtension::graphics_binding_gl;
#endif
#if defined(EGL_ENABLED) && defined(WAYLAND_ENABLED)
XrGraphicsBindingEGLMNDX OpenXROpenGLExtension::graphics_binding_egl;
#endif
#endif

void *OpenXROpenGLExtension::set_session_create_and_get_next_pointer(void *p_next_pointer) {
	XrVersion desired_version = XR_MAKE_VERSION(3, 3, 0);

	if (!check_graphics_api_support(desired_version)) {
		print_line("OpenXR: Trying to initialize with OpenGL anyway...");
		//return p_next_pointer;
	}

	DisplayServer *display_server = DisplayServer::get_singleton();

#ifdef WIN32
	graphics_binding_gl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
	graphics_binding_gl.next = p_next_pointer;

	graphics_binding_gl.hDC = (HDC)display_server->window_get_native_handle(DisplayServer::WINDOW_VIEW);
	graphics_binding_gl.hGLRC = (HGLRC)display_server->window_get_native_handle(DisplayServer::OPENGL_CONTEXT);
#elif defined(ANDROID_ENABLED)
	graphics_binding_gl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
	graphics_binding_gl.next = p_next_pointer;

	graphics_binding_gl.display = (void *)display_server->window_get_native_handle(DisplayServer::DISPLAY_HANDLE);
	graphics_binding_gl.config = (EGLConfig)0; // https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/master/src/tests/hello_xr/graphicsplugin_opengles.cpp#L122
	graphics_binding_gl.context = (void *)display_server->window_get_native_handle(DisplayServer::OPENGL_CONTEXT);
#else
#if defined(EGL_ENABLED) && defined(WAYLAND_ENABLED)
	if (display_server->get_name() == "Wayland") {
		ERR_FAIL_COND_V_MSG(!egl_extension_enabled, p_next_pointer, "OpenXR cannot initialize on Wayland without the XR_MNDX_egl_enable extension.");

		graphics_binding_egl.type = XR_TYPE_GRAPHICS_BINDING_EGL_MNDX;
		graphics_binding_egl.next = p_next_pointer;

		graphics_binding_egl.getProcAddress = eglGetProcAddress;
		graphics_binding_egl.display = (void *)display_server->window_get_native_handle(DisplayServer::EGL_DISPLAY);
		graphics_binding_egl.config = (void *)display_server->window_get_native_handle(DisplayServer::EGL_CONFIG);
		graphics_binding_egl.context = (void *)display_server->window_get_native_handle(DisplayServer::OPENGL_CONTEXT);

		return &graphics_binding_egl;
	}
#endif
#if defined(X11_ENABLED)
	graphics_binding_gl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
	graphics_binding_gl.next = p_next_pointer;

	void *display_handle = (void *)display_server->window_get_native_handle(DisplayServer::DISPLAY_HANDLE);
	void *glxcontext_handle = (void *)display_server->window_get_native_handle(DisplayServer::OPENGL_CONTEXT);
	void *glxdrawable_handle = (void *)display_server->window_get_native_handle(DisplayServer::WINDOW_HANDLE);

	graphics_binding_gl.xDisplay = (Display *)display_handle;
	graphics_binding_gl.glxContext = (GLXContext)glxcontext_handle;
	graphics_binding_gl.glxDrawable = (GLXDrawable)glxdrawable_handle;

	// spec says to use proper values but runtimes don't care
	graphics_binding_gl.visualid = 0;
	graphics_binding_gl.glxFBConfig = nullptr;
#endif
#endif

#if defined(WIN32) || defined(ANDROID_ENABLED) || defined(X11_ENABLED)
	return &graphics_binding_gl;
#else
	return p_next_pointer;
#endif
}

void OpenXROpenGLExtension::get_usable_swapchain_formats(Vector<int64_t> &p_usable_swap_chains) {
	p_usable_swap_chains.push_back(GL_SRGB8_ALPHA8);
	p_usable_swap_chains.push_back(GL_RGBA8);
}

void OpenXROpenGLExtension::get_usable_depth_formats(Vector<int64_t> &p_usable_depth_formats) {
	p_usable_depth_formats.push_back(GL_DEPTH_COMPONENT32F);
	p_usable_depth_formats.push_back(GL_DEPTH24_STENCIL8);
	p_usable_depth_formats.push_back(GL_DEPTH32F_STENCIL8);
	p_usable_depth_formats.push_back(GL_DEPTH_COMPONENT24);
}

bool OpenXROpenGLExtension::get_swapchain_image_data(XrSwapchain p_swapchain, int64_t p_swapchain_format, uint32_t p_width, uint32_t p_height, uint32_t p_sample_count, uint32_t p_array_size, void **r_swapchain_graphics_data) {
	GLES3::TextureStorage *texture_storage = GLES3::TextureStorage::get_singleton();
	ERR_FAIL_NULL_V(texture_storage, false);

	uint32_t swapchain_length;
	XrResult result = xrEnumerateSwapchainImages(p_swapchain, 0, &swapchain_length, nullptr);
	if (XR_FAILED(result)) {
		print_line("OpenXR: Failed to get swapchaim image count [", OpenXRAPI::get_singleton()->get_error_string(result), "]");
		return false;
	}

#ifdef ANDROID_ENABLED
	XrSwapchainImageOpenGLESKHR *images = (XrSwapchainImageOpenGLESKHR *)memalloc(sizeof(XrSwapchainImageOpenGLESKHR) * swapchain_length);
#else
	XrSwapchainImageOpenGLKHR *images = (XrSwapchainImageOpenGLKHR *)memalloc(sizeof(XrSwapchainImageOpenGLKHR) * swapchain_length);
#endif
	ERR_FAIL_NULL_V_MSG(images, false, "OpenXR Couldn't allocate memory for swap chain image");

	for (uint64_t i = 0; i < swapchain_length; i++) {
#ifdef ANDROID_ENABLED
		images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
#else
		images[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
#endif
		images[i].next = nullptr;
		images[i].image = 0;
	}

	result = xrEnumerateSwapchainImages(p_swapchain, swapchain_length, &swapchain_length, (XrSwapchainImageBaseHeader *)images);
	if (XR_FAILED(result)) {
		print_line("OpenXR: Failed to get swapchaim images [", OpenXRAPI::get_singleton()->get_error_string(result), "]");
		memfree(images);
		return false;
	}

	SwapchainGraphicsData *data = memnew(SwapchainGraphicsData);
	if (data == nullptr) {
		print_line("OpenXR: Failed to allocate memory for swapchain data");
		memfree(images);
		return false;
	}
	*r_swapchain_graphics_data = data;
	data->is_multiview = (p_array_size > 1);

	Image::Format format = Image::FORMAT_RGBA8;

	Vector<RID> texture_rids;

	for (uint64_t i = 0; i < swapchain_length; i++) {
		RID texture_rid = texture_storage->texture_create_from_native_handle(
				p_array_size == 1 ? RS::TEXTURE_TYPE_2D : RS::TEXTURE_TYPE_LAYERED,
				format,
				images[i].image,
				p_width,
				p_height,
				1,
				p_array_size);

		texture_rids.push_back(texture_rid);
	}

	data->texture_rids = texture_rids;

	memfree(images);

	return true;
}

bool OpenXROpenGLExtension::create_projection_fov(const XrFovf p_fov, double p_z_near, double p_z_far, Projection &r_camera_matrix) {
	OpenXRUtil::XrMatrix4x4f matrix;
	OpenXRUtil::XrMatrix4x4f_CreateProjectionFov(&matrix, OpenXRUtil::GRAPHICS_OPENGL, p_fov, (float)p_z_near, (float)p_z_far);

	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			r_camera_matrix.columns[j][i] = matrix.m[j * 4 + i];
		}
	}

	return true;
}

RID OpenXROpenGLExtension::get_texture(void *p_swapchain_graphics_data, int p_image_index) {
	SwapchainGraphicsData *data = (SwapchainGraphicsData *)p_swapchain_graphics_data;
	ERR_FAIL_NULL_V(data, RID());

	ERR_FAIL_INDEX_V(p_image_index, data->texture_rids.size(), RID());
	return data->texture_rids[p_image_index];
}

void OpenXROpenGLExtension::cleanup_swapchain_graphics_data(void **p_swapchain_graphics_data) {
	if (*p_swapchain_graphics_data == nullptr) {
		return;
	}

	GLES3::TextureStorage *texture_storage = GLES3::TextureStorage::get_singleton();
	ERR_FAIL_NULL(texture_storage);

	SwapchainGraphicsData *data = (SwapchainGraphicsData *)*p_swapchain_graphics_data;

	for (const RID &texture_rid : data->texture_rids) {
		texture_storage->texture_free(texture_rid);
	}
	data->texture_rids.clear();

	memdelete(data);
	*p_swapchain_graphics_data = nullptr;
}

#define ENUM_TO_STRING_CASE(e) \
	case e: {                  \
		return String(#e);     \
	} break;

String OpenXROpenGLExtension::get_swapchain_format_name(int64_t p_swapchain_format) const {
	// These are somewhat different per platform, will need to weed some stuff out...
	switch (p_swapchain_format) {
#ifdef ANDROID_ENABLED
		// using definitions from GLES3/gl3.h

		ENUM_TO_STRING_CASE(GL_RGBA4)
		ENUM_TO_STRING_CASE(GL_RGB5_A1)
		ENUM_TO_STRING_CASE(GL_RGB565)
		ENUM_TO_STRING_CASE(GL_RGB8)
		ENUM_TO_STRING_CASE(GL_RGBA8)
		ENUM_TO_STRING_CASE(GL_RGB10_A2)
		ENUM_TO_STRING_CASE(GL_RGBA32F)
		ENUM_TO_STRING_CASE(GL_RGB32F)
		ENUM_TO_STRING_CASE(GL_RGBA16F)
		ENUM_TO_STRING_CASE(GL_RGB16F)
		ENUM_TO_STRING_CASE(GL_R11F_G11F_B10F)
		ENUM_TO_STRING_CASE(GL_UNSIGNED_INT_10F_11F_11F_REV)
		ENUM_TO_STRING_CASE(GL_RGB9_E5)
		ENUM_TO_STRING_CASE(GL_UNSIGNED_INT_5_9_9_9_REV)
		ENUM_TO_STRING_CASE(GL_RGBA32UI)
		ENUM_TO_STRING_CASE(GL_RGB32UI)
		ENUM_TO_STRING_CASE(GL_RGBA16UI)
		ENUM_TO_STRING_CASE(GL_RGB16UI)
		ENUM_TO_STRING_CASE(GL_RGBA8UI)
		ENUM_TO_STRING_CASE(GL_RGB8UI)
		ENUM_TO_STRING_CASE(GL_RGBA32I)
		ENUM_TO_STRING_CASE(GL_RGB32I)
		ENUM_TO_STRING_CASE(GL_RGBA16I)
		ENUM_TO_STRING_CASE(GL_RGB16I)
		ENUM_TO_STRING_CASE(GL_RGBA8I)
		ENUM_TO_STRING_CASE(GL_RGB8I)
		ENUM_TO_STRING_CASE(GL_RG)
		ENUM_TO_STRING_CASE(GL_RG_INTEGER)
		ENUM_TO_STRING_CASE(GL_R8)
		ENUM_TO_STRING_CASE(GL_RG8)
		ENUM_TO_STRING_CASE(GL_R16F)
		ENUM_TO_STRING_CASE(GL_R32F)
		ENUM_TO_STRING_CASE(GL_RG16F)
		ENUM_TO_STRING_CASE(GL_RG32F)
		ENUM_TO_STRING_CASE(GL_R8I)
		ENUM_TO_STRING_CASE(GL_R8UI)
		ENUM_TO_STRING_CASE(GL_R16I)
		ENUM_TO_STRING_CASE(GL_R16UI)
		ENUM_TO_STRING_CASE(GL_R32I)
		ENUM_TO_STRING_CASE(GL_R32UI)
		ENUM_TO_STRING_CASE(GL_RG8I)
		ENUM_TO_STRING_CASE(GL_RG8UI)
		ENUM_TO_STRING_CASE(GL_RG16I)
		ENUM_TO_STRING_CASE(GL_RG16UI)
		ENUM_TO_STRING_CASE(GL_RG32I)
		ENUM_TO_STRING_CASE(GL_RG32UI)
		ENUM_TO_STRING_CASE(GL_R8_SNORM)
		ENUM_TO_STRING_CASE(GL_RG8_SNORM)
		ENUM_TO_STRING_CASE(GL_RGB8_SNORM)
		ENUM_TO_STRING_CASE(GL_RGBA8_SNORM)
		ENUM_TO_STRING_CASE(GL_RGB10_A2UI)
		ENUM_TO_STRING_CASE(GL_SRGB)
		ENUM_TO_STRING_CASE(GL_SRGB8)
		ENUM_TO_STRING_CASE(GL_SRGB8_ALPHA8)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_R11_EAC)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_SIGNED_R11_EAC)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_RG11_EAC)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_SIGNED_RG11_EAC)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_RGB8_ETC2)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_SRGB8_ETC2)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_RGBA8_ETC2_EAC)
		ENUM_TO_STRING_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
		ENUM_TO_STRING_CASE(GL_DEPTH_COMPONENT16)
		ENUM_TO_STRING_CASE(GL_DEPTH_COMPONENT24)
		ENUM_TO_STRING_CASE(GL_DEPTH24_STENCIL8)

#else
		// using definitions from GLAD
		ENUM_TO_STRING_CASE(GL_R8_SNORM)
		ENUM_TO_STRING_CASE(GL_RG8_SNORM)
		ENUM_TO_STRING_CASE(GL_RGB8_SNORM)
		ENUM_TO_STRING_CASE(GL_RGBA8_SNORM)
		ENUM_TO_STRING_CASE(GL_R16_SNORM)
		ENUM_TO_STRING_CASE(GL_RG16_SNORM)
		ENUM_TO_STRING_CASE(GL_RGB16_SNORM)
		ENUM_TO_STRING_CASE(GL_RGBA16_SNORM)
		ENUM_TO_STRING_CASE(GL_RGB4)
		ENUM_TO_STRING_CASE(GL_RGB5)
		ENUM_TO_STRING_CASE(GL_RGB8)
		ENUM_TO_STRING_CASE(GL_RGB10)
		ENUM_TO_STRING_CASE(GL_RGB12)
		ENUM_TO_STRING_CASE(GL_RGB16)
		ENUM_TO_STRING_CASE(GL_RGBA2)
		ENUM_TO_STRING_CASE(GL_RGBA4)
		ENUM_TO_STRING_CASE(GL_RGB5_A1)
		ENUM_TO_STRING_CASE(GL_RGBA8)
		ENUM_TO_STRING_CASE(GL_RGB10_A2)
		ENUM_TO_STRING_CASE(GL_RGBA12)
		ENUM_TO_STRING_CASE(GL_RGBA16)
		ENUM_TO_STRING_CASE(GL_RGBA32F)
		ENUM_TO_STRING_CASE(GL_RGB32F)
		ENUM_TO_STRING_CASE(GL_RGBA16F)
		ENUM_TO_STRING_CASE(GL_RGB16F)
		ENUM_TO_STRING_CASE(GL_RGBA32UI)
		ENUM_TO_STRING_CASE(GL_RGB32UI)
		ENUM_TO_STRING_CASE(GL_RGBA16UI)
		ENUM_TO_STRING_CASE(GL_RGB16UI)
		ENUM_TO_STRING_CASE(GL_RGBA8UI)
		ENUM_TO_STRING_CASE(GL_RGB8UI)
		ENUM_TO_STRING_CASE(GL_RGBA32I)
		ENUM_TO_STRING_CASE(GL_RGB32I)
		ENUM_TO_STRING_CASE(GL_RGBA16I)
		ENUM_TO_STRING_CASE(GL_RGB16I)
		ENUM_TO_STRING_CASE(GL_RGBA8I)
		ENUM_TO_STRING_CASE(GL_RGB8I)
		ENUM_TO_STRING_CASE(GL_RGB10_A2UI)
		ENUM_TO_STRING_CASE(GL_SRGB)
		ENUM_TO_STRING_CASE(GL_SRGB8)
		ENUM_TO_STRING_CASE(GL_SRGB_ALPHA)
		ENUM_TO_STRING_CASE(GL_SRGB8_ALPHA8)
		ENUM_TO_STRING_CASE(GL_DEPTH_COMPONENT16)
		ENUM_TO_STRING_CASE(GL_DEPTH_COMPONENT24)
		ENUM_TO_STRING_CASE(GL_DEPTH_COMPONENT32)
		ENUM_TO_STRING_CASE(GL_DEPTH24_STENCIL8)
		ENUM_TO_STRING_CASE(GL_R11F_G11F_B10F)
		ENUM_TO_STRING_CASE(GL_DEPTH_COMPONENT32F)
		ENUM_TO_STRING_CASE(GL_DEPTH32F_STENCIL8)
#endif
		default: {
			return String("Swapchain format 0x") + String::num_int64(p_swapchain_format, 16);
		} break;
	}
}

#endif // GLES3_ENABLED
