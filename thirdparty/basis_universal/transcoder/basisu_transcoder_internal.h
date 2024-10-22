// basisu_transcoder_internal.h - Universal texture format transcoder library.
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
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

#ifdef _MSC_VER
#pragma warning (disable: 4127) //  conditional expression is constant
#endif

// v1.50: Added UASTC HDR support
#define BASISD_LIB_VERSION 150
#define BASISD_VERSION_STRING "01.50"

#ifdef _DEBUG
#define BASISD_BUILD_DEBUG
#else
#define BASISD_BUILD_RELEASE
#endif

#include "basisu.h"

#define BASISD_znew (z = 36969 * (z & 65535) + (z >> 16))

namespace basisu
{
	extern bool g_debug_printf;
}

namespace basist
{
	// Low-level formats directly supported by the transcoder (other supported texture formats are combinations of these low-level block formats).
	// You probably don't care about these enum's unless you are going pretty low-level and calling the transcoder to decode individual slices.
	enum class block_format
	{
		cETC1,								// ETC1S RGB 
		cETC2_RGBA,							// full ETC2 EAC RGBA8 block
		cBC1,								// DXT1 RGB 
		cBC3,								// BC4 block followed by a four color BC1 block
		cBC4,								// DXT5A (alpha block only)
		cBC5,								// two BC4 blocks
		cPVRTC1_4_RGB,						// opaque-only PVRTC1 4bpp
		cPVRTC1_4_RGBA,						// PVRTC1 4bpp RGBA
		cBC7,								// Full BC7 block, any mode
		cBC7_M5_COLOR,						// RGB BC7 mode 5 color (writes an opaque mode 5 block)
		cBC7_M5_ALPHA,						// alpha portion of BC7 mode 5 (cBC7_M5_COLOR output data must have been written to the output buffer first to set the mode/rot fields etc.)
		cETC2_EAC_A8,						// alpha block of ETC2 EAC (first 8 bytes of the 16-bit ETC2 EAC RGBA format)
		cASTC_4x4,							// ASTC 4x4 (either color-only or color+alpha). Note that the transcoder always currently assumes sRGB is not enabled when outputting ASTC 
											// data. If you use a sRGB ASTC format you'll get ~1 LSB of additional error, because of the different way ASTC decoders scale 8-bit endpoints to 16-bits during unpacking.
		
		cATC_RGB,
		cATC_RGBA_INTERPOLATED_ALPHA,
		cFXT1_RGB,							// Opaque-only, has oddball 8x4 pixel block size

		cPVRTC2_4_RGB,
		cPVRTC2_4_RGBA,

		cETC2_EAC_R11,
		cETC2_EAC_RG11,
												
		cIndices,							// Used internally: Write 16-bit endpoint and selector indices directly to output (output block must be at least 32-bits)

		cRGB32,								// Writes RGB components to 32bpp output pixels
		cRGBA32,							// Writes RGB255 components to 32bpp output pixels
		cA32,								// Writes alpha component to 32bpp output pixels
				
		cRGB565,
		cBGR565,
		
		cRGBA4444_COLOR,
		cRGBA4444_ALPHA,
		cRGBA4444_COLOR_OPAQUE,
		cRGBA4444,
		cRGBA_HALF,
		cRGB_HALF,
		cRGB_9E5,

		cUASTC_4x4,							// LDR, universal
		cUASTC_HDR_4x4,						// HDR, transcodes only to 4x4 HDR ASTC, BC6H, or uncompressed
		cBC6H,
		cASTC_HDR_4x4,
								
		cTotalBlockFormats
	};

	const int COLOR5_PAL0_PREV_HI = 9, COLOR5_PAL0_DELTA_LO = -9, COLOR5_PAL0_DELTA_HI = 31;
	const int COLOR5_PAL1_PREV_HI = 21, COLOR5_PAL1_DELTA_LO = -21, COLOR5_PAL1_DELTA_HI = 21;
	const int COLOR5_PAL2_PREV_HI = 31, COLOR5_PAL2_DELTA_LO = -31, COLOR5_PAL2_DELTA_HI = 9;
	const int COLOR5_PAL_MIN_DELTA_B_RUNLEN = 3, COLOR5_PAL_DELTA_5_RUNLEN_VLC_BITS = 3;

