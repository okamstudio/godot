// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  WebP container demux.
//

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "src/utils/utils.h"
#include "src/webp/decode.h"     // WebPGetFeatures
#include "src/webp/demux.h"
#include "src/webp/format_constants.h"

#define DMUX_MAJ_VERSION 1
#define DMUX_MIN_VERSION 5
#define DMUX_REV_VERSION 0

typedef struct {
  size_t start_;        // start location of the data
  size_t end_;          // end location
  size_t riff_end_;     // riff chunk end location, can be > end_.
  size_t buf_size_;     // size of the buffer
  const uint8_t* buf_;
} MemBuffer;

typedef struct {
  size_t offset_;
  size_t size_;
} ChunkData;

typedef struct Frame {
  int x_offset_, y_offset_;
  int width_, height_;
  int has_alpha_;
  int duration_;
  WebPMuxAnimDispose dispose_method_;
  WebPMuxAnimBlend blend_method_;
  int frame_num_;
  int complete_;   // img_components_ contains a full image.
  ChunkData img_components_[2];  // 0=VP8{,L} 1=ALPH
  struct Frame* next_;
} Frame;

typedef struct Chunk {
  ChunkData data_;
  struct Chunk* next_;
} Chunk;

struct WebPDemuxer {
  MemBuffer mem_;
  WebPDemuxState state_;
  int is_ext_format_;
  uint32_t feature_flags_;
  int canvas_width_, canvas_height_;
  int loop_count_;
  uint32_t bgcolor_;
  int num_frames_;
  Frame* frames_;
  Frame** frames_tail_;
  Chunk* chunks_;  // non-image chunks
  Chunk** chunks_tail_;
};

typedef enum {
  PARSE_OK,
  PARSE_NEED_MORE_DATA,
  PARSE_ERROR
} ParseStatus;

typedef struct ChunkParser {
  uint8_t id[4];
  ParseStatus (*parse)(WebPDemuxer* const dmux);
  int (*valid)(const WebPDemuxer* const dmux);
} ChunkParser;

static ParseStatus ParseSingleImage(WebPDemuxer* const dmux);
static ParseStatus ParseVP8X(WebPDemuxer* const dmux);
static int IsValidSimpleFormat(const WebPDemuxer* const dmux);
static int IsValidExtendedFormat(const WebPDemuxer* const dmux);

static const ChunkParser kMasterChunks[] = {
  { { 'V', 'P', '8', ' ' }, ParseSingleImage, IsValidSimpleFormat },
  { { 'V', 'P', '8', 'L' }, ParseSingleImage, IsValidSimpleFormat },
  { { 'V', 'P', '8', 'X' }, ParseVP8X,        IsValidExtendedFormat },
  { { '0', '0', '0', '0' }, NULL,             NULL },
};

//------------------------------------------------------------------------------

int WebPGetDemuxVersion(void) {
  return (DMUX_MAJ_VERSION << 16) | (DMUX_MIN_VERSION << 8) | DMUX_REV_VERSION;
}

// -----------------------------------------------------------------------------
// MemBuffer

static int RemapMemBuffer(MemBuffer* const mem,
                          const uint8_t* data, size_t size) {
  if (size < mem->buf_size_) return 0;  // can't remap to a shorter buffer!

  mem->buf_ = data;
  mem->end_ = mem->buf_size_ = size;
  return 1;
}

static int InitMemBuffer(MemBuffer* const mem,
                         const uint8_t* data, size_t size) {
  memset(mem, 0, sizeof(*mem));
  return RemapMemBuffer(mem, data, size);
}

// Return the remaining data size available in 'mem'.
static WEBP_INLINE size_t MemDataSize(const MemBuffer* const mem) {
  return (mem->end_ - mem->start_);
}

// Return true if 'size' exceeds the end of the RIFF chunk.
static WEBP_INLINE int SizeIsInvalid(const MemBuffer* const mem, size_t size) {
  return (size > mem->riff_end_ - mem->start_);
}

static WEBP_INLINE void Skip(MemBuffer* const mem, size_t size) {
  mem->start_ += size;
}

static WEBP_INLINE void Rewind(MemBuffer* const mem, size_t size) {
  mem->start_ -= size;
}

static WEBP_INLINE const uint8_t* GetBuffer(MemBuffer* const mem) {
  return mem->buf_ + mem->start_;
}

// Read from 'mem' and skip the read bytes.
static WEBP_INLINE uint8_t ReadByte(MemBuffer* const mem) {
  const uint8_t byte = mem->buf_[mem->start_];
  Skip(mem, 1);
  return byte;
}

static WEBP_INLINE int ReadLE16s(MemBuffer* const mem) {
  const uint8_t* const data = mem->buf_ + mem->start_;
  const int val = GetLE16(data);
  Skip(mem, 2);
  return val;
}

