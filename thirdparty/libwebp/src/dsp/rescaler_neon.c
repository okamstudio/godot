// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// NEON version of rescaling functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_NEON) && !defined(WEBP_REDUCE_SIZE)

#include <arm_neon.h>
#include <assert.h>
#include "src/dsp/neon.h"
#include "src/utils/rescaler_utils.h"

#define ROUNDER (WEBP_RESCALER_ONE >> 1)
#define MULT_FIX_C(x, y) (((uint64_t)(x) * (y) + ROUNDER) >> WEBP_RESCALER_RFIX)
#define MULT_FIX_FLOOR_C(x, y) (((uint64_t)(x) * (y)) >> WEBP_RESCALER_RFIX)

#define LOAD_32x4(SRC, DST) const uint32x4_t DST = vld1q_u32((SRC))
#define LOAD_32x8(SRC, DST0, DST1)                                    \
    LOAD_32x4(SRC + 0, DST0);                                         \
    LOAD_32x4(SRC + 4, DST1)

#define STORE_32x8(SRC0, SRC1, DST) do {                              \
    vst1q_u32((DST) + 0, SRC0);                                       \
    vst1q_u32((DST) + 4, SRC1);                                       \
} while (0)

#if (WEBP_RESCALER_RFIX == 32)
#define MAKE_HALF_CST(C) vdupq_n_s32((int32_t)((C) >> 1))
// note: B is actualy scale>>1. See MAKE_HALF_CST
#define MULT_FIX(A, B) \
    vreinterpretq_u32_s32(vqrdmulhq_s32(vreinterpretq_s32_u32((A)), (B)))
#define MULT_FIX_FLOOR(A, B) \
    vreinterpretq_u32_s32(vqdmulhq_s32(vreinterpretq_s32_u32((A)), (B)))
#else
#error "MULT_FIX/WEBP_RESCALER_RFIX need some more work"
#endif

static uint32x4_t Interpolate_NEON(const rescaler_t* WEBP_RESTRICT const frow,
                                   const rescaler_t* WEBP_RESTRICT const irow,
                                   uint32_t A, uint32_t B) {
  LOAD_32x4(frow, A0);
  LOAD_32x4(irow, B0);
  const uint64x2_t C0 = vmull_n_u32(vget_low_u32(A0), A);
  const uint64x2_t C1 = vmull_n_u32(vget_high_u32(A0), A);
  const uint64x2_t D0 = vmlal_n_u32(C0, vget_low_u32(B0), B);
  const uint64x2_t D1 = vmlal_n_u32(C1, vget_high_u32(B0), B);
  const uint32x4_t E = vcombine_u32(
      vrshrn_n_u64(D0, WEBP_RESCALER_RFIX),
      vrshrn_n_u64(D1, WEBP_RESCALER_RFIX));
  return E;
}

