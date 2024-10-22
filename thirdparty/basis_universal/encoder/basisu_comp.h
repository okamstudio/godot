// basisu_comp.h
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
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
#include "basisu_frontend.h"
#include "basisu_backend.h"
#include "basisu_basis_file.h"
#include "../transcoder/basisu_transcoder.h"
#include "basisu_uastc_enc.h"
#include "basisu_astc_hdr_enc.h"

#define BASISU_LIB_VERSION 150
#define BASISU_LIB_VERSION_STRING "1.50"

#ifndef BASISD_SUPPORT_KTX2
	#error BASISD_SUPPORT_KTX2 is undefined
#endif
#ifndef BASISD_SUPPORT_KTX2_ZSTD
	#error BASISD_SUPPORT_KTX2_ZSTD is undefined
#endif

#if !BASISD_SUPPORT_KTX2
	#error BASISD_SUPPORT_KTX2 must be enabled when building the encoder. To reduce code size if KTX2 support is not needed, set BASISD_SUPPORT_KTX2_ZSTD to 0
#endif

namespace basisu
{
	struct opencl_context;
	typedef opencl_context* opencl_context_ptr;

	const uint32_t BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION = 16384;

	// Allow block's color distance to increase by 1.5 while searching for an alternative nearby endpoint.
	const float BASISU_DEFAULT_ENDPOINT_RDO_THRESH = 1.5f; 
	
	// Allow block's color distance to increase by 1.25 while searching the selector history buffer for a close enough match.
	const float BASISU_DEFAULT_SELECTOR_RDO_THRESH = 1.25f; 

	const int BASISU_DEFAULT_QUALITY = 128;
	const float BASISU_DEFAULT_HYBRID_SEL_CB_QUALITY_THRESH = 2.0f;

	const uint32_t BASISU_MAX_IMAGE_DIMENSION = 16384;
	const uint32_t BASISU_QUALITY_MIN = 1;
	const uint32_t BASISU_QUALITY_MAX = 255;
	const uint32_t BASISU_MAX_ENDPOINT_CLUSTERS = basisu_frontend::cMaxEndpointClusters;
	const uint32_t BASISU_MAX_SELECTOR_CLUSTERS = basisu_frontend::cMaxSelectorClusters;

	const uint32_t BASISU_MAX_SLICES = 0xFFFFFF;

	const int BASISU_RDO_UASTC_DICT_SIZE_DEFAULT = 4096; // 32768;
	const int BASISU_RDO_UASTC_DICT_SIZE_MIN = 64;
	const int BASISU_RDO_UASTC_DICT_SIZE_MAX = 65536;

	struct image_stats
	{
		image_stats()
		{
			clear();
		}

		void clear()
		{
			m_filename.clear();
			m_width = 0;
			m_height = 0;
						
			m_basis_rgb_avg_psnr = 0.0f;
			m_basis_rgba_avg_psnr = 0.0f;
			m_basis_a_avg_psnr = 0.0f;
			m_basis_luma_709_psnr = 0.0f;
			m_basis_luma_601_psnr = 0.0f;
			m_basis_luma_709_ssim = 0.0f;

			m_basis_rgb_avg_bc6h_psnr = 0.0f;

			m_bc7_rgb_avg_psnr = 0.0f;
			m_bc7_rgba_avg_psnr = 0.0f;
			m_bc7_a_avg_psnr = 0.0f;
			m_bc7_luma_709_psnr = 0.0f;
			m_bc7_luma_601_psnr = 0.0f;
			m_bc7_luma_709_ssim = 0.0f;
						
			m_best_etc1s_rgb_avg_psnr = 0.0f;
			m_best_etc1s_luma_709_psnr = 0.0f;
			m_best_etc1s_luma_601_psnr = 0.0f;
			m_best_etc1s_luma_709_ssim = 0.0f;

			m_opencl_failed = false;
		}

		std::string m_filename;
		uint32_t m_width;
		uint32_t m_height;

		// .basis/.ktx2 compressed (LDR: ETC1S or UASTC statistics, HDR: transcoded BC6H statistics)
		float m_basis_rgb_avg_psnr;
		float m_basis_rgba_avg_psnr;
		float m_basis_a_avg_psnr;
		float m_basis_luma_709_psnr;
		float m_basis_luma_601_psnr;
		float m_basis_luma_709_ssim;

