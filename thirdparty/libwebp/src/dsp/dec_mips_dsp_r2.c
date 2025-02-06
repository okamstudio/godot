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

#if defined(WEBP_USE_MIPS_DSP_R2)

#include "src/dsp/mips_macro.h"

static const int kC1 = WEBP_TRANSFORM_AC3_C1;
static const int kC2 = WEBP_TRANSFORM_AC3_C2;

static void TransformDC(const int16_t* WEBP_RESTRICT in,
                        uint8_t* WEBP_RESTRICT dst) {
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9, temp10;

  __asm__ volatile (
    LOAD_WITH_OFFSET_X4(temp1, temp2, temp3, temp4, dst,
                        0, 0, 0, 0,
                        0, 1, 2, 3,
                        BPS)
    "lh               %[temp5],  0(%[in])               \n\t"
    "addiu            %[temp5],  %[temp5],  4           \n\t"
    "ins              %[temp5],  %[temp5],  16, 16      \n\t"
    "shra.ph          %[temp5],  %[temp5],  3           \n\t"
    CONVERT_2_BYTES_TO_HALF(temp6, temp7, temp8, temp9, temp10, temp1, temp2,
                            temp3, temp1, temp2, temp3, temp4)
    STORE_SAT_SUM_X2(temp6, temp7, temp8, temp9, temp10, temp1, temp2, temp3,
                     temp5, temp5, temp5, temp5, temp5, temp5, temp5, temp5,
                     dst, 0, 1, 2, 3, BPS)

    OUTPUT_EARLY_CLOBBER_REGS_10()
    : [in]"r"(in), [dst]"r"(dst)
    : "memory"
  );
}

static void TransformAC3(const int16_t* WEBP_RESTRICT in,
                         uint8_t* WEBP_RESTRICT dst) {
  const int a = in[0] + 4;
  int c4 = WEBP_TRANSFORM_AC3_MUL2(in[4]);
  const int d4 = WEBP_TRANSFORM_AC3_MUL1(in[4]);
  const int c1 = WEBP_TRANSFORM_AC3_MUL2(in[1]);
  const int d1 = WEBP_TRANSFORM_AC3_MUL1(in[1]);
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9;
  int temp10, temp11, temp12, temp13, temp14, temp15, temp16, temp17, temp18;

  __asm__ volatile (
    "ins              %[c4],      %[d4],     16,       16    \n\t"
    "replv.ph         %[temp1],   %[a]                       \n\t"
    "replv.ph         %[temp4],   %[d1]                      \n\t"
    ADD_SUB_HALVES(temp2, temp3, temp1, c4)
    "replv.ph         %[temp5],   %[c1]                      \n\t"
    SHIFT_R_SUM_X2(temp1, temp6, temp7, temp8, temp2, temp9, temp10, temp4,
                   temp2, temp2, temp3, temp3, temp4, temp5, temp4, temp5)
    LOAD_WITH_OFFSET_X4(temp3, temp5, temp11, temp12, dst,
                        0, 0, 0, 0,
                        0, 1, 2, 3,
                        BPS)
    CONVERT_2_BYTES_TO_HALF(temp13, temp14, temp3, temp15, temp5, temp16,
                            temp11, temp17, temp3, temp5, temp11, temp12)
    PACK_2_HALVES_TO_WORD(temp12, temp18, temp7, temp6, temp1, temp8, temp2,
                          temp4, temp7, temp6, temp10, temp9)
    STORE_SAT_SUM_X2(temp13, temp14, temp3, temp15, temp5, temp16, temp11,
                     temp17, temp12, temp18, temp1, temp8, temp2, temp4,
                     temp7, temp6, dst, 0, 1, 2, 3, BPS)

    OUTPUT_EARLY_CLOBBER_REGS_18(),
      [c4]"+&r"(c4)
    : [dst]"r"(dst), [a]"r"(a), [d1]"r"(d1), [d4]"r"(d4), [c1]"r"(c1)
    : "memory"
  );
}

