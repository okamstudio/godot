// basis_etc.h
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
#include "../transcoder/basisu.h"
#include "basisu_enc.h"

namespace basisu
{
	enum etc_constants
	{
		cETC1BytesPerBlock = 8U,

		cETC1SelectorBits = 2U,
		cETC1SelectorValues = 1U << cETC1SelectorBits,
		cETC1SelectorMask = cETC1SelectorValues - 1U,

		cETC1BlockShift = 2U,
		cETC1BlockSize = 1U << cETC1BlockShift,

		cETC1LSBSelectorIndicesBitOffset = 0,
		cETC1MSBSelectorIndicesBitOffset = 16,

		cETC1FlipBitOffset = 32,
		cETC1DiffBitOffset = 33,

		cETC1IntenModifierNumBits = 3,
		cETC1IntenModifierValues = 1 << cETC1IntenModifierNumBits,
		cETC1RightIntenModifierTableBitOffset = 34,
		cETC1LeftIntenModifierTableBitOffset = 37,

		// Base+Delta encoding (5 bit bases, 3 bit delta)
		cETC1BaseColorCompNumBits = 5,
		cETC1BaseColorCompMax = 1 << cETC1BaseColorCompNumBits,

		cETC1DeltaColorCompNumBits = 3,
		cETC1DeltaColorComp = 1 << cETC1DeltaColorCompNumBits,
		cETC1DeltaColorCompMax = 1 << cETC1DeltaColorCompNumBits,

		cETC1BaseColor5RBitOffset = 59,
		cETC1BaseColor5GBitOffset = 51,
		cETC1BaseColor5BBitOffset = 43,

		cETC1DeltaColor3RBitOffset = 56,
		cETC1DeltaColor3GBitOffset = 48,
		cETC1DeltaColor3BBitOffset = 40,

		// Absolute (non-delta) encoding (two 4-bit per component bases)
		cETC1AbsColorCompNumBits = 4,
		cETC1AbsColorCompMax = 1 << cETC1AbsColorCompNumBits,

		cETC1AbsColor4R1BitOffset = 60,
		cETC1AbsColor4G1BitOffset = 52,
		cETC1AbsColor4B1BitOffset = 44,

		cETC1AbsColor4R2BitOffset = 56,
		cETC1AbsColor4G2BitOffset = 48,
		cETC1AbsColor4B2BitOffset = 40,

		cETC1ColorDeltaMin = -4,
		cETC1ColorDeltaMax = 3,

		// Delta3:
		// 0   1   2   3   4   5   6   7
		// 000 001 010 011 100 101 110 111
		// 0   1   2   3   -4  -3  -2  -1
	};
	
	extern const int g_etc1_inten_tables[cETC1IntenModifierValues][cETC1SelectorValues];
	extern const uint8_t g_etc1_to_selector_index[cETC1SelectorValues];
	extern const uint8_t g_selector_index_to_etc1[cETC1SelectorValues];

	struct etc_coord2
	{
		uint8_t m_x, m_y;
	};
	extern const etc_coord2 g_etc1_pixel_coords[2][2][8]; // [flipped][subblock][subblock_pixel]
	extern const uint32_t g_etc1_pixel_indices[2][2][8]; // [flipped][subblock][subblock_pixel]

	struct etc_block
	{
		// big endian uint64:
		// bit ofs:  56  48  40  32  24  16   8   0
		// byte ofs: b0, b1, b2, b3, b4, b5, b6, b7 
		union
		{
			uint64_t m_uint64;

			uint8_t m_bytes[8];
		};

		inline void clear()
		{
			assert(sizeof(*this) == 8);
			clear_obj(*this);
		}

		inline uint64_t get_all_bits() const
		{
			return read_be64(&m_uint64);
		}

		inline uint32_t get_general_bits(uint32_t ofs, uint32_t num) const
		{
			assert((ofs + num) <= 64U);
			assert(num && (num < 32U));
			return (uint32_t)(read_be64(&m_uint64) >> ofs) & ((1UL << num) - 1UL);
		}

		inline void set_general_bits(uint32_t ofs, uint32_t num, uint32_t bits)
		{
			assert((ofs + num) <= 64U);
			assert(num && (num < 32U));

			uint64_t x = read_be64(&m_uint64);
			uint64_t msk = ((1ULL << static_cast<uint64_t>(num)) - 1ULL) << static_cast<uint64_t>(ofs);
			x &= ~msk;
			x |= (static_cast<uint64_t>(bits) << static_cast<uint64_t>(ofs));
			write_be64(&m_uint64, x);
		}

		inline uint32_t get_byte_bits(uint32_t ofs, uint32_t num) const
		{
			assert((ofs + num) <= 64U);
			assert(num && (num <= 8U));
			assert((ofs >> 3) == ((ofs + num - 1) >> 3));
			const uint32_t byte_ofs = 7 - (ofs >> 3);
			const uint32_t byte_bit_ofs = ofs & 7;
			return (m_bytes[byte_ofs] >> byte_bit_ofs) & ((1 << num) - 1);
		}

		inline void set_byte_bits(uint32_t ofs, uint32_t num, uint32_t bits)
		{
			assert((ofs + num) <= 64U);
			assert(num && (num < 32U));
			assert((ofs >> 3) == ((ofs + num - 1) >> 3));
			assert(bits < (1U << num));
			const uint32_t byte_ofs = 7 - (ofs >> 3);
			const uint32_t byte_bit_ofs = ofs & 7;
			const uint32_t mask = (1 << num) - 1;
			m_bytes[byte_ofs] &= ~(mask << byte_bit_ofs);
			m_bytes[byte_ofs] |= (bits << byte_bit_ofs);
		}

