// File: basisu_bc7enc.cpp
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
#include "basisu_bc7enc.h"

#ifdef _DEBUG
#define BC7ENC_CHECK_OVERALL_ERROR 1
#else
#define BC7ENC_CHECK_OVERALL_ERROR 0
#endif

using namespace basist;

namespace basisu
{

// Helpers
static inline color_quad_u8 *color_quad_u8_set_clamped(color_quad_u8 *pRes, int32_t r, int32_t g, int32_t b, int32_t a) { pRes->m_c[0] = (uint8_t)clampi(r, 0, 255); pRes->m_c[1] = (uint8_t)clampi(g, 0, 255); pRes->m_c[2] = (uint8_t)clampi(b, 0, 255); pRes->m_c[3] = (uint8_t)clampi(a, 0, 255); return pRes; }
static inline color_quad_u8 *color_quad_u8_set(color_quad_u8 *pRes, int32_t r, int32_t g, int32_t b, int32_t a) { assert((uint32_t)(r | g | b | a) <= 255); pRes->m_c[0] = (uint8_t)r; pRes->m_c[1] = (uint8_t)g; pRes->m_c[2] = (uint8_t)b; pRes->m_c[3] = (uint8_t)a; return pRes; }
static inline bc7enc_bool color_quad_u8_notequals(const color_quad_u8 *pLHS, const color_quad_u8 *pRHS) { return (pLHS->m_c[0] != pRHS->m_c[0]) || (pLHS->m_c[1] != pRHS->m_c[1]) || (pLHS->m_c[2] != pRHS->m_c[2]) || (pLHS->m_c[3] != pRHS->m_c[3]); }
static inline bc7enc_vec4F*vec4F_set_scalar(bc7enc_vec4F*pV, float x) {	pV->m_c[0] = x; pV->m_c[1] = x; pV->m_c[2] = x;	pV->m_c[3] = x;	return pV; }
static inline bc7enc_vec4F*vec4F_set(bc7enc_vec4F*pV, float x, float y, float z, float w) {	pV->m_c[0] = x;	pV->m_c[1] = y;	pV->m_c[2] = z;	pV->m_c[3] = w;	return pV; }
static inline bc7enc_vec4F*vec4F_saturate_in_place(bc7enc_vec4F*pV) { pV->m_c[0] = saturate(pV->m_c[0]); pV->m_c[1] = saturate(pV->m_c[1]); pV->m_c[2] = saturate(pV->m_c[2]); pV->m_c[3] = saturate(pV->m_c[3]); return pV; }
static inline bc7enc_vec4F vec4F_saturate(const bc7enc_vec4F*pV) { bc7enc_vec4F res; res.m_c[0] = saturate(pV->m_c[0]); res.m_c[1] = saturate(pV->m_c[1]); res.m_c[2] = saturate(pV->m_c[2]); res.m_c[3] = saturate(pV->m_c[3]); return res; }
static inline bc7enc_vec4F vec4F_from_color(const color_quad_u8 *pC) { bc7enc_vec4F res; vec4F_set(&res, pC->m_c[0], pC->m_c[1], pC->m_c[2], pC->m_c[3]); return res; }
static inline bc7enc_vec4F vec4F_add(const bc7enc_vec4F*pLHS, const bc7enc_vec4F*pRHS) { bc7enc_vec4F res; vec4F_set(&res, pLHS->m_c[0] + pRHS->m_c[0], pLHS->m_c[1] + pRHS->m_c[1], pLHS->m_c[2] + pRHS->m_c[2], pLHS->m_c[3] + pRHS->m_c[3]); return res; }
static inline bc7enc_vec4F vec4F_sub(const bc7enc_vec4F*pLHS, const bc7enc_vec4F*pRHS) { bc7enc_vec4F res; vec4F_set(&res, pLHS->m_c[0] - pRHS->m_c[0], pLHS->m_c[1] - pRHS->m_c[1], pLHS->m_c[2] - pRHS->m_c[2], pLHS->m_c[3] - pRHS->m_c[3]); return res; }
static inline float vec4F_dot(const bc7enc_vec4F*pLHS, const bc7enc_vec4F*pRHS) { return pLHS->m_c[0] * pRHS->m_c[0] + pLHS->m_c[1] * pRHS->m_c[1] + pLHS->m_c[2] * pRHS->m_c[2] + pLHS->m_c[3] * pRHS->m_c[3]; }
static inline bc7enc_vec4F vec4F_mul(const bc7enc_vec4F*pLHS, float s) { bc7enc_vec4F res; vec4F_set(&res, pLHS->m_c[0] * s, pLHS->m_c[1] * s, pLHS->m_c[2] * s, pLHS->m_c[3] * s); return res; }
static inline bc7enc_vec4F* vec4F_normalize_in_place(bc7enc_vec4F*pV) { float s = pV->m_c[0] * pV->m_c[0] + pV->m_c[1] * pV->m_c[1] + pV->m_c[2] * pV->m_c[2] + pV->m_c[3] * pV->m_c[3]; if (s != 0.0f) { s = 1.0f / sqrtf(s); pV->m_c[0] *= s; pV->m_c[1] *= s; pV->m_c[2] *= s; pV->m_c[3] *= s; } return pV; }

// Precomputed weight constants used during least fit determination. For each entry in g_bc7_weights[]: w * w, (1.0f - w) * w, (1.0f - w) * (1.0f - w), w
const float g_bc7_weights1x[2 * 4] = { 0.000000f, 0.000000f, 1.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000000f, 1.000000f };

const float g_bc7_weights2x[4 * 4] = { 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.107666f, 0.220459f, 0.451416f, 0.328125f, 0.451416f, 0.220459f, 0.107666f, 0.671875f, 1.000000f, 0.000000f, 0.000000f, 1.000000f };

const float g_bc7_weights3x[8 * 4] = { 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.019775f, 0.120850f, 0.738525f, 0.140625f, 0.079102f, 0.202148f, 0.516602f, 0.281250f, 0.177979f, 0.243896f, 0.334229f, 0.421875f, 0.334229f, 0.243896f, 0.177979f, 0.578125f, 0.516602f, 0.202148f,
	0.079102f, 0.718750f, 0.738525f, 0.120850f, 0.019775f, 0.859375f, 1.000000f, 0.000000f, 0.000000f, 1.000000f };

const float g_bc7_weights4x[16 * 4] = { 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.003906f, 0.058594f, 0.878906f, 0.062500f, 0.019775f, 0.120850f, 0.738525f, 0.140625f, 0.041260f, 0.161865f, 0.635010f, 0.203125f, 0.070557f, 0.195068f, 0.539307f, 0.265625f, 0.107666f, 0.220459f,
	0.451416f, 0.328125f, 0.165039f, 0.241211f, 0.352539f, 0.406250f, 0.219727f, 0.249023f, 0.282227f, 0.468750f, 0.282227f, 0.249023f, 0.219727f, 0.531250f, 0.352539f, 0.241211f, 0.165039f, 0.593750f, 0.451416f, 0.220459f, 0.107666f, 0.671875f, 0.539307f, 0.195068f, 0.070557f, 0.734375f,
	0.635010f, 0.161865f, 0.041260f, 0.796875f, 0.738525f, 0.120850f, 0.019775f, 0.859375f, 0.878906f, 0.058594f, 0.003906f, 0.937500f, 1.000000f, 0.000000f, 0.000000f, 1.000000f };

const float g_astc_weights4x[16 * 4] = { 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.003906f, 0.058594f, 0.878906f, 0.062500f, 0.015625f, 0.109375f, 0.765625f, 0.125000f, 0.035156f, 0.152344f, 0.660156f, 0.187500f, 0.070557f, 0.195068f, 0.539307f, 0.265625f, 0.107666f, 0.220459f,
	0.451416f, 0.328125f, 0.152588f, 0.238037f, 0.371338f, 0.390625f, 0.205322f, 0.247803f, 0.299072f, 0.453125f, 0.299072f, 0.247803f, 0.205322f, 0.546875f, 0.371338f, 0.238037f, 0.152588f, 0.609375f, 0.451416f, 0.220459f, 0.107666f, 0.671875f, 0.539307f, 0.195068f, 0.070557f, 0.734375f,
	0.660156f, 0.152344f, 0.035156f, 0.812500f, 0.765625f, 0.109375f, 0.015625f, 0.875000f, 0.878906f, 0.058594f, 0.003906f, 0.937500f, 1.000000f, 0.000000f, 0.000000f, 1.000000f };

const float g_astc_weights5x[32 * 4] = { 0.000000f, 0.000000f, 1.000000f, 0.000000f, 0.000977f, 0.030273f, 0.938477f, 0.031250f, 0.003906f, 0.058594f, 0.878906f, 0.062500f, 0.008789f, 0.084961f, 0.821289f,
	0.093750f, 0.015625f, 0.109375f, 0.765625f, 0.125000f, 0.024414f, 0.131836f, 0.711914f, 0.156250f, 0.035156f, 0.152344f, 0.660156f, 0.187500f, 0.047852f, 0.170898f, 0.610352f, 0.218750f, 0.062500f, 0.187500f,
	0.562500f, 0.250000f, 0.079102f, 0.202148f, 0.516602f, 0.281250f, 0.097656f, 0.214844f, 0.472656f, 0.312500f, 0.118164f, 0.225586f, 0.430664f, 0.343750f, 0.140625f, 0.234375f, 0.390625f, 0.375000f, 0.165039f,
	0.241211f, 0.352539f, 0.406250f, 0.191406f, 0.246094f, 0.316406f, 0.437500f, 0.219727f, 0.249023f, 0.282227f, 0.468750f, 0.282227f, 0.249023f, 0.219727f, 0.531250f, 0.316406f, 0.246094f, 0.191406f, 0.562500f,
	0.352539f, 0.241211f, 0.165039f, 0.593750f, 0.390625f, 0.234375f, 0.140625f, 0.625000f, 0.430664f, 0.225586f, 0.118164f, 0.656250f, 0.472656f, 0.214844f, 0.097656f, 0.687500f, 0.516602f, 0.202148f, 0.079102f,
	0.718750f, 0.562500f, 0.187500f, 0.062500f, 0.750000f, 0.610352f, 0.170898f, 0.047852f, 0.781250f, 0.660156f, 0.152344f, 0.035156f, 0.812500f, 0.711914f, 0.131836f, 0.024414f, 0.843750f, 0.765625f, 0.109375f,
	0.015625f, 0.875000f, 0.821289f, 0.084961f, 0.008789f, 0.906250f, 0.878906f, 0.058594f, 0.003906f, 0.937500f, 0.938477f, 0.030273f, 0.000977f, 0.968750f, 1.000000f, 0.000000f, 0.000000f, 1.000000f };

const float g_astc_weights_3levelsx[3 * 4] = {
	0.000000f, 0.000000f, 1.000000f, 0.000000f,
	.5f * .5f, (1.0f - .5f) * .5f, (1.0f - .5f) * (1.0f - .5f), .5f,
	1.000000f, 0.000000f, 0.000000f, 1.000000f };

static endpoint_err g_bc7_mode_1_optimal_endpoints[256][2]; // [c][pbit]
static const uint32_t BC7ENC_MODE_1_OPTIMAL_INDEX = 2;

static endpoint_err g_astc_4bit_3bit_optimal_endpoints[256]; // [c]
static const uint32_t BC7ENC_ASTC_4BIT_3BIT_OPTIMAL_INDEX = 2;

static endpoint_err g_astc_4bit_2bit_optimal_endpoints[256]; // [c]
static const uint32_t BC7ENC_ASTC_4BIT_2BIT_OPTIMAL_INDEX = 1;

static endpoint_err g_astc_range7_2bit_optimal_endpoints[256]; // [c]
static const uint32_t BC7ENC_ASTC_RANGE7_2BIT_OPTIMAL_INDEX = 1;

static endpoint_err g_astc_range13_4bit_optimal_endpoints[256]; // [c]
static const uint32_t BC7ENC_ASTC_RANGE13_4BIT_OPTIMAL_INDEX = 2;

static endpoint_err g_astc_range13_2bit_optimal_endpoints[256]; // [c]
static const uint32_t BC7ENC_ASTC_RANGE13_2BIT_OPTIMAL_INDEX = 1;

static endpoint_err g_astc_range11_5bit_optimal_endpoints[256]; // [c]
static const uint32_t BC7ENC_ASTC_RANGE11_5BIT_OPTIMAL_INDEX = 13; // not 1, which is optimal, because 26 losslessly maps to BC7 4-bit weights

astc_quant_bin g_astc_sorted_order_unquant[BC7ENC_TOTAL_ASTC_RANGES][256]; // [sorted unquantized order]

static uint8_t g_astc_nearest_sorted_index[BC7ENC_TOTAL_ASTC_RANGES][256];

static void astc_init()
{
	for (uint32_t range = 0; range < BC7ENC_TOTAL_ASTC_RANGES; range++)
	{
		if (!astc_is_valid_endpoint_range(range))
			continue;
				
		const uint32_t levels = astc_get_levels(range);

		uint32_t vals[256];
		// TODO
		for (uint32_t i = 0; i < levels; i++)
			vals[i] = (unquant_astc_endpoint_val(i, range) << 8) | i;
		
		std::sort(vals, vals + levels);

		for (uint32_t i = 0; i < levels; i++)
		{
			uint32_t order = vals[i] & 0xFF;
			uint32_t unq = vals[i] >> 8;
						
			g_astc_sorted_order_unquant[range][i].m_unquant = (uint8_t)unq;
			g_astc_sorted_order_unquant[range][i].m_index = (uint8_t)order;
			
		} // i

#if 0
		if (g_astc_bise_range_table[range][1] || g_astc_bise_range_table[range][2])
		{
			printf("// Range: %u, Levels: %u, Bits: %u, Trits: %u, Quints: %u\n", range, levels, g_astc_bise_range_table[range][0], g_astc_bise_range_table[range][1], g_astc_bise_range_table[range][2]);

			printf("{");
			for (uint32_t i = 0; i < levels; i++)
			{
				printf("{%u,%u}", g_astc_sorted_order_unquant[range][i].m_index, g_astc_sorted_order_unquant[range][i].m_unquant);
				if (i != (levels - 1))
					printf(",");
			}
			printf("}\n");
		}
#endif

#if 0
		if (g_astc_bise_range_table[range][1] || g_astc_bise_range_table[range][2])
		{
			printf("// Range: %u, Levels: %u, Bits: %u, Trits: %u, Quints: %u\n", range, levels, g_astc_bise_range_table[range][0], g_astc_bise_range_table[range][1], g_astc_bise_range_table[range][2]);

			printf("{");
			for (uint32_t i = 0; i < levels; i++)
			{
				printf("{%u,%u}", g_astc_unquant[range][i].m_index, g_astc_unquant[range][i].m_unquant);
				if (i != (levels - 1))
					printf(",");
			}
			printf("}\n");
		}
#endif

		for (uint32_t i = 0; i < 256; i++)
		{
			uint32_t best_index = 0;
			int best_err = INT32_MAX;

			for (uint32_t j = 0; j < levels; j++)
			{
				int err = g_astc_sorted_order_unquant[range][j].m_unquant - i;
				if (err < 0)
					err = -err;
				if (err < best_err)
				{
					best_err = err;
					best_index = j;
				}
			}

			g_astc_nearest_sorted_index[range][i] = (uint8_t)best_index;
		} // i
	} // range
}

static inline uint32_t astc_interpolate_linear(uint32_t l, uint32_t h, uint32_t w)
{
	l = (l << 8) | l;
	h = (h << 8) | h;
	uint32_t k = (l * (64 - w) + h * w + 32) >> 6;
	return k >> 8;
}

// Initialize the lookup table used for optimal single color compression in mode 1. Must be called before encoding.
void bc7enc_compress_block_init()
{
	astc_init();
			
	// BC7 666.1
	for (int c = 0; c < 256; c++)
	{
		for (uint32_t lp = 0; lp < 2; lp++)
		{
			endpoint_err best;
			best.m_error = (uint16_t)UINT16_MAX;
			for (uint32_t l = 0; l < 64; l++)
			{
				uint32_t low = ((l << 1) | lp) << 1;
				low |= (low >> 7);
				for (uint32_t h = 0; h < 64; h++)
				{
					uint32_t high = ((h << 1) | lp) << 1;
					high |= (high >> 7);
					const int k = (low * (64 - g_bc7_weights3[BC7ENC_MODE_1_OPTIMAL_INDEX]) + high * g_bc7_weights3[BC7ENC_MODE_1_OPTIMAL_INDEX] + 32) >> 6;
					const int err = (k - c) * (k - c);
					if (err < best.m_error)
					{
						best.m_error = (uint16_t)err;
						best.m_lo = (uint8_t)l;
						best.m_hi = (uint8_t)h;
					}
				} // h
			} // l
			g_bc7_mode_1_optimal_endpoints[c][lp] = best;
		} // lp
	} // c

	// ASTC [0,15] 3-bit
	for (int c = 0; c < 256; c++)
	{
		endpoint_err best;
		best.m_error = (uint16_t)UINT16_MAX;
		for (uint32_t l = 0; l < 16; l++)
		{
			uint32_t low = (l << 4) | l;
			
			for (uint32_t h = 0; h < 16; h++)
			{
				uint32_t high = (h << 4) | h;
				
				const int k = astc_interpolate_linear(low, high, g_bc7_weights3[BC7ENC_ASTC_4BIT_3BIT_OPTIMAL_INDEX]);
				const int err = (k - c) * (k - c);

				if (err < best.m_error)
				{
					best.m_error = (uint16_t)err;
					best.m_lo = (uint8_t)l;
					best.m_hi = (uint8_t)h;
				}
			} // h
		} // l
		
		g_astc_4bit_3bit_optimal_endpoints[c] = best;
		
	} // c

	// ASTC [0,15] 2-bit
	for (int c = 0; c < 256; c++)
	{
		endpoint_err best;
		best.m_error = (uint16_t)UINT16_MAX;
		for (uint32_t l = 0; l < 16; l++)
		{
			uint32_t low = (l << 4) | l;
			
			for (uint32_t h = 0; h < 16; h++)
			{
				uint32_t high = (h << 4) | h;
				
				const int k = astc_interpolate_linear(low, high, g_bc7_weights2[BC7ENC_ASTC_4BIT_2BIT_OPTIMAL_INDEX]);
				const int err = (k - c) * (k - c);

				if (err < best.m_error)
				{
					best.m_error = (uint16_t)err;
					best.m_lo = (uint8_t)l;
					best.m_hi = (uint8_t)h;
				}
			} // h
		} // l
		
		g_astc_4bit_2bit_optimal_endpoints[c] = best;
		
	} // c

	// ASTC range 7 [0,11] 2-bit
	for (int c = 0; c < 256; c++)
	{
		endpoint_err best;
		best.m_error = (uint16_t)UINT16_MAX;
		for (uint32_t l = 0; l < 12; l++)
		{
			uint32_t low = g_astc_sorted_order_unquant[7][l].m_unquant;
			
			for (uint32_t h = 0; h < 12; h++)
			{
				uint32_t high = g_astc_sorted_order_unquant[7][h].m_unquant;
				
				const int k = astc_interpolate_linear(low, high, g_bc7_weights2[BC7ENC_ASTC_RANGE7_2BIT_OPTIMAL_INDEX]);
				const int err = (k - c) * (k - c);

				if (err < best.m_error)
				{
					best.m_error = (uint16_t)err;
					best.m_lo = (uint8_t)l;
					best.m_hi = (uint8_t)h;
				}
			} // h
		} // l
		
		g_astc_range7_2bit_optimal_endpoints[c] = best;
		
	} // c

	// ASTC range 13 [0,47] 4-bit
	for (int c = 0; c < 256; c++)
	{
		endpoint_err best;
		best.m_error = (uint16_t)UINT16_MAX;
		for (uint32_t l = 0; l < 48; l++)
		{
			uint32_t low = g_astc_sorted_order_unquant[13][l].m_unquant;
			
			for (uint32_t h = 0; h < 48; h++)
			{
				uint32_t high = g_astc_sorted_order_unquant[13][h].m_unquant;
				
				const int k = astc_interpolate_linear(low, high, g_astc_weights4[BC7ENC_ASTC_RANGE13_4BIT_OPTIMAL_INDEX]);
				const int err = (k - c) * (k - c);

				if (err < best.m_error)
				{
					best.m_error = (uint16_t)err;
					best.m_lo = (uint8_t)l;
					best.m_hi = (uint8_t)h;
				}
			} // h
		} // l
		
		g_astc_range13_4bit_optimal_endpoints[c] = best;
		
	} // c

	// ASTC range 13 [0,47] 2-bit
	for (int c = 0; c < 256; c++)
	{
		endpoint_err best;
		best.m_error = (uint16_t)UINT16_MAX;
		for (uint32_t l = 0; l < 48; l++)
		{
			uint32_t low = g_astc_sorted_order_unquant[13][l].m_unquant;
			
			for (uint32_t h = 0; h < 48; h++)
			{
				uint32_t high = g_astc_sorted_order_unquant[13][h].m_unquant;
				
				const int k = astc_interpolate_linear(low, high, g_bc7_weights2[BC7ENC_ASTC_RANGE13_2BIT_OPTIMAL_INDEX]);
				const int err = (k - c) * (k - c);

				if (err < best.m_error)
				{
					best.m_error = (uint16_t)err;
					best.m_lo = (uint8_t)l;
					best.m_hi = (uint8_t)h;
				}
			} // h
		} // l
		
		g_astc_range13_2bit_optimal_endpoints[c] = best;
		
	} // c

	// ASTC range 11 [0,31] 5-bit
	for (int c = 0; c < 256; c++)
	{
		endpoint_err best;
		best.m_error = (uint16_t)UINT16_MAX;
		for (uint32_t l = 0; l < 32; l++)
		{
			uint32_t low = g_astc_sorted_order_unquant[11][l].m_unquant;

			for (uint32_t h = 0; h < 32; h++)
			{
				uint32_t high = g_astc_sorted_order_unquant[11][h].m_unquant;

				const int k = astc_interpolate_linear(low, high, g_astc_weights5[BC7ENC_ASTC_RANGE11_5BIT_OPTIMAL_INDEX]);
				const int err = (k - c) * (k - c);

				if (err < best.m_error)
				{
					best.m_error = (uint16_t)err;
					best.m_lo = (uint8_t)l;
					best.m_hi = (uint8_t)h;
				}
			} // h
		} // l

		g_astc_range11_5bit_optimal_endpoints[c] = best;

	} // c
}

static void compute_least_squares_endpoints_rgba(uint32_t N, const uint8_t *pSelectors, const bc7enc_vec4F* pSelector_weights, bc7enc_vec4F* pXl, bc7enc_vec4F* pXh, const color_quad_u8 *pColors)
{
	// Least squares using normal equations: http://www.cs.cornell.edu/~bindel/class/cs3220-s12/notes/lec10.pdf 
	// https://web.archive.org/web/20150319232457/http://www.cs.cornell.edu/~bindel/class/cs3220-s12/notes/lec10.pdf
	// I did this in matrix form first, expanded out all the ops, then optimized it a bit.
	double z00 = 0.0f, z01 = 0.0f, z10 = 0.0f, z11 = 0.0f;
	double q00_r = 0.0f, q10_r = 0.0f, t_r = 0.0f;
	double q00_g = 0.0f, q10_g = 0.0f, t_g = 0.0f;
	double q00_b = 0.0f, q10_b = 0.0f, t_b = 0.0f;
	double q00_a = 0.0f, q10_a = 0.0f, t_a = 0.0f;
	
	for (uint32_t i = 0; i < N; i++)
	{
		const uint32_t sel = pSelectors[i];
		z00 += pSelector_weights[sel].m_c[0];
		z10 += pSelector_weights[sel].m_c[1];
		z11 += pSelector_weights[sel].m_c[2];
		float w = pSelector_weights[sel].m_c[3];
		q00_r += w * pColors[i].m_c[0]; t_r += pColors[i].m_c[0];
		q00_g += w * pColors[i].m_c[1]; t_g += pColors[i].m_c[1];
		q00_b += w * pColors[i].m_c[2]; t_b += pColors[i].m_c[2];
		q00_a += w * pColors[i].m_c[3]; t_a += pColors[i].m_c[3];
	}

	q10_r = t_r - q00_r;
	q10_g = t_g - q00_g;
	q10_b = t_b - q00_b;
	q10_a = t_a - q00_a;

	z01 = z10;

	double det = z00 * z11 - z01 * z10;
	if (det != 0.0f)
		det = 1.0f / det;

	double iz00, iz01, iz10, iz11;
	iz00 = z11 * det;
	iz01 = -z01 * det;
	iz10 = -z10 * det;
	iz11 = z00 * det;

	pXl->m_c[0] = (float)(iz00 * q00_r + iz01 * q10_r); pXh->m_c[0] = (float)(iz10 * q00_r + iz11 * q10_r);
	pXl->m_c[1] = (float)(iz00 * q00_g + iz01 * q10_g); pXh->m_c[1] = (float)(iz10 * q00_g + iz11 * q10_g);
	pXl->m_c[2] = (float)(iz00 * q00_b + iz01 * q10_b); pXh->m_c[2] = (float)(iz10 * q00_b + iz11 * q10_b);
	pXl->m_c[3] = (float)(iz00 * q00_a + iz01 * q10_a); pXh->m_c[3] = (float)(iz10 * q00_a + iz11 * q10_a);

	for (uint32_t c = 0; c < 4; c++)
	{
		if ((pXl->m_c[c] < 0.0f) || (pXh->m_c[c] > 255.0f))
		{
			uint32_t lo_v = UINT32_MAX, hi_v = 0;
			for (uint32_t i = 0; i < N; i++)
			{
				lo_v = minimumu(lo_v, pColors[i].m_c[c]);
				hi_v = maximumu(hi_v, pColors[i].m_c[c]);
			}

			if (lo_v == hi_v)
			{
				pXl->m_c[c] = (float)lo_v;
				pXh->m_c[c] = (float)hi_v;
			}
		}
	}
}

static void compute_least_squares_endpoints_rgb(uint32_t N, const uint8_t *pSelectors, const bc7enc_vec4F*pSelector_weights, bc7enc_vec4F*pXl, bc7enc_vec4F*pXh, const color_quad_u8 *pColors)
{
	double z00 = 0.0f, z01 = 0.0f, z10 = 0.0f, z11 = 0.0f;
	double q00_r = 0.0f, q10_r = 0.0f, t_r = 0.0f;
	double q00_g = 0.0f, q10_g = 0.0f, t_g = 0.0f;
	double q00_b = 0.0f, q10_b = 0.0f, t_b = 0.0f;

	for (uint32_t i = 0; i < N; i++)
	{
		const uint32_t sel = pSelectors[i];
		z00 += pSelector_weights[sel].m_c[0];
		z10 += pSelector_weights[sel].m_c[1];
		z11 += pSelector_weights[sel].m_c[2];
		float w = pSelector_weights[sel].m_c[3];
		q00_r += w * pColors[i].m_c[0]; t_r += pColors[i].m_c[0];
		q00_g += w * pColors[i].m_c[1]; t_g += pColors[i].m_c[1];
		q00_b += w * pColors[i].m_c[2]; t_b += pColors[i].m_c[2];
	}

	q10_r = t_r - q00_r;
	q10_g = t_g - q00_g;
	q10_b = t_b - q00_b;

	z01 = z10;

	double det = z00 * z11 - z01 * z10;
	if (det != 0.0f)
		det = 1.0f / det;

	double iz00, iz01, iz10, iz11;
	iz00 = z11 * det;
	iz01 = -z01 * det;
	iz10 = -z10 * det;
	iz11 = z00 * det;

	pXl->m_c[0] = (float)(iz00 * q00_r + iz01 * q10_r); pXh->m_c[0] = (float)(iz10 * q00_r + iz11 * q10_r);
	pXl->m_c[1] = (float)(iz00 * q00_g + iz01 * q10_g); pXh->m_c[1] = (float)(iz10 * q00_g + iz11 * q10_g);
	pXl->m_c[2] = (float)(iz00 * q00_b + iz01 * q10_b); pXh->m_c[2] = (float)(iz10 * q00_b + iz11 * q10_b);
	pXl->m_c[3] = 255.0f; pXh->m_c[3] = 255.0f;

	for (uint32_t c = 0; c < 3; c++)
	{
		if ((pXl->m_c[c] < 0.0f) || (pXh->m_c[c] > 255.0f))
		{
			uint32_t lo_v = UINT32_MAX, hi_v = 0;
			for (uint32_t i = 0; i < N; i++)
			{
				lo_v = minimumu(lo_v, pColors[i].m_c[c]);
				hi_v = maximumu(hi_v, pColors[i].m_c[c]);
			}

			if (lo_v == hi_v)
			{
				pXl->m_c[c] = (float)lo_v;
				pXh->m_c[c] = (float)hi_v;
			}
		}
	}
}

static inline color_quad_u8 scale_color(const color_quad_u8* pC, const color_cell_compressor_params* pParams)
{
	color_quad_u8 results;

	if (pParams->m_astc_endpoint_range)
	{
		for (uint32_t i = 0; i < 4; i++)
		{
			results.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pC->m_c[i]].m_unquant;
		}
	}
	else
	{
		const uint32_t n = pParams->m_comp_bits + (pParams->m_has_pbits ? 1 : 0);
		assert((n >= 4) && (n <= 8));

		for (uint32_t i = 0; i < 4; i++)
		{
			uint32_t v = pC->m_c[i] << (8 - n);
			v |= (v >> n);
			assert(v <= 255);
			results.m_c[i] = (uint8_t)(v);
		}
	}

