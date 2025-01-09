// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS version of dsp functions
//
// Author(s):  Djordje Pesut    (djordje.pesut@imgtec.com)
//             Jovan Zelincevic (jovan.zelincevic@imgtec.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MIPS32)

#include "src/dsp/mips_macro.h"

static const int kC1 = WEBP_TRANSFORM_AC3_C1;
static const int kC2 = WEBP_TRANSFORM_AC3_C2;

static WEBP_INLINE int abs_mips32(int x) {
  const int sign = x >> 31;
  return (x ^ sign) - sign;
}

// 4 pixels in, 2 pixels out
static WEBP_INLINE void do_filter2(uint8_t* p, int step) {
  const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0) + VP8ksclip1[p1 - q1];
  const int a1 = VP8ksclip2[(a + 4) >> 3];
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  p[-step] = VP8kclip1[p0 + a2];
  p[    0] = VP8kclip1[q0 - a1];
}

// 4 pixels in, 4 pixels out
static WEBP_INLINE void do_filter4(uint8_t* p, int step) {
  const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0);
  const int a1 = VP8ksclip2[(a + 4) >> 3];
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  const int a3 = (a1 + 1) >> 1;
  p[-2 * step] = VP8kclip1[p1 + a3];
  p[-    step] = VP8kclip1[p0 + a2];
  p[        0] = VP8kclip1[q0 - a1];
  p[     step] = VP8kclip1[q1 - a3];
}

// 6 pixels in, 6 pixels out
static WEBP_INLINE void do_filter6(uint8_t* p, int step) {
  const int p2 = p[-3 * step], p1 = p[-2 * step], p0 = p[-step];
  const int q0 = p[0], q1 = p[step], q2 = p[2 * step];
  const int a = VP8ksclip1[3 * (q0 - p0) + VP8ksclip1[p1 - q1]];
  // a is in [-128,127], a1 in [-27,27], a2 in [-18,18] and a3 in [-9,9]
  const int a1 = (27 * a + 63) >> 7;  // eq. to ((3 * a + 7) * 9) >> 7
  const int a2 = (18 * a + 63) >> 7;  // eq. to ((2 * a + 7) * 9) >> 7
  const int a3 = (9  * a + 63) >> 7;  // eq. to ((1 * a + 7) * 9) >> 7
  p[-3 * step] = VP8kclip1[p2 + a3];
  p[-2 * step] = VP8kclip1[p1 + a2];
  p[-    step] = VP8kclip1[p0 + a1];
  p[        0] = VP8kclip1[q0 - a1];
  p[     step] = VP8kclip1[q1 - a2];
  p[ 2 * step] = VP8kclip1[q2 - a3];
}

static WEBP_INLINE int hev(const uint8_t* p, int step, int thresh) {
  const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return (abs_mips32(p1 - p0) > thresh) || (abs_mips32(q1 - q0) > thresh);
}

static WEBP_INLINE int needs_filter(const uint8_t* p, int step, int t) {
  const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return ((4 * abs_mips32(p0 - q0) + abs_mips32(p1 - q1)) <= t);
}

static WEBP_INLINE int needs_filter2(const uint8_t* p,
                                     int step, int t, int it) {
  const int p3 = p[-4 * step], p2 = p[-3 * step];
  const int p1 = p[-2 * step], p0 = p[-step];
  const int q0 = p[0], q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
  if ((4 * abs_mips32(p0 - q0) + abs_mips32(p1 - q1)) > t) {
    return 0;
  }
  return abs_mips32(p3 - p2) <= it && abs_mips32(p2 - p1) <= it &&
         abs_mips32(p1 - p0) <= it && abs_mips32(q3 - q2) <= it &&
         abs_mips32(q2 - q1) <= it && abs_mips32(q1 - q0) <= it;
}

static WEBP_INLINE void FilterLoop26(uint8_t* p,
                                     int hstride, int vstride, int size,
                                     int thresh, int ithresh, int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  while (size-- > 0) {
    if (needs_filter2(p, hstride, thresh2, ithresh)) {
      if (hev(p, hstride, hev_thresh)) {
        do_filter2(p, hstride);
      } else {
        do_filter6(p, hstride);
      }
    }
    p += vstride;
  }
}

