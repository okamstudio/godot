// basisu_transcoder_uastc.h
#pragma once
#include "basisu_transcoder_internal.h"

namespace basist
{
	struct color_quad_u8
	{ 
		uint8_t m_c[4]; 
	};

	const uint32_t TOTAL_UASTC_MODES = 19;
	const uint32_t UASTC_MODE_INDEX_SOLID_COLOR = 8;

	const uint32_t TOTAL_ASTC_BC7_COMMON_PARTITIONS2 = 30;
	const uint32_t TOTAL_ASTC_BC6H_COMMON_PARTITIONS2 = 27; // BC6H only supports only 5-bit pattern indices, BC7 supports 4-bit or 6-bit
	const uint32_t TOTAL_ASTC_BC7_COMMON_PARTITIONS3 = 11;
	const uint32_t TOTAL_BC7_3_ASTC2_COMMON_PARTITIONS = 19;

	extern const uint8_t g_uastc_mode_weight_bits[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_weight_ranges[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_endpoint_ranges[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_subsets[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_planes[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_comps[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_has_etc1_bias[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_has_bc1_hint0[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_has_bc1_hint1[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_has_alpha[TOTAL_UASTC_MODES];
	extern const uint8_t g_uastc_mode_is_la[TOTAL_UASTC_MODES];

	struct astc_bc7_common_partition2_desc
	{
		uint8_t m_bc7;
		uint16_t m_astc;
		bool m_invert;
	};

	extern const astc_bc7_common_partition2_desc g_astc_bc7_common_partitions2[TOTAL_ASTC_BC7_COMMON_PARTITIONS2];

	struct bc73_astc2_common_partition_desc
	{
		uint8_t m_bc73;
		uint16_t m_astc2;
		uint8_t k;		// 0-5 - how to modify the BC7 3-subset pattern to match the ASTC pattern (LSB=invert)
	};

	extern const bc73_astc2_common_partition_desc g_bc7_3_astc2_common_partitions[TOTAL_BC7_3_ASTC2_COMMON_PARTITIONS];

	struct astc_bc7_common_partition3_desc
	{
		uint8_t m_bc7;
		uint16_t m_astc;
		uint8_t m_astc_to_bc7_perm; // converts ASTC to BC7 partition using g_astc_bc7_partition_index_perm_tables[][]
	};

	extern const astc_bc7_common_partition3_desc g_astc_bc7_common_partitions3[TOTAL_ASTC_BC7_COMMON_PARTITIONS3];

	extern const uint8_t g_astc_bc7_patterns2[TOTAL_ASTC_BC7_COMMON_PARTITIONS2][16];
	extern const uint8_t g_astc_bc7_patterns3[TOTAL_ASTC_BC7_COMMON_PARTITIONS3][16];
	extern const uint8_t g_bc7_3_astc2_patterns2[TOTAL_BC7_3_ASTC2_COMMON_PARTITIONS][16];

	extern const uint8_t g_astc_bc7_pattern2_anchors[TOTAL_ASTC_BC7_COMMON_PARTITIONS2][3];
	extern const uint8_t g_astc_bc7_pattern3_anchors[TOTAL_ASTC_BC7_COMMON_PARTITIONS3][3];
	extern const uint8_t g_bc7_3_astc2_patterns2_anchors[TOTAL_BC7_3_ASTC2_COMMON_PARTITIONS][3];

	extern const uint32_t g_uastc_mode_huff_codes[TOTAL_UASTC_MODES + 1][2];

	extern const uint8_t g_astc_to_bc7_partition_index_perm_tables[6][3];
	extern const uint8_t g_bc7_to_astc_partition_index_perm_tables[6][3]; // inverse of g_astc_to_bc7_partition_index_perm_tables

	extern const uint8_t* s_uastc_to_bc1_weights[6];

	uint32_t bc7_convert_partition_index_3_to_2(uint32_t p, uint32_t k);

	inline uint32_t astc_interpolate(uint32_t l, uint32_t h, uint32_t w, bool srgb)
	{
		if (srgb)
		{
			l = (l << 8) | 0x80;
			h = (h << 8) | 0x80;
		}
		else
		{
			l = (l << 8) | l;
			h = (h << 8) | h;
		}

		uint32_t k = (l * (64 - w) + h * w + 32) >> 6;

		return k >> 8;
	}

	struct astc_block_desc
	{
		int m_weight_range;	// weight BISE range

		int m_subsets;			// number of ASTC partitions
		int m_partition_seed;	// partition pattern seed
		int m_cem;				// color endpoint mode used by all subsets

		int m_ccs;				// color component selector (dual plane only)
		bool m_dual_plane;	// true if dual plane

		// Weight and endpoint BISE values. 
		// Note these values are NOT linear, they must be BISE encoded. See Table 97 and Table 107.
		uint8_t m_endpoints[18];	// endpoint values, in RR GG BB etc. order 
		uint8_t m_weights[64];		// weight index values, raster order, in P0 P1, P0 P1, etc. or P0, P0, P0, P0, etc. order
	};

	const uint32_t BC7ENC_TOTAL_ASTC_RANGES = 21;

	// See tables 81, 93, 18.13.Endpoint Unquantization
	const uint32_t TOTAL_ASTC_RANGES = 21;
	extern const int g_astc_bise_range_table[TOTAL_ASTC_RANGES][3];

	struct astc_quant_bin
	{
		uint8_t m_unquant; // unquantized value
		uint8_t m_index; // sorted index
	};

	extern astc_quant_bin g_astc_unquant[BC7ENC_TOTAL_ASTC_RANGES][256]; // [ASTC encoded endpoint index]

	int astc_get_levels(int range);
	bool astc_is_valid_endpoint_range(uint32_t range);
	uint32_t unquant_astc_endpoint(uint32_t packed_bits, uint32_t packed_trits, uint32_t packed_quints, uint32_t range);
	uint32_t unquant_astc_endpoint_val(uint32_t packed_val, uint32_t range);

	const uint8_t* get_anchor_indices(uint32_t subsets, uint32_t mode, uint32_t common_pattern, const uint8_t*& pPartition_pattern);

	// BC7
	const uint32_t BC7ENC_BLOCK_SIZE = 16;

	struct bc7_block
	{
		uint64_t m_qwords[2];
	};

	struct bc7_optimization_results
	{
		uint32_t m_mode;
		uint32_t m_partition;
		uint8_t m_selectors[16];
		uint8_t m_alpha_selectors[16];
		color_quad_u8 m_low[3];
		color_quad_u8 m_high[3];
		uint32_t m_pbits[3][2];
		uint32_t m_index_selector;
		uint32_t m_rotation;
	};

	extern const uint32_t g_bc7_weights1[2];
	extern const uint32_t g_bc7_weights2[4];
	extern const uint32_t g_bc7_weights3[8];
	extern const uint32_t g_bc7_weights4[16];
	extern const uint32_t g_astc_weights4[16];
	extern const uint32_t g_astc_weights5[32];
	extern const uint32_t g_astc_weights_3levels[3];
	extern const uint8_t g_bc7_partition1[16];
	extern const uint8_t g_bc7_partition2[64 * 16];
	extern const uint8_t g_bc7_partition3[64 * 16];
	extern const uint8_t g_bc7_table_anchor_index_second_subset[64];
	extern const uint8_t g_bc7_table_anchor_index_third_subset_1[64];
	extern const uint8_t g_bc7_table_anchor_index_third_subset_2[64];
	extern const uint8_t g_bc7_num_subsets[8];
	extern const uint8_t g_bc7_partition_bits[8];
	extern const uint8_t g_bc7_color_index_bitcount[8];
	extern const uint8_t g_bc7_mode_has_p_bits[8];
	extern const uint8_t g_bc7_mode_has_shared_p_bits[8];
	extern const uint8_t g_bc7_color_precision_table[8];
	extern const int8_t g_bc7_alpha_precision_table[8];
	extern const uint8_t g_bc7_alpha_index_bitcount[8];

	inline bool get_bc7_mode_has_seperate_alpha_selectors(int mode) { return (mode == 4) || (mode == 5); }
	inline int get_bc7_color_index_size(int mode, int index_selection_bit) { return g_bc7_color_index_bitcount[mode] + index_selection_bit; }
	inline int get_bc7_alpha_index_size(int mode, int index_selection_bit) { return g_bc7_alpha_index_bitcount[mode] - index_selection_bit; }

	struct endpoint_err
	{
		uint16_t m_error; uint8_t m_lo; uint8_t m_hi;
	};

	extern endpoint_err g_bc7_mode_6_optimal_endpoints[256][2]; // [c][pbit]
	const uint32_t BC7ENC_MODE_6_OPTIMAL_INDEX = 5;

	extern endpoint_err g_bc7_mode_5_optimal_endpoints[256]; // [c]
	const uint32_t BC7ENC_MODE_5_OPTIMAL_INDEX = 1;

	// Packs a BC7 block from a high-level description. Handles all BC7 modes.
	void encode_bc7_block(void* pBlock, const bc7_optimization_results* pResults);

	// Packs an ASTC block
	// Constraints: Always 4x4, all subset CEM's must be equal, only tested with LDR CEM's.
	bool pack_astc_block(uint32_t* pDst, const astc_block_desc* pBlock, uint32_t mode);

	void pack_astc_solid_block(void* pDst_block, const color32& color);

#ifdef _DEBUG
	int astc_compute_texel_partition(int seed, int x, int y, int z, int partitioncount, bool small_block);
#endif
		
	struct uastc_block
	{
		union
		{
			uint8_t m_bytes[16];
			uint32_t m_dwords[4];
		};
	};

	struct unpacked_uastc_block
	{
		astc_block_desc m_astc;

		uint32_t m_mode;
		uint32_t m_common_pattern;

		color32 m_solid_color;

		bool m_bc1_hint0;
		bool m_bc1_hint1;

		bool m_etc1_flip;
		bool m_etc1_diff;
		uint32_t m_etc1_inten0;
		uint32_t m_etc1_inten1;

		uint32_t m_etc1_bias;

		uint32_t m_etc2_hints;

		uint32_t m_etc1_selector;
		uint32_t m_etc1_r, m_etc1_g, m_etc1_b;
	};

	color32 apply_etc1_bias(const color32 &block_color, uint32_t bias, uint32_t limit, uint32_t subblock);
	
	struct decoder_etc_block;
	struct eac_block;
		
	bool unpack_uastc(uint32_t mode, uint32_t common_pattern, const color32& solid_color, const astc_block_desc& astc, color32* pPixels, bool srgb);
	bool unpack_uastc(const unpacked_uastc_block& unpacked_blk, color32* pPixels, bool srgb);

	bool unpack_uastc(const uastc_block& blk, color32* pPixels, bool srgb);
	bool unpack_uastc(const uastc_block& blk, unpacked_uastc_block& unpacked, bool undo_blue_contract, bool read_hints = true);

	bool transcode_uastc_to_astc(const uastc_block& src_blk, void* pDst);

	bool transcode_uastc_to_bc7(const unpacked_uastc_block& unpacked_src_blk, bc7_optimization_results& dst_blk);
	bool transcode_uastc_to_bc7(const uastc_block& src_blk, bc7_optimization_results& dst_blk);
	bool transcode_uastc_to_bc7(const uastc_block& src_blk, void* pDst);

	void transcode_uastc_to_etc1(unpacked_uastc_block& unpacked_src_blk, color32 block_pixels[4][4], void* pDst);
	bool transcode_uastc_to_etc1(const uastc_block& src_blk, void* pDst);
	bool transcode_uastc_to_etc1(const uastc_block& src_blk, void* pDst, uint32_t channel);

	void transcode_uastc_to_etc2_eac_a8(unpacked_uastc_block& unpacked_src_blk, color32 block_pixels[4][4], void* pDst);
	bool transcode_uastc_to_etc2_rgba(const uastc_block& src_blk, void* pDst);

	// Packs 16 scalar values to BC4. Same PSNR as stb_dxt's BC4 encoder, around 13% faster.
	void encode_bc4(void* pDst, const uint8_t* pPixels, uint32_t stride);
	
	void encode_bc1_solid_block(void* pDst, uint32_t fr, uint32_t fg, uint32_t fb);

	enum
	{
		cEncodeBC1HighQuality = 1,
		cEncodeBC1HigherQuality = 2,
		cEncodeBC1UseSelectors = 4,
	};
	void encode_bc1(void* pDst, const uint8_t* pPixels, uint32_t flags);
	
	// Alternate PCA-free encoder, around 15% faster, same (or slightly higher) avg. PSNR
	void encode_bc1_alt(void* pDst, const uint8_t* pPixels, uint32_t flags);

	void transcode_uastc_to_bc1_hint0(const unpacked_uastc_block& unpacked_src_blk, void* pDst);
	void transcode_uastc_to_bc1_hint1(const unpacked_uastc_block& unpacked_src_blk, const color32 block_pixels[4][4], void* pDst, bool high_quality);

	bool transcode_uastc_to_bc1(const uastc_block& src_blk, void* pDst, bool high_quality);
	bool transcode_uastc_to_bc3(const uastc_block& src_blk, void* pDst, bool high_quality);
	bool transcode_uastc_to_bc4(const uastc_block& src_blk, void* pDst, bool high_quality, uint32_t chan0);
	bool transcode_uastc_to_bc5(const uastc_block& src_blk, void* pDst, bool high_quality, uint32_t chan0, uint32_t chan1);

	bool transcode_uastc_to_etc2_eac_r11(const uastc_block& src_blk, void* pDst, bool high_quality, uint32_t chan0);
	bool transcode_uastc_to_etc2_eac_rg11(const uastc_block& src_blk, void* pDst, bool high_quality, uint32_t chan0, uint32_t chan1);

	bool transcode_uastc_to_pvrtc1_4_rgb(const uastc_block* pSrc_blocks, void* pDst_blocks, uint32_t num_blocks_x, uint32_t num_blocks_y, bool high_quality, bool from_alpha);
	bool transcode_uastc_to_pvrtc1_4_rgba(const uastc_block* pSrc_blocks, void* pDst_blocks, uint32_t num_blocks_x, uint32_t num_blocks_y, bool high_quality);
		
	// uastc_init() MUST be called before using this module.
	void uastc_init();

} // namespace basist
