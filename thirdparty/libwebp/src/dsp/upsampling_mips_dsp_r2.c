// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// YUV to RGB upsampling functions.
//
// Author(s): Branimir Vasic (branimir.vasic@imgtec.com)
//            Djordje Pesut  (djordje.pesut@imgtec.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MIPS_DSP_R2)

#include <assert.h>
#include "src/dsp/yuv.h"

#define YUV_TO_RGB(Y, U, V, R, G, B) do {                                      \
    const int t1 = MultHi(Y, 19077);                                           \
    const int t2 = MultHi(V, 13320);                                           \
    R = MultHi(V, 26149);                                                      \
    G = MultHi(U, 6419);                                                       \
    B = MultHi(U, 33050);                                                      \
    R = t1 + R;                                                                \
    G = t1 - G;                                                                \
    B = t1 + B;                                                                \
    R = R - 14234;                                                             \
    G = G - t2 + 8708;                                                         \
    B = B - 17685;                                                             \
    __asm__ volatile (                                                         \
      "shll_s.w         %[" #R "],      %[" #R "],        17         \n\t"     \
      "shll_s.w         %[" #G "],      %[" #G "],        17         \n\t"     \
      "shll_s.w         %[" #B "],      %[" #B "],        17         \n\t"     \
      "precrqu_s.qb.ph  %[" #R "],      %[" #R "],        $zero      \n\t"     \
      "precrqu_s.qb.ph  %[" #G "],      %[" #G "],        $zero      \n\t"     \
      "precrqu_s.qb.ph  %[" #B "],      %[" #B "],        $zero      \n\t"     \
      "srl              %[" #R "],      %[" #R "],        24         \n\t"     \
      "srl              %[" #G "],      %[" #G "],        24         \n\t"     \
      "srl              %[" #B "],      %[" #B "],        24         \n\t"     \
      : [R]"+r"(R), [G]"+r"(G), [B]"+r"(B)                                     \
      :                                                                        \
    );                                                                         \
  } while (0)

#if !defined(WEBP_REDUCE_CSP)
static WEBP_INLINE void YuvToRgb(int y, int u, int v, uint8_t* const rgb) {
  int r, g, b;
  YUV_TO_RGB(y, u, v, r, g, b);
  rgb[0] = r;
  rgb[1] = g;
  rgb[2] = b;
}
static WEBP_INLINE void YuvToBgr(int y, int u, int v, uint8_t* const bgr) {
  int r, g, b;
  YUV_TO_RGB(y, u, v, r, g, b);
  bgr[0] = b;
  bgr[1] = g;
  bgr[2] = r;
}
static WEBP_INLINE void YuvToRgb565(int y, int u, int v, uint8_t* const rgb) {
  int r, g, b;
  YUV_TO_RGB(y, u, v, r, g, b);
  {
    const int rg = (r & 0xf8) | (g >> 5);
    const int gb = ((g << 3) & 0xe0) | (b >> 3);
#if (WEBP_SWAP_16BIT_CSP == 1)
    rgb[0] = gb;
    rgb[1] = rg;
#else
    rgb[0] = rg;
    rgb[1] = gb;
#endif
  }
}
static WEBP_INLINE void YuvToRgba4444(int y, int u, int v,
                                      uint8_t* const argb) {
  int r, g, b;
  YUV_TO_RGB(y, u, v, r, g, b);
  {
    const int rg = (r & 0xf0) | (g >> 4);
    const int ba = (b & 0xf0) | 0x0f;     // overwrite the lower 4 bits
#if (WEBP_SWAP_16BIT_CSP == 1)
    argb[0] = ba;
    argb[1] = rg;
#else
    argb[0] = rg;
    argb[1] = ba;
#endif
   }
}
#endif   // WEBP_REDUCE_CSP

//-----------------------------------------------------------------------------
// Alpha handling variants

#if !defined(WEBP_REDUCE_CSP)
static WEBP_INLINE void YuvToArgb(uint8_t y, uint8_t u, uint8_t v,
                                  uint8_t* const argb) {
  int r, g, b;
  YUV_TO_RGB(y, u, v, r, g, b);
  argb[0] = 0xff;
  argb[1] = r;
  argb[2] = g;
  argb[3] = b;
}
#endif   // WEBP_REDUCE_CSP
static WEBP_INLINE void YuvToBgra(uint8_t y, uint8_t u, uint8_t v,
                                  uint8_t* const bgra) {
  int r, g, b;
  YUV_TO_RGB(y, u, v, r, g, b);
  bgra[0] = b;
  bgra[1] = g;
  bgra[2] = r;
  bgra[3] = 0xff;
}
static WEBP_INLINE void YuvToRgba(uint8_t y, uint8_t u, uint8_t v,
                                  uint8_t* const rgba) {
  int r, g, b;
  YUV_TO_RGB(y, u, v, r, g, b);
  rgba[0] = r;
  rgba[1] = g;
  rgba[2] = b;
  rgba[3] = 0xff;
}

