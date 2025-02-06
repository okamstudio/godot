// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA version of encoder dsp functions.
//
// Author:  Prashant Patil   (prashant.patil@imgtec.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MSA)

#include <stdlib.h>
#include "src/dsp/msa_macro.h"
#include "src/enc/vp8i_enc.h"

//------------------------------------------------------------------------------
// Transforms

#define IDCT_1D_W(in0, in1, in2, in3, out0, out1, out2, out3) do {  \
  v4i32 a1_m, b1_m, c1_m, d1_m;                                     \
  const v4i32 cospi8sqrt2minus1 = __msa_fill_w(20091);              \
  const v4i32 sinpi8sqrt2 = __msa_fill_w(35468);                    \
  v4i32 c_tmp1_m = in1 * sinpi8sqrt2;                               \
  v4i32 c_tmp2_m = in3 * cospi8sqrt2minus1;                         \
  v4i32 d_tmp1_m = in1 * cospi8sqrt2minus1;                         \
  v4i32 d_tmp2_m = in3 * sinpi8sqrt2;                               \
                                                                    \
  ADDSUB2(in0, in2, a1_m, b1_m);                                    \
  SRAI_W2_SW(c_tmp1_m, c_tmp2_m, 16);                               \
  c_tmp2_m = c_tmp2_m + in3;                                        \
  c1_m = c_tmp1_m - c_tmp2_m;                                       \
  SRAI_W2_SW(d_tmp1_m, d_tmp2_m, 16);                               \
  d_tmp1_m = d_tmp1_m + in1;                                        \
  d1_m = d_tmp1_m + d_tmp2_m;                                       \
  BUTTERFLY_4(a1_m, b1_m, c1_m, d1_m, out0, out1, out2, out3);      \
} while (0)

