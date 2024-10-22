/**************************************************************************/
/*  metal_objects.h                                                       */
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

/**************************************************************************/
/*                                                                        */
/* Portions of this code were derived from MoltenVK.                      */
/*                                                                        */
/* Copyright (c) 2015-2023 The Brenwill Workshop Ltd.                     */
/* (http://www.brenwill.com)                                              */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/**************************************************************************/

#ifndef METAL_OBJECTS_H
#define METAL_OBJECTS_H

#import "metal_device_properties.h"
#import "metal_utils.h"
#import "pixel_formats.h"

#import "servers/rendering/rendering_device_driver.h"

#import <CommonCrypto/CommonDigest.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <simd/simd.h>
#import <zlib.h>
#import <initializer_list>
#import <optional>
#import <spirv.hpp>

// These types can be used in Vector and other containers that use
// pointer operations not supported by ARC.
namespace MTL {
#define MTL_CLASS(name)                                  \
	class name {                                         \
	public:                                              \
		name(id<MTL##name> obj = nil) : m_obj(obj) {}    \
		operator id<MTL##name>() const { return m_obj; } \
		id<MTL##name> m_obj;                             \
	};

MTL_CLASS(Texture)

} //namespace MTL

enum ShaderStageUsage : uint32_t {
	None = 0,
	Vertex = RDD::SHADER_STAGE_VERTEX_BIT,
	Fragment = RDD::SHADER_STAGE_FRAGMENT_BIT,
	TesselationControl = RDD::SHADER_STAGE_TESSELATION_CONTROL_BIT,
	TesselationEvaluation = RDD::SHADER_STAGE_TESSELATION_EVALUATION_BIT,
	Compute = RDD::SHADER_STAGE_COMPUTE_BIT,
};

_FORCE_INLINE_ ShaderStageUsage &operator|=(ShaderStageUsage &p_a, int p_b) {
	p_a = ShaderStageUsage(uint32_t(p_a) | uint32_t(p_b));
	return p_a;
}

enum StageResourceUsage : uint32_t {
	VertexRead = (MTLResourceUsageRead << RDD::SHADER_STAGE_VERTEX * 2),
	VertexWrite = (MTLResourceUsageWrite << RDD::SHADER_STAGE_VERTEX * 2),
	FragmentRead = (MTLResourceUsageRead << RDD::SHADER_STAGE_FRAGMENT * 2),
	FragmentWrite = (MTLResourceUsageWrite << RDD::SHADER_STAGE_FRAGMENT * 2),
	TesselationControlRead = (MTLResourceUsageRead << RDD::SHADER_STAGE_TESSELATION_CONTROL * 2),
	TesselationControlWrite = (MTLResourceUsageWrite << RDD::SHADER_STAGE_TESSELATION_CONTROL * 2),
	TesselationEvaluationRead = (MTLResourceUsageRead << RDD::SHADER_STAGE_TESSELATION_EVALUATION * 2),
	TesselationEvaluationWrite = (MTLResourceUsageWrite << RDD::SHADER_STAGE_TESSELATION_EVALUATION * 2),
	ComputeRead = (MTLResourceUsageRead << RDD::SHADER_STAGE_COMPUTE * 2),
	ComputeWrite = (MTLResourceUsageWrite << RDD::SHADER_STAGE_COMPUTE * 2),
};

typedef LocalVector<__unsafe_unretained id<MTLResource>> ResourceVector;
typedef HashMap<StageResourceUsage, ResourceVector> ResourceUsageMap;

enum class MDCommandBufferStateType {
	None,
	Render,
	Compute,
	Blit,
};

enum class MDPipelineType {
	None,
	Render,
	Compute,
};

class MDRenderPass;
class MDPipeline;
class MDRenderPipeline;
class MDComputePipeline;
class MDFrameBuffer;
class RenderingDeviceDriverMetal;
class MDUniformSet;
class MDShader;

#pragma mark - Resource Factory

struct ClearAttKey {
	const static uint32_t COLOR_COUNT = MAX_COLOR_ATTACHMENT_COUNT;
	const static uint32_t DEPTH_INDEX = COLOR_COUNT;
	const static uint32_t STENCIL_INDEX = DEPTH_INDEX + 1;
	const static uint32_t ATTACHMENT_COUNT = STENCIL_INDEX + 1;

	uint16_t sample_count = 0;
	uint16_t pixel_formats[ATTACHMENT_COUNT] = { 0 };

