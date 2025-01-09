// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Image transforms and color space conversion methods for lossless decoder.
//
// Authors: Vikas Arora (vikaas.arora@gmail.com)
//          Jyrki Alakuijala (jyrki@google.com)
//          Vincent Rabaud (vrabaud@google.com)

#ifndef WEBP_DSP_LOSSLESS_COMMON_H_
#define WEBP_DSP_LOSSLESS_COMMON_H_

#include "src/dsp/cpu.h"
#include "src/utils/utils.h"
#include "src/webp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Decoding

// color mapping related functions.
static WEBP_INLINE uint32_t VP8GetARGBIndex(uint32_t idx) {
  return (idx >> 8) & 0xff;
}

static WEBP_INLINE uint8_t VP8GetAlphaIndex(uint8_t idx) {
  return idx;
}

static WEBP_INLINE uint32_t VP8GetARGBValue(uint32_t val) {
  return val;
}

static WEBP_INLINE uint8_t VP8GetAlphaValue(uint32_t val) {
  return (val >> 8) & 0xff;
}

//------------------------------------------------------------------------------
// Misc methods.

// Computes sampled size of 'size' when sampling using 'sampling bits'.
static WEBP_INLINE uint32_t VP8LSubSampleSize(uint32_t size,
                                              uint32_t sampling_bits) {
  return (size + (1 << sampling_bits) - 1) >> sampling_bits;
}

// Converts near lossless quality into max number of bits shaved off.
static WEBP_INLINE int VP8LNearLosslessBits(int near_lossless_quality) {
  //    100 -> 0
  // 80..99 -> 1
  // 60..79 -> 2
  // 40..59 -> 3
  // 20..39 -> 4
  //  0..19 -> 5
  return 5 - near_lossless_quality / 20;
}

// -----------------------------------------------------------------------------
// Faster logarithm for integers. Small values use a look-up table.

// The threshold till approximate version of log_2 can be used.
// Practically, we can get rid of the call to log() as the two values match to
// very high degree (the ratio of these two is 0.99999x).
// Keeping a high threshold for now.
#define APPROX_LOG_WITH_CORRECTION_MAX  65536
#define APPROX_LOG_MAX                   4096
// VP8LFastLog2 and VP8LFastSLog2 are used on elements from image histograms.
// The histogram values cannot exceed the maximum number of pixels, which
// is (1 << 14) * (1 << 14). Therefore S * log(S) < (1 << 33).
// No more than 32 bits of precision should be chosen.
// To match the original float implementation, 23 bits of precision are used.
#define LOG_2_PRECISION_BITS 23
#define LOG_2_RECIPROCAL 1.44269504088896338700465094007086
// LOG_2_RECIPROCAL * (1 << LOG_2_PRECISION_BITS)
#define LOG_2_RECIPROCAL_FIXED_DOUBLE 12102203.161561485379934310913085937500
#define LOG_2_RECIPROCAL_FIXED ((uint64_t)12102203)
#define LOG_LOOKUP_IDX_MAX 256
extern const uint32_t kLog2Table[LOG_LOOKUP_IDX_MAX];
extern const uint64_t kSLog2Table[LOG_LOOKUP_IDX_MAX];
typedef uint32_t (*VP8LFastLog2SlowFunc)(uint32_t v);
typedef uint64_t (*VP8LFastSLog2SlowFunc)(uint32_t v);

extern VP8LFastLog2SlowFunc VP8LFastLog2Slow;
extern VP8LFastSLog2SlowFunc VP8LFastSLog2Slow;

static WEBP_INLINE uint32_t VP8LFastLog2(uint32_t v) {
  return (v < LOG_LOOKUP_IDX_MAX) ? kLog2Table[v] : VP8LFastLog2Slow(v);
}
// Fast calculation of v * log2(v) for integer input.
static WEBP_INLINE uint64_t VP8LFastSLog2(uint32_t v) {
  return (v < LOG_LOOKUP_IDX_MAX) ? kSLog2Table[v] : VP8LFastSLog2Slow(v);
}

static WEBP_INLINE uint64_t RightShiftRound(uint64_t v, uint32_t shift) {
  return (v + (1ull << shift >> 1)) >> shift;
}

static WEBP_INLINE int64_t DivRound(int64_t a, int64_t b) {
  return ((a < 0) == (b < 0)) ? ((a + b / 2) / b) : ((a - b / 2) / b);
}

#define WEBP_INT64_MAX ((int64_t)((1ull << 63) - 1))
#define WEBP_UINT64_MAX (~0ull)

