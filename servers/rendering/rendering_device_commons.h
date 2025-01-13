/**************************************************************************/
/*  rendering_device_commons.h                                            */
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

#ifndef RENDERING_DEVICE_COMMONS_H
#define RENDERING_DEVICE_COMMONS_H

#include "core/object/object.h"
#include "core/variant/type_info.h"

#define STEPIFY(m_number, m_alignment) ((((m_number) + ((m_alignment) - 1)) / (m_alignment)) * (m_alignment))

class RenderingDeviceCommons : public Object {
	////////////////////////////////////////////
	// PUBLIC STUFF
	// Exposed by RenderingDevice, and shared
	// with RenderingDeviceDriver.
	////////////////////////////////////////////
public:
	static const bool command_pool_reset_enabled = true;

	/*****************/
	/**** GENERIC ****/
	/*****************/

	static const int INVALID_ID = -1;

	enum DataFormat {
		DATA_FORMAT_R4G4_UNORM_PACK8,
		DATA_FORMAT_R4G4B4A4_UNORM_PACK16,
		DATA_FORMAT_B4G4R4A4_UNORM_PACK16,
		DATA_FORMAT_R5G6B5_UNORM_PACK16,
		DATA_FORMAT_B5G6R5_UNORM_PACK16,
		DATA_FORMAT_R5G5B5A1_UNORM_PACK16,
		DATA_FORMAT_B5G5R5A1_UNORM_PACK16,
		DATA_FORMAT_A1R5G5B5_UNORM_PACK16,
		DATA_FORMAT_R8_UNORM,
		DATA_FORMAT_R8_SNORM,
		DATA_FORMAT_R8_USCALED,
		DATA_FORMAT_R8_SSCALED,
		DATA_FORMAT_R8_UINT,
		DATA_FORMAT_R8_SINT,
		DATA_FORMAT_R8_SRGB,
		DATA_FORMAT_R8G8_UNORM,
		DATA_FORMAT_R8G8_SNORM,
		DATA_FORMAT_R8G8_USCALED,
		DATA_FORMAT_R8G8_SSCALED,
		DATA_FORMAT_R8G8_UINT,
		DATA_FORMAT_R8G8_SINT,
		DATA_FORMAT_R8G8_SRGB,
		DATA_FORMAT_R8G8B8_UNORM,
		DATA_FORMAT_R8G8B8_SNORM,
		DATA_FORMAT_R8G8B8_USCALED,
		DATA_FORMAT_R8G8B8_SSCALED,
		DATA_FORMAT_R8G8B8_UINT,
		DATA_FORMAT_R8G8B8_SINT,
		DATA_FORMAT_R8G8B8_SRGB,
		DATA_FORMAT_B8G8R8_UNORM,
		DATA_FORMAT_B8G8R8_SNORM,
		DATA_FORMAT_B8G8R8_USCALED,
		DATA_FORMAT_B8G8R8_SSCALED,
		DATA_FORMAT_B8G8R8_UINT,
		DATA_FORMAT_B8G8R8_SINT,
		DATA_FORMAT_B8G8R8_SRGB,
		DATA_FORMAT_R8G8B8A8_UNORM,
		DATA_FORMAT_R8G8B8A8_SNORM,
		DATA_FORMAT_R8G8B8A8_USCALED,
		DATA_FORMAT_R8G8B8A8_SSCALED,
		DATA_FORMAT_R8G8B8A8_UINT,
		DATA_FORMAT_R8G8B8A8_SINT,
		DATA_FORMAT_R8G8B8A8_SRGB,
		DATA_FORMAT_B8G8R8A8_UNORM,
		DATA_FORMAT_B8G8R8A8_SNORM,
		DATA_FORMAT_B8G8R8A8_USCALED,
		DATA_FORMAT_B8G8R8A8_SSCALED,
		DATA_FORMAT_B8G8R8A8_UINT,
		DATA_FORMAT_B8G8R8A8_SINT,
		DATA_FORMAT_B8G8R8A8_SRGB,
		DATA_FORMAT_A8B8G8R8_UNORM_PACK32,
		DATA_FORMAT_A8B8G8R8_SNORM_PACK32,
		DATA_FORMAT_A8B8G8R8_USCALED_PACK32,
		DATA_FORMAT_A8B8G8R8_SSCALED_PACK32,
		DATA_FORMAT_A8B8G8R8_UINT_PACK32,
		DATA_FORMAT_A8B8G8R8_SINT_PACK32,
		DATA_FORMAT_A8B8G8R8_SRGB_PACK32,
		DATA_FORMAT_A2R10G10B10_UNORM_PACK32,
		DATA_FORMAT_A2R10G10B10_SNORM_PACK32,
		DATA_FORMAT_A2R10G10B10_USCALED_PACK32,
		DATA_FORMAT_A2R10G10B10_SSCALED_PACK32,
		DATA_FORMAT_A2R10G10B10_UINT_PACK32,
		DATA_FORMAT_A2R10G10B10_SINT_PACK32,
		DATA_FORMAT_A2B10G10R10_UNORM_PACK32,
		DATA_FORMAT_A2B10G10R10_SNORM_PACK32,
		DATA_FORMAT_A2B10G10R10_USCALED_PACK32,
		DATA_FORMAT_A2B10G10R10_SSCALED_PACK32,
		DATA_FORMAT_A2B10G10R10_UINT_PACK32,
		DATA_FORMAT_A2B10G10R10_SINT_PACK32,
		DATA_FORMAT_R16_UNORM,
		DATA_FORMAT_R16_SNORM,
		DATA_FORMAT_R16_USCALED,
		DATA_FORMAT_R16_SSCALED,
		DATA_FORMAT_R16_UINT,
		DATA_FORMAT_R16_SINT,
		DATA_FORMAT_R16_SFLOAT,
		DATA_FORMAT_R16G16_UNORM,
		DATA_FORMAT_R16G16_SNORM,
		DATA_FORMAT_R16G16_USCALED,
		DATA_FORMAT_R16G16_SSCALED,
		DATA_FORMAT_R16G16_UINT,
		DATA_FORMAT_R16G16_SINT,
		DATA_FORMAT_R16G16_SFLOAT,
		DATA_FORMAT_R16G16B16_UNORM,
		DATA_FORMAT_R16G16B16_SNORM,
		DATA_FORMAT_R16G16B16_USCALED,
		DATA_FORMAT_R16G16B16_SSCALED,
		DATA_FORMAT_R16G16B16_UINT,
		DATA_FORMAT_R16G16B16_SINT,
		DATA_FORMAT_R16G16B16_SFLOAT,
		DATA_FORMAT_R16G16B16A16_UNORM,
		DATA_FORMAT_R16G16B16A16_SNORM,
		DATA_FORMAT_R16G16B16A16_USCALED,
		DATA_FORMAT_R16G16B16A16_SSCALED,
		DATA_FORMAT_R16G16B16A16_UINT,
		DATA_FORMAT_R16G16B16A16_SINT,
		DATA_FORMAT_R16G16B16A16_SFLOAT,
		DATA_FORMAT_R32_UINT,
		DATA_FORMAT_R32_SINT,
		DATA_FORMAT_R32_SFLOAT,
		DATA_FORMAT_R32G32_UINT,
		DATA_FORMAT_R32G32_SINT,
		DATA_FORMAT_R32G32_SFLOAT,
		DATA_FORMAT_R32G32B32_UINT,
		DATA_FORMAT_R32G32B32_SINT,
		DATA_FORMAT_R32G32B32_SFLOAT,
		DATA_FORMAT_R32G32B32A32_UINT,
		DATA_FORMAT_R32G32B32A32_SINT,
		DATA_FORMAT_R32G32B32A32_SFLOAT,
		DATA_FORMAT_R64_UINT,
		DATA_FORMAT_R64_SINT,
		DATA_FORMAT_R64_SFLOAT,
		DATA_FORMAT_R64G64_UINT,
		DATA_FORMAT_R64G64_SINT,
		DATA_FORMAT_R64G64_SFLOAT,
		DATA_FORMAT_R64G64B64_UINT,
		DATA_FORMAT_R64G64B64_SINT,
		DATA_FORMAT_R64G64B64_SFLOAT,
		DATA_FORMAT_R64G64B64A64_UINT,
		DATA_FORMAT_R64G64B64A64_SINT,
		DATA_FORMAT_R64G64B64A64_SFLOAT,
		DATA_FORMAT_B10G11R11_UFLOAT_PACK32,
		DATA_FORMAT_E5B9G9R9_UFLOAT_PACK32,
		DATA_FORMAT_D16_UNORM,
		DATA_FORMAT_X8_D24_UNORM_PACK32,
		DATA_FORMAT_D32_SFLOAT,
		DATA_FORMAT_S8_UINT,
		DATA_FORMAT_D16_UNORM_S8_UINT,
		DATA_FORMAT_D24_UNORM_S8_UINT,
		DATA_FORMAT_D32_SFLOAT_S8_UINT,
		DATA_FORMAT_BC1_RGB_UNORM_BLOCK,
		DATA_FORMAT_BC1_RGB_SRGB_BLOCK,
		DATA_FORMAT_BC1_RGBA_UNORM_BLOCK,
		DATA_FORMAT_BC1_RGBA_SRGB_BLOCK,
		DATA_FORMAT_BC2_UNORM_BLOCK,
		DATA_FORMAT_BC2_SRGB_BLOCK,
		DATA_FORMAT_BC3_UNORM_BLOCK,
		DATA_FORMAT_BC3_SRGB_BLOCK,
		DATA_FORMAT_BC4_UNORM_BLOCK,
		DATA_FORMAT_BC4_SNORM_BLOCK,
		DATA_FORMAT_BC5_UNORM_BLOCK,
		DATA_FORMAT_BC5_SNORM_BLOCK,
		DATA_FORMAT_BC6H_UFLOAT_BLOCK,
		DATA_FORMAT_BC6H_SFLOAT_BLOCK,
		DATA_FORMAT_BC7_UNORM_BLOCK,
		DATA_FORMAT_BC7_SRGB_BLOCK,
		DATA_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
		DATA_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
		DATA_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
		DATA_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
		DATA_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
		DATA_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
		DATA_FORMAT_EAC_R11_UNORM_BLOCK,
		DATA_FORMAT_EAC_R11_SNORM_BLOCK,
		DATA_FORMAT_EAC_R11G11_UNORM_BLOCK,
		DATA_FORMAT_EAC_R11G11_SNORM_BLOCK,
		DATA_FORMAT_ASTC_4x4_UNORM_BLOCK,
		DATA_FORMAT_ASTC_4x4_SRGB_BLOCK,
		DATA_FORMAT_ASTC_5x4_UNORM_BLOCK,
		DATA_FORMAT_ASTC_5x4_SRGB_BLOCK,
		DATA_FORMAT_ASTC_5x5_UNORM_BLOCK,
		DATA_FORMAT_ASTC_5x5_SRGB_BLOCK,
		DATA_FORMAT_ASTC_6x5_UNORM_BLOCK,
		DATA_FORMAT_ASTC_6x5_SRGB_BLOCK,
		DATA_FORMAT_ASTC_6x6_UNORM_BLOCK,
		DATA_FORMAT_ASTC_6x6_SRGB_BLOCK,
		DATA_FORMAT_ASTC_8x5_UNORM_BLOCK,
		DATA_FORMAT_ASTC_8x5_SRGB_BLOCK,
		DATA_FORMAT_ASTC_8x6_UNORM_BLOCK,
		DATA_FORMAT_ASTC_8x6_SRGB_BLOCK,
		DATA_FORMAT_ASTC_8x8_UNORM_BLOCK,
		DATA_FORMAT_ASTC_8x8_SRGB_BLOCK,
		DATA_FORMAT_ASTC_10x5_UNORM_BLOCK,
		DATA_FORMAT_ASTC_10x5_SRGB_BLOCK,
		DATA_FORMAT_ASTC_10x6_UNORM_BLOCK,
		DATA_FORMAT_ASTC_10x6_SRGB_BLOCK,
		DATA_FORMAT_ASTC_10x8_UNORM_BLOCK,
		DATA_FORMAT_ASTC_10x8_SRGB_BLOCK,
		DATA_FORMAT_ASTC_10x10_UNORM_BLOCK,
		DATA_FORMAT_ASTC_10x10_SRGB_BLOCK,
		DATA_FORMAT_ASTC_12x10_UNORM_BLOCK,
		DATA_FORMAT_ASTC_12x10_SRGB_BLOCK,
		DATA_FORMAT_ASTC_12x12_UNORM_BLOCK,
		DATA_FORMAT_ASTC_12x12_SRGB_BLOCK,
		DATA_FORMAT_G8B8G8R8_422_UNORM,
		DATA_FORMAT_B8G8R8G8_422_UNORM,
		DATA_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		DATA_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		DATA_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		DATA_FORMAT_G8_B8R8_2PLANE_422_UNORM,
		DATA_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		DATA_FORMAT_R10X6_UNORM_PACK16,
		DATA_FORMAT_R10X6G10X6_UNORM_2PACK16,
		DATA_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
		DATA_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
		DATA_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
		DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
		DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
		DATA_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		DATA_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		DATA_FORMAT_R12X4_UNORM_PACK16,
		DATA_FORMAT_R12X4G12X4_UNORM_2PACK16,
		DATA_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
		DATA_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
		DATA_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
		DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
		DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
		DATA_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
		DATA_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
		DATA_FORMAT_G16B16G16R16_422_UNORM,
		DATA_FORMAT_B16G16R16G16_422_UNORM,
		DATA_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
		DATA_FORMAT_G16_B16R16_2PLANE_420_UNORM,
		DATA_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
		DATA_FORMAT_G16_B16R16_2PLANE_422_UNORM,
		DATA_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
		DATA_FORMAT_MAX,
	};

