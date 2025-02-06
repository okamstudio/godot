// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA version of dsp functions
//
// Author(s):  Prashant Patil   (prashant.patil@imgtec.com)


#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MSA)

#include "src/dsp/msa_macro.h"

//------------------------------------------------------------------------------
// Transforms

#define IDCT_1D_W(in0, in1, in2, in3, out0, out1, out2, out3) {  \
  v4i32 a1_m, b1_m, c1_m, d1_m;                                  \
  v4i32 c_tmp1_m, c_tmp2_m, d_tmp1_m, d_tmp2_m;                  \
  const v4i32 cospi8sqrt2minus1 = __msa_fill_w(20091);           \
  const v4i32 sinpi8sqrt2 = __msa_fill_w(35468);                 \
                                                                 \
  a1_m = in0 + in2;                                              \
  b1_m = in0 - in2;                                              \
  c_tmp1_m = (in1 * sinpi8sqrt2) >> 16;                          \
  c_tmp2_m = in3 + ((in3 * cospi8sqrt2minus1) >> 16);            \
  c1_m = c_tmp1_m - c_tmp2_m;                                    \
  d_tmp1_m = in1 + ((in1 * cospi8sqrt2minus1) >> 16);            \
  d_tmp2_m = (in3 * sinpi8sqrt2) >> 16;                          \
  d1_m = d_tmp1_m + d_tmp2_m;                                    \
  BUTTERFLY_4(a1_m, b1_m, c1_m, d1_m, out0, out1, out2, out3);   \
}

static void TransformOne(const int16_t* WEBP_RESTRICT in,
                         uint8_t* WEBP_RESTRICT dst) {
  v8i16 input0, input1;
  v4i32 in0, in1, in2, in3, hz0, hz1, hz2, hz3, vt0, vt1, vt2, vt3;
  v4i32 res0, res1, res2, res3;
  const v16i8 zero = { 0 };
  v16i8 dest0, dest1, dest2, dest3;

  LD_SH2(in, 8, input0, input1);
  UNPCK_SH_SW(input0, in0, in1);
  UNPCK_SH_SW(input1, in2, in3);
  IDCT_1D_W(in0, in1, in2, in3, hz0, hz1, hz2, hz3);
  TRANSPOSE4x4_SW_SW(hz0, hz1, hz2, hz3, hz0, hz1, hz2, hz3);
  IDCT_1D_W(hz0, hz1, hz2, hz3, vt0, vt1, vt2, vt3);
  SRARI_W4_SW(vt0, vt1, vt2, vt3, 3);
  TRANSPOSE4x4_SW_SW(vt0, vt1, vt2, vt3, vt0, vt1, vt2, vt3);
  LD_SB4(dst, BPS, dest0, dest1, dest2, dest3);
  ILVR_B4_SW(zero, dest0, zero, dest1, zero, dest2, zero, dest3,
             res0, res1, res2, res3);
  ILVR_H4_SW(zero, res0, zero, res1, zero, res2, zero, res3,
             res0, res1, res2, res3);
  ADD4(res0, vt0, res1, vt1, res2, vt2, res3, vt3, res0, res1, res2, res3);
  CLIP_SW4_0_255(res0, res1, res2, res3);
  PCKEV_B2_SW(res0, res1, res2, res3, vt0, vt1);
  res0 = (v4i32)__msa_pckev_b((v16i8)vt0, (v16i8)vt1);
  ST4x4_UB(res0, res0, 3, 2, 1, 0, dst, BPS);
}

static void TransformTwo(const int16_t* WEBP_RESTRICT in,
                         uint8_t* WEBP_RESTRICT dst, int do_two) {
  TransformOne(in, dst);
  if (do_two) {
    TransformOne(in + 16, dst + 4);
  }
}

static void TransformWHT(const int16_t* WEBP_RESTRICT in,
                         int16_t* WEBP_RESTRICT out) {
  v8i16 input0, input1;
  const v8i16 mask0 = { 0, 1, 2, 3, 8, 9, 10, 11 };
  const v8i16 mask1 = { 4, 5, 6, 7, 12, 13, 14, 15 };
  const v8i16 mask2 = { 0, 4, 8, 12, 1, 5, 9, 13 };
  const v8i16 mask3 = { 3, 7, 11, 15, 2, 6, 10, 14 };
  v8i16 tmp0, tmp1, tmp2, tmp3;
  v8i16 out0, out1;

  LD_SH2(in, 8, input0, input1);
  input1 = SLDI_SH(input1, input1, 8);
  tmp0 = input0 + input1;
  tmp1 = input0 - input1;
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  out0 = tmp2 + tmp3;
  out1 = tmp2 - tmp3;
  VSHF_H2_SH(out0, out1, out0, out1, mask2, mask3, input0, input1);
  tmp0 = input0 + input1;
  tmp1 = input0 - input1;
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  tmp0 = tmp2 + tmp3;
  tmp1 = tmp2 - tmp3;
  ADDVI_H2_SH(tmp0, 3, tmp1, 3, out0, out1);
  SRAI_H2_SH(out0, out1, 3);
  out[0] = __msa_copy_s_h(out0, 0);
  out[16] = __msa_copy_s_h(out0, 4);
  out[32] = __msa_copy_s_h(out1, 0);
  out[48] = __msa_copy_s_h(out1, 4);
  out[64] = __msa_copy_s_h(out0, 1);
  out[80] = __msa_copy_s_h(out0, 5);
  out[96] = __msa_copy_s_h(out1, 1);
  out[112] = __msa_copy_s_h(out1, 5);
  out[128] = __msa_copy_s_h(out0, 2);
  out[144] = __msa_copy_s_h(out0, 6);
  out[160] = __msa_copy_s_h(out1, 2);
  out[176] = __msa_copy_s_h(out1, 6);
  out[192] = __msa_copy_s_h(out0, 3);
  out[208] = __msa_copy_s_h(out0, 7);
  out[224] = __msa_copy_s_h(out1, 3);
  out[240] = __msa_copy_s_h(out1, 7);
}

static void TransformDC(const int16_t* WEBP_RESTRICT in,
                        uint8_t* WEBP_RESTRICT dst) {
  const int DC = (in[0] + 4) >> 3;
  const v8i16 tmp0 = __msa_fill_h(DC);
  ADDBLK_ST4x4_UB(tmp0, tmp0, tmp0, tmp0, dst, BPS);
}

