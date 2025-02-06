/**************************************************************************/
/*  noise.h                                                               */
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

#include "core/io/image.h"
#include "core/variant/typed_array.h"

class Noise : public Resource {
	GDCLASS(Noise, Resource);

	// Helper struct for get_seamless_image(). See comments in .cpp for usage.
	template <typename T>
	struct img_buff {
		T *img = nullptr;
		int width; // Array dimensions & default modulo for image.
		int height;
		int offset_x; // Offset index location on image (wrapped by specified modulo).
		int offset_y;
		int alt_width; // Alternate module for image.
		int alt_height;

		enum ALT_MODULO {
			DEFAULT = 0,
			ALT_X,
			ALT_Y,
			ALT_XY
		};

		// Multi-dimensional array indexer (e.g. img[x][y]) that supports multiple modulos.
		T &operator()(int x, int y, ALT_MODULO mode = DEFAULT) {
			switch (mode) {
				case ALT_XY:
					return img[(x + offset_x) % alt_width + ((y + offset_y) % alt_height) * width];
				case ALT_X:
					return img[(x + offset_x) % alt_width + ((y + offset_y) % height) * width];
				case ALT_Y:
					return img[(x + offset_x) % width + ((y + offset_y) % alt_height) * width];
				default:
					return img[(x + offset_x) % width + ((y + offset_y) % height) * width];
			}
		}
	};

	union l2c {
		uint32_t l;
		uint8_t c[4];
		struct {
			uint8_t r;
			uint8_t g;
			uint8_t b;
			uint8_t a;
		};
	};

	template <typename T>
	Vector<Ref<Image>> _generate_seamless_image(Vector<Ref<Image>> p_src, int p_width, int p_height, int p_depth, bool p_invert, real_t p_blend_skirt) const {
		/*
		To make a seamless image, we swap the quadrants so the edges are perfect matches.
		We initially get a 10% larger image so we have an overlap we can use to blend over the seams.

		Noise::img_buff::operator() acts as a multi-dimensional array indexer.
		It does the array math, translates between the flipped and non-flipped quadrants, and manages offsets and modulos.

		Here is how the larger source image and final output image map to each other:

		Output size = p_width*p_height	Source w/ extra 10% skirt `s` size = src_width*src_height
		Q1   Q2							Q4	Q3 s1
		Q3   Q4							Q2	Q1 s2
										s5	s4 s3

		All of the loops use output coordinates, so Output:Q1 == Source:Q1
		Ex: Output(half_width, half_height) [the midpoint, corner of Q1/Q4] =>
		on Source it's translated to
		corner of Q1/s3 unless the ALT_XY modulo moves it to Q4
		*/
		ERR_FAIL_COND_V(p_blend_skirt < 0, Vector<Ref<Image>>());

		int skirt_width = MAX(1, p_width * p_blend_skirt);
		int skirt_height = MAX(1, p_height * p_blend_skirt);
		int src_width = p_width + skirt_width;
		int src_height = p_height + skirt_height;
		int half_width = p_width * 0.5;
		int half_height = p_height * 0.5;
		int skirt_edge_x = half_width + skirt_width;
		int skirt_edge_y = half_height + skirt_height;

		Image::Format format = p_src[0]->get_format();
		int pixel_size = Image::get_format_pixel_size(format);

		Vector<Ref<Image>> images;
		images.resize(p_src.size());

		// First blend across x and y for all slices.
		for (int d = 0; d < images.size(); d++) {
			Vector<uint8_t> dest;
			dest.resize(p_width * p_height * pixel_size);

			img_buff<T> rd_src = {
				(T *)p_src[d]->get_data().ptr(),
				src_width, src_height,
				half_width, half_height,
				p_width, p_height
			};

			// `wr` is setup for straight x/y coordinate array access.
			img_buff<T> wr = {
				(T *)dest.ptrw(),
				p_width, p_height,
				0, 0, 0, 0
			};
			// `rd_dest` is a readable pointer to `wr`, i.e. what has already been written to the output buffer.
			img_buff<T> rd_dest = {
				(T *)dest.ptr(),
				p_width, p_height,
				0, 0, 0, 0
			};

			// Swap the quadrants to make edges seamless.
			for (int y = 0; y < p_height; y++) {
				for (int x = 0; x < p_width; x++) {
					// rd_src has a half offset and the shorter modulo ignores the skirt.
					// It reads and writes in Q1-4 order (see map above), skipping the skirt.
					wr(x, y) = rd_src(x, y, img_buff<T>::ALT_XY);
				}
			}

			// Blend the vertical skirt over the middle seam.
			for (int x = half_width; x < skirt_edge_x; x++) {
				int alpha = 255 * (1 - Math::smoothstep(0.1f, 0.9f, float(x - half_width) / float(skirt_width)));
				for (int y = 0; y < p_height; y++) {
					// Skip the center square
					if (y == half_height) {
						y = skirt_edge_y - 1;
					} else {
						// Starts reading at s2, ALT_Y skips s3, and continues with s1.
						wr(x, y) = _alpha_blend<T>(rd_dest(x, y), rd_src(x, y, img_buff<T>::ALT_Y), alpha);
					}
				}
			}

			// Blend the horizontal skirt over the middle seam.
			for (int y = half_height; y < skirt_edge_y; y++) {
				int alpha = 255 * (1 - Math::smoothstep(0.1f, 0.9f, float(y - half_height) / float(skirt_height)));
				for (int x = 0; x < p_width; x++) {
					// Skip the center square
					if (x == half_width) {
						x = skirt_edge_x - 1;
					} else {
						// Starts reading at s4, skips s3, continues with s5.
						wr(x, y) = _alpha_blend<T>(rd_dest(x, y), rd_src(x, y, img_buff<T>::ALT_X), alpha);
					}
				}
			}

			// Fill in the center square. Wr starts at the top left of Q4, which is the equivalent of the top left of s3, unless a modulo is used.
			for (int y = half_height; y < skirt_edge_y; y++) {
				for (int x = half_width; x < skirt_edge_x; x++) {
					int xpos = 255 * (1 - Math::smoothstep(0.1f, 0.9f, float(x - half_width) / float(skirt_width)));
					int ypos = 255 * (1 - Math::smoothstep(0.1f, 0.9f, float(y - half_height) / float(skirt_height)));

					// Blend s3(Q1) onto s5(Q2) for the top half.
					T top_blend = _alpha_blend<T>(rd_src(x, y, img_buff<T>::ALT_X), rd_src(x, y, img_buff<T>::DEFAULT), xpos);
					// Blend s1(Q3) onto Q4 for the bottom half.
					T bottom_blend = _alpha_blend<T>(rd_src(x, y, img_buff<T>::ALT_XY), rd_src(x, y, img_buff<T>::ALT_Y), xpos);
					// Blend the top half onto the bottom half.
					wr(x, y) = _alpha_blend<T>(bottom_blend, top_blend, ypos);
				}
			}
			Ref<Image> image = memnew(Image(p_width, p_height, false, format, dest));
			p_src.write[d].unref();
			images.write[d] = image;
		}

		// Now blend across z.
		if (p_depth > 1) {
			int skirt_depth = MAX(1, p_depth * p_blend_skirt);
			int half_depth = p_depth * 0.5;
			int skirt_edge_z = half_depth + skirt_depth;

			// Swap halves on depth.
			for (int i = 0; i < half_depth; i++) {
				Ref<Image> img = images[i];
				images.write[i] = images[i + half_depth];
				images.write[i + half_depth] = img;
			}

			Vector<Ref<Image>> new_images = images;
			new_images.resize(p_depth);

			// Scale seamless generation to third dimension.
			for (int z = half_depth; z < skirt_edge_z; z++) {
				int alpha = 255 * (1 - Math::smoothstep(0.1f, 0.9f, float(z - half_depth) / float(skirt_depth)));

				Vector<uint8_t> img = images[z % p_depth]->get_data();
				Vector<uint8_t> skirt = images[(z - half_depth) + p_depth]->get_data();

				Vector<uint8_t> dest;
				dest.resize(images[0]->get_width() * images[0]->get_height() * Image::get_format_pixel_size(images[0]->get_format()));

				for (int i = 0; i < img.size(); i++) {
					uint8_t fg, bg, out;

					fg = skirt[i];
					bg = img[i];

					uint16_t a = alpha + 1;
					uint16_t inv_a = 256 - alpha;

					out = (uint8_t)((a * fg + inv_a * bg) >> 8);

					dest.write[i] = out;
				}

				Ref<Image> new_image = memnew(Image(images[0]->get_width(), images[0]->get_height(), false, images[0]->get_format(), dest));
				new_images.write[z % p_depth] = new_image;
			}
			return new_images;
		}
		return images;
	}

