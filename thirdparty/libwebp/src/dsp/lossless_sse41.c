// Copyright 2021 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE41 variant of methods for lossless decoder

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_SSE41)

#include "src/dsp/common_sse41.h"
#include "src/dsp/lossless.h"
#include "src/dsp/lossless_common.h"

//------------------------------------------------------------------------------
// Color-space conversion functions

static void TransformColorInverse_SSE41(const VP8LMultipliers* const m,
                                        const uint32_t* const src,
                                        int num_pixels, uint32_t* dst) {
// sign-extended multiplying constants, pre-shifted by 5.
#define CST(X)  (((int16_t)(m->X << 8)) >> 5)   // sign-extend
  const __m128i mults_rb =
      _mm_set1_epi32((int)((uint32_t)CST(green_to_red_) << 16 |
                           (CST(green_to_blue_) & 0xffff)));
  const __m128i mults_b2 = _mm_set1_epi32(CST(red_to_blue_));
#undef CST
  const __m128i mask_ag = _mm_set1_epi32((int)0xff00ff00);
  const __m128i perm1 = _mm_setr_epi8(-1, 1, -1, 1, -1, 5, -1, 5,
                                      -1, 9, -1, 9, -1, 13, -1, 13);
  const __m128i perm2 = _mm_setr_epi8(-1, 2, -1, -1, -1, 6, -1, -1,
                                      -1, 10, -1, -1, -1, 14, -1, -1);
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i A = _mm_loadu_si128((const __m128i*)(src + i));
    const __m128i B = _mm_shuffle_epi8(A, perm1); // argb -> g0g0
    const __m128i C = _mm_mulhi_epi16(B, mults_rb);
    const __m128i D = _mm_add_epi8(A, C);
    const __m128i E = _mm_shuffle_epi8(D, perm2);
    const __m128i F = _mm_mulhi_epi16(E, mults_b2);
    const __m128i G = _mm_add_epi8(D, F);
    const __m128i out = _mm_blendv_epi8(G, A, mask_ag);
    _mm_storeu_si128((__m128i*)&dst[i], out);
  }
  // Fall-back to C-version for left-overs.
  if (i != num_pixels) {
    VP8LTransformColorInverse_C(m, src + i, num_pixels - i, dst + i);
  }
}

//------------------------------------------------------------------------------

#define ARGB_TO_RGB_SSE41 do {                        \
  while (num_pixels >= 16) {                          \
    const __m128i in0 = _mm_loadu_si128(in + 0);      \
    const __m128i in1 = _mm_loadu_si128(in + 1);      \
    const __m128i in2 = _mm_loadu_si128(in + 2);      \
    const __m128i in3 = _mm_loadu_si128(in + 3);      \
    const __m128i a0 = _mm_shuffle_epi8(in0, perm0);  \
    const __m128i a1 = _mm_shuffle_epi8(in1, perm1);  \
    const __m128i a2 = _mm_shuffle_epi8(in2, perm2);  \
    const __m128i a3 = _mm_shuffle_epi8(in3, perm3);  \
    const __m128i b0 = _mm_blend_epi16(a0, a1, 0xc0); \
    const __m128i b1 = _mm_blend_epi16(a1, a2, 0xf0); \
    const __m128i b2 = _mm_blend_epi16(a2, a3, 0xfc); \
    _mm_storeu_si128(out + 0, b0);                    \
    _mm_storeu_si128(out + 1, b1);                    \
    _mm_storeu_si128(out + 2, b2);                    \
    in += 4;                                          \
    out += 3;                                         \
    num_pixels -= 16;                                 \
  }                                                   \
} while (0)

static void ConvertBGRAToRGB_SSE41(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  const __m128i perm0 = _mm_setr_epi8(2, 1, 0, 6, 5, 4, 10, 9,
                                      8, 14, 13, 12, -1, -1, -1, -1);
  const __m128i perm1 = _mm_shuffle_epi32(perm0, 0x39);
  const __m128i perm2 = _mm_shuffle_epi32(perm0, 0x4e);
  const __m128i perm3 = _mm_shuffle_epi32(perm0, 0x93);

  ARGB_TO_RGB_SSE41;

  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToRGB_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToBGR_SSE41(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  const __m128i perm0 = _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10,
                                      12, 13, 14, -1, -1, -1, -1);
  const __m128i perm1 = _mm_shuffle_epi32(perm0, 0x39);
  const __m128i perm2 = _mm_shuffle_epi32(perm0, 0x4e);
  const __m128i perm3 = _mm_shuffle_epi32(perm0, 0x93);

  ARGB_TO_RGB_SSE41;

  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToBGR_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

#undef ARGB_TO_RGB_SSE41

//------------------------------------------------------------------------------
// Entry point

extern void VP8LDspInitSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitSSE41(void) {
  VP8LTransformColorInverse = TransformColorInverse_SSE41;
  VP8LConvertBGRAToRGB = ConvertBGRAToRGB_SSE41;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR_SSE41;
}

#else  // !WEBP_USE_SSE41

WEBP_DSP_INIT_STUB(VP8LDspInitSSE41)

#endif  // WEBP_USE_SSE41
