// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// YUV->RGB conversion functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/yuv.h"

#include <assert.h>
#include <stdlib.h>

//-----------------------------------------------------------------------------
// Plain-C version

#define ROW_FUNC(FUNC_NAME, FUNC, XSTEP)                                       \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                          \
                      const uint8_t* WEBP_RESTRICT u,                          \
                      const uint8_t* WEBP_RESTRICT v,                          \
                      uint8_t* WEBP_RESTRICT dst, int len) {                   \
  const uint8_t* const end = dst + (len & ~1) * (XSTEP);                       \
  while (dst != end) {                                                         \
    FUNC(y[0], u[0], v[0], dst);                                               \
    FUNC(y[1], u[0], v[0], dst + (XSTEP));                                     \
    y += 2;                                                                    \
    ++u;                                                                       \
    ++v;                                                                       \
    dst += 2 * (XSTEP);                                                        \
  }                                                                            \
  if (len & 1) {                                                               \
    FUNC(y[0], u[0], v[0], dst);                                               \
  }                                                                            \
}                                                                              \

// All variants implemented.
ROW_FUNC(YuvToRgbRow,      VP8YuvToRgb,  3)
ROW_FUNC(YuvToBgrRow,      VP8YuvToBgr,  3)
ROW_FUNC(YuvToRgbaRow,     VP8YuvToRgba, 4)
ROW_FUNC(YuvToBgraRow,     VP8YuvToBgra, 4)
ROW_FUNC(YuvToArgbRow,     VP8YuvToArgb, 4)
ROW_FUNC(YuvToRgba4444Row, VP8YuvToRgba4444, 2)
ROW_FUNC(YuvToRgb565Row,   VP8YuvToRgb565, 2)

#undef ROW_FUNC

// Main call for processing a plane with a WebPSamplerRowFunc function:
void WebPSamplerProcessPlane(const uint8_t* WEBP_RESTRICT y, int y_stride,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v, int uv_stride,
                             uint8_t* WEBP_RESTRICT dst, int dst_stride,
                             int width, int height, WebPSamplerRowFunc func) {
  int j;
  for (j = 0; j < height; ++j) {
    func(y, u, v, dst, width);
    y += y_stride;
    if (j & 1) {
      u += uv_stride;
      v += uv_stride;
    }
    dst += dst_stride;
  }
}

//-----------------------------------------------------------------------------
// Main call

WebPSamplerRowFunc WebPSamplers[MODE_LAST];

extern VP8CPUInfo VP8GetCPUInfo;
extern void WebPInitSamplersSSE2(void);
extern void WebPInitSamplersSSE41(void);
extern void WebPInitSamplersMIPS32(void);
extern void WebPInitSamplersMIPSdspR2(void);

WEBP_DSP_INIT_FUNC(WebPInitSamplers) {
  WebPSamplers[MODE_RGB]       = YuvToRgbRow;
  WebPSamplers[MODE_RGBA]      = YuvToRgbaRow;
  WebPSamplers[MODE_BGR]       = YuvToBgrRow;
  WebPSamplers[MODE_BGRA]      = YuvToBgraRow;
  WebPSamplers[MODE_ARGB]      = YuvToArgbRow;
  WebPSamplers[MODE_RGBA_4444] = YuvToRgba4444Row;
  WebPSamplers[MODE_RGB_565]   = YuvToRgb565Row;
  WebPSamplers[MODE_rgbA]      = YuvToRgbaRow;
  WebPSamplers[MODE_bgrA]      = YuvToBgraRow;
  WebPSamplers[MODE_Argb]      = YuvToArgbRow;
  WebPSamplers[MODE_rgbA_4444] = YuvToRgba4444Row;

  // If defined, use CPUInfo() to overwrite some pointers with faster versions.
  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitSamplersSSE2();
    }
#endif  // WEBP_HAVE_SSE2
#if defined(WEBP_HAVE_SSE41)
    if (VP8GetCPUInfo(kSSE4_1)) {
      WebPInitSamplersSSE41();
    }
#endif  // WEBP_HAVE_SSE41
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      WebPInitSamplersMIPS32();
    }
#endif  // WEBP_USE_MIPS32
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPInitSamplersMIPSdspR2();
    }
#endif  // WEBP_USE_MIPS_DSP_R2
  }
}

//-----------------------------------------------------------------------------
// ARGB -> YUV converters

static void ConvertARGBToY_C(const uint32_t* WEBP_RESTRICT argb,
                             uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i < width; ++i) {
    const uint32_t p = argb[i];
    y[i] = VP8RGBToY((p >> 16) & 0xff, (p >> 8) & 0xff, (p >>  0) & 0xff,
                     YUV_HALF);
  }
}

