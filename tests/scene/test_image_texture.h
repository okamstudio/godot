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

#pragma once

#include "core/io/image.h"
#include "scene/resources/image_texture.h"

#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestImageTexture {

// [SceneTree] in a test case name enables initializing a mock render server,
// which ImageTexture is dependent on.
TEST_CASE("[SceneTree][ImageTexture] constructor") {
	Ref<ImageTexture> image_texture = memnew(ImageTexture);
	CHECK(image_texture->get_width() == 0);
	CHECK(image_texture->get_height() == 0);
	CHECK(image_texture->get_format() == 0);
	CHECK(image_texture->has_alpha() == false);
	CHECK(image_texture->get_image() == Ref<Image>());
}

TEST_CASE("[SceneTree][ImageTexture] create_from_image") {
	Ref<Image> image = memnew(Image(16, 8, true, Image::FORMAT_RGBA8));
	Ref<ImageTexture> image_texture = ImageTexture::create_from_image(image);
	CHECK(image_texture->get_width() == 16);
	CHECK(image_texture->get_height() == 8);
	CHECK(image_texture->get_format() == Image::FORMAT_RGBA8);
	CHECK(image_texture->has_alpha() == true);
	CHECK(image_texture->get_rid().is_valid() == true);
}

TEST_CASE("[SceneTree][ImageTexture] set_image") {
	Ref<ImageTexture> image_texture = memnew(ImageTexture);
	Ref<Image> image = memnew(Image(8, 4, false, Image::FORMAT_RGB8));
	image_texture->set_image(image);
	CHECK(image_texture->get_width() == 8);
	CHECK(image_texture->get_height() == 4);
	CHECK(image_texture->get_format() == Image::FORMAT_RGB8);
	CHECK(image_texture->has_alpha() == false);
	CHECK(image_texture->get_width() == image_texture->get_image()->get_width());
	CHECK(image_texture->get_height() == image_texture->get_image()->get_height());
	CHECK(image_texture->get_format() == image_texture->get_image()->get_format());
}

TEST_CASE("[SceneTree][ImageTexture] set_size_override") {
	Ref<Image> image = memnew(Image(16, 8, false, Image::FORMAT_RGB8));
	Ref<ImageTexture> image_texture = ImageTexture::create_from_image(image);
	CHECK(image_texture->get_width() == 16);
	CHECK(image_texture->get_height() == 8);
	image_texture->set_size_override(Size2i(32, 16));
	CHECK(image_texture->get_width() == 32);
	CHECK(image_texture->get_height() == 16);
}

TEST_CASE("[SceneTree][ImageTexture] is_pixel_opaque") {
	Ref<Image> image = memnew(Image(8, 8, false, Image::FORMAT_RGBA8));
	image->set_pixel(0, 0, Color(0.0, 0.0, 0.0, 0.0)); // not opaque
	image->set_pixel(0, 1, Color(0.0, 0.0, 0.0, 0.1)); // not opaque
	image->set_pixel(0, 2, Color(0.0, 0.0, 0.0, 0.5)); // opaque
	image->set_pixel(0, 3, Color(0.0, 0.0, 0.0, 0.9)); // opaque
	image->set_pixel(0, 4, Color(0.0, 0.0, 0.0, 1.0)); // opaque

	Ref<ImageTexture> image_texture = ImageTexture::create_from_image(image);
	CHECK(image_texture->is_pixel_opaque(0, 0) == false);
	CHECK(image_texture->is_pixel_opaque(0, 1) == false);
	CHECK(image_texture->is_pixel_opaque(0, 2) == true);
	CHECK(image_texture->is_pixel_opaque(0, 3) == true);
	CHECK(image_texture->is_pixel_opaque(0, 4) == true);
}

TEST_CASE("[SceneTree][ImageTexture] set_path") {
	Ref<ImageTexture> image_texture = memnew(ImageTexture);
	String path = TestUtils::get_data_path("images/icon.png");
	image_texture->set_path(path, true);
	CHECK(image_texture->get_path() == path);
}

} //namespace TestImageTexture
