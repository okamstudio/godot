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

#ifndef WEBP_SHARPYUV_SHARPYUV_H_
#define WEBP_SHARPYUV_SHARPYUV_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SHARPYUV_EXTERN
#ifdef WEBP_EXTERN
#define SHARPYUV_EXTERN WEBP_EXTERN
#else
// This explicitly marks library functions and allows for changing the
// signature for e.g., Windows DLL builds.
#if defined(_WIN32) && defined(WEBP_DLL)
#define SHARPYUV_EXTERN __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define SHARPYUV_EXTERN extern __attribute__((visibility("default")))
#else
#define SHARPYUV_EXTERN extern
#endif /* defined(_WIN32) && defined(WEBP_DLL) */
#endif /* WEBP_EXTERN */
#endif /* SHARPYUV_EXTERN */

#ifndef SHARPYUV_INLINE
#ifdef WEBP_INLINE
#define SHARPYUV_INLINE WEBP_INLINE
#else
#ifndef _MSC_VER
#if defined(__cplusplus) || !defined(__STRICT_ANSI__) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define SHARPYUV_INLINE inline
#else
#define SHARPYUV_INLINE
#endif
#else
#define SHARPYUV_INLINE __forceinline
#endif /* _MSC_VER */
#endif /* WEBP_INLINE */
#endif /* SHARPYUV_INLINE */

// SharpYUV API version following the convention from semver.org
#define SHARPYUV_VERSION_MAJOR 0
#define SHARPYUV_VERSION_MINOR 4
#define SHARPYUV_VERSION_PATCH 1
// Version as a uint32_t. The major number is the high 8 bits.
// The minor number is the middle 8 bits. The patch number is the low 16 bits.
#define SHARPYUV_MAKE_VERSION(MAJOR, MINOR, PATCH) \
  (((MAJOR) << 24) | ((MINOR) << 16) | (PATCH))
#define SHARPYUV_VERSION                                                \
  SHARPYUV_MAKE_VERSION(SHARPYUV_VERSION_MAJOR, SHARPYUV_VERSION_MINOR, \
                        SHARPYUV_VERSION_PATCH)

// Returns the library's version number, packed in hexadecimal. See
// SHARPYUV_VERSION.
SHARPYUV_EXTERN int SharpYuvGetVersion(void);

// RGB to YUV conversion matrix, in 16 bit fixed point.
// y_ = rgb_to_y[0] * r + rgb_to_y[1] * g + rgb_to_y[2] * b + rgb_to_y[3]
// u_ = rgb_to_u[0] * r + rgb_to_u[1] * g + rgb_to_u[2] * b + rgb_to_u[3]
// v_ = rgb_to_v[0] * r + rgb_to_v[1] * g + rgb_to_v[2] * b + rgb_to_v[3]
// Then the values are divided by 1<<16 and rounded.
// y = (y_ + (1 << 15)) >> 16
// u = (u_ + (1 << 15)) >> 16
// v = (v_ + (1 << 15)) >> 16
//
// Typically, the offset values rgb_to_y[3], rgb_to_u[3] and rgb_to_v[3] depend
// on the input's bit depth, e.g., rgb_to_u[3] = 1 << (rgb_bit_depth - 1 + 16).
// See also sharpyuv_csp.h to get a predefined matrix or generate a matrix.
typedef struct {
  int rgb_to_y[4];
  int rgb_to_u[4];
  int rgb_to_v[4];
} SharpYuvConversionMatrix;

typedef struct SharpYuvOptions SharpYuvOptions;

// Enums for transfer functions, as defined in H.273,
// https://www.itu.int/rec/T-REC-H.273-202107-I/en
typedef enum SharpYuvTransferFunctionType {
  // 0 is reserved
  kSharpYuvTransferFunctionBt709 = 1,
  // 2 is unspecified
  // 3 is reserved
  kSharpYuvTransferFunctionBt470M = 4,
  kSharpYuvTransferFunctionBt470Bg = 5,
  kSharpYuvTransferFunctionBt601 = 6,
  kSharpYuvTransferFunctionSmpte240 = 7,
  kSharpYuvTransferFunctionLinear = 8,
  kSharpYuvTransferFunctionLog100 = 9,
  kSharpYuvTransferFunctionLog100_Sqrt10 = 10,
  kSharpYuvTransferFunctionIec61966 = 11,
  kSharpYuvTransferFunctionBt1361 = 12,
  kSharpYuvTransferFunctionSrgb = 13,
  kSharpYuvTransferFunctionBt2020_10Bit = 14,
  kSharpYuvTransferFunctionBt2020_12Bit = 15,
  kSharpYuvTransferFunctionSmpte2084 = 16,  // PQ
  kSharpYuvTransferFunctionSmpte428 = 17,
  kSharpYuvTransferFunctionHlg = 18,
  kSharpYuvTransferFunctionNum
} SharpYuvTransferFunctionType;

