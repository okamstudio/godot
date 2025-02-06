// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Alpha-plane compression.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "src/enc/vp8i_enc.h"
#include "src/dsp/dsp.h"
#include "src/utils/filters_utils.h"
#include "src/utils/quant_levels_utils.h"
#include "src/utils/utils.h"
#include "src/webp/encode.h"
#include "src/webp/format_constants.h"

// -----------------------------------------------------------------------------
// Encodes the given alpha data via specified compression method 'method'.
// The pre-processing (quantization) is performed if 'quality' is less than 100.
// For such cases, the encoding is lossy. The valid range is [0, 100] for
// 'quality' and [0, 1] for 'method':
//   'method = 0' - No compression;
//   'method = 1' - Use lossless coder on the alpha plane only
// 'filter' values [0, 4] correspond to prediction modes none, horizontal,
// vertical & gradient filters. The prediction mode 4 will try all the
// prediction modes 0 to 3 and pick the best one.
// 'effort_level': specifies how much effort must be spent to try and reduce
//  the compressed output size. In range 0 (quick) to 6 (slow).
//
// 'output' corresponds to the buffer containing compressed alpha data.
//          This buffer is allocated by this method and caller should call
//          WebPSafeFree(*output) when done.
// 'output_size' corresponds to size of this compressed alpha buffer.
//
// Returns 1 on successfully encoding the alpha and
//         0 if either:
//           invalid quality or method, or
//           memory allocation for the compressed data fails.

#include "src/enc/vp8li_enc.h"

static int EncodeLossless(const uint8_t* const data, int width, int height,
                          int effort_level,  // in [0..6] range
                          int use_quality_100, VP8LBitWriter* const bw,
                          WebPAuxStats* const stats) {
  int ok = 0;
  WebPConfig config;
  WebPPicture picture;

  if (!WebPPictureInit(&picture)) return 0;
  picture.width = width;
  picture.height = height;
  picture.use_argb = 1;
  picture.stats = stats;
  if (!WebPPictureAlloc(&picture)) return 0;

  // Transfer the alpha values to the green channel.
  WebPDispatchAlphaToGreen(data, width, picture.width, picture.height,
                           picture.argb, picture.argb_stride);

  if (!WebPConfigInit(&config)) return 0;
  config.lossless = 1;
  // Enable exact, or it would alter RGB values of transparent alpha, which is
  // normally OK but not here since we are not encoding the input image but  an
  // internal encoding-related image containing necessary exact information in
  // RGB channels.
  config.exact = 1;
  config.method = effort_level;  // impact is very small
  // Set a low default quality for encoding alpha. Ensure that Alpha quality at
  // lower methods (3 and below) is less than the threshold for triggering
  // costly 'BackwardReferencesTraceBackwards'.
  // If the alpha quality is set to 100 and the method to 6, allow for a high
  // lossless quality to trigger the cruncher.
  config.quality =
      (use_quality_100 && effort_level == 6) ? 100 : 8.f * effort_level;
  assert(config.quality >= 0 && config.quality <= 100.f);

  ok = VP8LEncodeStream(&config, &picture, bw);
  WebPPictureFree(&picture);
  ok = ok && !bw->error_;
  if (!ok) {
    VP8LBitWriterWipeOut(bw);
    return 0;
  }
  return 1;
}

// -----------------------------------------------------------------------------

// Small struct to hold the result of a filter mode compression attempt.
typedef struct {
  size_t score;
  VP8BitWriter bw;
  WebPAuxStats stats;
} FilterTrial;

