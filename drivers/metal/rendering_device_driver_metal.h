/**************************************************************************/
/*  rendering_device_driver_metal.h                                       */
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

#ifndef RENDERING_DEVICE_DRIVER_METAL_H
#define RENDERING_DEVICE_DRIVER_METAL_H

#import "metal_objects.h"

#import "servers/rendering/rendering_device_driver.h"

#import <Metal/Metal.h>
#import <spirv.hpp>
#import <variant>

#ifdef DEBUG_ENABLED
#ifndef _DEBUG
#define _DEBUG
#endif
#endif

class RenderingContextDriverMetal;

class API_AVAILABLE(macos(11.0), ios(14.0)) RenderingDeviceDriverMetal : public RenderingDeviceDriver {
	friend struct ShaderCacheEntry;

	template <typename T>
	using Result = std::variant<T, Error>;

#pragma mark - Generic

	RenderingContextDriverMetal *context_driver = nullptr;
	RenderingContextDriver::Device context_device;
	id<MTLDevice> device = nil;

	uint32_t version_major = 2;
	uint32_t version_minor = 0;
	MetalDeviceProperties *device_properties = nullptr;
	PixelFormats *pixel_formats = nullptr;
	std::unique_ptr<MDResourceCache> resource_cache;

	RDD::Capabilities capabilities;
	RDD::MultiviewCapabilities multiview_capabilities;

	id<MTLBinaryArchive> archive = nil;
	uint32_t archive_count = 0;

	id<MTLCommandQueue> device_queue = nil;
	id<MTLCaptureScope> device_scope = nil;

	String pipeline_cache_id;

	Error _create_device();
	Error _check_capabilities();

#pragma mark - Shader Cache

	ShaderLoadStrategy _shader_load_strategy = ShaderLoadStrategy::DEFAULT;

	/**
	 * The shader cache is a map of hashes of the Metal source to shader cache entries.
	 *
	 * To prevent unbounded growth of the cache, cache entries are automatically freed when
	 * there are no more references to the MDLibrary associated with the cache entry.
	 */
	HashMap<SHA256Digest, ShaderCacheEntry *, HashableHasher<SHA256Digest>> _shader_cache;
	void shader_cache_free_entry(const SHA256Digest &key);

public:
	Error initialize(uint32_t p_device_index, uint32_t p_frame_count) override final;

#pragma mark - Memory

#pragma mark - Buffers

public:
	virtual BufferID buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type) override final;
	virtual bool buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) override final;
	virtual void buffer_free(BufferID p_buffer) override final;
	virtual uint64_t buffer_get_allocation_size(BufferID p_buffer) override final;
	virtual uint8_t *buffer_map(BufferID p_buffer) override final;
	virtual void buffer_unmap(BufferID p_buffer) override final;

#pragma mark - Texture

private:
	// Returns true if the texture is a valid linear format.
	Result<bool> is_valid_linear(TextureFormat const &p_format) const;
	void _get_sub_resource(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) const;

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

#pragma mark - Sampler

public:
	virtual SamplerID sampler_create(const SamplerState &p_state) final override;
	virtual void sampler_free(SamplerID p_sampler) final override;
	virtual bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) override final;

#pragma mark - Vertex Array

private:
public:
	virtual VertexFormatID vertex_format_create(VectorView<VertexAttribute> p_vertex_attribs) override final;
	virtual void vertex_format_free(VertexFormatID p_vertex_format) override final;

#pragma mark - Barriers

	virtual void command_pipeline_barrier(
			CommandBufferID p_cmd_buffer,
			BitField<PipelineStageBits> p_src_stages,
			BitField<PipelineStageBits> p_dst_stages,
			VectorView<MemoryBarrier> p_memory_barriers,
			VectorView<BufferBarrier> p_buffer_barriers,
			VectorView<TextureBarrier> p_texture_barriers) override final;

#pragma mark - Fences

private:
	struct Fence {
		dispatch_semaphore_t semaphore;
		Fence() :
				semaphore(dispatch_semaphore_create(0)) {}
	};

