/**************************************************************************/
/*  rendering_context_driver_vulkan.cpp                                   */
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

#ifdef VULKAN_ENABLED

#include "rendering_context_driver_vulkan.h"

#include "vk_enum_string_helper.h"

#include "core/config/project_settings.h"
#include "core/version.h"

#include "rendering_device_driver_vulkan.h"
#include "vulkan_hooks.h"

#if defined(VK_TRACK_DRIVER_MEMORY)
/*************************************************/
// Driver memory tracking
/*************************************************/
// Total driver memory and allocation amount.
SafeNumeric<size_t> driver_memory_total_memory;
SafeNumeric<size_t> driver_memory_total_alloc_count;
// Amount of driver memory for every object type.
SafeNumeric<size_t> driver_memory_tracker[RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_COUNT][RenderingContextDriverVulkan::VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT];
// Amount of allocations for every object type.
SafeNumeric<uint32_t> driver_memory_allocation_count[RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_COUNT][RenderingContextDriverVulkan::VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT];
#endif

#if defined(VK_TRACK_DEVICE_MEMORY)
/*************************************************/
// Device memory report
/*************************************************/
// Total device memory and allocation amount.
HashMap<uint64_t, size_t> memory_report_table;
// Total memory and allocation amount.
SafeNumeric<uint64_t> memory_report_total_memory;
SafeNumeric<uint64_t> memory_report_total_alloc_count;
// Amount of device memory for every object type.
SafeNumeric<size_t> memory_report_mem_usage[RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_COUNT];
// Amount of device memory allocations for every object type.
SafeNumeric<size_t> memory_report_allocation_count[RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_COUNT];
#endif

const char *RenderingContextDriverVulkan::get_tracked_object_name(uint32_t p_type_index) const {
#if defined(VK_TRACK_DRIVER_MEMORY) || defined(VK_TRACK_DEVICE_MEMORY)
	static constexpr const char *vkTrackedObjectTypeNames[] = { "UNKNOWN",
		"INSTANCE",
		"PHYSICAL_DEVICE",
		"DEVICE",
		"QUEUE",
		"SEMAPHORE",
		"COMMAND_BUFFER",
		"FENCE",
		"DEVICE_MEMORY",
		"BUFFER",
		"IMAGE",
		"EVENT",
		"QUERY_POOL",
		"BUFFER_VIEW",
		"IMAGE_VIEW",
		"SHADER_MODULE",
		"PIPELINE_CACHE",
		"PIPELINE_LAYOUT",
		"RENDER_PASS",
		"PIPELINE",
		"DESCRIPTOR_SET_LAYOUT",
		"SAMPLER",
		"DESCRIPTOR_POOL",
		"DESCRIPTOR_SET",
		"FRAMEBUFFER",
		"COMMAND_POOL",
		"DESCRIPTOR_UPDATE_TEMPLATE_KHR",
		"SURFACE_KHR",
		"SWAPCHAIN_KHR",
		"DEBUG_UTILS_MESSENGER_EXT",
		"DEBUG_REPORT_CALLBACK_EXT",
		"ACCELERATION_STRUCTURE",
		"VMA_BUFFER_OR_IMAGE" };

	return vkTrackedObjectTypeNames[p_type_index];
#else
	return "VK_TRACK_*_MEMORY disabled at build time";
#endif
}

#if defined(VK_TRACK_DRIVER_MEMORY) || defined(VK_TRACK_DEVICE_MEMORY)
uint64_t RenderingContextDriverVulkan::get_tracked_object_type_count() const {
	return VK_TRACKED_OBJECT_TYPE_COUNT;
}
#endif

#if defined(VK_TRACK_DRIVER_MEMORY) || defined(VK_TRACK_DEVICE_MEMORY)
RenderingContextDriverVulkan::VkTrackedObjectType vk_object_to_tracked_object(VkObjectType p_type) {
	if (p_type > VK_OBJECT_TYPE_COMMAND_POOL && p_type != (VkObjectType)RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_VMA) {
		switch (p_type) {
			case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
				return RenderingContextDriverVulkan::VK_TRACKED_OBJECT_DESCRIPTOR_UPDATE_TEMPLATE_KHR;
			case VK_OBJECT_TYPE_SURFACE_KHR:
				return RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_SURFACE;
			case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
				return RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_SWAPCHAIN;
			case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
				return RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;
			case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
				return RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT;
			case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
			case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
				return RenderingContextDriverVulkan::VK_TRACKED_OBJECT_TYPE_ACCELERATION_STRUCTURE;
			default:
				_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Unknown VkObjectType enum value " + itos((uint32_t)p_type) + ".Please add it to VkTrackedObjectType, switch statement in "
																																 "vk_object_to_tracked_object and get_tracked_object_name.",
						(int)p_type);
				return (RenderingContextDriverVulkan::VkTrackedObjectType)VK_OBJECT_TYPE_UNKNOWN;
		}
	}

	return (RenderingContextDriverVulkan::VkTrackedObjectType)p_type;
}
#endif

#if defined(VK_TRACK_DEVICE_MEMORY)
uint64_t RenderingContextDriverVulkan::get_device_total_memory() const {
	return memory_report_total_memory.get();
}

uint64_t RenderingContextDriverVulkan::get_device_allocation_count() const {
	return memory_report_total_alloc_count.get();
}

