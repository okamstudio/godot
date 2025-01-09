// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// ARM NEON version of dsp functions and loop filtering.
//
// Authors: Somnath Banerjee (somnath@google.com)
//          Johann Koenig (johannkoenig@google.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_NEON)

#include "src/dsp/neon.h"
#include "src/dec/vp8i_dec.h"

//------------------------------------------------------------------------------
// NxM Loading functions

#if !defined(WORK_AROUND_GCC)

// This intrinsics version makes gcc-4.6.3 crash during Load4x??() compilation
// (register alloc, probably). The variants somewhat mitigate the problem, but
// not quite. HFilter16i() remains problematic.
static WEBP_INLINE uint8x8x4_t Load4x8_NEON(const uint8_t* const src,
                                            int stride) {
  const uint8x8_t zero = vdup_n_u8(0);
  uint8x8x4_t out;
  INIT_VECTOR4(out, zero, zero, zero, zero);
  out = vld4_lane_u8(src + 0 * stride, out, 0);
  out = vld4_lane_u8(src + 1 * stride, out, 1);
  out = vld4_lane_u8(src + 2 * stride, out, 2);
  out = vld4_lane_u8(src + 3 * stride, out, 3);
  out = vld4_lane_u8(src + 4 * stride, out, 4);
  out = vld4_lane_u8(src + 5 * stride, out, 5);
  out = vld4_lane_u8(src + 6 * stride, out, 6);
  out = vld4_lane_u8(src + 7 * stride, out, 7);
  return out;
}

static WEBP_INLINE void Load4x16_NEON(const uint8_t* const src, int stride,
                                      uint8x16_t* const p1,
                                      uint8x16_t* const p0,
                                      uint8x16_t* const q0,
                                      uint8x16_t* const q1) {
  // row0 = p1[0..7]|p0[0..7]|q0[0..7]|q1[0..7]
  // row8 = p1[8..15]|p0[8..15]|q0[8..15]|q1[8..15]
  const uint8x8x4_t row0 = Load4x8_NEON(src - 2 + 0 * stride, stride);
  const uint8x8x4_t row8 = Load4x8_NEON(src - 2 + 8 * stride, stride);
  *p1 = vcombine_u8(row0.val[0], row8.val[0]);
  *p0 = vcombine_u8(row0.val[1], row8.val[1]);
  *q0 = vcombine_u8(row0.val[2], row8.val[2]);
  *q1 = vcombine_u8(row0.val[3], row8.val[3]);
}

#else  // WORK_AROUND_GCC

#define LOADQ_LANE_32b(VALUE, LANE) do {                             \
  (VALUE) = vld1q_lane_u32((const uint32_t*)src, (VALUE), (LANE));   \
  src += stride;                                                     \
} while (0)

static WEBP_INLINE void Load4x16_NEON(const uint8_t* src, int stride,
                                      uint8x16_t* const p1,
                                      uint8x16_t* const p0,
                                      uint8x16_t* const q0,
                                      uint8x16_t* const q1) {
  const uint32x4_t zero = vdupq_n_u32(0);
  uint32x4x4_t in;
  INIT_VECTOR4(in, zero, zero, zero, zero);
  src -= 2;
  LOADQ_LANE_32b(in.val[0], 0);
  LOADQ_LANE_32b(in.val[1], 0);
  LOADQ_LANE_32b(in.val[2], 0);
  LOADQ_LANE_32b(in.val[3], 0);
  LOADQ_LANE_32b(in.val[0], 1);
  LOADQ_LANE_32b(in.val[1], 1);
  LOADQ_LANE_32b(in.val[2], 1);
  LOADQ_LANE_32b(in.val[3], 1);
  LOADQ_LANE_32b(in.val[0], 2);
  LOADQ_LANE_32b(in.val[1], 2);
  LOADQ_LANE_32b(in.val[2], 2);
  LOADQ_LANE_32b(in.val[3], 2);
  LOADQ_LANE_32b(in.val[0], 3);
  LOADQ_LANE_32b(in.val[1], 3);
  LOADQ_LANE_32b(in.val[2], 3);
  LOADQ_LANE_32b(in.val[3], 3);
  // Transpose four 4x4 parts:
  {
    const uint8x16x2_t row01 = vtrnq_u8(vreinterpretq_u8_u32(in.val[0]),
                                        vreinterpretq_u8_u32(in.val[1]));
    const uint8x16x2_t row23 = vtrnq_u8(vreinterpretq_u8_u32(in.val[2]),
                                        vreinterpretq_u8_u32(in.val[3]));
    const uint16x8x2_t row02 = vtrnq_u16(vreinterpretq_u16_u8(row01.val[0]),
                                         vreinterpretq_u16_u8(row23.val[0]));
    const uint16x8x2_t row13 = vtrnq_u16(vreinterpretq_u16_u8(row01.val[1]),
                                         vreinterpretq_u16_u8(row23.val[1]));
    *p1 = vreinterpretq_u8_u16(row02.val[0]);
    *p0 = vreinterpretq_u8_u16(row13.val[0]);
    *q0 = vreinterpretq_u8_u16(row02.val[1]);
    *q1 = vreinterpretq_u8_u16(row13.val[1]);
  }
}
#undef LOADQ_LANE_32b

#endif  // !WORK_AROUND_GCC

static WEBP_INLINE void Load8x16_NEON(
    const uint8_t* const src, int stride,
    uint8x16_t* const p3, uint8x16_t* const p2, uint8x16_t* const p1,
    uint8x16_t* const p0, uint8x16_t* const q0, uint8x16_t* const q1,
    uint8x16_t* const q2, uint8x16_t* const q3) {
  Load4x16_NEON(src - 2, stride, p3, p2, p1, p0);
  Load4x16_NEON(src + 2, stride, q0, q1, q2, q3);
}

static WEBP_INLINE void Load16x4_NEON(const uint8_t* const src, int stride,
                                      uint8x16_t* const p1,
                                      uint8x16_t* const p0,
                                      uint8x16_t* const q0,
                                      uint8x16_t* const q1) {
  *p1 = vld1q_u8(src - 2 * stride);
  *p0 = vld1q_u8(src - 1 * stride);
  *q0 = vld1q_u8(src + 0 * stride);
  *q1 = vld1q_u8(src + 1 * stride);
}

static WEBP_INLINE void Load16x8_NEON(
    const uint8_t* const src, int stride,
    uint8x16_t* const p3, uint8x16_t* const p2, uint8x16_t* const p1,
    uint8x16_t* const p0, uint8x16_t* const q0, uint8x16_t* const q1,
    uint8x16_t* const q2, uint8x16_t* const q3) {
  Load16x4_NEON(src - 2  * stride, stride, p3, p2, p1, p0);
  Load16x4_NEON(src + 2  * stride, stride, q0, q1, q2, q3);
}

static WEBP_INLINE void Load8x8x2_NEON(
    const uint8_t* const u, const uint8_t* const v, int stride,
    uint8x16_t* const p3, uint8x16_t* const p2, uint8x16_t* const p1,
    uint8x16_t* const p0, uint8x16_t* const q0, uint8x16_t* const q1,
    uint8x16_t* const q2, uint8x16_t* const q3) {
  // We pack the 8x8 u-samples in the lower half of the uint8x16_t destination
  // and the v-samples on the higher half.
  *p3 = vcombine_u8(vld1_u8(u - 4 * stride), vld1_u8(v - 4 * stride));
  *p2 = vcombine_u8(vld1_u8(u - 3 * stride), vld1_u8(v - 3 * stride));
  *p1 = vcombine_u8(vld1_u8(u - 2 * stride), vld1_u8(v - 2 * stride));
  *p0 = vcombine_u8(vld1_u8(u - 1 * stride), vld1_u8(v - 1 * stride));
  *q0 = vcombine_u8(vld1_u8(u + 0 * stride), vld1_u8(v + 0 * stride));
  *q1 = vcombine_u8(vld1_u8(u + 1 * stride), vld1_u8(v + 1 * stride));
  *q2 = vcombine_u8(vld1_u8(u + 2 * stride), vld1_u8(v + 2 * stride));
  *q3 = vcombine_u8(vld1_u8(u + 3 * stride), vld1_u8(v + 3 * stride));
}

#if !defined(WORK_AROUND_GCC)

#define LOAD_UV_8(ROW) \
  vcombine_u8(vld1_u8(u - 4 + (ROW) * stride), vld1_u8(v - 4 + (ROW) * stride))

static WEBP_INLINE void Load8x8x2T_NEON(
    const uint8_t* const u, const uint8_t* const v, int stride,
    uint8x16_t* const p3, uint8x16_t* const p2, uint8x16_t* const p1,
    uint8x16_t* const p0, uint8x16_t* const q0, uint8x16_t* const q1,
    uint8x16_t* const q2, uint8x16_t* const q3) {
  // We pack the 8x8 u-samples in the lower half of the uint8x16_t destination
  // and the v-samples on the higher half.
  const uint8x16_t row0 = LOAD_UV_8(0);
  const uint8x16_t row1 = LOAD_UV_8(1);
  const uint8x16_t row2 = LOAD_UV_8(2);
  const uint8x16_t row3 = LOAD_UV_8(3);
  const uint8x16_t row4 = LOAD_UV_8(4);
  const uint8x16_t row5 = LOAD_UV_8(5);
  const uint8x16_t row6 = LOAD_UV_8(6);
  const uint8x16_t row7 = LOAD_UV_8(7);
  // Perform two side-by-side 8x8 transposes
  // u00 u01 u02 u03 u04 u05 u06 u07 | v00 v01 v02 v03 v04 v05 v06 v07
  // u10 u11 u12 u13 u14 u15 u16 u17 | v10 v11 v12 ...
  // u20 u21 u22 u23 u24 u25 u26 u27 | v20 v21 ...
  // u30 u31 u32 u33 u34 u35 u36 u37 | ...
  // u40 u41 u42 u43 u44 u45 u46 u47 | ...
  // u50 u51 u52 u53 u54 u55 u56 u57 | ...
  // u60 u61 u62 u63 u64 u65 u66 u67 | v60 ...
  // u70 u71 u72 u73 u74 u75 u76 u77 | v70 v71 v72 ...
  const uint8x16x2_t row01 = vtrnq_u8(row0, row1);  // u00 u10 u02 u12 ...
                                                    // u01 u11 u03 u13 ...
  const uint8x16x2_t row23 = vtrnq_u8(row2, row3);  // u20 u30 u22 u32 ...
                                                    // u21 u31 u23 u33 ...
  const uint8x16x2_t row45 = vtrnq_u8(row4, row5);  // ...
  const uint8x16x2_t row67 = vtrnq_u8(row6, row7);  // ...
  const uint16x8x2_t row02 = vtrnq_u16(vreinterpretq_u16_u8(row01.val[0]),
                                       vreinterpretq_u16_u8(row23.val[0]));
  const uint16x8x2_t row13 = vtrnq_u16(vreinterpretq_u16_u8(row01.val[1]),
                                       vreinterpretq_u16_u8(row23.val[1]));
  const uint16x8x2_t row46 = vtrnq_u16(vreinterpretq_u16_u8(row45.val[0]),
                                       vreinterpretq_u16_u8(row67.val[0]));
  const uint16x8x2_t row57 = vtrnq_u16(vreinterpretq_u16_u8(row45.val[1]),
                                       vreinterpretq_u16_u8(row67.val[1]));
  const uint32x4x2_t row04 = vtrnq_u32(vreinterpretq_u32_u16(row02.val[0]),
                                       vreinterpretq_u32_u16(row46.val[0]));
  const uint32x4x2_t row26 = vtrnq_u32(vreinterpretq_u32_u16(row02.val[1]),
                                       vreinterpretq_u32_u16(row46.val[1]));
  const uint32x4x2_t row15 = vtrnq_u32(vreinterpretq_u32_u16(row13.val[0]),
                                       vreinterpretq_u32_u16(row57.val[0]));
  const uint32x4x2_t row37 = vtrnq_u32(vreinterpretq_u32_u16(row13.val[1]),
                                       vreinterpretq_u32_u16(row57.val[1]));
  *p3 = vreinterpretq_u8_u32(row04.val[0]);
  *p2 = vreinterpretq_u8_u32(row15.val[0]);
  *p1 = vreinterpretq_u8_u32(row26.val[0]);
  *p0 = vreinterpretq_u8_u32(row37.val[0]);
  *q0 = vreinterpretq_u8_u32(row04.val[1]);
  *q1 = vreinterpretq_u8_u32(row15.val[1]);
  *q2 = vreinterpretq_u8_u32(row26.val[1]);
  *q3 = vreinterpretq_u8_u32(row37.val[1]);
}
#undef LOAD_UV_8

