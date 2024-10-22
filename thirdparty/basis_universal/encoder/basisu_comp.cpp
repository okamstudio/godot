// basisu_comp.cpp
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
#include "basisu_comp.h"
#include "basisu_enc.h"
#include <unordered_set>
#include <atomic>
#include <map>

//#define UASTC_HDR_DEBUG_SAVE_CATEGORIZED_BLOCKS

// basisu_transcoder.cpp is where basisu_miniz lives now, we just need the declarations here.
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "basisu_miniz.h"

#include "basisu_opencl.h"

#include "../transcoder/basisu_astc_hdr_core.h"

#if !BASISD_SUPPORT_KTX2
#error BASISD_SUPPORT_KTX2 must be enabled (set to 1).
#endif

#if BASISD_SUPPORT_KTX2_ZSTD
#include <zstd.h>
#endif

// Set to 1 to disable the mipPadding alignment workaround (which only seems to be needed when no key-values are written at all)
#define BASISU_DISABLE_KTX2_ALIGNMENT_WORKAROUND (0)

// Set to 1 to disable writing all KTX2 key values, triggering an early validator bug.
#define BASISU_DISABLE_KTX2_KEY_VALUES (0)

using namespace buminiz;

#define BASISU_USE_STB_IMAGE_RESIZE_FOR_MIPMAP_GEN 0
#define DEBUG_CROP_TEXTURE_TO_64x64 (0)
#define DEBUG_RESIZE_TEXTURE (0)
#define DEBUG_EXTRACT_SINGLE_BLOCK (0)

namespace basisu
{
	basis_compressor::basis_compressor() :
		m_pOpenCL_context(nullptr),
		m_basis_file_size(0),
		m_basis_bits_per_texel(0.0f),
		m_total_blocks(0),
		m_any_source_image_has_alpha(false),
		m_opencl_failed(false)
	{
		debug_printf("basis_compressor::basis_compressor\n");
		
		assert(g_library_initialized);
	}

	basis_compressor::~basis_compressor()
	{
		if (m_pOpenCL_context)
		{
			opencl_destroy_context(m_pOpenCL_context);
			m_pOpenCL_context = nullptr;
		}
	}

	void basis_compressor::check_for_hdr_inputs()
	{
		if ((!m_params.m_source_filenames.size()) && (!m_params.m_source_images.size()))
		{
			if (m_params.m_source_images_hdr.size())
			{
				// Assume they want UASTC HDR if they've specified any HDR source images.
				m_params.m_hdr = true;
			}
		}

		if (!m_params.m_hdr)
		{
			// See if any files are .EXR or .HDR, if so switch the compressor to UASTC HDR mode.
			for (uint32_t i = 0; i < m_params.m_source_filenames.size(); i++)
			{
				std::string filename;
				string_get_filename(m_params.m_source_filenames[i].c_str(), filename);

				std::string ext(string_get_extension(filename));
				string_tolower(ext);

				if ((ext == "exr") || (ext == "hdr"))
				{
					m_params.m_hdr = true;
					break;
				}
			}
		}

		if (m_params.m_hdr)
		{
			if (m_params.m_source_alpha_filenames.size())
			{
				debug_printf("Warning: Alpha channel image filenames are not supported in UASTC HDR mode.\n");
				m_params.m_source_alpha_filenames.clear();
			}
		}

		if (m_params.m_hdr)
			m_params.m_uastc = true;
	}

	bool basis_compressor::sanity_check_input_params()
	{
		// Check for no source filenames specified.
		if ((m_params.m_read_source_images) && (!m_params.m_source_filenames.size()))
		{
			assert(0);
			return false;
		}

		// See if they've specified any source filenames, but didn't tell us to read them.
		if ((!m_params.m_read_source_images) && (m_params.m_source_filenames.size()))
		{
			assert(0);
			return false;
		}

		// Sanity check the input image parameters.
		if (m_params.m_read_source_images)
		{
			// Caller can't specify their own images if they want us to read source images from files.
			if (m_params.m_source_images.size() || m_params.m_source_images_hdr.size())
			{
				assert(0);
				return false;
			}

			if (m_params.m_source_mipmap_images.size() || m_params.m_source_mipmap_images_hdr.size())
			{
				assert(0);
				return false;
			}
		}
		else
		{
			// They didn't tell us to read any source files, so check for no LDR/HDR source images.
			if (!m_params.m_source_images.size() && !m_params.m_source_images_hdr.size())
			{
				assert(0);
				return false;
			}

			// Now we know we've been supplied LDR and/or HDR source images, check for LDR vs. HDR conflicts.

			if (m_params.m_source_images.size())
			{
				// They've supplied LDR images, so make sure they also haven't specified HDR input images.
				if (m_params.m_source_images_hdr.size() || m_params.m_source_mipmap_images_hdr.size())
				{
					assert(0);
					return false;
				}
			}
			else
			{
				// No LDR images, so make sure they haven't specified any LDR mipmaps.
				if (m_params.m_source_mipmap_images.size())
				{
					assert(0);
					return false;
				}

				// No LDR images, so ensure they've supplied some HDR images to process.
				if (!m_params.m_source_images_hdr.size())
				{
					assert(0);
					return false;
				}
			}
		}

		return true;
	}

	bool basis_compressor::init(const basis_compressor_params &params)
	{
		debug_printf("basis_compressor::init\n");
				
		if (!g_library_initialized)
		{
			error_printf("basis_compressor::init: basisu_encoder_init() MUST be called before using any encoder functionality!\n");
			return false;
		}

		if (!params.m_pJob_pool)
		{
			error_printf("basis_compressor::init: A non-null job_pool pointer must be specified\n");
			return false;
		}
				
		m_params = params;

		if ((m_params.m_compute_stats) && (!m_params.m_validate_output_data))
			m_params.m_validate_output_data = true;

		check_for_hdr_inputs();
		
		if (m_params.m_debug)
		{
			debug_printf("basis_compressor::init:\n");

#define PRINT_BOOL_VALUE(v) debug_printf("%s: %u %u\n", BASISU_STRINGIZE2(v), static_cast<int>(m_params.v), m_params.v.was_changed());
#define PRINT_INT_VALUE(v) debug_printf("%s: %i %u\n", BASISU_STRINGIZE2(v), static_cast<int>(m_params.v), m_params.v.was_changed());
#define PRINT_UINT_VALUE(v) debug_printf("%s: %u %u\n", BASISU_STRINGIZE2(v), static_cast<uint32_t>(m_params.v), m_params.v.was_changed());
#define PRINT_FLOAT_VALUE(v) debug_printf("%s: %f %u\n", BASISU_STRINGIZE2(v), static_cast<float>(m_params.v), m_params.v.was_changed());
						
			debug_printf("Source LDR images: %u, HDR images: %u, filenames: %u, alpha filenames: %i, LDR mipmap images: %u, HDR mipmap images: %u\n",
				m_params.m_source_images.size(), m_params.m_source_images_hdr.size(),
				m_params.m_source_filenames.size(), m_params.m_source_alpha_filenames.size(),
				m_params.m_source_mipmap_images.size(), m_params.m_source_mipmap_images_hdr.size());

			if (m_params.m_source_mipmap_images.size())
			{
				debug_printf("m_source_mipmap_images array sizes:\n");
				for (uint32_t i = 0; i < m_params.m_source_mipmap_images.size(); i++)
					debug_printf("%u ", m_params.m_source_mipmap_images[i].size());
				debug_printf("\n");
			}

			if (m_params.m_source_mipmap_images_hdr.size())
			{
				debug_printf("m_source_mipmap_images_hdr array sizes:\n");
				for (uint32_t i = 0; i < m_params.m_source_mipmap_images_hdr.size(); i++)
					debug_printf("%u ", m_params.m_source_mipmap_images_hdr[i].size());
				debug_printf("\n");
			}

			PRINT_BOOL_VALUE(m_hdr);
			PRINT_BOOL_VALUE(m_uastc);
			PRINT_BOOL_VALUE(m_use_opencl);
			PRINT_BOOL_VALUE(m_y_flip);
			PRINT_BOOL_VALUE(m_debug);
			PRINT_BOOL_VALUE(m_validate_etc1s);
			PRINT_BOOL_VALUE(m_debug_images);
			PRINT_INT_VALUE(m_compression_level);
			PRINT_BOOL_VALUE(m_perceptual);
			PRINT_BOOL_VALUE(m_no_endpoint_rdo);
			PRINT_BOOL_VALUE(m_no_selector_rdo);
			PRINT_BOOL_VALUE(m_read_source_images);
			PRINT_BOOL_VALUE(m_write_output_basis_or_ktx2_files);
			PRINT_BOOL_VALUE(m_compute_stats);
			PRINT_BOOL_VALUE(m_check_for_alpha);
			PRINT_BOOL_VALUE(m_force_alpha);
			debug_printf("swizzle: %d,%d,%d,%d\n",
				m_params.m_swizzle[0],
				m_params.m_swizzle[1],
				m_params.m_swizzle[2],
				m_params.m_swizzle[3]);
			PRINT_BOOL_VALUE(m_renormalize);
			PRINT_BOOL_VALUE(m_multithreading);
			PRINT_BOOL_VALUE(m_disable_hierarchical_endpoint_codebooks);
												
			PRINT_FLOAT_VALUE(m_endpoint_rdo_thresh);
			PRINT_FLOAT_VALUE(m_selector_rdo_thresh);
			
			PRINT_BOOL_VALUE(m_mip_gen);
			PRINT_BOOL_VALUE(m_mip_renormalize);
			PRINT_BOOL_VALUE(m_mip_wrapping);
			PRINT_BOOL_VALUE(m_mip_fast);
			PRINT_BOOL_VALUE(m_mip_srgb);
			PRINT_FLOAT_VALUE(m_mip_premultiplied);
			PRINT_FLOAT_VALUE(m_mip_scale);
			PRINT_INT_VALUE(m_mip_smallest_dimension);
			debug_printf("m_mip_filter: %s\n", m_params.m_mip_filter.c_str());

			debug_printf("m_max_endpoint_clusters: %u\n", m_params.m_max_endpoint_clusters);
			debug_printf("m_max_selector_clusters: %u\n", m_params.m_max_selector_clusters);
			debug_printf("m_quality_level: %i\n", m_params.m_quality_level);
			debug_printf("UASTC HDR quality level: %u\n", m_params.m_uastc_hdr_options.m_level);

			debug_printf("m_tex_type: %u\n", m_params.m_tex_type);
			debug_printf("m_userdata0: 0x%X, m_userdata1: 0x%X\n", m_params.m_userdata0, m_params.m_userdata1);
			debug_printf("m_us_per_frame: %i (%f fps)\n", m_params.m_us_per_frame, m_params.m_us_per_frame ? 1.0f / (m_params.m_us_per_frame / 1000000.0f) : 0);
			debug_printf("m_pack_uastc_flags: 0x%X\n", m_params.m_pack_uastc_flags);
			
			PRINT_BOOL_VALUE(m_rdo_uastc);
			PRINT_FLOAT_VALUE(m_rdo_uastc_quality_scalar);
			PRINT_INT_VALUE(m_rdo_uastc_dict_size);
			PRINT_FLOAT_VALUE(m_rdo_uastc_max_allowed_rms_increase_ratio);
			PRINT_FLOAT_VALUE(m_rdo_uastc_skip_block_rms_thresh);
			PRINT_FLOAT_VALUE(m_rdo_uastc_max_smooth_block_error_scale);
			PRINT_FLOAT_VALUE(m_rdo_uastc_smooth_block_max_std_dev);
			PRINT_BOOL_VALUE(m_rdo_uastc_favor_simpler_modes_in_rdo_mode)
			PRINT_BOOL_VALUE(m_rdo_uastc_multithreading);

			PRINT_INT_VALUE(m_resample_width);
			PRINT_INT_VALUE(m_resample_height);
			PRINT_FLOAT_VALUE(m_resample_factor);
			
			debug_printf("Has global codebooks: %u\n", m_params.m_pGlobal_codebooks ? 1 : 0);
			if (m_params.m_pGlobal_codebooks)
			{
				debug_printf("Global codebook endpoints: %u selectors: %u\n", m_params.m_pGlobal_codebooks->get_endpoints().size(), m_params.m_pGlobal_codebooks->get_selectors().size());
			}

			PRINT_BOOL_VALUE(m_create_ktx2_file);

			debug_printf("KTX2 UASTC supercompression: %u\n", m_params.m_ktx2_uastc_supercompression);
			debug_printf("KTX2 Zstd supercompression level: %i\n", (int)m_params.m_ktx2_zstd_supercompression_level);
			debug_printf("KTX2 sRGB transfer func: %u\n", (int)m_params.m_ktx2_srgb_transfer_func);
			debug_printf("Total KTX2 key values: %u\n", m_params.m_ktx2_key_values.size());
			for (uint32_t i = 0; i < m_params.m_ktx2_key_values.size(); i++)
			{
				debug_printf("Key: \"%s\"\n", m_params.m_ktx2_key_values[i].m_key.data());
				debug_printf("Value size: %u\n", m_params.m_ktx2_key_values[i].m_value.size());
			}

			PRINT_BOOL_VALUE(m_validate_output_data);
			PRINT_BOOL_VALUE(m_hdr_ldr_srgb_to_linear_conversion);
			debug_printf("Allow UASTC HDR uber mode: %u\n", m_params.m_uastc_hdr_options.m_allow_uber_mode);
			PRINT_BOOL_VALUE(m_hdr_favor_astc);
						
#undef PRINT_BOOL_VALUE
#undef PRINT_INT_VALUE
#undef PRINT_UINT_VALUE
#undef PRINT_FLOAT_VALUE
		}

		if (!sanity_check_input_params())
			return false;
		
		if ((m_params.m_use_opencl) && opencl_is_available() && !m_pOpenCL_context && !m_opencl_failed)
		{
			m_pOpenCL_context = opencl_create_context();
			if (!m_pOpenCL_context)
				m_opencl_failed = true;
		}

		return true;
	}
		
	basis_compressor::error_code basis_compressor::process()
	{
		debug_printf("basis_compressor::process\n");

		if (!read_dds_source_images())
			return cECFailedReadingSourceImages;

		if (!read_source_images())
			return cECFailedReadingSourceImages;

		if (!validate_texture_type_constraints())
			return cECFailedValidating;

		if (m_params.m_create_ktx2_file)
		{
			if (!validate_ktx2_constraints())
			{
				error_printf("Inputs do not satisfy .KTX2 texture constraints: all source images must be the same resolution and have the same number of mipmap levels.\n");
				return cECFailedValidating;
			}
		}

		if (!extract_source_blocks())
			return cECFailedFrontEnd;

		if (m_params.m_hdr)
		{
			// UASTC HDR
			printf("Mode: UASTC HDR Level %u\n", m_params.m_uastc_hdr_options.m_level);

			error_code ec = encode_slices_to_uastc_hdr();
			if (ec != cECSuccess)
				return ec;
		}
		else if (m_params.m_uastc)
		{
			// UASTC
			printf("Mode: UASTC LDR Level %u\n", m_params.m_pack_uastc_flags & cPackUASTCLevelMask);

			error_code ec = encode_slices_to_uastc();
			if (ec != cECSuccess)
				return ec;
		}
		else
		{
			// ETC1S
			printf("Mode: ETC1S Quality %i, Level %i\n", m_params.m_quality_level, (int)m_params.m_compression_level);
			
			if (!process_frontend())
				return cECFailedFrontEnd;

			if (!extract_frontend_texture_data())
				return cECFailedFontendExtract;

			if (!process_backend())
				return cECFailedBackend;
		}

		if (!create_basis_file_and_transcode())
			return cECFailedCreateBasisFile;

		if (m_params.m_create_ktx2_file)
		{
			if (!create_ktx2_file())
				return cECFailedCreateKTX2File;
		}

		if (!write_output_files_and_compute_stats())
			return cECFailedWritingOutput;

		return cECSuccess;
	}