	const uint32_t ENDPOINT_PRED_TOTAL_SYMBOLS = (4 * 4 * 4 * 4) + 1;
	const uint32_t ENDPOINT_PRED_REPEAT_LAST_SYMBOL = ENDPOINT_PRED_TOTAL_SYMBOLS - 1;
	const uint32_t ENDPOINT_PRED_MIN_REPEAT_COUNT = 3;
	const uint32_t ENDPOINT_PRED_COUNT_VLC_BITS = 4;

	const uint32_t NUM_ENDPOINT_PREDS = 3;// BASISU_ARRAY_SIZE(g_endpoint_preds);
	const uint32_t CR_ENDPOINT_PRED_INDEX = NUM_ENDPOINT_PREDS - 1;
	const uint32_t NO_ENDPOINT_PRED_INDEX = 3;//NUM_ENDPOINT_PREDS;
	const uint32_t MAX_SELECTOR_HISTORY_BUF_SIZE = 64;
	const uint32_t SELECTOR_HISTORY_BUF_RLE_COUNT_THRESH = 3;
	const uint32_t SELECTOR_HISTORY_BUF_RLE_COUNT_BITS = 6;
	const uint32_t SELECTOR_HISTORY_BUF_RLE_COUNT_TOTAL = (1 << SELECTOR_HISTORY_BUF_RLE_COUNT_BITS);
		
	uint16_t crc16(const void *r, size_t size, uint16_t crc);
		
	class huffman_decoding_table
	{
		friend class bitwise_decoder;

	public:
		huffman_decoding_table()
		{
		}

		void clear()
		{
			basisu::clear_vector(m_code_sizes);
			basisu::clear_vector(m_lookup);
			basisu::clear_vector(m_tree);
		}

		bool init(uint32_t total_syms, const uint8_t *pCode_sizes, uint32_t fast_lookup_bits = basisu::cHuffmanFastLookupBits)
		{
			if (!total_syms)
			{
				clear();
				return true;
			}

			m_code_sizes.resize(total_syms);
			memcpy(&m_code_sizes[0], pCode_sizes, total_syms);

			const uint32_t huffman_fast_lookup_size = 1 << fast_lookup_bits;

			m_lookup.resize(0);
			m_lookup.resize(huffman_fast_lookup_size);

			m_tree.resize(0);
			m_tree.resize(total_syms * 2);

			uint32_t syms_using_codesize[basisu::cHuffmanMaxSupportedInternalCodeSize + 1];
			basisu::clear_obj(syms_using_codesize);
			for (uint32_t i = 0; i < total_syms; i++)
			{
				if (pCode_sizes[i] > basisu::cHuffmanMaxSupportedInternalCodeSize)
					return false;
				syms_using_codesize[pCode_sizes[i]]++;
			}

			uint32_t next_code[basisu::cHuffmanMaxSupportedInternalCodeSize + 1];
			next_code[0] = next_code[1] = 0;

			uint32_t used_syms = 0, total = 0;
			for (uint32_t i = 1; i < basisu::cHuffmanMaxSupportedInternalCodeSize; i++)
			{
				used_syms += syms_using_codesize[i];
				next_code[i + 1] = (total = ((total + syms_using_codesize[i]) << 1));
			}

			if (((1U << basisu::cHuffmanMaxSupportedInternalCodeSize) != total) && (used_syms != 1U))
				return false;

			for (int tree_next = -1, sym_index = 0; sym_index < (int)total_syms; ++sym_index)
			{
				uint32_t rev_code = 0, l, cur_code, code_size = pCode_sizes[sym_index];
				if (!code_size)
					continue;

				cur_code = next_code[code_size]++;

				for (l = code_size; l > 0; l--, cur_code >>= 1)
					rev_code = (rev_code << 1) | (cur_code & 1);

				if (code_size <= fast_lookup_bits)
				{
					uint32_t k = (code_size << 16) | sym_index;
					while (rev_code < huffman_fast_lookup_size)
					{
						if (m_lookup[rev_code] != 0)
						{
							// Supplied codesizes can't create a valid prefix code.
							return false;
						}

						m_lookup[rev_code] = k;
						rev_code += (1 << code_size);
					}
					continue;
				}

				int tree_cur;
				if (0 == (tree_cur = m_lookup[rev_code & (huffman_fast_lookup_size - 1)]))
				{
					const uint32_t idx = rev_code & (huffman_fast_lookup_size - 1);
					if (m_lookup[idx] != 0)
					{
						// Supplied codesizes can't create a valid prefix code.
						return false;
					}

					m_lookup[idx] = tree_next;
					tree_cur = tree_next;
					tree_next -= 2;
				}

				if (tree_cur >= 0)
				{
					// Supplied codesizes can't create a valid prefix code.
					return false;
				}

				rev_code >>= (fast_lookup_bits - 1);

				for (int j = code_size; j > ((int)fast_lookup_bits + 1); j--)
				{
					tree_cur -= ((rev_code >>= 1) & 1);

					const int idx = -tree_cur - 1;
					if (idx < 0)
						return false;
					else if (idx >= (int)m_tree.size())
						m_tree.resize(idx + 1);
										
					if (!m_tree[idx])
					{
						m_tree[idx] = (int16_t)tree_next;
						tree_cur = tree_next;
						tree_next -= 2;
					}
					else
					{
						tree_cur = m_tree[idx];
						if (tree_cur >= 0)
						{
							// Supplied codesizes can't create a valid prefix code.
							return false;
						}
					}
				}

				tree_cur -= ((rev_code >>= 1) & 1);

				const int idx = -tree_cur - 1;
				if (idx < 0)
					return false;
				else if (idx >= (int)m_tree.size())
					m_tree.resize(idx + 1);

				if (m_tree[idx] != 0)
				{
					// Supplied codesizes can't create a valid prefix code.
					return false;
				}

				m_tree[idx] = (int16_t)sym_index;
			}

			return true;
		}