#endif  // !WORK_AROUND_GCC

static WEBP_INLINE void Store2x8_NEON(const uint8x8x2_t v,
                                      uint8_t* const dst, int stride) {
  vst2_lane_u8(dst + 0 * stride, v, 0);
  vst2_lane_u8(dst + 1 * stride, v, 1);
  vst2_lane_u8(dst + 2 * stride, v, 2);
  vst2_lane_u8(dst + 3 * stride, v, 3);
  vst2_lane_u8(dst + 4 * stride, v, 4);
  vst2_lane_u8(dst + 5 * stride, v, 5);
  vst2_lane_u8(dst + 6 * stride, v, 6);
  vst2_lane_u8(dst + 7 * stride, v, 7);
}

static WEBP_INLINE void Store2x16_NEON(const uint8x16_t p0, const uint8x16_t q0,
                                       uint8_t* const dst, int stride) {
  uint8x8x2_t lo, hi;
  lo.val[0] = vget_low_u8(p0);
  lo.val[1] = vget_low_u8(q0);
  hi.val[0] = vget_high_u8(p0);
  hi.val[1] = vget_high_u8(q0);
  Store2x8_NEON(lo, dst - 1 + 0 * stride, stride);
  Store2x8_NEON(hi, dst - 1 + 8 * stride, stride);
}

#if !defined(WORK_AROUND_GCC)
static WEBP_INLINE void Store4x8_NEON(const uint8x8x4_t v,
                                      uint8_t* const dst, int stride) {
  vst4_lane_u8(dst + 0 * stride, v, 0);
  vst4_lane_u8(dst + 1 * stride, v, 1);
  vst4_lane_u8(dst + 2 * stride, v, 2);
  vst4_lane_u8(dst + 3 * stride, v, 3);
  vst4_lane_u8(dst + 4 * stride, v, 4);
  vst4_lane_u8(dst + 5 * stride, v, 5);
  vst4_lane_u8(dst + 6 * stride, v, 6);
  vst4_lane_u8(dst + 7 * stride, v, 7);
}

static WEBP_INLINE void Store4x16_NEON(const uint8x16_t p1, const uint8x16_t p0,
                                       const uint8x16_t q0, const uint8x16_t q1,
                                       uint8_t* const dst, int stride) {
  uint8x8x4_t lo, hi;
  INIT_VECTOR4(lo,
               vget_low_u8(p1), vget_low_u8(p0),
               vget_low_u8(q0), vget_low_u8(q1));
  INIT_VECTOR4(hi,
               vget_high_u8(p1), vget_high_u8(p0),
               vget_high_u8(q0), vget_high_u8(q1));
  Store4x8_NEON(lo, dst - 2 + 0 * stride, stride);
  Store4x8_NEON(hi, dst - 2 + 8 * stride, stride);
}
#endif  // !WORK_AROUND_GCC

static WEBP_INLINE void Store16x2_NEON(const uint8x16_t p0, const uint8x16_t q0,
                                       uint8_t* const dst, int stride) {
  vst1q_u8(dst - stride, p0);
  vst1q_u8(dst, q0);
}

static WEBP_INLINE void Store16x4_NEON(const uint8x16_t p1, const uint8x16_t p0,
                                       const uint8x16_t q0, const uint8x16_t q1,
                                       uint8_t* const dst, int stride) {
  Store16x2_NEON(p1, p0, dst - stride, stride);
  Store16x2_NEON(q0, q1, dst + stride, stride);
}

static WEBP_INLINE void Store8x2x2_NEON(const uint8x16_t p0,
                                        const uint8x16_t q0,
                                        uint8_t* const u, uint8_t* const v,
                                        int stride) {
  // p0 and q0 contain the u+v samples packed in low/high halves.
  vst1_u8(u - stride, vget_low_u8(p0));
  vst1_u8(u,          vget_low_u8(q0));
  vst1_u8(v - stride, vget_high_u8(p0));
  vst1_u8(v,          vget_high_u8(q0));
}

static WEBP_INLINE void Store8x4x2_NEON(const uint8x16_t p1,
                                        const uint8x16_t p0,
                                        const uint8x16_t q0,
                                        const uint8x16_t q1,
                                        uint8_t* const u, uint8_t* const v,
                                        int stride) {
  // The p1...q1 registers contain the u+v samples packed in low/high halves.
  Store8x2x2_NEON(p1, p0, u - stride, v - stride, stride);
  Store8x2x2_NEON(q0, q1, u + stride, v + stride, stride);
}

#if !defined(WORK_AROUND_GCC)

#define STORE6_LANE(DST, VAL0, VAL1, LANE) do {   \
  vst3_lane_u8((DST) - 3, (VAL0), (LANE));        \
  vst3_lane_u8((DST) + 0, (VAL1), (LANE));        \
  (DST) += stride;                                \
} while (0)

static WEBP_INLINE void Store6x8x2_NEON(
    const uint8x16_t p2, const uint8x16_t p1, const uint8x16_t p0,
    const uint8x16_t q0, const uint8x16_t q1, const uint8x16_t q2,
    uint8_t* u, uint8_t* v, int stride) {
  uint8x8x3_t u0, u1, v0, v1;
  INIT_VECTOR3(u0, vget_low_u8(p2), vget_low_u8(p1), vget_low_u8(p0));
  INIT_VECTOR3(u1, vget_low_u8(q0), vget_low_u8(q1), vget_low_u8(q2));
  INIT_VECTOR3(v0, vget_high_u8(p2), vget_high_u8(p1), vget_high_u8(p0));
  INIT_VECTOR3(v1, vget_high_u8(q0), vget_high_u8(q1), vget_high_u8(q2));
  STORE6_LANE(u, u0, u1, 0);
  STORE6_LANE(u, u0, u1, 1);
  STORE6_LANE(u, u0, u1, 2);
  STORE6_LANE(u, u0, u1, 3);
  STORE6_LANE(u, u0, u1, 4);
  STORE6_LANE(u, u0, u1, 5);
  STORE6_LANE(u, u0, u1, 6);
  STORE6_LANE(u, u0, u1, 7);
  STORE6_LANE(v, v0, v1, 0);
  STORE6_LANE(v, v0, v1, 1);
  STORE6_LANE(v, v0, v1, 2);
  STORE6_LANE(v, v0, v1, 3);
  STORE6_LANE(v, v0, v1, 4);
  STORE6_LANE(v, v0, v1, 5);
  STORE6_LANE(v, v0, v1, 6);
  STORE6_LANE(v, v0, v1, 7);
}
#undef STORE6_LANE

static WEBP_INLINE void Store4x8x2_NEON(const uint8x16_t p1,
                                        const uint8x16_t p0,
                                        const uint8x16_t q0,
                                        const uint8x16_t q1,
                                        uint8_t* const u, uint8_t* const v,
                                        int stride) {
  uint8x8x4_t u0, v0;
  INIT_VECTOR4(u0,
               vget_low_u8(p1), vget_low_u8(p0),
               vget_low_u8(q0), vget_low_u8(q1));
  INIT_VECTOR4(v0,
               vget_high_u8(p1), vget_high_u8(p0),
               vget_high_u8(q0), vget_high_u8(q1));
  vst4_lane_u8(u - 2 + 0 * stride, u0, 0);
  vst4_lane_u8(u - 2 + 1 * stride, u0, 1);
  vst4_lane_u8(u - 2 + 2 * stride, u0, 2);
  vst4_lane_u8(u - 2 + 3 * stride, u0, 3);
  vst4_lane_u8(u - 2 + 4 * stride, u0, 4);
  vst4_lane_u8(u - 2 + 5 * stride, u0, 5);
  vst4_lane_u8(u - 2 + 6 * stride, u0, 6);
  vst4_lane_u8(u - 2 + 7 * stride, u0, 7);
  vst4_lane_u8(v - 2 + 0 * stride, v0, 0);
  vst4_lane_u8(v - 2 + 1 * stride, v0, 1);
  vst4_lane_u8(v - 2 + 2 * stride, v0, 2);
  vst4_lane_u8(v - 2 + 3 * stride, v0, 3);
  vst4_lane_u8(v - 2 + 4 * stride, v0, 4);
  vst4_lane_u8(v - 2 + 5 * stride, v0, 5);
  vst4_lane_u8(v - 2 + 6 * stride, v0, 6);
  vst4_lane_u8(v - 2 + 7 * stride, v0, 7);
}

#endif  // !WORK_AROUND_GCC

// Zero extend 'v' to an int16x8_t.
static WEBP_INLINE int16x8_t ConvertU8ToS16_NEON(uint8x8_t v) {
  return vreinterpretq_s16_u16(vmovl_u8(v));
}

// Performs unsigned 8b saturation on 'dst01' and 'dst23' storing the result
// to the corresponding rows of 'dst'.
static WEBP_INLINE void SaturateAndStore4x4_NEON(uint8_t* const dst,
                                                 const int16x8_t dst01,
                                                 const int16x8_t dst23) {
  // Unsigned saturate to 8b.
  const uint8x8_t dst01_u8 = vqmovun_s16(dst01);
  const uint8x8_t dst23_u8 = vqmovun_s16(dst23);

  // Store the results.
  vst1_lane_u32((uint32_t*)(dst + 0 * BPS), vreinterpret_u32_u8(dst01_u8), 0);
  vst1_lane_u32((uint32_t*)(dst + 1 * BPS), vreinterpret_u32_u8(dst01_u8), 1);
  vst1_lane_u32((uint32_t*)(dst + 2 * BPS), vreinterpret_u32_u8(dst23_u8), 0);
  vst1_lane_u32((uint32_t*)(dst + 3 * BPS), vreinterpret_u32_u8(dst23_u8), 1);
}

