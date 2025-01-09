// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// VP8 decoder: internal header.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_DEC_VP8I_DEC_H_
#define WEBP_DEC_VP8I_DEC_H_

#include <string.h>     // for memcpy()
#include "src/dec/common_dec.h"
#include "src/dec/vp8li_dec.h"
#include "src/utils/bit_reader_utils.h"
#include "src/utils/random_utils.h"
#include "src/utils/thread_utils.h"
#include "src/dsp/dsp.h"
#include "src/webp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
// Various defines and enums

// version numbers
#define DEC_MAJ_VERSION 1
#define DEC_MIN_VERSION 5
#define DEC_REV_VERSION 0

// YUV-cache parameters. Cache is 32-bytes wide (= one cacheline).
// Constraints are: We need to store one 16x16 block of luma samples (y),
// and two 8x8 chroma blocks (u/v). These are better be 16-bytes aligned,
// in order to be SIMD-friendly. We also need to store the top, left and
// top-left samples (from previously decoded blocks), along with four
// extra top-right samples for luma (intra4x4 prediction only).
// One possible layout is, using 32 * (17 + 9) bytes:
//
//   .+------   <- only 1 pixel high
//   .|yyyyt.
//   .|yyyyt.
//   .|yyyyt.
//   .|yyyy..
//   .+--.+--   <- only 1 pixel high
//   .|uu.|vv
//   .|uu.|vv
//
// Every character is a 4x4 block, with legend:
//  '.' = unused
//  'y' = y-samples   'u' = u-samples     'v' = u-samples
//  '|' = left sample,   '-' = top sample,    '+' = top-left sample
//  't' = extra top-right sample for 4x4 modes
#define YUV_SIZE (BPS * 17 + BPS * 9)
#define Y_OFF    (BPS * 1 + 8)
#define U_OFF    (Y_OFF + BPS * 16 + BPS)
#define V_OFF    (U_OFF + 16)

// minimal width under which lossy multi-threading is always disabled
#define MIN_WIDTH_FOR_THREADS 512

//------------------------------------------------------------------------------
// Headers

typedef struct {
  uint8_t key_frame_;
  uint8_t profile_;
  uint8_t show_;
  uint32_t partition_length_;
} VP8FrameHeader;

typedef struct {
  uint16_t width_;
  uint16_t height_;
  uint8_t xscale_;
  uint8_t yscale_;
  uint8_t colorspace_;   // 0 = YCbCr
  uint8_t clamp_type_;
} VP8PictureHeader;

// segment features
typedef struct {
  int use_segment_;
  int update_map_;        // whether to update the segment map or not
  int absolute_delta_;    // absolute or delta values for quantizer and filter
  int8_t quantizer_[NUM_MB_SEGMENTS];        // quantization changes
  int8_t filter_strength_[NUM_MB_SEGMENTS];  // filter strength for segments
} VP8SegmentHeader;

// probas associated to one of the contexts
typedef uint8_t VP8ProbaArray[NUM_PROBAS];

typedef struct {   // all the probas associated to one band
  VP8ProbaArray probas_[NUM_CTX];
} VP8BandProbas;

// Struct collecting all frame-persistent probabilities.
typedef struct {
  uint8_t segments_[MB_FEATURE_TREE_PROBS];
  // Type: 0:Intra16-AC  1:Intra16-DC   2:Chroma   3:Intra4
  VP8BandProbas bands_[NUM_TYPES][NUM_BANDS];
  const VP8BandProbas* bands_ptr_[NUM_TYPES][16 + 1];
} VP8Proba;

// Filter parameters
typedef struct {
  int simple_;                  // 0=complex, 1=simple
  int level_;                   // [0..63]
  int sharpness_;               // [0..7]
  int use_lf_delta_;
  int ref_lf_delta_[NUM_REF_LF_DELTAS];
  int mode_lf_delta_[NUM_MODE_LF_DELTAS];
} VP8FilterHeader;

//------------------------------------------------------------------------------
// Informations about the macroblocks.

typedef struct {  // filter specs
  uint8_t f_limit_;      // filter limit in [3..189], or 0 if no filtering
  uint8_t f_ilevel_;     // inner limit in [1..63]
  uint8_t f_inner_;      // do inner filtering?
  uint8_t hev_thresh_;   // high edge variance threshold in [0..2]
} VP8FInfo;