static void TransformAC3(const int16_t* WEBP_RESTRICT in,
                         uint8_t* WEBP_RESTRICT dst) {
  const int a = in[0] + 4;
  const int c4 = WEBP_TRANSFORM_AC3_MUL2(in[4]);
  const int d4 = WEBP_TRANSFORM_AC3_MUL1(in[4]);
  const int in2 = WEBP_TRANSFORM_AC3_MUL2(in[1]);
  const int in3 = WEBP_TRANSFORM_AC3_MUL1(in[1]);
  v4i32 tmp0 = { 0 };
  v4i32 out0 = __msa_fill_w(a + d4);
  v4i32 out1 = __msa_fill_w(a + c4);
  v4i32 out2 = __msa_fill_w(a - c4);
  v4i32 out3 = __msa_fill_w(a - d4);
  v4i32 res0, res1, res2, res3;
  const v4i32 zero = { 0 };
  v16u8 dest0, dest1, dest2, dest3;

  INSERT_W4_SW(in3, in2, -in2, -in3, tmp0);
  ADD4(out0, tmp0, out1, tmp0, out2, tmp0, out3, tmp0,
       out0, out1, out2, out3);
  SRAI_W4_SW(out0, out1, out2, out3, 3);
  LD_UB4(dst, BPS, dest0, dest1, dest2, dest3);
  ILVR_B4_SW(zero, dest0, zero, dest1, zero, dest2, zero, dest3,
             res0, res1, res2, res3);
  ILVR_H4_SW(zero, res0, zero, res1, zero, res2, zero, res3,
             res0, res1, res2, res3);
  ADD4(res0, out0, res1, out1, res2, out2, res3, out3, res0, res1, res2, res3);
  CLIP_SW4_0_255(res0, res1, res2, res3);
  PCKEV_B2_SW(res0, res1, res2, res3, out0, out1);
  res0 = (v4i32)__msa_pckev_b((v16i8)out0, (v16i8)out1);
  ST4x4_UB(res0, res0, 3, 2, 1, 0, dst, BPS);
}

//------------------------------------------------------------------------------
// Edge filtering functions

#define FLIP_SIGN2(in0, in1, out0, out1) {  \
  out0 = (v16i8)__msa_xori_b(in0, 0x80);    \
  out1 = (v16i8)__msa_xori_b(in1, 0x80);    \
}

#define FLIP_SIGN4(in0, in1, in2, in3, out0, out1, out2, out3) {  \
  FLIP_SIGN2(in0, in1, out0, out1);                               \
  FLIP_SIGN2(in2, in3, out2, out3);                               \
}

#define FILT_VAL(q0_m, p0_m, mask, filt) do {  \
  v16i8 q0_sub_p0;                             \
  q0_sub_p0 = __msa_subs_s_b(q0_m, p0_m);      \
  filt = __msa_adds_s_b(filt, q0_sub_p0);      \
  filt = __msa_adds_s_b(filt, q0_sub_p0);      \
  filt = __msa_adds_s_b(filt, q0_sub_p0);      \
  filt = filt & mask;                          \
} while (0)

#define FILT2(q_m, p_m, q, p) do {            \
  u_r = SRAI_H(temp1, 7);                     \
  u_r = __msa_sat_s_h(u_r, 7);                \
  u_l = SRAI_H(temp3, 7);                     \
  u_l = __msa_sat_s_h(u_l, 7);                \
  u = __msa_pckev_b((v16i8)u_l, (v16i8)u_r);  \
  q_m = __msa_subs_s_b(q_m, u);               \
  p_m = __msa_adds_s_b(p_m, u);               \
  q = __msa_xori_b((v16u8)q_m, 0x80);         \
  p = __msa_xori_b((v16u8)p_m, 0x80);         \
} while (0)

#define LPF_FILTER4_4W(p1, p0, q0, q1, mask, hev) do {  \
  v16i8 p1_m, p0_m, q0_m, q1_m;                         \
  v16i8 filt, t1, t2;                                   \
  const v16i8 cnst4b = __msa_ldi_b(4);                  \
  const v16i8 cnst3b = __msa_ldi_b(3);                  \
                                                        \
  FLIP_SIGN4(p1, p0, q0, q1, p1_m, p0_m, q0_m, q1_m);   \
  filt = __msa_subs_s_b(p1_m, q1_m);                    \
  filt = filt & hev;                                    \
  FILT_VAL(q0_m, p0_m, mask, filt);                     \
  t1 = __msa_adds_s_b(filt, cnst4b);                    \
  t1 = SRAI_B(t1, 3);                                   \
  t2 = __msa_adds_s_b(filt, cnst3b);                    \
  t2 = SRAI_B(t2, 3);                                   \
  q0_m = __msa_subs_s_b(q0_m, t1);                      \
  q0 = __msa_xori_b((v16u8)q0_m, 0x80);                 \
  p0_m = __msa_adds_s_b(p0_m, t2);                      \
  p0 = __msa_xori_b((v16u8)p0_m, 0x80);                 \
  filt = __msa_srari_b(t1, 1);                          \
  hev = __msa_xori_b(hev, 0xff);                        \
  filt = filt & hev;                                    \
  q1_m = __msa_subs_s_b(q1_m, filt);                    \
  q1 = __msa_xori_b((v16u8)q1_m, 0x80);                 \
  p1_m = __msa_adds_s_b(p1_m, filt);                    \
  p1 = __msa_xori_b((v16u8)p1_m, 0x80);                 \
} while (0)

#define LPF_MBFILTER(p2, p1, p0, q0, q1, q2, mask, hev) do {  \
  v16i8 p2_m, p1_m, p0_m, q2_m, q1_m, q0_m;                   \
  v16i8 u, filt, t1, t2, filt_sign;                           \
  v8i16 filt_r, filt_l, u_r, u_l;                             \
  v8i16 temp0, temp1, temp2, temp3;                           \
  const v16i8 cnst4b = __msa_ldi_b(4);                        \
  const v16i8 cnst3b = __msa_ldi_b(3);                        \
  const v8i16 cnst9h = __msa_ldi_h(9);                        \
  const v8i16 cnst63h = __msa_ldi_h(63);                      \
                                                              \
  FLIP_SIGN4(p1, p0, q0, q1, p1_m, p0_m, q0_m, q1_m);         \
  filt = __msa_subs_s_b(p1_m, q1_m);                          \
  FILT_VAL(q0_m, p0_m, mask, filt);                           \
  FLIP_SIGN2(p2, q2, p2_m, q2_m);                             \
  t2 = filt & hev;                                            \
  /* filt_val &= ~hev */                                      \
  hev = __msa_xori_b(hev, 0xff);                              \
  filt = filt & hev;                                          \
  t1 = __msa_adds_s_b(t2, cnst4b);                            \
  t1 = SRAI_B(t1, 3);                                         \
  t2 = __msa_adds_s_b(t2, cnst3b);                            \
  t2 = SRAI_B(t2, 3);                                         \
  q0_m = __msa_subs_s_b(q0_m, t1);                            \
  p0_m = __msa_adds_s_b(p0_m, t2);                            \
  filt_sign = __msa_clti_s_b(filt, 0);                        \
  ILVRL_B2_SH(filt_sign, filt, filt_r, filt_l);               \
  /* update q2/p2 */                                          \
  temp0 = filt_r * cnst9h;                                    \
  temp1 = temp0 + cnst63h;                                    \
  temp2 = filt_l * cnst9h;                                    \
  temp3 = temp2 + cnst63h;                                    \
  FILT2(q2_m, p2_m, q2, p2);                                  \
  /* update q1/p1 */                                          \
  temp1 = temp1 + temp0;                                      \
  temp3 = temp3 + temp2;                                      \
  FILT2(q1_m, p1_m, q1, p1);                                  \
  /* update q0/p0 */                                          \
  temp1 = temp1 + temp0;                                      \
  temp3 = temp3 + temp2;                                      \
  FILT2(q0_m, p0_m, q0, p0);                                  \
} while (0)

