/**************************************************************************/
/*  openxr_vulkan_extension.cpp                                           */
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

#include "openxr_vulkan_extension.h"

#include "../../openxr_util.h"

#include "core/string/print_string.h"
#include "servers/rendering/renderer_rd/effects/copy_effects.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/rendering_server_globals.h"
#include "servers/rendering_server.h"

HashMap<String, bool *> OpenXRVulkanExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	request_extensions[XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME] = nullptr; // must be available

	return request_extensions;
}

void OpenXRVulkanExtension::on_instance_created(const XrInstance p_instance) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	// Obtain pointers to functions we're accessing here, they are (not yet) part of core.

	EXT_INIT_XR_FUNC(xrGetVulkanGraphicsRequirements2KHR);
	EXT_INIT_XR_FUNC(xrCreateVulkanInstanceKHR);
	EXT_INIT_XR_FUNC(xrGetVulkanGraphicsDevice2KHR);
	EXT_INIT_XR_FUNC(xrCreateVulkanDeviceKHR);
	EXT_INIT_XR_FUNC(xrEnumerateSwapchainImages);
}

bool OpenXRVulkanExtension::check_graphics_api_support(XrVersion p_desired_version) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);

	XrGraphicsRequirementsVulkan2KHR vulkan_requirements = {
		XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR, // type
		nullptr, // next
		0, // minApiVersionSupported
		0 // maxApiVersionSupported
	};

	XrResult result = xrGetVulkanGraphicsRequirements2KHR(OpenXRAPI::get_singleton()->get_instance(), OpenXRAPI::get_singleton()->get_system_id(), &vulkan_requirements);
	if (XR_FAILED(result)) {
		print_line("OpenXR: Failed to get vulkan graphics requirements [", OpenXRAPI::get_singleton()->get_error_string(result), "]");
		return false;
	}

	// #ifdef DEBUG
	print_line("OpenXR: XrGraphicsRequirementsVulkan2KHR:");
	print_line(" - minApiVersionSupported: ", OpenXRUtil::make_xr_version_string(vulkan_requirements.minApiVersionSupported));
	print_line(" - maxApiVersionSupported: ", OpenXRUtil::make_xr_version_string(vulkan_requirements.maxApiVersionSupported));
	// #endif

	if (p_desired_version < vulkan_requirements.minApiVersionSupported) {
		print_line("OpenXR: Requested Vulkan version does not meet the minimum version this runtime supports.");
		print_line("- desired_version ", OpenXRUtil::make_xr_version_string(p_desired_version));
		print_line("- minApiVersionSupported ", OpenXRUtil::make_xr_version_string(vulkan_requirements.minApiVersionSupported));
		print_line("- maxApiVersionSupported ", OpenXRUtil::make_xr_version_string(vulkan_requirements.maxApiVersionSupported));
		return false;
	}

	if (p_desired_version > vulkan_requirements.maxApiVersionSupported) {
		print_line("OpenXR: Requested Vulkan version exceeds the maximum version this runtime has been tested on and is known to support.");
		print_line("- desired_version ", OpenXRUtil::make_xr_version_string(p_desired_version));
		print_line("- minApiVersionSupported ", OpenXRUtil::make_xr_version_string(vulkan_requirements.minApiVersionSupported));
		print_line("- maxApiVersionSupported ", OpenXRUtil::make_xr_version_string(vulkan_requirements.maxApiVersionSupported));
	}

	return true;
}

bool OpenXRVulkanExtension::create_vulkan_instance(const VkInstanceCreateInfo *p_vulkan_create_info, VkInstance *r_instance) {
	// get the vulkan version we are creating
	uint32_t vulkan_version = p_vulkan_create_info->pApplicationInfo->apiVersion;
	uint32_t major_version = VK_VERSION_MAJOR(vulkan_version);
	uint32_t minor_version = VK_VERSION_MINOR(vulkan_version);
	uint32_t patch_version = VK_VERSION_PATCH(vulkan_version);
	XrVersion desired_version = XR_MAKE_VERSION(major_version, minor_version, patch_version);

	// check if this is supported
	if (!check_graphics_api_support(desired_version)) {
		return false;
	}

	XrVulkanInstanceCreateInfoKHR xr_vulkan_instance_info = {
		XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR, // type
		nullptr, // next
		OpenXRAPI::get_singleton()->get_system_id(), // systemId
		0, // createFlags
		vkGetInstanceProcAddr, // pfnGetInstanceProcAddr
		p_vulkan_create_info, // vulkanCreateInfo
		nullptr, // vulkanAllocator
	};

	VkResult vk_result = VK_SUCCESS;
	XrResult result = xrCreateVulkanInstanceKHR(OpenXRAPI::get_singleton()->get_instance(), &xr_vulkan_instance_info, &vulkan_instance, &vk_result);
	if (XR_FAILED(result)) {
		print_line("OpenXR: Failed to create vulkan instance [", OpenXRAPI::get_singleton()->get_error_string(result), "]");
		return false;
	}

	ERR_FAIL_COND_V_MSG(vk_result == VK_ERROR_INCOMPATIBLE_DRIVER, false,
			"Cannot find a compatible Vulkan installable client driver (ICD).\n\n"
			"vkCreateInstance Failure");
	ERR_FAIL_COND_V_MSG(vk_result == VK_ERROR_EXTENSION_NOT_PRESENT, false,
			"Cannot find a specified extension library.\n"
			"Make sure your layers path is set appropriately.\n"
			"vkCreateInstance Failure");
	ERR_FAIL_COND_V_MSG(vk_result, false,
			"vkCreateInstance failed.\n\n"
			"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
			"Please look at the Getting Started guide for additional information.\n"
			"vkCreateInstance Failure");

	*r_instance = vulkan_instance;

	return true;
}

