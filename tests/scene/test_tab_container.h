/**************************************************************************/
/*  test_tab_container.h                                                  */
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

#ifndef TEST_TAB_CONTAINER_H
#define TEST_TAB_CONTAINER_H

#include "scene/gui/tab_container.h"

#include "tests/test_macros.h"

namespace TestTabContainer {
static inline Array build_array() {
	return Array();
}
template <typename... Targs>
static inline Array build_array(Variant item, Targs... Fargs) {
	Array a = build_array(Fargs...);
	a.push_front(item);
	return a;
}

TEST_CASE("[SceneTree][TabContainer] tab operations") {
	TabContainer *tab_container = memnew(TabContainer);
	SceneTree::get_singleton()->get_root()->add_child(tab_container);
	MessageQueue::get_singleton()->flush();
	SIGNAL_WATCH(tab_container, "tab_selected");
	SIGNAL_WATCH(tab_container, "tab_changed");

	Control *tab0 = memnew(Control);
	tab0->set_name("tab0");
	Control *tab1 = memnew(Control);
	tab1->set_name("tab1");
	Control *tab2 = memnew(Control);
	tab2->set_name("tab2");

	SUBCASE("[TabContainer] add tabs by adding children") {
		CHECK(tab_container->get_tab_count() == 0);
		CHECK(tab_container->get_current_tab() == -1);
		CHECK(tab_container->get_previous_tab() == -1);

		// Add first tab child.
		tab_container->add_child(tab0);
		// MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_tab_count() == 1);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(0)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(0)));

		// Add second tab child.
		tab_container->add_child(tab1);
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Check default values, the title is the name of the child.
		CHECK(tab_container->get_tab_control(0) == tab0);
		CHECK(tab_container->get_tab_idx_from_control(tab0) == 0);
		CHECK(tab_container->get_tab_title(0) == "tab0");
		CHECK(tab_container->get_tab_tooltip(0) == "");
		CHECK_FALSE(tab_container->is_tab_disabled(0));
		CHECK_FALSE(tab_container->is_tab_hidden(0));

		CHECK(tab_container->get_tab_control(1) == tab1);
		CHECK(tab_container->get_tab_idx_from_control(tab1) == 1);
		CHECK(tab_container->get_tab_title(1) == "tab1");
		CHECK(tab_container->get_tab_tooltip(1) == "");
		CHECK_FALSE(tab_container->is_tab_disabled(1));
		CHECK_FALSE(tab_container->is_tab_hidden(1));
	}

	SUBCASE("[TabContainer] remove tabs by removing children") {
		tab_container->add_child(tab0);
		tab_container->add_child(tab1);
		tab_container->add_child(tab2);
		tab_container->set_current_tab(1);
		CHECK(tab_container->get_tab_count() == 3);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Remove first tab.
		tab_container->remove_child(tab0);
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_tab_title(0) == "tab1");
		CHECK(tab_container->get_tab_title(1) == "tab2");
		CHECK(tab_container->get_tab_idx_from_control(tab1) == 0);
		CHECK(tab_container->get_tab_idx_from_control(tab2) == 1);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Remove last tab.
		tab_container->remove_child(tab2);
		CHECK(tab_container->get_tab_count() == 1);
		CHECK(tab_container->get_tab_title(0) == "tab1");
		CHECK(tab_container->get_tab_idx_from_control(tab1) == 0);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Remove only tab.
		tab_container->remove_child(tab1);
		CHECK(tab_container->get_tab_count() == 0);
		CHECK(tab_container->get_current_tab() == -1);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK("tab_changed", build_array(build_array(-1)));

		// Remove current tab when there are other tabs.
		tab_container->add_child(tab0);
		tab_container->add_child(tab1);
		tab_container->add_child(tab2);
		tab_container->set_current_tab(1);
		tab_container->set_current_tab(2);
		CHECK(tab_container->get_tab_count() == 3);
		CHECK(tab_container->get_current_tab() == 2);
		CHECK(tab_container->get_previous_tab() == 1);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		tab_container->remove_child(tab2);
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));
	}

	SUBCASE("[TabContainer] move tabs by moving children") {
		tab_container->add_child(tab0);
		tab_container->add_child(tab1);
		tab_container->add_child(tab2);
		tab_container->set_current_tab(1);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");

		// Move the first tab to the end.
		tab_container->move_child(tab0, 2);
		CHECK(tab_container->get_tab_idx_from_control(tab0) == 2);
		CHECK(tab_container->get_tab_idx_from_control(tab1) == 0);
		CHECK(tab_container->get_tab_idx_from_control(tab2) == 1);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == 2);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Move the second tab to the front.
		tab_container->move_child(tab2, 0);
		CHECK(tab_container->get_tab_idx_from_control(tab0) == 2);
		CHECK(tab_container->get_tab_idx_from_control(tab1) == 1);
		CHECK(tab_container->get_tab_idx_from_control(tab2) == 0);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 2);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
	}

	SUBCASE("[TabContainer] set current tab") {
		tab_container->add_child(tab0);
		tab_container->add_child(tab1);
		tab_container->add_child(tab2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(0)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(0)));
		MessageQueue::get_singleton()->flush();
		CHECK(tab0->is_visible());
		CHECK_FALSE(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());

		// Set the current tab.
		tab_container->set_current_tab(1);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());

		// Set to same tab.
		tab_container->set_current_tab(1);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK_FALSE("tab_changed");
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());

		// Out of bounds.
		ERR_PRINT_OFF;
		tab_container->set_current_tab(-5);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());

		tab_container->set_current_tab(5);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());
		ERR_PRINT_ON;
	}

	SUBCASE("[TabContainer] change current tab by changing visibility of children") {
		tab_container->add_child(tab0);
		tab_container->add_child(tab1);
		tab_container->add_child(tab2);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");
		MessageQueue::get_singleton()->flush();
		CHECK(tab0->is_visible());
		CHECK_FALSE(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());

		// Show a child to make it the current tab.
		tab1->show();
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());

		// Hide the visible child to select the next tab.
		tab1->hide();
		CHECK(tab_container->get_current_tab() == 2);
		CHECK(tab_container->get_previous_tab() == 1);
		SIGNAL_CHECK("tab_selected", build_array(build_array(2)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(2)));
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(tab0->is_visible());
		CHECK_FALSE(tab1->is_visible());
		CHECK(tab2->is_visible());

		// Hide the visible child to select the previous tab if there is no next.
		tab2->hide();
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 2);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());

		// Cannot hide if there is only one valid child since deselection is not enabled.
		tab_container->remove_child(tab1);
		tab_container->remove_child(tab2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_DISCARD("tab_selected");
		SIGNAL_DISCARD("tab_changed");
		MessageQueue::get_singleton()->flush();
		CHECK(tab0->is_visible());

		tab0->hide();
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		MessageQueue::get_singleton()->flush();
		CHECK(tab0->is_visible());

		// Can hide the last tab if deselection is enabled.
		tab_container->set_deselect_enabled(true);
		tab0->hide();
		CHECK(tab_container->get_current_tab() == -1);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(-1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(-1)));
		MessageQueue::get_singleton()->flush();
		CHECK_FALSE(tab0->is_visible());
	}

	SIGNAL_UNWATCH(tab_container, "tab_selected");
	SIGNAL_UNWATCH(tab_container, "tab_changed");

	memdelete(tab2);
	memdelete(tab1);
	memdelete(tab0);
	memdelete(tab_container);
}

