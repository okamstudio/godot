// basisu_transcoder.h
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
// Important: If compiling with gcc, be sure strict aliasing is disabled: -fno-strict-aliasing
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

// By default KTX2 support is enabled to simplify compilation. This implies the need for the Zstandard library (which we distribute as a single source file in the "zstd" directory) by default.
// Set BASISD_SUPPORT_KTX2 to 0 to completely disable KTX2 support as well as Zstd/miniz usage which is only required for UASTC supercompression in KTX2 files.
// Also see BASISD_SUPPORT_KTX2_ZSTD in basisu_transcoder.cpp, which individually disables Zstd usage.
#ifndef BASISD_SUPPORT_KTX2
	#define BASISD_SUPPORT_KTX2 1
#endif

// Set BASISD_SUPPORT_KTX2_ZSTD to 0 to disable Zstd usage and KTX2 UASTC Zstd supercompression support 
#ifndef BASISD_SUPPORT_KTX2_ZSTD
	#define BASISD_SUPPORT_KTX2_ZSTD 1
#endif

// Set BASISU_FORCE_DEVEL_MESSAGES to 1 to enable debug printf()'s whenever an error occurs, for easier debugging during development.
#ifndef BASISU_FORCE_DEVEL_MESSAGES
	// TODO - disable before checking in
	#define BASISU_FORCE_DEVEL_MESSAGES 0
#endif

#include "basisu_transcoder_internal.h"
#include "basisu_transcoder_uastc.h"
#include "basisu_file_headers.h"

namespace basist
{
	// High-level composite texture formats supported by the transcoder.
	// Each of these texture formats directly correspond to OpenGL/D3D/Vulkan etc. texture formats.
	// Notes:
	// - If you specify a texture format that supports alpha, but the .basis file doesn't have alpha, the transcoder will automatically output a 
	// fully opaque (255) alpha channel.
	// - The PVRTC1 texture formats only support power of 2 dimension .basis files, but this may be relaxed in a future version.
	// - The PVRTC1 transcoders are real-time encoders, so don't expect the highest quality. We may add a slower encoder with improved quality.
	// - These enums must be kept in sync with Javascript code that calls the transcoder.
	enum class transcoder_texture_format
	{
		// Compressed formats

		// ETC1-2
		cTFETC1_RGB = 0,							// Opaque only, returns RGB or alpha data if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified
		cTFETC2_RGBA = 1,							// Opaque+alpha, ETC2_EAC_A8 block followed by a ETC1 block, alpha channel will be opaque for opaque .basis files

		// BC1-5, BC7 (desktop, some mobile devices)
		cTFBC1_RGB = 2,								// Opaque only, no punchthrough alpha support yet, transcodes alpha slice if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified
		cTFBC3_RGBA = 3, 							// Opaque+alpha, BC4 followed by a BC1 block, alpha channel will be opaque for opaque .basis files
		cTFBC4_R = 4,								// Red only, alpha slice is transcoded to output if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified
		cTFBC5_RG = 5,								// XY: Two BC4 blocks, X=R and Y=Alpha, .basis file should have alpha data (if not Y will be all 255's)
		cTFBC7_RGBA = 6,							// RGB or RGBA, mode 5 for ETC1S, modes (1,2,3,5,6,7) for UASTC

		// PVRTC1 4bpp (mobile, PowerVR devices)
		cTFPVRTC1_4_RGB = 8,						// Opaque only, RGB or alpha if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified, nearly lowest quality of any texture format.
		cTFPVRTC1_4_RGBA = 9,						// Opaque+alpha, most useful for simple opacity maps. If .basis file doesn't have alpha cTFPVRTC1_4_RGB will be used instead. Lowest quality of any supported texture format.

		// ASTC (mobile, Intel devices, hopefully all desktop GPU's one day)
		cTFASTC_4x4_RGBA = 10,						// LDR. Opaque+alpha, ASTC 4x4, alpha channel will be opaque for opaque .basis files. 
													// LDR: Transcoder uses RGB/RGBA/L/LA modes, void extent, and up to two ([0,47] and [0,255]) endpoint precisions.

		// ATC (mobile, Adreno devices, this is a niche format)
		cTFATC_RGB = 11,							// Opaque, RGB or alpha if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified. ATI ATC (GL_ATC_RGB_AMD)
		cTFATC_RGBA = 12,							// Opaque+alpha, alpha channel will be opaque for opaque .basis files. ATI ATC (GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD) 

		// FXT1 (desktop, Intel devices, this is a super obscure format)
		cTFFXT1_RGB = 17,							// Opaque only, uses exclusively CC_MIXED blocks. Notable for having a 8x4 block size. GL_3DFX_texture_compression_FXT1 is supported on Intel integrated GPU's (such as HD 630).
													// Punch-through alpha is relatively easy to support, but full alpha is harder. This format is only here for completeness so opaque-only is fine for now.
													// See the BASISU_USE_ORIGINAL_3DFX_FXT1_ENCODING macro in basisu_transcoder_internal.h.

		cTFPVRTC2_4_RGB = 18,					// Opaque-only, almost BC1 quality, much faster to transcode and supports arbitrary texture dimensions (unlike PVRTC1 RGB).
		cTFPVRTC2_4_RGBA = 19,					// Opaque+alpha, slower to encode than cTFPVRTC2_4_RGB. Premultiplied alpha is highly recommended, otherwise the color channel can leak into the alpha channel on transparent blocks.

		cTFETC2_EAC_R11 = 20,					// R only (ETC2 EAC R11 unsigned)
		cTFETC2_EAC_RG11 = 21,					// RG only (ETC2 EAC RG11 unsigned), R=opaque.r, G=alpha - for tangent space normal maps

		cTFBC6H = 22,							// HDR, RGB only, unsigned
		cTFASTC_HDR_4x4_RGBA = 23,				// HDR, RGBA (currently UASTC HDR is only RGB), unsigned

		// Uncompressed (raw pixel) formats
		// Note these uncompressed formats (RGBA32, 565, and 4444) can only be transcoded to from LDR input files (ETC1S or UASTC LDR).
		cTFRGBA32 = 13,							// 32bpp RGBA image stored in raster (not block) order in memory, R is first byte, A is last byte.
		cTFRGB565 = 14,							// 16bpp RGB image stored in raster (not block) order in memory, R at bit position 11
		cTFBGR565 = 15,							// 16bpp RGB image stored in raster (not block) order in memory, R at bit position 0
		cTFRGBA4444 = 16,						// 16bpp RGBA image stored in raster (not block) order in memory, R at bit position 12, A at bit position 0
		