	// Breadcrumb markers are useful for debugging GPU crashes (i.e. DEVICE_LOST). Internally
	// they're just an uint32_t to "tag" a GPU command. These are only used for debugging and do not
	// (or at least shouldn't) alter the execution behavior in any way.
	//
	// When a GPU crashes and Godot was built in dev or debug mode; Godot will dump what commands
	// were being executed and what tag they were marked with.
	// This makes narrowing down the cause of a crash easier. Note that a GPU can be executing
	// multiple commands at the same time. It is also useful to identify data hazards.
	//
	// For example if each LIGHTMAPPER_PASS must be executed in sequential order, but dumps
	// indicated that pass (LIGHTMAPPER_PASS | 5) was being executed at the same time as
	// (LIGHTMAPPER_PASS | 4), that would indicate there is a missing barrier or a render graph bug.
	//
	// The enums are bitshifted by 16 bits so it's possible to add user data via bitwise operations.
	// Using this enum is not mandatory; but it is recommended so that all subsystems agree what each
	// ID means when dumping info.
	enum BreadcrumbMarker {
		NONE = 0,
		// Environment
		REFLECTION_PROBES = 1u << 16u,
		SKY_PASS = 2u << 16u,
		// Light mapping
		LIGHTMAPPER_PASS = 3u << 16u,
		// Shadows
		SHADOW_PASS_DIRECTIONAL = 4u << 16u,
		SHADOW_PASS_CUBE = 5u << 16u,
		// Geometry passes
		OPAQUE_PASS = 6u << 16u,
		ALPHA_PASS = 7u << 16u,
		TRANSPARENT_PASS = 8u << 16u,
		// Screen effects
		POST_PROCESSING_PASS = 9u << 16u,
		BLIT_PASS = 10u << 16u,
		UI_PASS = 11u << 16u,
		// Other
		DEBUG_PASS = 12u << 16u,
	};

