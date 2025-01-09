// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE2 version of YUV to RGB upsampling functions.
//
// Author: somnath@google.com (Somnath Banerjee)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_SSE2)

#include <assert.h>
#include <emmintrin.h>
#include <string.h>
#include "src/dsp/yuv.h"

#ifdef FANCY_UPSAMPLING

// We compute (9*a + 3*b + 3*c + d + 8) / 16 as follows
// u = (9*a + 3*b + 3*c + d + 8) / 16
//   = (a + (a + 3*b + 3*c + d) / 8 + 1) / 2
//   = (a + m + 1) / 2
// where m = (a + 3*b + 3*c + d) / 8
//         = ((a + b + c + d) / 2 + b + c) / 4
//
// Let's say  k = (a + b + c + d) / 4.
// We can compute k as
// k = (s + t + 1) / 2 - ((a^d) | (b^c) | (s^t)) & 1
// where s = (a + d + 1) / 2 and t = (b + c + 1) / 2
//
// Then m can be written as
// m = (k + t + 1) / 2 - (((b^c) & (s^t)) | (k^t)) & 1

// Computes out = (k + in + 1) / 2 - ((ij & (s^t)) | (k^in)) & 1
#define GET_M(ij, in, out) do {                                                \
  const __m128i tmp0 = _mm_avg_epu8(k, (in));     /* (k + in + 1) / 2 */       \
  const __m128i tmp1 = _mm_and_si128((ij), st);   /* (ij) & (s^t) */           \
  const __m128i tmp2 = _mm_xor_si128(k, (in));    /* (k^in) */                 \
  const __m128i tmp3 = _mm_or_si128(tmp1, tmp2);  /* ((ij) & (s^t)) | (k^in) */\
  const __m128i tmp4 = _mm_and_si128(tmp3, one);  /* & 1 -> lsb_correction */  \
  (out) = _mm_sub_epi8(tmp0, tmp4);    /* (k + in + 1) / 2 - lsb_correction */ \
} while (0)

// pack and store two alternating pixel rows
#define PACK_AND_STORE(a, b, da, db, out) do {                                 \
  const __m128i t_a = _mm_avg_epu8(a, da);  /* (9a + 3b + 3c +  d + 8) / 16 */ \
  const __m128i t_b = _mm_avg_epu8(b, db);  /* (3a + 9b +  c + 3d + 8) / 16 */ \
  const __m128i t_1 = _mm_unpacklo_epi8(t_a, t_b);                             \
  const __m128i t_2 = _mm_unpackhi_epi8(t_a, t_b);                             \
  _mm_store_si128(((__m128i*)(out)) + 0, t_1);                                 \
  _mm_store_si128(((__m128i*)(out)) + 1, t_2);                                 \
} while (0)

// Loads 17 pixels each from rows r1 and r2 and generates 32 pixels.
#define UPSAMPLE_32PIXELS(r1, r2, out) do {                                    \
  const __m128i one = _mm_set1_epi8(1);                                        \
  const __m128i a = _mm_loadu_si128((const __m128i*)&(r1)[0]);                 \
  const __m128i b = _mm_loadu_si128((const __m128i*)&(r1)[1]);                 \
  const __m128i c = _mm_loadu_si128((const __m128i*)&(r2)[0]);                 \
  const __m128i d = _mm_loadu_si128((const __m128i*)&(r2)[1]);                 \
                                                                               \
  const __m128i s = _mm_avg_epu8(a, d);        /* s = (a + d + 1) / 2 */       \
  const __m128i t = _mm_avg_epu8(b, c);        /* t = (b + c + 1) / 2 */       \
  const __m128i st = _mm_xor_si128(s, t);      /* st = s^t */                  \
                                                                               \
  const __m128i ad = _mm_xor_si128(a, d);      /* ad = a^d */                  \
  const __m128i bc = _mm_xor_si128(b, c);      /* bc = b^c */                  \
                                                                               \
  const __m128i t1 = _mm_or_si128(ad, bc);     /* (a^d) | (b^c) */             \
  const __m128i t2 = _mm_or_si128(t1, st);     /* (a^d) | (b^c) | (s^t) */     \
  const __m128i t3 = _mm_and_si128(t2, one);   /* (a^d) | (b^c) | (s^t) & 1 */ \
  const __m128i t4 = _mm_avg_epu8(s, t);                                       \
  const __m128i k = _mm_sub_epi8(t4, t3);      /* k = (a + b + c + d) / 4 */   \
  __m128i diag1, diag2;                                                        \
                                                                               \
  GET_M(bc, t, diag1);                  /* diag1 = (a + 3b + 3c + d) / 8 */    \
  GET_M(ad, s, diag2);                  /* diag2 = (3a + b + c + 3d) / 8 */    \
                                                                               \
  /* pack the alternate pixels */                                              \
  PACK_AND_STORE(a, b, diag1, diag2, (out) +      0);  /* store top */         \
  PACK_AND_STORE(c, d, diag2, diag1, (out) + 2 * 32);  /* store bottom */      \
} while (0)

