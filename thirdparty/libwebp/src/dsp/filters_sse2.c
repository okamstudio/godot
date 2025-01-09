// Copyright 2015 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// SSE2 variant of alpha filters
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/dsp/dsp.h"

#if defined(WEBP_USE_SSE2)

#include <assert.h>
#include <emmintrin.h>
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

static void PredictLineTop_SSE2(const uint8_t* WEBP_RESTRICT src,
                                const uint8_t* WEBP_RESTRICT pred,
                                uint8_t* WEBP_RESTRICT dst, int length) {
  int i;
  const int max_pos = length & ~31;
  assert(length >= 0);
  for (i = 0; i < max_pos; i += 32) {
    const __m128i A0 = _mm_loadu_si128((const __m128i*)&src[i +  0]);
    const __m128i A1 = _mm_loadu_si128((const __m128i*)&src[i + 16]);
    const __m128i B0 = _mm_loadu_si128((const __m128i*)&pred[i +  0]);
    const __m128i B1 = _mm_loadu_si128((const __m128i*)&pred[i + 16]);
    const __m128i C0 = _mm_sub_epi8(A0, B0);
    const __m128i C1 = _mm_sub_epi8(A1, B1);
    _mm_storeu_si128((__m128i*)&dst[i +  0], C0);
    _mm_storeu_si128((__m128i*)&dst[i + 16], C1);
  }
  for (; i < length; ++i) dst[i] = src[i] - pred[i];
}

// Special case for left-based prediction (when preds==dst-1 or preds==src-1).
static void PredictLineLeft_SSE2(const uint8_t* WEBP_RESTRICT src,
                                 uint8_t* WEBP_RESTRICT dst, int length) {
  int i;
  const int max_pos = length & ~31;
  assert(length >= 0);
  for (i = 0; i < max_pos; i += 32) {
    const __m128i A0 = _mm_loadu_si128((const __m128i*)(src + i +  0    ));
    const __m128i B0 = _mm_loadu_si128((const __m128i*)(src + i +  0 - 1));
    const __m128i A1 = _mm_loadu_si128((const __m128i*)(src + i + 16    ));
    const __m128i B1 = _mm_loadu_si128((const __m128i*)(src + i + 16 - 1));
    const __m128i C0 = _mm_sub_epi8(A0, B0);
    const __m128i C1 = _mm_sub_epi8(A1, B1);
    _mm_storeu_si128((__m128i*)(dst + i +  0), C0);
    _mm_storeu_si128((__m128i*)(dst + i + 16), C1);
  }
  for (; i < length; ++i) dst[i] = src[i] - src[i - 1];
}

//------------------------------------------------------------------------------
// Horizontal filter.

