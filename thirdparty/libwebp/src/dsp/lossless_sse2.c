// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE2 variant of methods for lossless decoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_SSE2)

#include "src/dsp/common_sse2.h"
#include "src/dsp/lossless.h"
#include "src/dsp/lossless_common.h"
#include <emmintrin.h>

//------------------------------------------------------------------------------
// Predictor Transform

static WEBP_INLINE uint32_t ClampedAddSubtractFull_SSE2(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i C0 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c0), zero);
  const __m128i C1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c1), zero);
  const __m128i C2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c2), zero);
  const __m128i V1 = _mm_add_epi16(C0, C1);
  const __m128i V2 = _mm_sub_epi16(V1, C2);
  const __m128i b = _mm_packus_epi16(V2, V2);
  return (uint32_t)_mm_cvtsi128_si32(b);
}

static WEBP_INLINE uint32_t ClampedAddSubtractHalf_SSE2(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i C0 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c0), zero);
  const __m128i C1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c1), zero);
  const __m128i B0 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c2), zero);
  const __m128i avg = _mm_add_epi16(C1, C0);
  const __m128i A0 = _mm_srli_epi16(avg, 1);
  const __m128i A1 = _mm_sub_epi16(A0, B0);
  const __m128i BgtA = _mm_cmpgt_epi16(B0, A0);
  const __m128i A2 = _mm_sub_epi16(A1, BgtA);
  const __m128i A3 = _mm_srai_epi16(A2, 1);
  const __m128i A4 = _mm_add_epi16(A0, A3);
  const __m128i A5 = _mm_packus_epi16(A4, A4);
  return (uint32_t)_mm_cvtsi128_si32(A5);
}

static WEBP_INLINE uint32_t Select_SSE2(uint32_t a, uint32_t b, uint32_t c) {
  int pa_minus_pb;
  const __m128i zero = _mm_setzero_si128();
  const __m128i A0 = _mm_cvtsi32_si128((int)a);
  const __m128i B0 = _mm_cvtsi32_si128((int)b);
  const __m128i C0 = _mm_cvtsi32_si128((int)c);
  const __m128i AC0 = _mm_subs_epu8(A0, C0);
  const __m128i CA0 = _mm_subs_epu8(C0, A0);
  const __m128i BC0 = _mm_subs_epu8(B0, C0);
  const __m128i CB0 = _mm_subs_epu8(C0, B0);
  const __m128i AC = _mm_or_si128(AC0, CA0);
  const __m128i BC = _mm_or_si128(BC0, CB0);
  const __m128i pa = _mm_unpacklo_epi8(AC, zero);  // |a - c|
  const __m128i pb = _mm_unpacklo_epi8(BC, zero);  // |b - c|
  const __m128i diff = _mm_sub_epi16(pb, pa);
  {
    int16_t out[8];
    _mm_storeu_si128((__m128i*)out, diff);
    pa_minus_pb = out[0] + out[1] + out[2] + out[3];
  }
  return (pa_minus_pb <= 0) ? a : b;
}

static WEBP_INLINE void Average2_m128i(const __m128i* const a0,
                                       const __m128i* const a1,
                                       __m128i* const avg) {
  // (a + b) >> 1 = ((a + b + 1) >> 1) - ((a ^ b) & 1)
  const __m128i ones = _mm_set1_epi8(1);
  const __m128i avg1 = _mm_avg_epu8(*a0, *a1);
  const __m128i one = _mm_and_si128(_mm_xor_si128(*a0, *a1), ones);
  *avg = _mm_sub_epi8(avg1, one);
}

static WEBP_INLINE void Average2_uint32_SSE2(const uint32_t a0,
                                             const uint32_t a1,
                                             __m128i* const avg) {
  // (a + b) >> 1 = ((a + b + 1) >> 1) - ((a ^ b) & 1)
  const __m128i ones = _mm_set1_epi8(1);
  const __m128i A0 = _mm_cvtsi32_si128((int)a0);
  const __m128i A1 = _mm_cvtsi32_si128((int)a1);
  const __m128i avg1 = _mm_avg_epu8(A0, A1);
  const __m128i one = _mm_and_si128(_mm_xor_si128(A0, A1), ones);
  *avg = _mm_sub_epi8(avg1, one);
}

