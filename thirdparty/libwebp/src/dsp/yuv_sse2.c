// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// YUV->RGB conversion functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/yuv.h"

#if defined(WEBP_USE_SSE2)

#include <stdlib.h>
#include <emmintrin.h>

#include "src/dsp/common_sse2.h"
#include "src/utils/utils.h"

//-----------------------------------------------------------------------------
// Convert spans of 32 pixels to various RGB formats for the fancy upsampler.

// These constants are 14b fixed-point version of ITU-R BT.601 constants.
// R = (19077 * y             + 26149 * v - 14234) >> 6
// G = (19077 * y -  6419 * u - 13320 * v +  8708) >> 6
// B = (19077 * y + 33050 * u             - 17685) >> 6
static void ConvertYUV444ToRGB_SSE2(const __m128i* const Y0,
                                    const __m128i* const U0,
                                    const __m128i* const V0,
                                    __m128i* const R,
                                    __m128i* const G,
                                    __m128i* const B) {
  const __m128i k19077 = _mm_set1_epi16(19077);
  const __m128i k26149 = _mm_set1_epi16(26149);
  const __m128i k14234 = _mm_set1_epi16(14234);
  // 33050 doesn't fit in a signed short: only use this with unsigned arithmetic
  const __m128i k33050 = _mm_set1_epi16((short)33050);
  const __m128i k17685 = _mm_set1_epi16(17685);
  const __m128i k6419  = _mm_set1_epi16(6419);
  const __m128i k13320 = _mm_set1_epi16(13320);
  const __m128i k8708  = _mm_set1_epi16(8708);

  const __m128i Y1 = _mm_mulhi_epu16(*Y0, k19077);

  const __m128i R0 = _mm_mulhi_epu16(*V0, k26149);
  const __m128i R1 = _mm_sub_epi16(Y1, k14234);
  const __m128i R2 = _mm_add_epi16(R1, R0);

  const __m128i G0 = _mm_mulhi_epu16(*U0, k6419);
  const __m128i G1 = _mm_mulhi_epu16(*V0, k13320);
  const __m128i G2 = _mm_add_epi16(Y1, k8708);
  const __m128i G3 = _mm_add_epi16(G0, G1);
  const __m128i G4 = _mm_sub_epi16(G2, G3);

  // be careful with the saturated *unsigned* arithmetic here!
  const __m128i B0 = _mm_mulhi_epu16(*U0, k33050);
  const __m128i B1 = _mm_adds_epu16(B0, Y1);
  const __m128i B2 = _mm_subs_epu16(B1, k17685);

  // use logical shift for B2, which can be larger than 32767
  *R = _mm_srai_epi16(R2, 6);   // range: [-14234, 30815]
  *G = _mm_srai_epi16(G4, 6);   // range: [-10953, 27710]
  *B = _mm_srli_epi16(B2, 6);   // range: [0, 34238]
}

// Load the bytes into the *upper* part of 16b words. That's "<< 8", basically.
static WEBP_INLINE __m128i Load_HI_16_SSE2(const uint8_t* src) {
  const __m128i zero = _mm_setzero_si128();
  return _mm_unpacklo_epi8(zero, _mm_loadl_epi64((const __m128i*)src));
}

// Load and replicate the U/V samples
static WEBP_INLINE __m128i Load_UV_HI_8_SSE2(const uint8_t* src) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i tmp0 = _mm_cvtsi32_si128(WebPMemToInt32(src));
  const __m128i tmp1 = _mm_unpacklo_epi8(zero, tmp0);
  return _mm_unpacklo_epi16(tmp1, tmp1);   // replicate samples
}

// Convert 32 samples of YUV444 to R/G/B
static void YUV444ToRGB_SSE2(const uint8_t* WEBP_RESTRICT const y,
                             const uint8_t* WEBP_RESTRICT const u,
                             const uint8_t* WEBP_RESTRICT const v,
                             __m128i* const R, __m128i* const G,
                             __m128i* const B) {
  const __m128i Y0 = Load_HI_16_SSE2(y), U0 = Load_HI_16_SSE2(u),
                V0 = Load_HI_16_SSE2(v);
  ConvertYUV444ToRGB_SSE2(&Y0, &U0, &V0, R, G, B);
}

// Convert 32 samples of YUV420 to R/G/B
static void YUV420ToRGB_SSE2(const uint8_t* WEBP_RESTRICT const y,
                             const uint8_t* WEBP_RESTRICT const u,
                             const uint8_t* WEBP_RESTRICT const v,
                             __m128i* const R, __m128i* const G,
                             __m128i* const B) {
  const __m128i Y0 = Load_HI_16_SSE2(y), U0 = Load_UV_HI_8_SSE2(u),
                V0 = Load_UV_HI_8_SSE2(v);
  ConvertYUV444ToRGB_SSE2(&Y0, &U0, &V0, R, G, B);
}

