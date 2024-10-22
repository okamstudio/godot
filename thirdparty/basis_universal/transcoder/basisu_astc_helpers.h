// basisu_astc_helpers.h
// Be sure to define ASTC_HELPERS_IMPLEMENTATION somewhere to get the implementation, otherwise you only get the header.
#pragma once
#ifndef BASISU_ASTC_HELPERS_HEADER
#define BASISU_ASTC_HELPERS_HEADER

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <fenv.h>

namespace astc_helpers
{
	const uint32_t MAX_WEIGHT_VALUE = 64; // grid texel weights must range from [0,64]
	const uint32_t MIN_GRID_DIM = 2; // the minimum dimension of a block's weight grid
	const uint32_t MIN_BLOCK_DIM = 4, MAX_BLOCK_DIM = 12; // the valid block dimensions in texels
	const uint32_t MAX_GRID_WEIGHTS = 64; // a block may have a maximum of 64 weight grid values

	static const uint32_t NUM_ASTC_BLOCK_SIZES = 14;
	extern const uint8_t g_astc_block_sizes[NUM_ASTC_BLOCK_SIZES][2];

	// The Color Endpoint Modes (CEM's)
	enum cems
	{
		CEM_LDR_LUM_DIRECT = 0,
		CEM_LDR_LUM_BASE_PLUS_OFS = 1,
		CEM_HDR_LUM_LARGE_RANGE = 2,
		CEM_HDR_LUM_SMALL_RANGE = 3,
		CEM_LDR_LUM_ALPHA_DIRECT = 4,
		CEM_LDR_LUM_ALPHA_BASE_PLUS_OFS = 5,
		CEM_LDR_RGB_BASE_SCALE = 6,
		CEM_HDR_RGB_BASE_SCALE = 7,
		CEM_LDR_RGB_DIRECT = 8,
		CEM_LDR_RGB_BASE_PLUS_OFFSET = 9,
		CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A = 10,
		CEM_HDR_RGB = 11,
		CEM_LDR_RGBA_DIRECT = 12,
		CEM_LDR_RGBA_BASE_PLUS_OFFSET = 13,
		CEM_HDR_RGB_LDR_ALPHA = 14,
		CEM_HDR_RGB_HDR_ALPHA = 15
	};

	// All Bounded Integer Sequence Coding (BISE or ISE) ranges.
	// Weights: Ranges [0,11] are valid.
	// Endpoints: Ranges [4,20] are valid.
	enum bise_levels
	{
		BISE_2_LEVELS = 0,
		BISE_3_LEVELS = 1,
		BISE_4_LEVELS = 2,
		BISE_5_LEVELS = 3,
		BISE_6_LEVELS = 4,
		BISE_8_LEVELS = 5,
		BISE_10_LEVELS = 6,
		BISE_12_LEVELS = 7,
		BISE_16_LEVELS = 8,
		BISE_20_LEVELS = 9,
		BISE_24_LEVELS = 10,
		BISE_32_LEVELS = 11,
		BISE_40_LEVELS = 12,
		BISE_48_LEVELS = 13,
		BISE_64_LEVELS = 14,
		BISE_80_LEVELS = 15,
		BISE_96_LEVELS = 16,
		BISE_128_LEVELS = 17,
		BISE_160_LEVELS = 18,
		BISE_192_LEVELS = 19,
		BISE_256_LEVELS = 20
	};

	const uint32_t TOTAL_ISE_RANGES = 21;

	// Valid endpoint ISE ranges
	const uint32_t FIRST_VALID_ENDPOINT_ISE_RANGE = BISE_6_LEVELS; // 4
	const uint32_t LAST_VALID_ENDPOINT_ISE_RANGE = BISE_256_LEVELS; // 20
	const uint32_t TOTAL_ENDPOINT_ISE_RANGES = LAST_VALID_ENDPOINT_ISE_RANGE - FIRST_VALID_ENDPOINT_ISE_RANGE + 1;

	// Valid weight ISE ranges
	const uint32_t FIRST_VALID_WEIGHT_ISE_RANGE = BISE_2_LEVELS; // 0
	const uint32_t LAST_VALID_WEIGHT_ISE_RANGE = BISE_32_LEVELS; // 11
	const uint32_t TOTAL_WEIGHT_ISE_RANGES = LAST_VALID_WEIGHT_ISE_RANGE - FIRST_VALID_WEIGHT_ISE_RANGE + 1;

	// The ISE range table.
	extern const int8_t g_ise_range_table[TOTAL_ISE_RANGES][3]; // 0=bits (0 to 8), 1=trits (0 or 1), 2=quints (0 or 1)

	// Possible Color Component Select values, used in dual plane mode. 
	// The CCS component will be interpolated using the 2nd weight plane.
	enum ccs
	{
		CCS_GBA_R = 0,
		CCS_RBA_G = 1,
		CCS_RGA_B = 2,
		CCS_RGB_A = 3
	};
		
	struct astc_block
	{
		uint32_t m_vals[4];
	};

	const uint32_t MAX_PARTITIONS = 4;				// Max # of partitions or subsets for single plane mode
	const uint32_t MAX_DUAL_PLANE_PARTITIONS = 3;	// Max # of partitions or subsets for dual plane mode
	const uint32_t NUM_PARTITION_PATTERNS = 1024;	// Total # of partition pattern seeds (10-bits)
	const uint32_t MAX_ENDPOINTS = 18;				// Maximum # of endpoint values in a block

	struct log_astc_block
	{
		bool m_error_flag;
		
		bool m_solid_color_flag_ldr, m_solid_color_flag_hdr;
		uint16_t m_solid_color[4];

		// Rest is only valid if !m_solid_color_flag_ldr && !m_solid_color_flag_hdr
		uint32_t m_grid_width, m_grid_height;	// weight grid dimensions, not the dimension of the block
		
		bool m_dual_plane;

		uint32_t m_weight_ise_range;			// 0-11
		uint32_t m_endpoint_ise_range;			// 4-20, this is actually inferred from the size of the other config bits+weights, but this is here for checking

		uint32_t m_color_component_selector;	// 0-3, 0=GBA R, 1=RBA G, 2=RGA B, 3=RGB A, only used in dual plane mode

		uint32_t m_num_partitions;				// or the # of subsets, 1-4 (1-3 if dual plane mode)
		uint32_t m_partition_id;				// 10-bits, must be 0 if m_num_partitions==1
		
		uint32_t m_color_endpoint_modes[MAX_PARTITIONS]; // each subset's CEM's
		
		// ISE weight grid values. In dual plane mode, the order is p0,p1,  p0,p1,  etc.
		uint8_t m_weights[MAX_GRID_WEIGHTS];
		
		// ISE endpoint values
		// Endpoint order examples:
		// 1 subset LA : LL0 LH0 AL0 AH0
		// 1 subset RGB : RL0 RH0 GL0 GH0 BL0 BH0
		// 1 subset RGBA : RL0 RH0 GL0 GH0 BL0 BH0 AL0 AH0
		// 2 subset LA : LL0 LH0 AL0 AH0 LL1 LH1 AL1 AH1
		// 2 subset RGB : RL0 RH0 GL0 GH0 BL0 BH0 RL1 RH1 GL1 GH1 BL1 BH1
		// 2 subset RGBA : RL0 RH0 GL0 GH0 BL0 BH0 AL0 AH0 RL1 RH1 GL1 GH1 BL1 BH1 AL1 AH1
		uint8_t m_endpoints[MAX_ENDPOINTS];

		void clear()
		{
			memset(this, 0, sizeof(*this));
		}
	};

	// Open interval
	inline int bounds_check(int v, int l, int h) { (void)v; (void)l; (void)h; assert(v >= l && v < h); return v; }
	inline uint32_t bounds_check(uint32_t v, uint32_t l, uint32_t h) { (void)v; (void)l; (void)h; assert(v >= l && v < h); return v; }

	inline uint32_t get_bits(uint32_t val, int low, int high)
	{
		const int num_bits = (high - low) + 1;
		assert((num_bits >= 1) && (num_bits <= 32));

		val >>= low;
		if (num_bits != 32)
			val &= ((1u << num_bits) - 1);

		return val;
	}

	// Returns the number of levels in the given ISE range.
	inline uint32_t get_ise_levels(uint32_t ise_range) 
	{ 
		assert(ise_range < TOTAL_ISE_RANGES);
		return (1 + 2 * g_ise_range_table[ise_range][1] + 4 * g_ise_range_table[ise_range][2]) << g_ise_range_table[ise_range][0];
	}

	inline int get_ise_sequence_bits(int count, int range)
	{
		// See 18.22 Data Size Determination
		int total_bits = g_ise_range_table[range][0] * count;
		total_bits += (g_ise_range_table[range][1] * 8 * count + 4) / 5;
		total_bits += (g_ise_range_table[range][2] * 7 * count + 2) / 3;
		return total_bits;
	}
		
	inline uint32_t weight_interpolate(uint32_t l, uint32_t h, uint32_t w)
	{
		assert(w <= MAX_WEIGHT_VALUE);
		return (l * (64 - w) + h * w + 32) >> 6;
	}

	void encode_bise(uint32_t* pDst, const uint8_t* pSrc_vals, uint32_t bit_pos, int num_vals, int range);

	// Packs a logical to physical ASTC block. Note this does not validate the block's dimensions (use is_valid_block_size()), just the grid dimensions.
	bool pack_astc_block(astc_block &phys_block, const log_astc_block& log_block, int* pExpected_endpoint_range = nullptr);

	// Pack LDR void extent (really solid color) blocks. For LDR, pass in (val | (val << 8)) for each component.
	void pack_void_extent_ldr(astc_block& blk, uint16_t r, uint16_t g, uint16_t b, uint16_t a);

	// Pack HDR void extent (16-bit values are FP16/half floats - no NaN/Inf's)
	void pack_void_extent_hdr(astc_block& blk, uint16_t rh, uint16_t gh, uint16_t bh, uint16_t ah);

	// These helpers are all quite slow, but are useful for table preparation.
	
	// Dequantizes ISE encoded endpoint val to [0,255]
	uint32_t dequant_bise_endpoint(uint32_t val, uint32_t ise_range); // ISE ranges 4-11
		
	// Dequantizes ISE encoded weight val to [0,64]
	uint32_t dequant_bise_weight(uint32_t val, uint32_t ise_range); // ISE ranges 0-10

	uint32_t find_nearest_bise_endpoint(int v, uint32_t ise_range);
	uint32_t find_nearest_bise_weight(int v, uint32_t ise_range);

	void create_quant_tables(
		uint8_t* pVal_to_ise,	// [0-255] or [0-64] value to nearest ISE symbol, array size is [256] or [65]
		uint8_t* pISE_to_val,	// ASTC encoded ISE symbol to [0,255] or [0,64] value, [levels]
		uint8_t* pISE_to_rank,	// returns the level rank index given an ISE symbol, [levels]
		uint8_t* pRank_to_ISE,  // returns the ISE symbol given a level rank, inverse of pISE_to_rank, [levels]
		uint32_t ise_range,		// ise range, [4,20] for endpoints, [0,11] for weights
		bool weight_flag);		// false if block endpoints, true if weights

	// True if the CEM is LDR.
	bool is_cem_ldr(uint32_t mode);
	inline bool is_cem_hdr(uint32_t mode) { return !is_cem_ldr(mode); }

	// True if the passed in dimensions are a valid ASTC block size. There are 14 supported configs, from 4x4 (8bpp) to 12x12 (.89bpp).
	bool is_valid_block_size(uint32_t w, uint32_t h);

	bool block_has_any_hdr_cems(const log_astc_block& log_blk);
	bool block_has_any_ldr_cems(const log_astc_block& log_blk);
	
	// Returns the # of endpoint values for the given CEM.
	inline uint32_t get_num_cem_values(uint32_t cem) { assert(cem <= 15); return 2 + 2 * (cem >> 2); }

	struct dequant_table
	{
		basisu::vector<uint8_t> m_val_to_ise;	// [0-255] or [0-64] value to nearest ISE symbol, array size is [256] or [65]
		basisu::vector<uint8_t> m_ISE_to_val;	// ASTC encoded ISE symbol to [0,255] or [0,64] value, [levels]
		basisu::vector<uint8_t> m_ISE_to_rank;	// returns the level rank index given an ISE symbol, [levels]
		basisu::vector<uint8_t> m_rank_to_ISE;  // returns the ISE symbol given a level rank, inverse of pISE_to_rank, [levels]		

		void init(bool weight_flag, uint32_t num_levels, bool init_rank_tabs)
		{
			m_val_to_ise.resize(weight_flag ? (MAX_WEIGHT_VALUE + 1) : 256);
			m_ISE_to_val.resize(num_levels);
			if (init_rank_tabs)
			{
				m_ISE_to_rank.resize(num_levels);
				m_rank_to_ISE.resize(num_levels);
			}
		}
	};

	struct dequant_tables
	{
		dequant_table m_weights[TOTAL_WEIGHT_ISE_RANGES];
		dequant_table m_endpoints[TOTAL_ENDPOINT_ISE_RANGES];

		const dequant_table& get_weight_tab(uint32_t range) const
		{
			assert((range >= FIRST_VALID_WEIGHT_ISE_RANGE) && (range <= LAST_VALID_WEIGHT_ISE_RANGE));
			return m_weights[range - FIRST_VALID_WEIGHT_ISE_RANGE];
		}

		dequant_table& get_weight_tab(uint32_t range)
		{
			assert((range >= FIRST_VALID_WEIGHT_ISE_RANGE) && (range <= LAST_VALID_WEIGHT_ISE_RANGE));
			return m_weights[range - FIRST_VALID_WEIGHT_ISE_RANGE];
		}

		const dequant_table& get_endpoint_tab(uint32_t range) const
		{
			assert((range >= FIRST_VALID_ENDPOINT_ISE_RANGE) && (range <= LAST_VALID_ENDPOINT_ISE_RANGE));
			return m_endpoints[range - FIRST_VALID_ENDPOINT_ISE_RANGE];
		}

		dequant_table& get_endpoint_tab(uint32_t range)
		{
			assert((range >= FIRST_VALID_ENDPOINT_ISE_RANGE) && (range <= LAST_VALID_ENDPOINT_ISE_RANGE));
			return m_endpoints[range - FIRST_VALID_ENDPOINT_ISE_RANGE];
		}

		void init(bool init_rank_tabs)
		{
			for (uint32_t range = FIRST_VALID_WEIGHT_ISE_RANGE; range <= LAST_VALID_WEIGHT_ISE_RANGE; range++)
			{
				const uint32_t num_levels = get_ise_levels(range);
				dequant_table& tab = get_weight_tab(range);

				tab.init(true, num_levels, init_rank_tabs);

				create_quant_tables(tab.m_val_to_ise.data(), tab.m_ISE_to_val.data(), init_rank_tabs ? tab.m_ISE_to_rank.data() : nullptr, init_rank_tabs ? tab.m_rank_to_ISE.data() : nullptr, range, true);
			}

			for (uint32_t range = FIRST_VALID_ENDPOINT_ISE_RANGE; range <= LAST_VALID_ENDPOINT_ISE_RANGE; range++)
			{
				const uint32_t num_levels = get_ise_levels(range);
				dequant_table& tab = get_endpoint_tab(range);

				tab.init(false, num_levels, init_rank_tabs);

				create_quant_tables(tab.m_val_to_ise.data(), tab.m_ISE_to_val.data(), init_rank_tabs ? tab.m_ISE_to_rank.data() : nullptr, init_rank_tabs ? tab.m_rank_to_ISE.data() : nullptr, range, false);
			}
		}
	};

	extern dequant_tables g_dequant_tables;
	void init_tables(bool init_rank_tabs);
		
	// Procedurally returns the texel partition/subset index given the block coordinate and config.
	int compute_texel_partition(uint32_t seedIn, uint32_t xIn, uint32_t yIn, uint32_t zIn, int num_partitions, bool small_block);
		
	void blue_contract(
		int r, int g, int b, int a,
		int& dr, int& dg, int& db, int& da);

	void bit_transfer_signed(int& a, int& b);

	void decode_endpoint(uint32_t cem_index, int (*pEndpoints)[2], const uint8_t* pE);

	typedef uint16_t half_float;
	half_float float_to_half(float val, bool toward_zero);
	float half_to_float(half_float hval);

	const int MAX_RGB9E5 = 0xff80;
	void unpack_rgb9e5(uint32_t packed, float& r, float& g, float& b);
	uint32_t pack_rgb9e5(float r, float g, float b);
	
	enum decode_mode
	{
		cDecodeModeSRGB8 = 0,	// returns uint8_t's, not valid on HDR blocks
		cDecodeModeLDR8 = 1,	// returns uint8_t's, not valid on HDR blocks
		cDecodeModeHDR16 = 2,   // returns uint16_t's (half floats), valid on all LDR/HDR blocks
		cDecodeModeRGB9E5 = 3	// returns uint32_t's, packed as RGB 9E5 (shared exponent), see https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
	};

	// Decodes logical block to output pixels.
	// pPixels must point to either 32-bit pixel values (SRGB8/LDR8/9E5) or 64-bit pixel values (HDR16)
	bool decode_block(const log_astc_block& log_blk, void* pPixels, uint32_t blk_width, uint32_t blk_height, decode_mode dec_mode);

	void decode_bise(uint32_t ise_range, uint8_t* pVals, uint32_t num_vals, const uint8_t *pBits128, uint32_t bit_ofs);

	// Unpack a physical ASTC encoded GPU texture block to a logical block description.
	bool unpack_block(const void* pASTC_block, log_astc_block& log_blk, uint32_t blk_width, uint32_t blk_height);
					
} // namespace astc_helpers

#endif // BASISU_ASTC_HELPERS_HEADER

//------------------------------------------------------------------

#ifdef BASISU_ASTC_HELPERS_IMPLEMENTATION

namespace astc_helpers
{
	template<typename T> inline T my_min(T a, T b) { return (a < b) ? a : b; }
	template<typename T> inline T my_max(T a, T b) { return (a > b) ? a : b; }