	return results;
}

static inline uint64_t compute_color_distance_rgb(const color_quad_u8 *pE1, const color_quad_u8 *pE2, bc7enc_bool perceptual, const uint32_t weights[4])
{
	int dr, dg, db;

	if (perceptual)
	{
		const int l1 = pE1->m_c[0] * 109 + pE1->m_c[1] * 366 + pE1->m_c[2] * 37;
		const int cr1 = ((int)pE1->m_c[0] << 9) - l1;
		const int cb1 = ((int)pE1->m_c[2] << 9) - l1;
		const int l2 = pE2->m_c[0] * 109 + pE2->m_c[1] * 366 + pE2->m_c[2] * 37;
		const int cr2 = ((int)pE2->m_c[0] << 9) - l2;
		const int cb2 = ((int)pE2->m_c[2] << 9) - l2;
		dr = (l1 - l2) >> 8;
		dg = (cr1 - cr2) >> 8;
		db = (cb1 - cb2) >> 8;
	}
	else
	{
		dr = (int)pE1->m_c[0] - (int)pE2->m_c[0];
		dg = (int)pE1->m_c[1] - (int)pE2->m_c[1];
		db = (int)pE1->m_c[2] - (int)pE2->m_c[2];
	}

	return weights[0] * (uint32_t)(dr * dr) + weights[1] * (uint32_t)(dg * dg) + weights[2] * (uint32_t)(db * db);
}

