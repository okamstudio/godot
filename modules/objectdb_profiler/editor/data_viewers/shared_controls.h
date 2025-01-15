/**************************************************************************/
/*  shared_controls.h                                                     */
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

#ifndef SHARED_CONTROLS_H
#define SHARED_CONTROLS_H

#include "scene/gui/box_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/tree.h"

class MenuButton;

class SpanningHeader : public PanelContainer {
	GDCLASS(SpanningHeader, PanelContainer);

public:
	SpanningHeader(const String &p_text);
};

class DarkPanelContainer : public PanelContainer {
	GDCLASS(DarkPanelContainer, PanelContainer);

public:
	DarkPanelContainer();
};

// Utility class that creates a filter text box and a sort menu.
// Takes a reference to a tree and applies the sort and filter to the tree.
class TreeSortAndFilterBar : public HBoxContainer {
	GDCLASS(TreeSortAndFilterBar, HBoxContainer);

public:
	// The ways a column can be sorted, either alphabetically or numerically.
	enum SortType {
		NUMERIC_SORT = 0,
		ALPHA_SORT,
		SORT_TYPE_MAX
	};

	// Returned when a new sort is added. Each new sort can be either ascending or descending,
	// so we return the index of each sort option.
	struct SortOptionIndexes {
		int ascending;
		int descending;
	};

protected:
	// Context needed to sort the tree in a certain way.
	// Combines a sort type, the column to apply it, and if it's ascending or descending.
	struct SortItem {
		SortItem() {}
		SortItem(int p_id, const String &p_label, SortType p_type, bool p_ascending, int p_column) :
				id(p_id), label(p_label), type(p_type), ascending(p_ascending), column(p_column) {}
		int id = 0;
		String label;
		SortType type = SortType::NUMERIC_SORT;
		bool ascending = false;
		int column = 0;
	};

	struct TreeItemColumn {
		TreeItemColumn() {}
		TreeItemColumn(TreeItem *p_item, int p_column) :
				item(p_item), column(p_column) {}
		TreeItem *item = nullptr;
		int column;
	};

	struct TreeItemAlphaComparator {
		bool operator()(const TreeItemColumn &p_a, const TreeItemColumn &p_b) const {
			return NoCaseComparator()(p_a.item->get_text(p_a.column), p_b.item->get_text(p_b.column));
		}
	};

	struct TreeItemNumericComparator {
		bool operator()(const TreeItemColumn &p_a, const TreeItemColumn &p_b) const {
			return p_a.item->get_text(p_a.column).to_int() < p_b.item->get_text(p_b.column).to_int();
		}
	};

	LineEdit *filter_edit = nullptr;
	MenuButton *sort_button = nullptr;
	Tree *managed_tree = nullptr;
	HashMap<int, SortItem> sort_items;
	int current_sort = 0;

	void _apply_filter(TreeItem *p_current_node = nullptr);
	void _apply_sort();
	void _sort_changed(int p_id);
	void _filter_changed(const String &p_filter);

public:
	TreeSortAndFilterBar(Tree *p_managed_tree, const String &p_filter_placeholder_text);
	void _notification(int p_what);
	SortOptionIndexes add_sort_option(const String &p_new_option, SortType p_sort_type, int p_sort_column, bool p_is_default = false);
	void clear_filter();
	void clear();
	void select_sort(int p_item_id);
	void apply();
};

#endif // SHARED_CONTROLS_H
