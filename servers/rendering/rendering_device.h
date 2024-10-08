/**************************************************************************/
/*  rendering_device.h                                                    */
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

#ifndef RENDERING_DEVICE_H
#define RENDERING_DEVICE_H

#include "core/object/class_db.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/condition_variable.h"
#include "core/os/thread_safe.h"
#include "core/templates/local_vector.h"
#include "core/templates/oa_hash_map.h"
#include "core/templates/rid_owner.h"
#include "core/variant/typed_array.h"
#include "servers/display_server.h"
#include "servers/rendering/rendering_device_commons.h"
#include "servers/rendering/rendering_device_driver.h"
#include "servers/rendering/rendering_device_graph.h"

class RDTextureFormat;
class RDTextureView;
class RDAttachmentFormat;
class RDSamplerState;
class RDVertexAttribute;
class RDShaderSource;
class RDShaderSPIRV;
class RDUniform;
class RDPipelineRasterizationState;
class RDPipelineMultisampleState;
class RDPipelineDepthStencilState;
class RDPipelineColorBlendState;
class RDFramebufferPass;
class RDPipelineSpecializationConstant;

class RenderingDevice : public RenderingDeviceCommons {
	GDCLASS(RenderingDevice, Object)

	_THREAD_SAFE_CLASS_

private:
	Thread::ID render_thread_id;

public:
	enum ShaderLanguage {
		SHADER_LANGUAGE_GLSL,
		SHADER_LANGUAGE_HLSL
	};

	typedef int64_t DrawListID;
	typedef int64_t ComputeListID;

	typedef String (*ShaderSPIRVGetCacheKeyFunction)(const RenderingDevice *p_render_device);
	typedef Vector<uint8_t> (*ShaderCompileToSPIRVFunction)(ShaderStage p_stage, const String &p_source_code, ShaderLanguage p_language, String *r_error, const RenderingDevice *p_render_device);
	typedef Vector<uint8_t> (*ShaderCacheFunction)(ShaderStage p_stage, const String &p_source_code, ShaderLanguage p_language);

	typedef void (*InvalidationCallback)(void *);

private:
	static ShaderCompileToSPIRVFunction compile_to_spirv_function;
	static ShaderCacheFunction cache_function;
	static ShaderSPIRVGetCacheKeyFunction get_spirv_cache_key_function;

	static RenderingDevice *singleton;

	RenderingContextDriver *context = nullptr;
	RenderingDeviceDriver *driver = nullptr;
	RenderingContextDriver::Device device;

	bool local_device_processing = false;
	bool is_main_instance = false;

protected:
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	RID _shader_create_from_bytecode_bind_compat_79606(const Vector<uint8_t> &p_shader_binary);
	static void _bind_compatibility_methods();
#endif

	/***************************/
	/**** ID INFRASTRUCTURE ****/
	/***************************/
public:
	//base numeric ID for all types
	enum {
		INVALID_FORMAT_ID = -1
	};

	enum IDType {
		ID_TYPE_FRAMEBUFFER_FORMAT,
		ID_TYPE_VERTEX_FORMAT,
		ID_TYPE_DRAW_LIST,
		ID_TYPE_COMPUTE_LIST = 4,
		ID_TYPE_MAX,
		ID_BASE_SHIFT = 58, // 5 bits for ID types.
		ID_MASK = (ID_BASE_SHIFT - 1),
	};

private:
	HashMap<RID, HashSet<RID>> dependency_map; // IDs to IDs that depend on it.
	HashMap<RID, HashSet<RID>> reverse_dependency_map; // Same as above, but in reverse.

	void _add_dependency(RID p_id, RID p_depends_on);
	void _free_dependencies(RID p_id);

private:
	/***************************/
	/**** BUFFER MANAGEMENT ****/
	/***************************/

	// These are temporary buffers on CPU memory that hold
	// the information until the CPU fetches it and places it
	// either on GPU buffers, or images (textures). It ensures
	// updates are properly synchronized with whatever the
	// GPU is doing.
	//
	// The logic here is as follows, only 3 of these
	// blocks are created at the beginning (one per frame)
	// they can each belong to a frame (assigned to current when
	// used) and they can only be reused after the same frame is
	// recycled.
	//
	// When CPU requires to allocate more than what is available,
	// more of these buffers are created. If a limit is reached,
	// then a fence will ensure will wait for blocks allocated
	// in previous frames are processed. If that fails, then
	// another fence will ensure everything pending for the current
	// frame is processed (effectively stalling).
	//
	// See the comments in the code to understand better how it works.

	struct StagingBufferBlock {
		RDD::BufferID driver_id;
		uint64_t frame_used = 0;
		uint32_t fill_amount = 0;
	};

	Vector<StagingBufferBlock> staging_buffer_blocks;
	int staging_buffer_current = 0;
	uint32_t staging_buffer_block_size = 0;
	uint64_t staging_buffer_max_size = 0;
	bool staging_buffer_used = false;

	enum StagingRequiredAction {
		STAGING_REQUIRED_ACTION_NONE,
		STAGING_REQUIRED_ACTION_FLUSH_AND_STALL_ALL,
		STAGING_REQUIRED_ACTION_STALL_PREVIOUS
	};

	Error _staging_buffer_allocate(uint32_t p_amount, uint32_t p_required_align, uint32_t &r_alloc_offset, uint32_t &r_alloc_size, StagingRequiredAction &r_required_action, bool p_can_segment = true);
	void _staging_buffer_execute_required_action(StagingRequiredAction p_required_action);
	Error _insert_staging_block();

	struct Buffer {
		RDD::BufferID driver_id;
		uint32_t size = 0;
		BitField<RDD::BufferUsageBits> usage;
		RDG::ResourceTracker *draw_tracker = nullptr;
		int32_t transfer_worker_index = -1;
		uint64_t transfer_worker_operation = 0;
	};

	Buffer *_get_buffer_from_owner(RID p_buffer);
	Error _buffer_initialize(Buffer *p_buffer, const uint8_t *p_data, size_t p_data_size, uint32_t p_required_align = 32);

	void update_perf_report();

	uint32_t gpu_copy_count = 0;
	uint32_t copy_bytes_count = 0;
	String perf_report_text;

	RID_Owner<Buffer, true> uniform_buffer_owner;
	RID_Owner<Buffer, true> storage_buffer_owner;
	RID_Owner<Buffer, true> texture_buffer_owner;

public:
	Error buffer_copy(RID p_src_buffer, RID p_dst_buffer, uint32_t p_src_offset, uint32_t p_dst_offset, uint32_t p_size);
	Error buffer_update(RID p_buffer, uint32_t p_offset, uint32_t p_size, const void *p_data);
	Error buffer_clear(RID p_buffer, uint32_t p_offset, uint32_t p_size);
	Vector<uint8_t> buffer_get_data(RID p_buffer, uint32_t p_offset = 0, uint32_t p_size = 0); // This causes stall, only use to retrieve large buffers for saving.

	/*****************/
	/**** TEXTURE ****/
	/*****************/

	// In modern APIs, the concept of textures may not exist;
	// instead there is the image (the memory pretty much,
	// the view (how the memory is interpreted) and the
	// sampler (how it's sampled from the shader).
	//
	// Texture here includes the first two stages, but
	// It's possible to create textures sharing the image
	// but with different views. The main use case for this
	// is textures that can be read as both SRGB/Linear,
	// or slices of a texture (a mipmap, a layer, a 3D slice)
	// for a framebuffer to render into it.

	struct Texture {
		struct SharedFallback {
			uint32_t revision = 1;
			RDD::TextureID texture;
			RDG::ResourceTracker *texture_tracker = nullptr;
			RDD::BufferID buffer;
			RDG::ResourceTracker *buffer_tracker = nullptr;
			bool raw_reinterpretation = false;
		};

		RDD::TextureID driver_id;

		TextureType type = TEXTURE_TYPE_MAX;
		DataFormat format = DATA_FORMAT_MAX;
		TextureSamples samples = TEXTURE_SAMPLES_MAX;
		TextureSliceType slice_type = TEXTURE_SLICE_MAX;
		Rect2i slice_rect;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t depth = 0;
		uint32_t layers = 0;
		uint32_t mipmaps = 0;
		uint32_t usage_flags = 0;
		uint32_t base_mipmap = 0;
		uint32_t base_layer = 0;

		Vector<DataFormat> allowed_shared_formats;

		bool is_resolve_buffer = false;
		bool has_initial_data = false;

		BitField<RDD::TextureAspectBits> read_aspect_flags;
		BitField<RDD::TextureAspectBits> barrier_aspect_flags;
		bool bound = false; // Bound to framebuffer.
		RID owner;