// Turn the macro into a function for reducing code-size when non-critical
static void Upsample32Pixels_SSE2(const uint8_t* WEBP_RESTRICT const r1,
                                  const uint8_t* WEBP_RESTRICT const r2,
                                  uint8_t* WEBP_RESTRICT const out) {
  UPSAMPLE_32PIXELS(r1, r2, out);
}

#define UPSAMPLE_LAST_BLOCK(tb, bb, num_pixels, out) {                         \
  uint8_t r1[17], r2[17];                                                      \
  memcpy(r1, (tb), (num_pixels));                                              \
  memcpy(r2, (bb), (num_pixels));                                              \
  /* replicate last byte */                                                    \
  memset(r1 + (num_pixels), r1[(num_pixels) - 1], 17 - (num_pixels));          \
  memset(r2 + (num_pixels), r2[(num_pixels) - 1], 17 - (num_pixels));          \
  /* using the shared function instead of the macro saves ~3k code size */     \
  Upsample32Pixels_SSE2(r1, r2, out);                                          \
}

#define CONVERT2RGB_32(FUNC, XSTEP, top_y, bottom_y,                           \
                       top_dst, bottom_dst, cur_x) do {                        \
  FUNC##32_SSE2((top_y) + (cur_x), r_u, r_v, (top_dst) + (cur_x) * (XSTEP));   \
  if ((bottom_y) != NULL) {                                                    \
    FUNC##32_SSE2((bottom_y) + (cur_x), r_u + 64, r_v + 64,                    \
                  (bottom_dst) + (cur_x) * (XSTEP));                           \
  }                                                                            \
} while (0)

#define SSE2_UPSAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                             \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT top_y,                      \
                      const uint8_t* WEBP_RESTRICT bottom_y,                   \
                      const uint8_t* WEBP_RESTRICT top_u,                      \
                      const uint8_t* WEBP_RESTRICT top_v,                      \
                      const uint8_t* WEBP_RESTRICT cur_u,                      \
                      const uint8_t* WEBP_RESTRICT cur_v,                      \
                      uint8_t* WEBP_RESTRICT top_dst,                          \
                      uint8_t* WEBP_RESTRICT bottom_dst, int len) {            \
  int uv_pos, pos;                                                             \
  /* 16byte-aligned array to cache reconstructed u and v */                    \
  uint8_t uv_buf[14 * 32 + 15] = { 0 };                                        \
  uint8_t* const r_u = (uint8_t*)((uintptr_t)(uv_buf + 15) & ~(uintptr_t)15);  \
  uint8_t* const r_v = r_u + 32;                                               \
                                                                               \
  assert(top_y != NULL);                                                       \
  {   /* Treat the first pixel in regular way */                               \
    const int u_diag = ((top_u[0] + cur_u[0]) >> 1) + 1;                       \
    const int v_diag = ((top_v[0] + cur_v[0]) >> 1) + 1;                       \
    const int u0_t = (top_u[0] + u_diag) >> 1;                                 \
    const int v0_t = (top_v[0] + v_diag) >> 1;                                 \
    FUNC(top_y[0], u0_t, v0_t, top_dst);                                       \
    if (bottom_y != NULL) {                                                    \
      const int u0_b = (cur_u[0] + u_diag) >> 1;                               \
      const int v0_b = (cur_v[0] + v_diag) >> 1;                               \
      FUNC(bottom_y[0], u0_b, v0_b, bottom_dst);                               \
    }                                                                          \
  }                                                                            \
  /* For UPSAMPLE_32PIXELS, 17 u/v values must be read-able for each block */  \
  for (pos = 1, uv_pos = 0; pos + 32 + 1 <= len; pos += 32, uv_pos += 16) {    \
    UPSAMPLE_32PIXELS(top_u + uv_pos, cur_u + uv_pos, r_u);                    \
    UPSAMPLE_32PIXELS(top_v + uv_pos, cur_v + uv_pos, r_v);                    \
    CONVERT2RGB_32(FUNC, XSTEP, top_y, bottom_y, top_dst, bottom_dst, pos);    \
  }                                                                            \
  if (len > 1) {                                                               \
    const int left_over = ((len + 1) >> 1) - (pos >> 1);                       \
    uint8_t* const tmp_top_dst = r_u + 4 * 32;                                 \
    uint8_t* const tmp_bottom_dst = tmp_top_dst + 4 * 32;                      \
    uint8_t* const tmp_top = tmp_bottom_dst + 4 * 32;                          \
    uint8_t* const tmp_bottom = (bottom_y == NULL) ? NULL : tmp_top + 32;      \
    assert(left_over > 0);                                                     \
    UPSAMPLE_LAST_BLOCK(top_u + uv_pos, cur_u + uv_pos, left_over, r_u);       \
    UPSAMPLE_LAST_BLOCK(top_v + uv_pos, cur_v + uv_pos, left_over, r_v);       \
    memcpy(tmp_top, top_y + pos, len - pos);                                   \
    if (bottom_y != NULL) memcpy(tmp_bottom, bottom_y + pos, len - pos);       \
    CONVERT2RGB_32(FUNC, XSTEP, tmp_top, tmp_bottom, tmp_top_dst,              \
         tmp_bottom_dst, 0);                                                   \
    memcpy(top_dst + pos * (XSTEP), tmp_top_dst, (len - pos) * (XSTEP));       \
    if (bottom_y != NULL) {                                                    \
      memcpy(bottom_dst + pos * (XSTEP), tmp_bottom_dst,                       \
             (len - pos) * (XSTEP));                                           \
    }                                                                          \
  }                                                                            \
}