TEST_CASE("[SceneTree][TabContainer] initialization") {
	TabContainer *tab_container = memnew(TabContainer);

	Control *tab0 = memnew(Control);
	tab0->set_name("tab0");
	Control *tab1 = memnew(Control);
	tab1->set_name("tab1 ");
	Control *tab2 = memnew(Control);
	tab2->set_name("tab2    ");

	SIGNAL_WATCH(tab_container, "tab_selected");
	SIGNAL_WATCH(tab_container, "tab_changed");

	SUBCASE("[TabContainer] add children before entering tree") {
		CHECK(tab_container->get_current_tab() == -1);
		CHECK(tab_container->get_previous_tab() == -1);

		tab_container->add_child(tab0);
		CHECK(tab_container->get_tab_count() == 1);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);

		tab_container->add_child(tab1);
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);

		SceneTree::get_singleton()->get_root()->add_child(tab_container);
		MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		CHECK(tab0->is_visible());
		CHECK_FALSE(tab1->is_visible());
	}

	SUBCASE("[TabContainer] current tab can be set before children are added") {
		// Set the current tab before there are any tabs.
		// This queues the current tab to update on entering the tree.
		tab_container->set_current_tab(1);
		CHECK(tab_container->get_current_tab() == -1);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		tab_container->add_child(tab0);
		CHECK(tab_container->get_tab_count() == 1);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);

		tab_container->add_child(tab1);
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);

		tab_container->add_child(tab2);
		CHECK(tab_container->get_tab_count() == 3);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// Current tab is set when entering the tree.
		SceneTree::get_singleton()->get_root()->add_child(tab_container);
		MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_tab_count() == 3);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());
	}

	SUBCASE("[TabContainer] cannot set current tab to an invalid value before tabs are set") {
		tab_container->set_current_tab(100);
		CHECK(tab_container->get_current_tab() == -1);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		tab_container->add_child(tab0);
		CHECK(tab_container->get_tab_count() == 1);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		tab_container->add_child(tab1);
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");

		// This will print an error message as if `set_current_tab` was called after.
		ERR_PRINT_OFF;
		SceneTree::get_singleton()->get_root()->add_child(tab_container);
		MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		ERR_PRINT_ON;
	}

	SUBCASE("[TabContainer] children visibility before entering tree") {
		CHECK(tab_container->get_current_tab() == -1);
		CHECK(tab_container->get_previous_tab() == -1);

		// Adding a hidden child first will change visibility because it is the current tab.
		tab0->hide();
		tab_container->add_child(tab0);
		CHECK(tab_container->get_tab_count() == 1);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		MessageQueue::get_singleton()->flush();
		CHECK(tab0->is_visible());

		// Adding a visible child after will hide it because it is not the current tab.
		tab_container->add_child(tab1);
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		MessageQueue::get_singleton()->flush();
		CHECK(tab0->is_visible());
		CHECK_FALSE(tab1->is_visible());

		// Can change current by showing child now after children have been added.
		// This queues the current tab to update on entering the tree.
		tab1->show();
		MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		CHECK(tab0->is_visible());
		CHECK(tab1->is_visible());

		SceneTree::get_singleton()->get_root()->add_child(tab_container);
		MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_tab_count() == 2);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
	}

	SUBCASE("[TabContainer] setting current tab and changing child visibility after adding") {
		tab_container->add_child(tab0);
		tab_container->add_child(tab1);
		tab_container->add_child(tab2);
		MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);

		tab2->show();
		MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_tab_count() == 3);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);
		SIGNAL_CHECK_FALSE("tab_selected");
		SIGNAL_CHECK_FALSE("tab_changed");
		CHECK(tab0->is_visible());
		CHECK_FALSE(tab1->is_visible());
		CHECK(tab2->is_visible());

		// Whichever happens last will have priority.
		tab_container->set_current_tab(1);
		CHECK(tab_container->get_current_tab() == 0);
		CHECK(tab_container->get_previous_tab() == -1);

		// Current tab is set when entering the tree.
		SceneTree::get_singleton()->get_root()->add_child(tab_container);
		MessageQueue::get_singleton()->flush();
		CHECK(tab_container->get_tab_count() == 3);
		CHECK(tab_container->get_current_tab() == 1);
		CHECK(tab_container->get_previous_tab() == 0);
		SIGNAL_CHECK("tab_selected", build_array(build_array(1)));
		SIGNAL_CHECK("tab_changed", build_array(build_array(1)));
		CHECK_FALSE(tab0->is_visible());
		CHECK(tab1->is_visible());
		CHECK_FALSE(tab2->is_visible());
	}

	SIGNAL_UNWATCH(tab_container, "tab_selected");
	SIGNAL_UNWATCH(tab_container, "tab_changed");

	memdelete(tab2);
	memdelete(tab1);
	memdelete(tab0);
	memdelete(tab_container);
}

