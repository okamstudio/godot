// File: basisu_bc7enc.h
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
#include "basisu_enc.h"
#include "../transcoder/basisu_transcoder_uastc.h"

namespace basisu
{

#define BC7ENC_MAX_PARTITIONS1 (64)
#define BC7ENC_MAX_UBER_LEVEL (4)

	typedef uint8_t bc7enc_bool;

#define BC7ENC_TRUE (1)
#define BC7ENC_FALSE (0)
		
	typedef struct { float m_c[4]; } bc7enc_vec4F;

	extern const float g_bc7_weights1x[2 * 4];
	extern const float g_bc7_weights2x[4 * 4];
	extern const float g_bc7_weights3x[8 * 4];
	extern const float g_bc7_weights4x[16 * 4];
	extern const float g_astc_weights4x[16 * 4];
	extern const float g_astc_weights5x[32 * 4];
	extern const float g_astc_weights_3levelsx[3 * 4];
			
	extern basist::astc_quant_bin g_astc_sorted_order_unquant[basist::BC7ENC_TOTAL_ASTC_RANGES][256]; // [sorted unquantized order]
	
	struct color_cell_compressor_params
	{
		uint32_t m_num_pixels;
		const basist::color_quad_u8* m_pPixels;

		uint32_t m_num_selector_weights;
		const uint32_t* m_pSelector_weights;

		const bc7enc_vec4F* m_pSelector_weightsx;
		uint32_t m_comp_bits;

		const uint8_t *m_pForce_selectors;

		// Non-zero m_astc_endpoint_range enables ASTC mode. m_comp_bits and m_has_pbits are always false. We only support 2, 3, or 4 bit weight encodings.
		uint32_t m_astc_endpoint_range;

		uint32_t m_weights[4];
		bc7enc_bool m_has_alpha;
		bc7enc_bool m_has_pbits;
		bc7enc_bool m_endpoints_share_pbit;
		bc7enc_bool m_perceptual;
	};

	struct color_cell_compressor_results
	{
		uint64_t m_best_overall_err;
		basist::color_quad_u8 m_low_endpoint;
		basist::color_quad_u8 m_high_endpoint;
		uint32_t m_pbits[2];
		uint8_t* m_pSelectors;
		uint8_t* m_pSelectors_temp;

		// Encoded ASTC indices, if ASTC mode is enabled
		basist::color_quad_u8 m_astc_low_endpoint;
		basist::color_quad_u8 m_astc_high_endpoint;
	};

	struct bc7enc_compress_block_params
	{
		// m_max_partitions_mode1 may range from 0 (disables mode 1) to BC7ENC_MAX_PARTITIONS1. The higher this value, the slower the compressor, but the higher the quality.
		uint32_t m_max_partitions_mode1;

		// Relative RGBA or YCbCrA weights.
		uint32_t m_weights[4];

		// m_uber_level may range from 0 to BC7ENC_MAX_UBER_LEVEL. The higher this value, the slower the compressor, but the higher the quality.
		uint32_t m_uber_level;

		// If m_perceptual is true, colorspace error is computed in YCbCr space, otherwise RGB.
		bc7enc_bool m_perceptual;

		uint32_t m_least_squares_passes;
	};

	uint64_t color_cell_compression(uint32_t mode, const color_cell_compressor_params* pParams, color_cell_compressor_results* pResults, const bc7enc_compress_block_params* pComp_params);
		
	uint64_t color_cell_compression_est_astc(
		uint32_t num_weights, uint32_t num_comps, const uint32_t* pWeight_table,
		uint32_t num_pixels, const basist::color_quad_u8* pPixels,
		uint64_t best_err_so_far, const uint32_t weights[4]);
		
	inline void bc7enc_compress_block_params_init_linear_weights(bc7enc_compress_block_params* p)
	{
		p->m_perceptual = BC7ENC_FALSE;
		p->m_weights[0] = 1;
		p->m_weights[1] = 1;
		p->m_weights[2] = 1;
		p->m_weights[3] = 1;
	}

	inline void bc7enc_compress_block_params_init_perceptual_weights(bc7enc_compress_block_params* p)
	{
		p->m_perceptual = BC7ENC_TRUE;
		p->m_weights[0] = 128;
		p->m_weights[1] = 64;
		p->m_weights[2] = 16;
		p->m_weights[3] = 32;
	}

	inline void bc7enc_compress_block_params_init(bc7enc_compress_block_params* p)
	{
		p->m_max_partitions_mode1 = BC7ENC_MAX_PARTITIONS1;
		p->m_least_squares_passes = 1;
		p->m_uber_level = 0;
		bc7enc_compress_block_params_init_perceptual_weights(p);
	}

	// bc7enc_compress_block_init() MUST be called before calling bc7enc_compress_block() (or you'll get artifacts).
	void bc7enc_compress_block_init();
				
} // namespace basisu