	basis_compressor::error_code basis_compressor::encode_slices_to_uastc_hdr()
	{
		debug_printf("basis_compressor::encode_slices_to_uastc_hdr\n");

		interval_timer tm;
		tm.start();

		m_uastc_slice_textures.resize(m_slice_descs.size());
		for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
			m_uastc_slice_textures[slice_index].init(texture_format::cUASTC_HDR_4x4, m_slice_descs[slice_index].m_orig_width, m_slice_descs[slice_index].m_orig_height);

		m_uastc_backend_output.m_tex_format = basist::basis_tex_format::cUASTC_HDR_4x4;
		m_uastc_backend_output.m_etc1s = false;
		m_uastc_backend_output.m_slice_desc = m_slice_descs;
		m_uastc_backend_output.m_slice_image_data.resize(m_slice_descs.size());
		m_uastc_backend_output.m_slice_image_crcs.resize(m_slice_descs.size());

		if (!m_params.m_perceptual)
		{
			m_params.m_uastc_hdr_options.m_r_err_scale = 1.0f;
			m_params.m_uastc_hdr_options.m_g_err_scale = 1.0f;
		}
		
		const float DEFAULT_BC6H_ERROR_WEIGHT = .85f;
		const float LOWEST_BC6H_ERROR_WEIGHT = .1f;
		m_params.m_uastc_hdr_options.m_bc6h_err_weight = m_params.m_hdr_favor_astc ? LOWEST_BC6H_ERROR_WEIGHT : DEFAULT_BC6H_ERROR_WEIGHT;

		std::atomic<bool> any_failures;
		any_failures = false;

		astc_hdr_block_stats enc_stats;
				
		struct uastc_blk_desc
		{
			uint32_t m_solid_flag;
			uint32_t m_num_partitions;
			uint32_t m_cem_index;
			uint32_t m_weight_ise_range;
			uint32_t m_endpoint_ise_range;

			bool operator< (const uastc_blk_desc& desc) const
			{
				if (this == &desc)
					return false;

#define COMP(XX) if (XX < desc.XX) return true; else if (XX != desc.XX) return false;
				COMP(m_solid_flag)
				COMP(m_num_partitions)
				COMP(m_cem_index)
				COMP(m_weight_ise_range)
				COMP(m_endpoint_ise_range)
#undef COMP

				return false;
			}
			
			bool operator== (const uastc_blk_desc& desc) const
			{
				if (this == &desc)
					return true;
				if ((*this < desc) || (desc < *this))
					return false;
				return true;
			}

			bool operator!= (const uastc_blk_desc& desc) const
			{
				return !(*this == desc);
			}
		};

		struct uastc_blk_desc_stats
		{
			uastc_blk_desc_stats() : m_count(0) { }
			uint32_t m_count;
#ifdef UASTC_HDR_DEBUG_SAVE_CATEGORIZED_BLOCKS
			basisu::vector<basist::astc_blk> m_blks;
#endif
		};

		std::map<uastc_blk_desc, uastc_blk_desc_stats> unique_block_descs;
		std::mutex unique_block_desc_mutex;
		
		for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
		{
			gpu_image& tex = m_uastc_slice_textures[slice_index];
			basisu_backend_slice_desc& slice_desc = m_slice_descs[slice_index];
			(void)slice_desc;

			const uint32_t num_blocks_x = tex.get_blocks_x();
			const uint32_t num_blocks_y = tex.get_blocks_y();
			const uint32_t total_blocks = tex.get_total_blocks();
			const imagef& source_image = m_slice_images_hdr[slice_index];

			std::atomic<uint32_t> total_blocks_processed;
			total_blocks_processed = 0;
						
			const uint32_t N = 256;
			for (uint32_t block_index_iter = 0; block_index_iter < total_blocks; block_index_iter += N)
			{
				const uint32_t first_index = block_index_iter;
				const uint32_t last_index = minimum<uint32_t>(total_blocks, block_index_iter + N);
			
				// FIXME: This sucks, but we're having a stack size related problem with std::function with emscripten.
#ifndef __EMSCRIPTEN__
				m_params.m_pJob_pool->add_job([this, first_index, last_index, num_blocks_x, num_blocks_y, total_blocks, &source_image, 
					&tex, &total_blocks_processed, &any_failures, &enc_stats, &unique_block_descs, &unique_block_desc_mutex]
					{
#endif
						BASISU_NOTE_UNUSED(num_blocks_y);

						basisu::vector<astc_hdr_pack_results> all_results;
						all_results.reserve(256);

						for (uint32_t block_index = first_index; block_index < last_index; block_index++)
						{
							const uint32_t block_x = block_index % num_blocks_x;
							const uint32_t block_y = block_index / num_blocks_x;

							vec4F block_pixels[16];

							source_image.extract_block_clamped(&block_pixels[0], block_x * 4, block_y * 4, 4, 4);

							basist::astc_blk& dest_block = *(basist::astc_blk*)tex.get_block_ptr(block_x, block_y);
														
							float rgb_pixels[16 * 3];
							basist::half_float rgb_pixels_half[16 * 3];
							for (uint32_t i = 0; i < 16; i++)
							{
								rgb_pixels[i * 3 + 0] = block_pixels[i][0];
								rgb_pixels_half[i * 3 + 0] = float_to_half_non_neg_no_nan_inf(block_pixels[i][0]);

								rgb_pixels[i * 3 + 1] = block_pixels[i][1];
								rgb_pixels_half[i * 3 + 1] = float_to_half_non_neg_no_nan_inf(block_pixels[i][1]);

								rgb_pixels[i * 3 + 2] = block_pixels[i][2];
								rgb_pixels_half[i * 3 + 2] = float_to_half_non_neg_no_nan_inf(block_pixels[i][2]);
							}
														
							bool status = astc_hdr_enc_block(&rgb_pixels[0], m_params.m_uastc_hdr_options, all_results);
							if (!status)
							{
								any_failures = true;
								continue;
							}

							double best_err = 1e+30f;
							int best_result_index = -1;
											
							const double bc6h_err_weight = m_params.m_uastc_hdr_options.m_bc6h_err_weight;
							const double astc_err_weight = (1.0f - bc6h_err_weight);
										
							for (uint32_t i = 0; i < all_results.size(); i++)
							{
								basist::half_float unpacked_bc6h_block[4 * 4 * 3];
								unpack_bc6h(&all_results[i].m_bc6h_block, unpacked_bc6h_block, false);

								all_results[i].m_bc6h_block_error = compute_block_error(rgb_pixels_half, unpacked_bc6h_block, m_params.m_uastc_hdr_options);

								double overall_err = (all_results[i].m_bc6h_block_error * bc6h_err_weight) + (all_results[i].m_best_block_error * astc_err_weight);

								if ((!i) || (overall_err < best_err))
								{
									best_err = overall_err;
									best_result_index = i;
								}
							}

							const astc_hdr_pack_results& best_results = all_results[best_result_index];
							
							astc_hdr_pack_results_to_block(dest_block, best_results);
								
							// Verify that this block is valid UASTC HDR and we can successfully transcode it to BC6H.
							// (Well, except in fastest mode.)
							if (m_params.m_uastc_hdr_options.m_level > 0)
							{
								basist::bc6h_block transcoded_bc6h_blk;
								bool transcode_results = astc_hdr_transcode_to_bc6h(dest_block, transcoded_bc6h_blk);
								assert(transcode_results);
								if ((!transcode_results) && (!any_failures))
								{
									error_printf("basis_compressor::encode_slices_to_uastc_hdr: UASTC HDR block transcode check failed!\n");

									any_failures = true;
									continue;
								}
							}

							if (m_params.m_debug)
							{
								// enc_stats has its own mutex
								enc_stats.update(best_results);

								uastc_blk_desc blk_desc;
								clear_obj(blk_desc);

								blk_desc.m_solid_flag = best_results.m_is_solid;
								if (!blk_desc.m_solid_flag)
								{
									blk_desc.m_num_partitions = best_results.m_best_blk.m_num_partitions;
									blk_desc.m_cem_index = best_results.m_best_blk.m_color_endpoint_modes[0];
									blk_desc.m_weight_ise_range = best_results.m_best_blk.m_weight_ise_range;
									blk_desc.m_endpoint_ise_range = best_results.m_best_blk.m_endpoint_ise_range;
								}
								
								{
									std::lock_guard<std::mutex> lck(unique_block_desc_mutex);
																		
									auto res = unique_block_descs.insert(std::make_pair(blk_desc, uastc_blk_desc_stats()));
									
									(res.first)->second.m_count++;
#ifdef UASTC_HDR_DEBUG_SAVE_CATEGORIZED_BLOCKS
									(res.first)->second.m_blks.push_back(dest_block);
#endif
								}
							}

							total_blocks_processed++;

							uint32_t val = total_blocks_processed;
							if (((val & 1023) == 1023) && m_params.m_status_output)
							{
								debug_printf("basis_compressor::encode_slices_to_uastc_hdr: %3.1f%% done\n", static_cast<float>(val) * 100.0f / total_blocks);
							}
						}

#ifndef __EMSCRIPTEN__
					});
#endif
			
			} // block_index_iter

#ifndef __EMSCRIPTEN__
			m_params.m_pJob_pool->wait_for_all();
#endif

			if (any_failures)
				return cECFailedEncodeUASTC;

			m_uastc_backend_output.m_slice_image_data[slice_index].resize(tex.get_size_in_bytes());
			memcpy(&m_uastc_backend_output.m_slice_image_data[slice_index][0], tex.get_ptr(), tex.get_size_in_bytes());

			m_uastc_backend_output.m_slice_image_crcs[slice_index] = basist::crc16(tex.get_ptr(), tex.get_size_in_bytes(), 0);

		} // slice_index
		
		debug_printf("basis_compressor::encode_slices_to_uastc_hdr: Total time: %3.3f secs\n", tm.get_elapsed_secs());

		if (m_params.m_debug)
		{
			debug_printf("\n----- Total unique UASTC block descs: %u\n", (uint32_t)unique_block_descs.size());

			uint32_t c = 0;
			for (auto it = unique_block_descs.begin(); it != unique_block_descs.end(); ++it)
			{
				debug_printf("%u. Total uses: %u %3.2f%%, solid color: %u\n", c, it->second.m_count,
					((float)it->second.m_count * 100.0f) / enc_stats.m_total_blocks, it->first.m_solid_flag);

				if (!it->first.m_solid_flag)
				{
					debug_printf("  Num partitions: %u\n", it->first.m_num_partitions);
					debug_printf("  CEM index: %u\n", it->first.m_cem_index);
					debug_printf("  Weight ISE range: %u (%u levels)\n", it->first.m_weight_ise_range, astc_helpers::get_ise_levels(it->first.m_weight_ise_range));
					debug_printf("  Endpoint ISE range: %u (%u levels)\n", it->first.m_endpoint_ise_range, astc_helpers::get_ise_levels(it->first.m_endpoint_ise_range));
				}

#ifdef UASTC_HDR_DEBUG_SAVE_CATEGORIZED_BLOCKS
				debug_printf("  -- UASTC HDR block bytes:\n");
				for (uint32_t j = 0; j < minimum<uint32_t>(4, it->second.m_blks.size()); j++)
				{
					basist::astc_blk& blk = it->second.m_blks[j];

					debug_printf("    - UASTC HDR: { ");
					for (uint32_t k = 0; k < 16; k++)
						debug_printf("%u%s", ((const uint8_t*)&blk)[k], (k != 15) ? ", " : "");
					debug_printf(" }\n");

					basist::bc6h_block bc6h_blk;
					bool res = astc_hdr_transcode_to_bc6h(blk, bc6h_blk);
					assert(res);
					if (!res)
					{
						error_printf("astc_hdr_transcode_to_bc6h() failed!\n");
						return cECFailedEncodeUASTC;
					}

					debug_printf("    - BC6H: { ");
					for (uint32_t k = 0; k < 16; k++)
						debug_printf("%u%s", ((const uint8_t*)&bc6h_blk)[k], (k != 15) ? ", " : "");
					debug_printf(" }\n");
				}
#endif
					
				c++;
			}
			printf("\n");
			
			enc_stats.print();
		}

		return cECSuccess;
	}

	basis_compressor::error_code basis_compressor::encode_slices_to_uastc()
	{
		debug_printf("basis_compressor::encode_slices_to_uastc\n");

		m_uastc_slice_textures.resize(m_slice_descs.size());
		for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
			m_uastc_slice_textures[slice_index].init(texture_format::cUASTC4x4, m_slice_descs[slice_index].m_orig_width, m_slice_descs[slice_index].m_orig_height);

		m_uastc_backend_output.m_tex_format = basist::basis_tex_format::cUASTC4x4;
		m_uastc_backend_output.m_etc1s = false;
		m_uastc_backend_output.m_slice_desc = m_slice_descs;
		m_uastc_backend_output.m_slice_image_data.resize(m_slice_descs.size());
		m_uastc_backend_output.m_slice_image_crcs.resize(m_slice_descs.size());
				
		for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
		{
			gpu_image& tex = m_uastc_slice_textures[slice_index];
			basisu_backend_slice_desc& slice_desc = m_slice_descs[slice_index];
			(void)slice_desc;

			const uint32_t num_blocks_x = tex.get_blocks_x();
			const uint32_t num_blocks_y = tex.get_blocks_y();
			const uint32_t total_blocks = tex.get_total_blocks();
			const image& source_image = m_slice_images[slice_index];
			
			std::atomic<uint32_t> total_blocks_processed;
			total_blocks_processed = 0;

			const uint32_t N = 256;
			for (uint32_t block_index_iter = 0; block_index_iter < total_blocks; block_index_iter += N)
			{
				const uint32_t first_index = block_index_iter;
				const uint32_t last_index = minimum<uint32_t>(total_blocks, block_index_iter + N);

				// FIXME: This sucks, but we're having a stack size related problem with std::function with emscripten.
#ifndef __EMSCRIPTEN__
				m_params.m_pJob_pool->add_job([this, first_index, last_index, num_blocks_x, num_blocks_y, total_blocks, &source_image, &tex, &total_blocks_processed]
					{
#endif
						BASISU_NOTE_UNUSED(num_blocks_y);
						
						uint32_t uastc_flags = m_params.m_pack_uastc_flags;
						if ((m_params.m_rdo_uastc) && (m_params.m_rdo_uastc_favor_simpler_modes_in_rdo_mode))
							uastc_flags |= cPackUASTCFavorSimplerModes;

						for (uint32_t block_index = first_index; block_index < last_index; block_index++)
						{
							const uint32_t block_x = block_index % num_blocks_x;
							const uint32_t block_y = block_index / num_blocks_x;

							color_rgba block_pixels[4][4];

							source_image.extract_block_clamped((color_rgba*)block_pixels, block_x * 4, block_y * 4, 4, 4);

							basist::uastc_block& dest_block = *(basist::uastc_block*)tex.get_block_ptr(block_x, block_y);

							encode_uastc(&block_pixels[0][0].r, dest_block, uastc_flags);

							total_blocks_processed++;
							
							uint32_t val = total_blocks_processed;
							if (((val & 16383) == 16383) && m_params.m_status_output)
							{
								debug_printf("basis_compressor::encode_slices_to_uastc: %3.1f%% done\n", static_cast<float>(val) * 100.0f / total_blocks);
							}

						}

#ifndef __EMSCRIPTEN__
					});
#endif

			} // block_index_iter

#ifndef __EMSCRIPTEN__
			m_params.m_pJob_pool->wait_for_all();
#endif

			if (m_params.m_rdo_uastc)
			{
				uastc_rdo_params rdo_params;
				rdo_params.m_lambda = m_params.m_rdo_uastc_quality_scalar;
				rdo_params.m_max_allowed_rms_increase_ratio = m_params.m_rdo_uastc_max_allowed_rms_increase_ratio;
				rdo_params.m_skip_block_rms_thresh = m_params.m_rdo_uastc_skip_block_rms_thresh;
				rdo_params.m_lz_dict_size = m_params.m_rdo_uastc_dict_size;
				rdo_params.m_smooth_block_max_error_scale = m_params.m_rdo_uastc_max_smooth_block_error_scale;
				rdo_params.m_max_smooth_block_std_dev = m_params.m_rdo_uastc_smooth_block_max_std_dev;
								
				bool status = uastc_rdo(tex.get_total_blocks(), (basist::uastc_block*)tex.get_ptr(),
					(const color_rgba *)m_source_blocks[slice_desc.m_first_block_index].m_pixels, rdo_params, m_params.m_pack_uastc_flags, m_params.m_rdo_uastc_multithreading ? m_params.m_pJob_pool : nullptr,
					(m_params.m_rdo_uastc_multithreading && m_params.m_pJob_pool) ? basisu::minimum<uint32_t>(4, (uint32_t)m_params.m_pJob_pool->get_total_threads()) : 0);
				if (!status)
				{
					return cECFailedUASTCRDOPostProcess;
				}
			}

			m_uastc_backend_output.m_slice_image_data[slice_index].resize(tex.get_size_in_bytes());
			memcpy(&m_uastc_backend_output.m_slice_image_data[slice_index][0], tex.get_ptr(), tex.get_size_in_bytes());
			
			m_uastc_backend_output.m_slice_image_crcs[slice_index] = basist::crc16(tex.get_ptr(), tex.get_size_in_bytes(), 0);
						
		} // slice_index
				
		return cECSuccess;
	}

	bool basis_compressor::generate_mipmaps(const imagef& img, basisu::vector<imagef>& mips, bool has_alpha)
	{
		debug_printf("basis_compressor::generate_mipmaps\n");

		interval_timer tm;
		tm.start();

		uint32_t total_levels = 1;
		uint32_t w = img.get_width(), h = img.get_height();
		while (maximum<uint32_t>(w, h) > (uint32_t)m_params.m_mip_smallest_dimension)
		{
			w = maximum(w >> 1U, 1U);
			h = maximum(h >> 1U, 1U);
			total_levels++;
		}

		for (uint32_t level = 1; level < total_levels; level++)
		{
			const uint32_t level_width = maximum<uint32_t>(1, img.get_width() >> level);
			const uint32_t level_height = maximum<uint32_t>(1, img.get_height() >> level);

			imagef& level_img = *enlarge_vector(mips, 1);
			level_img.resize(level_width, level_height);

			const imagef* pSource_image = &img;

			if (m_params.m_mip_fast)
			{
				if (level > 1)
					pSource_image = &mips[level - 1];
			}

			bool status = image_resample(*pSource_image, level_img, 
				//m_params.m_mip_filter.c_str(), 
				"box", // TODO: negative lobes in the filter are causing negative colors, try Mitchell
				m_params.m_mip_scale, m_params.m_mip_wrapping, 0, has_alpha ? 4 : 3);
			if (!status)
			{
				error_printf("basis_compressor::generate_mipmaps: image_resample() failed!\n");
				return false;
			}

			clean_hdr_image(level_img);
		}

		if (m_params.m_debug)
			debug_printf("Total mipmap generation time: %3.3f secs\n", tm.get_elapsed_secs());

		return true;
	}

	bool basis_compressor::generate_mipmaps(const image &img, basisu::vector<image> &mips, bool has_alpha)
	{
		debug_printf("basis_compressor::generate_mipmaps\n");

		interval_timer tm;
		tm.start();

		uint32_t total_levels = 1;
		uint32_t w = img.get_width(), h = img.get_height();
		while (maximum<uint32_t>(w, h) > (uint32_t)m_params.m_mip_smallest_dimension)
		{
			w = maximum(w >> 1U, 1U);
			h = maximum(h >> 1U, 1U);
			total_levels++;
		}

#if BASISU_USE_STB_IMAGE_RESIZE_FOR_MIPMAP_GEN
		// Requires stb_image_resize
		stbir_filter filter = STBIR_FILTER_DEFAULT;
		if (m_params.m_mip_filter == "box")
			filter = STBIR_FILTER_BOX;
		else if (m_params.m_mip_filter == "triangle")
			filter = STBIR_FILTER_TRIANGLE;
		else if (m_params.m_mip_filter == "cubic")
			filter = STBIR_FILTER_CUBICBSPLINE;
		else if (m_params.m_mip_filter == "catmull")
			filter = STBIR_FILTER_CATMULLROM;
		else if (m_params.m_mip_filter == "mitchell")
			filter = STBIR_FILTER_MITCHELL;

		for (uint32_t level = 1; level < total_levels; level++)
		{
			const uint32_t level_width = maximum<uint32_t>(1, img.get_width() >> level);
			const uint32_t level_height = maximum<uint32_t>(1, img.get_height() >> level);

			image &level_img = *enlarge_vector(mips, 1);
			level_img.resize(level_width, level_height);
						
			int result = stbir_resize_uint8_generic( 
				(const uint8_t *)img.get_ptr(), img.get_width(), img.get_height(), img.get_pitch() * sizeof(color_rgba),
            (uint8_t *)level_img.get_ptr(), level_img.get_width(), level_img.get_height(), level_img.get_pitch() * sizeof(color_rgba),
            has_alpha ? 4 : 3, has_alpha ? 3 : STBIR_ALPHA_CHANNEL_NONE, m_params.m_mip_premultiplied ? STBIR_FLAG_ALPHA_PREMULTIPLIED : 0,
            m_params.m_mip_wrapping ? STBIR_EDGE_WRAP : STBIR_EDGE_CLAMP, filter, m_params.m_mip_srgb ? STBIR_COLORSPACE_SRGB : STBIR_COLORSPACE_LINEAR, 
				nullptr);

			if (result == 0)
			{
				error_printf("basis_compressor::generate_mipmaps: stbir_resize_uint8_generic() failed!\n");
				return false;
			}
			
			if (m_params.m_mip_renormalize)
				level_img.renormalize_normal_map();
		}
#else
		for (uint32_t level = 1; level < total_levels; level++)
		{
			const uint32_t level_width = maximum<uint32_t>(1, img.get_width() >> level);
			const uint32_t level_height = maximum<uint32_t>(1, img.get_height() >> level);

			image& level_img = *enlarge_vector(mips, 1);
			level_img.resize(level_width, level_height);

			const image* pSource_image = &img;

			if (m_params.m_mip_fast)
			{
				if (level > 1)
					pSource_image = &mips[level - 1];
			}

			bool status = image_resample(*pSource_image, level_img, m_params.m_mip_srgb, m_params.m_mip_filter.c_str(), m_params.m_mip_scale, m_params.m_mip_wrapping, 0, has_alpha ? 4 : 3);
			if (!status)
			{
				error_printf("basis_compressor::generate_mipmaps: image_resample() failed!\n");
				return false;
			}

			if (m_params.m_mip_renormalize)
				level_img.renormalize_normal_map();
		}
#endif

		if (m_params.m_debug)
			debug_printf("Total mipmap generation time: %3.3f secs\n", tm.get_elapsed_secs());

		return true;
	}

