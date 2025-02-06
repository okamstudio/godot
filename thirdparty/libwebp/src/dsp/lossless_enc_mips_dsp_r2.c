// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Image transform methods for lossless encoder.
//
// Author(s):  Djordje Pesut    (djordje.pesut@imgtec.com)
//             Jovan Zelincevic (jovan.zelincevic@imgtec.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MIPS_DSP_R2)

#include "src/dsp/lossless.h"

static void SubtractGreenFromBlueAndRed_MIPSdspR2(uint32_t* argb_data,
                                                  int num_pixels) {
  uint32_t temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
  uint32_t* const p_loop1_end = argb_data + (num_pixels & ~3);
  uint32_t* const p_loop2_end = p_loop1_end + (num_pixels & 3);
  __asm__ volatile (
    ".set       push                                          \n\t"
    ".set       noreorder                                     \n\t"
    "beq        %[argb_data],    %[p_loop1_end],     3f       \n\t"
    " nop                                                     \n\t"
  "0:                                                         \n\t"
    "lw         %[temp0],        0(%[argb_data])              \n\t"
    "lw         %[temp1],        4(%[argb_data])              \n\t"
    "lw         %[temp2],        8(%[argb_data])              \n\t"
    "lw         %[temp3],        12(%[argb_data])             \n\t"
    "ext        %[temp4],        %[temp0],           8,    8  \n\t"
    "ext        %[temp5],        %[temp1],           8,    8  \n\t"
    "ext        %[temp6],        %[temp2],           8,    8  \n\t"
    "ext        %[temp7],        %[temp3],           8,    8  \n\t"
    "addiu      %[argb_data],    %[argb_data],       16       \n\t"
    "replv.ph   %[temp4],        %[temp4]                     \n\t"
    "replv.ph   %[temp5],        %[temp5]                     \n\t"
    "replv.ph   %[temp6],        %[temp6]                     \n\t"
    "replv.ph   %[temp7],        %[temp7]                     \n\t"
    "subu.qb    %[temp0],        %[temp0],           %[temp4] \n\t"
    "subu.qb    %[temp1],        %[temp1],           %[temp5] \n\t"
    "subu.qb    %[temp2],        %[temp2],           %[temp6] \n\t"
    "subu.qb    %[temp3],        %[temp3],           %[temp7] \n\t"
    "sw         %[temp0],        -16(%[argb_data])            \n\t"
    "sw         %[temp1],        -12(%[argb_data])            \n\t"
    "sw         %[temp2],        -8(%[argb_data])             \n\t"
    "bne        %[argb_data],    %[p_loop1_end],     0b       \n\t"
    " sw        %[temp3],        -4(%[argb_data])             \n\t"
  "3:                                                         \n\t"
    "beq        %[argb_data],    %[p_loop2_end],     2f       \n\t"
    " nop                                                     \n\t"
  "1:                                                         \n\t"
    "lw         %[temp0],        0(%[argb_data])              \n\t"
    "addiu      %[argb_data],    %[argb_data],       4        \n\t"
    "ext        %[temp4],        %[temp0],           8,    8  \n\t"
    "replv.ph   %[temp4],        %[temp4]                     \n\t"
    "subu.qb    %[temp0],        %[temp0],           %[temp4] \n\t"
    "bne        %[argb_data],    %[p_loop2_end],     1b       \n\t"
    " sw        %[temp0],        -4(%[argb_data])             \n\t"
  "2:                                                         \n\t"
    ".set       pop                                           \n\t"
    : [argb_data]"+&r"(argb_data), [temp0]"=&r"(temp0),
      [temp1]"=&r"(temp1), [temp2]"=&r"(temp2), [temp3]"=&r"(temp3),
      [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [temp6]"=&r"(temp6),
      [temp7]"=&r"(temp7)
    : [p_loop1_end]"r"(p_loop1_end), [p_loop2_end]"r"(p_loop2_end)
    : "memory"
  );
}

static WEBP_INLINE uint32_t ColorTransformDelta(int8_t color_pred,
                                                int8_t color) {
  return (uint32_t)((int)(color_pred) * color) >> 5;
}

static void TransformColor_MIPSdspR2(
    const VP8LMultipliers* WEBP_RESTRICT const m, uint32_t* WEBP_RESTRICT data,
    int num_pixels) {
  int temp0, temp1, temp2, temp3, temp4, temp5;
  uint32_t argb, argb1, new_red, new_red1;
  const uint32_t G_to_R = m->green_to_red_;
  const uint32_t G_to_B = m->green_to_blue_;
  const uint32_t R_to_B = m->red_to_blue_;
  uint32_t* const p_loop_end = data + (num_pixels & ~1);
  __asm__ volatile (
    ".set            push                                    \n\t"
    ".set            noreorder                               \n\t"
    "beq             %[data],      %[p_loop_end],  1f        \n\t"
    " nop                                                    \n\t"
    "replv.ph        %[temp0],     %[G_to_R]                 \n\t"
    "replv.ph        %[temp1],     %[G_to_B]                 \n\t"
    "replv.ph        %[temp2],     %[R_to_B]                 \n\t"
    "shll.ph         %[temp0],     %[temp0],       8         \n\t"
    "shll.ph         %[temp1],     %[temp1],       8         \n\t"
    "shll.ph         %[temp2],     %[temp2],       8         \n\t"
    "shra.ph         %[temp0],     %[temp0],       8         \n\t"
    "shra.ph         %[temp1],     %[temp1],       8         \n\t"
    "shra.ph         %[temp2],     %[temp2],       8         \n\t"
  "0:                                                        \n\t"
    "lw              %[argb],      0(%[data])                \n\t"
    "lw              %[argb1],     4(%[data])                \n\t"
    "lhu             %[new_red],   2(%[data])                \n\t"
    "lhu             %[new_red1],  6(%[data])                \n\t"
    "precrq.qb.ph    %[temp3],     %[argb],        %[argb1]  \n\t"
    "precr.qb.ph     %[temp4],     %[argb],        %[argb1]  \n\t"
    "preceu.ph.qbra  %[temp3],     %[temp3]                  \n\t"
    "preceu.ph.qbla  %[temp4],     %[temp4]                  \n\t"
    "shll.ph         %[temp3],     %[temp3],       8         \n\t"
    "shll.ph         %[temp4],     %[temp4],       8         \n\t"
    "shra.ph         %[temp3],     %[temp3],       8         \n\t"
    "shra.ph         %[temp4],     %[temp4],       8         \n\t"
    "mul.ph          %[temp5],     %[temp3],       %[temp0]  \n\t"
    "mul.ph          %[temp3],     %[temp3],       %[temp1]  \n\t"
    "mul.ph          %[temp4],     %[temp4],       %[temp2]  \n\t"
    "addiu           %[data],      %[data],        8         \n\t"
    "ins             %[new_red1],  %[new_red],     16,   16  \n\t"
    "ins             %[argb1],     %[argb],        16,   16  \n\t"
    "shra.ph         %[temp5],     %[temp5],       5         \n\t"
    "shra.ph         %[temp3],     %[temp3],       5         \n\t"
    "shra.ph         %[temp4],     %[temp4],       5         \n\t"
    "subu.ph         %[new_red1],  %[new_red1],    %[temp5]  \n\t"
    "subu.ph         %[argb1],     %[argb1],       %[temp3]  \n\t"
    "preceu.ph.qbra  %[temp5],     %[new_red1]               \n\t"
    "subu.ph         %[argb1],     %[argb1],       %[temp4]  \n\t"
    "preceu.ph.qbra  %[temp3],     %[argb1]                  \n\t"
    "sb              %[temp5],     -2(%[data])               \n\t"
    "sb              %[temp3],     -4(%[data])               \n\t"
    "sra             %[temp5],     %[temp5],       16        \n\t"
    "sra             %[temp3],     %[temp3],       16        \n\t"
    "sb              %[temp5],     -6(%[data])               \n\t"
    "bne             %[data],      %[p_loop_end],  0b        \n\t"
    " sb             %[temp3],     -8(%[data])               \n\t"
  "1:                                                        \n\t"
    ".set            pop                                     \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [new_red1]"=&r"(new_red1), [new_red]"=&r"(new_red),
      [argb]"=&r"(argb), [argb1]"=&r"(argb1), [data]"+&r"(data)
    : [G_to_R]"r"(G_to_R), [R_to_B]"r"(R_to_B),
      [G_to_B]"r"(G_to_B), [p_loop_end]"r"(p_loop_end)
    : "memory", "hi", "lo"
  );

  if (num_pixels & 1) {
    const uint32_t argb_ = data[0];
    const uint32_t green = argb_ >> 8;
    const uint32_t red = argb_ >> 16;
    uint32_t new_blue = argb_;
    new_red = red;
    new_red -= ColorTransformDelta(m->green_to_red_, green);
    new_red &= 0xff;
    new_blue -= ColorTransformDelta(m->green_to_blue_, green);
    new_blue -= ColorTransformDelta(m->red_to_blue_, red);
    new_blue &= 0xff;
    data[0] = (argb_ & 0xff00ff00u) | (new_red << 16) | (new_blue);
  }
}

static WEBP_INLINE uint8_t TransformColorBlue(uint8_t green_to_blue,
                                              uint8_t red_to_blue,
                                              uint32_t argb) {
  const uint32_t green = argb >> 8;
  const uint32_t red = argb >> 16;
  uint8_t new_blue = argb;
  new_blue -= ColorTransformDelta(green_to_blue, green);
  new_blue -= ColorTransformDelta(red_to_blue, red);
  return (new_blue & 0xff);
}

static void CollectColorBlueTransforms_MIPSdspR2(
    const uint32_t* WEBP_RESTRICT argb, int stride,
    int tile_width, int tile_height,
    int green_to_blue, int red_to_blue, uint32_t histo[]) {
  const int rtb = (red_to_blue << 16) | (red_to_blue & 0xffff);
  const int gtb = (green_to_blue << 16) | (green_to_blue & 0xffff);
  const uint32_t mask = 0xff00ffu;
  while (tile_height-- > 0) {
    int x;
    const uint32_t* p_argb = argb;
    argb += stride;
    for (x = 0; x < (tile_width >> 1); ++x) {
      int temp0, temp1, temp2, temp3, temp4, temp5, temp6;
      __asm__ volatile (
        "lw           %[temp0],  0(%[p_argb])             \n\t"
        "lw           %[temp1],  4(%[p_argb])             \n\t"
        "precr.qb.ph  %[temp2],  %[temp0],  %[temp1]      \n\t"
        "ins          %[temp1],  %[temp0],  16,    16     \n\t"
        "shra.ph      %[temp2],  %[temp2],  8             \n\t"
        "shra.ph      %[temp3],  %[temp1],  8             \n\t"
        "mul.ph       %[temp5],  %[temp2],  %[rtb]        \n\t"
        "mul.ph       %[temp6],  %[temp3],  %[gtb]        \n\t"
        "and          %[temp4],  %[temp1],  %[mask]       \n\t"
        "addiu        %[p_argb], %[p_argb], 8             \n\t"
        "shra.ph      %[temp5],  %[temp5],  5             \n\t"
        "shra.ph      %[temp6],  %[temp6],  5             \n\t"
        "subu.qb      %[temp2],  %[temp4],  %[temp5]      \n\t"
        "subu.qb      %[temp2],  %[temp2],  %[temp6]      \n\t"
        : [p_argb]"+&r"(p_argb), [temp0]"=&r"(temp0), [temp1]"=&r"(temp1),
          [temp2]"=&r"(temp2), [temp3]"=&r"(temp3), [temp4]"=&r"(temp4),
          [temp5]"=&r"(temp5), [temp6]"=&r"(temp6)
        : [rtb]"r"(rtb), [gtb]"r"(gtb), [mask]"r"(mask)
        : "memory", "hi", "lo"
      );
      ++histo[(uint8_t)(temp2 >> 16)];
      ++histo[(uint8_t)temp2];
    }
    if (tile_width & 1) {
      ++histo[TransformColorBlue(green_to_blue, red_to_blue, *p_argb)];
    }
  }
}

static WEBP_INLINE uint8_t TransformColorRed(uint8_t green_to_red,
                                             uint32_t argb) {
  const uint32_t green = argb >> 8;
  uint32_t new_red = argb >> 16;
  new_red -= ColorTransformDelta(green_to_red, green);
  return (new_red & 0xff);
}

static void CollectColorRedTransforms_MIPSdspR2(
    const uint32_t* WEBP_RESTRICT argb, int stride,
    int tile_width, int tile_height, int green_to_red, uint32_t histo[]) {
  const int gtr = (green_to_red << 16) | (green_to_red & 0xffff);
  while (tile_height-- > 0) {
    int x;
    const uint32_t* p_argb = argb;
    argb += stride;
    for (x = 0; x < (tile_width >> 1); ++x) {
      int temp0, temp1, temp2, temp3, temp4;
      __asm__ volatile (
        "lw           %[temp0],  0(%[p_argb])             \n\t"
        "lw           %[temp1],  4(%[p_argb])             \n\t"
        "precrq.ph.w  %[temp4],  %[temp0],  %[temp1]      \n\t"
        "ins          %[temp1],  %[temp0],  16,    16     \n\t"
        "shra.ph      %[temp3],  %[temp1],  8             \n\t"
        "mul.ph       %[temp2],  %[temp3],  %[gtr]        \n\t"
        "addiu        %[p_argb], %[p_argb], 8             \n\t"
        "shra.ph      %[temp2],  %[temp2],  5             \n\t"
        "subu.qb      %[temp2],  %[temp4],  %[temp2]      \n\t"
        : [p_argb]"+&r"(p_argb), [temp0]"=&r"(temp0), [temp1]"=&r"(temp1),
          [temp2]"=&r"(temp2), [temp3]"=&r"(temp3), [temp4]"=&r"(temp4)
        : [gtr]"r"(gtr)
        : "memory", "hi", "lo"
      );
      ++histo[(uint8_t)(temp2 >> 16)];
      ++histo[(uint8_t)temp2];
    }
    if (tile_width & 1) {
      ++histo[TransformColorRed(green_to_red, *p_argb)];
    }
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8LEncDspInitMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LEncDspInitMIPSdspR2(void) {
  VP8LSubtractGreenFromBlueAndRed = SubtractGreenFromBlueAndRed_MIPSdspR2;
  VP8LTransformColor = TransformColor_MIPSdspR2;
  VP8LCollectColorBlueTransforms = CollectColorBlueTransforms_MIPSdspR2;
  VP8LCollectColorRedTransforms = CollectColorRedTransforms_MIPSdspR2;
}

#else  // !WEBP_USE_MIPS_DSP_R2

WEBP_DSP_INIT_STUB(VP8LEncDspInitMIPSdspR2)

#endif  // WEBP_USE_MIPS_DSP_R2
