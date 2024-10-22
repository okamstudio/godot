// basisu_resampler_filters.cpp
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
#include "basisu_resampler_filters.h"

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

namespace basisu
{
#define BOX_FILTER_SUPPORT (0.5f)
	static float box_filter(float t) /* pulse/Fourier window */
	{
		// make_clist() calls the filter function with t inverted (pos = left, neg = right)
		if ((t >= -0.5f) && (t < 0.5f))
			return 1.0f;
		else
			return 0.0f;
	}

#define TENT_FILTER_SUPPORT (1.0f)
	static float tent_filter(float t) /* box (*) box, bilinear/triangle */
	{
		if (t < 0.0f)
			t = -t;

		if (t < 1.0f)
			return 1.0f - t;
		else
			return 0.0f;
	}

#define BELL_SUPPORT (1.5f)
	static float bell_filter(float t) /* box (*) box (*) box */
	{
		if (t < 0.0f)
			t = -t;

		if (t < .5f)
			return (.75f - (t * t));

		if (t < 1.5f)
		{
			t = (t - 1.5f);
			return (.5f * (t * t));
		}

		return (0.0f);
	}

#define B_SPLINE_SUPPORT (2.0f)
	static float B_spline_filter(float t) /* box (*) box (*) box (*) box */
	{
		float tt;

		if (t < 0.0f)
			t = -t;

		if (t < 1.0f)
		{
			tt = t * t;
			return ((.5f * tt * t) - tt + (2.0f / 3.0f));
		}
		else if (t < 2.0f)
		{
			t = 2.0f - t;
			return ((1.0f / 6.0f) * (t * t * t));
		}

		return (0.0f);
	}

	// Dodgson, N., "Quadratic Interpolation for Image Resampling"
#define QUADRATIC_SUPPORT 1.5f
	static float quadratic(float t, const float R)
	{
		if (t < 0.0f)
			t = -t;
		if (t < QUADRATIC_SUPPORT)
		{
			float tt = t * t;
			if (t <= .5f)
				return (-2.0f * R) * tt + .5f * (R + 1.0f);
			else
				return (R * tt) + (-2.0f * R - .5f) * t + (3.0f / 4.0f) * (R + 1.0f);
		}
		else
			return 0.0f;
	}

	static float quadratic_interp_filter(float t)
	{
		return quadratic(t, 1.0f);
	}

	static float quadratic_approx_filter(float t)
	{
		return quadratic(t, .5f);
	}

	static float quadratic_mix_filter(float t)
	{
		return quadratic(t, .8f);
	}

	// Mitchell, D. and A. Netravali, "Reconstruction Filters in Computer Graphics."
	// Computer Graphics, Vol. 22, No. 4, pp. 221-228.
	// (B, C)
	// (1/3, 1/3)  - Defaults recommended by Mitchell and Netravali
	// (1, 0)	   - Equivalent to the Cubic B-Spline
	// (0, 0.5)		- Equivalent to the Catmull-Rom Spline
	// (0, C)		- The family of Cardinal Cubic Splines
	// (B, 0)		- Duff's tensioned B-Splines.
	static float mitchell(float t, const float B, const float C)
	{
		float tt;

		tt = t * t;

		if (t < 0.0f)
			t = -t;

		if (t < 1.0f)
		{
			t = (((12.0f - 9.0f * B - 6.0f * C) * (t * tt)) + ((-18.0f + 12.0f * B + 6.0f * C) * tt) + (6.0f - 2.0f * B));

			return (t / 6.0f);
		}
		else if (t < 2.0f)
		{
			t = (((-1.0f * B - 6.0f * C) * (t * tt)) + ((6.0f * B + 30.0f * C) * tt) + ((-12.0f * B - 48.0f * C) * t) + (8.0f * B + 24.0f * C));

			return (t / 6.0f);
		}

		return (0.0f);
	}

#define MITCHELL_SUPPORT (2.0f)
	static float mitchell_filter(float t)
	{
		return mitchell(t, 1.0f / 3.0f, 1.0f / 3.0f);
	}

#define CATMULL_ROM_SUPPORT (2.0f)
	static float catmull_rom_filter(float t)
	{
		return mitchell(t, 0.0f, .5f);
	}

	static double sinc(double x)
	{
		x = (x * M_PI);

		if ((x < 0.01f) && (x > -0.01f))
			return 1.0f + x * x * (-1.0f / 6.0f + x * x * 1.0f / 120.0f);

		return sin(x) / x;
	}

	static float clean(double t)
	{
		const float EPSILON = .0000125f;
		if (fabs(t) < EPSILON)
			return 0.0f;
		return (float)t;
	}

	//static double blackman_window(double x)
	//{
	//	return .42f + .50f * cos(M_PI*x) + .08f * cos(2.0f*M_PI*x);
	//}