// Pack R/G/B/A results into 32b output.
static WEBP_INLINE void PackAndStore4_SSE2(const __m128i* const R,
                                           const __m128i* const G,
                                           const __m128i* const B,
                                           const __m128i* const A,
                                           uint8_t* WEBP_RESTRICT const dst) {
  const __m128i rb = _mm_packus_epi16(*R, *B);
  const __m128i ga = _mm_packus_epi16(*G, *A);
  const __m128i rg = _mm_unpacklo_epi8(rb, ga);
  const __m128i ba = _mm_unpackhi_epi8(rb, ga);
  const __m128i RGBA_lo = _mm_unpacklo_epi16(rg, ba);
  const __m128i RGBA_hi = _mm_unpackhi_epi16(rg, ba);
  _mm_storeu_si128((__m128i*)(dst +  0), RGBA_lo);
  _mm_storeu_si128((__m128i*)(dst + 16), RGBA_hi);
}

// Pack R/G/B/A results into 16b output.
static WEBP_INLINE void PackAndStore4444_SSE2(
     const __m128i* const R, const __m128i* const G, const __m128i* const B,
     const __m128i* const A, uint8_t* WEBP_RESTRICT const dst) {
#if (WEBP_SWAP_16BIT_CSP == 0)
  const __m128i rg0 = _mm_packus_epi16(*R, *G);
  const __m128i ba0 = _mm_packus_epi16(*B, *A);
#else
  const __m128i rg0 = _mm_packus_epi16(*B, *A);
  const __m128i ba0 = _mm_packus_epi16(*R, *G);
#endif
  const __m128i mask_0xf0 = _mm_set1_epi8((char)0xf0);
  const __m128i rb1 = _mm_unpacklo_epi8(rg0, ba0);  // rbrbrbrbrb...
  const __m128i ga1 = _mm_unpackhi_epi8(rg0, ba0);  // gagagagaga...
  const __m128i rb2 = _mm_and_si128(rb1, mask_0xf0);
  const __m128i ga2 = _mm_srli_epi16(_mm_and_si128(ga1, mask_0xf0), 4);
  const __m128i rgba4444 = _mm_or_si128(rb2, ga2);
  _mm_storeu_si128((__m128i*)dst, rgba4444);
}

// Pack R/G/B results into 16b output.
static WEBP_INLINE void PackAndStore565_SSE2(const __m128i* const R,
                                             const __m128i* const G,
                                             const __m128i* const B,
                                             uint8_t* WEBP_RESTRICT const dst) {
  const __m128i r0 = _mm_packus_epi16(*R, *R);
  const __m128i g0 = _mm_packus_epi16(*G, *G);
  const __m128i b0 = _mm_packus_epi16(*B, *B);
  const __m128i r1 = _mm_and_si128(r0, _mm_set1_epi8((char)0xf8));
  const __m128i b1 = _mm_and_si128(_mm_srli_epi16(b0, 3), _mm_set1_epi8(0x1f));
  const __m128i g1 =
      _mm_srli_epi16(_mm_and_si128(g0, _mm_set1_epi8((char)0xe0)), 5);
  const __m128i g2 = _mm_slli_epi16(_mm_and_si128(g0, _mm_set1_epi8(0x1c)), 3);
  const __m128i rg = _mm_or_si128(r1, g1);
  const __m128i gb = _mm_or_si128(g2, b1);
#if (WEBP_SWAP_16BIT_CSP == 0)
  const __m128i rgb565 = _mm_unpacklo_epi8(rg, gb);
#else
  const __m128i rgb565 = _mm_unpacklo_epi8(gb, rg);
#endif
  _mm_storeu_si128((__m128i*)dst, rgb565);
}