void WebPConvertARGBToUV_C(const uint32_t* WEBP_RESTRICT argb,
                           uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int src_width, int do_store) {
  // No rounding. Last pixel is dealt with separately.
  const int uv_width = src_width >> 1;
  int i;
  for (i = 0; i < uv_width; ++i) {
    const uint32_t v0 = argb[2 * i + 0];
    const uint32_t v1 = argb[2 * i + 1];
    // VP8RGBToU/V expects four accumulated pixels. Hence we need to
    // scale r/g/b value by a factor 2. We just shift v0/v1 one bit less.
    const int r = ((v0 >> 15) & 0x1fe) + ((v1 >> 15) & 0x1fe);
    const int g = ((v0 >>  7) & 0x1fe) + ((v1 >>  7) & 0x1fe);
    const int b = ((v0 <<  1) & 0x1fe) + ((v1 <<  1) & 0x1fe);
    const int tmp_u = VP8RGBToU(r, g, b, YUV_HALF << 2);
    const int tmp_v = VP8RGBToV(r, g, b, YUV_HALF << 2);
    if (do_store) {
      u[i] = tmp_u;
      v[i] = tmp_v;
    } else {
      // Approximated average-of-four. But it's an acceptable diff.
      u[i] = (u[i] + tmp_u + 1) >> 1;
      v[i] = (v[i] + tmp_v + 1) >> 1;
    }
  }
  if (src_width & 1) {       // last pixel
    const uint32_t v0 = argb[2 * i + 0];
    const int r = (v0 >> 14) & 0x3fc;
    const int g = (v0 >>  6) & 0x3fc;
    const int b = (v0 <<  2) & 0x3fc;
    const int tmp_u = VP8RGBToU(r, g, b, YUV_HALF << 2);
    const int tmp_v = VP8RGBToV(r, g, b, YUV_HALF << 2);
    if (do_store) {
      u[i] = tmp_u;
      v[i] = tmp_v;
    } else {
      u[i] = (u[i] + tmp_u + 1) >> 1;
      v[i] = (v[i] + tmp_v + 1) >> 1;
    }
  }
}

//-----------------------------------------------------------------------------

static void ConvertRGB24ToY_C(const uint8_t* WEBP_RESTRICT rgb,
                              uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i < width; ++i, rgb += 3) {
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGR24ToY_C(const uint8_t* WEBP_RESTRICT bgr,
                              uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i < width; ++i, bgr += 3) {
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

void WebPConvertRGBA32ToUV_C(const uint16_t* WEBP_RESTRICT rgb,
                             uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                             int width) {
  int i;
  for (i = 0; i < width; i += 1, rgb += 4) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    u[i] = VP8RGBToU(r, g, b, YUV_HALF << 2);
    v[i] = VP8RGBToV(r, g, b, YUV_HALF << 2);
  }
}

//-----------------------------------------------------------------------------

void (*WebPConvertRGB24ToY)(const uint8_t* WEBP_RESTRICT rgb,
                            uint8_t* WEBP_RESTRICT y, int width);
void (*WebPConvertBGR24ToY)(const uint8_t* WEBP_RESTRICT bgr,
                            uint8_t* WEBP_RESTRICT y, int width);
void (*WebPConvertRGBA32ToUV)(const uint16_t* WEBP_RESTRICT rgb,
                              uint8_t* WEBP_RESTRICT u,
                              uint8_t* WEBP_RESTRICT v, int width);

void (*WebPConvertARGBToY)(const uint32_t* WEBP_RESTRICT argb,
                           uint8_t* WEBP_RESTRICT y, int width);
void (*WebPConvertARGBToUV)(const uint32_t* WEBP_RESTRICT argb,
                            uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                            int src_width, int do_store);

extern void WebPInitConvertARGBToYUVSSE2(void);
extern void WebPInitConvertARGBToYUVSSE41(void);
extern void WebPInitConvertARGBToYUVNEON(void);

WEBP_DSP_INIT_FUNC(WebPInitConvertARGBToYUV) {
  WebPConvertARGBToY = ConvertARGBToY_C;
  WebPConvertARGBToUV = WebPConvertARGBToUV_C;

  WebPConvertRGB24ToY = ConvertRGB24ToY_C;
  WebPConvertBGR24ToY = ConvertBGR24ToY_C;

  WebPConvertRGBA32ToUV = WebPConvertRGBA32ToUV_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitConvertARGBToYUVSSE2();
    }
#endif  // WEBP_HAVE_SSE2
#if defined(WEBP_HAVE_SSE41)
    if (VP8GetCPUInfo(kSSE4_1)) {
      WebPInitConvertARGBToYUVSSE41();
    }
#endif  // WEBP_HAVE_SSE41
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    WebPInitConvertARGBToYUVNEON();
  }
#endif  // WEBP_HAVE_NEON

  assert(WebPConvertARGBToY != NULL);
  assert(WebPConvertARGBToUV != NULL);
  assert(WebPConvertRGB24ToY != NULL);
  assert(WebPConvertBGR24ToY != NULL);
  assert(WebPConvertRGBA32ToUV != NULL);
}
