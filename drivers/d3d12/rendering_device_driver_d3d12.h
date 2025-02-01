/**************************************************************************/
/*  rendering_device_driver_d3d12.h                                       */
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

#include "core/templates/hash_map.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/self_list.h"
#include "servers/rendering/rendering_device_driver.h"

#ifndef _MSC_VER
// Match current version used by MinGW, MSVC and Direct3D 12 headers use 500.
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wswitch"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wstring-plus-int"
#pragma clang diagnostic ignored "-Wswitch"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#endif

#include "d3dx12.h"
#include <dxgi1_6.h>
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#include "D3D12MemAlloc.h"

#include <wrl/client.h>

#if defined(_MSC_VER) && defined(MemoryBarrier)
// Annoying define from winnt.h. Reintroduced by some of the headers above.
#undef MemoryBarrier
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

using Microsoft::WRL::ComPtr;

#define D3D12_BITCODE_OFFSETS_NUM_STAGES 3

#ifdef DEV_ENABLED
#define CUSTOM_INFO_QUEUE_ENABLED 0
#endif

class RenderingContextDriverD3D12;

// Design principles:
// - D3D12 structs are zero-initialized and fields not requiring a non-zero value are omitted (except in cases where expresivity reasons apply).
class RenderingDeviceDriverD3D12 : public RenderingDeviceDriver {
	/*****************/
	/**** GENERIC ****/
	/*****************/

	struct D3D12Format {
		DXGI_FORMAT family = DXGI_FORMAT_UNKNOWN;
		DXGI_FORMAT general_format = DXGI_FORMAT_UNKNOWN;
		UINT swizzle = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;
	};

	static const D3D12Format RD_TO_D3D12_FORMAT[RDD::DATA_FORMAT_MAX];

	struct DeviceLimits {
		uint64_t max_srvs_per_shader_stage = 0;
		uint64_t max_cbvs_per_shader_stage = 0;
		uint64_t max_samplers_across_all_stages = 0;
		uint64_t max_uavs_across_all_stages = 0;
		uint64_t timestamp_frequency = 0;
	};

	struct SubgroupCapabilities {
		uint32_t size = 0;
		bool wave_ops_supported = false;
		uint32_t supported_stages_flags_rd() const;
		uint32_t supported_operations_flags_rd() const;
	};

	struct VRSCapabilities {
		bool draw_call_supported = false; // We can specify our fragment rate on a draw call level.
		bool primitive_supported = false; // We can specify our fragment rate on each drawcall.
		bool primitive_in_multiviewport = false;
		bool ss_image_supported = false; // We can provide a density map attachment on our framebuffer.
		uint32_t ss_image_tile_size = 0;
		uint32_t ss_max_fragment_size = 0;
		bool additional_rates_supported = false;
	};

	struct ShaderCapabilities {
		D3D_SHADER_MODEL shader_model = (D3D_SHADER_MODEL)0;
		bool native_16bit_ops = false;
	};

	struct StorageBufferCapabilities {
		bool storage_buffer_16_bit_access_is_supported = false;
	};

	struct FormatCapabilities {
		bool relaxed_casting_supported = false;
	};

	struct BarrierCapabilities {
		bool enhanced_barriers_supported = false;
	};

	struct MiscFeaturesSupport {
		bool depth_bounds_supported = false;
	};

	RenderingContextDriverD3D12 *context_driver = nullptr;
	RenderingContextDriver::Device context_device;
	ComPtr<IDXGIAdapter> adapter;
	DXGI_ADAPTER_DESC adapter_desc;
	ComPtr<ID3D12Device> device;
	DeviceLimits device_limits;
	RDD::Capabilities device_capabilities;
	uint32_t feature_level = 0; // Major * 10 + minor.
	SubgroupCapabilities subgroup_capabilities;
	RDD::MultiviewCapabilities multiview_capabilities;
	VRSCapabilities vrs_capabilities;
	ShaderCapabilities shader_capabilities;
	StorageBufferCapabilities storage_buffer_capabilities;
	FormatCapabilities format_capabilities;
	BarrierCapabilities barrier_capabilities;
	MiscFeaturesSupport misc_features_support;
	String pipeline_cache_id;