static WEBP_INLINE void Add4x4_NEON(const int16x8_t row01,
                                    const int16x8_t row23,
                                    uint8_t* const dst) {
  uint32x2_t dst01 = vdup_n_u32(0);
  uint32x2_t dst23 = vdup_n_u32(0);

  // Load the source pixels.
  dst01 = vld1_lane_u32((uint32_t*)(dst + 0 * BPS), dst01, 0);
  dst23 = vld1_lane_u32((uint32_t*)(dst + 2 * BPS), dst23, 0);
  dst01 = vld1_lane_u32((uint32_t*)(dst + 1 * BPS), dst01, 1);
  dst23 = vld1_lane_u32((uint32_t*)(dst + 3 * BPS), dst23, 1);

  {
    // Convert to 16b.
    const int16x8_t dst01_s16 = ConvertU8ToS16_NEON(vreinterpret_u8_u32(dst01));
    const int16x8_t dst23_s16 = ConvertU8ToS16_NEON(vreinterpret_u8_u32(dst23));

    // Descale with rounding.
    const int16x8_t out01 = vrsraq_n_s16(dst01_s16, row01, 3);
    const int16x8_t out23 = vrsraq_n_s16(dst23_s16, row23, 3);
    // Add the inverse transform.
    SaturateAndStore4x4_NEON(dst, out01, out23);
  }
}

//-----------------------------------------------------------------------------
// Simple In-loop filtering (Paragraph 15.2)

static uint8x16_t NeedsFilter_NEON(const uint8x16_t p1, const uint8x16_t p0,
                                   const uint8x16_t q0, const uint8x16_t q1,
                                   int thresh) {
  const uint8x16_t thresh_v = vdupq_n_u8((uint8_t)thresh);
  const uint8x16_t a_p0_q0 = vabdq_u8(p0, q0);               // abs(p0-q0)
  const uint8x16_t a_p1_q1 = vabdq_u8(p1, q1);               // abs(p1-q1)
  const uint8x16_t a_p0_q0_2 = vqaddq_u8(a_p0_q0, a_p0_q0);  // 2 * abs(p0-q0)
  const uint8x16_t a_p1_q1_2 = vshrq_n_u8(a_p1_q1, 1);       // abs(p1-q1) / 2
  const uint8x16_t sum = vqaddq_u8(a_p0_q0_2, a_p1_q1_2);
  const uint8x16_t mask = vcgeq_u8(thresh_v, sum);
  return mask;
}

static int8x16_t FlipSign_NEON(const uint8x16_t v) {
  const uint8x16_t sign_bit = vdupq_n_u8(0x80);
  return vreinterpretq_s8_u8(veorq_u8(v, sign_bit));
}

static uint8x16_t FlipSignBack_NEON(const int8x16_t v) {
  const int8x16_t sign_bit = vdupq_n_s8(0x80);
  return vreinterpretq_u8_s8(veorq_s8(v, sign_bit));
}

static int8x16_t GetBaseDelta_NEON(const int8x16_t p1, const int8x16_t p0,
                                   const int8x16_t q0, const int8x16_t q1) {
  const int8x16_t q0_p0 = vqsubq_s8(q0, p0);      // (q0-p0)
  const int8x16_t p1_q1 = vqsubq_s8(p1, q1);      // (p1-q1)
  const int8x16_t s1 = vqaddq_s8(p1_q1, q0_p0);   // (p1-q1) + 1 * (q0 - p0)
  const int8x16_t s2 = vqaddq_s8(q0_p0, s1);      // (p1-q1) + 2 * (q0 - p0)
  const int8x16_t s3 = vqaddq_s8(q0_p0, s2);      // (p1-q1) + 3 * (q0 - p0)
  return s3;
}

static int8x16_t GetBaseDelta0_NEON(const int8x16_t p0, const int8x16_t q0) {
  const int8x16_t q0_p0 = vqsubq_s8(q0, p0);      // (q0-p0)
  const int8x16_t s1 = vqaddq_s8(q0_p0, q0_p0);   // 2 * (q0 - p0)
  const int8x16_t s2 = vqaddq_s8(q0_p0, s1);      // 3 * (q0 - p0)
  return s2;
}

//------------------------------------------------------------------------------

static void ApplyFilter2NoFlip_NEON(const int8x16_t p0s, const int8x16_t q0s,
                                    const int8x16_t delta,
                                    int8x16_t* const op0,
                                    int8x16_t* const oq0) {
  const int8x16_t kCst3 = vdupq_n_s8(0x03);
  const int8x16_t kCst4 = vdupq_n_s8(0x04);
  const int8x16_t delta_p3 = vqaddq_s8(delta, kCst3);
  const int8x16_t delta_p4 = vqaddq_s8(delta, kCst4);
  const int8x16_t delta3 = vshrq_n_s8(delta_p3, 3);
  const int8x16_t delta4 = vshrq_n_s8(delta_p4, 3);
  *op0 = vqaddq_s8(p0s, delta3);
  *oq0 = vqsubq_s8(q0s, delta4);
}

#if defined(WEBP_USE_INTRINSICS)

static void ApplyFilter2_NEON(const int8x16_t p0s, const int8x16_t q0s,
                              const int8x16_t delta,
                              uint8x16_t* const op0, uint8x16_t* const oq0) {
  const int8x16_t kCst3 = vdupq_n_s8(0x03);
  const int8x16_t kCst4 = vdupq_n_s8(0x04);
  const int8x16_t delta_p3 = vqaddq_s8(delta, kCst3);
  const int8x16_t delta_p4 = vqaddq_s8(delta, kCst4);
  const int8x16_t delta3 = vshrq_n_s8(delta_p3, 3);
  const int8x16_t delta4 = vshrq_n_s8(delta_p4, 3);
  const int8x16_t sp0 = vqaddq_s8(p0s, delta3);
  const int8x16_t sq0 = vqsubq_s8(q0s, delta4);
  *op0 = FlipSignBack_NEON(sp0);
  *oq0 = FlipSignBack_NEON(sq0);
}

static void DoFilter2_NEON(const uint8x16_t p1, const uint8x16_t p0,
                           const uint8x16_t q0, const uint8x16_t q1,
                           const uint8x16_t mask,
                           uint8x16_t* const op0, uint8x16_t* const oq0) {
  const int8x16_t p1s = FlipSign_NEON(p1);
  const int8x16_t p0s = FlipSign_NEON(p0);
  const int8x16_t q0s = FlipSign_NEON(q0);
  const int8x16_t q1s = FlipSign_NEON(q1);
  const int8x16_t delta0 = GetBaseDelta_NEON(p1s, p0s, q0s, q1s);
  const int8x16_t delta1 = vandq_s8(delta0, vreinterpretq_s8_u8(mask));
  ApplyFilter2_NEON(p0s, q0s, delta1, op0, oq0);
}

static void SimpleVFilter16_NEON(uint8_t* p, int stride, int thresh) {
  uint8x16_t p1, p0, q0, q1, op0, oq0;
  Load16x4_NEON(p, stride, &p1, &p0, &q0, &q1);
  {
    const uint8x16_t mask = NeedsFilter_NEON(p1, p0, q0, q1, thresh);
    DoFilter2_NEON(p1, p0, q0, q1, mask, &op0, &oq0);
  }
  Store16x2_NEON(op0, oq0, p, stride);
}

static void SimpleHFilter16_NEON(uint8_t* p, int stride, int thresh) {
  uint8x16_t p1, p0, q0, q1, oq0, op0;
  Load4x16_NEON(p, stride, &p1, &p0, &q0, &q1);
  {
    const uint8x16_t mask = NeedsFilter_NEON(p1, p0, q0, q1, thresh);
    DoFilter2_NEON(p1, p0, q0, q1, mask, &op0, &oq0);
  }
  Store2x16_NEON(op0, oq0, p, stride);
}

#else

// Load/Store vertical edge
#define LOAD8x4(c1, c2, c3, c4, b1, b2, stride)                                \
  "vld4.8 {" #c1 "[0]," #c2 "[0]," #c3 "[0]," #c4 "[0]}," #b1 "," #stride "\n" \
  "vld4.8 {" #c1 "[1]," #c2 "[1]," #c3 "[1]," #c4 "[1]}," #b2 "," #stride "\n" \
  "vld4.8 {" #c1 "[2]," #c2 "[2]," #c3 "[2]," #c4 "[2]}," #b1 "," #stride "\n" \
  "vld4.8 {" #c1 "[3]," #c2 "[3]," #c3 "[3]," #c4 "[3]}," #b2 "," #stride "\n" \
  "vld4.8 {" #c1 "[4]," #c2 "[4]," #c3 "[4]," #c4 "[4]}," #b1 "," #stride "\n" \
  "vld4.8 {" #c1 "[5]," #c2 "[5]," #c3 "[5]," #c4 "[5]}," #b2 "," #stride "\n" \
  "vld4.8 {" #c1 "[6]," #c2 "[6]," #c3 "[6]," #c4 "[6]}," #b1 "," #stride "\n" \
  "vld4.8 {" #c1 "[7]," #c2 "[7]," #c3 "[7]," #c4 "[7]}," #b2 "," #stride "\n"

#define STORE8x2(c1, c2, p, stride)                                            \
  "vst2.8   {" #c1 "[0], " #c2 "[0]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[1], " #c2 "[1]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[2], " #c2 "[2]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[3], " #c2 "[3]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[4], " #c2 "[4]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[5], " #c2 "[5]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[6], " #c2 "[6]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[7], " #c2 "[7]}," #p "," #stride " \n"

#define QRegs "q0", "q1", "q2", "q3",                                          \
              "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15"

#define FLIP_SIGN_BIT2(a, b, s)                                                \
  "veor     " #a "," #a "," #s "               \n"                             \
  "veor     " #b "," #b "," #s "               \n"                             \

#define FLIP_SIGN_BIT4(a, b, c, d, s)                                          \
  FLIP_SIGN_BIT2(a, b, s)                                                      \
  FLIP_SIGN_BIT2(c, d, s)                                                      \

#define NEEDS_FILTER(p1, p0, q0, q1, thresh, mask)                             \
  "vabd.u8    q15," #p0 "," #q0 "         \n"  /* abs(p0 - q0) */              \
  "vabd.u8    q14," #p1 "," #q1 "         \n"  /* abs(p1 - q1) */              \
  "vqadd.u8   q15, q15, q15               \n"  /* abs(p0 - q0) * 2 */          \
  "vshr.u8    q14, q14, #1                \n"  /* abs(p1 - q1) / 2 */          \
  "vqadd.u8   q15, q15, q14     \n"  /* abs(p0 - q0) * 2 + abs(p1 - q1) / 2 */ \
  "vdup.8     q14, " #thresh "            \n"                                  \
  "vcge.u8   " #mask ", q14, q15          \n"  /* mask <= thresh */

#define GET_BASE_DELTA(p1, p0, q0, q1, o)                                      \
  "vqsub.s8   q15," #q0 "," #p0 "         \n"  /* (q0 - p0) */                 \
  "vqsub.s8  " #o "," #p1 "," #q1 "       \n"  /* (p1 - q1) */                 \
  "vqadd.s8  " #o "," #o ", q15           \n"  /* (p1 - q1) + 1 * (p0 - q0) */ \
  "vqadd.s8  " #o "," #o ", q15           \n"  /* (p1 - q1) + 2 * (p0 - q0) */ \
  "vqadd.s8  " #o "," #o ", q15           \n"  /* (p1 - q1) + 3 * (p0 - q0) */