bool OpenXRVulkanExtension::get_physical_device(VkPhysicalDevice *r_device) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);

	XrVulkanGraphicsDeviceGetInfoKHR get_info = {
		XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR, // type
		nullptr, // next
		OpenXRAPI::get_singleton()->get_system_id(), // systemId
		vulkan_instance, // vulkanInstance
	};

	XrResult result = xrGetVulkanGraphicsDevice2KHR(OpenXRAPI::get_singleton()->get_instance(), &get_info, &vulkan_physical_device);
	if (XR_FAILED(result)) {
		print_line("OpenXR: Failed to obtain vulkan physical device [", OpenXRAPI::get_singleton()->get_error_string(result), "]");
		return false;
	}

	*r_device = vulkan_physical_device;

	return true;
}

bool OpenXRVulkanExtension::create_vulkan_device(const VkDeviceCreateInfo *p_device_create_info, VkDevice *r_device) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);

	XrVulkanDeviceCreateInfoKHR create_info = {
		XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR, // type
		nullptr, // next
		OpenXRAPI::get_singleton()->get_system_id(), // systemId
		0, // createFlags
		vkGetInstanceProcAddr, // pfnGetInstanceProcAddr
		vulkan_physical_device, // vulkanPhysicalDevice
		p_device_create_info, // vulkanCreateInfo
		nullptr // vulkanAllocator
	};

	VkResult vk_result = VK_SUCCESS;
	XrResult result = xrCreateVulkanDeviceKHR(OpenXRAPI::get_singleton()->get_instance(), &create_info, &vulkan_device, &vk_result);
	if (XR_FAILED(result)) {
		print_line("OpenXR: Failed to create vulkan device [", OpenXRAPI::get_singleton()->get_error_string(result), "]");
		return false;
	}

	if (vk_result != VK_SUCCESS) {
		print_line("OpenXR: Failed to create vulkan device [vulkan error", vk_result, "]");
	}

	*r_device = vulkan_device;

	return true;
}

void OpenXRVulkanExtension::set_direct_queue_family_and_index(uint32_t p_queue_family_index, uint32_t p_queue_index) {
	vulkan_queue_family_index = p_queue_family_index;
	vulkan_queue_index = p_queue_index;
}

XrGraphicsBindingVulkanKHR OpenXRVulkanExtension::graphics_binding_vulkan;

void *OpenXRVulkanExtension::set_session_create_and_get_next_pointer(void *p_next_pointer) {
	DEV_ASSERT(vulkan_queue_family_index < UINT32_MAX && "Direct queue family index was not specified yet.");
	DEV_ASSERT(vulkan_queue_index < UINT32_MAX && "Direct queue index was not specified yet.");

	graphics_binding_vulkan.type = XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR;
	graphics_binding_vulkan.next = p_next_pointer;
	graphics_binding_vulkan.instance = vulkan_instance;
	graphics_binding_vulkan.physicalDevice = vulkan_physical_device;
	graphics_binding_vulkan.device = vulkan_device;
	graphics_binding_vulkan.queueFamilyIndex = vulkan_queue_family_index;
	graphics_binding_vulkan.queueIndex = vulkan_queue_index;

	return &graphics_binding_vulkan;
}

void OpenXRVulkanExtension::get_usable_swapchain_formats(Vector<int64_t> &p_usable_swap_chains) {
	// We might want to do more here especially if we keep things in linear color space
	// Possibly add in R10G10B10A2 as an option if we're using the mobile renderer.
	p_usable_swap_chains.push_back(VK_FORMAT_R8G8B8A8_SRGB);
	p_usable_swap_chains.push_back(VK_FORMAT_B8G8R8A8_SRGB);
	p_usable_swap_chains.push_back(VK_FORMAT_R8G8B8A8_UINT);
	p_usable_swap_chains.push_back(VK_FORMAT_B8G8R8A8_UINT);
}

