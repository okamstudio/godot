// Copyright 2022 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Sharp RGB to YUV conversion.
//
// Author: Skal (pascal.massimino@gmail.com)

#include "sharpyuv/sharpyuv.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "src/webp/types.h"
#include "sharpyuv/sharpyuv_cpu.h"
#include "sharpyuv/sharpyuv_dsp.h"
#include "sharpyuv/sharpyuv_gamma.h"

//------------------------------------------------------------------------------

int SharpYuvGetVersion(void) {
  return SHARPYUV_VERSION;
}

//------------------------------------------------------------------------------
// Sharp RGB->YUV conversion

static const int kNumIterations = 4;

#define YUV_FIX 16  // fixed-point precision for RGB->YUV
static const int kYuvHalf = 1 << (YUV_FIX - 1);

// Max bit depth so that intermediate calculations fit in 16 bits.
static const int kMaxBitDepth = 14;

// Returns the precision shift to use based on the input rgb_bit_depth.
static int GetPrecisionShift(int rgb_bit_depth) {
  // Try to add 2 bits of precision if it fits in kMaxBitDepth. Otherwise remove
  // bits if needed.
  return ((rgb_bit_depth + 2) <= kMaxBitDepth) ? 2
                                               : (kMaxBitDepth - rgb_bit_depth);
}

typedef int16_t fixed_t;      // signed type with extra precision for UV
typedef uint16_t fixed_y_t;   // unsigned type with extra precision for W

//------------------------------------------------------------------------------

static uint8_t clip_8b(fixed_t v) {
  return (!(v & ~0xff)) ? (uint8_t)v : (v < 0) ? 0u : 255u;
}

static uint16_t clip(fixed_t v, int max) {
  return (v < 0) ? 0 : (v > max) ? max : (uint16_t)v;
}

static fixed_y_t clip_bit_depth(int y, int bit_depth) {
  const int max = (1 << bit_depth) - 1;
  return (!(y & ~max)) ? (fixed_y_t)y : (y < 0) ? 0 : max;
}

//------------------------------------------------------------------------------

static int RGBToGray(int64_t r, int64_t g, int64_t b) {
  const int64_t luma = 13933 * r + 46871 * g + 4732 * b + kYuvHalf;
  return (int)(luma >> YUV_FIX);
}

static uint32_t ScaleDown(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
                          int rgb_bit_depth,
                          SharpYuvTransferFunctionType transfer_type) {
  const int bit_depth = rgb_bit_depth + GetPrecisionShift(rgb_bit_depth);
  const uint32_t A = SharpYuvGammaToLinear(a, bit_depth, transfer_type);
  const uint32_t B = SharpYuvGammaToLinear(b, bit_depth, transfer_type);
  const uint32_t C = SharpYuvGammaToLinear(c, bit_depth, transfer_type);
  const uint32_t D = SharpYuvGammaToLinear(d, bit_depth, transfer_type);
  return SharpYuvLinearToGamma((A + B + C + D + 2) >> 2, bit_depth,
                               transfer_type);
}

static WEBP_INLINE void UpdateW(const fixed_y_t* src, fixed_y_t* dst, int w,
                                int rgb_bit_depth,
                                SharpYuvTransferFunctionType transfer_type) {
  const int bit_depth = rgb_bit_depth + GetPrecisionShift(rgb_bit_depth);
  int i = 0;
  do {
    const uint32_t R =
        SharpYuvGammaToLinear(src[0 * w + i], bit_depth, transfer_type);
    const uint32_t G =
        SharpYuvGammaToLinear(src[1 * w + i], bit_depth, transfer_type);
    const uint32_t B =
        SharpYuvGammaToLinear(src[2 * w + i], bit_depth, transfer_type);
    const uint32_t Y = RGBToGray(R, G, B);
    dst[i] = (fixed_y_t)SharpYuvLinearToGamma(Y, bit_depth, transfer_type);
  } while (++i < w);
}