	const uint8_t g_astc_block_sizes[NUM_ASTC_BLOCK_SIZES][2] = { 
		{ 4, 4 }, { 5, 4 }, { 5, 5 }, { 6, 5 }, 
		{ 6, 6 }, { 8, 5 }, { 8, 6 }, { 10, 5 }, 
		{ 10, 6 }, { 8, 8 }, { 10, 8 }, { 10, 10 }, 
		{ 12, 10 }, { 12, 12 } 
	};

	const int8_t g_ise_range_table[TOTAL_ISE_RANGES][3] =
	{
		//b  t  q
		//2  3  5	 // rng  ise_index	notes
		{ 1, 0, 0 }, // 0..1 0
		{ 0, 1, 0 }, // 0..2 1
		{ 2, 0, 0 }, // 0..3 2
		{ 0, 0, 1 }, // 0..4 3
		{ 1, 1, 0 }, // 0..5 4			min endpoint ISE index
		{ 3, 0, 0 }, // 0..7 5
		{ 1, 0, 1 }, // 0..9 6
		{ 2, 1, 0 }, // 0..11 7
		{ 4, 0, 0 }, // 0..15 8
		{ 2, 0, 1 }, // 0..19 9
		{ 3, 1, 0 }, // 0..23 10
		{ 5, 0, 0 }, // 0..31 11		max weight ISE index
		{ 3, 0, 1 }, // 0..39 12
		{ 4, 1, 0 }, // 0..47 13
		{ 6, 0, 0 }, // 0..63 14
		{ 4, 0, 1 }, // 0..79 15
		{ 5, 1, 0 }, // 0..95 16
		{ 7, 0, 0 }, // 0..127 17
		{ 5, 0, 1 }, // 0..159 18
		{ 6, 1, 0 }, // 0..191 19
		{ 8, 0, 0 }, // 0..255 20
	};
		
	static inline void astc_set_bits_1_to_9(uint32_t* pDst, uint32_t& bit_offset, uint32_t code, uint32_t codesize)
	{
		uint8_t* pBuf = reinterpret_cast<uint8_t*>(pDst);

		assert(codesize <= 9);
		if (codesize)
		{
			uint32_t byte_bit_offset = bit_offset & 7;
			uint32_t val = code << byte_bit_offset;

			uint32_t index = bit_offset >> 3;
			pBuf[index] |= (uint8_t)val;

			if (codesize > (8 - byte_bit_offset))
				pBuf[index + 1] |= (uint8_t)(val >> 8);

			bit_offset += codesize;
		}
	}

	static inline uint32_t astc_extract_bits(uint32_t bits, int low, int high)
	{
		return (bits >> low) & ((1 << (high - low + 1)) - 1);
	}

	// Writes bits to output in an endian safe way
	static inline void astc_set_bits(uint32_t* pOutput, uint32_t& bit_pos, uint32_t value, uint32_t total_bits)
	{
		assert(total_bits <= 31);
		assert(value < (1u << total_bits));

		uint8_t* pBytes = reinterpret_cast<uint8_t*>(pOutput);

		while (total_bits)
		{
			const uint32_t bits_to_write = my_min<int>(total_bits, 8 - (bit_pos & 7));

			pBytes[bit_pos >> 3] |= static_cast<uint8_t>(value << (bit_pos & 7));

			bit_pos += bits_to_write;
			total_bits -= bits_to_write;
			value >>= bits_to_write;
		}
	}

	static const uint8_t g_astc_quint_encode[125] =
	{
		0, 1, 2, 3, 4, 8, 9, 10, 11, 12, 16, 17, 18, 19, 20, 24, 25, 26, 27, 28, 5, 13, 21, 29, 6, 32, 33, 34, 35, 36, 40, 41, 42, 43, 44, 48, 49, 50, 51, 52, 56, 57,
		58, 59, 60, 37, 45, 53, 61, 14, 64, 65, 66, 67, 68, 72, 73, 74, 75, 76, 80, 81, 82, 83, 84, 88, 89, 90, 91, 92, 69, 77, 85, 93, 22, 96, 97, 98, 99, 100, 104,
		105, 106, 107, 108, 112, 113, 114, 115, 116, 120, 121, 122, 123, 124, 101, 109, 117, 125, 30, 102, 103, 70, 71, 38, 110, 111, 78, 79, 46, 118, 119, 86, 87, 54,
		126, 127, 94, 95, 62, 39, 47, 55, 63, 7 /*31 - results in the same decode as 7*/
	};

	// Encodes 3 values to output, usable for any range that uses quints and bits
	static inline void astc_encode_quints(uint32_t* pOutput, const uint8_t* pValues, uint32_t& bit_pos, int n)
	{
		// First extract the quints and the bits from the 3 input values
		int quints = 0, bits[3];
		const uint32_t bit_mask = (1 << n) - 1;
		for (int i = 0; i < 3; i++)
		{
			static const int s_muls[3] = { 1, 5, 25 };

			const int t = pValues[i] >> n;

			quints += t * s_muls[i];
			bits[i] = pValues[i] & bit_mask;
		}

		// Encode the quints, by inverting the bit manipulations done by the decoder, converting 3 quints into 7-bits.
		// See https://www.khronos.org/registry/DataFormat/specs/1.2/dataformat.1.2.html#astc-integer-sequence-encoding

		assert(quints < 125);
		const int T = g_astc_quint_encode[quints];

		// Now interleave the 7 encoded quint bits with the bits to form the encoded output. See table 95-96.
		astc_set_bits(pOutput, bit_pos, bits[0] | (astc_extract_bits(T, 0, 2) << n) | (bits[1] << (3 + n)) | (astc_extract_bits(T, 3, 4) << (3 + n * 2)) |
			(bits[2] << (5 + n * 2)) | (astc_extract_bits(T, 5, 6) << (5 + n * 3)), 7 + n * 3);
	}

	static const uint8_t g_astc_trit_encode[243] = { 0, 1, 2, 4, 5, 6, 8, 9, 10, 16, 17, 18, 20, 21, 22, 24, 25, 26, 3, 7, 11, 19, 23, 27, 12, 13, 14, 32, 33, 34, 36, 37, 38, 40, 41, 42, 48, 49, 50, 52, 53, 54, 56, 57, 58, 35, 39,
		43, 51, 55, 59, 44, 45, 46, 64, 65, 66, 68, 69, 70, 72, 73, 74, 80, 81, 82, 84, 85, 86, 88, 89, 90, 67, 71, 75, 83, 87, 91, 76, 77, 78, 128, 129, 130, 132, 133, 134, 136, 137, 138, 144, 145, 146, 148, 149, 150, 152, 153, 154,
		131, 135, 139, 147, 151, 155, 140, 141, 142, 160, 161, 162, 164, 165, 166, 168, 169, 170, 176, 177, 178, 180, 181, 182, 184, 185, 186, 163, 167, 171, 179, 183, 187, 172, 173, 174, 192, 193, 194, 196, 197, 198, 200, 201, 202,
		208, 209, 210, 212, 213, 214, 216, 217, 218, 195, 199, 203, 211, 215, 219, 204, 205, 206, 96, 97, 98, 100, 101, 102, 104, 105, 106, 112, 113, 114, 116, 117, 118, 120, 121, 122, 99, 103, 107, 115, 119, 123, 108, 109, 110, 224,
		225, 226, 228, 229, 230, 232, 233, 234, 240, 241, 242, 244, 245, 246, 248, 249, 250, 227, 231, 235, 243, 247, 251, 236, 237, 238, 28, 29, 30, 60, 61, 62, 92, 93, 94, 156, 157, 158, 188, 189, 190, 220, 221, 222, 31, 63, 95, 159,
		191, 223, 124, 125, 126 };

	// Encodes 5 values to output, usable for any range that uses trits and bits
	static void astc_encode_trits(uint32_t* pOutput, const uint8_t* pValues, uint32_t& bit_pos, int n)
	{
		// First extract the trits and the bits from the 5 input values
		int trits = 0, bits[5];
		const uint32_t bit_mask = (1 << n) - 1;
		for (int i = 0; i < 5; i++)
		{
			static const int s_muls[5] = { 1, 3, 9, 27, 81 };

			const int t = pValues[i] >> n;

			trits += t * s_muls[i];
			bits[i] = pValues[i] & bit_mask;
		}

		// Encode the trits, by inverting the bit manipulations done by the decoder, converting 5 trits into 8-bits.
		// See https://www.khronos.org/registry/DataFormat/specs/1.2/dataformat.1.2.html#astc-integer-sequence-encoding

		assert(trits < 243);
		const int T = g_astc_trit_encode[trits];

		// Now interleave the 8 encoded trit bits with the bits to form the encoded output. See table 94.
		astc_set_bits(pOutput, bit_pos, bits[0] | (astc_extract_bits(T, 0, 1) << n) | (bits[1] << (2 + n)), n * 2 + 2);

		astc_set_bits(pOutput, bit_pos, astc_extract_bits(T, 2, 3) | (bits[2] << 2) | (astc_extract_bits(T, 4, 4) << (2 + n)) | (bits[3] << (3 + n)) | (astc_extract_bits(T, 5, 6) << (3 + n * 2)) |
			(bits[4] << (5 + n * 2)) | (astc_extract_bits(T, 7, 7) << (5 + n * 3)), n * 3 + 6);
	}

	// Packs values using ASTC's BISE to output buffer.
	void encode_bise(uint32_t* pDst, const uint8_t* pSrc_vals, uint32_t bit_pos, int num_vals, int range)
	{
		uint32_t temp[5] = { 0 };

		const int num_bits = g_ise_range_table[range][0];

		int group_size = 0;
		if (g_ise_range_table[range][1])
			group_size = 5;
		else if (g_ise_range_table[range][2])
			group_size = 3;

#ifndef NDEBUG
		const uint32_t num_levels = get_ise_levels(range);
		for (int i = 0; i < num_vals; i++)
		{
			assert(pSrc_vals[i] < num_levels);
		}
#endif

		if (group_size)
		{
			// Range has trits or quints - pack each group of 5 or 3 values 
			const int total_groups = (group_size == 5) ? ((num_vals + 4) / 5) : ((num_vals + 2) / 3);

			for (int group_index = 0; group_index < total_groups; group_index++)
			{
				uint8_t vals[5] = { 0 };

				const int limit = my_min(group_size, num_vals - group_index * group_size);
				for (int i = 0; i < limit; i++)
					vals[i] = pSrc_vals[group_index * group_size + i];

				if (group_size == 5)
					astc_encode_trits(temp, vals, bit_pos, num_bits);
				else
					astc_encode_quints(temp, vals, bit_pos, num_bits);
			}
		}
		else
		{
			for (int i = 0; i < num_vals; i++)
				astc_set_bits_1_to_9(temp, bit_pos, pSrc_vals[i], num_bits);
		}

		// TODO: Could this write too many bits on incomplete blocks?
		pDst[0] |= temp[0]; pDst[1] |= temp[1];
		pDst[2] |= temp[2]; pDst[3] |= temp[3];
	}

	inline uint32_t rev_dword(uint32_t bits)
	{
		uint32_t v = (bits << 16) | (bits >> 16);
		v = ((v & 0x00ff00ff) << 8) | ((v & 0xff00ff00) >> 8); v = ((v & 0x0f0f0f0f) << 4) | ((v & 0xf0f0f0f0) >> 4);
		v = ((v & 0x33333333) << 2) | ((v & 0xcccccccc) >> 2); v = ((v & 0x55555555) << 1) | ((v & 0xaaaaaaaa) >> 1);
		return v;
	}

	static inline bool is_packable(int value, int num_bits) { assert((num_bits >= 1) && (num_bits < 31)); return (value >= 0) && (value < (1 << num_bits)); }

	static bool get_config_bits(const log_astc_block &log_block, uint32_t &config_bits)
	{
		config_bits = 0;

		const int W = log_block.m_grid_width, H = log_block.m_grid_height;

		const uint32_t P = log_block.m_weight_ise_range >= 6; // high precision
		const uint32_t Dp_P = (log_block.m_dual_plane << 1) | P; // pack dual plane+high precision bits
		
		// See Tables 81-82
		// Compute p from weight range
		uint32_t p = 2 + log_block.m_weight_ise_range - (P ? 6 : 0);
		
		// Rearrange p's bits to p0 p2 p1
		p = (p >> 1) + ((p & 1) << 2);
		
		// Try encoding each row of table 82.

		// W+4 H+2
		if (is_packable(W - 4, 2) && is_packable(H - 2, 2))
		{
			config_bits = (Dp_P << 9) | ((W - 4) << 7) | ((H - 2) << 5) | ((p & 4) << 2) | (p & 3);
			return true;
		}

		// W+8 H+2
		if (is_packable(W - 8, 2) && is_packable(H - 2, 2))
		{
			config_bits = (Dp_P << 9) | ((W - 8) << 7) | ((H - 2) << 5) | ((p & 4) << 2) | 4 | (p & 3);
			return true;
		}

		// W+2 H+8
		if (is_packable(W - 2, 2) && is_packable(H - 8, 2))
		{
			config_bits = (Dp_P << 9) | ((H - 8) << 7) | ((W - 2) << 5) | ((p & 4) << 2) | 8 | (p & 3);
			return true;
		}

		// W+2 H+6
		if (is_packable(W - 2, 2) && is_packable(H - 6, 1))
		{
			config_bits = (Dp_P << 9) | ((H - 6) << 7) | ((W - 2) << 5) | ((p & 4) << 2) | 12 | (p & 3);
			return true;
		}

		// W+2 H+2
		if (is_packable(W - 2, 1) && is_packable(H - 2, 2))
		{
			config_bits = (Dp_P << 9) | ((W) << 7) | ((H - 2) << 5) | ((p & 4) << 2) | 12 | (p & 3);
			return true;
		}
				
		// 12 H+2
		if ((W == 12) && is_packable(H - 2, 2))
		{
			config_bits = (Dp_P << 9) | ((H - 2) << 5) | (p << 2);
			return true;
		}

		// W+2 12
		if ((H == 12) && is_packable(W - 2, 2))
		{
			config_bits = (Dp_P << 9) | (1 << 7) | ((W - 2) << 5) | (p << 2);
			return true;
		}

		// 6 10
		if ((W == 6) && (H == 10))
		{
			config_bits = (Dp_P << 9) | (3 << 7) | (p << 2);
			return true;
		}

		// 10 6
		if ((W == 10) && (H == 6))
		{
			config_bits = (Dp_P << 9) | (0b1101 << 5) | (p << 2);
			return true;
		}
				
		// W+6 H+6 (no dual plane or high prec)
		if ((!Dp_P) && is_packable(W - 6, 2) && is_packable(H - 6, 2))
		{
			config_bits = ((H - 6) << 9) | 256 | ((W - 6) << 5) | (p << 2);
			return true;
		}

		// Failed: unsupported weight grid dimensions or config.
		return false;
	}

	bool pack_astc_block(astc_block& phys_block, const log_astc_block& log_block, int* pExpected_endpoint_range)
	{
		memset(&phys_block, 0, sizeof(phys_block));

		if (pExpected_endpoint_range)
			*pExpected_endpoint_range = -1;

		assert(!log_block.m_error_flag);
		if (log_block.m_error_flag)
			return false;
				
		if (log_block.m_solid_color_flag_ldr)
		{
			pack_void_extent_ldr(phys_block, log_block.m_solid_color[0], log_block.m_solid_color[1], log_block.m_solid_color[2], log_block.m_solid_color[3]);
			return true;
		}
		else if (log_block.m_solid_color_flag_hdr)
		{
			pack_void_extent_hdr(phys_block, log_block.m_solid_color[0], log_block.m_solid_color[1], log_block.m_solid_color[2], log_block.m_solid_color[3]);
			return true;
		}
				
		if ((log_block.m_num_partitions < 1) || (log_block.m_num_partitions > MAX_PARTITIONS))
			return false;

		// Max usable weight range is 11
		if (log_block.m_weight_ise_range > LAST_VALID_WEIGHT_ISE_RANGE)
			return false;

		// See 23.24 Illegal Encodings, [0,5] is the minimum ISE encoding for endpoints
		if ((log_block.m_endpoint_ise_range < FIRST_VALID_ENDPOINT_ISE_RANGE) || (log_block.m_endpoint_ise_range > LAST_VALID_ENDPOINT_ISE_RANGE))
			return false;

		if (log_block.m_color_component_selector > 3)
			return false;
				
		uint32_t config_bits = 0;
		if (!get_config_bits(log_block, config_bits))
			return false;

		uint32_t bit_pos = 0;
		astc_set_bits(&phys_block.m_vals[0], bit_pos, config_bits, 11);

		const uint32_t total_grid_weights = (log_block.m_dual_plane ? 2 : 1) * (log_block.m_grid_width * log_block.m_grid_height);
		const uint32_t total_weight_bits = get_ise_sequence_bits(total_grid_weights, log_block.m_weight_ise_range);

		// 18.24 Illegal Encodings
		if ((!total_grid_weights) || (total_grid_weights > MAX_GRID_WEIGHTS) || (total_weight_bits < 24) || (total_weight_bits > 96))
			return false;

		uint32_t total_extra_bits = 0;

		astc_set_bits(&phys_block.m_vals[0], bit_pos, log_block.m_num_partitions - 1, 2);

		if (log_block.m_num_partitions > 1)
		{
			if (log_block.m_partition_id >= NUM_PARTITION_PATTERNS)
				return false;

			astc_set_bits(&phys_block.m_vals[0], bit_pos, log_block.m_partition_id, 10);

			uint32_t highest_cem = 0, lowest_cem = UINT32_MAX;
			for (uint32_t j = 0; j < log_block.m_num_partitions; j++)
			{
				highest_cem = my_max(highest_cem, log_block.m_color_endpoint_modes[j]);
				lowest_cem = my_min(lowest_cem, log_block.m_color_endpoint_modes[j]);
			}

			if (highest_cem > 15)
				return false;
			
			// Ensure CEM range is contiguous
			if (((highest_cem >> 2) > (1 + (lowest_cem >> 2))))
				return false;

			// See tables 79/80
			uint32_t encoded_cem = log_block.m_color_endpoint_modes[0] << 2;
			if (lowest_cem != highest_cem)
			{
				encoded_cem = my_min<uint32_t>(3, 1 + (lowest_cem >> 2));

				// See tables at 23.11 Color Endpoint Mode
				for (uint32_t j = 0; j < log_block.m_num_partitions; j++)
				{
					const int M = log_block.m_color_endpoint_modes[j] & 3;
					
					const int C = (log_block.m_color_endpoint_modes[j] >> 2) - ((encoded_cem & 3) - 1);
					if ((C & 1) != C)
						return false;

					encoded_cem |= (C << (2 + j)) | (M << (2 + log_block.m_num_partitions + 2 * j));
				}

				total_extra_bits = 3 * log_block.m_num_partitions - 4;

				if ((total_weight_bits + total_extra_bits) > 128)
					return false;

				uint32_t cem_bit_pos = 128 - total_weight_bits - total_extra_bits;
				astc_set_bits(&phys_block.m_vals[0], cem_bit_pos, encoded_cem >> 6, total_extra_bits);
			}

			astc_set_bits(&phys_block.m_vals[0], bit_pos, encoded_cem & 0x3f, 6);
		}
		else
		{
			if (log_block.m_partition_id)
				return false;
			if (log_block.m_color_endpoint_modes[0] > 15)
				return false;

			astc_set_bits(&phys_block.m_vals[0], bit_pos, log_block.m_color_endpoint_modes[0], 4);
		}

		if (log_block.m_dual_plane)
		{
			if (log_block.m_num_partitions > 3)
				return false;

			total_extra_bits += 2;
			
			uint32_t ccs_bit_pos = 128 - (int)total_weight_bits - (int)total_extra_bits;
			astc_set_bits(&phys_block.m_vals[0], ccs_bit_pos, log_block.m_color_component_selector, 2);
		}

		const uint32_t total_config_bits = bit_pos + total_extra_bits;
		const int num_remaining_bits = 128 - (int)total_config_bits - (int)total_weight_bits;
		if (num_remaining_bits < 0)
			return false;

		uint32_t total_cem_vals = 0;
		for (uint32_t j = 0; j < log_block.m_num_partitions; j++)
			total_cem_vals += 2 + 2 * (log_block.m_color_endpoint_modes[j] >> 2);

		if (total_cem_vals > MAX_ENDPOINTS)
			return false;

		int endpoint_ise_range = -1;
		for (int k = 20; k > 0; k--)
		{
			int bits = get_ise_sequence_bits(total_cem_vals, k);
			if (bits <= num_remaining_bits)
			{
				endpoint_ise_range = k;
				break;
			}
		}

		// See 23.24 Illegal Encodings, [0,5] is the minimum ISE encoding for endpoints
		if (endpoint_ise_range < (int)FIRST_VALID_ENDPOINT_ISE_RANGE)
			return false;

		// Ensure the caller utilized the right endpoint ISE range.
		if ((int)log_block.m_endpoint_ise_range != endpoint_ise_range)
		{
			if (pExpected_endpoint_range)
				*pExpected_endpoint_range = endpoint_ise_range;
			return false;
		}

		// Pack endpoints forwards
		encode_bise(&phys_block.m_vals[0], log_block.m_endpoints, bit_pos, total_cem_vals, endpoint_ise_range);
		
		// Pack weights backwards
		uint32_t weight_data[4] = { 0 };
		encode_bise(weight_data, log_block.m_weights, 0, total_grid_weights, log_block.m_weight_ise_range);

		for (uint32_t i = 0; i < 4; i++)
			phys_block.m_vals[i] |= rev_dword(weight_data[3 - i]);

		return true;
	}