static WEBP_INLINE void FilterLoop24(uint8_t* p,
                                     int hstride, int vstride, int size,
                                     int thresh, int ithresh, int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  while (size-- > 0) {
    if (needs_filter2(p, hstride, thresh2, ithresh)) {
      if (hev(p, hstride, hev_thresh)) {
        do_filter2(p, hstride);
      } else {
        do_filter4(p, hstride);
      }
    }
    p += vstride;
  }
}

// on macroblock edges
static void VFilter16(uint8_t* p, int stride,
                      int thresh, int ithresh, int hev_thresh) {
  FilterLoop26(p, stride, 1, 16, thresh, ithresh, hev_thresh);
}

static void HFilter16(uint8_t* p, int stride,
                      int thresh, int ithresh, int hev_thresh) {
  FilterLoop26(p, 1, stride, 16, thresh, ithresh, hev_thresh);
}

// 8-pixels wide variant, for chroma filtering
static void VFilter8(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                     int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop26(u, stride, 1, 8, thresh, ithresh, hev_thresh);
  FilterLoop26(v, stride, 1, 8, thresh, ithresh, hev_thresh);
}

static void HFilter8(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                     int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop26(u, 1, stride, 8, thresh, ithresh, hev_thresh);
  FilterLoop26(v, 1, stride, 8, thresh, ithresh, hev_thresh);
}

static void VFilter8i(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                      int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop24(u + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
  FilterLoop24(v + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
}

static void HFilter8i(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                      int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop24(u + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
  FilterLoop24(v + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
}

// on three inner edges
static void VFilter16i(uint8_t* p, int stride,
                       int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    FilterLoop24(p, stride, 1, 16, thresh, ithresh, hev_thresh);
  }
}

static void HFilter16i(uint8_t* p, int stride,
                       int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    FilterLoop24(p, 1, stride, 16, thresh, ithresh, hev_thresh);
  }
}

//------------------------------------------------------------------------------
// Simple In-loop filtering (Paragraph 15.2)

static void SimpleVFilter16(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (needs_filter(p + i, stride, thresh2)) {
      do_filter2(p + i, stride);
    }
  }
}

static void SimpleHFilter16(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (needs_filter(p + i * stride, 1, thresh2)) {
      do_filter2(p + i * stride, 1);
    }
  }
}

static void SimpleVFilter16i(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16(p, stride, thresh);
  }
}

static void SimpleHFilter16i(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16(p, stride, thresh);
  }
}

