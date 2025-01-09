// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// main entry for the decoder
//
// Authors: Vikas Arora (vikaas.arora@gmail.com)
//          Jyrki Alakuijala (jyrki@google.com)

#include <assert.h>
#include <stdlib.h>

#include "src/dec/alphai_dec.h"
#include "src/dec/vp8li_dec.h"
#include "src/dsp/dsp.h"
#include "src/dsp/lossless.h"
#include "src/dsp/lossless_common.h"
#include "src/utils/huffman_utils.h"
#include "src/utils/utils.h"
#include "src/webp/format_constants.h"

#define NUM_ARGB_CACHE_ROWS          16

static const int kCodeLengthLiterals = 16;
static const int kCodeLengthRepeatCode = 16;
static const uint8_t kCodeLengthExtraBits[3] = { 2, 3, 7 };
static const uint8_t kCodeLengthRepeatOffsets[3] = { 3, 3, 11 };

// -----------------------------------------------------------------------------
//  Five Huffman codes are used at each meta code:
//  1. green + length prefix codes + color cache codes,
//  2. alpha,
//  3. red,
//  4. blue, and,
//  5. distance prefix codes.
typedef enum {
  GREEN = 0,
  RED   = 1,
  BLUE  = 2,
  ALPHA = 3,
  DIST  = 4
} HuffIndex;

static const uint16_t kAlphabetSize[HUFFMAN_CODES_PER_META_CODE] = {
  NUM_LITERAL_CODES + NUM_LENGTH_CODES,
  NUM_LITERAL_CODES, NUM_LITERAL_CODES, NUM_LITERAL_CODES,
  NUM_DISTANCE_CODES
};

static const uint8_t kLiteralMap[HUFFMAN_CODES_PER_META_CODE] = {
  0, 1, 1, 1, 0
};

