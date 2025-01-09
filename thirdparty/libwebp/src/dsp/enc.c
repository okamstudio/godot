// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Speed-critical encoding functions.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdlib.h>  // for abs()

#include "src/dsp/dsp.h"
#include "src/enc/vp8i_enc.h"

static WEBP_INLINE uint8_t clip_8b(int v) {
  return (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
}

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE int clip_max(int v, int max) {
  return (v > max) ? max : v;
}
#endif  // !WEBP_NEON_OMIT_C_CODE

//------------------------------------------------------------------------------
// Compute susceptibility based on DCT-coeff histograms:
// the higher, the "easier" the macroblock is to compress.

const int VP8DspScan[16 + 4 + 4] = {
  // Luma
  0 +  0 * BPS,  4 +  0 * BPS, 8 +  0 * BPS, 12 +  0 * BPS,
  0 +  4 * BPS,  4 +  4 * BPS, 8 +  4 * BPS, 12 +  4 * BPS,
  0 +  8 * BPS,  4 +  8 * BPS, 8 +  8 * BPS, 12 +  8 * BPS,
  0 + 12 * BPS,  4 + 12 * BPS, 8 + 12 * BPS, 12 + 12 * BPS,

  0 + 0 * BPS,   4 + 0 * BPS, 0 + 4 * BPS,  4 + 4 * BPS,    // U
  8 + 0 * BPS,  12 + 0 * BPS, 8 + 4 * BPS, 12 + 4 * BPS     // V
};

// general-purpose util function
void VP8SetHistogramData(const int distribution[MAX_COEFF_THRESH + 1],
                         VP8Histogram* const histo) {
  int max_value = 0, last_non_zero = 1;
  int k;
  for (k = 0; k <= MAX_COEFF_THRESH; ++k) {
    const int value = distribution[k];
    if (value > 0) {
      if (value > max_value) max_value = value;
      last_non_zero = k;
    }
  }
  histo->max_value = max_value;
  histo->last_non_zero = last_non_zero;
}

#if !WEBP_NEON_OMIT_C_CODE
static void CollectHistogram_C(const uint8_t* WEBP_RESTRICT ref,
                               const uint8_t* WEBP_RESTRICT pred,
                               int start_block, int end_block,
                               VP8Histogram* WEBP_RESTRICT const histo) {
  int j;
  int distribution[MAX_COEFF_THRESH + 1] = { 0 };
  for (j = start_block; j < end_block; ++j) {
    int k;
    int16_t out[16];

    VP8FTransform(ref + VP8DspScan[j], pred + VP8DspScan[j], out);

    // Convert coefficients to bin.
    for (k = 0; k < 16; ++k) {
      const int v = abs(out[k]) >> 3;
      const int clipped_value = clip_max(v, MAX_COEFF_THRESH);
      ++distribution[clipped_value];
    }
  }
  VP8SetHistogramData(distribution, histo);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

//------------------------------------------------------------------------------
// run-time tables (~4k)

static uint8_t clip1[255 + 510 + 1];    // clips [-255,510] to [0,255]

// We declare this variable 'volatile' to prevent instruction reordering
// and make sure it's set to true _last_ (so as to be thread-safe)
static volatile int tables_ok = 0;

static WEBP_TSAN_IGNORE_FUNCTION void InitTables(void) {
  if (!tables_ok) {
    int i;
    for (i = -255; i <= 255 + 255; ++i) {
      clip1[255 + i] = clip_8b(i);
    }
    tables_ok = 1;
  }
}


//------------------------------------------------------------------------------
// Transforms (Paragraph 14.4)

#if !WEBP_NEON_OMIT_C_CODE

#define STORE(x, y, v) \
  dst[(x) + (y) * BPS] = clip_8b(ref[(x) + (y) * BPS] + ((v) >> 3))

static WEBP_INLINE void ITransformOne(const uint8_t* WEBP_RESTRICT ref,
                                      const int16_t* WEBP_RESTRICT in,
                                      uint8_t* WEBP_RESTRICT dst) {
  int C[4 * 4], *tmp;
  int i;
  tmp = C;
  for (i = 0; i < 4; ++i) {    // vertical pass
    const int a = in[0] + in[8];
    const int b = in[0] - in[8];
    const int c =
        WEBP_TRANSFORM_AC3_MUL2(in[4]) - WEBP_TRANSFORM_AC3_MUL1(in[12]);
    const int d =
        WEBP_TRANSFORM_AC3_MUL1(in[4]) + WEBP_TRANSFORM_AC3_MUL2(in[12]);
    tmp[0] = a + d;
    tmp[1] = b + c;
    tmp[2] = b - c;
    tmp[3] = a - d;
    tmp += 4;
    in++;
  }

  tmp = C;
  for (i = 0; i < 4; ++i) {    // horizontal pass
    const int dc = tmp[0] + 4;
    const int a = dc + tmp[8];
    const int b = dc - tmp[8];
    const int c =
        WEBP_TRANSFORM_AC3_MUL2(tmp[4]) - WEBP_TRANSFORM_AC3_MUL1(tmp[12]);
    const int d =
        WEBP_TRANSFORM_AC3_MUL1(tmp[4]) + WEBP_TRANSFORM_AC3_MUL2(tmp[12]);
    STORE(0, i, a + d);
    STORE(1, i, b + c);
    STORE(2, i, b - c);
    STORE(3, i, a - d);
    tmp++;
  }
}

static void ITransform_C(const uint8_t* WEBP_RESTRICT ref,
                         const int16_t* WEBP_RESTRICT in,
                         uint8_t* WEBP_RESTRICT dst,
                         int do_two) {
  ITransformOne(ref, in, dst);
  if (do_two) {
    ITransformOne(ref + 4, in + 16, dst + 4);
  }
}

static void FTransform_C(const uint8_t* WEBP_RESTRICT src,
                         const uint8_t* WEBP_RESTRICT ref,
                         int16_t* WEBP_RESTRICT out) {
  int i;
  int tmp[16];
  for (i = 0; i < 4; ++i, src += BPS, ref += BPS) {
    const int d0 = src[0] - ref[0];   // 9bit dynamic range ([-255,255])
    const int d1 = src[1] - ref[1];
    const int d2 = src[2] - ref[2];
    const int d3 = src[3] - ref[3];
    const int a0 = (d0 + d3);         // 10b                      [-510,510]
    const int a1 = (d1 + d2);
    const int a2 = (d1 - d2);
    const int a3 = (d0 - d3);
    tmp[0 + i * 4] = (a0 + a1) * 8;   // 14b                      [-8160,8160]
    tmp[1 + i * 4] = (a2 * 2217 + a3 * 5352 + 1812) >> 9;      // [-7536,7542]
    tmp[2 + i * 4] = (a0 - a1) * 8;
    tmp[3 + i * 4] = (a3 * 2217 - a2 * 5352 +  937) >> 9;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[12 + i]);  // 15b
    const int a1 = (tmp[4 + i] + tmp[ 8 + i]);
    const int a2 = (tmp[4 + i] - tmp[ 8 + i]);
    const int a3 = (tmp[0 + i] - tmp[12 + i]);
    out[0 + i] = (a0 + a1 + 7) >> 4;            // 12b
    out[4 + i] = ((a2 * 2217 + a3 * 5352 + 12000) >> 16) + (a3 != 0);
    out[8 + i] = (a0 - a1 + 7) >> 4;
    out[12+ i] = ((a3 * 2217 - a2 * 5352 + 51000) >> 16);
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

static void FTransform2_C(const uint8_t* WEBP_RESTRICT src,
                          const uint8_t* WEBP_RESTRICT ref,
                          int16_t* WEBP_RESTRICT out) {
  VP8FTransform(src, ref, out);
  VP8FTransform(src + 4, ref + 4, out + 16);
}

#if !WEBP_NEON_OMIT_C_CODE
static void FTransformWHT_C(const int16_t* WEBP_RESTRICT in,
                            int16_t* WEBP_RESTRICT out) {
  // input is 12b signed
  int32_t tmp[16];
  int i;
  for (i = 0; i < 4; ++i, in += 64) {
    const int a0 = (in[0 * 16] + in[2 * 16]);  // 13b
    const int a1 = (in[1 * 16] + in[3 * 16]);
    const int a2 = (in[1 * 16] - in[3 * 16]);
    const int a3 = (in[0 * 16] - in[2 * 16]);
    tmp[0 + i * 4] = a0 + a1;   // 14b
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  for (i = 0; i < 4; ++i) {
    const int a0 = (tmp[0 + i] + tmp[8 + i]);  // 15b
    const int a1 = (tmp[4 + i] + tmp[12+ i]);
    const int a2 = (tmp[4 + i] - tmp[12+ i]);
    const int a3 = (tmp[0 + i] - tmp[8 + i]);
    const int b0 = a0 + a1;    // 16b
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;
    out[ 0 + i] = b0 >> 1;     // 15b
    out[ 4 + i] = b1 >> 1;
    out[ 8 + i] = b2 >> 1;
    out[12 + i] = b3 >> 1;
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

#undef STORE

//------------------------------------------------------------------------------
// Intra predictions

static WEBP_INLINE void Fill(uint8_t* dst, int value, int size) {
  int j;
  for (j = 0; j < size; ++j) {
    memset(dst + j * BPS, value, size);
  }
}

static WEBP_INLINE void VerticalPred(uint8_t* WEBP_RESTRICT dst,
                                     const uint8_t* WEBP_RESTRICT top,
                                     int size) {
  int j;
  if (top != NULL) {
    for (j = 0; j < size; ++j) memcpy(dst + j * BPS, top, size);
  } else {
    Fill(dst, 127, size);
  }
}

static WEBP_INLINE void HorizontalPred(uint8_t* WEBP_RESTRICT dst,
                                       const uint8_t* WEBP_RESTRICT left,
                                       int size) {
  if (left != NULL) {
    int j;
    for (j = 0; j < size; ++j) {
      memset(dst + j * BPS, left[j], size);
    }
  } else {
    Fill(dst, 129, size);
  }
}

static WEBP_INLINE void TrueMotion(uint8_t* WEBP_RESTRICT dst,
                                   const uint8_t* WEBP_RESTRICT left,
                                   const uint8_t* WEBP_RESTRICT top, int size) {
  int y;
  if (left != NULL) {
    if (top != NULL) {
      const uint8_t* const clip = clip1 + 255 - left[-1];
      for (y = 0; y < size; ++y) {
        const uint8_t* const clip_table = clip + left[y];
        int x;
        for (x = 0; x < size; ++x) {
          dst[x] = clip_table[top[x]];
        }
        dst += BPS;
      }
    } else {
      HorizontalPred(dst, left, size);
    }
  } else {
    // true motion without left samples (hence: with default 129 value)
    // is equivalent to VE prediction where you just copy the top samples.
    // Note that if top samples are not available, the default value is
    // then 129, and not 127 as in the VerticalPred case.
    if (top != NULL) {
      VerticalPred(dst, top, size);
    } else {
      Fill(dst, 129, size);
    }
  }
}

static WEBP_INLINE void DCMode(uint8_t* WEBP_RESTRICT dst,
                               const uint8_t* WEBP_RESTRICT left,
                               const uint8_t* WEBP_RESTRICT top,
                               int size, int round, int shift) {
  int DC = 0;
  int j;
  if (top != NULL) {
    for (j = 0; j < size; ++j) DC += top[j];
    if (left != NULL) {   // top and left present
      for (j = 0; j < size; ++j) DC += left[j];
    } else {      // top, but no left
      DC += DC;
    }
    DC = (DC + round) >> shift;
  } else if (left != NULL) {   // left but no top
    for (j = 0; j < size; ++j) DC += left[j];
    DC += DC;
    DC = (DC + round) >> shift;
  } else {   // no top, no left, nothing.
    DC = 0x80;
  }
  Fill(dst, DC, size);
}

//------------------------------------------------------------------------------
// Chroma 8x8 prediction (paragraph 12.2)

static void IntraChromaPreds_C(uint8_t* WEBP_RESTRICT dst,
                               const uint8_t* WEBP_RESTRICT left,
                               const uint8_t* WEBP_RESTRICT top) {
  // U block
  DCMode(C8DC8 + dst, left, top, 8, 8, 4);
  VerticalPred(C8VE8 + dst, top, 8);
  HorizontalPred(C8HE8 + dst, left, 8);
  TrueMotion(C8TM8 + dst, left, top, 8);
  // V block
  dst += 8;
  if (top != NULL) top += 8;
  if (left != NULL) left += 16;
  DCMode(C8DC8 + dst, left, top, 8, 8, 4);
  VerticalPred(C8VE8 + dst, top, 8);
  HorizontalPred(C8HE8 + dst, left, 8);
  TrueMotion(C8TM8 + dst, left, top, 8);
}

//------------------------------------------------------------------------------
// luma 16x16 prediction (paragraph 12.3)

#if !WEBP_NEON_OMIT_C_CODE || !WEBP_AARCH64
static void Intra16Preds_C(uint8_t* WEBP_RESTRICT dst,
                           const uint8_t* WEBP_RESTRICT left,
                           const uint8_t* WEBP_RESTRICT top) {
  DCMode(I16DC16 + dst, left, top, 16, 16, 5);
  VerticalPred(I16VE16 + dst, top, 16);
  HorizontalPred(I16HE16 + dst, left, 16);
  TrueMotion(I16TM16 + dst, left, top, 16);
}
#endif  // !WEBP_NEON_OMIT_C_CODE || !WEBP_AARCH64

//------------------------------------------------------------------------------
// luma 4x4 prediction

#if !WEBP_NEON_OMIT_C_CODE || !WEBP_AARCH64 || BPS != 32

#define DST(x, y) dst[(x) + (y) * BPS]
#define AVG3(a, b, c) ((uint8_t)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

// vertical
static void VE4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  const uint8_t vals[4] = {
    AVG3(top[-1], top[0], top[1]),
    AVG3(top[ 0], top[1], top[2]),
    AVG3(top[ 1], top[2], top[3]),
    AVG3(top[ 2], top[3], top[4])
  };
  int i;
  for (i = 0; i < 4; ++i) {
    memcpy(dst + i * BPS, vals, 4);
  }
}

// horizontal
static void HE4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  WebPUint32ToMem(dst + 0 * BPS, 0x01010101U * AVG3(X, I, J));
  WebPUint32ToMem(dst + 1 * BPS, 0x01010101U * AVG3(I, J, K));
  WebPUint32ToMem(dst + 2 * BPS, 0x01010101U * AVG3(J, K, L));
  WebPUint32ToMem(dst + 3 * BPS, 0x01010101U * AVG3(K, L, L));
}

static void DC4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i) dc += top[i] + top[-5 + i];
  Fill(dst, dc >> 3, 4);
}

static void RD4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  DST(0, 3)                                     = AVG3(J, K, L);
  DST(0, 2) = DST(1, 3)                         = AVG3(I, J, K);
  DST(0, 1) = DST(1, 2) = DST(2, 3)             = AVG3(X, I, J);
  DST(0, 0) = DST(1, 1) = DST(2, 2) = DST(3, 3) = AVG3(A, X, I);
  DST(1, 0) = DST(2, 1) = DST(3, 2)             = AVG3(B, A, X);
  DST(2, 0) = DST(3, 1)                         = AVG3(C, B, A);
  DST(3, 0)                                     = AVG3(D, C, B);
}

static void LD4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top[4];
  const int F = top[5];
  const int G = top[6];
  const int H = top[7];
  DST(0, 0)                                     = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1)                         = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2)             = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3)             = AVG3(E, F, G);
  DST(3, 2) = DST(2, 3)                         = AVG3(F, G, H);
  DST(3, 3)                                     = AVG3(G, H, H);
}