TEST_CASE("[SceneTree][TabContainer] layout and offset") {
	TabContainer *tab_container = memnew(TabContainer);
	SceneTree::get_singleton()->get_root()->add_child(tab_container);
	tab_container->set_clip_tabs(false);

	Control *tab0 = memnew(Control);
	tab0->set_name("tab0");
	Control *tab1 = memnew(Control);
	tab1->set_name("tab1 ");
	Control *tab2 = memnew(Control);
	tab2->set_name("tab2    ");

	tab_container->add_child(tab0);
	tab_container->add_child(tab1);
	tab_container->add_child(tab2);

	MessageQueue::get_singleton()->flush();

	Size2 all_tabs_size = tab_container->get_size();
	const float side_margin = tab_container->get_theme_constant("side_margin");

	TabBar *tab_bar = tab_container->get_tab_bar();

	Vector<Rect2> tab_rects = {
		tab_bar->get_tab_rect(0),
		tab_bar->get_tab_rect(1),
		tab_bar->get_tab_rect(2)
	};

	SUBCASE("[TabContainer] tabs are arranged next to each other") {
		// Horizontal positions are next to each other.
		CHECK(tab_rects[0].position.x == 0);
		CHECK(tab_rects[1].position.x == tab_rects[0].size.x);
		CHECK(tab_rects[2].position.x == tab_rects[1].position.x + tab_rects[1].size.x);

		// Fills the entire width.
		CHECK(tab_rects[2].position.x + tab_rects[2].size.x == all_tabs_size.x - side_margin);

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

	SUBCASE("[TabContainer] tab position") {
		float tab_height = tab_rects[0].size.y;
		Ref<StyleBox> panel_style = tab_container->get_theme_stylebox("panel_style");

		// Initial position, same as top position.
		// Tab bar is at the top.
		CHECK(tab_bar->get_anchor(SIDE_TOP) == 0);
		CHECK(tab_bar->get_anchor(SIDE_BOTTOM) == 0);
		CHECK(tab_bar->get_anchor(SIDE_LEFT) == 0);
		CHECK(tab_bar->get_anchor(SIDE_RIGHT) == 1);
		CHECK(tab_bar->get_offset(SIDE_TOP) == 0);
		CHECK(tab_bar->get_offset(SIDE_BOTTOM) == tab_height);
		CHECK(tab_bar->get_offset(SIDE_LEFT) == side_margin);
		CHECK(tab_bar->get_offset(SIDE_RIGHT) == 0);

		// Child is expanded and below the tab bar.
		CHECK(tab0->get_anchor(SIDE_TOP) == 0);
		CHECK(tab0->get_anchor(SIDE_BOTTOM) == 1);
		CHECK(tab0->get_anchor(SIDE_LEFT) == 0);
		CHECK(tab0->get_anchor(SIDE_RIGHT) == 1);
		CHECK(tab0->get_offset(SIDE_TOP) == tab_height);
		CHECK(tab0->get_offset(SIDE_BOTTOM) == 0);
		CHECK(tab0->get_offset(SIDE_LEFT) == 0);
		CHECK(tab0->get_offset(SIDE_RIGHT) == 0);

		// Bottom position.
		tab_container->set_tabs_position(TabContainer::POSITION_BOTTOM);
		CHECK(tab_container->get_tabs_position() == TabContainer::POSITION_BOTTOM);
		MessageQueue::get_singleton()->flush();

		// Tab bar is at the bottom.
		CHECK(tab_bar->get_anchor(SIDE_TOP) == 1);
		CHECK(tab_bar->get_anchor(SIDE_BOTTOM) == 1);
		CHECK(tab_bar->get_anchor(SIDE_LEFT) == 0);
		CHECK(tab_bar->get_anchor(SIDE_RIGHT) == 1);
		CHECK(tab_bar->get_offset(SIDE_TOP) == -tab_height);
		CHECK(tab_bar->get_offset(SIDE_BOTTOM) == 0);
		CHECK(tab_bar->get_offset(SIDE_LEFT) == side_margin);
		CHECK(tab_bar->get_offset(SIDE_RIGHT) == 0);

		// Child is expanded and above the tab bar.
		CHECK(tab0->get_anchor(SIDE_TOP) == 0);
		CHECK(tab0->get_anchor(SIDE_BOTTOM) == 1);
		CHECK(tab0->get_anchor(SIDE_LEFT) == 0);
		CHECK(tab0->get_anchor(SIDE_RIGHT) == 1);
		CHECK(tab0->get_offset(SIDE_TOP) == 0);
		CHECK(tab0->get_offset(SIDE_BOTTOM) == -tab_height);
		CHECK(tab0->get_offset(SIDE_LEFT) == 0);
		CHECK(tab0->get_offset(SIDE_RIGHT) == 0);

		// Top position.
		tab_container->set_tabs_position(TabContainer::POSITION_TOP);
		CHECK(tab_container->get_tabs_position() == TabContainer::POSITION_TOP);
		MessageQueue::get_singleton()->flush();

		// Tab bar is at the top.
		CHECK(tab_bar->get_anchor(SIDE_TOP) == 0);
		CHECK(tab_bar->get_anchor(SIDE_BOTTOM) == 0);
		CHECK(tab_bar->get_anchor(SIDE_LEFT) == 0);
		CHECK(tab_bar->get_anchor(SIDE_RIGHT) == 1);
		CHECK(tab_bar->get_offset(SIDE_TOP) == 0);
		CHECK(tab_bar->get_offset(SIDE_BOTTOM) == tab_height);
		CHECK(tab_bar->get_offset(SIDE_LEFT) == side_margin);
		CHECK(tab_bar->get_offset(SIDE_RIGHT) == 0);

		// Child is expanded and below the tab bar.
		CHECK(tab0->get_anchor(SIDE_TOP) == 0);
		CHECK(tab0->get_anchor(SIDE_BOTTOM) == 1);
		CHECK(tab0->get_anchor(SIDE_LEFT) == 0);
		CHECK(tab0->get_anchor(SIDE_RIGHT) == 1);
		CHECK(tab0->get_offset(SIDE_TOP) == tab_height);
		CHECK(tab0->get_offset(SIDE_BOTTOM) == 0);
		CHECK(tab0->get_offset(SIDE_LEFT) == 0);
		CHECK(tab0->get_offset(SIDE_RIGHT) == 0);
	}

	memdelete(tab_container);
}

// FIXME: Add tests for mouse click, keyboard navigation, and drag and drop.

} // namespace TestTabContainer

#endif // TEST_TAB_CONTAINER_H
