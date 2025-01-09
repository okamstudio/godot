// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Speed-critical decoding functions, default plain-C implementations.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>

#include "src/dsp/dsp.h"
#include "src/dec/vp8i_dec.h"
#include "src/utils/utils.h"

//------------------------------------------------------------------------------

static WEBP_INLINE uint8_t clip_8b(int v) {
  return (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
}

//------------------------------------------------------------------------------
// Transforms (Paragraph 14.4)

#define STORE(x, y, v) \
  dst[(x) + (y) * BPS] = clip_8b(dst[(x) + (y) * BPS] + ((v) >> 3))

#define STORE2(y, dc, d, c) do {    \
  const int DC = (dc);              \
  STORE(0, y, DC + (d));            \
  STORE(1, y, DC + (c));            \
  STORE(2, y, DC - (c));            \
  STORE(3, y, DC - (d));            \
} while (0)

#if !WEBP_NEON_OMIT_C_CODE
static void TransformOne_C(const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst) {
  int C[4 * 4], *tmp;
  int i;
  tmp = C;
  for (i = 0; i < 4; ++i) {    // vertical pass
    const int a = in[0] + in[8];    // [-4096, 4094]
    const int b = in[0] - in[8];    // [-4095, 4095]
    const int c = WEBP_TRANSFORM_AC3_MUL2(in[4]) -
                  WEBP_TRANSFORM_AC3_MUL1(in[12]);  // [-3783, 3783]
    const int d = WEBP_TRANSFORM_AC3_MUL1(in[4]) +
                  WEBP_TRANSFORM_AC3_MUL2(in[12]);  // [-3785, 3781]
    tmp[0] = a + d;   // [-7881, 7875]
    tmp[1] = b + c;   // [-7878, 7878]
    tmp[2] = b - c;   // [-7878, 7878]
    tmp[3] = a - d;   // [-7877, 7879]
    tmp += 4;
    in++;
  }
  // Each pass is expanding the dynamic range by ~3.85 (upper bound).
  // The exact value is (2. + (20091 + 35468) / 65536).
  // After the second pass, maximum interval is [-3794, 3794], assuming
  // an input in [-2048, 2047] interval. We then need to add a dst value
  // in the [0, 255] range.
  // In the worst case scenario, the input to clip_8b() can be as large as
  // [-60713, 60968].
  tmp = C;
  for (i = 0; i < 4; ++i) {    // horizontal pass
    const int dc = tmp[0] + 4;
    const int a =  dc +  tmp[8];
    const int b =  dc -  tmp[8];
    const int c =
        WEBP_TRANSFORM_AC3_MUL2(tmp[4]) - WEBP_TRANSFORM_AC3_MUL1(tmp[12]);
    const int d =
        WEBP_TRANSFORM_AC3_MUL1(tmp[4]) + WEBP_TRANSFORM_AC3_MUL2(tmp[12]);
    STORE(0, 0, a + d);
    STORE(1, 0, b + c);
    STORE(2, 0, b - c);
    STORE(3, 0, a - d);
    tmp++;
    dst += BPS;
  }
}

// Simplified transform when only in[0], in[1] and in[4] are non-zero
static void TransformAC3_C(const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst) {
  const int a = in[0] + 4;
  const int c4 = WEBP_TRANSFORM_AC3_MUL2(in[4]);
  const int d4 = WEBP_TRANSFORM_AC3_MUL1(in[4]);
  const int c1 = WEBP_TRANSFORM_AC3_MUL2(in[1]);
  const int d1 = WEBP_TRANSFORM_AC3_MUL1(in[1]);
  STORE2(0, a + d4, d1, c1);
  STORE2(1, a + c4, d1, c1);
  STORE2(2, a - c4, d1, c1);
  STORE2(3, a - d4, d1, c1);
}
#undef STORE2

