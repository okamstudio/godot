/**************************************************************************/
/*  test_option_button.h                                                  */
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

#include "scene/gui/option_button.h"

#include "tests/test_macros.h"

namespace TestOptionButton {

TEST_CASE("[SceneTree][OptionButton] Initialization") {
	OptionButton *test_opt = memnew(OptionButton);

	SUBCASE("There should be no options right after initialization") {
		CHECK_FALSE(test_opt->has_selectable_items());
		CHECK(test_opt->get_item_count() == 0);
	}

	memdelete(test_opt);
}

TEST_CASE("[SceneTree][OptionButton] Single item") {
	OptionButton *test_opt = memnew(OptionButton);

	SUBCASE("There should a single item after after adding one") {
		test_opt->add_item("single", 1013);

		CHECK(test_opt->has_selectable_items());
		CHECK(test_opt->get_item_count() == 1);
		CHECK(test_opt->get_item_index(1013) == 0);
		CHECK(test_opt->get_item_id(0) == 1013);

		test_opt->remove_item(0);

		CHECK_FALSE(test_opt->has_selectable_items());
		CHECK(test_opt->get_item_count() == 0);
	}

	SUBCASE("There should a single item after after adding an icon") {
		Ref<Texture2D> test_icon = memnew(Texture2D);
		test_opt->add_icon_item(test_icon, "icon", 345);

		CHECK(test_opt->has_selectable_items());
		CHECK(test_opt->get_item_count() == 1);
		CHECK(test_opt->get_item_index(345) == 0);
		CHECK(test_opt->get_item_id(0) == 345);

		test_opt->remove_item(0);

		CHECK_FALSE(test_opt->has_selectable_items());
		CHECK(test_opt->get_item_count() == 0);
	}

	memdelete(test_opt);
}

TEST_CASE("[SceneTree][OptionButton] Complex structure") {
	OptionButton *test_opt = memnew(OptionButton);

	SUBCASE("Creating a complex structure and checking getters") {
		// Regular item at index 0.
		Ref<Texture2D> test_icon1 = memnew(Texture2D);
		Ref<Texture2D> test_icon2 = memnew(Texture2D);
		// Regular item at index 3.
		Ref<Texture2D> test_icon4 = memnew(Texture2D);

		test_opt->add_item("first", 100);
		test_opt->add_icon_item(test_icon1, "second_icon", 101);
		test_opt->add_icon_item(test_icon2, "third_icon", 102);
		test_opt->add_item("fourth", 104);
		test_opt->add_icon_item(test_icon4, "fifth_icon", 104);

		// Disable test_icon4.
		test_opt->set_item_disabled(4, true);

		CHECK(test_opt->has_selectable_items());
		CHECK(test_opt->get_item_count() == 5);

		// Check for test_icon2.
		CHECK(test_opt->get_item_index(102) == 2);
		CHECK(test_opt->get_item_id(2) == 102);

		// Remove the two regular items.
		test_opt->remove_item(3);
		test_opt->remove_item(0);

		CHECK(test_opt->has_selectable_items());
		CHECK(test_opt->get_item_count() == 3);

		// Check test_icon4.
		CHECK(test_opt->get_item_index(104) == 2);
		CHECK(test_opt->get_item_id(2) == 104);

		// Remove the two non-disabled icon items.
		test_opt->remove_item(1);
		test_opt->remove_item(0);

		CHECK_FALSE(test_opt->has_selectable_items());
		CHECK(test_opt->get_item_count() == 1);
	}

	memdelete(test_opt);
}

} // namespace TestOptionButton
