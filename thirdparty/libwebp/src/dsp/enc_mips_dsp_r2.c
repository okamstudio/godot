// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MIPS version of speed-critical encoding functions.
//
// Author(s): Darko Laus (darko.laus@imgtec.com)
//            Mirko Raus (mirko.raus@imgtec.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MIPS_DSP_R2)

#include "src/dsp/mips_macro.h"
#include "src/enc/cost_enc.h"
#include "src/enc/vp8i_enc.h"

static const int kC1 = WEBP_TRANSFORM_AC3_C1;
static const int kC2 = WEBP_TRANSFORM_AC3_C2;

// O - output
// I - input (macro doesn't change it)
#define ADD_SUB_HALVES_X4(O0, O1, O2, O3, O4, O5, O6, O7,                      \
                          I0, I1, I2, I3, I4, I5, I6, I7)                      \
  "addq.ph          %[" #O0 "],   %[" #I0 "],  %[" #I1 "]     \n\t"            \
  "subq.ph          %[" #O1 "],   %[" #I0 "],  %[" #I1 "]     \n\t"            \
  "addq.ph          %[" #O2 "],   %[" #I2 "],  %[" #I3 "]     \n\t"            \
  "subq.ph          %[" #O3 "],   %[" #I2 "],  %[" #I3 "]     \n\t"            \
  "addq.ph          %[" #O4 "],   %[" #I4 "],  %[" #I5 "]     \n\t"            \
  "subq.ph          %[" #O5 "],   %[" #I4 "],  %[" #I5 "]     \n\t"            \
  "addq.ph          %[" #O6 "],   %[" #I6 "],  %[" #I7 "]     \n\t"            \
  "subq.ph          %[" #O7 "],   %[" #I6 "],  %[" #I7 "]     \n\t"

// IO - input/output
#define ABS_X8(IO0, IO1, IO2, IO3, IO4, IO5, IO6, IO7)                         \
  "absq_s.ph        %[" #IO0 "],   %[" #IO0 "]                \n\t"            \
  "absq_s.ph        %[" #IO1 "],   %[" #IO1 "]                \n\t"            \
  "absq_s.ph        %[" #IO2 "],   %[" #IO2 "]                \n\t"            \
  "absq_s.ph        %[" #IO3 "],   %[" #IO3 "]                \n\t"            \
  "absq_s.ph        %[" #IO4 "],   %[" #IO4 "]                \n\t"            \
  "absq_s.ph        %[" #IO5 "],   %[" #IO5 "]                \n\t"            \
  "absq_s.ph        %[" #IO6 "],   %[" #IO6 "]                \n\t"            \
  "absq_s.ph        %[" #IO7 "],   %[" #IO7 "]                \n\t"

// dpa.w.ph $ac0 temp0 ,temp1
//  $ac += temp0[31..16] * temp1[31..16] + temp0[15..0] * temp1[15..0]
// dpax.w.ph $ac0 temp0 ,temp1
//  $ac += temp0[31..16] * temp1[15..0] + temp0[15..0] * temp1[31..16]
// O - output
// I - input (macro doesn't change it)
#define MUL_HALF(O0, I0, I1, I2, I3, I4, I5, I6, I7,                           \
                 I8, I9, I10, I11, I12, I13, I14, I15)                         \
    "mult            $ac0,      $zero,     $zero              \n\t"            \
    "dpa.w.ph        $ac0,      %[" #I2 "],  %[" #I0 "]       \n\t"            \
    "dpax.w.ph       $ac0,      %[" #I5 "],  %[" #I6 "]       \n\t"            \
    "dpa.w.ph        $ac0,      %[" #I8 "],  %[" #I9 "]       \n\t"            \
    "dpax.w.ph       $ac0,      %[" #I11 "], %[" #I4 "]       \n\t"            \
    "dpa.w.ph        $ac0,      %[" #I12 "], %[" #I7 "]       \n\t"            \
    "dpax.w.ph       $ac0,      %[" #I13 "], %[" #I1 "]       \n\t"            \
    "dpa.w.ph        $ac0,      %[" #I14 "], %[" #I3 "]       \n\t"            \
    "dpax.w.ph       $ac0,      %[" #I15 "], %[" #I10 "]      \n\t"            \
    "mflo            %[" #O0 "],  $ac0                        \n\t"

#define OUTPUT_EARLY_CLOBBER_REGS_17()                                         \
  OUTPUT_EARLY_CLOBBER_REGS_10(),                                              \
  [temp11]"=&r"(temp11), [temp12]"=&r"(temp12), [temp13]"=&r"(temp13),         \
  [temp14]"=&r"(temp14), [temp15]"=&r"(temp15), [temp16]"=&r"(temp16),         \
  [temp17]"=&r"(temp17)

// macro for one horizontal pass in FTransform
// temp0..temp15 holds tmp[0]..tmp[15]
// A - offset in bytes to load from src and ref buffers
// TEMP0..TEMP3 - registers for corresponding tmp elements
#define HORIZONTAL_PASS(A, TEMP0, TEMP1, TEMP2, TEMP3)                         \
  "lw              %[" #TEMP0 "],   0(%[args])                          \n\t"  \
  "lw              %[" #TEMP1 "],   4(%[args])                          \n\t"  \
  "lw              %[" #TEMP2 "],   " XSTR(BPS) "*" #A "(%[" #TEMP0 "]) \n\t"  \
  "lw              %[" #TEMP3 "],   " XSTR(BPS) "*" #A "(%[" #TEMP1 "]) \n\t"  \
  "preceu.ph.qbl   %[" #TEMP0 "],   %[" #TEMP2 "]                       \n\t"  \
  "preceu.ph.qbl   %[" #TEMP1 "],   %[" #TEMP3 "]                       \n\t"  \
  "preceu.ph.qbr   %[" #TEMP2 "],   %[" #TEMP2 "]                       \n\t"  \
  "preceu.ph.qbr   %[" #TEMP3 "],   %[" #TEMP3 "]                       \n\t"  \
  "subq.ph         %[" #TEMP0 "],   %[" #TEMP0 "],   %[" #TEMP1 "]      \n\t"  \
  "subq.ph         %[" #TEMP2 "],   %[" #TEMP2 "],   %[" #TEMP3 "]      \n\t"  \
  "rotr            %[" #TEMP0 "],   %[" #TEMP0 "],   16                 \n\t"  \
  "addq.ph         %[" #TEMP1 "],   %[" #TEMP2 "],   %[" #TEMP0 "]      \n\t"  \
  "subq.ph         %[" #TEMP3 "],   %[" #TEMP2 "],   %[" #TEMP0 "]      \n\t"  \
  "seh             %[" #TEMP0 "],   %[" #TEMP1 "]                       \n\t"  \
  "sra             %[temp16],     %[" #TEMP1 "],   16                   \n\t"  \
  "seh             %[temp19],     %[" #TEMP3 "]                         \n\t"  \
  "sra             %[" #TEMP3 "],   %[" #TEMP3 "],   16                 \n\t"  \
  "subu            %[" #TEMP2 "],   %[" #TEMP0 "],   %[temp16]          \n\t"  \
  "addu            %[" #TEMP0 "],   %[" #TEMP0 "],   %[temp16]          \n\t"  \
  "mul             %[temp17],     %[temp19],     %[c2217]               \n\t"  \
  "mul             %[temp18],     %[" #TEMP3 "],   %[c5352]             \n\t"  \
  "mul             %[" #TEMP1 "],   %[temp19],     %[c5352]             \n\t"  \
  "mul             %[temp16],     %[" #TEMP3 "],   %[c2217]             \n\t"  \
  "sll             %[" #TEMP2 "],   %[" #TEMP2 "],   3                  \n\t"  \
  "sll             %[" #TEMP0 "],   %[" #TEMP0 "],   3                  \n\t"  \
  "subu            %[" #TEMP3 "],   %[temp17],     %[temp18]            \n\t"  \
  "addu            %[" #TEMP1 "],   %[temp16],     %[" #TEMP1 "]        \n\t"  \
  "addiu           %[" #TEMP3 "],   %[" #TEMP3 "],   937                \n\t"  \
  "addiu           %[" #TEMP1 "],   %[" #TEMP1 "],   1812               \n\t"  \
  "sra             %[" #TEMP3 "],   %[" #TEMP3 "],   9                  \n\t"  \
  "sra             %[" #TEMP1 "],   %[" #TEMP1 "],   9                  \n\t"

// macro for one vertical pass in FTransform
// temp0..temp15 holds tmp[0]..tmp[15]
// A..D - offsets in bytes to store to out buffer
// TEMP0, TEMP4, TEMP8 and TEMP12 - registers for corresponding tmp elements
#define VERTICAL_PASS(A, B, C, D, TEMP0, TEMP4, TEMP8, TEMP12)                 \
  "addu            %[temp16],     %[" #TEMP0 "],   %[" #TEMP12 "]   \n\t"      \
  "subu            %[temp19],     %[" #TEMP0 "],   %[" #TEMP12 "]   \n\t"      \
  "addu            %[temp17],     %[" #TEMP4 "],   %[" #TEMP8 "]    \n\t"      \
  "subu            %[temp18],     %[" #TEMP4 "],   %[" #TEMP8 "]    \n\t"      \
  "mul             %[" #TEMP8 "],   %[temp19],     %[c2217]         \n\t"      \
  "mul             %[" #TEMP12 "],  %[temp18],     %[c2217]         \n\t"      \
  "mul             %[" #TEMP4 "],   %[temp19],     %[c5352]         \n\t"      \
  "mul             %[temp18],     %[temp18],     %[c5352]           \n\t"      \
  "addiu           %[temp16],     %[temp16],     7                  \n\t"      \
  "addu            %[" #TEMP0 "],   %[temp16],     %[temp17]        \n\t"      \
  "sra             %[" #TEMP0 "],   %[" #TEMP0 "],   4              \n\t"      \
  "addu            %[" #TEMP12 "],  %[" #TEMP12 "],  %[" #TEMP4 "]  \n\t"      \
  "subu            %[" #TEMP4 "],   %[temp16],     %[temp17]        \n\t"      \
  "sra             %[" #TEMP4 "],   %[" #TEMP4 "],   4              \n\t"      \
  "addiu           %[" #TEMP8 "],   %[" #TEMP8 "],   30000          \n\t"      \
  "addiu           %[" #TEMP12 "],  %[" #TEMP12 "],  12000          \n\t"      \
  "addiu           %[" #TEMP8 "],   %[" #TEMP8 "],   21000          \n\t"      \
  "subu            %[" #TEMP8 "],   %[" #TEMP8 "],   %[temp18]      \n\t"      \
  "sra             %[" #TEMP12 "],  %[" #TEMP12 "],  16             \n\t"      \
  "sra             %[" #TEMP8 "],   %[" #TEMP8 "],   16             \n\t"      \
  "addiu           %[temp16],     %[" #TEMP12 "],  1                \n\t"      \
  "movn            %[" #TEMP12 "],  %[temp16],     %[temp19]        \n\t"      \
  "sh              %[" #TEMP0 "],   " #A "(%[temp20])               \n\t"      \
  "sh              %[" #TEMP4 "],   " #C "(%[temp20])               \n\t"      \
  "sh              %[" #TEMP8 "],   " #D "(%[temp20])               \n\t"      \
  "sh              %[" #TEMP12 "],  " #B "(%[temp20])               \n\t"

