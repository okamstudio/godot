// Copyright 2012 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Author: Jyrki Alakuijala (jyrki@google.com)
//
#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#include <string.h>

#include "src/dsp/lossless.h"
#include "src/dsp/lossless_common.h"
#include "src/enc/backward_references_enc.h"
#include "src/enc/histogram_enc.h"
#include "src/enc/vp8i_enc.h"
#include "src/utils/utils.h"

// Number of partitions for the three dominant (literal, red and blue) symbol
// costs.
#define NUM_PARTITIONS 4
// The size of the bin-hash corresponding to the three dominant costs.
#define BIN_SIZE (NUM_PARTITIONS * NUM_PARTITIONS * NUM_PARTITIONS)
// Maximum number of histograms allowed in greedy combining algorithm.
#define MAX_HISTO_GREEDY 100

// Return the size of the histogram for a given cache_bits.
static int GetHistogramSize(int cache_bits) {
  const int literal_size = VP8LHistogramNumCodes(cache_bits);
  const size_t total_size = sizeof(VP8LHistogram) + sizeof(int) * literal_size;
  assert(total_size <= (size_t)0x7fffffff);
  return (int)total_size;
}

static void HistogramClear(VP8LHistogram* const p) {
  uint32_t* const literal = p->literal_;
  const int cache_bits = p->palette_code_bits_;
  const int histo_size = GetHistogramSize(cache_bits);
  memset(p, 0, histo_size);
  p->palette_code_bits_ = cache_bits;
  p->literal_ = literal;
}

// Swap two histogram pointers.
static void HistogramSwap(VP8LHistogram** const A, VP8LHistogram** const B) {
  VP8LHistogram* const tmp = *A;
  *A = *B;
  *B = tmp;
}

static void HistogramCopy(const VP8LHistogram* const src,
                          VP8LHistogram* const dst) {
  uint32_t* const dst_literal = dst->literal_;
  const int dst_cache_bits = dst->palette_code_bits_;
  const int literal_size = VP8LHistogramNumCodes(dst_cache_bits);
  const int histo_size = GetHistogramSize(dst_cache_bits);
  assert(src->palette_code_bits_ == dst_cache_bits);
  memcpy(dst, src, histo_size);
  dst->literal_ = dst_literal;
  memcpy(dst->literal_, src->literal_, literal_size * sizeof(*dst->literal_));
}

void VP8LFreeHistogram(VP8LHistogram* const histo) {
  WebPSafeFree(histo);
}

void VP8LFreeHistogramSet(VP8LHistogramSet* const histo) {
  WebPSafeFree(histo);
}

void VP8LHistogramStoreRefs(const VP8LBackwardRefs* const refs,
                            VP8LHistogram* const histo) {
  VP8LRefsCursor c = VP8LRefsCursorInit(refs);
  while (VP8LRefsCursorOk(&c)) {
    VP8LHistogramAddSinglePixOrCopy(histo, c.cur_pos, NULL, 0);
    VP8LRefsCursorNext(&c);
  }
}

void VP8LHistogramCreate(VP8LHistogram* const p,
                         const VP8LBackwardRefs* const refs,
                         int palette_code_bits) {
  if (palette_code_bits >= 0) {
    p->palette_code_bits_ = palette_code_bits;
  }
  HistogramClear(p);
  VP8LHistogramStoreRefs(refs, p);
}

void VP8LHistogramInit(VP8LHistogram* const p, int palette_code_bits,
                       int init_arrays) {
  p->palette_code_bits_ = palette_code_bits;
  if (init_arrays) {
    HistogramClear(p);
  } else {
    p->trivial_symbol_ = 0;
    p->bit_cost_ = 0;
    p->literal_cost_ = 0;
    p->red_cost_ = 0;
    p->blue_cost_ = 0;
    memset(p->is_used_, 0, sizeof(p->is_used_));
  }
}

VP8LHistogram* VP8LAllocateHistogram(int cache_bits) {
  VP8LHistogram* histo = NULL;
  const int total_size = GetHistogramSize(cache_bits);
  uint8_t* const memory = (uint8_t*)WebPSafeMalloc(total_size, sizeof(*memory));
  if (memory == NULL) return NULL;
  histo = (VP8LHistogram*)memory;
  // literal_ won't necessary be aligned.
  histo->literal_ = (uint32_t*)(memory + sizeof(VP8LHistogram));
  VP8LHistogramInit(histo, cache_bits, /*init_arrays=*/ 0);
  return histo;
}

// Resets the pointers of the histograms to point to the bit buffer in the set.
static void HistogramSetResetPointers(VP8LHistogramSet* const set,
                                      int cache_bits) {
  int i;
  const int histo_size = GetHistogramSize(cache_bits);
  uint8_t* memory = (uint8_t*) (set->histograms);
  memory += set->max_size * sizeof(*set->histograms);
  for (i = 0; i < set->max_size; ++i) {
    memory = (uint8_t*) WEBP_ALIGN(memory);
    set->histograms[i] = (VP8LHistogram*) memory;
    // literal_ won't necessary be aligned.
    set->histograms[i]->literal_ = (uint32_t*)(memory + sizeof(VP8LHistogram));
    memory += histo_size;
  }
}

// Returns the total size of the VP8LHistogramSet.
static size_t HistogramSetTotalSize(int size, int cache_bits) {
  const int histo_size = GetHistogramSize(cache_bits);
  return (sizeof(VP8LHistogramSet) + size * (sizeof(VP8LHistogram*) +
          histo_size + WEBP_ALIGN_CST));
}

VP8LHistogramSet* VP8LAllocateHistogramSet(int size, int cache_bits) {
  int i;
  VP8LHistogramSet* set;
  const size_t total_size = HistogramSetTotalSize(size, cache_bits);
  uint8_t* memory = (uint8_t*)WebPSafeMalloc(total_size, sizeof(*memory));
  if (memory == NULL) return NULL;

  set = (VP8LHistogramSet*)memory;
  memory += sizeof(*set);
  set->histograms = (VP8LHistogram**)memory;
  set->max_size = size;
  set->size = size;
  HistogramSetResetPointers(set, cache_bits);
  for (i = 0; i < size; ++i) {
    VP8LHistogramInit(set->histograms[i], cache_bits, /*init_arrays=*/ 0);
  }
  return set;
}

void VP8LHistogramSetClear(VP8LHistogramSet* const set) {
  int i;
  const int cache_bits = set->histograms[0]->palette_code_bits_;
  const int size = set->max_size;
  const size_t total_size = HistogramSetTotalSize(size, cache_bits);
  uint8_t* memory = (uint8_t*)set;

  memset(memory, 0, total_size);
  memory += sizeof(*set);
  set->histograms = (VP8LHistogram**)memory;
  set->max_size = size;
  set->size = size;
  HistogramSetResetPointers(set, cache_bits);
  for (i = 0; i < size; ++i) {
    set->histograms[i]->palette_code_bits_ = cache_bits;
  }
}