static void VR4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  DST(0, 0) = DST(1, 2) = AVG2(X, A);
  DST(1, 0) = DST(2, 2) = AVG2(A, B);
  DST(2, 0) = DST(3, 2) = AVG2(B, C);
  DST(3, 0)             = AVG2(C, D);

  DST(0, 3) =             AVG3(K, J, I);
  DST(0, 2) =             AVG3(J, I, X);
  DST(0, 1) = DST(1, 3) = AVG3(I, X, A);
  DST(1, 1) = DST(2, 3) = AVG3(X, A, B);
  DST(2, 1) = DST(3, 3) = AVG3(A, B, C);
  DST(3, 1) =             AVG3(B, C, D);
}

static void VL4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];
  const int D = top[3];
  const int E = top[4];
  const int F = top[5];
  const int G = top[6];
  const int H = top[7];
  DST(0, 0) =             AVG2(A, B);
  DST(1, 0) = DST(0, 2) = AVG2(B, C);
  DST(2, 0) = DST(1, 2) = AVG2(C, D);
  DST(3, 0) = DST(2, 2) = AVG2(D, E);

  DST(0, 1) =             AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
              DST(3, 2) = AVG3(E, F, G);
              DST(3, 3) = AVG3(F, G, H);
}

static void HU4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  DST(0, 0) =             AVG2(I, J);
  DST(2, 0) = DST(0, 1) = AVG2(J, K);
  DST(2, 1) = DST(0, 2) = AVG2(K, L);
  DST(1, 0) =             AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) =
  DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

