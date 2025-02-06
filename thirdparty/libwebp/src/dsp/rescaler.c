// Copyright 2014 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Rescaling functions
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>

#include "src/dsp/dsp.h"
#include "src/utils/rescaler_utils.h"

//------------------------------------------------------------------------------
// Implementations of critical functions ImportRow / ExportRow

#define ROUNDER (WEBP_RESCALER_ONE >> 1)
#define MULT_FIX(x, y) (((uint64_t)(x) * (y) + ROUNDER) >> WEBP_RESCALER_RFIX)
#define MULT_FIX_FLOOR(x, y) (((uint64_t)(x) * (y)) >> WEBP_RESCALER_RFIX)

//------------------------------------------------------------------------------
// Row import

void WebPRescalerImportRowExpand_C(WebPRescaler* WEBP_RESTRICT const wrk,
                                   const uint8_t* WEBP_RESTRICT src) {
  const int x_stride = wrk->num_channels;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  int channel;
  assert(!WebPRescalerInputDone(wrk));
  assert(wrk->x_expand);
  for (channel = 0; channel < x_stride; ++channel) {
    int x_in = channel;
    int x_out = channel;
    // simple bilinear interpolation
    int accum = wrk->x_add;
    rescaler_t left = (rescaler_t)src[x_in];
    rescaler_t right =
        (wrk->src_width > 1) ? (rescaler_t)src[x_in + x_stride] : left;
    x_in += x_stride;
    while (1) {
      wrk->frow[x_out] = right * wrk->x_add + (left - right) * accum;
      x_out += x_stride;
      if (x_out >= x_out_max) break;
      accum -= wrk->x_sub;
      if (accum < 0) {
        left = right;
        x_in += x_stride;
        assert(x_in < wrk->src_width * x_stride);
        right = (rescaler_t)src[x_in];
        accum += wrk->x_add;
      }
    }
    assert(wrk->x_sub == 0 /* <- special case for src_width=1 */ || accum == 0);
  }
}

void WebPRescalerImportRowShrink_C(WebPRescaler* WEBP_RESTRICT const wrk,
                                   const uint8_t* WEBP_RESTRICT src) {
  const int x_stride = wrk->num_channels;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  int channel;
  assert(!WebPRescalerInputDone(wrk));
  assert(!wrk->x_expand);
  for (channel = 0; channel < x_stride; ++channel) {
    int x_in = channel;
    int x_out = channel;
    uint32_t sum = 0;
    int accum = 0;
    while (x_out < x_out_max) {
      uint32_t base = 0;
      accum += wrk->x_add;
      while (accum > 0) {
        accum -= wrk->x_sub;
        assert(x_in < wrk->src_width * x_stride);
        base = src[x_in];
        sum += base;
        x_in += x_stride;
      }
      {        // Emit next horizontal pixel.
        const rescaler_t frac = base * (-accum);
        wrk->frow[x_out] = sum * wrk->x_sub - frac;
        // fresh fractional start for next pixel
        sum = (int)MULT_FIX(frac, wrk->fx_scale);
      }
      x_out += x_stride;
    }
    assert(accum == 0);
  }
}

//------------------------------------------------------------------------------
// Row export