		RDG::ResourceTracker *draw_tracker = nullptr;
		HashMap<Rect2i, RDG::ResourceTracker *> slice_trackers;
		SharedFallback *shared_fallback = nullptr;
		int32_t transfer_worker_index = -1;
		uint64_t transfer_worker_operation = 0;

		RDD::TextureSubresourceRange barrier_range() const {
			RDD::TextureSubresourceRange r;
			r.aspect = barrier_aspect_flags;
			r.base_mipmap = base_mipmap;
			r.mipmap_count = mipmaps;
			r.base_layer = base_layer;
			r.layer_count = layers;
			return r;
		}

		TextureFormat texture_format() const {
			TextureFormat tf;
			tf.format = format;
			tf.width = width;
			tf.height = height;
			tf.depth = depth;
			tf.array_layers = layers;
			tf.mipmaps = mipmaps;
			tf.texture_type = type;
			tf.samples = samples;
			tf.usage_bits = usage_flags;
			tf.shareable_formats = allowed_shared_formats;
			tf.is_resolve_buffer = is_resolve_buffer;
			return tf;
		}
	};

	RID_Owner<Texture, true> texture_owner;
	uint32_t texture_upload_region_size_px = 0;

	Vector<uint8_t> _texture_get_data(Texture *tex, uint32_t p_layer, bool p_2d = false);
	uint32_t _texture_layer_count(Texture *p_texture) const;
	uint32_t _texture_alignment(Texture *p_texture) const;
	Error _texture_initialize(RID p_texture, uint32_t p_layer, const Vector<uint8_t> &p_data);
	void _texture_check_shared_fallback(Texture *p_texture);
	void _texture_update_shared_fallback(RID p_texture_rid, Texture *p_texture, bool p_for_writing);
	void _texture_free_shared_fallback(Texture *p_texture);
	void _texture_copy_shared(RID p_src_texture_rid, Texture *p_src_texture, RID p_dst_texture_rid, Texture *p_dst_texture);
	void _texture_create_reinterpret_buffer(Texture *p_texture);

public:
	struct TextureView {
		DataFormat format_override = DATA_FORMAT_MAX; // // Means, use same as format.
		TextureSwizzle swizzle_r = TEXTURE_SWIZZLE_R;
		TextureSwizzle swizzle_g = TEXTURE_SWIZZLE_G;
		TextureSwizzle swizzle_b = TEXTURE_SWIZZLE_B;
		TextureSwizzle swizzle_a = TEXTURE_SWIZZLE_A;

		bool operator==(const TextureView &p_other) const {
			if (format_override != p_other.format_override) {
				return false;
			} else if (swizzle_r != p_other.swizzle_r) {
				return false;
			} else if (swizzle_g != p_other.swizzle_g) {
				return false;
			} else if (swizzle_b != p_other.swizzle_b) {
				return false;
			} else if (swizzle_a != p_other.swizzle_a) {
				return false;
			} else {
				return true;
			}
		}
	};

	RID texture_create(const TextureFormat &p_format, const TextureView &p_view, const Vector<Vector<uint8_t>> &p_data = Vector<Vector<uint8_t>>());
	RID texture_create_shared(const TextureView &p_view, RID p_with_texture);
	RID texture_create_from_extension(TextureType p_type, DataFormat p_format, TextureSamples p_samples, BitField<RenderingDevice::TextureUsageBits> p_usage, uint64_t p_image, uint64_t p_width, uint64_t p_height, uint64_t p_depth, uint64_t p_layers);
	RID texture_create_shared_from_slice(const TextureView &p_view, RID p_with_texture, uint32_t p_layer, uint32_t p_mipmap, uint32_t p_mipmaps = 1, TextureSliceType p_slice_type = TEXTURE_SLICE_2D, uint32_t p_layers = 0);
	Error texture_update(RID p_texture, uint32_t p_layer, const Vector<uint8_t> &p_data);
	Vector<uint8_t> texture_get_data(RID p_texture, uint32_t p_layer); // CPU textures will return immediately, while GPU textures will most likely force a flush

	bool texture_is_format_supported_for_usage(DataFormat p_format, BitField<TextureUsageBits> p_usage) const;
	bool texture_is_shared(RID p_texture);
	bool texture_is_valid(RID p_texture);
	TextureFormat texture_get_format(RID p_texture);
	Size2i texture_size(RID p_texture);
#ifndef DISABLE_DEPRECATED
	uint64_t texture_get_native_handle(RID p_texture);
#endif

	Error texture_copy(RID p_from_texture, RID p_to_texture, const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_size, uint32_t p_src_mipmap, uint32_t p_dst_mipmap, uint32_t p_src_layer, uint32_t p_dst_layer);
	Error texture_clear(RID p_texture, const Color &p_color, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers);
	Error texture_resolve_multisample(RID p_from_texture, RID p_to_texture);

	/************************/
	/**** DRAW LISTS (I) ****/
	/************************/

	enum InitialAction {
		INITIAL_ACTION_LOAD,
		INITIAL_ACTION_CLEAR,
		INITIAL_ACTION_DISCARD,
		INITIAL_ACTION_MAX,
#ifndef DISABLE_DEPRECATED
		INITIAL_ACTION_CLEAR_REGION = INITIAL_ACTION_CLEAR,
		INITIAL_ACTION_CLEAR_REGION_CONTINUE = INITIAL_ACTION_CLEAR,
		INITIAL_ACTION_KEEP = INITIAL_ACTION_LOAD,
		INITIAL_ACTION_DROP = INITIAL_ACTION_DISCARD,
		INITIAL_ACTION_CONTINUE = INITIAL_ACTION_LOAD,
#endif
	};

	enum FinalAction {
		FINAL_ACTION_STORE,
		FINAL_ACTION_DISCARD,
		FINAL_ACTION_MAX,
#ifndef DISABLE_DEPRECATED
		FINAL_ACTION_READ = FINAL_ACTION_STORE,
		FINAL_ACTION_CONTINUE = FINAL_ACTION_STORE,
#endif
	};

	/*********************/
	/**** FRAMEBUFFER ****/
	/*********************/

	// In modern APIs, generally, framebuffers work similar to how they
	// do in OpenGL, with the exception that
	// the "format" (RDD::RenderPassID) is not dynamic
	// and must be more or less the same as the one
	// used for the render pipelines.

	struct AttachmentFormat {
		enum : uint32_t {
			UNUSED_ATTACHMENT = 0xFFFFFFFF
		};
		DataFormat format;
		TextureSamples samples;
		uint32_t usage_flags;
		AttachmentFormat() {
			format = DATA_FORMAT_R8G8B8A8_UNORM;
			samples = TEXTURE_SAMPLES_1;
			usage_flags = 0;
		}
	};

	struct FramebufferPass {
		Vector<int32_t> color_attachments;
		Vector<int32_t> input_attachments;
		Vector<int32_t> resolve_attachments;
		Vector<int32_t> preserve_attachments;
		int32_t depth_attachment = ATTACHMENT_UNUSED;
		int32_t vrs_attachment = ATTACHMENT_UNUSED; // density map for VRS, only used if supported
	};

	typedef int64_t FramebufferFormatID;

private:
	struct FramebufferFormatKey {
		Vector<AttachmentFormat> attachments;
		Vector<FramebufferPass> passes;
		uint32_t view_count = 1;

