// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//   Quantization
//
// Author: Skal (pascal.massimino@gmail.com)

#include <assert.h>
#include <math.h>
#include <stdlib.h>  // for abs()

#include "src/dsp/quant.h"
#include "src/enc/vp8i_enc.h"
#include "src/enc/cost_enc.h"

#define DO_TRELLIS_I4  1
#define DO_TRELLIS_I16 1   // not a huge gain, but ok at low bitrate.
#define DO_TRELLIS_UV  0   // disable trellis for UV. Risky. Not worth.
#define USE_TDISTO 1

#define MID_ALPHA 64      // neutral value for susceptibility
#define MIN_ALPHA 30      // lowest usable value for susceptibility
#define MAX_ALPHA 100     // higher meaningful value for susceptibility

#define SNS_TO_DQ 0.9     // Scaling constant between the sns value and the QP
                          // power-law modulation. Must be strictly less than 1.

// number of non-zero coeffs below which we consider the block very flat
// (and apply a penalty to complex predictions)
#define FLATNESS_LIMIT_I16 0       // I16 mode (special case)
#define FLATNESS_LIMIT_I4  3       // I4 mode
#define FLATNESS_LIMIT_UV  2       // UV mode
#define FLATNESS_PENALTY   140     // roughly ~1bit per block

#define MULT_8B(a, b) (((a) * (b) + 128) >> 8)

#define RD_DISTO_MULT      256  // distortion multiplier (equivalent of lambda)

// #define DEBUG_BLOCK

//------------------------------------------------------------------------------

#if defined(DEBUG_BLOCK)

#include <stdio.h>
#include <stdlib.h>

static void PrintBlockInfo(const VP8EncIterator* const it,
                           const VP8ModeScore* const rd) {
  int i, j;
  const int is_i16 = (it->mb_->type_ == 1);
  const uint8_t* const y_in = it->yuv_in_ + Y_OFF_ENC;
  const uint8_t* const y_out = it->yuv_out_ + Y_OFF_ENC;
  const uint8_t* const uv_in = it->yuv_in_ + U_OFF_ENC;
  const uint8_t* const uv_out = it->yuv_out_ + U_OFF_ENC;
  printf("SOURCE / OUTPUT / ABS DELTA\n");
  for (j = 0; j < 16; ++j) {
    for (i = 0; i < 16; ++i) printf("%3d ", y_in[i + j * BPS]);
    printf("     ");
    for (i = 0; i < 16; ++i) printf("%3d ", y_out[i + j * BPS]);
    printf("     ");
    for (i = 0; i < 16; ++i) {
      printf("%1d ", abs(y_in[i + j * BPS] - y_out[i + j * BPS]));
    }
    printf("\n");
  }
  printf("\n");   // newline before the U/V block
  for (j = 0; j < 8; ++j) {
    for (i = 0; i < 8; ++i) printf("%3d ", uv_in[i + j * BPS]);
    printf(" ");
    for (i = 8; i < 16; ++i) printf("%3d ", uv_in[i + j * BPS]);
    printf("    ");
    for (i = 0; i < 8; ++i) printf("%3d ", uv_out[i + j * BPS]);
    printf(" ");
    for (i = 8; i < 16; ++i) printf("%3d ", uv_out[i + j * BPS]);
    printf("   ");
    for (i = 0; i < 8; ++i) {
      printf("%1d ", abs(uv_out[i + j * BPS] - uv_in[i + j * BPS]));
    }
    printf(" ");
    for (i = 8; i < 16; ++i) {
      printf("%1d ", abs(uv_out[i + j * BPS] - uv_in[i + j * BPS]));
    }
    printf("\n");
  }
  printf("\nD:%d SD:%d R:%d H:%d nz:0x%x score:%d\n",
    (int)rd->D, (int)rd->SD, (int)rd->R, (int)rd->H, (int)rd->nz,
    (int)rd->score);
  if (is_i16) {
    printf("Mode: %d\n", rd->mode_i16);
    printf("y_dc_levels:");
    for (i = 0; i < 16; ++i) printf("%3d ", rd->y_dc_levels[i]);
    printf("\n");
  } else {
    printf("Modes[16]: ");
    for (i = 0; i < 16; ++i) printf("%d ", rd->modes_i4[i]);
    printf("\n");
  }
  printf("y_ac_levels:\n");
  for (j = 0; j < 16; ++j) {
    for (i = is_i16 ? 1 : 0; i < 16; ++i) {
      printf("%4d ", rd->y_ac_levels[j][i]);
    }
    printf("\n");
  }
  printf("\n");
  printf("uv_levels (mode=%d):\n", rd->mode_uv);
  for (j = 0; j < 8; ++j) {
    for (i = 0; i < 16; ++i) {
      printf("%4d ", rd->uv_levels[j][i]);
    }
    printf("\n");
  }
}

#endif   // DEBUG_BLOCK

//------------------------------------------------------------------------------

static WEBP_INLINE int clip(int v, int m, int M) {
  return v < m ? m : v > M ? M : v;
}

static const uint8_t kZigzag[16] = {
  0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};

static const uint8_t kDcTable[128] = {
  4,     5,   6,   7,   8,   9,  10,  10,
  11,   12,  13,  14,  15,  16,  17,  17,
  18,   19,  20,  20,  21,  21,  22,  22,
  23,   23,  24,  25,  25,  26,  27,  28,
  29,   30,  31,  32,  33,  34,  35,  36,
  37,   37,  38,  39,  40,  41,  42,  43,
  44,   45,  46,  46,  47,  48,  49,  50,
  51,   52,  53,  54,  55,  56,  57,  58,
  59,   60,  61,  62,  63,  64,  65,  66,
  67,   68,  69,  70,  71,  72,  73,  74,
  75,   76,  76,  77,  78,  79,  80,  81,
  82,   83,  84,  85,  86,  87,  88,  89,
  91,   93,  95,  96,  98, 100, 101, 102,
  104, 106, 108, 110, 112, 114, 116, 118,
  122, 124, 126, 128, 130, 132, 134, 136,
  138, 140, 143, 145, 148, 151, 154, 157
};

static const uint16_t kAcTable[128] = {
  4,     5,   6,   7,   8,   9,  10,  11,
  12,   13,  14,  15,  16,  17,  18,  19,
  20,   21,  22,  23,  24,  25,  26,  27,
  28,   29,  30,  31,  32,  33,  34,  35,
  36,   37,  38,  39,  40,  41,  42,  43,
  44,   45,  46,  47,  48,  49,  50,  51,
  52,   53,  54,  55,  56,  57,  58,  60,
  62,   64,  66,  68,  70,  72,  74,  76,
  78,   80,  82,  84,  86,  88,  90,  92,
  94,   96,  98, 100, 102, 104, 106, 108,
  110, 112, 114, 116, 119, 122, 125, 128,
  131, 134, 137, 140, 143, 146, 149, 152,
  155, 158, 161, 164, 167, 170, 173, 177,
  181, 185, 189, 193, 197, 201, 205, 209,
  213, 217, 221, 225, 229, 234, 239, 245,
  249, 254, 259, 264, 269, 274, 279, 284
};

static const uint16_t kAcTable2[128] = {
  8,     8,   9,  10,  12,  13,  15,  17,
  18,   20,  21,  23,  24,  26,  27,  29,
  31,   32,  34,  35,  37,  38,  40,  41,
  43,   44,  46,  48,  49,  51,  52,  54,
  55,   57,  58,  60,  62,  63,  65,  66,
  68,   69,  71,  72,  74,  75,  77,  79,
  80,   82,  83,  85,  86,  88,  89,  93,
  96,   99, 102, 105, 108, 111, 114, 117,
  120, 124, 127, 130, 133, 136, 139, 142,
  145, 148, 151, 155, 158, 161, 164, 167,
  170, 173, 176, 179, 184, 189, 193, 198,
  203, 207, 212, 217, 221, 226, 230, 235,
  240, 244, 249, 254, 258, 263, 268, 274,
  280, 286, 292, 299, 305, 311, 317, 323,
  330, 336, 342, 348, 354, 362, 370, 379,
  385, 393, 401, 409, 416, 424, 432, 440
};

