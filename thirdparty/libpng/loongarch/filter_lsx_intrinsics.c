/* filter_lsx_intrinsics.c - LSX optimized filter functions
 *
 * Copyright (c) 2021 Loongson Technology Corporation Limited
 * All rights reserved.
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2016 Glenn Randers-Pehrson
 * Contributed by Jin Bo (jinbo@loongson.cn)
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

#include "../pngpriv.h"

#ifdef PNG_READ_SUPPORTED

#if PNG_LOONGARCH_LSX_IMPLEMENTATION == 1 /* intrinsics code from pngpriv.h */

#include <lsxintrin.h>

#define LSX_LD(psrc) __lsx_vld((psrc), 0)

#define LSX_LD_2(psrc, stride, out0, out1) \
{                                          \
   out0 = LSX_LD(psrc);                    \
   out1 = LSX_LD(psrc + stride);           \
}

#define LSX_LD_4(psrc, stride, out0, out1, out2, out3) \
{                                                      \
   LSX_LD_2(psrc, stride, out0, out1);                 \
   LSX_LD_2(psrc + stride * 2, stride, out2, out3);    \
}

#define LSX_ST(in, pdst) __lsx_vst(in, (pdst), 0)

#define LSX_ST_2(in0, in1, pdst, stride) \
{                                        \
   LSX_ST(in0, pdst);                    \
   LSX_ST(in1, pdst + stride);           \
}

#define LSX_ST_4(in0, in1, in2, in3, pdst, stride) \
{                                                  \
   LSX_ST_2(in0, in1, pdst, stride);               \
   LSX_ST_2(in2, in3, pdst + stride * 2, stride);  \
}

#define LSX_ADD_B(in0, in1, out0) \
{                                 \
   out0 = __lsx_vadd_b(in0, in1); \
}

#define LSX_ADD_B_2(in0, in1, in2, in3, out0, out1) \
{                                                   \
   LSX_ADD_B(in0, in1, out0);                       \
   LSX_ADD_B(in2, in3, out1);                       \
}

#define LSX_ADD_B_4(in0, in1, in2, in3, in4, in5,     \
                    in6, in7, out0, out1, out2, out3) \
{                                                     \
   LSX_ADD_B_2(in0, in1, in2, in3, out0, out1);       \
   LSX_ADD_B_2(in4, in5, in6, in7, out2, out3);       \
}

#define LSX_ABS_B_3(in0, in1, in2, out0, out1, out2) \
{                                                    \
   out0 = __lsx_vadda_h(in0, zero);                  \
   out1 = __lsx_vadda_h(in1, zero);                  \
   out2 = __lsx_vadda_h(in2, zero);                  \
}

#define LSX_ILVL_B(in_h, in_l, out0)  \
{                                     \
   out0 = __lsx_vilvl_b(in_h, in_l);  \
}

#define LSX_ILVL_B_2(in0_h, in0_l, in1_h, in1_l, out0, out1) \
{                                                            \
   LSX_ILVL_B(in0_h, in0_l, out0);                           \
   LSX_ILVL_B(in1_h, in1_l, out1);                           \
}

#define LSX_HSUB_HU_BU_2(in0, in1, out0, out1) \
{                                              \
   out0 = __lsx_vhsubw_hu_bu(in0, in0);        \
   out1 = __lsx_vhsubw_hu_bu(in1, in1);        \
}

#define LSX_CMP_PICK_SMALLER(in0, in1, in2, in3, in4, in5, out0) \
{                                                                \
   __m128i _cmph, _cmpb, _in0, _in3;                             \
   _cmph = __lsx_vslt_h(in1, in0);                               \
   _cmpb = __lsx_vpickev_b(_cmph, _cmph);                        \
   _in0  = __lsx_vmin_bu(in0,in1);                               \
   _in3  = __lsx_vbitsel_v(in3, in4, _cmpb);                     \
   _cmph = __lsx_vslt_h(in2, _in0);                              \
   _cmpb = __lsx_vpickev_b(_cmph, _cmph);                        \
   _in3  = __lsx_vbitsel_v(_in3, in5, _cmpb);                    \
   out0  = __lsx_vadd_b(out0, _in3);                             \
}

