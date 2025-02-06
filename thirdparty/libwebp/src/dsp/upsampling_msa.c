// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA version of YUV to RGB upsampling functions.
//
// Author: Prashant Patil (prashant.patil@imgtec.com)

#include <string.h>
#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MSA)

#include "src/dsp/msa_macro.h"
#include "src/dsp/yuv.h"

#ifdef FANCY_UPSAMPLING

#define ILVR_UW2(in, out0, out1) do {                            \
  const v8i16 t0 = (v8i16)__msa_ilvr_b((v16i8)zero, (v16i8)in);  \
  out0 = (v4u32)__msa_ilvr_h((v8i16)zero, t0);                   \
  out1 = (v4u32)__msa_ilvl_h((v8i16)zero, t0);                   \
} while (0)

#define ILVRL_UW4(in, out0, out1, out2, out3) do {  \
  v16u8 t0, t1;                                     \
  ILVRL_B2_UB(zero, in, t0, t1);                    \
  ILVRL_H2_UW(zero, t0, out0, out1);                \
  ILVRL_H2_UW(zero, t1, out2, out3);                \
} while (0)

#define MULTHI_16(in0, in1, in2, in3, cnst, out0, out1) do {   \
  const v4i32 const0 = (v4i32)__msa_fill_w(cnst * 256);        \
  v4u32 temp0, temp1, temp2, temp3;                            \
  MUL4(in0, const0, in1, const0, in2, const0, in3, const0,     \
       temp0, temp1, temp2, temp3);                            \
  PCKOD_H2_UH(temp1, temp0, temp3, temp2, out0, out1);         \
} while (0)

#define MULTHI_8(in0, in1, cnst, out0) do {                 \
  const v4i32 const0 = (v4i32)__msa_fill_w(cnst * 256);     \
  v4u32 temp0, temp1;                                       \
  MUL2(in0, const0, in1, const0, temp0, temp1);             \
  out0 = (v8u16)__msa_pckod_h((v8i16)temp1, (v8i16)temp0);  \
} while (0)

#define CALC_R16(y0, y1, v0, v1, dst) do {                \
  const v8i16 const_a = (v8i16)__msa_fill_h(14234);       \
  const v8i16 a0 = __msa_adds_s_h((v8i16)y0, (v8i16)v0);  \
  const v8i16 a1 = __msa_adds_s_h((v8i16)y1, (v8i16)v1);  \
  v8i16 b0 = __msa_subs_s_h(a0, const_a);                 \
  v8i16 b1 = __msa_subs_s_h(a1, const_a);                 \
  SRAI_H2_SH(b0, b1, 6);                                  \
  CLIP_SH2_0_255(b0, b1);                                 \
  dst = (v16u8)__msa_pckev_b((v16i8)b1, (v16i8)b0);       \
} while (0)

#define CALC_R8(y0, v0, dst) do {                         \
  const v8i16 const_a = (v8i16)__msa_fill_h(14234);       \
  const v8i16 a0 = __msa_adds_s_h((v8i16)y0, (v8i16)v0);  \
  v8i16 b0 = __msa_subs_s_h(a0, const_a);                 \
  b0 = SRAI_H(b0, 6);                                     \
  CLIP_SH_0_255(b0);                                      \
  dst = (v16u8)__msa_pckev_b((v16i8)b0, (v16i8)b0);       \
} while (0)

#define CALC_G16(y0, y1, u0, u1, v0, v1, dst) do {   \
  const v8i16 const_a = (v8i16)__msa_fill_h(8708);   \
  v8i16 a0 = __msa_subs_s_h((v8i16)y0, (v8i16)u0);   \
  v8i16 a1 = __msa_subs_s_h((v8i16)y1, (v8i16)u1);   \
  const v8i16 b0 = __msa_subs_s_h(a0, (v8i16)v0);    \
  const v8i16 b1 = __msa_subs_s_h(a1, (v8i16)v1);    \
  a0 = __msa_adds_s_h(b0, const_a);                  \
  a1 = __msa_adds_s_h(b1, const_a);                  \
  SRAI_H2_SH(a0, a1, 6);                             \
  CLIP_SH2_0_255(a0, a1);                            \
  dst = (v16u8)__msa_pckev_b((v16i8)a1, (v16i8)a0);  \
} while (0)