		// UASTC HDR only.
		float m_basis_rgb_avg_bc6h_psnr;

		// LDR: BC7 statistics
		float m_bc7_rgb_avg_psnr;
		float m_bc7_rgba_avg_psnr;
		float m_bc7_a_avg_psnr;
		float m_bc7_luma_709_psnr;
		float m_bc7_luma_601_psnr;
		float m_bc7_luma_709_ssim;
		
		// LDR: Highest achievable quality ETC1S statistics
		float m_best_etc1s_rgb_avg_psnr;
		float m_best_etc1s_luma_709_psnr;
		float m_best_etc1s_luma_601_psnr;
		float m_best_etc1s_luma_709_ssim;

		bool m_opencl_failed;
	};

	template<bool def>
	struct bool_param
	{
		bool_param() :
			m_value(def),
			m_changed(false)
		{
		}

		void clear()
		{
			m_value = def;
			m_changed = false;
		}

		operator bool() const
		{
			return m_value;
		}

		bool operator= (bool v)
		{
			m_value = v;
			m_changed = true;
			return m_value;
		}

		bool was_changed() const { return m_changed; }
		void set_changed(bool flag) { m_changed = flag; }

		bool m_value;
		bool m_changed;
	};

	template<typename T>
	struct param
	{
		param(T def, T min_v, T max_v) :
			m_value(def),
			m_def(def),
			m_min(min_v),
			m_max(max_v),
			m_changed(false)
		{
		}

		void clear()
		{
			m_value = m_def;
			m_changed = false;
		}

		operator T() const
		{
			return m_value;
		}

		T operator= (T v)
		{
			m_value = clamp<T>(v, m_min, m_max);
			m_changed = true;
			return m_value;
		}

		T operator *= (T v)
		{
			m_value *= v;
			m_changed = true;
			return m_value;
		}

		bool was_changed() const { return m_changed; }
		void set_changed(bool flag) { m_changed = flag; }

		T m_value;
		T m_def;
		T m_min;
		T m_max;
		bool m_changed;
	};

	struct basis_compressor_params
	{
		basis_compressor_params() :
			m_compression_level((int)BASISU_DEFAULT_COMPRESSION_LEVEL, 0, (int)BASISU_MAX_COMPRESSION_LEVEL),
			m_selector_rdo_thresh(BASISU_DEFAULT_SELECTOR_RDO_THRESH, 0.0f, 1e+10f),
			m_endpoint_rdo_thresh(BASISU_DEFAULT_ENDPOINT_RDO_THRESH, 0.0f, 1e+10f),
			m_mip_scale(1.0f, .000125f, 4.0f),
			m_mip_smallest_dimension(1, 1, 16384),
			m_max_endpoint_clusters(512),
			m_max_selector_clusters(512),
			m_quality_level(-1),
			m_pack_uastc_flags(cPackUASTCLevelDefault),
			m_rdo_uastc_quality_scalar(1.0f, 0.001f, 50.0f),
			m_rdo_uastc_dict_size(BASISU_RDO_UASTC_DICT_SIZE_DEFAULT, BASISU_RDO_UASTC_DICT_SIZE_MIN, BASISU_RDO_UASTC_DICT_SIZE_MAX),
			m_rdo_uastc_max_smooth_block_error_scale(UASTC_RDO_DEFAULT_SMOOTH_BLOCK_MAX_ERROR_SCALE, 1.0f, 300.0f),
			m_rdo_uastc_smooth_block_max_std_dev(UASTC_RDO_DEFAULT_MAX_SMOOTH_BLOCK_STD_DEV, .01f, 65536.0f),
			m_rdo_uastc_max_allowed_rms_increase_ratio(UASTC_RDO_DEFAULT_MAX_ALLOWED_RMS_INCREASE_RATIO, .01f, 100.0f),
			m_rdo_uastc_skip_block_rms_thresh(UASTC_RDO_DEFAULT_SKIP_BLOCK_RMS_THRESH, .01f, 100.0f),
			m_resample_width(0, 1, 16384),
			m_resample_height(0, 1, 16384),
			m_resample_factor(0.0f, .00125f, 100.0f),
			m_ktx2_uastc_supercompression(basist::KTX2_SS_NONE),
			m_ktx2_zstd_supercompression_level(6, INT_MIN, INT_MAX),
			m_pJob_pool(nullptr)
		{
			clear();
		}

