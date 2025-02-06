// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Boolean decoder
//
// Author: Skal (pascal.massimino@gmail.com)
//         Vikas Arora (vikaas.arora@gmail.com)

#ifndef WEBP_UTILS_BIT_READER_UTILS_H_
#define WEBP_UTILS_BIT_READER_UTILS_H_

#include <assert.h>
#ifdef _MSC_VER
#include <stdlib.h>  // _byteswap_ulong
#endif
#include "src/dsp/cpu.h"
#include "src/webp/types.h"

// Warning! This macro triggers quite some MACRO wizardry around func signature!
#if !defined(BITTRACE)
#define BITTRACE 0    // 0 = off, 1 = print bits, 2 = print bytes
#endif

#if (BITTRACE > 0)
struct VP8BitReader;
extern void BitTrace(const struct VP8BitReader* const br, const char label[]);
#define BT_TRACK(br) BitTrace(br, label)
#define VP8Get(BR, L) VP8GetValue(BR, 1, L)
#else
#define BT_TRACK(br)
// We'll REMOVE the 'const char label[]' from all signatures and calls (!!):
#define VP8GetValue(BR, N, L) VP8GetValue(BR, N)
#define VP8Get(BR, L) VP8GetValue(BR, 1, L)
#define VP8GetSignedValue(BR, N, L) VP8GetSignedValue(BR, N)
#define VP8GetBit(BR, P, L) VP8GetBit(BR, P)
#define VP8GetBitAlt(BR, P, L) VP8GetBitAlt(BR, P)
#define VP8GetSigned(BR, V, L) VP8GetSigned(BR, V)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// The Boolean decoder needs to maintain infinite precision on the value_ field.
// However, since range_ is only 8bit, we only need an active window of 8 bits
// for value_. Left bits (MSB) gets zeroed and shifted away when value_ falls
// below 128, range_ is updated, and fresh bits read from the bitstream are
// brought in as LSB. To avoid reading the fresh bits one by one (slow), we
// cache BITS of them ahead. The total of (BITS + 8) bits must fit into a
// natural register (with type bit_t). To fetch BITS bits from bitstream we
// use a type lbit_t.
//
// BITS can be any multiple of 8 from 8 to 56 (inclusive).
// Pick values that fit natural register size.

#if defined(__i386__) || defined(_M_IX86)      // x86 32bit
#define BITS 24
#elif defined(__x86_64__) || defined(_M_X64)   // x86 64bit
#define BITS 56
#elif defined(__arm__) || defined(_M_ARM)      // ARM
#define BITS 24
#elif WEBP_AARCH64                             // ARM 64bit
#define BITS 56
#elif defined(__mips__)                        // MIPS
#define BITS 24
#elif defined(__wasm__)                        // WASM
#define BITS 56
#else                                          // reasonable default
#define BITS 24
#endif

//------------------------------------------------------------------------------
// Derived types and constants:
//   bit_t = natural register type for storing 'value_' (which is BITS+8 bits)
//   range_t = register for 'range_' (which is 8bits only)

#if (BITS > 24)
typedef uint64_t bit_t;
#else
typedef uint32_t bit_t;
#endif

typedef uint32_t range_t;

//------------------------------------------------------------------------------
// Bitreader

typedef struct VP8BitReader VP8BitReader;
struct VP8BitReader {
  // boolean decoder  (keep the field ordering as is!)
  bit_t value_;               // current value
  range_t range_;             // current range minus 1. In [127, 254] interval.
  int bits_;                  // number of valid bits left
  // read buffer
  const uint8_t* buf_;        // next byte to be read
  const uint8_t* buf_end_;    // end of read buffer
  const uint8_t* buf_max_;    // max packed-read position on buffer
  int eof_;                   // true if input is exhausted
};

// Initialize the bit reader and the boolean decoder.
void VP8InitBitReader(VP8BitReader* const br,
                      const uint8_t* const start, size_t size);
