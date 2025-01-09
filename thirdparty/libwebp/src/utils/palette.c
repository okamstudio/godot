// Copyright 2023 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Utilities for palette analysis.
//
// Author: Vincent Rabaud (vrabaud@google.com)

#include "src/utils/palette.h"

#include <assert.h>
#include <stdlib.h>

#include "src/dsp/lossless_common.h"
#include "src/utils/color_cache_utils.h"
#include "src/utils/utils.h"
#include "src/webp/encode.h"
#include "src/webp/format_constants.h"

// -----------------------------------------------------------------------------

// Palette reordering for smaller sum of deltas (and for smaller storage).

static int PaletteCompareColorsForQsort(const void* p1, const void* p2) {
  const uint32_t a = WebPMemToUint32((uint8_t*)p1);
  const uint32_t b = WebPMemToUint32((uint8_t*)p2);
  assert(a != b);
  return (a < b) ? -1 : 1;
}

static WEBP_INLINE uint32_t PaletteComponentDistance(uint32_t v) {
  return (v <= 128) ? v : (256 - v);
}

// Computes a value that is related to the entropy created by the
// palette entry diff.
//
// Note that the last & 0xff is a no-operation in the next statement, but
// removed by most compilers and is here only for regularity of the code.
static WEBP_INLINE uint32_t PaletteColorDistance(uint32_t col1, uint32_t col2) {
  const uint32_t diff = VP8LSubPixels(col1, col2);
  const int kMoreWeightForRGBThanForAlpha = 9;
  uint32_t score;
  score = PaletteComponentDistance((diff >> 0) & 0xff);
  score += PaletteComponentDistance((diff >> 8) & 0xff);
  score += PaletteComponentDistance((diff >> 16) & 0xff);
  score *= kMoreWeightForRGBThanForAlpha;
  score += PaletteComponentDistance((diff >> 24) & 0xff);
  return score;
}

static WEBP_INLINE void SwapColor(uint32_t* const col1, uint32_t* const col2) {
  const uint32_t tmp = *col1;
  *col1 = *col2;
  *col2 = tmp;
}

int SearchColorNoIdx(const uint32_t sorted[], uint32_t color, int num_colors) {
  int low = 0, hi = num_colors;
  if (sorted[low] == color) return low;  // loop invariant: sorted[low] != color
  while (1) {
    const int mid = (low + hi) >> 1;
    if (sorted[mid] == color) {
      return mid;
    } else if (sorted[mid] < color) {
      low = mid;
    } else {
      hi = mid;
    }
  }
  assert(0);
  return 0;
}

void PrepareMapToPalette(const uint32_t palette[], uint32_t num_colors,
                         uint32_t sorted[], uint32_t idx_map[]) {
  uint32_t i;
  memcpy(sorted, palette, num_colors * sizeof(*sorted));
  qsort(sorted, num_colors, sizeof(*sorted), PaletteCompareColorsForQsort);
  for (i = 0; i < num_colors; ++i) {
    idx_map[SearchColorNoIdx(sorted, palette[i], num_colors)] = i;
  }
}

//------------------------------------------------------------------------------

#define COLOR_HASH_SIZE (MAX_PALETTE_SIZE * 4)
#define COLOR_HASH_RIGHT_SHIFT 22  // 32 - log2(COLOR_HASH_SIZE).

int GetColorPalette(const WebPPicture* const pic, uint32_t* const palette) {
  int i;
  int x, y;
  int num_colors = 0;
  uint8_t in_use[COLOR_HASH_SIZE] = {0};
  uint32_t colors[COLOR_HASH_SIZE] = {0};
  const uint32_t* argb = pic->argb;
  const int width = pic->width;
  const int height = pic->height;
  uint32_t last_pix = ~argb[0];  // so we're sure that last_pix != argb[0]
  assert(pic != NULL);
  assert(pic->use_argb);

  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      int key;
      if (argb[x] == last_pix) {
        continue;
      }
      last_pix = argb[x];
      key = VP8LHashPix(last_pix, COLOR_HASH_RIGHT_SHIFT);
      while (1) {
        if (!in_use[key]) {
          colors[key] = last_pix;
          in_use[key] = 1;
          ++num_colors;
          if (num_colors > MAX_PALETTE_SIZE) {
            return MAX_PALETTE_SIZE + 1;  // Exact count not needed.
          }
          break;
        } else if (colors[key] == last_pix) {
          break;  // The color is already there.
        } else {
          // Some other color sits here, so do linear conflict resolution.
          ++key;
          key &= (COLOR_HASH_SIZE - 1);  // Key mask.
        }
      }
    }
    argb += pic->argb_stride;
  }

  if (palette != NULL) {  // Fill the colors into palette.
    num_colors = 0;
    for (i = 0; i < COLOR_HASH_SIZE; ++i) {
      if (in_use[i]) {
        palette[num_colors] = colors[i];
        ++num_colors;
      }
    }
    qsort(palette, num_colors, sizeof(*palette), PaletteCompareColorsForQsort);
  }
  return num_colors;
}

#undef COLOR_HASH_SIZE
#undef COLOR_HASH_RIGHT_SHIFT

