// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS version of lossless functions
//
// Author(s):  Djordje Pesut    (djordje.pesut@imgtec.com)
//             Jovan Zelincevic (jovan.zelincevic@imgtec.com)

#include "src/dsp/dsp.h"
#include "src/dsp/lossless.h"
#include "src/dsp/lossless_common.h"

#if defined(WEBP_USE_MIPS32)

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint64_t FastSLog2Slow_MIPS32(uint32_t v) {
  assert(v >= LOG_LOOKUP_IDX_MAX);
  if (v < APPROX_LOG_WITH_CORRECTION_MAX) {
    uint32_t log_cnt, y;
    uint64_t correction;
    const int c24 = 24;
    uint32_t temp;

    // Xf = 256 = 2^8
    // log_cnt is index of leading one in upper 24 bits
    __asm__ volatile(
      "clz      %[log_cnt], %[v]                      \n\t"
      "addiu    %[y],       $zero,        1           \n\t"
      "subu     %[log_cnt], %[c24],       %[log_cnt]  \n\t"
      "sllv     %[y],       %[y],         %[log_cnt]  \n\t"
      "srlv     %[temp],    %[v],         %[log_cnt]  \n\t"
      : [log_cnt]"=&r"(log_cnt), [y]"=&r"(y),
        [temp]"=r"(temp)
      : [c24]"r"(c24), [v]"r"(v)
    );

    // vf = (2^log_cnt) * Xf; where y = 2^log_cnt and Xf < 256
    // Xf = floor(Xf) * (1 + (v % y) / v)
    // log2(Xf) = log2(floor(Xf)) + log2(1 + (v % y) / v)
    // The correction factor: log(1 + d) ~ d; for very small d values, so
    // log2(1 + (v % y) / v) ~ LOG_2_RECIPROCAL * (v % y)/v

    // (v % y) = (v % 2^log_cnt) = v & (2^log_cnt - 1)
    correction = LOG_2_RECIPROCAL_FIXED * (v & (y - 1));
    return (uint64_t)v * (kLog2Table[temp] +
                          ((uint64_t)log_cnt << LOG_2_PRECISION_BITS)) +
           correction;
  } else {
    return (uint64_t)(LOG_2_RECIPROCAL_FIXED_DOUBLE * v * log((double)v) + .5);
  }
}

static uint32_t FastLog2Slow_MIPS32(uint32_t v) {
  assert(v >= LOG_LOOKUP_IDX_MAX);
  if (v < APPROX_LOG_WITH_CORRECTION_MAX) {
    uint32_t log_cnt, y;
    const int c24 = 24;
    uint32_t log_2;
    uint32_t temp;

    __asm__ volatile(
      "clz      %[log_cnt], %[v]                      \n\t"
      "addiu    %[y],       $zero,        1           \n\t"
      "subu     %[log_cnt], %[c24],       %[log_cnt]  \n\t"
      "sllv     %[y],       %[y],         %[log_cnt]  \n\t"
      "srlv     %[temp],    %[v],         %[log_cnt]  \n\t"
      : [log_cnt]"=&r"(log_cnt), [y]"=&r"(y),
        [temp]"=r"(temp)
      : [c24]"r"(c24), [v]"r"(v)
    );

    log_2 = kLog2Table[temp] + (log_cnt << LOG_2_PRECISION_BITS);
    if (v >= APPROX_LOG_MAX) {
      // Since the division is still expensive, add this correction factor only
      // for large values of 'v'.
      const uint64_t correction = LOG_2_RECIPROCAL_FIXED * (v & (y - 1));
      log_2 += (uint32_t)DivRound(correction, v);
    }
    return log_2;
  } else {
    return (uint32_t)(LOG_2_RECIPROCAL_FIXED_DOUBLE * log((double)v) + .5);
  }
}