#define NUM_CODE_LENGTH_CODES       19
static const uint8_t kCodeLengthCodeOrder[NUM_CODE_LENGTH_CODES] = {
  17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

#define CODE_TO_PLANE_CODES        120
static const uint8_t kCodeToPlane[CODE_TO_PLANE_CODES] = {
  0x18, 0x07, 0x17, 0x19, 0x28, 0x06, 0x27, 0x29, 0x16, 0x1a,
  0x26, 0x2a, 0x38, 0x05, 0x37, 0x39, 0x15, 0x1b, 0x36, 0x3a,
  0x25, 0x2b, 0x48, 0x04, 0x47, 0x49, 0x14, 0x1c, 0x35, 0x3b,
  0x46, 0x4a, 0x24, 0x2c, 0x58, 0x45, 0x4b, 0x34, 0x3c, 0x03,
  0x57, 0x59, 0x13, 0x1d, 0x56, 0x5a, 0x23, 0x2d, 0x44, 0x4c,
  0x55, 0x5b, 0x33, 0x3d, 0x68, 0x02, 0x67, 0x69, 0x12, 0x1e,
  0x66, 0x6a, 0x22, 0x2e, 0x54, 0x5c, 0x43, 0x4d, 0x65, 0x6b,
  0x32, 0x3e, 0x78, 0x01, 0x77, 0x79, 0x53, 0x5d, 0x11, 0x1f,
  0x64, 0x6c, 0x42, 0x4e, 0x76, 0x7a, 0x21, 0x2f, 0x75, 0x7b,
  0x31, 0x3f, 0x63, 0x6d, 0x52, 0x5e, 0x00, 0x74, 0x7c, 0x41,
  0x4f, 0x10, 0x20, 0x62, 0x6e, 0x30, 0x73, 0x7d, 0x51, 0x5f,
  0x40, 0x72, 0x7e, 0x61, 0x6f, 0x50, 0x71, 0x7f, 0x60, 0x70
};

// Memory needed for lookup tables of one Huffman tree group. Red, blue, alpha
// and distance alphabets are constant (256 for red, blue and alpha, 40 for
// distance) and lookup table sizes for them in worst case are 630 and 410
// respectively. Size of green alphabet depends on color cache size and is equal
// to 256 (green component values) + 24 (length prefix values)
// + color_cache_size (between 0 and 2048).
// All values computed for 8-bit first level lookup with Mark Adler's tool:
// https://github.com/madler/zlib/blob/v1.2.5/examples/enough.c
#define FIXED_TABLE_SIZE (630 * 3 + 410)
static const uint16_t kTableSize[12] = {
  FIXED_TABLE_SIZE + 654,
  FIXED_TABLE_SIZE + 656,
  FIXED_TABLE_SIZE + 658,
  FIXED_TABLE_SIZE + 662,
  FIXED_TABLE_SIZE + 670,
  FIXED_TABLE_SIZE + 686,
  FIXED_TABLE_SIZE + 718,
  FIXED_TABLE_SIZE + 782,
  FIXED_TABLE_SIZE + 912,
  FIXED_TABLE_SIZE + 1168,
  FIXED_TABLE_SIZE + 1680,
  FIXED_TABLE_SIZE + 2704
};

static int VP8LSetError(VP8LDecoder* const dec, VP8StatusCode error) {
  // The oldest error reported takes precedence over the new one.
  if (dec->status_ == VP8_STATUS_OK || dec->status_ == VP8_STATUS_SUSPENDED) {
    dec->status_ = error;
  }
  return 0;
}

static int DecodeImageStream(int xsize, int ysize,
                             int is_level0,
                             VP8LDecoder* const dec,
                             uint32_t** const decoded_data);

//------------------------------------------------------------------------------

int VP8LCheckSignature(const uint8_t* const data, size_t size) {
  return (size >= VP8L_FRAME_HEADER_SIZE &&
          data[0] == VP8L_MAGIC_BYTE &&
          (data[4] >> 5) == 0);  // version
}

static int ReadImageInfo(VP8LBitReader* const br,
                         int* const width, int* const height,
                         int* const has_alpha) {
  if (VP8LReadBits(br, 8) != VP8L_MAGIC_BYTE) return 0;
  *width = VP8LReadBits(br, VP8L_IMAGE_SIZE_BITS) + 1;
  *height = VP8LReadBits(br, VP8L_IMAGE_SIZE_BITS) + 1;
  *has_alpha = VP8LReadBits(br, 1);
  if (VP8LReadBits(br, VP8L_VERSION_BITS) != 0) return 0;
  return !br->eos_;
}

int VP8LGetInfo(const uint8_t* data, size_t data_size,
                int* const width, int* const height, int* const has_alpha) {
  if (data == NULL || data_size < VP8L_FRAME_HEADER_SIZE) {
    return 0;         // not enough data
  } else if (!VP8LCheckSignature(data, data_size)) {
    return 0;         // bad signature
  } else {
    int w, h, a;
    VP8LBitReader br;
    VP8LInitBitReader(&br, data, data_size);
    if (!ReadImageInfo(&br, &w, &h, &a)) {
      return 0;
    }
    if (width != NULL) *width = w;
    if (height != NULL) *height = h;
    if (has_alpha != NULL) *has_alpha = a;
    return 1;
  }
}

//------------------------------------------------------------------------------

static WEBP_INLINE int GetCopyDistance(int distance_symbol,
                                       VP8LBitReader* const br) {
  int extra_bits, offset;
  if (distance_symbol < 4) {
    return distance_symbol + 1;
  }
  extra_bits = (distance_symbol - 2) >> 1;
  offset = (2 + (distance_symbol & 1)) << extra_bits;
  return offset + VP8LReadBits(br, extra_bits) + 1;
}

static WEBP_INLINE int GetCopyLength(int length_symbol,
                                     VP8LBitReader* const br) {
  // Length and distance prefixes are encoded the same way.
  return GetCopyDistance(length_symbol, br);
}

static WEBP_INLINE int PlaneCodeToDistance(int xsize, int plane_code) {
  if (plane_code > CODE_TO_PLANE_CODES) {
    return plane_code - CODE_TO_PLANE_CODES;
  } else {
    const int dist_code = kCodeToPlane[plane_code - 1];
    const int yoffset = dist_code >> 4;
    const int xoffset = 8 - (dist_code & 0xf);
    const int dist = yoffset * xsize + xoffset;
    return (dist >= 1) ? dist : 1;  // dist<1 can happen if xsize is very small
  }
}

//------------------------------------------------------------------------------
// Decodes the next Huffman code from bit-stream.
// VP8LFillBitWindow(br) needs to be called at minimum every second call
// to ReadSymbol, in order to pre-fetch enough bits.
static WEBP_INLINE int ReadSymbol(const HuffmanCode* table,
                                  VP8LBitReader* const br) {
  int nbits;
  uint32_t val = VP8LPrefetchBits(br);
  table += val & HUFFMAN_TABLE_MASK;
  nbits = table->bits - HUFFMAN_TABLE_BITS;
  if (nbits > 0) {
    VP8LSetBitPos(br, br->bit_pos_ + HUFFMAN_TABLE_BITS);
    val = VP8LPrefetchBits(br);
    table += table->value;
    table += val & ((1 << nbits) - 1);
  }
  VP8LSetBitPos(br, br->bit_pos_ + table->bits);
  return table->value;
}

// Reads packed symbol depending on GREEN channel
#define BITS_SPECIAL_MARKER 0x100  // something large enough (and a bit-mask)
#define PACKED_NON_LITERAL_CODE 0  // must be < NUM_LITERAL_CODES
static WEBP_INLINE int ReadPackedSymbols(const HTreeGroup* group,
                                         VP8LBitReader* const br,
                                         uint32_t* const dst) {
  const uint32_t val = VP8LPrefetchBits(br) & (HUFFMAN_PACKED_TABLE_SIZE - 1);
  const HuffmanCode32 code = group->packed_table[val];
  assert(group->use_packed_table);
  if (code.bits < BITS_SPECIAL_MARKER) {
    VP8LSetBitPos(br, br->bit_pos_ + code.bits);
    *dst = code.value;
    return PACKED_NON_LITERAL_CODE;
  } else {
    VP8LSetBitPos(br, br->bit_pos_ + code.bits - BITS_SPECIAL_MARKER);
    assert(code.value >= NUM_LITERAL_CODES);
    return code.value;
  }
}

static int AccumulateHCode(HuffmanCode hcode, int shift,
                           HuffmanCode32* const huff) {
  huff->bits += hcode.bits;
  huff->value |= (uint32_t)hcode.value << shift;
  assert(huff->bits <= HUFFMAN_TABLE_BITS);
  return hcode.bits;
}

static void BuildPackedTable(HTreeGroup* const htree_group) {
  uint32_t code;
  for (code = 0; code < HUFFMAN_PACKED_TABLE_SIZE; ++code) {
    uint32_t bits = code;
    HuffmanCode32* const huff = &htree_group->packed_table[bits];
    HuffmanCode hcode = htree_group->htrees[GREEN][bits];
    if (hcode.value >= NUM_LITERAL_CODES) {
      huff->bits = hcode.bits + BITS_SPECIAL_MARKER;
      huff->value = hcode.value;
    } else {
      huff->bits = 0;
      huff->value = 0;
      bits >>= AccumulateHCode(hcode, 8, huff);
      bits >>= AccumulateHCode(htree_group->htrees[RED][bits], 16, huff);
      bits >>= AccumulateHCode(htree_group->htrees[BLUE][bits], 0, huff);
      bits >>= AccumulateHCode(htree_group->htrees[ALPHA][bits], 24, huff);
      (void)bits;
    }
  }
}

static int ReadHuffmanCodeLengths(
    VP8LDecoder* const dec, const int* const code_length_code_lengths,
    int num_symbols, int* const code_lengths) {
  int ok = 0;
  VP8LBitReader* const br = &dec->br_;
  int symbol;
  int max_symbol;
  int prev_code_len = DEFAULT_CODE_LENGTH;
  HuffmanTables tables;

  if (!VP8LHuffmanTablesAllocate(1 << LENGTHS_TABLE_BITS, &tables) ||
      !VP8LBuildHuffmanTable(&tables, LENGTHS_TABLE_BITS,
                             code_length_code_lengths, NUM_CODE_LENGTH_CODES)) {
    goto End;
  }

  if (VP8LReadBits(br, 1)) {    // use length
    const int length_nbits = 2 + 2 * VP8LReadBits(br, 3);
    max_symbol = 2 + VP8LReadBits(br, length_nbits);
    if (max_symbol > num_symbols) {
      goto End;
    }
  } else {
    max_symbol = num_symbols;
  }

  symbol = 0;
  while (symbol < num_symbols) {
    const HuffmanCode* p;
    int code_len;
    if (max_symbol-- == 0) break;
    VP8LFillBitWindow(br);
    p = &tables.curr_segment->start[VP8LPrefetchBits(br) & LENGTHS_TABLE_MASK];
    VP8LSetBitPos(br, br->bit_pos_ + p->bits);
    code_len = p->value;
    if (code_len < kCodeLengthLiterals) {
      code_lengths[symbol++] = code_len;
      if (code_len != 0) prev_code_len = code_len;
    } else {
      const int use_prev = (code_len == kCodeLengthRepeatCode);
      const int slot = code_len - kCodeLengthLiterals;
      const int extra_bits = kCodeLengthExtraBits[slot];
      const int repeat_offset = kCodeLengthRepeatOffsets[slot];
      int repeat = VP8LReadBits(br, extra_bits) + repeat_offset;
      if (symbol + repeat > num_symbols) {
        goto End;
      } else {
        const int length = use_prev ? prev_code_len : 0;
        while (repeat-- > 0) code_lengths[symbol++] = length;
      }
    }
  }
  ok = 1;

 End:
  VP8LHuffmanTablesDeallocate(&tables);
  if (!ok) return VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
  return ok;
}

// 'code_lengths' is pre-allocated temporary buffer, used for creating Huffman
// tree.
static int ReadHuffmanCode(int alphabet_size, VP8LDecoder* const dec,
                           int* const code_lengths,
                           HuffmanTables* const table) {
  int ok = 0;
  int size = 0;
  VP8LBitReader* const br = &dec->br_;
  const int simple_code = VP8LReadBits(br, 1);

  memset(code_lengths, 0, alphabet_size * sizeof(*code_lengths));

  if (simple_code) {  // Read symbols, codes & code lengths directly.
    const int num_symbols = VP8LReadBits(br, 1) + 1;
    const int first_symbol_len_code = VP8LReadBits(br, 1);
    // The first code is either 1 bit or 8 bit code.
    int symbol = VP8LReadBits(br, (first_symbol_len_code == 0) ? 1 : 8);
    code_lengths[symbol] = 1;
    // The second code (if present), is always 8 bits long.
    if (num_symbols == 2) {
      symbol = VP8LReadBits(br, 8);
      code_lengths[symbol] = 1;
    }
    ok = 1;
  } else {  // Decode Huffman-coded code lengths.
    int i;
    int code_length_code_lengths[NUM_CODE_LENGTH_CODES] = { 0 };
    const int num_codes = VP8LReadBits(br, 4) + 4;
    assert(num_codes <= NUM_CODE_LENGTH_CODES);

    for (i = 0; i < num_codes; ++i) {
      code_length_code_lengths[kCodeLengthCodeOrder[i]] = VP8LReadBits(br, 3);
    }
    ok = ReadHuffmanCodeLengths(dec, code_length_code_lengths, alphabet_size,
                                code_lengths);
  }

  ok = ok && !br->eos_;
  if (ok) {
    size = VP8LBuildHuffmanTable(table, HUFFMAN_TABLE_BITS,
                                 code_lengths, alphabet_size);
  }
  if (!ok || size == 0) {
    return VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
  }
  return size;
}

static int ReadHuffmanCodes(VP8LDecoder* const dec, int xsize, int ysize,
                            int color_cache_bits, int allow_recursion) {
  int i;
  VP8LBitReader* const br = &dec->br_;
  VP8LMetadata* const hdr = &dec->hdr_;
  uint32_t* huffman_image = NULL;
  HTreeGroup* htree_groups = NULL;
  HuffmanTables* huffman_tables = &hdr->huffman_tables_;
  int num_htree_groups = 1;
  int num_htree_groups_max = 1;
  int* mapping = NULL;
  int ok = 0;

  // Check the table has been 0 initialized (through InitMetadata).
  assert(huffman_tables->root.start == NULL);
  assert(huffman_tables->curr_segment == NULL);

  if (allow_recursion && VP8LReadBits(br, 1)) {
    // use meta Huffman codes.
    const int huffman_precision =
        MIN_HUFFMAN_BITS + VP8LReadBits(br, NUM_HUFFMAN_BITS);
    const int huffman_xsize = VP8LSubSampleSize(xsize, huffman_precision);
    const int huffman_ysize = VP8LSubSampleSize(ysize, huffman_precision);
    const int huffman_pixs = huffman_xsize * huffman_ysize;
    if (!DecodeImageStream(huffman_xsize, huffman_ysize, /*is_level0=*/0, dec,
                           &huffman_image)) {
      goto Error;
    }
    hdr->huffman_subsample_bits_ = huffman_precision;
    for (i = 0; i < huffman_pixs; ++i) {
      // The huffman data is stored in red and green bytes.
      const int group = (huffman_image[i] >> 8) & 0xffff;
      huffman_image[i] = group;
      if (group >= num_htree_groups_max) {
        num_htree_groups_max = group + 1;
      }
    }
    // Check the validity of num_htree_groups_max. If it seems too big, use a
    // smaller value for later. This will prevent big memory allocations to end
    // up with a bad bitstream anyway.
    // The value of 1000 is totally arbitrary. We know that num_htree_groups_max
    // is smaller than (1 << 16) and should be smaller than the number of pixels
    // (though the format allows it to be bigger).
    if (num_htree_groups_max > 1000 || num_htree_groups_max > xsize * ysize) {
      // Create a mapping from the used indices to the minimal set of used
      // values [0, num_htree_groups)
      mapping = (int*)WebPSafeMalloc(num_htree_groups_max, sizeof(*mapping));
      if (mapping == NULL) {
        VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
        goto Error;
      }
      // -1 means a value is unmapped, and therefore unused in the Huffman
      // image.
      memset(mapping, 0xff, num_htree_groups_max * sizeof(*mapping));
      for (num_htree_groups = 0, i = 0; i < huffman_pixs; ++i) {
        // Get the current mapping for the group and remap the Huffman image.
        int* const mapped_group = &mapping[huffman_image[i]];
        if (*mapped_group == -1) *mapped_group = num_htree_groups++;
        huffman_image[i] = *mapped_group;
      }
    } else {
      num_htree_groups = num_htree_groups_max;
    }
  }

  if (br->eos_) goto Error;

  if (!ReadHuffmanCodesHelper(color_cache_bits, num_htree_groups,
                              num_htree_groups_max, mapping, dec,
                              huffman_tables, &htree_groups)) {
    goto Error;
  }
  ok = 1;

  // All OK. Finalize pointers.
  hdr->huffman_image_ = huffman_image;
  hdr->num_htree_groups_ = num_htree_groups;
  hdr->htree_groups_ = htree_groups;

 Error:
  WebPSafeFree(mapping);
  if (!ok) {
    WebPSafeFree(huffman_image);
    VP8LHuffmanTablesDeallocate(huffman_tables);
    VP8LHtreeGroupsFree(htree_groups);
  }
  return ok;
}

int ReadHuffmanCodesHelper(int color_cache_bits, int num_htree_groups,
                           int num_htree_groups_max, const int* const mapping,
                           VP8LDecoder* const dec,
                           HuffmanTables* const huffman_tables,
                           HTreeGroup** const htree_groups) {
  int i, j, ok = 0;
  const int max_alphabet_size =
      kAlphabetSize[0] + ((color_cache_bits > 0) ? 1 << color_cache_bits : 0);
  const int table_size = kTableSize[color_cache_bits];
  int* code_lengths = NULL;

  if ((mapping == NULL && num_htree_groups != num_htree_groups_max) ||
      num_htree_groups > num_htree_groups_max) {
    goto Error;
  }

  code_lengths =
      (int*)WebPSafeCalloc((uint64_t)max_alphabet_size, sizeof(*code_lengths));
  *htree_groups = VP8LHtreeGroupsNew(num_htree_groups);

  if (*htree_groups == NULL || code_lengths == NULL ||
      !VP8LHuffmanTablesAllocate(num_htree_groups * table_size,
                                 huffman_tables)) {
    VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
    goto Error;
  }

  for (i = 0; i < num_htree_groups_max; ++i) {
    // If the index "i" is unused in the Huffman image, just make sure the
    // coefficients are valid but do not store them.
    if (mapping != NULL && mapping[i] == -1) {
      for (j = 0; j < HUFFMAN_CODES_PER_META_CODE; ++j) {
        int alphabet_size = kAlphabetSize[j];
        if (j == 0 && color_cache_bits > 0) {
          alphabet_size += (1 << color_cache_bits);
        }
        // Passing in NULL so that nothing gets filled.
        if (!ReadHuffmanCode(alphabet_size, dec, code_lengths, NULL)) {
          goto Error;
        }
      }
    } else {
      HTreeGroup* const htree_group =
          &(*htree_groups)[(mapping == NULL) ? i : mapping[i]];
      HuffmanCode** const htrees = htree_group->htrees;
      int size;
      int total_size = 0;
      int is_trivial_literal = 1;
      int max_bits = 0;
      for (j = 0; j < HUFFMAN_CODES_PER_META_CODE; ++j) {
        int alphabet_size = kAlphabetSize[j];
        if (j == 0 && color_cache_bits > 0) {
          alphabet_size += (1 << color_cache_bits);
        }
        size =
            ReadHuffmanCode(alphabet_size, dec, code_lengths, huffman_tables);
        htrees[j] = huffman_tables->curr_segment->curr_table;
        if (size == 0) {
          goto Error;
        }
        if (is_trivial_literal && kLiteralMap[j] == 1) {
          is_trivial_literal = (htrees[j]->bits == 0);
        }
        total_size += htrees[j]->bits;
        huffman_tables->curr_segment->curr_table += size;
        if (j <= ALPHA) {
          int local_max_bits = code_lengths[0];
          int k;
          for (k = 1; k < alphabet_size; ++k) {
            if (code_lengths[k] > local_max_bits) {
              local_max_bits = code_lengths[k];
            }
          }
          max_bits += local_max_bits;
        }
      }
      htree_group->is_trivial_literal = is_trivial_literal;
      htree_group->is_trivial_code = 0;
      if (is_trivial_literal) {
        const int red = htrees[RED][0].value;
        const int blue = htrees[BLUE][0].value;
        const int alpha = htrees[ALPHA][0].value;
        htree_group->literal_arb = ((uint32_t)alpha << 24) | (red << 16) | blue;
        if (total_size == 0 && htrees[GREEN][0].value < NUM_LITERAL_CODES) {
          htree_group->is_trivial_code = 1;
          htree_group->literal_arb |= htrees[GREEN][0].value << 8;
        }
      }
      htree_group->use_packed_table =
          !htree_group->is_trivial_code && (max_bits < HUFFMAN_PACKED_BITS);
      if (htree_group->use_packed_table) BuildPackedTable(htree_group);
    }
  }
  ok = 1;

 Error:
  WebPSafeFree(code_lengths);
  if (!ok) {
    VP8LHuffmanTablesDeallocate(huffman_tables);
    VP8LHtreeGroupsFree(*htree_groups);
    *htree_groups = NULL;
  }
  return ok;
}

//------------------------------------------------------------------------------
// Scaling.

#if !defined(WEBP_REDUCE_SIZE)
static int AllocateAndInitRescaler(VP8LDecoder* const dec, VP8Io* const io) {
  const int num_channels = 4;
  const int in_width = io->mb_w;
  const int out_width = io->scaled_width;
  const int in_height = io->mb_h;
  const int out_height = io->scaled_height;
  const uint64_t work_size = 2 * num_channels * (uint64_t)out_width;
  rescaler_t* work;        // Rescaler work area.
  const uint64_t scaled_data_size = (uint64_t)out_width;
  uint32_t* scaled_data;  // Temporary storage for scaled BGRA data.
  const uint64_t memory_size = sizeof(*dec->rescaler) +
                               work_size * sizeof(*work) +
                               scaled_data_size * sizeof(*scaled_data);
  uint8_t* memory = (uint8_t*)WebPSafeMalloc(memory_size, sizeof(*memory));
  if (memory == NULL) {
    return VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
  }
  assert(dec->rescaler_memory == NULL);
  dec->rescaler_memory = memory;

  dec->rescaler = (WebPRescaler*)memory;
  memory += sizeof(*dec->rescaler);
  work = (rescaler_t*)memory;
  memory += work_size * sizeof(*work);
  scaled_data = (uint32_t*)memory;

  if (!WebPRescalerInit(dec->rescaler, in_width, in_height,
                        (uint8_t*)scaled_data, out_width, out_height,
                        0, num_channels, work)) {
    return 0;
  }
  return 1;
}
#endif   // WEBP_REDUCE_SIZE

//------------------------------------------------------------------------------
// Export to ARGB

#if !defined(WEBP_REDUCE_SIZE)

// We have special "export" function since we need to convert from BGRA
static int Export(WebPRescaler* const rescaler, WEBP_CSP_MODE colorspace,
                  int rgba_stride, uint8_t* const rgba) {
  uint32_t* const src = (uint32_t*)rescaler->dst;
  uint8_t* dst = rgba;
  const int dst_width = rescaler->dst_width;
  int num_lines_out = 0;
  while (WebPRescalerHasPendingOutput(rescaler)) {
    WebPRescalerExportRow(rescaler);
    WebPMultARGBRow(src, dst_width, 1);
    VP8LConvertFromBGRA(src, dst_width, colorspace, dst);
    dst += rgba_stride;
    ++num_lines_out;
  }
  return num_lines_out;
}

// Emit scaled rows.
static int EmitRescaledRowsRGBA(const VP8LDecoder* const dec,
                                uint8_t* in, int in_stride, int mb_h,
                                uint8_t* const out, int out_stride) {
  const WEBP_CSP_MODE colorspace = dec->output_->colorspace;
  int num_lines_in = 0;
  int num_lines_out = 0;
  while (num_lines_in < mb_h) {
    uint8_t* const row_in = in + (uint64_t)num_lines_in * in_stride;
    uint8_t* const row_out = out + (uint64_t)num_lines_out * out_stride;
    const int lines_left = mb_h - num_lines_in;
    const int needed_lines = WebPRescaleNeededLines(dec->rescaler, lines_left);
    int lines_imported;
    assert(needed_lines > 0 && needed_lines <= lines_left);
    WebPMultARGBRows(row_in, in_stride,
                     dec->rescaler->src_width, needed_lines, 0);
    lines_imported =
        WebPRescalerImport(dec->rescaler, lines_left, row_in, in_stride);
    assert(lines_imported == needed_lines);
    num_lines_in += lines_imported;
    num_lines_out += Export(dec->rescaler, colorspace, out_stride, row_out);
  }
  return num_lines_out;
}

#endif   // WEBP_REDUCE_SIZE

// Emit rows without any scaling.
static int EmitRows(WEBP_CSP_MODE colorspace,
                    const uint8_t* row_in, int in_stride,
                    int mb_w, int mb_h,
                    uint8_t* const out, int out_stride) {
  int lines = mb_h;
  uint8_t* row_out = out;
  while (lines-- > 0) {
    VP8LConvertFromBGRA((const uint32_t*)row_in, mb_w, colorspace, row_out);
    row_in += in_stride;
    row_out += out_stride;
  }
  return mb_h;  // Num rows out == num rows in.
}

//------------------------------------------------------------------------------
// Export to YUVA

static void ConvertToYUVA(const uint32_t* const src, int width, int y_pos,
                          const WebPDecBuffer* const output) {
  const WebPYUVABuffer* const buf = &output->u.YUVA;

  // first, the luma plane
  WebPConvertARGBToY(src, buf->y + y_pos * buf->y_stride, width);

  // then U/V planes
  {
    uint8_t* const u = buf->u + (y_pos >> 1) * buf->u_stride;
    uint8_t* const v = buf->v + (y_pos >> 1) * buf->v_stride;
    // even lines: store values
    // odd lines: average with previous values
    WebPConvertARGBToUV(src, u, v, width, !(y_pos & 1));
  }
  // Lastly, store alpha if needed.
  if (buf->a != NULL) {
    uint8_t* const a = buf->a + y_pos * buf->a_stride;
#if defined(WORDS_BIGENDIAN)
    WebPExtractAlpha((uint8_t*)src + 0, 0, width, 1, a, 0);
#else
    WebPExtractAlpha((uint8_t*)src + 3, 0, width, 1, a, 0);
#endif
  }
}

static int ExportYUVA(const VP8LDecoder* const dec, int y_pos) {
  WebPRescaler* const rescaler = dec->rescaler;
  uint32_t* const src = (uint32_t*)rescaler->dst;
  const int dst_width = rescaler->dst_width;
  int num_lines_out = 0;
  while (WebPRescalerHasPendingOutput(rescaler)) {
    WebPRescalerExportRow(rescaler);
    WebPMultARGBRow(src, dst_width, 1);
    ConvertToYUVA(src, dst_width, y_pos, dec->output_);
    ++y_pos;
    ++num_lines_out;
  }
  return num_lines_out;
}

static int EmitRescaledRowsYUVA(const VP8LDecoder* const dec,
                                uint8_t* in, int in_stride, int mb_h) {
  int num_lines_in = 0;
  int y_pos = dec->last_out_row_;
  while (num_lines_in < mb_h) {
    const int lines_left = mb_h - num_lines_in;
    const int needed_lines = WebPRescaleNeededLines(dec->rescaler, lines_left);
    int lines_imported;
    WebPMultARGBRows(in, in_stride, dec->rescaler->src_width, needed_lines, 0);
    lines_imported =
        WebPRescalerImport(dec->rescaler, lines_left, in, in_stride);
    assert(lines_imported == needed_lines);
    num_lines_in += lines_imported;
    in += needed_lines * in_stride;
    y_pos += ExportYUVA(dec, y_pos);
  }
  return y_pos;
}

static int EmitRowsYUVA(const VP8LDecoder* const dec,
                        const uint8_t* in, int in_stride,
                        int mb_w, int num_rows) {
  int y_pos = dec->last_out_row_;
  while (num_rows-- > 0) {
    ConvertToYUVA((const uint32_t*)in, mb_w, y_pos, dec->output_);
    in += in_stride;
    ++y_pos;
  }
  return y_pos;
}

//------------------------------------------------------------------------------
// Cropping.

// Sets io->mb_y, io->mb_h & io->mb_w according to start row, end row and
// crop options. Also updates the input data pointer, so that it points to the
// start of the cropped window. Note that pixels are in ARGB format even if
// 'in_data' is uint8_t*.
// Returns true if the crop window is not empty.
static int SetCropWindow(VP8Io* const io, int y_start, int y_end,
                         uint8_t** const in_data, int pixel_stride) {
  assert(y_start < y_end);
  assert(io->crop_left < io->crop_right);
  if (y_end > io->crop_bottom) {
    y_end = io->crop_bottom;  // make sure we don't overflow on last row.
  }
  if (y_start < io->crop_top) {
    const int delta = io->crop_top - y_start;
    y_start = io->crop_top;
    *in_data += delta * pixel_stride;
  }
  if (y_start >= y_end) return 0;  // Crop window is empty.

  *in_data += io->crop_left * sizeof(uint32_t);

  io->mb_y = y_start - io->crop_top;
  io->mb_w = io->crop_right - io->crop_left;
  io->mb_h = y_end - y_start;
  return 1;  // Non-empty crop window.
}

//------------------------------------------------------------------------------

static WEBP_INLINE int GetMetaIndex(
    const uint32_t* const image, int xsize, int bits, int x, int y) {
  if (bits == 0) return 0;
  return image[xsize * (y >> bits) + (x >> bits)];
}

static WEBP_INLINE HTreeGroup* GetHtreeGroupForPos(VP8LMetadata* const hdr,
                                                   int x, int y) {
  const int meta_index = GetMetaIndex(hdr->huffman_image_, hdr->huffman_xsize_,
                                      hdr->huffman_subsample_bits_, x, y);
  assert(meta_index < hdr->num_htree_groups_);
  return hdr->htree_groups_ + meta_index;
}

//------------------------------------------------------------------------------
// Main loop, with custom row-processing function

typedef void (*ProcessRowsFunc)(VP8LDecoder* const dec, int row);

static void ApplyInverseTransforms(VP8LDecoder* const dec,
                                   int start_row, int num_rows,
                                   const uint32_t* const rows) {
  int n = dec->next_transform_;
  const int cache_pixs = dec->width_ * num_rows;
  const int end_row = start_row + num_rows;
  const uint32_t* rows_in = rows;
  uint32_t* const rows_out = dec->argb_cache_;

  // Inverse transforms.
  while (n-- > 0) {
    VP8LTransform* const transform = &dec->transforms_[n];
    VP8LInverseTransform(transform, start_row, end_row, rows_in, rows_out);
    rows_in = rows_out;
  }
  if (rows_in != rows_out) {
    // No transform called, hence just copy.
    memcpy(rows_out, rows_in, cache_pixs * sizeof(*rows_out));
  }
}

// Processes (transforms, scales & color-converts) the rows decoded after the
// last call.
static void ProcessRows(VP8LDecoder* const dec, int row) {
  const uint32_t* const rows = dec->pixels_ + dec->width_ * dec->last_row_;
  const int num_rows = row - dec->last_row_;

  assert(row <= dec->io_->crop_bottom);
  // We can't process more than NUM_ARGB_CACHE_ROWS at a time (that's the size
  // of argb_cache_), but we currently don't need more than that.
  assert(num_rows <= NUM_ARGB_CACHE_ROWS);
  if (num_rows > 0) {    // Emit output.
    VP8Io* const io = dec->io_;
    uint8_t* rows_data = (uint8_t*)dec->argb_cache_;
    const int in_stride = io->width * sizeof(uint32_t);  // in unit of RGBA
    ApplyInverseTransforms(dec, dec->last_row_, num_rows, rows);
    if (!SetCropWindow(io, dec->last_row_, row, &rows_data, in_stride)) {
      // Nothing to output (this time).
    } else {
      const WebPDecBuffer* const output = dec->output_;
      if (WebPIsRGBMode(output->colorspace)) {  // convert to RGBA
        const WebPRGBABuffer* const buf = &output->u.RGBA;
        uint8_t* const rgba =
            buf->rgba + (int64_t)dec->last_out_row_ * buf->stride;
        const int num_rows_out =
#if !defined(WEBP_REDUCE_SIZE)
         io->use_scaling ?
            EmitRescaledRowsRGBA(dec, rows_data, in_stride, io->mb_h,
                                 rgba, buf->stride) :
#endif  // WEBP_REDUCE_SIZE
            EmitRows(output->colorspace, rows_data, in_stride,
                     io->mb_w, io->mb_h, rgba, buf->stride);
        // Update 'last_out_row_'.
        dec->last_out_row_ += num_rows_out;
      } else {                              // convert to YUVA
        dec->last_out_row_ = io->use_scaling ?
            EmitRescaledRowsYUVA(dec, rows_data, in_stride, io->mb_h) :
            EmitRowsYUVA(dec, rows_data, in_stride, io->mb_w, io->mb_h);
      }
      assert(dec->last_out_row_ <= output->height);
    }
  }

  // Update 'last_row_'.
  dec->last_row_ = row;
  assert(dec->last_row_ <= dec->height_);
}

// Row-processing for the special case when alpha data contains only one
// transform (color indexing), and trivial non-green literals.
static int Is8bOptimizable(const VP8LMetadata* const hdr) {
  int i;
  if (hdr->color_cache_size_ > 0) return 0;
  // When the Huffman tree contains only one symbol, we can skip the
  // call to ReadSymbol() for red/blue/alpha channels.
  for (i = 0; i < hdr->num_htree_groups_; ++i) {
    HuffmanCode** const htrees = hdr->htree_groups_[i].htrees;
    if (htrees[RED][0].bits > 0) return 0;
    if (htrees[BLUE][0].bits > 0) return 0;
    if (htrees[ALPHA][0].bits > 0) return 0;
  }
  return 1;
}

static void AlphaApplyFilter(ALPHDecoder* const alph_dec,
                             int first_row, int last_row,
                             uint8_t* out, int stride) {
  if (alph_dec->filter_ != WEBP_FILTER_NONE) {
    int y;
    const uint8_t* prev_line = alph_dec->prev_line_;
    assert(WebPUnfilters[alph_dec->filter_] != NULL);
    for (y = first_row; y < last_row; ++y) {
      WebPUnfilters[alph_dec->filter_](prev_line, out, out, stride);
      prev_line = out;
      out += stride;
    }
    alph_dec->prev_line_ = prev_line;
  }
}

static void ExtractPalettedAlphaRows(VP8LDecoder* const dec, int last_row) {
  // For vertical and gradient filtering, we need to decode the part above the
  // crop_top row, in order to have the correct spatial predictors.
  ALPHDecoder* const alph_dec = (ALPHDecoder*)dec->io_->opaque;
  const int top_row =
      (alph_dec->filter_ == WEBP_FILTER_NONE ||
       alph_dec->filter_ == WEBP_FILTER_HORIZONTAL) ? dec->io_->crop_top
                                                    : dec->last_row_;
  const int first_row = (dec->last_row_ < top_row) ? top_row : dec->last_row_;
  assert(last_row <= dec->io_->crop_bottom);
  if (last_row > first_row) {
    // Special method for paletted alpha data. We only process the cropped area.
    const int width = dec->io_->width;
    uint8_t* out = alph_dec->output_ + width * first_row;
    const uint8_t* const in =
      (uint8_t*)dec->pixels_ + dec->width_ * first_row;
    VP8LTransform* const transform = &dec->transforms_[0];
    assert(dec->next_transform_ == 1);
    assert(transform->type_ == COLOR_INDEXING_TRANSFORM);
    VP8LColorIndexInverseTransformAlpha(transform, first_row, last_row,
                                        in, out);
    AlphaApplyFilter(alph_dec, first_row, last_row, out, width);
  }
  dec->last_row_ = dec->last_out_row_ = last_row;
}

//------------------------------------------------------------------------------
// Helper functions for fast pattern copy (8b and 32b)

// cyclic rotation of pattern word
static WEBP_INLINE uint32_t Rotate8b(uint32_t V) {
#if defined(WORDS_BIGENDIAN)
  return ((V & 0xff000000u) >> 24) | (V << 8);
#else
  return ((V & 0xffu) << 24) | (V >> 8);
#endif
}

// copy 1, 2 or 4-bytes pattern
static WEBP_INLINE void CopySmallPattern8b(const uint8_t* src, uint8_t* dst,
                                           int length, uint32_t pattern) {
  int i;
  // align 'dst' to 4-bytes boundary. Adjust the pattern along the way.
  while ((uintptr_t)dst & 3) {
    *dst++ = *src++;
    pattern = Rotate8b(pattern);
    --length;
  }
  // Copy the pattern 4 bytes at a time.
  for (i = 0; i < (length >> 2); ++i) {
    ((uint32_t*)dst)[i] = pattern;
  }
  // Finish with left-overs. 'pattern' is still correctly positioned,
  // so no Rotate8b() call is needed.
  for (i <<= 2; i < length; ++i) {
    dst[i] = src[i];
  }
}

static WEBP_INLINE void CopyBlock8b(uint8_t* const dst, int dist, int length) {
  const uint8_t* src = dst - dist;
  if (length >= 8) {
    uint32_t pattern = 0;
    switch (dist) {
      case 1:
        pattern = src[0];
#if defined(__arm__) || defined(_M_ARM)   // arm doesn't like multiply that much
        pattern |= pattern << 8;
        pattern |= pattern << 16;
#elif defined(WEBP_USE_MIPS_DSP_R2)
        __asm__ volatile ("replv.qb %0, %0" : "+r"(pattern));
#else
        pattern = 0x01010101u * pattern;
#endif
        break;
      case 2:
#if !defined(WORDS_BIGENDIAN)
        memcpy(&pattern, src, sizeof(uint16_t));
#else
        pattern = ((uint32_t)src[0] << 8) | src[1];
#endif
#if defined(__arm__) || defined(_M_ARM)
        pattern |= pattern << 16;
#elif defined(WEBP_USE_MIPS_DSP_R2)
        __asm__ volatile ("replv.ph %0, %0" : "+r"(pattern));
#else
        pattern = 0x00010001u * pattern;
#endif
        break;
      case 4:
        memcpy(&pattern, src, sizeof(uint32_t));
        break;
      default:
        goto Copy;
    }
    CopySmallPattern8b(src, dst, length, pattern);
    return;
  }
 Copy:
  if (dist >= length) {  // no overlap -> use memcpy()
    memcpy(dst, src, length * sizeof(*dst));
  } else {
    int i;
    for (i = 0; i < length; ++i) dst[i] = src[i];
  }
}

// copy pattern of 1 or 2 uint32_t's
static WEBP_INLINE void CopySmallPattern32b(const uint32_t* src,
                                            uint32_t* dst,
                                            int length, uint64_t pattern) {
  int i;
  if ((uintptr_t)dst & 4) {           // Align 'dst' to 8-bytes boundary.
    *dst++ = *src++;
    pattern = (pattern >> 32) | (pattern << 32);
    --length;
  }
  assert(0 == ((uintptr_t)dst & 7));
  for (i = 0; i < (length >> 1); ++i) {
    ((uint64_t*)dst)[i] = pattern;    // Copy the pattern 8 bytes at a time.
  }
  if (length & 1) {                   // Finish with left-over.
    dst[i << 1] = src[i << 1];
  }
}

static WEBP_INLINE void CopyBlock32b(uint32_t* const dst,
                                     int dist, int length) {
  const uint32_t* const src = dst - dist;
  if (dist <= 2 && length >= 4 && ((uintptr_t)dst & 3) == 0) {
    uint64_t pattern;
    if (dist == 1) {
      pattern = (uint64_t)src[0];
      pattern |= pattern << 32;
    } else {
      memcpy(&pattern, src, sizeof(pattern));
    }
    CopySmallPattern32b(src, dst, length, pattern);
  } else if (dist >= length) {  // no overlap
    memcpy(dst, src, length * sizeof(*dst));
  } else {
    int i;
    for (i = 0; i < length; ++i) dst[i] = src[i];
  }
}

//------------------------------------------------------------------------------

static int DecodeAlphaData(VP8LDecoder* const dec, uint8_t* const data,
                           int width, int height, int last_row) {
  int ok = 1;
  int row = dec->last_pixel_ / width;
  int col = dec->last_pixel_ % width;
  VP8LBitReader* const br = &dec->br_;
  VP8LMetadata* const hdr = &dec->hdr_;
  int pos = dec->last_pixel_;         // current position
  const int end = width * height;     // End of data
  const int last = width * last_row;  // Last pixel to decode
  const int len_code_limit = NUM_LITERAL_CODES + NUM_LENGTH_CODES;
  const int mask = hdr->huffman_mask_;
  const HTreeGroup* htree_group =
      (pos < last) ? GetHtreeGroupForPos(hdr, col, row) : NULL;
  assert(pos <= end);
  assert(last_row <= height);
  assert(Is8bOptimizable(hdr));

  while (!br->eos_ && pos < last) {
    int code;
    // Only update when changing tile.
    if ((col & mask) == 0) {
      htree_group = GetHtreeGroupForPos(hdr, col, row);
    }
    assert(htree_group != NULL);
    VP8LFillBitWindow(br);
    code = ReadSymbol(htree_group->htrees[GREEN], br);
    if (code < NUM_LITERAL_CODES) {  // Literal
      data[pos] = code;
      ++pos;
      ++col;
      if (col >= width) {
        col = 0;
        ++row;
        if (row <= last_row && (row % NUM_ARGB_CACHE_ROWS == 0)) {
          ExtractPalettedAlphaRows(dec, row);
        }
      }
    } else if (code < len_code_limit) {  // Backward reference
      int dist_code, dist;
      const int length_sym = code - NUM_LITERAL_CODES;
      const int length = GetCopyLength(length_sym, br);
      const int dist_symbol = ReadSymbol(htree_group->htrees[DIST], br);
      VP8LFillBitWindow(br);
      dist_code = GetCopyDistance(dist_symbol, br);
      dist = PlaneCodeToDistance(width, dist_code);
      if (pos >= dist && end - pos >= length) {
        CopyBlock8b(data + pos, dist, length);
      } else {
        ok = 0;
        goto End;
      }
      pos += length;
      col += length;
      while (col >= width) {
        col -= width;
        ++row;
        if (row <= last_row && (row % NUM_ARGB_CACHE_ROWS == 0)) {
          ExtractPalettedAlphaRows(dec, row);
        }
      }
      if (pos < last && (col & mask)) {
        htree_group = GetHtreeGroupForPos(hdr, col, row);
      }
    } else {  // Not reached
      ok = 0;
      goto End;
    }
    br->eos_ = VP8LIsEndOfStream(br);
  }
  // Process the remaining rows corresponding to last row-block.
  ExtractPalettedAlphaRows(dec, row > last_row ? last_row : row);

 End:
  br->eos_ = VP8LIsEndOfStream(br);
  if (!ok || (br->eos_ && pos < end)) {
    return VP8LSetError(
        dec, br->eos_ ? VP8_STATUS_SUSPENDED : VP8_STATUS_BITSTREAM_ERROR);
  }
  dec->last_pixel_ = pos;
  return ok;
}

static void SaveState(VP8LDecoder* const dec, int last_pixel) {
  assert(dec->incremental_);
  dec->saved_br_ = dec->br_;
  dec->saved_last_pixel_ = last_pixel;
  if (dec->hdr_.color_cache_size_ > 0) {
    VP8LColorCacheCopy(&dec->hdr_.color_cache_, &dec->hdr_.saved_color_cache_);
  }
}

static void RestoreState(VP8LDecoder* const dec) {
  assert(dec->br_.eos_);
  dec->status_ = VP8_STATUS_SUSPENDED;
  dec->br_ = dec->saved_br_;
  dec->last_pixel_ = dec->saved_last_pixel_;
  if (dec->hdr_.color_cache_size_ > 0) {
    VP8LColorCacheCopy(&dec->hdr_.saved_color_cache_, &dec->hdr_.color_cache_);
  }
}

#define SYNC_EVERY_N_ROWS 8  // minimum number of rows between check-points
static int DecodeImageData(VP8LDecoder* const dec, uint32_t* const data,
                           int width, int height, int last_row,
                           ProcessRowsFunc process_func) {
  int row = dec->last_pixel_ / width;
  int col = dec->last_pixel_ % width;
  VP8LBitReader* const br = &dec->br_;
  VP8LMetadata* const hdr = &dec->hdr_;
  uint32_t* src = data + dec->last_pixel_;
  uint32_t* last_cached = src;
  uint32_t* const src_end = data + width * height;     // End of data
  uint32_t* const src_last = data + width * last_row;  // Last pixel to decode
  const int len_code_limit = NUM_LITERAL_CODES + NUM_LENGTH_CODES;
  const int color_cache_limit = len_code_limit + hdr->color_cache_size_;
  int next_sync_row = dec->incremental_ ? row : 1 << 24;
  VP8LColorCache* const color_cache =
      (hdr->color_cache_size_ > 0) ? &hdr->color_cache_ : NULL;
  const int mask = hdr->huffman_mask_;
  const HTreeGroup* htree_group =
      (src < src_last) ? GetHtreeGroupForPos(hdr, col, row) : NULL;
  assert(dec->last_row_ < last_row);
  assert(src_last <= src_end);

  while (src < src_last) {
    int code;
    if (row >= next_sync_row) {
      SaveState(dec, (int)(src - data));
      next_sync_row = row + SYNC_EVERY_N_ROWS;
    }
    // Only update when changing tile. Note we could use this test:
    // if "((((prev_col ^ col) | prev_row ^ row)) > mask)" -> tile changed
    // but that's actually slower and needs storing the previous col/row.
    if ((col & mask) == 0) {
      htree_group = GetHtreeGroupForPos(hdr, col, row);
    }
    assert(htree_group != NULL);
    if (htree_group->is_trivial_code) {
      *src = htree_group->literal_arb;
      goto AdvanceByOne;
    }
    VP8LFillBitWindow(br);
    if (htree_group->use_packed_table) {
      code = ReadPackedSymbols(htree_group, br, src);
      if (VP8LIsEndOfStream(br)) break;
      if (code == PACKED_NON_LITERAL_CODE) goto AdvanceByOne;
    } else {
      code = ReadSymbol(htree_group->htrees[GREEN], br);
    }
    if (VP8LIsEndOfStream(br)) break;
    if (code < NUM_LITERAL_CODES) {  // Literal
      if (htree_group->is_trivial_literal) {
        *src = htree_group->literal_arb | (code << 8);
      } else {
        int red, blue, alpha;
        red = ReadSymbol(htree_group->htrees[RED], br);
        VP8LFillBitWindow(br);
        blue = ReadSymbol(htree_group->htrees[BLUE], br);
        alpha = ReadSymbol(htree_group->htrees[ALPHA], br);
        if (VP8LIsEndOfStream(br)) break;
        *src = ((uint32_t)alpha << 24) | (red << 16) | (code << 8) | blue;
      }
    AdvanceByOne:
      ++src;
      ++col;
      if (col >= width) {
        col = 0;
        ++row;
        if (process_func != NULL) {
          if (row <= last_row && (row % NUM_ARGB_CACHE_ROWS == 0)) {
            process_func(dec, row);
          }
        }
        if (color_cache != NULL) {
          while (last_cached < src) {
            VP8LColorCacheInsert(color_cache, *last_cached++);
          }
        }
      }
    } else if (code < len_code_limit) {  // Backward reference
      int dist_code, dist;
      const int length_sym = code - NUM_LITERAL_CODES;
      const int length = GetCopyLength(length_sym, br);
      const int dist_symbol = ReadSymbol(htree_group->htrees[DIST], br);
      VP8LFillBitWindow(br);
      dist_code = GetCopyDistance(dist_symbol, br);
      dist = PlaneCodeToDistance(width, dist_code);

      if (VP8LIsEndOfStream(br)) break;
      if (src - data < (ptrdiff_t)dist || src_end - src < (ptrdiff_t)length) {
        goto Error;
      } else {
        CopyBlock32b(src, dist, length);
      }
      src += length;
      col += length;
      while (col >= width) {
        col -= width;
        ++row;
        if (process_func != NULL) {
          if (row <= last_row && (row % NUM_ARGB_CACHE_ROWS == 0)) {
            process_func(dec, row);
          }
        }
      }
      // Because of the check done above (before 'src' was incremented by
      // 'length'), the following holds true.
      assert(src <= src_end);
      if (col & mask) htree_group = GetHtreeGroupForPos(hdr, col, row);
      if (color_cache != NULL) {
        while (last_cached < src) {
          VP8LColorCacheInsert(color_cache, *last_cached++);
        }
      }
    } else if (code < color_cache_limit) {  // Color cache
      const int key = code - len_code_limit;
      assert(color_cache != NULL);
      while (last_cached < src) {
        VP8LColorCacheInsert(color_cache, *last_cached++);
      }
      *src = VP8LColorCacheLookup(color_cache, key);
      goto AdvanceByOne;
    } else {  // Not reached
      goto Error;
    }
  }

  br->eos_ = VP8LIsEndOfStream(br);
  // In incremental decoding:
  // br->eos_ && src < src_last: if 'br' reached the end of the buffer and
  // 'src_last' has not been reached yet, there is not enough data. 'dec' has to
  // be reset until there is more data.
  // !br->eos_ && src < src_last: this cannot happen as either the buffer is
  // fully read, either enough has been read to reach 'src_last'.
  // src >= src_last: 'src_last' is reached, all is fine. 'src' can actually go
  // beyond 'src_last' in case the image is cropped and an LZ77 goes further.
  // The buffer might have been enough or there is some left. 'br->eos_' does
  // not matter.
  assert(!dec->incremental_ || (br->eos_ && src < src_last) || src >= src_last);
  if (dec->incremental_ && br->eos_ && src < src_last) {
    RestoreState(dec);
  } else if ((dec->incremental_ && src >= src_last) || !br->eos_) {
    // Process the remaining rows corresponding to last row-block.
    if (process_func != NULL) {
      process_func(dec, row > last_row ? last_row : row);
    }
    dec->status_ = VP8_STATUS_OK;
    dec->last_pixel_ = (int)(src - data);  // end-of-scan marker
  } else {
    // if not incremental, and we are past the end of buffer (eos_=1), then this
    // is a real bitstream error.
    goto Error;
  }
  return 1;

 Error:
  return VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
}

// -----------------------------------------------------------------------------
// VP8LTransform

static void ClearTransform(VP8LTransform* const transform) {
  WebPSafeFree(transform->data_);
  transform->data_ = NULL;
}

// For security reason, we need to remap the color map to span
// the total possible bundled values, and not just the num_colors.
static int ExpandColorMap(int num_colors, VP8LTransform* const transform) {
  int i;
  const int final_num_colors = 1 << (8 >> transform->bits_);
  uint32_t* const new_color_map =
      (uint32_t*)WebPSafeMalloc((uint64_t)final_num_colors,
                                sizeof(*new_color_map));
  if (new_color_map == NULL) {
    return 0;
  } else {
    uint8_t* const data = (uint8_t*)transform->data_;
    uint8_t* const new_data = (uint8_t*)new_color_map;
    new_color_map[0] = transform->data_[0];
    for (i = 4; i < 4 * num_colors; ++i) {
      // Equivalent to VP8LAddPixels(), on a byte-basis.
      new_data[i] = (data[i] + new_data[i - 4]) & 0xff;
    }
    for (; i < 4 * final_num_colors; ++i) {
      new_data[i] = 0;  // black tail.
    }
    WebPSafeFree(transform->data_);
    transform->data_ = new_color_map;
  }
  return 1;
}

static int ReadTransform(int* const xsize, int const* ysize,
                         VP8LDecoder* const dec) {
  int ok = 1;
  VP8LBitReader* const br = &dec->br_;
  VP8LTransform* transform = &dec->transforms_[dec->next_transform_];
  const VP8LImageTransformType type =
      (VP8LImageTransformType)VP8LReadBits(br, 2);

  // Each transform type can only be present once in the stream.
  if (dec->transforms_seen_ & (1U << type)) {
    return 0;  // Already there, let's not accept the second same transform.
  }
  dec->transforms_seen_ |= (1U << type);

  transform->type_ = type;
  transform->xsize_ = *xsize;
  transform->ysize_ = *ysize;
  transform->data_ = NULL;
  ++dec->next_transform_;
  assert(dec->next_transform_ <= NUM_TRANSFORMS);

  switch (type) {
    case PREDICTOR_TRANSFORM:
    case CROSS_COLOR_TRANSFORM:
      transform->bits_ =
          MIN_TRANSFORM_BITS + VP8LReadBits(br, NUM_TRANSFORM_BITS);
      ok = DecodeImageStream(VP8LSubSampleSize(transform->xsize_,
                                               transform->bits_),
                             VP8LSubSampleSize(transform->ysize_,
                                               transform->bits_),
                             /*is_level0=*/0, dec, &transform->data_);
      break;
    case COLOR_INDEXING_TRANSFORM: {
       const int num_colors = VP8LReadBits(br, 8) + 1;
       const int bits = (num_colors > 16) ? 0
                      : (num_colors > 4) ? 1
                      : (num_colors > 2) ? 2
                      : 3;
       *xsize = VP8LSubSampleSize(transform->xsize_, bits);
       transform->bits_ = bits;
       ok = DecodeImageStream(num_colors, /*ysize=*/1, /*is_level0=*/0, dec,
                              &transform->data_);
       if (ok && !ExpandColorMap(num_colors, transform)) {
         return VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
       }
      break;
    }
    case SUBTRACT_GREEN_TRANSFORM:
      break;
    default:
      assert(0);    // can't happen
      break;
  }

  return ok;
}

// -----------------------------------------------------------------------------
// VP8LMetadata

static void InitMetadata(VP8LMetadata* const hdr) {
  assert(hdr != NULL);
  memset(hdr, 0, sizeof(*hdr));
}

static void ClearMetadata(VP8LMetadata* const hdr) {
  assert(hdr != NULL);

  WebPSafeFree(hdr->huffman_image_);
  VP8LHuffmanTablesDeallocate(&hdr->huffman_tables_);
  VP8LHtreeGroupsFree(hdr->htree_groups_);
  VP8LColorCacheClear(&hdr->color_cache_);
  VP8LColorCacheClear(&hdr->saved_color_cache_);
  InitMetadata(hdr);
}

// -----------------------------------------------------------------------------
// VP8LDecoder

VP8LDecoder* VP8LNew(void) {
  VP8LDecoder* const dec = (VP8LDecoder*)WebPSafeCalloc(1ULL, sizeof(*dec));
  if (dec == NULL) return NULL;
  dec->status_ = VP8_STATUS_OK;
  dec->state_ = READ_DIM;

  VP8LDspInit();  // Init critical function pointers.

  return dec;
}

// Resets the decoder in its initial state, reclaiming memory.
// Preserves the dec->status_ value.
static void VP8LClear(VP8LDecoder* const dec) {
  int i;
  if (dec == NULL) return;
  ClearMetadata(&dec->hdr_);

  WebPSafeFree(dec->pixels_);
  dec->pixels_ = NULL;
  for (i = 0; i < dec->next_transform_; ++i) {
    ClearTransform(&dec->transforms_[i]);
  }
  dec->next_transform_ = 0;
  dec->transforms_seen_ = 0;

  WebPSafeFree(dec->rescaler_memory);
  dec->rescaler_memory = NULL;

  dec->output_ = NULL;   // leave no trace behind
}

void VP8LDelete(VP8LDecoder* const dec) {
  if (dec != NULL) {
    VP8LClear(dec);
    WebPSafeFree(dec);
  }
}

static void UpdateDecoder(VP8LDecoder* const dec, int width, int height) {
  VP8LMetadata* const hdr = &dec->hdr_;
  const int num_bits = hdr->huffman_subsample_bits_;
  dec->width_ = width;
  dec->height_ = height;

  hdr->huffman_xsize_ = VP8LSubSampleSize(width, num_bits);
  hdr->huffman_mask_ = (num_bits == 0) ? ~0 : (1 << num_bits) - 1;
}

static int DecodeImageStream(int xsize, int ysize,
                             int is_level0,
                             VP8LDecoder* const dec,
                             uint32_t** const decoded_data) {
  int ok = 1;
  int transform_xsize = xsize;
  int transform_ysize = ysize;
  VP8LBitReader* const br = &dec->br_;
  VP8LMetadata* const hdr = &dec->hdr_;
  uint32_t* data = NULL;
  int color_cache_bits = 0;

  // Read the transforms (may recurse).
  if (is_level0) {
    while (ok && VP8LReadBits(br, 1)) {
      ok = ReadTransform(&transform_xsize, &transform_ysize, dec);
    }
  }

  // Color cache
  if (ok && VP8LReadBits(br, 1)) {
    color_cache_bits = VP8LReadBits(br, 4);
    ok = (color_cache_bits >= 1 && color_cache_bits <= MAX_CACHE_BITS);
    if (!ok) {
      VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
      goto End;
    }
  }

  // Read the Huffman codes (may recurse).
  ok = ok && ReadHuffmanCodes(dec, transform_xsize, transform_ysize,
                              color_cache_bits, is_level0);
  if (!ok) {
    VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
    goto End;
  }

  // Finish setting up the color-cache
  if (color_cache_bits > 0) {
    hdr->color_cache_size_ = 1 << color_cache_bits;
    if (!VP8LColorCacheInit(&hdr->color_cache_, color_cache_bits)) {
      ok = VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
      goto End;
    }
  } else {
    hdr->color_cache_size_ = 0;
  }
  UpdateDecoder(dec, transform_xsize, transform_ysize);

  if (is_level0) {   // level 0 complete
    dec->state_ = READ_HDR;
    goto End;
  }

  {
    const uint64_t total_size = (uint64_t)transform_xsize * transform_ysize;
    data = (uint32_t*)WebPSafeMalloc(total_size, sizeof(*data));
    if (data == NULL) {
      ok = VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
      goto End;
    }
  }

  // Use the Huffman trees to decode the LZ77 encoded data.
  ok = DecodeImageData(dec, data, transform_xsize, transform_ysize,
                       transform_ysize, NULL);
  ok = ok && !br->eos_;

 End:
  if (!ok) {
    WebPSafeFree(data);
    ClearMetadata(hdr);
  } else {
    if (decoded_data != NULL) {
      *decoded_data = data;
    } else {
      // We allocate image data in this function only for transforms. At level 0
      // (that is: not the transforms), we shouldn't have allocated anything.
      assert(data == NULL);
      assert(is_level0);
    }
    dec->last_pixel_ = 0;  // Reset for future DECODE_DATA_FUNC() calls.
    if (!is_level0) ClearMetadata(hdr);  // Clean up temporary data behind.
  }
  return ok;
}

//------------------------------------------------------------------------------
// Allocate internal buffers dec->pixels_ and dec->argb_cache_.
static int AllocateInternalBuffers32b(VP8LDecoder* const dec, int final_width) {
  const uint64_t num_pixels = (uint64_t)dec->width_ * dec->height_;
  // Scratch buffer corresponding to top-prediction row for transforming the
  // first row in the row-blocks. Not needed for paletted alpha.
  const uint64_t cache_top_pixels = (uint16_t)final_width;
  // Scratch buffer for temporary BGRA storage. Not needed for paletted alpha.
  const uint64_t cache_pixels = (uint64_t)final_width * NUM_ARGB_CACHE_ROWS;
  const uint64_t total_num_pixels =
      num_pixels + cache_top_pixels + cache_pixels;

  assert(dec->width_ <= final_width);
  dec->pixels_ = (uint32_t*)WebPSafeMalloc(total_num_pixels, sizeof(uint32_t));
  if (dec->pixels_ == NULL) {
    dec->argb_cache_ = NULL;    // for soundness
    return VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
  }
  dec->argb_cache_ = dec->pixels_ + num_pixels + cache_top_pixels;
  return 1;
}

static int AllocateInternalBuffers8b(VP8LDecoder* const dec) {
  const uint64_t total_num_pixels = (uint64_t)dec->width_ * dec->height_;
  dec->argb_cache_ = NULL;    // for soundness
  dec->pixels_ = (uint32_t*)WebPSafeMalloc(total_num_pixels, sizeof(uint8_t));
  if (dec->pixels_ == NULL) {
    return VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
  }
  return 1;
}

//------------------------------------------------------------------------------

// Special row-processing that only stores the alpha data.
static void ExtractAlphaRows(VP8LDecoder* const dec, int last_row) {
  int cur_row = dec->last_row_;
  int num_rows = last_row - cur_row;
  const uint32_t* in = dec->pixels_ + dec->width_ * cur_row;

  assert(last_row <= dec->io_->crop_bottom);
  while (num_rows > 0) {
    const int num_rows_to_process =
        (num_rows > NUM_ARGB_CACHE_ROWS) ? NUM_ARGB_CACHE_ROWS : num_rows;
    // Extract alpha (which is stored in the green plane).
    ALPHDecoder* const alph_dec = (ALPHDecoder*)dec->io_->opaque;
    uint8_t* const output = alph_dec->output_;
    const int width = dec->io_->width;      // the final width (!= dec->width_)
    const int cache_pixs = width * num_rows_to_process;
    uint8_t* const dst = output + width * cur_row;
    const uint32_t* const src = dec->argb_cache_;
    ApplyInverseTransforms(dec, cur_row, num_rows_to_process, in);
    WebPExtractGreen(src, dst, cache_pixs);
    AlphaApplyFilter(alph_dec,
                     cur_row, cur_row + num_rows_to_process, dst, width);
    num_rows -= num_rows_to_process;
    in += num_rows_to_process * dec->width_;
    cur_row += num_rows_to_process;
  }
  assert(cur_row == last_row);
  dec->last_row_ = dec->last_out_row_ = last_row;
}

int VP8LDecodeAlphaHeader(ALPHDecoder* const alph_dec,
                          const uint8_t* const data, size_t data_size) {
  int ok = 0;
  VP8LDecoder* dec = VP8LNew();

  if (dec == NULL) return 0;

  assert(alph_dec != NULL);

  dec->width_ = alph_dec->width_;
  dec->height_ = alph_dec->height_;
  dec->io_ = &alph_dec->io_;
  dec->io_->opaque = alph_dec;
  dec->io_->width = alph_dec->width_;
  dec->io_->height = alph_dec->height_;

  dec->status_ = VP8_STATUS_OK;
  VP8LInitBitReader(&dec->br_, data, data_size);

  if (!DecodeImageStream(alph_dec->width_, alph_dec->height_, /*is_level0=*/1,
                         dec, /*decoded_data=*/NULL)) {
    goto Err;
  }

  // Special case: if alpha data uses only the color indexing transform and
  // doesn't use color cache (a frequent case), we will use DecodeAlphaData()
  // method that only needs allocation of 1 byte per pixel (alpha channel).
  if (dec->next_transform_ == 1 &&
      dec->transforms_[0].type_ == COLOR_INDEXING_TRANSFORM &&
      Is8bOptimizable(&dec->hdr_)) {
    alph_dec->use_8b_decode_ = 1;
    ok = AllocateInternalBuffers8b(dec);
  } else {
    // Allocate internal buffers (note that dec->width_ may have changed here).
    alph_dec->use_8b_decode_ = 0;
    ok = AllocateInternalBuffers32b(dec, alph_dec->width_);
  }

  if (!ok) goto Err;

  // Only set here, once we are sure it is valid (to avoid thread races).
  alph_dec->vp8l_dec_ = dec;
  return 1;

 Err:
  VP8LDelete(dec);
  return 0;
}

int VP8LDecodeAlphaImageStream(ALPHDecoder* const alph_dec, int last_row) {
  VP8LDecoder* const dec = alph_dec->vp8l_dec_;
  assert(dec != NULL);
  assert(last_row <= dec->height_);

  if (dec->last_row_ >= last_row) {
    return 1;  // done
  }

  if (!alph_dec->use_8b_decode_) WebPInitAlphaProcessing();

  // Decode (with special row processing).
  return alph_dec->use_8b_decode_ ?
      DecodeAlphaData(dec, (uint8_t*)dec->pixels_, dec->width_, dec->height_,
                      last_row) :
      DecodeImageData(dec, dec->pixels_, dec->width_, dec->height_,
                      last_row, ExtractAlphaRows);
}

//------------------------------------------------------------------------------

int VP8LDecodeHeader(VP8LDecoder* const dec, VP8Io* const io) {
  int width, height, has_alpha;

  if (dec == NULL) return 0;
  if (io == NULL) {
    return VP8LSetError(dec, VP8_STATUS_INVALID_PARAM);
  }

  dec->io_ = io;
  dec->status_ = VP8_STATUS_OK;
  VP8LInitBitReader(&dec->br_, io->data, io->data_size);
  if (!ReadImageInfo(&dec->br_, &width, &height, &has_alpha)) {
    VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
    goto Error;
  }
  dec->state_ = READ_DIM;
  io->width = width;
  io->height = height;

  if (!DecodeImageStream(width, height, /*is_level0=*/1, dec,
                         /*decoded_data=*/NULL)) {
    goto Error;
  }
  return 1;

 Error:
  VP8LClear(dec);
  assert(dec->status_ != VP8_STATUS_OK);
  return 0;
}

int VP8LDecodeImage(VP8LDecoder* const dec) {
  VP8Io* io = NULL;
  WebPDecParams* params = NULL;

  if (dec == NULL) return 0;

  assert(dec->hdr_.huffman_tables_.root.start != NULL);
  assert(dec->hdr_.htree_groups_ != NULL);
  assert(dec->hdr_.num_htree_groups_ > 0);

  io = dec->io_;
  assert(io != NULL);
  params = (WebPDecParams*)io->opaque;
  assert(params != NULL);

  // Initialization.
  if (dec->state_ != READ_DATA) {
    dec->output_ = params->output;
    assert(dec->output_ != NULL);

    if (!WebPIoInitFromOptions(params->options, io, MODE_BGRA)) {
      VP8LSetError(dec, VP8_STATUS_INVALID_PARAM);
      goto Err;
    }

    if (!AllocateInternalBuffers32b(dec, io->width)) goto Err;

#if !defined(WEBP_REDUCE_SIZE)
    if (io->use_scaling && !AllocateAndInitRescaler(dec, io)) goto Err;
#else
    if (io->use_scaling) {
      VP8LSetError(dec, VP8_STATUS_INVALID_PARAM);
      goto Err;
    }
#endif
    if (io->use_scaling || WebPIsPremultipliedMode(dec->output_->colorspace)) {
      // need the alpha-multiply functions for premultiplied output or rescaling
      WebPInitAlphaProcessing();
    }

    if (!WebPIsRGBMode(dec->output_->colorspace)) {
      WebPInitConvertARGBToYUV();
      if (dec->output_->u.YUVA.a != NULL) WebPInitAlphaProcessing();
    }
    if (dec->incremental_) {
      if (dec->hdr_.color_cache_size_ > 0 &&
          dec->hdr_.saved_color_cache_.colors_ == NULL) {
        if (!VP8LColorCacheInit(&dec->hdr_.saved_color_cache_,
                                dec->hdr_.color_cache_.hash_bits_)) {
          VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
          goto Err;
        }
      }
    }
    dec->state_ = READ_DATA;
  }

  // Decode.
  if (!DecodeImageData(dec, dec->pixels_, dec->width_, dec->height_,
                       io->crop_bottom, ProcessRows)) {
    goto Err;
  }

  params->last_y = dec->last_out_row_;
  return 1;

 Err:
  VP8LClear(dec);
  assert(dec->status_ != VP8_STATUS_OK);
  return 0;
}

//------------------------------------------------------------------------------