	enum CompareOperator {
		COMPARE_OP_NEVER,
		COMPARE_OP_LESS,
		COMPARE_OP_EQUAL,
		COMPARE_OP_LESS_OR_EQUAL,
		COMPARE_OP_GREATER,
		COMPARE_OP_NOT_EQUAL,
		COMPARE_OP_GREATER_OR_EQUAL,
		COMPARE_OP_ALWAYS,
		COMPARE_OP_MAX
	};

	/*****************/
	/**** TEXTURE ****/
	/*****************/

	enum TextureType {
		TEXTURE_TYPE_1D,
		TEXTURE_TYPE_2D,
		TEXTURE_TYPE_3D,
		TEXTURE_TYPE_CUBE,
		TEXTURE_TYPE_1D_ARRAY,
		TEXTURE_TYPE_2D_ARRAY,
		TEXTURE_TYPE_CUBE_ARRAY,
		TEXTURE_TYPE_MAX,
	};

	enum TextureSamples {
		TEXTURE_SAMPLES_1,
		TEXTURE_SAMPLES_2,
		TEXTURE_SAMPLES_4,
		TEXTURE_SAMPLES_8,
		TEXTURE_SAMPLES_16,
		TEXTURE_SAMPLES_32,
		TEXTURE_SAMPLES_64,
		TEXTURE_SAMPLES_MAX,
	};

