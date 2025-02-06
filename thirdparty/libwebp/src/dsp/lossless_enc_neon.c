// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// NEON variant of methods for lossless encoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_NEON)

#include <arm_neon.h>

#include "src/dsp/lossless.h"
#include "src/dsp/neon.h"

//------------------------------------------------------------------------------
// Subtract-Green Transform

// vtbl?_u8 are marked unavailable for iOS arm64 with Xcode < 6.3, use
// non-standard versions there.
#if defined(__APPLE__) && WEBP_AARCH64 && \
    defined(__apple_build_version__) && (__apple_build_version__< 6020037)
#define USE_VTBLQ
#endif

#ifdef USE_VTBLQ
// 255 = byte will be zeroed
static const uint8_t kGreenShuffle[16] = {
  1, 255, 1, 255, 5, 255, 5, 255, 9, 255, 9, 255, 13, 255, 13, 255
};

static WEBP_INLINE uint8x16_t DoGreenShuffle_NEON(const uint8x16_t argb,
                                                  const uint8x16_t shuffle) {
  return vcombine_u8(vtbl1q_u8(argb, vget_low_u8(shuffle)),
                     vtbl1q_u8(argb, vget_high_u8(shuffle)));
}
#else  // !USE_VTBLQ
// 255 = byte will be zeroed
static const uint8_t kGreenShuffle[8] = { 1, 255, 1, 255, 5, 255, 5, 255  };

static WEBP_INLINE uint8x16_t DoGreenShuffle_NEON(const uint8x16_t argb,
                                                  const uint8x8_t shuffle) {
  return vcombine_u8(vtbl1_u8(vget_low_u8(argb), shuffle),
                     vtbl1_u8(vget_high_u8(argb), shuffle));
}
#endif  // USE_VTBLQ

static void SubtractGreenFromBlueAndRed_NEON(uint32_t* argb_data,
                                             int num_pixels) {
  const uint32_t* const end = argb_data + (num_pixels & ~3);
#ifdef USE_VTBLQ
  const uint8x16_t shuffle = vld1q_u8(kGreenShuffle);
#else
  const uint8x8_t shuffle = vld1_u8(kGreenShuffle);
#endif
  for (; argb_data < end; argb_data += 4) {
    const uint8x16_t argb = vld1q_u8((uint8_t*)argb_data);
    const uint8x16_t greens = DoGreenShuffle_NEON(argb, shuffle);
    vst1q_u8((uint8_t*)argb_data, vsubq_u8(argb, greens));
  }
  // fallthrough and finish off with plain-C
  VP8LSubtractGreenFromBlueAndRed_C(argb_data, num_pixels & 3);
}

//------------------------------------------------------------------------------
// Color Transform

static void TransformColor_NEON(const VP8LMultipliers* WEBP_RESTRICT const m,
                                uint32_t* WEBP_RESTRICT argb_data,
                                int num_pixels) {
  // sign-extended multiplying constants, pre-shifted by 6.
#define CST(X)  (((int16_t)(m->X << 8)) >> 6)
  const int16_t rb[8] = {
    CST(green_to_blue_), CST(green_to_red_),
    CST(green_to_blue_), CST(green_to_red_),
    CST(green_to_blue_), CST(green_to_red_),
    CST(green_to_blue_), CST(green_to_red_)
  };
  const int16x8_t mults_rb = vld1q_s16(rb);
  const int16_t b2[8] = {
    0, CST(red_to_blue_), 0, CST(red_to_blue_),
    0, CST(red_to_blue_), 0, CST(red_to_blue_),
  };
  const int16x8_t mults_b2 = vld1q_s16(b2);
#undef CST
#ifdef USE_VTBLQ
  static const uint8_t kg0g0[16] = {
    255, 1, 255, 1, 255, 5, 255, 5, 255, 9, 255, 9, 255, 13, 255, 13
  };
  const uint8x16_t shuffle = vld1q_u8(kg0g0);
#else
  static const uint8_t k0g0g[8] = { 255, 1, 255, 1, 255, 5, 255, 5 };
  const uint8x8_t shuffle = vld1_u8(k0g0g);
#endif
  const uint32x4_t mask_rb = vdupq_n_u32(0x00ff00ffu);  // red-blue masks
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t in = vld1q_u8((uint8_t*)(argb_data + i));
    // 0 g 0 g
    const uint8x16_t greens = DoGreenShuffle_NEON(in, shuffle);
    // x dr  x db1
    const int16x8_t A = vqdmulhq_s16(vreinterpretq_s16_u8(greens), mults_rb);
    // r 0   b   0
    const int16x8_t B = vshlq_n_s16(vreinterpretq_s16_u8(in), 8);
    // x db2 0   0
    const int16x8_t C = vqdmulhq_s16(B, mults_b2);
    // 0 0   x db2
    const uint32x4_t D = vshrq_n_u32(vreinterpretq_u32_s16(C), 16);
    // x dr  x  db
    const int8x16_t E = vaddq_s8(vreinterpretq_s8_u32(D),
                                 vreinterpretq_s8_s16(A));
    // 0 dr  0  db
    const uint32x4_t F = vandq_u32(vreinterpretq_u32_s8(E), mask_rb);
    const int8x16_t out = vsubq_s8(vreinterpretq_s8_u8(in),
                                   vreinterpretq_s8_u32(F));
    vst1q_s8((int8_t*)(argb_data + i), out);
  }
  // fallthrough and finish off with plain-C
  VP8LTransformColor_C(m, argb_data + i, num_pixels - i);
}

#undef USE_VTBLQ

//------------------------------------------------------------------------------
// Entry point

extern void VP8LEncDspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LEncDspInitNEON(void) {
  VP8LSubtractGreenFromBlueAndRed = SubtractGreenFromBlueAndRed_NEON;
  VP8LTransformColor = TransformColor_NEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8LEncDspInitNEON)

#endif  // WEBP_USE_NEON