	_FORCE_INLINE_ void set_color_format(uint32_t p_idx, MTLPixelFormat p_fmt) { pixel_formats[p_idx] = p_fmt; }
	_FORCE_INLINE_ void set_depth_format(MTLPixelFormat p_fmt) { pixel_formats[DEPTH_INDEX] = p_fmt; }
	_FORCE_INLINE_ void set_stencil_format(MTLPixelFormat p_fmt) { pixel_formats[STENCIL_INDEX] = p_fmt; }
	_FORCE_INLINE_ MTLPixelFormat depth_format() const { return (MTLPixelFormat)pixel_formats[DEPTH_INDEX]; }
	_FORCE_INLINE_ MTLPixelFormat stencil_format() const { return (MTLPixelFormat)pixel_formats[STENCIL_INDEX]; }

	_FORCE_INLINE_ bool is_enabled(uint32_t p_idx) const { return pixel_formats[p_idx] != 0; }
	_FORCE_INLINE_ bool is_depth_enabled() const { return pixel_formats[DEPTH_INDEX] != 0; }
	_FORCE_INLINE_ bool is_stencil_enabled() const { return pixel_formats[STENCIL_INDEX] != 0; }

	_FORCE_INLINE_ bool operator==(const ClearAttKey &p_rhs) const {
		return memcmp(this, &p_rhs, sizeof(ClearAttKey)) == 0;
	}