static const uint8_t kBiasMatrices[3][2] = {  // [luma-ac,luma-dc,chroma][dc,ac]
  { 96, 110 }, { 96, 108 }, { 110, 115 }
};

// Sharpening by (slightly) raising the hi-frequency coeffs.
// Hack-ish but helpful for mid-bitrate range. Use with care.
#define SHARPEN_BITS 11  // number of descaling bits for sharpening bias
static const uint8_t kFreqSharpening[16] = {
  0,  30, 60, 90,
  30, 60, 90, 90,
  60, 90, 90, 90,
  90, 90, 90, 90
};

//------------------------------------------------------------------------------
// Initialize quantization parameters in VP8Matrix

// Returns the average quantizer
static int ExpandMatrix(VP8Matrix* const m, int type) {
  int i, sum;
  for (i = 0; i < 2; ++i) {
    const int is_ac_coeff = (i > 0);
    const int bias = kBiasMatrices[type][is_ac_coeff];
    m->iq_[i] = (1 << QFIX) / m->q_[i];
    m->bias_[i] = BIAS(bias);
    // zthresh_ is the exact value such that QUANTDIV(coeff, iQ, B) is:
    //   * zero if coeff <= zthresh
    //   * non-zero if coeff > zthresh
    m->zthresh_[i] = ((1 << QFIX) - 1 - m->bias_[i]) / m->iq_[i];
  }
  for (i = 2; i < 16; ++i) {
    m->q_[i] = m->q_[1];
    m->iq_[i] = m->iq_[1];
    m->bias_[i] = m->bias_[1];
    m->zthresh_[i] = m->zthresh_[1];
  }
  for (sum = 0, i = 0; i < 16; ++i) {
    if (type == 0) {  // we only use sharpening for AC luma coeffs
      m->sharpen_[i] = (kFreqSharpening[i] * m->q_[i]) >> SHARPEN_BITS;
    } else {
      m->sharpen_[i] = 0;
    }
    sum += m->q_[i];
  }
  return (sum + 8) >> 4;
}

static void CheckLambdaValue(int* const v) { if (*v < 1) *v = 1; }

static void SetupMatrices(VP8Encoder* enc) {
  int i;
  const int tlambda_scale =
    (enc->method_ >= 4) ? enc->config_->sns_strength
                        : 0;
  const int num_segments = enc->segment_hdr_.num_segments_;
  for (i = 0; i < num_segments; ++i) {
    VP8SegmentInfo* const m = &enc->dqm_[i];
    const int q = m->quant_;
    int q_i4, q_i16, q_uv;
    m->y1_.q_[0] = kDcTable[clip(q + enc->dq_y1_dc_, 0, 127)];
    m->y1_.q_[1] = kAcTable[clip(q,                  0, 127)];

    m->y2_.q_[0] = kDcTable[ clip(q + enc->dq_y2_dc_, 0, 127)] * 2;
    m->y2_.q_[1] = kAcTable2[clip(q + enc->dq_y2_ac_, 0, 127)];

    m->uv_.q_[0] = kDcTable[clip(q + enc->dq_uv_dc_, 0, 117)];
    m->uv_.q_[1] = kAcTable[clip(q + enc->dq_uv_ac_, 0, 127)];

    q_i4  = ExpandMatrix(&m->y1_, 0);
    q_i16 = ExpandMatrix(&m->y2_, 1);
    q_uv  = ExpandMatrix(&m->uv_, 2);

    m->lambda_i4_          = (3 * q_i4 * q_i4) >> 7;
    m->lambda_i16_         = (3 * q_i16 * q_i16);
    m->lambda_uv_          = (3 * q_uv * q_uv) >> 6;
    m->lambda_mode_        = (1 * q_i4 * q_i4) >> 7;
    m->lambda_trellis_i4_  = (7 * q_i4 * q_i4) >> 3;
    m->lambda_trellis_i16_ = (q_i16 * q_i16) >> 2;
    m->lambda_trellis_uv_  = (q_uv * q_uv) << 1;
    m->tlambda_            = (tlambda_scale * q_i4) >> 5;

    // none of these constants should be < 1
    CheckLambdaValue(&m->lambda_i4_);
    CheckLambdaValue(&m->lambda_i16_);
    CheckLambdaValue(&m->lambda_uv_);
    CheckLambdaValue(&m->lambda_mode_);
    CheckLambdaValue(&m->lambda_trellis_i4_);
    CheckLambdaValue(&m->lambda_trellis_i16_);
    CheckLambdaValue(&m->lambda_trellis_uv_);
    CheckLambdaValue(&m->tlambda_);

    m->min_disto_ = 20 * m->y1_.q_[0];   // quantization-aware min disto
    m->max_edge_  = 0;

    m->i4_penalty_ = 1000 * q_i4 * q_i4;
  }
}

//------------------------------------------------------------------------------
// Initialize filtering parameters

// Very small filter-strength values have close to no visual effect. So we can
// save a little decoding-CPU by turning filtering off for these.
#define FSTRENGTH_CUTOFF 2

static void SetupFilterStrength(VP8Encoder* const enc) {
  int i;
  // level0 is in [0..500]. Using '-f 50' as filter_strength is mid-filtering.
  const int level0 = 5 * enc->config_->filter_strength;
  for (i = 0; i < NUM_MB_SEGMENTS; ++i) {
    VP8SegmentInfo* const m = &enc->dqm_[i];
    // We focus on the quantization of AC coeffs.
    const int qstep = kAcTable[clip(m->quant_, 0, 127)] >> 2;
    const int base_strength =
        VP8FilterStrengthFromDelta(enc->filter_hdr_.sharpness_, qstep);
    // Segments with lower complexity ('beta') will be less filtered.
    const int f = base_strength * level0 / (256 + m->beta_);
    m->fstrength_ = (f < FSTRENGTH_CUTOFF) ? 0 : (f > 63) ? 63 : f;
  }
  // We record the initial strength (mainly for the case of 1-segment only).
  enc->filter_hdr_.level_ = enc->dqm_[0].fstrength_;
  enc->filter_hdr_.simple_ = (enc->config_->filter_type == 0);
  enc->filter_hdr_.sharpness_ = enc->config_->filter_sharpness;
}

//------------------------------------------------------------------------------

// Note: if you change the values below, remember that the max range
// allowed by the syntax for DQ_UV is [-16,16].
#define MAX_DQ_UV (6)
#define MIN_DQ_UV (-4)

// We want to emulate jpeg-like behaviour where the expected "good" quality
// is around q=75. Internally, our "good" middle is around c=50. So we
// map accordingly using linear piece-wise function
static double QualityToCompression(double c) {
  const double linear_c = (c < 0.75) ? c * (2. / 3.) : 2. * c - 1.;
  // The file size roughly scales as pow(quantizer, 3.). Actually, the
  // exponent is somewhere between 2.8 and 3.2, but we're mostly interested
  // in the mid-quant range. So we scale the compressibility inversely to
  // this power-law: quant ~= compression ^ 1/3. This law holds well for
  // low quant. Finer modeling for high-quant would make use of kAcTable[]
  // more explicitly.
  const double v = pow(linear_c, 1 / 3.);
  return v;
}

static double QualityToJPEGCompression(double c, double alpha) {
  // We map the complexity 'alpha' and quality setting 'c' to a compression
  // exponent empirically matched to the compression curve of libjpeg6b.
  // On average, the WebP output size will be roughly similar to that of a
  // JPEG file compressed with same quality factor.
  const double amin = 0.30;
  const double amax = 0.85;
  const double exp_min = 0.4;
  const double exp_max = 0.9;
  const double slope = (exp_min - exp_max) / (amax - amin);
  // Linearly interpolate 'expn' from exp_min to exp_max
  // in the [amin, amax] range.
  const double expn = (alpha > amax) ? exp_min
                    : (alpha < amin) ? exp_max
                    : exp_max + slope * (alpha - amin);
  const double v = pow(c, expn);
  return v;
}

