// Copyright 2017 Google Inc. All Rights Reserved.
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

#if defined(WEBP_USE_NEON)

#include <assert.h>
#include <stdlib.h>

#include "src/dsp/neon.h"

//-----------------------------------------------------------------------------

static uint8x8_t ConvertRGBToY_NEON(const uint8x8_t R,
                                    const uint8x8_t G,
                                    const uint8x8_t B) {
  const uint16x8_t r = vmovl_u8(R);
  const uint16x8_t g = vmovl_u8(G);
  const uint16x8_t b = vmovl_u8(B);
  const uint16x4_t r_lo = vget_low_u16(r);
  const uint16x4_t r_hi = vget_high_u16(r);
  const uint16x4_t g_lo = vget_low_u16(g);
  const uint16x4_t g_hi = vget_high_u16(g);
  const uint16x4_t b_lo = vget_low_u16(b);
  const uint16x4_t b_hi = vget_high_u16(b);
  const uint32x4_t tmp0_lo = vmull_n_u16(         r_lo, 16839u);
  const uint32x4_t tmp0_hi = vmull_n_u16(         r_hi, 16839u);
  const uint32x4_t tmp1_lo = vmlal_n_u16(tmp0_lo, g_lo, 33059u);
  const uint32x4_t tmp1_hi = vmlal_n_u16(tmp0_hi, g_hi, 33059u);
  const uint32x4_t tmp2_lo = vmlal_n_u16(tmp1_lo, b_lo, 6420u);
  const uint32x4_t tmp2_hi = vmlal_n_u16(tmp1_hi, b_hi, 6420u);
  const uint16x8_t Y1 = vcombine_u16(vrshrn_n_u32(tmp2_lo, 16),
                                     vrshrn_n_u32(tmp2_hi, 16));
  const uint16x8_t Y2 = vaddq_u16(Y1, vdupq_n_u16(16));
  return vqmovn_u16(Y2);
}

static void ConvertRGB24ToY_NEON(const uint8_t* WEBP_RESTRICT rgb,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i + 8 <= width; i += 8, rgb += 3 * 8) {
    const uint8x8x3_t RGB = vld3_u8(rgb);
    const uint8x8_t Y = ConvertRGBToY_NEON(RGB.val[0], RGB.val[1], RGB.val[2]);
    vst1_u8(y + i, Y);
  }
  for (; i < width; ++i, rgb += 3) {   // left-over
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGR24ToY_NEON(const uint8_t* WEBP_RESTRICT bgr,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i + 8 <= width; i += 8, bgr += 3 * 8) {
    const uint8x8x3_t BGR = vld3_u8(bgr);
    const uint8x8_t Y = ConvertRGBToY_NEON(BGR.val[2], BGR.val[1], BGR.val[0]);
    vst1_u8(y + i, Y);
  }
  for (; i < width; ++i, bgr += 3) {  // left-over
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

static void ConvertARGBToY_NEON(const uint32_t* WEBP_RESTRICT argb,
                                uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i + 8 <= width; i += 8) {
    const uint8x8x4_t RGB = vld4_u8((const uint8_t*)&argb[i]);
    const uint8x8_t Y = ConvertRGBToY_NEON(RGB.val[2], RGB.val[1], RGB.val[0]);
    vst1_u8(y + i, Y);
  }
  for (; i < width; ++i) {   // left-over
    const uint32_t p = argb[i];
    y[i] = VP8RGBToY((p >> 16) & 0xff, (p >> 8) & 0xff, (p >>  0) & 0xff,
                     YUV_HALF);
  }
}

//-----------------------------------------------------------------------------

// computes: DST_s16 = [(C0 * r + C1 * g + C2 * b) >> 16] + CST
#define MULTIPLY_16b_PREAMBLE(r, g, b)                           \
  const int16x4_t r_lo = vreinterpret_s16_u16(vget_low_u16(r));  \
  const int16x4_t r_hi = vreinterpret_s16_u16(vget_high_u16(r)); \
  const int16x4_t g_lo = vreinterpret_s16_u16(vget_low_u16(g));  \
  const int16x4_t g_hi = vreinterpret_s16_u16(vget_high_u16(g)); \
  const int16x4_t b_lo = vreinterpret_s16_u16(vget_low_u16(b));  \
  const int16x4_t b_hi = vreinterpret_s16_u16(vget_high_u16(b))

#define MULTIPLY_16b(C0, C1, C2, CST, DST_s16) do {              \
  const int32x4_t tmp0_lo = vmull_n_s16(         r_lo, C0);      \
  const int32x4_t tmp0_hi = vmull_n_s16(         r_hi, C0);      \
  const int32x4_t tmp1_lo = vmlal_n_s16(tmp0_lo, g_lo, C1);      \
  const int32x4_t tmp1_hi = vmlal_n_s16(tmp0_hi, g_hi, C1);      \
  const int32x4_t tmp2_lo = vmlal_n_s16(tmp1_lo, b_lo, C2);      \
  const int32x4_t tmp2_hi = vmlal_n_s16(tmp1_hi, b_hi, C2);      \
  const int16x8_t tmp3 = vcombine_s16(vshrn_n_s32(tmp2_lo, 16),  \
                                      vshrn_n_s32(tmp2_hi, 16)); \
  DST_s16 = vaddq_s16(tmp3, vdupq_n_s16(CST));                   \
} while (0)

