/**************************************************************************/
/*  test_image.h                                                          */
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

#ifndef TEST_IMAGE_H
#define TEST_IMAGE_H

#include "core/io/image.h"
#include "core/os/os.h"

#include "tests/test_utils.h"

#include "modules/modules_enabled.gen.h"

#include "thirdparty/doctest/doctest.h"

namespace TestImage {

TEST_CASE("[Image] Instantiation") {
	Ref<Image> image = memnew(Image(8, 4, false, Image::FORMAT_RGBA8));
	CHECK_MESSAGE(
			!image->is_empty(),
			"An image created with specified size and format should not be empty at first.");
	CHECK_MESSAGE(
			image->is_invisible(),
			"A newly created image should be invisible.");
	CHECK_MESSAGE(
			!image->is_compressed(),
			"A newly created image should not be compressed.");
	CHECK(!image->has_mipmaps());

	PackedByteArray image_data = image->get_data();
	for (int i = 0; i < image_data.size(); i++) {
		CHECK_MESSAGE(
				image_data[i] == 0,
				"An image created without data specified should have its data zeroed out.");
	}

	Ref<Image> image_copy = memnew(Image());
	CHECK_MESSAGE(
			image_copy->is_empty(),
			"An image created without any specified size and format be empty at first.");
	image_copy->copy_internals_from(image);

	CHECK_MESSAGE(
			image->get_data() == image_copy->get_data(),
			"Duplicated images should have the same data.");

	image_data = image->get_data();
	Ref<Image> image_from_data = memnew(Image(8, 4, false, Image::FORMAT_RGBA8, image_data));
	CHECK_MESSAGE(
			image->get_data() == image_from_data->get_data(),
			"An image created from data of another image should have the same data of the original image.");
}

TEST_CASE("[Image] Saving and loading") {
	Ref<Image> image = memnew(Image(4, 4, false, Image::FORMAT_RGBA8));
	const String save_path_png = TestUtils::get_temp_path("image.png");
	const String save_path_exr = TestUtils::get_temp_path("image.exr");

	// Save PNG
	Error err;
	err = image->save_png(save_path_png);
	CHECK_MESSAGE(
			err == OK,
			"The image should be saved successfully as a .png file.");

	// Only available on editor builds.
#ifdef TOOLS_ENABLED
	// Save EXR
	err = image->save_exr(save_path_exr, false);
	CHECK_MESSAGE(
			err == OK,
			"The image should be saved successfully as an .exr file.");
#endif // TOOLS_ENABLED

	// Load using load()
	Ref<Image> image_load = memnew(Image());
	err = image_load->load(save_path_png);
	CHECK_MESSAGE(
			err == OK,
			"The image should load successfully using load().");
	CHECK_MESSAGE(
			image->get_data() == image_load->get_data(),
			"The loaded image should have the same data as the one that got saved.");

#ifdef MODULE_BMP_ENABLED
	// Load BMP
	Ref<Image> image_bmp = memnew(Image());
	Ref<FileAccess> f_bmp = FileAccess::open(TestUtils::get_data_path("images/icon.bmp"), FileAccess::READ, &err);
	REQUIRE(f_bmp.is_valid());
	PackedByteArray data_bmp;
	data_bmp.resize(f_bmp->get_length() + 1);
	f_bmp->get_buffer(data_bmp.ptrw(), f_bmp->get_length());
	CHECK_MESSAGE(
			image_bmp->load_bmp_from_buffer(data_bmp) == OK,
			"The BMP image should load successfully.");
#endif // MODULE_BMP_ENABLED

#ifdef MODULE_JPG_ENABLED
	// Load JPG
	Ref<Image> image_jpg = memnew(Image());
	Ref<FileAccess> f_jpg = FileAccess::open(TestUtils::get_data_path("images/icon.jpg"), FileAccess::READ, &err);
	REQUIRE(f_jpg.is_valid());
	PackedByteArray data_jpg;
	data_jpg.resize(f_jpg->get_length() + 1);
	f_jpg->get_buffer(data_jpg.ptrw(), f_jpg->get_length());
	CHECK_MESSAGE(
			image_jpg->load_jpg_from_buffer(data_jpg) == OK,
			"The JPG image should load successfully.");
#endif // MODULE_JPG_ENABLED

#ifdef MODULE_WEBP_ENABLED
	// Load WebP
	Ref<Image> image_webp = memnew(Image());
	Ref<FileAccess> f_webp = FileAccess::open(TestUtils::get_data_path("images/icon.webp"), FileAccess::READ, &err);
	REQUIRE(f_webp.is_valid());
	PackedByteArray data_webp;
	data_webp.resize(f_webp->get_length() + 1);
	f_webp->get_buffer(data_webp.ptrw(), f_webp->get_length());
	CHECK_MESSAGE(
			image_webp->load_webp_from_buffer(data_webp) == OK,
			"The WebP image should load successfully.");
#endif // MODULE_WEBP_ENABLED

	// Load PNG
	Ref<Image> image_png = memnew(Image());
	Ref<FileAccess> f_png = FileAccess::open(TestUtils::get_data_path("images/icon.png"), FileAccess::READ, &err);
	REQUIRE(f_png.is_valid());
	PackedByteArray data_png;
	data_png.resize(f_png->get_length() + 1);
	f_png->get_buffer(data_png.ptrw(), f_png->get_length());
	CHECK_MESSAGE(
			image_png->load_png_from_buffer(data_png) == OK,
			"The PNG image should load successfully.");

#ifdef MODULE_TGA_ENABLED
	// Load TGA
	Ref<Image> image_tga = memnew(Image());
	Ref<FileAccess> f_tga = FileAccess::open(TestUtils::get_data_path("images/icon.tga"), FileAccess::READ, &err);
	REQUIRE(f_tga.is_valid());
	PackedByteArray data_tga;
	data_tga.resize(f_tga->get_length() + 1);
	f_tga->get_buffer(data_tga.ptrw(), f_tga->get_length());
	CHECK_MESSAGE(
			image_tga->load_tga_from_buffer(data_tga) == OK,
			"The TGA image should load successfully.");
#endif // MODULE_TGA_ENABLED
}

TEST_CASE("[Image] Basic getters") {
	Ref<Image> image = memnew(Image(8, 4, false, Image::FORMAT_LA8));
	CHECK(image->get_width() == 8);
	CHECK(image->get_height() == 4);
	CHECK(image->get_size() == Vector2(8, 4));
	CHECK(image->get_format() == Image::FORMAT_LA8);
	CHECK(image->get_used_rect() == Rect2i(0, 0, 0, 0));
	Ref<Image> image_get_rect = image->get_region(Rect2i(0, 0, 2, 1));
	CHECK(image_get_rect->get_size() == Vector2(2, 1));
}

TEST_CASE("[Image] Resizing") {
	Ref<Image> image = memnew(Image(8, 8, false, Image::FORMAT_RGBA8));
	// Crop
	image->crop(4, 4);
	CHECK_MESSAGE(
			image->get_size() == Vector2(4, 4),
			"get_size() should return the correct size after cropping.");
	image->set_pixel(0, 0, Color(1, 1, 1, 1));

	// Resize
	for (int i = 0; i < 5; i++) {
		Ref<Image> image_resized = memnew(Image());
		image_resized->copy_internals_from(image);
		Image::Interpolation interpolation = static_cast<Image::Interpolation>(i);
		image_resized->resize(8, 8, interpolation);
		CHECK_MESSAGE(
				image_resized->get_size() == Vector2(8, 8),
				"get_size() should return the correct size after resizing.");
		CHECK_MESSAGE(
				image_resized->get_pixel(1, 1).a > 0,
				"Resizing an image should also affect its content.");
	}

	// shrink_x2()
	image->shrink_x2();
	CHECK_MESSAGE(
			image->get_size() == Vector2(2, 2),
			"get_size() should return the correct size after shrink_x2().");

	// resize_to_po2()
	Ref<Image> image_po_2 = memnew(Image(14, 28, false, Image::FORMAT_RGBA8));
	image_po_2->resize_to_po2();
	CHECK_MESSAGE(
			image_po_2->get_size() == Vector2(16, 32),
			"get_size() should return the correct size after resize_to_po2().");
}

TEST_CASE("[Image] Modifying pixels of an image") {
	Ref<Image> image = memnew(Image(3, 3, false, Image::FORMAT_RGBA8));
	image->set_pixel(0, 0, Color(1, 1, 1, 1));
	CHECK_MESSAGE(
			!image->is_invisible(),
			"Image should not be invisible after drawing on it.");
	CHECK_MESSAGE(
			image->get_pixelv(Vector2(0, 0)).is_equal_approx(Color(1, 1, 1, 1)),
			"Image's get_pixel() should return the same color value as the one being set with set_pixel() in the same position.");
	CHECK_MESSAGE(
			image->get_used_rect() == Rect2i(0, 0, 1, 1),
			"Image's get_used_rect should return the expected value, larger than Rect2i(0, 0, 0, 0) if it's visible.");

	image->set_pixelv(Vector2(0, 0), Color(0.5, 0.5, 0.5, 0.5));
	Ref<Image> image2 = memnew(Image(3, 3, false, Image::FORMAT_RGBA8));

	// Fill image with color
	image2->fill(Color(0.5, 0.5, 0.5, 0.5));
	for (int y = 0; y < image2->get_height(); y++) {
		for (int x = 0; x < image2->get_width(); x++) {
			CHECK_MESSAGE(
					image2->get_pixel(x, y).r > 0.49,
					"fill() should colorize all pixels of the image.");
		}
	}

	// Fill rect with color
	{
		const int img_width = 3;
		const int img_height = 3;
		Vector<Rect2i> rects;
		rects.push_back(Rect2i());
		rects.push_back(Rect2i(-5, -5, 3, 3));
		rects.push_back(Rect2i(img_width, 0, 12, 12));
		rects.push_back(Rect2i(0, img_height, 12, 12));
		rects.push_back(Rect2i(img_width + 1, img_height + 2, 12, 12));
		rects.push_back(Rect2i(1, 1, 1, 1));
		rects.push_back(Rect2i(0, 1, 2, 3));
		rects.push_back(Rect2i(-5, 0, img_width + 10, 2));
		rects.push_back(Rect2i(0, -5, 2, img_height + 10));
		rects.push_back(Rect2i(-1, -1, img_width + 1, img_height + 1));

		for (const Rect2i &rect : rects) {
			Ref<Image> img = memnew(Image(img_width, img_height, false, Image::FORMAT_RGBA8));
			img->fill_rect(rect, Color(1, 1, 1, 1));
			for (int y = 0; y < img->get_height(); y++) {
				for (int x = 0; x < img->get_width(); x++) {
					if (rect.abs().has_point(Point2(x, y))) {
						CHECK_MESSAGE(
								img->get_pixel(x, y).is_equal_approx(Color(1, 1, 1, 1)),
								"fill_rect() should colorize all image pixels within rect bounds.");
					} else {
						CHECK_MESSAGE(
								!img->get_pixel(x, y).is_equal_approx(Color(1, 1, 1, 1)),
								"fill_rect() shouldn't colorize any image pixel out of rect bounds.");
					}
				}
			}
		}
	}

	// Blend two images together
	image->blend_rect(image2, Rect2i(Vector2i(0, 0), image2->get_size()), Vector2i(0, 0));
	CHECK_MESSAGE(
			image->get_pixel(0, 0).a > 0.7,
			"blend_rect() should blend the alpha values of the two images.");
	CHECK_MESSAGE(
			image->get_used_rect().size == image->get_size(),
			"get_used_rect() should return the expected value, its Rect size should be the same as get_size() if there are no transparent pixels.");

	Ref<Image> image3 = memnew(Image(2, 2, false, Image::FORMAT_RGBA8));
	image3->set_pixel(0, 0, Color(0, 1, 0, 1));

	//blit_rect() two images together
	image->blit_rect(image3, Rect2i(Vector2i(0, 0), image3->get_size()), Vector2i(0, 0));
	CHECK_MESSAGE(
			image->get_pixel(0, 0).is_equal_approx(Color(0, 1, 0, 1)),
			"blit_rect() should replace old colors and not blend them.");
	CHECK_MESSAGE(
			!image->get_pixel(2, 2).is_equal_approx(Color(0, 1, 0, 1)),
			"blit_rect() should not affect the area of the image that is outside src_rect.");

	// Flip image
	image3->flip_x();
	CHECK(image3->get_pixel(1, 0).is_equal_approx(Color(0, 1, 0, 1)));
	CHECK_MESSAGE(
			image3->get_pixel(0, 0).is_equal_approx(Color(0, 0, 0, 0)),
			"flip_x() should not leave old pixels behind.");
	image3->flip_y();
	CHECK(image3->get_pixel(1, 1).is_equal_approx(Color(0, 1, 0, 1)));
	CHECK_MESSAGE(
			image3->get_pixel(1, 0).is_equal_approx(Color(0, 0, 0, 0)),
			"flip_y() should not leave old pixels behind.");

	// Pre-multiply Alpha then Convert from RGBA to L8, checking alpha
	{
		Ref<Image> gray_image = memnew(Image(3, 3, false, Image::FORMAT_RGBA8));
		gray_image->fill_rect(Rect2i(0, 0, 3, 3), Color(1, 1, 1, 0));
		gray_image->set_pixel(1, 1, Color(1, 1, 1, 1));
		gray_image->set_pixel(1, 2, Color(0.5, 0.5, 0.5, 0.5));
		gray_image->set_pixel(2, 1, Color(0.25, 0.05, 0.5, 1.0));
		gray_image->set_pixel(2, 2, Color(0.5, 0.25, 0.95, 0.75));
		gray_image->premultiply_alpha();
		gray_image->convert(Image::FORMAT_L8);
		CHECK_MESSAGE(gray_image->get_pixel(0, 0).is_equal_approx(Color(0, 0, 0, 1)), "convert() RGBA to L8 should be black.");
		CHECK_MESSAGE(gray_image->get_pixel(0, 1).is_equal_approx(Color(0, 0, 0, 1)), "convert() RGBA to L8 should be black.");
		CHECK_MESSAGE(gray_image->get_pixel(0, 2).is_equal_approx(Color(0, 0, 0, 1)), "convert() RGBA to L8 should be black.");
		CHECK_MESSAGE(gray_image->get_pixel(1, 0).is_equal_approx(Color(0, 0, 0, 1)), "convert() RGBA to L8 should be black.");
		CHECK_MESSAGE(gray_image->get_pixel(1, 1).is_equal_approx(Color(1, 1, 1, 1)), "convert() RGBA to L8 should be white.");
		CHECK_MESSAGE(gray_image->get_pixel(1, 2).is_equal_approx(Color(0.250980407, 0.250980407, 0.250980407, 1)), "convert() RGBA to L8 should be around 0.250980407 (64).");
		CHECK_MESSAGE(gray_image->get_pixel(2, 0).is_equal_approx(Color(0, 0, 0, 1)), "convert() RGBA to L8 should be black.");
		CHECK_MESSAGE(gray_image->get_pixel(2, 1).is_equal_approx(Color(0.121568628, 0.121568628, 0.121568628, 1)), "convert() RGBA to L8 should be around 0.121568628 (31).");
		CHECK_MESSAGE(gray_image->get_pixel(2, 2).is_equal_approx(Color(0.266666681, 0.266666681, 0.266666681, 1)), "convert() RGBA to L8 should be around 0.266666681 (68).");
	}
}

TEST_CASE("[Image] Custom mipmaps") {
	Ref<Image> image = memnew(Image(100, 100, false, Image::FORMAT_RGBA8));

	REQUIRE(!image->has_mipmaps());
	image->generate_mipmaps();
	REQUIRE(image->has_mipmaps());

	const int mipmaps = image->get_mipmap_count() + 1;
	REQUIRE(mipmaps == 7);

	// Initialize reference mipmap data.
	// Each byte is given value "mipmap_index * 5".

	{
		PackedByteArray data = image->get_data();
		uint8_t *data_ptr = data.ptrw();

		for (int mip = 0; mip < mipmaps; mip++) {
			int64_t mip_offset = 0;
			int64_t mip_size = 0;
			image->get_mipmap_offset_and_size(mip, mip_offset, mip_size);

			for (int i = 0; i < mip_size; i++) {
				data_ptr[mip_offset + i] = mip * 5;
			}
		}
		image->set_data(image->get_width(), image->get_height(), image->has_mipmaps(), image->get_format(), data);
	}

	// Byte format conversion.

	for (int format = Image::FORMAT_L8; format <= Image::FORMAT_RGBA8; format++) {
		Ref<Image> image_bytes = memnew(Image());
		image_bytes->copy_internals_from(image);
		image_bytes->convert((Image::Format)format);
		REQUIRE(image_bytes->has_mipmaps());

		PackedByteArray data = image_bytes->get_data();
		const uint8_t *data_ptr = data.ptr();

		for (int mip = 0; mip < mipmaps; mip++) {
			int64_t mip_offset = 0;
			int64_t mip_size = 0;
			image_bytes->get_mipmap_offset_and_size(mip, mip_offset, mip_size);

			for (int i = 0; i < mip_size; i++) {
				if (data_ptr[mip_offset + i] != mip * 5) {
					REQUIRE_MESSAGE(false, "Byte format conversion error.");
				}
			}
		}
	}

	// Floating point format conversion.

	for (int format = Image::FORMAT_RF; format <= Image::FORMAT_RGBAF; format++) {
		Ref<Image> image_rgbaf = memnew(Image());
		image_rgbaf->copy_internals_from(image);
		image_rgbaf->convert((Image::Format)format);
		REQUIRE(image_rgbaf->has_mipmaps());

		PackedByteArray data = image_rgbaf->get_data();
		const uint8_t *data_ptr = data.ptr();

		for (int mip = 0; mip < mipmaps; mip++) {
			int64_t mip_offset = 0;
			int64_t mip_size = 0;
			image_rgbaf->get_mipmap_offset_and_size(mip, mip_offset, mip_size);

			for (int i = 0; i < mip_size; i += 4) {
				float value = *(float *)(data_ptr + mip_offset + i);
				if (!Math::is_equal_approx(value * 255.0f, mip * 5)) {
					REQUIRE_MESSAGE(false, "Floating point conversion error.");
				}
			}
		}
	}
}

TEST_CASE("[Image] Convert image") {
	for (int format = Image::FORMAT_RF; format < Image::FORMAT_RGBE9995; format++) {
		for (int new_format = Image::FORMAT_RF; new_format < Image::FORMAT_RGBE9995; new_format++) {
			Ref<Image> image = memnew(Image(4, 4, false, (Image::Format)format));
			image->convert((Image::Format)new_format);
			String format_string = Image::format_names[(Image::Format)format];
			String new_format_string = Image::format_names[(Image::Format)new_format];
			format_string = "Error converting from " + format_string + " to " + new_format_string + ".";
			CHECK_MESSAGE(image->get_format() == new_format, format_string);
		}
	}

	Ref<Image> image = memnew(Image(4, 4, false, Image::FORMAT_RGBA8));
	PackedByteArray image_data = image->get_data();
	ERR_PRINT_OFF;
	image->convert((Image::Format)-1);
	ERR_PRINT_ON;
	CHECK_MESSAGE(image->get_data() == image_data, "Image conversion to invalid type (-1) should not alter image.");
	Ref<Image> image2 = memnew(Image(4, 4, false, Image::FORMAT_RGBA8));
	image_data = image2->get_data();
	ERR_PRINT_OFF;
	image2->convert((Image::Format)(Image::FORMAT_MAX + 1));
	ERR_PRINT_ON;
	CHECK_MESSAGE(image2->get_data() == image_data, "Image conversion to invalid type (Image::FORMAT_MAX + 1) should not alter image.");
}

} // namespace TestImage

#endif // TEST_IMAGE_H