//------------------------------------------------------------------------------
// Fancy upsampler

#ifdef FANCY_UPSAMPLING

// Given samples laid out in a square as:
//  [a b]
//  [c d]
// we interpolate u/v as:
//  ([9*a + 3*b + 3*c +   d    3*a + 9*b + 3*c +   d] + [8 8]) / 16
//  ([3*a +   b + 9*c + 3*d      a + 3*b + 3*c + 9*d]   [8 8]) / 16

// We process u and v together stashed into 32bit (16bit each).
#define LOAD_UV(u, v) ((u) | ((v) << 16))

#define UPSAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                                  \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT top_y,                      \
                      const uint8_t* WEBP_RESTRICT bottom_y,                   \
                      const uint8_t* WEBP_RESTRICT top_u,                      \
                      const uint8_t* WEBP_RESTRICT top_v,                      \
                      const uint8_t* WEBP_RESTRICT cur_u,                      \
                      const uint8_t* WEBP_RESTRICT cur_v,                      \
                      uint8_t* WEBP_RESTRICT top_dst,                          \
                      uint8_t* WEBP_RESTRICT bottom_dst, int len) {            \
  int x;                                                                       \
  const int last_pixel_pair = (len - 1) >> 1;                                  \
  uint32_t tl_uv = LOAD_UV(top_u[0], top_v[0]);   /* top-left sample */        \
  uint32_t l_uv  = LOAD_UV(cur_u[0], cur_v[0]);   /* left-sample */            \
  assert(top_y != NULL);                                                       \
  {                                                                            \
    const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;                \
    FUNC(top_y[0], uv0 & 0xff, (uv0 >> 16), top_dst);                          \
  }                                                                            \
  if (bottom_y != NULL) {                                                      \
    const uint32_t uv0 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;                \
    FUNC(bottom_y[0], uv0 & 0xff, (uv0 >> 16), bottom_dst);                    \
  }                                                                            \
  for (x = 1; x <= last_pixel_pair; ++x) {                                     \
    const uint32_t t_uv = LOAD_UV(top_u[x], top_v[x]);  /* top sample */       \
    const uint32_t uv   = LOAD_UV(cur_u[x], cur_v[x]);  /* sample */           \
    /* precompute invariant values associated with first and second diagonals*/\
    const uint32_t avg = tl_uv + t_uv + l_uv + uv + 0x00080008u;               \
    const uint32_t diag_12 = (avg + 2 * (t_uv + l_uv)) >> 3;                   \
    const uint32_t diag_03 = (avg + 2 * (tl_uv + uv)) >> 3;                    \
    {                                                                          \
      const uint32_t uv0 = (diag_12 + tl_uv) >> 1;                             \
      const uint32_t uv1 = (diag_03 + t_uv) >> 1;                              \
      FUNC(top_y[2 * x - 1], uv0 & 0xff, (uv0 >> 16),                          \
           top_dst + (2 * x - 1) * XSTEP);                                     \
      FUNC(top_y[2 * x - 0], uv1 & 0xff, (uv1 >> 16),                          \
           top_dst + (2 * x - 0) * XSTEP);                                     \
    }                                                                          \
    if (bottom_y != NULL) {                                                    \
      const uint32_t uv0 = (diag_03 + l_uv) >> 1;                              \
      const uint32_t uv1 = (diag_12 + uv) >> 1;                                \
      FUNC(bottom_y[2 * x - 1], uv0 & 0xff, (uv0 >> 16),                       \
           bottom_dst + (2 * x - 1) * XSTEP);                                  \
      FUNC(bottom_y[2 * x + 0], uv1 & 0xff, (uv1 >> 16),                       \
           bottom_dst + (2 * x + 0) * XSTEP);                                  \
    }                                                                          \
    tl_uv = t_uv;                                                              \
    l_uv = uv;                                                                 \
  }                                                                            \
  if (!(len & 1)) {                                                            \
    {                                                                          \
      const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;              \
      FUNC(top_y[len - 1], uv0 & 0xff, (uv0 >> 16),                            \
           top_dst + (len - 1) * XSTEP);                                       \
    }                                                                          \
    if (bottom_y != NULL) {                                                    \
      const uint32_t uv0 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;              \
      FUNC(bottom_y[len - 1], uv0 & 0xff, (uv0 >> 16),                         \
           bottom_dst + (len - 1) * XSTEP);                                    \
    }                                                                          \
  }                                                                            \
}