// This function always returns an initialized 'bw' object, even upon error.
static int EncodeAlphaInternal(const uint8_t* const data, int width, int height,
                               int method, int filter, int reduce_levels,
                               int effort_level,  // in [0..6] range
                               uint8_t* const tmp_alpha,
                               FilterTrial* result) {
  int ok = 0;
  const uint8_t* alpha_src;
  WebPFilterFunc filter_func;
  uint8_t header;
  const size_t data_size = width * height;
  const uint8_t* output = NULL;
  size_t output_size = 0;
  VP8LBitWriter tmp_bw;

  assert((uint64_t)data_size == (uint64_t)width * height);  // as per spec
  assert(filter >= 0 && filter < WEBP_FILTER_LAST);
  assert(method >= ALPHA_NO_COMPRESSION);
  assert(method <= ALPHA_LOSSLESS_COMPRESSION);
  assert(sizeof(header) == ALPHA_HEADER_LEN);

  filter_func = WebPFilters[filter];
  if (filter_func != NULL) {
    filter_func(data, width, height, width, tmp_alpha);
    alpha_src = tmp_alpha;
  }  else {
    alpha_src = data;
  }

  if (method != ALPHA_NO_COMPRESSION) {
    ok = VP8LBitWriterInit(&tmp_bw, data_size >> 3);
    ok = ok && EncodeLossless(alpha_src, width, height, effort_level,
                              !reduce_levels, &tmp_bw, &result->stats);
    if (ok) {
      output = VP8LBitWriterFinish(&tmp_bw);
      if (tmp_bw.error_) {
        VP8LBitWriterWipeOut(&tmp_bw);
        memset(&result->bw, 0, sizeof(result->bw));
        return 0;
      }
      output_size = VP8LBitWriterNumBytes(&tmp_bw);
      if (output_size > data_size) {
        // compressed size is larger than source! Revert to uncompressed mode.
        method = ALPHA_NO_COMPRESSION;
        VP8LBitWriterWipeOut(&tmp_bw);
      }
    } else {
      VP8LBitWriterWipeOut(&tmp_bw);
      memset(&result->bw, 0, sizeof(result->bw));
      return 0;
    }
  }

  if (method == ALPHA_NO_COMPRESSION) {
    output = alpha_src;
    output_size = data_size;
    ok = 1;
  }

  // Emit final result.
  header = method | (filter << 2);
  if (reduce_levels) header |= ALPHA_PREPROCESSED_LEVELS << 4;

  if (!VP8BitWriterInit(&result->bw, ALPHA_HEADER_LEN + output_size)) ok = 0;
  ok = ok && VP8BitWriterAppend(&result->bw, &header, ALPHA_HEADER_LEN);
  ok = ok && VP8BitWriterAppend(&result->bw, output, output_size);

  if (method != ALPHA_NO_COMPRESSION) {
    VP8LBitWriterWipeOut(&tmp_bw);
  }
  ok = ok && !result->bw.error_;
  result->score = VP8BitWriterSize(&result->bw);
  return ok;
}

// -----------------------------------------------------------------------------

static int GetNumColors(const uint8_t* data, int width, int height,
                        int stride) {
  int j;
  int colors = 0;
  uint8_t color[256] = { 0 };

  for (j = 0; j < height; ++j) {
    int i;
    const uint8_t* const p = data + j * stride;
    for (i = 0; i < width; ++i) {
      color[p[i]] = 1;
    }
  }
  for (j = 0; j < 256; ++j) {
    if (color[j] > 0) ++colors;
  }
  return colors;
}

#define FILTER_TRY_NONE (1 << WEBP_FILTER_NONE)
#define FILTER_TRY_ALL ((1 << WEBP_FILTER_LAST) - 1)

// Given the input 'filter' option, return an OR'd bit-set of filters to try.
static uint32_t GetFilterMap(const uint8_t* alpha, int width, int height,
                             int filter, int effort_level) {
  uint32_t bit_map = 0U;
  if (filter == WEBP_FILTER_FAST) {
    // Quick estimate of the best candidate.
    int try_filter_none = (effort_level > 3);
    const int kMinColorsForFilterNone = 16;
    const int kMaxColorsForFilterNone = 192;
    const int num_colors = GetNumColors(alpha, width, height, width);
    // For low number of colors, NONE yields better compression.
    filter = (num_colors <= kMinColorsForFilterNone)
        ? WEBP_FILTER_NONE
        : WebPEstimateBestFilter(alpha, width, height, width);
    bit_map |= 1 << filter;
    // For large number of colors, try FILTER_NONE in addition to the best
    // filter as well.
    if (try_filter_none || num_colors > kMaxColorsForFilterNone) {
      bit_map |= FILTER_TRY_NONE;
    }
  } else if (filter == WEBP_FILTER_NONE) {
    bit_map = FILTER_TRY_NONE;
  } else {  // WEBP_FILTER_BEST -> try all
    bit_map = FILTER_TRY_ALL;
  }
  return bit_map;
}

static void InitFilterTrial(FilterTrial* const score) {
  score->score = (size_t)~0U;
  VP8BitWriterInit(&score->bw, 0);
}