static WEBP_INLINE __m128i Average2_uint32_16_SSE2(uint32_t a0, uint32_t a1) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i A0 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)a0), zero);
  const __m128i A1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)a1), zero);
  const __m128i sum = _mm_add_epi16(A1, A0);
  return _mm_srli_epi16(sum, 1);
}

static WEBP_INLINE uint32_t Average2_SSE2(uint32_t a0, uint32_t a1) {
  __m128i output;
  Average2_uint32_SSE2(a0, a1, &output);
  return (uint32_t)_mm_cvtsi128_si32(output);
}

static WEBP_INLINE uint32_t Average3_SSE2(uint32_t a0, uint32_t a1,
                                          uint32_t a2) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i avg1 = Average2_uint32_16_SSE2(a0, a2);
  const __m128i A1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)a1), zero);
  const __m128i sum = _mm_add_epi16(avg1, A1);
  const __m128i avg2 = _mm_srli_epi16(sum, 1);
  const __m128i A2 = _mm_packus_epi16(avg2, avg2);
  return (uint32_t)_mm_cvtsi128_si32(A2);
}

static WEBP_INLINE uint32_t Average4_SSE2(uint32_t a0, uint32_t a1,
                                          uint32_t a2, uint32_t a3) {
  const __m128i avg1 = Average2_uint32_16_SSE2(a0, a1);
  const __m128i avg2 = Average2_uint32_16_SSE2(a2, a3);
  const __m128i sum = _mm_add_epi16(avg2, avg1);
  const __m128i avg3 = _mm_srli_epi16(sum, 1);
  const __m128i A0 = _mm_packus_epi16(avg3, avg3);
  return (uint32_t)_mm_cvtsi128_si32(A0);
}

static uint32_t Predictor5_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average3_SSE2(*left, top[0], top[1]);
  return pred;
}
static uint32_t Predictor6_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average2_SSE2(*left, top[-1]);
  return pred;
}
static uint32_t Predictor7_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average2_SSE2(*left, top[0]);
  return pred;
}
static uint32_t Predictor8_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average2_SSE2(top[-1], top[0]);
  (void)left;
  return pred;
}
static uint32_t Predictor9_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average2_SSE2(top[0], top[1]);
  (void)left;
  return pred;
}
static uint32_t Predictor10_SSE2(const uint32_t* const left,
                                 const uint32_t* const top) {
  const uint32_t pred = Average4_SSE2(*left, top[-1], top[0], top[1]);
  return pred;
}
static uint32_t Predictor11_SSE2(const uint32_t* const left,
                                 const uint32_t* const top) {
  const uint32_t pred = Select_SSE2(top[0], *left, top[-1]);
  return pred;
}
static uint32_t Predictor12_SSE2(const uint32_t* const left,
                                 const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractFull_SSE2(*left, top[0], top[-1]);
  return pred;
}
static uint32_t Predictor13_SSE2(const uint32_t* const left,
                                 const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractHalf_SSE2(*left, top[0], top[-1]);
  return pred;
}

// Batch versions of those functions.

// Predictor0: ARGB_BLACK.
static void PredictorAdd0_SSE2(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m128i black = _mm_set1_epi32((int)ARGB_BLACK);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    const __m128i res = _mm_add_epi8(src, black);
    _mm_storeu_si128((__m128i*)&out[i], res);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[0](in + i, NULL, num_pixels - i, out + i);
  }
  (void)upper;
}

// Predictor1: left.
static void PredictorAdd1_SSE2(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  __m128i prev = _mm_set1_epi32((int)out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    // a | b | c | d
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    // 0 | a | b | c
    const __m128i shift0 = _mm_slli_si128(src, 4);
    // a | a + b | b + c | c + d
    const __m128i sum0 = _mm_add_epi8(src, shift0);
    // 0 | 0 | a | a + b
    const __m128i shift1 = _mm_slli_si128(sum0, 8);
    // a | a + b | a + b + c | a + b + c + d
    const __m128i sum1 = _mm_add_epi8(sum0, shift1);
    const __m128i res = _mm_add_epi8(sum1, prev);
    _mm_storeu_si128((__m128i*)&out[i], res);
    // replicate prev output on the four lanes
    prev = _mm_shuffle_epi32(res, (3 << 0) | (3 << 2) | (3 << 4) | (3 << 6));
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[1](in + i, upper + i, num_pixels - i, out + i);
  }
}

