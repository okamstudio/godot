/**************************************************************************/
/*  rendering_device_driver_metal.mm                                      */
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

#import "rendering_device_driver_metal.h"

#import "pixel_formats.h"
#import "rendering_context_driver_metal.h"

#import "core/io/compression.h"
#import "core/io/marshalls.h"
#import "core/string/ustring.h"
#import "core/templates/hash_map.h"

#import <Metal/MTLTexture.h>
#import <Metal/Metal.h>
#import <os/log.h>
#import <os/signpost.h>
#import <spirv_msl.hpp>
#import <spirv_parser.hpp>

#pragma mark - Logging

os_log_t LOG_DRIVER;
// Used for dynamic tracing.
os_log_t LOG_INTERVALS;

__attribute__((constructor)) static void InitializeLogging(void) {
	LOG_DRIVER = os_log_create("org.godotengine.godot.metal", OS_LOG_CATEGORY_POINTS_OF_INTEREST);
	LOG_INTERVALS = os_log_create("org.godotengine.godot.metal", "events");
}

/*****************/
/**** GENERIC ****/
/*****************/

// RDD::CompareOperator == VkCompareOp.
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_NEVER, MTLCompareFunctionNever));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_LESS, MTLCompareFunctionLess));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_EQUAL, MTLCompareFunctionEqual));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_LESS_OR_EQUAL, MTLCompareFunctionLessEqual));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_GREATER, MTLCompareFunctionGreater));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_NOT_EQUAL, MTLCompareFunctionNotEqual));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_GREATER_OR_EQUAL, MTLCompareFunctionGreaterEqual));
static_assert(ENUM_MEMBERS_EQUAL(RDD::COMPARE_OP_ALWAYS, MTLCompareFunctionAlways));

_FORCE_INLINE_ MTLSize mipmapLevelSizeFromTexture(id<MTLTexture> p_tex, NSUInteger p_level) {
	MTLSize lvlSize;
	lvlSize.width = MAX(p_tex.width >> p_level, 1UL);
	lvlSize.height = MAX(p_tex.height >> p_level, 1UL);
	lvlSize.depth = MAX(p_tex.depth >> p_level, 1UL);
	return lvlSize;
}

_FORCE_INLINE_ MTLSize mipmapLevelSizeFromSize(MTLSize p_size, NSUInteger p_level) {
	if (p_level == 0) {
		return p_size;
	}

	MTLSize lvlSize;
	lvlSize.width = MAX(p_size.width >> p_level, 1UL);
	lvlSize.height = MAX(p_size.height >> p_level, 1UL);
	lvlSize.depth = MAX(p_size.depth >> p_level, 1UL);
	return lvlSize;
}

_FORCE_INLINE_ static bool operator==(MTLSize p_a, MTLSize p_b) {
	return p_a.width == p_b.width && p_a.height == p_b.height && p_a.depth == p_b.depth;
}

/*****************/
/**** BUFFERS ****/
/*****************/

RDD::BufferID RenderingDeviceDriverMetal::buffer_create(uint64_t p_size, BitField<BufferUsageBits> p_usage, MemoryAllocationType p_allocation_type) {
	MTLResourceOptions options = MTLResourceHazardTrackingModeTracked;
	switch (p_allocation_type) {
		case MEMORY_ALLOCATION_TYPE_CPU:
			options |= MTLResourceStorageModeShared;
			break;
		case MEMORY_ALLOCATION_TYPE_GPU:
			options |= MTLResourceStorageModePrivate;
			break;
	}

	id<MTLBuffer> obj = [device newBufferWithLength:p_size options:options];
	ERR_FAIL_NULL_V_MSG(obj, BufferID(), "Can't create buffer of size: " + itos(p_size));
	return rid::make(obj);
}

bool RenderingDeviceDriverMetal::buffer_set_texel_format(BufferID p_buffer, DataFormat p_format) {
	// Nothing to do.
	return true;
}

void RenderingDeviceDriverMetal::buffer_free(BufferID p_buffer) {
	rid::release(p_buffer);
}

uint64_t RenderingDeviceDriverMetal::buffer_get_allocation_size(BufferID p_buffer) {
	id<MTLBuffer> obj = rid::get(p_buffer);
	return obj.allocatedSize;
}

uint8_t *RenderingDeviceDriverMetal::buffer_map(BufferID p_buffer) {
	id<MTLBuffer> obj = rid::get(p_buffer);
	ERR_FAIL_COND_V_MSG(obj.storageMode != MTLStorageModeShared, nullptr, "Unable to map private buffers");
	return (uint8_t *)obj.contents;
}

void RenderingDeviceDriverMetal::buffer_unmap(BufferID p_buffer) {
	// Nothing to do.
}

#pragma mark - Texture

#pragma mark - Format Conversions

static const MTLTextureType TEXTURE_TYPE[RD::TEXTURE_TYPE_MAX] = {
	MTLTextureType1D,
	MTLTextureType2D,
	MTLTextureType3D,
	MTLTextureTypeCube,
	MTLTextureType1DArray,
	MTLTextureType2DArray,
	MTLTextureTypeCubeArray,
};

RenderingDeviceDriverMetal::Result<bool> RenderingDeviceDriverMetal::is_valid_linear(TextureFormat const &p_format) const {
	if (!flags::any(p_format.usage_bits, TEXTURE_USAGE_CPU_READ_BIT)) {
		return false;
	}

	PixelFormats &pf = *pixel_formats;
	MTLFormatType ft = pf.getFormatType(p_format.format);

	// Requesting a linear format, which has further restrictions, similar to Vulkan
	// when specifying VK_IMAGE_TILING_LINEAR.

	ERR_FAIL_COND_V_MSG(p_format.texture_type != TEXTURE_TYPE_2D, ERR_CANT_CREATE, "Linear (TEXTURE_USAGE_CPU_READ_BIT) textures must be 2D");
	ERR_FAIL_COND_V_MSG(ft != MTLFormatType::DepthStencil, ERR_CANT_CREATE, "Linear (TEXTURE_USAGE_CPU_READ_BIT) textures must not be a depth/stencil format");
	ERR_FAIL_COND_V_MSG(ft != MTLFormatType::Compressed, ERR_CANT_CREATE, "Linear (TEXTURE_USAGE_CPU_READ_BIT) textures must not be a compressed format");
	ERR_FAIL_COND_V_MSG(p_format.mipmaps != 1, ERR_CANT_CREATE, "Linear (TEXTURE_USAGE_CPU_READ_BIT) textures must have 1 mipmap level");
	ERR_FAIL_COND_V_MSG(p_format.array_layers != 1, ERR_CANT_CREATE, "Linear (TEXTURE_USAGE_CPU_READ_BIT) textures must have 1 array layer");
	ERR_FAIL_COND_V_MSG(p_format.samples != TEXTURE_SAMPLES_1, ERR_CANT_CREATE, "Linear (TEXTURE_USAGE_CPU_READ_BIT) textures must have 1 sample");

	return true;
}

RDD::TextureID RenderingDeviceDriverMetal::texture_create(const TextureFormat &p_format, const TextureView &p_view) {
	MTLTextureDescriptor *desc = [MTLTextureDescriptor new];
	desc.textureType = TEXTURE_TYPE[p_format.texture_type];

	PixelFormats &formats = *pixel_formats;
	desc.pixelFormat = formats.getMTLPixelFormat(p_format.format);
	MTLFmtCaps format_caps = formats.getCapabilities(desc.pixelFormat);

	desc.width = p_format.width;
	desc.height = p_format.height;
	desc.depth = p_format.depth;
	desc.mipmapLevelCount = p_format.mipmaps;

	if (p_format.texture_type == TEXTURE_TYPE_1D_ARRAY ||
			p_format.texture_type == TEXTURE_TYPE_2D_ARRAY) {
		desc.arrayLength = p_format.array_layers;
	} else if (p_format.texture_type == TEXTURE_TYPE_CUBE_ARRAY) {
		desc.arrayLength = p_format.array_layers / 6;
	}

	// TODO(sgc): Evaluate lossy texture support (perhaps as a project option?)
	//  https://developer.apple.com/videos/play/tech-talks/10876?time=459
	// desc.compressionType = MTLTextureCompressionTypeLossy;

	if (p_format.samples > TEXTURE_SAMPLES_1) {
		SampleCount supported = (*device_properties).find_nearest_supported_sample_count(p_format.samples);

		if (supported > SampleCount1) {
			bool ok = p_format.texture_type == TEXTURE_TYPE_2D || p_format.texture_type == TEXTURE_TYPE_2D_ARRAY;
			if (ok) {
				switch (p_format.texture_type) {
					case TEXTURE_TYPE_2D:
						desc.textureType = MTLTextureType2DMultisample;
						break;
					case TEXTURE_TYPE_2D_ARRAY:
						desc.textureType = MTLTextureType2DMultisampleArray;
						break;
					default:
						break;
				}
				desc.sampleCount = (NSUInteger)supported;
				if (p_format.mipmaps > 1) {
					// For a buffer-backed or multi-sample texture, the value must be 1.
					WARN_PRINT("mipmaps == 1 for multi-sample textures");
					desc.mipmapLevelCount = 1;
				}
			} else {
				WARN_PRINT("Unsupported multi-sample texture type; disabling multi-sample");
			}
		}
	}

	static const MTLTextureSwizzle COMPONENT_SWIZZLE[TEXTURE_SWIZZLE_MAX] = {
		static_cast<MTLTextureSwizzle>(255), // IDENTITY
		MTLTextureSwizzleZero,
		MTLTextureSwizzleOne,
		MTLTextureSwizzleRed,
		MTLTextureSwizzleGreen,
		MTLTextureSwizzleBlue,
		MTLTextureSwizzleAlpha,
	};

	MTLTextureSwizzleChannels swizzle = MTLTextureSwizzleChannelsMake(
			p_view.swizzle_r != TEXTURE_SWIZZLE_IDENTITY ? COMPONENT_SWIZZLE[p_view.swizzle_r] : MTLTextureSwizzleRed,
			p_view.swizzle_g != TEXTURE_SWIZZLE_IDENTITY ? COMPONENT_SWIZZLE[p_view.swizzle_g] : MTLTextureSwizzleGreen,
			p_view.swizzle_b != TEXTURE_SWIZZLE_IDENTITY ? COMPONENT_SWIZZLE[p_view.swizzle_b] : MTLTextureSwizzleBlue,
			p_view.swizzle_a != TEXTURE_SWIZZLE_IDENTITY ? COMPONENT_SWIZZLE[p_view.swizzle_a] : MTLTextureSwizzleAlpha);

	// Represents a swizzle operation that is a no-op.
	static MTLTextureSwizzleChannels IDENTITY_SWIZZLE = {
		.red = MTLTextureSwizzleRed,
		.green = MTLTextureSwizzleGreen,
		.blue = MTLTextureSwizzleBlue,
		.alpha = MTLTextureSwizzleAlpha,
	};

	bool no_swizzle = memcmp(&IDENTITY_SWIZZLE, &swizzle, sizeof(MTLTextureSwizzleChannels)) == 0;
	if (!no_swizzle) {
		desc.swizzle = swizzle;
	}

	// Usage.

	MTLResourceOptions options = 0;
	const bool supports_memoryless = (*device_properties).features.highestFamily >= MTLGPUFamilyApple2 && (*device_properties).features.highestFamily < MTLGPUFamilyMac1;
	if (supports_memoryless && p_format.usage_bits & TEXTURE_USAGE_TRANSIENT_BIT) {
		options = MTLResourceStorageModeMemoryless | MTLResourceHazardTrackingModeTracked;
		desc.storageMode = MTLStorageModeMemoryless;
	} else {
		options = MTLResourceCPUCacheModeDefaultCache | MTLResourceHazardTrackingModeTracked;
		if (p_format.usage_bits & TEXTURE_USAGE_CPU_READ_BIT) {
			options |= MTLResourceStorageModeShared;
		} else {
			options |= MTLResourceStorageModePrivate;
		}
	}
	desc.resourceOptions = options;

	if (p_format.usage_bits & TEXTURE_USAGE_SAMPLING_BIT) {
		desc.usage |= MTLTextureUsageShaderRead;
	}

	if (p_format.usage_bits & TEXTURE_USAGE_STORAGE_BIT) {
		desc.usage |= MTLTextureUsageShaderWrite;
	}

	if (p_format.usage_bits & TEXTURE_USAGE_STORAGE_ATOMIC_BIT) {
		desc.usage |= MTLTextureUsageShaderWrite;
	}

	bool can_be_attachment = flags::any(format_caps, (kMTLFmtCapsColorAtt | kMTLFmtCapsDSAtt));

	if (flags::any(p_format.usage_bits, TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) &&
			can_be_attachment) {
		desc.usage |= MTLTextureUsageRenderTarget;
	}

	if (p_format.usage_bits & TEXTURE_USAGE_INPUT_ATTACHMENT_BIT) {
		desc.usage |= MTLTextureUsageShaderRead;
	}

	if (p_format.usage_bits & TEXTURE_USAGE_VRS_ATTACHMENT_BIT) {
		ERR_FAIL_V_MSG(RDD::TextureID(), "unsupported: TEXTURE_USAGE_VRS_ATTACHMENT_BIT");
	}

	if (flags::any(p_format.usage_bits, TEXTURE_USAGE_CAN_UPDATE_BIT | TEXTURE_USAGE_CAN_COPY_TO_BIT) &&
			can_be_attachment && no_swizzle) {
		// Per MoltenVK, can be cleared as a render attachment.
		desc.usage |= MTLTextureUsageRenderTarget;
	}
	if (p_format.usage_bits & TEXTURE_USAGE_CAN_COPY_FROM_BIT) {
		// Covered by blits.
	}

	// Create texture views with a different component layout.
	if (!p_format.shareable_formats.is_empty()) {
		desc.usage |= MTLTextureUsagePixelFormatView;
	}

	// Allocate memory.

	bool is_linear;
	{
		Result<bool> is_linear_or_err = is_valid_linear(p_format);
		ERR_FAIL_COND_V(std::holds_alternative<Error>(is_linear_or_err), TextureID());
		is_linear = std::get<bool>(is_linear_or_err);
	}

	// Check if it is a linear format for atomic operations and therefore needs a buffer,
	// as generally Metal does not support atomic operations on textures.
	bool needs_buffer = is_linear || (p_format.array_layers == 1 && p_format.mipmaps == 1 && p_format.texture_type == TEXTURE_TYPE_2D && flags::any(p_format.usage_bits, TEXTURE_USAGE_STORAGE_BIT) && (p_format.format == DATA_FORMAT_R32_UINT || p_format.format == DATA_FORMAT_R32_SINT));

	id<MTLTexture> obj = nil;
	if (needs_buffer) {
		// Linear textures are restricted to 2D textures, a single mipmap level and a single array layer.
		MTLPixelFormat pixel_format = desc.pixelFormat;
		size_t row_alignment = get_texel_buffer_alignment_for_format(p_format.format);
		size_t bytes_per_row = formats.getBytesPerRow(pixel_format, p_format.width);
		bytes_per_row = round_up_to_alignment(bytes_per_row, row_alignment);
		size_t bytes_per_layer = formats.getBytesPerLayer(pixel_format, bytes_per_row, p_format.height);
		size_t byte_count = bytes_per_layer * p_format.depth * p_format.array_layers;

		id<MTLBuffer> buf = [device newBufferWithLength:byte_count options:options];
		obj = [buf newTextureWithDescriptor:desc offset:0 bytesPerRow:bytes_per_row];
	} else {
		obj = [device newTextureWithDescriptor:desc];
	}
	ERR_FAIL_NULL_V_MSG(obj, TextureID(), "Unable to create texture.");

	return rid::make(obj);
}

RDD::TextureID RenderingDeviceDriverMetal::texture_create_from_extension(uint64_t p_native_texture, TextureType p_type, DataFormat p_format, uint32_t p_array_layers, bool p_depth_stencil) {
	id<MTLTexture> res = (__bridge id<MTLTexture>)(void *)(uintptr_t)p_native_texture;

	// If the requested format is different, we need to create a view.
	MTLPixelFormat format = pixel_formats->getMTLPixelFormat(p_format);
	if (res.pixelFormat != format) {
		MTLTextureSwizzleChannels swizzle = MTLTextureSwizzleChannelsMake(
				MTLTextureSwizzleRed,
				MTLTextureSwizzleGreen,
				MTLTextureSwizzleBlue,
				MTLTextureSwizzleAlpha);
		res = [res newTextureViewWithPixelFormat:format
									 textureType:res.textureType
										  levels:NSMakeRange(0, res.mipmapLevelCount)
										  slices:NSMakeRange(0, p_array_layers)
										 swizzle:swizzle];
		ERR_FAIL_NULL_V_MSG(res, TextureID(), "Unable to create texture view.");
	}

	return rid::make(res);
}

RDD::TextureID RenderingDeviceDriverMetal::texture_create_shared(TextureID p_original_texture, const TextureView &p_view) {
	id<MTLTexture> src_texture = rid::get(p_original_texture);

#if DEV_ENABLED
	if (src_texture.sampleCount > 1) {
		// TODO(sgc): is it ok to create a shared texture from a multi-sample texture?
		WARN_PRINT("Is it safe to create a shared texture from multi-sample texture?");
	}
#endif

	MTLPixelFormat format = pixel_formats->getMTLPixelFormat(p_view.format);

	static const MTLTextureSwizzle component_swizzle[TEXTURE_SWIZZLE_MAX] = {
		static_cast<MTLTextureSwizzle>(255), // IDENTITY
		MTLTextureSwizzleZero,
		MTLTextureSwizzleOne,
		MTLTextureSwizzleRed,
		MTLTextureSwizzleGreen,
		MTLTextureSwizzleBlue,
		MTLTextureSwizzleAlpha,
	};

#define SWIZZLE(C, CHAN) (p_view.swizzle_##C != TEXTURE_SWIZZLE_IDENTITY ? component_swizzle[p_view.swizzle_##C] : MTLTextureSwizzle##CHAN)
	MTLTextureSwizzleChannels swizzle = MTLTextureSwizzleChannelsMake(
			SWIZZLE(r, Red),
			SWIZZLE(g, Green),
			SWIZZLE(b, Blue),
			SWIZZLE(a, Alpha));
#undef SWIZZLE
	id<MTLTexture> obj = [src_texture newTextureViewWithPixelFormat:format
														textureType:src_texture.textureType
															 levels:NSMakeRange(0, src_texture.mipmapLevelCount)
															 slices:NSMakeRange(0, src_texture.arrayLength)
															swizzle:swizzle];
	ERR_FAIL_NULL_V_MSG(obj, TextureID(), "Unable to create shared texture");
	return rid::make(obj);
}

RDD::TextureID RenderingDeviceDriverMetal::texture_create_shared_from_slice(TextureID p_original_texture, const TextureView &p_view, TextureSliceType p_slice_type, uint32_t p_layer, uint32_t p_layers, uint32_t p_mipmap, uint32_t p_mipmaps) {
	id<MTLTexture> src_texture = rid::get(p_original_texture);

	static const MTLTextureType VIEW_TYPES[] = {
		MTLTextureType1D, // MTLTextureType1D
		MTLTextureType1D, // MTLTextureType1DArray
		MTLTextureType2D, // MTLTextureType2D
		MTLTextureType2D, // MTLTextureType2DArray
		MTLTextureType2D, // MTLTextureType2DMultisample
		MTLTextureType2D, // MTLTextureTypeCube
		MTLTextureType2D, // MTLTextureTypeCubeArray
		MTLTextureType2D, // MTLTextureType3D
		MTLTextureType2D, // MTLTextureType2DMultisampleArray
	};

	MTLTextureType textureType = VIEW_TYPES[src_texture.textureType];
	switch (p_slice_type) {
		case TEXTURE_SLICE_2D: {
			textureType = MTLTextureType2D;
		} break;
		case TEXTURE_SLICE_3D: {
			textureType = MTLTextureType3D;
		} break;
		case TEXTURE_SLICE_CUBEMAP: {
			textureType = MTLTextureTypeCube;
		} break;
		case TEXTURE_SLICE_2D_ARRAY: {
			textureType = MTLTextureType2DArray;
		} break;
		case TEXTURE_SLICE_MAX: {
			ERR_FAIL_V_MSG(TextureID(), "Invalid texture slice type");
		} break;
	}

	MTLPixelFormat format = pixel_formats->getMTLPixelFormat(p_view.format);

	static const MTLTextureSwizzle component_swizzle[TEXTURE_SWIZZLE_MAX] = {
		static_cast<MTLTextureSwizzle>(255), // IDENTITY
		MTLTextureSwizzleZero,
		MTLTextureSwizzleOne,
		MTLTextureSwizzleRed,
		MTLTextureSwizzleGreen,
		MTLTextureSwizzleBlue,
		MTLTextureSwizzleAlpha,
	};

#define SWIZZLE(C, CHAN) (p_view.swizzle_##C != TEXTURE_SWIZZLE_IDENTITY ? component_swizzle[p_view.swizzle_##C] : MTLTextureSwizzle##CHAN)
	MTLTextureSwizzleChannels swizzle = MTLTextureSwizzleChannelsMake(
			SWIZZLE(r, Red),
			SWIZZLE(g, Green),
			SWIZZLE(b, Blue),
			SWIZZLE(a, Alpha));
#undef SWIZZLE
	id<MTLTexture> obj = [src_texture newTextureViewWithPixelFormat:format
														textureType:textureType
															 levels:NSMakeRange(p_mipmap, p_mipmaps)
															 slices:NSMakeRange(p_layer, p_layers)
															swizzle:swizzle];
	ERR_FAIL_NULL_V_MSG(obj, TextureID(), "Unable to create shared texture");
	return rid::make(obj);
}

