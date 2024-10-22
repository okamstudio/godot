// basisu_uastc_enc.cpp
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
#include "basisu_uastc_enc.h"
#include "3rdparty/android_astc_decomp.h"
#include "basisu_gpu_texture.h"
#include "basisu_bc7enc.h"

#ifdef _DEBUG
// When BASISU_VALIDATE_UASTC_ENC is 1, we pack and unpack to/from UASTC and ASTC, then validate that each codec returns the exact same results. This is slower.
#define BASISU_VALIDATE_UASTC_ENC 1
#endif

#define BASISU_SUPPORT_FORCE_MODE 0

using namespace basist;

namespace basisu
{
	const uint32_t MAX_ENCODE_RESULTS = 512;

#if BASISU_VALIDATE_UASTC_ENC
	static void validate_func(bool condition, int line)
	{
		if (!condition)
		{
			fprintf(stderr, "basisu_uastc_enc: Internal validation failed on line %u!\n", line);
		}
	}

	#define VALIDATE(c) validate_func(c, __LINE__);
#else
	#define VALIDATE(c)
#endif

	enum dxt_constants
	{
		cDXT1SelectorBits = 2U, cDXT1SelectorValues = 1U << cDXT1SelectorBits, cDXT1SelectorMask = cDXT1SelectorValues - 1U,
		cDXT5SelectorBits = 3U, cDXT5SelectorValues = 1U << cDXT5SelectorBits, cDXT5SelectorMask = cDXT5SelectorValues - 1U,
	};

	struct dxt1_block
	{
		enum { cTotalEndpointBytes = 2, cTotalSelectorBytes = 4 };

		uint8_t m_low_color[cTotalEndpointBytes];
		uint8_t m_high_color[cTotalEndpointBytes];
		uint8_t m_selectors[cTotalSelectorBytes];

		inline void clear() { basisu::clear_obj(*this); }

		inline uint32_t get_high_color() const { return m_high_color[0] | (m_high_color[1] << 8U); }
		inline uint32_t get_low_color() const { return m_low_color[0] | (m_low_color[1] << 8U); }
		inline void set_low_color(uint16_t c) { m_low_color[0] = static_cast<uint8_t>(c & 0xFF); m_low_color[1] = static_cast<uint8_t>((c >> 8) & 0xFF); }
		inline void set_high_color(uint16_t c) { m_high_color[0] = static_cast<uint8_t>(c & 0xFF); m_high_color[1] = static_cast<uint8_t>((c >> 8) & 0xFF); }
		inline uint32_t get_selector(uint32_t x, uint32_t y) const { assert((x < 4U) && (y < 4U)); return (m_selectors[y] >> (x * cDXT1SelectorBits))& cDXT1SelectorMask; }
		inline void set_selector(uint32_t x, uint32_t y, uint32_t val) { assert((x < 4U) && (y < 4U) && (val < 4U)); m_selectors[y] &= (~(cDXT1SelectorMask << (x * cDXT1SelectorBits))); m_selectors[y] |= (val << (x * cDXT1SelectorBits)); }

		static uint16_t pack_color(const color_rgba& color, bool scaled, uint32_t bias = 127U)
		{
			uint32_t r = color.r, g = color.g, b = color.b;
			if (scaled)
			{
				r = (r * 31U + bias) / 255U;
				g = (g * 63U + bias) / 255U;
				b = (b * 31U + bias) / 255U;
			}
			return static_cast<uint16_t>(basisu::minimum(b, 31U) | (basisu::minimum(g, 63U) << 5U) | (basisu::minimum(r, 31U) << 11U));
		}

		static uint16_t pack_unscaled_color(uint32_t r, uint32_t g, uint32_t b) { return static_cast<uint16_t>(b | (g << 5U) | (r << 11U)); }
	};

#define UASTC_WRITE_MODE_DESCS 0

	static inline void uastc_write_bits(uint8_t* pBuf, uint32_t& bit_offset, uint64_t code, uint32_t codesize, const char* pDesc)
	{
		(void)pDesc;

#if UASTC_WRITE_MODE_DESCS
		if (pDesc)
			printf("%s: %u %u\n", pDesc, bit_offset, codesize);
#endif

		assert((codesize == 64) || (code < (1ULL << codesize)));

		while (codesize)
		{
			uint32_t byte_bit_offset = bit_offset & 7;
			uint32_t bits_to_write = basisu::minimum<int>(codesize, 8 - byte_bit_offset);

			pBuf[bit_offset >> 3] |= (code << byte_bit_offset);

			code >>= bits_to_write;
			codesize -= bits_to_write;
			bit_offset += bits_to_write;
		}
	}

	void pack_uastc(basist::uastc_block& blk, const uastc_encode_results& result, const etc_block& etc1_blk, uint32_t etc1_bias, const eac_a8_block& etc_eac_a8_blk, bool bc1_hint0, bool bc1_hint1)
	{
		if ((g_uastc_mode_has_alpha[result.m_uastc_mode]) && (result.m_uastc_mode != UASTC_MODE_INDEX_SOLID_COLOR))
		{
			assert(etc_eac_a8_blk.m_multiplier >= 1);
		}

		uint8_t buf[32];
		memset(buf, 0, sizeof(buf));

		uint32_t block_bit_offset = 0;

#if UASTC_WRITE_MODE_DESCS
		printf("**** Mode: %u\n", result.m_uastc_mode);
#endif

		uastc_write_bits(buf, block_bit_offset, g_uastc_mode_huff_codes[result.m_uastc_mode][0], g_uastc_mode_huff_codes[result.m_uastc_mode][1], "mode");

		if (result.m_uastc_mode == UASTC_MODE_INDEX_SOLID_COLOR)
		{
			uastc_write_bits(buf, block_bit_offset, result.m_solid_color.r, 8, "R");
			uastc_write_bits(buf, block_bit_offset, result.m_solid_color.g, 8, "G");
			uastc_write_bits(buf, block_bit_offset, result.m_solid_color.b, 8, "B");
			uastc_write_bits(buf, block_bit_offset, result.m_solid_color.a, 8, "A");

			uastc_write_bits(buf, block_bit_offset, etc1_blk.get_diff_bit(), 1, "ETC1D");
			uastc_write_bits(buf, block_bit_offset, etc1_blk.get_inten_table(0), 3, "ETC1I");
			uastc_write_bits(buf, block_bit_offset, etc1_blk.get_selector(0, 0), 2, "ETC1S");

			uint32_t r, g, b;
			if (etc1_blk.get_diff_bit())
				etc_block::unpack_color5(r, g, b, etc1_blk.get_base5_color(), false);
			else
				etc_block::unpack_color4(r, g, b, etc1_blk.get_base4_color(0), false);

			uastc_write_bits(buf, block_bit_offset, r, 5, "ETC1R");
			uastc_write_bits(buf, block_bit_offset, g, 5, "ETC1G");
			uastc_write_bits(buf, block_bit_offset, b, 5, "ETC1B");

			memcpy(&blk, buf, sizeof(blk));
			return;
		}

		if (g_uastc_mode_has_bc1_hint0[result.m_uastc_mode])
			uastc_write_bits(buf, block_bit_offset, bc1_hint0, 1, "BC1H0");
		else
		{
			assert(bc1_hint0 == false);
		}

		if (g_uastc_mode_has_bc1_hint1[result.m_uastc_mode])
			uastc_write_bits(buf, block_bit_offset, bc1_hint1, 1, "BC1H1");
		else
		{
			assert(bc1_hint1 == false);
		}

		uastc_write_bits(buf, block_bit_offset, etc1_blk.get_flip_bit(), 1, "ETC1F");
		uastc_write_bits(buf, block_bit_offset, etc1_blk.get_diff_bit(), 1, "ETC1D");
		uastc_write_bits(buf, block_bit_offset, etc1_blk.get_inten_table(0), 3, "ETC1I0");
		uastc_write_bits(buf, block_bit_offset, etc1_blk.get_inten_table(1), 3, "ETC1I1");

		if (g_uastc_mode_has_etc1_bias[result.m_uastc_mode])
			uastc_write_bits(buf, block_bit_offset, etc1_bias, 5, "ETC1BIAS");
		else
		{
			assert(etc1_bias == 0);
		}

		if (g_uastc_mode_has_alpha[result.m_uastc_mode])
		{
			const uint32_t etc2_hints = etc_eac_a8_blk.m_table | (etc_eac_a8_blk.m_multiplier << 4);

			assert(etc2_hints > 0 && etc2_hints <= 0xFF);
			uastc_write_bits(buf, block_bit_offset, etc2_hints, 8, "ETC2TM");
		}

		uint32_t subsets = 1;
		switch (result.m_uastc_mode)
		{
		case 2:
		case 4:
		case 7:
		case 9:
		case 16:
			uastc_write_bits(buf, block_bit_offset, result.m_common_pattern, 5, "PAT");
			subsets = 2;
			break;
		case 3:
			uastc_write_bits(buf, block_bit_offset, result.m_common_pattern, 4, "PAT");
			subsets = 3;
			break;
		default:
			break;
		}

#ifdef _DEBUG
		uint32_t part_seed = 0;
		switch (result.m_uastc_mode)
		{
		case 2:
		case 4:
		case 9:
		case 16:
			part_seed = g_astc_bc7_common_partitions2[result.m_common_pattern].m_astc;
			break;
		case 3:
			part_seed = g_astc_bc7_common_partitions3[result.m_common_pattern].m_astc;
			break;
		case 7:
			part_seed = g_bc7_3_astc2_common_partitions[result.m_common_pattern].m_astc2;
			break;
		default:
			break;
		}
#endif		

		uint32_t total_planes = 1;
		switch (result.m_uastc_mode)
		{
		case 6:
		case 11:
		case 13:
			uastc_write_bits(buf, block_bit_offset, result.m_astc.m_ccs, 2, "COMPSEL");
			total_planes = 2;
			break;
		case 17:
			// CCS field is always 3 for dual plane LA.
			assert(result.m_astc.m_ccs == 3);
			total_planes = 2;
			break;
		default:
			break;
		}

		uint8_t weights[32];
		memcpy(weights, result.m_astc.m_weights, 16 * total_planes);

		uint8_t endpoints[18];
		memcpy(endpoints, result.m_astc.m_endpoints, sizeof(endpoints));

		const uint32_t total_comps = g_uastc_mode_comps[result.m_uastc_mode];

		// LLAA
		// LLAA LLAA
		// LLAA LLAA LLAA
		// RRGGBB
		// RRGGBB RRGGBB
		// RRGGBB RRGGBB RRGGBB
		// RRGGBBAA
		// RRGGBBAA RRGGBBAA

		const uint32_t weight_bits = g_uastc_mode_weight_bits[result.m_uastc_mode];

		const uint8_t* pPartition_pattern;
		const uint8_t* pSubset_anchor_indices = basist::get_anchor_indices(subsets, result.m_uastc_mode, result.m_common_pattern, pPartition_pattern);

		for (uint32_t plane_index = 0; plane_index < total_planes; plane_index++)
		{
			for (uint32_t subset_index = 0; subset_index < subsets; subset_index++)
			{
				const uint32_t anchor_index = pSubset_anchor_indices[subset_index];

#ifdef _DEBUG
				if (subsets >= 2)
				{
					for (uint32_t i = 0; i < 16; i++)
					{
						const uint32_t part_index = astc_compute_texel_partition(part_seed, i & 3, i >> 2, 0, subsets, true);
						if (part_index == subset_index)
						{
							assert(anchor_index == i);
							break;
						}
					}
				}
				else
				{
					assert(!anchor_index);
				}
#endif

				// Check anchor weight's MSB - if it's set then invert this subset's weights and swap the endpoints
				if (weights[anchor_index * total_planes + plane_index] & (1 << (weight_bits - 1)))
				{
					for (uint32_t i = 0; i < 16; i++)
					{
						const uint32_t part_index = pPartition_pattern[i];

#ifdef _DEBUG
						if (subsets >= 2)
						{
							assert(part_index == (uint32_t)astc_compute_texel_partition(part_seed, i & 3, i >> 2, 0, subsets, true));
						}
						else
						{
							assert(!part_index);
						}
#endif

						if (part_index == subset_index)
							weights[i * total_planes + plane_index] = ((1 << weight_bits) - 1) - weights[i * total_planes + plane_index];
					}

					if (total_planes == 2)
					{
						for (int c = 0; c < (int)total_comps; c++)
						{
							const uint32_t comp_plane = (total_comps == 2) ? c : ((c == result.m_astc.m_ccs) ? 1 : 0);

							if (comp_plane == plane_index)
								std::swap(endpoints[c * 2 + 0], endpoints[c * 2 + 1]);
						}
					}
					else
					{
						for (uint32_t c = 0; c < total_comps; c++)
							std::swap(endpoints[subset_index * total_comps * 2 + c * 2 + 0], endpoints[subset_index * total_comps * 2 + c * 2 + 1]);
					}
				}
			} // subset_index
		} // plane_index

		const uint32_t total_values = total_comps * 2 * subsets;
		const uint32_t endpoint_range = g_uastc_mode_endpoint_ranges[result.m_uastc_mode];

		uint32_t bit_values[18];
		uint32_t tq_values[8];
		uint32_t total_tq_values = 0;
		uint32_t tq_accum = 0;
		uint32_t tq_mul = 1;

		const uint32_t ep_bits = g_astc_bise_range_table[endpoint_range][0];
		const uint32_t ep_trits = g_astc_bise_range_table[endpoint_range][1];
		const uint32_t ep_quints = g_astc_bise_range_table[endpoint_range][2];

		for (uint32_t i = 0; i < total_values; i++)
		{
			uint32_t val = endpoints[i];

			uint32_t bits = val & ((1 << ep_bits) - 1);
			uint32_t tq = val >> ep_bits;

			bit_values[i] = bits;

			if (ep_trits)
			{
				assert(tq < 3);
				tq_accum += tq * tq_mul;
				tq_mul *= 3;
				if (tq_mul == 243)
				{
					tq_values[total_tq_values++] = tq_accum;
					tq_accum = 0;
					tq_mul = 1;
				}
			}
			else if (ep_quints)
			{
				assert(tq < 5);
				tq_accum += tq * tq_mul;
				tq_mul *= 5;
				if (tq_mul == 125)
				{
					tq_values[total_tq_values++] = tq_accum;
					tq_accum = 0;
					tq_mul = 1;
				}
			}
		}

		uint32_t total_endpoint_bits = 0;
		(void)total_endpoint_bits;

		for (uint32_t i = 0; i < total_tq_values; i++)
		{
			const uint32_t num_bits = ep_trits ? 8 : 7;
			uastc_write_bits(buf, block_bit_offset, tq_values[i], num_bits, "ETQ");
			total_endpoint_bits += num_bits;
		}

		if (tq_mul > 1)
		{
			uint32_t num_bits;
			if (ep_trits)
			{
				if (tq_mul == 3)
					num_bits = 2;
				else if (tq_mul == 9)
					num_bits = 4;
				else if (tq_mul == 27)
					num_bits = 5;
				else //if (tq_mul == 81)
					num_bits = 7;
			}
			else
			{
				if (tq_mul == 5)
					num_bits = 3;
				else //if (tq_mul == 25)
					num_bits = 5;
			}
			uastc_write_bits(buf, block_bit_offset, tq_accum, num_bits, "ETQ");
			total_endpoint_bits += num_bits;
		}

		for (uint32_t i = 0; i < total_values; i++)
		{
			uastc_write_bits(buf, block_bit_offset, bit_values[i], ep_bits, "EBITS");
			total_endpoint_bits += ep_bits;
		}

#if UASTC_WRITE_MODE_DESCS
		uint32_t weight_start = block_bit_offset;
#endif

		uint32_t total_weight_bits = 0;
		(void)total_weight_bits;

		const uint32_t plane_shift = (total_planes == 2) ? 1 : 0;
		for (uint32_t i = 0; i < 16 * total_planes; i++)
		{
			uint32_t numbits = weight_bits;
			for (uint32_t s = 0; s < subsets; s++)
			{
				if (pSubset_anchor_indices[s] == (i >> plane_shift))
				{
					numbits--;
					break;
				}
			}

			uastc_write_bits(buf, block_bit_offset, weights[i], numbits, nullptr);

			total_weight_bits += numbits;
		}

#if UASTC_WRITE_MODE_DESCS
		printf("WEIGHTS: %u %u\n", weight_start, total_weight_bits);
#endif

		assert(block_bit_offset <= 128);
		memcpy(&blk, buf, sizeof(blk));

#if UASTC_WRITE_MODE_DESCS
		printf("Total bits: %u, endpoint bits: %u, weight bits: %u\n", block_bit_offset, total_endpoint_bits, total_weight_bits);
#endif
	}
	
