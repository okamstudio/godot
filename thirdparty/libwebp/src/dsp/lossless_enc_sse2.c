// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE2 variant of methods for lossless encoder
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_SSE2)
#include <assert.h>
#include <emmintrin.h>
#include "src/dsp/lossless.h"
#include "src/dsp/common_sse2.h"
#include "src/dsp/lossless_common.h"

// For sign-extended multiplying constants, pre-shifted by 5:
#define CST_5b(X)  (((int16_t)((uint16_t)(X) << 8)) >> 5)

//------------------------------------------------------------------------------
// Subtract-Green Transform

static void SubtractGreenFromBlueAndRed_SSE2(uint32_t* argb_data,
                                             int num_pixels) {
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((__m128i*)&argb_data[i]); // argb
    const __m128i A = _mm_srli_epi16(in, 8);     // 0 a 0 g
    const __m128i B = _mm_shufflelo_epi16(A, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128i C = _mm_shufflehi_epi16(B, _MM_SHUFFLE(2, 2, 0, 0));  // 0g0g
    const __m128i out = _mm_sub_epi8(in, C);
    _mm_storeu_si128((__m128i*)&argb_data[i], out);
  }
  // fallthrough and finish off with plain-C
  if (i != num_pixels) {
    VP8LSubtractGreenFromBlueAndRed_C(argb_data + i, num_pixels - i);
  }
}

//------------------------------------------------------------------------------
// Color Transform

#define MK_CST_16(HI, LO) \
  _mm_set1_epi32((int)(((uint32_t)(HI) << 16) | ((LO) & 0xffff)))

static void TransformColor_SSE2(const VP8LMultipliers* WEBP_RESTRICT const m,
                                uint32_t* WEBP_RESTRICT argb_data,
                                int num_pixels) {
  const __m128i mults_rb = MK_CST_16(CST_5b(m->green_to_red_),
                                     CST_5b(m->green_to_blue_));
  const __m128i mults_b2 = MK_CST_16(CST_5b(m->red_to_blue_), 0);
  const __m128i mask_ag = _mm_set1_epi32((int)0xff00ff00);  // alpha-green masks
  const __m128i mask_rb = _mm_set1_epi32(0x00ff00ff);       // red-blue masks
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((__m128i*)&argb_data[i]); // argb
    const __m128i A = _mm_and_si128(in, mask_ag);     // a   0   g   0
    const __m128i B = _mm_shufflelo_epi16(A, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128i C = _mm_shufflehi_epi16(B, _MM_SHUFFLE(2, 2, 0, 0));  // g0g0
    const __m128i D = _mm_mulhi_epi16(C, mults_rb);    // x dr  x db1
    const __m128i E = _mm_slli_epi16(in, 8);           // r 0   b   0
    const __m128i F = _mm_mulhi_epi16(E, mults_b2);    // x db2 0   0
    const __m128i G = _mm_srli_epi32(F, 16);           // 0 0   x db2
    const __m128i H = _mm_add_epi8(G, D);              // x dr  x  db
    const __m128i I = _mm_and_si128(H, mask_rb);       // 0 dr  0  db
    const __m128i out = _mm_sub_epi8(in, I);
    _mm_storeu_si128((__m128i*)&argb_data[i], out);
  }
  // fallthrough and finish off with plain-C
  if (i != num_pixels) {
    VP8LTransformColor_C(m, argb_data + i, num_pixels - i);
  }
}