	uint32_t hash() const {
		uint32_t h = hash_murmur3_one_32(sample_count);
		h = hash_murmur3_buffer(pixel_formats, ATTACHMENT_COUNT * sizeof(pixel_formats[0]), h);
		return h;
	}
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDResourceFactory {
private:
	RenderingDeviceDriverMetal *device_driver;

	id<MTLFunction> new_func(NSString *p_source, NSString *p_name, NSError **p_error);
	id<MTLFunction> new_clear_vert_func(ClearAttKey &p_key);
	id<MTLFunction> new_clear_frag_func(ClearAttKey &p_key);
	NSString *get_format_type_string(MTLPixelFormat p_fmt);

public:
	id<MTLRenderPipelineState> new_clear_pipeline_state(ClearAttKey &p_key, NSError **p_error);
	id<MTLDepthStencilState> new_depth_stencil_state(bool p_use_depth, bool p_use_stencil);

	MDResourceFactory(RenderingDeviceDriverMetal *p_device_driver) :
			device_driver(p_device_driver) {}
	~MDResourceFactory() = default;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDResourceCache {
private:
	typedef HashMap<ClearAttKey, id<MTLRenderPipelineState>, HashableHasher<ClearAttKey>> HashMap;
	std::unique_ptr<MDResourceFactory> resource_factory;
	HashMap clear_states;

	struct {
		id<MTLDepthStencilState> all;
		id<MTLDepthStencilState> depth_only;
		id<MTLDepthStencilState> stencil_only;
		id<MTLDepthStencilState> none;
	} clear_depth_stencil_state;

public:
	id<MTLRenderPipelineState> get_clear_render_pipeline_state(ClearAttKey &p_key, NSError **p_error);
	id<MTLDepthStencilState> get_depth_stencil_state(bool p_use_depth, bool p_use_stencil);

	explicit MDResourceCache(RenderingDeviceDriverMetal *p_device_driver) :
			resource_factory(new MDResourceFactory(p_device_driver)) {}
	~MDResourceCache() = default;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDCommandBuffer {
private:
	RenderingDeviceDriverMetal *device_driver = nullptr;
	id<MTLCommandQueue> queue = nil;
	id<MTLCommandBuffer> commandBuffer = nil;

	void _end_compute_dispatch();
	void _end_blit();

#pragma mark - Render

	void _render_set_dirty_state();
	void _render_bind_uniform_sets();

	static void _populate_vertices(simd::float4 *p_vertices, Size2i p_fb_size, VectorView<Rect2i> p_rects);
	static uint32_t _populate_vertices(simd::float4 *p_vertices, uint32_t p_index, Rect2i const &p_rect, Size2i p_fb_size);
	void _end_render_pass();
	void _render_clear_render_area();

public:
	MDCommandBufferStateType type = MDCommandBufferStateType::None;

	struct RenderState {
		MDRenderPass *pass = nullptr;
		MDFrameBuffer *frameBuffer = nullptr;
		MDRenderPipeline *pipeline = nullptr;
		LocalVector<RDD::RenderPassClearValue> clear_values;
		LocalVector<MTLViewport> viewports;
		LocalVector<MTLScissorRect> scissors;
		std::optional<Color> blend_constants;
		uint32_t current_subpass = UINT32_MAX;
		Rect2i render_area = {};
		bool is_rendering_entire_area = false;
		MTLRenderPassDescriptor *desc = nil;
		id<MTLRenderCommandEncoder> encoder = nil;
		id<MTLBuffer> __unsafe_unretained index_buffer = nil; // Buffer is owned by RDD.
		MTLIndexType index_type = MTLIndexTypeUInt16;
		uint32_t index_offset = 0;
		LocalVector<id<MTLBuffer> __unsafe_unretained> vertex_buffers;
		LocalVector<NSUInteger> vertex_offsets;
		ResourceUsageMap resource_usage;
		// clang-format off
		enum DirtyFlag: uint8_t {
			DIRTY_NONE     = 0b0000'0000,
			DIRTY_PIPELINE = 0b0000'0001, //! pipeline state
			DIRTY_UNIFORMS = 0b0000'0010, //! uniform sets
			DIRTY_DEPTH    = 0b0000'0100, //! depth / stenci state
			DIRTY_VERTEX   = 0b0000'1000, //! vertex buffers
			DIRTY_VIEWPORT = 0b0001'0000, //! viewport rectangles
			DIRTY_SCISSOR  = 0b0010'0000, //! scissor rectangles
			DIRTY_BLEND    = 0b0100'0000, //! blend state
			DIRTY_RASTER   = 0b1000'0000, //! encoder state like cull mode

			DIRTY_ALL      = 0xff,
		};
		// clang-format on
		BitField<DirtyFlag> dirty = DIRTY_NONE;

		LocalVector<MDUniformSet *> uniform_sets;
		// Bit mask of the uniform sets that are dirty, to prevent redundant binding.
		uint64_t uniform_set_mask = 0;

		_FORCE_INLINE_ void reset() {
			pass = nil;
			frameBuffer = nil;
			pipeline = nil;
			current_subpass = UINT32_MAX;
			render_area = {};
			is_rendering_entire_area = false;
			desc = nil;
			encoder = nil;
			index_buffer = nil;
			index_type = MTLIndexTypeUInt16;
			dirty = DIRTY_NONE;
			uniform_sets.clear();
			uniform_set_mask = 0;
			clear_values.clear();
			viewports.clear();
			scissors.clear();
			blend_constants.reset();
			vertex_buffers.clear();
			vertex_offsets.clear();
			// Keep the keys, as they are likely to be used again.
			for (KeyValue<StageResourceUsage, LocalVector<__unsafe_unretained id<MTLResource>>> &kv : resource_usage) {
				kv.value.clear();
			}
		}

		void end_encoding();

		_FORCE_INLINE_ void mark_viewport_dirty() {
			if (viewports.is_empty()) {
				return;
			}
			dirty.set_flag(DirtyFlag::DIRTY_VIEWPORT);
		}

		_FORCE_INLINE_ void mark_scissors_dirty() {
			if (scissors.is_empty()) {
				return;
			}
			dirty.set_flag(DirtyFlag::DIRTY_SCISSOR);
		}

		_FORCE_INLINE_ void mark_vertex_dirty() {
			if (vertex_buffers.is_empty()) {
				return;
			}
			dirty.set_flag(DirtyFlag::DIRTY_VERTEX);
		}

		_FORCE_INLINE_ void mark_uniforms_dirty(std::initializer_list<uint32_t> l) {
			if (uniform_sets.is_empty()) {
				return;
			}
			for (uint32_t i : l) {
				if (i < uniform_sets.size() && uniform_sets[i] != nullptr) {
					uniform_set_mask |= 1 << i;
				}
			}
			dirty.set_flag(DirtyFlag::DIRTY_UNIFORMS);
		}

		_FORCE_INLINE_ void mark_uniforms_dirty(void) {
			if (uniform_sets.is_empty()) {
				return;
			}
			for (uint32_t i = 0; i < uniform_sets.size(); i++) {
				if (uniform_sets[i] != nullptr) {
					uniform_set_mask |= 1 << i;
				}
			}
			dirty.set_flag(DirtyFlag::DIRTY_UNIFORMS);
		}

		_FORCE_INLINE_ void mark_blend_dirty() {
			if (!blend_constants.has_value()) {
				return;
			}
			dirty.set_flag(DirtyFlag::DIRTY_BLEND);
		}

		MTLScissorRect clip_to_render_area(MTLScissorRect p_rect) const {
			uint32_t raLeft = render_area.position.x;
			uint32_t raRight = raLeft + render_area.size.width;
			uint32_t raBottom = render_area.position.y;
			uint32_t raTop = raBottom + render_area.size.height;

			p_rect.x = CLAMP(p_rect.x, raLeft, MAX(raRight - 1, raLeft));
			p_rect.y = CLAMP(p_rect.y, raBottom, MAX(raTop - 1, raBottom));
			p_rect.width = MIN(p_rect.width, raRight - p_rect.x);
			p_rect.height = MIN(p_rect.height, raTop - p_rect.y);

			return p_rect;
		}

		Rect2i clip_to_render_area(Rect2i p_rect) const {
			int32_t raLeft = render_area.position.x;
			int32_t raRight = raLeft + render_area.size.width;
			int32_t raBottom = render_area.position.y;
			int32_t raTop = raBottom + render_area.size.height;

			p_rect.position.x = CLAMP(p_rect.position.x, raLeft, MAX(raRight - 1, raLeft));
			p_rect.position.y = CLAMP(p_rect.position.y, raBottom, MAX(raTop - 1, raBottom));
			p_rect.size.width = MIN(p_rect.size.width, raRight - p_rect.position.x);
			p_rect.size.height = MIN(p_rect.size.height, raTop - p_rect.position.y);

			return p_rect;
		}

	} render;

	// State specific for a compute pass.
	struct ComputeState {
		MDComputePipeline *pipeline = nullptr;
		id<MTLComputeCommandEncoder> encoder = nil;
		ResourceUsageMap resource_usage;
		_FORCE_INLINE_ void reset() {
			pipeline = nil;
			encoder = nil;
			// Keep the keys, as they are likely to be used again.
			for (KeyValue<StageResourceUsage, LocalVector<__unsafe_unretained id<MTLResource>>> &kv : resource_usage) {
				kv.value.clear();
			}
		}

		void end_encoding();
	} compute;

	// State specific to a blit pass.
	struct {
		id<MTLBlitCommandEncoder> encoder = nil;
		_FORCE_INLINE_ void reset() {
			encoder = nil;
		}
	} blit;

	_FORCE_INLINE_ id<MTLCommandBuffer> get_command_buffer() const {
		return commandBuffer;
	}

	void begin();
	void commit();
	void end();

	id<MTLBlitCommandEncoder> blit_command_encoder();
	void encodeRenderCommandEncoderWithDescriptor(MTLRenderPassDescriptor *p_desc, NSString *p_label);

	void bind_pipeline(RDD::PipelineID p_pipeline);

#pragma mark - Render Commands

	void render_bind_uniform_set(RDD::UniformSetID p_uniform_set, RDD::ShaderID p_shader, uint32_t p_set_index);
	void render_clear_attachments(VectorView<RDD::AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects);
	void render_set_viewport(VectorView<Rect2i> p_viewports);
	void render_set_scissor(VectorView<Rect2i> p_scissors);
	void render_set_blend_constants(const Color &p_constants);
	void render_begin_pass(RDD::RenderPassID p_render_pass,
			RDD::FramebufferID p_frameBuffer,
			RDD::CommandBufferType p_cmd_buffer_type,
			const Rect2i &p_rect,
			VectorView<RDD::RenderPassClearValue> p_clear_values);
	void render_next_subpass();
	void render_draw(uint32_t p_vertex_count,
			uint32_t p_instance_count,
			uint32_t p_base_vertex,
			uint32_t p_first_instance);
	void render_bind_vertex_buffers(uint32_t p_binding_count, const RDD::BufferID *p_buffers, const uint64_t *p_offsets);
	void render_bind_index_buffer(RDD::BufferID p_buffer, RDD::IndexBufferFormat p_format, uint64_t p_offset);

	void render_draw_indexed(uint32_t p_index_count,
			uint32_t p_instance_count,
			uint32_t p_first_index,
			int32_t p_vertex_offset,
			uint32_t p_first_instance);

	void render_draw_indexed_indirect(RDD::BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride);
	void render_draw_indexed_indirect_count(RDD::BufferID p_indirect_buffer, uint64_t p_offset, RDD::BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride);
	void render_draw_indirect(RDD::BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride);
	void render_draw_indirect_count(RDD::BufferID p_indirect_buffer, uint64_t p_offset, RDD::BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride);

	void render_end_pass();

#pragma mark - Compute Commands

	void compute_bind_uniform_set(RDD::UniformSetID p_uniform_set, RDD::ShaderID p_shader, uint32_t p_set_index);
	void compute_dispatch(uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups);
	void compute_dispatch_indirect(RDD::BufferID p_indirect_buffer, uint64_t p_offset);

	MDCommandBuffer(id<MTLCommandQueue> p_queue, RenderingDeviceDriverMetal *p_device_driver) :
			device_driver(p_device_driver), queue(p_queue) {
		type = MDCommandBufferStateType::None;
	}

	MDCommandBuffer() = default;
};

#if (TARGET_OS_OSX && __MAC_OS_X_VERSION_MAX_ALLOWED < 140000) || (TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED < 170000)
#define MTLBindingAccess MTLArgumentAccess
#define MTLBindingAccessReadOnly MTLArgumentAccessReadOnly
#define MTLBindingAccessReadWrite MTLArgumentAccessReadWrite
#define MTLBindingAccessWriteOnly MTLArgumentAccessWriteOnly
#endif

struct API_AVAILABLE(macos(11.0), ios(14.0)) BindingInfo {
	MTLDataType dataType = MTLDataTypeNone;
	uint32_t index = 0;
	MTLBindingAccess access = MTLBindingAccessReadOnly;
	MTLResourceUsage usage = 0;
	MTLTextureType textureType = MTLTextureType2D;
	spv::ImageFormat imageFormat = spv::ImageFormatUnknown;
	uint32_t arrayLength = 0;
	bool isMultisampled = false;

	inline MTLArgumentDescriptor *new_argument_descriptor() const {
		MTLArgumentDescriptor *desc = MTLArgumentDescriptor.argumentDescriptor;
		desc.dataType = dataType;
		desc.index = index;
		desc.access = access;
		desc.textureType = textureType;
		desc.arrayLength = arrayLength;
		return desc;
	}

	size_t serialize_size() const {
		return sizeof(uint32_t) * 8 /* 8 uint32_t fields */;
	}

	template <typename W>
	void serialize(W &p_writer) const {
		p_writer.write((uint32_t)dataType);
		p_writer.write(index);
		p_writer.write((uint32_t)access);
		p_writer.write((uint32_t)usage);
		p_writer.write((uint32_t)textureType);
		p_writer.write(imageFormat);
		p_writer.write(arrayLength);
		p_writer.write(isMultisampled);
	}

	template <typename R>
	void deserialize(R &p_reader) {
		p_reader.read((uint32_t &)dataType);
		p_reader.read(index);
		p_reader.read((uint32_t &)access);
		p_reader.read((uint32_t &)usage);
		p_reader.read((uint32_t &)textureType);
		p_reader.read((uint32_t &)imageFormat);
		p_reader.read(arrayLength);
		p_reader.read(isMultisampled);
	}
};

using RDC = RenderingDeviceCommons;

typedef API_AVAILABLE(macos(11.0), ios(14.0)) HashMap<RDC::ShaderStage, BindingInfo> BindingInfoMap;

struct API_AVAILABLE(macos(11.0), ios(14.0)) UniformInfo {
	uint32_t binding;
	ShaderStageUsage active_stages = None;
	BindingInfoMap bindings;
	BindingInfoMap bindings_secondary;
};

struct API_AVAILABLE(macos(11.0), ios(14.0)) UniformSet {
	LocalVector<UniformInfo> uniforms;
	uint32_t buffer_size = 0;
	HashMap<RDC::ShaderStage, uint32_t> offsets;
	HashMap<RDC::ShaderStage, id<MTLArgumentEncoder>> encoders;
};

struct ShaderCacheEntry;

enum class ShaderLoadStrategy {
	DEFAULT,
	LAZY,
};

/// A Metal shader library.
@interface MDLibrary : NSObject {
	ShaderCacheEntry *_entry;
};
- (id<MTLLibrary>)library;
- (NSError *)error;
- (void)setLabel:(NSString *)label;

+ (instancetype)newLibraryWithCacheEntry:(ShaderCacheEntry *)entry
								  device:(id<MTLDevice>)device
								  source:(NSString *)source
								 options:(MTLCompileOptions *)options
								strategy:(ShaderLoadStrategy)strategy;
@end

struct SHA256Digest {
	unsigned char data[CC_SHA256_DIGEST_LENGTH];

	uint32_t hash() const {
		uint32_t c = crc32(0, data, CC_SHA256_DIGEST_LENGTH);
		return c;
	}

	SHA256Digest() {
		bzero(data, CC_SHA256_DIGEST_LENGTH);
	}

	SHA256Digest(const char *p_data, size_t p_length) {
		CC_SHA256(p_data, (CC_LONG)p_length, data);
	}

	_FORCE_INLINE_ uint32_t short_sha() const {
		return __builtin_bswap32(*(uint32_t *)&data[0]);
	}
};

template <>
struct HashMapComparatorDefault<SHA256Digest> {
	static bool compare(const SHA256Digest &p_lhs, const SHA256Digest &p_rhs) {
		return memcmp(p_lhs.data, p_rhs.data, CC_SHA256_DIGEST_LENGTH) == 0;
	}
};

/// A cache entry for a Metal shader library.
struct ShaderCacheEntry {
	RenderingDeviceDriverMetal &owner;
	/// A hash of the Metal shader source code.
	SHA256Digest key;
	CharString name;
	RD::ShaderStage stage = RD::SHADER_STAGE_VERTEX;
	/// This reference must be weak, to ensure that when the last strong reference to the library
	/// is released, the cache entry is freed.
	MDLibrary *__weak library = nil;

	/// Notify the cache that this entry is no longer needed.
	void notify_free() const;

	ShaderCacheEntry(RenderingDeviceDriverMetal &p_owner, SHA256Digest p_key) :
			owner(p_owner), key(p_key) {
	}
	~ShaderCacheEntry() = default;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDShader {
public:
	CharString name;
	Vector<UniformSet> sets;

	virtual void encode_push_constant_data(VectorView<uint32_t> p_data, MDCommandBuffer *p_cb) = 0;

	MDShader(CharString p_name, Vector<UniformSet> p_sets) :
			name(p_name), sets(p_sets) {}
	virtual ~MDShader() = default;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDComputeShader final : public MDShader {
public:
	struct {
		uint32_t binding = -1;
		uint32_t size = 0;
	} push_constants;
	MTLSize local = {};

	MDLibrary *kernel;
#if DEV_ENABLED
	CharString kernel_source;
#endif

	void encode_push_constant_data(VectorView<uint32_t> p_data, MDCommandBuffer *p_cb) final;

	MDComputeShader(CharString p_name, Vector<UniformSet> p_sets, MDLibrary *p_kernel);
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDRenderShader final : public MDShader {
public:
	struct {
		struct {
			int32_t binding = -1;
			uint32_t size = 0;
		} vert;
		struct {
			int32_t binding = -1;
			uint32_t size = 0;
		} frag;
	} push_constants;

	MDLibrary *vert;
	MDLibrary *frag;
#if DEV_ENABLED
	CharString vert_source;
	CharString frag_source;
#endif

	void encode_push_constant_data(VectorView<uint32_t> p_data, MDCommandBuffer *p_cb) final;

	MDRenderShader(CharString p_name, Vector<UniformSet> p_sets, MDLibrary *p_vert, MDLibrary *p_frag);
};

_FORCE_INLINE_ StageResourceUsage &operator|=(StageResourceUsage &p_a, uint32_t p_b) {
	p_a = StageResourceUsage(uint32_t(p_a) | p_b);
	return p_a;
}

_FORCE_INLINE_ StageResourceUsage stage_resource_usage(RDC::ShaderStage p_stage, MTLResourceUsage p_usage) {
	return StageResourceUsage(p_usage << (p_stage * 2));
}

_FORCE_INLINE_ MTLResourceUsage resource_usage_for_stage(StageResourceUsage p_usage, RDC::ShaderStage p_stage) {
	return MTLResourceUsage((p_usage >> (p_stage * 2)) & 0b11);
}

template <>
struct HashMapComparatorDefault<RDD::ShaderID> {
	static bool compare(const RDD::ShaderID &p_lhs, const RDD::ShaderID &p_rhs) {
		return p_lhs.id == p_rhs.id;
	}
};

struct BoundUniformSet {
	id<MTLBuffer> buffer;
	ResourceUsageMap usage_to_resources;

	/// Perform a 2-way merge each key of `ResourceVector` resources from this set into the
	/// destination set.
	///
	/// Assumes the vectors of resources are sorted.
	void merge_into(ResourceUsageMap &p_dst) const;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDUniformSet {
public:
	uint32_t index;
	LocalVector<RDD::BoundUniform> uniforms;
	HashMap<MDShader *, BoundUniformSet> bound_uniforms;

	BoundUniformSet &boundUniformSetForShader(MDShader *p_shader, id<MTLDevice> p_device);
};

enum class MDAttachmentType : uint8_t {
	None = 0,
	Color = 1 << 0,
	Depth = 1 << 1,
	Stencil = 1 << 2,
};

_FORCE_INLINE_ MDAttachmentType &operator|=(MDAttachmentType &p_a, MDAttachmentType p_b) {
	flags::set(p_a, p_b);
	return p_a;
}

_FORCE_INLINE_ bool operator&(MDAttachmentType p_a, MDAttachmentType p_b) {
	return uint8_t(p_a) & uint8_t(p_b);
}

struct MDSubpass {
	uint32_t subpass_index = 0;
	LocalVector<RDD::AttachmentReference> input_references;
	LocalVector<RDD::AttachmentReference> color_references;
	RDD::AttachmentReference depth_stencil_reference;
	LocalVector<RDD::AttachmentReference> resolve_references;

	MTLFmtCaps getRequiredFmtCapsForAttachmentAt(uint32_t p_index) const;
};

struct API_AVAILABLE(macos(11.0), ios(14.0)) MDAttachment {
private:
	uint32_t index = 0;
	uint32_t firstUseSubpassIndex = 0;
	uint32_t lastUseSubpassIndex = 0;

public:
	MTLPixelFormat format = MTLPixelFormatInvalid;
	MDAttachmentType type = MDAttachmentType::None;
	MTLLoadAction loadAction = MTLLoadActionDontCare;
	MTLStoreAction storeAction = MTLStoreActionDontCare;
	MTLLoadAction stencilLoadAction = MTLLoadActionDontCare;
	MTLStoreAction stencilStoreAction = MTLStoreActionDontCare;
	uint32_t samples = 1;

	/*!
	 * @brief Returns true if this attachment is first used in the given subpass.
	 * @param p_subpass
	 * @return
	 */
	_FORCE_INLINE_ bool isFirstUseOf(MDSubpass const &p_subpass) const {
		return p_subpass.subpass_index == firstUseSubpassIndex;
	}

	/*!
	 * @brief Returns true if this attachment is last used in the given subpass.
	 * @param p_subpass
	 * @return
	 */
	_FORCE_INLINE_ bool isLastUseOf(MDSubpass const &p_subpass) const {
		return p_subpass.subpass_index == lastUseSubpassIndex;
	}

	void linkToSubpass(MDRenderPass const &p_pass);

	MTLStoreAction getMTLStoreAction(MDSubpass const &p_subpass,
			bool p_is_rendering_entire_area,
			bool p_has_resolve,
			bool p_can_resolve,
			bool p_is_stencil) const;
	bool configureDescriptor(MTLRenderPassAttachmentDescriptor *p_desc,
			PixelFormats &p_pf,
			MDSubpass const &p_subpass,
			id<MTLTexture> p_attachment,
			bool p_is_rendering_entire_area,
			bool p_has_resolve,
			bool p_can_resolve,
			bool p_is_stencil) const;
	/** Returns whether this attachment should be cleared in the subpass. */
	bool shouldClear(MDSubpass const &p_subpass, bool p_is_stencil) const;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDRenderPass {
public:
	Vector<MDAttachment> attachments;
	Vector<MDSubpass> subpasses;

	uint32_t get_sample_count() const {
		return attachments.is_empty() ? 1 : attachments[0].samples;
	}

	MDRenderPass(Vector<MDAttachment> &p_attachments, Vector<MDSubpass> &p_subpasses);
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDPipeline {
public:
	MDPipelineType type;

	explicit MDPipeline(MDPipelineType p_type) :
			type(p_type) {}
	virtual ~MDPipeline() = default;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDRenderPipeline final : public MDPipeline {
public:
	id<MTLRenderPipelineState> state = nil;
	id<MTLDepthStencilState> depth_stencil = nil;
	uint32_t push_constant_size = 0;
	uint32_t push_constant_stages_mask = 0;
	SampleCount sample_count = SampleCount1;

	struct {
		MTLCullMode cull_mode = MTLCullModeNone;
		MTLTriangleFillMode fill_mode = MTLTriangleFillModeFill;
		MTLDepthClipMode clip_mode = MTLDepthClipModeClip;
		MTLWinding winding = MTLWindingClockwise;
		MTLPrimitiveType render_primitive = MTLPrimitiveTypePoint;

		struct {
			bool enabled = false;
		} depth_test;

		struct {
			bool enabled = false;
			float depth_bias = 0.0;
			float slope_scale = 0.0;
			float clamp = 0.0;
			_FORCE_INLINE_ void apply(id<MTLRenderCommandEncoder> __unsafe_unretained p_enc) const {
				if (!enabled) {
					return;
				}
				[p_enc setDepthBias:depth_bias slopeScale:slope_scale clamp:clamp];
			}
		} depth_bias;

		struct {
			bool enabled = false;
			uint32_t front_reference = 0;
			uint32_t back_reference = 0;
			_FORCE_INLINE_ void apply(id<MTLRenderCommandEncoder> __unsafe_unretained p_enc) const {
				if (!enabled)
					return;
				[p_enc setStencilFrontReferenceValue:front_reference backReferenceValue:back_reference];
			};
		} stencil;

		struct {
			bool enabled = false;
			float r = 0.0;
			float g = 0.0;
			float b = 0.0;
			float a = 0.0;

			_FORCE_INLINE_ void apply(id<MTLRenderCommandEncoder> __unsafe_unretained p_enc) const {
				//if (!enabled)
				//	return;
				[p_enc setBlendColorRed:r green:g blue:b alpha:a];
			};
		} blend;

		_FORCE_INLINE_ void apply(id<MTLRenderCommandEncoder> __unsafe_unretained p_enc) const {
			[p_enc setCullMode:cull_mode];
			[p_enc setTriangleFillMode:fill_mode];
			[p_enc setDepthClipMode:clip_mode];
			[p_enc setFrontFacingWinding:winding];
			depth_bias.apply(p_enc);
			stencil.apply(p_enc);
			blend.apply(p_enc);
		}

	} raster_state;

	MDRenderShader *shader = nil;

	MDRenderPipeline() :
			MDPipeline(MDPipelineType::Render) {}
	~MDRenderPipeline() final = default;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDComputePipeline final : public MDPipeline {
public:
	id<MTLComputePipelineState> state = nil;
	struct {
		MTLSize local = {};
	} compute_state;

	MDComputeShader *shader = nil;

	explicit MDComputePipeline(id<MTLComputePipelineState> p_state) :
			MDPipeline(MDPipelineType::Compute), state(p_state) {}
	~MDComputePipeline() final = default;
};

class API_AVAILABLE(macos(11.0), ios(14.0)) MDFrameBuffer {
public:
	Vector<MTL::Texture> textures;
	Size2i size;
	MDFrameBuffer(Vector<MTL::Texture> p_textures, Size2i p_size) :
			textures(p_textures), size(p_size) {}
	MDFrameBuffer() {}

	virtual ~MDFrameBuffer() = default;
};

// These functions are used to convert between Objective-C objects and
// the RIDs used by Godot, respecting automatic reference counting.
namespace rid {

// Converts an Objective-C object to a pointer, and incrementing the
// reference count.
_FORCE_INLINE_
void *owned(id p_id) {
	return (__bridge_retained void *)p_id;
}

#define MAKE_ID(FROM, TO) \
	_FORCE_INLINE_ TO make(FROM p_obj) { return TO(owned(p_obj)); }

MAKE_ID(id<MTLTexture>, RDD::TextureID)
MAKE_ID(id<MTLBuffer>, RDD::BufferID)
MAKE_ID(id<MTLSamplerState>, RDD::SamplerID)
MAKE_ID(MTLVertexDescriptor *, RDD::VertexFormatID)
MAKE_ID(id<MTLCommandQueue>, RDD::CommandPoolID)

// Converts a pointer to an Objective-C object without changing the reference count.
_FORCE_INLINE_
auto get(RDD::ID p_id) {
	return (p_id.id) ? (__bridge ::id)(void *)p_id.id : nil;
}

// Converts a pointer to an Objective-C object, and decrements the reference count.
_FORCE_INLINE_
auto release(RDD::ID p_id) {
	return (__bridge_transfer ::id)(void *)p_id.id;
}

} // namespace rid

#endif // METAL_OBJECTS_H