	enum TextureUsageBits {
		TEXTURE_USAGE_SAMPLING_BIT = (1 << 0),
		TEXTURE_USAGE_COLOR_ATTACHMENT_BIT = (1 << 1),
		TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = (1 << 2),
		TEXTURE_USAGE_STORAGE_BIT = (1 << 3),
		TEXTURE_USAGE_STORAGE_ATOMIC_BIT = (1 << 4),
		TEXTURE_USAGE_CPU_READ_BIT = (1 << 5),
		TEXTURE_USAGE_CAN_UPDATE_BIT = (1 << 6),
		TEXTURE_USAGE_CAN_COPY_FROM_BIT = (1 << 7),
		TEXTURE_USAGE_CAN_COPY_TO_BIT = (1 << 8),
		TEXTURE_USAGE_INPUT_ATTACHMENT_BIT = (1 << 9),
		TEXTURE_USAGE_VRS_ATTACHMENT_BIT = (1 << 10),
		// When set, the texture is not backed by actual memory. It only ever lives in the cache.
		// This is particularly useful for:
		//	1. Depth/stencil buffers that are not needed after producing the colour output.
		//	2. MSAA surfaces that are immediately resolved (i.e. its raw content isn't needed).
		//
		// This flag heavily improves performance & saves memory on TBDR GPUs (e.g. mobile).
		// On Desktop this flag won't save memory but it still instructs the render graph that data will
		// be discarded aggressively which may still improve some performance.
		//
		// It is not valid to perform copies from/to this texture, since it doesn't occupy actual RAM.
		// It is also not valid to sample from this texture except using subpasses or via read/write
		// pixel shader extensions (e.g. VK_EXT_rasterization_order_attachment_access).
		//
		// Try to set this bit as much as possible. If you set it, validation doesn't complain
		// and it works fine on mobile, then go ahead.
		TEXTURE_USAGE_TRANSIENT_BIT = (1 << 11),
	};

	struct TextureFormat {
		DataFormat format = DATA_FORMAT_R8_UNORM;
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t depth = 1;
		uint32_t array_layers = 1;
		uint32_t mipmaps = 1;
		TextureType texture_type = TEXTURE_TYPE_2D;
		TextureSamples samples = TEXTURE_SAMPLES_1;
		uint32_t usage_bits = 0;
		Vector<DataFormat> shareable_formats;
		bool is_resolve_buffer = false;
		bool is_discardable = false;

		bool operator==(const TextureFormat &b) const {
			if (format != b.format) {
				return false;
			} else if (width != b.width) {
				return false;
			} else if (height != b.height) {
				return false;
			} else if (depth != b.depth) {
				return false;
			} else if (array_layers != b.array_layers) {
				return false;
			} else if (mipmaps != b.mipmaps) {
				return false;
			} else if (texture_type != b.texture_type) {
				return false;
			} else if (samples != b.samples) {
				return false;
			} else if (usage_bits != b.usage_bits) {
				return false;
			} else if (shareable_formats != b.shareable_formats) {
				return false;
			} else if (is_resolve_buffer != b.is_resolve_buffer) {
				return false;
			} else if (is_discardable != b.is_discardable) {
				return false;
			} else {
				return true;
			}
		}
	};

	enum TextureSwizzle {
		TEXTURE_SWIZZLE_IDENTITY,
		TEXTURE_SWIZZLE_ZERO,
		TEXTURE_SWIZZLE_ONE,
		TEXTURE_SWIZZLE_R,
		TEXTURE_SWIZZLE_G,
		TEXTURE_SWIZZLE_B,
		TEXTURE_SWIZZLE_A,
		TEXTURE_SWIZZLE_MAX
	};

	enum TextureSliceType {
		TEXTURE_SLICE_2D,
		TEXTURE_SLICE_CUBEMAP,
		TEXTURE_SLICE_3D,
		TEXTURE_SLICE_2D_ARRAY,
		TEXTURE_SLICE_MAX
	};

	/*****************/
	/**** SAMPLER ****/
	/*****************/

	enum SamplerFilter {
		SAMPLER_FILTER_NEAREST,
		SAMPLER_FILTER_LINEAR,
	};

	enum SamplerRepeatMode {
		SAMPLER_REPEAT_MODE_REPEAT,
		SAMPLER_REPEAT_MODE_MIRRORED_REPEAT,
		SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE,
		SAMPLER_REPEAT_MODE_CLAMP_TO_BORDER,
		SAMPLER_REPEAT_MODE_MIRROR_CLAMP_TO_EDGE,
		SAMPLER_REPEAT_MODE_MAX
	};

	enum SamplerBorderColor {
		SAMPLER_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		SAMPLER_BORDER_COLOR_INT_TRANSPARENT_BLACK,
		SAMPLER_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
		SAMPLER_BORDER_COLOR_INT_OPAQUE_BLACK,
		SAMPLER_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		SAMPLER_BORDER_COLOR_INT_OPAQUE_WHITE,
		SAMPLER_BORDER_COLOR_MAX
	};

	struct SamplerState {
		SamplerFilter mag_filter = SAMPLER_FILTER_NEAREST;
		SamplerFilter min_filter = SAMPLER_FILTER_NEAREST;
		SamplerFilter mip_filter = SAMPLER_FILTER_NEAREST;
		SamplerRepeatMode repeat_u = SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		SamplerRepeatMode repeat_v = SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		SamplerRepeatMode repeat_w = SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		float lod_bias = 0.0f;
		bool use_anisotropy = false;
		float anisotropy_max = 1.0f;
		bool enable_compare = false;
		CompareOperator compare_op = COMPARE_OP_ALWAYS;
		float min_lod = 0.0f;
		float max_lod = 1e20; // Something very large should do.
		SamplerBorderColor border_color = SAMPLER_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		bool unnormalized_uvw = false;
	};

	/**********************/
	/**** VERTEX ARRAY ****/
	/**********************/

