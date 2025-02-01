/**************************************************************************/
/*  test_tab_bar.h                                                        */
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

#include "scene/gui/tab_bar.h"
#include "scene/main/window.h"

#include "tests/test_macros.h"

namespace TestTabBar {
static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}

TEST_CASE("[SceneTree][TabBar] tab operations") {
	TabBar *tab_bar = memnew(TabBar);
	SceneTree::get_singleton()->get_root()->add_child(tab_bar);
	tab_bar->set_clip_tabs(false);
	MessageQueue::get_singleton()->flush();

	SIGNAL_WATCH(tab_bar, "tab_selected");
	SIGNAL_WATCH(tab_bar, "tab_changed");

	SUBCASE("[TabBar] no tabs") {
		CHECK(tab_bar->get_tab_count() == 0);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
	}

	SUBCASE("[TabBar] add tabs") {
		tab_bar->add_tab("tab0");
		CHECK(tab_bar->get_tab_count() == 1);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(0)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(0)));

		tab_bar->add_tab("tab1");
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		tab_bar->add_tab("tab2");
		CHECK(tab_bar->get_tab_count() == 3);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		CHECK(tab_bar->get_tab_title(0) == "tab0");
		CHECK(tab_bar->get_tab_tooltip(0) == "");
		CHECK(tab_bar->get_tab_text_direction(0) == Control::TEXT_DIRECTION_INHERITED);
		CHECK_FALSE(tab_bar->is_tab_disabled(0));
		CHECK_FALSE(tab_bar->is_tab_hidden(0));

		CHECK(tab_bar->get_tab_title(1) == "tab1");
		CHECK(tab_bar->get_tab_tooltip(1) == "");
		CHECK(tab_bar->get_tab_text_direction(1) == Control::TEXT_DIRECTION_INHERITED);
		CHECK_FALSE(tab_bar->is_tab_disabled(1));
		CHECK_FALSE(tab_bar->is_tab_hidden(1));

		CHECK(tab_bar->get_tab_title(2) == "tab2");
		CHECK(tab_bar->get_tab_tooltip(2) == "");
		CHECK(tab_bar->get_tab_text_direction(2) == Control::TEXT_DIRECTION_INHERITED);
		CHECK_FALSE(tab_bar->is_tab_disabled(2));
		CHECK_FALSE(tab_bar->is_tab_hidden(2));
	}

	SUBCASE("[TabBar] set tab count") {
		// Adds multiple tabs at once.
		tab_bar->set_tab_count(3);
		CHECK(tab_bar->get_tab_count() == 3);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		CHECK(tab_bar->get_tab_title(0) == "");
		CHECK(tab_bar->get_tab_tooltip(0) == "");
		CHECK(tab_bar->get_tab_text_direction(0) == Control::TEXT_DIRECTION_INHERITED);
		CHECK_FALSE(tab_bar->is_tab_disabled(0));
		CHECK_FALSE(tab_bar->is_tab_hidden(0));

		CHECK(tab_bar->get_tab_title(1) == "");
		CHECK(tab_bar->get_tab_tooltip(1) == "");
		CHECK(tab_bar->get_tab_text_direction(1) == Control::TEXT_DIRECTION_INHERITED);
		CHECK_FALSE(tab_bar->is_tab_disabled(1));
		CHECK_FALSE(tab_bar->is_tab_hidden(1));

		CHECK(tab_bar->get_tab_title(2) == "");
		CHECK(tab_bar->get_tab_tooltip(2) == "");
		CHECK(tab_bar->get_tab_text_direction(2) == Control::TEXT_DIRECTION_INHERITED);
		CHECK_FALSE(tab_bar->is_tab_disabled(2));
		CHECK_FALSE(tab_bar->is_tab_hidden(2));

		// Setting to less tabs than there are removes from the end.
		tab_bar->set_tab_title(0, "tab0");
		tab_bar->set_tab_title(1, "tab1");
		tab_bar->set_tab_title(2, "tab2");

		tab_bar->set_tab_count(2);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		CHECK(tab_bar->get_tab_title(0) == "tab0");
		CHECK(tab_bar->get_tab_title(1) == "tab1");

		// Remove all tabs.
		tab_bar->set_tab_count(0);
		CHECK(tab_bar->get_tab_count() == 0);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
	}

	SUBCASE("[TabBar] clear tabs") {
		CHECK(tab_bar->get_tab_count() == 0);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);

		tab_bar->set_tab_count(2);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		tab_bar->clear_tabs();
		CHECK(tab_bar->get_tab_count() == 0);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
	}

	SUBCASE("[TabBar] remove tabs") {
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		tab_bar->set_current_tab(1);
		CHECK(tab_bar->get_tab_count() == 3);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Remove first tab.
		tab_bar->remove_tab(0);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_tab_title(0) == "tab1");
		CHECK(tab_bar->get_tab_title(1) == "tab2");
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Remove last tab.
		tab_bar->remove_tab(1);
		CHECK(tab_bar->get_tab_count() == 1);
		CHECK(tab_bar->get_tab_title(0) == "tab1");
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Remove only tab.
		tab_bar->remove_tab(0);
		CHECK(tab_bar->get_tab_count() == 0);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK("tab_changed", build_array(build_array(-1)));

		// Remove current tab when there are other tabs.
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		tab_bar->set_current_tab(1);
		tab_bar->set_current_tab(2);
		CHECK(tab_bar->get_tab_count() == 3);
		CHECK(tab_bar->get_current_tab() == 2);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		tab_bar->remove_tab(2);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));
	}

	SUBCASE("[TabBar] move tabs") {
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		tab_bar->set_current_tab(1);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Don't move if index is the same.
		tab_bar->move_tab(0, 0);
		CHECK(tab_bar->get_tab_title(0) == "tab0");
		CHECK(tab_bar->get_tab_title(1) == "tab1");
		CHECK(tab_bar->get_tab_title(2) == "tab2");
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Move the first tab to the end.
		tab_bar->move_tab(0, 2);
		CHECK(tab_bar->get_tab_title(0) == "tab1");
		CHECK(tab_bar->get_tab_title(1) == "tab2");
		CHECK(tab_bar->get_tab_title(2) == "tab0");
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 2);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Move the second tab to the front.
		tab_bar->move_tab(1, 0);
		CHECK(tab_bar->get_tab_title(0) == "tab2");
		CHECK(tab_bar->get_tab_title(1) == "tab1");
		CHECK(tab_bar->get_tab_title(2) == "tab0");
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 2);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
	}

	SUBCASE("[TabBar] set current tab") {
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(0)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(0)));

		// Set the current tab.
		tab_bar->set_current_tab(1);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));

		// Set to same tab.
		tab_bar->set_current_tab(1);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK_FALSE("tab_changed");

		// Out of bounds.
		ERR_PRINT_OFF;
		tab_bar->set_current_tab(-5);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		tab_bar->set_current_tab(5);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		ERR_PRINT_ON;
	}

	SUBCASE("[TabBar] deselection enabled") {
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Setting deselect enabled doesn't change current tab.
		tab_bar->set_deselect_enabled(true);
		CHECK(tab_bar->get_deselect_enabled());
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Can deselect all tabs by setting current to -1.
		tab_bar->set_current_tab(-1);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(-1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(-1)));

		// Adding a tab will still set the current tab to 0.
		tab_bar->clear_tabs();
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);

		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		CHECK(tab_bar->get_tab_count() == 3);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(0)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(0)));

		tab_bar->set_current_tab(-1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Disabling while at -1 will select the first available tab.
		tab_bar->set_deselect_enabled(false);
		CHECK_FALSE(tab_bar->get_deselect_enabled());
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(0)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(0)));

		// Cannot set to -1 if disabled.
		ERR_PRINT_OFF;
		tab_bar->set_current_tab(-1);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		ERR_PRINT_ON;

		// Disabling while at -1 skips any disabled or hidden tabs.
		tab_bar->set_deselect_enabled(true);
		tab_bar->set_tab_disabled(0, true);
		tab_bar->set_tab_hidden(1, true);
		tab_bar->set_current_tab(-1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");
		tab_bar->set_deselect_enabled(false);
		CHECK(tab_bar->get_current_tab() == 2);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(2)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(2)));
	}

	SUBCASE("[TabBar] hidden tabs") {
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		tab_bar->set_current_tab(1);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		CHECK_FALSE(tab_bar->is_tab_hidden(1));
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		MessageQueue::get_singleton()->flush();
		Vector<Rect2> tab_rects = {
			tab_bar->get_tab_rect(0),
			tab_bar->get_tab_rect(1),
			tab_bar->get_tab_rect(2)
		};

		// Hiding a tab does not affect current tab.
		tab_bar->set_tab_hidden(1, true);
		CHECK(tab_bar->is_tab_hidden(1));
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// The tabs after are moved over.
		MessageQueue::get_singleton()->flush();
		CHECK(tab_bar->get_tab_rect(0) == tab_rects[0]);
		CHECK(tab_bar->get_tab_rect(2) == tab_rects[1]);

		// Unhiding a tab does not affect current tab.
		tab_bar->set_tab_hidden(1, false);
		CHECK_FALSE(tab_bar->is_tab_hidden(1));
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// The tabs are back where they were.
		MessageQueue::get_singleton()->flush();
		CHECK(tab_bar->get_tab_rect(0) == tab_rects[0]);
		CHECK(tab_bar->get_tab_rect(1) == tab_rects[1]);
		CHECK(tab_bar->get_tab_rect(2) == tab_rects[2]);
	}

	SUBCASE("[TabBar] disabled tabs") {
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		CHECK_FALSE(tab_bar->is_tab_disabled(1));
		tab_bar->set_current_tab(1);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		CHECK_FALSE(tab_bar->is_tab_hidden(1));
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Disabling a tab does not affect current tab.
		tab_bar->set_tab_disabled(1, true);
		CHECK(tab_bar->is_tab_disabled(1));
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Enabling a tab does not affect current tab.
		tab_bar->set_tab_disabled(1, false);
		CHECK_FALSE(tab_bar->is_tab_disabled(1));
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
	}

	SUBCASE("[TabBar] select next available") {
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		tab_bar->add_tab("tab3");
		tab_bar->add_tab("tab4");
		tab_bar->set_tab_disabled(2, true);
		tab_bar->set_tab_hidden(3, true);
		tab_bar->set_current_tab(0);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Selects the next tab.
		CHECK(tab_bar->select_next_available());
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));

		// Skips over disabled and hidden tabs.
		CHECK(tab_bar->select_next_available());
		CHECK(tab_bar->get_current_tab() == 4);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(4)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(4)));

		// Does not wrap around.
		CHECK_FALSE(tab_bar->select_next_available());
		CHECK(tab_bar->get_current_tab() == 4);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Fails if there is only one valid tab.
		tab_bar->remove_tab(0);
		tab_bar->remove_tab(3);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		CHECK_FALSE(tab_bar->select_next_available());
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Fails if there are no valid tabs.
		tab_bar->remove_tab(0);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		CHECK_FALSE(tab_bar->select_next_available());
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Fails if there are no tabs.
		tab_bar->clear_tabs();
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		CHECK_FALSE(tab_bar->select_next_available());
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
	}

	SUBCASE("[TabBar] select previous available") {
		tab_bar->add_tab("tab0");
		tab_bar->add_tab("tab1");
		tab_bar->add_tab("tab2");
		tab_bar->add_tab("tab3");
		tab_bar->add_tab("tab4");
		tab_bar->set_tab_disabled(1, true);
		tab_bar->set_tab_hidden(2, true);
		tab_bar->set_current_tab(4);
		CHECK(tab_bar->get_current_tab() == 4);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Selects the previous tab.
		CHECK(tab_bar->select_previous_available());
		CHECK(tab_bar->get_current_tab() == 3);
		CHECK(tab_bar->get_previous_tab() == 4);
		SIGNAL_CHECK("tab_selected", build_array(build_array(3)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(3)));

		// Skips over disabled and hidden tabs.
		CHECK(tab_bar->select_previous_available());
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 3);
		SIGNAL_CHECK("tab_selected", build_array(build_array(0)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(0)));

		// Does not wrap around.
		CHECK_FALSE(tab_bar->select_previous_available());
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 3);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Fails if there is only one valid tab.
		tab_bar->remove_tab(4);
		tab_bar->remove_tab(3);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 2);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		CHECK_FALSE(tab_bar->select_previous_available());
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == 2);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Fails if there are no valid tabs.
		tab_bar->remove_tab(0);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		CHECK_FALSE(tab_bar->select_previous_available());
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == 1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Fails if there are no tabs.
		tab_bar->clear_tabs();
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		CHECK_FALSE(tab_bar->select_previous_available());
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
	}

	SIGNAL_UNWATCH(tab_bar, "tab_selected");
	SIGNAL_UNWATCH(tab_bar, "tab_changed");

	memdelete(tab_bar);
}

