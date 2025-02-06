/**************************************************************************/
/*  test_base_button.h                                                    */
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

#ifndef TEST_BASE_BUTTON_H
#define TEST_BASE_BUTTON_H

#include "scene/gui/base_button.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

namespace TestBaseButton {

// https://github.com/godotengine/godot/issues/43440

TEST_CASE("[SceneTree][BaseButton]") {
	BaseButton *base_button = memnew(BaseButton);
	base_button->set_size(Size2i(50, 50));
	base_button->set_position(Point2i(5, 5));
	Window *root = SceneTree::get_singleton()->get_root();
	root->add_child(base_button);

	Point2i onButtonTopLeft = Point2i(5, 5);
	Point2i onButtonBottomRight = Point2i(54, 54);
	Point2i offButtonTopLeft = Point2i(4, 4);
	Point2i offButtonBottomRight = Point2i(55, 55);

	Array args1;
	Array empty_args;
	empty_args.push_back(args1);

	Array trueArgs;
	trueArgs.push_back(true);
	Array toggled_on_args;
	toggled_on_args.push_back(trueArgs);

	Array falseArgs;
	falseArgs.push_back(false);
	Array toggled_off_args;
	toggled_off_args.push_back(falseArgs);

	SIGNAL_WATCH(base_button, "pressed");
	SIGNAL_WATCH(base_button, "button_down");
	SIGNAL_WATCH(base_button, "button_up");
	SIGNAL_WATCH(base_button, "toggled");

	SUBCASE("[BaseButton][Draw Mode] get_draw_mode works") {
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_NORMAL);
		base_button->set_disabled(true);
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_DISABLED);
		base_button->set_disabled(false);
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_NORMAL);
		SEND_GUI_MOUSE_MOTION_EVENT(onButtonTopLeft, MouseButtonMask::NONE, Key::NONE);
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_HOVER);
		SEND_GUI_MOUSE_MOTION_EVENT(offButtonTopLeft, MouseButtonMask::NONE, Key::NONE);
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_NORMAL);
		SEND_GUI_MOUSE_BUTTON_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_PRESSED);
		SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_HOVER);

		// TODO figure out how to properly trigger this
		// SEND_GUI_MOUSE_BUTTON_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		// CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_HOVER_PRESSED);
		// SEND_GUI_MOUSE_MOTION_EVENT(offButtonBottomRight, MouseButtonMask::NONE, Key::NONE);
		// CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_PRESSED);
		
		// SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_HOVER);
		SEND_GUI_MOUSE_MOTION_EVENT(offButtonBottomRight, MouseButtonMask::NONE, Key::NONE);
		CHECK_EQ(base_button->get_draw_mode(), BaseButton::DRAW_NORMAL);
	}

	// TODO redo this with different action mode
	SUBCASE("[BaseButton][Pressed] is_pressed is updated when mouse clicks and releases") {
		// Default state
		CHECK_FALSE(base_button->is_pressed());
		SIGNAL_CHECK_FALSE("pressed");
		SIGNAL_CHECK_FALSE("button_down");
		SIGNAL_CHECK_FALSE("button_up");
		SIGNAL_CHECK_FALSE("toggled");
		
		// Press the button
		SEND_GUI_MOUSE_BUTTON_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		CHECK(base_button->is_pressed());
		SIGNAL_CHECK_FALSE("pressed");
		SIGNAL_CHECK("button_down", empty_args);
		SIGNAL_CHECK_FALSE("button_up");
		SIGNAL_CHECK_FALSE("toggled");
		
		// Release the button
		SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		CHECK_FALSE(base_button->is_pressed());
		SIGNAL_CHECK("pressed", empty_args);
		SIGNAL_CHECK_FALSE("button_down");
		SIGNAL_CHECK("button_up", empty_args);
		SIGNAL_CHECK_FALSE("toggled");

		SUBCASE("is_pressed is updated when mouse releases for toggle_mode") {
			// Default state
			base_button->set_toggle_mode(true);
			CHECK_FALSE(base_button->is_pressed());
			SIGNAL_CHECK_FALSE("pressed");
			SIGNAL_CHECK_FALSE("button_down");
			SIGNAL_CHECK_FALSE("button_up");
			SIGNAL_CHECK_FALSE("toggled");

			// Press and release to toggle on
			SEND_GUI_MOUSE_BUTTON_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(base_button->is_pressed());
			SIGNAL_CHECK("button_down", empty_args);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK(base_button->is_pressed());
			SIGNAL_CHECK("pressed", empty_args);
			SIGNAL_CHECK_FALSE("button_down");
			SIGNAL_CHECK("button_up", empty_args);
			SIGNAL_CHECK("toggled", toggled_on_args);

			// Press and release to toggle off
			SEND_GUI_MOUSE_BUTTON_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK(base_button->is_pressed());
			SIGNAL_CHECK("button_down", empty_args);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(base_button->is_pressed());
			base_button->set_toggle_mode(false);
			SIGNAL_CHECK("pressed", empty_args);
			SIGNAL_CHECK_FALSE("button_down");
			SIGNAL_CHECK("button_up", empty_args);
			SIGNAL_CHECK("toggled", toggled_off_args);
		}
	}

	SUBCASE("[BaseButton][Pressing] is_pressing is updated when mouse clicks and releases") {
		CHECK_FALSE(base_button->is_pressing());
		SIGNAL_CHECK_FALSE("pressed");
		SIGNAL_CHECK_FALSE("button_down");
		SIGNAL_CHECK_FALSE("button_up");
		SIGNAL_CHECK_FALSE("toggled");
		
		SEND_GUI_MOUSE_BUTTON_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		
		CHECK(base_button->is_pressing());
		SIGNAL_CHECK_FALSE("pressed");
		SIGNAL_CHECK("button_down", empty_args);
		SIGNAL_CHECK_FALSE("button_up");
		SIGNAL_CHECK_FALSE("toggled");
		
		SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(onButtonTopLeft, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		
		CHECK_FALSE(base_button->is_pressing());
		SIGNAL_CHECK("pressed", empty_args);
		SIGNAL_CHECK_FALSE("button_down");
		SIGNAL_CHECK("button_up", empty_args);
		SIGNAL_CHECK_FALSE("toggled");
	}

	SUBCASE("[BaseButton][Hovering] is_hovered is updated when mouse enters and exits") {
		CHECK_FALSE(base_button->is_hovered());
		SEND_GUI_MOUSE_MOTION_EVENT(offButtonTopLeft, MouseButtonMask::NONE, Key::NONE);
		CHECK_FALSE(base_button->is_hovered());
		SEND_GUI_MOUSE_MOTION_EVENT(onButtonTopLeft, MouseButtonMask::NONE, Key::NONE);
		CHECK(base_button->is_hovered());
		SEND_GUI_MOUSE_MOTION_EVENT(onButtonBottomRight, MouseButtonMask::NONE, Key::NONE);
		CHECK(base_button->is_hovered());
		SEND_GUI_MOUSE_MOTION_EVENT(offButtonBottomRight, MouseButtonMask::NONE, Key::NONE);
		CHECK_FALSE(base_button->is_hovered());
	}

	SUBCASE("[BaseButton][Set Pressed] set_pressed works") {}
	SUBCASE("[BaseButton][] set_pressed_no_signal works") {}
	SUBCASE("[BaseButton][] set_toggle_mode works") {}
	SUBCASE("[BaseButton][] is_toggle_mode works") {}
	SUBCASE("[BaseButton][] set_shortcut_in_tooltip works") {}
	SUBCASE("[BaseButton][] is_shortcut_in_tooltip_enabled works") {}
	SUBCASE("[BaseButton][] set_disabled works") {}
	SUBCASE("[BaseButton][] is_disabled works") {}
	SUBCASE("[BaseButton][] set_action_mode works") {}
	SUBCASE("[BaseButton][] get_action_mode works") {}
	SUBCASE("[BaseButton][] set_keep_pressed_outside works") {}
	SUBCASE("[BaseButton][] is_keep_pressed_outside works") {}
	SUBCASE("[BaseButton][] set_shortcut_feedback works") {}
	SUBCASE("[BaseButton][] is_shortcut_feedback works") {}
	SUBCASE("[BaseButton][] set_button_mask works") {}
	SUBCASE("[BaseButton][] get_button_mask works") {}
	SUBCASE("[BaseButton][] set_shortcut works") {}
	SUBCASE("[BaseButton][] get_shortcut works") {}
	SUBCASE("[BaseButton][] get_tooltip works") {}
	SUBCASE("[BaseButton][] set_button_group works") {}
	SUBCASE("[BaseButton][] get_button_group works") {}
	SUBCASE("[BaseButton][] get_configuration_warnings works") {}

	SIGNAL_UNWATCH(base_button, "pressed");
	SIGNAL_UNWATCH(base_button, "button_down");
	SIGNAL_UNWATCH(base_button, "button_up");
	SIGNAL_UNWATCH(base_button, "toggled");

	memdelete(base_button);
}

} // namespace TestBaseButton

#endif // TEST_BASE_BUTTON_H