void png_read_filter_row_up_lsx(png_row_infop row_info, png_bytep row,
                                png_const_bytep prev_row)
{
   size_t n = row_info->rowbytes;
   png_bytep rp = row;
   png_const_bytep pp = prev_row;
   __m128i vec_0, vec_1, vec_2, vec_3;
   __m128i vec_4, vec_5, vec_6, vec_7;

   while (n >= 64)
   {
      LSX_LD_4(rp, 16, vec_0, vec_1, vec_2, vec_3);
      LSX_LD_4(pp, 16, vec_4, vec_5, vec_6, vec_7);
      pp += 64;
      LSX_ADD_B_4(vec_0 ,vec_4, vec_1, vec_5, vec_2, vec_6,
                  vec_3, vec_7, vec_0, vec_1, vec_2, vec_3);
      LSX_ST_4(vec_0, vec_1, vec_2, vec_3, rp, 16);
      rp += 64;
      n -= 64;
   }
   if (n & 63)
   {
      if (n >= 32)
      {
         LSX_LD_2(rp, 16, vec_0, vec_1);
         LSX_LD_2(pp, 16, vec_2, vec_3);
         pp += 32;
         LSX_ADD_B_2(vec_0, vec_2, vec_1, vec_3, vec_0, vec_1);
         LSX_ST_2(vec_0, vec_1, rp, 16);
         rp += 32;
         n -= 32;
      }
      if (n & 31)
      {
         if (n >= 16)
         {
            vec_0 = LSX_LD(rp);
            vec_1 = LSX_LD(pp);
            pp += 16;
            LSX_ADD_B(vec_0, vec_1, vec_0);
            LSX_ST(vec_0, rp);
            rp += 16;
            n -= 16;
         }
         if (n >= 8)
         {
            vec_0 = __lsx_vldrepl_d(rp, 0);
            vec_1 = __lsx_vldrepl_d(pp, 0);
            vec_0 = __lsx_vadd_b(vec_0, vec_1);
            __lsx_vstelm_d(vec_0, rp, 0, 0);
            rp += 8;
            pp += 8;
            n -= 8;
         }
         while (n--)
         {
            *rp = *rp + *pp++;
            rp++;
         }
      }
   }
}

void png_read_filter_row_sub3_lsx(png_row_infop row_info, png_bytep row,
                                  png_const_bytep prev_row)
{
   size_t n = row_info->rowbytes;
   png_uint_32 tmp;
   png_bytep nxt = row;
   __m128i vec_0, vec_1;

   PNG_UNUSED(prev_row);

   vec_0 = __lsx_vldrepl_w(nxt, 0);
   nxt += 3;
   n -= 3;

   while (n >= 3)
   {
      vec_1 = __lsx_vldrepl_w(nxt, 0);
      vec_1 = __lsx_vadd_b(vec_1, vec_0);
      __lsx_vstelm_h(vec_1, nxt, 0, 0);
      vec_0 = vec_1;
      nxt += 2;
      __lsx_vstelm_b(vec_1, nxt, 0, 2);
      nxt += 1;
      n -= 3;
   }

   row = nxt - 3;
   while (n--)
   {
      *nxt = *nxt + *row++;
      nxt++;
   }
}

void png_read_filter_row_sub4_lsx(png_row_infop row_info, png_bytep row,
                                  png_const_bytep prev_row)
{
   size_t n = row_info->rowbytes;
   __m128i vec_0, vec_1;

   PNG_UNUSED(prev_row);

   vec_0 = __lsx_vldrepl_w(row, 0);
   row += 4;
   n -= 4;

   while (n >= 4)
   {
      vec_1 = __lsx_vldrepl_w(row, 0);
      vec_1 = __lsx_vadd_b(vec_1, vec_0);
      __lsx_vstelm_w(vec_1, row, 0, 0);
      vec_0 = vec_1;
      row += 4;
      n -= 4;
   }
}