static void TransformTwo_C(const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst, int do_two) {
  TransformOne_C(in, dst);
  if (do_two) {
    TransformOne_C(in + 16, dst + 4);
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

static void TransformUV_C(const int16_t* WEBP_RESTRICT in,
                          uint8_t* WEBP_RESTRICT dst) {
  VP8Transform(in + 0 * 16, dst, 1);
  VP8Transform(in + 2 * 16, dst + 4 * BPS, 1);
}

#if !WEBP_NEON_OMIT_C_CODE
static void TransformDC_C(const int16_t* WEBP_RESTRICT in,
                          uint8_t* WEBP_RESTRICT dst) {
  const int DC = in[0] + 4;
  int i, j;
  for (j = 0; j < 4; ++j) {
    for (i = 0; i < 4; ++i) {
      STORE(i, j, DC);
    }
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

static void TransformDCUV_C(const int16_t* WEBP_RESTRICT in,
                            uint8_t* WEBP_RESTRICT dst) {
  if (in[0 * 16]) VP8TransformDC(in + 0 * 16, dst);
  if (in[1 * 16]) VP8TransformDC(in + 1 * 16, dst + 4);
  if (in[2 * 16]) VP8TransformDC(in + 2 * 16, dst + 4 * BPS);
  if (in[3 * 16]) VP8TransformDC(in + 3 * 16, dst + 4 * BPS + 4);
}

#undef STORE

//------------------------------------------------------------------------------
// Paragraph 14.3

#if !WEBP_NEON_OMIT_C_CODE
static void TransformWHT_C(const int16_t* WEBP_RESTRICT in,
                           int16_t* WEBP_RESTRICT out) {
  int tmp[16];
  int i;
  for (i = 0; i < 4; ++i) {
    const int a0 = in[0 + i] + in[12 + i];
    const int a1 = in[4 + i] + in[ 8 + i];
    const int a2 = in[4 + i] - in[ 8 + i];
    const int a3 = in[0 + i] - in[12 + i];
    tmp[0  + i] = a0 + a1;
    tmp[8  + i] = a0 - a1;
    tmp[4  + i] = a3 + a2;
    tmp[12 + i] = a3 - a2;
  }
  for (i = 0; i < 4; ++i) {
    const int dc = tmp[0 + i * 4] + 3;    // w/ rounder
    const int a0 = dc             + tmp[3 + i * 4];
    const int a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
    const int a2 = tmp[1 + i * 4] - tmp[2 + i * 4];
    const int a3 = dc             - tmp[3 + i * 4];
    out[ 0] = (a0 + a1) >> 3;
    out[16] = (a3 + a2) >> 3;
    out[32] = (a0 - a1) >> 3;
    out[48] = (a3 - a2) >> 3;
    out += 64;
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

VP8WHT VP8TransformWHT;

//------------------------------------------------------------------------------
// Intra predictions

#define DST(x, y) dst[(x) + (y) * BPS]

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE void TrueMotion(uint8_t* dst, int size) {
  const uint8_t* top = dst - BPS;
  const uint8_t* const clip0 = VP8kclip1 - top[-1];
  int y;
  for (y = 0; y < size; ++y) {
    const uint8_t* const clip = clip0 + dst[-1];
    int x;
    for (x = 0; x < size; ++x) {
      dst[x] = clip[top[x]];
    }
    dst += BPS;
  }
}
static void TM4_C(uint8_t* dst)   { TrueMotion(dst, 4); }
static void TM8uv_C(uint8_t* dst) { TrueMotion(dst, 8); }
static void TM16_C(uint8_t* dst)  { TrueMotion(dst, 16); }

//------------------------------------------------------------------------------
// 16x16

static void VE16_C(uint8_t* dst) {     // vertical
  int j;
  for (j = 0; j < 16; ++j) {
    memcpy(dst + j * BPS, dst - BPS, 16);
  }
}

static void HE16_C(uint8_t* dst) {     // horizontal
  int j;
  for (j = 16; j > 0; --j) {
    memset(dst, dst[-1], 16);
    dst += BPS;
  }
}

static WEBP_INLINE void Put16(int v, uint8_t* dst) {
  int j;
  for (j = 0; j < 16; ++j) {
    memset(dst + j * BPS, v, 16);
  }
}

static void DC16_C(uint8_t* dst) {    // DC
  int DC = 16;
  int j;
  for (j = 0; j < 16; ++j) {
    DC += dst[-1 + j * BPS] + dst[j - BPS];
  }
  Put16(DC >> 5, dst);
}

static void DC16NoTop_C(uint8_t* dst) {   // DC with top samples not available
  int DC = 8;
  int j;
  for (j = 0; j < 16; ++j) {
    DC += dst[-1 + j * BPS];
  }
  Put16(DC >> 4, dst);
}

static void DC16NoLeft_C(uint8_t* dst) {  // DC with left samples not available
  int DC = 8;
  int i;
  for (i = 0; i < 16; ++i) {
    DC += dst[i - BPS];
  }
  Put16(DC >> 4, dst);
}

static void DC16NoTopLeft_C(uint8_t* dst) {  // DC with no top and left samples
  Put16(0x80, dst);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

VP8PredFunc VP8PredLuma16[NUM_B_DC_MODES];

//------------------------------------------------------------------------------
// 4x4

#define AVG3(a, b, c) ((uint8_t)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

#if !WEBP_NEON_OMIT_C_CODE
static void VE4_C(uint8_t* dst) {    // vertical
  const uint8_t* top = dst - BPS;
  const uint8_t vals[4] = {
    AVG3(top[-1], top[0], top[1]),
    AVG3(top[ 0], top[1], top[2]),
    AVG3(top[ 1], top[2], top[3]),
    AVG3(top[ 2], top[3], top[4])
  };
  int i;
  for (i = 0; i < 4; ++i) {
    memcpy(dst + i * BPS, vals, sizeof(vals));
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

static void HE4_C(uint8_t* dst) {    // horizontal
  const int A = dst[-1 - BPS];
  const int B = dst[-1];
  const int C = dst[-1 + BPS];
  const int D = dst[-1 + 2 * BPS];
  const int E = dst[-1 + 3 * BPS];
  WebPUint32ToMem(dst + 0 * BPS, 0x01010101U * AVG3(A, B, C));
  WebPUint32ToMem(dst + 1 * BPS, 0x01010101U * AVG3(B, C, D));
  WebPUint32ToMem(dst + 2 * BPS, 0x01010101U * AVG3(C, D, E));
  WebPUint32ToMem(dst + 3 * BPS, 0x01010101U * AVG3(D, E, E));
}

#if !WEBP_NEON_OMIT_C_CODE
static void DC4_C(uint8_t* dst) {   // DC
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i) dc += dst[i - BPS] + dst[-1 + i * BPS];
  dc >>= 3;
  for (i = 0; i < 4; ++i) memset(dst + i * BPS, dc, 4);
}

static void RD4_C(uint8_t* dst) {   // Down-right
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int L = dst[-1 + 3 * BPS];
  const int X = dst[-1 - BPS];
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];
  const int D = dst[3 - BPS];
  DST(0, 3)                                     = AVG3(J, K, L);
  DST(1, 3) = DST(0, 2)                         = AVG3(I, J, K);
  DST(2, 3) = DST(1, 2) = DST(0, 1)             = AVG3(X, I, J);
  DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = AVG3(A, X, I);
              DST(3, 2) = DST(2, 1) = DST(1, 0) = AVG3(B, A, X);
                          DST(3, 1) = DST(2, 0) = AVG3(C, B, A);
                                      DST(3, 0) = AVG3(D, C, B);
}

static void LD4_C(uint8_t* dst) {   // Down-Left
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];
  const int D = dst[3 - BPS];
  const int E = dst[4 - BPS];
  const int F = dst[5 - BPS];
  const int G = dst[6 - BPS];
  const int H = dst[7 - BPS];
  DST(0, 0)                                     = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1)                         = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2)             = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
              DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
                          DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
                                      DST(3, 3) = AVG3(G, H, H);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

static void VR4_C(uint8_t* dst) {   // Vertical-Right
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int X = dst[-1 - BPS];
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];
  const int D = dst[3 - BPS];
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

static void VL4_C(uint8_t* dst) {   // Vertical-Left
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];
  const int D = dst[3 - BPS];
  const int E = dst[4 - BPS];
  const int F = dst[5 - BPS];
  const int G = dst[6 - BPS];
  const int H = dst[7 - BPS];
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

static void HU4_C(uint8_t* dst) {   // Horizontal-Up
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int L = dst[-1 + 3 * BPS];
  DST(0, 0) =             AVG2(I, J);
  DST(2, 0) = DST(0, 1) = AVG2(J, K);
  DST(2, 1) = DST(0, 2) = AVG2(K, L);
  DST(1, 0) =             AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) =
    DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

static void HD4_C(uint8_t* dst) {  // Horizontal-Down
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int L = dst[-1 + 3 * BPS];
  const int X = dst[-1 - BPS];
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];

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

#undef DST
#undef AVG3
#undef AVG2

VP8PredFunc VP8PredLuma4[NUM_BMODES];

//------------------------------------------------------------------------------
// Chroma

#if !WEBP_NEON_OMIT_C_CODE
static void VE8uv_C(uint8_t* dst) {    // vertical
  int j;
  for (j = 0; j < 8; ++j) {
    memcpy(dst + j * BPS, dst - BPS, 8);
  }
}

static void HE8uv_C(uint8_t* dst) {    // horizontal
  int j;
  for (j = 0; j < 8; ++j) {
    memset(dst, dst[-1], 8);
    dst += BPS;
  }
}

// helper for chroma-DC predictions
static WEBP_INLINE void Put8x8uv(uint8_t value, uint8_t* dst) {
  int j;
  for (j = 0; j < 8; ++j) {
    memset(dst + j * BPS, value, 8);
  }
}

static void DC8uv_C(uint8_t* dst) {     // DC
  int dc0 = 8;
  int i;
  for (i = 0; i < 8; ++i) {
    dc0 += dst[i - BPS] + dst[-1 + i * BPS];
  }
  Put8x8uv(dc0 >> 4, dst);
}

static void DC8uvNoLeft_C(uint8_t* dst) {   // DC with no left samples
  int dc0 = 4;
  int i;
  for (i = 0; i < 8; ++i) {
    dc0 += dst[i - BPS];
  }
  Put8x8uv(dc0 >> 3, dst);
}

static void DC8uvNoTop_C(uint8_t* dst) {  // DC with no top samples
  int dc0 = 4;
  int i;
  for (i = 0; i < 8; ++i) {
    dc0 += dst[-1 + i * BPS];
  }
  Put8x8uv(dc0 >> 3, dst);
}

static void DC8uvNoTopLeft_C(uint8_t* dst) {    // DC with nothing
  Put8x8uv(0x80, dst);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

VP8PredFunc VP8PredChroma8[NUM_B_DC_MODES];

//------------------------------------------------------------------------------
// Edge filtering functions

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
// 4 pixels in, 2 pixels out
static WEBP_INLINE void DoFilter2_C(uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0) + VP8ksclip1[p1 - q1];  // in [-893,892]
  const int a1 = VP8ksclip2[(a + 4) >> 3];            // in [-16,15]
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  p[-step] = VP8kclip1[p0 + a2];
  p[    0] = VP8kclip1[q0 - a1];
}

// 4 pixels in, 4 pixels out
static WEBP_INLINE void DoFilter4_C(uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0);
  const int a1 = VP8ksclip2[(a + 4) >> 3];
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  const int a3 = (a1 + 1) >> 1;
  p[-2*step] = VP8kclip1[p1 + a3];
  p[-  step] = VP8kclip1[p0 + a2];
  p[      0] = VP8kclip1[q0 - a1];
  p[   step] = VP8kclip1[q1 - a3];
}

// 6 pixels in, 6 pixels out
static WEBP_INLINE void DoFilter6_C(uint8_t* p, int step) {
  const int p2 = p[-3*step], p1 = p[-2*step], p0 = p[-step];
  const int q0 = p[0], q1 = p[step], q2 = p[2*step];
  const int a = VP8ksclip1[3 * (q0 - p0) + VP8ksclip1[p1 - q1]];
  // a is in [-128,127], a1 in [-27,27], a2 in [-18,18] and a3 in [-9,9]
  const int a1 = (27 * a + 63) >> 7;  // eq. to ((3 * a + 7) * 9) >> 7
  const int a2 = (18 * a + 63) >> 7;  // eq. to ((2 * a + 7) * 9) >> 7
  const int a3 = (9  * a + 63) >> 7;  // eq. to ((1 * a + 7) * 9) >> 7
  p[-3*step] = VP8kclip1[p2 + a3];
  p[-2*step] = VP8kclip1[p1 + a2];
  p[-  step] = VP8kclip1[p0 + a1];
  p[      0] = VP8kclip1[q0 - a1];
  p[   step] = VP8kclip1[q1 - a2];
  p[ 2*step] = VP8kclip1[q2 - a3];
}

static WEBP_INLINE int Hev(const uint8_t* p, int step, int thresh) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return (VP8kabs0[p1 - p0] > thresh) || (VP8kabs0[q1 - q0] > thresh);
}
#endif  // !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE int NeedsFilter_C(const uint8_t* p, int step, int t) {
  const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return ((4 * VP8kabs0[p0 - q0] + VP8kabs0[p1 - q1]) <= t);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static WEBP_INLINE int NeedsFilter2_C(const uint8_t* p,
                                      int step, int t, int it) {
  const int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step];
  const int p0 = p[-step], q0 = p[0];
  const int q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
  if ((4 * VP8kabs0[p0 - q0] + VP8kabs0[p1 - q1]) > t) return 0;
  return VP8kabs0[p3 - p2] <= it && VP8kabs0[p2 - p1] <= it &&
         VP8kabs0[p1 - p0] <= it && VP8kabs0[q3 - q2] <= it &&
         VP8kabs0[q2 - q1] <= it && VP8kabs0[q1 - q0] <= it;
}
#endif  // !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC

//------------------------------------------------------------------------------
// Simple In-loop filtering (Paragraph 15.2)

#if !WEBP_NEON_OMIT_C_CODE
static void SimpleVFilter16_C(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (NeedsFilter_C(p + i, stride, thresh2)) {
      DoFilter2_C(p + i, stride);
    }
  }
}

static void SimpleHFilter16_C(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (NeedsFilter_C(p + i * stride, 1, thresh2)) {
      DoFilter2_C(p + i * stride, 1);
    }
  }
}