	static inline uint32_t bit_replication_scale(uint32_t src, int num_src_bits, int num_dst_bits)
	{
		assert(num_src_bits <= num_dst_bits);
		assert((src & ((1 << num_src_bits) - 1)) == src);

		uint32_t dst = 0;
		for (int shift = num_dst_bits - num_src_bits; shift > -num_src_bits; shift -= num_src_bits)
			dst |= (shift >= 0) ? (src << shift) : (src >> -shift);

		return dst;
	}

	uint32_t dequant_bise_endpoint(uint32_t val, uint32_t ise_range)
	{
		assert((ise_range >= FIRST_VALID_ENDPOINT_ISE_RANGE) && (ise_range <= LAST_VALID_ENDPOINT_ISE_RANGE));
		assert(val < get_ise_levels(ise_range));

		uint32_t u = 0;

		switch (ise_range)
		{
		case 5:
		{
			u = bit_replication_scale(val, 3, 8);
			break;
		}
		case 8:
		{
			u = bit_replication_scale(val, 4, 8);
			break;
		}
		case 11:
		{
			u = bit_replication_scale(val, 5, 8);
			break;
		}
		case 14:
		{
			u = bit_replication_scale(val, 6, 8);
			break;
		}
		case 17:
		{
			u = bit_replication_scale(val, 7, 8);
			break;
		}
		case 20:
		{
			u = val;
			break;
		}
		case 4:
		case 6:
		case 7:
		case 9:
		case 10:
		case 12:
		case 13:
		case 15:
		case 16:
		case 18:
		case 19:
		{
			const uint32_t num_bits = g_ise_range_table[ise_range][0];
			const uint32_t num_trits = g_ise_range_table[ise_range][1]; BASISU_NOTE_UNUSED(num_trits);
			const uint32_t num_quints = g_ise_range_table[ise_range][2]; BASISU_NOTE_UNUSED(num_quints);

			// compute Table 103 row index
			const int range_index = (num_bits * 2 + (num_quints ? 1 : 0)) - 2;

			assert(range_index >= 0 && range_index <= 10);

			uint32_t bits = val & ((1 << num_bits) - 1);
			uint32_t tval = val >> num_bits;

			assert(tval < (num_trits ? 3U : 5U));

			uint32_t a = bits & 1;
			uint32_t b = (bits >> 1) & 1;
			uint32_t c = (bits >> 2) & 1;
			uint32_t d = (bits >> 3) & 1;
			uint32_t e = (bits >> 4) & 1;
			uint32_t f = (bits >> 5) & 1;

			uint32_t A = a ? 511 : 0;
			uint32_t B = 0;

			switch (range_index)
			{
			case 2:
			{
				// 876543210
				// b000b0bb0
				B = (b << 1) | (b << 2) | (b << 4) | (b << 8);
				break;
			}
			case 3:
			{
				// 876543210
				// b0000bb00
				B = (b << 2) | (b << 3) | (b << 8);
				break;
			}
			case 4:
			{
				// 876543210
				// cb000cbcb
				B = b | (c << 1) | (b << 2) | (c << 3) | (b << 7) | (c << 8);
				break;
			}
			case 5:
			{
				// 876543210
				// cb0000cbc
				B = c | (b << 1) | (c << 2) | (b << 7) | (c << 8);
				break;
			}
			case 6:
			{
				// 876543210
				// dcb000dcb
				B = b | (c << 1) | (d << 2) | (b << 6) | (c << 7) | (d << 8);
				break;
			}
			case 7:
			{
				// 876543210
				// dcb0000dc
				B = c | (d << 1) | (b << 6) | (c << 7) | (d << 8);
				break;
			}
			case 8:
			{
				// 876543210
				// edcb000ed
				B = d | (e << 1) | (b << 5) | (c << 6) | (d << 7) | (e << 8);
				break;
			}
			case 9:
			{
				// 876543210
				// edcb0000e
				B = e | (b << 5) | (c << 6) | (d << 7) | (e << 8);
				break;
			}
			case 10:
			{
				// 876543210
				// fedcb000f
				B = f | (b << 4) | (c << 5) | (d << 6) | (e << 7) | (f << 8);
				break;
			}
			default:
				break;
			}

			static uint8_t C_vals[11] = { 204, 113, 93, 54, 44, 26, 22, 13, 11, 6, 5 };
			uint32_t C = C_vals[range_index];
			uint32_t D = tval;

			u = D * C + B;
			u = u ^ A;
			u = (A & 0x80) | (u >> 2);

			break;
		}
		default:
		{
			assert(0);
			break;
		}
		}

		return u;
	}

	uint32_t dequant_bise_weight(uint32_t val, uint32_t ise_range)
	{
		assert(val < get_ise_levels(ise_range));

		uint32_t u = 0;
		switch (ise_range)
		{
		case 0: 
		{
			u = val ? 63 : 0;
			break;
		}
		case 1: // 0-2 
		{
			const uint8_t s_tab_0_2[3] = { 0, 32, 63 };
			u = s_tab_0_2[val];
			break;
		}
		case 2: // 0-3
		{
			u = bit_replication_scale(val, 2, 6);
			break;
		}
		case 3: // 0-4
		{
			const uint8_t s_tab_0_4[5] = { 0, 16, 32, 47, 63 };
			u = s_tab_0_4[val];
			break;
		}
		case 5: // 0-7
		{
			u = bit_replication_scale(val, 3, 6);
			break;
		}
		case 8: // 0-15
		{
			u = bit_replication_scale(val, 4, 6);
			break;
		}
		case 11: // 0-31
		{
			u = bit_replication_scale(val, 5, 6);
			break;
		}
		case 4: // 0-5
		case 6: // 0-9
		case 7: // 0-11
		case 9: // 0-19
		case 10: // 0-23
		{
			const uint32_t num_bits = g_ise_range_table[ise_range][0];
			const uint32_t num_trits = g_ise_range_table[ise_range][1]; BASISU_NOTE_UNUSED(num_trits);
			const uint32_t num_quints = g_ise_range_table[ise_range][2]; BASISU_NOTE_UNUSED(num_quints);
			
			// compute Table 103 row index
			const int range_index = num_bits * 2 + (num_quints ? 1 : 0);

			// Extract bits and tris/quints from value
			const uint32_t bits = val & ((1u << num_bits) - 1);
			const uint32_t D = val >> num_bits;

			assert(D < (num_trits ? 3U : 5U));

			// Now dequantize
			// See Table 103. ASTC weight unquantization parameters
			static const uint32_t C_table[5] = { 50, 28, 23, 13, 11 };
					
			const uint32_t a = bits & 1, b = (bits >> 1) & 1, c = (bits >> 2) & 1;

			const uint32_t A = (a == 0) ? 0 : 0x7F;
						
			uint32_t B = 0;
			if (range_index == 4)
				B = ((b << 6) | (b << 2) | (b << 0));
			else if (range_index == 5)
				B = ((b << 6) | (b << 1));
			else if (range_index == 6)
				B = ((c << 6) | (b << 5) | (c << 1) | (b << 0));

			const uint32_t C = C_table[range_index - 2];

			u = D * C + B;
			u = u ^ A;
			u = (A & 0x20) | (u >> 2);
			break;
		}
		default:
			assert(0);
			break;
		}

		if (u > 32)
			u++;

		return u;
	}

	// Returns the nearest ISE symbol given a [0,255] endpoint value.
	uint32_t find_nearest_bise_endpoint(int v, uint32_t ise_range)
	{
		assert(ise_range >= FIRST_VALID_ENDPOINT_ISE_RANGE && ise_range <= LAST_VALID_ENDPOINT_ISE_RANGE);

		const uint32_t total_levels = get_ise_levels(ise_range);
		int best_e = INT_MAX, best_index = 0;
		for (uint32_t i = 0; i < total_levels; i++)
		{
			const int qv = dequant_bise_endpoint(i, ise_range);
			int e = labs(v - qv);
			if (e < best_e)
			{
				best_e = e;
				best_index = i;
				if (!best_e)
					break;
			}
		}
		return best_index;
	}

	// Returns the nearest ISE weight given a [0,64] endpoint value.
	uint32_t find_nearest_bise_weight(int v, uint32_t ise_range)
	{
		assert(ise_range >= FIRST_VALID_WEIGHT_ISE_RANGE && ise_range <= LAST_VALID_WEIGHT_ISE_RANGE);
		assert(v <= (int)MAX_WEIGHT_VALUE);

		const uint32_t total_levels = get_ise_levels(ise_range);
		int best_e = INT_MAX, best_index = 0;
		for (uint32_t i = 0; i < total_levels; i++)
		{
			const int qv = dequant_bise_weight(i, ise_range);
			int e = labs(v - qv);
			if (e < best_e)
			{
				best_e = e;
				best_index = i;
				if (!best_e)
					break;
			}
		}
		return best_index;
	}

	void create_quant_tables(
		uint8_t* pVal_to_ise,	// [0-255] or [0-64] value to nearest ISE symbol, array size is [256] or [65]
		uint8_t* pISE_to_val,	// ASTC encoded ISE symbol to [0,255] or [0,64] value, [levels]
		uint8_t* pISE_to_rank,	// returns the level rank index given an ISE symbol, [levels]
		uint8_t* pRank_to_ISE,  // returns the ISE symbol given a level rank, inverse of pISE_to_rank, [levels]
		uint32_t ise_range,		// ise range, [4,20] for endpoints, [0,11] for weights
		bool weight_flag)		// false if block endpoints, true if weights
	{
		const uint32_t num_dequant_vals = weight_flag ? (MAX_WEIGHT_VALUE + 1) : 256;

		for (uint32_t i = 0; i < num_dequant_vals; i++)
		{
			uint32_t bise_index = weight_flag ? astc_helpers::find_nearest_bise_weight(i, ise_range) : astc_helpers::find_nearest_bise_endpoint(i, ise_range);

			if (pVal_to_ise)
				pVal_to_ise[i] = (uint8_t)bise_index;

			if (pISE_to_val)
				pISE_to_val[bise_index] = weight_flag ? (uint8_t)astc_helpers::dequant_bise_weight(bise_index, ise_range) : (uint8_t)astc_helpers::dequant_bise_endpoint(bise_index, ise_range);
		}

		if (pISE_to_rank || pRank_to_ISE)
		{
			const uint32_t num_levels = get_ise_levels(ise_range);

			if (!g_ise_range_table[ise_range][1] && !g_ise_range_table[ise_range][2])
			{
				// Only bits
				for (uint32_t i = 0; i < num_levels; i++)
				{
					if (pISE_to_rank)
						pISE_to_rank[i] = (uint8_t)i;

					if (pRank_to_ISE)
						pRank_to_ISE[i] = (uint8_t)i;
				}
			}
			else
			{
				// Range has trits or quints
				uint32_t vals[256];
				for (uint32_t i = 0; i < num_levels; i++)
				{
					uint32_t v = weight_flag ? astc_helpers::dequant_bise_weight(i, ise_range) : astc_helpers::dequant_bise_endpoint(i, ise_range);
					
					// Low=ISE value
					// High=dequantized value
					vals[i] = (v << 16) | i;
				}
				
				// Sorts by dequantized value
				std::sort(vals, vals + num_levels);
				
				for (uint32_t rank = 0; rank < num_levels; rank++)
				{
					uint32_t ise_val = (uint8_t)vals[rank];

					if (pISE_to_rank)
						pISE_to_rank[ise_val] = (uint8_t)rank;
					
					if (pRank_to_ISE)
						pRank_to_ISE[rank] = (uint8_t)ise_val;
				}
			}
		}
	}

	void pack_void_extent_ldr(astc_block &blk, uint16_t rh, uint16_t gh, uint16_t bh, uint16_t ah)
	{
		uint8_t* pDst = (uint8_t*)&blk.m_vals[0];
		memset(pDst, 0xFF, 16);

		pDst[0] = 0b11111100;
		pDst[1] = 0b11111101;

		pDst[8] = (uint8_t)rh;
		pDst[9] = (uint8_t)(rh >> 8);
		pDst[10] = (uint8_t)gh;
		pDst[11] = (uint8_t)(gh >> 8);
		pDst[12] = (uint8_t)bh;
		pDst[13] = (uint8_t)(bh >> 8);
		pDst[14] = (uint8_t)ah;
		pDst[15] = (uint8_t)(ah >> 8);
	}

	// rh-ah are half-floats
	void pack_void_extent_hdr(astc_block& blk, uint16_t rh, uint16_t gh, uint16_t bh, uint16_t ah) 
	{
		uint8_t* pDst = (uint8_t*)&blk.m_vals[0];
		memset(pDst, 0xFF, 16);

		pDst[0] = 0b11111100;
		
		pDst[8] = (uint8_t)rh;
		pDst[9] = (uint8_t)(rh >> 8);
		pDst[10] = (uint8_t)gh;
		pDst[11] = (uint8_t)(gh >> 8);
		pDst[12] = (uint8_t)bh;
		pDst[13] = (uint8_t)(bh >> 8);
		pDst[14] = (uint8_t)ah;
		pDst[15] = (uint8_t)(ah >> 8);
	}
		
	bool is_cem_ldr(uint32_t mode)
	{
		switch (mode)
		{
		case CEM_LDR_LUM_DIRECT:
		case CEM_LDR_LUM_BASE_PLUS_OFS:
		case CEM_LDR_LUM_ALPHA_DIRECT:
		case CEM_LDR_LUM_ALPHA_BASE_PLUS_OFS:
		case CEM_LDR_RGB_BASE_SCALE:
		case CEM_LDR_RGB_DIRECT:
		case CEM_LDR_RGB_BASE_PLUS_OFFSET:
		case CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A:
		case CEM_LDR_RGBA_DIRECT:
		case CEM_LDR_RGBA_BASE_PLUS_OFFSET:
			return true;
		default:
			break;
		}
	
		return false;
	}

	bool is_valid_block_size(uint32_t w, uint32_t h)
	{
		assert((w >= MIN_BLOCK_DIM) && (w <= MAX_BLOCK_DIM));
		assert((h >= MIN_BLOCK_DIM) && (h <= MAX_BLOCK_DIM));

#define SIZECHK(x, y) if ((w == (x)) && (h == (y))) return true;
		SIZECHK(4, 4);
		SIZECHK(5, 4);

		SIZECHK(5, 5);

		SIZECHK(6, 5);
		SIZECHK(6, 6);

		SIZECHK(8, 5);
		SIZECHK(8, 6);
		SIZECHK(10, 5);
		SIZECHK(10, 6);

		SIZECHK(8, 8);
		SIZECHK(10, 8);
		SIZECHK(10, 10);

		SIZECHK(12, 10);
		SIZECHK(12, 12);
#undef SIZECHK

		return false;
	}