static void FTransform_MIPSdspR2(const uint8_t* WEBP_RESTRICT src,
                                 const uint8_t* WEBP_RESTRICT ref,
                                 int16_t* WEBP_RESTRICT out) {
  const int c2217 = 2217;
  const int c5352 = 5352;
  int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8;
  int temp9, temp10, temp11, temp12, temp13, temp14, temp15, temp16;
  int temp17, temp18, temp19, temp20;
  const int* const args[3] =
      { (const int*)src, (const int*)ref, (const int*)out };

  __asm__ volatile (
    HORIZONTAL_PASS(0, temp0,  temp1,  temp2,  temp3)
    HORIZONTAL_PASS(1, temp4,  temp5,  temp6,  temp7)
    HORIZONTAL_PASS(2, temp8,  temp9,  temp10, temp11)
    HORIZONTAL_PASS(3, temp12, temp13, temp14, temp15)
    "lw            %[temp20],     8(%[args])                  \n\t"
    VERTICAL_PASS(0,  8, 16, 24, temp0, temp4, temp8,  temp12)
    VERTICAL_PASS(2, 10, 18, 26, temp1, temp5, temp9,  temp13)
    VERTICAL_PASS(4, 12, 20, 28, temp2, temp6, temp10, temp14)
    VERTICAL_PASS(6, 14, 22, 30, temp3, temp7, temp11, temp15)
    OUTPUT_EARLY_CLOBBER_REGS_18(),
      [temp0]"=&r"(temp0), [temp19]"=&r"(temp19), [temp20]"=&r"(temp20)
    : [args]"r"(args), [c2217]"r"(c2217), [c5352]"r"(c5352)
    : "memory", "hi", "lo"
  );
}

#undef VERTICAL_PASS
#undef HORIZONTAL_PASS

static WEBP_INLINE void ITransformOne(const uint8_t* WEBP_RESTRICT ref,
                                      const int16_t* WEBP_RESTRICT in,
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
    LOAD_WITH_OFFSET_X4(temp10, temp11, temp14, temp15, ref,
                        0, 0, 0, 0,
                        0, 1, 2, 3,
                        BPS)
    CONVERT_2_BYTES_TO_HALF(temp5, temp6, temp7, temp8, temp17, temp18, temp10,
                            temp11, temp10, temp11, temp14, temp15)
    STORE_SAT_SUM_X2(temp5, temp6, temp7, temp8, temp17, temp18, temp10, temp11,
                     temp9, temp12, temp1, temp2, temp13, temp16, temp3, temp4,
                     dst, 0, 1, 2, 3, BPS)

    OUTPUT_EARLY_CLOBBER_REGS_18()
    : [dst]"r"(dst), [in]"r"(in), [kC1]"r"(kC1), [kC2]"r"(kC2), [ref]"r"(ref)
    : "memory", "hi", "lo"
  );
}

static void ITransform_MIPSdspR2(const uint8_t* WEBP_RESTRICT ref,
                                 const int16_t* WEBP_RESTRICT in,
                                 uint8_t* WEBP_RESTRICT dst, int do_two) {
  ITransformOne(ref, in, dst);
  if (do_two) {
    ITransformOne(ref + 4, in + 16, dst + 4);
  }
}

static int Disto4x4_MIPSdspR2(const uint8_t* WEBP_RESTRICT const a,
                              const uint8_t* WEBP_RESTRICT const b,
                              const uint16_t* WEBP_RESTRICT const w) {
  int temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8, temp9;
  int temp10, temp11, temp12, temp13, temp14, temp15, temp16, temp17;

  __asm__ volatile (
    LOAD_WITH_OFFSET_X4(temp1, temp2, temp3, temp4, a,
                        0, 0, 0, 0,
                        0, 1, 2, 3,
                        BPS)
    CONVERT_2_BYTES_TO_HALF(temp5, temp6, temp7, temp8, temp9,temp10, temp11,
                            temp12, temp1, temp2, temp3, temp4)
    ADD_SUB_HALVES_X4(temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8,
                      temp5, temp6, temp7, temp8, temp9, temp10, temp11, temp12)
    PACK_2_HALVES_TO_WORD(temp9, temp10, temp11, temp12, temp1, temp3, temp5,
                          temp7, temp2, temp4, temp6, temp8)
    ADD_SUB_HALVES_X4(temp2, temp4, temp6, temp8, temp9, temp1, temp3, temp10,
                      temp1, temp9, temp3, temp10, temp5, temp11, temp7, temp12)
    ADD_SUB_HALVES_X4(temp5, temp11, temp7, temp2, temp9, temp3, temp6, temp12,
                      temp2, temp9, temp6, temp3, temp4, temp1, temp8, temp10)
    ADD_SUB_HALVES_X4(temp1, temp4, temp10, temp8, temp7, temp11, temp5, temp2,
                      temp5, temp7, temp11, temp2, temp9, temp6, temp3, temp12)
    ABS_X8(temp1, temp4, temp10, temp8, temp7, temp11, temp5, temp2)
    LOAD_WITH_OFFSET_X4(temp3, temp6, temp9, temp12, w,
                        0, 4, 8, 12,
                        0, 0, 0, 0,
                        0)
    LOAD_WITH_OFFSET_X4(temp13, temp14, temp15, temp16, w,
                        0, 4, 8, 12,
                        1, 1, 1, 1,
                        16)
    MUL_HALF(temp17, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8,
             temp9, temp10, temp11, temp12, temp13, temp14, temp15, temp16)
    LOAD_WITH_OFFSET_X4(temp1, temp2, temp3, temp4, b,
                        0, 0, 0, 0,
                        0, 1, 2, 3,
                        BPS)
    CONVERT_2_BYTES_TO_HALF(temp5,temp6, temp7, temp8, temp9,temp10, temp11,
                            temp12, temp1, temp2, temp3, temp4)
    ADD_SUB_HALVES_X4(temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8,
                      temp5, temp6, temp7, temp8, temp9, temp10, temp11, temp12)
    PACK_2_HALVES_TO_WORD(temp9, temp10, temp11, temp12, temp1, temp3, temp5,
                          temp7, temp2, temp4, temp6, temp8)
    ADD_SUB_HALVES_X4(temp2, temp4, temp6, temp8, temp9, temp1, temp3, temp10,
                      temp1, temp9, temp3, temp10, temp5, temp11, temp7, temp12)
    ADD_SUB_HALVES_X4(temp5, temp11, temp7, temp2, temp9, temp3, temp6, temp12,
                      temp2, temp9, temp6, temp3, temp4, temp1, temp8, temp10)
    ADD_SUB_HALVES_X4(temp1, temp4, temp10, temp8, temp7, temp11, temp5, temp2,
                      temp5, temp7, temp11, temp2, temp9, temp6, temp3, temp12)
    ABS_X8(temp1, temp4, temp10, temp8, temp7, temp11, temp5, temp2)
    LOAD_WITH_OFFSET_X4(temp3, temp6, temp9, temp12, w,
                        0, 4, 8, 12,
                        0, 0, 0, 0,
                        0)
    LOAD_WITH_OFFSET_X4(temp13, temp14, temp15, temp16, w,
                        0, 4, 8, 12,
                        1, 1, 1, 1,
                        16)
    MUL_HALF(temp3, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8,
             temp9, temp10, temp11, temp12, temp13, temp14, temp15, temp16)
    OUTPUT_EARLY_CLOBBER_REGS_17()
    : [a]"r"(a), [b]"r"(b), [w]"r"(w)
    : "memory", "hi", "lo"
  );
  return abs(temp3 - temp17) >> 5;
}

static int Disto16x16_MIPSdspR2(const uint8_t* WEBP_RESTRICT const a,
                                const uint8_t* WEBP_RESTRICT const b,
                                const uint16_t* WEBP_RESTRICT const w) {
  int D = 0;
  int x, y;
  for (y = 0; y < 16 * BPS; y += 4 * BPS) {
    for (x = 0; x < 16; x += 4) {
      D += Disto4x4_MIPSdspR2(a + x + y, b + x + y, w);
    }
  }
  return D;
}

//------------------------------------------------------------------------------
// Intra predictions

#define FILL_PART(J, SIZE)                                            \
    "usw        %[value],  0+" #J "*" XSTR(BPS) "(%[dst])  \n\t"      \
    "usw        %[value],  4+" #J "*" XSTR(BPS) "(%[dst])  \n\t"      \
  ".if " #SIZE " == 16                                     \n\t"      \
    "usw        %[value],  8+" #J "*" XSTR(BPS) "(%[dst])  \n\t"      \
    "usw        %[value], 12+" #J "*" XSTR(BPS) "(%[dst])  \n\t"      \
  ".endif                                                  \n\t"

#define FILL_8_OR_16(DST, VALUE, SIZE) do {                         \
  int value = (VALUE);                                              \
  __asm__ volatile (                                                \
    "replv.qb   %[value],  %[value]                      \n\t"      \
    FILL_PART( 0, SIZE)                                             \
    FILL_PART( 1, SIZE)                                             \
    FILL_PART( 2, SIZE)                                             \
    FILL_PART( 3, SIZE)                                             \
    FILL_PART( 4, SIZE)                                             \
    FILL_PART( 5, SIZE)                                             \
    FILL_PART( 6, SIZE)                                             \
    FILL_PART( 7, SIZE)                                             \
  ".if " #SIZE " == 16                                   \n\t"      \
    FILL_PART( 8, 16)                                               \
    FILL_PART( 9, 16)                                               \
    FILL_PART(10, 16)                                               \
    FILL_PART(11, 16)                                               \
    FILL_PART(12, 16)                                               \
    FILL_PART(13, 16)                                               \
    FILL_PART(14, 16)                                               \
    FILL_PART(15, 16)                                               \
  ".endif                                                \n\t"      \
    : [value]"+&r"(value)                                           \
    : [dst]"r"((DST))                                               \
    : "memory"                                                      \
  );                                                                \
} while (0)

#define VERTICAL_PRED(DST, TOP, SIZE)                                          \
static WEBP_INLINE void VerticalPred##SIZE(                                    \
    uint8_t* WEBP_RESTRICT (DST), const uint8_t* WEBP_RESTRICT (TOP)) {        \
  int j;                                                                       \
  if ((TOP)) {                                                                 \
    for (j = 0; j < (SIZE); ++j) memcpy((DST) + j * BPS, (TOP), (SIZE));       \
  } else {                                                                     \
    FILL_8_OR_16((DST), 127, (SIZE));                                          \
  }                                                                            \
}

VERTICAL_PRED(dst, top, 8)
VERTICAL_PRED(dst, top, 16)

#undef VERTICAL_PRED