// Removes the histogram 'i' from 'set' by setting it to NULL.
static void HistogramSetRemoveHistogram(VP8LHistogramSet* const set, int i,
                                        int* const num_used) {
  assert(set->histograms[i] != NULL);
  set->histograms[i] = NULL;
  --*num_used;
  // If we remove the last valid one, shrink until the next valid one.
  if (i == set->size - 1) {
    while (set->size >= 1 && set->histograms[set->size - 1] == NULL) {
      --set->size;
    }
  }
}

// -----------------------------------------------------------------------------

void VP8LHistogramAddSinglePixOrCopy(VP8LHistogram* const histo,
                                     const PixOrCopy* const v,
                                     int (*const distance_modifier)(int, int),
                                     int distance_modifier_arg0) {
  if (PixOrCopyIsLiteral(v)) {
    ++histo->alpha_[PixOrCopyLiteral(v, 3)];
    ++histo->red_[PixOrCopyLiteral(v, 2)];
    ++histo->literal_[PixOrCopyLiteral(v, 1)];
    ++histo->blue_[PixOrCopyLiteral(v, 0)];
  } else if (PixOrCopyIsCacheIdx(v)) {
    const int literal_ix =
        NUM_LITERAL_CODES + NUM_LENGTH_CODES + PixOrCopyCacheIdx(v);
    assert(histo->palette_code_bits_ != 0);
    ++histo->literal_[literal_ix];
  } else {
    int code, extra_bits;
    VP8LPrefixEncodeBits(PixOrCopyLength(v), &code, &extra_bits);
    ++histo->literal_[NUM_LITERAL_CODES + code];
    if (distance_modifier == NULL) {
      VP8LPrefixEncodeBits(PixOrCopyDistance(v), &code, &extra_bits);
    } else {
      VP8LPrefixEncodeBits(
          distance_modifier(distance_modifier_arg0, PixOrCopyDistance(v)),
          &code, &extra_bits);
    }
    ++histo->distance_[code];
  }
}

// -----------------------------------------------------------------------------
// Entropy-related functions.

static WEBP_INLINE uint64_t BitsEntropyRefine(const VP8LBitEntropy* entropy) {
  uint64_t mix;
  if (entropy->nonzeros < 5) {
    if (entropy->nonzeros <= 1) {
      return 0;
    }
    // Two symbols, they will be 0 and 1 in a Huffman code.
    // Let's mix in a bit of entropy to favor good clustering when
    // distributions of these are combined.
    if (entropy->nonzeros == 2) {
      return DivRound(99 * ((uint64_t)entropy->sum << LOG_2_PRECISION_BITS) +
                          entropy->entropy,
                      100);
    }
    // No matter what the entropy says, we cannot be better than min_limit
    // with Huffman coding. I am mixing a bit of entropy into the
    // min_limit since it produces much better (~0.5 %) compression results
    // perhaps because of better entropy clustering.
    if (entropy->nonzeros == 3) {
      mix = 950;
    } else {
      mix = 700;  // nonzeros == 4.
    }
  } else {
    mix = 627;
  }

  {
    uint64_t min_limit = (uint64_t)(2 * entropy->sum - entropy->max_val)
                         << LOG_2_PRECISION_BITS;
    min_limit =
        DivRound(mix * min_limit + (1000 - mix) * entropy->entropy, 1000);
    return (entropy->entropy < min_limit) ? min_limit : entropy->entropy;
  }
}

uint64_t VP8LBitsEntropy(const uint32_t* const array, int n) {
  VP8LBitEntropy entropy;
  VP8LBitsEntropyUnrefined(array, n, &entropy);

  return BitsEntropyRefine(&entropy);
}

static uint64_t InitialHuffmanCost(void) {
  // Small bias because Huffman code length is typically not stored in
  // full length.
  static const uint64_t kHuffmanCodeOfHuffmanCodeSize = CODE_LENGTH_CODES * 3;
  // Subtract a bias of 9.1.
  return (kHuffmanCodeOfHuffmanCodeSize << LOG_2_PRECISION_BITS) -
         DivRound(91ll << LOG_2_PRECISION_BITS, 10);
}

// Finalize the Huffman cost based on streak numbers and length type (<3 or >=3)
static uint64_t FinalHuffmanCost(const VP8LStreaks* const stats) {
  // The constants in this function are empirical and got rounded from
  // their original values in 1/8 when switched to 1/1024.
  uint64_t retval = InitialHuffmanCost();
  // Second coefficient: Many zeros in the histogram are covered efficiently
  // by a run-length encode. Originally 2/8.
  uint32_t retval_extra = stats->counts[0] * 1600 + 240 * stats->streaks[0][1];
  // Second coefficient: Constant values are encoded less efficiently, but still
  // RLE'ed. Originally 6/8.
  retval_extra += stats->counts[1] * 2640 + 720 * stats->streaks[1][1];
  // 0s are usually encoded more efficiently than non-0s.
  // Originally 15/8.
  retval_extra += 1840 * stats->streaks[0][0];
  // Originally 26/8.
  retval_extra += 3360 * stats->streaks[1][0];
  return retval + ((uint64_t)retval_extra << (LOG_2_PRECISION_BITS - 10));
}

// Get the symbol entropy for the distribution 'population'.
// Set 'trivial_sym', if there's only one symbol present in the distribution.
static uint64_t PopulationCost(const uint32_t* const population, int length,
                               uint32_t* const trivial_sym,
                               uint8_t* const is_used) {
  VP8LBitEntropy bit_entropy;
  VP8LStreaks stats;
  VP8LGetEntropyUnrefined(population, length, &bit_entropy, &stats);
  if (trivial_sym != NULL) {
    *trivial_sym = (bit_entropy.nonzeros == 1) ? bit_entropy.nonzero_code
                                               : VP8L_NON_TRIVIAL_SYM;
  }
  // The histogram is used if there is at least one non-zero streak.
  *is_used = (stats.streaks[1][0] != 0 || stats.streaks[1][1] != 0);

  return BitsEntropyRefine(&bit_entropy) + FinalHuffmanCost(&stats);
}

