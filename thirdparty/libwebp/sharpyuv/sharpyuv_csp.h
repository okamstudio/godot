// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Colorspace utilities.

#ifndef WEBP_SHARPYUV_SHARPYUV_CSP_H_
#define WEBP_SHARPYUV_SHARPYUV_CSP_H_

#include "sharpyuv/sharpyuv.h"

#ifdef __cplusplus
extern "C" {
#endif

// Range of YUV values.
typedef enum {
  kSharpYuvRangeFull,     // YUV values between [0;255] (for 8 bit)
  kSharpYuvRangeLimited   // Y in [16;235], YUV in [16;240] (for 8 bit)
} SharpYuvRange;

// Constants that define a YUV color space.
typedef struct {
  // Kr and Kb are defined such that:
  // Y = Kr * r + Kg * g + Kb * b where Kg = 1 - Kr - Kb.
  float kr;
  float kb;
  int bit_depth;  // 8, 10 or 12
  SharpYuvRange range;
} SharpYuvColorSpace;

// Fills in 'matrix' for the given YUVColorSpace.
SHARPYUV_EXTERN void SharpYuvComputeConversionMatrix(
    const SharpYuvColorSpace* yuv_color_space,
    SharpYuvConversionMatrix* matrix);

// Enums for precomputed conversion matrices.
typedef enum {
  // WebP's matrix, similar but not identical to kSharpYuvMatrixRec601Limited
  kSharpYuvMatrixWebp = 0,
  // Kr=0.2990f Kb=0.1140f bit_depth=8 range=kSharpYuvRangeLimited
  kSharpYuvMatrixRec601Limited,
  // Kr=0.2990f Kb=0.1140f bit_depth=8 range=kSharpYuvRangeFull
  kSharpYuvMatrixRec601Full,
  // Kr=0.2126f Kb=0.0722f bit_depth=8 range=kSharpYuvRangeLimited
  kSharpYuvMatrixRec709Limited,
  // Kr=0.2126f Kb=0.0722f bit_depth=8 range=kSharpYuvRangeFull
  kSharpYuvMatrixRec709Full,
  kSharpYuvMatrixNum
} SharpYuvMatrixType;

// Returns a pointer to a matrix for one of the predefined colorspaces.
SHARPYUV_EXTERN const SharpYuvConversionMatrix* SharpYuvGetConversionMatrix(
    SharpYuvMatrixType matrix_type);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBP_SHARPYUV_SHARPYUV_CSP_H_