	// MODE 0
	// 0. DualPlane: 0, WeightRange: 8 (16), Subsets: 1, CEM: 8 (RGB Direct       ), EndpointRange: 19 (192)       MODE6 RGB
	// 18. DualPlane: 0, WeightRange: 11 (32), Subsets: 1, CEM: 8 (RGB Direct       ), EndpointRange: 11 (32)       MODE6 RGB
	static void astc_mode0_or_18(uint32_t mode, const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params, const uint8_t *pForce_selectors = nullptr)
	{
		const uint32_t endpoint_range = (mode == 18) ? 11 : 19;
		const uint32_t weight_range = (mode == 18) ? 11 : 8;

		color_cell_compressor_params ccell_params;
		memset(&ccell_params, 0, sizeof(ccell_params));

		ccell_params.m_num_pixels = 16;
		ccell_params.m_pPixels = (color_quad_u8*)&block[0][0];
		ccell_params.m_num_selector_weights = (mode == 18) ? 32 : 16;
		ccell_params.m_pSelector_weights = (mode == 18) ? g_astc_weights5 : g_astc_weights4;
		ccell_params.m_pSelector_weightsx = (mode == 18) ? (const bc7enc_vec4F*)g_astc_weights5x : (const bc7enc_vec4F*)g_astc_weights4x;
		ccell_params.m_astc_endpoint_range = endpoint_range;
		ccell_params.m_weights[0] = 1;
		ccell_params.m_weights[1] = 1;
		ccell_params.m_weights[2] = 1;
		ccell_params.m_weights[3] = 1;
		ccell_params.m_pForce_selectors = pForce_selectors;

		color_cell_compressor_results ccell_results;
		uint8_t ccell_result_selectors[16];
		uint8_t ccell_result_selectors_temp[16];
		memset(&ccell_results, 0, sizeof(ccell_results));
		ccell_results.m_pSelectors = &ccell_result_selectors[0];
		ccell_results.m_pSelectors_temp = &ccell_result_selectors_temp[0];

		uint64_t part_err = color_cell_compression(255, &ccell_params, &ccell_results, &comp_params);

		// ASTC
		astc_block_desc astc_results;
		memset(&astc_results, 0, sizeof(astc_results));

		astc_results.m_dual_plane = false;
		astc_results.m_weight_range = weight_range;// (mode == 18) ? 11 : 8;

		astc_results.m_ccs = 0;
		astc_results.m_subsets = 1;
		astc_results.m_partition_seed = 0;
		astc_results.m_cem = 8;

		astc_results.m_endpoints[0] = ccell_results.m_astc_low_endpoint.m_c[0];
		astc_results.m_endpoints[1] = ccell_results.m_astc_high_endpoint.m_c[0];
		astc_results.m_endpoints[2] = ccell_results.m_astc_low_endpoint.m_c[1];
		astc_results.m_endpoints[3] = ccell_results.m_astc_high_endpoint.m_c[1];
		astc_results.m_endpoints[4] = ccell_results.m_astc_low_endpoint.m_c[2];
		astc_results.m_endpoints[5] = ccell_results.m_astc_high_endpoint.m_c[2];
				
		bool invert = false;

		if (pForce_selectors == nullptr)
		{
			int s0 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[4]].m_unquant;
			int s1 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[5]].m_unquant;
			if (s1 < s0)
			{
				std::swap(astc_results.m_endpoints[0], astc_results.m_endpoints[1]);
				std::swap(astc_results.m_endpoints[2], astc_results.m_endpoints[3]);
				std::swap(astc_results.m_endpoints[4], astc_results.m_endpoints[5]);
				invert = true;
			}
		}

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				astc_results.m_weights[x + y * 4] = ccell_result_selectors[x + y * 4];

				if (invert)
					astc_results.m_weights[x + y * 4] = ((mode == 18) ? 31 : 15) - astc_results.m_weights[x + y * 4];
			}
		}

		assert(total_results < MAX_ENCODE_RESULTS);
		if (total_results < MAX_ENCODE_RESULTS)
		{
			pResults[total_results].m_uastc_mode = mode;
			pResults[total_results].m_common_pattern = 0;
			pResults[total_results].m_astc = astc_results;
			pResults[total_results].m_astc_err = part_err;
			total_results++;
		}
	}

	// MODE 1
	// 1-subset, 2-bit indices, 8-bit endpoints, BC7 mode 3
	// DualPlane: 0, WeightRange: 2 (4), Subsets: 1, CEM: 8 (RGB Direct       ), EndpointRange: 20 (256)        MODE3 or MODE5 RGB
	static void astc_mode1(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		color_cell_compressor_params ccell_params;
		memset(&ccell_params, 0, sizeof(ccell_params));

		ccell_params.m_num_pixels = 16;
		ccell_params.m_pPixels = (color_quad_u8*)&block[0][0];
		ccell_params.m_num_selector_weights = 4;
		ccell_params.m_pSelector_weights = g_bc7_weights2;
		ccell_params.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
		ccell_params.m_astc_endpoint_range = 20;
		ccell_params.m_weights[0] = 1;
		ccell_params.m_weights[1] = 1;
		ccell_params.m_weights[2] = 1;
		ccell_params.m_weights[3] = 1;

		color_cell_compressor_results ccell_results;
		uint8_t ccell_result_selectors[16];
		uint8_t ccell_result_selectors_temp[16];
		memset(&ccell_results, 0, sizeof(ccell_results));
		ccell_results.m_pSelectors = &ccell_result_selectors[0];
		ccell_results.m_pSelectors_temp = &ccell_result_selectors_temp[0];

		uint64_t part_err = color_cell_compression(255, &ccell_params, &ccell_results, &comp_params);

		// ASTC
		astc_block_desc astc_results;
		memset(&astc_results, 0, sizeof(astc_results));

		astc_results.m_dual_plane = false;
		astc_results.m_weight_range = 2;

		astc_results.m_ccs = 0;
		astc_results.m_subsets = 1;
		astc_results.m_partition_seed = 0;
		astc_results.m_cem = 8;

		astc_results.m_endpoints[0] = ccell_results.m_astc_low_endpoint.m_c[0];
		astc_results.m_endpoints[1] = ccell_results.m_astc_high_endpoint.m_c[0];
		astc_results.m_endpoints[2] = ccell_results.m_astc_low_endpoint.m_c[1];
		astc_results.m_endpoints[3] = ccell_results.m_astc_high_endpoint.m_c[1];
		astc_results.m_endpoints[4] = ccell_results.m_astc_low_endpoint.m_c[2];
		astc_results.m_endpoints[5] = ccell_results.m_astc_high_endpoint.m_c[2];

		const uint32_t range = 20;

		bool invert = false;

		int s0 = g_astc_unquant[range][astc_results.m_endpoints[0]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[2]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[4]].m_unquant;
		int s1 = g_astc_unquant[range][astc_results.m_endpoints[1]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[3]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[5]].m_unquant;
		if (s1 < s0)
		{
			std::swap(astc_results.m_endpoints[0], astc_results.m_endpoints[1]);
			std::swap(astc_results.m_endpoints[2], astc_results.m_endpoints[3]);
			std::swap(astc_results.m_endpoints[4], astc_results.m_endpoints[5]);
			invert = true;
		}

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				astc_results.m_weights[x + y * 4] = ccell_result_selectors[x + y * 4];

				if (invert)
					astc_results.m_weights[x + y * 4] = 3 - astc_results.m_weights[x + y * 4];
			}
		}

		assert(total_results < MAX_ENCODE_RESULTS);
		if (total_results < MAX_ENCODE_RESULTS)
		{
			pResults[total_results].m_uastc_mode = 1;
			pResults[total_results].m_common_pattern = 0;
			pResults[total_results].m_astc = astc_results;
			pResults[total_results].m_astc_err = part_err;
			total_results++;
		}
	}

	static uint32_t estimate_partition2(uint32_t num_weights, uint32_t num_comps, const uint32_t* pWeights, const color_rgba block[4][4], const uint32_t weights[4])
	{
		assert(pWeights[0] == 0 && pWeights[num_weights - 1] == 64);

		uint64_t best_err = UINT64_MAX;
		uint32_t best_common_pattern = 0;

		for (uint32_t common_pattern = 0; common_pattern < TOTAL_ASTC_BC7_COMMON_PARTITIONS2; common_pattern++)
		{
			const uint32_t bc7_pattern = g_astc_bc7_common_partitions2[common_pattern].m_bc7;

			const uint8_t* pPartition = &g_bc7_partition2[bc7_pattern * 16];

			color_quad_u8 subset_colors[2][16];
			uint32_t subset_total_colors[2] = { 0, 0 };
			for (uint32_t index = 0; index < 16; index++)
				subset_colors[pPartition[index]][subset_total_colors[pPartition[index]]++] = ((const color_quad_u8*)block)[index];

			uint64_t total_subset_err = 0;
			for (uint32_t subset = 0; (subset < 2) && (total_subset_err < best_err); subset++)
				total_subset_err += color_cell_compression_est_astc(num_weights, num_comps, pWeights, subset_total_colors[subset], &subset_colors[subset][0], best_err, weights);

			if (total_subset_err < best_err)
			{
				best_err = total_subset_err;
				best_common_pattern = common_pattern;
			}
		}

		return best_common_pattern;
	}

	// MODE 2
	// 2-subset, 3-bit indices, 4-bit endpoints, BC7 mode 1
	// DualPlane: 0, WeightRange: 5 (8), Subsets: 2, CEM: 8 (RGB Direct       ), EndpointRange: 8 (16)          MODE1
	static void astc_mode2(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params, bool estimate_partition)
	{
		uint32_t first_common_pattern = 0;
		uint32_t last_common_pattern = TOTAL_ASTC_BC7_COMMON_PARTITIONS2;

		if (estimate_partition)
		{
			const uint32_t weights[4] = { 1, 1, 1, 1 };
			first_common_pattern = estimate_partition2(8, 3, g_bc7_weights3, block, weights);
			last_common_pattern = first_common_pattern + 1;
		}

		for (uint32_t common_pattern = first_common_pattern; common_pattern < last_common_pattern; common_pattern++)
		{
			const uint32_t bc7_pattern = g_astc_bc7_common_partitions2[common_pattern].m_bc7;

			color_rgba part_pixels[2][16];
			uint32_t part_pixel_index[4][4];
			uint32_t num_part_pixels[2] = { 0, 0 };

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t part = g_bc7_partition2[16 * bc7_pattern + x + y * 4];
					part_pixel_index[y][x] = num_part_pixels[part];
					part_pixels[part][num_part_pixels[part]++] = block[y][x];
				}
			}

			color_cell_compressor_params ccell_params[2];
			color_cell_compressor_results ccell_results[2];
			uint8_t ccell_result_selectors[2][16];
			uint8_t ccell_result_selectors_temp[2][16];

			uint64_t total_part_err = 0;
			for (uint32_t part = 0; part < 2; part++)
			{
				memset(&ccell_params[part], 0, sizeof(ccell_params[part]));

				ccell_params[part].m_num_pixels = num_part_pixels[part];
				ccell_params[part].m_pPixels = (color_quad_u8*)&part_pixels[part][0];
				ccell_params[part].m_num_selector_weights = 8;
				ccell_params[part].m_pSelector_weights = g_bc7_weights3;
				ccell_params[part].m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights3x;
				ccell_params[part].m_astc_endpoint_range = 8;
				ccell_params[part].m_weights[0] = 1;
				ccell_params[part].m_weights[1] = 1;
				ccell_params[part].m_weights[2] = 1;
				ccell_params[part].m_weights[3] = 1;

				memset(&ccell_results[part], 0, sizeof(ccell_results[part]));
				ccell_results[part].m_pSelectors = &ccell_result_selectors[part][0];
				ccell_results[part].m_pSelectors_temp = &ccell_result_selectors_temp[part][0];

				uint64_t part_err = color_cell_compression(255, &ccell_params[part], &ccell_results[part], &comp_params);
				total_part_err += part_err;
			} // part

			{
				// ASTC
				astc_block_desc astc_results;
				memset(&astc_results, 0, sizeof(astc_results));

				astc_results.m_dual_plane = false;
				astc_results.m_weight_range = 5;

				astc_results.m_ccs = 0;
				astc_results.m_subsets = 2;
				astc_results.m_partition_seed = g_astc_bc7_common_partitions2[common_pattern].m_astc;
				astc_results.m_cem = 8;

				uint32_t p0 = 0;
				uint32_t p1 = 1;
				if (g_astc_bc7_common_partitions2[common_pattern].m_invert)
					std::swap(p0, p1);

				astc_results.m_endpoints[0] = ccell_results[p0].m_astc_low_endpoint.m_c[0];
				astc_results.m_endpoints[1] = ccell_results[p0].m_astc_high_endpoint.m_c[0];
				astc_results.m_endpoints[2] = ccell_results[p0].m_astc_low_endpoint.m_c[1];
				astc_results.m_endpoints[3] = ccell_results[p0].m_astc_high_endpoint.m_c[1];
				astc_results.m_endpoints[4] = ccell_results[p0].m_astc_low_endpoint.m_c[2];
				astc_results.m_endpoints[5] = ccell_results[p0].m_astc_high_endpoint.m_c[2];

				const uint32_t range = 8;

				bool invert[2] = { false, false };

				int s0 = g_astc_unquant[range][astc_results.m_endpoints[0]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[2]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[4]].m_unquant;
				int s1 = g_astc_unquant[range][astc_results.m_endpoints[1]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[3]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[5]].m_unquant;
				if (s1 < s0)
				{
					std::swap(astc_results.m_endpoints[0], astc_results.m_endpoints[1]);
					std::swap(astc_results.m_endpoints[2], astc_results.m_endpoints[3]);
					std::swap(astc_results.m_endpoints[4], astc_results.m_endpoints[5]);
					invert[0] = true;
				}

				astc_results.m_endpoints[6] = ccell_results[p1].m_astc_low_endpoint.m_c[0];
				astc_results.m_endpoints[7] = ccell_results[p1].m_astc_high_endpoint.m_c[0];
				astc_results.m_endpoints[8] = ccell_results[p1].m_astc_low_endpoint.m_c[1];
				astc_results.m_endpoints[9] = ccell_results[p1].m_astc_high_endpoint.m_c[1];
				astc_results.m_endpoints[10] = ccell_results[p1].m_astc_low_endpoint.m_c[2];
				astc_results.m_endpoints[11] = ccell_results[p1].m_astc_high_endpoint.m_c[2];

				s0 = g_astc_unquant[range][astc_results.m_endpoints[0 + 6]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[2 + 6]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[4 + 6]].m_unquant;
				s1 = g_astc_unquant[range][astc_results.m_endpoints[1 + 6]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[3 + 6]].m_unquant + g_astc_unquant[range][astc_results.m_endpoints[5 + 6]].m_unquant;

				if (s1 < s0)
				{
					std::swap(astc_results.m_endpoints[0 + 6], astc_results.m_endpoints[1 + 6]);
					std::swap(astc_results.m_endpoints[2 + 6], astc_results.m_endpoints[3 + 6]);
					std::swap(astc_results.m_endpoints[4 + 6], astc_results.m_endpoints[5 + 6]);
					invert[1] = true;
				}

				for (uint32_t y = 0; y < 4; y++)
				{
					for (uint32_t x = 0; x < 4; x++)
					{
						const uint32_t bc7_part = g_bc7_partition2[16 * bc7_pattern + x + y * 4];

						astc_results.m_weights[x + y * 4] = ccell_result_selectors[bc7_part][part_pixel_index[y][x]];

						uint32_t astc_part = bc7_part;
						if (g_astc_bc7_common_partitions2[common_pattern].m_invert)
							astc_part = 1 - astc_part;

						if (invert[astc_part])
							astc_results.m_weights[x + y * 4] = 7 - astc_results.m_weights[x + y * 4];
					}
				}

				assert(total_results < MAX_ENCODE_RESULTS);
				if (total_results < MAX_ENCODE_RESULTS)
				{
					pResults[total_results].m_uastc_mode = 2;
					pResults[total_results].m_common_pattern = common_pattern;
					pResults[total_results].m_astc = astc_results;
					pResults[total_results].m_astc_err = total_part_err;
					total_results++;
				}
			}

		} // common_pattern
	}

	// MODE 3
	// 3-subsets, 2-bit indices, [0,11] endpoints, BC7 mode 2
	// DualPlane: 0, WeightRange: 2 (4), Subsets: 3, CEM: 8 (RGB Direct	     ), EndpointRange: 7 (12)		   MODE2
	static void astc_mode3(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params, bool estimate_partition)
	{
		uint32_t first_common_pattern = 0;
		uint32_t last_common_pattern = TOTAL_ASTC_BC7_COMMON_PARTITIONS3;

		if (estimate_partition)
		{
			uint64_t best_err = UINT64_MAX;
			uint32_t best_common_pattern = 0;
			const uint32_t weights[4] = { 1, 1, 1, 1 };

			for (uint32_t common_pattern = 0; common_pattern < TOTAL_ASTC_BC7_COMMON_PARTITIONS3; common_pattern++)
			{
				const uint32_t bc7_pattern = g_astc_bc7_common_partitions3[common_pattern].m_bc7;

				const uint8_t* pPartition = &g_bc7_partition3[bc7_pattern * 16];

				color_quad_u8 subset_colors[3][16];
				uint32_t subset_total_colors[3] = { 0, 0 };
				for (uint32_t index = 0; index < 16; index++)
					subset_colors[pPartition[index]][subset_total_colors[pPartition[index]]++] = ((const color_quad_u8*)block)[index];

				uint64_t total_subset_err = 0;
				for (uint32_t subset = 0; (subset < 3) && (total_subset_err < best_err); subset++)
					total_subset_err += color_cell_compression_est_astc(4, 3, g_bc7_weights2, subset_total_colors[subset], &subset_colors[subset][0], best_err, weights);

				if (total_subset_err < best_err)
				{
					best_err = total_subset_err;
					best_common_pattern = common_pattern;
				}
			}

			first_common_pattern = best_common_pattern;
			last_common_pattern = best_common_pattern + 1;
		}

		for (uint32_t common_pattern = first_common_pattern; common_pattern < last_common_pattern; common_pattern++)
		{
			const uint32_t endpoint_range = 7;

			const uint32_t bc7_pattern = g_astc_bc7_common_partitions3[common_pattern].m_bc7;

			color_rgba part_pixels[3][16];
			uint32_t part_pixel_index[4][4];
			uint32_t num_part_pixels[3] = { 0, 0, 0 };

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t bc7_part = g_bc7_partition3[16 * bc7_pattern + x + y * 4];
					part_pixel_index[y][x] = num_part_pixels[bc7_part];
					part_pixels[bc7_part][num_part_pixels[bc7_part]++] = block[y][x];
				}
			}

			color_cell_compressor_params ccell_params[3];
			color_cell_compressor_results ccell_results[3];
			uint8_t ccell_result_selectors[3][16];
			uint8_t ccell_result_selectors_temp[3][16];

			uint64_t total_part_err = 0;
			for (uint32_t bc7_part = 0; bc7_part < 3; bc7_part++)
			{
				memset(&ccell_params[bc7_part], 0, sizeof(ccell_params[bc7_part]));

				ccell_params[bc7_part].m_num_pixels = num_part_pixels[bc7_part];
				ccell_params[bc7_part].m_pPixels = (color_quad_u8*)&part_pixels[bc7_part][0];
				ccell_params[bc7_part].m_num_selector_weights = 4;
				ccell_params[bc7_part].m_pSelector_weights = g_bc7_weights2;
				ccell_params[bc7_part].m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
				ccell_params[bc7_part].m_astc_endpoint_range = endpoint_range;
				ccell_params[bc7_part].m_weights[0] = 1;
				ccell_params[bc7_part].m_weights[1] = 1;
				ccell_params[bc7_part].m_weights[2] = 1;
				ccell_params[bc7_part].m_weights[3] = 1;

				memset(&ccell_results[bc7_part], 0, sizeof(ccell_results[bc7_part]));
				ccell_results[bc7_part].m_pSelectors = &ccell_result_selectors[bc7_part][0];
				ccell_results[bc7_part].m_pSelectors_temp = &ccell_result_selectors_temp[bc7_part][0];

				uint64_t part_err = color_cell_compression(255, &ccell_params[bc7_part], &ccell_results[bc7_part], &comp_params);
				total_part_err += part_err;
			} // part

			{
				// ASTC
				astc_block_desc astc_results;
				memset(&astc_results, 0, sizeof(astc_results));

				astc_results.m_dual_plane = false;
				astc_results.m_weight_range = 2;

				astc_results.m_ccs = 0;
				astc_results.m_subsets = 3;
				astc_results.m_partition_seed = g_astc_bc7_common_partitions3[common_pattern].m_astc;
				astc_results.m_cem = 8;

				uint32_t astc_to_bc7_part[3]; // converts ASTC to BC7 partition index
				const uint32_t perm = g_astc_bc7_common_partitions3[common_pattern].m_astc_to_bc7_perm;
				astc_to_bc7_part[0] = g_astc_to_bc7_partition_index_perm_tables[perm][0];
				astc_to_bc7_part[1] = g_astc_to_bc7_partition_index_perm_tables[perm][1];
				astc_to_bc7_part[2] = g_astc_to_bc7_partition_index_perm_tables[perm][2];

				bool invert_astc_part[3] = { false, false, false };

				for (uint32_t astc_part = 0; astc_part < 3; astc_part++)
				{
					uint8_t* pEndpoints = &astc_results.m_endpoints[6 * astc_part];

					pEndpoints[0] = ccell_results[astc_to_bc7_part[astc_part]].m_astc_low_endpoint.m_c[0];
					pEndpoints[1] = ccell_results[astc_to_bc7_part[astc_part]].m_astc_high_endpoint.m_c[0];
					pEndpoints[2] = ccell_results[astc_to_bc7_part[astc_part]].m_astc_low_endpoint.m_c[1];
					pEndpoints[3] = ccell_results[astc_to_bc7_part[astc_part]].m_astc_high_endpoint.m_c[1];
					pEndpoints[4] = ccell_results[astc_to_bc7_part[astc_part]].m_astc_low_endpoint.m_c[2];
					pEndpoints[5] = ccell_results[astc_to_bc7_part[astc_part]].m_astc_high_endpoint.m_c[2];

					int s0 = g_astc_unquant[endpoint_range][pEndpoints[0]].m_unquant + g_astc_unquant[endpoint_range][pEndpoints[2]].m_unquant + g_astc_unquant[endpoint_range][pEndpoints[4]].m_unquant;
					int s1 = g_astc_unquant[endpoint_range][pEndpoints[1]].m_unquant + g_astc_unquant[endpoint_range][pEndpoints[3]].m_unquant + g_astc_unquant[endpoint_range][pEndpoints[5]].m_unquant;
					if (s1 < s0)
					{
						std::swap(pEndpoints[0], pEndpoints[1]);
						std::swap(pEndpoints[2], pEndpoints[3]);
						std::swap(pEndpoints[4], pEndpoints[5]);
						invert_astc_part[astc_part] = true;
					}
				}

				for (uint32_t y = 0; y < 4; y++)
				{
					for (uint32_t x = 0; x < 4; x++)
					{
						const uint32_t bc7_part = g_bc7_partition3[16 * bc7_pattern + x + y * 4];

						astc_results.m_weights[x + y * 4] = ccell_result_selectors[bc7_part][part_pixel_index[y][x]];

						uint32_t astc_part = 0;
						for (uint32_t i = 0; i < 3; i++)
						{
							if (astc_to_bc7_part[i] == bc7_part)
							{
								astc_part = i;
								break;
							}
						}

						if (invert_astc_part[astc_part])
							astc_results.m_weights[x + y * 4] = 3 - astc_results.m_weights[x + y * 4];
					}
				}

				assert(total_results < MAX_ENCODE_RESULTS);
				if (total_results < MAX_ENCODE_RESULTS)
				{
					pResults[total_results].m_uastc_mode = 3;
					pResults[total_results].m_common_pattern = common_pattern;
					pResults[total_results].m_astc = astc_results;
					pResults[total_results].m_astc_err = total_part_err;
					total_results++;
				}

			}

		} // common_pattern
	}

	// MODE 4
	// DualPlane: 0, WeightRange: 2 (4), Subsets: 2, CEM: 8 (RGB Direct       ), EndpointRange: 12 (40)         MODE3
	static void astc_mode4(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params, bool estimate_partition)
	{
		//const uint32_t weight_range = 2;
		const uint32_t endpoint_range = 12;

		uint32_t first_common_pattern = 0;
		uint32_t last_common_pattern = TOTAL_ASTC_BC7_COMMON_PARTITIONS2;

		if (estimate_partition)
		{
			const uint32_t weights[4] = { 1, 1, 1, 1 };
			first_common_pattern = estimate_partition2(4, 3, g_bc7_weights2, block, weights);
			last_common_pattern = first_common_pattern + 1;
		}

		for (uint32_t common_pattern = first_common_pattern; common_pattern < last_common_pattern; common_pattern++)
		{
			const uint32_t bc7_pattern = g_astc_bc7_common_partitions2[common_pattern].m_bc7;

			color_rgba part_pixels[2][16];
			uint32_t part_pixel_index[4][4];
			uint32_t num_part_pixels[2] = { 0, 0 };

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t part = g_bc7_partition2[16 * bc7_pattern + x + y * 4];
					part_pixel_index[y][x] = num_part_pixels[part];
					part_pixels[part][num_part_pixels[part]++] = block[y][x];
				}
			}

			color_cell_compressor_params ccell_params[2];
			color_cell_compressor_results ccell_results[2];
			uint8_t ccell_result_selectors[2][16];
			uint8_t ccell_result_selectors_temp[2][16];

			uint64_t total_part_err = 0;
			for (uint32_t part = 0; part < 2; part++)
			{
				memset(&ccell_params[part], 0, sizeof(ccell_params[part]));

				ccell_params[part].m_num_pixels = num_part_pixels[part];
				ccell_params[part].m_pPixels = (color_quad_u8*)&part_pixels[part][0];
				ccell_params[part].m_num_selector_weights = 4;
				ccell_params[part].m_pSelector_weights = g_bc7_weights2;
				ccell_params[part].m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
				ccell_params[part].m_astc_endpoint_range = endpoint_range;
				ccell_params[part].m_weights[0] = 1;
				ccell_params[part].m_weights[1] = 1;
				ccell_params[part].m_weights[2] = 1;
				ccell_params[part].m_weights[3] = 1;

				memset(&ccell_results[part], 0, sizeof(ccell_results[part]));
				ccell_results[part].m_pSelectors = &ccell_result_selectors[part][0];
				ccell_results[part].m_pSelectors_temp = &ccell_result_selectors_temp[part][0];

				uint64_t part_err = color_cell_compression(255, &ccell_params[part], &ccell_results[part], &comp_params);
				total_part_err += part_err;
			} // part

			// ASTC
			astc_block_desc astc_results;
			memset(&astc_results, 0, sizeof(astc_results));

			astc_results.m_dual_plane = false;
			astc_results.m_weight_range = 2;

			astc_results.m_ccs = 0;
			astc_results.m_subsets = 2;
			astc_results.m_partition_seed = g_astc_bc7_common_partitions2[common_pattern].m_astc;
			astc_results.m_cem = 8;

			uint32_t p0 = 0;
			uint32_t p1 = 1;
			if (g_astc_bc7_common_partitions2[common_pattern].m_invert)
				std::swap(p0, p1);

			astc_results.m_endpoints[0] = ccell_results[p0].m_astc_low_endpoint.m_c[0];
			astc_results.m_endpoints[1] = ccell_results[p0].m_astc_high_endpoint.m_c[0];
			astc_results.m_endpoints[2] = ccell_results[p0].m_astc_low_endpoint.m_c[1];
			astc_results.m_endpoints[3] = ccell_results[p0].m_astc_high_endpoint.m_c[1];
			astc_results.m_endpoints[4] = ccell_results[p0].m_astc_low_endpoint.m_c[2];
			astc_results.m_endpoints[5] = ccell_results[p0].m_astc_high_endpoint.m_c[2];

			bool invert[2] = { false, false };

			int s0 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[4]].m_unquant;
			int s1 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[5]].m_unquant;
			if (s1 < s0)
			{
				std::swap(astc_results.m_endpoints[0], astc_results.m_endpoints[1]);
				std::swap(astc_results.m_endpoints[2], astc_results.m_endpoints[3]);
				std::swap(astc_results.m_endpoints[4], astc_results.m_endpoints[5]);
				invert[0] = true;
			}

			astc_results.m_endpoints[6] = ccell_results[p1].m_astc_low_endpoint.m_c[0];
			astc_results.m_endpoints[7] = ccell_results[p1].m_astc_high_endpoint.m_c[0];
			astc_results.m_endpoints[8] = ccell_results[p1].m_astc_low_endpoint.m_c[1];
			astc_results.m_endpoints[9] = ccell_results[p1].m_astc_high_endpoint.m_c[1];
			astc_results.m_endpoints[10] = ccell_results[p1].m_astc_low_endpoint.m_c[2];
			astc_results.m_endpoints[11] = ccell_results[p1].m_astc_high_endpoint.m_c[2];

			s0 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[0 + 6]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[2 + 6]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[4 + 6]].m_unquant;
			s1 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[1 + 6]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[3 + 6]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[5 + 6]].m_unquant;

			if (s1 < s0)
			{
				std::swap(astc_results.m_endpoints[0 + 6], astc_results.m_endpoints[1 + 6]);
				std::swap(astc_results.m_endpoints[2 + 6], astc_results.m_endpoints[3 + 6]);
				std::swap(astc_results.m_endpoints[4 + 6], astc_results.m_endpoints[5 + 6]);
				invert[1] = true;
			}

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t bc7_part = g_bc7_partition2[16 * bc7_pattern + x + y * 4];

					astc_results.m_weights[x + y * 4] = ccell_result_selectors[bc7_part][part_pixel_index[y][x]];

					uint32_t astc_part = bc7_part;
					if (g_astc_bc7_common_partitions2[common_pattern].m_invert)
						astc_part = 1 - astc_part;

					if (invert[astc_part])
						astc_results.m_weights[x + y * 4] = 3 - astc_results.m_weights[x + y * 4];
				}
			}

			assert(total_results < MAX_ENCODE_RESULTS);
			if (total_results < MAX_ENCODE_RESULTS)
			{
				pResults[total_results].m_uastc_mode = 4;
				pResults[total_results].m_common_pattern = common_pattern;
				pResults[total_results].m_astc = astc_results;
				pResults[total_results].m_astc_err = total_part_err;
				total_results++;
			}

		} // common_pattern
	}

	// MODE 5 
	// DualPlane: 0, WeightRange: 5 (8), Subsets: 1, CEM: 8 (RGB Direct       ), EndpointRange: 20 (256) 		BC7 MODE 6 (or MODE 1 1-subset)
	static void astc_mode5(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		const uint32_t weight_range = 5;
		const uint32_t endpoint_range = 20;

		color_cell_compressor_params ccell_params;
		memset(&ccell_params, 0, sizeof(ccell_params));

		ccell_params.m_num_pixels = 16;
		ccell_params.m_pPixels = (color_quad_u8*)&block[0][0];
		ccell_params.m_num_selector_weights = 8;
		ccell_params.m_pSelector_weights = g_bc7_weights3;
		ccell_params.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights3x;
		ccell_params.m_astc_endpoint_range = endpoint_range;
		ccell_params.m_weights[0] = 1;
		ccell_params.m_weights[1] = 1;
		ccell_params.m_weights[2] = 1;
		ccell_params.m_weights[3] = 1;

		color_cell_compressor_results ccell_results;
		uint8_t ccell_result_selectors[16];
		uint8_t ccell_result_selectors_temp[16];
		memset(&ccell_results, 0, sizeof(ccell_results));
		ccell_results.m_pSelectors = &ccell_result_selectors[0];
		ccell_results.m_pSelectors_temp = &ccell_result_selectors_temp[0];

		uint64_t part_err = color_cell_compression(255, &ccell_params, &ccell_results, &comp_params);

		// ASTC
		astc_block_desc blk;
		memset(&blk, 0, sizeof(blk));

		blk.m_dual_plane = false;
		blk.m_weight_range = weight_range;

		blk.m_ccs = 0;
		blk.m_subsets = 1;
		blk.m_partition_seed = 0;
		blk.m_cem = 8;

		blk.m_endpoints[0] = ccell_results.m_astc_low_endpoint.m_c[0];
		blk.m_endpoints[1] = ccell_results.m_astc_high_endpoint.m_c[0];
		blk.m_endpoints[2] = ccell_results.m_astc_low_endpoint.m_c[1];
		blk.m_endpoints[3] = ccell_results.m_astc_high_endpoint.m_c[1];
		blk.m_endpoints[4] = ccell_results.m_astc_low_endpoint.m_c[2];
		blk.m_endpoints[5] = ccell_results.m_astc_high_endpoint.m_c[2];

		bool invert = false;

		int s0 = g_astc_unquant[endpoint_range][blk.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[4]].m_unquant;
		int s1 = g_astc_unquant[endpoint_range][blk.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[5]].m_unquant;
		if (s1 < s0)
		{
			std::swap(blk.m_endpoints[0], blk.m_endpoints[1]);
			std::swap(blk.m_endpoints[2], blk.m_endpoints[3]);
			std::swap(blk.m_endpoints[4], blk.m_endpoints[5]);
			invert = true;
		}

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				blk.m_weights[x + y * 4] = ccell_result_selectors[x + y * 4];

				if (invert)
					blk.m_weights[x + y * 4] = 7 - blk.m_weights[x + y * 4];
			}
		}

		assert(total_results < MAX_ENCODE_RESULTS);
		if (total_results < MAX_ENCODE_RESULTS)
		{
			pResults[total_results].m_uastc_mode = 5;
			pResults[total_results].m_common_pattern = 0;
			pResults[total_results].m_astc = blk;
			pResults[total_results].m_astc_err = part_err;
			total_results++;
		}
	}

	// MODE 6
	// DualPlane: 1, WeightRange: 2 (4), Subsets: 1, CEM: 8 (RGB Direct       ), EndpointRange: 18 (160)		BC7 MODE5
	static void astc_mode6(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		for (uint32_t rot_comp = 0; rot_comp < 3; rot_comp++)
		{
			const uint32_t weight_range = 2;
			const uint32_t endpoint_range = 18;

			color_quad_u8 block_rgb[16];
			color_quad_u8 block_a[16];
			for (uint32_t i = 0; i < 16; i++)
			{
				block_rgb[i] = ((color_quad_u8*)&block[0][0])[i];
				block_a[i] = block_rgb[i];

				uint8_t c = block_a[i].m_c[rot_comp];
				block_a[i].m_c[0] = c;
				block_a[i].m_c[1] = c;
				block_a[i].m_c[2] = c;
				block_a[i].m_c[3] = 255;

				block_rgb[i].m_c[rot_comp] = 255;
			}

			uint8_t ccell_result_selectors_temp[16];

			color_cell_compressor_params ccell_params_rgb;
			memset(&ccell_params_rgb, 0, sizeof(ccell_params_rgb));

			ccell_params_rgb.m_num_pixels = 16;
			ccell_params_rgb.m_pPixels = block_rgb;
			ccell_params_rgb.m_num_selector_weights = 4;
			ccell_params_rgb.m_pSelector_weights = g_bc7_weights2;
			ccell_params_rgb.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
			ccell_params_rgb.m_astc_endpoint_range = endpoint_range;
			ccell_params_rgb.m_weights[0] = 1;
			ccell_params_rgb.m_weights[1] = 1;
			ccell_params_rgb.m_weights[2] = 1;
			ccell_params_rgb.m_weights[3] = 1;

			color_cell_compressor_results ccell_results_rgb;
			uint8_t ccell_result_selectors_rgb[16];
			memset(&ccell_results_rgb, 0, sizeof(ccell_results_rgb));
			ccell_results_rgb.m_pSelectors = &ccell_result_selectors_rgb[0];
			ccell_results_rgb.m_pSelectors_temp = &ccell_result_selectors_temp[0];

			uint64_t part_err_rgb = color_cell_compression(255, &ccell_params_rgb, &ccell_results_rgb, &comp_params);
			
			color_cell_compressor_params ccell_params_a;
			memset(&ccell_params_a, 0, sizeof(ccell_params_a));

			ccell_params_a.m_num_pixels = 16;
			ccell_params_a.m_pPixels = block_a;
			ccell_params_a.m_num_selector_weights = 4;
			ccell_params_a.m_pSelector_weights = g_bc7_weights2;
			ccell_params_a.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
			ccell_params_a.m_astc_endpoint_range = endpoint_range;
			ccell_params_a.m_weights[0] = 1;
			ccell_params_a.m_weights[1] = 1;
			ccell_params_a.m_weights[2] = 1;
			ccell_params_a.m_weights[3] = 1;

			color_cell_compressor_results ccell_results_a;
			uint8_t ccell_result_selectors_a[16];
			memset(&ccell_results_a, 0, sizeof(ccell_results_a));
			ccell_results_a.m_pSelectors = &ccell_result_selectors_a[0];
			ccell_results_a.m_pSelectors_temp = &ccell_result_selectors_temp[0];

			uint64_t part_err_a = color_cell_compression(255, &ccell_params_a, &ccell_results_a, &comp_params) / 3;

			uint64_t total_err = part_err_rgb + part_err_a;

			// ASTC
			astc_block_desc blk;
			memset(&blk, 0, sizeof(blk));

			blk.m_dual_plane = true;
			blk.m_weight_range = weight_range;

			blk.m_ccs = rot_comp;
			blk.m_subsets = 1;
			blk.m_partition_seed = 0;
			blk.m_cem = 8;

			blk.m_endpoints[0] = (rot_comp == 0 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[0];
			blk.m_endpoints[1] = (rot_comp == 0 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[0];
			blk.m_endpoints[2] = (rot_comp == 1 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[1];
			blk.m_endpoints[3] = (rot_comp == 1 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[1];
			blk.m_endpoints[4] = (rot_comp == 2 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[2];
			blk.m_endpoints[5] = (rot_comp == 2 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[2];

			bool invert = false;

			int s0 = g_astc_unquant[endpoint_range][blk.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[4]].m_unquant;
			int s1 = g_astc_unquant[endpoint_range][blk.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[5]].m_unquant;
			if (s1 < s0)
			{
				std::swap(blk.m_endpoints[0], blk.m_endpoints[1]);
				std::swap(blk.m_endpoints[2], blk.m_endpoints[3]);
				std::swap(blk.m_endpoints[4], blk.m_endpoints[5]);
				invert = true;
			}

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					uint32_t rgb_index = ccell_result_selectors_rgb[x + y * 4];
					uint32_t a_index = ccell_result_selectors_a[x + y * 4];

					if (invert)
					{
						rgb_index = 3 - rgb_index;
						a_index = 3 - a_index;
					}

					blk.m_weights[(x + y * 4) * 2 + 0] = (uint8_t)rgb_index;
					blk.m_weights[(x + y * 4) * 2 + 1] = (uint8_t)a_index;
				}
			}

			assert(total_results < MAX_ENCODE_RESULTS);
			if (total_results < MAX_ENCODE_RESULTS)
			{
				pResults[total_results].m_uastc_mode = 6;
				pResults[total_results].m_common_pattern = 0;
				pResults[total_results].m_astc = blk;
				pResults[total_results].m_astc_err = total_err;
				total_results++;
			}
		} // rot_comp
	}

	// MODE 7 - 2 subset ASTC, 3 subset BC7
	// DualPlane: 0, WeightRange: 2 (4), Subsets: 2, CEM: 8 (RGB Direct       ), EndpointRange: 12 (40)         MODE2
	static void astc_mode7(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params, bool estimate_partition)
	{
		uint32_t first_common_pattern = 0;
		uint32_t last_common_pattern = TOTAL_BC7_3_ASTC2_COMMON_PARTITIONS;

		if (estimate_partition)
		{
			uint64_t best_err = UINT64_MAX;
			uint32_t best_common_pattern = 0;
			const uint32_t weights[4] = { 1, 1, 1, 1 };

			for (uint32_t common_pattern = 0; common_pattern < TOTAL_BC7_3_ASTC2_COMMON_PARTITIONS; common_pattern++)
			{
				const uint8_t* pPartition = &g_bc7_3_astc2_patterns2[common_pattern][0];

#ifdef _DEBUG
				const uint32_t astc_pattern = g_bc7_3_astc2_common_partitions[common_pattern].m_astc2;
				const uint32_t bc7_pattern = g_bc7_3_astc2_common_partitions[common_pattern].m_bc73;
				const uint32_t common_pattern_k = g_bc7_3_astc2_common_partitions[common_pattern].k;

				for (uint32_t y = 0; y < 4; y++)
				{
					for (uint32_t x = 0; x < 4; x++)
					{
						const uint32_t astc_part = bc7_convert_partition_index_3_to_2(g_bc7_partition3[16 * bc7_pattern + x + y * 4], common_pattern_k);
						assert((int)astc_part == astc_compute_texel_partition(astc_pattern, x, y, 0, 2, true));
						assert(astc_part == pPartition[x + y * 4]);
					}
				}
#endif

				color_quad_u8 subset_colors[2][16];
				uint32_t subset_total_colors[2] = { 0, 0 };
				for (uint32_t index = 0; index < 16; index++)
					subset_colors[pPartition[index]][subset_total_colors[pPartition[index]]++] = ((const color_quad_u8*)block)[index];

				uint64_t total_subset_err = 0;
				for (uint32_t subset = 0; (subset < 2) && (total_subset_err < best_err); subset++)
					total_subset_err += color_cell_compression_est_astc(4, 3, g_bc7_weights2, subset_total_colors[subset], &subset_colors[subset][0], best_err, weights);

				if (total_subset_err < best_err)
				{
					best_err = total_subset_err;
					best_common_pattern = common_pattern;
				}
			}

			first_common_pattern = best_common_pattern;
			last_common_pattern = best_common_pattern + 1;
		}

		//const uint32_t weight_range = 2;
		const uint32_t endpoint_range = 12;

		for (uint32_t common_pattern = first_common_pattern; common_pattern < last_common_pattern; common_pattern++)
		{
			const uint32_t astc_pattern = g_bc7_3_astc2_common_partitions[common_pattern].m_astc2;
			const uint32_t bc7_pattern = g_bc7_3_astc2_common_partitions[common_pattern].m_bc73;
			const uint32_t common_pattern_k = g_bc7_3_astc2_common_partitions[common_pattern].k;

			color_rgba part_pixels[2][16];
			uint32_t part_pixel_index[4][4];
			uint32_t num_part_pixels[2] = { 0, 0 };

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t astc_part = bc7_convert_partition_index_3_to_2(g_bc7_partition3[16 * bc7_pattern + x + y * 4], common_pattern_k);
#ifdef _DEBUG					
					assert((int)astc_part == astc_compute_texel_partition(astc_pattern, x, y, 0, 2, true));
#endif					

					part_pixel_index[y][x] = num_part_pixels[astc_part];
					part_pixels[astc_part][num_part_pixels[astc_part]++] = block[y][x];
				}
			}

			color_cell_compressor_params ccell_params[2];
			color_cell_compressor_results ccell_results[2];
			uint8_t ccell_result_selectors[2][16];
			uint8_t ccell_result_selectors_temp[2][16];

			uint64_t total_part_err = 0;
			for (uint32_t part = 0; part < 2; part++)
			{
				memset(&ccell_params[part], 0, sizeof(ccell_params[part]));

				ccell_params[part].m_num_pixels = num_part_pixels[part];
				ccell_params[part].m_pPixels = (color_quad_u8*)&part_pixels[part][0];
				ccell_params[part].m_num_selector_weights = 4;
				ccell_params[part].m_pSelector_weights = g_bc7_weights2;
				ccell_params[part].m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
				ccell_params[part].m_astc_endpoint_range = endpoint_range;
				ccell_params[part].m_weights[0] = 1;
				ccell_params[part].m_weights[1] = 1;
				ccell_params[part].m_weights[2] = 1;
				ccell_params[part].m_weights[3] = 1;

				memset(&ccell_results[part], 0, sizeof(ccell_results[part]));
				ccell_results[part].m_pSelectors = &ccell_result_selectors[part][0];
				ccell_results[part].m_pSelectors_temp = &ccell_result_selectors_temp[part][0];

				uint64_t part_err = color_cell_compression(255, &ccell_params[part], &ccell_results[part], &comp_params);
				total_part_err += part_err;
			} // part

			// ASTC
			astc_block_desc blk;
			memset(&blk, 0, sizeof(blk));

			blk.m_dual_plane = false;
			blk.m_weight_range = 2;

			blk.m_ccs = 0;
			blk.m_subsets = 2;
			blk.m_partition_seed = astc_pattern;
			blk.m_cem = 8;

			const uint32_t p0 = 0;
			const uint32_t p1 = 1;

			blk.m_endpoints[0] = ccell_results[p0].m_astc_low_endpoint.m_c[0];
			blk.m_endpoints[1] = ccell_results[p0].m_astc_high_endpoint.m_c[0];
			blk.m_endpoints[2] = ccell_results[p0].m_astc_low_endpoint.m_c[1];
			blk.m_endpoints[3] = ccell_results[p0].m_astc_high_endpoint.m_c[1];
			blk.m_endpoints[4] = ccell_results[p0].m_astc_low_endpoint.m_c[2];
			blk.m_endpoints[5] = ccell_results[p0].m_astc_high_endpoint.m_c[2];

			bool invert[2] = { false, false };

			int s0 = g_astc_unquant[endpoint_range][blk.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[4]].m_unquant;
			int s1 = g_astc_unquant[endpoint_range][blk.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[5]].m_unquant;
			if (s1 < s0)
			{
				std::swap(blk.m_endpoints[0], blk.m_endpoints[1]);
				std::swap(blk.m_endpoints[2], blk.m_endpoints[3]);
				std::swap(blk.m_endpoints[4], blk.m_endpoints[5]);
				invert[0] = true;
			}

			blk.m_endpoints[6] = ccell_results[p1].m_astc_low_endpoint.m_c[0];
			blk.m_endpoints[7] = ccell_results[p1].m_astc_high_endpoint.m_c[0];
			blk.m_endpoints[8] = ccell_results[p1].m_astc_low_endpoint.m_c[1];
			blk.m_endpoints[9] = ccell_results[p1].m_astc_high_endpoint.m_c[1];
			blk.m_endpoints[10] = ccell_results[p1].m_astc_low_endpoint.m_c[2];
			blk.m_endpoints[11] = ccell_results[p1].m_astc_high_endpoint.m_c[2];

			s0 = g_astc_unquant[endpoint_range][blk.m_endpoints[0 + 6]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[2 + 6]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[4 + 6]].m_unquant;
			s1 = g_astc_unquant[endpoint_range][blk.m_endpoints[1 + 6]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[3 + 6]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[5 + 6]].m_unquant;

			if (s1 < s0)
			{
				std::swap(blk.m_endpoints[0 + 6], blk.m_endpoints[1 + 6]);
				std::swap(blk.m_endpoints[2 + 6], blk.m_endpoints[3 + 6]);
				std::swap(blk.m_endpoints[4 + 6], blk.m_endpoints[5 + 6]);
				invert[1] = true;
			}

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t astc_part = bc7_convert_partition_index_3_to_2(g_bc7_partition3[16 * bc7_pattern + x + y * 4], common_pattern_k);

					blk.m_weights[x + y * 4] = ccell_result_selectors[astc_part][part_pixel_index[y][x]];

					if (invert[astc_part])
						blk.m_weights[x + y * 4] = 3 - blk.m_weights[x + y * 4];
				}
			}

			assert(total_results < MAX_ENCODE_RESULTS);
			if (total_results < MAX_ENCODE_RESULTS)
			{
				pResults[total_results].m_uastc_mode = 7;
				pResults[total_results].m_common_pattern = common_pattern;
				pResults[total_results].m_astc = blk;
				pResults[total_results].m_astc_err = total_part_err;
				total_results++;
			}

		} // common_pattern
	}

	static void estimate_partition2_list(uint32_t num_weights, uint32_t num_comps, const uint32_t* pWeights, const color_rgba block[4][4], uint32_t* pParts, uint32_t max_parts, const uint32_t weights[4])
	{
		assert(pWeights[0] == 0 && pWeights[num_weights - 1] == 64);

		const uint32_t MAX_PARTS = 8;
		assert(max_parts <= MAX_PARTS);

		uint64_t part_error[MAX_PARTS];
		memset(part_error, 0xFF, sizeof(part_error));
		memset(pParts, 0, sizeof(pParts[0]) * max_parts);

		for (uint32_t common_pattern = 0; common_pattern < TOTAL_ASTC_BC7_COMMON_PARTITIONS2; common_pattern++)
		{
			const uint32_t bc7_pattern = g_astc_bc7_common_partitions2[common_pattern].m_bc7;

			const uint8_t* pPartition = &g_bc7_partition2[bc7_pattern * 16];

			color_quad_u8 subset_colors[2][16];
			uint32_t subset_total_colors[2] = { 0, 0 };
			for (uint32_t index = 0; index < 16; index++)
				subset_colors[pPartition[index]][subset_total_colors[pPartition[index]]++] = ((const color_quad_u8*)block)[index];

			uint64_t total_subset_err = 0;
			for (uint32_t subset = 0; subset < 2; subset++)
				total_subset_err += color_cell_compression_est_astc(num_weights, num_comps, pWeights, subset_total_colors[subset], &subset_colors[subset][0], UINT64_MAX, weights);

			for (int i = 0; i < (int)max_parts; i++)
			{
				if (total_subset_err < part_error[i])
				{
					for (int j = max_parts - 1; j > i; --j)
					{
						pParts[j] = pParts[j - 1];
						part_error[j] = part_error[j - 1];
					}

					pParts[i] = common_pattern;
					part_error[i] = total_subset_err;

					break;
				}
			}
		}

#ifdef _DEBUG
		for (uint32_t i = 0; i < max_parts - 1; i++)
		{
			assert(part_error[i] <= part_error[i + 1]);
		}
#endif
	}
		
	// 9. DualPlane: 0, WeightRange: 2 (4), Subsets: 2, CEM: 12 (RGBA Direct), EndpointRange: 8 (16) - BC7 MODE 7
	// 16. DualPlane: 0, WeightRange : 2 (4), Subsets : 2, CEM: 4 (LA Direct), EndpointRange : 20 (256) - BC7 MODE 7
	static void astc_mode9_or_16(uint32_t mode, const color_rgba source_block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params, uint32_t estimate_partition_list_size)
	{
		assert(mode == 9 || mode == 16);

		const color_rgba* pBlock = &source_block[0][0];

		color_rgba temp_block[16];
		if (mode == 16)
		{
			for (uint32_t i = 0; i < 16; i++)
			{
				if (mode == 16)
				{
					assert(pBlock[i].r == pBlock[i].g);
					assert(pBlock[i].r == pBlock[i].b);
				}

				const uint32_t l = pBlock[i].r;
				const uint32_t a = pBlock[i].a;

				// Use (l,0,0,a) not (l,l,l,a) so both components are treated equally.
				temp_block[i].set_noclamp_rgba(l, 0, 0, a);
			}

			pBlock = temp_block;
		}

		const uint32_t weights[4] = { 1, 1, 1, 1 };

		//const uint32_t weight_range = 2;
		const uint32_t endpoint_range = (mode == 16) ? 20 : 8;

		uint32_t first_common_pattern = 0;
		uint32_t last_common_pattern = TOTAL_ASTC_BC7_COMMON_PARTITIONS2;
		bool use_part_list = false;

		const uint32_t MAX_PARTS = 8;
		uint32_t parts[MAX_PARTS];

		if (estimate_partition_list_size == 1)
		{
			first_common_pattern = estimate_partition2(4, 4, g_bc7_weights2, (const color_rgba(*)[4])pBlock, weights);
			last_common_pattern = first_common_pattern + 1;
		}
		else if (estimate_partition_list_size > 0)
		{
			assert(estimate_partition_list_size <= MAX_PARTS);
			estimate_partition_list_size = basisu::minimum(estimate_partition_list_size, MAX_PARTS);

			estimate_partition2_list(4, 4, g_bc7_weights2, (const color_rgba(*)[4])pBlock, parts, estimate_partition_list_size, weights);

			first_common_pattern = 0;
			last_common_pattern = estimate_partition_list_size;
			use_part_list = true;

#ifdef _DEBUG
			assert(parts[0] == estimate_partition2(4, 4, g_bc7_weights2, (const color_rgba(*)[4])pBlock, weights));
#endif
		}

		for (uint32_t common_pattern_iter = first_common_pattern; common_pattern_iter < last_common_pattern; common_pattern_iter++)
		{
			const uint32_t common_pattern = use_part_list ? parts[common_pattern_iter] : common_pattern_iter;

			const uint32_t bc7_pattern = g_astc_bc7_common_partitions2[common_pattern].m_bc7;

			color_rgba part_pixels[2][16];
			uint32_t part_pixel_index[4][4];
			uint32_t num_part_pixels[2] = { 0, 0 };

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t part = g_bc7_partition2[16 * bc7_pattern + x + y * 4];
					part_pixel_index[y][x] = num_part_pixels[part];
					part_pixels[part][num_part_pixels[part]++] = pBlock[y * 4 + x];
				}
			}

			color_cell_compressor_params ccell_params[2];
			color_cell_compressor_results ccell_results[2];
			uint8_t ccell_result_selectors[2][16];
			uint8_t ccell_result_selectors_temp[2][16];

			uint64_t total_err = 0;
			for (uint32_t subset = 0; subset < 2; subset++)
			{
				memset(&ccell_params[subset], 0, sizeof(ccell_params[subset]));

				ccell_params[subset].m_num_pixels = num_part_pixels[subset];
				ccell_params[subset].m_pPixels = (color_quad_u8*)&part_pixels[subset][0];
				ccell_params[subset].m_num_selector_weights = 4;
				ccell_params[subset].m_pSelector_weights = g_bc7_weights2;
				ccell_params[subset].m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
				ccell_params[subset].m_astc_endpoint_range = endpoint_range;
				ccell_params[subset].m_weights[0] = weights[0];
				ccell_params[subset].m_weights[1] = weights[1];
				ccell_params[subset].m_weights[2] = weights[2];
				ccell_params[subset].m_weights[3] = weights[3];
				ccell_params[subset].m_has_alpha = true;

				memset(&ccell_results[subset], 0, sizeof(ccell_results[subset]));
				ccell_results[subset].m_pSelectors = &ccell_result_selectors[subset][0];
				ccell_results[subset].m_pSelectors_temp = &ccell_result_selectors_temp[subset][0];

				uint64_t subset_err = color_cell_compression(255, &ccell_params[subset], &ccell_results[subset], &comp_params);

				if (mode == 16)
				{
					color_rgba colors[4];
					for (uint32_t c = 0; c < 4; c++)
					{
						colors[0].m_comps[c] = g_astc_unquant[endpoint_range][ccell_results[subset].m_astc_low_endpoint.m_c[(c < 3) ? 0 : 3]].m_unquant;
						colors[3].m_comps[c] = g_astc_unquant[endpoint_range][ccell_results[subset].m_astc_high_endpoint.m_c[(c < 3) ? 0 : 3]].m_unquant;
					}

					for (uint32_t i = 1; i < 4 - 1; i++)
						for (uint32_t c = 0; c < 4; c++)
							colors[i].m_comps[c] = (uint8_t)astc_interpolate(colors[0].m_comps[c], colors[3].m_comps[c], g_bc7_weights2[i], false);

					for (uint32_t p = 0; p < ccell_params[subset].m_num_pixels; p++)
					{
						color_rgba orig_pix(part_pixels[subset][p]);
						orig_pix.g = orig_pix.r;
						orig_pix.b = orig_pix.r;
						total_err += color_distance_la(orig_pix, colors[ccell_result_selectors[subset][p]]);
					}
				}
				else
				{
					total_err += subset_err;
				}
			} // subset

			// ASTC
			astc_block_desc astc_results;
			memset(&astc_results, 0, sizeof(astc_results));

			astc_results.m_dual_plane = false;
			astc_results.m_weight_range = 2;

			astc_results.m_ccs = 0;
			astc_results.m_subsets = 2;
			astc_results.m_partition_seed = g_astc_bc7_common_partitions2[common_pattern].m_astc;
			astc_results.m_cem = (mode == 16) ? 4 : 12;

			uint32_t part[2] = { 0, 1 };
			if (g_astc_bc7_common_partitions2[common_pattern].m_invert)
				std::swap(part[0], part[1]);

			bool invert[2] = { false, false };

			for (uint32_t p = 0; p < 2; p++)
			{
				if (mode == 16)
				{
					astc_results.m_endpoints[p * 4 + 0] = ccell_results[part[p]].m_astc_low_endpoint.m_c[0];
					astc_results.m_endpoints[p * 4 + 1] = ccell_results[part[p]].m_astc_high_endpoint.m_c[0];

					astc_results.m_endpoints[p * 4 + 2] = ccell_results[part[p]].m_astc_low_endpoint.m_c[3];
					astc_results.m_endpoints[p * 4 + 3] = ccell_results[part[p]].m_astc_high_endpoint.m_c[3];
				}
				else
				{
					for (uint32_t c = 0; c < 4; c++)
					{
						astc_results.m_endpoints[p * 8 + c * 2] = ccell_results[part[p]].m_astc_low_endpoint.m_c[c];
						astc_results.m_endpoints[p * 8 + c * 2 + 1] = ccell_results[part[p]].m_astc_high_endpoint.m_c[c];
					}

					int s0 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[p * 8 + 0]].m_unquant +
						g_astc_unquant[endpoint_range][astc_results.m_endpoints[p * 8 + 2]].m_unquant +
						g_astc_unquant[endpoint_range][astc_results.m_endpoints[p * 8 + 4]].m_unquant;

					int s1 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[p * 8 + 1]].m_unquant +
						g_astc_unquant[endpoint_range][astc_results.m_endpoints[p * 8 + 3]].m_unquant +
						g_astc_unquant[endpoint_range][astc_results.m_endpoints[p * 8 + 5]].m_unquant;

					if (s1 < s0)
					{
						std::swap(astc_results.m_endpoints[p * 8 + 0], astc_results.m_endpoints[p * 8 + 1]);
						std::swap(astc_results.m_endpoints[p * 8 + 2], astc_results.m_endpoints[p * 8 + 3]);
						std::swap(astc_results.m_endpoints[p * 8 + 4], astc_results.m_endpoints[p * 8 + 5]);
						std::swap(astc_results.m_endpoints[p * 8 + 6], astc_results.m_endpoints[p * 8 + 7]);
						invert[p] = true;
					}
				}
			}

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t bc7_part = g_bc7_partition2[16 * bc7_pattern + x + y * 4];

					astc_results.m_weights[x + y * 4] = ccell_result_selectors[bc7_part][part_pixel_index[y][x]];

					uint32_t astc_part = bc7_part;
					if (g_astc_bc7_common_partitions2[common_pattern].m_invert)
						astc_part = 1 - astc_part;

					if (invert[astc_part])
						astc_results.m_weights[x + y * 4] = 3 - astc_results.m_weights[x + y * 4];
				}
			}

			assert(total_results < MAX_ENCODE_RESULTS);
			if (total_results < MAX_ENCODE_RESULTS)
			{
				pResults[total_results].m_uastc_mode = mode;
				pResults[total_results].m_common_pattern = common_pattern;
				pResults[total_results].m_astc = astc_results;
				pResults[total_results].m_astc_err = total_err;
				total_results++;
			}

		} // common_pattern
	}

	// MODE 10
	// DualPlane: 0, WeightRange: 8 (16), Subsets: 1, CEM: 12 (RGBA Direct      ), EndpointRange: 13 (48)       MODE6
	static void astc_mode10(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		const uint32_t weight_range = 8;
		const uint32_t endpoint_range = 13;

		color_cell_compressor_params ccell_params;
		memset(&ccell_params, 0, sizeof(ccell_params));

		ccell_params.m_num_pixels = 16;
		ccell_params.m_pPixels = (color_quad_u8*)&block[0][0];
		ccell_params.m_num_selector_weights = 16;
		ccell_params.m_pSelector_weights = g_astc_weights4;
		ccell_params.m_pSelector_weightsx = (const bc7enc_vec4F*)g_astc_weights4x;
		ccell_params.m_astc_endpoint_range = endpoint_range;
		ccell_params.m_weights[0] = 1;
		ccell_params.m_weights[1] = 1;
		ccell_params.m_weights[2] = 1;
		ccell_params.m_weights[3] = 1;
		ccell_params.m_has_alpha = true;

		color_cell_compressor_results ccell_results;
		uint8_t ccell_result_selectors[16];
		uint8_t ccell_result_selectors_temp[16];
		memset(&ccell_results, 0, sizeof(ccell_results));
		ccell_results.m_pSelectors = &ccell_result_selectors[0];
		ccell_results.m_pSelectors_temp = &ccell_result_selectors_temp[0];

		uint64_t part_err = color_cell_compression(255, &ccell_params, &ccell_results, &comp_params);

		// ASTC
		astc_block_desc astc_results;
		memset(&astc_results, 0, sizeof(astc_results));

		astc_results.m_dual_plane = false;
		astc_results.m_weight_range = weight_range;

		astc_results.m_ccs = 0;
		astc_results.m_subsets = 1;
		astc_results.m_partition_seed = 0;
		astc_results.m_cem = 12;

		astc_results.m_endpoints[0] = ccell_results.m_astc_low_endpoint.m_c[0];
		astc_results.m_endpoints[1] = ccell_results.m_astc_high_endpoint.m_c[0];
		astc_results.m_endpoints[2] = ccell_results.m_astc_low_endpoint.m_c[1];
		astc_results.m_endpoints[3] = ccell_results.m_astc_high_endpoint.m_c[1];
		astc_results.m_endpoints[4] = ccell_results.m_astc_low_endpoint.m_c[2];
		astc_results.m_endpoints[5] = ccell_results.m_astc_high_endpoint.m_c[2];
		astc_results.m_endpoints[6] = ccell_results.m_astc_low_endpoint.m_c[3];
		astc_results.m_endpoints[7] = ccell_results.m_astc_high_endpoint.m_c[3];

		bool invert = false;

		int s0 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[4]].m_unquant;
		int s1 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[5]].m_unquant;
		if (s1 < s0)
		{
			std::swap(astc_results.m_endpoints[0], astc_results.m_endpoints[1]);
			std::swap(astc_results.m_endpoints[2], astc_results.m_endpoints[3]);
			std::swap(astc_results.m_endpoints[4], astc_results.m_endpoints[5]);
			std::swap(astc_results.m_endpoints[6], astc_results.m_endpoints[7]);
			invert = true;
		}

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				astc_results.m_weights[x + y * 4] = ccell_result_selectors[x + y * 4];

				if (invert)
					astc_results.m_weights[x + y * 4] = 15 - astc_results.m_weights[x + y * 4];
			}
		}

		assert(total_results < MAX_ENCODE_RESULTS);
		if (total_results < MAX_ENCODE_RESULTS)
		{
			pResults[total_results].m_uastc_mode = 10;
			pResults[total_results].m_common_pattern = 0;
			pResults[total_results].m_astc = astc_results;
			pResults[total_results].m_astc_err = part_err;
			total_results++;
		}
	}

	// 11. DualPlane: 1, WeightRange: 2 (4), Subsets: 1, CEM: 12 (RGBA Direct), EndpointRange: 13 (48)        MODE5
	// 17. DualPlane: 1, WeightRange : 2 (4), Subsets : 1, CEM : 4 (LA Direct), EndpointRange : 20 (256)    BC7 MODE5
	static void astc_mode11_or_17(uint32_t mode, const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		assert((mode == 11) || (mode == 17));

		const uint32_t weight_range = 2;
		const uint32_t endpoint_range = (mode == 17) ? 20 : 13;

		bc7enc_compress_block_params local_comp_params(comp_params);
		local_comp_params.m_perceptual = false;
		local_comp_params.m_weights[0] = 1;
		local_comp_params.m_weights[1] = 1;
		local_comp_params.m_weights[2] = 1;
		local_comp_params.m_weights[3] = 1;

		const uint32_t last_rot_comp = (mode == 17) ? 1 : 4;

		for (uint32_t rot_comp = 0; rot_comp < last_rot_comp; rot_comp++)
		{
			color_quad_u8 block_rgb[16];
			color_quad_u8 block_a[16];
			for (uint32_t i = 0; i < 16; i++)
			{
				block_rgb[i] = ((color_quad_u8*)&block[0][0])[i];
				block_a[i] = block_rgb[i];

				if (mode == 17)
				{
					assert(block_rgb[i].m_c[0] == block_rgb[i].m_c[1]);
					assert(block_rgb[i].m_c[0] == block_rgb[i].m_c[2]);

					block_a[i].m_c[0] = block_rgb[i].m_c[3];
					block_a[i].m_c[1] = block_rgb[i].m_c[3];
					block_a[i].m_c[2] = block_rgb[i].m_c[3];
					block_a[i].m_c[3] = 255;

					block_rgb[i].m_c[1] = block_rgb[i].m_c[0];
					block_rgb[i].m_c[2] = block_rgb[i].m_c[0];
					block_rgb[i].m_c[3] = 255;
				}
				else
				{
					uint8_t c = block_a[i].m_c[rot_comp];
					block_a[i].m_c[0] = c;
					block_a[i].m_c[1] = c;
					block_a[i].m_c[2] = c;
					block_a[i].m_c[3] = 255;

					block_rgb[i].m_c[rot_comp] = block_rgb[i].m_c[3];
					block_rgb[i].m_c[3] = 255;
				}
			}

			uint8_t ccell_result_selectors_temp[16];

			color_cell_compressor_params ccell_params_rgb;
			memset(&ccell_params_rgb, 0, sizeof(ccell_params_rgb));

			ccell_params_rgb.m_num_pixels = 16;
			ccell_params_rgb.m_pPixels = block_rgb;
			ccell_params_rgb.m_num_selector_weights = 4;
			ccell_params_rgb.m_pSelector_weights = g_bc7_weights2;
			ccell_params_rgb.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
			ccell_params_rgb.m_astc_endpoint_range = endpoint_range;
			ccell_params_rgb.m_weights[0] = 1;
			ccell_params_rgb.m_weights[1] = 1;
			ccell_params_rgb.m_weights[2] = 1;
			ccell_params_rgb.m_weights[3] = 1;

			color_cell_compressor_results ccell_results_rgb;
			uint8_t ccell_result_selectors_rgb[16];
			memset(&ccell_results_rgb, 0, sizeof(ccell_results_rgb));
			ccell_results_rgb.m_pSelectors = &ccell_result_selectors_rgb[0];
			ccell_results_rgb.m_pSelectors_temp = &ccell_result_selectors_temp[0];

			uint64_t part_err_rgb = color_cell_compression(255, &ccell_params_rgb, &ccell_results_rgb, &local_comp_params);

			color_cell_compressor_params ccell_params_a;
			memset(&ccell_params_a, 0, sizeof(ccell_params_a));

			ccell_params_a.m_num_pixels = 16;
			ccell_params_a.m_pPixels = block_a;
			ccell_params_a.m_num_selector_weights = 4;
			ccell_params_a.m_pSelector_weights = g_bc7_weights2;
			ccell_params_a.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
			ccell_params_a.m_astc_endpoint_range = endpoint_range;
			ccell_params_a.m_weights[0] = 1;
			ccell_params_a.m_weights[1] = 1;
			ccell_params_a.m_weights[2] = 1;
			ccell_params_a.m_weights[3] = 1;

			color_cell_compressor_results ccell_results_a;
			uint8_t ccell_result_selectors_a[16];
			memset(&ccell_results_a, 0, sizeof(ccell_results_a));
			ccell_results_a.m_pSelectors = &ccell_result_selectors_a[0];
			ccell_results_a.m_pSelectors_temp = &ccell_result_selectors_temp[0];

			uint64_t part_err_a = color_cell_compression(255, &ccell_params_a, &ccell_results_a, &local_comp_params) / 3;

			uint64_t total_err = (mode == 17) ? ((part_err_rgb / 3) + part_err_a) : (part_err_rgb + part_err_a);

			// ASTC
			astc_block_desc blk;
			memset(&blk, 0, sizeof(blk));

			blk.m_dual_plane = true;
			blk.m_weight_range = weight_range;

			blk.m_ccs = (mode == 17) ? 3 : rot_comp;
			blk.m_subsets = 1;
			blk.m_partition_seed = 0;
			blk.m_cem = (mode == 17) ? 4 : 12;

			bool invert = false;

			if (mode == 17)
			{
				assert(ccell_results_rgb.m_astc_low_endpoint.m_c[0] == ccell_results_rgb.m_astc_low_endpoint.m_c[1]);
				assert(ccell_results_rgb.m_astc_low_endpoint.m_c[0] == ccell_results_rgb.m_astc_low_endpoint.m_c[2]);

				assert(ccell_results_rgb.m_astc_high_endpoint.m_c[0] == ccell_results_rgb.m_astc_high_endpoint.m_c[1]);
				assert(ccell_results_rgb.m_astc_high_endpoint.m_c[0] == ccell_results_rgb.m_astc_high_endpoint.m_c[2]);

				blk.m_endpoints[0] = ccell_results_rgb.m_astc_low_endpoint.m_c[0];
				blk.m_endpoints[1] = ccell_results_rgb.m_astc_high_endpoint.m_c[0];

				blk.m_endpoints[2] = ccell_results_a.m_astc_low_endpoint.m_c[0];
				blk.m_endpoints[3] = ccell_results_a.m_astc_high_endpoint.m_c[0];
			}
			else
			{
				blk.m_endpoints[0] = (rot_comp == 0 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[0];
				blk.m_endpoints[1] = (rot_comp == 0 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[0];
				blk.m_endpoints[2] = (rot_comp == 1 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[1];
				blk.m_endpoints[3] = (rot_comp == 1 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[1];
				blk.m_endpoints[4] = (rot_comp == 2 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[2];
				blk.m_endpoints[5] = (rot_comp == 2 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[2];
				if (rot_comp == 3)
				{
					blk.m_endpoints[6] = ccell_results_a.m_astc_low_endpoint.m_c[0];
					blk.m_endpoints[7] = ccell_results_a.m_astc_high_endpoint.m_c[0];
				}
				else
				{
					blk.m_endpoints[6] = ccell_results_rgb.m_astc_low_endpoint.m_c[rot_comp];
					blk.m_endpoints[7] = ccell_results_rgb.m_astc_high_endpoint.m_c[rot_comp];
				}

				int s0 = g_astc_unquant[endpoint_range][blk.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[4]].m_unquant;
				int s1 = g_astc_unquant[endpoint_range][blk.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[5]].m_unquant;
				if (s1 < s0)
				{
					std::swap(blk.m_endpoints[0], blk.m_endpoints[1]);
					std::swap(blk.m_endpoints[2], blk.m_endpoints[3]);
					std::swap(blk.m_endpoints[4], blk.m_endpoints[5]);
					std::swap(blk.m_endpoints[6], blk.m_endpoints[7]);
					invert = true;
				}
			}

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					uint32_t rgb_index = ccell_result_selectors_rgb[x + y * 4];
					uint32_t a_index = ccell_result_selectors_a[x + y * 4];

					if (invert)
					{
						rgb_index = 3 - rgb_index;
						a_index = 3 - a_index;
					}

					blk.m_weights[(x + y * 4) * 2 + 0] = (uint8_t)rgb_index;
					blk.m_weights[(x + y * 4) * 2 + 1] = (uint8_t)a_index;
				}
			}

			assert(total_results < MAX_ENCODE_RESULTS);
			if (total_results < MAX_ENCODE_RESULTS)
			{
				pResults[total_results].m_uastc_mode = mode;
				pResults[total_results].m_common_pattern = 0;
				pResults[total_results].m_astc = blk;
				pResults[total_results].m_astc_err = total_err;
				total_results++;
			}
		} // rot_comp
	}

	// MODE 12
	// DualPlane: 0, WeightRange: 5 (8), Subsets: 1, CEM: 12 (RGBA Direct      ), EndpointRange: 19 (192)       MODE6
	static void astc_mode12(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		const uint32_t weight_range = 5;
		const uint32_t endpoint_range = 19;

		color_cell_compressor_params ccell_params;
		memset(&ccell_params, 0, sizeof(ccell_params));

		ccell_params.m_num_pixels = 16;
		ccell_params.m_pPixels = (color_quad_u8*)&block[0][0];
		ccell_params.m_num_selector_weights = 8;
		ccell_params.m_pSelector_weights = g_bc7_weights3;
		ccell_params.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights3x;
		ccell_params.m_astc_endpoint_range = endpoint_range;
		ccell_params.m_weights[0] = 1;
		ccell_params.m_weights[1] = 1;
		ccell_params.m_weights[2] = 1;
		ccell_params.m_weights[3] = 1;
		ccell_params.m_has_alpha = true;

		color_cell_compressor_results ccell_results;
		uint8_t ccell_result_selectors[16];
		uint8_t ccell_result_selectors_temp[16];
		memset(&ccell_results, 0, sizeof(ccell_results));
		ccell_results.m_pSelectors = &ccell_result_selectors[0];
		ccell_results.m_pSelectors_temp = &ccell_result_selectors_temp[0];

		uint64_t part_err = color_cell_compression(255, &ccell_params, &ccell_results, &comp_params);

		// ASTC
		astc_block_desc astc_results;
		memset(&astc_results, 0, sizeof(astc_results));

		astc_results.m_dual_plane = false;
		astc_results.m_weight_range = weight_range;

		astc_results.m_ccs = 0;
		astc_results.m_subsets = 1;
		astc_results.m_partition_seed = 0;
		astc_results.m_cem = 12;

		astc_results.m_endpoints[0] = ccell_results.m_astc_low_endpoint.m_c[0];
		astc_results.m_endpoints[1] = ccell_results.m_astc_high_endpoint.m_c[0];
		astc_results.m_endpoints[2] = ccell_results.m_astc_low_endpoint.m_c[1];
		astc_results.m_endpoints[3] = ccell_results.m_astc_high_endpoint.m_c[1];
		astc_results.m_endpoints[4] = ccell_results.m_astc_low_endpoint.m_c[2];
		astc_results.m_endpoints[5] = ccell_results.m_astc_high_endpoint.m_c[2];
		astc_results.m_endpoints[6] = ccell_results.m_astc_low_endpoint.m_c[3];
		astc_results.m_endpoints[7] = ccell_results.m_astc_high_endpoint.m_c[3];

		bool invert = false;

		int s0 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[4]].m_unquant;
		int s1 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[5]].m_unquant;
		if (s1 < s0)
		{
			std::swap(astc_results.m_endpoints[0], astc_results.m_endpoints[1]);
			std::swap(astc_results.m_endpoints[2], astc_results.m_endpoints[3]);
			std::swap(astc_results.m_endpoints[4], astc_results.m_endpoints[5]);
			std::swap(astc_results.m_endpoints[6], astc_results.m_endpoints[7]);
			invert = true;
		}

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				astc_results.m_weights[x + y * 4] = ccell_result_selectors[x + y * 4];

				if (invert)
					astc_results.m_weights[x + y * 4] = 7 - astc_results.m_weights[x + y * 4];
			}
		}

		assert(total_results < MAX_ENCODE_RESULTS);
		if (total_results < MAX_ENCODE_RESULTS)
		{
			pResults[total_results].m_uastc_mode = 12;
			pResults[total_results].m_common_pattern = 0;
			pResults[total_results].m_astc = astc_results;
			pResults[total_results].m_astc_err = part_err;
			total_results++;
		}
	}

	// 13. DualPlane: 1, WeightRange: 0 (2), Subsets: 1, CEM: 12 (RGBA Direct      ), EndpointRange: 20 (256)        MODE5
	static void astc_mode13(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		bc7enc_compress_block_params local_comp_params(comp_params);
		local_comp_params.m_perceptual = false;
		local_comp_params.m_weights[0] = 1;
		local_comp_params.m_weights[1] = 1;
		local_comp_params.m_weights[2] = 1;
		local_comp_params.m_weights[3] = 1;

		for (uint32_t rot_comp = 0; rot_comp < 4; rot_comp++)
		{
			const uint32_t weight_range = 0;
			const uint32_t endpoint_range = 20;

			color_quad_u8 block_rgb[16];
			color_quad_u8 block_a[16];
			for (uint32_t i = 0; i < 16; i++)
			{
				block_rgb[i] = ((color_quad_u8*)&block[0][0])[i];
				block_a[i] = block_rgb[i];

				uint8_t c = block_a[i].m_c[rot_comp];
				block_a[i].m_c[0] = c;
				block_a[i].m_c[1] = c;
				block_a[i].m_c[2] = c;
				block_a[i].m_c[3] = 255;

				block_rgb[i].m_c[rot_comp] = block_rgb[i].m_c[3];
				block_rgb[i].m_c[3] = 255;
			}

			uint8_t ccell_result_selectors_temp[16];

			color_cell_compressor_params ccell_params_rgb;
			memset(&ccell_params_rgb, 0, sizeof(ccell_params_rgb));

			ccell_params_rgb.m_num_pixels = 16;
			ccell_params_rgb.m_pPixels = block_rgb;
			ccell_params_rgb.m_num_selector_weights = 2;
			ccell_params_rgb.m_pSelector_weights = g_bc7_weights1;
			ccell_params_rgb.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights1x;
			ccell_params_rgb.m_astc_endpoint_range = endpoint_range;
			ccell_params_rgb.m_weights[0] = 1;
			ccell_params_rgb.m_weights[1] = 1;
			ccell_params_rgb.m_weights[2] = 1;
			ccell_params_rgb.m_weights[3] = 1;

			color_cell_compressor_results ccell_results_rgb;
			uint8_t ccell_result_selectors_rgb[16];
			memset(&ccell_results_rgb, 0, sizeof(ccell_results_rgb));
			ccell_results_rgb.m_pSelectors = &ccell_result_selectors_rgb[0];
			ccell_results_rgb.m_pSelectors_temp = &ccell_result_selectors_temp[0];

			uint64_t part_err_rgb = color_cell_compression(255, &ccell_params_rgb, &ccell_results_rgb, &local_comp_params);

			color_cell_compressor_params ccell_params_a;
			memset(&ccell_params_a, 0, sizeof(ccell_params_a));

			ccell_params_a.m_num_pixels = 16;
			ccell_params_a.m_pPixels = block_a;
			ccell_params_a.m_num_selector_weights = 2;
			ccell_params_a.m_pSelector_weights = g_bc7_weights1;
			ccell_params_a.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights1x;
			ccell_params_a.m_astc_endpoint_range = endpoint_range;
			ccell_params_a.m_weights[0] = 1;
			ccell_params_a.m_weights[1] = 1;
			ccell_params_a.m_weights[2] = 1;
			ccell_params_a.m_weights[3] = 1;

			color_cell_compressor_results ccell_results_a;
			uint8_t ccell_result_selectors_a[16];
			memset(&ccell_results_a, 0, sizeof(ccell_results_a));
			ccell_results_a.m_pSelectors = &ccell_result_selectors_a[0];
			ccell_results_a.m_pSelectors_temp = &ccell_result_selectors_temp[0];

			uint64_t part_err_a = color_cell_compression(255, &ccell_params_a, &ccell_results_a, &local_comp_params) / 3;

			uint64_t total_err = part_err_rgb + part_err_a;

			// ASTC
			astc_block_desc blk;
			memset(&blk, 0, sizeof(blk));

			blk.m_dual_plane = true;
			blk.m_weight_range = weight_range;

			blk.m_ccs = rot_comp;
			blk.m_subsets = 1;
			blk.m_partition_seed = 0;
			blk.m_cem = 12;

			blk.m_endpoints[0] = (rot_comp == 0 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[0];
			blk.m_endpoints[1] = (rot_comp == 0 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[0];
			blk.m_endpoints[2] = (rot_comp == 1 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[1];
			blk.m_endpoints[3] = (rot_comp == 1 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[1];
			blk.m_endpoints[4] = (rot_comp == 2 ? ccell_results_a : ccell_results_rgb).m_astc_low_endpoint.m_c[2];
			blk.m_endpoints[5] = (rot_comp == 2 ? ccell_results_a : ccell_results_rgb).m_astc_high_endpoint.m_c[2];
			if (rot_comp == 3)
			{
				blk.m_endpoints[6] = ccell_results_a.m_astc_low_endpoint.m_c[0];
				blk.m_endpoints[7] = ccell_results_a.m_astc_high_endpoint.m_c[0];
			}
			else
			{
				blk.m_endpoints[6] = ccell_results_rgb.m_astc_low_endpoint.m_c[rot_comp];
				blk.m_endpoints[7] = ccell_results_rgb.m_astc_high_endpoint.m_c[rot_comp];
			}

			bool invert = false;

			int s0 = g_astc_unquant[endpoint_range][blk.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[4]].m_unquant;
			int s1 = g_astc_unquant[endpoint_range][blk.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][blk.m_endpoints[5]].m_unquant;
			if (s1 < s0)
			{
				std::swap(blk.m_endpoints[0], blk.m_endpoints[1]);
				std::swap(blk.m_endpoints[2], blk.m_endpoints[3]);
				std::swap(blk.m_endpoints[4], blk.m_endpoints[5]);
				std::swap(blk.m_endpoints[6], blk.m_endpoints[7]);
				invert = true;
			}

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					uint32_t rgb_index = ccell_result_selectors_rgb[x + y * 4];
					uint32_t a_index = ccell_result_selectors_a[x + y * 4];

					if (invert)
					{
						rgb_index = 1 - rgb_index;
						a_index = 1 - a_index;
					}

					blk.m_weights[(x + y * 4) * 2 + 0] = (uint8_t)rgb_index;
					blk.m_weights[(x + y * 4) * 2 + 1] = (uint8_t)a_index;
				}
			}

			assert(total_results < MAX_ENCODE_RESULTS);
			if (total_results < MAX_ENCODE_RESULTS)
			{
				pResults[total_results].m_uastc_mode = 13;
				pResults[total_results].m_common_pattern = 0;
				pResults[total_results].m_astc = blk;
				pResults[total_results].m_astc_err = total_err;
				total_results++;
			}
		} // rot_comp
	}

	// MODE14
	// DualPlane: 0, WeightRange: 2 (4), Subsets: 1, CEM: 12 (RGBA Direct      ), EndpointRange: 20 (256)		MODE6
	static void astc_mode14(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		const uint32_t weight_range = 2;
		const uint32_t endpoint_range = 20;

		color_cell_compressor_params ccell_params;
		memset(&ccell_params, 0, sizeof(ccell_params));

		ccell_params.m_num_pixels = 16;
		ccell_params.m_pPixels = (color_quad_u8*)&block[0][0];
		ccell_params.m_num_selector_weights = 4;
		ccell_params.m_pSelector_weights = g_bc7_weights2;
		ccell_params.m_pSelector_weightsx = (const bc7enc_vec4F*)g_bc7_weights2x;
		ccell_params.m_astc_endpoint_range = endpoint_range;
		ccell_params.m_weights[0] = 1;
		ccell_params.m_weights[1] = 1;
		ccell_params.m_weights[2] = 1;
		ccell_params.m_weights[3] = 1;
		ccell_params.m_has_alpha = true;

		color_cell_compressor_results ccell_results;
		uint8_t ccell_result_selectors[16];
		uint8_t ccell_result_selectors_temp[16];
		memset(&ccell_results, 0, sizeof(ccell_results));
		ccell_results.m_pSelectors = &ccell_result_selectors[0];
		ccell_results.m_pSelectors_temp = &ccell_result_selectors_temp[0];

		uint64_t part_err = color_cell_compression(255, &ccell_params, &ccell_results, &comp_params);

		// ASTC
		astc_block_desc astc_results;
		memset(&astc_results, 0, sizeof(astc_results));

		astc_results.m_dual_plane = false;
		astc_results.m_weight_range = weight_range;

		astc_results.m_ccs = 0;
		astc_results.m_subsets = 1;
		astc_results.m_partition_seed = 0;
		astc_results.m_cem = 12;

		astc_results.m_endpoints[0] = ccell_results.m_astc_low_endpoint.m_c[0];
		astc_results.m_endpoints[1] = ccell_results.m_astc_high_endpoint.m_c[0];
		astc_results.m_endpoints[2] = ccell_results.m_astc_low_endpoint.m_c[1];
		astc_results.m_endpoints[3] = ccell_results.m_astc_high_endpoint.m_c[1];
		astc_results.m_endpoints[4] = ccell_results.m_astc_low_endpoint.m_c[2];
		astc_results.m_endpoints[5] = ccell_results.m_astc_high_endpoint.m_c[2];
		astc_results.m_endpoints[6] = ccell_results.m_astc_low_endpoint.m_c[3];
		astc_results.m_endpoints[7] = ccell_results.m_astc_high_endpoint.m_c[3];

		bool invert = false;

		int s0 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[0]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[2]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[4]].m_unquant;
		int s1 = g_astc_unquant[endpoint_range][astc_results.m_endpoints[1]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[3]].m_unquant + g_astc_unquant[endpoint_range][astc_results.m_endpoints[5]].m_unquant;
		if (s1 < s0)
		{
			std::swap(astc_results.m_endpoints[0], astc_results.m_endpoints[1]);
			std::swap(astc_results.m_endpoints[2], astc_results.m_endpoints[3]);
			std::swap(astc_results.m_endpoints[4], astc_results.m_endpoints[5]);
			std::swap(astc_results.m_endpoints[6], astc_results.m_endpoints[7]);
			invert = true;
		}

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				astc_results.m_weights[x + y * 4] = ccell_result_selectors[x + y * 4];

				if (invert)
					astc_results.m_weights[x + y * 4] = 3 - astc_results.m_weights[x + y * 4];
			}
		}

		assert(total_results < MAX_ENCODE_RESULTS);
		if (total_results < MAX_ENCODE_RESULTS)
		{
			pResults[total_results].m_uastc_mode = 14;
			pResults[total_results].m_common_pattern = 0;
			pResults[total_results].m_astc = astc_results;
			pResults[total_results].m_astc_err = part_err;
			total_results++;
		}
	}

	// MODE 15
	// DualPlane: 0, WeightRange : 8 (16), Subsets : 1, CEM : 4 (LA Direct), EndpointRange : 20 (256)   BC7 MODE6
	static void astc_mode15(const color_rgba block[4][4], uastc_encode_results* pResults, uint32_t& total_results, bc7enc_compress_block_params& comp_params)
	{
		const uint32_t weight_range = 8;
		const uint32_t endpoint_range = 20;

		color_cell_compressor_params ccell_params;
		memset(&ccell_params, 0, sizeof(ccell_params));

		color_rgba temp_block[16];
		for (uint32_t i = 0; i < 16; i++)
		{
			const uint32_t l = ((const color_rgba*)block)[i].r;
			const uint32_t a = ((const color_rgba*)block)[i].a;

			// Use (l,0,0,a) not (l,l,l,a) so both components are treated equally.
			temp_block[i].set_noclamp_rgba(l, 0, 0, a);
		}

		ccell_params.m_num_pixels = 16;
		//ccell_params.m_pPixels = (color_quad_u8*)&block[0][0];
		ccell_params.m_pPixels = (color_quad_u8*)temp_block;
		ccell_params.m_num_selector_weights = 16;
		ccell_params.m_pSelector_weights = g_astc_weights4;
		ccell_params.m_pSelector_weightsx = (const bc7enc_vec4F*)g_astc_weights4x;
		ccell_params.m_astc_endpoint_range = endpoint_range;
		ccell_params.m_weights[0] = 1;
		ccell_params.m_weights[1] = 1;
		ccell_params.m_weights[2] = 1;
		ccell_params.m_weights[3] = 1;
		ccell_params.m_has_alpha = true;

		color_cell_compressor_results ccell_results;
		uint8_t ccell_result_selectors[16];
		uint8_t ccell_result_selectors_temp[16];
		memset(&ccell_results, 0, sizeof(ccell_results));
		ccell_results.m_pSelectors = &ccell_result_selectors[0];
		ccell_results.m_pSelectors_temp = &ccell_result_selectors_temp[0];

		color_cell_compression(255, &ccell_params, &ccell_results, &comp_params);

		// ASTC
		astc_block_desc astc_results;
		memset(&astc_results, 0, sizeof(astc_results));

		astc_results.m_dual_plane = false;
		astc_results.m_weight_range = weight_range;

		astc_results.m_ccs = 0;
		astc_results.m_subsets = 1;
		astc_results.m_partition_seed = 0;
		astc_results.m_cem = 4;

		astc_results.m_endpoints[0] = ccell_results.m_astc_low_endpoint.m_c[0];
		astc_results.m_endpoints[1] = ccell_results.m_astc_high_endpoint.m_c[0];

		astc_results.m_endpoints[2] = ccell_results.m_astc_low_endpoint.m_c[3];
		astc_results.m_endpoints[3] = ccell_results.m_astc_high_endpoint.m_c[3];

		for (uint32_t y = 0; y < 4; y++)
			for (uint32_t x = 0; x < 4; x++)
				astc_results.m_weights[x + y * 4] = ccell_result_selectors[x + y * 4];

		color_rgba colors[16];
		for (uint32_t c = 0; c < 4; c++)
		{
			colors[0].m_comps[c] = g_astc_unquant[endpoint_range][ccell_results.m_astc_low_endpoint.m_c[(c < 3) ? 0 : 3]].m_unquant;
			colors[15].m_comps[c] = g_astc_unquant[endpoint_range][ccell_results.m_astc_high_endpoint.m_c[(c < 3) ? 0 : 3]].m_unquant;
		}

		for (uint32_t i = 1; i < 16 - 1; i++)
			for (uint32_t c = 0; c < 4; c++)
				colors[i].m_comps[c] = (uint8_t)astc_interpolate(colors[0].m_comps[c], colors[15].m_comps[c], g_astc_weights4[i], false);

		uint64_t total_err = 0;
		for (uint32_t p = 0; p < 16; p++)
			total_err += color_distance_la(((const color_rgba*)block)[p], colors[ccell_result_selectors[p]]);

		assert(total_results < MAX_ENCODE_RESULTS);
		if (total_results < MAX_ENCODE_RESULTS)
		{
			pResults[total_results].m_uastc_mode = 15;
			pResults[total_results].m_common_pattern = 0;
			pResults[total_results].m_astc = astc_results;
			pResults[total_results].m_astc_err = total_err;
			total_results++;
		}
	}
		
	static void compute_block_error(const color_rgba block[4][4], const color_rgba decoded_block[4][4], uint64_t &total_rgb_err, uint64_t &total_rgba_err, uint64_t &total_la_err)
	{
		uint64_t total_err_r = 0, total_err_g = 0, total_err_b = 0, total_err_a = 0;

		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				const int dr = (int)block[y][x].m_comps[0] - (int)decoded_block[y][x].m_comps[0];
				const int dg = (int)block[y][x].m_comps[1] - (int)decoded_block[y][x].m_comps[1];
				const int db = (int)block[y][x].m_comps[2] - (int)decoded_block[y][x].m_comps[2];
				const int da = (int)block[y][x].m_comps[3] - (int)decoded_block[y][x].m_comps[3];

				total_err_r += dr * dr;
				total_err_g += dg * dg;
				total_err_b += db * db;
				total_err_a += da * da;
			}
		}

		total_la_err = total_err_r + total_err_a;
		total_rgb_err = total_err_r + total_err_g + total_err_b;
		total_rgba_err = total_rgb_err + total_err_a;
	}

	static void compute_bc1_hints(bool &bc1_hint0, bool &bc1_hint1, const uastc_encode_results &best_results, const color_rgba block[4][4], const color_rgba decoded_uastc_block[4][4])
	{
		const uint32_t best_mode = best_results.m_uastc_mode;
		const bool perceptual = false;

		bc1_hint0 = false;
		bc1_hint1 = false;

		if (best_mode == UASTC_MODE_INDEX_SOLID_COLOR)
			return;

		if (!g_uastc_mode_has_bc1_hint0[best_mode] && !g_uastc_mode_has_bc1_hint1[best_mode])
			return;

		color_rgba tblock_bc1[4][4];
		dxt1_block tbc1_block[8];
		basist::encode_bc1(tbc1_block, (const uint8_t*)&decoded_uastc_block[0][0], 0);
		unpack_block(texture_format::cBC1, tbc1_block, &tblock_bc1[0][0]);

		color_rgba tblock_hint0_bc1[4][4];
		color_rgba tblock_hint1_bc1[4][4];
		
		etc_block etc1_blk;
		memset(&etc1_blk, 0, sizeof(etc1_blk));

		eac_a8_block etc2_blk;
		memset(&etc2_blk, 0, sizeof(etc2_blk));
		etc2_blk.m_multiplier = 1;
		
		// Pack to UASTC, then unpack, because the endpoints may be swapped.

		uastc_block temp_ublock;
		pack_uastc(temp_ublock, best_results, etc1_blk, 0, etc2_blk, false, false);

		unpacked_uastc_block temp_ublock_unpacked;
		unpack_uastc(temp_ublock, temp_ublock_unpacked, false);
										
		unpacked_uastc_block ublock;
		memset(&ublock, 0, sizeof(ublock));
		ublock.m_mode = best_results.m_uastc_mode;
		ublock.m_common_pattern = best_results.m_common_pattern;
		ublock.m_astc = temp_ublock_unpacked.m_astc;

		dxt1_block b;

		// HINT1
		if (!g_uastc_mode_has_bc1_hint1[best_mode])
		{
			memset(tblock_hint1_bc1, 0, sizeof(tblock_hint1_bc1));
		}
		else
		{
			transcode_uastc_to_bc1_hint1(ublock, (color32 (*)[4]) decoded_uastc_block, &b, false);

			unpack_block(texture_format::cBC1, &b, &tblock_hint1_bc1[0][0]);
		}

		// HINT0
		if (!g_uastc_mode_has_bc1_hint0[best_mode])
		{
			memset(tblock_hint0_bc1, 0, sizeof(tblock_hint0_bc1));
		}
		else
		{
			transcode_uastc_to_bc1_hint0(ublock, &b);
			
			unpack_block(texture_format::cBC1, &b, &tblock_hint0_bc1[0][0]);
		}

		// Compute block errors
		uint64_t total_t_err = 0, total_hint0_err = 0, total_hint1_err = 0;
		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				total_t_err += color_distance(perceptual, block[y][x], tblock_bc1[y][x], false);
				total_hint0_err += color_distance(perceptual, block[y][x], tblock_hint0_bc1[y][x], false);
				total_hint1_err += color_distance(perceptual, block[y][x], tblock_hint1_bc1[y][x], false);
			}
		}

		const float t_err = sqrtf((float)total_t_err);
		const float t_err_hint0 = sqrtf((float)total_hint0_err);
		const float t_err_hint1 = sqrtf((float)total_hint1_err);

		const float err_thresh0 = 1.075f;
		const float err_thresh1 = 1.075f;
		
		if ((g_uastc_mode_has_bc1_hint0[best_mode]) && (t_err_hint0 <= t_err * err_thresh0))
			bc1_hint0 = true;

		if ((g_uastc_mode_has_bc1_hint1[best_mode]) && (t_err_hint1 <= t_err * err_thresh1))
			bc1_hint1 = true;
	}

	struct ycbcr
	{
		int32_t m_y;
		int32_t m_cb;
		int32_t m_cr;
	};

	static inline void rgb_to_y_cb_cr(const color_rgba& c, ycbcr& dst)
	{
		const int y = c.r * 54 + c.g * 183 + c.b * 19;
		dst.m_y = y;
		dst.m_cb = (c.b << 8) - y;
		dst.m_cr = (c.r << 8) - y;
	}

	static inline uint64_t color_diff(const ycbcr& a, const ycbcr& b)
	{
		const int y_delta = a.m_y - b.m_y;
		const int cb_delta = a.m_cb - b.m_cb;
		const int cr_delta = a.m_cr - b.m_cr;
		return ((int64_t)y_delta * y_delta * 4) + ((int64_t)cr_delta * cr_delta) + ((int64_t)cb_delta * cb_delta);
	}

	static inline int gray_distance2(const color_rgba& c, int r, int g, int b)
	{
		int gray_dist = (((int)c[0] - r) + ((int)c[1] - g) + ((int)c[2] - b) + 1) / 3;

		int gray_point_r = clamp255(r + gray_dist);
		int gray_point_g = clamp255(g + gray_dist);
		int gray_point_b = clamp255(b + gray_dist);

		int dist_to_gray_point_r = c[0] - gray_point_r;
		int dist_to_gray_point_g = c[1] - gray_point_g;
		int dist_to_gray_point_b = c[2] - gray_point_b;

		return (dist_to_gray_point_r * dist_to_gray_point_r) + (dist_to_gray_point_g * dist_to_gray_point_g) + (dist_to_gray_point_b * dist_to_gray_point_b);
	}

	static bool pack_etc1_estimate_flipped(const color_rgba* pSrc_pixels)
	{
		int sums[3][2][2];

#define GET_XY(x, y, c) pSrc_pixels[(x) + ((y) * 4)][c]

		for (uint32_t c = 0; c < 3; c++)
		{
			sums[c][0][0] = GET_XY(0, 0, c) + GET_XY(0, 1, c) + GET_XY(1, 0, c) + GET_XY(1, 1, c);
			sums[c][1][0] = GET_XY(2, 0, c) + GET_XY(2, 1, c) + GET_XY(3, 0, c) + GET_XY(3, 1, c);
			sums[c][0][1] = GET_XY(0, 2, c) + GET_XY(0, 3, c) + GET_XY(1, 2, c) + GET_XY(1, 3, c);
			sums[c][1][1] = GET_XY(2, 2, c) + GET_XY(2, 3, c) + GET_XY(3, 2, c) + GET_XY(3, 3, c);
		}

		int upper_avg[3], lower_avg[3], left_avg[3], right_avg[3];
		for (uint32_t c = 0; c < 3; c++)
		{
			upper_avg[c] = (sums[c][0][0] + sums[c][1][0] + 4) / 8;
			lower_avg[c] = (sums[c][0][1] + sums[c][1][1] + 4) / 8;
			left_avg[c] = (sums[c][0][0] + sums[c][0][1] + 4) / 8;
			right_avg[c] = (sums[c][1][0] + sums[c][1][1] + 4) / 8;
		}

#undef GET_XY
#define GET_XY(x, y, a) gray_distance2(pSrc_pixels[(x) + ((y) * 4)], a[0], a[1], a[2])

		int upper_gray_dist = 0, lower_gray_dist = 0, left_gray_dist = 0, right_gray_dist = 0;
		for (uint32_t i = 0; i < 4; i++)
		{
			for (uint32_t j = 0; j < 2; j++)
			{
				upper_gray_dist += GET_XY(i, j, upper_avg);
				lower_gray_dist += GET_XY(i, 2 + j, lower_avg);
				left_gray_dist += GET_XY(j, i, left_avg);
				right_gray_dist += GET_XY(2 + j, i, right_avg);
			}
		}

#undef GET_XY

		int upper_lower_sum = upper_gray_dist + lower_gray_dist;
		int left_right_sum = left_gray_dist + right_gray_dist;

		return upper_lower_sum < left_right_sum;
	}

	static void compute_etc1_hints(etc_block& best_etc1_blk, uint32_t& best_etc1_bias, const uastc_encode_results& best_results, const color_rgba block[4][4], const color_rgba decoded_uastc_block[4][4], int level, uint32_t flags)
	{
		best_etc1_bias = 0;

		if (best_results.m_uastc_mode == UASTC_MODE_INDEX_SOLID_COLOR)
		{
			pack_etc1_block_solid_color(best_etc1_blk, &best_results.m_solid_color.m_comps[0]);
			return;
		}

		const bool faster_etc1 = (flags & cPackUASTCETC1FasterHints) != 0;
		const bool fastest_etc1 = (flags & cPackUASTCETC1FastestHints) != 0;

		const bool has_bias = g_uastc_mode_has_etc1_bias[best_results.m_uastc_mode];

		// 0 should be at the top, but we need 13 first because it represents bias (0,0,0).
		const uint8_t s_sorted_bias_modes[32] = { 13, 0, 22, 29, 27, 12, 26, 9, 30, 31, 8, 10, 25, 2, 23, 5, 15, 7, 3, 11, 6, 17, 28, 18, 1, 19, 20, 21, 24, 4, 14, 16 };

		uint32_t last_bias = 1;
		bool use_faster_bias_mode_table = false;
		const bool flip_estimate = (level <= cPackUASTCLevelFaster) || (faster_etc1) || (fastest_etc1);
		if (has_bias)
		{
			switch (level)
			{
			case cPackUASTCLevelFastest:
			{
				last_bias = fastest_etc1 ? 1 : (faster_etc1 ? 1 : 2);
				use_faster_bias_mode_table = true;
				break;
			}
			case cPackUASTCLevelFaster:
			{
				last_bias = fastest_etc1 ? 1 : (faster_etc1 ? 3 : 5);
				use_faster_bias_mode_table = true;
				break;
			}
			case cPackUASTCLevelDefault:
			{
				last_bias = fastest_etc1 ? 1 : (faster_etc1 ? 10 : 20);
				use_faster_bias_mode_table = true;
				break;
			}
			case cPackUASTCLevelSlower:
			{
				last_bias = fastest_etc1 ? 1 : (faster_etc1 ? 16 : 32);
				use_faster_bias_mode_table = true;
				break;
			}
			default:
			{
				last_bias = 32;
				break;
			}
			}
		}

		memset(&best_etc1_blk, 0, sizeof(best_etc1_blk));
		uint64_t best_err = UINT64_MAX;

		etc_block trial_block;
		memset(&trial_block, 0, sizeof(trial_block));

		ycbcr block_ycbcr[4][4], decoded_uastc_block_ycbcr[4][4];
		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				rgb_to_y_cb_cr(block[y][x], block_ycbcr[y][x]);
				rgb_to_y_cb_cr(decoded_uastc_block[y][x], decoded_uastc_block_ycbcr[y][x]);
			}
		}

		uint32_t first_flip = 0, last_flip = 2;
		uint32_t first_individ = 0, last_individ = 2;
		
		if (flags & cPackUASTCETC1DisableFlipAndIndividual)
		{
			last_flip = 1;
			last_individ = 1;
		}
		else if (flip_estimate)
		{
			if (pack_etc1_estimate_flipped(&decoded_uastc_block[0][0]))
				first_flip = 1;
			last_flip = first_flip + 1;
		}
										
		for (uint32_t flip = first_flip; flip < last_flip; flip++)
		{
			trial_block.set_flip_bit(flip != 0);

			for (uint32_t individ = first_individ; individ < last_individ; individ++)
			{
				const uint32_t mul = individ ? 15 : 31;
				
				trial_block.set_diff_bit(individ == 0);

				color_rgba unbiased_block_colors[2];

				int min_r[2] = { 255, 255 }, min_g[2] = { 255, 255 }, min_b[2] = { 255, 255 }, max_r[2] = { 0, 0 }, max_g[2] = { 0, 0 }, max_b[2] = { 0, 0 };

				for (uint32_t subset = 0; subset < 2; subset++)
				{
					uint32_t avg_color[3];
					memset(avg_color, 0, sizeof(avg_color));

					for (uint32_t j = 0; j < 8; j++)
					{
						const etc_coord2 &c = g_etc1_pixel_coords[flip][subset][j];
						const color_rgba& p = decoded_uastc_block[c.m_y][c.m_x];
												
						avg_color[0] += p.r;
						avg_color[1] += p.g;
						avg_color[2] += p.b;

						min_r[subset] = basisu::minimum<uint32_t>(min_r[subset], p.r);
						min_g[subset] = basisu::minimum<uint32_t>(min_g[subset], p.g);
						min_b[subset] = basisu::minimum<uint32_t>(min_b[subset], p.b);

						max_r[subset] = basisu::maximum<uint32_t>(max_r[subset], p.r);
						max_g[subset] = basisu::maximum<uint32_t>(max_g[subset], p.g);
						max_b[subset] = basisu::maximum<uint32_t>(max_b[subset], p.b);
					} // j

					unbiased_block_colors[subset][0] = (uint8_t)((avg_color[0] * mul + 1020) / (8 * 255));
					unbiased_block_colors[subset][1] = (uint8_t)((avg_color[1] * mul + 1020) / (8 * 255));
					unbiased_block_colors[subset][2] = (uint8_t)((avg_color[2] * mul + 1020) / (8 * 255));
					unbiased_block_colors[subset][3] = 0;
										
				} // subset
												
				for (uint32_t bias_iter = 0; bias_iter < last_bias; bias_iter++)
				{
					const uint32_t bias = use_faster_bias_mode_table ? s_sorted_bias_modes[bias_iter] : bias_iter;
										
					color_rgba block_colors[2];
					for (uint32_t subset = 0; subset < 2; subset++)
						block_colors[subset] = has_bias ? apply_etc1_bias((color32&)unbiased_block_colors[subset], bias, mul, subset) : unbiased_block_colors[subset];

					if (individ)
						trial_block.set_block_color4(block_colors[0], block_colors[1]);
					else
						trial_block.set_block_color5_clamp(block_colors[0], block_colors[1]);

					uint32_t range[2];
					for (uint32_t subset = 0; subset < 2; subset++)
					{
						const color_rgba base_c(trial_block.get_block_color(subset, true));

						const int pos_r = iabs(max_r[subset] - base_c.r);
						const int neg_r = iabs(base_c.r - min_r[subset]);

						const int pos_g = iabs(max_g[subset] - base_c.g);
						const int neg_g = iabs(base_c.g - min_g[subset]);

						const int pos_b = iabs(max_b[subset] - base_c.b);
						const int neg_b = iabs(base_c.b - min_b[subset]);

						range[subset] = maximum(maximum(pos_r, neg_r, pos_g, neg_g), pos_b, neg_b);
					}

					uint32_t best_inten_table[2] = { 0, 0 };

					for (uint32_t subset = 0; subset < 2; subset++)
					{
						uint64_t best_subset_err = UINT64_MAX;

						const uint32_t inten_table_limit = (level == cPackUASTCLevelVerySlow) ? 8 : ((range[subset] > 51) ? 8 : (range[subset] >= 7 ? 4 : 2));
						
						for (uint32_t inten_table = 0; inten_table < inten_table_limit; inten_table++)
						{
							trial_block.set_inten_table(subset, inten_table);

							color_rgba color_table[4];
							trial_block.get_block_colors(color_table, subset);

							ycbcr color_table_ycbcr[4];
							for (uint32_t i = 0; i < 4; i++)
								rgb_to_y_cb_cr(color_table[i], color_table_ycbcr[i]);

							uint64_t total_error = 0;
							if (flip)
							{
								for (uint32_t y = 0; y < 2; y++)
								{
									{
										const ycbcr& c = decoded_uastc_block_ycbcr[subset * 2 + y][0];
										total_error += minimum(color_diff(color_table_ycbcr[0], c), color_diff(color_table_ycbcr[1], c), color_diff(color_table_ycbcr[2], c), color_diff(color_table_ycbcr[3], c));
									}
									{
										const ycbcr& c = decoded_uastc_block_ycbcr[subset * 2 + y][1];
										total_error += minimum(color_diff(color_table_ycbcr[0], c), color_diff(color_table_ycbcr[1], c), color_diff(color_table_ycbcr[2], c), color_diff(color_table_ycbcr[3], c));
									}
									{
										const ycbcr& c = decoded_uastc_block_ycbcr[subset * 2 + y][2];
										total_error += minimum(color_diff(color_table_ycbcr[0], c), color_diff(color_table_ycbcr[1], c), color_diff(color_table_ycbcr[2], c), color_diff(color_table_ycbcr[3], c));
									}
									{
										const ycbcr& c = decoded_uastc_block_ycbcr[subset * 2 + y][3];
										total_error += minimum(color_diff(color_table_ycbcr[0], c), color_diff(color_table_ycbcr[1], c), color_diff(color_table_ycbcr[2], c), color_diff(color_table_ycbcr[3], c));
									}
									if (total_error >= best_subset_err)
										break;
								}
							}
							else
							{
								for (uint32_t y = 0; y < 4; y++)
								{
									{
										const ycbcr& c = decoded_uastc_block_ycbcr[y][subset * 2 + 0];
										total_error += minimum(color_diff(color_table_ycbcr[0], c), color_diff(color_table_ycbcr[1], c), color_diff(color_table_ycbcr[2], c), color_diff(color_table_ycbcr[3], c));
									}
									{
										const ycbcr& c = decoded_uastc_block_ycbcr[y][subset * 2 + 1];
										total_error += minimum(color_diff(color_table_ycbcr[0], c), color_diff(color_table_ycbcr[1], c), color_diff(color_table_ycbcr[2], c), color_diff(color_table_ycbcr[3], c));
									}
								}
								if (total_error >= best_subset_err)
									break;
							}

							if (total_error < best_subset_err)
							{
								best_subset_err = total_error;
								best_inten_table[subset] = inten_table;
							}

						} // inten_table

					} // subset

					trial_block.set_inten_table(0, best_inten_table[0]);
					trial_block.set_inten_table(1, best_inten_table[1]);

					// Compute error against the ORIGINAL block.
					uint64_t err = 0;

					for (uint32_t subset = 0; subset < 2; subset++)
					{
						color_rgba color_table[4];
						trial_block.get_block_colors(color_table, subset);

						ycbcr color_table_ycbcr[4];
						for (uint32_t i = 0; i < 4; i++)
							rgb_to_y_cb_cr(color_table[i], color_table_ycbcr[i]);

						if (flip)
						{
							for (uint32_t y = 0; y < 2; y++)
							{
								for (uint32_t x = 0; x < 4; x++)
								{
									const ycbcr& c = decoded_uastc_block_ycbcr[subset * 2 + y][x];
									const uint64_t best_index_err = minimum(color_diff(color_table_ycbcr[0], c) << 2, (color_diff(color_table_ycbcr[1], c) << 2) + 1, (color_diff(color_table_ycbcr[2], c) << 2) + 2, (color_diff(color_table_ycbcr[3], c) << 2) + 3);

									const uint32_t best_index = (uint32_t)best_index_err & 3;
									err += color_diff(block_ycbcr[subset * 2 + y][x], color_table_ycbcr[best_index]);
								}
								if (err >= best_err)
									break;
							}
						}
						else
						{
							for (uint32_t y = 0; y < 4; y++)
							{
								for (uint32_t x = 0; x < 2; x++)
								{
									const ycbcr& c = decoded_uastc_block_ycbcr[y][subset * 2 + x];
									const uint64_t best_index_err = minimum(color_diff(color_table_ycbcr[0], c) << 2, (color_diff(color_table_ycbcr[1], c) << 2) + 1, (color_diff(color_table_ycbcr[2], c) << 2) + 2, (color_diff(color_table_ycbcr[3], c) << 2) + 3);

									const uint32_t best_index = (uint32_t)best_index_err & 3;
									err += color_diff(block_ycbcr[y][subset * 2 + x], color_table_ycbcr[best_index]);
								}
								if (err >= best_err)
									break;
							}
						}

					} // subset

					if (err < best_err)
					{
						best_err = err;

						best_etc1_blk = trial_block;
						best_etc1_bias = bias;
					}

				} // bias_iter

			} // individ

		} // flip
	}

	struct uastc_pack_eac_a8_results
	{
		uint32_t m_base;
		uint32_t m_table;
		uint32_t m_multiplier;
	};
	
	static uint64_t uastc_pack_eac_a8(uastc_pack_eac_a8_results& results, const uint8_t* pPixels, uint32_t num_pixels, uint32_t base_search_rad, uint32_t mul_search_rad, uint32_t table_mask)
	{
		assert(num_pixels <= 16);

		uint32_t min_alpha = 255, max_alpha = 0;
		for (uint32_t i = 0; i < num_pixels; i++)
		{
			const uint32_t a = pPixels[i];
			if (a < min_alpha) min_alpha = a;
			if (a > max_alpha) max_alpha = a;
		}

		if (min_alpha == max_alpha)
		{
			results.m_base = min_alpha;
			results.m_table = 13;
			results.m_multiplier = 1;
			return 0;
		}

		const uint32_t alpha_range = max_alpha - min_alpha;

		uint64_t best_err = UINT64_MAX;

		for (uint32_t table = 0; table < 16; table++)
		{
			if ((table_mask & (1U << table)) == 0)
				continue;

			const float range = (float)(g_etc2_eac_tables[table][ETC2_EAC_MAX_VALUE_SELECTOR] - g_etc2_eac_tables[table][ETC2_EAC_MIN_VALUE_SELECTOR]);
			const int center = (int)roundf(lerp((float)min_alpha, (float)max_alpha, (float)(0 - g_etc2_eac_tables[table][ETC2_EAC_MIN_VALUE_SELECTOR]) / range));

			const int base_min = clamp255(center - base_search_rad);
			const int base_max = clamp255(center + base_search_rad);

			const int mul = (int)roundf(alpha_range / range);
			const int mul_low = clamp<int>(mul - mul_search_rad, 1, 15);
			const int mul_high = clamp<int>(mul + mul_search_rad, 1, 15);

			for (int base = base_min; base <= base_max; base++)
			{
				for (int multiplier = mul_low; multiplier <= mul_high; multiplier++)
				{
					uint64_t total_err = 0;

					for (uint32_t i = 0; i < num_pixels; i++)
					{
						const int a = pPixels[i];

						uint32_t best_s_err = UINT32_MAX;
						//uint32_t best_s = 0;
						for (uint32_t s = 0; s < 8; s++)
						{
							const int v = clamp255((int)multiplier * g_etc2_eac_tables[table][s] + (int)base);

							uint32_t err = iabs(a - v);
							if (err < best_s_err)
							{
								best_s_err = err;
								//best_s = s;
							}
						}

						total_err += best_s_err * best_s_err;
						if (total_err >= best_err)
							break;
					}

					if (total_err < best_err)
					{
						best_err = total_err;
						results.m_base = base;
						results.m_multiplier = multiplier;
						results.m_table = table;
						if (!best_err)
							return best_err;
					}

				} // table

			} // multiplier

		} // base

		return best_err;
	}

	const int32_t DEFAULT_BC7_ERROR_WEIGHT = 50;
	const float UASTC_ERROR_THRESH = 1.3f;

	// TODO: This is a quick hack to favor certain modes when we know we'll be followed up with an RDO postprocess.
	static inline float get_uastc_mode_weight(uint32_t mode)
	{
		const float FAVORED_MODE_WEIGHT = .8f;

		switch (mode)
		{
		case 0:
		case 10:
			return FAVORED_MODE_WEIGHT;
		default:
			break;
		}

		return 1.0f;
	}

	void encode_uastc(const uint8_t* pRGBAPixels, uastc_block& output_block, uint32_t flags)
	{
//		printf("encode_uastc: \n");
//		for (int i = 0; i < 16; i++)
//			printf("[%u %u %u %u] ", pRGBAPixels[i * 4 + 0], pRGBAPixels[i * 4 + 1], pRGBAPixels[i * 4 + 2], pRGBAPixels[i * 4 + 3]);
//		printf("\n");

		const color_rgba(*block)[4] = reinterpret_cast<const color_rgba(*)[4]>(pRGBAPixels);

		bool solid_color = true, has_alpha = false, is_la = true;

		const color_rgba first_color(block[0][0]);
		for (uint32_t y = 0; y < 4; y++)
		{
			for (uint32_t x = 0; x < 4; x++)
			{
				if (block[y][x].a < 255)
					has_alpha = true;

				if (block[y][x] != first_color)
					solid_color = false;

				if ((block[y][x].r != block[y][x].g) || (block[y][x].r != block[y][x].b))
					is_la = false;
			}
		}

		if (solid_color)
		{
			// Solid color blocks are so common that we handle them specially and as quickly as we can.
			uastc_encode_results solid_results;
			solid_results.m_uastc_mode = UASTC_MODE_INDEX_SOLID_COLOR;
			solid_results.m_astc_err = 0;
			solid_results.m_common_pattern = 0;
			solid_results.m_solid_color = first_color;
			memset(&solid_results.m_astc, 0, sizeof(solid_results.m_astc));
						
			etc_block etc1_blk;
			uint32_t etc1_bias = 0;

			pack_etc1_block_solid_color(etc1_blk, &first_color.m_comps[0]);

			eac_a8_block eac_a8_blk;
			eac_a8_blk.m_table = 0;
			eac_a8_blk.m_multiplier = 1;

			pack_uastc(output_block, solid_results, etc1_blk, etc1_bias, eac_a8_blk, false, false);

//			printf(" Solid\n");

			return;
		}
		
		int level = flags & 7;
		const bool favor_uastc_error = (flags & cPackUASTCFavorUASTCError) != 0;
		const bool favor_bc7_error = !favor_uastc_error && ((flags & cPackUASTCFavorBC7Error) != 0);
		//const bool etc1_perceptual = true;
		
		// TODO: This uses 64KB of stack space!
		uastc_encode_results results[MAX_ENCODE_RESULTS];
						
		level = clampi(level, cPackUASTCLevelFastest, cPackUASTCLevelVerySlow);
		
		// Set all options to slowest, then configure from there depending on the selected level.
		uint32_t mode_mask = UINT32_MAX;
		uint32_t uber_level = 6;
		bool estimate_partition = false;
		bool always_try_alpha_modes = true;
		uint32_t eac_a8_mul_search_rad = 3;
		uint32_t eac_a8_table_mask = UINT32_MAX;
		uint32_t least_squares_passes = 2;
		bool bc1_hints = true;
		bool only_use_la_on_transparent_blocks = false;
		
		switch (level)
		{
		case cPackUASTCLevelFastest:
		{
			mode_mask = (1 << 0) | (1 << 8) | 
				(1 << 11) | (1 << 12) |
				(1 << 15);
			always_try_alpha_modes = false;
			eac_a8_mul_search_rad = 0;
			eac_a8_table_mask = (1 << 2) | (1 << 8) | (1 << 11) | (1 << 13);
			uber_level = 0;
			least_squares_passes = 1;
			bc1_hints = false;
			estimate_partition = true;
			only_use_la_on_transparent_blocks = true;
			break;
		}
		case cPackUASTCLevelFaster:
		{
			mode_mask = (1 << 0) | (1 << 4) | (1 << 6) | (1 << 8) |
				(1 << 9) | (1 << 11) | (1 << 12) |
				(1 << 15) | (1 << 17);
			always_try_alpha_modes = false;
			eac_a8_mul_search_rad = 0;
			eac_a8_table_mask = (1 << 2) | (1 << 8) | (1 << 11) | (1 << 13);
			uber_level = 0;
			least_squares_passes = 1;
			estimate_partition = true;
			break;
		}
		case cPackUASTCLevelDefault: 
		{
			mode_mask = (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 8) |
				(1 << 9) | (1 << 10) | (1 << 11) | (1 << 12) | (1 << 13) |
				(1 << 15) | (1 << 16) | (1 << 17);
			always_try_alpha_modes = false;
			eac_a8_mul_search_rad = 1;
			eac_a8_table_mask = (1 << 0) | (1 << 2) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 10) | (1 << 11) | (1 << 13);
			uber_level = 1;
			least_squares_passes = 1;
			estimate_partition = true;
			break;
		}
		case cPackUASTCLevelSlower:
		{
			always_try_alpha_modes = false;
			eac_a8_mul_search_rad = 2;
			uber_level = 3;
			estimate_partition = true;
			break;
		}
		case cPackUASTCLevelVerySlow:
		{
			break;
		}
		}