static WEBP_INLINE int ReadLE24s(MemBuffer* const mem) {
  const uint8_t* const data = mem->buf_ + mem->start_;
  const int val = GetLE24(data);
  Skip(mem, 3);
  return val;
}

static WEBP_INLINE uint32_t ReadLE32(MemBuffer* const mem) {
  const uint8_t* const data = mem->buf_ + mem->start_;
  const uint32_t val = GetLE32(data);
  Skip(mem, 4);
  return val;
}

// -----------------------------------------------------------------------------
// Secondary chunk parsing

static void AddChunk(WebPDemuxer* const dmux, Chunk* const chunk) {
  *dmux->chunks_tail_ = chunk;
  chunk->next_ = NULL;
  dmux->chunks_tail_ = &chunk->next_;
}

// Add a frame to the end of the list, ensuring the last frame is complete.
// Returns true on success, false otherwise.
static int AddFrame(WebPDemuxer* const dmux, Frame* const frame) {
  const Frame* const last_frame = *dmux->frames_tail_;
  if (last_frame != NULL && !last_frame->complete_) return 0;

  *dmux->frames_tail_ = frame;
  frame->next_ = NULL;
  dmux->frames_tail_ = &frame->next_;
  return 1;
}

static void SetFrameInfo(size_t start_offset, size_t size,
                         int frame_num, int complete,
                         const WebPBitstreamFeatures* const features,
                         Frame* const frame) {
  frame->img_components_[0].offset_ = start_offset;
  frame->img_components_[0].size_ = size;
  frame->width_ = features->width;
  frame->height_ = features->height;
  frame->has_alpha_ |= features->has_alpha;
  frame->frame_num_ = frame_num;
  frame->complete_ = complete;
}

// Store image bearing chunks to 'frame'. 'min_size' is an optional size
// requirement, it may be zero.
static ParseStatus StoreFrame(int frame_num, uint32_t min_size,
                              MemBuffer* const mem, Frame* const frame) {
  int alpha_chunks = 0;
  int image_chunks = 0;
  int done = (MemDataSize(mem) < CHUNK_HEADER_SIZE ||
              MemDataSize(mem) < min_size);
  ParseStatus status = PARSE_OK;

  if (done) return PARSE_NEED_MORE_DATA;

  do {
    const size_t chunk_start_offset = mem->start_;
    const uint32_t fourcc = ReadLE32(mem);
    const uint32_t payload_size = ReadLE32(mem);
    uint32_t payload_size_padded;
    size_t payload_available;
    size_t chunk_size;

    if (payload_size > MAX_CHUNK_PAYLOAD) return PARSE_ERROR;

    payload_size_padded = payload_size + (payload_size & 1);
    payload_available = (payload_size_padded > MemDataSize(mem))
                      ? MemDataSize(mem) : payload_size_padded;
    chunk_size = CHUNK_HEADER_SIZE + payload_available;
    if (SizeIsInvalid(mem, payload_size_padded)) return PARSE_ERROR;
    if (payload_size_padded > MemDataSize(mem)) status = PARSE_NEED_MORE_DATA;

    switch (fourcc) {
      case MKFOURCC('A', 'L', 'P', 'H'):
        if (alpha_chunks == 0) {
          ++alpha_chunks;
          frame->img_components_[1].offset_ = chunk_start_offset;
          frame->img_components_[1].size_ = chunk_size;
          frame->has_alpha_ = 1;
          frame->frame_num_ = frame_num;
          Skip(mem, payload_available);
        } else {
          goto Done;
        }
        break;
      case MKFOURCC('V', 'P', '8', 'L'):
        if (alpha_chunks > 0) return PARSE_ERROR;  // VP8L has its own alpha
        // fall through
      case MKFOURCC('V', 'P', '8', ' '):
        if (image_chunks == 0) {
          // Extract the bitstream features, tolerating failures when the data
          // is incomplete.
          WebPBitstreamFeatures features;
          const VP8StatusCode vp8_status =
              WebPGetFeatures(mem->buf_ + chunk_start_offset, chunk_size,
                              &features);
          if (status == PARSE_NEED_MORE_DATA &&
              vp8_status == VP8_STATUS_NOT_ENOUGH_DATA) {
            return PARSE_NEED_MORE_DATA;
          } else if (vp8_status != VP8_STATUS_OK) {
            // We have enough data, and yet WebPGetFeatures() failed.
            return PARSE_ERROR;
          }
          ++image_chunks;
          SetFrameInfo(chunk_start_offset, chunk_size, frame_num,
                       status == PARSE_OK, &features, frame);
          Skip(mem, payload_available);
        } else {
          goto Done;
        }
        break;
 Done:
      default:
        // Restore fourcc/size when moving up one level in parsing.
        Rewind(mem, CHUNK_HEADER_SIZE);
        done = 1;
        break;
    }

    if (mem->start_ == mem->riff_end_) {
      done = 1;
    } else if (MemDataSize(mem) < CHUNK_HEADER_SIZE) {
      status = PARSE_NEED_MORE_DATA;
    }
  } while (!done && status == PARSE_OK);

  return status;
}

