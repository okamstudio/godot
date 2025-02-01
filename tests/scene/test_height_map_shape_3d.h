/**************************************************************************/
/*  test_height_map_shape_3d.h                                            */
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

#include "scene/resources/3d/height_map_shape_3d.h"
#include "scene/resources/image_texture.h"

#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestHeightMapShape3D {

TEST_CASE("[SceneTree][HeightMapShape3D] Constructor") {
	Ref<HeightMapShape3D> height_map_shape = memnew(HeightMapShape3D);
	CHECK(height_map_shape->get_map_width() == 2);
	CHECK(height_map_shape->get_map_depth() == 2);
	CHECK(height_map_shape->get_map_data().size() == 4);
	CHECK(height_map_shape->get_min_height() == 0.0);
	CHECK(height_map_shape->get_max_height() == 0.0);
}

TEST_CASE("[SceneTree][HeightMapShape3D] set_map_width and get_map_width") {
	Ref<HeightMapShape3D> height_map_shape = memnew(HeightMapShape3D);
	height_map_shape->set_map_width(10);
	CHECK(height_map_shape->get_map_width() == 10);
}

TEST_CASE("[SceneTree][HeightMapShape3D] set_map_depth and get_map_depth") {
	Ref<HeightMapShape3D> height_map_shape = memnew(HeightMapShape3D);
	height_map_shape->set_map_depth(15);
	CHECK(height_map_shape->get_map_depth() == 15);
}

TEST_CASE("[SceneTree][HeightMapShape3D] set_map_data and get_map_data") {
	Ref<HeightMapShape3D> height_map_shape = memnew(HeightMapShape3D);
	Vector<real_t> map_data;
	map_data.push_back(1.0);
	map_data.push_back(2.0);
	height_map_shape->set_map_data(map_data);
	CHECK(height_map_shape->get_map_data().size() == 4.0);
	CHECK(height_map_shape->get_map_data()[0] == 0.0);
	CHECK(height_map_shape->get_map_data()[1] == 0.0);
}

TEST_CASE("[SceneTree][HeightMapShape3D] get_min_height") {
	Ref<HeightMapShape3D> height_map_shape = memnew(HeightMapShape3D);
	height_map_shape->set_map_width(3);
	height_map_shape->set_map_depth(1);
	height_map_shape->set_map_data(Vector<real_t>{ 1.0, 2.0, 0.5 });
	CHECK(height_map_shape->get_min_height() == 0.5);
}

TEST_CASE("[SceneTree][HeightMapShape3D] get_max_height") {
	Ref<HeightMapShape3D> height_map_shape = memnew(HeightMapShape3D);
	height_map_shape->set_map_width(3);
	height_map_shape->set_map_depth(1);
	height_map_shape->set_map_data(Vector<real_t>{ 1.0, 2.0, 0.5 });
	CHECK(height_map_shape->get_max_height() == 2.0);
}

TEST_CASE("[SceneTree][HeightMapShape3D] update_map_data_from_image") {
	// Create a HeightMapShape3D instance.
	Ref<HeightMapShape3D> height_map_shape = memnew(HeightMapShape3D);

	// Create a mock image with FORMAT_R8 and set its data.
	Vector<uint8_t> image_data;
	image_data.push_back(0);
	image_data.push_back(128);
	image_data.push_back(255);
	image_data.push_back(64);

	Ref<Image> image = memnew(Image);
	image->set_data(2, 2, false, Image::FORMAT_R8, image_data);

	height_map_shape->update_map_data_from_image(image, 0.0, 10.0);

	// Check the map data.
	Vector<real_t> expected_map_data = { 0.0, 5.0, 10.0, 2.5 };
	Vector<real_t> actual_map_data = height_map_shape->get_map_data();
	real_t tolerance = 0.1;

	for (int i = 0; i < expected_map_data.size(); ++i) {
		CHECK(Math::abs(actual_map_data[i] - expected_map_data[i]) < tolerance);
	}

	// Check the min and max heights.
	CHECK(height_map_shape->get_min_height() == 0.0);
	CHECK(height_map_shape->get_max_height() == 10.0);
}

} // namespace TestHeightMapShape3D