#define DO_SIMPLE_FILTER(p0, q0, fl)                                           \
  "vmov.i8    q15, #0x03                  \n"                                  \
  "vqadd.s8   q15, q15, " #fl "           \n"  /* filter1 = filter + 3 */      \
  "vshr.s8    q15, q15, #3                \n"  /* filter1 >> 3 */              \
  "vqadd.s8  " #p0 "," #p0 ", q15         \n"  /* p0 += filter1 */             \
                                                                               \
  "vmov.i8    q15, #0x04                  \n"                                  \
  "vqadd.s8   q15, q15, " #fl "           \n"  /* filter1 = filter + 4 */      \
  "vshr.s8    q15, q15, #3                \n"  /* filter2 >> 3 */              \
  "vqsub.s8  " #q0 "," #q0 ", q15         \n"  /* q0 -= filter2 */

// Applies filter on 2 pixels (p0 and q0)
#define DO_FILTER2(p1, p0, q0, q1, thresh)                                     \
  NEEDS_FILTER(p1, p0, q0, q1, thresh, q9)     /* filter mask in q9 */         \
  "vmov.i8    q10, #0x80                  \n"  /* sign bit */                  \
  FLIP_SIGN_BIT4(p1, p0, q0, q1, q10)          /* convert to signed value */   \
  GET_BASE_DELTA(p1, p0, q0, q1, q11)          /* get filter level  */         \
  "vand       q9, q9, q11                 \n"  /* apply filter mask */         \
  DO_SIMPLE_FILTER(p0, q0, q9)                 /* apply filter */              \
  FLIP_SIGN_BIT2(p0, q0, q10)

static void SimpleVFilter16_NEON(uint8_t* p, int stride, int thresh) {
  __asm__ volatile (
    "sub        %[p], %[p], %[stride], lsl #1  \n"  // p -= 2 * stride

    "vld1.u8    {q1}, [%[p]], %[stride]        \n"  // p1
    "vld1.u8    {q2}, [%[p]], %[stride]        \n"  // p0
    "vld1.u8    {q3}, [%[p]], %[stride]        \n"  // q0
    "vld1.u8    {q12}, [%[p]]                  \n"  // q1

    DO_FILTER2(q1, q2, q3, q12, %[thresh])

    "sub        %[p], %[p], %[stride], lsl #1  \n"  // p -= 2 * stride

    "vst1.u8    {q2}, [%[p]], %[stride]        \n"  // store op0
    "vst1.u8    {q3}, [%[p]]                   \n"  // store oq0
    : [p] "+r"(p)
    : [stride] "r"(stride), [thresh] "r"(thresh)
    : "memory", QRegs
  );
}

static void SimpleHFilter16_NEON(uint8_t* p, int stride, int thresh) {
  __asm__ volatile (
    "sub        r4, %[p], #2                   \n"  // base1 = p - 2
    "lsl        r6, %[stride], #1              \n"  // r6 = 2 * stride
    "add        r5, r4, %[stride]              \n"  // base2 = base1 + stride

    LOAD8x4(d2, d3, d4, d5, [r4], [r5], r6)
    LOAD8x4(d24, d25, d26, d27, [r4], [r5], r6)
    "vswp       d3, d24                        \n"  // p1:q1 p0:q3
    "vswp       d5, d26                        \n"  // q0:q2 q1:q4
    "vswp       q2, q12                        \n"  // p1:q1 p0:q2 q0:q3 q1:q4

    DO_FILTER2(q1, q2, q12, q13, %[thresh])

    "sub        %[p], %[p], #1                 \n"  // p - 1

    "vswp        d5, d24                       \n"
    STORE8x2(d4, d5, [%[p]], %[stride])
    STORE8x2(d24, d25, [%[p]], %[stride])

    : [p] "+r"(p)
    : [stride] "r"(stride), [thresh] "r"(thresh)
    : "memory", "r4", "r5", "r6", QRegs
  );
}

#undef LOAD8x4
#undef STORE8x2

#endif    // WEBP_USE_INTRINSICS

static void SimpleVFilter16i_NEON(uint8_t* p, int stride, int thresh) {
  uint32_t k;
  for (k = 3; k != 0; --k) {
    p += 4 * stride;
    SimpleVFilter16_NEON(p, stride, thresh);
  }
}

static void SimpleHFilter16i_NEON(uint8_t* p, int stride, int thresh) {
  uint32_t k;
  for (k = 3; k != 0; --k) {
    p += 4;
    SimpleHFilter16_NEON(p, stride, thresh);
  }
}

//------------------------------------------------------------------------------
// Complex In-loop filtering (Paragraph 15.3)

static uint8x16_t NeedsHev_NEON(const uint8x16_t p1, const uint8x16_t p0,
                                const uint8x16_t q0, const uint8x16_t q1,
                                int hev_thresh) {
  const uint8x16_t hev_thresh_v = vdupq_n_u8((uint8_t)hev_thresh);
  const uint8x16_t a_p1_p0 = vabdq_u8(p1, p0);  // abs(p1 - p0)
  const uint8x16_t a_q1_q0 = vabdq_u8(q1, q0);  // abs(q1 - q0)
  const uint8x16_t a_max = vmaxq_u8(a_p1_p0, a_q1_q0);
  const uint8x16_t mask = vcgtq_u8(a_max, hev_thresh_v);
  return mask;
}

static uint8x16_t NeedsFilter2_NEON(const uint8x16_t p3, const uint8x16_t p2,
                                    const uint8x16_t p1, const uint8x16_t p0,
                                    const uint8x16_t q0, const uint8x16_t q1,
                                    const uint8x16_t q2, const uint8x16_t q3,
                                    int ithresh, int thresh) {
  const uint8x16_t ithresh_v = vdupq_n_u8((uint8_t)ithresh);
  const uint8x16_t a_p3_p2 = vabdq_u8(p3, p2);  // abs(p3 - p2)
  const uint8x16_t a_p2_p1 = vabdq_u8(p2, p1);  // abs(p2 - p1)
  const uint8x16_t a_p1_p0 = vabdq_u8(p1, p0);  // abs(p1 - p0)
  const uint8x16_t a_q3_q2 = vabdq_u8(q3, q2);  // abs(q3 - q2)
  const uint8x16_t a_q2_q1 = vabdq_u8(q2, q1);  // abs(q2 - q1)
  const uint8x16_t a_q1_q0 = vabdq_u8(q1, q0);  // abs(q1 - q0)
  const uint8x16_t max1 = vmaxq_u8(a_p3_p2, a_p2_p1);
  const uint8x16_t max2 = vmaxq_u8(a_p1_p0, a_q3_q2);
  const uint8x16_t max3 = vmaxq_u8(a_q2_q1, a_q1_q0);
  const uint8x16_t max12 = vmaxq_u8(max1, max2);
  const uint8x16_t max123 = vmaxq_u8(max12, max3);
  const uint8x16_t mask2 = vcgeq_u8(ithresh_v, max123);
  const uint8x16_t mask1 = NeedsFilter_NEON(p1, p0, q0, q1, thresh);
  const uint8x16_t mask = vandq_u8(mask1, mask2);
  return mask;
}

//  4-points filter

static void ApplyFilter4_NEON(
    const int8x16_t p1, const int8x16_t p0,
    const int8x16_t q0, const int8x16_t q1,
    const int8x16_t delta0,
    uint8x16_t* const op1, uint8x16_t* const op0,
    uint8x16_t* const oq0, uint8x16_t* const oq1) {
  const int8x16_t kCst3 = vdupq_n_s8(0x03);
  const int8x16_t kCst4 = vdupq_n_s8(0x04);
  const int8x16_t delta1 = vqaddq_s8(delta0, kCst4);
  const int8x16_t delta2 = vqaddq_s8(delta0, kCst3);
  const int8x16_t a1 = vshrq_n_s8(delta1, 3);
  const int8x16_t a2 = vshrq_n_s8(delta2, 3);
  const int8x16_t a3 = vrshrq_n_s8(a1, 1);   // a3 = (a1 + 1) >> 1
  *op0 = FlipSignBack_NEON(vqaddq_s8(p0, a2));  // clip(p0 + a2)
  *oq0 = FlipSignBack_NEON(vqsubq_s8(q0, a1));  // clip(q0 - a1)
  *op1 = FlipSignBack_NEON(vqaddq_s8(p1, a3));  // clip(p1 + a3)
  *oq1 = FlipSignBack_NEON(vqsubq_s8(q1, a3));  // clip(q1 - a3)
}

static void DoFilter4_NEON(
    const uint8x16_t p1, const uint8x16_t p0,
    const uint8x16_t q0, const uint8x16_t q1,
    const uint8x16_t mask, const uint8x16_t hev_mask,
    uint8x16_t* const op1, uint8x16_t* const op0,
    uint8x16_t* const oq0, uint8x16_t* const oq1) {
  // This is a fused version of DoFilter2() calling ApplyFilter2 directly
  const int8x16_t p1s = FlipSign_NEON(p1);
  int8x16_t p0s = FlipSign_NEON(p0);
  int8x16_t q0s = FlipSign_NEON(q0);
  const int8x16_t q1s = FlipSign_NEON(q1);
  const uint8x16_t simple_lf_mask = vandq_u8(mask, hev_mask);

  // do_filter2 part (simple loopfilter on pixels with hev)
  {
    const int8x16_t delta = GetBaseDelta_NEON(p1s, p0s, q0s, q1s);
    const int8x16_t simple_lf_delta =
        vandq_s8(delta, vreinterpretq_s8_u8(simple_lf_mask));
    ApplyFilter2NoFlip_NEON(p0s, q0s, simple_lf_delta, &p0s, &q0s);
  }

  // do_filter4 part (complex loopfilter on pixels without hev)
  {
    const int8x16_t delta0 = GetBaseDelta0_NEON(p0s, q0s);
    // we use: (mask & hev_mask) ^ mask = mask & !hev_mask
    const uint8x16_t complex_lf_mask = veorq_u8(simple_lf_mask, mask);
    const int8x16_t complex_lf_delta =
        vandq_s8(delta0, vreinterpretq_s8_u8(complex_lf_mask));
    ApplyFilter4_NEON(p1s, p0s, q0s, q1s, complex_lf_delta, op1, op0, oq0, oq1);
  }
}

//  6-points filter