#if BASISU_SUPPORT_FORCE_MODE
		static int force_mode = -1;
		force_mode = (force_mode + 1) % TOTAL_UASTC_MODES;
		mode_mask = UINT32_MAX;
		always_try_alpha_modes = true;
		only_use_la_on_transparent_blocks = false;
#endif

		// HACK HACK
		//mode_mask &= ~(1 << 18);
		//mode_mask = (1 << 18)| (1 << 10);
																				
		uint32_t total_results = 0;
				
		if (only_use_la_on_transparent_blocks)
		{
			if ((is_la) && (!has_alpha))
				is_la = false;
		}

		const bool try_alpha_modes = has_alpha || always_try_alpha_modes;
		
		bc7enc_compress_block_params comp_params;
		memset(&comp_params, 0, sizeof(comp_params));
		comp_params.m_max_partitions_mode1 = 64;
		comp_params.m_least_squares_passes = least_squares_passes;
		comp_params.m_weights[0] = 1;
		comp_params.m_weights[1] = 1;
		comp_params.m_weights[2] = 1;
		comp_params.m_weights[3] = 1;
		comp_params.m_uber_level = uber_level;

		if (is_la)
		{
			if (mode_mask & (1U << 15))
				astc_mode15(block, results, total_results, comp_params);

			if (mode_mask & (1U << 16))
				astc_mode9_or_16(16, block, results, total_results, comp_params, estimate_partition ? 4 : 0);

			if (mode_mask & (1U << 17))
				astc_mode11_or_17(17, block, results, total_results, comp_params);
		}

		if (!has_alpha)
		{
			if (mode_mask & (1U << 0))
				astc_mode0_or_18(0, block, results, total_results, comp_params);

			if (mode_mask & (1U << 1))
				astc_mode1(block, results, total_results, comp_params);

			if (mode_mask & (1U << 2))
				astc_mode2(block, results, total_results, comp_params, estimate_partition);

			if (mode_mask & (1U << 3))
				astc_mode3(block, results, total_results, comp_params, estimate_partition);

			if (mode_mask & (1U << 4))
				astc_mode4(block, results, total_results, comp_params, estimate_partition);

			if (mode_mask & (1U << 5))
				astc_mode5(block, results, total_results, comp_params);

			if (mode_mask & (1U << 6))
				astc_mode6(block, results, total_results, comp_params);

			if (mode_mask & (1U << 7))
				astc_mode7(block, results, total_results, comp_params, estimate_partition);

			if (mode_mask & (1U << 18))
				astc_mode0_or_18(18, block, results, total_results, comp_params);
		}

		if (try_alpha_modes)
		{
			if (mode_mask & (1U << 9))
				astc_mode9_or_16(9, block, results, total_results, comp_params, estimate_partition ? 4 : 0);

			if (mode_mask & (1U << 10))
				astc_mode10(block, results, total_results, comp_params);

			if (mode_mask & (1U << 11))
				astc_mode11_or_17(11, block, results, total_results, comp_params);

			if (mode_mask & (1U << 12))
				astc_mode12(block, results, total_results, comp_params);

			if (mode_mask & (1U << 13))
				astc_mode13(block, results, total_results, comp_params);

			if (mode_mask & (1U << 14))
				astc_mode14(block, results, total_results, comp_params);
		}

		assert(total_results);
		
		// Fix up the errors so we consistently have LA, RGB, or RGBA error.
		for (uint32_t i = 0; i < total_results; i++)
		{
			uastc_encode_results& r = results[i];
			if (!is_la)
			{
				if (g_uastc_mode_is_la[r.m_uastc_mode])
				{
					color_rgba unpacked_block[16];
					unpack_uastc(r.m_uastc_mode, r.m_common_pattern, r.m_solid_color.get_color32(), r.m_astc, (basist::color32 *)unpacked_block, false);

					uint64_t total_err = 0;
					for (uint32_t j = 0; j < 16; j++)
						total_err += color_distance(unpacked_block[j], ((const color_rgba*)block)[j], true);

					r.m_astc_err = total_err;
				}
			}
			else
			{
				if (!g_uastc_mode_is_la[r.m_uastc_mode])
				{
					color_rgba unpacked_block[16];
					unpack_uastc(r.m_uastc_mode, r.m_common_pattern, r.m_solid_color.get_color32(), r.m_astc, (basist::color32 *)unpacked_block, false);

					uint64_t total_err = 0;
					for (uint32_t j = 0; j < 16; j++)
						total_err += color_distance_la(unpacked_block[j], ((const color_rgba*)block)[j]);

					r.m_astc_err = total_err;
				}
			}
		}
				
		unpacked_uastc_block unpacked_ublock;
		memset(&unpacked_ublock, 0, sizeof(unpacked_ublock));

		uint64_t total_overall_err[MAX_ENCODE_RESULTS];
		float uastc_err_f[MAX_ENCODE_RESULTS];
		double best_uastc_err_f = 1e+20f;

		int best_index = -1;

		if (total_results == 1)
		{
			best_index = 0;
		}
		else
		{
			const uint32_t bc7_err_weight = favor_bc7_error ? 100 : ((favor_uastc_error ? 0 : DEFAULT_BC7_ERROR_WEIGHT));
			const uint32_t uastc_err_weight = favor_bc7_error ? 0 : 100;

			// Find best overall results, balancing UASTC and UASTC->BC7 error.
			// We purposely allow UASTC error to increase a little, if doing so lowers the BC7 error.
			for (uint32_t i = 0; i < total_results; i++)
			{
#if BASISU_SUPPORT_FORCE_MODE
				if (results[i].m_uastc_mode == force_mode)
				{
					best_index = i;
					break;
				}
#endif

				unpacked_ublock.m_mode = results[i].m_uastc_mode;
				unpacked_ublock.m_astc = results[i].m_astc;
				unpacked_ublock.m_common_pattern = results[i].m_common_pattern;
				unpacked_ublock.m_solid_color = results[i].m_solid_color.get_color32();

				color_rgba decoded_uastc_block[4][4];
				bool success = unpack_uastc(results[i].m_uastc_mode, results[i].m_common_pattern, results[i].m_solid_color.get_color32(), results[i].m_astc, (basist::color32 *)&decoded_uastc_block[0][0], false);
				(void)success;
				VALIDATE(success);

				uint64_t total_uastc_rgb_err, total_uastc_rgba_err, total_uastc_la_err;
				compute_block_error(block, decoded_uastc_block, total_uastc_rgb_err, total_uastc_rgba_err, total_uastc_la_err);

				// Validate the computed error, or we're go mad if it's inaccurate.
				if (results[i].m_uastc_mode == UASTC_MODE_INDEX_SOLID_COLOR)
				{
					VALIDATE(total_uastc_rgba_err == 0);
				}
				else if (is_la)
				{
					VALIDATE(total_uastc_la_err == results[i].m_astc_err);
				}
				else if (g_uastc_mode_has_alpha[results[i].m_uastc_mode])
				{
					VALIDATE(total_uastc_rgba_err == results[i].m_astc_err);
				}
				else
				{
					VALIDATE(total_uastc_rgb_err == results[i].m_astc_err);
				}

				// Transcode to BC7
				bc7_optimization_results bc7_results;
				transcode_uastc_to_bc7(unpacked_ublock, bc7_results);

				bc7_block bc7_data;
				encode_bc7_block(&bc7_data, &bc7_results);

				color_rgba decoded_bc7_block[4][4];
				unpack_block(texture_format::cBC7, &bc7_data, &decoded_bc7_block[0][0]);

				// Compute BC7 error
				uint64_t total_bc7_la_err, total_bc7_rgb_err, total_bc7_rgba_err;
				compute_block_error(block, decoded_bc7_block, total_bc7_rgb_err, total_bc7_rgba_err, total_bc7_la_err);

				if (results[i].m_uastc_mode == UASTC_MODE_INDEX_SOLID_COLOR)
				{
					VALIDATE(total_bc7_rgba_err == 0);

					best_index = i;
					break;
				}

				uint64_t total_uastc_err = 0, total_bc7_err = 0;
				if (is_la)
				{
					total_bc7_err = total_bc7_la_err;
					total_uastc_err = total_uastc_la_err;
				}
				else if (has_alpha)
				{
					total_bc7_err = total_bc7_rgba_err;
					total_uastc_err = total_uastc_rgba_err;
				}
				else
				{
					total_bc7_err = total_bc7_rgb_err;
					total_uastc_err = total_uastc_rgb_err;
				}

				total_overall_err[i] = ((total_bc7_err * bc7_err_weight) / 100) + ((total_uastc_err * uastc_err_weight) / 100);
				if (!total_overall_err[i])
				{
					best_index = i;
					break;
				}

				uastc_err_f[i] = sqrtf((float)total_uastc_err);

				if (uastc_err_f[i] < best_uastc_err_f)
				{
					best_uastc_err_f = uastc_err_f[i];
				}

			} // total_results

			if (best_index < 0)
			{
				uint64_t best_err = UINT64_MAX;

				if ((best_uastc_err_f == 0.0f) || (favor_bc7_error))
				{
					for (uint32_t i = 0; i < total_results; i++)
					{
						// TODO: This is a quick hack to favor modes 0 or 10 for better RDO compression.
						const float err_weight = (flags & cPackUASTCFavorSimplerModes) ? get_uastc_mode_weight(results[i].m_uastc_mode) : 1.0f;

						const uint64_t w = (uint64_t)(total_overall_err[i] * err_weight);
						if (w  < best_err)
						{
							best_err = w;
							best_index = i;
							if (!best_err)
								break;
						}
					} // i
				}
				else
				{
					// Scan the UASTC results, and consider all results within a window that has the best UASTC+BC7 error.
					for (uint32_t i = 0; i < total_results; i++)
					{
						double err_delta = uastc_err_f[i] / best_uastc_err_f;

						if (err_delta <= UASTC_ERROR_THRESH)
						{
							// TODO: This is a quick hack to favor modes 0 or 10 for better RDO compression.
							const float err_weight = (flags & cPackUASTCFavorSimplerModes) ? get_uastc_mode_weight(results[i].m_uastc_mode) : 1.0f;

							const uint64_t w = (uint64_t)(total_overall_err[i] * err_weight);
							if (w < best_err)
							{
								best_err = w;
								best_index = i;
								if (!best_err)
									break;
							}
						}
					} // i
				}
			}
		}

		const uastc_encode_results& best_results = results[best_index];
		const uint32_t best_mode = best_results.m_uastc_mode;
		const astc_block_desc& best_astc_results = best_results.m_astc;
				
		color_rgba decoded_uastc_block[4][4];
		bool success = unpack_uastc(best_mode, best_results.m_common_pattern, best_results.m_solid_color.get_color32(), best_astc_results, (basist::color32 *)&decoded_uastc_block[0][0], false);
		(void)success;
		VALIDATE(success);