void png_read_filter_row_avg3_lsx(png_row_infop row_info, png_bytep row,
                                  png_const_bytep prev_row)
{
   size_t n = row_info->rowbytes;
   png_bytep nxt = row;
   png_const_bytep prev_nxt = prev_row;
   __m128i vec_0, vec_1, vec_2;

   vec_0 = __lsx_vldrepl_w(nxt, 0);
   vec_1 = __lsx_vldrepl_w(prev_nxt, 0);
   prev_nxt += 3;
   vec_1 = __lsx_vsrli_b(vec_1, 1);
   vec_1 = __lsx_vadd_b(vec_1, vec_0);
   __lsx_vstelm_h(vec_1, nxt, 0, 0);
   nxt += 2;
   __lsx_vstelm_b(vec_1, nxt, 0, 2);
   nxt += 1;
   n -= 3;

   while (n >= 3)
   {
      vec_2 = vec_1;
      vec_0 = __lsx_vldrepl_w(nxt, 0);
      vec_1 = __lsx_vldrepl_w(prev_nxt, 0);
      prev_nxt += 3;

      vec_1 = __lsx_vavg_bu(vec_1, vec_2);
      vec_1 = __lsx_vadd_b(vec_1, vec_0);

      __lsx_vstelm_h(vec_1, nxt, 0, 0);
      nxt += 2;
      __lsx_vstelm_b(vec_1, nxt, 0, 2);
      nxt += 1;
      n -= 3;
   }

   row = nxt - 3;
   while (n--)
   {
      vec_2 = __lsx_vldrepl_b(row, 0);
      row++;
      vec_0 = __lsx_vldrepl_b(nxt, 0);
      vec_1 = __lsx_vldrepl_b(prev_nxt, 0);
      prev_nxt++;

      vec_1 = __lsx_vavg_bu(vec_1, vec_2);
      vec_1 = __lsx_vadd_b(vec_1, vec_0);

      __lsx_vstelm_b(vec_1, nxt, 0, 0);
      nxt++;
   }
}

void png_read_filter_row_avg4_lsx(png_row_infop row_info, png_bytep row,
                                  png_const_bytep prev_row)
{
   size_t n = row_info->rowbytes;
   __m128i vec_0, vec_1, vec_2;

   vec_0 = __lsx_vldrepl_w(row, 0);
   vec_1 = __lsx_vldrepl_w(prev_row, 0);
   prev_row += 4;
   vec_1 = __lsx_vsrli_b(vec_1, 1);
   vec_1 = __lsx_vadd_b(vec_1, vec_0);
   __lsx_vstelm_w(vec_1, row, 0, 0);
   row += 4;
   n -= 4;

   while (n >= 4)
   {
      vec_2 = vec_1;
      vec_0 = __lsx_vldrepl_w(row, 0);
      vec_1 = __lsx_vldrepl_w(prev_row, 0);
      prev_row += 4;

      vec_1 = __lsx_vavg_bu(vec_1, vec_2);
      vec_1 = __lsx_vadd_b(vec_1, vec_0);

      __lsx_vstelm_w(vec_1, row, 0, 0);
      row += 4;
      n -= 4;
   }
}

