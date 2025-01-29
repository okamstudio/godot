/**************************************************************************/
/*  rendering_device_driver.h                                             */
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

#ifndef RENDERING_DEVICE_DRIVER_H
#define RENDERING_DEVICE_DRIVER_H

// ***********************************************************************************
// RenderingDeviceDriver - Design principles
// -----------------------------------------
// - Very little validation is done, and normally only in dev or debug builds.
// - Error reporting is generally simple: returning an id of 0 or a false boolean.
// - Certain enums/constants/structs follow Vulkan values/layout. That makes things easier for RDDVulkan (it asserts compatibility).
// - We allocate as little as possible in functions expected to be quick (a counterexample is loading/saving shaders) and use alloca() whenever suitable.
// - We try to back opaque ids with the native ones or memory addresses.
// - When using bookkeeping structures because the actual API id of a resource is not enough, we use a PagedAllocator.
// - Every struct has default initializers.
// - Using VectorView to take array-like arguments. Vector<uint8_t> is an exception (an indiom for "BLOB").
// - If a driver needs some higher-level information (the kind of info RenderingDevice keeps), it shall store a copy of what it needs.
//   There's no backwards communication from the driver to query data from RenderingDevice.
// ***********************************************************************************

#include "core/object/object.h"
#include "core/variant/type_info.h"
#include "servers/rendering/rendering_context_driver.h"
#include "servers/rendering/rendering_device_commons.h"

#include <algorithm>

// This may one day be used in Godot for interoperability between C arrays, Vector and LocalVector.
// (See https://github.com/godotengine/godot-proposals/issues/5144.)
template <typename T>
class VectorView {
	const T *_ptr = nullptr;
	const uint32_t _size = 0;

public:
	const T &operator[](uint32_t p_index) {
		DEV_ASSERT(p_index < _size);
		return _ptr[p_index];
	}

	_ALWAYS_INLINE_ const T *ptr() const { return _ptr; }
	_ALWAYS_INLINE_ uint32_t size() const { return _size; }

	VectorView() = default;
	VectorView(const T &p_ptr) :
			// With this one you can pass a single element very conveniently!
			_ptr(&p_ptr),
			_size(1) {}
	VectorView(const T *p_ptr, uint32_t p_size) :
			_ptr(p_ptr), _size(p_size) {}
	VectorView(const Vector<T> &p_lv) :
			_ptr(p_lv.ptr()), _size(p_lv.size()) {}
	VectorView(const LocalVector<T> &p_lv) :
			_ptr(p_lv.ptr()), _size(p_lv.size()) {}
};

// These utilities help drivers avoid allocations.
#define ALLOCA(m_size) ((m_size != 0) ? alloca(m_size) : nullptr)
#define ALLOCA_ARRAY(m_type, m_count) ((m_type *)ALLOCA(sizeof(m_type) * (m_count)))
#define ALLOCA_SINGLE(m_type) ALLOCA_ARRAY(m_type, 1)

// This helps forwarding certain arrays to the API with confidence.
#define ARRAYS_COMPATIBLE(m_type_a, m_type_b) (sizeof(m_type_a) == sizeof(m_type_b) && alignof(m_type_a) == alignof(m_type_b))
// This is used when you also need to ensure structured types are compatible field-by-field.
// TODO: The fieldwise check is unimplemented, but still this one is useful, as a strong annotation about the needs.
#define ARRAYS_COMPATIBLE_FIELDWISE(m_type_a, m_type_b) ARRAYS_COMPATIBLE(m_type_a, m_type_b)
// Another utility, to make it easy to compare members of different enums, which is not fine with some compilers.
#define ENUM_MEMBERS_EQUAL(m_a, m_b) ((int64_t)m_a == (int64_t)m_b)

// This helps using a single paged allocator for many resource types.
template <typename... RESOURCE_TYPES>
struct VersatileResourceTemplate {
	static constexpr size_t RESOURCE_SIZES[] = { sizeof(RESOURCE_TYPES)... };
	static constexpr size_t MAX_RESOURCE_SIZE = std::max_element(RESOURCE_SIZES, RESOURCE_SIZES + sizeof...(RESOURCE_TYPES))[0];
	uint8_t data[MAX_RESOURCE_SIZE];

