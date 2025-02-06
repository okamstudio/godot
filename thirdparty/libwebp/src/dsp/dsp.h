// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//   Speed-critical functions.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_DSP_DSP_H_
#define WEBP_DSP_DSP_H_

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#include "src/dsp/cpu.h"
#include "src/webp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BPS 32   // this is the common stride for enc/dec

//------------------------------------------------------------------------------
// WEBP_RESTRICT

// Declares a pointer with the restrict type qualifier if available.
// This allows code to hint to the compiler that only this pointer references a
// particular object or memory region within the scope of the block in which it
// is declared. This may allow for improved optimizations due to the lack of
// pointer aliasing. See also:
// https://en.cppreference.com/w/c/language/restrict
#if defined(__GNUC__)
#define WEBP_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define WEBP_RESTRICT __restrict
#else
#define WEBP_RESTRICT
#endif


//------------------------------------------------------------------------------
// Init stub generator

// Defines an init function stub to ensure each module exposes a symbol,
// avoiding a compiler warning.
#define WEBP_DSP_INIT_STUB(func) \
  extern void func(void); \
  void func(void) {}

//------------------------------------------------------------------------------
// Encoding

// Transforms
// VP8Idct: Does one of two inverse transforms. If do_two is set, the transforms
//          will be done for (ref, in, dst) and (ref + 4, in + 16, dst + 4).
typedef void (*VP8Idct)(const uint8_t* WEBP_RESTRICT ref,
                        const int16_t* WEBP_RESTRICT in,
                        uint8_t* WEBP_RESTRICT dst, int do_two);
typedef void (*VP8Fdct)(const uint8_t* WEBP_RESTRICT src,
                        const uint8_t* WEBP_RESTRICT ref,
                        int16_t* WEBP_RESTRICT out);
typedef void (*VP8WHT)(const int16_t* WEBP_RESTRICT in,
                       int16_t* WEBP_RESTRICT out);
extern VP8Idct VP8ITransform;
extern VP8Fdct VP8FTransform;
extern VP8Fdct VP8FTransform2;   // performs two transforms at a time
extern VP8WHT VP8FTransformWHT;
// Predictions
// *dst is the destination block. *top and *left can be NULL.
typedef void (*VP8IntraPreds)(uint8_t* WEBP_RESTRICT dst,
                              const uint8_t* WEBP_RESTRICT left,
                              const uint8_t* WEBP_RESTRICT top);
typedef void (*VP8Intra4Preds)(uint8_t* WEBP_RESTRICT dst,
                               const uint8_t* WEBP_RESTRICT top);
extern VP8Intra4Preds VP8EncPredLuma4;
extern VP8IntraPreds VP8EncPredLuma16;
extern VP8IntraPreds VP8EncPredChroma8;

typedef int (*VP8Metric)(const uint8_t* WEBP_RESTRICT pix,
                         const uint8_t* WEBP_RESTRICT ref);
extern VP8Metric VP8SSE16x16, VP8SSE16x8, VP8SSE8x8, VP8SSE4x4;
typedef int (*VP8WMetric)(const uint8_t* WEBP_RESTRICT pix,
                          const uint8_t* WEBP_RESTRICT ref,
                          const uint16_t* WEBP_RESTRICT const weights);
// The weights for VP8TDisto4x4 and VP8TDisto16x16 contain a row-major
// 4 by 4 symmetric matrix.
extern VP8WMetric VP8TDisto4x4, VP8TDisto16x16;

// Compute the average (DC) of four 4x4 blocks.
// Each sub-4x4 block #i sum is stored in dc[i].
typedef void (*VP8MeanMetric)(const uint8_t* WEBP_RESTRICT ref,
                              uint32_t dc[4]);
extern VP8MeanMetric VP8Mean16x4;

typedef void (*VP8BlockCopy)(const uint8_t* WEBP_RESTRICT src,
                             uint8_t* WEBP_RESTRICT dst);
extern VP8BlockCopy VP8Copy4x4;
extern VP8BlockCopy VP8Copy16x8;
// Quantization
struct VP8Matrix;   // forward declaration
typedef int (*VP8QuantizeBlock)(
    int16_t in[16], int16_t out[16],
    const struct VP8Matrix* WEBP_RESTRICT const mtx);
