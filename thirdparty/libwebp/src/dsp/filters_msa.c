// Copyright 2016 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// MSA variant of alpha filters
//
// Author: Prashant Patil (prashant.patil@imgtec.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_MSA)

#include "src/dsp/msa_macro.h"

#include <assert.h>

static WEBP_INLINE void PredictLineInverse0(const uint8_t* src,
                                            const uint8_t* pred,
                                            uint8_t* WEBP_RESTRICT dst,
                                            int length) {
  v16u8 src0, pred0, dst0;
  assert(length >= 0);
  while (length >= 32) {
    v16u8 src1, pred1, dst1;
    LD_UB2(src, 16, src0, src1);
    LD_UB2(pred, 16, pred0, pred1);
    SUB2(src0, pred0, src1, pred1, dst0, dst1);
    ST_UB2(dst0, dst1, dst, 16);
    src += 32;
    pred += 32;
    dst += 32;
    length -= 32;
  }
  if (length > 0) {
    int i;
    if (length >= 16) {
      src0 = LD_UB(src);
      pred0 = LD_UB(pred);
      dst0 = src0 - pred0;
      ST_UB(dst0, dst);
      src += 16;
      pred += 16;
      dst += 16;
      length -= 16;
    }
    for (i = 0; i < length; i++) {
      dst[i] = src[i] - pred[i];
    }
  }
}

//------------------------------------------------------------------------------
// Helpful macro.

#define DCHECK(in, out)        \
  do {                         \
    assert((in) != NULL);      \
    assert((out) != NULL);     \
    assert((in) != (out));     \
    assert(width > 0);         \
    assert(height > 0);        \
    assert(stride >= width);   \
  } while (0)

//------------------------------------------------------------------------------
// Horrizontal filter

static void HorizontalFilter_MSA(const uint8_t* WEBP_RESTRICT data,
                                 int width, int height, int stride,
                                 uint8_t* WEBP_RESTRICT filtered_data) {
  const uint8_t* preds = data;
  const uint8_t* in = data;
  uint8_t* out = filtered_data;
  int row = 1;
  DCHECK(in, out);

  // Leftmost pixel is the same as input for topmost scanline.
  out[0] = in[0];
  PredictLineInverse0(in + 1, preds, out + 1, width - 1);
  preds += stride;
  in += stride;
  out += stride;
  // Filter line-by-line.
  while (row < height) {
    // Leftmost pixel is predicted from above.
    PredictLineInverse0(in, preds - stride, out, 1);
    PredictLineInverse0(in + 1, preds, out + 1, width - 1);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Gradient filter

static WEBP_INLINE void PredictLineGradient(const uint8_t* pinput,
                                            const uint8_t* ppred,
                                            uint8_t* WEBP_RESTRICT poutput,
                                            int stride, int size) {
  int w;
  const v16i8 zero = { 0 };
  while (size >= 16) {
    v16u8 pred0, dst0;
    v8i16 a0, a1, b0, b1, c0, c1;
    const v16u8 tmp0 = LD_UB(ppred - 1);
    const v16u8 tmp1 = LD_UB(ppred - stride);
    const v16u8 tmp2 = LD_UB(ppred - stride - 1);
    const v16u8 src0 = LD_UB(pinput);
    ILVRL_B2_SH(zero, tmp0, a0, a1);
    ILVRL_B2_SH(zero, tmp1, b0, b1);
    ILVRL_B2_SH(zero, tmp2, c0, c1);
    ADD2(a0, b0, a1, b1, a0, a1);
    SUB2(a0, c0, a1, c1, a0, a1);
    CLIP_SH2_0_255(a0, a1);
    pred0 = (v16u8)__msa_pckev_b((v16i8)a1, (v16i8)a0);
    dst0 = src0 - pred0;
    ST_UB(dst0, poutput);
    ppred += 16;
    pinput += 16;
    poutput += 16;
    size -= 16;
  }
  for (w = 0; w < size; ++w) {
    const int pred = ppred[w - 1] + ppred[w - stride] - ppred[w - stride - 1];
    poutput[w] = pinput[w] - (pred < 0 ? 0 : pred > 255 ? 255 : pred);
  }
}


static void GradientFilter_MSA(const uint8_t* WEBP_RESTRICT data,
                               int width, int height, int stride,
                               uint8_t* WEBP_RESTRICT filtered_data) {
  const uint8_t* in = data;
  const uint8_t* preds = data;
  uint8_t* out = filtered_data;
  int row = 1;
  DCHECK(in, out);

  // left prediction for top scan-line
  out[0] = in[0];
  PredictLineInverse0(in + 1, preds, out + 1, width - 1);
  preds += stride;
  in += stride;
  out += stride;
  // Filter line-by-line.
  while (row < height) {
    out[0] = in[0] - preds[- stride];
    PredictLineGradient(preds + 1, in + 1, out + 1, stride, width - 1);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Vertical filter

static void VerticalFilter_MSA(const uint8_t* WEBP_RESTRICT data,
                               int width, int height, int stride,
                               uint8_t* WEBP_RESTRICT filtered_data) {
  const uint8_t* in = data;
  const uint8_t* preds = data;
  uint8_t* out = filtered_data;
  int row = 1;
  DCHECK(in, out);

  // Very first top-left pixel is copied.
  out[0] = in[0];
  // Rest of top scan-line is left-predicted.
  PredictLineInverse0(in + 1, preds, out + 1, width - 1);
  in += stride;
  out += stride;

  // Filter line-by-line.
  while (row < height) {
    PredictLineInverse0(in, preds, out, width);
    ++row;
    preds += stride;
    in += stride;
    out += stride;
  }
}

#undef DCHECK

//------------------------------------------------------------------------------
// Entry point

extern void VP8FiltersInitMSA(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8FiltersInitMSA(void) {
  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter_MSA;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter_MSA;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter_MSA;
}

#else  // !WEBP_USE_MSA

WEBP_DSP_INIT_STUB(VP8FiltersInitMSA)

#endif  // WEBP_USE_MSA
