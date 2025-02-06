/**************************************************************************/
/*  openxr_fb_update_swapchain_extension.cpp                              */
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

#include "openxr_fb_update_swapchain_extension.h"

// Always include this as late as possible.
#include "../openxr_platform_inc.h"

#ifndef GL_CUBIC_IMG
#define GL_CUBIC_IMG 0x9139
#endif
#ifndef GL_CUBIC_MIPMAP_LINEAR_IMG
#define GL_CUBIC_MIPMAP_LINEAR_IMG 0x913B
#endif
#ifndef GL_CUBIC_MIPMAP_NEAREST_IMG
#define GL_CUBIC_MIPMAP_NEAREST_IMG 0x913A
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

OpenXRFBUpdateSwapchainExtension *OpenXRFBUpdateSwapchainExtension::singleton = nullptr;

OpenXRFBUpdateSwapchainExtension *OpenXRFBUpdateSwapchainExtension::get_singleton() {
	return singleton;
}

OpenXRFBUpdateSwapchainExtension::OpenXRFBUpdateSwapchainExtension(const String &p_rendering_driver) {
	singleton = this;
	rendering_driver = p_rendering_driver;
}

OpenXRFBUpdateSwapchainExtension::~OpenXRFBUpdateSwapchainExtension() {
	singleton = nullptr;
}

HashMap<String, bool *> OpenXRFBUpdateSwapchainExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	request_extensions[XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME] = &fb_swapchain_update_state_ext;

	if (rendering_driver == "vulkan") {
#ifdef XR_USE_GRAPHICS_API_VULKAN
		request_extensions[XR_FB_SWAPCHAIN_UPDATE_STATE_VULKAN_EXTENSION_NAME] = &fb_swapchain_update_state_vulkan_ext;
#endif
	} else if (rendering_driver == "opengl3") {
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
		request_extensions[XR_FB_SWAPCHAIN_UPDATE_STATE_OPENGL_ES_EXTENSION_NAME] = &fb_swapchain_update_state_opengles_ext;
#endif
	}

#ifdef ANDROID_ENABLED
	request_extensions[XR_FB_SWAPCHAIN_UPDATE_STATE_ANDROID_SURFACE_EXTENSION_NAME] = &fb_swapchain_update_state_android_ext;
#endif

	return request_extensions;
}

void OpenXRFBUpdateSwapchainExtension::on_instance_created(const XrInstance p_instance) {
	if (fb_swapchain_update_state_ext) {
		EXT_INIT_XR_FUNC(xrUpdateSwapchainFB);
		EXT_INIT_XR_FUNC(xrGetSwapchainStateFB);
	}

	if (fb_swapchain_update_state_vulkan_ext) {
		// nothing to register here...
	}

	if (fb_swapchain_update_state_opengles_ext) {
		// nothing to register here...
	}
}

void OpenXRFBUpdateSwapchainExtension::on_instance_destroyed() {
	fb_swapchain_update_state_ext = false;
	fb_swapchain_update_state_vulkan_ext = false;
	fb_swapchain_update_state_opengles_ext = false;
}

bool OpenXRFBUpdateSwapchainExtension::is_enabled() const {
	if (rendering_driver == "vulkan") {
		return fb_swapchain_update_state_ext && fb_swapchain_update_state_vulkan_ext;
	} else if (rendering_driver == "opengl3") {
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
		return fb_swapchain_update_state_ext && fb_swapchain_update_state_opengles_ext;
#else
		return fb_swapchain_update_state_ext;
#endif
	}

	return false;
}

bool OpenXRFBUpdateSwapchainExtension::is_android_ext_enabled() const {
	return fb_swapchain_update_state_android_ext;
}