// trivial_at_end is 1 if the two histograms only have one element that is
// non-zero: both the zero-th one, or both the last one.
static WEBP_INLINE uint64_t GetCombinedEntropy(const uint32_t* const X,
                                               const uint32_t* const Y,
                                               int length, int is_X_used,
                                               int is_Y_used,
                                               int trivial_at_end) {
  VP8LStreaks stats;
  if (trivial_at_end) {
    // This configuration is due to palettization that transforms an indexed
    // pixel into 0xff000000 | (pixel << 8) in VP8LBundleColorMap.
    // BitsEntropyRefine is 0 for histograms with only one non-zero value.
    // Only FinalHuffmanCost needs to be evaluated.
    memset(&stats, 0, sizeof(stats));
    // Deal with the non-zero value at index 0 or length-1.
    stats.streaks[1][0] = 1;
    // Deal with the following/previous zero streak.
    stats.counts[0] = 1;
    stats.streaks[0][1] = length - 1;
    return FinalHuffmanCost(&stats);
  } else {
    VP8LBitEntropy bit_entropy;
    if (is_X_used) {
      if (is_Y_used) {
        VP8LGetCombinedEntropyUnrefined(X, Y, length, &bit_entropy, &stats);
      } else {
        VP8LGetEntropyUnrefined(X, length, &bit_entropy, &stats);
      }
    } else {
      if (is_Y_used) {
        VP8LGetEntropyUnrefined(Y, length, &bit_entropy, &stats);
      } else {
        memset(&stats, 0, sizeof(stats));
        stats.counts[0] = 1;
        stats.streaks[0][length > 3] = length;
        VP8LBitEntropyInit(&bit_entropy);
      }
    }

    return BitsEntropyRefine(&bit_entropy) + FinalHuffmanCost(&stats);
  }
}

// Estimates the Entropy + Huffman + other block overhead size cost.
uint64_t VP8LHistogramEstimateBits(VP8LHistogram* const p) {
  return PopulationCost(p->literal_,
                        VP8LHistogramNumCodes(p->palette_code_bits_), NULL,
                        &p->is_used_[0]) +
         PopulationCost(p->red_, NUM_LITERAL_CODES, NULL, &p->is_used_[1]) +
         PopulationCost(p->blue_, NUM_LITERAL_CODES, NULL, &p->is_used_[2]) +
         PopulationCost(p->alpha_, NUM_LITERAL_CODES, NULL, &p->is_used_[3]) +
         PopulationCost(p->distance_, NUM_DISTANCE_CODES, NULL,
                        &p->is_used_[4]) +
         ((uint64_t)(VP8LExtraCost(p->literal_ + NUM_LITERAL_CODES,
                                   NUM_LENGTH_CODES) +
                     VP8LExtraCost(p->distance_, NUM_DISTANCE_CODES))
          << LOG_2_PRECISION_BITS);
}

// -----------------------------------------------------------------------------
// Various histogram combine/cost-eval functions

// Set a + b in b, saturating at WEBP_INT64_MAX.
static WEBP_INLINE void SaturateAdd(uint64_t a, int64_t* b) {
  if (*b < 0 || (int64_t)a <= WEBP_INT64_MAX - *b) {
    *b += (int64_t)a;
  } else {
    *b = WEBP_INT64_MAX;
  }
}

// Returns 1 if the cost of the combined histogram is less than the threshold.
// Otherwise returns 0 and the cost is invalid due to early bail-out.
WEBP_NODISCARD static int GetCombinedHistogramEntropy(
    const VP8LHistogram* const a, const VP8LHistogram* const b,
    int64_t cost_threshold_in, uint64_t* cost) {
  const int palette_code_bits = a->palette_code_bits_;
  int trivial_at_end = 0;
  const uint64_t cost_threshold = (uint64_t)cost_threshold_in;
  assert(a->palette_code_bits_ == b->palette_code_bits_);
  if (cost_threshold_in <= 0) return 0;
  *cost = GetCombinedEntropy(a->literal_, b->literal_,
                             VP8LHistogramNumCodes(palette_code_bits),
                             a->is_used_[0], b->is_used_[0], 0);
  *cost += (uint64_t)VP8LExtraCostCombined(a->literal_ + NUM_LITERAL_CODES,
                                           b->literal_ + NUM_LITERAL_CODES,
                                           NUM_LENGTH_CODES)
           << LOG_2_PRECISION_BITS;
  if (*cost >= cost_threshold) return 0;

  if (a->trivial_symbol_ != VP8L_NON_TRIVIAL_SYM &&
      a->trivial_symbol_ == b->trivial_symbol_) {
    // A, R and B are all 0 or 0xff.
    const uint32_t color_a = (a->trivial_symbol_ >> 24) & 0xff;
    const uint32_t color_r = (a->trivial_symbol_ >> 16) & 0xff;
    const uint32_t color_b = (a->trivial_symbol_ >> 0) & 0xff;
    if ((color_a == 0 || color_a == 0xff) &&
        (color_r == 0 || color_r == 0xff) &&
        (color_b == 0 || color_b == 0xff)) {
      trivial_at_end = 1;
    }
  }

  *cost += GetCombinedEntropy(a->red_, b->red_, NUM_LITERAL_CODES,
                              a->is_used_[1], b->is_used_[1], trivial_at_end);
  if (*cost >= cost_threshold) return 0;

  *cost += GetCombinedEntropy(a->blue_, b->blue_, NUM_LITERAL_CODES,
                              a->is_used_[2], b->is_used_[2], trivial_at_end);
  if (*cost >= cost_threshold) return 0;

  *cost += GetCombinedEntropy(a->alpha_, b->alpha_, NUM_LITERAL_CODES,
                              a->is_used_[3], b->is_used_[3], trivial_at_end);
  if (*cost >= cost_threshold) return 0;

  *cost += GetCombinedEntropy(a->distance_, b->distance_, NUM_DISTANCE_CODES,
                              a->is_used_[4], b->is_used_[4], 0);
  *cost += (uint64_t)VP8LExtraCostCombined(a->distance_, b->distance_,
                                           NUM_DISTANCE_CODES)
           << LOG_2_PRECISION_BITS;
  if (*cost >= cost_threshold) return 0;

  return 1;
}

static WEBP_INLINE void HistogramAdd(const VP8LHistogram* const a,
                                     const VP8LHistogram* const b,
                                     VP8LHistogram* const out) {
  VP8LHistogramAdd(a, b, out);
  out->trivial_symbol_ = (a->trivial_symbol_ == b->trivial_symbol_)
                       ? a->trivial_symbol_
                       : VP8L_NON_TRIVIAL_SYM;
}

// Performs out = a + b, computing the cost C(a+b) - C(a) - C(b) while comparing
// to the threshold value 'cost_threshold'. The score returned is
//  Score = C(a+b) - C(a) - C(b), where C(a) + C(b) is known and fixed.
// Since the previous score passed is 'cost_threshold', we only need to compare
// the partial cost against 'cost_threshold + C(a) + C(b)' to possibly bail-out
// early.
// Returns 1 if the cost is less than the threshold.
// Otherwise returns 0 and the cost is invalid due to early bail-out.
WEBP_NODISCARD static int HistogramAddEval(const VP8LHistogram* const a,
                                           const VP8LHistogram* const b,
                                           VP8LHistogram* const out,
                                           int64_t cost_threshold) {
  uint64_t cost;
  const uint64_t sum_cost = a->bit_cost_ + b->bit_cost_;
  SaturateAdd(sum_cost, &cost_threshold);
  if (!GetCombinedHistogramEntropy(a, b, cost_threshold, &cost)) return 0;

  HistogramAdd(a, b, out);
  out->bit_cost_ = cost;
  out->palette_code_bits_ = a->palette_code_bits_;
  return 1;
}