	class DescriptorsHeap {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		ComPtr<ID3D12DescriptorHeap> heap;
		uint32_t handle_size = 0;

	public:
		class Walker { // Texas Ranger.
			friend class DescriptorsHeap;

			uint32_t handle_size = 0;
			uint32_t handle_count = 0;
			D3D12_CPU_DESCRIPTOR_HANDLE first_cpu_handle = {};
			D3D12_GPU_DESCRIPTOR_HANDLE first_gpu_handle = {};
			uint32_t handle_index = 0;

		public:
			D3D12_CPU_DESCRIPTOR_HANDLE get_curr_cpu_handle();
			D3D12_GPU_DESCRIPTOR_HANDLE get_curr_gpu_handle();
			_FORCE_INLINE_ void rewind() { handle_index = 0; }
			void advance(uint32_t p_count = 1);
			uint32_t get_current_handle_index() const { return handle_index; }
			uint32_t get_free_handles() { return handle_count - handle_index; }
			bool is_at_eof() { return handle_index == handle_count; }
		};

		Error allocate(ID3D12Device *m_device, D3D12_DESCRIPTOR_HEAP_TYPE m_type, uint32_t m_descriptor_count, bool p_for_gpu);
		uint32_t get_descriptor_count() const { return desc.NumDescriptors; }
		ID3D12DescriptorHeap *get_heap() const { return heap.Get(); }

		Walker make_walker() const;
	};

	struct {
		ComPtr<ID3D12CommandSignature> draw;
		ComPtr<ID3D12CommandSignature> draw_indexed;
		ComPtr<ID3D12CommandSignature> dispatch;
	} indirect_cmd_signatures;

	static void STDMETHODCALLTYPE _debug_message_func(D3D12_MESSAGE_CATEGORY p_category, D3D12_MESSAGE_SEVERITY p_severity, D3D12_MESSAGE_ID p_id, LPCSTR p_description, void *p_context);
	void _set_object_name(ID3D12Object *p_object, String p_object_name);
	Error _initialize_device();
	Error _check_capabilities();
	Error _get_device_limits();
	Error _initialize_allocator();
	Error _initialize_frames(uint32_t p_frame_count);
	Error _initialize_command_signatures();

public:
	Error initialize(uint32_t p_device_index, uint32_t p_frame_count) override final;

private:
	/****************/
	/**** MEMORY ****/
	/****************/

	ComPtr<D3D12MA::Allocator> allocator;

	/******************/
	/**** RESOURCE ****/
	/******************/

	struct ResourceInfo {
		struct States {
			// As many subresources as mipmaps * layers; planes (for depth-stencil) are tracked together.
			TightLocalVector<D3D12_RESOURCE_STATES> subresource_states; // Used only if not a view.
			uint32_t last_batch_with_uav_barrier = 0;
		};

		ID3D12Resource *resource = nullptr; // Non-null even if not owned.
		struct {
			ComPtr<ID3D12Resource> resource;
			ComPtr<D3D12MA::Allocation> allocation;
			States states;
		} owner_info; // All empty if the resource is not owned.
		States *states_ptr = nullptr; // Own or from another if it doesn't own the D3D12 resource.
	};

	struct BarrierRequest {
		static const uint32_t MAX_GROUPS = 4;
		// Maybe this is too much data to have it locally. Benchmarking may reveal that
		// cache would be used better by having a maximum of local subresource masks and beyond
		// that have an allocated vector with the rest.
		static const uint32_t MAX_SUBRESOURCES = 4096;
		ID3D12Resource *dx_resource = nullptr;
		uint8_t subres_mask_qwords = 0;
		uint8_t planes = 0;
		struct Group {
			D3D12_RESOURCE_STATES states = {};
			static_assert(MAX_SUBRESOURCES % 64 == 0);
			uint64_t subres_mask[MAX_SUBRESOURCES / 64] = {};
		} groups[MAX_GROUPS];
		uint8_t groups_count = 0;
		static const D3D12_RESOURCE_STATES DELETED_GROUP = D3D12_RESOURCE_STATES(0xFFFFFFFFU);
	};

	struct CommandBufferInfo;