void RenderingDeviceDriverMetal::texture_free(TextureID p_texture) {
	rid::release(p_texture);
}

uint64_t RenderingDeviceDriverMetal::texture_get_allocation_size(TextureID p_texture) {
	id<MTLTexture> obj = rid::get(p_texture);
	return obj.allocatedSize;
}

void RenderingDeviceDriverMetal::_get_sub_resource(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) const {
	id<MTLTexture> obj = rid::get(p_texture);

	*r_layout = {};

	PixelFormats &pf = *pixel_formats;

	size_t row_alignment = get_texel_buffer_alignment_for_format(obj.pixelFormat);
	size_t offset = 0;
	size_t array_layers = obj.arrayLength;
	MTLSize size = MTLSizeMake(obj.width, obj.height, obj.depth);
	MTLPixelFormat pixel_format = obj.pixelFormat;

	// First skip over the mipmap levels.
	for (uint32_t mipLvl = 0; mipLvl < p_subresource.mipmap; mipLvl++) {
		MTLSize mip_size = mipmapLevelSizeFromSize(size, mipLvl);
		size_t bytes_per_row = pf.getBytesPerRow(pixel_format, mip_size.width);
		bytes_per_row = round_up_to_alignment(bytes_per_row, row_alignment);
		size_t bytes_per_layer = pf.getBytesPerLayer(pixel_format, bytes_per_row, mip_size.height);
		offset += bytes_per_layer * mip_size.depth * array_layers;
	}

	// Get current mipmap.
	MTLSize mip_size = mipmapLevelSizeFromSize(size, p_subresource.mipmap);
	size_t bytes_per_row = pf.getBytesPerRow(pixel_format, mip_size.width);
	bytes_per_row = round_up_to_alignment(bytes_per_row, row_alignment);
	size_t bytes_per_layer = pf.getBytesPerLayer(pixel_format, bytes_per_row, mip_size.height);
	r_layout->size = bytes_per_layer * mip_size.depth;
	r_layout->offset = offset + (r_layout->size * p_subresource.layer - 1);
	r_layout->depth_pitch = bytes_per_layer;
	r_layout->row_pitch = bytes_per_row;
	r_layout->layer_pitch = r_layout->size * array_layers;
}

void RenderingDeviceDriverMetal::texture_get_copyable_layout(TextureID p_texture, const TextureSubresource &p_subresource, TextureCopyableLayout *r_layout) {
	id<MTLTexture> obj = rid::get(p_texture);
	*r_layout = {};

	if ((obj.resourceOptions & MTLResourceStorageModePrivate) != 0) {
		MTLSize sz = MTLSizeMake(obj.width, obj.height, obj.depth);

		PixelFormats &pf = *pixel_formats;
		DataFormat format = pf.getDataFormat(obj.pixelFormat);
		if (p_subresource.mipmap > 0) {
			r_layout->offset = get_image_format_required_size(format, sz.width, sz.height, sz.depth, p_subresource.mipmap);
		}

		sz = mipmapLevelSizeFromSize(sz, p_subresource.mipmap);

		uint32_t bw = 0, bh = 0;
		get_compressed_image_format_block_dimensions(format, bw, bh);
		uint32_t sbw = 0, sbh = 0;
		r_layout->size = get_image_format_required_size(format, sz.width, sz.height, sz.depth, 1, &sbw, &sbh);
		r_layout->row_pitch = r_layout->size / ((sbh / bh) * sz.depth);
		r_layout->depth_pitch = r_layout->size / sz.depth;
		r_layout->layer_pitch = r_layout->size / obj.arrayLength;
	} else {
		CRASH_NOW_MSG("need to calculate layout for shared texture");
	}
}

uint8_t *RenderingDeviceDriverMetal::texture_map(TextureID p_texture, const TextureSubresource &p_subresource) {
	id<MTLTexture> obj = rid::get(p_texture);
	ERR_FAIL_NULL_V_MSG(obj.buffer, nullptr, "texture is not created from a buffer");

	TextureCopyableLayout layout;
	_get_sub_resource(p_texture, p_subresource, &layout);
	return (uint8_t *)(obj.buffer.contents) + layout.offset;
	PixelFormats &pf = *pixel_formats;

	size_t row_alignment = get_texel_buffer_alignment_for_format(obj.pixelFormat);
	size_t offset = 0;
	size_t array_layers = obj.arrayLength;
	MTLSize size = MTLSizeMake(obj.width, obj.height, obj.depth);
	MTLPixelFormat pixel_format = obj.pixelFormat;

	// First skip over the mipmap levels.
	for (uint32_t mipLvl = 0; mipLvl < p_subresource.mipmap; mipLvl++) {
		MTLSize mipExtent = mipmapLevelSizeFromSize(size, mipLvl);
		size_t bytes_per_row = pf.getBytesPerRow(pixel_format, mipExtent.width);
		bytes_per_row = round_up_to_alignment(bytes_per_row, row_alignment);
		size_t bytes_per_layer = pf.getBytesPerLayer(pixel_format, bytes_per_row, mipExtent.height);
		offset += bytes_per_layer * mipExtent.depth * array_layers;
	}

	if (p_subresource.layer > 1) {
		// Calculate offset to desired layer.
		MTLSize mipExtent = mipmapLevelSizeFromSize(size, p_subresource.mipmap);
		size_t bytes_per_row = pf.getBytesPerRow(pixel_format, mipExtent.width);
		bytes_per_row = round_up_to_alignment(bytes_per_row, row_alignment);
		size_t bytes_per_layer = pf.getBytesPerLayer(pixel_format, bytes_per_row, mipExtent.height);
		offset += bytes_per_layer * mipExtent.depth * (p_subresource.layer - 1);
	}

	// TODO: Confirm with rendering team that there is no other way Godot may attempt to map a texture with multiple mipmaps or array layers.

	// NOTE: It is not possible to create a buffer-backed texture with mipmaps or array layers,
	//  as noted in the is_valid_linear function, so the offset calculation SHOULD always be zero.
	//  Given that, this code should be simplified.

	return (uint8_t *)(obj.buffer.contents) + offset;
}

void RenderingDeviceDriverMetal::texture_unmap(TextureID p_texture) {
	// Nothing to do.
}

BitField<RDD::TextureUsageBits> RenderingDeviceDriverMetal::texture_get_usages_supported_by_format(DataFormat p_format, bool p_cpu_readable) {
	PixelFormats &pf = *pixel_formats;
	if (pf.getMTLPixelFormat(p_format) == MTLPixelFormatInvalid) {
		return 0;
	}

	MTLFmtCaps caps = pf.getCapabilities(p_format);

	// Everything supported by default makes an all-or-nothing check easier for the caller.
	BitField<RDD::TextureUsageBits> supported = INT64_MAX;
	supported.clear_flag(TEXTURE_USAGE_VRS_ATTACHMENT_BIT); // No VRS support for Metal.

	if (!flags::any(caps, kMTLFmtCapsColorAtt)) {
		supported.clear_flag(TEXTURE_USAGE_COLOR_ATTACHMENT_BIT);
	}
	if (!flags::any(caps, kMTLFmtCapsDSAtt)) {
		supported.clear_flag(TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}
	if (!flags::any(caps, kMTLFmtCapsRead)) {
		supported.clear_flag(TEXTURE_USAGE_SAMPLING_BIT);
	}
	if (!flags::any(caps, kMTLFmtCapsAtomic)) {
		supported.clear_flag(TEXTURE_USAGE_STORAGE_ATOMIC_BIT);
	}

	return supported;
}

bool RenderingDeviceDriverMetal::texture_can_make_shared_with_format(TextureID p_texture, DataFormat p_format, bool &r_raw_reinterpretation) {
	r_raw_reinterpretation = false;
	return true;
}

#pragma mark - Sampler

static const MTLCompareFunction COMPARE_OPERATORS[RD::COMPARE_OP_MAX] = {
	MTLCompareFunctionNever,
	MTLCompareFunctionLess,
	MTLCompareFunctionEqual,
	MTLCompareFunctionLessEqual,
	MTLCompareFunctionGreater,
	MTLCompareFunctionNotEqual,
	MTLCompareFunctionGreaterEqual,
	MTLCompareFunctionAlways,
};

static const MTLStencilOperation STENCIL_OPERATIONS[RD::STENCIL_OP_MAX] = {
	MTLStencilOperationKeep,
	MTLStencilOperationZero,
	MTLStencilOperationReplace,
	MTLStencilOperationIncrementClamp,
	MTLStencilOperationDecrementClamp,
	MTLStencilOperationInvert,
	MTLStencilOperationIncrementWrap,
	MTLStencilOperationDecrementWrap,
};

static const MTLBlendFactor BLEND_FACTORS[RD::BLEND_FACTOR_MAX] = {
	MTLBlendFactorZero,
	MTLBlendFactorOne,
	MTLBlendFactorSourceColor,
	MTLBlendFactorOneMinusSourceColor,
	MTLBlendFactorDestinationColor,
	MTLBlendFactorOneMinusDestinationColor,
	MTLBlendFactorSourceAlpha,
	MTLBlendFactorOneMinusSourceAlpha,
	MTLBlendFactorDestinationAlpha,
	MTLBlendFactorOneMinusDestinationAlpha,
	MTLBlendFactorBlendColor,
	MTLBlendFactorOneMinusBlendColor,
	MTLBlendFactorBlendAlpha,
	MTLBlendFactorOneMinusBlendAlpha,
	MTLBlendFactorSourceAlphaSaturated,
	MTLBlendFactorSource1Color,
	MTLBlendFactorOneMinusSource1Color,
	MTLBlendFactorSource1Alpha,
	MTLBlendFactorOneMinusSource1Alpha,
};
static const MTLBlendOperation BLEND_OPERATIONS[RD::BLEND_OP_MAX] = {
	MTLBlendOperationAdd,
	MTLBlendOperationSubtract,
	MTLBlendOperationReverseSubtract,
	MTLBlendOperationMin,
	MTLBlendOperationMax,
};

static const API_AVAILABLE(macos(11.0), ios(14.0)) MTLSamplerAddressMode ADDRESS_MODES[RD::SAMPLER_REPEAT_MODE_MAX] = {
	MTLSamplerAddressModeRepeat,
	MTLSamplerAddressModeMirrorRepeat,
	MTLSamplerAddressModeClampToEdge,
	MTLSamplerAddressModeClampToBorderColor,
	MTLSamplerAddressModeMirrorClampToEdge,
};

static const API_AVAILABLE(macos(11.0), ios(14.0)) MTLSamplerBorderColor SAMPLER_BORDER_COLORS[RD::SAMPLER_BORDER_COLOR_MAX] = {
	MTLSamplerBorderColorTransparentBlack,
	MTLSamplerBorderColorTransparentBlack,
	MTLSamplerBorderColorOpaqueBlack,
	MTLSamplerBorderColorOpaqueBlack,
	MTLSamplerBorderColorOpaqueWhite,
	MTLSamplerBorderColorOpaqueWhite,
};

RDD::SamplerID RenderingDeviceDriverMetal::sampler_create(const SamplerState &p_state) {
	MTLSamplerDescriptor *desc = [MTLSamplerDescriptor new];
	desc.supportArgumentBuffers = YES;

	desc.magFilter = p_state.mag_filter == SAMPLER_FILTER_LINEAR ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	desc.minFilter = p_state.min_filter == SAMPLER_FILTER_LINEAR ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	desc.mipFilter = p_state.mip_filter == SAMPLER_FILTER_LINEAR ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;

	desc.sAddressMode = ADDRESS_MODES[p_state.repeat_u];
	desc.tAddressMode = ADDRESS_MODES[p_state.repeat_v];
	desc.rAddressMode = ADDRESS_MODES[p_state.repeat_w];

	if (p_state.use_anisotropy) {
		desc.maxAnisotropy = p_state.anisotropy_max;
	}

	desc.compareFunction = COMPARE_OPERATORS[p_state.compare_op];

	desc.lodMinClamp = p_state.min_lod;
	desc.lodMaxClamp = p_state.max_lod;

	desc.borderColor = SAMPLER_BORDER_COLORS[p_state.border_color];

	desc.normalizedCoordinates = !p_state.unnormalized_uvw;

	if (p_state.lod_bias != 0.0) {
		WARN_VERBOSE("Metal does not support LOD bias for samplers.");
	}

	id<MTLSamplerState> obj = [device newSamplerStateWithDescriptor:desc];
	ERR_FAIL_NULL_V_MSG(obj, SamplerID(), "newSamplerStateWithDescriptor failed");
	return rid::make(obj);
}

void RenderingDeviceDriverMetal::sampler_free(SamplerID p_sampler) {
	rid::release(p_sampler);
}

bool RenderingDeviceDriverMetal::sampler_is_format_supported_for_filter(DataFormat p_format, SamplerFilter p_filter) {
	switch (p_filter) {
		case SAMPLER_FILTER_NEAREST:
			return true;
		case SAMPLER_FILTER_LINEAR: {
			MTLFmtCaps caps = pixel_formats->getCapabilities(p_format);
			return flags::any(caps, kMTLFmtCapsFilter);
		}
	}
}

#pragma mark - Vertex Array

RDD::VertexFormatID RenderingDeviceDriverMetal::vertex_format_create(VectorView<VertexAttribute> p_vertex_attribs) {
	MTLVertexDescriptor *desc = MTLVertexDescriptor.vertexDescriptor;

	for (uint32_t i = 0; i < p_vertex_attribs.size(); i++) {
		VertexAttribute const &vf = p_vertex_attribs[i];

		ERR_FAIL_COND_V_MSG(get_format_vertex_size(vf.format) == 0, VertexFormatID(),
				"Data format for attachment (" + itos(i) + "), '" + FORMAT_NAMES[vf.format] + "', is not valid for a vertex array.");

		desc.attributes[vf.location].format = pixel_formats->getMTLVertexFormat(vf.format);
		desc.attributes[vf.location].offset = vf.offset;
		uint32_t idx = get_metal_buffer_index_for_vertex_attribute_binding(i);
		desc.attributes[vf.location].bufferIndex = idx;
		if (vf.stride == 0) {
			desc.layouts[idx].stepFunction = MTLVertexStepFunctionConstant;
			desc.layouts[idx].stepRate = 0;
			desc.layouts[idx].stride = pixel_formats->getBytesPerBlock(vf.format);
		} else {
			desc.layouts[idx].stepFunction = vf.frequency == VERTEX_FREQUENCY_VERTEX ? MTLVertexStepFunctionPerVertex : MTLVertexStepFunctionPerInstance;
			desc.layouts[idx].stepRate = 1;
			desc.layouts[idx].stride = vf.stride;
		}
	}

	return rid::make(desc);
}

void RenderingDeviceDriverMetal::vertex_format_free(VertexFormatID p_vertex_format) {
	rid::release(p_vertex_format);
}

#pragma mark - Barriers

void RenderingDeviceDriverMetal::command_pipeline_barrier(
		CommandBufferID p_cmd_buffer,
		BitField<PipelineStageBits> p_src_stages,
		BitField<PipelineStageBits> p_dst_stages,
		VectorView<MemoryBarrier> p_memory_barriers,
		VectorView<BufferBarrier> p_buffer_barriers,
		VectorView<TextureBarrier> p_texture_barriers) {
	WARN_PRINT_ONCE("not implemented");
}

#pragma mark - Fences

RDD::FenceID RenderingDeviceDriverMetal::fence_create() {
	Fence *fence = memnew(Fence);
	return FenceID(fence);
}

Error RenderingDeviceDriverMetal::fence_wait(FenceID p_fence) {
	Fence *fence = (Fence *)(p_fence.id);

	// Wait forever, so this function is infallible.
	dispatch_semaphore_wait(fence->semaphore, DISPATCH_TIME_FOREVER);

	return OK;
}

void RenderingDeviceDriverMetal::fence_free(FenceID p_fence) {
	Fence *fence = (Fence *)(p_fence.id);
	memdelete(fence);
}

#pragma mark - Semaphores

RDD::SemaphoreID RenderingDeviceDriverMetal::semaphore_create() {
	// Metal doesn't use semaphores, as their purpose within Godot is to ensure ordering of command buffer execution.
	return SemaphoreID(1);
}

void RenderingDeviceDriverMetal::semaphore_free(SemaphoreID p_semaphore) {
}

#pragma mark - Queues

RDD::CommandQueueFamilyID RenderingDeviceDriverMetal::command_queue_family_get(BitField<CommandQueueFamilyBits> p_cmd_queue_family_bits, RenderingContextDriver::SurfaceID p_surface) {
	if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_GRAPHICS_BIT) || (p_surface != 0)) {
		return CommandQueueFamilyID(COMMAND_QUEUE_FAMILY_GRAPHICS_BIT);
	} else if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_COMPUTE_BIT)) {
		return CommandQueueFamilyID(COMMAND_QUEUE_FAMILY_COMPUTE_BIT);
	} else if (p_cmd_queue_family_bits.has_flag(COMMAND_QUEUE_FAMILY_TRANSFER_BIT)) {
		return CommandQueueFamilyID(COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	} else {
		return CommandQueueFamilyID();
	}
}

RDD::CommandQueueID RenderingDeviceDriverMetal::command_queue_create(CommandQueueFamilyID p_cmd_queue_family, bool p_identify_as_main_queue) {
	return CommandQueueID(1);
}

Error RenderingDeviceDriverMetal::command_queue_execute_and_present(CommandQueueID p_cmd_queue, VectorView<SemaphoreID>, VectorView<CommandBufferID> p_cmd_buffers, VectorView<SemaphoreID>, FenceID p_cmd_fence, VectorView<SwapChainID> p_swap_chains) {
	uint32_t size = p_cmd_buffers.size();
	if (size == 0) {
		return OK;
	}

	for (uint32_t i = 0; i < size - 1; i++) {
		MDCommandBuffer *cmd_buffer = (MDCommandBuffer *)(p_cmd_buffers[i].id);
		cmd_buffer->commit();
	}

	// The last command buffer will signal the fence and semaphores.
	MDCommandBuffer *cmd_buffer = (MDCommandBuffer *)(p_cmd_buffers[size - 1].id);
	Fence *fence = (Fence *)(p_cmd_fence.id);
	if (fence != nullptr) {
		[cmd_buffer->get_command_buffer() addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
			dispatch_semaphore_signal(fence->semaphore);
		}];
	}

	for (uint32_t i = 0; i < p_swap_chains.size(); i++) {
		SwapChain *swap_chain = (SwapChain *)(p_swap_chains[i].id);
		RenderingContextDriverMetal::Surface *metal_surface = (RenderingContextDriverMetal::Surface *)(swap_chain->surface);
		metal_surface->present(cmd_buffer);
	}

	cmd_buffer->commit();

	if (p_swap_chains.size() > 0) {
		// Used as a signal that we're presenting, so this is the end of a frame.
		[device_scope endScope];
		[device_scope beginScope];
	}

	return OK;
}

void RenderingDeviceDriverMetal::command_queue_free(CommandQueueID p_cmd_queue) {
}

#pragma mark - Command Buffers

// ----- POOL -----

RDD::CommandPoolID RenderingDeviceDriverMetal::command_pool_create(CommandQueueFamilyID p_cmd_queue_family, CommandBufferType p_cmd_buffer_type) {
	DEV_ASSERT(p_cmd_buffer_type == COMMAND_BUFFER_TYPE_PRIMARY);
	return rid::make(device_queue);
}

bool RenderingDeviceDriverMetal::command_pool_reset(CommandPoolID p_cmd_pool) {
	return true;
}

void RenderingDeviceDriverMetal::command_pool_free(CommandPoolID p_cmd_pool) {
	rid::release(p_cmd_pool);
}

// ----- BUFFER -----

RDD::CommandBufferID RenderingDeviceDriverMetal::command_buffer_create(CommandPoolID p_cmd_pool) {
	id<MTLCommandQueue> queue = rid::get(p_cmd_pool);
	MDCommandBuffer *obj = new MDCommandBuffer(queue, this);
	command_buffers.push_back(obj);
	return CommandBufferID(obj);
}

bool RenderingDeviceDriverMetal::command_buffer_begin(CommandBufferID p_cmd_buffer) {
	MDCommandBuffer *obj = (MDCommandBuffer *)(p_cmd_buffer.id);
	obj->begin();
	return true;
}

bool RenderingDeviceDriverMetal::command_buffer_begin_secondary(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, uint32_t p_subpass, FramebufferID p_framebuffer) {
	ERR_FAIL_V_MSG(false, "not implemented");
}

void RenderingDeviceDriverMetal::command_buffer_end(CommandBufferID p_cmd_buffer) {
	MDCommandBuffer *obj = (MDCommandBuffer *)(p_cmd_buffer.id);
	obj->end();
}

void RenderingDeviceDriverMetal::command_buffer_execute_secondary(CommandBufferID p_cmd_buffer, VectorView<CommandBufferID> p_secondary_cmd_buffers) {
	ERR_FAIL_MSG("not implemented");
}

#pragma mark - Swap Chain

void RenderingDeviceDriverMetal::_swap_chain_release(SwapChain *p_swap_chain) {
	_swap_chain_release_buffers(p_swap_chain);
}

void RenderingDeviceDriverMetal::_swap_chain_release_buffers(SwapChain *p_swap_chain) {
}