//------------------------------------------------------------------------------
#define SPAN 8
static void CollectColorBlueTransforms_SSE2(const uint32_t* WEBP_RESTRICT argb,
                                            int stride,
                                            int tile_width, int tile_height,
                                            int green_to_blue, int red_to_blue,
                                            uint32_t histo[]) {
  const __m128i mults_r = MK_CST_16(CST_5b(red_to_blue), 0);
  const __m128i mults_g = MK_CST_16(0, CST_5b(green_to_blue));
  const __m128i mask_g = _mm_set1_epi32(0x00ff00);  // green mask
  const __m128i mask_b = _mm_set1_epi32(0x0000ff);  // blue mask
  int y;
  for (y = 0; y < tile_height; ++y) {
    const uint32_t* const src = argb + y * stride;
    int i, x;
    for (x = 0; x + SPAN <= tile_width; x += SPAN) {
      uint16_t values[SPAN];
      const __m128i in0 = _mm_loadu_si128((__m128i*)&src[x +        0]);
      const __m128i in1 = _mm_loadu_si128((__m128i*)&src[x + SPAN / 2]);
      const __m128i A0 = _mm_slli_epi16(in0, 8);        // r 0  | b 0
      const __m128i A1 = _mm_slli_epi16(in1, 8);
      const __m128i B0 = _mm_and_si128(in0, mask_g);    // 0 0  | g 0
      const __m128i B1 = _mm_and_si128(in1, mask_g);
      const __m128i C0 = _mm_mulhi_epi16(A0, mults_r);  // x db | 0 0
      const __m128i C1 = _mm_mulhi_epi16(A1, mults_r);
      const __m128i D0 = _mm_mulhi_epi16(B0, mults_g);  // 0 0  | x db
      const __m128i D1 = _mm_mulhi_epi16(B1, mults_g);
      const __m128i E0 = _mm_sub_epi8(in0, D0);         // x x  | x b'
      const __m128i E1 = _mm_sub_epi8(in1, D1);
      const __m128i F0 = _mm_srli_epi32(C0, 16);        // 0 0  | x db
      const __m128i F1 = _mm_srli_epi32(C1, 16);
      const __m128i G0 = _mm_sub_epi8(E0, F0);          // 0 0  | x b'
      const __m128i G1 = _mm_sub_epi8(E1, F1);
      const __m128i H0 = _mm_and_si128(G0, mask_b);     // 0 0  | 0 b
      const __m128i H1 = _mm_and_si128(G1, mask_b);
      const __m128i I = _mm_packs_epi32(H0, H1);        // 0 b' | 0 b'
      _mm_storeu_si128((__m128i*)values, I);
      for (i = 0; i < SPAN; ++i) ++histo[values[i]];
    }
  }
  {
    const int left_over = tile_width & (SPAN - 1);
    if (left_over > 0) {
      VP8LCollectColorBlueTransforms_C(argb + tile_width - left_over, stride,
                                       left_over, tile_height,
                                       green_to_blue, red_to_blue, histo);
    }
  }
}

static void CollectColorRedTransforms_SSE2(const uint32_t* WEBP_RESTRICT argb,
                                           int stride,
                                           int tile_width, int tile_height,
                                           int green_to_red, uint32_t histo[]) {
  const __m128i mults_g = MK_CST_16(0, CST_5b(green_to_red));
  const __m128i mask_g = _mm_set1_epi32(0x00ff00);  // green mask
  const __m128i mask = _mm_set1_epi32(0xff);

  int y;
  for (y = 0; y < tile_height; ++y) {
    const uint32_t* const src = argb + y * stride;
    int i, x;
    for (x = 0; x + SPAN <= tile_width; x += SPAN) {
      uint16_t values[SPAN];
      const __m128i in0 = _mm_loadu_si128((__m128i*)&src[x +        0]);
      const __m128i in1 = _mm_loadu_si128((__m128i*)&src[x + SPAN / 2]);
      const __m128i A0 = _mm_and_si128(in0, mask_g);    // 0 0  | g 0
      const __m128i A1 = _mm_and_si128(in1, mask_g);
      const __m128i B0 = _mm_srli_epi32(in0, 16);       // 0 0  | x r
      const __m128i B1 = _mm_srli_epi32(in1, 16);
      const __m128i C0 = _mm_mulhi_epi16(A0, mults_g);  // 0 0  | x dr
      const __m128i C1 = _mm_mulhi_epi16(A1, mults_g);
      const __m128i E0 = _mm_sub_epi8(B0, C0);          // x x  | x r'
      const __m128i E1 = _mm_sub_epi8(B1, C1);
      const __m128i F0 = _mm_and_si128(E0, mask);       // 0 0  | 0 r'
      const __m128i F1 = _mm_and_si128(E1, mask);
      const __m128i I = _mm_packs_epi32(F0, F1);
      _mm_storeu_si128((__m128i*)values, I);
      for (i = 0; i < SPAN; ++i) ++histo[values[i]];
    }
  }
  {
    const int left_over = tile_width & (SPAN - 1);
    if (left_over > 0) {
      VP8LCollectColorRedTransforms_C(argb + tile_width - left_over, stride,
                                      left_over, tile_height,
                                      green_to_red, histo);
    }
  }
}
#undef SPAN
#undef MK_CST_16

//------------------------------------------------------------------------------