static void UpdateChroma(const fixed_y_t* src1, const fixed_y_t* src2,
                         fixed_t* dst, int uv_w, int rgb_bit_depth,
                         SharpYuvTransferFunctionType transfer_type) {
  int i = 0;
  do {
    const int r =
        ScaleDown(src1[0 * uv_w + 0], src1[0 * uv_w + 1], src2[0 * uv_w + 0],
                  src2[0 * uv_w + 1], rgb_bit_depth, transfer_type);
    const int g =
        ScaleDown(src1[2 * uv_w + 0], src1[2 * uv_w + 1], src2[2 * uv_w + 0],
                  src2[2 * uv_w + 1], rgb_bit_depth, transfer_type);
    const int b =
        ScaleDown(src1[4 * uv_w + 0], src1[4 * uv_w + 1], src2[4 * uv_w + 0],
                  src2[4 * uv_w + 1], rgb_bit_depth, transfer_type);
    const int W = RGBToGray(r, g, b);
    dst[0 * uv_w] = (fixed_t)(r - W);
    dst[1 * uv_w] = (fixed_t)(g - W);
    dst[2 * uv_w] = (fixed_t)(b - W);
    dst  += 1;
    src1 += 2;
    src2 += 2;
  } while (++i < uv_w);
}

static void StoreGray(const fixed_y_t* rgb, fixed_y_t* y, int w) {
  int i = 0;
  assert(w > 0);
  do {
    y[i] = RGBToGray(rgb[0 * w + i], rgb[1 * w + i], rgb[2 * w + i]);
  } while (++i < w);
}

//------------------------------------------------------------------------------

static WEBP_INLINE fixed_y_t Filter2(int A, int B, int W0, int bit_depth) {
  const int v0 = (A * 3 + B + 2) >> 2;
  return clip_bit_depth(v0 + W0, bit_depth);
}

//------------------------------------------------------------------------------

static WEBP_INLINE int Shift(int v, int shift) {
  return (shift >= 0) ? (v << shift) : (v >> -shift);
}

static void ImportOneRow(const uint8_t* const r_ptr,
                         const uint8_t* const g_ptr,
                         const uint8_t* const b_ptr,
                         int rgb_step,
                         int rgb_bit_depth,
                         int pic_width,
                         fixed_y_t* const dst) {
  // Convert the rgb_step from a number of bytes to a number of uint8_t or
  // uint16_t values depending the bit depth.
  const int step = (rgb_bit_depth > 8) ? rgb_step / 2 : rgb_step;
  int i = 0;
  const int w = (pic_width + 1) & ~1;
  do {
    const int off = i * step;
    const int shift = GetPrecisionShift(rgb_bit_depth);
    if (rgb_bit_depth == 8) {
      dst[i + 0 * w] = Shift(r_ptr[off], shift);
      dst[i + 1 * w] = Shift(g_ptr[off], shift);
      dst[i + 2 * w] = Shift(b_ptr[off], shift);
    } else {
      dst[i + 0 * w] = Shift(((uint16_t*)r_ptr)[off], shift);
      dst[i + 1 * w] = Shift(((uint16_t*)g_ptr)[off], shift);
      dst[i + 2 * w] = Shift(((uint16_t*)b_ptr)[off], shift);
    }
  } while (++i < pic_width);
  if (pic_width & 1) {  // replicate rightmost pixel
    dst[pic_width + 0 * w] = dst[pic_width + 0 * w - 1];
    dst[pic_width + 1 * w] = dst[pic_width + 1 * w - 1];
    dst[pic_width + 2 * w] = dst[pic_width + 2 * w - 1];
  }
}

static void InterpolateTwoRows(const fixed_y_t* const best_y,
                               const fixed_t* prev_uv,
                               const fixed_t* cur_uv,
                               const fixed_t* next_uv,
                               int w,
                               fixed_y_t* out1,
                               fixed_y_t* out2,
                               int rgb_bit_depth) {
  const int uv_w = w >> 1;
  const int len = (w - 1) >> 1;   // length to filter
  int k = 3;
  const int bit_depth = rgb_bit_depth + GetPrecisionShift(rgb_bit_depth);
  while (k-- > 0) {   // process each R/G/B segments in turn
    // special boundary case for i==0
    out1[0] = Filter2(cur_uv[0], prev_uv[0], best_y[0], bit_depth);
    out2[0] = Filter2(cur_uv[0], next_uv[0], best_y[w], bit_depth);

    SharpYuvFilterRow(cur_uv, prev_uv, len, best_y + 0 + 1, out1 + 1,
                      bit_depth);
    SharpYuvFilterRow(cur_uv, next_uv, len, best_y + w + 1, out2 + 1,
                      bit_depth);

    // special boundary case for i == w - 1 when w is even
    if (!(w & 1)) {
      out1[w - 1] = Filter2(cur_uv[uv_w - 1], prev_uv[uv_w - 1],
                            best_y[w - 1 + 0], bit_depth);
      out2[w - 1] = Filter2(cur_uv[uv_w - 1], next_uv[uv_w - 1],
                            best_y[w - 1 + w], bit_depth);
    }
    out1 += w;
    out2 += w;
    prev_uv += uv_w;
    cur_uv  += uv_w;
    next_uv += uv_w;
  }
}