static WEBP_INLINE void ITransformOne(const uint8_t* WEBP_RESTRICT ref,
                                      const int16_t* WEBP_RESTRICT in,
                                      uint8_t* WEBP_RESTRICT dst) {
  v8i16 input0, input1;
  v4i32 in0, in1, in2, in3, hz0, hz1, hz2, hz3, vt0, vt1, vt2, vt3;
  v4i32 res0, res1, res2, res3;
  v16i8 dest0, dest1, dest2, dest3;
  const v16i8 zero = { 0 };

  LD_SH2(in, 8, input0, input1);
  UNPCK_SH_SW(input0, in0, in1);
  UNPCK_SH_SW(input1, in2, in3);
  IDCT_1D_W(in0, in1, in2, in3, hz0, hz1, hz2, hz3);
  TRANSPOSE4x4_SW_SW(hz0, hz1, hz2, hz3, hz0, hz1, hz2, hz3);
  IDCT_1D_W(hz0, hz1, hz2, hz3, vt0, vt1, vt2, vt3);
  SRARI_W4_SW(vt0, vt1, vt2, vt3, 3);
  TRANSPOSE4x4_SW_SW(vt0, vt1, vt2, vt3, vt0, vt1, vt2, vt3);
  LD_SB4(ref, BPS, dest0, dest1, dest2, dest3);
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

static void ITransform_MSA(const uint8_t* WEBP_RESTRICT ref,
                           const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst, int do_two) {
  ITransformOne(ref, in, dst);
  if (do_two) {
    ITransformOne(ref + 4, in + 16, dst + 4);
  }
}

static void FTransform_MSA(const uint8_t* WEBP_RESTRICT src,
                           const uint8_t* WEBP_RESTRICT ref,
                           int16_t* WEBP_RESTRICT out) {
  uint64_t out0, out1, out2, out3;
  uint32_t in0, in1, in2, in3;
  v4i32 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
  v8i16 t0, t1, t2, t3;
  v16u8 srcl0, srcl1, src0 = { 0 }, src1 = { 0 };
  const v8i16 mask0 = { 0, 4, 8, 12, 1, 5, 9, 13 };
  const v8i16 mask1 = { 3, 7, 11, 15, 2, 6, 10, 14 };
  const v8i16 mask2 = { 4, 0, 5, 1, 6, 2, 7, 3 };
  const v8i16 mask3 = { 0, 4, 1, 5, 2, 6, 3, 7 };
  const v8i16 cnst0 = { 2217, -5352, 2217, -5352, 2217, -5352, 2217, -5352 };
  const v8i16 cnst1 = { 5352, 2217, 5352, 2217, 5352, 2217, 5352, 2217 };

  LW4(src, BPS, in0, in1, in2, in3);
  INSERT_W4_UB(in0, in1, in2, in3, src0);
  LW4(ref, BPS, in0, in1, in2, in3);
  INSERT_W4_UB(in0, in1, in2, in3, src1);
  ILVRL_B2_UB(src0, src1, srcl0, srcl1);
  HSUB_UB2_SH(srcl0, srcl1, t0, t1);
  VSHF_H2_SH(t0, t1, t0, t1, mask0, mask1, t2, t3);
  ADDSUB2(t2, t3, t0, t1);
  t0 = SRLI_H(t0, 3);
  VSHF_H2_SH(t0, t0, t1, t1, mask2, mask3, t3, t2);
  tmp0 = __msa_hadd_s_w(t3, t3);
  tmp2 = __msa_hsub_s_w(t3, t3);
  FILL_W2_SW(1812, 937, tmp1, tmp3);
  DPADD_SH2_SW(t2, t2, cnst0, cnst1, tmp3, tmp1);
  SRAI_W2_SW(tmp1, tmp3, 9);
  PCKEV_H2_SH(tmp1, tmp0, tmp3, tmp2, t0, t1);
  VSHF_H2_SH(t0, t1, t0, t1, mask0, mask1, t2, t3);
  ADDSUB2(t2, t3, t0, t1);
  VSHF_H2_SH(t0, t0, t1, t1, mask2, mask3, t3, t2);
  tmp0 = __msa_hadd_s_w(t3, t3);
  tmp2 = __msa_hsub_s_w(t3, t3);
  ADDVI_W2_SW(tmp0, 7, tmp2, 7, tmp0, tmp2);
  SRAI_W2_SW(tmp0, tmp2, 4);
  FILL_W2_SW(12000, 51000, tmp1, tmp3);
  DPADD_SH2_SW(t2, t2, cnst0, cnst1, tmp3, tmp1);
  SRAI_W2_SW(tmp1, tmp3, 16);
  UNPCK_R_SH_SW(t1, tmp4);
  tmp5 = __msa_ceqi_w(tmp4, 0);
  tmp4 = (v4i32)__msa_nor_v((v16u8)tmp5, (v16u8)tmp5);
  tmp5 = __msa_fill_w(1);
  tmp5 = (v4i32)__msa_and_v((v16u8)tmp5, (v16u8)tmp4);
  tmp1 += tmp5;
  PCKEV_H2_SH(tmp1, tmp0, tmp3, tmp2, t0, t1);
  out0 = __msa_copy_s_d((v2i64)t0, 0);
  out1 = __msa_copy_s_d((v2i64)t0, 1);
  out2 = __msa_copy_s_d((v2i64)t1, 0);
  out3 = __msa_copy_s_d((v2i64)t1, 1);
  SD4(out0, out1, out2, out3, out, 8);
}

static void FTransformWHT_MSA(const int16_t* WEBP_RESTRICT in,
                              int16_t* WEBP_RESTRICT out) {
  v8i16 in0 = { 0 };
  v8i16 in1 = { 0 };
  v8i16 tmp0, tmp1, tmp2, tmp3;
  v8i16 out0, out1;
  const v8i16 mask0 = { 0, 1, 2, 3, 8, 9, 10, 11 };
  const v8i16 mask1 = { 4, 5, 6, 7, 12, 13, 14, 15 };
  const v8i16 mask2 = { 0, 4, 8, 12, 1, 5, 9, 13 };
  const v8i16 mask3 = { 3, 7, 11, 15, 2, 6, 10, 14 };

  in0 = __msa_insert_h(in0, 0, in[  0]);
  in0 = __msa_insert_h(in0, 1, in[ 64]);
  in0 = __msa_insert_h(in0, 2, in[128]);
  in0 = __msa_insert_h(in0, 3, in[192]);
  in0 = __msa_insert_h(in0, 4, in[ 16]);
  in0 = __msa_insert_h(in0, 5, in[ 80]);
  in0 = __msa_insert_h(in0, 6, in[144]);
  in0 = __msa_insert_h(in0, 7, in[208]);
  in1 = __msa_insert_h(in1, 0, in[ 48]);
  in1 = __msa_insert_h(in1, 1, in[112]);
  in1 = __msa_insert_h(in1, 2, in[176]);
  in1 = __msa_insert_h(in1, 3, in[240]);
  in1 = __msa_insert_h(in1, 4, in[ 32]);
  in1 = __msa_insert_h(in1, 5, in[ 96]);
  in1 = __msa_insert_h(in1, 6, in[160]);
  in1 = __msa_insert_h(in1, 7, in[224]);
  ADDSUB2(in0, in1, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  ADDSUB2(tmp2, tmp3, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask2, mask3, in0, in1);
  ADDSUB2(in0, in1, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  ADDSUB2(tmp2, tmp3, out0, out1);
  SRAI_H2_SH(out0, out1, 1);
  ST_SH2(out0, out1, out, 8);
}

static int TTransform_MSA(const uint8_t* WEBP_RESTRICT in,
                          const uint16_t* WEBP_RESTRICT w) {
  int sum;
  uint32_t in0_m, in1_m, in2_m, in3_m;
  v16i8 src0 = { 0 };
  v8i16 in0, in1, tmp0, tmp1, tmp2, tmp3;
  v4i32 dst0, dst1;
  const v16i8 zero = { 0 };
  const v8i16 mask0 = { 0, 1, 2, 3, 8, 9, 10, 11 };
  const v8i16 mask1 = { 4, 5, 6, 7, 12, 13, 14, 15 };
  const v8i16 mask2 = { 0, 4, 8, 12, 1, 5, 9, 13 };
  const v8i16 mask3 = { 3, 7, 11, 15, 2, 6, 10, 14 };

  LW4(in, BPS, in0_m, in1_m, in2_m, in3_m);
  INSERT_W4_SB(in0_m, in1_m, in2_m, in3_m, src0);
  ILVRL_B2_SH(zero, src0, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask2, mask3, in0, in1);
  ADDSUB2(in0, in1, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  ADDSUB2(tmp2, tmp3, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask2, mask3, in0, in1);
  ADDSUB2(in0, in1, tmp0, tmp1);
  VSHF_H2_SH(tmp0, tmp1, tmp0, tmp1, mask0, mask1, tmp2, tmp3);
  ADDSUB2(tmp2, tmp3, tmp0, tmp1);
  tmp0 = __msa_add_a_h(tmp0, (v8i16)zero);
  tmp1 = __msa_add_a_h(tmp1, (v8i16)zero);
  LD_SH2(w, 8, tmp2, tmp3);
  DOTP_SH2_SW(tmp0, tmp1, tmp2, tmp3, dst0, dst1);
  dst0 = dst0 + dst1;
  sum = HADD_SW_S32(dst0);
  return sum;
}

static int Disto4x4_MSA(const uint8_t* WEBP_RESTRICT const a,
                        const uint8_t* WEBP_RESTRICT const b,
                        const uint16_t* WEBP_RESTRICT const w) {
  const int sum1 = TTransform_MSA(a, w);
  const int sum2 = TTransform_MSA(b, w);
  return abs(sum2 - sum1) >> 5;
}

static int Disto16x16_MSA(const uint8_t* WEBP_RESTRICT const a,
                          const uint8_t* WEBP_RESTRICT const b,
                          const uint16_t* WEBP_RESTRICT const w) {
  int D = 0;
  int x, y;
  for (y = 0; y < 16 * BPS; y += 4 * BPS) {
    for (x = 0; x < 16; x += 4) {
      D += Disto4x4_MSA(a + x + y, b + x + y, w);
    }
  }
  return D;
}

//------------------------------------------------------------------------------
// Histogram

static void CollectHistogram_MSA(const uint8_t* ref, const uint8_t* pred,
                                 int start_block, int end_block,
                                 VP8Histogram* const histo) {
  int j;
  int distribution[MAX_COEFF_THRESH + 1] = { 0 };
  for (j = start_block; j < end_block; ++j) {
    int16_t out[16];
    VP8FTransform(ref + VP8DspScan[j], pred + VP8DspScan[j], out);
    {
      int k;
      v8i16 coeff0, coeff1;
      const v8i16 zero = { 0 };
      const v8i16 max_coeff_thr = __msa_ldi_h(MAX_COEFF_THRESH);
      LD_SH2(&out[0], 8, coeff0, coeff1);
      coeff0 = __msa_add_a_h(coeff0, zero);
      coeff1 = __msa_add_a_h(coeff1, zero);
      SRAI_H2_SH(coeff0, coeff1, 3);
      coeff0 = __msa_min_s_h(coeff0, max_coeff_thr);
      coeff1 = __msa_min_s_h(coeff1, max_coeff_thr);
      ST_SH2(coeff0, coeff1, &out[0], 8);
      for (k = 0; k < 16; ++k) {
        ++distribution[out[k]];
      }
    }
  }
  VP8SetHistogramData(distribution, histo);
}

//------------------------------------------------------------------------------
// Intra predictions

// luma 4x4 prediction

#define DST(x, y) dst[(x) + (y) * BPS]
#define AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

// vertical
static WEBP_INLINE void VE4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
  const v16u8 A1 = { 0 };
  const uint64_t val_m = LD(top - 1);
  const v16u8 A = (v16u8)__msa_insert_d((v2i64)A1, 0, val_m);
  const v16u8 B = SLDI_UB(A, A, 1);
  const v16u8 C = SLDI_UB(A, A, 2);
  const v16u8 AC = __msa_ave_u_b(A, C);
  const v16u8 B2 = __msa_ave_u_b(B, B);
  const v16u8 R = __msa_aver_u_b(AC, B2);
  const uint32_t out = __msa_copy_s_w((v4i32)R, 0);
  SW4(out, out, out, out, dst, BPS);
}

// horizontal
static WEBP_INLINE void HE4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
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

static WEBP_INLINE void DC4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i) dc += top[i] + top[-5 + i];
  dc >>= 3;
  dc = dc | (dc << 8) | (dc << 16) | (dc << 24);
  SW4(dc, dc, dc, dc, dst, BPS);
}

static WEBP_INLINE void RD4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
  const v16u8 A2 = { 0 };
  const uint64_t val_m = LD(top - 5);
  const v16u8 A1 = (v16u8)__msa_insert_d((v2i64)A2, 0, val_m);
  const v16u8 A = (v16u8)__msa_insert_b((v16i8)A1, 8, top[3]);
  const v16u8 B = SLDI_UB(A, A, 1);
  const v16u8 C = SLDI_UB(A, A, 2);
  const v16u8 AC = __msa_ave_u_b(A, C);
  const v16u8 B2 = __msa_ave_u_b(B, B);
  const v16u8 R0 = __msa_aver_u_b(AC, B2);
  const v16u8 R1 = SLDI_UB(R0, R0, 1);
  const v16u8 R2 = SLDI_UB(R1, R1, 1);
  const v16u8 R3 = SLDI_UB(R2, R2, 1);
  const uint32_t val0 = __msa_copy_s_w((v4i32)R0, 0);
  const uint32_t val1 = __msa_copy_s_w((v4i32)R1, 0);
  const uint32_t val2 = __msa_copy_s_w((v4i32)R2, 0);
  const uint32_t val3 = __msa_copy_s_w((v4i32)R3, 0);
  SW4(val3, val2, val1, val0, dst, BPS);
}

