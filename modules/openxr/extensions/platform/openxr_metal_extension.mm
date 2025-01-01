/**************************************************************************/
/*  openxr_metal_extension.mm                                             */
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

#include "openxr_metal_extension.h"

#include "../../openxr_util.h"
#include "drivers/metal/rendering_device_driver_metal.h"
#include "servers/rendering/rendering_server_globals.h"

HashMap<String, bool *> OpenXRMetalExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	request_extensions[XR_KHR_METAL_ENABLE_EXTENSION_NAME] = nullptr;

	return request_extensions;
}

void OpenXRMetalExtension::on_instance_created(const XrInstance p_instance) {
	// Obtain pointers to functions we're accessing here.
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	EXT_INIT_XR_FUNC(xrGetMetalGraphicsRequirementsKHR);
	EXT_INIT_XR_FUNC(xrEnumerateSwapchainImages);
}

bool OpenXRMetalExtension::check_graphics_api_support() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);

	// TODO We may need to do a callback like we do in Vulkan where we run this first
	// and provide the obtained metalDevice to our Metal driver to use.
	// But according to Stuart Macs only have 1 device so it should always be the
	// same one and we should be able to get away with not doing this just yet.
	// If we do go forward with this, this means that just like with Vulkan,
	// we have to start with OpenXR before Metal can be setup, and we thus
	// can't support applications that want to add XR as an optional/temporary
	// feature that users enable when needed.

	XrSystemId system_id = OpenXRAPI::get_singleton()->get_system_id();
	XrInstance instance = OpenXRAPI::get_singleton()->get_instance();

	XrGraphicsRequirementsMetalKHR metal_requirements;
	metal_requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_METAL_KHR;
	metal_requirements.next = nullptr;
	metal_requirements.metalDevice = nullptr;

	XrResult result = xrGetMetalGraphicsRequirementsKHR(instance, system_id, &metal_requirements);
	if (!OpenXRAPI::get_singleton()->xr_result(result, "Failed to get Metal graphics requirements!")) {
		return false;
	}

	// See what metal device we are using...
	RenderingServer *rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL_V(rendering_server, false);
	RenderingDevice *rendering_device = rendering_server->get_rendering_device();
	ERR_FAIL_NULL_V(rendering_device, false);

	void *our_metal_device = (void *)rendering_device->get_driver_resource(RD::DRIVER_RESOURCE_LOGICAL_DEVICE);

	// Make sure we're using the same one.
	ERR_FAIL_COND_V(metal_requirements.metalDevice != our_metal_device, false);

	return true;
}

XrGraphicsBindingMetalKHR OpenXRMetalExtension::graphics_binding_metal;

void *OpenXRMetalExtension::set_session_create_and_get_next_pointer(void *p_next_pointer) {
	if (!check_graphics_api_support()) {
		return p_next_pointer;
	}

	RenderingServer *rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL_V(rendering_server, p_next_pointer);
	RenderingDevice *rendering_device = rendering_server->get_rendering_device();
	ERR_FAIL_NULL_V(rendering_device, p_next_pointer);

	graphics_binding_metal.type = XR_TYPE_GRAPHICS_BINDING_METAL_KHR;
	graphics_binding_metal.next = p_next_pointer;
	graphics_binding_metal.commandQueue = (void *)rendering_device->get_driver_resource(RD::DRIVER_RESOURCE_COMMAND_QUEUE);

	return &graphics_binding_metal;
}

void OpenXRMetalExtension::get_usable_swapchain_formats(Vector<int64_t> &p_usable_swap_chains) {
	p_usable_swap_chains.push_back(MTLPixelFormatRGBA8Unorm_sRGB);
	p_usable_swap_chains.push_back(MTLPixelFormatBGRA8Unorm_sRGB);
	p_usable_swap_chains.push_back(MTLPixelFormatRGBA8Uint);
}

void OpenXRMetalExtension::get_usable_depth_formats(Vector<int64_t> &p_usable_swap_chains) {
	p_usable_swap_chains.push_back(MTLPixelFormatDepth24Unorm_Stencil8);
	p_usable_swap_chains.push_back(MTLPixelFormatDepth32Float_Stencil8);
	p_usable_swap_chains.push_back(MTLPixelFormatDepth32Float);
}