// Creates a new Frame if 'actual_size' is within bounds and 'mem' contains
// enough data ('min_size') to parse the payload.
// Returns PARSE_OK on success with *frame pointing to the new Frame.
// Returns PARSE_NEED_MORE_DATA with insufficient data, PARSE_ERROR otherwise.
static ParseStatus NewFrame(const MemBuffer* const mem,
                            uint32_t min_size, uint32_t actual_size,
                            Frame** frame) {
  if (SizeIsInvalid(mem, min_size)) return PARSE_ERROR;
  if (actual_size < min_size) return PARSE_ERROR;
  if (MemDataSize(mem) < min_size)  return PARSE_NEED_MORE_DATA;

  *frame = (Frame*)WebPSafeCalloc(1ULL, sizeof(**frame));
  return (*frame == NULL) ? PARSE_ERROR : PARSE_OK;
}

// Parse a 'ANMF' chunk and any image bearing chunks that immediately follow.
// 'frame_chunk_size' is the previously validated, padded chunk size.
static ParseStatus ParseAnimationFrame(
    WebPDemuxer* const dmux, uint32_t frame_chunk_size) {
  const int is_animation = !!(dmux->feature_flags_ & ANIMATION_FLAG);
  const uint32_t anmf_payload_size = frame_chunk_size - ANMF_CHUNK_SIZE;
  int added_frame = 0;
  int bits;
  MemBuffer* const mem = &dmux->mem_;
  Frame* frame;
  size_t start_offset;
  ParseStatus status =
      NewFrame(mem, ANMF_CHUNK_SIZE, frame_chunk_size, &frame);
  if (status != PARSE_OK) return status;

  frame->x_offset_       = 2 * ReadLE24s(mem);
  frame->y_offset_       = 2 * ReadLE24s(mem);
  frame->width_          = 1 + ReadLE24s(mem);
  frame->height_         = 1 + ReadLE24s(mem);
  frame->duration_       = ReadLE24s(mem);
  bits = ReadByte(mem);
  frame->dispose_method_ =
      (bits & 1) ? WEBP_MUX_DISPOSE_BACKGROUND : WEBP_MUX_DISPOSE_NONE;
  frame->blend_method_ = (bits & 2) ? WEBP_MUX_NO_BLEND : WEBP_MUX_BLEND;
  if (frame->width_ * (uint64_t)frame->height_ >= MAX_IMAGE_AREA) {
    WebPSafeFree(frame);
    return PARSE_ERROR;
  }

  // Store a frame only if the animation flag is set there is some data for
  // this frame is available.
  start_offset = mem->start_;
  status = StoreFrame(dmux->num_frames_ + 1, anmf_payload_size, mem, frame);
  if (status != PARSE_ERROR && mem->start_ - start_offset > anmf_payload_size) {
    status = PARSE_ERROR;
  }
  if (status != PARSE_ERROR && is_animation && frame->frame_num_ > 0) {
    added_frame = AddFrame(dmux, frame);
    if (added_frame) {
      ++dmux->num_frames_;
    } else {
      status = PARSE_ERROR;
    }
  }

  if (!added_frame) WebPSafeFree(frame);
  return status;
}

// General chunk storage, starting with the header at 'start_offset', allowing
// the user to request the payload via a fourcc string. 'size' includes the
// header and the unpadded payload size.
// Returns true on success, false otherwise.
static int StoreChunk(WebPDemuxer* const dmux,
                      size_t start_offset, uint32_t size) {
  Chunk* const chunk = (Chunk*)WebPSafeCalloc(1ULL, sizeof(*chunk));
  if (chunk == NULL) return 0;

  chunk->data_.offset_ = start_offset;
  chunk->data_.size_ = size;
  AddChunk(dmux, chunk);
  return 1;
}

// -----------------------------------------------------------------------------
// Primary chunk parsing

static ParseStatus ReadHeader(MemBuffer* const mem) {
  const size_t min_size = RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE;
  uint32_t riff_size;

  // Basic file level validation.
  if (MemDataSize(mem) < min_size) return PARSE_NEED_MORE_DATA;
  if (memcmp(GetBuffer(mem), "RIFF", CHUNK_SIZE_BYTES) ||
      memcmp(GetBuffer(mem) + CHUNK_HEADER_SIZE, "WEBP", CHUNK_SIZE_BYTES)) {
    return PARSE_ERROR;
  }

  riff_size = GetLE32(GetBuffer(mem) + TAG_SIZE);
  if (riff_size < CHUNK_HEADER_SIZE) return PARSE_ERROR;
  if (riff_size > MAX_CHUNK_PAYLOAD) return PARSE_ERROR;

  // There's no point in reading past the end of the RIFF chunk
  mem->riff_end_ = riff_size + CHUNK_HEADER_SIZE;
  if (mem->buf_size_ > mem->riff_end_) {
    mem->buf_size_ = mem->end_ = mem->riff_end_;
  }

  Skip(mem, RIFF_HEADER_SIZE);
  return PARSE_OK;
}