// Same as VP8QuantizeBlock, but quantizes two consecutive blocks.
typedef int (*VP8Quantize2Blocks)(
    int16_t in[32], int16_t out[32],
    const struct VP8Matrix* WEBP_RESTRICT const mtx);

extern VP8QuantizeBlock VP8EncQuantizeBlock;
extern VP8Quantize2Blocks VP8EncQuantize2Blocks;

// specific to 2nd transform:
typedef int (*VP8QuantizeBlockWHT)(
    int16_t in[16], int16_t out[16],
    const struct VP8Matrix* WEBP_RESTRICT const mtx);
extern VP8QuantizeBlockWHT VP8EncQuantizeBlockWHT;

extern const int VP8DspScan[16 + 4 + 4];

// Collect histogram for susceptibility calculation.
#define MAX_COEFF_THRESH   31   // size of histogram used by CollectHistogram.
typedef struct {
  // We only need to store max_value and last_non_zero, not the distribution.
  int max_value;
  int last_non_zero;
} VP8Histogram;
typedef void (*VP8CHisto)(const uint8_t* WEBP_RESTRICT ref,
                          const uint8_t* WEBP_RESTRICT pred,
                          int start_block, int end_block,
                          VP8Histogram* WEBP_RESTRICT const histo);
extern VP8CHisto VP8CollectHistogram;
// General-purpose util function to help VP8CollectHistogram().
void VP8SetHistogramData(const int distribution[MAX_COEFF_THRESH + 1],
                         VP8Histogram* const histo);

// must be called before using any of the above
void VP8EncDspInit(void);

//------------------------------------------------------------------------------
// cost functions (encoding)

extern const uint16_t VP8EntropyCost[256];        // 8bit fixed-point log(p)
// approximate cost per level:
extern const uint16_t VP8LevelFixedCosts[2047 /*MAX_LEVEL*/ + 1];
extern const uint8_t VP8EncBands[16 + 1];

struct VP8Residual;
typedef void (*VP8SetResidualCoeffsFunc)(
    const int16_t* WEBP_RESTRICT const coeffs,
    struct VP8Residual* WEBP_RESTRICT const res);
extern VP8SetResidualCoeffsFunc VP8SetResidualCoeffs;

// Cost calculation function.
typedef int (*VP8GetResidualCostFunc)(int ctx0,
                                      const struct VP8Residual* const res);
extern VP8GetResidualCostFunc VP8GetResidualCost;

// must be called before anything using the above
void VP8EncDspCostInit(void);

//------------------------------------------------------------------------------
// SSIM / PSNR utils

// struct for accumulating statistical moments
typedef struct {
  uint32_t w;              // sum(w_i) : sum of weights
  uint32_t xm, ym;         // sum(w_i * x_i), sum(w_i * y_i)
  uint32_t xxm, xym, yym;  // sum(w_i * x_i * x_i), etc.
} VP8DistoStats;

// Compute the final SSIM value
// The non-clipped version assumes stats->w = (2 * VP8_SSIM_KERNEL + 1)^2.
double VP8SSIMFromStats(const VP8DistoStats* const stats);
double VP8SSIMFromStatsClipped(const VP8DistoStats* const stats);

#define VP8_SSIM_KERNEL 3   // total size of the kernel: 2 * VP8_SSIM_KERNEL + 1
typedef double (*VP8SSIMGetClippedFunc)(const uint8_t* src1, int stride1,
                                        const uint8_t* src2, int stride2,
                                        int xo, int yo,  // center position
                                        int W, int H);   // plane dimension

#if !defined(WEBP_REDUCE_SIZE)
// This version is called with the guarantee that you can load 8 bytes and
// 8 rows at offset src1 and src2
typedef double (*VP8SSIMGetFunc)(const uint8_t* src1, int stride1,
                                 const uint8_t* src2, int stride2);

extern VP8SSIMGetFunc VP8SSIMGet;         // unclipped / unchecked
extern VP8SSIMGetClippedFunc VP8SSIMGetClipped;   // with clipping
#endif

#if !defined(WEBP_DISABLE_STATS)
typedef uint32_t (*VP8AccumulateSSEFunc)(const uint8_t* src1,
                                         const uint8_t* src2, int len);
extern VP8AccumulateSSEFunc VP8AccumulateSSE;
#endif

// must be called before using any of the above directly
void VP8SSIMDspInit(void);

//------------------------------------------------------------------------------
// Decoding