// Note we are adding uint32_t's as *signed* int32's (using _mm_add_epi32). But
// that's ok since the histogram values are less than 1<<28 (max picture size).
static void AddVector_SSE2(const uint32_t* WEBP_RESTRICT a,
                           const uint32_t* WEBP_RESTRICT b,
                           uint32_t* WEBP_RESTRICT out, int size) {
  int i = 0;
  int aligned_size = size & ~15;
  // Size is, at minimum, NUM_DISTANCE_CODES (40) and may be as large as
  // NUM_LITERAL_CODES (256) + NUM_LENGTH_CODES (24) + (0 or a non-zero power of
  // 2). See the usage in VP8LHistogramAdd().
  assert(size >= 16);
  assert(size % 2 == 0);

  do {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)&a[i +  0]);
    const __m128i a1 = _mm_loadu_si128((const __m128i*)&a[i +  4]);
    const __m128i a2 = _mm_loadu_si128((const __m128i*)&a[i +  8]);
    const __m128i a3 = _mm_loadu_si128((const __m128i*)&a[i + 12]);
    const __m128i b0 = _mm_loadu_si128((const __m128i*)&b[i +  0]);
    const __m128i b1 = _mm_loadu_si128((const __m128i*)&b[i +  4]);
    const __m128i b2 = _mm_loadu_si128((const __m128i*)&b[i +  8]);
    const __m128i b3 = _mm_loadu_si128((const __m128i*)&b[i + 12]);
    _mm_storeu_si128((__m128i*)&out[i +  0], _mm_add_epi32(a0, b0));
    _mm_storeu_si128((__m128i*)&out[i +  4], _mm_add_epi32(a1, b1));
    _mm_storeu_si128((__m128i*)&out[i +  8], _mm_add_epi32(a2, b2));
    _mm_storeu_si128((__m128i*)&out[i + 12], _mm_add_epi32(a3, b3));
    i += 16;
  } while (i != aligned_size);

  if ((size & 8) != 0) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)&a[i + 0]);
    const __m128i a1 = _mm_loadu_si128((const __m128i*)&a[i + 4]);
    const __m128i b0 = _mm_loadu_si128((const __m128i*)&b[i + 0]);
    const __m128i b1 = _mm_loadu_si128((const __m128i*)&b[i + 4]);
    _mm_storeu_si128((__m128i*)&out[i + 0], _mm_add_epi32(a0, b0));
    _mm_storeu_si128((__m128i*)&out[i + 4], _mm_add_epi32(a1, b1));
    i += 8;
  }

  size &= 7;
  if (size == 4) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)&a[i]);
    const __m128i b0 = _mm_loadu_si128((const __m128i*)&b[i]);
    _mm_storeu_si128((__m128i*)&out[i], _mm_add_epi32(a0, b0));
  } else if (size == 2) {
    const __m128i a0 = _mm_loadl_epi64((const __m128i*)&a[i]);
    const __m128i b0 = _mm_loadl_epi64((const __m128i*)&b[i]);
    _mm_storel_epi64((__m128i*)&out[i], _mm_add_epi32(a0, b0));
  }
}

static void AddVectorEq_SSE2(const uint32_t* WEBP_RESTRICT a,
                             uint32_t* WEBP_RESTRICT out, int size) {
  int i = 0;
  int aligned_size = size & ~15;
  // Size is, at minimum, NUM_DISTANCE_CODES (40) and may be as large as
  // NUM_LITERAL_CODES (256) + NUM_LENGTH_CODES (24) + (0 or a non-zero power of
  // 2). See the usage in VP8LHistogramAdd().
  assert(size >= 16);
  assert(size % 2 == 0);

  do {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)&a[i +  0]);
    const __m128i a1 = _mm_loadu_si128((const __m128i*)&a[i +  4]);
    const __m128i a2 = _mm_loadu_si128((const __m128i*)&a[i +  8]);
    const __m128i a3 = _mm_loadu_si128((const __m128i*)&a[i + 12]);
    const __m128i b0 = _mm_loadu_si128((const __m128i*)&out[i +  0]);
    const __m128i b1 = _mm_loadu_si128((const __m128i*)&out[i +  4]);
    const __m128i b2 = _mm_loadu_si128((const __m128i*)&out[i +  8]);
    const __m128i b3 = _mm_loadu_si128((const __m128i*)&out[i + 12]);
    _mm_storeu_si128((__m128i*)&out[i +  0], _mm_add_epi32(a0, b0));
    _mm_storeu_si128((__m128i*)&out[i +  4], _mm_add_epi32(a1, b1));
    _mm_storeu_si128((__m128i*)&out[i +  8], _mm_add_epi32(a2, b2));
    _mm_storeu_si128((__m128i*)&out[i + 12], _mm_add_epi32(a3, b3));
    i += 16;
  } while (i != aligned_size);

  if ((size & 8) != 0) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)&a[i + 0]);
    const __m128i a1 = _mm_loadu_si128((const __m128i*)&a[i + 4]);
    const __m128i b0 = _mm_loadu_si128((const __m128i*)&out[i + 0]);
    const __m128i b1 = _mm_loadu_si128((const __m128i*)&out[i + 4]);
    _mm_storeu_si128((__m128i*)&out[i + 0], _mm_add_epi32(a0, b0));
    _mm_storeu_si128((__m128i*)&out[i + 4], _mm_add_epi32(a1, b1));
    i += 8;
  }

  size &= 7;
  if (size == 4) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)&a[i]);
    const __m128i b0 = _mm_loadu_si128((const __m128i*)&out[i]);
    _mm_storeu_si128((__m128i*)&out[i], _mm_add_epi32(a0, b0));
  } else if (size == 2) {
    const __m128i a0 = _mm_loadl_epi64((const __m128i*)&a[i]);
    const __m128i b0 = _mm_loadl_epi64((const __m128i*)&out[i]);
    _mm_storel_epi64((__m128i*)&out[i], _mm_add_epi32(a0, b0));
  }
}

