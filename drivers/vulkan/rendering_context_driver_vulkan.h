/**************************************************************************/
/*  rendering_context_driver_vulkan.h                                     */
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

#pragma once

#ifdef VULKAN_ENABLED

#include "servers/rendering/rendering_context_driver.h"

#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
#define VK_TRACK_DRIVER_MEMORY
#define VK_TRACK_DEVICE_MEMORY
#endif

#include "drivers/vulkan/godot_vulkan.h"

class RenderingContextDriverVulkan : public RenderingContextDriver {
public:
	struct Functions {
		// Physical device.
		PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2 = nullptr;
		PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2 = nullptr;

		// Device.
		PFN_vkGetDeviceProcAddr GetDeviceProcAddr = nullptr;

		// Surfaces.
		PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR = nullptr;
		PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR = nullptr;
		PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
		PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR = nullptr;

		// Debug utils.
		PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT = nullptr;
		PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT = nullptr;
		PFN_vkCmdBeginDebugUtilsLabelEXT CmdBeginDebugUtilsLabelEXT = nullptr;
		PFN_vkCmdEndDebugUtilsLabelEXT CmdEndDebugUtilsLabelEXT = nullptr;
		PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT = nullptr;

		bool debug_util_functions_available() const {
			return CreateDebugUtilsMessengerEXT != nullptr &&
					DestroyDebugUtilsMessengerEXT != nullptr &&
					CmdBeginDebugUtilsLabelEXT != nullptr &&
					CmdEndDebugUtilsLabelEXT != nullptr &&
					SetDebugUtilsObjectNameEXT != nullptr;
		}

		// Debug report.
		PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallbackEXT = nullptr;
		PFN_vkDebugReportMessageEXT DebugReportMessageEXT = nullptr;
		PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallbackEXT = nullptr;

		// Debug marker extensions.
		PFN_vkCmdDebugMarkerBeginEXT CmdDebugMarkerBeginEXT = nullptr;
		PFN_vkCmdDebugMarkerEndEXT CmdDebugMarkerEndEXT = nullptr;
		PFN_vkCmdDebugMarkerInsertEXT CmdDebugMarkerInsertEXT = nullptr;
		PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectNameEXT = nullptr;

		bool debug_report_functions_available() const {
			return CreateDebugReportCallbackEXT != nullptr &&
					DebugReportMessageEXT != nullptr &&
					DestroyDebugReportCallbackEXT != nullptr;
		}
	};

private:
	struct DeviceQueueFamilies {
		TightLocalVector<VkQueueFamilyProperties> properties;
	};

	VkInstance instance = VK_NULL_HANDLE;
	uint32_t instance_api_version = VK_API_VERSION_1_0;
	HashMap<CharString, bool> requested_instance_extensions;
	HashSet<CharString> enabled_instance_extension_names;
	TightLocalVector<Device> driver_devices;
	TightLocalVector<VkPhysicalDevice> physical_devices;
	TightLocalVector<DeviceQueueFamilies> device_queue_families;
	VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
	VkDebugReportCallbackEXT debug_report = VK_NULL_HANDLE;
	Functions functions;

	Error _initialize_vulkan_version();
	void _register_requested_instance_extension(const CharString &p_extension_name, bool p_required);
	Error _initialize_instance_extensions();
	Error _initialize_instance();
	Error _initialize_devices();
	void _check_driver_workarounds(const VkPhysicalDeviceProperties &p_device_properties, Device &r_device);

	// Static callbacks.
	static VKAPI_ATTR VkBool32 VKAPI_CALL _debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT p_message_severity, VkDebugUtilsMessageTypeFlagsEXT p_message_type, const VkDebugUtilsMessengerCallbackDataEXT *p_callback_data, void *p_user_data);
	static VKAPI_ATTR VkBool32 VKAPI_CALL _debug_report_callback(VkDebugReportFlagsEXT p_flags, VkDebugReportObjectTypeEXT p_object_type, uint64_t p_object, size_t p_location, int32_t p_message_code, const char *p_layer_prefix, const char *p_message, void *p_user_data);
	// Debug marker extensions.
	VkDebugReportObjectTypeEXT _convert_to_debug_report_objectType(VkObjectType p_object_type);