		bool operator<(const FramebufferFormatKey &p_key) const {
			if (view_count != p_key.view_count) {
				return view_count < p_key.view_count;
			}

			uint32_t pass_size = passes.size();
			uint32_t key_pass_size = p_key.passes.size();
			if (pass_size != key_pass_size) {
				return pass_size < key_pass_size;
			}
			const FramebufferPass *pass_ptr = passes.ptr();
			const FramebufferPass *key_pass_ptr = p_key.passes.ptr();

			for (uint32_t i = 0; i < pass_size; i++) {
				{ // Compare color attachments.
					uint32_t attachment_size = pass_ptr[i].color_attachments.size();
					uint32_t key_attachment_size = key_pass_ptr[i].color_attachments.size();
					if (attachment_size != key_attachment_size) {
						return attachment_size < key_attachment_size;
					}
					const int32_t *pass_attachment_ptr = pass_ptr[i].color_attachments.ptr();
					const int32_t *key_pass_attachment_ptr = key_pass_ptr[i].color_attachments.ptr();

					for (uint32_t j = 0; j < attachment_size; j++) {
						if (pass_attachment_ptr[j] != key_pass_attachment_ptr[j]) {
							return pass_attachment_ptr[j] < key_pass_attachment_ptr[j];
						}
					}
				}
				{ // Compare input attachments.
					uint32_t attachment_size = pass_ptr[i].input_attachments.size();
					uint32_t key_attachment_size = key_pass_ptr[i].input_attachments.size();
					if (attachment_size != key_attachment_size) {
						return attachment_size < key_attachment_size;
					}
					const int32_t *pass_attachment_ptr = pass_ptr[i].input_attachments.ptr();
					const int32_t *key_pass_attachment_ptr = key_pass_ptr[i].input_attachments.ptr();

					for (uint32_t j = 0; j < attachment_size; j++) {
						if (pass_attachment_ptr[j] != key_pass_attachment_ptr[j]) {
							return pass_attachment_ptr[j] < key_pass_attachment_ptr[j];
						}
					}
				}
				{ // Compare resolve attachments.
					uint32_t attachment_size = pass_ptr[i].resolve_attachments.size();
					uint32_t key_attachment_size = key_pass_ptr[i].resolve_attachments.size();
					if (attachment_size != key_attachment_size) {
						return attachment_size < key_attachment_size;
					}
					const int32_t *pass_attachment_ptr = pass_ptr[i].resolve_attachments.ptr();
					const int32_t *key_pass_attachment_ptr = key_pass_ptr[i].resolve_attachments.ptr();

					for (uint32_t j = 0; j < attachment_size; j++) {
						if (pass_attachment_ptr[j] != key_pass_attachment_ptr[j]) {
							return pass_attachment_ptr[j] < key_pass_attachment_ptr[j];
						}
					}
				}
				{ // Compare preserve attachments.
					uint32_t attachment_size = pass_ptr[i].preserve_attachments.size();
					uint32_t key_attachment_size = key_pass_ptr[i].preserve_attachments.size();
					if (attachment_size != key_attachment_size) {
						return attachment_size < key_attachment_size;
					}
					const int32_t *pass_attachment_ptr = pass_ptr[i].preserve_attachments.ptr();
					const int32_t *key_pass_attachment_ptr = key_pass_ptr[i].preserve_attachments.ptr();

					for (uint32_t j = 0; j < attachment_size; j++) {
						if (pass_attachment_ptr[j] != key_pass_attachment_ptr[j]) {
							return pass_attachment_ptr[j] < key_pass_attachment_ptr[j];
						}
					}
				}
				if (pass_ptr[i].depth_attachment != key_pass_ptr[i].depth_attachment) {
					return pass_ptr[i].depth_attachment < key_pass_ptr[i].depth_attachment;
				}
			}

			int as = attachments.size();
			int bs = p_key.attachments.size();
			if (as != bs) {
				return as < bs;
			}

			const AttachmentFormat *af_a = attachments.ptr();
			const AttachmentFormat *af_b = p_key.attachments.ptr();
			for (int i = 0; i < as; i++) {
				const AttachmentFormat &a = af_a[i];
				const AttachmentFormat &b = af_b[i];
				if (a.format != b.format) {
					return a.format < b.format;
				}
				if (a.samples != b.samples) {
					return a.samples < b.samples;
				}
				if (a.usage_flags != b.usage_flags) {
					return a.usage_flags < b.usage_flags;
				}
			}

			return false; // Equal.
		}
	};

	RDD::RenderPassID _render_pass_create(const Vector<AttachmentFormat> &p_attachments, const Vector<FramebufferPass> &p_passes, InitialAction p_initial_action, FinalAction p_final_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, uint32_t p_view_count = 1, Vector<TextureSamples> *r_samples = nullptr);

	// This is a cache and it's never freed, it ensures
	// IDs for a given format are always unique.
	RBMap<FramebufferFormatKey, FramebufferFormatID> framebuffer_format_cache;
	struct FramebufferFormat {
		const RBMap<FramebufferFormatKey, FramebufferFormatID>::Element *E;
		RDD::RenderPassID render_pass; // Here for constructing shaders, never used, see section (7.2. Render Pass Compatibility from Vulkan spec).
		Vector<TextureSamples> pass_samples;
		uint32_t view_count = 1; // Number of views.
	};

	HashMap<FramebufferFormatID, FramebufferFormat> framebuffer_formats;

	struct Framebuffer {
		FramebufferFormatID format_id;
		struct VersionKey {
			InitialAction initial_color_action;
			FinalAction final_color_action;
			InitialAction initial_depth_action;
			FinalAction final_depth_action;
			uint32_t view_count;

			bool operator<(const VersionKey &p_key) const {
				if (initial_color_action == p_key.initial_color_action) {
					if (final_color_action == p_key.final_color_action) {
						if (initial_depth_action == p_key.initial_depth_action) {
							if (final_depth_action == p_key.final_depth_action) {
								return view_count < p_key.view_count;
							} else {
								return final_depth_action < p_key.final_depth_action;
							}
						} else {
							return initial_depth_action < p_key.initial_depth_action;
						}
					} else {
						return final_color_action < p_key.final_color_action;
					}
				} else {
					return initial_color_action < p_key.initial_color_action;
				}
			}
		};

		uint32_t storage_mask = 0;
		Vector<RID> texture_ids;
		InvalidationCallback invalidated_callback = nullptr;
		void *invalidated_callback_userdata = nullptr;

		struct Version {
			RDD::FramebufferID framebuffer;
			RDD::RenderPassID render_pass; // This one is owned.
			uint32_t subpass_count = 1;
		};

		RBMap<VersionKey, Version> framebuffers;
		Size2 size;
		uint32_t view_count;
	};

	RID_Owner<Framebuffer, true> framebuffer_owner;

public:
	// This ID is warranted to be unique for the same formats, does not need to be freed
	FramebufferFormatID framebuffer_format_create(const Vector<AttachmentFormat> &p_format, uint32_t p_view_count = 1);
	FramebufferFormatID framebuffer_format_create_multipass(const Vector<AttachmentFormat> &p_attachments, const Vector<FramebufferPass> &p_passes, uint32_t p_view_count = 1);
	FramebufferFormatID framebuffer_format_create_empty(TextureSamples p_samples = TEXTURE_SAMPLES_1);
	TextureSamples framebuffer_format_get_texture_samples(FramebufferFormatID p_format, uint32_t p_pass = 0);

	RID framebuffer_create(const Vector<RID> &p_texture_attachments, FramebufferFormatID p_format_check = INVALID_ID, uint32_t p_view_count = 1);
	RID framebuffer_create_multipass(const Vector<RID> &p_texture_attachments, const Vector<FramebufferPass> &p_passes, FramebufferFormatID p_format_check = INVALID_ID, uint32_t p_view_count = 1);
	RID framebuffer_create_empty(const Size2i &p_size, TextureSamples p_samples = TEXTURE_SAMPLES_1, FramebufferFormatID p_format_check = INVALID_ID);
	bool framebuffer_is_valid(RID p_framebuffer) const;
	void framebuffer_set_invalidation_callback(RID p_framebuffer, InvalidationCallback p_callback, void *p_userdata);

	FramebufferFormatID framebuffer_get_format(RID p_framebuffer);

	/*****************/
	/**** SAMPLER ****/
	/*****************/
private:
	RID_Owner<RDD::SamplerID, true> sampler_owner;

public:
	RID sampler_create(const SamplerState &p_state);
	bool sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_sampler_filter) const;

	/**********************/
	/**** VERTEX ARRAY ****/
	/**********************/

	typedef int64_t VertexFormatID;

