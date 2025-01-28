/* filter_sse2_intrinsics.c - SSE2 optimized filter functions
 *
 * Copyright (c) 2018 Cosmin Truta
 * Copyright (c) 2016-2017 Glenn Randers-Pehrson
 * Written by Mike Klein and Matt Sarett
 * Derived from arm/filter_neon_intrinsics.c
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 */

#include "../pngpriv.h"

#ifdef PNG_READ_SUPPORTED

#if PNG_INTEL_SSE_IMPLEMENTATION > 0

#include <immintrin.h>

/* Functions in this file look at most 3 pixels (a,b,c) to predict the 4th (d).
 * They're positioned like this:
 *    prev:  c b
 *    row:   a d
 * The Sub filter predicts d=a, Avg d=(a+b)/2, and Paeth predicts d to be
 * whichever of a, b, or c is closest to p=a+b-c.
 */

static __m128i load4(const void* p) {
   int tmp;
   memcpy(&tmp, p, sizeof(tmp));
   return _mm_cvtsi32_si128(tmp);
}

static void store4(void* p, __m128i v) {
   int tmp = _mm_cvtsi128_si32(v);
   memcpy(p, &tmp, sizeof(int));
}

static __m128i load3(const void* p) {
   png_uint_32 tmp = 0;
   memcpy(&tmp, p, 3);
   return _mm_cvtsi32_si128(tmp);
}

static void store3(void* p, __m128i v) {
   int tmp = _mm_cvtsi128_si32(v);
   memcpy(p, &tmp, 3);
}

void png_read_filter_row_sub3_sse2(png_row_infop row_info, png_bytep row,
   png_const_bytep prev)
{
   /* The Sub filter predicts each pixel as the previous pixel, a.
    * There is no pixel to the left of the first pixel.  It's encoded directly.
    * That works with our main loop if we just say that left pixel was zero.
    */
   size_t rb;

   __m128i a, d = _mm_setzero_si128();

   png_debug(1, "in png_read_filter_row_sub3_sse2");

   rb = row_info->rowbytes;
   while (rb >= 4) {
      a = d; d = load4(row);
      d = _mm_add_epi8(d, a);
      store3(row, d);

      row += 3;
      rb  -= 3;
   }
   if (rb > 0) {
      a = d; d = load3(row);
      d = _mm_add_epi8(d, a);
      store3(row, d);

      row += 3;
      rb  -= 3;
   }
   PNG_UNUSED(prev)
}

void png_read_filter_row_sub4_sse2(png_row_infop row_info, png_bytep row,
   png_const_bytep prev)
{
   /* The Sub filter predicts each pixel as the previous pixel, a.
    * There is no pixel to the left of the first pixel.  It's encoded directly.
    * That works with our main loop if we just say that left pixel was zero.
    */
   size_t rb;

   __m128i a, d = _mm_setzero_si128();

   png_debug(1, "in png_read_filter_row_sub4_sse2");

   rb = row_info->rowbytes+4;
   while (rb > 4) {
      a = d; d = load4(row);
      d = _mm_add_epi8(d, a);
      store4(row, d);

      row += 4;
      rb  -= 4;
   }
   PNG_UNUSED(prev)
}

void png_read_filter_row_avg3_sse2(png_row_infop row_info, png_bytep row,
   png_const_bytep prev)
{
   /* The Avg filter predicts each pixel as the (truncated) average of a and b.
    * There's no pixel to the left of the first pixel.  Luckily, it's
    * predicted to be half of the pixel above it.  So again, this works
    * perfectly with our loop if we make sure a starts at zero.
    */

   size_t rb;

   const __m128i zero = _mm_setzero_si128();

   __m128i    b;
   __m128i a, d = zero;

   png_debug(1, "in png_read_filter_row_avg3_sse2");
   rb = row_info->rowbytes;
   while (rb >= 4) {
      __m128i avg;
             b = load4(prev);
      a = d; d = load4(row );

      /* PNG requires a truncating average, so we can't just use _mm_avg_epu8 */
      avg = _mm_avg_epu8(a,b);
      /* ...but we can fix it up by subtracting off 1 if it rounded up. */
      avg = _mm_sub_epi8(avg, _mm_and_si128(_mm_xor_si128(a,b),
                                            _mm_set1_epi8(1)));
      d = _mm_add_epi8(d, avg);
      store3(row, d);

      prev += 3;
      row  += 3;
      rb   -= 3;
   }
   if (rb > 0) {
      __m128i avg;
             b = load3(prev);
      a = d; d = load3(row );

      /* PNG requires a truncating average, so we can't just use _mm_avg_epu8 */
      avg = _mm_avg_epu8(a,b);
      /* ...but we can fix it up by subtracting off 1 if it rounded up. */
      avg = _mm_sub_epi8(avg, _mm_and_si128(_mm_xor_si128(a,b),
                                            _mm_set1_epi8(1)));

      d = _mm_add_epi8(d, avg);
      store3(row, d);

      prev += 3;
      row  += 3;
      rb   -= 3;
   }
}