#define CALC_G8(y0, u0, v0, dst) do {                \
  const v8i16 const_a = (v8i16)__msa_fill_h(8708);   \
  v8i16 a0 = __msa_subs_s_h((v8i16)y0, (v8i16)u0);   \
  const v8i16 b0 = __msa_subs_s_h(a0, (v8i16)v0);    \
  a0 = __msa_adds_s_h(b0, const_a);                  \
  a0 = SRAI_H(a0, 6);                                \
  CLIP_SH_0_255(a0);                                 \
  dst = (v16u8)__msa_pckev_b((v16i8)a0, (v16i8)a0);  \
} while (0)

#define CALC_B16(y0, y1, u0, u1, dst) do {           \
  const v8u16 const_a = (v8u16)__msa_fill_h(17685);  \
  const v8u16 a0 = __msa_adds_u_h((v8u16)y0, u0);    \
  const v8u16 a1 = __msa_adds_u_h((v8u16)y1, u1);    \
  v8u16 b0 = __msa_subs_u_h(a0, const_a);            \
  v8u16 b1 = __msa_subs_u_h(a1, const_a);            \
  SRAI_H2_UH(b0, b1, 6);                             \
  CLIP_UH2_0_255(b0, b1);                            \
  dst = (v16u8)__msa_pckev_b((v16i8)b1, (v16i8)b0);  \
} while (0)

#define CALC_B8(y0, u0, dst) do {                    \
  const v8u16 const_a = (v8u16)__msa_fill_h(17685);  \
  const v8u16 a0 = __msa_adds_u_h((v8u16)y0, u0);    \
  v8u16 b0 = __msa_subs_u_h(a0, const_a);            \
  b0 = SRAI_H(b0, 6);                                \
  CLIP_UH_0_255(b0);                                 \
  dst = (v16u8)__msa_pckev_b((v16i8)b0, (v16i8)b0);  \
} while (0)

#define CALC_RGB16(y, u, v, R, G, B) do {    \
  const v16u8 zero = { 0 };                  \
  v8u16 y0, y1, u0, u1, v0, v1;              \
  v4u32 p0, p1, p2, p3;                      \
  const v16u8 in_y = LD_UB(y);               \
  const v16u8 in_u = LD_UB(u);               \
  const v16u8 in_v = LD_UB(v);               \
  ILVRL_UW4(in_y, p0, p1, p2, p3);           \
  MULTHI_16(p0, p1, p2, p3, 19077, y0, y1);  \
  ILVRL_UW4(in_v, p0, p1, p2, p3);           \
  MULTHI_16(p0, p1, p2, p3, 26149, v0, v1);  \
  CALC_R16(y0, y1, v0, v1, R);               \
  MULTHI_16(p0, p1, p2, p3, 13320, v0, v1);  \
  ILVRL_UW4(in_u, p0, p1, p2, p3);           \
  MULTHI_16(p0, p1, p2, p3, 6419, u0, u1);   \
  CALC_G16(y0, y1, u0, u1, v0, v1, G);       \
  MULTHI_16(p0, p1, p2, p3, 33050, u0, u1);  \
  CALC_B16(y0, y1, u0, u1, B);               \
} while (0)

#define CALC_RGB8(y, u, v, R, G, B) do {  \
  const v16u8 zero = { 0 };               \
  v8u16 y0, u0, v0;                       \
  v4u32 p0, p1;                           \
  const v16u8 in_y = LD_UB(y);            \
  const v16u8 in_u = LD_UB(u);            \
  const v16u8 in_v = LD_UB(v);            \
  ILVR_UW2(in_y, p0, p1);                 \
  MULTHI_8(p0, p1, 19077, y0);            \
  ILVR_UW2(in_v, p0, p1);                 \
  MULTHI_8(p0, p1, 26149, v0);            \
  CALC_R8(y0, v0, R);                     \
  MULTHI_8(p0, p1, 13320, v0);            \
  ILVR_UW2(in_u, p0, p1);                 \
  MULTHI_8(p0, p1, 6419, u0);             \
  CALC_G8(y0, u0, v0, G);                 \
  MULTHI_8(p0, p1, 33050, u0);            \
  CALC_B8(y0, u0, B);                     \
} while (0)