//------------------------------------------------------------------------------
// Entropy

#if !defined(WEBP_HAVE_SLOW_CLZ_CTZ)

static uint64_t CombinedShannonEntropy_SSE2(const uint32_t X[256],
                                            const uint32_t Y[256]) {
  int i;
  uint64_t retval = 0;
  uint32_t sumX = 0, sumXY = 0;
  const __m128i zero = _mm_setzero_si128();

  for (i = 0; i < 256; i += 16) {
    const __m128i x0 = _mm_loadu_si128((const __m128i*)(X + i +  0));
    const __m128i y0 = _mm_loadu_si128((const __m128i*)(Y + i +  0));
    const __m128i x1 = _mm_loadu_si128((const __m128i*)(X + i +  4));
    const __m128i y1 = _mm_loadu_si128((const __m128i*)(Y + i +  4));
    const __m128i x2 = _mm_loadu_si128((const __m128i*)(X + i +  8));
    const __m128i y2 = _mm_loadu_si128((const __m128i*)(Y + i +  8));
    const __m128i x3 = _mm_loadu_si128((const __m128i*)(X + i + 12));
    const __m128i y3 = _mm_loadu_si128((const __m128i*)(Y + i + 12));
    const __m128i x4 = _mm_packs_epi16(_mm_packs_epi32(x0, x1),
                                       _mm_packs_epi32(x2, x3));
    const __m128i y4 = _mm_packs_epi16(_mm_packs_epi32(y0, y1),
                                       _mm_packs_epi32(y2, y3));
    const int32_t mx = _mm_movemask_epi8(_mm_cmpgt_epi8(x4, zero));
    int32_t my = _mm_movemask_epi8(_mm_cmpgt_epi8(y4, zero)) | mx;
    while (my) {
      const int32_t j = BitsCtz(my);
      uint32_t xy;
      if ((mx >> j) & 1) {
        const int x = X[i + j];
        sumXY += x;
        retval += VP8LFastSLog2(x);
      }
      xy = X[i + j] + Y[i + j];
      sumX += xy;
      retval += VP8LFastSLog2(xy);
      my &= my - 1;
    }
  }
  retval = VP8LFastSLog2(sumX) + VP8LFastSLog2(sumXY) - retval;
  return retval;
}

#else

#define DONT_USE_COMBINED_SHANNON_ENTROPY_SSE2_FUNC   // won't be faster

#endif

//------------------------------------------------------------------------------