		const basisu::uint8_vec &get_code_sizes() const { return m_code_sizes; }
		const basisu::int_vec &get_lookup() const { return m_lookup; }
		const basisu::int16_vec &get_tree() const { return m_tree; }

		bool is_valid() const { return m_code_sizes.size() > 0; }

	private:
		basisu::uint8_vec m_code_sizes;
		basisu::int_vec m_lookup;
		basisu::int16_vec m_tree;
	};

	class bitwise_decoder
	{
	public:
		bitwise_decoder() :
			m_buf_size(0),
			m_pBuf(nullptr),
			m_pBuf_start(nullptr),
			m_pBuf_end(nullptr),
			m_bit_buf(0),
			m_bit_buf_size(0)
		{
		}

		void clear()
		{
			m_buf_size = 0;
			m_pBuf = nullptr;
			m_pBuf_start = nullptr;
			m_pBuf_end = nullptr;
			m_bit_buf = 0;
			m_bit_buf_size = 0;
		}

		bool init(const uint8_t *pBuf, uint32_t buf_size)
		{
			if ((!pBuf) && (buf_size))
				return false;

			m_buf_size = buf_size;
			m_pBuf = pBuf;
			m_pBuf_start = pBuf;
			m_pBuf_end = pBuf + buf_size;
			m_bit_buf = 0;
			m_bit_buf_size = 0;
			return true;
		}

		void stop()
		{
		}

		inline uint32_t peek_bits(uint32_t num_bits)
		{
			if (!num_bits)
				return 0;

			assert(num_bits <= 25);

			while (m_bit_buf_size < num_bits)
			{
				uint32_t c = 0;
				if (m_pBuf < m_pBuf_end)
					c = *m_pBuf++;

				m_bit_buf |= (c << m_bit_buf_size);
				m_bit_buf_size += 8;
				assert(m_bit_buf_size <= 32);
			}

			return m_bit_buf & ((1 << num_bits) - 1);
		}

		void remove_bits(uint32_t num_bits)
		{
			assert(m_bit_buf_size >= num_bits);

			m_bit_buf >>= num_bits;
			m_bit_buf_size -= num_bits;
		}