void png_read_filter_row_avg4_sse2(png_row_infop row_info, png_bytep row,
   png_const_bytep prev)
{
   /* The Avg filter predicts each pixel as the (truncated) average of a and b.
    * There's no pixel to the left of the first pixel.  Luckily, it's
    * predicted to be half of the pixel above it.  So again, this works
    * perfectly with our loop if we make sure a starts at zero.
    */
   size_t rb;
   const __m128i zero = _mm_setzero_si128();
   __m128i    b;
   __m128i a, d = zero;

   png_debug(1, "in png_read_filter_row_avg4_sse2");

   rb = row_info->rowbytes+4;
   while (rb > 4) {
      __m128i avg;
             b = load4(prev);
      a = d; d = load4(row );

      /* PNG requires a truncating average, so we can't just use _mm_avg_epu8 */
      avg = _mm_avg_epu8(a,b);
      /* ...but we can fix it up by subtracting off 1 if it rounded up. */
      avg = _mm_sub_epi8(avg, _mm_and_si128(_mm_xor_si128(a,b),
                                            _mm_set1_epi8(1)));

      d = _mm_add_epi8(d, avg);
      store4(row, d);

      prev += 4;
      row  += 4;
      rb   -= 4;
   }
}

/* Returns |x| for 16-bit lanes. */
static __m128i abs_i16(__m128i x) {
#if PNG_INTEL_SSE_IMPLEMENTATION >= 2
   return _mm_abs_epi16(x);
#else
   /* Read this all as, return x<0 ? -x : x.
   * To negate two's complement, you flip all the bits then add 1.
    */
   __m128i is_negative = _mm_cmplt_epi16(x, _mm_setzero_si128());

   /* Flip negative lanes. */
   x = _mm_xor_si128(x, is_negative);

   /* +1 to negative lanes, else +0. */
   x = _mm_sub_epi16(x, is_negative);
   return x;
#endif
}

/* Bytewise c ? t : e. */
static __m128i if_then_else(__m128i c, __m128i t, __m128i e) {
#if PNG_INTEL_SSE_IMPLEMENTATION >= 3
   return _mm_blendv_epi8(e,t,c);
#else
   return _mm_or_si128(_mm_and_si128(c, t), _mm_andnot_si128(c, e));
#endif
}