		void clear()
		{
			m_uastc.clear();
			m_use_opencl.clear();
			m_status_output.clear();

			m_source_filenames.clear();
			m_source_alpha_filenames.clear();

			m_source_images.clear();
			m_source_mipmap_images.clear();

			m_out_filename.clear();

			m_y_flip.clear();
			m_debug.clear();
			m_validate_etc1s.clear();
			m_debug_images.clear();
			m_perceptual.clear();
			m_no_selector_rdo.clear();
			m_selector_rdo_thresh.clear();
			m_read_source_images.clear();
			m_write_output_basis_or_ktx2_files.clear();
			m_compression_level.clear();
			m_compute_stats.clear();
			m_print_stats.clear();
			m_check_for_alpha.clear();
			m_force_alpha.clear();
			m_multithreading.clear();
			m_swizzle[0] = 0;
			m_swizzle[1] = 1;
			m_swizzle[2] = 2;
			m_swizzle[3] = 3;
			m_renormalize.clear();
			m_disable_hierarchical_endpoint_codebooks.clear();

			m_no_endpoint_rdo.clear();
			m_endpoint_rdo_thresh.clear();
						
			m_mip_gen.clear();
			m_mip_scale.clear();
			m_mip_filter = "kaiser";
			m_mip_scale = 1.0f;
			m_mip_srgb.clear();
			m_mip_premultiplied.clear();
			m_mip_renormalize.clear();
			m_mip_wrapping.clear();
			m_mip_fast.clear();
			m_mip_smallest_dimension.clear();

			m_max_endpoint_clusters = 0;
			m_max_selector_clusters = 0;
			m_quality_level = -1;

			m_tex_type = basist::cBASISTexType2D;
			m_userdata0 = 0;
			m_userdata1 = 0;
			m_us_per_frame = 0;

			m_pack_uastc_flags = cPackUASTCLevelDefault;
			m_rdo_uastc.clear();
			m_rdo_uastc_quality_scalar.clear();
			m_rdo_uastc_max_smooth_block_error_scale.clear();
			m_rdo_uastc_smooth_block_max_std_dev.clear();
			m_rdo_uastc_max_allowed_rms_increase_ratio.clear();
			m_rdo_uastc_skip_block_rms_thresh.clear();
			m_rdo_uastc_favor_simpler_modes_in_rdo_mode.clear();
			m_rdo_uastc_multithreading.clear();

			m_resample_width.clear();
			m_resample_height.clear();
			m_resample_factor.clear();

			m_pGlobal_codebooks = nullptr;

			m_create_ktx2_file.clear();
			m_ktx2_uastc_supercompression = basist::KTX2_SS_NONE;
			m_ktx2_key_values.clear();
			m_ktx2_zstd_supercompression_level.clear();
			m_ktx2_srgb_transfer_func.clear();

			m_validate_output_data.clear();

			m_hdr_ldr_srgb_to_linear_conversion.clear();

			m_hdr_favor_astc.clear();
			
			m_pJob_pool = nullptr;
		}
						
		// True to generate UASTC .basis/.KTX2 file data, otherwise ETC1S.
		bool_param<false> m_uastc;

		// Set m_hdr to true to switch to UASTC HDR mode.
		bool_param<false> m_hdr;

		bool_param<false> m_use_opencl;

		// If m_read_source_images is true, m_source_filenames (and optionally m_source_alpha_filenames) contains the filenames of PNG etc. images to read. 
		// Otherwise, the compressor processes the images in m_source_images or m_source_images_hdr.
		basisu::vector<std::string> m_source_filenames;
		basisu::vector<std::string> m_source_alpha_filenames;
		
		basisu::vector<image> m_source_images;
		
		basisu::vector<imagef> m_source_images_hdr;
				