uint64_t RenderingContextDriverVulkan::get_device_memory_by_object_type(uint32_t p_type) const {
	return memory_report_mem_usage[p_type].get();
}

uint64_t RenderingContextDriverVulkan::get_device_allocs_by_object_type(uint32_t p_type) const {
	return memory_report_allocation_count[p_type].get();
}
#endif

#if defined(VK_TRACK_DRIVER_MEMORY)
uint64_t RenderingContextDriverVulkan::get_driver_total_memory() const {
	return driver_memory_total_memory.get();
}

uint64_t RenderingContextDriverVulkan::get_driver_allocation_count() const {
	return driver_memory_total_alloc_count.get();
}

uint64_t RenderingContextDriverVulkan::get_driver_memory_by_object_type(uint32_t p_type) const {
	uint64_t ret = 0;
	for (uint32_t i = 0; i < VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT; i++) {
		ret += driver_memory_tracker[p_type][i].get();
	}

	return ret;
}

uint64_t RenderingContextDriverVulkan::get_driver_allocs_by_object_type(uint32_t p_type) const {
	uint64_t ret = 0;
	for (uint32_t i = 0; i < VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT; i++) {
		ret += driver_memory_allocation_count[p_type][i].get();
	}

	return ret;
}
#endif

#if defined(VK_TRACK_DEVICE_MEMORY)
void RenderingContextDriverVulkan::memory_report_callback(const VkDeviceMemoryReportCallbackDataEXT *p_callback_data, void *p_user_data) {
	if (!p_callback_data) {
		return;
	}
	const RenderingContextDriverVulkan::VkTrackedObjectType obj_type = vk_object_to_tracked_object(p_callback_data->objectType);
	uint64_t obj_id = p_callback_data->memoryObjectId;

	if (p_callback_data->type == VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_ALLOCATE_EXT) {
		// Realloc, update size
		if (memory_report_table.has(obj_id)) {
			memory_report_total_memory.sub(memory_report_table[obj_id]);
			memory_report_mem_usage[obj_type].sub(memory_report_table[obj_id]);

			memory_report_total_memory.add(p_callback_data->size);
			memory_report_mem_usage[obj_type].add(p_callback_data->size);

			memory_report_table[p_callback_data->memoryObjectId] = p_callback_data->size;
		} else {
			memory_report_table[obj_id] = p_callback_data->size;

			memory_report_total_alloc_count.increment();
			memory_report_allocation_count[obj_type].increment();
			memory_report_mem_usage[obj_type].add(p_callback_data->size);
			memory_report_total_memory.add(p_callback_data->size);
		}
	} else if (p_callback_data->type == VK_DEVICE_MEMORY_REPORT_EVENT_TYPE_FREE_EXT) {
		if (memory_report_table.has(obj_id)) {
			memory_report_total_alloc_count.decrement();
			memory_report_allocation_count[obj_type].decrement();
			memory_report_mem_usage[obj_type].sub(p_callback_data->size);
			memory_report_total_memory.sub(p_callback_data->size);

			memory_report_table.remove(memory_report_table.find(obj_id));
		}
	}
}
#endif