typedef void (*VP8DecIdct)(const int16_t* WEBP_RESTRICT coeffs,
                           uint8_t* WEBP_RESTRICT dst);
// when doing two transforms, coeffs is actually int16_t[2][16].
typedef void (*VP8DecIdct2)(const int16_t* WEBP_RESTRICT coeffs,
                            uint8_t* WEBP_RESTRICT dst, int do_two);
extern VP8DecIdct2 VP8Transform;
extern VP8DecIdct VP8TransformAC3;
extern VP8DecIdct VP8TransformUV;
extern VP8DecIdct VP8TransformDC;
extern VP8DecIdct VP8TransformDCUV;
extern VP8WHT VP8TransformWHT;

#define WEBP_TRANSFORM_AC3_C1 20091
#define WEBP_TRANSFORM_AC3_C2 35468
#define WEBP_TRANSFORM_AC3_MUL1(a) ((((a) * WEBP_TRANSFORM_AC3_C1) >> 16) + (a))
#define WEBP_TRANSFORM_AC3_MUL2(a) (((a) * WEBP_TRANSFORM_AC3_C2) >> 16)

// *dst is the destination block, with stride BPS. Boundary samples are
// assumed accessible when needed.
typedef void (*VP8PredFunc)(uint8_t* dst);
extern VP8PredFunc VP8PredLuma16[/* NUM_B_DC_MODES */];
extern VP8PredFunc VP8PredChroma8[/* NUM_B_DC_MODES */];
extern VP8PredFunc VP8PredLuma4[/* NUM_BMODES */];

// clipping tables (for filtering)
extern const int8_t* const VP8ksclip1;  // clips [-1020, 1020] to [-128, 127]
extern const int8_t* const VP8ksclip2;  // clips [-112, 112] to [-16, 15]
extern const uint8_t* const VP8kclip1;  // clips [-255,511] to [0,255]
extern const uint8_t* const VP8kabs0;   // abs(x) for x in [-255,255]
// must be called first
void VP8InitClipTables(void);

// simple filter (only for luma)
typedef void (*VP8SimpleFilterFunc)(uint8_t* p, int stride, int thresh);
extern VP8SimpleFilterFunc VP8SimpleVFilter16;
extern VP8SimpleFilterFunc VP8SimpleHFilter16;
extern VP8SimpleFilterFunc VP8SimpleVFilter16i;  // filter 3 inner edges
extern VP8SimpleFilterFunc VP8SimpleHFilter16i;

// regular filter (on both macroblock edges and inner edges)
typedef void (*VP8LumaFilterFunc)(uint8_t* luma, int stride,
                                  int thresh, int ithresh, int hev_t);
typedef void (*VP8ChromaFilterFunc)(uint8_t* WEBP_RESTRICT u,
                                    uint8_t* WEBP_RESTRICT v, int stride,
                                    int thresh, int ithresh, int hev_t);
// on outer edge
extern VP8LumaFilterFunc VP8VFilter16;
extern VP8LumaFilterFunc VP8HFilter16;
extern VP8ChromaFilterFunc VP8VFilter8;
extern VP8ChromaFilterFunc VP8HFilter8;

// on inner edge
extern VP8LumaFilterFunc VP8VFilter16i;   // filtering 3 inner edges altogether
extern VP8LumaFilterFunc VP8HFilter16i;
extern VP8ChromaFilterFunc VP8VFilter8i;  // filtering u and v altogether
extern VP8ChromaFilterFunc VP8HFilter8i;

// Dithering. Combines dithering values (centered around 128) with dst[],
// according to: dst[] = clip(dst[] + (((dither[]-128) + 8) >> 4)
#define VP8_DITHER_DESCALE 4
#define VP8_DITHER_DESCALE_ROUNDER (1 << (VP8_DITHER_DESCALE - 1))
#define VP8_DITHER_AMP_BITS 7
#define VP8_DITHER_AMP_CENTER (1 << VP8_DITHER_AMP_BITS)
extern void (*VP8DitherCombine8x8)(const uint8_t* WEBP_RESTRICT dither,
                                   uint8_t* WEBP_RESTRICT dst, int dst_stride);

// must be called before anything using the above
void VP8DspInit(void);

//------------------------------------------------------------------------------
// WebP I/O

#define FANCY_UPSAMPLING   // undefined to remove fancy upsampling support