#define LPF_MASK_HEV(p3_in, p2_in, p1_in, p0_in,                 \
                     q0_in, q1_in, q2_in, q3_in,                 \
                     limit_in, b_limit_in, thresh_in,            \
                     hev_out, mask_out) do {                     \
  v16u8 p3_asub_p2_m, p2_asub_p1_m, p1_asub_p0_m, q1_asub_q0_m;  \
  v16u8 p1_asub_q1_m, p0_asub_q0_m, q3_asub_q2_m, q2_asub_q1_m;  \
  v16u8 flat_out;                                                \
                                                                 \
  /* absolute subtraction of pixel values */                     \
  p3_asub_p2_m = __msa_asub_u_b(p3_in, p2_in);                   \
  p2_asub_p1_m = __msa_asub_u_b(p2_in, p1_in);                   \
  p1_asub_p0_m = __msa_asub_u_b(p1_in, p0_in);                   \
  q1_asub_q0_m = __msa_asub_u_b(q1_in, q0_in);                   \
  q2_asub_q1_m = __msa_asub_u_b(q2_in, q1_in);                   \
  q3_asub_q2_m = __msa_asub_u_b(q3_in, q2_in);                   \
  p0_asub_q0_m = __msa_asub_u_b(p0_in, q0_in);                   \
  p1_asub_q1_m = __msa_asub_u_b(p1_in, q1_in);                   \
  /* calculation of hev */                                       \
  flat_out = __msa_max_u_b(p1_asub_p0_m, q1_asub_q0_m);          \
  hev_out = (thresh_in < flat_out);                              \
  /* calculation of mask */                                      \
  p0_asub_q0_m = __msa_adds_u_b(p0_asub_q0_m, p0_asub_q0_m);     \
  p1_asub_q1_m = SRAI_B(p1_asub_q1_m, 1);                        \
  p0_asub_q0_m = __msa_adds_u_b(p0_asub_q0_m, p1_asub_q1_m);     \
  mask_out = (b_limit_in < p0_asub_q0_m);                        \
  mask_out = __msa_max_u_b(flat_out, mask_out);                  \
  p3_asub_p2_m = __msa_max_u_b(p3_asub_p2_m, p2_asub_p1_m);      \
  mask_out = __msa_max_u_b(p3_asub_p2_m, mask_out);              \
  q2_asub_q1_m = __msa_max_u_b(q2_asub_q1_m, q3_asub_q2_m);      \
  mask_out = __msa_max_u_b(q2_asub_q1_m, mask_out);              \
  mask_out = (limit_in < mask_out);                              \
  mask_out = __msa_xori_b(mask_out, 0xff);                       \
} while (0)

#define ST6x1_UB(in0, in0_idx, in1, in1_idx, pdst, stride) do { \
  const uint16_t tmp0_h = __msa_copy_s_h((v8i16)in1, in1_idx);  \
  const uint32_t tmp0_w = __msa_copy_s_w((v4i32)in0, in0_idx);  \
  SW(tmp0_w, pdst);                                             \
  SH(tmp0_h, pdst + stride);                                    \
} while (0)

#define ST6x4_UB(in0, start_in0_idx, in1, start_in1_idx, pdst, stride) do { \
  uint8_t* ptmp1 = (uint8_t*)pdst;                                          \
  ST6x1_UB(in0, start_in0_idx, in1, start_in1_idx, ptmp1, 4);               \
  ptmp1 += stride;                                                          \
  ST6x1_UB(in0, start_in0_idx + 1, in1, start_in1_idx + 1, ptmp1, 4);       \
  ptmp1 += stride;                                                          \
  ST6x1_UB(in0, start_in0_idx + 2, in1, start_in1_idx + 2, ptmp1, 4);       \
  ptmp1 += stride;                                                          \
  ST6x1_UB(in0, start_in0_idx + 3, in1, start_in1_idx + 3, ptmp1, 4);       \
} while (0)

#define LPF_SIMPLE_FILT(p1_in, p0_in, q0_in, q1_in, mask) do {       \
    v16i8 p1_m, p0_m, q0_m, q1_m, filt, filt1, filt2;                \
    const v16i8 cnst4b = __msa_ldi_b(4);                             \
    const v16i8 cnst3b =  __msa_ldi_b(3);                            \
                                                                     \
    FLIP_SIGN4(p1_in, p0_in, q0_in, q1_in, p1_m, p0_m, q0_m, q1_m);  \
    filt = __msa_subs_s_b(p1_m, q1_m);                               \
    FILT_VAL(q0_m, p0_m, mask, filt);                                \
    filt1 = __msa_adds_s_b(filt, cnst4b);                            \
    filt1 = SRAI_B(filt1, 3);                                        \
    filt2 = __msa_adds_s_b(filt, cnst3b);                            \
    filt2 = SRAI_B(filt2, 3);                                        \
    q0_m = __msa_subs_s_b(q0_m, filt1);                              \
    p0_m = __msa_adds_s_b(p0_m, filt2);                              \
    q0_in = __msa_xori_b((v16u8)q0_m, 0x80);                         \
    p0_in = __msa_xori_b((v16u8)p0_m, 0x80);                         \
} while (0)

#define LPF_SIMPLE_MASK(p1, p0, q0, q1, b_limit, mask) do {    \
    v16u8 p1_a_sub_q1, p0_a_sub_q0;                            \
                                                               \
    p0_a_sub_q0 = __msa_asub_u_b(p0, q0);                      \
    p1_a_sub_q1 = __msa_asub_u_b(p1, q1);                      \
    p1_a_sub_q1 = (v16u8)__msa_srli_b((v16i8)p1_a_sub_q1, 1);  \
    p0_a_sub_q0 = __msa_adds_u_b(p0_a_sub_q0, p0_a_sub_q0);    \
    mask = __msa_adds_u_b(p0_a_sub_q0, p1_a_sub_q1);           \
    mask = (mask <= b_limit);                                  \
} while (0)

static void VFilter16(uint8_t* src, int stride,
                      int b_limit_in, int limit_in, int thresh_in) {
  uint8_t* ptemp = src - 4 * stride;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0;
  v16u8 mask, hev;
  const v16u8 thresh = (v16u8)__msa_fill_b(thresh_in);
  const v16u8 limit = (v16u8)__msa_fill_b(limit_in);
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);

  LD_UB8(ptemp, stride, p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit, b_limit, thresh,
               hev, mask);
  LPF_MBFILTER(p2, p1, p0, q0, q1, q2, mask, hev);
  ptemp = src - 3 * stride;
  ST_UB4(p2, p1, p0, q0, ptemp, stride);
  ptemp += (4 * stride);
  ST_UB2(q1, q2, ptemp, stride);
}