#define STORE16_3(a0, a1, a2, dst) do {                          \
  const v16u8 mask0 = { 0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19,  \
                        8, 9, 20, 10 };                          \
  const v16u8 mask1 = { 0, 21, 1, 2, 22, 3, 4, 23, 5, 6, 24, 7,  \
                        8, 25, 9, 10 };                          \
  const v16u8 mask2 = { 26, 0, 1, 27, 2, 3, 28, 4, 5, 29, 6, 7,  \
                        30, 8, 9, 31 };                          \
  v16u8 out0, out1, out2, tmp0, tmp1, tmp2;                      \
  ILVRL_B2_UB(a1, a0, tmp0, tmp1);                               \
  out0 = VSHF_UB(tmp0, a2, mask0);                               \
  tmp2 = SLDI_UB(tmp1, tmp0, 11);                                \
  out1 = VSHF_UB(tmp2, a2, mask1);                               \
  tmp2 = SLDI_UB(tmp1, tmp1, 6);                                 \
  out2 = VSHF_UB(tmp2, a2, mask2);                               \
  ST_UB(out0, dst +  0);                                         \
  ST_UB(out1, dst + 16);                                         \
  ST_UB(out2, dst + 32);                                         \
} while (0)

#define STORE8_3(a0, a1, a2, dst) do {                             \
  int64_t out_m;                                                   \
  const v16u8 mask0 = { 0, 1, 16, 2, 3, 17, 4, 5, 18, 6, 7, 19,    \
                        8, 9, 20, 10 };                            \
  const v16u8 mask1 = { 11, 21, 12, 13, 22, 14, 15, 23,            \
                        255, 255, 255, 255, 255, 255, 255, 255 };  \
  const v16u8 tmp0 = (v16u8)__msa_ilvr_b((v16i8)a1, (v16i8)a0);    \
  v16u8 out0, out1;                                                \
  VSHF_B2_UB(tmp0, a2, tmp0, a2, mask0, mask1, out0, out1);        \
  ST_UB(out0, dst);                                                \
  out_m = __msa_copy_s_d((v2i64)out1, 0);                          \
  SD(out_m, dst + 16);                                             \
} while (0)

#define STORE16_4(a0, a1, a2, a3, dst) do {  \
  v16u8 tmp0, tmp1, tmp2, tmp3;              \
  v16u8 out0, out1, out2, out3;              \
  ILVRL_B2_UB(a1, a0, tmp0, tmp1);           \
  ILVRL_B2_UB(a3, a2, tmp2, tmp3);           \
  ILVRL_H2_UB(tmp2, tmp0, out0, out1);       \
  ILVRL_H2_UB(tmp3, tmp1, out2, out3);       \
  ST_UB(out0, dst +  0);                     \
  ST_UB(out1, dst + 16);                     \
  ST_UB(out2, dst + 32);                     \
  ST_UB(out3, dst + 48);                     \
} while (0)

#define STORE8_4(a0, a1, a2, a3, dst) do {  \
  v16u8 tmp0, tmp1, tmp2, tmp3;             \
  ILVR_B2_UB(a1, a0, a3, a2, tmp0, tmp1);   \
  ILVRL_H2_UB(tmp1, tmp0, tmp2, tmp3);      \
  ST_UB(tmp2, dst +  0);                    \
  ST_UB(tmp3, dst + 16);                    \
} while (0)

#define STORE2_16(a0, a1, dst) do {  \
  v16u8 out0, out1;                  \
  ILVRL_B2_UB(a1, a0, out0, out1);   \
  ST_UB(out0, dst +  0);             \
  ST_UB(out1, dst + 16);             \
} while (0)

#define STORE2_8(a0, a1, dst) do {                               \
  const v16u8 out0 = (v16u8)__msa_ilvr_b((v16i8)a1, (v16i8)a0);  \
  ST_UB(out0, dst);                                              \
} while (0)

#define CALC_RGBA4444(y, u, v, out0, out1, N, dst) do {  \
  CALC_RGB##N(y, u, v, R, G, B);                         \
  tmp0 = ANDI_B(R, 0xf0);                                \
  tmp1 = SRAI_B(G, 4);                                   \
  RG = tmp0 | tmp1;                                      \
  tmp0 = ANDI_B(B, 0xf0);                                \
  BA = ORI_B(tmp0, 0x0f);                                \
  STORE2_##N(out0, out1, dst);                           \
} while (0)