// Macro that adds 32-bit integers from IN using mod 256 arithmetic
// per 8 bit channel.
#define GENERATE_PREDICTOR_1(X, IN)                                           \
static void PredictorAdd##X##_SSE2(const uint32_t* in, const uint32_t* upper, \
                                   int num_pixels,                            \
                                   uint32_t* WEBP_RESTRICT out) {             \
  int i;                                                                      \
  for (i = 0; i + 4 <= num_pixels; i += 4) {                                  \
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);              \
    const __m128i other = _mm_loadu_si128((const __m128i*)&(IN));             \
    const __m128i res = _mm_add_epi8(src, other);                             \
    _mm_storeu_si128((__m128i*)&out[i], res);                                 \
  }                                                                           \
  if (i != num_pixels) {                                                      \
    VP8LPredictorsAdd_C[(X)](in + i, upper + i, num_pixels - i, out + i);     \
  }                                                                           \
}

// Predictor2: Top.
GENERATE_PREDICTOR_1(2, upper[i])
// Predictor3: Top-right.
GENERATE_PREDICTOR_1(3, upper[i + 1])
// Predictor4: Top-left.
GENERATE_PREDICTOR_1(4, upper[i - 1])
#undef GENERATE_PREDICTOR_1

// Due to averages with integers, values cannot be accumulated in parallel for
// predictors 5 to 7.
GENERATE_PREDICTOR_ADD(Predictor5_SSE2, PredictorAdd5_SSE2)
GENERATE_PREDICTOR_ADD(Predictor6_SSE2, PredictorAdd6_SSE2)
GENERATE_PREDICTOR_ADD(Predictor7_SSE2, PredictorAdd7_SSE2)

#define GENERATE_PREDICTOR_2(X, IN)                                           \
static void PredictorAdd##X##_SSE2(const uint32_t* in, const uint32_t* upper, \
                                   int num_pixels,                            \
                                   uint32_t* WEBP_RESTRICT out) {             \
  int i;                                                                      \
  for (i = 0; i + 4 <= num_pixels; i += 4) {                                  \
    const __m128i Tother = _mm_loadu_si128((const __m128i*)&(IN));            \
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);             \
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);              \
    __m128i avg, res;                                                         \
    Average2_m128i(&T, &Tother, &avg);                                        \
    res = _mm_add_epi8(avg, src);                                             \
    _mm_storeu_si128((__m128i*)&out[i], res);                                 \
  }                                                                           \
  if (i != num_pixels) {                                                      \
    VP8LPredictorsAdd_C[(X)](in + i, upper + i, num_pixels - i, out + i);     \
  }                                                                           \
}
// Predictor8: average TL T.
GENERATE_PREDICTOR_2(8, upper[i - 1])
// Predictor9: average T TR.
GENERATE_PREDICTOR_2(9, upper[i + 1])
#undef GENERATE_PREDICTOR_2

// Predictor10: average of (average of (L,TL), average of (T, TR)).
#define DO_PRED10(OUT) do {                         \
  __m128i avgLTL, avg;                              \
  Average2_m128i(&L, &TL, &avgLTL);                 \
  Average2_m128i(&avgTTR, &avgLTL, &avg);           \
  L = _mm_add_epi8(avg, src);                       \
  out[i + (OUT)] = (uint32_t)_mm_cvtsi128_si32(L);  \
} while (0)

#define DO_PRED10_SHIFT do {                                  \
  /* Rotate the pre-computed values for the next iteration.*/ \
  avgTTR = _mm_srli_si128(avgTTR, 4);                         \
  TL = _mm_srli_si128(TL, 4);                                 \
  src = _mm_srli_si128(src, 4);                               \
} while (0)

