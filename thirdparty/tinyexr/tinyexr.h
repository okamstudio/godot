#ifndef TINYEXR_H_
#define TINYEXR_H_
/*
Copyright (c) 2014 - 2021, Syoyo Fujita and many contributors.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Syoyo Fujita nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// TinyEXR contains some OpenEXR code, which is licensed under ------------

///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

// End of OpenEXR license -------------------------------------------------


//
//
//   Do this:
//    #define TINYEXR_IMPLEMENTATION
//   before you include this file in *one* C or C++ file to create the
//   implementation.
//
//   // i.e. it should look like this:
//   #include ...
//   #include ...
//   #include ...
//   #define TINYEXR_IMPLEMENTATION
//   #include "tinyexr.h"
//
//

#include <stddef.h>  // for size_t
#include <stdint.h>  // guess stdint.h is available(C99)

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || \
    defined(__i386) || defined(__i486__) || defined(__i486) ||  \
    defined(i386) || defined(__ia64__) || defined(__x86_64__)
#define TINYEXR_X86_OR_X64_CPU 1
#else
#define TINYEXR_X86_OR_X64_CPU 0
#endif

#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || TINYEXR_X86_OR_X64_CPU
#define TINYEXR_LITTLE_ENDIAN 1
#else
#define TINYEXR_LITTLE_ENDIAN 0
#endif

// Use miniz or not to decode ZIP format pixel. Linking with zlib
// required if this flag is 0 and TINYEXR_USE_STB_ZLIB is 0.
#ifndef TINYEXR_USE_MINIZ
#define TINYEXR_USE_MINIZ (1)
#endif

// Use the ZIP implementation of stb_image.h and stb_image_write.h.
#ifndef TINYEXR_USE_STB_ZLIB
#define TINYEXR_USE_STB_ZLIB (0)
#endif

// Use nanozlib.
#ifndef TINYEXR_USE_NANOZLIB
#define TINYEXR_USE_NANOZLIB (0)
#endif

// Disable PIZ compression when applying cpplint.
#ifndef TINYEXR_USE_PIZ
#define TINYEXR_USE_PIZ (1)
#endif

#ifndef TINYEXR_USE_ZFP
#define TINYEXR_USE_ZFP (0)  // TinyEXR extension.
// http://computation.llnl.gov/projects/floating-point-compression
#endif

#ifndef TINYEXR_USE_THREAD
#define TINYEXR_USE_THREAD (0)  // No threaded loading.
// http://computation.llnl.gov/projects/floating-point-compression
#endif

#ifndef TINYEXR_USE_OPENMP
#ifdef _OPENMP
#define TINYEXR_USE_OPENMP (1)
#else
#define TINYEXR_USE_OPENMP (0)
#endif
#endif

#define TINYEXR_SUCCESS (0)
#define TINYEXR_ERROR_INVALID_MAGIC_NUMBER (-1)
#define TINYEXR_ERROR_INVALID_EXR_VERSION (-2)
#define TINYEXR_ERROR_INVALID_ARGUMENT (-3)
#define TINYEXR_ERROR_INVALID_DATA (-4)
#define TINYEXR_ERROR_INVALID_FILE (-5)
#define TINYEXR_ERROR_INVALID_PARAMETER (-6)
#define TINYEXR_ERROR_CANT_OPEN_FILE (-7)
#define TINYEXR_ERROR_UNSUPPORTED_FORMAT (-8)
#define TINYEXR_ERROR_INVALID_HEADER (-9)
#define TINYEXR_ERROR_UNSUPPORTED_FEATURE (-10)
#define TINYEXR_ERROR_CANT_WRITE_FILE (-11)
#define TINYEXR_ERROR_SERIALIZATION_FAILED (-12)
#define TINYEXR_ERROR_LAYER_NOT_FOUND (-13)
#define TINYEXR_ERROR_DATA_TOO_LARGE (-14)

// @note { OpenEXR file format: http://www.openexr.com/openexrfilelayout.pdf }

// pixel type: possible values are: UINT = 0 HALF = 1 FLOAT = 2
#define TINYEXR_PIXELTYPE_UINT (0)
#define TINYEXR_PIXELTYPE_HALF (1)
#define TINYEXR_PIXELTYPE_FLOAT (2)

#define TINYEXR_MAX_HEADER_ATTRIBUTES (1024)
#define TINYEXR_MAX_CUSTOM_ATTRIBUTES (128)

#define TINYEXR_COMPRESSIONTYPE_NONE (0)
#define TINYEXR_COMPRESSIONTYPE_RLE (1)
#define TINYEXR_COMPRESSIONTYPE_ZIPS (2)
#define TINYEXR_COMPRESSIONTYPE_ZIP (3)
#define TINYEXR_COMPRESSIONTYPE_PIZ (4)
#define TINYEXR_COMPRESSIONTYPE_ZFP (128)  // TinyEXR extension

#define TINYEXR_ZFP_COMPRESSIONTYPE_RATE (0)
#define TINYEXR_ZFP_COMPRESSIONTYPE_PRECISION (1)
#define TINYEXR_ZFP_COMPRESSIONTYPE_ACCURACY (2)

#define TINYEXR_TILE_ONE_LEVEL (0)
#define TINYEXR_TILE_MIPMAP_LEVELS (1)
#define TINYEXR_TILE_RIPMAP_LEVELS (2)

#define TINYEXR_TILE_ROUND_DOWN (0)
#define TINYEXR_TILE_ROUND_UP (1)

typedef struct TEXRVersion {
  int version;    // this must be 2
  // tile format image;
  // not zero for only a single-part "normal" tiled file (according to spec.)
  int tiled;
  int long_name;  // long name attribute
  // deep image(EXR 2.0);
  // for a multi-part file, indicates that at least one part is of type deep* (according to spec.)
  int non_image;
  int multipart;  // multi-part(EXR 2.0)
} EXRVersion;

typedef struct TEXRAttribute {
  char name[256];  // name and type are up to 255 chars long.
  char type[256];
  unsigned char *value;  // uint8_t*
  int size;
  int pad0;
} EXRAttribute;

typedef struct TEXRChannelInfo {
  char name[256];  // less than 255 bytes long
  int pixel_type;
  int x_sampling;
  int y_sampling;
  unsigned char p_linear;
  unsigned char pad[3];
} EXRChannelInfo;

typedef struct TEXRTile {
  int offset_x;
  int offset_y;
  int level_x;
  int level_y;

  int width;   // actual width in a tile.
  int height;  // actual height int a tile.

  unsigned char **images;  // image[channels][pixels]
} EXRTile;

typedef struct TEXRBox2i {
  int min_x;
  int min_y;
  int max_x;
  int max_y;
} EXRBox2i;

typedef struct TEXRHeader {
  float pixel_aspect_ratio;
  int line_order;
  EXRBox2i data_window;
  EXRBox2i display_window;
  float screen_window_center[2];
  float screen_window_width;

  int chunk_count;

  // Properties for tiled format(`tiledesc`).
  int tiled;
  int tile_size_x;
  int tile_size_y;
  int tile_level_mode;
  int tile_rounding_mode;

  int long_name;
  // for a single-part file, agree with the version field bit 11
  // for a multi-part file, it is consistent with the type of part
  int non_image;
  int multipart;
  unsigned int header_len;

  // Custom attributes(exludes required attributes(e.g. `channels`,
  // `compression`, etc)
  int num_custom_attributes;
  EXRAttribute *custom_attributes;  // array of EXRAttribute. size =
                                    // `num_custom_attributes`.

  EXRChannelInfo *channels;  // [num_channels]

  int *pixel_types;  // Loaded pixel type(TINYEXR_PIXELTYPE_*) of `images` for
  // each channel. This is overwritten with `requested_pixel_types` when
  // loading.
  int num_channels;

  int compression_type;        // compression type(TINYEXR_COMPRESSIONTYPE_*)
  int *requested_pixel_types;  // Filled initially by
                               // ParseEXRHeaderFrom(Meomory|File), then users
                               // can edit it(only valid for HALF pixel type
                               // channel)
  // name attribute required for multipart files;
  // must be unique and non empty (according to spec.);
  // use EXRSetNameAttr for setting value;
  // max 255 character allowed - excluding terminating zero
  char name[256];
} EXRHeader;

typedef struct TEXRMultiPartHeader {
  int num_headers;
  EXRHeader *headers;

} EXRMultiPartHeader;

typedef struct TEXRImage {
  EXRTile *tiles;  // Tiled pixel data. The application must reconstruct image
                   // from tiles manually. NULL if scanline format.
  struct TEXRImage* next_level; // NULL if scanline format or image is the last level.
  int level_x; // x level index
  int level_y; // y level index

  unsigned char **images;  // image[channels][pixels]. NULL if tiled format.

  int width;
  int height;
  int num_channels;

  // Properties for tile format.
  int num_tiles;

} EXRImage;

typedef struct TEXRMultiPartImage {
  int num_images;
  EXRImage *images;

} EXRMultiPartImage;

typedef struct TDeepImage {
  const char **channel_names;
  float ***image;      // image[channels][scanlines][samples]
  int **offset_table;  // offset_table[scanline][offsets]
  int num_channels;
  int width;
  int height;
  int pad0;
} DeepImage;

// @deprecated { For backward compatibility. Not recommended to use. }
// Loads single-frame OpenEXR image. Assume EXR image contains A(single channel
// alpha) or RGB(A) channels.
// Application must free image data as returned by `out_rgba`
// Result image format is: float x RGBA x width x hight
// Returns negative value and may set error string in `err` when there's an
// error
extern int LoadEXR(float **out_rgba, int *width, int *height,
                   const char *filename, const char **err);

// Loads single-frame OpenEXR image by specifying layer name. Assume EXR image
// contains A(single channel alpha) or RGB(A) channels. Application must free
// image data as returned by `out_rgba` Result image format is: float x RGBA x
// width x hight Returns negative value and may set error string in `err` when
// there's an error When the specified layer name is not found in the EXR file,
// the function will return `TINYEXR_ERROR_LAYER_NOT_FOUND`.
extern int LoadEXRWithLayer(float **out_rgba, int *width, int *height,
                            const char *filename, const char *layer_name,
                            const char **err);

//
// Get layer infos from EXR file.
//
// @param[out] layer_names List of layer names. Application must free memory
// after using this.
// @param[out] num_layers The number of layers
// @param[out] err Error string(will be filled when the function returns error
// code). Free it using FreeEXRErrorMessage after using this value.
//
// @return TINYEXR_SUCCEES upon success.
//
extern int EXRLayers(const char *filename, const char **layer_names[],
                     int *num_layers, const char **err);

// @deprecated
// Simple wrapper API for ParseEXRHeaderFromFile.
// checking given file is a EXR file(by just look up header)
// @return TINYEXR_SUCCEES for EXR image, TINYEXR_ERROR_INVALID_HEADER for
// others
extern int IsEXR(const char *filename);

// Simple wrapper API for ParseEXRHeaderFromMemory.
// Check if given data is a EXR image(by just looking up a header section)
// @return TINYEXR_SUCCEES for EXR image, TINYEXR_ERROR_INVALID_HEADER for
// others
extern int IsEXRFromMemory(const unsigned char *memory, size_t size);

// @deprecated
// Saves single-frame OpenEXR image to a buffer. Assume EXR image contains RGB(A) channels.
// components must be 1(Grayscale), 3(RGB) or 4(RGBA).
// Input image format is: `float x width x height`, or `float x RGB(A) x width x
// hight`
// Save image as fp16(HALF) format when `save_as_fp16` is positive non-zero
// value.
// Save image as fp32(FLOAT) format when `save_as_fp16` is 0.
// Use ZIP compression by default.
// `buffer` is the pointer to write EXR data.
// Memory for `buffer` is allocated internally in SaveEXRToMemory.
// Returns the data size of EXR file when the value is positive(up to 2GB EXR data).
// Returns negative value and may set error string in `err` when there's an
// error
extern int SaveEXRToMemory(const float *data, const int width, const int height,
                   const int components, const int save_as_fp16,
                   unsigned char **buffer, const char **err);

// @deprecated { Not recommended, but handy to use. }
// Saves single-frame OpenEXR image to a buffer. Assume EXR image contains RGB(A) channels.
// components must be 1(Grayscale), 3(RGB) or 4(RGBA).
// Input image format is: `float x width x height`, or `float x RGB(A) x width x
// hight`
// Save image as fp16(HALF) format when `save_as_fp16` is positive non-zero
// value.
// Save image as fp32(FLOAT) format when `save_as_fp16` is 0.
// Use ZIP compression by default.
// Returns TINYEXR_SUCCEES(0) when success.
// Returns negative value and may set error string in `err` when there's an
// error
extern int SaveEXR(const float *data, const int width, const int height,
                   const int components, const int save_as_fp16,
                   const char *filename, const char **err);

// Returns the number of resolution levels of the image (including the base)
extern int EXRNumLevels(const EXRImage* exr_image);

// Initialize EXRHeader struct
extern void InitEXRHeader(EXRHeader *exr_header);

// Set name attribute of EXRHeader struct (it makes a copy)
extern void EXRSetNameAttr(EXRHeader *exr_header, const char* name);

// Initialize EXRImage struct
extern void InitEXRImage(EXRImage *exr_image);

// Frees internal data of EXRHeader struct
extern int FreeEXRHeader(EXRHeader *exr_header);

// Frees internal data of EXRImage struct
extern int FreeEXRImage(EXRImage *exr_image);

// Frees error message
extern void FreeEXRErrorMessage(const char *msg);

// Parse EXR version header of a file.
extern int ParseEXRVersionFromFile(EXRVersion *version, const char *filename);

// Parse EXR version header from memory-mapped EXR data.
extern int ParseEXRVersionFromMemory(EXRVersion *version,
                                     const unsigned char *memory, size_t size);

// Parse single-part OpenEXR header from a file and initialize `EXRHeader`.
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int ParseEXRHeaderFromFile(EXRHeader *header, const EXRVersion *version,
                                  const char *filename, const char **err);

// Parse single-part OpenEXR header from a memory and initialize `EXRHeader`.
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int ParseEXRHeaderFromMemory(EXRHeader *header,
                                    const EXRVersion *version,
                                    const unsigned char *memory, size_t size,
                                    const char **err);

// Parse multi-part OpenEXR headers from a file and initialize `EXRHeader*`
// array.
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int ParseEXRMultipartHeaderFromFile(EXRHeader ***headers,
                                           int *num_headers,
                                           const EXRVersion *version,
                                           const char *filename,
                                           const char **err);

// Parse multi-part OpenEXR headers from a memory and initialize `EXRHeader*`
// array
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int ParseEXRMultipartHeaderFromMemory(EXRHeader ***headers,
                                             int *num_headers,
                                             const EXRVersion *version,
                                             const unsigned char *memory,
                                             size_t size, const char **err);

// Loads single-part OpenEXR image from a file.
// Application must setup `ParseEXRHeaderFromFile` before calling this function.
// Application can free EXRImage using `FreeEXRImage`
// Returns negative value and may set error string in `err` when there's an
// error
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int LoadEXRImageFromFile(EXRImage *image, const EXRHeader *header,
                                const char *filename, const char **err);

// Loads single-part OpenEXR image from a memory.
// Application must setup `EXRHeader` with
// `ParseEXRHeaderFromMemory` before calling this function.
// Application can free EXRImage using `FreeEXRImage`
// Returns negative value and may set error string in `err` when there's an
// error
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int LoadEXRImageFromMemory(EXRImage *image, const EXRHeader *header,
                                  const unsigned char *memory,
                                  const size_t size, const char **err);

// Loads multi-part OpenEXR image from a file.
// Application must setup `ParseEXRMultipartHeaderFromFile` before calling this
// function.
// Application can free EXRImage using `FreeEXRImage`
// Returns negative value and may set error string in `err` when there's an
// error
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int LoadEXRMultipartImageFromFile(EXRImage *images,
                                         const EXRHeader **headers,
                                         unsigned int num_parts,
                                         const char *filename,
                                         const char **err);

// Loads multi-part OpenEXR image from a memory.
// Application must setup `EXRHeader*` array with
// `ParseEXRMultipartHeaderFromMemory` before calling this function.
// Application can free EXRImage using `FreeEXRImage`
// Returns negative value and may set error string in `err` when there's an
// error
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int LoadEXRMultipartImageFromMemory(EXRImage *images,
                                           const EXRHeader **headers,
                                           unsigned int num_parts,
                                           const unsigned char *memory,
                                           const size_t size, const char **err);

// Saves multi-channel, single-frame OpenEXR image to a file.
// Returns negative value and may set error string in `err` when there's an
// error
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int SaveEXRImageToFile(const EXRImage *image,
                              const EXRHeader *exr_header, const char *filename,
                              const char **err);

// Saves multi-channel, single-frame OpenEXR image to a memory.
// Image is compressed using EXRImage.compression value.
// Return the number of bytes if success.
// Return zero and will set error string in `err` when there's an
// error.
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern size_t SaveEXRImageToMemory(const EXRImage *image,
                                   const EXRHeader *exr_header,
                                   unsigned char **memory, const char **err);

// Saves multi-channel, multi-frame OpenEXR image to a memory.
// Image is compressed using EXRImage.compression value.
// File global attributes (eg. display_window) must be set in the first header.
// Returns negative value and may set error string in `err` when there's an
// error
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int SaveEXRMultipartImageToFile(const EXRImage *images,
                                       const EXRHeader **exr_headers,
                                       unsigned int num_parts,
                                       const char *filename, const char **err);

// Saves multi-channel, multi-frame OpenEXR image to a memory.
// Image is compressed using EXRImage.compression value.
// File global attributes (eg. display_window) must be set in the first header.
// Return the number of bytes if success.
// Return zero and will set error string in `err` when there's an
// error.
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern size_t SaveEXRMultipartImageToMemory(const EXRImage *images,
                                            const EXRHeader **exr_headers,
                                            unsigned int num_parts,
                                            unsigned char **memory, const char **err);
// Loads single-frame OpenEXR deep image.
// Application must free memory of variables in DeepImage(image, offset_table)
// Returns negative value and may set error string in `err` when there's an
// error
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int LoadDeepEXR(DeepImage *out_image, const char *filename,
                       const char **err);

// NOT YET IMPLEMENTED:
// Saves single-frame OpenEXR deep image.
// Returns negative value and may set error string in `err` when there's an
// error
// extern int SaveDeepEXR(const DeepImage *in_image, const char *filename,
//                       const char **err);

// NOT YET IMPLEMENTED:
// Loads multi-part OpenEXR deep image.
// Application must free memory of variables in DeepImage(image, offset_table)
// extern int LoadMultiPartDeepEXR(DeepImage **out_image, int num_parts, const
// char *filename,
//                       const char **err);

// For emscripten.
// Loads single-frame OpenEXR image from memory. Assume EXR image contains
// RGB(A) channels.
// Returns negative value and may set error string in `err` when there's an
// error
// When there was an error message, Application must free `err` with
// FreeEXRErrorMessage()
extern int LoadEXRFromMemory(float **out_rgba, int *width, int *height,
                             const unsigned char *memory, size_t size,
                             const char **err);

#ifdef __cplusplus
}
#endif

#endif  // TINYEXR_H_

#ifdef TINYEXR_IMPLEMENTATION
#ifndef TINYEXR_IMPLEMENTATION_DEFINED
#define TINYEXR_IMPLEMENTATION_DEFINED

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>  // for UTF-8 and memory-mapping

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#define TINYEXR_USE_WIN32_MMAP (1)
#endif

#elif defined(__linux__) || defined(__unix__)
#include <fcntl.h>     // for open()
#include <sys/mman.h>  // for memory-mapping
#include <sys/stat.h>  // for stat
#include <unistd.h>    // for close()
#define TINYEXR_USE_POSIX_MMAP (1)
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

//#include <iostream> // debug

#include <limits>
#include <string>
#include <vector>
#include <set>

// https://stackoverflow.com/questions/5047971/how-do-i-check-for-c11-support
#if __cplusplus > 199711L || (defined(_MSC_VER) && _MSC_VER >= 1900)
#define TINYEXR_HAS_CXX11 (1)
// C++11
#include <cstdint>

#if TINYEXR_USE_THREAD
#include <atomic>
#include <thread>
#endif

#else  // __cplusplus > 199711L
#define TINYEXR_HAS_CXX11 (0)
#endif  // __cplusplus > 199711L

#if TINYEXR_USE_OPENMP
#include <omp.h>
#endif

#if defined(TINYEXR_USE_MINIZ) && (TINYEXR_USE_MINIZ==1)
#include <miniz.h>
#else
//  Issue #46. Please include your own zlib-compatible API header before
//  including `tinyexr.h`
//#include "zlib.h"
#endif

#if defined(TINYEXR_USE_NANOZLIB) && (TINYEXR_USE_NANOZLIB==1)
#define NANOZLIB_IMPLEMENTATION
#include "nanozlib.h"
#endif

#if TINYEXR_USE_STB_ZLIB
// Since we don't know where a project has stb_image.h and stb_image_write.h
// and whether they are in the include path, we don't include them here, and
// instead declare the two relevant functions manually.
// from stb_image.h:
extern "C" int stbi_zlib_decode_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);
// from stb_image_write.h:
extern "C" unsigned char *stbi_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality);
#endif


#if TINYEXR_USE_ZFP

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "zfp.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif

// cond: conditional expression
// msg: std::string
// err: std::string*
#define TINYEXR_CHECK_AND_RETURN_MSG(cond, msg, err) do { \
  if (!(cond)) { \
    if (!err) { \
      std::ostringstream ss_e; \
      ss_e << __func__ << "():" << __LINE__ << msg << "\n"; \
      (*err) += ss_e.str(); \
    } \
    return false;\
  } \
  } while(0)

// no error message.
#define TINYEXR_CHECK_AND_RETURN_C(cond, retcode) do { \
  if (!(cond)) { \
    return retcode; \
  } \
  } while(0)

namespace tinyexr {

#if __cplusplus > 199711L
// C++11
typedef uint64_t tinyexr_uint64;
typedef int64_t tinyexr_int64;
#else
// Although `long long` is not a standard type pre C++11, assume it is defined
// as a compiler's extension.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++11-long-long"
#endif
typedef unsigned long long tinyexr_uint64;
typedef long long tinyexr_int64;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

// static bool IsBigEndian(void) {
//  union {
//    unsigned int i;
//    char c[4];
//  } bint = {0x01020304};
//
//  return bint.c[0] == 1;
//}

static void SetErrorMessage(const std::string &msg, const char **err) {
  if (err) {
#ifdef _WIN32
    (*err) = _strdup(msg.c_str());
#else
    (*err) = strdup(msg.c_str());
#endif
  }
}

#if 0
static void SetWarningMessage(const std::string &msg, const char **warn) {
  if (warn) {
#ifdef _WIN32
    (*warn) = _strdup(msg.c_str());
#else
    (*warn) = strdup(msg.c_str());
#endif
  }
}
#endif

static const int kEXRVersionSize = 8;

static void cpy2(unsigned short *dst_val, const unsigned short *src_val) {
  unsigned char *dst = reinterpret_cast<unsigned char *>(dst_val);
  const unsigned char *src = reinterpret_cast<const unsigned char *>(src_val);

  dst[0] = src[0];
  dst[1] = src[1];
}

static void swap2(unsigned short *val) {
#if TINYEXR_LITTLE_ENDIAN
  (void)val;
#else
  unsigned short tmp = *val;
  unsigned char *dst = reinterpret_cast<unsigned char *>(val);
  unsigned char *src = reinterpret_cast<unsigned char *>(&tmp);

  dst[0] = src[1];
  dst[1] = src[0];
#endif
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
static void cpy4(int *dst_val, const int *src_val) {
  unsigned char *dst = reinterpret_cast<unsigned char *>(dst_val);
  const unsigned char *src = reinterpret_cast<const unsigned char *>(src_val);

  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

static void cpy4(unsigned int *dst_val, const unsigned int *src_val) {
  unsigned char *dst = reinterpret_cast<unsigned char *>(dst_val);
  const unsigned char *src = reinterpret_cast<const unsigned char *>(src_val);

  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

static void cpy4(float *dst_val, const float *src_val) {
  unsigned char *dst = reinterpret_cast<unsigned char *>(dst_val);
  const unsigned char *src = reinterpret_cast<const unsigned char *>(src_val);

  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static void swap4(unsigned int *val) {
#if TINYEXR_LITTLE_ENDIAN
  (void)val;
#else
  unsigned int tmp = *val;
  unsigned char *dst = reinterpret_cast<unsigned char *>(val);
  unsigned char *src = reinterpret_cast<unsigned char *>(&tmp);

  dst[0] = src[3];
  dst[1] = src[2];
  dst[2] = src[1];
  dst[3] = src[0];
#endif
}

static void swap4(int *val) {
#if TINYEXR_LITTLE_ENDIAN
  (void)val;
#else
  int tmp = *val;
  unsigned char *dst = reinterpret_cast<unsigned char *>(val);
  unsigned char *src = reinterpret_cast<unsigned char *>(&tmp);

  dst[0] = src[3];
  dst[1] = src[2];
  dst[2] = src[1];
  dst[3] = src[0];
#endif
}

static void swap4(float *val) {
#if TINYEXR_LITTLE_ENDIAN
  (void)val;
#else
  float tmp = *val;
  unsigned char *dst = reinterpret_cast<unsigned char *>(val);
  unsigned char *src = reinterpret_cast<unsigned char *>(&tmp);

  dst[0] = src[3];
  dst[1] = src[2];
  dst[2] = src[1];
  dst[3] = src[0];
#endif
}

#if 0
static void cpy8(tinyexr::tinyexr_uint64 *dst_val, const tinyexr::tinyexr_uint64 *src_val) {
  unsigned char *dst = reinterpret_cast<unsigned char *>(dst_val);
  const unsigned char *src = reinterpret_cast<const unsigned char *>(src_val);

  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
  dst[4] = src[4];
  dst[5] = src[5];
  dst[6] = src[6];
  dst[7] = src[7];
}
#endif

static void swap8(tinyexr::tinyexr_uint64 *val) {
#if TINYEXR_LITTLE_ENDIAN
  (void)val;
#else
  tinyexr::tinyexr_uint64 tmp = (*val);
  unsigned char *dst = reinterpret_cast<unsigned char *>(val);
  unsigned char *src = reinterpret_cast<unsigned char *>(&tmp);

  dst[0] = src[7];
  dst[1] = src[6];
  dst[2] = src[5];
  dst[3] = src[4];
  dst[4] = src[3];
  dst[5] = src[2];
  dst[6] = src[1];
  dst[7] = src[0];
#endif
}

// https://gist.github.com/rygorous/2156668
union FP32 {
  unsigned int u;
  float f;
  struct {
#if TINYEXR_LITTLE_ENDIAN
    unsigned int Mantissa : 23;
    unsigned int Exponent : 8;
    unsigned int Sign : 1;
#else
    unsigned int Sign : 1;
    unsigned int Exponent : 8;
    unsigned int Mantissa : 23;
#endif
  } s;
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#endif

union FP16 {
  unsigned short u;
  struct {
#if TINYEXR_LITTLE_ENDIAN
    unsigned int Mantissa : 10;
    unsigned int Exponent : 5;
    unsigned int Sign : 1;
#else
    unsigned int Sign : 1;
    unsigned int Exponent : 5;
    unsigned int Mantissa : 10;
#endif
  } s;
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

static FP32 half_to_float(FP16 h) {
  static const FP32 magic = {113 << 23};
  static const unsigned int shifted_exp = 0x7c00
                                          << 13;  // exponent mask after shift
  FP32 o;

  o.u = (h.u & 0x7fffU) << 13U;           // exponent/mantissa bits
  unsigned int exp_ = shifted_exp & o.u;  // just the exponent
  o.u += (127 - 15) << 23;                // exponent adjust

  // handle exponent special cases
  if (exp_ == shifted_exp)    // Inf/NaN?
    o.u += (128 - 16) << 23;  // extra exp adjust
  else if (exp_ == 0)         // Zero/Denormal?
  {
    o.u += 1 << 23;  // extra exp adjust
    o.f -= magic.f;  // renormalize
  }

  o.u |= (h.u & 0x8000U) << 16U;  // sign bit
  return o;
}

static FP16 float_to_half_full(FP32 f) {
  FP16 o = {0};

  // Based on ISPC reference code (with minor modifications)
  if (f.s.Exponent == 0)  // Signed zero/denormal (which will underflow)
    o.s.Exponent = 0;
  else if (f.s.Exponent == 255)  // Inf or NaN (all exponent bits set)
  {
    o.s.Exponent = 31;
    o.s.Mantissa = f.s.Mantissa ? 0x200 : 0;  // NaN->qNaN and Inf->Inf
  } else                                      // Normalized number
  {
    // Exponent unbias the single, then bias the halfp
    int newexp = f.s.Exponent - 127 + 15;
    if (newexp >= 31)  // Overflow, return signed infinity
      o.s.Exponent = 31;
    else if (newexp <= 0)  // Underflow
    {
      if ((14 - newexp) <= 24)  // Mantissa might be non-zero
      {
        unsigned int mant = f.s.Mantissa | 0x800000;  // Hidden 1 bit
        o.s.Mantissa = mant >> (14 - newexp);
        if ((mant >> (13 - newexp)) & 1)  // Check for rounding
          o.u++;  // Round, might overflow into exp bit, but this is OK
      }
    } else {
      o.s.Exponent = static_cast<unsigned int>(newexp);
      o.s.Mantissa = f.s.Mantissa >> 13;
      if (f.s.Mantissa & 0x1000)  // Check for rounding
        o.u++;                    // Round, might overflow to inf, this is OK
    }
  }

  o.s.Sign = f.s.Sign;
  return o;
}

// NOTE: From OpenEXR code
// #define IMF_INCREASING_Y  0
// #define IMF_DECREASING_Y  1
// #define IMF_RAMDOM_Y    2
//
// #define IMF_NO_COMPRESSION  0
// #define IMF_RLE_COMPRESSION 1
// #define IMF_ZIPS_COMPRESSION  2
// #define IMF_ZIP_COMPRESSION 3
// #define IMF_PIZ_COMPRESSION 4
// #define IMF_PXR24_COMPRESSION 5
// #define IMF_B44_COMPRESSION 6
// #define IMF_B44A_COMPRESSION  7

#ifdef __clang__
#pragma clang diagnostic push

#if __has_warning("-Wzero-as-null-pointer-constant")
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#endif

static const char *ReadString(std::string *s, const char *ptr, size_t len) {
  // Read untile NULL(\0).
  const char *p = ptr;
  const char *q = ptr;
  while ((size_t(q - ptr) < len) && (*q) != 0) {
    q++;
  }

  if (size_t(q - ptr) >= len) {
    (*s).clear();
    return NULL;
  }

  (*s) = std::string(p, q);

  return q + 1;  // skip '\0'
}

static bool ReadAttribute(std::string *name, std::string *type,
                          std::vector<unsigned char> *data, size_t *marker_size,
                          const char *marker, size_t size) {
  size_t name_len = strnlen(marker, size);
  if (name_len == size) {
    // String does not have a terminating character.
    return false;
  }
  *name = std::string(marker, name_len);

  marker += name_len + 1;
  size -= name_len + 1;

  size_t type_len = strnlen(marker, size);
  if (type_len == size) {
    return false;
  }
  *type = std::string(marker, type_len);

  marker += type_len + 1;
  size -= type_len + 1;

  if (size < sizeof(uint32_t)) {
    return false;
  }

  uint32_t data_len;
  memcpy(&data_len, marker, sizeof(uint32_t));
  tinyexr::swap4(reinterpret_cast<unsigned int *>(&data_len));

  if (data_len == 0) {
    if ((*type).compare("string") == 0) {
      // Accept empty string attribute.

      marker += sizeof(uint32_t);
      size -= sizeof(uint32_t);

      *marker_size = name_len + 1 + type_len + 1 + sizeof(uint32_t);

      data->resize(1);
      (*data)[0] = '\0';

      return true;
    } else {
      return false;
    }
  }

  marker += sizeof(uint32_t);
  size -= sizeof(uint32_t);

  if (size < data_len) {
    return false;
  }

  data->resize(static_cast<size_t>(data_len));
  memcpy(&data->at(0), marker, static_cast<size_t>(data_len));

  *marker_size = name_len + 1 + type_len + 1 + sizeof(uint32_t) + data_len;
  return true;
}

static void WriteAttributeToMemory(std::vector<unsigned char> *out,
                                   const char *name, const char *type,
                                   const unsigned char *data, int len) {
  out->insert(out->end(), name, name + strlen(name) + 1);
  out->insert(out->end(), type, type + strlen(type) + 1);

  int outLen = len;
  tinyexr::swap4(&outLen);
  out->insert(out->end(), reinterpret_cast<unsigned char *>(&outLen),
              reinterpret_cast<unsigned char *>(&outLen) + sizeof(int));
  out->insert(out->end(), data, data + len);
}

typedef struct TChannelInfo {
  std::string name;  // less than 255 bytes long
  int pixel_type;
  int requested_pixel_type;
  int x_sampling;
  int y_sampling;
  unsigned char p_linear;
  unsigned char pad[3];
} ChannelInfo;

typedef struct {
  int min_x;
  int min_y;
  int max_x;
  int max_y;
} Box2iInfo;

struct HeaderInfo {
  std::vector<tinyexr::ChannelInfo> channels;
  std::vector<EXRAttribute> attributes;

  Box2iInfo data_window;
  int line_order;
  Box2iInfo display_window;
  float screen_window_center[2];
  float screen_window_width;
  float pixel_aspect_ratio;

  int chunk_count;

  // Tiled format
  int tiled; // Non-zero if the part is tiled.
  int tile_size_x;
  int tile_size_y;
  int tile_level_mode;
  int tile_rounding_mode;

  unsigned int header_len;

  int compression_type;

  // required for multi-part or non-image files
  std::string name;
  // required for multi-part or non-image files
  std::string type;

  void clear() {
    channels.clear();
    attributes.clear();

    data_window.min_x = 0;
    data_window.min_y = 0;
    data_window.max_x = 0;
    data_window.max_y = 0;
    line_order = 0;
    display_window.min_x = 0;
    display_window.min_y = 0;
    display_window.max_x = 0;
    display_window.max_y = 0;
    screen_window_center[0] = 0.0f;
    screen_window_center[1] = 0.0f;
    screen_window_width = 0.0f;
    pixel_aspect_ratio = 0.0f;

    chunk_count = 0;

    // Tiled format
    tiled = 0;
    tile_size_x = 0;
    tile_size_y = 0;
    tile_level_mode = 0;
    tile_rounding_mode = 0;

    header_len = 0;
    compression_type = 0;

    name.clear();
    type.clear();
  }
};

static bool ReadChannelInfo(std::vector<ChannelInfo> &channels,
                            const std::vector<unsigned char> &data) {
  const char *p = reinterpret_cast<const char *>(&data.at(0));

  for (;;) {
    if ((*p) == 0) {
      break;
    }
    ChannelInfo info;
    info.requested_pixel_type = 0;

    tinyexr_int64 data_len = static_cast<tinyexr_int64>(data.size()) -
                             (p - reinterpret_cast<const char *>(data.data()));
    if (data_len < 0) {
      return false;
    }

    p = ReadString(&info.name, p, size_t(data_len));
    if ((p == NULL) && (info.name.empty())) {
      // Buffer overrun. Issue #51.
      return false;
    }

    const unsigned char *data_end =
        reinterpret_cast<const unsigned char *>(p) + 16;
    if (data_end >= (data.data() + data.size())) {
      return false;
    }

    memcpy(&info.pixel_type, p, sizeof(int));
    p += 4;
    info.p_linear = static_cast<unsigned char>(p[0]);  // uchar
    p += 1 + 3;                                        // reserved: uchar[3]
    memcpy(&info.x_sampling, p, sizeof(int));          // int
    p += 4;
    memcpy(&info.y_sampling, p, sizeof(int));  // int
    p += 4;

    tinyexr::swap4(&info.pixel_type);
    tinyexr::swap4(&info.x_sampling);
    tinyexr::swap4(&info.y_sampling);

    channels.push_back(info);
  }

  return true;
}

static void WriteChannelInfo(std::vector<unsigned char> &data,
                             const std::vector<ChannelInfo> &channels) {
  size_t sz = 0;

  // Calculate total size.
  for (size_t c = 0; c < channels.size(); c++) {
    sz += channels[c].name.length() + 1;  // +1 for \0
    sz += 16;                                    // 4 * int
  }
  data.resize(sz + 1);

  unsigned char *p = &data.at(0);

  for (size_t c = 0; c < channels.size(); c++) {
    memcpy(p, channels[c].name.c_str(), channels[c].name.length());
    p += channels[c].name.length();
    (*p) = '\0';
    p++;

    int pixel_type = channels[c].requested_pixel_type;
    int x_sampling = channels[c].x_sampling;
    int y_sampling = channels[c].y_sampling;
    tinyexr::swap4(&pixel_type);
    tinyexr::swap4(&x_sampling);
    tinyexr::swap4(&y_sampling);

    memcpy(p, &pixel_type, sizeof(int));
    p += sizeof(int);

    (*p) = channels[c].p_linear;
    p += 4;

    memcpy(p, &x_sampling, sizeof(int));
    p += sizeof(int);

    memcpy(p, &y_sampling, sizeof(int));
    p += sizeof(int);
  }

  (*p) = '\0';
}

static bool CompressZip(unsigned char *dst,
                        tinyexr::tinyexr_uint64 &compressedSize,
                        const unsigned char *src, unsigned long src_size) {
  std::vector<unsigned char> tmpBuf(src_size);

  //
  // Apply EXR-specific? postprocess. Grabbed from OpenEXR's
  // ImfZipCompressor.cpp
  //

  //
  // Reorder the pixel data.
  //

  const char *srcPtr = reinterpret_cast<const char *>(src);

  {
    char *t1 = reinterpret_cast<char *>(&tmpBuf.at(0));
    char *t2 = reinterpret_cast<char *>(&tmpBuf.at(0)) + (src_size + 1) / 2;
    const char *stop = srcPtr + src_size;

    for (;;) {
      if (srcPtr < stop)
        *(t1++) = *(srcPtr++);
      else
        break;

      if (srcPtr < stop)
        *(t2++) = *(srcPtr++);
      else
        break;
    }
  }

  //
  // Predictor.
  //

  {
    unsigned char *t = &tmpBuf.at(0) + 1;
    unsigned char *stop = &tmpBuf.at(0) + src_size;
    int p = t[-1];

    while (t < stop) {
      int d = int(t[0]) - p + (128 + 256);
      p = t[0];
      t[0] = static_cast<unsigned char>(d);
      ++t;
    }
  }

#if defined(TINYEXR_USE_MINIZ) && (TINYEXR_USE_MINIZ==1)
  //
  // Compress the data using miniz
  //

  mz_ulong outSize = mz_compressBound(src_size);
  int ret = mz_compress(
      dst, &outSize, static_cast<const unsigned char *>(&tmpBuf.at(0)),
      src_size);
  if (ret != MZ_OK) {
    return false;
  }

  compressedSize = outSize;
#elif defined(TINYEXR_USE_STB_ZLIB) && (TINYEXR_USE_STB_ZLIB==1)
  int outSize;
  unsigned char* ret = stbi_zlib_compress(const_cast<unsigned char*>(&tmpBuf.at(0)), src_size, &outSize, 8);
  if (!ret) {
    return false;
  }
  memcpy(dst, ret, outSize);
  free(ret);

  compressedSize = outSize;
#elif defined(TINYEXR_USE_NANOZLIB) && (TINYEXR_USE_NANOZLIB==1)
  uint64_t dstSize = nanoz_compressBound(static_cast<uint64_t>(src_size));
  int outSize{0};
  unsigned char *ret = nanoz_compress(&tmpBuf.at(0), src_size, &outSize, /* quality */8);
  if (!ret) {
    return false;
  }

  memcpy(dst, ret, outSize);
  free(ret);
  
  compressedSize = outSize;
