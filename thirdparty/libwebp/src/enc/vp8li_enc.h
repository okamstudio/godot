// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Lossless encoder: internal header.
//
// Author: Vikas Arora (vikaas.arora@gmail.com)

#ifndef WEBP_ENC_VP8LI_ENC_H_
#define WEBP_ENC_VP8LI_ENC_H_

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif
// Either WEBP_NEAR_LOSSLESS is defined as 0 in config.h when compiling to
// disable near-lossless, or it is enabled by default.
#ifndef WEBP_NEAR_LOSSLESS
#define WEBP_NEAR_LOSSLESS 1
#endif

#include "src/enc/backward_references_enc.h"
#include "src/enc/histogram_enc.h"
#include "src/utils/bit_writer_utils.h"
#include "src/webp/encode.h"
#include "src/webp/format_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// maximum value of transform_bits_ in VP8LEncoder.
#define MAX_TRANSFORM_BITS (MIN_TRANSFORM_BITS + (1 << NUM_TRANSFORM_BITS) - 1)

typedef enum {
  kEncoderNone = 0,
  kEncoderARGB,
  kEncoderNearLossless,
  kEncoderPalette
} VP8LEncoderARGBContent;

typedef struct {
  const WebPConfig* config_;      // user configuration and parameters
  const WebPPicture* pic_;        // input picture.

  uint32_t* argb_;                       // Transformed argb image data.
  VP8LEncoderARGBContent argb_content_;  // Content type of the argb buffer.
  uint32_t* argb_scratch_;               // Scratch memory for argb rows
                                         // (used for prediction).
  uint32_t* transform_data_;             // Scratch memory for transform data.
  uint32_t* transform_mem_;              // Currently allocated memory.
  size_t    transform_mem_size_;         // Currently allocated memory size.

  int       current_width_;       // Corresponds to packed image width.

  // Encoding parameters derived from quality parameter.
  int histo_bits_;
  int predictor_transform_bits_;    // <= MAX_TRANSFORM_BITS
  int cross_color_transform_bits_;  // <= MAX_TRANSFORM_BITS
  int cache_bits_;        // If equal to 0, don't use color cache.

  // Encoding parameters derived from image characteristics.
  int use_cross_color_;
  int use_subtract_green_;
  int use_predict_;
  int use_palette_;
  int palette_size_;
  uint32_t palette_[MAX_PALETTE_SIZE];
  // Sorted version of palette_ for cache purposes.
  uint32_t palette_sorted_[MAX_PALETTE_SIZE];

  // Some 'scratch' (potentially large) objects.
  struct VP8LBackwardRefs refs_[4];  // Backward Refs array for temporaries.
  VP8LHashChain hash_chain_;         // HashChain data for constructing
                                     // backward references.
} VP8LEncoder;

//------------------------------------------------------------------------------
// internal functions. Not public.

// Encodes the picture.
// Returns 0 if config or picture is NULL or picture doesn't have valid argb
// input.
int VP8LEncodeImage(const WebPConfig* const config,
                    const WebPPicture* const picture);

// Encodes the main image stream using the supplied bit writer.
// Returns false in case of error (stored in picture->error_code).
int VP8LEncodeStream(const WebPConfig* const config,
                     const WebPPicture* const picture, VP8LBitWriter* const bw);

#if (WEBP_NEAR_LOSSLESS == 1)
// in near_lossless.c
// Near lossless preprocessing in RGB color-space.
int VP8ApplyNearLossless(const WebPPicture* const picture, int quality,
                         uint32_t* const argb_dst);
#endif

//------------------------------------------------------------------------------
// Image transforms in predictor.c.

// pic and percent are for progress.
// Returns false in case of error (stored in pic->error_code).
int VP8LResidualImage(int width, int height, int min_bits, int max_bits,
                      int low_effort, uint32_t* const argb,
                      uint32_t* const argb_scratch, uint32_t* const image,
                      int near_lossless, int exact, int used_subtract_green,
                      const WebPPicture* const pic, int percent_range,
                      int* const percent, int* const best_bits);

int VP8LColorSpaceTransform(int width, int height, int bits, int quality,
                            uint32_t* const argb, uint32_t* image,
                            const WebPPicture* const pic, int percent_range,
                            int* const percent, int* const best_bits);

void VP8LOptimizeSampling(uint32_t* const image, int full_width,
                          int full_height, int bits, int max_bits,
                          int* best_bits_out);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_ENC_VP8LI_ENC_H_
