// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Coding tools configuration
//
// Author: Skal (pascal.massimino@gmail.com)

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#include "src/webp/encode.h"

//------------------------------------------------------------------------------
// WebPConfig
//------------------------------------------------------------------------------

int WebPConfigInitInternal(WebPConfig* config,
                           WebPPreset preset, float quality, int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_ENCODER_ABI_VERSION)) {
    return 0;   // caller/system version mismatch!
  }
  if (config == NULL) return 0;

  config->quality = quality;
  config->target_size = 0;
  config->target_PSNR = 0.;
  config->method = 4;
  config->sns_strength = 50;
  config->filter_strength = 60;   // mid-filtering
  config->filter_sharpness = 0;
  config->filter_type = 1;        // default: strong (so U/V is filtered too)
  config->partitions = 0;
  config->segments = 4;
  config->pass = 1;
  config->qmin = 0;
  config->qmax = 100;
  config->show_compressed = 0;
  config->preprocessing = 0;
  config->autofilter = 0;
  config->partition_limit = 0;
  config->alpha_compression = 1;
  config->alpha_filtering = 1;
  config->alpha_quality = 100;
  config->lossless = 0;
  config->exact = 0;
  config->image_hint = WEBP_HINT_DEFAULT;
  config->emulate_jpeg_size = 0;
  config->thread_level = 0;
  config->low_memory = 0;
  config->near_lossless = 100;
  config->use_sharp_yuv = 0;

  // TODO(skal): tune.
  switch (preset) {
    case WEBP_PRESET_PICTURE:
      config->sns_strength = 80;
      config->filter_sharpness = 4;
      config->filter_strength = 35;
      config->preprocessing &= ~2;   // no dithering
      break;
    case WEBP_PRESET_PHOTO:
      config->sns_strength = 80;
      config->filter_sharpness = 3;
      config->filter_strength = 30;
      config->preprocessing |= 2;
      break;
    case WEBP_PRESET_DRAWING:
      config->sns_strength = 25;
      config->filter_sharpness = 6;
      config->filter_strength = 10;
      break;
    case WEBP_PRESET_ICON:
      config->sns_strength = 0;
      config->filter_strength = 0;   // disable filtering to retain sharpness
      config->preprocessing &= ~2;   // no dithering
      break;
    case WEBP_PRESET_TEXT:
      config->sns_strength = 0;
      config->filter_strength = 0;   // disable filtering to retain sharpness
      config->preprocessing &= ~2;   // no dithering
      config->segments = 2;
      break;
    case WEBP_PRESET_DEFAULT:
    default:
      break;
  }
  return WebPValidateConfig(config);
}

int WebPValidateConfig(const WebPConfig* config) {
  if (config == NULL) return 0;
  if (config->quality < 0 || config->quality > 100) return 0;
  if (config->target_size < 0) return 0;
  if (config->target_PSNR < 0) return 0;
  if (config->method < 0 || config->method > 6) return 0;
  if (config->segments < 1 || config->segments > 4) return 0;
  if (config->sns_strength < 0 || config->sns_strength > 100) return 0;
  if (config->filter_strength < 0 || config->filter_strength > 100) return 0;
  if (config->filter_sharpness < 0 || config->filter_sharpness > 7) return 0;
  if (config->filter_type < 0 || config->filter_type > 1) return 0;
  if (config->autofilter < 0 || config->autofilter > 1) return 0;
  if (config->pass < 1 || config->pass > 10) return 0;
  if (config->qmin < 0 || config->qmax > 100 || config->qmin > config->qmax) {
    return 0;
  }
  if (config->show_compressed < 0 || config->show_compressed > 1) return 0;
  if (config->preprocessing < 0 || config->preprocessing > 7) return 0;
  if (config->partitions < 0 || config->partitions > 3) return 0;
  if (config->partition_limit < 0 || config->partition_limit > 100) return 0;
  if (config->alpha_compression < 0) return 0;
  if (config->alpha_filtering < 0) return 0;
  if (config->alpha_quality < 0 || config->alpha_quality > 100) return 0;
  if (config->lossless < 0 || config->lossless > 1) return 0;
  if (config->near_lossless < 0 || config->near_lossless > 100) return 0;
  if (config->image_hint >= WEBP_HINT_LAST) return 0;
  if (config->emulate_jpeg_size < 0 || config->emulate_jpeg_size > 1) return 0;
  if (config->thread_level < 0 || config->thread_level > 1) return 0;
  if (config->low_memory < 0 || config->low_memory > 1) return 0;
  if (config->exact < 0 || config->exact > 1) return 0;
  if (config->use_sharp_yuv < 0 || config->use_sharp_yuv > 1) return 0;

  return 1;
}

//------------------------------------------------------------------------------

#define MAX_LEVEL 9

// Mapping between -z level and -m / -q parameter settings.
static const struct {
  uint8_t method_;
  uint8_t quality_;
} kLosslessPresets[MAX_LEVEL + 1] = {
  { 0,  0 }, { 1, 20 }, { 2, 25 }, { 3, 30 }, { 3, 50 },
  { 4, 50 }, { 4, 75 }, { 4, 90 }, { 5, 90 }, { 6, 100 }
};

int WebPConfigLosslessPreset(WebPConfig* config, int level) {
  if (config == NULL || level < 0 || level > MAX_LEVEL) return 0;
  config->lossless = 1;
  config->method = kLosslessPresets[level].method_;
  config->quality = kLosslessPresets[level].quality_;
  return 1;
}

//------------------------------------------------------------------------------