static void HFilter16(uint8_t* src, int stride,
                      int b_limit_in, int limit_in, int thresh_in) {
  uint8_t* ptmp  = src - 4;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0;
  v16u8 mask, hev;
  v16u8 row0, row1, row2, row3, row4, row5, row6, row7, row8;
  v16u8 row9, row10, row11, row12, row13, row14, row15;
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);
  const v16u8 limit = (v16u8)__msa_fill_b(limit_in);
  const v16u8 thresh = (v16u8)__msa_fill_b(thresh_in);

  LD_UB8(ptmp, stride, row0, row1, row2, row3, row4, row5, row6, row7);
  ptmp += (8 * stride);
  LD_UB8(ptmp, stride, row8, row9, row10, row11, row12, row13, row14, row15);
  TRANSPOSE16x8_UB_UB(row0, row1, row2, row3, row4, row5, row6, row7,
                      row8, row9, row10, row11, row12, row13, row14, row15,
                      p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit, b_limit, thresh,
               hev, mask);
  LPF_MBFILTER(p2, p1, p0, q0, q1, q2, mask, hev);
  ILVR_B2_SH(p1, p2, q0, p0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp3, tmp4);
  ILVL_B2_SH(p1, p2, q0, p0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp6, tmp7);
  ILVRL_B2_SH(q2, q1, tmp2, tmp5);
  ptmp = src - 3;
  ST6x1_UB(tmp3, 0, tmp2, 0, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp3, 1, tmp2, 1, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp3, 2, tmp2, 2, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp3, 3, tmp2, 3, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp4, 0, tmp2, 4, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp4, 1, tmp2, 5, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp4, 2, tmp2, 6, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp4, 3, tmp2, 7, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp6, 0, tmp5, 0, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp6, 1, tmp5, 1, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp6, 2, tmp5, 2, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp6, 3, tmp5, 3, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp7, 0, tmp5, 4, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp7, 1, tmp5, 5, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp7, 2, tmp5, 6, ptmp, 4);
  ptmp += stride;
  ST6x1_UB(tmp7, 3, tmp5, 7, ptmp, 4);
}

// on three inner edges
static void VFilterHorEdge16i(uint8_t* src, int stride,
                              int b_limit, int limit, int thresh) {
  v16u8 mask, hev;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0;
  const v16u8 thresh0 = (v16u8)__msa_fill_b(thresh);
  const v16u8 b_limit0 = (v16u8)__msa_fill_b(b_limit);
  const v16u8 limit0 = (v16u8)__msa_fill_b(limit);

  LD_UB8((src - 4 * stride), stride, p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit0, b_limit0, thresh0,
               hev, mask);
  LPF_FILTER4_4W(p1, p0, q0, q1, mask, hev);
  ST_UB4(p1, p0, q0, q1, (src - 2 * stride), stride);
}

static void VFilter16i(uint8_t* src_y, int stride,
                       int b_limit, int limit, int thresh) {
  VFilterHorEdge16i(src_y +  4 * stride, stride, b_limit, limit, thresh);
  VFilterHorEdge16i(src_y +  8 * stride, stride, b_limit, limit, thresh);
  VFilterHorEdge16i(src_y + 12 * stride, stride, b_limit, limit, thresh);
}

static void HFilterVertEdge16i(uint8_t* src, int stride,
                               int b_limit, int limit, int thresh) {
  v16u8 mask, hev;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0;
  v16u8 row0, row1, row2, row3, row4, row5, row6, row7;
  v16u8 row8, row9, row10, row11, row12, row13, row14, row15;
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
  const v16u8 thresh0 = (v16u8)__msa_fill_b(thresh);
  const v16u8 b_limit0 = (v16u8)__msa_fill_b(b_limit);
  const v16u8 limit0 = (v16u8)__msa_fill_b(limit);

  LD_UB8(src - 4, stride, row0, row1, row2, row3, row4, row5, row6, row7);
  LD_UB8(src - 4 + (8 * stride), stride,
         row8, row9, row10, row11, row12, row13, row14, row15);
  TRANSPOSE16x8_UB_UB(row0, row1, row2, row3, row4, row5, row6, row7,
                      row8, row9, row10, row11, row12, row13, row14, row15,
                      p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit0, b_limit0, thresh0,
               hev, mask);
  LPF_FILTER4_4W(p1, p0, q0, q1, mask, hev);
  ILVR_B2_SH(p0, p1, q1, q0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp2, tmp3);
  ILVL_B2_SH(p0, p1, q1, q0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp4, tmp5);
  src -= 2;
  ST4x8_UB(tmp2, tmp3, src, stride);
  src += (8 * stride);
  ST4x8_UB(tmp4, tmp5, src, stride);
}

static void HFilter16i(uint8_t* src_y, int stride,
                       int b_limit, int limit, int thresh) {
  HFilterVertEdge16i(src_y +  4, stride, b_limit, limit, thresh);
  HFilterVertEdge16i(src_y +  8, stride, b_limit, limit, thresh);
  HFilterVertEdge16i(src_y + 12, stride, b_limit, limit, thresh);
}