static inline uint64_t compute_color_distance_rgba(const color_quad_u8 *pE1, const color_quad_u8 *pE2, bc7enc_bool perceptual, const uint32_t weights[4])
{
	int da = (int)pE1->m_c[3] - (int)pE2->m_c[3];
	return compute_color_distance_rgb(pE1, pE2, perceptual, weights) + (weights[3] * (uint32_t)(da * da));
}

static uint64_t pack_mode1_to_one_color(const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults, uint32_t r, uint32_t g, uint32_t b, uint8_t *pSelectors)
{
	uint32_t best_err = UINT_MAX;
	uint32_t best_p = 0;

	for (uint32_t p = 0; p < 2; p++)
	{
		uint32_t err = g_bc7_mode_1_optimal_endpoints[r][p].m_error + g_bc7_mode_1_optimal_endpoints[g][p].m_error + g_bc7_mode_1_optimal_endpoints[b][p].m_error;
		if (err < best_err)
		{
			best_err = err;
			best_p = p;
		}
	}

	const endpoint_err *pEr = &g_bc7_mode_1_optimal_endpoints[r][best_p];
	const endpoint_err *pEg = &g_bc7_mode_1_optimal_endpoints[g][best_p];
	const endpoint_err *pEb = &g_bc7_mode_1_optimal_endpoints[b][best_p];

	color_quad_u8_set(&pResults->m_low_endpoint, pEr->m_lo, pEg->m_lo, pEb->m_lo, 0);
	color_quad_u8_set(&pResults->m_high_endpoint, pEr->m_hi, pEg->m_hi, pEb->m_hi, 0);
	pResults->m_pbits[0] = best_p;
	pResults->m_pbits[1] = 0;

	memset(pSelectors, BC7ENC_MODE_1_OPTIMAL_INDEX, pParams->m_num_pixels);

	color_quad_u8 p;
	for (uint32_t i = 0; i < 3; i++)
	{
		uint32_t low = ((pResults->m_low_endpoint.m_c[i] << 1) | pResults->m_pbits[0]) << 1;
		low |= (low >> 7);

		uint32_t high = ((pResults->m_high_endpoint.m_c[i] << 1) | pResults->m_pbits[0]) << 1;
		high |= (high >> 7);

		p.m_c[i] = (uint8_t)((low * (64 - g_bc7_weights3[BC7ENC_MODE_1_OPTIMAL_INDEX]) + high * g_bc7_weights3[BC7ENC_MODE_1_OPTIMAL_INDEX] + 32) >> 6);
	}
	p.m_c[3] = 255;

	uint64_t total_err = 0;
	for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		total_err += compute_color_distance_rgb(&p, &pParams->m_pPixels[i], pParams->m_perceptual, pParams->m_weights);

	pResults->m_best_overall_err = total_err;

	return total_err;
}

static uint64_t pack_astc_4bit_3bit_to_one_color(const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults, uint32_t r, uint32_t g, uint32_t b, uint8_t *pSelectors)
{
	const endpoint_err *pEr = &g_astc_4bit_3bit_optimal_endpoints[r];
	const endpoint_err *pEg = &g_astc_4bit_3bit_optimal_endpoints[g];
	const endpoint_err *pEb = &g_astc_4bit_3bit_optimal_endpoints[b];

	color_quad_u8_set(&pResults->m_low_endpoint, pEr->m_lo, pEg->m_lo, pEb->m_lo, 0);
	color_quad_u8_set(&pResults->m_high_endpoint, pEr->m_hi, pEg->m_hi, pEb->m_hi, 0);
	pResults->m_pbits[0] = 0;
	pResults->m_pbits[1] = 0;

	for (uint32_t i = 0; i < 4; i++)
	{
		pResults->m_astc_low_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_low_endpoint.m_c[i]].m_index;
		pResults->m_astc_high_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_high_endpoint.m_c[i]].m_index;
	}

	memset(pSelectors, BC7ENC_ASTC_4BIT_3BIT_OPTIMAL_INDEX, pParams->m_num_pixels);

	color_quad_u8 p;
	for (uint32_t i = 0; i < 3; i++)
	{
		uint32_t low = (pResults->m_low_endpoint.m_c[i] << 4) | pResults->m_low_endpoint.m_c[i];
		uint32_t high = (pResults->m_high_endpoint.m_c[i] << 4) | pResults->m_high_endpoint.m_c[i];
		
		p.m_c[i] = (uint8_t)astc_interpolate_linear(low, high, g_bc7_weights3[BC7ENC_ASTC_4BIT_3BIT_OPTIMAL_INDEX]);
	}
	p.m_c[3] = 255;

	uint64_t total_err = 0;
	for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		total_err += compute_color_distance_rgb(&p, &pParams->m_pPixels[i], pParams->m_perceptual, pParams->m_weights);

	pResults->m_best_overall_err = total_err;

	return total_err;
}