protected:
	Error _find_validation_layers(TightLocalVector<const char *> &r_layer_names) const;

	// Can be overridden by platform-specific drivers.
	virtual const char *_get_platform_surface_extension() const { return nullptr; }
	virtual bool _use_validation_layers() const;
	virtual Error _create_vulkan_instance(const VkInstanceCreateInfo *p_create_info, VkInstance *r_instance);

public:
	virtual Error initialize() override;
	virtual const Device &device_get(uint32_t p_device_index) const override;
	virtual uint32_t device_get_count() const override;
	virtual bool device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const override;
	virtual RenderingDeviceDriver *driver_create() override;
	virtual void driver_free(RenderingDeviceDriver *p_driver) override;
	virtual SurfaceID surface_create(const void *p_platform_data) override;
	virtual void surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) override;
	virtual void surface_set_vsync_mode(SurfaceID p_surface, DisplayServer::VSyncMode p_vsync_mode) override;
	virtual DisplayServer::VSyncMode surface_get_vsync_mode(SurfaceID p_surface) const override;
	virtual uint32_t surface_get_width(SurfaceID p_surface) const override;
	virtual uint32_t surface_get_height(SurfaceID p_surface) const override;
	virtual void surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) override;
	virtual bool surface_get_needs_resize(SurfaceID p_surface) const override;
	virtual void surface_destroy(SurfaceID p_surface) override;
	virtual bool is_debug_utils_enabled() const override;

	// Vulkan-only methods.
	struct Surface {
		VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
		uint32_t width = 0;
		uint32_t height = 0;
		DisplayServer::VSyncMode vsync_mode = DisplayServer::VSYNC_ENABLED;
		bool needs_resize = false;
	};

	VkInstance instance_get() const;
	VkPhysicalDevice physical_device_get(uint32_t p_device_index) const;
	uint32_t queue_family_get_count(uint32_t p_device_index) const;
	VkQueueFamilyProperties queue_family_get(uint32_t p_device_index, uint32_t p_queue_family_index) const;
	bool queue_family_supports_present(VkPhysicalDevice p_physical_device, uint32_t p_queue_family_index, SurfaceID p_surface) const;
	const Functions &functions_get() const;

	static VkAllocationCallbacks *get_allocation_callbacks(VkObjectType p_type);

#if defined(VK_TRACK_DRIVER_MEMORY) || defined(VK_TRACK_DEVICE_MEMORY)
	enum VkTrackedObjectType {
		VK_TRACKED_OBJECT_DESCRIPTOR_UPDATE_TEMPLATE_KHR = VK_OBJECT_TYPE_COMMAND_POOL + 1,
		VK_TRACKED_OBJECT_TYPE_SURFACE,
		VK_TRACKED_OBJECT_TYPE_SWAPCHAIN,
		VK_TRACKED_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT,
		VK_TRACKED_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT,
		VK_TRACKED_OBJECT_TYPE_ACCELERATION_STRUCTURE,
		VK_TRACKED_OBJECT_TYPE_VMA,
		VK_TRACKED_OBJECT_TYPE_COUNT
	};

	enum VkTrackedSystemAllocationScope {
		VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT = VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE + 1
	};
#endif

	const char *get_tracked_object_name(uint32_t p_type_index) const override;
#if defined(VK_TRACK_DRIVER_MEMORY) || defined(VK_TRACK_DEVICE_MEMORY)
	uint64_t get_tracked_object_type_count() const override;
#endif

#if defined(VK_TRACK_DRIVER_MEMORY)
	uint64_t get_driver_total_memory() const override;
	uint64_t get_driver_allocation_count() const override;
	uint64_t get_driver_memory_by_object_type(uint32_t p_type) const override;
	uint64_t get_driver_allocs_by_object_type(uint32_t p_type) const override;
#endif

#if defined(VK_TRACK_DEVICE_MEMORY)
	uint64_t get_device_total_memory() const override;
	uint64_t get_device_allocation_count() const override;
	uint64_t get_device_memory_by_object_type(uint32_t p_type) const override;
	uint64_t get_device_allocs_by_object_type(uint32_t p_type) const override;
	static VKAPI_ATTR void VKAPI_CALL memory_report_callback(const VkDeviceMemoryReportCallbackDataEXT *p_callback_data, void *p_user_data);
#endif

	RenderingContextDriverVulkan();
	virtual ~RenderingContextDriverVulkan() override;
};

#endif // VULKAN_ENABLED