static void SimpleVFilter16i_C(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16_C(p, stride, thresh);
  }
}

static void SimpleHFilter16i_C(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16_C(p, stride, thresh);
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

//------------------------------------------------------------------------------
// Complex In-loop filtering (Paragraph 15.3)

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static WEBP_INLINE void FilterLoop26_C(uint8_t* p,
                                       int hstride, int vstride, int size,
                                       int thresh, int ithresh,
                                       int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  while (size-- > 0) {
    if (NeedsFilter2_C(p, hstride, thresh2, ithresh)) {
      if (Hev(p, hstride, hev_thresh)) {
        DoFilter2_C(p, hstride);
      } else {
        DoFilter6_C(p, hstride);
      }
    }
    p += vstride;
  }
}

static WEBP_INLINE void FilterLoop24_C(uint8_t* p,
                                       int hstride, int vstride, int size,
                                       int thresh, int ithresh,
                                       int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  while (size-- > 0) {
    if (NeedsFilter2_C(p, hstride, thresh2, ithresh)) {
      if (Hev(p, hstride, hev_thresh)) {
        DoFilter2_C(p, hstride);
      } else {
        DoFilter4_C(p, hstride);
      }
    }
    p += vstride;
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC

#if !WEBP_NEON_OMIT_C_CODE
// on macroblock edges
static void VFilter16_C(uint8_t* p, int stride,
                        int thresh, int ithresh, int hev_thresh) {
  FilterLoop26_C(p, stride, 1, 16, thresh, ithresh, hev_thresh);
}

static void HFilter16_C(uint8_t* p, int stride,
                        int thresh, int ithresh, int hev_thresh) {
  FilterLoop26_C(p, 1, stride, 16, thresh, ithresh, hev_thresh);
}

// on three inner edges
static void VFilter16i_C(uint8_t* p, int stride,
                         int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    FilterLoop24_C(p, stride, 1, 16, thresh, ithresh, hev_thresh);
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static void HFilter16i_C(uint8_t* p, int stride,
                         int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    FilterLoop24_C(p, 1, stride, 16, thresh, ithresh, hev_thresh);
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC

#if !WEBP_NEON_OMIT_C_CODE
// 8-pixels wide variant, for chroma filtering
static void VFilter8_C(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                       int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop26_C(u, stride, 1, 8, thresh, ithresh, hev_thresh);
  FilterLoop26_C(v, stride, 1, 8, thresh, ithresh, hev_thresh);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static void HFilter8_C(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                       int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop26_C(u, 1, stride, 8, thresh, ithresh, hev_thresh);
  FilterLoop26_C(v, 1, stride, 8, thresh, ithresh, hev_thresh);
}
#endif  // !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC

#if !WEBP_NEON_OMIT_C_CODE
static void VFilter8i_C(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                        int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop24_C(u + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
  FilterLoop24_C(v + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static void HFilter8i_C(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                        int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop24_C(u + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
  FilterLoop24_C(v + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
}
#endif  // !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC

//------------------------------------------------------------------------------

static void DitherCombine8x8_C(const uint8_t* WEBP_RESTRICT dither,
                               uint8_t* WEBP_RESTRICT dst, int dst_stride) {
  int i, j;
  for (j = 0; j < 8; ++j) {
    for (i = 0; i < 8; ++i) {
      const int delta0 = dither[i] - VP8_DITHER_AMP_CENTER;
      const int delta1 =
          (delta0 + VP8_DITHER_DESCALE_ROUNDER) >> VP8_DITHER_DESCALE;
      dst[i] = clip_8b((int)dst[i] + delta1);
    }
    dst += dst_stride;
    dither += 8;
  }
}

//------------------------------------------------------------------------------

VP8DecIdct2 VP8Transform;
VP8DecIdct VP8TransformAC3;
VP8DecIdct VP8TransformUV;
VP8DecIdct VP8TransformDC;
VP8DecIdct VP8TransformDCUV;

VP8LumaFilterFunc VP8VFilter16;
VP8LumaFilterFunc VP8HFilter16;
VP8ChromaFilterFunc VP8VFilter8;
VP8ChromaFilterFunc VP8HFilter8;
VP8LumaFilterFunc VP8VFilter16i;
VP8LumaFilterFunc VP8HFilter16i;
VP8ChromaFilterFunc VP8VFilter8i;
VP8ChromaFilterFunc VP8HFilter8i;
VP8SimpleFilterFunc VP8SimpleVFilter16;
VP8SimpleFilterFunc VP8SimpleHFilter16;
VP8SimpleFilterFunc VP8SimpleVFilter16i;
VP8SimpleFilterFunc VP8SimpleHFilter16i;

void (*VP8DitherCombine8x8)(const uint8_t* WEBP_RESTRICT dither,
                            uint8_t* WEBP_RESTRICT dst, int dst_stride);

extern VP8CPUInfo VP8GetCPUInfo;
extern void VP8DspInitSSE2(void);
extern void VP8DspInitSSE41(void);
extern void VP8DspInitNEON(void);
extern void VP8DspInitMIPS32(void);
extern void VP8DspInitMIPSdspR2(void);
extern void VP8DspInitMSA(void);

WEBP_DSP_INIT_FUNC(VP8DspInit) {
  VP8InitClipTables();

#if !WEBP_NEON_OMIT_C_CODE
  VP8TransformWHT = TransformWHT_C;
  VP8Transform = TransformTwo_C;
  VP8TransformDC = TransformDC_C;
  VP8TransformAC3 = TransformAC3_C;
#endif
  VP8TransformUV = TransformUV_C;
  VP8TransformDCUV = TransformDCUV_C;

#if !WEBP_NEON_OMIT_C_CODE
  VP8VFilter16 = VFilter16_C;
  VP8VFilter16i = VFilter16i_C;
  VP8HFilter16 = HFilter16_C;
  VP8VFilter8 = VFilter8_C;
  VP8VFilter8i = VFilter8i_C;
  VP8SimpleVFilter16 = SimpleVFilter16_C;
  VP8SimpleHFilter16 = SimpleHFilter16_C;
  VP8SimpleVFilter16i = SimpleVFilter16i_C;
  VP8SimpleHFilter16i = SimpleHFilter16i_C;
#endif

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
  VP8HFilter16i = HFilter16i_C;
  VP8HFilter8 = HFilter8_C;
  VP8HFilter8i = HFilter8i_C;
#endif

#if !WEBP_NEON_OMIT_C_CODE
  VP8PredLuma4[0] = DC4_C;
  VP8PredLuma4[1] = TM4_C;
  VP8PredLuma4[2] = VE4_C;
  VP8PredLuma4[4] = RD4_C;
  VP8PredLuma4[6] = LD4_C;
#endif

  VP8PredLuma4[3] = HE4_C;
  VP8PredLuma4[5] = VR4_C;
  VP8PredLuma4[7] = VL4_C;
  VP8PredLuma4[8] = HD4_C;
  VP8PredLuma4[9] = HU4_C;

#if !WEBP_NEON_OMIT_C_CODE
  VP8PredLuma16[0] = DC16_C;
  VP8PredLuma16[1] = TM16_C;
  VP8PredLuma16[2] = VE16_C;
  VP8PredLuma16[3] = HE16_C;
  VP8PredLuma16[4] = DC16NoTop_C;
  VP8PredLuma16[5] = DC16NoLeft_C;
  VP8PredLuma16[6] = DC16NoTopLeft_C;

  VP8PredChroma8[0] = DC8uv_C;
  VP8PredChroma8[1] = TM8uv_C;
  VP8PredChroma8[2] = VE8uv_C;
  VP8PredChroma8[3] = HE8uv_C;
  VP8PredChroma8[4] = DC8uvNoTop_C;
  VP8PredChroma8[5] = DC8uvNoLeft_C;
  VP8PredChroma8[6] = DC8uvNoTopLeft_C;
#endif

  VP8DitherCombine8x8 = DitherCombine8x8_C;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      VP8DspInitSSE2();
#if defined(WEBP_HAVE_SSE41)
      if (VP8GetCPUInfo(kSSE4_1)) {
        VP8DspInitSSE41();
      }
#endif
    }
#endif
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      VP8DspInitMIPS32();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      VP8DspInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      VP8DspInitMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    VP8DspInitNEON();
  }
#endif

  assert(VP8TransformWHT != NULL);
  assert(VP8Transform != NULL);
  assert(VP8TransformDC != NULL);
  assert(VP8TransformAC3 != NULL);
  assert(VP8TransformUV != NULL);
  assert(VP8TransformDCUV != NULL);
  assert(VP8VFilter16 != NULL);
  assert(VP8HFilter16 != NULL);
  assert(VP8VFilter8 != NULL);
  assert(VP8HFilter8 != NULL);
  assert(VP8VFilter16i != NULL);
  assert(VP8HFilter16i != NULL);
  assert(VP8VFilter8i != NULL);
  assert(VP8HFilter8i != NULL);
  assert(VP8SimpleVFilter16 != NULL);
  assert(VP8SimpleHFilter16 != NULL);
  assert(VP8SimpleVFilter16i != NULL);
  assert(VP8SimpleHFilter16i != NULL);
  assert(VP8PredLuma4[0] != NULL);
  assert(VP8PredLuma4[1] != NULL);
  assert(VP8PredLuma4[2] != NULL);
  assert(VP8PredLuma4[3] != NULL);
  assert(VP8PredLuma4[4] != NULL);
  assert(VP8PredLuma4[5] != NULL);
  assert(VP8PredLuma4[6] != NULL);
  assert(VP8PredLuma4[7] != NULL);
  assert(VP8PredLuma4[8] != NULL);
  assert(VP8PredLuma4[9] != NULL);
  assert(VP8PredLuma16[0] != NULL);
  assert(VP8PredLuma16[1] != NULL);
  assert(VP8PredLuma16[2] != NULL);
  assert(VP8PredLuma16[3] != NULL);
  assert(VP8PredLuma16[4] != NULL);
  assert(VP8PredLuma16[5] != NULL);
  assert(VP8PredLuma16[6] != NULL);
  assert(VP8PredChroma8[0] != NULL);
  assert(VP8PredChroma8[1] != NULL);
  assert(VP8PredChroma8[2] != NULL);
  assert(VP8PredChroma8[3] != NULL);
  assert(VP8PredChroma8[4] != NULL);
  assert(VP8PredChroma8[5] != NULL);
  assert(VP8PredChroma8[6] != NULL);
  assert(VP8DitherCombine8x8 != NULL);
}