static WEBP_INLINE int RGBToYUVComponent(int r, int g, int b,
                                         const int coeffs[4], int sfix) {
  const int srounder = 1 << (YUV_FIX + sfix - 1);
  const int luma = coeffs[0] * r + coeffs[1] * g + coeffs[2] * b +
                   coeffs[3] + srounder;
  return (luma >> (YUV_FIX + sfix));
}

static int ConvertWRGBToYUV(const fixed_y_t* best_y, const fixed_t* best_uv,
                            uint8_t* y_ptr, int y_stride, uint8_t* u_ptr,
                            int u_stride, uint8_t* v_ptr, int v_stride,
                            int rgb_bit_depth,
                            int yuv_bit_depth, int width, int height,
                            const SharpYuvConversionMatrix* yuv_matrix) {
  int i, j;
  const fixed_t* const best_uv_base = best_uv;
  const int w = (width + 1) & ~1;
  const int h = (height + 1) & ~1;
  const int uv_w = w >> 1;
  const int uv_h = h >> 1;
  const int sfix = GetPrecisionShift(rgb_bit_depth);
  const int yuv_max = (1 << yuv_bit_depth) - 1;

  best_uv = best_uv_base;
  j = 0;
  do {
    i = 0;
    do {
      const int off = (i >> 1);
      const int W = best_y[i];
      const int r = best_uv[off + 0 * uv_w] + W;
      const int g = best_uv[off + 1 * uv_w] + W;
      const int b = best_uv[off + 2 * uv_w] + W;
      const int y = RGBToYUVComponent(r, g, b, yuv_matrix->rgb_to_y, sfix);
      if (yuv_bit_depth <= 8) {
        y_ptr[i] = clip_8b(y);
      } else {
        ((uint16_t*)y_ptr)[i] = clip(y, yuv_max);
      }
    } while (++i < width);
    best_y += w;
    best_uv += (j & 1) * 3 * uv_w;
    y_ptr += y_stride;
  } while (++j < height);

  best_uv = best_uv_base;
  j = 0;
  do {
    i = 0;
    do {
      // Note r, g and b values here are off by W, but a constant offset on all
      // 3 components doesn't change the value of u and v with a YCbCr matrix.
      const int r = best_uv[i + 0 * uv_w];
      const int g = best_uv[i + 1 * uv_w];
      const int b = best_uv[i + 2 * uv_w];
      const int u = RGBToYUVComponent(r, g, b, yuv_matrix->rgb_to_u, sfix);
      const int v = RGBToYUVComponent(r, g, b, yuv_matrix->rgb_to_v, sfix);
      if (yuv_bit_depth <= 8) {
        u_ptr[i] = clip_8b(u);
        v_ptr[i] = clip_8b(v);
      } else {
        ((uint16_t*)u_ptr)[i] = clip(u, yuv_max);
        ((uint16_t*)v_ptr)[i] = clip(v, yuv_max);
      }
    } while (++i < uv_w);
    best_uv += 3 * uv_w;
    u_ptr += u_stride;
    v_ptr += v_stride;
  } while (++j < uv_h);
  return 1;
}

//------------------------------------------------------------------------------
// Main function

static void* SafeMalloc(uint64_t nmemb, size_t size) {
  const uint64_t total_size = nmemb * (uint64_t)size;
  if (total_size != (size_t)total_size) return NULL;
  return malloc((size_t)total_size);
}

#define SAFE_ALLOC(W, H, T) ((T*)SafeMalloc((uint64_t)(W) * (H), sizeof(T)))