		// false = left/right subblocks
		// true = upper/lower subblocks
		inline bool get_flip_bit() const
		{
			return (m_bytes[3] & 1) != 0;
		}

		inline void set_flip_bit(bool flip)
		{
			m_bytes[3] &= ~1;
			m_bytes[3] |= static_cast<uint8_t>(flip);
		}

		inline bool get_diff_bit() const
		{
			return (m_bytes[3] & 2) != 0;
		}

		inline void set_diff_bit(bool diff)
		{
			m_bytes[3] &= ~2;
			m_bytes[3] |= (static_cast<uint32_t>(diff) << 1);
		}

		// Returns intensity modifier table (0-7) used by subblock subblock_id.
		// subblock_id=0 left/top (CW 1), 1=right/bottom (CW 2)
		inline uint32_t get_inten_table(uint32_t subblock_id) const
		{
			assert(subblock_id < 2);
			const uint32_t ofs = subblock_id ? 2 : 5;
			return (m_bytes[3] >> ofs) & 7;
		}

		// Sets intensity modifier table (0-7) used by subblock subblock_id (0 or 1)
		inline void set_inten_table(uint32_t subblock_id, uint32_t t)
		{
			assert(subblock_id < 2);
			assert(t < 8);
			const uint32_t ofs = subblock_id ? 2 : 5;
			m_bytes[3] &= ~(7 << ofs);
			m_bytes[3] |= (t << ofs);
		}

		inline void set_inten_tables_etc1s(uint32_t t)
		{
			set_inten_table(0, t);
			set_inten_table(1, t);
		}

		inline bool is_etc1s() const
		{
			if (get_inten_table(0) != get_inten_table(1))
				return false;

			if (get_diff_bit())
			{
				if (get_delta3_color() != 0)
					return false;
			}
			else
			{
				if (get_base4_color(0) != get_base4_color(1))
					return false;
			}

			return true;
		}

		// Returned encoded selector value ranges from 0-3 (this is NOT a direct index into g_etc1_inten_tables, see get_selector())
		inline uint32_t get_raw_selector(uint32_t x, uint32_t y) const
		{
			assert((x | y) < 4);

			const uint32_t bit_index = x * 4 + y;
			const uint32_t byte_bit_ofs = bit_index & 7;
			const uint8_t *p = &m_bytes[7 - (bit_index >> 3)];
			const uint32_t lsb = (p[0] >> byte_bit_ofs) & 1;
			const uint32_t msb = (p[-2] >> byte_bit_ofs) & 1;
			const uint32_t val = lsb | (msb << 1);

			return val;
		}

		// Returned selector value ranges from 0-3 and is a direct index into g_etc1_inten_tables.
		inline uint32_t get_selector(uint32_t x, uint32_t y) const
		{
			return g_etc1_to_selector_index[get_raw_selector(x, y)];
		}

		// Selector "val" ranges from 0-3 and is a direct index into g_etc1_inten_tables.
		inline void set_selector(uint32_t x, uint32_t y, uint32_t val)
		{
			assert((x | y | val) < 4);
			const uint32_t bit_index = x * 4 + y;

			uint8_t *p = &m_bytes[7 - (bit_index >> 3)];

			const uint32_t byte_bit_ofs = bit_index & 7;
			const uint32_t mask = 1 << byte_bit_ofs;

			const uint32_t etc1_val = g_selector_index_to_etc1[val];

			const uint32_t lsb = etc1_val & 1;
			const uint32_t msb = etc1_val >> 1;

			p[0] &= ~mask;
			p[0] |= (lsb << byte_bit_ofs);

			p[-2] &= ~mask;
			p[-2] |= (msb << byte_bit_ofs);
		}

		// Selector "etc1_val" ranges from 0-3 and is a direct (raw) ETC1 selector.
		inline void set_raw_selector(uint32_t x, uint32_t y, uint32_t etc1_val)
		{
			assert((x | y | etc1_val) < 4);
			const uint32_t bit_index = x * 4 + y;

			uint8_t* p = &m_bytes[7 - (bit_index >> 3)];

			const uint32_t byte_bit_ofs = bit_index & 7;
			const uint32_t mask = 1 << byte_bit_ofs;
						
			const uint32_t lsb = etc1_val & 1;
			const uint32_t msb = etc1_val >> 1;

			p[0] &= ~mask;
			p[0] |= (lsb << byte_bit_ofs);

			p[-2] &= ~mask;
			p[-2] |= (msb << byte_bit_ofs);
		}

		inline uint32_t get_raw_selector_bits() const
		{
			return m_bytes[4] | (m_bytes[5] << 8) | (m_bytes[6] << 16) | (m_bytes[7] << 24);
		}

		inline void set_raw_selector_bits(uint32_t bits)
		{
			m_bytes[4] = static_cast<uint8_t>(bits);
			m_bytes[5] = static_cast<uint8_t>(bits >> 8);
			m_bytes[6] = static_cast<uint8_t>(bits >> 16);
			m_bytes[7] = static_cast<uint8_t>(bits >> 24);
		}

		inline void set_raw_selector_bits(uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3)
		{
			m_bytes[4] = byte0;
			m_bytes[5] = byte1;
			m_bytes[6] = byte2;
			m_bytes[7] = byte3;
		}