// C version of this function:
//   int i = 0;
//   int64_t cost = 0;
//   const uint32_t* pop = &population[4];
//   const uint32_t* LoopEnd = &population[length];
//   while (pop != LoopEnd) {
//     ++i;
//     cost += i * *pop;
//     cost += i * *(pop + 1);
//     pop += 2;
//   }
//   return cost;
static uint32_t ExtraCost_MIPS32(const uint32_t* const population, int length) {
  int i, temp0, temp1;
  const uint32_t* pop = &population[4];
  const uint32_t* const LoopEnd = &population[length];

  __asm__ volatile(
    "mult   $zero,    $zero                  \n\t"
    "xor    %[i],     %[i],       %[i]       \n\t"
    "beq    %[pop],   %[LoopEnd], 2f         \n\t"
  "1:                                        \n\t"
    "lw     %[temp0], 0(%[pop])              \n\t"
    "lw     %[temp1], 4(%[pop])              \n\t"
    "addiu  %[i],     %[i],       1          \n\t"
    "addiu  %[pop],   %[pop],     8          \n\t"
    "madd   %[i],     %[temp0]               \n\t"
    "madd   %[i],     %[temp1]               \n\t"
    "bne    %[pop],   %[LoopEnd], 1b         \n\t"
  "2:                                        \n\t"
    "mfhi   %[temp0]                         \n\t"
    "mflo   %[temp1]                         \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1),
      [i]"=&r"(i), [pop]"+r"(pop)
    : [LoopEnd]"r"(LoopEnd)
    : "memory", "hi", "lo"
  );

  return ((int64_t)temp0 << 32 | temp1);
}

// C version of this function:
//   int i = 0;
//   int64_t cost = 0;
//   const uint32_t* pX = &X[4];
//   const uint32_t* pY = &Y[4];
//   const uint32_t* LoopEnd = &X[length];
//   while (pX != LoopEnd) {
//     const uint32_t xy0 = *pX + *pY;
//     const uint32_t xy1 = *(pX + 1) + *(pY + 1);
//     ++i;
//     cost += i * xy0;
//     cost += i * xy1;
//     pX += 2;
//     pY += 2;
//   }
//   return cost;
static uint32_t ExtraCostCombined_MIPS32(const uint32_t* WEBP_RESTRICT const X,
                                         const uint32_t* WEBP_RESTRICT const Y,
                                         int length) {
  int i, temp0, temp1, temp2, temp3;
  const uint32_t* pX = &X[4];
  const uint32_t* pY = &Y[4];
  const uint32_t* const LoopEnd = &X[length];

  __asm__ volatile(
    "mult   $zero,    $zero                  \n\t"
    "xor    %[i],     %[i],       %[i]       \n\t"
    "beq    %[pX],    %[LoopEnd], 2f         \n\t"
  "1:                                        \n\t"
    "lw     %[temp0], 0(%[pX])               \n\t"
    "lw     %[temp1], 0(%[pY])               \n\t"
    "lw     %[temp2], 4(%[pX])               \n\t"
    "lw     %[temp3], 4(%[pY])               \n\t"
    "addiu  %[i],     %[i],       1          \n\t"
    "addu   %[temp0], %[temp0],   %[temp1]   \n\t"
    "addu   %[temp2], %[temp2],   %[temp3]   \n\t"
    "addiu  %[pX],    %[pX],      8          \n\t"
    "addiu  %[pY],    %[pY],      8          \n\t"
    "madd   %[i],     %[temp0]               \n\t"
    "madd   %[i],     %[temp2]               \n\t"
    "bne    %[pX],    %[LoopEnd], 1b         \n\t"
  "2:                                        \n\t"
    "mfhi   %[temp0]                         \n\t"
    "mflo   %[temp1]                         \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1),
      [temp2]"=&r"(temp2), [temp3]"=&r"(temp3),
      [i]"=&r"(i), [pX]"+r"(pX), [pY]"+r"(pY)
    : [LoopEnd]"r"(LoopEnd)
    : "memory", "hi", "lo"
  );

  return ((int64_t)temp0 << 32 | temp1);
}

#define HUFFMAN_COST_PASS                                 \
  __asm__ volatile(                                       \
    "sll   %[temp1],  %[temp0],    3           \n\t"      \
    "addiu %[temp3],  %[streak],   -3          \n\t"      \
    "addu  %[temp2],  %[pstreaks], %[temp1]    \n\t"      \
    "blez  %[temp3],  1f                       \n\t"      \
    "srl   %[temp1],  %[temp1],    1           \n\t"      \
    "addu  %[temp3],  %[pcnts],    %[temp1]    \n\t"      \
    "lw    %[temp0],  4(%[temp2])              \n\t"      \
    "lw    %[temp1],  0(%[temp3])              \n\t"      \
    "addu  %[temp0],  %[temp0],    %[streak]   \n\t"      \
    "addiu %[temp1],  %[temp1],    1           \n\t"      \
    "sw    %[temp0],  4(%[temp2])              \n\t"      \
    "sw    %[temp1],  0(%[temp3])              \n\t"      \
    "b     2f                                  \n\t"      \
  "1:                                          \n\t"      \
    "lw    %[temp0],  0(%[temp2])              \n\t"      \
    "addu  %[temp0],  %[temp0],    %[streak]   \n\t"      \
    "sw    %[temp0],  0(%[temp2])              \n\t"      \
  "2:                                          \n\t"      \
    : [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),           \
      [temp3]"=&r"(temp3), [temp0]"+r"(temp0)             \
    : [pstreaks]"r"(pstreaks), [pcnts]"r"(pcnts),         \
      [streak]"r"(streak)                                 \
    : "memory"                                            \
  );