RDD::SwapChainID RenderingDeviceDriverMetal::swap_chain_create(RenderingContextDriver::SurfaceID p_surface) {
	RenderingContextDriverMetal::Surface const *surface = (RenderingContextDriverMetal::Surface *)(p_surface);

	// Create the render pass that will be used to draw to the swap chain's framebuffers.
	RDD::Attachment attachment;
	attachment.format = pixel_formats->getDataFormat(surface->get_pixel_format());
	attachment.samples = RDD::TEXTURE_SAMPLES_1;
	attachment.load_op = RDD::ATTACHMENT_LOAD_OP_CLEAR;
	attachment.store_op = RDD::ATTACHMENT_STORE_OP_STORE;

	RDD::Subpass subpass;
	RDD::AttachmentReference color_ref;
	color_ref.attachment = 0;
	color_ref.aspect.set_flag(RDD::TEXTURE_ASPECT_COLOR_BIT);
	subpass.color_references.push_back(color_ref);

	RenderPassID render_pass = render_pass_create(attachment, subpass, {}, 1);
	ERR_FAIL_COND_V(!render_pass, SwapChainID());

	// Create the empty swap chain until it is resized.
	SwapChain *swap_chain = memnew(SwapChain);
	swap_chain->surface = p_surface;
	swap_chain->data_format = attachment.format;
	swap_chain->render_pass = render_pass;
	return SwapChainID(swap_chain);
}

Error RenderingDeviceDriverMetal::swap_chain_resize(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, uint32_t p_desired_framebuffer_count) {
	DEV_ASSERT(p_cmd_queue.id != 0);
	DEV_ASSERT(p_swap_chain.id != 0);

	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	RenderingContextDriverMetal::Surface *surface = (RenderingContextDriverMetal::Surface *)(swap_chain->surface);
	surface->resize(p_desired_framebuffer_count);

	// Once everything's been created correctly, indicate the surface no longer needs to be resized.
	context_driver->surface_set_needs_resize(swap_chain->surface, false);

	return OK;
}

RDD::FramebufferID RenderingDeviceDriverMetal::swap_chain_acquire_framebuffer(CommandQueueID p_cmd_queue, SwapChainID p_swap_chain, bool &r_resize_required) {
	DEV_ASSERT(p_cmd_queue.id != 0);
	DEV_ASSERT(p_swap_chain.id != 0);

	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	if (context_driver->surface_get_needs_resize(swap_chain->surface)) {
		r_resize_required = true;
		return FramebufferID();
	}

	RenderingContextDriverMetal::Surface *metal_surface = (RenderingContextDriverMetal::Surface *)(swap_chain->surface);
	return metal_surface->acquire_next_frame_buffer();
}

RDD::RenderPassID RenderingDeviceDriverMetal::swap_chain_get_render_pass(SwapChainID p_swap_chain) {
	const SwapChain *swap_chain = (const SwapChain *)(p_swap_chain.id);
	return swap_chain->render_pass;
}

RDD::DataFormat RenderingDeviceDriverMetal::swap_chain_get_format(SwapChainID p_swap_chain) {
	const SwapChain *swap_chain = (const SwapChain *)(p_swap_chain.id);
	return swap_chain->data_format;
}

void RenderingDeviceDriverMetal::swap_chain_set_max_fps(SwapChainID p_swap_chain, int p_max_fps) {
	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	RenderingContextDriverMetal::Surface *metal_surface = (RenderingContextDriverMetal::Surface *)(swap_chain->surface);
	metal_surface->set_max_fps(p_max_fps);
}

void RenderingDeviceDriverMetal::swap_chain_free(SwapChainID p_swap_chain) {
	SwapChain *swap_chain = (SwapChain *)(p_swap_chain.id);
	_swap_chain_release(swap_chain);
	render_pass_free(swap_chain->render_pass);
	memdelete(swap_chain);
}

#pragma mark - Frame buffer

RDD::FramebufferID RenderingDeviceDriverMetal::framebuffer_create(RenderPassID p_render_pass, VectorView<TextureID> p_attachments, uint32_t p_width, uint32_t p_height) {
	MDRenderPass *pass = (MDRenderPass *)(p_render_pass.id);

	Vector<MTL::Texture> textures;
	textures.resize(p_attachments.size());

	for (uint32_t i = 0; i < p_attachments.size(); i += 1) {
		MDAttachment const &a = pass->attachments[i];
		id<MTLTexture> tex = rid::get(p_attachments[i]);
		if (tex == nil) {
#if DEV_ENABLED
			WARN_PRINT("Invalid texture for attachment " + itos(i));
#endif
		}
		if (a.samples > 1) {
			if (tex.sampleCount != a.samples) {
#if DEV_ENABLED
				WARN_PRINT("Mismatched sample count for attachment " + itos(i) + "; expected " + itos(a.samples) + ", got " + itos(tex.sampleCount));
#endif
			}
		}
		textures.write[i] = tex;
	}

	MDFrameBuffer *fb = new MDFrameBuffer(textures, Size2i(p_width, p_height));
	return FramebufferID(fb);
}

void RenderingDeviceDriverMetal::framebuffer_free(FramebufferID p_framebuffer) {
	MDFrameBuffer *obj = (MDFrameBuffer *)(p_framebuffer.id);
	delete obj;
}

#pragma mark - Shader

const uint32_t SHADER_BINARY_VERSION = 4;

// region Serialization

class BufWriter;

template <typename T>
concept Serializable = requires(T t, BufWriter &p_writer) {
	{
		t.serialize_size()
	} -> std::same_as<size_t>;
	{
		t.serialize(p_writer)
	} -> std::same_as<void>;
};

class BufWriter {
	uint8_t *data = nullptr;
	uint64_t length = 0; // Length of data.
	uint64_t pos = 0;

public:
	BufWriter(uint8_t *p_data, uint64_t p_length) :
			data(p_data), length(p_length) {}

	template <Serializable T>
	void write(T const &p_value) {
		p_value.serialize(*this);
	}

	_FORCE_INLINE_ void write(uint32_t p_value) {
		DEV_ASSERT(pos + sizeof(uint32_t) <= length);
		pos += encode_uint32(p_value, data + pos);
	}

	_FORCE_INLINE_ void write(RD::ShaderStage p_value) {
		write((uint32_t)p_value);
	}

	_FORCE_INLINE_ void write(bool p_value) {
		DEV_ASSERT(pos + sizeof(uint8_t) <= length);
		*(data + pos) = p_value ? 1 : 0;
		pos += 1;
	}

	_FORCE_INLINE_ void write(int p_value) {
		write((uint32_t)p_value);
	}

	_FORCE_INLINE_ void write(uint64_t p_value) {
		DEV_ASSERT(pos + sizeof(uint64_t) <= length);
		pos += encode_uint64(p_value, data + pos);
	}

	_FORCE_INLINE_ void write(float p_value) {
		DEV_ASSERT(pos + sizeof(float) <= length);
		pos += encode_float(p_value, data + pos);
	}

	_FORCE_INLINE_ void write(double p_value) {
		DEV_ASSERT(pos + sizeof(double) <= length);
		pos += encode_double(p_value, data + pos);
	}

	void write_compressed(CharString const &p_string) {
		write(p_string.length()); // Uncompressed size.

		DEV_ASSERT(pos + sizeof(uint32_t) + Compression::get_max_compressed_buffer_size(p_string.length(), Compression::MODE_ZSTD) <= length);

		// Save pointer for compressed size.
		uint8_t *dst_size_ptr = data + pos; // Compressed size.
		pos += sizeof(uint32_t);

		int dst_size = Compression::compress(data + pos, reinterpret_cast<uint8_t const *>(p_string.ptr()), p_string.length(), Compression::MODE_ZSTD);
		encode_uint32(dst_size, dst_size_ptr);
		pos += dst_size;
	}

	void write(CharString const &p_string) {
		write_buffer(reinterpret_cast<const uint8_t *>(p_string.ptr()), p_string.length());
	}

	template <typename T>
	void write(VectorView<T> p_vector) {
		write(p_vector.size());
		for (uint32_t i = 0; i < p_vector.size(); i++) {
			T const &e = p_vector[i];
			write(e);
		}
	}

	void write(VectorView<uint8_t> p_vector) {
		write_buffer(p_vector.ptr(), p_vector.size());
	}

	template <typename K, typename V>
	void write(HashMap<K, V> const &p_map) {
		write(p_map.size());
		for (KeyValue<K, V> const &e : p_map) {
			write(e.key);
			write(e.value);
		}
	}

	uint64_t get_pos() const {
		return pos;
	}

	uint64_t get_length() const {
		return length;
	}

private:
	void write_buffer(uint8_t const *p_buffer, uint32_t p_length) {
		write(p_length);

		DEV_ASSERT(pos + p_length <= length);
		memcpy(data + pos, p_buffer, p_length);
		pos += p_length;
	}
};

class BufReader;

template <typename T>
concept Deserializable = requires(T t, BufReader &p_reader) {
	{
		t.serialize_size()
	} -> std::same_as<size_t>;
	{
		t.deserialize(p_reader)
	} -> std::same_as<void>;
};

class BufReader {
	uint8_t const *data = nullptr;
	uint64_t length = 0;
	uint64_t pos = 0;

	bool check_length(size_t p_size) {
		if (status != Status::OK)
			return false;

		if (pos + p_size > length) {
			status = Status::SHORT_BUFFER;
			return false;
		}
		return true;
	}

#define CHECK(p_size)          \
	if (!check_length(p_size)) \
	return

public:
	enum class Status {
		OK,
		SHORT_BUFFER,
		BAD_COMPRESSION,
	};

	Status status = Status::OK;

	BufReader(uint8_t const *p_data, uint64_t p_length) :
			data(p_data), length(p_length) {}

	template <Deserializable T>
	void read(T &p_value) {
		p_value.deserialize(*this);
	}

	_FORCE_INLINE_ void read(uint32_t &p_val) {
		CHECK(sizeof(uint32_t));

		p_val = decode_uint32(data + pos);
		pos += sizeof(uint32_t);
	}

	_FORCE_INLINE_ void read(RD::ShaderStage &p_val) {
		uint32_t val;
		read(val);
		p_val = (RD::ShaderStage)val;
	}

	_FORCE_INLINE_ void read(bool &p_val) {
		CHECK(sizeof(uint8_t));

		p_val = *(data + pos) > 0;
		pos += 1;
	}

	_FORCE_INLINE_ void read(uint64_t &p_val) {
		CHECK(sizeof(uint64_t));

		p_val = decode_uint64(data + pos);
		pos += sizeof(uint64_t);
	}

	_FORCE_INLINE_ void read(float &p_val) {
		CHECK(sizeof(float));

		p_val = decode_float(data + pos);
		pos += sizeof(float);
	}

	_FORCE_INLINE_ void read(double &p_val) {
		CHECK(sizeof(double));

		p_val = decode_double(data + pos);
		pos += sizeof(double);
	}

	void read(CharString &p_val) {
		uint32_t len;
		read(len);
		CHECK(len);
		p_val.resize(len + 1 /* NUL */);
		memcpy(p_val.ptrw(), data + pos, len);
		p_val.set(len, 0);
		pos += len;
	}

	void read_compressed(CharString &p_val) {
		uint32_t len;
		read(len);
		uint32_t comp_size;
		read(comp_size);

		CHECK(comp_size);

		p_val.resize(len + 1 /* NUL */);
		uint32_t bytes = (uint32_t)Compression::decompress(reinterpret_cast<uint8_t *>(p_val.ptrw()), len, data + pos, comp_size, Compression::MODE_ZSTD);
		if (bytes != len) {
			status = Status::BAD_COMPRESSION;
			return;
		}
		p_val.set(len, 0);
		pos += comp_size;
	}

	void read(LocalVector<uint8_t> &p_val) {
		uint32_t len;
		read(len);
		CHECK(len);
		p_val.resize(len);
		memcpy(p_val.ptr(), data + pos, len);
		pos += len;
	}

	template <typename T>
	void read(LocalVector<T> &p_val) {
		uint32_t len;
		read(len);
		CHECK(len);
		p_val.resize(len);
		for (uint32_t i = 0; i < len; i++) {
			read(p_val[i]);
		}
	}

	template <typename K, typename V>
	void read(HashMap<K, V> &p_map) {
		uint32_t len;
		read(len);
		CHECK(len);
		p_map.reserve(len);
		for (uint32_t i = 0; i < len; i++) {
			K key;
			read(key);
			V value;
			read(value);
			p_map[key] = value;
		}
	}

#undef CHECK
};

const uint32_t R32UI_ALIGNMENT_CONSTANT_ID = 65535;

struct ComputeSize {
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t z = 0;

	size_t serialize_size() const {
		return sizeof(uint32_t) * 3;
	}

	void serialize(BufWriter &p_writer) const {
		p_writer.write(x);
		p_writer.write(y);
		p_writer.write(z);
	}

	void deserialize(BufReader &p_reader) {
		p_reader.read(x);
		p_reader.read(y);
		p_reader.read(z);
	}
};

struct ShaderStageData {
	RD::ShaderStage stage = RD::ShaderStage::SHADER_STAGE_MAX;
	uint32_t is_position_invariant = UINT32_MAX;
	uint32_t supports_fast_math = UINT32_MAX;
	CharString entry_point_name;
	CharString source;

	size_t serialize_size() const {
		int comp_size = Compression::get_max_compressed_buffer_size(source.length(), Compression::MODE_ZSTD);
		return sizeof(uint32_t) // Stage.
				+ sizeof(uint32_t) // is_position_invariant
				+ sizeof(uint32_t) // supports_fast_math
				+ sizeof(uint32_t) /* entry_point_name.utf8().length */
				+ entry_point_name.length() + sizeof(uint32_t) /* uncompressed size */ + sizeof(uint32_t) /* compressed size */ + comp_size;
	}

	void serialize(BufWriter &p_writer) const {
		p_writer.write((uint32_t)stage);
		p_writer.write(is_position_invariant);
		p_writer.write(supports_fast_math);
		p_writer.write(entry_point_name);
		p_writer.write_compressed(source);
	}

	void deserialize(BufReader &p_reader) {
		p_reader.read((uint32_t &)stage);
		p_reader.read(is_position_invariant);
		p_reader.read(supports_fast_math);
		p_reader.read(entry_point_name);
		p_reader.read_compressed(source);
	}
};

struct SpecializationConstantData {
	uint32_t constant_id = UINT32_MAX;
	RD::PipelineSpecializationConstantType type = RD::PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT;
	ShaderStageUsage stages = ShaderStageUsage::None;
	// Specifies the stages the constant is used by Metal.
	ShaderStageUsage used_stages = ShaderStageUsage::None;
	uint32_t int_value = UINT32_MAX;

	size_t serialize_size() const {
		return sizeof(constant_id) + sizeof(uint32_t) // type
				+ sizeof(stages) + sizeof(used_stages) // used_stages
				+ sizeof(int_value); // int_value
	}

	void serialize(BufWriter &p_writer) const {
		p_writer.write(constant_id);
		p_writer.write((uint32_t)type);
		p_writer.write(stages);
		p_writer.write(used_stages);
		p_writer.write(int_value);
	}

	void deserialize(BufReader &p_reader) {
		p_reader.read(constant_id);
		p_reader.read((uint32_t &)type);
		p_reader.read((uint32_t &)stages);
		p_reader.read((uint32_t &)used_stages);
		p_reader.read(int_value);
	}
};

struct API_AVAILABLE(macos(11.0), ios(14.0)) UniformData {
	RD::UniformType type = RD::UniformType::UNIFORM_TYPE_MAX;
	uint32_t binding = UINT32_MAX;
	bool writable = false;
	uint32_t length = UINT32_MAX;
	ShaderStageUsage stages = ShaderStageUsage::None;
	// Specifies the stages the uniform data is
	// used by the Metal shader.
	ShaderStageUsage active_stages = ShaderStageUsage::None;
	BindingInfoMap bindings;
	BindingInfoMap bindings_secondary;

	size_t serialize_size() const {
		size_t size = 0;
		size += sizeof(uint32_t); // type
		size += sizeof(uint32_t); // binding
		size += sizeof(uint32_t); // writable
		size += sizeof(uint32_t); // length
		size += sizeof(uint32_t); // stages
		size += sizeof(uint32_t); // active_stages
		size += sizeof(uint32_t); // bindings.size()
		size += sizeof(uint32_t) * bindings.size(); // Total size of keys.
		for (KeyValue<RD::ShaderStage, BindingInfo> const &e : bindings) {
			size += e.value.serialize_size();
		}
		size += sizeof(uint32_t); // bindings_secondary.size()
		size += sizeof(uint32_t) * bindings_secondary.size(); // Total size of keys.
		for (KeyValue<RD::ShaderStage, BindingInfo> const &e : bindings_secondary) {
			size += e.value.serialize_size();
		}
		return size;
	}

	void serialize(BufWriter &p_writer) const {
		p_writer.write((uint32_t)type);
		p_writer.write(binding);
		p_writer.write(writable);
		p_writer.write(length);
		p_writer.write(stages);
		p_writer.write(active_stages);
		p_writer.write(bindings);
		p_writer.write(bindings_secondary);
	}

	void deserialize(BufReader &p_reader) {
		p_reader.read((uint32_t &)type);
		p_reader.read(binding);
		p_reader.read(writable);
		p_reader.read(length);
		p_reader.read((uint32_t &)stages);
		p_reader.read((uint32_t &)active_stages);
		p_reader.read(bindings);
		p_reader.read(bindings_secondary);
	}
};

struct API_AVAILABLE(macos(11.0), ios(14.0)) UniformSetData {
	uint32_t index = UINT32_MAX;
	LocalVector<UniformData> uniforms;

	size_t serialize_size() const {
		size_t size = 0;
		size += sizeof(uint32_t); // index
		size += sizeof(uint32_t); // uniforms.size()
		for (UniformData const &e : uniforms) {
			size += e.serialize_size();
		}
		return size;
	}

	void serialize(BufWriter &p_writer) const {
		p_writer.write(index);
		p_writer.write(VectorView(uniforms));
	}

	void deserialize(BufReader &p_reader) {
		p_reader.read(index);
		p_reader.read(uniforms);
	}
	UniformSetData() = default;
	UniformSetData(uint32_t p_index) :
			index(p_index) {}
};

struct PushConstantData {
	uint32_t size = UINT32_MAX;
	ShaderStageUsage stages = ShaderStageUsage::None;
	ShaderStageUsage used_stages = ShaderStageUsage::None;
	HashMap<RD::ShaderStage, uint32_t> msl_binding;

	size_t serialize_size() const {
		return sizeof(uint32_t) // size
				+ sizeof(uint32_t) // stages
				+ sizeof(uint32_t) // used_stages
				+ sizeof(uint32_t) // msl_binding.size()
				+ sizeof(uint32_t) * msl_binding.size() // keys
				+ sizeof(uint32_t) * msl_binding.size(); // values
	}

	void serialize(BufWriter &p_writer) const {
		p_writer.write(size);
		p_writer.write((uint32_t)stages);
		p_writer.write((uint32_t)used_stages);
		p_writer.write(msl_binding);
	}

	void deserialize(BufReader &p_reader) {
		p_reader.read(size);
		p_reader.read((uint32_t &)stages);
		p_reader.read((uint32_t &)used_stages);
		p_reader.read(msl_binding);
	}
};

struct API_AVAILABLE(macos(11.0), ios(14.0)) ShaderBinaryData {
	enum Flags : uint32_t {
		NONE = 0,
		NEEDS_VIEW_MASK_BUFFER = 1 << 0,
		USES_ARGUMENT_BUFFERS = 1 << 1,
	};
	CharString shader_name;
	// The Metal language version specified when compiling SPIR-V to MSL.
	// Format is major * 10000 + minor * 100 + patch.
	uint32_t msl_version = UINT32_MAX;
	uint32_t vertex_input_mask = UINT32_MAX;
	uint32_t fragment_output_mask = UINT32_MAX;
	uint32_t spirv_specialization_constants_ids_mask = UINT32_MAX;
	uint32_t flags = NONE;
	ComputeSize compute_local_size;
	PushConstantData push_constant;
	LocalVector<ShaderStageData> stages;
	LocalVector<SpecializationConstantData> constants;
	LocalVector<UniformSetData> uniforms;

	MTLLanguageVersion get_msl_version() const {
		uint32_t major = msl_version / 10000;
		uint32_t minor = (msl_version / 100) % 100;
		return MTLLanguageVersion((major << 0x10) + minor);
	}

	bool is_compute() const {
		return std::any_of(stages.begin(), stages.end(), [](ShaderStageData const &e) {
			return e.stage == RD::ShaderStage::SHADER_STAGE_COMPUTE;
		});
	}

	bool needs_view_mask_buffer() const {
		return flags & NEEDS_VIEW_MASK_BUFFER;
	}

	void set_needs_view_mask_buffer(bool p_value) {
		if (p_value) {
			flags |= NEEDS_VIEW_MASK_BUFFER;
		} else {
			flags &= ~NEEDS_VIEW_MASK_BUFFER;
		}
	}

	bool uses_argument_buffers() const {
		return flags & USES_ARGUMENT_BUFFERS;
	}

	void set_uses_argument_buffers(bool p_value) {
		if (p_value) {
			flags |= USES_ARGUMENT_BUFFERS;
		} else {
			flags &= ~USES_ARGUMENT_BUFFERS;
		}
	}

	size_t serialize_size() const {
		size_t size = 0;
		size += sizeof(uint32_t) + shader_name.length(); // shader_name
		size += sizeof(msl_version);
		size += sizeof(vertex_input_mask);
		size += sizeof(fragment_output_mask);
		size += sizeof(spirv_specialization_constants_ids_mask);
		size += sizeof(flags);
		size += compute_local_size.serialize_size();
		size += push_constant.serialize_size();
		size += sizeof(uint32_t); // stages.size()
		for (ShaderStageData const &e : stages) {
			size += e.serialize_size();
		}
		size += sizeof(uint32_t); // constants.size()
		for (SpecializationConstantData const &e : constants) {
			size += e.serialize_size();
		}
		size += sizeof(uint32_t); // uniforms.size()
		for (UniformSetData const &e : uniforms) {
			size += e.serialize_size();
		}
		return size;
	}

