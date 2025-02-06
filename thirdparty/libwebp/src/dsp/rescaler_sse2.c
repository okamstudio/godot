// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE2 Rescaling functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_SSE2) && !defined(WEBP_REDUCE_SIZE)
#include <emmintrin.h>

#include <assert.h>
#include "src/utils/rescaler_utils.h"
#include "src/utils/utils.h"

//------------------------------------------------------------------------------
// Implementations of critical functions ImportRow / ExportRow

#define ROUNDER (WEBP_RESCALER_ONE >> 1)
#define MULT_FIX(x, y) (((uint64_t)(x) * (y) + ROUNDER) >> WEBP_RESCALER_RFIX)
#define MULT_FIX_FLOOR(x, y) (((uint64_t)(x) * (y)) >> WEBP_RESCALER_RFIX)

// input: 8 bytes ABCDEFGH -> output: A0E0B0F0C0G0D0H0
static void LoadTwoPixels_SSE2(const uint8_t* const src, __m128i* out) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i A = _mm_loadl_epi64((const __m128i*)(src));  // ABCDEFGH
  const __m128i B = _mm_unpacklo_epi8(A, zero);              // A0B0C0D0E0F0G0H0
  const __m128i C = _mm_srli_si128(B, 8);                    // E0F0G0H0
  *out = _mm_unpacklo_epi16(B, C);
}

// input: 8 bytes ABCDEFGH -> output: A0B0C0D0E0F0G0H0
static void LoadEightPixels_SSE2(const uint8_t* const src, __m128i* out) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i A = _mm_loadl_epi64((const __m128i*)(src));  // ABCDEFGH
  *out = _mm_unpacklo_epi8(A, zero);
}

static void RescalerImportRowExpand_SSE2(WebPRescaler* WEBP_RESTRICT const wrk,
                                         const uint8_t* WEBP_RESTRICT src) {
  rescaler_t* frow = wrk->frow;
  const rescaler_t* const frow_end = frow + wrk->dst_width * wrk->num_channels;
  const int x_add = wrk->x_add;
  int accum = x_add;
  __m128i cur_pixels;

  // SSE2 implementation only works with 16b signed arithmetic at max.
  if (wrk->src_width < 8 || accum >= (1 << 15)) {
    WebPRescalerImportRowExpand_C(wrk, src);
    return;
  }

  assert(!WebPRescalerInputDone(wrk));
  assert(wrk->x_expand);
  if (wrk->num_channels == 4) {
    LoadTwoPixels_SSE2(src, &cur_pixels);
    src += 4;
    while (1) {
      const __m128i mult = _mm_set1_epi32(((x_add - accum) << 16) | accum);
      const __m128i out = _mm_madd_epi16(cur_pixels, mult);
      _mm_storeu_si128((__m128i*)frow, out);
      frow += 4;
      if (frow >= frow_end) break;
      accum -= wrk->x_sub;
      if (accum < 0) {
        LoadTwoPixels_SSE2(src, &cur_pixels);
        src += 4;
        accum += x_add;
      }
    }
  } else {
    int left;
    const uint8_t* const src_limit = src + wrk->src_width - 8;
    LoadEightPixels_SSE2(src, &cur_pixels);
    src += 7;
    left = 7;
    while (1) {
      const __m128i mult = _mm_cvtsi32_si128(((x_add - accum) << 16) | accum);
      const __m128i out = _mm_madd_epi16(cur_pixels, mult);
      assert(sizeof(*frow) == sizeof(uint32_t));
      WebPInt32ToMem((uint8_t*)frow, _mm_cvtsi128_si32(out));
      frow += 1;
      if (frow >= frow_end) break;
      accum -= wrk->x_sub;
      if (accum < 0) {
        if (--left) {
          cur_pixels = _mm_srli_si128(cur_pixels, 2);
        } else if (src <= src_limit) {
          LoadEightPixels_SSE2(src, &cur_pixels);
          src += 7;
          left = 7;
        } else {   // tail
          cur_pixels = _mm_srli_si128(cur_pixels, 2);
          cur_pixels = _mm_insert_epi16(cur_pixels, src[1], 1);
          src += 1;
          left = 1;
        }
        accum += x_add;
      }
    }
  }
  assert(accum == 0);
}