		inline void set_base4_color(uint32_t idx, uint16_t c)
		{
			if (idx)
			{
				set_byte_bits(cETC1AbsColor4R2BitOffset, 4, (c >> 8) & 15);
				set_byte_bits(cETC1AbsColor4G2BitOffset, 4, (c >> 4) & 15);
				set_byte_bits(cETC1AbsColor4B2BitOffset, 4, c & 15);
			}
			else
			{
				set_byte_bits(cETC1AbsColor4R1BitOffset, 4, (c >> 8) & 15);
				set_byte_bits(cETC1AbsColor4G1BitOffset, 4, (c >> 4) & 15);
				set_byte_bits(cETC1AbsColor4B1BitOffset, 4, c & 15);
			}
		}

		inline uint16_t get_base4_color(uint32_t idx) const
		{
			uint32_t r, g, b;
			if (idx)
			{
				r = get_byte_bits(cETC1AbsColor4R2BitOffset, 4);
				g = get_byte_bits(cETC1AbsColor4G2BitOffset, 4);
				b = get_byte_bits(cETC1AbsColor4B2BitOffset, 4);
			}
			else
			{
				r = get_byte_bits(cETC1AbsColor4R1BitOffset, 4);
				g = get_byte_bits(cETC1AbsColor4G1BitOffset, 4);
				b = get_byte_bits(cETC1AbsColor4B1BitOffset, 4);
			}
			return static_cast<uint16_t>(b | (g << 4U) | (r << 8U));
		}

		inline void set_base5_color(uint16_t c)
		{
			set_byte_bits(cETC1BaseColor5RBitOffset, 5, (c >> 10) & 31);
			set_byte_bits(cETC1BaseColor5GBitOffset, 5, (c >> 5) & 31);
			set_byte_bits(cETC1BaseColor5BBitOffset, 5, c & 31);
		}

		inline uint16_t get_base5_color() const
		{
			const uint32_t r = get_byte_bits(cETC1BaseColor5RBitOffset, 5);
			const uint32_t g = get_byte_bits(cETC1BaseColor5GBitOffset, 5);
			const uint32_t b = get_byte_bits(cETC1BaseColor5BBitOffset, 5);
			return static_cast<uint16_t>(b | (g << 5U) | (r << 10U));
		}

		void set_delta3_color(uint16_t c)
		{
			set_byte_bits(cETC1DeltaColor3RBitOffset, 3, (c >> 6) & 7);
			set_byte_bits(cETC1DeltaColor3GBitOffset, 3, (c >> 3) & 7);
			set_byte_bits(cETC1DeltaColor3BBitOffset, 3, c & 7);
		}

		inline uint16_t get_delta3_color() const
		{
			const uint32_t r = get_byte_bits(cETC1DeltaColor3RBitOffset, 3);
			const uint32_t g = get_byte_bits(cETC1DeltaColor3GBitOffset, 3);
			const uint32_t b = get_byte_bits(cETC1DeltaColor3BBitOffset, 3);
			return static_cast<uint16_t>(b | (g << 3U) | (r << 6U));
		}

		uint64_t determine_selectors(const color_rgba* pSource_pixels, bool perceptual, uint32_t begin_subblock = 0, uint32_t end_subblock = 2)
		{
			uint64_t total_error = 0;

			for (uint32_t subblock = begin_subblock; subblock < end_subblock; subblock++)
			{
				color_rgba block_colors[4];
				get_block_colors(block_colors, subblock);

				if (get_flip_bit())
				{
					for (uint32_t y = 0; y < 2; y++)
					{
						for (uint32_t x = 0; x < 4; x++)
						{
							uint32_t best_selector = 0;
							uint64_t best_error = UINT64_MAX;

							for (uint32_t s = 0; s < 4; s++)
							{
								uint64_t err = color_distance(perceptual, block_colors[s], pSource_pixels[x + (subblock * 2 + y) * 4], false);
								if (err < best_error)
								{
									best_error = err;
									best_selector = s;
								}
							}

							set_selector(x, subblock * 2 + y, best_selector);

							total_error += best_error;
						}
					}
				}
				else
				{
					for (uint32_t y = 0; y < 4; y++)
					{
						for (uint32_t x = 0; x < 2; x++)
						{
							uint32_t best_selector = 0;
							uint64_t best_error = UINT64_MAX;

							for (uint32_t s = 0; s < 4; s++)
							{
								uint64_t err = color_distance(perceptual, block_colors[s], pSource_pixels[(subblock * 2) + x + y * 4], false);
								if (err < best_error)
								{
									best_error = err;
									best_selector = s;
								}
							}

							set_selector(subblock * 2 + x, y, best_selector);

							total_error += best_error;
						}
					}
				}
			}

			return total_error;
		}

		color_rgba get_block_color(uint32_t subblock_index, bool scaled) const
		{
			color_rgba b;

			if (get_diff_bit())
			{
				if (subblock_index)
					unpack_color5(b, get_base5_color(), get_delta3_color(), scaled);
				else
					unpack_color5(b, get_base5_color(), scaled);
			}
			else
			{
				b = unpack_color4(get_base4_color(subblock_index), scaled);
			}

			return b;
		}

		uint32_t get_subblock_index(uint32_t x, uint32_t y) const
		{
			if (get_flip_bit())
				return y >= 2;
			else
				return x >= 2;
		}