static int SegmentsAreEquivalent(const VP8SegmentInfo* const S1,
                                 const VP8SegmentInfo* const S2) {
  return (S1->quant_ == S2->quant_) && (S1->fstrength_ == S2->fstrength_);
}

static void SimplifySegments(VP8Encoder* const enc) {
  int map[NUM_MB_SEGMENTS] = { 0, 1, 2, 3 };
  // 'num_segments_' is previously validated and <= NUM_MB_SEGMENTS, but an
  // explicit check is needed to avoid a spurious warning about 'i' exceeding
  // array bounds of 'dqm_' with some compilers (noticed with gcc-4.9).
  const int num_segments = (enc->segment_hdr_.num_segments_ < NUM_MB_SEGMENTS)
                               ? enc->segment_hdr_.num_segments_
                               : NUM_MB_SEGMENTS;
  int num_final_segments = 1;
  int s1, s2;
  for (s1 = 1; s1 < num_segments; ++s1) {    // find similar segments
    const VP8SegmentInfo* const S1 = &enc->dqm_[s1];
    int found = 0;
    // check if we already have similar segment
    for (s2 = 0; s2 < num_final_segments; ++s2) {
      const VP8SegmentInfo* const S2 = &enc->dqm_[s2];
      if (SegmentsAreEquivalent(S1, S2)) {
        found = 1;
        break;
      }
    }
    map[s1] = s2;
    if (!found) {
      if (num_final_segments != s1) {
        enc->dqm_[num_final_segments] = enc->dqm_[s1];
      }
      ++num_final_segments;
    }
  }
  if (num_final_segments < num_segments) {  // Remap
    int i = enc->mb_w_ * enc->mb_h_;
    while (i-- > 0) enc->mb_info_[i].segment_ = map[enc->mb_info_[i].segment_];
    enc->segment_hdr_.num_segments_ = num_final_segments;
    // Replicate the trailing segment infos (it's mostly cosmetics)
    for (i = num_final_segments; i < num_segments; ++i) {
      enc->dqm_[i] = enc->dqm_[num_final_segments - 1];
    }
  }
}

void VP8SetSegmentParams(VP8Encoder* const enc, float quality) {
  int i;
  int dq_uv_ac, dq_uv_dc;
  const int num_segments = enc->segment_hdr_.num_segments_;
  const double amp = SNS_TO_DQ * enc->config_->sns_strength / 100. / 128.;
  const double Q = quality / 100.;
  const double c_base = enc->config_->emulate_jpeg_size ?
      QualityToJPEGCompression(Q, enc->alpha_ / 255.) :
      QualityToCompression(Q);
  for (i = 0; i < num_segments; ++i) {
    // We modulate the base coefficient to accommodate for the quantization
    // susceptibility and allow denser segments to be quantized more.
    const double expn = 1. - amp * enc->dqm_[i].alpha_;
    const double c = pow(c_base, expn);
    const int q = (int)(127. * (1. - c));
    assert(expn > 0.);
    enc->dqm_[i].quant_ = clip(q, 0, 127);
  }

  // purely indicative in the bitstream (except for the 1-segment case)
  enc->base_quant_ = enc->dqm_[0].quant_;

  // fill-in values for the unused segments (required by the syntax)
  for (i = num_segments; i < NUM_MB_SEGMENTS; ++i) {
    enc->dqm_[i].quant_ = enc->base_quant_;
  }

  // uv_alpha_ is normally spread around ~60. The useful range is
  // typically ~30 (quite bad) to ~100 (ok to decimate UV more).
  // We map it to the safe maximal range of MAX/MIN_DQ_UV for dq_uv.
  dq_uv_ac = (enc->uv_alpha_ - MID_ALPHA) * (MAX_DQ_UV - MIN_DQ_UV)
                                          / (MAX_ALPHA - MIN_ALPHA);
  // we rescale by the user-defined strength of adaptation
  dq_uv_ac = dq_uv_ac * enc->config_->sns_strength / 100;
  // and make it safe.
  dq_uv_ac = clip(dq_uv_ac, MIN_DQ_UV, MAX_DQ_UV);
  // We also boost the dc-uv-quant a little, based on sns-strength, since
  // U/V channels are quite more reactive to high quants (flat DC-blocks
  // tend to appear, and are unpleasant).
  dq_uv_dc = -4 * enc->config_->sns_strength / 100;
  dq_uv_dc = clip(dq_uv_dc, -15, 15);   // 4bit-signed max allowed

  enc->dq_y1_dc_ = 0;       // TODO(skal): dq-lum
  enc->dq_y2_dc_ = 0;
  enc->dq_y2_ac_ = 0;
  enc->dq_uv_dc_ = dq_uv_dc;
  enc->dq_uv_ac_ = dq_uv_ac;

  SetupFilterStrength(enc);   // initialize segments' filtering, eventually

  if (num_segments > 1) SimplifySegments(enc);

  SetupMatrices(enc);         // finalize quantization matrices
}

//------------------------------------------------------------------------------
// Form the predictions in cache

// Must be ordered using {DC_PRED, TM_PRED, V_PRED, H_PRED} as index
const uint16_t VP8I16ModeOffsets[4] = { I16DC16, I16TM16, I16VE16, I16HE16 };
const uint16_t VP8UVModeOffsets[4] = { C8DC8, C8TM8, C8VE8, C8HE8 };

// Must be indexed using {B_DC_PRED -> B_HU_PRED} as index
static const uint16_t VP8I4ModeOffsets[NUM_BMODES] = {
  I4DC4, I4TM4, I4VE4, I4HE4, I4RD4, I4VR4, I4LD4, I4VL4, I4HD4, I4HU4
};

void VP8MakeLuma16Preds(const VP8EncIterator* const it) {
  const uint8_t* const left = it->x_ ? it->y_left_ : NULL;
  const uint8_t* const top = it->y_ ? it->y_top_ : NULL;
  VP8EncPredLuma16(it->yuv_p_, left, top);
}

void VP8MakeChroma8Preds(const VP8EncIterator* const it) {
  const uint8_t* const left = it->x_ ? it->u_left_ : NULL;
  const uint8_t* const top = it->y_ ? it->uv_top_ : NULL;
  VP8EncPredChroma8(it->yuv_p_, left, top);
}

// Form all the ten Intra4x4 predictions in the yuv_p_ cache
// for the 4x4 block it->i4_
static void MakeIntra4Preds(const VP8EncIterator* const it) {
  VP8EncPredLuma4(it->yuv_p_, it->i4_top_);
}

//------------------------------------------------------------------------------
// Quantize

// Layout:
// +----+----+
// |YYYY|UUVV| 0
// |YYYY|UUVV| 4
// |YYYY|....| 8
// |YYYY|....| 12
// +----+----+

const uint16_t VP8Scan[16] = {  // Luma
  0 +  0 * BPS,  4 +  0 * BPS, 8 +  0 * BPS, 12 +  0 * BPS,
  0 +  4 * BPS,  4 +  4 * BPS, 8 +  4 * BPS, 12 +  4 * BPS,
  0 +  8 * BPS,  4 +  8 * BPS, 8 +  8 * BPS, 12 +  8 * BPS,
  0 + 12 * BPS,  4 + 12 * BPS, 8 + 12 * BPS, 12 + 12 * BPS,
};

static const uint16_t VP8ScanUV[4 + 4] = {
  0 + 0 * BPS,   4 + 0 * BPS, 0 + 4 * BPS,  4 + 4 * BPS,    // U
  8 + 0 * BPS,  12 + 0 * BPS, 8 + 4 * BPS, 12 + 4 * BPS     // V
};