	void serialize(BufWriter &p_writer) const {
		p_writer.write(shader_name);
		p_writer.write(msl_version);
		p_writer.write(vertex_input_mask);
		p_writer.write(fragment_output_mask);
		p_writer.write(spirv_specialization_constants_ids_mask);
		p_writer.write(flags);
		p_writer.write(compute_local_size);
		p_writer.write(push_constant);
		p_writer.write(VectorView(stages));
		p_writer.write(VectorView(constants));
		p_writer.write(VectorView(uniforms));
	}

	void deserialize(BufReader &p_reader) {
		p_reader.read(shader_name);
		p_reader.read(msl_version);
		p_reader.read(vertex_input_mask);
		p_reader.read(fragment_output_mask);
		p_reader.read(spirv_specialization_constants_ids_mask);
		p_reader.read(flags);
		p_reader.read(compute_local_size);
		p_reader.read(push_constant);
		p_reader.read(stages);
		p_reader.read(constants);
		p_reader.read(uniforms);
	}
};

// endregion

String RenderingDeviceDriverMetal::shader_get_binary_cache_key() {
	static const String cache_key = "Metal-SV" + uitos(SHADER_BINARY_VERSION);
	return cache_key;
}

Error RenderingDeviceDriverMetal::_reflect_spirv16(VectorView<ShaderStageSPIRVData> p_spirv, ShaderReflection &r_reflection, ShaderMeta &r_shader_meta) {
	using namespace spirv_cross;
	using spirv_cross::Resource;

	r_reflection = {};
	r_shader_meta = {};

	for (uint32_t i = 0; i < p_spirv.size(); i++) {
		ShaderStageSPIRVData const &v = p_spirv[i];
		ShaderStage stage = v.shader_stage;
		uint32_t const *const ir = reinterpret_cast<uint32_t const *const>(v.spirv.ptr());
		size_t word_count = v.spirv.size() / sizeof(uint32_t);
		Parser parser(ir, word_count);
		try {
			parser.parse();
		} catch (CompilerError &e) {
			ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Failed to parse IR at stage " + String(SHADER_STAGE_NAMES[stage]) + ": " + e.what());
		}

		ShaderStage stage_flag = (ShaderStage)(1 << p_spirv[i].shader_stage);

		if (p_spirv[i].shader_stage == SHADER_STAGE_COMPUTE) {
			r_reflection.is_compute = true;
			ERR_FAIL_COND_V_MSG(p_spirv.size() != 1, FAILED,
					"Compute shaders can only receive one stage, dedicated to compute.");
		}
		ERR_FAIL_COND_V_MSG(r_reflection.stages.has_flag(stage_flag), FAILED,
				"Stage " + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + " submitted more than once.");

		ParsedIR &pir = parser.get_parsed_ir();
		using BT = SPIRType::BaseType;

		Compiler compiler(std::move(pir));

		if (r_reflection.is_compute) {
			r_reflection.compute_local_size[0] = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
			r_reflection.compute_local_size[1] = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
			r_reflection.compute_local_size[2] = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);
		}

		// Parse bindings.

		auto get_decoration = [&compiler](spirv_cross::ID id, spv::Decoration decoration) {
			uint32_t res = -1;
			if (compiler.has_decoration(id, decoration)) {
				res = compiler.get_decoration(id, decoration);
			}
			return res;
		};

		// Always clearer than a boolean.
		enum class Writable {
			No,
			Maybe,
		};

		// clang-format off
		enum {
		  SPIRV_WORD_SIZE      = sizeof(uint32_t),
		  SPIRV_DATA_ALIGNMENT = 4 * SPIRV_WORD_SIZE,
		};
		// clang-format on

		auto process_uniforms = [&r_reflection, &compiler, &get_decoration, stage, stage_flag](SmallVector<Resource> &resources, Writable writable, std::function<RDD::UniformType(SPIRType const &)> uniform_type) {
			for (Resource const &res : resources) {
				ShaderUniform uniform;

				std::string const &name = compiler.get_name(res.id);
				uint32_t set = get_decoration(res.id, spv::DecorationDescriptorSet);
				ERR_FAIL_COND_V_MSG(set == (uint32_t)-1, FAILED, "No descriptor set found");
				ERR_FAIL_COND_V_MSG(set >= MAX_UNIFORM_SETS, FAILED, "On shader stage '" + String(SHADER_STAGE_NAMES[stage]) + "', uniform '" + name.c_str() + "' uses a set (" + itos(set) + ") index larger than what is supported (" + itos(MAX_UNIFORM_SETS) + ").");

				uniform.binding = get_decoration(res.id, spv::DecorationBinding);
				ERR_FAIL_COND_V_MSG(uniform.binding == (uint32_t)-1, FAILED, "No binding found");

				SPIRType const &a_type = compiler.get_type(res.type_id);
				uniform.type = uniform_type(a_type);

				// Update length.
				switch (a_type.basetype) {
					case BT::Struct: {
						if (uniform.type == UNIFORM_TYPE_STORAGE_BUFFER) {
							// Consistent with spirv_reflect.
							uniform.length = 0;
						} else {
							uniform.length = round_up_to_alignment(compiler.get_declared_struct_size(a_type), SPIRV_DATA_ALIGNMENT);
						}
					} break;
					case BT::Image:
					case BT::Sampler:
					case BT::SampledImage: {
						uniform.length = 1;
						for (uint32_t const &a : a_type.array) {
							uniform.length *= a;
						}
					} break;
					default:
						break;
				}

				// Update writable.
				if (writable == Writable::Maybe) {
					if (a_type.basetype == BT::Struct) {
						Bitset flags = compiler.get_buffer_block_flags(res.id);
						uniform.writable = !compiler.has_decoration(res.id, spv::DecorationNonWritable) && !flags.get(spv::DecorationNonWritable);
					} else if (a_type.basetype == BT::Image) {
						if (a_type.image.access == spv::AccessQualifierMax) {
							uniform.writable = !compiler.has_decoration(res.id, spv::DecorationNonWritable);
						} else {
							uniform.writable = a_type.image.access != spv::AccessQualifierReadOnly;
						}
					}
				}

				if (set < (uint32_t)r_reflection.uniform_sets.size()) {
					// Check if this already exists.
					bool exists = false;
					for (uint32_t k = 0; k < r_reflection.uniform_sets[set].size(); k++) {
						if (r_reflection.uniform_sets[set][k].binding == uniform.binding) {
							// Already exists, verify that it's the same type.
							ERR_FAIL_COND_V_MSG(r_reflection.uniform_sets[set][k].type != uniform.type, FAILED,
									"On shader stage '" + String(SHADER_STAGE_NAMES[stage]) + "', uniform '" + name.c_str() + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different uniform type.");

							// Also, verify that it's the same size.
							ERR_FAIL_COND_V_MSG(r_reflection.uniform_sets[set][k].length != uniform.length, FAILED,
									"On shader stage '" + String(SHADER_STAGE_NAMES[stage]) + "', uniform '" + name.c_str() + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different uniform size.");

							// Also, verify that it has the same writability.
							ERR_FAIL_COND_V_MSG(r_reflection.uniform_sets[set][k].writable != uniform.writable, FAILED,
									"On shader stage '" + String(SHADER_STAGE_NAMES[stage]) + "', uniform '" + name.c_str() + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different writability.");

							// Just append stage mask and continue.
							r_reflection.uniform_sets.write[set].write[k].stages.set_flag(stage_flag);
							exists = true;
							break;
						}
					}

					if (exists) {
						continue; // Merged.
					}
				}

				uniform.stages.set_flag(stage_flag);

				if (set >= (uint32_t)r_reflection.uniform_sets.size()) {
					r_reflection.uniform_sets.resize(set + 1);
				}

				r_reflection.uniform_sets.write[set].push_back(uniform);
			}

			return OK;
		};

		ShaderResources resources = compiler.get_shader_resources();

		process_uniforms(resources.uniform_buffers, Writable::No, [](SPIRType const &a_type) {
			DEV_ASSERT(a_type.basetype == BT::Struct);
			return UNIFORM_TYPE_UNIFORM_BUFFER;
		});

		process_uniforms(resources.storage_buffers, Writable::Maybe, [](SPIRType const &a_type) {
			DEV_ASSERT(a_type.basetype == BT::Struct);
			return UNIFORM_TYPE_STORAGE_BUFFER;
		});

		process_uniforms(resources.storage_images, Writable::Maybe, [](SPIRType const &a_type) {
			DEV_ASSERT(a_type.basetype == BT::Image);
			if (a_type.image.dim == spv::DimBuffer) {
				return UNIFORM_TYPE_IMAGE_BUFFER;
			} else {
				return UNIFORM_TYPE_IMAGE;
			}
		});

		process_uniforms(resources.sampled_images, Writable::No, [](SPIRType const &a_type) {
			DEV_ASSERT(a_type.basetype == BT::SampledImage);
			return UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		});

		process_uniforms(resources.separate_images, Writable::No, [](SPIRType const &a_type) {
			DEV_ASSERT(a_type.basetype == BT::Image);
			if (a_type.image.dim == spv::DimBuffer) {
				return UNIFORM_TYPE_TEXTURE_BUFFER;
			} else {
				return UNIFORM_TYPE_TEXTURE;
			}
		});

		process_uniforms(resources.separate_samplers, Writable::No, [](SPIRType const &a_type) {
			DEV_ASSERT(a_type.basetype == BT::Sampler);
			return UNIFORM_TYPE_SAMPLER;
		});

		process_uniforms(resources.subpass_inputs, Writable::No, [](SPIRType const &a_type) {
			DEV_ASSERT(a_type.basetype == BT::Image && a_type.image.dim == spv::DimSubpassData);
			return UNIFORM_TYPE_INPUT_ATTACHMENT;
		});

		if (!resources.push_constant_buffers.empty()) {
			// There can be only one push constant block.
			Resource const &res = resources.push_constant_buffers.front();

			size_t push_constant_size = round_up_to_alignment(compiler.get_declared_struct_size(compiler.get_type(res.base_type_id)), SPIRV_DATA_ALIGNMENT);
			ERR_FAIL_COND_V_MSG(r_reflection.push_constant_size && r_reflection.push_constant_size != push_constant_size, FAILED,
					"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "': Push constant block must be the same across shader stages.");

			r_reflection.push_constant_size = push_constant_size;
			r_reflection.push_constant_stages.set_flag(stage_flag);
		}

		ERR_FAIL_COND_V_MSG(!resources.atomic_counters.empty(), FAILED, "Atomic counters not supported");
		ERR_FAIL_COND_V_MSG(!resources.acceleration_structures.empty(), FAILED, "Acceleration structures not supported");
		ERR_FAIL_COND_V_MSG(!resources.shader_record_buffers.empty(), FAILED, "Shader record buffers not supported");

		if (stage == SHADER_STAGE_VERTEX && !resources.stage_inputs.empty()) {
			for (Resource const &res : resources.stage_inputs) {
				SPIRType a_type = compiler.get_type(res.base_type_id);
				uint32_t loc = get_decoration(res.id, spv::DecorationLocation);
				if (loc != (uint32_t)-1) {
					r_reflection.vertex_input_mask |= 1 << loc;
				}
			}
		}

		if (stage == SHADER_STAGE_FRAGMENT && !resources.stage_outputs.empty()) {
			for (Resource const &res : resources.stage_outputs) {
				SPIRType a_type = compiler.get_type(res.base_type_id);
				uint32_t loc = get_decoration(res.id, spv::DecorationLocation);
				uint32_t built_in = spv::BuiltIn(get_decoration(res.id, spv::DecorationBuiltIn));
				if (loc != (uint32_t)-1 && built_in != spv::BuiltInFragDepth) {
					r_reflection.fragment_output_mask |= 1 << loc;
				}
			}
		}

		for (const BuiltInResource &res : resources.builtin_inputs) {
			if (res.builtin == spv::BuiltInViewIndex || res.builtin == spv::BuiltInViewportIndex) {
				r_shader_meta.has_multiview = true;
			}
		}

		if (!r_shader_meta.has_multiview) {
			for (const BuiltInResource &res : resources.builtin_outputs) {
				if (res.builtin == spv::BuiltInViewIndex || res.builtin == spv::BuiltInViewportIndex) {
					r_shader_meta.has_multiview = true;
				}
			}
		}

		// Specialization constants.
		for (SpecializationConstant const &constant : compiler.get_specialization_constants()) {
			int32_t existing = -1;
			ShaderSpecializationConstant sconst;
			SPIRConstant &spc = compiler.get_constant(constant.id);
			SPIRType const &spct = compiler.get_type(spc.constant_type);

			sconst.constant_id = constant.constant_id;
			sconst.int_value = 0;

			switch (spct.basetype) {
				case BT::Boolean: {
					sconst.type = PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
					sconst.bool_value = spc.scalar() != 0;
				} break;
				case BT::Int:
				case BT::UInt: {
					sconst.type = PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT;
					sconst.int_value = spc.scalar();
				} break;
				case BT::Float: {
					sconst.type = PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT;
					sconst.float_value = spc.scalar_f32();
				} break;
				default:
					ERR_FAIL_V_MSG(FAILED, "Unsupported specialization constant type");
			}
			sconst.stages.set_flag(stage_flag);

			for (uint32_t k = 0; k < r_reflection.specialization_constants.size(); k++) {
				if (r_reflection.specialization_constants[k].constant_id == sconst.constant_id) {
					ERR_FAIL_COND_V_MSG(r_reflection.specialization_constants[k].type != sconst.type, FAILED, "More than one specialization constant used for id (" + itos(sconst.constant_id) + "), but their types differ.");
					ERR_FAIL_COND_V_MSG(r_reflection.specialization_constants[k].int_value != sconst.int_value, FAILED, "More than one specialization constant used for id (" + itos(sconst.constant_id) + "), but their default values differ.");
					existing = k;
					break;
				}
			}

			if (existing > 0) {
				r_reflection.specialization_constants.write[existing].stages.set_flag(stage_flag);
			} else {
				r_reflection.specialization_constants.push_back(sconst);
			}
		}

		r_reflection.stages.set_flag(stage_flag);
	}

	// Sort all uniform_sets.
	for (uint32_t i = 0; i < r_reflection.uniform_sets.size(); i++) {
		r_reflection.uniform_sets.write[i].sort();
	}

	return OK;
}

Vector<uint8_t> RenderingDeviceDriverMetal::shader_compile_binary_from_spirv(VectorView<ShaderStageSPIRVData> p_spirv, const String &p_shader_name) {
	using Result = ::Vector<uint8_t>;
	using namespace spirv_cross;
	using spirv_cross::CompilerMSL;
	using spirv_cross::Resource;

	ShaderReflection spirv_data;
	ShaderMeta shader_meta;
	ERR_FAIL_COND_V(_reflect_spirv16(p_spirv, spirv_data, shader_meta), Result());

	ShaderBinaryData bin_data{};
	if (!p_shader_name.is_empty()) {
		bin_data.shader_name = p_shader_name.utf8();
	} else {
		bin_data.shader_name = "unnamed";
	}

	bin_data.vertex_input_mask = spirv_data.vertex_input_mask;
	bin_data.fragment_output_mask = spirv_data.fragment_output_mask;
	bin_data.compute_local_size = ComputeSize{
		.x = spirv_data.compute_local_size[0],
		.y = spirv_data.compute_local_size[1],
		.z = spirv_data.compute_local_size[2],
	};
	bin_data.push_constant.size = spirv_data.push_constant_size;
	bin_data.push_constant.stages = (ShaderStageUsage)(uint8_t)spirv_data.push_constant_stages;
	bin_data.set_needs_view_mask_buffer(shader_meta.has_multiview);

	for (uint32_t i = 0; i < spirv_data.uniform_sets.size(); i++) {
		const ::Vector<ShaderUniform> &spirv_set = spirv_data.uniform_sets[i];
		UniformSetData set(i);
		for (const ShaderUniform &spirv_uniform : spirv_set) {
			UniformData binding{};
			binding.type = spirv_uniform.type;
			binding.binding = spirv_uniform.binding;
			binding.writable = spirv_uniform.writable;
			binding.stages = (ShaderStageUsage)(uint8_t)spirv_uniform.stages;
			binding.length = spirv_uniform.length;
			set.uniforms.push_back(binding);
		}
		bin_data.uniforms.push_back(set);
	}

	for (const ShaderSpecializationConstant &spirv_sc : spirv_data.specialization_constants) {
		SpecializationConstantData spec_constant{};
		spec_constant.type = spirv_sc.type;
		spec_constant.constant_id = spirv_sc.constant_id;
		spec_constant.int_value = spirv_sc.int_value;
		spec_constant.stages = (ShaderStageUsage)(uint8_t)spirv_sc.stages;
		bin_data.constants.push_back(spec_constant);
		bin_data.spirv_specialization_constants_ids_mask |= (1 << spirv_sc.constant_id);
	}

	// Reflection using SPIRV-Cross:
	// https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide

	CompilerMSL::Options msl_options{};
	msl_options.set_msl_version(version_major, version_minor);
	if (version_major == 3 && version_minor >= 1) {
		// TODO(sgc): Restrict to Metal 3.0 for now, until bugs in SPIRV-cross image atomics are resolved.
		msl_options.set_msl_version(3, 0);
	}
	bin_data.msl_version = msl_options.msl_version;
#if TARGET_OS_OSX
	msl_options.platform = CompilerMSL::Options::macOS;
#else
	msl_options.platform = CompilerMSL::Options::iOS;
#endif

#if TARGET_OS_IOS
	msl_options.ios_use_simdgroup_functions = (*device_properties).features.simdPermute;
#endif

	bool disable_argument_buffers = false;
	if (String v = OS::get_singleton()->get_environment(U"GODOT_DISABLE_ARGUMENT_BUFFERS"); v == U"1") {
		disable_argument_buffers = true;
	}

	if (device_properties->features.argument_buffers_tier >= MTLArgumentBuffersTier2 && !disable_argument_buffers) {
		msl_options.argument_buffers_tier = CompilerMSL::Options::ArgumentBuffersTier::Tier2;
		msl_options.argument_buffers = true;
		bin_data.set_uses_argument_buffers(true);
	} else {
		msl_options.argument_buffers_tier = CompilerMSL::Options::ArgumentBuffersTier::Tier1;
		// Tier 1 argument buffers don't support writable textures, so we disable them completely.
		msl_options.argument_buffers = false;
		bin_data.set_uses_argument_buffers(false);
	}

	msl_options.force_active_argument_buffer_resources = true; // Same as MoltenVK when using argument buffers.
	// msl_options.pad_argument_buffer_resources = true; // Same as MoltenVK when using argument buffers.
	msl_options.texture_buffer_native = true; // Enable texture buffer support.
	msl_options.use_framebuffer_fetch_subpasses = false;
	msl_options.pad_fragment_output_components = true;
	msl_options.r32ui_alignment_constant_id = R32UI_ALIGNMENT_CONSTANT_ID;
	msl_options.agx_manual_cube_grad_fixup = true;
	if (shader_meta.has_multiview) {
		msl_options.multiview = true;
		msl_options.multiview_layered_rendering = true;
		msl_options.view_mask_buffer_index = VIEW_MASK_BUFFER_INDEX;
	}

	CompilerGLSL::Options options{};
	options.vertex.flip_vert_y = true;
#if DEV_ENABLED
	options.emit_line_directives = true;
#endif

	for (uint32_t i = 0; i < p_spirv.size(); i++) {
		ShaderStageSPIRVData const &v = p_spirv[i];
		ShaderStage stage = v.shader_stage;
		char const *stage_name = SHADER_STAGE_NAMES[stage];
		uint32_t const *const ir = reinterpret_cast<uint32_t const *const>(v.spirv.ptr());
		size_t word_count = v.spirv.size() / sizeof(uint32_t);
		Parser parser(ir, word_count);
		try {
			parser.parse();
		} catch (CompilerError &e) {
			ERR_FAIL_V_MSG(Result(), "Failed to parse IR at stage " + String(SHADER_STAGE_NAMES[stage]) + ": " + e.what());
		}

		CompilerMSL compiler(std::move(parser.get_parsed_ir()));
		compiler.set_msl_options(msl_options);
		compiler.set_common_options(options);

		std::unordered_set<VariableID> active = compiler.get_active_interface_variables();
		ShaderResources resources = compiler.get_shader_resources();

		std::string source;
		try {
			source = compiler.compile();
		} catch (CompilerError &e) {
			ERR_FAIL_V_MSG(Result(), "Failed to compile stage " + String(SHADER_STAGE_NAMES[stage]) + ": " + e.what());
		}

		ERR_FAIL_COND_V_MSG(compiler.get_entry_points_and_stages().size() != 1, Result(), "Expected a single entry point and stage.");

		SmallVector<EntryPoint> entry_pts_stages = compiler.get_entry_points_and_stages();
		EntryPoint &entry_point_stage = entry_pts_stages.front();
		SPIREntryPoint &entry_point = compiler.get_entry_point(entry_point_stage.name, entry_point_stage.execution_model);

		// Process specialization constants.
		if (!compiler.get_specialization_constants().empty()) {
			for (SpecializationConstant const &constant : compiler.get_specialization_constants()) {
				LocalVector<SpecializationConstantData>::Iterator res = bin_data.constants.begin();
				while (res != bin_data.constants.end()) {
					if (res->constant_id == constant.constant_id) {
						res->used_stages |= 1 << stage;
						break;
					}
					++res;
				}
				if (res == bin_data.constants.end()) {
					WARN_PRINT(String(stage_name) + ": unable to find constant_id: " + itos(constant.constant_id));
				}
			}
		}

		// Process bindings.

		LocalVector<UniformSetData> &uniform_sets = bin_data.uniforms;
		using BT = SPIRType::BaseType;

		// Always clearer than a boolean.
		enum class Writable {
			No,
			Maybe,
		};

		// Returns a std::optional containing the value of the
		// decoration, if it exists.
		auto get_decoration = [&compiler](spirv_cross::ID id, spv::Decoration decoration) {
			uint32_t res = -1;
			if (compiler.has_decoration(id, decoration)) {
				res = compiler.get_decoration(id, decoration);
			}
			return res;
		};

		auto descriptor_bindings = [&compiler, &active, &uniform_sets, stage, &get_decoration](SmallVector<Resource> &p_resources, Writable p_writable) {
			for (Resource const &res : p_resources) {
				uint32_t dset = get_decoration(res.id, spv::DecorationDescriptorSet);
				uint32_t dbin = get_decoration(res.id, spv::DecorationBinding);
				UniformData *found = nullptr;
				if (dset != (uint32_t)-1 && dbin != (uint32_t)-1 && dset < uniform_sets.size()) {
					UniformSetData &set = uniform_sets[dset];
					LocalVector<UniformData>::Iterator pos = set.uniforms.begin();
					while (pos != set.uniforms.end()) {
						if (dbin == pos->binding) {
							found = &(*pos);
							break;
						}
						++pos;
					}
				}

				ERR_FAIL_NULL_V_MSG(found, ERR_CANT_CREATE, "UniformData not found");

				bool is_active = active.find(res.id) != active.end();
				if (is_active) {
					found->active_stages |= 1 << stage;
				}

				BindingInfo primary{};

				SPIRType const &a_type = compiler.get_type(res.type_id);
				BT basetype = a_type.basetype;

				switch (basetype) {
					case BT::Struct: {
						primary.dataType = MTLDataTypePointer;
					} break;

					case BT::Image:
					case BT::SampledImage: {
						primary.dataType = MTLDataTypeTexture;
					} break;

					case BT::Sampler: {
						primary.dataType = MTLDataTypeSampler;
						primary.arrayLength = 1;
						for (uint32_t const &a : a_type.array) {
							primary.arrayLength *= a;
						}
					} break;

					default: {
						ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Unexpected BaseType");
					} break;
				}

				// Find array length of image.
				if (basetype == BT::Image || basetype == BT::SampledImage) {
					primary.arrayLength = 1;
					for (uint32_t const &a : a_type.array) {
						primary.arrayLength *= a;
					}
					primary.isMultisampled = a_type.image.ms;

					SPIRType::ImageType const &image = a_type.image;
					primary.imageFormat = image.format;

					switch (image.dim) {
						case spv::Dim1D: {
							if (image.arrayed) {
								primary.textureType = MTLTextureType1DArray;
							} else {
								primary.textureType = MTLTextureType1D;
							}
						} break;
						case spv::DimSubpassData: {
							DISPATCH_FALLTHROUGH;
						}
						case spv::Dim2D: {
							if (image.arrayed && image.ms) {
								primary.textureType = MTLTextureType2DMultisampleArray;
							} else if (image.arrayed) {
								primary.textureType = MTLTextureType2DArray;
							} else if (image.ms) {
								primary.textureType = MTLTextureType2DMultisample;
							} else {
								primary.textureType = MTLTextureType2D;
							}
						} break;
						case spv::Dim3D: {
							primary.textureType = MTLTextureType3D;
						} break;
						case spv::DimCube: {
							if (image.arrayed) {
								primary.textureType = MTLTextureTypeCube;
							}
						} break;
						case spv::DimRect: {
						} break;
						case spv::DimBuffer: {
							// VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
							primary.textureType = MTLTextureTypeTextureBuffer;
						} break;
						case spv::DimMax: {
							// Add all enumerations to silence the compiler warning
							// and generate future warnings, should a new one be added.
						} break;
					}
				}

				// Update writable.
				if (p_writable == Writable::Maybe) {
					if (basetype == BT::Struct) {
						Bitset flags = compiler.get_buffer_block_flags(res.id);
						if (!flags.get(spv::DecorationNonWritable)) {
							if (flags.get(spv::DecorationNonReadable)) {
								primary.access = MTLBindingAccessWriteOnly;
							} else {
								primary.access = MTLBindingAccessReadWrite;
							}
						}
					} else if (basetype == BT::Image) {
						switch (a_type.image.access) {
							case spv::AccessQualifierWriteOnly:
								primary.access = MTLBindingAccessWriteOnly;
								break;
							case spv::AccessQualifierReadWrite:
								primary.access = MTLBindingAccessReadWrite;
								break;
							case spv::AccessQualifierReadOnly:
								break;
							case spv::AccessQualifierMax:
								DISPATCH_FALLTHROUGH;
							default:
								if (!compiler.has_decoration(res.id, spv::DecorationNonWritable)) {
									if (compiler.has_decoration(res.id, spv::DecorationNonReadable)) {
										primary.access = MTLBindingAccessWriteOnly;
									} else {
										primary.access = MTLBindingAccessReadWrite;
									}
								}
								break;
						}
					}
				}

				switch (primary.access) {
					case MTLBindingAccessReadOnly:
						primary.usage = MTLResourceUsageRead;
						break;
					case MTLBindingAccessWriteOnly:
						primary.usage = MTLResourceUsageWrite;
						break;
					case MTLBindingAccessReadWrite:
						primary.usage = MTLResourceUsageRead | MTLResourceUsageWrite;
						break;
				}

				primary.index = compiler.get_automatic_msl_resource_binding(res.id);

				found->bindings[stage] = primary;

				// A sampled image contains two bindings, the primary
				// is to the image, and the secondary is to the associated sampler.
				if (basetype == BT::SampledImage) {
					uint32_t binding = compiler.get_automatic_msl_resource_binding_secondary(res.id);
					if (binding != (uint32_t)-1) {
						found->bindings_secondary[stage] = BindingInfo{
							.dataType = MTLDataTypeSampler,
							.index = binding,
							.access = MTLBindingAccessReadOnly,
						};
					}
				}

				// An image may have a secondary binding if it is used
				// for atomic operations.
				if (basetype == BT::Image) {
					uint32_t binding = compiler.get_automatic_msl_resource_binding_secondary(res.id);
					if (binding != (uint32_t)-1) {
						found->bindings_secondary[stage] = BindingInfo{
							.dataType = MTLDataTypePointer,
							.index = binding,
							.access = MTLBindingAccessReadWrite,
						};
					}
				}
			}
			return Error::OK;
		};

		if (!resources.uniform_buffers.empty()) {
			Error err = descriptor_bindings(resources.uniform_buffers, Writable::No);
			ERR_FAIL_COND_V(err != OK, Result());
		}
		if (!resources.storage_buffers.empty()) {
			Error err = descriptor_bindings(resources.storage_buffers, Writable::Maybe);
			ERR_FAIL_COND_V(err != OK, Result());
		}
		if (!resources.storage_images.empty()) {
			Error err = descriptor_bindings(resources.storage_images, Writable::Maybe);
			ERR_FAIL_COND_V(err != OK, Result());
		}
		if (!resources.sampled_images.empty()) {
			Error err = descriptor_bindings(resources.sampled_images, Writable::No);
			ERR_FAIL_COND_V(err != OK, Result());
		}
		if (!resources.separate_images.empty()) {
			Error err = descriptor_bindings(resources.separate_images, Writable::No);
			ERR_FAIL_COND_V(err != OK, Result());
		}
		if (!resources.separate_samplers.empty()) {
			Error err = descriptor_bindings(resources.separate_samplers, Writable::No);
			ERR_FAIL_COND_V(err != OK, Result());
		}
		if (!resources.subpass_inputs.empty()) {
			Error err = descriptor_bindings(resources.subpass_inputs, Writable::No);
			ERR_FAIL_COND_V(err != OK, Result());
		}

		if (!resources.push_constant_buffers.empty()) {
			for (Resource const &res : resources.push_constant_buffers) {
				uint32_t binding = compiler.get_automatic_msl_resource_binding(res.id);
				if (binding != (uint32_t)-1) {
					bin_data.push_constant.used_stages |= 1 << stage;
					bin_data.push_constant.msl_binding[stage] = binding;
				}
			}
		}

		ERR_FAIL_COND_V_MSG(!resources.atomic_counters.empty(), Result(), "Atomic counters not supported");
		ERR_FAIL_COND_V_MSG(!resources.acceleration_structures.empty(), Result(), "Acceleration structures not supported");
		ERR_FAIL_COND_V_MSG(!resources.shader_record_buffers.empty(), Result(), "Shader record buffers not supported");

		if (!resources.stage_inputs.empty()) {
			for (Resource const &res : resources.stage_inputs) {
				uint32_t binding = compiler.get_automatic_msl_resource_binding(res.id);
				if (binding != (uint32_t)-1) {
					bin_data.vertex_input_mask |= 1 << binding;
				}
			}
		}

		ShaderStageData stage_data;
		stage_data.stage = v.shader_stage;
		stage_data.is_position_invariant = compiler.is_position_invariant();
		stage_data.supports_fast_math = !entry_point.flags.get(spv::ExecutionModeSignedZeroInfNanPreserve);
		stage_data.entry_point_name = entry_point.name.c_str();
		stage_data.source = source.c_str();
		bin_data.stages.push_back(stage_data);
	}

	size_t vec_size = bin_data.serialize_size() + 8;

	::Vector<uint8_t> ret;
	ret.resize(vec_size);
	BufWriter writer(ret.ptrw(), vec_size);
	const uint8_t HEADER[4] = { 'G', 'M', 'S', 'L' };
	writer.write(*(uint32_t *)HEADER);
	writer.write(SHADER_BINARY_VERSION);
	bin_data.serialize(writer);
	ret.resize(writer.get_pos());

	return ret;
}

void RenderingDeviceDriverMetal::shader_cache_free_entry(const SHA256Digest &key) {
	if (ShaderCacheEntry **pentry = _shader_cache.getptr(key); pentry != nullptr) {
		ShaderCacheEntry *entry = *pentry;
		_shader_cache.erase(key);
		entry->library = nil;
		memdelete(entry);
	}
}

RDD::ShaderID RenderingDeviceDriverMetal::shader_create_from_bytecode(const Vector<uint8_t> &p_shader_binary, ShaderDescription &r_shader_desc, String &r_name, const Vector<ImmutableSampler> &p_immutable_samplers) {
	r_shader_desc = {}; // Driver-agnostic.

	const uint8_t *binptr = p_shader_binary.ptr();
	uint32_t binsize = p_shader_binary.size();

	BufReader reader(binptr, binsize);
	uint8_t header[4];
	reader.read((uint32_t &)header);
	ERR_FAIL_COND_V_MSG(memcmp(header, "GMSL", 4) != 0, ShaderID(), "Invalid header");
	uint32_t version = 0;
	reader.read(version);
	ERR_FAIL_COND_V_MSG(version != SHADER_BINARY_VERSION, ShaderID(), "Invalid shader binary version");

	ShaderBinaryData binary_data;
	binary_data.deserialize(reader);
	switch (reader.status) {
		case BufReader::Status::OK:
			break;
		case BufReader::Status::BAD_COMPRESSION:
			ERR_FAIL_V_MSG(ShaderID(), "Invalid compressed data");
		case BufReader::Status::SHORT_BUFFER:
			ERR_FAIL_V_MSG(ShaderID(), "Unexpected end of buffer");
	}

	// We need to regenerate the shader if the cache is moved to an incompatible device.
	ERR_FAIL_COND_V_MSG(device_properties->features.argument_buffers_tier < MTLArgumentBuffersTier2 && binary_data.uses_argument_buffers(),
			ShaderID(),
			"Shader was generated with argument buffers, but device has limited support");

	MTLCompileOptions *options = [MTLCompileOptions new];
	options.languageVersion = binary_data.get_msl_version();
	HashMap<ShaderStage, MDLibrary *> libraries;

	for (ShaderStageData &shader_data : binary_data.stages) {
		SHA256Digest key = SHA256Digest(shader_data.source.ptr(), shader_data.source.length());

		if (ShaderCacheEntry **p = _shader_cache.getptr(key); p != nullptr) {
			libraries[shader_data.stage] = (*p)->library;
			continue;
		}

		NSString *source = [[NSString alloc] initWithBytes:(void *)shader_data.source.ptr()
													length:shader_data.source.length()
												  encoding:NSUTF8StringEncoding];

		ShaderCacheEntry *cd = memnew(ShaderCacheEntry(*this, key));
		cd->name = binary_data.shader_name;
		cd->stage = shader_data.stage;
		options.preserveInvariance = shader_data.is_position_invariant;
		options.fastMathEnabled = YES;
		MDLibrary *library = [MDLibrary newLibraryWithCacheEntry:cd
														  device:device
														  source:source
														 options:options
														strategy:_shader_load_strategy];
		_shader_cache[key] = cd;
		libraries[shader_data.stage] = library;
	}

	Vector<UniformSet> uniform_sets;
	uniform_sets.resize(binary_data.uniforms.size());

	r_shader_desc.uniform_sets.resize(binary_data.uniforms.size());

	// Create sets.
	for (UniformSetData &uniform_set : binary_data.uniforms) {
		UniformSet &set = uniform_sets.write[uniform_set.index];
		set.uniforms.resize(uniform_set.uniforms.size());

		Vector<ShaderUniform> &uset = r_shader_desc.uniform_sets.write[uniform_set.index];
		uset.resize(uniform_set.uniforms.size());

		for (uint32_t i = 0; i < uniform_set.uniforms.size(); i++) {
			UniformData &uniform = uniform_set.uniforms[i];

			ShaderUniform su;
			su.type = uniform.type;
			su.writable = uniform.writable;
			su.length = uniform.length;
			su.binding = uniform.binding;
			su.stages = uniform.stages;
			uset.write[i] = su;

			UniformInfo ui;
			ui.binding = uniform.binding;
			ui.active_stages = uniform.active_stages;
			for (KeyValue<RDC::ShaderStage, BindingInfo> &kv : uniform.bindings) {
				ui.bindings.insert(kv.key, kv.value);
			}
			for (KeyValue<RDC::ShaderStage, BindingInfo> &kv : uniform.bindings_secondary) {
				ui.bindings_secondary.insert(kv.key, kv.value);
			}
			set.uniforms[i] = ui;
		}
	}
	for (UniformSetData &uniform_set : binary_data.uniforms) {
		UniformSet &set = uniform_sets.write[uniform_set.index];

		// Make encoders.
		for (ShaderStageData const &stage_data : binary_data.stages) {
			ShaderStage stage = stage_data.stage;
			NSMutableArray<MTLArgumentDescriptor *> *descriptors = [NSMutableArray new];

			for (UniformInfo const &uniform : set.uniforms) {
				BindingInfo const *binding_info = uniform.bindings.getptr(stage);
				if (binding_info == nullptr)
					continue;

				[descriptors addObject:binding_info->new_argument_descriptor()];
				BindingInfo const *secondary_binding_info = uniform.bindings_secondary.getptr(stage);
				if (secondary_binding_info != nullptr) {
					[descriptors addObject:secondary_binding_info->new_argument_descriptor()];
				}
			}

			if (descriptors.count == 0) {
				// No bindings.
				continue;
			}
			// Sort by index.
			[descriptors sortUsingComparator:^NSComparisonResult(MTLArgumentDescriptor *a, MTLArgumentDescriptor *b) {
				if (a.index < b.index) {
					return NSOrderedAscending;
				} else if (a.index > b.index) {
					return NSOrderedDescending;
				} else {
					return NSOrderedSame;
				}
			}];

			id<MTLArgumentEncoder> enc = [device newArgumentEncoderWithArguments:descriptors];
			set.encoders[stage] = enc;
			set.offsets[stage] = set.buffer_size;
			set.buffer_size += enc.encodedLength;
		}
	}

	r_shader_desc.specialization_constants.resize(binary_data.constants.size());
	for (uint32_t i = 0; i < binary_data.constants.size(); i++) {
		SpecializationConstantData &c = binary_data.constants[i];

		ShaderSpecializationConstant sc;
		sc.type = c.type;
		sc.constant_id = c.constant_id;
		sc.int_value = c.int_value;
		sc.stages = c.stages;
		r_shader_desc.specialization_constants.write[i] = sc;
	}

	MDShader *shader = nullptr;
	if (binary_data.is_compute()) {
		MDComputeShader *cs = new MDComputeShader(
				binary_data.shader_name,
				uniform_sets,
				binary_data.uses_argument_buffers(),
				libraries[ShaderStage::SHADER_STAGE_COMPUTE]);

		uint32_t *binding = binary_data.push_constant.msl_binding.getptr(SHADER_STAGE_COMPUTE);
		if (binding) {
			cs->push_constants.size = binary_data.push_constant.size;
			cs->push_constants.binding = *binding;
		}

		cs->local = MTLSizeMake(binary_data.compute_local_size.x, binary_data.compute_local_size.y, binary_data.compute_local_size.z);
#if DEV_ENABLED
		cs->kernel_source = binary_data.stages[0].source;
#endif
		shader = cs;
	} else {
		MDRenderShader *rs = new MDRenderShader(
				binary_data.shader_name,
				uniform_sets,
				binary_data.needs_view_mask_buffer(),
				binary_data.uses_argument_buffers(),
				libraries[ShaderStage::SHADER_STAGE_VERTEX],
				libraries[ShaderStage::SHADER_STAGE_FRAGMENT]);

		uint32_t *vert_binding = binary_data.push_constant.msl_binding.getptr(SHADER_STAGE_VERTEX);
		if (vert_binding) {
			rs->push_constants.vert.size = binary_data.push_constant.size;
			rs->push_constants.vert.binding = *vert_binding;
		}
		uint32_t *frag_binding = binary_data.push_constant.msl_binding.getptr(SHADER_STAGE_FRAGMENT);
		if (frag_binding) {
			rs->push_constants.frag.size = binary_data.push_constant.size;
			rs->push_constants.frag.binding = *frag_binding;
		}

#if DEV_ENABLED
		for (ShaderStageData &stage_data : binary_data.stages) {
			if (stage_data.stage == ShaderStage::SHADER_STAGE_VERTEX) {
				rs->vert_source = stage_data.source;
			} else if (stage_data.stage == ShaderStage::SHADER_STAGE_FRAGMENT) {
				rs->frag_source = stage_data.source;
			}
		}
#endif
		shader = rs;
	}

	r_shader_desc.vertex_input_mask = binary_data.vertex_input_mask;
	r_shader_desc.fragment_output_mask = binary_data.fragment_output_mask;
	r_shader_desc.is_compute = binary_data.is_compute();
	r_shader_desc.compute_local_size[0] = binary_data.compute_local_size.x;
	r_shader_desc.compute_local_size[1] = binary_data.compute_local_size.y;
	r_shader_desc.compute_local_size[2] = binary_data.compute_local_size.z;
	r_shader_desc.push_constant_size = binary_data.push_constant.size;

	return ShaderID(shader);
}

void RenderingDeviceDriverMetal::shader_free(ShaderID p_shader) {
	MDShader *obj = (MDShader *)p_shader.id;
	delete obj;
}

void RenderingDeviceDriverMetal::shader_destroy_modules(ShaderID p_shader) {
	// TODO.
}

/*********************/
/**** UNIFORM SET ****/
/*********************/

RDD::UniformSetID RenderingDeviceDriverMetal::uniform_set_create(VectorView<BoundUniform> p_uniforms, ShaderID p_shader, uint32_t p_set_index, int p_linear_pool_index) {
	//p_linear_pool_index = -1; // TODO:? Linear pools not implemented or not supported by API backend.

	MDUniformSet *set = memnew(MDUniformSet);
	Vector<BoundUniform> bound_uniforms;
	bound_uniforms.resize(p_uniforms.size());
	for (uint32_t i = 0; i < p_uniforms.size(); i += 1) {
		bound_uniforms.write[i] = p_uniforms[i];
	}
	set->uniforms = bound_uniforms;
	set->index = p_set_index;

	return UniformSetID(set);
}

void RenderingDeviceDriverMetal::uniform_set_free(UniformSetID p_uniform_set) {
	MDUniformSet *obj = (MDUniformSet *)p_uniform_set.id;
	memdelete(obj);
}

void RenderingDeviceDriverMetal::command_uniform_set_prepare_for_use(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
}

#pragma mark - Transfer

void RenderingDeviceDriverMetal::command_clear_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, uint64_t p_offset, uint64_t p_size) {
	MDCommandBuffer *cmd = (MDCommandBuffer *)(p_cmd_buffer.id);
	id<MTLBuffer> buffer = rid::get(p_buffer);

	id<MTLBlitCommandEncoder> blit = cmd->blit_command_encoder();
	[blit fillBuffer:buffer
			   range:NSMakeRange(p_offset, p_size)
			   value:0];
}

void RenderingDeviceDriverMetal::command_copy_buffer(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, BufferID p_dst_buffer, VectorView<BufferCopyRegion> p_regions) {
	MDCommandBuffer *cmd = (MDCommandBuffer *)(p_cmd_buffer.id);
	id<MTLBuffer> src = rid::get(p_src_buffer);
	id<MTLBuffer> dst = rid::get(p_dst_buffer);

	id<MTLBlitCommandEncoder> blit = cmd->blit_command_encoder();

	for (uint32_t i = 0; i < p_regions.size(); i++) {
		BufferCopyRegion region = p_regions[i];
		[blit copyFromBuffer:src
					 sourceOffset:region.src_offset
						 toBuffer:dst
				destinationOffset:region.dst_offset
							 size:region.size];
	}
}

MTLSize MTLSizeFromVector3i(Vector3i p_size) {
	return MTLSizeMake(p_size.x, p_size.y, p_size.z);
}

MTLOrigin MTLOriginFromVector3i(Vector3i p_origin) {
	return MTLOriginMake(p_origin.x, p_origin.y, p_origin.z);
}

// Clamps the size so that the sum of the origin and size do not exceed the maximum size.
static inline MTLSize clampMTLSize(MTLSize p_size, MTLOrigin p_origin, MTLSize p_max_size) {
	MTLSize clamped;
	clamped.width = MIN(p_size.width, p_max_size.width - p_origin.x);
	clamped.height = MIN(p_size.height, p_max_size.height - p_origin.y);
	clamped.depth = MIN(p_size.depth, p_max_size.depth - p_origin.z);
	return clamped;
}

void RenderingDeviceDriverMetal::command_copy_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<TextureCopyRegion> p_regions) {
	MDCommandBuffer *cmd = (MDCommandBuffer *)(p_cmd_buffer.id);
	id<MTLTexture> src = rid::get(p_src_texture);
	id<MTLTexture> dst = rid::get(p_dst_texture);

	id<MTLBlitCommandEncoder> blit = cmd->blit_command_encoder();
	PixelFormats &pf = *pixel_formats;

	MTLPixelFormat src_fmt = src.pixelFormat;
	bool src_is_compressed = pf.getFormatType(src_fmt) == MTLFormatType::Compressed;
	MTLPixelFormat dst_fmt = dst.pixelFormat;
	bool dst_is_compressed = pf.getFormatType(dst_fmt) == MTLFormatType::Compressed;

	// Validate copy.
	if (src.sampleCount != dst.sampleCount || pf.getBytesPerBlock(src_fmt) != pf.getBytesPerBlock(dst_fmt)) {
		ERR_FAIL_MSG("Cannot copy between incompatible pixel formats, such as formats of different pixel sizes, or between images with different sample counts.");
	}

	// If source and destination have different formats and at least one is compressed, a temporary buffer is required.
	bool need_tmp_buffer = (src_fmt != dst_fmt) && (src_is_compressed || dst_is_compressed);
	if (need_tmp_buffer) {
		ERR_FAIL_MSG("not implemented: copy with intermediate buffer");
	}

	if (src_fmt != dst_fmt) {
		// Map the source pixel format to the dst through a texture view on the source texture.
		src = [src newTextureViewWithPixelFormat:dst_fmt];
	}

	for (uint32_t i = 0; i < p_regions.size(); i++) {
		TextureCopyRegion region = p_regions[i];

		MTLSize extent = MTLSizeFromVector3i(region.size);

		// If copies can be performed using direct texture-texture copying, do so.
		uint32_t src_level = region.src_subresources.mipmap;
		uint32_t src_base_layer = region.src_subresources.base_layer;
		MTLSize src_extent = mipmapLevelSizeFromTexture(src, src_level);
		uint32_t dst_level = region.dst_subresources.mipmap;
		uint32_t dst_base_layer = region.dst_subresources.base_layer;
		MTLSize dst_extent = mipmapLevelSizeFromTexture(dst, dst_level);

		// All layers may be copied at once, if the extent completely covers both images.
		if (src_extent == extent && dst_extent == extent) {
			[blit copyFromTexture:src
						 sourceSlice:src_base_layer
						 sourceLevel:src_level
						   toTexture:dst
					destinationSlice:dst_base_layer
					destinationLevel:dst_level
						  sliceCount:region.src_subresources.layer_count
						  levelCount:1];
		} else {
			MTLOrigin src_origin = MTLOriginFromVector3i(region.src_offset);
			MTLSize src_size = clampMTLSize(extent, src_origin, src_extent);
			uint32_t layer_count = 0;
			if ((src.textureType == MTLTextureType3D) != (dst.textureType == MTLTextureType3D)) {
				// In the case, the number of layers to copy is in extent.depth. Use that value,
				// then clamp the depth, so we don't try to copy more than Metal will allow.
				layer_count = extent.depth;
				src_size.depth = 1;
			} else {
				layer_count = region.src_subresources.layer_count;
			}
			MTLOrigin dst_origin = MTLOriginFromVector3i(region.dst_offset);

			for (uint32_t layer = 0; layer < layer_count; layer++) {
				// We can copy between a 3D and a 2D image easily. Just copy between
				// one slice of the 2D image and one plane of the 3D image at a time.
				if ((src.textureType == MTLTextureType3D) == (dst.textureType == MTLTextureType3D)) {
					[blit copyFromTexture:src
								  sourceSlice:src_base_layer + layer
								  sourceLevel:src_level
								 sourceOrigin:src_origin
								   sourceSize:src_size
									toTexture:dst
							 destinationSlice:dst_base_layer + layer
							 destinationLevel:dst_level
							destinationOrigin:dst_origin];
				} else if (src.textureType == MTLTextureType3D) {
					[blit copyFromTexture:src
								  sourceSlice:src_base_layer
								  sourceLevel:src_level
								 sourceOrigin:MTLOriginMake(src_origin.x, src_origin.y, src_origin.z + layer)
								   sourceSize:src_size
									toTexture:dst
							 destinationSlice:dst_base_layer + layer
							 destinationLevel:dst_level
							destinationOrigin:dst_origin];
				} else {
					DEV_ASSERT(dst.textureType == MTLTextureType3D);
					[blit copyFromTexture:src
								  sourceSlice:src_base_layer + layer
								  sourceLevel:src_level
								 sourceOrigin:src_origin
								   sourceSize:src_size
									toTexture:dst
							 destinationSlice:dst_base_layer
							 destinationLevel:dst_level
							destinationOrigin:MTLOriginMake(dst_origin.x, dst_origin.y, dst_origin.z + layer)];
				}
			}
		}
	}
}

void RenderingDeviceDriverMetal::command_resolve_texture(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, uint32_t p_src_layer, uint32_t p_src_mipmap, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, uint32_t p_dst_layer, uint32_t p_dst_mipmap) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	id<MTLTexture> src_tex = rid::get(p_src_texture);
	id<MTLTexture> dst_tex = rid::get(p_dst_texture);

	MTLRenderPassDescriptor *mtlRPD = [MTLRenderPassDescriptor renderPassDescriptor];
	MTLRenderPassColorAttachmentDescriptor *mtlColorAttDesc = mtlRPD.colorAttachments[0];
	mtlColorAttDesc.loadAction = MTLLoadActionLoad;
	mtlColorAttDesc.storeAction = MTLStoreActionMultisampleResolve;

	mtlColorAttDesc.texture = src_tex;
	mtlColorAttDesc.resolveTexture = dst_tex;
	mtlColorAttDesc.level = p_src_mipmap;
	mtlColorAttDesc.slice = p_src_layer;
	mtlColorAttDesc.resolveLevel = p_dst_mipmap;
	mtlColorAttDesc.resolveSlice = p_dst_layer;
	cb->encodeRenderCommandEncoderWithDescriptor(mtlRPD, @"Resolve Image");
}