static void TransformOne(const int16_t* WEBP_RESTRICT in,
                         uint8_t* WEBP_RESTRICT dst) {
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9;
  int temp10, temp11, temp12, temp13, temp14, temp15, temp16, temp17, temp18;

  __asm__ volatile (
    "ulw              %[temp1],   0(%[in])                 \n\t"
    "ulw              %[temp2],   16(%[in])                \n\t"
    LOAD_IN_X2(temp5, temp6, 24, 26)
    ADD_SUB_HALVES(temp3, temp4, temp1, temp2)
    LOAD_IN_X2(temp1, temp2, 8, 10)
    MUL_SHIFT_SUM(temp7, temp8, temp9, temp10, temp11, temp12, temp13, temp14,
                  temp10, temp8, temp9, temp7, temp1, temp2, temp5, temp6,
                  temp13, temp11, temp14, temp12)
    INSERT_HALF_X2(temp8, temp7, temp10, temp9)
    "ulw              %[temp17],  4(%[in])                 \n\t"
    "ulw              %[temp18],  20(%[in])                \n\t"
    ADD_SUB_HALVES(temp1, temp2, temp3, temp8)
    ADD_SUB_HALVES(temp5, temp6, temp4, temp7)
    ADD_SUB_HALVES(temp7, temp8, temp17, temp18)
    LOAD_IN_X2(temp17, temp18, 12, 14)
    LOAD_IN_X2(temp9, temp10, 28, 30)
    MUL_SHIFT_SUM(temp11, temp12, temp13, temp14, temp15, temp16, temp4, temp17,
                  temp12, temp14, temp11, temp13, temp17, temp18, temp9, temp10,
                  temp15, temp4, temp16, temp17)
    INSERT_HALF_X2(temp11, temp12, temp13, temp14)
    ADD_SUB_HALVES(temp17, temp8, temp8, temp11)
    ADD_SUB_HALVES(temp3, temp4, temp7, temp12)

    // horizontal
    SRA_16(temp9, temp10, temp11, temp12, temp1, temp2, temp5, temp6)
    INSERT_HALF_X2(temp1, temp6, temp5, temp2)
    SRA_16(temp13, temp14, temp15, temp16, temp3, temp4, temp17, temp8)
    "repl.ph          %[temp2],   0x4                      \n\t"
    INSERT_HALF_X2(temp3, temp8, temp17, temp4)
    "addq.ph          %[temp1],   %[temp1],  %[temp2]      \n\t"
    "addq.ph          %[temp6],   %[temp6],  %[temp2]      \n\t"
    ADD_SUB_HALVES(temp2, temp4, temp1, temp3)
    ADD_SUB_HALVES(temp5, temp7, temp6, temp8)
    MUL_SHIFT_SUM(temp1, temp3, temp6, temp8, temp9, temp13, temp17, temp18,
                  temp3, temp13, temp1, temp9, temp9, temp13, temp11, temp15,
                  temp6, temp17, temp8, temp18)
    MUL_SHIFT_SUM(temp6, temp8, temp18, temp17, temp11, temp15, temp12, temp16,
                  temp8, temp15, temp6, temp11, temp12, temp16, temp10, temp14,
                  temp18, temp12, temp17, temp16)
    INSERT_HALF_X2(temp1, temp3, temp9, temp13)
    INSERT_HALF_X2(temp6, temp8, temp11, temp15)
    SHIFT_R_SUM_X2(temp9, temp10, temp11, temp12, temp13, temp14, temp15,
                   temp16, temp2, temp4, temp5, temp7, temp3, temp1, temp8,
                   temp6)
    PACK_2_HALVES_TO_WORD(temp1, temp2, temp3, temp4, temp9, temp12, temp13,
                          temp16, temp11, temp10, temp15, temp14)
    LOAD_WITH_OFFSET_X4(temp10, temp11, temp14, temp15, dst,
                        0, 0, 0, 0,
                        0, 1, 2, 3,
                        BPS)
    CONVERT_2_BYTES_TO_HALF(temp5, temp6, temp7, temp8, temp17, temp18, temp10,
                            temp11, temp10, temp11, temp14, temp15)
    STORE_SAT_SUM_X2(temp5, temp6, temp7, temp8, temp17, temp18, temp10, temp11,
                     temp9, temp12, temp1, temp2, temp13, temp16, temp3, temp4,
                     dst, 0, 1, 2, 3, BPS)

    OUTPUT_EARLY_CLOBBER_REGS_18()
    : [dst]"r"(dst), [in]"r"(in), [kC1]"r"(kC1), [kC2]"r"(kC2)
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

static WEBP_INLINE void FilterLoop26(uint8_t* p,
                                     int hstride, int vstride, int size,
                                     int thresh, int ithresh, int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9;
  int temp10, temp11, temp12, temp13, temp14, temp15;

  __asm__ volatile (
    ".set      push                                      \n\t"
    ".set      noreorder                                 \n\t"
  "1:                                                    \n\t"
    "negu      %[temp1],  %[hstride]                     \n\t"
    "addiu     %[size],   %[size],        -1             \n\t"
    "sll       %[temp2],  %[hstride],     1              \n\t"
    "sll       %[temp3],  %[temp1],       1              \n\t"
    "addu      %[temp4],  %[temp2],       %[hstride]     \n\t"
    "addu      %[temp5],  %[temp3],       %[temp1]       \n\t"
    "lbu       %[temp7],  0(%[p])                        \n\t"
    "sll       %[temp6],  %[temp3],       1              \n\t"
    "lbux      %[temp8],  %[temp5](%[p])                 \n\t"
    "lbux      %[temp9],  %[temp3](%[p])                 \n\t"
    "lbux      %[temp10], %[temp1](%[p])                 \n\t"
    "lbux      %[temp11], %[temp6](%[p])                 \n\t"
    "lbux      %[temp12], %[hstride](%[p])               \n\t"
    "lbux      %[temp13], %[temp2](%[p])                 \n\t"
    "lbux      %[temp14], %[temp4](%[p])                 \n\t"
    "subu      %[temp1],  %[temp10],      %[temp7]       \n\t"
    "subu      %[temp2],  %[temp9],       %[temp12]      \n\t"
    "absq_s.w  %[temp3],  %[temp1]                       \n\t"
    "absq_s.w  %[temp4],  %[temp2]                       \n\t"
    "negu      %[temp1],  %[temp1]                       \n\t"
    "sll       %[temp3],  %[temp3],       2              \n\t"
    "addu      %[temp15], %[temp3],       %[temp4]       \n\t"
    "subu      %[temp3],  %[temp15],      %[thresh2]     \n\t"
    "sll       %[temp6],  %[temp1],       1              \n\t"
    "bgtz      %[temp3],  3f                             \n\t"
    " subu     %[temp4],  %[temp11],      %[temp8]       \n\t"
    "absq_s.w  %[temp4],  %[temp4]                       \n\t"
    "shll_s.w  %[temp2],  %[temp2],       24             \n\t"
    "subu      %[temp4],  %[temp4],       %[ithresh]     \n\t"
    "bgtz      %[temp4],  3f                             \n\t"
    " subu     %[temp3],  %[temp8],       %[temp9]       \n\t"
    "absq_s.w  %[temp3],  %[temp3]                       \n\t"
    "subu      %[temp3],  %[temp3],       %[ithresh]     \n\t"
    "bgtz      %[temp3],  3f                             \n\t"
    " subu     %[temp5],  %[temp9],       %[temp10]      \n\t"
    "absq_s.w  %[temp3],  %[temp5]                       \n\t"
    "absq_s.w  %[temp5],  %[temp5]                       \n\t"
    "subu      %[temp3],  %[temp3],       %[ithresh]     \n\t"
    "bgtz      %[temp3],  3f                             \n\t"
    " subu     %[temp3],  %[temp14],      %[temp13]      \n\t"
    "absq_s.w  %[temp3],  %[temp3]                       \n\t"
    "slt       %[temp5],  %[hev_thresh],  %[temp5]       \n\t"
    "subu      %[temp3],  %[temp3],       %[ithresh]     \n\t"
    "bgtz      %[temp3],  3f                             \n\t"
    " subu     %[temp3],  %[temp13],      %[temp12]      \n\t"
    "absq_s.w  %[temp3],  %[temp3]                       \n\t"
    "sra       %[temp4],  %[temp2],       24             \n\t"
    "subu      %[temp3],  %[temp3],       %[ithresh]     \n\t"
    "bgtz      %[temp3],  3f                             \n\t"
    " subu     %[temp15], %[temp12],      %[temp7]       \n\t"
    "absq_s.w  %[temp3],  %[temp15]                      \n\t"
    "absq_s.w  %[temp15], %[temp15]                      \n\t"
    "subu      %[temp3],  %[temp3],       %[ithresh]     \n\t"
    "bgtz      %[temp3],  3f                             \n\t"
    " slt      %[temp15], %[hev_thresh],  %[temp15]      \n\t"
    "addu      %[temp3],  %[temp6],       %[temp1]       \n\t"
    "or        %[temp2],  %[temp5],       %[temp15]      \n\t"
    "addu      %[temp5],  %[temp4],       %[temp3]       \n\t"
    "beqz      %[temp2],  4f                             \n\t"
    " shra_r.w %[temp1],  %[temp5],       3              \n\t"
    "addiu     %[temp2],  %[temp5],       3              \n\t"
    "sra       %[temp2],  %[temp2],       3              \n\t"
    "shll_s.w  %[temp1],  %[temp1],       27             \n\t"
    "shll_s.w  %[temp2],  %[temp2],       27             \n\t"
    "subu      %[temp3],  %[p],           %[hstride]     \n\t"
    "sra       %[temp1],  %[temp1],       27             \n\t"
    "sra       %[temp2],  %[temp2],       27             \n\t"
    "subu      %[temp1],  %[temp7],       %[temp1]       \n\t"
    "addu      %[temp2],  %[temp10],      %[temp2]       \n\t"
    "lbux      %[temp2],  %[temp2](%[VP8kclip1])         \n\t"
    "lbux      %[temp1],  %[temp1](%[VP8kclip1])         \n\t"
    "sb        %[temp2],  0(%[temp3])                    \n\t"
    "j         3f                                        \n\t"
    " sb       %[temp1],  0(%[p])                        \n\t"
  "4:                                                    \n\t"
    "shll_s.w  %[temp5],  %[temp5],       24             \n\t"
    "subu      %[temp14], %[p],           %[hstride]     \n\t"
    "subu      %[temp11], %[temp14],      %[hstride]     \n\t"
    "sra       %[temp6],  %[temp5],       24             \n\t"
    "sll       %[temp1],  %[temp6],       3              \n\t"
    "subu      %[temp15], %[temp11],      %[hstride]     \n\t"
    "addu      %[temp2],  %[temp6],       %[temp1]       \n\t"
    "sll       %[temp3],  %[temp2],       1              \n\t"
    "addu      %[temp4],  %[temp3],       %[temp2]       \n\t"
    "addiu     %[temp2],  %[temp2],       63             \n\t"
    "addiu     %[temp3],  %[temp3],       63             \n\t"
    "addiu     %[temp4],  %[temp4],       63             \n\t"
    "sra       %[temp2],  %[temp2],       7              \n\t"
    "sra       %[temp3],  %[temp3],       7              \n\t"
    "sra       %[temp4],  %[temp4],       7              \n\t"
    "addu      %[temp1],  %[temp8],       %[temp2]       \n\t"
    "addu      %[temp5],  %[temp9],       %[temp3]       \n\t"
    "addu      %[temp6],  %[temp10],      %[temp4]       \n\t"
    "subu      %[temp8],  %[temp7],       %[temp4]       \n\t"
    "subu      %[temp7],  %[temp12],      %[temp3]       \n\t"
    "addu      %[temp10], %[p],           %[hstride]     \n\t"
    "subu      %[temp9],  %[temp13],      %[temp2]       \n\t"
    "addu      %[temp12], %[temp10],      %[hstride]     \n\t"
    "lbux      %[temp2],  %[temp1](%[VP8kclip1])         \n\t"
    "lbux      %[temp3],  %[temp5](%[VP8kclip1])         \n\t"
    "lbux      %[temp4],  %[temp6](%[VP8kclip1])         \n\t"
    "lbux      %[temp5],  %[temp8](%[VP8kclip1])         \n\t"
    "lbux      %[temp6],  %[temp7](%[VP8kclip1])         \n\t"
    "lbux      %[temp8],  %[temp9](%[VP8kclip1])         \n\t"
    "sb        %[temp2],  0(%[temp15])                   \n\t"
    "sb        %[temp3],  0(%[temp11])                   \n\t"
    "sb        %[temp4],  0(%[temp14])                   \n\t"
    "sb        %[temp5],  0(%[p])                        \n\t"
    "sb        %[temp6],  0(%[temp10])                   \n\t"
    "sb        %[temp8],  0(%[temp12])                   \n\t"
  "3:                                                    \n\t"
    "bgtz      %[size],   1b                             \n\t"
    " addu     %[p],      %[p],           %[vstride]     \n\t"
    ".set      pop                                       \n\t"
    : [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),[temp3]"=&r"(temp3),
      [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [temp6]"=&r"(temp6),
      [temp7]"=&r"(temp7),[temp8]"=&r"(temp8),[temp9]"=&r"(temp9),
      [temp10]"=&r"(temp10),[temp11]"=&r"(temp11),[temp12]"=&r"(temp12),
      [temp13]"=&r"(temp13),[temp14]"=&r"(temp14),[temp15]"=&r"(temp15),
      [size]"+&r"(size), [p]"+&r"(p)
    : [hstride]"r"(hstride), [thresh2]"r"(thresh2),
      [ithresh]"r"(ithresh),[vstride]"r"(vstride), [hev_thresh]"r"(hev_thresh),
      [VP8kclip1]"r"(VP8kclip1)
    : "memory"
  );
}

static WEBP_INLINE void FilterLoop24(uint8_t* p,
                                     int hstride, int vstride, int size,
                                     int thresh, int ithresh, int hev_thresh) {
  int p0, q0, p1, q1, p2, q2, p3, q3;
  int step1, step2, temp1, temp2, temp3, temp4;
  uint8_t* pTemp0;
  uint8_t* pTemp1;
  const int thresh2 = 2 * thresh + 1;

  __asm__ volatile (
    ".set      push                                   \n\t"
    ".set      noreorder                              \n\t"
    "bltz      %[size],    3f                         \n\t"
    " nop                                             \n\t"
  "2:                                                 \n\t"
    "negu      %[step1],   %[hstride]                 \n\t"
    "lbu       %[q0],      0(%[p])                    \n\t"
    "lbux      %[p0],      %[step1](%[p])             \n\t"
    "subu      %[step1],   %[step1],      %[hstride]  \n\t"
    "lbux      %[q1],      %[hstride](%[p])           \n\t"
    "subu      %[temp1],   %[p0],         %[q0]       \n\t"
    "lbux      %[p1],      %[step1](%[p])             \n\t"
    "addu      %[step2],   %[hstride],    %[hstride]  \n\t"
    "absq_s.w  %[temp2],   %[temp1]                   \n\t"
    "subu      %[temp3],   %[p1],         %[q1]       \n\t"
    "absq_s.w  %[temp4],   %[temp3]                   \n\t"
    "sll       %[temp2],   %[temp2],      2           \n\t"
    "addu      %[temp2],   %[temp2],      %[temp4]    \n\t"
    "subu      %[temp4],   %[temp2],      %[thresh2]  \n\t"
    "subu      %[step1],   %[step1],      %[hstride]  \n\t"
    "bgtz      %[temp4],   0f                         \n\t"
    " lbux     %[p2],      %[step1](%[p])             \n\t"
    "subu      %[step1],   %[step1],      %[hstride]  \n\t"
    "lbux      %[q2],      %[step2](%[p])             \n\t"
    "lbux      %[p3],      %[step1](%[p])             \n\t"
    "subu      %[temp4],   %[p2],         %[p1]       \n\t"
    "addu      %[step2],   %[step2],      %[hstride]  \n\t"
    "subu      %[temp2],   %[p3],         %[p2]       \n\t"
    "absq_s.w  %[temp4],   %[temp4]                   \n\t"
    "absq_s.w  %[temp2],   %[temp2]                   \n\t"
    "lbux      %[q3],      %[step2](%[p])             \n\t"
    "subu      %[temp4],   %[temp4],      %[ithresh]  \n\t"
    "negu      %[temp1],   %[temp1]                   \n\t"
    "bgtz      %[temp4],   0f                         \n\t"
    " subu     %[temp2],   %[temp2],      %[ithresh]  \n\t"
    "subu      %[p3],      %[p1],         %[p0]       \n\t"
    "bgtz      %[temp2],   0f                         \n\t"
    " absq_s.w %[p3],      %[p3]                      \n\t"
    "subu      %[temp4],   %[q3],         %[q2]       \n\t"
    "subu      %[pTemp0],  %[p],          %[hstride]  \n\t"
    "absq_s.w  %[temp4],   %[temp4]                   \n\t"
    "subu      %[temp2],   %[p3],         %[ithresh]  \n\t"
    "sll       %[step1],   %[temp1],      1           \n\t"
    "bgtz      %[temp2],   0f                         \n\t"
    " subu     %[temp4],   %[temp4],      %[ithresh]  \n\t"
    "subu      %[temp2],   %[q2],         %[q1]       \n\t"
    "bgtz      %[temp4],   0f                         \n\t"
    " absq_s.w %[temp2],   %[temp2]                   \n\t"
    "subu      %[q3],      %[q1],         %[q0]       \n\t"
    "absq_s.w  %[q3],      %[q3]                      \n\t"
    "subu      %[temp2],   %[temp2],      %[ithresh]  \n\t"
    "addu      %[temp1],   %[temp1],      %[step1]    \n\t"
    "bgtz      %[temp2],   0f                         \n\t"
    " subu     %[temp4],   %[q3],         %[ithresh]  \n\t"
    "slt       %[p3],      %[hev_thresh], %[p3]       \n\t"
    "bgtz      %[temp4],   0f                         \n\t"
    " slt      %[q3],      %[hev_thresh], %[q3]       \n\t"
    "or        %[q3],      %[q3],         %[p3]       \n\t"
    "bgtz      %[q3],      1f                         \n\t"
    " shra_r.w %[temp2],   %[temp1],      3           \n\t"
    "addiu     %[temp1],   %[temp1],      3           \n\t"
    "sra       %[temp1],   %[temp1],      3           \n\t"
    "shll_s.w  %[temp2],   %[temp2],      27          \n\t"
    "shll_s.w  %[temp1],   %[temp1],      27          \n\t"
    "addu      %[pTemp1],  %[p],          %[hstride]  \n\t"
    "sra       %[temp2],   %[temp2],      27          \n\t"
    "sra       %[temp1],   %[temp1],      27          \n\t"
    "addiu     %[step1],   %[temp2],      1           \n\t"
    "sra       %[step1],   %[step1],      1           \n\t"
    "addu      %[p0],      %[p0],         %[temp1]    \n\t"
    "addu      %[p1],      %[p1],         %[step1]    \n\t"
    "subu      %[q0],      %[q0],         %[temp2]    \n\t"
    "subu      %[q1],      %[q1],         %[step1]    \n\t"
    "lbux      %[temp2],   %[p0](%[VP8kclip1])        \n\t"
    "lbux      %[temp3],   %[q0](%[VP8kclip1])        \n\t"
    "lbux      %[temp4],   %[q1](%[VP8kclip1])        \n\t"
    "sb        %[temp2],   0(%[pTemp0])               \n\t"
    "lbux      %[temp1],   %[p1](%[VP8kclip1])        \n\t"
    "subu      %[pTemp0],  %[pTemp0],    %[hstride]   \n\t"
    "sb        %[temp3],   0(%[p])                    \n\t"
    "sb        %[temp4],   0(%[pTemp1])               \n\t"
    "j         0f                                     \n\t"
    " sb       %[temp1],   0(%[pTemp0])               \n\t"
  "1:                                                 \n\t"
    "shll_s.w  %[temp3],   %[temp3],      24          \n\t"
    "sra       %[temp3],   %[temp3],      24          \n\t"
    "addu      %[temp1],   %[temp1],      %[temp3]    \n\t"
    "shra_r.w  %[temp2],   %[temp1],      3           \n\t"
    "addiu     %[temp1],   %[temp1],      3           \n\t"
    "shll_s.w  %[temp2],   %[temp2],      27          \n\t"
    "sra       %[temp1],   %[temp1],      3           \n\t"
    "shll_s.w  %[temp1],   %[temp1],      27          \n\t"
    "sra       %[temp2],   %[temp2],      27          \n\t"
    "sra       %[temp1],   %[temp1],      27          \n\t"
    "addu      %[p0],      %[p0],         %[temp1]    \n\t"
    "subu      %[q0],      %[q0],         %[temp2]    \n\t"
    "lbux      %[temp1],   %[p0](%[VP8kclip1])        \n\t"
    "lbux      %[temp2],   %[q0](%[VP8kclip1])        \n\t"
    "sb        %[temp2],   0(%[p])                    \n\t"
    "sb        %[temp1],   0(%[pTemp0])               \n\t"
  "0:                                                 \n\t"
    "subu      %[size],    %[size],       1           \n\t"
    "bgtz      %[size],    2b                         \n\t"
    " addu     %[p],       %[p],          %[vstride]  \n\t"
  "3:                                                 \n\t"
    ".set      pop                                    \n\t"
    : [p0]"=&r"(p0), [q0]"=&r"(q0), [p1]"=&r"(p1), [q1]"=&r"(q1),
      [p2]"=&r"(p2), [q2]"=&r"(q2), [p3]"=&r"(p3), [q3]"=&r"(q3),
      [step2]"=&r"(step2), [step1]"=&r"(step1), [temp1]"=&r"(temp1),
      [temp2]"=&r"(temp2), [temp3]"=&r"(temp3), [temp4]"=&r"(temp4),
      [pTemp0]"=&r"(pTemp0), [pTemp1]"=&r"(pTemp1), [p]"+&r"(p),
      [size]"+&r"(size)
    : [vstride]"r"(vstride), [ithresh]"r"(ithresh),
      [hev_thresh]"r"(hev_thresh), [hstride]"r"(hstride),
      [VP8kclip1]"r"(VP8kclip1), [thresh2]"r"(thresh2)
    : "memory"
  );
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

//------------------------------------------------------------------------------
// Simple In-loop filtering (Paragraph 15.2)

static void SimpleVFilter16(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8;
  uint8_t* p1 = p - stride;
  __asm__ volatile (
    ".set      push                                      \n\t"
    ".set      noreorder                                 \n\t"
    "li        %[i],        16                           \n\t"
  "0:                                                    \n\t"
    "negu      %[temp4],    %[stride]                    \n\t"
    "sll       %[temp5],    %[temp4],       1            \n\t"
    "lbu       %[temp2],    0(%[p])                      \n\t"
    "lbux      %[temp3],    %[stride](%[p])              \n\t"
    "lbux      %[temp1],    %[temp4](%[p])               \n\t"
    "lbux      %[temp0],    %[temp5](%[p])               \n\t"
    "subu      %[temp7],    %[temp1],       %[temp2]     \n\t"
    "subu      %[temp6],    %[temp0],       %[temp3]     \n\t"
    "absq_s.w  %[temp4],    %[temp7]                     \n\t"
    "absq_s.w  %[temp5],    %[temp6]                     \n\t"
    "sll       %[temp4],    %[temp4],       2            \n\t"
    "subu      %[temp5],    %[temp5],       %[thresh2]   \n\t"
    "addu      %[temp5],    %[temp4],       %[temp5]     \n\t"
    "negu      %[temp8],    %[temp7]                     \n\t"
    "bgtz      %[temp5],    1f                           \n\t"
    " addiu    %[i],        %[i],           -1           \n\t"
    "sll       %[temp4],    %[temp8],       1            \n\t"
    "shll_s.w  %[temp5],    %[temp6],       24           \n\t"
    "addu      %[temp3],    %[temp4],       %[temp8]     \n\t"
    "sra       %[temp5],    %[temp5],       24           \n\t"
    "addu      %[temp3],    %[temp3],       %[temp5]     \n\t"
    "addiu     %[temp7],    %[temp3],       3            \n\t"
    "sra       %[temp7],    %[temp7],       3            \n\t"
    "shra_r.w  %[temp8],    %[temp3],       3            \n\t"
    "shll_s.w  %[temp0],    %[temp7],       27           \n\t"
    "shll_s.w  %[temp4],    %[temp8],       27           \n\t"
    "sra       %[temp0],    %[temp0],       27           \n\t"
    "sra       %[temp4],    %[temp4],       27           \n\t"
    "addu      %[temp7],    %[temp1],       %[temp0]     \n\t"
    "subu      %[temp2],    %[temp2],       %[temp4]     \n\t"
    "lbux      %[temp3],    %[temp7](%[VP8kclip1])       \n\t"
    "lbux      %[temp4],    %[temp2](%[VP8kclip1])       \n\t"
    "sb        %[temp3],    0(%[p1])                     \n\t"
    "sb        %[temp4],    0(%[p])                      \n\t"
  "1:                                                    \n\t"
    "addiu     %[p1],       %[p1],          1            \n\t"
    "bgtz      %[i],        0b                           \n\t"
    " addiu    %[p],        %[p],           1            \n\t"
    " .set     pop                                       \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [p]"+&r"(p), [i]"=&r"(i), [p1]"+&r"(p1)
    : [stride]"r"(stride), [VP8kclip1]"r"(VP8kclip1), [thresh2]"r"(thresh2)
    : "memory"
  );
}

// TEMP0 = SRC[A + A1 * BPS]
// TEMP1 = SRC[B + B1 * BPS]
// TEMP2 = SRC[C + C1 * BPS]
// TEMP3 = SRC[D + D1 * BPS]
#define LOAD_4_BYTES(TEMP0, TEMP1, TEMP2, TEMP3,                               \
                     A, A1, B, B1, C, C1, D, D1, SRC)                          \
  "lbu      %[" #TEMP0 "],   " #A "+" #A1 "*" XSTR(BPS) "(%[" #SRC "]) \n\t"   \
  "lbu      %[" #TEMP1 "],   " #B "+" #B1 "*" XSTR(BPS) "(%[" #SRC "]) \n\t"   \
  "lbu      %[" #TEMP2 "],   " #C "+" #C1 "*" XSTR(BPS) "(%[" #SRC "]) \n\t"   \
  "lbu      %[" #TEMP3 "],   " #D "+" #D1 "*" XSTR(BPS) "(%[" #SRC "]) \n\t"   \

static void SimpleHFilter16(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8;
  __asm__ volatile (
    ".set      push                                     \n\t"
    ".set      noreorder                                \n\t"
    "li        %[i],       16                           \n\t"
  "0:                                                   \n\t"
    LOAD_4_BYTES(temp0, temp1, temp2, temp3, -2, 0, -1, 0, 0, 0, 1, 0, p)
    "subu      %[temp7],    %[temp1],       %[temp2]    \n\t"
    "subu      %[temp6],    %[temp0],       %[temp3]    \n\t"
    "absq_s.w  %[temp4],    %[temp7]                    \n\t"
    "absq_s.w  %[temp5],    %[temp6]                    \n\t"
    "sll       %[temp4],    %[temp4],       2           \n\t"
    "addu      %[temp5],    %[temp4],       %[temp5]    \n\t"
    "subu      %[temp5],    %[temp5],       %[thresh2]  \n\t"
    "negu      %[temp8],    %[temp7]                    \n\t"
    "bgtz      %[temp5],    1f                          \n\t"
    " addiu    %[i],        %[i],           -1          \n\t"
    "sll       %[temp4],    %[temp8],       1           \n\t"
    "shll_s.w  %[temp5],    %[temp6],       24          \n\t"
    "addu      %[temp3],    %[temp4],       %[temp8]    \n\t"
    "sra       %[temp5],    %[temp5],       24          \n\t"
    "addu      %[temp3],    %[temp3],       %[temp5]    \n\t"
    "addiu     %[temp7],    %[temp3],       3           \n\t"
    "sra       %[temp7],    %[temp7],       3           \n\t"
    "shra_r.w  %[temp8],    %[temp3],       3           \n\t"
    "shll_s.w  %[temp0],    %[temp7],       27          \n\t"
    "shll_s.w  %[temp4],    %[temp8],       27          \n\t"
    "sra       %[temp0],    %[temp0],       27          \n\t"
    "sra       %[temp4],    %[temp4],       27          \n\t"
    "addu      %[temp7],    %[temp1],       %[temp0]    \n\t"
    "subu      %[temp2],    %[temp2],       %[temp4]    \n\t"
    "lbux      %[temp3],    %[temp7](%[VP8kclip1])      \n\t"
    "lbux      %[temp4],    %[temp2](%[VP8kclip1])      \n\t"
    "sb        %[temp3],    -1(%[p])                    \n\t"
    "sb        %[temp4],    0(%[p])                     \n\t"
  "1:                                                   \n\t"
    "bgtz      %[i],        0b                          \n\t"
    " addu     %[p],        %[p],           %[stride]   \n\t"
    ".set      pop                                      \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [p]"+&r"(p), [i]"=&r"(i)
    : [stride]"r"(stride), [VP8kclip1]"r"(VP8kclip1), [thresh2]"r"(thresh2)
    : "memory"
  );
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

// DST[A * BPS]     = TEMP0
// DST[B + C * BPS] = TEMP1
#define STORE_8_BYTES(TEMP0, TEMP1, A, B, C, DST)                              \
  "usw    %[" #TEMP0 "],   " #A "*" XSTR(BPS) "(%[" #DST "])         \n\t"     \
  "usw    %[" #TEMP1 "],   " #B "+" #C "*" XSTR(BPS) "(%[" #DST "])  \n\t"

static void VE4(uint8_t* dst) {    // vertical
  const uint8_t* top = dst - BPS;
  int temp0, temp1, temp2, temp3, temp4, temp5, temp6;
  __asm__ volatile (
    "ulw             %[temp0],   -1(%[top])              \n\t"
    "ulh             %[temp1],   3(%[top])               \n\t"
    "preceu.ph.qbr   %[temp2],   %[temp0]                \n\t"
    "preceu.ph.qbl   %[temp3],   %[temp0]                \n\t"
    "preceu.ph.qbr   %[temp4],   %[temp1]                \n\t"
    "packrl.ph       %[temp5],   %[temp3],    %[temp2]   \n\t"
    "packrl.ph       %[temp6],   %[temp4],    %[temp3]   \n\t"
    "shll.ph         %[temp5],   %[temp5],    1          \n\t"
    "shll.ph         %[temp6],   %[temp6],    1          \n\t"
    "addq.ph         %[temp2],   %[temp5],    %[temp2]   \n\t"
    "addq.ph         %[temp6],   %[temp6],    %[temp4]   \n\t"
    "addq.ph         %[temp2],   %[temp2],    %[temp3]   \n\t"
    "addq.ph         %[temp6],   %[temp6],    %[temp3]   \n\t"
    "shra_r.ph       %[temp2],   %[temp2],    2          \n\t"
    "shra_r.ph       %[temp6],   %[temp6],    2          \n\t"
    "precr.qb.ph     %[temp4],   %[temp6],    %[temp2]   \n\t"
    STORE_8_BYTES(temp4, temp4, 0, 0, 1, dst)
    STORE_8_BYTES(temp4, temp4, 2, 0, 3, dst)
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void DC4(uint8_t* dst) {   // DC
  int temp0, temp1, temp2, temp3, temp4;
  __asm__ volatile (
    "ulw          %[temp0],   -1*" XSTR(BPS) "(%[dst]) \n\t"
    LOAD_4_BYTES(temp1, temp2, temp3, temp4, -1, 0, -1, 1, -1, 2, -1, 3, dst)
    "ins          %[temp1],   %[temp2],    8,     8    \n\t"
    "ins          %[temp1],   %[temp3],    16,    8    \n\t"
    "ins          %[temp1],   %[temp4],    24,    8    \n\t"
    "raddu.w.qb   %[temp0],   %[temp0]                 \n\t"
    "raddu.w.qb   %[temp1],   %[temp1]                 \n\t"
    "addu         %[temp0],   %[temp0],    %[temp1]    \n\t"
    "shra_r.w     %[temp0],   %[temp0],    3           \n\t"
    "replv.qb     %[temp0],   %[temp0]                 \n\t"
    STORE_8_BYTES(temp0, temp0, 0, 0, 1, dst)
    STORE_8_BYTES(temp0, temp0, 2, 0, 3, dst)
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4)
    : [dst]"r"(dst)
    : "memory"
  );
}

static void RD4(uint8_t* dst) {   // Down-right
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8;
  __asm__ volatile (
    LOAD_4_BYTES(temp0, temp1, temp2, temp3, -1, 0, -1, 1, -1, 2, -1, 3, dst)
    "ulw            %[temp7],   -1-" XSTR(BPS) "(%[dst])       \n\t"
    "ins            %[temp1],   %[temp0], 16, 16               \n\t"
    "preceu.ph.qbr  %[temp5],   %[temp7]                       \n\t"
    "ins            %[temp2],   %[temp1], 16, 16               \n\t"
    "preceu.ph.qbl  %[temp4],   %[temp7]                       \n\t"
    "ins            %[temp3],   %[temp2], 16, 16               \n\t"
    "shll.ph        %[temp2],   %[temp2], 1                    \n\t"
    "addq.ph        %[temp3],   %[temp3], %[temp1]             \n\t"
    "packrl.ph      %[temp6],   %[temp5], %[temp1]             \n\t"
    "addq.ph        %[temp3],   %[temp3], %[temp2]             \n\t"
    "addq.ph        %[temp1],   %[temp1], %[temp5]             \n\t"
    "shll.ph        %[temp6],   %[temp6], 1                    \n\t"
    "addq.ph        %[temp1],   %[temp1], %[temp6]             \n\t"
    "packrl.ph      %[temp0],   %[temp4], %[temp5]             \n\t"
    "addq.ph        %[temp8],   %[temp5], %[temp4]             \n\t"
    "shra_r.ph      %[temp3],   %[temp3], 2                    \n\t"
    "shll.ph        %[temp0],   %[temp0], 1                    \n\t"
    "shra_r.ph      %[temp1],   %[temp1], 2                    \n\t"
    "addq.ph        %[temp8],   %[temp0], %[temp8]             \n\t"
    "lbu            %[temp5],   3-" XSTR(BPS) "(%[dst])        \n\t"
    "precrq.ph.w    %[temp7],   %[temp7], %[temp7]             \n\t"
    "shra_r.ph      %[temp8],   %[temp8], 2                    \n\t"
    "ins            %[temp7],   %[temp5], 0,  8                \n\t"
    "precr.qb.ph    %[temp2],   %[temp1], %[temp3]             \n\t"
    "raddu.w.qb     %[temp4],   %[temp7]                       \n\t"
    "precr.qb.ph    %[temp6],   %[temp8], %[temp1]             \n\t"
    "shra_r.w       %[temp4],   %[temp4], 2                    \n\t"
    STORE_8_BYTES(temp2, temp6, 3, 0, 1, dst)
    "prepend        %[temp2],   %[temp8], 8                    \n\t"
    "prepend        %[temp6],   %[temp4], 8                    \n\t"
    STORE_8_BYTES(temp2, temp6, 2, 0, 0, dst)
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8)
    : [dst]"r"(dst)
    : "memory"
  );
}

// TEMP0 = SRC[A * BPS]
// TEMP1 = SRC[B + C * BPS]
#define LOAD_8_BYTES(TEMP0, TEMP1, A, B, C, SRC)                               \
  "ulw    %[" #TEMP0 "],   " #A "*" XSTR(BPS) "(%[" #SRC "])         \n\t"     \
  "ulw    %[" #TEMP1 "],   " #B "+" #C "*" XSTR(BPS) "(%[" #SRC "])  \n\t"

static void LD4(uint8_t* dst) {   // Down-Left
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8, temp9;
  __asm__ volatile (
    LOAD_8_BYTES(temp0, temp1, -1, 4, -1, dst)
    "preceu.ph.qbl   %[temp2],    %[temp0]                     \n\t"
    "preceu.ph.qbr   %[temp3],    %[temp0]                     \n\t"
    "preceu.ph.qbr   %[temp4],    %[temp1]                     \n\t"
    "preceu.ph.qbl   %[temp5],    %[temp1]                     \n\t"
    "packrl.ph       %[temp6],    %[temp2],    %[temp3]        \n\t"
    "packrl.ph       %[temp7],    %[temp4],    %[temp2]        \n\t"
    "packrl.ph       %[temp8],    %[temp5],    %[temp4]        \n\t"
    "shll.ph         %[temp6],    %[temp6],    1               \n\t"
    "addq.ph         %[temp9],    %[temp2],    %[temp6]        \n\t"
    "shll.ph         %[temp7],    %[temp7],    1               \n\t"
    "addq.ph         %[temp9],    %[temp9],    %[temp3]        \n\t"
    "shll.ph         %[temp8],    %[temp8],    1               \n\t"
    "shra_r.ph       %[temp9],    %[temp9],    2               \n\t"
    "addq.ph         %[temp3],    %[temp4],    %[temp7]        \n\t"
    "addq.ph         %[temp0],    %[temp5],    %[temp8]        \n\t"
    "addq.ph         %[temp3],    %[temp3],    %[temp2]        \n\t"
    "addq.ph         %[temp0],    %[temp0],    %[temp4]        \n\t"
    "shra_r.ph       %[temp3],    %[temp3],    2               \n\t"
    "shra_r.ph       %[temp0],    %[temp0],    2               \n\t"
    "srl             %[temp1],    %[temp1],    24              \n\t"
    "sll             %[temp1],    %[temp1],    1               \n\t"
    "raddu.w.qb      %[temp5],    %[temp5]                     \n\t"
    "precr.qb.ph     %[temp9],    %[temp3],    %[temp9]        \n\t"
    "precr.qb.ph     %[temp3],    %[temp0],    %[temp3]        \n\t"
    "addu            %[temp1],    %[temp1],    %[temp5]        \n\t"
    "shra_r.w        %[temp1],    %[temp1],    2               \n\t"
    STORE_8_BYTES(temp9, temp3, 0, 0, 2, dst)
    "prepend         %[temp9],    %[temp0],    8               \n\t"
    "prepend         %[temp3],    %[temp1],    8               \n\t"
    STORE_8_BYTES(temp9, temp3, 1, 0, 3, dst)
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9)
    : [dst]"r"(dst)
    : "memory"
  );
}

//------------------------------------------------------------------------------
// Chroma

static void DC8uv(uint8_t* dst) {     // DC
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8, temp9;
  __asm__ volatile (
    LOAD_8_BYTES(temp0, temp1, -1, 4, -1, dst)
    LOAD_4_BYTES(temp2, temp3, temp4, temp5, -1, 0, -1, 1, -1, 2, -1, 3, dst)
    LOAD_4_BYTES(temp6, temp7, temp8, temp9, -1, 4, -1, 5, -1, 6, -1, 7, dst)
    "raddu.w.qb   %[temp0],   %[temp0]                   \n\t"
    "raddu.w.qb   %[temp1],   %[temp1]                   \n\t"
    "addu         %[temp2],   %[temp2],    %[temp3]      \n\t"
    "addu         %[temp4],   %[temp4],    %[temp5]      \n\t"
    "addu         %[temp6],   %[temp6],    %[temp7]      \n\t"
    "addu         %[temp8],   %[temp8],    %[temp9]      \n\t"
    "addu         %[temp0],   %[temp0],    %[temp1]      \n\t"
    "addu         %[temp2],   %[temp2],    %[temp4]      \n\t"
    "addu         %[temp6],   %[temp6],    %[temp8]      \n\t"
    "addu         %[temp0],   %[temp0],    %[temp2]      \n\t"
    "addu         %[temp0],   %[temp0],    %[temp6]      \n\t"
    "shra_r.w     %[temp0],   %[temp0],    4             \n\t"
    "replv.qb     %[temp0],   %[temp0]                   \n\t"
    STORE_8_BYTES(temp0, temp0, 0, 4, 0, dst)
    STORE_8_BYTES(temp0, temp0, 1, 4, 1, dst)
    STORE_8_BYTES(temp0, temp0, 2, 4, 2, dst)
    STORE_8_BYTES(temp0, temp0, 3, 4, 3, dst)
    STORE_8_BYTES(temp0, temp0, 4, 4, 4, dst)
    STORE_8_BYTES(temp0, temp0, 5, 4, 5, dst)
    STORE_8_BYTES(temp0, temp0, 6, 4, 6, dst)
    STORE_8_BYTES(temp0, temp0, 7, 4, 7, dst)
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9)
    : [dst]"r"(dst)
    : "memory"
  );
}

static void DC8uvNoLeft(uint8_t* dst) {   // DC with no left samples
  int temp0, temp1;
  __asm__ volatile (
    LOAD_8_BYTES(temp0, temp1, -1, 4, -1, dst)
    "raddu.w.qb   %[temp0],   %[temp0]                   \n\t"
    "raddu.w.qb   %[temp1],   %[temp1]                   \n\t"
    "addu         %[temp0],   %[temp0],    %[temp1]      \n\t"
    "shra_r.w     %[temp0],   %[temp0],    3             \n\t"
    "replv.qb     %[temp0],   %[temp0]                   \n\t"
    STORE_8_BYTES(temp0, temp0, 0, 4, 0, dst)
    STORE_8_BYTES(temp0, temp0, 1, 4, 1, dst)
    STORE_8_BYTES(temp0, temp0, 2, 4, 2, dst)
    STORE_8_BYTES(temp0, temp0, 3, 4, 3, dst)
    STORE_8_BYTES(temp0, temp0, 4, 4, 4, dst)
    STORE_8_BYTES(temp0, temp0, 5, 4, 5, dst)
    STORE_8_BYTES(temp0, temp0, 6, 4, 6, dst)
    STORE_8_BYTES(temp0, temp0, 7, 4, 7, dst)
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1)
    : [dst]"r"(dst)
    : "memory"
  );
}

static void DC8uvNoTop(uint8_t* dst) {  // DC with no top samples
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8;
  __asm__ volatile (
    LOAD_4_BYTES(temp2, temp3, temp4, temp5, -1, 0, -1, 1, -1, 2, -1, 3, dst)
    LOAD_4_BYTES(temp6, temp7, temp8, temp1, -1, 4, -1, 5, -1, 6, -1, 7, dst)
    "addu         %[temp2],   %[temp2],    %[temp3]      \n\t"
    "addu         %[temp4],   %[temp4],    %[temp5]      \n\t"
    "addu         %[temp6],   %[temp6],    %[temp7]      \n\t"
    "addu         %[temp8],   %[temp8],    %[temp1]      \n\t"
    "addu         %[temp2],   %[temp2],    %[temp4]      \n\t"
    "addu         %[temp6],   %[temp6],    %[temp8]      \n\t"
    "addu         %[temp0],   %[temp6],    %[temp2]      \n\t"
    "shra_r.w     %[temp0],   %[temp0],    3             \n\t"
    "replv.qb     %[temp0],   %[temp0]                   \n\t"
    STORE_8_BYTES(temp0, temp0, 0, 4, 0, dst)
    STORE_8_BYTES(temp0, temp0, 1, 4, 1, dst)
    STORE_8_BYTES(temp0, temp0, 2, 4, 2, dst)
    STORE_8_BYTES(temp0, temp0, 3, 4, 3, dst)
    STORE_8_BYTES(temp0, temp0, 4, 4, 4, dst)
    STORE_8_BYTES(temp0, temp0, 5, 4, 5, dst)
    STORE_8_BYTES(temp0, temp0, 6, 4, 6, dst)
    STORE_8_BYTES(temp0, temp0, 7, 4, 7, dst)
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8)
    : [dst]"r"(dst)
    : "memory"
  );
}