static uint64_t pack_astc_4bit_2bit_to_one_color_rgba(const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults, uint32_t r, uint32_t g, uint32_t b, uint32_t a, uint8_t *pSelectors)
{
	const endpoint_err *pEr = &g_astc_4bit_2bit_optimal_endpoints[r];
	const endpoint_err *pEg = &g_astc_4bit_2bit_optimal_endpoints[g];
	const endpoint_err *pEb = &g_astc_4bit_2bit_optimal_endpoints[b];
	const endpoint_err *pEa = &g_astc_4bit_2bit_optimal_endpoints[a];

	color_quad_u8_set(&pResults->m_low_endpoint, pEr->m_lo, pEg->m_lo, pEb->m_lo, pEa->m_lo);
	color_quad_u8_set(&pResults->m_high_endpoint, pEr->m_hi, pEg->m_hi, pEb->m_hi, pEa->m_hi);
	pResults->m_pbits[0] = 0;
	pResults->m_pbits[1] = 0;

	for (uint32_t i = 0; i < 4; i++)
	{
		pResults->m_astc_low_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_low_endpoint.m_c[i]].m_index;
		pResults->m_astc_high_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_high_endpoint.m_c[i]].m_index;
	}

	memset(pSelectors, BC7ENC_ASTC_4BIT_2BIT_OPTIMAL_INDEX, pParams->m_num_pixels);

	color_quad_u8 p;
	for (uint32_t i = 0; i < 4; i++)
	{
		uint32_t low = (pResults->m_low_endpoint.m_c[i] << 4) | pResults->m_low_endpoint.m_c[i];
		uint32_t high = (pResults->m_high_endpoint.m_c[i] << 4) | pResults->m_high_endpoint.m_c[i];
		
		p.m_c[i] = (uint8_t)astc_interpolate_linear(low, high, g_bc7_weights2[BC7ENC_ASTC_4BIT_2BIT_OPTIMAL_INDEX]);
	}
	
	uint64_t total_err = 0;
	for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		total_err += compute_color_distance_rgba(&p, &pParams->m_pPixels[i], pParams->m_perceptual, pParams->m_weights);

	pResults->m_best_overall_err = total_err;

	return total_err;
}

static uint64_t pack_astc_range7_2bit_to_one_color(const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults, uint32_t r, uint32_t g, uint32_t b, uint8_t *pSelectors)
{
	assert(pParams->m_astc_endpoint_range == 7 && pParams->m_num_selector_weights == 4);

	const endpoint_err *pEr = &g_astc_range7_2bit_optimal_endpoints[r];
	const endpoint_err *pEg = &g_astc_range7_2bit_optimal_endpoints[g];
	const endpoint_err *pEb = &g_astc_range7_2bit_optimal_endpoints[b];

	color_quad_u8_set(&pResults->m_low_endpoint, pEr->m_lo, pEg->m_lo, pEb->m_lo, 0);
	color_quad_u8_set(&pResults->m_high_endpoint, pEr->m_hi, pEg->m_hi, pEb->m_hi, 0);
	pResults->m_pbits[0] = 0;
	pResults->m_pbits[1] = 0;

	for (uint32_t i = 0; i < 4; i++)
	{
		pResults->m_astc_low_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_low_endpoint.m_c[i]].m_index;
		pResults->m_astc_high_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_high_endpoint.m_c[i]].m_index;
	}

	memset(pSelectors, BC7ENC_ASTC_RANGE7_2BIT_OPTIMAL_INDEX, pParams->m_num_pixels);

	color_quad_u8 p;
	for (uint32_t i = 0; i < 3; i++)
	{
		uint32_t low = g_astc_sorted_order_unquant[7][pResults->m_low_endpoint.m_c[i]].m_unquant;
		uint32_t high = g_astc_sorted_order_unquant[7][pResults->m_high_endpoint.m_c[i]].m_unquant;
		
		p.m_c[i] = (uint8_t)astc_interpolate_linear(low, high, g_bc7_weights2[BC7ENC_ASTC_RANGE7_2BIT_OPTIMAL_INDEX]);
	}
	p.m_c[3] = 255;

	uint64_t total_err = 0;
	for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		total_err += compute_color_distance_rgb(&p, &pParams->m_pPixels[i], pParams->m_perceptual, pParams->m_weights);

	pResults->m_best_overall_err = total_err;

	return total_err;
}

static uint64_t pack_astc_range13_2bit_to_one_color(const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults, uint32_t r, uint32_t g, uint32_t b, uint8_t *pSelectors)
{
	assert(pParams->m_astc_endpoint_range == 13 && pParams->m_num_selector_weights == 4 && !pParams->m_has_alpha);

	const endpoint_err *pEr = &g_astc_range13_2bit_optimal_endpoints[r];
	const endpoint_err *pEg = &g_astc_range13_2bit_optimal_endpoints[g];
	const endpoint_err *pEb = &g_astc_range13_2bit_optimal_endpoints[b];
	
	color_quad_u8_set(&pResults->m_low_endpoint, pEr->m_lo, pEg->m_lo, pEb->m_lo, 47);
	color_quad_u8_set(&pResults->m_high_endpoint, pEr->m_hi, pEg->m_hi, pEb->m_hi, 47);
	pResults->m_pbits[0] = 0;
	pResults->m_pbits[1] = 0;

	for (uint32_t i = 0; i < 4; i++)
	{
		pResults->m_astc_low_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_low_endpoint.m_c[i]].m_index;
		pResults->m_astc_high_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_high_endpoint.m_c[i]].m_index;
	}

	memset(pSelectors, BC7ENC_ASTC_RANGE13_2BIT_OPTIMAL_INDEX, pParams->m_num_pixels);

	color_quad_u8 p;
	for (uint32_t i = 0; i < 4; i++)
	{
		uint32_t low = g_astc_sorted_order_unquant[13][pResults->m_low_endpoint.m_c[i]].m_unquant;
		uint32_t high = g_astc_sorted_order_unquant[13][pResults->m_high_endpoint.m_c[i]].m_unquant;
		
		p.m_c[i] = (uint8_t)astc_interpolate_linear(low, high, g_bc7_weights2[BC7ENC_ASTC_RANGE13_2BIT_OPTIMAL_INDEX]);
	}
	
	uint64_t total_err = 0;
	for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		total_err += compute_color_distance_rgb(&p, &pParams->m_pPixels[i], pParams->m_perceptual, pParams->m_weights);

	pResults->m_best_overall_err = total_err;

	return total_err;
}

static uint64_t pack_astc_range11_5bit_to_one_color(const color_cell_compressor_params* pParams, color_cell_compressor_results* pResults, uint32_t r, uint32_t g, uint32_t b, uint8_t* pSelectors)
{
	assert(pParams->m_astc_endpoint_range == 11 && pParams->m_num_selector_weights == 32 && !pParams->m_has_alpha);

	const endpoint_err* pEr = &g_astc_range11_5bit_optimal_endpoints[r];
	const endpoint_err* pEg = &g_astc_range11_5bit_optimal_endpoints[g];
	const endpoint_err* pEb = &g_astc_range11_5bit_optimal_endpoints[b];

	color_quad_u8_set(&pResults->m_low_endpoint, pEr->m_lo, pEg->m_lo, pEb->m_lo, 31);
	color_quad_u8_set(&pResults->m_high_endpoint, pEr->m_hi, pEg->m_hi, pEb->m_hi, 31);
	pResults->m_pbits[0] = 0;
	pResults->m_pbits[1] = 0;

	for (uint32_t i = 0; i < 4; i++)
	{
		pResults->m_astc_low_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_low_endpoint.m_c[i]].m_index;
		pResults->m_astc_high_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_high_endpoint.m_c[i]].m_index;
	}

	memset(pSelectors, BC7ENC_ASTC_RANGE11_5BIT_OPTIMAL_INDEX, pParams->m_num_pixels);

	color_quad_u8 p;
	for (uint32_t i = 0; i < 4; i++)
	{
		uint32_t low = g_astc_sorted_order_unquant[11][pResults->m_low_endpoint.m_c[i]].m_unquant;
		uint32_t high = g_astc_sorted_order_unquant[11][pResults->m_high_endpoint.m_c[i]].m_unquant;

		p.m_c[i] = (uint8_t)astc_interpolate_linear(low, high, g_astc_weights5[BC7ENC_ASTC_RANGE11_5BIT_OPTIMAL_INDEX]);
	}

	uint64_t total_err = 0;
	for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		total_err += compute_color_distance_rgb(&p, &pParams->m_pPixels[i], pParams->m_perceptual, pParams->m_weights);

	pResults->m_best_overall_err = total_err;

	return total_err;
}

static uint64_t evaluate_solution(const color_quad_u8 *pLow, const color_quad_u8 *pHigh, const uint32_t pbits[2], const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults)
{
	color_quad_u8 quantMinColor = *pLow;
	color_quad_u8 quantMaxColor = *pHigh;

	if (pParams->m_has_pbits)
	{
		uint32_t minPBit, maxPBit;

		if (pParams->m_endpoints_share_pbit)
			maxPBit = minPBit = pbits[0];
		else
		{
			minPBit = pbits[0];
			maxPBit = pbits[1];
		}

		quantMinColor.m_c[0] = (uint8_t)((pLow->m_c[0] << 1) | minPBit);
		quantMinColor.m_c[1] = (uint8_t)((pLow->m_c[1] << 1) | minPBit);
		quantMinColor.m_c[2] = (uint8_t)((pLow->m_c[2] << 1) | minPBit);
		quantMinColor.m_c[3] = (uint8_t)((pLow->m_c[3] << 1) | minPBit);

		quantMaxColor.m_c[0] = (uint8_t)((pHigh->m_c[0] << 1) | maxPBit);
		quantMaxColor.m_c[1] = (uint8_t)((pHigh->m_c[1] << 1) | maxPBit);
		quantMaxColor.m_c[2] = (uint8_t)((pHigh->m_c[2] << 1) | maxPBit);
		quantMaxColor.m_c[3] = (uint8_t)((pHigh->m_c[3] << 1) | maxPBit);
	}

	color_quad_u8 actualMinColor = scale_color(&quantMinColor, pParams);
	color_quad_u8 actualMaxColor = scale_color(&quantMaxColor, pParams);

	const uint32_t N = pParams->m_num_selector_weights;
	assert(N >= 1 && N <= 32);

	color_quad_u8 weightedColors[32];
	weightedColors[0] = actualMinColor;
	weightedColors[N - 1] = actualMaxColor;

	const uint32_t nc = pParams->m_has_alpha ? 4 : 3;
	if (pParams->m_astc_endpoint_range)
	{
		for (uint32_t i = 1; i < (N - 1); i++)
		{
			for (uint32_t j = 0; j < nc; j++)
				weightedColors[i].m_c[j] = (uint8_t)(astc_interpolate_linear(actualMinColor.m_c[j], actualMaxColor.m_c[j], pParams->m_pSelector_weights[i]));
		}
	}
	else
	{
		for (uint32_t i = 1; i < (N - 1); i++)
			for (uint32_t j = 0; j < nc; j++)
				weightedColors[i].m_c[j] = (uint8_t)((actualMinColor.m_c[j] * (64 - pParams->m_pSelector_weights[i]) + actualMaxColor.m_c[j] * pParams->m_pSelector_weights[i] + 32) >> 6);
	}

	const int lr = actualMinColor.m_c[0];
	const int lg = actualMinColor.m_c[1];
	const int lb = actualMinColor.m_c[2];
	const int dr = actualMaxColor.m_c[0] - lr;
	const int dg = actualMaxColor.m_c[1] - lg;
	const int db = actualMaxColor.m_c[2] - lb;
	
	uint64_t total_err = 0;
	
	if (pParams->m_pForce_selectors)
	{
		for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		{
			const color_quad_u8* pC = &pParams->m_pPixels[i];
			
			const uint8_t sel = pParams->m_pForce_selectors[i];
			assert(sel < N);
			
			total_err += (pParams->m_has_alpha ? compute_color_distance_rgba : compute_color_distance_rgb)(&weightedColors[sel], pC, pParams->m_perceptual, pParams->m_weights);

			pResults->m_pSelectors_temp[i] = sel;
		}
	}
	else if (!pParams->m_perceptual)
	{
		if (pParams->m_has_alpha)
		{
			const int la = actualMinColor.m_c[3];
			const int da = actualMaxColor.m_c[3] - la;

			const float f = N / (float)(squarei(dr) + squarei(dg) + squarei(db) + squarei(da) + .00000125f);

			for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
			{
				const color_quad_u8 *pC = &pParams->m_pPixels[i];
				int r = pC->m_c[0];
				int g = pC->m_c[1];
				int b = pC->m_c[2];
				int a = pC->m_c[3];

				int best_sel = (int)((float)((r - lr) * dr + (g - lg) * dg + (b - lb) * db + (a - la) * da) * f + .5f);
				best_sel = clampi(best_sel, 1, N - 1);

				uint64_t err0 = compute_color_distance_rgba(&weightedColors[best_sel - 1], pC, BC7ENC_FALSE, pParams->m_weights);
				uint64_t err1 = compute_color_distance_rgba(&weightedColors[best_sel], pC, BC7ENC_FALSE, pParams->m_weights);

				if (err0 == err1)
				{
					// Prefer non-interpolation
					if ((best_sel - 1) == 0)
						best_sel = 0;
				}
				else if (err1 > err0)
				{
					err1 = err0;
					--best_sel;
				}
				total_err += err1;
								
				pResults->m_pSelectors_temp[i] = (uint8_t)best_sel;
			}
		}
		else
		{
			const float f = N / (float)(squarei(dr) + squarei(dg) + squarei(db) + .00000125f);

			for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
			{
				const color_quad_u8 *pC = &pParams->m_pPixels[i];
				int r = pC->m_c[0];
				int g = pC->m_c[1];
				int b = pC->m_c[2];

				int sel = (int)((float)((r - lr) * dr + (g - lg) * dg + (b - lb) * db) * f + .5f);
				sel = clampi(sel, 1, N - 1);

				uint64_t err0 = compute_color_distance_rgb(&weightedColors[sel - 1], pC, BC7ENC_FALSE, pParams->m_weights);
				uint64_t err1 = compute_color_distance_rgb(&weightedColors[sel], pC, BC7ENC_FALSE, pParams->m_weights);

				int best_sel = sel;
				uint64_t best_err = err1;
				if (err0 == err1)
				{
					// Prefer non-interpolation
					if ((best_sel - 1) == 0)
						best_sel = 0;
				}
				else if (err0 < best_err)
				{
					best_err = err0;
					best_sel = sel - 1;
				}

				total_err += best_err;

				pResults->m_pSelectors_temp[i] = (uint8_t)best_sel;
			}
		}
	}
	else
	{
		for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		{
			uint64_t best_err = UINT64_MAX;
			uint32_t best_sel = 0;

			if (pParams->m_has_alpha)
			{
				for (uint32_t j = 0; j < N; j++)
				{
					uint64_t err = compute_color_distance_rgba(&weightedColors[j], &pParams->m_pPixels[i], BC7ENC_TRUE, pParams->m_weights);
					if (err < best_err)
					{
						best_err = err;
						best_sel = j;
					}
					// Prefer non-interpolation
					else if ((err == best_err) && (j == (N - 1)))
						best_sel = j;
				}
			}
			else
			{
				for (uint32_t j = 0; j < N; j++)
				{
					uint64_t err = compute_color_distance_rgb(&weightedColors[j], &pParams->m_pPixels[i], BC7ENC_TRUE, pParams->m_weights);
					if (err < best_err)
					{
						best_err = err;
						best_sel = j;
					}
					// Prefer non-interpolation
					else if ((err == best_err) && (j == (N - 1)))
						best_sel = j;
				}
			}

			total_err += best_err;

			pResults->m_pSelectors_temp[i] = (uint8_t)best_sel;
		}
	}

	if (total_err < pResults->m_best_overall_err)
	{
		pResults->m_best_overall_err = total_err;

		pResults->m_low_endpoint = *pLow;
		pResults->m_high_endpoint = *pHigh;

		pResults->m_pbits[0] = pbits[0];
		pResults->m_pbits[1] = pbits[1];

		memcpy(pResults->m_pSelectors, pResults->m_pSelectors_temp, sizeof(pResults->m_pSelectors[0]) * pParams->m_num_pixels);
	}
				
	return total_err;
}