TEST_CASE("[SceneTree][TabBar] initialization") {
	TabBar *tab_bar = memnew(TabBar);

	SIGNAL_WATCH(tab_bar, "tab_selected");
	SIGNAL_WATCH(tab_bar, "tab_changed");

	SUBCASE("[TabBar] current tab can be set before tabs are set") {
		// This queues the current tab to update on when tabs are set.
		tab_bar->set_current_tab(1);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		tab_bar->set_tab_count(2);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));

		// Does not work again.
		ERR_PRINT_OFF;
		tab_bar->set_current_tab(2);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		tab_bar->set_tab_count(3);
		CHECK(tab_bar->get_tab_count() == 3);
		CHECK(tab_bar->get_current_tab() == 1);
		CHECK(tab_bar->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		ERR_PRINT_ON;
	}

	SUBCASE("[TabBar] setting tabs works normally if no current tab was set") {
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);

		tab_bar->set_tab_count(2);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
	}

	SUBCASE("[TabBar] cannot set current tab to an invalid value before tabs are set") {
		tab_bar->set_current_tab(100);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// This will print an error message as if `set_current_tab` was called after.
		ERR_PRINT_OFF;
		tab_bar->set_tab_count(2);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		ERR_PRINT_ON;
	}

	SUBCASE("[TabBar] setting the current tab before tabs only works when out of tree") {
		tab_bar->set_current_tab(1);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		SceneTree::get_singleton()->get_root()->add_child(tab_bar);
		MessageQueue::get_singleton()->flush();
		CHECK(tab_bar->get_tab_count() == 0);
		CHECK(tab_bar->get_current_tab() == -1);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Works normally.
		tab_bar->set_tab_count(2);
		CHECK(tab_bar->get_tab_count() == 2);
		CHECK(tab_bar->get_current_tab() == 0);
		CHECK(tab_bar->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
	}

	SIGNAL_UNWATCH(tab_bar, "tab_selected");
	SIGNAL_UNWATCH(tab_bar, "tab_changed");

	memdelete(tab_bar);
}