	template <typename T>
	T _alpha_blend(T p_bg, T p_fg, int p_alpha) const {
		l2c fg, bg, out;

		fg.l = p_fg;
		bg.l = p_bg;

		uint16_t alpha;
		uint16_t inv_alpha;

		// If no alpha argument specified, use the alpha channel in the color
		if (p_alpha == -1) {
			alpha = fg.c[3] + 1;
			inv_alpha = 256 - fg.c[3];
		} else {
			alpha = p_alpha + 1;
			inv_alpha = 256 - p_alpha;
		}

		out.c[0] = (uint8_t)((alpha * fg.c[0] + inv_alpha * bg.c[0]) >> 8);
		out.c[1] = (uint8_t)((alpha * fg.c[1] + inv_alpha * bg.c[1]) >> 8);
		out.c[2] = (uint8_t)((alpha * fg.c[2] + inv_alpha * bg.c[2]) >> 8);
		out.c[3] = 0xFF;

		return out.l;
	}

protected:
	static void _bind_methods();

public:
	// Virtual destructor so we can delete any Noise derived object when referenced as a Noise*.
	virtual ~Noise() {}

	virtual real_t get_noise_1d(real_t p_x) const = 0;

	virtual real_t get_noise_2dv(Vector2 p_v) const = 0;
	virtual real_t get_noise_2d(real_t p_x, real_t p_y) const = 0;

	virtual real_t get_noise_3dv(Vector3 p_v) const = 0;
	virtual real_t get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const = 0;

	Vector<Ref<Image>> _get_image(int p_width, int p_height, int p_depth, bool p_invert = false, bool p_in_3d_space = false, bool p_normalize = true) const;
	virtual Ref<Image> get_image(int p_width, int p_height, bool p_invert = false, bool p_in_3d_space = false, bool p_normalize = true) const;
	virtual TypedArray<Image> get_image_3d(int p_width, int p_height, int p_depth, bool p_invert = false, bool p_normalize = true) const;

	Vector<Ref<Image>> _get_seamless_image(int p_width, int p_height, int p_depth, bool p_invert = false, bool p_in_3d_space = false, real_t p_blend_skirt = 0.1, bool p_normalize = true) const;
	virtual Ref<Image> get_seamless_image(int p_width, int p_height, bool p_invert = false, bool p_in_3d_space = false, real_t p_blend_skirt = 0.1, bool p_normalize = true) const;
	virtual TypedArray<Image> get_seamless_image_3d(int p_width, int p_height, int p_depth, bool p_invert = false, real_t p_blend_skirt = 0.1, bool p_normalize = true) const;
};
