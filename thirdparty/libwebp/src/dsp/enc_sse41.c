// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE4 version of some encoding functions.
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_SSE41)
#include <smmintrin.h>
#include <stdlib.h>  // for abs()

#include "src/dsp/common_sse2.h"
#include "src/enc/vp8i_enc.h"

//------------------------------------------------------------------------------
// Compute susceptibility based on DCT-coeff histograms.

static void CollectHistogram_SSE41(const uint8_t* WEBP_RESTRICT ref,
                                   const uint8_t* WEBP_RESTRICT pred,
                                   int start_block, int end_block,
                                   VP8Histogram* WEBP_RESTRICT const histo) {
  const __m128i max_coeff_thresh = _mm_set1_epi16(MAX_COEFF_THRESH);
  int j;
  int distribution[MAX_COEFF_THRESH + 1] = { 0 };
  for (j = start_block; j < end_block; ++j) {
    int16_t out[16];
    int k;

    VP8FTransform(ref + VP8DspScan[j], pred + VP8DspScan[j], out);

    // Convert coefficients to bin (within out[]).
    {
      // Load.
      const __m128i out0 = _mm_loadu_si128((__m128i*)&out[0]);
      const __m128i out1 = _mm_loadu_si128((__m128i*)&out[8]);
      // v = abs(out) >> 3
      const __m128i abs0 = _mm_abs_epi16(out0);
      const __m128i abs1 = _mm_abs_epi16(out1);
      const __m128i v0 = _mm_srai_epi16(abs0, 3);
      const __m128i v1 = _mm_srai_epi16(abs1, 3);
      // bin = min(v, MAX_COEFF_THRESH)
      const __m128i bin0 = _mm_min_epi16(v0, max_coeff_thresh);
      const __m128i bin1 = _mm_min_epi16(v1, max_coeff_thresh);
      // Store.
      _mm_storeu_si128((__m128i*)&out[0], bin0);
      _mm_storeu_si128((__m128i*)&out[8], bin1);
    }

    // Convert coefficients to bin.
    for (k = 0; k < 16; ++k) {
      ++distribution[out[k]];
    }
  }
  VP8SetHistogramData(distribution, histo);
}

//------------------------------------------------------------------------------
// Texture distortion
//
// We try to match the spectral content (weighted) between source and
// reconstructed samples.