static WEBP_INLINE void LD4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
  const v16u8 A1 = { 0 };
  const uint64_t val_m = LD(top);
  const v16u8 A = (v16u8)__msa_insert_d((v2i64)A1, 0, val_m);
  const v16u8 B = SLDI_UB(A, A, 1);
  const v16u8 C1 = SLDI_UB(A, A, 2);
  const v16u8 C = (v16u8)__msa_insert_b((v16i8)C1, 6, top[7]);
  const v16u8 AC = __msa_ave_u_b(A, C);
  const v16u8 B2 = __msa_ave_u_b(B, B);
  const v16u8 R0 = __msa_aver_u_b(AC, B2);
  const v16u8 R1 = SLDI_UB(R0, R0, 1);
  const v16u8 R2 = SLDI_UB(R1, R1, 1);
  const v16u8 R3 = SLDI_UB(R2, R2, 1);
  const uint32_t val0 = __msa_copy_s_w((v4i32)R0, 0);
  const uint32_t val1 = __msa_copy_s_w((v4i32)R1, 0);
  const uint32_t val2 = __msa_copy_s_w((v4i32)R2, 0);
  const uint32_t val3 = __msa_copy_s_w((v4i32)R3, 0);
  SW4(val0, val1, val2, val3, dst, BPS);
}