// Same as HistogramAddEval(), except that the resulting histogram
// is not stored. Only the cost C(a+b) - C(a) is evaluated. We omit
// the term C(b) which is constant over all the evaluations.
// Returns 1 if the cost is less than the threshold.
// Otherwise returns 0 and the cost is invalid due to early bail-out.
WEBP_NODISCARD static int HistogramAddThresh(const VP8LHistogram* const a,
                                             const VP8LHistogram* const b,
                                             int64_t cost_threshold,
                                             int64_t* cost_out) {
  uint64_t cost;
  assert(a != NULL && b != NULL);
  SaturateAdd(a->bit_cost_, &cost_threshold);
  if (!GetCombinedHistogramEntropy(a, b, cost_threshold, &cost)) return 0;

  *cost_out = (int64_t)cost - (int64_t)a->bit_cost_;
  return 1;
}

// -----------------------------------------------------------------------------

// The structure to keep track of cost range for the three dominant entropy
// symbols.
typedef struct {
  uint64_t literal_max_;
  uint64_t literal_min_;
  uint64_t red_max_;
  uint64_t red_min_;
  uint64_t blue_max_;
  uint64_t blue_min_;
} DominantCostRange;

static void DominantCostRangeInit(DominantCostRange* const c) {
  c->literal_max_ = 0;
  c->literal_min_ = WEBP_UINT64_MAX;
  c->red_max_ = 0;
  c->red_min_ = WEBP_UINT64_MAX;
  c->blue_max_ = 0;
  c->blue_min_ = WEBP_UINT64_MAX;
}

static void UpdateDominantCostRange(
    const VP8LHistogram* const h, DominantCostRange* const c) {
  if (c->literal_max_ < h->literal_cost_) c->literal_max_ = h->literal_cost_;
  if (c->literal_min_ > h->literal_cost_) c->literal_min_ = h->literal_cost_;
  if (c->red_max_ < h->red_cost_) c->red_max_ = h->red_cost_;
  if (c->red_min_ > h->red_cost_) c->red_min_ = h->red_cost_;
  if (c->blue_max_ < h->blue_cost_) c->blue_max_ = h->blue_cost_;
  if (c->blue_min_ > h->blue_cost_) c->blue_min_ = h->blue_cost_;
}

static void UpdateHistogramCost(VP8LHistogram* const h) {
  uint32_t alpha_sym, red_sym, blue_sym;
  const uint64_t alpha_cost =
      PopulationCost(h->alpha_, NUM_LITERAL_CODES, &alpha_sym, &h->is_used_[3]);
  const uint64_t distance_cost =
      PopulationCost(h->distance_, NUM_DISTANCE_CODES, NULL, &h->is_used_[4]) +
      ((uint64_t)VP8LExtraCost(h->distance_, NUM_DISTANCE_CODES)
       << LOG_2_PRECISION_BITS);
  const int num_codes = VP8LHistogramNumCodes(h->palette_code_bits_);
  h->literal_cost_ =
      PopulationCost(h->literal_, num_codes, NULL, &h->is_used_[0]) +
      ((uint64_t)VP8LExtraCost(h->literal_ + NUM_LITERAL_CODES,
                               NUM_LENGTH_CODES)
       << LOG_2_PRECISION_BITS);
  h->red_cost_ =
      PopulationCost(h->red_, NUM_LITERAL_CODES, &red_sym, &h->is_used_[1]);
  h->blue_cost_ =
      PopulationCost(h->blue_, NUM_LITERAL_CODES, &blue_sym, &h->is_used_[2]);
  h->bit_cost_ = h->literal_cost_ + h->red_cost_ + h->blue_cost_ +
                 alpha_cost + distance_cost;
  if ((alpha_sym | red_sym | blue_sym) == VP8L_NON_TRIVIAL_SYM) {
    h->trivial_symbol_ = VP8L_NON_TRIVIAL_SYM;
  } else {
    h->trivial_symbol_ =
        ((uint32_t)alpha_sym << 24) | (red_sym << 16) | (blue_sym << 0);
  }
}

static int GetBinIdForEntropy(uint64_t min, uint64_t max, uint64_t val) {
  const uint64_t range = max - min;
  if (range > 0) {
    const uint64_t delta = val - min;
    return (int)((NUM_PARTITIONS - 1e-6) * delta / range);
  } else {
    return 0;
  }
}

static int GetHistoBinIndex(const VP8LHistogram* const h,
                            const DominantCostRange* const c, int low_effort) {
  int bin_id = GetBinIdForEntropy(c->literal_min_, c->literal_max_,
                                  h->literal_cost_);
  assert(bin_id < NUM_PARTITIONS);
  if (!low_effort) {
    bin_id = bin_id * NUM_PARTITIONS
           + GetBinIdForEntropy(c->red_min_, c->red_max_, h->red_cost_);
    bin_id = bin_id * NUM_PARTITIONS
           + GetBinIdForEntropy(c->blue_min_, c->blue_max_, h->blue_cost_);
    assert(bin_id < BIN_SIZE);
  }
  return bin_id;
}

// Construct the histograms from backward references.
static void HistogramBuild(
    int xsize, int histo_bits, const VP8LBackwardRefs* const backward_refs,
    VP8LHistogramSet* const image_histo) {
  int x = 0, y = 0;
  const int histo_xsize = VP8LSubSampleSize(xsize, histo_bits);
  VP8LHistogram** const histograms = image_histo->histograms;
  VP8LRefsCursor c = VP8LRefsCursorInit(backward_refs);
  assert(histo_bits > 0);
  VP8LHistogramSetClear(image_histo);
  while (VP8LRefsCursorOk(&c)) {
    const PixOrCopy* const v = c.cur_pos;
    const int ix = (y >> histo_bits) * histo_xsize + (x >> histo_bits);
    VP8LHistogramAddSinglePixOrCopy(histograms[ix], v, NULL, 0);
    x += PixOrCopyLength(v);
    while (x >= xsize) {
      x -= xsize;
      ++y;
    }
    VP8LRefsCursorNext(&c);
  }
}