// Hadamard transform
// Returns the weighted sum of the absolute value of transformed coefficients.
// w[] contains a row-major 4 by 4 symmetric matrix.
static int TTransform_SSE41(const uint8_t* inA, const uint8_t* inB,
                            const uint16_t* const w) {
  int32_t sum[4];
  __m128i tmp_0, tmp_1, tmp_2, tmp_3;

  // Load and combine inputs.
  {
    const __m128i inA_0 = _mm_loadu_si128((const __m128i*)&inA[BPS * 0]);
    const __m128i inA_1 = _mm_loadu_si128((const __m128i*)&inA[BPS * 1]);
    const __m128i inA_2 = _mm_loadu_si128((const __m128i*)&inA[BPS * 2]);
    // In SSE4.1, with gcc 4.8 at least (maybe other versions),
    // _mm_loadu_si128 is faster than _mm_loadl_epi64. But for the last lump
    // of inA and inB, _mm_loadl_epi64 is still used not to have an out of
    // bound read.
    const __m128i inA_3 = _mm_loadl_epi64((const __m128i*)&inA[BPS * 3]);
    const __m128i inB_0 = _mm_loadu_si128((const __m128i*)&inB[BPS * 0]);
    const __m128i inB_1 = _mm_loadu_si128((const __m128i*)&inB[BPS * 1]);
    const __m128i inB_2 = _mm_loadu_si128((const __m128i*)&inB[BPS * 2]);
    const __m128i inB_3 = _mm_loadl_epi64((const __m128i*)&inB[BPS * 3]);

    // Combine inA and inB (we'll do two transforms in parallel).
    const __m128i inAB_0 = _mm_unpacklo_epi32(inA_0, inB_0);
    const __m128i inAB_1 = _mm_unpacklo_epi32(inA_1, inB_1);
    const __m128i inAB_2 = _mm_unpacklo_epi32(inA_2, inB_2);
    const __m128i inAB_3 = _mm_unpacklo_epi32(inA_3, inB_3);
    tmp_0 = _mm_cvtepu8_epi16(inAB_0);
    tmp_1 = _mm_cvtepu8_epi16(inAB_1);
    tmp_2 = _mm_cvtepu8_epi16(inAB_2);
    tmp_3 = _mm_cvtepu8_epi16(inAB_3);
    // a00 a01 a02 a03   b00 b01 b02 b03
    // a10 a11 a12 a13   b10 b11 b12 b13
    // a20 a21 a22 a23   b20 b21 b22 b23
    // a30 a31 a32 a33   b30 b31 b32 b33
  }

  // Vertical pass first to avoid a transpose (vertical and horizontal passes
  // are commutative because w/kWeightY is symmetric) and subsequent transpose.
  {
    // Calculate a and b (two 4x4 at once).
    const __m128i a0 = _mm_add_epi16(tmp_0, tmp_2);
    const __m128i a1 = _mm_add_epi16(tmp_1, tmp_3);
    const __m128i a2 = _mm_sub_epi16(tmp_1, tmp_3);
    const __m128i a3 = _mm_sub_epi16(tmp_0, tmp_2);
    const __m128i b0 = _mm_add_epi16(a0, a1);
    const __m128i b1 = _mm_add_epi16(a3, a2);
    const __m128i b2 = _mm_sub_epi16(a3, a2);
    const __m128i b3 = _mm_sub_epi16(a0, a1);
    // a00 a01 a02 a03   b00 b01 b02 b03
    // a10 a11 a12 a13   b10 b11 b12 b13
    // a20 a21 a22 a23   b20 b21 b22 b23
    // a30 a31 a32 a33   b30 b31 b32 b33

    // Transpose the two 4x4.
    VP8Transpose_2_4x4_16b(&b0, &b1, &b2, &b3, &tmp_0, &tmp_1, &tmp_2, &tmp_3);
  }

  // Horizontal pass and difference of weighted sums.
  {
    // Load all inputs.
    const __m128i w_0 = _mm_loadu_si128((const __m128i*)&w[0]);
    const __m128i w_8 = _mm_loadu_si128((const __m128i*)&w[8]);

    // Calculate a and b (two 4x4 at once).
    const __m128i a0 = _mm_add_epi16(tmp_0, tmp_2);
    const __m128i a1 = _mm_add_epi16(tmp_1, tmp_3);
    const __m128i a2 = _mm_sub_epi16(tmp_1, tmp_3);
    const __m128i a3 = _mm_sub_epi16(tmp_0, tmp_2);
    const __m128i b0 = _mm_add_epi16(a0, a1);
    const __m128i b1 = _mm_add_epi16(a3, a2);
    const __m128i b2 = _mm_sub_epi16(a3, a2);
    const __m128i b3 = _mm_sub_epi16(a0, a1);

    // Separate the transforms of inA and inB.
    __m128i A_b0 = _mm_unpacklo_epi64(b0, b1);
    __m128i A_b2 = _mm_unpacklo_epi64(b2, b3);
    __m128i B_b0 = _mm_unpackhi_epi64(b0, b1);
    __m128i B_b2 = _mm_unpackhi_epi64(b2, b3);

    A_b0 = _mm_abs_epi16(A_b0);
    A_b2 = _mm_abs_epi16(A_b2);
    B_b0 = _mm_abs_epi16(B_b0);
    B_b2 = _mm_abs_epi16(B_b2);

    // weighted sums
    A_b0 = _mm_madd_epi16(A_b0, w_0);
    A_b2 = _mm_madd_epi16(A_b2, w_8);
    B_b0 = _mm_madd_epi16(B_b0, w_0);
    B_b2 = _mm_madd_epi16(B_b2, w_8);
    A_b0 = _mm_add_epi32(A_b0, A_b2);
    B_b0 = _mm_add_epi32(B_b0, B_b2);

    // difference of weighted sums
    A_b2 = _mm_sub_epi32(A_b0, B_b0);
    _mm_storeu_si128((__m128i*)&sum[0], A_b2);
  }
  return sum[0] + sum[1] + sum[2] + sum[3];
}

static int Disto4x4_SSE41(const uint8_t* WEBP_RESTRICT const a,
                          const uint8_t* WEBP_RESTRICT const b,
                          const uint16_t* WEBP_RESTRICT const w) {
  const int diff_sum = TTransform_SSE41(a, b, w);
  return abs(diff_sum) >> 5;
}

static int Disto16x16_SSE41(const uint8_t* WEBP_RESTRICT const a,
                            const uint8_t* WEBP_RESTRICT const b,
                            const uint16_t* WEBP_RESTRICT const w) {
  int D = 0;
  int x, y;
  for (y = 0; y < 16 * BPS; y += 4 * BPS) {
    for (x = 0; x < 16; x += 4) {
      D += Disto4x4_SSE41(a + x + y, b + x + y, w);
    }
  }
  return D;
}