		// Stores mipmaps starting from level 1. Level 0 is still stored in m_source_images, as usual.
		// If m_source_mipmaps isn't empty, automatic mipmap generation isn't done. m_source_mipmaps.size() MUST equal m_source_images.size() or the compressor returns an error.
		// The compressor applies the user-provided swizzling (in m_swizzle) to these images.
		basisu::vector< basisu::vector<image> > m_source_mipmap_images;

		basisu::vector< basisu::vector<imagef> > m_source_mipmap_images_hdr;
						
		// Filename of the output basis/ktx2 file
		std::string m_out_filename;

		// The params are done this way so we can detect when the user has explictly changed them.

		// Flip images across Y axis
		bool_param<false> m_y_flip;

		// If true, the compressor will print basis status to stdout during compression.
		bool_param<true> m_status_output;
		
		// Output debug information during compression
		bool_param<false> m_debug;
		bool_param<false> m_validate_etc1s;
		
		// m_debug_images is pretty slow
		bool_param<false> m_debug_images;

		// ETC1S compression level, from 0 to BASISU_MAX_COMPRESSION_LEVEL (higher is slower). 
		// This parameter controls numerous internal encoding speed vs. compression efficiency/performance tradeoffs.
		// Note this is NOT the same as the ETC1S quality level, and most users shouldn't change this.
		param<int> m_compression_level;
						
		// Use perceptual sRGB colorspace metrics instead of linear
		bool_param<true> m_perceptual;

		// Disable selector RDO, for faster compression but larger files
		bool_param<false> m_no_selector_rdo;
		param<float> m_selector_rdo_thresh;

		bool_param<false> m_no_endpoint_rdo;
		param<float> m_endpoint_rdo_thresh;

		// Read source images from m_source_filenames/m_source_alpha_filenames
		bool_param<false> m_read_source_images;

		// Write the output basis/ktx2 file to disk using m_out_filename
		bool_param<false> m_write_output_basis_or_ktx2_files;
								
		// Compute and display image metrics 
		bool_param<false> m_compute_stats;

		// Print stats to stdout, if m_compute_stats is true.
		bool_param<true> m_print_stats;
		
		// Check to see if any input image has an alpha channel, if so then the output basis/ktx2 file will have alpha channels
		bool_param<true> m_check_for_alpha;
		
		// Always put alpha slices in the output basis/ktx2 file, even when the input doesn't have alpha
		bool_param<false> m_force_alpha; 
		bool_param<true> m_multithreading;
		
		// Split the R channel to RGB and the G channel to alpha, then write a basis/ktx2 file with alpha channels
		uint8_t m_swizzle[4];

		bool_param<false> m_renormalize;

		// If true the front end will not use 2 level endpoint codebook searching, for slightly higher quality but much slower execution.
		// Note some m_compression_level's disable this automatically.
		bool_param<false> m_disable_hierarchical_endpoint_codebooks;
						
		// mipmap generation parameters
		bool_param<false> m_mip_gen;
		param<float> m_mip_scale;
		std::string m_mip_filter;
		bool_param<false> m_mip_srgb;
		bool_param<true> m_mip_premultiplied; // not currently supported
		bool_param<false> m_mip_renormalize; 
		bool_param<true> m_mip_wrapping;
		bool_param<true> m_mip_fast;
		param<int> m_mip_smallest_dimension;
						
		// Codebook size (quality) control. 
		// If m_quality_level != -1, it controls the quality level. It ranges from [1,255] or [BASISU_QUALITY_MIN, BASISU_QUALITY_MAX].
		// Otherwise m_max_endpoint_clusters/m_max_selector_clusters controls the codebook sizes directly.
		uint32_t m_max_endpoint_clusters;
		uint32_t m_max_selector_clusters;
		int m_quality_level;
		
		// m_tex_type, m_userdata0, m_userdata1, m_framerate - These fields go directly into the Basis file header.
		basist::basis_texture_type m_tex_type;
		uint32_t m_userdata0;
		uint32_t m_userdata1;
		uint32_t m_us_per_frame;