private:
	// Vertex buffers in Vulkan are similar to how
	// they work in OpenGL, except that instead of
	// an attribute index, there is a buffer binding
	// index (for binding the buffers in real-time)
	// and a location index (what is used in the shader).
	//
	// This mapping is done here internally, and it's not
	// exposed.

	RID_Owner<Buffer, true> vertex_buffer_owner;

	struct VertexDescriptionKey {
		Vector<VertexAttribute> vertex_formats;

		bool operator==(const VertexDescriptionKey &p_key) const {
			int vdc = vertex_formats.size();
			int vdck = p_key.vertex_formats.size();

			if (vdc != vdck) {
				return false;
			} else {
				const VertexAttribute *a_ptr = vertex_formats.ptr();
				const VertexAttribute *b_ptr = p_key.vertex_formats.ptr();
				for (int i = 0; i < vdc; i++) {
					const VertexAttribute &a = a_ptr[i];
					const VertexAttribute &b = b_ptr[i];

					if (a.location != b.location) {
						return false;
					}
					if (a.offset != b.offset) {
						return false;
					}
					if (a.format != b.format) {
						return false;
					}
					if (a.stride != b.stride) {
						return false;
					}
					if (a.frequency != b.frequency) {
						return false;
					}
				}
				return true; // They are equal.
			}
		}

		uint32_t hash() const {
			int vdc = vertex_formats.size();
			uint32_t h = hash_murmur3_one_32(vdc);
			const VertexAttribute *ptr = vertex_formats.ptr();
			for (int i = 0; i < vdc; i++) {
				const VertexAttribute &vd = ptr[i];
				h = hash_murmur3_one_32(vd.location, h);
				h = hash_murmur3_one_32(vd.offset, h);
				h = hash_murmur3_one_32(vd.format, h);
				h = hash_murmur3_one_32(vd.stride, h);
				h = hash_murmur3_one_32(vd.frequency, h);
			}
			return hash_fmix32(h);
		}
	};

	struct VertexDescriptionHash {
		static _FORCE_INLINE_ uint32_t hash(const VertexDescriptionKey &p_key) {
			return p_key.hash();
		}
	};

	// This is a cache and it's never freed, it ensures that
	// ID used for a specific format always remain the same.
	HashMap<VertexDescriptionKey, VertexFormatID, VertexDescriptionHash> vertex_format_cache;

	struct VertexDescriptionCache {
		Vector<VertexAttribute> vertex_formats;
		RDD::VertexFormatID driver_id;
	};

	HashMap<VertexFormatID, VertexDescriptionCache> vertex_formats;

	struct VertexArray {
		RID buffer;
		VertexFormatID description;
		int vertex_count = 0;
		uint32_t max_instances_allowed = 0;

		Vector<RDD::BufferID> buffers; // Not owned, just referenced.
		Vector<RDG::ResourceTracker *> draw_trackers; // Not owned, just referenced.
		Vector<uint64_t> offsets;
		Vector<int32_t> transfer_worker_indices;
		Vector<uint64_t> transfer_worker_operations;
		HashSet<RID> untracked_buffers;
	};

	RID_Owner<VertexArray, true> vertex_array_owner;

	struct IndexBuffer : public Buffer {
		uint32_t max_index = 0; // Used for validation.
		uint32_t index_count = 0;
		IndexBufferFormat format = INDEX_BUFFER_FORMAT_UINT16;
		bool supports_restart_indices = false;
	};

	RID_Owner<IndexBuffer, true> index_buffer_owner;

	struct IndexArray {
		uint32_t max_index = 0; // Remember the maximum index here too, for validation.
		RDD::BufferID driver_id; // Not owned, inherited from index buffer.
		RDG::ResourceTracker *draw_tracker = nullptr; // Not owned, inherited from index buffer.
		uint32_t offset = 0;
		uint32_t indices = 0;
		IndexBufferFormat format = INDEX_BUFFER_FORMAT_UINT16;
		bool supports_restart_indices = false;
		int32_t transfer_worker_index = -1;
		uint64_t transfer_worker_operation = 0;
	};

	RID_Owner<IndexArray, true> index_array_owner;

public:
	RID vertex_buffer_create(uint32_t p_size_bytes, const Vector<uint8_t> &p_data = Vector<uint8_t>(), bool p_use_as_storage = false);

	// This ID is warranted to be unique for the same formats, does not need to be freed
	VertexFormatID vertex_format_create(const Vector<VertexAttribute> &p_vertex_descriptions);
	RID vertex_array_create(uint32_t p_vertex_count, VertexFormatID p_vertex_format, const Vector<RID> &p_src_buffers, const Vector<uint64_t> &p_offsets = Vector<uint64_t>());

	RID index_buffer_create(uint32_t p_size_indices, IndexBufferFormat p_format, const Vector<uint8_t> &p_data = Vector<uint8_t>(), bool p_use_restart_indices = false);
	RID index_array_create(RID p_index_buffer, uint32_t p_index_offset, uint32_t p_index_count);

	/****************/
	/**** SHADER ****/
	/****************/

	// Some APIs (e.g., Vulkan) specifies a really complex behavior for the application
	// in order to tell when descriptor sets need to be re-bound (or not).
	// "When binding a descriptor set (see Descriptor Set Binding) to set
	//  number N, if the previously bound descriptor sets for sets zero
	//  through N-1 were all bound using compatible pipeline layouts,
	//  then performing this binding does not disturb any of the lower numbered sets.
	//  If, additionally, the previous bound descriptor set for set N was
	//  bound using a pipeline layout compatible for set N, then the bindings
	//  in sets numbered greater than N are also not disturbed."
	// As a result, we need to figure out quickly when something is no longer "compatible".
	// in order to avoid costly rebinds.

private:
	struct UniformSetFormat {
		Vector<ShaderUniform> uniforms;

		_FORCE_INLINE_ bool operator<(const UniformSetFormat &p_other) const {
			if (uniforms.size() != p_other.uniforms.size()) {
				return uniforms.size() < p_other.uniforms.size();
			}
			for (int i = 0; i < uniforms.size(); i++) {
				if (uniforms[i] < p_other.uniforms[i]) {
					return true;
				} else if (p_other.uniforms[i] < uniforms[i]) {
					return false;
				}
			}
			return false;
		}
	};

	// Always grows, never shrinks, ensuring unique IDs, but we assume
	// the amount of formats will never be a problem, as the amount of shaders
	// in a game is limited.
	RBMap<UniformSetFormat, uint32_t> uniform_set_format_cache;

	// Shaders in Vulkan are just pretty much
	// precompiled blocks of SPIR-V bytecode. They
	// are most likely not really compiled to host
	// assembly until a pipeline is created.
	//
	// When supplying the shaders, this implementation
	// will use the reflection abilities of glslang to
	// understand and cache everything required to
	// create and use the descriptor sets (Vulkan's
	// biggest pain).
	//
	// Additionally, hashes are created for every set
	// to do quick validation and ensuring the user
	// does not submit something invalid.

	struct Shader : public ShaderDescription {
		String name; // Used for debug.
		RDD::ShaderID driver_id;
		uint32_t layout_hash = 0;
		BitField<RDD::PipelineStageBits> stage_bits;
		Vector<uint32_t> set_formats;
	};

	String _shader_uniform_debug(RID p_shader, int p_set = -1);

	RID_Owner<Shader, true> shader_owner;

#ifndef DISABLE_DEPRECATED
public:
	enum BarrierMask {
		BARRIER_MASK_VERTEX = 1,
		BARRIER_MASK_FRAGMENT = 8,
		BARRIER_MASK_COMPUTE = 2,
		BARRIER_MASK_TRANSFER = 4,

		BARRIER_MASK_RASTER = BARRIER_MASK_VERTEX | BARRIER_MASK_FRAGMENT, // 9,
		BARRIER_MASK_ALL_BARRIERS = 0x7FFF, // all flags set
		BARRIER_MASK_NO_BARRIER = 0x8000,
	};

	void barrier(BitField<BarrierMask> p_from = BARRIER_MASK_ALL_BARRIERS, BitField<BarrierMask> p_to = BARRIER_MASK_ALL_BARRIERS);
	void full_barrier();
	void draw_command_insert_label(String p_label_name, const Color &p_color = Color(1, 1, 1, 1));
	Error draw_list_begin_split(RID p_framebuffer, uint32_t p_splits, DrawListID *r_split_ids, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values = Vector<Color>(), float p_clear_depth = 1.0, uint32_t p_clear_stencil = 0, const Rect2 &p_region = Rect2(), const Vector<RID> &p_storage_textures = Vector<RID>());
	Error draw_list_switch_to_next_pass_split(uint32_t p_splits, DrawListID *r_split_ids);
	Vector<int64_t> _draw_list_begin_split(RID p_framebuffer, uint32_t p_splits, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values = Vector<Color>(), float p_clear_depth = 1.0, uint32_t p_clear_stencil = 0, const Rect2 &p_region = Rect2(), const TypedArray<RID> &p_storage_textures = TypedArray<RID>());
	Vector<int64_t> _draw_list_switch_to_next_pass_split(uint32_t p_splits);