#define ENUM_TO_STRING_CASE(m_e) \
	case m_e: {                  \
		return String(#m_e);     \
	} break;

String OpenXRMetalExtension::get_swapchain_format_name(int64_t p_swapchain_format) const {
	// This really should be in vulkan_context...
	MTLPixelFormat format = MTLPixelFormat(p_swapchain_format);
	switch (format) {
		ENUM_TO_STRING_CASE(MTLPixelFormatRGBA8Unorm)
		ENUM_TO_STRING_CASE(MTLPixelFormatRGBA8Unorm_sRGB)
		ENUM_TO_STRING_CASE(MTLPixelFormatBGRA8Unorm)
		ENUM_TO_STRING_CASE(MTLPixelFormatBGRA8Unorm_sRGB)
		ENUM_TO_STRING_CASE(MTLPixelFormatRGBA8Uint)
		ENUM_TO_STRING_CASE(MTLPixelFormatDepth24Unorm_Stencil8)
		ENUM_TO_STRING_CASE(MTLPixelFormatDepth32Float_Stencil8)
		ENUM_TO_STRING_CASE(MTLPixelFormatDepth32Float)
		default: {
			return String("Swapchain format ") + String::num_int64(int64_t(p_swapchain_format));
		} break;
	}
}

bool OpenXRMetalExtension::get_swapchain_image_data(XrSwapchain p_swapchain, int64_t p_swapchain_format, uint32_t p_width, uint32_t p_height, uint32_t p_sample_count, uint32_t p_array_size, void **r_swapchain_graphics_data) {
	LocalVector<XrSwapchainImageMetalKHR, uint32_t, false, true> images;

	RenderingServer *rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL_V(rendering_server, false);
	RenderingDevice *rendering_device = rendering_server->get_rendering_device();
	ERR_FAIL_NULL_V(rendering_device, false);

	uint32_t swapchain_length;
	XrResult result = xrEnumerateSwapchainImages(p_swapchain, 0, &swapchain_length, nullptr);
	if (XR_FAILED(result)) {
		print_line("OpenXR: Failed to get swapchaim image count [", OpenXRAPI::get_singleton()->get_error_string(result), "]");
		return false;
	}

	images.resize(swapchain_length);

	for (XrSwapchainImageMetalKHR &image : images) {
		image.type = XR_TYPE_SWAPCHAIN_IMAGE_METAL_KHR;
		image.next = nullptr;
		image.texture = nullptr;
	}

	result = xrEnumerateSwapchainImages(p_swapchain, swapchain_length, &swapchain_length, (XrSwapchainImageBaseHeader *)images.ptr());
	if (XR_FAILED(result)) {
		print_line("OpenXR: Failed to get swapchaim images [", OpenXRAPI::get_singleton()->get_error_string(result), "]");
		return false;
	}

	SwapchainGraphicsData *data = memnew(SwapchainGraphicsData);
	if (data == nullptr) {
		print_line("OpenXR: Failed to allocate memory for swapchain data");
		return false;
	}
	*r_swapchain_graphics_data = data;
	data->is_multiview = (p_array_size > 1);

	RenderingDevice::DataFormat format = RenderingDevice::DATA_FORMAT_R8G8B8A8_SRGB;
	RenderingDevice::TextureSamples samples = RenderingDevice::TEXTURE_SAMPLES_1;
	uint64_t usage_flags = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT;

	switch (p_swapchain_format) {
		case MTLPixelFormatRGBA8Unorm_sRGB:
			// Even though this is an sRGB framebuffer format we're using UNORM here.
			// The reason here is because Godot does a linear to sRGB conversion while
			// with the sRGB format, this conversion would be doubled by the hardware.
			// This also means we're reading the values as is for our preview on screen.
			// The OpenXR runtime however is still treating this as an sRGB format and
			// will thus do an sRGB -> Linear conversion as expected.
			//format = RenderingDevice::DATA_FORMAT_R8G8B8A8_SRGB;
			format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
			break;
		case MTLPixelFormatBGRA8Unorm_sRGB:
			format = RenderingDevice::DATA_FORMAT_B8G8R8A8_UNORM;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
			break;
		case MTLPixelFormatRGBA8Uint:
			format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UINT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
			break;
		case MTLPixelFormatDepth32Float:
			format = RenderingDevice::DATA_FORMAT_D32_SFLOAT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;
		case MTLPixelFormatDepth24Unorm_Stencil8:
			format = RenderingDevice::DATA_FORMAT_D24_UNORM_S8_UINT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;
		case MTLPixelFormatDepth32Float_Stencil8:
			format = RenderingDevice::DATA_FORMAT_D32_SFLOAT_S8_UINT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;
		default:
			// Continue with our default value.
			print_line("OpenXR: Unsupported swapchain format", p_swapchain_format);
			break;
	}

	switch (p_sample_count) {
		case 1:
			samples = RenderingDevice::TEXTURE_SAMPLES_1;
			break;
		case 2:
			samples = RenderingDevice::TEXTURE_SAMPLES_2;
			break;
		case 4:
			samples = RenderingDevice::TEXTURE_SAMPLES_4;
			break;
		case 8:
			samples = RenderingDevice::TEXTURE_SAMPLES_8;
			break;
		case 16:
			samples = RenderingDevice::TEXTURE_SAMPLES_16;
			break;
		case 32:
			samples = RenderingDevice::TEXTURE_SAMPLES_32;
			break;
		case 64:
			samples = RenderingDevice::TEXTURE_SAMPLES_64;
			break;
		default:
			// Continue with our default value.
			print_line("OpenXR: Unsupported sample count", p_sample_count);
			break;
	}

	Vector<RID> texture_rids;

	// Create Godot texture objects for each entry in our swapchain.
	for (uint64_t i = 0; i < swapchain_length; i++) {
		// Note, the formats we sent to render_device are ignored on metal.
		RID image_rid = rendering_device->texture_create_from_extension(
				p_array_size == 1 ? RenderingDevice::TEXTURE_TYPE_2D : RenderingDevice::TEXTURE_TYPE_2D_ARRAY,
				format,
				samples,
				usage_flags,
				(uint64_t)images[i].texture,
				p_width,
				p_height,
				1,
				p_array_size);

		texture_rids.push_back(image_rid);
	}

	data->texture_rids = texture_rids;

	return true;
}

void OpenXRMetalExtension::cleanup_swapchain_graphics_data(void **p_swapchain_graphics_data) {
	if (*p_swapchain_graphics_data == nullptr) {
		return;
	}

	SwapchainGraphicsData *data = (SwapchainGraphicsData *)*p_swapchain_graphics_data;

	RenderingServer *rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL(rendering_server);
	RenderingDevice *rendering_device = rendering_server->get_rendering_device();
	ERR_FAIL_NULL(rendering_device);

	for (const RID &texture_rid : data->texture_rids) {
		rendering_device->free(texture_rid);
	}
	data->texture_rids.clear();

	memdelete(data);
	*p_swapchain_graphics_data = nullptr;
}

bool OpenXRMetalExtension::create_projection_fov(const XrFovf p_fov, double p_z_near, double p_z_far, Projection &r_camera_matrix) {
	// Even though this is a Metal renderer we're using OpenGL coordinate systems.
	OpenXRUtil::XrMatrix4x4f matrix;
	OpenXRUtil::XrMatrix4x4f_CreateProjectionFov(&matrix, OpenXRUtil::GRAPHICS_OPENGL, p_fov, (float)p_z_near, (float)p_z_far);

	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			r_camera_matrix.columns[j][i] = matrix.m[j * 4 + i];
		}
	}

	return true;
}

RID OpenXRMetalExtension::get_texture(void *p_swapchain_graphics_data, int p_image_index) {
	SwapchainGraphicsData *data = (SwapchainGraphicsData *)p_swapchain_graphics_data;
	ERR_FAIL_NULL_V(data, RID());

	ERR_FAIL_INDEX_V(p_image_index, data->texture_rids.size(), RID());
	return data->texture_rids[p_image_index];
}