static void RescalerImportRowShrink_SSE2(WebPRescaler* WEBP_RESTRICT const wrk,
                                         const uint8_t* WEBP_RESTRICT src) {
  const int x_sub = wrk->x_sub;
  int accum = 0;
  const __m128i zero = _mm_setzero_si128();
  const __m128i mult0 = _mm_set1_epi16(x_sub);
  const __m128i mult1 = _mm_set1_epi32(wrk->fx_scale);
  const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
  __m128i sum = zero;
  rescaler_t* frow = wrk->frow;
  const rescaler_t* const frow_end = wrk->frow + 4 * wrk->dst_width;

  if (wrk->num_channels != 4 || wrk->x_add > (x_sub << 7)) {
    WebPRescalerImportRowShrink_C(wrk, src);
    return;
  }
  assert(!WebPRescalerInputDone(wrk));
  assert(!wrk->x_expand);

  for (; frow < frow_end; frow += 4) {
    __m128i base = zero;
    accum += wrk->x_add;
    while (accum > 0) {
      const __m128i A = _mm_cvtsi32_si128(WebPMemToInt32(src));
      src += 4;
      base = _mm_unpacklo_epi8(A, zero);
      // To avoid overflow, we need: base * x_add / x_sub < 32768
      // => x_add < x_sub << 7. That's a 1/128 reduction ratio limit.
      sum = _mm_add_epi16(sum, base);
      accum -= x_sub;
    }
    {    // Emit next horizontal pixel.
      const __m128i mult = _mm_set1_epi16(-accum);
      const __m128i frac0 = _mm_mullo_epi16(base, mult);  // 16b x 16b -> 32b
      const __m128i frac1 = _mm_mulhi_epu16(base, mult);
      const __m128i frac = _mm_unpacklo_epi16(frac0, frac1);  // frac is 32b
      const __m128i A0 = _mm_mullo_epi16(sum, mult0);
      const __m128i A1 = _mm_mulhi_epu16(sum, mult0);
      const __m128i B0 = _mm_unpacklo_epi16(A0, A1);      // sum * x_sub
      const __m128i frow_out = _mm_sub_epi32(B0, frac);   // sum * x_sub - frac
      const __m128i D0 = _mm_srli_epi64(frac, 32);
      const __m128i D1 = _mm_mul_epu32(frac, mult1);      // 32b x 16b -> 64b
      const __m128i D2 = _mm_mul_epu32(D0, mult1);
      const __m128i E1 = _mm_add_epi64(D1, rounder);
      const __m128i E2 = _mm_add_epi64(D2, rounder);
      const __m128i F1 = _mm_shuffle_epi32(E1, 1 | (3 << 2));
      const __m128i F2 = _mm_shuffle_epi32(E2, 1 | (3 << 2));
      const __m128i G = _mm_unpacklo_epi32(F1, F2);
      sum = _mm_packs_epi32(G, zero);
      _mm_storeu_si128((__m128i*)frow, frow_out);
    }
  }
  assert(accum == 0);
}

//------------------------------------------------------------------------------
// Row export

// load *src as epi64, multiply by mult and store result in [out0 ... out3]
static WEBP_INLINE void LoadDispatchAndMult_SSE2(
    const rescaler_t* WEBP_RESTRICT const src, const __m128i* const mult,
    __m128i* const out0, __m128i* const out1, __m128i* const out2,
    __m128i* const out3) {
  const __m128i A0 = _mm_loadu_si128((const __m128i*)(src + 0));
  const __m128i A1 = _mm_loadu_si128((const __m128i*)(src + 4));
  const __m128i A2 = _mm_srli_epi64(A0, 32);
  const __m128i A3 = _mm_srli_epi64(A1, 32);
  if (mult != NULL) {
    *out0 = _mm_mul_epu32(A0, *mult);
    *out1 = _mm_mul_epu32(A1, *mult);
    *out2 = _mm_mul_epu32(A2, *mult);
    *out3 = _mm_mul_epu32(A3, *mult);
  } else {
    *out0 = A0;
    *out1 = A1;
    *out2 = A2;
    *out3 = A3;
  }
}