	bool block_has_any_hdr_cems(const log_astc_block& log_blk)
	{
		assert((log_blk.m_num_partitions >= 1) && (log_blk.m_num_partitions <= MAX_PARTITIONS));

		for (uint32_t i = 0; i < log_blk.m_num_partitions; i++)
			if (is_cem_hdr(log_blk.m_color_endpoint_modes[i]))
				return true;

		return false;
	}

	bool block_has_any_ldr_cems(const log_astc_block& log_blk)
	{
		assert((log_blk.m_num_partitions >= 1) && (log_blk.m_num_partitions <= MAX_PARTITIONS));

		for (uint32_t i = 0; i < log_blk.m_num_partitions; i++)
			if (!is_cem_hdr(log_blk.m_color_endpoint_modes[i]))
				return true;

		return false;
	}
		
	dequant_tables g_dequant_tables;

	void precompute_texel_partitions_4x4();

	void init_tables(bool init_rank_tabs)
	{
		g_dequant_tables.init(init_rank_tabs);
		
		precompute_texel_partitions_4x4();
	}

	struct weighted_sample
	{
		uint8_t m_src_x;
		uint8_t m_src_y;
		uint8_t m_weights[2][2]; // [y][x], scaled by 16, round by adding 8
	};

	static void compute_upsample_weights(
		int block_width, int block_height,
		int weight_grid_width, int weight_grid_height,
		weighted_sample* pWeights) // there will be block_width * block_height bilinear samples
	{
		const uint32_t scaleX = (1024 + block_width / 2) / (block_width - 1);
		const uint32_t scaleY = (1024 + block_height / 2) / (block_height - 1);

		for (int texelY = 0; texelY < block_height; texelY++)
		{
			for (int texelX = 0; texelX < block_width; texelX++)
			{
				const uint32_t gX = (scaleX * texelX * (weight_grid_width - 1) + 32) >> 6;
				const uint32_t gY = (scaleY * texelY * (weight_grid_height - 1) + 32) >> 6;
				const uint32_t jX = gX >> 4;
				const uint32_t jY = gY >> 4;
				const uint32_t fX = gX & 0xf;
				const uint32_t fY = gY & 0xf;
				const uint32_t w11 = (fX * fY + 8) >> 4;
				const uint32_t w10 = fY - w11;
				const uint32_t w01 = fX - w11;
				const uint32_t w00 = 16 - fX - fY + w11;

				weighted_sample& s = pWeights[texelX + texelY * block_width];
				s.m_src_x = (uint8_t)jX;
				s.m_src_y = (uint8_t)jY;
				s.m_weights[0][0] = (uint8_t)w00;
				s.m_weights[0][1] = (uint8_t)w01;
				s.m_weights[1][0] = (uint8_t)w10;
				s.m_weights[1][1] = (uint8_t)w11;
			}
		}
	}

	// Should be dequantized [0,64] weights
	static void upsample_weight_grid(
		uint32_t bx, uint32_t by,		// destination/to dimension
		uint32_t wx, uint32_t wy,		// source/from dimension
		const uint8_t* pSrc_weights,	// these are dequantized [0,64] weights, NOT ISE symbols, [wy][wx]
		uint8_t* pDst_weights)			// [by][bx]
	{
		assert((bx >= 2) && (by >= 2) && (bx <= 12) && (by <= 12));
		assert((wx >= 2) && (wy >= 2) && (wx <= bx) && (wy <= by));

		const uint32_t total_src_weights = wx * wy;
		const uint32_t total_dst_weights = bx * by;

		if (total_src_weights == total_dst_weights)
		{
			memcpy(pDst_weights, pSrc_weights, total_src_weights);
			return;
		}

		weighted_sample weights[12 * 12];
		compute_upsample_weights(bx, by, wx, wy, weights);

		const weighted_sample* pS = weights;

		for (uint32_t y = 0; y < by; y++)
		{
			for (uint32_t x = 0; x < bx; x++, ++pS)
			{
				const uint32_t w00 = pS->m_weights[0][0];
				const uint32_t w01 = pS->m_weights[0][1];
				const uint32_t w10 = pS->m_weights[1][0];
				const uint32_t w11 = pS->m_weights[1][1];

				assert(w00 || w01 || w10 || w11);

				const uint32_t sx = pS->m_src_x, sy = pS->m_src_y;

				uint32_t total = 8;
				if (w00) total += pSrc_weights[bounds_check(sx + sy * wx, 0U, total_src_weights)] * w00;
				if (w01) total += pSrc_weights[bounds_check(sx + 1 + sy * wx, 0U, total_src_weights)] * w01;
				if (w10) total += pSrc_weights[bounds_check(sx + (sy + 1) * wx, 0U, total_src_weights)] * w10;
				if (w11) total += pSrc_weights[bounds_check(sx + 1 + (sy + 1) * wx, 0U, total_src_weights)] * w11;

				pDst_weights[x + y * bx] = (uint8_t)(total >> 4);
			}
		}
	}

	inline uint32_t hash52(uint32_t v)
	{
		uint32_t p = v;
		p ^= p >> 15;   p -= p << 17;   p += p << 7;    p += p << 4;
		p ^= p >> 5;   p += p << 16;   p ^= p >> 7;    p ^= p >> 3;
		p ^= p << 6;   p ^= p >> 17;
		return p;
	}

	int compute_texel_partition(uint32_t seedIn, uint32_t xIn, uint32_t yIn, uint32_t zIn, int num_partitions, bool small_block)
	{
		assert(zIn == 0);

		const uint32_t  x = small_block ? xIn << 1 : xIn;
		const uint32_t  y = small_block ? yIn << 1 : yIn;
		const uint32_t  z = small_block ? zIn << 1 : zIn;
		const uint32_t  seed = seedIn + 1024 * (num_partitions - 1);
		const uint32_t  rnum = hash52(seed);

		uint8_t         seed1 = (uint8_t)(rnum & 0xf);
		uint8_t         seed2 = (uint8_t)((rnum >> 4) & 0xf);
		uint8_t         seed3 = (uint8_t)((rnum >> 8) & 0xf);
		uint8_t         seed4 = (uint8_t)((rnum >> 12) & 0xf);
		uint8_t         seed5 = (uint8_t)((rnum >> 16) & 0xf);
		uint8_t         seed6 = (uint8_t)((rnum >> 20) & 0xf);
		uint8_t         seed7 = (uint8_t)((rnum >> 24) & 0xf);
		uint8_t         seed8 = (uint8_t)((rnum >> 28) & 0xf);
		uint8_t         seed9 = (uint8_t)((rnum >> 18) & 0xf);
		uint8_t         seed10 = (uint8_t)((rnum >> 22) & 0xf);
		uint8_t         seed11 = (uint8_t)((rnum >> 26) & 0xf);
		uint8_t         seed12 = (uint8_t)(((rnum >> 30) | (rnum << 2)) & 0xf);

		seed1 = (uint8_t)(seed1 * seed1);
		seed2 = (uint8_t)(seed2 * seed2);
		seed3 = (uint8_t)(seed3 * seed3);
		seed4 = (uint8_t)(seed4 * seed4);
		seed5 = (uint8_t)(seed5 * seed5);
		seed6 = (uint8_t)(seed6 * seed6);
		seed7 = (uint8_t)(seed7 * seed7);
		seed8 = (uint8_t)(seed8 * seed8);
		seed9 = (uint8_t)(seed9 * seed9);
		seed10 = (uint8_t)(seed10 * seed10);
		seed11 = (uint8_t)(seed11 * seed11);
		seed12 = (uint8_t)(seed12 * seed12);

		const int shA = (seed & 2) != 0 ? 4 : 5;
		const int shB = (num_partitions == 3) ? 6 : 5;
		const int sh1 = (seed & 1) != 0 ? shA : shB;
		const int sh2 = (seed & 1) != 0 ? shB : shA;
		const int sh3 = (seed & 0x10) != 0 ? sh1 : sh2;

		seed1 = (uint8_t)(seed1 >> sh1);
		seed2 = (uint8_t)(seed2 >> sh2);
		seed3 = (uint8_t)(seed3 >> sh1);
		seed4 = (uint8_t)(seed4 >> sh2);
		seed5 = (uint8_t)(seed5 >> sh1);
		seed6 = (uint8_t)(seed6 >> sh2);
		seed7 = (uint8_t)(seed7 >> sh1);
		seed8 = (uint8_t)(seed8 >> sh2);
		seed9 = (uint8_t)(seed9 >> sh3);
		seed10 = (uint8_t)(seed10 >> sh3);
		seed11 = (uint8_t)(seed11 >> sh3);
		seed12 = (uint8_t)(seed12 >> sh3);

		const int a = 0x3f & (seed1 * x + seed2 * y + seed11 * z + (rnum >> 14));
		const int b = 0x3f & (seed3 * x + seed4 * y + seed12 * z + (rnum >> 10));
		const int c = (num_partitions >= 3) ? 0x3f & (seed5 * x + seed6 * y + seed9 * z + (rnum >> 6)) : 0;
		const int d = (num_partitions >= 4) ? 0x3f & (seed7 * x + seed8 * y + seed10 * z + (rnum >> 2)) : 0;

		return (a >= b && a >= c && a >= d) ? 0
			: (b >= c && b >= d) ? 1
			: (c >= d) ? 2
			: 3;
	}

	static uint32_t g_texel_partitions_4x4[1024][2];