		uint32_t get_bits(uint32_t num_bits)
		{
			if (num_bits > 25)
			{
				assert(num_bits <= 32);

				const uint32_t bits0 = peek_bits(25);
				m_bit_buf >>= 25;
				m_bit_buf_size -= 25;
				num_bits -= 25;

				const uint32_t bits = peek_bits(num_bits);
				m_bit_buf >>= num_bits;
				m_bit_buf_size -= num_bits;

				return bits0 | (bits << 25);
			}

			const uint32_t bits = peek_bits(num_bits);

			m_bit_buf >>= num_bits;
			m_bit_buf_size -= num_bits;

			return bits;
		}

		uint32_t decode_truncated_binary(uint32_t n)
		{
			assert(n >= 2);

			const uint32_t k = basisu::floor_log2i(n);
			const uint32_t u = (1 << (k + 1)) - n;

			uint32_t result = get_bits(k);

			if (result >= u)
				result = ((result << 1) | get_bits(1)) - u;

			return result;
		}

		uint32_t decode_rice(uint32_t m)
		{
			assert(m);

			uint32_t q = 0;
			for (;;)
			{
				uint32_t k = peek_bits(16);
				
				uint32_t l = 0;
				while (k & 1)
				{
					l++;
					k >>= 1;
				}
				
				q += l;

				remove_bits(l);

				if (l < 16)
					break;
			}

			return (q << m) + (get_bits(m + 1) >> 1);
		}

		inline uint32_t decode_vlc(uint32_t chunk_bits)
		{
			assert(chunk_bits);

			const uint32_t chunk_size = 1 << chunk_bits;
			const uint32_t chunk_mask = chunk_size - 1;
					
			uint32_t v = 0;
			uint32_t ofs = 0;

			for ( ; ; )
			{
				uint32_t s = get_bits(chunk_bits + 1);
				v |= ((s & chunk_mask) << ofs);
				ofs += chunk_bits;

				if ((s & chunk_size) == 0)
					break;
				
				if (ofs >= 32)
				{
					assert(0);
					break;
				}
			}

			return v;
		}

		inline uint32_t decode_huffman(const huffman_decoding_table &ct, int fast_lookup_bits = basisu::cHuffmanFastLookupBits)
		{
			assert(ct.m_code_sizes.size());

			const uint32_t huffman_fast_lookup_size = 1 << fast_lookup_bits;
						
			while (m_bit_buf_size < 16)
			{
				uint32_t c = 0;
				if (m_pBuf < m_pBuf_end)
					c = *m_pBuf++;

				m_bit_buf |= (c << m_bit_buf_size);
				m_bit_buf_size += 8;
				assert(m_bit_buf_size <= 32);
			}
						
			int code_len;

			int sym;
			if ((sym = ct.m_lookup[m_bit_buf & (huffman_fast_lookup_size - 1)]) >= 0)
			{
				code_len = sym >> 16;
				sym &= 0xFFFF;
			}
			else
			{
				code_len = fast_lookup_bits;
				do
				{
					sym = ct.m_tree[~sym + ((m_bit_buf >> code_len++) & 1)]; // ~sym = -sym - 1
				} while (sym < 0);
			}

			m_bit_buf >>= code_len;
			m_bit_buf_size -= code_len;

			return sym;
		}