		// Note these uncompressed formats (HALF and 9E5) can only be transcoded to from HDR input files (UASTC HDR).
		cTFRGB_HALF = 24,						// 48bpp RGB half (16-bits/component, 3 components)
		cTFRGBA_HALF = 25,						// 64bpp RGBA half (16-bits/component, 4 components) (A will always currently 1.0, UASTC_HDR doesn't support alpha)
		cTFRGB_9E5 = 26,						// 32bpp RGB 9E5 (shared exponent, positive only, see GL_EXT_texture_shared_exponent)

		cTFTotalTextureFormats = 27,

		// Old enums for compatibility with code compiled against previous versions
		cTFETC1 = cTFETC1_RGB,
		cTFETC2 = cTFETC2_RGBA,
		cTFBC1 = cTFBC1_RGB,
		cTFBC3 = cTFBC3_RGBA,
		cTFBC4 = cTFBC4_R,
		cTFBC5 = cTFBC5_RG,

		// Previously, the caller had some control over which BC7 mode the transcoder output. We've simplified this due to UASTC, which supports numerous modes.
		cTFBC7_M6_RGB = cTFBC7_RGBA,			// Opaque only, RGB or alpha if cDecodeFlagsTranscodeAlphaDataToOpaqueFormats flag is specified. Highest quality of all the non-ETC1 formats.
		cTFBC7_M5_RGBA = cTFBC7_RGBA,			// Opaque+alpha, alpha channel will be opaque for opaque .basis files
		cTFBC7_M6_OPAQUE_ONLY = cTFBC7_RGBA,
		cTFBC7_M5 = cTFBC7_RGBA,
		cTFBC7_ALT = 7,

		cTFASTC_4x4 = cTFASTC_4x4_RGBA,

		cTFATC_RGBA_INTERPOLATED_ALPHA = cTFATC_RGBA,
	};

	// For compressed texture formats, this returns the # of bytes per block. For uncompressed, it returns the # of bytes per pixel.
	// NOTE: Previously, this function was called basis_get_bytes_per_block(), and it always returned 16*bytes_per_pixel for uncompressed formats which was confusing.
	uint32_t basis_get_bytes_per_block_or_pixel(transcoder_texture_format fmt);

	// Returns format's name in ASCII
	const char* basis_get_format_name(transcoder_texture_format fmt);

	// Returns block format name in ASCII
	const char* basis_get_block_format_name(block_format fmt);

	// Returns true if the format supports an alpha channel.
	bool basis_transcoder_format_has_alpha(transcoder_texture_format fmt);

	// Returns true if the format is HDR.
	bool basis_transcoder_format_is_hdr(transcoder_texture_format fmt);

	// Returns the basisu::texture_format corresponding to the specified transcoder_texture_format.
	basisu::texture_format basis_get_basisu_texture_format(transcoder_texture_format fmt);

	// Returns the texture type's name in ASCII.
	const char* basis_get_texture_type_name(basis_texture_type tex_type);

	// Returns true if the transcoder texture type is an uncompressed (raw pixel) format.
	bool basis_transcoder_format_is_uncompressed(transcoder_texture_format tex_type);

	// Returns the # of bytes per pixel for uncompressed formats, or 0 for block texture formats.
	uint32_t basis_get_uncompressed_bytes_per_pixel(transcoder_texture_format fmt);

	// Returns the block width for the specified texture format, which is currently either 4 or 8 for FXT1.
	uint32_t basis_get_block_width(transcoder_texture_format tex_type);

	// Returns the block height for the specified texture format, which is currently always 4.
	uint32_t basis_get_block_height(transcoder_texture_format tex_type);

	// Returns true if the specified format was enabled at compile time, and is supported for the specific basis/ktx2 texture format (ETC1S, UASTC, or UASTC HDR).
	bool basis_is_format_supported(transcoder_texture_format tex_type, basis_tex_format fmt = basis_tex_format::cETC1S);

	// Validates that the output buffer is large enough to hold the entire transcoded texture.
	// For uncompressed texture formats, most input parameters are in pixels, not blocks. Blocks are 4x4 pixels.
	bool basis_validate_output_buffer_size(transcoder_texture_format target_format,
		uint32_t output_blocks_buf_size_in_blocks_or_pixels,
		uint32_t orig_width, uint32_t orig_height,
		uint32_t output_row_pitch_in_blocks_or_pixels,
		uint32_t output_rows_in_pixels,
		uint32_t total_slice_blocks);

	class basisu_transcoder;

	// This struct holds all state used during transcoding. For video, it needs to persist between image transcodes (it holds the previous frame).
	// For threading you can use one state per thread.
	struct basisu_transcoder_state
	{
		struct block_preds
		{
			uint16_t m_endpoint_index;
			uint8_t m_pred_bits;
		};

		basisu::vector<block_preds> m_block_endpoint_preds[2];

		enum { cMaxPrevFrameLevels = 16 };
		basisu::vector<uint32_t> m_prev_frame_indices[2][cMaxPrevFrameLevels]; // [alpha_flag][level_index] 

		void clear()
		{
			for (uint32_t i = 0; i < 2; i++)
			{
				m_block_endpoint_preds[i].clear();

				for (uint32_t j = 0; j < cMaxPrevFrameLevels; j++)
					m_prev_frame_indices[i][j].clear();
			}
		}
	};

	// Low-level helper class that does the actual transcoding.
	class basisu_lowlevel_etc1s_transcoder
	{
		friend class basisu_transcoder;

	public:
		basisu_lowlevel_etc1s_transcoder();

		void set_global_codebooks(const basisu_lowlevel_etc1s_transcoder* pGlobal_codebook) { m_pGlobal_codebook = pGlobal_codebook; }
		const basisu_lowlevel_etc1s_transcoder* get_global_codebooks() const { return m_pGlobal_codebook; }

		bool decode_palettes(
			uint32_t num_endpoints, const uint8_t* pEndpoints_data, uint32_t endpoints_data_size,
			uint32_t num_selectors, const uint8_t* pSelectors_data, uint32_t selectors_data_size);

		bool decode_tables(const uint8_t* pTable_data, uint32_t table_data_size);

		bool transcode_slice(void* pDst_blocks, uint32_t num_blocks_x, uint32_t num_blocks_y, const uint8_t* pImage_data, uint32_t image_data_size, block_format fmt,
			uint32_t output_block_or_pixel_stride_in_bytes, bool bc1_allow_threecolor_blocks, const bool is_video, const bool is_alpha_slice, const uint32_t level_index, const uint32_t orig_width, const uint32_t orig_height, uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr, bool astc_transcode_alpha = false, void* pAlpha_blocks = nullptr, uint32_t output_rows_in_pixels = 0);