#undef LOAD_8_BYTES
#undef STORE_8_BYTES
#undef LOAD_4_BYTES

#define CLIPPING(SIZE)                                                         \
  "preceu.ph.qbl   %[temp2],   %[temp0]                  \n\t"                 \
  "preceu.ph.qbr   %[temp0],   %[temp0]                  \n\t"                 \
".if " #SIZE " == 8                                      \n\t"                 \
  "preceu.ph.qbl   %[temp3],   %[temp1]                  \n\t"                 \
  "preceu.ph.qbr   %[temp1],   %[temp1]                  \n\t"                 \
".endif                                                  \n\t"                 \
  "addu.ph         %[temp2],   %[temp2],   %[dst_1]      \n\t"                 \
  "addu.ph         %[temp0],   %[temp0],   %[dst_1]      \n\t"                 \
".if " #SIZE " == 8                                      \n\t"                 \
  "addu.ph         %[temp3],   %[temp3],   %[dst_1]      \n\t"                 \
  "addu.ph         %[temp1],   %[temp1],   %[dst_1]      \n\t"                 \
".endif                                                  \n\t"                 \
  "shll_s.ph       %[temp2],   %[temp2],   7             \n\t"                 \
  "shll_s.ph       %[temp0],   %[temp0],   7             \n\t"                 \
