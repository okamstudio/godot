/**************************************************************************/
/*  test_input_event_mouse.h                                              */
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

#include "core/input/input_event.h"
#include "tests/test_macros.h"

namespace TestInputEventMouse {

TEST_CASE("[InputEventMouse] Mouse button mask is set correctly") {
	InputEventMouse mousekey;

	mousekey.set_button_mask(MouseButtonMask::LEFT);
	CHECK(mousekey.get_button_mask().has_flag(MouseButtonMask::LEFT));

	mousekey.set_button_mask(MouseButtonMask::MB_XBUTTON1);
	CHECK(mousekey.get_button_mask().has_flag(MouseButtonMask::MB_XBUTTON1));

	mousekey.set_button_mask(MouseButtonMask::MB_XBUTTON2);
	CHECK(mousekey.get_button_mask().has_flag(MouseButtonMask::MB_XBUTTON2));

	mousekey.set_button_mask(MouseButtonMask::MIDDLE);
	CHECK(mousekey.get_button_mask().has_flag(MouseButtonMask::MIDDLE));

	mousekey.set_button_mask(MouseButtonMask::RIGHT);
	CHECK(mousekey.get_button_mask().has_flag(MouseButtonMask::RIGHT));
}

TEST_CASE("[InputEventMouse] Setting the mouse position works correctly") {
	InputEventMouse mousekey;

	mousekey.set_position(Vector2{ 10, 10 });
	CHECK(mousekey.get_position() == Vector2{ 10, 10 });

	mousekey.set_position(Vector2{ -1, -1 });
	CHECK(mousekey.get_position() == Vector2{ -1, -1 });
}

TEST_CASE("[InputEventMouse] Setting the global mouse position works correctly") {
	InputEventMouse mousekey;

	mousekey.set_global_position(Vector2{ 10, 10 });
	CHECK(mousekey.get_global_position() == Vector2{ 10, 10 });
	CHECK(mousekey.get_global_position() != Vector2{ 1, 1 });

	mousekey.set_global_position(Vector2{ -1, -1 });
	CHECK(mousekey.get_global_position() == Vector2{ -1, -1 });
	CHECK(mousekey.get_global_position() != Vector2{ 1, 1 });
}
} // namespace TestInputEventMouse