VkAllocationCallbacks *RenderingContextDriverVulkan::get_allocation_callbacks(VkObjectType p_type) {
#if !defined(VK_TRACK_DRIVER_MEMORY)
	return nullptr;
#else
	if (!Engine::get_singleton()->is_extra_gpu_memory_tracking_enabled()) {
		return nullptr;
	}

#ifdef _MSC_VER
#define LAMBDA_VK_CALL_CONV
#else
#define LAMBDA_VK_CALL_CONV VKAPI_PTR
#endif

	struct TrackedMemHeader {
		size_t size;
		VkSystemAllocationScope allocation_scope;
		VkTrackedObjectType type;
	};
	VkAllocationCallbacks tracking_callbacks = {
		// Allocation function
		nullptr,
		[](
				void *p_user_data,
				size_t size,
				size_t alignment,
				VkSystemAllocationScope allocation_scope) LAMBDA_VK_CALL_CONV -> void * {
			static constexpr size_t tracking_data_size = 32;
			VkTrackedObjectType type = static_cast<VkTrackedObjectType>(*reinterpret_cast<VkTrackedObjectType *>(p_user_data));

			driver_memory_total_memory.add(size);
			driver_memory_total_alloc_count.increment();
			driver_memory_tracker[type][allocation_scope].add(size);
			driver_memory_allocation_count[type][allocation_scope].increment();

			alignment = MAX(alignment, tracking_data_size);

			uint8_t *ret = reinterpret_cast<uint8_t *>(Memory::alloc_aligned_static(size + alignment, alignment));
			if (ret == nullptr) {
				return nullptr;
			}

			// Track allocation
			TrackedMemHeader *header = reinterpret_cast<TrackedMemHeader *>(ret);
			header->size = size;
			header->allocation_scope = allocation_scope;
			header->type = type;
			*reinterpret_cast<size_t *>(ret + alignment - sizeof(size_t)) = alignment;

			// Return first available chunk of memory
			return ret + alignment;
		},

		// Reallocation function
		[](
				void *p_user_data,
				void *p_original,
				size_t size,
				size_t alignment,
				VkSystemAllocationScope allocation_scope) LAMBDA_VK_CALL_CONV -> void * {
			if (p_original == nullptr) {
				VkObjectType type = static_cast<VkObjectType>(*reinterpret_cast<uint32_t *>(p_user_data));
				return get_allocation_callbacks(type)->pfnAllocation(p_user_data, size, alignment, allocation_scope);
			}

			uint8_t *mem = reinterpret_cast<uint8_t *>(p_original);
			// Retrieve alignment
			alignment = *reinterpret_cast<size_t *>(mem - sizeof(size_t));
			// Retrieve allocation data
			TrackedMemHeader *header = reinterpret_cast<TrackedMemHeader *>(mem - alignment);

			// Update allocation size
			driver_memory_total_memory.sub(header->size);
			driver_memory_total_memory.add(size);
			driver_memory_tracker[header->type][header->allocation_scope].sub(header->size);
			driver_memory_tracker[header->type][header->allocation_scope].add(size);

			uint8_t *ret = reinterpret_cast<uint8_t *>(Memory::realloc_aligned_static(header, size + alignment, header->size + alignment, alignment));
			if (ret == nullptr) {
				return nullptr;
			}
			// Update tracker
			header = reinterpret_cast<TrackedMemHeader *>(ret);
			header->size = size;
			return ret + alignment;
		},

		// Free function
		[](
				void *p_user_data,
				void *p_memory) LAMBDA_VK_CALL_CONV {
			if (!p_memory) {
				return;
			}

			uint8_t *mem = reinterpret_cast<uint8_t *>(p_memory);
			size_t alignment = *reinterpret_cast<size_t *>(mem - sizeof(size_t));
			TrackedMemHeader *header = reinterpret_cast<TrackedMemHeader *>(mem - alignment);

			driver_memory_total_alloc_count.decrement();
			driver_memory_total_memory.sub(header->size);
			driver_memory_tracker[header->type][header->allocation_scope].sub(header->size);
			driver_memory_allocation_count[header->type][header->allocation_scope].decrement();

			Memory::free_aligned_static(header);
		},
		// Internal allocation / deallocation. We don't track them as they cannot really be controlled or optimized by the programmer.
		[](
				void *p_user_data,
				size_t size,
				VkInternalAllocationType allocation_type,
				VkSystemAllocationScope allocation_scope) LAMBDA_VK_CALL_CONV {
		},
		[](
				void *p_user_data,
				size_t size,
				VkInternalAllocationType allocation_type,
				VkSystemAllocationScope allocation_scope) LAMBDA_VK_CALL_CONV {
		},
	};

	// Create a callback per object type
	static VkAllocationCallbacks object_callbacks[VK_TRACKED_OBJECT_TYPE_COUNT] = {};
	static uint32_t object_user_data[VK_TRACKED_OBJECT_TYPE_COUNT] = {};

	// Only build the first time
	if (!object_callbacks[0].pfnAllocation) {
		for (uint32_t c = 0; c < VK_TRACKED_OBJECT_TYPE_COUNT; ++c) {
			object_callbacks[c] = tracking_callbacks;
			object_user_data[c] = c;
			object_callbacks[c].pUserData = &object_user_data[c];

			for (uint32_t i = 0; i < VK_TRACKED_SYSTEM_ALLOCATION_SCOPE_COUNT; i++) {
				driver_memory_tracker[c][i].set(0);
				driver_memory_allocation_count[c][i].set(0);
			}
		}
	}

	uint32_t type_index = vk_object_to_tracked_object(p_type);
	return &object_callbacks[type_index];
#endif
}

RenderingContextDriverVulkan::RenderingContextDriverVulkan() {
	// Empty constructor.
}

RenderingContextDriverVulkan::~RenderingContextDriverVulkan() {
	if (debug_messenger != VK_NULL_HANDLE && functions.DestroyDebugUtilsMessengerEXT != nullptr) {
		functions.DestroyDebugUtilsMessengerEXT(instance, debug_messenger, get_allocation_callbacks(VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT));
	}

	if (debug_report != VK_NULL_HANDLE && functions.DestroyDebugReportCallbackEXT != nullptr) {
		functions.DestroyDebugReportCallbackEXT(instance, debug_report, get_allocation_callbacks(VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT));
	}

	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, get_allocation_callbacks(VK_OBJECT_TYPE_INSTANCE));
	}
}

Error RenderingContextDriverVulkan::_initialize_vulkan_version() {
	// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkApplicationInfo.html#_description
	// For Vulkan 1.0 vkEnumerateInstanceVersion is not available, including not in the loader we compile against on Android.
	typedef VkResult(VKAPI_PTR * _vkEnumerateInstanceVersion)(uint32_t *);
	_vkEnumerateInstanceVersion func = (_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
	if (func != nullptr) {
		uint32_t api_version;
		VkResult res = func(&api_version);
		if (res == VK_SUCCESS) {
			instance_api_version = api_version;
		} else {
			// According to the documentation this shouldn't fail with anything except a memory allocation error
			// in which case we're in deep trouble anyway.
			ERR_FAIL_V(ERR_CANT_CREATE);
		}
	} else {
		print_line("vkEnumerateInstanceVersion not available, assuming Vulkan 1.0.");
		instance_api_version = VK_API_VERSION_1_0;
	}

	return OK;
}

void RenderingContextDriverVulkan::_register_requested_instance_extension(const CharString &p_extension_name, bool p_required) {
	ERR_FAIL_COND(requested_instance_extensions.has(p_extension_name));
	requested_instance_extensions[p_extension_name] = p_required;
}

Error RenderingContextDriverVulkan::_initialize_instance_extensions() {
	enabled_instance_extension_names.clear();

	// The surface extension and the platform-specific surface extension are core requirements.
	_register_requested_instance_extension(VK_KHR_SURFACE_EXTENSION_NAME, true);
	if (_get_platform_surface_extension()) {
		_register_requested_instance_extension(_get_platform_surface_extension(), true);
	}

	if (_use_validation_layers()) {
		_register_requested_instance_extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, false);
	}

	// This extension allows us to use the properties2 features to query additional device capabilities.
	_register_requested_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, false);

