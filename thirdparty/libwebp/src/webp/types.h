// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Common types + memory wrappers
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_WEBP_TYPES_H_
#define WEBP_WEBP_TYPES_H_

#include <stddef.h>  // for size_t

#ifndef _MSC_VER
#include <inttypes.h>
#if defined(__cplusplus) || !defined(__STRICT_ANSI__) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define WEBP_INLINE inline
#else
#define WEBP_INLINE
#endif
#else
typedef signed   char int8_t;
typedef unsigned char uint8_t;
typedef signed   short int16_t;
typedef unsigned short uint16_t;
typedef signed   int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef long long int int64_t;
#define WEBP_INLINE __forceinline
#endif  /* _MSC_VER */

#ifndef WEBP_NODISCARD
#if defined(WEBP_ENABLE_NODISCARD) && WEBP_ENABLE_NODISCARD
#if (defined(__cplusplus) && __cplusplus >= 201703L) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#define WEBP_NODISCARD [[nodiscard]]
#else
// gcc's __attribute__((warn_unused_result)) does not work for enums.
#if defined(__clang__) && defined(__has_attribute)
#if __has_attribute(warn_unused_result)
#define WEBP_NODISCARD __attribute__((warn_unused_result))
#else
#define WEBP_NODISCARD
#endif  /* __has_attribute(warn_unused_result) */
#else
#define WEBP_NODISCARD
#endif  /* defined(__clang__) && defined(__has_attribute) */
#endif  /* (defined(__cplusplus) && __cplusplus >= 201700L) ||
           (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L) */
#else
#define WEBP_NODISCARD
#endif  /* defined(WEBP_ENABLE_NODISCARD) && WEBP_ENABLE_NODISCARD */
#endif  /* WEBP_NODISCARD */

#ifndef WEBP_EXTERN
// This explicitly marks library functions and allows for changing the
// signature for e.g., Windows DLL builds.
# if defined(_WIN32) && defined(WEBP_DLL)
#  define WEBP_EXTERN __declspec(dllexport)
# elif defined(__GNUC__) && __GNUC__ >= 4
#  define WEBP_EXTERN extern __attribute__ ((visibility ("default")))
# else
#  define WEBP_EXTERN extern
# endif  /* defined(_WIN32) && defined(WEBP_DLL) */
#endif  /* WEBP_EXTERN */

// Macro to check ABI compatibility (same major revision number)
#define WEBP_ABI_IS_INCOMPATIBLE(a, b) (((a) >> 8) != ((b) >> 8))

#ifdef __cplusplus
extern "C" {
#endif

// Allocates 'size' bytes of memory. Returns NULL upon error. Memory
// must be deallocated by calling WebPFree(). This function is made available
// by the core 'libwebp' library.
WEBP_NODISCARD WEBP_EXTERN void* WebPMalloc(size_t size);

// Releases memory returned by the WebPDecode*() functions (from decode.h).
WEBP_EXTERN void WebPFree(void* ptr);

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_WEBP_TYPES_H_
