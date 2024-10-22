// basisu_kernels_declares.h
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if BASISU_SUPPORT_SSE
void CPPSPMD_NAME(perceptual_distance_rgb_4_N)(int64_t* pDistance, const uint8_t* pSelectors, const basisu::color_rgba* pBlock_colors, const basisu::color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_err);
void CPPSPMD_NAME(linear_distance_rgb_4_N)(int64_t* pDistance, const uint8_t* pSelectors, const basisu::color_rgba* pBlock_colors, const basisu::color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_err);

void CPPSPMD_NAME(find_selectors_perceptual_rgb_4_N)(int64_t* pDistance, uint8_t* pSelectors, const basisu::color_rgba* pBlock_colors, const basisu::color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_err);
void CPPSPMD_NAME(find_selectors_linear_rgb_4_N)(int64_t* pDistance, uint8_t* pSelectors, const basisu::color_rgba* pBlock_colors, const basisu::color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_err);

void CPPSPMD_NAME(find_lowest_error_perceptual_rgb_4_N)(int64_t* pDistance, const basisu::color_rgba* pBlock_colors, const basisu::color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_error);
void CPPSPMD_NAME(find_lowest_error_linear_rgb_4_N)(int64_t* pDistance, const basisu::color_rgba* pBlock_colors, const basisu::color_rgba* pSrc_pixels, uint32_t n, int64_t early_out_error);

void CPPSPMD_NAME(update_covar_matrix_16x16)(uint32_t num_vecs, const void* pWeighted_vecs, const void *pOrigin, const uint32_t* pVec_indices, void *pMatrix16x16);
#endif