".if " #SIZE " == 8                                      \n\t"                 \
  "shll_s.ph       %[temp3],   %[temp3],   7             \n\t"                 \
  "shll_s.ph       %[temp1],   %[temp1],   7             \n\t"                 \
".endif                                                  \n\t"                 \
  "precrqu_s.qb.ph %[temp0],   %[temp2],   %[temp0]      \n\t"                 \
".if " #SIZE " == 8                                      \n\t"                 \
  "precrqu_s.qb.ph %[temp1],   %[temp3],   %[temp1]      \n\t"                 \
".endif                                                  \n\t"


#define CLIP_8B_TO_DST(DST, TOP, SIZE) do {                                    \
  int dst_1 = ((int)(DST)[-1] << 16) + (DST)[-1];                              \
  int temp0, temp1, temp2, temp3;                                              \
  __asm__ volatile (                                                           \
  ".if " #SIZE " < 8                                     \n\t"                 \
    "ulw             %[temp0],   0(%[top])               \n\t"                 \
    "subu.ph         %[dst_1],   %[dst_1],    %[top_1]   \n\t"                 \
    CLIPPING(4)                                                                \
    "usw             %[temp0],   0(%[dst])               \n\t"                 \
  ".else                                                 \n\t"                 \
    "ulw             %[temp0],   0(%[top])               \n\t"                 \
    "ulw             %[temp1],   4(%[top])               \n\t"                 \
    "subu.ph         %[dst_1],   %[dst_1],    %[top_1]   \n\t"                 \
    CLIPPING(8)                                                                \
    "usw             %[temp0],   0(%[dst])               \n\t"                 \
    "usw             %[temp1],   4(%[dst])               \n\t"                 \
  ".if " #SIZE " == 16                                   \n\t"                 \
    "ulw             %[temp0],   8(%[top])               \n\t"                 \
    "ulw             %[temp1],   12(%[top])              \n\t"                 \
    CLIPPING(8)                                                                \
    "usw             %[temp0],   8(%[dst])               \n\t"                 \
    "usw             %[temp1],   12(%[dst])              \n\t"                 \
  ".endif                                                \n\t"                 \
  ".endif                                                \n\t"                 \
    : [dst_1]"+&r"(dst_1), [temp0]"=&r"(temp0), [temp1]"=&r"(temp1),           \
      [temp2]"=&r"(temp2), [temp3]"=&r"(temp3)                                 \
    : [top_1]"r"(top_1), [top]"r"((TOP)), [dst]"r"((DST))                      \
    : "memory"                                                                 \
  );                                                                           \
} while (0)

