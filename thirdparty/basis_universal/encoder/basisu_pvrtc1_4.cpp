// basisu_pvrtc1_4.cpp
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
#include "basisu_pvrtc1_4.h"

namespace basisu
{
#if 0
	static const uint8_t g_pvrtc_5[32] = { 0,8,16,24,33,41,49,57,66,74,82,90,99,107,115,123,132,140,148,156,165,173,181,189,198,206,214,222,231,239,247,255 };
	static const uint8_t g_pvrtc_4[16] = { 0,16,33,49,66,82,99,115,140,156,173,189,206,222,239,255 };
	static const uint8_t g_pvrtc_3[8] = { 0,33,74,107,148,181,222,255 };
	static const uint8_t g_pvrtc_alpha[9] = { 0,34,68,102,136,170,204,238,255 };
#endif

	static const uint8_t g_pvrtc_5_nearest[256] = { 0,0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,16,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18,19,19,19,19,19,19,19,19,20,20,20,20,20,20,20,20,20,21,21,21,21,21,21,21,21,22,22,22,22,22,22,22,22,23,23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,30,31,31,31,31 };
	static const uint8_t g_pvrtc_4_nearest[256] = { 0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15 };
#if 0
	static const uint8_t g_pvrtc_3_nearest[256] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7 };
	static const uint8_t g_pvrtc_alpha_nearest[256] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8 };
#endif

#if 0
	static const uint8_t g_pvrtc_5_floor[256] =
	{
		0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,
		3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,
		7,7,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,11,11,11,11,11,11,
		11,11,11,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,15,15,15,15,15,
		15,15,15,15,16,16,16,16,16,16,16,16,17,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18,19,19,19,19,
		19,19,19,19,19,20,20,20,20,20,20,20,20,21,21,21,21,21,21,21,21,22,22,22,22,22,22,22,22,23,23,23,
		23,23,23,23,23,23,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,27,27,
		27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,30,31
	};

	static const uint8_t g_pvrtc_5_ceil[256] =
	{
		0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,
		4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,8,8,8,8,8,8,
		8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,12,12,12,12,12,
		12,12,12,12,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,16,16,16,16,
		16,16,16,16,16,17,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18,19,19,19,19,19,19,19,19,20,20,20,
		20,20,20,20,20,20,21,21,21,21,21,21,21,21,22,22,22,22,22,22,22,22,23,23,23,23,23,23,23,23,24,24,
		24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,28,
		28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,30,31,31,31,31,31,31,31,31
	};

	static const uint8_t g_pvrtc_4_floor[256] =
	{
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
		3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
		5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,
		9,9,9,9,9,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,
		11,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,
		13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15
	};

	static const uint8_t g_pvrtc_4_ceil[256] =
	{
		0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,
		6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,
		8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,10,10,10,
		10,10,10,10,10,10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,
		12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,14,
		14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15
	};

	static const uint8_t g_pvrtc_3_floor[256] =
	{
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
		3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,
		4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7
	};

	static const uint8_t g_pvrtc_3_ceil[256] =
	{
		0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
		3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
	};

	static const uint8_t g_pvrtc_alpha_floor[256] =
	{
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
		3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		6,6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8
	};

	static const uint8_t g_pvrtc_alpha_ceil[256] =
	{
		0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
		3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
		4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
		5,5,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
		6,6,6,6,6,6,6,6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
	};