static void HD4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  const int X = top[-1];
  const int I = top[-2];
  const int J = top[-3];
  const int K = top[-4];
  const int L = top[-5];
  const int A = top[0];
  const int B = top[1];
  const int C = top[2];

  DST(0, 0) = DST(2, 1) = AVG2(I, X);
  DST(0, 1) = DST(2, 2) = AVG2(J, I);
  DST(0, 2) = DST(2, 3) = AVG2(K, J);
  DST(0, 3)             = AVG2(L, K);

  DST(3, 0)             = AVG3(A, B, C);
  DST(2, 0)             = AVG3(X, A, B);
  DST(1, 0) = DST(3, 1) = AVG3(I, X, A);
  DST(1, 1) = DST(3, 2) = AVG3(J, I, X);
  DST(1, 2) = DST(3, 3) = AVG3(K, J, I);
  DST(1, 3)             = AVG3(L, K, J);
}

static void TM4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int x, y;
  const uint8_t* const clip = clip1 + 255 - top[-1];
  for (y = 0; y < 4; ++y) {
    const uint8_t* const clip_table = clip + top[-2 - y];
    for (x = 0; x < 4; ++x) {
      dst[x] = clip_table[top[x]];
    }
    dst += BPS;
  }
}

#undef DST
#undef AVG3
#undef AVG2

