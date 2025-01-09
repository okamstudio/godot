// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Boolean decoder non-inlined methods
//
// Author: Skal (pascal.massimino@gmail.com)

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#include "src/dsp/cpu.h"
#include "src/utils/bit_reader_inl_utils.h"
#include "src/utils/utils.h"

//------------------------------------------------------------------------------
// VP8BitReader

void VP8BitReaderSetBuffer(VP8BitReader* const br,
                           const uint8_t* const start,
                           size_t size) {
  br->buf_     = start;
  br->buf_end_ = start + size;
  br->buf_max_ =
      (size >= sizeof(lbit_t)) ? start + size - sizeof(lbit_t) + 1
                               : start;
}

void VP8InitBitReader(VP8BitReader* const br,
                      const uint8_t* const start, size_t size) {
  assert(br != NULL);
  assert(start != NULL);
  assert(size < (1u << 31));   // limit ensured by format and upstream checks
  br->range_   = 255 - 1;
  br->value_   = 0;
  br->bits_    = -8;   // to load the very first 8bits
  br->eof_     = 0;
  VP8BitReaderSetBuffer(br, start, size);
  VP8LoadNewBytes(br);
}

void VP8RemapBitReader(VP8BitReader* const br, ptrdiff_t offset) {
  if (br->buf_ != NULL) {
    br->buf_ += offset;
    br->buf_end_ += offset;
    br->buf_max_ += offset;
  }
}

const uint8_t kVP8Log2Range[128] = {
     7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0
};

// range = ((range - 1) << kVP8Log2Range[range]) + 1
const uint8_t kVP8NewRange[128] = {
  127, 127, 191, 127, 159, 191, 223, 127,
  143, 159, 175, 191, 207, 223, 239, 127,
  135, 143, 151, 159, 167, 175, 183, 191,
  199, 207, 215, 223, 231, 239, 247, 127,
  131, 135, 139, 143, 147, 151, 155, 159,
  163, 167, 171, 175, 179, 183, 187, 191,
  195, 199, 203, 207, 211, 215, 219, 223,
  227, 231, 235, 239, 243, 247, 251, 127,
  129, 131, 133, 135, 137, 139, 141, 143,
  145, 147, 149, 151, 153, 155, 157, 159,
  161, 163, 165, 167, 169, 171, 173, 175,
  177, 179, 181, 183, 185, 187, 189, 191,
  193, 195, 197, 199, 201, 203, 205, 207,
  209, 211, 213, 215, 217, 219, 221, 223,
  225, 227, 229, 231, 233, 235, 237, 239,
  241, 243, 245, 247, 249, 251, 253, 127
};

void VP8LoadFinalBytes(VP8BitReader* const br) {
  assert(br != NULL && br->buf_ != NULL);
  // Only read 8bits at a time
  if (br->buf_ < br->buf_end_) {
    br->bits_ += 8;
    br->value_ = (bit_t)(*br->buf_++) | (br->value_ << 8);
  } else if (!br->eof_) {
    br->value_ <<= 8;
    br->bits_ += 8;
    br->eof_ = 1;
  } else {
    br->bits_ = 0;  // This is to avoid undefined behaviour with shifts.
  }
}

//------------------------------------------------------------------------------
// Higher-level calls

uint32_t VP8GetValue(VP8BitReader* const br, int bits, const char label[]) {
  uint32_t v = 0;
  while (bits-- > 0) {
    v |= VP8GetBit(br, 0x80, label) << bits;
  }
  return v;
}

int32_t VP8GetSignedValue(VP8BitReader* const br, int bits,
                          const char label[]) {
  const int value = VP8GetValue(br, bits, label);
  return VP8Get(br, label) ? -value : value;
}

//------------------------------------------------------------------------------
// VP8LBitReader

#define VP8L_LOG8_WBITS 4  // Number of bytes needed to store VP8L_WBITS bits.

#if defined(__arm__) || defined(_M_ARM) || WEBP_AARCH64 || \
    defined(__i386__) || defined(_M_IX86) || \
    defined(__x86_64__) || defined(_M_X64) || \
    defined(__wasm__)
#define VP8L_USE_FAST_LOAD
#endif

static const uint32_t kBitMask[VP8L_MAX_NUM_BIT_READ + 1] = {
  0,
  0x000001, 0x000003, 0x000007, 0x00000f,
  0x00001f, 0x00003f, 0x00007f, 0x0000ff,
  0x0001ff, 0x0003ff, 0x0007ff, 0x000fff,
  0x001fff, 0x003fff, 0x007fff, 0x00ffff,
  0x01ffff, 0x03ffff, 0x07ffff, 0x0fffff,
  0x1fffff, 0x3fffff, 0x7fffff, 0xffffff
};

void VP8LInitBitReader(VP8LBitReader* const br, const uint8_t* const start,
                       size_t length) {
  size_t i;
  vp8l_val_t value = 0;
  assert(br != NULL);
  assert(start != NULL);
  assert(length < 0xfffffff8u);   // can't happen with a RIFF chunk.

  br->len_ = length;
  br->val_ = 0;
  br->bit_pos_ = 0;
  br->eos_ = 0;

  if (length > sizeof(br->val_)) {
    length = sizeof(br->val_);
  }
  for (i = 0; i < length; ++i) {
    value |= (vp8l_val_t)start[i] << (8 * i);
  }
  br->val_ = value;
  br->pos_ = length;
  br->buf_ = start;
}

