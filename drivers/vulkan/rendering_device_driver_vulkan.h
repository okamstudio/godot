/**************************************************************************/
/*  rendering_device_driver_vulkan.h                                      */
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

#ifndef RENDERING_DEVICE_DRIVER_VULKAN_H
#define RENDERING_DEVICE_DRIVER_VULKAN_H

#include "core/templates/hash_map.h"
#include "core/templates/paged_allocator.h"
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#include "servers/rendering/rendering_device_driver.h"

#ifdef DEBUG_ENABLED
#ifndef _MSC_VER
#define _DEBUG
#endif
#endif
#include "thirdparty/vulkan/vk_mem_alloc.h"

#include "drivers/vulkan/godot_vulkan.h"

// Design principles:
// - Vulkan structs are zero-initialized and fields not requiring a non-zero value are omitted (except in cases where expresivity reasons apply).
class RenderingDeviceDriverVulkan : public RenderingDeviceDriver {
	/*****************/
	/**** GENERIC ****/
	/*****************/

	struct CommandQueue;
	struct SwapChain;

	struct Queue {
		VkQueue queue = VK_NULL_HANDLE;
		uint32_t virtual_count = 0;
		BinaryMutex submit_mutex;
	};

	struct SubgroupCapabilities {
		uint32_t size = 0;
		uint32_t min_size = 0;
		uint32_t max_size = 0;
		VkShaderStageFlags supported_stages = 0;
		VkSubgroupFeatureFlags supported_operations = 0;
		VkBool32 quad_operations_in_all_stages = false;
		bool size_control_is_supported = false;

		uint32_t supported_stages_flags_rd() const;
		String supported_stages_desc() const;
		uint32_t supported_operations_flags_rd() const;
		String supported_operations_desc() const;
	};

	struct VRSCapabilities {
		bool pipeline_vrs_supported = false; // We can specify our fragment rate on a pipeline level.
		bool primitive_vrs_supported = false; // We can specify our fragment rate on each drawcall.
		bool attachment_vrs_supported = false; // We can provide a density map attachment on our framebuffer.

		Size2i min_texel_size;
		Size2i max_texel_size;
		Size2i max_fragment_size;

		Size2i texel_size; // The texel size we'll use
	};

	struct ShaderCapabilities {
		bool shader_float16_is_supported = false;
		bool shader_int8_is_supported = false;
	};

	struct StorageBufferCapabilities {
		bool storage_buffer_16_bit_access_is_supported = false;
		bool uniform_and_storage_buffer_16_bit_access_is_supported = false;
		bool storage_push_constant_16_is_supported = false;
		bool storage_input_output_16 = false;
	};

	struct DeviceFunctions {
		PFN_vkCreateSwapchainKHR CreateSwapchainKHR = nullptr;
		PFN_vkDestroySwapchainKHR DestroySwapchainKHR = nullptr;
		PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR = nullptr;
		PFN_vkAcquireNextImageKHR AcquireNextImageKHR = nullptr;
		PFN_vkQueuePresentKHR QueuePresentKHR = nullptr;
		PFN_vkCreateRenderPass2KHR CreateRenderPass2KHR = nullptr;

		// Debug marker extensions.
		PFN_vkCmdDebugMarkerBeginEXT CmdDebugMarkerBeginEXT = nullptr;
		PFN_vkCmdDebugMarkerEndEXT CmdDebugMarkerEndEXT = nullptr;
		PFN_vkCmdDebugMarkerInsertEXT CmdDebugMarkerInsertEXT = nullptr;
		PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectNameEXT = nullptr;

		// Debug device fault.
		PFN_vkGetDeviceFaultInfoEXT GetDeviceFaultInfoEXT = nullptr;
	};
	// Debug marker extensions.
	VkDebugReportObjectTypeEXT _convert_to_debug_report_objectType(VkObjectType p_object_type);