// Pack the planar buffers
// rrrr... rrrr... gggg... gggg... bbbb... bbbb....
// triplet by triplet in the output buffer rgb as rgbrgbrgbrgb ...
static WEBP_INLINE void PlanarTo24b_SSE2(__m128i* const in0, __m128i* const in1,
                                         __m128i* const in2, __m128i* const in3,
                                         __m128i* const in4, __m128i* const in5,
                                         uint8_t* WEBP_RESTRICT const rgb) {
  // The input is 6 registers of sixteen 8b but for the sake of explanation,
  // let's take 6 registers of four 8b values.
  // To pack, we will keep taking one every two 8b integer and move it
  // around as follows:
  // Input:
  //   r0r1r2r3 | r4r5r6r7 | g0g1g2g3 | g4g5g6g7 | b0b1b2b3 | b4b5b6b7
  // Split the 6 registers in two sets of 3 registers: the first set as the even
  // 8b bytes, the second the odd ones:
  //   r0r2r4r6 | g0g2g4g6 | b0b2b4b6 | r1r3r5r7 | g1g3g5g7 | b1b3b5b7
  // Repeat the same permutations twice more:
  //   r0r4g0g4 | b0b4r1r5 | g1g5b1b5 | r2r6g2g6 | b2b6r3r7 | g3g7b3b7
  //   r0g0b0r1 | g1b1r2g2 | b2r3g3b3 | r4g4b4r5 | g5b5r6g6 | b6r7g7b7
  VP8PlanarTo24b_SSE2(in0, in1, in2, in3, in4, in5);

  _mm_storeu_si128((__m128i*)(rgb +  0), *in0);
  _mm_storeu_si128((__m128i*)(rgb + 16), *in1);
  _mm_storeu_si128((__m128i*)(rgb + 32), *in2);
  _mm_storeu_si128((__m128i*)(rgb + 48), *in3);
  _mm_storeu_si128((__m128i*)(rgb + 64), *in4);
  _mm_storeu_si128((__m128i*)(rgb + 80), *in5);
}

void VP8YuvToRgba32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n < 32; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore4_SSE2(&R, &G, &B, &kAlpha, dst);
  }
}

void VP8YuvToBgra32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n < 32; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore4_SSE2(&B, &G, &R, &kAlpha, dst);
  }
}

void VP8YuvToArgb32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n < 32; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore4_SSE2(&kAlpha, &R, &G, &B, dst);
  }
}

void VP8YuvToRgba444432_SSE2(const uint8_t* WEBP_RESTRICT y,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v,
                             uint8_t* WEBP_RESTRICT dst) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n < 32; n += 8, dst += 16) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore4444_SSE2(&R, &G, &B, &kAlpha, dst);
  }
}

void VP8YuvToRgb56532_SSE2(const uint8_t* WEBP_RESTRICT y,
                           const uint8_t* WEBP_RESTRICT u,
                           const uint8_t* WEBP_RESTRICT v,
                           uint8_t* WEBP_RESTRICT dst) {
  int n;
  for (n = 0; n < 32; n += 8, dst += 16) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore565_SSE2(&R, &G, &B, dst);
  }
}

void VP8YuvToRgb32_SSE2(const uint8_t* WEBP_RESTRICT y,
                        const uint8_t* WEBP_RESTRICT u,
                        const uint8_t* WEBP_RESTRICT v,
                        uint8_t* WEBP_RESTRICT dst) {
  __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
  __m128i rgb0, rgb1, rgb2, rgb3, rgb4, rgb5;

  YUV444ToRGB_SSE2(y + 0, u + 0, v + 0, &R0, &G0, &B0);
  YUV444ToRGB_SSE2(y + 8, u + 8, v + 8, &R1, &G1, &B1);
  YUV444ToRGB_SSE2(y + 16, u + 16, v + 16, &R2, &G2, &B2);
  YUV444ToRGB_SSE2(y + 24, u + 24, v + 24, &R3, &G3, &B3);

  // Cast to 8b and store as RRRRGGGGBBBB.
  rgb0 = _mm_packus_epi16(R0, R1);
  rgb1 = _mm_packus_epi16(R2, R3);
  rgb2 = _mm_packus_epi16(G0, G1);
  rgb3 = _mm_packus_epi16(G2, G3);
  rgb4 = _mm_packus_epi16(B0, B1);
  rgb5 = _mm_packus_epi16(B2, B3);

  // Pack as RGBRGBRGBRGB.
  PlanarTo24b_SSE2(&rgb0, &rgb1, &rgb2, &rgb3, &rgb4, &rgb5, dst);
}

