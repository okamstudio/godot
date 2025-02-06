/**************************************************************************/
/*  image_scaling.h                                                       */
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

#ifndef IMAGE_SCALING_H
#define IMAGE_SCALING_H

#include "core/math/math_funcs.h"

enum DataType {
	DATA_UINT8,
	DATA_FLOAT16,
	DATA_FLOAT32,
};

static double _bicubic_interp_kernel(double x) {
	x = ABS(x);

	double bc = 0;

	if (x <= 1) {
		bc = (1.5 * x - 2.5) * x * x + 1;
	} else if (x < 2) {
		bc = ((-0.5 * x + 2.5) * x - 4) * x + 2;
	}

	return bc;
}

template <int CC, DataType TYPE, typename T>
static void _scale_cubic(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_width, uint32_t p_src_height, uint32_t p_dst_width, uint32_t p_dst_height) {
	// get source image size
	int width = p_src_width;
	int height = p_src_height;
	double xfac = (double)width / p_dst_width;
	double yfac = (double)height / p_dst_height;
	// coordinates of source points and coefficients
	double ox, oy, dx, dy;
	int ox1, oy1, ox2, oy2;
	// destination pixel values
	// width and height decreased by 1
	int ymax = height - 1;
	int xmax = width - 1;
	// temporary pointer

	for (uint32_t y = 0; y < p_dst_height; y++) {
		// Y coordinates
		oy = (double)y * yfac - 0.5f;
		oy1 = (int)oy;
		dy = oy - (double)oy1;

		for (uint32_t x = 0; x < p_dst_width; x++) {
			// X coordinates
			ox = (double)x * xfac - 0.5f;
			ox1 = (int)ox;
			dx = ox - (double)ox1;

			// initial pixel value

			T *__restrict dst = ((T *)p_dst) + (y * p_dst_width + x) * CC;

			double color[CC];
			for (int i = 0; i < CC; i++) {
				color[i] = 0;
			}

			for (int n = -1; n < 3; n++) {
				// get Y coefficient
				[[maybe_unused]] double k1 = _bicubic_interp_kernel(dy - (double)n);

				oy2 = oy1 + n;
				if (oy2 < 0) {
					oy2 = 0;
				}
				if (oy2 > ymax) {
					oy2 = ymax;
				}

				for (int m = -1; m < 3; m++) {
					// get X coefficient
					[[maybe_unused]] double k2 = k1 * _bicubic_interp_kernel((double)m - dx);

					ox2 = ox1 + m;
					if (ox2 < 0) {
						ox2 = 0;
					}
					if (ox2 > xmax) {
						ox2 = xmax;
					}

					// get pixel of original image
					const T *__restrict p = ((T *)p_src) + (oy2 * p_src_width + ox2) * CC;

					for (int i = 0; i < CC; i++) {
						if constexpr (TYPE == DATA_FLOAT16) {
							color[i] = Math::half_to_float(p[i]);
						} else {
							color[i] += p[i] * k2;
						}
					}
				}
			}

			for (int i = 0; i < CC; i++) {
				if constexpr (TYPE == DATA_UINT8) {
					dst[i] = CLAMP(Math::fast_ftoi(color[i]), 0, 255);
				} else if constexpr (TYPE == DATA_FLOAT16) {
					dst[i] = Math::make_half_float(color[i]);
				} else {
					dst[i] = color[i];
				}
			}
		}
	}
}