		bool get_block_colors(color_rgba* pBlock_colors, uint32_t subblock_index) const
		{
			color_rgba b;

			if (get_diff_bit())
			{
				if (subblock_index)
					unpack_color5(b, get_base5_color(), get_delta3_color(), true);
				else
					unpack_color5(b, get_base5_color(), true);
			}
			else
			{
				b = unpack_color4(get_base4_color(subblock_index), true);
			}

			const int* pInten_table = g_etc1_inten_tables[get_inten_table(subblock_index)];

			bool dc = false;

			pBlock_colors[0].set(clamp255(b.r + pInten_table[0], dc), clamp255(b.g + pInten_table[0], dc), clamp255(b.b + pInten_table[0], dc), 255);
			pBlock_colors[1].set(clamp255(b.r + pInten_table[1], dc), clamp255(b.g + pInten_table[1], dc), clamp255(b.b + pInten_table[1], dc), 255);
			pBlock_colors[2].set(clamp255(b.r + pInten_table[2], dc), clamp255(b.g + pInten_table[2], dc), clamp255(b.b + pInten_table[2], dc), 255);
			pBlock_colors[3].set(clamp255(b.r + pInten_table[3], dc), clamp255(b.g + pInten_table[3], dc), clamp255(b.b + pInten_table[3], dc), 255);

			return dc;
		}

		void get_block_colors_etc1s(color_rgba* pBlock_colors) const
		{
			color_rgba b;

			unpack_color5(b, get_base5_color(), true);

			const int* pInten_table = g_etc1_inten_tables[get_inten_table(0)];

			pBlock_colors[0].set(clamp255(b.r + pInten_table[0]), clamp255(b.g + pInten_table[0]), clamp255(b.b + pInten_table[0]), 255);
			pBlock_colors[1].set(clamp255(b.r + pInten_table[1]), clamp255(b.g + pInten_table[1]), clamp255(b.b + pInten_table[1]), 255);
			pBlock_colors[2].set(clamp255(b.r + pInten_table[2]), clamp255(b.g + pInten_table[2]), clamp255(b.b + pInten_table[2]), 255);
			pBlock_colors[3].set(clamp255(b.r + pInten_table[3]), clamp255(b.g + pInten_table[3]), clamp255(b.b + pInten_table[3]), 255);
		}

		static void get_block_colors_etc1s(color_rgba* pBlock_colors, const color_rgba &base5_color, uint32_t inten_table)
		{
			color_rgba b;
			b.r = (base5_color.r << 3U) | (base5_color.r >> 2U);
			b.g = (base5_color.g << 3U) | (base5_color.g >> 2U);
			b.b = (base5_color.b << 3U) | (base5_color.b >> 2U);
						
			const int* pInten_table = g_etc1_inten_tables[inten_table];

			pBlock_colors[0].set(clamp255(b.r + pInten_table[0]), clamp255(b.g + pInten_table[0]), clamp255(b.b + pInten_table[0]), 255);
			pBlock_colors[1].set(clamp255(b.r + pInten_table[1]), clamp255(b.g + pInten_table[1]), clamp255(b.b + pInten_table[1]), 255);
			pBlock_colors[2].set(clamp255(b.r + pInten_table[2]), clamp255(b.g + pInten_table[2]), clamp255(b.b + pInten_table[2]), 255);
			pBlock_colors[3].set(clamp255(b.r + pInten_table[3]), clamp255(b.g + pInten_table[3]), clamp255(b.b + pInten_table[3]), 255);
		}

		void get_block_color(color_rgba& color, uint32_t subblock_index, uint32_t selector_index) const
		{
			color_rgba b;

			if (get_diff_bit())
			{
				if (subblock_index)
					unpack_color5(b, get_base5_color(), get_delta3_color(), true);
				else
					unpack_color5(b, get_base5_color(), true);
			}
			else
			{
				b = unpack_color4(get_base4_color(subblock_index), true);
			}

			const int* pInten_table = g_etc1_inten_tables[get_inten_table(subblock_index)];

			color.set(clamp255(b.r + pInten_table[selector_index]), clamp255(b.g + pInten_table[selector_index]), clamp255(b.b + pInten_table[selector_index]), 255);
		}

		bool get_block_low_high_colors(color_rgba* pBlock_colors, uint32_t subblock_index) const
		{
			color_rgba b;

			if (get_diff_bit())
			{
				if (subblock_index)
					unpack_color5(b, get_base5_color(), get_delta3_color(), true);
				else
					unpack_color5(b, get_base5_color(), true);
			}
			else
			{
				b = unpack_color4(get_base4_color(subblock_index), true);
			}

			const int* pInten_table = g_etc1_inten_tables[get_inten_table(subblock_index)];

			bool dc = false;

			pBlock_colors[0].set(clamp255(b.r + pInten_table[0], dc), clamp255(b.g + pInten_table[0], dc), clamp255(b.b + pInten_table[0], dc), 255);
			pBlock_colors[1].set(clamp255(b.r + pInten_table[3], dc), clamp255(b.g + pInten_table[3], dc), clamp255(b.b + pInten_table[3], dc), 255);

			return dc;
		}

		static void get_block_colors5(color_rgba *pBlock_colors, const color_rgba &base_color5, uint32_t inten_table, bool scaled = false)
		{
			color_rgba b(base_color5);

			if (!scaled)
			{
				b.r = (b.r << 3) | (b.r >> 2);
				b.g = (b.g << 3) | (b.g >> 2);
				b.b = (b.b << 3) | (b.b >> 2);
			}

			const int* pInten_table = g_etc1_inten_tables[inten_table];

			pBlock_colors[0].set(clamp255(b.r + pInten_table[0]), clamp255(b.g + pInten_table[0]), clamp255(b.b + pInten_table[0]), 255);
			pBlock_colors[1].set(clamp255(b.r + pInten_table[1]), clamp255(b.g + pInten_table[1]), clamp255(b.b + pInten_table[1]), 255);
			pBlock_colors[2].set(clamp255(b.r + pInten_table[2]), clamp255(b.g + pInten_table[2]), clamp255(b.b + pInten_table[2]), 255);
			pBlock_colors[3].set(clamp255(b.r + pInten_table[3]), clamp255(b.g + pInten_table[3]), clamp255(b.b + pInten_table[3]), 255);
		}

