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
#pragma once
#include "basisu_gpu_texture.h"

namespace basisu
{
	enum 
	{ 
		PVRTC2_MIN_WIDTH = 16, 
		PVRTC2_MIN_HEIGHT = 8, 
		PVRTC4_MIN_WIDTH = 8, 
		PVRTC4_MIN_HEIGHT = 8 
	};
	
	struct pvrtc4_block
	{
		uint32_t m_modulation;
		uint32_t m_endpoints;

		pvrtc4_block() : m_modulation(0), m_endpoints(0) { }

		inline bool operator== (const pvrtc4_block& rhs) const
		{
			return (m_modulation == rhs.m_modulation) && (m_endpoints == rhs.m_endpoints);
		}

		inline void clear()
		{
			m_modulation = 0;
			m_endpoints = 0;
		}

		inline bool get_block_uses_transparent_modulation() const
		{
			return (m_endpoints & 1) != 0;
		}

		inline bool is_endpoint_opaque(uint32_t endpoint_index) const
		{
			static const uint32_t s_bitmasks[2] = { 0x8000U, 0x80000000U };
			return (m_endpoints & s_bitmasks[open_range_check(endpoint_index, 2U)]) != 0;
		}

		// Returns raw endpoint or 8888
		color_rgba get_endpoint(uint32_t endpoint_index, bool unpack) const;
		
		color_rgba get_endpoint_5554(uint32_t endpoint_index) const;
		
		static uint32_t get_component_precision_in_bits(uint32_t c, uint32_t endpoint_index, bool opaque_endpoint)
		{
			static const uint32_t s_comp_prec[4][4] =
			{
				// R0 G0 B0 A0      R1 G1 B1 A1
				{  4, 4, 3, 3 }, {  4, 4, 4, 3 }, // transparent endpoint

				{  5, 5, 4, 0 }, {  5, 5, 5, 0 }  // opaque endpoint
			};
			return s_comp_prec[open_range_check(endpoint_index, 2U) + (opaque_endpoint * 2)][open_range_check(c, 4U)];
		}

		static color_rgba get_color_precision_in_bits(uint32_t endpoint_index, bool opaque_endpoint)
		{
			static const color_rgba s_color_prec[4] =
			{
			   color_rgba(4, 4, 3, 3), color_rgba(4, 4, 4, 3), // transparent endpoint
			   color_rgba(5, 5, 4, 0), color_rgba(5, 5, 5, 0)  // opaque endpoint
			};
			return s_color_prec[open_range_check(endpoint_index, 2U) + (opaque_endpoint * 2)];
		}
		
		inline uint32_t get_modulation(uint32_t x, uint32_t y) const
		{
			assert((x < 4) && (y < 4));
			return (m_modulation >> ((y * 4 + x) * 2)) & 3;
		}

		inline void set_modulation(uint32_t x, uint32_t y, uint32_t s)
		{
			assert((x < 4) && (y < 4) && (s < 4));
			uint32_t n = (y * 4 + x) * 2;
			m_modulation = (m_modulation & (~(3 << n))) | (s << n);
			assert(get_modulation(x, y) == s);
		}

		// Scaled by 8
		inline const uint32_t* get_scaled_modulation_values(bool block_uses_transparent_modulation) const
		{
			static const uint32_t s_block_scales[2][4] = { { 0, 3, 5, 8 }, { 0, 4, 4, 8 } };
			return s_block_scales[block_uses_transparent_modulation];
		}

		// Scaled by 8
		inline uint32_t get_scaled_modulation(uint32_t x, uint32_t y) const
		{
			return get_scaled_modulation_values(get_block_uses_transparent_modulation())[get_modulation(x, y)];
		}

		inline void byte_swap()
		{
			m_modulation = byteswap32(m_modulation);
			m_endpoints = byteswap32(m_endpoints);
		}

		// opaque endpoints:	554, 555
		// transparent endpoints: 3443, 3444
		inline void set_endpoint_raw(uint32_t endpoint_index, const color_rgba& c, bool opaque_endpoint)
		{
			assert(endpoint_index < 2);
			const uint32_t m = m_endpoints & 1;
			uint32_t r = c[0], g = c[1], b = c[2], a = c[3];
						
			uint32_t packed;

			if (opaque_endpoint)
			{
				if (!endpoint_index)
				{
					// 554
					// 1RRRRRGGGGGBBBBM
					assert((r < 32) && (g < 32) && (b < 16));
					packed = 0x8000 | (r << 10) | (g << 5) | (b << 1) | m;
				}
				else
				{
					// 555
					// 1RRRRRGGGGGBBBBB
					assert((r < 32) && (g < 32) && (b < 32));
					packed = 0x8000 | (r << 10) | (g << 5) | b;
				}
			}
			else
			{
				if (!endpoint_index)
				{
					// 3443
					// 0AAA RRRR GGGG BBBM
					assert((r < 16) && (g < 16) && (b < 8) && (a < 8));
					packed = (a << 12) | (r << 8) | (g << 4) | (b << 1) | m;
				}
				else
				{
					// 3444
					// 0AAA RRRR GGGG BBBB
					assert((r < 16) && (g < 16) && (b < 16) && (a < 8));
					packed = (a << 12) | (r << 8) | (g << 4) | b;
				}
			}

			assert(packed <= 0xFFFF);

			if (endpoint_index)
				m_endpoints = (m_endpoints & 0xFFFFU) | (packed << 16);
			else
				m_endpoints = (m_endpoints & 0xFFFF0000U) | packed;
		}
	};