public:
	virtual FenceID fence_create() override final;
	virtual Error fence_wait(FenceID p_fence) override final;
	virtual void fence_free(FenceID p_fence) override final;

#pragma mark - Semaphores

public:
	virtual SemaphoreID semaphore_create() override final;
	virtual void semaphore_free(SemaphoreID p_semaphore) override final;

#pragma mark - Commands
	// ----- QUEUE FAMILY -----

	virtual CommandQueueFamilyID command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface = 0) override final;

	// ----- QUEUE -----
public:
	virtual CommandQueueID command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue = false) override final;
	virtual Error command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID> p_wait_semaphores, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID> p_cmd_semaphores, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) override final;
	virtual void command_queue_free(CommandQueueID p_cmd_queue) override final;

	// ----- POOL -----

	virtual CommandPoolID command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) override final;
	virtual bool command_pool_reset(CommandPoolID p_cmd_pool) override final;
	virtual void command_pool_free(CommandPoolID p_cmd_pool) override final;

	// ----- BUFFER -----

private:
	// Used to maintain references.
	Vector<MDCommandBuffer *> command_buffers;

public:
	virtual CommandBufferID command_buffer_create(CommandPoolID p_cmd_pool) override final;
	virtual bool command_buffer_begin(CommandBufferID p_cmd_buffer) override final;
	virtual bool command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) override final;
	virtual void command_buffer_end(CommandBufferID p_cmd_buffer) override final;
	virtual void command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) override final;

#pragma mark - Swapchain

private:
	struct SwapChain {
		RenderingContextDriver::SurfaceID surface = RenderingContextDriver::SurfaceID();
		RenderPassID render_pass;
		RDD::DataFormat data_format = DATA_FORMAT_MAX;
		SwapChain() :
				render_pass(nullptr) {}
	};

	void _swap_chain_release(SwapChain *p_swap_chain);
	void _swap_chain_release_buffers(SwapChain *p_swap_chain);

public:
	virtual SwapChainID swap_chain_create(RenderingContextDriver::SurfaceID p_surface) override final;
	virtual Error swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) override final;
	virtual FramebufferID swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) override final;
	virtual RenderPassID swap_chain_get_render_pass(SwapChainID p_swap_chain) override final;
	virtual DataFormat swap_chain_get_format(SwapChainID p_swap_chain) override final;
	virtual void swap_chain_set_max_fps(SwapChainID p_swap_chain, int p_max_fps) override final;
	virtual void swap_chain_free(SwapChainID p_swap_chain) override final;

#pragma mark - Frame Buffer

	virtual FramebufferID framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) override final;
	virtual void framebuffer_free(FramebufferID p_framebuffer) override final;

#pragma mark - Shader

private:
	// Serialization types need access to private state.

	friend struct ShaderStageData;
	friend struct SpecializationConstantData;
	friend struct UniformData;
	friend struct ShaderBinaryData;
	friend struct PushConstantData;

private:
	/// Contains additional metadata about the shader.
	struct ShaderMeta {
		/// Indicates whether the shader uses multiview.
		bool has_multiview = false;
	};

	Error _reflect_spirv16(VectorView<ShaderStageSPIRVData> p_spirv, ShaderReflection &r_reflection, ShaderMeta &r_shader_meta);

public:
	virtual String shader_get_binary_cache_key() override final;
	virtual Vector<uint8_t> shader_compile_binary_from_spirv(VectorView<ShaderStageSPIRVData> p_spirv, const String &p_shader_name) override final;
	virtual ShaderID shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, ShaderDescription &r_shader_desc, String &r_name, const Vector<ImmutableSampler> &p_immutable_samplers) override final;
	virtual void shader_free(ShaderID p_shader) override final;
	virtual void shader_destroy_modules(ShaderID p_shader) override final;

#pragma mark - Uniform Set

public:
	virtual UniformSetID uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) override final;
	virtual void uniform_set_free(UniformSetID p_uniform_set) override final;

#pragma mark - Commands

	virtual void command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) override final;

#pragma mark Transfer