	void _resource_transition_batch(CommandBufferInfo *p_command_buffer, ResourceInfo *p_resource, uint32_t p_subresource, uint32_t p_num_planes, D3D12_RESOURCE_STATES p_new_state);
	void _resource_transitions_flush(CommandBufferInfo *p_command_buffer);

	/*****************/
	/**** BUFFERS ****/
	/*****************/

	struct BufferInfo : public ResourceInfo {
		DataFormat texel_format = DATA_FORMAT_MAX;
		uint64_t size = 0;
		struct {
			bool usable_as_uav : 1;
		} flags = {};
	};

public:
	virtual BufferID buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type) override final;
	virtual bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) override final;
	virtual void buffer_free(BufferID p_buffer) override final;
	virtual uint64_t buffer_get_allocation_size(BufferID p_buffer) override final;
	virtual uint8_t *buffer_map(BufferID p_buffer) override final;
	virtual void buffer_unmap(BufferID p_buffer) override final;
	virtual uint64_t buffer_get_device_address(BufferID p_buffer) override final;

	/*****************/
	/**** TEXTURE ****/
	/*****************/
private:
	struct TextureInfo : public ResourceInfo {
		DataFormat format = DATA_FORMAT_MAX;
		CD3DX12_RESOURCE_DESC desc = {};
		uint32_t base_layer = 0;
		uint32_t layers = 0;
		uint32_t base_mip = 0;
		uint32_t mipmaps = 0;

		struct {
			D3D12_SHADER_RESOURCE_VIEW_DESC srv;
			D3D12_UNORDERED_ACCESS_VIEW_DESC uav;
		} view_descs = {};

		TextureInfo *main_texture = nullptr;

		UINT mapped_subresource = UINT_MAX;
		SelfList<TextureInfo> pending_clear{ this };
	};
	SelfList<TextureInfo>::List textures_pending_clear;

	HashMap<DXGI_FORMAT, uint32_t> format_sample_counts_mask_cache;
	Mutex format_sample_counts_mask_cache_mutex;

	uint32_t _find_max_common_supported_sample_count(VectorView<DXGI_FORMAT> p_formats);
	UINT _compute_component_mapping(const TextureView &p_view);
	UINT _compute_plane_slice(DataFormat p_format, BitField<TextureAspectBits> p_aspect_bits);
	UINT _compute_plane_slice(DataFormat p_format, TextureAspect p_aspect);
	UINT _compute_subresource_from_layers(TextureInfo *p_texture, const TextureSubresourceLayers &p_layers, uint32_t p_layer_offset);

	void _discard_texture_subresources(const TextureInfo *p_tex_info, const CommandBufferInfo *p_cmd_buf_info);

protected:
	virtual bool _unordered_access_supported_by_format(DataFormat p_format);

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

private:
	TextureID _texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps);

public:
	/*****************/
	/**** SAMPLER ****/
	/*****************/
private:
	LocalVector<D3D12_SAMPLER_DESC> samplers;

public:
	virtual SamplerID sampler_create(const SamplerState &p_state) final override;
	virtual void sampler_free(SamplerID p_sampler) final override;
	virtual bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) override final;

	/**********************/
	/**** VERTEX ARRAY ****/
	/**********************/
private:
	struct VertexFormatInfo {
		TightLocalVector<D3D12_INPUT_ELEMENT_DESC> input_elem_descs;
		TightLocalVector<UINT> vertex_buffer_strides;
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
			VectorView<RDD::MemoryBarrier> p_memory_barriers,
			VectorView<RDD::BufferBarrier> p_buffer_barriers,
			VectorView<RDD::TextureBarrier> p_texture_barriers) override final;

private:
	/****************/
	/**** FENCES ****/
	/****************/

	struct FenceInfo {
		ComPtr<ID3D12Fence> d3d_fence = nullptr;
		HANDLE event_handle = nullptr;
		UINT64 fence_value = 0;
	};

public:
	virtual FenceID fence_create() override;
	virtual Error fence_wait(FenceID p_fence) override;
	virtual void fence_free(FenceID p_fence) override;

