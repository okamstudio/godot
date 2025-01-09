// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Image transforms and color space conversion methods for lossless decoder.
//
// Authors: Vikas Arora (vikaas.arora@gmail.com)
//          Jyrki Alakuijala (jyrki@google.com)

#ifndef WEBP_DSP_LOSSLESS_H_
#define WEBP_DSP_LOSSLESS_H_

#include "src/webp/types.h"
#include "src/webp/decode.h"

#include "src/dsp/dsp.h"
#include "src/enc/histogram_enc.h"
#include "src/utils/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Decoding

typedef uint32_t (*VP8LPredictorFunc)(const uint32_t* const left,
                                      const uint32_t* const top);
extern VP8LPredictorFunc VP8LPredictors[16];

uint32_t VP8LPredictor2_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor3_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor4_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor5_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor6_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor7_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor8_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor9_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor10_C(const uint32_t* const left,
                           const uint32_t* const top);
uint32_t VP8LPredictor11_C(const uint32_t* const left,
                           const uint32_t* const top);
uint32_t VP8LPredictor12_C(const uint32_t* const left,
                           const uint32_t* const top);
uint32_t VP8LPredictor13_C(const uint32_t* const left,
                           const uint32_t* const top);

// These Add/Sub function expects upper[-1] and out[-1] to be readable.
typedef void (*VP8LPredictorAddSubFunc)(const uint32_t* in,
                                        const uint32_t* upper, int num_pixels,
                                        uint32_t* WEBP_RESTRICT out);
extern VP8LPredictorAddSubFunc VP8LPredictorsAdd[16];
extern VP8LPredictorAddSubFunc VP8LPredictorsAdd_C[16];

typedef void (*VP8LProcessDecBlueAndRedFunc)(const uint32_t* src,
                                             int num_pixels, uint32_t* dst);
extern VP8LProcessDecBlueAndRedFunc VP8LAddGreenToBlueAndRed;

typedef struct {
  // Note: the members are uint8_t, so that any negative values are
  // automatically converted to "mod 256" values.
  uint8_t green_to_red_;
  uint8_t green_to_blue_;
  uint8_t red_to_blue_;
} VP8LMultipliers;
typedef void (*VP8LTransformColorInverseFunc)(const VP8LMultipliers* const m,
                                              const uint32_t* src,
                                              int num_pixels, uint32_t* dst);
extern VP8LTransformColorInverseFunc VP8LTransformColorInverse;

struct VP8LTransform;  // Defined in dec/vp8li.h.

// Performs inverse transform of data given transform information, start and end
// rows. Transform will be applied to rows [row_start, row_end[.
// The *in and *out pointers refer to source and destination data respectively
// corresponding to the intermediate row (row_start).
void VP8LInverseTransform(const struct VP8LTransform* const transform,
                          int row_start, int row_end,
                          const uint32_t* const in, uint32_t* const out);

// Color space conversion.
typedef void (*VP8LConvertFunc)(const uint32_t* WEBP_RESTRICT src,
                                int num_pixels, uint8_t* WEBP_RESTRICT dst);
extern VP8LConvertFunc VP8LConvertBGRAToRGB;
extern VP8LConvertFunc VP8LConvertBGRAToRGBA;
extern VP8LConvertFunc VP8LConvertBGRAToRGBA4444;
extern VP8LConvertFunc VP8LConvertBGRAToRGB565;
extern VP8LConvertFunc VP8LConvertBGRAToBGR;

// Converts from BGRA to other color spaces.
void VP8LConvertFromBGRA(const uint32_t* const in_data, int num_pixels,
                         WEBP_CSP_MODE out_colorspace, uint8_t* const rgba);

typedef void (*VP8LMapARGBFunc)(const uint32_t* src,
                                const uint32_t* const color_map,
                                uint32_t* dst, int y_start,
                                int y_end, int width);
typedef void (*VP8LMapAlphaFunc)(const uint8_t* src,
                                 const uint32_t* const color_map,
                                 uint8_t* dst, int y_start,
                                 int y_end, int width);