#else
  uLong outSize = compressBound(static_cast<uLong>(src_size));
  int ret = compress(dst, &outSize, static_cast<const Bytef *>(&tmpBuf.at(0)),
                     src_size);
  if (ret != Z_OK) {
    return false;
  }

  compressedSize = outSize;
#endif

  // Use uncompressed data when compressed data is larger than uncompressed.
  // (Issue 40)
  if (compressedSize >= src_size) {
    compressedSize = src_size;
    memcpy(dst, src, src_size);
  }

  return true;
}

static bool DecompressZip(unsigned char *dst,
                          unsigned long *uncompressed_size /* inout */,
                          const unsigned char *src, unsigned long src_size) {
  if ((*uncompressed_size) == src_size) {
    // Data is not compressed(Issue 40).
    memcpy(dst, src, src_size);
    return true;
  }
  std::vector<unsigned char> tmpBuf(*uncompressed_size);

#if defined(TINYEXR_USE_MINIZ) && (TINYEXR_USE_MINIZ==1)
  int ret =
      mz_uncompress(&tmpBuf.at(0), uncompressed_size, src, src_size);
  if (MZ_OK != ret) {
    return false;
  }
#elif TINYEXR_USE_STB_ZLIB
  int ret = stbi_zlib_decode_buffer(reinterpret_cast<char*>(&tmpBuf.at(0)),
      *uncompressed_size, reinterpret_cast<const char*>(src), src_size);
  if (ret < 0) {
    return false;
  }
#elif defined(TINYEXR_USE_NANOZLIB) && (TINYEXR_USE_NANOZLIB==1)
  uint64_t dest_size = (*uncompressed_size);
  uint64_t uncomp_size{0};
  nanoz_status_t ret =
      nanoz_uncompress(src, src_size, dest_size, &tmpBuf.at(0), &uncomp_size);
  if (NANOZ_SUCCESS != ret) {
    return false;
  }
  if ((*uncompressed_size) != uncomp_size) {
    return false;
  }
#else
  int ret = uncompress(&tmpBuf.at(0), uncompressed_size, src, src_size);
  if (Z_OK != ret) {
    return false;
  }
#endif

  //
  // Apply EXR-specific? postprocess. Grabbed from OpenEXR's
  // ImfZipCompressor.cpp
  //

  // Predictor.
  {
    unsigned char *t = &tmpBuf.at(0) + 1;
    unsigned char *stop = &tmpBuf.at(0) + (*uncompressed_size);

    while (t < stop) {
      int d = int(t[-1]) + int(t[0]) - 128;
      t[0] = static_cast<unsigned char>(d);
      ++t;
    }
  }

  // Reorder the pixel data.
  {
    const char *t1 = reinterpret_cast<const char *>(&tmpBuf.at(0));
    const char *t2 = reinterpret_cast<const char *>(&tmpBuf.at(0)) +
                     (*uncompressed_size + 1) / 2;
    char *s = reinterpret_cast<char *>(dst);
    char *stop = s + (*uncompressed_size);

    for (;;) {
      if (s < stop)
        *(s++) = *(t1++);
      else
        break;

      if (s < stop)
        *(s++) = *(t2++);
      else
        break;
    }
  }

  return true;
}

// RLE code from OpenEXR --------------------------------------

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#if __has_warning("-Wextra-semi-stmt")
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4204)  // nonstandard extension used : non-constant
                                 // aggregate initializer (also supported by GNU
                                 // C and C99, so no big deal)
#pragma warning(disable : 4244)  // 'initializing': conversion from '__int64' to
                                 // 'int', possible loss of data
#pragma warning(disable : 4267)  // 'argument': conversion from '__int64' to
                                 // 'int', possible loss of data
#pragma warning(disable : 4996)  // 'strdup': The POSIX name for this item is
                                 // deprecated. Instead, use the ISO C and C++
                                 // conformant name: _strdup.
#endif

const int MIN_RUN_LENGTH = 3;
const int MAX_RUN_LENGTH = 127;

//
// Compress an array of bytes, using run-length encoding,
// and return the length of the compressed data.
//

static int rleCompress(int inLength, const char in[], signed char out[]) {
  const char *inEnd = in + inLength;
  const char *runStart = in;
  const char *runEnd = in + 1;
  signed char *outWrite = out;

  while (runStart < inEnd) {
    while (runEnd < inEnd && *runStart == *runEnd &&
           runEnd - runStart - 1 < MAX_RUN_LENGTH) {
      ++runEnd;
    }

    if (runEnd - runStart >= MIN_RUN_LENGTH) {
      //
      // Compressible run
      //

      *outWrite++ = static_cast<char>(runEnd - runStart) - 1;
      *outWrite++ = *(reinterpret_cast<const signed char *>(runStart));
      runStart = runEnd;
    } else {
      //
      // Uncompressable run
      //

      while (runEnd < inEnd &&
             ((runEnd + 1 >= inEnd || *runEnd != *(runEnd + 1)) ||
              (runEnd + 2 >= inEnd || *(runEnd + 1) != *(runEnd + 2))) &&
             runEnd - runStart < MAX_RUN_LENGTH) {
        ++runEnd;
      }

      *outWrite++ = static_cast<char>(runStart - runEnd);

      while (runStart < runEnd) {
        *outWrite++ = *(reinterpret_cast<const signed char *>(runStart++));
      }
    }

    ++runEnd;
  }

  return static_cast<int>(outWrite - out);
}

//
// Uncompress an array of bytes compressed with rleCompress().
// Returns the length of the uncompressed data, or 0 if the
// length of the uncompressed data would be more than maxLength.
//