void png_read_filter_row_paeth3_lsx(png_row_infop row_info,
                                    png_bytep row,
                                    png_const_bytep prev_row)
{
   size_t n = row_info->rowbytes;
   png_bytep nxt = row;
   png_const_bytep prev_nxt = prev_row;
   __m128i vec_a, vec_b, vec_c, vec_d;
   __m128i vec_pa, vec_pb, vec_pc;
   __m128i zero = {0};

   vec_a = __lsx_vldrepl_w(nxt, 0);
   vec_b = __lsx_vldrepl_w(prev_nxt, 0);
   prev_nxt += 3;
   vec_d = __lsx_vadd_b(vec_a, vec_b);
   __lsx_vstelm_h(vec_d, nxt, 0, 0);
   nxt += 2;
   __lsx_vstelm_b(vec_d, nxt, 0, 2);
   nxt += 1;
   n -= 3;

   while (n >= 3)
   {
      vec_a = vec_d;
      vec_c = vec_b;
      vec_b = __lsx_vldrepl_w(prev_nxt, 0);
      prev_nxt += 3;
      vec_d = __lsx_vldrepl_w(nxt, 0);

      LSX_ILVL_B_2(vec_b, vec_c, vec_a, vec_c, vec_pa, vec_pb);
      LSX_HSUB_HU_BU_2(vec_pa, vec_pb, vec_pa, vec_pb);
      vec_pc = __lsx_vadd_h(vec_pa, vec_pb);
      LSX_ABS_B_3(vec_pa, vec_pb, vec_pc, vec_pa, vec_pb, vec_pc);
      LSX_CMP_PICK_SMALLER(vec_pa, vec_pb, vec_pc, vec_a, vec_b, vec_c, vec_d);

      __lsx_vstelm_h(vec_d, nxt, 0, 0);
      nxt += 2;
      __lsx_vstelm_b(vec_d, nxt, 0, 2);
      nxt += 1;
      n -= 3;
   }

   prev_row = prev_nxt - 3;
   row = nxt - 3;
   while (n--)
   {
      vec_a = __lsx_vldrepl_b(row, 0);
      row++;
      vec_b = __lsx_vldrepl_b(prev_nxt, 0);
      prev_nxt++;
      vec_c = __lsx_vldrepl_b(prev_row, 0);
      prev_row++;
      vec_d = __lsx_vldrepl_b(nxt, 0);

      LSX_ILVL_B_2(vec_b, vec_c, vec_a, vec_c, vec_pa, vec_pb);
      LSX_HSUB_HU_BU_2(vec_pa, vec_pb, vec_pa, vec_pb);
      vec_pc = __lsx_vadd_h(vec_pa, vec_pb);
      LSX_ABS_B_3(vec_pa, vec_pb, vec_pc, vec_pa, vec_pb, vec_pc);
      LSX_CMP_PICK_SMALLER(vec_pa, vec_pb, vec_pc, vec_a, vec_b, vec_c, vec_d);

      __lsx_vstelm_b(vec_d, nxt, 0, 0);
      nxt++;
   }
}

void png_read_filter_row_paeth4_lsx(png_row_infop row_info,
                                    png_bytep row,
                                    png_const_bytep prev_row)
{
   size_t n = row_info->rowbytes;
   __m128i vec_a, vec_b, vec_c, vec_d;
   __m128i vec_pa, vec_pb, vec_pc;
   __m128i zero = {0};

   vec_a = __lsx_vldrepl_w(row, 0);
   vec_b = __lsx_vldrepl_w(prev_row, 0);
   prev_row += 4;
   vec_d = __lsx_vadd_b(vec_a, vec_b);
   __lsx_vstelm_w(vec_d, row, 0, 0);
   row += 4;
   n -= 4;

   while (n >= 4)
   {
      vec_a = vec_d;
      vec_c = vec_b;
      vec_b = __lsx_vldrepl_w(prev_row, 0);
      prev_row += 4;
      vec_d = __lsx_vldrepl_w(row, 0);

      LSX_ILVL_B_2(vec_b, vec_c, vec_a, vec_c, vec_pa, vec_pb);
      LSX_HSUB_HU_BU_2(vec_pa, vec_pb, vec_pa, vec_pb);
      vec_pc = __lsx_vadd_h(vec_pa, vec_pb);
      LSX_ABS_B_3(vec_pa, vec_pb, vec_pc, vec_pa, vec_pb, vec_pc);
      LSX_CMP_PICK_SMALLER(vec_pa, vec_pb, vec_pc, vec_a, vec_b, vec_c, vec_d);

      __lsx_vstelm_w(vec_d, row, 0, 0);
      row += 4;
      n -= 4;
   }
}

#endif /* PNG_LOONGARCH_LSX_IMPLEMENTATION == 1 (intrinsics) */
#endif /* PNG_READ_SUPPORTED */