	enum IndexBufferFormat {
		INDEX_BUFFER_FORMAT_UINT16,
		INDEX_BUFFER_FORMAT_UINT32,
	};

	enum VertexFrequency {
		VERTEX_FREQUENCY_VERTEX,
		VERTEX_FREQUENCY_INSTANCE,
	};

	struct VertexAttribute {
		uint32_t location = 0; // Shader location.
		uint32_t offset = 0;
		DataFormat format = DATA_FORMAT_MAX;
		uint32_t stride = 0;
		VertexFrequency frequency = VERTEX_FREQUENCY_VERTEX;
	};

	/*********************/
	/**** FRAMEBUFFER ****/
	/*********************/

	static const int32_t ATTACHMENT_UNUSED = -1;

	/****************/
	/**** SHADER ****/
	/****************/

	enum ShaderStage {
		SHADER_STAGE_VERTEX,
		SHADER_STAGE_FRAGMENT,
		SHADER_STAGE_TESSELATION_CONTROL,
		SHADER_STAGE_TESSELATION_EVALUATION,
		SHADER_STAGE_COMPUTE,
		SHADER_STAGE_MAX,
		SHADER_STAGE_VERTEX_BIT = (1 << SHADER_STAGE_VERTEX),
		SHADER_STAGE_FRAGMENT_BIT = (1 << SHADER_STAGE_FRAGMENT),
		SHADER_STAGE_TESSELATION_CONTROL_BIT = (1 << SHADER_STAGE_TESSELATION_CONTROL),
		SHADER_STAGE_TESSELATION_EVALUATION_BIT = (1 << SHADER_STAGE_TESSELATION_EVALUATION),
		SHADER_STAGE_COMPUTE_BIT = (1 << SHADER_STAGE_COMPUTE),
	};

	struct ShaderStageSPIRVData {
		ShaderStage shader_stage = SHADER_STAGE_MAX;
		Vector<uint8_t> spirv;
	};

	/*********************/
	/**** UNIFORM SET ****/
	/*********************/

	static const uint32_t MAX_UNIFORM_SETS = 16;

	enum UniformType {
		UNIFORM_TYPE_SAMPLER, // For sampling only (sampler GLSL type).
		UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, // For sampling only, but includes a texture, (samplerXX GLSL type), first a sampler then a texture.
		UNIFORM_TYPE_TEXTURE, // Only texture, (textureXX GLSL type).
		UNIFORM_TYPE_IMAGE, // Storage image (imageXX GLSL type), for compute mostly.
		UNIFORM_TYPE_TEXTURE_BUFFER, // Buffer texture (or TBO, textureBuffer type).
		UNIFORM_TYPE_SAMPLER_WITH_TEXTURE_BUFFER, // Buffer texture with a sampler(or TBO, samplerBuffer type).
		UNIFORM_TYPE_IMAGE_BUFFER, // Texel buffer, (imageBuffer type), for compute mostly.
		UNIFORM_TYPE_UNIFORM_BUFFER, // Regular uniform buffer (or UBO).
		UNIFORM_TYPE_STORAGE_BUFFER, // Storage buffer ("buffer" qualifier) like UBO, but supports storage, for compute mostly.
		UNIFORM_TYPE_INPUT_ATTACHMENT, // Used for sub-pass read/write, for mobile mostly.
		UNIFORM_TYPE_MAX
	};

	/******************/
	/**** PIPELINE ****/
	/******************/

	enum PipelineSpecializationConstantType {
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL,
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT,
		PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT,
	};

	struct PipelineSpecializationConstant {
		PipelineSpecializationConstantType type = {};
		uint32_t constant_id = 0xffffffff;
		union {
			uint32_t int_value = 0;
			float float_value;
			bool bool_value;
		};
	};

	/*******************/
	/**** RENDERING ****/
	/*******************/

	// ----- PIPELINE -----

	enum RenderPrimitive {
		RENDER_PRIMITIVE_POINTS,
		RENDER_PRIMITIVE_LINES,
		RENDER_PRIMITIVE_LINES_WITH_ADJACENCY,
		RENDER_PRIMITIVE_LINESTRIPS,
		RENDER_PRIMITIVE_LINESTRIPS_WITH_ADJACENCY,
		RENDER_PRIMITIVE_TRIANGLES,
		RENDER_PRIMITIVE_TRIANGLES_WITH_ADJACENCY,
		RENDER_PRIMITIVE_TRIANGLE_STRIPS,
		RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_AJACENCY,
		RENDER_PRIMITIVE_TRIANGLE_STRIPS_WITH_RESTART_INDEX,
		RENDER_PRIMITIVE_TESSELATION_PATCH,
		RENDER_PRIMITIVE_MAX
	};

	enum PolygonCullMode {
		POLYGON_CULL_DISABLED,
		POLYGON_CULL_FRONT,
		POLYGON_CULL_BACK,
		POLYGON_CULL_MAX
	};

	enum PolygonFrontFace {
		POLYGON_FRONT_FACE_CLOCKWISE,
		POLYGON_FRONT_FACE_COUNTER_CLOCKWISE,
	};

	enum StencilOperation {
		STENCIL_OP_KEEP,
		STENCIL_OP_ZERO,
		STENCIL_OP_REPLACE,
		STENCIL_OP_INCREMENT_AND_CLAMP,
		STENCIL_OP_DECREMENT_AND_CLAMP,
		STENCIL_OP_INVERT,
		STENCIL_OP_INCREMENT_AND_WRAP,
		STENCIL_OP_DECREMENT_AND_WRAP,
		STENCIL_OP_MAX
	};