	VkDevice vk_device = VK_NULL_HANDLE;
	RenderingContextDriverVulkan *context_driver = nullptr;
	RenderingContextDriver::Device context_device = {};
	uint32_t frame_count = 1;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties physical_device_properties = {};
	VkPhysicalDeviceFeatures physical_device_features = {};
	VkPhysicalDeviceFeatures requested_device_features = {};
	HashMap<CharString, bool> requested_device_extensions;
	HashSet<CharString> enabled_device_extension_names;
	TightLocalVector<TightLocalVector<Queue>> queue_families;
	TightLocalVector<VkQueueFamilyProperties> queue_family_properties;
	RDD::Capabilities device_capabilities;
	SubgroupCapabilities subgroup_capabilities;
	MultiviewCapabilities multiview_capabilities;
	VRSCapabilities vrs_capabilities;
	ShaderCapabilities shader_capabilities;
	StorageBufferCapabilities storage_buffer_capabilities;
	bool pipeline_cache_control_support = false;
	bool device_fault_support = false;
#if defined(VK_TRACK_DEVICE_MEMORY)
	bool device_memory_report_support = false;
#endif
	DeviceFunctions device_functions;

	void _register_requested_device_extension(const CharString &p_extension_name, bool p_required);
	Error _initialize_device_extensions();
	Error _check_device_features();
	Error _check_device_capabilities();
	Error _add_queue_create_info(LocalVector<VkDeviceQueueCreateInfo> &r_queue_create_info);
	Error _initialize_device(const LocalVector<VkDeviceQueueCreateInfo> &p_queue_create_info);
	Error _initialize_allocator();
	Error _initialize_pipeline_cache();
	VkResult _create_render_pass(VkDevice p_device, const VkRenderPassCreateInfo2 *p_create_info, const VkAllocationCallbacks *p_allocator, VkRenderPass *p_render_pass);
	bool _release_image_semaphore(CommandQueue *p_command_queue, uint32_t p_semaphore_index, bool p_release_on_swap_chain);
	bool _recreate_image_semaphore(CommandQueue *p_command_queue, uint32_t p_semaphore_index, bool p_release_on_swap_chain);
	void _set_object_name(VkObjectType p_object_type, uint64_t p_object_handle, String p_object_name);

public:
	Error initialize(uint32_t p_device_index, uint32_t p_frame_count) override final;

private:
	/****************/
	/**** MEMORY ****/
	/****************/

	VmaAllocator allocator = nullptr;
	HashMap<uint32_t, VmaPool> small_allocs_pools;

	VmaPool _find_or_create_small_allocs_pool(uint32_t p_mem_type_index);

private:
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	// It's a circular buffer.
	BufferID breadcrumb_buffer;
	uint32_t breadcrumb_offset = 0u;
	uint32_t breadcrumb_id = 0u;
#endif

public:
	/*****************/
	/**** BUFFERS ****/
	/*****************/
	struct BufferInfo {
		VkBuffer vk_buffer = VK_NULL_HANDLE;
		struct {
			VmaAllocation handle = nullptr;
			uint64_t size = UINT64_MAX;
		} allocation;
		uint64_t size = 0;
		VkBufferView vk_view = VK_NULL_HANDLE; // For texel buffers.
	};

	virtual BufferID buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type) override final;
	virtual bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) override final;
	virtual void buffer_free(BufferID p_buffer) override final;
	virtual uint64_t buffer_get_allocation_size(BufferID p_buffer) override final;
	virtual uint8_t *buffer_map(BufferID p_buffer) override final;
	virtual void buffer_unmap(BufferID p_buffer) override final;

	/*****************/
	/**** TEXTURE ****/
	/*****************/

	struct TextureInfo {
		VkImage vk_image = VK_NULL_HANDLE;
		VkImageView vk_view = VK_NULL_HANDLE;
		DataFormat rd_format = DATA_FORMAT_MAX;
		VkImageCreateInfo vk_create_info = {};
		VkImageViewCreateInfo vk_view_create_info = {};
		struct {
			VmaAllocation handle = nullptr;
			VmaAllocationInfo info = {};
		} allocation; // All 0/null if just a view.
#ifdef DEBUG_ENABLED
		bool created_from_extension = false;
#endif
	};

	VkSampleCountFlagBits _ensure_supported_sample_count(TextureSamples p_requested_sample_count);