		bool transcode_slice(void* pDst_blocks, uint32_t num_blocks_x, uint32_t num_blocks_y, const uint8_t* pImage_data, uint32_t image_data_size, block_format fmt,
			uint32_t output_block_or_pixel_stride_in_bytes, bool bc1_allow_threecolor_blocks, const basis_file_header& header, const basis_slice_desc& slice_desc, uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr, bool astc_transcode_alpha = false, void* pAlpha_blocks = nullptr, uint32_t output_rows_in_pixels = 0)
		{
			return transcode_slice(pDst_blocks, num_blocks_x, num_blocks_y, pImage_data, image_data_size, fmt, output_block_or_pixel_stride_in_bytes, bc1_allow_threecolor_blocks,
				header.m_tex_type == cBASISTexTypeVideoFrames, (slice_desc.m_flags & cSliceDescFlagsHasAlpha) != 0, slice_desc.m_level_index,
				slice_desc.m_orig_width, slice_desc.m_orig_height, output_row_pitch_in_blocks_or_pixels, pState,
				astc_transcode_alpha,
				pAlpha_blocks,
				output_rows_in_pixels);
		}

		// Container independent transcoding
		bool transcode_image(
			transcoder_texture_format target_format,
			void* pOutput_blocks, uint32_t output_blocks_buf_size_in_blocks_or_pixels,
			const uint8_t* pCompressed_data, uint32_t compressed_data_length,
			uint32_t num_blocks_x, uint32_t num_blocks_y, uint32_t orig_width, uint32_t orig_height, uint32_t level_index,
			uint32_t rgb_offset, uint32_t rgb_length, uint32_t alpha_offset, uint32_t alpha_length,
			uint32_t decode_flags = 0,
			bool basis_file_has_alpha_slices = false,
			bool is_video = false,
			uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr,
			uint32_t output_rows_in_pixels = 0);

		void clear()
		{
			m_local_endpoints.clear();
			m_local_selectors.clear();
			m_endpoint_pred_model.clear();
			m_delta_endpoint_model.clear();
			m_selector_model.clear();
			m_selector_history_buf_rle_model.clear();
			m_selector_history_buf_size = 0;
		}

		// Low-level methods
		typedef basisu::vector<endpoint> endpoint_vec;
		const endpoint_vec& get_endpoints() const { return m_local_endpoints; }

		typedef basisu::vector<selector> selector_vec;
		const selector_vec& get_selectors() const { return m_local_selectors; }
				
	private:
		const basisu_lowlevel_etc1s_transcoder* m_pGlobal_codebook;

		endpoint_vec m_local_endpoints;
		selector_vec m_local_selectors;
				
		huffman_decoding_table m_endpoint_pred_model, m_delta_endpoint_model, m_selector_model, m_selector_history_buf_rle_model;

		uint32_t m_selector_history_buf_size;

		basisu_transcoder_state m_def_state;
	};

	enum basisu_decode_flags
	{
		// PVRTC1: decode non-pow2 ETC1S texture level to the next larger power of 2 (not implemented yet, but we're going to support it). Ignored if the slice's dimensions are already a power of 2.
		cDecodeFlagsPVRTCDecodeToNextPow2 = 2,

		// When decoding to an opaque texture format, if the basis file has alpha, decode the alpha slice instead of the color slice to the output texture format.
		// This is primarily to allow decoding of textures with alpha to multiple ETC1 textures (one for color, another for alpha).
		cDecodeFlagsTranscodeAlphaDataToOpaqueFormats = 4,

		// Forbid usage of BC1 3 color blocks (we don't support BC1 punchthrough alpha yet).
		// This flag is used internally when decoding to BC3.
		cDecodeFlagsBC1ForbidThreeColorBlocks = 8,

		// The output buffer contains alpha endpoint/selector indices. 
		// Used internally when decoding formats like ASTC that require both color and alpha data to be available when transcoding to the output format.
		cDecodeFlagsOutputHasAlphaIndices = 16,

		cDecodeFlagsHighQuality = 32
	};

	class basisu_lowlevel_uastc_transcoder
	{
		friend class basisu_transcoder;

	public:
		basisu_lowlevel_uastc_transcoder();

		bool transcode_slice(void* pDst_blocks, uint32_t num_blocks_x, uint32_t num_blocks_y, const uint8_t* pImage_data, uint32_t image_data_size, block_format fmt,
			uint32_t output_block_or_pixel_stride_in_bytes, bool bc1_allow_threecolor_blocks, bool has_alpha, const uint32_t orig_width, const uint32_t orig_height, uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr, uint32_t output_rows_in_pixels = 0, int channel0 = -1, int channel1 = -1, uint32_t decode_flags = 0);

		bool transcode_slice(void* pDst_blocks, uint32_t num_blocks_x, uint32_t num_blocks_y, const uint8_t* pImage_data, uint32_t image_data_size, block_format fmt,
			uint32_t output_block_or_pixel_stride_in_bytes, bool bc1_allow_threecolor_blocks, const basis_file_header& header, const basis_slice_desc& slice_desc, uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr, uint32_t output_rows_in_pixels = 0, int channel0 = -1, int channel1 = -1, uint32_t decode_flags = 0)
		{
			return transcode_slice(pDst_blocks, num_blocks_x, num_blocks_y, pImage_data, image_data_size, fmt,
				output_block_or_pixel_stride_in_bytes, bc1_allow_threecolor_blocks, (header.m_flags & cBASISHeaderFlagHasAlphaSlices) != 0, slice_desc.m_orig_width, slice_desc.m_orig_height, output_row_pitch_in_blocks_or_pixels,
				pState, output_rows_in_pixels, channel0, channel1, decode_flags);
		}

		// Container independent transcoding
		bool transcode_image(
			transcoder_texture_format target_format,
			void* pOutput_blocks, uint32_t output_blocks_buf_size_in_blocks_or_pixels,
			const uint8_t* pCompressed_data, uint32_t compressed_data_length,
			uint32_t num_blocks_x, uint32_t num_blocks_y, uint32_t orig_width, uint32_t orig_height, uint32_t level_index,
			uint32_t slice_offset, uint32_t slice_length,
			uint32_t decode_flags = 0,
			bool has_alpha = false,
			bool is_video = false,
			uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr,
			uint32_t output_rows_in_pixels = 0,
			int channel0 = -1, int channel1 = -1);
	};

	class basisu_lowlevel_uastc_hdr_transcoder
	{
		friend class basisu_transcoder;

	public:
		basisu_lowlevel_uastc_hdr_transcoder();