#define CALC_RGB565(y, u, v, out0, out1, N, dst) do {  \
  CALC_RGB##N(y, u, v, R, G, B);                       \
  tmp0 = ANDI_B(R, 0xf8);                              \
  tmp1 = SRAI_B(G, 5);                                 \
  RG = tmp0 | tmp1;                                    \
  tmp0 = SLLI_B(G, 3);                                 \
  tmp1 = ANDI_B(tmp0, 0xe0);                           \
  tmp0 = SRAI_B(B, 3);                                 \
  GB = tmp0 | tmp1;                                    \
  STORE2_##N(out0, out1, dst);                         \
} while (0)

static WEBP_INLINE int Clip8(int v) {
  return v < 0 ? 0 : v > 255 ? 255 : v;
}

static void YuvToRgb(int y, int u, int v, uint8_t* const rgb) {
  const int y1 = MultHi(y, 19077);
  const int r1 = y1 + MultHi(v, 26149) - 14234;
  const int g1 = y1 - MultHi(u, 6419) - MultHi(v, 13320) + 8708;
  const int b1 = y1 + MultHi(u, 33050) - 17685;
  rgb[0] = Clip8(r1 >> 6);
  rgb[1] = Clip8(g1 >> 6);
  rgb[2] = Clip8(b1 >> 6);
}

static void YuvToBgr(int y, int u, int v, uint8_t* const bgr) {
  const int y1 = MultHi(y, 19077);
  const int r1 = y1 + MultHi(v, 26149) - 14234;
  const int g1 = y1 - MultHi(u, 6419) - MultHi(v, 13320) + 8708;
  const int b1 = y1 + MultHi(u, 33050) - 17685;
  bgr[0] = Clip8(b1 >> 6);
  bgr[1] = Clip8(g1 >> 6);
  bgr[2] = Clip8(r1 >> 6);
}

#if !defined(WEBP_REDUCE_CSP)
static void YuvToRgb565(int y, int u, int v, uint8_t* const rgb) {
  const int y1 = MultHi(y, 19077);
  const int r1 = y1 + MultHi(v, 26149) - 14234;
  const int g1 = y1 - MultHi(u, 6419) - MultHi(v, 13320) + 8708;
  const int b1 = y1 + MultHi(u, 33050) - 17685;
  const int r = Clip8(r1 >> 6);
  const int g = Clip8(g1 >> 6);
  const int b = Clip8(b1 >> 6);
  const int rg = (r & 0xf8) | (g >> 5);
  const int gb = ((g << 3) & 0xe0) | (b >> 3);
#if (WEBP_SWAP_16BIT_CSP == 1)
  rgb[0] = gb;
  rgb[1] = rg;
#else
  rgb[0] = rg;
  rgb[1] = gb;
#endif
}

static void YuvToRgba4444(int y, int u, int v, uint8_t* const argb) {
  const int y1 = MultHi(y, 19077);
  const int r1 = y1 + MultHi(v, 26149) - 14234;
  const int g1 = y1 - MultHi(u, 6419) - MultHi(v, 13320) + 8708;
  const int b1 = y1 + MultHi(u, 33050) - 17685;
  const int r = Clip8(r1 >> 6);
  const int g = Clip8(g1 >> 6);
  const int b = Clip8(b1 >> 6);
  const int rg = (r & 0xf0) | (g >> 4);
  const int ba = (b & 0xf0) | 0x0f;     // overwrite the lower 4 bits
#if (WEBP_SWAP_16BIT_CSP == 1)
  argb[0] = ba;
  argb[1] = rg;
#else
  argb[0] = rg;
  argb[1] = ba;
#endif
}

static void YuvToArgb(uint8_t y, uint8_t u, uint8_t v, uint8_t* const argb) {
  argb[0] = 0xff;
  YuvToRgb(y, u, v, argb + 1);
}
#endif  // WEBP_REDUCE_CSP

static void YuvToBgra(uint8_t y, uint8_t u, uint8_t v, uint8_t* const bgra) {
  YuvToBgr(y, u, v, bgra);
  bgra[3] = 0xff;
}

static void YuvToRgba(uint8_t y, uint8_t u, uint8_t v, uint8_t* const rgba) {
  YuvToRgb(y, u, v, rgba);
  rgba[3] = 0xff;
}