#if defined(USE_VOLK) && (defined(MACOS_ENABLED) || defined(IOS_ENABLED))
	_register_requested_instance_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, true);
#endif

	// Only enable debug utils in verbose mode or DEV_ENABLED.
	// End users would get spammed with messages of varying verbosity due to the
	// mess that thirdparty layers/extensions and drivers seem to leave in their
	// wake, making the Windows registry a bottomless pit of broken layer JSON.
#ifdef DEV_ENABLED
	bool want_debug_utils = true;
#else
	bool want_debug_utils = OS::get_singleton()->is_stdout_verbose();
#endif
	if (want_debug_utils) {
		_register_requested_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false);
	}

	// Load instance extensions that are available.
	uint32_t instance_extension_count = 0;
	VkResult err = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, nullptr);
	ERR_FAIL_COND_V(err != VK_SUCCESS && err != VK_INCOMPLETE, ERR_CANT_CREATE);
	ERR_FAIL_COND_V_MSG(instance_extension_count == 0, ERR_CANT_CREATE, "No instance extensions were found.");

	TightLocalVector<VkExtensionProperties> instance_extensions;
	instance_extensions.resize(instance_extension_count);
	err = vkEnumerateInstanceExtensionProperties(nullptr, &instance_extension_count, instance_extensions.ptr());
	if (err != VK_SUCCESS && err != VK_INCOMPLETE) {
		ERR_FAIL_V(ERR_CANT_CREATE);
	}

#ifdef DEV_ENABLED
	for (uint32_t i = 0; i < instance_extension_count; i++) {
		print_verbose(String("VULKAN: Found instance extension ") + String::utf8(instance_extensions[i].extensionName) + String("."));
	}
#endif

	// Enable all extensions that are supported and requested.
	for (uint32_t i = 0; i < instance_extension_count; i++) {
		CharString extension_name(instance_extensions[i].extensionName);
		if (requested_instance_extensions.has(extension_name)) {
			enabled_instance_extension_names.insert(extension_name);
		}
	}

	// Now check our requested extensions.
	for (KeyValue<CharString, bool> &requested_extension : requested_instance_extensions) {
		if (!enabled_instance_extension_names.has(requested_extension.key)) {
			if (requested_extension.value) {
				ERR_FAIL_V_MSG(ERR_BUG, String("Required extension ") + String::utf8(requested_extension.key) + String(" not found."));
			} else {
				print_verbose(String("Optional extension ") + String::utf8(requested_extension.key) + String(" not found."));
			}
		}
	}

	return OK;
}

Error RenderingContextDriverVulkan::_find_validation_layers(TightLocalVector<const char *> &r_layer_names) const {
	r_layer_names.clear();

	uint32_t instance_layer_count = 0;
	VkResult err = vkEnumerateInstanceLayerProperties(&instance_layer_count, nullptr);
	ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
	if (instance_layer_count > 0) {
		TightLocalVector<VkLayerProperties> layer_properties;
		layer_properties.resize(instance_layer_count);
		err = vkEnumerateInstanceLayerProperties(&instance_layer_count, layer_properties.ptr());
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);

		// Preferred set of validation layers.
		const std::initializer_list<const char *> preferred = { "VK_LAYER_KHRONOS_validation" };

		// Alternative (deprecated, removed in SDK 1.1.126.0) set of validation layers.
		const std::initializer_list<const char *> lunarg = { "VK_LAYER_LUNARG_standard_validation" };

		// Alternative (deprecated, removed in SDK 1.1.121.1) set of validation layers.
		const std::initializer_list<const char *> google = { "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation", "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation", "VK_LAYER_GOOGLE_unique_objects" };

		// Verify all the layers of the list are present.
		for (const std::initializer_list<const char *> &list : { preferred, lunarg, google }) {
			bool layers_found = false;
			for (const char *layer_name : list) {
				layers_found = false;

				for (const VkLayerProperties &properties : layer_properties) {
					if (!strcmp(properties.layerName, layer_name)) {
						layers_found = true;
						break;
					}
				}

				if (!layers_found) {
					break;
				}
			}

			if (layers_found) {
				r_layer_names.reserve(list.size());
				for (const char *layer_name : list) {
					r_layer_names.push_back(layer_name);
				}

				break;
			}
		}
	}

	return OK;
}