// Left samples are top[-5 .. -2], top_left is top[-1], top are
// located at top[0..3], and top right is top[4..7]
static void Intra4Preds_C(uint8_t* WEBP_RESTRICT dst,
                          const uint8_t* WEBP_RESTRICT top) {
  DC4(I4DC4 + dst, top);
  TM4(I4TM4 + dst, top);
  VE4(I4VE4 + dst, top);
  HE4(I4HE4 + dst, top);
  RD4(I4RD4 + dst, top);
  VR4(I4VR4 + dst, top);
  LD4(I4LD4 + dst, top);
  VL4(I4VL4 + dst, top);
  HD4(I4HD4 + dst, top);
  HU4(I4HU4 + dst, top);
}

#endif  // !WEBP_NEON_OMIT_C_CODE || !WEBP_AARCH64 || BPS != 32

//------------------------------------------------------------------------------
// Metric

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE int GetSSE(const uint8_t* WEBP_RESTRICT a,
                              const uint8_t* WEBP_RESTRICT b,
                              int w, int h) {
  int count = 0;
  int y, x;
  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      const int diff = (int)a[x] - b[x];
      count += diff * diff;
    }
    a += BPS;
    b += BPS;
  }
  return count;
}

static int SSE16x16_C(const uint8_t* WEBP_RESTRICT a,
                      const uint8_t* WEBP_RESTRICT b) {
  return GetSSE(a, b, 16, 16);
}
static int SSE16x8_C(const uint8_t* WEBP_RESTRICT a,
                     const uint8_t* WEBP_RESTRICT b) {
  return GetSSE(a, b, 16, 8);
}
static int SSE8x8_C(const uint8_t* WEBP_RESTRICT a,
                    const uint8_t* WEBP_RESTRICT b) {
  return GetSSE(a, b, 8, 8);
}
static int SSE4x4_C(const uint8_t* WEBP_RESTRICT a,
                    const uint8_t* WEBP_RESTRICT b) {
  return GetSSE(a, b, 4, 4);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

static void Mean16x4_C(const uint8_t* WEBP_RESTRICT ref, uint32_t dc[4]) {
  int k, x, y;
  for (k = 0; k < 4; ++k) {
    uint32_t avg = 0;
    for (y = 0; y < 4; ++y) {
      for (x = 0; x < 4; ++x) {
        avg += ref[x + y * BPS];
      }
    }
    dc[k] = avg;
    ref += 4;   // go to next 4x4 block.
  }
}

//------------------------------------------------------------------------------
// Texture distortion
//
// We try to match the spectral content (weighted) between source and
// reconstructed samples.

#if !WEBP_NEON_OMIT_C_CODE
// Hadamard transform
// Returns the weighted sum of the absolute value of transformed coefficients.
// w[] contains a row-major 4 by 4 symmetric matrix.
static int TTransform(const uint8_t* WEBP_RESTRICT in,
                      const uint16_t* WEBP_RESTRICT w) {
  int sum = 0;
  int tmp[16];
  int i;
  // horizontal pass
  for (i = 0; i < 4; ++i, in += BPS) {
    const int a0 = in[0] + in[2];
    const int a1 = in[1] + in[3];
    const int a2 = in[1] - in[3];
    const int a3 = in[0] - in[2];
    tmp[0 + i * 4] = a0 + a1;
    tmp[1 + i * 4] = a3 + a2;
    tmp[2 + i * 4] = a3 - a2;
    tmp[3 + i * 4] = a0 - a1;
  }
  // vertical pass
  for (i = 0; i < 4; ++i, ++w) {
    const int a0 = tmp[0 + i] + tmp[8 + i];
    const int a1 = tmp[4 + i] + tmp[12+ i];
    const int a2 = tmp[4 + i] - tmp[12+ i];
    const int a3 = tmp[0 + i] - tmp[8 + i];
    const int b0 = a0 + a1;
    const int b1 = a3 + a2;
    const int b2 = a3 - a2;
    const int b3 = a0 - a1;

    sum += w[ 0] * abs(b0);
    sum += w[ 4] * abs(b1);
    sum += w[ 8] * abs(b2);
    sum += w[12] * abs(b3);
  }
  return sum;
}

static int Disto4x4_C(const uint8_t* WEBP_RESTRICT const a,
                      const uint8_t* WEBP_RESTRICT const b,
                      const uint16_t* WEBP_RESTRICT const w) {
  const int sum1 = TTransform(a, w);
  const int sum2 = TTransform(b, w);
  return abs(sum2 - sum1) >> 5;
}

static int Disto16x16_C(const uint8_t* WEBP_RESTRICT const a,
                        const uint8_t* WEBP_RESTRICT const b,
                        const uint16_t* WEBP_RESTRICT const w) {
  int D = 0;
  int x, y;
  for (y = 0; y < 16 * BPS; y += 4 * BPS) {
    for (x = 0; x < 16; x += 4) {
      D += Disto4x4_C(a + x + y, b + x + y, w);
    }
  }
  return D;
}
#endif  // !WEBP_NEON_OMIT_C_CODE

//------------------------------------------------------------------------------
// Quantization
//

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static const uint8_t kZigzag[16] = {
  0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

// Simple quantization
static int QuantizeBlock_C(int16_t in[16], int16_t out[16],
                           const VP8Matrix* WEBP_RESTRICT const mtx) {
  int last = -1;
  int n;
  for (n = 0; n < 16; ++n) {
    const int j = kZigzag[n];
    const int sign = (in[j] < 0);
    const uint32_t coeff = (sign ? -in[j] : in[j]) + mtx->sharpen_[j];
    if (coeff > mtx->zthresh_[j]) {
      const uint32_t Q = mtx->q_[j];
      const uint32_t iQ = mtx->iq_[j];
      const uint32_t B = mtx->bias_[j];
      int level = QUANTDIV(coeff, iQ, B);
      if (level > MAX_LEVEL) level = MAX_LEVEL;
      if (sign) level = -level;
      in[j] = level * (int)Q;
      out[n] = level;
      if (level) last = n;
    } else {
      out[n] = 0;
      in[j] = 0;
    }
  }
  return (last >= 0);
}

static int Quantize2Blocks_C(int16_t in[32], int16_t out[32],
                             const VP8Matrix* WEBP_RESTRICT const mtx) {
  int nz;
  nz  = VP8EncQuantizeBlock(in + 0 * 16, out + 0 * 16, mtx) << 0;
  nz |= VP8EncQuantizeBlock(in + 1 * 16, out + 1 * 16, mtx) << 1;
  return nz;
}
#endif  // !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC

//------------------------------------------------------------------------------
// Block copy

static WEBP_INLINE void Copy(const uint8_t* WEBP_RESTRICT src,
                             uint8_t* WEBP_RESTRICT dst, int w, int h) {
  int y;
  for (y = 0; y < h; ++y) {
    memcpy(dst, src, w);
    src += BPS;
    dst += BPS;
  }
}

static void Copy4x4_C(const uint8_t* WEBP_RESTRICT src,
                      uint8_t* WEBP_RESTRICT dst) {
  Copy(src, dst, 4, 4);
}

static void Copy16x8_C(const uint8_t* WEBP_RESTRICT src,
                       uint8_t* WEBP_RESTRICT dst) {
  Copy(src, dst, 16, 8);
}

//------------------------------------------------------------------------------
// Initialization

// Speed-critical function pointers. We have to initialize them to the default
// implementations within VP8EncDspInit().
VP8CHisto VP8CollectHistogram;
VP8Idct VP8ITransform;
VP8Fdct VP8FTransform;
VP8Fdct VP8FTransform2;
VP8WHT VP8FTransformWHT;
VP8Intra4Preds VP8EncPredLuma4;
VP8IntraPreds VP8EncPredLuma16;
VP8IntraPreds VP8EncPredChroma8;
VP8Metric VP8SSE16x16;
VP8Metric VP8SSE8x8;
VP8Metric VP8SSE16x8;
VP8Metric VP8SSE4x4;
VP8WMetric VP8TDisto4x4;
VP8WMetric VP8TDisto16x16;
VP8MeanMetric VP8Mean16x4;
VP8QuantizeBlock VP8EncQuantizeBlock;
VP8Quantize2Blocks VP8EncQuantize2Blocks;
VP8QuantizeBlockWHT VP8EncQuantizeBlockWHT;
VP8BlockCopy VP8Copy4x4;
VP8BlockCopy VP8Copy16x8;

extern VP8CPUInfo VP8GetCPUInfo;
extern void VP8EncDspInitSSE2(void);
extern void VP8EncDspInitSSE41(void);
extern void VP8EncDspInitNEON(void);
extern void VP8EncDspInitMIPS32(void);
extern void VP8EncDspInitMIPSdspR2(void);
extern void VP8EncDspInitMSA(void);

WEBP_DSP_INIT_FUNC(VP8EncDspInit) {
  VP8DspInit();  // common inverse transforms
  InitTables();

  // default C implementations
#if !WEBP_NEON_OMIT_C_CODE
  VP8ITransform = ITransform_C;
  VP8FTransform = FTransform_C;
  VP8FTransformWHT = FTransformWHT_C;
  VP8TDisto4x4 = Disto4x4_C;
  VP8TDisto16x16 = Disto16x16_C;
  VP8CollectHistogram = CollectHistogram_C;
  VP8SSE16x16 = SSE16x16_C;
  VP8SSE16x8 = SSE16x8_C;
  VP8SSE8x8 = SSE8x8_C;
  VP8SSE4x4 = SSE4x4_C;
#endif

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
  VP8EncQuantizeBlock = QuantizeBlock_C;
  VP8EncQuantize2Blocks = Quantize2Blocks_C;
  VP8EncQuantizeBlockWHT = QuantizeBlock_C;
#endif

#if !WEBP_NEON_OMIT_C_CODE || !WEBP_AARCH64 || BPS != 32
  VP8EncPredLuma4 = Intra4Preds_C;
#endif
#if !WEBP_NEON_OMIT_C_CODE || !WEBP_AARCH64
  VP8EncPredLuma16 = Intra16Preds_C;
#endif

  VP8FTransform2 = FTransform2_C;
  VP8EncPredChroma8 = IntraChromaPreds_C;
  VP8Mean16x4 = Mean16x4_C;
  VP8Copy4x4 = Copy4x4_C;
  VP8Copy16x8 = Copy16x8_C;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      VP8EncDspInitSSE2();
#if defined(WEBP_HAVE_SSE41)
      if (VP8GetCPUInfo(kSSE4_1)) {
        VP8EncDspInitSSE41();
      }
#endif
    }
#endif
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      VP8EncDspInitMIPS32();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      VP8EncDspInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      VP8EncDspInitMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    VP8EncDspInitNEON();
  }
#endif

  assert(VP8ITransform != NULL);
  assert(VP8FTransform != NULL);
  assert(VP8FTransformWHT != NULL);
  assert(VP8TDisto4x4 != NULL);
  assert(VP8TDisto16x16 != NULL);
  assert(VP8CollectHistogram != NULL);
  assert(VP8SSE16x16 != NULL);
  assert(VP8SSE16x8 != NULL);
  assert(VP8SSE8x8 != NULL);
  assert(VP8SSE4x4 != NULL);
  assert(VP8EncQuantizeBlock != NULL);
  assert(VP8EncQuantize2Blocks != NULL);
  assert(VP8FTransform2 != NULL);
  assert(VP8EncPredLuma4 != NULL);
  assert(VP8EncPredLuma16 != NULL);
  assert(VP8EncPredChroma8 != NULL);
  assert(VP8Mean16x4 != NULL);
  assert(VP8EncQuantizeBlockWHT != NULL);
  assert(VP8Copy4x4 != NULL);
  assert(VP8Copy16x8 != NULL);
}