static ParseStatus ParseSingleImage(WebPDemuxer* const dmux) {
  const size_t min_size = CHUNK_HEADER_SIZE;
  MemBuffer* const mem = &dmux->mem_;
  Frame* frame;
  ParseStatus status;
  int image_added = 0;

  if (dmux->frames_ != NULL) return PARSE_ERROR;
  if (SizeIsInvalid(mem, min_size)) return PARSE_ERROR;
  if (MemDataSize(mem) < min_size) return PARSE_NEED_MORE_DATA;

  frame = (Frame*)WebPSafeCalloc(1ULL, sizeof(*frame));
  if (frame == NULL) return PARSE_ERROR;

  // For the single image case we allow parsing of a partial frame, so no
  // minimum size is imposed here.
  status = StoreFrame(1, 0, &dmux->mem_, frame);
  if (status != PARSE_ERROR) {
    const int has_alpha = !!(dmux->feature_flags_ & ALPHA_FLAG);
    // Clear any alpha when the alpha flag is missing.
    if (!has_alpha && frame->img_components_[1].size_ > 0) {
      frame->img_components_[1].offset_ = 0;
      frame->img_components_[1].size_ = 0;
      frame->has_alpha_ = 0;
    }

    // Use the frame width/height as the canvas values for non-vp8x files.
    // Also, set ALPHA_FLAG if this is a lossless image with alpha.
    if (!dmux->is_ext_format_ && frame->width_ > 0 && frame->height_ > 0) {
      dmux->state_ = WEBP_DEMUX_PARSED_HEADER;
      dmux->canvas_width_ = frame->width_;
      dmux->canvas_height_ = frame->height_;
      dmux->feature_flags_ |= frame->has_alpha_ ? ALPHA_FLAG : 0;
    }
    if (!AddFrame(dmux, frame)) {
      status = PARSE_ERROR;  // last frame was left incomplete
    } else {
      image_added = 1;
      dmux->num_frames_ = 1;
    }
  }

  if (!image_added) WebPSafeFree(frame);
  return status;
}

static ParseStatus ParseVP8XChunks(WebPDemuxer* const dmux) {
  const int is_animation = !!(dmux->feature_flags_ & ANIMATION_FLAG);
  MemBuffer* const mem = &dmux->mem_;
  int anim_chunks = 0;
  ParseStatus status = PARSE_OK;

  do {
    int store_chunk = 1;
    const size_t chunk_start_offset = mem->start_;
    const uint32_t fourcc = ReadLE32(mem);
    const uint32_t chunk_size = ReadLE32(mem);
    uint32_t chunk_size_padded;

    if (chunk_size > MAX_CHUNK_PAYLOAD) return PARSE_ERROR;

    chunk_size_padded = chunk_size + (chunk_size & 1);
    if (SizeIsInvalid(mem, chunk_size_padded)) return PARSE_ERROR;

    switch (fourcc) {
      case MKFOURCC('V', 'P', '8', 'X'): {
        return PARSE_ERROR;
      }
      case MKFOURCC('A', 'L', 'P', 'H'):
      case MKFOURCC('V', 'P', '8', ' '):
      case MKFOURCC('V', 'P', '8', 'L'): {
        // check that this isn't an animation (all frames should be in an ANMF).
        if (anim_chunks > 0 || is_animation) return PARSE_ERROR;

        Rewind(mem, CHUNK_HEADER_SIZE);
        status = ParseSingleImage(dmux);
        break;
      }
      case MKFOURCC('A', 'N', 'I', 'M'): {
        if (chunk_size_padded < ANIM_CHUNK_SIZE) return PARSE_ERROR;

        if (MemDataSize(mem) < chunk_size_padded) {
          status = PARSE_NEED_MORE_DATA;
        } else if (anim_chunks == 0) {
          ++anim_chunks;
          dmux->bgcolor_ = ReadLE32(mem);
          dmux->loop_count_ = ReadLE16s(mem);
          Skip(mem, chunk_size_padded - ANIM_CHUNK_SIZE);
        } else {
          store_chunk = 0;
          goto Skip;
        }
        break;
      }
      case MKFOURCC('A', 'N', 'M', 'F'): {
        if (anim_chunks == 0) return PARSE_ERROR;  // 'ANIM' precedes frames.
        status = ParseAnimationFrame(dmux, chunk_size_padded);
        break;
      }
      case MKFOURCC('I', 'C', 'C', 'P'): {
        store_chunk = !!(dmux->feature_flags_ & ICCP_FLAG);
        goto Skip;
      }
      case MKFOURCC('E', 'X', 'I', 'F'): {
        store_chunk = !!(dmux->feature_flags_ & EXIF_FLAG);
        goto Skip;
      }
      case MKFOURCC('X', 'M', 'P', ' '): {
        store_chunk = !!(dmux->feature_flags_ & XMP_FLAG);
        goto Skip;
      }
 Skip:
      default: {
        if (chunk_size_padded <= MemDataSize(mem)) {
          if (store_chunk) {
            // Store only the chunk header and unpadded size as only the payload
            // will be returned to the user.
            if (!StoreChunk(dmux, chunk_start_offset,
                            CHUNK_HEADER_SIZE + chunk_size)) {
              return PARSE_ERROR;
            }
          }
          Skip(mem, chunk_size_padded);
        } else {
          status = PARSE_NEED_MORE_DATA;
        }
      }
    }

    if (mem->start_ == mem->riff_end_) {
      break;
    } else if (MemDataSize(mem) < CHUNK_HEADER_SIZE) {
      status = PARSE_NEED_MORE_DATA;
    }
  } while (status == PARSE_OK);

  return status;
}