//------------------------------------------------------------------------------
// Distortion measurement

static const uint16_t kWeightY[16] = {
  38, 32, 20, 9, 32, 28, 17, 7, 20, 17, 10, 4, 9, 7, 4, 2
};

static const uint16_t kWeightTrellis[16] = {
#if USE_TDISTO == 0
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
#else
  30, 27, 19, 11,
  27, 24, 17, 10,
  19, 17, 12,  8,
  11, 10,  8,  6
#endif
};

// Init/Copy the common fields in score.
static void InitScore(VP8ModeScore* const rd) {
  rd->D  = 0;
  rd->SD = 0;
  rd->R  = 0;
  rd->H  = 0;
  rd->nz = 0;
  rd->score = MAX_COST;
}

static void CopyScore(VP8ModeScore* WEBP_RESTRICT const dst,
                      const VP8ModeScore* WEBP_RESTRICT const src) {
  dst->D  = src->D;
  dst->SD = src->SD;
  dst->R  = src->R;
  dst->H  = src->H;
  dst->nz = src->nz;      // note that nz is not accumulated, but just copied.
  dst->score = src->score;
}

static void AddScore(VP8ModeScore* WEBP_RESTRICT const dst,
                     const VP8ModeScore* WEBP_RESTRICT const src) {
  dst->D  += src->D;
  dst->SD += src->SD;
  dst->R  += src->R;
  dst->H  += src->H;
  dst->nz |= src->nz;     // here, new nz bits are accumulated.
  dst->score += src->score;
}

//------------------------------------------------------------------------------
// Performs trellis-optimized quantization.

// -- GODOT start --
// Prevents Visual Studio debugger from using this Node struct in place of the Godot Node class.
#define Node Node_libwebp_quant
// -- GODOT end --

// Trellis node
typedef struct {
  int8_t prev;            // best previous node
  int8_t sign;            // sign of coeff_i
  int16_t level;          // level
} Node;

// Score state
typedef struct {
  score_t score;          // partial RD score
  const uint16_t* costs;  // shortcut to cost tables
} ScoreState;

// If a coefficient was quantized to a value Q (using a neutral bias),
// we test all alternate possibilities between [Q-MIN_DELTA, Q+MAX_DELTA]
// We don't test negative values though.
#define MIN_DELTA 0   // how much lower level to try
#define MAX_DELTA 1   // how much higher
#define NUM_NODES (MIN_DELTA + 1 + MAX_DELTA)
#define NODE(n, l) (nodes[(n)][(l) + MIN_DELTA])
#define SCORE_STATE(n, l) (score_states[n][(l) + MIN_DELTA])

static WEBP_INLINE void SetRDScore(int lambda, VP8ModeScore* const rd) {
  rd->score = (rd->R + rd->H) * lambda + RD_DISTO_MULT * (rd->D + rd->SD);
}

static WEBP_INLINE score_t RDScoreTrellis(int lambda, score_t rate,
                                          score_t distortion) {
  return rate * lambda + RD_DISTO_MULT * distortion;
}

// Coefficient type.
enum { TYPE_I16_AC = 0, TYPE_I16_DC = 1, TYPE_CHROMA_A = 2, TYPE_I4_AC = 3 };

static int TrellisQuantizeBlock(const VP8Encoder* WEBP_RESTRICT const enc,
                                int16_t in[16], int16_t out[16],
                                int ctx0, int coeff_type,
                                const VP8Matrix* WEBP_RESTRICT const mtx,
                                int lambda) {
  const ProbaArray* const probas = enc->proba_.coeffs_[coeff_type];
  CostArrayPtr const costs =
      (CostArrayPtr)enc->proba_.remapped_costs_[coeff_type];
  const int first = (coeff_type == TYPE_I16_AC) ? 1 : 0;
  Node nodes[16][NUM_NODES];
  ScoreState score_states[2][NUM_NODES];
  ScoreState* ss_cur = &SCORE_STATE(0, MIN_DELTA);
  ScoreState* ss_prev = &SCORE_STATE(1, MIN_DELTA);
  int best_path[3] = {-1, -1, -1};   // store best-last/best-level/best-previous
  score_t best_score;
  int n, m, p, last;

  {
    score_t cost;
    const int thresh = mtx->q_[1] * mtx->q_[1] / 4;
    const int last_proba = probas[VP8EncBands[first]][ctx0][0];

    // compute the position of the last interesting coefficient
    last = first - 1;
    for (n = 15; n >= first; --n) {
      const int j = kZigzag[n];
      const int err = in[j] * in[j];
      if (err > thresh) {
        last = n;
        break;
      }
    }
    // we don't need to go inspect up to n = 16 coeffs. We can just go up
    // to last + 1 (inclusive) without losing much.
    if (last < 15) ++last;

    // compute 'skip' score. This is the max score one can do.
    cost = VP8BitCost(0, last_proba);
    best_score = RDScoreTrellis(lambda, cost, 0);

    // initialize source node.
    for (m = -MIN_DELTA; m <= MAX_DELTA; ++m) {
      const score_t rate = (ctx0 == 0) ? VP8BitCost(1, last_proba) : 0;
      ss_cur[m].score = RDScoreTrellis(lambda, rate, 0);
      ss_cur[m].costs = costs[first][ctx0];
    }
  }

  // traverse trellis.
  for (n = first; n <= last; ++n) {
    const int j = kZigzag[n];
    const uint32_t Q  = mtx->q_[j];
    const uint32_t iQ = mtx->iq_[j];
    const uint32_t B = BIAS(0x00);     // neutral bias
    // note: it's important to take sign of the _original_ coeff,
    // so we don't have to consider level < 0 afterward.
    const int sign = (in[j] < 0);
    const uint32_t coeff0 = (sign ? -in[j] : in[j]) + mtx->sharpen_[j];
    int level0 = QUANTDIV(coeff0, iQ, B);
    int thresh_level = QUANTDIV(coeff0, iQ, BIAS(0x80));
    if (thresh_level > MAX_LEVEL) thresh_level = MAX_LEVEL;
    if (level0 > MAX_LEVEL) level0 = MAX_LEVEL;

    {   // Swap current and previous score states
      ScoreState* const tmp = ss_cur;
      ss_cur = ss_prev;
      ss_prev = tmp;
    }

    // test all alternate level values around level0.
    for (m = -MIN_DELTA; m <= MAX_DELTA; ++m) {
      Node* const cur = &NODE(n, m);
      const int level = level0 + m;
      const int ctx = (level > 2) ? 2 : level;
      const int band = VP8EncBands[n + 1];
      score_t base_score;
      score_t best_cur_score;
      int best_prev;
      score_t cost, score;

      ss_cur[m].costs = costs[n + 1][ctx];
      if (level < 0 || level > thresh_level) {
        ss_cur[m].score = MAX_COST;
        // Node is dead.
        continue;
      }

      {
        // Compute delta_error = how much coding this level will
        // subtract to max_error as distortion.
        // Here, distortion = sum of (|coeff_i| - level_i * Q_i)^2
        const int new_error = coeff0 - level * Q;
        const int delta_error =
            kWeightTrellis[j] * (new_error * new_error - coeff0 * coeff0);
        base_score = RDScoreTrellis(lambda, 0, delta_error);
      }

      // Inspect all possible non-dead predecessors. Retain only the best one.
      // The base_score is added to all scores so it is only added for the final
      // value after the loop.
      cost = VP8LevelCost(ss_prev[-MIN_DELTA].costs, level);
      best_cur_score =
          ss_prev[-MIN_DELTA].score + RDScoreTrellis(lambda, cost, 0);
      best_prev = -MIN_DELTA;
      for (p = -MIN_DELTA + 1; p <= MAX_DELTA; ++p) {
        // Dead nodes (with ss_prev[p].score >= MAX_COST) are automatically
        // eliminated since their score can't be better than the current best.
        cost = VP8LevelCost(ss_prev[p].costs, level);
        // Examine node assuming it's a non-terminal one.
        score = ss_prev[p].score + RDScoreTrellis(lambda, cost, 0);
        if (score < best_cur_score) {
          best_cur_score = score;
          best_prev = p;
        }
      }
      best_cur_score += base_score;
      // Store best finding in current node.
      cur->sign = sign;
      cur->level = level;
      cur->prev = best_prev;
      ss_cur[m].score = best_cur_score;

      // Now, record best terminal node (and thus best entry in the graph).
      if (level != 0 && best_cur_score < best_score) {
        const score_t last_pos_cost =
            (n < 15) ? VP8BitCost(0, probas[band][ctx][0]) : 0;
        const score_t last_pos_score = RDScoreTrellis(lambda, last_pos_cost, 0);
        score = best_cur_score + last_pos_score;
        if (score < best_score) {
          best_score = score;
          best_path[0] = n;                     // best eob position
          best_path[1] = m;                     // best node index
          best_path[2] = best_prev;             // best predecessor
        }
      }
    }
  }

  // Fresh start
  // Beware! We must preserve in[0]/out[0] value for TYPE_I16_AC case.
  if (coeff_type == TYPE_I16_AC) {
    memset(in + 1, 0, 15 * sizeof(*in));
    memset(out + 1, 0, 15 * sizeof(*out));
  } else {
    memset(in, 0, 16 * sizeof(*in));
    memset(out, 0, 16 * sizeof(*out));
  }
  if (best_path[0] == -1) {
    return 0;  // skip!
  }

  {
    // Unwind the best path.
    // Note: best-prev on terminal node is not necessarily equal to the
    // best_prev for non-terminal. So we patch best_path[2] in.
    int nz = 0;
    int best_node = best_path[1];
    n = best_path[0];
    NODE(n, best_node).prev = best_path[2];   // force best-prev for terminal

    for (; n >= first; --n) {
      const Node* const node = &NODE(n, best_node);
      const int j = kZigzag[n];
      out[n] = node->sign ? -node->level : node->level;
      nz |= node->level;
      in[j] = out[n] * mtx->q_[j];
      best_node = node->prev;
    }
    return (nz != 0);
  }
}