#if !defined(WEBP_REDUCE_CSP)
static void YuvToRgbLine(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst, int length) {
  v16u8 R, G, B;
  while (length >= 16) {
    CALC_RGB16(y, u, v, R, G, B);
    STORE16_3(R, G, B, dst);
    y      += 16;
    u      += 16;
    v      += 16;
    dst    += 16 * 3;
    length -= 16;
  }
  if (length > 8) {
    uint8_t temp[3 * 16] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB16(temp, u, v, R, G, B);
    STORE16_3(R, G, B, temp);
    memcpy(dst, temp, length * 3 * sizeof(*dst));
  } else if (length > 0) {
    uint8_t temp[3 * 8] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB8(temp, u, v, R, G, B);
    STORE8_3(R, G, B, temp);
    memcpy(dst, temp, length * 3 * sizeof(*dst));
  }
}

static void YuvToBgrLine(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst, int length) {
  v16u8 R, G, B;
  while (length >= 16) {
    CALC_RGB16(y, u, v, R, G, B);
    STORE16_3(B, G, R, dst);
    y      += 16;
    u      += 16;
    v      += 16;
    dst    += 16 * 3;
    length -= 16;
  }
  if (length > 8) {
    uint8_t temp[3 * 16] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB16(temp, u, v, R, G, B);
    STORE16_3(B, G, R, temp);
    memcpy(dst, temp, length * 3 * sizeof(*dst));
  } else if (length > 0) {
    uint8_t temp[3 * 8] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB8(temp, u, v, R, G, B);
    STORE8_3(B, G, R, temp);
    memcpy(dst, temp, length * 3 * sizeof(*dst));
  }
}
#endif  // WEBP_REDUCE_CSP

static void YuvToRgbaLine(const uint8_t* WEBP_RESTRICT y,
                          const uint8_t* WEBP_RESTRICT u,
                          const uint8_t* WEBP_RESTRICT v,
                          uint8_t* WEBP_RESTRICT dst, int length) {
  v16u8 R, G, B;
  const v16u8 A = (v16u8)__msa_ldi_b(ALPHAVAL);
  while (length >= 16) {
    CALC_RGB16(y, u, v, R, G, B);
    STORE16_4(R, G, B, A, dst);
    y      += 16;
    u      += 16;
    v      += 16;
    dst    += 16 * 4;
    length -= 16;
  }
  if (length > 8) {
    uint8_t temp[4 * 16] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB16(&temp[0], u, v, R, G, B);
    STORE16_4(R, G, B, A, temp);
    memcpy(dst, temp, length * 4 * sizeof(*dst));
  } else if (length > 0) {
    uint8_t temp[4 * 8] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB8(temp, u, v, R, G, B);
    STORE8_4(R, G, B, A, temp);
    memcpy(dst, temp, length * 4 * sizeof(*dst));
  }
}

static void YuvToBgraLine(const uint8_t* WEBP_RESTRICT y,
                          const uint8_t* WEBP_RESTRICT u,
                          const uint8_t* WEBP_RESTRICT v,
                          uint8_t* WEBP_RESTRICT dst, int length) {
  v16u8 R, G, B;
  const v16u8 A = (v16u8)__msa_ldi_b(ALPHAVAL);
  while (length >= 16) {
    CALC_RGB16(y, u, v, R, G, B);
    STORE16_4(B, G, R, A, dst);
    y      += 16;
    u      += 16;
    v      += 16;
    dst    += 16 * 4;
    length -= 16;
  }
  if (length > 8) {
    uint8_t temp[4 * 16] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB16(temp, u, v, R, G, B);
    STORE16_4(B, G, R, A, temp);
    memcpy(dst, temp, length * 4 * sizeof(*dst));
  } else if (length > 0) {
    uint8_t temp[4 * 8] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB8(temp, u, v, R, G, B);
    STORE8_4(B, G, R, A, temp);
    memcpy(dst, temp, length * 4 * sizeof(*dst));
  }
}

#if !defined(WEBP_REDUCE_CSP)
static void YuvToArgbLine(const uint8_t* WEBP_RESTRICT y,
                          const uint8_t* WEBP_RESTRICT u,
                          const uint8_t* WEBP_RESTRICT v,
                          uint8_t* WEBP_RESTRICT dst, int length) {
  v16u8 R, G, B;
  const v16u8 A = (v16u8)__msa_ldi_b(ALPHAVAL);
  while (length >= 16) {
    CALC_RGB16(y, u, v, R, G, B);
    STORE16_4(A, R, G, B, dst);
    y      += 16;
    u      += 16;
    v      += 16;
    dst    += 16 * 4;
    length -= 16;
  }
  if (length > 8) {
    uint8_t temp[4 * 16] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB16(temp, u, v, R, G, B);
    STORE16_4(A, R, G, B, temp);
    memcpy(dst, temp, length * 4 * sizeof(*dst));
  } else if (length > 0) {
    uint8_t temp[4 * 8] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
    CALC_RGB8(temp, u, v, R, G, B);
    STORE8_4(A, R, G, B, temp);
    memcpy(dst, temp, length * 4 * sizeof(*dst));
  }
}