void VP8YuvToBgr32_SSE2(const uint8_t* WEBP_RESTRICT y,
                        const uint8_t* WEBP_RESTRICT u,
                        const uint8_t* WEBP_RESTRICT v,
                        uint8_t* WEBP_RESTRICT dst) {
  __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
  __m128i bgr0, bgr1, bgr2, bgr3, bgr4, bgr5;

  YUV444ToRGB_SSE2(y +  0, u +  0, v +  0, &R0, &G0, &B0);
  YUV444ToRGB_SSE2(y +  8, u +  8, v +  8, &R1, &G1, &B1);
  YUV444ToRGB_SSE2(y + 16, u + 16, v + 16, &R2, &G2, &B2);
  YUV444ToRGB_SSE2(y + 24, u + 24, v + 24, &R3, &G3, &B3);

  // Cast to 8b and store as BBBBGGGGRRRR.
  bgr0 = _mm_packus_epi16(B0, B1);
  bgr1 = _mm_packus_epi16(B2, B3);
  bgr2 = _mm_packus_epi16(G0, G1);
  bgr3 = _mm_packus_epi16(G2, G3);
  bgr4 = _mm_packus_epi16(R0, R1);
  bgr5= _mm_packus_epi16(R2, R3);

  // Pack as BGRBGRBGRBGR.
  PlanarTo24b_SSE2(&bgr0, &bgr1, &bgr2, &bgr3, &bgr4, &bgr5, dst);
}

//-----------------------------------------------------------------------------
// Arbitrary-length row conversion functions