static int rleUncompress(int inLength, int maxLength, const signed char in[],
                         char out[]) {
  char *outStart = out;

  while (inLength > 0) {
    if (*in < 0) {
      int count = -(static_cast<int>(*in++));
      inLength -= count + 1;

      // Fixes #116: Add bounds check to in buffer.
      if ((0 > (maxLength -= count)) || (inLength < 0)) return 0;

      memcpy(out, in, count);
      out += count;
      in += count;
    } else {
      int count = *in++;
      inLength -= 2;

      if ((0 > (maxLength -= count + 1)) || (inLength < 0)) return 0;

      memset(out, *reinterpret_cast<const char *>(in), count + 1);
      out += count + 1;

      in++;
    }
  }

  return static_cast<int>(out - outStart);
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

// End of RLE code from OpenEXR -----------------------------------

static bool CompressRle(unsigned char *dst,
                        tinyexr::tinyexr_uint64 &compressedSize,
                        const unsigned char *src, unsigned long src_size) {
  std::vector<unsigned char> tmpBuf(src_size);

  //
  // Apply EXR-specific? postprocess. Grabbed from OpenEXR's
  // ImfRleCompressor.cpp
  //

  //
  // Reorder the pixel data.
  //

  const char *srcPtr = reinterpret_cast<const char *>(src);

  {
    char *t1 = reinterpret_cast<char *>(&tmpBuf.at(0));
    char *t2 = reinterpret_cast<char *>(&tmpBuf.at(0)) + (src_size + 1) / 2;
    const char *stop = srcPtr + src_size;

    for (;;) {
      if (srcPtr < stop)
        *(t1++) = *(srcPtr++);
      else
        break;

      if (srcPtr < stop)
        *(t2++) = *(srcPtr++);
      else
        break;
    }
  }

  //
  // Predictor.
  //

  {
    unsigned char *t = &tmpBuf.at(0) + 1;
    unsigned char *stop = &tmpBuf.at(0) + src_size;
    int p = t[-1];

    while (t < stop) {
      int d = int(t[0]) - p + (128 + 256);
      p = t[0];
      t[0] = static_cast<unsigned char>(d);
      ++t;
    }
  }

  // outSize will be (srcSiz * 3) / 2 at max.
  int outSize = rleCompress(static_cast<int>(src_size),
                            reinterpret_cast<const char *>(&tmpBuf.at(0)),
                            reinterpret_cast<signed char *>(dst));
  TINYEXR_CHECK_AND_RETURN_C(outSize > 0, false);

  compressedSize = static_cast<tinyexr::tinyexr_uint64>(outSize);

  // Use uncompressed data when compressed data is larger than uncompressed.
  // (Issue 40)
  if (compressedSize >= src_size) {
    compressedSize = src_size;
    memcpy(dst, src, src_size);
  }

  return true;
}

static bool DecompressRle(unsigned char *dst,
                          const unsigned long uncompressed_size,
                          const unsigned char *src, unsigned long src_size) {
  if (uncompressed_size == src_size) {
    // Data is not compressed(Issue 40).
    memcpy(dst, src, src_size);
    return true;
  }

  // Workaround for issue #112.
  // TODO(syoyo): Add more robust out-of-bounds check in `rleUncompress`.
  if (src_size <= 2) {
    return false;
  }

  std::vector<unsigned char> tmpBuf(uncompressed_size);

  int ret = rleUncompress(static_cast<int>(src_size),
                          static_cast<int>(uncompressed_size),
                          reinterpret_cast<const signed char *>(src),
                          reinterpret_cast<char *>(&tmpBuf.at(0)));
  if (ret != static_cast<int>(uncompressed_size)) {
    return false;
  }

  //
  // Apply EXR-specific? postprocess. Grabbed from OpenEXR's
  // ImfRleCompressor.cpp
  //

  // Predictor.
  {
    unsigned char *t = &tmpBuf.at(0) + 1;
    unsigned char *stop = &tmpBuf.at(0) + uncompressed_size;

    while (t < stop) {
      int d = int(t[-1]) + int(t[0]) - 128;
      t[0] = static_cast<unsigned char>(d);
      ++t;
    }
  }

  // Reorder the pixel data.
  {
    const char *t1 = reinterpret_cast<const char *>(&tmpBuf.at(0));
    const char *t2 = reinterpret_cast<const char *>(&tmpBuf.at(0)) +
                     (uncompressed_size + 1) / 2;
    char *s = reinterpret_cast<char *>(dst);
    char *stop = s + uncompressed_size;

    for (;;) {
      if (s < stop)
        *(s++) = *(t1++);
      else
        break;

      if (s < stop)
        *(s++) = *(t2++);
      else
        break;
    }
  }

  return true;
}

#if TINYEXR_USE_PIZ

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++11-long-long"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wc++11-extensions"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"

#if __has_warning("-Wcast-qual")
#pragma clang diagnostic ignored "-Wcast-qual"
#endif

#if __has_warning("-Wextra-semi-stmt")
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#endif

#endif

//
// PIZ compress/uncompress, based on OpenEXR's ImfPizCompressor.cpp
//
// -----------------------------------------------------------------
// Copyright (c) 2004, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC)
// (3 clause BSD license)
//

struct PIZChannelData {
  unsigned short *start;
  unsigned short *end;
  int nx;
  int ny;
  int ys;
  int size;
};

//-----------------------------------------------------------------------------
//
//  16-bit Haar Wavelet encoding and decoding
//
//  The source code in this file is derived from the encoding
//  and decoding routines written by Christian Rouet for his
//  PIZ image file format.
//
//-----------------------------------------------------------------------------

//
// Wavelet basis functions without modulo arithmetic; they produce
// the best compression ratios when the wavelet-transformed data are
// Huffman-encoded, but the wavelet transform works only for 14-bit
// data (untransformed data values must be less than (1 << 14)).
//

inline void wenc14(unsigned short a, unsigned short b, unsigned short &l,
                   unsigned short &h) {
  short as = static_cast<short>(a);
  short bs = static_cast<short>(b);

  short ms = (as + bs) >> 1;
  short ds = as - bs;

  l = static_cast<unsigned short>(ms);
  h = static_cast<unsigned short>(ds);
}

inline void wdec14(unsigned short l, unsigned short h, unsigned short &a,
                   unsigned short &b) {
  short ls = static_cast<short>(l);
  short hs = static_cast<short>(h);

  int hi = hs;
  int ai = ls + (hi & 1) + (hi >> 1);

  short as = static_cast<short>(ai);
  short bs = static_cast<short>(ai - hi);

  a = static_cast<unsigned short>(as);
  b = static_cast<unsigned short>(bs);
}

//
// Wavelet basis functions with modulo arithmetic; they work with full
// 16-bit data, but Huffman-encoding the wavelet-transformed data doesn't
// compress the data quite as well.
//

const int NBITS = 16;
const int A_OFFSET = 1 << (NBITS - 1);
const int M_OFFSET = 1 << (NBITS - 1);
const int MOD_MASK = (1 << NBITS) - 1;

inline void wenc16(unsigned short a, unsigned short b, unsigned short &l,
                   unsigned short &h) {
  int ao = (a + A_OFFSET) & MOD_MASK;
  int m = ((ao + b) >> 1);
  int d = ao - b;

  if (d < 0) m = (m + M_OFFSET) & MOD_MASK;

  d &= MOD_MASK;

  l = static_cast<unsigned short>(m);
  h = static_cast<unsigned short>(d);
}

inline void wdec16(unsigned short l, unsigned short h, unsigned short &a,
                   unsigned short &b) {
  int m = l;
  int d = h;
  int bb = (m - (d >> 1)) & MOD_MASK;
  int aa = (d + bb - A_OFFSET) & MOD_MASK;
  b = static_cast<unsigned short>(bb);
  a = static_cast<unsigned short>(aa);
}

//
// 2D Wavelet encoding:
//

static void wav2Encode(
    unsigned short *in,  // io: values are transformed in place
    int nx,              // i : x size
    int ox,              // i : x offset
    int ny,              // i : y size
    int oy,              // i : y offset
    unsigned short mx)   // i : maximum in[x][y] value
{
  bool w14 = (mx < (1 << 14));
  int n = (nx > ny) ? ny : nx;
  int p = 1;   // == 1 <<  level
  int p2 = 2;  // == 1 << (level+1)

  //
  // Hierarchical loop on smaller dimension n
  //

  while (p2 <= n) {
    unsigned short *py = in;
    unsigned short *ey = in + oy * (ny - p2);
    int oy1 = oy * p;
    int oy2 = oy * p2;
    int ox1 = ox * p;
    int ox2 = ox * p2;
    unsigned short i00, i01, i10, i11;

    //
    // Y loop
    //

    for (; py <= ey; py += oy2) {
      unsigned short *px = py;
      unsigned short *ex = py + ox * (nx - p2);

      //
      // X loop
      //

      for (; px <= ex; px += ox2) {
        unsigned short *p01 = px + ox1;
        unsigned short *p10 = px + oy1;
        unsigned short *p11 = p10 + ox1;

        //
        // 2D wavelet encoding
        //

        if (w14) {
          wenc14(*px, *p01, i00, i01);
          wenc14(*p10, *p11, i10, i11);
          wenc14(i00, i10, *px, *p10);
          wenc14(i01, i11, *p01, *p11);
        } else {
          wenc16(*px, *p01, i00, i01);
          wenc16(*p10, *p11, i10, i11);
          wenc16(i00, i10, *px, *p10);
          wenc16(i01, i11, *p01, *p11);
        }
      }

      //
      // Encode (1D) odd column (still in Y loop)
      //

      if (nx & p) {
        unsigned short *p10 = px + oy1;

        if (w14)
          wenc14(*px, *p10, i00, *p10);
        else
          wenc16(*px, *p10, i00, *p10);

        *px = i00;
      }
    }

    //
    // Encode (1D) odd line (must loop in X)
    //

    if (ny & p) {
      unsigned short *px = py;
      unsigned short *ex = py + ox * (nx - p2);

      for (; px <= ex; px += ox2) {
        unsigned short *p01 = px + ox1;

        if (w14)
          wenc14(*px, *p01, i00, *p01);
        else
          wenc16(*px, *p01, i00, *p01);

        *px = i00;
      }
    }

    //
    // Next level
    //

    p = p2;
    p2 <<= 1;
  }
}

//
// 2D Wavelet decoding:
//

static void wav2Decode(
    unsigned short *in,  // io: values are transformed in place
    int nx,              // i : x size
    int ox,              // i : x offset
    int ny,              // i : y size
    int oy,              // i : y offset
    unsigned short mx)   // i : maximum in[x][y] value
{
  bool w14 = (mx < (1 << 14));
  int n = (nx > ny) ? ny : nx;
  int p = 1;
  int p2;

  //
  // Search max level
  //

  while (p <= n) p <<= 1;

  p >>= 1;
  p2 = p;
  p >>= 1;

  //
  // Hierarchical loop on smaller dimension n
  //

  while (p >= 1) {
    unsigned short *py = in;
    unsigned short *ey = in + oy * (ny - p2);
    int oy1 = oy * p;
    int oy2 = oy * p2;
    int ox1 = ox * p;
    int ox2 = ox * p2;
    unsigned short i00, i01, i10, i11;

    //
    // Y loop
    //

    for (; py <= ey; py += oy2) {
      unsigned short *px = py;
      unsigned short *ex = py + ox * (nx - p2);

      //
      // X loop
      //

      for (; px <= ex; px += ox2) {
        unsigned short *p01 = px + ox1;
        unsigned short *p10 = px + oy1;
        unsigned short *p11 = p10 + ox1;

        //
        // 2D wavelet decoding
        //

        if (w14) {
          wdec14(*px, *p10, i00, i10);
          wdec14(*p01, *p11, i01, i11);
          wdec14(i00, i01, *px, *p01);
          wdec14(i10, i11, *p10, *p11);
        } else {
          wdec16(*px, *p10, i00, i10);
          wdec16(*p01, *p11, i01, i11);
          wdec16(i00, i01, *px, *p01);
          wdec16(i10, i11, *p10, *p11);
        }
      }

      //
      // Decode (1D) odd column (still in Y loop)
      //

      if (nx & p) {
        unsigned short *p10 = px + oy1;

        if (w14)
          wdec14(*px, *p10, i00, *p10);
        else
          wdec16(*px, *p10, i00, *p10);

        *px = i00;
      }
    }

    //
    // Decode (1D) odd line (must loop in X)
    //

    if (ny & p) {
      unsigned short *px = py;
      unsigned short *ex = py + ox * (nx - p2);

      for (; px <= ex; px += ox2) {
        unsigned short *p01 = px + ox1;

        if (w14)
          wdec14(*px, *p01, i00, *p01);
        else
          wdec16(*px, *p01, i00, *p01);

        *px = i00;
      }
    }

    //
    // Next level
    //

    p2 = p;
    p >>= 1;
  }
}

//-----------------------------------------------------------------------------
//
//  16-bit Huffman compression and decompression.
//
//  The source code in this file is derived from the 8-bit
//  Huffman compression and decompression routines written
//  by Christian Rouet for his PIZ image file format.
//
//-----------------------------------------------------------------------------

// Adds some modification for tinyexr.

const int HUF_ENCBITS = 16;  // literal (value) bit length
const int HUF_DECBITS = 14;  // decoding bit size (>= 8)

const int HUF_ENCSIZE = (1 << HUF_ENCBITS) + 1;  // encoding table size
const int HUF_DECSIZE = 1 << HUF_DECBITS;        // decoding table size
const int HUF_DECMASK = HUF_DECSIZE - 1;

struct HufDec {  // short code    long code
  //-------------------------------
  unsigned int len : 8;   // code length    0
  unsigned int lit : 24;  // lit      p size
  unsigned int *p;        // 0      lits
};

inline long long hufLength(long long code) { return code & 63; }

inline long long hufCode(long long code) { return code >> 6; }

inline void outputBits(int nBits, long long bits, long long &c, int &lc,
                       char *&out) {
  c <<= nBits;
  lc += nBits;

  c |= bits;

  while (lc >= 8) *out++ = static_cast<char>((c >> (lc -= 8)));
}

inline long long getBits(int nBits, long long &c, int &lc, const char *&in) {
  while (lc < nBits) {
    c = (c << 8) | *(reinterpret_cast<const unsigned char *>(in++));
    lc += 8;
  }

  lc -= nBits;
  return (c >> lc) & ((1 << nBits) - 1);
}

//
// ENCODING TABLE BUILDING & (UN)PACKING
//

//
// Build a "canonical" Huffman code table:
//  - for each (uncompressed) symbol, hcode contains the length
//    of the corresponding code (in the compressed data)
//  - canonical codes are computed and stored in hcode
//  - the rules for constructing canonical codes are as follows:
//    * shorter codes (if filled with zeroes to the right)
//      have a numerically higher value than longer codes
//    * for codes with the same length, numerical values
//      increase with numerical symbol values
//  - because the canonical code table can be constructed from
//    symbol lengths alone, the code table can be transmitted
//    without sending the actual code values
//  - see http://www.compressconsult.com/huffman/
//

static void hufCanonicalCodeTable(long long hcode[HUF_ENCSIZE]) {
  long long n[59];

  //
  // For each i from 0 through 58, count the
  // number of different codes of length i, and
  // store the count in n[i].
  //

  for (int i = 0; i <= 58; ++i) n[i] = 0;

  for (int i = 0; i < HUF_ENCSIZE; ++i) n[hcode[i]] += 1;

  //
  // For each i from 58 through 1, compute the
  // numerically lowest code with length i, and
  // store that code in n[i].
  //

  long long c = 0;

  for (int i = 58; i > 0; --i) {
    long long nc = ((c + n[i]) >> 1);
    n[i] = c;
    c = nc;
  }

  //
  // hcode[i] contains the length, l, of the
  // code for symbol i.  Assign the next available
  // code of length l to the symbol and store both
  // l and the code in hcode[i].
  //

  for (int i = 0; i < HUF_ENCSIZE; ++i) {
    int l = static_cast<int>(hcode[i]);

    if (l > 0) hcode[i] = l | (n[l]++ << 6);
  }
}

//
// Compute Huffman codes (based on frq input) and store them in frq:
//  - code structure is : [63:lsb - 6:msb] | [5-0: bit length];
//  - max code length is 58 bits;
//  - codes outside the range [im-iM] have a null length (unused values);
//  - original frequencies are destroyed;
//  - encoding tables are used by hufEncode() and hufBuildDecTable();
//

struct FHeapCompare {
  bool operator()(long long *a, long long *b) { return *a > *b; }
};

static bool hufBuildEncTable(
    long long *frq,  // io: input frequencies [HUF_ENCSIZE], output table
    int *im,         //  o: min frq index
    int *iM)         //  o: max frq index
{
  //
  // This function assumes that when it is called, array frq
  // indicates the frequency of all possible symbols in the data
  // that are to be Huffman-encoded.  (frq[i] contains the number
  // of occurrences of symbol i in the data.)
  //
  // The loop below does three things:
  //
  // 1) Finds the minimum and maximum indices that point
  //    to non-zero entries in frq:
  //
  //     frq[im] != 0, and frq[i] == 0 for all i < im
  //     frq[iM] != 0, and frq[i] == 0 for all i > iM
  //
  // 2) Fills array fHeap with pointers to all non-zero
  //    entries in frq.
  //
  // 3) Initializes array hlink such that hlink[i] == i
  //    for all array entries.
  //

  std::vector<int> hlink(HUF_ENCSIZE);
  std::vector<long long *> fHeap(HUF_ENCSIZE);

  *im = 0;

  while (!frq[*im]) (*im)++;

  int nf = 0;

  for (int i = *im; i < HUF_ENCSIZE; i++) {
    hlink[i] = i;

    if (frq[i]) {
      fHeap[nf] = &frq[i];
      nf++;
      *iM = i;
    }
  }

  //
  // Add a pseudo-symbol, with a frequency count of 1, to frq;
  // adjust the fHeap and hlink array accordingly.  Function
  // hufEncode() uses the pseudo-symbol for run-length encoding.
  //

  (*iM)++;
  frq[*iM] = 1;
  fHeap[nf] = &frq[*iM];
  nf++;

  //
  // Build an array, scode, such that scode[i] contains the number
  // of bits assigned to symbol i.  Conceptually this is done by
  // constructing a tree whose leaves are the symbols with non-zero
  // frequency:
  //
  //     Make a heap that contains all symbols with a non-zero frequency,
  //     with the least frequent symbol on top.
  //
  //     Repeat until only one symbol is left on the heap:
  //
  //         Take the two least frequent symbols off the top of the heap.
  //         Create a new node that has first two nodes as children, and
  //         whose frequency is the sum of the frequencies of the first
  //         two nodes.  Put the new node back into the heap.
  //
  // The last node left on the heap is the root of the tree.  For each
  // leaf node, the distance between the root and the leaf is the length
  // of the code for the corresponding symbol.
  //
  // The loop below doesn't actually build the tree; instead we compute
  // the distances of the leaves from the root on the fly.  When a new
  // node is added to the heap, then that node's descendants are linked
  // into a single linear list that starts at the new node, and the code
  // lengths of the descendants (that is, their distance from the root
  // of the tree) are incremented by one.
  //

  std::make_heap(&fHeap[0], &fHeap[nf], FHeapCompare());

  std::vector<long long> scode(HUF_ENCSIZE);
  memset(scode.data(), 0, sizeof(long long) * HUF_ENCSIZE);

  while (nf > 1) {
    //
    // Find the indices, mm and m, of the two smallest non-zero frq
    // values in fHeap, add the smallest frq to the second-smallest
    // frq, and remove the smallest frq value from fHeap.
    //

    int mm = fHeap[0] - frq;
    std::pop_heap(&fHeap[0], &fHeap[nf], FHeapCompare());
    --nf;

    int m = fHeap[0] - frq;
    std::pop_heap(&fHeap[0], &fHeap[nf], FHeapCompare());

    frq[m] += frq[mm];
    std::push_heap(&fHeap[0], &fHeap[nf], FHeapCompare());

    //
    // The entries in scode are linked into lists with the
    // entries in hlink serving as "next" pointers and with
    // the end of a list marked by hlink[j] == j.
    //
    // Traverse the lists that start at scode[m] and scode[mm].
    // For each element visited, increment the length of the
    // corresponding code by one bit. (If we visit scode[j]
    // during the traversal, then the code for symbol j becomes
    // one bit longer.)
    //
    // Merge the lists that start at scode[m] and scode[mm]
    // into a single list that starts at scode[m].
    //

    //
    // Add a bit to all codes in the first list.
    //

    for (int j = m;; j = hlink[j]) {
      scode[j]++;

      TINYEXR_CHECK_AND_RETURN_C(scode[j] <= 58, false);

      if (hlink[j] == j) {
        //
        // Merge the two lists.
        //

        hlink[j] = mm;
        break;
      }
    }

    //
    // Add a bit to all codes in the second list
    //

    for (int j = mm;; j = hlink[j]) {
      scode[j]++;

      TINYEXR_CHECK_AND_RETURN_C(scode[j] <= 58, false);

      if (hlink[j] == j) break;
    }
  }

  //
  // Build a canonical Huffman code table, replacing the code
  // lengths in scode with (code, code length) pairs.  Copy the
  // code table from scode into frq.
  //

  hufCanonicalCodeTable(scode.data());
  memcpy(frq, scode.data(), sizeof(long long) * HUF_ENCSIZE);

  return true;
}

//
// Pack an encoding table:
//  - only code lengths, not actual codes, are stored
//  - runs of zeroes are compressed as follows:
//
//    unpacked    packed
//    --------------------------------
//    1 zero    0  (6 bits)
//    2 zeroes    59
//    3 zeroes    60
//    4 zeroes    61
//    5 zeroes    62
//    n zeroes (6 or more)  63 n-6  (6 + 8 bits)
//

const int SHORT_ZEROCODE_RUN = 59;
const int LONG_ZEROCODE_RUN = 63;
const int SHORTEST_LONG_RUN = 2 + LONG_ZEROCODE_RUN - SHORT_ZEROCODE_RUN;
const int LONGEST_LONG_RUN = 255 + SHORTEST_LONG_RUN;

static void hufPackEncTable(
    const long long *hcode,  // i : encoding table [HUF_ENCSIZE]
    int im,                  // i : min hcode index
    int iM,                  // i : max hcode index
    char **pcode)            //  o: ptr to packed table (updated)
{
  char *p = *pcode;
  long long c = 0;
  int lc = 0;

  for (; im <= iM; im++) {
    int l = hufLength(hcode[im]);

    if (l == 0) {
      int zerun = 1;

      while ((im < iM) && (zerun < LONGEST_LONG_RUN)) {
        if (hufLength(hcode[im + 1]) > 0) break;
        im++;
        zerun++;
      }

      if (zerun >= 2) {
        if (zerun >= SHORTEST_LONG_RUN) {
          outputBits(6, LONG_ZEROCODE_RUN, c, lc, p);
          outputBits(8, zerun - SHORTEST_LONG_RUN, c, lc, p);
        } else {
          outputBits(6, SHORT_ZEROCODE_RUN + zerun - 2, c, lc, p);
        }
        continue;
      }
    }

    outputBits(6, l, c, lc, p);
  }

  if (lc > 0) *p++ = (unsigned char)(c << (8 - lc));

  *pcode = p;
}

//
// Unpack an encoding table packed by hufPackEncTable():
//

static bool hufUnpackEncTable(
    const char **pcode,  // io: ptr to packed table (updated)
    int ni,              // i : input size (in bytes)
    int im,              // i : min hcode index
    int iM,              // i : max hcode index
    long long *hcode)    //  o: encoding table [HUF_ENCSIZE]
{
  memset(hcode, 0, sizeof(long long) * HUF_ENCSIZE);

  const char *p = *pcode;
  long long c = 0;
  int lc = 0;

  for (; im <= iM; im++) {
    if (p - *pcode >= ni) {
      return false;
    }

    long long l = hcode[im] = getBits(6, c, lc, p);  // code length

    if (l == (long long)LONG_ZEROCODE_RUN) {
      if (p - *pcode > ni) {
        return false;
      }

      int zerun = getBits(8, c, lc, p) + SHORTEST_LONG_RUN;

      if (im + zerun > iM + 1) {
        return false;
      }

      while (zerun--) hcode[im++] = 0;

      im--;
    } else if (l >= (long long)SHORT_ZEROCODE_RUN) {
      int zerun = l - SHORT_ZEROCODE_RUN + 2;

      if (im + zerun > iM + 1) {
        return false;
      }

      while (zerun--) hcode[im++] = 0;

      im--;
    }
  }

  *pcode = const_cast<char *>(p);

  hufCanonicalCodeTable(hcode);

  return true;
}

//
// DECODING TABLE BUILDING
//

//
// Clear a newly allocated decoding table so that it contains only zeroes.
//

static void hufClearDecTable(HufDec *hdecod)  // io: (allocated by caller)
//     decoding table [HUF_DECSIZE]
{
  for (int i = 0; i < HUF_DECSIZE; i++) {
    hdecod[i].len = 0;
    hdecod[i].lit = 0;
    hdecod[i].p = NULL;
  }
  // memset(hdecod, 0, sizeof(HufDec) * HUF_DECSIZE);
}

//
// Build a decoding hash table based on the encoding table hcode:
//  - short codes (<= HUF_DECBITS) are resolved with a single table access;
//  - long code entry allocations are not optimized, because long codes are
//    unfrequent;
//  - decoding tables are used by hufDecode();
//

static bool hufBuildDecTable(const long long *hcode,  // i : encoding table
                             int im,                  // i : min index in hcode
                             int iM,                  // i : max index in hcode
                             HufDec *hdecod)  //  o: (allocated by caller)
//     decoding table [HUF_DECSIZE]
{
  //
  // Init hashtable & loop on all codes.
  // Assumes that hufClearDecTable(hdecod) has already been called.
  //

  for (; im <= iM; im++) {
    long long c = hufCode(hcode[im]);
    int l = hufLength(hcode[im]);

    if (c >> l) {
      //
      // Error: c is supposed to be an l-bit code,
      // but c contains a value that is greater
      // than the largest l-bit number.
      //

      // invalidTableEntry();
      return false;
    }

    if (l > HUF_DECBITS) {
      //
      // Long code: add a secondary entry
      //

      HufDec *pl = hdecod + (c >> (l - HUF_DECBITS));

      if (pl->len) {
        //
        // Error: a short code has already
        // been stored in table entry *pl.
        //

        // invalidTableEntry();
        return false;
      }

      pl->lit++;

      if (pl->p) {
        unsigned int *p = pl->p;
        pl->p = new unsigned int[pl->lit];

        for (unsigned int i = 0; i < pl->lit - 1u; ++i) pl->p[i] = p[i];

        delete[] p;
      } else {
        pl->p = new unsigned int[1];
      }

      pl->p[pl->lit - 1] = im;
    } else if (l) {
      //
      // Short code: init all primary entries
      //

      HufDec *pl = hdecod + (c << (HUF_DECBITS - l));

      for (long long i = 1ULL << (HUF_DECBITS - l); i > 0; i--, pl++) {
        if (pl->len || pl->p) {
          //
          // Error: a short code or a long code has
          // already been stored in table entry *pl.
          //

          // invalidTableEntry();
          return false;
        }

        pl->len = l;
        pl->lit = im;
      }
    }
  }

  return true;
}

//
// Free the long code entries of a decoding table built by hufBuildDecTable()
//

static void hufFreeDecTable(HufDec *hdecod)  // io: Decoding table
{
  for (int i = 0; i < HUF_DECSIZE; i++) {
    if (hdecod[i].p) {
      delete[] hdecod[i].p;
      hdecod[i].p = 0;
    }
  }
}

//
// ENCODING
//

inline void outputCode(long long code, long long &c, int &lc, char *&out) {
  outputBits(hufLength(code), hufCode(code), c, lc, out);
}

inline void sendCode(long long sCode, int runCount, long long runCode,
                     long long &c, int &lc, char *&out) {
  //
  // Output a run of runCount instances of the symbol sCount.
  // Output the symbols explicitly, or if that is shorter, output
  // the sCode symbol once followed by a runCode symbol and runCount
  // expressed as an 8-bit number.
  //

  if (hufLength(sCode) + hufLength(runCode) + 8 < hufLength(sCode) * runCount) {
    outputCode(sCode, c, lc, out);
    outputCode(runCode, c, lc, out);
    outputBits(8, runCount, c, lc, out);
  } else {
    while (runCount-- >= 0) outputCode(sCode, c, lc, out);
  }
}

//
// Encode (compress) ni values based on the Huffman encoding table hcode:
//

static int hufEncode            // return: output size (in bits)
    (const long long *hcode,    // i : encoding table
     const unsigned short *in,  // i : uncompressed input buffer
     const int ni,              // i : input buffer size (in bytes)
     int rlc,                   // i : rl code
     char *out)                 //  o: compressed output buffer
{
  char *outStart = out;
  long long c = 0;  // bits not yet written to out
  int lc = 0;       // number of valid bits in c (LSB)
  int s = in[0];
  int cs = 0;

  //
  // Loop on input values
  //

  for (int i = 1; i < ni; i++) {
    //
    // Count same values or send code
    //

    if (s == in[i] && cs < 255) {
      cs++;
    } else {
      sendCode(hcode[s], cs, hcode[rlc], c, lc, out);
      cs = 0;
    }

    s = in[i];
  }

  //
  // Send remaining code
  //

  sendCode(hcode[s], cs, hcode[rlc], c, lc, out);

  if (lc) *out = (c << (8 - lc)) & 0xff;

  return (out - outStart) * 8 + lc;
}

//
// DECODING
//

//
// In order to force the compiler to inline them,
// getChar() and getCode() are implemented as macros
// instead of "inline" functions.
//

#define getChar(c, lc, in)                   \
  {                                          \
    c = (c << 8) | *(unsigned char *)(in++); \
    lc += 8;                                 \
  }

#if 0
#define getCode(po, rlc, c, lc, in, out, ob, oe) \
  {                                              \
    if (po == rlc) {                             \
      if (lc < 8) getChar(c, lc, in);            \
                                                 \
      lc -= 8;                                   \
                                                 \
      unsigned char cs = (c >> lc);              \
                                                 \
      if (out + cs > oe) return false;           \
                                                 \
      /* TinyEXR issue 78 */                     \
      unsigned short s = out[-1];                \
                                                 \
      while (cs-- > 0) *out++ = s;               \
    } else if (out < oe) {                       \
      *out++ = po;                               \
    } else {                                     \
      return false;                              \
    }                                            \
  }
#else
static bool getCode(int po, int rlc, long long &c, int &lc, const char *&in,
                    const char *in_end, unsigned short *&out,
                    const unsigned short *ob, const unsigned short *oe) {
  (void)ob;
  if (po == rlc) {
    if (lc < 8) {
      /* TinyEXR issue 78 */
      /* TinyEXR issue 160. in + 1 -> in */
      if (in >= in_end) {
        return false;
      }

      getChar(c, lc, in);
    }

    lc -= 8;

    unsigned char cs = (c >> lc);

    if (out + cs > oe) return false;

    // Bounds check for safety
    // Issue 100.
    if ((out - 1) < ob) return false;
    unsigned short s = out[-1];

    while (cs-- > 0) *out++ = s;
  } else if (out < oe) {
    *out++ = po;
  } else {
    return false;
  }
  return true;
}
#endif

//
// Decode (uncompress) ni bits based on encoding & decoding tables:
//

static bool hufDecode(const long long *hcode,  // i : encoding table
                      const HufDec *hdecod,    // i : decoding table
                      const char *in,          // i : compressed input buffer
                      int ni,                  // i : input size (in bits)
                      int rlc,                 // i : run-length code
                      int no,  // i : expected output size (in bytes)
                      unsigned short *out)  //  o: uncompressed output buffer
{
  long long c = 0;
  int lc = 0;
  unsigned short *outb = out;          // begin
  unsigned short *oe = out + no;       // end
  const char *ie = in + (ni + 7) / 8;  // input byte size

  //
  // Loop on input bytes
  //

  while (in < ie) {
    getChar(c, lc, in);

    //
    // Access decoding table
    //

    while (lc >= HUF_DECBITS) {
      const HufDec pl = hdecod[(c >> (lc - HUF_DECBITS)) & HUF_DECMASK];

      if (pl.len) {
        //
        // Get short code
        //

        lc -= pl.len;
        // std::cout << "lit = " << pl.lit << std::endl;
        // std::cout << "rlc = " << rlc << std::endl;
        // std::cout << "c = " << c << std::endl;
        // std::cout << "lc = " << lc << std::endl;
        // std::cout << "in = " << in << std::endl;
        // std::cout << "out = " << out << std::endl;
        // std::cout << "oe = " << oe << std::endl;
        if (!getCode(pl.lit, rlc, c, lc, in, ie, out, outb, oe)) {
          return false;
        }
      } else {
        if (!pl.p) {
          return false;
        }
        // invalidCode(); // wrong code

        //
        // Search long code
        //

        unsigned int j;

        for (j = 0; j < pl.lit; j++) {
          int l = hufLength(hcode[pl.p[j]]);

          while (lc < l && in < ie)  // get more bits
            getChar(c, lc, in);

          if (lc >= l) {
            if (hufCode(hcode[pl.p[j]]) ==
                ((c >> (lc - l)) & (((long long)(1) << l) - 1))) {
              //
              // Found : get long code
              //

              lc -= l;
              if (!getCode(pl.p[j], rlc, c, lc, in, ie, out, outb, oe)) {
                return false;
              }
              break;
            }
          }
        }

        if (j == pl.lit) {
          return false;
          // invalidCode(); // Not found
        }
      }
    }
  }

  //
  // Get remaining (short) codes
  //

  int i = (8 - ni) & 7;
  c >>= i;
  lc -= i;

  while (lc > 0) {
    const HufDec pl = hdecod[(c << (HUF_DECBITS - lc)) & HUF_DECMASK];

    if (pl.len) {
      lc -= pl.len;
      if (!getCode(pl.lit, rlc, c, lc, in, ie, out, outb, oe)) {
        return false;
      }
    } else {
      return false;
      // invalidCode(); // wrong (long) code
    }
  }

  if (out - outb != no) {
    return false;
  }
  // notEnoughData ();

  return true;
}

static void countFrequencies(std::vector<long long> &freq,
                             const unsigned short data[/*n*/], int n) {
  for (int i = 0; i < HUF_ENCSIZE; ++i) freq[i] = 0;

  for (int i = 0; i < n; ++i) ++freq[data[i]];
}

static void writeUInt(char buf[4], unsigned int i) {
  unsigned char *b = (unsigned char *)buf;

  b[0] = i;
  b[1] = i >> 8;
  b[2] = i >> 16;
  b[3] = i >> 24;
}

static unsigned int readUInt(const char buf[4]) {
  const unsigned char *b = (const unsigned char *)buf;

  return (b[0] & 0x000000ff) | ((b[1] << 8) & 0x0000ff00) |
         ((b[2] << 16) & 0x00ff0000) | ((b[3] << 24) & 0xff000000);
}

//
// EXTERNAL INTERFACE
//

static int hufCompress(const unsigned short raw[], int nRaw,
                       char compressed[]) {
  if (nRaw == 0) return 0;

  std::vector<long long> freq(HUF_ENCSIZE);

  countFrequencies(freq, raw, nRaw);

  int im = 0;
  int iM = 0;
  hufBuildEncTable(freq.data(), &im, &iM);

  char *tableStart = compressed + 20;
  char *tableEnd = tableStart;
  hufPackEncTable(freq.data(), im, iM, &tableEnd);
  int tableLength = tableEnd - tableStart;

  char *dataStart = tableEnd;
  int nBits = hufEncode(freq.data(), raw, nRaw, iM, dataStart);
  int data_length = (nBits + 7) / 8;

  writeUInt(compressed, im);
  writeUInt(compressed + 4, iM);
  writeUInt(compressed + 8, tableLength);
  writeUInt(compressed + 12, nBits);
  writeUInt(compressed + 16, 0);  // room for future extensions

  return dataStart + data_length - compressed;
}

static bool hufUncompress(const char compressed[], int nCompressed,
                          std::vector<unsigned short> *raw) {
  if (nCompressed == 0) {
    if (raw->size() != 0) return false;

    return false;
  }

  int im = readUInt(compressed);
  int iM = readUInt(compressed + 4);
  // int tableLength = readUInt (compressed + 8);
  int nBits = readUInt(compressed + 12);

  if (im < 0 || im >= HUF_ENCSIZE || iM < 0 || iM >= HUF_ENCSIZE) return false;

  const char *ptr = compressed + 20;

  //
  // Fast decoder needs at least 2x64-bits of compressed data, and
  // needs to be run-able on this platform. Otherwise, fall back
  // to the original decoder
  //

  // if (FastHufDecoder::enabled() && nBits > 128)
  //{
  //    FastHufDecoder fhd (ptr, nCompressed - (ptr - compressed), im, iM, iM);
  //    fhd.decode ((unsigned char*)ptr, nBits, raw, nRaw);
  //}
  // else
  {
    std::vector<long long> freq(HUF_ENCSIZE);
    std::vector<HufDec> hdec(HUF_DECSIZE);

    hufClearDecTable(&hdec.at(0));

    hufUnpackEncTable(&ptr, nCompressed - (ptr - compressed), im, iM,
                      &freq.at(0));

    {
      if (nBits > 8 * (nCompressed - (ptr - compressed))) {
        return false;
      }

      hufBuildDecTable(&freq.at(0), im, iM, &hdec.at(0));
      hufDecode(&freq.at(0), &hdec.at(0), ptr, nBits, iM, raw->size(),
                raw->data());
    }
    // catch (...)
    //{
    //    hufFreeDecTable (hdec);
    //    throw;
    //}

    hufFreeDecTable(&hdec.at(0));
  }

  return true;
}

//
// Functions to compress the range of values in the pixel data
//

const int USHORT_RANGE = (1 << 16);
const int BITMAP_SIZE = (USHORT_RANGE >> 3);

static void bitmapFromData(const unsigned short data[/*nData*/], int nData,
                           unsigned char bitmap[BITMAP_SIZE],
                           unsigned short &minNonZero,
                           unsigned short &maxNonZero) {
  for (int i = 0; i < BITMAP_SIZE; ++i) bitmap[i] = 0;

  for (int i = 0; i < nData; ++i) bitmap[data[i] >> 3] |= (1 << (data[i] & 7));

  bitmap[0] &= ~1;  // zero is not explicitly stored in
                    // the bitmap; we assume that the
                    // data always contain zeroes
  minNonZero = BITMAP_SIZE - 1;
  maxNonZero = 0;

  for (int i = 0; i < BITMAP_SIZE; ++i) {
    if (bitmap[i]) {
      if (minNonZero > i) minNonZero = i;
      if (maxNonZero < i) maxNonZero = i;
    }
  }
}

static unsigned short forwardLutFromBitmap(
    const unsigned char bitmap[BITMAP_SIZE], unsigned short lut[USHORT_RANGE]) {
  int k = 0;

  for (int i = 0; i < USHORT_RANGE; ++i) {
    if ((i == 0) || (bitmap[i >> 3] & (1 << (i & 7))))
      lut[i] = k++;
    else
      lut[i] = 0;
  }

  return k - 1;  // maximum value stored in lut[],
}  // i.e. number of ones in bitmap minus 1

static unsigned short reverseLutFromBitmap(
    const unsigned char bitmap[BITMAP_SIZE], unsigned short lut[USHORT_RANGE]) {
  int k = 0;

  for (int i = 0; i < USHORT_RANGE; ++i) {
    if ((i == 0) || (bitmap[i >> 3] & (1 << (i & 7)))) lut[k++] = i;
  }

  int n = k - 1;

  while (k < USHORT_RANGE) lut[k++] = 0;

  return n;  // maximum k where lut[k] is non-zero,
}  // i.e. number of ones in bitmap minus 1

static void applyLut(const unsigned short lut[USHORT_RANGE],
                     unsigned short data[/*nData*/], int nData) {
  for (int i = 0; i < nData; ++i) data[i] = lut[data[i]];
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif  // __clang__

#ifdef _MSC_VER
#pragma warning(pop)
#endif

static bool CompressPiz(unsigned char *outPtr, unsigned int *outSize,
                        const unsigned char *inPtr, size_t inSize,
                        const std::vector<ChannelInfo> &channelInfo,
                        int data_width, int num_lines) {
  std::vector<unsigned char> bitmap(BITMAP_SIZE);
  unsigned short minNonZero;
  unsigned short maxNonZero;

#if !TINYEXR_LITTLE_ENDIAN
  // @todo { PIZ compression on BigEndian architecture. }
  return false;
#endif

  // Assume `inSize` is multiple of 2 or 4.
  std::vector<unsigned short> tmpBuffer(inSize / sizeof(unsigned short));

  std::vector<PIZChannelData> channelData(channelInfo.size());
  unsigned short *tmpBufferEnd = &tmpBuffer.at(0);

  for (size_t c = 0; c < channelData.size(); c++) {
    PIZChannelData &cd = channelData[c];

    cd.start = tmpBufferEnd;
    cd.end = cd.start;

    cd.nx = data_width;
    cd.ny = num_lines;
    // cd.ys = c.channel().ySampling;

    size_t pixelSize = sizeof(int);  // UINT and FLOAT
    if (channelInfo[c].requested_pixel_type == TINYEXR_PIXELTYPE_HALF) {
      pixelSize = sizeof(short);
    }

    cd.size = static_cast<int>(pixelSize / sizeof(short));

    tmpBufferEnd += cd.nx * cd.ny * cd.size;
  }

  const unsigned char *ptr = inPtr;
  for (int y = 0; y < num_lines; ++y) {
    for (size_t i = 0; i < channelData.size(); ++i) {
      PIZChannelData &cd = channelData[i];

      // if (modp (y, cd.ys) != 0)
      //    continue;

      size_t n = static_cast<size_t>(cd.nx * cd.size);
      memcpy(cd.end, ptr, n * sizeof(unsigned short));
      ptr += n * sizeof(unsigned short);
      cd.end += n;
    }
  }

  bitmapFromData(&tmpBuffer.at(0), static_cast<int>(tmpBuffer.size()),
                 bitmap.data(), minNonZero, maxNonZero);

  std::vector<unsigned short> lut(USHORT_RANGE);
  unsigned short maxValue = forwardLutFromBitmap(bitmap.data(), lut.data());
  applyLut(lut.data(), &tmpBuffer.at(0), static_cast<int>(tmpBuffer.size()));

  //
  // Store range compression info in _outBuffer
  //

  char *buf = reinterpret_cast<char *>(outPtr);

  memcpy(buf, &minNonZero, sizeof(unsigned short));
  buf += sizeof(unsigned short);
  memcpy(buf, &maxNonZero, sizeof(unsigned short));
  buf += sizeof(unsigned short);

  if (minNonZero <= maxNonZero) {
    memcpy(buf, reinterpret_cast<char *>(&bitmap[0] + minNonZero),
           maxNonZero - minNonZero + 1);
    buf += maxNonZero - minNonZero + 1;
  }

  //
  // Apply wavelet encoding
  //

  for (size_t i = 0; i < channelData.size(); ++i) {
    PIZChannelData &cd = channelData[i];

    for (int j = 0; j < cd.size; ++j) {
      wav2Encode(cd.start + j, cd.nx, cd.size, cd.ny, cd.nx * cd.size,
                 maxValue);
    }
  }

  //
  // Apply Huffman encoding; append the result to _outBuffer
  //

  // length header(4byte), then huff data. Initialize length header with zero,
  // then later fill it by `length`.
  char *lengthPtr = buf;
  int zero = 0;
  memcpy(buf, &zero, sizeof(int));
  buf += sizeof(int);

  int length =
      hufCompress(&tmpBuffer.at(0), static_cast<int>(tmpBuffer.size()), buf);
  memcpy(lengthPtr, &length, sizeof(int));

  (*outSize) = static_cast<unsigned int>(
      (reinterpret_cast<unsigned char *>(buf) - outPtr) +
      static_cast<unsigned int>(length));

  // Use uncompressed data when compressed data is larger than uncompressed.
  // (Issue 40)
  if ((*outSize) >= inSize) {
    (*outSize) = static_cast<unsigned int>(inSize);
    memcpy(outPtr, inPtr, inSize);
  }
  return true;
}

static bool DecompressPiz(unsigned char *outPtr, const unsigned char *inPtr,
                          size_t tmpBufSizeInBytes, size_t inLen, int num_channels,
                          const EXRChannelInfo *channels, int data_width,
                          int num_lines) {
  if (inLen == tmpBufSizeInBytes) {
    // Data is not compressed(Issue 40).
    memcpy(outPtr, inPtr, inLen);
    return true;
  }

  std::vector<unsigned char> bitmap(BITMAP_SIZE);
  unsigned short minNonZero;
  unsigned short maxNonZero;

#if !TINYEXR_LITTLE_ENDIAN
  // @todo { PIZ compression on BigEndian architecture. }
  return false;
#endif

  memset(bitmap.data(), 0, BITMAP_SIZE);

  if (inLen < 4) {
    return false;
  }

  size_t readLen = 0;

  const unsigned char *ptr = inPtr;
  // minNonZero = *(reinterpret_cast<const unsigned short *>(ptr));
  tinyexr::cpy2(&minNonZero, reinterpret_cast<const unsigned short *>(ptr));
  // maxNonZero = *(reinterpret_cast<const unsigned short *>(ptr + 2));
  tinyexr::cpy2(&maxNonZero, reinterpret_cast<const unsigned short *>(ptr + 2));
  ptr += 4;
  readLen += 4;

  if (maxNonZero >= BITMAP_SIZE) {
    return false;
  }

  //printf("maxNonZero = %d\n", maxNonZero);
  //printf("minNonZero = %d\n", minNonZero);
  //printf("len = %d\n", (maxNonZero - minNonZero + 1));
  //printf("BITMAPSIZE - min = %d\n", (BITMAP_SIZE - minNonZero));

  if (minNonZero <= maxNonZero) {
    if (((maxNonZero - minNonZero + 1) + readLen) > inLen) {
      // Input too short
      return false;
    }

    memcpy(reinterpret_cast<char *>(&bitmap[0] + minNonZero), ptr,
           maxNonZero - minNonZero + 1);
    ptr += maxNonZero - minNonZero + 1;
    readLen += maxNonZero - minNonZero + 1;
  } else {
    // Issue 194
    if ((minNonZero == (BITMAP_SIZE - 1)) && (maxNonZero == 0)) {
      // OK. all pixels are zero. And no need to read `bitmap` data.
    } else {
      // invalid minNonZero/maxNonZero combination.
      return false;
    }
  }

  std::vector<unsigned short> lut(USHORT_RANGE);
  memset(lut.data(), 0, sizeof(unsigned short) * USHORT_RANGE);
  unsigned short maxValue = reverseLutFromBitmap(bitmap.data(), lut.data());

  //
  // Huffman decoding
  //

  if ((readLen + 4) > inLen) {
    return false;
  }

  int length=0;

  // length = *(reinterpret_cast<const int *>(ptr));
  tinyexr::cpy4(&length, reinterpret_cast<const int *>(ptr));
  ptr += sizeof(int);

  if (size_t((ptr - inPtr) + length) > inLen) {
    return false;
  }

  std::vector<unsigned short> tmpBuffer(tmpBufSizeInBytes / sizeof(unsigned short));
  hufUncompress(reinterpret_cast<const char *>(ptr), length, &tmpBuffer);

  //
  // Wavelet decoding
  //

  std::vector<PIZChannelData> channelData(static_cast<size_t>(num_channels));

  unsigned short *tmpBufferEnd = &tmpBuffer.at(0);

  for (size_t i = 0; i < static_cast<size_t>(num_channels); ++i) {
    const EXRChannelInfo &chan = channels[i];

    size_t pixelSize = sizeof(int);  // UINT and FLOAT
    if (chan.pixel_type == TINYEXR_PIXELTYPE_HALF) {
      pixelSize = sizeof(short);
    }

    channelData[i].start = tmpBufferEnd;
    channelData[i].end = channelData[i].start;
    channelData[i].nx = data_width;
    channelData[i].ny = num_lines;
    // channelData[i].ys = 1;
    channelData[i].size = static_cast<int>(pixelSize / sizeof(short));

    tmpBufferEnd += channelData[i].nx * channelData[i].ny * channelData[i].size;
  }

  for (size_t i = 0; i < channelData.size(); ++i) {
    PIZChannelData &cd = channelData[i];

    for (int j = 0; j < cd.size; ++j) {
      wav2Decode(cd.start + j, cd.nx, cd.size, cd.ny, cd.nx * cd.size,
                 maxValue);
    }
  }

  //
  // Expand the pixel data to their original range
  //

  applyLut(lut.data(), &tmpBuffer.at(0), static_cast<int>(tmpBufSizeInBytes / sizeof(unsigned short)));

  for (int y = 0; y < num_lines; y++) {
    for (size_t i = 0; i < channelData.size(); ++i) {
      PIZChannelData &cd = channelData[i];

      // if (modp (y, cd.ys) != 0)
      //    continue;

      size_t n = static_cast<size_t>(cd.nx * cd.size);
      memcpy(outPtr, cd.end, static_cast<size_t>(n * sizeof(unsigned short)));
      outPtr += n * sizeof(unsigned short);
      cd.end += n;
    }
  }

  return true;
}
#endif  // TINYEXR_USE_PIZ

#if TINYEXR_USE_ZFP

struct ZFPCompressionParam {
  double rate;
  unsigned int precision;
  unsigned int __pad0;
  double tolerance;
  int type;  // TINYEXR_ZFP_COMPRESSIONTYPE_*
  unsigned int __pad1;

  ZFPCompressionParam() {
    type = TINYEXR_ZFP_COMPRESSIONTYPE_RATE;
    rate = 2.0;
    precision = 0;
    tolerance = 0.0;
  }
};

static bool FindZFPCompressionParam(ZFPCompressionParam *param,
                                    const EXRAttribute *attributes,
                                    int num_attributes, std::string *err) {
  bool foundType = false;

  for (int i = 0; i < num_attributes; i++) {
    if ((strcmp(attributes[i].name, "zfpCompressionType") == 0)) {
      if (attributes[i].size == 1) {
        param->type = static_cast<int>(attributes[i].value[0]);
        foundType = true;
        break;
      } else {
        if (err) {
          (*err) +=
              "zfpCompressionType attribute must be uchar(1 byte) type.\n";
        }
        return false;
      }
    }
  }

  if (!foundType) {
    if (err) {
      (*err) += "`zfpCompressionType` attribute not found.\n";
    }
    return false;
  }

  if (param->type == TINYEXR_ZFP_COMPRESSIONTYPE_RATE) {
    for (int i = 0; i < num_attributes; i++) {
      if ((strcmp(attributes[i].name, "zfpCompressionRate") == 0) &&
          (attributes[i].size == 8)) {
        param->rate = *(reinterpret_cast<double *>(attributes[i].value));
        return true;
      }
    }

    if (err) {
      (*err) += "`zfpCompressionRate` attribute not found.\n";
    }

  } else if (param->type == TINYEXR_ZFP_COMPRESSIONTYPE_PRECISION) {
    for (int i = 0; i < num_attributes; i++) {
      if ((strcmp(attributes[i].name, "zfpCompressionPrecision") == 0) &&
          (attributes[i].size == 4)) {
        param->rate = *(reinterpret_cast<int *>(attributes[i].value));
        return true;
      }
    }

    if (err) {
      (*err) += "`zfpCompressionPrecision` attribute not found.\n";
    }

  } else if (param->type == TINYEXR_ZFP_COMPRESSIONTYPE_ACCURACY) {
    for (int i = 0; i < num_attributes; i++) {
      if ((strcmp(attributes[i].name, "zfpCompressionTolerance") == 0) &&
          (attributes[i].size == 8)) {
        param->tolerance = *(reinterpret_cast<double *>(attributes[i].value));
        return true;
      }
    }

    if (err) {
      (*err) += "`zfpCompressionTolerance` attribute not found.\n";
    }
  } else {
    if (err) {
      (*err) += "Unknown value specified for `zfpCompressionType`.\n";
    }
  }

  return false;
}

// Assume pixel format is FLOAT for all channels.
static bool DecompressZfp(float *dst, int dst_width, int dst_num_lines,
                          size_t num_channels, const unsigned char *src,
                          unsigned long src_size,
                          const ZFPCompressionParam &param) {
  size_t uncompressed_size =
      size_t(dst_width) * size_t(dst_num_lines) * num_channels;

  if (uncompressed_size == src_size) {
    // Data is not compressed(Issue 40).
    memcpy(dst, src, src_size);
  }

  zfp_stream *zfp = NULL;
  zfp_field *field = NULL;

  TINYEXR_CHECK_AND_RETURN_C((dst_width % 4) == 0, false);
  TINYEXR_CHECK_AND_RETURN_C((dst_num_lines % 4) == 0, false);

  if ((size_t(dst_width) & 3U) || (size_t(dst_num_lines) & 3U)) {
    return false;
  }

  field =
      zfp_field_2d(reinterpret_cast<void *>(const_cast<unsigned char *>(src)),
                   zfp_type_float, static_cast<unsigned int>(dst_width),
                   static_cast<unsigned int>(dst_num_lines) *
                       static_cast<unsigned int>(num_channels));
  zfp = zfp_stream_open(NULL);

  if (param.type == TINYEXR_ZFP_COMPRESSIONTYPE_RATE) {
    zfp_stream_set_rate(zfp, param.rate, zfp_type_float, /* dimension */ 2,
                        /* write random access */ 0);
  } else if (param.type == TINYEXR_ZFP_COMPRESSIONTYPE_PRECISION) {
    zfp_stream_set_precision(zfp, param.precision);
  } else if (param.type == TINYEXR_ZFP_COMPRESSIONTYPE_ACCURACY) {
    zfp_stream_set_accuracy(zfp, param.tolerance);
  } else {
    return false;
  }

  size_t buf_size = zfp_stream_maximum_size(zfp, field);
  std::vector<unsigned char> buf(buf_size);
  memcpy(&buf.at(0), src, src_size);

  bitstream *stream = stream_open(&buf.at(0), buf_size);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_stream_rewind(zfp);

  size_t image_size = size_t(dst_width) * size_t(dst_num_lines);

  for (size_t c = 0; c < size_t(num_channels); c++) {
    // decompress 4x4 pixel block.
    for (size_t y = 0; y < size_t(dst_num_lines); y += 4) {
      for (size_t x = 0; x < size_t(dst_width); x += 4) {
        float fblock[16];
        zfp_decode_block_float_2(zfp, fblock);
        for (size_t j = 0; j < 4; j++) {
          for (size_t i = 0; i < 4; i++) {
            dst[c * image_size + ((y + j) * size_t(dst_width) + (x + i))] =
                fblock[j * 4 + i];
          }
        }
      }
    }
  }

  zfp_field_free(field);
  zfp_stream_close(zfp);
  stream_close(stream);

  return true;
}

// Assume pixel format is FLOAT for all channels.
static bool CompressZfp(std::vector<unsigned char> *outBuf,
                        unsigned int *outSize, const float *inPtr, int width,
                        int num_lines, int num_channels,
                        const ZFPCompressionParam &param) {
  zfp_stream *zfp = NULL;
  zfp_field *field = NULL;

  TINYEXR_CHECK_AND_RETURN_C((width % 4) == 0, false);
  TINYEXR_CHECK_AND_RETURN_C((num_lines % 4) == 0, false);

  if ((size_t(width) & 3U) || (size_t(num_lines) & 3U)) {
    return false;
  }

  // create input array.
  field = zfp_field_2d(reinterpret_cast<void *>(const_cast<float *>(inPtr)),
                       zfp_type_float, static_cast<unsigned int>(width),
                       static_cast<unsigned int>(num_lines * num_channels));

  zfp = zfp_stream_open(NULL);

  if (param.type == TINYEXR_ZFP_COMPRESSIONTYPE_RATE) {
    zfp_stream_set_rate(zfp, param.rate, zfp_type_float, 2, 0);
  } else if (param.type == TINYEXR_ZFP_COMPRESSIONTYPE_PRECISION) {
    zfp_stream_set_precision(zfp, param.precision);
  } else if (param.type == TINYEXR_ZFP_COMPRESSIONTYPE_ACCURACY) {
    zfp_stream_set_accuracy(zfp, param.tolerance);
  } else {
    return false;
  }

  size_t buf_size = zfp_stream_maximum_size(zfp, field);

  outBuf->resize(buf_size);

  bitstream *stream = stream_open(&outBuf->at(0), buf_size);
  zfp_stream_set_bit_stream(zfp, stream);
  zfp_field_free(field);

  size_t image_size = size_t(width) * size_t(num_lines);

  for (size_t c = 0; c < size_t(num_channels); c++) {
    // compress 4x4 pixel block.
    for (size_t y = 0; y < size_t(num_lines); y += 4) {
      for (size_t x = 0; x < size_t(width); x += 4) {
        float fblock[16];
        for (size_t j = 0; j < 4; j++) {
          for (size_t i = 0; i < 4; i++) {
            fblock[j * 4 + i] =
                inPtr[c * image_size + ((y + j) * size_t(width) + (x + i))];
          }
        }
        zfp_encode_block_float_2(zfp, fblock);
      }
    }
  }

  zfp_stream_flush(zfp);
  (*outSize) = static_cast<unsigned int>(zfp_stream_compressed_size(zfp));

  zfp_stream_close(zfp);

  return true;
}

#endif

//
// -----------------------------------------------------------------
//

// heuristics
#define TINYEXR_DIMENSION_THRESHOLD (1024 * 8192)

// TODO(syoyo): Refactor function arguments.
static bool DecodePixelData(/* out */ unsigned char **out_images,
                            const int *requested_pixel_types,
                            const unsigned char *data_ptr, size_t data_len,
                            int compression_type, int line_order, int width,
                            int height, int x_stride, int y, int line_no,
                            int num_lines, size_t pixel_data_size,
                            size_t num_attributes,
                            const EXRAttribute *attributes, size_t num_channels,
                            const EXRChannelInfo *channels,
                            const std::vector<size_t> &channel_offset_list) {
  if (compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {  // PIZ
#if TINYEXR_USE_PIZ
    if ((width == 0) || (num_lines == 0) || (pixel_data_size == 0)) {
      // Invalid input #90
      return false;
    }

    // Allocate original data size.
    std::vector<unsigned char> outBuf(static_cast<size_t>(
        static_cast<size_t>(width * num_lines) * pixel_data_size));
    size_t tmpBufLen = outBuf.size();

    bool ret = tinyexr::DecompressPiz(
        reinterpret_cast<unsigned char *>(&outBuf.at(0)), data_ptr, tmpBufLen,
        data_len, static_cast<int>(num_channels), channels, width, num_lines);

    if (!ret) {
      return false;
    }

    // For PIZ_COMPRESSION:
    //   pixel sample data for channel 0 for scanline 0
    //   pixel sample data for channel 1 for scanline 0
    //   pixel sample data for channel ... for scanline 0
    //   pixel sample data for channel n for scanline 0
    //   pixel sample data for channel 0 for scanline 1
    //   pixel sample data for channel 1 for scanline 1
    //   pixel sample data for channel ... for scanline 1
    //   pixel sample data for channel n for scanline 1
    //   ...
    for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
      if (channels[c].pixel_type == TINYEXR_PIXELTYPE_HALF) {
        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const unsigned short *line_ptr = reinterpret_cast<unsigned short *>(
              &outBuf.at(v * pixel_data_size * static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            FP16 hf;

            // hf.u = line_ptr[u];
            // use `cpy` to avoid unaligned memory access when compiler's
            // optimization is on.
            tinyexr::cpy2(&(hf.u), line_ptr + u);

            tinyexr::swap2(reinterpret_cast<unsigned short *>(&hf.u));

            if (requested_pixel_types[c] == TINYEXR_PIXELTYPE_HALF) {
              unsigned short *image =
                  reinterpret_cast<unsigned short **>(out_images)[c];
              if (line_order == 0) {
                image += (static_cast<size_t>(line_no) + v) *
                             static_cast<size_t>(x_stride) +
                         u;
              } else {
                image += static_cast<size_t>(
                             (height - 1 - (line_no + static_cast<int>(v)))) *
                             static_cast<size_t>(x_stride) +
                         u;
              }
              *image = hf.u;
            } else {  // HALF -> FLOAT
              FP32 f32 = half_to_float(hf);
              float *image = reinterpret_cast<float **>(out_images)[c];
              size_t offset = 0;
              if (line_order == 0) {
                offset = (static_cast<size_t>(line_no) + v) *
                             static_cast<size_t>(x_stride) +
                         u;
              } else {
                offset = static_cast<size_t>(
                             (height - 1 - (line_no + static_cast<int>(v)))) *
                             static_cast<size_t>(x_stride) +
                         u;
              }
              image += offset;
              *image = f32.f;
            }
          }
        }
      } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_UINT) {
        TINYEXR_CHECK_AND_RETURN_C(requested_pixel_types[c] == TINYEXR_PIXELTYPE_UINT, false);

        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const unsigned int *line_ptr = reinterpret_cast<unsigned int *>(
              &outBuf.at(v * pixel_data_size * static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            unsigned int val;
            // val = line_ptr[u];
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(&val);

            unsigned int *image =
                reinterpret_cast<unsigned int **>(out_images)[c];
            if (line_order == 0) {
              image += (static_cast<size_t>(line_no) + v) *
                           static_cast<size_t>(x_stride) +
                       u;
            } else {
              image += static_cast<size_t>(
                           (height - 1 - (line_no + static_cast<int>(v)))) *
                           static_cast<size_t>(x_stride) +
                       u;
            }
            *image = val;
          }
        }
      } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
        TINYEXR_CHECK_AND_RETURN_C(requested_pixel_types[c] == TINYEXR_PIXELTYPE_FLOAT, false);
        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const float *line_ptr = reinterpret_cast<float *>(&outBuf.at(
              v * pixel_data_size * static_cast<size_t>(width) +
              channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            float val;
            // val = line_ptr[u];
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(reinterpret_cast<unsigned int *>(&val));

            float *image = reinterpret_cast<float **>(out_images)[c];
            if (line_order == 0) {
              image += (static_cast<size_t>(line_no) + v) *
                           static_cast<size_t>(x_stride) +
                       u;
            } else {
              image += static_cast<size_t>(
                           (height - 1 - (line_no + static_cast<int>(v)))) *
                           static_cast<size_t>(x_stride) +
                       u;
            }
            *image = val;
          }
        }
      } else {
        return false;
      }
    }
#else
    return false;
#endif

  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_ZIPS ||
             compression_type == TINYEXR_COMPRESSIONTYPE_ZIP) {
    // Allocate original data size.
    std::vector<unsigned char> outBuf(static_cast<size_t>(width) *
                                      static_cast<size_t>(num_lines) *
                                      pixel_data_size);

    unsigned long dstLen = static_cast<unsigned long>(outBuf.size());
    TINYEXR_CHECK_AND_RETURN_C(dstLen > 0, false);
    if (!tinyexr::DecompressZip(
            reinterpret_cast<unsigned char *>(&outBuf.at(0)), &dstLen, data_ptr,
            static_cast<unsigned long>(data_len))) {
      return false;
    }

    // For ZIP_COMPRESSION:
    //   pixel sample data for channel 0 for scanline 0
    //   pixel sample data for channel 1 for scanline 0
    //   pixel sample data for channel ... for scanline 0
    //   pixel sample data for channel n for scanline 0
    //   pixel sample data for channel 0 for scanline 1
    //   pixel sample data for channel 1 for scanline 1
    //   pixel sample data for channel ... for scanline 1
    //   pixel sample data for channel n for scanline 1
    //   ...
    for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
      if (channels[c].pixel_type == TINYEXR_PIXELTYPE_HALF) {
        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const unsigned short *line_ptr = reinterpret_cast<unsigned short *>(
              &outBuf.at(v * static_cast<size_t>(pixel_data_size) *
                             static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            tinyexr::FP16 hf;

            // hf.u = line_ptr[u];
            tinyexr::cpy2(&(hf.u), line_ptr + u);

            tinyexr::swap2(reinterpret_cast<unsigned short *>(&hf.u));

            if (requested_pixel_types[c] == TINYEXR_PIXELTYPE_HALF) {
              unsigned short *image =
                  reinterpret_cast<unsigned short **>(out_images)[c];
              if (line_order == 0) {
                image += (static_cast<size_t>(line_no) + v) *
                             static_cast<size_t>(x_stride) +
                         u;
              } else {
                image += (static_cast<size_t>(height) - 1U -
                          (static_cast<size_t>(line_no) + v)) *
                             static_cast<size_t>(x_stride) +
                         u;
              }
              *image = hf.u;
            } else {  // HALF -> FLOAT
              tinyexr::FP32 f32 = half_to_float(hf);
              float *image = reinterpret_cast<float **>(out_images)[c];
              size_t offset = 0;
              if (line_order == 0) {
                offset = (static_cast<size_t>(line_no) + v) *
                             static_cast<size_t>(x_stride) +
                         u;
              } else {
                offset = (static_cast<size_t>(height) - 1U -
                          (static_cast<size_t>(line_no) + v)) *
                             static_cast<size_t>(x_stride) +
                         u;
              }
              image += offset;

              *image = f32.f;
            }
          }
        }
      } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_UINT) {
        TINYEXR_CHECK_AND_RETURN_C(requested_pixel_types[c] == TINYEXR_PIXELTYPE_UINT, false);

        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const unsigned int *line_ptr = reinterpret_cast<unsigned int *>(
              &outBuf.at(v * pixel_data_size * static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            unsigned int val;
            // val = line_ptr[u];
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(&val);

            unsigned int *image =
                reinterpret_cast<unsigned int **>(out_images)[c];
            if (line_order == 0) {
              image += (static_cast<size_t>(line_no) + v) *
                           static_cast<size_t>(x_stride) +
                       u;
            } else {
              image += (static_cast<size_t>(height) - 1U -
                        (static_cast<size_t>(line_no) + v)) *
                           static_cast<size_t>(x_stride) +
                       u;
            }
            *image = val;
          }
        }
      } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
        TINYEXR_CHECK_AND_RETURN_C(requested_pixel_types[c] == TINYEXR_PIXELTYPE_FLOAT, false);
        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const float *line_ptr = reinterpret_cast<float *>(
              &outBuf.at(v * pixel_data_size * static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            float val;
            // val = line_ptr[u];
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(reinterpret_cast<unsigned int *>(&val));

            float *image = reinterpret_cast<float **>(out_images)[c];
            if (line_order == 0) {
              image += (static_cast<size_t>(line_no) + v) *
                           static_cast<size_t>(x_stride) +
                       u;
            } else {
              image += (static_cast<size_t>(height) - 1U -
                        (static_cast<size_t>(line_no) + v)) *
                           static_cast<size_t>(x_stride) +
                       u;
            }
            *image = val;
          }
        }
      } else {
        return false;
      }
    }
  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_RLE) {
    // Allocate original data size.
    std::vector<unsigned char> outBuf(static_cast<size_t>(width) *
                                      static_cast<size_t>(num_lines) *
                                      pixel_data_size);

    unsigned long dstLen = static_cast<unsigned long>(outBuf.size());
    if (dstLen == 0) {
      return false;
    }

    if (!tinyexr::DecompressRle(
            reinterpret_cast<unsigned char *>(&outBuf.at(0)), dstLen, data_ptr,
            static_cast<unsigned long>(data_len))) {
      return false;
    }

    // For RLE_COMPRESSION:
    //   pixel sample data for channel 0 for scanline 0
    //   pixel sample data for channel 1 for scanline 0
    //   pixel sample data for channel ... for scanline 0
    //   pixel sample data for channel n for scanline 0
    //   pixel sample data for channel 0 for scanline 1
    //   pixel sample data for channel 1 for scanline 1
    //   pixel sample data for channel ... for scanline 1
    //   pixel sample data for channel n for scanline 1
    //   ...
    for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
      if (channels[c].pixel_type == TINYEXR_PIXELTYPE_HALF) {
        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const unsigned short *line_ptr = reinterpret_cast<unsigned short *>(
              &outBuf.at(v * static_cast<size_t>(pixel_data_size) *
                             static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            tinyexr::FP16 hf;

            // hf.u = line_ptr[u];
            tinyexr::cpy2(&(hf.u), line_ptr + u);

            tinyexr::swap2(reinterpret_cast<unsigned short *>(&hf.u));

            if (requested_pixel_types[c] == TINYEXR_PIXELTYPE_HALF) {
              unsigned short *image =
                  reinterpret_cast<unsigned short **>(out_images)[c];
              if (line_order == 0) {
                image += (static_cast<size_t>(line_no) + v) *
                             static_cast<size_t>(x_stride) +
                         u;
              } else {
                image += (static_cast<size_t>(height) - 1U -
                          (static_cast<size_t>(line_no) + v)) *
                             static_cast<size_t>(x_stride) +
                         u;
              }
              *image = hf.u;
            } else {  // HALF -> FLOAT
              tinyexr::FP32 f32 = half_to_float(hf);
              float *image = reinterpret_cast<float **>(out_images)[c];
              if (line_order == 0) {
                image += (static_cast<size_t>(line_no) + v) *
                             static_cast<size_t>(x_stride) +
                         u;
              } else {
                image += (static_cast<size_t>(height) - 1U -
                          (static_cast<size_t>(line_no) + v)) *
                             static_cast<size_t>(x_stride) +
                         u;
              }
              *image = f32.f;
            }
          }
        }
      } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_UINT) {
        TINYEXR_CHECK_AND_RETURN_C(requested_pixel_types[c] == TINYEXR_PIXELTYPE_UINT, false);

        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const unsigned int *line_ptr = reinterpret_cast<unsigned int *>(
              &outBuf.at(v * pixel_data_size * static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            unsigned int val;
            // val = line_ptr[u];
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(&val);

            unsigned int *image =
                reinterpret_cast<unsigned int **>(out_images)[c];
            if (line_order == 0) {
              image += (static_cast<size_t>(line_no) + v) *
                           static_cast<size_t>(x_stride) +
                       u;
            } else {
              image += (static_cast<size_t>(height) - 1U -
                        (static_cast<size_t>(line_no) + v)) *
                           static_cast<size_t>(x_stride) +
                       u;
            }
            *image = val;
          }
        }
      } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
        TINYEXR_CHECK_AND_RETURN_C(requested_pixel_types[c] == TINYEXR_PIXELTYPE_FLOAT, false);
        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const float *line_ptr = reinterpret_cast<float *>(
              &outBuf.at(v * pixel_data_size * static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            float val;
            // val = line_ptr[u];
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(reinterpret_cast<unsigned int *>(&val));

            float *image = reinterpret_cast<float **>(out_images)[c];
            if (line_order == 0) {
              image += (static_cast<size_t>(line_no) + v) *
                           static_cast<size_t>(x_stride) +
                       u;
            } else {
              image += (static_cast<size_t>(height) - 1U -
                        (static_cast<size_t>(line_no) + v)) *
                           static_cast<size_t>(x_stride) +
                       u;
            }
            *image = val;
          }
        }
      } else {
        return false;
      }
    }
  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
