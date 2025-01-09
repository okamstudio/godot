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

#include "sharpyuv/sharpyuv_csp.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>

static int ToFixed16(float f) { return (int)floor(f * (1 << 16) + 0.5f); }

void SharpYuvComputeConversionMatrix(const SharpYuvColorSpace* yuv_color_space,
                                     SharpYuvConversionMatrix* matrix) {
  const float kr = yuv_color_space->kr;
  const float kb = yuv_color_space->kb;
  const float kg = 1.0f - kr - kb;
  const float cb = 0.5f / (1.0f - kb);
  const float cr = 0.5f / (1.0f - kr);

  const int shift = yuv_color_space->bit_depth - 8;

  const float denom = (float)((1 << yuv_color_space->bit_depth) - 1);
  float scale_y = 1.0f;
  float add_y = 0.0f;
  float scale_u = cb;
  float scale_v = cr;
  float add_uv = (float)(128 << shift);
  assert(yuv_color_space->bit_depth >= 8);

  if (yuv_color_space->range == kSharpYuvRangeLimited) {
    scale_y *= (219 << shift) / denom;
    scale_u *= (224 << shift) / denom;
    scale_v *= (224 << shift) / denom;
    add_y = (float)(16 << shift);
  }

  matrix->rgb_to_y[0] = ToFixed16(kr * scale_y);
  matrix->rgb_to_y[1] = ToFixed16(kg * scale_y);
  matrix->rgb_to_y[2] = ToFixed16(kb * scale_y);
  matrix->rgb_to_y[3] = ToFixed16(add_y);

  matrix->rgb_to_u[0] = ToFixed16(-kr * scale_u);
  matrix->rgb_to_u[1] = ToFixed16(-kg * scale_u);
  matrix->rgb_to_u[2] = ToFixed16((1 - kb) * scale_u);
  matrix->rgb_to_u[3] = ToFixed16(add_uv);

  matrix->rgb_to_v[0] = ToFixed16((1 - kr) * scale_v);
  matrix->rgb_to_v[1] = ToFixed16(-kg * scale_v);
  matrix->rgb_to_v[2] = ToFixed16(-kb * scale_v);
  matrix->rgb_to_v[3] = ToFixed16(add_uv);
}

// Matrices are in YUV_FIX fixed point precision.
// WebP's matrix, similar but not identical to kRec601LimitedMatrix
// Derived using the following formulas:
// Y = 0.2569 * R + 0.5044 * G + 0.0979 * B + 16
// U = -0.1483 * R - 0.2911 * G + 0.4394 * B + 128
// V = 0.4394 * R - 0.3679 * G - 0.0715 * B + 128
static const SharpYuvConversionMatrix kWebpMatrix = {
  {16839, 33059, 6420, 16 << 16},
  {-9719, -19081, 28800, 128 << 16},
  {28800, -24116, -4684, 128 << 16},
};
// Kr=0.2990f Kb=0.1140f bit_depth=8 range=kSharpYuvRangeLimited
static const SharpYuvConversionMatrix kRec601LimitedMatrix = {
  {16829, 33039, 6416, 16 << 16},
  {-9714, -19071, 28784, 128 << 16},
  {28784, -24103, -4681, 128 << 16},
};
// Kr=0.2990f Kb=0.1140f bit_depth=8 range=kSharpYuvRangeFull
static const SharpYuvConversionMatrix kRec601FullMatrix = {
  {19595, 38470, 7471, 0},
  {-11058, -21710, 32768, 128 << 16},
  {32768, -27439, -5329, 128 << 16},
};
// Kr=0.2126f Kb=0.0722f bit_depth=8 range=kSharpYuvRangeLimited
static const SharpYuvConversionMatrix kRec709LimitedMatrix = {
  {11966, 40254, 4064, 16 << 16},
  {-6596, -22189, 28784, 128 << 16},
  {28784, -26145, -2639, 128 << 16},
};
// Kr=0.2126f Kb=0.0722f bit_depth=8 range=kSharpYuvRangeFull
static const SharpYuvConversionMatrix kRec709FullMatrix = {
  {13933, 46871, 4732, 0},
  {-7509, -25259, 32768, 128 << 16},
  {32768, -29763, -3005, 128 << 16},
};

const SharpYuvConversionMatrix* SharpYuvGetConversionMatrix(
    SharpYuvMatrixType matrix_type) {
  switch (matrix_type) {
    case kSharpYuvMatrixWebp:
      return &kWebpMatrix;
    case kSharpYuvMatrixRec601Limited:
      return &kRec601LimitedMatrix;
    case kSharpYuvMatrixRec601Full:
      return &kRec601FullMatrix;
    case kSharpYuvMatrixRec709Limited:
      return &kRec709LimitedMatrix;
    case kSharpYuvMatrixRec709Full:
      return &kRec709FullMatrix;
    case kSharpYuvMatrixNum:
      return NULL;
  }
  return NULL;
}
