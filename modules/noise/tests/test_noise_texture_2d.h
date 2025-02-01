/**************************************************************************/
/*  test_noise_texture_2d.h                                               */
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

#include "../noise_texture_2d.h"

#include "tests/test_macros.h"

namespace TestNoiseTexture2D {

class NoiseTextureTester : public RefCounted {
	GDCLASS(NoiseTextureTester, RefCounted);

	const NoiseTexture2D *const texture;

public:
	NoiseTextureTester(const NoiseTexture2D *const p_texture) :
			texture{ p_texture } {}

	Color compute_average_color(const Ref<Image> &p_noise_image) {
		Color r_avg_color{};

		for (int i = 0; i < p_noise_image->get_width(); ++i) {
			for (int j = 0; j < p_noise_image->get_height(); ++j) {
				const Color pixel = p_noise_image->get_pixel(i, j);
				r_avg_color += pixel;
			}
		}

		int pixel_count = p_noise_image->get_width() * p_noise_image->get_height();
		r_avg_color /= pixel_count;
		return r_avg_color;
	}

	void check_mip_and_color_ramp() {
		const Ref<Image> noise_image = texture->get_image();
		CHECK(noise_image.is_valid());
		CHECK(noise_image->get_width() == texture->get_width());
		CHECK(noise_image->get_height() == texture->get_height());

		CHECK(noise_image->get_format() == Image::FORMAT_RGBA8);
		CHECK(noise_image->has_mipmaps());

		Color avg_color = compute_average_color(noise_image);

		// Check that the noise texture is modulated correctly by the color ramp (Gradient).
		CHECK_FALSE_MESSAGE((avg_color.r + avg_color.g + avg_color.b) == doctest::Approx(0.0), "The noise texture should not be all black");
		CHECK_FALSE_MESSAGE((avg_color.r + avg_color.g + avg_color.b) == doctest::Approx(noise_image->get_width() * noise_image->get_height() * 3.0), "The noise texture should not be all white");
		CHECK_MESSAGE(avg_color.g == doctest::Approx(0.0), "The noise texture should not have any green when modulated correctly by the color ramp");
	}

	void check_normal_map() {
		const Ref<Image> noise_image = texture->get_image();
		CHECK(noise_image.is_valid());
		CHECK(noise_image->get_width() == texture->get_width());
		CHECK(noise_image->get_height() == texture->get_height());

		CHECK(noise_image->get_format() == Image::FORMAT_RGBA8);
		CHECK_FALSE(noise_image->has_mipmaps());

		Color avg_color = compute_average_color(noise_image);

		// Check for the characteristic color distribution (for tangent space) of a normal map.
		CHECK(avg_color.r == doctest::Approx(0.5).epsilon(0.05));
		CHECK(avg_color.g == doctest::Approx(0.5).epsilon(0.05));
		CHECK(avg_color.b == doctest::Approx(1.0).epsilon(0.05));
	}

	void check_seamless_texture_grayscale() {
		const Ref<Image> noise_image = texture->get_image();
		CHECK(noise_image.is_valid());
		CHECK(noise_image->get_width() == texture->get_width());
		CHECK(noise_image->get_height() == texture->get_height());

		CHECK(noise_image->get_format() == Image::FORMAT_L8);

		Color avg_color = compute_average_color(noise_image);

		// Since it's a grayscale image and every channel except the alpha channel has the
		// same values (conversion happens in Image::get_pixel) we only need to test one channel.
		CHECK(avg_color.r == doctest::Approx(0.5).epsilon(0.05));
	}