void WebPRescalerExportRowExpand_C(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(wrk->y_expand);
  assert(wrk->y_sub != 0);
  if (wrk->y_accum == 0) {
    for (x_out = 0; x_out < x_out_max; ++x_out) {
      const uint32_t J = frow[x_out];
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  } else {
    const uint32_t B = WEBP_RESCALER_FRAC(-wrk->y_accum, wrk->y_sub);
    const uint32_t A = (uint32_t)(WEBP_RESCALER_ONE - B);
    for (x_out = 0; x_out < x_out_max; ++x_out) {
      const uint64_t I = (uint64_t)A * frow[x_out]
                       + (uint64_t)B * irow[x_out];
      const uint32_t J = (uint32_t)((I + ROUNDER) >> WEBP_RESCALER_RFIX);
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  }
}

void WebPRescalerExportRowShrink_C(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t yscale = wrk->fy_scale * (-wrk->y_accum);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(!wrk->y_expand);
  if (yscale) {
    for (x_out = 0; x_out < x_out_max; ++x_out) {
      const uint32_t frac = (uint32_t)MULT_FIX_FLOOR(frow[x_out], yscale);
      const int v = (int)MULT_FIX(irow[x_out] - frac, wrk->fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = frac;   // new fractional start
    }
  } else {
    for (x_out = 0; x_out < x_out_max; ++x_out) {
      const int v = (int)MULT_FIX(irow[x_out], wrk->fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = 0;
    }
  }
}

#undef MULT_FIX_FLOOR
#undef MULT_FIX
#undef ROUNDER

//------------------------------------------------------------------------------
// Main entry calls

void WebPRescalerImportRow(WebPRescaler* WEBP_RESTRICT const wrk,
                           const uint8_t* WEBP_RESTRICT src) {
  assert(!WebPRescalerInputDone(wrk));
  if (!wrk->x_expand) {
    WebPRescalerImportRowShrink(wrk, src);
  } else {
    WebPRescalerImportRowExpand(wrk, src);
  }
}

void WebPRescalerExportRow(WebPRescaler* const wrk) {
  if (wrk->y_accum <= 0) {
    assert(!WebPRescalerOutputDone(wrk));
    if (wrk->y_expand) {
      WebPRescalerExportRowExpand(wrk);
    } else if (wrk->fxy_scale) {
      WebPRescalerExportRowShrink(wrk);
    } else {  // special case
      int i;
      assert(wrk->src_height == wrk->dst_height && wrk->x_add == 1);
      assert(wrk->src_width == 1 && wrk->dst_width <= 2);
      for (i = 0; i < wrk->num_channels * wrk->dst_width; ++i) {
        wrk->dst[i] = wrk->irow[i];
        wrk->irow[i] = 0;
      }
    }
    wrk->y_accum += wrk->y_add;
    wrk->dst += wrk->dst_stride;
    ++wrk->dst_y;
  }
}

//------------------------------------------------------------------------------

WebPRescalerImportRowFunc WebPRescalerImportRowExpand;
WebPRescalerImportRowFunc WebPRescalerImportRowShrink;

WebPRescalerExportRowFunc WebPRescalerExportRowExpand;
WebPRescalerExportRowFunc WebPRescalerExportRowShrink;

extern VP8CPUInfo VP8GetCPUInfo;
extern void WebPRescalerDspInitSSE2(void);
extern void WebPRescalerDspInitMIPS32(void);
extern void WebPRescalerDspInitMIPSdspR2(void);
extern void WebPRescalerDspInitMSA(void);
extern void WebPRescalerDspInitNEON(void);

WEBP_DSP_INIT_FUNC(WebPRescalerDspInit) {
#if !defined(WEBP_REDUCE_SIZE)
#if !WEBP_NEON_OMIT_C_CODE
  WebPRescalerExportRowExpand = WebPRescalerExportRowExpand_C;
  WebPRescalerExportRowShrink = WebPRescalerExportRowShrink_C;
#endif

  WebPRescalerImportRowExpand = WebPRescalerImportRowExpand_C;
  WebPRescalerImportRowShrink = WebPRescalerImportRowShrink_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPRescalerDspInitSSE2();
    }
#endif
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      WebPRescalerDspInitMIPS32();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPRescalerDspInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      WebPRescalerDspInitMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    WebPRescalerDspInitNEON();
  }
#endif

  assert(WebPRescalerExportRowExpand != NULL);
  assert(WebPRescalerExportRowShrink != NULL);
  assert(WebPRescalerImportRowExpand != NULL);
  assert(WebPRescalerImportRowShrink != NULL);
#endif   // WEBP_REDUCE_SIZE
}
