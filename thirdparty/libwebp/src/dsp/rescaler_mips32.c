// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS version of rescaling functions
//
// Author(s): Djordje Pesut (djordje.pesut@imgtec.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MIPS32) && !defined(WEBP_REDUCE_SIZE)

#include <assert.h>
#include "src/utils/rescaler_utils.h"

//------------------------------------------------------------------------------
// Row import

static void ImportRowShrink_MIPS32(WebPRescaler* WEBP_RESTRICT const wrk,
                                   const uint8_t* WEBP_RESTRICT src) {
  const int x_stride = wrk->num_channels;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const int fx_scale = wrk->fx_scale;
  const int x_add = wrk->x_add;
  const int x_sub = wrk->x_sub;
  const int x_stride1 = x_stride << 2;
  int channel;
  assert(!wrk->x_expand);
  assert(!WebPRescalerInputDone(wrk));

  for (channel = 0; channel < x_stride; ++channel) {
    const uint8_t* src1 = src + channel;
    rescaler_t* frow = wrk->frow + channel;
    int temp1, temp2, temp3;
    int base, frac, sum;
    int accum, accum1;
    int loop_c = x_out_max - channel;

    __asm__ volatile (
      "li     %[temp1],   0x8000                    \n\t"
      "li     %[temp2],   0x10000                   \n\t"
      "li     %[sum],     0                         \n\t"
      "li     %[accum],   0                         \n\t"
    "1:                                             \n\t"
      "addu   %[accum],   %[accum],   %[x_add]      \n\t"
      "li     %[base],    0                         \n\t"
      "blez   %[accum],   3f                        \n\t"
    "2:                                             \n\t"
      "lbu    %[base],    0(%[src1])                \n\t"
      "subu   %[accum],   %[accum],   %[x_sub]      \n\t"
      "addu   %[src1],    %[src1],    %[x_stride]   \n\t"
      "addu   %[sum],     %[sum],     %[base]       \n\t"
      "bgtz   %[accum],   2b                        \n\t"
    "3:                                             \n\t"
      "negu   %[accum1],  %[accum]                  \n\t"
      "mul    %[frac],    %[base],    %[accum1]     \n\t"
      "mul    %[temp3],   %[sum],     %[x_sub]      \n\t"
      "subu   %[loop_c],  %[loop_c],  %[x_stride]   \n\t"
      "mult   %[temp1],   %[temp2]                  \n\t"
      "maddu  %[frac],    %[fx_scale]               \n\t"
      "mfhi   %[sum]                                \n\t"
      "subu   %[temp3],   %[temp3],   %[frac]       \n\t"
      "sw     %[temp3],   0(%[frow])                \n\t"
      "addu   %[frow],    %[frow],    %[x_stride1]  \n\t"
      "bgtz   %[loop_c],  1b                        \n\t"
      : [accum]"=&r"(accum), [src1]"+r"(src1), [temp3]"=&r"(temp3),
        [sum]"=&r"(sum), [base]"=&r"(base), [frac]"=&r"(frac),
        [frow]"+r"(frow), [accum1]"=&r"(accum1),
        [temp2]"=&r"(temp2), [temp1]"=&r"(temp1)
      : [x_stride]"r"(x_stride), [fx_scale]"r"(fx_scale),
        [x_sub]"r"(x_sub), [x_add]"r"(x_add),
        [loop_c]"r"(loop_c), [x_stride1]"r"(x_stride1)
      : "memory", "hi", "lo"
    );
    assert(accum == 0);
  }
}