#define HORIZONTAL_PRED(DST, LEFT, SIZE)                                       \
static WEBP_INLINE void HorizontalPred##SIZE(                                  \
    uint8_t* WEBP_RESTRICT (DST), const uint8_t* WEBP_RESTRICT (LEFT)) {       \
  if (LEFT) {                                                                  \
    int j;                                                                     \
    for (j = 0; j < (SIZE); ++j) {                                             \
      memset((DST) + j * BPS, (LEFT)[j], (SIZE));                              \
    }                                                                          \
  } else {                                                                     \
    FILL_8_OR_16((DST), 129, (SIZE));                                          \
  }                                                                            \
}

HORIZONTAL_PRED(dst, left, 8)
HORIZONTAL_PRED(dst, left, 16)

#undef HORIZONTAL_PRED

#define CLIPPING()                                                             \
  "preceu.ph.qbl   %[temp2],   %[temp0]                  \n\t"                 \
  "preceu.ph.qbr   %[temp0],   %[temp0]                  \n\t"                 \
  "preceu.ph.qbl   %[temp3],   %[temp1]                  \n\t"                 \
  "preceu.ph.qbr   %[temp1],   %[temp1]                  \n\t"                 \
  "addu.ph         %[temp2],   %[temp2],   %[leftY_1]    \n\t"                 \
  "addu.ph         %[temp0],   %[temp0],   %[leftY_1]    \n\t"                 \
  "addu.ph         %[temp3],   %[temp3],   %[leftY_1]    \n\t"                 \
  "addu.ph         %[temp1],   %[temp1],   %[leftY_1]    \n\t"                 \
  "shll_s.ph       %[temp2],   %[temp2],   7             \n\t"                 \
  "shll_s.ph       %[temp0],   %[temp0],   7             \n\t"                 \
  "shll_s.ph       %[temp3],   %[temp3],   7             \n\t"                 \
  "shll_s.ph       %[temp1],   %[temp1],   7             \n\t"                 \
  "precrqu_s.qb.ph %[temp0],   %[temp2],   %[temp0]      \n\t"                 \
  "precrqu_s.qb.ph %[temp1],   %[temp3],   %[temp1]      \n\t"

#define CLIP_8B_TO_DST(DST, LEFT, TOP, SIZE) do {                              \
  int leftY_1 = ((int)(LEFT)[y] << 16) + (LEFT)[y];                            \
  int temp0, temp1, temp2, temp3;                                              \
  __asm__ volatile (                                                           \
    "replv.ph        %[leftY_1], %[leftY_1]              \n\t"                 \
    "ulw             %[temp0],   0(%[top])               \n\t"                 \
    "ulw             %[temp1],   4(%[top])               \n\t"                 \
    "subu.ph         %[leftY_1], %[leftY_1], %[left_1]   \n\t"                 \
    CLIPPING()                                                                 \
    "usw             %[temp0],   0(%[dst])               \n\t"                 \
    "usw             %[temp1],   4(%[dst])               \n\t"                 \
  ".if " #SIZE " == 16                                   \n\t"                 \
    "ulw             %[temp0],   8(%[top])               \n\t"                 \
    "ulw             %[temp1],   12(%[top])              \n\t"                 \
    CLIPPING()                                                                 \
    "usw             %[temp0],   8(%[dst])               \n\t"                 \
    "usw             %[temp1],   12(%[dst])              \n\t"                 \
  ".endif                                                \n\t"                 \
    : [leftY_1]"+&r"(leftY_1), [temp0]"=&r"(temp0), [temp1]"=&r"(temp1),       \
      [temp2]"=&r"(temp2), [temp3]"=&r"(temp3)                                 \
    : [left_1]"r"(left_1), [top]"r"((TOP)), [dst]"r"((DST))                    \
    : "memory"                                                                 \
  );                                                                           \
} while (0)

#define CLIP_TO_DST(DST, LEFT, TOP, SIZE) do {                                 \
  int y;                                                                       \
  const int left_1 = ((int)(LEFT)[-1] << 16) + (LEFT)[-1];                     \
  for (y = 0; y < (SIZE); ++y) {                                               \
    CLIP_8B_TO_DST((DST), (LEFT), (TOP), (SIZE));                              \
    (DST) += BPS;                                                              \
  }                                                                            \
} while (0)

#define TRUE_MOTION(DST, LEFT, TOP, SIZE)                                      \
static WEBP_INLINE void TrueMotion##SIZE(uint8_t* WEBP_RESTRICT (DST),         \
                                         const uint8_t* WEBP_RESTRICT (LEFT),  \
                                         const uint8_t* WEBP_RESTRICT (TOP)) { \
  if ((LEFT) != NULL) {                                                        \
    if ((TOP) != NULL) {                                                       \
      CLIP_TO_DST((DST), (LEFT), (TOP), (SIZE));                               \
    } else {                                                                   \
      HorizontalPred##SIZE((DST), (LEFT));                                     \
    }                                                                          \
  } else {                                                                     \
    /* true motion without left samples (hence: with default 129 value)    */  \
    /* is equivalent to VE prediction where you just copy the top samples. */  \
    /* Note that if top samples are not available, the default value is    */  \
    /* then 129, and not 127 as in the VerticalPred case.                  */  \
    if ((TOP) != NULL) {                                                       \
      VerticalPred##SIZE((DST), (TOP));                                        \
    } else {                                                                   \
      FILL_8_OR_16((DST), 129, (SIZE));                                        \
    }                                                                          \
  }                                                                            \
}

TRUE_MOTION(dst, left, top, 8)
TRUE_MOTION(dst, left, top, 16)

#undef TRUE_MOTION
#undef CLIP_TO_DST
#undef CLIP_8B_TO_DST
#undef CLIPPING

static WEBP_INLINE void DCMode16(uint8_t* WEBP_RESTRICT dst,
                                 const uint8_t* WEBP_RESTRICT left,
                                 const uint8_t* WEBP_RESTRICT top) {
  int DC, DC1;
  int temp0, temp1, temp2, temp3;

  __asm__ volatile(
    "beqz        %[top],   2f                  \n\t"
    LOAD_WITH_OFFSET_X4(temp0, temp1, temp2, temp3, top,
                        0, 4, 8, 12,
                        0, 0, 0, 0,
                        0)
    "raddu.w.qb  %[temp0], %[temp0]            \n\t"
    "raddu.w.qb  %[temp1], %[temp1]            \n\t"
    "raddu.w.qb  %[temp2], %[temp2]            \n\t"
    "raddu.w.qb  %[temp3], %[temp3]            \n\t"
    "addu        %[temp0], %[temp0], %[temp1]  \n\t"
    "addu        %[temp2], %[temp2], %[temp3]  \n\t"
    "addu        %[DC],    %[temp0], %[temp2]  \n\t"
    "move        %[DC1],   %[DC]               \n\t"
    "beqz        %[left],  1f                  \n\t"
    LOAD_WITH_OFFSET_X4(temp0, temp1, temp2, temp3, left,
                        0, 4, 8, 12,
                        0, 0, 0, 0,
                        0)
    "raddu.w.qb  %[temp0], %[temp0]            \n\t"
    "raddu.w.qb  %[temp1], %[temp1]            \n\t"
    "raddu.w.qb  %[temp2], %[temp2]            \n\t"
    "raddu.w.qb  %[temp3], %[temp3]            \n\t"
    "addu        %[temp0], %[temp0], %[temp1]  \n\t"
    "addu        %[temp2], %[temp2], %[temp3]  \n\t"
    "addu        %[DC1],   %[temp0], %[temp2]  \n\t"
  "1:                                          \n\t"
    "addu        %[DC],   %[DC],     %[DC1]    \n\t"
    "j           3f                            \n\t"
  "2:                                          \n\t"
    "beqz        %[left],  4f                  \n\t"
    LOAD_WITH_OFFSET_X4(temp0, temp1, temp2, temp3, left,
                        0, 4, 8, 12,
                        0, 0, 0, 0,
                        0)
    "raddu.w.qb  %[temp0], %[temp0]            \n\t"
    "raddu.w.qb  %[temp1], %[temp1]            \n\t"
    "raddu.w.qb  %[temp2], %[temp2]            \n\t"
    "raddu.w.qb  %[temp3], %[temp3]            \n\t"
    "addu        %[temp0], %[temp0], %[temp1]  \n\t"
    "addu        %[temp2], %[temp2], %[temp3]  \n\t"
    "addu        %[DC],    %[temp0], %[temp2]  \n\t"
    "addu        %[DC],    %[DC],    %[DC]     \n\t"
  "3:                                          \n\t"
    "shra_r.w    %[DC],    %[DC],    5         \n\t"
    "j           5f                            \n\t"
  "4:                                          \n\t"
    "li          %[DC],    0x80                \n\t"
  "5:                                          \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [DC]"=&r"(DC),
      [temp2]"=&r"(temp2), [temp3]"=&r"(temp3), [DC1]"=&r"(DC1)
    : [left]"r"(left), [top]"r"(top)
    : "memory"
  );

  FILL_8_OR_16(dst, DC, 16);
}

static WEBP_INLINE void DCMode8(uint8_t* WEBP_RESTRICT dst,
                                const uint8_t* WEBP_RESTRICT left,
                                const uint8_t* WEBP_RESTRICT top) {
  int DC, DC1;
  int temp0, temp1, temp2, temp3;

  __asm__ volatile(
    "beqz        %[top],   2f                  \n\t"
    "ulw         %[temp0], 0(%[top])           \n\t"
    "ulw         %[temp1], 4(%[top])           \n\t"
    "raddu.w.qb  %[temp0], %[temp0]            \n\t"
    "raddu.w.qb  %[temp1], %[temp1]            \n\t"
    "addu        %[DC],    %[temp0], %[temp1]  \n\t"
    "move        %[DC1],   %[DC]               \n\t"
    "beqz        %[left],  1f                  \n\t"
    "ulw         %[temp2], 0(%[left])          \n\t"
    "ulw         %[temp3], 4(%[left])          \n\t"
    "raddu.w.qb  %[temp2], %[temp2]            \n\t"
    "raddu.w.qb  %[temp3], %[temp3]            \n\t"
    "addu        %[DC1],   %[temp2], %[temp3]  \n\t"
  "1:                                          \n\t"
    "addu        %[DC],    %[DC],    %[DC1]    \n\t"
    "j           3f                            \n\t"
  "2:                                          \n\t"
    "beqz        %[left],  4f                  \n\t"
    "ulw         %[temp2], 0(%[left])          \n\t"
    "ulw         %[temp3], 4(%[left])          \n\t"
    "raddu.w.qb  %[temp2], %[temp2]            \n\t"
    "raddu.w.qb  %[temp3], %[temp3]            \n\t"
    "addu        %[DC],    %[temp2], %[temp3]  \n\t"
    "addu        %[DC],    %[DC],    %[DC]     \n\t"
  "3:                                          \n\t"
    "shra_r.w    %[DC], %[DC], 4               \n\t"
    "j           5f                            \n\t"
  "4:                                          \n\t"
    "li          %[DC], 0x80                   \n\t"
  "5:                                          \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [DC]"=&r"(DC),
      [temp2]"=&r"(temp2), [temp3]"=&r"(temp3), [DC1]"=&r"(DC1)
    : [left]"r"(left), [top]"r"(top)
    : "memory"
  );

  FILL_8_OR_16(dst, DC, 8);
}