	static double blackman_exact_window(double x)
	{
		return 0.42659071f + 0.49656062f * cos(M_PI * x) + 0.07684867f * cos(2.0f * M_PI * x);
	}

#define BLACKMAN_SUPPORT (3.0f)
	static float blackman_filter(float t)
	{
		if (t < 0.0f)
			t = -t;

		if (t < 3.0f)
			//return clean(sinc(t) * blackman_window(t / 3.0f));
			return clean(sinc(t) * blackman_exact_window(t / 3.0f));
		else
			return (0.0f);
	}

#define GAUSSIAN_SUPPORT (1.25f)
	static float gaussian_filter(float t) // with blackman window
	{
		if (t < 0)
			t = -t;
		if (t < GAUSSIAN_SUPPORT)
			return clean(exp(-2.0f * t * t) * sqrt(2.0f / M_PI) * blackman_exact_window(t / GAUSSIAN_SUPPORT));
		else
			return 0.0f;
	}

	// Windowed sinc -- see "Jimm Blinn's Corner: Dirty Pixels" pg. 26.
#define LANCZOS3_SUPPORT (3.0f)
	static float lanczos3_filter(float t)
	{
		if (t < 0.0f)
			t = -t;

		if (t < 3.0f)
			return clean(sinc(t) * sinc(t / 3.0f));
		else
			return (0.0f);
	}

#define LANCZOS4_SUPPORT (4.0f)
	static float lanczos4_filter(float t)
	{
		if (t < 0.0f)
			t = -t;

		if (t < 4.0f)
			return clean(sinc(t) * sinc(t / 4.0f));
		else
			return (0.0f);
	}

#define LANCZOS6_SUPPORT (6.0f)
	static float lanczos6_filter(float t)
	{
		if (t < 0.0f)
			t = -t;

		if (t < 6.0f)
			return clean(sinc(t) * sinc(t / 6.0f));
		else
			return (0.0f);
	}

#define LANCZOS12_SUPPORT (12.0f)
	static float lanczos12_filter(float t)
	{
		if (t < 0.0f)
			t = -t;

		if (t < 12.0f)
			return clean(sinc(t) * sinc(t / 12.0f));
		else
			return (0.0f);
	}

	static double bessel0(double x)
	{
		const double EPSILON_RATIO = 1E-16;
		double xh, sum, pow, ds;
		int k;

		xh = 0.5 * x;
		sum = 1.0;
		pow = 1.0;
		k = 0;
		ds = 1.0;
		while (ds > sum * EPSILON_RATIO) // FIXME: Shouldn't this stop after X iterations for max. safety?
		{
			++k;
			pow = pow * (xh / k);
			ds = pow * pow;
			sum = sum + ds;
		}

		return sum;
	}

	//static const float KAISER_ALPHA = 4.0;
	static double kaiser(double alpha, double half_width, double x)
	{
		const double ratio = (x / half_width);
		return bessel0(alpha * sqrt(1 - ratio * ratio)) / bessel0(alpha);
	}

#define KAISER_SUPPORT 3
	static float kaiser_filter(float t)
	{
		if (t < 0.0f)
			t = -t;

		if (t < KAISER_SUPPORT)
		{
			// db atten
			const float att = 40.0f;
			const float alpha = (float)(exp(log((double)0.58417 * (att - 20.96)) * 0.4) + 0.07886 * (att - 20.96));
			//const float alpha = KAISER_ALPHA;
			return (float)clean(sinc(t) * kaiser(alpha, KAISER_SUPPORT, t));
		}

		return 0.0f;
	}

	const resample_filter g_resample_filters[] =
	{
		{ "box", box_filter, BOX_FILTER_SUPPORT }, 
		{ "tent", tent_filter, TENT_FILTER_SUPPORT }, 
		{ "bell", bell_filter, BELL_SUPPORT }, 
		{ "b-spline", B_spline_filter, B_SPLINE_SUPPORT },
		{ "mitchell", mitchell_filter, MITCHELL_SUPPORT }, 
		{ "blackman", blackman_filter, BLACKMAN_SUPPORT }, 
		{ "lanczos3", lanczos3_filter, LANCZOS3_SUPPORT },
		{ "lanczos4", lanczos4_filter, LANCZOS4_SUPPORT },
		{ "lanczos6", lanczos6_filter, LANCZOS6_SUPPORT }, 
		{ "lanczos12", lanczos12_filter, LANCZOS12_SUPPORT }, 
		{ "kaiser", kaiser_filter, KAISER_SUPPORT }, 
		{ "gaussian", gaussian_filter, GAUSSIAN_SUPPORT },
		{ "catmullrom", catmull_rom_filter, CATMULL_ROM_SUPPORT }, 
		{ "quadratic_interp", quadratic_interp_filter, QUADRATIC_SUPPORT }, 
		{ "quadratic_approx", quadratic_approx_filter, QUADRATIC_SUPPORT }, 
		{ "quadratic_mix", quadratic_mix_filter, QUADRATIC_SUPPORT },
	};

	const int g_num_resample_filters = BASISU_ARRAY_SIZE(g_resample_filters);

	int find_resample_filter(const char *pName)
	{
		for (int i = 0; i < g_num_resample_filters; i++)
			if (strcmp(pName, g_resample_filters[i].name) == 0)
				return i;
		return -1;
	}
} // namespace basisu