private:
	/********************/
	/**** SEMAPHORES ****/
	/********************/

	struct SemaphoreInfo {
		ComPtr<ID3D12Fence> d3d_fence = nullptr;
		UINT64 fence_value = 0;
	};

	virtual SemaphoreID semaphore_create() override;
	virtual void semaphore_free(SemaphoreID p_semaphore) override;

	/******************/
	/**** COMMANDS ****/
	/******************/

	// ----- QUEUE FAMILY -----

	virtual CommandQueueFamilyID command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface = 0) override;

private:
	// ----- QUEUE -----

	struct CommandQueueInfo {
		ComPtr<ID3D12CommandQueue> d3d_queue;
	};

public:
	virtual CommandQueueID command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue = false) override;
	virtual Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_semaphores, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) override;
	virtual void command_queue_free(CommandQueueID p_cmd_queue) override;

private:
	// ----- POOL -----
	struct CommandPoolInfo {
		CommandQueueFamilyID queue_family;
		CommandBufferType buffer_type = COMMAND_BUFFER_TYPE_PRIMARY;
		// Since there are no command pools in D3D12, we need to track the command buffers created by this pool
		// so that we can free them when the pool is freed.
		SelfList<CommandBufferInfo>::List command_buffers;
	};

public:
	virtual CommandPoolID command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) override final;
	virtual bool command_pool_reset(CommandPoolID p_cmd_pool) override final;
	virtual void command_pool_free(CommandPoolID p_cmd_pool) override final;

	// ----- BUFFER -----

private:
	// Belongs to RENDERING-SUBPASS, but needed here.
	struct FramebufferInfo;
	struct RenderPassInfo;
	struct RenderPassState {
		uint32_t current_subpass = UINT32_MAX;
		const FramebufferInfo *fb_info = nullptr;
		const RenderPassInfo *pass_info = nullptr;
		CD3DX12_RECT region_rect = {};
		bool region_is_all = false;

		const VertexFormatInfo *vf_info = nullptr;
		D3D12_VERTEX_BUFFER_VIEW vertex_buffer_views[8] = {};
		uint32_t vertex_buffer_count = 0;
	};

	// Leveraging knowledge of actual usage and D3D12 specifics (namely, command lists from the same allocator
	// can't be freely begun and ended), an allocator per list works better.
	struct CommandBufferInfo {
		// Store a self list reference to be used by the command pool.
		SelfList<CommandBufferInfo> command_buffer_info_elem{ this };

		ComPtr<ID3D12CommandAllocator> cmd_allocator;
		ComPtr<ID3D12GraphicsCommandList> cmd_list;

		ID3D12PipelineState *graphics_pso = nullptr;
		ID3D12PipelineState *compute_pso = nullptr;

		uint32_t graphics_root_signature_crc = 0;
		uint32_t compute_root_signature_crc = 0;

		RenderPassState render_pass_state;
		bool descriptor_heaps_set = false;

		HashMap<ResourceInfo::States *, BarrierRequest> res_barriers_requests;
		LocalVector<D3D12_RESOURCE_BARRIER> res_barriers;
		uint32_t res_barriers_count = 0;
		uint32_t res_barriers_batch = 0;
	};

public:
	virtual CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool) override final;
	virtual bool command_buffer_begin(CommandBufferID p_cmd_buffer) override final;
	virtual bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) override final;
	virtual void command_buffer_end(CommandBufferID p_cmd_buffer) override final;
	virtual void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) override final;

private:
	/********************/
	/**** SWAP CHAIN ****/
	/********************/

	struct SwapChain {
		ComPtr<IDXGISwapChain3> d3d_swap_chain;
		RenderingContextDriver::SurfaceID surface = RenderingContextDriver::SurfaceID();
		UINT present_flags = 0;
		UINT sync_interval = 1;
		UINT creation_flags = 0;
		RenderPassID render_pass;
		TightLocalVector<ID3D12Resource *> render_targets;
		TightLocalVector<TextureInfo> render_targets_info;
		TightLocalVector<FramebufferID> framebuffers;
		RDD::DataFormat data_format = DATA_FORMAT_MAX;
	};

	void _swap_chain_release(SwapChain *p_swap_chain);
	void _swap_chain_release_buffers(SwapChain *p_swap_chain);