#undef NODE

//------------------------------------------------------------------------------
// Performs: difference, transform, quantize, back-transform, add
// all at once. Output is the reconstructed block in *yuv_out, and the
// quantized levels in *levels.

static int ReconstructIntra16(VP8EncIterator* WEBP_RESTRICT const it,
                              VP8ModeScore* WEBP_RESTRICT const rd,
                              uint8_t* WEBP_RESTRICT const yuv_out,
                              int mode) {
  const VP8Encoder* const enc = it->enc_;
  const uint8_t* const ref = it->yuv_p_ + VP8I16ModeOffsets[mode];
  const uint8_t* const src = it->yuv_in_ + Y_OFF_ENC;
  const VP8SegmentInfo* const dqm = &enc->dqm_[it->mb_->segment_];
  int nz = 0;
  int n;
  int16_t tmp[16][16], dc_tmp[16];

  for (n = 0; n < 16; n += 2) {
    VP8FTransform2(src + VP8Scan[n], ref + VP8Scan[n], tmp[n]);
  }
  VP8FTransformWHT(tmp[0], dc_tmp);
  nz |= VP8EncQuantizeBlockWHT(dc_tmp, rd->y_dc_levels, &dqm->y2_) << 24;

  if (DO_TRELLIS_I16 && it->do_trellis_) {
    int x, y;
    VP8IteratorNzToBytes(it);
    for (y = 0, n = 0; y < 4; ++y) {
      for (x = 0; x < 4; ++x, ++n) {
        const int ctx = it->top_nz_[x] + it->left_nz_[y];
        const int non_zero = TrellisQuantizeBlock(
            enc, tmp[n], rd->y_ac_levels[n], ctx, TYPE_I16_AC, &dqm->y1_,
            dqm->lambda_trellis_i16_);
        it->top_nz_[x] = it->left_nz_[y] = non_zero;
        rd->y_ac_levels[n][0] = 0;
        nz |= non_zero << n;
      }
    }
  } else {
    for (n = 0; n < 16; n += 2) {
      // Zero-out the first coeff, so that: a) nz is correct below, and
      // b) finding 'last' non-zero coeffs in SetResidualCoeffs() is simplified.
      tmp[n][0] = tmp[n + 1][0] = 0;
      nz |= VP8EncQuantize2Blocks(tmp[n], rd->y_ac_levels[n], &dqm->y1_) << n;
      assert(rd->y_ac_levels[n + 0][0] == 0);
      assert(rd->y_ac_levels[n + 1][0] == 0);
    }
  }

  // Transform back
  VP8TransformWHT(dc_tmp, tmp[0]);
  for (n = 0; n < 16; n += 2) {
    VP8ITransform(ref + VP8Scan[n], tmp[n], yuv_out + VP8Scan[n], 1);
  }

  return nz;
}

static int ReconstructIntra4(VP8EncIterator* WEBP_RESTRICT const it,
                             int16_t levels[16],
                             const uint8_t* WEBP_RESTRICT const src,
                             uint8_t* WEBP_RESTRICT const yuv_out,
                             int mode) {
  const VP8Encoder* const enc = it->enc_;
  const uint8_t* const ref = it->yuv_p_ + VP8I4ModeOffsets[mode];
  const VP8SegmentInfo* const dqm = &enc->dqm_[it->mb_->segment_];
  int nz = 0;
  int16_t tmp[16];

  VP8FTransform(src, ref, tmp);
  if (DO_TRELLIS_I4 && it->do_trellis_) {
    const int x = it->i4_ & 3, y = it->i4_ >> 2;
    const int ctx = it->top_nz_[x] + it->left_nz_[y];
    nz = TrellisQuantizeBlock(enc, tmp, levels, ctx, TYPE_I4_AC, &dqm->y1_,
                              dqm->lambda_trellis_i4_);
  } else {
    nz = VP8EncQuantizeBlock(tmp, levels, &dqm->y1_);
  }
  VP8ITransform(ref, tmp, yuv_out, 0);
  return nz;
}

//------------------------------------------------------------------------------
// DC-error diffusion

// Diffusion weights. We under-correct a bit (15/16th of the error is actually
// diffused) to avoid 'rainbow' chessboard pattern of blocks at q~=0.
#define C1 7    // fraction of error sent to the 4x4 block below
#define C2 8    // fraction of error sent to the 4x4 block on the right
#define DSHIFT 4
#define DSCALE 1   // storage descaling, needed to make the error fit int8_t

// Quantize as usual, but also compute and return the quantization error.
// Error is already divided by DSHIFT.
static int QuantizeSingle(int16_t* WEBP_RESTRICT const v,
                          const VP8Matrix* WEBP_RESTRICT const mtx) {
  int V = *v;
  const int sign = (V < 0);
  if (sign) V = -V;
  if (V > (int)mtx->zthresh_[0]) {
    const int qV = QUANTDIV(V, mtx->iq_[0], mtx->bias_[0]) * mtx->q_[0];
    const int err = (V - qV);
    *v = sign ? -qV : qV;
    return (sign ? -err : err) >> DSCALE;
  }
  *v = 0;
  return (sign ? -V : V) >> DSCALE;
}