static void ApplyFilter6_NEON(
    const int8x16_t p2, const int8x16_t p1, const int8x16_t p0,
    const int8x16_t q0, const int8x16_t q1, const int8x16_t q2,
    const int8x16_t delta,
    uint8x16_t* const op2, uint8x16_t* const op1, uint8x16_t* const op0,
    uint8x16_t* const oq0, uint8x16_t* const oq1, uint8x16_t* const oq2) {
  // We have to compute: X = (9*a+63) >> 7, Y = (18*a+63)>>7, Z = (27*a+63) >> 7
  // Turns out, there's a common sub-expression S=9 * a - 1 that can be used
  // with the special vqrshrn_n_s16 rounding-shift-and-narrow instruction:
  //   X = (S + 64) >> 7, Y = (S + 32) >> 6, Z = (18 * a + S + 64) >> 7
  const int8x8_t delta_lo = vget_low_s8(delta);
  const int8x8_t delta_hi = vget_high_s8(delta);
  const int8x8_t kCst9 = vdup_n_s8(9);
  const int16x8_t kCstm1 = vdupq_n_s16(-1);
  const int8x8_t kCst18 = vdup_n_s8(18);
  const int16x8_t S_lo = vmlal_s8(kCstm1, kCst9, delta_lo);  // S = 9 * a - 1
  const int16x8_t S_hi = vmlal_s8(kCstm1, kCst9, delta_hi);
  const int16x8_t Z_lo = vmlal_s8(S_lo, kCst18, delta_lo);   // S + 18 * a
  const int16x8_t Z_hi = vmlal_s8(S_hi, kCst18, delta_hi);
  const int8x8_t a3_lo = vqrshrn_n_s16(S_lo, 7);   // (9 * a + 63) >> 7
  const int8x8_t a3_hi = vqrshrn_n_s16(S_hi, 7);
  const int8x8_t a2_lo = vqrshrn_n_s16(S_lo, 6);   // (9 * a + 31) >> 6
  const int8x8_t a2_hi = vqrshrn_n_s16(S_hi, 6);
  const int8x8_t a1_lo = vqrshrn_n_s16(Z_lo, 7);   // (27 * a + 63) >> 7
  const int8x8_t a1_hi = vqrshrn_n_s16(Z_hi, 7);
  const int8x16_t a1 = vcombine_s8(a1_lo, a1_hi);
  const int8x16_t a2 = vcombine_s8(a2_lo, a2_hi);
  const int8x16_t a3 = vcombine_s8(a3_lo, a3_hi);

  *op0 = FlipSignBack_NEON(vqaddq_s8(p0, a1));  // clip(p0 + a1)
  *oq0 = FlipSignBack_NEON(vqsubq_s8(q0, a1));  // clip(q0 - q1)
  *oq1 = FlipSignBack_NEON(vqsubq_s8(q1, a2));  // clip(q1 - a2)
  *op1 = FlipSignBack_NEON(vqaddq_s8(p1, a2));  // clip(p1 + a2)
  *oq2 = FlipSignBack_NEON(vqsubq_s8(q2, a3));  // clip(q2 - a3)
  *op2 = FlipSignBack_NEON(vqaddq_s8(p2, a3));  // clip(p2 + a3)
}

static void DoFilter6_NEON(
    const uint8x16_t p2, const uint8x16_t p1, const uint8x16_t p0,
    const uint8x16_t q0, const uint8x16_t q1, const uint8x16_t q2,
    const uint8x16_t mask, const uint8x16_t hev_mask,
    uint8x16_t* const op2, uint8x16_t* const op1, uint8x16_t* const op0,
    uint8x16_t* const oq0, uint8x16_t* const oq1, uint8x16_t* const oq2) {
  // This is a fused version of DoFilter2() calling ApplyFilter2 directly
  const int8x16_t p2s = FlipSign_NEON(p2);
  const int8x16_t p1s = FlipSign_NEON(p1);
  int8x16_t p0s = FlipSign_NEON(p0);
  int8x16_t q0s = FlipSign_NEON(q0);
  const int8x16_t q1s = FlipSign_NEON(q1);
  const int8x16_t q2s = FlipSign_NEON(q2);
  const uint8x16_t simple_lf_mask = vandq_u8(mask, hev_mask);
  const int8x16_t delta0 = GetBaseDelta_NEON(p1s, p0s, q0s, q1s);

  // do_filter2 part (simple loopfilter on pixels with hev)
  {
    const int8x16_t simple_lf_delta =
        vandq_s8(delta0, vreinterpretq_s8_u8(simple_lf_mask));
    ApplyFilter2NoFlip_NEON(p0s, q0s, simple_lf_delta, &p0s, &q0s);
  }

  // do_filter6 part (complex loopfilter on pixels without hev)
  {
    // we use: (mask & hev_mask) ^ mask = mask & !hev_mask
    const uint8x16_t complex_lf_mask = veorq_u8(simple_lf_mask, mask);
    const int8x16_t complex_lf_delta =
        vandq_s8(delta0, vreinterpretq_s8_u8(complex_lf_mask));
    ApplyFilter6_NEON(p2s, p1s, p0s, q0s, q1s, q2s, complex_lf_delta,
                      op2, op1, op0, oq0, oq1, oq2);
  }
}

// on macroblock edges

static void VFilter16_NEON(uint8_t* p, int stride,
                           int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  Load16x8_NEON(p, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op2, op1, op0, oq0, oq1, oq2;
    DoFilter6_NEON(p2, p1, p0, q0, q1, q2, mask, hev_mask,
                   &op2, &op1, &op0, &oq0, &oq1, &oq2);
    Store16x2_NEON(op2, op1, p - 2 * stride, stride);
    Store16x2_NEON(op0, oq0, p + 0 * stride, stride);
    Store16x2_NEON(oq1, oq2, p + 2 * stride, stride);
  }
}

static void HFilter16_NEON(uint8_t* p, int stride,
                           int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  Load8x16_NEON(p, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op2, op1, op0, oq0, oq1, oq2;
    DoFilter6_NEON(p2, p1, p0, q0, q1, q2, mask, hev_mask,
                   &op2, &op1, &op0, &oq0, &oq1, &oq2);
    Store2x16_NEON(op2, op1, p - 2, stride);
    Store2x16_NEON(op0, oq0, p + 0, stride);
    Store2x16_NEON(oq1, oq2, p + 2, stride);
  }
}

// on three inner edges
static void VFilter16i_NEON(uint8_t* p, int stride,
                            int thresh, int ithresh, int hev_thresh) {
  uint32_t k;
  uint8x16_t p3, p2, p1, p0;
  Load16x4_NEON(p + 2  * stride, stride, &p3, &p2, &p1, &p0);
  for (k = 3; k != 0; --k) {
    uint8x16_t q0, q1, q2, q3;
    p += 4 * stride;
    Load16x4_NEON(p + 2  * stride, stride, &q0, &q1, &q2, &q3);
    {
      const uint8x16_t mask =
          NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3, ithresh, thresh);
      const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
      // p3 and p2 are not just temporary variables here: they will be
      // re-used for next span. And q2/q3 will become p1/p0 accordingly.
      DoFilter4_NEON(p1, p0, q0, q1, mask, hev_mask, &p1, &p0, &p3, &p2);
      Store16x4_NEON(p1, p0, p3, p2, p, stride);
      p1 = q2;
      p0 = q3;
    }
  }
}

#if !defined(WORK_AROUND_GCC)
static void HFilter16i_NEON(uint8_t* p, int stride,
                            int thresh, int ithresh, int hev_thresh) {
  uint32_t k;
  uint8x16_t p3, p2, p1, p0;
  Load4x16_NEON(p + 2, stride, &p3, &p2, &p1, &p0);
  for (k = 3; k != 0; --k) {
    uint8x16_t q0, q1, q2, q3;
    p += 4;
    Load4x16_NEON(p + 2, stride, &q0, &q1, &q2, &q3);
    {
      const uint8x16_t mask =
          NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3, ithresh, thresh);
      const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
      DoFilter4_NEON(p1, p0, q0, q1, mask, hev_mask, &p1, &p0, &p3, &p2);
      Store4x16_NEON(p1, p0, p3, p2, p, stride);
      p1 = q2;
      p0 = q3;
    }
  }
}
#endif  // !WORK_AROUND_GCC

// 8-pixels wide variant, for chroma filtering
static void VFilter8_NEON(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                          int stride, int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  Load8x8x2_NEON(u, v, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op2, op1, op0, oq0, oq1, oq2;
    DoFilter6_NEON(p2, p1, p0, q0, q1, q2, mask, hev_mask,
                   &op2, &op1, &op0, &oq0, &oq1, &oq2);
    Store8x2x2_NEON(op2, op1, u - 2 * stride, v - 2 * stride, stride);
    Store8x2x2_NEON(op0, oq0, u + 0 * stride, v + 0 * stride, stride);
    Store8x2x2_NEON(oq1, oq2, u + 2 * stride, v + 2 * stride, stride);
  }
}
static void VFilter8i_NEON(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int stride,
                           int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  u += 4 * stride;
  v += 4 * stride;
  Load8x8x2_NEON(u, v, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op1, op0, oq0, oq1;
    DoFilter4_NEON(p1, p0, q0, q1, mask, hev_mask, &op1, &op0, &oq0, &oq1);
    Store8x4x2_NEON(op1, op0, oq0, oq1, u, v, stride);
  }
}

#if !defined(WORK_AROUND_GCC)
static void HFilter8_NEON(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                          int stride, int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  Load8x8x2T_NEON(u, v, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op2, op1, op0, oq0, oq1, oq2;
    DoFilter6_NEON(p2, p1, p0, q0, q1, q2, mask, hev_mask,
                   &op2, &op1, &op0, &oq0, &oq1, &oq2);
    Store6x8x2_NEON(op2, op1, op0, oq0, oq1, oq2, u, v, stride);
  }
}

static void HFilter8i_NEON(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int stride,
                           int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  u += 4;
  v += 4;
  Load8x8x2T_NEON(u, v, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op1, op0, oq0, oq1;
    DoFilter4_NEON(p1, p0, q0, q1, mask, hev_mask, &op1, &op0, &oq0, &oq1);
    Store4x8x2_NEON(op1, op0, oq0, oq1, u, v, stride);
  }
}
#endif  // !WORK_AROUND_GCC

//-----------------------------------------------------------------------------
// Inverse transforms (Paragraph 14.4)

// Technically these are unsigned but vqdmulh is only available in signed.
// vqdmulh returns high half (effectively >> 16) but also doubles the value,
// changing the >> 16 to >> 15 and requiring an additional >> 1.
// We use this to our advantage with kC2. The canonical value is 35468.
// However, the high bit is set so treating it as signed will give incorrect
// results. We avoid this by down shifting by 1 here to clear the highest bit.
// Combined with the doubling effect of vqdmulh we get >> 16.
// This can not be applied to kC1 because the lowest bit is set. Down shifting
// the constant would reduce precision.

// libwebp uses a trick to avoid some extra addition that libvpx does.
// Instead of:
// temp2 = ip[12] + ((ip[12] * cospi8sqrt2minus1) >> 16);
// libwebp adds 1 << 16 to cospi8sqrt2minus1 (kC1). However, this causes the
// same issue with kC1 and vqdmulh that we work around by down shifting kC2

static const int16_t kC1 = WEBP_TRANSFORM_AC3_C1;
static const int16_t kC2 =
    WEBP_TRANSFORM_AC3_C2 / 2;  // half of kC2, actually. See comment above.

#if defined(WEBP_USE_INTRINSICS)
static WEBP_INLINE void Transpose8x2_NEON(const int16x8_t in0,
                                          const int16x8_t in1,
                                          int16x8x2_t* const out) {
  // a0 a1 a2 a3 | b0 b1 b2 b3   => a0 b0 c0 d0 | a1 b1 c1 d1
  // c0 c1 c2 c3 | d0 d1 d2 d3      a2 b2 c2 d2 | a3 b3 c3 d3
  const int16x8x2_t tmp0 = vzipq_s16(in0, in1);   // a0 c0 a1 c1 a2 c2 ...
                                                  // b0 d0 b1 d1 b2 d2 ...
  *out = vzipq_s16(tmp0.val[0], tmp0.val[1]);
}