// 8-pixels wide variants, for chroma filtering
static void VFilter8(uint8_t* WEBP_RESTRICT src_u, uint8_t* WEBP_RESTRICT src_v,
                     int stride, int b_limit_in, int limit_in, int thresh_in) {
  uint8_t* ptmp_src_u = src_u - 4 * stride;
  uint8_t* ptmp_src_v = src_v - 4 * stride;
  uint64_t p2_d, p1_d, p0_d, q0_d, q1_d, q2_d;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0, mask, hev;
  v16u8 p3_u, p2_u, p1_u, p0_u, q3_u, q2_u, q1_u, q0_u;
  v16u8 p3_v, p2_v, p1_v, p0_v, q3_v, q2_v, q1_v, q0_v;
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);
  const v16u8 limit = (v16u8)__msa_fill_b(limit_in);
  const v16u8 thresh = (v16u8)__msa_fill_b(thresh_in);

  LD_UB8(ptmp_src_u, stride, p3_u, p2_u, p1_u, p0_u, q0_u, q1_u, q2_u, q3_u);
  LD_UB8(ptmp_src_v, stride, p3_v, p2_v, p1_v, p0_v, q0_v, q1_v, q2_v, q3_v);
  ILVR_D4_UB(p3_v, p3_u, p2_v, p2_u, p1_v, p1_u, p0_v, p0_u, p3, p2, p1, p0);
  ILVR_D4_UB(q0_v, q0_u, q1_v, q1_u, q2_v, q2_u, q3_v, q3_u, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit, b_limit, thresh,
               hev, mask);
  LPF_MBFILTER(p2, p1, p0, q0, q1, q2, mask, hev);
  p2_d = __msa_copy_s_d((v2i64)p2, 0);
  p1_d = __msa_copy_s_d((v2i64)p1, 0);
  p0_d = __msa_copy_s_d((v2i64)p0, 0);
  q0_d = __msa_copy_s_d((v2i64)q0, 0);
  q1_d = __msa_copy_s_d((v2i64)q1, 0);
  q2_d = __msa_copy_s_d((v2i64)q2, 0);
  ptmp_src_u += stride;
  SD4(p2_d, p1_d, p0_d, q0_d, ptmp_src_u, stride);
  ptmp_src_u += (4 * stride);
  SD(q1_d, ptmp_src_u);
  ptmp_src_u += stride;
  SD(q2_d, ptmp_src_u);
  p2_d = __msa_copy_s_d((v2i64)p2, 1);
  p1_d = __msa_copy_s_d((v2i64)p1, 1);
  p0_d = __msa_copy_s_d((v2i64)p0, 1);
  q0_d = __msa_copy_s_d((v2i64)q0, 1);
  q1_d = __msa_copy_s_d((v2i64)q1, 1);
  q2_d = __msa_copy_s_d((v2i64)q2, 1);
  ptmp_src_v += stride;
  SD4(p2_d, p1_d, p0_d, q0_d, ptmp_src_v, stride);
  ptmp_src_v += (4 * stride);
  SD(q1_d, ptmp_src_v);
  ptmp_src_v += stride;
  SD(q2_d, ptmp_src_v);
}

static void HFilter8(uint8_t* WEBP_RESTRICT src_u, uint8_t* WEBP_RESTRICT src_v,
                     int stride, int b_limit_in, int limit_in, int thresh_in) {
  uint8_t* ptmp_src_u = src_u - 4;
  uint8_t* ptmp_src_v = src_v - 4;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0, mask, hev;
  v16u8 row0, row1, row2, row3, row4, row5, row6, row7, row8;
  v16u8 row9, row10, row11, row12, row13, row14, row15;
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);
  const v16u8 limit = (v16u8)__msa_fill_b(limit_in);
  const v16u8 thresh = (v16u8)__msa_fill_b(thresh_in);

  LD_UB8(ptmp_src_u, stride, row0, row1, row2, row3, row4, row5, row6, row7);
  LD_UB8(ptmp_src_v, stride,
         row8, row9, row10, row11, row12, row13, row14, row15);
  TRANSPOSE16x8_UB_UB(row0, row1, row2, row3, row4, row5, row6, row7,
                      row8, row9, row10, row11, row12, row13, row14, row15,
                      p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit, b_limit, thresh,
               hev, mask);
  LPF_MBFILTER(p2, p1, p0, q0, q1, q2, mask, hev);
  ILVR_B2_SH(p1, p2, q0, p0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp3, tmp4);
  ILVL_B2_SH(p1, p2, q0, p0, tmp0, tmp1);
  ILVRL_H2_SH(tmp1, tmp0, tmp6, tmp7);
  ILVRL_B2_SH(q2, q1, tmp2, tmp5);
  ptmp_src_u += 1;
  ST6x4_UB(tmp3, 0, tmp2, 0, ptmp_src_u, stride);
  ptmp_src_u += 4 * stride;
  ST6x4_UB(tmp4, 0, tmp2, 4, ptmp_src_u, stride);
  ptmp_src_v += 1;
  ST6x4_UB(tmp6, 0, tmp5, 0, ptmp_src_v, stride);
  ptmp_src_v += 4 * stride;
  ST6x4_UB(tmp7, 0, tmp5, 4, ptmp_src_v, stride);
}

static void VFilter8i(uint8_t* WEBP_RESTRICT src_u,
                      uint8_t* WEBP_RESTRICT src_v, int stride,
                      int b_limit_in, int limit_in, int thresh_in) {
  uint64_t p1_d, p0_d, q0_d, q1_d;
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0, mask, hev;
  v16u8 p3_u, p2_u, p1_u, p0_u, q3_u, q2_u, q1_u, q0_u;
  v16u8 p3_v, p2_v, p1_v, p0_v, q3_v, q2_v, q1_v, q0_v;
  const v16u8 thresh = (v16u8)__msa_fill_b(thresh_in);
  const v16u8 limit = (v16u8)__msa_fill_b(limit_in);
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);

  LD_UB8(src_u, stride, p3_u, p2_u, p1_u, p0_u, q0_u, q1_u, q2_u, q3_u);
  src_u += (5 * stride);
  LD_UB8(src_v, stride, p3_v, p2_v, p1_v, p0_v, q0_v, q1_v, q2_v, q3_v);
  src_v += (5 * stride);
  ILVR_D4_UB(p3_v, p3_u, p2_v, p2_u, p1_v, p1_u, p0_v, p0_u, p3, p2, p1, p0);
  ILVR_D4_UB(q0_v, q0_u, q1_v, q1_u, q2_v, q2_u, q3_v, q3_u, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit, b_limit, thresh,
               hev, mask);
  LPF_FILTER4_4W(p1, p0, q0, q1, mask, hev);
  p1_d = __msa_copy_s_d((v2i64)p1, 0);
  p0_d = __msa_copy_s_d((v2i64)p0, 0);
  q0_d = __msa_copy_s_d((v2i64)q0, 0);
  q1_d = __msa_copy_s_d((v2i64)q1, 0);
  SD4(q1_d, q0_d, p0_d, p1_d, src_u, -stride);
  p1_d = __msa_copy_s_d((v2i64)p1, 1);
  p0_d = __msa_copy_s_d((v2i64)p0, 1);
  q0_d = __msa_copy_s_d((v2i64)q0, 1);
  q1_d = __msa_copy_s_d((v2i64)q1, 1);
  SD4(q1_d, q0_d, p0_d, p1_d, src_v, -stride);
}