	template <typename T>
	static T *allocate(PagedAllocator<VersatileResourceTemplate, true> &p_allocator) {
		T *obj = (T *)p_allocator.alloc();
		memnew_placement(obj, T);
		return obj;
	}

	template <typename T>
	static void free(PagedAllocator<VersatileResourceTemplate, true> &p_allocator, T *p_object) {
		p_object->~T();
		p_allocator.free((VersatileResourceTemplate *)p_object);
	}
};

class RenderingDeviceDriver : public RenderingDeviceCommons {
public:
	struct ID {
		uint64_t id = 0;
		_ALWAYS_INLINE_ ID() = default;
		_ALWAYS_INLINE_ ID(uint64_t p_id) :
				id(p_id) {}
	};

#define DEFINE_ID(m_name)                                                         \
	struct m_name##ID : public ID {                                               \
		_ALWAYS_INLINE_ explicit operator bool() const {                          \
			return id != 0;                                                       \
		}                                                                         \
		_ALWAYS_INLINE_ m_name##ID &operator=(m_name##ID p_other) {               \
			id = p_other.id;                                                      \
			return *this;                                                         \
		}                                                                         \
		_ALWAYS_INLINE_ bool operator<(const m_name##ID &p_other) const {         \
			return id < p_other.id;                                               \
		}                                                                         \
		_ALWAYS_INLINE_ bool operator==(const m_name##ID &p_other) const {        \
			return id == p_other.id;                                              \
		}                                                                         \
		_ALWAYS_INLINE_ bool operator!=(const m_name##ID &p_other) const {        \
			return id != p_other.id;                                              \
		}                                                                         \
		_ALWAYS_INLINE_ m_name##ID(const m_name##ID &p_other) : ID(p_other.id) {} \
		_ALWAYS_INLINE_ explicit m_name##ID(uint64_t p_int) : ID(p_int) {}        \
		_ALWAYS_INLINE_ explicit m_name##ID(void *p_ptr) : ID((uint64_t)p_ptr) {} \
		_ALWAYS_INLINE_ m_name##ID() = default;                                   \
	};

	// Id types declared before anything else to prevent cyclic dependencies between the different concerns.
	DEFINE_ID(Buffer);
	DEFINE_ID(Texture);
	DEFINE_ID(Sampler);
	DEFINE_ID(VertexFormat);
	DEFINE_ID(CommandQueue);
	DEFINE_ID(CommandQueueFamily);
	DEFINE_ID(CommandPool);
	DEFINE_ID(CommandBuffer);
	DEFINE_ID(SwapChain);
	DEFINE_ID(Framebuffer);
	DEFINE_ID(Shader);
	DEFINE_ID(UniformSet);
	DEFINE_ID(Pipeline);
	DEFINE_ID(RenderPass);
	DEFINE_ID(QueryPool);
	DEFINE_ID(Fence);
	DEFINE_ID(Semaphore);