		bool read_huffman_table(huffman_decoding_table &ct)
		{
			ct.clear();

			const uint32_t total_used_syms = get_bits(basisu::cHuffmanMaxSymsLog2);

			if (!total_used_syms)
				return true;
			if (total_used_syms > basisu::cHuffmanMaxSyms)
				return false;

			uint8_t code_length_code_sizes[basisu::cHuffmanTotalCodelengthCodes];
			basisu::clear_obj(code_length_code_sizes);

			const uint32_t num_codelength_codes = get_bits(5);
			if ((num_codelength_codes < 1) || (num_codelength_codes > basisu::cHuffmanTotalCodelengthCodes))
				return false;

			for (uint32_t i = 0; i < num_codelength_codes; i++)
				code_length_code_sizes[basisu::g_huffman_sorted_codelength_codes[i]] = static_cast<uint8_t>(get_bits(3));

			huffman_decoding_table code_length_table;
			if (!code_length_table.init(basisu::cHuffmanTotalCodelengthCodes, code_length_code_sizes))
				return false;

			if (!code_length_table.is_valid())
				return false;

			basisu::uint8_vec code_sizes(total_used_syms);

			uint32_t cur = 0;
			while (cur < total_used_syms)
			{
				int c = decode_huffman(code_length_table);

				if (c <= 16)
					code_sizes[cur++] = static_cast<uint8_t>(c);
				else if (c == basisu::cHuffmanSmallZeroRunCode)
					cur += get_bits(basisu::cHuffmanSmallZeroRunExtraBits) + basisu::cHuffmanSmallZeroRunSizeMin;
				else if (c == basisu::cHuffmanBigZeroRunCode)
					cur += get_bits(basisu::cHuffmanBigZeroRunExtraBits) + basisu::cHuffmanBigZeroRunSizeMin;
				else
				{
					if (!cur)
						return false;

					uint32_t l;
					if (c == basisu::cHuffmanSmallRepeatCode)
						l = get_bits(basisu::cHuffmanSmallRepeatExtraBits) + basisu::cHuffmanSmallRepeatSizeMin;
					else
						l = get_bits(basisu::cHuffmanBigRepeatExtraBits) + basisu::cHuffmanBigRepeatSizeMin;

					const uint8_t prev = code_sizes[cur - 1];
					if (prev == 0)
						return false;
					do
					{
						if (cur >= total_used_syms)
							return false;
						code_sizes[cur++] = prev;
					} while (--l > 0);
				}
			}

			if (cur != total_used_syms)
				return false;

			return ct.init(total_used_syms, &code_sizes[0]);
		}

	private:
		uint32_t m_buf_size;
		const uint8_t *m_pBuf;
		const uint8_t *m_pBuf_start;
		const uint8_t *m_pBuf_end;

		uint32_t m_bit_buf;
		uint32_t m_bit_buf_size;
	};

	inline uint32_t basisd_rand(uint32_t seed)
	{
		if (!seed)
			seed++;
		uint32_t z = seed;
		BASISD_znew;
		return z;
	}

	// Returns random number in [0,limit). Max limit is 0xFFFF.
	inline uint32_t basisd_urand(uint32_t& seed, uint32_t limit)
	{
		seed = basisd_rand(seed);
		return (((seed ^ (seed >> 16)) & 0xFFFF) * limit) >> 16;
	}

	class approx_move_to_front
	{
	public:
		approx_move_to_front(uint32_t n)
		{
			init(n);
		}

		void init(uint32_t n)
		{
			m_values.resize(n);
			m_rover = n / 2;
		}

		const basisu::int_vec& get_values() const { return m_values; }
		basisu::int_vec& get_values() { return m_values; }

		uint32_t size() const { return (uint32_t)m_values.size(); }

		const int& operator[] (uint32_t index) const { return m_values[index]; }
		int operator[] (uint32_t index) { return m_values[index]; }

		void add(int new_value)
		{
			m_values[m_rover++] = new_value;
			if (m_rover == m_values.size())
				m_rover = (uint32_t)m_values.size() / 2;
		}

		void use(uint32_t index)
		{
			if (index)
			{
				//std::swap(m_values[index / 2], m_values[index]);
				int x = m_values[index / 2];
				int y = m_values[index];
				m_values[index / 2] = y;
				m_values[index] = x;
			}
		}

		// returns -1 if not found
		int find(int value) const
		{
			for (uint32_t i = 0; i < m_values.size(); i++)
				if (m_values[i] == value)
					return i;
			return -1;
		}

		void reset()
		{
			const uint32_t n = (uint32_t)m_values.size();

			m_values.clear();

			init(n);
		}

	private:
		basisu::int_vec m_values;
		uint32_t m_rover;
	};

	struct decoder_etc_block;
	
	inline uint8_t clamp255(int32_t i)
	{
		return (uint8_t)((i & 0xFFFFFF00U) ? (~(i >> 31)) : i);
	}