#if BASISU_VALIDATE_UASTC_ENC
		// Make sure that the UASTC block unpacks to the same exact pixels as the ASTC block does, using two different decoders.
		{
			// Round trip to packed UASTC and back, then decode to pixels.
			etc_block etc1_blk;
			memset(&etc1_blk, 0, sizeof(etc1_blk));
			eac_a8_block etc_eac_a8_blk;
			memset(&etc_eac_a8_blk, 0, sizeof(etc_eac_a8_blk));
			etc_eac_a8_blk.m_multiplier = 1;

			basist::uastc_block temp_block;
			pack_uastc(temp_block, best_results, etc1_blk, 0, etc_eac_a8_blk, false, false);
			
			basist::color32 temp_block_unpacked[4][4];
			success = basist::unpack_uastc(temp_block, (basist::color32 *)temp_block_unpacked, false);
			VALIDATE(success);

			// Now round trip to packed ASTC and back, then decode to pixels.
			uint32_t astc_data[4];
			
			if (best_results.m_uastc_mode == UASTC_MODE_INDEX_SOLID_COLOR)
				pack_astc_solid_block(astc_data, (color32 &)best_results.m_solid_color);
			else
			{
				success = pack_astc_block(astc_data, &best_astc_results, best_results.m_uastc_mode);
				VALIDATE(success);
			}

			color_rgba decoded_astc_block[4][4];
			success = basisu_astc::astc::decompress_ldr((uint8_t*)decoded_astc_block, (uint8_t*)&astc_data, false, 4, 4);
			VALIDATE(success);

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					VALIDATE(decoded_astc_block[y][x] == decoded_uastc_block[y][x]);
					
					VALIDATE(temp_block_unpacked[y][x].c[0] == decoded_uastc_block[y][x].r);
					VALIDATE(temp_block_unpacked[y][x].c[1] == decoded_uastc_block[y][x].g);
					VALIDATE(temp_block_unpacked[y][x].c[2] == decoded_uastc_block[y][x].b);
					VALIDATE(temp_block_unpacked[y][x].c[3] == decoded_uastc_block[y][x].a);
				}
			}
		}