static void ImportRowExpand_MIPS32(WebPRescaler* WEBP_RESTRICT const wrk,
                                   const uint8_t* WEBP_RESTRICT src) {
  const int x_stride = wrk->num_channels;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const int x_add = wrk->x_add;
  const int x_sub = wrk->x_sub;
  const int src_width = wrk->src_width;
  const int x_stride1 = x_stride << 2;
  int channel;
  assert(wrk->x_expand);
  assert(!WebPRescalerInputDone(wrk));

  for (channel = 0; channel < x_stride; ++channel) {
    const uint8_t* src1 = src + channel;
    rescaler_t* frow = wrk->frow + channel;
    int temp1, temp2, temp3, temp4;
    int frac;
    int accum;
    int x_out = channel;

    __asm__ volatile (
      "addiu  %[temp3],   %[src_width], -1            \n\t"
      "lbu    %[temp2],   0(%[src1])                  \n\t"
      "addu   %[src1],    %[src1],      %[x_stride]   \n\t"
      "bgtz   %[temp3],   0f                          \n\t"
      "addiu  %[temp1],   %[temp2],     0             \n\t"
      "b      3f                                      \n\t"
    "0:                                               \n\t"
      "lbu    %[temp1],   0(%[src1])                  \n\t"
    "3:                                               \n\t"
      "addiu  %[accum],   %[x_add],     0             \n\t"
    "1:                                               \n\t"
      "subu   %[temp3],   %[temp2],     %[temp1]      \n\t"
      "mul    %[temp3],   %[temp3],     %[accum]      \n\t"
      "mul    %[temp4],   %[temp1],     %[x_add]      \n\t"
      "addu   %[temp3],   %[temp4],     %[temp3]      \n\t"
      "sw     %[temp3],   0(%[frow])                  \n\t"
      "addu   %[frow],    %[frow],      %[x_stride1]  \n\t"
      "addu   %[x_out],   %[x_out],     %[x_stride]   \n\t"
      "subu   %[temp3],   %[x_out],     %[x_out_max]  \n\t"
      "bgez   %[temp3],   2f                          \n\t"
      "subu   %[accum],   %[accum],     %[x_sub]      \n\t"
      "bgez   %[accum],   4f                          \n\t"
      "addiu  %[temp2],   %[temp1],     0             \n\t"
      "addu   %[src1],    %[src1],      %[x_stride]   \n\t"
      "lbu    %[temp1],   0(%[src1])                  \n\t"
      "addu   %[accum],   %[accum],     %[x_add]      \n\t"
    "4:                                               \n\t"
      "b      1b                                      \n\t"
    "2:                                               \n\t"
      : [src1]"+r"(src1), [accum]"=&r"(accum), [temp1]"=&r"(temp1),
        [temp2]"=&r"(temp2), [temp3]"=&r"(temp3), [temp4]"=&r"(temp4),
        [x_out]"+r"(x_out), [frac]"=&r"(frac), [frow]"+r"(frow)
      : [x_stride]"r"(x_stride), [x_add]"r"(x_add), [x_sub]"r"(x_sub),
        [x_stride1]"r"(x_stride1), [src_width]"r"(src_width),
        [x_out_max]"r"(x_out_max)
      : "memory", "hi", "lo"
    );
    assert(wrk->x_sub == 0 /* <- special case for src_width=1 */ || accum == 0);
  }
}

//------------------------------------------------------------------------------
// Row export

static void ExportRowExpand_MIPS32(WebPRescaler* const wrk) {
  uint8_t* dst = wrk->dst;
  rescaler_t* irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* frow = wrk->frow;
  int temp0, temp1, temp3, temp4, temp5, loop_end;
  const int temp2 = (int)wrk->fy_scale;
  const int temp6 = x_out_max << 2;
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(wrk->y_expand);
  assert(wrk->y_sub != 0);
  if (wrk->y_accum == 0) {
    __asm__ volatile (
      "li       %[temp3],    0x10000                    \n\t"
      "li       %[temp4],    0x8000                     \n\t"
      "addu     %[loop_end], %[frow],     %[temp6]      \n\t"
    "1:                                                 \n\t"
      "lw       %[temp0],    0(%[frow])                 \n\t"
      "addiu    %[dst],      %[dst],      1             \n\t"
      "addiu    %[frow],     %[frow],     4             \n\t"
      "mult     %[temp3],    %[temp4]                   \n\t"
      "maddu    %[temp0],    %[temp2]                   \n\t"
      "mfhi     %[temp5]                                \n\t"
      "sb       %[temp5],    -1(%[dst])                 \n\t"
      "bne      %[frow],     %[loop_end], 1b            \n\t"
      : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp3]"=&r"(temp3),
        [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [frow]"+r"(frow),
        [dst]"+r"(dst), [loop_end]"=&r"(loop_end)
      : [temp2]"r"(temp2), [temp6]"r"(temp6)
      : "memory", "hi", "lo"
    );
  } else {
    const uint32_t B = WEBP_RESCALER_FRAC(-wrk->y_accum, wrk->y_sub);
    const uint32_t A = (uint32_t)(WEBP_RESCALER_ONE - B);
    __asm__ volatile (
      "li       %[temp3],    0x10000                    \n\t"
      "li       %[temp4],    0x8000                     \n\t"
      "addu     %[loop_end], %[frow],     %[temp6]      \n\t"
    "1:                                                 \n\t"
      "lw       %[temp0],    0(%[frow])                 \n\t"
      "lw       %[temp1],    0(%[irow])                 \n\t"
      "addiu    %[dst],      %[dst],      1             \n\t"
      "mult     %[temp3],    %[temp4]                   \n\t"
      "maddu    %[A],        %[temp0]                   \n\t"
      "maddu    %[B],        %[temp1]                   \n\t"
      "addiu    %[frow],     %[frow],     4             \n\t"
      "addiu    %[irow],     %[irow],     4             \n\t"
      "mfhi     %[temp5]                                \n\t"
      "mult     %[temp3],    %[temp4]                   \n\t"
      "maddu    %[temp5],    %[temp2]                   \n\t"
      "mfhi     %[temp5]                                \n\t"
      "sb       %[temp5],    -1(%[dst])                 \n\t"
      "bne      %[frow],     %[loop_end], 1b            \n\t"
      : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp3]"=&r"(temp3),
        [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [frow]"+r"(frow),
        [irow]"+r"(irow), [dst]"+r"(dst), [loop_end]"=&r"(loop_end)
      : [temp2]"r"(temp2), [temp6]"r"(temp6), [A]"r"(A), [B]"r"(B)
      : "memory", "hi", "lo"
    );
  }
}

