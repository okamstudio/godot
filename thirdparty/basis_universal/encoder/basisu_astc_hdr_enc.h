// basisu_astc_hdr_enc.h
#pragma once
#include "basisu_enc.h"
#include "basisu_gpu_texture.h"
#include "../transcoder/basisu_astc_helpers.h"
#include "../transcoder/basisu_astc_hdr_core.h"

namespace basisu
{
	// This MUST be called before encoding any blocks.
	void astc_hdr_enc_init();

	const uint32_t MODE11_FIRST_ISE_RANGE = astc_helpers::BISE_3_LEVELS, MODE11_LAST_ISE_RANGE = astc_helpers::BISE_16_LEVELS;
	const uint32_t MODE7_PART1_FIRST_ISE_RANGE = astc_helpers::BISE_3_LEVELS, MODE7_PART1_LAST_ISE_RANGE = astc_helpers::BISE_16_LEVELS;
	const uint32_t MODE7_PART2_FIRST_ISE_RANGE = astc_helpers::BISE_3_LEVELS, MODE7_PART2_LAST_ISE_RANGE = astc_helpers::BISE_8_LEVELS;
	const uint32_t MODE11_PART2_FIRST_ISE_RANGE = astc_helpers::BISE_3_LEVELS, MODE11_PART2_LAST_ISE_RANGE = astc_helpers::BISE_4_LEVELS;
	const uint32_t MODE11_TOTAL_SUBMODES = 8; // plus an extra hidden submode, directly encoded, for direct, so really 9 (see tables 99/100 of the ASTC spec)
	const uint32_t MODE7_TOTAL_SUBMODES = 6;
		
	struct astc_hdr_codec_options
	{
		float m_bc6h_err_weight;

		bool m_use_solid;

		bool m_use_mode11;
		bool m_mode11_uber_mode;
		uint32_t m_first_mode11_weight_ise_range;
		uint32_t m_last_mode11_weight_ise_range;
		bool m_mode11_direct_only;
		int32_t m_first_mode11_submode;
		int32_t m_last_mode11_submode;

		bool m_use_mode7_part1;
		uint32_t m_first_mode7_part1_weight_ise_range;
		uint32_t m_last_mode7_part1_weight_ise_range;

		bool m_use_mode7_part2;
		uint32_t m_mode7_part2_part_masks;
		uint32_t m_first_mode7_part2_weight_ise_range;
		uint32_t m_last_mode7_part2_weight_ise_range;

		bool m_use_mode11_part2;
		uint32_t m_mode11_part2_part_masks;
		uint32_t m_first_mode11_part2_weight_ise_range;
		uint32_t m_last_mode11_part2_weight_ise_range;

		float m_r_err_scale, m_g_err_scale;

		bool m_refine_weights;

		uint32_t m_level;

		bool m_use_estimated_partitions;
		uint32_t m_max_estimated_partitions;

		// If true, the ASTC HDR compressor is allowed to more aggressively vary weight indices for slightly higher compression in non-fastest mode. This will hurt BC6H quality, however.
		bool m_allow_uber_mode;

		astc_hdr_codec_options();

		void init();
				
		// TODO: set_quality_level() is preferred to configure the codec for transcoding purposes.
		static const int cMinLevel = 0;
		static const int cMaxLevel = 4;
		static const int cDefaultLevel = 1;
		void set_quality_level(int level);

	private:
		void set_quality_best();
		void set_quality_normal();
		void set_quality_fastest();
	};

	struct astc_hdr_pack_results
	{
		double m_best_block_error;
		double m_bc6h_block_error; // note this is not used/set by the encoder, here for convienance 

		// Encoder results (logical ASTC block)
		astc_helpers::log_astc_block m_best_blk;
		
		// For statistical use
		uint32_t m_best_submodes[2];
		uint32_t m_best_pat_index;
		bool m_constrained_weights;

		bool m_improved_via_refinement_flag;
				
		// Only valid if the block is solid
		basist::astc_blk m_solid_blk;
		
		// The BC6H transcoded block
		basist::bc6h_block m_bc6h_block;

		// Solid color/void extent flag
		bool m_is_solid;

		void clear()
		{
			m_best_block_error = 1e+30f;
			m_bc6h_block_error = 1e+30f;

			m_best_blk.clear();
			m_best_blk.m_grid_width = 4;
			m_best_blk.m_grid_height = 4;
			m_best_blk.m_endpoint_ise_range = 20; // 0-255

			clear_obj(m_best_submodes);

			m_best_pat_index = 0;
			m_constrained_weights = false;
									
			clear_obj(m_bc6h_block);
			
			m_is_solid = false;
			m_improved_via_refinement_flag = false;
		}
	};
			