// Returns the various RLE counts
static WEBP_INLINE void GetEntropyUnrefinedHelper(
    uint32_t val, int i, uint32_t* WEBP_RESTRICT const val_prev,
    int* WEBP_RESTRICT const i_prev,
    VP8LBitEntropy* WEBP_RESTRICT const bit_entropy,
    VP8LStreaks* WEBP_RESTRICT const stats) {
  int* const pstreaks = &stats->streaks[0][0];
  int* const pcnts = &stats->counts[0];
  int temp0, temp1, temp2, temp3;
  const int streak = i - *i_prev;

  // Gather info for the bit entropy.
  if (*val_prev != 0) {
    bit_entropy->sum += (*val_prev) * streak;
    bit_entropy->nonzeros += streak;
    bit_entropy->nonzero_code = *i_prev;
    bit_entropy->entropy += VP8LFastSLog2(*val_prev) * streak;
    if (bit_entropy->max_val < *val_prev) {
      bit_entropy->max_val = *val_prev;
    }
  }

  // Gather info for the Huffman cost.
  temp0 = (*val_prev != 0);
  HUFFMAN_COST_PASS

  *val_prev = val;
  *i_prev = i;
}

static void GetEntropyUnrefined_MIPS32(
    const uint32_t X[], int length,
    VP8LBitEntropy* WEBP_RESTRICT const bit_entropy,
    VP8LStreaks* WEBP_RESTRICT const stats) {
  int i;
  int i_prev = 0;
  uint32_t x_prev = X[0];

  memset(stats, 0, sizeof(*stats));
  VP8LBitEntropyInit(bit_entropy);

  for (i = 1; i < length; ++i) {
    const uint32_t x = X[i];
    if (x != x_prev) {
      GetEntropyUnrefinedHelper(x, i, &x_prev, &i_prev, bit_entropy, stats);
    }
  }
  GetEntropyUnrefinedHelper(0, i, &x_prev, &i_prev, bit_entropy, stats);

  bit_entropy->entropy = VP8LFastSLog2(bit_entropy->sum) - bit_entropy->entropy;
}

static void GetCombinedEntropyUnrefined_MIPS32(
    const uint32_t X[], const uint32_t Y[], int length,
    VP8LBitEntropy* WEBP_RESTRICT const entropy,
    VP8LStreaks* WEBP_RESTRICT const stats) {
  int i = 1;
  int i_prev = 0;
  uint32_t xy_prev = X[0] + Y[0];

  memset(stats, 0, sizeof(*stats));
  VP8LBitEntropyInit(entropy);

  for (i = 1; i < length; ++i) {
    const uint32_t xy = X[i] + Y[i];
    if (xy != xy_prev) {
      GetEntropyUnrefinedHelper(xy, i, &xy_prev, &i_prev, entropy, stats);
    }
  }
  GetEntropyUnrefinedHelper(0, i, &xy_prev, &i_prev, entropy, stats);

  entropy->entropy = VP8LFastSLog2(entropy->sum) - entropy->entropy;
}