	enum eNoClamp
	{
		cNoClamp = 0
	};

	struct color32
	{
		union
		{
			struct
			{
				uint8_t r;
				uint8_t g;
				uint8_t b;
				uint8_t a;
			};

			uint8_t c[4];
			
			uint32_t m;
		};

		color32() { }

		color32(uint32_t vr, uint32_t vg, uint32_t vb, uint32_t va) { set(vr, vg, vb, va); }
		color32(eNoClamp unused, uint32_t vr, uint32_t vg, uint32_t vb, uint32_t va) { (void)unused; set_noclamp_rgba(vr, vg, vb, va); }

		void set(uint32_t vr, uint32_t vg, uint32_t vb, uint32_t va) { c[0] = static_cast<uint8_t>(vr); c[1] = static_cast<uint8_t>(vg); c[2] = static_cast<uint8_t>(vb); c[3] = static_cast<uint8_t>(va); }

		void set_noclamp_rgb(uint32_t vr, uint32_t vg, uint32_t vb) { c[0] = static_cast<uint8_t>(vr); c[1] = static_cast<uint8_t>(vg); c[2] = static_cast<uint8_t>(vb); }
		void set_noclamp_rgba(uint32_t vr, uint32_t vg, uint32_t vb, uint32_t va) { set(vr, vg, vb, va); }

		void set_clamped(int vr, int vg, int vb, int va) { c[0] = clamp255(vr); c[1] = clamp255(vg);	c[2] = clamp255(vb); c[3] = clamp255(va); }

		uint8_t operator[] (uint32_t idx) const { assert(idx < 4); return c[idx]; }
		uint8_t &operator[] (uint32_t idx) { assert(idx < 4); return c[idx]; }

		bool operator== (const color32&rhs) const { return m == rhs.m; }

		static color32 comp_min(const color32& a, const color32& b) { return color32(cNoClamp, basisu::minimum(a[0], b[0]), basisu::minimum(a[1], b[1]), basisu::minimum(a[2], b[2]), basisu::minimum(a[3], b[3])); }
		static color32 comp_max(const color32& a, const color32& b) { return color32(cNoClamp, basisu::maximum(a[0], b[0]), basisu::maximum(a[1], b[1]), basisu::maximum(a[2], b[2]), basisu::maximum(a[3], b[3])); }
	};

	struct endpoint
	{
		color32 m_color5;
		uint8_t m_inten5;
		bool operator== (const endpoint& rhs) const
		{
			return (m_color5.r == rhs.m_color5.r) && (m_color5.g == rhs.m_color5.g) && (m_color5.b == rhs.m_color5.b) && (m_inten5 == rhs.m_inten5);
		}
		bool operator!= (const endpoint& rhs) const { return !(*this == rhs); }
	};

	struct selector
	{
		// Plain selectors (2-bits per value)
		uint8_t m_selectors[4];

		// ETC1 selectors
		uint8_t m_bytes[4];

		uint8_t m_lo_selector, m_hi_selector;
		uint8_t m_num_unique_selectors;
		bool operator== (const selector& rhs) const
		{
			return (m_selectors[0] == rhs.m_selectors[0]) &&
				(m_selectors[1] == rhs.m_selectors[1]) &&
				(m_selectors[2] == rhs.m_selectors[2]) &&
				(m_selectors[3] == rhs.m_selectors[3]);
		}
		bool operator!= (const selector& rhs) const
		{
			return !(*this == rhs);
		}

		void init_flags()
		{
			uint32_t hist[4] = { 0, 0, 0, 0 };
			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					uint32_t s = get_selector(x, y);
					hist[s]++;
				}
			}

			m_lo_selector = 3;
			m_hi_selector = 0;
			m_num_unique_selectors = 0;

