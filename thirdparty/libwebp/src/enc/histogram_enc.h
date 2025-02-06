// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Author: Jyrki Alakuijala (jyrki@google.com)
//
// Models the histograms of literal and distance codes.

#ifndef WEBP_ENC_HISTOGRAM_ENC_H_
#define WEBP_ENC_HISTOGRAM_ENC_H_

#include <string.h>

#include "src/enc/backward_references_enc.h"
#include "src/webp/format_constants.h"
#include "src/webp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Not a trivial literal symbol.
#define VP8L_NON_TRIVIAL_SYM (0xffffffff)

// A simple container for histograms of data.
typedef struct {
  // literal_ contains green literal, palette-code and
  // copy-length-prefix histogram
  uint32_t* literal_;         // Pointer to the allocated buffer for literal.
  uint32_t red_[NUM_LITERAL_CODES];
  uint32_t blue_[NUM_LITERAL_CODES];
  uint32_t alpha_[NUM_LITERAL_CODES];
  // Backward reference prefix-code histogram.
  uint32_t distance_[NUM_DISTANCE_CODES];
  int palette_code_bits_;
  uint32_t trivial_symbol_;  // True, if histograms for Red, Blue & Alpha
                             // literal symbols are single valued.
  uint64_t bit_cost_;        // cached value of bit cost.
  uint64_t literal_cost_;    // Cached values of dominant entropy costs:
  uint64_t red_cost_;        // literal, red & blue.
  uint64_t blue_cost_;
  uint8_t is_used_[5];       // 5 for literal, red, blue, alpha, distance
} VP8LHistogram;

// Collection of histograms with fixed capacity, allocated as one
// big memory chunk. Can be destroyed by calling WebPSafeFree().
typedef struct {
  int size;         // number of slots currently in use
  int max_size;     // maximum capacity
  VP8LHistogram** histograms;
} VP8LHistogramSet;

// Create the histogram.
//
// The input data is the PixOrCopy data, which models the literals, stop
// codes and backward references (both distances and lengths).  Also: if
// palette_code_bits is >= 0, initialize the histogram with this value.
void VP8LHistogramCreate(VP8LHistogram* const p,
                         const VP8LBackwardRefs* const refs,
                         int palette_code_bits);

// Set the palette_code_bits and reset the stats.
// If init_arrays is true, the arrays are also filled with 0's.
void VP8LHistogramInit(VP8LHistogram* const p, int palette_code_bits,
                       int init_arrays);

// Collect all the references into a histogram (without reset)
void VP8LHistogramStoreRefs(const VP8LBackwardRefs* const refs,
                            VP8LHistogram* const histo);

// Free the memory allocated for the histogram.
void VP8LFreeHistogram(VP8LHistogram* const histo);

// Free the memory allocated for the histogram set.
void VP8LFreeHistogramSet(VP8LHistogramSet* const histo);

// Allocate an array of pointer to histograms, allocated and initialized
// using 'cache_bits'. Return NULL in case of memory error.
VP8LHistogramSet* VP8LAllocateHistogramSet(int size, int cache_bits);

// Set the histograms in set to 0.
void VP8LHistogramSetClear(VP8LHistogramSet* const set);

// Allocate and initialize histogram object with specified 'cache_bits'.
// Returns NULL in case of memory error.
// Special case of VP8LAllocateHistogramSet, with size equals 1.
VP8LHistogram* VP8LAllocateHistogram(int cache_bits);

// Accumulate a token 'v' into a histogram.
void VP8LHistogramAddSinglePixOrCopy(VP8LHistogram* const histo,
                                     const PixOrCopy* const v,
                                     int (*const distance_modifier)(int, int),
                                     int distance_modifier_arg0);

static WEBP_INLINE int VP8LHistogramNumCodes(int palette_code_bits) {
  return NUM_LITERAL_CODES + NUM_LENGTH_CODES +
      ((palette_code_bits > 0) ? (1 << palette_code_bits) : 0);
}

// Builds the histogram image. pic and percent are for progress.
// Returns false in case of error (stored in pic->error_code).
int VP8LGetHistoImageSymbols(int xsize, int ysize,
                             const VP8LBackwardRefs* const refs, int quality,
                             int low_effort, int histogram_bits, int cache_bits,
                             VP8LHistogramSet* const image_histo,
                             VP8LHistogram* const tmp_histo,
                             uint32_t* const histogram_symbols,
                             const WebPPicture* const pic, int percent_range,
                             int* const percent);

// Returns the entropy for the symbols in the input array.
uint64_t VP8LBitsEntropy(const uint32_t* const array, int n);

// Estimate how many bits the combined entropy of literals and distance
// approximately maps to.
uint64_t VP8LHistogramEstimateBits(VP8LHistogram* const p);

#ifdef __cplusplus
}
#endif

#endif  // WEBP_ENC_HISTOGRAM_ENC_H_