#if TINYEXR_USE_ZFP
    tinyexr::ZFPCompressionParam zfp_compression_param;
    std::string e;
    if (!tinyexr::FindZFPCompressionParam(&zfp_compression_param, attributes,
                                          int(num_attributes), &e)) {
      // This code path should not be reachable.
      return false;
    }

    // Allocate original data size.
    std::vector<unsigned char> outBuf(static_cast<size_t>(width) *
                                      static_cast<size_t>(num_lines) *
                                      pixel_data_size);

    unsigned long dstLen = outBuf.size();
    TINYEXR_CHECK_AND_RETURN_C(dstLen > 0, false);
    tinyexr::DecompressZfp(reinterpret_cast<float *>(&outBuf.at(0)), width,
                           num_lines, num_channels, data_ptr,
                           static_cast<unsigned long>(data_len),
                           zfp_compression_param);

    // For ZFP_COMPRESSION:
    //   pixel sample data for channel 0 for scanline 0
    //   pixel sample data for channel 1 for scanline 0
    //   pixel sample data for channel ... for scanline 0
    //   pixel sample data for channel n for scanline 0
    //   pixel sample data for channel 0 for scanline 1
    //   pixel sample data for channel 1 for scanline 1
    //   pixel sample data for channel ... for scanline 1
    //   pixel sample data for channel n for scanline 1
    //   ...
    for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
      TINYEXR_CHECK_AND_RETURN_C(channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT, false);
      if (channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
        TINYEXR_CHECK_AND_RETURN_C(requested_pixel_types[c] == TINYEXR_PIXELTYPE_FLOAT, false);
        for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
          const float *line_ptr = reinterpret_cast<float *>(
              &outBuf.at(v * pixel_data_size * static_cast<size_t>(width) +
                         channel_offset_list[c] * static_cast<size_t>(width)));
          for (size_t u = 0; u < static_cast<size_t>(width); u++) {
            float val;
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(reinterpret_cast<unsigned int *>(&val));

            float *image = reinterpret_cast<float **>(out_images)[c];
            if (line_order == 0) {
              image += (static_cast<size_t>(line_no) + v) *
                           static_cast<size_t>(x_stride) +
                       u;
            } else {
              image += (static_cast<size_t>(height) - 1U -
                        (static_cast<size_t>(line_no) + v)) *
                           static_cast<size_t>(x_stride) +
                       u;
            }
            *image = val;
          }
        }
      } else {
        return false;
      }
    }
#else
    (void)attributes;
    (void)num_attributes;
    (void)num_channels;
    return false;
#endif
  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_NONE) {
    for (size_t c = 0; c < num_channels; c++) {
      for (size_t v = 0; v < static_cast<size_t>(num_lines); v++) {
        if (channels[c].pixel_type == TINYEXR_PIXELTYPE_HALF) {
          const unsigned short *line_ptr =
              reinterpret_cast<const unsigned short *>(
                  data_ptr + v * pixel_data_size * size_t(width) +
                  channel_offset_list[c] * static_cast<size_t>(width));

          if (requested_pixel_types[c] == TINYEXR_PIXELTYPE_HALF) {
            unsigned short *outLine =
                reinterpret_cast<unsigned short *>(out_images[c]);
            if (line_order == 0) {
              outLine += (size_t(y) + v) * size_t(x_stride);
            } else {
              outLine +=
                  (size_t(height) - 1 - (size_t(y) + v)) * size_t(x_stride);
            }

            for (int u = 0; u < width; u++) {
              tinyexr::FP16 hf;

              // hf.u = line_ptr[u];
              tinyexr::cpy2(&(hf.u), line_ptr + u);

              tinyexr::swap2(reinterpret_cast<unsigned short *>(&hf.u));

              outLine[u] = hf.u;
            }
          } else if (requested_pixel_types[c] == TINYEXR_PIXELTYPE_FLOAT) {
            float *outLine = reinterpret_cast<float *>(out_images[c]);
            if (line_order == 0) {
              outLine += (size_t(y) + v) * size_t(x_stride);
            } else {
              outLine +=
                  (size_t(height) - 1 - (size_t(y) + v)) * size_t(x_stride);
            }

            if (reinterpret_cast<const unsigned char *>(line_ptr + width) >
                (data_ptr + data_len)) {
              // Insufficient data size
              return false;
            }

            for (int u = 0; u < width; u++) {
              tinyexr::FP16 hf;

              // address may not be aligned. use byte-wise copy for safety.#76
              // hf.u = line_ptr[u];
              tinyexr::cpy2(&(hf.u), line_ptr + u);

              tinyexr::swap2(reinterpret_cast<unsigned short *>(&hf.u));

              tinyexr::FP32 f32 = half_to_float(hf);

              outLine[u] = f32.f;
            }
          } else {
            return false;
          }
        } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
          const float *line_ptr = reinterpret_cast<const float *>(
              data_ptr + v * pixel_data_size * size_t(width) +
              channel_offset_list[c] * static_cast<size_t>(width));

          float *outLine = reinterpret_cast<float *>(out_images[c]);
          if (line_order == 0) {
            outLine += (size_t(y) + v) * size_t(x_stride);
          } else {
            outLine +=
                (size_t(height) - 1 - (size_t(y) + v)) * size_t(x_stride);
          }

          if (reinterpret_cast<const unsigned char *>(line_ptr + width) >
              (data_ptr + data_len)) {
            // Insufficient data size
            return false;
          }

          for (int u = 0; u < width; u++) {
            float val;
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(reinterpret_cast<unsigned int *>(&val));

            outLine[u] = val;
          }
        } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_UINT) {
          const unsigned int *line_ptr = reinterpret_cast<const unsigned int *>(
              data_ptr + v * pixel_data_size * size_t(width) +
              channel_offset_list[c] * static_cast<size_t>(width));

          unsigned int *outLine =
              reinterpret_cast<unsigned int *>(out_images[c]);
          if (line_order == 0) {
            outLine += (size_t(y) + v) * size_t(x_stride);
          } else {
            outLine +=
                (size_t(height) - 1 - (size_t(y) + v)) * size_t(x_stride);
          }

          if (reinterpret_cast<const unsigned char *>(line_ptr + width) >
              (data_ptr + data_len)) {
            // Corrupted data
            return false;
          }

          for (int u = 0; u < width; u++) {

            unsigned int val;
            tinyexr::cpy4(&val, line_ptr + u);

            tinyexr::swap4(reinterpret_cast<unsigned int *>(&val));

            outLine[u] = val;
          }
        }
      }
    }
  }

  return true;
}

static bool DecodeTiledPixelData(
    unsigned char **out_images, int *width, int *height,
    const int *requested_pixel_types, const unsigned char *data_ptr,
    size_t data_len, int compression_type, int line_order, int data_width,
    int data_height, int tile_offset_x, int tile_offset_y, int tile_size_x,
    int tile_size_y, size_t pixel_data_size, size_t num_attributes,
    const EXRAttribute *attributes, size_t num_channels,
    const EXRChannelInfo *channels,
    const std::vector<size_t> &channel_offset_list) {
  // Here, data_width and data_height are the dimensions of the current (sub)level.
  if (tile_size_x * tile_offset_x > data_width ||
      tile_size_y * tile_offset_y > data_height) {
    return false;
  }

  // Compute actual image size in a tile.
  if ((tile_offset_x + 1) * tile_size_x >= data_width) {
    (*width) = data_width - (tile_offset_x * tile_size_x);
  } else {
    (*width) = tile_size_x;
  }

  if ((tile_offset_y + 1) * tile_size_y >= data_height) {
    (*height) = data_height - (tile_offset_y * tile_size_y);
  } else {
    (*height) = tile_size_y;
  }

  // Image size = tile size.
  return DecodePixelData(out_images, requested_pixel_types, data_ptr, data_len,
                         compression_type, line_order, (*width), tile_size_y,
                         /* stride */ tile_size_x, /* y */ 0, /* line_no */ 0,
                         (*height), pixel_data_size, num_attributes, attributes,
                         num_channels, channels, channel_offset_list);
}

static bool ComputeChannelLayout(std::vector<size_t> *channel_offset_list,
                                 int *pixel_data_size, size_t *channel_offset,
                                 int num_channels,
                                 const EXRChannelInfo *channels) {
  channel_offset_list->resize(static_cast<size_t>(num_channels));

  (*pixel_data_size) = 0;
  (*channel_offset) = 0;

  for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
    (*channel_offset_list)[c] = (*channel_offset);
    if (channels[c].pixel_type == TINYEXR_PIXELTYPE_HALF) {
      (*pixel_data_size) += sizeof(unsigned short);
      (*channel_offset) += sizeof(unsigned short);
    } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
      (*pixel_data_size) += sizeof(float);
      (*channel_offset) += sizeof(float);
    } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_UINT) {
      (*pixel_data_size) += sizeof(unsigned int);
      (*channel_offset) += sizeof(unsigned int);
    } else {
      // ???
      return false;
    }
  }
  return true;
}

// TODO: Simply return nullptr when failed to allocate?
static unsigned char **AllocateImage(int num_channels,
                                     const EXRChannelInfo *channels,
                                     const int *requested_pixel_types,
                                     int data_width, int data_height, bool *success) {
  unsigned char **images =
      reinterpret_cast<unsigned char **>(static_cast<float **>(
          malloc(sizeof(float *) * static_cast<size_t>(num_channels))));

  for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
    images[c] = NULL;
  }

  bool valid = true;

  for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
    size_t data_len =
        static_cast<size_t>(data_width) * static_cast<size_t>(data_height);
    if (channels[c].pixel_type == TINYEXR_PIXELTYPE_HALF) {
      // pixel_data_size += sizeof(unsigned short);
      // channel_offset += sizeof(unsigned short);
      // Alloc internal image for half type.
      if (requested_pixel_types[c] == TINYEXR_PIXELTYPE_HALF) {
        images[c] =
            reinterpret_cast<unsigned char *>(static_cast<unsigned short *>(
                malloc(sizeof(unsigned short) * data_len)));
      } else if (requested_pixel_types[c] == TINYEXR_PIXELTYPE_FLOAT) {
        images[c] = reinterpret_cast<unsigned char *>(
            static_cast<float *>(malloc(sizeof(float) * data_len)));
      } else {
        images[c] = NULL; // just in case.
        valid = false;
        break;
      }
    } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
      // pixel_data_size += sizeof(float);
      // channel_offset += sizeof(float);
      images[c] = reinterpret_cast<unsigned char *>(
          static_cast<float *>(malloc(sizeof(float) * data_len)));
    } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_UINT) {
      // pixel_data_size += sizeof(unsigned int);
      // channel_offset += sizeof(unsigned int);
      images[c] = reinterpret_cast<unsigned char *>(
          static_cast<unsigned int *>(malloc(sizeof(unsigned int) * data_len)));
    } else {
      images[c] = NULL; // just in case.
      valid = false;
      break;
    }
  }

  if (!valid) {
    for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
      if (images[c]) {
        free(images[c]);
        images[c] = NULL;
      }
    }

    if (success) {
      (*success) = false;
    }
  } else {
    if (success) {
      (*success) = true;
    }
  }

  return images;
}

#ifdef _WIN32
static inline std::wstring UTF8ToWchar(const std::string &str) {
  int wstr_size =
      MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
  std::wstring wstr(wstr_size, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0],
                      (int)wstr.size());
  return wstr;
}
#endif