static WEBP_INLINE void VR4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
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

static WEBP_INLINE void VL4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
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

static WEBP_INLINE void HU4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
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

static WEBP_INLINE void HD4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
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

static WEBP_INLINE void TM4(uint8_t* WEBP_RESTRICT dst,
                            const uint8_t* WEBP_RESTRICT top) {
  const v16i8 zero = { 0 };
  const v8i16 TL = (v8i16)__msa_fill_h(top[-1]);
  const v8i16 L0 = (v8i16)__msa_fill_h(top[-2]);
  const v8i16 L1 = (v8i16)__msa_fill_h(top[-3]);
  const v8i16 L2 = (v8i16)__msa_fill_h(top[-4]);
  const v8i16 L3 = (v8i16)__msa_fill_h(top[-5]);
  const v16u8 T1 = LD_UB(top);
  const v8i16 T  = (v8i16)__msa_ilvr_b(zero, (v16i8)T1);
  const v8i16 d = T - TL;
  v8i16 r0, r1, r2, r3;
  ADD4(d, L0, d, L1, d, L2, d, L3, r0, r1, r2, r3);
  CLIP_SH4_0_255(r0, r1, r2, r3);
  PCKEV_ST4x4_UB(r0, r1, r2, r3, dst, BPS);
}

#undef DST
#undef AVG3
#undef AVG2

static void Intra4Preds_MSA(uint8_t* WEBP_RESTRICT dst,
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

// luma 16x16 prediction

#define STORE16x16(out, dst) do {                                        \
    ST_UB8(out, out, out, out, out, out, out, out, dst + 0 * BPS, BPS);  \
    ST_UB8(out, out, out, out, out, out, out, out, dst + 8 * BPS, BPS);  \
} while (0)

static WEBP_INLINE void VerticalPred16x16(uint8_t* WEBP_RESTRICT dst,
                                          const uint8_t* WEBP_RESTRICT top) {
  if (top != NULL) {
    const v16u8 out = LD_UB(top);
    STORE16x16(out, dst);
  } else {
    const v16u8 out = (v16u8)__msa_fill_b(0x7f);
    STORE16x16(out, dst);
  }
}

static WEBP_INLINE void HorizontalPred16x16(uint8_t* WEBP_RESTRICT dst,
                                            const uint8_t* WEBP_RESTRICT left) {
  if (left != NULL) {
    int j;
    for (j = 0; j < 16; j += 4) {
      const v16u8 L0 = (v16u8)__msa_fill_b(left[0]);
      const v16u8 L1 = (v16u8)__msa_fill_b(left[1]);
      const v16u8 L2 = (v16u8)__msa_fill_b(left[2]);
      const v16u8 L3 = (v16u8)__msa_fill_b(left[3]);
      ST_UB4(L0, L1, L2, L3, dst, BPS);
      dst += 4 * BPS;
      left += 4;
    }
  } else {
    const v16u8 out = (v16u8)__msa_fill_b(0x81);
    STORE16x16(out, dst);
  }
}

static WEBP_INLINE void TrueMotion16x16(uint8_t* WEBP_RESTRICT dst,
                                        const uint8_t* WEBP_RESTRICT left,
                                        const uint8_t* WEBP_RESTRICT top) {
  if (left != NULL) {
    if (top != NULL) {
      int j;
      v8i16 d1, d2;
      const v16i8 zero = { 0 };
      const v8i16 TL = (v8i16)__msa_fill_h(left[-1]);
      const v16u8 T = LD_UB(top);
      ILVRL_B2_SH(zero, T, d1, d2);
      SUB2(d1, TL, d2, TL, d1, d2);
      for (j = 0; j < 16; j += 4) {
        v16i8 t0, t1, t2, t3;
        v8i16 r0, r1, r2, r3, r4, r5, r6, r7;
        const v8i16 L0 = (v8i16)__msa_fill_h(left[j + 0]);
        const v8i16 L1 = (v8i16)__msa_fill_h(left[j + 1]);
        const v8i16 L2 = (v8i16)__msa_fill_h(left[j + 2]);
        const v8i16 L3 = (v8i16)__msa_fill_h(left[j + 3]);
        ADD4(d1, L0, d1, L1, d1, L2, d1, L3, r0, r1, r2, r3);
        ADD4(d2, L0, d2, L1, d2, L2, d2, L3, r4, r5, r6, r7);
        CLIP_SH4_0_255(r0, r1, r2, r3);
        CLIP_SH4_0_255(r4, r5, r6, r7);
        PCKEV_B4_SB(r4, r0, r5, r1, r6, r2, r7, r3, t0, t1, t2, t3);
        ST_SB4(t0, t1, t2, t3, dst, BPS);
        dst += 4 * BPS;
      }
    } else {
      HorizontalPred16x16(dst, left);
    }
  } else {
    if (top != NULL) {
      VerticalPred16x16(dst, top);
    } else {
      const v16u8 out = (v16u8)__msa_fill_b(0x81);
      STORE16x16(out, dst);
    }
  }
}