// Convert a pair of y/u/v lines together to the output rgb/a colorspace.
// bottom_y can be NULL if only one line of output is needed (at top/bottom).
typedef void (*WebPUpsampleLinePairFunc)(
    const uint8_t* WEBP_RESTRICT top_y, const uint8_t* WEBP_RESTRICT bottom_y,
    const uint8_t* WEBP_RESTRICT top_u, const uint8_t* WEBP_RESTRICT top_v,
    const uint8_t* WEBP_RESTRICT cur_u, const uint8_t* WEBP_RESTRICT cur_v,
    uint8_t* WEBP_RESTRICT top_dst, uint8_t* WEBP_RESTRICT bottom_dst, int len);

#ifdef FANCY_UPSAMPLING

// Fancy upsampling functions to convert YUV to RGB(A) modes
extern WebPUpsampleLinePairFunc WebPUpsamplers[/* MODE_LAST */];

#endif    // FANCY_UPSAMPLING

// Per-row point-sampling methods.
typedef void (*WebPSamplerRowFunc)(const uint8_t* WEBP_RESTRICT y,
                                   const uint8_t* WEBP_RESTRICT u,
                                   const uint8_t* WEBP_RESTRICT v,
                                   uint8_t* WEBP_RESTRICT dst, int len);
// Generic function to apply 'WebPSamplerRowFunc' to the whole plane:
void WebPSamplerProcessPlane(const uint8_t* WEBP_RESTRICT y, int y_stride,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v, int uv_stride,
                             uint8_t* WEBP_RESTRICT dst, int dst_stride,
                             int width, int height, WebPSamplerRowFunc func);

// Sampling functions to convert rows of YUV to RGB(A)
extern WebPSamplerRowFunc WebPSamplers[/* MODE_LAST */];

// General function for converting two lines of ARGB or RGBA.
// 'alpha_is_last' should be true if 0xff000000 is stored in memory as
// as 0x00, 0x00, 0x00, 0xff (little endian).
WebPUpsampleLinePairFunc WebPGetLinePairConverter(int alpha_is_last);

// YUV444->RGB converters
typedef void (*WebPYUV444Converter)(const uint8_t* WEBP_RESTRICT y,
                                    const uint8_t* WEBP_RESTRICT u,
                                    const uint8_t* WEBP_RESTRICT v,
                                    uint8_t* WEBP_RESTRICT dst, int len);

extern WebPYUV444Converter WebPYUV444Converters[/* MODE_LAST */];

// Must be called before using the WebPUpsamplers[] (and for premultiplied
// colorspaces like rgbA, rgbA4444, etc)
void WebPInitUpsamplers(void);
// Must be called before using WebPSamplers[]
void WebPInitSamplers(void);
// Must be called before using WebPYUV444Converters[]
void WebPInitYUV444Converters(void);

//------------------------------------------------------------------------------
// ARGB -> YUV converters

// Convert ARGB samples to luma Y.
extern void (*WebPConvertARGBToY)(const uint32_t* WEBP_RESTRICT argb,
                                  uint8_t* WEBP_RESTRICT y, int width);
// Convert ARGB samples to U/V with downsampling. do_store should be '1' for
// even lines and '0' for odd ones. 'src_width' is the original width, not
// the U/V one.
extern void (*WebPConvertARGBToUV)(const uint32_t* WEBP_RESTRICT argb,
                                   uint8_t* WEBP_RESTRICT u,
                                   uint8_t* WEBP_RESTRICT v,
                                   int src_width, int do_store);

// Convert a row of accumulated (four-values) of rgba32 toward U/V
extern void (*WebPConvertRGBA32ToUV)(const uint16_t* WEBP_RESTRICT rgb,
                                     uint8_t* WEBP_RESTRICT u,
                                     uint8_t* WEBP_RESTRICT v, int width);

// Convert RGB or BGR to Y
extern void (*WebPConvertRGB24ToY)(const uint8_t* WEBP_RESTRICT rgb,
                                   uint8_t* WEBP_RESTRICT y, int width);
extern void (*WebPConvertBGR24ToY)(const uint8_t* WEBP_RESTRICT bgr,
                                   uint8_t* WEBP_RESTRICT y, int width);

// used for plain-C fallback.
extern void WebPConvertARGBToUV_C(const uint32_t* WEBP_RESTRICT argb,
                                  uint8_t* WEBP_RESTRICT u,
                                  uint8_t* WEBP_RESTRICT v,
                                  int src_width, int do_store);