		static void get_block_colors4(color_rgba *pBlock_colors, const color_rgba &base_color4, uint32_t inten_table, bool scaled = false)
		{
			color_rgba b(base_color4);

			if (!scaled)
			{
				b.r = (b.r << 4) | b.r;
				b.g = (b.g << 4) | b.g;
				b.b = (b.b << 4) | b.b;
			}

			const int* pInten_table = g_etc1_inten_tables[inten_table];

			pBlock_colors[0].set(clamp255(b.r + pInten_table[0]), clamp255(b.g + pInten_table[0]), clamp255(b.b + pInten_table[0]), 255);
			pBlock_colors[1].set(clamp255(b.r + pInten_table[1]), clamp255(b.g + pInten_table[1]), clamp255(b.b + pInten_table[1]), 255);
			pBlock_colors[2].set(clamp255(b.r + pInten_table[2]), clamp255(b.g + pInten_table[2]), clamp255(b.b + pInten_table[2]), 255);
			pBlock_colors[3].set(clamp255(b.r + pInten_table[3]), clamp255(b.g + pInten_table[3]), clamp255(b.b + pInten_table[3]), 255);
		}

		uint64_t evaluate_etc1_error(const color_rgba* pBlock_pixels, bool perceptual, int subblock_index = -1) const;
		void get_subblock_pixels(color_rgba* pPixels, int subblock_index = -1) const;