#define ASM_START                                       \
  __asm__ volatile(                                     \
    ".set   push                            \n\t"       \
    ".set   at                              \n\t"       \
    ".set   macro                           \n\t"       \
  "1:                                       \n\t"

// P2 = P0 + P1
// A..D - offsets
// E - temp variable to tell macro
//     if pointer should be incremented
// literal_ and successive histograms could be unaligned
// so we must use ulw and usw
#define ADD_TO_OUT(A, B, C, D, E, P0, P1, P2)           \
    "ulw    %[temp0], " #A "(%[" #P0 "])    \n\t"       \
    "ulw    %[temp1], " #B "(%[" #P0 "])    \n\t"       \
    "ulw    %[temp2], " #C "(%[" #P0 "])    \n\t"       \
    "ulw    %[temp3], " #D "(%[" #P0 "])    \n\t"       \
    "ulw    %[temp4], " #A "(%[" #P1 "])    \n\t"       \
    "ulw    %[temp5], " #B "(%[" #P1 "])    \n\t"       \
    "ulw    %[temp6], " #C "(%[" #P1 "])    \n\t"       \
    "ulw    %[temp7], " #D "(%[" #P1 "])    \n\t"       \
    "addu   %[temp4], %[temp4],   %[temp0]  \n\t"       \
    "addu   %[temp5], %[temp5],   %[temp1]  \n\t"       \
    "addu   %[temp6], %[temp6],   %[temp2]  \n\t"       \
    "addu   %[temp7], %[temp7],   %[temp3]  \n\t"       \
    "addiu  %[" #P0 "],  %[" #P0 "],  16    \n\t"       \
  ".if " #E " == 1                          \n\t"       \
    "addiu  %[" #P1 "],  %[" #P1 "],  16    \n\t"       \
  ".endif                                   \n\t"       \
    "usw    %[temp4], " #A "(%[" #P2 "])    \n\t"       \
    "usw    %[temp5], " #B "(%[" #P2 "])    \n\t"       \
    "usw    %[temp6], " #C "(%[" #P2 "])    \n\t"       \
    "usw    %[temp7], " #D "(%[" #P2 "])    \n\t"       \
    "addiu  %[" #P2 "], %[" #P2 "],   16    \n\t"       \
    "bne    %[" #P0 "], %[LoopEnd], 1b      \n\t"       \
    ".set   pop                             \n\t"       \

#define ASM_END_COMMON_0                                \
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1),         \
      [temp2]"=&r"(temp2), [temp3]"=&r"(temp3),         \
      [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),         \
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7),         \
      [pa]"+r"(pa), [pout]"+r"(pout)

#define ASM_END_COMMON_1                                \
    : [LoopEnd]"r"(LoopEnd)                             \
    : "memory", "at"                                    \
  );

#define ASM_END_0                                       \
    ASM_END_COMMON_0                                    \
      , [pb]"+r"(pb)                                    \
    ASM_END_COMMON_1

#define ASM_END_1                                       \
    ASM_END_COMMON_0                                    \
    ASM_END_COMMON_1

static void AddVector_MIPS32(const uint32_t* WEBP_RESTRICT pa,
                             const uint32_t* WEBP_RESTRICT pb,
                             uint32_t* WEBP_RESTRICT pout, int size) {
  uint32_t temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
  const int end = ((size) / 4) * 4;
  const uint32_t* const LoopEnd = pa + end;
  int i;
  ASM_START
  ADD_TO_OUT(0, 4, 8, 12, 1, pa, pb, pout)
  ASM_END_0
  for (i = 0; i < size - end; ++i) pout[i] = pa[i] + pb[i];
}

static void AddVectorEq_MIPS32(const uint32_t* WEBP_RESTRICT pa,
                               uint32_t* WEBP_RESTRICT pout, int size) {
  uint32_t temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
  const int end = ((size) / 4) * 4;
  const uint32_t* const LoopEnd = pa + end;
  int i;
  ASM_START
  ADD_TO_OUT(0, 4, 8, 12, 0, pa, pout, pout)
  ASM_END_1
  for (i = 0; i < size - end; ++i) pout[i] += pa[i];
}

#undef ASM_END_1
#undef ASM_END_0
#undef ASM_END_COMMON_1
#undef ASM_END_COMMON_0
#undef ADD_TO_OUT
#undef ASM_START

//------------------------------------------------------------------------------
// Entry point

extern void VP8LEncDspInitMIPS32(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LEncDspInitMIPS32(void) {
  VP8LFastSLog2Slow = FastSLog2Slow_MIPS32;
  VP8LFastLog2Slow = FastLog2Slow_MIPS32;
  VP8LExtraCost = ExtraCost_MIPS32;
  VP8LExtraCostCombined = ExtraCostCombined_MIPS32;
  VP8LGetEntropyUnrefined = GetEntropyUnrefined_MIPS32;
  VP8LGetCombinedEntropyUnrefined = GetCombinedEntropyUnrefined_MIPS32;
  VP8LAddVector = AddVector_MIPS32;
  VP8LAddVectorEq = AddVectorEq_MIPS32;
}

#else  // !WEBP_USE_MIPS32

WEBP_DSP_INIT_STUB(VP8LEncDspInitMIPS32)

#endif  // WEBP_USE_MIPS32