static WEBP_INLINE void DCMode16x16(uint8_t* WEBP_RESTRICT dst,
                                    const uint8_t* WEBP_RESTRICT left,
                                    const uint8_t* WEBP_RESTRICT top) {
  int DC;
  v16u8 out;
  if (top != NULL && left != NULL) {
    const v16u8 rtop = LD_UB(top);
    const v8u16 dctop = __msa_hadd_u_h(rtop, rtop);
    const v16u8 rleft = LD_UB(left);
    const v8u16 dcleft = __msa_hadd_u_h(rleft, rleft);
    const v8u16 dctemp = dctop + dcleft;
    DC = HADD_UH_U32(dctemp);
    DC = (DC + 16) >> 5;
  } else if (left != NULL) {   // left but no top
    const v16u8 rleft = LD_UB(left);
    const v8u16 dcleft = __msa_hadd_u_h(rleft, rleft);
    DC = HADD_UH_U32(dcleft);
    DC = (DC + DC + 16) >> 5;
  } else if (top != NULL) {   // top but no left
    const v16u8 rtop = LD_UB(top);
    const v8u16 dctop = __msa_hadd_u_h(rtop, rtop);
    DC = HADD_UH_U32(dctop);
    DC = (DC + DC + 16) >> 5;
  } else {   // no top, no left, nothing.
    DC = 0x80;
  }
  out = (v16u8)__msa_fill_b(DC);
  STORE16x16(out, dst);
}

static void Intra16Preds_MSA(uint8_t* WEBP_RESTRICT dst,
                             const uint8_t* WEBP_RESTRICT left,
                             const uint8_t* WEBP_RESTRICT top) {
  DCMode16x16(I16DC16 + dst, left, top);
  VerticalPred16x16(I16VE16 + dst, top);
  HorizontalPred16x16(I16HE16 + dst, left);
  TrueMotion16x16(I16TM16 + dst, left, top);
}

// Chroma 8x8 prediction

#define CALC_DC8(in, out) do {                              \
  const v8u16 temp0 = __msa_hadd_u_h(in, in);               \
  const v4u32 temp1 = __msa_hadd_u_w(temp0, temp0);         \
  const v2i64 temp2 = (v2i64)__msa_hadd_u_d(temp1, temp1);  \
  const v2i64 temp3 = __msa_splati_d(temp2, 1);             \
  const v2i64 temp4 = temp3 + temp2;                        \
  const v16i8 temp5 = (v16i8)__msa_srari_d(temp4, 4);       \
  const v2i64 temp6 = (v2i64)__msa_splati_b(temp5, 0);      \
  out = __msa_copy_s_d(temp6, 0);                           \
} while (0)

#define STORE8x8(out, dst) do {                 \
  SD4(out, out, out, out, dst + 0 * BPS, BPS);  \
  SD4(out, out, out, out, dst + 4 * BPS, BPS);  \
} while (0)

static WEBP_INLINE void VerticalPred8x8(uint8_t* WEBP_RESTRICT dst,
                                        const uint8_t* WEBP_RESTRICT top) {
  if (top != NULL) {
    const uint64_t out = LD(top);
    STORE8x8(out, dst);
  } else {
    const uint64_t out = 0x7f7f7f7f7f7f7f7fULL;
    STORE8x8(out, dst);
  }
}

static WEBP_INLINE void HorizontalPred8x8(uint8_t* WEBP_RESTRICT dst,
                                          const uint8_t* WEBP_RESTRICT left) {
  if (left != NULL) {
    int j;
    for (j = 0; j < 8; j += 4) {
      const v16u8 L0 = (v16u8)__msa_fill_b(left[0]);
      const v16u8 L1 = (v16u8)__msa_fill_b(left[1]);
      const v16u8 L2 = (v16u8)__msa_fill_b(left[2]);
      const v16u8 L3 = (v16u8)__msa_fill_b(left[3]);
      const uint64_t out0 = __msa_copy_s_d((v2i64)L0, 0);
      const uint64_t out1 = __msa_copy_s_d((v2i64)L1, 0);
      const uint64_t out2 = __msa_copy_s_d((v2i64)L2, 0);
      const uint64_t out3 = __msa_copy_s_d((v2i64)L3, 0);
      SD4(out0, out1, out2, out3, dst, BPS);
      dst += 4 * BPS;
      left += 4;
    }
  } else {
    const uint64_t out = 0x8181818181818181ULL;
    STORE8x8(out, dst);
  }
}