static void YuvToRgba4444Line(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int length) {
  v16u8 R, G, B, RG, BA, tmp0, tmp1;
  while (length >= 16) {
#if (WEBP_SWAP_16BIT_CSP == 1)
    CALC_RGBA4444(y, u, v, BA, RG, 16, dst);
#else
    CALC_RGBA4444(y, u, v, RG, BA, 16, dst);
#endif
    y      += 16;
    u      += 16;
    v      += 16;
    dst    += 16 * 2;
    length -= 16;
  }
  if (length > 8) {
    uint8_t temp[2 * 16] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
#if (WEBP_SWAP_16BIT_CSP == 1)
    CALC_RGBA4444(temp, u, v, BA, RG, 16, temp);
#else
    CALC_RGBA4444(temp, u, v, RG, BA, 16, temp);
#endif
    memcpy(dst, temp, length * 2 * sizeof(*dst));
  } else if (length > 0) {
    uint8_t temp[2 * 8] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
#if (WEBP_SWAP_16BIT_CSP == 1)
    CALC_RGBA4444(temp, u, v, BA, RG, 8, temp);
#else
    CALC_RGBA4444(temp, u, v, RG, BA, 8, temp);
#endif
    memcpy(dst, temp, length * 2 * sizeof(*dst));
  }
}

static void YuvToRgb565Line(const uint8_t* WEBP_RESTRICT y,
                            const uint8_t* WEBP_RESTRICT u,
                            const uint8_t* WEBP_RESTRICT v,
                            uint8_t* WEBP_RESTRICT dst, int length) {
  v16u8 R, G, B, RG, GB, tmp0, tmp1;
  while (length >= 16) {
#if (WEBP_SWAP_16BIT_CSP == 1)
    CALC_RGB565(y, u, v, GB, RG, 16, dst);
#else
    CALC_RGB565(y, u, v, RG, GB, 16, dst);
#endif
    y      += 16;
    u      += 16;
    v      += 16;
    dst    += 16 * 2;
    length -= 16;
  }
  if (length > 8) {
    uint8_t temp[2 * 16] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
#if (WEBP_SWAP_16BIT_CSP == 1)
    CALC_RGB565(temp, u, v, GB, RG, 16, temp);
#else
    CALC_RGB565(temp, u, v, RG, GB, 16, temp);
#endif
    memcpy(dst, temp, length * 2 * sizeof(*dst));
  } else if (length > 0) {
    uint8_t temp[2 * 8] = { 0 };
    memcpy(temp, y, length * sizeof(*temp));
#if (WEBP_SWAP_16BIT_CSP == 1)
    CALC_RGB565(temp, u, v, GB, RG, 8, temp);
#else
    CALC_RGB565(temp, u, v, RG, GB, 8, temp);
#endif
    memcpy(dst, temp, length * 2 * sizeof(*dst));
  }
}
#endif  // WEBP_REDUCE_CSP

#define UPSAMPLE_32PIXELS(a, b, c, d) do {    \
  v16u8 s = __msa_aver_u_b(a, d);             \
  v16u8 t = __msa_aver_u_b(b, c);             \
  const v16u8 st = s ^ t;                     \
  v16u8 ad = a ^ d;                           \
  v16u8 bc = b ^ c;                           \
  v16u8 t0 = ad | bc;                         \
  v16u8 t1 = t0 | st;                         \
  v16u8 t2 = ANDI_B(t1, 1);                   \
  v16u8 t3 = __msa_aver_u_b(s, t);            \
  const v16u8 k = t3 - t2;                    \
  v16u8 diag1, diag2;                         \
  AVER_UB2_UB(t, k, s, k, t0, t1);            \
  bc = bc & st;                               \
  ad = ad & st;                               \
  t = t ^ k;                                  \
  s = s ^ k;                                  \
  t2 = bc | t;                                \
  t3 = ad | s;                                \
  t2 = ANDI_B(t2, 1);                         \
  t3 = ANDI_B(t3, 1);                         \
  SUB2(t0, t2, t1, t3, diag1, diag2);         \
  AVER_UB2_UB(a, diag1, b, diag2, t0, t1);    \
  ILVRL_B2_UB(t1, t0, a, b);                  \
  if (pbot_y != NULL) {                       \
    AVER_UB2_UB(c, diag2, d, diag1, t0, t1);  \
    ILVRL_B2_UB(t1, t0, c, d);                \
  }                                           \
} while (0)