static void HFilter8i(uint8_t* WEBP_RESTRICT src_u,
                      uint8_t* WEBP_RESTRICT src_v, int stride,
                      int b_limit_in, int limit_in, int thresh_in) {
  v16u8 p3, p2, p1, p0, q3, q2, q1, q0, mask, hev;
  v16u8 row0, row1, row2, row3, row4, row5, row6, row7, row8;
  v16u8 row9, row10, row11, row12, row13, row14, row15;
  v4i32 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
  const v16u8 thresh = (v16u8)__msa_fill_b(thresh_in);
  const v16u8 limit = (v16u8)__msa_fill_b(limit_in);
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);

  LD_UB8(src_u, stride, row0, row1, row2, row3, row4, row5, row6, row7);
  LD_UB8(src_v, stride,
         row8, row9, row10, row11, row12, row13, row14, row15);
  TRANSPOSE16x8_UB_UB(row0, row1, row2, row3, row4, row5, row6, row7,
                      row8, row9, row10, row11, row12, row13, row14, row15,
                      p3, p2, p1, p0, q0, q1, q2, q3);
  LPF_MASK_HEV(p3, p2, p1, p0, q0, q1, q2, q3, limit, b_limit, thresh,
               hev, mask);
  LPF_FILTER4_4W(p1, p0, q0, q1, mask, hev);
  ILVR_B2_SW(p0, p1, q1, q0, tmp0, tmp1);
  ILVRL_H2_SW(tmp1, tmp0, tmp2, tmp3);
  ILVL_B2_SW(p0, p1, q1, q0, tmp0, tmp1);
  ILVRL_H2_SW(tmp1, tmp0, tmp4, tmp5);
  src_u += 2;
  ST4x4_UB(tmp2, tmp2, 0, 1, 2, 3, src_u, stride);
  src_u += 4 * stride;
  ST4x4_UB(tmp3, tmp3, 0, 1, 2, 3, src_u, stride);
  src_v += 2;
  ST4x4_UB(tmp4, tmp4, 0, 1, 2, 3, src_v, stride);
  src_v += 4 * stride;
  ST4x4_UB(tmp5, tmp5, 0, 1, 2, 3, src_v, stride);
}

static void SimpleVFilter16(uint8_t* src, int stride, int b_limit_in) {
  v16u8 p1, p0, q1, q0, mask;
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);

  LD_UB4(src - 2 * stride, stride, p1, p0, q0, q1);
  LPF_SIMPLE_MASK(p1, p0, q0, q1, b_limit, mask);
  LPF_SIMPLE_FILT(p1, p0, q0, q1, mask);
  ST_UB2(p0, q0, src - stride, stride);
}

static void SimpleHFilter16(uint8_t* src, int stride, int b_limit_in) {
  v16u8 p1, p0, q1, q0, mask, row0, row1, row2, row3, row4, row5, row6, row7;
  v16u8 row8, row9, row10, row11, row12, row13, row14, row15;
  v8i16 tmp0, tmp1;
  const v16u8 b_limit = (v16u8)__msa_fill_b(b_limit_in);
  uint8_t* ptemp_src = src - 2;

  LD_UB8(ptemp_src, stride, row0, row1, row2, row3, row4, row5, row6, row7);
  LD_UB8(ptemp_src + 8 * stride, stride,
         row8, row9, row10, row11, row12, row13, row14, row15);
  TRANSPOSE16x4_UB_UB(row0, row1, row2, row3, row4, row5, row6, row7,
                      row8, row9, row10, row11, row12, row13, row14, row15,
                      p1, p0, q0, q1);
  LPF_SIMPLE_MASK(p1, p0, q0, q1, b_limit, mask);
  LPF_SIMPLE_FILT(p1, p0, q0, q1, mask);
  ILVRL_B2_SH(q0, p0, tmp1, tmp0);
  ptemp_src += 1;
  ST2x4_UB(tmp1, 0, ptemp_src, stride);
  ptemp_src += 4 * stride;
  ST2x4_UB(tmp1, 4, ptemp_src, stride);
  ptemp_src += 4 * stride;
  ST2x4_UB(tmp0, 0, ptemp_src, stride);
  ptemp_src += 4 * stride;
  ST2x4_UB(tmp0, 4, ptemp_src, stride);
  ptemp_src += 4 * stride;
}

static void SimpleVFilter16i(uint8_t* src_y, int stride, int b_limit_in) {
  SimpleVFilter16(src_y +  4 * stride, stride, b_limit_in);
  SimpleVFilter16(src_y +  8 * stride, stride, b_limit_in);
  SimpleVFilter16(src_y + 12 * stride, stride, b_limit_in);
}

static void SimpleHFilter16i(uint8_t* src_y, int stride, int b_limit_in) {
  SimpleHFilter16(src_y +  4, stride, b_limit_in);
  SimpleHFilter16(src_y +  8, stride, b_limit_in);
  SimpleHFilter16(src_y + 12, stride, b_limit_in);
}

//------------------------------------------------------------------------------
// Intra predictions
//------------------------------------------------------------------------------

// 4x4

static void DC4(uint8_t* dst) {   // DC
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i) dc += dst[i - BPS] + dst[-1 + i * BPS];
  dc >>= 3;
  dc = dc | (dc << 8) | (dc << 16) | (dc << 24);
  SW4(dc, dc, dc, dc, dst, BPS);
}

static void TM4(uint8_t* dst) {
  const uint8_t* const ptemp = dst - BPS - 1;
  v8i16 T, d, r0, r1, r2, r3;
  const v16i8 zero = { 0 };
  const v8i16 TL = (v8i16)__msa_fill_h(ptemp[0 * BPS]);
  const v8i16 L0 = (v8i16)__msa_fill_h(ptemp[1 * BPS]);
  const v8i16 L1 = (v8i16)__msa_fill_h(ptemp[2 * BPS]);
  const v8i16 L2 = (v8i16)__msa_fill_h(ptemp[3 * BPS]);
  const v8i16 L3 = (v8i16)__msa_fill_h(ptemp[4 * BPS]);
  const v16u8 T1 = LD_UB(ptemp + 1);

  T  = (v8i16)__msa_ilvr_b(zero, (v16i8)T1);
  d = T - TL;
  ADD4(d, L0, d, L1, d, L2, d, L3, r0, r1, r2, r3);
  CLIP_SH4_0_255(r0, r1, r2, r3);
  PCKEV_ST4x4_UB(r0, r1, r2, r3, dst, BPS);
}

static void VE4(uint8_t* dst) {    // vertical
  const uint8_t* const ptop = dst - BPS - 1;
  const uint32_t val0 = LW(ptop + 0);
  const uint32_t val1 = LW(ptop + 4);
  uint32_t out;
  v16u8 A = { 0 }, B, C, AC, B2, R;

  INSERT_W2_UB(val0, val1, A);
  B = SLDI_UB(A, A, 1);
  C = SLDI_UB(A, A, 2);
  AC = __msa_ave_u_b(A, C);
  B2 = __msa_ave_u_b(B, B);
  R = __msa_aver_u_b(AC, B2);
  out = __msa_copy_s_w((v4i32)R, 0);
  SW4(out, out, out, out, dst, BPS);
}