	void precompute_texel_partitions_4x4()
	{
		for (uint32_t p = 0; p < 1024; p++)
		{
			uint32_t v2 = 0, v3 = 0;

			for (uint32_t y = 0; y < 4; y++)
			{
				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t shift = x * 2 + y * 8;
					v2 |= (compute_texel_partition(p, x, y, 0, 2, true) << shift);
					v3 |= (compute_texel_partition(p, x, y, 0, 3, true) << shift);
				}
			}

			g_texel_partitions_4x4[p][0] = v2;
			g_texel_partitions_4x4[p][1] = v3;
		}
	}

	static inline int get_precompute_texel_partitions_4x4(uint32_t seed, uint32_t x, uint32_t y, uint32_t num_partitions)
	{
		assert(g_texel_partitions_4x4[1][0]);
		assert(seed < 1024);
		assert((x <= 3) && (y <= 3));
		assert((num_partitions >= 2) && (num_partitions <= 3));
	
		const uint32_t shift = x * 2 + y * 8;
		return (g_texel_partitions_4x4[seed][num_partitions - 2] >> shift) & 3;
	}

	void blue_contract(
		int r, int g, int b, int a, 
		int &dr, int &dg, int &db, int &da)
	{
		dr = (r + b) >> 1;
		dg = (g + b) >> 1;
		db = b;
		da = a;
	}

	inline void bit_transfer_signed(int& a, int& b)
	{
		b >>= 1;
		b |= (a & 0x80);
		a >>= 1;
		a &= 0x3F;
		if ((a & 0x20) != 0) 
			a -= 0x40;
	}

	static inline int clamp(int a, int l, int h)
	{
		if (a < l)
			a = l;
		else if (a > h)
			a = h;
		return a;
	}

	static inline float clampf(float a, float l, float h)
	{
		if (a < l)
			a = l;
		else if (a > h)
			a = h;
		return a;
	}

	inline int sign_extend(int src, int num_src_bits)
	{
		assert((num_src_bits >= 2) && (num_src_bits <= 31));

		const bool negative = (src & (1 << (num_src_bits - 1))) != 0;
		if (negative)
			return src | ~((1 << num_src_bits) - 1);
		else
			return src & ((1 << num_src_bits) - 1);
	}

	// endpoints is [4][2]
	void decode_endpoint(uint32_t cem_index, int (*pEndpoints)[2], const uint8_t *pE)
	{
		assert(cem_index <= CEM_HDR_RGB_HDR_ALPHA);

		int v0 = pE[0], v1 = pE[1];

		int& e0_r = pEndpoints[0][0], &e0_g = pEndpoints[1][0], &e0_b = pEndpoints[2][0], &e0_a = pEndpoints[3][0];
		int& e1_r = pEndpoints[0][1], &e1_g = pEndpoints[1][1], &e1_b = pEndpoints[2][1], &e1_a = pEndpoints[3][1];

		switch (cem_index)
		{
		case CEM_LDR_LUM_DIRECT:
		{
			e0_r = v0; e1_r = v1;
			e0_g = v0; e1_g = v1;
			e0_b = v0; e1_b = v1;
			e0_a = 0xFF; e1_a = 0xFF;
			break;
		}
		case CEM_LDR_LUM_BASE_PLUS_OFS:
		{
			int l0 = (v0 >> 2) | (v1 & 0xc0);
			int l1 = l0 + (v1 & 0x3f);

			if (l1 > 0xFF)
				l1 = 0xFF;

			e0_r = l0; e1_r = l1;
			e0_g = l0; e1_g = l1;
			e0_b = l0; e1_b = l1;
			e0_a = 0xFF; e1_a = 0xFF;
			break;
		}
		case CEM_LDR_LUM_ALPHA_DIRECT:
		{
			int v2 = pE[2], v3 = pE[3];

			e0_r = v0; e1_r = v1;
			e0_g = v0; e1_g = v1;
			e0_b = v0; e1_b = v1;
			e0_a = v2; e1_a = v3;
			break;
		}
		case CEM_LDR_LUM_ALPHA_BASE_PLUS_OFS:
		{
			int v2 = pE[2], v3 = pE[3];

			bit_transfer_signed(v1, v0);
			bit_transfer_signed(v3, v2);

			e0_r = v0; e1_r = v0 + v1;
			e0_g = v0; e1_g = v0 + v1;
			e0_b = v0; e1_b = v0 + v1;
			e0_a = v2; e1_a = v2 + v3;

			for (uint32_t c = 0; c < 4; c++)
			{
				pEndpoints[c][0] = clamp(pEndpoints[c][0], 0, 255);
				pEndpoints[c][1] = clamp(pEndpoints[c][1], 0, 255);
			}

			break;
		}
		case CEM_LDR_RGB_BASE_SCALE:
		{
			int v2 = pE[2], v3 = pE[3];

			e0_r = (v0 * v3) >> 8; e1_r = v0;
			e0_g = (v1 * v3) >> 8; e1_g = v1;
			e0_b = (v2 * v3) >> 8; e1_b = v2;
			e0_a = 0xFF; e1_a = 0xFF;

			break;
		}
		case CEM_LDR_RGB_DIRECT:
		{
			int v2 = pE[2], v3 = pE[3], v4 = pE[4], v5 = pE[5];

			if ((v1 + v3 + v5) >= (v0 + v2 + v4))
			{
				e0_r = v0; e1_r = v1;
				e0_g = v2; e1_g = v3;
				e0_b = v4; e1_b = v5;
				e0_a = 0xFF; e1_a = 0xFF;
			}
			else
			{
				blue_contract(v1, v3, v5, 0xFF, e0_r, e0_g, e0_b, e0_a);
				blue_contract(v0, v2, v4, 0xFF, e1_r, e1_g, e1_b, e1_a);
			}

			break;
		}
		case CEM_LDR_RGB_BASE_PLUS_OFFSET:
		{
			int v2 = pE[2], v3 = pE[3], v4 = pE[4], v5 = pE[5];

			bit_transfer_signed(v1, v0);
			bit_transfer_signed(v3, v2);
			bit_transfer_signed(v5, v4);

			if ((v1 + v3 + v5) >= 0)
			{
				e0_r = v0; e1_r = v0 + v1;
				e0_g = v2; e1_g = v2 + v3;
				e0_b = v4; e1_b = v4 + v5;
				e0_a = 0xFF; e1_a = 0xFF;
			}
			else
			{
				blue_contract(v0 + v1, v2 + v3, v4 + v5, 0xFF, e0_r, e0_g, e0_b, e0_a);
				blue_contract(v0, v2, v4, 0xFF, e1_r, e1_g, e1_b, e1_a);
			}

			for (uint32_t c = 0; c < 4; c++)
			{
				pEndpoints[c][0] = clamp(pEndpoints[c][0], 0, 255);
				pEndpoints[c][1] = clamp(pEndpoints[c][1], 0, 255);
			}

			break;
		}
		case CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A:
		{
			int v2 = pE[2], v3 = pE[3], v4 = pE[4], v5 = pE[5];

			e0_r = (v0 * v3) >> 8; e1_r = v0;
			e0_g = (v1 * v3) >> 8; e1_g = v1;
			e0_b = (v2 * v3) >> 8; e1_b = v2;
			e0_a = v4; e1_a = v5;

			break;
		}
		case CEM_LDR_RGBA_DIRECT:
		{
			int v2 = pE[2], v3 = pE[3], v4 = pE[4], v5 = pE[5], v6 = pE[6], v7 = pE[7];

			if ((v1 + v3 + v5) >= (v0 + v2 + v4))
			{
				e0_r = v0; e1_r = v1;
				e0_g = v2; e1_g = v3;
				e0_b = v4; e1_b = v5;
				e0_a = v6; e1_a = v7;
			}
			else
			{
				blue_contract(v1, v3, v5, v7, e0_r, e0_g, e0_b, e0_a);
				blue_contract(v0, v2, v4, v6, e1_r, e1_g, e1_b, e1_a);
			}

			break;
		}
		case CEM_LDR_RGBA_BASE_PLUS_OFFSET:
		{
			int v2 = pE[2], v3 = pE[3], v4 = pE[4], v5 = pE[5], v6 = pE[6], v7 = pE[7];

			bit_transfer_signed(v1, v0);
			bit_transfer_signed(v3, v2);
			bit_transfer_signed(v5, v4);
			bit_transfer_signed(v7, v6);

			if ((v1 + v3 + v5) >= 0)
			{
				e0_r = v0; e1_r = v0 + v1;
				e0_g = v2; e1_g = v2 + v3;
				e0_b = v4; e1_b = v4 + v5;
				e0_a = v6; e1_a = v6 + v7;
			}
			else
			{
				blue_contract(v0 + v1, v2 + v3, v4 + v5, v6 + v7, e0_r, e0_g, e0_b, e0_a);
				blue_contract(v0, v2, v4, v6, e1_r, e1_g, e1_b, e1_a);
			}

			for (uint32_t c = 0; c < 4; c++)
			{
				pEndpoints[c][0] = clamp(pEndpoints[c][0], 0, 255);
				pEndpoints[c][1] = clamp(pEndpoints[c][1], 0, 255);
			}

			break;
		}
		case CEM_HDR_LUM_LARGE_RANGE:
		{
			int y0, y1;
			if (v1 >= v0)
			{
				y0 = (v0 << 4);
				y1 = (v1 << 4);
			}
			else
			{
				y0 = (v1 << 4) + 8;
				y1 = (v0 << 4) - 8;
			}

			e0_r = y0; e1_r = y1;
			e0_g = y0; e1_g = y1;
			e0_b = y0; e1_b = y1;
			e0_a = 0x780; e1_a = 0x780;
						
			break;
		}
		case CEM_HDR_LUM_SMALL_RANGE:
		{
			int y0, y1, d;

			if ((v0 & 0x80) != 0)
			{
				y0 = ((v1 & 0xE0) << 4) | ((v0 & 0x7F) << 2);
				d = (v1 & 0x1F) << 2;
			}
			else
			{
				y0 = ((v1 & 0xF0) << 4) | ((v0 & 0x7F) << 1);
				d = (v1 & 0x0F) << 1;
			}
						
			y1 = y0 + d;
			if (y1 > 0xFFF) 
				y1 = 0xFFF;
						
			e0_r = y0; e1_r = y1;
			e0_g = y0; e1_g = y1;
			e0_b = y0; e1_b = y1;
			e0_a = 0x780; e1_a = 0x780;

			break;
		}
		case CEM_HDR_RGB_BASE_SCALE:
		{
			int v2 = pE[2], v3 = pE[3];
						
			int modeval = ((v0 & 0xC0) >> 6) | ((v1 & 0x80) >> 5) | ((v2 & 0x80) >> 4);
			
			int majcomp, mode;
			if ((modeval & 0xC) != 0xC) 
			{
				majcomp = modeval >> 2; 
				mode = modeval & 3;
			}
			else if (modeval != 0xF) 
			{
				majcomp = modeval & 3;  
				mode = 4;
			}
			else 
			{
				majcomp = 0; 
				mode = 5;
			}

			int red = v0 & 0x3f; 
			int green = v1 & 0x1f;
			int blue = v2 & 0x1f; 
			int scale = v3 & 0x1f;

			int x0 = (v1 >> 6) & 1; 
			int x1 = (v1 >> 5) & 1; 
			int x2 = (v2 >> 6) & 1;
			int x3 = (v2 >> 5) & 1; 
			int x4 = (v3 >> 7) & 1; 
			int x5 = (v3 >> 6) & 1;
			int x6 = (v3 >> 5) & 1;

			int ohm = 1 << mode;
			if (ohm & 0x30) green |= x0 << 6;
			if (ohm & 0x3A) green |= x1 << 5;
			if (ohm & 0x30) blue |= x2 << 6;
			if (ohm & 0x3A) blue |= x3 << 5;
			if (ohm & 0x3D) scale |= x6 << 5;
			if (ohm & 0x2D) scale |= x5 << 6;
			if (ohm & 0x04) scale |= x4 << 7;
			if (ohm & 0x3B) red |= x4 << 6;
			if (ohm & 0x04) red |= x3 << 6;
			if (ohm & 0x10) red |= x5 << 7;
			if (ohm & 0x0F) red |= x2 << 7;
			if (ohm & 0x05) red |= x1 << 8;
			if (ohm & 0x0A) red |= x0 << 8;
			if (ohm & 0x05) red |= x0 << 9;
			if (ohm & 0x02) red |= x6 << 9;
			if (ohm & 0x01) red |= x3 << 10;
			if (ohm & 0x02) red |= x5 << 10;

			static const int s_shamts[6] = { 1,1,2,3,4,5 };
			
			const int shamt = s_shamts[mode];
			red <<= shamt; 
			green <<= shamt; 
			blue <<= shamt; 
			scale <<= shamt;

			if (mode != 5) 
			{ 
				green = red - green; 
				blue = red - blue; 
			}

			if (majcomp == 1) 
				std::swap(red, green);

			if (majcomp == 2) 
				std::swap(red, blue);
						
			e1_r = clamp(red, 0, 0xFFF);
			e1_g = clamp(green, 0, 0xFFF);
			e1_b = clamp(blue, 0, 0xFFF);
			e1_a = 0x780;

			e0_r = clamp(red - scale, 0, 0xFFF);
			e0_g = clamp(green - scale, 0, 0xFFF);
			e0_b = clamp(blue - scale, 0, 0xFFF);
			e0_a = 0x780;

			break;
		}
		case CEM_HDR_RGB_HDR_ALPHA:
		case CEM_HDR_RGB_LDR_ALPHA:
		case CEM_HDR_RGB:
		{
			int v2 = pE[2], v3 = pE[3], v4 = pE[4], v5 = pE[5];

			int majcomp = ((v4 & 0x80) >> 7) | ((v5 & 0x80) >> 6);

			e0_a = 0x780;
			e1_a = 0x780;

			if (majcomp == 3) 
			{
				e0_r = v0 << 4;
				e0_g = v2 << 4;
				e0_b = (v4 & 0x7f) << 5;

				e1_r = v1 << 4;
				e1_g = v3 << 4;
				e1_b = (v5 & 0x7f) << 5;
			}
			else
			{
				int mode = ((v1 & 0x80) >> 7) | ((v2 & 0x80) >> 6) | ((v3 & 0x80) >> 5);
				int va = v0 | ((v1 & 0x40) << 2);
				int vb0 = v2 & 0x3f;
				int vb1 = v3 & 0x3f;
				int vc = v1 & 0x3f;
				int vd0 = v4 & 0x7f;
				int vd1 = v5 & 0x7f;

				static const int s_dbitstab[8] = { 7,6,7,6,5,6,5,6 };
				vd0 = sign_extend(vd0, s_dbitstab[mode]);
				vd1 = sign_extend(vd1, s_dbitstab[mode]);

				int x0 = (v2 >> 6) & 1;
				int x1 = (v3 >> 6) & 1;
				int x2 = (v4 >> 6) & 1;
				int x3 = (v5 >> 6) & 1;
				int x4 = (v4 >> 5) & 1;
				int x5 = (v5 >> 5) & 1;

				int ohm = 1 << mode;
				if (ohm & 0xA4) va |= x0 << 9;
				if (ohm & 0x08) va |= x2 << 9;
				if (ohm & 0x50) va |= x4 << 9;
				if (ohm & 0x50) va |= x5 << 10;
				if (ohm & 0xA0) va |= x1 << 10;
				if (ohm & 0xC0) va |= x2 << 11;
				if (ohm & 0x04) vc |= x1 << 6;
				if (ohm & 0xE8) vc |= x3 << 6;
				if (ohm & 0x20) vc |= x2 << 7;
				if (ohm & 0x5B) vb0 |= x0 << 6;
				if (ohm & 0x5B) vb1 |= x1 << 6;
				if (ohm & 0x12) vb0 |= x2 << 7;
				if (ohm & 0x12) vb1 |= x3 << 7;

				int shamt = (mode >> 1) ^ 3;
				va  = (uint32_t)va  << shamt;
				vb0 = (uint32_t)vb0 << shamt;
				vb1 = (uint32_t)vb1 << shamt;
				vc  = (uint32_t)vc  << shamt;
				vd0 = (uint32_t)vd0 << shamt;
				vd1 = (uint32_t)vd1 << shamt;

				e1_r = clamp(va, 0, 0xFFF);
				e1_g = clamp(va - vb0, 0, 0xFFF);
				e1_b = clamp(va - vb1, 0, 0xFFF);

				e0_r = clamp(va - vc, 0, 0xFFF);
				e0_g = clamp(va - vb0 - vc - vd0, 0, 0xFFF);
				e0_b = clamp(va - vb1 - vc - vd1, 0, 0xFFF);

				if (majcomp == 1)
				{
					std::swap(e0_r, e0_g);
					std::swap(e1_r, e1_g);
				}
				else if (majcomp == 2)
				{
					std::swap(e0_r, e0_b);
					std::swap(e1_r, e1_b);
				}
			}

			if (cem_index == CEM_HDR_RGB_LDR_ALPHA)
			{
				int v6 = pE[6], v7 = pE[7];

				e0_a = v6;
				e1_a = v7;
			}
			else if (cem_index == CEM_HDR_RGB_HDR_ALPHA)
			{
				int v6 = pE[6], v7 = pE[7];

				// Extract mode bits
				int mode = ((v6 >> 7) & 1) | ((v7 >> 6) & 2);
				v6 &= 0x7F;
				v7 &= 0x7F;

				if (mode == 3)
				{
					e0_a = v6 << 5;
					e1_a = v7 << 5;
				}
				else
				{
					v6 |= (v7 << (mode + 1)) & 0x780;
					v7 &= (0x3F >> mode);
					v7 ^= (0x20 >> mode);
					v7 -= (0x20 >> mode);
					v6 <<= (4 - mode); 
					v7 <<= (4 - mode);

					v7 += v6;
					v7 = clamp(v7, 0, 0xFFF);
					e0_a = v6; 
					e1_a = v7;
				}
			}

			break;
		}
		default:
		{
			assert(0);
			for (uint32_t c = 0; c < 4; c++)
			{
				pEndpoints[c][0] = 0;
				pEndpoints[c][1] = 0;
			}
			break;
		}
		}
	}
		
	static inline bool is_half_inf_or_nan(half_float v)
	{
		return get_bits(v, 10, 14) == 31;
	}

	// This float->half conversion matches how "F32TO16" works on Intel GPU's.
	half_float float_to_half(float val, bool toward_zero)
	{
		union { float f; int32_t i; uint32_t u; } fi = { val };
		const int flt_m = fi.i & 0x7FFFFF, flt_e = (fi.i >> 23) & 0xFF, flt_s = (fi.i >> 31) & 0x1;
		int s = flt_s, e = 0, m = 0;

		// inf/NaN
		if (flt_e == 0xff)
		{
			e = 31;
			if (flt_m != 0) // NaN
				m = 1;
		}
		// not zero or denormal
		else if (flt_e != 0)
		{
			int new_exp = flt_e - 127;
			if (new_exp > 15)
				e = 31;
			else if (new_exp < -14)
			{
				if (toward_zero)
					m = (int)truncf((1 << 24) * fabsf(fi.f));
				else
					m = lrintf((1 << 24) * fabsf(fi.f));
			}
			else
			{
				e = new_exp + 15;
				if (toward_zero)
					m = (int)truncf((float)flt_m * (1.0f / (float)(1 << 13)));
				else
					m = lrintf((float)flt_m * (1.0f / (float)(1 << 13)));
			}
		}

		assert((0 <= m) && (m <= 1024));
		if (m == 1024)
		{
			e++;
			m = 0;
		}

		assert((s >= 0) && (s <= 1));
		assert((e >= 0) && (e <= 31));
		assert((m >= 0) && (m <= 1023));

		half_float result = (half_float)((s << 15) | (e << 10) | m);
		return result;
	}

	float half_to_float(half_float hval)
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

	static inline half_float qlog16_to_half(int k)
	{
		assert((k >= 0) && (k <= 0xFFFF));

		int E = (k & 0xF800) >> 11;
		int M = k & 0x7FF;

		int Mt;
		if (M < 512)
			Mt = 3 * M;
		else if (M >= 1536)
			Mt = 5 * M - 2048;
		else
			Mt = 4 * M - 512;

		return (half_float)((E << 10) + (Mt >> 3));
	}

	// See https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_shared_exponent.txt
	const int RGB9E5_EXPONENT_BITS = 5, RGB9E5_MANTISSA_BITS = 9, RGB9E5_EXP_BIAS = 15, RGB9E5_MAX_VALID_BIASED_EXP = 31;
	const int MAX_RGB9E5_EXP = (RGB9E5_MAX_VALID_BIASED_EXP - RGB9E5_EXP_BIAS);
	const int RGB9E5_MANTISSA_VALUES = (1 << RGB9E5_MANTISSA_BITS);
	const int MAX_RGB9E5_MANTISSA = (RGB9E5_MANTISSA_VALUES - 1);
	//const int MAX_RGB9E5 = (int)(((float)MAX_RGB9E5_MANTISSA) / RGB9E5_MANTISSA_VALUES * (1 << MAX_RGB9E5_EXP));
	const int EPSILON_RGB9E5 = (int)((1.0f / (float)RGB9E5_MANTISSA_VALUES) / (float)(1 << RGB9E5_EXP_BIAS));
		
	void unpack_rgb9e5(uint32_t packed, float& r, float& g, float& b)
	{
		int x = packed & 511;
		int y = (packed >> 9) & 511;
		int z = (packed >> 18) & 511;
		int w = (packed >> 27) & 31;

		const float scale = powf(2.0f, static_cast<float>(w - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS));

		r = x * scale;
		g = y * scale;
		b = z * scale;
	}
			
	// floor_log2 is not correct for the denorm and zero values, but we are going to do a max of this value with the minimum rgb9e5 exponent that will hide these problem cases.
	static inline int floor_log2(float x) 
	{
		union float754
		{
			unsigned int raw;
			float value;
		};

		float754 f;
		f.value = x;
		// Extract float exponent
		return ((f.raw >> 23) & 0xFF) - 127;
	}

	static inline int maximumi(int a, int b) { return (a > b) ? a : b; }
	static inline float maximumf(float a, float b) { return (a > b) ? a : b; }

	uint32_t pack_rgb9e5(float r, float g, float b)
	{
		r = clampf(r, 0.0f, MAX_RGB9E5);
		g = clampf(g, 0.0f, MAX_RGB9E5);
		b = clampf(b, 0.0f, MAX_RGB9E5);

		float maxrgb = maximumf(maximumf(r, g), b);
		int exp_shared = maximumi(-RGB9E5_EXP_BIAS - 1, floor_log2(maxrgb)) + 1 + RGB9E5_EXP_BIAS;
		assert((exp_shared >= 0) && (exp_shared <= RGB9E5_MAX_VALID_BIASED_EXP));

		float denom = powf(2.0f, (float)(exp_shared - RGB9E5_EXP_BIAS - RGB9E5_MANTISSA_BITS));

		int maxm = (int)floorf((maxrgb / denom) + 0.5f);
		if (maxm == (MAX_RGB9E5_MANTISSA + 1))
		{
			denom *= 2;
			exp_shared += 1;
			assert(exp_shared <= RGB9E5_MAX_VALID_BIASED_EXP);
		}
		else 
		{
			assert(maxm <= MAX_RGB9E5_MANTISSA);
		}

		int rm = (int)floorf((r / denom) + 0.5f);
		int gm = (int)floorf((g / denom) + 0.5f);
		int bm = (int)floorf((b / denom) + 0.5f);

		assert((rm >= 0) && (rm <= MAX_RGB9E5_MANTISSA));
		assert((gm >= 0) && (gm <= MAX_RGB9E5_MANTISSA));
		assert((bm >= 0) && (bm <= MAX_RGB9E5_MANTISSA));
		
		return rm | (gm << 9) | (bm << 18) | (exp_shared << 27);
	}

	static inline int clz17(uint32_t x)
	{
		assert(x <= 0x1FFFF);
		x &= 0x1FFFF;

		if (!x)
			return 17;
				
		uint32_t n = 0;
		while ((x & 0x10000) == 0)
		{
			x <<= 1u;
			n++;
		}

		return n;
	}

	static inline uint32_t pack_rgb9e5_ldr_astc(int Cr, int Cg, int Cb)
	{
		int lz = clz17(Cr | Cg | Cb | 1);
		if (Cr == 65535) { Cr = 65536; lz = 0; }
		if (Cg == 65535) { Cg = 65536; lz = 0; }
		if (Cb == 65535) { Cb = 65536; lz = 0; }
		Cr <<= lz; Cg <<= lz; Cb <<= lz;
		Cr = (Cr >> 8) & 0x1FF;
		Cg = (Cg >> 8) & 0x1FF;
		Cb = (Cb >> 8) & 0x1FF;
		uint32_t exponent = 16 - lz;
		uint32_t texel = (exponent << 27) | (Cb << 18) | (Cg << 9) | Cr;
		return texel;
	}

	static inline uint32_t pack_rgb9e5_hdr_astc(int Cr, int Cg, int Cb)
	{
		if (Cr > 0x7c00) Cr = 0; else if (Cr == 0x7c00) Cr = 0x7bff;
		if (Cg > 0x7c00) Cg = 0; else if (Cg == 0x7c00) Cg = 0x7bff;
		if (Cb > 0x7c00) Cb = 0; else if (Cb == 0x7c00) Cb = 0x7bff;
		int Re = (Cr >> 10) & 0x1F;
		int Ge = (Cg >> 10) & 0x1F;
		int Be = (Cb >> 10) & 0x1F;
		int Rex = (Re == 0) ? 1 : Re;
		int Gex = (Ge == 0) ? 1 : Ge;
		int Bex = (Be == 0) ? 1 : Be;
		int Xm = ((Cr | Cg | Cb) & 0x200) >> 9;
		int Xe = Re | Ge | Be;
		uint32_t rshift, gshift, bshift, expo;

		if (Xe == 0)
		{
			expo = rshift = gshift = bshift = Xm;
		}
		else if (Re >= Ge && Re >= Be)
		{
			expo = Rex + 1;
			rshift = 2;
			gshift = Rex - Gex + 2;
			bshift = Rex - Bex + 2;
		}
		else if (Ge >= Be)
		{
			expo = Gex + 1;
			rshift = Gex - Rex + 2;
			gshift = 2;
			bshift = Gex - Bex + 2;
		}
		else
		{
			expo = Bex + 1;
			rshift = Bex - Rex + 2;
			gshift = Bex - Gex + 2;
			bshift = 2;
		}

		int Rm = (Cr & 0x3FF) | (Re == 0 ? 0 : 0x400);
		int Gm = (Cg & 0x3FF) | (Ge == 0 ? 0 : 0x400);
		int Bm = (Cb & 0x3FF) | (Be == 0 ? 0 : 0x400);
		Rm = (Rm >> rshift) & 0x1FF;
		Gm = (Gm >> gshift) & 0x1FF;
		Bm = (Bm >> bshift) & 0x1FF;

		uint32_t texel = (expo << 27) | (Bm << 18) | (Gm << 9) | (Rm << 0);
		return texel;
	}
		
	// Important: pPixels is either 32-bit/texel or 64-bit/texel.
	bool decode_block(const log_astc_block& log_blk, void* pPixels, uint32_t blk_width, uint32_t blk_height, decode_mode dec_mode)
	{
		assert(is_valid_block_size(blk_width, blk_height));
				
		assert(g_dequant_tables.m_endpoints[0].m_ISE_to_val.size());
		if (!g_dequant_tables.m_endpoints[0].m_ISE_to_val.size())
			return false;

		const uint32_t num_blk_pixels = blk_width * blk_height;
		
		// Write block error color
		if (dec_mode == cDecodeModeHDR16)
		{
			// NaN's
			memset(pPixels, 0xFF, num_blk_pixels * sizeof(half_float) * 4);
		}
		else if (dec_mode == cDecodeModeRGB9E5)
		{
			const uint32_t purple_9e5 = pack_rgb9e5(1.0f, 0.0f, 1.0f);

			for (uint32_t i = 0; i < num_blk_pixels; i++)
				((uint32_t*)pPixels)[i] = purple_9e5;
		}
		else
		{
			for (uint32_t i = 0; i < num_blk_pixels; i++)
				((uint32_t*)pPixels)[i] = 0xFFFF00FF;
		}

		if (log_blk.m_error_flag)
		{
			// Should this return false? It's not an invalid logical block config, though.
			return false;
		}

		// Handle solid color blocks
		if (log_blk.m_solid_color_flag_ldr)
		{
			// LDR solid block
			if (dec_mode == cDecodeModeHDR16)
			{
				// Convert LDR pixels to half-float
				half_float h[4];
				for (uint32_t c = 0; c < 4; c++)
					h[c] = (log_blk.m_solid_color[c] == 0xFFFF) ? 0x3C00 : float_to_half((float)log_blk.m_solid_color[c] * (1.0f / 65536.0f), true);

				for (uint32_t i = 0; i < num_blk_pixels; i++)
					memcpy((uint16_t*)pPixels + i * 4, h, sizeof(half_float) * 4);
			}
			else if (dec_mode == cDecodeModeRGB9E5)
			{
				float r = (log_blk.m_solid_color[0] == 0xFFFF) ? 1.0f : ((float)log_blk.m_solid_color[0] * (1.0f / 65536.0f));
				float g = (log_blk.m_solid_color[1] == 0xFFFF) ? 1.0f : ((float)log_blk.m_solid_color[1] * (1.0f / 65536.0f));
				float b = (log_blk.m_solid_color[2] == 0xFFFF) ? 1.0f : ((float)log_blk.m_solid_color[2] * (1.0f / 65536.0f));

				const uint32_t packed = pack_rgb9e5(r, g, b);

				for (uint32_t i = 0; i < num_blk_pixels; i++)
					((uint32_t*)pPixels)[i] = packed;
			}
			else
			{
				// Convert LDR pixels to 8-bits
				for (uint32_t i = 0; i < num_blk_pixels; i++)
					for (uint32_t c = 0; c < 4; c++)
						((uint8_t*)pPixels)[i * 4 + c] = (log_blk.m_solid_color[c] >> 8);
			}

			return true;
		}
		else if (log_blk.m_solid_color_flag_hdr)
		{
			// HDR solid block, decode mode must be half-float or RGB9E5
			if (dec_mode == cDecodeModeHDR16)
			{
				for (uint32_t i = 0; i < num_blk_pixels; i++)
					memcpy((uint16_t*)pPixels + i * 4, log_blk.m_solid_color, sizeof(half_float) * 4);
			}
			else if (dec_mode == cDecodeModeRGB9E5)
			{
				float r = half_to_float(log_blk.m_solid_color[0]);
				float g = half_to_float(log_blk.m_solid_color[1]);
				float b = half_to_float(log_blk.m_solid_color[2]);
				
				const uint32_t packed = pack_rgb9e5(r, g, b);

				for (uint32_t i = 0; i < num_blk_pixels; i++)
					((uint32_t*)pPixels)[i] = packed;
			}
			else
			{
				return false;
			}

			return true;
		}
						
		// Sanity check block's config
		if ((log_blk.m_grid_width < 2) || (log_blk.m_grid_height < 2))
			return false;
		if ((log_blk.m_grid_width > blk_width) || (log_blk.m_grid_height > blk_height))
			return false;

		if ((log_blk.m_endpoint_ise_range < FIRST_VALID_ENDPOINT_ISE_RANGE) || (log_blk.m_endpoint_ise_range > LAST_VALID_ENDPOINT_ISE_RANGE))
			return false;
		if ((log_blk.m_weight_ise_range < FIRST_VALID_WEIGHT_ISE_RANGE) || (log_blk.m_weight_ise_range > LAST_VALID_WEIGHT_ISE_RANGE))
			return false;
		if ((log_blk.m_num_partitions < 1) || (log_blk.m_num_partitions > MAX_PARTITIONS))
			return false;
		if ((log_blk.m_dual_plane) && (log_blk.m_num_partitions > MAX_DUAL_PLANE_PARTITIONS))
			return false;
		if (log_blk.m_partition_id >= NUM_PARTITION_PATTERNS)
			return false;
		if ((log_blk.m_num_partitions == 1) && (log_blk.m_partition_id > 0))
			return false;
		if (log_blk.m_color_component_selector > 3)
			return false;

		const uint32_t total_endpoint_levels = get_ise_levels(log_blk.m_endpoint_ise_range);
		const uint32_t total_weight_levels = get_ise_levels(log_blk.m_weight_ise_range);
				
		bool is_ldr_endpoints[MAX_PARTITIONS];

		// Check CEM's
		uint32_t total_cem_vals = 0;
		for (uint32_t i = 0; i < log_blk.m_num_partitions; i++)
		{
			if (log_blk.m_color_endpoint_modes[i] > 15)
				return false;

			total_cem_vals += get_num_cem_values(log_blk.m_color_endpoint_modes[i]);
			
			is_ldr_endpoints[i] = is_cem_ldr(log_blk.m_color_endpoint_modes[i]);
		}

		if (total_cem_vals > MAX_ENDPOINTS)
			return false;

		const dequant_table& endpoint_dequant_tab = g_dequant_tables.get_endpoint_tab(log_blk.m_endpoint_ise_range);
		const uint8_t* pEndpoint_dequant = endpoint_dequant_tab.m_ISE_to_val.data();

		// Dequantized endpoints to [0,255]
		uint8_t dequantized_endpoints[MAX_ENDPOINTS];
		for (uint32_t i = 0; i < total_cem_vals; i++)
		{
			if (log_blk.m_endpoints[i] >= total_endpoint_levels)
				return false;
			dequantized_endpoints[i] = pEndpoint_dequant[log_blk.m_endpoints[i]];
		}
				
		// Dequantize weights to [0,64]
		uint8_t dequantized_weights[2][12 * 12];
		
		const dequant_table& weight_dequant_tab = g_dequant_tables.get_weight_tab(log_blk.m_weight_ise_range);
		const uint8_t* pWeight_dequant = weight_dequant_tab.m_ISE_to_val.data();
		
		const uint32_t total_weight_vals = (log_blk.m_dual_plane ? 2 : 1) * log_blk.m_grid_width * log_blk.m_grid_height;
		for (uint32_t i = 0; i < total_weight_vals; i++)
		{
			if (log_blk.m_weights[i] >= total_weight_levels)
				return false;

			const uint32_t plane_index = log_blk.m_dual_plane ? (i & 1) : 0;
			const uint32_t grid_index = log_blk.m_dual_plane ? (i >> 1) : i;

			dequantized_weights[plane_index][grid_index] = pWeight_dequant[log_blk.m_weights[i]];
		}

		// Upsample weight grid. [0,64] weights
		uint8_t upsampled_weights[2][12 * 12];

		upsample_weight_grid(blk_width, blk_height, log_blk.m_grid_width, log_blk.m_grid_height, &dequantized_weights[0][0], &upsampled_weights[0][0]);
		if (log_blk.m_dual_plane)
			upsample_weight_grid(blk_width, blk_height, log_blk.m_grid_width, log_blk.m_grid_height, &dequantized_weights[1][0], &upsampled_weights[1][0]);

		// Decode CEM's
		int endpoints[4][4][2]; // [subset][comp][l/h]

		uint32_t endpoint_val_index = 0;
		for (uint32_t subset = 0; subset < log_blk.m_num_partitions; subset++)
		{
			const uint32_t cem_index = log_blk.m_color_endpoint_modes[subset];

			decode_endpoint(cem_index, &endpoints[subset][0], &dequantized_endpoints[endpoint_val_index]);

			endpoint_val_index += get_num_cem_values(cem_index);
		}

		// Decode texels
		const bool small_block = num_blk_pixels < 31;
		const bool use_precomputed_texel_partitions = (blk_width == 4) && (blk_height == 4) && (log_blk.m_num_partitions >= 2) && (log_blk.m_num_partitions <= 3);
		const uint32_t ccs = log_blk.m_dual_plane ? log_blk.m_color_component_selector : UINT32_MAX;
		
		bool success = true;

		if (dec_mode == cDecodeModeRGB9E5)
		{
			// returns uint32_t's
			for (uint32_t y = 0; y < blk_height; y++)
			{
				for (uint32_t x = 0; x < blk_width; x++)
				{
					const uint32_t pixel_index = x + y * blk_width;
					const uint32_t subset = (log_blk.m_num_partitions > 1) ? 
						(use_precomputed_texel_partitions ? get_precompute_texel_partitions_4x4(log_blk.m_partition_id, x, y, log_blk.m_num_partitions) : compute_texel_partition(log_blk.m_partition_id, x, y, 0, log_blk.m_num_partitions, small_block))
						: 0;

					int comp[3];

					for (uint32_t c = 0; c < 3; c++)
					{
						const uint32_t w = upsampled_weights[(c == ccs) ? 1 : 0][pixel_index];

						if (is_ldr_endpoints[subset])
						{
							assert((endpoints[subset][c][0] >= 0) && (endpoints[subset][c][0] <= 0xFF));
							assert((endpoints[subset][c][1] >= 0) && (endpoints[subset][c][1] <= 0xFF));

							int le = endpoints[subset][c][0];
							int he = endpoints[subset][c][1];

							le = (le << 8) | le;
							he = (he << 8) | he;

							int k = weight_interpolate(le, he, w);
							assert((k >= 0) && (k <= 0xFFFF));

							comp[c] = k; // 1.0
						}
						else
						{
							assert((endpoints[subset][c][0] >= 0) && (endpoints[subset][c][0] <= 0xFFF));
							assert((endpoints[subset][c][1] >= 0) && (endpoints[subset][c][1] <= 0xFFF));

							int le = endpoints[subset][c][0] << 4;
							int he = endpoints[subset][c][1] << 4;

							int qlog16 = weight_interpolate(le, he, w);

							comp[c] = qlog16_to_half(qlog16);

							if (is_half_inf_or_nan((half_float)comp[c]))
								comp[c] = 0x7BFF;
						}
						
					} // c

					uint32_t packed;
					if (is_ldr_endpoints[subset])
						packed = pack_rgb9e5_ldr_astc(comp[0], comp[1], comp[2]);
					else
						packed = pack_rgb9e5_hdr_astc(comp[0], comp[1], comp[2]);

					((uint32_t*)pPixels)[pixel_index] = packed;

				} // x
			} // y
		}
		else if (dec_mode == cDecodeModeHDR16)
		{
			// Note: must round towards zero when converting float to half for ASTC (18.19 Weight Application)
			
			// returns half floats
			for (uint32_t y = 0; y < blk_height; y++)
			{
				for (uint32_t x = 0; x < blk_width; x++)
				{
					const uint32_t pixel_index = x + y * blk_width;
					const uint32_t subset = (log_blk.m_num_partitions > 1) ?
						(use_precomputed_texel_partitions ? get_precompute_texel_partitions_4x4(log_blk.m_partition_id, x, y, log_blk.m_num_partitions) : compute_texel_partition(log_blk.m_partition_id, x, y, 0, log_blk.m_num_partitions, small_block))
						: 0;

					for (uint32_t c = 0; c < 4; c++)
					{
						const uint32_t w = upsampled_weights[(c == ccs) ? 1 : 0][pixel_index];

						half_float o;

						if ( (is_ldr_endpoints[subset]) ||
							 ((log_blk.m_color_endpoint_modes[subset] == CEM_HDR_RGB_LDR_ALPHA) && (c == 3)) )
						{
							assert((endpoints[subset][c][0] >= 0) && (endpoints[subset][c][0] <= 0xFF));
							assert((endpoints[subset][c][1] >= 0) && (endpoints[subset][c][1] <= 0xFF));

							int le = endpoints[subset][c][0];
							int he = endpoints[subset][c][1];

							le = (le << 8) | le;
							he = (he << 8) | he;

							int k = weight_interpolate(le, he, w);
							assert((k >= 0) && (k <= 0xFFFF));

							if (k == 0xFFFF)
								o = 0x3C00; // 1.0
							else
								o = float_to_half((float)k * (1.0f / 65536.0f), true);
						}
						else
						{
							assert((endpoints[subset][c][0] >= 0) && (endpoints[subset][c][0] <= 0xFFF));
							assert((endpoints[subset][c][1] >= 0) && (endpoints[subset][c][1] <= 0xFFF));

							int le = endpoints[subset][c][0] << 4;
							int he = endpoints[subset][c][1] << 4;

							int qlog16 = weight_interpolate(le, he, w);
							
							o = qlog16_to_half(qlog16);

							if (is_half_inf_or_nan(o))
								o = 0x7BFF;
						}
												
						((half_float*)pPixels)[pixel_index * 4 + c] = o;
					}

				} // x
			} // y
		}
		else
		{
			// returns uint8_t's
			for (uint32_t y = 0; y < blk_height; y++)
			{
				for (uint32_t x = 0; x < blk_width; x++)
				{
					const uint32_t pixel_index = x + y * blk_width;

					const uint32_t subset = (log_blk.m_num_partitions > 1) ?
						(use_precomputed_texel_partitions ? get_precompute_texel_partitions_4x4(log_blk.m_partition_id, x, y, log_blk.m_num_partitions) : compute_texel_partition(log_blk.m_partition_id, x, y, 0, log_blk.m_num_partitions, small_block))
						: 0;

					if (!is_ldr_endpoints[subset])
					{
						((uint32_t*)pPixels)[pixel_index * 4] = 0xFFFF00FF;
						success = false;
					}
					else
					{
						for (uint32_t c = 0; c < 4; c++)
						{
							const uint32_t w = upsampled_weights[(c == ccs) ? 1 : 0][pixel_index];

							int le = endpoints[subset][c][0];
							int he = endpoints[subset][c][1];

							// FIXME: the spec is apparently wrong? this matches ARM's and Google's decoder
							//if ((dec_mode == cDecodeModeSRGB8) && (c <= 2))
							// See https://github.com/ARM-software/astc-encoder/issues/447
							if (dec_mode == cDecodeModeSRGB8)
							{
								le = (le << 8) | 0x80;
								he = (he << 8) | 0x80;
							}
							else
							{
								le = (le << 8) | le;
								he = (he << 8) | he;
							}

							uint32_t k = weight_interpolate(le, he, w);

							// FIXME: This is what the spec says to do in LDR mode, but this is not what ARM's decoder does
							// See decompress_symbolic_block(), decode_texel() and unorm16_to_sf16. 
							// It seems to effectively divide by 65535.0 and convert to FP16, then back to float, mul by 255.0, add .5 and then convert to 8-bit.
							((uint8_t*)pPixels)[pixel_index * 4 + c] = (uint8_t)(k >> 8);
						}
					}

				} // x
			} // y
		}
		
		return success;
	}

	//------------------------------------------------
	// Physical to logical block decoding

	// unsigned 128-bit int, with some signed helpers
	class uint128
	{
		uint64_t m_lo, m_hi;

	public:
		uint128() = default;
		inline uint128(uint64_t lo) : m_lo(lo), m_hi(0) { }
		inline uint128(uint64_t lo, uint64_t hi) : m_lo(lo), m_hi(hi) { }
		inline uint128(const uint128& other) : m_lo(other.m_lo), m_hi(other.m_hi) { }

		inline uint128& set_signed(int64_t lo) { m_lo = lo; m_hi = (lo < 0) ? UINT64_MAX : 0; return *this; }
		inline uint128& set(uint64_t lo) { m_lo = lo; m_hi = 0; return *this; }

		inline explicit operator uint8_t () const { return (uint8_t)m_lo; }
		inline explicit operator uint16_t () const { return (uint16_t)m_lo; }
		inline explicit operator uint32_t () const { return (uint32_t)m_lo; }
		inline explicit operator uint64_t () const { return m_lo; }

		inline uint128& operator= (const uint128& rhs) { m_lo = rhs.m_lo; m_hi = rhs.m_hi; return *this; }
		inline uint128& operator= (const uint64_t val) { m_lo = val; m_hi = 0; return *this; }

		inline uint64_t get_low() const { return m_lo; }
		inline uint64_t& get_low() { return m_lo; }

		inline uint64_t get_high() const { return m_hi; }
		inline uint64_t& get_high() { return m_hi; }

		inline bool operator== (const uint128& rhs) const { return (m_lo == rhs.m_lo) && (m_hi == rhs.m_hi); }
		inline bool operator!= (const uint128& rhs) const { return (m_lo != rhs.m_lo) || (m_hi != rhs.m_hi); }

		inline bool operator< (const uint128& rhs) const
		{
			if (m_hi < rhs.m_hi)
				return true;

			if (m_hi == rhs.m_hi)
			{
				if (m_lo < rhs.m_lo)
					return true;
			}

			return false;
		}

		inline bool operator> (const uint128& rhs) const { return (rhs < *this); }

		inline bool operator<= (const uint128& rhs) const { return (*this == rhs) || (*this < rhs); }
		inline bool operator>= (const uint128& rhs) const { return (*this == rhs) || (*this > rhs); }

		inline bool is_zero() const { return (m_lo == 0) && (m_hi == 0); }
		inline bool is_all_ones() const { return (m_lo == UINT64_MAX) && (m_hi == UINT64_MAX); }
		inline bool is_non_zero() const { return (m_lo != 0) || (m_hi != 0); }
		inline explicit operator bool() const { return is_non_zero(); }
		inline bool is_signed() const { return ((int64_t)m_hi) < 0; }

		inline bool signed_less(const uint128& rhs) const
		{
			const bool l_signed = is_signed(), r_signed = rhs.is_signed();

			if (l_signed == r_signed)
				return *this < rhs;

			if (l_signed && !r_signed)
				return true;

			assert(!l_signed && r_signed);
			return false;
		}

		inline bool signed_greater(const uint128& rhs) const { return rhs.signed_less(*this); }
		inline bool signed_less_equal(const uint128& rhs) const { return !rhs.signed_less(*this); }
		inline bool signed_greater_equal(const uint128& rhs) const { return !signed_less(rhs); }

		double get_double() const
		{
			double res = 0;

			if (m_hi)
				res = (double)m_hi * pow(2.0f, 64.0f);

			res += (double)m_lo;

			return res;
		}

		double get_signed_double() const
		{
			if (is_signed())
				return -(uint128(*this).abs().get_double());
			else
				return get_double();
		}

		inline uint128 abs() const
		{
			uint128 res(*this);
			if (res.is_signed())
				res = -res;
			return res;
		}

		inline uint128& operator<<= (int shift)
		{
			assert(shift >= 0);
			if (shift < 0)
				return *this;

			m_hi = (shift >= 64) ? ((shift >= 128) ? 0 : (m_lo << (shift - 64))) : (m_hi << shift);

			if ((shift) && (shift < 64))
				m_hi |= (m_lo >> (64 - shift));

			m_lo = (shift >= 64) ? 0 : (m_lo << shift);

			return *this;
		}

		inline uint128 operator<< (int shift) const { uint128 res(*this); res <<= shift; return res; }

		inline uint128& operator>>= (int shift)
		{
			assert(shift >= 0);
			if (shift < 0)
				return *this;

			m_lo = (shift >= 64) ? ((shift >= 128) ? 0 : (m_hi >> (shift - 64))) : (m_lo >> shift);

			if ((shift) && (shift < 64))
				m_lo |= (m_hi << (64 - shift));

			m_hi = (shift >= 64) ? 0 : (m_hi >> shift);

			return *this;
		}

		inline uint128 operator>> (int shift) const { uint128 res(*this); res >>= shift; return res; }

		inline uint128 signed_shift_right(int shift) const
		{
			uint128 res(*this);
			res >>= shift;

			if (is_signed())
			{
				uint128 x(0U);
				x = ~x;
				x >>= shift;
				res |= (~x);
			}

			return res;
		}

		inline uint128& operator |= (const uint128& rhs) { m_lo |= rhs.m_lo; m_hi |= rhs.m_hi; return *this; }
		inline uint128 operator | (const uint128& rhs) const { uint128 res(*this); res |= rhs; return res; }

		inline uint128& operator &= (const uint128& rhs) { m_lo &= rhs.m_lo; m_hi &= rhs.m_hi; return *this; }
		inline uint128 operator & (const uint128& rhs) const { uint128 res(*this); res &= rhs;	return res; }

		inline uint128& operator ^= (const uint128& rhs) { m_lo ^= rhs.m_lo; m_hi ^= rhs.m_hi; return *this; }
		inline uint128 operator ^ (const uint128& rhs) const { uint128 res(*this); res ^= rhs;	return res; }

		inline uint128 operator ~() const { return uint128(~m_lo, ~m_hi); }

		inline uint128 operator -() const { uint128 res(~*this); if (++res.m_lo == 0) ++res.m_hi; return res; }

		// prefix
		inline uint128 operator ++()
		{
			if (++m_lo == 0)
				++m_hi;
			return *this;
		}

		// postfix
		inline uint128 operator ++(int)
		{
			uint128 res(*this);
			if (++m_lo == 0)
				++m_hi;
			return res;
		}

		// prefix
		inline uint128 operator --()
		{
			const uint64_t t = m_lo;
			if (--m_lo > t)
				--m_hi;
			return *this;
		}

		// postfix
		inline uint128 operator --(int)
		{
			const uint64_t t = m_lo;
			uint128 res(*this);
			if (--m_lo > t)
				--m_hi;
			return res;
		}

		inline uint128& operator+= (const uint128& rhs)
		{
			const uint64_t t = m_lo + rhs.m_lo;
			m_hi = m_hi + rhs.m_hi + (t < m_lo);
			m_lo = t;
			return *this;
		}

		inline uint128 operator+ (const uint128& rhs) const { uint128 res(*this); res += rhs; return res; }

		inline uint128& operator-= (const uint128& rhs)
		{
			const uint64_t t = m_lo - rhs.m_lo;
			m_hi = m_hi - rhs.m_hi - (t > m_lo);
			m_lo = t;
			return *this;
		}

		inline uint128 operator- (const uint128& rhs) const { uint128 res(*this); res -= rhs; return res; }

		// computes bit by bit, very slow
		uint128& operator*=(const uint128& rhs)
		{
			uint128 temp(*this), result(0U);

			for (uint128 bitmask(rhs); bitmask; bitmask >>= 1, temp <<= 1)
				if (bitmask.get_low() & 1)
					result += temp;

			*this = result;
			return *this;
		}

		uint128 operator*(const uint128& rhs) const { uint128 res(*this); res *= rhs; return res; }

		// computes bit by bit, very slow
		friend uint128 divide(const uint128& dividend, const uint128& divisor, uint128& remainder)
		{
			remainder = 0;

			if (!divisor)
			{
				assert(0);
				return ~uint128(0U);
			}

			uint128 quotient(0), one(1);

			for (int i = 127; i >= 0; i--)
			{
				remainder = (remainder << 1) | ((dividend >> i) & one);
				if (remainder >= divisor)
				{
					remainder -= divisor;
					quotient |= (one << i);
				}
			}

			return quotient;
		}

		uint128 operator/(const uint128& rhs) const { uint128 remainder, res; res = divide(*this, rhs, remainder); return res; }
		uint128 operator/=(const uint128& rhs) { uint128 remainder; *this = divide(*this, rhs, remainder); return *this; }

		uint128 operator%(const uint128& rhs) const { uint128 remainder; divide(*this, rhs, remainder); return remainder; }
		uint128 operator%=(const uint128& rhs) { uint128 remainder; divide(*this, rhs, remainder); *this = remainder; return *this; }

		void print_hex(FILE* pFile) const
		{
			fprintf(pFile, "0x%016llx%016llx", (unsigned long long int)m_hi, (unsigned long long int)m_lo);
		}

		void format_unsigned(std::string& res) const
		{
			basisu::vector<uint8_t> digits;
			digits.reserve(39 + 1);

			uint128 k(*this), ten(10);
			do
			{
				uint128 r;
				k = divide(k, ten, r);
				digits.push_back((uint8_t)r);
			} while (k);

			for (int i = (int)digits.size() - 1; i >= 0; i--)
				res += ('0' + digits[i]);
		}

		void format_signed(std::string& res) const
		{
			uint128 val(*this);

			if (val.is_signed())
			{
				res.push_back('-');
				val = -val;
			}

			val.format_unsigned(res);
		}

		void print_unsigned(FILE* pFile)
		{
			std::string str;
			format_unsigned(str);
			fprintf(pFile, "%s", str.c_str());
		}

		void print_signed(FILE* pFile)
		{
			std::string str;
			format_signed(str);
			fprintf(pFile, "%s", str.c_str());
		}

		uint128 get_reversed_bits() const
		{
			uint128 res;

			const uint32_t* pSrc = (const uint32_t*)this;
			uint32_t* pDst = (uint32_t*)&res;

			pDst[0] = rev_dword(pSrc[3]);
			pDst[1] = rev_dword(pSrc[2]);
			pDst[2] = rev_dword(pSrc[1]);
			pDst[3] = rev_dword(pSrc[0]);

			return res;
		}

		uint128 get_byteswapped() const
		{
			uint128 res;

			const uint8_t* pSrc = (const uint8_t*)this;
			uint8_t* pDst = (uint8_t*)&res;

			for (uint32_t i = 0; i < 16; i++)
				pDst[i] = pSrc[15 - i];

			return res;
		}

		inline uint64_t get_bits64(uint32_t bit_ofs, uint32_t bit_len) const
		{
			assert(bit_ofs < 128);
			assert(bit_len && (bit_len <= 64) && ((bit_ofs + bit_len) <= 128));

			uint128 res(*this);
			res >>= bit_ofs;

			const uint64_t bitmask = (bit_len == 64) ? UINT64_MAX : ((1ull << bit_len) - 1);
			return res.get_low() & bitmask;
		}

		inline uint32_t get_bits(uint32_t bit_ofs, uint32_t bit_len) const
		{
			assert(bit_len <= 32);
			return (uint32_t)get_bits64(bit_ofs, bit_len);
		}

		inline uint32_t next_bits(uint32_t& bit_ofs, uint32_t len) const
		{
			assert(len && (len <= 32));
			uint32_t x = get_bits(bit_ofs, len);
			bit_ofs += len;
			return x;
		}

		inline uint128& set_bits(uint64_t val, uint32_t bit_ofs, uint32_t num_bits)
		{
			assert(bit_ofs < 128);
			assert(num_bits && (num_bits <= 64) && ((bit_ofs + num_bits) <= 128));

			uint128 bitmask(1);
			bitmask = (bitmask << num_bits) - 1;
			assert(uint128(val) <= bitmask);

			bitmask <<= bit_ofs;
			*this &= ~bitmask;

			*this = *this | (uint128(val) << bit_ofs);
			return *this;
		}
	};
		
	static bool decode_void_extent(const uint128& bits, log_astc_block& log_blk)
	{
		if (bits.get_bits(10, 2) != 0b11)
			return false;

		uint32_t bit_ofs = 12;
		const uint32_t min_s = bits.next_bits(bit_ofs, 13);
		const uint32_t max_s = bits.next_bits(bit_ofs, 13);
		const uint32_t min_t = bits.next_bits(bit_ofs, 13);
		const uint32_t max_t = bits.next_bits(bit_ofs, 13);
		assert(bit_ofs == 64);
		
		const bool all_extents_all_ones = (min_s == 0x1FFF) && (max_s == 0x1FFF) && (min_t == 0x1FFF) && (max_t == 0x1FFF);
		
		if (!all_extents_all_ones && ((min_s >= max_s) || (min_t >= max_t)))
			return false;

		const bool hdr_flag = bits.get_bits(9, 1) != 0;

		if (hdr_flag)
			log_blk.m_solid_color_flag_hdr = true;
		else
			log_blk.m_solid_color_flag_ldr = true;

		log_blk.m_solid_color[0] = (uint16_t)bits.get_bits(64, 16);
		log_blk.m_solid_color[1] = (uint16_t)bits.get_bits(80, 16);
		log_blk.m_solid_color[2] = (uint16_t)bits.get_bits(96, 16);
		log_blk.m_solid_color[3] = (uint16_t)bits.get_bits(112, 16);

		if (log_blk.m_solid_color_flag_hdr)
		{
			for (uint32_t c = 0; c < 4; c++)
				if (is_half_inf_or_nan(log_blk.m_solid_color[c]))
					return false;
		}
		
		return true;
	}

	struct astc_dec_row
	{
		int8_t Dp_ofs, P_ofs, W_ofs, W_size, H_ofs, H_size, W_bias, H_bias, p0_ofs, p1_ofs, p2_ofs;
	};

	static const astc_dec_row s_dec_rows[10] =
	{
		// Dp_ofs, P_ofs, W_ofs, W_size, H_ofs, H_size, W_bias, H_bias, p0_ofs, p1_ofs, p2_ofs;
		{  10,     9,     7,     2,      5,     2,      4,      2,      4,      0,      1      }, // 4 2
		{  10,     9,     7,     2,      5,     2,      8,      2,      4,      0,      1      }, // 8 2 
		{  10,     9,     5,     2,      7,     2,      2,      8,      4,      0,      1      }, // 2 8
		{  10,     9,     5,     2,      7,     1,      2,      6,      4,      0,      1      }, // 2 6

		{  10,     9,     7,     1,      5,     2,      2,      2,      4,      0,      1      }, // 2 2
		{  10,     9,     0,     0,      5,     2,      12,     2,      4,      2,      3      }, // 12 2
		{  10,     9,     5,     2,      0,     0,      2,     12,      4,      2,      3      }, // 2 12
		{  10,     9,     0,     0,      0,     0,      6,     10,      4,      2,      3      }, // 6 10

		{  10,     9,     0,     0,      0,     0,      10,    6,       4,      2,      3      }, // 10 6
		{  -1,    -1,     5,     2,      9,     2,      6,     6,       4,      2,      3      }, // 6 6
	};

	static bool decode_config(const uint128& bits, log_astc_block& log_blk)
	{
		// Reserved
		if (bits.get_bits(0, 4) == 0)
			return false;

		// Reserved
		if ((bits.get_bits(0, 2) == 0) && (bits.get_bits(6, 3) == 0b111))
		{
			if (bits.get_bits(2, 4) != 0b1111) 
				return false;
		}

		// Void extent
		if (bits.get_bits(0, 9) == 0b111111100)
			return decode_void_extent(bits, log_blk);
												
		// Check rows
		const uint32_t x0_2 = bits.get_bits(0, 2), x2_2 = bits.get_bits(2, 2);
		const uint32_t x5_4 = bits.get_bits(5, 4), x8_1 = bits.get_bits(8, 1);
		const uint32_t x7_2 = bits.get_bits(7, 2);

		int row_index = -1;
		if (x0_2 == 0)
		{
			if (x7_2 == 0b00)
				row_index = 5;
			else if (x7_2 == 0b01)
				row_index = 6;
			else if (x5_4 == 0b1100)
				row_index = 7;
			else if (x5_4 == 0b1101)
				row_index = 8;
			else if (x7_2 == 0b10)
				row_index = 9;
		}
		else
		{
			if (x2_2 == 0b00)
				row_index = 0;
			else if (x2_2 == 0b01)
				row_index = 1;
			else if (x2_2 == 0b10)
				row_index = 2;
			else if ((x2_2 == 0b11) && (x8_1 == 0))
				row_index = 3;
			else if ((x2_2 == 0b11) && (x8_1 == 1))
				row_index = 4;
		}
		if (row_index < 0)
			return false;

		const astc_dec_row& r = s_dec_rows[row_index];

		bool P = false, Dp = false;
		uint32_t W = r.W_bias, H = r.H_bias;

		if (r.P_ofs >= 0)
			P = bits.get_bits(r.P_ofs, 1) != 0;

		if (r.Dp_ofs >= 0)
			Dp = bits.get_bits(r.Dp_ofs, 1) != 0;
				
		if (r.W_size)
			W += bits.get_bits(r.W_ofs, r.W_size);

		if (r.H_size)
			H += bits.get_bits(r.H_ofs, r.H_size);

		assert((W >= MIN_GRID_DIM) && (W <= MAX_BLOCK_DIM));
		assert((H >= MIN_GRID_DIM) && (H <= MAX_BLOCK_DIM));
		
		int p0 = bits.get_bits(r.p0_ofs, 1);
		int p1 = bits.get_bits(r.p1_ofs, 1);
		int p2 = bits.get_bits(r.p2_ofs, 1);

		uint32_t p = p0 | (p1 << 1) | (p2 << 2);
		if (p < 2)
			return false;
		
		log_blk.m_grid_width = W;
		log_blk.m_grid_height = H;
		
		log_blk.m_weight_ise_range = (p - 2) + (P * BISE_10_LEVELS);
		assert(log_blk.m_weight_ise_range <= LAST_VALID_WEIGHT_ISE_RANGE);

		log_blk.m_dual_plane = Dp;

		return true;
	}

	static inline uint32_t read_le_dword(const uint8_t* pBytes)
	{
		return (pBytes[0]) | (pBytes[1] << 8U) | (pBytes[2] << 16U) | (pBytes[3] << 24U);
	}

	// See 18.12.Integer Sequence Encoding - tables computed by executing the decoder functions with all possible 8/7-bit inputs.
	static const uint8_t s_trit_decode[256][5] =
	{
		{0,0,0,0,0},{1,0,0,0,0},{2,0,0,0,0},{0,0,2,0,0},{0,1,0,0,0},{1,1,0,0,0},{2,1,0,0,0},{1,0,2,0,0},
		{0,2,0,0,0},{1,2,0,0,0},{2,2,0,0,0},{2,0,2,0,0},{0,2,2,0,0},{1,2,2,0,0},{2,2,2,0,0},{2,0,2,0,0},
		{0,0,1,0,0},{1,0,1,0,0},{2,0,1,0,0},{0,1,2,0,0},{0,1,1,0,0},{1,1,1,0,0},{2,1,1,0,0},{1,1,2,0,0},
		{0,2,1,0,0},{1,2,1,0,0},{2,2,1,0,0},{2,1,2,0,0},{0,0,0,2,2},{1,0,0,2,2},{2,0,0,2,2},{0,0,2,2,2},
		{0,0,0,1,0},{1,0,0,1,0},{2,0,0,1,0},{0,0,2,1,0},{0,1,0,1,0},{1,1,0,1,0},{2,1,0,1,0},{1,0,2,1,0},
		{0,2,0,1,0},{1,2,0,1,0},{2,2,0,1,0},{2,0,2,1,0},{0,2,2,1,0},{1,2,2,1,0},{2,2,2,1,0},{2,0,2,1,0},
		{0,0,1,1,0},{1,0,1,1,0},{2,0,1,1,0},{0,1,2,1,0},{0,1,1,1,0},{1,1,1,1,0},{2,1,1,1,0},{1,1,2,1,0},
		{0,2,1,1,0},{1,2,1,1,0},{2,2,1,1,0},{2,1,2,1,0},{0,1,0,2,2},{1,1,0,2,2},{2,1,0,2,2},{1,0,2,2,2},
		{0,0,0,2,0},{1,0,0,2,0},{2,0,0,2,0},{0,0,2,2,0},{0,1,0,2,0},{1,1,0,2,0},{2,1,0,2,0},{1,0,2,2,0},
		{0,2,0,2,0},{1,2,0,2,0},{2,2,0,2,0},{2,0,2,2,0},{0,2,2,2,0},{1,2,2,2,0},{2,2,2,2,0},{2,0,2,2,0},
		{0,0,1,2,0},{1,0,1,2,0},{2,0,1,2,0},{0,1,2,2,0},{0,1,1,2,0},{1,1,1,2,0},{2,1,1,2,0},{1,1,2,2,0},
		{0,2,1,2,0},{1,2,1,2,0},{2,2,1,2,0},{2,1,2,2,0},{0,2,0,2,2},{1,2,0,2,2},{2,2,0,2,2},{2,0,2,2,2},
		{0,0,0,0,2},{1,0,0,0,2},{2,0,0,0,2},{0,0,2,0,2},{0,1,0,0,2},{1,1,0,0,2},{2,1,0,0,2},{1,0,2,0,2},
		{0,2,0,0,2},{1,2,0,0,2},{2,2,0,0,2},{2,0,2,0,2},{0,2,2,0,2},{1,2,2,0,2},{2,2,2,0,2},{2,0,2,0,2},
		{0,0,1,0,2},{1,0,1,0,2},{2,0,1,0,2},{0,1,2,0,2},{0,1,1,0,2},{1,1,1,0,2},{2,1,1,0,2},{1,1,2,0,2},
		{0,2,1,0,2},{1,2,1,0,2},{2,2,1,0,2},{2,1,2,0,2},{0,2,2,2,2},{1,2,2,2,2},{2,2,2,2,2},{2,0,2,2,2},
		{0,0,0,0,1},{1,0,0,0,1},{2,0,0,0,1},{0,0,2,0,1},{0,1,0,0,1},{1,1,0,0,1},{2,1,0,0,1},{1,0,2,0,1},
		{0,2,0,0,1},{1,2,0,0,1},{2,2,0,0,1},{2,0,2,0,1},{0,2,2,0,1},{1,2,2,0,1},{2,2,2,0,1},{2,0,2,0,1},
		{0,0,1,0,1},{1,0,1,0,1},{2,0,1,0,1},{0,1,2,0,1},{0,1,1,0,1},{1,1,1,0,1},{2,1,1,0,1},{1,1,2,0,1},
		{0,2,1,0,1},{1,2,1,0,1},{2,2,1,0,1},{2,1,2,0,1},{0,0,1,2,2},{1,0,1,2,2},{2,0,1,2,2},{0,1,2,2,2},
		{0,0,0,1,1},{1,0,0,1,1},{2,0,0,1,1},{0,0,2,1,1},{0,1,0,1,1},{1,1,0,1,1},{2,1,0,1,1},{1,0,2,1,1},
		{0,2,0,1,1},{1,2,0,1,1},{2,2,0,1,1},{2,0,2,1,1},{0,2,2,1,1},{1,2,2,1,1},{2,2,2,1,1},{2,0,2,1,1},
		{0,0,1,1,1},{1,0,1,1,1},{2,0,1,1,1},{0,1,2,1,1},{0,1,1,1,1},{1,1,1,1,1},{2,1,1,1,1},{1,1,2,1,1},
		{0,2,1,1,1},{1,2,1,1,1},{2,2,1,1,1},{2,1,2,1,1},{0,1,1,2,2},{1,1,1,2,2},{2,1,1,2,2},{1,1,2,2,2},
		{0,0,0,2,1},{1,0,0,2,1},{2,0,0,2,1},{0,0,2,2,1},{0,1,0,2,1},{1,1,0,2,1},{2,1,0,2,1},{1,0,2,2,1},
		{0,2,0,2,1},{1,2,0,2,1},{2,2,0,2,1},{2,0,2,2,1},{0,2,2,2,1},{1,2,2,2,1},{2,2,2,2,1},{2,0,2,2,1},
		{0,0,1,2,1},{1,0,1,2,1},{2,0,1,2,1},{0,1,2,2,1},{0,1,1,2,1},{1,1,1,2,1},{2,1,1,2,1},{1,1,2,2,1},
		{0,2,1,2,1},{1,2,1,2,1},{2,2,1,2,1},{2,1,2,2,1},{0,2,1,2,2},{1,2,1,2,2},{2,2,1,2,2},{2,1,2,2,2},
		{0,0,0,1,2},{1,0,0,1,2},{2,0,0,1,2},{0,0,2,1,2},{0,1,0,1,2},{1,1,0,1,2},{2,1,0,1,2},{1,0,2,1,2},
		{0,2,0,1,2},{1,2,0,1,2},{2,2,0,1,2},{2,0,2,1,2},{0,2,2,1,2},{1,2,2,1,2},{2,2,2,1,2},{2,0,2,1,2},
		{0,0,1,1,2},{1,0,1,1,2},{2,0,1,1,2},{0,1,2,1,2},{0,1,1,1,2},{1,1,1,1,2},{2,1,1,1,2},{1,1,2,1,2},
		{0,2,1,1,2},{1,2,1,1,2},{2,2,1,1,2},{2,1,2,1,2},{0,2,2,2,2},{1,2,2,2,2},{2,2,2,2,2},{2,1,2,2,2}
	};

	static const uint8_t s_quint_decode[128][3] =
	{
		{0,0,0},{1,0,0},{2,0,0},{3,0,0},{4,0,0},{0,4,0},{4,4,0},{4,4,4},
		{0,1,0},{1,1,0},{2,1,0},{3,1,0},{4,1,0},{1,4,0},{4,4,1},{4,4,4},
		{0,2,0},{1,2,0},{2,2,0},{3,2,0},{4,2,0},{2,4,0},{4,4,2},{4,4,4},
		{0,3,0},{1,3,0},{2,3,0},{3,3,0},{4,3,0},{3,4,0},{4,4,3},{4,4,4},
		{0,0,1},{1,0,1},{2,0,1},{3,0,1},{4,0,1},{0,4,1},{4,0,4},{0,4,4},
		{0,1,1},{1,1,1},{2,1,1},{3,1,1},{4,1,1},{1,4,1},{4,1,4},{1,4,4},
		{0,2,1},{1,2,1},{2,2,1},{3,2,1},{4,2,1},{2,4,1},{4,2,4},{2,4,4},
		{0,3,1},{1,3,1},{2,3,1},{3,3,1},{4,3,1},{3,4,1},{4,3,4},{3,4,4},
		{0,0,2},{1,0,2},{2,0,2},{3,0,2},{4,0,2},{0,4,2},{2,0,4},{3,0,4},
		{0,1,2},{1,1,2},{2,1,2},{3,1,2},{4,1,2},{1,4,2},{2,1,4},{3,1,4},
		{0,2,2},{1,2,2},{2,2,2},{3,2,2},{4,2,2},{2,4,2},{2,2,4},{3,2,4},
		{0,3,2},{1,3,2},{2,3,2},{3,3,2},{4,3,2},{3,4,2},{2,3,4},{3,3,4},
		{0,0,3},{1,0,3},{2,0,3},{3,0,3},{4,0,3},{0,4,3},{0,0,4},{1,0,4},
		{0,1,3},{1,1,3},{2,1,3},{3,1,3},{4,1,3},{1,4,3},{0,1,4},{1,1,4},
		{0,2,3},{1,2,3},{2,2,3},{3,2,3},{4,2,3},{2,4,3},{0,2,4},{1,2,4},
		{0,3,3},{1,3,3},{2,3,3},{3,3,3},{4,3,3},{3,4,3},{0,3,4},{1,3,4}
	};

	static void decode_trit_block(uint8_t* pVals, uint32_t num_vals, const uint128& bits, uint32_t& bit_ofs, uint32_t bits_per_val)
	{
		assert((num_vals >= 1) && (num_vals <= 5));
		uint32_t m[5] = { 0 }, T = 0;

		static const uint8_t s_t_bits[5] = { 2, 2, 1, 2, 1 };

		for (uint32_t T_ofs = 0, c = 0; c < num_vals; c++)
		{
			if (bits_per_val)
				m[c] = bits.next_bits(bit_ofs, bits_per_val);
			T |= (bits.next_bits(bit_ofs, s_t_bits[c]) << T_ofs);
			T_ofs += s_t_bits[c];
		}

		const uint8_t (&p_trits)[5] = s_trit_decode[T];

		for (uint32_t i = 0; i < num_vals; i++)
			pVals[i] = (uint8_t)((p_trits[i] << bits_per_val) | m[i]);
	}

	static void decode_quint_block(uint8_t* pVals, uint32_t num_vals, const uint128& bits, uint32_t& bit_ofs, uint32_t bits_per_val)
	{
		assert((num_vals >= 1) && (num_vals <= 3));
		uint32_t m[3] = { 0 }, T = 0;

		static const uint8_t s_t_bits[3] = { 3, 2, 2 };

		for (uint32_t T_ofs = 0, c = 0; c < num_vals; c++)
		{
			if (bits_per_val)
				m[c] = bits.next_bits(bit_ofs, bits_per_val);
			T |= (bits.next_bits(bit_ofs, s_t_bits[c]) << T_ofs);
			T_ofs += s_t_bits[c];
		}

		const uint8_t (&p_quints)[3] = s_quint_decode[T];

		for (uint32_t i = 0; i < num_vals; i++)
			pVals[i] = (uint8_t)((p_quints[i] << bits_per_val) | m[i]);
	}

	static void decode_bise(uint32_t ise_range, uint8_t* pVals, uint32_t num_vals, const uint128& bits, uint32_t bit_ofs)
	{
		assert(num_vals && (ise_range < TOTAL_ISE_RANGES));
		
		const uint32_t bits_per_val = g_ise_range_table[ise_range][0];

		if (g_ise_range_table[ise_range][1])
		{
			// Trits+bits, 5 vals per block, 7 bits extra per block
			const uint32_t total_blocks = (num_vals + 4) / 5;
			for (uint32_t b = 0; b < total_blocks; b++)
			{
				const uint32_t num_vals_in_block = std::min<int>(num_vals - 5 * b, 5);
				decode_trit_block(pVals + 5 * b, num_vals_in_block, bits, bit_ofs, bits_per_val);
			}
		}
		else if (g_ise_range_table[ise_range][2])
		{
			// Quints+bits, 3 vals per block, 8 bits extra per block
			const uint32_t total_blocks = (num_vals + 2) / 3;
			for (uint32_t b = 0; b < total_blocks; b++)
			{
				const uint32_t num_vals_in_block = std::min<int>(num_vals - 3 * b, 3);
				decode_quint_block(pVals + 3 * b, num_vals_in_block, bits, bit_ofs, bits_per_val);
			}
		}
		else
		{
			assert(bits_per_val);

			// Only bits
			for (uint32_t i = 0; i < num_vals; i++)
				pVals[i] = (uint8_t)bits.next_bits(bit_ofs, bits_per_val);
		}
	}

	void decode_bise(uint32_t ise_range, uint8_t* pVals, uint32_t num_vals, const uint8_t* pBits128, uint32_t bit_ofs)
	{
		const uint128 bits(
			(uint64_t)read_le_dword(pBits128) | (((uint64_t)read_le_dword(pBits128 + sizeof(uint32_t))) << 32),
			(uint64_t)read_le_dword(pBits128 + sizeof(uint32_t) * 2) | (((uint64_t)read_le_dword(pBits128 + sizeof(uint32_t) * 3)) << 32));

		return decode_bise(ise_range, pVals, num_vals, bits, bit_ofs);
	}
		
	// Decodes a physical ASTC block to a logical ASTC block.
	// blk_width/blk_height are only used to validate the weight grid's dimensions.
	bool unpack_block(const void* pASTC_block, log_astc_block& log_blk, uint32_t blk_width, uint32_t blk_height)
	{
		assert(is_valid_block_size(blk_width, blk_height));
				
		const uint8_t* pS = (uint8_t*)pASTC_block;

		log_blk.clear();
		log_blk.m_error_flag = true;
		
		const uint128 bits(
			(uint64_t)read_le_dword(pS) | (((uint64_t)read_le_dword(pS + sizeof(uint32_t))) << 32),
			(uint64_t)read_le_dword(pS + sizeof(uint32_t) * 2) | (((uint64_t)read_le_dword(pS + sizeof(uint32_t) * 3)) << 32));
		
		const uint128 rev_bits(bits.get_reversed_bits());
				
		if (!decode_config(bits, log_blk))
			return false;

		if (log_blk.m_solid_color_flag_hdr || log_blk.m_solid_color_flag_ldr)
		{
			// Void extent
			log_blk.m_error_flag = false;
			return true;
		}

		// Check grid dimensions
		if ((log_blk.m_grid_width > blk_width) || (log_blk.m_grid_height > blk_height))
			return false;
		
		// Now we have the grid width/height, dual plane, weight ISE range
		
		const uint32_t total_grid_weights = (log_blk.m_dual_plane ? 2 : 1) * (log_blk.m_grid_width * log_blk.m_grid_height);
		const uint32_t total_weight_bits = get_ise_sequence_bits(total_grid_weights, log_blk.m_weight_ise_range);
				
		// 18.24 Illegal Encodings
		if ((!total_grid_weights) || (total_grid_weights > MAX_GRID_WEIGHTS) || (total_weight_bits < 24) || (total_weight_bits > 96))
			return false;
		
		const uint32_t end_of_weight_bit_ofs = 128 - total_weight_bits;

		uint32_t total_extra_bits = 0;

		// Right before the weight bits, there may be extra CEM bits, then the 2 CCS bits if dual plane.

		log_blk.m_num_partitions = bits.get_bits(11, 2) + 1;
		if (log_blk.m_num_partitions == 1)
			log_blk.m_color_endpoint_modes[0] = bits.get_bits(13, 4); // read CEM bits
		else
		{
			// 2 or more partitions
			if (log_blk.m_dual_plane && (log_blk.m_num_partitions == 4))
				return false;

			log_blk.m_partition_id = bits.get_bits(13, 10);

			uint32_t cem_bits = bits.get_bits(23, 6);

			if ((cem_bits & 3) == 0)
			{
				// All CEM's the same
				for (uint32_t i = 0; i < log_blk.m_num_partitions; i++)
					log_blk.m_color_endpoint_modes[i] = cem_bits >> 2;
			}
			else
			{
				// CEM's different, but within up to 2 adjacent classes
				const uint32_t first_cem_index = ((cem_bits & 3) - 1) * 4;

				total_extra_bits = 3 * log_blk.m_num_partitions - 4;

				if ((total_weight_bits + total_extra_bits) > 128)
					return false;

				uint32_t cem_bit_pos = end_of_weight_bit_ofs - total_extra_bits;
				
				uint32_t c[4] = { 0 }, m[4] = { 0 };
				
				cem_bits >>= 2;
				for (uint32_t i = 0; i < log_blk.m_num_partitions; i++, cem_bits >>= 1)
					c[i] = cem_bits & 1;

				switch (log_blk.m_num_partitions)
				{
				case 2:
				{
					m[0] = cem_bits & 3;
					m[1] = bits.next_bits(cem_bit_pos, 2);
					break;
				}
				case 3:
				{
					m[0] = cem_bits & 1;
					m[0] |= (bits.next_bits(cem_bit_pos, 1) << 1);
					m[1] = bits.next_bits(cem_bit_pos, 2);
					m[2] = bits.next_bits(cem_bit_pos, 2);
					break;
				}
				case 4:
				{
					for (uint32_t i = 0; i < 4; i++)
						m[i] = bits.next_bits(cem_bit_pos, 2);
					break;
				}
				default:
				{
					assert(0);
					break;
				}
				}

				assert(cem_bit_pos == end_of_weight_bit_ofs);

				for (uint32_t i = 0; i < log_blk.m_num_partitions; i++)
				{
					log_blk.m_color_endpoint_modes[i] = first_cem_index + (c[i] * 4) + m[i];
					assert(log_blk.m_color_endpoint_modes[i] <= 15);
				}
			}
		}

		// Now we have all the CEM indices.

		if (log_blk.m_dual_plane)
		{
			// Read CCS bits, beneath any CEM bits
			total_extra_bits += 2;

			if (total_extra_bits > end_of_weight_bit_ofs)
				return false;

			uint32_t ccs_bit_pos = end_of_weight_bit_ofs - total_extra_bits;
			log_blk.m_color_component_selector = bits.get_bits(ccs_bit_pos, 2);
		}

		uint32_t config_bit_pos = 11 + 2; // config+num_parts
		if (log_blk.m_num_partitions == 1)
			config_bit_pos += 4; // CEM bits
		else
			config_bit_pos += 10 + 6; // part_id+CEM bits

		// config+num_parts+total_extra_bits (CEM extra+CCS)
		uint32_t total_config_bits = config_bit_pos + total_extra_bits;
		
		// Compute number of remaining bits in block
		const int num_remaining_bits = 128 - (int)total_config_bits - (int)total_weight_bits;
		if (num_remaining_bits < 0)
			return false;

		// Compute total number of ISE encoded color endpoint mode values
		uint32_t total_cem_vals = 0;
		for (uint32_t j = 0; j < log_blk.m_num_partitions; j++)
			total_cem_vals += get_num_cem_values(log_blk.m_color_endpoint_modes[j]);

		if (total_cem_vals > MAX_ENDPOINTS)
			return false;

		// Infer endpoint ISE range based off the # of values we need to encode, and the # of remaining bits in the block
		int endpoint_ise_range = -1;
		for (int k = 20; k > 0; k--)
		{
			int b = get_ise_sequence_bits(total_cem_vals, k);
			if (b <= num_remaining_bits)
			{
				endpoint_ise_range = k;
				break;
			}
		}

		// See 23.24 Illegal Encodings, [0,5] is the minimum ISE encoding for endpoints
		if (endpoint_ise_range < (int)FIRST_VALID_ENDPOINT_ISE_RANGE)
			return false;

		log_blk.m_endpoint_ise_range = endpoint_ise_range;

		// Decode endpoints forwards in block
		decode_bise(log_blk.m_endpoint_ise_range, log_blk.m_endpoints, total_cem_vals, bits, config_bit_pos);

		// Decode grid weights backwards in block
		decode_bise(log_blk.m_weight_ise_range, log_blk.m_weights, total_grid_weights, rev_bits, 0);

		log_blk.m_error_flag = false;

		return true;
	}
		
} // namespace astc_helpers

#endif //BASISU_ASTC_HELPERS_IMPLEMENTATION