static bool areDegenerateEndpoints(color_quad_u8* pTrialMinColor, color_quad_u8* pTrialMaxColor, const bc7enc_vec4F* pXl, const bc7enc_vec4F* pXh)
{
	for (uint32_t i = 0; i < 3; i++)
	{
		if (pTrialMinColor->m_c[i] == pTrialMaxColor->m_c[i])
		{
			if (fabs(pXl->m_c[i] - pXh->m_c[i]) > 0.0f)
				return true;
		}
	}

	return false;
}

static void fixDegenerateEndpoints(uint32_t mode, color_quad_u8 *pTrialMinColor, color_quad_u8 *pTrialMaxColor, const bc7enc_vec4F*pXl, const bc7enc_vec4F*pXh, uint32_t iscale, int flags)
{
	if (mode == 255)
	{
		for (uint32_t i = 0; i < 3; i++)
		{
			if (pTrialMinColor->m_c[i] == pTrialMaxColor->m_c[i])
			{
				if (fabs(pXl->m_c[i] - pXh->m_c[i]) > 0.000125f)
				{
					if (flags & 1)
					{
						if (pTrialMinColor->m_c[i] > 0)
							pTrialMinColor->m_c[i]--;
					}
					if (flags & 2)
					{
						if (pTrialMaxColor->m_c[i] < iscale)
							pTrialMaxColor->m_c[i]++;
					}
				}
			}
		}
	}
	else if (mode == 1)
	{
		// fix degenerate case where the input collapses to a single colorspace voxel, and we loose all freedom (test with grayscale ramps)
		for (uint32_t i = 0; i < 3; i++)
		{
			if (pTrialMinColor->m_c[i] == pTrialMaxColor->m_c[i])
			{
				if (fabs(pXl->m_c[i] - pXh->m_c[i]) > 0.000125f)
				{
					if (pTrialMinColor->m_c[i] > (iscale >> 1))
					{
						if (pTrialMinColor->m_c[i] > 0)
							pTrialMinColor->m_c[i]--;
						else
							if (pTrialMaxColor->m_c[i] < iscale)
								pTrialMaxColor->m_c[i]++;
					}
					else
					{
						if (pTrialMaxColor->m_c[i] < iscale)
							pTrialMaxColor->m_c[i]++;
						else if (pTrialMinColor->m_c[i] > 0)
							pTrialMinColor->m_c[i]--;
					}
				}
			}
		}
	}
}

static uint64_t find_optimal_solution(uint32_t mode, bc7enc_vec4F xl, bc7enc_vec4F xh, const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults)
{
	vec4F_saturate_in_place(&xl); vec4F_saturate_in_place(&xh);

	if (pParams->m_astc_endpoint_range)
	{
		const uint32_t levels = astc_get_levels(pParams->m_astc_endpoint_range);

		const float scale = 255.0f;

		color_quad_u8 trialMinColor8Bit, trialMaxColor8Bit;
		color_quad_u8_set_clamped(&trialMinColor8Bit, (int)(xl.m_c[0] * scale + .5f), (int)(xl.m_c[1] * scale + .5f), (int)(xl.m_c[2] * scale + .5f), (int)(xl.m_c[3] * scale + .5f));
		color_quad_u8_set_clamped(&trialMaxColor8Bit, (int)(xh.m_c[0] * scale + .5f), (int)(xh.m_c[1] * scale + .5f), (int)(xh.m_c[2] * scale + .5f), (int)(xh.m_c[3] * scale + .5f));

		color_quad_u8 trialMinColor, trialMaxColor;
		for (uint32_t i = 0; i < 4; i++)
		{
			trialMinColor.m_c[i] = g_astc_nearest_sorted_index[pParams->m_astc_endpoint_range][trialMinColor8Bit.m_c[i]];
			trialMaxColor.m_c[i] = g_astc_nearest_sorted_index[pParams->m_astc_endpoint_range][trialMaxColor8Bit.m_c[i]];
		}

		if (areDegenerateEndpoints(&trialMinColor, &trialMaxColor, &xl, &xh))
		{
			color_quad_u8 trialMinColorOrig(trialMinColor), trialMaxColorOrig(trialMaxColor);

			fixDegenerateEndpoints(mode, &trialMinColor, &trialMaxColor, &xl, &xh, levels - 1, 1);
			if ((pResults->m_best_overall_err == UINT64_MAX) || color_quad_u8_notequals(&trialMinColor, &pResults->m_low_endpoint) || color_quad_u8_notequals(&trialMaxColor, &pResults->m_high_endpoint))
				evaluate_solution(&trialMinColor, &trialMaxColor, pResults->m_pbits, pParams, pResults);

			trialMinColor = trialMinColorOrig;
			trialMaxColor = trialMaxColorOrig;
			fixDegenerateEndpoints(mode, &trialMinColor, &trialMaxColor, &xl, &xh, levels - 1, 0);
			if ((pResults->m_best_overall_err == UINT64_MAX) || color_quad_u8_notequals(&trialMinColor, &pResults->m_low_endpoint) || color_quad_u8_notequals(&trialMaxColor, &pResults->m_high_endpoint))
				evaluate_solution(&trialMinColor, &trialMaxColor, pResults->m_pbits, pParams, pResults);

			trialMinColor = trialMinColorOrig;
			trialMaxColor = trialMaxColorOrig;
			fixDegenerateEndpoints(mode, &trialMinColor, &trialMaxColor, &xl, &xh, levels - 1, 2);
			if ((pResults->m_best_overall_err == UINT64_MAX) || color_quad_u8_notequals(&trialMinColor, &pResults->m_low_endpoint) || color_quad_u8_notequals(&trialMaxColor, &pResults->m_high_endpoint))
				evaluate_solution(&trialMinColor, &trialMaxColor, pResults->m_pbits, pParams, pResults);

			trialMinColor = trialMinColorOrig;
			trialMaxColor = trialMaxColorOrig;
			fixDegenerateEndpoints(mode, &trialMinColor, &trialMaxColor, &xl, &xh, levels - 1, 3);
			if ((pResults->m_best_overall_err == UINT64_MAX) || color_quad_u8_notequals(&trialMinColor, &pResults->m_low_endpoint) || color_quad_u8_notequals(&trialMaxColor, &pResults->m_high_endpoint))
				evaluate_solution(&trialMinColor, &trialMaxColor, pResults->m_pbits, pParams, pResults);
		}
		else
		{
			if ((pResults->m_best_overall_err == UINT64_MAX) || color_quad_u8_notequals(&trialMinColor, &pResults->m_low_endpoint) || color_quad_u8_notequals(&trialMaxColor, &pResults->m_high_endpoint))
			{
				evaluate_solution(&trialMinColor, &trialMaxColor, pResults->m_pbits, pParams, pResults);
			}
		}

		for (uint32_t i = 0; i < 4; i++)
		{
			pResults->m_astc_low_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_low_endpoint.m_c[i]].m_index;
			pResults->m_astc_high_endpoint.m_c[i] = g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_high_endpoint.m_c[i]].m_index;
		}
	}
	else if (pParams->m_has_pbits)
	{
		const int iscalep = (1 << (pParams->m_comp_bits + 1)) - 1;
		const float scalep = (float)iscalep;

		const int32_t totalComps = pParams->m_has_alpha ? 4 : 3;

		uint32_t best_pbits[2];
		color_quad_u8 bestMinColor, bestMaxColor;

		if (!pParams->m_endpoints_share_pbit)
		{
			float best_err0 = 1e+9;
			float best_err1 = 1e+9;

			for (int p = 0; p < 2; p++)
			{
				color_quad_u8 xMinColor, xMaxColor;

				// Notes: The pbit controls which quantization intervals are selected.
				// total_levels=2^(comp_bits+1), where comp_bits=4 for mode 0, etc.
				// pbit 0: v=(b*2)/(total_levels-1), pbit 1: v=(b*2+1)/(total_levels-1) where b is the component bin from [0,total_levels/2-1] and v is the [0,1] component value
				// rearranging you get for pbit 0: b=floor(v*(total_levels-1)/2+.5)
				// rearranging you get for pbit 1: b=floor((v*(total_levels-1)-1)/2+.5)
				for (uint32_t c = 0; c < 4; c++)
				{
					xMinColor.m_c[c] = (uint8_t)(clampi(((int)((xl.m_c[c] * scalep - p) / 2.0f + .5f)) * 2 + p, p, iscalep - 1 + p));
					xMaxColor.m_c[c] = (uint8_t)(clampi(((int)((xh.m_c[c] * scalep - p) / 2.0f + .5f)) * 2 + p, p, iscalep - 1 + p));
				}

				color_quad_u8 scaledLow = scale_color(&xMinColor, pParams);
				color_quad_u8 scaledHigh = scale_color(&xMaxColor, pParams);

				float err0 = 0, err1 = 0;
				for (int i = 0; i < totalComps; i++)
				{
					err0 += squaref(scaledLow.m_c[i] - xl.m_c[i] * 255.0f);
					err1 += squaref(scaledHigh.m_c[i] - xh.m_c[i] * 255.0f);
				}

				if (err0 < best_err0)
				{
					best_err0 = err0;
					best_pbits[0] = p;

					bestMinColor.m_c[0] = xMinColor.m_c[0] >> 1;
					bestMinColor.m_c[1] = xMinColor.m_c[1] >> 1;
					bestMinColor.m_c[2] = xMinColor.m_c[2] >> 1;
					bestMinColor.m_c[3] = xMinColor.m_c[3] >> 1;
				}

				if (err1 < best_err1)
				{
					best_err1 = err1;
					best_pbits[1] = p;

					bestMaxColor.m_c[0] = xMaxColor.m_c[0] >> 1;
					bestMaxColor.m_c[1] = xMaxColor.m_c[1] >> 1;
					bestMaxColor.m_c[2] = xMaxColor.m_c[2] >> 1;
					bestMaxColor.m_c[3] = xMaxColor.m_c[3] >> 1;
				}
			}
		}
		else
		{
			// Endpoints share pbits
			float best_err = 1e+9;

			for (int p = 0; p < 2; p++)
			{
				color_quad_u8 xMinColor, xMaxColor;
				for (uint32_t c = 0; c < 4; c++)
				{
					xMinColor.m_c[c] = (uint8_t)(clampi(((int)((xl.m_c[c] * scalep - p) / 2.0f + .5f)) * 2 + p, p, iscalep - 1 + p));
					xMaxColor.m_c[c] = (uint8_t)(clampi(((int)((xh.m_c[c] * scalep - p) / 2.0f + .5f)) * 2 + p, p, iscalep - 1 + p));
				}

				color_quad_u8 scaledLow = scale_color(&xMinColor, pParams);
				color_quad_u8 scaledHigh = scale_color(&xMaxColor, pParams);

				float err = 0;
				for (int i = 0; i < totalComps; i++)
					err += squaref((scaledLow.m_c[i] / 255.0f) - xl.m_c[i]) + squaref((scaledHigh.m_c[i] / 255.0f) - xh.m_c[i]);

				if (err < best_err)
				{
					best_err = err;
					best_pbits[0] = p;
					best_pbits[1] = p;
					for (uint32_t j = 0; j < 4; j++)
					{
						bestMinColor.m_c[j] = xMinColor.m_c[j] >> 1;
						bestMaxColor.m_c[j] = xMaxColor.m_c[j] >> 1;
					}
				}
			}
		}
						
		fixDegenerateEndpoints(mode, &bestMinColor, &bestMaxColor, &xl, &xh, iscalep >> 1, 0);

		if ((pResults->m_best_overall_err == UINT64_MAX) || color_quad_u8_notequals(&bestMinColor, &pResults->m_low_endpoint) || color_quad_u8_notequals(&bestMaxColor, &pResults->m_high_endpoint) || (best_pbits[0] != pResults->m_pbits[0]) || (best_pbits[1] != pResults->m_pbits[1]))
			evaluate_solution(&bestMinColor, &bestMaxColor, best_pbits, pParams, pResults);
	}
	else
	{
		const int iscale = (1 << pParams->m_comp_bits) - 1;
		const float scale = (float)iscale;

		color_quad_u8 trialMinColor, trialMaxColor;
		color_quad_u8_set_clamped(&trialMinColor, (int)(xl.m_c[0] * scale + .5f), (int)(xl.m_c[1] * scale + .5f), (int)(xl.m_c[2] * scale + .5f), (int)(xl.m_c[3] * scale + .5f));
		color_quad_u8_set_clamped(&trialMaxColor, (int)(xh.m_c[0] * scale + .5f), (int)(xh.m_c[1] * scale + .5f), (int)(xh.m_c[2] * scale + .5f), (int)(xh.m_c[3] * scale + .5f));

		fixDegenerateEndpoints(mode, &trialMinColor, &trialMaxColor, &xl, &xh, iscale, 0);

		if ((pResults->m_best_overall_err == UINT64_MAX) || color_quad_u8_notequals(&trialMinColor, &pResults->m_low_endpoint) || color_quad_u8_notequals(&trialMaxColor, &pResults->m_high_endpoint))
			evaluate_solution(&trialMinColor, &trialMaxColor, pResults->m_pbits, pParams, pResults);
	}

	return pResults->m_best_overall_err;
}