//------------------------------------------------------------------------------
// Quantization
//

// Generates a pshufb constant for shuffling 16b words.
#define PSHUFB_CST(A,B,C,D,E,F,G,H) \
  _mm_set_epi8(2 * (H) + 1, 2 * (H) + 0, 2 * (G) + 1, 2 * (G) + 0, \
               2 * (F) + 1, 2 * (F) + 0, 2 * (E) + 1, 2 * (E) + 0, \
               2 * (D) + 1, 2 * (D) + 0, 2 * (C) + 1, 2 * (C) + 0, \
               2 * (B) + 1, 2 * (B) + 0, 2 * (A) + 1, 2 * (A) + 0)

static WEBP_INLINE int DoQuantizeBlock_SSE41(int16_t in[16], int16_t out[16],
                                             const uint16_t* const sharpen,
                                             const VP8Matrix* const mtx) {
  const __m128i max_coeff_2047 = _mm_set1_epi16(MAX_LEVEL);
  const __m128i zero = _mm_setzero_si128();
  __m128i out0, out8;
  __m128i packed_out;

  // Load all inputs.
  __m128i in0 = _mm_loadu_si128((__m128i*)&in[0]);
  __m128i in8 = _mm_loadu_si128((__m128i*)&in[8]);
  const __m128i iq0 = _mm_loadu_si128((const __m128i*)&mtx->iq_[0]);
  const __m128i iq8 = _mm_loadu_si128((const __m128i*)&mtx->iq_[8]);
  const __m128i q0 = _mm_loadu_si128((const __m128i*)&mtx->q_[0]);
  const __m128i q8 = _mm_loadu_si128((const __m128i*)&mtx->q_[8]);

  // coeff = abs(in)
  __m128i coeff0 = _mm_abs_epi16(in0);
  __m128i coeff8 = _mm_abs_epi16(in8);

  // coeff = abs(in) + sharpen
  if (sharpen != NULL) {
    const __m128i sharpen0 = _mm_loadu_si128((const __m128i*)&sharpen[0]);
    const __m128i sharpen8 = _mm_loadu_si128((const __m128i*)&sharpen[8]);
    coeff0 = _mm_add_epi16(coeff0, sharpen0);
    coeff8 = _mm_add_epi16(coeff8, sharpen8);
  }

  // out = (coeff * iQ + B) >> QFIX
  {
    // doing calculations with 32b precision (QFIX=17)
    // out = (coeff * iQ)
    const __m128i coeff_iQ0H = _mm_mulhi_epu16(coeff0, iq0);
    const __m128i coeff_iQ0L = _mm_mullo_epi16(coeff0, iq0);
    const __m128i coeff_iQ8H = _mm_mulhi_epu16(coeff8, iq8);
    const __m128i coeff_iQ8L = _mm_mullo_epi16(coeff8, iq8);
    __m128i out_00 = _mm_unpacklo_epi16(coeff_iQ0L, coeff_iQ0H);
    __m128i out_04 = _mm_unpackhi_epi16(coeff_iQ0L, coeff_iQ0H);
    __m128i out_08 = _mm_unpacklo_epi16(coeff_iQ8L, coeff_iQ8H);
    __m128i out_12 = _mm_unpackhi_epi16(coeff_iQ8L, coeff_iQ8H);
    // out = (coeff * iQ + B)
    const __m128i bias_00 = _mm_loadu_si128((const __m128i*)&mtx->bias_[0]);
    const __m128i bias_04 = _mm_loadu_si128((const __m128i*)&mtx->bias_[4]);
    const __m128i bias_08 = _mm_loadu_si128((const __m128i*)&mtx->bias_[8]);
    const __m128i bias_12 = _mm_loadu_si128((const __m128i*)&mtx->bias_[12]);
    out_00 = _mm_add_epi32(out_00, bias_00);
    out_04 = _mm_add_epi32(out_04, bias_04);
    out_08 = _mm_add_epi32(out_08, bias_08);
    out_12 = _mm_add_epi32(out_12, bias_12);
    // out = QUANTDIV(coeff, iQ, B, QFIX)
    out_00 = _mm_srai_epi32(out_00, QFIX);
    out_04 = _mm_srai_epi32(out_04, QFIX);
    out_08 = _mm_srai_epi32(out_08, QFIX);
    out_12 = _mm_srai_epi32(out_12, QFIX);

    // pack result as 16b
    out0 = _mm_packs_epi32(out_00, out_04);
    out8 = _mm_packs_epi32(out_08, out_12);

    // if (coeff > 2047) coeff = 2047
    out0 = _mm_min_epi16(out0, max_coeff_2047);
    out8 = _mm_min_epi16(out8, max_coeff_2047);
  }

  // put sign back
  out0 = _mm_sign_epi16(out0, in0);
  out8 = _mm_sign_epi16(out8, in8);

  // in = out * Q
  in0 = _mm_mullo_epi16(out0, q0);
  in8 = _mm_mullo_epi16(out8, q8);

  _mm_storeu_si128((__m128i*)&in[0], in0);
  _mm_storeu_si128((__m128i*)&in[8], in8);

  // zigzag the output before storing it. The re-ordering is:
  //    0 1 2 3 4 5 6 7 | 8  9 10 11 12 13 14 15
  // -> 0 1 4[8]5 2 3 6 | 9 12 13 10 [7]11 14 15
  // There's only two misplaced entries ([8] and [7]) that are crossing the
  // reg's boundaries.
  // We use pshufb instead of pshuflo/pshufhi.
  {
    const __m128i kCst_lo = PSHUFB_CST(0, 1, 4, -1, 5, 2, 3, 6);
    const __m128i kCst_7 = PSHUFB_CST(-1, -1, -1, -1, 7, -1, -1, -1);
    const __m128i tmp_lo = _mm_shuffle_epi8(out0, kCst_lo);
    const __m128i tmp_7 = _mm_shuffle_epi8(out0, kCst_7);  // extract #7
    const __m128i kCst_hi = PSHUFB_CST(1, 4, 5, 2, -1, 3, 6, 7);
    const __m128i kCst_8 = PSHUFB_CST(-1, -1, -1, 0, -1, -1, -1, -1);
    const __m128i tmp_hi = _mm_shuffle_epi8(out8, kCst_hi);
    const __m128i tmp_8 = _mm_shuffle_epi8(out8, kCst_8);  // extract #8
    const __m128i out_z0 = _mm_or_si128(tmp_lo, tmp_8);
    const __m128i out_z8 = _mm_or_si128(tmp_hi, tmp_7);
    _mm_storeu_si128((__m128i*)&out[0], out_z0);
    _mm_storeu_si128((__m128i*)&out[8], out_z8);
    packed_out = _mm_packs_epi16(out_z0, out_z8);
  }

  // detect if all 'out' values are zeroes or not
  return (_mm_movemask_epi8(_mm_cmpeq_epi8(packed_out, zero)) != 0xffff);
}