public:
	/*****************/
	/**** GENERIC ****/
	/*****************/

	virtual Error initialize(uint32_t p_device_index, uint32_t p_frame_count) = 0;

	/****************/
	/**** MEMORY ****/
	/****************/

	enum MemoryAllocationType {
		MEMORY_ALLOCATION_TYPE_CPU, // For images, CPU allocation also means linear, GPU is tiling optimal.
		MEMORY_ALLOCATION_TYPE_GPU,
	};

	/*****************/
	/**** BUFFERS ****/
	/*****************/

	enum BufferUsageBits {
		BUFFER_USAGE_TRANSFER_FROM_BIT = (1 << 0),
		BUFFER_USAGE_TRANSFER_TO_BIT = (1 << 1),
		BUFFER_USAGE_TEXEL_BIT = (1 << 2),
		BUFFER_USAGE_UNIFORM_BIT = (1 << 4),
		BUFFER_USAGE_STORAGE_BIT = (1 << 5),
		BUFFER_USAGE_INDEX_BIT = (1 << 6),
		BUFFER_USAGE_VERTEX_BIT = (1 << 7),
		BUFFER_USAGE_INDIRECT_BIT = (1 << 8),
		BUFFER_USAGE_DEVICE_ADDRESS_BIT = (1 << 17),
	};

	enum {
		BUFFER_WHOLE_SIZE = ~0ULL
	};

	virtual BufferID buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type) = 0;
	// Only for a buffer with BUFFER_USAGE_TEXEL_BIT.
	virtual bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) = 0;
	virtual void buffer_free(BufferID p_buffer) = 0;
	virtual uint64_t buffer_get_allocation_size(BufferID p_buffer) = 0;
	virtual uint8_t *buffer_map(BufferID p_buffer) = 0;
	virtual void buffer_unmap(BufferID p_buffer) = 0;
	// Only for a buffer with BUFFER_USAGE_DEVICE_ADDRESS_BIT.
	virtual uint64_t buffer_get_device_address(BufferID p_buffer) = 0;

	/*****************/
	/**** TEXTURE ****/
	/*****************/

	struct TextureView {
		DataFormat format = DATA_FORMAT_MAX;
		TextureSwizzle swizzle_r = TEXTURE_SWIZZLE_R;
		TextureSwizzle swizzle_g = TEXTURE_SWIZZLE_G;
		TextureSwizzle swizzle_b = TEXTURE_SWIZZLE_B;
		TextureSwizzle swizzle_a = TEXTURE_SWIZZLE_A;
	};

	enum TextureLayout {
		TEXTURE_LAYOUT_UNDEFINED,
		TEXTURE_LAYOUT_GENERAL,
		TEXTURE_LAYOUT_STORAGE_OPTIMAL,
		TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		TEXTURE_LAYOUT_COPY_SRC_OPTIMAL,
		TEXTURE_LAYOUT_COPY_DST_OPTIMAL,
		TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL,
		TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL,
		TEXTURE_LAYOUT_VRS_ATTACHMENT_OPTIMAL,
		TEXTURE_LAYOUT_MAX
	};

	enum TextureAspect {
		TEXTURE_ASPECT_COLOR = 0,
		TEXTURE_ASPECT_DEPTH = 1,
		TEXTURE_ASPECT_STENCIL = 2,
		TEXTURE_ASPECT_MAX
	};

	enum TextureAspectBits {
		TEXTURE_ASPECT_COLOR_BIT = (1 << TEXTURE_ASPECT_COLOR),
		TEXTURE_ASPECT_DEPTH_BIT = (1 << TEXTURE_ASPECT_DEPTH),
		TEXTURE_ASPECT_STENCIL_BIT = (1 << TEXTURE_ASPECT_STENCIL),
	};

	struct TextureSubresource {
		TextureAspect aspect = TEXTURE_ASPECT_COLOR;
		uint32_t layer = 0;
		uint32_t mipmap = 0;
	};

	struct TextureSubresourceLayers {
		BitField<TextureAspectBits> aspect;
		uint32_t mipmap = 0;
		uint32_t base_layer = 0;
		uint32_t layer_count = 0;
	};

	struct TextureSubresourceRange {
		BitField<TextureAspectBits> aspect;
		uint32_t base_mipmap = 0;
		uint32_t mipmap_count = 0;
		uint32_t base_layer = 0;
		uint32_t layer_count = 0;
	};

	struct TextureCopyableLayout {
		uint64_t offset = 0;
		uint64_t size = 0;
		uint64_t row_pitch = 0;
		uint64_t depth_pitch = 0;
		uint64_t layer_pitch = 0;
	};

	virtual TextureID texture_create(const TextureFormat &p_format, const TextureView &p_view) = 0;
	virtual TextureID texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil) = 0;
	// texture_create_shared_*() can only use original, non-view textures as original. RenderingDevice is responsible for ensuring that.
	virtual TextureID texture_create_shared(TextureID p_original_texture, const TextureView &p_view) = 0;
	virtual TextureID texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) = 0;
	virtual void texture_free(TextureID p_texture) = 0;
	virtual uint64_t texture_get_allocation_size(TextureID p_texture) = 0;
	virtual void texture_get_copyable_layout(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) = 0;
	virtual uint8_t *texture_map(TextureID p_texture, const TextureSubresource &p_subresource) = 0;
	virtual void texture_unmap(TextureID p_texture) = 0;
	virtual BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) = 0;
	virtual bool texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool &r_raw_reinterpretation) = 0;

	/*****************/
	/**** SAMPLER ****/
	/*****************/

	virtual SamplerID sampler_create(const SamplerState &p_state) = 0;
	virtual void sampler_free(SamplerID p_sampler) = 0;
	virtual bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) = 0;

	/**********************/
	/**** VERTEX ARRAY ****/
	/**********************/

	virtual VertexFormatID vertex_format_create(VectorView<VertexAttribute> p_vertex_attribs) = 0;
	virtual void vertex_format_free(VertexFormatID p_vertex_format) = 0;

	/******************/
	/**** BARRIERS ****/
	/******************/

	enum PipelineStageBits {
		PIPELINE_STAGE_TOP_OF_PIPE_BIT = (1 << 0),
		PIPELINE_STAGE_DRAW_INDIRECT_BIT = (1 << 1),
		PIPELINE_STAGE_VERTEX_INPUT_BIT = (1 << 2),
		PIPELINE_STAGE_VERTEX_SHADER_BIT = (1 << 3),
		PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT = (1 << 4),
		PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT = (1 << 5),
		PIPELINE_STAGE_GEOMETRY_SHADER_BIT = (1 << 6),
		PIPELINE_STAGE_FRAGMENT_SHADER_BIT = (1 << 7),
		PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT = (1 << 8),
		PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT = (1 << 9),
		PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT = (1 << 10),
		PIPELINE_STAGE_COMPUTE_SHADER_BIT = (1 << 11),
		PIPELINE_STAGE_COPY_BIT = (1 << 12),
		PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT = (1 << 13),
		PIPELINE_STAGE_RESOLVE_BIT = (1 << 14),
		PIPELINE_STAGE_ALL_GRAPHICS_BIT = (1 << 15),
		PIPELINE_STAGE_ALL_COMMANDS_BIT = (1 << 16),
		PIPELINE_STAGE_CLEAR_STORAGE_BIT = (1 << 17),
	};

	enum BarrierAccessBits {
		BARRIER_ACCESS_INDIRECT_COMMAND_READ_BIT = (1 << 0),
		BARRIER_ACCESS_INDEX_READ_BIT = (1 << 1),
		BARRIER_ACCESS_VERTEX_ATTRIBUTE_READ_BIT = (1 << 2),
		BARRIER_ACCESS_UNIFORM_READ_BIT = (1 << 3),
		BARRIER_ACCESS_INPUT_ATTACHMENT_READ_BIT = (1 << 4),
		BARRIER_ACCESS_SHADER_READ_BIT = (1 << 5),
		BARRIER_ACCESS_SHADER_WRITE_BIT = (1 << 6),
		BARRIER_ACCESS_COLOR_ATTACHMENT_READ_BIT = (1 << 7),
		BARRIER_ACCESS_COLOR_ATTACHMENT_WRITE_BIT = (1 << 8),
		BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT = (1 << 9),
		BARRIER_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT = (1 << 10),
		BARRIER_ACCESS_COPY_READ_BIT = (1 << 11),
		BARRIER_ACCESS_COPY_WRITE_BIT = (1 << 12),
		BARRIER_ACCESS_HOST_READ_BIT = (1 << 13),
		BARRIER_ACCESS_HOST_WRITE_BIT = (1 << 14),
		BARRIER_ACCESS_MEMORY_READ_BIT = (1 << 15),
		BARRIER_ACCESS_MEMORY_WRITE_BIT = (1 << 16),
		BARRIER_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT = (1 << 23),
		BARRIER_ACCESS_RESOLVE_READ_BIT = (1 << 24),
		BARRIER_ACCESS_RESOLVE_WRITE_BIT = (1 << 25),
		BARRIER_ACCESS_STORAGE_CLEAR_BIT = (1 << 27),
	};

	struct MemoryBarrier {
		BitField<BarrierAccessBits> src_access;
		BitField<BarrierAccessBits> dst_access;
	};

	struct BufferBarrier {
		BufferID buffer;
		BitField<BarrierAccessBits> src_access;
		BitField<BarrierAccessBits> dst_access;
		uint64_t offset = 0;
		uint64_t size = 0;
	};

	struct TextureBarrier {
		TextureID texture;
		BitField<BarrierAccessBits> src_access;
		BitField<BarrierAccessBits> dst_access;
		TextureLayout prev_layout = TEXTURE_LAYOUT_UNDEFINED;
		TextureLayout next_layout = TEXTURE_LAYOUT_UNDEFINED;
		TextureSubresourceRange subresources;
	};

	virtual void command_pipeline_barrier(
			CommandBufferID p_cmd_buffer,
			BitField<PipelineStageBits> p_src_stages,
			BitField<PipelineStageBits> p_dst_stages,
			VectorView<MemoryBarrier> p_memory_barriers,
			VectorView<BufferBarrier> p_buffer_barriers,
			VectorView<TextureBarrier> p_texture_barriers) = 0;

	/****************/
	/**** FENCES ****/
	/****************/

	virtual FenceID fence_create() = 0;
	virtual Error fence_wait(FenceID p_fence) = 0;
	virtual void fence_free(FenceID p_fence) = 0;

	/********************/
	/**** SEMAPHORES ****/
	/********************/

	virtual SemaphoreID semaphore_create() = 0;
	virtual void semaphore_free(SemaphoreID p_semaphore) = 0;

	/*************************/
	/**** COMMAND BUFFERS ****/
	/*************************/

	// ----- QUEUE FAMILY -----

	enum CommandQueueFamilyBits {
		COMMAND_QUEUE_FAMILY_GRAPHICS_BIT = 0x1,
		COMMAND_QUEUE_FAMILY_COMPUTE_BIT = 0x2,
		COMMAND_QUEUE_FAMILY_TRANSFER_BIT = 0x4
	};

	// The requested command queue family must support all specified bits or it'll fail to return a valid family otherwise. If a valid surface is specified, the queue must support presenting to it.
	// It is valid to specify no bits and a valid surface: in this case, the dedicated presentation queue family will be the preferred option.
	virtual CommandQueueFamilyID command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface = 0) = 0;

	// ----- QUEUE -----

	virtual CommandQueueID command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue = false) = 0;
	virtual Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_semaphores, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) = 0;
	virtual void command_queue_free(CommandQueueID p_cmd_queue) = 0;

	// ----- POOL -----

	enum CommandBufferType {
		COMMAND_BUFFER_TYPE_PRIMARY,
		COMMAND_BUFFER_TYPE_SECONDARY,
	};

	virtual CommandPoolID command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) = 0;
	virtual bool command_pool_reset(CommandPoolID p_cmd_pool) = 0;
	virtual void command_pool_free(CommandPoolID p_cmd_pool) = 0;

	// ----- BUFFER -----

	virtual CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool) = 0;
	virtual bool command_buffer_begin(CommandBufferID p_cmd_buffer) = 0;
	virtual bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) = 0;
	virtual void command_buffer_end(CommandBufferID p_cmd_buffer) = 0;
	virtual void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) = 0;

	/********************/
	/**** SWAP CHAIN ****/
	/********************/

	// The swap chain won't be valid for use until it is resized at least once.
	virtual SwapChainID swap_chain_create(RenderingContextDriver::SurfaceID p_surface) = 0;

	// The swap chain must not be in use when a resize is requested. Wait until all rendering associated to the swap chain is finished before resizing it.
	virtual Error swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) = 0;

	// Acquire the framebuffer that can be used for drawing. This must be called only once every time a new frame will be rendered.
	virtual FramebufferID swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) = 0;

	// Retrieve the render pass that can be used to draw on the swap chain's framebuffers.
	virtual RenderPassID swap_chain_get_render_pass(SwapChainID p_swap_chain) = 0;

	// Retrieve the rotation in degrees to apply as a pre-transform. Usually 0 on PC. May be 0, 90, 180 & 270 on Android.
	virtual int swap_chain_get_pre_rotation_degrees(SwapChainID p_swap_chain) { return 0; }

	// Retrieve the format used by the swap chain's framebuffers.
	virtual DataFormat swap_chain_get_format(SwapChainID p_swap_chain) = 0;

	// Tells the swapchain the max_fps so it can use the proper frame pacing.
	// Android uses this with Swappy library. Some implementations or platforms may ignore this hint.
	virtual void swap_chain_set_max_fps(SwapChainID p_swap_chain, int p_max_fps) {}

	// Wait until all rendering associated to the swap chain is finished before deleting it.
	virtual void swap_chain_free(SwapChainID p_swap_chain) = 0;

	/*********************/
	/**** FRAMEBUFFER ****/
	/*********************/

	virtual FramebufferID framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) = 0;
	virtual void framebuffer_free(FramebufferID p_framebuffer) = 0;

	/****************/
	/**** SHADER ****/
	/****************/

	virtual String shader_get_binary_cache_key() = 0;
	virtual Vector<uint8_t> shader_compile_binary_from_spirv(VectorView<ShaderStageSPIRVData> p_spirv, const String &p_shader_name) = 0;

	struct ImmutableSampler {
		UniformType type = UNIFORM_TYPE_MAX;
		uint32_t binding = 0xffffffff; // Binding index as specified in shader.
		LocalVector<ID> ids;
	};
	/** Creates a Pipeline State Object (PSO) out of the shader and all the input data it needs.
	@param p_shader_binary		Shader binary bytecode (e.g. SPIR-V).
	@param r_shader_desc		TBD.
	@param r_name				TBD.
	@param p_immutable_samplers	Immutable samplers can be embedded when creating the pipeline layout on the condition they
								remain valid and unchanged, so they don't need to be specified when creating uniform sets.
	@return						PSO resource for binding.
	*/
	virtual ShaderID shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, ShaderDescription &r_shader_desc, String &r_name, const Vector<ImmutableSampler> &p_immutable_samplers) = 0;
	// Only meaningful if API_TRAIT_SHADER_CHANGE_INVALIDATION is SHADER_CHANGE_INVALIDATION_ALL_OR_NONE_ACCORDING_TO_LAYOUT_HASH.
	virtual uint32_t shader_get_layout_hash(ShaderID p_shader) { return 0; }
	virtual void shader_free(ShaderID p_shader) = 0;
	virtual void shader_destroy_modules(ShaderID p_shader) = 0;