public:
	virtual TextureID texture_create(const TextureFormat &p_format, const TextureView &p_view) override final;
	virtual TextureID texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil) override final;
	virtual TextureID texture_create_shared(TextureID p_original_texture, const TextureView &p_view) override final;
	virtual TextureID texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) override final;
	virtual void texture_free(TextureID p_texture) override final;
	virtual uint64_t texture_get_allocation_size(TextureID p_texture) override final;
	virtual void texture_get_copyable_layout(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) override final;
	virtual uint8_t *texture_map(TextureID p_texture, const TextureSubresource &p_subresource) override final;
	virtual void texture_unmap(TextureID p_texture) override final;
	virtual BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) override final;
	virtual bool texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool &r_raw_reinterpretation) override final;

	/*****************/
	/**** SAMPLER ****/
	/*****************/
public:
	virtual SamplerID sampler_create(const SamplerState &p_state) final override;
	virtual void sampler_free(SamplerID p_sampler) final override;
	virtual bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) override final;

	/**********************/
	/**** VERTEX ARRAY ****/
	/**********************/
private:
	struct VertexFormatInfo {
		TightLocalVector<VkVertexInputBindingDescription> vk_bindings;
		TightLocalVector<VkVertexInputAttributeDescription> vk_attributes;
		VkPipelineVertexInputStateCreateInfo vk_create_info = {};
	};

public:
	virtual VertexFormatID vertex_format_create(VectorView<VertexAttribute> p_vertex_attribs) override final;
	virtual void vertex_format_free(VertexFormatID p_vertex_format) override final;

	/******************/
	/**** BARRIERS ****/
	/******************/

	virtual void command_pipeline_barrier(
			CommandBufferID p_cmd_buffer,
			BitField<PipelineStageBits> p_src_stages,
			BitField<PipelineStageBits> p_dst_stages,
			VectorView<MemoryBarrier> p_memory_barriers,
			VectorView<BufferBarrier> p_buffer_barriers,
			VectorView<TextureBarrier> p_texture_barriers) override final;

	/****************/
	/**** FENCES ****/
	/****************/

private:
	struct Fence {
		VkFence vk_fence = VK_NULL_HANDLE;
		CommandQueue *queue_signaled_from = nullptr;
	};

public:
	virtual FenceID fence_create() override final;
	virtual Error fence_wait(FenceID p_fence) override final;
	virtual void fence_free(FenceID p_fence) override final;

	/********************/
	/**** SEMAPHORES ****/
	/********************/

	virtual SemaphoreID semaphore_create() override final;
	virtual void semaphore_free(SemaphoreID p_semaphore) override final;

	/******************/
	/**** COMMANDS ****/
	/******************/

	// ----- QUEUE FAMILY -----

	virtual CommandQueueFamilyID command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface = 0) override final;

	// ----- QUEUE -----
private:
	struct CommandQueue {
		LocalVector<VkSemaphore> present_semaphores;
		LocalVector<VkSemaphore> image_semaphores;
		LocalVector<SwapChain *> image_semaphores_swap_chains;
		LocalVector<uint32_t> pending_semaphores_for_execute;
		LocalVector<uint32_t> pending_semaphores_for_fence;
		LocalVector<uint32_t> free_image_semaphores;
		LocalVector<Pair<Fence *, uint32_t>> image_semaphores_for_fences;
		uint32_t queue_family = 0;
		uint32_t queue_index = 0;
		uint32_t present_semaphore_index = 0;
	};

public:
	virtual CommandQueueID command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue = false) override final;
	virtual Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_semaphores, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) override final;
	virtual void command_queue_free(CommandQueueID p_cmd_queue) override final;

private:
	// ----- POOL -----

	struct CommandPool {
		VkCommandPool vk_command_pool = VK_NULL_HANDLE;
		CommandBufferType buffer_type = COMMAND_BUFFER_TYPE_PRIMARY;
	};