#define CLIP_TO_DST(DST, SIZE) do {                                            \
  int y;                                                                       \
  const uint8_t* top = (DST) - BPS;                                            \
  const int top_1 = ((int)top[-1] << 16) + top[-1];                            \
  for (y = 0; y < (SIZE); ++y) {                                               \
    CLIP_8B_TO_DST((DST), top, (SIZE));                                        \
    (DST) += BPS;                                                              \
  }                                                                            \
} while (0)

#define TRUE_MOTION(DST, SIZE)                                                 \
static void TrueMotion##SIZE(uint8_t* (DST)) {                                 \
  CLIP_TO_DST((DST), (SIZE));                                                  \
}

TRUE_MOTION(dst, 4)
TRUE_MOTION(dst, 8)
TRUE_MOTION(dst, 16)

#undef TRUE_MOTION
#undef CLIP_TO_DST
#undef CLIP_8B_TO_DST
#undef CLIPPING

//------------------------------------------------------------------------------
// Entry point

extern void VP8DspInitMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitMIPSdspR2(void) {
  VP8TransformDC = TransformDC;
  VP8TransformAC3 = TransformAC3;
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

  VP8PredLuma4[0] = DC4;
  VP8PredLuma4[1] = TrueMotion4;
  VP8PredLuma4[2] = VE4;
  VP8PredLuma4[4] = RD4;
  VP8PredLuma4[6] = LD4;

  VP8PredChroma8[0] = DC8uv;
  VP8PredChroma8[1] = TrueMotion8;
  VP8PredChroma8[4] = DC8uvNoTop;
  VP8PredChroma8[5] = DC8uvNoLeft;

  VP8PredLuma16[1] = TrueMotion16;
}

#else  // !WEBP_USE_MIPS_DSP_R2

WEBP_DSP_INIT_STUB(VP8DspInitMIPSdspR2)

#endif  // WEBP_USE_MIPS_DSP_R2
