/**************************************************************************/
/*  refcounted_view.cpp                                                   */
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

#include "refcounted_view.h"

#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/split_container.h"

SnapshotRefCountedView::SnapshotRefCountedView() {
	set_name(TTR("RefCounted"));
}

void SnapshotRefCountedView::show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) {
	SnapshotView::show_snapshot(p_data, p_diff_data);

	item_data_map.clear();
	data_item_map.clear();

	set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	refs_view = memnew(HSplitContainer);
	add_child(refs_view);
	refs_view->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);

	VBoxContainer *refs_column = memnew(VBoxContainer);
	refs_column->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);
	refs_view->add_child(refs_column);

	// Tree of Refs.
	refs_list = memnew(Tree);

	filter_bar = memnew(TreeSortAndFilterBar(refs_list, TTR("Filter RefCounteds")));
	refs_column->add_child(filter_bar);
	int offset = diff_data ? 1 : 0;
	if (diff_data) {
		filter_bar->add_sort_option(TTR("Snapshot"), TreeSortAndFilterBar::SortType::ALPHA_SORT, 0);
	}
	filter_bar->add_sort_option(TTR("Class"), TreeSortAndFilterBar::SortType::ALPHA_SORT, offset + 0);
	filter_bar->add_sort_option(TTR("Name"), TreeSortAndFilterBar::SortType::ALPHA_SORT, offset + 1);
	TreeSortAndFilterBar::SortOptionIndexes default_sort = filter_bar->add_sort_option(
			TTR("Native Refs"),
			TreeSortAndFilterBar::SortType::NUMERIC_SORT,
			offset + 2);
	filter_bar->add_sort_option(TTR("ObjectDB Refs"), TreeSortAndFilterBar::SortType::NUMERIC_SORT, offset + 3);
	filter_bar->add_sort_option(TTR("Total Refs"), TreeSortAndFilterBar::SortType::NUMERIC_SORT, offset + 4);
	filter_bar->add_sort_option(TTR("ObjectDB Cycles"), TreeSortAndFilterBar::SortType::NUMERIC_SORT, offset + 5);

	refs_list->set_select_mode(Tree::SelectMode::SELECT_ROW);
	refs_list->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	refs_list->set_hide_folding(false);
	refs_column->add_child(refs_list);
	refs_list->set_hide_root(true);
	refs_list->set_columns(diff_data ? 7 : 6);
	refs_list->set_column_titles_visible(true);
	if (diff_data) {
		refs_list->set_column_title(0, TTR("Snapshot"));
		refs_list->set_column_expand(0, false);
		refs_list->set_column_title_tooltip_text(0, "A: " + snapshot_data->name + ", B: " + diff_data->name);
	}
	refs_list->set_column_title(offset + 0, TTR("Class"));
	refs_list->set_column_expand(offset + 0, true);
	refs_list->set_column_title_tooltip_text(offset + 0, TTR("Object's class"));
	refs_list->set_column_title(offset + 1, TTR("Name"));
	refs_list->set_column_expand(offset + 1, true);
	refs_list->set_column_expand_ratio(offset + 1, 2);
	refs_list->set_column_title_tooltip_text(offset + 1, TTR("Object's name"));
	refs_list->set_column_title(offset + 2, TTR("Native Refs"));
	refs_list->set_column_expand(offset + 2, false);
	refs_list->set_column_title_tooltip_text(offset + 2, TTR("References not owned by the ObjectDB"));
	refs_list->set_column_title(offset + 3, TTR("ObjectDB Refs"));
	refs_list->set_column_expand(offset + 3, false);
	refs_list->set_column_title_tooltip_text(offset + 3, TTR("References owned by the ObjectDB"));
	refs_list->set_column_title(offset + 4, TTR("Total Refs"));
	refs_list->set_column_expand(offset + 4, false);
	refs_list->set_column_title_tooltip_text(offset + 4, TTR("ObjectDB References + Native References"));
	refs_list->set_column_title(offset + 5, TTR("ObjectDB Cycles"));
	refs_list->set_column_expand(offset + 5, false);
	refs_list->set_column_title_tooltip_text(offset + 5, TTR("Cycles detected in the ObjectDB"));
	refs_list->connect("item_selected", callable_mp(this, &SnapshotRefCountedView::_refcounted_selected));
	refs_list->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	refs_list->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	// View of the selected refcounted.
	ref_details = memnew(VBoxContainer);
	ref_details->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	refs_view->add_child(ref_details);
	ref_details->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	ref_details->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	refs_list->create_item();
	_insert_data(snapshot_data, TTR("A"));
	if (diff_data) {
		_insert_data(diff_data, TTR("B"));
	}

	// Push the split as far right as possible.
	filter_bar->select_sort(default_sort.descending);
	filter_bar->apply();
	refs_list->set_selected(refs_list->get_root()->get_first_child());

	callable_mp(this, &SnapshotRefCountedView::_set_split_to_center).call_deferred();
}