static ParseStatus ParseVP8X(WebPDemuxer* const dmux) {
  MemBuffer* const mem = &dmux->mem_;
  uint32_t vp8x_size;

  if (MemDataSize(mem) < CHUNK_HEADER_SIZE) return PARSE_NEED_MORE_DATA;

  dmux->is_ext_format_ = 1;
  Skip(mem, TAG_SIZE);  // VP8X
  vp8x_size = ReadLE32(mem);
  if (vp8x_size > MAX_CHUNK_PAYLOAD) return PARSE_ERROR;
  if (vp8x_size < VP8X_CHUNK_SIZE) return PARSE_ERROR;
  vp8x_size += vp8x_size & 1;
  if (SizeIsInvalid(mem, vp8x_size)) return PARSE_ERROR;
  if (MemDataSize(mem) < vp8x_size) return PARSE_NEED_MORE_DATA;

  dmux->feature_flags_ = ReadByte(mem);
  Skip(mem, 3);  // Reserved.
  dmux->canvas_width_  = 1 + ReadLE24s(mem);
  dmux->canvas_height_ = 1 + ReadLE24s(mem);
  if (dmux->canvas_width_ * (uint64_t)dmux->canvas_height_ >= MAX_IMAGE_AREA) {
    return PARSE_ERROR;  // image final dimension is too large
  }
  Skip(mem, vp8x_size - VP8X_CHUNK_SIZE);  // skip any trailing data.
  dmux->state_ = WEBP_DEMUX_PARSED_HEADER;

  if (SizeIsInvalid(mem, CHUNK_HEADER_SIZE)) return PARSE_ERROR;
  if (MemDataSize(mem) < CHUNK_HEADER_SIZE) return PARSE_NEED_MORE_DATA;

  return ParseVP8XChunks(dmux);
}

// -----------------------------------------------------------------------------
// Format validation

static int IsValidSimpleFormat(const WebPDemuxer* const dmux) {
  const Frame* const frame = dmux->frames_;
  if (dmux->state_ == WEBP_DEMUX_PARSING_HEADER) return 1;

  if (dmux->canvas_width_ <= 0 || dmux->canvas_height_ <= 0) return 0;
  if (dmux->state_ == WEBP_DEMUX_DONE && frame == NULL) return 0;

  if (frame->width_ <= 0 || frame->height_ <= 0) return 0;
  return 1;
}

// If 'exact' is true, check that the image resolution matches the canvas.
// If 'exact' is false, check that the x/y offsets do not exceed the canvas.
static int CheckFrameBounds(const Frame* const frame, int exact,
                            int canvas_width, int canvas_height) {
  if (exact) {
    if (frame->x_offset_ != 0 || frame->y_offset_ != 0) {
      return 0;
    }
    if (frame->width_ != canvas_width || frame->height_ != canvas_height) {
      return 0;
    }
  } else {
    if (frame->x_offset_ < 0 || frame->y_offset_ < 0) return 0;
    if (frame->width_ + frame->x_offset_ > canvas_width) return 0;
    if (frame->height_ + frame->y_offset_ > canvas_height) return 0;
  }
  return 1;
}