void png_read_filter_row_paeth3_sse2(png_row_infop row_info, png_bytep row,
   png_const_bytep prev)
{
   /* Paeth tries to predict pixel d using the pixel to the left of it, a,
    * and two pixels from the previous row, b and c:
    *   prev: c b
    *   row:  a d
    * The Paeth function predicts d to be whichever of a, b, or c is nearest to
    * p=a+b-c.
    *
    * The first pixel has no left context, and so uses an Up filter, p = b.
    * This works naturally with our main loop's p = a+b-c if we force a and c
    * to zero.
    * Here we zero b and d, which become c and a respectively at the start of
    * the loop.
    */
   size_t rb;
   const __m128i zero = _mm_setzero_si128();
   __m128i c, b = zero,
           a, d = zero;

   png_debug(1, "in png_read_filter_row_paeth3_sse2");

   rb = row_info->rowbytes;
   while (rb >= 4) {
      /* It's easiest to do this math (particularly, deal with pc) with 16-bit
       * intermediates.
       */
      __m128i pa,pb,pc,smallest,nearest;
      c = b; b = _mm_unpacklo_epi8(load4(prev), zero);
      a = d; d = _mm_unpacklo_epi8(load4(row ), zero);

      /* (p-a) == (a+b-c - a) == (b-c) */

      pa = _mm_sub_epi16(b,c);

      /* (p-b) == (a+b-c - b) == (a-c) */
      pb = _mm_sub_epi16(a,c);

      /* (p-c) == (a+b-c - c) == (a+b-c-c) == (b-c)+(a-c) */
      pc = _mm_add_epi16(pa,pb);

      pa = abs_i16(pa);  /* |p-a| */
      pb = abs_i16(pb);  /* |p-b| */
      pc = abs_i16(pc);  /* |p-c| */

      smallest = _mm_min_epi16(pc, _mm_min_epi16(pa, pb));

      /* Paeth breaks ties favoring a over b over c. */
      nearest  = if_then_else(_mm_cmpeq_epi16(smallest, pa), a,
                 if_then_else(_mm_cmpeq_epi16(smallest, pb), b,
                                                             c));

      /* Note `_epi8`: we need addition to wrap modulo 255. */
      d = _mm_add_epi8(d, nearest);
      store3(row, _mm_packus_epi16(d,d));

      prev += 3;
      row  += 3;
      rb   -= 3;
   }
   if (rb > 0) {
      /* It's easiest to do this math (particularly, deal with pc) with 16-bit
       * intermediates.
       */
      __m128i pa,pb,pc,smallest,nearest;
      c = b; b = _mm_unpacklo_epi8(load3(prev), zero);
      a = d; d = _mm_unpacklo_epi8(load3(row ), zero);

      /* (p-a) == (a+b-c - a) == (b-c) */
      pa = _mm_sub_epi16(b,c);

      /* (p-b) == (a+b-c - b) == (a-c) */
      pb = _mm_sub_epi16(a,c);

      /* (p-c) == (a+b-c - c) == (a+b-c-c) == (b-c)+(a-c) */
      pc = _mm_add_epi16(pa,pb);

      pa = abs_i16(pa);  /* |p-a| */
      pb = abs_i16(pb);  /* |p-b| */
      pc = abs_i16(pc);  /* |p-c| */

      smallest = _mm_min_epi16(pc, _mm_min_epi16(pa, pb));

      /* Paeth breaks ties favoring a over b over c. */
      nearest  = if_then_else(_mm_cmpeq_epi16(smallest, pa), a,
                         if_then_else(_mm_cmpeq_epi16(smallest, pb), b,
                                                                     c));

      /* Note `_epi8`: we need addition to wrap modulo 255. */
      d = _mm_add_epi8(d, nearest);
      store3(row, _mm_packus_epi16(d,d));

      prev += 3;
      row  += 3;
      rb   -= 3;
   }
}

void png_read_filter_row_paeth4_sse2(png_row_infop row_info, png_bytep row,
   png_const_bytep prev)
{
   /* Paeth tries to predict pixel d using the pixel to the left of it, a,
    * and two pixels from the previous row, b and c:
    *   prev: c b
    *   row:  a d
    * The Paeth function predicts d to be whichever of a, b, or c is nearest to
    * p=a+b-c.
    *
    * The first pixel has no left context, and so uses an Up filter, p = b.
    * This works naturally with our main loop's p = a+b-c if we force a and c
    * to zero.
    * Here we zero b and d, which become c and a respectively at the start of
    * the loop.
    */
   size_t rb;
   const __m128i zero = _mm_setzero_si128();
   __m128i pa,pb,pc,smallest,nearest;
   __m128i c, b = zero,
           a, d = zero;

   png_debug(1, "in png_read_filter_row_paeth4_sse2");

   rb = row_info->rowbytes+4;
   while (rb > 4) {
      /* It's easiest to do this math (particularly, deal with pc) with 16-bit
       * intermediates.
       */
      c = b; b = _mm_unpacklo_epi8(load4(prev), zero);
      a = d; d = _mm_unpacklo_epi8(load4(row ), zero);

      /* (p-a) == (a+b-c - a) == (b-c) */
      pa = _mm_sub_epi16(b,c);

      /* (p-b) == (a+b-c - b) == (a-c) */
      pb = _mm_sub_epi16(a,c);

      /* (p-c) == (a+b-c - c) == (a+b-c-c) == (b-c)+(a-c) */
      pc = _mm_add_epi16(pa,pb);

      pa = abs_i16(pa);  /* |p-a| */
      pb = abs_i16(pb);  /* |p-b| */
      pc = abs_i16(pc);  /* |p-c| */

      smallest = _mm_min_epi16(pc, _mm_min_epi16(pa, pb));

      /* Paeth breaks ties favoring a over b over c. */
      nearest  = if_then_else(_mm_cmpeq_epi16(smallest, pa), a,
                         if_then_else(_mm_cmpeq_epi16(smallest, pb), b,
                                                                     c));

      /* Note `_epi8`: we need addition to wrap modulo 255. */
      d = _mm_add_epi8(d, nearest);
      store4(row, _mm_packus_epi16(d,d));

      prev += 4;
      row  += 4;
      rb   -= 4;
   }
}

#endif /* PNG_INTEL_SSE_IMPLEMENTATION > 0 */
#endif /* READ */
