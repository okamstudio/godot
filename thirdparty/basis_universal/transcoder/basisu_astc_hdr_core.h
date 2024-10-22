// File: basisu_astc_hdr_core.h
#pragma once
#include "basisu_astc_helpers.h"

namespace basist
{
	struct astc_blk
	{
		uint8_t m_vals[16];
	};

	// ASTC_HDR_MAX_VAL is the maximum color component value that can be encoded.
	// If the input has values higher than this, they need to be linearly scaled so all values are between [0,ASTC_HDR_MAX_VAL], and the linear scaling inverted in the shader.
	const float ASTC_HDR_MAX_VAL = 65216.0f; // actually MAX_QLOG12_VAL

	// Maximum usable QLOG encodings, and their floating point equivalent values, that don't result in NaN/Inf's.
	const uint32_t MAX_QLOG7 = 123;
	//const float MAX_QLOG7_VAL = 55296.0f;

	const uint32_t MAX_QLOG8 = 247;
	//const float MAX_QLOG8_VAL = 60416.0f;

	const uint32_t MAX_QLOG9 = 495;
	//const float MAX_QLOG9_VAL = 62976.0f;

	const uint32_t MAX_QLOG10 = 991;
	//const float MAX_QLOG10_VAL = 64256.0f;

	const uint32_t MAX_QLOG11 = 1983;
	//const float MAX_QLOG11_VAL = 64896.0f;

	const uint32_t MAX_QLOG12 = 3967;
	//const float MAX_QLOG12_VAL = 65216.0f;

	const uint32_t MAX_QLOG16 = 63487;
	const float MAX_QLOG16_VAL = 65504.0f;

	const uint32_t NUM_MODE11_ENDPOINTS = 6, NUM_MODE7_ENDPOINTS = 4;

	// Notes:
	// qlog16_to_half(half_to_qlog16(half_val_as_int)) == half_val_as_int (is lossless)
	// However, this is not lossless in the general sense.
	inline half_float qlog16_to_half_slow(uint32_t qlog16)
	{
		assert(qlog16 <= 0xFFFF);

		int C = qlog16;

		int E = (C & 0xF800) >> 11;
		int M = C & 0x7FF;

		int Mt;
		if (M < 512)
			Mt = 3 * M;
		else if (M >= 1536)
			Mt = 5 * M - 2048;
		else
			Mt = 4 * M - 512;

		int Cf = (E << 10) + (Mt >> 3);
		return (half_float)Cf;
	}

	// This is not lossless
	inline half_float qlog_to_half_slow(uint32_t qlog, uint32_t bits)
	{
		assert((bits >= 7U) && (bits <= 16U));
		assert(qlog < (1U << bits));

		int C = qlog << (16 - bits);
		return qlog16_to_half_slow(C);
	}

	void astc_hdr_core_init();

	void decode_mode7_to_qlog12_ise20(
		const uint8_t* pEndpoints,
		int e[2][3],
		int* pScale);

	bool decode_mode7_to_qlog12(
		const uint8_t* pEndpoints,
		int e[2][3],
		int* pScale,
		uint32_t ise_endpoint_range);

	void decode_mode11_to_qlog12_ise20(
		const uint8_t* pEndpoints,
		int e[2][3]);

	bool decode_mode11_to_qlog12(
		const uint8_t* pEndpoints,
		int e[2][3],
		uint32_t ise_endpoint_range);

	bool transcode_bc6h_1subset(half_float h_e[3][2], const astc_helpers::log_astc_block& best_blk, bc6h_block& transcoded_bc6h_blk);
	bool transcode_bc6h_2subsets(uint32_t common_part_index, const astc_helpers::log_astc_block& best_blk, bc6h_block& transcoded_bc6h_blk);

	bool astc_hdr_transcode_to_bc6h(const astc_blk& src_blk, bc6h_block& dst_blk);
	bool astc_hdr_transcode_to_bc6h(const astc_helpers::log_astc_block& log_blk, bc6h_block& dst_blk);

} // namespace basist