private:
	void _draw_list_end_bind_compat_81356(BitField<BarrierMask> p_post_barrier);
	void _compute_list_end_bind_compat_81356(BitField<BarrierMask> p_post_barrier);
	void _barrier_bind_compat_81356(BitField<BarrierMask> p_from, BitField<BarrierMask> p_to);

	void _draw_list_end_bind_compat_84976(BitField<BarrierMask> p_post_barrier);
	void _compute_list_end_bind_compat_84976(BitField<BarrierMask> p_post_barrier);
	InitialAction _convert_initial_action_84976(InitialAction p_old_initial_action);
	FinalAction _convert_final_action_84976(FinalAction p_old_final_action);
	DrawListID _draw_list_begin_bind_compat_84976(RID p_framebuffer, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values, float p_clear_depth, uint32_t p_clear_stencil, const Rect2 &p_region, const TypedArray<RID> &p_storage_textures);
	ComputeListID _compute_list_begin_bind_compat_84976(bool p_allow_draw_overlap);
	Error _buffer_update_bind_compat_84976(RID p_buffer, uint32_t p_offset, uint32_t p_size, const Vector<uint8_t> &p_data, BitField<BarrierMask> p_post_barrier);
	Error _buffer_clear_bind_compat_84976(RID p_buffer, uint32_t p_offset, uint32_t p_size, BitField<BarrierMask> p_post_barrier);
	Error _texture_update_bind_compat_84976(RID p_texture, uint32_t p_layer, const Vector<uint8_t> &p_data, BitField<BarrierMask> p_post_barrier);
	Error _texture_copy_bind_compat_84976(RID p_from_texture, RID p_to_texture, const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_size, uint32_t p_src_mipmap, uint32_t p_dst_mipmap, uint32_t p_src_layer, uint32_t p_dst_layer, BitField<BarrierMask> p_post_barrier);
	Error _texture_clear_bind_compat_84976(RID p_texture, const Color &p_color, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers, BitField<BarrierMask> p_post_barrier);
	Error _texture_resolve_multisample_bind_compat_84976(RID p_from_texture, RID p_to_texture, BitField<BarrierMask> p_post_barrier);

	FramebufferFormatID _screen_get_framebuffer_format_bind_compat_87340() const;

	DrawListID _draw_list_begin_bind_compat_90993(RID p_framebuffer, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values = Vector<Color>(), float p_clear_depth = 1.0, uint32_t p_clear_stencil = 0, const Rect2 &p_region = Rect2());
#endif

public:
	RenderingContextDriver *get_context_driver() const { return context; }

	const RDD::Capabilities &get_device_capabilities() const { return driver->get_capabilities(); }

	bool has_feature(const Features p_feature) const;

	Vector<uint8_t> shader_compile_spirv_from_source(ShaderStage p_stage, const String &p_source_code, ShaderLanguage p_language = SHADER_LANGUAGE_GLSL, String *r_error = nullptr, bool p_allow_cache = true);
	String shader_get_spirv_cache_key() const;

	static void shader_set_compile_to_spirv_function(ShaderCompileToSPIRVFunction p_function);
	static void shader_set_spirv_cache_function(ShaderCacheFunction p_function);
	static void shader_set_get_cache_key_function(ShaderSPIRVGetCacheKeyFunction p_function);

	String shader_get_binary_cache_key() const;
	Vector<uint8_t> shader_compile_binary_from_spirv(const Vector<ShaderStageSPIRVData> &p_spirv, const String &p_shader_name = "");

	RID shader_create_from_spirv(const Vector<ShaderStageSPIRVData> &p_spirv, const String &p_shader_name = "");
	RID shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, RID p_placeholder = RID());
	RID shader_create_placeholder();

	uint64_t shader_get_vertex_input_attribute_mask(RID p_shader);

	/******************/
	/**** UNIFORMS ****/
	/******************/
	String get_perf_report() const;

	enum StorageBufferUsage {
		STORAGE_BUFFER_USAGE_DISPATCH_INDIRECT = 1,
	};

	RID uniform_buffer_create(uint32_t p_size_bytes, const Vector<uint8_t> &p_data = Vector<uint8_t>());
	RID storage_buffer_create(uint32_t p_size, const Vector<uint8_t> &p_data = Vector<uint8_t>(), BitField<StorageBufferUsage> p_usage = 0);
	RID texture_buffer_create(uint32_t p_size_elements, DataFormat p_format, const Vector<uint8_t> &p_data = Vector<uint8_t>());

	struct Uniform {
		UniformType uniform_type = UNIFORM_TYPE_IMAGE;
		uint32_t binding = 0; // Binding index as specified in shader.

	private:
		// In most cases only one ID is provided per binding, so avoid allocating memory unnecessarily for performance.
		RID id; // If only one is provided, this is used.
		Vector<RID> ids; // If multiple ones are provided, this is used instead.

	public:
		_FORCE_INLINE_ uint32_t get_id_count() const {
			return (id.is_valid() ? 1 : ids.size());
		}

		_FORCE_INLINE_ RID get_id(uint32_t p_idx) const {
			if (id.is_valid()) {
				ERR_FAIL_COND_V(p_idx != 0, RID());
				return id;
			} else {
				return ids[p_idx];
			}
		}
		_FORCE_INLINE_ void set_id(uint32_t p_idx, RID p_id) {
			if (id.is_valid()) {
				ERR_FAIL_COND(p_idx != 0);
				id = p_id;
			} else {
				ids.write[p_idx] = p_id;
			}
		}

		_FORCE_INLINE_ void append_id(RID p_id) {
			if (ids.is_empty()) {
				if (id == RID()) {
					id = p_id;
				} else {
					ids.push_back(id);
					ids.push_back(p_id);
					id = RID();
				}
			} else {
				ids.push_back(p_id);
			}
		}

		_FORCE_INLINE_ void clear_ids() {
			id = RID();
			ids.clear();
		}

		_FORCE_INLINE_ Uniform(UniformType p_type, int p_binding, RID p_id) {
			uniform_type = p_type;
			binding = p_binding;
			id = p_id;
		}
		_FORCE_INLINE_ Uniform(UniformType p_type, int p_binding, const Vector<RID> &p_ids) {
			uniform_type = p_type;
			binding = p_binding;
			ids = p_ids;
		}
		_FORCE_INLINE_ Uniform() = default;
	};

private:
	static const uint32_t MAX_UNIFORM_SETS = 16;
	static const uint32_t MAX_PUSH_CONSTANT_SIZE = 128;

	// This structure contains the descriptor set. They _need_ to be allocated
	// for a shader (and will be erased when this shader is erased), but should
	// work for other shaders as long as the hash matches. This covers using
	// them in shader variants.
	//
	// Keep also in mind that you can share buffers between descriptor sets, so
	// the above restriction is not too serious.

	struct UniformSet {
		uint32_t format = 0;
		RID shader_id;
		uint32_t shader_set = 0;
		RDD::UniformSetID driver_id;
		struct AttachableTexture {
			uint32_t bind = 0;
			RID texture;
		};

		struct SharedTexture {
			uint32_t writing = 0;
			RID texture;
		};

		LocalVector<AttachableTexture> attachable_textures; // Used for validation.
		Vector<RDG::ResourceTracker *> draw_trackers;
		Vector<RDG::ResourceUsage> draw_trackers_usage;
		HashMap<RID, RDG::ResourceUsage> untracked_usage;
		LocalVector<SharedTexture> shared_textures_to_update;
		InvalidationCallback invalidated_callback = nullptr;
		void *invalidated_callback_userdata = nullptr;
	};

	RID_Owner<UniformSet, true> uniform_set_owner;

	void _uniform_set_update_shared(UniformSet *p_uniform_set);

public:
	RID uniform_set_create(const Vector<Uniform> &p_uniforms, RID p_shader, uint32_t p_shader_set);
	bool uniform_set_is_valid(RID p_uniform_set);
	void uniform_set_set_invalidation_callback(RID p_uniform_set, InvalidationCallback p_callback, void *p_userdata);

	/*******************/
	/**** PIPELINES ****/
	/*******************/

	// Render pipeline contains ALL the
	// information required for drawing.
	// This includes all the rasterizer state
	// as well as shader used, framebuffer format,
	// etc.
	// While the pipeline is just a single object
	// (VkPipeline) a lot of values are also saved
	// here to do validation (vulkan does none by
	// default) and warn the user if something
	// was not supplied as intended.
private:
	struct RenderPipeline {
		// Cached values for validation.
#ifdef DEBUG_ENABLED
		struct Validation {
			FramebufferFormatID framebuffer_format;
			uint32_t render_pass = 0;
			uint32_t dynamic_state = 0;
			VertexFormatID vertex_format;
			bool uses_restart_indices = false;
			uint32_t primitive_minimum = 0;
			uint32_t primitive_divisor = 0;
		} validation;
#endif
		// Actual pipeline.
		RID shader;
		RDD::ShaderID shader_driver_id;
		uint32_t shader_layout_hash = 0;
		Vector<uint32_t> set_formats;
		RDD::PipelineID driver_id;
		BitField<RDD::PipelineStageBits> stage_bits;
		uint32_t push_constant_size = 0;
	};

	RID_Owner<RenderPipeline, true> render_pipeline_owner;

	bool pipeline_cache_enabled = false;
	size_t pipeline_cache_size = 0;
	String pipeline_cache_file_path;
	WorkerThreadPool::TaskID pipeline_cache_save_task = WorkerThreadPool::INVALID_TASK_ID;

	Vector<uint8_t> _load_pipeline_cache();
	void _update_pipeline_cache(bool p_closing = false);
	static void _save_pipeline_cache(void *p_data);

	struct ComputePipeline {
		RID shader;
		RDD::ShaderID shader_driver_id;
		uint32_t shader_layout_hash = 0;
		Vector<uint32_t> set_formats;
		RDD::PipelineID driver_id;
		uint32_t push_constant_size = 0;
		uint32_t local_group_size[3] = { 0, 0, 0 };
	};

	RID_Owner<ComputePipeline, true> compute_pipeline_owner;