void OpenXRVulkanExtension::get_usable_depth_formats(Vector<int64_t> &p_usable_swap_chains) {
	// Note, it is very likely we do NOT support any of depth formats where we can combine our stencil support (e.g. _S8_UINT).
	// Right now this isn't a problem but once stencil support becomes an issue, we need to check for this in the rendering engine
	// and create a separate buffer for the stencil.

	p_usable_swap_chains.push_back(VK_FORMAT_D24_UNORM_S8_UINT);
	p_usable_swap_chains.push_back(VK_FORMAT_D32_SFLOAT_S8_UINT);
	p_usable_swap_chains.push_back(VK_FORMAT_D32_SFLOAT);
}

bool OpenXRVulkanExtension::get_swapchain_image_data(XrSwapchain p_swapchain, int64_t p_swapchain_format, uint32_t p_width, uint32_t p_height, uint32_t p_sample_count, uint32_t p_array_size, void **r_swapchain_graphics_data) {
	LocalVector<XrSwapchainImageVulkanKHR> images;

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

	for (XrSwapchainImageVulkanKHR &image : images) {
		image.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
		image.next = nullptr;
		image.image = VK_NULL_HANDLE;
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
		case VK_FORMAT_R8G8B8A8_SRGB:
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
		case VK_FORMAT_B8G8R8A8_SRGB:
			//format = RenderingDevice::DATA_FORMAT_B8G8R8A8_SRGB;
			format = RenderingDevice::DATA_FORMAT_B8G8R8A8_UNORM;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
			break;
		case VK_FORMAT_R8G8B8A8_UINT:
			format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UINT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
			break;
		case VK_FORMAT_B8G8R8A8_UINT:
			format = RenderingDevice::DATA_FORMAT_B8G8R8A8_UINT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
			break;
		case VK_FORMAT_D32_SFLOAT:
			format = RenderingDevice::DATA_FORMAT_D32_SFLOAT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;
		case VK_FORMAT_D24_UNORM_S8_UINT:
			format = RenderingDevice::DATA_FORMAT_D24_UNORM_S8_UINT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			format = RenderingDevice::DATA_FORMAT_D32_SFLOAT_S8_UINT;
			usage_flags |= RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			break;
		default:
			// continue with our default value
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
			// continue with our default value
			print_line("OpenXR: Unsupported sample count", p_sample_count);
			break;
	}

	Vector<RID> texture_rids;

	// create Godot texture objects for each entry in our swapchain
	for (const XrSwapchainImageVulkanKHR &swapchain_image : images) {
		RID image_rid = rendering_device->texture_create_from_extension(
				p_array_size == 1 ? RenderingDevice::TEXTURE_TYPE_2D : RenderingDevice::TEXTURE_TYPE_2D_ARRAY,
				format,
				samples,
				usage_flags,
				(uint64_t)swapchain_image.image,
				p_width,
				p_height,
				1,
				p_array_size);

		texture_rids.push_back(image_rid);
	}

	data->texture_rids = texture_rids;

	return true;
}

bool OpenXRVulkanExtension::create_projection_fov(const XrFovf p_fov, double p_z_near, double p_z_far, Projection &r_camera_matrix) {
	// Even though this is a Vulkan renderer we're using OpenGL coordinate systems.
	OpenXRUtil::XrMatrix4x4f matrix;
	OpenXRUtil::XrMatrix4x4f_CreateProjectionFov(&matrix, OpenXRUtil::GRAPHICS_OPENGL, p_fov, (float)p_z_near, (float)p_z_far);

	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			r_camera_matrix.columns[j][i] = matrix.m[j * 4 + i];
		}
	}

	return true;
}

RID OpenXRVulkanExtension::get_texture(void *p_swapchain_graphics_data, int p_image_index) {
	SwapchainGraphicsData *data = (SwapchainGraphicsData *)p_swapchain_graphics_data;
	ERR_FAIL_NULL_V(data, RID());

	ERR_FAIL_INDEX_V(p_image_index, data->texture_rids.size(), RID());
	return data->texture_rids[p_image_index];
}

void OpenXRVulkanExtension::cleanup_swapchain_graphics_data(void **p_swapchain_graphics_data) {
	if (*p_swapchain_graphics_data == nullptr) {
		return;
	}

	SwapchainGraphicsData *data = (SwapchainGraphicsData *)*p_swapchain_graphics_data;

	RenderingServer *rendering_server = RenderingServer::get_singleton();
	ERR_FAIL_NULL(rendering_server);
	RenderingDevice *rendering_device = rendering_server->get_rendering_device();
	ERR_FAIL_NULL(rendering_device);

	for (const RID &texture_rid : data->texture_rids) {
		// This should clean up our RIDs and associated texture objects but shouldn't destroy the images, they are owned by our XrSwapchain.
		rendering_device->free(texture_rid);
	}
	data->texture_rids.clear();

	memdelete(data);
	*p_swapchain_graphics_data = nullptr;
}