	enum LogicOperation {
		LOGIC_OP_CLEAR,
		LOGIC_OP_AND,
		LOGIC_OP_AND_REVERSE,
		LOGIC_OP_COPY,
		LOGIC_OP_AND_INVERTED,
		LOGIC_OP_NO_OP,
		LOGIC_OP_XOR,
		LOGIC_OP_OR,
		LOGIC_OP_NOR,
		LOGIC_OP_EQUIVALENT,
		LOGIC_OP_INVERT,
		LOGIC_OP_OR_REVERSE,
		LOGIC_OP_COPY_INVERTED,
		LOGIC_OP_OR_INVERTED,
		LOGIC_OP_NAND,
		LOGIC_OP_SET,
		LOGIC_OP_MAX
	};

	enum BlendFactor {
		BLEND_FACTOR_ZERO,
		BLEND_FACTOR_ONE,
		BLEND_FACTOR_SRC_COLOR,
		BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		BLEND_FACTOR_DST_COLOR,
		BLEND_FACTOR_ONE_MINUS_DST_COLOR,
		BLEND_FACTOR_SRC_ALPHA,
		BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		BLEND_FACTOR_DST_ALPHA,
		BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
		BLEND_FACTOR_CONSTANT_COLOR,
		BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
		BLEND_FACTOR_CONSTANT_ALPHA,
		BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
		BLEND_FACTOR_SRC_ALPHA_SATURATE,
		BLEND_FACTOR_SRC1_COLOR,
		BLEND_FACTOR_ONE_MINUS_SRC1_COLOR,
		BLEND_FACTOR_SRC1_ALPHA,
		BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
		BLEND_FACTOR_MAX
	};

	enum BlendOperation {
		BLEND_OP_ADD,
		BLEND_OP_SUBTRACT,
		BLEND_OP_REVERSE_SUBTRACT,
		BLEND_OP_MINIMUM,
		BLEND_OP_MAXIMUM, // Yes, this one is an actual operator.
		BLEND_OP_MAX
	};

	struct PipelineRasterizationState {
		bool enable_depth_clamp = false;
		bool discard_primitives = false;
		bool wireframe = false;
		PolygonCullMode cull_mode = POLYGON_CULL_DISABLED;
		PolygonFrontFace front_face = POLYGON_FRONT_FACE_CLOCKWISE;
		bool depth_bias_enabled = false;
		float depth_bias_constant_factor = 0.0f;
		float depth_bias_clamp = 0.0f;
		float depth_bias_slope_factor = 0.0f;
		float line_width = 1.0f;
		uint32_t patch_control_points = 1;
	};

	struct PipelineMultisampleState {
		TextureSamples sample_count = TEXTURE_SAMPLES_1;
		bool enable_sample_shading = false;
		float min_sample_shading = 0.0f;
		Vector<uint32_t> sample_mask;
		bool enable_alpha_to_coverage = false;
		bool enable_alpha_to_one = false;
	};

	struct PipelineDepthStencilState {
		bool enable_depth_test = false;
		bool enable_depth_write = false;
		CompareOperator depth_compare_operator = COMPARE_OP_ALWAYS;
		bool enable_depth_range = false;
		float depth_range_min = 0;
		float depth_range_max = 0;
		bool enable_stencil = false;

		struct StencilOperationState {
			StencilOperation fail = STENCIL_OP_ZERO;
			StencilOperation pass = STENCIL_OP_ZERO;
			StencilOperation depth_fail = STENCIL_OP_ZERO;
			CompareOperator compare = COMPARE_OP_ALWAYS;
			uint32_t compare_mask = 0;
			uint32_t write_mask = 0;
			uint32_t reference = 0;
		};

		StencilOperationState front_op;
		StencilOperationState back_op;
	};

	struct PipelineColorBlendState {
		bool enable_logic_op = false;
		LogicOperation logic_op = LOGIC_OP_CLEAR;

		struct Attachment {
			bool enable_blend = false;
			BlendFactor src_color_blend_factor = BLEND_FACTOR_ZERO;
			BlendFactor dst_color_blend_factor = BLEND_FACTOR_ZERO;
			BlendOperation color_blend_op = BLEND_OP_ADD;
			BlendFactor src_alpha_blend_factor = BLEND_FACTOR_ZERO;
			BlendFactor dst_alpha_blend_factor = BLEND_FACTOR_ZERO;
			BlendOperation alpha_blend_op = BLEND_OP_ADD;
			bool write_r = true;
			bool write_g = true;
			bool write_b = true;
			bool write_a = true;
		};

		static PipelineColorBlendState create_disabled(int p_attachments = 1) {
			PipelineColorBlendState bs;
			for (int i = 0; i < p_attachments; i++) {
				bs.attachments.push_back(Attachment());
			}
			return bs;
		}

		static PipelineColorBlendState create_blend(int p_attachments = 1) {
			PipelineColorBlendState bs;
			for (int i = 0; i < p_attachments; i++) {
				Attachment ba;
				ba.enable_blend = true;
				ba.src_color_blend_factor = BLEND_FACTOR_SRC_ALPHA;
				ba.dst_color_blend_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				ba.src_alpha_blend_factor = BLEND_FACTOR_SRC_ALPHA;
				ba.dst_alpha_blend_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

				bs.attachments.push_back(ba);
			}
			return bs;
		}

		Vector<Attachment> attachments; // One per render target texture.
		Color blend_constant;
	};

	enum PipelineDynamicStateFlags {
		DYNAMIC_STATE_LINE_WIDTH = (1 << 0),
		DYNAMIC_STATE_DEPTH_BIAS = (1 << 1),
		DYNAMIC_STATE_BLEND_CONSTANTS = (1 << 2),
		DYNAMIC_STATE_DEPTH_BOUNDS = (1 << 3),
		DYNAMIC_STATE_STENCIL_COMPARE_MASK = (1 << 4),
		DYNAMIC_STATE_STENCIL_WRITE_MASK = (1 << 5),
		DYNAMIC_STATE_STENCIL_REFERENCE = (1 << 6),
	};

	/**************/
	/**** MISC ****/
	/**************/

