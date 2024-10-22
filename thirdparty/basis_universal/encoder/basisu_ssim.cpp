// basisu_ssim.cpp
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
#include "basisu_ssim.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace basisu
{
	float gauss(int x, int y, float sigma_sqr)
	{
		float pow = expf(-((x * x + y * y) / (2.0f * sigma_sqr)));
		float g = (1.0f / (sqrtf((float)(2.0f * M_PI * sigma_sqr)))) * pow;
		return g;
	}
		
	// size_x/y should be odd
	void compute_gaussian_kernel(float *pDst, int size_x, int size_y, float sigma_sqr, uint32_t flags)
	{
		assert(size_x & size_y & 1);

		if (!(size_x | size_y))
			return;

		int mid_x = size_x / 2;
		int mid_y = size_y / 2;

		double sum = 0;
		for (int x = 0; x < size_x; x++)
		{
			for (int y = 0; y < size_y; y++)
			{
				float g;
				if ((x > mid_x) && (y < mid_y))
					g = pDst[(size_x - x - 1) + y * size_x];
				else if ((x < mid_x) && (y > mid_y))
					g = pDst[x + (size_y - y - 1) * size_x];
				else if ((x > mid_x) && (y > mid_y))
					g = pDst[(size_x - x - 1) + (size_y - y - 1) * size_x];
				else
					g = gauss(x - mid_x, y - mid_y, sigma_sqr);

				pDst[x + y * size_x] = g;
				sum += g;
			}
		}

		if (flags & cComputeGaussianFlagNormalizeCenterToOne)
		{
			sum = pDst[mid_x + mid_y * size_x];
		}

		if (flags & (cComputeGaussianFlagNormalizeCenterToOne | cComputeGaussianFlagNormalize))
		{
			double one_over_sum = 1.0f / sum;
			for (int i = 0; i < size_x * size_y; i++)
				pDst[i] = static_cast<float>(pDst[i] * one_over_sum);

			if (flags & cComputeGaussianFlagNormalizeCenterToOne)
				pDst[mid_x + mid_y * size_x] = 1.0f;
		}

		if (flags & cComputeGaussianFlagPrint)
		{
			printf("{\n");
			for (int y = 0; y < size_y; y++)
			{
				printf("  ");
				for (int x = 0; x < size_x; x++)
				{
					printf("%f, ", pDst[x + y * size_x]);
				}
				printf("\n");
			}
			printf("}");
		}
	}

	void gaussian_filter(imagef &dst, const imagef &orig_img, uint32_t odd_filter_width, float sigma_sqr, bool wrapping, uint32_t width_divisor, uint32_t height_divisor)
	{
		assert(odd_filter_width && (odd_filter_width & 1));
		odd_filter_width |= 1;

		vector2D<float> kernel(odd_filter_width, odd_filter_width);
		compute_gaussian_kernel(kernel.get_ptr(), odd_filter_width, odd_filter_width, sigma_sqr, cComputeGaussianFlagNormalize);

		const int dst_width = orig_img.get_width() / width_divisor;
		const int dst_height = orig_img.get_height() / height_divisor;

		const int H = odd_filter_width / 2;
		const int L = -H;

		dst.crop(dst_width, dst_height);

//#pragma omp parallel for
		for (int oy = 0; oy < dst_height; oy++)
		{
			for (int ox = 0; ox < dst_width; ox++)
			{
				vec4F c(0.0f);

				for (int yd = L; yd <= H; yd++)
				{
					int y = oy * height_divisor + (height_divisor >> 1) + yd;

					for (int xd = L; xd <= H; xd++)
					{
						int x = ox * width_divisor + (width_divisor >> 1) + xd;

						const vec4F &p = orig_img.get_clamped_or_wrapped(x, y, wrapping, wrapping);

						float w = kernel(xd + H, yd + H);
						c[0] += p[0] * w;
						c[1] += p[1] * w;
						c[2] += p[2] * w;
						c[3] += p[3] * w;
					}
				}

				dst(ox, oy).set(c[0], c[1], c[2], c[3]);
			}
		}
	}

	void pow_image(const imagef &src, imagef &dst, const vec4F &power)
	{
		dst.resize(src);

//#pragma omp parallel for
		for (int y = 0; y < (int)dst.get_height(); y++)
		{
			for (uint32_t x = 0; x < dst.get_width(); x++)
			{
				const vec4F &p = src(x, y);

				if ((power[0] == 2.0f) && (power[1] == 2.0f) && (power[2] == 2.0f) && (power[3] == 2.0f))
					dst(x, y).set(p[0] * p[0], p[1] * p[1], p[2] * p[2], p[3] * p[3]);
				else
					dst(x, y).set(powf(p[0], power[0]), powf(p[1], power[1]), powf(p[2], power[2]), powf(p[3], power[3]));
			}
		}
	}

	void mul_image(const imagef &src, imagef &dst, const vec4F &mul)
	{
		dst.resize(src);

//#pragma omp parallel for
		for (int y = 0; y < (int)dst.get_height(); y++)
		{
			for (uint32_t x = 0; x < dst.get_width(); x++)
			{
				const vec4F &p = src(x, y);
				dst(x, y).set(p[0] * mul[0], p[1] * mul[1], p[2] * mul[2], p[3] * mul[3]);
			}
		}
	}

	void scale_image(const imagef &src, imagef &dst, const vec4F &scale, const vec4F &shift)
	{
		dst.resize(src);

//#pragma omp parallel for
		for (int y = 0; y < (int)dst.get_height(); y++)
		{
			for (uint32_t x = 0; x < dst.get_width(); x++)
			{
				const vec4F &p = src(x, y);

				vec4F d;

				for (uint32_t c = 0; c < 4; c++)
					d[c] = scale[c] * p[c] + shift[c];

				dst(x, y).set(d[0], d[1], d[2], d[3]);
			}
		}
	}

	void add_weighted_image(const imagef &src1, const vec4F &alpha, const imagef &src2, const vec4F &beta, const vec4F &gamma, imagef &dst)
	{
		dst.resize(src1);

//#pragma omp parallel for
		for (int y = 0; y < (int)dst.get_height(); y++)
		{
			for (uint32_t x = 0; x < dst.get_width(); x++)
			{
				const vec4F &s1 = src1(x, y);
				const vec4F &s2 = src2(x, y);

				dst(x, y).set(
					s1[0] * alpha[0] + s2[0] * beta[0] + gamma[0],
					s1[1] * alpha[1] + s2[1] * beta[1] + gamma[1],
					s1[2] * alpha[2] + s2[2] * beta[2] + gamma[2],
					s1[3] * alpha[3] + s2[3] * beta[3] + gamma[3]);
			}
		}
	}

	void add_image(const imagef &src1, const imagef &src2, imagef &dst)
	{
		dst.resize(src1);

//#pragma omp parallel for
		for (int y = 0; y < (int)dst.get_height(); y++)
		{
			for (uint32_t x = 0; x < dst.get_width(); x++)
			{
				const vec4F &s1 = src1(x, y);
				const vec4F &s2 = src2(x, y);

				dst(x, y).set(s1[0] + s2[0], s1[1] + s2[1], s1[2] + s2[2], s1[3] + s2[3]);
			}
		}
	}

	void adds_image(const imagef &src, const vec4F &value, imagef &dst)
	{
		dst.resize(src);

//#pragma omp parallel for
		for (int y = 0; y < (int)dst.get_height(); y++)
		{
			for (uint32_t x = 0; x < dst.get_width(); x++)
			{
				const vec4F &p = src(x, y);

				dst(x, y).set(p[0] + value[0], p[1] + value[1], p[2] + value[2], p[3] + value[3]);
			}
		}
	}

	void mul_image(const imagef &src1, const imagef &src2, imagef &dst, const vec4F &scale)
	{
		dst.resize(src1);

//#pragma omp parallel for
		for (int y = 0; y < (int)dst.get_height(); y++)
		{
			for (uint32_t x = 0; x < dst.get_width(); x++)
			{
				const vec4F &s1 = src1(x, y);
				const vec4F &s2 = src2(x, y);

				vec4F d;

				for (uint32_t c = 0; c < 4; c++)
				{
					float v1 = s1[c];
					float v2 = s2[c];
					d[c] = v1 * v2 * scale[c];
				}

				dst(x, y) = d;
			}
		}
	}

	void div_image(const imagef &src1, const imagef &src2, imagef &dst, const vec4F &scale)
	{
		dst.resize(src1);

//#pragma omp parallel for
		for (int y = 0; y < (int)dst.get_height(); y++)
		{
			for (uint32_t x = 0; x < dst.get_width(); x++)
			{
				const vec4F &s1 = src1(x, y);
				const vec4F &s2 = src2(x, y);

				vec4F d;

				for (uint32_t c = 0; c < 4; c++)
				{
					float v = s2[c];
					if (v == 0.0f)
						d[c] = 0.0f;
					else
						d[c] = (s1[c] * scale[c]) / v;
				}

				dst(x, y) = d;
			}
		}
	}

	vec4F avg_image(const imagef &src)
	{
		vec4F avg(0.0f);

		for (uint32_t y = 0; y < src.get_height(); y++)
		{
			for (uint32_t x = 0; x < src.get_width(); x++)
			{
				const vec4F &s = src(x, y);

				avg += vec4F(s[0], s[1], s[2], s[3]);
			}
		}

		avg /= static_cast<float>(src.get_total_pixels());

		return avg;
	}
		
	// Reference: https://ece.uwaterloo.ca/~z70wang/research/ssim/index.html
	vec4F compute_ssim(const imagef &a, const imagef &b)
	{
		imagef axb, a_sq, b_sq, mu1, mu2, mu1_sq, mu2_sq, mu1_mu2, s1_sq, s2_sq, s12, smap, t1, t2, t3;

		const float C1 = 6.50250f, C2 = 58.52250f;
				
		pow_image(a, a_sq, vec4F(2));
		pow_image(b, b_sq, vec4F(2));
		mul_image(a, b, axb, vec4F(1.0f));

		gaussian_filter(mu1, a, 11, 1.5f * 1.5f);
		gaussian_filter(mu2, b, 11, 1.5f * 1.5f);

		pow_image(mu1, mu1_sq, vec4F(2));
		pow_image(mu2, mu2_sq, vec4F(2));
		mul_image(mu1, mu2, mu1_mu2, vec4F(1.0f));

		gaussian_filter(s1_sq, a_sq, 11, 1.5f * 1.5f);
		add_weighted_image(s1_sq, vec4F(1), mu1_sq, vec4F(-1), vec4F(0), s1_sq);

		gaussian_filter(s2_sq, b_sq, 11, 1.5f * 1.5f);
		add_weighted_image(s2_sq, vec4F(1), mu2_sq, vec4F(-1), vec4F(0), s2_sq);

		gaussian_filter(s12, axb, 11, 1.5f * 1.5f);
		add_weighted_image(s12, vec4F(1), mu1_mu2, vec4F(-1), vec4F(0), s12);

		scale_image(mu1_mu2, t1, vec4F(2), vec4F(0));
		adds_image(t1, vec4F(C1), t1);

		scale_image(s12, t2, vec4F(2), vec4F(0));
		adds_image(t2, vec4F(C2), t2);

		mul_image(t1, t2, t3, vec4F(1));

		add_image(mu1_sq, mu2_sq, t1);
		adds_image(t1, vec4F(C1), t1);

		add_image(s1_sq, s2_sq, t2);
		adds_image(t2, vec4F(C2), t2);

		mul_image(t1, t2, t1, vec4F(1));

		div_image(t3, t1, smap, vec4F(1));

		return avg_image(smap);
	}

	vec4F compute_ssim(const image &a, const image &b, bool luma, bool luma_601)
	{
		image ta(a), tb(b);

		if ((ta.get_width() != tb.get_width()) || (ta.get_height() != tb.get_height()))
		{
			debug_printf("compute_ssim: Cropping input images to equal dimensions\n");

			const uint32_t w = minimum(a.get_width(), b.get_width());
			const uint32_t h = minimum(a.get_height(), b.get_height());
			ta.crop(w, h);
			tb.crop(w, h);
		}

		if (!ta.get_width() || !ta.get_height())
		{
			assert(0);
			return vec4F(0);
		}

		if (luma)
		{
			for (uint32_t y = 0; y < ta.get_height(); y++)
			{
				for (uint32_t x = 0; x < ta.get_width(); x++)
				{
					ta(x, y).set(ta(x, y).get_luma(luma_601), ta(x, y).a);
					tb(x, y).set(tb(x, y).get_luma(luma_601), tb(x, y).a);
				}
			}
		}

		imagef fta, ftb;

		fta.set(ta);
		ftb.set(tb);

		return compute_ssim(fta, ftb);
	}

} // namespace basisu