void check_best_overall_error(const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults)
{
	const uint32_t n = pParams->m_num_selector_weights;

	assert(n <= 32);

	color_quad_u8 colors[32];
	for (uint32_t c = 0; c < 4; c++)
	{
		colors[0].m_c[c] = g_astc_unquant[pParams->m_astc_endpoint_range][pResults->m_astc_low_endpoint.m_c[c]].m_unquant;
		assert(colors[0].m_c[c] == g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_low_endpoint.m_c[c]].m_unquant);

		colors[n-1].m_c[c] = g_astc_unquant[pParams->m_astc_endpoint_range][pResults->m_astc_high_endpoint.m_c[c]].m_unquant;
		assert(colors[n-1].m_c[c] == g_astc_sorted_order_unquant[pParams->m_astc_endpoint_range][pResults->m_high_endpoint.m_c[c]].m_unquant);
	}
	
	for (uint32_t i = 1; i < pParams->m_num_selector_weights - 1; i++)
		for (uint32_t c = 0; c < 4; c++)
			colors[i].m_c[c] = (uint8_t)astc_interpolate_linear(colors[0].m_c[c], colors[n - 1].m_c[c], pParams->m_pSelector_weights[i]);

#ifdef _DEBUG
	uint64_t total_err = 0;
	for (uint32_t p = 0; p < pParams->m_num_pixels; p++)
	{
		const color_quad_u8 &orig = pParams->m_pPixels[p];
		const color_quad_u8 &packed = colors[pResults->m_pSelectors[p]];
				
		if (pParams->m_has_alpha)
			total_err += compute_color_distance_rgba(&orig, &packed, pParams->m_perceptual, pParams->m_weights);
		else
			total_err += compute_color_distance_rgb(&orig, &packed, pParams->m_perceptual, pParams->m_weights);
	}
	assert(total_err == pResults->m_best_overall_err);
#endif
	
	// HACK HACK
	//if (total_err != pResults->m_best_overall_err)
	//	printf("X");
}

static bool is_solid_rgb(const color_cell_compressor_params *pParams, uint32_t &r, uint32_t &g, uint32_t &b)
{
	r = pParams->m_pPixels[0].m_c[0];
	g = pParams->m_pPixels[0].m_c[1];
	b = pParams->m_pPixels[0].m_c[2];

	bool allSame = true;
	for (uint32_t i = 1; i < pParams->m_num_pixels; i++)
	{
		if ((r != pParams->m_pPixels[i].m_c[0]) || (g != pParams->m_pPixels[i].m_c[1]) || (b != pParams->m_pPixels[i].m_c[2]))
		{
			allSame = false;
			break;
		}
	}

	return allSame;
}

static bool is_solid_rgba(const color_cell_compressor_params *pParams, uint32_t &r, uint32_t &g, uint32_t &b, uint32_t &a)
{
	r = pParams->m_pPixels[0].m_c[0];
	g = pParams->m_pPixels[0].m_c[1];
	b = pParams->m_pPixels[0].m_c[2];
	a = pParams->m_pPixels[0].m_c[3];

	bool allSame = true;
	for (uint32_t i = 1; i < pParams->m_num_pixels; i++)
	{
		if ((r != pParams->m_pPixels[i].m_c[0]) || (g != pParams->m_pPixels[i].m_c[1]) || (b != pParams->m_pPixels[i].m_c[2]) || (a != pParams->m_pPixels[i].m_c[3]))
		{
			allSame = false;
			break;
		}
	}

	return allSame;
}

