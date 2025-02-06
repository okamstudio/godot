// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Spatial prediction using various filters
//
// Author: Urvang (urvang@google.com)

#include "src/dsp/dsp.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
// Helpful macro.

#define DCHECK(in, out)                                                        \
  do {                                                                         \
    assert((in) != NULL);                                                      \
    assert((out) != NULL);                                                     \
    assert((in) != (out));                                                     \
    assert(width > 0);                                                         \
    assert(height > 0);                                                        \
    assert(stride >= width);                                                   \
  } while (0)

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE void PredictLine_C(const uint8_t* WEBP_RESTRICT src,
                                      const uint8_t* WEBP_RESTRICT pred,
                                      uint8_t* WEBP_RESTRICT dst, int length) {
  int i;
  for (i = 0; i < length; ++i) dst[i] = (uint8_t)(src[i] - pred[i]);
}

//------------------------------------------------------------------------------
// Horizontal filter.

static WEBP_INLINE void DoHorizontalFilter_C(const uint8_t* WEBP_RESTRICT in,
                                             int width, int height, int stride,
                                             uint8_t* WEBP_RESTRICT out) {
  const uint8_t* preds = in;
  int row;
  DCHECK(in, out);

  // Leftmost pixel is the same as input for topmost scanline.
  out[0] = in[0];
  PredictLine_C(in + 1, preds, out + 1, width - 1);
  preds += stride;
  in += stride;
  out += stride;

  // Filter line-by-line.
  for (row = 1; row < height; ++row) {
    // Leftmost pixel is predicted from above.
    PredictLine_C(in, preds - stride, out, 1);
    PredictLine_C(in + 1, preds, out + 1, width - 1);
    preds += stride;
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Vertical filter.

static WEBP_INLINE void DoVerticalFilter_C(const uint8_t* WEBP_RESTRICT in,
                                           int width, int height, int stride,
                                           uint8_t* WEBP_RESTRICT out) {
  const uint8_t* preds = in;
  int row;
  DCHECK(in, out);

  // Very first top-left pixel is copied.
  out[0] = in[0];
  // Rest of top scan-line is left-predicted.
  PredictLine_C(in + 1, preds, out + 1, width - 1);
  in += stride;
  out += stride;

  // Filter line-by-line.
  for (row = 1; row < height; ++row) {
    PredictLine_C(in, preds, out, width);
    preds += stride;
    in += stride;
    out += stride;
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

//------------------------------------------------------------------------------
// Gradient filter.

static WEBP_INLINE int GradientPredictor_C(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;  // clip to 8bit
}

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE void DoGradientFilter_C(const uint8_t* WEBP_RESTRICT in,
                                           int width, int height, int stride,
                                           uint8_t* WEBP_RESTRICT out) {
  const uint8_t* preds = in;
  int row;
  DCHECK(in, out);

  // left prediction for top scan-line
  out[0] = in[0];
  PredictLine_C(in + 1, preds, out + 1, width - 1);
  preds += stride;
  in += stride;
  out += stride;

  // Filter line-by-line.
  for (row = 1; row < height; ++row) {
    int w;
    // leftmost pixel: predict from above.
    PredictLine_C(in, preds - stride, out, 1);
    for (w = 1; w < width; ++w) {
      const int pred = GradientPredictor_C(preds[w - 1],
                                           preds[w - stride],
                                           preds[w - stride - 1]);
      out[w] = (uint8_t)(in[w] - pred);
    }
    preds += stride;
    in += stride;
    out += stride;
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

#undef DCHECK

//------------------------------------------------------------------------------

#if !WEBP_NEON_OMIT_C_CODE
static void HorizontalFilter_C(const uint8_t* WEBP_RESTRICT data,
                               int width, int height, int stride,
                               uint8_t* WEBP_RESTRICT filtered_data) {
  DoHorizontalFilter_C(data, width, height, stride, filtered_data);
}

static void VerticalFilter_C(const uint8_t* WEBP_RESTRICT data,
                             int width, int height, int stride,
                             uint8_t* WEBP_RESTRICT filtered_data) {
  DoVerticalFilter_C(data, width, height, stride, filtered_data);
}

static void GradientFilter_C(const uint8_t* WEBP_RESTRICT data,
                             int width, int height, int stride,
                             uint8_t* WEBP_RESTRICT filtered_data) {
  DoGradientFilter_C(data, width, height, stride, filtered_data);
}
#endif  // !WEBP_NEON_OMIT_C_CODE

//------------------------------------------------------------------------------

static void NoneUnfilter_C(const uint8_t* prev, const uint8_t* in,
                           uint8_t* out, int width) {
  (void)prev;
  if (out != in) memcpy(out, in, width * sizeof(*out));
}

static void HorizontalUnfilter_C(const uint8_t* prev, const uint8_t* in,
                                 uint8_t* out, int width) {
  uint8_t pred = (prev == NULL) ? 0 : prev[0];
  int i;
  for (i = 0; i < width; ++i) {
    out[i] = (uint8_t)(pred + in[i]);
    pred = out[i];
  }
}

#if !WEBP_NEON_OMIT_C_CODE
static void VerticalUnfilter_C(const uint8_t* prev, const uint8_t* in,
                               uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_C(NULL, in, out, width);
  } else {
    int i;
    for (i = 0; i < width; ++i) out[i] = (uint8_t)(prev[i] + in[i]);
  }
}
#endif  // !WEBP_NEON_OMIT_C_CODE

static void GradientUnfilter_C(const uint8_t* prev, const uint8_t* in,
                               uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_C(NULL, in, out, width);
  } else {
    uint8_t top = prev[0], top_left = top, left = top;
    int i;
    for (i = 0; i < width; ++i) {
      top = prev[i];  // need to read this first, in case prev==out
      left = (uint8_t)(in[i] + GradientPredictor_C(left, top, top_left));
      top_left = top;
      out[i] = left;
    }
  }
}

//------------------------------------------------------------------------------
// Init function

WebPFilterFunc WebPFilters[WEBP_FILTER_LAST];
WebPUnfilterFunc WebPUnfilters[WEBP_FILTER_LAST];

extern VP8CPUInfo VP8GetCPUInfo;
extern void VP8FiltersInitMIPSdspR2(void);
extern void VP8FiltersInitMSA(void);
extern void VP8FiltersInitNEON(void);
extern void VP8FiltersInitSSE2(void);

WEBP_DSP_INIT_FUNC(VP8FiltersInit) {
  WebPUnfilters[WEBP_FILTER_NONE] = NoneUnfilter_C;
#if !WEBP_NEON_OMIT_C_CODE
  WebPUnfilters[WEBP_FILTER_HORIZONTAL] = HorizontalUnfilter_C;
  WebPUnfilters[WEBP_FILTER_VERTICAL] = VerticalUnfilter_C;
#endif
  WebPUnfilters[WEBP_FILTER_GRADIENT] = GradientUnfilter_C;

  WebPFilters[WEBP_FILTER_NONE] = NULL;
#if !WEBP_NEON_OMIT_C_CODE
  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter_C;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter_C;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter_C;
#endif

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      VP8FiltersInitSSE2();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      VP8FiltersInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      VP8FiltersInitMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    VP8FiltersInitNEON();
  }
#endif

  assert(WebPUnfilters[WEBP_FILTER_NONE] != NULL);
  assert(WebPUnfilters[WEBP_FILTER_HORIZONTAL] != NULL);
  assert(WebPUnfilters[WEBP_FILTER_VERTICAL] != NULL);
  assert(WebPUnfilters[WEBP_FILTER_GRADIENT] != NULL);
  assert(WebPFilters[WEBP_FILTER_HORIZONTAL] != NULL);
  assert(WebPFilters[WEBP_FILTER_VERTICAL] != NULL);
  assert(WebPFilters[WEBP_FILTER_GRADIENT] != NULL);
}
