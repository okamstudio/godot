/**************************************************************************/
/*  test_texture_progress_bar.h                                           */
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

#include "scene/gui/texture_progress_bar.h"

#include "tests/test_macros.h"

namespace TestTextureProgressBar {

TEST_CASE("[SceneTree][TextureProgressBar]") {
	TextureProgressBar *texture_progress_bar = memnew(TextureProgressBar);

	SUBCASE("[TextureProgressBar] set_radial_initial_angle() should wrap angle between 0 and 360 degrees (inclusive).") {
		texture_progress_bar->set_radial_initial_angle(0.0);
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)0.0));

		texture_progress_bar->set_radial_initial_angle(360.0);
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)360.0));

		texture_progress_bar->set_radial_initial_angle(30.5);
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)30.5));

		texture_progress_bar->set_radial_initial_angle(-30.5);
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)(360 - 30.5)));

		texture_progress_bar->set_radial_initial_angle(36000 + 30.5);
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)30.5));

		texture_progress_bar->set_radial_initial_angle(-(36000 + 30.5));
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)(360 - 30.5)));
	}

	SUBCASE("[TextureProgressBar] set_radial_initial_angle() should not set non-finite values.") {
		texture_progress_bar->set_radial_initial_angle(30.5);

		ERR_PRINT_OFF;
		texture_progress_bar->set_radial_initial_angle(INFINITY);
		ERR_PRINT_ON;
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)30.5));

		ERR_PRINT_OFF;
		texture_progress_bar->set_radial_initial_angle(-INFINITY);
		ERR_PRINT_ON;
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)30.5));

		ERR_PRINT_OFF;
		texture_progress_bar->set_radial_initial_angle(NAN);
		ERR_PRINT_ON;
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)30.5));

		ERR_PRINT_OFF;
		texture_progress_bar->set_radial_initial_angle(-NAN);
		ERR_PRINT_ON;
		CHECK(Math::is_equal_approx(texture_progress_bar->get_radial_initial_angle(), (float)30.5));
	}

	memdelete(texture_progress_bar);
}

} // namespace TestTextureProgressBar