#if 0  // disabled for now. TODO(skal): make match the C-code
static void ExportRowShrink_MIPS32(WebPRescaler* const wrk) {
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  uint8_t* dst = wrk->dst;
  rescaler_t* irow = wrk->irow;
  const rescaler_t* frow = wrk->frow;
  const int yscale = wrk->fy_scale * (-wrk->y_accum);
  int temp0, temp1, temp3, temp4, temp5, loop_end;
  const int temp2 = (int)wrk->fxy_scale;
  const int temp6 = x_out_max << 2;

  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(!wrk->y_expand);
  assert(wrk->fxy_scale != 0);
  if (yscale) {
    __asm__ volatile (
      "li       %[temp3],    0x10000                    \n\t"
      "li       %[temp4],    0x8000                     \n\t"
      "addu     %[loop_end], %[frow],     %[temp6]      \n\t"
    "1:                                                 \n\t"
      "lw       %[temp0],    0(%[frow])                 \n\t"
      "mult     %[temp3],    %[temp4]                   \n\t"
      "addiu    %[frow],     %[frow],     4             \n\t"
      "maddu    %[temp0],    %[yscale]                  \n\t"
      "mfhi     %[temp1]                                \n\t"
      "lw       %[temp0],    0(%[irow])                 \n\t"
      "addiu    %[dst],      %[dst],      1             \n\t"
      "addiu    %[irow],     %[irow],     4             \n\t"
      "subu     %[temp0],    %[temp0],    %[temp1]      \n\t"
      "mult     %[temp3],    %[temp4]                   \n\t"
      "maddu    %[temp0],    %[temp2]                   \n\t"
      "mfhi     %[temp5]                                \n\t"
      "sw       %[temp1],    -4(%[irow])                \n\t"
      "sb       %[temp5],    -1(%[dst])                 \n\t"
      "bne      %[frow],     %[loop_end], 1b            \n\t"
      : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp3]"=&r"(temp3),
        [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [frow]"+r"(frow),
        [irow]"+r"(irow), [dst]"+r"(dst), [loop_end]"=&r"(loop_end)
      : [temp2]"r"(temp2), [yscale]"r"(yscale), [temp6]"r"(temp6)
      : "memory", "hi", "lo"
    );
  } else {
    __asm__ volatile (
      "li       %[temp3],    0x10000                    \n\t"
      "li       %[temp4],    0x8000                     \n\t"
      "addu     %[loop_end], %[irow],     %[temp6]      \n\t"
    "1:                                                 \n\t"
      "lw       %[temp0],    0(%[irow])                 \n\t"
      "addiu    %[dst],      %[dst],      1             \n\t"
      "addiu    %[irow],     %[irow],     4             \n\t"
      "mult     %[temp3],    %[temp4]                   \n\t"
      "maddu    %[temp0],    %[temp2]                   \n\t"
      "mfhi     %[temp5]                                \n\t"
      "sw       $zero,       -4(%[irow])                \n\t"
      "sb       %[temp5],    -1(%[dst])                 \n\t"
      "bne      %[irow],     %[loop_end], 1b            \n\t"
      : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp3]"=&r"(temp3),
        [temp4]"=&r"(temp4), [temp5]"=&r"(temp5), [irow]"+r"(irow),
        [dst]"+r"(dst), [loop_end]"=&r"(loop_end)
      : [temp2]"r"(temp2), [temp6]"r"(temp6)
      : "memory", "hi", "lo"
    );
  }
}
#endif  // 0

//------------------------------------------------------------------------------
// Entry point

extern void WebPRescalerDspInitMIPS32(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPRescalerDspInitMIPS32(void) {
  WebPRescalerImportRowExpand = ImportRowExpand_MIPS32;
  WebPRescalerImportRowShrink = ImportRowShrink_MIPS32;
  WebPRescalerExportRowExpand = ExportRowExpand_MIPS32;
//  WebPRescalerExportRowShrink = ExportRowShrink_MIPS32;
}

#else  // !WEBP_USE_MIPS32

WEBP_DSP_INIT_STUB(WebPRescalerDspInitMIPS32)

#endif  // WEBP_USE_MIPS32