		// cPackUASTCLevelDefault, etc.
		uint32_t m_pack_uastc_flags;
		bool_param<false> m_rdo_uastc;
		param<float> m_rdo_uastc_quality_scalar;
		param<int> m_rdo_uastc_dict_size;
		param<float> m_rdo_uastc_max_smooth_block_error_scale;
		param<float> m_rdo_uastc_smooth_block_max_std_dev;
		param<float> m_rdo_uastc_max_allowed_rms_increase_ratio;
		param<float> m_rdo_uastc_skip_block_rms_thresh;
		bool_param<true> m_rdo_uastc_favor_simpler_modes_in_rdo_mode;
		bool_param<true> m_rdo_uastc_multithreading;

		param<int> m_resample_width;
		param<int> m_resample_height;
		param<float> m_resample_factor;

		const basist::basisu_lowlevel_etc1s_transcoder *m_pGlobal_codebooks;

		// KTX2 specific parameters.
		// Internally, the compressor always creates a .basis file then it converts that lossless to KTX2.
		bool_param<false> m_create_ktx2_file;
		basist::ktx2_supercompression m_ktx2_uastc_supercompression;
		basist::ktx2_transcoder::key_value_vec m_ktx2_key_values;
		param<int> m_ktx2_zstd_supercompression_level;
		bool_param<false> m_ktx2_srgb_transfer_func;

		astc_hdr_codec_options m_uastc_hdr_options;

		bool_param<false> m_validate_output_data;

		// If true, LDR images (such as PNG) will be converted to normalized [0,1] linear light (via a sRGB->Linear conversion) and then processed as HDR. 
		// Otherwise, LDR images will be processed as HDR as-is.
		bool_param<true> m_hdr_ldr_srgb_to_linear_conversion;

		// If true, ASTC HDR quality is favored more than BC6H quality. Otherwise it's a rough balance.
		bool_param<false> m_hdr_favor_astc;
						
		job_pool *m_pJob_pool;
	};

	// Important: basisu_encoder_init() MUST be called first before using this class.
	class basis_compressor
	{
		BASISU_NO_EQUALS_OR_COPY_CONSTRUCT(basis_compressor);

	public:
		basis_compressor();
		~basis_compressor();

		// Note it *should* be possible to call init() multiple times with different inputs, but this scenario isn't well tested. Ideally, create 1 object, compress, then delete it.
		bool init(const basis_compressor_params &params);
		
		enum error_code
		{
			cECSuccess = 0,
			cECFailedInitializing,
			cECFailedReadingSourceImages,
			cECFailedValidating,
			cECFailedEncodeUASTC,
			cECFailedFrontEnd,
			cECFailedFontendExtract,
			cECFailedBackend,
			cECFailedCreateBasisFile,
			cECFailedWritingOutput,
			cECFailedUASTCRDOPostProcess,
			cECFailedCreateKTX2File
		};

		error_code process();

		// The output .basis file will always be valid of process() succeeded.
		const uint8_vec &get_output_basis_file() const { return m_output_basis_file; }
		
		// The output .ktx2 file will only be valid if m_create_ktx2_file was true and process() succeeded.
		const uint8_vec& get_output_ktx2_file() const { return m_output_ktx2_file; }

		const basisu::vector<image_stats> &get_stats() const { return m_stats; }

		uint32_t get_basis_file_size() const { return m_basis_file_size; }
		double get_basis_bits_per_texel() const { return m_basis_bits_per_texel; }
		
		bool get_any_source_image_has_alpha() const { return m_any_source_image_has_alpha; }

		bool get_opencl_failed() const { return m_opencl_failed; }
								
	private:
		basis_compressor_params m_params;

		opencl_context_ptr m_pOpenCL_context;
		
		basisu::vector<image> m_slice_images;
		basisu::vector<imagef> m_slice_images_hdr;

		basisu::vector<image_stats> m_stats;

		uint32_t m_basis_file_size;
		double m_basis_bits_per_texel;
						
		basisu_backend_slice_desc_vec m_slice_descs;

		uint32_t m_total_blocks;
		
		basisu_frontend m_frontend;

		pixel_block_vec m_source_blocks;
		pixel_block_hdr_vec m_source_blocks_hdr;

		basisu::vector<gpu_image> m_frontend_output_textures;

		basisu::vector<gpu_image> m_best_etc1s_images;
		basisu::vector<image> m_best_etc1s_images_unpacked;

		basisu_backend m_backend;

		basisu_file m_basis_file;