	void check_seamless_texture_rgba() {
		const Ref<Image> noise_image = texture->get_image();
		CHECK(noise_image.is_valid());
		CHECK(noise_image->get_width() == texture->get_width());
		CHECK(noise_image->get_height() == texture->get_height());

		CHECK(noise_image->get_format() == Image::FORMAT_RGBA8);

		// Check that the noise texture is modulated correctly by the color ramp (Gradient).
		Color avg_color = compute_average_color(noise_image);

		// We use a default (black to white) gradient, so the average of the red, green and blue channels should be the same.
		CHECK(avg_color.r == doctest::Approx(0.5).epsilon(0.05));
		CHECK(avg_color.g == doctest::Approx(0.5).epsilon(0.05));
		CHECK(avg_color.b == doctest::Approx(0.5).epsilon(0.05));
	}
};

TEST_CASE("[NoiseTexture][SceneTree] Getter and setter") {
	Ref<NoiseTexture2D> noise_texture = memnew(NoiseTexture2D);

	Ref<FastNoiseLite> noise = memnew(FastNoiseLite);
	noise_texture->set_noise(noise);
	CHECK(noise_texture->get_noise() == noise);
	noise_texture->set_noise(nullptr);
	CHECK(noise_texture->get_noise().is_null());

	noise_texture->set_width(8);
	noise_texture->set_height(4);
	CHECK(noise_texture->get_width() == 8);
	CHECK(noise_texture->get_height() == 4);

	ERR_PRINT_OFF;
	noise_texture->set_width(-1);
	noise_texture->set_height(-1);
	ERR_PRINT_ON;
	CHECK(noise_texture->get_width() == 8);
	CHECK(noise_texture->get_height() == 4);

	noise_texture->set_invert(true);
	CHECK(noise_texture->get_invert() == true);
	noise_texture->set_invert(false);
	CHECK(noise_texture->get_invert() == false);

	noise_texture->set_in_3d_space(true);
	CHECK(noise_texture->is_in_3d_space() == true);
	noise_texture->set_in_3d_space(false);
	CHECK(noise_texture->is_in_3d_space() == false);

	noise_texture->set_generate_mipmaps(true);
	CHECK(noise_texture->is_generating_mipmaps() == true);
	noise_texture->set_generate_mipmaps(false);
	CHECK(noise_texture->is_generating_mipmaps() == false);

	noise_texture->set_seamless(true);
	CHECK(noise_texture->get_seamless() == true);
	noise_texture->set_seamless(false);
	CHECK(noise_texture->get_seamless() == false);

	noise_texture->set_seamless_blend_skirt(0.45);
	CHECK(noise_texture->get_seamless_blend_skirt() == doctest::Approx(0.45));

	ERR_PRINT_OFF;
	noise_texture->set_seamless_blend_skirt(-1.0);
	noise_texture->set_seamless_blend_skirt(2.0);
	CHECK(noise_texture->get_seamless_blend_skirt() == doctest::Approx(0.45));
	ERR_PRINT_ON;

	noise_texture->set_as_normal_map(true);
	CHECK(noise_texture->is_normal_map() == true);
	noise_texture->set_as_normal_map(false);
	CHECK(noise_texture->is_normal_map() == false);

	noise_texture->set_bump_strength(0.168);
	CHECK(noise_texture->get_bump_strength() == doctest::Approx(0.168));

	Ref<Gradient> gradient = memnew(Gradient);
	noise_texture->set_color_ramp(gradient);
	CHECK(noise_texture->get_color_ramp() == gradient);
	noise_texture->set_color_ramp(nullptr);
	CHECK(noise_texture->get_color_ramp().is_null());
}

TEST_CASE("[NoiseTexture2D][SceneTree] Generating a basic noise texture with mipmaps and color ramp modulation") {
	Ref<NoiseTexture2D> noise_texture = memnew(NoiseTexture2D);

	Ref<FastNoiseLite> noise = memnew(FastNoiseLite);
	noise_texture->set_noise(noise);

	Ref<Gradient> gradient = memnew(Gradient);
	Vector<float> offsets = { 0.0, 1.0 };
	Vector<Color> colors = { Color(1, 0, 0), Color(0, 0, 1) };
	gradient->set_offsets(offsets);
	gradient->set_colors(colors);

	noise_texture->set_color_ramp(gradient);
	noise_texture->set_width(16);
	noise_texture->set_height(16);
	noise_texture->set_generate_mipmaps(true);

	Ref<NoiseTextureTester> tester = memnew(NoiseTextureTester(noise_texture.ptr()));
	noise_texture->connect_changed(callable_mp(tester.ptr(), &NoiseTextureTester::check_mip_and_color_ramp));
	MessageQueue::get_singleton()->flush();
}

TEST_CASE("[NoiseTexture2D][SceneTree] Generating a normal map without mipmaps") {
	Ref<NoiseTexture2D> noise_texture = memnew(NoiseTexture2D);

	Ref<FastNoiseLite> noise = memnew(FastNoiseLite);
	noise->set_frequency(0.5);
	noise_texture->set_noise(noise);
	noise_texture->set_width(16);
	noise_texture->set_height(16);
	noise_texture->set_as_normal_map(true);
	noise_texture->set_bump_strength(0.5);
	noise_texture->set_generate_mipmaps(false);

	Ref<NoiseTextureTester> tester = memnew(NoiseTextureTester(noise_texture.ptr()));
	noise_texture->connect_changed(callable_mp(tester.ptr(), &NoiseTextureTester::check_normal_map));
	MessageQueue::get_singleton()->flush();
}

TEST_CASE("[NoiseTexture2D][SceneTree] Generating a seamless noise texture") {
	Ref<NoiseTexture2D> noise_texture = memnew(NoiseTexture2D);

	Ref<FastNoiseLite> noise = memnew(FastNoiseLite);
	noise->set_frequency(0.5);
	noise_texture->set_noise(noise);
	noise_texture->set_width(16);
	noise_texture->set_height(16);
	noise_texture->set_seamless(true);

	Ref<NoiseTextureTester> tester = memnew(NoiseTextureTester(noise_texture.ptr()));

	SUBCASE("Grayscale(L8) 16x16, with seamless blend skirt of 0.05") {
		noise_texture->set_seamless_blend_skirt(0.05);
		noise_texture->connect_changed(callable_mp(tester.ptr(), &NoiseTextureTester::check_seamless_texture_grayscale));
		MessageQueue::get_singleton()->flush();
	}

	SUBCASE("16x16 modulated with default (transparent)black and white gradient (RGBA8), with seamless blend skirt of 1.0") {
		Ref<Gradient> gradient = memnew(Gradient);

		Vector<float> offsets = { 0.0, 1.0 };
		Vector<Color> colors = { Color(0, 0, 0, 0), Color(1, 1, 1, 1) };
		gradient->set_offsets(offsets);
		gradient->set_colors(colors);

		noise_texture->set_color_ramp(gradient);
		noise_texture->set_seamless_blend_skirt(1.0);
		noise_texture->connect_changed(callable_mp(tester.ptr(), &NoiseTextureTester::check_seamless_texture_rgba));
		MessageQueue::get_singleton()->flush();
	}
}

} //namespace TestNoiseTexture2D