	void interpolate_qlog12_colors(
		const int e[2][3],
		basist::half_float* pDecoded_half,
		vec3F* pDecoded_float,
		uint32_t n, uint32_t ise_weight_range);
		
	bool get_astc_hdr_mode_11_block_colors(
		const uint8_t* pEndpoints,
		basist::half_float* pDecoded_half,
		vec3F* pDecoded_float,
		uint32_t n, uint32_t ise_weight_range, uint32_t ise_endpoint_range);
		
	bool get_astc_hdr_mode_7_block_colors(
		const uint8_t* pEndpoints,
		basist::half_float* pDecoded_half,
		vec3F* pDecoded_float,
		uint32_t n, uint32_t ise_weight_range, uint32_t ise_endpoint_range);

	double eval_selectors(
		uint32_t num_pixels,
		uint8_t* pWeights,
		const basist::half_float* pBlock_pixels_half,
		uint32_t num_weight_levels,
		const basist::half_float* pDecoded_half,
		const astc_hdr_codec_options& coptions,
		uint32_t usable_selector_bitmask = UINT32_MAX);

	double compute_block_error(const basist::half_float* pOrig_block, const basist::half_float* pPacked_block, const astc_hdr_codec_options& coptions);

	// Encodes a 4x4 ASTC HDR block given a 4x4 array of source block pixels/texels.
	// Supports solid color blocks, mode 11 (all submodes), mode 7/1 partition (all submodes), 
	// and mode 7/2 partitions (all submodes) - 30 patterns, only the ones also in common with the BC6H format.
	// The packed ASTC weight grid dimensions are currently always 4x4 texels, but may be also 3x3 in the future.
	// This function is thread safe, i.e. it may be called from multiple encoding threads simultanously with different blocks.
	// 
	// Parameters:
	// pRGBPixels - An array of 48 (16 RGB) floats: the 4x4 block to pack
	// pPacked_block - A pointer to the packed ASTC HDR block
	// coptions - Codec options
	// pInternal_results - An optional pointer to details about how the block was packed, for statistics/debugging purposes. May be nullptr.
	// 
	// Requirements: 
	// astc_hdr_enc_init() MUST have been called first to initialized the codec.
	// Input pixels are checked and cannot be NaN's, Inf's, signed, or too large (greater than MAX_HALF_FLOAT, or 65504). 
	// Normal values and denormals are okay.
	bool astc_hdr_enc_block(
		const float* pRGBPixels,
		const astc_hdr_codec_options& coptions,
		basisu::vector<astc_hdr_pack_results> &all_results);

	bool astc_hdr_pack_results_to_block(basist::astc_blk& dst_blk, const astc_hdr_pack_results& results);
		
	bool astc_hdr_refine_weights(const basist::half_float* pSource_block, astc_hdr_pack_results& cur_results, const astc_hdr_codec_options& coptions, float bc6h_weight, bool* pImproved_flag);

	struct astc_hdr_block_stats
	{
		std::mutex m_mutex;

		uint32_t m_total_blocks;
		uint32_t m_total_2part, m_total_solid;
		uint32_t m_total_mode7_1part, m_total_mode7_2part;
		uint32_t m_total_mode11_1part, m_total_mode11_2part;
		uint32_t m_total_mode11_1part_constrained_weights;

		uint32_t m_weight_range_hist_7[11];
		uint32_t m_weight_range_hist_7_2part[11];
		uint32_t m_mode7_submode_hist[6];

		uint32_t m_weight_range_hist_11[11];
		uint32_t m_weight_range_hist_11_2part[11];
		uint32_t m_mode11_submode_hist[9];
								
		uint32_t m_part_hist[32];

		uint32_t m_total_refined;
								
		astc_hdr_block_stats() { clear(); }

		void clear()
		{
			std::lock_guard<std::mutex> lck(m_mutex);

			m_total_blocks = 0;
			m_total_mode7_1part = 0, m_total_mode7_2part = 0, m_total_mode11_1part = 0, m_total_2part = 0, m_total_solid = 0, m_total_mode11_2part = 0;
			m_total_mode11_1part_constrained_weights = 0;
			m_total_refined = 0;

			clear_obj(m_weight_range_hist_11);
			clear_obj(m_weight_range_hist_11_2part);
			clear_obj(m_weight_range_hist_7);
			clear_obj(m_weight_range_hist_7_2part);
			clear_obj(m_mode7_submode_hist);
			clear_obj(m_mode11_submode_hist);
			clear_obj(m_part_hist);
		}

		void update(const astc_hdr_pack_results& log_blk);
		
		void print();
	};
		
} // namespace basisu