	void basis_compressor::clean_hdr_image(imagef& src_img)
	{
		const uint32_t width = src_img.get_width();
		const uint32_t height = src_img.get_height();

		float max_used_val = 0.0f;
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				vec4F& c = src_img(x, y);
				for (uint32_t i = 0; i < 3; i++)
					max_used_val = maximum(max_used_val, c[i]);
			}
		}

		double hdr_image_scale = 1.0f;
		if (max_used_val > basist::ASTC_HDR_MAX_VAL)
		{
			hdr_image_scale = max_used_val / basist::ASTC_HDR_MAX_VAL;

			const double inv_hdr_image_scale = basist::ASTC_HDR_MAX_VAL / max_used_val;

			for (uint32_t y = 0; y < src_img.get_height(); y++)
			{
				for (uint32_t x = 0; x < src_img.get_width(); x++)
				{
					vec4F& c = src_img(x, y);

					for (uint32_t i = 0; i < 3; i++)
						c[i] = (float)minimum<double>(c[i] * inv_hdr_image_scale, basist::ASTC_HDR_MAX_VAL);
				}
			}

			printf("Warning: The input HDR image's maximum used float value was %f, which is too high to encode as ASTC HDR. The image's components have been linearly scaled so the maximum used value is %f, by multiplying by %f.\n",
				max_used_val, basist::ASTC_HDR_MAX_VAL, inv_hdr_image_scale);

			printf("The decoded ASTC HDR texture will have to be scaled up by %f.\n", hdr_image_scale);
		}

		// TODO: Determine a constant scale factor, apply if > MAX_HALF_FLOAT
		if (!src_img.clean_astc_hdr_pixels(basist::ASTC_HDR_MAX_VAL))
			printf("Warning: clean_astc_hdr_pixels() had to modify the input image to encode to ASTC HDR - see previous warning(s).\n");

		float lowest_nonzero_val = 1e+30f;
		float lowest_val = 1e+30f;
		float highest_val = -1e+30f;

		for (uint32_t y = 0; y < src_img.get_height(); y++)
		{
			for (uint32_t x = 0; x < src_img.get_width(); x++)
			{
				const vec4F& c = src_img(x, y);

				for (uint32_t i = 0; i < 3; i++)
				{
					lowest_val = basisu::minimum(lowest_val, c[i]);

					if (c[i] != 0.0f)
						lowest_nonzero_val = basisu::minimum(lowest_nonzero_val, c[i]);

					highest_val = basisu::maximum(highest_val, c[i]);
				}
			}
		}

		debug_printf("Lowest image value: %e, lowest non-zero value: %e, highest value: %e, dynamic range: %e\n", lowest_val, lowest_nonzero_val, highest_val, highest_val / lowest_nonzero_val);
	}

	bool basis_compressor::read_dds_source_images()
	{
		debug_printf("basis_compressor::read_dds_source_images\n");

		// Nothing to do if the caller doesn't want us reading source images.
		if ((!m_params.m_read_source_images) || (!m_params.m_source_filenames.size()))
			return true;

		// Just bail of the caller has specified their own source images.
		if (m_params.m_source_images.size() || m_params.m_source_images_hdr.size())
			return true;

		if (m_params.m_source_mipmap_images.size() || m_params.m_source_mipmap_images_hdr.size())
			return true;
				
		// See if any input filenames are .DDS
		bool any_dds = false, all_dds = true;
		for (uint32_t i = 0; i < m_params.m_source_filenames.size(); i++)
		{
			std::string ext(string_get_extension(m_params.m_source_filenames[i]));
			if (strcasecmp(ext.c_str(), "dds") == 0)
				any_dds = true;
			else
				all_dds = false;
		}

		// Bail if no .DDS files specified.
		if (!any_dds)
			return true;

		// If any input is .DDS they all must be .DDS, for simplicity.
		if (!all_dds)
		{
			error_printf("If any filename is DDS, all filenames must be DDS.\n");
			return false;
		}

		// Can't jam in alpha channel images if any .DDS files specified.
		if (m_params.m_source_alpha_filenames.size())
		{
			error_printf("Source alpha filenames are not supported in DDS mode.\n");
			return false;
		}

		bool any_mipmaps = false;

		// Read each .DDS texture file
		for (uint32_t i = 0; i < m_params.m_source_filenames.size(); i++)
		{
			basisu::vector<image> ldr_mips;
			basisu::vector<imagef> hdr_mips;
			bool status = read_uncompressed_dds_file(m_params.m_source_filenames[i].c_str(), ldr_mips, hdr_mips);
			if (!status)
				return false;

			assert(ldr_mips.size() || hdr_mips.size());

			if (m_params.m_status_output)
			{
				printf("Read DDS file \"%s\", %s, %ux%u, %u mipmap levels\n",
					m_params.m_source_filenames[i].c_str(),
					ldr_mips.size() ? "LDR" : "HDR",
					ldr_mips.size() ? ldr_mips[0].get_width() : hdr_mips[0].get_width(),
					ldr_mips.size() ? ldr_mips[0].get_height() : hdr_mips[0].get_height(),
					ldr_mips.size() ? ldr_mips.size() : hdr_mips.size());
			}

			if (ldr_mips.size())
			{
				if (m_params.m_source_images_hdr.size())
				{
					error_printf("All DDS files must be of the same type (all LDR, or all HDR)\n");
					return false;
				}

				m_params.m_source_images.push_back(ldr_mips[0]);
				m_params.m_source_mipmap_images.resize(m_params.m_source_mipmap_images.size() + 1);

				if (ldr_mips.size() > 1)
				{
					ldr_mips.erase(0U);

					m_params.m_source_mipmap_images.back().swap(ldr_mips);
					
					any_mipmaps = true;
				}
			}
			else
			{
				if (m_params.m_source_images.size())
				{
					error_printf("All DDS files must be of the same type (all LDR, or all HDR)\n");
					return false;
				}

				m_params.m_source_images_hdr.push_back(hdr_mips[0]);
				m_params.m_source_mipmap_images_hdr.resize(m_params.m_source_mipmap_images_hdr.size() + 1);

				if (hdr_mips.size() > 1)
				{
					hdr_mips.erase(0U);

					m_params.m_source_mipmap_images_hdr.back().swap(hdr_mips);
					
					any_mipmaps = true;
				}

				m_params.m_hdr = true;
				m_params.m_uastc = true;
			}
		}

		m_params.m_read_source_images = false;
		m_params.m_source_filenames.clear();
		m_params.m_source_alpha_filenames.clear();

		if (!any_mipmaps)
		{
			m_params.m_source_mipmap_images.clear();
			m_params.m_source_mipmap_images_hdr.clear();
		}

		if ((m_params.m_hdr) && (!m_params.m_source_images_hdr.size()))
		{
			error_printf("HDR mode enabled, but only LDR .DDS files were loaded. HDR mode requires half or float (HDR) .DDS inputs.\n");
			return false;
		}
		
		return true;
	}

	bool basis_compressor::read_source_images()
	{
		debug_printf("basis_compressor::read_source_images\n");

		const uint32_t total_source_files = m_params.m_read_source_images ? (uint32_t)m_params.m_source_filenames.size() : 
			(m_params.m_hdr ? (uint32_t)m_params.m_source_images_hdr.size() : (uint32_t)m_params.m_source_images.size());

		if (!total_source_files)
		{
			debug_printf("basis_compressor::read_source_images: No source images to process\n");

			return false;
		}

		m_stats.resize(0);
		m_slice_descs.resize(0);
		m_slice_images.resize(0);
		m_slice_images_hdr.resize(0);

		m_total_blocks = 0;
		uint32_t total_macroblocks = 0;

		m_any_source_image_has_alpha = false;

		basisu::vector<image> source_images;
		basisu::vector<imagef> source_images_hdr;

		basisu::vector<std::string> source_filenames;
		
		// TODO: Note HDR images don't support alpha here, currently.

		// First load all source images, and determine if any have an alpha channel.
		for (uint32_t source_file_index = 0; source_file_index < total_source_files; source_file_index++)
		{
			const char* pSource_filename = "";

			image file_image;
			imagef file_image_hdr;

			if (m_params.m_read_source_images)
			{
				pSource_filename = m_params.m_source_filenames[source_file_index].c_str();

				// Load the source image
				if (m_params.m_hdr)
				{
					if (!load_image_hdr(pSource_filename, file_image_hdr, m_params.m_hdr_ldr_srgb_to_linear_conversion))
					{
						error_printf("Failed reading source image: %s\n", pSource_filename);
						return false;
					}

					// For now, just slam alpha to 1.0f. UASTC HDR doesn't support alpha yet.
					for (uint32_t y = 0; y < file_image_hdr.get_height(); y++)
						for (uint32_t x = 0; x < file_image_hdr.get_width(); x++)
							file_image_hdr(x, y)[3] = 1.0f;
				}
				else
				{
					if (!load_image(pSource_filename, file_image))
					{
						error_printf("Failed reading source image: %s\n", pSource_filename);
						return false;
					}
				}

				const uint32_t width = m_params.m_hdr ? file_image_hdr.get_width() : file_image.get_width();
				const uint32_t height = m_params.m_hdr ? file_image_hdr.get_height() : file_image.get_height();

				if (m_params.m_status_output)
				{
					printf("Read source image \"%s\", %ux%u\n", pSource_filename, width, height);
				}

				if (m_params.m_hdr)
				{
					clean_hdr_image(file_image_hdr);
				}
				else
				{
					// Optionally load another image and put a grayscale version of it into the alpha channel.
					if ((source_file_index < m_params.m_source_alpha_filenames.size()) && (m_params.m_source_alpha_filenames[source_file_index].size()))
					{
						const char* pSource_alpha_image = m_params.m_source_alpha_filenames[source_file_index].c_str();

						image alpha_data;

						if (!load_image(pSource_alpha_image, alpha_data))
						{
							error_printf("Failed reading source image: %s\n", pSource_alpha_image);
							return false;
						}

						printf("Read source alpha image \"%s\", %ux%u\n", pSource_alpha_image, alpha_data.get_width(), alpha_data.get_height());

						alpha_data.crop(width, height);

						for (uint32_t y = 0; y < height; y++)
							for (uint32_t x = 0; x < width; x++)
								file_image(x, y).a = (uint8_t)alpha_data(x, y).get_709_luma();
					}
				}
			}
			else
			{
				if (m_params.m_hdr)
				{
					file_image_hdr = m_params.m_source_images_hdr[source_file_index];
					clean_hdr_image(file_image_hdr);
				}
				else
				{
					file_image = m_params.m_source_images[source_file_index];
				}
			}

			if (!m_params.m_hdr)
			{
				if (m_params.m_renormalize)
					file_image.renormalize_normal_map();
			}

			bool alpha_swizzled = false;

			if (m_params.m_swizzle[0] != 0 ||
				m_params.m_swizzle[1] != 1 ||
				m_params.m_swizzle[2] != 2 ||
				m_params.m_swizzle[3] != 3)
			{
				if (!m_params.m_hdr)
				{
					// Used for XY normal maps in RG - puts X in color, Y in alpha
					for (uint32_t y = 0; y < file_image.get_height(); y++)
					{
						for (uint32_t x = 0; x < file_image.get_width(); x++)
						{
							const color_rgba& c = file_image(x, y);
							file_image(x, y).set_noclamp_rgba(c[m_params.m_swizzle[0]], c[m_params.m_swizzle[1]], c[m_params.m_swizzle[2]], c[m_params.m_swizzle[3]]);
						}
					}

					alpha_swizzled = (m_params.m_swizzle[3] != 3);
				}
				else
				{
					// Used for XY normal maps in RG - puts X in color, Y in alpha
					for (uint32_t y = 0; y < file_image_hdr.get_height(); y++)
					{
						for (uint32_t x = 0; x < file_image_hdr.get_width(); x++)
						{
							const vec4F& c = file_image_hdr(x, y);
							
							// For now, alpha is always 1.0f in UASTC HDR.
							file_image_hdr(x, y).set(c[m_params.m_swizzle[0]], c[m_params.m_swizzle[1]], c[m_params.m_swizzle[2]], 1.0f); // c[m_params.m_swizzle[3]]);
						}
					}
				}
			}

			bool has_alpha = false;

			if (!m_params.m_hdr)
			{
				if (m_params.m_force_alpha || alpha_swizzled)
					has_alpha = true;
				else if (!m_params.m_check_for_alpha)
					file_image.set_alpha(255);
				else if (file_image.has_alpha())
					has_alpha = true;

				if (has_alpha)
					m_any_source_image_has_alpha = true;
			}

			{
				const uint32_t width = m_params.m_hdr ? file_image_hdr.get_width() : file_image.get_width();
				const uint32_t height = m_params.m_hdr ? file_image_hdr.get_height() : file_image.get_height();

				debug_printf("Source image index %u filename %s %ux%u has alpha: %u\n", source_file_index, pSource_filename, width, height, has_alpha);
			}

			if (m_params.m_y_flip)
			{
				if (m_params.m_hdr)
					file_image_hdr.flip_y();
				else
					file_image.flip_y();
			}

#if DEBUG_EXTRACT_SINGLE_BLOCK
			const uint32_t block_x = 0;
			const uint32_t block_y = 0;

			if (m_params.m_hdr)
			{
				imagef block_image(4, 4);
				block_image_hdr.blit(block_x * 4, block_y * 4, 4, 4, 0, 0, file_image_hdr, 0);
				file_image_hdr = block_image;
			}
			else
			{
				image block_image(4, 4);
				block_image.blit(block_x * 4, block_y * 4, 4, 4, 0, 0, file_image, 0);
				file_image = block_image;
			}
#endif

#if DEBUG_CROP_TEXTURE_TO_64x64
			if (m_params.m_hdr)
				file_image_hdr.resize(64, 64);
			else
				file_image.resize(64, 64);
#endif

			if ((m_params.m_resample_width > 0) && (m_params.m_resample_height > 0))
			{
				int new_width = basisu::minimum<int>(m_params.m_resample_width, BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION);
				int new_height = basisu::minimum<int>(m_params.m_resample_height, BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION);

				debug_printf("Resampling to %ix%i\n", new_width, new_height);

				// TODO: A box filter - kaiser looks too sharp on video. Let the caller control this.
				if (m_params.m_hdr)
				{
					imagef temp_img(new_width, new_height);
					image_resample(file_image_hdr, temp_img, "box"); // "kaiser");
					clean_hdr_image(temp_img);
					temp_img.swap(file_image_hdr);
				}
				else
				{
					image temp_img(new_width, new_height);
					image_resample(file_image, temp_img, m_params.m_perceptual, "box"); // "kaiser");
					temp_img.swap(file_image);
				}
			}
			else if (m_params.m_resample_factor > 0.0f)
			{
				// TODO: A box filter - kaiser looks too sharp on video. Let the caller control this.
				if (m_params.m_hdr)
				{
					int new_width = basisu::minimum<int>(basisu::maximum(1, (int)ceilf(file_image_hdr.get_width() * m_params.m_resample_factor)), BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION);
					int new_height = basisu::minimum<int>(basisu::maximum(1, (int)ceilf(file_image_hdr.get_height() * m_params.m_resample_factor)), BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION);

					debug_printf("Resampling to %ix%i\n", new_width, new_height);

					imagef temp_img(new_width, new_height);
					image_resample(file_image_hdr, temp_img, "box"); // "kaiser");
					clean_hdr_image(temp_img);
					temp_img.swap(file_image_hdr);
				}
				else
				{
					int new_width = basisu::minimum<int>(basisu::maximum(1, (int)ceilf(file_image.get_width() * m_params.m_resample_factor)), BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION);
					int new_height = basisu::minimum<int>(basisu::maximum(1, (int)ceilf(file_image.get_height() * m_params.m_resample_factor)), BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION);

					debug_printf("Resampling to %ix%i\n", new_width, new_height);

					image temp_img(new_width, new_height);
					image_resample(file_image, temp_img, m_params.m_perceptual, "box"); // "kaiser");
					temp_img.swap(file_image);
				}
			}

			const uint32_t width = m_params.m_hdr ? file_image_hdr.get_width() : file_image.get_width();
			const uint32_t height = m_params.m_hdr ? file_image_hdr.get_height() : file_image.get_height();

			if ((!width) || (!height))
			{
				error_printf("basis_compressor::read_source_images: Source image has a zero width and/or height!\n");
				return false;
			}

			if ((width > BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION) || (height > BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION))
			{
				error_printf("basis_compressor::read_source_images: Source image \"%s\" is too large!\n", pSource_filename);
				return false;
			}

			if (!m_params.m_hdr)
				source_images.enlarge(1)->swap(file_image);
			else
				source_images_hdr.enlarge(1)->swap(file_image_hdr);

			source_filenames.push_back(pSource_filename);
		}

		// Check if the caller has generated their own mipmaps. 
		if (m_params.m_hdr)
		{
			if (m_params.m_source_mipmap_images_hdr.size())
			{
				// Make sure they've passed us enough mipmap chains.
				if ((m_params.m_source_images_hdr.size() != m_params.m_source_mipmap_images_hdr.size()) || (total_source_files != m_params.m_source_images_hdr.size()))
				{
					error_printf("basis_compressor::read_source_images(): m_params.m_source_mipmap_images_hdr.size() must equal m_params.m_source_images_hdr.size()!\n");
					return false;
				}
			}
		}
		else 
		{
			if (m_params.m_source_mipmap_images.size())
			{
				// Make sure they've passed us enough mipmap chains.
				if ((m_params.m_source_images.size() != m_params.m_source_mipmap_images.size()) || (total_source_files != m_params.m_source_images.size()))
				{
					error_printf("basis_compressor::read_source_images(): m_params.m_source_mipmap_images.size() must equal m_params.m_source_images.size()!\n");
					return false;
				}

				// Check if any of the user-supplied mipmap levels has alpha.
				if (!m_any_source_image_has_alpha)
				{
					for (uint32_t source_file_index = 0; source_file_index < total_source_files; source_file_index++)
					{
						for (uint32_t mip_index = 0; mip_index < m_params.m_source_mipmap_images[source_file_index].size(); mip_index++)
						{
							const image& mip_img = m_params.m_source_mipmap_images[source_file_index][mip_index];

							// Be sure to take into account any swizzling which will be applied.
							if (mip_img.has_alpha(m_params.m_swizzle[3]))
							{
								m_any_source_image_has_alpha = true;
								break;
							}
						}

						if (m_any_source_image_has_alpha)
							break;
					}
				}
			}
		}

		debug_printf("Any source image has alpha: %u\n", m_any_source_image_has_alpha);

		// Now, for each source image, create the slices corresponding to that image.
		for (uint32_t source_file_index = 0; source_file_index < total_source_files; source_file_index++)
		{
			const std::string &source_filename = source_filenames[source_file_index];
						
			basisu::vector<image> slices;
			basisu::vector<imagef> slices_hdr;
			
			slices.reserve(32);
			slices_hdr.reserve(32);
						
			// The first (largest) mipmap level.
			image *pFile_image = source_images.size() ? &source_images[source_file_index] : nullptr;
			imagef *pFile_image_hdr = source_images_hdr.size() ? &source_images_hdr[source_file_index] : nullptr;
									
			// Reserve a slot for mip0.
			if (m_params.m_hdr)
				slices_hdr.resize(1);
			else
				slices.resize(1);
			
			if ((!m_params.m_hdr) && (m_params.m_source_mipmap_images.size()))
			{
				// User-provided mipmaps for each layer or image in the texture array.
				for (uint32_t mip_index = 0; mip_index < m_params.m_source_mipmap_images[source_file_index].size(); mip_index++)
				{
					image& mip_img = m_params.m_source_mipmap_images[source_file_index][mip_index];

					if ((m_params.m_swizzle[0] != 0) ||
						(m_params.m_swizzle[1] != 1) ||
						(m_params.m_swizzle[2] != 2) ||
						(m_params.m_swizzle[3] != 3))
					{
						// Used for XY normal maps in RG - puts X in color, Y in alpha
						for (uint32_t y = 0; y < mip_img.get_height(); y++)
						{
							for (uint32_t x = 0; x < mip_img.get_width(); x++)
							{
								const color_rgba& c = mip_img(x, y);
								mip_img(x, y).set_noclamp_rgba(c[m_params.m_swizzle[0]], c[m_params.m_swizzle[1]], c[m_params.m_swizzle[2]], c[m_params.m_swizzle[3]]);
							}
						}
					}

					slices.push_back(mip_img);
				}
			}
			else if ((m_params.m_hdr) && (m_params.m_source_mipmap_images_hdr.size()))
			{
				// User-provided mipmaps for each layer or image in the texture array.
				for (uint32_t mip_index = 0; mip_index < m_params.m_source_mipmap_images_hdr[source_file_index].size(); mip_index++)
				{
					imagef& mip_img = m_params.m_source_mipmap_images_hdr[source_file_index][mip_index];

					if ((m_params.m_swizzle[0] != 0) ||
						(m_params.m_swizzle[1] != 1) ||
						(m_params.m_swizzle[2] != 2) ||
						(m_params.m_swizzle[3] != 3))
					{
						// Used for XY normal maps in RG - puts X in color, Y in alpha
						for (uint32_t y = 0; y < mip_img.get_height(); y++)
						{
							for (uint32_t x = 0; x < mip_img.get_width(); x++)
							{
								const vec4F& c = mip_img(x, y);

								// For now, HDR alpha is always 1.0f.
								mip_img(x, y).set(c[m_params.m_swizzle[0]], c[m_params.m_swizzle[1]], c[m_params.m_swizzle[2]], 1.0f); // c[m_params.m_swizzle[3]]);
							}
						}
					}

					clean_hdr_image(mip_img);

					slices_hdr.push_back(mip_img);
				}
			}
			else if (m_params.m_mip_gen)
			{
				// Automatically generate mipmaps.
				if (m_params.m_hdr)
				{
					if (!generate_mipmaps(*pFile_image_hdr, slices_hdr, m_any_source_image_has_alpha))
						return false;
				}
				else
				{
					if (!generate_mipmaps(*pFile_image, slices, m_any_source_image_has_alpha))
						return false;
				}
			}

			// Swap in the largest mipmap level here to avoid copying it, because generate_mips() will change the array.
			// NOTE: file_image is now blank.
			if (m_params.m_hdr)
				slices_hdr[0].swap(*pFile_image_hdr);
			else
				slices[0].swap(*pFile_image);

			uint_vec mip_indices(m_params.m_hdr ? slices_hdr.size() : slices.size());
			for (uint32_t i = 0; i < (m_params.m_hdr ? slices_hdr.size() : slices.size()); i++)
				mip_indices[i] = i;
						
			if ((!m_params.m_hdr) && (m_any_source_image_has_alpha) && (!m_params.m_uastc))
			{
				// For ETC1S, if source has alpha, then even mips will have RGB, and odd mips will have alpha in RGB. 
				basisu::vector<image> alpha_slices;
				uint_vec new_mip_indices;

				alpha_slices.reserve(slices.size() * 2);

				for (uint32_t i = 0; i < slices.size(); i++)
				{
					image lvl_rgb(slices[i]);
					image lvl_a(lvl_rgb);

					for (uint32_t y = 0; y < lvl_a.get_height(); y++)
					{
						for (uint32_t x = 0; x < lvl_a.get_width(); x++)
						{
							uint8_t a = lvl_a(x, y).a;
							lvl_a(x, y).set_noclamp_rgba(a, a, a, 255);
						}
					}
					
					lvl_rgb.set_alpha(255);

					alpha_slices.push_back(lvl_rgb);
					new_mip_indices.push_back(i);

					alpha_slices.push_back(lvl_a);
					new_mip_indices.push_back(i);
				}

				slices.swap(alpha_slices);
				mip_indices.swap(new_mip_indices);
			}

			if (m_params.m_hdr)
			{
				assert(slices_hdr.size() == mip_indices.size());
			}
			else
			{
				assert(slices.size() == mip_indices.size());
			}
					
			for (uint32_t slice_index = 0; slice_index < (m_params.m_hdr ? slices_hdr.size() : slices.size()); slice_index++)
			{
				image *pSlice_image = m_params.m_hdr ? nullptr : &slices[slice_index];
				imagef *pSlice_image_hdr = m_params.m_hdr ? &slices_hdr[slice_index] : nullptr;

				const uint32_t orig_width = m_params.m_hdr ? pSlice_image_hdr->get_width() : pSlice_image->get_width();
				const uint32_t orig_height = m_params.m_hdr ? pSlice_image_hdr->get_height() : pSlice_image->get_height();

				bool is_alpha_slice = false;
				if ((!m_params.m_hdr) && (m_any_source_image_has_alpha))
				{
					if (m_params.m_uastc)
					{
						is_alpha_slice = pSlice_image->has_alpha();
					}
					else
					{
						is_alpha_slice = (slice_index & 1) != 0;
					}
				}

				// Enlarge the source image to 4x4 block boundaries, duplicating edge pixels if necessary to avoid introducing extra colors into blocks.
				if (m_params.m_hdr)
					pSlice_image_hdr->crop_dup_borders(pSlice_image_hdr->get_block_width(4) * 4, pSlice_image_hdr->get_block_height(4) * 4);
				else
					pSlice_image->crop_dup_borders(pSlice_image->get_block_width(4) * 4, pSlice_image->get_block_height(4) * 4);

				if (m_params.m_debug_images)
				{
					if (m_params.m_hdr)
						write_exr(string_format("basis_debug_source_image_%u_slice_%u.exr", source_file_index, slice_index).c_str(), *pSlice_image_hdr, 3, 0);
					else
						save_png(string_format("basis_debug_source_image_%u_slice_%u.png", source_file_index, slice_index).c_str(), *pSlice_image);
				}

				const uint32_t dest_image_index = (m_params.m_hdr ? m_slice_images_hdr.size() : m_slice_images.size());

				enlarge_vector(m_stats, 1);

				if (m_params.m_hdr)
					enlarge_vector(m_slice_images_hdr, 1);
				else
					enlarge_vector(m_slice_images, 1);

				enlarge_vector(m_slice_descs, 1);

				m_stats[dest_image_index].m_filename = source_filename.c_str();
				m_stats[dest_image_index].m_width = orig_width;
				m_stats[dest_image_index].m_height = orig_height;

				debug_printf("****** Slice %u: mip %u, alpha_slice: %u, filename: \"%s\", original: %ux%u actual: %ux%u\n", 
					m_slice_descs.size() - 1, mip_indices[slice_index], is_alpha_slice, source_filename.c_str(), 
					orig_width, orig_height, 
					m_params.m_hdr ? pSlice_image_hdr->get_width() : pSlice_image->get_width(), 
					m_params.m_hdr ? pSlice_image_hdr->get_height() : pSlice_image->get_height());

				basisu_backend_slice_desc& slice_desc = m_slice_descs[dest_image_index];

				slice_desc.m_first_block_index = m_total_blocks;

				slice_desc.m_orig_width = orig_width;
				slice_desc.m_orig_height = orig_height;

				if (m_params.m_hdr)
				{
					slice_desc.m_width = pSlice_image_hdr->get_width();
					slice_desc.m_height = pSlice_image_hdr->get_height();

					slice_desc.m_num_blocks_x = pSlice_image_hdr->get_block_width(4);
					slice_desc.m_num_blocks_y = pSlice_image_hdr->get_block_height(4);
				}
				else
				{
					slice_desc.m_width = pSlice_image->get_width();
					slice_desc.m_height = pSlice_image->get_height();

					slice_desc.m_num_blocks_x = pSlice_image->get_block_width(4);
					slice_desc.m_num_blocks_y = pSlice_image->get_block_height(4);
				}

				slice_desc.m_num_macroblocks_x = (slice_desc.m_num_blocks_x + 1) >> 1;
				slice_desc.m_num_macroblocks_y = (slice_desc.m_num_blocks_y + 1) >> 1;

				slice_desc.m_source_file_index = source_file_index;

				slice_desc.m_mip_index = mip_indices[slice_index];

				slice_desc.m_alpha = is_alpha_slice;
				slice_desc.m_iframe = false;
				if (m_params.m_tex_type == basist::cBASISTexTypeVideoFrames)
				{
					slice_desc.m_iframe = (source_file_index == 0);
				}

				m_total_blocks += slice_desc.m_num_blocks_x * slice_desc.m_num_blocks_y;
				total_macroblocks += slice_desc.m_num_macroblocks_x * slice_desc.m_num_macroblocks_y;

				// Finally, swap in the slice's image to avoid copying it.
				// NOTE: slice_image is now blank.
				if (m_params.m_hdr)
					m_slice_images_hdr[dest_image_index].swap(*pSlice_image_hdr);
				else
					m_slice_images[dest_image_index].swap(*pSlice_image);

			} // slice_index

		} // source_file_index

		debug_printf("Total blocks: %u, Total macroblocks: %u\n", m_total_blocks, total_macroblocks);

		// Make sure we don't have too many slices
		if (m_slice_descs.size() > BASISU_MAX_SLICES)
		{
			error_printf("Too many slices!\n");
			return false;
		}
				
		// Basic sanity check on the slices
		for (uint32_t i = 1; i < m_slice_descs.size(); i++)
		{
			const basisu_backend_slice_desc &prev_slice_desc = m_slice_descs[i - 1];
			const basisu_backend_slice_desc &slice_desc = m_slice_descs[i];

			// Make sure images are in order
			int image_delta = (int)slice_desc.m_source_file_index - (int)prev_slice_desc.m_source_file_index;
			if (image_delta > 1)
				return false;	

			// Make sure mipmap levels are in order
			if (!image_delta)
			{
				int level_delta = (int)slice_desc.m_mip_index - (int)prev_slice_desc.m_mip_index;
				if (level_delta > 1)
					return false;
			}
		}

		if (m_params.m_status_output)
		{
			printf("Total slices: %u\n", (uint32_t)m_slice_descs.size());
		}

		for (uint32_t i = 0; i < m_slice_descs.size(); i++)
		{
			const basisu_backend_slice_desc &slice_desc = m_slice_descs[i];

			if (m_params.m_status_output)
			{
				printf("Slice: %u, alpha: %u, orig width/height: %ux%u, width/height: %ux%u, first_block: %u, image_index: %u, mip_level: %u, iframe: %u\n",
					i, slice_desc.m_alpha, slice_desc.m_orig_width, slice_desc.m_orig_height, 
					slice_desc.m_width, slice_desc.m_height, 
					slice_desc.m_first_block_index, slice_desc.m_source_file_index, slice_desc.m_mip_index, slice_desc.m_iframe);
			}

			if (m_any_source_image_has_alpha)
			{
				// HDR doesn't support alpha yet
				if (m_params.m_hdr)
					return false;

				if (!m_params.m_uastc)
				{
					// For ETC1S, alpha slices must be at odd slice indices.
					if (slice_desc.m_alpha)
					{
						if ((i & 1) == 0)
							return false;

						const basisu_backend_slice_desc& prev_slice_desc = m_slice_descs[i - 1];

						// Make sure previous slice has this image's color data
						if (prev_slice_desc.m_source_file_index != slice_desc.m_source_file_index)
							return false;
						if (prev_slice_desc.m_alpha)
							return false;
						if (prev_slice_desc.m_mip_index != slice_desc.m_mip_index)
							return false;
						if (prev_slice_desc.m_num_blocks_x != slice_desc.m_num_blocks_x)
							return false;
						if (prev_slice_desc.m_num_blocks_y != slice_desc.m_num_blocks_y)
							return false;
					}
					else if (i & 1)
						return false;
				}
			}
			else if (slice_desc.m_alpha)
			{
				return false;
			}

			if ((slice_desc.m_orig_width > slice_desc.m_width) || (slice_desc.m_orig_height > slice_desc.m_height))
				return false;

			if ((slice_desc.m_source_file_index == 0) && (m_params.m_tex_type == basist::cBASISTexTypeVideoFrames))
			{
				if (!slice_desc.m_iframe)
					return false;
			}
		}

		return true;
	}

	// Do some basic validation for 2D arrays, cubemaps, video, and volumes.
	bool basis_compressor::validate_texture_type_constraints() 
	{
		debug_printf("basis_compressor::validate_texture_type_constraints\n");

		// In 2D mode anything goes (each image may have a different resolution and # of mipmap levels).
		if (m_params.m_tex_type == basist::cBASISTexType2D)
			return true;
				
		uint32_t total_basis_images = 0;

		for (uint32_t slice_index = 0; slice_index < (m_params.m_hdr ? m_slice_images_hdr.size() : m_slice_images.size()); slice_index++)
		{
			const basisu_backend_slice_desc &slice_desc = m_slice_descs[slice_index];
				
			total_basis_images = maximum<uint32_t>(total_basis_images, slice_desc.m_source_file_index + 1);
		}

		if (m_params.m_tex_type == basist::cBASISTexTypeCubemapArray)
		{
			// For cubemaps, validate that the total # of Basis images is a multiple of 6.
			if ((total_basis_images % 6) != 0)
			{
				error_printf("basis_compressor::validate_texture_type_constraints: For cubemaps the total number of input images is not a multiple of 6!\n");
				return false;
			}
		}

		// Now validate that all the mip0's have the same dimensions, and that each image has the same # of mipmap levels.
		uint_vec image_mipmap_levels(total_basis_images);

		int width = -1, height = -1;
		for (uint32_t slice_index = 0; slice_index < (m_params.m_hdr ? m_slice_images_hdr.size() : m_slice_images.size()); slice_index++)
		{
			const basisu_backend_slice_desc &slice_desc = m_slice_descs[slice_index];

			image_mipmap_levels[slice_desc.m_source_file_index] = maximum(image_mipmap_levels[slice_desc.m_source_file_index], slice_desc.m_mip_index + 1);
				
			if (slice_desc.m_mip_index != 0)
				continue;

			if (width < 0)
			{
				width = slice_desc.m_orig_width;
				height = slice_desc.m_orig_height;
			}
			else if ((width != (int)slice_desc.m_orig_width) || (height != (int)slice_desc.m_orig_height))
			{
				error_printf("basis_compressor::validate_texture_type_constraints: The source image resolutions are not all equal!\n");
				return false;
			}
		}

		for (size_t i = 1; i < image_mipmap_levels.size(); i++)
		{
			if (image_mipmap_levels[0] != image_mipmap_levels[i])
			{
				error_printf("basis_compressor::validate_texture_type_constraints: Each image must have the same number of mipmap levels!\n");
				return false;
			}
		}

		return true;
	}

	bool basis_compressor::extract_source_blocks()
	{
		debug_printf("basis_compressor::extract_source_blocks\n");

		if (m_params.m_hdr)
			m_source_blocks_hdr.resize(m_total_blocks);
		else
			m_source_blocks.resize(m_total_blocks);

		for (uint32_t slice_index = 0; slice_index < (m_params.m_hdr ? m_slice_images_hdr.size() : m_slice_images.size()); slice_index++)
		{
			const basisu_backend_slice_desc& slice_desc = m_slice_descs[slice_index];

			const uint32_t num_blocks_x = slice_desc.m_num_blocks_x;
			const uint32_t num_blocks_y = slice_desc.m_num_blocks_y;

			const image *pSource_image = m_params.m_hdr ? nullptr : &m_slice_images[slice_index];
			const imagef *pSource_image_hdr = m_params.m_hdr ? &m_slice_images_hdr[slice_index] : nullptr;

			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
			{
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
				{
					if (m_params.m_hdr)
					{
						vec4F* pBlock = m_source_blocks_hdr[slice_desc.m_first_block_index + block_x + block_y * num_blocks_x].get_ptr();

						pSource_image_hdr->extract_block_clamped(pBlock, block_x * 4, block_y * 4, 4, 4);

						// Additional (technically optional) early sanity checking of the block texels.
						for (uint32_t i = 0; i < 16; i++)
						{
							for (uint32_t c = 0; c < 3; c++)
							{
								float v = pBlock[i][c];

								if (std::isnan(v) || std::isinf(v) || (v < 0.0f) || (v > basist::MAX_HALF_FLOAT))
								{
									error_printf("basis_compressor::extract_source_blocks: invalid float component\n");
									return false;
								}
							}
						}
					}
					else
					{
						pSource_image->extract_block_clamped(m_source_blocks[slice_desc.m_first_block_index + block_x + block_y * num_blocks_x].get_ptr(), block_x * 4, block_y * 4, 4, 4);
					}
				}
			}
		}

		return true;
	}

	bool basis_compressor::process_frontend()
	{
		debug_printf("basis_compressor::process_frontend\n");
						
#if 0
		// TODO
		basis_etc1_pack_params pack_params;
		pack_params.m_quality = cETCQualityMedium;
		pack_params.m_perceptual = m_params.m_perceptual;
		pack_params.m_use_color4 = false;

		pack_etc1_block_context pack_context;

		std::unordered_set<uint64_t> endpoint_hash;
		std::unordered_set<uint32_t> selector_hash;

		for (uint32_t i = 0; i < m_source_blocks.size(); i++)
		{
			etc_block blk;
			pack_etc1_block(blk, m_source_blocks[i].get_ptr(), pack_params, pack_context);

			const color_rgba c0(blk.get_block_color(0, false));
			endpoint_hash.insert((c0.r | (c0.g << 5) | (c0.b << 10)) | (blk.get_inten_table(0) << 16));

			const color_rgba c1(blk.get_block_color(1, false));
			endpoint_hash.insert((c1.r | (c1.g << 5) | (c1.b << 10)) | (blk.get_inten_table(1) << 16));

			selector_hash.insert(blk.get_raw_selector_bits());
		}

		const uint32_t total_unique_endpoints = (uint32_t)endpoint_hash.size();
		const uint32_t total_unique_selectors = (uint32_t)selector_hash.size();

		if (m_params.m_debug)
		{
			debug_printf("Unique endpoints: %u, unique selectors: %u\n", total_unique_endpoints, total_unique_selectors);
		}
#endif

		const double total_texels = m_total_blocks * 16.0f;

		int endpoint_clusters = m_params.m_max_endpoint_clusters;
		int selector_clusters = m_params.m_max_selector_clusters;

		if (endpoint_clusters > basisu_frontend::cMaxEndpointClusters)
		{
			error_printf("Too many endpoint clusters! (%u but max is %u)\n", endpoint_clusters, basisu_frontend::cMaxEndpointClusters);
			return false;
		}
		if (selector_clusters > basisu_frontend::cMaxSelectorClusters)
		{
			error_printf("Too many selector clusters! (%u but max is %u)\n", selector_clusters, basisu_frontend::cMaxSelectorClusters);
			return false;
		}
		
		if (m_params.m_quality_level != -1)
		{
			const float quality = saturate(m_params.m_quality_level / 255.0f);
									
			const float bits_per_endpoint_cluster = 14.0f;
			const float max_desired_endpoint_cluster_bits_per_texel = 1.0f; // .15f
			int max_endpoints = static_cast<int>((max_desired_endpoint_cluster_bits_per_texel * total_texels) / bits_per_endpoint_cluster);
			
			const float mid = 128.0f / 255.0f;

			float color_endpoint_quality = quality;

			const float endpoint_split_point = 0.5f;
			
			// In v1.2 and in previous versions, the endpoint codebook size at quality 128 was 3072. This wasn't quite large enough.
			const int ENDPOINT_CODEBOOK_MID_QUALITY_CODEBOOK_SIZE = 4800;
			const int MAX_ENDPOINT_CODEBOOK_SIZE = 8192;

			if (color_endpoint_quality <= mid)
			{
				color_endpoint_quality = lerp(0.0f, endpoint_split_point, powf(color_endpoint_quality / mid, .65f));

				max_endpoints = clamp<int>(max_endpoints, 256, ENDPOINT_CODEBOOK_MID_QUALITY_CODEBOOK_SIZE);
				max_endpoints = minimum<uint32_t>(max_endpoints, m_total_blocks);
								
				if (max_endpoints < 64)
					max_endpoints = 64;
				endpoint_clusters = clamp<uint32_t>((uint32_t)(.5f + lerp<float>(32, static_cast<float>(max_endpoints), color_endpoint_quality)), 32, basisu_frontend::cMaxEndpointClusters);
			}
			else
			{
				color_endpoint_quality = powf((color_endpoint_quality - mid) / (1.0f - mid), 1.6f);

				max_endpoints = clamp<int>(max_endpoints, 256, MAX_ENDPOINT_CODEBOOK_SIZE);
				max_endpoints = minimum<uint32_t>(max_endpoints, m_total_blocks);
								
				if (max_endpoints < ENDPOINT_CODEBOOK_MID_QUALITY_CODEBOOK_SIZE)
					max_endpoints = ENDPOINT_CODEBOOK_MID_QUALITY_CODEBOOK_SIZE;
				endpoint_clusters = clamp<uint32_t>((uint32_t)(.5f + lerp<float>(ENDPOINT_CODEBOOK_MID_QUALITY_CODEBOOK_SIZE, static_cast<float>(max_endpoints), color_endpoint_quality)), 32, basisu_frontend::cMaxEndpointClusters);
			}
						
			float bits_per_selector_cluster = 14.0f;

			const float max_desired_selector_cluster_bits_per_texel = 1.0f; // .15f
			int max_selectors = static_cast<int>((max_desired_selector_cluster_bits_per_texel * total_texels) / bits_per_selector_cluster);
			max_selectors = clamp<int>(max_selectors, 256, basisu_frontend::cMaxSelectorClusters);
			max_selectors = minimum<uint32_t>(max_selectors, m_total_blocks);

			float color_selector_quality = quality;
			//color_selector_quality = powf(color_selector_quality, 1.65f);
			color_selector_quality = powf(color_selector_quality, 2.62f);

			if (max_selectors < 96)
				max_selectors = 96;
			selector_clusters = clamp<uint32_t>((uint32_t)(.5f + lerp<float>(96, static_cast<float>(max_selectors), color_selector_quality)), 8, basisu_frontend::cMaxSelectorClusters);

			debug_printf("Max endpoints: %u, max selectors: %u\n", endpoint_clusters, selector_clusters);

			if (m_params.m_quality_level >= 223)
			{
				if (!m_params.m_selector_rdo_thresh.was_changed())
				{
					if (!m_params.m_endpoint_rdo_thresh.was_changed())
						m_params.m_endpoint_rdo_thresh *= .25f;
					
					if (!m_params.m_selector_rdo_thresh.was_changed())
						m_params.m_selector_rdo_thresh *= .25f;
				}
			}
			else if (m_params.m_quality_level >= 192)
			{
				if (!m_params.m_endpoint_rdo_thresh.was_changed())
					m_params.m_endpoint_rdo_thresh *= .5f;

				if (!m_params.m_selector_rdo_thresh.was_changed())
					m_params.m_selector_rdo_thresh *= .5f;
			}
			else if (m_params.m_quality_level >= 160)
			{
				if (!m_params.m_endpoint_rdo_thresh.was_changed())
					m_params.m_endpoint_rdo_thresh *= .75f;

				if (!m_params.m_selector_rdo_thresh.was_changed())
					m_params.m_selector_rdo_thresh *= .75f;
			}
			else if (m_params.m_quality_level >= 129)
			{
				float l = (quality - 129 / 255.0f) / ((160 - 129) / 255.0f);

				if (!m_params.m_endpoint_rdo_thresh.was_changed())
					m_params.m_endpoint_rdo_thresh *= lerp<float>(1.0f, .75f, l);
				
				if (!m_params.m_selector_rdo_thresh.was_changed())
					m_params.m_selector_rdo_thresh *= lerp<float>(1.0f, .75f, l);
			}
		}
				
		basisu_frontend::params p;
		p.m_num_source_blocks = m_total_blocks;
		p.m_pSource_blocks = &m_source_blocks[0];
		p.m_max_endpoint_clusters = endpoint_clusters;
		p.m_max_selector_clusters = selector_clusters;
		p.m_perceptual = m_params.m_perceptual;
		p.m_debug_stats = m_params.m_debug;
		p.m_debug_images = m_params.m_debug_images;
		p.m_compression_level = m_params.m_compression_level;
		p.m_tex_type = m_params.m_tex_type;
		p.m_multithreaded = m_params.m_multithreading;
		p.m_disable_hierarchical_endpoint_codebooks = m_params.m_disable_hierarchical_endpoint_codebooks;
		p.m_validate = m_params.m_validate_etc1s;
		p.m_pJob_pool = m_params.m_pJob_pool;
		p.m_pGlobal_codebooks = m_params.m_pGlobal_codebooks;
		
		// Don't keep trying to use OpenCL if it ever fails.
		p.m_pOpenCL_context = !m_opencl_failed ? m_pOpenCL_context : nullptr;

		if (!m_frontend.init(p))
		{
			error_printf("basisu_frontend::init() failed!\n");
			return false;
		}
				
		m_frontend.compress();

		if (m_frontend.get_opencl_failed())
			m_opencl_failed = true;

		if (m_params.m_debug_images)
		{
			for (uint32_t i = 0; i < m_slice_descs.size(); i++)
			{
				char filename[1024];
#ifdef _WIN32				
				sprintf_s(filename, sizeof(filename), "rdo_frontend_output_output_blocks_%u.png", i);
#else
				snprintf(filename, sizeof(filename), "rdo_frontend_output_output_blocks_%u.png", i);
#endif				
				m_frontend.dump_debug_image(filename, m_slice_descs[i].m_first_block_index, m_slice_descs[i].m_num_blocks_x, m_slice_descs[i].m_num_blocks_y, true);

#ifdef _WIN32
				sprintf_s(filename, sizeof(filename), "rdo_frontend_output_api_%u.png", i);
#else
				snprintf(filename, sizeof(filename), "rdo_frontend_output_api_%u.png", i);
#endif				
				m_frontend.dump_debug_image(filename, m_slice_descs[i].m_first_block_index, m_slice_descs[i].m_num_blocks_x, m_slice_descs[i].m_num_blocks_y, false);
			}
		}

		return true;
	}

	bool basis_compressor::extract_frontend_texture_data()
	{
		if (!m_params.m_compute_stats)
			return true;

		debug_printf("basis_compressor::extract_frontend_texture_data\n");

		m_frontend_output_textures.resize(m_slice_descs.size());
		m_best_etc1s_images.resize(m_slice_descs.size());
		m_best_etc1s_images_unpacked.resize(m_slice_descs.size());

		for (uint32_t i = 0; i < m_slice_descs.size(); i++)
		{
			const basisu_backend_slice_desc &slice_desc = m_slice_descs[i];

			const uint32_t num_blocks_x = slice_desc.m_num_blocks_x;
			const uint32_t num_blocks_y = slice_desc.m_num_blocks_y;

			const uint32_t width = num_blocks_x * 4;
			const uint32_t height = num_blocks_y * 4;

			m_frontend_output_textures[i].init(texture_format::cETC1, width, height);

			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
					memcpy(m_frontend_output_textures[i].get_block_ptr(block_x, block_y, 0), &m_frontend.get_output_block(slice_desc.m_first_block_index + block_x + block_y * num_blocks_x), sizeof(etc_block));

#if 0
			if (m_params.m_debug_images)
			{
				char filename[1024];
				sprintf_s(filename, sizeof(filename), "rdo_etc_frontend_%u_", i);
				write_etc1_vis_images(m_frontend_output_textures[i], filename);
			}
#endif

			m_best_etc1s_images[i].init(texture_format::cETC1, width, height);
			for (uint32_t block_y = 0; block_y < num_blocks_y; block_y++)
				for (uint32_t block_x = 0; block_x < num_blocks_x; block_x++)
					memcpy(m_best_etc1s_images[i].get_block_ptr(block_x, block_y, 0), &m_frontend.get_etc1s_block(slice_desc.m_first_block_index + block_x + block_y * num_blocks_x), sizeof(etc_block));

			m_best_etc1s_images[i].unpack(m_best_etc1s_images_unpacked[i]);
		}

		return true;
	}

	bool basis_compressor::process_backend()
	{
		debug_printf("basis_compressor::process_backend\n");

		basisu_backend_params backend_params;
		backend_params.m_debug = m_params.m_debug;
		backend_params.m_debug_images = m_params.m_debug_images;
		backend_params.m_etc1s = true;
		backend_params.m_compression_level = m_params.m_compression_level;
		
		if (!m_params.m_no_endpoint_rdo)
			backend_params.m_endpoint_rdo_quality_thresh = m_params.m_endpoint_rdo_thresh;

		if (!m_params.m_no_selector_rdo)
			backend_params.m_selector_rdo_quality_thresh = m_params.m_selector_rdo_thresh;
				
		backend_params.m_used_global_codebooks = m_frontend.get_params().m_pGlobal_codebooks != nullptr;
		backend_params.m_validate = m_params.m_validate_output_data;

		m_backend.init(&m_frontend, backend_params, m_slice_descs);
		uint32_t total_packed_bytes = m_backend.encode();

		if (!total_packed_bytes)
		{
			error_printf("basis_compressor::encode() failed!\n");
			return false;
		}

		debug_printf("Total packed bytes (estimated): %u\n", total_packed_bytes);

		return true;
	}

	bool basis_compressor::create_basis_file_and_transcode()
	{
		debug_printf("basis_compressor::create_basis_file_and_transcode\n");

		const basisu_backend_output& encoded_output = m_params.m_uastc ? m_uastc_backend_output : m_backend.get_output();

		if (!m_basis_file.init(encoded_output, m_params.m_tex_type, m_params.m_userdata0, m_params.m_userdata1, m_params.m_y_flip, m_params.m_us_per_frame))
		{
			error_printf("basis_compressor::create_basis_file_and_transcode: basisu_backend:init() failed!\n");
			return false;
		}
	
		const uint8_vec &comp_data = m_basis_file.get_compressed_data();

		m_output_basis_file = comp_data;

		uint32_t total_orig_pixels = 0, total_texels = 0, total_orig_texels = 0;
		(void)total_texels;

		for (uint32_t i = 0; i < m_slice_descs.size(); i++)
		{
			const basisu_backend_slice_desc& slice_desc = m_slice_descs[i];

			total_orig_pixels += slice_desc.m_orig_width * slice_desc.m_orig_height;
			total_texels += slice_desc.m_width * slice_desc.m_height;
		}

		m_basis_file_size = (uint32_t)comp_data.size();
		m_basis_bits_per_texel = total_orig_texels ? (comp_data.size() * 8.0f) / total_orig_texels : 0;

		debug_printf("Total .basis output file size: %u, %3.3f bits/texel\n", comp_data.size(), comp_data.size() * 8.0f / total_orig_pixels);

		if (m_params.m_validate_output_data)
		{
			interval_timer tm;
			tm.start();

			basist::basisu_transcoder_init();

			debug_printf("basist::basisu_transcoder_init: Took %f ms\n", tm.get_elapsed_ms());

			// Verify the compressed data by transcoding it to ASTC (or ETC1)/BC7 and validating the CRC's.
			basist::basisu_transcoder decoder;
			if (!decoder.validate_file_checksums(&comp_data[0], (uint32_t)comp_data.size(), true))
			{
				error_printf("decoder.validate_file_checksums() failed!\n");
				return false;
			}

			m_decoded_output_textures.resize(m_slice_descs.size());

			if (m_params.m_hdr)
			{
				m_decoded_output_textures_bc6h_hdr_unpacked.resize(m_slice_descs.size());

				m_decoded_output_textures_astc_hdr.resize(m_slice_descs.size());
				m_decoded_output_textures_astc_hdr_unpacked.resize(m_slice_descs.size());
			}
			else
			{
				m_decoded_output_textures_unpacked.resize(m_slice_descs.size());

				m_decoded_output_textures_bc7.resize(m_slice_descs.size());
				m_decoded_output_textures_unpacked_bc7.resize(m_slice_descs.size());
			}

			tm.start();
			if (m_params.m_pGlobal_codebooks)
			{
				decoder.set_global_codebooks(m_params.m_pGlobal_codebooks);
			}

			if (!decoder.start_transcoding(&comp_data[0], (uint32_t)comp_data.size()))
			{
				error_printf("decoder.start_transcoding() failed!\n");
				return false;
			}

			double start_transcoding_time = tm.get_elapsed_secs();

			debug_printf("basisu_compressor::start_transcoding() took %3.3fms\n", start_transcoding_time * 1000.0f);

			double total_time_etc1s_or_astc = 0;

			for (uint32_t i = 0; i < m_slice_descs.size(); i++)
			{
				basisu::texture_format tex_format = m_params.m_hdr ? texture_format::cBC6HUnsigned : (m_params.m_uastc ? texture_format::cUASTC4x4 : texture_format::cETC1);
				basist::block_format format = m_params.m_hdr ? basist::block_format::cBC6H : (m_params.m_uastc ? basist::block_format::cUASTC_4x4 : basist::block_format::cETC1);

				gpu_image decoded_texture;
				decoded_texture.init(
					tex_format, 
					m_slice_descs[i].m_width, m_slice_descs[i].m_height);

				tm.start();
								
				uint32_t bytes_per_block = m_params.m_uastc ? 16 : 8;

				if (!decoder.transcode_slice(&comp_data[0], (uint32_t)comp_data.size(), i,
					reinterpret_cast<etc_block*>(decoded_texture.get_ptr()), m_slice_descs[i].m_num_blocks_x * m_slice_descs[i].m_num_blocks_y, format, bytes_per_block))
				{
					error_printf("Transcoding failed on slice %u!\n", i);
					return false;
				}

				total_time_etc1s_or_astc += tm.get_elapsed_secs();

				if (encoded_output.m_tex_format == basist::basis_tex_format::cETC1S)
				{
					uint32_t image_crc16 = basist::crc16(decoded_texture.get_ptr(), decoded_texture.get_size_in_bytes(), 0);
					if (image_crc16 != encoded_output.m_slice_image_crcs[i])
					{
						error_printf("Decoded image data CRC check failed on slice %u!\n", i);
						return false;
					}
					debug_printf("Decoded image data CRC check succeeded on slice %i\n", i);
				}

				m_decoded_output_textures[i] = decoded_texture;
			}

			double total_alt_transcode_time = 0;
			tm.start();

			if (m_params.m_hdr)
			{
				assert(basist::basis_is_format_supported(basist::transcoder_texture_format::cTFASTC_HDR_4x4_RGBA, basist::basis_tex_format::cUASTC_HDR_4x4));

				for (uint32_t i = 0; i < m_slice_descs.size(); i++)
				{
					gpu_image decoded_texture;
					decoded_texture.init(texture_format::cASTC_HDR_4x4, m_slice_descs[i].m_width, m_slice_descs[i].m_height);

					tm.start();

					if (!decoder.transcode_slice(&comp_data[0], (uint32_t)comp_data.size(), i,
						reinterpret_cast<basist::astc_blk*>(decoded_texture.get_ptr()), m_slice_descs[i].m_num_blocks_x * m_slice_descs[i].m_num_blocks_y, basist::block_format::cASTC_HDR_4x4, 16))
					{
						error_printf("Transcoding failed to ASTC HDR on slice %u!\n", i);
						return false;
					}
										
					m_decoded_output_textures_astc_hdr[i] = decoded_texture;
				}
			}
			else
			{
				if (basist::basis_is_format_supported(basist::transcoder_texture_format::cTFBC7_RGBA, basist::basis_tex_format::cUASTC4x4) &&
					basist::basis_is_format_supported(basist::transcoder_texture_format::cTFBC7_RGBA, basist::basis_tex_format::cETC1S))
				{
					for (uint32_t i = 0; i < m_slice_descs.size(); i++)
					{
						gpu_image decoded_texture;
						decoded_texture.init(texture_format::cBC7, m_slice_descs[i].m_width, m_slice_descs[i].m_height);
												
						if (!decoder.transcode_slice(&comp_data[0], (uint32_t)comp_data.size(), i,
							reinterpret_cast<etc_block*>(decoded_texture.get_ptr()), m_slice_descs[i].m_num_blocks_x * m_slice_descs[i].m_num_blocks_y, basist::block_format::cBC7, 16))
						{
							error_printf("Transcoding failed to BC7 on slice %u!\n", i);
							return false;
						}
												
						m_decoded_output_textures_bc7[i] = decoded_texture;
					}
				}
			}

			total_alt_transcode_time = tm.get_elapsed_secs();

			for (uint32_t i = 0; i < m_slice_descs.size(); i++)
			{
				if (m_params.m_hdr)
				{
					// BC6H
					bool status = m_decoded_output_textures[i].unpack_hdr(m_decoded_output_textures_bc6h_hdr_unpacked[i]);
					assert(status);
					BASISU_NOTE_UNUSED(status);
					
					// ASTC HDR
					status = m_decoded_output_textures_astc_hdr[i].unpack_hdr(m_decoded_output_textures_astc_hdr_unpacked[i]);
					assert(status);
				}
				else
				{
					bool status = m_decoded_output_textures[i].unpack(m_decoded_output_textures_unpacked[i]);
					assert(status);
					BASISU_NOTE_UNUSED(status);

					if (m_decoded_output_textures_bc7[i].get_pixel_width())
					{
						status = m_decoded_output_textures_bc7[i].unpack(m_decoded_output_textures_unpacked_bc7[i]);
						assert(status);
					}
				}
			}

			debug_printf("Transcoded to %s in %3.3fms, %f texels/sec\n", 
				m_params.m_hdr ? "BC6H" : (m_params.m_uastc ? "ASTC" : "ETC1"),
				total_time_etc1s_or_astc * 1000.0f, total_orig_pixels / total_time_etc1s_or_astc);

			if (total_alt_transcode_time != 0)
				debug_printf("Alternate transcode in %3.3fms, %f texels/sec\n", total_alt_transcode_time * 1000.0f, total_orig_pixels / total_alt_transcode_time);

			for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
			{
				const basisu_backend_slice_desc& slice_desc = m_slice_descs[slice_index];

				const uint32_t total_blocks = slice_desc.m_num_blocks_x * slice_desc.m_num_blocks_y;
				BASISU_NOTE_UNUSED(total_blocks);

				assert(m_decoded_output_textures[slice_index].get_total_blocks() == total_blocks);
			}

		} // if (m_params.m_validate_output_data)
				
		return true;
	}

	bool basis_compressor::write_hdr_debug_images(const char* pBasename, const imagef& orig_hdr_img, uint32_t width, uint32_t height)
	{
		// Copy image to account for 4x4 block expansion
		imagef hdr_img(orig_hdr_img);
		hdr_img.resize(width, height);

		image srgb_img(width, height);

		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				vec4F p(hdr_img(x, y));

				p[0] = clamp(p[0], 0.0f, 1.0f);
				p[1] = clamp(p[1], 0.0f, 1.0f);
				p[2] = clamp(p[2], 0.0f, 1.0f);

				int rc = (int)std::round(linear_to_srgb(p[0]) * 255.0f);
				int gc = (int)std::round(linear_to_srgb(p[1]) * 255.0f);
				int bc = (int)std::round(linear_to_srgb(p[2]) * 255.0f);

				srgb_img.set_clipped(x, y, color_rgba(rc, gc, bc, 255));
			}
		}

		{
			const std::string filename(string_format("%s_linear_clamped_to_srgb.png", pBasename));
			save_png(filename.c_str(), srgb_img);
			printf("Wrote .PNG file %s\n", filename.c_str());
		}

		{
			const std::string filename(string_format("%s_compressive_tonemapped.png", pBasename));
			image compressive_tonemapped_img;
			
			bool status = tonemap_image_compressive(compressive_tonemapped_img, hdr_img);
			if (!status)
			{
				error_printf("basis_compressor::write_hdr_debug_images: tonemap_image_compressive() failed (invalid half-float input)\n");
			}
			else
			{
				save_png(filename.c_str(), compressive_tonemapped_img);
				printf("Wrote .PNG file %s\n", filename.c_str());
			}
		}

		image tonemapped_img;

		for (int e = -5; e <= 5; e++)
		{
			const float scale = powf(2.0f, (float)e);

			tonemap_image_reinhard(tonemapped_img, hdr_img, scale);

			std::string filename(string_format("%s_reinhard_tonemapped_scale_%f.png", pBasename, scale));
			save_png(filename.c_str(), tonemapped_img, cImageSaveIgnoreAlpha);
			printf("Wrote .PNG file %s\n", filename.c_str());
		}

		return true;
	}

	bool basis_compressor::write_output_files_and_compute_stats()
	{
		debug_printf("basis_compressor::write_output_files_and_compute_stats\n");

		const uint8_vec& comp_data = m_params.m_create_ktx2_file ? m_output_ktx2_file : m_basis_file.get_compressed_data();
		if (m_params.m_write_output_basis_or_ktx2_files)
		{
			const std::string& output_filename = m_params.m_out_filename;

			if (!write_vec_to_file(output_filename.c_str(), comp_data))
			{
				error_printf("Failed writing output data to file \"%s\"\n", output_filename.c_str());
				return false;
			}

			//if (m_params.m_status_output)
			{
				printf("Wrote output .basis/.ktx2 file \"%s\"\n", output_filename.c_str());
			}
		}

		size_t comp_size = 0;
		if ((m_params.m_compute_stats) && (m_params.m_uastc) && (comp_data.size()))
		{
			void* pComp_data = tdefl_compress_mem_to_heap(&comp_data[0], comp_data.size(), &comp_size, TDEFL_MAX_PROBES_MASK);// TDEFL_DEFAULT_MAX_PROBES);
			size_t decomp_size = 0;
			void* pDecomp_data = tinfl_decompress_mem_to_heap(pComp_data, comp_size, &decomp_size, 0);
			if ((decomp_size != comp_data.size()) || (memcmp(pDecomp_data, &comp_data[0], decomp_size) != 0))
			{
				printf("basis_compressor::create_basis_file_and_transcode:: miniz compression or decompression failed!\n");
				return false;
			}

			mz_free(pComp_data);
			mz_free(pDecomp_data);

			uint32_t total_texels = 0;
			for (uint32_t i = 0; i < m_slice_descs.size(); i++)
				total_texels += (m_slice_descs[i].m_num_blocks_x * m_slice_descs[i].m_num_blocks_y) * 16;
			
			m_basis_bits_per_texel = comp_size * 8.0f / total_texels;

			debug_printf("Output file size: %u, LZ compressed file size: %u, %3.2f bits/texel\n",
				(uint32_t)comp_data.size(),
				(uint32_t)comp_size,
				m_basis_bits_per_texel);
		}

		m_stats.resize(m_slice_descs.size());
		
		if (m_params.m_validate_output_data)
		{
			if (m_params.m_hdr)
			{
				if (m_params.m_print_stats)
				{
					printf("ASTC/BC6H half float space error metrics (a piecewise linear approximation of log2 error):\n");
				}

				for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
				{
					const basisu_backend_slice_desc& slice_desc = m_slice_descs[slice_index];

					if (m_params.m_compute_stats)
					{
						image_stats& s = m_stats[slice_index];

						if (m_params.m_print_stats)
						{
							printf("Slice: %u\n", slice_index);
						}

						image_metrics im;

						if (m_params.m_print_stats)
						{
							printf("\nASTC channels:\n");
							for (uint32_t i = 0; i < 3; i++)
							{
								im.calc_half(m_slice_images_hdr[slice_index], m_decoded_output_textures_astc_hdr_unpacked[slice_index], i, 1, true);

								printf("%c:   ", "RGB"[i]);
								im.print_hp();
							}

							printf("BC6H channels:\n");
							for (uint32_t i = 0; i < 3; i++)
							{
								im.calc_half(m_slice_images_hdr[slice_index], m_decoded_output_textures_bc6h_hdr_unpacked[slice_index], i, 1, true);

								printf("%c:   ", "RGB"[i]);
								im.print_hp();
							}
						}

						im.calc_half(m_slice_images_hdr[slice_index], m_decoded_output_textures_astc_hdr_unpacked[slice_index], 0, 3, true);
						s.m_basis_rgb_avg_psnr = (float)im.m_psnr;

						if (m_params.m_print_stats)
						{
							printf("\nASTC RGB: ");
							im.print_hp();
#if 0
							// Validation
							im.calc_half2(m_slice_images_hdr[slice_index], m_decoded_output_textures_astc_hdr_unpacked[slice_index], 0, 3, true);
							printf("\nASTC RGB (Alt): ");
							im.print_hp();
#endif
						}

						im.calc_half(m_slice_images_hdr[slice_index], m_decoded_output_textures_bc6h_hdr_unpacked[slice_index], 0, 3, true);
						s.m_basis_rgb_avg_bc6h_psnr = (float)im.m_psnr;

						if (m_params.m_print_stats)
						{
							printf("BC6H RGB: ");
							im.print_hp();
							printf("\n");
						}
					}
					
					if (m_params.m_debug_images)
					{
						std::string out_basename;
						if (m_params.m_out_filename.size())
							string_get_filename(m_params.m_out_filename.c_str(), out_basename);
						else if (m_params.m_source_filenames.size())
							string_get_filename(m_params.m_source_filenames[slice_desc.m_source_file_index].c_str(), out_basename);

						string_remove_extension(out_basename);
						out_basename = "basis_debug_" + out_basename + string_format("_slice_%u", slice_index);

						// Write BC6H .DDS file.
						{
							gpu_image bc6h_tex(m_decoded_output_textures[slice_index]);
							bc6h_tex.override_dimensions(slice_desc.m_orig_width, slice_desc.m_orig_height);
							
							std::string filename(out_basename + "_bc6h.dds");
							write_compressed_texture_file(filename.c_str(), bc6h_tex, true);
							printf("Wrote .DDS file %s\n", filename.c_str());
						}

						// Write ASTC .KTX/.astc files. ("astcenc -dh input.astc output.exr" to decode the astc file.)
						{
							gpu_image astc_tex(m_decoded_output_textures_astc_hdr[slice_index]);
							astc_tex.override_dimensions(slice_desc.m_orig_width, slice_desc.m_orig_height);
							
							std::string filename1(out_basename + "_astc.astc");
							write_astc_file(filename1.c_str(), astc_tex.get_ptr(), 4, 4, slice_desc.m_orig_width, slice_desc.m_orig_height);
							printf("Wrote .ASTC file %s\n", filename1.c_str());

							std::string filename2(out_basename + "_astc.ktx");
							write_compressed_texture_file(filename2.c_str(), astc_tex, true);
							printf("Wrote .KTX file %s\n", filename2.c_str());
						}

						// Write unpacked ASTC image to .EXR
						{
							imagef astc_img(m_decoded_output_textures_astc_hdr_unpacked[slice_index]);
							astc_img.resize(slice_desc.m_orig_width, slice_desc.m_orig_height);
							
							std::string filename(out_basename + "_unpacked_astc.exr");
							write_exr(filename.c_str(), astc_img, 3, 0);
							printf("Wrote .EXR file %s\n", filename.c_str());
						}

						// Write unpacked BC6H image to .EXR
						{
							imagef bc6h_img(m_decoded_output_textures_bc6h_hdr_unpacked[slice_index]);
							bc6h_img.resize(slice_desc.m_orig_width, slice_desc.m_orig_height);

							std::string filename(out_basename + "_unpacked_bc6h.exr");
							write_exr(filename.c_str(), bc6h_img, 3, 0);
							printf("Wrote .EXR file %s\n", filename.c_str());
						}

						// Write tonemapped/srgb images
						write_hdr_debug_images((out_basename + "_source").c_str(), m_slice_images_hdr[slice_index], slice_desc.m_orig_width, slice_desc.m_orig_height);
						write_hdr_debug_images((out_basename + "_unpacked_astc").c_str(), m_decoded_output_textures_astc_hdr_unpacked[slice_index], slice_desc.m_orig_width, slice_desc.m_orig_height);
						write_hdr_debug_images((out_basename + "_unpacked_bc6h").c_str(), m_decoded_output_textures_bc6h_hdr_unpacked[slice_index], slice_desc.m_orig_width, slice_desc.m_orig_height);
					}
				}
			}
			else
			{
				for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
				{
					const basisu_backend_slice_desc& slice_desc = m_slice_descs[slice_index];

					if (m_params.m_compute_stats)
					{
						if (m_params.m_print_stats)
							printf("Slice: %u\n", slice_index);

						image_stats& s = m_stats[slice_index];
												
						image_metrics em;

						// ---- .basis stats
						em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked[slice_index], 0, 3);
						if (m_params.m_print_stats)
							em.print(".basis RGB Avg:          ");
						s.m_basis_rgb_avg_psnr = (float)em.m_psnr;

						em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked[slice_index], 0, 4);
						if (m_params.m_print_stats)
							em.print(".basis RGBA Avg:         ");
						s.m_basis_rgba_avg_psnr = (float)em.m_psnr;

						em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked[slice_index], 0, 1);
						if (m_params.m_print_stats)
							em.print(".basis R   Avg:          ");

						em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked[slice_index], 1, 1);
						if (m_params.m_print_stats)
							em.print(".basis G   Avg:          ");

						em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked[slice_index], 2, 1);
						if (m_params.m_print_stats)
							em.print(".basis B   Avg:          ");

						if (m_params.m_uastc)
						{
							em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked[slice_index], 3, 1);
							if (m_params.m_print_stats)
								em.print(".basis A   Avg:          ");

							s.m_basis_a_avg_psnr = (float)em.m_psnr;
						}

						em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked[slice_index], 0, 0);
						if (m_params.m_print_stats)
							em.print(".basis 709 Luma:         ");
						s.m_basis_luma_709_psnr = static_cast<float>(em.m_psnr);
						s.m_basis_luma_709_ssim = static_cast<float>(em.m_ssim);

						em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked[slice_index], 0, 0, true, true);
						if (m_params.m_print_stats)
							em.print(".basis 601 Luma:         ");
						s.m_basis_luma_601_psnr = static_cast<float>(em.m_psnr);

						if (m_slice_descs.size() == 1)
						{
							const uint32_t output_size = comp_size ? (uint32_t)comp_size : (uint32_t)comp_data.size();
							if (m_params.m_print_stats)
							{
								debug_printf(".basis RGB PSNR per bit/texel*10000: %3.3f\n", 10000.0f * s.m_basis_rgb_avg_psnr / ((output_size * 8.0f) / (slice_desc.m_orig_width * slice_desc.m_orig_height)));
								debug_printf(".basis Luma 709 PSNR per bit/texel*10000: %3.3f\n", 10000.0f * s.m_basis_luma_709_psnr / ((output_size * 8.0f) / (slice_desc.m_orig_width * slice_desc.m_orig_height)));
							}
						}

						if (m_decoded_output_textures_unpacked_bc7[slice_index].get_width())
						{
							// ---- BC7 stats
							em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked_bc7[slice_index], 0, 3);
							//if (m_params.m_print_stats)
							//	em.print("BC7 RGB Avg:             ");
							s.m_bc7_rgb_avg_psnr = (float)em.m_psnr;

							em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked_bc7[slice_index], 0, 4);
							//if (m_params.m_print_stats)
							//	em.print("BC7 RGBA Avg:            ");
							s.m_bc7_rgba_avg_psnr = (float)em.m_psnr;

							em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked_bc7[slice_index], 0, 1);
							//if (m_params.m_print_stats)
							//	em.print("BC7 R   Avg:             ");

							em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked_bc7[slice_index], 1, 1);
							//if (m_params.m_print_stats)
							//	em.print("BC7 G   Avg:             ");

							em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked_bc7[slice_index], 2, 1);
							//if (m_params.m_print_stats)
							//	em.print("BC7 B   Avg:             ");

							if (m_params.m_uastc)
							{
								em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked_bc7[slice_index], 3, 1);
								//if (m_params.m_print_stats)
								//	em.print("BC7 A   Avg:             ");

								s.m_bc7_a_avg_psnr = (float)em.m_psnr;
							}

							em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked_bc7[slice_index], 0, 0);
							//if (m_params.m_print_stats)
							//	em.print("BC7 709 Luma:            ");
							s.m_bc7_luma_709_psnr = static_cast<float>(em.m_psnr);
							s.m_bc7_luma_709_ssim = static_cast<float>(em.m_ssim);

							em.calc(m_slice_images[slice_index], m_decoded_output_textures_unpacked_bc7[slice_index], 0, 0, true, true);
							//if (m_params.m_print_stats)
							//	em.print("BC7 601 Luma:            ");
							s.m_bc7_luma_601_psnr = static_cast<float>(em.m_psnr);
						}

						if (!m_params.m_uastc)
						{
							// ---- Nearly best possible ETC1S stats
							em.calc(m_slice_images[slice_index], m_best_etc1s_images_unpacked[slice_index], 0, 3);
							//if (m_params.m_print_stats)
							//	em.print("Unquantized ETC1S RGB Avg:     ");
							s.m_best_etc1s_rgb_avg_psnr = static_cast<float>(em.m_psnr);

							em.calc(m_slice_images[slice_index], m_best_etc1s_images_unpacked[slice_index], 0, 0);
							//if (m_params.m_print_stats)
							//	em.print("Unquantized ETC1S 709 Luma:    ");
							s.m_best_etc1s_luma_709_psnr = static_cast<float>(em.m_psnr);
							s.m_best_etc1s_luma_709_ssim = static_cast<float>(em.m_ssim);

							em.calc(m_slice_images[slice_index], m_best_etc1s_images_unpacked[slice_index], 0, 0, true, true);
							//if (m_params.m_print_stats)
							//	em.print("Unquantized ETC1S 601 Luma:    ");
							s.m_best_etc1s_luma_601_psnr = static_cast<float>(em.m_psnr);
						}
					}

					std::string out_basename;
					if (m_params.m_out_filename.size())
						string_get_filename(m_params.m_out_filename.c_str(), out_basename);
					else if (m_params.m_source_filenames.size())
						string_get_filename(m_params.m_source_filenames[slice_desc.m_source_file_index].c_str(), out_basename);

					string_remove_extension(out_basename);
					out_basename = "basis_debug_" + out_basename + string_format("_slice_%u", slice_index);

					if ((!m_params.m_uastc) && (m_frontend.get_params().m_debug_images))
					{
						// Write "best" ETC1S debug images
						if (!m_params.m_uastc)
						{
							gpu_image best_etc1s_gpu_image(m_best_etc1s_images[slice_index]);
							best_etc1s_gpu_image.override_dimensions(slice_desc.m_orig_width, slice_desc.m_orig_height);
							write_compressed_texture_file((out_basename + "_best_etc1s.ktx").c_str(), best_etc1s_gpu_image, true);

							image best_etc1s_unpacked;
							best_etc1s_gpu_image.unpack(best_etc1s_unpacked);
							save_png(out_basename + "_best_etc1s.png", best_etc1s_unpacked);
						}
					}

					if (m_params.m_debug_images)
					{
						// Write decoded ETC1S/ASTC debug images
						{
							gpu_image decoded_etc1s_or_astc(m_decoded_output_textures[slice_index]);
							decoded_etc1s_or_astc.override_dimensions(slice_desc.m_orig_width, slice_desc.m_orig_height);
							write_compressed_texture_file((out_basename + "_transcoded_etc1s_or_astc.ktx").c_str(), decoded_etc1s_or_astc, true);

							image temp(m_decoded_output_textures_unpacked[slice_index]);
							temp.crop(slice_desc.m_orig_width, slice_desc.m_orig_height);
							save_png(out_basename + "_transcoded_etc1s_or_astc.png", temp);
						}

						// Write decoded BC7 debug images
						if (m_decoded_output_textures_bc7[slice_index].get_pixel_width())
						{
							gpu_image decoded_bc7(m_decoded_output_textures_bc7[slice_index]);
							decoded_bc7.override_dimensions(slice_desc.m_orig_width, slice_desc.m_orig_height);
							write_compressed_texture_file((out_basename + "_transcoded_bc7.ktx").c_str(), decoded_bc7, true);

							image temp(m_decoded_output_textures_unpacked_bc7[slice_index]);
							temp.crop(slice_desc.m_orig_width, slice_desc.m_orig_height);
							save_png(out_basename + "_transcoded_bc7.png", temp);
						}
					}
				}
			} // if (m_params.m_hdr)

		} // if (m_params.m_validate_output_data)
				
		return true;
	}
	
	// Make sure all the mip 0's have the same dimensions and number of mipmap levels, or we can't encode the KTX2 file.
	bool basis_compressor::validate_ktx2_constraints()
	{
		uint32_t base_width = 0, base_height = 0;
		uint32_t total_layers = 0;
		for (uint32_t i = 0; i < m_slice_descs.size(); i++)
		{
			if (m_slice_descs[i].m_mip_index == 0)
			{
				if (!base_width)
				{
					base_width = m_slice_descs[i].m_orig_width;
					base_height = m_slice_descs[i].m_orig_height;
				}
				else
				{
					if ((m_slice_descs[i].m_orig_width != base_width) || (m_slice_descs[i].m_orig_height != base_height))
					{
						return false;
					}
				}

				total_layers = maximum<uint32_t>(total_layers, m_slice_descs[i].m_source_file_index + 1);
			}
		}

		basisu::vector<uint32_t> total_mips(total_layers);
		for (uint32_t i = 0; i < m_slice_descs.size(); i++)
			total_mips[m_slice_descs[i].m_source_file_index] = maximum<uint32_t>(total_mips[m_slice_descs[i].m_source_file_index], m_slice_descs[i].m_mip_index + 1);

		for (uint32_t i = 1; i < total_layers; i++)
		{
			if (total_mips[0] != total_mips[i])
			{
				return false;
			}
		}

		return true;
	}

	static uint8_t g_ktx2_etc1s_nonalpha_dfd[44] = { 0x2C,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x28,0x0,0xA3,0x1,0x2,0x0,0x3,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3F,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xFF,0xFF,0xFF,0xFF };
	static uint8_t g_ktx2_etc1s_alpha_dfd[60]    = { 0x3C,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x38,0x0,0xA3,0x1,0x2,0x0,0x3,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x3F,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xFF,0xFF,0xFF,0xFF,0x40,0x0,0x3F,0xF,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xFF,0xFF,0xFF,0xFF };
	
	static uint8_t g_ktx2_uastc_nonalpha_dfd[44] = { 0x2C,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x28,0x0,0xA6,0x1,0x2,0x0,0x3,0x3,0x0,0x0,0x10,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x7F,0x4,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xFF,0xFF,0xFF,0xFF };
	static uint8_t g_ktx2_uastc_alpha_dfd[44]    = { 0x2C,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x2,0x0,0x28,0x0,0xA6,0x1,0x2,0x0,0x3,0x3,0x0,0x0,0x10,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x7F,0x3,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xFF,0xFF,0xFF,0xFF };

	// HDR TODO - what is the best Khronos DFD to use for UASTC HDR?
	static uint8_t g_ktx2_uastc_hdr_nonalpha_dfd[44] = 
	{
		0x2C,0x0,0x0,0x0,		// 0 totalSize
		0x0,0x0,0x0,0x0,		// 1 descriptorType/vendorId
		0x2,0x0,0x28,0x0,		// 2 descriptorBlockSize/versionNumber
		0xA7,0x1,0x1,0x0,		// 3 flags, transferFunction, colorPrimaries, colorModel
		0x3,0x3,0x0,0x0,		// 4 texelBlockDimension0-texelBlockDimension3
		0x10,0x0,0x0,0x0,		// 5 bytesPlane0-bytesPlane3
		0x0,0x0,0x0,0x0,		// 6 bytesPlane4-bytesPlane7
		0x0,0x0,0x7F,0x80,		// 7 bitLength/bitOffset/channelType and Qualifer flags (KHR_DF_SAMPLE_DATATYPE_FLOAT etc.)
		0x0,0x0,0x0,0x0,		// 8 samplePosition0-samplePosition3
		0x0,0x0,0x0,0x0,		// 9 sampleLower (0.0)
		0x00, 0x00, 0x80, 0x3F  // 10 sampleHigher (1.0)
	};
			
	void basis_compressor::get_dfd(uint8_vec &dfd, const basist::ktx2_header &header)
	{
		const uint8_t* pDFD;
		uint32_t dfd_len;

		if (m_params.m_uastc)
		{
			if (m_params.m_hdr)
			{
				pDFD = g_ktx2_uastc_hdr_nonalpha_dfd;
				dfd_len = sizeof(g_ktx2_uastc_hdr_nonalpha_dfd);
			}
			else if (m_any_source_image_has_alpha)
			{
				pDFD = g_ktx2_uastc_alpha_dfd;
				dfd_len = sizeof(g_ktx2_uastc_alpha_dfd);
			}
			else
			{
				pDFD = g_ktx2_uastc_nonalpha_dfd;
				dfd_len = sizeof(g_ktx2_uastc_nonalpha_dfd);
			}
		}
		else
		{
			if (m_any_source_image_has_alpha)
			{
				pDFD = g_ktx2_etc1s_alpha_dfd;
				dfd_len = sizeof(g_ktx2_etc1s_alpha_dfd);
			}
			else
			{
				pDFD = g_ktx2_etc1s_nonalpha_dfd;
				dfd_len = sizeof(g_ktx2_etc1s_nonalpha_dfd);
			}
		}
				
		assert(dfd_len >= 44);

		dfd.resize(dfd_len);
		memcpy(dfd.data(), pDFD, dfd_len);

		uint32_t dfd_bits = basisu::read_le_dword(dfd.data() + 3 * sizeof(uint32_t));
		
		dfd_bits &= ~(0xFF << 16);

		if (m_params.m_hdr)
		{
			// TODO: In HDR mode, always write linear for now.
			dfd_bits |= (basist::KTX2_KHR_DF_TRANSFER_LINEAR << 16);
		}
		else
		{
			if (m_params.m_ktx2_srgb_transfer_func)
				dfd_bits |= (basist::KTX2_KHR_DF_TRANSFER_SRGB << 16);
			else
				dfd_bits |= (basist::KTX2_KHR_DF_TRANSFER_LINEAR << 16);
		}

		basisu::write_le_dword(dfd.data() + 3 * sizeof(uint32_t), dfd_bits);

		if (header.m_supercompression_scheme != basist::KTX2_SS_NONE)
		{
			uint32_t plane_bits = basisu::read_le_dword(dfd.data() + 5 * sizeof(uint32_t));

			plane_bits &= ~0xFF;

			basisu::write_le_dword(dfd.data() + 5 * sizeof(uint32_t), plane_bits);
		}

		// Fix up the DFD channel(s)
		uint32_t dfd_chan0 = basisu::read_le_dword(dfd.data() + 7 * sizeof(uint32_t));

		if (m_params.m_uastc)
		{
			dfd_chan0 &= ~(0xF << 24);
			
			// TODO: Allow the caller to override this
			if (m_any_source_image_has_alpha)
				dfd_chan0 |= (basist::KTX2_DF_CHANNEL_UASTC_RGBA << 24);
			else
				dfd_chan0 |= (basist::KTX2_DF_CHANNEL_UASTC_RGB << 24);
		}

		basisu::write_le_dword(dfd.data() + 7 * sizeof(uint32_t), dfd_chan0);
	}

	bool basis_compressor::create_ktx2_file()
	{
		if (m_params.m_uastc)
		{
			if ((m_params.m_ktx2_uastc_supercompression != basist::KTX2_SS_NONE) && (m_params.m_ktx2_uastc_supercompression != basist::KTX2_SS_ZSTANDARD))
				return false;
		}

		const basisu_backend_output& backend_output = m_backend.get_output();

		// Determine the width/height, number of array layers, mipmap levels, and the number of faces (1 for 2D, 6 for cubemap).
		// This does not support 1D or 3D.
		uint32_t base_width = 0, base_height = 0, total_layers = 0, total_levels = 0, total_faces = 1;
				
		for (uint32_t i = 0; i < m_slice_descs.size(); i++)
		{
			if ((m_slice_descs[i].m_mip_index == 0) && (!base_width))
			{
				base_width = m_slice_descs[i].m_orig_width;
				base_height = m_slice_descs[i].m_orig_height;
			}

			total_layers = maximum<uint32_t>(total_layers, m_slice_descs[i].m_source_file_index + 1);

			if (!m_slice_descs[i].m_source_file_index)
				total_levels = maximum<uint32_t>(total_levels, m_slice_descs[i].m_mip_index + 1);
		}

		if (m_params.m_tex_type == basist::cBASISTexTypeCubemapArray)
		{
			assert((total_layers % 6) == 0);
			
			total_layers /= 6;
			assert(total_layers >= 1);

			total_faces = 6;
		}

		basist::ktx2_header header;
		memset(&header, 0, sizeof(header));

		memcpy(header.m_identifier, basist::g_ktx2_file_identifier, sizeof(basist::g_ktx2_file_identifier));
		header.m_pixel_width = base_width;
		header.m_pixel_height = base_height;
		header.m_face_count = total_faces;
		
		if (m_params.m_hdr)
			header.m_vk_format = basist::KTX2_FORMAT_UASTC_4x4_SFLOAT_BLOCK;
		else
			header.m_vk_format = basist::KTX2_VK_FORMAT_UNDEFINED;

		header.m_type_size = 1;
		header.m_level_count = total_levels;
		header.m_layer_count = (total_layers > 1) ? total_layers : 0;

		if (m_params.m_uastc)
		{
			switch (m_params.m_ktx2_uastc_supercompression)
			{
			case basist::KTX2_SS_NONE:
			{
				header.m_supercompression_scheme = basist::KTX2_SS_NONE;
				break;
			}
			case basist::KTX2_SS_ZSTANDARD:
			{
#if BASISD_SUPPORT_KTX2_ZSTD
				header.m_supercompression_scheme = basist::KTX2_SS_ZSTANDARD;
#else
				header.m_supercompression_scheme = basist::KTX2_SS_NONE;
#endif
				break;
			}
			default: assert(0); return false;
			}
		}

		basisu::vector<uint8_vec> level_data_bytes(total_levels);
		basisu::vector<uint8_vec> compressed_level_data_bytes(total_levels);
		uint_vec slice_level_offsets(m_slice_descs.size());

		// This will append the texture data in the correct order (for each level: layer, then face).
		for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
		{
			const basisu_backend_slice_desc& slice_desc = m_slice_descs[slice_index];

			slice_level_offsets[slice_index] = level_data_bytes[slice_desc.m_mip_index].size();

			if (m_params.m_uastc)
				append_vector(level_data_bytes[slice_desc.m_mip_index], m_uastc_backend_output.m_slice_image_data[slice_index]);
			else
				append_vector(level_data_bytes[slice_desc.m_mip_index], backend_output.m_slice_image_data[slice_index]);
		}

		// UASTC supercompression
		if ((m_params.m_uastc) && (header.m_supercompression_scheme == basist::KTX2_SS_ZSTANDARD))
		{
#if BASISD_SUPPORT_KTX2_ZSTD
			for (uint32_t level_index = 0; level_index < total_levels; level_index++)
			{
				compressed_level_data_bytes[level_index].resize(ZSTD_compressBound(level_data_bytes[level_index].size()));

				size_t result = ZSTD_compress(compressed_level_data_bytes[level_index].data(), compressed_level_data_bytes[level_index].size(),
					level_data_bytes[level_index].data(), level_data_bytes[level_index].size(),
					m_params.m_ktx2_zstd_supercompression_level);

				if (ZSTD_isError(result))
					return false;

				compressed_level_data_bytes[level_index].resize(result);
			}
#else
			// Can't get here
			assert(0);
			return false;
#endif
		}
		else
		{
			// No supercompression
			compressed_level_data_bytes = level_data_bytes;
		}
				
		uint8_vec etc1s_global_data;

		// Create ETC1S global supercompressed data
		if (!m_params.m_uastc)
		{
			basist::ktx2_etc1s_global_data_header etc1s_global_data_header;
			clear_obj(etc1s_global_data_header);

			etc1s_global_data_header.m_endpoint_count = backend_output.m_num_endpoints;
			etc1s_global_data_header.m_selector_count = backend_output.m_num_selectors;
			etc1s_global_data_header.m_endpoints_byte_length = backend_output.m_endpoint_palette.size();
			etc1s_global_data_header.m_selectors_byte_length = backend_output.m_selector_palette.size();
			etc1s_global_data_header.m_tables_byte_length = backend_output.m_slice_image_tables.size();

			basisu::vector<basist::ktx2_etc1s_image_desc> etc1s_image_descs(total_levels * total_layers * total_faces);
			memset(etc1s_image_descs.data(), 0, etc1s_image_descs.size_in_bytes());

			for (uint32_t slice_index = 0; slice_index < m_slice_descs.size(); slice_index++)
			{
				const basisu_backend_slice_desc& slice_desc = m_slice_descs[slice_index];

				const uint32_t level_index = slice_desc.m_mip_index;
				uint32_t layer_index = slice_desc.m_source_file_index;
				uint32_t face_index = 0;

				if (m_params.m_tex_type == basist::cBASISTexTypeCubemapArray)
				{
					face_index = layer_index % 6;
					layer_index /= 6;
				}

				const uint32_t etc1s_image_index = level_index * (total_layers * total_faces) + layer_index * total_faces + face_index;

				if (slice_desc.m_alpha)
				{
					etc1s_image_descs[etc1s_image_index].m_alpha_slice_byte_length = backend_output.m_slice_image_data[slice_index].size();
					etc1s_image_descs[etc1s_image_index].m_alpha_slice_byte_offset = slice_level_offsets[slice_index];
				}
				else
				{
					if (m_params.m_tex_type == basist::cBASISTexTypeVideoFrames)
						etc1s_image_descs[etc1s_image_index].m_image_flags = !slice_desc.m_iframe ? basist::KTX2_IMAGE_IS_P_FRAME : 0;

					etc1s_image_descs[etc1s_image_index].m_rgb_slice_byte_length = backend_output.m_slice_image_data[slice_index].size();
					etc1s_image_descs[etc1s_image_index].m_rgb_slice_byte_offset = slice_level_offsets[slice_index];
				}
			} // slice_index

			append_vector(etc1s_global_data, (const uint8_t*)&etc1s_global_data_header, sizeof(etc1s_global_data_header));
			append_vector(etc1s_global_data, (const uint8_t*)etc1s_image_descs.data(), etc1s_image_descs.size_in_bytes());
			append_vector(etc1s_global_data, backend_output.m_endpoint_palette);
			append_vector(etc1s_global_data, backend_output.m_selector_palette);
			append_vector(etc1s_global_data, backend_output.m_slice_image_tables);
			
			header.m_supercompression_scheme = basist::KTX2_SS_BASISLZ;
		}

		// Key values
		basist::ktx2_transcoder::key_value_vec key_values(m_params.m_ktx2_key_values);
		key_values.enlarge(1);
		
		const char* pKTXwriter = "KTXwriter";
		key_values.back().m_key.resize(strlen(pKTXwriter) + 1);
		memcpy(key_values.back().m_key.data(), pKTXwriter, strlen(pKTXwriter) + 1);

		char writer_id[128];
#ifdef _MSC_VER
		sprintf_s(writer_id, sizeof(writer_id), "Basis Universal %s", BASISU_LIB_VERSION_STRING);
#else
		snprintf(writer_id, sizeof(writer_id), "Basis Universal %s", BASISU_LIB_VERSION_STRING);
#endif
		key_values.back().m_value.resize(strlen(writer_id) + 1);
		memcpy(key_values.back().m_value.data(), writer_id, strlen(writer_id) + 1);

		key_values.sort();

#if BASISU_DISABLE_KTX2_KEY_VALUES
		// HACK HACK - Clear the key values array, which causes no key values to be written (triggering the ktx2check validator bug).
		key_values.clear();
#endif

		uint8_vec key_value_data;

		// DFD
		uint8_vec dfd;
		get_dfd(dfd, header);

		const uint32_t kvd_file_offset = sizeof(header) + sizeof(basist::ktx2_level_index) * total_levels + dfd.size();

		for (uint32_t pass = 0; pass < 2; pass++)
		{
			for (uint32_t i = 0; i < key_values.size(); i++)
			{
				if (key_values[i].m_key.size() < 2)
					return false;

				if (key_values[i].m_key.back() != 0)
					return false;

				const uint64_t total_len = (uint64_t)key_values[i].m_key.size() + (uint64_t)key_values[i].m_value.size();
				if (total_len >= UINT32_MAX)
					return false;

				packed_uint<4> le_len((uint32_t)total_len);
				append_vector(key_value_data, (const uint8_t*)&le_len, sizeof(le_len));

				append_vector(key_value_data, key_values[i].m_key);
				append_vector(key_value_data, key_values[i].m_value);

				const uint32_t ofs = key_value_data.size() & 3;
				const uint32_t padding = (4 - ofs) & 3;
				for (uint32_t p = 0; p < padding; p++)
					key_value_data.push_back(0);
			}

			if (header.m_supercompression_scheme != basist::KTX2_SS_NONE)
				break;

#if BASISU_DISABLE_KTX2_ALIGNMENT_WORKAROUND
			break;
#endif
			
			// Hack to ensure the KVD block ends on a 16 byte boundary, because we have no other official way of aligning the data.
			uint32_t kvd_end_file_offset = kvd_file_offset + key_value_data.size();
			uint32_t bytes_needed_to_pad = (16 - (kvd_end_file_offset & 15)) & 15;
			if (!bytes_needed_to_pad)
			{
				// We're good. No need to add a dummy key.
				break;
			}

			assert(!pass);
			if (pass)
				return false;

			if (bytes_needed_to_pad < 6)
				bytes_needed_to_pad += 16;

			// Just add the padding. It's likely not necessary anymore, but can't really hurt.
			//printf("WARNING: Due to a KTX2 validator bug related to mipPadding, we must insert a dummy key into the KTX2 file of %u bytes\n", bytes_needed_to_pad);
			
			// We're not good - need to add a dummy key large enough to force file alignment so the mip level array gets aligned. 
			// We can't just add some bytes before the mip level array because ktx2check will see that as extra data in the file that shouldn't be there in ktxValidator::validateDataSize().
			key_values.enlarge(1);
			for (uint32_t i = 0; i < (bytes_needed_to_pad - 4 - 1 - 1); i++)
				key_values.back().m_key.push_back(127);
			
			key_values.back().m_key.push_back(0);

			key_values.back().m_value.push_back(0);

			key_values.sort();

			key_value_data.resize(0);
			
			// Try again
		}

		basisu::vector<basist::ktx2_level_index> level_index_array(total_levels);
		memset(level_index_array.data(), 0, level_index_array.size_in_bytes());
				
		m_output_ktx2_file.clear();
		m_output_ktx2_file.reserve(m_output_basis_file.size());

		// Dummy header
		m_output_ktx2_file.resize(sizeof(header));

		// Level index array
		append_vector(m_output_ktx2_file, (const uint8_t*)level_index_array.data(), level_index_array.size_in_bytes());
				
		// DFD
		const uint8_t* pDFD = dfd.data();
		uint32_t dfd_len = dfd.size();

		header.m_dfd_byte_offset = m_output_ktx2_file.size();
		header.m_dfd_byte_length = dfd_len;
		append_vector(m_output_ktx2_file, pDFD, dfd_len);

		// Key value data
		if (key_value_data.size())
		{
			assert(kvd_file_offset == m_output_ktx2_file.size());

			header.m_kvd_byte_offset = m_output_ktx2_file.size();
			header.m_kvd_byte_length = key_value_data.size();
			append_vector(m_output_ktx2_file, key_value_data);
		}

		// Global Supercompressed Data
		if (etc1s_global_data.size())
		{
			uint32_t ofs = m_output_ktx2_file.size() & 7;
			uint32_t padding = (8 - ofs) & 7;
			for (uint32_t i = 0; i < padding; i++)
				m_output_ktx2_file.push_back(0);

			header.m_sgd_byte_length = etc1s_global_data.size();
			header.m_sgd_byte_offset = m_output_ktx2_file.size();

			append_vector(m_output_ktx2_file, etc1s_global_data);
		}

		// mipPadding
		if (header.m_supercompression_scheme == basist::KTX2_SS_NONE)
		{
			// We currently can't do this or the validator will incorrectly give an error.
			uint32_t ofs = m_output_ktx2_file.size() & 15;
			uint32_t padding = (16 - ofs) & 15;

			// Make sure we're always aligned here (due to a validator bug).
			if (padding)
			{
				printf("Warning: KTX2 mip level data is not 16-byte aligned. This may trigger a ktx2check validation bug. Writing %u bytes of mipPadding.\n", padding);
			}

			for (uint32_t i = 0; i < padding; i++)
				m_output_ktx2_file.push_back(0);
		}

		// Level data - write the smallest mipmap first.
		for (int level = total_levels - 1; level >= 0; level--)
		{
			level_index_array[level].m_byte_length = compressed_level_data_bytes[level].size();
			if (m_params.m_uastc)
				level_index_array[level].m_uncompressed_byte_length = level_data_bytes[level].size();

			level_index_array[level].m_byte_offset = m_output_ktx2_file.size();
			append_vector(m_output_ktx2_file, compressed_level_data_bytes[level]);
		}
		
		// Write final header
		memcpy(m_output_ktx2_file.data(), &header, sizeof(header));

		// Write final level index array
		memcpy(m_output_ktx2_file.data() + sizeof(header), level_index_array.data(), level_index_array.size_in_bytes());

		debug_printf("Total .ktx2 output file size: %u\n", m_output_ktx2_file.size());

		return true;
	}

	bool basis_parallel_compress(
		uint32_t total_threads,
		const basisu::vector<basis_compressor_params>& params_vec,
		basisu::vector< parallel_results >& results_vec)
	{
		assert(g_library_initialized);
		if (!g_library_initialized)
		{
			error_printf("basis_parallel_compress: basisu_encoder_init() MUST be called before using any encoder functionality!\n");
			return false;
		}

		assert(total_threads >= 1);
		total_threads = basisu::maximum<uint32_t>(total_threads, 1);

		job_pool jpool(total_threads);

		results_vec.resize(0);
		results_vec.resize(params_vec.size());

		std::atomic<bool> result;
		result = true;
		
		std::atomic<bool> opencl_failed;
		opencl_failed = false;

		for (uint32_t pindex = 0; pindex < params_vec.size(); pindex++)
		{
			jpool.add_job([pindex, &params_vec, &results_vec, &result, &opencl_failed] {

				basis_compressor_params params = params_vec[pindex];
				parallel_results& results = results_vec[pindex];

				interval_timer tm;
				tm.start();

				basis_compressor c;
				
				// Dummy job pool
				job_pool task_jpool(1);
				params.m_pJob_pool = &task_jpool;
				// TODO: Remove this flag entirely
				params.m_multithreading = true; 
				
				// Stop using OpenCL if a failure ever occurs.
				if (opencl_failed)
					params.m_use_opencl = false;

				bool status = c.init(params);
				
				if (c.get_opencl_failed())
					opencl_failed = true;

				if (status)
				{
					basis_compressor::error_code ec = c.process();

					if (c.get_opencl_failed())
						opencl_failed = true;

					results.m_error_code = ec;

					if (ec == basis_compressor::cECSuccess)
					{
						results.m_basis_file = c.get_output_basis_file();
						results.m_ktx2_file = c.get_output_ktx2_file();
						results.m_stats = c.get_stats();
						results.m_basis_bits_per_texel = c.get_basis_bits_per_texel();
						results.m_any_source_image_has_alpha = c.get_any_source_image_has_alpha();
					}
					else
					{
						result = false;
					}
				}
				else
				{
					results.m_error_code = basis_compressor::cECFailedInitializing;
					
					result = false;
				}

				results.m_total_time = tm.get_elapsed_secs();
			} );

		} // pindex

		jpool.wait_for_all();

		if (opencl_failed)
			error_printf("An OpenCL error occured sometime during compression. The compressor fell back to CPU processing after the failure.\n");

		return result;
	}

	static void* basis_compress(
		const basisu::vector<image> *pSource_images,
		const basisu::vector<imagef> *pSource_images_hdr,
		uint32_t flags_and_quality, float uastc_rdo_quality,
		size_t* pSize,
		image_stats* pStats)
	{
		assert((pSource_images != nullptr) || (pSource_images_hdr != nullptr));
		assert(!((pSource_images != nullptr) && (pSource_images_hdr != nullptr)));
		
		// Check input parameters
		if (pSource_images)
		{
			if ((!pSource_images->size()) || (!pSize))
			{
				error_printf("basis_compress: Invalid parameter\n");
				assert(0);
				return nullptr;
			}
		}
		else
		{
			if ((!pSource_images_hdr->size()) || (!pSize))
			{
				error_printf("basis_compress: Invalid parameter\n");
				assert(0);
				return nullptr;
			}
		}

		*pSize = 0;

		// Initialize a job pool
		uint32_t num_threads = 1;
		if (flags_and_quality & cFlagThreaded)
			num_threads = basisu::maximum<uint32_t>(1, std::thread::hardware_concurrency());

		job_pool jp(num_threads);

		// Initialize the compressor parameter struct
		basis_compressor_params comp_params;
		comp_params.m_pJob_pool = &jp;

		comp_params.m_y_flip = (flags_and_quality & cFlagYFlip) != 0;
		comp_params.m_debug = (flags_and_quality & cFlagDebug) != 0;
		comp_params.m_debug_images = (flags_and_quality & cFlagDebugImages) != 0;
		
		// Copy the largest mipmap level
		if (pSource_images)
		{
			comp_params.m_source_images.resize(1);
			comp_params.m_source_images[0] = (*pSource_images)[0];

			// Copy the smaller mipmap levels, if any
			if (pSource_images->size() > 1)
			{
				comp_params.m_source_mipmap_images.resize(1);
				comp_params.m_source_mipmap_images[0].resize(pSource_images->size() - 1);

				for (uint32_t i = 1; i < pSource_images->size(); i++)
					comp_params.m_source_mipmap_images[0][i - 1] = (*pSource_images)[i];
			}
		}
		else
		{
			comp_params.m_source_images_hdr.resize(1);
			comp_params.m_source_images_hdr[0] = (*pSource_images_hdr)[0];

			// Copy the smaller mipmap levels, if any
			if (pSource_images_hdr->size() > 1)
			{
				comp_params.m_source_mipmap_images_hdr.resize(1);
				comp_params.m_source_mipmap_images_hdr[0].resize(pSource_images_hdr->size() - 1);

				for (uint32_t i = 1; i < pSource_images->size(); i++)
					comp_params.m_source_mipmap_images_hdr[0][i - 1] = (*pSource_images_hdr)[i];
			}
		}
				
		comp_params.m_multithreading = (flags_and_quality & cFlagThreaded) != 0;
		comp_params.m_use_opencl = (flags_and_quality & cFlagUseOpenCL) != 0;

		comp_params.m_write_output_basis_or_ktx2_files = false;

		comp_params.m_perceptual = (flags_and_quality & cFlagSRGB) != 0;
		comp_params.m_mip_srgb = comp_params.m_perceptual;
		comp_params.m_mip_gen = (flags_and_quality & (cFlagGenMipsWrap | cFlagGenMipsClamp)) != 0;
		comp_params.m_mip_wrapping = (flags_and_quality & cFlagGenMipsWrap) != 0;

		if ((pSource_images_hdr) || (flags_and_quality & cFlagHDR))
		{
			// In UASTC HDR mode, the compressor will jam this to true anyway.
			// And there's no need to set UASTC LDR or ETC1S options.
			comp_params.m_uastc = true;
		}
		else
		{
			comp_params.m_uastc = (flags_and_quality & cFlagUASTC) != 0;
			if (comp_params.m_uastc)
			{
				comp_params.m_pack_uastc_flags = flags_and_quality & cPackUASTCLevelMask;
				comp_params.m_rdo_uastc = (flags_and_quality & cFlagUASTCRDO) != 0;
				comp_params.m_rdo_uastc_quality_scalar = uastc_rdo_quality;
			}
			else
			{
				comp_params.m_quality_level = basisu::maximum<uint32_t>(1, flags_and_quality & 255);
			}
		}
				
		comp_params.m_create_ktx2_file = (flags_and_quality & cFlagKTX2) != 0;
						
		if (comp_params.m_create_ktx2_file)
		{
			// Set KTX2 specific parameters.
			if ((flags_and_quality & cFlagKTX2UASTCSuperCompression) && (comp_params.m_uastc))
				comp_params.m_ktx2_uastc_supercompression = basist::KTX2_SS_ZSTANDARD;

			comp_params.m_ktx2_srgb_transfer_func = comp_params.m_perceptual;
		}

		comp_params.m_compute_stats = (pStats != nullptr);
		comp_params.m_print_stats = (flags_and_quality & cFlagPrintStats) != 0;
		comp_params.m_status_output = (flags_and_quality & cFlagPrintStatus) != 0;

		if ((flags_and_quality & cFlagHDR) || (pSource_images_hdr))
		{
			comp_params.m_hdr = true;
			comp_params.m_uastc_hdr_options.set_quality_level(flags_and_quality & cPackUASTCLevelMask);
		}

		if (flags_and_quality & cFlagHDRLDRImageSRGBToLinearConversion)
			comp_params.m_hdr_ldr_srgb_to_linear_conversion = true;

		// Create the compressor, initialize it, and process the input
		basis_compressor comp;
		if (!comp.init(comp_params))
		{
			error_printf("basis_compress: basis_compressor::init() failed!\n");
			return nullptr;
		}

		basis_compressor::error_code ec = comp.process();

		if (ec != basis_compressor::cECSuccess)
		{
			error_printf("basis_compress: basis_compressor::process() failed with error code %u\n", (uint32_t)ec);
			return nullptr;
		}

		if ((pStats) && (comp.get_opencl_failed()))
		{
			pStats->m_opencl_failed = true;
		}

		// Get the output file data and return it to the caller
		void* pFile_data = nullptr;
		const uint8_vec* pFile_data_vec = comp_params.m_create_ktx2_file ? &comp.get_output_ktx2_file() : &comp.get_output_basis_file();

		pFile_data = malloc(pFile_data_vec->size());
		if (!pFile_data)
		{
			error_printf("basis_compress: Out of memory\n");
			return nullptr;
		}
		memcpy(pFile_data, pFile_data_vec->get_ptr(), pFile_data_vec->size());

		*pSize = pFile_data_vec->size();

		if ((pStats) && (comp.get_stats().size()))
		{
			*pStats = comp.get_stats()[0];
		}

		return pFile_data;
	}

	void* basis_compress(
		const basisu::vector<image>& source_images,
		uint32_t flags_and_quality, float uastc_rdo_quality,
		size_t* pSize,
		image_stats* pStats)
	{
		return basis_compress(&source_images, nullptr, flags_and_quality, uastc_rdo_quality, pSize, pStats);
	}

	void* basis_compress(
		const basisu::vector<imagef>& source_images_hdr,
		uint32_t flags_and_quality,
		size_t* pSize,
		image_stats* pStats)
	{
		return basis_compress(nullptr, &source_images_hdr, flags_and_quality, 0.0f, pSize, pStats);
	}

	void* basis_compress(
		const uint8_t* pImageRGBA, uint32_t width, uint32_t height, uint32_t pitch_in_pixels,
		uint32_t flags_and_quality, float uastc_rdo_quality,
		size_t* pSize,
		image_stats* pStats)
	{
		if (!pitch_in_pixels)
			pitch_in_pixels = width;

		if ((!pImageRGBA) || (!width) || (!height) || (pitch_in_pixels < width) || (!pSize))
		{
			error_printf("basis_compress: Invalid parameter\n");
			assert(0);
			return nullptr;
		}

		*pSize = 0;

		if ((width > BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION) || (height > BASISU_MAX_SUPPORTED_TEXTURE_DIMENSION))
		{
			error_printf("basis_compress: Image too large\n");
			return nullptr;
		}

		// Copy the source image
		basisu::vector<image> source_image(1);
		source_image[0].crop(width, height, width, g_black_color, false);
		for (uint32_t y = 0; y < height; y++)
			memcpy(source_image[0].get_ptr() + y * width, (const color_rgba*)pImageRGBA + y * pitch_in_pixels, width * sizeof(color_rgba));

		return basis_compress(source_image, flags_and_quality, uastc_rdo_quality, pSize, pStats);
	}

	void basis_free_data(void* p)
	{
		free(p);
	}

	bool basis_benchmark_etc1s_opencl(bool* pOpenCL_failed)
	{
		if (pOpenCL_failed)
			*pOpenCL_failed = false;

		if (!opencl_is_available())
		{
			error_printf("basis_benchmark_etc1s_opencl: OpenCL support must be enabled first!\n");
			return false;
		}

		const uint32_t W = 1024, H = 1024;
		basisu::vector<image> images;
		image& img = images.enlarge(1)->resize(W, H);
		
		const uint32_t NUM_RAND_LETTERS = 6000;// 40000;

		rand r;
		r.seed(200);

		for (uint32_t i = 0; i < NUM_RAND_LETTERS; i++)
		{
			uint32_t x = r.irand(0, W - 1), y = r.irand(0, H - 1);
			uint32_t sx = r.irand(1, 4), sy = r.irand(1, 4);
			color_rgba c(r.byte(), r.byte(), r.byte(), 255);

			img.debug_text(x, y, sx, sy, c, nullptr, false, "%c", static_cast<char>(r.irand(32, 127)));
		}

		//save_png("test.png", img);

		image_stats stats;

		uint32_t flags_and_quality = cFlagSRGB | cFlagThreaded | 255;
		size_t comp_size = 0;

		double best_cpu_time = 1e+9f, best_gpu_time = 1e+9f;

		const uint32_t TIMES_TO_ENCODE = 2;
		interval_timer tm;

		for (uint32_t i = 0; i < TIMES_TO_ENCODE; i++)
		{
			tm.start();
			void* pComp_data = basis_compress(
				images,
				flags_and_quality, 1.0f,
				&comp_size,
				&stats);
			double cpu_time = tm.get_elapsed_secs();
			if (!pComp_data)
			{
				error_printf("basis_benchmark_etc1s_opencl: basis_compress() failed (CPU)!\n");
				return false;
			}
			
			best_cpu_time = minimum(best_cpu_time, cpu_time);

			basis_free_data(pComp_data);
		}

		printf("Best CPU time: %3.3f\n", best_cpu_time);

		for (uint32_t i = 0; i < TIMES_TO_ENCODE; i++)
		{
			tm.start();
			void* pComp_data = basis_compress(
				images,
				flags_and_quality | cFlagUseOpenCL, 1.0f,
				&comp_size,
				&stats);

			if (stats.m_opencl_failed)
			{
				error_printf("basis_benchmark_etc1s_opencl: OpenCL failed!\n");

				basis_free_data(pComp_data);

				if (pOpenCL_failed)
					*pOpenCL_failed = true;

				return false;
			}

			double gpu_time = tm.get_elapsed_secs();
			if (!pComp_data)
			{
				error_printf("basis_benchmark_etc1s_opencl: basis_compress() failed (GPU)!\n");
				return false;
			}

			best_gpu_time = minimum(best_gpu_time, gpu_time);

			basis_free_data(pComp_data);
		}

		printf("Best GPU time: %3.3f\n", best_gpu_time);
				
		return best_gpu_time < best_cpu_time;
	}

} // namespace basisu