static int ApplyFiltersAndEncode(const uint8_t* alpha, int width, int height,
                                 size_t data_size, int method, int filter,
                                 int reduce_levels, int effort_level,
                                 uint8_t** const output,
                                 size_t* const output_size,
                                 WebPAuxStats* const stats) {
  int ok = 1;
  FilterTrial best;
  uint32_t try_map =
      GetFilterMap(alpha, width, height, filter, effort_level);
  InitFilterTrial(&best);

  if (try_map != FILTER_TRY_NONE) {
    uint8_t* filtered_alpha =  (uint8_t*)WebPSafeMalloc(1ULL, data_size);
    if (filtered_alpha == NULL) return 0;

    for (filter = WEBP_FILTER_NONE; ok && try_map; ++filter, try_map >>= 1) {
      if (try_map & 1) {
        FilterTrial trial;
        ok = EncodeAlphaInternal(alpha, width, height, method, filter,
                                 reduce_levels, effort_level, filtered_alpha,
                                 &trial);
        if (ok && trial.score < best.score) {
          VP8BitWriterWipeOut(&best.bw);
          best = trial;
        } else {
          VP8BitWriterWipeOut(&trial.bw);
        }
      }
    }
    WebPSafeFree(filtered_alpha);
  } else {
    ok = EncodeAlphaInternal(alpha, width, height, method, WEBP_FILTER_NONE,
                             reduce_levels, effort_level, NULL, &best);
  }
  if (ok) {
#if !defined(WEBP_DISABLE_STATS)
    if (stats != NULL) {
      stats->lossless_features = best.stats.lossless_features;
      stats->histogram_bits = best.stats.histogram_bits;
      stats->transform_bits = best.stats.transform_bits;
      stats->cross_color_transform_bits = best.stats.cross_color_transform_bits;
      stats->cache_bits = best.stats.cache_bits;
      stats->palette_size = best.stats.palette_size;
      stats->lossless_size = best.stats.lossless_size;
      stats->lossless_hdr_size = best.stats.lossless_hdr_size;
      stats->lossless_data_size = best.stats.lossless_data_size;
    }
#else
    (void)stats;
#endif
    *output_size = VP8BitWriterSize(&best.bw);
    *output = VP8BitWriterBuf(&best.bw);
  } else {
    VP8BitWriterWipeOut(&best.bw);
  }
  return ok;
}