// -----------------------------------------------------------------------------
// PrefixEncode()

// Splitting of distance and length codes into prefixes and
// extra bits. The prefixes are encoded with an entropy code
// while the extra bits are stored just as normal bits.
static WEBP_INLINE void VP8LPrefixEncodeBitsNoLUT(int distance, int* const code,
                                                  int* const extra_bits) {
  const int highest_bit = BitsLog2Floor(--distance);
  const int second_highest_bit = (distance >> (highest_bit - 1)) & 1;
  *extra_bits = highest_bit - 1;
  *code = 2 * highest_bit + second_highest_bit;
}

static WEBP_INLINE void VP8LPrefixEncodeNoLUT(int distance, int* const code,
                                              int* const extra_bits,
                                              int* const extra_bits_value) {
  const int highest_bit = BitsLog2Floor(--distance);
  const int second_highest_bit = (distance >> (highest_bit - 1)) & 1;
  *extra_bits = highest_bit - 1;
  *extra_bits_value = distance & ((1 << *extra_bits) - 1);
  *code = 2 * highest_bit + second_highest_bit;
}

#define PREFIX_LOOKUP_IDX_MAX   512
typedef struct {
  int8_t code_;
  int8_t extra_bits_;
} VP8LPrefixCode;

// These tables are derived using VP8LPrefixEncodeNoLUT.
extern const VP8LPrefixCode kPrefixEncodeCode[PREFIX_LOOKUP_IDX_MAX];
extern const uint8_t kPrefixEncodeExtraBitsValue[PREFIX_LOOKUP_IDX_MAX];
static WEBP_INLINE void VP8LPrefixEncodeBits(int distance, int* const code,
                                             int* const extra_bits) {
  if (distance < PREFIX_LOOKUP_IDX_MAX) {
    const VP8LPrefixCode prefix_code = kPrefixEncodeCode[distance];
    *code = prefix_code.code_;
    *extra_bits = prefix_code.extra_bits_;
  } else {
    VP8LPrefixEncodeBitsNoLUT(distance, code, extra_bits);
  }
}

static WEBP_INLINE void VP8LPrefixEncode(int distance, int* const code,
                                         int* const extra_bits,
                                         int* const extra_bits_value) {
  if (distance < PREFIX_LOOKUP_IDX_MAX) {
    const VP8LPrefixCode prefix_code = kPrefixEncodeCode[distance];
    *code = prefix_code.code_;
    *extra_bits = prefix_code.extra_bits_;
    *extra_bits_value = kPrefixEncodeExtraBitsValue[distance];
  } else {
    VP8LPrefixEncodeNoLUT(distance, code, extra_bits, extra_bits_value);
  }
}

// Sum of each component, mod 256.
static WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW WEBP_INLINE
uint32_t VP8LAddPixels(uint32_t a, uint32_t b) {
  const uint32_t alpha_and_green = (a & 0xff00ff00u) + (b & 0xff00ff00u);
  const uint32_t red_and_blue = (a & 0x00ff00ffu) + (b & 0x00ff00ffu);
  return (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

// Difference of each component, mod 256.
static WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW WEBP_INLINE
uint32_t VP8LSubPixels(uint32_t a, uint32_t b) {
  const uint32_t alpha_and_green =
      0x00ff00ffu + (a & 0xff00ff00u) - (b & 0xff00ff00u);
  const uint32_t red_and_blue =
      0xff00ff00u + (a & 0x00ff00ffu) - (b & 0x00ff00ffu);
  return (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

//------------------------------------------------------------------------------
// Transform-related functions used in both encoding and decoding.

// Macros used to create a batch predictor that iteratively uses a
// one-pixel predictor.

// The predictor is added to the output pixel (which
// is therefore considered as a residual) to get the final prediction.
#define GENERATE_PREDICTOR_ADD(PREDICTOR, PREDICTOR_ADD)                 \
static void PREDICTOR_ADD(const uint32_t* in, const uint32_t* upper,     \
                          int num_pixels, uint32_t* WEBP_RESTRICT out) { \
  int x;                                                                 \
  assert(upper != NULL);                                                 \
  for (x = 0; x < num_pixels; ++x) {                                     \
    const uint32_t pred = (PREDICTOR)(&out[x - 1], upper + x);           \
    out[x] = VP8LAddPixels(in[x], pred);                                 \
  }                                                                      \
}

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_DSP_LOSSLESS_COMMON_H_
