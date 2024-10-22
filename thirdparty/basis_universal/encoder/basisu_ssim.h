// basisu_ssim.h
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
#pragma once
#include "basisu_enc.h"

namespace basisu
{
	float gauss(int x, int y, float sigma_sqr);

	enum
	{
		cComputeGaussianFlagNormalize = 1,
		cComputeGaussianFlagPrint = 2,
		cComputeGaussianFlagNormalizeCenterToOne = 4
	};

	void compute_gaussian_kernel(float *pDst, int size_x, int size_y, float sigma_sqr, uint32_t flags = 0);

	void scale_image(const imagef &src, imagef &dst, const vec4F &scale, const vec4F &shift);
	void add_weighted_image(const imagef &src1, const vec4F &alpha, const imagef &src2, const vec4F &beta, const vec4F &gamma, imagef &dst);
	void add_image(const imagef &src1, const imagef &src2, imagef &dst);
	void adds_image(const imagef &src, const vec4F &value, imagef &dst);
	void mul_image(const imagef &src1, const imagef &src2, imagef &dst, const vec4F &scale);
	void div_image(const imagef &src1, const imagef &src2, imagef &dst, const vec4F &scale);
	vec4F avg_image(const imagef &src);

	void gaussian_filter(imagef &dst, const imagef &orig_img, uint32_t odd_filter_width, float sigma_sqr, bool wrapping = false, uint32_t width_divisor = 1, uint32_t height_divisor = 1);

	vec4F compute_ssim(const imagef &a, const imagef &b);
	vec4F compute_ssim(const image &a, const image &b, bool luma, bool luma_601);

} // namespace basisu