		basisu::vector<gpu_image> m_decoded_output_textures;			// BC6H in HDR mode
		basisu::vector<image> m_decoded_output_textures_unpacked;
		
		basisu::vector<gpu_image> m_decoded_output_textures_bc7;
		basisu::vector<image> m_decoded_output_textures_unpacked_bc7;

		basisu::vector<imagef> m_decoded_output_textures_bc6h_hdr_unpacked;	// BC6H in HDR mode

		basisu::vector<gpu_image> m_decoded_output_textures_astc_hdr;
		basisu::vector<imagef> m_decoded_output_textures_astc_hdr_unpacked;

		uint8_vec m_output_basis_file;
		uint8_vec m_output_ktx2_file;
		
		basisu::vector<gpu_image> m_uastc_slice_textures;
		basisu_backend_output m_uastc_backend_output;

		bool m_any_source_image_has_alpha;

		bool m_opencl_failed;

		void check_for_hdr_inputs();
		bool sanity_check_input_params();
		void clean_hdr_image(imagef& src_img);
		bool read_dds_source_images();
		bool read_source_images();
		bool extract_source_blocks();
		bool process_frontend();
		bool extract_frontend_texture_data();
		bool process_backend();
		bool create_basis_file_and_transcode();
		bool write_hdr_debug_images(const char* pBasename, const imagef& img, uint32_t width, uint32_t height);
		bool write_output_files_and_compute_stats();
		error_code encode_slices_to_uastc_hdr();
		error_code encode_slices_to_uastc();
		bool generate_mipmaps(const imagef& img, basisu::vector<imagef>& mips, bool has_alpha);
		bool generate_mipmaps(const image &img, basisu::vector<image> &mips, bool has_alpha);
		bool validate_texture_type_constraints();
		bool validate_ktx2_constraints();
		void get_dfd(uint8_vec& dfd, const basist::ktx2_header& hdr);
		bool create_ktx2_file();
	};
				
	// Alternative simple C-style wrapper API around the basis_compressor class. 
	// This doesn't expose every encoder feature, but it's enough to get going.
	// Important: basisu_encoder_init() MUST be called first before calling these functions.
	//
	// Input parameters:
	//   source_images: Array of "image" objects, one per mipmap level, largest mipmap level first.
	// OR
	//   pImageRGBA: pointer to a 32-bpp RGBx or RGBA raster image, R first in memory, A last. Top scanline first in memory.
	//   width/height/pitch_in_pixels: dimensions of pImageRGBA
	//   
	// flags_and_quality: Combination of the above flags logically OR'd with the ETC1S or UASTC level, i.e. "cFlagSRGB | cFlagGenMipsClamp | cFlagThreaded | 128" or "cFlagSRGB | cFlagGenMipsClamp | cFlagUASTC | cFlagThreaded | cPackUASTCLevelDefault".
	//	  In ETC1S mode, the lower 8-bits are the ETC1S quality level which ranges from [1,255] (higher=better quality/larger files)
	//	  In UASTC mode, the lower 8-bits are the UASTC LDR/HDR pack level (see cPackUASTCLevelFastest, etc.). Fastest/lowest quality is 0, so be sure to set it correctly. Valid values are [0,4] for both LDR/HDR.
	//	  In UASTC mode, be sure to set this, otherwise it defaults to 0 (fastest/lowest quality).
	// 
	// uastc_rdo_quality: Float UASTC RDO quality level (0=no change, higher values lower quality but increase compressibility, initially try .5-1.5)
	// 
	// pSize: Returns the output data's compressed size in bytes
	// 
	// Return value is the compressed .basis or .ktx2 file data, or nullptr on failure. Must call basis_free() to free it.
	enum
	{
		cFlagUseOpenCL = 1 << 8,		// use OpenCL if available
		cFlagThreaded = 1 << 9,			// use multiple threads for compression
		cFlagDebug = 1 << 10,			// enable debug output

		cFlagKTX2 = 1 << 11,			// generate a KTX2 file
		cFlagKTX2UASTCSuperCompression = 1 << 12, // use KTX2 Zstd supercompression on UASTC files