public:
	virtual SwapChainID swap_chain_create(RenderingContextDriver::SurfaceID p_surface) override;
	virtual Error swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) override;
	virtual FramebufferID swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) override;
	virtual RenderPassID swap_chain_get_render_pass(SwapChainID p_swap_chain) override;
	virtual DataFormat swap_chain_get_format(SwapChainID p_swap_chain) override;
	virtual void swap_chain_free(SwapChainID p_swap_chain) override;

	/*********************/
	/**** FRAMEBUFFER ****/
	/*********************/
private:
	struct FramebufferInfo {
		bool is_screen = false;
		Size2i size;
		TightLocalVector<uint32_t> attachments_handle_inds; // RTV heap index for color; DSV heap index for DSV.
		DescriptorsHeap rtv_heap;
		DescriptorsHeap dsv_heap; // Used only if not for screen and some depth-stencil attachments.

		TightLocalVector<TextureID> attachments; // Color and depth-stencil. Used if not screen.
		TextureID vrs_attachment;
	};

	D3D12_RENDER_TARGET_VIEW_DESC _make_rtv_for_texture(const TextureInfo *p_texture_info, uint32_t p_mipmap_offset, uint32_t p_layer_offset, uint32_t p_layers, bool p_add_bases = true);
	D3D12_UNORDERED_ACCESS_VIEW_DESC _make_ranged_uav_for_texture(const TextureInfo *p_texture_info, uint32_t p_mipmap_offset, uint32_t p_layer_offset, uint32_t p_layers, bool p_add_bases = true);
	D3D12_DEPTH_STENCIL_VIEW_DESC _make_dsv_for_texture(const TextureInfo *p_texture_info);

	FramebufferID _framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height, bool p_is_screen);

public:
	virtual FramebufferID framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) override final;
	virtual void framebuffer_free(FramebufferID p_framebuffer) override final;

	/****************/
	/**** SHADER ****/
	/****************/