public:
	RID render_pipeline_create(RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const PipelineRasterizationState &p_rasterization_state, const PipelineMultisampleState &p_multisample_state, const PipelineDepthStencilState &p_depth_stencil_state, const PipelineColorBlendState &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags = 0, uint32_t p_for_render_pass = 0, const Vector<PipelineSpecializationConstant> &p_specialization_constants = Vector<PipelineSpecializationConstant>());
	bool render_pipeline_is_valid(RID p_pipeline);

	RID compute_pipeline_create(RID p_shader, const Vector<PipelineSpecializationConstant> &p_specialization_constants = Vector<PipelineSpecializationConstant>());
	bool compute_pipeline_is_valid(RID p_pipeline);

private:
	/****************/
	/**** SCREEN ****/
	/****************/
	HashMap<DisplayServer::WindowID, RDD::SwapChainID> screen_swap_chains;
	HashMap<DisplayServer::WindowID, RDD::FramebufferID> screen_framebuffers;

	uint32_t _get_swap_chain_desired_count() const;

public:
	Error screen_create(DisplayServer::WindowID p_screen = DisplayServer::MAIN_WINDOW_ID);
	Error screen_prepare_for_drawing(DisplayServer::WindowID p_screen = DisplayServer::MAIN_WINDOW_ID);
	int screen_get_width(DisplayServer::WindowID p_screen = DisplayServer::MAIN_WINDOW_ID) const;
	int screen_get_height(DisplayServer::WindowID p_screen = DisplayServer::MAIN_WINDOW_ID) const;
	FramebufferFormatID screen_get_framebuffer_format(DisplayServer::WindowID p_screen = DisplayServer::MAIN_WINDOW_ID) const;
	Error screen_free(DisplayServer::WindowID p_screen = DisplayServer::MAIN_WINDOW_ID);

	/*************************/
	/**** DRAW LISTS (II) ****/
	/*************************/

private:
	// Draw list contains both the command buffer
	// used for drawing as well as a LOT of
	// information used for validation. This
	// validation is cheap so most of it can
	// also run in release builds.

	struct DrawList {
		Rect2i viewport;
		bool viewport_set = false;

		struct SetState {
			uint32_t pipeline_expected_format = 0;
			uint32_t uniform_set_format = 0;
			RDD::UniformSetID uniform_set_driver_id;
			RID uniform_set;
			bool bound = false;
		};

		struct State {
			SetState sets[MAX_UNIFORM_SETS];
			uint32_t set_count = 0;
			RID pipeline;
			RID pipeline_shader;
			RDD::ShaderID pipeline_shader_driver_id;
			uint32_t pipeline_shader_layout_hash = 0;
			uint32_t pipeline_push_constant_size = 0;
			RID vertex_array;
			RID index_array;
			uint32_t draw_count = 0;
		} state;

#ifdef DEBUG_ENABLED
		struct Validation {
			bool active = true; // Means command buffer was not closed, so you can keep adding things.
			// Actual render pass values.
			uint32_t dynamic_state = 0;
			VertexFormatID vertex_format = INVALID_ID;
			uint32_t vertex_array_size = 0;
			uint32_t vertex_max_instances_allowed = 0xFFFFFFFF;
			bool index_buffer_uses_restart_indices = false;
			uint32_t index_array_count = 0;
			uint32_t index_array_max_index = 0;
			Vector<uint32_t> set_formats;
			Vector<bool> set_bound;
			Vector<RID> set_rids;
			// Last pipeline set values.
			bool pipeline_active = false;
			uint32_t pipeline_dynamic_state = 0;
			VertexFormatID pipeline_vertex_format = INVALID_ID;
			RID pipeline_shader;
			bool pipeline_uses_restart_indices = false;
			uint32_t pipeline_primitive_divisor = 0;
			uint32_t pipeline_primitive_minimum = 0;
			uint32_t pipeline_push_constant_size = 0;
			bool pipeline_push_constant_supplied = false;
		} validation;
#else
		struct Validation {
			uint32_t vertex_array_size = 0;
			uint32_t index_array_count = 0;
		} validation;
#endif
	};

	DrawList *draw_list = nullptr;
	uint32_t draw_list_subpass_count = 0;
	RDD::RenderPassID draw_list_render_pass;
	RDD::FramebufferID draw_list_vkframebuffer;
#ifdef DEBUG_ENABLED
	FramebufferFormatID draw_list_framebuffer_format = INVALID_ID;
#endif
	uint32_t draw_list_current_subpass = 0;

	Vector<RID> draw_list_bound_textures;

	void _draw_list_insert_clear_region(DrawList *p_draw_list, Framebuffer *p_framebuffer, Point2i p_viewport_offset, Point2i p_viewport_size, bool p_clear_color, const Vector<Color> &p_clear_colors, bool p_clear_depth, float p_depth, uint32_t p_stencil);
	Error _draw_list_setup_framebuffer(Framebuffer *p_framebuffer, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, RDD::FramebufferID *r_framebuffer, RDD::RenderPassID *r_render_pass, uint32_t *r_subpass_count);
	Error _draw_list_render_pass_begin(Framebuffer *p_framebuffer, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_colors, float p_clear_depth, uint32_t p_clear_stencil, Point2i p_viewport_offset, Point2i p_viewport_size, RDD::FramebufferID p_framebuffer_driver_id, RDD::RenderPassID p_render_pass, uint32_t p_breadcrumb);
	_FORCE_INLINE_ DrawList *_get_draw_list_ptr(DrawListID p_id);
	Error _draw_list_allocate(const Rect2i &p_viewport, uint32_t p_subpass);
	void _draw_list_free(Rect2i *r_last_viewport = nullptr);

public:
	DrawListID draw_list_begin_for_screen(DisplayServer::WindowID p_screen = 0, const Color &p_clear_color = Color());
	DrawListID draw_list_begin(RID p_framebuffer, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values = Vector<Color>(), float p_clear_depth = 1.0, uint32_t p_clear_stencil = 0, const Rect2 &p_region = Rect2(), uint32_t p_breadcrumb = 0);

	void draw_list_set_blend_constants(DrawListID p_list, const Color &p_color);
	void draw_list_bind_render_pipeline(DrawListID p_list, RID p_render_pipeline);
	void draw_list_bind_uniform_set(DrawListID p_list, RID p_uniform_set, uint32_t p_index);
	void draw_list_bind_vertex_array(DrawListID p_list, RID p_vertex_array);
	void draw_list_bind_index_array(DrawListID p_list, RID p_index_array);
	void draw_list_set_line_width(DrawListID p_list, float p_width);
	void draw_list_set_push_constant(DrawListID p_list, const void *p_data, uint32_t p_data_size);

	void draw_list_draw(DrawListID p_list, bool p_use_indices, uint32_t p_instances = 1, uint32_t p_procedural_vertices = 0);

	void draw_list_enable_scissor(DrawListID p_list, const Rect2 &p_rect);
	void draw_list_disable_scissor(DrawListID p_list);

	uint32_t draw_list_get_current_pass();
	DrawListID draw_list_switch_to_next_pass();

	void draw_list_end();