uint64_t color_cell_compression(uint32_t mode, const color_cell_compressor_params *pParams, color_cell_compressor_results *pResults, const bc7enc_compress_block_params *pComp_params)
{
	if (!pParams->m_astc_endpoint_range)
	{
		assert((mode == 6) || (!pParams->m_has_alpha));
	}
	assert(pParams->m_num_selector_weights >= 1 && pParams->m_num_selector_weights <= 32);
	assert(pParams->m_pSelector_weights[0] == 0);
	assert(pParams->m_pSelector_weights[pParams->m_num_selector_weights - 1] == 64);

	pResults->m_best_overall_err = UINT64_MAX;

	uint32_t cr, cg, cb, ca;

	// If the partition's colors are all the same, then just pack them as a single color.
	if (!pParams->m_pForce_selectors)
	{
		if (mode == 1)
		{
			if (is_solid_rgb(pParams, cr, cg, cb))
				return pack_mode1_to_one_color(pParams, pResults, cr, cg, cb, pResults->m_pSelectors);
		}
		else if ((pParams->m_astc_endpoint_range == 8) && (pParams->m_num_selector_weights == 8) && (!pParams->m_has_alpha))
		{
			if (is_solid_rgb(pParams, cr, cg, cb))
				return pack_astc_4bit_3bit_to_one_color(pParams, pResults, cr, cg, cb, pResults->m_pSelectors);
		}
		else if ((pParams->m_astc_endpoint_range == 7) && (pParams->m_num_selector_weights == 4) && (!pParams->m_has_alpha))
		{
			if (is_solid_rgb(pParams, cr, cg, cb))
				return pack_astc_range7_2bit_to_one_color(pParams, pResults, cr, cg, cb, pResults->m_pSelectors);
		}
		else if ((pParams->m_astc_endpoint_range == 8) && (pParams->m_num_selector_weights == 4) && (pParams->m_has_alpha))
		{
			if (is_solid_rgba(pParams, cr, cg, cb, ca))
				return pack_astc_4bit_2bit_to_one_color_rgba(pParams, pResults, cr, cg, cb, ca, pResults->m_pSelectors);
		}
		else if ((pParams->m_astc_endpoint_range == 13) && (pParams->m_num_selector_weights == 4) && (!pParams->m_has_alpha))
		{
			if (is_solid_rgb(pParams, cr, cg, cb))
				return pack_astc_range13_2bit_to_one_color(pParams, pResults, cr, cg, cb, pResults->m_pSelectors);
		}
		else if ((pParams->m_astc_endpoint_range == 11) && (pParams->m_num_selector_weights == 32) && (!pParams->m_has_alpha))
		{
			if (is_solid_rgb(pParams, cr, cg, cb))
				return pack_astc_range11_5bit_to_one_color(pParams, pResults, cr, cg, cb, pResults->m_pSelectors);
		}
	}

	// Compute partition's mean color and principle axis.
	bc7enc_vec4F meanColor, axis;
	vec4F_set_scalar(&meanColor, 0.0f);

	for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
	{
		bc7enc_vec4F color = vec4F_from_color(&pParams->m_pPixels[i]);
		meanColor = vec4F_add(&meanColor, &color);
	}
				
	bc7enc_vec4F meanColorScaled = vec4F_mul(&meanColor, 1.0f / (float)(pParams->m_num_pixels));

	meanColor = vec4F_mul(&meanColor, 1.0f / (float)(pParams->m_num_pixels * 255.0f));
	vec4F_saturate_in_place(&meanColor);
	
	if (pParams->m_has_alpha)
	{
		// Use incremental PCA for RGBA PCA, because it's simple.
		vec4F_set_scalar(&axis, 0.0f);
		for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		{
			bc7enc_vec4F color = vec4F_from_color(&pParams->m_pPixels[i]);
			color = vec4F_sub(&color, &meanColorScaled);
			bc7enc_vec4F a = vec4F_mul(&color, color.m_c[0]);
			bc7enc_vec4F b = vec4F_mul(&color, color.m_c[1]);
			bc7enc_vec4F c = vec4F_mul(&color, color.m_c[2]);
			bc7enc_vec4F d = vec4F_mul(&color, color.m_c[3]);
			bc7enc_vec4F n = i ? axis : color;
			vec4F_normalize_in_place(&n);
			axis.m_c[0] += vec4F_dot(&a, &n);
			axis.m_c[1] += vec4F_dot(&b, &n);
			axis.m_c[2] += vec4F_dot(&c, &n);
			axis.m_c[3] += vec4F_dot(&d, &n);
		}
		vec4F_normalize_in_place(&axis);
	}
	else
	{
		// Use covar technique for RGB PCA, because it doesn't require per-pixel normalization.
		float cov[6] = { 0, 0, 0, 0, 0, 0 };

		for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		{
			const color_quad_u8 *pV = &pParams->m_pPixels[i];
			float r = pV->m_c[0] - meanColorScaled.m_c[0];
			float g = pV->m_c[1] - meanColorScaled.m_c[1];
			float b = pV->m_c[2] - meanColorScaled.m_c[2];
			cov[0] += r*r; cov[1] += r*g; cov[2] += r*b; cov[3] += g*g; cov[4] += g*b; cov[5] += b*b;
		}

		float xr = .9f, xg = 1.0f, xb = .7f;
		for (uint32_t iter = 0; iter < 3; iter++)
		{
			float r = xr * cov[0] + xg * cov[1] + xb * cov[2];
			float g = xr * cov[1] + xg * cov[3] + xb * cov[4];
			float b = xr * cov[2] + xg * cov[4] + xb * cov[5];

			float m = maximumf(maximumf(fabsf(r), fabsf(g)), fabsf(b));
			if (m > 1e-10f)
			{
				m = 1.0f / m;
				r *= m; g *= m; b *= m;
			}

			xr = r; xg = g; xb = b;
		}

		float len = xr * xr + xg * xg + xb * xb;
		if (len < 1e-10f)
			vec4F_set_scalar(&axis, 0.0f);
		else
		{
			len = 1.0f / sqrtf(len);
			xr *= len; xg *= len; xb *= len;
			vec4F_set(&axis, xr, xg, xb, 0);
		}
	}
				
	if (vec4F_dot(&axis, &axis) < .5f)
	{
		if (pParams->m_perceptual)
			vec4F_set(&axis, .213f, .715f, .072f, pParams->m_has_alpha ? .715f : 0);
		else
			vec4F_set(&axis, 1.0f, 1.0f, 1.0f, pParams->m_has_alpha ? 1.0f : 0);
		vec4F_normalize_in_place(&axis);
	}
			
	bc7enc_vec4F minColor, maxColor;

	float l = 1e+9f, h = -1e+9f;

	for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
	{
		bc7enc_vec4F color = vec4F_from_color(&pParams->m_pPixels[i]);

		bc7enc_vec4F q = vec4F_sub(&color, &meanColorScaled);
		float d = vec4F_dot(&q, &axis);

		l = minimumf(l, d);
		h = maximumf(h, d);
	}

	l *= (1.0f / 255.0f);
	h *= (1.0f / 255.0f);

	bc7enc_vec4F b0 = vec4F_mul(&axis, l);
	bc7enc_vec4F b1 = vec4F_mul(&axis, h);
	bc7enc_vec4F c0 = vec4F_add(&meanColor, &b0);
	bc7enc_vec4F c1 = vec4F_add(&meanColor, &b1);
	minColor = vec4F_saturate(&c0);
	maxColor = vec4F_saturate(&c1);
				
	bc7enc_vec4F whiteVec;
	vec4F_set_scalar(&whiteVec, 1.0f);
	if (vec4F_dot(&minColor, &whiteVec) > vec4F_dot(&maxColor, &whiteVec))
	{
#if 1
		std::swap(minColor.m_c[0], maxColor.m_c[0]);
		std::swap(minColor.m_c[1], maxColor.m_c[1]);
		std::swap(minColor.m_c[2], maxColor.m_c[2]);
		std::swap(minColor.m_c[3], maxColor.m_c[3]);
#elif 0
		// Fails to compile correctly with MSVC 2019 (code generation bug)
		std::swap(minColor, maxColor);
#else
		// Fails with MSVC 2019
		bc7enc_vec4F temp = minColor;
		minColor = maxColor;
		maxColor = temp;
#endif
	}

	// First find a solution using the block's PCA.
	if (!find_optimal_solution(mode, minColor, maxColor, pParams, pResults))
		return 0;
	
	for (uint32_t i = 0; i < pComp_params->m_least_squares_passes; i++)
	{
		// Now try to refine the solution using least squares by computing the optimal endpoints from the current selectors.
		bc7enc_vec4F xl, xh;
		vec4F_set_scalar(&xl, 0.0f);
		vec4F_set_scalar(&xh, 0.0f);
		if (pParams->m_has_alpha)
			compute_least_squares_endpoints_rgba(pParams->m_num_pixels, pResults->m_pSelectors, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);
		else
			compute_least_squares_endpoints_rgb(pParams->m_num_pixels, pResults->m_pSelectors, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);

		xl = vec4F_mul(&xl, (1.0f / 255.0f));
		xh = vec4F_mul(&xh, (1.0f / 255.0f));
				
		if (!find_optimal_solution(mode, xl, xh, pParams, pResults))
			return 0;
	}
	
	if ((!pParams->m_pForce_selectors) && (pComp_params->m_uber_level > 0))
	{
		// In uber level 1, try varying the selectors a little, somewhat like cluster fit would. First try incrementing the minimum selectors,
		// then try decrementing the selectrors, then try both.
		uint8_t selectors_temp[16], selectors_temp1[16];
		memcpy(selectors_temp, pResults->m_pSelectors, pParams->m_num_pixels);

		const int max_selector = pParams->m_num_selector_weights - 1;

		uint32_t min_sel = 256;
		uint32_t max_sel = 0;
		for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		{
			uint32_t sel = selectors_temp[i];
			min_sel = minimumu(min_sel, sel);
			max_sel = maximumu(max_sel, sel);
		}

		for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		{
			uint32_t sel = selectors_temp[i];
			if ((sel == min_sel) && (sel < (pParams->m_num_selector_weights - 1)))
				sel++;
			selectors_temp1[i] = (uint8_t)sel;
		}

		bc7enc_vec4F xl, xh;
		vec4F_set_scalar(&xl, 0.0f);
		vec4F_set_scalar(&xh, 0.0f);
		if (pParams->m_has_alpha)
			compute_least_squares_endpoints_rgba(pParams->m_num_pixels, selectors_temp1, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);
		else
			compute_least_squares_endpoints_rgb(pParams->m_num_pixels, selectors_temp1, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);

		xl = vec4F_mul(&xl, (1.0f / 255.0f));
		xh = vec4F_mul(&xh, (1.0f / 255.0f));
				
		if (!find_optimal_solution(mode, xl, xh, pParams, pResults))
			return 0;

		for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		{
			uint32_t sel = selectors_temp[i];
			if ((sel == max_sel) && (sel > 0))
				sel--;
			selectors_temp1[i] = (uint8_t)sel;
		}

		if (pParams->m_has_alpha)
			compute_least_squares_endpoints_rgba(pParams->m_num_pixels, selectors_temp1, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);
		else
			compute_least_squares_endpoints_rgb(pParams->m_num_pixels, selectors_temp1, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);

		xl = vec4F_mul(&xl, (1.0f / 255.0f));
		xh = vec4F_mul(&xh, (1.0f / 255.0f));
				
		if (!find_optimal_solution(mode, xl, xh, pParams, pResults))
			return 0;

		for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
		{
			uint32_t sel = selectors_temp[i];
			if ((sel == min_sel) && (sel < (pParams->m_num_selector_weights - 1)))
				sel++;
			else if ((sel == max_sel) && (sel > 0))
				sel--;
			selectors_temp1[i] = (uint8_t)sel;
		}

		if (pParams->m_has_alpha)
			compute_least_squares_endpoints_rgba(pParams->m_num_pixels, selectors_temp1, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);
		else
			compute_least_squares_endpoints_rgb(pParams->m_num_pixels, selectors_temp1, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);

		xl = vec4F_mul(&xl, (1.0f / 255.0f));
		xh = vec4F_mul(&xh, (1.0f / 255.0f));

		if (!find_optimal_solution(mode, xl, xh, pParams, pResults))
			return 0;

		// In uber levels 2+, try taking more advantage of endpoint extrapolation by scaling the selectors in one direction or another.
		const uint32_t uber_err_thresh = (pParams->m_num_pixels * 56) >> 4;
		if ((pComp_params->m_uber_level >= 2) && (pResults->m_best_overall_err > uber_err_thresh))
		{
			const int Q = (pComp_params->m_uber_level >= 4) ? (pComp_params->m_uber_level - 2) : 1;
			for (int ly = -Q; ly <= 1; ly++)
			{
				for (int hy = max_selector - 1; hy <= (max_selector + Q); hy++)
				{
					if ((ly == 0) && (hy == max_selector))
						continue;

					for (uint32_t i = 0; i < pParams->m_num_pixels; i++)
						selectors_temp1[i] = (uint8_t)clampf(floorf((float)max_selector * ((float)selectors_temp[i] - (float)ly) / ((float)hy - (float)ly) + .5f), 0, (float)max_selector);

					//bc7enc_vec4F xl, xh;
					vec4F_set_scalar(&xl, 0.0f);
					vec4F_set_scalar(&xh, 0.0f);
					if (pParams->m_has_alpha)
						compute_least_squares_endpoints_rgba(pParams->m_num_pixels, selectors_temp1, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);
					else
						compute_least_squares_endpoints_rgb(pParams->m_num_pixels, selectors_temp1, pParams->m_pSelector_weightsx, &xl, &xh, pParams->m_pPixels);

					xl = vec4F_mul(&xl, (1.0f / 255.0f));
					xh = vec4F_mul(&xh, (1.0f / 255.0f));

					if (!find_optimal_solution(mode, xl, xh, pParams, pResults))
						return 0;
				}
			}
		}
	}
	
	if (!pParams->m_pForce_selectors)
	{
		// Try encoding the partition as a single color by using the optimal single colors tables to encode the block to its mean.
		if (mode == 1)
		{
			color_cell_compressor_results avg_results = *pResults;
			const uint32_t r = (int)(.5f + meanColor.m_c[0] * 255.0f), g = (int)(.5f + meanColor.m_c[1] * 255.0f), b = (int)(.5f + meanColor.m_c[2] * 255.0f);
			uint64_t avg_err = pack_mode1_to_one_color(pParams, &avg_results, r, g, b, pResults->m_pSelectors_temp);
			if (avg_err < pResults->m_best_overall_err)
			{
				*pResults = avg_results;
				memcpy(pResults->m_pSelectors, pResults->m_pSelectors_temp, sizeof(pResults->m_pSelectors[0]) * pParams->m_num_pixels);
				pResults->m_best_overall_err = avg_err;
			}
		}
		else if ((pParams->m_astc_endpoint_range == 8) && (pParams->m_num_selector_weights == 8) && (!pParams->m_has_alpha))
		{
			color_cell_compressor_results avg_results = *pResults;
			const uint32_t r = (int)(.5f + meanColor.m_c[0] * 255.0f), g = (int)(.5f + meanColor.m_c[1] * 255.0f), b = (int)(.5f + meanColor.m_c[2] * 255.0f);
			uint64_t avg_err = pack_astc_4bit_3bit_to_one_color(pParams, &avg_results, r, g, b, pResults->m_pSelectors_temp);
			if (avg_err < pResults->m_best_overall_err)
			{
				*pResults = avg_results;
				memcpy(pResults->m_pSelectors, pResults->m_pSelectors_temp, sizeof(pResults->m_pSelectors[0]) * pParams->m_num_pixels);
				pResults->m_best_overall_err = avg_err;
			}
		}
		else if ((pParams->m_astc_endpoint_range == 7) && (pParams->m_num_selector_weights == 4) && (!pParams->m_has_alpha))
		{
			color_cell_compressor_results avg_results = *pResults;
			const uint32_t r = (int)(.5f + meanColor.m_c[0] * 255.0f), g = (int)(.5f + meanColor.m_c[1] * 255.0f), b = (int)(.5f + meanColor.m_c[2] * 255.0f);
			uint64_t avg_err = pack_astc_range7_2bit_to_one_color(pParams, &avg_results, r, g, b, pResults->m_pSelectors_temp);
			if (avg_err < pResults->m_best_overall_err)
			{
				*pResults = avg_results;
				memcpy(pResults->m_pSelectors, pResults->m_pSelectors_temp, sizeof(pResults->m_pSelectors[0]) * pParams->m_num_pixels);
				pResults->m_best_overall_err = avg_err;
			}
		}
		else if ((pParams->m_astc_endpoint_range == 8) && (pParams->m_num_selector_weights == 4) && (pParams->m_has_alpha))
		{
			color_cell_compressor_results avg_results = *pResults;
			const uint32_t r = (int)(.5f + meanColor.m_c[0] * 255.0f), g = (int)(.5f + meanColor.m_c[1] * 255.0f), b = (int)(.5f + meanColor.m_c[2] * 255.0f), a = (int)(.5f + meanColor.m_c[3] * 255.0f);
			uint64_t avg_err = pack_astc_4bit_2bit_to_one_color_rgba(pParams, &avg_results, r, g, b, a, pResults->m_pSelectors_temp);
			if (avg_err < pResults->m_best_overall_err)
			{
				*pResults = avg_results;
				memcpy(pResults->m_pSelectors, pResults->m_pSelectors_temp, sizeof(pResults->m_pSelectors[0]) * pParams->m_num_pixels);
				pResults->m_best_overall_err = avg_err;
			}
		}
		else if ((pParams->m_astc_endpoint_range == 13) && (pParams->m_num_selector_weights == 4) && (!pParams->m_has_alpha))
		{
			color_cell_compressor_results avg_results = *pResults;
			const uint32_t r = (int)(.5f + meanColor.m_c[0] * 255.0f), g = (int)(.5f + meanColor.m_c[1] * 255.0f), b = (int)(.5f + meanColor.m_c[2] * 255.0f);
			uint64_t avg_err = pack_astc_range13_2bit_to_one_color(pParams, &avg_results, r, g, b, pResults->m_pSelectors_temp);
			if (avg_err < pResults->m_best_overall_err)
			{
				*pResults = avg_results;
				memcpy(pResults->m_pSelectors, pResults->m_pSelectors_temp, sizeof(pResults->m_pSelectors[0]) * pParams->m_num_pixels);
				pResults->m_best_overall_err = avg_err;
			}
		}
		else if ((pParams->m_astc_endpoint_range == 11) && (pParams->m_num_selector_weights == 32) && (!pParams->m_has_alpha))
		{
			color_cell_compressor_results avg_results = *pResults;
			const uint32_t r = (int)(.5f + meanColor.m_c[0] * 255.0f), g = (int)(.5f + meanColor.m_c[1] * 255.0f), b = (int)(.5f + meanColor.m_c[2] * 255.0f);
			uint64_t avg_err = pack_astc_range11_5bit_to_one_color(pParams, &avg_results, r, g, b, pResults->m_pSelectors_temp);
			if (avg_err < pResults->m_best_overall_err)
			{
				*pResults = avg_results;
				memcpy(pResults->m_pSelectors, pResults->m_pSelectors_temp, sizeof(pResults->m_pSelectors[0]) * pParams->m_num_pixels);
				pResults->m_best_overall_err = avg_err;
			}
		}
	}

#if BC7ENC_CHECK_OVERALL_ERROR
	check_best_overall_error(pParams, pResults);
#endif
		
	return pResults->m_best_overall_err;
}

