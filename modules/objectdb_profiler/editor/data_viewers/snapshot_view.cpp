/**************************************************************************/
/*  snapshot_view.cpp                                                     */
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

#include "snapshot_view.h"

#include "scene/gui/label.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/tree.h"

void SnapshotView::clear_snapshot() {
	snapshot_data = nullptr;
	diff_data = nullptr;
	for (int i = 0; i < get_child_count(); i++) {
		get_child(i)->queue_free();
	}
}

void SnapshotView::show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) {
	clear_snapshot();
	snapshot_data = p_data;
	diff_data = p_diff_data;
}

bool SnapshotView::is_showing_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) {
	return p_data == snapshot_data && p_diff_data == diff_data;
}

List<TreeItem *> SnapshotView::_get_children_recursive(Tree *p_tree) {
	List<TreeItem *> found_items;
	List<TreeItem *> items_to_check;
	if (p_tree && p_tree->get_root()) {
		items_to_check.push_back(p_tree->get_root());
	}
	while (items_to_check.size() > 0) {
		TreeItem *to_check = items_to_check.front()->get();
		items_to_check.pop_front();
		found_items.push_back(to_check);
		for (int i = 0; i < to_check->get_child_count(); i++) {
			items_to_check.push_back(to_check->get_child(i));
		}
	}
	return found_items;
}