static int DoSharpArgbToYuv(const uint8_t* r_ptr, const uint8_t* g_ptr,
                            const uint8_t* b_ptr, int rgb_step, int rgb_stride,
                            int rgb_bit_depth, uint8_t* y_ptr, int y_stride,
                            uint8_t* u_ptr, int u_stride, uint8_t* v_ptr,
                            int v_stride, int yuv_bit_depth, int width,
                            int height,
                            const SharpYuvConversionMatrix* yuv_matrix,
                            SharpYuvTransferFunctionType transfer_type) {
  // we expand the right/bottom border if needed
  const int w = (width + 1) & ~1;
  const int h = (height + 1) & ~1;
  const int uv_w = w >> 1;
  const int uv_h = h >> 1;
  const int y_bit_depth = rgb_bit_depth + GetPrecisionShift(rgb_bit_depth);
  uint64_t prev_diff_y_sum = ~0;
  int j, iter;

  // TODO(skal): allocate one big memory chunk. But for now, it's easier
  // for valgrind debugging to have several chunks.
  fixed_y_t* const tmp_buffer = SAFE_ALLOC(w * 3, 2, fixed_y_t);   // scratch
  fixed_y_t* const best_y_base = SAFE_ALLOC(w, h, fixed_y_t);
  fixed_y_t* const target_y_base = SAFE_ALLOC(w, h, fixed_y_t);
  fixed_y_t* const best_rgb_y = SAFE_ALLOC(w, 2, fixed_y_t);
  fixed_t* const best_uv_base = SAFE_ALLOC(uv_w * 3, uv_h, fixed_t);
  fixed_t* const target_uv_base = SAFE_ALLOC(uv_w * 3, uv_h, fixed_t);
  fixed_t* const best_rgb_uv = SAFE_ALLOC(uv_w * 3, 1, fixed_t);
  fixed_y_t* best_y = best_y_base;
  fixed_y_t* target_y = target_y_base;
  fixed_t* best_uv = best_uv_base;
  fixed_t* target_uv = target_uv_base;
  const uint64_t diff_y_threshold = (uint64_t)(3.0 * w * h);
  int ok;
  assert(w > 0);
  assert(h > 0);

  if (best_y_base == NULL || best_uv_base == NULL ||
      target_y_base == NULL || target_uv_base == NULL ||
      best_rgb_y == NULL || best_rgb_uv == NULL ||
      tmp_buffer == NULL) {
    ok = 0;
    goto End;
  }

  // Import RGB samples to W/RGB representation.
  for (j = 0; j < height; j += 2) {
    const int is_last_row = (j == height - 1);
    fixed_y_t* const src1 = tmp_buffer + 0 * w;
    fixed_y_t* const src2 = tmp_buffer + 3 * w;

    // prepare two rows of input
    ImportOneRow(r_ptr, g_ptr, b_ptr, rgb_step, rgb_bit_depth, width,
                 src1);
    if (!is_last_row) {
      ImportOneRow(r_ptr + rgb_stride, g_ptr + rgb_stride, b_ptr + rgb_stride,
                   rgb_step, rgb_bit_depth, width, src2);
    } else {
      memcpy(src2, src1, 3 * w * sizeof(*src2));
    }
    StoreGray(src1, best_y + 0, w);
    StoreGray(src2, best_y + w, w);

    UpdateW(src1, target_y, w, rgb_bit_depth, transfer_type);
    UpdateW(src2, target_y + w, w, rgb_bit_depth, transfer_type);
    UpdateChroma(src1, src2, target_uv, uv_w, rgb_bit_depth, transfer_type);
    memcpy(best_uv, target_uv, 3 * uv_w * sizeof(*best_uv));
    best_y += 2 * w;
    best_uv += 3 * uv_w;
    target_y += 2 * w;
    target_uv += 3 * uv_w;
    r_ptr += 2 * rgb_stride;
    g_ptr += 2 * rgb_stride;
    b_ptr += 2 * rgb_stride;
  }

  // Iterate and resolve clipping conflicts.
  for (iter = 0; iter < kNumIterations; ++iter) {
    const fixed_t* cur_uv = best_uv_base;
    const fixed_t* prev_uv = best_uv_base;
    uint64_t diff_y_sum = 0;

    best_y = best_y_base;
    best_uv = best_uv_base;
    target_y = target_y_base;
    target_uv = target_uv_base;
    j = 0;
    do {
      fixed_y_t* const src1 = tmp_buffer + 0 * w;
      fixed_y_t* const src2 = tmp_buffer + 3 * w;
      {
        const fixed_t* const next_uv = cur_uv + ((j < h - 2) ? 3 * uv_w : 0);
        InterpolateTwoRows(best_y, prev_uv, cur_uv, next_uv, w,
                           src1, src2, rgb_bit_depth);
        prev_uv = cur_uv;
        cur_uv = next_uv;
      }

      UpdateW(src1, best_rgb_y + 0 * w, w, rgb_bit_depth, transfer_type);
      UpdateW(src2, best_rgb_y + 1 * w, w, rgb_bit_depth, transfer_type);
      UpdateChroma(src1, src2, best_rgb_uv, uv_w, rgb_bit_depth, transfer_type);

      // update two rows of Y and one row of RGB
      diff_y_sum +=
          SharpYuvUpdateY(target_y, best_rgb_y, best_y, 2 * w, y_bit_depth);
      SharpYuvUpdateRGB(target_uv, best_rgb_uv, best_uv, 3 * uv_w);

      best_y += 2 * w;
      best_uv += 3 * uv_w;
      target_y += 2 * w;
      target_uv += 3 * uv_w;
      j += 2;
    } while (j < h);
    // test exit condition
    if (iter > 0) {
      if (diff_y_sum < diff_y_threshold) break;
      if (diff_y_sum > prev_diff_y_sum) break;
    }
    prev_diff_y_sum = diff_y_sum;
  }

  // final reconstruction
  ok = ConvertWRGBToYUV(best_y_base, best_uv_base, y_ptr, y_stride, u_ptr,
                        u_stride, v_ptr, v_stride, rgb_bit_depth, yuv_bit_depth,
                        width, height, yuv_matrix);

 End:
  free(best_y_base);
  free(best_uv_base);
  free(target_y_base);
  free(target_uv_base);
  free(best_rgb_y);
  free(best_rgb_uv);
  free(tmp_buffer);
  return ok;
}