static int VectorMismatch_SSE2(const uint32_t* const array1,
                               const uint32_t* const array2, int length) {
  int match_len;

  if (length >= 12) {
    __m128i A0 = _mm_loadu_si128((const __m128i*)&array1[0]);
    __m128i A1 = _mm_loadu_si128((const __m128i*)&array2[0]);
    match_len = 0;
    do {
      // Loop unrolling and early load both provide a speedup of 10% for the
      // current function. Also, max_limit can be MAX_LENGTH=4096 at most.
      const __m128i cmpA = _mm_cmpeq_epi32(A0, A1);
      const __m128i B0 =
          _mm_loadu_si128((const __m128i*)&array1[match_len + 4]);
      const __m128i B1 =
          _mm_loadu_si128((const __m128i*)&array2[match_len + 4]);
      if (_mm_movemask_epi8(cmpA) != 0xffff) break;
      match_len += 4;

      {
        const __m128i cmpB = _mm_cmpeq_epi32(B0, B1);
        A0 = _mm_loadu_si128((const __m128i*)&array1[match_len + 4]);
        A1 = _mm_loadu_si128((const __m128i*)&array2[match_len + 4]);
        if (_mm_movemask_epi8(cmpB) != 0xffff) break;
        match_len += 4;
      }
    } while (match_len + 12 < length);
  } else {
    match_len = 0;
    // Unroll the potential first two loops.
    if (length >= 4 &&
        _mm_movemask_epi8(_mm_cmpeq_epi32(
            _mm_loadu_si128((const __m128i*)&array1[0]),
            _mm_loadu_si128((const __m128i*)&array2[0]))) == 0xffff) {
      match_len = 4;
      if (length >= 8 &&
          _mm_movemask_epi8(_mm_cmpeq_epi32(
              _mm_loadu_si128((const __m128i*)&array1[4]),
              _mm_loadu_si128((const __m128i*)&array2[4]))) == 0xffff) {
        match_len = 8;
      }
    }
  }

  while (match_len < length && array1[match_len] == array2[match_len]) {
    ++match_len;
  }
  return match_len;
}

// Bundles multiple (1, 2, 4 or 8) pixels into a single pixel.
static void BundleColorMap_SSE2(const uint8_t* WEBP_RESTRICT const row,
                                int width, int xbits,
                                uint32_t* WEBP_RESTRICT dst) {
  int x;
  assert(xbits >= 0);
  assert(xbits <= 3);
  switch (xbits) {
    case 0: {
      const __m128i ff = _mm_set1_epi16((short)0xff00);
      const __m128i zero = _mm_setzero_si128();
      // Store 0xff000000 | (row[x] << 8).
      for (x = 0; x + 16 <= width; x += 16, dst += 16) {
        const __m128i in = _mm_loadu_si128((const __m128i*)&row[x]);
        const __m128i in_lo = _mm_unpacklo_epi8(zero, in);
        const __m128i dst0 = _mm_unpacklo_epi16(in_lo, ff);
        const __m128i dst1 = _mm_unpackhi_epi16(in_lo, ff);
        const __m128i in_hi = _mm_unpackhi_epi8(zero, in);
        const __m128i dst2 = _mm_unpacklo_epi16(in_hi, ff);
        const __m128i dst3 = _mm_unpackhi_epi16(in_hi, ff);
        _mm_storeu_si128((__m128i*)&dst[0], dst0);
        _mm_storeu_si128((__m128i*)&dst[4], dst1);
        _mm_storeu_si128((__m128i*)&dst[8], dst2);
        _mm_storeu_si128((__m128i*)&dst[12], dst3);
      }
      break;
    }
    case 1: {
      const __m128i ff = _mm_set1_epi16((short)0xff00);
      const __m128i mul = _mm_set1_epi16(0x110);
      for (x = 0; x + 16 <= width; x += 16, dst += 8) {
        // 0a0b | (where a/b are 4 bits).
        const __m128i in = _mm_loadu_si128((const __m128i*)&row[x]);
        const __m128i tmp = _mm_mullo_epi16(in, mul);  // aba0
        const __m128i pack = _mm_and_si128(tmp, ff);   // ab00
        const __m128i dst0 = _mm_unpacklo_epi16(pack, ff);
        const __m128i dst1 = _mm_unpackhi_epi16(pack, ff);
        _mm_storeu_si128((__m128i*)&dst[0], dst0);
        _mm_storeu_si128((__m128i*)&dst[4], dst1);
      }
      break;
    }
    case 2: {
      const __m128i mask_or = _mm_set1_epi32((int)0xff000000);
      const __m128i mul_cst = _mm_set1_epi16(0x0104);
      const __m128i mask_mul = _mm_set1_epi16(0x0f00);
      for (x = 0; x + 16 <= width; x += 16, dst += 4) {
        // 000a000b000c000d | (where a/b/c/d are 2 bits).
        const __m128i in = _mm_loadu_si128((const __m128i*)&row[x]);
        const __m128i mul = _mm_mullo_epi16(in, mul_cst);  // 00ab00b000cd00d0
        const __m128i tmp = _mm_and_si128(mul, mask_mul);  // 00ab000000cd0000
        const __m128i shift = _mm_srli_epi32(tmp, 12);     // 00000000ab000000
        const __m128i pack = _mm_or_si128(shift, tmp);     // 00000000abcd0000
        // Convert to 0xff00**00.
        const __m128i res = _mm_or_si128(pack, mask_or);
        _mm_storeu_si128((__m128i*)dst, res);
      }
      break;
    }
    default: {
      assert(xbits == 3);
      for (x = 0; x + 16 <= width; x += 16, dst += 2) {
        // 0000000a00000000b... | (where a/b are 1 bit).
        const __m128i in = _mm_loadu_si128((const __m128i*)&row[x]);
        const __m128i shift = _mm_slli_epi64(in, 7);
        const uint32_t move = _mm_movemask_epi8(shift);
        dst[0] = 0xff000000 | ((move & 0xff) << 8);
        dst[1] = 0xff000000 | (move & 0xff00);
      }
      break;
    }
  }
  if (x != width) {
    VP8LBundleColorMap_C(row + x, width - x, xbits, dst);
  }
}

