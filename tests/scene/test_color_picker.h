/**************************************************************************/
/*  test_color_picker.h                                                   */
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

#include "scene/gui/color_picker.h"

#include "tests/test_macros.h"

namespace TestColorPicker {

TEST_CASE("[SceneTree][ColorPicker]") {
	ColorPicker *cp = memnew(ColorPicker);
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(cp);

	SUBCASE("[COLOR_PICKER] Mouse movement after Slider release") {
		Point2i pos_left = Point2i(50, 340); // On the left side of the red slider.
		Point2i pos_right = Point2i(200, 340); // On the right side of the red slider.
		SEND_GUI_MOUSE_MOTION_EVENT(pos_left, MouseButtonMask::NONE, Key::NONE);
		SEND_GUI_MOUSE_BUTTON_EVENT(pos_left, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
		CHECK(cp->get_pick_color().r < 0.5);
		SEND_GUI_MOUSE_MOTION_EVENT(pos_right, MouseButtonMask::LEFT, Key::NONE);
		CHECK(cp->get_pick_color().r > 0.5);
		SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(pos_right, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		SEND_GUI_MOUSE_MOTION_EVENT(pos_left, MouseButtonMask::NONE, Key::NONE);
		CHECK(cp->get_pick_color().r > 0.5); // Issue GH-77773.
	}
}

} // namespace TestColorPicker