// Copies the histograms and computes its bit_cost.
static const uint32_t kInvalidHistogramSymbol = (uint32_t)(-1);
static void HistogramCopyAndAnalyze(VP8LHistogramSet* const orig_histo,
                                    VP8LHistogramSet* const image_histo,
                                    int* const num_used,
                                    uint32_t* const histogram_symbols) {
  int i, cluster_id;
  int num_used_orig = *num_used;
  VP8LHistogram** const orig_histograms = orig_histo->histograms;
  VP8LHistogram** const histograms = image_histo->histograms;
  assert(image_histo->max_size == orig_histo->max_size);
  for (cluster_id = 0, i = 0; i < orig_histo->max_size; ++i) {
    VP8LHistogram* const histo = orig_histograms[i];
    UpdateHistogramCost(histo);

    // Skip the histogram if it is completely empty, which can happen for tiles
    // with no information (when they are skipped because of LZ77).
    if (!histo->is_used_[0] && !histo->is_used_[1] && !histo->is_used_[2]
        && !histo->is_used_[3] && !histo->is_used_[4]) {
      // The first histogram is always used. If an histogram is empty, we set
      // its id to be the same as the previous one: this will improve
      // compressibility for later LZ77.
      assert(i > 0);
      HistogramSetRemoveHistogram(image_histo, i, num_used);
      HistogramSetRemoveHistogram(orig_histo, i, &num_used_orig);
      histogram_symbols[i] = kInvalidHistogramSymbol;
    } else {
      // Copy histograms from orig_histo[] to image_histo[].
      HistogramCopy(histo, histograms[i]);
      histogram_symbols[i] = cluster_id++;
      assert(cluster_id <= image_histo->max_size);
    }
  }
}

// Partition histograms to different entropy bins for three dominant (literal,
// red and blue) symbol costs and compute the histogram aggregate bit_cost.
static void HistogramAnalyzeEntropyBin(VP8LHistogramSet* const image_histo,
                                       uint16_t* const bin_map,
                                       int low_effort) {
  int i;
  VP8LHistogram** const histograms = image_histo->histograms;
  const int histo_size = image_histo->size;
  DominantCostRange cost_range;
  DominantCostRangeInit(&cost_range);

  // Analyze the dominant (literal, red and blue) entropy costs.
  for (i = 0; i < histo_size; ++i) {
    if (histograms[i] == NULL) continue;
    UpdateDominantCostRange(histograms[i], &cost_range);
  }

  // bin-hash histograms on three of the dominant (literal, red and blue)
  // symbol costs and store the resulting bin_id for each histogram.
  for (i = 0; i < histo_size; ++i) {
    // bin_map[i] is not set to a special value as its use will later be guarded
    // by another (histograms[i] == NULL).
    if (histograms[i] == NULL) continue;
    bin_map[i] = GetHistoBinIndex(histograms[i], &cost_range, low_effort);
  }
}

// Merges some histograms with same bin_id together if it's advantageous.
// Sets the remaining histograms to NULL.
// 'combine_cost_factor' has to be divided by 100.
static void HistogramCombineEntropyBin(
    VP8LHistogramSet* const image_histo, int* num_used,
    const uint32_t* const clusters, uint16_t* const cluster_mappings,
    VP8LHistogram* cur_combo, const uint16_t* const bin_map, int num_bins,
    int32_t combine_cost_factor, int low_effort) {
  VP8LHistogram** const histograms = image_histo->histograms;
  int idx;
  struct {
    int16_t first;    // position of the histogram that accumulates all
                      // histograms with the same bin_id
    uint16_t num_combine_failures;   // number of combine failures per bin_id
  } bin_info[BIN_SIZE];

  assert(num_bins <= BIN_SIZE);
  for (idx = 0; idx < num_bins; ++idx) {
    bin_info[idx].first = -1;
    bin_info[idx].num_combine_failures = 0;
  }

  // By default, a cluster matches itself.
  for (idx = 0; idx < *num_used; ++idx) cluster_mappings[idx] = idx;
  for (idx = 0; idx < image_histo->size; ++idx) {
    int bin_id, first;
    if (histograms[idx] == NULL) continue;
    bin_id = bin_map[idx];
    first = bin_info[bin_id].first;
    if (first == -1) {
      bin_info[bin_id].first = idx;
    } else if (low_effort) {
      HistogramAdd(histograms[idx], histograms[first], histograms[first]);
      HistogramSetRemoveHistogram(image_histo, idx, num_used);
      cluster_mappings[clusters[idx]] = clusters[first];
    } else {
      // try to merge #idx into #first (both share the same bin_id)
      const uint64_t bit_cost = histograms[idx]->bit_cost_;
      const int64_t bit_cost_thresh =
          -DivRound((int64_t)bit_cost * combine_cost_factor, 100);
      if (HistogramAddEval(histograms[first], histograms[idx], cur_combo,
                           bit_cost_thresh)) {
        // Try to merge two histograms only if the combo is a trivial one or
        // the two candidate histograms are already non-trivial.
        // For some images, 'try_combine' turns out to be false for a lot of
        // histogram pairs. In that case, we fallback to combining
        // histograms as usual to avoid increasing the header size.
        const int try_combine =
            (cur_combo->trivial_symbol_ != VP8L_NON_TRIVIAL_SYM) ||
            ((histograms[idx]->trivial_symbol_ == VP8L_NON_TRIVIAL_SYM) &&
             (histograms[first]->trivial_symbol_ == VP8L_NON_TRIVIAL_SYM));
        const int max_combine_failures = 32;
        if (try_combine ||
            bin_info[bin_id].num_combine_failures >= max_combine_failures) {
          // move the (better) merged histogram to its final slot
          HistogramSwap(&cur_combo, &histograms[first]);
          HistogramSetRemoveHistogram(image_histo, idx, num_used);
          cluster_mappings[clusters[idx]] = clusters[first];
        } else {
          ++bin_info[bin_id].num_combine_failures;
        }
      }
    }
  }
  if (low_effort) {
    // for low_effort case, update the final cost when everything is merged
    for (idx = 0; idx < image_histo->size; ++idx) {
      if (histograms[idx] == NULL) continue;
      UpdateHistogramCost(histograms[idx]);
    }
  }
}

// Implement a Lehmer random number generator with a multiplicative constant of
// 48271 and a modulo constant of 2^31 - 1.
static uint32_t MyRand(uint32_t* const seed) {
  *seed = (uint32_t)(((uint64_t)(*seed) * 48271u) % 2147483647u);
  assert(*seed > 0);
  return *seed;
}

// -----------------------------------------------------------------------------
// Histogram pairs priority queue

// Pair of histograms. Negative idx1 value means that pair is out-of-date.
typedef struct {
  int idx1;
  int idx2;
  int64_t cost_diff;
  uint64_t cost_combo;
} HistogramPair;

typedef struct {
  HistogramPair* queue;
  int size;
  int max_size;
} HistoQueue;

static int HistoQueueInit(HistoQueue* const histo_queue, const int max_size) {
  histo_queue->size = 0;
  histo_queue->max_size = max_size;
  // We allocate max_size + 1 because the last element at index "size" is
  // used as temporary data (and it could be up to max_size).
  histo_queue->queue = (HistogramPair*)WebPSafeMalloc(
      histo_queue->max_size + 1, sizeof(*histo_queue->queue));
  return histo_queue->queue != NULL;
}