uint64_t color_cell_compression_est_astc(
	uint32_t num_weights, uint32_t num_comps, const uint32_t *pWeight_table,
	uint32_t num_pixels, const color_quad_u8* pPixels, 
	uint64_t best_err_so_far, const uint32_t weights[4])
{
	assert(num_comps == 3 || num_comps == 4);
	assert(num_weights >= 1 && num_weights <= 32);
	assert(pWeight_table[0] == 0 && pWeight_table[num_weights - 1] == 64);

	// Find RGB bounds as an approximation of the block's principle axis
	uint32_t lr = 255, lg = 255, lb = 255, la = 255;
	uint32_t hr = 0, hg = 0, hb = 0, ha = 0;
	if (num_comps == 4)
	{
		for (uint32_t i = 0; i < num_pixels; i++)
		{
			const color_quad_u8* pC = &pPixels[i];
			if (pC->m_c[0] < lr) lr = pC->m_c[0];
			if (pC->m_c[1] < lg) lg = pC->m_c[1];
			if (pC->m_c[2] < lb) lb = pC->m_c[2];
			if (pC->m_c[3] < la) la = pC->m_c[3];

			if (pC->m_c[0] > hr) hr = pC->m_c[0];
			if (pC->m_c[1] > hg) hg = pC->m_c[1];
			if (pC->m_c[2] > hb) hb = pC->m_c[2];
			if (pC->m_c[3] > ha) ha = pC->m_c[3];
		}
	}
	else
	{
		for (uint32_t i = 0; i < num_pixels; i++)
		{
			const color_quad_u8* pC = &pPixels[i];
			if (pC->m_c[0] < lr) lr = pC->m_c[0];
			if (pC->m_c[1] < lg) lg = pC->m_c[1];
			if (pC->m_c[2] < lb) lb = pC->m_c[2];

			if (pC->m_c[0] > hr) hr = pC->m_c[0];
			if (pC->m_c[1] > hg) hg = pC->m_c[1];
			if (pC->m_c[2] > hb) hb = pC->m_c[2];
		}
		la = 255;
		ha = 255;
	}

	color_quad_u8 lowColor, highColor;
	color_quad_u8_set(&lowColor, lr, lg, lb, la);
	color_quad_u8_set(&highColor, hr, hg, hb, ha);

	// Place endpoints at bbox diagonals and compute interpolated colors 
	color_quad_u8 weightedColors[32];

	weightedColors[0] = lowColor;
	weightedColors[num_weights - 1] = highColor;
	for (uint32_t i = 1; i < (num_weights - 1); i++)
	{
		weightedColors[i].m_c[0] = (uint8_t)astc_interpolate_linear(lowColor.m_c[0], highColor.m_c[0], pWeight_table[i]);
		weightedColors[i].m_c[1] = (uint8_t)astc_interpolate_linear(lowColor.m_c[1], highColor.m_c[1], pWeight_table[i]);
		weightedColors[i].m_c[2] = (uint8_t)astc_interpolate_linear(lowColor.m_c[2], highColor.m_c[2], pWeight_table[i]);
		weightedColors[i].m_c[3] = (num_comps == 4) ? (uint8_t)astc_interpolate_linear(lowColor.m_c[3], highColor.m_c[3], pWeight_table[i]) : 255;
	}

	// Compute dots and thresholds
	const int ar = highColor.m_c[0] - lowColor.m_c[0];
	const int ag = highColor.m_c[1] - lowColor.m_c[1];
	const int ab = highColor.m_c[2] - lowColor.m_c[2];
	const int aa = highColor.m_c[3] - lowColor.m_c[3];

	int dots[32];
	if (num_comps == 4)
	{
		for (uint32_t i = 0; i < num_weights; i++)
			dots[i] = weightedColors[i].m_c[0] * ar + weightedColors[i].m_c[1] * ag + weightedColors[i].m_c[2] * ab + weightedColors[i].m_c[3] * aa;
	}
	else
	{
		assert(aa == 0);
		for (uint32_t i = 0; i < num_weights; i++)
			dots[i] = weightedColors[i].m_c[0] * ar + weightedColors[i].m_c[1] * ag + weightedColors[i].m_c[2] * ab;
	}

	int thresh[32 - 1];
	for (uint32_t i = 0; i < (num_weights - 1); i++)
		thresh[i] = (dots[i] + dots[i + 1] + 1) >> 1;

	uint64_t total_err = 0;
	if ((weights[0] | weights[1] | weights[2] | weights[3]) == 1)
	{
		if (num_comps == 4)
		{
			for (uint32_t i = 0; i < num_pixels; i++)
			{
				const color_quad_u8* pC = &pPixels[i];

				int d = ar * pC->m_c[0] + ag * pC->m_c[1] + ab * pC->m_c[2] + aa * pC->m_c[3];

				// Find approximate selector
				uint32_t s = 0;
				for (int j = num_weights - 2; j >= 0; j--)
				{
					if (d >= thresh[j])
					{
						s = j + 1;
						break;
					}
				}

				// Compute error
				const color_quad_u8* pE1 = &weightedColors[s];

				int dr = (int)pE1->m_c[0] - (int)pC->m_c[0];
				int dg = (int)pE1->m_c[1] - (int)pC->m_c[1];
				int db = (int)pE1->m_c[2] - (int)pC->m_c[2];
				int da = (int)pE1->m_c[3] - (int)pC->m_c[3];

				total_err += (dr * dr) + (dg * dg) + (db * db) + (da * da);
				if (total_err > best_err_so_far)
					break;
			}
		}
		else
		{
			for (uint32_t i = 0; i < num_pixels; i++)
			{
				const color_quad_u8* pC = &pPixels[i];

				int d = ar * pC->m_c[0] + ag * pC->m_c[1] + ab * pC->m_c[2];

				// Find approximate selector
				uint32_t s = 0;
				for (int j = num_weights - 2; j >= 0; j--)
				{
					if (d >= thresh[j])
					{
						s = j + 1;
						break;
					}
				}

				// Compute error
				const color_quad_u8* pE1 = &weightedColors[s];

				int dr = (int)pE1->m_c[0] - (int)pC->m_c[0];
				int dg = (int)pE1->m_c[1] - (int)pC->m_c[1];
				int db = (int)pE1->m_c[2] - (int)pC->m_c[2];

				total_err += (dr * dr) + (dg * dg) + (db * db);
				if (total_err > best_err_so_far)
					break;
			}
		}
	}
	else
	{
		if (num_comps == 4)
		{
			for (uint32_t i = 0; i < num_pixels; i++)
			{
				const color_quad_u8* pC = &pPixels[i];

				int d = ar * pC->m_c[0] + ag * pC->m_c[1] + ab * pC->m_c[2] + aa * pC->m_c[3];

				// Find approximate selector
				uint32_t s = 0;
				for (int j = num_weights - 2; j >= 0; j--)
				{
					if (d >= thresh[j])
					{
						s = j + 1;
						break;
					}
				}

				// Compute error
				const color_quad_u8* pE1 = &weightedColors[s];

				int dr = (int)pE1->m_c[0] - (int)pC->m_c[0];
				int dg = (int)pE1->m_c[1] - (int)pC->m_c[1];
				int db = (int)pE1->m_c[2] - (int)pC->m_c[2];
				int da = (int)pE1->m_c[3] - (int)pC->m_c[3];

				total_err += weights[0] * (dr * dr) + weights[1] * (dg * dg) + weights[2] * (db * db) + weights[3] * (da * da);
				if (total_err > best_err_so_far)
					break;
			}
		}
		else
		{
			for (uint32_t i = 0; i < num_pixels; i++)
			{
				const color_quad_u8* pC = &pPixels[i];

				int d = ar * pC->m_c[0] + ag * pC->m_c[1] + ab * pC->m_c[2];

				// Find approximate selector
				uint32_t s = 0;
				for (int j = num_weights - 2; j >= 0; j--)
				{
					if (d >= thresh[j])
					{
						s = j + 1;
						break;
					}
				}

				// Compute error
				const color_quad_u8* pE1 = &weightedColors[s];

				int dr = (int)pE1->m_c[0] - (int)pC->m_c[0];
				int dg = (int)pE1->m_c[1] - (int)pC->m_c[1];
				int db = (int)pE1->m_c[2] - (int)pC->m_c[2];

				total_err += weights[0] * (dr * dr) + weights[1] * (dg * dg) + weights[2] * (db * db);
				if (total_err > best_err_so_far)
					break;
			}
		}
	}

	return total_err;
}

} // namespace basisu