static int IsValidExtendedFormat(const WebPDemuxer* const dmux) {
  const int is_animation = !!(dmux->feature_flags_ & ANIMATION_FLAG);
  const Frame* f = dmux->frames_;

  if (dmux->state_ == WEBP_DEMUX_PARSING_HEADER) return 1;

  if (dmux->canvas_width_ <= 0 || dmux->canvas_height_ <= 0) return 0;
  if (dmux->loop_count_ < 0) return 0;
  if (dmux->state_ == WEBP_DEMUX_DONE && dmux->frames_ == NULL) return 0;
  if (dmux->feature_flags_ & ~ALL_VALID_FLAGS) return 0;  // invalid bitstream

  while (f != NULL) {
    const int cur_frame_set = f->frame_num_;

    // Check frame properties.
    for (; f != NULL && f->frame_num_ == cur_frame_set; f = f->next_) {
      const ChunkData* const image = f->img_components_;
      const ChunkData* const alpha = f->img_components_ + 1;

      if (!is_animation && f->frame_num_ > 1) return 0;

      if (f->complete_) {
        if (alpha->size_ == 0 && image->size_ == 0) return 0;
        // Ensure alpha precedes image bitstream.
        if (alpha->size_ > 0 && alpha->offset_ > image->offset_) {
          return 0;
        }

        if (f->width_ <= 0 || f->height_ <= 0) return 0;
      } else {
        // There shouldn't be a partial frame in a complete file.
        if (dmux->state_ == WEBP_DEMUX_DONE) return 0;

        // Ensure alpha precedes image bitstream.
        if (alpha->size_ > 0 && image->size_ > 0 &&
            alpha->offset_ > image->offset_) {
          return 0;
        }
        // There shouldn't be any frames after an incomplete one.
        if (f->next_ != NULL) return 0;
      }

      if (f->width_ > 0 && f->height_ > 0 &&
          !CheckFrameBounds(f, !is_animation,
                            dmux->canvas_width_, dmux->canvas_height_)) {
        return 0;
      }
    }
  }
  return 1;
}

// -----------------------------------------------------------------------------
// WebPDemuxer object

static void InitDemux(WebPDemuxer* const dmux, const MemBuffer* const mem) {
  dmux->state_ = WEBP_DEMUX_PARSING_HEADER;
  dmux->loop_count_ = 1;
  dmux->bgcolor_ = 0xFFFFFFFF;  // White background by default.
  dmux->canvas_width_ = -1;
  dmux->canvas_height_ = -1;
  dmux->frames_tail_ = &dmux->frames_;
  dmux->chunks_tail_ = &dmux->chunks_;
  dmux->mem_ = *mem;
}

static ParseStatus CreateRawImageDemuxer(MemBuffer* const mem,
                                         WebPDemuxer** demuxer) {
  WebPBitstreamFeatures features;
  const VP8StatusCode status =
      WebPGetFeatures(mem->buf_, mem->buf_size_, &features);
  *demuxer = NULL;
  if (status != VP8_STATUS_OK) {
    return (status == VP8_STATUS_NOT_ENOUGH_DATA) ? PARSE_NEED_MORE_DATA
                                                  : PARSE_ERROR;
  }

  {
    WebPDemuxer* const dmux = (WebPDemuxer*)WebPSafeCalloc(1ULL, sizeof(*dmux));
    Frame* const frame = (Frame*)WebPSafeCalloc(1ULL, sizeof(*frame));
    if (dmux == NULL || frame == NULL) goto Error;
    InitDemux(dmux, mem);
    SetFrameInfo(0, mem->buf_size_, 1 /*frame_num*/, 1 /*complete*/, &features,
                 frame);
    if (!AddFrame(dmux, frame)) goto Error;
    dmux->state_ = WEBP_DEMUX_DONE;
    dmux->canvas_width_ = frame->width_;
    dmux->canvas_height_ = frame->height_;
    dmux->feature_flags_ |= frame->has_alpha_ ? ALPHA_FLAG : 0;
    dmux->num_frames_ = 1;
    assert(IsValidSimpleFormat(dmux));
    *demuxer = dmux;
    return PARSE_OK;

 Error:
    WebPSafeFree(dmux);
    WebPSafeFree(frame);
    return PARSE_ERROR;
  }
}