		bool transcode_slice(void* pDst_blocks, uint32_t num_blocks_x, uint32_t num_blocks_y, const uint8_t* pImage_data, uint32_t image_data_size, block_format fmt,
			uint32_t output_block_or_pixel_stride_in_bytes, bool bc1_allow_threecolor_blocks, bool has_alpha, const uint32_t orig_width, const uint32_t orig_height, uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr, uint32_t output_rows_in_pixels = 0, int channel0 = -1, int channel1 = -1, uint32_t decode_flags = 0);

		bool transcode_slice(void* pDst_blocks, uint32_t num_blocks_x, uint32_t num_blocks_y, const uint8_t* pImage_data, uint32_t image_data_size, block_format fmt,
			uint32_t output_block_or_pixel_stride_in_bytes, bool bc1_allow_threecolor_blocks, const basis_file_header& header, const basis_slice_desc& slice_desc, uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr, uint32_t output_rows_in_pixels = 0, int channel0 = -1, int channel1 = -1, uint32_t decode_flags = 0)
		{
			return transcode_slice(pDst_blocks, num_blocks_x, num_blocks_y, pImage_data, image_data_size, fmt,
				output_block_or_pixel_stride_in_bytes, bc1_allow_threecolor_blocks, (header.m_flags & cBASISHeaderFlagHasAlphaSlices) != 0, slice_desc.m_orig_width, slice_desc.m_orig_height, output_row_pitch_in_blocks_or_pixels,
				pState, output_rows_in_pixels, channel0, channel1, decode_flags);
		}

		// Container independent transcoding
		bool transcode_image(
			transcoder_texture_format target_format,
			void* pOutput_blocks, uint32_t output_blocks_buf_size_in_blocks_or_pixels,
			const uint8_t* pCompressed_data, uint32_t compressed_data_length,
			uint32_t num_blocks_x, uint32_t num_blocks_y, uint32_t orig_width, uint32_t orig_height, uint32_t level_index,
			uint32_t slice_offset, uint32_t slice_length,
			uint32_t decode_flags = 0,
			bool has_alpha = false,
			bool is_video = false,
			uint32_t output_row_pitch_in_blocks_or_pixels = 0,
			basisu_transcoder_state* pState = nullptr,
			uint32_t output_rows_in_pixels = 0,
			int channel0 = -1, int channel1 = -1);
	};

	struct basisu_slice_info
	{
		uint32_t m_orig_width;
		uint32_t m_orig_height;

		uint32_t m_width;
		uint32_t m_height;

		uint32_t m_num_blocks_x;
		uint32_t m_num_blocks_y;
		uint32_t m_total_blocks;

		uint32_t m_compressed_size;

		uint32_t m_slice_index;	// the slice index in the .basis file
		uint32_t m_image_index;	// the source image index originally provided to the encoder
		uint32_t m_level_index;	// the mipmap level within this image

		uint32_t m_unpacked_slice_crc16;

		bool m_alpha_flag;		// true if the slice has alpha data
		bool m_iframe_flag;		// true if the slice is an I-Frame
	};

	typedef basisu::vector<basisu_slice_info> basisu_slice_info_vec;

	struct basisu_image_info
	{
		uint32_t m_image_index;
		uint32_t m_total_levels;

		uint32_t m_orig_width;
		uint32_t m_orig_height;

		uint32_t m_width;
		uint32_t m_height;

		uint32_t m_num_blocks_x;
		uint32_t m_num_blocks_y;
		uint32_t m_total_blocks;

		uint32_t m_first_slice_index;

		bool m_alpha_flag;		// true if the image has alpha data
		bool m_iframe_flag;		// true if the image is an I-Frame
	};

	struct basisu_image_level_info
	{
		uint32_t m_image_index;
		uint32_t m_level_index;

		uint32_t m_orig_width;
		uint32_t m_orig_height;

		uint32_t m_width;
		uint32_t m_height;

		uint32_t m_num_blocks_x;
		uint32_t m_num_blocks_y;
		uint32_t m_total_blocks;

		uint32_t m_first_slice_index;

		uint32_t m_rgb_file_ofs;
		uint32_t m_rgb_file_len;
		uint32_t m_alpha_file_ofs;
		uint32_t m_alpha_file_len;

		bool m_alpha_flag;		// true if the image has alpha data
		bool m_iframe_flag;		// true if the image is an I-Frame
	};

	struct basisu_file_info
	{
		uint32_t m_version;
		uint32_t m_total_header_size;

		uint32_t m_total_selectors;
		// will be 0 for UASTC or if the file uses global codebooks
		uint32_t m_selector_codebook_ofs;
		uint32_t m_selector_codebook_size;

		uint32_t m_total_endpoints;
		// will be 0 for UASTC or if the file uses global codebooks
		uint32_t m_endpoint_codebook_ofs;
		uint32_t m_endpoint_codebook_size;

		uint32_t m_tables_ofs;
		uint32_t m_tables_size;

		uint32_t m_slices_size;

		basis_texture_type m_tex_type;
		uint32_t m_us_per_frame;

		// Low-level slice information (1 slice per image for color-only basis files, 2 for alpha basis files)
		basisu_slice_info_vec m_slice_info;

		uint32_t m_total_images;	 // total # of images
		basisu::vector<uint32_t> m_image_mipmap_levels; // the # of mipmap levels for each image

		uint32_t m_userdata0;
		uint32_t m_userdata1;

		basis_tex_format m_tex_format; // ETC1S, UASTC, etc.

		bool m_y_flipped;				// true if the image was Y flipped
		bool m_etc1s;					// true if the file is ETC1S
		bool m_has_alpha_slices;	// true if the texture has alpha slices (for ETC1S: even slices RGB, odd slices alpha)
	};

	// High-level transcoder class which accepts .basis file data and allows the caller to query information about the file and transcode image levels to various texture formats.
	// If you're just starting out this is the class you care about.
	class basisu_transcoder
	{
		basisu_transcoder(basisu_transcoder&);
		basisu_transcoder& operator= (const basisu_transcoder&);

	public:
		basisu_transcoder();

		// Validates the .basis file. This computes a crc16 over the entire file, so it's slow.
		bool validate_file_checksums(const void* pData, uint32_t data_size, bool full_validation) const;

		// Quick header validation - no crc16 checks.
		bool validate_header(const void* pData, uint32_t data_size) const;

		basis_texture_type get_texture_type(const void* pData, uint32_t data_size) const;
		bool get_userdata(const void* pData, uint32_t data_size, uint32_t& userdata0, uint32_t& userdata1) const;

		// Returns the total number of images in the basis file (always 1 or more).
		// Note that the number of mipmap levels for each image may differ, and that images may have different resolutions.
		uint32_t get_total_images(const void* pData, uint32_t data_size) const;