	// This enum matches VkPhysicalDeviceType (except for `DEVICE_TYPE_MAX`).
	// Unlike VkPhysicalDeviceType, DeviceType is exposed to the scripting API.
	enum DeviceType {
		DEVICE_TYPE_OTHER,
		DEVICE_TYPE_INTEGRATED_GPU,
		DEVICE_TYPE_DISCRETE_GPU,
		DEVICE_TYPE_VIRTUAL_GPU,
		DEVICE_TYPE_CPU,
		DEVICE_TYPE_MAX
	};

	// Defined in an API-agnostic way.
	// Some may not make sense for the underlying API; in that case, 0 is returned.
	enum DriverResource {
		DRIVER_RESOURCE_LOGICAL_DEVICE,
		DRIVER_RESOURCE_PHYSICAL_DEVICE,
		DRIVER_RESOURCE_TOPMOST_OBJECT,
		DRIVER_RESOURCE_COMMAND_QUEUE,
		DRIVER_RESOURCE_QUEUE_FAMILY,
		DRIVER_RESOURCE_TEXTURE,
		DRIVER_RESOURCE_TEXTURE_VIEW,
		DRIVER_RESOURCE_TEXTURE_DATA_FORMAT,
		DRIVER_RESOURCE_SAMPLER,
		DRIVER_RESOURCE_UNIFORM_SET,
		DRIVER_RESOURCE_BUFFER,
		DRIVER_RESOURCE_COMPUTE_PIPELINE,
		DRIVER_RESOURCE_RENDER_PIPELINE,
#ifndef DISABLE_DEPRECATED
		DRIVER_RESOURCE_VULKAN_DEVICE = DRIVER_RESOURCE_LOGICAL_DEVICE,
		DRIVER_RESOURCE_VULKAN_PHYSICAL_DEVICE = DRIVER_RESOURCE_PHYSICAL_DEVICE,
		DRIVER_RESOURCE_VULKAN_INSTANCE = DRIVER_RESOURCE_TOPMOST_OBJECT,
		DRIVER_RESOURCE_VULKAN_QUEUE = DRIVER_RESOURCE_COMMAND_QUEUE,
		DRIVER_RESOURCE_VULKAN_QUEUE_FAMILY_INDEX = DRIVER_RESOURCE_QUEUE_FAMILY,
		DRIVER_RESOURCE_VULKAN_IMAGE = DRIVER_RESOURCE_TEXTURE,
		DRIVER_RESOURCE_VULKAN_IMAGE_VIEW = DRIVER_RESOURCE_TEXTURE_VIEW,
		DRIVER_RESOURCE_VULKAN_IMAGE_NATIVE_TEXTURE_FORMAT = DRIVER_RESOURCE_TEXTURE_DATA_FORMAT,
		DRIVER_RESOURCE_VULKAN_SAMPLER = DRIVER_RESOURCE_SAMPLER,
		DRIVER_RESOURCE_VULKAN_DESCRIPTOR_SET = DRIVER_RESOURCE_UNIFORM_SET,
		DRIVER_RESOURCE_VULKAN_BUFFER = DRIVER_RESOURCE_BUFFER,
		DRIVER_RESOURCE_VULKAN_COMPUTE_PIPELINE = DRIVER_RESOURCE_COMPUTE_PIPELINE,
		DRIVER_RESOURCE_VULKAN_RENDER_PIPELINE = DRIVER_RESOURCE_RENDER_PIPELINE,
#endif
	};

	enum Limit {
		LIMIT_MAX_BOUND_UNIFORM_SETS,
		LIMIT_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS,
		LIMIT_MAX_TEXTURES_PER_UNIFORM_SET,
		LIMIT_MAX_SAMPLERS_PER_UNIFORM_SET,
		LIMIT_MAX_STORAGE_BUFFERS_PER_UNIFORM_SET,
		LIMIT_MAX_STORAGE_IMAGES_PER_UNIFORM_SET,
		LIMIT_MAX_UNIFORM_BUFFERS_PER_UNIFORM_SET,
		LIMIT_MAX_DRAW_INDEXED_INDEX,
		LIMIT_MAX_FRAMEBUFFER_HEIGHT,
		LIMIT_MAX_FRAMEBUFFER_WIDTH,
		LIMIT_MAX_TEXTURE_ARRAY_LAYERS,
		LIMIT_MAX_TEXTURE_SIZE_1D,
		LIMIT_MAX_TEXTURE_SIZE_2D,
		LIMIT_MAX_TEXTURE_SIZE_3D,
		LIMIT_MAX_TEXTURE_SIZE_CUBE,
		LIMIT_MAX_TEXTURES_PER_SHADER_STAGE,
		LIMIT_MAX_SAMPLERS_PER_SHADER_STAGE,
		LIMIT_MAX_STORAGE_BUFFERS_PER_SHADER_STAGE,
		LIMIT_MAX_STORAGE_IMAGES_PER_SHADER_STAGE,
		LIMIT_MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE,
		LIMIT_MAX_PUSH_CONSTANT_SIZE,
		LIMIT_MAX_UNIFORM_BUFFER_SIZE,
		LIMIT_MAX_VERTEX_INPUT_ATTRIBUTE_OFFSET,
		LIMIT_MAX_VERTEX_INPUT_ATTRIBUTES,
		LIMIT_MAX_VERTEX_INPUT_BINDINGS,
		LIMIT_MAX_VERTEX_INPUT_BINDING_STRIDE,
		LIMIT_MIN_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
		LIMIT_MAX_COMPUTE_SHARED_MEMORY_SIZE,
		LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_X,
		LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Y,
		LIMIT_MAX_COMPUTE_WORKGROUP_COUNT_Z,
		LIMIT_MAX_COMPUTE_WORKGROUP_INVOCATIONS,
		LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_X,
		LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Y,
		LIMIT_MAX_COMPUTE_WORKGROUP_SIZE_Z,
		LIMIT_MAX_VIEWPORT_DIMENSIONS_X,
		LIMIT_MAX_VIEWPORT_DIMENSIONS_Y,
		LIMIT_SUBGROUP_SIZE,
		LIMIT_SUBGROUP_MIN_SIZE,
		LIMIT_SUBGROUP_MAX_SIZE,
		LIMIT_SUBGROUP_IN_SHADERS, // Set flags using SHADER_STAGE_VERTEX_BIT, SHADER_STAGE_FRAGMENT_BIT, etc.
		LIMIT_SUBGROUP_OPERATIONS,
		LIMIT_VRS_TEXEL_WIDTH,
		LIMIT_VRS_TEXEL_HEIGHT,
		LIMIT_VRS_MAX_FRAGMENT_WIDTH,
		LIMIT_VRS_MAX_FRAGMENT_HEIGHT,
	};