static WEBP_INLINE void TransformPass_NEON(int16x8x2_t* const rows) {
  // {rows} = in0 | in4
  //          in8 | in12
  // B1 = in4 | in12
  const int16x8_t B1 =
      vcombine_s16(vget_high_s16(rows->val[0]), vget_high_s16(rows->val[1]));
  // C0 = kC1 * in4 | kC1 * in12
  // C1 = kC2 * in4 | kC2 * in12
  const int16x8_t C0 = vsraq_n_s16(B1, vqdmulhq_n_s16(B1, kC1), 1);
  const int16x8_t C1 = vqdmulhq_n_s16(B1, kC2);
  const int16x4_t a = vqadd_s16(vget_low_s16(rows->val[0]),
                                vget_low_s16(rows->val[1]));   // in0 + in8
  const int16x4_t b = vqsub_s16(vget_low_s16(rows->val[0]),
                                vget_low_s16(rows->val[1]));   // in0 - in8
  // c = kC2 * in4 - kC1 * in12
  // d = kC1 * in4 + kC2 * in12
  const int16x4_t c = vqsub_s16(vget_low_s16(C1), vget_high_s16(C0));
  const int16x4_t d = vqadd_s16(vget_low_s16(C0), vget_high_s16(C1));
  const int16x8_t D0 = vcombine_s16(a, b);      // D0 = a | b
  const int16x8_t D1 = vcombine_s16(d, c);      // D1 = d | c
  const int16x8_t E0 = vqaddq_s16(D0, D1);      // a+d | b+c
  const int16x8_t E_tmp = vqsubq_s16(D0, D1);   // a-d | b-c
  const int16x8_t E1 = vcombine_s16(vget_high_s16(E_tmp), vget_low_s16(E_tmp));
  Transpose8x2_NEON(E0, E1, rows);
}

static void TransformOne_NEON(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst) {
  int16x8x2_t rows;
  INIT_VECTOR2(rows, vld1q_s16(in + 0), vld1q_s16(in + 8));
  TransformPass_NEON(&rows);
  TransformPass_NEON(&rows);
  Add4x4_NEON(rows.val[0], rows.val[1], dst);
}

#else

static void TransformOne_NEON(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst) {
  const int kBPS = BPS;
  // kC1, kC2. Padded because vld1.16 loads 8 bytes
  const int16_t constants[4] = { kC1, kC2, 0, 0 };
  /* Adapted from libvpx: vp8/common/arm/neon/shortidct4x4llm_neon.asm */
  __asm__ volatile (
    "vld1.16         {q1, q2}, [%[in]]           \n"
    "vld1.16         {d0}, [%[constants]]        \n"

    /* d2: in[0]
     * d3: in[8]
     * d4: in[4]
     * d5: in[12]
     */
    "vswp            d3, d4                      \n"

    /* q8 = {in[4], in[12]} * kC1 * 2 >> 16
     * q9 = {in[4], in[12]} * kC2 >> 16
     */
    "vqdmulh.s16     q8, q2, d0[0]               \n"
    "vqdmulh.s16     q9, q2, d0[1]               \n"

    /* d22 = a = in[0] + in[8]
     * d23 = b = in[0] - in[8]
     */
    "vqadd.s16       d22, d2, d3                 \n"
    "vqsub.s16       d23, d2, d3                 \n"

    /* The multiplication should be x * kC1 >> 16
     * However, with vqdmulh we get x * kC1 * 2 >> 16
     * (multiply, double, return high half)
     * We avoided this in kC2 by pre-shifting the constant.
     * q8 = in[4]/[12] * kC1 >> 16
     */
    "vshr.s16        q8, q8, #1                  \n"

    /* Add {in[4], in[12]} back after the multiplication. This is handled by
     * adding 1 << 16 to kC1 in the libwebp C code.
     */
    "vqadd.s16       q8, q2, q8                  \n"

    /* d20 = c = in[4]*kC2 - in[12]*kC1
     * d21 = d = in[4]*kC1 + in[12]*kC2
     */
    "vqsub.s16       d20, d18, d17               \n"
    "vqadd.s16       d21, d19, d16               \n"

    /* d2 = tmp[0] = a + d
     * d3 = tmp[1] = b + c
     * d4 = tmp[2] = b - c
     * d5 = tmp[3] = a - d
     */
    "vqadd.s16       d2, d22, d21                \n"
    "vqadd.s16       d3, d23, d20                \n"
    "vqsub.s16       d4, d23, d20                \n"
    "vqsub.s16       d5, d22, d21                \n"

    "vzip.16         q1, q2                      \n"
    "vzip.16         q1, q2                      \n"

    "vswp            d3, d4                      \n"

    /* q8 = {tmp[4], tmp[12]} * kC1 * 2 >> 16
     * q9 = {tmp[4], tmp[12]} * kC2 >> 16
     */
    "vqdmulh.s16     q8, q2, d0[0]               \n"
    "vqdmulh.s16     q9, q2, d0[1]               \n"

    /* d22 = a = tmp[0] + tmp[8]
     * d23 = b = tmp[0] - tmp[8]
     */
    "vqadd.s16       d22, d2, d3                 \n"
    "vqsub.s16       d23, d2, d3                 \n"

    /* See long winded explanations prior */
    "vshr.s16        q8, q8, #1                  \n"
    "vqadd.s16       q8, q2, q8                  \n"

    /* d20 = c = in[4]*kC2 - in[12]*kC1
     * d21 = d = in[4]*kC1 + in[12]*kC2
     */
    "vqsub.s16       d20, d18, d17               \n"
    "vqadd.s16       d21, d19, d16               \n"

    /* d2 = tmp[0] = a + d
     * d3 = tmp[1] = b + c
     * d4 = tmp[2] = b - c
     * d5 = tmp[3] = a - d
     */
    "vqadd.s16       d2, d22, d21                \n"
    "vqadd.s16       d3, d23, d20                \n"
    "vqsub.s16       d4, d23, d20                \n"
    "vqsub.s16       d5, d22, d21                \n"

    "vld1.32         d6[0], [%[dst]], %[kBPS]    \n"
    "vld1.32         d6[1], [%[dst]], %[kBPS]    \n"
    "vld1.32         d7[0], [%[dst]], %[kBPS]    \n"
    "vld1.32         d7[1], [%[dst]], %[kBPS]    \n"

    "sub         %[dst], %[dst], %[kBPS], lsl #2 \n"

    /* (val) + 4 >> 3 */
    "vrshr.s16       d2, d2, #3                  \n"
    "vrshr.s16       d3, d3, #3                  \n"
    "vrshr.s16       d4, d4, #3                  \n"
    "vrshr.s16       d5, d5, #3                  \n"

    "vzip.16         q1, q2                      \n"
    "vzip.16         q1, q2                      \n"

    /* Must accumulate before saturating */
    "vmovl.u8        q8, d6                      \n"
    "vmovl.u8        q9, d7                      \n"

    "vqadd.s16       q1, q1, q8                  \n"
    "vqadd.s16       q2, q2, q9                  \n"

    "vqmovun.s16     d0, q1                      \n"
    "vqmovun.s16     d1, q2                      \n"

    "vst1.32         d0[0], [%[dst]], %[kBPS]    \n"
    "vst1.32         d0[1], [%[dst]], %[kBPS]    \n"
    "vst1.32         d1[0], [%[dst]], %[kBPS]    \n"
    "vst1.32         d1[1], [%[dst]]             \n"

    : [in] "+r"(in), [dst] "+r"(dst)  /* modified registers */
    : [kBPS] "r"(kBPS), [constants] "r"(constants)  /* constants */
    : "memory", "q0", "q1", "q2", "q8", "q9", "q10", "q11"  /* clobbered */
  );
}

#endif    // WEBP_USE_INTRINSICS

static void TransformTwo_NEON(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst, int do_two) {
  TransformOne_NEON(in, dst);
  if (do_two) {
    TransformOne_NEON(in + 16, dst + 4);
  }
}

static void TransformDC_NEON(const int16_t* WEBP_RESTRICT in,
                             uint8_t* WEBP_RESTRICT dst) {
  const int16x8_t DC = vdupq_n_s16(in[0]);
  Add4x4_NEON(DC, DC, dst);
}

//------------------------------------------------------------------------------

#define STORE_WHT(dst, col, rows) do {                  \
  *dst = vgetq_lane_s32(rows.val[0], col); (dst) += 16; \
  *dst = vgetq_lane_s32(rows.val[1], col); (dst) += 16; \
  *dst = vgetq_lane_s32(rows.val[2], col); (dst) += 16; \
  *dst = vgetq_lane_s32(rows.val[3], col); (dst) += 16; \
} while (0)

static void TransformWHT_NEON(const int16_t* WEBP_RESTRICT in,
                              int16_t* WEBP_RESTRICT out) {
  int32x4x4_t tmp;

  {
    // Load the source.
    const int16x4_t in00_03 = vld1_s16(in + 0);
    const int16x4_t in04_07 = vld1_s16(in + 4);
    const int16x4_t in08_11 = vld1_s16(in + 8);
    const int16x4_t in12_15 = vld1_s16(in + 12);
    const int32x4_t a0 = vaddl_s16(in00_03, in12_15);  // in[0..3] + in[12..15]
    const int32x4_t a1 = vaddl_s16(in04_07, in08_11);  // in[4..7] + in[8..11]
    const int32x4_t a2 = vsubl_s16(in04_07, in08_11);  // in[4..7] - in[8..11]
    const int32x4_t a3 = vsubl_s16(in00_03, in12_15);  // in[0..3] - in[12..15]
    tmp.val[0] = vaddq_s32(a0, a1);
    tmp.val[1] = vaddq_s32(a3, a2);
    tmp.val[2] = vsubq_s32(a0, a1);
    tmp.val[3] = vsubq_s32(a3, a2);
    // Arrange the temporary results column-wise.
    tmp = Transpose4x4_NEON(tmp);
  }

  {
    const int32x4_t kCst3 = vdupq_n_s32(3);
    const int32x4_t dc = vaddq_s32(tmp.val[0], kCst3);  // add rounder
    const int32x4_t a0 = vaddq_s32(dc, tmp.val[3]);
    const int32x4_t a1 = vaddq_s32(tmp.val[1], tmp.val[2]);
    const int32x4_t a2 = vsubq_s32(tmp.val[1], tmp.val[2]);
    const int32x4_t a3 = vsubq_s32(dc, tmp.val[3]);

    tmp.val[0] = vaddq_s32(a0, a1);
    tmp.val[1] = vaddq_s32(a3, a2);
    tmp.val[2] = vsubq_s32(a0, a1);
    tmp.val[3] = vsubq_s32(a3, a2);

    // right shift the results by 3.
    tmp.val[0] = vshrq_n_s32(tmp.val[0], 3);
    tmp.val[1] = vshrq_n_s32(tmp.val[1], 3);
    tmp.val[2] = vshrq_n_s32(tmp.val[2], 3);
    tmp.val[3] = vshrq_n_s32(tmp.val[3], 3);

    STORE_WHT(out, 0, tmp);
    STORE_WHT(out, 1, tmp);
    STORE_WHT(out, 2, tmp);
    STORE_WHT(out, 3, tmp);
  }
}

#undef STORE_WHT

//------------------------------------------------------------------------------