//------------------------------------------------------------------------------
// Batch version of Predictor Transform subtraction

static WEBP_INLINE void Average2_m128i(const __m128i* const a0,
                                       const __m128i* const a1,
                                       __m128i* const avg) {
  // (a + b) >> 1 = ((a + b + 1) >> 1) - ((a ^ b) & 1)
  const __m128i ones = _mm_set1_epi8(1);
  const __m128i avg1 = _mm_avg_epu8(*a0, *a1);
  const __m128i one = _mm_and_si128(_mm_xor_si128(*a0, *a1), ones);
  *avg = _mm_sub_epi8(avg1, one);
}

// Predictor0: ARGB_BLACK.
static void PredictorSub0_SSE2(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m128i black = _mm_set1_epi32((int)ARGB_BLACK);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    const __m128i res = _mm_sub_epi8(src, black);
    _mm_storeu_si128((__m128i*)&out[i], res);
  }
  if (i != num_pixels) {
    VP8LPredictorsSub_C[0](in + i, NULL, num_pixels - i, out + i);
  }
  (void)upper;
}

#define GENERATE_PREDICTOR_1(X, IN)                                         \
  static void PredictorSub##X##_SSE2(const uint32_t* const in,              \
                                     const uint32_t* const upper,           \
                                     int num_pixels,                        \
                                     uint32_t* WEBP_RESTRICT const out) {   \
    int i;                                                                  \
    for (i = 0; i + 4 <= num_pixels; i += 4) {                              \
      const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);          \
      const __m128i pred = _mm_loadu_si128((const __m128i*)&(IN));          \
      const __m128i res = _mm_sub_epi8(src, pred);                          \
      _mm_storeu_si128((__m128i*)&out[i], res);                             \
    }                                                                       \
    if (i != num_pixels) {                                                  \
      VP8LPredictorsSub_C[(X)](in + i, WEBP_OFFSET_PTR(upper, i),           \
                               num_pixels - i, out + i);                    \
    }                                                                       \
  }

GENERATE_PREDICTOR_1(1, in[i - 1])       // Predictor1: L
GENERATE_PREDICTOR_1(2, upper[i])        // Predictor2: T
GENERATE_PREDICTOR_1(3, upper[i + 1])    // Predictor3: TR
GENERATE_PREDICTOR_1(4, upper[i - 1])    // Predictor4: TL
#undef GENERATE_PREDICTOR_1

// Predictor5: avg2(avg2(L, TR), T)
static void PredictorSub5_SSE2(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i L = _mm_loadu_si128((const __m128i*)&in[i - 1]);
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i TR = _mm_loadu_si128((const __m128i*)&upper[i + 1]);
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    __m128i avg, pred, res;
    Average2_m128i(&L, &TR, &avg);
    Average2_m128i(&avg, &T, &pred);
    res = _mm_sub_epi8(src, pred);
    _mm_storeu_si128((__m128i*)&out[i], res);
  }
  if (i != num_pixels) {
    VP8LPredictorsSub_C[5](in + i, upper + i, num_pixels - i, out + i);
  }
}