#undef PSHUFB_CST

static int QuantizeBlock_SSE41(int16_t in[16], int16_t out[16],
                               const VP8Matrix* WEBP_RESTRICT const mtx) {
  return DoQuantizeBlock_SSE41(in, out, &mtx->sharpen_[0], mtx);
}

static int QuantizeBlockWHT_SSE41(int16_t in[16], int16_t out[16],
                                  const VP8Matrix* WEBP_RESTRICT const mtx) {
  return DoQuantizeBlock_SSE41(in, out, NULL, mtx);
}

static int Quantize2Blocks_SSE41(int16_t in[32], int16_t out[32],
                                 const VP8Matrix* WEBP_RESTRICT const mtx) {
  int nz;
  const uint16_t* const sharpen = &mtx->sharpen_[0];
  nz  = DoQuantizeBlock_SSE41(in + 0 * 16, out + 0 * 16, sharpen, mtx) << 0;
  nz |= DoQuantizeBlock_SSE41(in + 1 * 16, out + 1 * 16, sharpen, mtx) << 1;
  return nz;
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspInitSSE41(void);
WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitSSE41(void) {
  VP8CollectHistogram = CollectHistogram_SSE41;
  VP8EncQuantizeBlock = QuantizeBlock_SSE41;
  VP8EncQuantize2Blocks = Quantize2Blocks_SSE41;
  VP8EncQuantizeBlockWHT = QuantizeBlockWHT_SSE41;
  VP8TDisto4x4 = Disto4x4_SSE41;
  VP8TDisto16x16 = Disto16x16_SSE41;
}

#else  // !WEBP_USE_SSE41

WEBP_DSP_INIT_STUB(VP8EncDspInitSSE41)

#endif  // WEBP_USE_SSE41