// SSE2 variants of the fancy upsampler.
SSE2_UPSAMPLE_FUNC(UpsampleRgbaLinePair_SSE2, VP8YuvToRgba, 4)
SSE2_UPSAMPLE_FUNC(UpsampleBgraLinePair_SSE2, VP8YuvToBgra, 4)

#if !defined(WEBP_REDUCE_CSP)
SSE2_UPSAMPLE_FUNC(UpsampleRgbLinePair_SSE2,  VP8YuvToRgb,  3)
SSE2_UPSAMPLE_FUNC(UpsampleBgrLinePair_SSE2,  VP8YuvToBgr,  3)
SSE2_UPSAMPLE_FUNC(UpsampleArgbLinePair_SSE2, VP8YuvToArgb, 4)
SSE2_UPSAMPLE_FUNC(UpsampleRgba4444LinePair_SSE2, VP8YuvToRgba4444, 2)
SSE2_UPSAMPLE_FUNC(UpsampleRgb565LinePair_SSE2, VP8YuvToRgb565, 2)
#endif   // WEBP_REDUCE_CSP

#undef GET_M
#undef PACK_AND_STORE
#undef UPSAMPLE_32PIXELS
#undef UPSAMPLE_LAST_BLOCK
#undef CONVERT2RGB
#undef CONVERT2RGB_32
#undef SSE2_UPSAMPLE_FUNC

//------------------------------------------------------------------------------
// Entry point

extern WebPUpsampleLinePairFunc WebPUpsamplers[/* MODE_LAST */];

extern void WebPInitUpsamplersSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitUpsamplersSSE2(void) {
  WebPUpsamplers[MODE_RGBA] = UpsampleRgbaLinePair_SSE2;
  WebPUpsamplers[MODE_BGRA] = UpsampleBgraLinePair_SSE2;
  WebPUpsamplers[MODE_rgbA] = UpsampleRgbaLinePair_SSE2;
  WebPUpsamplers[MODE_bgrA] = UpsampleBgraLinePair_SSE2;
#if !defined(WEBP_REDUCE_CSP)
  WebPUpsamplers[MODE_RGB]  = UpsampleRgbLinePair_SSE2;
  WebPUpsamplers[MODE_BGR]  = UpsampleBgrLinePair_SSE2;
  WebPUpsamplers[MODE_ARGB] = UpsampleArgbLinePair_SSE2;
  WebPUpsamplers[MODE_Argb] = UpsampleArgbLinePair_SSE2;
  WebPUpsamplers[MODE_RGB_565] = UpsampleRgb565LinePair_SSE2;
  WebPUpsamplers[MODE_RGBA_4444] = UpsampleRgba4444LinePair_SSE2;
  WebPUpsamplers[MODE_rgbA_4444] = UpsampleRgba4444LinePair_SSE2;
#endif   // WEBP_REDUCE_CSP
}