private:
	/***********************/
	/**** COMPUTE LISTS ****/
	/***********************/

	struct ComputeList {
		struct SetState {
			uint32_t pipeline_expected_format = 0;
			uint32_t uniform_set_format = 0;
			RDD::UniformSetID uniform_set_driver_id;
			RID uniform_set;
			bool bound = false;
		};

		struct State {
			SetState sets[MAX_UNIFORM_SETS];
			uint32_t set_count = 0;
			RID pipeline;
			RID pipeline_shader;
			RDD::ShaderID pipeline_shader_driver_id;
			uint32_t pipeline_shader_layout_hash = 0;
			uint32_t local_group_size[3] = { 0, 0, 0 };
			uint8_t push_constant_data[MAX_PUSH_CONSTANT_SIZE] = {};
			uint32_t push_constant_size = 0;
			uint32_t dispatch_count = 0;
		} state;

#ifdef DEBUG_ENABLED
		struct Validation {
			bool active = true; // Means command buffer was not closed, so you can keep adding things.
			Vector<uint32_t> set_formats;
			Vector<bool> set_bound;
			Vector<RID> set_rids;
			// Last pipeline set values.
			bool pipeline_active = false;
			RID pipeline_shader;
			uint32_t invalid_set_from = 0;
			uint32_t pipeline_push_constant_size = 0;
			bool pipeline_push_constant_supplied = false;
		} validation;
#endif
	};

	ComputeList *compute_list = nullptr;
	ComputeList::State compute_list_barrier_state;

public:
	ComputeListID compute_list_begin();
	void compute_list_bind_compute_pipeline(ComputeListID p_list, RID p_compute_pipeline);
	void compute_list_bind_uniform_set(ComputeListID p_list, RID p_uniform_set, uint32_t p_index);
	void compute_list_set_push_constant(ComputeListID p_list, const void *p_data, uint32_t p_data_size);
	void compute_list_dispatch(ComputeListID p_list, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups);
	void compute_list_dispatch_threads(ComputeListID p_list, uint32_t p_x_threads, uint32_t p_y_threads, uint32_t p_z_threads);
	void compute_list_dispatch_indirect(ComputeListID p_list, RID p_buffer, uint32_t p_offset);
	void compute_list_add_barrier(ComputeListID p_list);

	void compute_list_end();

private:
	/*************************/
	/**** TRANSFER WORKER ****/
	/*************************/

	struct TransferWorker {
		uint32_t index = 0;
		RDD::BufferID staging_buffer;
		uint32_t max_transfer_size = 0;
		uint32_t staging_buffer_size_in_use = 0;
		uint32_t staging_buffer_size_allocated = 0;
		RDD::CommandBufferID command_buffer;
		RDD::CommandPoolID command_pool;
		RDD::FenceID command_fence;
		RDD::SemaphoreID command_semaphore;
		bool recording = false;
		bool submitted = false;
		BinaryMutex thread_mutex;
		uint64_t operations_processed = 0;
		uint64_t operations_submitted = 0;
		uint64_t operations_counter = 0;
		BinaryMutex operations_mutex;
	};

	LocalVector<TransferWorker *> transfer_worker_pool;
	uint32_t transfer_worker_pool_max_size = 1;
	LocalVector<uint64_t> transfer_worker_operation_used_by_draw;
	LocalVector<uint32_t> transfer_worker_pool_available_list;
	BinaryMutex transfer_worker_pool_mutex;
	ConditionVariable transfer_worker_pool_condition;

	TransferWorker *_acquire_transfer_worker(uint32_t p_transfer_size, uint32_t p_required_align, uint32_t &r_staging_offset);
	void _release_transfer_worker(TransferWorker *p_transfer_worker);
	void _end_transfer_worker(TransferWorker *p_transfer_worker);
	void _submit_transfer_worker(TransferWorker *p_transfer_worker, bool p_signal_semaphore);
	void _wait_for_transfer_worker(TransferWorker *p_transfer_worker);
	void _check_transfer_worker_operation(uint32_t p_transfer_worker_index, uint64_t p_transfer_worker_operation);
	void _check_transfer_worker_buffer(Buffer *p_buffer);
	void _check_transfer_worker_texture(Texture *p_texture);
	void _check_transfer_worker_vertex_array(VertexArray *p_vertex_array);
	void _check_transfer_worker_index_array(IndexArray *p_index_array);
	void _submit_transfer_workers(bool p_operations_used_by_draw);
	void _wait_for_transfer_workers();
	void _free_transfer_workers();

	/***********************/
	/**** COMMAND GRAPH ****/
	/***********************/

	bool _texture_make_mutable(Texture *p_texture, RID p_texture_id);
	bool _buffer_make_mutable(Buffer *p_buffer, RID p_buffer_id);
	bool _vertex_array_make_mutable(VertexArray *p_vertex_array, RID p_resource_id, RDG::ResourceTracker *p_resource_tracker);
	bool _index_array_make_mutable(IndexArray *p_index_array, RDG::ResourceTracker *p_resource_tracker);
	bool _uniform_set_make_mutable(UniformSet *p_uniform_set, RID p_resource_id, RDG::ResourceTracker *p_resource_tracker);
	bool _dependency_make_mutable(RID p_id, RID p_resource_id, RDG::ResourceTracker *p_resource_tracker);
	bool _dependencies_make_mutable_recursive(RID p_id, RDG::ResourceTracker *p_resource_tracker);
	bool _dependencies_make_mutable(RID p_id, RDG::ResourceTracker *p_resource_tracker);

	RenderingDeviceGraph draw_graph;

	/**************************/
	/**** QUEUE MANAGEMENT ****/
	/**************************/

	RDD::CommandQueueFamilyID main_queue_family;
	RDD::CommandQueueFamilyID transfer_queue_family;
	RDD::CommandQueueFamilyID present_queue_family;
	RDD::CommandQueueID main_queue;
	RDD::CommandQueueID transfer_queue;
	RDD::CommandQueueID present_queue;

	/**************************/
	/**** FRAME MANAGEMENT ****/
	/**************************/

	// This is the frame structure. There are normally
	// 3 of these (used for triple buffering), or 2
	// (double buffering). They are cycled constantly.
	//
	// It contains two command buffers, one that is
	// used internally for setting up (creating stuff)
	// and another used mostly for drawing.
	//
	// They also contains a list of things that need
	// to be disposed of when deleted, which can't
	// happen immediately due to the asynchronous
	// nature of the GPU. They will get deleted
	// when the frame is cycled.

	struct Frame {
		// List in usage order, from last to free to first to free.
		List<Buffer> buffers_to_dispose_of;
		List<Texture> textures_to_dispose_of;
		List<Framebuffer> framebuffers_to_dispose_of;
		List<RDD::SamplerID> samplers_to_dispose_of;
		List<Shader> shaders_to_dispose_of;
		List<UniformSet> uniform_sets_to_dispose_of;
		List<RenderPipeline> render_pipelines_to_dispose_of;
		List<ComputePipeline> compute_pipelines_to_dispose_of;

		// The command pool used by the command buffer.
		RDD::CommandPoolID command_pool;

		// The command buffer used by the main thread when recording the frame.
		RDD::CommandBufferID command_buffer;

		// Signaled by the command buffer submission. Present must wait on this semaphore.
		RDD::SemaphoreID semaphore;

		// Signaled by the command buffer submission. Must wait on this fence before beginning command recording for the frame.
		RDD::FenceID fence;
		bool fence_signaled = false;

		// Semaphores the frame must wait on before executing the command buffer.
		LocalVector<RDD::SemaphoreID> semaphores_to_wait_on;

		// Swap chains prepared for drawing during the frame that must be presented.
		LocalVector<RDD::SwapChainID> swap_chains_to_present;

		// Extra command buffer pool used for driver workarounds.
		RDG::CommandBufferPool command_buffer_pool;

		struct Timestamp {
			String description;
			uint64_t value = 0;
		};

		RDD::QueryPoolID timestamp_pool;

		TightLocalVector<String> timestamp_names;
		TightLocalVector<uint64_t> timestamp_cpu_values;
		uint32_t timestamp_count = 0;
		TightLocalVector<String> timestamp_result_names;
		TightLocalVector<uint64_t> timestamp_cpu_result_values;
		TightLocalVector<uint64_t> timestamp_result_values;
		uint32_t timestamp_result_count = 0;
		uint64_t index = 0;
	};

	uint32_t max_timestamp_query_elements = 0;

	int frame = 0;
	TightLocalVector<Frame> frames;
	uint64_t frames_drawn = 0;

	void _free_pending_resources(int p_frame);

	uint64_t texture_memory = 0;
	uint64_t buffer_memory = 0;

	void _free_internal(RID p_id);
	void _begin_frame();
	void _end_frame();
	void _execute_frame(bool p_present);
	void _stall_for_previous_frames();
	void _flush_and_stall_for_all_frames();

	template <typename T>
	void _free_rids(T &p_owner, const char *p_type);

#ifdef DEV_ENABLED
	HashMap<RID, String> resource_names;
#endif