static WEBP_INLINE void ProcessRow_SSE2(const __m128i* const A0,
                                        const __m128i* const A1,
                                        const __m128i* const A2,
                                        const __m128i* const A3,
                                        const __m128i* const mult,
                                        uint8_t* const dst) {
  const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
  const __m128i mask = _mm_set_epi32(~0, 0, ~0, 0);
  const __m128i B0 = _mm_mul_epu32(*A0, *mult);
  const __m128i B1 = _mm_mul_epu32(*A1, *mult);
  const __m128i B2 = _mm_mul_epu32(*A2, *mult);
  const __m128i B3 = _mm_mul_epu32(*A3, *mult);
  const __m128i C0 = _mm_add_epi64(B0, rounder);
  const __m128i C1 = _mm_add_epi64(B1, rounder);
  const __m128i C2 = _mm_add_epi64(B2, rounder);
  const __m128i C3 = _mm_add_epi64(B3, rounder);
  const __m128i D0 = _mm_srli_epi64(C0, WEBP_RESCALER_RFIX);
  const __m128i D1 = _mm_srli_epi64(C1, WEBP_RESCALER_RFIX);
#if (WEBP_RESCALER_RFIX < 32)
  const __m128i D2 =
      _mm_and_si128(_mm_slli_epi64(C2, 32 - WEBP_RESCALER_RFIX), mask);
  const __m128i D3 =
      _mm_and_si128(_mm_slli_epi64(C3, 32 - WEBP_RESCALER_RFIX), mask);
#else
  const __m128i D2 = _mm_and_si128(C2, mask);
  const __m128i D3 = _mm_and_si128(C3, mask);
#endif
  const __m128i E0 = _mm_or_si128(D0, D2);
  const __m128i E1 = _mm_or_si128(D1, D3);
  const __m128i F = _mm_packs_epi32(E0, E1);
  const __m128i G = _mm_packus_epi16(F, F);
  _mm_storel_epi64((__m128i*)dst, G);
}

static void RescalerExportRowExpand_SSE2(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  const __m128i mult = _mm_set_epi32(0, wrk->fy_scale, 0, wrk->fy_scale);

  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0 && wrk->y_sub + wrk->y_accum >= 0);
  assert(wrk->y_expand);
  if (wrk->y_accum == 0) {
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3;
      LoadDispatchAndMult_SSE2(frow + x_out, NULL, &A0, &A1, &A2, &A3);
      ProcessRow_SSE2(&A0, &A1, &A2, &A3, &mult, dst + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t J = frow[x_out];
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  } else {
    const uint32_t B = WEBP_RESCALER_FRAC(-wrk->y_accum, wrk->y_sub);
    const uint32_t A = (uint32_t)(WEBP_RESCALER_ONE - B);
    const __m128i mA = _mm_set_epi32(0, A, 0, A);
    const __m128i mB = _mm_set_epi32(0, B, 0, B);
    const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3, B0, B1, B2, B3;
      LoadDispatchAndMult_SSE2(frow + x_out, &mA, &A0, &A1, &A2, &A3);
      LoadDispatchAndMult_SSE2(irow + x_out, &mB, &B0, &B1, &B2, &B3);
      {
        const __m128i C0 = _mm_add_epi64(A0, B0);
        const __m128i C1 = _mm_add_epi64(A1, B1);
        const __m128i C2 = _mm_add_epi64(A2, B2);
        const __m128i C3 = _mm_add_epi64(A3, B3);
        const __m128i D0 = _mm_add_epi64(C0, rounder);
        const __m128i D1 = _mm_add_epi64(C1, rounder);
        const __m128i D2 = _mm_add_epi64(C2, rounder);
        const __m128i D3 = _mm_add_epi64(C3, rounder);
        const __m128i E0 = _mm_srli_epi64(D0, WEBP_RESCALER_RFIX);
        const __m128i E1 = _mm_srli_epi64(D1, WEBP_RESCALER_RFIX);
        const __m128i E2 = _mm_srli_epi64(D2, WEBP_RESCALER_RFIX);
        const __m128i E3 = _mm_srli_epi64(D3, WEBP_RESCALER_RFIX);
        ProcessRow_SSE2(&E0, &E1, &E2, &E3, &mult, dst + x_out);
      }
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint64_t I = (uint64_t)A * frow[x_out]
                       + (uint64_t)B * irow[x_out];
      const uint32_t J = (uint32_t)((I + ROUNDER) >> WEBP_RESCALER_RFIX);
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  }
}