void RenderingDeviceDriverMetal::command_clear_color_texture(CommandBufferID p_cmd_buffer, TextureID p_texture, TextureLayout p_texture_layout, const Color &p_color, const TextureSubresourceRange &p_subresources) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	id<MTLTexture> src_tex = rid::get(p_texture);

	if (src_tex.parentTexture) {
		// Clear via the parent texture rather than the view.
		src_tex = src_tex.parentTexture;
	}

	PixelFormats &pf = *pixel_formats;

	if (pf.isDepthFormat(src_tex.pixelFormat) || pf.isStencilFormat(src_tex.pixelFormat)) {
		ERR_FAIL_MSG("invalid: depth or stencil texture format");
	}

	MTLRenderPassDescriptor *desc = MTLRenderPassDescriptor.renderPassDescriptor;

	if (p_subresources.aspect.has_flag(TEXTURE_ASPECT_COLOR_BIT)) {
		MTLRenderPassColorAttachmentDescriptor *caDesc = desc.colorAttachments[0];
		caDesc.texture = src_tex;
		caDesc.loadAction = MTLLoadActionClear;
		caDesc.storeAction = MTLStoreActionStore;
		caDesc.clearColor = MTLClearColorMake(p_color.r, p_color.g, p_color.b, p_color.a);

		// Extract the mipmap levels that are to be updated.
		uint32_t mipLvlStart = p_subresources.base_mipmap;
		uint32_t mipLvlCnt = p_subresources.mipmap_count;
		uint32_t mipLvlEnd = mipLvlStart + mipLvlCnt;

		uint32_t levelCount = src_tex.mipmapLevelCount;

		// Extract the cube or array layers (slices) that are to be updated.
		bool is3D = src_tex.textureType == MTLTextureType3D;
		uint32_t layerStart = is3D ? 0 : p_subresources.base_layer;
		uint32_t layerCnt = p_subresources.layer_count;
		uint32_t layerEnd = layerStart + layerCnt;

		MetalFeatures const &features = (*device_properties).features;

		// Iterate across mipmap levels and layers, and perform and empty render to clear each.
		for (uint32_t mipLvl = mipLvlStart; mipLvl < mipLvlEnd; mipLvl++) {
			ERR_FAIL_INDEX_MSG(mipLvl, levelCount, "mip level out of range");

			caDesc.level = mipLvl;

			// If a 3D image, we need to get the depth for each level.
			if (is3D) {
				layerCnt = mipmapLevelSizeFromTexture(src_tex, mipLvl).depth;
				layerEnd = layerStart + layerCnt;
			}

			if ((features.layeredRendering && src_tex.sampleCount == 1) || features.multisampleLayeredRendering) {
				// We can clear all layers at once.
				if (is3D) {
					caDesc.depthPlane = layerStart;
				} else {
					caDesc.slice = layerStart;
				}
				desc.renderTargetArrayLength = layerCnt;
				cb->encodeRenderCommandEncoderWithDescriptor(desc, @"Clear Image");
			} else {
				for (uint32_t layer = layerStart; layer < layerEnd; layer++) {
					if (is3D) {
						caDesc.depthPlane = layer;
					} else {
						caDesc.slice = layer;
					}
					cb->encodeRenderCommandEncoderWithDescriptor(desc, @"Clear Image");
				}
			}
		}
	}
}

