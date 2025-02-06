/**************************************************************************/
/*  test_image_texture.h                                                  */
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

#ifndef TEST_IMAGE_TEXTURE_H
#define TEST_IMAGE_TEXTURE_H

#include "scene/resources/image_texture.h"

#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestImageTexture {
const int default_height = 64;
const int default_width = 64;
const int default_channels = 3;
const Image::Format default_format = Image::Format::FORMAT_RGB8;

static Ref<Image> create_test_image_base(int p_channels, Image::Format p_format) {
	Vector<uint8_t> data;
	data.resize(default_width * default_height * p_channels);

	// This loop fills the data with image pixel values (RGBA format).
	for (int y = 0; y < default_height; y++) {
		for (int x = 0; x < default_width; x++) {
			int offset = (y * default_width + x) * p_channels;
			for (int c = 0; c < p_channels; c++) {
				data.set(offset + c, 255);
			}
		}
	}

	return Image::create_from_data(default_width, default_height, false, p_format, data);
}

static Ref<Image> create_test_image() {
	return create_test_image_base(default_channels, default_format);
}

static Ref<Image> create_test_image_with_alpha() {
	return create_test_image_base(4, Image::FORMAT_RGBA8);
}

TEST_CASE("[ImageTexture] Constructor") {
	// Create an ImageTexture object.
	Ref<ImageTexture> imageTexture;
	imageTexture.instantiate();

	// Ensure that it's not null.
	REQUIRE(imageTexture.is_valid());

	// Verify default values are correct.
	CHECK(imageTexture->get_width() == 0);
	CHECK(imageTexture->get_height() == 0);
	CHECK(imageTexture->get_format() == Image::FORMAT_L8);
	CHECK_FALSE(imageTexture->has_alpha());
}

TEST_CASE("[ImageTexture][SceneTree] Set image") {
	// Create an image for the texture.
	Ref<Image> image = create_test_image();

	// Create the image texture, initially as empty.
	Ref<ImageTexture> imageTexture;
	imageTexture.instantiate();

	// Set the image on the image texture.
	imageTexture->set_image(image);

	// Ensure that the image texture is not null.
	REQUIRE_FALSE(imageTexture.is_null());

	// Check if the properties are initialized based on the image.
	CHECK(imageTexture->get_width() == default_width);
	CHECK(imageTexture->get_height() == default_width);
	CHECK(imageTexture->get_format() == default_format);
	CHECK_FALSE(imageTexture->has_alpha());
}

TEST_CASE("[ImageTexture][SceneTree] Create from image") {
	// Create an image for the texture.
	Ref<Image> image = create_test_image();

	// Create an ImageTexture from the image.
	Ref<ImageTexture> imageTexture = ImageTexture::create_from_image(image);

	// Check if the ImageTexture's properties are updated.
	CHECK(imageTexture->get_width() == default_width);
	CHECK(imageTexture->get_height() == default_height);
	CHECK(imageTexture->get_format() == default_format);
	CHECK_FALSE(imageTexture->has_alpha());
}

TEST_CASE("[ImageTexture][SceneTree] Create with alpha channel") {
	// Create an image with an alpha channel.
	Ref<Image> image_with_alpha = create_test_image_with_alpha();

	// Create an image texture from the image.
	const Ref<ImageTexture> image_texture_with_alpha = ImageTexture::create_from_image(image_with_alpha);

	// Verify that the image texture has an alpha channel.
	CHECK(image_texture_with_alpha->has_alpha());
}

TEST_CASE("[ImageTexture][SceneTree] Check opaque pixels") {
	// Create an image with an alpha channel.
	Ref<Image> image = create_test_image_with_alpha();

	// Set different opacity levels in the image.
	for (int y = 0; y < default_width; y++) {
		for (int x = 0; x < default_height; x++) {
			Color pixel_color;
			if (x < default_width / 2) {
				// Make the left half of the image fully opaque.
				pixel_color = Color(1.0, 0.0, 0.0, 1.0);
			} else {
				// Make the right half of the image transparent.
				pixel_color = Color(0.0, 0.0, 1.0, 0.1);
			}
			image->set_pixel(x, y, pixel_color);
		}
	}

	// Create an image texture from the image.
	const Ref<ImageTexture> image_texture = ImageTexture::create_from_image(image);

	// Ensure that pixels on the left half are opaque.
	for (int y = 0; y < default_height; y++) {
		for (int x = 0; x < default_width / 2; x++) {
			bool opaque = image_texture->is_pixel_opaque(x, y);
			CHECK(opaque);
		}
	}

	// Ensure pixels on the right half are not opaque.
	for (int y = 0; y < default_height; y++) {
		for (int x = default_width / 2; x < default_width; x++) {
			bool opaque = image_texture->is_pixel_opaque(x, y);
			CHECK_FALSE(opaque);
		}
	}
}

TEST_CASE("[ImageTexture][SceneTree] Set size override") {
	// Create a test image.
	Ref<Image> image = create_test_image();

	// Create an ImageTexture using this image.
	Ref<ImageTexture> image_texture = ImageTexture::create_from_image(image);

	// Create and set a size override.
	int size_override_x = 128;
	int size_override_y = 128;
	Size2i size_override(size_override_x, size_override_y);
	image_texture->set_size_override(size_override);

	// Get the width and height after setting the size override.
	int overridden_width = image_texture->get_width();
	int overridden_height = image_texture->get_height();

	// Ensure that the width and height have been overridden.
	CHECK(overridden_width == size_override_x);
	CHECK(overridden_height == size_override_y);

	// Restore the original size.
	Size2i original_size(default_width, default_height);
	image_texture->set_size_override(original_size);

	// Get the width and height after restoring the original size.
	int updated_width = image_texture->get_width();
	int updated_height = image_texture->get_height();

	// Ensure that the width and height have been overridden to their original values.
	CHECK(updated_width == default_width);
	CHECK(updated_height == default_height);
}

TEST_CASE("[ImageTexture][SceneTree] Set path") {
	// Create an ImageTexture object.
	Ref<ImageTexture> image_texture;
	image_texture.instantiate();

	// Set a path and take it over.
	String path = TestUtils::get_data_path("scene/resources/64x64_1.png");
	image_texture->set_path(path, true);

	// Verify the path is correct.
	CHECK(image_texture->get_path() == path);

	// Set a new path without taking over.
	String new_path = TestUtils::get_data_path("scene/resources/64x64_2.png");
	image_texture->set_path(new_path, false);

	// Verify the new path is correct.
	CHECK(image_texture->get_path() == new_path);
}
} // namespace TestImageTexture

#endif // TEST_IMAGE_TEXTURE_H