			for (uint32_t i = 0; i < 4; i++)
			{
				if (hist[i])
				{
					m_num_unique_selectors++;
					if (i < m_lo_selector) m_lo_selector = static_cast<uint8_t>(i);
					if (i > m_hi_selector) m_hi_selector = static_cast<uint8_t>(i);
				}
			}
		}

		// Returned selector value ranges from 0-3 and is a direct index into g_etc1_inten_tables.
		inline uint32_t get_selector(uint32_t x, uint32_t y) const
		{
			assert((x < 4) && (y < 4));
			return (m_selectors[y] >> (x * 2)) & 3;
		}

		void set_selector(uint32_t x, uint32_t y, uint32_t val)
		{
			static const uint8_t s_selector_index_to_etc1[4] = { 3, 2, 0, 1 };

			assert((x | y | val) < 4);

			m_selectors[y] &= ~(3 << (x * 2));
			m_selectors[y] |= (val << (x * 2));

			const uint32_t etc1_bit_index = x * 4 + y;

			uint8_t *p = &m_bytes[3 - (etc1_bit_index >> 3)];

			const uint32_t byte_bit_ofs = etc1_bit_index & 7;
			const uint32_t mask = 1 << byte_bit_ofs;

			const uint32_t etc1_val = s_selector_index_to_etc1[val];

			const uint32_t lsb = etc1_val & 1;
			const uint32_t msb = etc1_val >> 1;

			p[0] &= ~mask;
			p[0] |= (lsb << byte_bit_ofs);

			p[-2] &= ~mask;
			p[-2] |= (msb << byte_bit_ofs);
		}
	};

	bool basis_block_format_is_uncompressed(block_format tex_type);

	//------------------------------------

	typedef uint16_t half_float;

	const double MIN_DENORM_HALF_FLOAT = 0.000000059604645; // smallest positive subnormal number
	const double MIN_HALF_FLOAT = 0.00006103515625; // smallest positive normal number
	const double MAX_HALF_FLOAT = 65504.0; // largest normal number

	inline uint32_t get_bits(uint32_t val, int low, int high)
	{
		const int num_bits = (high - low) + 1;
		assert((num_bits >= 1) && (num_bits <= 32));

		val >>= low;
		if (num_bits != 32)
			val &= ((1u << num_bits) - 1);

		return val;
	}

	inline bool is_half_inf_or_nan(half_float v)
	{
		return get_bits(v, 10, 14) == 31;
	}

	inline bool is_half_denorm(half_float v)
	{
		int e = (v >> 10) & 31;
		return !e;
	}

	inline int get_half_exp(half_float v)
	{
		int e = ((v >> 10) & 31);
		return e ? (e - 15) : -14;
	}

	inline int get_half_mantissa(half_float v)
	{
		if (is_half_denorm(v))
			return v & 0x3FF;
		return (v & 0x3FF) | 0x400;
	}

	inline float get_half_mantissaf(half_float v)
	{
		return ((float)get_half_mantissa(v)) / 1024.0f;
	}

	inline int get_half_sign(half_float v)
	{
		return v ? ((v & 0x8000) ? -1 : 1) : 0;
	}

	inline bool half_is_signed(half_float v)
	{
		return (v & 0x8000) != 0;
	}

#if 0
	int hexp = get_half_exp(Cf);
	float hman = get_half_mantissaf(Cf);
	int hsign = get_half_sign(Cf);
	float k = powf(2.0f, hexp) * hman * hsign;
	if (is_half_inf_or_nan(Cf))
		k = std::numeric_limits<float>::quiet_NaN();