extern void WebPConvertRGBA32ToUV_C(const uint16_t* WEBP_RESTRICT rgb,
                                    uint8_t* WEBP_RESTRICT u,
                                    uint8_t* WEBP_RESTRICT v, int width);

// Must be called before using the above.
void WebPInitConvertARGBToYUV(void);

//------------------------------------------------------------------------------
// Rescaler

struct WebPRescaler;

// Import a row of data and save its contribution in the rescaler.
// 'channel' denotes the channel number to be imported. 'Expand' corresponds to
// the wrk->x_expand case. Otherwise, 'Shrink' is to be used.
typedef void (*WebPRescalerImportRowFunc)(
    struct WebPRescaler* WEBP_RESTRICT const wrk,
    const uint8_t* WEBP_RESTRICT src);

extern WebPRescalerImportRowFunc WebPRescalerImportRowExpand;
extern WebPRescalerImportRowFunc WebPRescalerImportRowShrink;

// Export one row (starting at x_out position) from rescaler.
// 'Expand' corresponds to the wrk->y_expand case.
// Otherwise 'Shrink' is to be used
typedef void (*WebPRescalerExportRowFunc)(struct WebPRescaler* const wrk);
extern WebPRescalerExportRowFunc WebPRescalerExportRowExpand;
extern WebPRescalerExportRowFunc WebPRescalerExportRowShrink;

// Plain-C implementation, as fall-back.
extern void WebPRescalerImportRowExpand_C(
    struct WebPRescaler* WEBP_RESTRICT const wrk,
    const uint8_t* WEBP_RESTRICT src);
extern void WebPRescalerImportRowShrink_C(
    struct WebPRescaler* WEBP_RESTRICT const wrk,
    const uint8_t* WEBP_RESTRICT src);
extern void WebPRescalerExportRowExpand_C(struct WebPRescaler* const wrk);
extern void WebPRescalerExportRowShrink_C(struct WebPRescaler* const wrk);

// Main entry calls:
extern void WebPRescalerImportRow(
    struct WebPRescaler* WEBP_RESTRICT const wrk,
    const uint8_t* WEBP_RESTRICT src);
// Export one row (starting at x_out position) from rescaler.
extern void WebPRescalerExportRow(struct WebPRescaler* const wrk);

// Must be called first before using the above.
void WebPRescalerDspInit(void);

//------------------------------------------------------------------------------
// Utilities for processing transparent channel.

// Apply alpha pre-multiply on an rgba, bgra or argb plane of size w * h.
// alpha_first should be 0 for argb, 1 for rgba or bgra (where alpha is last).
extern void (*WebPApplyAlphaMultiply)(
    uint8_t* rgba, int alpha_first, int w, int h, int stride);

// Same, buf specifically for RGBA4444 format
extern void (*WebPApplyAlphaMultiply4444)(
    uint8_t* rgba4444, int w, int h, int stride);

// Dispatch the values from alpha[] plane to the ARGB destination 'dst'.
// Returns true if alpha[] plane has non-trivial values different from 0xff.
extern int (*WebPDispatchAlpha)(const uint8_t* WEBP_RESTRICT alpha,
                                int alpha_stride, int width, int height,
                                uint8_t* WEBP_RESTRICT dst, int dst_stride);

// Transfer packed 8b alpha[] values to green channel in dst[], zero'ing the
// A/R/B values. 'dst_stride' is the stride for dst[] in uint32_t units.
extern void (*WebPDispatchAlphaToGreen)(const uint8_t* WEBP_RESTRICT alpha,
                                        int alpha_stride, int width, int height,
                                        uint32_t* WEBP_RESTRICT dst,
                                        int dst_stride);

// Extract the alpha values from 32b values in argb[] and pack them into alpha[]
// (this is the opposite of WebPDispatchAlpha).
// Returns true if there's only trivial 0xff alpha values.
extern int (*WebPExtractAlpha)(const uint8_t* WEBP_RESTRICT argb,
                               int argb_stride, int width, int height,
                               uint8_t* WEBP_RESTRICT alpha,
                               int alpha_stride);

// Extract the green values from 32b values in argb[] and pack them into alpha[]
// (this is the opposite of WebPDispatchAlphaToGreen).
extern void (*WebPExtractGreen)(const uint32_t* WEBP_RESTRICT argb,
                                uint8_t* WEBP_RESTRICT alpha, int size);