typedef struct {  // Top/Left Contexts used for syntax-parsing
  uint8_t nz_;        // non-zero AC/DC coeffs (4bit for luma + 4bit for chroma)
  uint8_t nz_dc_;     // non-zero DC coeff (1bit)
} VP8MB;

// Dequantization matrices
typedef int quant_t[2];      // [DC / AC].  Can be 'uint16_t[2]' too (~slower).
typedef struct {
  quant_t y1_mat_, y2_mat_, uv_mat_;

  int uv_quant_;   // U/V quantizer value
  int dither_;     // dithering amplitude (0 = off, max=255)
} VP8QuantMatrix;

// Data needed to reconstruct a macroblock
typedef struct {
  int16_t coeffs_[384];   // 384 coeffs = (16+4+4) * 4*4
  uint8_t is_i4x4_;       // true if intra4x4
  uint8_t imodes_[16];    // one 16x16 mode (#0) or sixteen 4x4 modes
  uint8_t uvmode_;        // chroma prediction mode
  // bit-wise info about the content of each sub-4x4 blocks (in decoding order).
  // Each of the 4x4 blocks for y/u/v is associated with a 2b code according to:
  //   code=0 -> no coefficient
  //   code=1 -> only DC
  //   code=2 -> first three coefficients are non-zero
  //   code=3 -> more than three coefficients are non-zero
  // This allows to call specialized transform functions.
  uint32_t non_zero_y_;
  uint32_t non_zero_uv_;
  uint8_t dither_;      // local dithering strength (deduced from non_zero_*)
  uint8_t skip_;
  uint8_t segment_;
} VP8MBData;

// Persistent information needed by the parallel processing
typedef struct {
  int id_;              // cache row to process (in [0..2])
  int mb_y_;            // macroblock position of the row
  int filter_row_;      // true if row-filtering is needed
  VP8FInfo* f_info_;    // filter strengths (swapped with dec->f_info_)
  VP8MBData* mb_data_;  // reconstruction data (swapped with dec->mb_data_)
  VP8Io io_;            // copy of the VP8Io to pass to put()
} VP8ThreadContext;

// Saved top samples, per macroblock. Fits into a cache-line.
typedef struct {
  uint8_t y[16], u[8], v[8];
} VP8TopSamples;

//------------------------------------------------------------------------------
// VP8Decoder: the main opaque structure handed over to user

struct VP8Decoder {
  VP8StatusCode status_;
  int ready_;     // true if ready to decode a picture with VP8Decode()
  const char* error_msg_;  // set when status_ is not OK.

  // Main data source
  VP8BitReader br_;
  int incremental_;  // if true, incremental decoding is expected

  // headers
  VP8FrameHeader   frm_hdr_;
  VP8PictureHeader pic_hdr_;
  VP8FilterHeader  filter_hdr_;
  VP8SegmentHeader segment_hdr_;

  // Worker
  WebPWorker worker_;
  int mt_method_;      // multi-thread method: 0=off, 1=[parse+recon][filter]
                       // 2=[parse][recon+filter]
  int cache_id_;       // current cache row
  int num_caches_;     // number of cached rows of 16 pixels (1, 2 or 3)
  VP8ThreadContext thread_ctx_;  // Thread context

  // dimension, in macroblock units.
  int mb_w_, mb_h_;

  // Macroblock to process/filter, depending on cropping and filter_type.
  int tl_mb_x_, tl_mb_y_;  // top-left MB that must be in-loop filtered
  int br_mb_x_, br_mb_y_;  // last bottom-right MB that must be decoded

  // number of partitions minus one.
  uint32_t num_parts_minus_one_;
  // per-partition boolean decoders.
  VP8BitReader parts_[MAX_NUM_PARTITIONS];

  // Dithering strength, deduced from decoding options
  int dither_;                // whether to use dithering or not
  VP8Random dithering_rg_;    // random generator for dithering

  // dequantization (one set of DC/AC dequant factor per segment)
  VP8QuantMatrix dqm_[NUM_MB_SEGMENTS];