static void RD4(uint8_t* dst) {   // Down-right
  const uint8_t* const ptop = dst - 1 - BPS;
  uint32_t val0 = LW(ptop + 0);
  uint32_t val1 = LW(ptop + 4);
  uint32_t val2, val3;
  v16u8 A, B, C, AC, B2, R, A1 = { 0 };

  INSERT_W2_UB(val0, val1, A1);
  A = SLDI_UB(A1, A1, 12);
  A = (v16u8)__msa_insert_b((v16i8)A, 3, ptop[1 * BPS]);
  A = (v16u8)__msa_insert_b((v16i8)A, 2, ptop[2 * BPS]);
  A = (v16u8)__msa_insert_b((v16i8)A, 1, ptop[3 * BPS]);
  A = (v16u8)__msa_insert_b((v16i8)A, 0, ptop[4 * BPS]);
  B = SLDI_UB(A, A, 1);
  C = SLDI_UB(A, A, 2);
  AC = __msa_ave_u_b(A, C);
  B2 = __msa_ave_u_b(B, B);
  R = __msa_aver_u_b(AC, B2);
  val3 = __msa_copy_s_w((v4i32)R, 0);
  R = SLDI_UB(R, R, 1);
  val2 = __msa_copy_s_w((v4i32)R, 0);
  R = SLDI_UB(R, R, 1);
  val1 = __msa_copy_s_w((v4i32)R, 0);
  R = SLDI_UB(R, R, 1);
  val0 = __msa_copy_s_w((v4i32)R, 0);
  SW4(val0, val1, val2, val3, dst, BPS);
}

static void LD4(uint8_t* dst) {   // Down-Left
  const uint8_t* const ptop = dst - BPS;
  uint32_t val0 = LW(ptop + 0);
  uint32_t val1 = LW(ptop + 4);
  uint32_t val2, val3;
  v16u8 A = { 0 }, B, C, AC, B2, R;

  INSERT_W2_UB(val0, val1, A);
  B = SLDI_UB(A, A, 1);
  C = SLDI_UB(A, A, 2);
  C = (v16u8)__msa_insert_b((v16i8)C, 6, ptop[7]);
  AC = __msa_ave_u_b(A, C);
  B2 = __msa_ave_u_b(B, B);
  R = __msa_aver_u_b(AC, B2);
  val0 = __msa_copy_s_w((v4i32)R, 0);
  R = SLDI_UB(R, R, 1);
  val1 = __msa_copy_s_w((v4i32)R, 0);
  R = SLDI_UB(R, R, 1);
  val2 = __msa_copy_s_w((v4i32)R, 0);
  R = SLDI_UB(R, R, 1);
  val3 = __msa_copy_s_w((v4i32)R, 0);
  SW4(val0, val1, val2, val3, dst, BPS);
}

// 16x16

static void DC16(uint8_t* dst) {   // DC
  uint32_t dc = 16;
  int i;
  const v16u8 rtop = LD_UB(dst - BPS);
  const v8u16 dctop = __msa_hadd_u_h(rtop, rtop);
  v16u8 out;

  for (i = 0; i < 16; ++i) {
    dc += dst[-1 + i * BPS];
  }
  dc += HADD_UH_U32(dctop);
  out = (v16u8)__msa_fill_b(dc >> 5);
  ST_UB8(out, out, out, out, out, out, out, out, dst, BPS);
  ST_UB8(out, out, out, out, out, out, out, out, dst + 8 * BPS, BPS);
}

static void TM16(uint8_t* dst) {
  int j;
  v8i16 d1, d2;
  const v16i8 zero = { 0 };
  const v8i16 TL = (v8i16)__msa_fill_h(dst[-1 - BPS]);
  const v16i8 T = LD_SB(dst - BPS);

  ILVRL_B2_SH(zero, T, d1, d2);
  SUB2(d1, TL, d2, TL, d1, d2);
  for (j = 0; j < 16; j += 4) {
    v16i8 t0, t1, t2, t3;
    v8i16 r0, r1, r2, r3, r4, r5, r6, r7;
    const v8i16 L0 = (v8i16)__msa_fill_h(dst[-1 + 0 * BPS]);
    const v8i16 L1 = (v8i16)__msa_fill_h(dst[-1 + 1 * BPS]);
    const v8i16 L2 = (v8i16)__msa_fill_h(dst[-1 + 2 * BPS]);
    const v8i16 L3 = (v8i16)__msa_fill_h(dst[-1 + 3 * BPS]);
    ADD4(d1, L0, d1, L1, d1, L2, d1, L3, r0, r1, r2, r3);
    ADD4(d2, L0, d2, L1, d2, L2, d2, L3, r4, r5, r6, r7);
    CLIP_SH4_0_255(r0, r1, r2, r3);
    CLIP_SH4_0_255(r4, r5, r6, r7);
    PCKEV_B4_SB(r4, r0, r5, r1, r6, r2, r7, r3, t0, t1, t2, t3);
    ST_SB4(t0, t1, t2, t3, dst, BPS);
    dst += 4 * BPS;
  }
}

static void VE16(uint8_t* dst) {   // vertical
  const v16u8 rtop = LD_UB(dst - BPS);
  ST_UB8(rtop, rtop, rtop, rtop, rtop, rtop, rtop, rtop, dst, BPS);
  ST_UB8(rtop, rtop, rtop, rtop, rtop, rtop, rtop, rtop, dst + 8 * BPS, BPS);
}

static void HE16(uint8_t* dst) {   // horizontal
  int j;
  for (j = 16; j > 0; j -= 4) {
    const v16u8 L0 = (v16u8)__msa_fill_b(dst[-1 + 0 * BPS]);
    const v16u8 L1 = (v16u8)__msa_fill_b(dst[-1 + 1 * BPS]);
    const v16u8 L2 = (v16u8)__msa_fill_b(dst[-1 + 2 * BPS]);
    const v16u8 L3 = (v16u8)__msa_fill_b(dst[-1 + 3 * BPS]);
    ST_UB4(L0, L1, L2, L3, dst, BPS);
    dst += 4 * BPS;
  }
}

static void DC16NoTop(uint8_t* dst) {   // DC with top samples not available
  int j;
  uint32_t dc = 8;
  v16u8 out;

  for (j = 0; j < 16; ++j) {
    dc += dst[-1 + j * BPS];
  }
  out = (v16u8)__msa_fill_b(dc >> 4);
  ST_UB8(out, out, out, out, out, out, out, out, dst, BPS);
  ST_UB8(out, out, out, out, out, out, out, out, dst + 8 * BPS, BPS);
}

static void DC16NoLeft(uint8_t* dst) {   // DC with left samples not available
  uint32_t dc = 8;
  const v16u8 rtop = LD_UB(dst - BPS);
  const v8u16 dctop = __msa_hadd_u_h(rtop, rtop);
  v16u8 out;

  dc += HADD_UH_U32(dctop);
  out = (v16u8)__msa_fill_b(dc >> 4);
  ST_UB8(out, out, out, out, out, out, out, out, dst, BPS);
  ST_UB8(out, out, out, out, out, out, out, out, dst + 8 * BPS, BPS);
}

static void DC16NoTopLeft(uint8_t* dst) {   // DC with nothing
  const v16u8 out = (v16u8)__msa_fill_b(0x80);
  ST_UB8(out, out, out, out, out, out, out, out, dst, BPS);
  ST_UB8(out, out, out, out, out, out, out, out, dst + 8 * BPS, BPS);
}

// Chroma

#define STORE8x8(out, dst) do {                 \
  SD4(out, out, out, out, dst + 0 * BPS, BPS);  \
  SD4(out, out, out, out, dst + 4 * BPS, BPS);  \
} while (0)