static void CorrectDCValues(const VP8EncIterator* WEBP_RESTRICT const it,
                            const VP8Matrix* WEBP_RESTRICT const mtx,
                            int16_t tmp[][16],
                            VP8ModeScore* WEBP_RESTRICT const rd) {
  //         | top[0] | top[1]
  // --------+--------+---------
  // left[0] | tmp[0]   tmp[1]  <->   err0 err1
  // left[1] | tmp[2]   tmp[3]        err2 err3
  //
  // Final errors {err1,err2,err3} are preserved and later restored
  // as top[]/left[] on the next block.
  int ch;
  for (ch = 0; ch <= 1; ++ch) {
    const int8_t* const top = it->top_derr_[it->x_][ch];
    const int8_t* const left = it->left_derr_[ch];
    int16_t (* const c)[16] = &tmp[ch * 4];
    int err0, err1, err2, err3;
    c[0][0] += (C1 * top[0] + C2 * left[0]) >> (DSHIFT - DSCALE);
    err0 = QuantizeSingle(&c[0][0], mtx);
    c[1][0] += (C1 * top[1] + C2 * err0) >> (DSHIFT - DSCALE);
    err1 = QuantizeSingle(&c[1][0], mtx);
    c[2][0] += (C1 * err0 + C2 * left[1]) >> (DSHIFT - DSCALE);
    err2 = QuantizeSingle(&c[2][0], mtx);
    c[3][0] += (C1 * err1 + C2 * err2) >> (DSHIFT - DSCALE);
    err3 = QuantizeSingle(&c[3][0], mtx);
    // error 'err' is bounded by mtx->q_[0] which is 132 at max. Hence
    // err >> DSCALE will fit in an int8_t type if DSCALE>=1.
    assert(abs(err1) <= 127 && abs(err2) <= 127 && abs(err3) <= 127);
    rd->derr[ch][0] = (int8_t)err1;
    rd->derr[ch][1] = (int8_t)err2;
    rd->derr[ch][2] = (int8_t)err3;
  }
}

static void StoreDiffusionErrors(VP8EncIterator* WEBP_RESTRICT const it,
                                 const VP8ModeScore* WEBP_RESTRICT const rd) {
  int ch;
  for (ch = 0; ch <= 1; ++ch) {
    int8_t* const top = it->top_derr_[it->x_][ch];
    int8_t* const left = it->left_derr_[ch];
    left[0] = rd->derr[ch][0];            // restore err1
    left[1] = 3 * rd->derr[ch][2] >> 2;   //     ... 3/4th of err3
    top[0]  = rd->derr[ch][1];            //     ... err2
    top[1]  = rd->derr[ch][2] - left[1];  //     ... 1/4th of err3.
  }
}

#undef C1
#undef C2
#undef DSHIFT
#undef DSCALE

//------------------------------------------------------------------------------

static int ReconstructUV(VP8EncIterator* WEBP_RESTRICT const it,
                         VP8ModeScore* WEBP_RESTRICT const rd,
                         uint8_t* WEBP_RESTRICT const yuv_out, int mode) {
  const VP8Encoder* const enc = it->enc_;
  const uint8_t* const ref = it->yuv_p_ + VP8UVModeOffsets[mode];
  const uint8_t* const src = it->yuv_in_ + U_OFF_ENC;
  const VP8SegmentInfo* const dqm = &enc->dqm_[it->mb_->segment_];
  int nz = 0;
  int n;
  int16_t tmp[8][16];

  for (n = 0; n < 8; n += 2) {
    VP8FTransform2(src + VP8ScanUV[n], ref + VP8ScanUV[n], tmp[n]);
  }
  if (it->top_derr_ != NULL) CorrectDCValues(it, &dqm->uv_, tmp, rd);

  if (DO_TRELLIS_UV && it->do_trellis_) {
    int ch, x, y;
    for (ch = 0, n = 0; ch <= 2; ch += 2) {
      for (y = 0; y < 2; ++y) {
        for (x = 0; x < 2; ++x, ++n) {
          const int ctx = it->top_nz_[4 + ch + x] + it->left_nz_[4 + ch + y];
          const int non_zero = TrellisQuantizeBlock(
              enc, tmp[n], rd->uv_levels[n], ctx, TYPE_CHROMA_A, &dqm->uv_,
              dqm->lambda_trellis_uv_);
          it->top_nz_[4 + ch + x] = it->left_nz_[4 + ch + y] = non_zero;
          nz |= non_zero << n;
        }
      }
    }
  } else {
    for (n = 0; n < 8; n += 2) {
      nz |= VP8EncQuantize2Blocks(tmp[n], rd->uv_levels[n], &dqm->uv_) << n;
    }
  }

  for (n = 0; n < 8; n += 2) {
    VP8ITransform(ref + VP8ScanUV[n], tmp[n], yuv_out + VP8ScanUV[n], 1);
  }
  return (nz << 16);
}

//------------------------------------------------------------------------------
// RD-opt decision. Reconstruct each modes, evalue distortion and bit-cost.
// Pick the mode is lower RD-cost = Rate + lambda * Distortion.

static void StoreMaxDelta(VP8SegmentInfo* const dqm, const int16_t DCs[16]) {
  // We look at the first three AC coefficients to determine what is the average
  // delta between each sub-4x4 block.
  const int v0 = abs(DCs[1]);
  const int v1 = abs(DCs[2]);
  const int v2 = abs(DCs[4]);
  int max_v = (v1 > v0) ? v1 : v0;
  max_v = (v2 > max_v) ? v2 : max_v;
  if (max_v > dqm->max_edge_) dqm->max_edge_ = max_v;
}

static void SwapModeScore(VP8ModeScore** a, VP8ModeScore** b) {
  VP8ModeScore* const tmp = *a;
  *a = *b;
  *b = tmp;
}

static void SwapPtr(uint8_t** a, uint8_t** b) {
  uint8_t* const tmp = *a;
  *a = *b;
  *b = tmp;
}

static void SwapOut(VP8EncIterator* const it) {
  SwapPtr(&it->yuv_out_, &it->yuv_out2_);
}

static void PickBestIntra16(VP8EncIterator* WEBP_RESTRICT const it,
                            VP8ModeScore* WEBP_RESTRICT rd) {
  const int kNumBlocks = 16;
  VP8SegmentInfo* const dqm = &it->enc_->dqm_[it->mb_->segment_];
  const int lambda = dqm->lambda_i16_;
  const int tlambda = dqm->tlambda_;
  const uint8_t* const src = it->yuv_in_ + Y_OFF_ENC;
  VP8ModeScore rd_tmp;
  VP8ModeScore* rd_cur = &rd_tmp;
  VP8ModeScore* rd_best = rd;
  int mode;
  int is_flat = IsFlatSource16(it->yuv_in_ + Y_OFF_ENC);

  rd->mode_i16 = -1;
  for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
    uint8_t* const tmp_dst = it->yuv_out2_ + Y_OFF_ENC;  // scratch buffer
    rd_cur->mode_i16 = mode;

    // Reconstruct
    rd_cur->nz = ReconstructIntra16(it, rd_cur, tmp_dst, mode);

    // Measure RD-score
    rd_cur->D = VP8SSE16x16(src, tmp_dst);
    rd_cur->SD =
        tlambda ? MULT_8B(tlambda, VP8TDisto16x16(src, tmp_dst, kWeightY)) : 0;
    rd_cur->H = VP8FixedCostsI16[mode];
    rd_cur->R = VP8GetCostLuma16(it, rd_cur);
    if (is_flat) {
      // refine the first impression (which was in pixel space)
      is_flat = IsFlat(rd_cur->y_ac_levels[0], kNumBlocks, FLATNESS_LIMIT_I16);
      if (is_flat) {
        // Block is very flat. We put emphasis on the distortion being very low!
        rd_cur->D *= 2;
        rd_cur->SD *= 2;
      }
    }

    // Since we always examine Intra16 first, we can overwrite *rd directly.
    SetRDScore(lambda, rd_cur);
    if (mode == 0 || rd_cur->score < rd_best->score) {
      SwapModeScore(&rd_cur, &rd_best);
      SwapOut(it);
    }
  }
  if (rd_best != rd) {
    memcpy(rd, rd_best, sizeof(*rd));
  }
  SetRDScore(dqm->lambda_mode_, rd);   // finalize score for mode decision.
  VP8SetIntra16Mode(it, rd->mode_i16);

  // we have a blocky macroblock (only DCs are non-zero) with fairly high
  // distortion, record max delta so we can later adjust the minimal filtering
  // strength needed to smooth these blocks out.
  if ((rd->nz & 0x100ffff) == 0x1000000 && rd->D > dqm->min_disto_) {
    StoreMaxDelta(dqm, rd->y_dc_levels);
  }
}