public:
	virtual CommandPoolID command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) override final;
	virtual void command_pool_free(CommandPoolID p_cmd_pool) override final;

	// ----- BUFFER -----

	virtual CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool) override final;
	virtual bool command_buffer_begin(CommandBufferID p_cmd_buffer) override final;
	virtual bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) override final;
	virtual void command_buffer_end(CommandBufferID p_cmd_buffer) override final;
	virtual void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) override final;

	/********************/
	/**** SWAP CHAIN ****/
	/********************/

private:
	struct SwapChain {
		VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
		RenderingContextDriver::SurfaceID surface = RenderingContextDriver::SurfaceID();
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		TightLocalVector<VkImage> images;
		TightLocalVector<VkImageView> image_views;
		TightLocalVector<FramebufferID> framebuffers;
		LocalVector<CommandQueue *> command_queues_acquired;
		LocalVector<uint32_t> command_queues_acquired_semaphores;
		RenderPassID render_pass;
		uint32_t image_index = 0;
	};

	void _swap_chain_release(SwapChain *p_swap_chain);

public:
	virtual SwapChainID swap_chain_create(RenderingContextDriver::SurfaceID p_surface) override final;
	virtual Error swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) override final;
	virtual FramebufferID swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) override final;
	virtual RenderPassID swap_chain_get_render_pass(SwapChainID p_swap_chain) override final;
	virtual DataFormat swap_chain_get_format(SwapChainID p_swap_chain) override final;
	virtual void swap_chain_free(SwapChainID p_swap_chain) override final;

	/*********************/
	/**** FRAMEBUFFER ****/
	/*********************/

	struct Framebuffer {
		VkFramebuffer vk_framebuffer = VK_NULL_HANDLE;

		// Only filled in by a framebuffer created by a swap chain. Unused otherwise.
		VkImage swap_chain_image = VK_NULL_HANDLE;
		VkImageSubresourceRange swap_chain_image_subresource_range = {};
		bool swap_chain_acquired = false;
	};

	virtual FramebufferID framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) override final;
	virtual void framebuffer_free(FramebufferID p_framebuffer) override final;

	/****************/
	/**** SHADER ****/
	/****************/
private:
	struct ShaderBinary {
		// Version 1: initial.
		// Version 2: Added shader name.
		// Version 3: Added writable.
		// Version 4: 64-bit vertex input mask.
		static const uint32_t VERSION = 4;

		struct DataBinding {
			uint32_t type = 0;
			uint32_t binding = 0;
			uint32_t stages = 0;
			uint32_t length = 0; // Size of arrays (in total elements), or UBOs (in bytes * total elements).
			uint32_t writable = 0;
		};

		struct SpecializationConstant {
			uint32_t type = 0;
			uint32_t constant_id = 0;
			uint32_t int_value = 0;
			uint32_t stage_flags = 0;
		};

		struct Data {
			uint64_t vertex_input_mask = 0;
			uint32_t fragment_output_mask = 0;
			uint32_t specialization_constants_count = 0;
			uint32_t is_compute = 0;
			uint32_t compute_local_size[3] = {};
			uint32_t set_count = 0;
			uint32_t push_constant_size = 0;
			uint32_t vk_push_constant_stages_mask = 0;
			uint32_t stage_count = 0;
			uint32_t shader_name_len = 0;
		};
	};

	struct ShaderInfo {
		VkShaderStageFlags vk_push_constant_stages = 0;
		TightLocalVector<VkPipelineShaderStageCreateInfo> vk_stages_create_info;
		TightLocalVector<VkDescriptorSetLayout> vk_descriptor_set_layouts;
		VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;
	};