static WEBP_INLINE void TrueMotion8x8(uint8_t* WEBP_RESTRICT dst,
                                      const uint8_t* WEBP_RESTRICT left,
                                      const uint8_t* WEBP_RESTRICT top) {
  if (left != NULL) {
    if (top != NULL) {
      int j;
      const v8i16 TL = (v8i16)__msa_fill_h(left[-1]);
      const v16u8 T1 = LD_UB(top);
      const v16i8 zero = { 0 };
      const v8i16 T  = (v8i16)__msa_ilvr_b(zero, (v16i8)T1);
      const v8i16 d = T - TL;
      for (j = 0; j < 8; j += 4) {
        uint64_t out0, out1, out2, out3;
        v16i8 t0, t1;
        v8i16 r0 = (v8i16)__msa_fill_h(left[j + 0]);
        v8i16 r1 = (v8i16)__msa_fill_h(left[j + 1]);
        v8i16 r2 = (v8i16)__msa_fill_h(left[j + 2]);
        v8i16 r3 = (v8i16)__msa_fill_h(left[j + 3]);
        ADD4(d, r0, d, r1, d, r2, d, r3, r0, r1, r2, r3);
        CLIP_SH4_0_255(r0, r1, r2, r3);
        PCKEV_B2_SB(r1, r0, r3, r2, t0, t1);
        out0 = __msa_copy_s_d((v2i64)t0, 0);
        out1 = __msa_copy_s_d((v2i64)t0, 1);
        out2 = __msa_copy_s_d((v2i64)t1, 0);
        out3 = __msa_copy_s_d((v2i64)t1, 1);
        SD4(out0, out1, out2, out3, dst, BPS);
        dst += 4 * BPS;
      }
    } else {
      HorizontalPred8x8(dst, left);
    }
  } else {
    if (top != NULL) {
      VerticalPred8x8(dst, top);
    } else {
      const uint64_t out = 0x8181818181818181ULL;
      STORE8x8(out, dst);
    }
  }
}

static WEBP_INLINE void DCMode8x8(uint8_t* WEBP_RESTRICT dst,
                                  const uint8_t* WEBP_RESTRICT left,
                                  const uint8_t* WEBP_RESTRICT top) {
  uint64_t out;
  v16u8 src = { 0 };
  if (top != NULL && left != NULL) {
    const uint64_t left_m = LD(left);
    const uint64_t top_m = LD(top);
    INSERT_D2_UB(left_m, top_m, src);
    CALC_DC8(src, out);
  } else if (left != NULL) {   // left but no top
    const uint64_t left_m = LD(left);
    INSERT_D2_UB(left_m, left_m, src);
    CALC_DC8(src, out);
  } else if (top != NULL) {   // top but no left
    const uint64_t top_m = LD(top);
    INSERT_D2_UB(top_m, top_m, src);
    CALC_DC8(src, out);
  } else {   // no top, no left, nothing.
    src = (v16u8)__msa_fill_b(0x80);
    out = __msa_copy_s_d((v2i64)src, 0);
  }
  STORE8x8(out, dst);
}

static void IntraChromaPreds_MSA(uint8_t* WEBP_RESTRICT dst,
                                 const uint8_t* WEBP_RESTRICT left,
                                 const uint8_t* WEBP_RESTRICT top) {
  // U block
  DCMode8x8(C8DC8 + dst, left, top);
  VerticalPred8x8(C8VE8 + dst, top);
  HorizontalPred8x8(C8HE8 + dst, left);
  TrueMotion8x8(C8TM8 + dst, left, top);
  // V block
  dst += 8;
  if (top != NULL) top += 8;
  if (left != NULL) left += 16;
  DCMode8x8(C8DC8 + dst, left, top);
  VerticalPred8x8(C8VE8 + dst, top);
  HorizontalPred8x8(C8HE8 + dst, left);
  TrueMotion8x8(C8TM8 + dst, left, top);
}

//------------------------------------------------------------------------------
// Metric

#define PACK_DOTP_UB4_SW(in0, in1, in2, in3, out0, out1, out2, out3) do {  \
  v16u8 tmp0, tmp1;                                                        \
  v8i16 tmp2, tmp3;                                                        \
  ILVRL_B2_UB(in0, in1, tmp0, tmp1);                                       \
  HSUB_UB2_SH(tmp0, tmp1, tmp2, tmp3);                                     \
  DOTP_SH2_SW(tmp2, tmp3, tmp2, tmp3, out0, out1);                         \
  ILVRL_B2_UB(in2, in3, tmp0, tmp1);                                       \
  HSUB_UB2_SH(tmp0, tmp1, tmp2, tmp3);                                     \
  DOTP_SH2_SW(tmp2, tmp3, tmp2, tmp3, out2, out3);                         \
} while (0)

#define PACK_DPADD_UB4_SW(in0, in1, in2, in3, out0, out1, out2, out3) do {  \
  v16u8 tmp0, tmp1;                                                         \
  v8i16 tmp2, tmp3;                                                         \
  ILVRL_B2_UB(in0, in1, tmp0, tmp1);                                        \
  HSUB_UB2_SH(tmp0, tmp1, tmp2, tmp3);                                      \
  DPADD_SH2_SW(tmp2, tmp3, tmp2, tmp3, out0, out1);                         \
  ILVRL_B2_UB(in2, in3, tmp0, tmp1);                                        \
  HSUB_UB2_SH(tmp0, tmp1, tmp2, tmp3);                                      \
  DPADD_SH2_SW(tmp2, tmp3, tmp2, tmp3, out2, out3);                         \
} while (0)