static WEBP_INLINE void DoHorizontalFilter_SSE2(
    const uint8_t* WEBP_RESTRICT in, int width, int height, int stride,
    uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  // Leftmost pixel is the same as input for topmost scanline.
  out[0] = in[0];
  PredictLineLeft_SSE2(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  // Filter line-by-line.
  for (row = 1; row < height; ++row) {
    // Leftmost pixel is predicted from above.
    out[0] = in[0] - in[-stride];
    PredictLineLeft_SSE2(in + 1, out + 1, width - 1);
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Vertical filter.

static WEBP_INLINE void DoVerticalFilter_SSE2(const uint8_t* WEBP_RESTRICT in,
                                              int width, int height, int stride,
                                              uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  // Very first top-left pixel is copied.
  out[0] = in[0];
  // Rest of top scan-line is left-predicted.
  PredictLineLeft_SSE2(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  // Filter line-by-line.
  for (row = 1; row < height; ++row) {
    PredictLineTop_SSE2(in, in - stride, out, width);
    in += stride;
    out += stride;
  }
}

//------------------------------------------------------------------------------
// Gradient filter.

static WEBP_INLINE int GradientPredictor_SSE2(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;  // clip to 8bit
}

static void GradientPredictDirect_SSE2(const uint8_t* const row,
                                       const uint8_t* const top,
                                       uint8_t* WEBP_RESTRICT const out,
                                       int length) {
  const int max_pos = length & ~7;
  int i;
  const __m128i zero = _mm_setzero_si128();
  for (i = 0; i < max_pos; i += 8) {
    const __m128i A0 = _mm_loadl_epi64((const __m128i*)&row[i - 1]);
    const __m128i B0 = _mm_loadl_epi64((const __m128i*)&top[i]);
    const __m128i C0 = _mm_loadl_epi64((const __m128i*)&top[i - 1]);
    const __m128i D = _mm_loadl_epi64((const __m128i*)&row[i]);
    const __m128i A1 = _mm_unpacklo_epi8(A0, zero);
    const __m128i B1 = _mm_unpacklo_epi8(B0, zero);
    const __m128i C1 = _mm_unpacklo_epi8(C0, zero);
    const __m128i E = _mm_add_epi16(A1, B1);
    const __m128i F = _mm_sub_epi16(E, C1);
    const __m128i G = _mm_packus_epi16(F, zero);
    const __m128i H = _mm_sub_epi8(D, G);
    _mm_storel_epi64((__m128i*)(out + i), H);
  }
  for (; i < length; ++i) {
    const int delta = GradientPredictor_SSE2(row[i - 1], top[i], top[i - 1]);
    out[i] = (uint8_t)(row[i] - delta);
  }
}

static WEBP_INLINE void DoGradientFilter_SSE2(const uint8_t* WEBP_RESTRICT in,
                                              int width, int height, int stride,
                                              uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  // left prediction for top scan-line
  out[0] = in[0];
  PredictLineLeft_SSE2(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  // Filter line-by-line.
  for (row = 1; row < height; ++row) {
    out[0] = (uint8_t)(in[0] - in[-stride]);
    GradientPredictDirect_SSE2(in + 1, in + 1 - stride, out + 1, width - 1);
    in += stride;
    out += stride;
  }
}

#undef DCHECK

//------------------------------------------------------------------------------

static void HorizontalFilter_SSE2(const uint8_t* WEBP_RESTRICT data,
                                  int width, int height, int stride,
                                  uint8_t* WEBP_RESTRICT filtered_data) {
  DoHorizontalFilter_SSE2(data, width, height, stride, filtered_data);
}

static void VerticalFilter_SSE2(const uint8_t* WEBP_RESTRICT data,
                                int width, int height, int stride,
                                uint8_t* WEBP_RESTRICT filtered_data) {
  DoVerticalFilter_SSE2(data, width, height, stride, filtered_data);
}

static void GradientFilter_SSE2(const uint8_t* WEBP_RESTRICT data,
                                int width, int height, int stride,
                                uint8_t* WEBP_RESTRICT filtered_data) {
  DoGradientFilter_SSE2(data, width, height, stride, filtered_data);
}

//------------------------------------------------------------------------------
// Inverse transforms

static void HorizontalUnfilter_SSE2(const uint8_t* prev, const uint8_t* in,
                                    uint8_t* out, int width) {
  int i;
  __m128i last;
  out[0] = (uint8_t)(in[0] + (prev == NULL ? 0 : prev[0]));
  if (width <= 1) return;
  last = _mm_set_epi32(0, 0, 0, out[0]);
  for (i = 1; i + 8 <= width; i += 8) {
    const __m128i A0 = _mm_loadl_epi64((const __m128i*)(in + i));
    const __m128i A1 = _mm_add_epi8(A0, last);
    const __m128i A2 = _mm_slli_si128(A1, 1);
    const __m128i A3 = _mm_add_epi8(A1, A2);
    const __m128i A4 = _mm_slli_si128(A3, 2);
    const __m128i A5 = _mm_add_epi8(A3, A4);
    const __m128i A6 = _mm_slli_si128(A5, 4);
    const __m128i A7 = _mm_add_epi8(A5, A6);
    _mm_storel_epi64((__m128i*)(out + i), A7);
    last = _mm_srli_epi64(A7, 56);
  }
  for (; i < width; ++i) out[i] = (uint8_t)(in[i] + out[i - 1]);
}

static void VerticalUnfilter_SSE2(const uint8_t* prev, const uint8_t* in,
                                  uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_SSE2(NULL, in, out, width);
  } else {
    int i;
    const int max_pos = width & ~31;
    assert(width >= 0);
    for (i = 0; i < max_pos; i += 32) {
      const __m128i A0 = _mm_loadu_si128((const __m128i*)&in[i +  0]);
      const __m128i A1 = _mm_loadu_si128((const __m128i*)&in[i + 16]);
      const __m128i B0 = _mm_loadu_si128((const __m128i*)&prev[i +  0]);
      const __m128i B1 = _mm_loadu_si128((const __m128i*)&prev[i + 16]);
      const __m128i C0 = _mm_add_epi8(A0, B0);
      const __m128i C1 = _mm_add_epi8(A1, B1);
      _mm_storeu_si128((__m128i*)&out[i +  0], C0);
      _mm_storeu_si128((__m128i*)&out[i + 16], C1);
    }
    for (; i < width; ++i) out[i] = (uint8_t)(in[i] + prev[i]);
  }
}

static void GradientPredictInverse_SSE2(const uint8_t* const in,
                                        const uint8_t* const top,
                                        uint8_t* const row, int length) {
  if (length > 0) {
    int i;
    const int max_pos = length & ~7;
    const __m128i zero = _mm_setzero_si128();
    __m128i A = _mm_set_epi32(0, 0, 0, row[-1]);   // left sample
    for (i = 0; i < max_pos; i += 8) {
      const __m128i tmp0 = _mm_loadl_epi64((const __m128i*)&top[i]);
      const __m128i tmp1 = _mm_loadl_epi64((const __m128i*)&top[i - 1]);
      const __m128i B = _mm_unpacklo_epi8(tmp0, zero);
      const __m128i C = _mm_unpacklo_epi8(tmp1, zero);
      const __m128i D = _mm_loadl_epi64((const __m128i*)&in[i]);  // base input
      const __m128i E = _mm_sub_epi16(B, C);  // unclipped gradient basis B - C
      __m128i out = zero;                     // accumulator for output
      __m128i mask_hi = _mm_set_epi32(0, 0, 0, 0xff);
      int k = 8;
      while (1) {
        const __m128i tmp3 = _mm_add_epi16(A, E);           // delta = A + B - C
        const __m128i tmp4 = _mm_packus_epi16(tmp3, zero);  // saturate delta
        const __m128i tmp5 = _mm_add_epi8(tmp4, D);         // add to in[]
        A = _mm_and_si128(tmp5, mask_hi);                   // 1-complement clip
        out = _mm_or_si128(out, A);                         // accumulate output
        if (--k == 0) break;
        A = _mm_slli_si128(A, 1);                        // rotate left sample
        mask_hi = _mm_slli_si128(mask_hi, 1);            // rotate mask
        A = _mm_unpacklo_epi8(A, zero);                  // convert 8b->16b
      }
      A = _mm_srli_si128(A, 7);       // prepare left sample for next iteration
      _mm_storel_epi64((__m128i*)&row[i], out);
    }
    for (; i < length; ++i) {
      const int delta = GradientPredictor_SSE2(row[i - 1], top[i], top[i - 1]);
      row[i] = (uint8_t)(in[i] + delta);
    }
  }
}

static void GradientUnfilter_SSE2(const uint8_t* prev, const uint8_t* in,
                                  uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_SSE2(NULL, in, out, width);
  } else {
    out[0] = (uint8_t)(in[0] + prev[0]);  // predict from above
    GradientPredictInverse_SSE2(in + 1, prev + 1, out + 1, width - 1);
  }
}

//------------------------------------------------------------------------------
// Entry point

extern void VP8FiltersInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8FiltersInitSSE2(void) {
  WebPUnfilters[WEBP_FILTER_HORIZONTAL] = HorizontalUnfilter_SSE2;
#if defined(CHROMIUM)
  // TODO(crbug.com/654974)
  (void)VerticalUnfilter_SSE2;
#else
  WebPUnfilters[WEBP_FILTER_VERTICAL] = VerticalUnfilter_SSE2;
#endif
  WebPUnfilters[WEBP_FILTER_GRADIENT] = GradientUnfilter_SSE2;

  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter_SSE2;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter_SSE2;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter_SSE2;
}

#else  // !WEBP_USE_SSE2

WEBP_DSP_INIT_STUB(VP8FiltersInitSSE2)

#endif  // WEBP_USE_SSE2