		void get_selector_range(uint32_t& low, uint32_t& high) const
		{
			low = 3;
			high = 0;
			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t s = get_selector(x, y);
					low = minimum(low, s);
					high = maximum(high, s);
				}
			}
		}

		void set_block_color4(const color_rgba &c0_unscaled, const color_rgba &c1_unscaled)
		{
			set_diff_bit(false);

			set_base4_color(0, pack_color4(c0_unscaled, false));
			set_base4_color(1, pack_color4(c1_unscaled, false));
		}

		void set_block_color5(const color_rgba &c0_unscaled, const color_rgba &c1_unscaled)
		{
			set_diff_bit(true);

			set_base5_color(pack_color5(c0_unscaled, false));

			int dr = c1_unscaled.r - c0_unscaled.r;
			int dg = c1_unscaled.g - c0_unscaled.g;
			int db = c1_unscaled.b - c0_unscaled.b;

			set_delta3_color(pack_delta3(dr, dg, db));
		}

		void set_block_color5_etc1s(const color_rgba &c_unscaled)
		{
			set_diff_bit(true);
			
			set_base5_color(pack_color5(c_unscaled, false));
			set_delta3_color(pack_delta3(0, 0, 0));
		}

		bool set_block_color5_check(const color_rgba &c0_unscaled, const color_rgba &c1_unscaled)
		{
			set_diff_bit(true);

			set_base5_color(pack_color5(c0_unscaled, false));

			int dr = c1_unscaled.r - c0_unscaled.r;
			int dg = c1_unscaled.g - c0_unscaled.g;
			int db = c1_unscaled.b - c0_unscaled.b;

			if (((dr < cETC1ColorDeltaMin) || (dr > cETC1ColorDeltaMax)) ||
				((dg < cETC1ColorDeltaMin) || (dg > cETC1ColorDeltaMax)) ||
				((db < cETC1ColorDeltaMin) || (db > cETC1ColorDeltaMax)))
				return false;

			set_delta3_color(pack_delta3(dr, dg, db));

			return true;
		}

		bool set_block_color5_clamp(const color_rgba &c0_unscaled, const color_rgba &c1_unscaled)
		{
			set_diff_bit(true);
			set_base5_color(pack_color5(c0_unscaled, false));

			int dr = c1_unscaled.r - c0_unscaled.r;
			int dg = c1_unscaled.g - c0_unscaled.g;
			int db = c1_unscaled.b - c0_unscaled.b;
			
			dr = clamp<int>(dr, cETC1ColorDeltaMin, cETC1ColorDeltaMax);
			dg = clamp<int>(dg, cETC1ColorDeltaMin, cETC1ColorDeltaMax);
			db = clamp<int>(db, cETC1ColorDeltaMin, cETC1ColorDeltaMax);
						
			set_delta3_color(pack_delta3(dr, dg, db));

			return true;
		}
		color_rgba get_selector_color(uint32_t x, uint32_t y, uint32_t s) const
		{
			color_rgba block_colors[4];

			get_block_colors(block_colors, get_subblock_index(x, y));

			return block_colors[s];
		}

		// Base color 5
		static uint16_t pack_color5(const color_rgba& color, bool scaled, uint32_t bias = 127U);
		static uint16_t pack_color5(uint32_t r, uint32_t g, uint32_t b, bool scaled, uint32_t bias = 127U);

		static color_rgba unpack_color5(uint16_t packed_color5, bool scaled, uint32_t alpha = 255U);
		static void unpack_color5(uint32_t& r, uint32_t& g, uint32_t& b, uint16_t packed_color, bool scaled);
		static void unpack_color5(color_rgba& result, uint16_t packed_color5, bool scaled);

		static bool unpack_color5(color_rgba& result, uint16_t packed_color5, uint16_t packed_delta3, bool scaled, uint32_t alpha = 255U);
		static bool unpack_color5(uint32_t& r, uint32_t& g, uint32_t& b, uint16_t packed_color5, uint16_t packed_delta3, bool scaled, uint32_t alpha = 255U);

		// Delta color 3
		// Inputs range from -4 to 3 (cETC1ColorDeltaMin to cETC1ColorDeltaMax)
		static uint16_t pack_delta3(const color_rgba_i16& color);
		static uint16_t pack_delta3(int r, int g, int b);

		// Results range from -4 to 3 (cETC1ColorDeltaMin to cETC1ColorDeltaMax)
		static color_rgba_i16 unpack_delta3(uint16_t packed_delta3);
		static void unpack_delta3(int& r, int& g, int& b, uint16_t packed_delta3);

		static bool try_pack_color5_delta3(const color_rgba *pColor5_unscaled)
		{
			int dr = pColor5_unscaled[1].r - pColor5_unscaled[0].r;
			int dg = pColor5_unscaled[1].g - pColor5_unscaled[0].g;
			int db = pColor5_unscaled[1].b - pColor5_unscaled[0].b;

			if ((minimum(dr, dg, db) < cETC1ColorDeltaMin) || (maximum(dr, dg, db) > cETC1ColorDeltaMax))
				return false;

			return true;
		}

		// Abs color 4
		static uint16_t pack_color4(const color_rgba& color, bool scaled, uint32_t bias = 127U);
		static uint16_t pack_color4(uint32_t r, uint32_t g, uint32_t b, bool scaled, uint32_t bias = 127U);

		static color_rgba unpack_color4(uint16_t packed_color4, bool scaled, uint32_t alpha = 255U);
		static void unpack_color4(uint32_t& r, uint32_t& g, uint32_t& b, uint16_t packed_color4, bool scaled);

		// subblock colors
		static void get_diff_subblock_colors(color_rgba* pDst, uint16_t packed_color5, uint32_t table_idx);
		static bool get_diff_subblock_colors(color_rgba* pDst, uint16_t packed_color5, uint16_t packed_delta3, uint32_t table_idx);
		static void get_abs_subblock_colors(color_rgba* pDst, uint16_t packed_color4, uint32_t table_idx);

		static inline void unscaled_to_scaled_color(color_rgba& dst, const color_rgba& src, bool color4)
		{
			if (color4)
			{
				dst.r = src.r | (src.r << 4);
				dst.g = src.g | (src.g << 4);
				dst.b = src.b | (src.b << 4);
			}
			else
			{
				dst.r = (src.r >> 2) | (src.r << 3);
				dst.g = (src.g >> 2) | (src.g << 3);
				dst.b = (src.b >> 2) | (src.b << 3);
			}
			dst.a = src.a;
		}

	private:
		static uint8_t clamp255(int x, bool &did_clamp)
		{
			if (x < 0)
			{
				did_clamp = true;
				return 0;
			}
			else if (x > 255)
			{
				did_clamp = true;
				return 255;
			}

			return static_cast<uint8_t>(x);
		}

		static uint8_t clamp255(int x)
		{
			if (x < 0)
				return 0;
			else if (x > 255)
				return 255;

			return static_cast<uint8_t>(x);
		}
	};
		
	typedef basisu::vector<etc_block> etc_block_vec;

	// Returns false if the unpack fails (could be bogus data or ETC2)
	bool unpack_etc1(const etc_block& block, color_rgba *pDst, bool preserve_alpha = false);
		
	enum basis_etc_quality
	{
		cETCQualityFast,
		cETCQualityMedium,
		cETCQualitySlow,
		cETCQualityUber,
		cETCQualityTotal,
	};

	struct basis_etc1_pack_params
	{
		basis_etc_quality m_quality;
		bool m_perceptual;
		bool m_cluster_fit;
		bool m_force_etc1s;
		bool m_use_color4;
		float m_flip_bias;

		inline basis_etc1_pack_params()
		{
			clear();
		}

		void clear()
		{
			m_quality = cETCQualitySlow;
			m_perceptual = true;
			m_cluster_fit = true;
			m_force_etc1s = false;
			m_use_color4 = true;
			m_flip_bias = 0.0f;
		}
	};

	struct etc1_solution_coordinates
	{
		inline etc1_solution_coordinates() :
			m_unscaled_color(0, 0, 0, 0),
			m_inten_table(0),
			m_color4(false)
		{
		}

		inline etc1_solution_coordinates(uint32_t r, uint32_t g, uint32_t b, uint32_t inten_table, bool color4) :
			m_unscaled_color((uint8_t)r, (uint8_t)g, (uint8_t)b, 255),
			m_inten_table((uint8_t)inten_table),
			m_color4(color4)
		{
		}

		inline etc1_solution_coordinates(const color_rgba& c, uint32_t inten_table, bool color4) :
			m_unscaled_color(c),
			m_inten_table(inten_table),
			m_color4(color4)
		{
		}

		inline etc1_solution_coordinates(const etc1_solution_coordinates& other)
		{
			*this = other;
		}

		inline etc1_solution_coordinates& operator= (const etc1_solution_coordinates& rhs)
		{
			m_unscaled_color = rhs.m_unscaled_color;
			m_inten_table = rhs.m_inten_table;
			m_color4 = rhs.m_color4;
			return *this;
		}

		inline void clear()
		{
			m_unscaled_color.clear();
			m_inten_table = 0;
			m_color4 = false;
		}

		inline void init(const color_rgba& c, uint32_t inten_table, bool color4)
		{
			m_unscaled_color = c;
			m_inten_table = inten_table;
			m_color4 = color4;
		}

		inline color_rgba get_scaled_color() const
		{
			int br, bg, bb;
			if (m_color4)
			{
				br = m_unscaled_color.r | (m_unscaled_color.r << 4);
				bg = m_unscaled_color.g | (m_unscaled_color.g << 4);
				bb = m_unscaled_color.b | (m_unscaled_color.b << 4);
			}
			else
			{
				br = (m_unscaled_color.r >> 2) | (m_unscaled_color.r << 3);
				bg = (m_unscaled_color.g >> 2) | (m_unscaled_color.g << 3);
				bb = (m_unscaled_color.b >> 2) | (m_unscaled_color.b << 3);
			}
			return color_rgba((uint8_t)br, (uint8_t)bg, (uint8_t)bb, 255);
		}

		// returns true if anything was clamped
		inline void get_block_colors(color_rgba* pBlock_colors)
		{
			int br, bg, bb;
			if (m_color4)
			{
				br = m_unscaled_color.r | (m_unscaled_color.r << 4);
				bg = m_unscaled_color.g | (m_unscaled_color.g << 4);
				bb = m_unscaled_color.b | (m_unscaled_color.b << 4);
			}
			else
			{
				br = (m_unscaled_color.r >> 2) | (m_unscaled_color.r << 3);
				bg = (m_unscaled_color.g >> 2) | (m_unscaled_color.g << 3);
				bb = (m_unscaled_color.b >> 2) | (m_unscaled_color.b << 3);
			}
			const int* pInten_table = g_etc1_inten_tables[m_inten_table];
			pBlock_colors[0].set(br + pInten_table[0], bg + pInten_table[0], bb + pInten_table[0], 255);
			pBlock_colors[1].set(br + pInten_table[1], bg + pInten_table[1], bb + pInten_table[1], 255);
			pBlock_colors[2].set(br + pInten_table[2], bg + pInten_table[2], bb + pInten_table[2], 255);
			pBlock_colors[3].set(br + pInten_table[3], bg + pInten_table[3], bb + pInten_table[3], 255);
		}

		color_rgba m_unscaled_color;
		uint32_t m_inten_table;
		bool m_color4;
	};

	class etc1_optimizer
	{
		BASISU_NO_EQUALS_OR_COPY_CONSTRUCT(etc1_optimizer);

	public:
		etc1_optimizer()
		{
			clear();
		}

		void clear()
		{
			m_pParams = nullptr;
			m_pResult = nullptr;
			m_pSorted_luma = nullptr;
			m_pSorted_luma_indices = nullptr;
		}

		struct params;

		typedef bool(*evaluate_solution_override_func)(uint64_t &error, const params &p, const color_rgba* pBlock_colors, const uint8_t* pSelectors, const etc1_solution_coordinates& coords);

		struct params : basis_etc1_pack_params
		{
			params()
			{
				clear();
			}

			params(const basis_etc1_pack_params& base_params)
			{
				clear_optimizer_params();

				*static_cast<basis_etc1_pack_params *>(this) = base_params;
			}

			void clear()
			{
				clear_optimizer_params();
			}

			void clear_optimizer_params()
			{
				basis_etc1_pack_params::clear();

				m_num_src_pixels = 0;
				m_pSrc_pixels = 0;

				m_use_color4 = false;
				static const int s_default_scan_delta[] = { 0 };
				m_pScan_deltas = s_default_scan_delta;
				m_scan_delta_size = 1;

				m_base_color5.clear();
				m_constrain_against_base_color5 = false;

				m_refinement = true;

				m_pForce_selectors = nullptr;
			}

			uint32_t m_num_src_pixels;
			const color_rgba* m_pSrc_pixels;

			bool m_use_color4;
			const int* m_pScan_deltas;
			uint32_t m_scan_delta_size;

			color_rgba m_base_color5;
			bool m_constrain_against_base_color5;

			bool m_refinement;

			const uint8_t* m_pForce_selectors;
		};

		struct results
		{
			uint64_t m_error;
			color_rgba m_block_color_unscaled;
			uint32_t m_block_inten_table;
			uint32_t m_n;
			uint8_t* m_pSelectors;
			bool m_block_color4;

			inline results& operator= (const results& rhs)
			{
				m_block_color_unscaled = rhs.m_block_color_unscaled;
				m_block_color4 = rhs.m_block_color4;
				m_block_inten_table = rhs.m_block_inten_table;
				m_error = rhs.m_error;
				memcpy(m_pSelectors, rhs.m_pSelectors, minimum(rhs.m_n, m_n));
				return *this;
			}
		};

		void init(const params& params, results& result);
		bool compute();

		const params* get_params() const { return m_pParams; }

	private:
		struct potential_solution
		{
			potential_solution() : m_coords(), m_error(UINT64_MAX), m_valid(false)
			{
			}

			etc1_solution_coordinates  m_coords;
			basisu::vector<uint8_t>    m_selectors;
			uint64_t                     m_error;
			bool                       m_valid;

			void clear()
			{
				m_coords.clear();
				m_selectors.resize(0);
				m_error = UINT64_MAX;
				m_valid = false;
			}

			bool are_selectors_all_equal() const
			{
				if (!m_selectors.size())
					return false;
				const uint32_t s = m_selectors[0];
				for (uint32_t i = 1; i < m_selectors.size(); i++)
					if (m_selectors[i] != s)
						return false;
				return true;
			}
		};

		const params* m_pParams;
		results* m_pResult;

		int m_limit;

		vec3F m_avg_color;
		int m_br, m_bg, m_bb;
		int m_max_comp_spread;
		basisu::vector<uint16_t> m_luma;
		basisu::vector<uint32_t> m_sorted_luma;
		basisu::vector<uint32_t> m_sorted_luma_indices;
		const uint32_t* m_pSorted_luma_indices;
		uint32_t* m_pSorted_luma;

		basisu::vector<uint8_t> m_selectors;
		basisu::vector<uint8_t> m_best_selectors;

		potential_solution m_best_solution;
		potential_solution m_trial_solution;
		basisu::vector<uint8_t> m_temp_selectors;

		enum { cSolutionsTriedHashBits = 10, cTotalSolutionsTriedHashSize = 1 << cSolutionsTriedHashBits, cSolutionsTriedHashMask = cTotalSolutionsTriedHashSize - 1 };
		uint8_t m_solutions_tried[cTotalSolutionsTriedHashSize / 8];
		
		void get_nearby_inten_tables(uint32_t idx, int &first_inten_table, int &last_inten_table)
		{
			first_inten_table = maximum<int>(idx - 1, 0);
			last_inten_table = minimum<int>(cETC1IntenModifierValues, idx + 1);
		}
		
		bool check_for_redundant_solution(const etc1_solution_coordinates& coords);
		bool evaluate_solution_slow(const etc1_solution_coordinates& coords, potential_solution& trial_solution, potential_solution* pBest_solution);
		bool evaluate_solution_fast(const etc1_solution_coordinates& coords, potential_solution& trial_solution, potential_solution* pBest_solution);

		inline bool evaluate_solution(const etc1_solution_coordinates& coords, potential_solution& trial_solution, potential_solution* pBest_solution)
		{
			if (m_pParams->m_quality >= cETCQualityMedium)
				return evaluate_solution_slow(coords, trial_solution, pBest_solution);
			else
				return evaluate_solution_fast(coords, trial_solution, pBest_solution);
		}

		void refine_solution(uint32_t max_refinement_trials);
		void compute_internal_neighborhood(int scan_r, int scan_g, int scan_b);
		void compute_internal_cluster_fit(uint32_t total_perms_to_try);
	};

	struct pack_etc1_block_context
	{
		etc1_optimizer m_optimizer;
	};
	
	void pack_etc1_solid_color_init();
	uint64_t pack_etc1_block_solid_color(etc_block& block, const uint8_t* pColor);

	// ETC EAC
	extern const int8_t g_etc2_eac_tables[16][8];
	extern const int8_t g_etc2_eac_tables8[16][8];

	const uint32_t ETC2_EAC_MIN_VALUE_SELECTOR = 3, ETC2_EAC_MAX_VALUE_SELECTOR = 7;

	struct eac_a8_block
	{
		uint16_t m_base : 8;
		uint16_t m_table : 4;
		uint16_t m_multiplier : 4;

		uint8_t m_selectors[6];

		inline uint32_t get_selector(uint32_t x, uint32_t y, uint64_t selector_bits) const
		{
			assert((x < 4) && (y < 4));
			return static_cast<uint32_t>((selector_bits >> (45 - (y + x * 4) * 3)) & 7);
		}

		inline uint64_t get_selector_bits() const
		{
			uint64_t pixels = ((uint64_t)m_selectors[0] << 40) | ((uint64_t)m_selectors[1] << 32) | ((uint64_t)m_selectors[2] << 24) | ((uint64_t)m_selectors[3] << 16) | ((uint64_t)m_selectors[4] << 8) | m_selectors[5];
			return pixels;
		}

		inline void set_selector_bits(uint64_t pixels)
		{
			m_selectors[0] = (uint8_t)(pixels >> 40);
			m_selectors[1] = (uint8_t)(pixels >> 32);
			m_selectors[2] = (uint8_t)(pixels >> 24);
			m_selectors[3] = (uint8_t)(pixels >> 16);
			m_selectors[4] = (uint8_t)(pixels >> 8);
			m_selectors[5] = (uint8_t)(pixels);
		}

		void set_selector(uint32_t x, uint32_t y, uint32_t s)
		{
			assert((x < 4) && (y < 4) && (s < 8));

			const uint32_t ofs = 45 - (y + x * 4) * 3;

			uint64_t pixels = get_selector_bits();

			pixels &= ~(7ULL << ofs);
			pixels |= (static_cast<uint64_t>(s) << ofs);

			set_selector_bits(pixels);
		}
	};

	struct etc2_rgba_block
	{
		eac_a8_block m_alpha;
		etc_block m_rgb;
	};

	struct pack_eac_a8_results
	{
		uint32_t m_base;
		uint32_t m_table;
		uint32_t m_multiplier;
		uint8_vec m_selectors;
		uint8_vec m_selectors_temp;
	};

	uint64_t pack_eac_a8(pack_eac_a8_results& results, const uint8_t* pPixels, uint32_t num_pixels, uint32_t base_search_rad, uint32_t mul_search_rad, uint32_t table_mask = UINT32_MAX);
	void pack_eac_a8(eac_a8_block* pBlock, const uint8_t* pPixels, uint32_t base_search_rad, uint32_t mul_search_rad, uint32_t table_mask = UINT32_MAX);
		
} // namespace basisu