static int SSE16x16_MSA(const uint8_t* WEBP_RESTRICT a,
                        const uint8_t* WEBP_RESTRICT b) {
  uint32_t sum;
  v16u8 src0, src1, src2, src3, src4, src5, src6, src7;
  v16u8 ref0, ref1, ref2, ref3, ref4, ref5, ref6, ref7;
  v4i32 out0, out1, out2, out3;

  LD_UB8(a, BPS, src0, src1, src2, src3, src4, src5, src6, src7);
  LD_UB8(b, BPS, ref0, ref1, ref2, ref3, ref4, ref5, ref6, ref7);
  PACK_DOTP_UB4_SW(src0, ref0, src1, ref1, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src2, ref2, src3, ref3, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src4, ref4, src5, ref5, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src6, ref6, src7, ref7, out0, out1, out2, out3);
  a += 8 * BPS;
  b += 8 * BPS;
  LD_UB8(a, BPS, src0, src1, src2, src3, src4, src5, src6, src7);
  LD_UB8(b, BPS, ref0, ref1, ref2, ref3, ref4, ref5, ref6, ref7);
  PACK_DPADD_UB4_SW(src0, ref0, src1, ref1, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src2, ref2, src3, ref3, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src4, ref4, src5, ref5, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src6, ref6, src7, ref7, out0, out1, out2, out3);
  out0 += out1;
  out2 += out3;
  out0 += out2;
  sum = HADD_SW_S32(out0);
  return sum;
}

static int SSE16x8_MSA(const uint8_t* WEBP_RESTRICT a,
                       const uint8_t* WEBP_RESTRICT b) {
  uint32_t sum;
  v16u8 src0, src1, src2, src3, src4, src5, src6, src7;
  v16u8 ref0, ref1, ref2, ref3, ref4, ref5, ref6, ref7;
  v4i32 out0, out1, out2, out3;

  LD_UB8(a, BPS, src0, src1, src2, src3, src4, src5, src6, src7);
  LD_UB8(b, BPS, ref0, ref1, ref2, ref3, ref4, ref5, ref6, ref7);
  PACK_DOTP_UB4_SW(src0, ref0, src1, ref1, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src2, ref2, src3, ref3, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src4, ref4, src5, ref5, out0, out1, out2, out3);
  PACK_DPADD_UB4_SW(src6, ref6, src7, ref7, out0, out1, out2, out3);
  out0 += out1;
  out2 += out3;
  out0 += out2;
  sum = HADD_SW_S32(out0);
  return sum;
}

static int SSE8x8_MSA(const uint8_t* WEBP_RESTRICT a,
                      const uint8_t* WEBP_RESTRICT b) {
  uint32_t sum;
  v16u8 src0, src1, src2, src3, src4, src5, src6, src7;
  v16u8 ref0, ref1, ref2, ref3, ref4, ref5, ref6, ref7;
  v16u8 t0, t1, t2, t3;
  v4i32 out0, out1, out2, out3;

  LD_UB8(a, BPS, src0, src1, src2, src3, src4, src5, src6, src7);
  LD_UB8(b, BPS, ref0, ref1, ref2, ref3, ref4, ref5, ref6, ref7);
  ILVR_B4_UB(src0, src1, src2, src3, ref0, ref1, ref2, ref3, t0, t1, t2, t3);
  PACK_DOTP_UB4_SW(t0, t2, t1, t3, out0, out1, out2, out3);
  ILVR_B4_UB(src4, src5, src6, src7, ref4, ref5, ref6, ref7, t0, t1, t2, t3);
  PACK_DPADD_UB4_SW(t0, t2, t1, t3, out0, out1, out2, out3);
  out0 += out1;
  out2 += out3;
  out0 += out2;
  sum = HADD_SW_S32(out0);
  return sum;
}

static int SSE4x4_MSA(const uint8_t* WEBP_RESTRICT a,
                      const uint8_t* WEBP_RESTRICT b) {
  uint32_t sum = 0;
  uint32_t src0, src1, src2, src3, ref0, ref1, ref2, ref3;
  v16u8 src = { 0 }, ref = { 0 }, tmp0, tmp1;
  v8i16 diff0, diff1;
  v4i32 out0, out1;

  LW4(a, BPS, src0, src1, src2, src3);
  LW4(b, BPS, ref0, ref1, ref2, ref3);
  INSERT_W4_UB(src0, src1, src2, src3, src);
  INSERT_W4_UB(ref0, ref1, ref2, ref3, ref);
  ILVRL_B2_UB(src, ref, tmp0, tmp1);
  HSUB_UB2_SH(tmp0, tmp1, diff0, diff1);
  DOTP_SH2_SW(diff0, diff1, diff0, diff1, out0, out1);
  out0 += out1;
  sum = HADD_SW_S32(out0);
  return sum;
}

//------------------------------------------------------------------------------
// Quantization

