// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Cost tables for level and modes.
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef WEBP_ENC_COST_ENC_H_
#define WEBP_ENC_COST_ENC_H_

#include <assert.h>
#include <stdlib.h>
#include "src/enc/vp8i_enc.h"

#ifdef __cplusplus
extern "C" {
#endif

// On-the-fly info about the current set of residuals. Handy to avoid
// passing zillions of params.
typedef struct VP8Residual VP8Residual;
struct VP8Residual {
  int first;
  int last;
  const int16_t* coeffs;

  int coeff_type;
  ProbaArray*   prob;
  StatsArray*   stats;
  CostArrayPtr  costs;
};

void VP8InitResidual(int first, int coeff_type,
                     VP8Encoder* const enc, VP8Residual* const res);

int VP8RecordCoeffs(int ctx, const VP8Residual* const res);

// Record proba context used.
static WEBP_INLINE int VP8RecordStats(int bit, proba_t* const stats) {
  proba_t p = *stats;
  // An overflow is inbound. Note we handle this at 0xfffe0000u instead of
  // 0xffff0000u to make sure p + 1u does not overflow.
  if (p >= 0xfffe0000u) {
    p = ((p + 1u) >> 1) & 0x7fff7fffu;  // -> divide the stats by 2.
  }
  // record bit count (lower 16 bits) and increment total count (upper 16 bits).
  p += 0x00010000u + bit;
  *stats = p;
  return bit;
}

// Cost of coding one event with probability 'proba'.
static WEBP_INLINE int VP8BitCost(int bit, uint8_t proba) {
  return !bit ? VP8EntropyCost[proba] : VP8EntropyCost[255 - proba];
}

// Level cost calculations
void VP8CalculateLevelCosts(VP8EncProba* const proba);
static WEBP_INLINE int VP8LevelCost(const uint16_t* const table, int level) {
  return VP8LevelFixedCosts[level]
       + table[(level > MAX_VARIABLE_LEVEL) ? MAX_VARIABLE_LEVEL : level];
}

// Mode costs
extern const uint16_t VP8FixedCostsUV[4];
extern const uint16_t VP8FixedCostsI16[4];
extern const uint16_t VP8FixedCostsI4[NUM_BMODES][NUM_BMODES][NUM_BMODES];

//------------------------------------------------------------------------------

#ifdef __cplusplus
}    // extern "C"
#endif

#endif  // WEBP_ENC_COST_ENC_H_