// -----------------------------------------------------------------------------

// The palette has been sorted by alpha. This function checks if the other
// components of the palette have a monotonic development with regards to
// position in the palette. If all have monotonic development, there is
// no benefit to re-organize them greedily. A monotonic development
// would be spotted in green-only situations (like lossy alpha) or gray-scale
// images.
static int PaletteHasNonMonotonousDeltas(const uint32_t* const palette,
                                         int num_colors) {
  uint32_t predict = 0x000000;
  int i;
  uint8_t sign_found = 0x00;
  for (i = 0; i < num_colors; ++i) {
    const uint32_t diff = VP8LSubPixels(palette[i], predict);
    const uint8_t rd = (diff >> 16) & 0xff;
    const uint8_t gd = (diff >> 8) & 0xff;
    const uint8_t bd = (diff >> 0) & 0xff;
    if (rd != 0x00) {
      sign_found |= (rd < 0x80) ? 1 : 2;
    }
    if (gd != 0x00) {
      sign_found |= (gd < 0x80) ? 8 : 16;
    }
    if (bd != 0x00) {
      sign_found |= (bd < 0x80) ? 64 : 128;
    }
    predict = palette[i];
  }
  return (sign_found & (sign_found << 1)) != 0;  // two consequent signs.
}

static void PaletteSortMinimizeDeltas(const uint32_t* const palette_sorted,
                                      int num_colors, uint32_t* const palette) {
  uint32_t predict = 0x00000000;
  int i, k;
  memcpy(palette, palette_sorted, num_colors * sizeof(*palette));
  if (!PaletteHasNonMonotonousDeltas(palette_sorted, num_colors)) return;
  // Find greedily always the closest color of the predicted color to minimize
  // deltas in the palette. This reduces storage needs since the
  // palette is stored with delta encoding.
  if (num_colors > 17) {
    if (palette[0] == 0) {
      --num_colors;
      SwapColor(&palette[num_colors], &palette[0]);
    }
  }
  for (i = 0; i < num_colors; ++i) {
    int best_ix = i;
    uint32_t best_score = ~0U;
    for (k = i; k < num_colors; ++k) {
      const uint32_t cur_score = PaletteColorDistance(palette[k], predict);
      if (best_score > cur_score) {
        best_score = cur_score;
        best_ix = k;
      }
    }
    SwapColor(&palette[best_ix], &palette[i]);
    predict = palette[i];
  }
}

// -----------------------------------------------------------------------------
// Modified Zeng method from "A Survey on Palette Reordering
// Methods for Improving the Compression of Color-Indexed Images" by Armando J.
// Pinho and Antonio J. R. Neves.

// Finds the biggest cooccurrence in the matrix.
static void CoOccurrenceFindMax(const uint32_t* const cooccurrence,
                                uint32_t num_colors, uint8_t* const c1,
                                uint8_t* const c2) {
  // Find the index that is most frequently located adjacent to other
  // (different) indexes.
  uint32_t best_sum = 0u;
  uint32_t i, j, best_cooccurrence;
  *c1 = 0u;
  for (i = 0; i < num_colors; ++i) {
    uint32_t sum = 0;
    for (j = 0; j < num_colors; ++j) sum += cooccurrence[i * num_colors + j];
    if (sum > best_sum) {
      best_sum = sum;
      *c1 = i;
    }
  }
  // Find the index that is most frequently found adjacent to *c1.
  *c2 = 0u;
  best_cooccurrence = 0u;
  for (i = 0; i < num_colors; ++i) {
    if (cooccurrence[*c1 * num_colors + i] > best_cooccurrence) {
      best_cooccurrence = cooccurrence[*c1 * num_colors + i];
      *c2 = i;
    }
  }
  assert(*c1 != *c2);
}

// Builds the cooccurrence matrix
static int CoOccurrenceBuild(const WebPPicture* const pic,
                             const uint32_t* const palette, uint32_t num_colors,
                             uint32_t* cooccurrence) {
  uint32_t *lines, *line_top, *line_current, *line_tmp;
  int x, y;
  const uint32_t* src = pic->argb;
  uint32_t prev_pix = ~src[0];
  uint32_t prev_idx = 0u;
  uint32_t idx_map[MAX_PALETTE_SIZE] = {0};
  uint32_t palette_sorted[MAX_PALETTE_SIZE];
  lines = (uint32_t*)WebPSafeMalloc(2 * pic->width, sizeof(*lines));
  if (lines == NULL) {
    return 0;
  }
  line_top = &lines[0];
  line_current = &lines[pic->width];
  PrepareMapToPalette(palette, num_colors, palette_sorted, idx_map);
  for (y = 0; y < pic->height; ++y) {
    for (x = 0; x < pic->width; ++x) {
      const uint32_t pix = src[x];
      if (pix != prev_pix) {
        prev_idx = idx_map[SearchColorNoIdx(palette_sorted, pix, num_colors)];
        prev_pix = pix;
      }
      line_current[x] = prev_idx;
      // 4-connectivity is what works best as mentioned in "On the relation
      // between Memon's and the modified Zeng's palette reordering methods".
      if (x > 0 && prev_idx != line_current[x - 1]) {
        const uint32_t left_idx = line_current[x - 1];
        ++cooccurrence[prev_idx * num_colors + left_idx];
        ++cooccurrence[left_idx * num_colors + prev_idx];
      }
      if (y > 0 && prev_idx != line_top[x]) {
        const uint32_t top_idx = line_top[x];
        ++cooccurrence[prev_idx * num_colors + top_idx];
        ++cooccurrence[top_idx * num_colors + prev_idx];
      }
    }
    line_tmp = line_top;
    line_top = line_current;
    line_current = line_tmp;
    src += pic->argb_stride;
  }
  WebPSafeFree(lines);
  return 1;
}