template <int CC, DataType TYPE, typename T>
static void _scale_bilinear(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_width, uint32_t p_src_height, uint32_t p_dst_width, uint32_t p_dst_height) {
	constexpr uint32_t FRAC_BITS = 8;
	constexpr uint32_t FRAC_LEN = (1 << FRAC_BITS);
	constexpr uint32_t FRAC_HALF = (FRAC_LEN >> 1);
	constexpr uint32_t FRAC_MASK = FRAC_LEN - 1;

	for (uint32_t i = 0; i < p_dst_height; i++) {
		// Add 0.5 in order to interpolate based on pixel center
		uint32_t src_yofs_up_fp = (i + 0.5) * p_src_height * FRAC_LEN / p_dst_height;
		// Calculate nearest src pixel center above current, and truncate to get y index
		uint32_t src_yofs_up = src_yofs_up_fp >= FRAC_HALF ? (src_yofs_up_fp - FRAC_HALF) >> FRAC_BITS : 0;
		uint32_t src_yofs_down = (src_yofs_up_fp + FRAC_HALF) >> FRAC_BITS;
		if (src_yofs_down >= p_src_height) {
			src_yofs_down = p_src_height - 1;
		}
		// Calculate distance to pixel center of src_yofs_up
		uint32_t src_yofs_frac = src_yofs_up_fp & FRAC_MASK;
		src_yofs_frac = src_yofs_frac >= FRAC_HALF ? src_yofs_frac - FRAC_HALF : src_yofs_frac + FRAC_HALF;

		uint32_t y_ofs_up = src_yofs_up * p_src_width * CC;
		uint32_t y_ofs_down = src_yofs_down * p_src_width * CC;

		for (uint32_t j = 0; j < p_dst_width; j++) {
			uint32_t src_xofs_left_fp = (j + 0.5) * p_src_width * FRAC_LEN / p_dst_width;
			uint32_t src_xofs_left = src_xofs_left_fp >= FRAC_HALF ? (src_xofs_left_fp - FRAC_HALF) >> FRAC_BITS : 0;
			uint32_t src_xofs_right = (src_xofs_left_fp + FRAC_HALF) >> FRAC_BITS;
			if (src_xofs_right >= p_src_width) {
				src_xofs_right = p_src_width - 1;
			}
			uint32_t src_xofs_frac = src_xofs_left_fp & FRAC_MASK;
			src_xofs_frac = src_xofs_frac >= FRAC_HALF ? src_xofs_frac - FRAC_HALF : src_xofs_frac + FRAC_HALF;

			src_xofs_left *= CC;
			src_xofs_right *= CC;

			for (uint32_t l = 0; l < CC; l++) {
				if constexpr (TYPE == DATA_UINT8) {
					uint32_t p00 = p_src[y_ofs_up + src_xofs_left + l] << FRAC_BITS;
					uint32_t p10 = p_src[y_ofs_up + src_xofs_right + l] << FRAC_BITS;
					uint32_t p01 = p_src[y_ofs_down + src_xofs_left + l] << FRAC_BITS;
					uint32_t p11 = p_src[y_ofs_down + src_xofs_right + l] << FRAC_BITS;

					uint32_t interp_up = p00 + (((p10 - p00) * src_xofs_frac) >> FRAC_BITS);
					uint32_t interp_down = p01 + (((p11 - p01) * src_xofs_frac) >> FRAC_BITS);
					uint32_t interp = interp_up + (((interp_down - interp_up) * src_yofs_frac) >> FRAC_BITS);
					interp >>= FRAC_BITS;
					p_dst[i * p_dst_width * CC + j * CC + l] = uint8_t(interp);
				} else if constexpr (TYPE == DATA_FLOAT16) {
					float xofs_frac = float(src_xofs_frac) / (1 << FRAC_BITS);
					float yofs_frac = float(src_yofs_frac) / (1 << FRAC_BITS);
					const T *src = ((const T *)p_src);
					T *dst = ((T *)p_dst);

					float p00 = Math::half_to_float(src[y_ofs_up + src_xofs_left + l]);
					float p10 = Math::half_to_float(src[y_ofs_up + src_xofs_right + l]);
					float p01 = Math::half_to_float(src[y_ofs_down + src_xofs_left + l]);
					float p11 = Math::half_to_float(src[y_ofs_down + src_xofs_right + l]);

					float interp_up = p00 + (p10 - p00) * xofs_frac;
					float interp_down = p01 + (p11 - p01) * xofs_frac;
					float interp = interp_up + ((interp_down - interp_up) * yofs_frac);

					dst[i * p_dst_width * CC + j * CC + l] = Math::make_half_float(interp);
				} else if constexpr (TYPE == DATA_FLOAT32) {
					float xofs_frac = float(src_xofs_frac) / (1 << FRAC_BITS);
					float yofs_frac = float(src_yofs_frac) / (1 << FRAC_BITS);
					const T *src = ((const T *)p_src);
					T *dst = ((T *)p_dst);

					float p00 = src[y_ofs_up + src_xofs_left + l];
					float p10 = src[y_ofs_up + src_xofs_right + l];
					float p01 = src[y_ofs_down + src_xofs_left + l];
					float p11 = src[y_ofs_down + src_xofs_right + l];

					float interp_up = p00 + (p10 - p00) * xofs_frac;
					float interp_down = p01 + (p11 - p01) * xofs_frac;
					float interp = interp_up + ((interp_down - interp_up) * yofs_frac);

					dst[i * p_dst_width * CC + j * CC + l] = interp;
				}
			}
		}
	}
}

template <int CC, typename T>
static void _scale_nearest(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_width, uint32_t p_src_height, uint32_t p_dst_width, uint32_t p_dst_height) {
	for (uint32_t i = 0; i < p_dst_height; i++) {
		uint32_t src_yofs = i * p_src_height / p_dst_height;
		uint32_t y_ofs = src_yofs * p_src_width * CC;

		for (uint32_t j = 0; j < p_dst_width; j++) {
			uint32_t src_xofs = j * p_src_width / p_dst_width;
			src_xofs *= CC;

			for (uint32_t l = 0; l < CC; l++) {
				const T *src = ((const T *)p_src);
				T *dst = ((T *)p_dst);

				T p = src[y_ofs + src_xofs + l];
				dst[i * p_dst_width * CC + j * CC + l] = p;
			}
		}
	}
}

#define LANCZOS_TYPE 3

static float _lanczos(float p_x) {
	return Math::abs(p_x) >= LANCZOS_TYPE ? 0 : Math::sincn(p_x) * Math::sincn(p_x / LANCZOS_TYPE);
}