static void PredictorAdd10_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  __m128i L = _mm_cvtsi32_si128((int)out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i TR = _mm_loadu_si128((const __m128i*)&upper[i + 1]);
    __m128i avgTTR;
    Average2_m128i(&T, &TR, &avgTTR);
    DO_PRED10(0);
    DO_PRED10_SHIFT;
    DO_PRED10(1);
    DO_PRED10_SHIFT;
    DO_PRED10(2);
    DO_PRED10_SHIFT;
    DO_PRED10(3);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[10](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED10
#undef DO_PRED10_SHIFT

// Predictor11: select.
#define DO_PRED11(OUT) do {                                            \
  const __m128i L_lo = _mm_unpacklo_epi32(L, T);                       \
  const __m128i TL_lo = _mm_unpacklo_epi32(TL, T);                     \
  const __m128i pb = _mm_sad_epu8(L_lo, TL_lo); /* pb = sum |L-TL|*/   \
  const __m128i mask = _mm_cmpgt_epi32(pb, pa);                        \
  const __m128i A = _mm_and_si128(mask, L);                            \
  const __m128i B = _mm_andnot_si128(mask, T);                         \
  const __m128i pred = _mm_or_si128(A, B); /* pred = (pa > b)? L : T*/ \
  L = _mm_add_epi8(src, pred);                                         \
  out[i + (OUT)] = (uint32_t)_mm_cvtsi128_si32(L);                     \
} while (0)

#define DO_PRED11_SHIFT do {                                \
  /* Shift the pre-computed value for the next iteration.*/ \
  T = _mm_srli_si128(T, 4);                                 \
  TL = _mm_srli_si128(TL, 4);                               \
  src = _mm_srli_si128(src, 4);                             \
  pa = _mm_srli_si128(pa, 4);                               \
} while (0)

static void PredictorAdd11_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  __m128i pa;
  __m128i L = _mm_cvtsi32_si128((int)out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    {
      // We can unpack with any value on the upper 32 bits, provided it's the
      // same on both operands (so that their sum of abs diff is zero). Here we
      // use T.
      const __m128i T_lo = _mm_unpacklo_epi32(T, T);
      const __m128i TL_lo = _mm_unpacklo_epi32(TL, T);
      const __m128i T_hi = _mm_unpackhi_epi32(T, T);
      const __m128i TL_hi = _mm_unpackhi_epi32(TL, T);
      const __m128i s_lo = _mm_sad_epu8(T_lo, TL_lo);
      const __m128i s_hi = _mm_sad_epu8(T_hi, TL_hi);
      pa = _mm_packs_epi32(s_lo, s_hi);  // pa = sum |T-TL|
    }
    DO_PRED11(0);
    DO_PRED11_SHIFT;
    DO_PRED11(1);
    DO_PRED11_SHIFT;
    DO_PRED11(2);
    DO_PRED11_SHIFT;
    DO_PRED11(3);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[11](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED11
#undef DO_PRED11_SHIFT

// Predictor12: ClampedAddSubtractFull.
#define DO_PRED12(DIFF, LANE, OUT) do {              \
  const __m128i all = _mm_add_epi16(L, (DIFF));      \
  const __m128i alls = _mm_packus_epi16(all, all);   \
  const __m128i res = _mm_add_epi8(src, alls);       \
  out[i + (OUT)] = (uint32_t)_mm_cvtsi128_si32(res); \
  L = _mm_unpacklo_epi8(res, zero);                  \
} while (0)

#define DO_PRED12_SHIFT(DIFF, LANE) do {                    \
  /* Shift the pre-computed value for the next iteration.*/ \
  if ((LANE) == 0) (DIFF) = _mm_srli_si128((DIFF), 8);      \
  src = _mm_srli_si128(src, 4);                             \
} while (0)