//------------------------------------------------------------------------------

// return the cost array corresponding to the surrounding prediction modes.
static const uint16_t* GetCostModeI4(VP8EncIterator* WEBP_RESTRICT const it,
                                     const uint8_t modes[16]) {
  const int preds_w = it->enc_->preds_w_;
  const int x = (it->i4_ & 3), y = it->i4_ >> 2;
  const int left = (x == 0) ? it->preds_[y * preds_w - 1] : modes[it->i4_ - 1];
  const int top = (y == 0) ? it->preds_[-preds_w + x] : modes[it->i4_ - 4];
  return VP8FixedCostsI4[top][left];
}

static int PickBestIntra4(VP8EncIterator* WEBP_RESTRICT const it,
                          VP8ModeScore* WEBP_RESTRICT const rd) {
  const VP8Encoder* const enc = it->enc_;
  const VP8SegmentInfo* const dqm = &enc->dqm_[it->mb_->segment_];
  const int lambda = dqm->lambda_i4_;
  const int tlambda = dqm->tlambda_;
  const uint8_t* const src0 = it->yuv_in_ + Y_OFF_ENC;
  uint8_t* const best_blocks = it->yuv_out2_ + Y_OFF_ENC;
  int total_header_bits = 0;
  VP8ModeScore rd_best;

  if (enc->max_i4_header_bits_ == 0) {
    return 0;
  }

  InitScore(&rd_best);
  rd_best.H = 211;  // '211' is the value of VP8BitCost(0, 145)
  SetRDScore(dqm->lambda_mode_, &rd_best);
  VP8IteratorStartI4(it);
  do {
    const int kNumBlocks = 1;
    VP8ModeScore rd_i4;
    int mode;
    int best_mode = -1;
    const uint8_t* const src = src0 + VP8Scan[it->i4_];
    const uint16_t* const mode_costs = GetCostModeI4(it, rd->modes_i4);
    uint8_t* best_block = best_blocks + VP8Scan[it->i4_];
    uint8_t* tmp_dst = it->yuv_p_ + I4TMP;    // scratch buffer.

    InitScore(&rd_i4);
    MakeIntra4Preds(it);
    for (mode = 0; mode < NUM_BMODES; ++mode) {
      VP8ModeScore rd_tmp;
      int16_t tmp_levels[16];

      // Reconstruct
      rd_tmp.nz =
          ReconstructIntra4(it, tmp_levels, src, tmp_dst, mode) << it->i4_;

      // Compute RD-score
      rd_tmp.D = VP8SSE4x4(src, tmp_dst);
      rd_tmp.SD =
          tlambda ? MULT_8B(tlambda, VP8TDisto4x4(src, tmp_dst, kWeightY))
                  : 0;
      rd_tmp.H = mode_costs[mode];

      // Add flatness penalty, to avoid flat area to be mispredicted
      // by a complex mode.
      if (mode > 0 && IsFlat(tmp_levels, kNumBlocks, FLATNESS_LIMIT_I4)) {
        rd_tmp.R = FLATNESS_PENALTY * kNumBlocks;
      } else {
        rd_tmp.R = 0;
      }

      // early-out check
      SetRDScore(lambda, &rd_tmp);
      if (best_mode >= 0 && rd_tmp.score >= rd_i4.score) continue;

      // finish computing score
      rd_tmp.R += VP8GetCostLuma4(it, tmp_levels);
      SetRDScore(lambda, &rd_tmp);

      if (best_mode < 0 || rd_tmp.score < rd_i4.score) {
        CopyScore(&rd_i4, &rd_tmp);
        best_mode = mode;
        SwapPtr(&tmp_dst, &best_block);
        memcpy(rd_best.y_ac_levels[it->i4_], tmp_levels,
               sizeof(rd_best.y_ac_levels[it->i4_]));
      }
    }
    SetRDScore(dqm->lambda_mode_, &rd_i4);
    AddScore(&rd_best, &rd_i4);
    if (rd_best.score >= rd->score) {
      return 0;
    }
    total_header_bits += (int)rd_i4.H;   // <- equal to mode_costs[best_mode];
    if (total_header_bits > enc->max_i4_header_bits_) {
      return 0;
    }
    // Copy selected samples if not in the right place already.
    if (best_block != best_blocks + VP8Scan[it->i4_]) {
      VP8Copy4x4(best_block, best_blocks + VP8Scan[it->i4_]);
    }
    rd->modes_i4[it->i4_] = best_mode;
    it->top_nz_[it->i4_ & 3] = it->left_nz_[it->i4_ >> 2] = (rd_i4.nz ? 1 : 0);
  } while (VP8IteratorRotateI4(it, best_blocks));

  // finalize state
  CopyScore(rd, &rd_best);
  VP8SetIntra4Mode(it, rd->modes_i4);
  SwapOut(it);
  memcpy(rd->y_ac_levels, rd_best.y_ac_levels, sizeof(rd->y_ac_levels));
  return 1;   // select intra4x4 over intra16x16
}

//------------------------------------------------------------------------------

static void PickBestUV(VP8EncIterator* WEBP_RESTRICT const it,
                       VP8ModeScore* WEBP_RESTRICT const rd) {
  const int kNumBlocks = 8;
  const VP8SegmentInfo* const dqm = &it->enc_->dqm_[it->mb_->segment_];
  const int lambda = dqm->lambda_uv_;
  const uint8_t* const src = it->yuv_in_ + U_OFF_ENC;
  uint8_t* tmp_dst = it->yuv_out2_ + U_OFF_ENC;  // scratch buffer
  uint8_t* dst0 = it->yuv_out_ + U_OFF_ENC;
  uint8_t* dst = dst0;
  VP8ModeScore rd_best;
  int mode;

  rd->mode_uv = -1;
  InitScore(&rd_best);
  for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
    VP8ModeScore rd_uv;

    // Reconstruct
    rd_uv.nz = ReconstructUV(it, &rd_uv, tmp_dst, mode);

    // Compute RD-score
    rd_uv.D  = VP8SSE16x8(src, tmp_dst);
    rd_uv.SD = 0;    // not calling TDisto here: it tends to flatten areas.
    rd_uv.H  = VP8FixedCostsUV[mode];
    rd_uv.R  = VP8GetCostUV(it, &rd_uv);
    if (mode > 0 && IsFlat(rd_uv.uv_levels[0], kNumBlocks, FLATNESS_LIMIT_UV)) {
      rd_uv.R += FLATNESS_PENALTY * kNumBlocks;
    }

    SetRDScore(lambda, &rd_uv);
    if (mode == 0 || rd_uv.score < rd_best.score) {
      CopyScore(&rd_best, &rd_uv);
      rd->mode_uv = mode;
      memcpy(rd->uv_levels, rd_uv.uv_levels, sizeof(rd->uv_levels));
      if (it->top_derr_ != NULL) {
        memcpy(rd->derr, rd_uv.derr, sizeof(rd_uv.derr));
      }
      SwapPtr(&dst, &tmp_dst);
    }
  }
  VP8SetIntraUVMode(it, rd->mode_uv);
  AddScore(rd, &rd_best);
  if (dst != dst0) {   // copy 16x8 block if needed
    VP8Copy16x8(dst, dst0);
  }
  if (it->top_derr_ != NULL) {  // store diffusion errors for next block
    StoreDiffusionErrors(it, rd);
  }
}

//------------------------------------------------------------------------------
// Final reconstruction and quantization.