template <int CC, DataType TYPE, typename T>
static void _scale_lanczos(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_width, uint32_t p_src_height, uint32_t p_dst_width, uint32_t p_dst_height) {
	int32_t src_width = p_src_width;
	int32_t src_height = p_src_height;
	int32_t dst_height = p_dst_height;
	int32_t dst_width = p_dst_width;

	uint32_t buffer_size = src_height * dst_width * CC;
	float *buffer = memnew_arr(float, buffer_size); // Store the first pass in a buffer

	{ // FIRST PASS (horizontal)

		float x_scale = float(src_width) / float(dst_width);

		float scale_factor = MAX(x_scale, 1); // A larger kernel is required only when downscaling
		int32_t half_kernel = LANCZOS_TYPE * scale_factor;

		float *kernel = memnew_arr(float, half_kernel * 2);

		for (int32_t buffer_x = 0; buffer_x < dst_width; buffer_x++) {
			// The corresponding point on the source image
			float src_x = (buffer_x + 0.5f) * x_scale; // Offset by 0.5 so it uses the pixel's center
			int32_t start_x = MAX(0, int32_t(src_x) - half_kernel + 1);
			int32_t end_x = MIN(src_width - 1, int32_t(src_x) + half_kernel);

			// Create the kernel used by all the pixels of the column
			for (int32_t target_x = start_x; target_x <= end_x; target_x++) {
				kernel[target_x - start_x] = _lanczos((target_x + 0.5f - src_x) / scale_factor);
			}

			for (int32_t buffer_y = 0; buffer_y < src_height; buffer_y++) {
				float pixel[CC] = { 0 };
				float weight = 0;

				for (int32_t target_x = start_x; target_x <= end_x; target_x++) {
					float lanczos_val = kernel[target_x - start_x];
					weight += lanczos_val;

					const T *__restrict src_data = ((const T *)p_src) + (buffer_y * src_width + target_x) * CC;

					for (uint32_t i = 0; i < CC; i++) {
						if constexpr (TYPE == DATA_FLOAT16) {
							pixel[i] += Math::half_to_float(src_data[i]) * lanczos_val;
						} else {
							pixel[i] += src_data[i] * lanczos_val;
						}
					}
				}

				float *dst_data = ((float *)buffer) + (buffer_y * dst_width + buffer_x) * CC;

				for (uint32_t i = 0; i < CC; i++) {
					dst_data[i] = pixel[i] / weight; // Normalize the sum of all the samples
				}
			}
		}

		memdelete_arr(kernel);
	} // End of first pass

	{ // SECOND PASS (vertical + result)

		float y_scale = float(src_height) / float(dst_height);

		float scale_factor = MAX(y_scale, 1);
		int32_t half_kernel = LANCZOS_TYPE * scale_factor;

		float *kernel = memnew_arr(float, half_kernel * 2);

		for (int32_t dst_y = 0; dst_y < dst_height; dst_y++) {
			float buffer_y = (dst_y + 0.5f) * y_scale;
			int32_t start_y = MAX(0, int32_t(buffer_y) - half_kernel + 1);
			int32_t end_y = MIN(src_height - 1, int32_t(buffer_y) + half_kernel);

			for (int32_t target_y = start_y; target_y <= end_y; target_y++) {
				kernel[target_y - start_y] = _lanczos((target_y + 0.5f - buffer_y) / scale_factor);
			}

			for (int32_t dst_x = 0; dst_x < dst_width; dst_x++) {
				float pixel[CC] = { 0 };
				float weight = 0;

				for (int32_t target_y = start_y; target_y <= end_y; target_y++) {
					float lanczos_val = kernel[target_y - start_y];
					weight += lanczos_val;

					float *buffer_data = ((float *)buffer) + (target_y * dst_width + dst_x) * CC;

					for (uint32_t i = 0; i < CC; i++) {
						pixel[i] += buffer_data[i] * lanczos_val;
					}
				}

				T *dst_data = ((T *)p_dst) + (dst_y * dst_width + dst_x) * CC;

				for (uint32_t i = 0; i < CC; i++) {
					pixel[i] /= weight;

					if constexpr (TYPE == DATA_UINT8) {
						dst_data[i] = CLAMP(Math::fast_ftoi(pixel[i]), 0, 255);
					} else if constexpr (TYPE == DATA_FLOAT16) {
						dst_data[i] = Math::make_half_float(pixel[i]);
					} else { // float
						dst_data[i] = pixel[i];
					}
				}
			}
		}

		memdelete_arr(kernel);
	} // End of second pass

	memdelete_arr(buffer);
}

static void _overlay(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, float p_alpha, uint32_t p_width, uint32_t p_height, uint32_t p_pixel_size) {
	uint16_t alpha = MIN((uint16_t)(p_alpha * 256.0f), 256);

	for (uint32_t i = 0; i < p_width * p_height * p_pixel_size; i++) {
		p_dst[i] = (p_dst[i] * (256 - alpha) + p_src[i] * alpha) >> 8;
	}
}

#endif // IMAGE_SCALING_H