static void RescalerExportRowExpand_NEON(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const int max_span = x_out_max & ~7;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t fy_scale = wrk->fy_scale;
  const int32x4_t fy_scale_half = MAKE_HALF_CST(fy_scale);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(wrk->y_expand);
  assert(wrk->y_sub != 0);
  if (wrk->y_accum == 0) {
    for (x_out = 0; x_out < max_span; x_out += 8) {
      LOAD_32x4(frow + x_out + 0, A0);
      LOAD_32x4(frow + x_out + 4, A1);
      const uint32x4_t B0 = MULT_FIX(A0, fy_scale_half);
      const uint32x4_t B1 = MULT_FIX(A1, fy_scale_half);
      const uint16x4_t C0 = vmovn_u32(B0);
      const uint16x4_t C1 = vmovn_u32(B1);
      const uint8x8_t D = vqmovn_u16(vcombine_u16(C0, C1));
      vst1_u8(dst + x_out, D);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t J = frow[x_out];
      const int v = (int)MULT_FIX_C(J, fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  } else {
    const uint32_t B = WEBP_RESCALER_FRAC(-wrk->y_accum, wrk->y_sub);
    const uint32_t A = (uint32_t)(WEBP_RESCALER_ONE - B);
    for (x_out = 0; x_out < max_span; x_out += 8) {
      const uint32x4_t C0 =
          Interpolate_NEON(frow + x_out + 0, irow + x_out + 0, A, B);
      const uint32x4_t C1 =
          Interpolate_NEON(frow + x_out + 4, irow + x_out + 4, A, B);
      const uint32x4_t D0 = MULT_FIX(C0, fy_scale_half);
      const uint32x4_t D1 = MULT_FIX(C1, fy_scale_half);
      const uint16x4_t E0 = vmovn_u32(D0);
      const uint16x4_t E1 = vmovn_u32(D1);
      const uint8x8_t F = vqmovn_u16(vcombine_u16(E0, E1));
      vst1_u8(dst + x_out, F);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint64_t I = (uint64_t)A * frow[x_out]
                       + (uint64_t)B * irow[x_out];
      const uint32_t J = (uint32_t)((I + ROUNDER) >> WEBP_RESCALER_RFIX);
      const int v = (int)MULT_FIX_C(J, fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  }
}

static void RescalerExportRowShrink_NEON(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const int max_span = x_out_max & ~7;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t yscale = wrk->fy_scale * (-wrk->y_accum);
  const uint32_t fxy_scale = wrk->fxy_scale;
  const uint32x4_t zero = vdupq_n_u32(0);
  const int32x4_t yscale_half = MAKE_HALF_CST(yscale);
  const int32x4_t fxy_scale_half = MAKE_HALF_CST(fxy_scale);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(!wrk->y_expand);
  if (yscale) {
    for (x_out = 0; x_out < max_span; x_out += 8) {
      LOAD_32x8(frow + x_out, in0, in1);
      LOAD_32x8(irow + x_out, in2, in3);
      const uint32x4_t A0 = MULT_FIX_FLOOR(in0, yscale_half);
      const uint32x4_t A1 = MULT_FIX_FLOOR(in1, yscale_half);
      const uint32x4_t B0 = vqsubq_u32(in2, A0);
      const uint32x4_t B1 = vqsubq_u32(in3, A1);
      const uint32x4_t C0 = MULT_FIX(B0, fxy_scale_half);
      const uint32x4_t C1 = MULT_FIX(B1, fxy_scale_half);
      const uint16x4_t D0 = vmovn_u32(C0);
      const uint16x4_t D1 = vmovn_u32(C1);
      const uint8x8_t E = vqmovn_u16(vcombine_u16(D0, D1));
      vst1_u8(dst + x_out, E);
      STORE_32x8(A0, A1, irow + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t frac = (uint32_t)MULT_FIX_FLOOR_C(frow[x_out], yscale);
      const int v = (int)MULT_FIX_C(irow[x_out] - frac, fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = frac;   // new fractional start
    }
  } else {
    for (x_out = 0; x_out < max_span; x_out += 8) {
      LOAD_32x8(irow + x_out, in0, in1);
      const uint32x4_t A0 = MULT_FIX(in0, fxy_scale_half);
      const uint32x4_t A1 = MULT_FIX(in1, fxy_scale_half);
      const uint16x4_t B0 = vmovn_u32(A0);
      const uint16x4_t B1 = vmovn_u32(A1);
      const uint8x8_t C = vqmovn_u16(vcombine_u16(B0, B1));
      vst1_u8(dst + x_out, C);
      STORE_32x8(zero, zero, irow + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const int v = (int)MULT_FIX_C(irow[x_out], fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = 0;
    }
  }
}

#undef MULT_FIX_FLOOR_C
#undef MULT_FIX_C
#undef MULT_FIX_FLOOR
#undef MULT_FIX
#undef ROUNDER

//------------------------------------------------------------------------------

extern void WebPRescalerDspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPRescalerDspInitNEON(void) {
  WebPRescalerExportRowExpand = RescalerExportRowExpand_NEON;
  WebPRescalerExportRowShrink = RescalerExportRowShrink_NEON;
}

#else     // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(WebPRescalerDspInitNEON)

#endif    // WEBP_USE_NEON