static void TransformAC3_NEON(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst) {
  const int16x4_t A = vld1_dup_s16(in);
  const int16x4_t c4 = vdup_n_s16(WEBP_TRANSFORM_AC3_MUL2(in[4]));
  const int16x4_t d4 = vdup_n_s16(WEBP_TRANSFORM_AC3_MUL1(in[4]));
  const int c1 = WEBP_TRANSFORM_AC3_MUL2(in[1]);
  const int d1 = WEBP_TRANSFORM_AC3_MUL1(in[1]);
  const uint64_t cd = (uint64_t)( d1 & 0xffff) <<  0 |
                      (uint64_t)( c1 & 0xffff) << 16 |
                      (uint64_t)(-c1 & 0xffff) << 32 |
                      (uint64_t)(-d1 & 0xffff) << 48;
  const int16x4_t CD = vcreate_s16(cd);
  const int16x4_t B = vqadd_s16(A, CD);
  const int16x8_t m0_m1 = vcombine_s16(vqadd_s16(B, d4), vqadd_s16(B, c4));
  const int16x8_t m2_m3 = vcombine_s16(vqsub_s16(B, c4), vqsub_s16(B, d4));
  Add4x4_NEON(m0_m1, m2_m3, dst);
}

//------------------------------------------------------------------------------
// 4x4

static void DC4_NEON(uint8_t* dst) {    // DC
  const uint8x8_t A = vld1_u8(dst - BPS);  // top row
  const uint16x4_t p0 = vpaddl_u8(A);  // cascading summation of the top
  const uint16x4_t p1 = vpadd_u16(p0, p0);
  const uint8x8_t L0 = vld1_u8(dst + 0 * BPS - 1);
  const uint8x8_t L1 = vld1_u8(dst + 1 * BPS - 1);
  const uint8x8_t L2 = vld1_u8(dst + 2 * BPS - 1);
  const uint8x8_t L3 = vld1_u8(dst + 3 * BPS - 1);
  const uint16x8_t s0 = vaddl_u8(L0, L1);
  const uint16x8_t s1 = vaddl_u8(L2, L3);
  const uint16x8_t s01 = vaddq_u16(s0, s1);
  const uint16x8_t sum = vaddq_u16(s01, vcombine_u16(p1, p1));
  const uint8x8_t dc0 = vrshrn_n_u16(sum, 3);  // (sum + 4) >> 3
  const uint8x8_t dc = vdup_lane_u8(dc0, 0);
  int i;
  for (i = 0; i < 4; ++i) {
    vst1_lane_u32((uint32_t*)(dst + i * BPS), vreinterpret_u32_u8(dc), 0);
  }
}

// TrueMotion (4x4 + 8x8)
static WEBP_INLINE void TrueMotion_NEON(uint8_t* dst, int size) {
  const uint8x8_t TL = vld1_dup_u8(dst - BPS - 1);  // top-left pixel 'A[-1]'
  const uint8x8_t T = vld1_u8(dst - BPS);  // top row 'A[0..3]'
  const uint16x8_t d = vsubl_u8(T, TL);  // A[c] - A[-1]
  int y;
  for (y = 0; y < size; y += 4) {
    // left edge
    const uint8x8_t L0 = vld1_dup_u8(dst + 0 * BPS - 1);
    const uint8x8_t L1 = vld1_dup_u8(dst + 1 * BPS - 1);
    const uint8x8_t L2 = vld1_dup_u8(dst + 2 * BPS - 1);
    const uint8x8_t L3 = vld1_dup_u8(dst + 3 * BPS - 1);
    // L[r] + A[c] - A[-1]
    const int16x8_t r0 = vreinterpretq_s16_u16(vaddw_u8(d, L0));
    const int16x8_t r1 = vreinterpretq_s16_u16(vaddw_u8(d, L1));
    const int16x8_t r2 = vreinterpretq_s16_u16(vaddw_u8(d, L2));
    const int16x8_t r3 = vreinterpretq_s16_u16(vaddw_u8(d, L3));
    // Saturate and store the result.
    const uint32x2_t r0_u32 = vreinterpret_u32_u8(vqmovun_s16(r0));
    const uint32x2_t r1_u32 = vreinterpret_u32_u8(vqmovun_s16(r1));
    const uint32x2_t r2_u32 = vreinterpret_u32_u8(vqmovun_s16(r2));
    const uint32x2_t r3_u32 = vreinterpret_u32_u8(vqmovun_s16(r3));
    if (size == 4) {
      vst1_lane_u32((uint32_t*)(dst + 0 * BPS), r0_u32, 0);
      vst1_lane_u32((uint32_t*)(dst + 1 * BPS), r1_u32, 0);
      vst1_lane_u32((uint32_t*)(dst + 2 * BPS), r2_u32, 0);
      vst1_lane_u32((uint32_t*)(dst + 3 * BPS), r3_u32, 0);
    } else {
      vst1_u32((uint32_t*)(dst + 0 * BPS), r0_u32);
      vst1_u32((uint32_t*)(dst + 1 * BPS), r1_u32);
      vst1_u32((uint32_t*)(dst + 2 * BPS), r2_u32);
      vst1_u32((uint32_t*)(dst + 3 * BPS), r3_u32);
    }
    dst += 4 * BPS;
  }
}

static void TM4_NEON(uint8_t* dst) { TrueMotion_NEON(dst, 4); }

static void VE4_NEON(uint8_t* dst) {    // vertical
  // NB: avoid vld1_u64 here as an alignment hint may be added -> SIGBUS.
  const uint64x1_t A0 = vreinterpret_u64_u8(vld1_u8(dst - BPS - 1));  // top row
  const uint64x1_t A1 = vshr_n_u64(A0, 8);
  const uint64x1_t A2 = vshr_n_u64(A0, 16);
  const uint8x8_t ABCDEFGH = vreinterpret_u8_u64(A0);
  const uint8x8_t BCDEFGH0 = vreinterpret_u8_u64(A1);
  const uint8x8_t CDEFGH00 = vreinterpret_u8_u64(A2);
  const uint8x8_t b = vhadd_u8(ABCDEFGH, CDEFGH00);
  const uint8x8_t avg = vrhadd_u8(b, BCDEFGH0);
  int i;
  for (i = 0; i < 4; ++i) {
    vst1_lane_u32((uint32_t*)(dst + i * BPS), vreinterpret_u32_u8(avg), 0);
  }
}

static void RD4_NEON(uint8_t* dst) {   // Down-right
  const uint8x8_t XABCD_u8 = vld1_u8(dst - BPS - 1);
  const uint64x1_t XABCD = vreinterpret_u64_u8(XABCD_u8);
  const uint64x1_t ____XABC = vshl_n_u64(XABCD, 32);
  const uint32_t I = dst[-1 + 0 * BPS];
  const uint32_t J = dst[-1 + 1 * BPS];
  const uint32_t K = dst[-1 + 2 * BPS];
  const uint32_t L = dst[-1 + 3 * BPS];
  const uint64x1_t LKJI____ =
      vcreate_u64((uint64_t)L | (K << 8) | (J << 16) | (I << 24));
  const uint64x1_t LKJIXABC = vorr_u64(LKJI____, ____XABC);
  const uint8x8_t KJIXABC_ = vreinterpret_u8_u64(vshr_n_u64(LKJIXABC, 8));
  const uint8x8_t JIXABC__ = vreinterpret_u8_u64(vshr_n_u64(LKJIXABC, 16));
  const uint8_t D = vget_lane_u8(XABCD_u8, 4);
  const uint8x8_t JIXABCD_ = vset_lane_u8(D, JIXABC__, 6);
  const uint8x8_t LKJIXABC_u8 = vreinterpret_u8_u64(LKJIXABC);
  const uint8x8_t avg1 = vhadd_u8(JIXABCD_, LKJIXABC_u8);
  const uint8x8_t avg2 = vrhadd_u8(avg1, KJIXABC_);
  const uint64x1_t avg2_u64 = vreinterpret_u64_u8(avg2);
  const uint32x2_t r3 = vreinterpret_u32_u8(avg2);
  const uint32x2_t r2 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 8));
  const uint32x2_t r1 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 16));
  const uint32x2_t r0 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 24));
  vst1_lane_u32((uint32_t*)(dst + 0 * BPS), r0, 0);
  vst1_lane_u32((uint32_t*)(dst + 1 * BPS), r1, 0);
  vst1_lane_u32((uint32_t*)(dst + 2 * BPS), r2, 0);
  vst1_lane_u32((uint32_t*)(dst + 3 * BPS), r3, 0);
}

static void LD4_NEON(uint8_t* dst) {    // Down-left
  // Note using the same shift trick as VE4() is slower here.
  const uint8x8_t ABCDEFGH = vld1_u8(dst - BPS + 0);
  const uint8x8_t BCDEFGH0 = vld1_u8(dst - BPS + 1);
  const uint8x8_t CDEFGH00 = vld1_u8(dst - BPS + 2);
  const uint8x8_t CDEFGHH0 = vset_lane_u8(dst[-BPS + 7], CDEFGH00, 6);
  const uint8x8_t avg1 = vhadd_u8(ABCDEFGH, CDEFGHH0);
  const uint8x8_t avg2 = vrhadd_u8(avg1, BCDEFGH0);
  const uint64x1_t avg2_u64 = vreinterpret_u64_u8(avg2);
  const uint32x2_t r0 = vreinterpret_u32_u8(avg2);
  const uint32x2_t r1 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 8));
  const uint32x2_t r2 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 16));
  const uint32x2_t r3 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 24));
  vst1_lane_u32((uint32_t*)(dst + 0 * BPS), r0, 0);
  vst1_lane_u32((uint32_t*)(dst + 1 * BPS), r1, 0);
  vst1_lane_u32((uint32_t*)(dst + 2 * BPS), r2, 0);
  vst1_lane_u32((uint32_t*)(dst + 3 * BPS), r3, 0);
}

//------------------------------------------------------------------------------
// Chroma

static void VE8uv_NEON(uint8_t* dst) {    // vertical
  const uint8x8_t top = vld1_u8(dst - BPS);
  int j;
  for (j = 0; j < 8; ++j) {
    vst1_u8(dst + j * BPS, top);
  }
}

static void HE8uv_NEON(uint8_t* dst) {    // horizontal
  int j;
  for (j = 0; j < 8; ++j) {
    const uint8x8_t left = vld1_dup_u8(dst - 1);
    vst1_u8(dst, left);
    dst += BPS;
  }
}