static void RescalerExportRowShrink_SSE2(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t yscale = wrk->fy_scale * (-wrk->y_accum);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(!wrk->y_expand);
  if (yscale) {
    const int scale_xy = wrk->fxy_scale;
    const __m128i mult_xy = _mm_set_epi32(0, scale_xy, 0, scale_xy);
    const __m128i mult_y = _mm_set_epi32(0, yscale, 0, yscale);
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3, B0, B1, B2, B3;
      LoadDispatchAndMult_SSE2(irow + x_out, NULL, &A0, &A1, &A2, &A3);
      LoadDispatchAndMult_SSE2(frow + x_out, &mult_y, &B0, &B1, &B2, &B3);
      {
        const __m128i D0 = _mm_srli_epi64(B0, WEBP_RESCALER_RFIX);   // = frac
        const __m128i D1 = _mm_srli_epi64(B1, WEBP_RESCALER_RFIX);
        const __m128i D2 = _mm_srli_epi64(B2, WEBP_RESCALER_RFIX);
        const __m128i D3 = _mm_srli_epi64(B3, WEBP_RESCALER_RFIX);
        const __m128i E0 = _mm_sub_epi64(A0, D0);   // irow[x] - frac
        const __m128i E1 = _mm_sub_epi64(A1, D1);
        const __m128i E2 = _mm_sub_epi64(A2, D2);
        const __m128i E3 = _mm_sub_epi64(A3, D3);
        const __m128i F2 = _mm_slli_epi64(D2, 32);
        const __m128i F3 = _mm_slli_epi64(D3, 32);
        const __m128i G0 = _mm_or_si128(D0, F2);
        const __m128i G1 = _mm_or_si128(D1, F3);
        _mm_storeu_si128((__m128i*)(irow + x_out + 0), G0);
        _mm_storeu_si128((__m128i*)(irow + x_out + 4), G1);
        ProcessRow_SSE2(&E0, &E1, &E2, &E3, &mult_xy, dst + x_out);
      }
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t frac = (int)MULT_FIX_FLOOR(frow[x_out], yscale);
      const int v = (int)MULT_FIX(irow[x_out] - frac, wrk->fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = frac;   // new fractional start
    }
  } else {
    const uint32_t scale = wrk->fxy_scale;
    const __m128i mult = _mm_set_epi32(0, scale, 0, scale);
    const __m128i zero = _mm_setzero_si128();
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3;
      LoadDispatchAndMult_SSE2(irow + x_out, NULL, &A0, &A1, &A2, &A3);
      _mm_storeu_si128((__m128i*)(irow + x_out + 0), zero);
      _mm_storeu_si128((__m128i*)(irow + x_out + 4), zero);
      ProcessRow_SSE2(&A0, &A1, &A2, &A3, &mult, dst + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const int v = (int)MULT_FIX(irow[x_out], scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = 0;
    }
  }
}

#undef MULT_FIX_FLOOR
#undef MULT_FIX
#undef ROUNDER

//------------------------------------------------------------------------------

extern void WebPRescalerDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPRescalerDspInitSSE2(void) {
  WebPRescalerImportRowExpand = RescalerImportRowExpand_SSE2;
  WebPRescalerImportRowShrink = RescalerImportRowShrink_SSE2;
  WebPRescalerExportRowExpand = RescalerExportRowExpand_SSE2;
  WebPRescalerExportRowShrink = RescalerExportRowShrink_SSE2;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(WebPRescalerDspInitSSE2)

#endif  // WEBP_USE_SSE2