static int EncodeAlpha(VP8Encoder* const enc,
                       int quality, int method, int filter,
                       int effort_level,
                       uint8_t** const output, size_t* const output_size) {
  const WebPPicture* const pic = enc->pic_;
  const int width = pic->width;
  const int height = pic->height;

  uint8_t* quant_alpha = NULL;
  const size_t data_size = width * height;
  uint64_t sse = 0;
  int ok = 1;
  const int reduce_levels = (quality < 100);

  // quick correctness checks
  assert((uint64_t)data_size == (uint64_t)width * height);  // as per spec
  assert(enc != NULL && pic != NULL && pic->a != NULL);
  assert(output != NULL && output_size != NULL);
  assert(width > 0 && height > 0);
  assert(pic->a_stride >= width);
  assert(filter >= WEBP_FILTER_NONE && filter <= WEBP_FILTER_FAST);

  if (quality < 0 || quality > 100) {
    return WebPEncodingSetError(pic, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }

  if (method < ALPHA_NO_COMPRESSION || method > ALPHA_LOSSLESS_COMPRESSION) {
    return WebPEncodingSetError(pic, VP8_ENC_ERROR_INVALID_CONFIGURATION);
  }

  if (method == ALPHA_NO_COMPRESSION) {
    // Don't filter, as filtering will make no impact on compressed size.
    filter = WEBP_FILTER_NONE;
  }

  quant_alpha = (uint8_t*)WebPSafeMalloc(1ULL, data_size);
  if (quant_alpha == NULL) {
    return WebPEncodingSetError(pic, VP8_ENC_ERROR_OUT_OF_MEMORY);
  }

  // Extract alpha data (width x height) from raw_data (stride x height).
  WebPCopyPlane(pic->a, pic->a_stride, quant_alpha, width, width, height);

  if (reduce_levels) {  // No Quantization required for 'quality = 100'.
    // 16 alpha levels gives quite a low MSE w.r.t original alpha plane hence
    // mapped to moderate quality 70. Hence Quality:[0, 70] -> Levels:[2, 16]
    // and Quality:]70, 100] -> Levels:]16, 256].
    const int alpha_levels = (quality <= 70) ? (2 + quality / 5)
                                             : (16 + (quality - 70) * 8);
    ok = QuantizeLevels(quant_alpha, width, height, alpha_levels, &sse);
  }

  if (ok) {
    VP8FiltersInit();
    ok = ApplyFiltersAndEncode(quant_alpha, width, height, data_size, method,
                               filter, reduce_levels, effort_level, output,
                               output_size, pic->stats);
    if (!ok) {
      WebPEncodingSetError(pic, VP8_ENC_ERROR_OUT_OF_MEMORY);  // imprecise
    }
#if !defined(WEBP_DISABLE_STATS)
    if (pic->stats != NULL) {  // need stats?
      pic->stats->coded_size += (int)(*output_size);
      enc->sse_[3] = sse;
    }
#endif
  }

  WebPSafeFree(quant_alpha);
  return ok;
}

//------------------------------------------------------------------------------
// Main calls

static int CompressAlphaJob(void* arg1, void* unused) {
  VP8Encoder* const enc = (VP8Encoder*)arg1;
  const WebPConfig* config = enc->config_;
  uint8_t* alpha_data = NULL;
  size_t alpha_size = 0;
  const int effort_level = config->method;  // maps to [0..6]
  const WEBP_FILTER_TYPE filter =
      (config->alpha_filtering == 0) ? WEBP_FILTER_NONE :
      (config->alpha_filtering == 1) ? WEBP_FILTER_FAST :
                                       WEBP_FILTER_BEST;
  if (!EncodeAlpha(enc, config->alpha_quality, config->alpha_compression,
                   filter, effort_level, &alpha_data, &alpha_size)) {
    return 0;
  }
  if (alpha_size != (uint32_t)alpha_size) {  // Soundness check.
    WebPSafeFree(alpha_data);
    return 0;
  }
  enc->alpha_data_size_ = (uint32_t)alpha_size;
  enc->alpha_data_ = alpha_data;
  (void)unused;
  return 1;
}

void VP8EncInitAlpha(VP8Encoder* const enc) {
  WebPInitAlphaProcessing();
  enc->has_alpha_ = WebPPictureHasTransparency(enc->pic_);
  enc->alpha_data_ = NULL;
  enc->alpha_data_size_ = 0;
  if (enc->thread_level_ > 0) {
    WebPWorker* const worker = &enc->alpha_worker_;
    WebPGetWorkerInterface()->Init(worker);
    worker->data1 = enc;
    worker->data2 = NULL;
    worker->hook = CompressAlphaJob;
  }
}

int VP8EncStartAlpha(VP8Encoder* const enc) {
  if (enc->has_alpha_) {
    if (enc->thread_level_ > 0) {
      WebPWorker* const worker = &enc->alpha_worker_;
      // Makes sure worker is good to go.
      if (!WebPGetWorkerInterface()->Reset(worker)) {
        return WebPEncodingSetError(enc->pic_, VP8_ENC_ERROR_OUT_OF_MEMORY);
      }
      WebPGetWorkerInterface()->Launch(worker);
      return 1;
    } else {
      return CompressAlphaJob(enc, NULL);   // just do the job right away
    }
  }
  return 1;
}

int VP8EncFinishAlpha(VP8Encoder* const enc) {
  if (enc->has_alpha_) {
    if (enc->thread_level_ > 0) {
      WebPWorker* const worker = &enc->alpha_worker_;
      if (!WebPGetWorkerInterface()->Sync(worker)) return 0;  // error
    }
  }
  return WebPReportProgress(enc->pic_, enc->percent_ + 20, &enc->percent_);
}

int VP8EncDeleteAlpha(VP8Encoder* const enc) {
  int ok = 1;
  if (enc->thread_level_ > 0) {
    WebPWorker* const worker = &enc->alpha_worker_;
    // finish anything left in flight
    ok = WebPGetWorkerInterface()->Sync(worker);
    // still need to end the worker, even if !ok
    WebPGetWorkerInterface()->End(worker);
  }
  WebPSafeFree(enc->alpha_data_);
  enc->alpha_data_ = NULL;
  enc->alpha_data_size_ = 0;
  enc->has_alpha_ = 0;
  return ok;
}