static WEBP_INLINE void DC8_NEON(uint8_t* dst, int do_top, int do_left) {
  uint16x8_t sum_top;
  uint16x8_t sum_left;
  uint8x8_t dc0;

  if (do_top) {
    const uint8x8_t A = vld1_u8(dst - BPS);  // top row
#if WEBP_AARCH64
    const uint16_t p2 = vaddlv_u8(A);
    sum_top = vdupq_n_u16(p2);
#else
    const uint16x4_t p0 = vpaddl_u8(A);  // cascading summation of the top
    const uint16x4_t p1 = vpadd_u16(p0, p0);
    const uint16x4_t p2 = vpadd_u16(p1, p1);
    sum_top = vcombine_u16(p2, p2);
#endif
  }

  if (do_left) {
    const uint8x8_t L0 = vld1_u8(dst + 0 * BPS - 1);
    const uint8x8_t L1 = vld1_u8(dst + 1 * BPS - 1);
    const uint8x8_t L2 = vld1_u8(dst + 2 * BPS - 1);
    const uint8x8_t L3 = vld1_u8(dst + 3 * BPS - 1);
    const uint8x8_t L4 = vld1_u8(dst + 4 * BPS - 1);
    const uint8x8_t L5 = vld1_u8(dst + 5 * BPS - 1);
    const uint8x8_t L6 = vld1_u8(dst + 6 * BPS - 1);
    const uint8x8_t L7 = vld1_u8(dst + 7 * BPS - 1);
    const uint16x8_t s0 = vaddl_u8(L0, L1);
    const uint16x8_t s1 = vaddl_u8(L2, L3);
    const uint16x8_t s2 = vaddl_u8(L4, L5);
    const uint16x8_t s3 = vaddl_u8(L6, L7);
    const uint16x8_t s01 = vaddq_u16(s0, s1);
    const uint16x8_t s23 = vaddq_u16(s2, s3);
    sum_left = vaddq_u16(s01, s23);
  }

  if (do_top && do_left) {
    const uint16x8_t sum = vaddq_u16(sum_left, sum_top);
    dc0 = vrshrn_n_u16(sum, 4);
  } else if (do_top) {
    dc0 = vrshrn_n_u16(sum_top, 3);
  } else if (do_left) {
    dc0 = vrshrn_n_u16(sum_left, 3);
  } else {
    dc0 = vdup_n_u8(0x80);
  }

  {
    const uint8x8_t dc = vdup_lane_u8(dc0, 0);
    int i;
    for (i = 0; i < 8; ++i) {
      vst1_u32((uint32_t*)(dst + i * BPS), vreinterpret_u32_u8(dc));
    }
  }
}

static void DC8uv_NEON(uint8_t* dst) { DC8_NEON(dst, 1, 1); }
static void DC8uvNoTop_NEON(uint8_t* dst) { DC8_NEON(dst, 0, 1); }
static void DC8uvNoLeft_NEON(uint8_t* dst) { DC8_NEON(dst, 1, 0); }
static void DC8uvNoTopLeft_NEON(uint8_t* dst) { DC8_NEON(dst, 0, 0); }

static void TM8uv_NEON(uint8_t* dst) { TrueMotion_NEON(dst, 8); }

//------------------------------------------------------------------------------
// 16x16

static void VE16_NEON(uint8_t* dst) {     // vertical
  const uint8x16_t top = vld1q_u8(dst - BPS);
  int j;
  for (j = 0; j < 16; ++j) {
    vst1q_u8(dst + j * BPS, top);
  }
}

static void HE16_NEON(uint8_t* dst) {     // horizontal
  int j;
  for (j = 0; j < 16; ++j) {
    const uint8x16_t left = vld1q_dup_u8(dst - 1);
    vst1q_u8(dst, left);
    dst += BPS;
  }
}

static WEBP_INLINE void DC16_NEON(uint8_t* dst, int do_top, int do_left) {
  uint16x8_t sum_top;
  uint16x8_t sum_left;
  uint8x8_t dc0;

  if (do_top) {
    const uint8x16_t A = vld1q_u8(dst - BPS);  // top row
#if WEBP_AARCH64
    const uint16_t p3 = vaddlvq_u8(A);
    sum_top = vdupq_n_u16(p3);
#else
    const uint16x8_t p0 = vpaddlq_u8(A);  // cascading summation of the top
    const uint16x4_t p1 = vadd_u16(vget_low_u16(p0), vget_high_u16(p0));
    const uint16x4_t p2 = vpadd_u16(p1, p1);
    const uint16x4_t p3 = vpadd_u16(p2, p2);
    sum_top = vcombine_u16(p3, p3);
#endif
  }

  if (do_left) {
    int i;
    sum_left = vdupq_n_u16(0);
    for (i = 0; i < 16; i += 8) {
      const uint8x8_t L0 = vld1_u8(dst + (i + 0) * BPS - 1);
      const uint8x8_t L1 = vld1_u8(dst + (i + 1) * BPS - 1);
      const uint8x8_t L2 = vld1_u8(dst + (i + 2) * BPS - 1);
      const uint8x8_t L3 = vld1_u8(dst + (i + 3) * BPS - 1);
      const uint8x8_t L4 = vld1_u8(dst + (i + 4) * BPS - 1);
      const uint8x8_t L5 = vld1_u8(dst + (i + 5) * BPS - 1);
      const uint8x8_t L6 = vld1_u8(dst + (i + 6) * BPS - 1);
      const uint8x8_t L7 = vld1_u8(dst + (i + 7) * BPS - 1);
      const uint16x8_t s0 = vaddl_u8(L0, L1);
      const uint16x8_t s1 = vaddl_u8(L2, L3);
      const uint16x8_t s2 = vaddl_u8(L4, L5);
      const uint16x8_t s3 = vaddl_u8(L6, L7);
      const uint16x8_t s01 = vaddq_u16(s0, s1);
      const uint16x8_t s23 = vaddq_u16(s2, s3);
      const uint16x8_t sum = vaddq_u16(s01, s23);
      sum_left = vaddq_u16(sum_left, sum);
    }
  }

  if (do_top && do_left) {
    const uint16x8_t sum = vaddq_u16(sum_left, sum_top);
    dc0 = vrshrn_n_u16(sum, 5);
  } else if (do_top) {
    dc0 = vrshrn_n_u16(sum_top, 4);
  } else if (do_left) {
    dc0 = vrshrn_n_u16(sum_left, 4);
  } else {
    dc0 = vdup_n_u8(0x80);
  }

  {
    const uint8x16_t dc = vdupq_lane_u8(dc0, 0);
    int i;
    for (i = 0; i < 16; ++i) {
      vst1q_u8(dst + i * BPS, dc);
    }
  }
}

static void DC16TopLeft_NEON(uint8_t* dst) { DC16_NEON(dst, 1, 1); }
static void DC16NoTop_NEON(uint8_t* dst) { DC16_NEON(dst, 0, 1); }
static void DC16NoLeft_NEON(uint8_t* dst) { DC16_NEON(dst, 1, 0); }
static void DC16NoTopLeft_NEON(uint8_t* dst) { DC16_NEON(dst, 0, 0); }

static void TM16_NEON(uint8_t* dst) {
  const uint8x8_t TL = vld1_dup_u8(dst - BPS - 1);  // top-left pixel 'A[-1]'
  const uint8x16_t T = vld1q_u8(dst - BPS);  // top row 'A[0..15]'
  // A[c] - A[-1]
  const uint16x8_t d_lo = vsubl_u8(vget_low_u8(T), TL);
  const uint16x8_t d_hi = vsubl_u8(vget_high_u8(T), TL);
  int y;
  for (y = 0; y < 16; y += 4) {
    // left edge
    const uint8x8_t L0 = vld1_dup_u8(dst + 0 * BPS - 1);
    const uint8x8_t L1 = vld1_dup_u8(dst + 1 * BPS - 1);
    const uint8x8_t L2 = vld1_dup_u8(dst + 2 * BPS - 1);
    const uint8x8_t L3 = vld1_dup_u8(dst + 3 * BPS - 1);
    // L[r] + A[c] - A[-1]
    const int16x8_t r0_lo = vreinterpretq_s16_u16(vaddw_u8(d_lo, L0));
    const int16x8_t r1_lo = vreinterpretq_s16_u16(vaddw_u8(d_lo, L1));
    const int16x8_t r2_lo = vreinterpretq_s16_u16(vaddw_u8(d_lo, L2));
    const int16x8_t r3_lo = vreinterpretq_s16_u16(vaddw_u8(d_lo, L3));
    const int16x8_t r0_hi = vreinterpretq_s16_u16(vaddw_u8(d_hi, L0));
    const int16x8_t r1_hi = vreinterpretq_s16_u16(vaddw_u8(d_hi, L1));
    const int16x8_t r2_hi = vreinterpretq_s16_u16(vaddw_u8(d_hi, L2));
    const int16x8_t r3_hi = vreinterpretq_s16_u16(vaddw_u8(d_hi, L3));
    // Saturate and store the result.
    const uint8x16_t row0 = vcombine_u8(vqmovun_s16(r0_lo), vqmovun_s16(r0_hi));
    const uint8x16_t row1 = vcombine_u8(vqmovun_s16(r1_lo), vqmovun_s16(r1_hi));
    const uint8x16_t row2 = vcombine_u8(vqmovun_s16(r2_lo), vqmovun_s16(r2_hi));
    const uint8x16_t row3 = vcombine_u8(vqmovun_s16(r3_lo), vqmovun_s16(r3_hi));
    vst1q_u8(dst + 0 * BPS, row0);
    vst1q_u8(dst + 1 * BPS, row1);
    vst1q_u8(dst + 2 * BPS, row2);
    vst1q_u8(dst + 3 * BPS, row3);
    dst += 4 * BPS;
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitNEON(void) {
  VP8Transform = TransformTwo_NEON;
  VP8TransformAC3 = TransformAC3_NEON;
  VP8TransformDC = TransformDC_NEON;
  VP8TransformWHT = TransformWHT_NEON;

  VP8VFilter16 = VFilter16_NEON;
  VP8VFilter16i = VFilter16i_NEON;
  VP8HFilter16 = HFilter16_NEON;
#if !defined(WORK_AROUND_GCC)
  VP8HFilter16i = HFilter16i_NEON;
#endif
  VP8VFilter8 = VFilter8_NEON;
  VP8VFilter8i = VFilter8i_NEON;
#if !defined(WORK_AROUND_GCC)
  VP8HFilter8 = HFilter8_NEON;
  VP8HFilter8i = HFilter8i_NEON;
#endif
  VP8SimpleVFilter16 = SimpleVFilter16_NEON;
  VP8SimpleHFilter16 = SimpleHFilter16_NEON;
  VP8SimpleVFilter16i = SimpleVFilter16i_NEON;
  VP8SimpleHFilter16i = SimpleHFilter16i_NEON;

  VP8PredLuma4[0] = DC4_NEON;
  VP8PredLuma4[1] = TM4_NEON;
  VP8PredLuma4[2] = VE4_NEON;
  VP8PredLuma4[4] = RD4_NEON;
  VP8PredLuma4[6] = LD4_NEON;

  VP8PredLuma16[0] = DC16TopLeft_NEON;
  VP8PredLuma16[1] = TM16_NEON;
  VP8PredLuma16[2] = VE16_NEON;
  VP8PredLuma16[3] = HE16_NEON;
  VP8PredLuma16[4] = DC16NoTop_NEON;
  VP8PredLuma16[5] = DC16NoLeft_NEON;
  VP8PredLuma16[6] = DC16NoTopLeft_NEON;

  VP8PredChroma8[0] = DC8uv_NEON;
  VP8PredChroma8[1] = TM8uv_NEON;
  VP8PredChroma8[2] = VE8uv_NEON;
  VP8PredChroma8[3] = HE8uv_NEON;
  VP8PredChroma8[4] = DC8uvNoTop_NEON;
  VP8PredChroma8[5] = DC8uvNoLeft_NEON;
  VP8PredChroma8[6] = DC8uvNoTopLeft_NEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(VP8DspInitNEON)

#endif  // WEBP_USE_NEON