// All variants implemented.
UPSAMPLE_FUNC(UpsampleRgbaLinePair,     YuvToRgba,     4)
UPSAMPLE_FUNC(UpsampleBgraLinePair,     YuvToBgra,     4)
#if !defined(WEBP_REDUCE_CSP)
UPSAMPLE_FUNC(UpsampleRgbLinePair,      YuvToRgb,      3)
UPSAMPLE_FUNC(UpsampleBgrLinePair,      YuvToBgr,      3)
UPSAMPLE_FUNC(UpsampleArgbLinePair,     YuvToArgb,     4)
UPSAMPLE_FUNC(UpsampleRgba4444LinePair, YuvToRgba4444, 2)
UPSAMPLE_FUNC(UpsampleRgb565LinePair,   YuvToRgb565,   2)
#endif   // WEBP_REDUCE_CSP

#undef LOAD_UV
#undef UPSAMPLE_FUNC

//------------------------------------------------------------------------------
// Entry point

extern void WebPInitUpsamplersMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitUpsamplersMIPSdspR2(void) {
  WebPUpsamplers[MODE_RGBA]      = UpsampleRgbaLinePair;
  WebPUpsamplers[MODE_BGRA]      = UpsampleBgraLinePair;
  WebPUpsamplers[MODE_rgbA]      = UpsampleRgbaLinePair;
  WebPUpsamplers[MODE_bgrA]      = UpsampleBgraLinePair;
#if !defined(WEBP_REDUCE_CSP)
  WebPUpsamplers[MODE_RGB]       = UpsampleRgbLinePair;
  WebPUpsamplers[MODE_BGR]       = UpsampleBgrLinePair;
  WebPUpsamplers[MODE_ARGB]      = UpsampleArgbLinePair;
  WebPUpsamplers[MODE_RGBA_4444] = UpsampleRgba4444LinePair;
  WebPUpsamplers[MODE_RGB_565]   = UpsampleRgb565LinePair;
  WebPUpsamplers[MODE_Argb]      = UpsampleArgbLinePair;
  WebPUpsamplers[MODE_rgbA_4444] = UpsampleRgba4444LinePair;
#endif   // WEBP_REDUCE_CSP
}

#endif  // FANCY_UPSAMPLING

//------------------------------------------------------------------------------
// YUV444 converter

#define YUV444_FUNC(FUNC_NAME, FUNC, XSTEP)                                    \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                          \
                      const uint8_t* WEBP_RESTRICT u,                          \
                      const uint8_t* WEBP_RESTRICT v,                          \
                      uint8_t* WEBP_RESTRICT dst, int len) {                   \
  int i;                                                                       \
  for (i = 0; i < len; ++i) FUNC(y[i], u[i], v[i], &dst[i * XSTEP]);           \
}

YUV444_FUNC(Yuv444ToRgba,     YuvToRgba,     4)
YUV444_FUNC(Yuv444ToBgra,     YuvToBgra,     4)
#if !defined(WEBP_REDUCE_CSP)
YUV444_FUNC(Yuv444ToRgb,      YuvToRgb,      3)
YUV444_FUNC(Yuv444ToBgr,      YuvToBgr,      3)
YUV444_FUNC(Yuv444ToArgb,     YuvToArgb,     4)
YUV444_FUNC(Yuv444ToRgba4444, YuvToRgba4444, 2)
YUV444_FUNC(Yuv444ToRgb565,   YuvToRgb565,   2)
#endif   // WEBP_REDUCE_CSP

#undef YUV444_FUNC

//------------------------------------------------------------------------------
// Entry point

extern void WebPInitYUV444ConvertersMIPSdspR2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitYUV444ConvertersMIPSdspR2(void) {
  WebPYUV444Converters[MODE_RGBA]      = Yuv444ToRgba;
  WebPYUV444Converters[MODE_BGRA]      = Yuv444ToBgra;
  WebPYUV444Converters[MODE_rgbA]      = Yuv444ToRgba;
  WebPYUV444Converters[MODE_bgrA]      = Yuv444ToBgra;
#if !defined(WEBP_REDUCE_CSP)
  WebPYUV444Converters[MODE_RGB]       = Yuv444ToRgb;
  WebPYUV444Converters[MODE_BGR]       = Yuv444ToBgr;
  WebPYUV444Converters[MODE_ARGB]      = Yuv444ToArgb;
  WebPYUV444Converters[MODE_RGBA_4444] = Yuv444ToRgba4444;
  WebPYUV444Converters[MODE_RGB_565]   = Yuv444ToRgb565;
  WebPYUV444Converters[MODE_Argb]      = Yuv444ToArgb;
  WebPYUV444Converters[MODE_rgbA_4444] = Yuv444ToRgba4444;
#endif   // WEBP_REDUCE_CSP
}

#else  // !WEBP_USE_MIPS_DSP_R2

WEBP_DSP_INIT_STUB(WebPInitYUV444ConvertersMIPSdspR2)

#endif  // WEBP_USE_MIPS_DSP_R2

#if !(defined(FANCY_UPSAMPLING) && defined(WEBP_USE_MIPS_DSP_R2))
WEBP_DSP_INIT_STUB(WebPInitUpsamplersMIPSdspR2)
#endif