void VP8LBitReaderSetBuffer(VP8LBitReader* const br,
                            const uint8_t* const buf, size_t len) {
  assert(br != NULL);
  assert(buf != NULL);
  assert(len < 0xfffffff8u);   // can't happen with a RIFF chunk.
  br->buf_ = buf;
  br->len_ = len;
  // pos_ > len_ should be considered a param error.
  br->eos_ = (br->pos_ > br->len_) || VP8LIsEndOfStream(br);
}

static void VP8LSetEndOfStream(VP8LBitReader* const br) {
  br->eos_ = 1;
  br->bit_pos_ = 0;  // To avoid undefined behaviour with shifts.
}

// If not at EOS, reload up to VP8L_LBITS byte-by-byte
static void ShiftBytes(VP8LBitReader* const br) {
  while (br->bit_pos_ >= 8 && br->pos_ < br->len_) {
    br->val_ >>= 8;
    br->val_ |= ((vp8l_val_t)br->buf_[br->pos_]) << (VP8L_LBITS - 8);
    ++br->pos_;
    br->bit_pos_ -= 8;
  }
  if (VP8LIsEndOfStream(br)) {
    VP8LSetEndOfStream(br);
  }
}

void VP8LDoFillBitWindow(VP8LBitReader* const br) {
  assert(br->bit_pos_ >= VP8L_WBITS);
#if defined(VP8L_USE_FAST_LOAD)
  if (br->pos_ + sizeof(br->val_) < br->len_) {
    br->val_ >>= VP8L_WBITS;
    br->bit_pos_ -= VP8L_WBITS;
    br->val_ |= (vp8l_val_t)HToLE32(WebPMemToUint32(br->buf_ + br->pos_)) <<
                (VP8L_LBITS - VP8L_WBITS);
    br->pos_ += VP8L_LOG8_WBITS;
    return;
  }
#endif
  ShiftBytes(br);       // Slow path.
}

uint32_t VP8LReadBits(VP8LBitReader* const br, int n_bits) {
  assert(n_bits >= 0);
  // Flag an error if end_of_stream or n_bits is more than allowed limit.
  if (!br->eos_ && n_bits <= VP8L_MAX_NUM_BIT_READ) {
    const uint32_t val = VP8LPrefetchBits(br) & kBitMask[n_bits];
    const int new_bits = br->bit_pos_ + n_bits;
    br->bit_pos_ = new_bits;
    ShiftBytes(br);
    return val;
  } else {
    VP8LSetEndOfStream(br);
    return 0;
  }
}

//------------------------------------------------------------------------------
// Bit-tracing tool

#if (BITTRACE > 0)

#include <stdlib.h>   // for atexit()
#include <stdio.h>
#include <string.h>

#define MAX_NUM_LABELS 32
static struct {
  const char* label;
  int size;
  int count;
} kLabels[MAX_NUM_LABELS];

static int last_label = 0;
static int last_pos = 0;
static const uint8_t* buf_start = NULL;
static int init_done = 0;

static void PrintBitTraces(void) {
  int i;
  int scale = 1;
  int total = 0;
  const char* units = "bits";
#if (BITTRACE == 2)
  scale = 8;
  units = "bytes";
#endif
  for (i = 0; i < last_label; ++i) total += kLabels[i].size;
  if (total < 1) total = 1;   // avoid rounding errors
  printf("=== Bit traces ===\n");
  for (i = 0; i < last_label; ++i) {
    const int skip = 16 - (int)strlen(kLabels[i].label);
    const int value = (kLabels[i].size + scale - 1) / scale;
    assert(skip > 0);
    printf("%s \%*s: %6d %s   \t[%5.2f%%] [count: %7d]\n",
           kLabels[i].label, skip, "", value, units,
           100.f * kLabels[i].size / total,
           kLabels[i].count);
  }
  total = (total + scale - 1) / scale;
  printf("Total: %d %s\n", total, units);
}

void BitTrace(const struct VP8BitReader* const br, const char label[]) {
  int i, pos;
  if (!init_done) {
    memset(kLabels, 0, sizeof(kLabels));
    atexit(PrintBitTraces);
    buf_start = br->buf_;
    init_done = 1;
  }
  pos = (int)(br->buf_ - buf_start) * 8 - br->bits_;
  // if there's a too large jump, we've changed partition -> reset counter
  if (abs(pos - last_pos) > 32) {
    buf_start = br->buf_;
    pos = 0;
    last_pos = 0;
  }
  if (br->range_ >= 0x7f) pos += kVP8Log2Range[br->range_ - 0x7f];
  for (i = 0; i < last_label; ++i) {
    if (!strcmp(label, kLabels[i].label)) break;
  }
  if (i == MAX_NUM_LABELS) abort();   // overflow!
  kLabels[i].label = label;
  kLabels[i].size += pos - last_pos;
  kLabels[i].count += 1;
  if (i == last_label) ++last_label;
  last_pos = pos;
}

#endif  // BITTRACE > 0

//------------------------------------------------------------------------------
