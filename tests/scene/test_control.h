/**************************************************************************/
/*  test_control.h                                                        */
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

#ifndef TEST_CONTROL_H
#define TEST_CONTROL_H

#include "scene/gui/control.h"

#include "tests/test_macros.h"

namespace TestControl {

TEST_CASE("[SceneTree][Control]") {
	SUBCASE("[Control][Global Transform] Global Transform should be accessible while not in SceneTree.") { // GH-79453
		Control *test_node = memnew(Control);
		Control *test_child = memnew(Control);
		test_node->add_child(test_child);

		test_node->set_global_position(Point2(1, 1));
		CHECK_EQ(test_node->get_global_position(), Point2(1, 1));
		CHECK_EQ(test_child->get_global_position(), Point2(1, 1));
		test_node->set_global_position(Point2(2, 2));
		CHECK_EQ(test_node->get_global_position(), Point2(2, 2));
		test_node->set_scale(Vector2(4, 4));
		CHECK_EQ(test_node->get_global_transform(), Transform2D(0, Size2(4, 4), 0, Vector2(2, 2)));
		test_node->set_scale(Vector2(1, 1));
		test_node->set_rotation_degrees(90);
		CHECK_EQ(test_node->get_global_transform(), Transform2D(Math_PI / 2, Vector2(2, 2)));
		test_node->set_pivot_offset(Vector2(1, 0));
		CHECK_EQ(test_node->get_global_transform(), Transform2D(Math_PI / 2, Vector2(3, 1)));

		memdelete(test_child);
		memdelete(test_node);
	}
}

TEST_CASE("[SceneTree][Control] Focus") {
	Control *ctrl = memnew(Control);
	SceneTree::get_singleton()->get_root()->add_child(ctrl);

	SUBCASE("[SceneTree][Control] Default focus") {
		CHECK_FALSE(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Can't grab focus with default focus mode") {
		ERR_PRINT_OFF
		ctrl->grab_focus();
		ERR_PRINT_ON

		CHECK_FALSE(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Can grab focus") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->grab_focus();

		CHECK(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Can release focus") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->grab_focus();
		CHECK(ctrl->has_focus());

		ctrl->release_focus();
		CHECK_FALSE(ctrl->has_focus());
	}

	SUBCASE("[SceneTree][Control] Only one can grab focus at the same time") {
		ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		ctrl->grab_focus();
		CHECK(ctrl->has_focus());

		Control *other_ctrl = memnew(Control);
		SceneTree::get_singleton()->get_root()->add_child(other_ctrl);
		other_ctrl->set_focus_mode(Control::FocusMode::FOCUS_ALL);
		other_ctrl->grab_focus();

		CHECK(other_ctrl->has_focus());
		CHECK_FALSE(ctrl->has_focus());

		memdelete(other_ctrl);
	}

	memdelete(ctrl);
}

} // namespace TestControl

#endif // TEST_CONTROL_H