static int QuantizeBlock_MSA(int16_t in[16], int16_t out[16],
                             const VP8Matrix* WEBP_RESTRICT const mtx) {
  int sum;
  v8i16 in0, in1, sh0, sh1, out0, out1;
  v8i16 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, sign0, sign1;
  v4i32 s0, s1, s2, s3, b0, b1, b2, b3, t0, t1, t2, t3;
  const v8i16 zero = { 0 };
  const v8i16 zigzag0 = { 0, 1, 4, 8, 5, 2, 3, 6 };
  const v8i16 zigzag1 = { 9, 12, 13, 10, 7, 11, 14, 15 };
  const v8i16 maxlevel = __msa_fill_h(MAX_LEVEL);

  LD_SH2(&in[0], 8, in0, in1);
  LD_SH2(&mtx->sharpen_[0], 8, sh0, sh1);
  tmp4 = __msa_add_a_h(in0, zero);
  tmp5 = __msa_add_a_h(in1, zero);
  ILVRL_H2_SH(sh0, tmp4, tmp0, tmp1);
  ILVRL_H2_SH(sh1, tmp5, tmp2, tmp3);
  HADD_SH4_SW(tmp0, tmp1, tmp2, tmp3, s0, s1, s2, s3);
  sign0 = (in0 < zero);
  sign1 = (in1 < zero);                           // sign
  LD_SH2(&mtx->iq_[0], 8, tmp0, tmp1);            // iq
  ILVRL_H2_SW(zero, tmp0, t0, t1);
  ILVRL_H2_SW(zero, tmp1, t2, t3);
  LD_SW4(&mtx->bias_[0], 4, b0, b1, b2, b3);      // bias
  MUL4(t0, s0, t1, s1, t2, s2, t3, s3, t0, t1, t2, t3);
  ADD4(b0, t0, b1, t1, b2, t2, b3, t3, b0, b1, b2, b3);
  SRAI_W4_SW(b0, b1, b2, b3, 17);
  PCKEV_H2_SH(b1, b0, b3, b2, tmp2, tmp3);
  tmp0 = (tmp2 > maxlevel);
  tmp1 = (tmp3 > maxlevel);
  tmp2 = (v8i16)__msa_bmnz_v((v16u8)tmp2, (v16u8)maxlevel, (v16u8)tmp0);
  tmp3 = (v8i16)__msa_bmnz_v((v16u8)tmp3, (v16u8)maxlevel, (v16u8)tmp1);
  SUB2(zero, tmp2, zero, tmp3, tmp0, tmp1);
  tmp2 = (v8i16)__msa_bmnz_v((v16u8)tmp2, (v16u8)tmp0, (v16u8)sign0);
  tmp3 = (v8i16)__msa_bmnz_v((v16u8)tmp3, (v16u8)tmp1, (v16u8)sign1);
  LD_SW4(&mtx->zthresh_[0], 4, t0, t1, t2, t3);   // zthresh
  t0 = (s0 > t0);
  t1 = (s1 > t1);
  t2 = (s2 > t2);
  t3 = (s3 > t3);
  PCKEV_H2_SH(t1, t0, t3, t2, tmp0, tmp1);
  tmp4 = (v8i16)__msa_bmnz_v((v16u8)zero, (v16u8)tmp2, (v16u8)tmp0);
  tmp5 = (v8i16)__msa_bmnz_v((v16u8)zero, (v16u8)tmp3, (v16u8)tmp1);
  LD_SH2(&mtx->q_[0], 8, tmp0, tmp1);
  MUL2(tmp4, tmp0, tmp5, tmp1, in0, in1);
  VSHF_H2_SH(tmp4, tmp5, tmp4, tmp5, zigzag0, zigzag1, out0, out1);
  ST_SH2(in0, in1, &in[0], 8);
  ST_SH2(out0, out1, &out[0], 8);
  out0 = __msa_add_a_h(out0, out1);
  sum = HADD_SH_S32(out0);
  return (sum > 0);
}

static int Quantize2Blocks_MSA(int16_t in[32], int16_t out[32],
                               const VP8Matrix* WEBP_RESTRICT const mtx) {
  int nz;
  nz  = VP8EncQuantizeBlock(in + 0 * 16, out + 0 * 16, mtx) << 0;
  nz |= VP8EncQuantizeBlock(in + 1 * 16, out + 1 * 16, mtx) << 1;
  return nz;
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspInitMSA(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitMSA(void) {
  VP8ITransform = ITransform_MSA;
  VP8FTransform = FTransform_MSA;
  VP8FTransformWHT = FTransformWHT_MSA;

  VP8TDisto4x4 = Disto4x4_MSA;
  VP8TDisto16x16 = Disto16x16_MSA;
  VP8CollectHistogram = CollectHistogram_MSA;

  VP8EncPredLuma4 = Intra4Preds_MSA;
  VP8EncPredLuma16 = Intra16Preds_MSA;
  VP8EncPredChroma8 = IntraChromaPreds_MSA;

  VP8SSE16x16 = SSE16x16_MSA;
  VP8SSE16x8 = SSE16x8_MSA;
  VP8SSE8x8 = SSE8x8_MSA;
  VP8SSE4x4 = SSE4x4_MSA;

  VP8EncQuantizeBlock = QuantizeBlock_MSA;
  VP8EncQuantize2Blocks = Quantize2Blocks_MSA;
  VP8EncQuantizeBlockWHT = QuantizeBlock_MSA;
}

#else  // !WEBP_USE_MSA

WEBP_DSP_INIT_STUB(VP8EncDspInitMSA)

#endif  // WEBP_USE_MSA