static void HistoQueueClear(HistoQueue* const histo_queue) {
  assert(histo_queue != NULL);
  WebPSafeFree(histo_queue->queue);
  histo_queue->size = 0;
  histo_queue->max_size = 0;
}

// Pop a specific pair in the queue by replacing it with the last one
// and shrinking the queue.
static void HistoQueuePopPair(HistoQueue* const histo_queue,
                              HistogramPair* const pair) {
  assert(pair >= histo_queue->queue &&
         pair < (histo_queue->queue + histo_queue->size));
  assert(histo_queue->size > 0);
  *pair = histo_queue->queue[histo_queue->size - 1];
  --histo_queue->size;
}

// Check whether a pair in the queue should be updated as head or not.
static void HistoQueueUpdateHead(HistoQueue* const histo_queue,
                                 HistogramPair* const pair) {
  assert(pair->cost_diff < 0);
  assert(pair >= histo_queue->queue &&
         pair < (histo_queue->queue + histo_queue->size));
  assert(histo_queue->size > 0);
  if (pair->cost_diff < histo_queue->queue[0].cost_diff) {
    // Replace the best pair.
    const HistogramPair tmp = histo_queue->queue[0];
    histo_queue->queue[0] = *pair;
    *pair = tmp;
  }
}

// Update the cost diff and combo of a pair of histograms. This needs to be
// called when the histograms have been merged with a third one.
// Returns 1 if the cost diff is less than the threshold.
// Otherwise returns 0 and the cost is invalid due to early bail-out.
WEBP_NODISCARD static int HistoQueueUpdatePair(const VP8LHistogram* const h1,
                                               const VP8LHistogram* const h2,
                                               int64_t cost_threshold,
                                               HistogramPair* const pair) {
  const int64_t sum_cost = h1->bit_cost_ + h2->bit_cost_;
  SaturateAdd(sum_cost, &cost_threshold);
  if (!GetCombinedHistogramEntropy(h1, h2, cost_threshold, &pair->cost_combo)) {
    return 0;
  }
  pair->cost_diff = (int64_t)pair->cost_combo - sum_cost;
  return 1;
}

// Create a pair from indices "idx1" and "idx2" provided its cost
// is inferior to "threshold", a negative entropy.
// It returns the cost of the pair, or 0 if it superior to threshold.
static int64_t HistoQueuePush(HistoQueue* const histo_queue,
                              VP8LHistogram** const histograms, int idx1,
                              int idx2, int64_t threshold) {
  const VP8LHistogram* h1;
  const VP8LHistogram* h2;
  HistogramPair pair;

  // Stop here if the queue is full.
  if (histo_queue->size == histo_queue->max_size) return 0;
  assert(threshold <= 0);
  if (idx1 > idx2) {
    const int tmp = idx2;
    idx2 = idx1;
    idx1 = tmp;
  }
  pair.idx1 = idx1;
  pair.idx2 = idx2;
  h1 = histograms[idx1];
  h2 = histograms[idx2];

  // Do not even consider the pair if it does not improve the entropy.
  if (!HistoQueueUpdatePair(h1, h2, threshold, &pair)) return 0;

  histo_queue->queue[histo_queue->size++] = pair;
  HistoQueueUpdateHead(histo_queue, &histo_queue->queue[histo_queue->size - 1]);

  return pair.cost_diff;
}

// -----------------------------------------------------------------------------

// Combines histograms by continuously choosing the one with the highest cost
// reduction.
static int HistogramCombineGreedy(VP8LHistogramSet* const image_histo,
                                  int* const num_used) {
  int ok = 0;
  const int image_histo_size = image_histo->size;
  int i, j;
  VP8LHistogram** const histograms = image_histo->histograms;
  // Priority queue of histogram pairs.
  HistoQueue histo_queue;

  // image_histo_size^2 for the queue size is safe. If you look at
  // HistogramCombineGreedy, and imagine that UpdateQueueFront always pushes
  // data to the queue, you insert at most:
  // - image_histo_size*(image_histo_size-1)/2 (the first two for loops)
  // - image_histo_size - 1 in the last for loop at the first iteration of
  //   the while loop, image_histo_size - 2 at the second iteration ...
  //   therefore image_histo_size*(image_histo_size-1)/2 overall too
  if (!HistoQueueInit(&histo_queue, image_histo_size * image_histo_size)) {
    goto End;
  }

  for (i = 0; i < image_histo_size; ++i) {
    if (image_histo->histograms[i] == NULL) continue;
    for (j = i + 1; j < image_histo_size; ++j) {
      // Initialize queue.
      if (image_histo->histograms[j] == NULL) continue;
      HistoQueuePush(&histo_queue, histograms, i, j, 0);
    }
  }

  while (histo_queue.size > 0) {
    const int idx1 = histo_queue.queue[0].idx1;
    const int idx2 = histo_queue.queue[0].idx2;
    HistogramAdd(histograms[idx2], histograms[idx1], histograms[idx1]);
    histograms[idx1]->bit_cost_ = histo_queue.queue[0].cost_combo;

    // Remove merged histogram.
    HistogramSetRemoveHistogram(image_histo, idx2, num_used);

    // Remove pairs intersecting the just combined best pair.
    for (i = 0; i < histo_queue.size;) {
      HistogramPair* const p = histo_queue.queue + i;
      if (p->idx1 == idx1 || p->idx2 == idx1 ||
          p->idx1 == idx2 || p->idx2 == idx2) {
        HistoQueuePopPair(&histo_queue, p);
      } else {
        HistoQueueUpdateHead(&histo_queue, p);
        ++i;
      }
    }

    // Push new pairs formed with combined histogram to the queue.
    for (i = 0; i < image_histo->size; ++i) {
      if (i == idx1 || image_histo->histograms[i] == NULL) continue;
      HistoQueuePush(&histo_queue, image_histo->histograms, idx1, i, 0);
    }
  }

  ok = 1;

 End:
  HistoQueueClear(&histo_queue);
  return ok;
}