#undef SAFE_ALLOC

#if defined(WEBP_USE_THREAD) && !defined(_WIN32)
#include <pthread.h>  // NOLINT

#define LOCK_ACCESS \
    static pthread_mutex_t sharpyuv_lock = PTHREAD_MUTEX_INITIALIZER; \
    if (pthread_mutex_lock(&sharpyuv_lock)) return
#define UNLOCK_ACCESS_AND_RETURN                  \
    do {                                          \
      (void)pthread_mutex_unlock(&sharpyuv_lock); \
      return;                                     \
    } while (0)
#else  // !(defined(WEBP_USE_THREAD) && !defined(_WIN32))
#define LOCK_ACCESS do {} while (0)
#define UNLOCK_ACCESS_AND_RETURN return
#endif  // defined(WEBP_USE_THREAD) && !defined(_WIN32)

// Hidden exported init function.
// By default SharpYuvConvert calls it with SharpYuvGetCPUInfo. If needed,
// users can declare it as extern and call it with an alternate VP8CPUInfo
// function.
extern VP8CPUInfo SharpYuvGetCPUInfo;
SHARPYUV_EXTERN void SharpYuvInit(VP8CPUInfo cpu_info_func);
void SharpYuvInit(VP8CPUInfo cpu_info_func) {
  static volatile VP8CPUInfo sharpyuv_last_cpuinfo_used =
      (VP8CPUInfo)&sharpyuv_last_cpuinfo_used;
  LOCK_ACCESS;
  // Only update SharpYuvGetCPUInfo when called from external code to avoid a
  // race on reading the value in SharpYuvConvert().
  if (cpu_info_func != (VP8CPUInfo)&SharpYuvGetCPUInfo) {
    SharpYuvGetCPUInfo = cpu_info_func;
  }
  if (sharpyuv_last_cpuinfo_used == SharpYuvGetCPUInfo) {
    UNLOCK_ACCESS_AND_RETURN;
  }

  SharpYuvInitDsp();
  SharpYuvInitGammaTables();

  sharpyuv_last_cpuinfo_used = SharpYuvGetCPUInfo;
  UNLOCK_ACCESS_AND_RETURN;
}

int SharpYuvConvert(const void* r_ptr, const void* g_ptr, const void* b_ptr,
                    int rgb_step, int rgb_stride, int rgb_bit_depth,
                    void* y_ptr, int y_stride, void* u_ptr, int u_stride,
                    void* v_ptr, int v_stride, int yuv_bit_depth, int width,
                    int height, const SharpYuvConversionMatrix* yuv_matrix) {
  SharpYuvOptions options;
  options.yuv_matrix = yuv_matrix;
  options.transfer_type = kSharpYuvTransferFunctionSrgb;
  return SharpYuvConvertWithOptions(
      r_ptr, g_ptr, b_ptr, rgb_step, rgb_stride, rgb_bit_depth, y_ptr, y_stride,
      u_ptr, u_stride, v_ptr, v_stride, yuv_bit_depth, width, height, &options);
}