static int ParseEXRHeader(HeaderInfo *info, bool *empty_header,
                          const EXRVersion *version, std::string *err,
                          const unsigned char *buf, size_t size) {
  const char *marker = reinterpret_cast<const char *>(&buf[0]);

  if (empty_header) {
    (*empty_header) = false;
  }

  if (version->multipart) {
    if (size > 0 && marker[0] == '\0') {
      // End of header list.
      if (empty_header) {
        (*empty_header) = true;
      }
      return TINYEXR_SUCCESS;
    }
  }

  // According to the spec, the header of every OpenEXR file must contain at
  // least the following attributes:
  //
  // channels chlist
  // compression compression
  // dataWindow box2i
  // displayWindow box2i
  // lineOrder lineOrder
  // pixelAspectRatio float
  // screenWindowCenter v2f
  // screenWindowWidth float
  bool has_channels = false;
  bool has_compression = false;
  bool has_data_window = false;
  bool has_display_window = false;
  bool has_line_order = false;
  bool has_pixel_aspect_ratio = false;
  bool has_screen_window_center = false;
  bool has_screen_window_width = false;
  bool has_name = false;
  bool has_type = false;

  info->name.clear();
  info->type.clear();

  info->data_window.min_x = 0;
  info->data_window.min_y = 0;
  info->data_window.max_x = 0;
  info->data_window.max_y = 0;
  info->line_order = 0;  // @fixme
  info->display_window.min_x = 0;
  info->display_window.min_y = 0;
  info->display_window.max_x = 0;
  info->display_window.max_y = 0;
  info->screen_window_center[0] = 0.0f;
  info->screen_window_center[1] = 0.0f;
  info->screen_window_width = -1.0f;
  info->pixel_aspect_ratio = -1.0f;

  info->tiled = 0;
  info->tile_size_x = -1;
  info->tile_size_y = -1;
  info->tile_level_mode = -1;
  info->tile_rounding_mode = -1;

  info->attributes.clear();

  // Read attributes
  size_t orig_size = size;
  for (size_t nattr = 0; nattr < TINYEXR_MAX_HEADER_ATTRIBUTES; nattr++) {
    if (0 == size) {
      if (err) {
        (*err) += "Insufficient data size for attributes.\n";
      }
      return TINYEXR_ERROR_INVALID_DATA;
    } else if (marker[0] == '\0') {
      size--;
      break;
    }

    std::string attr_name;
    std::string attr_type;
    std::vector<unsigned char> data;
    size_t marker_size;
    if (!tinyexr::ReadAttribute(&attr_name, &attr_type, &data, &marker_size,
                                marker, size)) {
      if (err) {
        (*err) += "Failed to read attribute.\n";
      }
      return TINYEXR_ERROR_INVALID_DATA;
    }
    marker += marker_size;
    size -= marker_size;

    // For a multipart file, the version field 9th bit is 0.
    if ((version->tiled || version->multipart || version->non_image) && attr_name.compare("tiles") == 0) {
      unsigned int x_size, y_size;
      unsigned char tile_mode;
      if (data.size() != 9) {
        if (err) {
          (*err) += "(ParseEXRHeader) Invalid attribute data size. Attribute data size must be 9.\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      memcpy(&x_size, &data.at(0), sizeof(int));
      memcpy(&y_size, &data.at(4), sizeof(int));
      tile_mode = data[8];
      tinyexr::swap4(&x_size);
      tinyexr::swap4(&y_size);

      if (x_size > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
          y_size > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        if (err) {
          (*err) = "Tile sizes were invalid.";
        }
        return TINYEXR_ERROR_UNSUPPORTED_FORMAT;
      }

      info->tile_size_x = static_cast<int>(x_size);
      info->tile_size_y = static_cast<int>(y_size);

      // mode = levelMode + roundingMode * 16
      info->tile_level_mode = tile_mode & 0x3;
      info->tile_rounding_mode = (tile_mode >> 4) & 0x1;
      info->tiled = 1;
    } else if (attr_name.compare("compression") == 0) {
      bool ok = false;
      if (data[0] < TINYEXR_COMPRESSIONTYPE_PIZ) {
        ok = true;
      }

      if (data[0] == TINYEXR_COMPRESSIONTYPE_PIZ) {
#if TINYEXR_USE_PIZ
        ok = true;
#else
        if (err) {
          (*err) = "PIZ compression is not supported.";
        }
        return TINYEXR_ERROR_UNSUPPORTED_FORMAT;
#endif
      }

      if (data[0] == TINYEXR_COMPRESSIONTYPE_ZFP) {
#if TINYEXR_USE_ZFP
        ok = true;
#else
        if (err) {
          (*err) = "ZFP compression is not supported.";
        }
        return TINYEXR_ERROR_UNSUPPORTED_FORMAT;
#endif
      }

      if (!ok) {
        if (err) {
          (*err) = "Unknown compression type.";
        }
        return TINYEXR_ERROR_UNSUPPORTED_FORMAT;
      }

      info->compression_type = static_cast<int>(data[0]);
      has_compression = true;

    } else if (attr_name.compare("channels") == 0) {
      // name: zero-terminated string, from 1 to 255 bytes long
      // pixel type: int, possible values are: UINT = 0 HALF = 1 FLOAT = 2
      // pLinear: unsigned char, possible values are 0 and 1
      // reserved: three chars, should be zero
      // xSampling: int
      // ySampling: int

      if (!ReadChannelInfo(info->channels, data)) {
        if (err) {
          (*err) += "Failed to parse channel info.\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      if (info->channels.size() < 1) {
        if (err) {
          (*err) += "# of channels is zero.\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      has_channels = true;

    } else if (attr_name.compare("dataWindow") == 0) {
      if (data.size() >= 16) {
        memcpy(&info->data_window.min_x, &data.at(0), sizeof(int));
        memcpy(&info->data_window.min_y, &data.at(4), sizeof(int));
        memcpy(&info->data_window.max_x, &data.at(8), sizeof(int));
        memcpy(&info->data_window.max_y, &data.at(12), sizeof(int));
        tinyexr::swap4(&info->data_window.min_x);
        tinyexr::swap4(&info->data_window.min_y);
        tinyexr::swap4(&info->data_window.max_x);
        tinyexr::swap4(&info->data_window.max_y);
        has_data_window = true;
      }
    } else if (attr_name.compare("displayWindow") == 0) {
      if (data.size() >= 16) {
        memcpy(&info->display_window.min_x, &data.at(0), sizeof(int));
        memcpy(&info->display_window.min_y, &data.at(4), sizeof(int));
        memcpy(&info->display_window.max_x, &data.at(8), sizeof(int));
        memcpy(&info->display_window.max_y, &data.at(12), sizeof(int));
        tinyexr::swap4(&info->display_window.min_x);
        tinyexr::swap4(&info->display_window.min_y);
        tinyexr::swap4(&info->display_window.max_x);
        tinyexr::swap4(&info->display_window.max_y);

        has_display_window = true;
      }
    } else if (attr_name.compare("lineOrder") == 0) {
      if (data.size() >= 1) {
        info->line_order = static_cast<int>(data[0]);
        has_line_order = true;
      }
    } else if (attr_name.compare("pixelAspectRatio") == 0) {
      if (data.size() >= sizeof(float)) {
        memcpy(&info->pixel_aspect_ratio, &data.at(0), sizeof(float));
        tinyexr::swap4(&info->pixel_aspect_ratio);
        has_pixel_aspect_ratio = true;
      }
    } else if (attr_name.compare("screenWindowCenter") == 0) {
      if (data.size() >= 8) {
        memcpy(&info->screen_window_center[0], &data.at(0), sizeof(float));
        memcpy(&info->screen_window_center[1], &data.at(4), sizeof(float));
        tinyexr::swap4(&info->screen_window_center[0]);
        tinyexr::swap4(&info->screen_window_center[1]);
        has_screen_window_center = true;
      }
    } else if (attr_name.compare("screenWindowWidth") == 0) {
      if (data.size() >= sizeof(float)) {
        memcpy(&info->screen_window_width, &data.at(0), sizeof(float));
        tinyexr::swap4(&info->screen_window_width);

        has_screen_window_width = true;
      }
    } else if (attr_name.compare("chunkCount") == 0) {
      if (data.size() >= sizeof(int)) {
        memcpy(&info->chunk_count, &data.at(0), sizeof(int));
        tinyexr::swap4(&info->chunk_count);
      }
    } else if (attr_name.compare("name") == 0) {
      if (!data.empty() && data[0]) {
        data.push_back(0);
        size_t len = strlen(reinterpret_cast<const char*>(&data[0]));
        info->name.resize(len);
        info->name.assign(reinterpret_cast<const char*>(&data[0]), len);
        has_name = true;
      }
    } else if (attr_name.compare("type") == 0) {
      if (!data.empty() && data[0]) {
        data.push_back(0);
        size_t len = strlen(reinterpret_cast<const char*>(&data[0]));
        info->type.resize(len);
        info->type.assign(reinterpret_cast<const char*>(&data[0]), len);
        has_type = true;
      }
    } else {
      // Custom attribute(up to TINYEXR_MAX_CUSTOM_ATTRIBUTES)
      if (info->attributes.size() < TINYEXR_MAX_CUSTOM_ATTRIBUTES) {
        EXRAttribute attrib;
#ifdef _MSC_VER
        strncpy_s(attrib.name, attr_name.c_str(), 255);
        strncpy_s(attrib.type, attr_type.c_str(), 255);
#else
        strncpy(attrib.name, attr_name.c_str(), 255);
        strncpy(attrib.type, attr_type.c_str(), 255);
#endif
        attrib.name[255] = '\0';
        attrib.type[255] = '\0';
        //std::cout << "i = " << info->attributes.size() << ", dsize = " << data.size() << "\n";
        attrib.size = static_cast<int>(data.size());
        attrib.value = static_cast<unsigned char *>(malloc(data.size()));
        memcpy(reinterpret_cast<char *>(attrib.value), &data.at(0),
               data.size());
        info->attributes.push_back(attrib);
      }
    }
  }

  // Check if required attributes exist
  {
    std::stringstream ss_err;

    if (!has_compression) {
      ss_err << "\"compression\" attribute not found in the header."
             << std::endl;
    }

    if (!has_channels) {
      ss_err << "\"channels\" attribute not found in the header." << std::endl;
    }

    if (!has_line_order) {
      ss_err << "\"lineOrder\" attribute not found in the header." << std::endl;
    }

    if (!has_display_window) {
      ss_err << "\"displayWindow\" attribute not found in the header."
             << std::endl;
    }

    if (!has_data_window) {
      ss_err << "\"dataWindow\" attribute not found in the header or invalid."
             << std::endl;
    }

    if (!has_pixel_aspect_ratio) {
      ss_err << "\"pixelAspectRatio\" attribute not found in the header."
             << std::endl;
    }

    if (!has_screen_window_width) {
      ss_err << "\"screenWindowWidth\" attribute not found in the header."
             << std::endl;
    }

    if (!has_screen_window_center) {
      ss_err << "\"screenWindowCenter\" attribute not found in the header."
             << std::endl;
    }

    if (version->multipart || version->non_image) {
      if (!has_name) {
        ss_err << "\"name\" attribute not found in the header."
          << std::endl;
      }
      if (!has_type) {
        ss_err << "\"type\" attribute not found in the header."
          << std::endl;
      }
    }

    if (!(ss_err.str().empty())) {
      if (err) {
        (*err) += ss_err.str();
      }

      return TINYEXR_ERROR_INVALID_HEADER;
    }
  }

  info->header_len = static_cast<unsigned int>(orig_size - size);

  return TINYEXR_SUCCESS;
}

// C++ HeaderInfo to C EXRHeader conversion.
static bool ConvertHeader(EXRHeader *exr_header, const HeaderInfo &info, std::string *warn, std::string *err) {
  exr_header->pixel_aspect_ratio = info.pixel_aspect_ratio;
  exr_header->screen_window_center[0] = info.screen_window_center[0];
  exr_header->screen_window_center[1] = info.screen_window_center[1];
  exr_header->screen_window_width = info.screen_window_width;
  exr_header->chunk_count = info.chunk_count;
  exr_header->display_window.min_x = info.display_window.min_x;
  exr_header->display_window.min_y = info.display_window.min_y;
  exr_header->display_window.max_x = info.display_window.max_x;
  exr_header->display_window.max_y = info.display_window.max_y;
  exr_header->data_window.min_x = info.data_window.min_x;
  exr_header->data_window.min_y = info.data_window.min_y;
  exr_header->data_window.max_x = info.data_window.max_x;
  exr_header->data_window.max_y = info.data_window.max_y;
  exr_header->line_order = info.line_order;
  exr_header->compression_type = info.compression_type;
  exr_header->tiled = info.tiled;
  exr_header->tile_size_x = info.tile_size_x;
  exr_header->tile_size_y = info.tile_size_y;
  exr_header->tile_level_mode = info.tile_level_mode;
  exr_header->tile_rounding_mode = info.tile_rounding_mode;

  EXRSetNameAttr(exr_header, info.name.c_str());


  if (!info.type.empty()) {
    bool valid = true;
    if (info.type == "scanlineimage") {
      if (exr_header->tiled) {
        if (err) {
          (*err) += "(ConvertHeader) tiled bit must be off for `scanlineimage` type.\n";
        }
        valid = false;
      }
    } else if (info.type == "tiledimage") {
      if (!exr_header->tiled) {
        if (err) {
          (*err) += "(ConvertHeader) tiled bit must be on for `tiledimage` type.\n";
        }
        valid = false;
      }
    } else if (info.type == "deeptile") {
      exr_header->non_image = 1;
      if (!exr_header->tiled) {
        if (err) {
          (*err) += "(ConvertHeader) tiled bit must be on for `deeptile` type.\n";
        }
        valid = false;
      }
    } else if (info.type == "deepscanline") {
      exr_header->non_image = 1;
      if (exr_header->tiled) {
        if (err) {
          (*err) += "(ConvertHeader) tiled bit must be off for `deepscanline` type.\n";
        }
        //valid = false;
      }
    } else {
      if (warn) {
        std::stringstream ss;
        ss << "(ConvertHeader) Unsupported or unknown info.type: " << info.type << "\n";
        (*warn) += ss.str();
      }
    }

    if (!valid) {
      return false;
    }
  }

  exr_header->num_channels = static_cast<int>(info.channels.size());

  exr_header->channels = static_cast<EXRChannelInfo *>(malloc(
      sizeof(EXRChannelInfo) * static_cast<size_t>(exr_header->num_channels)));
  for (size_t c = 0; c < static_cast<size_t>(exr_header->num_channels); c++) {
#ifdef _MSC_VER
    strncpy_s(exr_header->channels[c].name, info.channels[c].name.c_str(), 255);
#else
    strncpy(exr_header->channels[c].name, info.channels[c].name.c_str(), 255);
#endif
    // manually add '\0' for safety.
    exr_header->channels[c].name[255] = '\0';

    exr_header->channels[c].pixel_type = info.channels[c].pixel_type;
    exr_header->channels[c].p_linear = info.channels[c].p_linear;
    exr_header->channels[c].x_sampling = info.channels[c].x_sampling;
    exr_header->channels[c].y_sampling = info.channels[c].y_sampling;
  }

  exr_header->pixel_types = static_cast<int *>(
      malloc(sizeof(int) * static_cast<size_t>(exr_header->num_channels)));
  for (size_t c = 0; c < static_cast<size_t>(exr_header->num_channels); c++) {
    exr_header->pixel_types[c] = info.channels[c].pixel_type;
  }

  // Initially fill with values of `pixel_types`
  exr_header->requested_pixel_types = static_cast<int *>(
      malloc(sizeof(int) * static_cast<size_t>(exr_header->num_channels)));
  for (size_t c = 0; c < static_cast<size_t>(exr_header->num_channels); c++) {
    exr_header->requested_pixel_types[c] = info.channels[c].pixel_type;
  }

  exr_header->num_custom_attributes = static_cast<int>(info.attributes.size());

  if (exr_header->num_custom_attributes > 0) {
    // TODO(syoyo): Report warning when # of attributes exceeds
    // `TINYEXR_MAX_CUSTOM_ATTRIBUTES`
    if (exr_header->num_custom_attributes > TINYEXR_MAX_CUSTOM_ATTRIBUTES) {
      exr_header->num_custom_attributes = TINYEXR_MAX_CUSTOM_ATTRIBUTES;
    }

    exr_header->custom_attributes = static_cast<EXRAttribute *>(malloc(
        sizeof(EXRAttribute) * size_t(exr_header->num_custom_attributes)));

    for (size_t i = 0; i < size_t(exr_header->num_custom_attributes); i++) {
      memcpy(exr_header->custom_attributes[i].name, info.attributes[i].name,
             256);
      memcpy(exr_header->custom_attributes[i].type, info.attributes[i].type,
             256);
      exr_header->custom_attributes[i].size = info.attributes[i].size;
      // Just copy pointer
      exr_header->custom_attributes[i].value = info.attributes[i].value;
    }

  } else {
    exr_header->custom_attributes = NULL;
  }

  exr_header->header_len = info.header_len;

  return true;
}

struct OffsetData {
  OffsetData() : num_x_levels(0), num_y_levels(0) {}
  std::vector<std::vector<std::vector <tinyexr::tinyexr_uint64> > > offsets;
  int num_x_levels;
  int num_y_levels;
};

// -1 = error
static int LevelIndex(int lx, int ly, int tile_level_mode, int num_x_levels) {
  switch (tile_level_mode) {
  case TINYEXR_TILE_ONE_LEVEL:
    return 0;

  case TINYEXR_TILE_MIPMAP_LEVELS:
    return lx;

  case TINYEXR_TILE_RIPMAP_LEVELS:
    return lx + ly * num_x_levels;

  default:
    return -1;
  }
  return 0;
}

static int LevelSize(int toplevel_size, int level, int tile_rounding_mode) {
  if (level < 0) {
    return -1;
  }

  int b = static_cast<int>(1u << static_cast<unsigned int>(level));
  int level_size = toplevel_size / b;

  if (tile_rounding_mode == TINYEXR_TILE_ROUND_UP && level_size * b < toplevel_size)
    level_size += 1;

  return std::max(level_size, 1);
}

static int DecodeTiledLevel(EXRImage* exr_image, const EXRHeader* exr_header,
  const OffsetData& offset_data,
  const std::vector<size_t>& channel_offset_list,
  int pixel_data_size,
  const unsigned char* head, const size_t size,
  std::string* err) {
  int num_channels = exr_header->num_channels;

  int level_index = LevelIndex(exr_image->level_x, exr_image->level_y, exr_header->tile_level_mode, offset_data.num_x_levels);
  int num_y_tiles = int(offset_data.offsets[size_t(level_index)].size());
  if (num_y_tiles < 1) {
    return TINYEXR_ERROR_INVALID_DATA;
  }
  int num_x_tiles = int(offset_data.offsets[size_t(level_index)][0].size());
  if (num_x_tiles < 1) {
    return TINYEXR_ERROR_INVALID_DATA;
  }
  int num_tiles = num_x_tiles * num_y_tiles;

  int err_code = TINYEXR_SUCCESS;

  enum {
    EF_SUCCESS = 0,
    EF_INVALID_DATA = 1,
    EF_INSUFFICIENT_DATA = 2,
    EF_FAILED_TO_DECODE = 4
  };
#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
  std::atomic<unsigned> error_flag(EF_SUCCESS);
#else
  unsigned error_flag(EF_SUCCESS);
#endif

  // Although the spec says : "...the data window is subdivided into an array of smaller rectangles...",
  // the IlmImf library allows the dimensions of the tile to be larger (or equal) than the dimensions of the data window.
#if 0
  if ((exr_header->tile_size_x > exr_image->width || exr_header->tile_size_y > exr_image->height) &&
    exr_image->level_x == 0 && exr_image->level_y == 0) {
    if (err) {
      (*err) += "Failed to decode tile data.\n";
    }
    err_code = TINYEXR_ERROR_INVALID_DATA;
  }
#endif
  exr_image->tiles = static_cast<EXRTile*>(
    calloc(static_cast<size_t>(num_tiles), sizeof(EXRTile)));

#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
  std::vector<std::thread> workers;
  std::atomic<int> tile_count(0);

  int num_threads = std::max(1, int(std::thread::hardware_concurrency()));
  if (num_threads > int(num_tiles)) {
    num_threads = int(num_tiles);
  }

  for (int t = 0; t < num_threads; t++) {
    workers.emplace_back(std::thread([&]()
      {
        int tile_idx = 0;
        while ((tile_idx = tile_count++) < num_tiles) {

#else
#if TINYEXR_USE_OPENMP
#pragma omp parallel for
#endif
  for (int tile_idx = 0; tile_idx < num_tiles; tile_idx++) {
#endif
    // Allocate memory for each tile.
    bool alloc_success = false;
    exr_image->tiles[tile_idx].images = tinyexr::AllocateImage(
      num_channels, exr_header->channels,
      exr_header->requested_pixel_types, exr_header->tile_size_x,
      exr_header->tile_size_y, &alloc_success);

    if (!alloc_success) {
      error_flag |= EF_INVALID_DATA;
      continue;
    }

    int x_tile = tile_idx % num_x_tiles;
    int y_tile = tile_idx / num_x_tiles;
    // 16 byte: tile coordinates
    // 4 byte : data size
    // ~      : data(uncompressed or compressed)
    tinyexr::tinyexr_uint64 offset = offset_data.offsets[size_t(level_index)][size_t(y_tile)][size_t(x_tile)];
    if (offset + sizeof(int) * 5 > size) {
      // Insufficient data size.
      error_flag |= EF_INSUFFICIENT_DATA;
      continue;
    }

    size_t data_size =
      size_t(size - (offset + sizeof(int) * 5));
    const unsigned char* data_ptr =
      reinterpret_cast<const unsigned char*>(head + offset);

    int tile_coordinates[4];
    memcpy(tile_coordinates, data_ptr, sizeof(int) * 4);
    tinyexr::swap4(&tile_coordinates[0]);
    tinyexr::swap4(&tile_coordinates[1]);
    tinyexr::swap4(&tile_coordinates[2]);
    tinyexr::swap4(&tile_coordinates[3]);

    if (tile_coordinates[2] != exr_image->level_x) {
      // Invalid data.
      error_flag |= EF_INVALID_DATA;
      continue;
    }
    if (tile_coordinates[3] != exr_image->level_y) {
      // Invalid data.
      error_flag |= EF_INVALID_DATA;
      continue;
    }

    int data_len;
    memcpy(&data_len, data_ptr + 16,
      sizeof(int));  // 16 = sizeof(tile_coordinates)
    tinyexr::swap4(&data_len);

    if (data_len < 2 || size_t(data_len) > data_size) {
      // Insufficient data size.
      error_flag |= EF_INSUFFICIENT_DATA;
      continue;
    }

    // Move to data addr: 20 = 16 + 4;
    data_ptr += 20;
    bool ret = tinyexr::DecodeTiledPixelData(
      exr_image->tiles[tile_idx].images,
      &(exr_image->tiles[tile_idx].width),
      &(exr_image->tiles[tile_idx].height),
      exr_header->requested_pixel_types, data_ptr,
      static_cast<size_t>(data_len), exr_header->compression_type,
      exr_header->line_order,
      exr_image->width, exr_image->height,
      tile_coordinates[0], tile_coordinates[1], exr_header->tile_size_x,
      exr_header->tile_size_y, static_cast<size_t>(pixel_data_size),
      static_cast<size_t>(exr_header->num_custom_attributes),
      exr_header->custom_attributes,
      static_cast<size_t>(exr_header->num_channels),
      exr_header->channels, channel_offset_list);

    if (!ret) {
      // Failed to decode tile data.
      error_flag |= EF_FAILED_TO_DECODE;
    }

    exr_image->tiles[tile_idx].offset_x = tile_coordinates[0];
    exr_image->tiles[tile_idx].offset_y = tile_coordinates[1];
    exr_image->tiles[tile_idx].level_x = tile_coordinates[2];
    exr_image->tiles[tile_idx].level_y = tile_coordinates[3];

#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
  }
        }));
    }  // num_thread loop

    for (auto& t : workers) {
      t.join();
    }

#else
  } // parallel for
#endif

  // Even in the event of an error, the reserved memory may be freed.
  exr_image->num_channels = num_channels;
  exr_image->num_tiles = static_cast<int>(num_tiles);

  if (error_flag)  err_code = TINYEXR_ERROR_INVALID_DATA;
  if (err) {
    if (error_flag & EF_INSUFFICIENT_DATA) {
      (*err) += "Insufficient data length.\n";
    }
    if (error_flag & EF_FAILED_TO_DECODE) {
      (*err) += "Failed to decode tile data.\n";
    }
  }
  return err_code;
}

static int DecodeChunk(EXRImage *exr_image, const EXRHeader *exr_header,
                       const OffsetData& offset_data,
                       const unsigned char *head, const size_t size,
                       std::string *err) {
  int num_channels = exr_header->num_channels;

  int num_scanline_blocks = 1;
  if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_ZIP) {
    num_scanline_blocks = 16;
  } else if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {
    num_scanline_blocks = 32;
  } else if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
    num_scanline_blocks = 16;

#if TINYEXR_USE_ZFP
    tinyexr::ZFPCompressionParam zfp_compression_param;
    if (!FindZFPCompressionParam(&zfp_compression_param,
                                 exr_header->custom_attributes,
                                 int(exr_header->num_custom_attributes), err)) {
      return TINYEXR_ERROR_INVALID_HEADER;
    }
#endif
  }

  if (exr_header->data_window.max_x < exr_header->data_window.min_x ||
      exr_header->data_window.max_y < exr_header->data_window.min_y) {
    if (err) {
      (*err) += "Invalid data window.\n";
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }

  tinyexr_int64 data_width =
      static_cast<tinyexr_int64>(exr_header->data_window.max_x) - static_cast<tinyexr_int64>(exr_header->data_window.min_x) + static_cast<tinyexr_int64>(1);
  tinyexr_int64 data_height =
      static_cast<tinyexr_int64>(exr_header->data_window.max_y) - static_cast<tinyexr_int64>(exr_header->data_window.min_y) + static_cast<tinyexr_int64>(1);

  if (data_width <= 0) {
    if (err) {
      (*err) += "Invalid data window width.\n";
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }

  if (data_height <= 0) {
    if (err) {
      (*err) += "Invalid data window height.\n";
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }

  // Do not allow too large data_width and data_height. header invalid?
  {
    if ((data_width > TINYEXR_DIMENSION_THRESHOLD) || (data_height > TINYEXR_DIMENSION_THRESHOLD)) {
      if (err) {
        std::stringstream ss;
        ss << "data_with or data_height too large. data_width: " << data_width
           << ", "
           << "data_height = " << data_height << std::endl;
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_DATA;
    }
    if (exr_header->tiled) {
      if ((exr_header->tile_size_x > TINYEXR_DIMENSION_THRESHOLD) || (exr_header->tile_size_y > TINYEXR_DIMENSION_THRESHOLD)) {
        if (err) {
          std::stringstream ss;
          ss << "tile with or tile height too large. tile width: " << exr_header->tile_size_x
            << ", "
            << "tile height = " << exr_header->tile_size_y << std::endl;
          (*err) += ss.str();
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }
    }
  }

  const std::vector<tinyexr::tinyexr_uint64>& offsets = offset_data.offsets[0][0];
  size_t num_blocks = offsets.size();

  std::vector<size_t> channel_offset_list;
  int pixel_data_size = 0;
  size_t channel_offset = 0;
  if (!tinyexr::ComputeChannelLayout(&channel_offset_list, &pixel_data_size,
                                     &channel_offset, num_channels,
                                     exr_header->channels)) {
    if (err) {
      (*err) += "Failed to compute channel layout.\n";
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }

#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
  std::atomic<bool> invalid_data(false);
#else
  bool invalid_data(false);
#endif

  if (exr_header->tiled) {
    // value check
    if (exr_header->tile_size_x < 0) {
      if (err) {
        std::stringstream ss;
        ss << "Invalid tile size x : " << exr_header->tile_size_x << "\n";
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_HEADER;
    }

    if (exr_header->tile_size_y < 0) {
      if (err) {
        std::stringstream ss;
        ss << "Invalid tile size y : " << exr_header->tile_size_y << "\n";
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_HEADER;
    }
    if (exr_header->tile_level_mode != TINYEXR_TILE_RIPMAP_LEVELS) {
      EXRImage* level_image = NULL;
      for (int level = 0; level < offset_data.num_x_levels; ++level) {
        if (!level_image) {
          level_image = exr_image;
        } else {
          level_image->next_level = new EXRImage;
          InitEXRImage(level_image->next_level);
          level_image = level_image->next_level;
        }
        level_image->width =
          LevelSize(exr_header->data_window.max_x - exr_header->data_window.min_x + 1, level, exr_header->tile_rounding_mode);
        if (level_image->width < 1) {
          return TINYEXR_ERROR_INVALID_DATA;
        }

        level_image->height =
          LevelSize(exr_header->data_window.max_y - exr_header->data_window.min_y + 1, level, exr_header->tile_rounding_mode);

        if (level_image->height < 1) {
          return TINYEXR_ERROR_INVALID_DATA;
        }

        level_image->level_x = level;
        level_image->level_y = level;

        int ret = DecodeTiledLevel(level_image, exr_header,
          offset_data,
          channel_offset_list,
          pixel_data_size,
          head, size,
          err);
        if (ret != TINYEXR_SUCCESS) return ret;
      }
    } else {
      EXRImage* level_image = NULL;
      for (int level_y = 0; level_y < offset_data.num_y_levels; ++level_y)
        for (int level_x = 0; level_x < offset_data.num_x_levels; ++level_x) {
          if (!level_image) {
            level_image = exr_image;
          } else {
            level_image->next_level = new EXRImage;
            InitEXRImage(level_image->next_level);
            level_image = level_image->next_level;
          }

          level_image->width =
            LevelSize(exr_header->data_window.max_x - exr_header->data_window.min_x + 1, level_x, exr_header->tile_rounding_mode);
          if (level_image->width < 1) {
            return TINYEXR_ERROR_INVALID_DATA;
          }

          level_image->height =
            LevelSize(exr_header->data_window.max_y - exr_header->data_window.min_y + 1, level_y, exr_header->tile_rounding_mode);
          if (level_image->height < 1) {
            return TINYEXR_ERROR_INVALID_DATA;
          }

          level_image->level_x = level_x;
          level_image->level_y = level_y;

          int ret = DecodeTiledLevel(level_image, exr_header,
            offset_data,
            channel_offset_list,
            pixel_data_size,
            head, size,
            err);
          if (ret != TINYEXR_SUCCESS) return ret;
        }
    }
  } else {  // scanline format
    // Don't allow too large image(256GB * pixel_data_size or more). Workaround
    // for #104.
    size_t total_data_len =
        size_t(data_width) * size_t(data_height) * size_t(num_channels);
    const bool total_data_len_overflown =
        sizeof(void *) == 8 ? (total_data_len >= 0x4000000000) : false;
    if ((total_data_len == 0) || total_data_len_overflown) {
      if (err) {
        std::stringstream ss;
        ss << "Image data size is zero or too large: width = " << data_width
           << ", height = " << data_height << ", channels = " << num_channels
           << std::endl;
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_DATA;
    }

    bool alloc_success = false;
    exr_image->images = tinyexr::AllocateImage(
        num_channels, exr_header->channels, exr_header->requested_pixel_types,
        int(data_width), int(data_height), &alloc_success);

    if (!alloc_success) {
      if (err) {
        std::stringstream ss;
        ss << "Failed to allocate memory for Images. Maybe EXR header is corrupted or Image data size is too large: width = " << data_width
           << ", height = " << data_height << ", channels = " << num_channels
           << std::endl;
        (*err) += ss.str();
      }
      return TINYEXR_ERROR_INVALID_DATA;
    }

#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
    std::vector<std::thread> workers;
    std::atomic<int> y_count(0);

    int num_threads = std::max(1, int(std::thread::hardware_concurrency()));
    if (num_threads > int(num_blocks)) {
      num_threads = int(num_blocks);
    }

    for (int t = 0; t < num_threads; t++) {
      workers.emplace_back(std::thread([&]() {
        int y = 0;
        while ((y = y_count++) < int(num_blocks)) {

#else

#if TINYEXR_USE_OPENMP
#pragma omp parallel for
#endif
    for (int y = 0; y < static_cast<int>(num_blocks); y++) {

#endif
          size_t y_idx = static_cast<size_t>(y);

          if (offsets[y_idx] + sizeof(int) * 2 > size) {
            invalid_data = true;
          } else {
            // 4 byte: scan line
            // 4 byte: data size
            // ~     : pixel data(uncompressed or compressed)
            size_t data_size =
                size_t(size - (offsets[y_idx] + sizeof(int) * 2));
            const unsigned char *data_ptr =
                reinterpret_cast<const unsigned char *>(head + offsets[y_idx]);

            int line_no;
            memcpy(&line_no, data_ptr, sizeof(int));
            int data_len;
            memcpy(&data_len, data_ptr + 4, sizeof(int));
            tinyexr::swap4(&line_no);
            tinyexr::swap4(&data_len);

            if (size_t(data_len) > data_size) {
              invalid_data = true;

            } else if ((line_no > (2 << 20)) || (line_no < -(2 << 20))) {
              // Too large value. Assume this is invalid
              // 2**20 = 1048576 = heuristic value.
              invalid_data = true;
            } else if (data_len == 0) {
              // TODO(syoyo): May be ok to raise the threshold for example
              // `data_len < 4`
              invalid_data = true;
            } else {
              // line_no may be negative.
              int end_line_no = (std::min)(line_no + num_scanline_blocks,
                                           (exr_header->data_window.max_y + 1));

              int num_lines = end_line_no - line_no;

              if (num_lines <= 0) {
                invalid_data = true;
              } else {
                // Move to data addr: 8 = 4 + 4;
                data_ptr += 8;

                // Adjust line_no with data_window.bmin.y

                // overflow check
                tinyexr_int64 lno =
                    static_cast<tinyexr_int64>(line_no) -
                    static_cast<tinyexr_int64>(exr_header->data_window.min_y);
                if (lno > std::numeric_limits<int>::max()) {
                  line_no = -1;  // invalid
                } else if (lno < -std::numeric_limits<int>::max()) {
                  line_no = -1;  // invalid
                } else {
                  line_no -= exr_header->data_window.min_y;
                }

                if (line_no < 0) {
                  invalid_data = true;
                } else {
                  if (!tinyexr::DecodePixelData(
                          exr_image->images, exr_header->requested_pixel_types,
                          data_ptr, static_cast<size_t>(data_len),
                          exr_header->compression_type, exr_header->line_order,
                          int(data_width), int(data_height), int(data_width), y, line_no,
                          num_lines, static_cast<size_t>(pixel_data_size),
                          static_cast<size_t>(
                              exr_header->num_custom_attributes),
                          exr_header->custom_attributes,
                          static_cast<size_t>(exr_header->num_channels),
                          exr_header->channels, channel_offset_list)) {
                    invalid_data = true;
                  }
                }
              }
            }
          }

#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
        }
      }));
    }

    for (auto &t : workers) {
      t.join();
    }
#else
    }  // omp parallel
#endif
  }

  if (invalid_data) {
    if (err) {
      (*err) += "Invalid/Corrupted data found when decoding pixels.\n";
    }

    // free alloced image.
    for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
      if (exr_image->images[c]) {
        free(exr_image->images[c]);
        exr_image->images[c] = NULL;
      }
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }

  // Overwrite `pixel_type` with `requested_pixel_type`.
  {
    for (int c = 0; c < exr_header->num_channels; c++) {
      exr_header->pixel_types[c] = exr_header->requested_pixel_types[c];
    }
  }

  {
    exr_image->num_channels = num_channels;

    exr_image->width = int(data_width);
    exr_image->height = int(data_height);
  }

  return TINYEXR_SUCCESS;
}

static bool ReconstructLineOffsets(
    std::vector<tinyexr::tinyexr_uint64> *offsets, size_t n,
    const unsigned char *head, const unsigned char *marker, const size_t size) {
  if (head >= marker) {
    return false;
  }
  if (offsets->size() != n) {
    return false;
  }

  for (size_t i = 0; i < n; i++) {
    size_t offset = static_cast<size_t>(marker - head);
    // Offset should not exceed whole EXR file/data size.
    if ((offset + sizeof(tinyexr::tinyexr_uint64)) >= size) {
      return false;
    }

    int y;
    unsigned int data_len;

    memcpy(&y, marker, sizeof(int));
    memcpy(&data_len, marker + 4, sizeof(unsigned int));

    if (data_len >= size) {
      return false;
    }

    tinyexr::swap4(&y);
    tinyexr::swap4(&data_len);

    (*offsets)[i] = offset;

    marker += data_len + 8;  // 8 = 4 bytes(y) + 4 bytes(data_len)
  }

  return true;
}


static int FloorLog2(unsigned x) {
  //
  // For x > 0, floorLog2(y) returns floor(log(x)/log(2)).
  //
  int y = 0;
  while (x > 1) {
    y += 1;
    x >>= 1u;
  }
  return y;
}


static int CeilLog2(unsigned x) {
  //
  // For x > 0, ceilLog2(y) returns ceil(log(x)/log(2)).
  //
  int y = 0;
  int r = 0;
  while (x > 1) {
    if (x & 1)
      r = 1;

    y += 1;
    x >>= 1u;
  }
  return y + r;
}

static int RoundLog2(int x, int tile_rounding_mode) {
  return (tile_rounding_mode == TINYEXR_TILE_ROUND_DOWN) ? FloorLog2(static_cast<unsigned>(x)) : CeilLog2(static_cast<unsigned>(x));
}

static int CalculateNumXLevels(const EXRHeader* exr_header) {
  int min_x = exr_header->data_window.min_x;
  int max_x = exr_header->data_window.max_x;
  int min_y = exr_header->data_window.min_y;
  int max_y = exr_header->data_window.max_y;

  int num = 0;
  switch (exr_header->tile_level_mode) {
  case TINYEXR_TILE_ONE_LEVEL:

    num = 1;
    break;

  case TINYEXR_TILE_MIPMAP_LEVELS:

  {
    int w = max_x - min_x + 1;
    int h = max_y - min_y + 1;
    num = RoundLog2(std::max(w, h), exr_header->tile_rounding_mode) + 1;
  }
  break;

  case TINYEXR_TILE_RIPMAP_LEVELS:

  {
    int w = max_x - min_x + 1;
    num = RoundLog2(w, exr_header->tile_rounding_mode) + 1;
  }
  break;

  default:

    return -1;
  }

  return num;
}

static int CalculateNumYLevels(const EXRHeader* exr_header) {
  int min_x = exr_header->data_window.min_x;
  int max_x = exr_header->data_window.max_x;
  int min_y = exr_header->data_window.min_y;
  int max_y = exr_header->data_window.max_y;
  int num = 0;

  switch (exr_header->tile_level_mode) {
  case TINYEXR_TILE_ONE_LEVEL:

    num = 1;
    break;

  case TINYEXR_TILE_MIPMAP_LEVELS:

  {
    int w = max_x - min_x + 1;
    int h = max_y - min_y + 1;
    num = RoundLog2(std::max(w, h), exr_header->tile_rounding_mode) + 1;
  }
  break;

  case TINYEXR_TILE_RIPMAP_LEVELS:

  {
    int h = max_y - min_y + 1;
    num = RoundLog2(h, exr_header->tile_rounding_mode) + 1;
  }
  break;

  default:

    return -1;
  }

  return num;
}

static bool CalculateNumTiles(std::vector<int>& numTiles,
  int toplevel_size,
  int size,
  int tile_rounding_mode) {
  for (unsigned i = 0; i < numTiles.size(); i++) {
    int l = LevelSize(toplevel_size, int(i), tile_rounding_mode);
    if (l < 0) {
      return false;
    }
    TINYEXR_CHECK_AND_RETURN_C(l <= std::numeric_limits<int>::max() - size + 1, false);

    numTiles[i] = (l + size - 1) / size;
  }
  return true;
}

static bool PrecalculateTileInfo(std::vector<int>& num_x_tiles,
  std::vector<int>& num_y_tiles,
  const EXRHeader* exr_header) {
  int min_x = exr_header->data_window.min_x;
  int max_x = exr_header->data_window.max_x;
  int min_y = exr_header->data_window.min_y;
  int max_y = exr_header->data_window.max_y;

  int num_x_levels = CalculateNumXLevels(exr_header);

  if (num_x_levels < 0) {
    return false;
  }

  int num_y_levels = CalculateNumYLevels(exr_header);

  if (num_y_levels < 0) {
    return false;
  }

  num_x_tiles.resize(size_t(num_x_levels));
  num_y_tiles.resize(size_t(num_y_levels));

  if (!CalculateNumTiles(num_x_tiles,
    max_x - min_x + 1,
    exr_header->tile_size_x,
    exr_header->tile_rounding_mode)) {
    return false;
  }

  if (!CalculateNumTiles(num_y_tiles,
    max_y - min_y + 1,
    exr_header->tile_size_y,
    exr_header->tile_rounding_mode)) {
    return false;
  }

  return true;
}

static void InitSingleResolutionOffsets(OffsetData& offset_data, size_t num_blocks) {
  offset_data.offsets.resize(1);
  offset_data.offsets[0].resize(1);
  offset_data.offsets[0][0].resize(num_blocks);
  offset_data.num_x_levels = 1;
  offset_data.num_y_levels = 1;
}

// Return sum of tile blocks.
// 0 = error
static int InitTileOffsets(OffsetData& offset_data,
  const EXRHeader* exr_header,
  const std::vector<int>& num_x_tiles,
  const std::vector<int>& num_y_tiles) {
  int num_tile_blocks = 0;
  offset_data.num_x_levels = static_cast<int>(num_x_tiles.size());
  offset_data.num_y_levels = static_cast<int>(num_y_tiles.size());
  switch (exr_header->tile_level_mode) {
  case TINYEXR_TILE_ONE_LEVEL:
  case TINYEXR_TILE_MIPMAP_LEVELS:
    TINYEXR_CHECK_AND_RETURN_C(offset_data.num_x_levels == offset_data.num_y_levels, 0);
    offset_data.offsets.resize(size_t(offset_data.num_x_levels));

    for (unsigned int l = 0; l < offset_data.offsets.size(); ++l) {
      offset_data.offsets[l].resize(size_t(num_y_tiles[l]));

      for (unsigned int dy = 0; dy < offset_data.offsets[l].size(); ++dy) {
        offset_data.offsets[l][dy].resize(size_t(num_x_tiles[l]));
        num_tile_blocks += num_x_tiles[l];
      }
    }
    break;

  case TINYEXR_TILE_RIPMAP_LEVELS:

    offset_data.offsets.resize(static_cast<size_t>(offset_data.num_x_levels) * static_cast<size_t>(offset_data.num_y_levels));

    for (int ly = 0; ly < offset_data.num_y_levels; ++ly) {
      for (int lx = 0; lx < offset_data.num_x_levels; ++lx) {
        int l = ly * offset_data.num_x_levels + lx;
        offset_data.offsets[size_t(l)].resize(size_t(num_y_tiles[size_t(ly)]));

        for (size_t dy = 0; dy < offset_data.offsets[size_t(l)].size(); ++dy) {
          offset_data.offsets[size_t(l)][dy].resize(size_t(num_x_tiles[size_t(lx)]));
          num_tile_blocks += num_x_tiles[size_t(lx)];
        }
      }
    }
    break;

  default:
    return 0;
  }
  return num_tile_blocks;
}

static bool IsAnyOffsetsAreInvalid(const OffsetData& offset_data) {
  for (unsigned int l = 0; l < offset_data.offsets.size(); ++l)
    for (unsigned int dy = 0; dy < offset_data.offsets[l].size(); ++dy)
      for (unsigned int dx = 0; dx < offset_data.offsets[l][dy].size(); ++dx)
        if (reinterpret_cast<const tinyexr::tinyexr_int64&>(offset_data.offsets[l][dy][dx]) <= 0)
          return true;

  return false;
}

static bool isValidTile(const EXRHeader* exr_header,
                        const OffsetData& offset_data,
                        int dx, int dy, int lx, int ly) {
  if (lx < 0 || ly < 0 || dx < 0 || dy < 0) return false;
  int num_x_levels = offset_data.num_x_levels;
  int num_y_levels = offset_data.num_y_levels;
  switch (exr_header->tile_level_mode) {
  case TINYEXR_TILE_ONE_LEVEL:

    if (lx == 0 &&
        ly == 0 &&
        offset_data.offsets.size() > 0 &&
        offset_data.offsets[0].size() > static_cast<size_t>(dy) &&
        offset_data.offsets[0][size_t(dy)].size() > static_cast<size_t>(dx)) {
      return true;
    }

    break;

  case TINYEXR_TILE_MIPMAP_LEVELS:

    if (lx < num_x_levels &&
        ly < num_y_levels &&
        offset_data.offsets.size() > static_cast<size_t>(lx) &&
        offset_data.offsets[size_t(lx)].size() > static_cast<size_t>(dy) &&
        offset_data.offsets[size_t(lx)][size_t(dy)].size() > static_cast<size_t>(dx)) {
      return true;
    }

    break;

  case TINYEXR_TILE_RIPMAP_LEVELS:
  {
    size_t idx = static_cast<size_t>(lx) + static_cast<size_t>(ly)* static_cast<size_t>(num_x_levels);
    if (lx < num_x_levels &&
       ly < num_y_levels &&
       (offset_data.offsets.size() > idx) &&
       offset_data.offsets[idx].size() > static_cast<size_t>(dy) &&
       offset_data.offsets[idx][size_t(dy)].size() > static_cast<size_t>(dx)) {
      return true;
    }
  }

    break;

  default:

    return false;
  }

  return false;
}

static bool ReconstructTileOffsets(OffsetData& offset_data,
                                   const EXRHeader* exr_header,
                                   const unsigned char* head, const unsigned char* marker, const size_t size,
                                   bool isMultiPartFile,
                                   bool isDeep) {
  int numXLevels = offset_data.num_x_levels;
  for (unsigned int l = 0; l < offset_data.offsets.size(); ++l) {
    for (unsigned int dy = 0; dy < offset_data.offsets[l].size(); ++dy) {
      for (unsigned int dx = 0; dx < offset_data.offsets[l][dy].size(); ++dx) {
        tinyexr::tinyexr_uint64 tileOffset = tinyexr::tinyexr_uint64(marker - head);


        if (isMultiPartFile) {
          if ((marker + sizeof(int)) >= (head + size)) {
            return false;
          }

          //int partNumber;
          marker += sizeof(int);
        }

        if ((marker + 4 * sizeof(int)) >= (head + size)) {
          return false;
        }

        int tileX;
        memcpy(&tileX, marker, sizeof(int));
        tinyexr::swap4(&tileX);
        marker += sizeof(int);

        int tileY;
        memcpy(&tileY, marker, sizeof(int));
        tinyexr::swap4(&tileY);
        marker += sizeof(int);

        int levelX;
        memcpy(&levelX, marker, sizeof(int));
        tinyexr::swap4(&levelX);
        marker += sizeof(int);

        int levelY;
        memcpy(&levelY, marker, sizeof(int));
        tinyexr::swap4(&levelY);
        marker += sizeof(int);

        if (isDeep) {
          if ((marker + 2 * sizeof(tinyexr::tinyexr_int64)) >= (head + size)) {
            return false;
          }
          tinyexr::tinyexr_int64 packed_offset_table_size;
          memcpy(&packed_offset_table_size, marker, sizeof(tinyexr::tinyexr_int64));
          tinyexr::swap8(reinterpret_cast<tinyexr::tinyexr_uint64*>(&packed_offset_table_size));
          marker += sizeof(tinyexr::tinyexr_int64);

          tinyexr::tinyexr_int64 packed_sample_size;
          memcpy(&packed_sample_size, marker, sizeof(tinyexr::tinyexr_int64));
          tinyexr::swap8(reinterpret_cast<tinyexr::tinyexr_uint64*>(&packed_sample_size));
          marker += sizeof(tinyexr::tinyexr_int64);

          // next Int64 is unpacked sample size - skip that too
          marker += packed_offset_table_size + packed_sample_size + 8;

          if (marker >= (head + size)) {
            return false;
          }

        } else {

          if ((marker + sizeof(uint32_t)) >= (head + size)) {
            return false;
          }

          uint32_t dataSize;
          memcpy(&dataSize, marker, sizeof(uint32_t));
          tinyexr::swap4(&dataSize);
          marker += sizeof(uint32_t);

          marker += dataSize;

          if (marker >= (head + size)) {
            return false;
          }
        }

        if (!isValidTile(exr_header, offset_data,
          tileX, tileY, levelX, levelY)) {
          return false;
        }

        int level_idx = LevelIndex(levelX, levelY, exr_header->tile_level_mode, numXLevels);
        if (level_idx < 0) {
          return false;
        }

        if (size_t(level_idx) >= offset_data.offsets.size()) {
          return false;
        }

        if (size_t(tileY) >= offset_data.offsets[size_t(level_idx)].size()) {
          return false;
        }

        if (size_t(tileX) >= offset_data.offsets[size_t(level_idx)][size_t(tileY)].size()) {
          return false;
        }
        
        offset_data.offsets[size_t(level_idx)][size_t(tileY)][size_t(tileX)] = tileOffset;
      }
    }
  }
  return true;
}

// marker output is also
static int ReadOffsets(OffsetData& offset_data,
                       const unsigned char* head,
                       const unsigned char*& marker,
                       const size_t size,
                       const char** err) {
  for (unsigned int l = 0; l < offset_data.offsets.size(); ++l) {
    for (unsigned int dy = 0; dy < offset_data.offsets[l].size(); ++dy) {
      for (unsigned int dx = 0; dx < offset_data.offsets[l][dy].size(); ++dx) {
        tinyexr::tinyexr_uint64 offset;
        if ((marker + sizeof(tinyexr_uint64)) >= (head + size)) {
          tinyexr::SetErrorMessage("Insufficient data size in offset table.", err);
          return TINYEXR_ERROR_INVALID_DATA;
        }

        memcpy(&offset, marker, sizeof(tinyexr::tinyexr_uint64));
        tinyexr::swap8(&offset);
        if (offset >= size) {
          tinyexr::SetErrorMessage("Invalid offset value in DecodeEXRImage.", err);
          return TINYEXR_ERROR_INVALID_DATA;
        }
        marker += sizeof(tinyexr::tinyexr_uint64);  // = 8
        offset_data.offsets[l][dy][dx] = offset;
      }
    }
  }
  return TINYEXR_SUCCESS;
}

static int DecodeEXRImage(EXRImage *exr_image, const EXRHeader *exr_header,
                          const unsigned char *head,
                          const unsigned char *marker, const size_t size,
                          const char **err) {
  if (exr_image == NULL || exr_header == NULL || head == NULL ||
      marker == NULL || (size <= tinyexr::kEXRVersionSize)) {
    tinyexr::SetErrorMessage("Invalid argument for DecodeEXRImage().", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  int num_scanline_blocks = 1;
  if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_ZIP) {
    num_scanline_blocks = 16;
  } else if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {
    num_scanline_blocks = 32;
  } else if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
    num_scanline_blocks = 16;
  }

  if (exr_header->data_window.max_x < exr_header->data_window.min_x ||
      exr_header->data_window.max_x - exr_header->data_window.min_x ==
          std::numeric_limits<int>::max()) {
    // Issue 63
    tinyexr::SetErrorMessage("Invalid data width value", err);
    return TINYEXR_ERROR_INVALID_DATA;
  }
  tinyexr_int64 data_width =
      static_cast<tinyexr_int64>(exr_header->data_window.max_x) - static_cast<tinyexr_int64>(exr_header->data_window.min_x) + static_cast<tinyexr_int64>(1);
  if (data_width <= 0) {
    tinyexr::SetErrorMessage("Invalid data window width value", err);
    return TINYEXR_ERROR_INVALID_DATA;
  }

  if (exr_header->data_window.max_y < exr_header->data_window.min_y ||
      exr_header->data_window.max_y - exr_header->data_window.min_y ==
          std::numeric_limits<int>::max()) {
    tinyexr::SetErrorMessage("Invalid data height value", err);
    return TINYEXR_ERROR_INVALID_DATA;
  }
  tinyexr_int64 data_height =
      static_cast<tinyexr_int64>(exr_header->data_window.max_y) - static_cast<tinyexr_int64>(exr_header->data_window.min_y) + static_cast<tinyexr_int64>(1);

  if (data_height <= 0) {
    tinyexr::SetErrorMessage("Invalid data window height value", err);
    return TINYEXR_ERROR_INVALID_DATA;
  }

  // Do not allow too large data_width and data_height. header invalid?
  {
    if (data_width > TINYEXR_DIMENSION_THRESHOLD) {
      tinyexr::SetErrorMessage("data width too large.", err);
      return TINYEXR_ERROR_INVALID_DATA;
    }
    if (data_height > TINYEXR_DIMENSION_THRESHOLD) {
      tinyexr::SetErrorMessage("data height too large.", err);
      return TINYEXR_ERROR_INVALID_DATA;
    }
  }

  if (exr_header->tiled) {
    if (exr_header->tile_size_x > TINYEXR_DIMENSION_THRESHOLD) {
      tinyexr::SetErrorMessage("tile width too large.", err);
      return TINYEXR_ERROR_INVALID_DATA;
    }
    if (exr_header->tile_size_y > TINYEXR_DIMENSION_THRESHOLD) {
      tinyexr::SetErrorMessage("tile height too large.", err);
      return TINYEXR_ERROR_INVALID_DATA;
    }
  }

  // Read offset tables.
  OffsetData offset_data;
  size_t num_blocks = 0;
  // For a multi-resolution image, the size of the offset table will be calculated from the other attributes of the header.
  // If chunk_count > 0 then chunk_count must be equal to the calculated tile count.
  if (exr_header->tiled) {
    {
      std::vector<int> num_x_tiles, num_y_tiles;
      if (!PrecalculateTileInfo(num_x_tiles, num_y_tiles, exr_header)) {
        tinyexr::SetErrorMessage("Failed to precalculate tile info.", err);
        return TINYEXR_ERROR_INVALID_DATA;
      }
      num_blocks = size_t(InitTileOffsets(offset_data, exr_header, num_x_tiles, num_y_tiles));
      if (exr_header->chunk_count > 0) {
        if (exr_header->chunk_count != static_cast<int>(num_blocks)) {
          tinyexr::SetErrorMessage("Invalid offset table size.", err);
          return TINYEXR_ERROR_INVALID_DATA;
        }
      }
    }

    int ret = ReadOffsets(offset_data, head, marker, size, err);
    if (ret != TINYEXR_SUCCESS) return ret;
    if (IsAnyOffsetsAreInvalid(offset_data)) {
      if (!ReconstructTileOffsets(offset_data, exr_header,
        head, marker, size,
        exr_header->multipart, exr_header->non_image)) {

          tinyexr::SetErrorMessage("Invalid Tile Offsets data.", err);
          return TINYEXR_ERROR_INVALID_DATA;
      }
    }
  } else if (exr_header->chunk_count > 0) {
    // Use `chunkCount` attribute.
    num_blocks = static_cast<size_t>(exr_header->chunk_count);
    InitSingleResolutionOffsets(offset_data, num_blocks);
  } else {
    num_blocks = static_cast<size_t>(data_height) /
      static_cast<size_t>(num_scanline_blocks);
    if (num_blocks * static_cast<size_t>(num_scanline_blocks) <
      static_cast<size_t>(data_height)) {
      num_blocks++;
    }

    InitSingleResolutionOffsets(offset_data, num_blocks);
  }

  if (!exr_header->tiled) {
    std::vector<tinyexr::tinyexr_uint64>& offsets = offset_data.offsets[0][0];
    for (size_t y = 0; y < num_blocks; y++) {
      tinyexr::tinyexr_uint64 offset;
      // Issue #81
      if ((marker + sizeof(tinyexr_uint64)) >= (head + size)) {
        tinyexr::SetErrorMessage("Insufficient data size in offset table.", err);
        return TINYEXR_ERROR_INVALID_DATA;
      }

      memcpy(&offset, marker, sizeof(tinyexr::tinyexr_uint64));
      tinyexr::swap8(&offset);
      if (offset >= size) {
        tinyexr::SetErrorMessage("Invalid offset value in DecodeEXRImage.", err);
        return TINYEXR_ERROR_INVALID_DATA;
      }
      marker += sizeof(tinyexr::tinyexr_uint64);  // = 8
      offsets[y] = offset;
    }

    // If line offsets are invalid, we try to reconstruct it.
    // See OpenEXR/IlmImf/ImfScanLineInputFile.cpp::readLineOffsets() for details.
    for (size_t y = 0; y < num_blocks; y++) {
      if (offsets[y] <= 0) {
        // TODO(syoyo) Report as warning?
        // if (err) {
        //  stringstream ss;
        //  ss << "Incomplete lineOffsets." << std::endl;
        //  (*err) += ss.str();
        //}
        bool ret =
          ReconstructLineOffsets(&offsets, num_blocks, head, marker, size);
        if (ret) {
          // OK
          break;
        } else {
          tinyexr::SetErrorMessage(
            "Cannot reconstruct lineOffset table in DecodeEXRImage.", err);
          return TINYEXR_ERROR_INVALID_DATA;
        }
      }
    }
  }

  {
    std::string e;
    int ret = DecodeChunk(exr_image, exr_header, offset_data, head, size, &e);

    if (ret != TINYEXR_SUCCESS) {
      if (!e.empty()) {
        tinyexr::SetErrorMessage(e, err);
      }

#if 1
      FreeEXRImage(exr_image);
#else
      // release memory(if exists)
      if ((exr_header->num_channels > 0) && exr_image && exr_image->images) {
        for (size_t c = 0; c < size_t(exr_header->num_channels); c++) {
          if (exr_image->images[c]) {
            free(exr_image->images[c]);
            exr_image->images[c] = NULL;
          }
        }
        free(exr_image->images);
        exr_image->images = NULL;
      }
#endif
    }

    return ret;
  }
}

static void GetLayers(const EXRHeader &exr_header,
                      std::vector<std::string> &layer_names) {
  // Naive implementation
  // Group channels by layers
  // go over all channel names, split by periods
  // collect unique names
  layer_names.clear();
  for (int c = 0; c < exr_header.num_channels; c++) {
    std::string full_name(exr_header.channels[c].name);
    const size_t pos = full_name.find_last_of('.');
    if (pos != std::string::npos && pos != 0 && pos + 1 < full_name.size()) {
      full_name.erase(pos);
      if (std::find(layer_names.begin(), layer_names.end(), full_name) ==
          layer_names.end())
        layer_names.push_back(full_name);
    }
  }
}

struct LayerChannel {
  explicit LayerChannel(size_t i, std::string n) : index(i), name(n) {}
  size_t index;
  std::string name;
};

static void ChannelsInLayer(const EXRHeader &exr_header,
                            const std::string &layer_name,
                            std::vector<LayerChannel> &channels) {
  channels.clear();
  //std::cout << "layer_name = " << layer_name << "\n";
  for (int c = 0; c < exr_header.num_channels; c++) {
    //std::cout << "chan[" << c << "] = " << exr_header.channels[c].name << "\n";
    std::string ch_name(exr_header.channels[c].name);
    if (layer_name.empty()) {
      const size_t pos = ch_name.find_last_of('.');
      if (pos != std::string::npos && pos < ch_name.size()) {
        if (pos != 0) continue;
        ch_name = ch_name.substr(pos + 1);
      }
    } else {
      const size_t pos = ch_name.find(layer_name + '.');
      if (pos == std::string::npos) continue;
      if (pos == 0) {
        ch_name = ch_name.substr(layer_name.size() + 1);
      }
    }
    LayerChannel ch(size_t(c), ch_name);
    channels.push_back(ch);
  }
}

}  // namespace tinyexr

int EXRLayers(const char *filename, const char **layer_names[], int *num_layers,
              const char **err) {
  EXRVersion exr_version;
  EXRHeader exr_header;
  InitEXRHeader(&exr_header);

  {
    int ret = ParseEXRVersionFromFile(&exr_version, filename);
    if (ret != TINYEXR_SUCCESS) {
      tinyexr::SetErrorMessage("Invalid EXR header.", err);
      return ret;
    }

    if (exr_version.multipart || exr_version.non_image) {
      tinyexr::SetErrorMessage(
          "Loading multipart or DeepImage is not supported  in LoadEXR() API",
          err);
      return TINYEXR_ERROR_INVALID_DATA;  // @fixme.
    }
  }

  int ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, filename, err);
  if (ret != TINYEXR_SUCCESS) {
    FreeEXRHeader(&exr_header);
    return ret;
  }

  std::vector<std::string> layer_vec;
  tinyexr::GetLayers(exr_header, layer_vec);

  (*num_layers) = int(layer_vec.size());
  (*layer_names) = static_cast<const char **>(
      malloc(sizeof(const char *) * static_cast<size_t>(layer_vec.size())));
  for (size_t c = 0; c < static_cast<size_t>(layer_vec.size()); c++) {
#ifdef _MSC_VER
    (*layer_names)[c] = _strdup(layer_vec[c].c_str());
#else
    (*layer_names)[c] = strdup(layer_vec[c].c_str());
#endif
  }

  FreeEXRHeader(&exr_header);
  return TINYEXR_SUCCESS;
}

int LoadEXR(float **out_rgba, int *width, int *height, const char *filename,
            const char **err) {
  return LoadEXRWithLayer(out_rgba, width, height, filename,
                          /* layername */ NULL, err);
}

int LoadEXRWithLayer(float **out_rgba, int *width, int *height,
                     const char *filename, const char *layername,
                     const char **err) {
  if (out_rgba == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for LoadEXR()", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  EXRVersion exr_version;
  EXRImage exr_image;
  EXRHeader exr_header;
  InitEXRHeader(&exr_header);
  InitEXRImage(&exr_image);

  {
    int ret = ParseEXRVersionFromFile(&exr_version, filename);
    if (ret != TINYEXR_SUCCESS) {
      std::stringstream ss;
      ss << "Failed to open EXR file or read version info from EXR file. code("
         << ret << ")";
      tinyexr::SetErrorMessage(ss.str(), err);
      return ret;
    }

    if (exr_version.multipart || exr_version.non_image) {
      tinyexr::SetErrorMessage(
          "Loading multipart or DeepImage is not supported  in LoadEXR() API",
          err);
      return TINYEXR_ERROR_INVALID_DATA;  // @fixme.
    }
  }

  {
    int ret = ParseEXRHeaderFromFile(&exr_header, &exr_version, filename, err);
    if (ret != TINYEXR_SUCCESS) {
      FreeEXRHeader(&exr_header);
      return ret;
    }
  }

  // Read HALF channel as FLOAT.
  for (int i = 0; i < exr_header.num_channels; i++) {
    if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
      exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    }
  }

  // TODO: Probably limit loading to layers (channels) selected by layer index
  {
    int ret = LoadEXRImageFromFile(&exr_image, &exr_header, filename, err);
    if (ret != TINYEXR_SUCCESS) {
      FreeEXRHeader(&exr_header);
      return ret;
    }
  }

  // RGBA
  int idxR = -1;
  int idxG = -1;
  int idxB = -1;
  int idxA = -1;

  std::vector<std::string> layer_names;
  tinyexr::GetLayers(exr_header, layer_names);

  std::vector<tinyexr::LayerChannel> channels;
  tinyexr::ChannelsInLayer(
      exr_header, layername == NULL ? "" : std::string(layername), channels);


  if (channels.size() < 1) {
    if (layername == NULL) {
      tinyexr::SetErrorMessage("Layer Not Found. Seems EXR contains channels with layer(e.g. `diffuse.R`). if you are using LoadEXR(), please try LoadEXRWithLayer(). LoadEXR() cannot load EXR having channels with layer.", err);

    } else {
      tinyexr::SetErrorMessage("Layer Not Found", err);
    }
    FreeEXRHeader(&exr_header);
    FreeEXRImage(&exr_image);
    return TINYEXR_ERROR_LAYER_NOT_FOUND;
  }

  size_t ch_count = channels.size() < 4 ? channels.size() : 4;
  for (size_t c = 0; c < ch_count; c++) {
    const tinyexr::LayerChannel &ch = channels[c];

    if (ch.name == "R") {
      idxR = int(ch.index);
    } else if (ch.name == "G") {
      idxG = int(ch.index);
    } else if (ch.name == "B") {
      idxB = int(ch.index);
    } else if (ch.name == "A") {
      idxA = int(ch.index);
    }
  }

  if (channels.size() == 1) {
    int chIdx = int(channels.front().index);
    // Grayscale channel only.

    (*out_rgba) = reinterpret_cast<float *>(
        malloc(4 * sizeof(float) * static_cast<size_t>(exr_image.width) *
               static_cast<size_t>(exr_image.height)));

    if (exr_header.tiled) {
      const size_t tile_size_x = static_cast<size_t>(exr_header.tile_size_x);
      const size_t tile_size_y = static_cast<size_t>(exr_header.tile_size_y);
      for (int it = 0; it < exr_image.num_tiles; it++) {
        for (size_t j = 0; j < tile_size_y; j++) {
          for (size_t i = 0; i < tile_size_x; i++) {
            const size_t ii =
              static_cast<size_t>(exr_image.tiles[it].offset_x) * tile_size_x +
              i;
            const size_t jj =
              static_cast<size_t>(exr_image.tiles[it].offset_y) * tile_size_y +
              j;
            const size_t idx = ii + jj * static_cast<size_t>(exr_image.width);

            // out of region check.
            if (ii >= static_cast<size_t>(exr_image.width)) {
              continue;
            }
            if (jj >= static_cast<size_t>(exr_image.height)) {
              continue;
            }
            const size_t srcIdx = i + j * tile_size_x;
            unsigned char **src = exr_image.tiles[it].images;
            (*out_rgba)[4 * idx + 0] =
                reinterpret_cast<float **>(src)[chIdx][srcIdx];
            (*out_rgba)[4 * idx + 1] =
                reinterpret_cast<float **>(src)[chIdx][srcIdx];
            (*out_rgba)[4 * idx + 2] =
                reinterpret_cast<float **>(src)[chIdx][srcIdx];
            (*out_rgba)[4 * idx + 3] =
                reinterpret_cast<float **>(src)[chIdx][srcIdx];
          }
        }
      }
    } else {
      const size_t pixel_size = static_cast<size_t>(exr_image.width) *
        static_cast<size_t>(exr_image.height);
      for (size_t i = 0; i < pixel_size; i++) {
        const float val =
            reinterpret_cast<float **>(exr_image.images)[chIdx][i];
        (*out_rgba)[4 * i + 0] = val;
        (*out_rgba)[4 * i + 1] = val;
        (*out_rgba)[4 * i + 2] = val;
        (*out_rgba)[4 * i + 3] = val;
      }
    }
  } else {
    // Assume RGB(A)

    if (idxR == -1) {
      tinyexr::SetErrorMessage("R channel not found", err);

      FreeEXRHeader(&exr_header);
      FreeEXRImage(&exr_image);
      return TINYEXR_ERROR_INVALID_DATA;
    }

    if (idxG == -1) {
      tinyexr::SetErrorMessage("G channel not found", err);
      FreeEXRHeader(&exr_header);
      FreeEXRImage(&exr_image);
      return TINYEXR_ERROR_INVALID_DATA;
    }

    if (idxB == -1) {
      tinyexr::SetErrorMessage("B channel not found", err);
      FreeEXRHeader(&exr_header);
      FreeEXRImage(&exr_image);
      return TINYEXR_ERROR_INVALID_DATA;
    }

    (*out_rgba) = reinterpret_cast<float *>(
        malloc(4 * sizeof(float) * static_cast<size_t>(exr_image.width) *
               static_cast<size_t>(exr_image.height)));
    if (exr_header.tiled) {
      const size_t tile_size_x = static_cast<size_t>(exr_header.tile_size_x);
      const size_t tile_size_y = static_cast<size_t>(exr_header.tile_size_y);
      for (int it = 0; it < exr_image.num_tiles; it++) {
        for (size_t j = 0; j < tile_size_y; j++) {
          for (size_t i = 0; i < tile_size_x; i++) {
            const size_t ii =
                static_cast<size_t>(exr_image.tiles[it].offset_x) *
                    tile_size_x +
                i;
            const size_t jj =
                static_cast<size_t>(exr_image.tiles[it].offset_y) *
                    tile_size_y +
                j;
            const size_t idx = ii + jj * static_cast<size_t>(exr_image.width);

            // out of region check.
            if (ii >= static_cast<size_t>(exr_image.width)) {
              continue;
            }
            if (jj >= static_cast<size_t>(exr_image.height)) {
              continue;
            }
            const size_t srcIdx = i + j * tile_size_x;
            unsigned char **src = exr_image.tiles[it].images;
            (*out_rgba)[4 * idx + 0] =
                reinterpret_cast<float **>(src)[idxR][srcIdx];
            (*out_rgba)[4 * idx + 1] =
                reinterpret_cast<float **>(src)[idxG][srcIdx];
            (*out_rgba)[4 * idx + 2] =
                reinterpret_cast<float **>(src)[idxB][srcIdx];
            if (idxA != -1) {
              (*out_rgba)[4 * idx + 3] =
                  reinterpret_cast<float **>(src)[idxA][srcIdx];
            } else {
              (*out_rgba)[4 * idx + 3] = 1.0;
            }
          }
        }
      }
    } else {
      const size_t pixel_size = static_cast<size_t>(exr_image.width) *
        static_cast<size_t>(exr_image.height);
      for (size_t i = 0; i < pixel_size; i++) {
        (*out_rgba)[4 * i + 0] =
            reinterpret_cast<float **>(exr_image.images)[idxR][i];
        (*out_rgba)[4 * i + 1] =
            reinterpret_cast<float **>(exr_image.images)[idxG][i];
        (*out_rgba)[4 * i + 2] =
            reinterpret_cast<float **>(exr_image.images)[idxB][i];
        if (idxA != -1) {
          (*out_rgba)[4 * i + 3] =
              reinterpret_cast<float **>(exr_image.images)[idxA][i];
        } else {
          (*out_rgba)[4 * i + 3] = 1.0;
        }
      }
    }
  }

  (*width) = exr_image.width;
  (*height) = exr_image.height;

  FreeEXRHeader(&exr_header);
  FreeEXRImage(&exr_image);

  return TINYEXR_SUCCESS;
}

int IsEXR(const char *filename) {
  EXRVersion exr_version;

  int ret = ParseEXRVersionFromFile(&exr_version, filename);
  if (ret != TINYEXR_SUCCESS) {
    return ret;
  }

  return TINYEXR_SUCCESS;
}

int IsEXRFromMemory(const unsigned char *memory, size_t size) {
  EXRVersion exr_version;

  int ret = ParseEXRVersionFromMemory(&exr_version, memory, size);
  if (ret != TINYEXR_SUCCESS) {
    return ret;
  }

  return TINYEXR_SUCCESS;
}

int ParseEXRHeaderFromMemory(EXRHeader *exr_header, const EXRVersion *version,
                             const unsigned char *memory, size_t size,
                             const char **err) {
  if (memory == NULL || exr_header == NULL) {
    tinyexr::SetErrorMessage(
        "Invalid argument. `memory` or `exr_header` argument is null in "
        "ParseEXRHeaderFromMemory()",
        err);

    // Invalid argument
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  if (size < tinyexr::kEXRVersionSize) {
    tinyexr::SetErrorMessage("Insufficient header/data size.\n", err);
    return TINYEXR_ERROR_INVALID_DATA;
  }

  const unsigned char *marker = memory + tinyexr::kEXRVersionSize;
  size_t marker_size = size - tinyexr::kEXRVersionSize;

  tinyexr::HeaderInfo info;
  info.clear();

  int ret;
  {
    std::string err_str;
    ret = ParseEXRHeader(&info, NULL, version, &err_str, marker, marker_size);

    if (ret != TINYEXR_SUCCESS) {
      if (err && !err_str.empty()) {
        tinyexr::SetErrorMessage(err_str, err);
      }
    }
  }

  {
    std::string warn;
    std::string err_str;

    if (!ConvertHeader(exr_header, info, &warn, &err_str)) {
      // release mem
      for (size_t i = 0; i < info.attributes.size(); i++) {
        if (info.attributes[i].value) {
          free(info.attributes[i].value);
        }
      }
      if (err && !err_str.empty()) {
        tinyexr::SetErrorMessage(err_str, err);
      }
      ret = TINYEXR_ERROR_INVALID_HEADER;
    }
  }

  exr_header->multipart = version->multipart ? 1 : 0;
  exr_header->non_image = version->non_image ? 1 : 0;

  return ret;
}

int LoadEXRFromMemory(float **out_rgba, int *width, int *height,
                      const unsigned char *memory, size_t size,
                      const char **err) {
  if (out_rgba == NULL || memory == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for LoadEXRFromMemory", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  EXRVersion exr_version;
  EXRImage exr_image;
  EXRHeader exr_header;

  InitEXRHeader(&exr_header);

  int ret = ParseEXRVersionFromMemory(&exr_version, memory, size);
  if (ret != TINYEXR_SUCCESS) {
    std::stringstream ss;
    ss << "Failed to parse EXR version. code(" << ret << ")";
    tinyexr::SetErrorMessage(ss.str(), err);
    return ret;
  }

  ret = ParseEXRHeaderFromMemory(&exr_header, &exr_version, memory, size, err);
  if (ret != TINYEXR_SUCCESS) {
    return ret;
  }

  // Read HALF channel as FLOAT.
  for (int i = 0; i < exr_header.num_channels; i++) {
    if (exr_header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
      exr_header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    }
  }

  InitEXRImage(&exr_image);
  ret = LoadEXRImageFromMemory(&exr_image, &exr_header, memory, size, err);
  if (ret != TINYEXR_SUCCESS) {
    return ret;
  }

  // RGBA
  int idxR = -1;
  int idxG = -1;
  int idxB = -1;
  int idxA = -1;
  for (int c = 0; c < exr_header.num_channels; c++) {
    if (strcmp(exr_header.channels[c].name, "R") == 0) {
      idxR = c;
    } else if (strcmp(exr_header.channels[c].name, "G") == 0) {
      idxG = c;
    } else if (strcmp(exr_header.channels[c].name, "B") == 0) {
      idxB = c;
    } else if (strcmp(exr_header.channels[c].name, "A") == 0) {
      idxA = c;
    }
  }

  // TODO(syoyo): Refactor removing same code as used in LoadEXR().
  if (exr_header.num_channels == 1) {
    // Grayscale channel only.

    (*out_rgba) = reinterpret_cast<float *>(
        malloc(4 * sizeof(float) * static_cast<size_t>(exr_image.width) *
               static_cast<size_t>(exr_image.height)));

    if (exr_header.tiled) {
      const size_t tile_size_x = static_cast<size_t>(exr_header.tile_size_x);
      const size_t tile_size_y = static_cast<size_t>(exr_header.tile_size_y);
      for (int it = 0; it < exr_image.num_tiles; it++) {
        for (size_t j = 0; j < tile_size_y; j++) {
          for (size_t i = 0; i < tile_size_x; i++) {
            const size_t ii =
                static_cast<size_t>(exr_image.tiles[it].offset_x) *
                    tile_size_x +
                i;
            const size_t jj =
                static_cast<size_t>(exr_image.tiles[it].offset_y) *
                    tile_size_y +
                j;
            const size_t idx = ii + jj * static_cast<size_t>(exr_image.width);

            // out of region check.
            if (ii >= static_cast<size_t>(exr_image.width)) {
              continue;
            }
            if (jj >= static_cast<size_t>(exr_image.height)) {
              continue;
            }
            const size_t srcIdx = i + j * tile_size_x;
            unsigned char **src = exr_image.tiles[it].images;
            (*out_rgba)[4 * idx + 0] =
                reinterpret_cast<float **>(src)[0][srcIdx];
            (*out_rgba)[4 * idx + 1] =
                reinterpret_cast<float **>(src)[0][srcIdx];
            (*out_rgba)[4 * idx + 2] =
                reinterpret_cast<float **>(src)[0][srcIdx];
            (*out_rgba)[4 * idx + 3] =
                reinterpret_cast<float **>(src)[0][srcIdx];
          }
        }
      }
    } else {
      const size_t pixel_size = static_cast<size_t>(exr_image.width) *
        static_cast<size_t>(exr_image.height);
      for (size_t i = 0; i < pixel_size; i++) {
        const float val = reinterpret_cast<float **>(exr_image.images)[0][i];
        (*out_rgba)[4 * i + 0] = val;
        (*out_rgba)[4 * i + 1] = val;
        (*out_rgba)[4 * i + 2] = val;
        (*out_rgba)[4 * i + 3] = val;
      }
    }

  } else {
    // TODO(syoyo): Support non RGBA image.

    if (idxR == -1) {
      tinyexr::SetErrorMessage("R channel not found", err);

      // @todo { free exr_image }
      return TINYEXR_ERROR_INVALID_DATA;
    }

    if (idxG == -1) {
      tinyexr::SetErrorMessage("G channel not found", err);
      // @todo { free exr_image }
      return TINYEXR_ERROR_INVALID_DATA;
    }

    if (idxB == -1) {
      tinyexr::SetErrorMessage("B channel not found", err);
      // @todo { free exr_image }
      return TINYEXR_ERROR_INVALID_DATA;
    }

    (*out_rgba) = reinterpret_cast<float *>(
        malloc(4 * sizeof(float) * static_cast<size_t>(exr_image.width) *
               static_cast<size_t>(exr_image.height)));

    if (exr_header.tiled) {
      const size_t tile_size_x = static_cast<size_t>(exr_header.tile_size_x);
      const size_t tile_size_y = static_cast<size_t>(exr_header.tile_size_y);
      for (int it = 0; it < exr_image.num_tiles; it++) {
        for (size_t j = 0; j < tile_size_y; j++)
          for (size_t i = 0; i < tile_size_x; i++) {
            const size_t ii =
                static_cast<size_t>(exr_image.tiles[it].offset_x) *
                    tile_size_x +
                i;
            const size_t jj =
                static_cast<size_t>(exr_image.tiles[it].offset_y) *
                    tile_size_y +
                j;
            const size_t idx = ii + jj * static_cast<size_t>(exr_image.width);

            // out of region check.
            if (ii >= static_cast<size_t>(exr_image.width)) {
              continue;
            }
            if (jj >= static_cast<size_t>(exr_image.height)) {
              continue;
            }
            const size_t srcIdx = i + j * tile_size_x;
            unsigned char **src = exr_image.tiles[it].images;
            (*out_rgba)[4 * idx + 0] =
                reinterpret_cast<float **>(src)[idxR][srcIdx];
            (*out_rgba)[4 * idx + 1] =
                reinterpret_cast<float **>(src)[idxG][srcIdx];
            (*out_rgba)[4 * idx + 2] =
                reinterpret_cast<float **>(src)[idxB][srcIdx];
            if (idxA != -1) {
              (*out_rgba)[4 * idx + 3] =
                  reinterpret_cast<float **>(src)[idxA][srcIdx];
            } else {
              (*out_rgba)[4 * idx + 3] = 1.0;
            }
          }
      }
    } else {
      const size_t pixel_size = static_cast<size_t>(exr_image.width) *
        static_cast<size_t>(exr_image.height);
      for (size_t i = 0; i < pixel_size; i++) {
        (*out_rgba)[4 * i + 0] =
            reinterpret_cast<float **>(exr_image.images)[idxR][i];
        (*out_rgba)[4 * i + 1] =
            reinterpret_cast<float **>(exr_image.images)[idxG][i];
        (*out_rgba)[4 * i + 2] =
            reinterpret_cast<float **>(exr_image.images)[idxB][i];
        if (idxA != -1) {
          (*out_rgba)[4 * i + 3] =
              reinterpret_cast<float **>(exr_image.images)[idxA][i];
        } else {
          (*out_rgba)[4 * i + 3] = 1.0;
        }
      }
    }
  }

  (*width) = exr_image.width;
  (*height) = exr_image.height;

  FreeEXRHeader(&exr_header);
  FreeEXRImage(&exr_image);

  return TINYEXR_SUCCESS;
}

// Represents a read-only file mapped to an address space in memory.
// If no memory-mapping API is available, falls back to allocating a buffer
// with a copy of the file's data.
struct MemoryMappedFile {
  unsigned char *data;  // To the start of the file's data.
  size_t size;          // The size of the file in bytes.
#ifdef TINYEXR_USE_WIN32_MMAP
  HANDLE windows_file;
  HANDLE windows_file_mapping;
#elif defined(TINYEXR_USE_POSIX_MMAP)
  int posix_descriptor;
#endif

  // MemoryMappedFile's constructor tries to map memory to a file.
  // If this succeeds, valid() will return true and all fields
  // are usable; otherwise, valid() will return false.
  MemoryMappedFile(const char *filename) {
    data = NULL;
    size = 0;
#ifdef TINYEXR_USE_WIN32_MMAP
    windows_file_mapping = NULL;
    windows_file =
        CreateFileW(tinyexr::UTF8ToWchar(filename).c_str(),  // lpFileName
                    GENERIC_READ,                            // dwDesiredAccess
                    FILE_SHARE_READ,                         // dwShareMode
                    NULL,                     // lpSecurityAttributes
                    OPEN_EXISTING,            // dwCreationDisposition
                    FILE_ATTRIBUTE_READONLY,  // dwFlagsAndAttributes
                    NULL);                    // hTemplateFile
    if (windows_file == INVALID_HANDLE_VALUE) {
      return;
    }

    windows_file_mapping = CreateFileMapping(windows_file,  // hFile
                                             NULL,  // lpFileMappingAttributes
                                             PAGE_READONLY,  // flProtect
                                             0,      // dwMaximumSizeHigh
                                             0,      // dwMaximumSizeLow
                                             NULL);  // lpName
    if (windows_file_mapping == NULL) {
      return;
    }

    data = reinterpret_cast<unsigned char *>(
        MapViewOfFile(windows_file_mapping,  // hFileMappingObject
                      FILE_MAP_READ,         // dwDesiredAccess
                      0,                     // dwFileOffsetHigh
                      0,                     // dwFileOffsetLow
                      0));                   // dwNumberOfBytesToMap
    if (!data) {
      return;
    }

    LARGE_INTEGER windows_file_size = {};
    if (!GetFileSizeEx(windows_file, &windows_file_size) ||
        static_cast<ULONGLONG>(windows_file_size.QuadPart) >
            std::numeric_limits<size_t>::max()) {
      UnmapViewOfFile(data);
      data = NULL;
      return;
    }
    size = static_cast<size_t>(windows_file_size.QuadPart);
#elif defined(TINYEXR_USE_POSIX_MMAP)
    posix_descriptor = open(filename, O_RDONLY);
    if (posix_descriptor == -1) {
      return;
    }

    struct stat info;
    if (fstat(posix_descriptor, &info) < 0) {
      return;
    }
    // Make sure st_size is in the valid range for a size_t. The second case
    // can only fail if a POSIX implementation defines off_t to be a larger
    // type than size_t - for instance, compiling with _FILE_OFFSET_BITS=64
    // on a 32-bit system. On current 64-bit systems, this check can never
    // fail, so we turn off clang's Wtautological-type-limit-compare warning
    // around this code.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-type-limit-compare"
#endif
    if (info.st_size < 0 ||
        info.st_size > std::numeric_limits<ssize_t>::max()) {
      return;
    }
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    size = static_cast<size_t>(info.st_size);

    data = reinterpret_cast<unsigned char *>(
        mmap(0, size, PROT_READ, MAP_SHARED, posix_descriptor, 0));
    if (data == MAP_FAILED) {
      data = nullptr;
      return;
    }
#else
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
      return;
    }

    // Calling fseek(fp, 0, SEEK_END) isn't strictly-conforming C code, but
    // since neither the WIN32 nor POSIX APIs are available in this branch, this
    // is a reasonable fallback option.
    if (fseek(fp, 0, SEEK_END) != 0) {
      fclose(fp);
      return;
    }
    const long ftell_result = ftell(fp);
    if (ftell_result < 0) {
      // Error from ftell
      fclose(fp);
      return;
    }
    size = static_cast<size_t>(ftell_result);
    if (fseek(fp, 0, SEEK_SET) != 0) {
      fclose(fp);
      size = 0;
      return;
    }

    data = reinterpret_cast<unsigned char *>(malloc(size));
    if (!data) {
      size = 0;
      fclose(fp);
      return;
    }
    size_t read_bytes = fread(data, 1, size, fp);
    if (read_bytes != size) {
      // TODO: Try to read data until reading `size` bytes.
      fclose(fp);
      size = 0; 
      data = nullptr;
      return;
    }
    fclose(fp);
#endif
  }

  // MemoryMappedFile's destructor closes all its handles.
  ~MemoryMappedFile() {
#ifdef TINYEXR_USE_WIN32_MMAP
    if (data) {
      (void)UnmapViewOfFile(data);
      data = NULL;
    }

    if (windows_file_mapping != NULL) {
      (void)CloseHandle(windows_file_mapping);
    }

    if (windows_file != INVALID_HANDLE_VALUE) {
      (void)CloseHandle(windows_file);
    }
#elif defined(TINYEXR_USE_POSIX_MMAP)
    if (data) {
      (void)munmap(data, size);
      data = NULL;
    }

    if (posix_descriptor != -1) {
      (void)close(posix_descriptor);
    }
#else
    if (data) {
      (void)free(data);
    }
    data = NULL;
#endif
  }

  // A MemoryMappedFile cannot be copied or moved.
  // Only check for this when compiling with C++11 or higher, since deleted
  // function definitions were added then.
#if TINYEXR_HAS_CXX11
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat"
#endif
  MemoryMappedFile(const MemoryMappedFile &) = delete;
  MemoryMappedFile &operator=(const MemoryMappedFile &) = delete;
  MemoryMappedFile(MemoryMappedFile &&other) noexcept = delete;
  MemoryMappedFile &operator=(MemoryMappedFile &&other) noexcept = delete;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

  // Returns whether this was successfully opened.
  bool valid() const { return data; }
};

int LoadEXRImageFromFile(EXRImage *exr_image, const EXRHeader *exr_header,
                         const char *filename, const char **err) {
  if (exr_image == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for LoadEXRImageFromFile", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  MemoryMappedFile file(filename);
  if (!file.valid()) {
    tinyexr::SetErrorMessage("Cannot read file " + std::string(filename), err);
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  if (file.size < 16) {
    tinyexr::SetErrorMessage("File size too short : " + std::string(filename),
                             err);
    return TINYEXR_ERROR_INVALID_FILE;
  }

  return LoadEXRImageFromMemory(exr_image, exr_header, file.data, file.size,
                                err);
}

int LoadEXRImageFromMemory(EXRImage *exr_image, const EXRHeader *exr_header,
                           const unsigned char *memory, const size_t size,
                           const char **err) {
  if (exr_image == NULL || memory == NULL ||
      (size < tinyexr::kEXRVersionSize)) {
    tinyexr::SetErrorMessage("Invalid argument for LoadEXRImageFromMemory",
                             err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  if (exr_header->header_len == 0) {
    tinyexr::SetErrorMessage("EXRHeader variable is not initialized.", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  const unsigned char *head = memory;
  const unsigned char *marker = reinterpret_cast<const unsigned char *>(
      memory + exr_header->header_len +
      8);  // +8 for magic number + version header.
  return tinyexr::DecodeEXRImage(exr_image, exr_header, head, marker, size,
                                 err);
}

namespace tinyexr
{

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif

// out_data must be allocated initially with the block-header size
// of the current image(-part) type
static bool EncodePixelData(/* out */ std::vector<unsigned char>& out_data,
                            const unsigned char* const* images,
                            int compression_type,
                            int /*line_order*/,
                            int width, // for tiled : tile.width
                            int /*height*/, // for tiled : header.tile_size_y
                            int x_stride, // for tiled : header.tile_size_x
                            int line_no, // for tiled : 0
                            int num_lines, // for tiled : tile.height
                            size_t pixel_data_size,
                            const std::vector<ChannelInfo>& channels,
                            const std::vector<size_t>& channel_offset_list,
                            std::string *err,
                            const void* compression_param = 0) // zfp compression param
{
  size_t buf_size = static_cast<size_t>(width) *
                  static_cast<size_t>(num_lines) *
                  static_cast<size_t>(pixel_data_size);
  //int last2bit = (buf_size & 3);
  // buf_size must be multiple of four
  //if(last2bit) buf_size += 4 - last2bit;
  std::vector<unsigned char> buf(buf_size);

  size_t start_y = static_cast<size_t>(line_no);
  for (size_t c = 0; c < channels.size(); c++) {
    if (channels[c].pixel_type == TINYEXR_PIXELTYPE_HALF) {
      if (channels[c].requested_pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
        for (int y = 0; y < num_lines; y++) {
          // Assume increasing Y
          float *line_ptr = reinterpret_cast<float *>(&buf.at(
            static_cast<size_t>(pixel_data_size * size_t(y) * size_t(width)) +
            channel_offset_list[c] *
            static_cast<size_t>(width)));
          for (int x = 0; x < width; x++) {
            tinyexr::FP16 h16;
            h16.u = reinterpret_cast<const unsigned short * const *>(
              images)[c][(y + start_y) * size_t(x_stride) + size_t(x)];

            tinyexr::FP32 f32 = half_to_float(h16);

            tinyexr::swap4(&f32.f);

            // line_ptr[x] = f32.f;
            tinyexr::cpy4(line_ptr + x, &(f32.f));
          }
        }
      } else if (channels[c].requested_pixel_type == TINYEXR_PIXELTYPE_HALF) {
        for (int y = 0; y < num_lines; y++) {
          // Assume increasing Y
          unsigned short *line_ptr = reinterpret_cast<unsigned short *>(
            &buf.at(static_cast<size_t>(pixel_data_size * y *
                                        width) +
                    channel_offset_list[c] *
                    static_cast<size_t>(width)));
          for (int x = 0; x < width; x++) {
            unsigned short val = reinterpret_cast<const unsigned short * const *>(
              images)[c][(y + start_y) * x_stride + x];

            tinyexr::swap2(&val);

            // line_ptr[x] = val;
            tinyexr::cpy2(line_ptr + x, &val);
          }
        }
      } else {
        if (err) {
          (*err) += "Invalid requested_pixel_type.\n";
        }
        return false;
      }

    } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
      if (channels[c].requested_pixel_type == TINYEXR_PIXELTYPE_HALF) {
        for (int y = 0; y < num_lines; y++) {
          // Assume increasing Y
          unsigned short *line_ptr = reinterpret_cast<unsigned short *>(
            &buf.at(static_cast<size_t>(pixel_data_size * y *
                                        width) +
                    channel_offset_list[c] *
                    static_cast<size_t>(width)));
          for (int x = 0; x < width; x++) {
            tinyexr::FP32 f32;
            f32.f = reinterpret_cast<const float * const *>(
              images)[c][(y + start_y) * x_stride + x];

            tinyexr::FP16 h16;
            h16 = float_to_half_full(f32);

            tinyexr::swap2(reinterpret_cast<unsigned short *>(&h16.u));

            // line_ptr[x] = h16.u;
            tinyexr::cpy2(line_ptr + x, &(h16.u));
          }
        }
      } else if (channels[c].requested_pixel_type == TINYEXR_PIXELTYPE_FLOAT) {
        for (int y = 0; y < num_lines; y++) {
          // Assume increasing Y
          float *line_ptr = reinterpret_cast<float *>(&buf.at(
            static_cast<size_t>(pixel_data_size * y * width) +
            channel_offset_list[c] *
            static_cast<size_t>(width)));
          for (int x = 0; x < width; x++) {
            float val = reinterpret_cast<const float * const *>(
              images)[c][(y + start_y) * x_stride + x];

            tinyexr::swap4(&val);

            // line_ptr[x] = val;
            tinyexr::cpy4(line_ptr + x, &val);
          }
        }
      } else {
        if (err) {
          (*err) += "Invalid requested_pixel_type.\n";
        }
        return false;
      }
    } else if (channels[c].pixel_type == TINYEXR_PIXELTYPE_UINT) {
      for (int y = 0; y < num_lines; y++) {
        // Assume increasing Y
        unsigned int *line_ptr = reinterpret_cast<unsigned int *>(&buf.at(
          static_cast<size_t>(pixel_data_size * y * width) +
          channel_offset_list[c] * static_cast<size_t>(width)));
        for (int x = 0; x < width; x++) {
          unsigned int val = reinterpret_cast<const unsigned int * const *>(
            images)[c][(y + start_y) * x_stride + x];

          tinyexr::swap4(&val);

          // line_ptr[x] = val;
          tinyexr::cpy4(line_ptr + x, &val);
        }
      }
    }
  }

  if (compression_type == TINYEXR_COMPRESSIONTYPE_NONE) {
    // 4 byte: scan line
    // 4 byte: data size
    // ~     : pixel data(uncompressed)
    out_data.insert(out_data.end(), buf.begin(), buf.end());

  } else if ((compression_type == TINYEXR_COMPRESSIONTYPE_ZIPS) ||
    (compression_type == TINYEXR_COMPRESSIONTYPE_ZIP)) {
#if defined(TINYEXR_USE_MINIZ) && (TINYEXR_USE_MINIZ==1)
    std::vector<unsigned char> block(mz_compressBound(
      static_cast<unsigned long>(buf.size())));
#elif TINYEXR_USE_STB_ZLIB
    // there is no compressBound() function, so we use a value that
    // is grossly overestimated, but should always work
    std::vector<unsigned char> block(256 + 2 * buf.size());
#elif defined(TINYEXR_USE_NANOZLIB) && (TINYEXR_USE_NANOZLIB == 1)
    std::vector<unsigned char> block(nanoz_compressBound(
      static_cast<unsigned long>(buf.size())));
#else
    std::vector<unsigned char> block(
      compressBound(static_cast<uLong>(buf.size())));
#endif
    tinyexr::tinyexr_uint64 outSize = block.size();

    if (!tinyexr::CompressZip(&block.at(0), outSize,
                         reinterpret_cast<const unsigned char *>(&buf.at(0)),
                         static_cast<unsigned long>(buf.size()))) {
      if (err) {
        (*err) += "Zip compresssion failed.\n";
      }
      return false;
    }

    // 4 byte: scan line
    // 4 byte: data size
    // ~     : pixel data(compressed)
    unsigned int data_len = static_cast<unsigned int>(outSize);  // truncate

    out_data.insert(out_data.end(), block.begin(), block.begin() + data_len);

  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_RLE) {
    // (buf.size() * 3) / 2 would be enough.
    std::vector<unsigned char> block((buf.size() * 3) / 2);

    tinyexr::tinyexr_uint64 outSize = block.size();

    if (!tinyexr::CompressRle(&block.at(0), outSize,
                         reinterpret_cast<const unsigned char *>(&buf.at(0)),
                         static_cast<unsigned long>(buf.size()))) {
      if (err) {
        (*err) += "RLE compresssion failed.\n";
      }
      return false;
    }

    // 4 byte: scan line
    // 4 byte: data size
    // ~     : pixel data(compressed)
    unsigned int data_len = static_cast<unsigned int>(outSize);  // truncate
    out_data.insert(out_data.end(), block.begin(), block.begin() + data_len);

  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {
#if TINYEXR_USE_PIZ
    unsigned int bufLen =
      8192 + static_cast<unsigned int>(
        2 * static_cast<unsigned int>(
          buf.size()));  // @fixme { compute good bound. }
    std::vector<unsigned char> block(bufLen);
    unsigned int outSize = static_cast<unsigned int>(block.size());

    if (!CompressPiz(&block.at(0), &outSize,
                reinterpret_cast<const unsigned char *>(&buf.at(0)),
                buf.size(), channels, width, num_lines)) {
      if (err) {
        (*err) += "PIZ compresssion failed.\n";
      }
      return false;
    }

    // 4 byte: scan line
    // 4 byte: data size
    // ~     : pixel data(compressed)
    unsigned int data_len = outSize;
    out_data.insert(out_data.end(), block.begin(), block.begin() + data_len);

#else
    if (err) {
      (*err) += "PIZ compression is disabled in this build.\n";
    }
    return false;
#endif
  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
#if TINYEXR_USE_ZFP
    const ZFPCompressionParam* zfp_compression_param = reinterpret_cast<const ZFPCompressionParam*>(compression_param);
    std::vector<unsigned char> block;
    unsigned int outSize;

    tinyexr::CompressZfp(
      &block, &outSize, reinterpret_cast<const float *>(&buf.at(0)),
      width, num_lines, static_cast<int>(channels.size()), *zfp_compression_param);

    // 4 byte: scan line
    // 4 byte: data size
    // ~     : pixel data(compressed)
    unsigned int data_len = outSize;
    out_data.insert(out_data.end(), block.begin(), block.begin() + data_len);

#else
    if (err) {
      (*err) += "ZFP compression is disabled in this build.\n";
    }
    (void)compression_param;
    return false;
#endif
  } else {
    return false;
  }

  return true;
}

static int EncodeTiledLevel(const EXRImage* level_image, const EXRHeader* exr_header,
                            const std::vector<tinyexr::ChannelInfo>& channels,
                            std::vector<std::vector<unsigned char> >& data_list,
                            size_t start_index, // for data_list
                            int num_x_tiles, int num_y_tiles,
                            const std::vector<size_t>& channel_offset_list,
                            int pixel_data_size,
                            const void* compression_param, // must be set if zfp compression is enabled
                            std::string* err) {
  int num_tiles = num_x_tiles * num_y_tiles;
  if (num_tiles != level_image->num_tiles) {
    if (err) {
      (*err) += "Invalid number of tiles in argument.\n";
    }
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  if ((exr_header->tile_size_x > level_image->width || exr_header->tile_size_y > level_image->height) &&
      level_image->level_x == 0 && level_image->level_y == 0) {
      if (err) {
        (*err) += "Failed to encode tile data.\n";
      }
      return TINYEXR_ERROR_INVALID_DATA;
  }


#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
  std::atomic<bool> invalid_data(false);
#else
  bool invalid_data(false);
#endif

#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
  std::vector<std::thread> workers;
  std::atomic<int> tile_count(0);

  int num_threads = std::max(1, int(std::thread::hardware_concurrency()));
  if (num_threads > int(num_tiles)) {
    num_threads = int(num_tiles);
  }

  for (int t = 0; t < num_threads; t++) {
    workers.emplace_back(std::thread([&]() {
      int i = 0;
      while ((i = tile_count++) < num_tiles) {

#else
  // Use signed int since some OpenMP compiler doesn't allow unsigned type for
  // `parallel for`
#if TINYEXR_USE_OPENMP
#pragma omp parallel for
#endif
  for (int i = 0; i < num_tiles; i++) {

#endif
    size_t tile_idx = static_cast<size_t>(i);
    size_t data_idx = tile_idx + start_index;

    int x_tile = i % num_x_tiles;
    int y_tile = i / num_x_tiles;

    EXRTile& tile = level_image->tiles[tile_idx];

    const unsigned char* const* images =
      static_cast<const unsigned char* const*>(tile.images);

    data_list[data_idx].resize(5*sizeof(int));
    size_t data_header_size = data_list[data_idx].size();
    bool ret = EncodePixelData(data_list[data_idx],
                               images,
                               exr_header->compression_type,
                               0, // increasing y
                               tile.width,
                               exr_header->tile_size_y,
                               exr_header->tile_size_x,
                               0,
                               tile.height,
                               pixel_data_size,
                               channels,
                               channel_offset_list,
                               err, compression_param);
    if (!ret) {
      invalid_data = true;
      continue;
    }
    if (data_list[data_idx].size() <= data_header_size) {
      invalid_data = true;
      continue;
    }

    int data_len = static_cast<int>(data_list[data_idx].size() - data_header_size);
    //tileX, tileY, levelX, levelY // pixel_data_size(int)
    memcpy(&data_list[data_idx][0], &x_tile, sizeof(int));
    memcpy(&data_list[data_idx][4], &y_tile, sizeof(int));
    memcpy(&data_list[data_idx][8], &level_image->level_x, sizeof(int));
    memcpy(&data_list[data_idx][12], &level_image->level_y, sizeof(int));
    memcpy(&data_list[data_idx][16], &data_len, sizeof(int));

    swap4(reinterpret_cast<int*>(&data_list[data_idx][0]));
    swap4(reinterpret_cast<int*>(&data_list[data_idx][4]));
    swap4(reinterpret_cast<int*>(&data_list[data_idx][8]));
    swap4(reinterpret_cast<int*>(&data_list[data_idx][12]));
    swap4(reinterpret_cast<int*>(&data_list[data_idx][16]));

#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
  }
}));
    }

    for (auto &t : workers) {
      t.join();
    }
#else
    }  // omp parallel
#endif

  if (invalid_data) {
    if (err) {
      (*err) += "Failed to encode tile data.\n";
    }
    return TINYEXR_ERROR_INVALID_DATA;
  }
  return TINYEXR_SUCCESS;
}

static int NumScanlines(int compression_type) {
  int num_scanlines = 1;
  if (compression_type == TINYEXR_COMPRESSIONTYPE_ZIP) {
    num_scanlines = 16;
  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {
    num_scanlines = 32;
  } else if (compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
    num_scanlines = 16;
  }
  return num_scanlines;
}

static int EncodeChunk(const EXRImage* exr_image, const EXRHeader* exr_header,
                       const std::vector<ChannelInfo>& channels,
                       int num_blocks,
                       tinyexr_uint64 chunk_offset, // starting offset of current chunk
                       bool is_multipart,
                       OffsetData& offset_data, // output block offsets, must be initialized
                       std::vector<std::vector<unsigned char> >& data_list, // output
                       tinyexr_uint64& total_size, // output: ending offset of current chunk
                       std::string* err) {
  int num_scanlines = NumScanlines(exr_header->compression_type);

  data_list.resize(num_blocks);

  std::vector<size_t> channel_offset_list(
    static_cast<size_t>(exr_header->num_channels));

  int pixel_data_size = 0;
  {
    size_t channel_offset = 0;
    for (size_t c = 0; c < static_cast<size_t>(exr_header->num_channels); c++) {
      channel_offset_list[c] = channel_offset;
      if (channels[c].requested_pixel_type == TINYEXR_PIXELTYPE_HALF) {
        pixel_data_size += sizeof(unsigned short);
        channel_offset += sizeof(unsigned short);
      } else if (channels[c].requested_pixel_type ==
                 TINYEXR_PIXELTYPE_FLOAT) {
        pixel_data_size += sizeof(float);
        channel_offset += sizeof(float);
      } else if (channels[c].requested_pixel_type == TINYEXR_PIXELTYPE_UINT) {
        pixel_data_size += sizeof(unsigned int);
        channel_offset += sizeof(unsigned int);
      } else {
        if (err) {
          (*err) += "Invalid requested_pixel_type.\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }
    }
  }

  const void* compression_param = 0;
#if TINYEXR_USE_ZFP
  tinyexr::ZFPCompressionParam zfp_compression_param;

  // Use ZFP compression parameter from custom attributes(if such a parameter
  // exists)
  {
    std::string e;
    bool ret = tinyexr::FindZFPCompressionParam(
      &zfp_compression_param, exr_header->custom_attributes,
      exr_header->num_custom_attributes, &e);

    if (!ret) {
      // Use predefined compression parameter.
      zfp_compression_param.type = 0;
      zfp_compression_param.rate = 2;
    }
    compression_param = &zfp_compression_param;
  }
#endif

  tinyexr_uint64 offset = chunk_offset;
  tinyexr_uint64 doffset = is_multipart ? 4u : 0u;

  if (exr_image->tiles) {
    const EXRImage* level_image = exr_image;
    size_t block_idx = 0;
    //tinyexr::tinyexr_uint64 block_data_size = 0;
    int num_levels = (exr_header->tile_level_mode != TINYEXR_TILE_RIPMAP_LEVELS) ?
      offset_data.num_x_levels : (offset_data.num_x_levels * offset_data.num_y_levels);
    for (int level_index = 0; level_index < num_levels; ++level_index) {
      if (!level_image) {
        if (err) {
          (*err) += "Invalid number of tiled levels for EncodeChunk\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      int level_index_from_image = LevelIndex(level_image->level_x, level_image->level_y,
                                    exr_header->tile_level_mode, offset_data.num_x_levels);
      if (level_index_from_image < 0) {
        if (err) {
          (*err) += "Invalid tile level mode\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      if (level_index_from_image != level_index) {
        if (err) {
          (*err) += "Incorrect level ordering in tiled image\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }
      int num_y_tiles = int(offset_data.offsets[level_index].size());
      if (num_y_tiles <= 0) {
        if (err) {
          (*err) += "Invalid Y tile size\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      int num_x_tiles = int(offset_data.offsets[level_index][0].size());
      if (num_x_tiles <= 0) {
        if (err) {
          (*err) += "Invalid X tile size\n";
        }
        return TINYEXR_ERROR_INVALID_DATA;
      }

      std::string e;
      int ret = EncodeTiledLevel(level_image,
                                  exr_header,
                                  channels,
                                  data_list,
                                  block_idx,
                                  num_x_tiles,
                                  num_y_tiles,
                                  channel_offset_list,
                                  pixel_data_size,
                                  compression_param,
                                  &e);
      if (ret != TINYEXR_SUCCESS) {
        if (!e.empty() && err) {
          (*err) += e;
        }
        return ret;
      }

      for (size_t j = 0; j < static_cast<size_t>(num_y_tiles); ++j)
        for (size_t i = 0; i < static_cast<size_t>(num_x_tiles); ++i) {
          offset_data.offsets[level_index][j][i] = offset;
          swap8(reinterpret_cast<tinyexr_uint64*>(&offset_data.offsets[level_index][j][i]));
          offset += data_list[block_idx].size() + doffset;
          //block_data_size += data_list[block_idx].size();
          ++block_idx;
        }
      level_image = level_image->next_level;
    }
    TINYEXR_CHECK_AND_RETURN_C(static_cast<int>(block_idx) == num_blocks, TINYEXR_ERROR_INVALID_DATA);
    total_size = offset;
  } else { // scanlines
    std::vector<tinyexr::tinyexr_uint64>& offsets = offset_data.offsets[0][0];

#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
    std::atomic<bool> invalid_data(false);
    std::vector<std::thread> workers;
    std::atomic<int> block_count(0);

    int num_threads = std::min(std::max(1, int(std::thread::hardware_concurrency())), num_blocks);

    for (int t = 0; t < num_threads; t++) {
      workers.emplace_back(std::thread([&]() {
        int i = 0;
        while ((i = block_count++) < num_blocks) {

#else
    bool invalid_data(false);
#if TINYEXR_USE_OPENMP
#pragma omp parallel for
#endif
    for (int i = 0; i < num_blocks; i++) {

#endif
      int start_y = num_scanlines * i;
      int end_Y = (std::min)(num_scanlines * (i + 1), exr_image->height);
      int num_lines = end_Y - start_y;

      const unsigned char* const* images =
        static_cast<const unsigned char* const*>(exr_image->images);

      data_list[i].resize(2*sizeof(int));
      size_t data_header_size = data_list[i].size();

      bool ret = EncodePixelData(data_list[i],
                                 images,
                                 exr_header->compression_type,
                                 0, // increasing y
                                 exr_image->width,
                                 exr_image->height,
                                 exr_image->width,
                                 start_y,
                                 num_lines,
                                 pixel_data_size,
                                 channels,
                                 channel_offset_list,
                                 err,
                                 compression_param);
      if (!ret) {
        invalid_data = true;
        continue; // "break" cannot be used with OpenMP
      }
      if (data_list[i].size() <= data_header_size) {
        invalid_data = true;
        continue; // "break" cannot be used with OpenMP
      }
      int data_len = static_cast<int>(data_list[i].size() - data_header_size);
      memcpy(&data_list[i][0], &start_y, sizeof(int));
      memcpy(&data_list[i][4], &data_len, sizeof(int));

      swap4(reinterpret_cast<int*>(&data_list[i][0]));
      swap4(reinterpret_cast<int*>(&data_list[i][4]));
#if TINYEXR_HAS_CXX11 && (TINYEXR_USE_THREAD > 0)
        }
                                       }));
    }

    for (auto &t : workers) {
      t.join();
    }
#else
    }  // omp parallel
#endif

    if (invalid_data) {
      if (err) {
        (*err) += "Failed to encode scanline data.\n";
      }
      return TINYEXR_ERROR_INVALID_DATA;
    }

    for (size_t i = 0; i < static_cast<size_t>(num_blocks); i++) {
      offsets[i] = offset;
      tinyexr::swap8(reinterpret_cast<tinyexr::tinyexr_uint64 *>(&offsets[i]));
      offset += data_list[i].size() + doffset;
    }

    total_size = static_cast<size_t>(offset);
  }
  return TINYEXR_SUCCESS;
}

// can save a single or multi-part image (no deep* formats)
static size_t SaveEXRNPartImageToMemory(const EXRImage* exr_images,
                                        const EXRHeader** exr_headers,
                                        unsigned int num_parts,
                                        unsigned char** memory_out, const char** err) {
  if (exr_images == NULL || exr_headers == NULL || num_parts == 0 ||
      memory_out == NULL) {
    SetErrorMessage("Invalid argument for SaveEXRNPartImageToMemory",
                    err);
    return 0;
  }
  {
    for (unsigned int i = 0; i < num_parts; ++i) {
      if (exr_headers[i]->compression_type < 0) {
        SetErrorMessage("Invalid argument for SaveEXRNPartImageToMemory",
                        err);
        return 0;
      }
#if !TINYEXR_USE_PIZ
      if (exr_headers[i]->compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {
        SetErrorMessage("PIZ compression is not supported in this build",
                        err);
        return 0;
      }
#endif
      if (exr_headers[i]->compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
#if !TINYEXR_USE_ZFP
        SetErrorMessage("ZFP compression is not supported in this build",
                        err);
        return 0;
#else
        // All channels must be fp32.
        // No fp16 support in ZFP atm(as of 2023 June)
        // https://github.com/LLNL/fpzip/issues/2
        for (int c = 0; c < exr_headers[i]->num_channels; ++c) {
          if (exr_headers[i]->requested_pixel_types[c] != TINYEXR_PIXELTYPE_FLOAT) {
            SetErrorMessage("Pixel type must be FLOAT for ZFP compression",
                            err);
            return 0;
          }
        }
#endif
      }
    }
  }

  std::vector<unsigned char> memory;

  // Header
  {
    const char header[] = { 0x76, 0x2f, 0x31, 0x01 };
    memory.insert(memory.end(), header, header + 4);
  }

  // Version
  // using value from the first header
  int long_name = exr_headers[0]->long_name;
  {
    char marker[] = { 2, 0, 0, 0 };
    /* @todo
    if (exr_header->non_image) {
    marker[1] |= 0x8;
    }
    */
    // tiled
    if (num_parts == 1 && exr_images[0].tiles) {
      marker[1] |= 0x2;
    }
    // long_name
    if (long_name) {
      marker[1] |= 0x4;
    }
    // multipart
    if (num_parts > 1) {
      marker[1] |= 0x10;
    }
    memory.insert(memory.end(), marker, marker + 4);
  }

  int total_chunk_count = 0;
  std::vector<int> chunk_count(num_parts);
  std::vector<OffsetData> offset_data(num_parts);
  for (unsigned int i = 0; i < num_parts; ++i) {
    if (!exr_images[i].tiles) {
      int num_scanlines = NumScanlines(exr_headers[i]->compression_type);
      chunk_count[i] =
        (exr_images[i].height + num_scanlines - 1) / num_scanlines;
      InitSingleResolutionOffsets(offset_data[i], chunk_count[i]);
      total_chunk_count += chunk_count[i];
    } else {
      {
        std::vector<int> num_x_tiles, num_y_tiles;
        if (!PrecalculateTileInfo(num_x_tiles, num_y_tiles, exr_headers[i])) {
          SetErrorMessage("Failed to precalculate Tile info",
                          err);
          return TINYEXR_ERROR_INVALID_DATA;
        }
        int ntiles = InitTileOffsets(offset_data[i], exr_headers[i], num_x_tiles, num_y_tiles);
        if (ntiles > 0) {
          chunk_count[i] = ntiles;
        } else {
          SetErrorMessage("Failed to compute Tile offsets",
                          err);
          return TINYEXR_ERROR_INVALID_DATA;
          
        }
        total_chunk_count += chunk_count[i];
      }
    }
  }
  // Write attributes to memory buffer.
  std::vector< std::vector<tinyexr::ChannelInfo> > channels(num_parts);
  {
    std::set<std::string> partnames;
    for (unsigned int i = 0; i < num_parts; ++i) {
      //channels
      {
        std::vector<unsigned char> data;

        for (int c = 0; c < exr_headers[i]->num_channels; c++) {
          tinyexr::ChannelInfo info;
          info.p_linear = 0;
          info.pixel_type = exr_headers[i]->pixel_types[c];
          info.requested_pixel_type = exr_headers[i]->requested_pixel_types[c];
          info.x_sampling = 1;
          info.y_sampling = 1;
          info.name = std::string(exr_headers[i]->channels[c].name);
          channels[i].push_back(info);
        }

        tinyexr::WriteChannelInfo(data, channels[i]);

        tinyexr::WriteAttributeToMemory(&memory, "channels", "chlist", &data.at(0),
                                        static_cast<int>(data.size()));
      }

      {
        int comp = exr_headers[i]->compression_type;
        swap4(&comp);
        WriteAttributeToMemory(
          &memory, "compression", "compression",
          reinterpret_cast<const unsigned char*>(&comp), 1);
      }

      {
        int data[4] = { 0, 0, exr_images[i].width - 1, exr_images[i].height - 1 };
        swap4(&data[0]);
        swap4(&data[1]);
        swap4(&data[2]);
        swap4(&data[3]);
        WriteAttributeToMemory(
          &memory, "dataWindow", "box2i",
          reinterpret_cast<const unsigned char*>(data), sizeof(int) * 4);

        int data0[4] = { 0, 0, exr_images[0].width - 1, exr_images[0].height - 1 };
        swap4(&data0[0]);
        swap4(&data0[1]);
        swap4(&data0[2]);
        swap4(&data0[3]);
        // Note: must be the same across parts (currently, using value from the first header)
        WriteAttributeToMemory(
          &memory, "displayWindow", "box2i",
          reinterpret_cast<const unsigned char*>(data0), sizeof(int) * 4);
      }

      {
        unsigned char line_order = 0;  // @fixme { read line_order from EXRHeader }
        WriteAttributeToMemory(&memory, "lineOrder", "lineOrder",
                               &line_order, 1);
      }

      {
        // Note: must be the same across parts
        float aspectRatio = 1.0f;
        swap4(&aspectRatio);
        WriteAttributeToMemory(
          &memory, "pixelAspectRatio", "float",
          reinterpret_cast<const unsigned char*>(&aspectRatio), sizeof(float));
      }

      {
        float center[2] = { 0.0f, 0.0f };
        swap4(&center[0]);
        swap4(&center[1]);
        WriteAttributeToMemory(
          &memory, "screenWindowCenter", "v2f",
          reinterpret_cast<const unsigned char*>(center), 2 * sizeof(float));
      }

      {
        float w = 1.0f;
        swap4(&w);
        WriteAttributeToMemory(&memory, "screenWindowWidth", "float",
                               reinterpret_cast<const unsigned char*>(&w),
                               sizeof(float));
      }

      if (exr_images[i].tiles) {
        unsigned char tile_mode = static_cast<unsigned char>(exr_headers[i]->tile_level_mode & 0x3);
        if (exr_headers[i]->tile_rounding_mode) tile_mode |= (1u << 4u);
        //unsigned char data[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        unsigned int datai[3] = { 0, 0, 0 };
        unsigned char* data = reinterpret_cast<unsigned char*>(&datai[0]);
        datai[0] = static_cast<unsigned int>(exr_headers[i]->tile_size_x);
        datai[1] = static_cast<unsigned int>(exr_headers[i]->tile_size_y);
        data[8] = tile_mode;
        swap4(reinterpret_cast<unsigned int*>(&data[0]));
        swap4(reinterpret_cast<unsigned int*>(&data[4]));
        WriteAttributeToMemory(
          &memory, "tiles", "tiledesc",
          reinterpret_cast<const unsigned char*>(data), 9);
      }

      // must be present for multi-part files - according to spec.
      if (num_parts > 1) {
        // name
        {
          size_t len = 0;
          if ((len = strlen(exr_headers[i]->name)) > 0) {
#if TINYEXR_HAS_CXX11
            partnames.emplace(exr_headers[i]->name);
#else
            partnames.insert(std::string(exr_headers[i]->name));
#endif
            if (partnames.size() != i + 1) {
              SetErrorMessage("'name' attributes must be unique for a multi-part file", err);
              return 0;
            }
            WriteAttributeToMemory(
              &memory, "name", "string",
              reinterpret_cast<const unsigned char*>(exr_headers[i]->name),
              static_cast<int>(len));
          } else {
            SetErrorMessage("Invalid 'name' attribute for a multi-part file", err);
            return 0;
          }
        }
        // type
        {
          const char* type = "scanlineimage";
          if (exr_images[i].tiles) type = "tiledimage";
          WriteAttributeToMemory(
            &memory, "type", "string",
            reinterpret_cast<const unsigned char*>(type),
            static_cast<int>(strlen(type)));
        }
        // chunkCount
        {
          WriteAttributeToMemory(
            &memory, "chunkCount", "int",
            reinterpret_cast<const unsigned char*>(&chunk_count[i]),
            4);
        }
      }

      // Custom attributes
      if (exr_headers[i]->num_custom_attributes > 0) {
        for (int j = 0; j < exr_headers[i]->num_custom_attributes; j++) {
          tinyexr::WriteAttributeToMemory(
            &memory, exr_headers[i]->custom_attributes[j].name,
            exr_headers[i]->custom_attributes[j].type,
            reinterpret_cast<const unsigned char*>(
              exr_headers[i]->custom_attributes[j].value),
            exr_headers[i]->custom_attributes[j].size);
        }
      }

      {  // end of header
        memory.push_back(0);
      }
    }
  }
  if (num_parts > 1) {
    // end of header list
    memory.push_back(0);
  }

  tinyexr_uint64 chunk_offset = memory.size() + size_t(total_chunk_count) * sizeof(tinyexr_uint64);

  tinyexr_uint64 total_size = 0;
  std::vector< std::vector< std::vector<unsigned char> > > data_lists(num_parts);
  for (unsigned int i = 0; i < num_parts; ++i) {
    std::string e;
    int ret = EncodeChunk(&exr_images[i], exr_headers[i],
                          channels[i],
                          chunk_count[i],
                          // starting offset of current chunk after part-number
                          chunk_offset,
                          num_parts > 1,
                          offset_data[i], // output: block offsets, must be initialized
                          data_lists[i], // output
                          total_size, // output
                          &e);
    if (ret != TINYEXR_SUCCESS) {
      if (!e.empty()) {
        tinyexr::SetErrorMessage(e, err);
      }
      return 0;
    }
    chunk_offset = total_size;
  }

  // Allocating required memory
  if (total_size == 0) { // something went wrong
    tinyexr::SetErrorMessage("Output memory size is zero", err);
    return TINYEXR_ERROR_INVALID_DATA;
  }
  (*memory_out) = static_cast<unsigned char*>(malloc(size_t(total_size)));

  // Writing header
  memcpy((*memory_out), &memory[0], memory.size());
  unsigned char* memory_ptr = *memory_out + memory.size();
  size_t sum = memory.size();

  // Writing offset data for chunks
  for (unsigned int i = 0; i < num_parts; ++i) {
    if (exr_images[i].tiles) {
      const EXRImage* level_image = &exr_images[i];
      int num_levels = (exr_headers[i]->tile_level_mode != TINYEXR_TILE_RIPMAP_LEVELS) ?
        offset_data[i].num_x_levels : (offset_data[i].num_x_levels * offset_data[i].num_y_levels);
      for (int level_index = 0; level_index < num_levels; ++level_index) {
        for (size_t j = 0; j < offset_data[i].offsets[level_index].size(); ++j) {
          size_t num_bytes = sizeof(tinyexr_uint64) * offset_data[i].offsets[level_index][j].size();
          sum += num_bytes;
          if (sum > total_size) {
            tinyexr::SetErrorMessage("Invalid offset bytes in Tiled Part image.", err);
            return TINYEXR_ERROR_INVALID_DATA;
          }

          memcpy(memory_ptr,
                 reinterpret_cast<unsigned char*>(&offset_data[i].offsets[level_index][j][0]),
                 num_bytes);
          memory_ptr += num_bytes;
        }
        level_image = level_image->next_level;
      }
    } else {
      size_t num_bytes = sizeof(tinyexr::tinyexr_uint64) * static_cast<size_t>(chunk_count[i]);
      sum += num_bytes;
      if (sum > total_size) {
        tinyexr::SetErrorMessage("Invalid offset bytes in Part image.", err);
        return TINYEXR_ERROR_INVALID_DATA;
      }
      std::vector<tinyexr::tinyexr_uint64>& offsets = offset_data[i].offsets[0][0];
      memcpy(memory_ptr, reinterpret_cast<unsigned char*>(&offsets[0]), num_bytes);
      memory_ptr += num_bytes;
    }
  }

  // Writing chunk data
  for (unsigned int i = 0; i < num_parts; ++i) {
    for (size_t j = 0; j < static_cast<size_t>(chunk_count[i]); ++j) {
      if (num_parts > 1) {
        sum += 4;
        if (sum > total_size) {
          tinyexr::SetErrorMessage("Buffer overrun in reading Part image chunk data.", err);
          return TINYEXR_ERROR_INVALID_DATA;
        }
        unsigned int part_number = i;
        swap4(&part_number);
        memcpy(memory_ptr, &part_number, 4);
        memory_ptr += 4;
      }
      sum += data_lists[i][j].size();
      if (sum > total_size) {
        tinyexr::SetErrorMessage("Buffer overrun in reading Part image chunk data.", err);
        return TINYEXR_ERROR_INVALID_DATA;
      }
      memcpy(memory_ptr, &data_lists[i][j][0], data_lists[i][j].size());
      memory_ptr += data_lists[i][j].size();
    }
  }

  if (sum != total_size) {
    tinyexr::SetErrorMessage("Corrupted Part image chunk data.", err);
    return TINYEXR_ERROR_INVALID_DATA;
  }

  return size_t(total_size);  // OK
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

} // tinyexr

size_t SaveEXRImageToMemory(const EXRImage* exr_image,
                             const EXRHeader* exr_header,
                             unsigned char** memory_out, const char** err) {
  return tinyexr::SaveEXRNPartImageToMemory(exr_image, &exr_header, 1, memory_out, err);
}

int SaveEXRImageToFile(const EXRImage *exr_image, const EXRHeader *exr_header,
                       const char *filename, const char **err) {
  if (exr_image == NULL || filename == NULL ||
      exr_header->compression_type < 0) {
    tinyexr::SetErrorMessage("Invalid argument for SaveEXRImageToFile", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

#if !TINYEXR_USE_PIZ
  if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_PIZ) {
    tinyexr::SetErrorMessage("PIZ compression is not supported in this build",
                             err);
    return TINYEXR_ERROR_UNSUPPORTED_FEATURE;
  }
#endif

#if !TINYEXR_USE_ZFP
  if (exr_header->compression_type == TINYEXR_COMPRESSIONTYPE_ZFP) {
    tinyexr::SetErrorMessage("ZFP compression is not supported in this build",
                             err);
    return TINYEXR_ERROR_UNSUPPORTED_FEATURE;
  }
#endif

  FILE *fp = NULL;
#ifdef _WIN32
#if defined(_MSC_VER) || (defined(MINGW_HAS_SECURE_API) && MINGW_HAS_SECURE_API) // MSVC, MinGW GCC, or Clang
  errno_t errcode =
      _wfopen_s(&fp, tinyexr::UTF8ToWchar(filename).c_str(), L"wb");
  if (errcode != 0) {
    tinyexr::SetErrorMessage("Cannot write a file: " + std::string(filename),
                             err);
    return TINYEXR_ERROR_CANT_WRITE_FILE;
  }
#else
  // Unknown compiler or MinGW without MINGW_HAS_SECURE_API.
  fp = fopen(filename, "wb");
#endif
#else
  fp = fopen(filename, "wb");
#endif
  if (!fp) {
    tinyexr::SetErrorMessage("Cannot write a file: " + std::string(filename),
                             err);
    return TINYEXR_ERROR_CANT_WRITE_FILE;
  }

  unsigned char *mem = NULL;
  size_t mem_size = SaveEXRImageToMemory(exr_image, exr_header, &mem, err);
  if (mem_size == 0) {
    fclose(fp);
    return TINYEXR_ERROR_SERIALIZATION_FAILED;
  }

  size_t written_size = 0;
  if ((mem_size > 0) && mem) {
    written_size = fwrite(mem, 1, mem_size, fp);
  }
  free(mem);

  fclose(fp);

  if (written_size != mem_size) {
    tinyexr::SetErrorMessage("Cannot write a file", err);
    return TINYEXR_ERROR_CANT_WRITE_FILE;
  }

  return TINYEXR_SUCCESS;
}

size_t SaveEXRMultipartImageToMemory(const EXRImage* exr_images,
                                     const EXRHeader** exr_headers,
                                     unsigned int num_parts,
                                     unsigned char** memory_out, const char** err) {
  if (exr_images == NULL || exr_headers == NULL || num_parts < 2 ||
      memory_out == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for SaveEXRNPartImageToMemory",
                              err);
    return 0;
  }
  return tinyexr::SaveEXRNPartImageToMemory(exr_images, exr_headers, num_parts, memory_out, err);
}

int SaveEXRMultipartImageToFile(const EXRImage* exr_images,
                                const EXRHeader** exr_headers,
                                unsigned int num_parts,
                                const char* filename,
                                const char** err) {
  if (exr_images == NULL || exr_headers == NULL || num_parts < 2) {
    tinyexr::SetErrorMessage("Invalid argument for SaveEXRMultipartImageToFile",
                              err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  FILE *fp = NULL;
#ifdef _WIN32
#if defined(_MSC_VER) || (defined(MINGW_HAS_SECURE_API) && MINGW_HAS_SECURE_API) // MSVC, MinGW GCC, or Clang.
  errno_t errcode =
    _wfopen_s(&fp, tinyexr::UTF8ToWchar(filename).c_str(), L"wb");
  if (errcode != 0) {
    tinyexr::SetErrorMessage("Cannot write a file: " + std::string(filename),
                             err);
    return TINYEXR_ERROR_CANT_WRITE_FILE;
  }
#else
  // Unknown compiler or MinGW without MINGW_HAS_SECURE_API.
  fp = fopen(filename, "wb");
#endif
#else
  fp = fopen(filename, "wb");
#endif
  if (!fp) {
    tinyexr::SetErrorMessage("Cannot write a file: " + std::string(filename),
                             err);
    return TINYEXR_ERROR_CANT_WRITE_FILE;
  }

  unsigned char *mem = NULL;
  size_t mem_size = SaveEXRMultipartImageToMemory(exr_images, exr_headers, num_parts, &mem, err);
  if (mem_size == 0) {
    fclose(fp);
    return TINYEXR_ERROR_SERIALIZATION_FAILED;
  }

  size_t written_size = 0;
  if ((mem_size > 0) && mem) {
    written_size = fwrite(mem, 1, mem_size, fp);
  }
  free(mem);

  fclose(fp);

  if (written_size != mem_size) {
    tinyexr::SetErrorMessage("Cannot write a file", err);
    return TINYEXR_ERROR_CANT_WRITE_FILE;
  }

  return TINYEXR_SUCCESS;
}

int LoadDeepEXR(DeepImage *deep_image, const char *filename, const char **err) {
  if (deep_image == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for LoadDeepEXR", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  MemoryMappedFile file(filename);
  if (!file.valid()) {
    tinyexr::SetErrorMessage("Cannot read file " + std::string(filename), err);
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  if (file.size == 0) {
    tinyexr::SetErrorMessage("File size is zero : " + std::string(filename),
                             err);
    return TINYEXR_ERROR_INVALID_FILE;
  }

  const char *head = reinterpret_cast<const char *>(file.data);
  const char *marker = reinterpret_cast<const char *>(file.data);

  // Header check.
  {
    const char header[] = {0x76, 0x2f, 0x31, 0x01};

    if (memcmp(marker, header, 4) != 0) {
      tinyexr::SetErrorMessage("Invalid magic number", err);
      return TINYEXR_ERROR_INVALID_MAGIC_NUMBER;
    }
    marker += 4;
  }

  // Version, scanline.
  {
    // ver 2.0, scanline, deep bit on(0x800)
    // must be [2, 0, 0, 0]
    if (marker[0] != 2 || marker[1] != 8 || marker[2] != 0 || marker[3] != 0) {
      tinyexr::SetErrorMessage("Unsupported version or scanline", err);
      return TINYEXR_ERROR_UNSUPPORTED_FORMAT;
    }

    marker += 4;
  }

  int dx = -1;
  int dy = -1;
  int dw = -1;
  int dh = -1;
  int num_scanline_blocks = 1;  // 16 for ZIP compression.
  int compression_type = -1;
  int num_channels = -1;
  std::vector<tinyexr::ChannelInfo> channels;

  // Read attributes
  size_t size = file.size - tinyexr::kEXRVersionSize;
  for (;;) {
    if (0 == size) {
      return TINYEXR_ERROR_INVALID_DATA;
    } else if (marker[0] == '\0') {
      marker++;
      size--;
      break;
    }

    std::string attr_name;
    std::string attr_type;
    std::vector<unsigned char> data;
    size_t marker_size;
    if (!tinyexr::ReadAttribute(&attr_name, &attr_type, &data, &marker_size,
                                marker, size)) {
      std::stringstream ss;
      ss << "Failed to parse attribute\n";
      tinyexr::SetErrorMessage(ss.str(), err);
      return TINYEXR_ERROR_INVALID_DATA;
    }
    marker += marker_size;
    size -= marker_size;

    if (attr_name.compare("compression") == 0) {
      compression_type = data[0];
      if (compression_type > TINYEXR_COMPRESSIONTYPE_PIZ) {
        std::stringstream ss;
        ss << "Unsupported compression type : " << compression_type;
        tinyexr::SetErrorMessage(ss.str(), err);
        return TINYEXR_ERROR_UNSUPPORTED_FORMAT;
      }

      if (compression_type == TINYEXR_COMPRESSIONTYPE_ZIP) {
        num_scanline_blocks = 16;
      }

    } else if (attr_name.compare("channels") == 0) {
      // name: zero-terminated string, from 1 to 255 bytes long
      // pixel type: int, possible values are: UINT = 0 HALF = 1 FLOAT = 2
      // pLinear: unsigned char, possible values are 0 and 1
      // reserved: three chars, should be zero
      // xSampling: int
      // ySampling: int

      if (!tinyexr::ReadChannelInfo(channels, data)) {
        tinyexr::SetErrorMessage("Failed to parse channel info", err);
        return TINYEXR_ERROR_INVALID_DATA;
      }

      num_channels = static_cast<int>(channels.size());

      if (num_channels < 1) {
        tinyexr::SetErrorMessage("Invalid channels format", err);
        return TINYEXR_ERROR_INVALID_DATA;
      }

    } else if (attr_name.compare("dataWindow") == 0) {
      memcpy(&dx, &data.at(0), sizeof(int));
      memcpy(&dy, &data.at(4), sizeof(int));
      memcpy(&dw, &data.at(8), sizeof(int));
      memcpy(&dh, &data.at(12), sizeof(int));
      tinyexr::swap4(&dx);
      tinyexr::swap4(&dy);
      tinyexr::swap4(&dw);
      tinyexr::swap4(&dh);

    } else if (attr_name.compare("displayWindow") == 0) {
      int x;
      int y;
      int w;
      int h;
      memcpy(&x, &data.at(0), sizeof(int));
      memcpy(&y, &data.at(4), sizeof(int));
      memcpy(&w, &data.at(8), sizeof(int));
      memcpy(&h, &data.at(12), sizeof(int));
      tinyexr::swap4(&x);
      tinyexr::swap4(&y);
      tinyexr::swap4(&w);
      tinyexr::swap4(&h);
    }
  }

  TINYEXR_CHECK_AND_RETURN_C(dx >= 0, TINYEXR_ERROR_INVALID_DATA);
  TINYEXR_CHECK_AND_RETURN_C(dy >= 0, TINYEXR_ERROR_INVALID_DATA);
  TINYEXR_CHECK_AND_RETURN_C(dw >= 0, TINYEXR_ERROR_INVALID_DATA);
  TINYEXR_CHECK_AND_RETURN_C(dh >= 0, TINYEXR_ERROR_INVALID_DATA);
  TINYEXR_CHECK_AND_RETURN_C(num_channels >= 1, TINYEXR_ERROR_INVALID_DATA);

  int data_width = dw - dx + 1;
  int data_height = dh - dy + 1;

  // Read offset tables.
  int num_blocks = data_height / num_scanline_blocks;
  if (num_blocks * num_scanline_blocks < data_height) {
    num_blocks++;
  }

  std::vector<tinyexr::tinyexr_int64> offsets(static_cast<size_t>(num_blocks));

  for (size_t y = 0; y < static_cast<size_t>(num_blocks); y++) {
    tinyexr::tinyexr_int64 offset;
    memcpy(&offset, marker, sizeof(tinyexr::tinyexr_int64));
    tinyexr::swap8(reinterpret_cast<tinyexr::tinyexr_uint64 *>(&offset));
    marker += sizeof(tinyexr::tinyexr_int64);  // = 8
    offsets[y] = offset;
  }

#if TINYEXR_USE_PIZ
  if ((compression_type == TINYEXR_COMPRESSIONTYPE_NONE) ||
      (compression_type == TINYEXR_COMPRESSIONTYPE_RLE) ||
      (compression_type == TINYEXR_COMPRESSIONTYPE_ZIPS) ||
      (compression_type == TINYEXR_COMPRESSIONTYPE_ZIP) ||
      (compression_type == TINYEXR_COMPRESSIONTYPE_PIZ)) {
#else
  if ((compression_type == TINYEXR_COMPRESSIONTYPE_NONE) ||
      (compression_type == TINYEXR_COMPRESSIONTYPE_RLE) ||
      (compression_type == TINYEXR_COMPRESSIONTYPE_ZIPS) ||
      (compression_type == TINYEXR_COMPRESSIONTYPE_ZIP)) {
#endif
    // OK
  } else {
    tinyexr::SetErrorMessage("Unsupported compression format", err);
    return TINYEXR_ERROR_UNSUPPORTED_FORMAT;
  }

  deep_image->image = static_cast<float ***>(
      malloc(sizeof(float **) * static_cast<size_t>(num_channels)));
  for (int c = 0; c < num_channels; c++) {
    deep_image->image[c] = static_cast<float **>(
        malloc(sizeof(float *) * static_cast<size_t>(data_height)));
    for (int y = 0; y < data_height; y++) {
    }
  }

  deep_image->offset_table = static_cast<int **>(
      malloc(sizeof(int *) * static_cast<size_t>(data_height)));
  for (int y = 0; y < data_height; y++) {
    deep_image->offset_table[y] = static_cast<int *>(
        malloc(sizeof(int) * static_cast<size_t>(data_width)));
  }

  for (size_t y = 0; y < static_cast<size_t>(num_blocks); y++) {
    const unsigned char *data_ptr =
        reinterpret_cast<const unsigned char *>(head + offsets[y]);

    // int: y coordinate
    // int64: packed size of pixel offset table
    // int64: packed size of sample data
    // int64: unpacked size of sample data
    // compressed pixel offset table
    // compressed sample data
    int line_no;
    tinyexr::tinyexr_int64 packedOffsetTableSize;
    tinyexr::tinyexr_int64 packedSampleDataSize;
    tinyexr::tinyexr_int64 unpackedSampleDataSize;
    memcpy(&line_no, data_ptr, sizeof(int));
    memcpy(&packedOffsetTableSize, data_ptr + 4,
           sizeof(tinyexr::tinyexr_int64));
    memcpy(&packedSampleDataSize, data_ptr + 12,
           sizeof(tinyexr::tinyexr_int64));
    memcpy(&unpackedSampleDataSize, data_ptr + 20,
           sizeof(tinyexr::tinyexr_int64));

    tinyexr::swap4(&line_no);
    tinyexr::swap8(
        reinterpret_cast<tinyexr::tinyexr_uint64 *>(&packedOffsetTableSize));
    tinyexr::swap8(
        reinterpret_cast<tinyexr::tinyexr_uint64 *>(&packedSampleDataSize));
    tinyexr::swap8(
        reinterpret_cast<tinyexr::tinyexr_uint64 *>(&unpackedSampleDataSize));

    std::vector<int> pixelOffsetTable(static_cast<size_t>(data_width));

    // decode pixel offset table.
    {
      unsigned long dstLen =
          static_cast<unsigned long>(pixelOffsetTable.size() * sizeof(int));
      if (!tinyexr::DecompressZip(
              reinterpret_cast<unsigned char *>(&pixelOffsetTable.at(0)),
              &dstLen, data_ptr + 28,
              static_cast<unsigned long>(packedOffsetTableSize))) {
        return false;
      }

      TINYEXR_CHECK_AND_RETURN_C(dstLen == pixelOffsetTable.size() * sizeof(int), TINYEXR_ERROR_INVALID_DATA);
      for (size_t i = 0; i < static_cast<size_t>(data_width); i++) {
        deep_image->offset_table[y][i] = pixelOffsetTable[i];
      }
    }

    std::vector<unsigned char> sample_data(
        static_cast<size_t>(unpackedSampleDataSize));

    // decode sample data.
    {
      unsigned long dstLen = static_cast<unsigned long>(unpackedSampleDataSize);
      if (dstLen) {
        if (!tinyexr::DecompressZip(
                reinterpret_cast<unsigned char *>(&sample_data.at(0)), &dstLen,
                data_ptr + 28 + packedOffsetTableSize,
                static_cast<unsigned long>(packedSampleDataSize))) {
          return false;
        }
        TINYEXR_CHECK_AND_RETURN_C(dstLen == static_cast<unsigned long>(unpackedSampleDataSize), TINYEXR_ERROR_INVALID_DATA);
      }
    }

    // decode sample
    int sampleSize = -1;
    std::vector<int> channel_offset_list(static_cast<size_t>(num_channels));
    {
      int channel_offset = 0;
      for (size_t i = 0; i < static_cast<size_t>(num_channels); i++) {
        channel_offset_list[i] = channel_offset;
        if (channels[i].pixel_type == TINYEXR_PIXELTYPE_UINT) {  // UINT
          channel_offset += 4;
        } else if (channels[i].pixel_type == TINYEXR_PIXELTYPE_HALF) {  // half
          channel_offset += 2;
        } else if (channels[i].pixel_type ==
                   TINYEXR_PIXELTYPE_FLOAT) {  // float
          channel_offset += 4;
        } else {
          tinyexr::SetErrorMessage("Invalid pixel_type in chnnels.", err);
          return TINYEXR_ERROR_INVALID_DATA;
        }
      }
      sampleSize = channel_offset;
    }
    TINYEXR_CHECK_AND_RETURN_C(sampleSize >= 2, TINYEXR_ERROR_INVALID_DATA);

    TINYEXR_CHECK_AND_RETURN_C(static_cast<size_t>(
               pixelOffsetTable[static_cast<size_t>(data_width - 1)] *
               sampleSize) == sample_data.size(), TINYEXR_ERROR_INVALID_DATA);
    int samples_per_line = static_cast<int>(sample_data.size()) / sampleSize;

    //
    // Alloc memory
    //

    //
    // pixel data is stored as image[channels][pixel_samples]
    //
    {
      tinyexr::tinyexr_uint64 data_offset = 0;
      for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
        deep_image->image[c][y] = static_cast<float *>(
            malloc(sizeof(float) * static_cast<size_t>(samples_per_line)));

        if (channels[c].pixel_type == 0) {  // UINT
          for (size_t x = 0; x < static_cast<size_t>(samples_per_line); x++) {
            unsigned int ui;
            unsigned int *src_ptr = reinterpret_cast<unsigned int *>(
                &sample_data.at(size_t(data_offset) + x * sizeof(int)));
            tinyexr::cpy4(&ui, src_ptr);
            deep_image->image[c][y][x] = static_cast<float>(ui);  // @fixme
          }
          data_offset +=
              sizeof(unsigned int) * static_cast<size_t>(samples_per_line);
        } else if (channels[c].pixel_type == 1) {  // half
          for (size_t x = 0; x < static_cast<size_t>(samples_per_line); x++) {
            tinyexr::FP16 f16;
            const unsigned short *src_ptr = reinterpret_cast<unsigned short *>(
                &sample_data.at(size_t(data_offset) + x * sizeof(short)));
            tinyexr::cpy2(&(f16.u), src_ptr);
            tinyexr::FP32 f32 = half_to_float(f16);
            deep_image->image[c][y][x] = f32.f;
          }
          data_offset += sizeof(short) * static_cast<size_t>(samples_per_line);
        } else {  // float
          for (size_t x = 0; x < static_cast<size_t>(samples_per_line); x++) {
            float f;
            const float *src_ptr = reinterpret_cast<float *>(
                &sample_data.at(size_t(data_offset) + x * sizeof(float)));
            tinyexr::cpy4(&f, src_ptr);
            deep_image->image[c][y][x] = f;
          }
          data_offset += sizeof(float) * static_cast<size_t>(samples_per_line);
        }
      }
    }
  }  // y

  deep_image->width = data_width;
  deep_image->height = data_height;

  deep_image->channel_names = static_cast<const char **>(
      malloc(sizeof(const char *) * static_cast<size_t>(num_channels)));
  for (size_t c = 0; c < static_cast<size_t>(num_channels); c++) {
#ifdef _WIN32
    deep_image->channel_names[c] = _strdup(channels[c].name.c_str());
#else
    deep_image->channel_names[c] = strdup(channels[c].name.c_str());
#endif
  }
  deep_image->num_channels = num_channels;

  return TINYEXR_SUCCESS;
}

void InitEXRImage(EXRImage *exr_image) {
  if (exr_image == NULL) {
    return;
  }

  exr_image->width = 0;
  exr_image->height = 0;
  exr_image->num_channels = 0;

  exr_image->images = NULL;
  exr_image->tiles = NULL;
  exr_image->next_level = NULL;
  exr_image->level_x = 0;
  exr_image->level_y = 0;

  exr_image->num_tiles = 0;
}

void FreeEXRErrorMessage(const char *msg) {
  if (msg) {
    free(reinterpret_cast<void *>(const_cast<char *>(msg)));
  }
  return;
}

void InitEXRHeader(EXRHeader *exr_header) {
  if (exr_header == NULL) {
    return;
  }

  memset(exr_header, 0, sizeof(EXRHeader));
}

int FreeEXRHeader(EXRHeader *exr_header) {
  if (exr_header == NULL) {
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  if (exr_header->channels) {
    free(exr_header->channels);
  }

  if (exr_header->pixel_types) {
    free(exr_header->pixel_types);
  }

  if (exr_header->requested_pixel_types) {
    free(exr_header->requested_pixel_types);
  }

  for (int i = 0; i < exr_header->num_custom_attributes; i++) {
    if (exr_header->custom_attributes[i].value) {
      free(exr_header->custom_attributes[i].value);
    }
  }

  if (exr_header->custom_attributes) {
    free(exr_header->custom_attributes);
  }

  EXRSetNameAttr(exr_header, NULL);

  return TINYEXR_SUCCESS;
}

void EXRSetNameAttr(EXRHeader* exr_header, const char* name) {
  if (exr_header == NULL) {
    return;
  }
  memset(exr_header->name, 0, 256);
  if (name != NULL) {
    size_t len = std::min(strlen(name), size_t(255));
    if (len) {
      memcpy(exr_header->name, name, len);
    }
  }
}

int EXRNumLevels(const EXRImage* exr_image) {
  if (exr_image == NULL) return 0;
  if(exr_image->images) return 1; // scanlines
  int levels = 1;
  const EXRImage* level_image = exr_image;
  while((level_image = level_image->next_level)) ++levels;
  return levels;
}

int FreeEXRImage(EXRImage *exr_image) {
  if (exr_image == NULL) {
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  if (exr_image->next_level) {
    FreeEXRImage(exr_image->next_level);
    delete exr_image->next_level;
  }

  for (int i = 0; i < exr_image->num_channels; i++) {
    if (exr_image->images && exr_image->images[i]) {
      free(exr_image->images[i]);
    }
  }

  if (exr_image->images) {
    free(exr_image->images);
  }

  if (exr_image->tiles) {
    for (int tid = 0; tid < exr_image->num_tiles; tid++) {
      for (int i = 0; i < exr_image->num_channels; i++) {
        if (exr_image->tiles[tid].images && exr_image->tiles[tid].images[i]) {
          free(exr_image->tiles[tid].images[i]);
        }
      }
      if (exr_image->tiles[tid].images) {
        free(exr_image->tiles[tid].images);
      }
    }
    free(exr_image->tiles);
  }

  return TINYEXR_SUCCESS;
}

int ParseEXRHeaderFromFile(EXRHeader *exr_header, const EXRVersion *exr_version,
                           const char *filename, const char **err) {
  if (exr_header == NULL || exr_version == NULL || filename == NULL) {
    tinyexr::SetErrorMessage("Invalid argument for ParseEXRHeaderFromFile",
                             err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  MemoryMappedFile file(filename);
  if (!file.valid()) {
    tinyexr::SetErrorMessage("Cannot read file " + std::string(filename), err);
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  return ParseEXRHeaderFromMemory(exr_header, exr_version, file.data, file.size,
                                  err);
}

int ParseEXRMultipartHeaderFromMemory(EXRHeader ***exr_headers,
                                      int *num_headers,
                                      const EXRVersion *exr_version,
                                      const unsigned char *memory, size_t size,
                                      const char **err) {
  if (memory == NULL || exr_headers == NULL || num_headers == NULL ||
      exr_version == NULL) {
    // Invalid argument
    tinyexr::SetErrorMessage(
        "Invalid argument for ParseEXRMultipartHeaderFromMemory", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  if (size < tinyexr::kEXRVersionSize) {
    tinyexr::SetErrorMessage("Data size too short", err);
    return TINYEXR_ERROR_INVALID_DATA;
  }

  const unsigned char *marker = memory + tinyexr::kEXRVersionSize;
  size_t marker_size = size - tinyexr::kEXRVersionSize;

  std::vector<tinyexr::HeaderInfo> infos;

  for (;;) {
    tinyexr::HeaderInfo info;
    info.clear();

    std::string err_str;
    bool empty_header = false;
    int ret = ParseEXRHeader(&info, &empty_header, exr_version, &err_str,
                             marker, marker_size);

    if (ret != TINYEXR_SUCCESS) {

      // Free malloc-allocated memory here.
      for (size_t i = 0; i < info.attributes.size(); i++) {
        if (info.attributes[i].value) {
          free(info.attributes[i].value);
        }
      }

      tinyexr::SetErrorMessage(err_str, err);
      return ret;
    }

    if (empty_header) {
      marker += 1;  // skip '\0'
      break;
    }

    // `chunkCount` must exist in the header.
    if (info.chunk_count == 0) {

      // Free malloc-allocated memory here.
      for (size_t i = 0; i < info.attributes.size(); i++) {
        if (info.attributes[i].value) {
          free(info.attributes[i].value);
        }
      }

      tinyexr::SetErrorMessage(
          "`chunkCount' attribute is not found in the header.", err);
      return TINYEXR_ERROR_INVALID_DATA;
    }

    infos.push_back(info);

    // move to next header.
    marker += info.header_len;
    size -= info.header_len;
  }

  // allocate memory for EXRHeader and create array of EXRHeader pointers.
  (*exr_headers) =
      static_cast<EXRHeader **>(malloc(sizeof(EXRHeader *) * infos.size()));


  int retcode = TINYEXR_SUCCESS;

  for (size_t i = 0; i < infos.size(); i++) {
    EXRHeader *exr_header = static_cast<EXRHeader *>(malloc(sizeof(EXRHeader)));
    memset(exr_header, 0, sizeof(EXRHeader));

    std::string warn;
    std::string _err;
    if (!ConvertHeader(exr_header, infos[i], &warn, &_err)) {

      // Free malloc-allocated memory here.
      for (size_t k = 0; k < infos[i].attributes.size(); k++) {
        if (infos[i].attributes[k].value) {
          free(infos[i].attributes[k].value);
        }
      }

      if (!_err.empty()) {
        tinyexr::SetErrorMessage(
            _err, err);
      }
      // continue to converting headers
      retcode = TINYEXR_ERROR_INVALID_HEADER;
    }

    exr_header->multipart = exr_version->multipart ? 1 : 0;

    (*exr_headers)[i] = exr_header;
  }

  (*num_headers) = static_cast<int>(infos.size());

  return retcode;
}

int ParseEXRMultipartHeaderFromFile(EXRHeader ***exr_headers, int *num_headers,
                                    const EXRVersion *exr_version,
                                    const char *filename, const char **err) {
  if (exr_headers == NULL || num_headers == NULL || exr_version == NULL ||
      filename == NULL) {
    tinyexr::SetErrorMessage(
        "Invalid argument for ParseEXRMultipartHeaderFromFile()", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  MemoryMappedFile file(filename);
  if (!file.valid()) {
    tinyexr::SetErrorMessage("Cannot read file " + std::string(filename), err);
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  return ParseEXRMultipartHeaderFromMemory(
      exr_headers, num_headers, exr_version, file.data, file.size, err);
}

int ParseEXRVersionFromMemory(EXRVersion *version, const unsigned char *memory,
                              size_t size) {
  if (version == NULL || memory == NULL) {
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  if (size < tinyexr::kEXRVersionSize) {
    return TINYEXR_ERROR_INVALID_DATA;
  }

  const unsigned char *marker = memory;

  // Header check.
  {
    const char header[] = {0x76, 0x2f, 0x31, 0x01};

    if (memcmp(marker, header, 4) != 0) {
      return TINYEXR_ERROR_INVALID_MAGIC_NUMBER;
    }
    marker += 4;
  }

  version->tiled = false;
  version->long_name = false;
  version->non_image = false;
  version->multipart = false;

  // Parse version header.
  {
    // must be 2
    if (marker[0] != 2) {
      return TINYEXR_ERROR_INVALID_EXR_VERSION;
    }

    if (version == NULL) {
      return TINYEXR_SUCCESS;  // May OK
    }

    version->version = 2;

    if (marker[1] & 0x2) {  // 9th bit
      version->tiled = true;
    }
    if (marker[1] & 0x4) {  // 10th bit
      version->long_name = true;
    }
    if (marker[1] & 0x8) {        // 11th bit
      version->non_image = true;  // (deep image)
    }
    if (marker[1] & 0x10) {  // 12th bit
      version->multipart = true;
    }
  }

  return TINYEXR_SUCCESS;
}

int ParseEXRVersionFromFile(EXRVersion *version, const char *filename) {
  if (filename == NULL) {
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  FILE *fp = NULL;
#ifdef _WIN32
#if defined(_MSC_VER) || (defined(MINGW_HAS_SECURE_API) && MINGW_HAS_SECURE_API) // MSVC, MinGW GCC, or Clang.
  errno_t err = _wfopen_s(&fp, tinyexr::UTF8ToWchar(filename).c_str(), L"rb");
  if (err != 0) {
    // TODO(syoyo): return wfopen_s erro code
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }
#else
  // Unknown compiler or MinGW without MINGW_HAS_SECURE_API.
  fp = fopen(filename, "rb");
#endif
#else
  fp = fopen(filename, "rb");
#endif
  if (!fp) {
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  // Try to read kEXRVersionSize bytes; if the file is shorter than
  // kEXRVersionSize, this will produce an error. This avoids a call to
  // fseek(fp, 0, SEEK_END), which is not required to be supported by C
  // implementations.
  unsigned char buf[tinyexr::kEXRVersionSize];
  size_t ret = fread(&buf[0], 1, tinyexr::kEXRVersionSize, fp);
  fclose(fp);

  if (ret != tinyexr::kEXRVersionSize) {
    return TINYEXR_ERROR_INVALID_FILE;
  }

  return ParseEXRVersionFromMemory(version, buf, tinyexr::kEXRVersionSize);
}

int LoadEXRMultipartImageFromMemory(EXRImage *exr_images,
                                    const EXRHeader **exr_headers,
                                    unsigned int num_parts,
                                    const unsigned char *memory,
                                    const size_t size, const char **err) {
  if (exr_images == NULL || exr_headers == NULL || num_parts == 0 ||
      memory == NULL || (size <= tinyexr::kEXRVersionSize)) {
    tinyexr::SetErrorMessage(
        "Invalid argument for LoadEXRMultipartImageFromMemory()", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  // compute total header size.
  size_t total_header_size = 0;
  for (unsigned int i = 0; i < num_parts; i++) {
    if (exr_headers[i]->header_len == 0) {
      tinyexr::SetErrorMessage("EXRHeader variable is not initialized.", err);
      return TINYEXR_ERROR_INVALID_ARGUMENT;
    }

    total_header_size += exr_headers[i]->header_len;
  }

  const char *marker = reinterpret_cast<const char *>(
      memory + total_header_size + 4 +
      4);  // +8 for magic number and version header.

  marker += 1;  // Skip empty header.

  // NOTE 1:
  //   In multipart image, There is 'part number' before chunk data.
  //   4 byte : part number
  //   4+     : chunk
  //
  // NOTE 2:
  //   EXR spec says 'part number' is 'unsigned long' but actually this is
  //   'unsigned int(4 bytes)' in OpenEXR implementation...
  //   http://www.openexr.com/openexrfilelayout.pdf

  // Load chunk offset table.
  std::vector<tinyexr::OffsetData> chunk_offset_table_list;
  chunk_offset_table_list.reserve(num_parts);
  for (size_t i = 0; i < static_cast<size_t>(num_parts); i++) {
    chunk_offset_table_list.resize(chunk_offset_table_list.size() + 1);
    tinyexr::OffsetData& offset_data = chunk_offset_table_list.back();
    if (!exr_headers[i]->tiled || exr_headers[i]->tile_level_mode == TINYEXR_TILE_ONE_LEVEL) {
      tinyexr::InitSingleResolutionOffsets(offset_data, size_t(exr_headers[i]->chunk_count));
      std::vector<tinyexr::tinyexr_uint64>& offset_table = offset_data.offsets[0][0];

      for (size_t c = 0; c < offset_table.size(); c++) {
        tinyexr::tinyexr_uint64 offset;
        memcpy(&offset, marker, 8);
        tinyexr::swap8(&offset);

        if (offset >= size) {
          tinyexr::SetErrorMessage("Invalid offset size in EXR header chunks.",
                                   err);
          return TINYEXR_ERROR_INVALID_DATA;
        }

        offset_table[c] = offset + 4;  // +4 to skip 'part number'
        marker += 8;
      }
    } else {
      {
        std::vector<int> num_x_tiles, num_y_tiles;
        if (!tinyexr::PrecalculateTileInfo(num_x_tiles, num_y_tiles, exr_headers[i])) {
          tinyexr::SetErrorMessage("Invalid tile info.", err);
          return TINYEXR_ERROR_INVALID_DATA;
        }
        int num_blocks = InitTileOffsets(offset_data, exr_headers[i], num_x_tiles, num_y_tiles);
        if (num_blocks != exr_headers[i]->chunk_count) {
          tinyexr::SetErrorMessage("Invalid offset table size.", err);
          return TINYEXR_ERROR_INVALID_DATA;
        }
      }
      for (unsigned int l = 0; l < offset_data.offsets.size(); ++l) {
        for (unsigned int dy = 0; dy < offset_data.offsets[l].size(); ++dy) {
          for (unsigned int dx = 0; dx < offset_data.offsets[l][dy].size(); ++dx) {
            tinyexr::tinyexr_uint64 offset;
            memcpy(&offset, marker, sizeof(tinyexr::tinyexr_uint64));
            tinyexr::swap8(&offset);
            if (offset >= size) {
              tinyexr::SetErrorMessage("Invalid offset size in EXR header chunks.",
                err);
              return TINYEXR_ERROR_INVALID_DATA;
            }
            offset_data.offsets[l][dy][dx] = offset + 4; // +4 to skip 'part number'
            marker += sizeof(tinyexr::tinyexr_uint64);  // = 8
          }
        }
      }
    }
  }

  // Decode image.
  for (size_t i = 0; i < static_cast<size_t>(num_parts); i++) {
    tinyexr::OffsetData &offset_data = chunk_offset_table_list[i];

    // First check 'part number' is identical to 'i'
    for (unsigned int l = 0; l < offset_data.offsets.size(); ++l)
      for (unsigned int dy = 0; dy < offset_data.offsets[l].size(); ++dy)
        for (unsigned int dx = 0; dx < offset_data.offsets[l][dy].size(); ++dx) {

          const unsigned char *part_number_addr =
              memory + offset_data.offsets[l][dy][dx] - 4;  // -4 to move to 'part number' field.
          unsigned int part_no;
          memcpy(&part_no, part_number_addr, sizeof(unsigned int));  // 4
          tinyexr::swap4(&part_no);

          if (part_no != i) {
            tinyexr::SetErrorMessage("Invalid `part number' in EXR header chunks.",
                                     err);
            return TINYEXR_ERROR_INVALID_DATA;
          }
        }

    std::string e;
    int ret = tinyexr::DecodeChunk(&exr_images[i], exr_headers[i], offset_data,
                                   memory, size, &e);
    if (ret != TINYEXR_SUCCESS) {
      if (!e.empty()) {
        tinyexr::SetErrorMessage(e, err);
      }
      return ret;
    }
  }

  return TINYEXR_SUCCESS;
}

int LoadEXRMultipartImageFromFile(EXRImage *exr_images,
                                  const EXRHeader **exr_headers,
                                  unsigned int num_parts, const char *filename,
                                  const char **err) {
  if (exr_images == NULL || exr_headers == NULL || num_parts == 0) {
    tinyexr::SetErrorMessage(
        "Invalid argument for LoadEXRMultipartImageFromFile", err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  MemoryMappedFile file(filename);
  if (!file.valid()) {
    tinyexr::SetErrorMessage("Cannot read file " + std::string(filename), err);
    return TINYEXR_ERROR_CANT_OPEN_FILE;
  }

  return LoadEXRMultipartImageFromMemory(exr_images, exr_headers, num_parts,
                                         file.data, file.size, err);
}

int SaveEXRToMemory(const float *data, int width, int height, int components,
            const int save_as_fp16, unsigned char **outbuf, const char **err) {

  if ((components == 1) || components == 3 || components == 4) {
    // OK
  } else {
    std::stringstream ss;
    ss << "Unsupported component value : " << components << std::endl;

    tinyexr::SetErrorMessage(ss.str(), err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  EXRHeader header;
  InitEXRHeader(&header);

  if ((width < 16) && (height < 16)) {
    // No compression for small image.
    header.compression_type = TINYEXR_COMPRESSIONTYPE_NONE;
  } else {
    header.compression_type = TINYEXR_COMPRESSIONTYPE_ZIP;
  }

  EXRImage image;
  InitEXRImage(&image);

  image.num_channels = components;

  std::vector<float> images[4];

  if (components == 1) {
    images[0].resize(static_cast<size_t>(width * height));
    memcpy(images[0].data(), data, sizeof(float) * size_t(width * height));
  } else {
    images[0].resize(static_cast<size_t>(width * height));
    images[1].resize(static_cast<size_t>(width * height));
    images[2].resize(static_cast<size_t>(width * height));
    images[3].resize(static_cast<size_t>(width * height));

    // Split RGB(A)RGB(A)RGB(A)... into R, G and B(and A) layers
    for (size_t i = 0; i < static_cast<size_t>(width * height); i++) {
      images[0][i] = data[static_cast<size_t>(components) * i + 0];
      images[1][i] = data[static_cast<size_t>(components) * i + 1];
      images[2][i] = data[static_cast<size_t>(components) * i + 2];
      if (components == 4) {
        images[3][i] = data[static_cast<size_t>(components) * i + 3];
      }
    }
  }

  float *image_ptr[4] = {0, 0, 0, 0};
  if (components == 4) {
    image_ptr[0] = &(images[3].at(0));  // A
    image_ptr[1] = &(images[2].at(0));  // B
    image_ptr[2] = &(images[1].at(0));  // G
    image_ptr[3] = &(images[0].at(0));  // R
  } else if (components == 3) {
    image_ptr[0] = &(images[2].at(0));  // B
    image_ptr[1] = &(images[1].at(0));  // G
    image_ptr[2] = &(images[0].at(0));  // R
  } else if (components == 1) {
    image_ptr[0] = &(images[0].at(0));  // A
  }

  image.images = reinterpret_cast<unsigned char **>(image_ptr);
  image.width = width;
  image.height = height;

  header.num_channels = components;
  header.channels = static_cast<EXRChannelInfo *>(malloc(
      sizeof(EXRChannelInfo) * static_cast<size_t>(header.num_channels)));
  // Must be (A)BGR order, since most of EXR viewers expect this channel order.
  if (components == 4) {
#ifdef _MSC_VER
    strncpy_s(header.channels[0].name, "A", 255);
    strncpy_s(header.channels[1].name, "B", 255);
    strncpy_s(header.channels[2].name, "G", 255);
    strncpy_s(header.channels[3].name, "R", 255);
#else
    strncpy(header.channels[0].name, "A", 255);
    strncpy(header.channels[1].name, "B", 255);
    strncpy(header.channels[2].name, "G", 255);
    strncpy(header.channels[3].name, "R", 255);
#endif
    header.channels[0].name[strlen("A")] = '\0';
    header.channels[1].name[strlen("B")] = '\0';
    header.channels[2].name[strlen("G")] = '\0';
    header.channels[3].name[strlen("R")] = '\0';
  } else if (components == 3) {
#ifdef _MSC_VER
    strncpy_s(header.channels[0].name, "B", 255);
    strncpy_s(header.channels[1].name, "G", 255);
    strncpy_s(header.channels[2].name, "R", 255);
#else
    strncpy(header.channels[0].name, "B", 255);
    strncpy(header.channels[1].name, "G", 255);
    strncpy(header.channels[2].name, "R", 255);
#endif
    header.channels[0].name[strlen("B")] = '\0';
    header.channels[1].name[strlen("G")] = '\0';
    header.channels[2].name[strlen("R")] = '\0';
  } else {
#ifdef _MSC_VER
    strncpy_s(header.channels[0].name, "A", 255);
#else
    strncpy(header.channels[0].name, "A", 255);
#endif
    header.channels[0].name[strlen("A")] = '\0';
  }

  header.pixel_types = static_cast<int *>(
      malloc(sizeof(int) * static_cast<size_t>(header.num_channels)));
  header.requested_pixel_types = static_cast<int *>(
      malloc(sizeof(int) * static_cast<size_t>(header.num_channels)));
  for (int i = 0; i < header.num_channels; i++) {
    header.pixel_types[i] =
        TINYEXR_PIXELTYPE_FLOAT;  // pixel type of input image

    if (save_as_fp16 > 0) {
      header.requested_pixel_types[i] =
          TINYEXR_PIXELTYPE_HALF;  // save with half(fp16) pixel format
    } else {
      header.requested_pixel_types[i] =
          TINYEXR_PIXELTYPE_FLOAT;  // save with float(fp32) pixel format(i.e.
                                    // no precision reduction)
    }
  }


  unsigned char *mem_buf;
  size_t mem_size = SaveEXRImageToMemory(&image, &header, &mem_buf, err);

  if (mem_size == 0) {
    return TINYEXR_ERROR_SERIALIZATION_FAILED;
  }

  free(header.channels);
  free(header.pixel_types);
  free(header.requested_pixel_types);

  if (mem_size > size_t(std::numeric_limits<int>::max())) {
    free(mem_buf);
    return TINYEXR_ERROR_DATA_TOO_LARGE;
  }

  (*outbuf) = mem_buf;

  return int(mem_size);
}

int SaveEXR(const float *data, int width, int height, int components,
            const int save_as_fp16, const char *outfilename, const char **err) {
  if ((components == 1) || components == 3 || components == 4) {
    // OK
  } else {
    std::stringstream ss;
    ss << "Unsupported component value : " << components << std::endl;

    tinyexr::SetErrorMessage(ss.str(), err);
    return TINYEXR_ERROR_INVALID_ARGUMENT;
  }

  EXRHeader header;
  InitEXRHeader(&header);

  if ((width < 16) && (height < 16)) {
    // No compression for small image.
    header.compression_type = TINYEXR_COMPRESSIONTYPE_NONE;
  } else {
    header.compression_type = TINYEXR_COMPRESSIONTYPE_ZIP;
  }

  EXRImage image;
  InitEXRImage(&image);

  image.num_channels = components;

  std::vector<float> images[4];
  const size_t pixel_count =
      static_cast<size_t>(width) * static_cast<size_t>(height);

  if (components == 1) {
    images[0].resize(pixel_count);
    memcpy(images[0].data(), data, sizeof(float) * pixel_count);
  } else {
    images[0].resize(pixel_count);
    images[1].resize(pixel_count);
    images[2].resize(pixel_count);
    images[3].resize(pixel_count);

    // Split RGB(A)RGB(A)RGB(A)... into R, G and B(and A) layers
    for (size_t i = 0; i < pixel_count; i++) {
      images[0][i] = data[static_cast<size_t>(components) * i + 0];
      images[1][i] = data[static_cast<size_t>(components) * i + 1];
      images[2][i] = data[static_cast<size_t>(components) * i + 2];
      if (components == 4) {
        images[3][i] = data[static_cast<size_t>(components) * i + 3];
      }
    }
  }

  float *image_ptr[4] = {0, 0, 0, 0};
  if (components == 4) {
    image_ptr[0] = &(images[3].at(0));  // A
    image_ptr[1] = &(images[2].at(0));  // B
    image_ptr[2] = &(images[1].at(0));  // G
    image_ptr[3] = &(images[0].at(0));  // R
  } else if (components == 3) {
    image_ptr[0] = &(images[2].at(0));  // B
    image_ptr[1] = &(images[1].at(0));  // G
    image_ptr[2] = &(images[0].at(0));  // R
  } else if (components == 1) {
    image_ptr[0] = &(images[0].at(0));  // A
  }

  image.images = reinterpret_cast<unsigned char **>(image_ptr);
  image.width = width;
  image.height = height;

  header.num_channels = components;
  header.channels = static_cast<EXRChannelInfo *>(malloc(
      sizeof(EXRChannelInfo) * static_cast<size_t>(header.num_channels)));
  // Must be (A)BGR order, since most of EXR viewers expect this channel order.
  if (components == 4) {
#ifdef _MSC_VER
    strncpy_s(header.channels[0].name, "A", 255);
    strncpy_s(header.channels[1].name, "B", 255);
    strncpy_s(header.channels[2].name, "G", 255);
    strncpy_s(header.channels[3].name, "R", 255);
#else
    strncpy(header.channels[0].name, "A", 255);
    strncpy(header.channels[1].name, "B", 255);
    strncpy(header.channels[2].name, "G", 255);
    strncpy(header.channels[3].name, "R", 255);
#endif
    header.channels[0].name[strlen("A")] = '\0';
    header.channels[1].name[strlen("B")] = '\0';
    header.channels[2].name[strlen("G")] = '\0';
    header.channels[3].name[strlen("R")] = '\0';
  } else if (components == 3) {
#ifdef _MSC_VER
    strncpy_s(header.channels[0].name, "B", 255);
    strncpy_s(header.channels[1].name, "G", 255);
    strncpy_s(header.channels[2].name, "R", 255);
#else
    strncpy(header.channels[0].name, "B", 255);
    strncpy(header.channels[1].name, "G", 255);
    strncpy(header.channels[2].name, "R", 255);
#endif
    header.channels[0].name[strlen("B")] = '\0';
    header.channels[1].name[strlen("G")] = '\0';
    header.channels[2].name[strlen("R")] = '\0';
  } else {
#ifdef _MSC_VER
    strncpy_s(header.channels[0].name, "A", 255);
#else
    strncpy(header.channels[0].name, "A", 255);
#endif
    header.channels[0].name[strlen("A")] = '\0';
  }

  header.pixel_types = static_cast<int *>(
      malloc(sizeof(int) * static_cast<size_t>(header.num_channels)));
  header.requested_pixel_types = static_cast<int *>(
      malloc(sizeof(int) * static_cast<size_t>(header.num_channels)));
  for (int i = 0; i < header.num_channels; i++) {
    header.pixel_types[i] =
        TINYEXR_PIXELTYPE_FLOAT;  // pixel type of input image

    if (save_as_fp16 > 0) {
      header.requested_pixel_types[i] =
          TINYEXR_PIXELTYPE_HALF;  // save with half(fp16) pixel format
    } else {
      header.requested_pixel_types[i] =
          TINYEXR_PIXELTYPE_FLOAT;  // save with float(fp32) pixel format(i.e.
                                    // no precision reduction)
    }
  }

  int ret = SaveEXRImageToFile(&image, &header, outfilename, err);

  free(header.channels);
  free(header.pixel_types);
  free(header.requested_pixel_types);

  return ret;
}

#ifdef __clang__
// zero-as-null-pointer-constant
#pragma clang diagnostic pop
#endif

#endif  // TINYEXR_IMPLEMENTATION_DEFINED
#endif  // TINYEXR_IMPLEMENTATION
