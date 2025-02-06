/**************************************************************************/
/*  shared_controls.cpp                                                   */
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

#include "shared_controls.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/resources/style_box_flat.h"

SpanningHeader::SpanningHeader(const String &p_text) {
	Ref<StyleBoxFlat> title_sbf;
	title_sbf.instantiate();
	title_sbf->set_bg_color(EditorNode::get_singleton()->get_editor_theme()->get_color("dark_color_3", "Editor"));
	add_theme_style_override(SceneStringName(panel), title_sbf);
	set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	Label *title = memnew(Label(p_text));
	add_child(title);
	title->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	title->set_vertical_alignment(VerticalAlignment::VERTICAL_ALIGNMENT_CENTER);
}

DarkPanelContainer::DarkPanelContainer() {
	set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	Ref<StyleBoxFlat> content_wrapper_sbf;
	content_wrapper_sbf.instantiate();
	content_wrapper_sbf->set_bg_color(EditorNode::get_singleton()->get_editor_theme()->get_color("dark_color_2", "Editor"));
	add_theme_style_override("panel", content_wrapper_sbf);
}

void TreeSortAndFilterBar::_apply_filter(TreeItem *p_current_node) {
	if (!p_current_node) {
		p_current_node = managed_tree->get_root();
	}

	if (!p_current_node) {
		return;
	}

	// Reset ourself to default state.
	p_current_node->set_visible(true);
	p_current_node->clear_custom_color(0);

	// Go through each child and filter them.
	bool any_child_visible = false;
	for (TreeItem *child = p_current_node->get_first_child(); child; child = child->get_next()) {
		_apply_filter(child);
		if (child->is_visible()) {
			any_child_visible = true;
		}
	}

	// Check if we match the filter.
	String filter_str = filter_edit->get_text().strip_edges(true, true).to_lower();

	// We are visible.
	bool matches_filter = false;
	for (int i = 0; i < managed_tree->get_columns(); i++) {
		if (p_current_node->get_text(i).to_lower().contains(filter_str)) {
			matches_filter = true;
			break;
		}
	}
	if (matches_filter || filter_str.is_empty()) {
		p_current_node->set_visible(true);
	} else if (any_child_visible) {
		// We have a visible child.
		p_current_node->set_custom_color(0, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	} else {
		// We and out children aren't visible.
		p_current_node->set_visible(false);
	}
}

void TreeSortAndFilterBar::_apply_sort() {
	if (!sort_button->is_visible()) {
		return;
	}
	for (int i = 0; i != sort_button->get_popup()->get_item_count(); i++) {
		// Update the popup buttons to be checked/unchecked.
		sort_button->get_popup()->set_item_checked(i, (i == (int)current_sort));
	}

	SortItem sort = sort_items[current_sort];

	List<TreeItem *> items_to_sort;
	items_to_sort.push_back(managed_tree->get_root());

	while (items_to_sort.size() > 0) {
		TreeItem *to_sort = items_to_sort.front()->get();
		items_to_sort.pop_front();

		List<TreeItemColumn> items;
		for (int i = 0; i < to_sort->get_child_count(); i++) {
			items.push_back(TreeItemColumn(to_sort->get_child(i), sort.column));
		}

		if (sort.type == ALPHA_SORT && sort.ascending == true) {
			items.sort_custom<TreeItemAlphaComparator>();
		}
		if (sort.type == ALPHA_SORT && sort.ascending == false) {
			items.sort_custom<TreeItemAlphaComparator>();
			items.reverse();
		}
		if (sort.type == NUMERIC_SORT && sort.ascending == true) {
			items.sort_custom<TreeItemNumericComparator>();
		}
		if (sort.type == NUMERIC_SORT && sort.ascending == false) {
			items.sort_custom<TreeItemNumericComparator>();
			items.reverse();
		}

		int idx = 0;
		for (const TreeItemColumn &item : items) {
			item.item->move_before(to_sort->get_child(idx));
			items_to_sort.push_back(item.item);
			idx++;
		}
	}
}

void TreeSortAndFilterBar::_sort_changed(int p_id) {
	current_sort = p_id;
	_apply_sort();
}

void TreeSortAndFilterBar::_filter_changed(const String &p_filter) {
	_apply_filter();
}

TreeSortAndFilterBar::TreeSortAndFilterBar(Tree *p_managed_tree, const String &p_filter_placeholder_text) :
		managed_tree(p_managed_tree) {
	set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	add_theme_constant_override("h_separation", 10 * EDSCALE);
	filter_edit = memnew(LineEdit);
	filter_edit->set_clear_button_enabled(true);
	filter_edit->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	filter_edit->set_placeholder(p_filter_placeholder_text);
	add_child(filter_edit);
	filter_edit->connect(SceneStringName(text_changed), callable_mp(this, &TreeSortAndFilterBar::_filter_changed));

	sort_button = memnew(MenuButton);
	sort_button->set_visible(false);
	sort_button->set_flat(false);
	sort_button->set_theme_type_variation("FlatMenuButton");
	PopupMenu *p = sort_button->get_popup();
	p->connect(SceneStringName(id_pressed), callable_mp(this, &TreeSortAndFilterBar::_sort_changed));

	add_child(sort_button);
}

void TreeSortAndFilterBar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE:
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			filter_edit->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			sort_button->set_button_icon(get_editor_theme_icon(SNAME("Sort")));

			apply();

		} break;
	}
}

TreeSortAndFilterBar::SortOptionIndexes TreeSortAndFilterBar::add_sort_option(const String &p_new_option, SortType p_sort_type, int p_sort_column, bool p_is_default) {
	sort_button->set_visible(true);
	bool is_first_item = sort_items.is_empty();
	SortItem item_ascending(sort_items.size(), TTR("Sort By ") + p_new_option + TTR(" (Ascending)"), p_sort_type, true, p_sort_column);
	sort_items[item_ascending.id] = item_ascending;
	sort_button->get_popup()->add_radio_check_item(item_ascending.label, item_ascending.id);

	SortItem item_descending(sort_items.size(), TTR("Sort By ") + p_new_option + TTR(" (Descending)"), p_sort_type, false, p_sort_column);
	sort_items[item_descending.id] = item_descending;
	sort_button->get_popup()->add_radio_check_item(item_descending.label, item_descending.id);

	if (is_first_item) {
		sort_button->get_popup()->set_item_checked(0, true);
	}

	SortOptionIndexes indexes;
	indexes.ascending = item_ascending.id;
	indexes.descending = item_descending.id;
	return indexes;
}

void TreeSortAndFilterBar::clear_filter() {
	filter_edit->clear();
}

void TreeSortAndFilterBar::clear() {
	sort_button->set_visible(false);
	sort_button->get_popup()->clear();
	filter_edit->clear();
}

void TreeSortAndFilterBar::select_sort(int p_item_id) {
	_sort_changed(p_item_id);
}

void TreeSortAndFilterBar::apply() {
	if (!managed_tree || !managed_tree->get_root()) {
		return;
	}
	_apply_sort();
	_apply_filter();
}