static void TransformOne(const int16_t* WEBP_RESTRICT in,
                         uint8_t* WEBP_RESTRICT dst) {
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8, temp9;
  int temp10, temp11, temp12, temp13, temp14;
  int temp15, temp16, temp17, temp18, temp19;
  int16_t* p_in = (int16_t*)in;

  // loops unrolled and merged to avoid usage of tmp buffer
  // and to reduce number of stalls. MUL macro is written
  // in assembler and inlined
  __asm__ volatile(
    "lh       %[temp0],  0(%[in])                      \n\t"
    "lh       %[temp8],  16(%[in])                     \n\t"
    "lh       %[temp4],  8(%[in])                      \n\t"
    "lh       %[temp12], 24(%[in])                     \n\t"
    "addu     %[temp16], %[temp0],  %[temp8]           \n\t"
    "subu     %[temp0],  %[temp0],  %[temp8]           \n\t"
    "mul      %[temp8],  %[temp4],  %[kC2]             \n\t"
    MUL_SHIFT_C1(temp17, temp12)
    MUL_SHIFT_C1_IO(temp4, temp19)
    "mul      %[temp12], %[temp12], %[kC2]             \n\t"
    "lh       %[temp1],  2(%[in])                      \n\t"
    "lh       %[temp5],  10(%[in])                     \n\t"
    "lh       %[temp9],  18(%[in])                     \n\t"
    "lh       %[temp13], 26(%[in])                     \n\t"
    "sra      %[temp8],  %[temp8],  16                 \n\t"
    "sra      %[temp12], %[temp12], 16                 \n\t"
    "lh       %[temp2],  4(%[in])                      \n\t"
    "lh       %[temp6],  12(%[in])                     \n\t"
    "lh       %[temp10], 20(%[in])                     \n\t"
    "lh       %[temp14], 28(%[in])                     \n\t"
    "subu     %[temp17], %[temp8],  %[temp17]          \n\t"
    "addu     %[temp4],  %[temp4],  %[temp12]          \n\t"
    "addu     %[temp8],  %[temp16], %[temp4]           \n\t"
    "subu     %[temp4],  %[temp16], %[temp4]           \n\t"
    "addu     %[temp16], %[temp1],  %[temp9]           \n\t"
    "subu     %[temp1],  %[temp1],  %[temp9]           \n\t"
    "lh       %[temp3],  6(%[in])                      \n\t"
    "lh       %[temp7],  14(%[in])                     \n\t"
    "lh       %[temp11], 22(%[in])                     \n\t"
    "lh       %[temp15], 30(%[in])                     \n\t"
    "addu     %[temp12], %[temp0],  %[temp17]          \n\t"
    "subu     %[temp0],  %[temp0],  %[temp17]          \n\t"
    "mul      %[temp9],  %[temp5],  %[kC2]             \n\t"
    MUL_SHIFT_C1(temp17, temp13)
    MUL_SHIFT_C1_IO(temp5, temp19)
    "mul      %[temp13], %[temp13], %[kC2]             \n\t"
    "sra      %[temp9],  %[temp9],  16                 \n\t"
    "subu     %[temp17], %[temp9],  %[temp17]          \n\t"
    "sra      %[temp13], %[temp13], 16                 \n\t"
    "addu     %[temp5],  %[temp5],  %[temp13]          \n\t"
    "addu     %[temp13], %[temp1],  %[temp17]          \n\t"
    "subu     %[temp1],  %[temp1],  %[temp17]          \n\t"
    MUL_SHIFT_C1(temp17, temp14)
    "mul      %[temp14], %[temp14], %[kC2]             \n\t"
    "addu     %[temp9],  %[temp16], %[temp5]           \n\t"
    "subu     %[temp5],  %[temp16], %[temp5]           \n\t"
    "addu     %[temp16], %[temp2],  %[temp10]          \n\t"
    "subu     %[temp2],  %[temp2],  %[temp10]          \n\t"
    "mul      %[temp10], %[temp6],  %[kC2]             \n\t"
    MUL_SHIFT_C1_IO(temp6, temp19)
    "sra      %[temp14], %[temp14], 16                 \n\t"
    "sra      %[temp10], %[temp10], 16                 \n\t"
    "subu     %[temp17], %[temp10], %[temp17]          \n\t"
    "addu     %[temp6],  %[temp6],  %[temp14]          \n\t"
    "addu     %[temp10], %[temp16], %[temp6]           \n\t"
    "subu     %[temp6],  %[temp16], %[temp6]           \n\t"
    "addu     %[temp14], %[temp2],  %[temp17]          \n\t"
    "subu     %[temp2],  %[temp2],  %[temp17]          \n\t"
    MUL_SHIFT_C1(temp17, temp15)
    "mul      %[temp15], %[temp15], %[kC2]             \n\t"
    "addu     %[temp16], %[temp3],  %[temp11]          \n\t"
    "subu     %[temp3],  %[temp3],  %[temp11]          \n\t"
    "mul      %[temp11], %[temp7],  %[kC2]             \n\t"
    MUL_SHIFT_C1_IO(temp7, temp19)
    "addiu    %[temp8],  %[temp8],  4                  \n\t"
    "addiu    %[temp12], %[temp12], 4                  \n\t"
    "addiu    %[temp0],  %[temp0],  4                  \n\t"
    "addiu    %[temp4],  %[temp4],  4                  \n\t"
    "sra      %[temp15], %[temp15], 16                 \n\t"
    "sra      %[temp11], %[temp11], 16                 \n\t"
    "subu     %[temp17], %[temp11], %[temp17]          \n\t"
    "addu     %[temp7],  %[temp7],  %[temp15]          \n\t"
    "addu     %[temp15], %[temp3],  %[temp17]          \n\t"
    "subu     %[temp3],  %[temp3],  %[temp17]          \n\t"
    "addu     %[temp11], %[temp16], %[temp7]           \n\t"
    "subu     %[temp7],  %[temp16], %[temp7]           \n\t"
    "addu     %[temp16], %[temp8],  %[temp10]          \n\t"
    "subu     %[temp8],  %[temp8],  %[temp10]          \n\t"
    "mul      %[temp10], %[temp9],  %[kC2]             \n\t"
    MUL_SHIFT_C1(temp17, temp11)
    MUL_SHIFT_C1_IO(temp9, temp19)
    "mul      %[temp11], %[temp11], %[kC2]             \n\t"
    "sra      %[temp10], %[temp10], 16                 \n\t"
    "sra      %[temp11], %[temp11], 16                 \n\t"
    "subu     %[temp17], %[temp10], %[temp17]          \n\t"
    "addu     %[temp11], %[temp9],  %[temp11]          \n\t"
    "addu     %[temp10], %[temp12], %[temp14]          \n\t"
    "subu     %[temp12], %[temp12], %[temp14]          \n\t"
    "mul      %[temp14], %[temp13], %[kC2]             \n\t"
    MUL_SHIFT_C1(temp9, temp15)
    MUL_SHIFT_C1_IO(temp13, temp19)
    "mul      %[temp15], %[temp15], %[kC2]             \n\t"
    "sra      %[temp14], %[temp14], 16                 \n\t"
    "sra      %[temp15], %[temp15], 16                 \n\t"
    "subu     %[temp9],  %[temp14], %[temp9]           \n\t"
    "addu     %[temp15], %[temp13], %[temp15]          \n\t"
    "addu     %[temp14], %[temp0],  %[temp2]           \n\t"
    "subu     %[temp0],  %[temp0],  %[temp2]           \n\t"
    "mul      %[temp2],  %[temp1],  %[kC2]             \n\t"
    MUL_SHIFT_C1(temp13, temp3)
    MUL_SHIFT_C1_IO(temp1, temp19)
    "mul      %[temp3],  %[temp3],  %[kC2]             \n\t"
    "sra      %[temp2],  %[temp2],  16                 \n\t"
    "sra      %[temp3],  %[temp3],  16                 \n\t"
    "subu     %[temp13], %[temp2],  %[temp13]          \n\t"
    "addu     %[temp3],  %[temp1],  %[temp3]           \n\t"
    "addu     %[temp2],  %[temp4],  %[temp6]           \n\t"
    "subu     %[temp4],  %[temp4],  %[temp6]           \n\t"
    "mul      %[temp6],  %[temp5],  %[kC2]             \n\t"
    MUL_SHIFT_C1(temp1, temp7)
    MUL_SHIFT_C1_IO(temp5, temp19)
    "mul      %[temp7],  %[temp7],  %[kC2]             \n\t"
    "sra      %[temp6],  %[temp6],  16                 \n\t"
    "sra      %[temp7],  %[temp7],  16                 \n\t"
    "subu     %[temp1],  %[temp6],  %[temp1]           \n\t"
    "addu     %[temp7],  %[temp5],  %[temp7]           \n\t"
    "addu     %[temp5],  %[temp16], %[temp11]          \n\t"
    "subu     %[temp16], %[temp16], %[temp11]          \n\t"
    "addu     %[temp11], %[temp8],  %[temp17]          \n\t"
    "subu     %[temp8],  %[temp8],  %[temp17]          \n\t"
    "sra      %[temp5],  %[temp5],  3                  \n\t"
    "sra      %[temp16], %[temp16], 3                  \n\t"
    "sra      %[temp11], %[temp11], 3                  \n\t"
    "sra      %[temp8],  %[temp8],  3                  \n\t"
    "addu     %[temp17], %[temp10], %[temp15]          \n\t"
    "subu     %[temp10], %[temp10], %[temp15]          \n\t"
    "addu     %[temp15], %[temp12], %[temp9]           \n\t"
    "subu     %[temp12], %[temp12], %[temp9]           \n\t"
    "sra      %[temp17], %[temp17], 3                  \n\t"
    "sra      %[temp10], %[temp10], 3                  \n\t"
    "sra      %[temp15], %[temp15], 3                  \n\t"
    "sra      %[temp12], %[temp12], 3                  \n\t"
    "addu     %[temp9],  %[temp14], %[temp3]           \n\t"
    "subu     %[temp14], %[temp14], %[temp3]           \n\t"
    "addu     %[temp3],  %[temp0],  %[temp13]          \n\t"
    "subu     %[temp0],  %[temp0],  %[temp13]          \n\t"
    "sra      %[temp9],  %[temp9],  3                  \n\t"
    "sra      %[temp14], %[temp14], 3                  \n\t"
    "sra      %[temp3],  %[temp3],  3                  \n\t"
    "sra      %[temp0],  %[temp0],  3                  \n\t"
    "addu     %[temp13], %[temp2],  %[temp7]           \n\t"
    "subu     %[temp2],  %[temp2],  %[temp7]           \n\t"
    "addu     %[temp7],  %[temp4],  %[temp1]           \n\t"
    "subu     %[temp4],  %[temp4],  %[temp1]           \n\t"
    "sra      %[temp13], %[temp13], 3                  \n\t"
    "sra      %[temp2],  %[temp2],  3                  \n\t"
    "sra      %[temp7],  %[temp7],  3                  \n\t"
    "sra      %[temp4],  %[temp4],  3                  \n\t"
    "addiu    %[temp6],  $zero,     255                \n\t"
    "lbu      %[temp1],  0+0*" XSTR(BPS) "(%[dst])     \n\t"
    "addu     %[temp1],  %[temp1],  %[temp5]           \n\t"
    "sra      %[temp5],  %[temp1],  8                  \n\t"
    "sra      %[temp18], %[temp1],  31                 \n\t"
    "beqz     %[temp5],  1f                            \n\t"
    "xor      %[temp1],  %[temp1],  %[temp1]           \n\t"
    "movz     %[temp1],  %[temp6],  %[temp18]          \n\t"
  "1:                                                  \n\t"
    "lbu      %[temp18], 1+0*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp1],  0+0*" XSTR(BPS) "(%[dst])     \n\t"
    "addu     %[temp18], %[temp18], %[temp11]          \n\t"
    "sra      %[temp11], %[temp18], 8                  \n\t"
    "sra      %[temp1],  %[temp18], 31                 \n\t"
    "beqz     %[temp11], 2f                            \n\t"
    "xor      %[temp18], %[temp18], %[temp18]          \n\t"
    "movz     %[temp18], %[temp6],  %[temp1]           \n\t"
  "2:                                                  \n\t"
    "lbu      %[temp1],  2+0*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp18], 1+0*" XSTR(BPS) "(%[dst])     \n\t"
    "addu     %[temp1],  %[temp1],  %[temp8]           \n\t"
    "sra      %[temp8],  %[temp1],  8                  \n\t"
    "sra      %[temp18], %[temp1],  31                 \n\t"
    "beqz     %[temp8],  3f                            \n\t"
    "xor      %[temp1],  %[temp1],  %[temp1]           \n\t"
    "movz     %[temp1],  %[temp6],  %[temp18]          \n\t"
  "3:                                                  \n\t"
    "lbu      %[temp18], 3+0*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp1],  2+0*" XSTR(BPS) "(%[dst])     \n\t"
    "addu     %[temp18], %[temp18], %[temp16]          \n\t"
    "sra      %[temp16], %[temp18], 8                  \n\t"
    "sra      %[temp1],  %[temp18], 31                 \n\t"
    "beqz     %[temp16], 4f                            \n\t"
    "xor      %[temp18], %[temp18], %[temp18]          \n\t"
    "movz     %[temp18], %[temp6],  %[temp1]           \n\t"
  "4:                                                  \n\t"
    "sb       %[temp18], 3+0*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp5],  0+1*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp8],  1+1*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp11], 2+1*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp16], 3+1*" XSTR(BPS) "(%[dst])     \n\t"
    "addu     %[temp5],  %[temp5],  %[temp17]          \n\t"
    "addu     %[temp8],  %[temp8],  %[temp15]          \n\t"
    "addu     %[temp11], %[temp11], %[temp12]          \n\t"
    "addu     %[temp16], %[temp16], %[temp10]          \n\t"
    "sra      %[temp18], %[temp5],  8                  \n\t"
    "sra      %[temp1],  %[temp5],  31                 \n\t"
    "beqz     %[temp18], 5f                            \n\t"
    "xor      %[temp5],  %[temp5],  %[temp5]           \n\t"
    "movz     %[temp5],  %[temp6],  %[temp1]           \n\t"
  "5:                                                  \n\t"
    "sra      %[temp18], %[temp8],  8                  \n\t"
    "sra      %[temp1],  %[temp8],  31                 \n\t"
    "beqz     %[temp18], 6f                            \n\t"
    "xor      %[temp8],  %[temp8],  %[temp8]           \n\t"
    "movz     %[temp8],  %[temp6],  %[temp1]           \n\t"
  "6:                                                  \n\t"
    "sra      %[temp18], %[temp11], 8                  \n\t"
    "sra      %[temp1],  %[temp11], 31                 \n\t"
    "sra      %[temp17], %[temp16], 8                  \n\t"
    "sra      %[temp15], %[temp16], 31                 \n\t"
    "beqz     %[temp18], 7f                            \n\t"
    "xor      %[temp11], %[temp11], %[temp11]          \n\t"
    "movz     %[temp11], %[temp6],  %[temp1]           \n\t"
  "7:                                                  \n\t"
    "beqz     %[temp17], 8f                            \n\t"
    "xor      %[temp16], %[temp16], %[temp16]          \n\t"
    "movz     %[temp16], %[temp6],  %[temp15]          \n\t"
  "8:                                                  \n\t"
    "sb       %[temp5],  0+1*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp8],  1+1*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp11], 2+1*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp16], 3+1*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp5],  0+2*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp8],  1+2*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp11], 2+2*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp16], 3+2*" XSTR(BPS) "(%[dst])     \n\t"
    "addu     %[temp5],  %[temp5],  %[temp9]           \n\t"
    "addu     %[temp8],  %[temp8],  %[temp3]           \n\t"
    "addu     %[temp11], %[temp11], %[temp0]           \n\t"
    "addu     %[temp16], %[temp16], %[temp14]          \n\t"
    "sra      %[temp18], %[temp5],  8                  \n\t"
    "sra      %[temp1],  %[temp5],  31                 \n\t"
    "sra      %[temp17], %[temp8],  8                  \n\t"
    "sra      %[temp15], %[temp8],  31                 \n\t"
    "sra      %[temp12], %[temp11], 8                  \n\t"
    "sra      %[temp10], %[temp11], 31                 \n\t"
    "sra      %[temp9],  %[temp16], 8                  \n\t"
    "sra      %[temp3],  %[temp16], 31                 \n\t"
    "beqz     %[temp18], 9f                            \n\t"
    "xor      %[temp5],  %[temp5],  %[temp5]           \n\t"
    "movz     %[temp5],  %[temp6],  %[temp1]           \n\t"
  "9:                                                  \n\t"
    "beqz     %[temp17], 10f                           \n\t"
    "xor      %[temp8],  %[temp8],  %[temp8]           \n\t"
    "movz     %[temp8],  %[temp6],  %[temp15]          \n\t"
  "10:                                                 \n\t"
    "beqz     %[temp12], 11f                           \n\t"
    "xor      %[temp11], %[temp11], %[temp11]          \n\t"
    "movz     %[temp11], %[temp6],  %[temp10]          \n\t"
  "11:                                                 \n\t"
    "beqz     %[temp9],  12f                           \n\t"
    "xor      %[temp16], %[temp16], %[temp16]          \n\t"
    "movz     %[temp16], %[temp6],  %[temp3]           \n\t"
  "12:                                                 \n\t"
    "sb       %[temp5],  0+2*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp8],  1+2*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp11], 2+2*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp16], 3+2*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp5],  0+3*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp8],  1+3*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp11], 2+3*" XSTR(BPS) "(%[dst])     \n\t"
    "lbu      %[temp16], 3+3*" XSTR(BPS) "(%[dst])     \n\t"
    "addu     %[temp5],  %[temp5],  %[temp13]          \n\t"
    "addu     %[temp8],  %[temp8],  %[temp7]           \n\t"
    "addu     %[temp11], %[temp11], %[temp4]           \n\t"
    "addu     %[temp16], %[temp16], %[temp2]           \n\t"
    "sra      %[temp18], %[temp5],  8                  \n\t"
    "sra      %[temp1],  %[temp5],  31                 \n\t"
    "sra      %[temp17], %[temp8],  8                  \n\t"
    "sra      %[temp15], %[temp8],  31                 \n\t"
    "sra      %[temp12], %[temp11], 8                  \n\t"
    "sra      %[temp10], %[temp11], 31                 \n\t"
    "sra      %[temp9],  %[temp16], 8                  \n\t"
    "sra      %[temp3],  %[temp16], 31                 \n\t"
    "beqz     %[temp18], 13f                           \n\t"
    "xor      %[temp5],  %[temp5],  %[temp5]           \n\t"
    "movz     %[temp5],  %[temp6],  %[temp1]           \n\t"
  "13:                                                 \n\t"
    "beqz     %[temp17], 14f                           \n\t"
    "xor      %[temp8],  %[temp8],  %[temp8]           \n\t"
    "movz     %[temp8],  %[temp6],  %[temp15]          \n\t"
  "14:                                                 \n\t"
    "beqz     %[temp12], 15f                           \n\t"
    "xor      %[temp11], %[temp11], %[temp11]          \n\t"
    "movz     %[temp11], %[temp6],  %[temp10]          \n\t"
  "15:                                                 \n\t"
    "beqz     %[temp9],  16f                           \n\t"
    "xor      %[temp16], %[temp16], %[temp16]          \n\t"
    "movz     %[temp16], %[temp6],  %[temp3]           \n\t"
  "16:                                                 \n\t"
    "sb       %[temp5],  0+3*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp8],  1+3*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp11], 2+3*" XSTR(BPS) "(%[dst])     \n\t"
    "sb       %[temp16], 3+3*" XSTR(BPS) "(%[dst])     \n\t"

    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9), [temp10]"=&r"(temp10), [temp11]"=&r"(temp11),
      [temp12]"=&r"(temp12), [temp13]"=&r"(temp13), [temp14]"=&r"(temp14),
      [temp15]"=&r"(temp15), [temp16]"=&r"(temp16), [temp17]"=&r"(temp17),
      [temp18]"=&r"(temp18), [temp19]"=&r"(temp19)
    : [in]"r"(p_in), [kC1]"r"(kC1), [kC2]"r"(kC2), [dst]"r"(dst)
    : "memory", "hi", "lo"
  );
}

static void TransformTwo(const int16_t* WEBP_RESTRICT in,
                         uint8_t* WEBP_RESTRICT dst, int do_two) {
  TransformOne(in, dst);
  if (do_two) {
    TransformOne(in + 16, dst + 4);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitMIPS32(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitMIPS32(void) {
  VP8InitClipTables();

  VP8Transform = TransformTwo;

  VP8VFilter16 = VFilter16;
  VP8HFilter16 = HFilter16;
  VP8VFilter8 = VFilter8;
  VP8HFilter8 = HFilter8;
  VP8VFilter16i = VFilter16i;
  VP8HFilter16i = HFilter16i;
  VP8VFilter8i = VFilter8i;
  VP8HFilter8i = HFilter8i;

  VP8SimpleVFilter16 = SimpleVFilter16;
  VP8SimpleHFilter16 = SimpleHFilter16;
  VP8SimpleVFilter16i = SimpleVFilter16i;
  VP8SimpleHFilter16i = SimpleHFilter16i;
}

#else  // !WEBP_USE_MIPS32

WEBP_DSP_INIT_STUB(VP8DspInitMIPS32)

#endif  // WEBP_USE_MIPS32