void OpenXRFBUpdateSwapchainExtension::update_swapchain_state(XrSwapchain p_swapchain, const SwapchainState *p_swapchain_state) {
	if (!p_swapchain_state) {
		return;
	}

	if (rendering_driver == "vulkan") {
#ifdef XR_USE_GRAPHICS_API_VULKAN
		if (!fb_swapchain_update_state_ext || !fb_swapchain_update_state_vulkan_ext) {
			return;
		}

		Color border_color = p_swapchain_state->border_color;
		XrSwapchainStateSamplerVulkanFB swapchain_state = {
			XR_TYPE_SWAPCHAIN_STATE_SAMPLER_VULKAN_FB, // type
			nullptr, // next
			(VkFilter)filter_to_vk(p_swapchain_state->min_filter), // minFilter
			(VkFilter)filter_to_vk(p_swapchain_state->mag_filter), // magFilter
			(VkSamplerMipmapMode)mipmap_mode_to_vk(p_swapchain_state->mipmap_mode), // mipmapMode
			(VkSamplerAddressMode)wrap_to_vk(p_swapchain_state->horizontal_wrap), // wrapModeS;
			(VkSamplerAddressMode)wrap_to_vk(p_swapchain_state->vertical_wrap), // wrapModeT
			(VkComponentSwizzle)swizzle_to_vk(p_swapchain_state->red_swizzle), // swizzleRed
			(VkComponentSwizzle)swizzle_to_vk(p_swapchain_state->green_swizzle), // swizzleGreen
			(VkComponentSwizzle)swizzle_to_vk(p_swapchain_state->blue_swizzle), // swizzleBlue
			(VkComponentSwizzle)swizzle_to_vk(p_swapchain_state->alpha_swizzle), // swizzleAlpha
			p_swapchain_state->max_anisotropy, // maxAnisotropy
			{ border_color.r, border_color.g, border_color.b, border_color.a } // borderColor
		};

		XrResult result = xrUpdateSwapchainFB(p_swapchain, (XrSwapchainStateBaseHeaderFB *)&swapchain_state);
		if (XR_FAILED(result)) {
			print_error(vformat("OpenXR: Failed to update swapchain [%s]", OpenXRAPI::get_singleton()->get_error_string(result)));
			return;
		}
#endif
	} else if (rendering_driver == "opengl3") {
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
		if (!fb_swapchain_update_state_ext || !fb_swapchain_update_state_opengles_ext) {
			return;
		}

		Color border_color = p_swapchain_state->border_color;
		XrSwapchainStateSamplerOpenGLESFB swapchain_state = {
			XR_TYPE_SWAPCHAIN_STATE_SAMPLER_OPENGL_ES_FB, // type
			nullptr, // next
			filter_to_gl(p_swapchain_state->min_filter, p_swapchain_state->mipmap_mode), // minFilter
			filter_to_gl(p_swapchain_state->mag_filter), // magFilter
			wrap_to_gl(p_swapchain_state->horizontal_wrap), // wrapModeS;
			wrap_to_gl(p_swapchain_state->vertical_wrap), // wrapModeT
			swizzle_to_gl(p_swapchain_state->red_swizzle), // swizzleRed
			swizzle_to_gl(p_swapchain_state->green_swizzle), // swizzleGreen
			swizzle_to_gl(p_swapchain_state->blue_swizzle), // swizzleBlue
			swizzle_to_gl(p_swapchain_state->alpha_swizzle), // swizzleAlpha
			p_swapchain_state->max_anisotropy, // maxAnisotropy
			{ border_color.r, border_color.g, border_color.b, border_color.a } // borderColor
		};

		XrResult result = xrUpdateSwapchainFB(p_swapchain, (XrSwapchainStateBaseHeaderFB *)&swapchain_state);
		if (XR_FAILED(result)) {
			print_error(vformat("OpenXR: Failed to update swapchain [%s]", OpenXRAPI::get_singleton()->get_error_string(result)));
			return;
		}
#endif
	}
}

void OpenXRFBUpdateSwapchainExtension::update_swapchain_surface_size(XrSwapchain p_swapchain, const Size2i &p_size) {
#ifdef ANDROID_ENABLED
	if (!fb_swapchain_update_state_ext || !fb_swapchain_update_state_android_ext) {
		return;
	}

	XrSwapchainStateAndroidSurfaceDimensionsFB swapchain_state = {
		XR_TYPE_SWAPCHAIN_STATE_ANDROID_SURFACE_DIMENSIONS_FB, // type
		nullptr, // next
		(uint32_t)p_size.width, // width
		(uint32_t)p_size.height // height
	};

	XrResult result = xrUpdateSwapchainFB(p_swapchain, (XrSwapchainStateBaseHeaderFB *)&swapchain_state);
	if (XR_FAILED(result)) {
		print_error(vformat("OpenXR: Failed to update swapchain surface size [%s]", OpenXRAPI::get_singleton()->get_error_string(result)));
	}
#endif
}

