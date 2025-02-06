// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS DSPr2 version of YUV to RGB upsampling functions.
//
// Author(s):  Branimir Vasic (branimir.vasic@imgtec.com)
//             Djordje Pesut  (djordje.pesut@imgtec.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MIPS_DSP_R2)

#include "src/dsp/yuv.h"

//------------------------------------------------------------------------------
// simple point-sampling

#define ROW_FUNC_PART_1()                                                      \
  "lbu              %[temp3],   0(%[v])                         \n\t"          \
  "lbu              %[temp4],   0(%[u])                         \n\t"          \
  "lbu              %[temp0],   0(%[y])                         \n\t"          \
  "mul              %[temp1],   %[t_con_1],     %[temp3]        \n\t"          \
  "mul              %[temp3],   %[t_con_2],     %[temp3]        \n\t"          \
  "mul              %[temp2],   %[t_con_3],     %[temp4]        \n\t"          \
  "mul              %[temp4],   %[t_con_4],     %[temp4]        \n\t"          \
  "mul              %[temp0],   %[t_con_5],     %[temp0]        \n\t"          \
  "subu             %[temp1],   %[temp1],       %[t_con_6]      \n\t"          \
  "subu             %[temp3],   %[temp3],       %[t_con_7]      \n\t"          \
  "addu             %[temp2],   %[temp2],       %[temp3]        \n\t"          \
  "subu             %[temp4],   %[temp4],       %[t_con_8]      \n\t"          \

#define ROW_FUNC_PART_2(R, G, B, K)                                            \
  "addu             %[temp5],   %[temp0],       %[temp1]        \n\t"          \
  "subu             %[temp6],   %[temp0],       %[temp2]        \n\t"          \
  "addu             %[temp7],   %[temp0],       %[temp4]        \n\t"          \
".if " #K "                                                     \n\t"          \
  "lbu              %[temp0],   1(%[y])                         \n\t"          \
".endif                                                         \n\t"          \
  "shll_s.w         %[temp5],   %[temp5],       17              \n\t"          \
  "shll_s.w         %[temp6],   %[temp6],       17              \n\t"          \
".if " #K "                                                     \n\t"          \
  "mul              %[temp0],   %[t_con_5],     %[temp0]        \n\t"          \
".endif                                                         \n\t"          \
  "shll_s.w         %[temp7],   %[temp7],       17              \n\t"          \
  "precrqu_s.qb.ph  %[temp5],   %[temp5],       $zero           \n\t"          \
  "precrqu_s.qb.ph  %[temp6],   %[temp6],       $zero           \n\t"          \
  "precrqu_s.qb.ph  %[temp7],   %[temp7],       $zero           \n\t"          \
  "srl              %[temp5],   %[temp5],       24              \n\t"          \
  "srl              %[temp6],   %[temp6],       24              \n\t"          \
  "srl              %[temp7],   %[temp7],       24              \n\t"          \
  "sb               %[temp5],   " #R "(%[dst])                  \n\t"          \
  "sb               %[temp6],   " #G "(%[dst])                  \n\t"          \
  "sb               %[temp7],   " #B "(%[dst])                  \n\t"          \

#define ASM_CLOBBER_LIST()                                                     \
  : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),             \
    [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),             \
    [temp6]"=&r"(temp6), [temp7]"=&r"(temp7)                                   \
  : [t_con_1]"r"(t_con_1), [t_con_2]"r"(t_con_2), [t_con_3]"r"(t_con_3),       \
    [t_con_4]"r"(t_con_4), [t_con_5]"r"(t_con_5), [t_con_6]"r"(t_con_6),       \
    [u]"r"(u), [v]"r"(v), [y]"r"(y), [dst]"r"(dst),                            \
    [t_con_7]"r"(t_con_7), [t_con_8]"r"(t_con_8)                               \
  : "memory", "hi", "lo"                                                       \

#define ROW_FUNC(FUNC_NAME, XSTEP, R, G, B, A)                                 \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                          \
                      const uint8_t* WEBP_RESTRICT u,                          \
                      const uint8_t* WEBP_RESTRICT v,                          \
                      uint8_t* WEBP_RESTRICT dst, int len) {                   \
  int i;                                                                       \
  uint32_t temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;             \
  const int t_con_1 = 26149;                                                   \
  const int t_con_2 = 13320;                                                   \
  const int t_con_3 = 6419;                                                    \
  const int t_con_4 = 33050;                                                   \
  const int t_con_5 = 19077;                                                   \
  const int t_con_6 = 14234;                                                   \
  const int t_con_7 = 8708;                                                    \
  const int t_con_8 = 17685;                                                   \
  for (i = 0; i < (len >> 1); i++) {                                           \
    __asm__ volatile (                                                         \
      ROW_FUNC_PART_1()                                                        \
      ROW_FUNC_PART_2(R, G, B, 1)                                              \
      ROW_FUNC_PART_2(R + XSTEP, G + XSTEP, B + XSTEP, 0)                      \
      ASM_CLOBBER_LIST()                                                       \
    );                                                                         \
    if (A) dst[A] = dst[A + XSTEP] = 0xff;                                     \
    y += 2;                                                                    \
    ++u;                                                                       \
    ++v;                                                                       \
    dst += 2 * XSTEP;                                                          \
  }                                                                            \
  if (len & 1) {                                                               \
    __asm__ volatile (                                                         \
      ROW_FUNC_PART_1()                                                        \
      ROW_FUNC_PART_2(R, G, B, 0)                                              \
      ASM_CLOBBER_LIST()                                                       \
    );                                                                         \
    if (A) dst[A] = 0xff;                                                      \
  }                                                                            \
}

ROW_FUNC(YuvToRgbRow_MIPSdspR2,      3, 0, 1, 2, 0)
ROW_FUNC(YuvToRgbaRow_MIPSdspR2,     4, 0, 1, 2, 3)
ROW_FUNC(YuvToBgrRow_MIPSdspR2,      3, 2, 1, 0, 0)
ROW_FUNC(YuvToBgraRow_MIPSdspR2,     4, 2, 1, 0, 3)

#undef ROW_FUNC
#undef ASM_CLOBBER_LIST
#undef ROW_FUNC_PART_2
#undef ROW_FUNC_PART_1

//------------------------------------------------------------------------------
// Entry point

extern void WebPInitSamplersMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitSamplersMIPSdspR2(void) {
  WebPSamplers[MODE_RGB]  = YuvToRgbRow_MIPSdspR2;
  WebPSamplers[MODE_RGBA] = YuvToRgbaRow_MIPSdspR2;
  WebPSamplers[MODE_BGR]  = YuvToBgrRow_MIPSdspR2;
  WebPSamplers[MODE_BGRA] = YuvToBgraRow_MIPSdspR2;
}

#else  // !WEBP_USE_MIPS_DSP_R2

WEBP_DSP_INIT_STUB(WebPInitSamplersMIPSdspR2)

#endif  // WEBP_USE_MIPS_DSP_R2
