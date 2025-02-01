/**************************************************************************/
/*  test_gradient_texture.h                                               */
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

#include "scene/resources/gradient_texture.h"

#include "tests/test_macros.h"

namespace TestGradientTexture {

// [SceneTree] in a test case name enables initializing a mock render server,
// which ImageTexture is dependent on.
TEST_CASE("[SceneTree][GradientTexture1D] Create GradientTexture1D") {
	Ref<GradientTexture1D> gradient_texture = memnew(GradientTexture1D);

	Ref<Gradient> test_gradient = memnew(Gradient);
	gradient_texture->set_gradient(test_gradient);
	CHECK(gradient_texture->get_gradient() == test_gradient);

	gradient_texture->set_width(83);
	CHECK(gradient_texture->get_width() == 83);

	gradient_texture->set_use_hdr(true);
	CHECK(gradient_texture->is_using_hdr());
}

TEST_CASE("[SceneTree][GradientTexture2D] Create GradientTexture2D") {
	Ref<GradientTexture2D> gradient_texture = memnew(GradientTexture2D);

	Ref<Gradient> test_gradient = memnew(Gradient);
	gradient_texture->set_gradient(test_gradient);
	CHECK(gradient_texture->get_gradient() == test_gradient);

	gradient_texture->set_width(82);
	CHECK(gradient_texture->get_width() == 82);

	gradient_texture->set_height(81);
	CHECK(gradient_texture->get_height() == 81);

	gradient_texture->set_use_hdr(true);
	CHECK(gradient_texture->is_using_hdr());

	gradient_texture->set_fill(GradientTexture2D::Fill::FILL_SQUARE);
	CHECK(gradient_texture->get_fill() == GradientTexture2D::Fill::FILL_SQUARE);

	gradient_texture->set_fill_from(Vector2(0.2, 0.25));
	CHECK(gradient_texture->get_fill_from() == Vector2(0.2, 0.25));

	gradient_texture->set_fill_to(Vector2(0.35, 0.5));
	CHECK(gradient_texture->get_fill_to() == Vector2(0.35, 0.5));

	gradient_texture->set_repeat(GradientTexture2D::Repeat::REPEAT);
	CHECK(gradient_texture->get_repeat() == GradientTexture2D::Repeat::REPEAT);
}

} //namespace TestGradientTexture