// Sets the working read buffer.
void VP8BitReaderSetBuffer(VP8BitReader* const br,
                           const uint8_t* const start, size_t size);

// Update internal pointers to displace the byte buffer by the
// relative offset 'offset'.
void VP8RemapBitReader(VP8BitReader* const br, ptrdiff_t offset);

// return the next value made of 'num_bits' bits
uint32_t VP8GetValue(VP8BitReader* const br, int num_bits, const char label[]);

// return the next value with sign-extension.
int32_t VP8GetSignedValue(VP8BitReader* const br, int num_bits,
                          const char label[]);

// bit_reader_inl.h will implement the following methods:
//   static WEBP_INLINE int VP8GetBit(VP8BitReader* const br, int prob, ...)
//   static WEBP_INLINE int VP8GetSigned(VP8BitReader* const br, int v, ...)
// and should be included by the .c files that actually need them.
// This is to avoid recompiling the whole library whenever this file is touched,
// and also allowing platform-specific ad-hoc hacks.

// -----------------------------------------------------------------------------
// Bitreader for lossless format

// maximum number of bits (inclusive) the bit-reader can handle:
#define VP8L_MAX_NUM_BIT_READ 24

#define VP8L_LBITS 64  // Number of bits prefetched (= bit-size of vp8l_val_t).
#define VP8L_WBITS 32  // Minimum number of bytes ready after VP8LFillBitWindow.

typedef uint64_t vp8l_val_t;  // right now, this bit-reader can only use 64bit.

typedef struct {
  vp8l_val_t     val_;        // pre-fetched bits
  const uint8_t* buf_;        // input byte buffer
  size_t         len_;        // buffer length
  size_t         pos_;        // byte position in buf_
  int            bit_pos_;    // current bit-reading position in val_
  int            eos_;        // true if a bit was read past the end of buffer
} VP8LBitReader;

void VP8LInitBitReader(VP8LBitReader* const br,
                       const uint8_t* const start,
                       size_t length);

//  Sets a new data buffer.
void VP8LBitReaderSetBuffer(VP8LBitReader* const br,
                            const uint8_t* const buffer, size_t length);

// Reads the specified number of bits from read buffer.
// Flags an error in case end_of_stream or n_bits is more than the allowed limit
// of VP8L_MAX_NUM_BIT_READ (inclusive).
// Flags eos_ if this read attempt is going to cross the read buffer.
uint32_t VP8LReadBits(VP8LBitReader* const br, int n_bits);

// Return the prefetched bits, so they can be looked up.
static WEBP_INLINE uint32_t VP8LPrefetchBits(VP8LBitReader* const br) {
  return (uint32_t)(br->val_ >> (br->bit_pos_ & (VP8L_LBITS - 1)));
}

// Returns true if there was an attempt at reading bit past the end of
// the buffer. Doesn't set br->eos_ flag.
static WEBP_INLINE int VP8LIsEndOfStream(const VP8LBitReader* const br) {
  assert(br->pos_ <= br->len_);
  return br->eos_ || ((br->pos_ == br->len_) && (br->bit_pos_ > VP8L_LBITS));
}

// For jumping over a number of bits in the bit stream when accessed with
// VP8LPrefetchBits and VP8LFillBitWindow.
// This function does *not* set br->eos_, since it's speed-critical.
// Use with extreme care!
static WEBP_INLINE void VP8LSetBitPos(VP8LBitReader* const br, int val) {
  br->bit_pos_ = val;
}

// Advances the read buffer by 4 bytes to make room for reading next 32 bits.
// Speed critical, but infrequent part of the code can be non-inlined.
extern void VP8LDoFillBitWindow(VP8LBitReader* const br);
static WEBP_INLINE void VP8LFillBitWindow(VP8LBitReader* const br) {
  if (br->bit_pos_ >= VP8L_WBITS) VP8LDoFillBitWindow(br);
}

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_UTILS_BIT_READER_UTILS_H_