static void YuvToRgbaRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int len) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n + 8 <= len; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV420ToRGB_SSE2(y, u, v, &R, &G, &B);
    PackAndStore4_SSE2(&R, &G, &B, &kAlpha, dst);
    y += 8;
    u += 4;
    v += 4;
  }
  for (; n < len; ++n) {   // Finish off
    VP8YuvToRgba(y[0], u[0], v[0], dst);
    dst += 4;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToBgraRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int len) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n + 8 <= len; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV420ToRGB_SSE2(y, u, v, &R, &G, &B);
    PackAndStore4_SSE2(&B, &G, &R, &kAlpha, dst);
    y += 8;
    u += 4;
    v += 4;
  }
  for (; n < len; ++n) {   // Finish off
    VP8YuvToBgra(y[0], u[0], v[0], dst);
    dst += 4;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToArgbRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int len) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n + 8 <= len; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV420ToRGB_SSE2(y, u, v, &R, &G, &B);
    PackAndStore4_SSE2(&kAlpha, &R, &G, &B, dst);
    y += 8;
    u += 4;
    v += 4;
  }
  for (; n < len; ++n) {   // Finish off
    VP8YuvToArgb(y[0], u[0], v[0], dst);
    dst += 4;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToRgbRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v,
                             uint8_t* WEBP_RESTRICT dst, int len) {
  int n;
  for (n = 0; n + 32 <= len; n += 32, dst += 32 * 3) {
    __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
    __m128i rgb0, rgb1, rgb2, rgb3, rgb4, rgb5;

    YUV420ToRGB_SSE2(y +  0, u +  0, v +  0, &R0, &G0, &B0);
    YUV420ToRGB_SSE2(y +  8, u +  4, v +  4, &R1, &G1, &B1);
    YUV420ToRGB_SSE2(y + 16, u +  8, v +  8, &R2, &G2, &B2);
    YUV420ToRGB_SSE2(y + 24, u + 12, v + 12, &R3, &G3, &B3);

    // Cast to 8b and store as RRRRGGGGBBBB.
    rgb0 = _mm_packus_epi16(R0, R1);
    rgb1 = _mm_packus_epi16(R2, R3);
    rgb2 = _mm_packus_epi16(G0, G1);
    rgb3 = _mm_packus_epi16(G2, G3);
    rgb4 = _mm_packus_epi16(B0, B1);
    rgb5 = _mm_packus_epi16(B2, B3);

    // Pack as RGBRGBRGBRGB.
    PlanarTo24b_SSE2(&rgb0, &rgb1, &rgb2, &rgb3, &rgb4, &rgb5, dst);

    y += 32;
    u += 16;
    v += 16;
  }
  for (; n < len; ++n) {   // Finish off
    VP8YuvToRgb(y[0], u[0], v[0], dst);
    dst += 3;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToBgrRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v,
                             uint8_t* WEBP_RESTRICT dst, int len) {
  int n;
  for (n = 0; n + 32 <= len; n += 32, dst += 32 * 3) {
    __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
    __m128i bgr0, bgr1, bgr2, bgr3, bgr4, bgr5;

    YUV420ToRGB_SSE2(y +  0, u +  0, v +  0, &R0, &G0, &B0);
    YUV420ToRGB_SSE2(y +  8, u +  4, v +  4, &R1, &G1, &B1);
    YUV420ToRGB_SSE2(y + 16, u +  8, v +  8, &R2, &G2, &B2);
    YUV420ToRGB_SSE2(y + 24, u + 12, v + 12, &R3, &G3, &B3);

    // Cast to 8b and store as BBBBGGGGRRRR.
    bgr0 = _mm_packus_epi16(B0, B1);
    bgr1 = _mm_packus_epi16(B2, B3);
    bgr2 = _mm_packus_epi16(G0, G1);
    bgr3 = _mm_packus_epi16(G2, G3);
    bgr4 = _mm_packus_epi16(R0, R1);
    bgr5 = _mm_packus_epi16(R2, R3);

    // Pack as BGRBGRBGRBGR.
    PlanarTo24b_SSE2(&bgr0, &bgr1, &bgr2, &bgr3, &bgr4, &bgr5, dst);

    y += 32;
    u += 16;
    v += 16;
  }
  for (; n < len; ++n) {   // Finish off
    VP8YuvToBgr(y[0], u[0], v[0], dst);
    dst += 3;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void WebPInitSamplersSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitSamplersSSE2(void) {
  WebPSamplers[MODE_RGB]  = YuvToRgbRow_SSE2;
  WebPSamplers[MODE_RGBA] = YuvToRgbaRow_SSE2;
  WebPSamplers[MODE_BGR]  = YuvToBgrRow_SSE2;
  WebPSamplers[MODE_BGRA] = YuvToBgraRow_SSE2;
  WebPSamplers[MODE_ARGB] = YuvToArgbRow_SSE2;
}

//------------------------------------------------------------------------------
// RGB24/32 -> YUV converters

// Load eight 16b-words from *src.
#define LOAD_16(src) _mm_loadu_si128((const __m128i*)(src))
// Store either 16b-words into *dst
#define STORE_16(V, dst) _mm_storeu_si128((__m128i*)(dst), (V))

// Function that inserts a value of the second half of the in buffer in between
// every two char of the first half.
static WEBP_INLINE void RGB24PackedToPlanarHelper_SSE2(
    const __m128i* const in /*in[6]*/, __m128i* const out /*out[6]*/) {
  out[0] = _mm_unpacklo_epi8(in[0], in[3]);
  out[1] = _mm_unpackhi_epi8(in[0], in[3]);
  out[2] = _mm_unpacklo_epi8(in[1], in[4]);
  out[3] = _mm_unpackhi_epi8(in[1], in[4]);
  out[4] = _mm_unpacklo_epi8(in[2], in[5]);
  out[5] = _mm_unpackhi_epi8(in[2], in[5]);
}

// Unpack the 8b input rgbrgbrgbrgb ... as contiguous registers:
// rrrr... rrrr... gggg... gggg... bbbb... bbbb....
// Similar to PlanarTo24bHelper(), but in reverse order.
static WEBP_INLINE void RGB24PackedToPlanar_SSE2(
    const uint8_t* WEBP_RESTRICT const rgb, __m128i* const out /*out[6]*/) {
  __m128i tmp[6];
  tmp[0] = _mm_loadu_si128((const __m128i*)(rgb +  0));
  tmp[1] = _mm_loadu_si128((const __m128i*)(rgb + 16));
  tmp[2] = _mm_loadu_si128((const __m128i*)(rgb + 32));
  tmp[3] = _mm_loadu_si128((const __m128i*)(rgb + 48));
  tmp[4] = _mm_loadu_si128((const __m128i*)(rgb + 64));
  tmp[5] = _mm_loadu_si128((const __m128i*)(rgb + 80));

  RGB24PackedToPlanarHelper_SSE2(tmp, out);
  RGB24PackedToPlanarHelper_SSE2(out, tmp);
  RGB24PackedToPlanarHelper_SSE2(tmp, out);
  RGB24PackedToPlanarHelper_SSE2(out, tmp);
  RGB24PackedToPlanarHelper_SSE2(tmp, out);
}

// Convert 8 packed ARGB to r[], g[], b[]
static WEBP_INLINE void RGB32PackedToPlanar_SSE2(
    const uint32_t* WEBP_RESTRICT const argb, __m128i* const rgb /*in[6]*/) {
  const __m128i zero = _mm_setzero_si128();
  __m128i a0 = LOAD_16(argb + 0);
  __m128i a1 = LOAD_16(argb + 4);
  __m128i a2 = LOAD_16(argb + 8);
  __m128i a3 = LOAD_16(argb + 12);
  VP8L32bToPlanar_SSE2(&a0, &a1, &a2, &a3);
  rgb[0] = _mm_unpacklo_epi8(a1, zero);
  rgb[1] = _mm_unpackhi_epi8(a1, zero);
  rgb[2] = _mm_unpacklo_epi8(a2, zero);
  rgb[3] = _mm_unpackhi_epi8(a2, zero);
  rgb[4] = _mm_unpacklo_epi8(a3, zero);
  rgb[5] = _mm_unpackhi_epi8(a3, zero);
}

// This macro computes (RG * MULT_RG + GB * MULT_GB + ROUNDER) >> DESCALE_FIX
// It's a macro and not a function because we need to use immediate values with
// srai_epi32, e.g.
#define TRANSFORM(RG_LO, RG_HI, GB_LO, GB_HI, MULT_RG, MULT_GB, \
                  ROUNDER, DESCALE_FIX, OUT) do {               \
  const __m128i V0_lo = _mm_madd_epi16(RG_LO, MULT_RG);         \
  const __m128i V0_hi = _mm_madd_epi16(RG_HI, MULT_RG);         \
  const __m128i V1_lo = _mm_madd_epi16(GB_LO, MULT_GB);         \
  const __m128i V1_hi = _mm_madd_epi16(GB_HI, MULT_GB);         \
  const __m128i V2_lo = _mm_add_epi32(V0_lo, V1_lo);            \
  const __m128i V2_hi = _mm_add_epi32(V0_hi, V1_hi);            \
  const __m128i V3_lo = _mm_add_epi32(V2_lo, ROUNDER);          \
  const __m128i V3_hi = _mm_add_epi32(V2_hi, ROUNDER);          \
  const __m128i V5_lo = _mm_srai_epi32(V3_lo, DESCALE_FIX);     \
  const __m128i V5_hi = _mm_srai_epi32(V3_hi, DESCALE_FIX);     \
  (OUT) = _mm_packs_epi32(V5_lo, V5_hi);                        \
} while (0)

#define MK_CST_16(A, B) _mm_set_epi16((B), (A), (B), (A), (B), (A), (B), (A))
static WEBP_INLINE void ConvertRGBToY_SSE2(const __m128i* const R,
                                           const __m128i* const G,
                                           const __m128i* const B,
                                           __m128i* const Y) {
  const __m128i kRG_y = MK_CST_16(16839, 33059 - 16384);
  const __m128i kGB_y = MK_CST_16(16384, 6420);
  const __m128i kHALF_Y = _mm_set1_epi32((16 << YUV_FIX) + YUV_HALF);

  const __m128i RG_lo = _mm_unpacklo_epi16(*R, *G);
  const __m128i RG_hi = _mm_unpackhi_epi16(*R, *G);
  const __m128i GB_lo = _mm_unpacklo_epi16(*G, *B);
  const __m128i GB_hi = _mm_unpackhi_epi16(*G, *B);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_y, kGB_y, kHALF_Y, YUV_FIX, *Y);
}

static WEBP_INLINE void ConvertRGBToUV_SSE2(const __m128i* const R,
                                            const __m128i* const G,
                                            const __m128i* const B,
                                            __m128i* const U,
                                            __m128i* const V) {
  const __m128i kRG_u = MK_CST_16(-9719, -19081);
  const __m128i kGB_u = MK_CST_16(0, 28800);
  const __m128i kRG_v = MK_CST_16(28800, 0);
  const __m128i kGB_v = MK_CST_16(-24116, -4684);
  const __m128i kHALF_UV = _mm_set1_epi32(((128 << YUV_FIX) + YUV_HALF) << 2);

  const __m128i RG_lo = _mm_unpacklo_epi16(*R, *G);
  const __m128i RG_hi = _mm_unpackhi_epi16(*R, *G);
  const __m128i GB_lo = _mm_unpacklo_epi16(*G, *B);
  const __m128i GB_hi = _mm_unpackhi_epi16(*G, *B);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_u, kGB_u,
            kHALF_UV, YUV_FIX + 2, *U);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_v, kGB_v,
            kHALF_UV, YUV_FIX + 2, *V);
}