#define UPSAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                            \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT top_y,                \
                      const uint8_t* WEBP_RESTRICT bot_y,                \
                      const uint8_t* WEBP_RESTRICT top_u,                \
                      const uint8_t* WEBP_RESTRICT top_v,                \
                      const uint8_t* WEBP_RESTRICT cur_u,                \
                      const uint8_t* WEBP_RESTRICT cur_v,                \
                      uint8_t* WEBP_RESTRICT top_dst,                    \
                      uint8_t* WEBP_RESTRICT bot_dst, int len) {         \
  int size = (len - 1) >> 1;                                             \
  uint8_t temp_u[64];                                                    \
  uint8_t temp_v[64];                                                    \
  const uint32_t tl_uv = ((top_u[0]) | ((top_v[0]) << 16));              \
  const uint32_t l_uv = ((cur_u[0]) | ((cur_v[0]) << 16));               \
  const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;            \
  const uint8_t* ptop_y = &top_y[1];                                     \
  uint8_t* ptop_dst = top_dst + XSTEP;                                   \
  const uint8_t* pbot_y = &bot_y[1];                                     \
  uint8_t* pbot_dst = bot_dst + XSTEP;                                   \
                                                                         \
  FUNC(top_y[0], uv0 & 0xff, (uv0 >> 16), top_dst);                      \
  if (bot_y != NULL) {                                                   \
    const uint32_t uv1 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;          \
    FUNC(bot_y[0], uv1 & 0xff, (uv1 >> 16), bot_dst);                    \
  }                                                                      \
  while (size >= 16) {                                                   \
    v16u8 tu0, tu1, tv0, tv1, cu0, cu1, cv0, cv1;                        \
    LD_UB2(top_u, 1, tu0, tu1);                                          \
    LD_UB2(cur_u, 1, cu0, cu1);                                          \
    LD_UB2(top_v, 1, tv0, tv1);                                          \
    LD_UB2(cur_v, 1, cv0, cv1);                                          \
    UPSAMPLE_32PIXELS(tu0, tu1, cu0, cu1);                               \
    UPSAMPLE_32PIXELS(tv0, tv1, cv0, cv1);                               \
    ST_UB4(tu0, tu1, cu0, cu1, &temp_u[0], 16);                          \
    ST_UB4(tv0, tv1, cv0, cv1, &temp_v[0], 16);                          \
    FUNC##Line(ptop_y, &temp_u[ 0], &temp_v[0], ptop_dst, 32);           \
    if (bot_y != NULL) {                                                 \
      FUNC##Line(pbot_y, &temp_u[32], &temp_v[32], pbot_dst, 32);        \
    }                                                                    \
    ptop_y   += 32;                                                      \
    pbot_y   += 32;                                                      \
    ptop_dst += XSTEP * 32;                                              \
    pbot_dst += XSTEP * 32;                                              \
    top_u    += 16;                                                      \
    top_v    += 16;                                                      \
    cur_u    += 16;                                                      \
    cur_v    += 16;                                                      \
    size     -= 16;                                                      \
  }                                                                      \
  if (size > 0) {                                                        \
    v16u8 tu0, tu1, tv0, tv1, cu0, cu1, cv0, cv1;                        \
    memcpy(&temp_u[ 0], top_u, 17 * sizeof(uint8_t));                    \
    memcpy(&temp_u[32], cur_u, 17 * sizeof(uint8_t));                    \
    memcpy(&temp_v[ 0], top_v, 17 * sizeof(uint8_t));                    \
    memcpy(&temp_v[32], cur_v, 17 * sizeof(uint8_t));                    \
    LD_UB2(&temp_u[ 0], 1, tu0, tu1);                                    \
    LD_UB2(&temp_u[32], 1, cu0, cu1);                                    \
    LD_UB2(&temp_v[ 0], 1, tv0, tv1);                                    \
    LD_UB2(&temp_v[32], 1, cv0, cv1);                                    \
    UPSAMPLE_32PIXELS(tu0, tu1, cu0, cu1);                               \
    UPSAMPLE_32PIXELS(tv0, tv1, cv0, cv1);                               \
    ST_UB4(tu0, tu1, cu0, cu1, &temp_u[0], 16);                          \
    ST_UB4(tv0, tv1, cv0, cv1, &temp_v[0], 16);                          \
    FUNC##Line(ptop_y, &temp_u[ 0], &temp_v[0], ptop_dst, size * 2);     \
    if (bot_y != NULL) {                                                 \
      FUNC##Line(pbot_y, &temp_u[32], &temp_v[32], pbot_dst, size * 2);  \
    }                                                                    \
    top_u += size;                                                       \
    top_v += size;                                                       \
    cur_u += size;                                                       \
    cur_v += size;                                                       \
  }                                                                      \
  if (!(len & 1)) {                                                      \
    const uint32_t t0 = ((top_u[0]) | ((top_v[0]) << 16));               \
    const uint32_t c0  = ((cur_u[0]) | ((cur_v[0]) << 16));              \
    const uint32_t tmp0 = (3 * t0 + c0 + 0x00020002u) >> 2;              \
    FUNC(top_y[len - 1], tmp0 & 0xff, (tmp0 >> 16),                      \
                top_dst + (len - 1) * XSTEP);                            \
    if (bot_y != NULL) {                                                 \
      const uint32_t tmp1 = (3 * c0 + t0 + 0x00020002u) >> 2;            \
      FUNC(bot_y[len - 1], tmp1 & 0xff, (tmp1 >> 16),                    \
           bot_dst + (len - 1) * XSTEP);                                 \
    }                                                                    \
  }                                                                      \
}