#endif

	uint32_t pvrtc4_swizzle_uv(uint32_t width, uint32_t height, uint32_t x, uint32_t y)
	{
		assert((x < width) && (y < height) && basisu::is_pow2(height) && basisu::is_pow2(width));
				
		uint32_t min_d = width, max_v = y;
		if (height < width)
		{
			min_d = height;
			max_v = x;
		}

		// Interleave the XY LSB's
		uint32_t shift_ofs = 0, swizzled = 0;
		for (uint32_t s_bit = 1, d_bit = 1; s_bit < min_d; s_bit <<= 1, d_bit <<= 2, ++shift_ofs)
		{
			if (y & s_bit) swizzled |= d_bit;
			if (x & s_bit) swizzled |= (2 * d_bit);
		}

		max_v >>= shift_ofs;
		
		// OR in the rest of the bits from the largest dimension
		swizzled |= (max_v << (2 * shift_ofs));

		return swizzled;
	}

	color_rgba pvrtc4_block::get_endpoint(uint32_t endpoint_index, bool unpack) const
	{
		assert(endpoint_index < 2);
		const uint32_t packed = m_endpoints >> (endpoint_index * 16);

		uint32_t r, g, b, a;
		if (packed & 0x8000)
		{
			// opaque 554 or 555
			if (!endpoint_index)
			{
				r = (packed >> 10) & 31;
				g = (packed >> 5) & 31;
				b = (packed >> 1) & 15;
					
				if (unpack)
				{
					b = (b << 1) | (b >> 3);
				}
			}
			else
			{
				r = (packed >> 10) & 31;
				g = (packed >> 5) & 31;
				b = packed & 31;
			}

			a = unpack ? 255 : 7;
		}
		else
		{
			// translucent 4433 or 4443
			if (!endpoint_index)
			{
				a = (packed >> 12) & 7;
				r = (packed >> 8) & 15;
				g = (packed >> 4) & 15;
				b = (packed >> 1) & 7;

				if (unpack)
				{
					a = (a << 1);
					a = (a << 4) | a;
						
					r = (r << 1) | (r >> 3);
					g = (g << 1) | (g >> 3);
					b = (b << 2) | (b >> 1);
				}
			}
			else
			{
				a = (packed >> 12) & 7;
				r = (packed >> 8) & 15;
				g = (packed >> 4) & 15;
				b = packed & 15;

				if (unpack)
				{
					a = (a << 1);
					a = (a << 4) | a;

					r = (r << 1) | (r >> 3);
					g = (g << 1) | (g >> 3);
					b = (b << 1) | (b >> 3);
				}
			}
		}

		if (unpack)
		{
			r = (r << 3) | (r >> 2);
			g = (g << 3) | (g >> 2);
			b = (b << 3) | (b >> 2);
		}

		assert((r < 256) && (g < 256) && (b < 256) && (a < 256));

		return color_rgba(r, g, b, a);
	}

	color_rgba pvrtc4_block::get_endpoint_5554(uint32_t endpoint_index) const
	{
		assert(endpoint_index < 2);
		const uint32_t packed = m_endpoints >> (endpoint_index * 16);

		uint32_t r, g, b, a;
		if (packed & 0x8000)
		{
			// opaque 554 or 555
			if (!endpoint_index)
			{
				r = (packed >> 10) & 31;
				g = (packed >> 5) & 31;
				b = (packed >> 1) & 15;

				b = (b << 1) | (b >> 3);
			}
			else
			{
				r = (packed >> 10) & 31;
				g = (packed >> 5) & 31;
				b = packed & 31;
			}

			a = 15;
		}
		else
		{
			// translucent 4433 or 4443
			if (!endpoint_index)
			{
				a = (packed >> 12) & 7;
				r = (packed >> 8) & 15;
				g = (packed >> 4) & 15;
				b = (packed >> 1) & 7;

				a = a << 1;
						
				r = (r << 1) | (r >> 3);
				g = (g << 1) | (g >> 3);
				b = (b << 2) | (b >> 1);
			}
			else
			{
				a = (packed >> 12) & 7;
				r = (packed >> 8) & 15;
				g = (packed >> 4) & 15;
				b = packed & 15;

				a = a << 1;
						
				r = (r << 1) | (r >> 3);
				g = (g << 1) | (g >> 3);
				b = (b << 1) | (b >> 3);
			}
		}
						
		assert((r < 32) && (g < 32) && (b < 32) && (a < 16));

		return color_rgba(r, g, b, a);
	}

	bool pvrtc4_image::get_interpolated_colors(uint32_t x, uint32_t y, color_rgba* pColors) const
	{
		assert((x < m_width) && (y < m_height));

		int block_x0 = (static_cast<int>(x) - 2) >> 2;
		int block_x1 = block_x0 + 1;
		int block_y0 = (static_cast<int>(y) - 2) >> 2;
		int block_y1 = block_y0 + 1;
		
		block_x0 = posmod(block_x0, m_block_width);
		block_x1 = posmod(block_x1, m_block_width);
		block_y0 = posmod(block_y0, m_block_height);
		block_y1 = posmod(block_y1, m_block_height);
		
		pColors[0] = interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(0), m_blocks(block_x1, block_y0).get_endpoint_5554(0), m_blocks(block_x0, block_y1).get_endpoint_5554(0), m_blocks(block_x1, block_y1).get_endpoint_5554(0));
		pColors[3] = interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(1), m_blocks(block_x1, block_y0).get_endpoint_5554(1), m_blocks(block_x0, block_y1).get_endpoint_5554(1), m_blocks(block_x1, block_y1).get_endpoint_5554(1));

		if (get_block_uses_transparent_modulation(x >> 2, y >> 2))
		{
			for (uint32_t c = 0; c < 4; c++)
			{
				uint32_t m = (pColors[0][c] + pColors[3][c]) / 2;
				pColors[1][c] = static_cast<uint8_t>(m);
				pColors[2][c] = static_cast<uint8_t>(m);
			}
			pColors[2][3] = 0;
			return true;
		}

		for (uint32_t c = 0; c < 4; c++)
		{
			pColors[1][c] = static_cast<uint8_t>((pColors[0][c] * 5 + pColors[3][c] * 3) / 8);
			pColors[2][c] = static_cast<uint8_t>((pColors[0][c] * 3 + pColors[3][c] * 5) / 8);
		}

		return false;
	}
		
	color_rgba pvrtc4_image::get_pixel(uint32_t x, uint32_t y, uint32_t m) const
	{
		assert((x < m_width) && (y < m_height));

		int block_x0 = (static_cast<int>(x) - 2) >> 2;
		int block_x1 = block_x0 + 1;
		int block_y0 = (static_cast<int>(y) - 2) >> 2;
		int block_y1 = block_y0 + 1;
		
		block_x0 = posmod(block_x0, m_block_width);
		block_x1 = posmod(block_x1, m_block_width);
		block_y0 = posmod(block_y0, m_block_height);
		block_y1 = posmod(block_y1, m_block_height);
		
		if (get_block_uses_transparent_modulation(x >> 2, y >> 2))
		{
			if (m == 0)
				return interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(0), m_blocks(block_x1, block_y0).get_endpoint_5554(0), m_blocks(block_x0, block_y1).get_endpoint_5554(0), m_blocks(block_x1, block_y1).get_endpoint_5554(0));
			else if (m == 3)
				return interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(1), m_blocks(block_x1, block_y0).get_endpoint_5554(1), m_blocks(block_x0, block_y1).get_endpoint_5554(1), m_blocks(block_x1, block_y1).get_endpoint_5554(1));

			color_rgba l(interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(0), m_blocks(block_x1, block_y0).get_endpoint_5554(0), m_blocks(block_x0, block_y1).get_endpoint_5554(0), m_blocks(block_x1, block_y1).get_endpoint_5554(0)));
			color_rgba h(interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(1), m_blocks(block_x1, block_y0).get_endpoint_5554(1), m_blocks(block_x0, block_y1).get_endpoint_5554(1), m_blocks(block_x1, block_y1).get_endpoint_5554(1)));

			return color_rgba((l[0] + h[0]) / 2, (l[1] + h[1]) / 2, (l[2] + h[2]) / 2, (m == 2) ? 0 : (l[3] + h[3]) / 2);
		}
		else
		{
			if (m == 0)
				return interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(0), m_blocks(block_x1, block_y0).get_endpoint_5554(0), m_blocks(block_x0, block_y1).get_endpoint_5554(0), m_blocks(block_x1, block_y1).get_endpoint_5554(0));
			else if (m == 3)
				return interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(1), m_blocks(block_x1, block_y0).get_endpoint_5554(1), m_blocks(block_x0, block_y1).get_endpoint_5554(1), m_blocks(block_x1, block_y1).get_endpoint_5554(1));

			color_rgba l(interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(0), m_blocks(block_x1, block_y0).get_endpoint_5554(0), m_blocks(block_x0, block_y1).get_endpoint_5554(0), m_blocks(block_x1, block_y1).get_endpoint_5554(0)));
			color_rgba h(interpolate(x, y, m_blocks(block_x0, block_y0).get_endpoint_5554(1), m_blocks(block_x1, block_y0).get_endpoint_5554(1), m_blocks(block_x0, block_y1).get_endpoint_5554(1), m_blocks(block_x1, block_y1).get_endpoint_5554(1)));

			if (m == 2)
				return color_rgba((l[0] * 3 + h[0] * 5) / 8, (l[1] * 3 + h[1] * 5) / 8, (l[2] * 3 + h[2] * 5) / 8, (l[3] * 3 + h[3] * 5) / 8);
			else
				return color_rgba((l[0] * 5 + h[0] * 3) / 8, (l[1] * 5 + h[1] * 3) / 8, (l[2] * 5 + h[2] * 3) / 8, (l[3] * 5 + h[3] * 3) / 8);
		}
	}

	uint64_t pvrtc4_image::local_endpoint_optimization_opaque(uint32_t bx, uint32_t by, const image& orig_img, bool perceptual)
	{
		uint64_t initial_error = evaluate_1x1_endpoint_error(bx, by, orig_img, perceptual, false);
		if (!initial_error)
			return initial_error;

		vec3F c_avg_orig(0);

		for (int y = 0; y < 7; y++)
		{
			const uint32_t py = wrap_y(by * 4 + y - 1);
			for (uint32_t x = 0; x < 7; x++)
			{
				const uint32_t px = wrap_x(bx * 4 + x - 1);

				const color_rgba& c = orig_img(px, py);

				c_avg_orig[0] += c[0];
				c_avg_orig[1] += c[1];
				c_avg_orig[2] += c[2];
			}
		}

		c_avg_orig *= 1.0f / 49.0f;

		vec3F quant_colors[2];
		quant_colors[0].set(c_avg_orig);
		quant_colors[0] -= vec3F(.0125f);

		quant_colors[1].set(c_avg_orig);
		quant_colors[1] += vec3F(.0125f);

		float total_weight[2];

		bool success = true;

		for (uint32_t pass = 0; pass < 4; pass++)
		{
			vec3F new_colors[2] = { vec3F(0), vec3F(0) };
			memset(total_weight, 0, sizeof(total_weight));

			static const float s_weights[7][7] =
			{
				{ 1.000000f, 1.637089f, 2.080362f, 2.242640f, 2.080362f, 1.637089f, 1.000000f },
				{ 1.637089f, 2.414213f, 3.006572f, 3.242640f, 3.006572f, 2.414213f, 1.637089f },
				{ 2.080362f, 3.006572f, 3.828426f, 4.242640f, 3.828426f, 3.006572f, 2.080362f },
				{ 2.242640f, 3.242640f, 4.242640f, 5.000000f, 4.242640f, 3.242640f, 2.242640f },
				{ 2.080362f, 3.006572f, 3.828426f, 4.242640f, 3.828426f, 3.006572f, 2.080362f },
				{ 1.637089f, 2.414213f, 3.006572f, 3.242640f, 3.006572f, 2.414213f, 1.637089f },
				{ 1.000000f, 1.637089f, 2.080362f, 2.242640f, 2.080362f, 1.637089f, 1.000000f }
			};

			for (int y = 0; y < 7; y++)
			{
				const uint32_t py = wrap_y(by * 4 + y - 1);
				for (uint32_t x = 0; x < 7; x++)
				{
					const uint32_t px = wrap_x(bx * 4 + x - 1);

					const color_rgba& orig_c = orig_img(px, py);

					vec3F color(orig_c[0], orig_c[1], orig_c[2]);

					uint32_t c = quant_colors[0].squared_distance(color) > quant_colors[1].squared_distance(color);

					const float weight = s_weights[y][x];
					new_colors[c] += color * weight;

					total_weight[c] += weight;
				}
			}

			if (!total_weight[0] || !total_weight[1])
				success = false;

			quant_colors[0] = new_colors[0] / (float)total_weight[0];
			quant_colors[1] = new_colors[1] / (float)total_weight[1];
		}

		if (!success)
		{
			quant_colors[0] = c_avg_orig;
			quant_colors[1] = c_avg_orig;
		}

		vec4F colors[2] = { quant_colors[0], quant_colors[1] };

		colors[0] += vec3F(.5f);
		colors[1] += vec3F(.5f);
		color_rgba color_0((int)colors[0][0], (int)colors[0][1], (int)colors[0][2], 0);
		color_rgba color_1((int)colors[1][0], (int)colors[1][1], (int)colors[1][2], 0);

		pvrtc4_block cur_blocks[3][3];
		
		for (int y = -1; y <= 1; y++)
		{
			for (int x = -1; x <= 1; x++)
			{
				const uint32_t block_x = wrap_block_x(bx + x);
				const uint32_t block_y = wrap_block_y(by + y);
				cur_blocks[x + 1][y + 1] = m_blocks(block_x, block_y);
			}
		}

		color_rgba l1(0), h1(0);

		l1[0] = g_pvrtc_5_nearest[color_0[0]];
		h1[0] = g_pvrtc_5_nearest[color_1[0]];

		l1[1] = g_pvrtc_5_nearest[color_0[1]];
		h1[1] = g_pvrtc_5_nearest[color_1[1]];

		l1[2] = g_pvrtc_4_nearest[color_0[2]];
		h1[2] = g_pvrtc_5_nearest[color_0[2]];

		l1[3] = 0;
		h1[3] = 0;

		m_blocks(bx, by).set_endpoint_raw(0, l1, true);
		m_blocks(bx, by).set_endpoint_raw(1, h1, true);

		uint64_t e03_err_0 = remap_pixels_influenced_by_endpoint(bx, by, orig_img, perceptual, false);

		pvrtc4_block blocks0[3][3];
		for (int y = -1; y <= 1; y++)
		{
			for (int x = -1; x <= 1; x++)
			{
				const uint32_t block_x = wrap_block_x(bx + x);
				const uint32_t block_y = wrap_block_y(by + y);
				blocks0[x + 1][y + 1] = m_blocks(block_x, block_y);
			}
		}

		l1[0] = g_pvrtc_5_nearest[color_1[0]];
		h1[0] = g_pvrtc_5_nearest[color_0[0]];

		l1[1] = g_pvrtc_5_nearest[color_1[1]];
		h1[1] = g_pvrtc_5_nearest[color_0[1]];

		l1[2] = g_pvrtc_4_nearest[color_1[2]];
		h1[2] = g_pvrtc_5_nearest[color_0[2]];

		l1[3] = 0;
		h1[3] = 0;

		m_blocks(bx, by).set_endpoint_raw(0, l1, true);
		m_blocks(bx, by).set_endpoint_raw(1, h1, true);

		uint64_t e03_err_1 = remap_pixels_influenced_by_endpoint(bx, by, orig_img, perceptual, false);

		if (initial_error < basisu::minimum(e03_err_0, e03_err_1))
		{
			for (int y = -1; y <= 1; y++)
			{
				for (int x = -1; x <= 1; x++)
				{
					const uint32_t block_x = wrap_block_x(bx + x);
					const uint32_t block_y = wrap_block_y(by + y);
					m_blocks(block_x, block_y) = cur_blocks[x + 1][y + 1];
				}
			}
			return initial_error;
		}
		else if (e03_err_0 < e03_err_1)
		{
			for (int y = -1; y <= 1; y++)
			{
				for (int x = -1; x <= 1; x++)
				{
					const uint32_t block_x = wrap_block_x(bx + x);
					const uint32_t block_y = wrap_block_y(by + y);
					m_blocks(block_x, block_y) = blocks0[x + 1][y + 1];
				}
			}
			assert(e03_err_0 == evaluate_1x1_endpoint_error(bx, by, orig_img, perceptual, false));
			return e03_err_0;
		}

		assert(e03_err_1 == evaluate_1x1_endpoint_error(bx, by, orig_img, perceptual, false));
		return e03_err_1;
	}

} // basisu