		basis_tex_format get_tex_format(const void* pData, uint32_t data_size) const;

		// Returns the number of mipmap levels in an image.
		uint32_t get_total_image_levels(const void* pData, uint32_t data_size, uint32_t image_index) const;

		// Returns basic information about an image. Note that orig_width/orig_height may not be a multiple of 4.
		bool get_image_level_desc(const void* pData, uint32_t data_size, uint32_t image_index, uint32_t level_index, uint32_t& orig_width, uint32_t& orig_height, uint32_t& total_blocks) const;

		// Returns information about the specified image.
		bool get_image_info(const void* pData, uint32_t data_size, basisu_image_info& image_info, uint32_t image_index) const;

		// Returns information about the specified image's mipmap level.
		bool get_image_level_info(const void* pData, uint32_t data_size, basisu_image_level_info& level_info, uint32_t image_index, uint32_t level_index) const;

		// Get a description of the basis file and low-level information about each slice.
		bool get_file_info(const void* pData, uint32_t data_size, basisu_file_info& file_info) const;

		// start_transcoding() must be called before calling transcode_slice() or transcode_image_level().
		// For ETC1S files, this call decompresses the selector/endpoint codebooks, so ideally you would only call this once per .basis file (not each image/mipmap level).
		bool start_transcoding(const void* pData, uint32_t data_size);

		bool stop_transcoding();

		// Returns true if start_transcoding() has been called.
		bool get_ready_to_transcode() const { return m_ready_to_transcode; }

		// transcode_image_level() decodes a single mipmap level from the .basis file to any of the supported output texture formats.
		// It'll first find the slice(s) to transcode, then call transcode_slice() one or two times to decode both the color and alpha texture data (or RG texture data from two slices for BC5).
		// If the .basis file doesn't have alpha slices, the output alpha blocks will be set to fully opaque (all 255's).
		// Currently, to decode to PVRTC1 the basis texture's dimensions in pixels must be a power of 2, due to PVRTC1 format requirements. 
		// output_blocks_buf_size_in_blocks_or_pixels should be at least the image level's total_blocks (num_blocks_x * num_blocks_y), or the total number of output pixels if fmt==cTFRGBA32.
		// output_row_pitch_in_blocks_or_pixels: Number of blocks or pixels per row. If 0, the transcoder uses the slice's num_blocks_x or orig_width (NOT num_blocks_x * 4). Ignored for PVRTC1 (due to texture swizzling).
		// output_rows_in_pixels: Ignored unless fmt is uncompressed (cRGBA32, etc.). The total number of output rows in the output buffer. If 0, the transcoder assumes the slice's orig_height (NOT num_blocks_y * 4).
		// Notes: 
		// - basisu_transcoder_init() must have been called first to initialize the transcoder lookup tables before calling this function.
		// - This method assumes the output texture buffer is readable. In some cases to handle alpha, the transcoder will write temporary data to the output texture in
		// a first pass, which will be read in a second pass.
		bool transcode_image_level(
			const void* pData, uint32_t data_size,
			uint32_t image_index, uint32_t level_index,
			void* pOutput_blocks, uint32_t output_blocks_buf_size_in_blocks_or_pixels,
			transcoder_texture_format fmt,
			uint32_t decode_flags = 0, uint32_t output_row_pitch_in_blocks_or_pixels = 0, basisu_transcoder_state* pState = nullptr, uint32_t output_rows_in_pixels = 0) const;

		// Finds the basis slice corresponding to the specified image/level/alpha params, or -1 if the slice can't be found.
		int find_slice(const void* pData, uint32_t data_size, uint32_t image_index, uint32_t level_index, bool alpha_data) const;

		// transcode_slice() decodes a single slice from the .basis file. It's a low-level API - most likely you want to use transcode_image_level().
		// This is a low-level API, and will be needed to be called multiple times to decode some texture formats (like BC3, BC5, or ETC2).
		// output_blocks_buf_size_in_blocks_or_pixels is just used for verification to make sure the output buffer is large enough.
		// output_blocks_buf_size_in_blocks_or_pixels should be at least the image level's total_blocks (num_blocks_x * num_blocks_y), or the total number of output pixels if fmt==cTFRGBA32.
		// output_block_stride_in_bytes: Number of bytes between each output block.
		// output_row_pitch_in_blocks_or_pixels: Number of blocks or pixels per row. If 0, the transcoder uses the slice's num_blocks_x or orig_width (NOT num_blocks_x * 4). Ignored for PVRTC1 (due to texture swizzling).
		// output_rows_in_pixels: Ignored unless fmt is cRGBA32. The total number of output rows in the output buffer. If 0, the transcoder assumes the slice's orig_height (NOT num_blocks_y * 4).
		// Notes:
		// - basisu_transcoder_init() must have been called first to initialize the transcoder lookup tables before calling this function.
		bool transcode_slice(const void* pData, uint32_t data_size, uint32_t slice_index,
			void* pOutput_blocks, uint32_t output_blocks_buf_size_in_blocks_or_pixels,
			block_format fmt, uint32_t output_block_stride_in_bytes, uint32_t decode_flags = 0, uint32_t output_row_pitch_in_blocks_or_pixels = 0, basisu_transcoder_state* pState = nullptr, void* pAlpha_blocks = nullptr,
			uint32_t output_rows_in_pixels = 0, int channel0 = -1, int channel1 = -1) const;

		static void write_opaque_alpha_blocks(
			uint32_t num_blocks_x, uint32_t num_blocks_y,
			void* pOutput_blocks, block_format fmt,
			uint32_t block_stride_in_bytes, uint32_t output_row_pitch_in_blocks_or_pixels);

		void set_global_codebooks(const basisu_lowlevel_etc1s_transcoder* pGlobal_codebook) { m_lowlevel_etc1s_decoder.set_global_codebooks(pGlobal_codebook); }
		const basisu_lowlevel_etc1s_transcoder* get_global_codebooks() const { return m_lowlevel_etc1s_decoder.get_global_codebooks(); }

		const basisu_lowlevel_etc1s_transcoder& get_lowlevel_etc1s_decoder() const { return m_lowlevel_etc1s_decoder; }
		basisu_lowlevel_etc1s_transcoder& get_lowlevel_etc1s_decoder() { return m_lowlevel_etc1s_decoder; }

		const basisu_lowlevel_uastc_transcoder& get_lowlevel_uastc_decoder() const { return m_lowlevel_uastc_decoder; }
		basisu_lowlevel_uastc_transcoder& get_lowlevel_uastc_decoder() { return m_lowlevel_uastc_decoder; }