API_AVAILABLE(macos(11.0), ios(14.0))
bool isArrayTexture(MTLTextureType p_type) {
	return (p_type == MTLTextureType3D ||
			p_type == MTLTextureType2DArray ||
			p_type == MTLTextureType2DMultisampleArray ||
			p_type == MTLTextureType1DArray);
}

void RenderingDeviceDriverMetal::_copy_texture_buffer(CommandBufferID p_cmd_buffer,
		CopySource p_source,
		TextureID p_texture,
		BufferID p_buffer,
		VectorView<BufferTextureCopyRegion> p_regions) {
	MDCommandBuffer *cmd = (MDCommandBuffer *)(p_cmd_buffer.id);
	id<MTLBuffer> buffer = rid::get(p_buffer);
	id<MTLTexture> texture = rid::get(p_texture);

	id<MTLBlitCommandEncoder> enc = cmd->blit_command_encoder();

	PixelFormats &pf = *pixel_formats;
	MTLPixelFormat mtlPixFmt = texture.pixelFormat;

	MTLBlitOption options = MTLBlitOptionNone;
	if (pf.isPVRTCFormat(mtlPixFmt)) {
		options |= MTLBlitOptionRowLinearPVRTC;
	}

	for (uint32_t i = 0; i < p_regions.size(); i++) {
		BufferTextureCopyRegion region = p_regions[i];

		uint32_t mip_level = region.texture_subresources.mipmap;
		MTLOrigin txt_origin = MTLOriginMake(region.texture_offset.x, region.texture_offset.y, region.texture_offset.z);
		MTLSize src_extent = mipmapLevelSizeFromTexture(texture, mip_level);
		MTLSize txt_size = clampMTLSize(MTLSizeMake(region.texture_region_size.x, region.texture_region_size.y, region.texture_region_size.z),
				txt_origin,
				src_extent);

		uint32_t buffImgWd = region.texture_region_size.x;
		uint32_t buffImgHt = region.texture_region_size.y;

		NSUInteger bytesPerRow = pf.getBytesPerRow(mtlPixFmt, buffImgWd);
		NSUInteger bytesPerImg = pf.getBytesPerLayer(mtlPixFmt, bytesPerRow, buffImgHt);

		MTLBlitOption blit_options = options;

		if (pf.isDepthFormat(mtlPixFmt) && pf.isStencilFormat(mtlPixFmt)) {
			bool want_depth = flags::all(region.texture_subresources.aspect, TEXTURE_ASPECT_DEPTH_BIT);
			bool want_stencil = flags::all(region.texture_subresources.aspect, TEXTURE_ASPECT_STENCIL_BIT);

			// The stencil component is always 1 byte per pixel.
			// Don't reduce depths of 32-bit depth/stencil formats.
			if (want_depth && !want_stencil) {
				if (pf.getBytesPerTexel(mtlPixFmt) != 4) {
					bytesPerRow -= buffImgWd;
					bytesPerImg -= buffImgWd * buffImgHt;
				}
				blit_options |= MTLBlitOptionDepthFromDepthStencil;
			} else if (want_stencil && !want_depth) {
				bytesPerRow = buffImgWd;
				bytesPerImg = buffImgWd * buffImgHt;
				blit_options |= MTLBlitOptionStencilFromDepthStencil;
			}
		}

		if (!isArrayTexture(texture.textureType)) {
			bytesPerImg = 0;
		}

		if (p_source == CopySource::Buffer) {
			for (uint32_t lyrIdx = 0; lyrIdx < region.texture_subresources.layer_count; lyrIdx++) {
				[enc copyFromBuffer:buffer
							   sourceOffset:region.buffer_offset + (bytesPerImg * lyrIdx)
						  sourceBytesPerRow:bytesPerRow
						sourceBytesPerImage:bytesPerImg
								 sourceSize:txt_size
								  toTexture:texture
						   destinationSlice:region.texture_subresources.base_layer + lyrIdx
						   destinationLevel:mip_level
						  destinationOrigin:txt_origin
									options:blit_options];
			}
		} else {
			for (uint32_t lyrIdx = 0; lyrIdx < region.texture_subresources.layer_count; lyrIdx++) {
				[enc copyFromTexture:texture
									 sourceSlice:region.texture_subresources.base_layer + lyrIdx
									 sourceLevel:mip_level
									sourceOrigin:txt_origin
									  sourceSize:txt_size
										toBuffer:buffer
							   destinationOffset:region.buffer_offset + (bytesPerImg * lyrIdx)
						  destinationBytesPerRow:bytesPerRow
						destinationBytesPerImage:bytesPerImg
										 options:blit_options];
			}
		}
	}
}

void RenderingDeviceDriverMetal::command_copy_buffer_to_texture(CommandBufferID p_cmd_buffer, BufferID p_src_buffer, TextureID p_dst_texture, TextureLayout p_dst_texture_layout, VectorView<BufferTextureCopyRegion> p_regions) {
	_copy_texture_buffer(p_cmd_buffer, CopySource::Buffer, p_dst_texture, p_src_buffer, p_regions);
}

void RenderingDeviceDriverMetal::command_copy_texture_to_buffer(CommandBufferID p_cmd_buffer, TextureID p_src_texture, TextureLayout p_src_texture_layout, BufferID p_dst_buffer, VectorView<BufferTextureCopyRegion> p_regions) {
	_copy_texture_buffer(p_cmd_buffer, CopySource::Texture, p_src_texture, p_dst_buffer, p_regions);
}

#pragma mark - Pipeline

void RenderingDeviceDriverMetal::pipeline_free(PipelineID p_pipeline_id) {
	MDPipeline *obj = (MDPipeline *)(p_pipeline_id.id);
	delete obj;
}

// ----- BINDING -----

void RenderingDeviceDriverMetal::command_bind_push_constants(CommandBufferID p_cmd_buffer, ShaderID p_shader, uint32_t p_dst_first_index, VectorView<uint32_t> p_data) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	MDShader *shader = (MDShader *)(p_shader.id);
	shader->encode_push_constant_data(p_data, cb);
}

// ----- CACHE -----

String RenderingDeviceDriverMetal::_pipeline_get_cache_path() const {
	String path = OS::get_singleton()->get_user_data_dir() + "/metal/pipelines";
	path += "." + context_device.name.validate_filename().replace(" ", "_").to_lower();
	if (Engine::get_singleton()->is_editor_hint()) {
		path += ".editor";
	}
	path += ".cache";

	return path;
}

bool RenderingDeviceDriverMetal::pipeline_cache_create(const Vector<uint8_t> &p_data) {
	return false;
	CharString path = _pipeline_get_cache_path().utf8();
	NSString *nPath = [[NSString alloc] initWithBytesNoCopy:path.ptrw()
													 length:path.length()
												   encoding:NSUTF8StringEncoding
											   freeWhenDone:NO];
	MTLBinaryArchiveDescriptor *desc = [MTLBinaryArchiveDescriptor new];
	if ([[NSFileManager defaultManager] fileExistsAtPath:nPath]) {
		desc.url = [NSURL fileURLWithPath:nPath];
	}
	NSError *error = nil;
	archive = [device newBinaryArchiveWithDescriptor:desc error:&error];
	return true;
}

void RenderingDeviceDriverMetal::pipeline_cache_free() {
	archive = nil;
}

size_t RenderingDeviceDriverMetal::pipeline_cache_query_size() {
	return archive_count * 1024;
}

Vector<uint8_t> RenderingDeviceDriverMetal::pipeline_cache_serialize() {
	if (!archive) {
		return Vector<uint8_t>();
	}

	CharString path = _pipeline_get_cache_path().utf8();

	NSString *nPath = [[NSString alloc] initWithBytesNoCopy:path.ptrw()
													 length:path.length()
												   encoding:NSUTF8StringEncoding
											   freeWhenDone:NO];
	NSURL *target = [NSURL fileURLWithPath:nPath];
	NSError *error = nil;
	if ([archive serializeToURL:target error:&error]) {
		return Vector<uint8_t>();
	} else {
		print_line(error.localizedDescription.UTF8String);
		return Vector<uint8_t>();
	}
}

#pragma mark - Rendering

// ----- SUBPASS -----

RDD::RenderPassID RenderingDeviceDriverMetal::render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_subpass_dependencies, uint32_t p_view_count) {
	PixelFormats &pf = *pixel_formats;

	size_t subpass_count = p_subpasses.size();

	Vector<MDSubpass> subpasses;
	subpasses.resize(subpass_count);
	for (uint32_t i = 0; i < subpass_count; i++) {
		MDSubpass &subpass = subpasses.write[i];
		subpass.subpass_index = i;
		subpass.view_count = p_view_count;
		subpass.input_references = p_subpasses[i].input_references;
		subpass.color_references = p_subpasses[i].color_references;
		subpass.depth_stencil_reference = p_subpasses[i].depth_stencil_reference;
		subpass.resolve_references = p_subpasses[i].resolve_references;
	}

	static const MTLLoadAction LOAD_ACTIONS[] = {
		[ATTACHMENT_LOAD_OP_LOAD] = MTLLoadActionLoad,
		[ATTACHMENT_LOAD_OP_CLEAR] = MTLLoadActionClear,
		[ATTACHMENT_LOAD_OP_DONT_CARE] = MTLLoadActionDontCare,
	};

	static const MTLStoreAction STORE_ACTIONS[] = {
		[ATTACHMENT_STORE_OP_STORE] = MTLStoreActionStore,
		[ATTACHMENT_STORE_OP_DONT_CARE] = MTLStoreActionDontCare,
	};

	Vector<MDAttachment> attachments;
	attachments.resize(p_attachments.size());

	for (uint32_t i = 0; i < p_attachments.size(); i++) {
		Attachment const &a = p_attachments[i];
		MDAttachment &mda = attachments.write[i];
		MTLPixelFormat format = pf.getMTLPixelFormat(a.format);
		mda.format = format;
		if (a.samples > TEXTURE_SAMPLES_1) {
			mda.samples = (*device_properties).find_nearest_supported_sample_count(a.samples);
		}
		mda.loadAction = LOAD_ACTIONS[a.load_op];
		mda.storeAction = STORE_ACTIONS[a.store_op];
		bool is_depth = pf.isDepthFormat(format);
		if (is_depth) {
			mda.type |= MDAttachmentType::Depth;
		}
		bool is_stencil = pf.isStencilFormat(format);
		if (is_stencil) {
			mda.type |= MDAttachmentType::Stencil;
			mda.stencilLoadAction = LOAD_ACTIONS[a.stencil_load_op];
			mda.stencilStoreAction = STORE_ACTIONS[a.stencil_store_op];
		}
		if (!is_depth && !is_stencil) {
			mda.type |= MDAttachmentType::Color;
		}
	}
	MDRenderPass *obj = new MDRenderPass(attachments, subpasses);
	return RenderPassID(obj);
}

void RenderingDeviceDriverMetal::render_pass_free(RenderPassID p_render_pass) {
	MDRenderPass *obj = (MDRenderPass *)(p_render_pass.id);
	delete obj;
}

// ----- COMMANDS -----

void RenderingDeviceDriverMetal::command_begin_render_pass(CommandBufferID p_cmd_buffer, RenderPassID p_render_pass, FramebufferID p_framebuffer, CommandBufferType p_cmd_buffer_type, const Rect2i &p_rect, VectorView<RenderPassClearValue> p_clear_values) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_begin_pass(p_render_pass, p_framebuffer, p_cmd_buffer_type, p_rect, p_clear_values);
}

void RenderingDeviceDriverMetal::command_end_render_pass(CommandBufferID p_cmd_buffer) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_end_pass();
}