	typedef vector2D<pvrtc4_block> pvrtc4_block_vector2D;

	uint32_t pvrtc4_swizzle_uv(uint32_t XSize, uint32_t YSize, uint32_t XPos, uint32_t YPos);

	class pvrtc4_image
	{
	public:
		inline pvrtc4_image() :
			m_width(0), m_height(0), m_block_width(0), m_block_height(0), m_uses_alpha(false)
		{
		}

		inline pvrtc4_image(uint32_t width, uint32_t height) :
			m_width(0), m_height(0), m_block_width(0), m_block_height(0), m_uses_alpha(false)
		{
			resize(width, height);
		}

		inline void clear()
		{
			m_width = 0;
			m_height = 0;
			m_block_width = 0;
			m_block_height = 0;
			m_blocks.clear();
			m_uses_alpha = false;
		}

		inline void resize(uint32_t width, uint32_t height)
		{
			if ((width == m_width) && (height == m_height))
				return;

			m_width = width;
			m_height = height;

			m_block_width = (width + 3) >> 2;
			m_block_height = (height + 3) >> 2;

			m_blocks.resize(m_block_width, m_block_height);
		}

		inline uint32_t get_width() const { return m_width; }
		inline uint32_t get_height() const { return m_height; }

		inline uint32_t get_block_width() const { return m_block_width; }
		inline uint32_t get_block_height() const { return m_block_height; }

		inline const pvrtc4_block_vector2D &get_blocks() const { return m_blocks; }
		inline		 pvrtc4_block_vector2D &get_blocks() { return m_blocks; }

		inline uint32_t get_total_blocks() const { return m_block_width * m_block_height; }

		inline bool get_uses_alpha() const { return m_uses_alpha; }
		inline void set_uses_alpha(bool uses_alpha) { m_uses_alpha = uses_alpha; }

		inline bool are_blocks_equal(const pvrtc4_image& rhs) const
		{
			return m_blocks == rhs.m_blocks;
		}

		inline void set_to_black()
		{
#ifndef __EMSCRIPTEN__
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"            
#endif                          
#endif
			memset(m_blocks.get_ptr(), 0, m_blocks.size_in_bytes());
#ifndef __EMSCRIPTEN__
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif                
#endif
		}

		inline bool get_block_uses_transparent_modulation(uint32_t bx, uint32_t by) const
		{
			return m_blocks(bx, by).get_block_uses_transparent_modulation();
		}

		inline bool is_endpoint_opaque(uint32_t bx, uint32_t by, uint32_t endpoint_index) const
		{
			return m_blocks(bx, by).is_endpoint_opaque(endpoint_index);
		}
				
		color_rgba get_endpoint(uint32_t bx, uint32_t by, uint32_t endpoint_index, bool unpack) const
		{
			assert((bx < m_block_width) && (by < m_block_height));
			return m_blocks(bx, by).get_endpoint(endpoint_index, unpack);
		}

		inline uint32_t get_modulation(uint32_t x, uint32_t y) const
		{
			assert((x < m_width) && (y < m_height));
			return m_blocks(x >> 2, y >> 2).get_modulation(x & 3, y & 3);
		}
				
		// Returns true if the block uses transparent modulation.
		bool get_interpolated_colors(uint32_t x, uint32_t y, color_rgba* pColors) const;
		
		color_rgba get_pixel(uint32_t x, uint32_t y, uint32_t m) const;
		
		inline color_rgba get_pixel(uint32_t x, uint32_t y) const
		{
			assert((x < m_width) && (y < m_height));
			return get_pixel(x, y, m_blocks(x >> 2, y >> 2).get_modulation(x & 3, y & 3));
		}

		void deswizzle()
		{
			pvrtc4_block_vector2D temp(m_blocks);

			for (uint32_t y = 0; y < m_block_height; y++)
				for (uint32_t x = 0; x < m_block_width; x++)
					m_blocks(x, y) = temp[pvrtc4_swizzle_uv(m_block_width, m_block_height, x, y)];
		}

		void swizzle()
		{
			pvrtc4_block_vector2D temp(m_blocks);

			for (uint32_t y = 0; y < m_block_height; y++)
				for (uint32_t x = 0; x < m_block_width; x++)
					m_blocks[pvrtc4_swizzle_uv(m_block_width, m_block_height, x, y)] = temp(x, y);
		}

		void unpack_all_pixels(image& img) const
		{
			img.crop(m_width, m_height);

			for (uint32_t y = 0; y < m_height; y++)
				for (uint32_t x = 0; x < m_width; x++)
					img(x, y) = get_pixel(x, y);
		}