		cFlagSRGB = 1 << 13,			// input texture is sRGB, use perceptual colorspace metrics, also use sRGB filtering during mipmap gen, and also sets KTX2 output transfer func to sRGB
		cFlagGenMipsClamp = 1 << 14,  // generate mipmaps with clamp addressing
		cFlagGenMipsWrap = 1 << 15,  // generate mipmaps with wrap addressing
		
		cFlagYFlip = 1 << 16,		// flip source image on Y axis before compression
		
		cFlagUASTC = 1 << 17,		// use UASTC compression vs. ETC1S
		cFlagUASTCRDO = 1 << 18,		// use RDO postprocessing when generating UASTC files (must set uastc_rdo_quality to the quality scalar)
		
		cFlagPrintStats = 1 << 19,	// print image stats to stdout
		cFlagPrintStatus = 1 << 20,	// print status to stdout
		
		cFlagHDR = 1 << 21,			// Force encoder into HDR mode, even if source image is LDR.
		cFlagHDRLDRImageSRGBToLinearConversion = 1 << 22, // In HDR mode, convert LDR source images to linear before encoding.
		
		cFlagDebugImages = 1 << 23	// enable status output
	};

	// This function accepts an array of source images. 
	// If more than one image is provided, it's assumed the images form a mipmap pyramid and automatic mipmap generation is disabled.
	// Returns a pointer to the compressed .basis or .ktx2 file data. *pSize is the size of the compressed data. 
	// Important: The returned block MUST be manually freed using basis_free_data().
	// basisu_encoder_init() MUST be called first!
	// LDR version. To compress the LDR source image as HDR: Use the cFlagHDR flag.
	void* basis_compress(
		const basisu::vector<image> &source_images,
		uint32_t flags_and_quality, float uastc_rdo_quality,
		size_t* pSize,
		image_stats* pStats = nullptr);

	// HDR-only version.
	// Important: The returned block MUST be manually freed using basis_free_data().
	void* basis_compress(
		const basisu::vector<imagef>& source_images_hdr,
		uint32_t flags_and_quality, 
		size_t* pSize,
		image_stats* pStats = nullptr);

	// This function only accepts a single LDR source image. It's just a wrapper for basis_compress() above.
	// Important: The returned block MUST be manually freed using basis_free_data().
	void* basis_compress(
		const uint8_t* pImageRGBA, uint32_t width, uint32_t height, uint32_t pitch_in_pixels,
		uint32_t flags_and_quality, float uastc_rdo_quality,
		size_t* pSize,
		image_stats* pStats = nullptr);

	// Frees the dynamically allocated file data returned by basis_compress().
	// This MUST be called on the pointer returned by basis_compress() when you're done with it.
	void basis_free_data(void* p);

	// Runs a short benchmark using synthetic image data to time OpenCL encoding vs. CPU encoding, with multithreading enabled.
	// Returns true if opencl is worth using on this system, otherwise false.
	// If pOpenCL_failed is not null, it will be set to true if OpenCL encoding failed *on this particular machine/driver/BasisU version* and the encoder falled back to CPU encoding.
	// basisu_encoder_init() MUST be called first. If OpenCL support wasn't enabled this always returns false.
	bool basis_benchmark_etc1s_opencl(bool *pOpenCL_failed = nullptr);

	// Parallel compression API
	struct parallel_results
	{
		double m_total_time;
		basis_compressor::error_code m_error_code;
		uint8_vec m_basis_file;
		uint8_vec m_ktx2_file;
		basisu::vector<image_stats> m_stats;
		double m_basis_bits_per_texel;
		bool m_any_source_image_has_alpha;

		parallel_results() 
		{
			clear();
		}

		void clear()
		{
			m_total_time = 0.0f;
			m_error_code = basis_compressor::cECFailedInitializing;
			m_basis_file.clear();
			m_ktx2_file.clear();
			m_stats.clear();
			m_basis_bits_per_texel = 0.0f;
			m_any_source_image_has_alpha = false;
		}
	};
		
	// Compresses an array of input textures across total_threads threads using the basis_compressor class.
	// Compressing multiple textures at a time is substantially more efficient than just compressing one at a time.
	// total_threads must be >= 1.
	bool basis_parallel_compress(
		uint32_t total_threads,
		const basisu::vector<basis_compressor_params> &params_vec,
		basisu::vector< parallel_results > &results_vec);
		
} // namespace basisu