private:
	static const uint32_t ROOT_SIGNATURE_SIZE = 256;
	static const uint32_t PUSH_CONSTANT_SIZE = 128; // Mimicking Vulkan.

	enum {
		// We can only aim to set a maximum here, since depending on the shader
		// there may be more or less root signature free for descriptor tables.
		// Therefore, we'll have to rely on the final check at runtime, when building
		// the root signature structure for a given shader.
		// To be precise, these may be present or not, and their size vary statically:
		// - Push constant (we'll assume this is always present to avoid reserving much
		//   more space for descriptor sets than needed for almost any imaginable case,
		//   given that most shader templates feature push constants).
		// - NIR-DXIL runtime data.
		MAX_UNIFORM_SETS = (ROOT_SIGNATURE_SIZE - PUSH_CONSTANT_SIZE) / sizeof(uint32_t),
	};

	enum RootSignatureLocationType {
		RS_LOC_TYPE_RESOURCE,
		RS_LOC_TYPE_SAMPLER,
	};

	enum ResourceClass {
		RES_CLASS_INVALID,
		RES_CLASS_CBV,
		RES_CLASS_SRV,
		RES_CLASS_UAV,
	};

	struct ShaderBinary {
		// Version 1: Initial.
		// Version 2: 64-bit vertex input mask.
		// Version 3: Added SC stage mask.
		static const uint32_t VERSION = 3;

		// Phase 1: SPIR-V reflection, where the Vulkan/RD interface of the shader is discovered.
		// Phase 2: SPIR-V to DXIL translation, where the DXIL interface is discovered, which may have gaps due to optimizations.

		struct DataBinding {
			// - Phase 1.
			uint32_t type = 0;
			uint32_t binding = 0;
			uint32_t stages = 0;
			uint32_t length = 0; // Size of arrays (in total elements), or ubos (in bytes * total elements).
			uint32_t writable = 0;
			// - Phase 2.
			uint32_t res_class = 0;
			uint32_t has_sampler = 0;
			uint32_t dxil_stages = 0;
			struct RootSignatureLocation {
				uint32_t root_param_idx = UINT32_MAX; // UINT32_MAX if unused.
				uint32_t range_idx = UINT32_MAX; // UINT32_MAX if unused.
			};
			RootSignatureLocation root_sig_locations[2]; // Index is RootSignatureLocationType.

			// We need to sort these to fill the root signature locations properly.
			bool operator<(const DataBinding &p_other) const {
				return binding < p_other.binding;
			}
		};

		struct SpecializationConstant {
			// - Phase 1.
			uint32_t type = 0;
			uint32_t constant_id = 0;
			union {
				uint32_t int_value = 0;
				float float_value;
				bool bool_value;
			};
			uint32_t stage_flags = 0;
			// - Phase 2.
			uint64_t stages_bit_offsets[D3D12_BITCODE_OFFSETS_NUM_STAGES] = {};
		};

		struct Data {
			uint64_t vertex_input_mask = 0;
			uint32_t fragment_output_mask = 0;
			uint32_t specialization_constants_count = 0;
			uint32_t spirv_specialization_constants_ids_mask = 0;
			uint32_t is_compute = 0;
			uint32_t compute_local_size[3] = {};
			uint32_t set_count = 0;
			uint32_t push_constant_size = 0;
			uint32_t dxil_push_constant_stages = 0; // Phase 2.
			uint32_t nir_runtime_data_root_param_idx = 0; // Phase 2.
			uint32_t stage_count = 0;
			uint32_t shader_name_len = 0;
			uint32_t root_signature_len = 0;
			uint32_t root_signature_crc = 0;
		};
	};

	struct ShaderInfo {
		uint32_t dxil_push_constant_size = 0;
		uint32_t nir_runtime_data_root_param_idx = UINT32_MAX;
		bool is_compute = false;

		struct UniformBindingInfo {
			uint32_t stages = 0; // Actual shader stages using the uniform (0 if totally optimized out).
			ResourceClass res_class = RES_CLASS_INVALID;
			UniformType type = UNIFORM_TYPE_MAX;
			uint32_t length = UINT32_MAX;
#ifdef DEV_ENABLED
			bool writable = false;
#endif
			struct RootSignatureLocation {
				uint32_t root_param_idx = UINT32_MAX;
				uint32_t range_idx = UINT32_MAX;
			};
			struct {
				RootSignatureLocation resource;
				RootSignatureLocation sampler;
			} root_sig_locations;
		};

		struct UniformSet {
			TightLocalVector<UniformBindingInfo> bindings;
			struct {
				uint32_t resources = 0;
				uint32_t samplers = 0;
			} num_root_params;
		};

		TightLocalVector<UniformSet> sets;

		struct SpecializationConstant {
			uint32_t constant_id = UINT32_MAX;
			uint32_t int_value = UINT32_MAX;
			uint64_t stages_bit_offsets[D3D12_BITCODE_OFFSETS_NUM_STAGES] = {};
		};

		TightLocalVector<SpecializationConstant> specialization_constants;
		uint32_t spirv_specialization_constants_ids_mask = 0;

		HashMap<ShaderStage, Vector<uint8_t>> stages_bytecode;

		ComPtr<ID3D12RootSignature> root_signature;
		ComPtr<ID3D12RootSignatureDeserializer> root_signature_deserializer;
		const D3D12_ROOT_SIGNATURE_DESC *root_signature_desc = nullptr; // Owned by the deserializer.
		uint32_t root_signature_crc = 0;
	};

	uint32_t _shader_patch_dxil_specialization_constant(
			PipelineSpecializationConstantType p_type,
			const void *p_value,
			const uint64_t (&p_stages_bit_offsets)[D3D12_BITCODE_OFFSETS_NUM_STAGES],
			HashMap<ShaderStage, Vector<uint8_t>> &r_stages_bytecodes,
			bool p_is_first_patch);
	bool _shader_apply_specialization_constants(
			const ShaderInfo *p_shader_info,
			VectorView<PipelineSpecializationConstant> p_specialization_constants,
			HashMap<ShaderStage, Vector<uint8_t>> &r_final_stages_bytecode);
	void _shader_sign_dxil_bytecode(ShaderStage p_stage, Vector<uint8_t> &r_dxil_blob);

