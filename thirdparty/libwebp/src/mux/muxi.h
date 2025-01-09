// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Internal header for mux library.
//
// Author: Urvang (urvang@google.com)

#ifndef WEBP_MUX_MUXI_H_
#define WEBP_MUX_MUXI_H_

#include <assert.h>
#include <stdlib.h>
#include "src/dec/vp8i_dec.h"
#include "src/dec/vp8li_dec.h"
#include "src/webp/mux.h"

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Defines and constants.

#define MUX_MAJ_VERSION 1
#define MUX_MIN_VERSION 5
#define MUX_REV_VERSION 0

// Chunk object.
typedef struct WebPChunk WebPChunk;
struct WebPChunk {
  uint32_t        tag_;
  int             owner_;  // True if *data_ memory is owned internally.
                           // VP8X, ANIM, and other internally created chunks
                           // like ANMF are always owned.
  WebPData        data_;
  WebPChunk*      next_;
};

// MuxImage object. Store a full WebP image (including ANMF chunk, ALPH
// chunk and VP8/VP8L chunk),
typedef struct WebPMuxImage WebPMuxImage;
struct WebPMuxImage {
  WebPChunk*  header_;      // Corresponds to WEBP_CHUNK_ANMF.
  WebPChunk*  alpha_;       // Corresponds to WEBP_CHUNK_ALPHA.
  WebPChunk*  img_;         // Corresponds to WEBP_CHUNK_IMAGE.
  WebPChunk*  unknown_;     // Corresponds to WEBP_CHUNK_UNKNOWN.
  int         width_;
  int         height_;
  int         has_alpha_;   // Through ALPH chunk or as part of VP8L.
  int         is_partial_;  // True if only some of the chunks are filled.
  WebPMuxImage* next_;
};

// Main mux object. Stores data chunks.
struct WebPMux {
  WebPMuxImage*   images_;
  WebPChunk*      iccp_;
  WebPChunk*      exif_;
  WebPChunk*      xmp_;
  WebPChunk*      anim_;
  WebPChunk*      vp8x_;

  WebPChunk*      unknown_;
  int             canvas_width_;
  int             canvas_height_;
};

// CHUNK_INDEX enum: used for indexing within 'kChunks' (defined below) only.
// Note: the reason for having two enums ('WebPChunkId' and 'CHUNK_INDEX') is to
// allow two different chunks to have the same id (e.g. WebPChunkId
// 'WEBP_CHUNK_IMAGE' can correspond to CHUNK_INDEX 'IDX_VP8' or 'IDX_VP8L').
typedef enum {
  IDX_VP8X = 0,
  IDX_ICCP,
  IDX_ANIM,
  IDX_ANMF,
  IDX_ALPHA,
  IDX_VP8,
  IDX_VP8L,
  IDX_EXIF,
  IDX_XMP,
  IDX_UNKNOWN,

  IDX_NIL,
  IDX_LAST_CHUNK
} CHUNK_INDEX;

#define NIL_TAG 0x00000000u  // To signal void chunk.

typedef struct {
  uint32_t      tag;
  WebPChunkId   id;
  uint32_t      size;
} ChunkInfo;

extern const ChunkInfo kChunks[IDX_LAST_CHUNK];

//------------------------------------------------------------------------------
// Chunk object management.

// Initialize.
void ChunkInit(WebPChunk* const chunk);

// Get chunk index from chunk tag. Returns IDX_UNKNOWN if not found.
CHUNK_INDEX ChunkGetIndexFromTag(uint32_t tag);

// Get chunk id from chunk tag. Returns WEBP_CHUNK_UNKNOWN if not found.
WebPChunkId ChunkGetIdFromTag(uint32_t tag);

// Convert a fourcc string to a tag.
uint32_t ChunkGetTagFromFourCC(const char fourcc[4]);

// Get chunk index from fourcc. Returns IDX_UNKNOWN if given fourcc is unknown.
CHUNK_INDEX ChunkGetIndexFromFourCC(const char fourcc[4]);

// Search for nth chunk with given 'tag' in the chunk list.
// nth = 0 means "last of the list".
WebPChunk* ChunkSearchList(WebPChunk* first, uint32_t nth, uint32_t tag);

// Fill the chunk with the given data.
WebPMuxError ChunkAssignData(WebPChunk* chunk, const WebPData* const data,
                             int copy_data, uint32_t tag);

