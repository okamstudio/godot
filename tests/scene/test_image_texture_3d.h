/**************************************************************************/
/*  test_image_texture_3d.h                                               */
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

namespace TestImageTexture3D {

// [SceneTree] in a test case name enables initializing a mock render server,
// which ImageTexture3D is dependent on.
TEST_CASE("[SceneTree][ImageTexture3D] Constructor") {
	Ref<ImageTexture3D> image_texture_3d = memnew(ImageTexture3D);
	CHECK(image_texture_3d->get_format() == Image::FORMAT_L8);
	CHECK(image_texture_3d->get_width() == 1);
	CHECK(image_texture_3d->get_height() == 1);
	CHECK(image_texture_3d->get_depth() == 1);
	CHECK(image_texture_3d->has_mipmaps() == false);
}

TEST_CASE("[SceneTree][ImageTexture3D] get_format") {
	Ref<ImageTexture3D> image_texture_3d = memnew(ImageTexture3D);
	CHECK(image_texture_3d->get_format() == Image::FORMAT_L8);
}

TEST_CASE("[SceneTree][ImageTexture3D] get_width") {
	Ref<ImageTexture3D> image_texture_3d = memnew(ImageTexture3D);
	CHECK(image_texture_3d->get_width() == 1);
}

TEST_CASE("[SceneTree][ImageTexture3D] get_height") {
	Ref<ImageTexture3D> image_texture_3d = memnew(ImageTexture3D);
	CHECK(image_texture_3d->get_height() == 1);
}

TEST_CASE("[SceneTree][ImageTexture3D] get_depth") {
	Ref<ImageTexture3D> image_texture_3d = memnew(ImageTexture3D);
	CHECK(image_texture_3d->get_depth() == 1);
}

TEST_CASE("[SceneTree][ImageTexture3D] has_mipmaps") {
	const Vector<Ref<Image>> images = { memnew(Image(8, 8, false, Image::FORMAT_RGBA8)), memnew(Image(8, 8, false, Image::FORMAT_RGBA8)) };
	Ref<ImageTexture3D> image_texture_3d = memnew(ImageTexture3D);
	CHECK(image_texture_3d->has_mipmaps() == false); // No mipmaps.
	image_texture_3d->create(Image::FORMAT_RGBA8, 2, 2, 2, true, images);
	CHECK(image_texture_3d->has_mipmaps() == true); // Mipmaps.
}

TEST_CASE("[SceneTree][ImageTexture3D] create") {
	const Vector<Ref<Image>> images = { memnew(Image(8, 8, false, Image::FORMAT_RGBA8)), memnew(Image(8, 8, false, Image::FORMAT_RGBA8)) };
	Ref<ImageTexture3D> image_texture_3d = memnew(ImageTexture3D);
	CHECK(image_texture_3d->create(Image::FORMAT_RGBA8, 2, 2, 2, true, images) == OK); // Run create and check return value simultaneously.
	CHECK(image_texture_3d->get_format() == Image::FORMAT_RGBA8);
	CHECK(image_texture_3d->get_width() == 2);
	CHECK(image_texture_3d->get_height() == 2);
	CHECK(image_texture_3d->get_depth() == 2);
	CHECK(image_texture_3d->has_mipmaps() == true);
}

TEST_CASE("[SceneTree][ImageTexture3D] set_path") {
	Ref<ImageTexture3D> image_texture_3d = memnew(ImageTexture3D);
	String path = TestUtils::get_data_path("images/icon.png");
	image_texture_3d->set_path(path, true);
	CHECK(image_texture_3d->get_path() == path);
}

} //namespace TestImageTexture3D
