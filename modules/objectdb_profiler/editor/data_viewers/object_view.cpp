/**************************************************************************/
/*  object_view.cpp                                                       */
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

#include "object_view.h"

#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/split_container.h"

SnapshotObjectView::SnapshotObjectView() {
	set_name(TTR("Objects"));
}

void SnapshotObjectView::show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) {
	SnapshotView::show_snapshot(p_data, p_diff_data);

	item_data_map.clear();
	data_item_map.clear();

	set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	objects_view = memnew(HSplitContainer);
	add_child(objects_view);
	objects_view->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);

	VBoxContainer *object_column = memnew(VBoxContainer);
	object_column->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);
	objects_view->add_child(object_column);
	object_column->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	object_column->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	object_list = memnew(Tree);

	filter_bar = memnew(TreeSortAndFilterBar(object_list, TTR("Filter Objects")));
	object_column->add_child(filter_bar);
	int sort_idx = 0;
	if (diff_data) {
		filter_bar->add_sort_option(TTR("Snapshot"), TreeSortAndFilterBar::SortType::ALPHA_SORT, sort_idx++);
	}
	filter_bar->add_sort_option(TTR("Class"), TreeSortAndFilterBar::SortType::ALPHA_SORT, sort_idx++);
	filter_bar->add_sort_option(TTR("Name"), TreeSortAndFilterBar::SortType::ALPHA_SORT, sort_idx++);
	filter_bar->add_sort_option(TTR("Inbound References"), TreeSortAndFilterBar::SortType::NUMERIC_SORT, sort_idx++);
	TreeSortAndFilterBar::SortOptionIndexes default_sort = filter_bar->add_sort_option(
			TTR("Outbound References"), TreeSortAndFilterBar::SortType::NUMERIC_SORT, sort_idx++);

	// Tree of objects.
	object_list->set_select_mode(Tree::SelectMode::SELECT_ROW);
	object_list->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	object_list->set_hide_folding(false);
	object_column->add_child(object_list);
	object_list->set_hide_root(true);
	object_list->set_columns(diff_data ? 5 : 4);
	object_list->set_column_titles_visible(true);
	int offset = 0;
	if (diff_data) {
		object_list->set_column_title(0, TTR("Snapshot"));
		object_list->set_column_expand(0, false);
		object_list->set_column_title_tooltip_text(0, "A: " + snapshot_data->name + ", B: " + diff_data->name);
		offset++;
	}
	object_list->set_column_title(offset + 0, TTR("Class"));
	object_list->set_column_expand(offset + 0, true);
	object_list->set_column_title_tooltip_text(offset + 0, TTR("Object's class"));
	object_list->set_column_title(offset + 1, TTR("Object"));
	object_list->set_column_expand(offset + 1, true);
	object_list->set_column_expand_ratio(offset + 1, 2);
	object_list->set_column_title_tooltip_text(offset + 1, TTR("Object's name"));
	object_list->set_column_title(offset + 2, TTR("In"));
	object_list->set_column_expand(offset + 2, false);
	object_list->set_column_clip_content(offset + 2, false);
	object_list->set_column_title_tooltip_text(offset + 2, TTR("Number of inbound references"));
	object_list->set_column_custom_minimum_width(offset + 2, 30 * EDSCALE);
	object_list->set_column_title(offset + 3, TTR("Out"));
	object_list->set_column_expand(offset + 3, false);
	object_list->set_column_clip_content(offset + 3, false);
	object_list->set_column_title_tooltip_text(offset + 3, TTR("Number of outbound references"));
	object_list->set_column_custom_minimum_width(offset + 2, 30 * EDSCALE);
	object_list->connect(SceneStringName(item_selected), callable_mp(this, &SnapshotObjectView::_object_selected));
	object_list->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	object_list->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	object_details = memnew(VBoxContainer);
	object_details->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	objects_view->add_child(object_details);
	object_details->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	object_details->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	object_list->create_item();
	_insert_data(snapshot_data, TTR("A"));
	if (diff_data) {
		_insert_data(diff_data, TTR("B"));
	}

	filter_bar->select_sort(default_sort.descending);
	filter_bar->apply();
	object_list->set_selected(object_list->get_root()->get_first_child());
	// Expand the left panel as wide as we can. Passing `INT_MAX` or any very large int will have the opposite effect
	// and shrink the left panel as small as it can go. So, pass an int we know is larger than the current panel, but not
	// 'very' large (whatever that exact number is).
	objects_view->set_split_offset(get_viewport_rect().size.x);
}

void SnapshotObjectView::_insert_data(GameStateSnapshot *p_snapshot, const String &p_name) {
	for (const KeyValue<ObjectID, SnapshotDataObject *> &pair : p_snapshot->objects) {
		TreeItem *item = object_list->create_item(object_list->get_root());
		int offset = 0;
		if (diff_data) {
			item->set_text(0, p_name);
			item->set_tooltip_text(0, p_snapshot->name);
			offset = 1;
		}
		item->set_text(offset + 0, pair.value->type_name);
		item->set_text(offset + 1, pair.value->get_name());
		item->set_text(offset + 2, String::num_uint64(pair.value->inbound_references.size()));
		item->set_text(offset + 3, String::num_uint64(pair.value->outbound_references.size()));
		item_data_map[item] = pair.value;
		data_item_map[pair.value] = item;
	}
}

