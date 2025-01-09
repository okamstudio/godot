// Copyright 2018 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// ARM NEON version of cost functions

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_NEON)

#include "src/dsp/neon.h"
#include "src/enc/cost_enc.h"

static const uint8_t position[16] = { 1, 2,  3,  4,  5,  6,  7,  8,
                                      9, 10, 11, 12, 13, 14, 15, 16 };

static void SetResidualCoeffs_NEON(const int16_t* WEBP_RESTRICT const coeffs,
                                   VP8Residual* WEBP_RESTRICT const res) {
  const int16x8_t minus_one = vdupq_n_s16(-1);
  const int16x8_t coeffs_0 = vld1q_s16(coeffs);
  const int16x8_t coeffs_1 = vld1q_s16(coeffs + 8);
  const uint16x8_t eob_0 = vtstq_s16(coeffs_0, minus_one);
  const uint16x8_t eob_1 = vtstq_s16(coeffs_1, minus_one);
  const uint8x16_t eob = vcombine_u8(vqmovn_u16(eob_0), vqmovn_u16(eob_1));
  const uint8x16_t masked = vandq_u8(eob, vld1q_u8(position));

#if WEBP_AARCH64
  res->last = vmaxvq_u8(masked) - 1;
#else
  const uint8x8_t eob_8x8 = vmax_u8(vget_low_u8(masked), vget_high_u8(masked));
  const uint16x8_t eob_16x8 = vmovl_u8(eob_8x8);
  const uint16x4_t eob_16x4 =
      vmax_u16(vget_low_u16(eob_16x8), vget_high_u16(eob_16x8));
  const uint32x4_t eob_32x4 = vmovl_u16(eob_16x4);
  uint32x2_t eob_32x2 =
      vmax_u32(vget_low_u32(eob_32x4), vget_high_u32(eob_32x4));
  eob_32x2 = vpmax_u32(eob_32x2, eob_32x2);

  vst1_lane_s32(&res->last, vreinterpret_s32_u32(eob_32x2), 0);
  --res->last;
#endif  // WEBP_AARCH64

  res->coeffs = coeffs;
}

static int GetResidualCost_NEON(int ctx0, const VP8Residual* const res) {
  uint8_t levels[16], ctxs[16];
  uint16_t abs_levels[16];
  int n = res->first;
  // should be prob[VP8EncBands[n]], but it's equivalent for n=0 or 1
  const int p0 = res->prob[n][ctx0][0];
  CostArrayPtr const costs = res->costs;
  const uint16_t* t = costs[n][ctx0];
  // bit_cost(1, p0) is already incorporated in t[] tables, but only if ctx != 0
  // (as required by the syntax). For ctx0 == 0, we need to add it here or it'll
  // be missing during the loop.
  int cost = (ctx0 == 0) ? VP8BitCost(1, p0) : 0;

  if (res->last < 0) {
    return VP8BitCost(0, p0);
  }

  {   // precompute clamped levels and contexts, packed to 8b.
    const uint8x16_t kCst2 = vdupq_n_u8(2);
    const uint8x16_t kCst67 = vdupq_n_u8(MAX_VARIABLE_LEVEL);
    const int16x8_t c0 = vld1q_s16(res->coeffs);
    const int16x8_t c1 = vld1q_s16(res->coeffs + 8);
    const uint16x8_t E0 = vreinterpretq_u16_s16(vabsq_s16(c0));
    const uint16x8_t E1 = vreinterpretq_u16_s16(vabsq_s16(c1));
    const uint8x16_t F = vcombine_u8(vqmovn_u16(E0), vqmovn_u16(E1));
    const uint8x16_t G = vminq_u8(F, kCst2);   // context = 0,1,2
    const uint8x16_t H = vminq_u8(F, kCst67);  // clamp_level in [0..67]

    vst1q_u8(ctxs, G);
    vst1q_u8(levels, H);

    vst1q_u16(abs_levels, E0);
    vst1q_u16(abs_levels + 8, E1);
  }
  for (; n < res->last; ++n) {
    const int ctx = ctxs[n];
    const int level = levels[n];
    const int flevel = abs_levels[n];   // full level
    cost += VP8LevelFixedCosts[flevel] + t[level];  // simplified VP8LevelCost()
    t = costs[n + 1][ctx];
  }
  // Last coefficient is always non-zero
  {
    const int level = levels[n];
    const int flevel = abs_levels[n];
    assert(flevel != 0);
    cost += VP8LevelFixedCosts[flevel] + t[level];
    if (n < 15) {
      const int b = VP8EncBands[n + 1];
      const int ctx = ctxs[n];
      const int last_p0 = res->prob[b][ctx][0];
      cost += VP8BitCost(0, last_p0);
    }
  }
  return cost;
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspCostInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspCostInitNEON(void) {
  VP8SetResidualCoeffs = SetResidualCoeffs_NEON;
  VP8GetResidualCost = GetResidualCost_NEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8EncDspCostInitNEON)

#endif  // WEBP_USE_NEON