VKAPI_ATTR VkBool32 VKAPI_CALL RenderingContextDriverVulkan::_debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT p_message_severity, VkDebugUtilsMessageTypeFlagsEXT p_message_type, const VkDebugUtilsMessengerCallbackDataEXT *p_callback_data, void *p_user_data) {
	// This error needs to be ignored because the AMD allocator will mix up memory types on IGP processors.
	if (strstr(p_callback_data->pMessage, "Mapping an image with layout") != nullptr && strstr(p_callback_data->pMessage, "can result in undefined behavior if this memory is used by the device") != nullptr) {
		return VK_FALSE;
	}
	// This needs to be ignored because Validator is wrong here.
	if (strstr(p_callback_data->pMessage, "Invalid SPIR-V binary version 1.3") != nullptr) {
		return VK_FALSE;
	}
	// This needs to be ignored because Validator is wrong here.
	if (strstr(p_callback_data->pMessage, "Shader requires flag") != nullptr) {
		return VK_FALSE;
	}

	// This needs to be ignored because Validator is wrong here.
	if (strstr(p_callback_data->pMessage, "SPIR-V module not valid: Pointer operand") != nullptr && strstr(p_callback_data->pMessage, "must be a memory object") != nullptr) {
		return VK_FALSE;
	}

	if (p_callback_data->pMessageIdName && strstr(p_callback_data->pMessageIdName, "UNASSIGNED-CoreValidation-DrawState-ClearCmdBeforeDraw") != nullptr) {
		return VK_FALSE;
	}

	String type_string;
	switch (p_message_type) {
		case (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT):
			type_string = "GENERAL";
			break;
		case (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT):
			type_string = "VALIDATION";
			break;
		case (VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT):
			type_string = "PERFORMANCE";
			break;
		case (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT):
			type_string = "VALIDATION|PERFORMANCE";
			break;
	}

	String objects_string;
	if (p_callback_data->objectCount > 0) {
		objects_string = "\n\tObjects - " + String::num_int64(p_callback_data->objectCount);
		for (uint32_t object = 0; object < p_callback_data->objectCount; ++object) {
			objects_string +=
					"\n\t\tObject[" + String::num_int64(object) + "]" +
					" - " + string_VkObjectType(p_callback_data->pObjects[object].objectType) +
					", Handle " + String::num_int64(p_callback_data->pObjects[object].objectHandle);

			if (p_callback_data->pObjects[object].pObjectName != nullptr && strlen(p_callback_data->pObjects[object].pObjectName) > 0) {
				objects_string += ", Name \"" + String(p_callback_data->pObjects[object].pObjectName) + "\"";
			}
		}
	}

	String labels_string;
	if (p_callback_data->cmdBufLabelCount > 0) {
		labels_string = "\n\tCommand Buffer Labels - " + String::num_int64(p_callback_data->cmdBufLabelCount);
		for (uint32_t cmd_buf_label = 0; cmd_buf_label < p_callback_data->cmdBufLabelCount; ++cmd_buf_label) {
			labels_string +=
					"\n\t\tLabel[" + String::num_int64(cmd_buf_label) + "]" +
					" - " + p_callback_data->pCmdBufLabels[cmd_buf_label].pLabelName +
					"{ ";

			for (int color_idx = 0; color_idx < 4; ++color_idx) {
				labels_string += String::num(p_callback_data->pCmdBufLabels[cmd_buf_label].color[color_idx]);
				if (color_idx < 3) {
					labels_string += ", ";
				}
			}

			labels_string += " }";
		}
	}

	String error_message(type_string +
			" - Message Id Number: " + String::num_int64(p_callback_data->messageIdNumber) +
			" | Message Id Name: " + p_callback_data->pMessageIdName +
			"\n\t" + p_callback_data->pMessage +
			objects_string + labels_string);

	// Convert VK severity to our own log macros.
	switch (p_message_severity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			print_verbose(error_message);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			print_line(error_message);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			WARN_PRINT(error_message);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			ERR_PRINT(error_message);
			CRASH_COND_MSG(Engine::get_singleton()->is_abort_on_gpu_errors_enabled(), "Crashing, because abort on GPU errors is enabled.");
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
			break; // Shouldn't happen, only handling to make compilers happy.
	}

	return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL RenderingContextDriverVulkan::_debug_report_callback(VkDebugReportFlagsEXT p_flags, VkDebugReportObjectTypeEXT p_object_type, uint64_t p_object, size_t p_location, int32_t p_message_code, const char *p_layer_prefix, const char *p_message, void *p_user_data) {
	String debug_message = String("Vulkan Debug Report: object - ") + String::num_int64(p_object) + "\n" + p_message;

	switch (p_flags) {
		case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
		case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
			print_line(debug_message);
			break;
		case VK_DEBUG_REPORT_WARNING_BIT_EXT:
		case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
			WARN_PRINT(debug_message);
			break;
		case VK_DEBUG_REPORT_ERROR_BIT_EXT:
			ERR_PRINT(debug_message);
			break;
	}

	return VK_FALSE;
}

Error RenderingContextDriverVulkan::_initialize_instance() {
	Error err;
	TightLocalVector<const char *> enabled_extension_names;
	enabled_extension_names.reserve(enabled_instance_extension_names.size());
	for (const CharString &extension_name : enabled_instance_extension_names) {
		enabled_extension_names.push_back(extension_name.ptr());
	}

	// We'll set application version to the Vulkan version we're developing against, even if our instance is based on an older Vulkan
	// version, devices can still support newer versions of Vulkan. The exception is when we're on Vulkan 1.0, we should not set this
	// to anything but 1.0. Note that this value is only used by validation layers to warn us about version issues.
	uint32_t application_api_version = instance_api_version == VK_API_VERSION_1_0 ? VK_API_VERSION_1_0 : VK_API_VERSION_1_2;

	CharString cs = GLOBAL_GET("application/config/name").operator String().utf8();
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = cs.get_data();
	app_info.pEngineName = VERSION_NAME;
	app_info.engineVersion = VK_MAKE_VERSION(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	app_info.apiVersion = application_api_version;

	TightLocalVector<const char *> enabled_layer_names;
	if (_use_validation_layers()) {
		err = _find_validation_layers(enabled_layer_names);
		ERR_FAIL_COND_V(err != OK, err);
	}

	VkInstanceCreateInfo instance_info = {};
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

#if defined(USE_VOLK) && (defined(MACOS_ENABLED) || defined(IOS_ENABLED))
	instance_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

	instance_info.pApplicationInfo = &app_info;
	instance_info.enabledExtensionCount = enabled_extension_names.size();
	instance_info.ppEnabledExtensionNames = enabled_extension_names.ptr();
	instance_info.enabledLayerCount = enabled_layer_names.size();
	instance_info.ppEnabledLayerNames = enabled_layer_names.ptr();

	// This is info for a temp callback to use during CreateInstance. After the instance is created, we use the instance-based function to register the final callback.
	VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};
	VkDebugReportCallbackCreateInfoEXT debug_report_callback_create_info = {};
	const bool has_debug_utils_extension = enabled_instance_extension_names.has(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	const bool has_debug_report_extension = enabled_instance_extension_names.has(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	if (has_debug_utils_extension) {
		debug_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debug_messenger_create_info.pNext = nullptr;
		debug_messenger_create_info.flags = 0;
		debug_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debug_messenger_create_info.pfnUserCallback = _debug_messenger_callback;
		debug_messenger_create_info.pUserData = this;
		instance_info.pNext = &debug_messenger_create_info;
	} else if (has_debug_report_extension) {
		debug_report_callback_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debug_report_callback_create_info.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
		debug_report_callback_create_info.pfnCallback = _debug_report_callback;
		debug_report_callback_create_info.pUserData = this;
		instance_info.pNext = &debug_report_callback_create_info;
	}

	err = _create_vulkan_instance(&instance_info, &instance);
	ERR_FAIL_COND_V(err != OK, err);

#ifdef USE_VOLK
	volkLoadInstance(instance);
#endif

	// Physical device.
	if (enabled_instance_extension_names.has(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
		functions.GetPhysicalDeviceFeatures2 = PFN_vkGetPhysicalDeviceFeatures2(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2"));
		functions.GetPhysicalDeviceProperties2 = PFN_vkGetPhysicalDeviceProperties2(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2"));

		// In Vulkan 1.0, the functions might be accessible under their original extension names.
		if (functions.GetPhysicalDeviceFeatures2 == nullptr) {
			functions.GetPhysicalDeviceFeatures2 = PFN_vkGetPhysicalDeviceFeatures2(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
		}

		if (functions.GetPhysicalDeviceProperties2 == nullptr) {
			functions.GetPhysicalDeviceProperties2 = PFN_vkGetPhysicalDeviceProperties2(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));
		}
	}

	// Device.
	functions.GetDeviceProcAddr = PFN_vkGetDeviceProcAddr(vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));

	// Surfaces.
	functions.GetPhysicalDeviceSurfaceSupportKHR = PFN_vkGetPhysicalDeviceSurfaceSupportKHR(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
	functions.GetPhysicalDeviceSurfaceFormatsKHR = PFN_vkGetPhysicalDeviceSurfaceFormatsKHR(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
	functions.GetPhysicalDeviceSurfaceCapabilitiesKHR = PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
	functions.GetPhysicalDeviceSurfacePresentModesKHR = PFN_vkGetPhysicalDeviceSurfacePresentModesKHR(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));

	// Debug utils and report.
	if (has_debug_utils_extension) {
		// Setup VK_EXT_debug_utils function pointers always (we use them for debug labels and names).
		functions.CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		functions.DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		functions.CmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT");
		functions.CmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT");
		functions.SetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");

		if (!functions.debug_util_functions_available()) {
			ERR_FAIL_V_MSG(ERR_CANT_CREATE, "GetProcAddr: Failed to init VK_EXT_debug_utils\nGetProcAddr: Failure");
		}

		VkResult res = functions.CreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, get_allocation_callbacks(VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT), &debug_messenger);
		switch (res) {
			case VK_SUCCESS:
				break;
			case VK_ERROR_OUT_OF_HOST_MEMORY:
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CreateDebugUtilsMessengerEXT: out of host memory\nCreateDebugUtilsMessengerEXT Failure");
				break;
			default:
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CreateDebugUtilsMessengerEXT: unknown failure\nCreateDebugUtilsMessengerEXT Failure");
				break;
		}
	} else if (has_debug_report_extension) {
		functions.CreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
		functions.DebugReportMessageEXT = (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT");
		functions.DestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");

		if (!functions.debug_report_functions_available()) {
			ERR_FAIL_V_MSG(ERR_CANT_CREATE, "GetProcAddr: Failed to init VK_EXT_debug_report\nGetProcAddr: Failure");
		}

		VkResult res = functions.CreateDebugReportCallbackEXT(instance, &debug_report_callback_create_info, get_allocation_callbacks(VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT), &debug_report);
		switch (res) {
			case VK_SUCCESS:
				break;
			case VK_ERROR_OUT_OF_HOST_MEMORY:
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CreateDebugReportCallbackEXT: out of host memory\nCreateDebugReportCallbackEXT Failure");
				break;
			default:
				ERR_FAIL_V_MSG(ERR_CANT_CREATE, "CreateDebugReportCallbackEXT: unknown failure\nCreateDebugReportCallbackEXT Failure");
				break;
		}
	}

	return OK;
}

Error RenderingContextDriverVulkan::_initialize_devices() {
	if (VulkanHooks::get_singleton() != nullptr) {
		VkPhysicalDevice physical_device;
		bool device_retrieved = VulkanHooks::get_singleton()->get_physical_device(&physical_device);
		ERR_FAIL_COND_V(!device_retrieved, ERR_CANT_CREATE);

		// When a hook is active, pretend the device returned by the hook is the only device available.
		driver_devices.resize(1);
		physical_devices.resize(1);
		device_queue_families.resize(1);
		physical_devices[0] = physical_device;

	} else {
		uint32_t physical_device_count = 0;
		VkResult err = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
		ERR_FAIL_COND_V_MSG(physical_device_count == 0, ERR_CANT_CREATE, "vkEnumeratePhysicalDevices reported zero accessible devices.\n\nDo you have a compatible Vulkan installable client driver (ICD) installed?\nvkEnumeratePhysicalDevices Failure.");

		driver_devices.resize(physical_device_count);
		physical_devices.resize(physical_device_count);
		device_queue_families.resize(physical_device_count);
		err = vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.ptr());
		ERR_FAIL_COND_V(err != VK_SUCCESS, ERR_CANT_CREATE);
	}

	// Fill the list of driver devices with the properties from the physical devices.
	for (uint32_t i = 0; i < physical_devices.size(); i++) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(physical_devices[i], &props);

		Device &driver_device = driver_devices[i];
		driver_device.name = String::utf8(props.deviceName);
		driver_device.vendor = props.vendorID;
		driver_device.type = DeviceType(props.deviceType);
		driver_device.workarounds = Workarounds();

		_check_driver_workarounds(props, driver_device);

		uint32_t queue_family_properties_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_properties_count, nullptr);

		if (queue_family_properties_count > 0) {
			device_queue_families[i].properties.resize(queue_family_properties_count);
			vkGetPhysicalDeviceQueueFamilyProperties(physical_devices[i], &queue_family_properties_count, device_queue_families[i].properties.ptr());
		}
	}

	return OK;
}

void RenderingContextDriverVulkan::_check_driver_workarounds(const VkPhysicalDeviceProperties &p_device_properties, Device &r_device) {
	// Workaround for the Adreno 6XX family of devices.
	//
	// There's a known issue with the Vulkan driver in this family of devices where it'll crash if a dynamic state for drawing is
	// used in a command buffer before a dispatch call is issued. As both dynamic scissor and viewport are basic requirements for
	// the engine to not bake this state into the PSO, the only known way to fix this issue is to reset the command buffer entirely.
	//
	// As the render graph has no built in limitations of whether it'll issue compute work before anything needs to draw on the
	// frame, and there's no guarantee that compute work will never be dependent on rasterization in the future, this workaround
	// will end recording on the current command buffer any time a compute list is encountered after a draw list was executed.
	// A new command buffer will be created afterwards and the appropriate synchronization primitives will be inserted.
	//
	// Executing this workaround has the added cost of synchronization between all the command buffers that are created as well as
	// all the individual submissions. This performance hit is accepted for the sake of being able to support these devices without
	// limiting the design of the renderer.
	//
	// This bug was fixed in driver version 512.503.0, so we only enabled it on devices older than this.
	//
	r_device.workarounds.avoid_compute_after_draw =
			r_device.vendor == Vendor::VENDOR_QUALCOMM &&
			p_device_properties.deviceID >= 0x6000000 && // Adreno 6xx
			p_device_properties.driverVersion < VK_MAKE_VERSION(512, 503, 0) &&
			r_device.name.find("Turnip") < 0;
}

bool RenderingContextDriverVulkan::_use_validation_layers() const {
	return Engine::get_singleton()->is_validation_layers_enabled();
}

Error RenderingContextDriverVulkan::_create_vulkan_instance(const VkInstanceCreateInfo *p_create_info, VkInstance *r_instance) {
	if (VulkanHooks::get_singleton() != nullptr) {
		return VulkanHooks::get_singleton()->create_vulkan_instance(p_create_info, r_instance) ? OK : ERR_CANT_CREATE;
	} else {
		VkResult err = vkCreateInstance(p_create_info, get_allocation_callbacks(VK_OBJECT_TYPE_INSTANCE), r_instance);
		ERR_FAIL_COND_V_MSG(err == VK_ERROR_INCOMPATIBLE_DRIVER, ERR_CANT_CREATE,
				"Cannot find a compatible Vulkan installable client driver (ICD).\n\n"
				"vkCreateInstance Failure");
		ERR_FAIL_COND_V_MSG(err == VK_ERROR_EXTENSION_NOT_PRESENT, ERR_CANT_CREATE,
				"Cannot find a specified extension library.\n"
				"Make sure your layers path is set appropriately.\n"
				"vkCreateInstance Failure");
		ERR_FAIL_COND_V_MSG(err, ERR_CANT_CREATE,
				"vkCreateInstance failed.\n\n"
				"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
				"Please look at the Getting Started guide for additional information.\n"
				"vkCreateInstance Failure");
	}

	return OK;
}

Error RenderingContextDriverVulkan::initialize() {
	Error err;

#ifdef USE_VOLK
	if (volkInitialize() != VK_SUCCESS) {
		return FAILED;
	}
#endif

	err = _initialize_vulkan_version();
	ERR_FAIL_COND_V(err != OK, err);

	err = _initialize_instance_extensions();
	ERR_FAIL_COND_V(err != OK, err);

	err = _initialize_instance();
	ERR_FAIL_COND_V(err != OK, err);

	err = _initialize_devices();
	ERR_FAIL_COND_V(err != OK, err);

	return OK;
}

const RenderingContextDriver::Device &RenderingContextDriverVulkan::device_get(uint32_t p_device_index) const {
	DEV_ASSERT(p_device_index < driver_devices.size());
	return driver_devices[p_device_index];
}

uint32_t RenderingContextDriverVulkan::device_get_count() const {
	return driver_devices.size();
}

bool RenderingContextDriverVulkan::device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const {
	DEV_ASSERT(p_device_index < physical_devices.size());

	// Check if any of the queues supported by the device supports presenting to the window's surface.
	const VkPhysicalDevice physical_device = physical_devices[p_device_index];
	const DeviceQueueFamilies &queue_families = device_queue_families[p_device_index];
	for (uint32_t i = 0; i < queue_families.properties.size(); i++) {
		if ((queue_families.properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && queue_family_supports_present(physical_device, i, p_surface)) {
			return true;
		}
	}

	return false;
}

RenderingDeviceDriver *RenderingContextDriverVulkan::driver_create() {
	return memnew(RenderingDeviceDriverVulkan(this));
}

void RenderingContextDriverVulkan::driver_free(RenderingDeviceDriver *p_driver) {
	memdelete(p_driver);
}

RenderingContextDriver::SurfaceID RenderingContextDriverVulkan::surface_create(const void *p_platform_data) {
	DEV_ASSERT(false && "Surface creation should not be called on the platform-agnostic version of the driver.");
	return SurfaceID();
}

void RenderingContextDriverVulkan::surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) {
	Surface *surface = (Surface *)(p_surface);
	surface->width = p_width;
	surface->height = p_height;
	surface->needs_resize = true;
}

void RenderingContextDriverVulkan::surface_set_vsync_mode(SurfaceID p_surface, DisplayServer::VSyncMode p_vsync_mode) {
	Surface *surface = (Surface *)(p_surface);
	surface->vsync_mode = p_vsync_mode;
	surface->needs_resize = true;
}

DisplayServer::VSyncMode RenderingContextDriverVulkan::surface_get_vsync_mode(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->vsync_mode;
}

uint32_t RenderingContextDriverVulkan::surface_get_width(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->width;
}

uint32_t RenderingContextDriverVulkan::surface_get_height(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->height;
}

void RenderingContextDriverVulkan::surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) {
	Surface *surface = (Surface *)(p_surface);
	surface->needs_resize = p_needs_resize;
}

bool RenderingContextDriverVulkan::surface_get_needs_resize(SurfaceID p_surface) const {
	Surface *surface = (Surface *)(p_surface);
	return surface->needs_resize;
}

void RenderingContextDriverVulkan::surface_destroy(SurfaceID p_surface) {
	Surface *surface = (Surface *)(p_surface);
	vkDestroySurfaceKHR(instance, surface->vk_surface, get_allocation_callbacks(VK_OBJECT_TYPE_SURFACE_KHR));
	memdelete(surface);
}

bool RenderingContextDriverVulkan::is_debug_utils_enabled() const {
	return enabled_instance_extension_names.has(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}

VkInstance RenderingContextDriverVulkan::instance_get() const {
	return instance;
}

VkPhysicalDevice RenderingContextDriverVulkan::physical_device_get(uint32_t p_device_index) const {
	DEV_ASSERT(p_device_index < physical_devices.size());
	return physical_devices[p_device_index];
}

uint32_t RenderingContextDriverVulkan::queue_family_get_count(uint32_t p_device_index) const {
	DEV_ASSERT(p_device_index < physical_devices.size());
	return device_queue_families[p_device_index].properties.size();
}

VkQueueFamilyProperties RenderingContextDriverVulkan::queue_family_get(uint32_t p_device_index, uint32_t p_queue_family_index) const {
	DEV_ASSERT(p_device_index < physical_devices.size());
	DEV_ASSERT(p_queue_family_index < queue_family_get_count(p_device_index));
	return device_queue_families[p_device_index].properties[p_queue_family_index];
}

bool RenderingContextDriverVulkan::queue_family_supports_present(VkPhysicalDevice p_physical_device, uint32_t p_queue_family_index, SurfaceID p_surface) const {
	DEV_ASSERT(p_physical_device != VK_NULL_HANDLE);
	DEV_ASSERT(p_surface != 0);
	Surface *surface = (Surface *)(p_surface);
	VkBool32 present_supported = false;
	VkResult err = vkGetPhysicalDeviceSurfaceSupportKHR(p_physical_device, p_queue_family_index, surface->vk_surface, &present_supported);
	return err == VK_SUCCESS && present_supported;
}

const RenderingContextDriverVulkan::Functions &RenderingContextDriverVulkan::functions_get() const {
	return functions;
}

#endif // VULKAN_ENABLED