	private:
		mutable basisu_lowlevel_etc1s_transcoder m_lowlevel_etc1s_decoder;
		mutable basisu_lowlevel_uastc_transcoder m_lowlevel_uastc_decoder;
		mutable basisu_lowlevel_uastc_hdr_transcoder m_lowlevel_uastc_hdr_decoder;

		bool m_ready_to_transcode;

		int find_first_slice_index(const void* pData, uint32_t data_size, uint32_t image_index, uint32_t level_index) const;

		bool validate_header_quick(const void* pData, uint32_t data_size) const;
	};

	// basisu_transcoder_init() MUST be called before a .basis file can be transcoded.
	void basisu_transcoder_init();
		
	enum debug_flags_t
	{
		cDebugFlagVisCRs = 1,
		cDebugFlagVisBC1Sels = 2,
		cDebugFlagVisBC1Endpoints = 4
	};
	uint32_t get_debug_flags();
	void set_debug_flags(uint32_t f);

	// ------------------------------------------------------------------------------------------------------ 
	// Optional .KTX2 file format support
	// KTX2 reading optionally requires miniz or Zstd decompressors for supercompressed UASTC files.
	// ------------------------------------------------------------------------------------------------------ 
#if BASISD_SUPPORT_KTX2
#pragma pack(push)
#pragma pack(1)
	struct ktx2_header
	{
		uint8_t m_identifier[12];
		basisu::packed_uint<4> m_vk_format;
		basisu::packed_uint<4> m_type_size;
		basisu::packed_uint<4> m_pixel_width;
		basisu::packed_uint<4> m_pixel_height;
		basisu::packed_uint<4> m_pixel_depth;
		basisu::packed_uint<4> m_layer_count;
		basisu::packed_uint<4> m_face_count;
		basisu::packed_uint<4> m_level_count;
		basisu::packed_uint<4> m_supercompression_scheme;
		basisu::packed_uint<4> m_dfd_byte_offset;
		basisu::packed_uint<4> m_dfd_byte_length;
		basisu::packed_uint<4> m_kvd_byte_offset;
		basisu::packed_uint<4> m_kvd_byte_length;
		basisu::packed_uint<8> m_sgd_byte_offset;
		basisu::packed_uint<8> m_sgd_byte_length;
	};

	struct ktx2_level_index
	{
		basisu::packed_uint<8> m_byte_offset;
		basisu::packed_uint<8> m_byte_length;
		basisu::packed_uint<8> m_uncompressed_byte_length;
	};

	struct ktx2_etc1s_global_data_header
	{
		basisu::packed_uint<2> m_endpoint_count;
		basisu::packed_uint<2> m_selector_count;
		basisu::packed_uint<4> m_endpoints_byte_length;
		basisu::packed_uint<4> m_selectors_byte_length;
		basisu::packed_uint<4> m_tables_byte_length;
		basisu::packed_uint<4> m_extended_byte_length;
	};

	struct ktx2_etc1s_image_desc
	{
		basisu::packed_uint<4> m_image_flags;
		basisu::packed_uint<4> m_rgb_slice_byte_offset;
		basisu::packed_uint<4> m_rgb_slice_byte_length;
		basisu::packed_uint<4> m_alpha_slice_byte_offset;
		basisu::packed_uint<4> m_alpha_slice_byte_length;
	};

	struct ktx2_animdata
	{
		basisu::packed_uint<4> m_duration;
		basisu::packed_uint<4> m_timescale;
		basisu::packed_uint<4> m_loopcount;
	};
#pragma pack(pop)

	const uint32_t KTX2_VK_FORMAT_UNDEFINED = 0;
	const uint32_t KTX2_FORMAT_UASTC_4x4_SFLOAT_BLOCK = 1000066000; // TODO, is this correct?
	const uint32_t KTX2_KDF_DF_MODEL_UASTC = 166;
	const uint32_t KTX2_KDF_DF_MODEL_UASTC_HDR = 167;
	const uint32_t KTX2_KDF_DF_MODEL_ETC1S = 163;
	const uint32_t KTX2_IMAGE_IS_P_FRAME = 2;
	const uint32_t KTX2_UASTC_BLOCK_SIZE = 16; // also the block size for UASTC_HDR
	const uint32_t KTX2_MAX_SUPPORTED_LEVEL_COUNT = 16; // this is an implementation specific constraint and can be increased

	// The KTX2 transfer functions supported by KTX2
	const uint32_t KTX2_KHR_DF_TRANSFER_LINEAR = 1;
	const uint32_t KTX2_KHR_DF_TRANSFER_SRGB = 2;

	enum ktx2_supercompression
	{
		KTX2_SS_NONE = 0,
		KTX2_SS_BASISLZ = 1,
		KTX2_SS_ZSTANDARD = 2
	};

	extern const uint8_t g_ktx2_file_identifier[12];

	enum ktx2_df_channel_id
	{
		KTX2_DF_CHANNEL_ETC1S_RGB = 0U,
		KTX2_DF_CHANNEL_ETC1S_RRR = 3U,
		KTX2_DF_CHANNEL_ETC1S_GGG = 4U,
		KTX2_DF_CHANNEL_ETC1S_AAA = 15U,

		KTX2_DF_CHANNEL_UASTC_DATA = 0U,
		KTX2_DF_CHANNEL_UASTC_RGB = 0U,
		KTX2_DF_CHANNEL_UASTC_RGBA = 3U,
		KTX2_DF_CHANNEL_UASTC_RRR = 4U,
		KTX2_DF_CHANNEL_UASTC_RRRG = 5U,
		KTX2_DF_CHANNEL_UASTC_RG = 6U,
	};

	inline const char* ktx2_get_etc1s_df_channel_id_str(ktx2_df_channel_id id)
	{
		switch (id)
		{
		case KTX2_DF_CHANNEL_ETC1S_RGB: return "RGB";
		case KTX2_DF_CHANNEL_ETC1S_RRR: return "RRR";
		case KTX2_DF_CHANNEL_ETC1S_GGG: return "GGG";
		case KTX2_DF_CHANNEL_ETC1S_AAA: return "AAA";
		default: break;
		}
		return "?";
	}

	inline const char* ktx2_get_uastc_df_channel_id_str(ktx2_df_channel_id id)
	{
		switch (id)
		{
		case KTX2_DF_CHANNEL_UASTC_RGB: return "RGB";
		case KTX2_DF_CHANNEL_UASTC_RGBA: return "RGBA";
		case KTX2_DF_CHANNEL_UASTC_RRR: return "RRR";
		case KTX2_DF_CHANNEL_UASTC_RRRG: return "RRRG";
		case KTX2_DF_CHANNEL_UASTC_RG: return "RG";
		default: break;
		}
		return "?";
	}