WebPDemuxer* WebPDemuxInternal(const WebPData* data, int allow_partial,
                               WebPDemuxState* state, int version) {
  const ChunkParser* parser;
  int partial;
  ParseStatus status = PARSE_ERROR;
  MemBuffer mem;
  WebPDemuxer* dmux;

  if (state != NULL) *state = WEBP_DEMUX_PARSE_ERROR;

  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_DEMUX_ABI_VERSION)) return NULL;
  if (data == NULL || data->bytes == NULL || data->size == 0) return NULL;

  if (!InitMemBuffer(&mem, data->bytes, data->size)) return NULL;
  status = ReadHeader(&mem);
  if (status != PARSE_OK) {
    // If parsing of the webp file header fails attempt to handle a raw
    // VP8/VP8L frame. Note 'allow_partial' is ignored in this case.
    if (status == PARSE_ERROR) {
      status = CreateRawImageDemuxer(&mem, &dmux);
      if (status == PARSE_OK) {
        if (state != NULL) *state = WEBP_DEMUX_DONE;
        return dmux;
      }
    }
    if (state != NULL) {
      *state = (status == PARSE_NEED_MORE_DATA) ? WEBP_DEMUX_PARSING_HEADER
                                                : WEBP_DEMUX_PARSE_ERROR;
    }
    return NULL;
  }

  partial = (mem.buf_size_ < mem.riff_end_);
  if (!allow_partial && partial) return NULL;

  dmux = (WebPDemuxer*)WebPSafeCalloc(1ULL, sizeof(*dmux));
  if (dmux == NULL) return NULL;
  InitDemux(dmux, &mem);

  status = PARSE_ERROR;
  for (parser = kMasterChunks; parser->parse != NULL; ++parser) {
    if (!memcmp(parser->id, GetBuffer(&dmux->mem_), TAG_SIZE)) {
      status = parser->parse(dmux);
      if (status == PARSE_OK) dmux->state_ = WEBP_DEMUX_DONE;
      if (status == PARSE_NEED_MORE_DATA && !partial) status = PARSE_ERROR;
      if (status != PARSE_ERROR && !parser->valid(dmux)) status = PARSE_ERROR;
      if (status == PARSE_ERROR) dmux->state_ = WEBP_DEMUX_PARSE_ERROR;
      break;
    }
  }
  if (state != NULL) *state = dmux->state_;

  if (status == PARSE_ERROR) {
    WebPDemuxDelete(dmux);
    return NULL;
  }
  return dmux;
}

void WebPDemuxDelete(WebPDemuxer* dmux) {
  Chunk* c;
  Frame* f;
  if (dmux == NULL) return;

  for (f = dmux->frames_; f != NULL;) {
    Frame* const cur_frame = f;
    f = f->next_;
    WebPSafeFree(cur_frame);
  }
  for (c = dmux->chunks_; c != NULL;) {
    Chunk* const cur_chunk = c;
    c = c->next_;
    WebPSafeFree(cur_chunk);
  }
  WebPSafeFree(dmux);
}

// -----------------------------------------------------------------------------

uint32_t WebPDemuxGetI(const WebPDemuxer* dmux, WebPFormatFeature feature) {
  if (dmux == NULL) return 0;

  switch (feature) {
    case WEBP_FF_FORMAT_FLAGS:     return dmux->feature_flags_;
    case WEBP_FF_CANVAS_WIDTH:     return (uint32_t)dmux->canvas_width_;
    case WEBP_FF_CANVAS_HEIGHT:    return (uint32_t)dmux->canvas_height_;
    case WEBP_FF_LOOP_COUNT:       return (uint32_t)dmux->loop_count_;
    case WEBP_FF_BACKGROUND_COLOR: return dmux->bgcolor_;
    case WEBP_FF_FRAME_COUNT:      return (uint32_t)dmux->num_frames_;
  }
  return 0;
}

// -----------------------------------------------------------------------------
// Frame iteration

static const Frame* GetFrame(const WebPDemuxer* const dmux, int frame_num) {
  const Frame* f;
  for (f = dmux->frames_; f != NULL; f = f->next_) {
    if (frame_num == f->frame_num_) break;
  }
  return f;
}

static const uint8_t* GetFramePayload(const uint8_t* const mem_buf,
                                      const Frame* const frame,
                                      size_t* const data_size) {
  *data_size = 0;
  if (frame != NULL) {
    const ChunkData* const image = frame->img_components_;
    const ChunkData* const alpha = frame->img_components_ + 1;
    size_t start_offset = image->offset_;
    *data_size = image->size_;

    // if alpha exists it precedes image, update the size allowing for
    // intervening chunks.
    if (alpha->size_ > 0) {
      const size_t inter_size = (image->offset_ > 0)
                              ? image->offset_ - (alpha->offset_ + alpha->size_)
                              : 0;
      start_offset = alpha->offset_;
      *data_size  += alpha->size_ + inter_size;
    }
    return mem_buf + start_offset;
  }
  return NULL;
}

// Create a whole 'frame' from VP8 (+ alpha) or lossless.
static int SynthesizeFrame(const WebPDemuxer* const dmux,
                           const Frame* const frame,
                           WebPIterator* const iter) {
  const uint8_t* const mem_buf = dmux->mem_.buf_;
  size_t payload_size = 0;
  const uint8_t* const payload = GetFramePayload(mem_buf, frame, &payload_size);
  if (payload == NULL) return 0;
  assert(frame != NULL);

  iter->frame_num      = frame->frame_num_;
  iter->num_frames     = dmux->num_frames_;
  iter->x_offset       = frame->x_offset_;
  iter->y_offset       = frame->y_offset_;
  iter->width          = frame->width_;
  iter->height         = frame->height_;
  iter->has_alpha      = frame->has_alpha_;
  iter->duration       = frame->duration_;
  iter->dispose_method = frame->dispose_method_;
  iter->blend_method   = frame->blend_method_;
  iter->complete       = frame->complete_;
  iter->fragment.bytes = payload;
  iter->fragment.size  = payload_size;
  return 1;
}