	enum Features {
		SUPPORTS_MULTIVIEW,
		SUPPORTS_FSR_HALF_FLOAT,
		SUPPORTS_ATTACHMENT_VRS,
		// If not supported, a fragment shader with only side effets (i.e., writes  to buffers, but doesn't output to attachments), may be optimized down to no-op by the GPU driver.
		SUPPORTS_FRAGMENT_SHADER_WITH_ONLY_SIDE_EFFECTS,
	};

	enum SubgroupOperations {
		SUBGROUP_BASIC_BIT = 1,
		SUBGROUP_VOTE_BIT = 2,
		SUBGROUP_ARITHMETIC_BIT = 4,
		SUBGROUP_BALLOT_BIT = 8,
		SUBGROUP_SHUFFLE_BIT = 16,
		SUBGROUP_SHUFFLE_RELATIVE_BIT = 32,
		SUBGROUP_CLUSTERED_BIT = 64,
		SUBGROUP_QUAD_BIT = 128,
	};

	////////////////////////////////////////////
	// PROTECTED STUFF
	// Not exposed by RenderingDevice, but shared
	// with RenderingDeviceDriver for convenience.
	////////////////////////////////////////////
protected:
	/*****************/
	/**** GENERIC ****/
	/*****************/

	static const char *const FORMAT_NAMES[DATA_FORMAT_MAX];

	/*****************/
	/**** TEXTURE ****/
	/*****************/

	static const uint32_t MAX_IMAGE_FORMAT_PLANES = 2;

	static const uint32_t TEXTURE_SAMPLES_COUNT[TEXTURE_SAMPLES_MAX];

	static uint32_t get_image_format_pixel_size(DataFormat p_format);
	static void get_compressed_image_format_block_dimensions(DataFormat p_format, uint32_t &r_w, uint32_t &r_h);
	uint32_t get_compressed_image_format_block_byte_size(DataFormat p_format) const;
	static uint32_t get_compressed_image_format_pixel_rshift(DataFormat p_format);
	static uint32_t get_image_format_required_size(DataFormat p_format, uint32_t p_width, uint32_t p_height, uint32_t p_depth, uint32_t p_mipmaps, uint32_t *r_blockw = nullptr, uint32_t *r_blockh = nullptr, uint32_t *r_depth = nullptr);
	static uint32_t get_image_required_mipmaps(uint32_t p_width, uint32_t p_height, uint32_t p_depth);
	static bool format_has_stencil(DataFormat p_format);
	static uint32_t format_get_plane_count(DataFormat p_format);

	/*****************/
	/**** SAMPLER ****/
	/*****************/

	static const Color SAMPLER_BORDER_COLOR_VALUE[SAMPLER_BORDER_COLOR_MAX];

	/**********************/
	/**** VERTEX ARRAY ****/
	/**********************/

	static uint32_t get_format_vertex_size(DataFormat p_format);

	/****************/
	/**** SHADER ****/
	/****************/

	static const char *SHADER_STAGE_NAMES[SHADER_STAGE_MAX];

public:
	struct ShaderUniform {
		UniformType type = UniformType::UNIFORM_TYPE_MAX;
		bool writable = false;
		uint32_t binding = 0;
		BitField<ShaderStage> stages;
		uint32_t length = 0; // Size of arrays (in total elements), or ubos (in bytes * total elements).

		bool operator!=(const ShaderUniform &p_other) const {
			return binding != p_other.binding || type != p_other.type || writable != p_other.writable || stages != p_other.stages || length != p_other.length;
		}

		bool operator<(const ShaderUniform &p_other) const {
			if (binding != p_other.binding) {
				return binding < p_other.binding;
			}
			if (type != p_other.type) {
				return type < p_other.type;
			}
			if (writable != p_other.writable) {
				return writable < p_other.writable;
			}
			if (stages != p_other.stages) {
				return stages < p_other.stages;
			}
			if (length != p_other.length) {
				return length < p_other.length;
			}
			return false;
		}
	};

	struct ShaderSpecializationConstant : public PipelineSpecializationConstant {
		BitField<ShaderStage> stages;

		bool operator<(const ShaderSpecializationConstant &p_other) const { return constant_id < p_other.constant_id; }
	};

	struct ShaderDescription {
		uint64_t vertex_input_mask = 0;
		uint32_t fragment_output_mask = 0;
		bool is_compute = false;
		uint32_t compute_local_size[3] = {};
		uint32_t push_constant_size = 0;

		Vector<Vector<ShaderUniform>> uniform_sets;
		Vector<ShaderSpecializationConstant> specialization_constants;
		Vector<ShaderStage> stages;
	};

protected:
	struct ShaderReflection : public ShaderDescription {
		BitField<ShaderStage> stages;
		BitField<ShaderStage> push_constant_stages;
	};
};

#endif // RENDERING_DEVICE_COMMONS_H