static void SimpleQuantize(VP8EncIterator* WEBP_RESTRICT const it,
                           VP8ModeScore* WEBP_RESTRICT const rd) {
  const VP8Encoder* const enc = it->enc_;
  const int is_i16 = (it->mb_->type_ == 1);
  int nz = 0;

  if (is_i16) {
    nz = ReconstructIntra16(it, rd, it->yuv_out_ + Y_OFF_ENC, it->preds_[0]);
  } else {
    VP8IteratorStartI4(it);
    do {
      const int mode =
          it->preds_[(it->i4_ & 3) + (it->i4_ >> 2) * enc->preds_w_];
      const uint8_t* const src = it->yuv_in_ + Y_OFF_ENC + VP8Scan[it->i4_];
      uint8_t* const dst = it->yuv_out_ + Y_OFF_ENC + VP8Scan[it->i4_];
      MakeIntra4Preds(it);
      nz |= ReconstructIntra4(it, rd->y_ac_levels[it->i4_],
                              src, dst, mode) << it->i4_;
    } while (VP8IteratorRotateI4(it, it->yuv_out_ + Y_OFF_ENC));
  }

  nz |= ReconstructUV(it, rd, it->yuv_out_ + U_OFF_ENC, it->mb_->uv_mode_);
  rd->nz = nz;
}

// Refine intra16/intra4 sub-modes based on distortion only (not rate).
static void RefineUsingDistortion(VP8EncIterator* WEBP_RESTRICT const it,
                                  int try_both_modes, int refine_uv_mode,
                                  VP8ModeScore* WEBP_RESTRICT const rd) {
  score_t best_score = MAX_COST;
  int nz = 0;
  int mode;
  int is_i16 = try_both_modes || (it->mb_->type_ == 1);

  const VP8SegmentInfo* const dqm = &it->enc_->dqm_[it->mb_->segment_];
  // Some empiric constants, of approximate order of magnitude.
  const int lambda_d_i16 = 106;
  const int lambda_d_i4 = 11;
  const int lambda_d_uv = 120;
  score_t score_i4 = dqm->i4_penalty_;
  score_t i4_bit_sum = 0;
  const score_t bit_limit = try_both_modes ? it->enc_->mb_header_limit_
                                           : MAX_COST;  // no early-out allowed

  if (is_i16) {   // First, evaluate Intra16 distortion
    int best_mode = -1;
    const uint8_t* const src = it->yuv_in_ + Y_OFF_ENC;
    for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
      const uint8_t* const ref = it->yuv_p_ + VP8I16ModeOffsets[mode];
      const score_t score = (score_t)VP8SSE16x16(src, ref) * RD_DISTO_MULT
                          + VP8FixedCostsI16[mode] * lambda_d_i16;
      if (mode > 0 && VP8FixedCostsI16[mode] > bit_limit) {
        continue;
      }

      if (score < best_score) {
        best_mode = mode;
        best_score = score;
      }
    }
    if (it->x_ == 0 || it->y_ == 0) {
      // avoid starting a checkerboard resonance from the border. See bug #432.
      if (IsFlatSource16(src)) {
        best_mode = (it->x_ == 0) ? 0 : 2;
        try_both_modes = 0;  // stick to i16
      }
    }
    VP8SetIntra16Mode(it, best_mode);
    // we'll reconstruct later, if i16 mode actually gets selected
  }

  // Next, evaluate Intra4
  if (try_both_modes || !is_i16) {
    // We don't evaluate the rate here, but just account for it through a
    // constant penalty (i4 mode usually needs more bits compared to i16).
    is_i16 = 0;
    VP8IteratorStartI4(it);
    do {
      int best_i4_mode = -1;
      score_t best_i4_score = MAX_COST;
      const uint8_t* const src = it->yuv_in_ + Y_OFF_ENC + VP8Scan[it->i4_];
      const uint16_t* const mode_costs = GetCostModeI4(it, rd->modes_i4);

      MakeIntra4Preds(it);
      for (mode = 0; mode < NUM_BMODES; ++mode) {
        const uint8_t* const ref = it->yuv_p_ + VP8I4ModeOffsets[mode];
        const score_t score = VP8SSE4x4(src, ref) * RD_DISTO_MULT
                            + mode_costs[mode] * lambda_d_i4;
        if (score < best_i4_score) {
          best_i4_mode = mode;
          best_i4_score = score;
        }
      }
      i4_bit_sum += mode_costs[best_i4_mode];
      rd->modes_i4[it->i4_] = best_i4_mode;
      score_i4 += best_i4_score;
      if (score_i4 >= best_score || i4_bit_sum > bit_limit) {
        // Intra4 won't be better than Intra16. Bail out and pick Intra16.
        is_i16 = 1;
        break;
      } else {  // reconstruct partial block inside yuv_out2_ buffer
        uint8_t* const tmp_dst = it->yuv_out2_ + Y_OFF_ENC + VP8Scan[it->i4_];
        nz |= ReconstructIntra4(it, rd->y_ac_levels[it->i4_],
                                src, tmp_dst, best_i4_mode) << it->i4_;
      }
    } while (VP8IteratorRotateI4(it, it->yuv_out2_ + Y_OFF_ENC));
  }

  // Final reconstruction, depending on which mode is selected.
  if (!is_i16) {
    VP8SetIntra4Mode(it, rd->modes_i4);
    SwapOut(it);
    best_score = score_i4;
  } else {
    nz = ReconstructIntra16(it, rd, it->yuv_out_ + Y_OFF_ENC, it->preds_[0]);
  }

  // ... and UV!
  if (refine_uv_mode) {
    int best_mode = -1;
    score_t best_uv_score = MAX_COST;
    const uint8_t* const src = it->yuv_in_ + U_OFF_ENC;
    for (mode = 0; mode < NUM_PRED_MODES; ++mode) {
      const uint8_t* const ref = it->yuv_p_ + VP8UVModeOffsets[mode];
      const score_t score = VP8SSE16x8(src, ref) * RD_DISTO_MULT
                          + VP8FixedCostsUV[mode] * lambda_d_uv;
      if (score < best_uv_score) {
        best_mode = mode;
        best_uv_score = score;
      }
    }
    VP8SetIntraUVMode(it, best_mode);
  }
  nz |= ReconstructUV(it, rd, it->yuv_out_ + U_OFF_ENC, it->mb_->uv_mode_);

  rd->nz = nz;
  rd->score = best_score;
}

//------------------------------------------------------------------------------
// Entry point

int VP8Decimate(VP8EncIterator* WEBP_RESTRICT const it,
                VP8ModeScore* WEBP_RESTRICT const rd,
                VP8RDLevel rd_opt) {
  int is_skipped;
  const int method = it->enc_->method_;

  InitScore(rd);

  // We can perform predictions for Luma16x16 and Chroma8x8 already.
  // Luma4x4 predictions needs to be done as-we-go.
  VP8MakeLuma16Preds(it);
  VP8MakeChroma8Preds(it);

  if (rd_opt > RD_OPT_NONE) {
    it->do_trellis_ = (rd_opt >= RD_OPT_TRELLIS_ALL);
    PickBestIntra16(it, rd);
    if (method >= 2) {
      PickBestIntra4(it, rd);
    }
    PickBestUV(it, rd);
    if (rd_opt == RD_OPT_TRELLIS) {   // finish off with trellis-optim now
      it->do_trellis_ = 1;
      SimpleQuantize(it, rd);
    }
  } else {
    // At this point we have heuristically decided intra16 / intra4.
    // For method >= 2, pick the best intra4/intra16 based on SSE (~tad slower).
    // For method <= 1, we don't re-examine the decision but just go ahead with
    // quantization/reconstruction.
    RefineUsingDistortion(it, (method >= 2), (method >= 1), rd);
  }
  is_skipped = (rd->nz == 0);
  VP8SetSkip(it, is_skipped);
  return is_skipped;
}