// Perform histogram aggregation using a stochastic approach.
// 'do_greedy' is set to 1 if a greedy approach needs to be performed
// afterwards, 0 otherwise.
static int PairComparison(const void* idx1, const void* idx2) {
  // To be used with bsearch: <0 when *idx1<*idx2, >0 if >, 0 when ==.
  return (*(int*) idx1 - *(int*) idx2);
}
static int HistogramCombineStochastic(VP8LHistogramSet* const image_histo,
                                      int* const num_used, int min_cluster_size,
                                      int* const do_greedy) {
  int j, iter;
  uint32_t seed = 1;
  int tries_with_no_success = 0;
  const int outer_iters = *num_used;
  const int num_tries_no_success = outer_iters / 2;
  VP8LHistogram** const histograms = image_histo->histograms;
  // Priority queue of histogram pairs. Its size of 'kHistoQueueSize'
  // impacts the quality of the compression and the speed: the smaller the
  // faster but the worse for the compression.
  HistoQueue histo_queue;
  const int kHistoQueueSize = 9;
  int ok = 0;
  // mapping from an index in image_histo with no NULL histogram to the full
  // blown image_histo.
  int* mappings;

  if (*num_used < min_cluster_size) {
    *do_greedy = 1;
    return 1;
  }

  mappings = (int*) WebPSafeMalloc(*num_used, sizeof(*mappings));
  if (mappings == NULL) return 0;
  if (!HistoQueueInit(&histo_queue, kHistoQueueSize)) goto End;
  // Fill the initial mapping.
  for (j = 0, iter = 0; iter < image_histo->size; ++iter) {
    if (histograms[iter] == NULL) continue;
    mappings[j++] = iter;
  }
  assert(j == *num_used);

  // Collapse similar histograms in 'image_histo'.
  for (iter = 0;
       iter < outer_iters && *num_used >= min_cluster_size &&
           ++tries_with_no_success < num_tries_no_success;
       ++iter) {
    int* mapping_index;
    int64_t best_cost =
        (histo_queue.size == 0) ? 0 : histo_queue.queue[0].cost_diff;
    int best_idx1 = -1, best_idx2 = 1;
    const uint32_t rand_range = (*num_used - 1) * (*num_used);
    // (*num_used) / 2 was chosen empirically. Less means faster but worse
    // compression.
    const int num_tries = (*num_used) / 2;

    // Pick random samples.
    for (j = 0; *num_used >= 2 && j < num_tries; ++j) {
      int64_t curr_cost;
      // Choose two different histograms at random and try to combine them.
      const uint32_t tmp = MyRand(&seed) % rand_range;
      uint32_t idx1 = tmp / (*num_used - 1);
      uint32_t idx2 = tmp % (*num_used - 1);
      if (idx2 >= idx1) ++idx2;
      idx1 = mappings[idx1];
      idx2 = mappings[idx2];

      // Calculate cost reduction on combination.
      curr_cost =
          HistoQueuePush(&histo_queue, histograms, idx1, idx2, best_cost);
      if (curr_cost < 0) {  // found a better pair?
        best_cost = curr_cost;
        // Empty the queue if we reached full capacity.
        if (histo_queue.size == histo_queue.max_size) break;
      }
    }
    if (histo_queue.size == 0) continue;

    // Get the best histograms.
    best_idx1 = histo_queue.queue[0].idx1;
    best_idx2 = histo_queue.queue[0].idx2;
    assert(best_idx1 < best_idx2);
    // Pop best_idx2 from mappings.
    mapping_index = (int*) bsearch(&best_idx2, mappings, *num_used,
                                   sizeof(best_idx2), &PairComparison);
    assert(mapping_index != NULL);
    memmove(mapping_index, mapping_index + 1, sizeof(*mapping_index) *
        ((*num_used) - (mapping_index - mappings) - 1));
    // Merge the histograms and remove best_idx2 from the queue.
    HistogramAdd(histograms[best_idx2], histograms[best_idx1],
                 histograms[best_idx1]);
    histograms[best_idx1]->bit_cost_ = histo_queue.queue[0].cost_combo;
    HistogramSetRemoveHistogram(image_histo, best_idx2, num_used);
    // Parse the queue and update each pair that deals with best_idx1,
    // best_idx2 or image_histo_size.
    for (j = 0; j < histo_queue.size;) {
      HistogramPair* const p = histo_queue.queue + j;
      const int is_idx1_best = p->idx1 == best_idx1 || p->idx1 == best_idx2;
      const int is_idx2_best = p->idx2 == best_idx1 || p->idx2 == best_idx2;
      int do_eval = 0;
      // The front pair could have been duplicated by a random pick so
      // check for it all the time nevertheless.
      if (is_idx1_best && is_idx2_best) {
        HistoQueuePopPair(&histo_queue, p);
        continue;
      }
      // Any pair containing one of the two best indices should only refer to
      // best_idx1. Its cost should also be updated.
      if (is_idx1_best) {
        p->idx1 = best_idx1;
        do_eval = 1;
      } else if (is_idx2_best) {
        p->idx2 = best_idx1;
        do_eval = 1;
      }
      // Make sure the index order is respected.
      if (p->idx1 > p->idx2) {
        const int tmp = p->idx2;
        p->idx2 = p->idx1;
        p->idx1 = tmp;
      }
      if (do_eval) {
        // Re-evaluate the cost of an updated pair.
        if (!HistoQueueUpdatePair(histograms[p->idx1], histograms[p->idx2], 0,
                                  p)) {
          HistoQueuePopPair(&histo_queue, p);
          continue;
        }
      }
      HistoQueueUpdateHead(&histo_queue, p);
      ++j;
    }
    tries_with_no_success = 0;
  }
  *do_greedy = (*num_used <= min_cluster_size);
  ok = 1;

 End:
  HistoQueueClear(&histo_queue);
  WebPSafeFree(mappings);
  return ok;
}

// -----------------------------------------------------------------------------
// Histogram refinement

// Find the best 'out' histogram for each of the 'in' histograms.
// At call-time, 'out' contains the histograms of the clusters.
// Note: we assume that out[]->bit_cost_ is already up-to-date.
static void HistogramRemap(const VP8LHistogramSet* const in,
                           VP8LHistogramSet* const out,
                           uint32_t* const symbols) {
  int i;
  VP8LHistogram** const in_histo = in->histograms;
  VP8LHistogram** const out_histo = out->histograms;
  const int in_size = out->max_size;
  const int out_size = out->size;
  if (out_size > 1) {
    for (i = 0; i < in_size; ++i) {
      int best_out = 0;
      int64_t best_bits = WEBP_INT64_MAX;
      int k;
      if (in_histo[i] == NULL) {
        // Arbitrarily set to the previous value if unused to help future LZ77.
        symbols[i] = symbols[i - 1];
        continue;
      }
      for (k = 0; k < out_size; ++k) {
        int64_t cur_bits;
        if (HistogramAddThresh(out_histo[k], in_histo[i], best_bits,
                               &cur_bits)) {
          best_bits = cur_bits;
          best_out = k;
        }
      }
      symbols[i] = best_out;
    }
  } else {
    assert(out_size == 1);
    for (i = 0; i < in_size; ++i) {
      symbols[i] = 0;
    }
  }

  // Recompute each out based on raw and symbols.
  VP8LHistogramSetClear(out);
  out->size = out_size;

  for (i = 0; i < in_size; ++i) {
    int idx;
    if (in_histo[i] == NULL) continue;
    idx = symbols[i];
    HistogramAdd(in_histo[i], out_histo[idx], out_histo[idx]);
  }
}