public:
	virtual String shader_get_binary_cache_key() override final;
	virtual Vector<uint8_t> shader_compile_binary_from_spirv(VectorView<ShaderStageSPIRVData> p_spirv, const String &p_shader_name) override final;
	virtual ShaderID shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, ShaderDescription &r_shader_desc, String &r_name) override final;
	virtual void shader_free(ShaderID p_shader) override final;

	virtual void shader_destroy_modules(ShaderID p_shader) override final;
	/*********************/
	/**** UNIFORM SET ****/
	/*********************/

	// Descriptor sets require allocation from a pool.
	// The documentation on how to use pools properly
	// is scarce, and the documentation is strange.
	//
	// Basically, you can mix and match pools as you
	// like, but you'll run into fragmentation issues.
	// Because of this, the recommended approach is to
	// create a pool for every descriptor set type, as
	// this prevents fragmentation.
	//
	// This is implemented here as a having a list of
	// pools (each can contain up to 64 sets) for each
	// set layout. The amount of sets for each type
	// is used as the key.

private:
	static const uint32_t MAX_UNIFORM_POOL_ELEMENT = 65535;

	struct DescriptorSetPoolKey {
		uint16_t uniform_type[UNIFORM_TYPE_MAX] = {};

		bool operator<(const DescriptorSetPoolKey &p_other) const {
			return memcmp(uniform_type, p_other.uniform_type, sizeof(uniform_type)) < 0;
		}
	};

	using DescriptorSetPools = RBMap<DescriptorSetPoolKey, HashMap<VkDescriptorPool, uint32_t>>;
	DescriptorSetPools descriptor_set_pools;
	uint32_t max_descriptor_sets_per_pool = 0;

	VkDescriptorPool _descriptor_set_pool_find_or_create(const DescriptorSetPoolKey &p_key, DescriptorSetPools::Iterator *r_pool_sets_it);
	void _descriptor_set_pool_unreference(DescriptorSetPools::Iterator p_pool_sets_it, VkDescriptorPool p_vk_descriptor_pool);

	struct UniformSetInfo {
		VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
		VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
		DescriptorSetPools::Iterator pool_sets_it = {};
	};

public:
	virtual UniformSetID uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index) override final;
	virtual void uniform_set_free(UniformSetID p_uniform_set) override final;

	// ----- COMMANDS -----

	virtual void command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;

	/******************/
	/**** TRANSFER ****/
	/******************/

	virtual void command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) override final;
	virtual void command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) override final;

	virtual void command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) override final;
	virtual void command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) override final;
	virtual void command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) override final;

	virtual void command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) override final;
	virtual void command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) override final;

	/******************/
	/**** PIPELINE ****/
	/******************/
private:
	struct PipelineCacheHeader {
		uint32_t magic = 0;
		uint32_t data_size = 0;
		uint64_t data_hash = 0;
		uint32_t vendor_id = 0;
		uint32_t device_id = 0;
		uint32_t driver_version = 0;
		uint8_t uuid[VK_UUID_SIZE] = {};
		uint8_t driver_abi = 0;
	};

	struct PipelineCache {
		String file_path;
		size_t current_size = 0;
		Vector<uint8_t> buffer; // Header then data.
		VkPipelineCache vk_cache = VK_NULL_HANDLE;
	};

	static int caching_instance_count;
	PipelineCache pipelines_cache;
	String pipeline_cache_id;
	HashMap<uint64_t, bool> has_comp_alpha;