#undef MK_CST_16
#undef TRANSFORM

static void ConvertRGB24ToY_SSE2(const uint8_t* WEBP_RESTRICT rgb,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~31;
  int i;
  for (i = 0; i < max_width; rgb += 3 * 16 * 2) {
    __m128i rgb_plane[6];
    int j;

    RGB24PackedToPlanar_SSE2(rgb, rgb_plane);

    for (j = 0; j < 2; ++j, i += 16) {
      const __m128i zero = _mm_setzero_si128();
      __m128i r, g, b, Y0, Y1;

      // Convert to 16-bit Y.
      r = _mm_unpacklo_epi8(rgb_plane[0 + j], zero);
      g = _mm_unpacklo_epi8(rgb_plane[2 + j], zero);
      b = _mm_unpacklo_epi8(rgb_plane[4 + j], zero);
      ConvertRGBToY_SSE2(&r, &g, &b, &Y0);

      // Convert to 16-bit Y.
      r = _mm_unpackhi_epi8(rgb_plane[0 + j], zero);
      g = _mm_unpackhi_epi8(rgb_plane[2 + j], zero);
      b = _mm_unpackhi_epi8(rgb_plane[4 + j], zero);
      ConvertRGBToY_SSE2(&r, &g, &b, &Y1);

      // Cast to 8-bit and store.
      STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
    }
  }
  for (; i < width; ++i, rgb += 3) {   // left-over
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGR24ToY_SSE2(const uint8_t* WEBP_RESTRICT bgr,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~31;
  int i;
  for (i = 0; i < max_width; bgr += 3 * 16 * 2) {
    __m128i bgr_plane[6];
    int j;

    RGB24PackedToPlanar_SSE2(bgr, bgr_plane);

    for (j = 0; j < 2; ++j, i += 16) {
      const __m128i zero = _mm_setzero_si128();
      __m128i r, g, b, Y0, Y1;

      // Convert to 16-bit Y.
      b = _mm_unpacklo_epi8(bgr_plane[0 + j], zero);
      g = _mm_unpacklo_epi8(bgr_plane[2 + j], zero);
      r = _mm_unpacklo_epi8(bgr_plane[4 + j], zero);
      ConvertRGBToY_SSE2(&r, &g, &b, &Y0);

      // Convert to 16-bit Y.
      b = _mm_unpackhi_epi8(bgr_plane[0 + j], zero);
      g = _mm_unpackhi_epi8(bgr_plane[2 + j], zero);
      r = _mm_unpackhi_epi8(bgr_plane[4 + j], zero);
      ConvertRGBToY_SSE2(&r, &g, &b, &Y1);

      // Cast to 8-bit and store.
      STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
    }
  }
  for (; i < width; ++i, bgr += 3) {  // left-over
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

static void ConvertARGBToY_SSE2(const uint32_t* WEBP_RESTRICT argb,
                                uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~15;
  int i;
  for (i = 0; i < max_width; i += 16) {
    __m128i Y0, Y1, rgb[6];
    RGB32PackedToPlanar_SSE2(&argb[i], rgb);
    ConvertRGBToY_SSE2(&rgb[0], &rgb[2], &rgb[4], &Y0);
    ConvertRGBToY_SSE2(&rgb[1], &rgb[3], &rgb[5], &Y1);
    STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
  }
  for (; i < width; ++i) {   // left-over
    const uint32_t p = argb[i];
    y[i] = VP8RGBToY((p >> 16) & 0xff, (p >> 8) & 0xff, (p >>  0) & 0xff,
                     YUV_HALF);
  }
}

// Horizontal add (doubled) of two 16b values, result is 16b.
// in: A | B | C | D | ... -> out: 2*(A+B) | 2*(C+D) | ...
static void HorizontalAddPack_SSE2(const __m128i* const A,
                                   const __m128i* const B,
                                   __m128i* const out) {
  const __m128i k2 = _mm_set1_epi16(2);
  const __m128i C = _mm_madd_epi16(*A, k2);
  const __m128i D = _mm_madd_epi16(*B, k2);
  *out = _mm_packs_epi32(C, D);
}

static void ConvertARGBToUV_SSE2(const uint32_t* WEBP_RESTRICT argb,
                                 uint8_t* WEBP_RESTRICT u,
                                 uint8_t* WEBP_RESTRICT v,
                                 int src_width, int do_store) {
  const int max_width = src_width & ~31;
  int i;
  for (i = 0; i < max_width; i += 32, u += 16, v += 16) {
    __m128i rgb[6], U0, V0, U1, V1;
    RGB32PackedToPlanar_SSE2(&argb[i], rgb);
    HorizontalAddPack_SSE2(&rgb[0], &rgb[1], &rgb[0]);
    HorizontalAddPack_SSE2(&rgb[2], &rgb[3], &rgb[2]);
    HorizontalAddPack_SSE2(&rgb[4], &rgb[5], &rgb[4]);
    ConvertRGBToUV_SSE2(&rgb[0], &rgb[2], &rgb[4], &U0, &V0);

    RGB32PackedToPlanar_SSE2(&argb[i + 16], rgb);
    HorizontalAddPack_SSE2(&rgb[0], &rgb[1], &rgb[0]);
    HorizontalAddPack_SSE2(&rgb[2], &rgb[3], &rgb[2]);
    HorizontalAddPack_SSE2(&rgb[4], &rgb[5], &rgb[4]);
    ConvertRGBToUV_SSE2(&rgb[0], &rgb[2], &rgb[4], &U1, &V1);

    U0 = _mm_packus_epi16(U0, U1);
    V0 = _mm_packus_epi16(V0, V1);
    if (!do_store) {
      const __m128i prev_u = LOAD_16(u);
      const __m128i prev_v = LOAD_16(v);
      U0 = _mm_avg_epu8(U0, prev_u);
      V0 = _mm_avg_epu8(V0, prev_v);
    }
    STORE_16(U0, u);
    STORE_16(V0, v);
  }
  if (i < src_width) {  // left-over
    WebPConvertARGBToUV_C(argb + i, u, v, src_width - i, do_store);
  }
}

// Convert 16 packed ARGB 16b-values to r[], g[], b[]
static WEBP_INLINE void RGBA32PackedToPlanar_16b_SSE2(
    const uint16_t* WEBP_RESTRICT const rgbx,
    __m128i* const r, __m128i* const g, __m128i* const b) {
  const __m128i in0 = LOAD_16(rgbx +  0);  // r0 | g0 | b0 |x| r1 | g1 | b1 |x
  const __m128i in1 = LOAD_16(rgbx +  8);  // r2 | g2 | b2 |x| r3 | g3 | b3 |x
  const __m128i in2 = LOAD_16(rgbx + 16);  // r4 | ...
  const __m128i in3 = LOAD_16(rgbx + 24);  // r6 | ...
  // column-wise transpose
  const __m128i A0 = _mm_unpacklo_epi16(in0, in1);
  const __m128i A1 = _mm_unpackhi_epi16(in0, in1);
  const __m128i A2 = _mm_unpacklo_epi16(in2, in3);
  const __m128i A3 = _mm_unpackhi_epi16(in2, in3);
  const __m128i B0 = _mm_unpacklo_epi16(A0, A1);  // r0 r1 r2 r3 | g0 g1 ..
  const __m128i B1 = _mm_unpackhi_epi16(A0, A1);  // b0 b1 b2 b3 | x x x x
  const __m128i B2 = _mm_unpacklo_epi16(A2, A3);  // r4 r5 r6 r7 | g4 g5 ..
  const __m128i B3 = _mm_unpackhi_epi16(A2, A3);  // b4 b5 b6 b7 | x x x x
  *r = _mm_unpacklo_epi64(B0, B2);
  *g = _mm_unpackhi_epi64(B0, B2);
  *b = _mm_unpacklo_epi64(B1, B3);
}

static void ConvertRGBA32ToUV_SSE2(const uint16_t* WEBP_RESTRICT rgb,
                                   uint8_t* WEBP_RESTRICT u,
                                   uint8_t* WEBP_RESTRICT v, int width) {
  const int max_width = width & ~15;
  const uint16_t* const last_rgb = rgb + 4 * max_width;
  while (rgb < last_rgb) {
    __m128i r, g, b, U0, V0, U1, V1;
    RGBA32PackedToPlanar_16b_SSE2(rgb +  0, &r, &g, &b);
    ConvertRGBToUV_SSE2(&r, &g, &b, &U0, &V0);
    RGBA32PackedToPlanar_16b_SSE2(rgb + 32, &r, &g, &b);
    ConvertRGBToUV_SSE2(&r, &g, &b, &U1, &V1);
    STORE_16(_mm_packus_epi16(U0, U1), u);
    STORE_16(_mm_packus_epi16(V0, V1), v);
    u += 16;
    v += 16;
    rgb += 2 * 32;
  }
  if (max_width < width) {  // left-over
    WebPConvertRGBA32ToUV_C(rgb, u, v, width - max_width);
  }
}

//------------------------------------------------------------------------------

extern void WebPInitConvertARGBToYUVSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitConvertARGBToYUVSSE2(void) {
  WebPConvertARGBToY = ConvertARGBToY_SSE2;
  WebPConvertARGBToUV = ConvertARGBToUV_SSE2;

  WebPConvertRGB24ToY = ConvertRGB24ToY_SSE2;
  WebPConvertBGR24ToY = ConvertBGR24ToY_SSE2;

  WebPConvertRGBA32ToUV = ConvertRGBA32ToUV_SSE2;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(WebPInitSamplersSSE2)
WEBP_DSP_INIT_STUB(WebPInitConvertARGBToYUVSSE2)

#endif  // WEBP_USE_SSE2