void RenderingDeviceDriverMetal::command_next_render_subpass(CommandBufferID p_cmd_buffer, CommandBufferType p_cmd_buffer_type) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_next_subpass();
}

void RenderingDeviceDriverMetal::command_render_set_viewport(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_viewports) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_set_viewport(p_viewports);
}

void RenderingDeviceDriverMetal::command_render_set_scissor(CommandBufferID p_cmd_buffer, VectorView<Rect2i> p_scissors) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_set_scissor(p_scissors);
}

void RenderingDeviceDriverMetal::command_render_clear_attachments(CommandBufferID p_cmd_buffer, VectorView<AttachmentClear> p_attachment_clears, VectorView<Rect2i> p_rects) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_clear_attachments(p_attachment_clears, p_rects);
}

void RenderingDeviceDriverMetal::command_bind_render_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->bind_pipeline(p_pipeline);
}

void RenderingDeviceDriverMetal::command_bind_render_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_bind_uniform_set(p_uniform_set, p_shader, p_set_index);
}

void RenderingDeviceDriverMetal::command_bind_render_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_bind_uniform_sets(p_uniform_sets, p_shader, p_first_set_index, p_set_count);
}

void RenderingDeviceDriverMetal::command_render_draw(CommandBufferID p_cmd_buffer, uint32_t p_vertex_count, uint32_t p_instance_count, uint32_t p_base_vertex, uint32_t p_first_instance) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_draw(p_vertex_count, p_instance_count, p_base_vertex, p_first_instance);
}

void RenderingDeviceDriverMetal::command_render_draw_indexed(CommandBufferID p_cmd_buffer, uint32_t p_index_count, uint32_t p_instance_count, uint32_t p_first_index, int32_t p_vertex_offset, uint32_t p_first_instance) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_draw_indexed(p_index_count, p_instance_count, p_first_index, p_vertex_offset, p_first_instance);
}

void RenderingDeviceDriverMetal::command_render_draw_indexed_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_draw_indexed_indirect(p_indirect_buffer, p_offset, p_draw_count, p_stride);
}

void RenderingDeviceDriverMetal::command_render_draw_indexed_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_draw_indexed_indirect_count(p_indirect_buffer, p_offset, p_count_buffer, p_count_buffer_offset, p_max_draw_count, p_stride);
}

void RenderingDeviceDriverMetal::command_render_draw_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, uint32_t p_draw_count, uint32_t p_stride) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_draw_indirect(p_indirect_buffer, p_offset, p_draw_count, p_stride);
}

void RenderingDeviceDriverMetal::command_render_draw_indirect_count(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset, BufferID p_count_buffer, uint64_t p_count_buffer_offset, uint32_t p_max_draw_count, uint32_t p_stride) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_draw_indirect_count(p_indirect_buffer, p_offset, p_count_buffer, p_count_buffer_offset, p_max_draw_count, p_stride);
}

void RenderingDeviceDriverMetal::command_render_bind_vertex_buffers(CommandBufferID p_cmd_buffer, uint32_t p_binding_count, const BufferID *p_buffers, const uint64_t *p_offsets) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_bind_vertex_buffers(p_binding_count, p_buffers, p_offsets);
}

void RenderingDeviceDriverMetal::command_render_bind_index_buffer(CommandBufferID p_cmd_buffer, BufferID p_buffer, IndexBufferFormat p_format, uint64_t p_offset) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_bind_index_buffer(p_buffer, p_format, p_offset);
}

void RenderingDeviceDriverMetal::command_render_set_blend_constants(CommandBufferID p_cmd_buffer, const Color &p_constants) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->render_set_blend_constants(p_constants);
}

void RenderingDeviceDriverMetal::command_render_set_line_width(CommandBufferID p_cmd_buffer, float p_width) {
	if (!Math::is_equal_approx(p_width, 1.0f)) {
		ERR_FAIL_MSG("Setting line widths other than 1.0 is not supported by the Metal rendering driver.");
	}
}

// ----- PIPELINE -----

RenderingDeviceDriverMetal::Result<id<MTLFunction>> RenderingDeviceDriverMetal::_create_function(MDLibrary *p_library, NSString *p_name, VectorView<PipelineSpecializationConstant> &p_specialization_constants) {
	id<MTLLibrary> library = p_library.library;
	if (!library) {
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Failed to compile Metal library");
	}

	id<MTLFunction> function = [library newFunctionWithName:p_name];
	ERR_FAIL_NULL_V_MSG(function, ERR_CANT_CREATE, "No function named main0");

	if (function.functionConstantsDictionary.count == 0) {
		return function;
	}

	NSArray<MTLFunctionConstant *> *constants = function.functionConstantsDictionary.allValues;
	bool is_sorted = true;
	for (uint32_t i = 1; i < constants.count; i++) {
		if (constants[i - 1].index > constants[i].index) {
			is_sorted = false;
			break;
		}
	}

	if (!is_sorted) {
		constants = [constants sortedArrayUsingComparator:^NSComparisonResult(MTLFunctionConstant *a, MTLFunctionConstant *b) {
			if (a.index < b.index) {
				return NSOrderedAscending;
			} else if (a.index > b.index) {
				return NSOrderedDescending;
			} else {
				return NSOrderedSame;
			}
		}];
	}

	// Initialize an array of integers representing the indexes of p_specialization_constants
	uint32_t *indexes = (uint32_t *)alloca(p_specialization_constants.size() * sizeof(uint32_t));
	for (uint32_t i = 0; i < p_specialization_constants.size(); i++) {
		indexes[i] = i;
	}
	// Sort the array of integers based on the values in p_specialization_constants
	std::sort(indexes, &indexes[p_specialization_constants.size()], [&](int a, int b) {
		return p_specialization_constants[a].constant_id < p_specialization_constants[b].constant_id;
	});

	MTLFunctionConstantValues *constantValues = [MTLFunctionConstantValues new];
	uint32_t i = 0;
	uint32_t j = 0;
	while (i < constants.count && j < p_specialization_constants.size()) {
		MTLFunctionConstant *curr = constants[i];
		PipelineSpecializationConstant const &sc = p_specialization_constants[indexes[j]];
		if (curr.index == sc.constant_id) {
			switch (curr.type) {
				case MTLDataTypeBool:
				case MTLDataTypeFloat:
				case MTLDataTypeInt:
				case MTLDataTypeUInt: {
					[constantValues setConstantValue:&sc.int_value
												type:curr.type
											 atIndex:sc.constant_id];
				} break;
				default:
					ERR_FAIL_V_MSG(function, "Invalid specialization constant type");
			}
			i++;
			j++;
		} else if (curr.index < sc.constant_id) {
			i++;
		} else {
			j++;
		}
	}

	if (i != constants.count) {
		MTLFunctionConstant *curr = constants[i];
		if (curr.index == R32UI_ALIGNMENT_CONSTANT_ID) {
			uint32_t alignment = 16; // TODO(sgc): is this always correct?
			[constantValues setConstantValue:&alignment
										type:curr.type
									 atIndex:curr.index];
			i++;
		}
	}

	NSError *err = nil;
	function = [library newFunctionWithName:@"main0"
							 constantValues:constantValues
									  error:&err];
	ERR_FAIL_NULL_V_MSG(function, ERR_CANT_CREATE, String("specialized function failed: ") + err.localizedDescription.UTF8String);

	return function;
}

// RDD::PolygonCullMode == MTLCullMode.
static_assert(ENUM_MEMBERS_EQUAL(RDD::POLYGON_CULL_DISABLED, MTLCullModeNone));
static_assert(ENUM_MEMBERS_EQUAL(RDD::POLYGON_CULL_FRONT, MTLCullModeFront));
static_assert(ENUM_MEMBERS_EQUAL(RDD::POLYGON_CULL_BACK, MTLCullModeBack));

// RDD::StencilOperation == MTLStencilOperation.
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_KEEP, MTLStencilOperationKeep));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_ZERO, MTLStencilOperationZero));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_REPLACE, MTLStencilOperationReplace));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_INCREMENT_AND_CLAMP, MTLStencilOperationIncrementClamp));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_DECREMENT_AND_CLAMP, MTLStencilOperationDecrementClamp));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_INVERT, MTLStencilOperationInvert));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_INCREMENT_AND_WRAP, MTLStencilOperationIncrementWrap));
static_assert(ENUM_MEMBERS_EQUAL(RDD::STENCIL_OP_DECREMENT_AND_WRAP, MTLStencilOperationDecrementWrap));

// RDD::BlendOperation == MTLBlendOperation.
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_ADD, MTLBlendOperationAdd));
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_SUBTRACT, MTLBlendOperationSubtract));
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_REVERSE_SUBTRACT, MTLBlendOperationReverseSubtract));
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_MINIMUM, MTLBlendOperationMin));
static_assert(ENUM_MEMBERS_EQUAL(RDD::BLEND_OP_MAXIMUM, MTLBlendOperationMax));

RDD::PipelineID RenderingDeviceDriverMetal::render_pipeline_create(
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
		VectorView<PipelineSpecializationConstant> p_specialization_constants) {
	MDRenderShader *shader = (MDRenderShader *)(p_shader.id);
	MTLVertexDescriptor *vert_desc = rid::get(p_vertex_format);
	MDRenderPass *pass = (MDRenderPass *)(p_render_pass.id);

	os_signpost_id_t reflect_id = os_signpost_id_make_with_pointer(LOG_INTERVALS, shader);
	os_signpost_interval_begin(LOG_INTERVALS, reflect_id, "render_pipeline_create", "shader_name=%{public}s", shader->name.get_data());
	DEFER([=]() {
		os_signpost_interval_end(LOG_INTERVALS, reflect_id, "render_pipeline_create");
	});

	os_signpost_event_emit(LOG_DRIVER, OS_SIGNPOST_ID_EXCLUSIVE, "create_pipeline");

	MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];

	{
		MDSubpass const &subpass = pass->subpasses[p_render_subpass];
		for (uint32_t i = 0; i < subpass.color_references.size(); i++) {
			uint32_t attachment = subpass.color_references[i].attachment;
			if (attachment != AttachmentReference::UNUSED) {
				MDAttachment const &a = pass->attachments[attachment];
				desc.colorAttachments[i].pixelFormat = a.format;
			}
		}

		if (subpass.depth_stencil_reference.attachment != AttachmentReference::UNUSED) {
			uint32_t attachment = subpass.depth_stencil_reference.attachment;
			MDAttachment const &a = pass->attachments[attachment];

			if (a.type & MDAttachmentType::Depth) {
				desc.depthAttachmentPixelFormat = a.format;
			}

			if (a.type & MDAttachmentType::Stencil) {
				desc.stencilAttachmentPixelFormat = a.format;
			}
		}
	}

	desc.vertexDescriptor = vert_desc;
	desc.label = [NSString stringWithUTF8String:shader->name.get_data()];

	// Input assembly & tessellation.

	MDRenderPipeline *pipeline = new MDRenderPipeline();

	switch (p_render_primitive) {
		case RENDER_PRIMITIVE_POINTS:
			desc.inputPrimitiveTopology = MTLPrimitiveTopologyClassPoint;
			break;
		case RENDER_PRIMITIVE_LINES:
		case RENDER_PRIMITIVE_LINES_WITH_ADJACENCY:
		case RENDER_PRIMITIVE_LINESTRIPS_WITH_ADJACENCY:
		case RENDER_PRIMITIVE_LINESTRIPS:
			desc.inputPrimitiveTopology = MTLPrimitiveTopologyClassLine;
			break;
		case RENDER_PRIMITIVE_TRIANGLES:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS:
		case RENDER_PRIMITIVE_TRIANGLES_WITH_ADJACENCY:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_AJACENCY:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX:
			desc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
			break;
		case RENDER_PRIMITIVE_TESSELATION_PATCH:
			desc.maxTessellationFactor = p_rasterization_state.patch_control_points;
			desc.tessellationPartitionMode = MTLTessellationPartitionModeInteger;
			ERR_FAIL_V_MSG(PipelineID(), "tessellation not implemented");
			break;
		case RENDER_PRIMITIVE_MAX:
		default:
			desc.inputPrimitiveTopology = MTLPrimitiveTopologyClassUnspecified;
			break;
	}

	switch (p_render_primitive) {
		case RENDER_PRIMITIVE_POINTS:
			pipeline->raster_state.render_primitive = MTLPrimitiveTypePoint;
			break;
		case RENDER_PRIMITIVE_LINES:
		case RENDER_PRIMITIVE_LINES_WITH_ADJACENCY:
			pipeline->raster_state.render_primitive = MTLPrimitiveTypeLine;
			break;
		case RENDER_PRIMITIVE_LINESTRIPS:
		case RENDER_PRIMITIVE_LINESTRIPS_WITH_ADJACENCY:
			pipeline->raster_state.render_primitive = MTLPrimitiveTypeLineStrip;
			break;
		case RENDER_PRIMITIVE_TRIANGLES:
		case RENDER_PRIMITIVE_TRIANGLES_WITH_ADJACENCY:
			pipeline->raster_state.render_primitive = MTLPrimitiveTypeTriangle;
			break;
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_AJACENCY:
		case RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX:
			pipeline->raster_state.render_primitive = MTLPrimitiveTypeTriangleStrip;
			break;
		default:
			break;
	}

	// Rasterization.
	desc.rasterizationEnabled = !p_rasterization_state.discard_primitives;
	pipeline->raster_state.clip_mode = p_rasterization_state.enable_depth_clamp ? MTLDepthClipModeClamp : MTLDepthClipModeClip;
	pipeline->raster_state.fill_mode = p_rasterization_state.wireframe ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;

	static const MTLCullMode CULL_MODE[3] = {
		MTLCullModeNone,
		MTLCullModeFront,
		MTLCullModeBack,
	};
	pipeline->raster_state.cull_mode = CULL_MODE[p_rasterization_state.cull_mode];
	pipeline->raster_state.winding = (p_rasterization_state.front_face == POLYGON_FRONT_FACE_CLOCKWISE) ? MTLWindingClockwise : MTLWindingCounterClockwise;
	pipeline->raster_state.depth_bias.enabled = p_rasterization_state.depth_bias_enabled;
	pipeline->raster_state.depth_bias.depth_bias = p_rasterization_state.depth_bias_constant_factor;
	pipeline->raster_state.depth_bias.slope_scale = p_rasterization_state.depth_bias_slope_factor;
	pipeline->raster_state.depth_bias.clamp = p_rasterization_state.depth_bias_clamp;
	// In Metal there is no line width.
	if (!Math::is_equal_approx(p_rasterization_state.line_width, 1.0f)) {
		WARN_PRINT("unsupported: line width");
	}

	// Multisample.
	if (p_multisample_state.enable_sample_shading) {
		WARN_PRINT("unsupported: multi-sample shading");
	}

	if (p_multisample_state.sample_count > TEXTURE_SAMPLES_1) {
		pipeline->sample_count = (*device_properties).find_nearest_supported_sample_count(p_multisample_state.sample_count);
	}
	desc.rasterSampleCount = static_cast<NSUInteger>(pipeline->sample_count);
	desc.alphaToCoverageEnabled = p_multisample_state.enable_alpha_to_coverage;
	desc.alphaToOneEnabled = p_multisample_state.enable_alpha_to_one;

	// Depth stencil.
	if (p_depth_stencil_state.enable_depth_test && desc.depthAttachmentPixelFormat != MTLPixelFormatInvalid) {
		pipeline->raster_state.depth_test.enabled = true;
		MTLDepthStencilDescriptor *ds_desc = [MTLDepthStencilDescriptor new];
		ds_desc.depthWriteEnabled = p_depth_stencil_state.enable_depth_write;
		ds_desc.depthCompareFunction = COMPARE_OPERATORS[p_depth_stencil_state.depth_compare_operator];
		if (p_depth_stencil_state.enable_depth_range) {
			WARN_PRINT("unsupported: depth range");
		}

		if (p_depth_stencil_state.enable_stencil) {
			pipeline->raster_state.stencil.front_reference = p_depth_stencil_state.front_op.reference;
			pipeline->raster_state.stencil.back_reference = p_depth_stencil_state.back_op.reference;

			{
				// Front.
				MTLStencilDescriptor *sd = [MTLStencilDescriptor new];
				sd.stencilFailureOperation = STENCIL_OPERATIONS[p_depth_stencil_state.front_op.fail];
				sd.depthStencilPassOperation = STENCIL_OPERATIONS[p_depth_stencil_state.front_op.pass];
				sd.depthFailureOperation = STENCIL_OPERATIONS[p_depth_stencil_state.front_op.depth_fail];
				sd.stencilCompareFunction = COMPARE_OPERATORS[p_depth_stencil_state.front_op.compare];
				sd.readMask = p_depth_stencil_state.front_op.compare_mask;
				sd.writeMask = p_depth_stencil_state.front_op.write_mask;
				ds_desc.frontFaceStencil = sd;
			}
			{
				// Back.
				MTLStencilDescriptor *sd = [MTLStencilDescriptor new];
				sd.stencilFailureOperation = STENCIL_OPERATIONS[p_depth_stencil_state.back_op.fail];
				sd.depthStencilPassOperation = STENCIL_OPERATIONS[p_depth_stencil_state.back_op.pass];
				sd.depthFailureOperation = STENCIL_OPERATIONS[p_depth_stencil_state.back_op.depth_fail];
				sd.stencilCompareFunction = COMPARE_OPERATORS[p_depth_stencil_state.back_op.compare];
				sd.readMask = p_depth_stencil_state.back_op.compare_mask;
				sd.writeMask = p_depth_stencil_state.back_op.write_mask;
				ds_desc.backFaceStencil = sd;
			}
		}

		pipeline->depth_stencil = [device newDepthStencilStateWithDescriptor:ds_desc];
		ERR_FAIL_NULL_V_MSG(pipeline->depth_stencil, PipelineID(), "Failed to create depth stencil state");
	} else {
		// TODO(sgc): FB13671991 raised as Apple docs state calling setDepthStencilState:nil is valid, but currently generates an exception
		pipeline->depth_stencil = get_resource_cache().get_depth_stencil_state(false, false);
	}

	// Blend state.
	{
		for (uint32_t i = 0; i < p_color_attachments.size(); i++) {
			if (p_color_attachments[i] == ATTACHMENT_UNUSED) {
				continue;
			}

			const PipelineColorBlendState::Attachment &bs = p_blend_state.attachments[i];

			MTLRenderPipelineColorAttachmentDescriptor *ca_desc = desc.colorAttachments[p_color_attachments[i]];
			ca_desc.blendingEnabled = bs.enable_blend;

			ca_desc.sourceRGBBlendFactor = BLEND_FACTORS[bs.src_color_blend_factor];
			ca_desc.destinationRGBBlendFactor = BLEND_FACTORS[bs.dst_color_blend_factor];
			ca_desc.rgbBlendOperation = BLEND_OPERATIONS[bs.color_blend_op];

			ca_desc.sourceAlphaBlendFactor = BLEND_FACTORS[bs.src_alpha_blend_factor];
			ca_desc.destinationAlphaBlendFactor = BLEND_FACTORS[bs.dst_alpha_blend_factor];
			ca_desc.alphaBlendOperation = BLEND_OPERATIONS[bs.alpha_blend_op];

			ca_desc.writeMask = MTLColorWriteMaskNone;
			if (bs.write_r) {
				ca_desc.writeMask |= MTLColorWriteMaskRed;
			}
			if (bs.write_g) {
				ca_desc.writeMask |= MTLColorWriteMaskGreen;
			}
			if (bs.write_b) {
				ca_desc.writeMask |= MTLColorWriteMaskBlue;
			}
			if (bs.write_a) {
				ca_desc.writeMask |= MTLColorWriteMaskAlpha;
			}
		}

		pipeline->raster_state.blend.r = p_blend_state.blend_constant.r;
		pipeline->raster_state.blend.g = p_blend_state.blend_constant.g;
		pipeline->raster_state.blend.b = p_blend_state.blend_constant.b;
		pipeline->raster_state.blend.a = p_blend_state.blend_constant.a;
	}

	// Dynamic state.

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_DEPTH_BIAS)) {
		pipeline->raster_state.depth_bias.enabled = true;
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_BLEND_CONSTANTS)) {
		pipeline->raster_state.blend.enabled = true;
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_DEPTH_BOUNDS)) {
		// TODO(sgc): ??
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_COMPARE_MASK)) {
		// TODO(sgc): ??
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_WRITE_MASK)) {
		// TODO(sgc): ??
	}

	if (p_dynamic_state.has_flag(DYNAMIC_STATE_STENCIL_REFERENCE)) {
		pipeline->raster_state.stencil.enabled = true;
	}

	if (shader->vert != nil) {
		Result<id<MTLFunction>> function_or_err = _create_function(shader->vert, @"main0", p_specialization_constants);
		ERR_FAIL_COND_V(std::holds_alternative<Error>(function_or_err), PipelineID());
		desc.vertexFunction = std::get<id<MTLFunction>>(function_or_err);
	}

	if (shader->frag != nil) {
		Result<id<MTLFunction>> function_or_err = _create_function(shader->frag, @"main0", p_specialization_constants);
		ERR_FAIL_COND_V(std::holds_alternative<Error>(function_or_err), PipelineID());
		desc.fragmentFunction = std::get<id<MTLFunction>>(function_or_err);
	}

	if (archive) {
		desc.binaryArchives = @[ archive ];
	}

	NSError *error = nil;
	pipeline->state = [device newRenderPipelineStateWithDescriptor:desc
															 error:&error];
	pipeline->shader = shader;

	ERR_FAIL_COND_V_MSG(error != nil, PipelineID(), ([NSString stringWithFormat:@"error creating pipeline: %@", error.localizedDescription].UTF8String));

	if (archive) {
		if ([archive addRenderPipelineFunctionsWithDescriptor:desc error:&error]) {
			archive_count += 1;
		} else {
			print_error(error.localizedDescription.UTF8String);
		}
	}

	return PipelineID(pipeline);
}