#endif  // FANCY_UPSAMPLING

//------------------------------------------------------------------------------

extern WebPYUV444Converter WebPYUV444Converters[/* MODE_LAST */];
extern void WebPInitYUV444ConvertersSSE2(void);

#define YUV444_FUNC(FUNC_NAME, CALL, CALL_C, XSTEP)                            \
extern void CALL_C(const uint8_t* WEBP_RESTRICT y,                             \
                   const uint8_t* WEBP_RESTRICT u,                             \
                   const uint8_t* WEBP_RESTRICT v,                             \
                   uint8_t* WEBP_RESTRICT dst, int len);                       \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                          \
                      const uint8_t* WEBP_RESTRICT u,                          \
                      const uint8_t* WEBP_RESTRICT v,                          \
                      uint8_t* WEBP_RESTRICT dst, int len) {                   \
  int i;                                                                       \
  const int max_len = len & ~31;                                               \
  for (i = 0; i < max_len; i += 32) {                                          \
    CALL(y + i, u + i, v + i, dst + i * (XSTEP));                              \
  }                                                                            \
  if (i < len) {  /* C-fallback */                                             \
    CALL_C(y + i, u + i, v + i, dst + i * (XSTEP), len - i);                   \
  }                                                                            \
}

YUV444_FUNC(Yuv444ToRgba_SSE2, VP8YuvToRgba32_SSE2, WebPYuv444ToRgba_C, 4)
YUV444_FUNC(Yuv444ToBgra_SSE2, VP8YuvToBgra32_SSE2, WebPYuv444ToBgra_C, 4)
#if !defined(WEBP_REDUCE_CSP)
YUV444_FUNC(Yuv444ToRgb_SSE2, VP8YuvToRgb32_SSE2, WebPYuv444ToRgb_C, 3)
YUV444_FUNC(Yuv444ToBgr_SSE2, VP8YuvToBgr32_SSE2, WebPYuv444ToBgr_C, 3)
YUV444_FUNC(Yuv444ToArgb_SSE2, VP8YuvToArgb32_SSE2, WebPYuv444ToArgb_C, 4)
YUV444_FUNC(Yuv444ToRgba4444_SSE2, VP8YuvToRgba444432_SSE2, \
            WebPYuv444ToRgba4444_C, 2)
YUV444_FUNC(Yuv444ToRgb565_SSE2, VP8YuvToRgb56532_SSE2, WebPYuv444ToRgb565_C, 2)
#endif   // WEBP_REDUCE_CSP

WEBP_TSAN_IGNORE_FUNCTION void WebPInitYUV444ConvertersSSE2(void) {
  WebPYUV444Converters[MODE_RGBA]      = Yuv444ToRgba_SSE2;
  WebPYUV444Converters[MODE_BGRA]      = Yuv444ToBgra_SSE2;
  WebPYUV444Converters[MODE_rgbA]      = Yuv444ToRgba_SSE2;
  WebPYUV444Converters[MODE_bgrA]      = Yuv444ToBgra_SSE2;
#if !defined(WEBP_REDUCE_CSP)
  WebPYUV444Converters[MODE_RGB]       = Yuv444ToRgb_SSE2;
  WebPYUV444Converters[MODE_BGR]       = Yuv444ToBgr_SSE2;
  WebPYUV444Converters[MODE_ARGB]      = Yuv444ToArgb_SSE2;
  WebPYUV444Converters[MODE_RGBA_4444] = Yuv444ToRgba4444_SSE2;
  WebPYUV444Converters[MODE_RGB_565]   = Yuv444ToRgb565_SSE2;
  WebPYUV444Converters[MODE_Argb]      = Yuv444ToArgb_SSE2;
  WebPYUV444Converters[MODE_rgbA_4444] = Yuv444ToRgba4444_SSE2;
#endif   // WEBP_REDUCE_CSP
}

#else

WEBP_DSP_INIT_STUB(WebPInitYUV444ConvertersSSE2)

#endif  // WEBP_USE_SSE2

#if !(defined(FANCY_UPSAMPLING) && defined(WEBP_USE_SSE2))
WEBP_DSP_INIT_STUB(WebPInitUpsamplersSSE2)
#endif