static int SetFrame(int frame_num, WebPIterator* const iter) {
  const Frame* frame;
  const WebPDemuxer* const dmux = (WebPDemuxer*)iter->private_;
  if (dmux == NULL || frame_num < 0) return 0;
  if (frame_num > dmux->num_frames_) return 0;
  if (frame_num == 0) frame_num = dmux->num_frames_;

  frame = GetFrame(dmux, frame_num);
  if (frame == NULL) return 0;

  return SynthesizeFrame(dmux, frame, iter);
}

int WebPDemuxGetFrame(const WebPDemuxer* dmux, int frame, WebPIterator* iter) {
  if (iter == NULL) return 0;

  memset(iter, 0, sizeof(*iter));
  iter->private_ = (void*)dmux;
  return SetFrame(frame, iter);
}

int WebPDemuxNextFrame(WebPIterator* iter) {
  if (iter == NULL) return 0;
  return SetFrame(iter->frame_num + 1, iter);
}

int WebPDemuxPrevFrame(WebPIterator* iter) {
  if (iter == NULL) return 0;
  if (iter->frame_num <= 1) return 0;
  return SetFrame(iter->frame_num - 1, iter);
}

void WebPDemuxReleaseIterator(WebPIterator* iter) {
  (void)iter;
}

// -----------------------------------------------------------------------------
// Chunk iteration

static int ChunkCount(const WebPDemuxer* const dmux, const char fourcc[4]) {
  const uint8_t* const mem_buf = dmux->mem_.buf_;
  const Chunk* c;
  int count = 0;
  for (c = dmux->chunks_; c != NULL; c = c->next_) {
    const uint8_t* const header = mem_buf + c->data_.offset_;
    if (!memcmp(header, fourcc, TAG_SIZE)) ++count;
  }
  return count;
}

static const Chunk* GetChunk(const WebPDemuxer* const dmux,
                             const char fourcc[4], int chunk_num) {
  const uint8_t* const mem_buf = dmux->mem_.buf_;
  const Chunk* c;
  int count = 0;
  for (c = dmux->chunks_; c != NULL; c = c->next_) {
    const uint8_t* const header = mem_buf + c->data_.offset_;
    if (!memcmp(header, fourcc, TAG_SIZE)) ++count;
    if (count == chunk_num) break;
  }
  return c;
}

static int SetChunk(const char fourcc[4], int chunk_num,
                    WebPChunkIterator* const iter) {
  const WebPDemuxer* const dmux = (WebPDemuxer*)iter->private_;
  int count;

  if (dmux == NULL || fourcc == NULL || chunk_num < 0) return 0;
  count = ChunkCount(dmux, fourcc);
  if (count == 0) return 0;
  if (chunk_num == 0) chunk_num = count;

  if (chunk_num <= count) {
    const uint8_t* const mem_buf = dmux->mem_.buf_;
    const Chunk* const chunk = GetChunk(dmux, fourcc, chunk_num);
    iter->chunk.bytes = mem_buf + chunk->data_.offset_ + CHUNK_HEADER_SIZE;
    iter->chunk.size  = chunk->data_.size_ - CHUNK_HEADER_SIZE;
    iter->num_chunks  = count;
    iter->chunk_num   = chunk_num;
    return 1;
  }
  return 0;
}

int WebPDemuxGetChunk(const WebPDemuxer* dmux,
                      const char fourcc[4], int chunk_num,
                      WebPChunkIterator* iter) {
  if (iter == NULL) return 0;

  memset(iter, 0, sizeof(*iter));
  iter->private_ = (void*)dmux;
  return SetChunk(fourcc, chunk_num, iter);
}

int WebPDemuxNextChunk(WebPChunkIterator* iter) {
  if (iter != NULL) {
    const char* const fourcc =
        (const char*)iter->chunk.bytes - CHUNK_HEADER_SIZE;
    return SetChunk(fourcc, iter->chunk_num + 1, iter);
  }
  return 0;
}

int WebPDemuxPrevChunk(WebPChunkIterator* iter) {
  if (iter != NULL && iter->chunk_num > 1) {
    const char* const fourcc =
        (const char*)iter->chunk.bytes - CHUNK_HEADER_SIZE;
    return SetChunk(fourcc, iter->chunk_num - 1, iter);
  }
  return 0;
}

void WebPDemuxReleaseChunkIterator(WebPChunkIterator* iter) {
  (void)iter;
}

