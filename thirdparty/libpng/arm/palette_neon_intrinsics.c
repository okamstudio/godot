/* palette_neon_intrinsics.c - NEON optimised palette expansion functions
 *
 * Copyright (c) 2018-2019 Cosmin Truta
 * Copyright (c) 2017-2018 Arm Holdings. All rights reserved.
 * Written by Richard Townsend <Richard.Townsend@arm.com>, February 2017.
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

#include "../pngpriv.h"

#if PNG_ARM_NEON_IMPLEMENTATION == 1

#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64)
#  include <arm64_neon.h>
#else
#  include <arm_neon.h>
#endif

/* Build an RGBA8 palette from the separate RGB and alpha palettes. */
void
png_riffle_palette_neon(png_structrp png_ptr)
{
   png_const_colorp palette = png_ptr->palette;
   png_bytep riffled_palette = png_ptr->riffled_palette;
   png_const_bytep trans_alpha = png_ptr->trans_alpha;
   int num_trans = png_ptr->num_trans;
   int i;

   /* Initially black, opaque. */
   uint8x16x4_t w = {{
      vdupq_n_u8(0x00),
      vdupq_n_u8(0x00),
      vdupq_n_u8(0x00),
      vdupq_n_u8(0xff),
   }};

   png_debug(1, "in png_riffle_palette_neon");

   /* First, riffle the RGB colours into an RGBA8 palette.
    * The alpha component is set to opaque for now.
    */
   for (i = 0; i < 256; i += 16)
   {
      uint8x16x3_t v = vld3q_u8((png_const_bytep)(palette + i));
      w.val[0] = v.val[0];
      w.val[1] = v.val[1];
      w.val[2] = v.val[2];
      vst4q_u8(riffled_palette + (i << 2), w);
   }

   /* Fix up the missing transparency values. */
   for (i = 0; i < num_trans; i++)
      riffled_palette[(i << 2) + 3] = trans_alpha[i];
}

/* Expands a palettized row into RGBA8. */
int
png_do_expand_palette_rgba8_neon(png_structrp png_ptr, png_row_infop row_info,
    png_const_bytep row, png_bytepp ssp, png_bytepp ddp)
{
   png_uint_32 row_width = row_info->width;
   const png_uint_32 *riffled_palette =
      png_aligncastconst(png_const_uint_32p, png_ptr->riffled_palette);
   const png_uint_32 pixels_per_chunk = 4;
   png_uint_32 i;

   png_debug(1, "in png_do_expand_palette_rgba8_neon");

   PNG_UNUSED(row)
   if (row_width < pixels_per_chunk)
      return 0;

   /* This function originally gets the last byte of the output row.
    * The NEON part writes forward from a given position, so we have
    * to seek this back by 4 pixels x 4 bytes.
    */
   *ddp = *ddp - ((pixels_per_chunk * sizeof(png_uint_32)) - 1);

   for (i = 0; i < row_width; i += pixels_per_chunk)
   {
      uint32x4_t cur;
      png_bytep sp = *ssp - i, dp = *ddp - (i << 2);
      cur = vld1q_dup_u32 (riffled_palette + *(sp - 3));
      cur = vld1q_lane_u32(riffled_palette + *(sp - 2), cur, 1);
      cur = vld1q_lane_u32(riffled_palette + *(sp - 1), cur, 2);
      cur = vld1q_lane_u32(riffled_palette + *(sp - 0), cur, 3);
      vst1q_u32((void *)dp, cur);
   }
   if (i != row_width)
   {
      /* Remove the amount that wasn't processed. */
      i -= pixels_per_chunk;
   }

   /* Decrement output pointers. */
   *ssp = *ssp - i;
   *ddp = *ddp - (i << 2);
   return i;
}

/* Expands a palettized row into RGB8. */
int
png_do_expand_palette_rgb8_neon(png_structrp png_ptr, png_row_infop row_info,
    png_const_bytep row, png_bytepp ssp, png_bytepp ddp)
{
   png_uint_32 row_width = row_info->width;
   png_const_bytep palette = (png_const_bytep)png_ptr->palette;
   const png_uint_32 pixels_per_chunk = 8;
   png_uint_32 i;

   png_debug(1, "in png_do_expand_palette_rgb8_neon");

   PNG_UNUSED(row)
   if (row_width <= pixels_per_chunk)
      return 0;

   /* Seeking this back by 8 pixels x 3 bytes. */
   *ddp = *ddp - ((pixels_per_chunk * sizeof(png_color)) - 1);

   for (i = 0; i < row_width; i += pixels_per_chunk)
   {
      uint8x8x3_t cur;
      png_bytep sp = *ssp - i, dp = *ddp - ((i << 1) + i);
      cur = vld3_dup_u8(palette + sizeof(png_color) * (*(sp - 7)));
      cur = vld3_lane_u8(palette + sizeof(png_color) * (*(sp - 6)), cur, 1);
      cur = vld3_lane_u8(palette + sizeof(png_color) * (*(sp - 5)), cur, 2);
      cur = vld3_lane_u8(palette + sizeof(png_color) * (*(sp - 4)), cur, 3);
      cur = vld3_lane_u8(palette + sizeof(png_color) * (*(sp - 3)), cur, 4);
      cur = vld3_lane_u8(palette + sizeof(png_color) * (*(sp - 2)), cur, 5);
      cur = vld3_lane_u8(palette + sizeof(png_color) * (*(sp - 1)), cur, 6);
      cur = vld3_lane_u8(palette + sizeof(png_color) * (*(sp - 0)), cur, 7);
      vst3_u8((void *)dp, cur);
   }

   if (i != row_width)
   {
      /* Remove the amount that wasn't processed. */
      i -= pixels_per_chunk;
   }

   /* Decrement output pointers. */
   *ssp = *ssp - i;
   *ddp = *ddp - ((i << 1) + i);
   return i;
}

#endif /* PNG_ARM_NEON_IMPLEMENTATION */