#define GENERATE_PREDICTOR_2(X, A, B)                                         \
static void PredictorSub##X##_SSE2(const uint32_t* in, const uint32_t* upper, \
                                   int num_pixels,                            \
                                   uint32_t* WEBP_RESTRICT out) {             \
  int i;                                                                      \
  for (i = 0; i + 4 <= num_pixels; i += 4) {                                  \
    const __m128i tA = _mm_loadu_si128((const __m128i*)&(A));                 \
    const __m128i tB = _mm_loadu_si128((const __m128i*)&(B));                 \
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);              \
    __m128i pred, res;                                                        \
    Average2_m128i(&tA, &tB, &pred);                                          \
    res = _mm_sub_epi8(src, pred);                                            \
    _mm_storeu_si128((__m128i*)&out[i], res);                                 \
  }                                                                           \
  if (i != num_pixels) {                                                      \
    VP8LPredictorsSub_C[(X)](in + i, upper + i, num_pixels - i, out + i);     \
  }                                                                           \
}

GENERATE_PREDICTOR_2(6, in[i - 1], upper[i - 1])   // Predictor6: avg(L, TL)
GENERATE_PREDICTOR_2(7, in[i - 1], upper[i])       // Predictor7: avg(L, T)
GENERATE_PREDICTOR_2(8, upper[i - 1], upper[i])    // Predictor8: avg(TL, T)
GENERATE_PREDICTOR_2(9, upper[i], upper[i + 1])    // Predictor9: average(T, TR)
#undef GENERATE_PREDICTOR_2

// Predictor10: avg(avg(L,TL), avg(T, TR)).
static void PredictorSub10_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i L = _mm_loadu_si128((const __m128i*)&in[i - 1]);
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    const __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i TR = _mm_loadu_si128((const __m128i*)&upper[i + 1]);
    __m128i avgTTR, avgLTL, avg, res;
    Average2_m128i(&T, &TR, &avgTTR);
    Average2_m128i(&L, &TL, &avgLTL);
    Average2_m128i(&avgTTR, &avgLTL, &avg);
    res = _mm_sub_epi8(src, avg);
    _mm_storeu_si128((__m128i*)&out[i], res);
  }
  if (i != num_pixels) {
    VP8LPredictorsSub_C[10](in + i, upper + i, num_pixels - i, out + i);
  }
}

// Predictor11: select.
static void GetSumAbsDiff32_SSE2(const __m128i* const A, const __m128i* const B,
                                 __m128i* const out) {
  // We can unpack with any value on the upper 32 bits, provided it's the same
  // on both operands (to that their sum of abs diff is zero). Here we use *A.
  const __m128i A_lo = _mm_unpacklo_epi32(*A, *A);
  const __m128i B_lo = _mm_unpacklo_epi32(*B, *A);
  const __m128i A_hi = _mm_unpackhi_epi32(*A, *A);
  const __m128i B_hi = _mm_unpackhi_epi32(*B, *A);
  const __m128i s_lo = _mm_sad_epu8(A_lo, B_lo);
  const __m128i s_hi = _mm_sad_epu8(A_hi, B_hi);
  *out = _mm_packs_epi32(s_lo, s_hi);
}

static void PredictorSub11_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i L = _mm_loadu_si128((const __m128i*)&in[i - 1]);
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    __m128i pa, pb;
    GetSumAbsDiff32_SSE2(&T, &TL, &pa);   // pa = sum |T-TL|
    GetSumAbsDiff32_SSE2(&L, &TL, &pb);   // pb = sum |L-TL|
    {
      const __m128i mask = _mm_cmpgt_epi32(pb, pa);
      const __m128i A = _mm_and_si128(mask, L);
      const __m128i B = _mm_andnot_si128(mask, T);
      const __m128i pred = _mm_or_si128(A, B);    // pred = (L > T)? L : T
      const __m128i res = _mm_sub_epi8(src, pred);
      _mm_storeu_si128((__m128i*)&out[i], res);
    }
  }
  if (i != num_pixels) {
    VP8LPredictorsSub_C[11](in + i, upper + i, num_pixels - i, out + i);
  }
}

// Predictor12: ClampedSubSubtractFull.
static void PredictorSub12_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m128i zero = _mm_setzero_si128();
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    const __m128i L = _mm_loadu_si128((const __m128i*)&in[i - 1]);
    const __m128i L_lo = _mm_unpacklo_epi8(L, zero);
    const __m128i L_hi = _mm_unpackhi_epi8(L, zero);
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i T_lo = _mm_unpacklo_epi8(T, zero);
    const __m128i T_hi = _mm_unpackhi_epi8(T, zero);
    const __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    const __m128i TL_lo = _mm_unpacklo_epi8(TL, zero);
    const __m128i TL_hi = _mm_unpackhi_epi8(TL, zero);
    const __m128i diff_lo = _mm_sub_epi16(T_lo, TL_lo);
    const __m128i diff_hi = _mm_sub_epi16(T_hi, TL_hi);
    const __m128i pred_lo = _mm_add_epi16(L_lo, diff_lo);
    const __m128i pred_hi = _mm_add_epi16(L_hi, diff_hi);
    const __m128i pred = _mm_packus_epi16(pred_lo, pred_hi);
    const __m128i res = _mm_sub_epi8(src, pred);
    _mm_storeu_si128((__m128i*)&out[i], res);
  }
  if (i != num_pixels) {
    VP8LPredictorsSub_C[12](in + i, upper + i, num_pixels - i, out + i);
  }
}