uint32_t OpenXRFBUpdateSwapchainExtension::filter_to_gl(OpenXRViewportCompositionLayerProvider::Filter p_filter, OpenXRViewportCompositionLayerProvider::MipmapMode p_mipmap_mode) {
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
	switch (p_mipmap_mode) {
		case OpenXRViewportCompositionLayerProvider::MipmapMode::MIPMAP_MODE_DISABLED:
			switch (p_filter) {
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_NEAREST:
					return GL_NEAREST;
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_LINEAR:
					return GL_LINEAR;
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_CUBIC:
					return GL_CUBIC_IMG;
			}
		case OpenXRViewportCompositionLayerProvider::MipmapMode::MIPMAP_MODE_NEAREST:
			switch (p_filter) {
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_NEAREST:
					return GL_NEAREST_MIPMAP_NEAREST;
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_LINEAR:
					return GL_LINEAR_MIPMAP_NEAREST;
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_CUBIC:
					return GL_CUBIC_MIPMAP_NEAREST_IMG;
			}
		case OpenXRViewportCompositionLayerProvider::MipmapMode::MIPMAP_MODE_LINEAR:
			switch (p_filter) {
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_NEAREST:
					return GL_NEAREST_MIPMAP_LINEAR;
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_LINEAR:
					return GL_LINEAR_MIPMAP_LINEAR;
				case OpenXRViewportCompositionLayerProvider::Filter::FILTER_CUBIC:
					return GL_CUBIC_MIPMAP_LINEAR_IMG;
			}
	}
#endif
	return 0;
}

uint32_t OpenXRFBUpdateSwapchainExtension::wrap_to_gl(OpenXRViewportCompositionLayerProvider::Wrap p_wrap) {
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
	switch (p_wrap) {
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_CLAMP_TO_BORDER:
			return GL_CLAMP_TO_BORDER;
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_CLAMP_TO_EDGE:
			return GL_CLAMP_TO_EDGE;
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_REPEAT:
			return GL_REPEAT;
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_MIRRORED_REPEAT:
			return GL_MIRRORED_REPEAT;
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_MIRROR_CLAMP_TO_EDGE:
			return GL_CLAMP_TO_EDGE;
	}
#endif
	return 0;
}

uint32_t OpenXRFBUpdateSwapchainExtension::swizzle_to_gl(OpenXRViewportCompositionLayerProvider::Swizzle p_swizzle) {
#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
	switch (p_swizzle) {
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_RED:
			return GL_RED;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_GREEN:
			return GL_GREEN;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_BLUE:
			return GL_BLUE;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_ALPHA:
			return GL_ALPHA;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_ZERO:
			return GL_ZERO;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_ONE:
			return GL_ONE;
	}
#endif
	return 0;
}

uint32_t OpenXRFBUpdateSwapchainExtension::filter_to_vk(OpenXRViewportCompositionLayerProvider::Filter p_filter) {
#ifdef XR_USE_GRAPHICS_API_VULKAN
	switch (p_filter) {
		case OpenXRViewportCompositionLayerProvider::Filter::FILTER_NEAREST:
			return VK_FILTER_NEAREST;
		case OpenXRViewportCompositionLayerProvider::Filter::FILTER_LINEAR:
			return VK_FILTER_LINEAR;
		case OpenXRViewportCompositionLayerProvider::Filter::FILTER_CUBIC:
			return VK_FILTER_CUBIC_EXT;
	}
#endif
	return 0;
}

uint32_t OpenXRFBUpdateSwapchainExtension::mipmap_mode_to_vk(OpenXRViewportCompositionLayerProvider::MipmapMode p_mipmap_mode) {
#ifdef XR_USE_GRAPHICS_API_VULKAN
	switch (p_mipmap_mode) {
		case OpenXRViewportCompositionLayerProvider::MipmapMode::MIPMAP_MODE_DISABLED:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		case OpenXRViewportCompositionLayerProvider::MipmapMode::MIPMAP_MODE_NEAREST:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case OpenXRViewportCompositionLayerProvider::MipmapMode::MIPMAP_MODE_LINEAR:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
#endif
	return 0;
}

uint32_t OpenXRFBUpdateSwapchainExtension::wrap_to_vk(OpenXRViewportCompositionLayerProvider::Wrap p_wrap) {
#ifdef XR_USE_GRAPHICS_API_VULKAN
	switch (p_wrap) {
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_CLAMP_TO_BORDER:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case OpenXRViewportCompositionLayerProvider::Wrap::WRAP_MIRROR_CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
	}
#endif
	return 0;
}

uint32_t OpenXRFBUpdateSwapchainExtension::swizzle_to_vk(OpenXRViewportCompositionLayerProvider::Swizzle p_swizzle) {
#ifdef XR_USE_GRAPHICS_API_VULKAN
	switch (p_swizzle) {
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_RED:
			return VK_COMPONENT_SWIZZLE_R;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_GREEN:
			return VK_COMPONENT_SWIZZLE_G;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_BLUE:
			return VK_COMPONENT_SWIZZLE_B;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_ALPHA:
			return VK_COMPONENT_SWIZZLE_A;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_ZERO:
			return VK_COMPONENT_SWIZZLE_ZERO;
		case OpenXRViewportCompositionLayerProvider::Swizzle::SWIZZLE_ONE:
			return VK_COMPONENT_SWIZZLE_ONE;
	}
#endif
	return 0;
}