// Sets 'chunk' as the only element in 'chunk_list' if it is empty.
// On success ownership is transferred from 'chunk' to the 'chunk_list'.
WebPMuxError ChunkSetHead(WebPChunk* const chunk, WebPChunk** const chunk_list);
// Sets 'chunk' at last position in the 'chunk_list'.
// On success ownership is transferred from 'chunk' to the 'chunk_list'.
// *chunk_list also points towards the last valid element of the initial
// *chunk_list.
WebPMuxError ChunkAppend(WebPChunk* const chunk, WebPChunk*** const chunk_list);

// Releases chunk and returns chunk->next_.
WebPChunk* ChunkRelease(WebPChunk* const chunk);

// Deletes given chunk & returns chunk->next_.
WebPChunk* ChunkDelete(WebPChunk* const chunk);

// Deletes all chunks in the given chunk list.
void ChunkListDelete(WebPChunk** const chunk_list);

// Returns size of the chunk including chunk header and padding byte (if any).
static WEBP_INLINE size_t SizeWithPadding(size_t chunk_size) {
  assert(chunk_size <= MAX_CHUNK_PAYLOAD);
  return CHUNK_HEADER_SIZE + ((chunk_size + 1) & ~1U);
}

// Size of a chunk including header and padding.
static WEBP_INLINE size_t ChunkDiskSize(const WebPChunk* chunk) {
  const size_t data_size = chunk->data_.size;
  return SizeWithPadding(data_size);
}

// Total size of a list of chunks.
size_t ChunkListDiskSize(const WebPChunk* chunk_list);

// Write out the given list of chunks into 'dst'.
uint8_t* ChunkListEmit(const WebPChunk* chunk_list, uint8_t* dst);

//------------------------------------------------------------------------------
// MuxImage object management.

// Initialize.
void MuxImageInit(WebPMuxImage* const wpi);

// Releases image 'wpi' and returns wpi->next.
WebPMuxImage* MuxImageRelease(WebPMuxImage* const wpi);

// Delete image 'wpi' and return the next image in the list or NULL.
// 'wpi' can be NULL.
WebPMuxImage* MuxImageDelete(WebPMuxImage* const wpi);

// Count number of images matching the given tag id in the 'wpi_list'.
// If id == WEBP_CHUNK_NIL, all images will be matched.
int MuxImageCount(const WebPMuxImage* wpi_list, WebPChunkId id);

// Update width/height/has_alpha info from chunks within wpi.
// Also remove ALPH chunk if not needed.
int MuxImageFinalize(WebPMuxImage* const wpi);

// Check if given ID corresponds to an image related chunk.
static WEBP_INLINE int IsWPI(WebPChunkId id) {
  switch (id) {
    case WEBP_CHUNK_ANMF:
    case WEBP_CHUNK_ALPHA:
    case WEBP_CHUNK_IMAGE:  return 1;
    default:        return 0;
  }
}

// Pushes 'wpi' at the end of 'wpi_list'.
WebPMuxError MuxImagePush(const WebPMuxImage* wpi, WebPMuxImage** wpi_list);

// Delete nth image in the image list.
WebPMuxError MuxImageDeleteNth(WebPMuxImage** wpi_list, uint32_t nth);

// Get nth image in the image list.
WebPMuxError MuxImageGetNth(const WebPMuxImage** wpi_list, uint32_t nth,
                            WebPMuxImage** wpi);

// Total size of the given image.
size_t MuxImageDiskSize(const WebPMuxImage* const wpi);

// Write out the given image into 'dst'.
uint8_t* MuxImageEmit(const WebPMuxImage* const wpi, uint8_t* dst);

//------------------------------------------------------------------------------
// Helper methods for mux.

// Checks if the given image list contains at least one image with alpha.
int MuxHasAlpha(const WebPMuxImage* images);

// Write out RIFF header into 'data', given total data size 'size'.
uint8_t* MuxEmitRiffHeader(uint8_t* const data, size_t size);

// Returns the list where chunk with given ID is to be inserted in mux.
WebPChunk** MuxGetChunkListFromId(const WebPMux* mux, WebPChunkId id);

// Validates the given mux object.
WebPMuxError MuxValidate(const WebPMux* const mux);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_MUX_MUXI_H_