// This needs to be a macro, since (128 << SHIFT) needs to be an immediate.
#define CONVERT_RGB_TO_UV(r, g, b, SHIFT, U_DST, V_DST) do {     \
  MULTIPLY_16b_PREAMBLE(r, g, b);                                \
  MULTIPLY_16b(-9719, -19081, 28800, 128 << SHIFT, U_DST);       \
  MULTIPLY_16b(28800, -24116, -4684, 128 << SHIFT, V_DST);       \
} while (0)

static void ConvertRGBA32ToUV_NEON(const uint16_t* WEBP_RESTRICT rgb,
                                   uint8_t* WEBP_RESTRICT u,
                                   uint8_t* WEBP_RESTRICT v, int width) {
  int i;
  for (i = 0; i + 8 <= width; i += 8, rgb += 4 * 8) {
    const uint16x8x4_t RGB = vld4q_u16((const uint16_t*)rgb);
    int16x8_t U, V;
    CONVERT_RGB_TO_UV(RGB.val[0], RGB.val[1], RGB.val[2], 2, U, V);
    vst1_u8(u + i, vqrshrun_n_s16(U, 2));
    vst1_u8(v + i, vqrshrun_n_s16(V, 2));
  }
  for (; i < width; i += 1, rgb += 4) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    u[i] = VP8RGBToU(r, g, b, YUV_HALF << 2);
    v[i] = VP8RGBToV(r, g, b, YUV_HALF << 2);
  }
}

static void ConvertARGBToUV_NEON(const uint32_t* WEBP_RESTRICT argb,
                                 uint8_t* WEBP_RESTRICT u,
                                 uint8_t* WEBP_RESTRICT v,
                                 int src_width, int do_store) {
  int i;
  for (i = 0; i + 16 <= src_width; i += 16, u += 8, v += 8) {
    const uint8x16x4_t RGB = vld4q_u8((const uint8_t*)&argb[i]);
    const uint16x8_t R = vpaddlq_u8(RGB.val[2]);  // pair-wise adds
    const uint16x8_t G = vpaddlq_u8(RGB.val[1]);
    const uint16x8_t B = vpaddlq_u8(RGB.val[0]);
    int16x8_t U_tmp, V_tmp;
    CONVERT_RGB_TO_UV(R, G, B, 1, U_tmp, V_tmp);
    {
      const uint8x8_t U = vqrshrun_n_s16(U_tmp, 1);
      const uint8x8_t V = vqrshrun_n_s16(V_tmp, 1);
      if (do_store) {
        vst1_u8(u, U);
        vst1_u8(v, V);
      } else {
        const uint8x8_t prev_u = vld1_u8(u);
        const uint8x8_t prev_v = vld1_u8(v);
        vst1_u8(u, vrhadd_u8(U, prev_u));
        vst1_u8(v, vrhadd_u8(V, prev_v));
      }
    }
  }
  if (i < src_width) {  // left-over
    WebPConvertARGBToUV_C(argb + i, u, v, src_width - i, do_store);
  }
}


//------------------------------------------------------------------------------

extern void WebPInitConvertARGBToYUVNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitConvertARGBToYUVNEON(void) {
  WebPConvertRGB24ToY = ConvertRGB24ToY_NEON;
  WebPConvertBGR24ToY = ConvertBGR24ToY_NEON;
  WebPConvertARGBToY = ConvertARGBToY_NEON;
  WebPConvertARGBToUV = ConvertARGBToUV_NEON;
  WebPConvertRGBA32ToUV = ConvertRGBA32ToUV_NEON;
}

#else  // !WEBP_USE_NEON

WEBP_DSP_INIT_STUB(WebPInitConvertARGBToYUVNEON)

#endif  // WEBP_USE_NEON