void SnapshotObjectView::_object_selected() {
	reference_item_map.clear();

	for (int i = 0; i < object_details->get_child_count(); i++) {
		object_details->get_child(i)->queue_free();
	}

	SnapshotDataObject *d = item_data_map[object_list->get_selected()];
	EditorNode::get_singleton()->push_item((Object *)d);

	DarkPanelContainer *object_panel = memnew(DarkPanelContainer);
	VBoxContainer *object_panel_content = memnew(VBoxContainer);
	object_panel_content->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	object_panel_content->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	object_details->add_child(object_panel);
	object_panel->add_child(object_panel_content);
	object_panel_content->add_child(memnew(SpanningHeader(d->get_name())));

	ScrollContainer *properties_scroll = memnew(ScrollContainer);
	properties_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	properties_scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
	properties_scroll->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	properties_scroll->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	object_panel_content->add_child(properties_scroll);

	VBoxContainer *properties_container = memnew(VBoxContainer);
	properties_container->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	properties_scroll->add_child(properties_container);
	properties_container->add_theme_constant_override("separation", 8);

	inbound_tree = _make_references_list(properties_container, TTR("Inbound References"), TTR("Source"), TTR("Other object referencing this object"), TTR("Property"), TTR("Property of other object referencing this object"));
	inbound_tree->connect(SceneStringName(item_selected), callable_mp(this, &SnapshotObjectView::_reference_selected).bind(inbound_tree));
	TreeItem *ib_root = inbound_tree->create_item();
	for (const KeyValue<String, ObjectID> &ob : d->inbound_references) {
		TreeItem *i = inbound_tree->create_item(ib_root);
		SnapshotDataObject *target = d->snapshot->objects[ob.value];
		i->set_text(0, target->get_name());
		i->set_text(1, ob.key);
		reference_item_map[i] = data_item_map[target];
	}

	outbound_tree = _make_references_list(properties_container, TTR("Outbound References"), TTR("Property"), TTR("Property of this object referencing other object"), TTR("Target"), TTR("Other object being referenced"));
	outbound_tree->connect(SceneStringName(item_selected), callable_mp(this, &SnapshotObjectView::_reference_selected).bind(outbound_tree));
	TreeItem *ob_root = outbound_tree->create_item();
	for (const KeyValue<String, ObjectID> &ob : d->outbound_references) {
		TreeItem *i = outbound_tree->create_item(ob_root);
		SnapshotDataObject *target = d->snapshot->objects[ob.value];
		i->set_text(0, ob.key);
		i->set_text(1, target->get_name());
		reference_item_map[i] = data_item_map[target];
	}
}

void SnapshotObjectView::_reference_selected(Tree *p_source_tree) {
	TreeItem *ref_item = p_source_tree->get_selected();
	Tree *other_tree = p_source_tree == inbound_tree ? outbound_tree : inbound_tree;
	other_tree->deselect_all();
	TreeItem *other = reference_item_map[ref_item];
	if (other) {
		if (!other->is_visible()) {
			// Clear the filter if we can't see the node we just chose.
			filter_bar->clear_filter();
		}
		other->get_tree()->deselect_all();
		other->get_tree()->set_selected(other);
		other->get_tree()->ensure_cursor_is_visible();
	}
}

Tree *SnapshotObjectView::_make_references_list(Control *p_container, const String &p_name, const String &p_col_1, const String &p_col_1_tooltip, const String &p_col_2, const String &p_col_2_tooltip) {
	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	vbox->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	vbox->add_theme_constant_override("separation", 4);
	p_container->add_child(vbox);

	vbox->set_custom_minimum_size(Vector2(300, 0) * EDSCALE);

	RichTextLabel *lbl = memnew(RichTextLabel("[center]" + p_name + "[center]"));
	lbl->set_fit_content(true);
	lbl->set_use_bbcode(true);
	vbox->add_child(lbl);
	Tree *tree = memnew(Tree);
	tree->set_hide_folding(true);
	vbox->add_child(tree);
	tree->set_select_mode(Tree::SelectMode::SELECT_ROW);
	tree->set_hide_root(true);
	tree->set_columns(2);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, p_col_1);
	tree->set_column_expand(0, true);
	tree->set_column_title_tooltip_text(0, p_col_1_tooltip);
	tree->set_column_clip_content(0, false);
	tree->set_column_title(1, p_col_2);
	tree->set_column_expand(1, true);
	tree->set_column_clip_content(1, false);
	tree->set_column_title_tooltip_text(1, p_col_2_tooltip);
	tree->set_v_scroll_enabled(false);
	tree->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	return tree;
}