// Predictors13: ClampedAddSubtractHalf
static void PredictorSub13_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m128i zero = _mm_setzero_si128();
  for (i = 0; i + 2 <= num_pixels; i += 2) {
    // we can only process two pixels at a time
    const __m128i L = _mm_loadl_epi64((const __m128i*)&in[i - 1]);
    const __m128i src = _mm_loadl_epi64((const __m128i*)&in[i]);
    const __m128i T = _mm_loadl_epi64((const __m128i*)&upper[i]);
    const __m128i TL = _mm_loadl_epi64((const __m128i*)&upper[i - 1]);
    const __m128i L_lo = _mm_unpacklo_epi8(L, zero);
    const __m128i T_lo = _mm_unpacklo_epi8(T, zero);
    const __m128i TL_lo = _mm_unpacklo_epi8(TL, zero);
    const __m128i sum = _mm_add_epi16(T_lo, L_lo);
    const __m128i avg = _mm_srli_epi16(sum, 1);
    const __m128i A1 = _mm_sub_epi16(avg, TL_lo);
    const __m128i bit_fix = _mm_cmpgt_epi16(TL_lo, avg);
    const __m128i A2 = _mm_sub_epi16(A1, bit_fix);
    const __m128i A3 = _mm_srai_epi16(A2, 1);
    const __m128i A4 = _mm_add_epi16(avg, A3);
    const __m128i pred = _mm_packus_epi16(A4, A4);
    const __m128i res = _mm_sub_epi8(src, pred);
    _mm_storel_epi64((__m128i*)&out[i], res);
  }
  if (i != num_pixels) {
    VP8LPredictorsSub_C[13](in + i, upper + i, num_pixels - i, out + i);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8LEncDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LEncDspInitSSE2(void) {
  VP8LSubtractGreenFromBlueAndRed = SubtractGreenFromBlueAndRed_SSE2;
  VP8LTransformColor = TransformColor_SSE2;
  VP8LCollectColorBlueTransforms = CollectColorBlueTransforms_SSE2;
  VP8LCollectColorRedTransforms = CollectColorRedTransforms_SSE2;
  VP8LAddVector = AddVector_SSE2;
  VP8LAddVectorEq = AddVectorEq_SSE2;
#if !defined(DONT_USE_COMBINED_SHANNON_ENTROPY_SSE2_FUNC)
  VP8LCombinedShannonEntropy = CombinedShannonEntropy_SSE2;
#endif
  VP8LVectorMismatch = VectorMismatch_SSE2;
  VP8LBundleColorMap = BundleColorMap_SSE2;

  VP8LPredictorsSub[0] = PredictorSub0_SSE2;
  VP8LPredictorsSub[1] = PredictorSub1_SSE2;
  VP8LPredictorsSub[2] = PredictorSub2_SSE2;
  VP8LPredictorsSub[3] = PredictorSub3_SSE2;
  VP8LPredictorsSub[4] = PredictorSub4_SSE2;
  VP8LPredictorsSub[5] = PredictorSub5_SSE2;
  VP8LPredictorsSub[6] = PredictorSub6_SSE2;
  VP8LPredictorsSub[7] = PredictorSub7_SSE2;
  VP8LPredictorsSub[8] = PredictorSub8_SSE2;
  VP8LPredictorsSub[9] = PredictorSub9_SSE2;
  VP8LPredictorsSub[10] = PredictorSub10_SSE2;
  VP8LPredictorsSub[11] = PredictorSub11_SSE2;
  VP8LPredictorsSub[12] = PredictorSub12_SSE2;
  VP8LPredictorsSub[13] = PredictorSub13_SSE2;
  VP8LPredictorsSub[14] = PredictorSub0_SSE2;  // <- padding security sentinels
  VP8LPredictorsSub[15] = PredictorSub0_SSE2;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(VP8LEncDspInitSSE2)

#endif  // WEBP_USE_SSE2