static void DC8uv(uint8_t* dst) {   // DC
  uint32_t dc = 8;
  int i;
  uint64_t out;
  const v16u8 rtop = LD_UB(dst - BPS);
  const v8u16 temp0 = __msa_hadd_u_h(rtop, rtop);
  const v4u32 temp1 = __msa_hadd_u_w(temp0, temp0);
  const v2u64 temp2 = __msa_hadd_u_d(temp1, temp1);
  v16u8 dctemp;

  for (i = 0; i < 8; ++i) {
    dc += dst[-1 + i * BPS];
  }
  dc += __msa_copy_s_w((v4i32)temp2, 0);
  dctemp = (v16u8)__msa_fill_b(dc >> 4);
  out = __msa_copy_s_d((v2i64)dctemp, 0);
  STORE8x8(out, dst);
}

static void TM8uv(uint8_t* dst) {
  int j;
  const v16i8 T1 = LD_SB(dst - BPS);
  const v16i8 zero = { 0 };
  const v8i16 T  = (v8i16)__msa_ilvr_b(zero, T1);
  const v8i16 TL = (v8i16)__msa_fill_h(dst[-1 - BPS]);
  const v8i16 d = T - TL;

  for (j = 0; j < 8; j += 4) {
    v16i8 t0, t1;
    v8i16 r0 = (v8i16)__msa_fill_h(dst[-1 + 0 * BPS]);
    v8i16 r1 = (v8i16)__msa_fill_h(dst[-1 + 1 * BPS]);
    v8i16 r2 = (v8i16)__msa_fill_h(dst[-1 + 2 * BPS]);
    v8i16 r3 = (v8i16)__msa_fill_h(dst[-1 + 3 * BPS]);
    ADD4(d, r0, d, r1, d, r2, d, r3, r0, r1, r2, r3);
    CLIP_SH4_0_255(r0, r1, r2, r3);
    PCKEV_B2_SB(r1, r0, r3, r2, t0, t1);
    ST4x4_UB(t0, t1, 0, 2, 0, 2, dst, BPS);
    ST4x4_UB(t0, t1, 1, 3, 1, 3, dst + 4, BPS);
    dst += 4 * BPS;
  }
}

static void VE8uv(uint8_t* dst) {   // vertical
  const v16u8 rtop = LD_UB(dst - BPS);
  const uint64_t out = __msa_copy_s_d((v2i64)rtop, 0);
  STORE8x8(out, dst);
}

static void HE8uv(uint8_t* dst) {   // horizontal
  int j;
  for (j = 0; j < 8; j += 4) {
    const v16u8 L0 = (v16u8)__msa_fill_b(dst[-1 + 0 * BPS]);
    const v16u8 L1 = (v16u8)__msa_fill_b(dst[-1 + 1 * BPS]);
    const v16u8 L2 = (v16u8)__msa_fill_b(dst[-1 + 2 * BPS]);
    const v16u8 L3 = (v16u8)__msa_fill_b(dst[-1 + 3 * BPS]);
    const uint64_t out0 = __msa_copy_s_d((v2i64)L0, 0);
    const uint64_t out1 = __msa_copy_s_d((v2i64)L1, 0);
    const uint64_t out2 = __msa_copy_s_d((v2i64)L2, 0);
    const uint64_t out3 = __msa_copy_s_d((v2i64)L3, 0);
    SD4(out0, out1, out2, out3, dst, BPS);
    dst += 4 * BPS;
  }
}

static void DC8uvNoLeft(uint8_t* dst) {   // DC with no left samples
  const uint32_t dc = 4;
  const v16u8 rtop = LD_UB(dst - BPS);
  const v8u16 temp0 = __msa_hadd_u_h(rtop, rtop);
  const v4u32 temp1 = __msa_hadd_u_w(temp0, temp0);
  const v2u64 temp2 = __msa_hadd_u_d(temp1, temp1);
  const uint32_t sum_m = __msa_copy_s_w((v4i32)temp2, 0);
  const v16u8 dcval = (v16u8)__msa_fill_b((dc + sum_m) >> 3);
  const uint64_t out = __msa_copy_s_d((v2i64)dcval, 0);
  STORE8x8(out, dst);
}

static void DC8uvNoTop(uint8_t* dst) {   // DC with no top samples
  uint32_t dc = 4;
  int i;
  uint64_t out;
  v16u8 dctemp;

  for (i = 0; i < 8; ++i) {
    dc += dst[-1 + i * BPS];
  }
  dctemp = (v16u8)__msa_fill_b(dc >> 3);
  out = __msa_copy_s_d((v2i64)dctemp, 0);
  STORE8x8(out, dst);
}

static void DC8uvNoTopLeft(uint8_t* dst) {   // DC with nothing
  const uint64_t out = 0x8080808080808080ULL;
  STORE8x8(out, dst);
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitMSA(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitMSA(void) {
  VP8TransformWHT = TransformWHT;
  VP8Transform = TransformTwo;
  VP8TransformDC = TransformDC;
  VP8TransformAC3 = TransformAC3;

  VP8VFilter16  = VFilter16;
  VP8HFilter16  = HFilter16;
  VP8VFilter16i = VFilter16i;
  VP8HFilter16i = HFilter16i;
  VP8VFilter8  = VFilter8;
  VP8HFilter8  = HFilter8;
  VP8VFilter8i = VFilter8i;
  VP8HFilter8i = HFilter8i;
  VP8SimpleVFilter16  = SimpleVFilter16;
  VP8SimpleHFilter16  = SimpleHFilter16;
  VP8SimpleVFilter16i = SimpleVFilter16i;
  VP8SimpleHFilter16i = SimpleHFilter16i;

  VP8PredLuma4[0] = DC4;
  VP8PredLuma4[1] = TM4;
  VP8PredLuma4[2] = VE4;
  VP8PredLuma4[4] = RD4;
  VP8PredLuma4[6] = LD4;
  VP8PredLuma16[0] = DC16;
  VP8PredLuma16[1] = TM16;
  VP8PredLuma16[2] = VE16;
  VP8PredLuma16[3] = HE16;
  VP8PredLuma16[4] = DC16NoTop;
  VP8PredLuma16[5] = DC16NoLeft;
  VP8PredLuma16[6] = DC16NoTopLeft;
  VP8PredChroma8[0] = DC8uv;
  VP8PredChroma8[1] = TM8uv;
  VP8PredChroma8[2] = VE8uv;
  VP8PredChroma8[3] = HE8uv;
  VP8PredChroma8[4] = DC8uvNoTop;
  VP8PredChroma8[5] = DC8uvNoLeft;
  VP8PredChroma8[6] = DC8uvNoTopLeft;
}

#else  // !WEBP_USE_MSA

WEBP_DSP_INIT_STUB(VP8DspInitMSA)

#endif  // WEBP_USE_MSA