public:
	virtual String shader_get_binary_cache_key() override final;
	virtual Vector<uint8_t> shader_compile_binary_from_spirv(VectorView<ShaderStageSPIRVData> p_spirv, const String &p_shader_name) override final;
	virtual ShaderID shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, ShaderDescription &r_shader_desc, String &r_name, const Vector<ImmutableSampler> &p_immutable_samplers) override final;
	virtual uint32_t shader_get_layout_hash(ShaderID p_shader) override final;
	virtual void shader_free(ShaderID p_shader) override final;
	virtual void shader_destroy_modules(ShaderID p_shader) override final;

	/*********************/
	/**** UNIFORM SET ****/
	/*********************/

private:
	struct RootDescriptorTable {
		uint32_t root_param_idx = UINT32_MAX;
		D3D12_GPU_DESCRIPTOR_HANDLE start_gpu_handle = {};
	};

	struct UniformSetInfo {
		struct {
			DescriptorsHeap resources;
			DescriptorsHeap samplers;
		} desc_heaps;

		struct StateRequirement {
			ResourceInfo *resource = nullptr;
			bool is_buffer = false;
			D3D12_RESOURCE_STATES states = {};
			uint64_t shader_uniform_idx_mask = 0;
		};
		TightLocalVector<StateRequirement> resource_states;

		struct RecentBind {
			uint64_t segment_serial = 0;
			uint32_t root_signature_crc = 0;
			struct {
				TightLocalVector<RootDescriptorTable> resources;
				TightLocalVector<RootDescriptorTable> samplers;
			} root_tables;
			int uses = 0;
		} recent_binds[4]; // A better amount may be empirically found.

#ifdef DEV_ENABLED
		// Filthy, but useful for dev.
		struct ResourceDescInfo {
			D3D12_DESCRIPTOR_RANGE_TYPE type;
			D3D12_SRV_DIMENSION srv_dimension;
		};
		TightLocalVector<ResourceDescInfo> resources_desc_info;
#endif
	};

public:
	virtual UniformSetID uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) override final;
	virtual void uniform_set_free(UniformSetID p_uniform_set) override final;

	// ----- COMMANDS -----

	virtual void command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;

private:
	void _command_check_descriptor_sets(CommandBufferID p_cmd_buffer);
	void _command_bind_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index, bool p_for_compute);
	void _command_bind_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count, bool p_for_compute);

public:
	/******************/
	/**** TRANSFER ****/
	/******************/

	virtual void command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) override final;
	virtual void command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) override final;

	virtual void command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) override final;
	virtual void command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) override final;
	virtual void command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) override final;

public:
	virtual void command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) override final;
	virtual void command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) override final;

	/******************/
	/**** PIPELINE ****/
	/******************/

	struct RenderPipelineInfo {
		const VertexFormatInfo *vf_info = nullptr;

		struct {
			D3D12_PRIMITIVE_TOPOLOGY primitive_topology = {};
			Color blend_constant;
			float depth_bounds_min = 0.0f;
			float depth_bounds_max = 0.0f;
			uint32_t stencil_reference = 0;
		} dyn_params;
	};

	struct PipelineInfo {
		ID3D12PipelineState *pso = nullptr;
		const ShaderInfo *shader_info = nullptr;
		RenderPipelineInfo render_info;
	};

	virtual void pipeline_free(PipelineID p_pipeline) override final;

public:
	// ----- BINDING -----

	virtual void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_dst_first_index, VectorView<uint32_t> p_data) override final;

	// ----- CACHE -----

	virtual bool pipeline_cache_create(const Vector<uint8_t> &p_data) override final;
	virtual void pipeline_cache_free() override final;
	virtual size_t pipeline_cache_query_size() override final;
	virtual Vector<uint8_t> pipeline_cache_serialize() override final;

	/*******************/
	/**** RENDERING ****/
	/*******************/

	// ----- SUBPASS -----

private:
	struct RenderPassInfo {
		TightLocalVector<Attachment> attachments;
		TightLocalVector<Subpass> subpasses;
		uint32_t view_count = 0;
		uint32_t max_supported_sample_count = 0;
	};

public:
	virtual RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count) override final;
	virtual void render_pass_free(RenderPassID p_render_pass) override final;

	// ----- COMMANDS -----

	virtual void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) override final;