#define ENUM_TO_STRING_CASE(e) \
	case e: {                  \
		return String(#e);     \
	} break;

String OpenXRVulkanExtension::get_swapchain_format_name(int64_t p_swapchain_format) const {
	// This really should be in vulkan_context...
	VkFormat format = VkFormat(p_swapchain_format);
	switch (format) {
		ENUM_TO_STRING_CASE(VK_FORMAT_UNDEFINED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R4G4_UNORM_PACK8)
		ENUM_TO_STRING_CASE(VK_FORMAT_R4G4B4A4_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_B4G4R4A4_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_R5G6B5_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_B5G6R5_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_R5G5B5A1_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_B5G5R5A1_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_A1R5G5B5_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8_SRGB)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8_SRGB)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8_SRGB)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8_SRGB)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8A8_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8A8_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8A8_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8A8_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8A8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8A8_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R8G8B8A8_SRGB)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8A8_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8A8_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8A8_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8A8_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8A8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8A8_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8A8_SRGB)
		ENUM_TO_STRING_CASE(VK_FORMAT_A8B8G8R8_UNORM_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A8B8G8R8_SNORM_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A8B8G8R8_USCALED_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A8B8G8R8_SSCALED_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A8B8G8R8_UINT_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A8B8G8R8_SINT_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A8B8G8R8_SRGB_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2R10G10B10_UNORM_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2R10G10B10_SNORM_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2R10G10B10_USCALED_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2R10G10B10_SSCALED_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2R10G10B10_UINT_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2R10G10B10_SINT_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2B10G10R10_UNORM_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2B10G10R10_SNORM_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2B10G10R10_USCALED_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2B10G10R10_SSCALED_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2B10G10R10_UINT_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_A2B10G10R10_SINT_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16A16_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16A16_SNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16A16_USCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16A16_SSCALED)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16A16_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16A16_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R16G16B16A16_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32B32_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32B32_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32B32_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32B32A32_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32B32A32_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R32G32B32A32_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64B64_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64B64_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64B64_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64B64A64_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64B64A64_SINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_R64G64B64A64_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_D16_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_X8_D24_UNORM_PACK32)
		ENUM_TO_STRING_CASE(VK_FORMAT_D32_SFLOAT)
		ENUM_TO_STRING_CASE(VK_FORMAT_S8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_D16_UNORM_S8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_D24_UNORM_S8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_D32_SFLOAT_S8_UINT)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC1_RGB_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC1_RGB_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC1_RGBA_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC2_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC2_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC3_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC3_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC4_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC4_SNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC5_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC5_SNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC6H_UFLOAT_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC6H_SFLOAT_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC7_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_BC7_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_EAC_R11_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_EAC_R11_SNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_EAC_R11G11_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_EAC_R11G11_SNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_4x4_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_4x4_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_5x4_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_5x4_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_5x5_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_5x5_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_6x5_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_6x5_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_6x6_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_6x6_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x5_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x5_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x6_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x6_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x8_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x8_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x5_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x5_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x6_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x6_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x8_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x8_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x10_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x10_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_12x10_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_12x10_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_12x12_UNORM_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
		ENUM_TO_STRING_CASE(VK_FORMAT_G8B8G8R8_422_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_B8G8R8G8_422_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G8_B8R8_2PLANE_422_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_R10X6_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_R10X6G10X6_UNORM_2PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_R12X4_UNORM_PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_R12X4G12X4_UNORM_2PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16)
		ENUM_TO_STRING_CASE(VK_FORMAT_G16B16G16R16_422_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_B16G16R16G16_422_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G16_B16R16_2PLANE_420_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G16_B16R16_2PLANE_422_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM)
		ENUM_TO_STRING_CASE(VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG)
		ENUM_TO_STRING_CASE(VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG)
		ENUM_TO_STRING_CASE(VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG)
		ENUM_TO_STRING_CASE(VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG)
		ENUM_TO_STRING_CASE(VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG)
		ENUM_TO_STRING_CASE(VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG)
		ENUM_TO_STRING_CASE(VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG)
		ENUM_TO_STRING_CASE(VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT)
		ENUM_TO_STRING_CASE(VK_FORMAT_MAX_ENUM)
		default: {
			return String("Swapchain format ") + String::num_int64(int64_t(p_swapchain_format));
		} break;
	}
}