static void PredictorAdd12_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m128i zero = _mm_setzero_si128();
  const __m128i L8 = _mm_cvtsi32_si128((int)out[-1]);
  __m128i L = _mm_unpacklo_epi8(L8, zero);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    // Load 4 pixels at a time.
    __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i T_lo = _mm_unpacklo_epi8(T, zero);
    const __m128i T_hi = _mm_unpackhi_epi8(T, zero);
    const __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    const __m128i TL_lo = _mm_unpacklo_epi8(TL, zero);
    const __m128i TL_hi = _mm_unpackhi_epi8(TL, zero);
    __m128i diff_lo = _mm_sub_epi16(T_lo, TL_lo);
    __m128i diff_hi = _mm_sub_epi16(T_hi, TL_hi);
    DO_PRED12(diff_lo, 0, 0);
    DO_PRED12_SHIFT(diff_lo, 0);
    DO_PRED12(diff_lo, 1, 1);
    DO_PRED12_SHIFT(diff_lo, 1);
    DO_PRED12(diff_hi, 0, 2);
    DO_PRED12_SHIFT(diff_hi, 0);
    DO_PRED12(diff_hi, 1, 3);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[12](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED12
#undef DO_PRED12_SHIFT

// Due to averages with integers, values cannot be accumulated in parallel for
// predictors 13.
GENERATE_PREDICTOR_ADD(Predictor13_SSE2, PredictorAdd13_SSE2)

//------------------------------------------------------------------------------
// Subtract-Green Transform

static void AddGreenToBlueAndRed_SSE2(const uint32_t* const src, int num_pixels,
                                      uint32_t* dst) {
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((const __m128i*)&src[i]); // argb
    const __m128i A = _mm_srli_epi16(in, 8);     // 0 a 0 g
    const __m128i B = _mm_shufflelo_epi16(A, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128i C = _mm_shufflehi_epi16(B, _MM_SHUFFLE(2, 2, 0, 0));  // 0g0g
    const __m128i out = _mm_add_epi8(in, C);
    _mm_storeu_si128((__m128i*)&dst[i], out);
  }
  // fallthrough and finish off with plain-C
  if (i != num_pixels) {
    VP8LAddGreenToBlueAndRed_C(src + i, num_pixels - i, dst + i);
  }
}

//------------------------------------------------------------------------------
// Color Transform

static void TransformColorInverse_SSE2(const VP8LMultipliers* const m,
                                       const uint32_t* const src,
                                       int num_pixels, uint32_t* dst) {
// sign-extended multiplying constants, pre-shifted by 5.
#define CST(X)  (((int16_t)(m->X << 8)) >> 5)   // sign-extend
#define MK_CST_16(HI, LO) \
  _mm_set1_epi32((int)(((uint32_t)(HI) << 16) | ((LO) & 0xffff)))
  const __m128i mults_rb = MK_CST_16(CST(green_to_red_), CST(green_to_blue_));
  const __m128i mults_b2 = MK_CST_16(CST(red_to_blue_), 0);
#undef MK_CST_16
#undef CST
  const __m128i mask_ag = _mm_set1_epi32((int)0xff00ff00);  // alpha-green masks
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((const __m128i*)&src[i]); // argb
    const __m128i A = _mm_and_si128(in, mask_ag);     // a   0   g   0
    const __m128i B = _mm_shufflelo_epi16(A, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128i C = _mm_shufflehi_epi16(B, _MM_SHUFFLE(2, 2, 0, 0));  // g0g0
    const __m128i D = _mm_mulhi_epi16(C, mults_rb);    // x dr  x db1
    const __m128i E = _mm_add_epi8(in, D);             // x r'  x   b'
    const __m128i F = _mm_slli_epi16(E, 8);            // r' 0   b' 0
    const __m128i G = _mm_mulhi_epi16(F, mults_b2);    // x db2  0  0
    const __m128i H = _mm_srli_epi32(G, 8);            // 0  x db2  0
    const __m128i I = _mm_add_epi8(H, F);              // r' x  b'' 0
    const __m128i J = _mm_srli_epi16(I, 8);            // 0  r'  0  b''
    const __m128i out = _mm_or_si128(J, A);
    _mm_storeu_si128((__m128i*)&dst[i], out);
  }
  // Fall-back to C-version for left-overs.
  if (i != num_pixels) {
    VP8LTransformColorInverse_C(m, src + i, num_pixels - i, dst + i);
  }
}

//------------------------------------------------------------------------------
// Color-space conversion functions

static void ConvertBGRAToRGB_SSE2(const uint32_t* WEBP_RESTRICT src,
                                  int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;

  while (num_pixels >= 32) {
    // Load the BGRA buffers.
    __m128i in0 = _mm_loadu_si128(in + 0);
    __m128i in1 = _mm_loadu_si128(in + 1);
    __m128i in2 = _mm_loadu_si128(in + 2);
    __m128i in3 = _mm_loadu_si128(in + 3);
    __m128i in4 = _mm_loadu_si128(in + 4);
    __m128i in5 = _mm_loadu_si128(in + 5);
    __m128i in6 = _mm_loadu_si128(in + 6);
    __m128i in7 = _mm_loadu_si128(in + 7);
    VP8L32bToPlanar_SSE2(&in0, &in1, &in2, &in3);
    VP8L32bToPlanar_SSE2(&in4, &in5, &in6, &in7);
    // At this points, in1/in5 contains red only, in2/in6 green only ...
    // Pack the colors in 24b RGB.
    VP8PlanarTo24b_SSE2(&in1, &in5, &in2, &in6, &in3, &in7);
    _mm_storeu_si128(out + 0, in1);
    _mm_storeu_si128(out + 1, in5);
    _mm_storeu_si128(out + 2, in2);
    _mm_storeu_si128(out + 3, in6);
    _mm_storeu_si128(out + 4, in3);
    _mm_storeu_si128(out + 5, in7);
    in += 8;
    out += 6;
    num_pixels -= 32;
  }
  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToRGB_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToRGBA_SSE2(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i red_blue_mask = _mm_set1_epi32(0x00ff00ff);
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  while (num_pixels >= 8) {
    const __m128i A1 = _mm_loadu_si128(in++);
    const __m128i A2 = _mm_loadu_si128(in++);
    const __m128i B1 = _mm_and_si128(A1, red_blue_mask);     // R 0 B 0
    const __m128i B2 = _mm_and_si128(A2, red_blue_mask);     // R 0 B 0
    const __m128i C1 = _mm_andnot_si128(red_blue_mask, A1);  // 0 G 0 A
    const __m128i C2 = _mm_andnot_si128(red_blue_mask, A2);  // 0 G 0 A
    const __m128i D1 = _mm_shufflelo_epi16(B1, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128i D2 = _mm_shufflelo_epi16(B2, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128i E1 = _mm_shufflehi_epi16(D1, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128i E2 = _mm_shufflehi_epi16(D2, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128i F1 = _mm_or_si128(E1, C1);
    const __m128i F2 = _mm_or_si128(E2, C2);
    _mm_storeu_si128(out++, F1);
    _mm_storeu_si128(out++, F2);
    num_pixels -= 8;
  }
  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToRGBA_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToRGBA4444_SSE2(const uint32_t* WEBP_RESTRICT src,
                                       int num_pixels,
                                       uint8_t* WEBP_RESTRICT dst) {
  const __m128i mask_0x0f = _mm_set1_epi8(0x0f);
  const __m128i mask_0xf0 = _mm_set1_epi8((char)0xf0);
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  while (num_pixels >= 8) {
    const __m128i bgra0 = _mm_loadu_si128(in++);     // bgra0|bgra1|bgra2|bgra3
    const __m128i bgra4 = _mm_loadu_si128(in++);     // bgra4|bgra5|bgra6|bgra7
    const __m128i v0l = _mm_unpacklo_epi8(bgra0, bgra4);  // b0b4g0g4r0r4a0a4...
    const __m128i v0h = _mm_unpackhi_epi8(bgra0, bgra4);  // b2b6g2g6r2r6a2a6...
    const __m128i v1l = _mm_unpacklo_epi8(v0l, v0h);    // b0b2b4b6g0g2g4g6...
    const __m128i v1h = _mm_unpackhi_epi8(v0l, v0h);    // b1b3b5b7g1g3g5g7...
    const __m128i v2l = _mm_unpacklo_epi8(v1l, v1h);    // b0...b7 | g0...g7
    const __m128i v2h = _mm_unpackhi_epi8(v1l, v1h);    // r0...r7 | a0...a7
    const __m128i ga0 = _mm_unpackhi_epi64(v2l, v2h);   // g0...g7 | a0...a7
    const __m128i rb0 = _mm_unpacklo_epi64(v2h, v2l);   // r0...r7 | b0...b7
    const __m128i ga1 = _mm_srli_epi16(ga0, 4);         // g0-|g1-|...|a6-|a7-
    const __m128i rb1 = _mm_and_si128(rb0, mask_0xf0);  // -r0|-r1|...|-b6|-a7
    const __m128i ga2 = _mm_and_si128(ga1, mask_0x0f);  // g0-|g1-|...|a6-|a7-
    const __m128i rgba0 = _mm_or_si128(ga2, rb1);       // rg0..rg7 | ba0..ba7
    const __m128i rgba1 = _mm_srli_si128(rgba0, 8);     // ba0..ba7 | 0
#if (WEBP_SWAP_16BIT_CSP == 1)
    const __m128i rgba = _mm_unpacklo_epi8(rgba1, rgba0);  // barg0...barg7
#else
    const __m128i rgba = _mm_unpacklo_epi8(rgba0, rgba1);  // rgba0...rgba7
#endif
    _mm_storeu_si128(out++, rgba);
    num_pixels -= 8;
  }
  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToRGBA4444_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToRGB565_SSE2(const uint32_t* WEBP_RESTRICT src,
                                     int num_pixels,
                                     uint8_t* WEBP_RESTRICT dst) {
  const __m128i mask_0xe0 = _mm_set1_epi8((char)0xe0);
  const __m128i mask_0xf8 = _mm_set1_epi8((char)0xf8);
  const __m128i mask_0x07 = _mm_set1_epi8(0x07);
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  while (num_pixels >= 8) {
    const __m128i bgra0 = _mm_loadu_si128(in++);     // bgra0|bgra1|bgra2|bgra3
    const __m128i bgra4 = _mm_loadu_si128(in++);     // bgra4|bgra5|bgra6|bgra7
    const __m128i v0l = _mm_unpacklo_epi8(bgra0, bgra4);  // b0b4g0g4r0r4a0a4...
    const __m128i v0h = _mm_unpackhi_epi8(bgra0, bgra4);  // b2b6g2g6r2r6a2a6...
    const __m128i v1l = _mm_unpacklo_epi8(v0l, v0h);      // b0b2b4b6g0g2g4g6...
    const __m128i v1h = _mm_unpackhi_epi8(v0l, v0h);      // b1b3b5b7g1g3g5g7...
    const __m128i v2l = _mm_unpacklo_epi8(v1l, v1h);      // b0...b7 | g0...g7
    const __m128i v2h = _mm_unpackhi_epi8(v1l, v1h);      // r0...r7 | a0...a7
    const __m128i ga0 = _mm_unpackhi_epi64(v2l, v2h);     // g0...g7 | a0...a7
    const __m128i rb0 = _mm_unpacklo_epi64(v2h, v2l);     // r0...r7 | b0...b7
    const __m128i rb1 = _mm_and_si128(rb0, mask_0xf8);    // -r0..-r7|-b0..-b7
    const __m128i g_lo1 = _mm_srli_epi16(ga0, 5);
    const __m128i g_lo2 = _mm_and_si128(g_lo1, mask_0x07);  // g0-...g7-|xx (3b)
    const __m128i g_hi1 = _mm_slli_epi16(ga0, 3);
    const __m128i g_hi2 = _mm_and_si128(g_hi1, mask_0xe0);  // -g0...-g7|xx (3b)
    const __m128i b0 = _mm_srli_si128(rb1, 8);              // -b0...-b7|0
    const __m128i rg1 = _mm_or_si128(rb1, g_lo2);           // gr0...gr7|xx
    const __m128i b1 = _mm_srli_epi16(b0, 3);
    const __m128i gb1 = _mm_or_si128(b1, g_hi2);            // bg0...bg7|xx
#if (WEBP_SWAP_16BIT_CSP == 1)
    const __m128i rgba = _mm_unpacklo_epi8(gb1, rg1);     // rggb0...rggb7
#else
    const __m128i rgba = _mm_unpacklo_epi8(rg1, gb1);     // bgrb0...bgrb7
#endif
    _mm_storeu_si128(out++, rgba);
    num_pixels -= 8;
  }
  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToRGB565_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToBGR_SSE2(const uint32_t* WEBP_RESTRICT src,
                                  int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i mask_l = _mm_set_epi32(0, 0x00ffffff, 0, 0x00ffffff);
  const __m128i mask_h = _mm_set_epi32(0x00ffffff, 0, 0x00ffffff, 0);
  const __m128i* in = (const __m128i*)src;
  const uint8_t* const end = dst + num_pixels * 3;
  // the last storel_epi64 below writes 8 bytes starting at offset 18
  while (dst + 26 <= end) {
    const __m128i bgra0 = _mm_loadu_si128(in++);     // bgra0|bgra1|bgra2|bgra3
    const __m128i bgra4 = _mm_loadu_si128(in++);     // bgra4|bgra5|bgra6|bgra7
    const __m128i a0l = _mm_and_si128(bgra0, mask_l);   // bgr0|0|bgr0|0
    const __m128i a4l = _mm_and_si128(bgra4, mask_l);   // bgr0|0|bgr0|0
    const __m128i a0h = _mm_and_si128(bgra0, mask_h);   // 0|bgr0|0|bgr0
    const __m128i a4h = _mm_and_si128(bgra4, mask_h);   // 0|bgr0|0|bgr0
    const __m128i b0h = _mm_srli_epi64(a0h, 8);         // 000b|gr00|000b|gr00
    const __m128i b4h = _mm_srli_epi64(a4h, 8);         // 000b|gr00|000b|gr00
    const __m128i c0 = _mm_or_si128(a0l, b0h);          // rgbrgb00|rgbrgb00
    const __m128i c4 = _mm_or_si128(a4l, b4h);          // rgbrgb00|rgbrgb00
    const __m128i c2 = _mm_srli_si128(c0, 8);
    const __m128i c6 = _mm_srli_si128(c4, 8);
    _mm_storel_epi64((__m128i*)(dst +   0), c0);
    _mm_storel_epi64((__m128i*)(dst +   6), c2);
    _mm_storel_epi64((__m128i*)(dst +  12), c4);
    _mm_storel_epi64((__m128i*)(dst +  18), c6);
    dst += 24;
    num_pixels -= 8;
  }
  // left-overs
  if (num_pixels > 0) {
    VP8LConvertBGRAToBGR_C((const uint32_t*)in, num_pixels, dst);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8LDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitSSE2(void) {
  VP8LPredictors[5] = Predictor5_SSE2;
  VP8LPredictors[6] = Predictor6_SSE2;
  VP8LPredictors[7] = Predictor7_SSE2;
  VP8LPredictors[8] = Predictor8_SSE2;
  VP8LPredictors[9] = Predictor9_SSE2;
  VP8LPredictors[10] = Predictor10_SSE2;
  VP8LPredictors[11] = Predictor11_SSE2;
  VP8LPredictors[12] = Predictor12_SSE2;
  VP8LPredictors[13] = Predictor13_SSE2;

  VP8LPredictorsAdd[0] = PredictorAdd0_SSE2;
  VP8LPredictorsAdd[1] = PredictorAdd1_SSE2;
  VP8LPredictorsAdd[2] = PredictorAdd2_SSE2;
  VP8LPredictorsAdd[3] = PredictorAdd3_SSE2;
  VP8LPredictorsAdd[4] = PredictorAdd4_SSE2;
  VP8LPredictorsAdd[5] = PredictorAdd5_SSE2;
  VP8LPredictorsAdd[6] = PredictorAdd6_SSE2;
  VP8LPredictorsAdd[7] = PredictorAdd7_SSE2;
  VP8LPredictorsAdd[8] = PredictorAdd8_SSE2;
  VP8LPredictorsAdd[9] = PredictorAdd9_SSE2;
  VP8LPredictorsAdd[10] = PredictorAdd10_SSE2;
  VP8LPredictorsAdd[11] = PredictorAdd11_SSE2;
  VP8LPredictorsAdd[12] = PredictorAdd12_SSE2;
  VP8LPredictorsAdd[13] = PredictorAdd13_SSE2;

  VP8LAddGreenToBlueAndRed = AddGreenToBlueAndRed_SSE2;
  VP8LTransformColorInverse = TransformColorInverse_SSE2;

  VP8LConvertBGRAToRGB = ConvertBGRAToRGB_SSE2;
  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA_SSE2;
  VP8LConvertBGRAToRGBA4444 = ConvertBGRAToRGBA4444_SSE2;
  VP8LConvertBGRAToRGB565 = ConvertBGRAToRGB565_SSE2;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR_SSE2;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(VP8LDspInitSSE2)

#endif  // WEBP_USE_SSE2