void SnapshotRefCountedView::_set_split_to_center() {
	refs_view->set_split_offset(refs_view->get_size().x * 0.5);
}

void SnapshotRefCountedView::_insert_data(GameStateSnapshot *p_snapshot, const String &p_name) {
	for (const KeyValue<ObjectID, SnapshotDataObject *> &pair : p_snapshot->objects) {
		if (!pair.value->is_refcounted()) {
			continue;
		}

		TreeItem *item = refs_list->create_item(refs_list->get_root());
		item_data_map[item] = pair.value;
		data_item_map[pair.value] = item;
		int total_refs = pair.value->extra_debug_data.has("ref_count") ? (uint64_t)pair.value->extra_debug_data["ref_count"] : 0;
		int objectdb_refs = pair.value->get_unique_inbound_references().size();
		int native_refs = total_refs - objectdb_refs;

		Array ref_cycles = (Array)pair.value->extra_debug_data["ref_cycles"];

		int offset = 0;
		if (diff_data) {
			item->set_text(0, p_name);
			item->set_tooltip_text(0, p_snapshot->name);
			offset = 1;
		}

		item->set_text(offset + 0, pair.value->type_name);
		item->set_text(offset + 1, pair.value->get_name());
		item->set_text(offset + 2, String::num_uint64(native_refs));
		item->set_text(offset + 3, String::num_uint64(objectdb_refs));
		item->set_text(offset + 4, String::num_uint64(total_refs));
		item->set_text(offset + 5, String::num_uint64(ref_cycles.size())); // Compute cycles and attach it to refcounted object.

		if (total_refs == ref_cycles.size()) {
			// Often, references are held by the engine so we can't know if we're stuck in a cycle or not
			// But if the full cycle is visible in the ObjectDB,
			// tell the user by highlighting the cells in red.
			item->set_custom_bg_color(offset + 5, Color(1, 0, 0, 0.1));
		}
	}
}