#pragma mark - Compute

// ----- COMMANDS -----

void RenderingDeviceDriverMetal::command_bind_compute_pipeline(CommandBufferID p_cmd_buffer, PipelineID p_pipeline) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->bind_pipeline(p_pipeline);
}

void RenderingDeviceDriverMetal::command_bind_compute_uniform_set(CommandBufferID p_cmd_buffer, UniformSetID p_uniform_set, ShaderID p_shader, uint32_t p_set_index) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->compute_bind_uniform_set(p_uniform_set, p_shader, p_set_index);
}

void RenderingDeviceDriverMetal::command_bind_compute_uniform_sets(CommandBufferID p_cmd_buffer, VectorView<UniformSetID> p_uniform_sets, ShaderID p_shader, uint32_t p_first_set_index, uint32_t p_set_count) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->compute_bind_uniform_sets(p_uniform_sets, p_shader, p_first_set_index, p_set_count);
}

void RenderingDeviceDriverMetal::command_compute_dispatch(CommandBufferID p_cmd_buffer, uint32_t p_x_groups, uint32_t p_y_groups, uint32_t p_z_groups) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->compute_dispatch(p_x_groups, p_y_groups, p_z_groups);
}

void RenderingDeviceDriverMetal::command_compute_dispatch_indirect(CommandBufferID p_cmd_buffer, BufferID p_indirect_buffer, uint64_t p_offset) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	cb->compute_dispatch_indirect(p_indirect_buffer, p_offset);
}

// ----- PIPELINE -----

RDD::PipelineID RenderingDeviceDriverMetal::compute_pipeline_create(ShaderID p_shader, VectorView<PipelineSpecializationConstant> p_specialization_constants) {
	MDComputeShader *shader = (MDComputeShader *)(p_shader.id);

	os_signpost_id_t reflect_id = os_signpost_id_make_with_pointer(LOG_INTERVALS, shader);
	os_signpost_interval_begin(LOG_INTERVALS, reflect_id, "compute_pipeline_create", "shader_name=%{public}s", shader->name.get_data());
	DEFER([=]() {
		os_signpost_interval_end(LOG_INTERVALS, reflect_id, "compute_pipeline_create");
	});

	os_signpost_event_emit(LOG_DRIVER, OS_SIGNPOST_ID_EXCLUSIVE, "create_pipeline");

	Result<id<MTLFunction>> function_or_err = _create_function(shader->kernel, @"main0", p_specialization_constants);
	ERR_FAIL_COND_V(std::holds_alternative<Error>(function_or_err), PipelineID());
	id<MTLFunction> function = std::get<id<MTLFunction>>(function_or_err);

	MTLComputePipelineDescriptor *desc = [MTLComputePipelineDescriptor new];
	desc.computeFunction = function;
	if (archive) {
		desc.binaryArchives = @[ archive ];
	}

	NSError *error;
	id<MTLComputePipelineState> state = [device newComputePipelineStateWithDescriptor:desc
																			  options:MTLPipelineOptionNone
																		   reflection:nil
																				error:&error];
	ERR_FAIL_COND_V_MSG(error != nil, PipelineID(), ([NSString stringWithFormat:@"error creating pipeline: %@", error.localizedDescription].UTF8String));

	MDComputePipeline *pipeline = new MDComputePipeline(state);
	pipeline->compute_state.local = shader->local;
	pipeline->shader = shader;

	if (archive) {
		if ([archive addComputePipelineFunctionsWithDescriptor:desc error:&error]) {
			archive_count += 1;
		} else {
			print_error(error.localizedDescription.UTF8String);
		}
	}

	return PipelineID(pipeline);
}

#pragma mark - Queries

// ----- TIMESTAMP -----

RDD::QueryPoolID RenderingDeviceDriverMetal::timestamp_query_pool_create(uint32_t p_query_count) {
	return QueryPoolID(1);
}

void RenderingDeviceDriverMetal::timestamp_query_pool_free(QueryPoolID p_pool_id) {
}

void RenderingDeviceDriverMetal::timestamp_query_pool_get_results(QueryPoolID p_pool_id, uint32_t p_query_count, uint64_t *r_results) {
	// Metal doesn't support timestamp queries, so we just clear the buffer.
	bzero(r_results, p_query_count * sizeof(uint64_t));
}

uint64_t RenderingDeviceDriverMetal::timestamp_query_result_to_time(uint64_t p_result) {
	return p_result;
}

void RenderingDeviceDriverMetal::command_timestamp_query_pool_reset(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_query_count) {
}

void RenderingDeviceDriverMetal::command_timestamp_write(CommandBufferID p_cmd_buffer, QueryPoolID p_pool_id, uint32_t p_index) {
}

#pragma mark - Labels

void RenderingDeviceDriverMetal::command_begin_label(CommandBufferID p_cmd_buffer, const char *p_label_name, const Color &p_color) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	NSString *s = [[NSString alloc] initWithBytesNoCopy:(void *)p_label_name length:strlen(p_label_name) encoding:NSUTF8StringEncoding freeWhenDone:NO];
	[cb->get_command_buffer() pushDebugGroup:s];
}

void RenderingDeviceDriverMetal::command_end_label(CommandBufferID p_cmd_buffer) {
	MDCommandBuffer *cb = (MDCommandBuffer *)(p_cmd_buffer.id);
	[cb->get_command_buffer() popDebugGroup];
}

#pragma mark - Debug

void RenderingDeviceDriverMetal::command_insert_breadcrumb(CommandBufferID p_cmd_buffer, uint32_t p_data) {
	// TODO: Implement.
}

#pragma mark - Submission

void RenderingDeviceDriverMetal::begin_segment(uint32_t p_frame_index, uint32_t p_frames_drawn) {
}

void RenderingDeviceDriverMetal::end_segment() {
}

#pragma mark - Misc

void RenderingDeviceDriverMetal::set_object_name(ObjectType p_type, ID p_driver_id, const String &p_name) {
	switch (p_type) {
		case OBJECT_TYPE_TEXTURE: {
			id<MTLTexture> tex = rid::get(p_driver_id);
			tex.label = [NSString stringWithUTF8String:p_name.utf8().get_data()];
		} break;
		case OBJECT_TYPE_SAMPLER: {
			// Can't set label after creation.
		} break;
		case OBJECT_TYPE_BUFFER: {
			id<MTLBuffer> buffer = rid::get(p_driver_id);
			buffer.label = [NSString stringWithUTF8String:p_name.utf8().get_data()];
		} break;
		case OBJECT_TYPE_SHADER: {
			NSString *label = [NSString stringWithUTF8String:p_name.utf8().get_data()];
			MDShader *shader = (MDShader *)(p_driver_id.id);
			if (MDRenderShader *rs = dynamic_cast<MDRenderShader *>(shader); rs != nullptr) {
				[rs->vert setLabel:label];
				[rs->frag setLabel:label];
			} else if (MDComputeShader *cs = dynamic_cast<MDComputeShader *>(shader); cs != nullptr) {
				[cs->kernel setLabel:label];
			} else {
				DEV_ASSERT(false);
			}
		} break;
		case OBJECT_TYPE_UNIFORM_SET: {
			MDUniformSet *set = (MDUniformSet *)(p_driver_id.id);
			for (KeyValue<MDShader *, BoundUniformSet> &keyval : set->bound_uniforms) {
				keyval.value.buffer.label = [NSString stringWithUTF8String:p_name.utf8().get_data()];
			}
		} break;
		case OBJECT_TYPE_PIPELINE: {
			// Can't set label after creation.
		} break;
		default: {
			DEV_ASSERT(false);
		}
	}
}

uint64_t RenderingDeviceDriverMetal::get_resource_native_handle(DriverResource p_type, ID p_driver_id) {
	switch (p_type) {
		case DRIVER_RESOURCE_LOGICAL_DEVICE: {
			return (uint64_t)(uintptr_t)(__bridge void *)device;
		}
		case DRIVER_RESOURCE_PHYSICAL_DEVICE: {
			return 0;
		}
		case DRIVER_RESOURCE_TOPMOST_OBJECT: {
			return 0;
		}
		case DRIVER_RESOURCE_COMMAND_QUEUE: {
			return (uint64_t)(uintptr_t)(__bridge void *)device_queue;
		}
		case DRIVER_RESOURCE_QUEUE_FAMILY: {
			return 0;
		}
		case DRIVER_RESOURCE_TEXTURE: {
			return p_driver_id.id;
		}
		case DRIVER_RESOURCE_TEXTURE_VIEW: {
			return p_driver_id.id;
		}
		case DRIVER_RESOURCE_TEXTURE_DATA_FORMAT: {
			return 0;
		}
		case DRIVER_RESOURCE_SAMPLER: {
			return p_driver_id.id;
		}
		case DRIVER_RESOURCE_UNIFORM_SET: {
			return 0;
		}
		case DRIVER_RESOURCE_BUFFER: {
			return p_driver_id.id;
		}
		case DRIVER_RESOURCE_COMPUTE_PIPELINE: {
			MDComputePipeline *pipeline = (MDComputePipeline *)(p_driver_id.id);
			return (uint64_t)(uintptr_t)(__bridge void *)pipeline->state;
		}
		case DRIVER_RESOURCE_RENDER_PIPELINE: {
			MDRenderPipeline *pipeline = (MDRenderPipeline *)(p_driver_id.id);
			return (uint64_t)(uintptr_t)(__bridge void *)pipeline->state;
		}
		default: {
			return 0;
		}
	}
}

uint64_t RenderingDeviceDriverMetal::get_total_memory_used() {
	return device.currentAllocatedSize;
}

uint64_t RenderingDeviceDriverMetal::get_lazily_memory_used() {
	return 0; // TODO: Track this (grep for memoryless in Godot's Metal backend).
}

uint64_t RenderingDeviceDriverMetal::limit_get(Limit p_limit) {
	MetalDeviceProperties const &props = (*device_properties);
	MetalLimits const &limits = props.limits;

#if defined(DEV_ENABLED)
#define UNKNOWN(NAME)                                                            \
	case NAME:                                                                   \
		WARN_PRINT_ONCE("Returning maximum value for unknown limit " #NAME "."); \
		return (uint64_t)1 << 30;
#else
#define UNKNOWN(NAME) \
	case NAME:        \
		return (uint64_t)1 << 30
#endif

	// clang-format off
	switch (p_limit) {
		case LIMIT_MAX_BOUND_UNIFORM_SETS:
			return limits.maxBoundDescriptorSets;
		case LIMIT_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
			return limits.maxColorAttachments;
		case LIMIT_MAX_TEXTURES_PER_UNIFORM_SET:
			return limits.maxTexturesPerArgumentBuffer;
		case LIMIT_MAX_SAMPLERS_PER_UNIFORM_SET:
			return limits.maxSamplersPerArgumentBuffer;
		case LIMIT_MAX_STORAGE_BUFFERS_PER_UNIFORM_SET:
			return limits.maxBuffersPerArgumentBuffer;
		case LIMIT_MAX_STORAGE_IMAGES_PER_UNIFORM_SET:
			return limits.maxTexturesPerArgumentBuffer;
		case LIMIT_MAX_UNIFORM_BUFFERS_PER_UNIFORM_SET:
			return limits.maxBuffersPerArgumentBuffer;
		case LIMIT_MAX_DRAW_INDEXED_INDEX:
			return limits.maxDrawIndexedIndexValue;
		case LIMIT_MAX_FRAMEBUFFER_HEIGHT:
			return limits.maxFramebufferHeight;
		case LIMIT_MAX_FRAMEBUFFER_WIDTH:
			return limits.maxFramebufferWidth;
		case LIMIT_MAX_TEXTURE_ARRAY_LAYERS:
			return limits.maxImageArrayLayers;
		case LIMIT_MAX_TEXTURE_SIZE_1D:
			return limits.maxImageDimension1D;
		case LIMIT_MAX_TEXTURE_SIZE_2D:
			return limits.maxImageDimension2D;
		case LIMIT_MAX_TEXTURE_SIZE_3D:
			return limits.maxImageDimension3D;
		case LIMIT_MAX_TEXTURE_SIZE_CUBE:
			return limits.maxImageDimensionCube;
		case LIMIT_MAX_TEXTURES_PER_SHADER_STAGE:
			return limits.maxTexturesPerArgumentBuffer;
		case LIMIT_MAX_SAMPLERS_PER_SHADER_STAGE:
			return limits.maxSamplersPerArgumentBuffer;
		case LIMIT_MAX_STORAGE_BUFFERS_PER_SHADER_STAGE:
			return limits.maxBuffersPerArgumentBuffer;
		case LIMIT_MAX_STORAGE_IMAGES_PER_SHADER_STAGE:
			return limits.maxTexturesPerArgumentBuffer;
		case LIMIT_MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE:
			return limits.maxBuffersPerArgumentBuffer;
		case LIMIT_MAX_PUSH_CONSTANT_SIZE:
			return limits.maxBufferLength;
		case LIMIT_MAX_UNIFORM_BUFFER_SIZE:
			return limits.maxBufferLength;
		case LIMIT_MAX_VERTEX_INPUT_ATTRIBUTE_OFFSET:
			return limits.maxVertexDescriptorLayoutStride;
		case LIMIT_MAX_VERTEX_INPUT_ATTRIBUTES:
			return limits.maxVertexInputAttributes;
		case LIMIT_MAX_VERTEX_INPUT_BINDINGS:
			return limits.maxVertexInputBindings;
		case LIMIT_MAX_VERTEX_INPUT_BINDING_STRIDE:
			return limits.maxVertexInputBindingStride;
		case LIMIT_MIN_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
			return limits.minUniformBufferOffsetAlignment;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X:
			return limits.maxComputeWorkGroupCount.width;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y:
			return limits.maxComputeWorkGroupCount.height;
		case LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z:
			return limits.maxComputeWorkGroupCount.depth;
		case LIMIT_MAX_COMPUTE_WORKGROUP_INVOCATIONS:
			return std::max({ limits.maxThreadsPerThreadGroup.width, limits.maxThreadsPerThreadGroup.height, limits.maxThreadsPerThreadGroup.depth });
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_X:
			return limits.maxThreadsPerThreadGroup.width;
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Y:
			return limits.maxThreadsPerThreadGroup.height;
		case LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Z:
			return limits.maxThreadsPerThreadGroup.depth;
		case LIMIT_MAX_VIEWPORT_DIMENSIONS_X:
			return limits.maxViewportDimensionX;
		case LIMIT_MAX_VIEWPORT_DIMENSIONS_Y:
			return limits.maxViewportDimensionY;
		case LIMIT_SUBGROUP_SIZE:
			// MoltenVK sets the subgroupSize to the same as the maxSubgroupSize.
			return limits.maxSubgroupSize;
		case LIMIT_SUBGROUP_MIN_SIZE:
			return limits.minSubgroupSize;
		case LIMIT_SUBGROUP_MAX_SIZE:
			return limits.maxSubgroupSize;
		case LIMIT_SUBGROUP_IN_SHADERS:
			return (uint64_t)limits.subgroupSupportedShaderStages;
		case LIMIT_SUBGROUP_OPERATIONS:
			return (uint64_t)limits.subgroupSupportedOperations;
		UNKNOWN(LIMIT_VRS_TEXEL_WIDTH);
		UNKNOWN(LIMIT_VRS_TEXEL_HEIGHT);
		UNKNOWN(LIMIT_VRS_MAX_FRAGMENT_WIDTH);
		UNKNOWN(LIMIT_VRS_MAX_FRAGMENT_HEIGHT);
		default:
			ERR_FAIL_V(0);
	}
	// clang-format on
	return 0;
}

uint64_t RenderingDeviceDriverMetal::api_trait_get(ApiTrait p_trait) {
	switch (p_trait) {
		case API_TRAIT_HONORS_PIPELINE_BARRIERS:
			return 0;
		default:
			return RenderingDeviceDriver::api_trait_get(p_trait);
	}
}

bool RenderingDeviceDriverMetal::has_feature(Features p_feature) {
	switch (p_feature) {
		case SUPPORTS_MULTIVIEW:
			return multiview_capabilities.is_supported;
		case SUPPORTS_FSR_HALF_FLOAT:
			return true;
		case SUPPORTS_ATTACHMENT_VRS:
			// TODO(sgc): Maybe supported via https://developer.apple.com/documentation/metal/render_passes/rendering_at_different_rasterization_rates?language=objc
			// See also:
			//
			// * https://forum.beyond3d.com/threads/variable-rate-shading-vs-variable-rate-rasterization.62243/post-2191363
			//
			return false;
		case SUPPORTS_FRAGMENT_SHADER_WITH_ONLY_SIDE_EFFECTS:
			return true;
		default:
			return false;
	}
}

const RDD::MultiviewCapabilities &RenderingDeviceDriverMetal::get_multiview_capabilities() {
	return multiview_capabilities;
}

String RenderingDeviceDriverMetal::get_api_version() const {
	return vformat("%d.%d", version_major, version_minor);
}

String RenderingDeviceDriverMetal::get_pipeline_cache_uuid() const {
	return pipeline_cache_id;
}

const RDD::Capabilities &RenderingDeviceDriverMetal::get_capabilities() const {
	return capabilities;
}

bool RenderingDeviceDriverMetal::is_composite_alpha_supported(CommandQueueID p_queue) const {
	// The CAMetalLayer.opaque property is configured according to this global setting.
	return OS::get_singleton()->is_layered_allowed();
}

size_t RenderingDeviceDriverMetal::get_texel_buffer_alignment_for_format(RDD::DataFormat p_format) const {
	return [device minimumLinearTextureAlignmentForPixelFormat:pixel_formats->getMTLPixelFormat(p_format)];
}

size_t RenderingDeviceDriverMetal::get_texel_buffer_alignment_for_format(MTLPixelFormat p_format) const {
	return [device minimumLinearTextureAlignmentForPixelFormat:p_format];
}

/******************/

RenderingDeviceDriverMetal::RenderingDeviceDriverMetal(RenderingContextDriverMetal *p_context_driver) :
		context_driver(p_context_driver) {
	DEV_ASSERT(p_context_driver != nullptr);

	if (String res = OS::get_singleton()->get_environment("GODOT_MTL_SHADER_LOAD_STRATEGY"); res == U"lazy") {
		_shader_load_strategy = ShaderLoadStrategy::LAZY;
	}
}

RenderingDeviceDriverMetal::~RenderingDeviceDriverMetal() {
	for (MDCommandBuffer *cb : command_buffers) {
		delete cb;
	}

	for (KeyValue<SHA256Digest, ShaderCacheEntry *> &kv : _shader_cache) {
		memdelete(kv.value);
	}
}

#pragma mark - Initialization

Error RenderingDeviceDriverMetal::_create_device() {
	device = context_driver->get_metal_device();

	device_queue = [device newCommandQueue];
	ERR_FAIL_NULL_V(device_queue, ERR_CANT_CREATE);

	device_scope = [MTLCaptureManager.sharedCaptureManager newCaptureScopeWithCommandQueue:device_queue];
	device_scope.label = @"Godot Frame";
	[device_scope beginScope]; // Allow Xcode to capture the first frame, if desired.

	resource_cache = std::make_unique<MDResourceCache>(this);

	return OK;
}

Error RenderingDeviceDriverMetal::_check_capabilities() {
	MTLCompileOptions *options = [MTLCompileOptions new];
	version_major = (options.languageVersion >> 0x10) & 0xff;
	version_minor = (options.languageVersion >> 0x00) & 0xff;

	capabilities.device_family = DEVICE_METAL;
	capabilities.version_major = version_major;
	capabilities.version_minor = version_minor;

	return OK;
}

Error RenderingDeviceDriverMetal::initialize(uint32_t p_device_index, uint32_t p_frame_count) {
	context_device = context_driver->device_get(p_device_index);
	Error err = _create_device();
	ERR_FAIL_COND_V(err, ERR_CANT_CREATE);

	err = _check_capabilities();
	ERR_FAIL_COND_V(err, ERR_CANT_CREATE);

	// Set the pipeline cache ID based on the Metal version.
	pipeline_cache_id = "metal-driver-" + get_api_version();

	device_properties = memnew(MetalDeviceProperties(device));
	pixel_formats = memnew(PixelFormats(device));
	if (device_properties->features.layeredRendering) {
		multiview_capabilities.is_supported = true;
		multiview_capabilities.max_view_count = device_properties->limits.maxViewports;
		// NOTE: I'm not sure what the limit is as I don't see it referenced anywhere
		multiview_capabilities.max_instance_count = UINT32_MAX;

		print_verbose("- Metal multiview supported:");
		print_verbose("  max view count: " + itos(multiview_capabilities.max_view_count));
		print_verbose("  max instances: " + itos(multiview_capabilities.max_instance_count));
	} else {
		print_verbose("- Metal multiview not supported");
	}

	// The Metal renderer requires Apple4 family. This is 2017 era A11 chips and newer.
	if (device_properties->features.highestFamily < MTLGPUFamilyApple4) {
		String error_string = vformat("Your Apple GPU does not support the following features, which are required to use Metal-based renderers in Godot:\n\n");
		if (!device_properties->features.imageCubeArray) {
			error_string += "- No support for image cube arrays.\n";
		}

#if defined(IOS_ENABLED)
		// iOS platform ports currently don't exit themselves when this method returns `ERR_CANT_CREATE`.
		OS::get_singleton()->alert(error_string + "\nClick OK to exit (black screen will be visible).");
#else
		OS::get_singleton()->alert(error_string + "\nClick OK to exit.");
#endif

		return ERR_CANT_CREATE;
	}

	return OK;
}