extern VP8LMapARGBFunc VP8LMapColor32b;
extern VP8LMapAlphaFunc VP8LMapColor8b;

// Similar to the static method ColorIndexInverseTransform() that is part of
// lossless.c, but used only for alpha decoding. It takes uint8_t (rather than
// uint32_t) arguments for 'src' and 'dst'.
void VP8LColorIndexInverseTransformAlpha(
    const struct VP8LTransform* const transform, int y_start, int y_end,
    const uint8_t* src, uint8_t* dst);

// Expose some C-only fallback functions
void VP8LTransformColorInverse_C(const VP8LMultipliers* const m,
                                 const uint32_t* src, int num_pixels,
                                 uint32_t* dst);

void VP8LConvertBGRAToRGB_C(const uint32_t* WEBP_RESTRICT src, int num_pixels,
                            uint8_t* WEBP_RESTRICT dst);
void VP8LConvertBGRAToRGBA_C(const uint32_t* WEBP_RESTRICT src, int num_pixels,
                             uint8_t* WEBP_RESTRICT dst);
void VP8LConvertBGRAToRGBA4444_C(const uint32_t* WEBP_RESTRICT src,
                                 int num_pixels, uint8_t* WEBP_RESTRICT dst);
void VP8LConvertBGRAToRGB565_C(const uint32_t* WEBP_RESTRICT src,
                               int num_pixels, uint8_t* WEBP_RESTRICT dst);
void VP8LConvertBGRAToBGR_C(const uint32_t* WEBP_RESTRICT src, int num_pixels,
                            uint8_t* WEBP_RESTRICT dst);
void VP8LAddGreenToBlueAndRed_C(const uint32_t* src, int num_pixels,
                                uint32_t* dst);

// Must be called before calling any of the above methods.
void VP8LDspInit(void);

//------------------------------------------------------------------------------
// Encoding

typedef void (*VP8LProcessEncBlueAndRedFunc)(uint32_t* dst, int num_pixels);
extern VP8LProcessEncBlueAndRedFunc VP8LSubtractGreenFromBlueAndRed;
typedef void (*VP8LTransformColorFunc)(
    const VP8LMultipliers* WEBP_RESTRICT const m, uint32_t* WEBP_RESTRICT dst,
    int num_pixels);
extern VP8LTransformColorFunc VP8LTransformColor;
typedef void (*VP8LCollectColorBlueTransformsFunc)(
    const uint32_t* WEBP_RESTRICT argb, int stride,
    int tile_width, int tile_height,
    int green_to_blue, int red_to_blue, uint32_t histo[]);
extern VP8LCollectColorBlueTransformsFunc VP8LCollectColorBlueTransforms;

typedef void (*VP8LCollectColorRedTransformsFunc)(
    const uint32_t* WEBP_RESTRICT argb, int stride,
    int tile_width, int tile_height,
    int green_to_red, uint32_t histo[]);
extern VP8LCollectColorRedTransformsFunc VP8LCollectColorRedTransforms;

// Expose some C-only fallback functions
void VP8LTransformColor_C(const VP8LMultipliers* WEBP_RESTRICT const m,
                          uint32_t* WEBP_RESTRICT data, int num_pixels);
void VP8LSubtractGreenFromBlueAndRed_C(uint32_t* argb_data, int num_pixels);
void VP8LCollectColorRedTransforms_C(const uint32_t* WEBP_RESTRICT argb,
                                     int stride,
                                     int tile_width, int tile_height,
                                     int green_to_red, uint32_t histo[]);
void VP8LCollectColorBlueTransforms_C(const uint32_t* WEBP_RESTRICT argb,
                                      int stride,
                                      int tile_width, int tile_height,
                                      int green_to_blue, int red_to_blue,
                                      uint32_t histo[]);

extern VP8LPredictorAddSubFunc VP8LPredictorsSub[16];
extern VP8LPredictorAddSubFunc VP8LPredictorsSub_C[16];

// -----------------------------------------------------------------------------
// Huffman-cost related functions.

typedef uint32_t (*VP8LCostFunc)(const uint32_t* population, int length);
typedef uint32_t (*VP8LCostCombinedFunc)(const uint32_t* WEBP_RESTRICT X,
                                         const uint32_t* WEBP_RESTRICT Y,
                                         int length);