		void unpack_block(image &dst, uint32_t block_x, uint32_t block_y)
		{
			for (uint32_t y = 0; y < 4; y++)
				for (uint32_t x = 0; x < 4; x++)
					dst(x, y) = get_pixel(block_x * 4 + x, block_y * 4 + y);
		}

		inline int wrap_x(int x) const
		{
			return posmod(x, m_width);
		}

		inline int wrap_y(int y) const
		{
			return posmod(y, m_height);
		}

		inline int wrap_block_x(int bx) const
		{
			return posmod(bx, m_block_width);
		}

		inline int wrap_block_y(int by) const
		{
			return posmod(by, m_block_height);
		}

		inline vec2F get_interpolation_factors(uint32_t x, uint32_t y) const
		{
			// 0 1 2 3
			// 2 3 0 1
			// .5 .75 0 .25
			static const float s_interp[4] = { 2, 3, 0, 1 };
			return vec2F(s_interp[x & 3], s_interp[y & 3]);
		}

		inline color_rgba interpolate(int x, int y,
			const color_rgba& p, const color_rgba& q,
			const color_rgba& r, const color_rgba& s) const
		{
			static const int s_interp[4] = { 2, 3, 0, 1 };
			const int u_interp = s_interp[x & 3];
			const int v_interp = s_interp[y & 3];

			color_rgba result;

			for (uint32_t c = 0; c < 4; c++)
			{
				int t = p[c] * 4 + u_interp * ((int)q[c] - (int)p[c]);
				int b = r[c] * 4 + u_interp * ((int)s[c] - (int)r[c]);
				int v = t * 4 + v_interp * (b - t);
				if (c < 3)
				{
					v >>= 1;
					v += (v >> 5);
				}
				else
				{
					v += (v >> 4);
				}
				assert((v >= 0) && (v < 256));
				result[c] = static_cast<uint8_t>(v);
			}

			return result;
		}

		inline void set_modulation(uint32_t x, uint32_t y, uint32_t s)
		{
			assert((x < m_width) && (y < m_height));
			return m_blocks(x >> 2, y >> 2).set_modulation(x & 3, y & 3, s);
		}

		inline uint64_t map_pixel(uint32_t x, uint32_t y, const color_rgba& c, bool perceptual, bool alpha_is_significant, bool record = true)
		{
			color_rgba v[4];
			get_interpolated_colors(x, y, v);

			uint64_t best_dist = color_distance(perceptual, c, v[0], alpha_is_significant);
			uint32_t best_v = 0;
			for (uint32_t i = 1; i < 4; i++)
			{
				uint64_t dist = color_distance(perceptual, c, v[i], alpha_is_significant);
				if (dist < best_dist)
				{
					best_dist = dist;
					best_v = i;
				}
			}

			if (record)
				set_modulation(x, y, best_v);

			return best_dist;
		}

		inline uint64_t remap_pixels_influenced_by_endpoint(uint32_t bx, uint32_t by, const image& orig_img, bool perceptual, bool alpha_is_significant)
		{
			uint64_t total_error = 0;

			for (int yd = -3; yd <= 3; yd++)
			{
				const int y = wrap_y((int)by * 4 + 2 + yd);

				for (int xd = -3; xd <= 3; xd++)
				{
					const int x = wrap_x((int)bx * 4 + 2 + xd);

					total_error += map_pixel(x, y, orig_img(x, y), perceptual, alpha_is_significant);
				}
			}

			return total_error;
		}

		inline uint64_t evaluate_1x1_endpoint_error(uint32_t bx, uint32_t by, const image& orig_img, bool perceptual, bool alpha_is_significant, uint64_t threshold_error = 0) const
		{
			uint64_t total_error = 0;

			for (int yd = -3; yd <= 3; yd++)
			{
				const int y = wrap_y((int)by * 4 + 2 + yd);

				for (int xd = -3; xd <= 3; xd++)
				{
					const int x = wrap_x((int)bx * 4 + 2 + xd);

					total_error += color_distance(perceptual, get_pixel(x, y), orig_img(x, y), alpha_is_significant);

					if ((threshold_error) && (total_error >= threshold_error))
						return total_error;
				}
			}

			return total_error;
		}

		uint64_t local_endpoint_optimization_opaque(uint32_t bx, uint32_t by, const image& orig_img, bool perceptual);

		inline uint64_t map_all_pixels(const image& img, bool perceptual, bool alpha_is_significant)
		{
			assert(m_width == img.get_width());
			assert(m_height == img.get_height());

			uint64_t total_error = 0;
			for (uint32_t y = 0; y < img.get_height(); y++)
				for (uint32_t x = 0; x < img.get_width(); x++)
					total_error += map_pixel(x, y, img(x, y), perceptual, alpha_is_significant);

			return total_error;
		}
	
	public:						
		uint32_t m_width, m_height;
		pvrtc4_block_vector2D m_blocks;
		uint32_t m_block_width, m_block_height;
						
		bool m_uses_alpha;
	};

} // namespace basisu