protected:
	// An optional service to implementations.
	Error _reflect_spirv(VectorView<ShaderStageSPIRVData> p_spirv, ShaderReflection &r_reflection);

public:
	/*********************/
	/**** UNIFORM SET ****/
	/*********************/

	struct BoundUniform {
		UniformType type = UNIFORM_TYPE_MAX;
		uint32_t binding = 0xffffffff; // Binding index as specified in shader.
		LocalVector<ID> ids;
		// Flag to indicate  that this is an immutable sampler so it is skipped when creating uniform
		// sets, as it would be set previously when creating the pipeline layout.
		bool immutable_sampler = false;
	};

	virtual UniformSetID uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) = 0;
	virtual void linear_uniform_set_pools_reset(int p_linear_pool_index) {}
	virtual void uniform_set_free(UniformSetID p_uniform_set) = 0;
	virtual bool uniform_sets_have_linear_pools() const { return false; }

	// ----- COMMANDS -----

	virtual void command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) = 0;

	/******************/
	/**** TRANSFER ****/
	/******************/

	struct BufferCopyRegion {
		uint64_t src_offset = 0;
		uint64_t dst_offset = 0;
		uint64_t size = 0;
	};

	struct TextureCopyRegion {
		TextureSubresourceLayers src_subresources;
		Vector3i src_offset;
		TextureSubresourceLayers dst_subresources;
		Vector3i dst_offset;
		Vector3i size;
	};

	struct BufferTextureCopyRegion {
		uint64_t buffer_offset = 0;
		TextureSubresourceLayers texture_subresources;
		Vector3i texture_offset;
		Vector3i texture_region_size;
	};

	virtual void command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) = 0;
	virtual void command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) = 0;

	virtual void command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) = 0;
	virtual void command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) = 0;
	virtual void command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) = 0;

	virtual void command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) = 0;
	virtual void command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) = 0;

	/******************/
	/**** PIPELINE ****/
	/******************/

	virtual void pipeline_free(PipelineID p_pipeline) = 0;

	// ----- BINDING -----

	virtual void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_first_index, VectorView<uint32_t> p_data) = 0;

	// ----- CACHE -----

	virtual bool pipeline_cache_create(const Vector<uint8_t> &p_data) = 0;
	virtual void pipeline_cache_free() = 0;
	virtual size_t pipeline_cache_query_size() = 0;
	virtual Vector<uint8_t> pipeline_cache_serialize() = 0;

	/*******************/
	/**** RENDERING ****/
	/*******************/

	// ----- SUBPASS -----

	enum AttachmentLoadOp {
		ATTACHMENT_LOAD_OP_LOAD = 0,
		ATTACHMENT_LOAD_OP_CLEAR = 1,
		ATTACHMENT_LOAD_OP_DONT_CARE = 2,
	};

	enum AttachmentStoreOp {
		ATTACHMENT_STORE_OP_STORE = 0,
		ATTACHMENT_STORE_OP_DONT_CARE = 1,
	};

	struct Attachment {
		DataFormat format = DATA_FORMAT_MAX;
		TextureSamples samples = TEXTURE_SAMPLES_MAX;
		AttachmentLoadOp load_op = ATTACHMENT_LOAD_OP_DONT_CARE;
		AttachmentStoreOp store_op = ATTACHMENT_STORE_OP_DONT_CARE;
		AttachmentLoadOp stencil_load_op = ATTACHMENT_LOAD_OP_DONT_CARE;
		AttachmentStoreOp stencil_store_op = ATTACHMENT_STORE_OP_DONT_CARE;
		TextureLayout initial_layout = TEXTURE_LAYOUT_UNDEFINED;
		TextureLayout final_layout = TEXTURE_LAYOUT_UNDEFINED;
	};

	struct AttachmentReference {
		static const uint32_t UNUSED = 0xffffffff;
		uint32_t attachment = UNUSED;
		TextureLayout layout = TEXTURE_LAYOUT_UNDEFINED;
		BitField<TextureAspectBits> aspect;
	};

	struct Subpass {
		LocalVector<AttachmentReference> input_references;
		LocalVector<AttachmentReference> color_references;
		AttachmentReference depth_stencil_reference;
		LocalVector<AttachmentReference> resolve_references;
		LocalVector<uint32_t> preserve_attachments;
		AttachmentReference vrs_reference;
	};

	struct SubpassDependency {
		uint32_t src_subpass = 0xffffffff;
		uint32_t dst_subpass = 0xffffffff;
		BitField<PipelineStageBits> src_stages;
		BitField<PipelineStageBits> dst_stages;
		BitField<BarrierAccessBits> src_access;
		BitField<BarrierAccessBits> dst_access;
	};

	virtual RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count) = 0;
	virtual void render_pass_free(RenderPassID p_render_pass) = 0;

	// ----- COMMANDS -----

	union RenderPassClearValue {
		Color color = {};
		struct {
			float depth;
			uint32_t stencil;
		};

		RenderPassClearValue() {}
	};

	struct AttachmentClear {
		BitField<TextureAspectBits> aspect;
		uint32_t color_attachment = 0xffffffff;
		RenderPassClearValue value;
	};

	virtual void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) = 0;
	virtual void command_end_render_pass(CommandBufferID p_cmd_buffer) = 0;
	virtual void command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) = 0;
	virtual void command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) = 0;
	virtual void command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) = 0;
	virtual void command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) = 0;

	// Binding.
	virtual void command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) = 0;
	virtual void command_bind_render_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) = 0;
	virtual void command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) = 0;

	// Drawing.
	virtual void command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) = 0;
	virtual void command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) = 0;
	virtual void command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) = 0;
	virtual void command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) = 0;
	virtual void command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) = 0;
	virtual void command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) = 0;

	// Buffer binding.
	virtual void command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID *p_buffers, const uint64_t *p_offsets) = 0;
	virtual void command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) = 0;

	// Dynamic state.
	virtual void command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) = 0;
	virtual void command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) = 0;

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
			VectorView<PipelineSpecializationConstant> p_specialization_constants) = 0;

	/*****************/
	/**** COMPUTE ****/
	/*****************/

	// ----- COMMANDS -----

	// Binding.
	virtual void command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) = 0;
	virtual void command_bind_compute_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) = 0;
	virtual void command_bind_compute_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) = 0;

	// Dispatching.
	virtual void command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) = 0;
	virtual void command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) = 0;

	// ----- PIPELINE -----

	virtual PipelineID compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) = 0;

	/******************/
	/**** CALLBACK ****/
	/******************/

	typedef void (*DriverCallback)(RenderingDeviceDriver *p_driver, CommandBufferID p_command_buffer, void *p_userdata);

	/*****************/
	/**** QUERIES ****/
	/*****************/

	// ----- TIMESTAMP -----

	// Basic.
	virtual QueryPoolID timestamp_query_pool_create(uint32_t p_query_count) = 0;
	virtual void timestamp_query_pool_free(QueryPoolID p_pool_id) = 0;
	virtual void timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) = 0;
	virtual uint64_t timestamp_query_result_to_time(uint64_t p_result) = 0;

	// Commands.
	virtual void command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) = 0;
	virtual void command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) = 0;

	/****************/
	/**** LABELS ****/
	/****************/

	virtual void command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) = 0;
	virtual void command_end_label(CommandBufferID p_cmd_buffer) = 0;

	/****************/
	/**** DEBUG *****/
	/****************/
	virtual void command_insert_breadcrumb(CommandBufferID p_cmd_buffer, uint32_t p_data) = 0;

	/********************/
	/**** SUBMISSION ****/
	/********************/

	virtual void begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) = 0;
	virtual void end_segment() = 0;

	/**************/
	/**** MISC ****/
	/**************/

	enum ObjectType {
		OBJECT_TYPE_TEXTURE,
		OBJECT_TYPE_SAMPLER,
		OBJECT_TYPE_BUFFER,
		OBJECT_TYPE_SHADER,
		OBJECT_TYPE_UNIFORM_SET,
		OBJECT_TYPE_PIPELINE,
	};

	struct MultiviewCapabilities {
		bool is_supported = false;
		bool geometry_shader_is_supported = false;
		bool tessellation_shader_is_supported = false;
		uint32_t max_view_count = 0;
		uint32_t max_instance_count = 0;
	};

	enum ApiTrait {
		API_TRAIT_HONORS_PIPELINE_BARRIERS,
		API_TRAIT_SHADER_CHANGE_INVALIDATION,
		API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT,
		API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP,
		API_TRAIT_SECONDARY_VIEWPORT_SCISSOR,
		API_TRAIT_CLEARS_WITH_COPY_ENGINE,
		API_TRAIT_USE_GENERAL_IN_COPY_QUEUES,
		API_TRAIT_BUFFERS_REQUIRE_TRANSITIONS,
	};

	enum ShaderChangeInvalidation {
		SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS,
		// What Vulkan does.
		SHADER_CHANGE_INVALIDATION_INCOMPATIBLE_SETS_PLUS_CASCADE,
		// What D3D12 does.
		SHADER_CHANGE_INVALIDATION_ALL_OR_NONE_ACCORDING_TO_LAYOUT_HASH,
	};

	enum DeviceFamily {
		DEVICE_UNKNOWN,
		DEVICE_OPENGL,
		DEVICE_VULKAN,
		DEVICE_DIRECTX,
		DEVICE_METAL,
	};

	struct Capabilities {
		DeviceFamily device_family = DEVICE_UNKNOWN;
		uint32_t version_major = 1;
		uint32_t version_minor = 0;
	};

	virtual void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) = 0;
	virtual uint64_t get_resource_native_handle(DriverResource p_type, ID p_driver_id) = 0;
	virtual uint64_t get_total_memory_used() = 0;
	virtual uint64_t get_lazily_memory_used() = 0;
	virtual uint64_t limit_get(Limit p_limit) = 0;
	virtual uint64_t api_trait_get(ApiTrait p_trait);
	virtual bool has_feature(Features p_feature) = 0;
	virtual const MultiviewCapabilities &get_multiview_capabilities() = 0;
	virtual String get_api_name() const = 0;
	virtual String get_api_version() const = 0;
	virtual String get_pipeline_cache_uuid() const = 0;
	virtual const Capabilities &get_capabilities() const = 0;

	virtual bool is_composite_alpha_supported(CommandQueueID p_queue) const { return false; }

	/******************/

	virtual ~RenderingDeviceDriver();
};

using RDD = RenderingDeviceDriver;

#endif // RENDERING_DEVICE_DRIVER_H