  // probabilities
  VP8Proba proba_;
  int use_skip_proba_;
  uint8_t skip_p_;

  // Boundary data cache and persistent buffers.
  uint8_t* intra_t_;      // top intra modes values: 4 * mb_w_
  uint8_t  intra_l_[4];   // left intra modes values

  VP8TopSamples* yuv_t_;  // top y/u/v samples

  VP8MB* mb_info_;        // contextual macroblock info (mb_w_ + 1)
  VP8FInfo* f_info_;      // filter strength info
  uint8_t* yuv_b_;        // main block for Y/U/V (size = YUV_SIZE)

  uint8_t* cache_y_;      // macroblock row for storing unfiltered samples
  uint8_t* cache_u_;
  uint8_t* cache_v_;
  int cache_y_stride_;
  int cache_uv_stride_;

  // main memory chunk for the above data. Persistent.
  void* mem_;
  size_t mem_size_;

  // Per macroblock non-persistent infos.
  int mb_x_, mb_y_;       // current position, in macroblock units
  VP8MBData* mb_data_;    // parsed reconstruction data

  // Filtering side-info
  int filter_type_;                          // 0=off, 1=simple, 2=complex
  VP8FInfo fstrengths_[NUM_MB_SEGMENTS][2];  // precalculated per-segment/type

  // Alpha
  struct ALPHDecoder* alph_dec_;  // alpha-plane decoder object
  const uint8_t* alpha_data_;     // compressed alpha data (if present)
  size_t alpha_data_size_;
  int is_alpha_decoded_;      // true if alpha_data_ is decoded in alpha_plane_
  uint8_t* alpha_plane_mem_;  // memory allocated for alpha_plane_
  uint8_t* alpha_plane_;      // output. Persistent, contains the whole data.
  const uint8_t* alpha_prev_line_;  // last decoded alpha row (or NULL)
  int alpha_dithering_;       // derived from decoding options (0=off, 100=full)
};

//------------------------------------------------------------------------------
// internal functions. Not public.

// in vp8.c
int VP8SetError(VP8Decoder* const dec,
                VP8StatusCode error, const char* const msg);

// in tree.c
void VP8ResetProba(VP8Proba* const proba);
void VP8ParseProba(VP8BitReader* const br, VP8Decoder* const dec);
// parses one row of intra mode data in partition 0, returns !eof
int VP8ParseIntraModeRow(VP8BitReader* const br, VP8Decoder* const dec);

// in quant.c
void VP8ParseQuant(VP8Decoder* const dec);

// in frame.c
WEBP_NODISCARD int VP8InitFrame(VP8Decoder* const dec, VP8Io* const io);
// Call io->setup() and finish setting up scan parameters.
// After this call returns, one must always call VP8ExitCritical() with the
// same parameters. Both functions should be used in pair. Returns VP8_STATUS_OK
// if ok, otherwise sets and returns the error status on *dec.
VP8StatusCode VP8EnterCritical(VP8Decoder* const dec, VP8Io* const io);
// Must always be called in pair with VP8EnterCritical().
// Returns false in case of error.
WEBP_NODISCARD int VP8ExitCritical(VP8Decoder* const dec, VP8Io* const io);
// Return the multi-threading method to use (0=off), depending
// on options and bitstream size. Only for lossy decoding.
int VP8GetThreadMethod(const WebPDecoderOptions* const options,
                       const WebPHeaderStructure* const headers,
                       int width, int height);
// Initialize dithering post-process if needed.
void VP8InitDithering(const WebPDecoderOptions* const options,
                      VP8Decoder* const dec);
// Process the last decoded row (filtering + output).
WEBP_NODISCARD int VP8ProcessRow(VP8Decoder* const dec, VP8Io* const io);
// To be called at the start of a new scanline, to initialize predictors.
void VP8InitScanline(VP8Decoder* const dec);
// Decode one macroblock. Returns false if there is not enough data.
WEBP_NODISCARD int VP8DecodeMB(VP8Decoder* const dec,
                               VP8BitReader* const token_br);

// in alpha.c
const uint8_t* VP8DecompressAlphaRows(VP8Decoder* const dec,
                                      const VP8Io* const io,
                                      int row, int num_rows);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_DEC_VP8I_DEC_H_