	enum ktx2_df_color_primaries
	{
		KTX2_DF_PRIMARIES_UNSPECIFIED = 0,
		KTX2_DF_PRIMARIES_BT709 = 1,
		KTX2_DF_PRIMARIES_SRGB = 1,
		KTX2_DF_PRIMARIES_BT601_EBU = 2,
		KTX2_DF_PRIMARIES_BT601_SMPTE = 3,
		KTX2_DF_PRIMARIES_BT2020 = 4,
		KTX2_DF_PRIMARIES_CIEXYZ = 5,
		KTX2_DF_PRIMARIES_ACES = 6,
		KTX2_DF_PRIMARIES_ACESCC = 7,
		KTX2_DF_PRIMARIES_NTSC1953 = 8,
		KTX2_DF_PRIMARIES_PAL525 = 9,
		KTX2_DF_PRIMARIES_DISPLAYP3 = 10,
		KTX2_DF_PRIMARIES_ADOBERGB = 11
	};

	inline const char* ktx2_get_df_color_primaries_str(ktx2_df_color_primaries p)
	{
		switch (p)
		{
		case KTX2_DF_PRIMARIES_UNSPECIFIED: return "UNSPECIFIED";
		case KTX2_DF_PRIMARIES_BT709: return "BT709";
		case KTX2_DF_PRIMARIES_BT601_EBU: return "EBU"; 
		case KTX2_DF_PRIMARIES_BT601_SMPTE: return "SMPTE";
		case KTX2_DF_PRIMARIES_BT2020: return "BT2020";
		case KTX2_DF_PRIMARIES_CIEXYZ: return "CIEXYZ";
		case KTX2_DF_PRIMARIES_ACES: return "ACES";
		case KTX2_DF_PRIMARIES_ACESCC: return "ACESCC"; 
		case KTX2_DF_PRIMARIES_NTSC1953: return "NTSC1953";
		case KTX2_DF_PRIMARIES_PAL525: return "PAL525";
		case KTX2_DF_PRIMARIES_DISPLAYP3: return "DISPLAYP3";
		case KTX2_DF_PRIMARIES_ADOBERGB: return "ADOBERGB";
		default: break;
		}
		return "?";
	}	

	// Information about a single 2D texture "image" in a KTX2 file.
	struct ktx2_image_level_info
	{
		// The mipmap level index (0=largest), texture array layer index, and cubemap face index of the image.
		uint32_t m_level_index;
		uint32_t m_layer_index;
		uint32_t m_face_index;

		// The image's actual (or the original source image's) width/height in pixels, which may not be divisible by 4 pixels.
		uint32_t m_orig_width;
		uint32_t m_orig_height;

		// The image's physical width/height, which will always be divisible by 4 pixels.
		uint32_t m_width;
		uint32_t m_height;

		// The texture's dimensions in 4x4 texel blocks.
		uint32_t m_num_blocks_x;
		uint32_t m_num_blocks_y;

		// The total number of blocks
		uint32_t m_total_blocks;

		// true if the image has alpha data
		bool m_alpha_flag;

		// true if the image is an I-Frame. Currently, for ETC1S textures, the first frame will always be an I-Frame, and subsequent frames will always be P-Frames.
		bool m_iframe_flag;
	};
		
	// Thread-specific ETC1S/supercompressed UASTC transcoder state. (If you're not doing multithreading transcoding you can ignore this.)
	struct ktx2_transcoder_state
	{
		basist::basisu_transcoder_state m_transcoder_state;
		basisu::uint8_vec m_level_uncomp_data;
		int m_uncomp_data_level_index;

		void clear()
		{
			m_transcoder_state.clear();
			m_level_uncomp_data.clear();
			m_uncomp_data_level_index = -1;
		}
	};

	// This class is quite similar to basisu_transcoder. It treats KTX2 files as a simple container for ETC1S/UASTC texture data.
	// It does not support 1D or 3D textures.
	// It only supports 2D and cubemap textures, with or without mipmaps, texture arrays of 2D/cubemap textures, and texture video files. 
	// It only supports raw non-supercompressed UASTC, ETC1S, UASTC+Zstd, or UASTC+zlib compressed files.
	// DFD (Data Format Descriptor) parsing is purposely as simple as possible. 
	// If you need to know how to interpret the texture channels you'll need to parse the DFD yourself after calling get_dfd().
	class ktx2_transcoder
	{
	public:
		ktx2_transcoder();

		// Frees all allocations, resets object.
		void clear();

		// init() parses the KTX2 header, level index array, DFD, and key values, but nothing else.
		// Importantly, it does not parse or decompress the ETC1S global supercompressed data, so some things (like which frames are I/P-Frames) won't be available until start_transcoding() is called.
		// This method holds a pointer to the file data until clear() is called.
		bool init(const void* pData, uint32_t data_size);

		// Returns the data/size passed to init().
		const uint8_t* get_data() const { return m_pData; }
		uint32_t get_data_size() const { return m_data_size; }

		// Returns the KTX2 header. Valid after init().
		const ktx2_header& get_header() const { return m_header; }

		// Returns the KTX2 level index array. There will be one entry for each mipmap level. Valid after init().
		const basisu::vector<ktx2_level_index>& get_level_index() const { return m_levels; }

		// Returns the texture's width in texels. Always non-zero, might not be divisible by 4. Valid after init().
		uint32_t get_width() const { return m_header.m_pixel_width; }

		// Returns the texture's height in texels. Always non-zero, might not be divisible by 4. Valid after init().
		uint32_t get_height() const { return m_header.m_pixel_height; }

		// Returns the texture's number of mipmap levels. Always returns 1 or higher. Valid after init().
		uint32_t get_levels() const { return m_header.m_level_count; }

		// Returns the number of faces. Returns 1 for 2D textures and or 6 for cubemaps. Valid after init().
		uint32_t get_faces() const { return m_header.m_face_count; }

		// Returns 0 or the number of layers in the texture array or texture video. Valid after init().
		uint32_t get_layers() const { return m_header.m_layer_count; }

		// Returns cETC1S, cUASTC4x4, or cUASTC_HDR_4x4. Valid after init().
		basist::basis_tex_format get_format() const { return m_format; } 
				
		bool is_etc1s() const { return get_format() == basist::basis_tex_format::cETC1S; }

		bool is_uastc() const { return get_format() == basist::basis_tex_format::cUASTC4x4; }

		bool is_hdr() const { return get_format() == basist::basis_tex_format::cUASTC_HDR_4x4; }

		// Returns true if the ETC1S file has two planes (typically RGBA, or RRRG), or true if the UASTC file has alpha data. Valid after init().
		uint32_t get_has_alpha() const { return m_has_alpha; }