struct Sum {
  uint8_t index;
  uint32_t sum;
};

static int PaletteSortModifiedZeng(const WebPPicture* const pic,
                                   const uint32_t* const palette_in,
                                   uint32_t num_colors,
                                   uint32_t* const palette) {
  uint32_t i, j, ind;
  uint8_t remapping[MAX_PALETTE_SIZE];
  uint32_t* cooccurrence;
  struct Sum sums[MAX_PALETTE_SIZE];
  uint32_t first, last;
  uint32_t num_sums;
  // TODO(vrabaud) check whether one color images should use palette or not.
  if (num_colors <= 1) return 1;
  // Build the co-occurrence matrix.
  cooccurrence =
      (uint32_t*)WebPSafeCalloc(num_colors * num_colors, sizeof(*cooccurrence));
  if (cooccurrence == NULL) {
    return 0;
  }
  if (!CoOccurrenceBuild(pic, palette_in, num_colors, cooccurrence)) {
    WebPSafeFree(cooccurrence);
    return 0;
  }

  // Initialize the mapping list with the two best indices.
  CoOccurrenceFindMax(cooccurrence, num_colors, &remapping[0], &remapping[1]);

  // We need to append and prepend to the list of remapping. To this end, we
  // actually define the next start/end of the list as indices in a vector (with
  // a wrap around when the end is reached).
  first = 0;
  last = 1;
  num_sums = num_colors - 2;  // -2 because we know the first two values
  if (num_sums > 0) {
    // Initialize the sums with the first two remappings and find the best one
    struct Sum* best_sum = &sums[0];
    best_sum->index = 0u;
    best_sum->sum = 0u;
    for (i = 0, j = 0; i < num_colors; ++i) {
      if (i == remapping[0] || i == remapping[1]) continue;
      sums[j].index = i;
      sums[j].sum = cooccurrence[i * num_colors + remapping[0]] +
                    cooccurrence[i * num_colors + remapping[1]];
      if (sums[j].sum > best_sum->sum) best_sum = &sums[j];
      ++j;
    }

    while (num_sums > 0) {
      const uint8_t best_index = best_sum->index;
      // Compute delta to know if we need to prepend or append the best index.
      int32_t delta = 0;
      const int32_t n = num_colors - num_sums;
      for (ind = first, j = 0; (ind + j) % num_colors != last + 1; ++j) {
        const uint16_t l_j = remapping[(ind + j) % num_colors];
        delta += (n - 1 - 2 * (int32_t)j) *
                 (int32_t)cooccurrence[best_index * num_colors + l_j];
      }
      if (delta > 0) {
        first = (first == 0) ? num_colors - 1 : first - 1;
        remapping[first] = best_index;
      } else {
        ++last;
        remapping[last] = best_index;
      }
      // Remove best_sum from sums.
      *best_sum = sums[num_sums - 1];
      --num_sums;
      // Update all the sums and find the best one.
      best_sum = &sums[0];
      for (i = 0; i < num_sums; ++i) {
        sums[i].sum += cooccurrence[best_index * num_colors + sums[i].index];
        if (sums[i].sum > best_sum->sum) best_sum = &sums[i];
      }
    }
  }
  assert((last + 1) % num_colors == first);
  WebPSafeFree(cooccurrence);

  // Re-map the palette.
  for (i = 0; i < num_colors; ++i) {
    palette[i] = palette_in[remapping[(first + i) % num_colors]];
  }
  return 1;
}

// -----------------------------------------------------------------------------

int PaletteSort(PaletteSorting method, const struct WebPPicture* const pic,
                const uint32_t* const palette_sorted, uint32_t num_colors,
                uint32_t* const palette) {
  switch (method) {
    case kSortedDefault:
      if (palette_sorted[0] == 0 && num_colors > 17) {
        memcpy(palette, palette_sorted + 1,
               (num_colors - 1) * sizeof(*palette_sorted));
        palette[num_colors - 1] = 0;
      } else {
        memcpy(palette, palette_sorted, num_colors * sizeof(*palette));
      }
      return 1;
    case kMinimizeDelta:
      PaletteSortMinimizeDeltas(palette_sorted, num_colors, palette);
      return 1;
    case kModifiedZeng:
      return PaletteSortModifiedZeng(pic, palette_sorted, num_colors, palette);
    case kUnusedPalette:
    case kPaletteSortingNum:
      break;
  }

  assert(0);
  return 0;
}