#endif

	half_float float_to_half(float val);

	inline float half_to_float(half_float hval)
	{
		union { float f; uint32_t u; } x = { 0 };

		uint32_t s = ((uint32_t)hval >> 15) & 1;
		uint32_t e = ((uint32_t)hval >> 10) & 0x1F;
		uint32_t m = (uint32_t)hval & 0x3FF;

		if (!e)
		{
			if (!m)
			{
				// +- 0
				x.u = s << 31;
				return x.f;
			}
			else
			{
				// denormalized
				while (!(m & 0x00000400))
				{
					m <<= 1;
					--e;
				}

				++e;
				m &= ~0x00000400;
			}
		}
		else if (e == 31)
		{
			if (m == 0)
			{
				// +/- INF
				x.u = (s << 31) | 0x7f800000;
				return x.f;
			}
			else
			{
				// +/- NaN
				x.u = (s << 31) | 0x7f800000 | (m << 13);
				return x.f;
			}
		}

		e = e + (127 - 15);
		m = m << 13;

		assert(s <= 1);
		assert(m <= 0x7FFFFF);
		assert(e <= 255);

		x.u = m | (e << 23) | (s << 31);
		return x.f;
	}

	// Originally from bc6h_enc.h

	void bc6h_enc_init();

	const uint32_t MAX_BLOG16_VAL = 0xFFFF;

	// BC6H internals
	const uint32_t NUM_BC6H_MODES = 14;
	const uint32_t BC6H_LAST_MODE_INDEX = 13;
	const uint32_t BC6H_FIRST_1SUBSET_MODE_INDEX = 10; // in the MS docs, this is "mode 11" (where the first mode is 1), 60 bits for endpoints (10.10, 10.10, 10.10), 63 bits for weights
	const uint32_t TOTAL_BC6H_PARTITION_PATTERNS = 32;

	extern const uint8_t g_bc6h_mode_sig_bits[NUM_BC6H_MODES][4]; // base, r, g, b

	struct bc6h_bit_layout
	{
		int8_t m_comp; // R=0,G=1,B=2,D=3 (D=partition index)
		int8_t m_index; // 0-3, 0-1 Low/High subset 1, 2-3 Low/High subset 2, -1=partition index (d)
		int8_t m_last_bit;
		int8_t m_first_bit; // may be -1 if a single bit, may be >m_last_bit if reversed
	};

	const uint32_t MAX_BC6H_LAYOUT_INDEX = 25;
	extern const bc6h_bit_layout g_bc6h_bit_layouts[NUM_BC6H_MODES][MAX_BC6H_LAYOUT_INDEX];

	extern const uint8_t g_bc6h_2subset_patterns[TOTAL_BC6H_PARTITION_PATTERNS][4][4]; // [y][x]

	extern const uint8_t g_bc6h_weight3[8];
	extern const uint8_t g_bc6h_weight4[16];

	extern const int8_t g_bc6h_mode_lookup[32];
		
	// Converts b16 to half float
	inline half_float bc6h_blog16_to_half(uint32_t comp)
	{
		assert(comp <= 0xFFFF);

		// scale the magnitude by 31/64
		comp = (comp * 31u) >> 6u;
		return (half_float)comp;
	}

	const uint32_t MAX_BC6H_HALF_FLOAT_AS_UINT = 0x7BFF;

	// Inverts bc6h_blog16_to_half().
	// Returns the nearest blog16 given a half value. 
	inline uint32_t bc6h_half_to_blog16(half_float h)
	{
		assert(h <= MAX_BC6H_HALF_FLOAT_AS_UINT);
		return (h * 64 + 30) / 31;
	}

	struct bc6h_block
	{
		uint8_t m_bytes[16];
	};

	void bc6h_enc_block_mode10(bc6h_block* pPacked_block, const half_float pEndpoints[3][2], const uint8_t* pWeights);
	void bc6h_enc_block_1subset_4bit_weights(bc6h_block* pPacked_block, const half_float pEndpoints[3][2], const uint8_t* pWeights);
	void bc6h_enc_block_1subset_mode9_3bit_weights(bc6h_block* pPacked_block, const half_float pEndpoints[3][2], const uint8_t* pWeights);
	void bc6h_enc_block_1subset_3bit_weights(bc6h_block* pPacked_block, const half_float pEndpoints[3][2], const uint8_t* pWeights);
	void bc6h_enc_block_2subset_mode9_3bit_weights(bc6h_block* pPacked_block, uint32_t common_part_index, const half_float pEndpoints[2][3][2], const uint8_t* pWeights); // pEndpoints[subset][comp][lh_index]
	void bc6h_enc_block_2subset_3bit_weights(bc6h_block* pPacked_block, uint32_t common_part_index, const half_float pEndpoints[2][3][2], const uint8_t* pWeights); // pEndpoints[subset][comp][lh_index]
	bool bc6h_enc_block_solid_color(bc6h_block* pPacked_block, const half_float pColor[3]);
		
} // namespace basist