// Converts RGB to YUV420 using a downsampling algorithm that minimizes
// artefacts caused by chroma subsampling.
// This is slower than standard downsampling (averaging of 4 UV values).
// Assumes that the image will be upsampled using a bilinear filter. If nearest
// neighbor is used instead, the upsampled image might look worse than with
// standard downsampling.
// r_ptr, g_ptr, b_ptr: pointers to the source r, g and b channels. Should point
//     to uint8_t buffers if rgb_bit_depth is 8, or uint16_t buffers otherwise.
// rgb_step: distance in bytes between two horizontally adjacent pixels on the
//     r, g and b channels. If rgb_bit_depth is > 8, it should be a
//     multiple of 2.
// rgb_stride: distance in bytes between two vertically adjacent pixels on the
//     r, g, and b channels. If rgb_bit_depth is > 8, it should be a
//     multiple of 2.
// rgb_bit_depth: number of bits for each r/g/b value. One of: 8, 10, 12, 16.
//     Note: 16 bit input is truncated to 14 bits before conversion to yuv.
// yuv_bit_depth: number of bits for each y/u/v value. One of: 8, 10, 12.
// y_ptr, u_ptr, v_ptr: pointers to the destination y, u and v channels.  Should
//     point to uint8_t buffers if yuv_bit_depth is 8, or uint16_t buffers
//     otherwise.
// y_stride, u_stride, v_stride: distance in bytes between two vertically
//     adjacent pixels on the y, u and v channels. If yuv_bit_depth > 8, they
//     should be multiples of 2.
// width, height: width and height of the image in pixels
// yuv_matrix: RGB to YUV conversion matrix. The matrix values typically
//     depend on the input's rgb_bit_depth.
// This function calls SharpYuvConvertWithOptions with a default transfer
// function of kSharpYuvTransferFunctionSrgb.
SHARPYUV_EXTERN int SharpYuvConvert(const void* r_ptr, const void* g_ptr,
                                    const void* b_ptr, int rgb_step,
                                    int rgb_stride, int rgb_bit_depth,
                                    void* y_ptr, int y_stride, void* u_ptr,
                                    int u_stride, void* v_ptr, int v_stride,
                                    int yuv_bit_depth, int width, int height,
                                    const SharpYuvConversionMatrix* yuv_matrix);

struct SharpYuvOptions {
  // This matrix cannot be NULL and can be initialized by
  // SharpYuvComputeConversionMatrix.
  const SharpYuvConversionMatrix* yuv_matrix;
  SharpYuvTransferFunctionType transfer_type;
};

// Internal, version-checked, entry point
SHARPYUV_EXTERN int SharpYuvOptionsInitInternal(const SharpYuvConversionMatrix*,
                                                SharpYuvOptions*, int);

// Should always be called, to initialize a fresh SharpYuvOptions
// structure before modification. SharpYuvOptionsInit() must have succeeded
// before using the 'options' object.
static SHARPYUV_INLINE int SharpYuvOptionsInit(
    const SharpYuvConversionMatrix* yuv_matrix, SharpYuvOptions* options) {
  return SharpYuvOptionsInitInternal(yuv_matrix, options, SHARPYUV_VERSION);
}

SHARPYUV_EXTERN int SharpYuvConvertWithOptions(
    const void* r_ptr, const void* g_ptr, const void* b_ptr, int rgb_step,
    int rgb_stride, int rgb_bit_depth, void* y_ptr, int y_stride, void* u_ptr,
    int u_stride, void* v_ptr, int v_stride, int yuv_bit_depth, int width,
    int height, const SharpYuvOptions* options);

// TODO(b/194336375): Add YUV444 to YUV420 conversion. Maybe also add 422
// support (it's rarely used in practice, especially for images).

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // WEBP_SHARPYUV_SHARPYUV_H_