typedef uint64_t (*VP8LCombinedShannonEntropyFunc)(const uint32_t X[256],
                                                   const uint32_t Y[256]);
typedef uint64_t (*VP8LShannonEntropyFunc)(const uint32_t* X, int length);

extern VP8LCostFunc VP8LExtraCost;
extern VP8LCostCombinedFunc VP8LExtraCostCombined;
extern VP8LCombinedShannonEntropyFunc VP8LCombinedShannonEntropy;
extern VP8LShannonEntropyFunc VP8LShannonEntropy;

typedef struct {        // small struct to hold counters
  int counts[2];        // index: 0=zero streak, 1=non-zero streak
  int streaks[2][2];    // [zero/non-zero][streak<3 / streak>=3]
} VP8LStreaks;

typedef struct {            // small struct to hold bit entropy results
  uint64_t entropy;         // entropy
  uint32_t sum;             // sum of the population
  int nonzeros;             // number of non-zero elements in the population
  uint32_t max_val;         // maximum value in the population
  uint32_t nonzero_code;    // index of the last non-zero in the population
} VP8LBitEntropy;

void VP8LBitEntropyInit(VP8LBitEntropy* const entropy);

// Get the combined symbol bit entropy and Huffman cost stats for the
// distributions 'X' and 'Y'. Those results can then be refined according to
// codec specific heuristics.
typedef void (*VP8LGetCombinedEntropyUnrefinedFunc)(
    const uint32_t X[], const uint32_t Y[], int length,
    VP8LBitEntropy* WEBP_RESTRICT const bit_entropy,
    VP8LStreaks* WEBP_RESTRICT const stats);
extern VP8LGetCombinedEntropyUnrefinedFunc VP8LGetCombinedEntropyUnrefined;

// Get the entropy for the distribution 'X'.
typedef void (*VP8LGetEntropyUnrefinedFunc)(
    const uint32_t X[], int length,
    VP8LBitEntropy* WEBP_RESTRICT const bit_entropy,
    VP8LStreaks* WEBP_RESTRICT const stats);
extern VP8LGetEntropyUnrefinedFunc VP8LGetEntropyUnrefined;

void VP8LBitsEntropyUnrefined(const uint32_t* WEBP_RESTRICT const array, int n,
                              VP8LBitEntropy* WEBP_RESTRICT const entropy);

typedef void (*VP8LAddVectorFunc)(const uint32_t* WEBP_RESTRICT a,
                                  const uint32_t* WEBP_RESTRICT b,
                                  uint32_t* WEBP_RESTRICT out, int size);
extern VP8LAddVectorFunc VP8LAddVector;
typedef void (*VP8LAddVectorEqFunc)(const uint32_t* WEBP_RESTRICT a,
                                    uint32_t* WEBP_RESTRICT out, int size);
extern VP8LAddVectorEqFunc VP8LAddVectorEq;
void VP8LHistogramAdd(const VP8LHistogram* WEBP_RESTRICT const a,
                      const VP8LHistogram* WEBP_RESTRICT const b,
                      VP8LHistogram* WEBP_RESTRICT const out);

// -----------------------------------------------------------------------------
// PrefixEncode()

typedef int (*VP8LVectorMismatchFunc)(const uint32_t* const array1,
                                      const uint32_t* const array2, int length);
// Returns the first index where array1 and array2 are different.
extern VP8LVectorMismatchFunc VP8LVectorMismatch;

typedef void (*VP8LBundleColorMapFunc)(const uint8_t* WEBP_RESTRICT const row,
                                       int width, int xbits,
                                       uint32_t* WEBP_RESTRICT dst);
extern VP8LBundleColorMapFunc VP8LBundleColorMap;
void VP8LBundleColorMap_C(const uint8_t* WEBP_RESTRICT const row,
                          int width, int xbits, uint32_t* WEBP_RESTRICT dst);

// Must be called before calling any of the above methods.
void VP8LEncDspInit(void);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_DSP_LOSSLESS_H_