private:
	void _end_render_pass(CommandBufferID p_cmd_buffer);

public:
	virtual void command_end_render_pass(CommandBufferID p_cmd_buffer) override final;
	virtual void command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) override final;
	virtual void command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) override final;
	virtual void command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) override final;

	virtual void command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) override final;

	// Binding.
	virtual void command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) override final;
	virtual void command_bind_render_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;
	virtual void command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) override final;

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

private:
	void _bind_vertex_buffers(CommandBufferInfo *p_cmd_buf_info);

public:
	// Dynamic state.
	virtual void command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) override final;
	virtual void command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) override final;

	// ----- PIPELINE -----

public:
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
	virtual void command_bind_compute_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) override final;

	// Dispatching.
	virtual void command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) override final;
	virtual void command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) override final;

	// ----- PIPELINE -----

	virtual PipelineID compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) override final;

	/*****************/
	/**** QUERIES ****/
	/*****************/

	// ----- TIMESTAMP -----

private:
	struct TimestampQueryPoolInfo {
		ComPtr<ID3D12QueryHeap> query_heap;
		uint32_t query_count = 0;
		ComPtr<D3D12MA::Allocation> results_buffer_allocation;
	};

public:
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

	/********************/
	/**** SUBMISSION ****/
	/********************/
private:
	struct FrameInfo {
		struct {
			DescriptorsHeap resources;
			DescriptorsHeap samplers;
			DescriptorsHeap aux;
			DescriptorsHeap rtv;
		} desc_heaps;
		struct {
			DescriptorsHeap::Walker resources;
			DescriptorsHeap::Walker samplers;
			DescriptorsHeap::Walker aux;
			DescriptorsHeap::Walker rtv;
		} desc_heap_walkers;
		struct {
			bool resources = false;
			bool samplers = false;
			bool aux = false;
			bool rtv = false;
		} desc_heaps_exhausted_reported;
		CD3DX12_CPU_DESCRIPTOR_HANDLE null_rtv_handle = {}; // For [[MANUAL_SUBPASSES]].
		uint32_t segment_serial = 0;

#ifdef DEV_ENABLED
		uint32_t uniform_set_reused = 0;
#endif
	};
	TightLocalVector<FrameInfo> frames;
	uint32_t frame_idx = 0;
	uint32_t frames_drawn = 0;
	uint32_t segment_serial = 0;
	bool segment_begun = false;
	HashMap<uint64_t, bool> has_comp_alpha;

public:
	virtual void begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) override final;
	virtual void end_segment() override final;

	/**************/
	/**** MISC ****/
	/**************/

	virtual void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override final;
	virtual uint64_t get_resource_native_handle(DriverResource p_type, ID p_driver_id) override final;
	virtual uint64_t get_total_memory_used() override final;
	virtual uint64_t get_lazily_memory_used() override final;
	virtual uint64_t limit_get(Limit p_limit) override final;
	virtual uint64_t api_trait_get(ApiTrait p_trait) override final;
	virtual bool has_feature(Features p_feature) override final;
	virtual const MultiviewCapabilities &get_multiview_capabilities() override final;
	virtual String get_api_name() const override final;
	virtual String get_api_version() const override final;
	virtual String get_pipeline_cache_uuid() const override final;
	virtual const Capabilities &get_capabilities() const override final;

	virtual bool is_composite_alpha_supported(CommandQueueID p_queue) const override final;

	static bool is_in_developer_mode();

private:
	/*********************/
	/**** BOOKKEEPING ****/
	/*********************/

	using VersatileResource = VersatileResourceTemplate<
			BufferInfo,
			TextureInfo,
			TextureInfo,
			TextureInfo,
			VertexFormatInfo,
			CommandBufferInfo,
			FramebufferInfo,
			ShaderInfo,
			UniformSetInfo,
			RenderPassInfo,
			TimestampQueryPoolInfo>;
	PagedAllocator<VersatileResource, true> resources_allocator;

	/******************/

public:
	RenderingDeviceDriverD3D12(RenderingContextDriverD3D12 *p_context_driver);
	virtual ~RenderingDeviceDriverD3D12();
};