		// Returns the entire Data Format Descriptor (DFD) from the KTX2 file. Valid after init().
		// See https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.html#_the_khronos_data_format_descriptor_overview
		const basisu::uint8_vec& get_dfd() const { return m_dfd; }

		// Some basic DFD accessors. Valid after init().
		uint32_t get_dfd_color_model() const { return m_dfd_color_model; }

		// Returns the DFD color primary.
		// We do not validate the color primaries, so the returned value may not be in the ktx2_df_color_primaries enum.
		ktx2_df_color_primaries get_dfd_color_primaries() const { return m_dfd_color_prims; }
		
		// Returns KTX2_KHR_DF_TRANSFER_LINEAR or KTX2_KHR_DF_TRANSFER_SRGB.
		uint32_t get_dfd_transfer_func() const { return m_dfd_transfer_func; }

		uint32_t get_dfd_flags() const { return m_dfd_flags; }

		// Returns 1 (ETC1S/UASTC) or 2 (ETC1S with an internal alpha channel).
		uint32_t get_dfd_total_samples() const { return m_dfd_samples;	}
		
		// Returns the channel mapping for each DFD "sample". UASTC always has 1 sample, ETC1S can have one or two. 
		// Note the returned value SHOULD be one of the ktx2_df_channel_id enums, but we don't validate that. 
		// It's up to the caller to decide what to do if the value isn't in the enum.
		ktx2_df_channel_id get_dfd_channel_id0() const { return m_dfd_chan0; }
		ktx2_df_channel_id get_dfd_channel_id1() const { return m_dfd_chan1; }

		// Key value field data.
		struct key_value
		{
			// The key field is UTF8 and always zero terminated.
			basisu::uint8_vec m_key;

			// The value may be empty. It consists of raw bytes which may or may not be zero terminated.
			basisu::uint8_vec m_value;

			bool operator< (const key_value& rhs) const { return strcmp((const char*)m_key.data(), (const char *)rhs.m_key.data()) < 0; }
		};
		typedef basisu::vector<key_value> key_value_vec;

		// Returns the array of key-value entries. This may be empty. Valid after init().
		// The order of key values fields in this array exactly matches the order they were stored in the file. The keys are supposed to be sorted by their Unicode code points.
		const key_value_vec& get_key_values() const { return m_key_values; }

		const basisu::uint8_vec *find_key(const std::string& key_name) const;

		// Low-level ETC1S specific accessors

		// Returns the ETC1S global supercompression data header, which is only valid after start_transcoding() is called.
		const ktx2_etc1s_global_data_header& get_etc1s_header() const { return m_etc1s_header; }

		// Returns the array of ETC1S image descriptors, which is only valid after get_etc1s_image_descs() is called.
		const basisu::vector<ktx2_etc1s_image_desc>& get_etc1s_image_descs() const { return m_etc1s_image_descs; }

		// Must have called startTranscoding() first
		uint32_t get_etc1s_image_descs_image_flags(uint32_t level_index, uint32_t layer_index, uint32_t face_index) const;

		// is_video() is only valid after start_transcoding() is called.
		// For ETC1S data, if this returns true you must currently transcode the file from first to last frame, in order, without skipping any frames.
		bool is_video() const { return m_is_video; }
				
		// start_transcoding() MUST be called before calling transcode_image().
		// This method decompresses the ETC1S global endpoint/selector codebooks, which is not free, so try to avoid calling it excessively.
		bool start_transcoding();
								
		// get_image_level_info() be called after init(), but the m_iframe_flag's won't be valid until start_transcoding() is called.
		// You can call this method before calling transcode_image_level() to retrieve basic information about the mipmap level's dimensions, etc.
		bool get_image_level_info(ktx2_image_level_info& level_info, uint32_t level_index, uint32_t layer_index, uint32_t face_index) const;

		// transcode_image_level() transcodes a single 2D texture or cubemap face from the KTX2 file.
		// Internally it uses the same low-level transcode API's as basisu_transcoder::transcode_image_level().
		// If the file is UASTC and is supercompressed with Zstandard, and the file is a texture array or cubemap, it's highly recommended that each mipmap level is 
		// completely transcoded before switching to another level. Every time the mipmap level is changed all supercompressed level data must be decompressed using Zstandard as a single unit.
		// Currently ETC1S videos must always be transcoded from first to last frame (or KTX2 "layer"), in order, with no skipping of frames.
		// By default this method is not thread safe unless you specify a pointer to a user allocated thread-specific transcoder_state struct.
		bool transcode_image_level(
			uint32_t level_index, uint32_t layer_index, uint32_t face_index,
			void* pOutput_blocks, uint32_t output_blocks_buf_size_in_blocks_or_pixels,
			basist::transcoder_texture_format fmt,
			uint32_t decode_flags = 0, uint32_t output_row_pitch_in_blocks_or_pixels = 0, uint32_t output_rows_in_pixels = 0, int channel0 = -1, int channel1 = -1,
			ktx2_transcoder_state *pState = nullptr);
				
	private:
		const uint8_t* m_pData;
		uint32_t m_data_size;

		ktx2_header m_header;
		basisu::vector<ktx2_level_index> m_levels;
		basisu::uint8_vec m_dfd;
		key_value_vec m_key_values;
		
		ktx2_etc1s_global_data_header m_etc1s_header;
		basisu::vector<ktx2_etc1s_image_desc> m_etc1s_image_descs;

		basist::basis_tex_format m_format;
					
		uint32_t m_dfd_color_model;
		ktx2_df_color_primaries m_dfd_color_prims;
		uint32_t m_dfd_transfer_func;
		uint32_t m_dfd_flags;
		uint32_t m_dfd_samples;
		ktx2_df_channel_id m_dfd_chan0, m_dfd_chan1;
								
		basist::basisu_lowlevel_etc1s_transcoder m_etc1s_transcoder;
		basist::basisu_lowlevel_uastc_transcoder m_uastc_transcoder;
		basist::basisu_lowlevel_uastc_hdr_transcoder m_uastc_hdr_transcoder;
				
		ktx2_transcoder_state m_def_transcoder_state;

		bool m_has_alpha;
		bool m_is_video;

		bool decompress_level_data(uint32_t level_index, basisu::uint8_vec& uncomp_data);
		bool decompress_etc1s_global_data();
		bool read_key_values();
	};

#endif // BASISD_SUPPORT_KTX2

	// Returns true if the transcoder was compiled with KTX2 support.
	bool basisu_transcoder_supports_ktx2();

	// Returns true if the transcoder was compiled with Zstandard support.
	bool basisu_transcoder_supports_ktx2_zstd();

} // namespace basisu