public:
	Error initialize(RenderingContextDriver *p_context, DisplayServer::WindowID p_main_window = DisplayServer::INVALID_WINDOW_ID);
	void finalize();

	void free(RID p_id);

	/****************/
	/**** Timing ****/
	/****************/

	void capture_timestamp(const String &p_name);
	uint32_t get_captured_timestamps_count() const;
	uint64_t get_captured_timestamps_frame() const;
	uint64_t get_captured_timestamp_gpu_time(uint32_t p_index) const;
	uint64_t get_captured_timestamp_cpu_time(uint32_t p_index) const;
	String get_captured_timestamp_name(uint32_t p_index) const;

	/****************/
	/**** LIMITS ****/
	/****************/

	uint64_t limit_get(Limit p_limit) const;

	void swap_buffers();

	uint32_t get_frame_delay() const;

	void submit();
	void sync();

	enum MemoryType {
		MEMORY_TEXTURES,
		MEMORY_BUFFERS,
		MEMORY_TOTAL
	};

	uint64_t get_memory_usage(MemoryType p_type) const;

	RenderingDevice *create_local_device();

	void set_resource_name(RID p_id, const String &p_name);

	void draw_command_begin_label(String p_label_name, const Color &p_color = Color(1, 1, 1, 1));
	void draw_command_end_label();

	String get_device_vendor_name() const;
	String get_device_name() const;
	DeviceType get_device_type() const;
	String get_device_api_name() const;
	String get_device_api_version() const;
	String get_device_pipeline_cache_uuid() const;

	bool is_composite_alpha_supported() const;

	uint64_t get_driver_resource(DriverResource p_resource, RID p_rid = RID(), uint64_t p_index = 0);

	String get_driver_and_device_memory_report() const;

	String get_tracked_object_name(uint32_t p_type_index) const;
	uint64_t get_tracked_object_type_count() const;

	uint64_t get_driver_total_memory() const;
	uint64_t get_driver_allocation_count() const;
	uint64_t get_driver_memory_by_object_type(uint32_t p_type) const;
	uint64_t get_driver_allocs_by_object_type(uint32_t p_type) const;

	uint64_t get_device_total_memory() const;
	uint64_t get_device_allocation_count() const;
	uint64_t get_device_memory_by_object_type(uint32_t p_type) const;
	uint64_t get_device_allocs_by_object_type(uint32_t p_type) const;

	static RenderingDevice *get_singleton();

	RenderingDevice();
	~RenderingDevice();

private:
	/*****************/
	/**** BINDERS ****/
	/*****************/

	RID _texture_create(const Ref<RDTextureFormat> &p_format, const Ref<RDTextureView> &p_view, const TypedArray<PackedByteArray> &p_data = Array());
	RID _texture_create_shared(const Ref<RDTextureView> &p_view, RID p_with_texture);
	RID _texture_create_shared_from_slice(const Ref<RDTextureView> &p_view, RID p_with_texture, uint32_t p_layer, uint32_t p_mipmap, uint32_t p_mipmaps = 1, TextureSliceType p_slice_type = TEXTURE_SLICE_2D);
	Ref<RDTextureFormat> _texture_get_format(RID p_rd_texture);

	FramebufferFormatID _framebuffer_format_create(const TypedArray<RDAttachmentFormat> &p_attachments, uint32_t p_view_count);
	FramebufferFormatID _framebuffer_format_create_multipass(const TypedArray<RDAttachmentFormat> &p_attachments, const TypedArray<RDFramebufferPass> &p_passes, uint32_t p_view_count);
	RID _framebuffer_create(const TypedArray<RID> &p_textures, FramebufferFormatID p_format_check = INVALID_ID, uint32_t p_view_count = 1);
	RID _framebuffer_create_multipass(const TypedArray<RID> &p_textures, const TypedArray<RDFramebufferPass> &p_passes, FramebufferFormatID p_format_check = INVALID_ID, uint32_t p_view_count = 1);

	RID _sampler_create(const Ref<RDSamplerState> &p_state);

	VertexFormatID _vertex_format_create(const TypedArray<RDVertexAttribute> &p_vertex_formats);
	RID _vertex_array_create(uint32_t p_vertex_count, VertexFormatID p_vertex_format, const TypedArray<RID> &p_src_buffers, const Vector<int64_t> &p_offsets = Vector<int64_t>());

	Ref<RDShaderSPIRV> _shader_compile_spirv_from_source(const Ref<RDShaderSource> &p_source, bool p_allow_cache = true);
	Vector<uint8_t> _shader_compile_binary_from_spirv(const Ref<RDShaderSPIRV> &p_bytecode, const String &p_shader_name = "");
	RID _shader_create_from_spirv(const Ref<RDShaderSPIRV> &p_spirv, const String &p_shader_name = "");

	RID _uniform_set_create(const TypedArray<RDUniform> &p_uniforms, RID p_shader, uint32_t p_shader_set);

	Error _buffer_update_bind(RID p_buffer, uint32_t p_offset, uint32_t p_size, const Vector<uint8_t> &p_data);

	RID _render_pipeline_create(RID p_shader, FramebufferFormatID p_framebuffer_format, VertexFormatID p_vertex_format, RenderPrimitive p_render_primitive, const Ref<RDPipelineRasterizationState> &p_rasterization_state, const Ref<RDPipelineMultisampleState> &p_multisample_state, const Ref<RDPipelineDepthStencilState> &p_depth_stencil_state, const Ref<RDPipelineColorBlendState> &p_blend_state, BitField<PipelineDynamicStateFlags> p_dynamic_state_flags, uint32_t p_for_render_pass, const TypedArray<RDPipelineSpecializationConstant> &p_specialization_constants);
	RID _compute_pipeline_create(RID p_shader, const TypedArray<RDPipelineSpecializationConstant> &p_specialization_constants);

	void _draw_list_set_push_constant(DrawListID p_list, const Vector<uint8_t> &p_data, uint32_t p_data_size);
	void _compute_list_set_push_constant(ComputeListID p_list, const Vector<uint8_t> &p_data, uint32_t p_data_size);
};

VARIANT_ENUM_CAST(RenderingDevice::DeviceType)
VARIANT_ENUM_CAST(RenderingDevice::DriverResource)
VARIANT_ENUM_CAST(RenderingDevice::ShaderStage)
VARIANT_ENUM_CAST(RenderingDevice::ShaderLanguage)
VARIANT_ENUM_CAST(RenderingDevice::CompareOperator)
VARIANT_ENUM_CAST(RenderingDevice::DataFormat)
VARIANT_ENUM_CAST(RenderingDevice::TextureType)
VARIANT_ENUM_CAST(RenderingDevice::TextureSamples)
VARIANT_BITFIELD_CAST(RenderingDevice::TextureUsageBits)
VARIANT_ENUM_CAST(RenderingDevice::TextureSwizzle)
VARIANT_ENUM_CAST(RenderingDevice::TextureSliceType)
VARIANT_ENUM_CAST(RenderingDevice::SamplerFilter)
VARIANT_ENUM_CAST(RenderingDevice::SamplerRepeatMode)
VARIANT_ENUM_CAST(RenderingDevice::SamplerBorderColor)
VARIANT_ENUM_CAST(RenderingDevice::VertexFrequency)
VARIANT_ENUM_CAST(RenderingDevice::IndexBufferFormat)
VARIANT_BITFIELD_CAST(RenderingDevice::StorageBufferUsage)
VARIANT_ENUM_CAST(RenderingDevice::UniformType)
VARIANT_ENUM_CAST(RenderingDevice::RenderPrimitive)
VARIANT_ENUM_CAST(RenderingDevice::PolygonCullMode)
VARIANT_ENUM_CAST(RenderingDevice::PolygonFrontFace)
VARIANT_ENUM_CAST(RenderingDevice::StencilOperation)
VARIANT_ENUM_CAST(RenderingDevice::LogicOperation)
VARIANT_ENUM_CAST(RenderingDevice::BlendFactor)
VARIANT_ENUM_CAST(RenderingDevice::BlendOperation)
VARIANT_BITFIELD_CAST(RenderingDevice::PipelineDynamicStateFlags)
VARIANT_ENUM_CAST(RenderingDevice::PipelineSpecializationConstantType)
VARIANT_ENUM_CAST(RenderingDevice::InitialAction)
VARIANT_ENUM_CAST(RenderingDevice::FinalAction)
VARIANT_ENUM_CAST(RenderingDevice::Limit)
VARIANT_ENUM_CAST(RenderingDevice::MemoryType)
VARIANT_ENUM_CAST(RenderingDevice::Features)
VARIANT_ENUM_CAST(RenderingDevice::BreadcrumbMarker)

#ifndef DISABLE_DEPRECATED
VARIANT_BITFIELD_CAST(RenderingDevice::BarrierMask);
#endif

typedef RenderingDevice RD;

#endif // RENDERING_DEVICE_H