#endif

		// Compute BC1 hints
		bool bc1_hint0 = false, bc1_hint1 = false;
		if (bc1_hints)
			compute_bc1_hints(bc1_hint0, bc1_hint1, best_results, block, decoded_uastc_block);
		
		eac_a8_block eac_a8_blk;
		if ((g_uastc_mode_has_alpha[best_mode]) && (best_mode != UASTC_MODE_INDEX_SOLID_COLOR))
		{
			// Compute ETC2 hints
			uint8_t decoded_uastc_block_alpha[16];
			for (uint32_t i = 0; i < 16; i++)
				decoded_uastc_block_alpha[i] = decoded_uastc_block[i >> 2][i & 3].a;

			uastc_pack_eac_a8_results eac8_a8_results;
			memset(&eac8_a8_results, 0, sizeof(eac8_a8_results));
			uastc_pack_eac_a8(eac8_a8_results, decoded_uastc_block_alpha, 16, 0, eac_a8_mul_search_rad, eac_a8_table_mask);
						
			// All we care about for hinting is the table and multiplier.
			eac_a8_blk.m_table = eac8_a8_results.m_table;
			eac_a8_blk.m_multiplier = eac8_a8_results.m_multiplier;
		}
		else
		{
			memset(&eac_a8_blk, 0, sizeof(eac_a8_blk));
		}

		// Compute ETC1 hints
		etc_block etc1_blk;
		uint32_t etc1_bias = 0;
		compute_etc1_hints(etc1_blk, etc1_bias, best_results, block, decoded_uastc_block, level, flags);

		// Finally, pack the UASTC block with its hints and we're done.
		pack_uastc(output_block, best_results, etc1_blk, etc1_bias, eac_a8_blk, bc1_hint0, bc1_hint1);