static void DC4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1;
  __asm__ volatile(
    "ulw          %[temp0],   0(%[top])               \n\t"
    "ulw          %[temp1],   -5(%[top])              \n\t"
    "raddu.w.qb   %[temp0],   %[temp0]                \n\t"
    "raddu.w.qb   %[temp1],   %[temp1]                \n\t"
    "addu         %[temp0],   %[temp0],    %[temp1]   \n\t"
    "addiu        %[temp0],   %[temp0],    4          \n\t"
    "srl          %[temp0],   %[temp0],    3          \n\t"
    "replv.qb     %[temp0],   %[temp0]                \n\t"
    "usw          %[temp0],   0*" XSTR(BPS) "(%[dst]) \n\t"
    "usw          %[temp0],   1*" XSTR(BPS) "(%[dst]) \n\t"
    "usw          %[temp0],   2*" XSTR(BPS) "(%[dst]) \n\t"
    "usw          %[temp0],   3*" XSTR(BPS) "(%[dst]) \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void TM4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int a10, a32, temp0, temp1, temp2, temp3, temp4, temp5;
  const int c35 = 0xff00ff;
  __asm__ volatile (
    "lbu              %[temp1],  0(%[top])                     \n\t"
    "lbu              %[a10],    1(%[top])                     \n\t"
    "lbu              %[temp2],  2(%[top])                     \n\t"
    "lbu              %[a32],    3(%[top])                     \n\t"
    "ulw              %[temp0],  -5(%[top])                    \n\t"
    "lbu              %[temp4],  -1(%[top])                    \n\t"
    "append           %[a10],    %[temp1],   16                \n\t"
    "append           %[a32],    %[temp2],   16                \n\t"
    "replv.ph         %[temp4],  %[temp4]                      \n\t"
    "shrl.ph          %[temp1],  %[temp0],   8                 \n\t"
    "and              %[temp0],  %[temp0],   %[c35]            \n\t"
    "subu.ph          %[temp1],  %[temp1],   %[temp4]          \n\t"
    "subu.ph          %[temp0],  %[temp0],   %[temp4]          \n\t"
    "srl              %[temp2],  %[temp1],   16                \n\t"
    "srl              %[temp3],  %[temp0],   16                \n\t"
    "replv.ph         %[temp2],  %[temp2]                      \n\t"
    "replv.ph         %[temp3],  %[temp3]                      \n\t"
    "replv.ph         %[temp4],  %[temp1]                      \n\t"
    "replv.ph         %[temp5],  %[temp0]                      \n\t"
    "addu.ph          %[temp0],  %[temp3],   %[a10]            \n\t"
    "addu.ph          %[temp1],  %[temp3],   %[a32]            \n\t"
    "addu.ph          %[temp3],  %[temp2],   %[a10]            \n\t"
    "addu.ph          %[temp2],  %[temp2],   %[a32]            \n\t"
    "shll_s.ph        %[temp0],  %[temp0],   7                 \n\t"
    "shll_s.ph        %[temp1],  %[temp1],   7                 \n\t"
    "shll_s.ph        %[temp3],  %[temp3],   7                 \n\t"
    "shll_s.ph        %[temp2],  %[temp2],   7                 \n\t"
    "precrqu_s.qb.ph  %[temp0],  %[temp1],   %[temp0]          \n\t"
    "precrqu_s.qb.ph  %[temp1],  %[temp2],   %[temp3]          \n\t"
    "addu.ph          %[temp2],  %[temp5],   %[a10]            \n\t"
    "addu.ph          %[temp3],  %[temp5],   %[a32]            \n\t"
    "addu.ph          %[temp5],  %[temp4],   %[a10]            \n\t"
    "addu.ph          %[temp4],  %[temp4],   %[a32]            \n\t"
    "shll_s.ph        %[temp2],  %[temp2],   7                 \n\t"
    "shll_s.ph        %[temp3],  %[temp3],   7                 \n\t"
    "shll_s.ph        %[temp4],  %[temp4],   7                 \n\t"
    "shll_s.ph        %[temp5],  %[temp5],   7                 \n\t"
    "precrqu_s.qb.ph  %[temp2],  %[temp3],   %[temp2]          \n\t"
    "precrqu_s.qb.ph  %[temp3],  %[temp4],   %[temp5]          \n\t"
    "usw              %[temp1],  0*" XSTR(BPS) "(%[dst])       \n\t"
    "usw              %[temp0],  1*" XSTR(BPS) "(%[dst])       \n\t"
    "usw              %[temp3],  2*" XSTR(BPS) "(%[dst])       \n\t"
    "usw              %[temp2],  3*" XSTR(BPS) "(%[dst])       \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [a10]"=&r"(a10), [a32]"=&r"(a32)
    : [c35]"r"(c35), [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void VE4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1, temp2, temp3, temp4, temp5, temp6;
  __asm__ volatile(
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
    "usw             %[temp4],   0*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp4],   1*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp4],   2*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp4],   3*" XSTR(BPS) "(%[dst]) \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void HE4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1, temp2, temp3, temp4, temp5, temp6;
  __asm__ volatile(
    "ulw             %[temp0],   -4(%[top])              \n\t"
    "lbu             %[temp1],   -5(%[top])              \n\t"
    "preceu.ph.qbr   %[temp2],   %[temp0]                \n\t"
    "preceu.ph.qbl   %[temp3],   %[temp0]                \n\t"
    "replv.ph        %[temp4],   %[temp1]                \n\t"
    "packrl.ph       %[temp5],   %[temp3],    %[temp2]   \n\t"
    "packrl.ph       %[temp6],   %[temp2],    %[temp4]   \n\t"
    "shll.ph         %[temp5],   %[temp5],    1          \n\t"
    "shll.ph         %[temp6],   %[temp6],    1          \n\t"
    "addq.ph         %[temp3],   %[temp3],    %[temp5]   \n\t"
    "addq.ph         %[temp3],   %[temp3],    %[temp2]   \n\t"
    "addq.ph         %[temp2],   %[temp2],    %[temp6]   \n\t"
    "addq.ph         %[temp2],   %[temp2],    %[temp4]   \n\t"
    "shra_r.ph       %[temp3],   %[temp3],    2          \n\t"
    "shra_r.ph       %[temp2],   %[temp2],    2          \n\t"
    "replv.qb        %[temp0],   %[temp3]                \n\t"
    "replv.qb        %[temp1],   %[temp2]                \n\t"
    "srl             %[temp3],   %[temp3],    16         \n\t"
    "srl             %[temp2],   %[temp2],    16         \n\t"
    "replv.qb        %[temp3],   %[temp3]                \n\t"
    "replv.qb        %[temp2],   %[temp2]                \n\t"
    "usw             %[temp3],   0*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp0],   1*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp2],   2*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp1],   3*" XSTR(BPS) "(%[dst]) \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void RD4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1, temp2, temp3, temp4, temp5;
  int temp6, temp7, temp8, temp9, temp10, temp11;
  __asm__ volatile(
    "ulw             %[temp0],    -5(%[top])               \n\t"
    "ulw             %[temp1],    -1(%[top])               \n\t"
    "preceu.ph.qbl   %[temp2],    %[temp0]                 \n\t"
    "preceu.ph.qbr   %[temp3],    %[temp0]                 \n\t"
    "preceu.ph.qbr   %[temp4],    %[temp1]                 \n\t"
    "preceu.ph.qbl   %[temp5],    %[temp1]                 \n\t"
    "packrl.ph       %[temp6],    %[temp2],    %[temp3]    \n\t"
    "packrl.ph       %[temp7],    %[temp4],    %[temp2]    \n\t"
    "packrl.ph       %[temp8],    %[temp5],    %[temp4]    \n\t"
    "shll.ph         %[temp6],    %[temp6],    1           \n\t"
    "addq.ph         %[temp9],    %[temp2],    %[temp6]    \n\t"
    "shll.ph         %[temp7],    %[temp7],    1           \n\t"
    "addq.ph         %[temp9],    %[temp9],    %[temp3]    \n\t"
    "shll.ph         %[temp8],    %[temp8],    1           \n\t"
    "shra_r.ph       %[temp9],    %[temp9],    2           \n\t"
    "addq.ph         %[temp10],   %[temp4],    %[temp7]    \n\t"
    "addq.ph         %[temp11],   %[temp5],    %[temp8]    \n\t"
    "addq.ph         %[temp10],   %[temp10],   %[temp2]    \n\t"
    "addq.ph         %[temp11],   %[temp11],   %[temp4]    \n\t"
    "shra_r.ph       %[temp10],   %[temp10],   2           \n\t"
    "shra_r.ph       %[temp11],   %[temp11],   2           \n\t"
    "lbu             %[temp0],    3(%[top])                \n\t"
    "lbu             %[temp1],    2(%[top])                \n\t"
    "lbu             %[temp2],    1(%[top])                \n\t"
    "sll             %[temp1],    %[temp1],    1           \n\t"
    "addu            %[temp0],    %[temp0],    %[temp1]    \n\t"
    "addu            %[temp0],    %[temp0],    %[temp2]    \n\t"
    "precr.qb.ph     %[temp9],    %[temp10],   %[temp9]    \n\t"
    "shra_r.w        %[temp0],    %[temp0],    2           \n\t"
    "precr.qb.ph     %[temp10],   %[temp11],   %[temp10]   \n\t"
    "usw             %[temp9],    3*" XSTR(BPS) "(%[dst])  \n\t"
    "usw             %[temp10],   1*" XSTR(BPS) "(%[dst])  \n\t"
    "prepend         %[temp9],    %[temp11],   8           \n\t"
    "prepend         %[temp10],   %[temp0],    8           \n\t"
    "usw             %[temp9],    2*" XSTR(BPS) "(%[dst])  \n\t"
    "usw             %[temp10],   0*" XSTR(BPS) "(%[dst])  \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9), [temp10]"=&r"(temp10), [temp11]"=&r"(temp11)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void VR4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8, temp9;
  __asm__ volatile (
    "ulw              %[temp0],   -4(%[top])              \n\t"
    "ulw              %[temp1],   0(%[top])               \n\t"
    "preceu.ph.qbl    %[temp2],   %[temp0]                \n\t"
    "preceu.ph.qbr    %[temp0],   %[temp0]                \n\t"
    "preceu.ph.qbla   %[temp3],   %[temp1]                \n\t"
    "preceu.ph.qbra   %[temp1],   %[temp1]                \n\t"
    "packrl.ph        %[temp7],   %[temp3],    %[temp2]   \n\t"
    "addqh_r.ph       %[temp4],   %[temp1],    %[temp3]   \n\t"
    "move             %[temp6],   %[temp1]                \n\t"
    "append           %[temp1],   %[temp2],    16         \n\t"
    "shll.ph          %[temp9],   %[temp6],    1          \n\t"
    "addqh_r.ph       %[temp5],   %[temp7],    %[temp6]   \n\t"
    "shll.ph          %[temp8],   %[temp7],    1          \n\t"
    "addu.ph          %[temp3],   %[temp7],    %[temp3]   \n\t"
    "addu.ph          %[temp1],   %[temp1],    %[temp6]   \n\t"
    "packrl.ph        %[temp7],   %[temp2],    %[temp0]   \n\t"
    "addu.ph          %[temp6],   %[temp0],    %[temp2]   \n\t"
    "addu.ph          %[temp3],   %[temp3],    %[temp9]   \n\t"
    "addu.ph          %[temp1],   %[temp1],    %[temp8]   \n\t"
    "shll.ph          %[temp7],   %[temp7],    1          \n\t"
    "shra_r.ph        %[temp3],   %[temp3],    2          \n\t"
    "shra_r.ph        %[temp1],   %[temp1],    2          \n\t"
    "addu.ph          %[temp6],   %[temp6],    %[temp7]   \n\t"
    "shra_r.ph        %[temp6],   %[temp6],    2          \n\t"
    "precrq.ph.w      %[temp8],   %[temp4],    %[temp5]   \n\t"
    "append           %[temp4],   %[temp5],    16         \n\t"
    "precrq.ph.w      %[temp2],   %[temp3],    %[temp1]   \n\t"
    "append           %[temp3],   %[temp1],    16         \n\t"
    "precr.qb.ph      %[temp8],   %[temp8],    %[temp4]   \n\t"
    "precr.qb.ph      %[temp3],   %[temp2],    %[temp3]   \n\t"
    "usw              %[temp8],   0*" XSTR(BPS) "(%[dst]) \n\t"
    "usw              %[temp3],   1*" XSTR(BPS) "(%[dst]) \n\t"
    "append           %[temp3],   %[temp6],    8          \n\t"
    "srl              %[temp6],   %[temp6],    16         \n\t"
    "append           %[temp8],   %[temp6],    8          \n\t"
    "usw              %[temp3],   3*" XSTR(BPS) "(%[dst]) \n\t"
    "usw              %[temp8],   2*" XSTR(BPS) "(%[dst]) \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void LD4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1, temp2, temp3, temp4, temp5;
  int temp6, temp7, temp8, temp9, temp10, temp11;
  __asm__ volatile(
    "ulw             %[temp0],    0(%[top])               \n\t"
    "ulw             %[temp1],    4(%[top])               \n\t"
    "preceu.ph.qbl   %[temp2],    %[temp0]                \n\t"
    "preceu.ph.qbr   %[temp3],    %[temp0]                \n\t"
    "preceu.ph.qbr   %[temp4],    %[temp1]                \n\t"
    "preceu.ph.qbl   %[temp5],    %[temp1]                \n\t"
    "packrl.ph       %[temp6],    %[temp2],    %[temp3]   \n\t"
    "packrl.ph       %[temp7],    %[temp4],    %[temp2]   \n\t"
    "packrl.ph       %[temp8],    %[temp5],    %[temp4]   \n\t"
    "shll.ph         %[temp6],    %[temp6],    1          \n\t"
    "addq.ph         %[temp9],    %[temp2],    %[temp6]   \n\t"
    "shll.ph         %[temp7],    %[temp7],    1          \n\t"
    "addq.ph         %[temp9],    %[temp9],    %[temp3]   \n\t"
    "shll.ph         %[temp8],    %[temp8],    1          \n\t"
    "shra_r.ph       %[temp9],    %[temp9],    2          \n\t"
    "addq.ph         %[temp10],   %[temp4],    %[temp7]   \n\t"
    "addq.ph         %[temp11],   %[temp5],    %[temp8]   \n\t"
    "addq.ph         %[temp10],   %[temp10],   %[temp2]   \n\t"
    "addq.ph         %[temp11],   %[temp11],   %[temp4]   \n\t"
    "shra_r.ph       %[temp10],   %[temp10],   2          \n\t"
    "shra_r.ph       %[temp11],   %[temp11],   2          \n\t"
    "srl             %[temp1],    %[temp1],    24         \n\t"
    "sll             %[temp1],    %[temp1],    1          \n\t"
    "raddu.w.qb      %[temp5],    %[temp5]                \n\t"
    "precr.qb.ph     %[temp9],    %[temp10],   %[temp9]   \n\t"
    "precr.qb.ph     %[temp10],   %[temp11],   %[temp10]  \n\t"
    "addu            %[temp1],    %[temp1],    %[temp5]   \n\t"
    "shra_r.w        %[temp1],    %[temp1],    2          \n\t"
    "usw             %[temp9],    0*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp10],   2*" XSTR(BPS) "(%[dst]) \n\t"
    "prepend         %[temp9],    %[temp11],   8          \n\t"
    "prepend         %[temp10],   %[temp1],    8          \n\t"
    "usw             %[temp9],    1*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp10],   3*" XSTR(BPS) "(%[dst]) \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9), [temp10]"=&r"(temp10), [temp11]"=&r"(temp11)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void VL4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8, temp9;
  __asm__ volatile (
    "ulw              %[temp0],   0(%[top])               \n\t"
    "ulw              %[temp1],   4(%[top])               \n\t"
    "preceu.ph.qbla   %[temp2],   %[temp0]                \n\t"
    "preceu.ph.qbra   %[temp0],   %[temp0]                \n\t"
    "preceu.ph.qbl    %[temp3],   %[temp1]                \n\t"
    "preceu.ph.qbr    %[temp1],   %[temp1]                \n\t"
    "addqh_r.ph       %[temp4],   %[temp0],    %[temp2]   \n\t"
    "packrl.ph        %[temp7],   %[temp1],    %[temp0]   \n\t"
    "precrq.ph.w      %[temp6],   %[temp1],    %[temp2]   \n\t"
    "shll.ph          %[temp9],   %[temp2],    1          \n\t"
    "addqh_r.ph       %[temp5],   %[temp7],    %[temp2]   \n\t"
    "shll.ph          %[temp8],   %[temp7],    1          \n\t"
    "addu.ph          %[temp2],   %[temp2],    %[temp6]   \n\t"
    "addu.ph          %[temp0],   %[temp0],    %[temp7]   \n\t"
    "packrl.ph        %[temp7],   %[temp3],    %[temp1]   \n\t"
    "addu.ph          %[temp6],   %[temp1],    %[temp3]   \n\t"
    "addu.ph          %[temp2],   %[temp2],    %[temp8]   \n\t"
    "addu.ph          %[temp0],   %[temp0],    %[temp9]   \n\t"
    "shll.ph          %[temp7],   %[temp7],    1          \n\t"
    "shra_r.ph        %[temp2],   %[temp2],    2          \n\t"
    "shra_r.ph        %[temp0],   %[temp0],    2          \n\t"
    "addu.ph          %[temp6],   %[temp6],    %[temp7]   \n\t"
    "shra_r.ph        %[temp6],   %[temp6],    2          \n\t"
    "precrq.ph.w      %[temp8],   %[temp5],    %[temp4]   \n\t"
    "append           %[temp5],   %[temp4],    16         \n\t"
    "precrq.ph.w      %[temp3],   %[temp2],    %[temp0]   \n\t"
    "append           %[temp2],   %[temp0],    16         \n\t"
    "precr.qb.ph      %[temp8],   %[temp8],    %[temp5]   \n\t"
    "precr.qb.ph      %[temp3],   %[temp3],    %[temp2]   \n\t"
    "usw              %[temp8],   0*" XSTR(BPS) "(%[dst]) \n\t"
    "prepend          %[temp8],   %[temp6],    8          \n\t"
    "usw              %[temp3],   1*" XSTR(BPS) "(%[dst]) \n\t"
    "srl              %[temp6],   %[temp6],    16         \n\t"
    "prepend          %[temp3],   %[temp6],    8          \n\t"
    "usw              %[temp8],   2*" XSTR(BPS) "(%[dst]) \n\t"
    "usw              %[temp3],   3*" XSTR(BPS) "(%[dst]) \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void HD4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8, temp9;
  __asm__ volatile (
    "ulw              %[temp0],   -5(%[top])              \n\t"
    "ulw              %[temp1],   -1(%[top])              \n\t"
    "preceu.ph.qbla   %[temp2],   %[temp0]                \n\t"
    "preceu.ph.qbra   %[temp0],   %[temp0]                \n\t"
    "preceu.ph.qbl    %[temp3],   %[temp1]                \n\t"
    "preceu.ph.qbr    %[temp1],   %[temp1]                \n\t"
    "addqh_r.ph       %[temp4],   %[temp0],    %[temp2]   \n\t"
    "packrl.ph        %[temp7],   %[temp1],    %[temp0]   \n\t"
    "precrq.ph.w      %[temp6],   %[temp1],    %[temp2]   \n\t"
    "shll.ph          %[temp9],   %[temp2],    1          \n\t"
    "addqh_r.ph       %[temp5],   %[temp7],    %[temp2]   \n\t"
    "shll.ph          %[temp8],   %[temp7],    1          \n\t"
    "addu.ph          %[temp2],   %[temp2],    %[temp6]   \n\t"
    "addu.ph          %[temp0],   %[temp0],    %[temp7]   \n\t"
    "packrl.ph        %[temp7],   %[temp3],    %[temp1]   \n\t"
    "addu.ph          %[temp6],   %[temp1],    %[temp3]   \n\t"
    "addu.ph          %[temp2],   %[temp2],    %[temp8]   \n\t"
    "addu.ph          %[temp0],   %[temp0],    %[temp9]   \n\t"
    "shll.ph          %[temp7],   %[temp7],    1          \n\t"
    "shra_r.ph        %[temp2],   %[temp2],    2          \n\t"
    "shra_r.ph        %[temp0],   %[temp0],    2          \n\t"
    "addu.ph          %[temp6],   %[temp6],    %[temp7]   \n\t"
    "shra_r.ph        %[temp6],   %[temp6],    2          \n\t"
    "precrq.ph.w      %[temp1],   %[temp2],    %[temp5]   \n\t"
    "precrq.ph.w      %[temp3],   %[temp0],    %[temp4]   \n\t"
    "precr.qb.ph      %[temp7],   %[temp6],    %[temp1]   \n\t"
    "precr.qb.ph      %[temp6],   %[temp1],    %[temp3]   \n\t"
    "usw              %[temp7],   0*" XSTR(BPS) "(%[dst]) \n\t"
    "usw              %[temp6],   1*" XSTR(BPS) "(%[dst]) \n\t"
    "append           %[temp2],   %[temp5],    16         \n\t"
    "append           %[temp0],   %[temp4],    16         \n\t"
    "precr.qb.ph      %[temp5],   %[temp3],    %[temp2]   \n\t"
    "precr.qb.ph      %[temp4],   %[temp2],    %[temp0]   \n\t"
    "usw              %[temp5],   2*" XSTR(BPS) "(%[dst]) \n\t"
    "usw              %[temp4],   3*" XSTR(BPS) "(%[dst]) \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

static void HU4(uint8_t* WEBP_RESTRICT dst, const uint8_t* WEBP_RESTRICT top) {
  int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7;
  __asm__ volatile (
    "ulw             %[temp0],   -5(%[top])              \n\t"
    "preceu.ph.qbl   %[temp1],   %[temp0]                \n\t"
    "preceu.ph.qbr   %[temp2],   %[temp0]                \n\t"
    "packrl.ph       %[temp3],   %[temp1],    %[temp2]   \n\t"
    "replv.qb        %[temp7],   %[temp2]                \n\t"
    "addqh_r.ph      %[temp4],   %[temp1],    %[temp3]   \n\t"
    "addqh_r.ph      %[temp5],   %[temp3],    %[temp2]   \n\t"
    "shll.ph         %[temp6],   %[temp3],    1          \n\t"
    "addu.ph         %[temp3],   %[temp2],    %[temp3]   \n\t"
    "addu.ph         %[temp6],   %[temp1],    %[temp6]   \n\t"
    "shll.ph         %[temp0],   %[temp2],    1          \n\t"
    "addu.ph         %[temp6],   %[temp6],    %[temp2]   \n\t"
    "addu.ph         %[temp0],   %[temp3],    %[temp0]   \n\t"
    "shra_r.ph       %[temp6],   %[temp6],    2          \n\t"
    "shra_r.ph       %[temp0],   %[temp0],    2          \n\t"
    "packrl.ph       %[temp3],   %[temp6],    %[temp5]   \n\t"
    "precrq.ph.w     %[temp2],   %[temp6],    %[temp4]   \n\t"
    "append          %[temp0],   %[temp5],    16         \n\t"
    "precr.qb.ph     %[temp3],   %[temp3],    %[temp2]   \n\t"
    "usw             %[temp3],   0*" XSTR(BPS) "(%[dst]) \n\t"
    "precr.qb.ph     %[temp1],   %[temp7],    %[temp0]   \n\t"
    "usw             %[temp7],   3*" XSTR(BPS) "(%[dst]) \n\t"
    "packrl.ph       %[temp2],   %[temp1],    %[temp3]   \n\t"
    "usw             %[temp1],   2*" XSTR(BPS) "(%[dst]) \n\t"
    "usw             %[temp2],   1*" XSTR(BPS) "(%[dst]) \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7)
    : [top]"r"(top), [dst]"r"(dst)
    : "memory"
  );
}

//------------------------------------------------------------------------------
// Chroma 8x8 prediction (paragraph 12.2)

static void IntraChromaPreds_MIPSdspR2(uint8_t* WEBP_RESTRICT dst,
                                       const uint8_t* WEBP_RESTRICT left,
                                       const uint8_t* WEBP_RESTRICT top) {
  // U block
  DCMode8(C8DC8 + dst, left, top);
  VerticalPred8(C8VE8 + dst, top);
  HorizontalPred8(C8HE8 + dst, left);
  TrueMotion8(C8TM8 + dst, left, top);
  // V block
  dst += 8;
  if (top) top += 8;
  if (left) left += 16;
  DCMode8(C8DC8 + dst, left, top);
  VerticalPred8(C8VE8 + dst, top);
  HorizontalPred8(C8HE8 + dst, left);
  TrueMotion8(C8TM8 + dst, left, top);
}

//------------------------------------------------------------------------------
// luma 16x16 prediction (paragraph 12.3)

static void Intra16Preds_MIPSdspR2(uint8_t* WEBP_RESTRICT dst,
                                   const uint8_t* WEBP_RESTRICT left,
                                   const uint8_t* WEBP_RESTRICT top) {
  DCMode16(I16DC16 + dst, left, top);
  VerticalPred16(I16VE16 + dst, top);
  HorizontalPred16(I16HE16 + dst, left);
  TrueMotion16(I16TM16 + dst, left, top);
}

// Left samples are top[-5 .. -2], top_left is top[-1], top are
// located at top[0..3], and top right is top[4..7]
static void Intra4Preds_MIPSdspR2(uint8_t* WEBP_RESTRICT dst,
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

//------------------------------------------------------------------------------
// Metric

#if !defined(WORK_AROUND_GCC)

#define GET_SSE_INNER(A)                                                  \
  "lw               %[temp0],    " #A "(%[a])                  \n\t"      \
  "lw               %[temp1],    " #A "(%[b])                  \n\t"      \
  "preceu.ph.qbr    %[temp2],    %[temp0]                      \n\t"      \
  "preceu.ph.qbl    %[temp0],    %[temp0]                      \n\t"      \
  "preceu.ph.qbr    %[temp3],    %[temp1]                      \n\t"      \
  "preceu.ph.qbl    %[temp1],    %[temp1]                      \n\t"      \
  "subq.ph          %[temp2],    %[temp2],    %[temp3]         \n\t"      \
  "subq.ph          %[temp0],    %[temp0],    %[temp1]         \n\t"      \
  "dpa.w.ph         $ac0,        %[temp2],    %[temp2]         \n\t"      \
  "dpa.w.ph         $ac0,        %[temp0],    %[temp0]         \n\t"

#define GET_SSE(A, B, C, D)               \
  GET_SSE_INNER(A)                        \
  GET_SSE_INNER(B)                        \
  GET_SSE_INNER(C)                        \
  GET_SSE_INNER(D)

static int SSE16x16_MIPSdspR2(const uint8_t* WEBP_RESTRICT a,
                              const uint8_t* WEBP_RESTRICT b) {
  int count;
  int temp0, temp1, temp2, temp3;
  __asm__ volatile (
    "mult   $zero,    $zero                            \n\t"
    GET_SSE( 0 * BPS, 4 +  0 * BPS, 8 +  0 * BPS, 12 +  0 * BPS)
    GET_SSE( 1 * BPS, 4 +  1 * BPS, 8 +  1 * BPS, 12 +  1 * BPS)
    GET_SSE( 2 * BPS, 4 +  2 * BPS, 8 +  2 * BPS, 12 +  2 * BPS)
    GET_SSE( 3 * BPS, 4 +  3 * BPS, 8 +  3 * BPS, 12 +  3 * BPS)
    GET_SSE( 4 * BPS, 4 +  4 * BPS, 8 +  4 * BPS, 12 +  4 * BPS)
    GET_SSE( 5 * BPS, 4 +  5 * BPS, 8 +  5 * BPS, 12 +  5 * BPS)
    GET_SSE( 6 * BPS, 4 +  6 * BPS, 8 +  6 * BPS, 12 +  6 * BPS)
    GET_SSE( 7 * BPS, 4 +  7 * BPS, 8 +  7 * BPS, 12 +  7 * BPS)
    GET_SSE( 8 * BPS, 4 +  8 * BPS, 8 +  8 * BPS, 12 +  8 * BPS)
    GET_SSE( 9 * BPS, 4 +  9 * BPS, 8 +  9 * BPS, 12 +  9 * BPS)
    GET_SSE(10 * BPS, 4 + 10 * BPS, 8 + 10 * BPS, 12 + 10 * BPS)
    GET_SSE(11 * BPS, 4 + 11 * BPS, 8 + 11 * BPS, 12 + 11 * BPS)
    GET_SSE(12 * BPS, 4 + 12 * BPS, 8 + 12 * BPS, 12 + 12 * BPS)
    GET_SSE(13 * BPS, 4 + 13 * BPS, 8 + 13 * BPS, 12 + 13 * BPS)
    GET_SSE(14 * BPS, 4 + 14 * BPS, 8 + 14 * BPS, 12 + 14 * BPS)
    GET_SSE(15 * BPS, 4 + 15 * BPS, 8 + 15 * BPS, 12 + 15 * BPS)
    "mflo   %[count]                                   \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [count]"=&r"(count)
    : [a]"r"(a), [b]"r"(b)
    : "memory", "hi", "lo"
  );
  return count;
}

static int SSE16x8_MIPSdspR2(const uint8_t* WEBP_RESTRICT a,
                             const uint8_t* WEBP_RESTRICT b) {
  int count;
  int temp0, temp1, temp2, temp3;
  __asm__ volatile (
    "mult   $zero,    $zero                            \n\t"
    GET_SSE( 0 * BPS, 4 +  0 * BPS, 8 +  0 * BPS, 12 +  0 * BPS)
    GET_SSE( 1 * BPS, 4 +  1 * BPS, 8 +  1 * BPS, 12 +  1 * BPS)
    GET_SSE( 2 * BPS, 4 +  2 * BPS, 8 +  2 * BPS, 12 +  2 * BPS)
    GET_SSE( 3 * BPS, 4 +  3 * BPS, 8 +  3 * BPS, 12 +  3 * BPS)
    GET_SSE( 4 * BPS, 4 +  4 * BPS, 8 +  4 * BPS, 12 +  4 * BPS)
    GET_SSE( 5 * BPS, 4 +  5 * BPS, 8 +  5 * BPS, 12 +  5 * BPS)
    GET_SSE( 6 * BPS, 4 +  6 * BPS, 8 +  6 * BPS, 12 +  6 * BPS)
    GET_SSE( 7 * BPS, 4 +  7 * BPS, 8 +  7 * BPS, 12 +  7 * BPS)
    "mflo   %[count]                                   \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [count]"=&r"(count)
    : [a]"r"(a), [b]"r"(b)
    : "memory", "hi", "lo"
  );
  return count;
}

static int SSE8x8_MIPSdspR2(const uint8_t* WEBP_RESTRICT a,
                            const uint8_t* WEBP_RESTRICT b) {
  int count;
  int temp0, temp1, temp2, temp3;
  __asm__ volatile (
    "mult   $zero,    $zero                            \n\t"
    GET_SSE(0 * BPS, 4 + 0 * BPS, 1 * BPS, 4 + 1 * BPS)
    GET_SSE(2 * BPS, 4 + 2 * BPS, 3 * BPS, 4 + 3 * BPS)
    GET_SSE(4 * BPS, 4 + 4 * BPS, 5 * BPS, 4 + 5 * BPS)
    GET_SSE(6 * BPS, 4 + 6 * BPS, 7 * BPS, 4 + 7 * BPS)
    "mflo   %[count]                                   \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [count]"=&r"(count)
    : [a]"r"(a), [b]"r"(b)
    : "memory", "hi", "lo"
  );
  return count;
}

static int SSE4x4_MIPSdspR2(const uint8_t* WEBP_RESTRICT a,
                            const uint8_t* WEBP_RESTRICT b) {
  int count;
  int temp0, temp1, temp2, temp3;
  __asm__ volatile (
    "mult   $zero,    $zero                            \n\t"
    GET_SSE(0 * BPS, 1 * BPS, 2 * BPS, 3 * BPS)
    "mflo   %[count]                                   \n\t"
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [count]"=&r"(count)
    : [a]"r"(a), [b]"r"(b)
    : "memory", "hi", "lo"
  );
  return count;
}

#undef GET_SSE
#undef GET_SSE_INNER

#endif  // !WORK_AROUND_GCC

#undef FILL_8_OR_16
#undef FILL_PART
#undef OUTPUT_EARLY_CLOBBER_REGS_17
#undef MUL_HALF
#undef ABS_X8
#undef ADD_SUB_HALVES_X4

//------------------------------------------------------------------------------
// Quantization
//

// macro for one pass through for loop in QuantizeBlock reading 2 values at time
// QUANTDIV macro inlined
// J - offset in bytes (kZigzag[n] * 2)
// K - offset in bytes (kZigzag[n] * 4)
// N - offset in bytes (n * 2)
// N1 - offset in bytes ((n + 1) * 2)
#define QUANTIZE_ONE(J, K, N, N1)                                         \
  "ulw         %[temp1],     " #J "(%[ppin])                 \n\t"        \
  "ulw         %[temp2],     " #J "(%[ppsharpen])            \n\t"        \
  "lhu         %[temp3],     " #K "(%[ppzthresh])            \n\t"        \
  "lhu         %[temp6],     " #K "+4(%[ppzthresh])          \n\t"        \
  "absq_s.ph   %[temp4],     %[temp1]                        \n\t"        \
  "ins         %[temp3],     %[temp6],         16,       16  \n\t"        \
  "addu.ph     %[coeff],     %[temp4],         %[temp2]      \n\t"        \
  "shra.ph     %[sign],      %[temp1],         15            \n\t"        \
  "li          %[level],     0x10001                         \n\t"        \
  "cmp.lt.ph   %[temp3],     %[coeff]                        \n\t"        \
  "lhu         %[temp1],     " #J "(%[ppiq])                 \n\t"        \
  "pick.ph     %[temp5],     %[level],         $0            \n\t"        \
  "lw          %[temp2],     " #K "(%[ppbias])               \n\t"        \
  "beqz        %[temp5],     0f                              \n\t"        \
  "lhu         %[temp3],     " #J "(%[ppq])                  \n\t"        \
  "beq         %[temp5],     %[level],         1f            \n\t"        \
  "andi        %[temp5],     %[temp5],         0x1           \n\t"        \
  "andi        %[temp4],     %[coeff],         0xffff        \n\t"        \
  "beqz        %[temp5],     2f                              \n\t"        \
  "mul         %[level],     %[temp4],         %[temp1]      \n\t"        \
  "sh          $0,           " #J "+2(%[ppin])               \n\t"        \
  "sh          $0,           " #N1 "(%[pout])                \n\t"        \
  "addu        %[level],     %[level],         %[temp2]      \n\t"        \
  "sra         %[level],     %[level],         17            \n\t"        \
  "slt         %[temp4],     %[max_level],     %[level]      \n\t"        \
  "movn        %[level],     %[max_level],     %[temp4]      \n\t"        \
  "andi        %[temp6],     %[sign],          0xffff        \n\t"        \
  "xor         %[level],     %[level],         %[temp6]      \n\t"        \
  "subu        %[level],     %[level],         %[temp6]      \n\t"        \
  "mul         %[temp5],     %[level],         %[temp3]      \n\t"        \
  "or          %[ret],       %[ret],           %[level]      \n\t"        \
  "sh          %[level],     " #N "(%[pout])                 \n\t"        \
  "sh          %[temp5],     " #J "(%[ppin])                 \n\t"        \
  "j           3f                                            \n\t"        \
"2:                                                          \n\t"        \
  "lhu         %[temp1],     " #J "+2(%[ppiq])               \n\t"        \
  "srl         %[temp5],     %[coeff],         16            \n\t"        \
  "mul         %[level],     %[temp5],         %[temp1]      \n\t"        \
  "lw          %[temp2],     " #K "+4(%[ppbias])             \n\t"        \
  "lhu         %[temp3],     " #J "+2(%[ppq])                \n\t"        \
  "addu        %[level],     %[level],         %[temp2]      \n\t"        \
  "sra         %[level],     %[level],         17            \n\t"        \
  "srl         %[temp6],     %[sign],          16            \n\t"        \
  "slt         %[temp4],     %[max_level],     %[level]      \n\t"        \
  "movn        %[level],     %[max_level],     %[temp4]      \n\t"        \
  "xor         %[level],     %[level],         %[temp6]      \n\t"        \
  "subu        %[level],     %[level],         %[temp6]      \n\t"        \
  "mul         %[temp5],     %[level],         %[temp3]      \n\t"        \
  "sh          $0,           " #J "(%[ppin])                 \n\t"        \
  "sh          $0,           " #N "(%[pout])                 \n\t"        \
  "or          %[ret],       %[ret],           %[level]      \n\t"        \
  "sh          %[temp5],     " #J "+2(%[ppin])               \n\t"        \
  "sh          %[level],     " #N1 "(%[pout])                \n\t"        \
  "j           3f                                            \n\t"        \
"1:                                                          \n\t"        \
  "lhu         %[temp1],     " #J "(%[ppiq])                 \n\t"        \
  "lw          %[temp2],     " #K "(%[ppbias])               \n\t"        \
  "ulw         %[temp3],     " #J "(%[ppq])                  \n\t"        \
  "andi        %[temp5],     %[coeff],         0xffff        \n\t"        \
  "srl         %[temp0],     %[coeff],         16            \n\t"        \
  "lhu         %[temp6],     " #J "+2(%[ppiq])               \n\t"        \
  "lw          %[coeff],     " #K "+4(%[ppbias])             \n\t"        \
  "mul         %[level],     %[temp5],         %[temp1]      \n\t"        \
  "mul         %[temp4],     %[temp0],         %[temp6]      \n\t"        \
  "addu        %[level],     %[level],         %[temp2]      \n\t"        \
  "addu        %[temp4],     %[temp4],         %[coeff]      \n\t"        \
  "precrq.ph.w %[level],     %[temp4],         %[level]      \n\t"        \
  "shra.ph     %[level],     %[level],         1             \n\t"        \
  "cmp.lt.ph   %[max_level1],%[level]                        \n\t"        \
  "pick.ph     %[level],     %[max_level],     %[level]      \n\t"        \
  "xor         %[level],     %[level],         %[sign]       \n\t"        \
  "subu.ph     %[level],     %[level],         %[sign]       \n\t"        \
  "mul.ph      %[temp3],     %[level],         %[temp3]      \n\t"        \
  "or          %[ret],       %[ret],           %[level]      \n\t"        \
  "sh          %[level],     " #N "(%[pout])                 \n\t"        \
  "srl         %[level],     %[level],         16            \n\t"        \
  "sh          %[level],     " #N1 "(%[pout])                \n\t"        \
  "usw         %[temp3],     " #J "(%[ppin])                 \n\t"        \
  "j           3f                                            \n\t"        \
"0:                                                          \n\t"        \
  "sh          $0,           " #N "(%[pout])                 \n\t"        \
  "sh          $0,           " #N1 "(%[pout])                \n\t"        \
  "usw         $0,           " #J "(%[ppin])                 \n\t"        \
"3:                                                          \n\t"

static int QuantizeBlock_MIPSdspR2(int16_t in[16], int16_t out[16],
                                   const VP8Matrix* WEBP_RESTRICT const mtx) {
  int temp0, temp1, temp2, temp3, temp4, temp5,temp6;
  int sign, coeff, level;
  int max_level = MAX_LEVEL;
  int max_level1 = max_level << 16 | max_level;
  int ret = 0;

  int16_t* ppin             = &in[0];
  int16_t* pout             = &out[0];
  const uint16_t* ppsharpen = &mtx->sharpen_[0];
  const uint32_t* ppzthresh = &mtx->zthresh_[0];
  const uint16_t* ppq       = &mtx->q_[0];
  const uint16_t* ppiq      = &mtx->iq_[0];
  const uint32_t* ppbias    = &mtx->bias_[0];

  __asm__ volatile (
    QUANTIZE_ONE( 0,  0,  0,  2)
    QUANTIZE_ONE( 4,  8, 10, 12)
    QUANTIZE_ONE( 8, 16,  4,  8)
    QUANTIZE_ONE(12, 24, 14, 24)
    QUANTIZE_ONE(16, 32,  6, 16)
    QUANTIZE_ONE(20, 40, 22, 26)
    QUANTIZE_ONE(24, 48, 18, 20)
    QUANTIZE_ONE(28, 56, 28, 30)

    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1),
      [temp2]"=&r"(temp2), [temp3]"=&r"(temp3),
      [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [sign]"=&r"(sign), [coeff]"=&r"(coeff),
      [level]"=&r"(level), [temp6]"=&r"(temp6), [ret]"+&r"(ret)
    : [ppin]"r"(ppin), [pout]"r"(pout), [max_level1]"r"(max_level1),
      [ppiq]"r"(ppiq), [max_level]"r"(max_level),
      [ppbias]"r"(ppbias), [ppzthresh]"r"(ppzthresh),
      [ppsharpen]"r"(ppsharpen), [ppq]"r"(ppq)
    : "memory", "hi", "lo"
  );

  return (ret != 0);
}

static int Quantize2Blocks_MIPSdspR2(int16_t in[32], int16_t out[32],
                                     const VP8Matrix* WEBP_RESTRICT const mtx) {
  int nz;
  nz  = QuantizeBlock_MIPSdspR2(in + 0 * 16, out + 0 * 16, mtx) << 0;
  nz |= QuantizeBlock_MIPSdspR2(in + 1 * 16, out + 1 * 16, mtx) << 1;
  return nz;
}

#undef QUANTIZE_ONE

// macro for one horizontal pass in FTransformWHT
// temp0..temp7 holds tmp[0]..tmp[15]
// A, B, C, D - offset in bytes to load from in buffer
// TEMP0, TEMP1 - registers for corresponding tmp elements
#define HORIZONTAL_PASS_WHT(A, B, C, D, TEMP0, TEMP1)                          \
  "lh              %[" #TEMP0 "],  " #A "(%[in])            \n\t"              \
  "lh              %[" #TEMP1 "],  " #B "(%[in])            \n\t"              \
  "lh              %[temp8],     " #C "(%[in])              \n\t"              \
  "lh              %[temp9],     " #D "(%[in])              \n\t"              \
  "ins             %[" #TEMP1 "],  %[" #TEMP0 "],  16,  16  \n\t"              \
  "ins             %[temp9],     %[temp8],     16,  16      \n\t"              \
  "subq.ph         %[temp8],     %[" #TEMP1 "],  %[temp9]   \n\t"              \
  "addq.ph         %[temp9],     %[" #TEMP1 "],  %[temp9]   \n\t"              \
  "precrq.ph.w     %[" #TEMP0 "],  %[temp8],     %[temp9]   \n\t"              \
  "append          %[temp8],     %[temp9],     16           \n\t"              \
  "subq.ph         %[" #TEMP1 "],  %[" #TEMP0 "],  %[temp8] \n\t"              \
  "addq.ph         %[" #TEMP0 "],  %[" #TEMP0 "],  %[temp8] \n\t"              \
  "rotr            %[" #TEMP1 "],  %[" #TEMP1 "],  16       \n\t"

// macro for one vertical pass in FTransformWHT
// temp0..temp7 holds tmp[0]..tmp[15]
// A, B, C, D - offsets in bytes to store to out buffer
// TEMP0, TEMP2, TEMP4 and TEMP6 - registers for corresponding tmp elements
#define VERTICAL_PASS_WHT(A, B, C, D, TEMP0, TEMP2, TEMP4, TEMP6)              \
  "addq.ph         %[temp8],     %[" #TEMP0 "],  %[" #TEMP4 "]    \n\t"        \
  "addq.ph         %[temp9],     %[" #TEMP2 "],  %[" #TEMP6 "]    \n\t"        \
  "subq.ph         %[" #TEMP2 "],  %[" #TEMP2 "],  %[" #TEMP6 "]  \n\t"        \
  "subq.ph         %[" #TEMP6 "],  %[" #TEMP0 "],  %[" #TEMP4 "]  \n\t"        \
  "addqh.ph        %[" #TEMP0 "],  %[temp8],     %[temp9]         \n\t"        \
  "subqh.ph        %[" #TEMP4 "],  %[" #TEMP6 "],  %[" #TEMP2 "]  \n\t"        \
  "addqh.ph        %[" #TEMP2 "],  %[" #TEMP2 "],  %[" #TEMP6 "]  \n\t"        \
  "subqh.ph        %[" #TEMP6 "],  %[temp8],     %[temp9]         \n\t"        \
  "usw             %[" #TEMP0 "],  " #A "(%[out])                 \n\t"        \
  "usw             %[" #TEMP2 "],  " #B "(%[out])                 \n\t"        \
  "usw             %[" #TEMP4 "],  " #C "(%[out])                 \n\t"        \
  "usw             %[" #TEMP6 "],  " #D "(%[out])                 \n\t"

static void FTransformWHT_MIPSdspR2(const int16_t* WEBP_RESTRICT in,
                                    int16_t* WEBP_RESTRICT out) {
  int temp0, temp1, temp2, temp3, temp4;
  int temp5, temp6, temp7, temp8, temp9;

  __asm__ volatile (
    HORIZONTAL_PASS_WHT(  0,  32,  64,  96, temp0, temp1)
    HORIZONTAL_PASS_WHT(128, 160, 192, 224, temp2, temp3)
    HORIZONTAL_PASS_WHT(256, 288, 320, 352, temp4, temp5)
    HORIZONTAL_PASS_WHT(384, 416, 448, 480, temp6, temp7)
    VERTICAL_PASS_WHT(0,  8, 16, 24, temp0, temp2, temp4, temp6)
    VERTICAL_PASS_WHT(4, 12, 20, 28, temp1, temp3, temp5, temp7)
    : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
      [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
      [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8),
      [temp9]"=&r"(temp9)
    : [in]"r"(in), [out]"r"(out)
    : "memory"
  );
}

#undef VERTICAL_PASS_WHT
#undef HORIZONTAL_PASS_WHT

// macro for converting coefficients to bin
// convert 8 coeffs at time
// A, B, C, D - offsets in bytes to load from out buffer
#define CONVERT_COEFFS_TO_BIN(A, B, C, D)                                      \
  "ulw        %[temp0],  " #A "(%[out])                \n\t"                   \
  "ulw        %[temp1],  " #B "(%[out])                \n\t"                   \
  "ulw        %[temp2],  " #C "(%[out])                \n\t"                   \
  "ulw        %[temp3],  " #D "(%[out])                \n\t"                   \
  "absq_s.ph  %[temp0],  %[temp0]                      \n\t"                   \
  "absq_s.ph  %[temp1],  %[temp1]                      \n\t"                   \
  "absq_s.ph  %[temp2],  %[temp2]                      \n\t"                   \
  "absq_s.ph  %[temp3],  %[temp3]                      \n\t"                   \
  "shra.ph    %[temp0],  %[temp0],    3                \n\t"                   \
  "shra.ph    %[temp1],  %[temp1],    3                \n\t"                   \
  "shra.ph    %[temp2],  %[temp2],    3                \n\t"                   \
  "shra.ph    %[temp3],  %[temp3],    3                \n\t"                   \
  "shll_s.ph  %[temp0],  %[temp0],    10               \n\t"                   \
  "shll_s.ph  %[temp1],  %[temp1],    10               \n\t"                   \
  "shll_s.ph  %[temp2],  %[temp2],    10               \n\t"                   \
  "shll_s.ph  %[temp3],  %[temp3],    10               \n\t"                   \
  "shrl.ph    %[temp0],  %[temp0],    10               \n\t"                   \
  "shrl.ph    %[temp1],  %[temp1],    10               \n\t"                   \
  "shrl.ph    %[temp2],  %[temp2],    10               \n\t"                   \
  "shrl.ph    %[temp3],  %[temp3],    10               \n\t"                   \
  "shll.ph    %[temp0],  %[temp0],    2                \n\t"                   \
  "shll.ph    %[temp1],  %[temp1],    2                \n\t"                   \
  "shll.ph    %[temp2],  %[temp2],    2                \n\t"                   \
  "shll.ph    %[temp3],  %[temp3],    2                \n\t"                   \
  "ext        %[temp4],  %[temp0],    0,       16      \n\t"                   \
  "ext        %[temp0],  %[temp0],    16,      16      \n\t"                   \
  "addu       %[temp4],  %[temp4],    %[dist]          \n\t"                   \
  "addu       %[temp0],  %[temp0],    %[dist]          \n\t"                   \
  "ext        %[temp5],  %[temp1],    0,       16      \n\t"                   \
  "lw         %[temp8],  0(%[temp4])                   \n\t"                   \
  "ext        %[temp1],  %[temp1],    16,      16      \n\t"                   \
  "addu       %[temp5],  %[temp5],    %[dist]          \n\t"                   \
  "addiu      %[temp8],  %[temp8],    1                \n\t"                   \
  "sw         %[temp8],  0(%[temp4])                   \n\t"                   \
  "lw         %[temp8],  0(%[temp0])                   \n\t"                   \
  "addu       %[temp1],  %[temp1],    %[dist]          \n\t"                   \
  "ext        %[temp6],  %[temp2],    0,       16      \n\t"                   \
  "addiu      %[temp8],  %[temp8],    1                \n\t"                   \
  "sw         %[temp8],  0(%[temp0])                   \n\t"                   \
  "lw         %[temp8],  0(%[temp5])                   \n\t"                   \
  "ext        %[temp2],  %[temp2],    16,      16      \n\t"                   \
  "addu       %[temp6],  %[temp6],    %[dist]          \n\t"                   \
  "addiu      %[temp8],  %[temp8],    1                \n\t"                   \
  "sw         %[temp8],  0(%[temp5])                   \n\t"                   \
  "lw         %[temp8],  0(%[temp1])                   \n\t"                   \
  "addu       %[temp2],  %[temp2],    %[dist]          \n\t"                   \
  "ext        %[temp7],  %[temp3],    0,       16      \n\t"                   \
  "addiu      %[temp8],  %[temp8],    1                \n\t"                   \
  "sw         %[temp8],  0(%[temp1])                   \n\t"                   \
  "lw         %[temp8],  0(%[temp6])                   \n\t"                   \
  "ext        %[temp3],  %[temp3],    16,      16      \n\t"                   \
  "addu       %[temp7],  %[temp7],    %[dist]          \n\t"                   \
  "addiu      %[temp8],  %[temp8],    1                \n\t"                   \
  "sw         %[temp8],  0(%[temp6])                   \n\t"                   \
  "lw         %[temp8],  0(%[temp2])                   \n\t"                   \
  "addu       %[temp3],  %[temp3],    %[dist]          \n\t"                   \
  "addiu      %[temp8],  %[temp8],    1                \n\t"                   \
  "sw         %[temp8],  0(%[temp2])                   \n\t"                   \
  "lw         %[temp8],  0(%[temp7])                   \n\t"                   \
  "addiu      %[temp8],  %[temp8],    1                \n\t"                   \
  "sw         %[temp8],  0(%[temp7])                   \n\t"                   \
  "lw         %[temp8],  0(%[temp3])                   \n\t"                   \
  "addiu      %[temp8],  %[temp8],    1                \n\t"                   \
  "sw         %[temp8],  0(%[temp3])                   \n\t"

static void CollectHistogram_MIPSdspR2(const uint8_t* ref, const uint8_t* pred,
                                       int start_block, int end_block,
                                       VP8Histogram* const histo) {
  int j;
  int distribution[MAX_COEFF_THRESH + 1] = { 0 };
  const int max_coeff = (MAX_COEFF_THRESH << 16) + MAX_COEFF_THRESH;
  for (j = start_block; j < end_block; ++j) {
    int16_t out[16];
    int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7, temp8;

    VP8FTransform(ref + VP8DspScan[j], pred + VP8DspScan[j], out);

    // Convert coefficients to bin.
    __asm__ volatile (
      CONVERT_COEFFS_TO_BIN( 0,  4,  8, 12)
      CONVERT_COEFFS_TO_BIN(16, 20, 24, 28)
      : [temp0]"=&r"(temp0), [temp1]"=&r"(temp1), [temp2]"=&r"(temp2),
        [temp3]"=&r"(temp3), [temp4]"=&r"(temp4), [temp5]"=&r"(temp5),
        [temp6]"=&r"(temp6), [temp7]"=&r"(temp7), [temp8]"=&r"(temp8)
      : [dist]"r"(distribution), [out]"r"(out), [max_coeff]"r"(max_coeff)
      : "memory"
    );
  }
  VP8SetHistogramData(distribution, histo);
}

#undef CONVERT_COEFFS_TO_BIN

//------------------------------------------------------------------------------
// Entry point

extern void VP8EncDspInitMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8EncDspInitMIPSdspR2(void) {
  VP8FTransform = FTransform_MIPSdspR2;
  VP8FTransformWHT = FTransformWHT_MIPSdspR2;
  VP8ITransform = ITransform_MIPSdspR2;

  VP8TDisto4x4 = Disto4x4_MIPSdspR2;
  VP8TDisto16x16 = Disto16x16_MIPSdspR2;

  VP8EncPredLuma16 = Intra16Preds_MIPSdspR2;
  VP8EncPredChroma8 = IntraChromaPreds_MIPSdspR2;
  VP8EncPredLuma4 = Intra4Preds_MIPSdspR2;

#if !defined(WORK_AROUND_GCC)
  VP8SSE16x16 = SSE16x16_MIPSdspR2;
  VP8SSE8x8 = SSE8x8_MIPSdspR2;
  VP8SSE16x8 = SSE16x8_MIPSdspR2;
  VP8SSE4x4 = SSE4x4_MIPSdspR2;
#endif

  VP8EncQuantizeBlock = QuantizeBlock_MIPSdspR2;
  VP8EncQuantize2Blocks = Quantize2Blocks_MIPSdspR2;

  VP8CollectHistogram = CollectHistogram_MIPSdspR2;
}

#else  // !WEBP_USE_MIPS_DSP_R2

WEBP_DSP_INIT_STUB(VP8EncDspInitMIPSdspR2)

#endif  // WEBP_USE_MIPS_DSP_R2