int SharpYuvOptionsInitInternal(const SharpYuvConversionMatrix* yuv_matrix,
                                SharpYuvOptions* options, int version) {
  const int major = (version >> 24);
  const int minor = (version >> 16) & 0xff;
  if (options == NULL || yuv_matrix == NULL ||
      (major == SHARPYUV_VERSION_MAJOR && major == 0 &&
       minor != SHARPYUV_VERSION_MINOR) ||
      (major != SHARPYUV_VERSION_MAJOR)) {
    return 0;
  }
  options->yuv_matrix = yuv_matrix;
  options->transfer_type = kSharpYuvTransferFunctionSrgb;
  return 1;
}

int SharpYuvConvertWithOptions(const void* r_ptr, const void* g_ptr,
                               const void* b_ptr, int rgb_step, int rgb_stride,
                               int rgb_bit_depth, void* y_ptr, int y_stride,
                               void* u_ptr, int u_stride, void* v_ptr,
                               int v_stride, int yuv_bit_depth, int width,
                               int height, const SharpYuvOptions* options) {
  const SharpYuvConversionMatrix* yuv_matrix = options->yuv_matrix;
  SharpYuvTransferFunctionType transfer_type = options->transfer_type;
  SharpYuvConversionMatrix scaled_matrix;
  const int rgb_max = (1 << rgb_bit_depth) - 1;
  const int rgb_round = 1 << (rgb_bit_depth - 1);
  const int yuv_max = (1 << yuv_bit_depth) - 1;
  const int sfix = GetPrecisionShift(rgb_bit_depth);

  if (width < 1 || height < 1 || width == INT_MAX || height == INT_MAX ||
      r_ptr == NULL || g_ptr == NULL || b_ptr == NULL || y_ptr == NULL ||
      u_ptr == NULL || v_ptr == NULL) {
    return 0;
  }
  if (rgb_bit_depth != 8 && rgb_bit_depth != 10 && rgb_bit_depth != 12 &&
      rgb_bit_depth != 16) {
    return 0;
  }
  if (yuv_bit_depth != 8 && yuv_bit_depth != 10 && yuv_bit_depth != 12) {
    return 0;
  }
  if (rgb_bit_depth > 8 && (rgb_step % 2 != 0 || rgb_stride % 2 != 0)) {
    // Step/stride should be even for uint16_t buffers.
    return 0;
  }
  if (yuv_bit_depth > 8 &&
      (y_stride % 2 != 0 || u_stride % 2 != 0 || v_stride % 2 != 0)) {
    // Stride should be even for uint16_t buffers.
    return 0;
  }
  // The address of the function pointer is used to avoid a read race.
  SharpYuvInit((VP8CPUInfo)&SharpYuvGetCPUInfo);

  // Add scaling factor to go from rgb_bit_depth to yuv_bit_depth, to the
  // rgb->yuv conversion matrix.
  if (rgb_bit_depth == yuv_bit_depth) {
    memcpy(&scaled_matrix, yuv_matrix, sizeof(scaled_matrix));
  } else {
    int i;
    for (i = 0; i < 3; ++i) {
      scaled_matrix.rgb_to_y[i] =
          (yuv_matrix->rgb_to_y[i] * yuv_max + rgb_round) / rgb_max;
      scaled_matrix.rgb_to_u[i] =
          (yuv_matrix->rgb_to_u[i] * yuv_max + rgb_round) / rgb_max;
      scaled_matrix.rgb_to_v[i] =
          (yuv_matrix->rgb_to_v[i] * yuv_max + rgb_round) / rgb_max;
    }
  }
  // Also incorporate precision change scaling.
  scaled_matrix.rgb_to_y[3] = Shift(yuv_matrix->rgb_to_y[3], sfix);
  scaled_matrix.rgb_to_u[3] = Shift(yuv_matrix->rgb_to_u[3], sfix);
  scaled_matrix.rgb_to_v[3] = Shift(yuv_matrix->rgb_to_v[3], sfix);

  return DoSharpArgbToYuv(
      (const uint8_t*)r_ptr, (const uint8_t*)g_ptr, (const uint8_t*)b_ptr,
      rgb_step, rgb_stride, rgb_bit_depth, (uint8_t*)y_ptr, y_stride,
      (uint8_t*)u_ptr, u_stride, (uint8_t*)v_ptr, v_stride, yuv_bit_depth,
      width, height, &scaled_matrix, transfer_type);
}

//------------------------------------------------------------------------------