public:
	virtual void pipeline_free(PipelineID p_pipeline) override final;

	// ----- BINDING -----

	virtual void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_first_index, VectorView<uint32_t> p_data) override final;

	// ----- CACHE -----

	virtual bool pipeline_cache_create(const Vector<uint8_t> &p_data) override final;
	virtual void pipeline_cache_free() override final;
	virtual size_t pipeline_cache_query_size() override final;
	virtual Vector<uint8_t> pipeline_cache_serialize() override final;

	/*******************/
	/**** RENDERING ****/
	/*******************/

	// ----- SUBPASS -----

	virtual RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count) override final;
	virtual void render_pass_free(RenderPassID p_render_pass) override final;

	// ----- COMMANDS -----

	virtual void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) override final;
	virtual void command_end_render_pass(CommandBufferID p_cmd_buffer) override final;
	virtual void command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) override final;
	virtual void command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) override final;
	virtual void command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) override final;
	virtual void command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) override final;

	// Binding.
	virtual void command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override final;
	virtual void command_bind_render_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;

	// Drawing.
	virtual void command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) override final;
	virtual void command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) override final;
	virtual void command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) override final;
	virtual void command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override final;
	virtual void command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) override final;
	virtual void command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) override final;

	// Buffer binding.
	virtual void command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID *p_buffers, const uint64_t *p_offsets) override final;
	virtual void command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) override final;

	// Dynamic state.
	virtual void command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) override final;
	virtual void command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) override final;

	// ----- PIPELINE -----

	virtual PipelineID render_pipeline_create(
			ShaderID p_shader,
			VertexFormatID p_vertex_format,
			RenderPrimitive p_render_primitive,
			PipelineRasterizationState p_rasterization_state,
			PipelineMultisampleState p_multisample_state,
			PipelineDepthStencilState p_depth_stencil_state,
			PipelineColorBlendState p_blend_state,
			VectorView<int32_t> p_color_attachments,
			BitField<PipelineDynamicStateFlags> p_dynamic_state,
			RenderPassID p_render_pass,
			uint32_t p_render_subpass,
			VectorView<PipelineSpecializationConstant> p_specialization_constants) override final;

	/*****************/
	/**** COMPUTE ****/
	/*****************/

	// ----- COMMANDS -----

	// Binding.
	virtual void command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override final;
	virtual void command_bind_compute_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;

	// Dispatching.
	virtual void command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) override final;
	virtual void command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) override final;

	// ----- PIPELINE -----

	virtual PipelineID compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) override final;

	/*****************/
	/**** QUERIES ****/
	/*****************/

	// ----- TIMESTAMP -----

	// Basic.
	virtual QueryPoolID timestamp_query_pool_create(uint32_t p_query_count) override final;
	virtual void timestamp_query_pool_free(QueryPoolID p_pool_id) override final;
	virtual void timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) override final;
	virtual uint64_t timestamp_query_result_to_time(uint64_t p_result) override final;

	// Commands.
	virtual void command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) override final;
	virtual void command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) override final;

	/****************/
	/**** LABELS ****/
	/****************/

	virtual void command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) override final;
	virtual void command_end_label(CommandBufferID p_cmd_buffer) override final;

	/****************/
	/**** DEBUG *****/
	/****************/
	virtual void command_insert_breadcrumb(CommandBufferID p_cmd_buffer, uint32_t p_data) override final;
	void print_lost_device_info();
	void on_device_lost() const;

	/********************/
	/**** SUBMISSION ****/
	/********************/

	virtual void begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) override final;
	virtual void end_segment() override final;

	/**************/
	/**** MISC ****/
	/**************/

	virtual void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override final;
	virtual uint64_t get_resource_native_handle(DriverResource p_type, ID p_driver_id) override final;
	virtual uint64_t get_total_memory_used() override final;

	virtual uint64_t limit_get(Limit p_limit) override final;
	virtual uint64_t api_trait_get(ApiTrait p_trait) override final;
	virtual bool has_feature(Features p_feature) override final;
	virtual const MultiviewCapabilities &get_multiview_capabilities() override final;
	virtual String get_api_name() const override final;
	virtual String get_api_version() const override final;
	virtual String get_pipeline_cache_uuid() const override final;
	virtual const Capabilities &get_capabilities() const override final;

	virtual bool is_composite_alpha_supported(CommandQueueID p_queue) const override final;

private:
	/*********************/
	/**** BOOKKEEPING ****/
	/*********************/

	using VersatileResource = VersatileResourceTemplate<
			BufferInfo,
			TextureInfo,
			VertexFormatInfo,
			ShaderInfo,
			UniformSetInfo>;
	PagedAllocator<VersatileResource, true> resources_allocator;

	/******************/

public:
	RenderingDeviceDriverVulkan(RenderingContextDriverVulkan *p_context_driver);
	virtual ~RenderingDeviceDriverVulkan();
};

using VKC = RenderingContextDriverVulkan;

#endif // RENDERING_DEVICE_DRIVER_VULKAN_H