//		printf(" Packed: ");
//		for (int i = 0; i < 16; i++)
//			printf("%X ", output_block.m_bytes[i]);
//		printf("\n");
	}

	static bool uastc_recompute_hints(basist::uastc_block* pBlock, const color_rgba* pBlock_pixels, uint32_t flags, const unpacked_uastc_block *pUnpacked_blk)
	{
		unpacked_uastc_block unpacked_blk;

		if (pUnpacked_blk)
			unpacked_blk = *pUnpacked_blk;
		else
		{
			if (!unpack_uastc(*pBlock, unpacked_blk, false, true))
				return false;
		}
		color_rgba decoded_uastc_block[4][4];
		if (!unpack_uastc(unpacked_blk, (basist::color32 *)decoded_uastc_block, false))
			return false;
		uastc_encode_results results;
		results.m_uastc_mode = unpacked_blk.m_mode;
		results.m_common_pattern = unpacked_blk.m_common_pattern;
		results.m_astc = unpacked_blk.m_astc;
		results.m_solid_color = unpacked_blk.m_solid_color;
		results.m_astc_err = 0;
		bool bc1_hints = true;
		uint32_t eac_a8_mul_search_rad = 3;
		uint32_t eac_a8_table_mask = UINT32_MAX;
		const uint32_t level = flags & cPackUASTCLevelMask;
		switch (level)
		{
		case cPackUASTCLevelFastest:
		{
			eac_a8_mul_search_rad = 0;
			eac_a8_table_mask = (1 << 2) | (1 << 8) | (1 << 11) | (1 << 13);
			bc1_hints = false;
			break;
		}
		case cPackUASTCLevelFaster:
		{
			eac_a8_mul_search_rad = 0;
			eac_a8_table_mask = (1 << 2) | (1 << 8) | (1 << 11) | (1 << 13);
			break;
		}
		case cPackUASTCLevelDefault:
		{
			eac_a8_mul_search_rad = 1;
			eac_a8_table_mask = (1 << 0) | (1 << 2) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 10) | (1 << 11) | (1 << 13);
			break;
		}
		case cPackUASTCLevelSlower:
		{
			eac_a8_mul_search_rad = 2;
			break;
		}
		case cPackUASTCLevelVerySlow:
		{
			break;
		}
		}
		bool bc1_hint0 = false, bc1_hint1 = false;
		if (bc1_hints)
			compute_bc1_hints(bc1_hint0, bc1_hint1, results, (color_rgba (*)[4])pBlock_pixels, decoded_uastc_block);
		const uint32_t best_mode = unpacked_blk.m_mode;
		eac_a8_block eac_a8_blk;
		if ((g_uastc_mode_has_alpha[best_mode]) && (best_mode != UASTC_MODE_INDEX_SOLID_COLOR))
		{
			uint8_t decoded_uastc_block_alpha[16];
			for (uint32_t i = 0; i < 16; i++)
				decoded_uastc_block_alpha[i] = decoded_uastc_block[i >> 2][i & 3].a;
			uastc_pack_eac_a8_results eac8_a8_results;
			memset(&eac8_a8_results, 0, sizeof(eac8_a8_results));
			uastc_pack_eac_a8(eac8_a8_results, decoded_uastc_block_alpha, 16, 0, eac_a8_mul_search_rad, eac_a8_table_mask);
			eac_a8_blk.m_table = eac8_a8_results.m_table;
			eac_a8_blk.m_multiplier = eac8_a8_results.m_multiplier;
		}
		else
		{
			memset(&eac_a8_blk, 0, sizeof(eac_a8_blk));
		}
		etc_block etc1_blk;
		uint32_t etc1_bias = 0;
		compute_etc1_hints(etc1_blk, etc1_bias, results, (color_rgba (*)[4])pBlock_pixels, decoded_uastc_block, level, flags);
		pack_uastc(*pBlock, results, etc1_blk, etc1_bias, eac_a8_blk, bc1_hint0, bc1_hint1);
		return true;
	}

	static const uint8_t g_uastc_mode_selector_bits[TOTAL_UASTC_MODES][2] =
	{
		{ 65, 63 }, { 69, 31 }, { 73, 46 }, { 89, 29 },
		{ 89, 30 }, { 68, 47 }, { 66, 62 }, { 89, 30 },
		{ 0, 0 }, { 97, 30 }, { 65, 63 }, { 66, 62 },
		{ 81, 47 }, { 94, 30 }, { 92, 31 }, { 62, 63 },
		{ 98, 30 }, { 61, 62 }, { 49, 79 }
	};

	static inline uint32_t set_block_bits(uint8_t* pBytes, uint64_t val, uint32_t num_bits, uint32_t cur_ofs)
	{
		assert(num_bits <= 64);
		assert((num_bits == 64) || (val < (1ULL << num_bits)));
		uint64_t mask = (num_bits == 64) ? UINT64_MAX : ((1ULL << num_bits) - 1);
		while (num_bits)
		{
			const uint32_t n = basisu::minimum<uint32_t>(8U - (cur_ofs & 7U), num_bits);
			pBytes[cur_ofs >> 3] &= ~static_cast<uint8_t>(mask << (cur_ofs & 7U));
			pBytes[cur_ofs >> 3] |= static_cast<uint8_t>(val << (cur_ofs & 7U));
			val >>= n;
			mask >>= n;
			num_bits -= n;
			cur_ofs += n;
		}
		return cur_ofs;
	}

	static const uint8_t g_tdefl_small_dist_extra[512] =
	{
		0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7
	};

	static const uint8_t g_tdefl_large_dist_extra[128] =
	{
		0, 0, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
		12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
		13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13
	};

	static inline uint32_t compute_match_cost_estimate(uint32_t dist)
	{
		uint32_t len_cost = 7;
		uint32_t dist_cost = 5;
		if (dist < 512)
			dist_cost += g_tdefl_small_dist_extra[dist & 511];
		else
		{
			dist_cost += g_tdefl_large_dist_extra[basisu::minimum<uint32_t>(dist, 32767) >> 8];
			while (dist >= 32768)
			{
				dist_cost++;
				dist >>= 1;
			}
		}
		return len_cost + dist_cost;
	}

	struct selector_bitsequence
	{
		uint64_t m_sel;
		uint32_t m_ofs;
		uint32_t m_pad; // avoid implicit padding for selector_bitsequence_hash
		selector_bitsequence() { }
		selector_bitsequence(uint32_t bit_ofs, uint64_t sel) : m_sel(sel), m_ofs(bit_ofs), m_pad(0) { }
		bool operator== (const selector_bitsequence& other) const
		{
			return (m_ofs == other.m_ofs) && (m_sel == other.m_sel);
		}

		bool operator< (const selector_bitsequence& other) const
		{
			if (m_ofs < other.m_ofs)
				return true;
			else if (m_ofs == other.m_ofs)
				return m_sel < other.m_sel;

			return false;
		}
	};

	struct selector_bitsequence_hash
	{
		std::size_t operator()(selector_bitsequence const& s) const noexcept
		{
			return hash_hsieh((const uint8_t*)&s, sizeof(s));
		}
	};

	class tracked_stat
	{
	public:
		tracked_stat() { clear(); }

		void clear() { m_num = 0; m_total = 0; m_total2 = 0; }

		void update(uint32_t val) { m_num++; m_total += val; m_total2 += val * val; }

		tracked_stat& operator += (uint32_t val) { update(val); return *this; }

		uint32_t get_number_of_values() { return m_num; }
		uint64_t get_total() const { return m_total; }
		uint64_t get_total2() const { return m_total2; }

		float get_average() const { return m_num ? (float)m_total / m_num : 0.0f; };
		float get_std_dev() const { return m_num ? sqrtf((float)(m_num * m_total2 - m_total * m_total)) / m_num : 0.0f; }
		float get_variance() const { float s = get_std_dev(); return s * s; }

	private:
		uint32_t m_num;
		uint64_t m_total;
		uint64_t m_total2;
	};
		
	static bool uastc_rdo_blocks(uint32_t first_index, uint32_t last_index, basist::uastc_block* pBlocks, const color_rgba* pBlock_pixels, const uastc_rdo_params& params, uint32_t flags, 
		uint32_t &total_skipped, uint32_t &total_refined, uint32_t &total_modified, uint32_t &total_smooth)
	{
		debug_printf("uastc_rdo_blocks: Processing blocks %u to %u\n", first_index, last_index);

		const int total_blocks_to_check = basisu::maximum<uint32_t>(1U, params.m_lz_dict_size / sizeof(basist::uastc_block));
		const bool perceptual = false;

		std::unordered_map<selector_bitsequence, uint32_t, selector_bitsequence_hash> selector_history;
						
		for (uint32_t block_index = first_index; block_index < last_index; block_index++)
		{
			const basist::uastc_block& blk = pBlocks[block_index];
			const color_rgba* pPixels = &pBlock_pixels[16 * block_index];

			unpacked_uastc_block unpacked_blk;
			if (!unpack_uastc(blk, unpacked_blk, false, true))
				return false;

			const uint32_t block_mode = unpacked_blk.m_mode;
			if (block_mode == UASTC_MODE_INDEX_SOLID_COLOR)
				continue;

			tracked_stat r_stats, g_stats, b_stats, a_stats;

			for (uint32_t i = 0; i < 16; i++)
			{
				r_stats.update(pPixels[i].r);
				g_stats.update(pPixels[i].g);
				b_stats.update(pPixels[i].b);
				a_stats.update(pPixels[i].a);
			}

			const float max_std_dev = basisu::maximum<float>(basisu::maximum<float>(basisu::maximum(r_stats.get_std_dev(), g_stats.get_std_dev()), b_stats.get_std_dev()), a_stats.get_std_dev());

			float yl = clamp<float>(max_std_dev / params.m_max_smooth_block_std_dev, 0.0f, 1.0f);
			yl = yl * yl;
			const float smooth_block_error_scale = lerp<float>(params.m_smooth_block_max_error_scale, 1.0f, yl);
			if (smooth_block_error_scale > 1.0f)
				total_smooth++;

			color_rgba decoded_uastc_block[4][4];
			if (!unpack_uastc(unpacked_blk, (basist::color32*)decoded_uastc_block, false))
				return false;

			uint64_t uastc_err = 0;
			for (uint32_t i = 0; i < 16; i++)
				uastc_err += color_distance(perceptual, pPixels[i], ((color_rgba*)decoded_uastc_block)[i], true);

			// Transcode to BC7
			bc7_optimization_results b7_results;
			if (!transcode_uastc_to_bc7(unpacked_blk, b7_results))
				return false;

			basist::bc7_block b7_block;
			basist::encode_bc7_block(&b7_block, &b7_results);

			color_rgba decoded_b7_blk[4][4];
			unpack_block(texture_format::cBC7, &b7_block, &decoded_b7_blk[0][0]);
						
			uint64_t bc7_err = 0;
			for (uint32_t i = 0; i < 16; i++)
				bc7_err += color_distance(perceptual, pPixels[i], ((color_rgba*)decoded_b7_blk)[i], true);

			uint64_t cur_err = (uastc_err + bc7_err) / 2;

			// Divide by 16*4 to compute RMS error
			const float cur_ms_err = (float)cur_err * (1.0f / 64.0f);
			const float cur_rms_err = sqrt(cur_ms_err);

			const uint32_t first_sel_bit = g_uastc_mode_selector_bits[block_mode][0];
			const uint32_t total_sel_bits = g_uastc_mode_selector_bits[block_mode][1];
			assert(first_sel_bit + total_sel_bits <= 128);
			assert(total_sel_bits > 0);

			uint32_t cur_bit_offset = first_sel_bit;
			uint64_t cur_sel_bits = read_bits((const uint8_t*)&blk, cur_bit_offset, basisu::minimum(64U, total_sel_bits));

			if (cur_rms_err >= params.m_skip_block_rms_thresh)
			{
				auto cur_search_res = selector_history.insert(std::make_pair(selector_bitsequence(first_sel_bit, cur_sel_bits), block_index));

				// Block already has too much error, so don't mess with it.
				if (!cur_search_res.second)
					(*cur_search_res.first).second = block_index;

				total_skipped++;
				continue;
			}

			int cur_bits;
			auto cur_find_res = selector_history.find(selector_bitsequence(first_sel_bit, cur_sel_bits));
			if (cur_find_res == selector_history.end())
			{
				// Wasn't found - wildly estimate literal cost
				//cur_bits = (total_sel_bits * 5) / 4;
				cur_bits = (total_sel_bits * params.m_lz_literal_cost) / 100;
			}
			else
			{
				// Was found - wildly estimate match cost
				uint32_t match_block_index = cur_find_res->second;
				const int block_dist_in_bytes = (block_index - match_block_index) * 16;
				cur_bits = compute_match_cost_estimate(block_dist_in_bytes);
			}

			int first_block_to_check = basisu::maximum<int>(first_index, block_index - total_blocks_to_check);
			int last_block_to_check = block_index - 1;

			basist::uastc_block best_block(blk);
			uint32_t best_block_index = block_index;

			float best_t = cur_ms_err * smooth_block_error_scale + cur_bits * params.m_lambda;

			// Now scan through previous blocks, insert their selector bit patterns into the current block, and find 
			// selector bit patterns which don't increase the overall block error too much.
			for (int prev_block_index = last_block_to_check; prev_block_index >= first_block_to_check; --prev_block_index)
			{
				const basist::uastc_block& prev_blk = pBlocks[prev_block_index];

				uint32_t bit_offset = first_sel_bit;
				uint64_t sel_bits = read_bits((const uint8_t*)&prev_blk, bit_offset, basisu::minimum(64U, total_sel_bits));

				int match_block_index = prev_block_index;
				auto res = selector_history.find(selector_bitsequence(first_sel_bit, sel_bits));
				if (res != selector_history.end())
					match_block_index = res->second;
				// Have we already checked this bit pattern? If so then skip this block.
				if (match_block_index > prev_block_index)
					continue;

				unpacked_uastc_block unpacked_prev_blk;
				if (!unpack_uastc(prev_blk, unpacked_prev_blk, false, true))
					return false;

				basist::uastc_block trial_blk(blk);

				set_block_bits((uint8_t*)&trial_blk, sel_bits, basisu::minimum(64U, total_sel_bits), first_sel_bit);

				if (total_sel_bits > 64)
				{
					sel_bits = read_bits((const uint8_t*)&prev_blk, bit_offset, total_sel_bits - 64U);

					set_block_bits((uint8_t*)&trial_blk, sel_bits, total_sel_bits - 64U, first_sel_bit + basisu::minimum(64U, total_sel_bits));
				}

				unpacked_uastc_block unpacked_trial_blk;
				if (!unpack_uastc(trial_blk, unpacked_trial_blk, false, true))
					continue;

				color_rgba decoded_trial_uastc_block[4][4];
				if (!unpack_uastc(unpacked_trial_blk, (basist::color32*)decoded_trial_uastc_block, false))
					continue;

				uint64_t trial_uastc_err = 0;
				for (uint32_t i = 0; i < 16; i++)
					trial_uastc_err += color_distance(perceptual, pPixels[i], ((color_rgba*)decoded_trial_uastc_block)[i], true);

				// Transcode trial to BC7, compute error
				bc7_optimization_results trial_b7_results;
				if (!transcode_uastc_to_bc7(unpacked_trial_blk, trial_b7_results))
					return false;

				basist::bc7_block trial_b7_block;
				basist::encode_bc7_block(&trial_b7_block, &trial_b7_results);

				color_rgba decoded_trial_b7_blk[4][4];
				unpack_block(texture_format::cBC7, &trial_b7_block, &decoded_trial_b7_blk[0][0]);

				uint64_t trial_bc7_err = 0;
				for (uint32_t i = 0; i < 16; i++)
					trial_bc7_err += color_distance(perceptual, pPixels[i], ((color_rgba*)decoded_trial_b7_blk)[i], true);

				uint64_t trial_err = (trial_uastc_err + trial_bc7_err) / 2;

				const float trial_ms_err = (float)trial_err * (1.0f / 64.0f);
				const float trial_rms_err = sqrtf(trial_ms_err);

				if (trial_rms_err > cur_rms_err * params.m_max_allowed_rms_increase_ratio)
					continue;

				const int block_dist_in_bytes = (block_index - match_block_index) * 16;
				const int match_bits = compute_match_cost_estimate(block_dist_in_bytes);

				float t = trial_ms_err * smooth_block_error_scale + match_bits * params.m_lambda;
				if (t < best_t)
				{
					best_t = t;
					best_block_index = prev_block_index;

					best_block = trial_blk;
				}

			} // prev_block_index

			if (best_block_index != block_index)
			{
				total_modified++;

				unpacked_uastc_block unpacked_best_blk;
				if (!unpack_uastc(best_block, unpacked_best_blk, false, false))
					return false;

				if ((params.m_endpoint_refinement) && (block_mode == 0))
				{
					// Attempt to refine mode 0 block's endpoints, using the new selectors. This doesn't help much, but it does help.
					// TODO: We could do this with the other modes too.
					color_rgba decoded_best_uastc_block[4][4];
					if (!unpack_uastc(unpacked_best_blk, (basist::color32*)decoded_best_uastc_block, false))
						return false;

					// Compute the block's current error (with the modified selectors).
					uint64_t best_uastc_err = 0;
					for (uint32_t i = 0; i < 16; i++)
						best_uastc_err += color_distance(perceptual, pPixels[i], ((color_rgba*)decoded_best_uastc_block)[i], true);

					bc7enc_compress_block_params comp_params;
					memset(&comp_params, 0, sizeof(comp_params));
					comp_params.m_max_partitions_mode1 = 64;
					comp_params.m_least_squares_passes = 1;
					comp_params.m_weights[0] = 1;
					comp_params.m_weights[1] = 1;
					comp_params.m_weights[2] = 1;
					comp_params.m_weights[3] = 1;
					comp_params.m_uber_level = 0;

					uastc_encode_results results;
					uint32_t total_results = 0;
					astc_mode0_or_18(0, (color_rgba(*)[4])pPixels, &results, total_results, comp_params, unpacked_best_blk.m_astc.m_weights);
					assert(total_results == 1);

					// See if the overall error has actually gone done.

					color_rgba decoded_trial_uastc_block[4][4];
					bool success = unpack_uastc(results.m_uastc_mode, results.m_common_pattern, results.m_solid_color.get_color32(), results.m_astc, (basist::color32*) & decoded_trial_uastc_block[0][0], false);
					assert(success);
					
					BASISU_NOTE_UNUSED(success);

					uint64_t trial_uastc_err = 0;
					for (uint32_t i = 0; i < 16; i++)
						trial_uastc_err += color_distance(perceptual, pPixels[i], ((color_rgba*)decoded_trial_uastc_block)[i], true);

					if (trial_uastc_err < best_uastc_err)
					{
						// The error went down, so accept the new endpoints.

						// Ensure the selectors haven't changed, otherwise we'll invalidate the LZ matches.
						for (uint32_t i = 0; i < 16; i++)
							assert(unpacked_best_blk.m_astc.m_weights[i] == results.m_astc.m_weights[i]);

						unpacked_best_blk.m_astc = results.m_astc;

						total_refined++;
					}
				} // if ((params.m_endpoint_refinement) && (block_mode == 0))

				// The selectors have changed, so go recompute the block hints.
				if (!uastc_recompute_hints(&best_block, pPixels, flags, &unpacked_best_blk))
					return false;

				// Write the modified block
				pBlocks[block_index] = best_block;
			
			} // if (best_block_index != block_index)

			{
				uint32_t bit_offset = first_sel_bit;
				uint64_t sel_bits = read_bits((const uint8_t*)&best_block, bit_offset, basisu::minimum(64U, total_sel_bits));

				auto res = selector_history.insert(std::make_pair(selector_bitsequence(first_sel_bit, sel_bits), block_index));
				if (!res.second)
					(*res.first).second = block_index;
			}

		} // block_index

		return true;
	}
				
	// This function implements a basic form of rate distortion optimization (RDO) for UASTC. 
	// It only changes selectors and then updates the hints. It uses very approximate LZ bitprice estimation.
	// There's A LOT that can be done better in here, but it's a start.
	// One nice advantage of the method used here is that it works for any input, no matter which or how many modes it uses.
	bool uastc_rdo(uint32_t num_blocks, basist::uastc_block* pBlocks, const color_rgba* pBlock_pixels, const uastc_rdo_params& params, uint32_t flags, job_pool* pJob_pool, uint32_t total_jobs)
	{
		assert(params.m_max_allowed_rms_increase_ratio > 1.0f);
		assert(params.m_lz_dict_size > 0);
		assert(params.m_lambda > 0.0f);

		uint32_t total_skipped = 0, total_modified = 0, total_refined = 0, total_smooth = 0;

		uint32_t blocks_per_job = total_jobs ? (num_blocks / total_jobs) : 0;

		std::mutex stat_mutex;

		bool status = false;

		if ((!pJob_pool) || (total_jobs <= 1) || (blocks_per_job <= 8))
		{
			status = uastc_rdo_blocks(0, num_blocks, pBlocks, pBlock_pixels, params, flags, total_skipped, total_refined, total_modified, total_smooth);
		}
		else
		{
			bool all_succeeded = true;

			for (uint32_t block_index_iter = 0; block_index_iter < num_blocks; block_index_iter += blocks_per_job)
			{
				const uint32_t first_index = block_index_iter;
				const uint32_t last_index = minimum<uint32_t>(num_blocks, block_index_iter + blocks_per_job);

#ifndef __EMSCRIPTEN__
				pJob_pool->add_job([first_index, last_index, pBlocks, pBlock_pixels, &params, flags, &total_skipped, &total_modified, &total_refined, &total_smooth, &all_succeeded, &stat_mutex] {
#endif

					uint32_t job_skipped = 0, job_modified = 0, job_refined = 0, job_smooth = 0;

					bool status = uastc_rdo_blocks(first_index, last_index, pBlocks, pBlock_pixels, params, flags, job_skipped, job_refined, job_modified, job_smooth);

					{
						std::lock_guard<std::mutex> lck(stat_mutex);
						
						all_succeeded = all_succeeded && status;
						total_skipped += job_skipped;
						total_modified += job_modified;
						total_refined += job_refined;
						total_smooth += job_smooth;
					}

#ifndef __EMSCRIPTEN__
					}
				);
#endif

			} // block_index_iter

#ifndef __EMSCRIPTEN__
			pJob_pool->wait_for_all();
#endif

			status = all_succeeded;
		}

		debug_printf("uastc_rdo: Total modified: %3.2f%%, total skipped: %3.2f%%, total refined: %3.2f%%, total smooth: %3.2f%%\n", total_modified * 100.0f / num_blocks, total_skipped * 100.0f / num_blocks, total_refined * 100.0f / num_blocks, total_smooth * 100.0f / num_blocks);
				
		return status;
	}
} // namespace basisu