static int32_t GetCombineCostFactor(int histo_size, int quality) {
  int32_t combine_cost_factor = 16;
  if (quality < 90) {
    if (histo_size > 256) combine_cost_factor /= 2;
    if (histo_size > 512) combine_cost_factor /= 2;
    if (histo_size > 1024) combine_cost_factor /= 2;
    if (quality <= 50) combine_cost_factor /= 2;
  }
  return combine_cost_factor;
}

// Given a HistogramSet 'set', the mapping of clusters 'cluster_mapping' and the
// current assignment of the cells in 'symbols', merge the clusters and
// assign the smallest possible clusters values.
static void OptimizeHistogramSymbols(const VP8LHistogramSet* const set,
                                     uint16_t* const cluster_mappings,
                                     uint32_t num_clusters,
                                     uint16_t* const cluster_mappings_tmp,
                                     uint32_t* const symbols) {
  uint32_t i, cluster_max;
  int do_continue = 1;
  // First, assign the lowest cluster to each pixel.
  while (do_continue) {
    do_continue = 0;
    for (i = 0; i < num_clusters; ++i) {
      int k;
      k = cluster_mappings[i];
      while (k != cluster_mappings[k]) {
        cluster_mappings[k] = cluster_mappings[cluster_mappings[k]];
        k = cluster_mappings[k];
      }
      if (k != cluster_mappings[i]) {
        do_continue = 1;
        cluster_mappings[i] = k;
      }
    }
  }
  // Create a mapping from a cluster id to its minimal version.
  cluster_max = 0;
  memset(cluster_mappings_tmp, 0,
         set->max_size * sizeof(*cluster_mappings_tmp));
  assert(cluster_mappings[0] == 0);
  // Re-map the ids.
  for (i = 0; i < (uint32_t)set->max_size; ++i) {
    int cluster;
    if (symbols[i] == kInvalidHistogramSymbol) continue;
    cluster = cluster_mappings[symbols[i]];
    assert(symbols[i] < num_clusters);
    if (cluster > 0 && cluster_mappings_tmp[cluster] == 0) {
      ++cluster_max;
      cluster_mappings_tmp[cluster] = cluster_max;
    }
    symbols[i] = cluster_mappings_tmp[cluster];
  }

  // Make sure all cluster values are used.
  cluster_max = 0;
  for (i = 0; i < (uint32_t)set->max_size; ++i) {
    if (symbols[i] == kInvalidHistogramSymbol) continue;
    if (symbols[i] <= cluster_max) continue;
    ++cluster_max;
    assert(symbols[i] == cluster_max);
  }
}

static void RemoveEmptyHistograms(VP8LHistogramSet* const image_histo) {
  uint32_t size;
  int i;
  for (i = 0, size = 0; i < image_histo->size; ++i) {
    if (image_histo->histograms[i] == NULL) continue;
    image_histo->histograms[size++] = image_histo->histograms[i];
  }
  image_histo->size = size;
}

int VP8LGetHistoImageSymbols(int xsize, int ysize,
                             const VP8LBackwardRefs* const refs, int quality,
                             int low_effort, int histogram_bits, int cache_bits,
                             VP8LHistogramSet* const image_histo,
                             VP8LHistogram* const tmp_histo,
                             uint32_t* const histogram_symbols,
                             const WebPPicture* const pic, int percent_range,
                             int* const percent) {
  const int histo_xsize =
      histogram_bits ? VP8LSubSampleSize(xsize, histogram_bits) : 1;
  const int histo_ysize =
      histogram_bits ? VP8LSubSampleSize(ysize, histogram_bits) : 1;
  const int image_histo_raw_size = histo_xsize * histo_ysize;
  VP8LHistogramSet* const orig_histo =
      VP8LAllocateHistogramSet(image_histo_raw_size, cache_bits);
  // Don't attempt linear bin-partition heuristic for
  // histograms of small sizes (as bin_map will be very sparse) and
  // maximum quality q==100 (to preserve the compression gains at that level).
  const int entropy_combine_num_bins = low_effort ? NUM_PARTITIONS : BIN_SIZE;
  int entropy_combine;
  uint16_t* const map_tmp =
      (uint16_t*)WebPSafeMalloc(2 * image_histo_raw_size, sizeof(*map_tmp));
  uint16_t* const cluster_mappings = map_tmp + image_histo_raw_size;
  int num_used = image_histo_raw_size;
  if (orig_histo == NULL || map_tmp == NULL) {
    WebPEncodingSetError(pic, VP8_ENC_ERROR_OUT_OF_MEMORY);
    goto Error;
  }

  // Construct the histograms from backward references.
  HistogramBuild(xsize, histogram_bits, refs, orig_histo);
  // Copies the histograms and computes its bit_cost.
  // histogram_symbols is optimized
  HistogramCopyAndAnalyze(orig_histo, image_histo, &num_used,
                          histogram_symbols);

  entropy_combine =
      (num_used > entropy_combine_num_bins * 2) && (quality < 100);

  if (entropy_combine) {
    uint16_t* const bin_map = map_tmp;
    const int32_t combine_cost_factor =
        GetCombineCostFactor(image_histo_raw_size, quality);
    const uint32_t num_clusters = num_used;

    HistogramAnalyzeEntropyBin(image_histo, bin_map, low_effort);
    // Collapse histograms with similar entropy.
    HistogramCombineEntropyBin(
        image_histo, &num_used, histogram_symbols, cluster_mappings, tmp_histo,
        bin_map, entropy_combine_num_bins, combine_cost_factor, low_effort);
    OptimizeHistogramSymbols(image_histo, cluster_mappings, num_clusters,
                             map_tmp, histogram_symbols);
  }

  // Don't combine the histograms using stochastic and greedy heuristics for
  // low-effort compression mode.
  if (!low_effort || !entropy_combine) {
    // cubic ramp between 1 and MAX_HISTO_GREEDY:
    const int threshold_size =
        (int)(1 + DivRound(quality * quality * quality * (MAX_HISTO_GREEDY - 1),
                           100 * 100 * 100));
    int do_greedy;
    if (!HistogramCombineStochastic(image_histo, &num_used, threshold_size,
                                    &do_greedy)) {
      WebPEncodingSetError(pic, VP8_ENC_ERROR_OUT_OF_MEMORY);
      goto Error;
    }
    if (do_greedy) {
      RemoveEmptyHistograms(image_histo);
      if (!HistogramCombineGreedy(image_histo, &num_used)) {
        WebPEncodingSetError(pic, VP8_ENC_ERROR_OUT_OF_MEMORY);
        goto Error;
      }
    }
  }

  // Find the optimal map from original histograms to the final ones.
  RemoveEmptyHistograms(image_histo);
  HistogramRemap(orig_histo, image_histo, histogram_symbols);

  if (!WebPReportProgress(pic, *percent + percent_range, percent)) {
    goto Error;
  }

 Error:
  VP8LFreeHistogramSet(orig_histo);
  WebPSafeFree(map_tmp);
  return (pic->error_code == VP8_ENC_OK);
}