UPSAMPLE_FUNC(UpsampleRgbaLinePair,     YuvToRgba,     4)
UPSAMPLE_FUNC(UpsampleBgraLinePair,     YuvToBgra,     4)
#if !defined(WEBP_REDUCE_CSP)
UPSAMPLE_FUNC(UpsampleRgbLinePair,      YuvToRgb,      3)
UPSAMPLE_FUNC(UpsampleBgrLinePair,      YuvToBgr,      3)
UPSAMPLE_FUNC(UpsampleArgbLinePair,     YuvToArgb,     4)
UPSAMPLE_FUNC(UpsampleRgba4444LinePair, YuvToRgba4444, 2)
UPSAMPLE_FUNC(UpsampleRgb565LinePair,   YuvToRgb565,   2)
#endif   // WEBP_REDUCE_CSP

//------------------------------------------------------------------------------
// Entry point

extern WebPUpsampleLinePairFunc WebPUpsamplers[/* MODE_LAST */];

extern void WebPInitUpsamplersMSA(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitUpsamplersMSA(void) {
  WebPUpsamplers[MODE_RGBA]      = UpsampleRgbaLinePair;
  WebPUpsamplers[MODE_BGRA]      = UpsampleBgraLinePair;
  WebPUpsamplers[MODE_rgbA]      = UpsampleRgbaLinePair;
  WebPUpsamplers[MODE_bgrA]      = UpsampleBgraLinePair;
#if !defined(WEBP_REDUCE_CSP)
  WebPUpsamplers[MODE_RGB]       = UpsampleRgbLinePair;
  WebPUpsamplers[MODE_BGR]       = UpsampleBgrLinePair;
  WebPUpsamplers[MODE_ARGB]      = UpsampleArgbLinePair;
  WebPUpsamplers[MODE_Argb]      = UpsampleArgbLinePair;
  WebPUpsamplers[MODE_RGB_565]   = UpsampleRgb565LinePair;
  WebPUpsamplers[MODE_RGBA_4444] = UpsampleRgba4444LinePair;
  WebPUpsamplers[MODE_rgbA_4444] = UpsampleRgba4444LinePair;
#endif   // WEBP_REDUCE_CSP
}

#endif  // FANCY_UPSAMPLING

#endif  // WEBP_USE_MSA

#if !(defined(FANCY_UPSAMPLING) && defined(WEBP_USE_MSA))
WEBP_DSP_INIT_STUB(WebPInitUpsamplersMSA)
#endif