TEST_CASE("[SceneTree][TabBar] layout and offset") {
	TabBar *tab_bar = memnew(TabBar);
	SceneTree::get_singleton()->get_root()->add_child(tab_bar);

	tab_bar->set_clip_tabs(false);
	tab_bar->add_tab("tab0");
	tab_bar->add_tab("tab1 ");
	tab_bar->add_tab("tab2    ");
	MessageQueue::get_singleton()->flush();
	Size2 all_tabs_size = tab_bar->get_size();

	Vector<Rect2> tab_rects = {
		tab_bar->get_tab_rect(0),
		tab_bar->get_tab_rect(1),
		tab_bar->get_tab_rect(2)
	};

	SUBCASE("[TabBar] tabs are arranged next to each other") {
		// Horizontal positions are next to each other.
		CHECK(tab_rects[0].position.x == 0);
		CHECK(tab_rects[1].position.x == tab_rects[0].size.x);
		CHECK(tab_rects[2].position.x == tab_rects[1].position.x + tab_rects[1].size.x);

		// Fills the entire width.
		CHECK(tab_rects[2].position.x + tab_rects[2].size.x == all_tabs_size.x);

		// Horizontal sizes are positive.
		CHECK(tab_rects[0].size.x > 0);
		CHECK(tab_rects[1].size.x > 0);
		CHECK(tab_rects[2].size.x > 0);

		// Vertical positions are at 0.
		CHECK(tab_rects[0].position.y == 0);
		CHECK(tab_rects[1].position.y == 0);
		CHECK(tab_rects[2].position.y == 0);

		// Vertical sizes are the same.
		CHECK(tab_rects[0].size.y == tab_rects[1].size.y);
		CHECK(tab_rects[1].size.y == tab_rects[2].size.y);
	}

	SUBCASE("[TabBar] tab alignment") {
		// Add extra space so the alignment can be seen.
		tab_bar->set_size(Size2(all_tabs_size.x + 100, all_tabs_size.y));

		// Left alignment.
		tab_bar->set_tab_alignment(TabBar::ALIGNMENT_LEFT);
		MessageQueue::get_singleton()->flush();
		tab_rects = {
			tab_bar->get_tab_rect(0),
			tab_bar->get_tab_rect(1),
			tab_bar->get_tab_rect(2)
		};
		CHECK(tab_bar->get_tab_alignment() == TabBar::ALIGNMENT_LEFT);
		CHECK(tab_rects[0].position.x == 0);
		CHECK(tab_rects[1].position.x == tab_rects[0].size.x);
		CHECK(tab_rects[2].position.x == tab_rects[1].position.x + tab_rects[1].size.x);

		// Right alignment.
		tab_bar->set_tab_alignment(TabBar::ALIGNMENT_RIGHT);
		MessageQueue::get_singleton()->flush();
		tab_rects = {
			tab_bar->get_tab_rect(0),
			tab_bar->get_tab_rect(1),
			tab_bar->get_tab_rect(2)
		};
		CHECK(tab_bar->get_tab_alignment() == TabBar::ALIGNMENT_RIGHT);
		CHECK(tab_rects[2].position.x == tab_bar->get_size().x - tab_rects[2].size.x);
		CHECK(tab_rects[1].position.x == tab_rects[2].position.x - tab_rects[1].size.x);
		CHECK(tab_rects[0].position.x == tab_rects[1].position.x - tab_rects[0].size.x);

		// Center alignment.
		tab_bar->set_tab_alignment(TabBar::ALIGNMENT_CENTER);
		MessageQueue::get_singleton()->flush();
		tab_rects = {
			tab_bar->get_tab_rect(0),
			tab_bar->get_tab_rect(1),
			tab_bar->get_tab_rect(2)
		};
		CHECK(tab_bar->get_tab_alignment() == TabBar::ALIGNMENT_CENTER);
		float center_pos = tab_bar->get_size().x / 2;
		CHECK(tab_rects[0].position.x == center_pos - all_tabs_size.x / 2);
		CHECK(tab_rects[1].position.x == tab_rects[0].position.x + tab_rects[0].size.x);
		CHECK(tab_rects[2].position.x == tab_rects[1].position.x + tab_rects[1].size.x);
	}

	SUBCASE("[TabBar] clip tabs") {
		// Clip tabs disabled means all tabs are visible and the minimum size holds all of them.
		tab_bar->set_clip_tabs(false);
		CHECK_FALSE(tab_bar->get_clip_tabs());
		MessageQueue::get_singleton()->flush();
		CHECK(tab_bar->get_tab_offset() == 0);
		CHECK(tab_bar->get_minimum_size() == tab_bar->get_size());
		CHECK(tab_bar->get_size().x == tab_rects[0].size.x + tab_rects[1].size.x + tab_rects[2].size.x);
		CHECK(tab_bar->get_size().y == MAX(tab_rects[0].size.y, MAX(tab_rects[1].size.y, tab_rects[2].size.y)));

		tab_bar->set_clip_tabs(true);
		CHECK(tab_bar->get_clip_tabs());
		MessageQueue::get_singleton()->flush();
		CHECK(tab_bar->get_tab_offset() == 0);

		// Horizontal size and minimum size get set to 0.
		CHECK(tab_bar->get_minimum_size().x == 0);
		CHECK(tab_bar->get_minimum_size().y == all_tabs_size.y);
		CHECK(tab_bar->get_size().x == 0);
		CHECK(tab_bar->get_size().y == all_tabs_size.y);
	}

	SUBCASE("[TabBar] ensure tab visible") {
		tab_bar->set_scroll_to_selected(false);
		tab_bar->set_clip_tabs(true);

		// Resize tab bar to only be able to fit 2 tabs.
		const float offset_button_size = tab_bar->get_theme_icon("decrement_icon")->get_width() + tab_bar->get_theme_icon("increment_icon")->get_width();
		tab_bar->set_size(Size2(tab_rects[2].size.x + tab_rects[1].size.x + offset_button_size, all_tabs_size.y));
		MessageQueue::get_singleton()->flush();
		CHECK(tab_bar->get_tab_offset() == 0);
		CHECK(tab_bar->get_offset_buttons_visible());

		// Scroll right to a tab that is not visible.
		tab_bar->ensure_tab_visible(2);
		CHECK(tab_bar->get_tab_offset() == 1);
		CHECK(tab_bar->get_tab_rect(1).position.x == 0);
		CHECK(tab_bar->get_tab_rect(2).position.x == tab_rects[1].size.x);

		tab_bar->set_tab_offset(2);
		CHECK(tab_bar->get_tab_offset() == 2);
		CHECK(tab_bar->get_tab_rect(2).position.x == 0);

		// Scroll left to a previous tab.
		tab_bar->ensure_tab_visible(1);
		CHECK(tab_bar->get_tab_offset() == 1);
		CHECK(tab_bar->get_tab_rect(1).position.x == 0);
		CHECK(tab_bar->get_tab_rect(2).position.x == tab_rects[1].size.x);

		// Will not scroll if the tab is already visible.
		tab_bar->ensure_tab_visible(2);
		CHECK(tab_bar->get_tab_offset() == 1);
		CHECK(tab_bar->get_tab_rect(1).position.x == 0);
		CHECK(tab_bar->get_tab_rect(2).position.x == tab_rects[1].size.x);
	}

	memdelete(tab_bar);
}

// FIXME: Add tests for mouse click, keyboard navigation, and drag and drop.

} // namespace TestTabBar