// Pre-Multiply operation transforms x into x * A / 255  (where x=Y,R,G or B).
// Un-Multiply operation transforms x into x * 255 / A.

// Pre-Multiply or Un-Multiply (if 'inverse' is true) argb values in a row.
extern void (*WebPMultARGBRow)(uint32_t* const ptr, int width, int inverse);

// Same a WebPMultARGBRow(), but for several rows.
void WebPMultARGBRows(uint8_t* ptr, int stride, int width, int num_rows,
                      int inverse);

// Same for a row of single values, with side alpha values.
extern void (*WebPMultRow)(uint8_t* WEBP_RESTRICT const ptr,
                           const uint8_t* WEBP_RESTRICT const alpha,
                           int width, int inverse);

// Same a WebPMultRow(), but for several 'num_rows' rows.
void WebPMultRows(uint8_t* WEBP_RESTRICT ptr, int stride,
                  const uint8_t* WEBP_RESTRICT alpha, int alpha_stride,
                  int width, int num_rows, int inverse);

// Plain-C versions, used as fallback by some implementations.
void WebPMultRow_C(uint8_t* WEBP_RESTRICT const ptr,
                   const uint8_t* WEBP_RESTRICT const alpha,
                   int width, int inverse);
void WebPMultARGBRow_C(uint32_t* const ptr, int width, int inverse);

#ifdef WORDS_BIGENDIAN
// ARGB packing function: a/r/g/b input is rgba or bgra order.
extern void (*WebPPackARGB)(const uint8_t* WEBP_RESTRICT a,
                            const uint8_t* WEBP_RESTRICT r,
                            const uint8_t* WEBP_RESTRICT g,
                            const uint8_t* WEBP_RESTRICT b,
                            int len, uint32_t* WEBP_RESTRICT out);
#endif

// RGB packing function. 'step' can be 3 or 4. r/g/b input is rgb or bgr order.
extern void (*WebPPackRGB)(const uint8_t* WEBP_RESTRICT r,
                           const uint8_t* WEBP_RESTRICT g,
                           const uint8_t* WEBP_RESTRICT b,
                           int len, int step, uint32_t* WEBP_RESTRICT out);

// This function returns true if src[i] contains a value different from 0xff.
extern int (*WebPHasAlpha8b)(const uint8_t* src, int length);
// This function returns true if src[4*i] contains a value different from 0xff.
extern int (*WebPHasAlpha32b)(const uint8_t* src, int length);
// replaces transparent values in src[] by 'color'.
extern void (*WebPAlphaReplace)(uint32_t* src, int length, uint32_t color);

// To be called first before using the above.
void WebPInitAlphaProcessing(void);

//------------------------------------------------------------------------------
// Filter functions

typedef enum {     // Filter types.
  WEBP_FILTER_NONE = 0,
  WEBP_FILTER_HORIZONTAL,
  WEBP_FILTER_VERTICAL,
  WEBP_FILTER_GRADIENT,
  WEBP_FILTER_LAST = WEBP_FILTER_GRADIENT + 1,  // end marker
  WEBP_FILTER_BEST,    // meta-types
  WEBP_FILTER_FAST
} WEBP_FILTER_TYPE;

typedef void (*WebPFilterFunc)(const uint8_t* WEBP_RESTRICT in,
                               int width, int height, int stride,
                               uint8_t* WEBP_RESTRICT out);
// In-place un-filtering.
// Warning! 'prev_line' pointer can be equal to 'cur_line' or 'preds'.
typedef void (*WebPUnfilterFunc)(const uint8_t* prev_line, const uint8_t* preds,
                                 uint8_t* cur_line, int width);

// Filter the given data using the given predictor.
// 'in' corresponds to a 2-dimensional pixel array of size (stride * height)
// in raster order.
// 'stride' is number of bytes per scan line (with possible padding).
// 'out' should be pre-allocated.
extern WebPFilterFunc WebPFilters[WEBP_FILTER_LAST];

// In-place reconstruct the original data from the given filtered data.
// The reconstruction will be done for 'num_rows' rows starting from 'row'
// (assuming rows upto 'row - 1' are already reconstructed).
extern WebPUnfilterFunc WebPUnfilters[WEBP_FILTER_LAST];

// To be called first before using the above.
void VP8FiltersInit(void);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_DSP_DSP_H_
