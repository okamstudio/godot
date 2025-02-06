/**************************************************************************/
/*  math_funcs.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/error/error_macros.h"
#include "core/math/math_defs.h"
#include "core/math/random_pcg.h"
#include "core/typedefs.h"

#include "thirdparty/misc/pcg.h"

#include <float.h>
#include <math.h>

class Math {
	static RandomPCG default_rand;

public:
	Math() {} // useless to instance

	// Not using 'RANDOM_MAX' to avoid conflict with system headers on some OSes (at least NetBSD).
	static const uint64_t RANDOM_32BIT_MAX = 0xFFFFFFFF;

	static _ALWAYS_INLINE_ double sin(double p_x) { return ::sin(p_x); }
	static _ALWAYS_INLINE_ float sin(float p_x) { return ::sinf(p_x); }

	static _ALWAYS_INLINE_ double cos(double p_x) { return ::cos(p_x); }
	static _ALWAYS_INLINE_ float cos(float p_x) { return ::cosf(p_x); }

	static _ALWAYS_INLINE_ double tan(double p_x) { return ::tan(p_x); }
	static _ALWAYS_INLINE_ float tan(float p_x) { return ::tanf(p_x); }

	static _ALWAYS_INLINE_ double sinh(double p_x) { return ::sinh(p_x); }
	static _ALWAYS_INLINE_ float sinh(float p_x) { return ::sinhf(p_x); }

	static _ALWAYS_INLINE_ float sinc(float p_x) { return p_x == 0 ? 1 : ::sin(p_x) / p_x; }
	static _ALWAYS_INLINE_ double sinc(double p_x) { return p_x == 0 ? 1 : ::sin(p_x) / p_x; }

	static _ALWAYS_INLINE_ float sincn(float p_x) { return sinc((float)Math_PI * p_x); }
	static _ALWAYS_INLINE_ double sincn(double p_x) { return sinc(Math_PI * p_x); }

	static _ALWAYS_INLINE_ double cosh(double p_x) { return ::cosh(p_x); }
	static _ALWAYS_INLINE_ float cosh(float p_x) { return ::coshf(p_x); }

	static _ALWAYS_INLINE_ double tanh(double p_x) { return ::tanh(p_x); }
	static _ALWAYS_INLINE_ float tanh(float p_x) { return ::tanhf(p_x); }

	// Always does clamping so always safe to use.
	static _ALWAYS_INLINE_ double asin(double p_x) { return p_x < -1 ? (-Math_PI / 2) : (p_x > 1 ? (Math_PI / 2) : ::asin(p_x)); }
	static _ALWAYS_INLINE_ float asin(float p_x) { return p_x < -1 ? (-Math_PI / 2) : (p_x > 1 ? (Math_PI / 2) : ::asinf(p_x)); }

	// Always does clamping so always safe to use.
	static _ALWAYS_INLINE_ double acos(double p_x) { return p_x < -1 ? Math_PI : (p_x > 1 ? 0 : ::acos(p_x)); }
	static _ALWAYS_INLINE_ float acos(float p_x) { return p_x < -1 ? Math_PI : (p_x > 1 ? 0 : ::acosf(p_x)); }

	static _ALWAYS_INLINE_ double atan(double p_x) { return ::atan(p_x); }
	static _ALWAYS_INLINE_ float atan(float p_x) { return ::atanf(p_x); }

	static _ALWAYS_INLINE_ double atan2(double p_y, double p_x) { return ::atan2(p_y, p_x); }
	static _ALWAYS_INLINE_ float atan2(float p_y, float p_x) { return ::atan2f(p_y, p_x); }

	static _ALWAYS_INLINE_ double asinh(double p_x) { return ::asinh(p_x); }
	static _ALWAYS_INLINE_ float asinh(float p_x) { return ::asinhf(p_x); }

	// Always does clamping so always safe to use.
	static _ALWAYS_INLINE_ double acosh(double p_x) { return p_x < 1 ? 0 : ::acosh(p_x); }
	static _ALWAYS_INLINE_ float acosh(float p_x) { return p_x < 1 ? 0 : ::acoshf(p_x); }

	// Always does clamping so always safe to use.
	static _ALWAYS_INLINE_ double atanh(double p_x) { return p_x <= -1 ? -INFINITY : (p_x >= 1 ? INFINITY : ::atanh(p_x)); }
	static _ALWAYS_INLINE_ float atanh(float p_x) { return p_x <= -1 ? -INFINITY : (p_x >= 1 ? INFINITY : ::atanhf(p_x)); }

	static _ALWAYS_INLINE_ double sqrt(double p_x) { return ::sqrt(p_x); }
	static _ALWAYS_INLINE_ float sqrt(float p_x) { return ::sqrtf(p_x); }

	static _ALWAYS_INLINE_ double fmod(double p_x, double p_y) { return ::fmod(p_x, p_y); }
	static _ALWAYS_INLINE_ float fmod(float p_x, float p_y) { return ::fmodf(p_x, p_y); }

	static _ALWAYS_INLINE_ double modf(double p_x, double *r_y) { return ::modf(p_x, r_y); }
	static _ALWAYS_INLINE_ float modf(float p_x, float *r_y) { return ::modff(p_x, r_y); }

	static _ALWAYS_INLINE_ double floor(double p_x) { return ::floor(p_x); }
	static _ALWAYS_INLINE_ float floor(float p_x) { return ::floorf(p_x); }

	static _ALWAYS_INLINE_ double ceil(double p_x) { return ::ceil(p_x); }
	static _ALWAYS_INLINE_ float ceil(float p_x) { return ::ceilf(p_x); }

	static _ALWAYS_INLINE_ double pow(double p_x, double p_y) { return ::pow(p_x, p_y); }
	static _ALWAYS_INLINE_ float pow(float p_x, float p_y) { return ::powf(p_x, p_y); }

	static _ALWAYS_INLINE_ double log(double p_x) { return ::log(p_x); }
	static _ALWAYS_INLINE_ float log(float p_x) { return ::logf(p_x); }

	static _ALWAYS_INLINE_ double log1p(double p_x) { return ::log1p(p_x); }
	static _ALWAYS_INLINE_ float log1p(float p_x) { return ::log1pf(p_x); }

	static _ALWAYS_INLINE_ double log2(double p_x) { return ::log2(p_x); }
	static _ALWAYS_INLINE_ float log2(float p_x) { return ::log2f(p_x); }

	static _ALWAYS_INLINE_ double exp(double p_x) { return ::exp(p_x); }
	static _ALWAYS_INLINE_ float exp(float p_x) { return ::expf(p_x); }

	static _ALWAYS_INLINE_ bool is_nan(double p_val) {
#ifdef _MSC_VER
		return _isnan(p_val);
#elif defined(__GNUC__) && __GNUC__ < 6
		union {
			uint64_t u;
			double f;
		} ieee754;
		ieee754.f = p_val;
		// (unsigned)(0x7ff0000000000001 >> 32) : 0x7ff00000
		return ((((unsigned)(ieee754.u >> 32) & 0x7fffffff) + ((unsigned)ieee754.u != 0)) > 0x7ff00000);
#else
		return isnan(p_val);
#endif
	}

	static _ALWAYS_INLINE_ bool is_nan(float p_val) {
#ifdef _MSC_VER
		return _isnan(p_val);
#elif defined(__GNUC__) && __GNUC__ < 6
		union {
			uint32_t u;
			float f;
		} ieee754;
		ieee754.f = p_val;
		// -----------------------------------
		// (single-precision floating-point)
		// NaN : s111 1111 1xxx xxxx xxxx xxxx xxxx xxxx
		//     : (> 0x7f800000)
		// where,
		//   s : sign
		//   x : non-zero number
		// -----------------------------------
		return ((ieee754.u & 0x7fffffff) > 0x7f800000);
#else
		return isnan(p_val);
#endif
	}

	static _ALWAYS_INLINE_ bool is_inf(double p_val) {
#ifdef _MSC_VER
		return !_finite(p_val);
// use an inline implementation of isinf as a workaround for problematic libstdc++ versions from gcc 5.x era
#elif defined(__GNUC__) && __GNUC__ < 6
		union {
			uint64_t u;
			double f;
		} ieee754;
		ieee754.f = p_val;
		return ((unsigned)(ieee754.u >> 32) & 0x7fffffff) == 0x7ff00000 &&
				((unsigned)ieee754.u == 0);
#else
		return isinf(p_val);
#endif
	}

	static _ALWAYS_INLINE_ bool is_inf(float p_val) {
#ifdef _MSC_VER
		return !_finite(p_val);
// use an inline implementation of isinf as a workaround for problematic libstdc++ versions from gcc 5.x era
#elif defined(__GNUC__) && __GNUC__ < 6
		union {
			uint32_t u;
			float f;
		} ieee754;
		ieee754.f = p_val;
		return (ieee754.u & 0x7fffffff) == 0x7f800000;
#else
		return isinf(p_val);
#endif
	}

	// These methods assume (p_num + p_den) doesn't overflow.
	static _ALWAYS_INLINE_ int32_t division_round_up(int32_t p_num, int32_t p_den) {
		int32_t offset = (p_num < 0 && p_den < 0) ? 1 : -1;
		return (p_num + p_den + offset) / p_den;
	}
	static _ALWAYS_INLINE_ uint32_t division_round_up(uint32_t p_num, uint32_t p_den) {
		return (p_num + p_den - 1) / p_den;
	}
	static _ALWAYS_INLINE_ int64_t division_round_up(int64_t p_num, int64_t p_den) {
		int32_t offset = (p_num < 0 && p_den < 0) ? 1 : -1;
		return (p_num + p_den + offset) / p_den;
	}
	static _ALWAYS_INLINE_ uint64_t division_round_up(uint64_t p_num, uint64_t p_den) {
		return (p_num + p_den - 1) / p_den;
	}

	static _ALWAYS_INLINE_ bool is_finite(double p_val) { return isfinite(p_val); }
	static _ALWAYS_INLINE_ bool is_finite(float p_val) { return isfinite(p_val); }

	static _ALWAYS_INLINE_ double abs(double g) { return absd(g); }
	static _ALWAYS_INLINE_ float abs(float g) { return absf(g); }
	static _ALWAYS_INLINE_ int abs(int g) { return g > 0 ? g : -g; }

	static _ALWAYS_INLINE_ double fposmod(double p_x, double p_y) {
		double value = Math::fmod(p_x, p_y);
		if (((value < 0) && (p_y > 0)) || ((value > 0) && (p_y < 0))) {
			value += p_y;
		}
		value += 0.0;
		return value;
	}
	static _ALWAYS_INLINE_ float fposmod(float p_x, float p_y) {
		float value = Math::fmod(p_x, p_y);
		if (((value < 0) && (p_y > 0)) || ((value > 0) && (p_y < 0))) {
			value += p_y;
		}
		value += 0.0f;
		return value;
	}
	static _ALWAYS_INLINE_ float fposmodp(float p_x, float p_y) {
		float value = Math::fmod(p_x, p_y);
		if (value < 0) {
			value += p_y;
		}
		value += 0.0f;
		return value;
	}
	static _ALWAYS_INLINE_ double fposmodp(double p_x, double p_y) {
		double value = Math::fmod(p_x, p_y);
		if (value < 0) {
			value += p_y;
		}
		value += 0.0;
		return value;
	}

	static _ALWAYS_INLINE_ int64_t posmod(int64_t p_x, int64_t p_y) {
		ERR_FAIL_COND_V_MSG(p_y == 0, 0, "Division by zero in posmod is undefined. Returning 0 as fallback.");
		int64_t value = p_x % p_y;
		if (((value < 0) && (p_y > 0)) || ((value > 0) && (p_y < 0))) {
			value += p_y;
		}
		return value;
	}

	static _ALWAYS_INLINE_ double deg_to_rad(double p_y) { return p_y * (Math_PI / 180.0); }
	static _ALWAYS_INLINE_ float deg_to_rad(float p_y) { return p_y * (float)(Math_PI / 180.0); }

	static _ALWAYS_INLINE_ double rad_to_deg(double p_y) { return p_y * (180.0 / Math_PI); }
	static _ALWAYS_INLINE_ float rad_to_deg(float p_y) { return p_y * (float)(180.0 / Math_PI); }

	static _ALWAYS_INLINE_ double lerp(double p_from, double p_to, double p_weight) { return p_from + (p_to - p_from) * p_weight; }
	static _ALWAYS_INLINE_ float lerp(float p_from, float p_to, float p_weight) { return p_from + (p_to - p_from) * p_weight; }

	static _ALWAYS_INLINE_ double cubic_interpolate(double p_from, double p_to, double p_pre, double p_post, double p_weight) {
		return 0.5 *
				((p_from * 2.0) +
						(-p_pre + p_to) * p_weight +
						(2.0 * p_pre - 5.0 * p_from + 4.0 * p_to - p_post) * (p_weight * p_weight) +
						(-p_pre + 3.0 * p_from - 3.0 * p_to + p_post) * (p_weight * p_weight * p_weight));
	}
	static _ALWAYS_INLINE_ float cubic_interpolate(float p_from, float p_to, float p_pre, float p_post, float p_weight) {
		return 0.5f *
				((p_from * 2.0f) +
						(-p_pre + p_to) * p_weight +
						(2.0f * p_pre - 5.0f * p_from + 4.0f * p_to - p_post) * (p_weight * p_weight) +
						(-p_pre + 3.0f * p_from - 3.0f * p_to + p_post) * (p_weight * p_weight * p_weight));
	}

	static _ALWAYS_INLINE_ double cubic_interpolate_angle(double p_from, double p_to, double p_pre, double p_post, double p_weight) {
		double from_rot = fmod(p_from, Math_TAU);

		double pre_diff = fmod(p_pre - from_rot, Math_TAU);
		double pre_rot = from_rot + fmod(2.0 * pre_diff, Math_TAU) - pre_diff;

		double to_diff = fmod(p_to - from_rot, Math_TAU);
		double to_rot = from_rot + fmod(2.0 * to_diff, Math_TAU) - to_diff;

		double post_diff = fmod(p_post - to_rot, Math_TAU);
		double post_rot = to_rot + fmod(2.0 * post_diff, Math_TAU) - post_diff;

		return cubic_interpolate(from_rot, to_rot, pre_rot, post_rot, p_weight);
	}

	static _ALWAYS_INLINE_ float cubic_interpolate_angle(float p_from, float p_to, float p_pre, float p_post, float p_weight) {
		float from_rot = fmod(p_from, (float)Math_TAU);

		float pre_diff = fmod(p_pre - from_rot, (float)Math_TAU);
		float pre_rot = from_rot + fmod(2.0f * pre_diff, (float)Math_TAU) - pre_diff;

		float to_diff = fmod(p_to - from_rot, (float)Math_TAU);
		float to_rot = from_rot + fmod(2.0f * to_diff, (float)Math_TAU) - to_diff;

		float post_diff = fmod(p_post - to_rot, (float)Math_TAU);
		float post_rot = to_rot + fmod(2.0f * post_diff, (float)Math_TAU) - post_diff;

		return cubic_interpolate(from_rot, to_rot, pre_rot, post_rot, p_weight);
	}

	static _ALWAYS_INLINE_ double cubic_interpolate_in_time(double p_from, double p_to, double p_pre, double p_post, double p_weight,
			double p_to_t, double p_pre_t, double p_post_t) {
		/* Barry-Goldman method */
		double t = Math::lerp(0.0, p_to_t, p_weight);
		double a1 = Math::lerp(p_pre, p_from, p_pre_t == 0 ? 0.0 : (t - p_pre_t) / -p_pre_t);
		double a2 = Math::lerp(p_from, p_to, p_to_t == 0 ? 0.5 : t / p_to_t);
		double a3 = Math::lerp(p_to, p_post, p_post_t - p_to_t == 0 ? 1.0 : (t - p_to_t) / (p_post_t - p_to_t));
		double b1 = Math::lerp(a1, a2, p_to_t - p_pre_t == 0 ? 0.0 : (t - p_pre_t) / (p_to_t - p_pre_t));
		double b2 = Math::lerp(a2, a3, p_post_t == 0 ? 1.0 : t / p_post_t);
		return Math::lerp(b1, b2, p_to_t == 0 ? 0.5 : t / p_to_t);
	}

	static _ALWAYS_INLINE_ float cubic_interpolate_in_time(float p_from, float p_to, float p_pre, float p_post, float p_weight,
			float p_to_t, float p_pre_t, float p_post_t) {
		/* Barry-Goldman method */
		float t = Math::lerp(0.0f, p_to_t, p_weight);
		float a1 = Math::lerp(p_pre, p_from, p_pre_t == 0 ? 0.0f : (t - p_pre_t) / -p_pre_t);
		float a2 = Math::lerp(p_from, p_to, p_to_t == 0 ? 0.5f : t / p_to_t);
		float a3 = Math::lerp(p_to, p_post, p_post_t - p_to_t == 0 ? 1.0f : (t - p_to_t) / (p_post_t - p_to_t));
		float b1 = Math::lerp(a1, a2, p_to_t - p_pre_t == 0 ? 0.0f : (t - p_pre_t) / (p_to_t - p_pre_t));
		float b2 = Math::lerp(a2, a3, p_post_t == 0 ? 1.0f : t / p_post_t);
		return Math::lerp(b1, b2, p_to_t == 0 ? 0.5f : t / p_to_t);
	}

	static _ALWAYS_INLINE_ double cubic_interpolate_angle_in_time(double p_from, double p_to, double p_pre, double p_post, double p_weight,
			double p_to_t, double p_pre_t, double p_post_t) {
		double from_rot = fmod(p_from, Math_TAU);

		double pre_diff = fmod(p_pre - from_rot, Math_TAU);
		double pre_rot = from_rot + fmod(2.0 * pre_diff, Math_TAU) - pre_diff;

		double to_diff = fmod(p_to - from_rot, Math_TAU);
		double to_rot = from_rot + fmod(2.0 * to_diff, Math_TAU) - to_diff;

		double post_diff = fmod(p_post - to_rot, Math_TAU);
		double post_rot = to_rot + fmod(2.0 * post_diff, Math_TAU) - post_diff;

		return cubic_interpolate_in_time(from_rot, to_rot, pre_rot, post_rot, p_weight, p_to_t, p_pre_t, p_post_t);
	}

	static _ALWAYS_INLINE_ float cubic_interpolate_angle_in_time(float p_from, float p_to, float p_pre, float p_post, float p_weight,
			float p_to_t, float p_pre_t, float p_post_t) {
		float from_rot = fmod(p_from, (float)Math_TAU);

		float pre_diff = fmod(p_pre - from_rot, (float)Math_TAU);
		float pre_rot = from_rot + fmod(2.0f * pre_diff, (float)Math_TAU) - pre_diff;

		float to_diff = fmod(p_to - from_rot, (float)Math_TAU);
		float to_rot = from_rot + fmod(2.0f * to_diff, (float)Math_TAU) - to_diff;

		float post_diff = fmod(p_post - to_rot, (float)Math_TAU);
		float post_rot = to_rot + fmod(2.0f * post_diff, (float)Math_TAU) - post_diff;

		return cubic_interpolate_in_time(from_rot, to_rot, pre_rot, post_rot, p_weight, p_to_t, p_pre_t, p_post_t);
	}

	static _ALWAYS_INLINE_ double bezier_interpolate(double p_start, double p_control_1, double p_control_2, double p_end, double p_t) {
		/* Formula from Wikipedia article on Bezier curves. */
		double omt = (1.0 - p_t);
		double omt2 = omt * omt;
		double omt3 = omt2 * omt;
		double t2 = p_t * p_t;
		double t3 = t2 * p_t;

		return p_start * omt3 + p_control_1 * omt2 * p_t * 3.0 + p_control_2 * omt * t2 * 3.0 + p_end * t3;
	}

	static _ALWAYS_INLINE_ float bezier_interpolate(float p_start, float p_control_1, float p_control_2, float p_end, float p_t) {
		/* Formula from Wikipedia article on Bezier curves. */
		float omt = (1.0f - p_t);
		float omt2 = omt * omt;
		float omt3 = omt2 * omt;
		float t2 = p_t * p_t;
		float t3 = t2 * p_t;

		return p_start * omt3 + p_control_1 * omt2 * p_t * 3.0f + p_control_2 * omt * t2 * 3.0f + p_end * t3;
	}

	static _ALWAYS_INLINE_ double bezier_derivative(double p_start, double p_control_1, double p_control_2, double p_end, double p_t) {
		/* Formula from Wikipedia article on Bezier curves. */
		double omt = (1.0 - p_t);
		double omt2 = omt * omt;
		double t2 = p_t * p_t;

		double d = (p_control_1 - p_start) * 3.0 * omt2 + (p_control_2 - p_control_1) * 6.0 * omt * p_t + (p_end - p_control_2) * 3.0 * t2;
		return d;
	}

	static _ALWAYS_INLINE_ float bezier_derivative(float p_start, float p_control_1, float p_control_2, float p_end, float p_t) {
		/* Formula from Wikipedia article on Bezier curves. */
		float omt = (1.0f - p_t);
		float omt2 = omt * omt;
		float t2 = p_t * p_t;

		float d = (p_control_1 - p_start) * 3.0f * omt2 + (p_control_2 - p_control_1) * 6.0f * omt * p_t + (p_end - p_control_2) * 3.0f * t2;
		return d;
	}

	static _ALWAYS_INLINE_ double angle_difference(double p_from, double p_to) {
		double difference = fmod(p_to - p_from, Math_TAU);
		return fmod(2.0 * difference, Math_TAU) - difference;
	}
	static _ALWAYS_INLINE_ float angle_difference(float p_from, float p_to) {
		float difference = fmod(p_to - p_from, (float)Math_TAU);
		return fmod(2.0f * difference, (float)Math_TAU) - difference;
	}

	static _ALWAYS_INLINE_ double lerp_angle(double p_from, double p_to, double p_weight) {
		return p_from + Math::angle_difference(p_from, p_to) * p_weight;
	}
	static _ALWAYS_INLINE_ float lerp_angle(float p_from, float p_to, float p_weight) {
		return p_from + Math::angle_difference(p_from, p_to) * p_weight;
	}

	static _ALWAYS_INLINE_ double inverse_lerp(double p_from, double p_to, double p_value) {
		return (p_value - p_from) / (p_to - p_from);
	}
	static _ALWAYS_INLINE_ float inverse_lerp(float p_from, float p_to, float p_value) {
		return (p_value - p_from) / (p_to - p_from);
	}

	static _ALWAYS_INLINE_ double remap(double p_value, double p_istart, double p_istop, double p_ostart, double p_ostop) {
		return Math::lerp(p_ostart, p_ostop, Math::inverse_lerp(p_istart, p_istop, p_value));
	}
	static _ALWAYS_INLINE_ float remap(float p_value, float p_istart, float p_istop, float p_ostart, float p_ostop) {
		return Math::lerp(p_ostart, p_ostop, Math::inverse_lerp(p_istart, p_istop, p_value));
	}

	static _ALWAYS_INLINE_ double smoothstep(double p_from, double p_to, double p_s) {
		if (is_equal_approx(p_from, p_to)) {
			if (likely(p_from <= p_to)) {
				return p_s <= p_from ? 0.0 : 1.0;
			} else {
				return p_s <= p_to ? 1.0 : 0.0;
			}
		}
		double s = CLAMP((p_s - p_from) / (p_to - p_from), 0.0, 1.0);
		return s * s * (3.0 - 2.0 * s);
	}
	static _ALWAYS_INLINE_ float smoothstep(float p_from, float p_to, float p_s) {
		if (is_equal_approx(p_from, p_to)) {
			if (likely(p_from <= p_to)) {
				return p_s <= p_from ? 0.0f : 1.0f;
			} else {
				return p_s <= p_to ? 1.0f : 0.0f;
			}
		}
		float s = CLAMP((p_s - p_from) / (p_to - p_from), 0.0f, 1.0f);
		return s * s * (3.0f - 2.0f * s);
	}

	static _ALWAYS_INLINE_ double move_toward(double p_from, double p_to, double p_delta) {
		return abs(p_to - p_from) <= p_delta ? p_to : p_from + SIGN(p_to - p_from) * p_delta;
	}
	static _ALWAYS_INLINE_ float move_toward(float p_from, float p_to, float p_delta) {
		return abs(p_to - p_from) <= p_delta ? p_to : p_from + SIGN(p_to - p_from) * p_delta;
	}

	static _ALWAYS_INLINE_ double rotate_toward(double p_from, double p_to, double p_delta) {
		double difference = Math::angle_difference(p_from, p_to);
		double abs_difference = Math::abs(difference);
		// When `p_delta < 0` move no further than to PI radians away from `p_to` (as PI is the max possible angle distance).
		return p_from + CLAMP(p_delta, abs_difference - Math_PI, abs_difference) * (difference >= 0.0 ? 1.0 : -1.0);
	}
	static _ALWAYS_INLINE_ float rotate_toward(float p_from, float p_to, float p_delta) {
		float difference = Math::angle_difference(p_from, p_to);
		float abs_difference = Math::abs(difference);
		// When `p_delta < 0` move no further than to PI radians away from `p_to` (as PI is the max possible angle distance).
		return p_from + CLAMP(p_delta, abs_difference - (float)Math_PI, abs_difference) * (difference >= 0.0f ? 1.0f : -1.0f);
	}

	static _ALWAYS_INLINE_ double linear_to_db(double p_linear) {
		return Math::log(p_linear) * 8.6858896380650365530225783783321;
	}
	static _ALWAYS_INLINE_ float linear_to_db(float p_linear) {
		return Math::log(p_linear) * (float)8.6858896380650365530225783783321;
	}

	static _ALWAYS_INLINE_ double db_to_linear(double p_db) {
		return Math::exp(p_db * 0.11512925464970228420089957273422);
	}
	static _ALWAYS_INLINE_ float db_to_linear(float p_db) {
		return Math::exp(p_db * (float)0.11512925464970228420089957273422);
	}

	static _ALWAYS_INLINE_ double round(double p_val) { return ::round(p_val); }
	static _ALWAYS_INLINE_ float round(float p_val) { return ::roundf(p_val); }

	static _ALWAYS_INLINE_ int64_t wrapi(int64_t value, int64_t min, int64_t max) {
		int64_t range = max - min;
		return range == 0 ? min : min + ((((value - min) % range) + range) % range);
	}
	static _ALWAYS_INLINE_ double wrapf(double value, double min, double max) {
		double range = max - min;
		if (is_zero_approx(range)) {
			return min;
		}
		double result = value - (range * Math::floor((value - min) / range));
		if (is_equal_approx(result, max)) {
			return min;
		}
		return result;
	}
	static _ALWAYS_INLINE_ float wrapf(float value, float min, float max) {
		float range = max - min;
		if (is_zero_approx(range)) {
			return min;
		}
		float result = value - (range * Math::floor((value - min) / range));
		if (is_equal_approx(result, max)) {
			return min;
		}
		return result;
	}

	static _ALWAYS_INLINE_ float fract(float value) {
		return value - floor(value);
	}
	static _ALWAYS_INLINE_ double fract(double value) {
		return value - floor(value);
	}
	static _ALWAYS_INLINE_ float pingpong(float value, float length) {
		return (length != 0.0f) ? abs(fract((value - length) / (length * 2.0f)) * length * 2.0f - length) : 0.0f;
	}
	static _ALWAYS_INLINE_ double pingpong(double value, double length) {
		return (length != 0.0) ? abs(fract((value - length) / (length * 2.0)) * length * 2.0 - length) : 0.0;
	}

	// double only, as these functions are mainly used by the editor and not performance-critical,
	static double ease(double p_x, double p_c);
	static int step_decimals(double p_step);
	static int range_step_decimals(double p_step); // For editor use only.
	static double snapped(double p_value, double p_step);

	static uint32_t larger_prime(uint32_t p_val);

	static void seed(uint64_t x);
	static void randomize();
	static uint32_t rand_from_seed(uint64_t *seed);
	static uint32_t rand();
	static _ALWAYS_INLINE_ double randd() { return (double)rand() / (double)Math::RANDOM_32BIT_MAX; }
	static _ALWAYS_INLINE_ float randf() { return (float)rand() / (float)Math::RANDOM_32BIT_MAX; }
	static double randfn(double mean, double deviation);

	static double random(double from, double to);
	static float random(float from, float to);
	static int random(int from, int to);

	static _ALWAYS_INLINE_ bool is_equal_approx(float a, float b) {
		// Check for exact equality first, required to handle "infinity" values.
		if (a == b) {
			return true;
		}
		// Then check for approximate equality.
		float tolerance = (float)CMP_EPSILON * abs(a);
		if (tolerance < (float)CMP_EPSILON) {
			tolerance = (float)CMP_EPSILON;
		}
		return abs(a - b) < tolerance;
	}

	static _ALWAYS_INLINE_ bool is_equal_approx(float a, float b, float tolerance) {
		// Check for exact equality first, required to handle "infinity" values.
		if (a == b) {
			return true;
		}
		// Then check for approximate equality.
		return abs(a - b) < tolerance;
	}

	static _ALWAYS_INLINE_ bool is_zero_approx(float s) {
		return abs(s) < (float)CMP_EPSILON;
	}

	static _ALWAYS_INLINE_ bool is_equal_approx(double a, double b) {
		// Check for exact equality first, required to handle "infinity" values.
		if (a == b) {
			return true;
		}
		// Then check for approximate equality.
		double tolerance = CMP_EPSILON * abs(a);
		if (tolerance < CMP_EPSILON) {
			tolerance = CMP_EPSILON;
		}
		return abs(a - b) < tolerance;
	}

	static _ALWAYS_INLINE_ bool is_equal_approx(double a, double b, double tolerance) {
		// Check for exact equality first, required to handle "infinity" values.
		if (a == b) {
			return true;
		}
		// Then check for approximate equality.
		return abs(a - b) < tolerance;
	}

	static _ALWAYS_INLINE_ bool is_zero_approx(double s) {
		return abs(s) < CMP_EPSILON;
	}

	static _ALWAYS_INLINE_ float absf(float g) {
		union {
			float f;
			uint32_t i;
		} u;

		u.f = g;
		u.i &= 2147483647u;
		return u.f;
	}

	static _ALWAYS_INLINE_ double absd(double g) {
		union {
			double d;
			uint64_t i;
		} u;
		u.d = g;
		u.i &= (uint64_t)9223372036854775807ll;
		return u.d;
	}

	// This function should be as fast as possible and rounding mode should not matter.
	static _ALWAYS_INLINE_ int fast_ftoi(float a) {
		// Assuming every supported compiler has `lrint()`.
		return lrintf(a);
	}

	static _ALWAYS_INLINE_ uint32_t halfbits_to_floatbits(uint16_t h) {
		uint16_t h_exp, h_sig;
		uint32_t f_sgn, f_exp, f_sig;

		h_exp = (h & 0x7c00u);
		f_sgn = ((uint32_t)h & 0x8000u) << 16;
		switch (h_exp) {
			case 0x0000u: /* 0 or subnormal */
				h_sig = (h & 0x03ffu);
				/* Signed zero */
				if (h_sig == 0) {
					return f_sgn;
				}
				/* Subnormal */
				h_sig <<= 1;
				while ((h_sig & 0x0400u) == 0) {
					h_sig <<= 1;
					h_exp++;
				}
				f_exp = ((uint32_t)(127 - 15 - h_exp)) << 23;
				f_sig = ((uint32_t)(h_sig & 0x03ffu)) << 13;
				return f_sgn + f_exp + f_sig;
			case 0x7c00u: /* inf or NaN */
				/* All-ones exponent and a copy of the significand */
				return f_sgn + 0x7f800000u + (((uint32_t)(h & 0x03ffu)) << 13);
			default: /* normalized */
				/* Just need to adjust the exponent and shift */
				return f_sgn + (((uint32_t)(h & 0x7fffu) + 0x1c000u) << 13);
		}
	}

	static _ALWAYS_INLINE_ float halfptr_to_float(const uint16_t *h) {
		union {
			uint32_t u32;
			float f32;
		} u;

		u.u32 = halfbits_to_floatbits(*h);
		return u.f32;
	}

	static _ALWAYS_INLINE_ float half_to_float(const uint16_t h) {
		return halfptr_to_float(&h);
	}

	static _ALWAYS_INLINE_ uint16_t make_half_float(float f) {
		union {
			float fv;
			uint32_t ui;
		} ci;
		ci.fv = f;

		uint32_t x = ci.ui;
		uint32_t sign = (unsigned short)(x >> 31);
		uint32_t mantissa;
		uint32_t exponent;
		uint16_t hf;

		// get mantissa
		mantissa = x & ((1 << 23) - 1);
		// get exponent bits
		exponent = x & (0xFF << 23);
		if (exponent >= 0x47800000) {
			// check if the original single precision float number is a NaN
			if (mantissa && (exponent == (0xFF << 23))) {
				// we have a single precision NaN
				mantissa = (1 << 23) - 1;
			} else {
				// 16-bit half-float representation stores number as Inf
				mantissa = 0;
			}
			hf = (((uint16_t)sign) << 15) | (uint16_t)((0x1F << 10)) |
					(uint16_t)(mantissa >> 13);
		}
		// check if exponent is <= -15
		else if (exponent <= 0x38000000) {
			/*
			// store a denorm half-float value or zero
			exponent = (0x38000000 - exponent) >> 23;
			mantissa >>= (14 + exponent);

			hf = (((uint16_t)sign) << 15) | (uint16_t)(mantissa);
			*/
			hf = 0; //denormals do not work for 3D, convert to zero
		} else {
			hf = (((uint16_t)sign) << 15) |
					(uint16_t)((exponent - 0x38000000) >> 13) |
					(uint16_t)(mantissa >> 13);
		}

		return hf;
	}

	static _ALWAYS_INLINE_ float snap_scalar(float p_offset, float p_step, float p_target) {
		return p_step != 0 ? Math::snapped(p_target - p_offset, p_step) + p_offset : p_target;
	}

	static _ALWAYS_INLINE_ float snap_scalar_separation(float p_offset, float p_step, float p_target, float p_separation) {
		if (p_step != 0) {
			float a = Math::snapped(p_target - p_offset, p_step + p_separation) + p_offset;
			float b = a;
			if (p_target >= 0) {
				b -= p_separation;
			} else {
				b += p_step;
			}
			return (Math::abs(p_target - a) < Math::abs(p_target - b)) ? a : b;
		}
		return p_target;
	}
};