void SnapshotRefCountedView::_refcounted_selected() {
	for (int i = 0; i < ref_details->get_child_count(); i++) {
		ref_details->get_child(i)->queue_free();
	}

	SnapshotDataObject *d = item_data_map[refs_list->get_selected()];
	EditorNode::get_singleton()->push_item((Object *)d);

	DarkPanelContainer *refcounted_panel = memnew(DarkPanelContainer);
	VBoxContainer *refcounted_panel_content = memnew(VBoxContainer);
	refcounted_panel_content->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	refcounted_panel_content->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	ref_details->add_child(refcounted_panel);
	refcounted_panel->add_child(refcounted_panel_content);
	refcounted_panel_content->add_child(memnew(SpanningHeader(d->get_name())));

	ScrollContainer *properties_scroll = memnew(ScrollContainer);
	properties_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	properties_scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_AUTO);
	properties_scroll->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	properties_scroll->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	refcounted_panel_content->add_child(properties_scroll);

	VBoxContainer *properties_container = memnew(VBoxContainer);
	properties_container->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	properties_scroll->add_child(properties_container);
	properties_container->add_theme_constant_override("separation", 5);
	properties_container->add_theme_constant_override("margin_left", 2);
	properties_container->add_theme_constant_override("margin_right", 2);
	properties_container->add_theme_constant_override("margin_top", 2);
	properties_container->add_theme_constant_override("margin_bottom", 2);

	int total_refs = d->extra_debug_data.has("ref_count") ? (uint64_t)d->extra_debug_data["ref_count"] : 0;
	int objectdb_refs = d->get_unique_inbound_references().size();
	int native_refs = total_refs - objectdb_refs;
	Array ref_cycles = (Array)d->extra_debug_data["ref_cycles"];

	String count_str = "[ul]\n";
	count_str += TTR(" Native References: ") + String::num_uint64(native_refs) + "\n";
	count_str += TTR(" ObjectDB References: ") + String::num_uint64(objectdb_refs) + "\n";
	count_str += TTR(" Total References: ") + String::num_uint64(total_refs) + "\n";
	count_str += TTR(" ObjectDB Cycles: ") + String::num_uint64(ref_cycles.size()) + "\n";
	count_str += "[/ul]\n";
	RichTextLabel *counts = memnew(RichTextLabel(count_str));
	counts->set_use_bbcode(true);
	counts->set_fit_content(true);
	counts->add_theme_constant_override("line_separation", 6);
	properties_container->add_child(counts);

	if (d->inbound_references.size() > 0) {
		RichTextLabel *inbound_lbl = memnew(RichTextLabel(TTR("[center]ObjectDB References[center]")));
		inbound_lbl->set_fit_content(true);
		inbound_lbl->set_use_bbcode(true);
		properties_container->add_child(inbound_lbl);
		Tree *inbound_tree = memnew(Tree);
		inbound_tree->set_hide_folding(true);
		properties_container->add_child(inbound_tree);
		inbound_tree->set_select_mode(Tree::SelectMode::SELECT_ROW);
		inbound_tree->set_hide_root(true);
		inbound_tree->set_columns(3);
		inbound_tree->set_column_titles_visible(true);
		inbound_tree->set_column_title(0, TTR("Source"));
		inbound_tree->set_column_expand(0, true);
		inbound_tree->set_column_clip_content(0, false);
		inbound_tree->set_column_title_tooltip_text(0, TTR("Other object referencing this object"));
		inbound_tree->set_column_title(1, TTR("Property"));
		inbound_tree->set_column_expand(1, true);
		inbound_tree->set_column_clip_content(1, true);
		inbound_tree->set_column_title_tooltip_text(1, TTR("Property of other object referencing this object"));
		inbound_tree->set_column_title(2, TTR("Duplicate?"));
		inbound_tree->set_column_expand(2, false);
		inbound_tree->set_column_title_tooltip_text(2, TTR("Was the same reference returned by multiple getters on the source object?"));
		inbound_tree->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
		inbound_tree->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
		inbound_tree->set_v_scroll_enabled(false);
		inbound_tree->connect(SceneStringName(item_selected), callable_mp(this, &SnapshotRefCountedView::_ref_selected).bind(inbound_tree));

		// The same reference can exist as multiple properties of an object (for example, gdscript `@export` properties exist twice).
		// We flag for the user if a property is exposed multiple times so it's clearer why there are more references in the list
		// than the ObjectDB References count would suggest.
		HashMap<ObjectID, int> property_repeat_count;
		for (const KeyValue<String, ObjectID> &ob : d->inbound_references) {
			if (!property_repeat_count.has(ob.value)) {
				property_repeat_count.insert(ob.value, 0);
			}
			property_repeat_count[ob.value]++;
		}

		TreeItem *root = inbound_tree->create_item();
		for (const KeyValue<String, ObjectID> &ob : d->inbound_references) {
			TreeItem *i = inbound_tree->create_item(root);
			SnapshotDataObject *target = d->snapshot->objects[ob.value];
			i->set_text(0, target->get_name());
			i->set_text(1, ob.key);
			i->set_text(2, property_repeat_count[ob.value] > 1 ? TTR("Yes") : TTR("No"));
			reference_item_map[i] = data_item_map[target];
		}
	}

	if (ref_cycles.size() > 0) {
		properties_container->add_child(memnew(SpanningHeader(TTR("ObjectDB Cycles"))));
		Tree *cycles_tree = memnew(Tree);
		cycles_tree->set_hide_folding(true);
		properties_container->add_child(cycles_tree);
		cycles_tree->set_select_mode(Tree::SelectMode::SELECT_ROW);
		cycles_tree->set_hide_root(true);
		cycles_tree->set_columns(1);
		cycles_tree->set_column_titles_visible(false);
		cycles_tree->set_column_expand(0, true);
		cycles_tree->set_column_clip_content(0, false);
		cycles_tree->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
		cycles_tree->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
		cycles_tree->set_v_scroll_enabled(false);

		TreeItem *root = cycles_tree->create_item();
		for (const Variant &cycle : ref_cycles) {
			TreeItem *i = cycles_tree->create_item(root);
			i->set_text(0, cycle);
			i->set_text_overrun_behavior(0, TextServer::OverrunBehavior::OVERRUN_NO_TRIMMING);
		}
	}
}

void SnapshotRefCountedView::_ref_selected(Tree *p_source_tree) {
	TreeItem *target = reference_item_map[p_source_tree->get_selected()];
	if (target) {
		if (!target->is_visible()) {
			// Clear the filter if we can't see the node we just chose.
			filter_bar->clear_filter();
		}
		target->get_tree()->deselect_all();
		target->get_tree()->set_selected(target);
		target->get_tree()->ensure_cursor_is_visible();
	}
}