private:
	enum class CopySource {
		Buffer,
		Texture,
	};
	void _copy_texture_buffer(CommandBufferID p_cmd_buffer,
			CopySource p_source,
			TextureID p_texture,
			BufferID p_buffer,
			VectorView<BufferTextureCopyRegion> p_regions);

public:
	virtual void command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) override final;
	virtual void command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) override final;

	virtual void command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) override final;
	virtual void command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) override final;
	virtual void command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) override final;

	virtual void command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) override final;
	virtual void command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) override final;

#pragma mark Pipeline

private:
	Result<id<MTLFunction>> _create_function(MDLibrary *p_library, NSString *p_name, VectorView<PipelineSpecializationConstant> &p_specialization_constants);

public:
	virtual void pipeline_free(PipelineID p_pipeline_id) override final;

	// ----- BINDING -----

	virtual void command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_first_index, VectorView<uint32_t> p_data) override final;

	// ----- CACHE -----
private:
	String _pipeline_get_cache_path() const;

public:
	virtual bool pipeline_cache_create(const Vector<uint8_t> &p_data) override final;
	virtual void pipeline_cache_free() override final;
	virtual size_t pipeline_cache_query_size() override final;
	virtual Vector<uint8_t> pipeline_cache_serialize() override final;

#pragma mark Rendering

	// ----- SUBPASS -----

	virtual RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count) override final;
	virtual void render_pass_free(RenderPassID p_render_pass) override final;

	// ----- COMMANDS -----

public:
	virtual void command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) override final;
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

#pragma mark - Compute

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

#pragma mark - Queries

	// ----- TIMESTAMP -----

	// Basic.
	virtual QueryPoolID timestamp_query_pool_create(uint32_t p_query_count) override final;
	virtual void timestamp_query_pool_free(QueryPoolID p_pool_id) override final;
	virtual void timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) override final;
	virtual uint64_t timestamp_query_result_to_time(uint64_t p_result) override final;

	// Commands.
	virtual void command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) override final;
	virtual void command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) override final;

#pragma mark - Labels

	virtual void command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) override final;
	virtual void command_end_label(CommandBufferID p_cmd_buffer) override final;

#pragma mark - Debug

	virtual void command_insert_breadcrumb(CommandBufferID p_cmd_buffer, uint32_t p_data) override final;

#pragma mark - Submission

	virtual void begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) override final;
	virtual void end_segment() override final;

#pragma mark - Miscellaneous

	virtual void set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) override final;
	virtual uint64_t get_resource_native_handle(DriverResource p_type, ID p_driver_id) override final;
	virtual uint64_t get_total_memory_used() override final;
	virtual uint64_t get_lazily_memory_used() override final;
	virtual uint64_t limit_get(Limit p_limit) override final;
	virtual uint64_t api_trait_get(ApiTrait p_trait) override final;
	virtual bool has_feature(Features p_feature) override final;
	virtual const MultiviewCapabilities &get_multiview_capabilities() override final;
	virtual String get_api_name() const override final { return "Metal"; }
	virtual String get_api_version() const override final;
	virtual String get_pipeline_cache_uuid() const override final;
	virtual const Capabilities &get_capabilities() const override final;
	virtual bool is_composite_alpha_supported(CommandQueueID p_queue) const override final;

	// Metal-specific.
	id<MTLDevice> get_device() const { return device; }
	PixelFormats &get_pixel_formats() const { return *pixel_formats; }
	MDResourceCache &get_resource_cache() const { return *resource_cache; }
	MetalDeviceProperties const &get_device_properties() const { return *device_properties; }

	_FORCE_INLINE_ uint32_t get_metal_buffer_index_for_vertex_attribute_binding(uint32_t p_binding) {
		return (device_properties->limits.maxPerStageBufferCount - 1) - p_binding;
	}

	size_t get_texel_buffer_alignment_for_format(RDD::DataFormat p_format) const;
	size_t get_texel_buffer_alignment_for_format(MTLPixelFormat p_format) const;

	/******************/
	RenderingDeviceDriverMetal(RenderingContextDriverMetal *p_context_driver);
	~RenderingDeviceDriverMetal();
};

#endif // RENDERING_DEVICE_DRIVER_METAL_H
