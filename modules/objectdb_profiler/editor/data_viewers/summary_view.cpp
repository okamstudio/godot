/**************************************************************************/
/*  summary_view.cpp                                                      */
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

#include "summary_view.h"

#include "core/os/time.h"
#include "editor/editor_node.h"
#include "scene/gui/center_container.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/resources/style_box_flat.h"

SnapshotSummaryView::SnapshotSummaryView() {
	set_name("Summary");

	set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);

	MarginContainer *mc = memnew(MarginContainer);
	mc->add_theme_constant_override("margin_left", 5);
	mc->add_theme_constant_override("margin_right", 5);
	mc->add_theme_constant_override("margin_top", 5);
	mc->add_theme_constant_override("margin_bottom", 5);
	mc->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);
	PanelContainer *content_wrapper = memnew(PanelContainer);
	content_wrapper->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);
	Ref<StyleBoxFlat> content_wrapper_sbf;
	content_wrapper_sbf.instantiate();
	content_wrapper_sbf->set_bg_color(EditorNode::get_singleton()->get_editor_theme()->get_color("dark_color_2", "Editor"));
	content_wrapper->add_theme_style_override(SceneStringName(panel), content_wrapper_sbf);
	content_wrapper->add_child(mc);
	add_child(content_wrapper);

	VBoxContainer *content = memnew(VBoxContainer);
	mc->add_child(content);
	content->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);

	PanelContainer *pc = memnew(PanelContainer);
	Ref<StyleBoxFlat> sbf;
	sbf.instantiate();
	sbf->set_bg_color(EditorNode::get_singleton()->get_editor_theme()->get_color("dark_color_3", "Editor"));
	pc->add_theme_style_override("panel", sbf);
	content->add_child(pc);
	pc->set_anchors_preset(LayoutPreset::PRESET_TOP_WIDE);
	Label *title = memnew(Label(TTR("ObjectDB Snapshot Summary")));
	pc->add_child(title);
	title->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	title->set_vertical_alignment(VerticalAlignment::VERTICAL_ALIGNMENT_CENTER);

	explainer_text = memnew(CenterContainer);
	explainer_text->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	explainer_text->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	content->add_child(explainer_text);
	VBoxContainer *explainer_lines = memnew(VBoxContainer);
	explainer_text->add_child(explainer_lines);
	Label *l1 = memnew(Label(TTR("Press 'Take ObjectDB Snapshot' to snapshot the ObjectDB.")));
	Label *l2 = memnew(Label(TTR("Memory in Godot is either owned natively by the engine or owned by the ObjectDB.")));
	Label *l3 = memnew(Label(TTR("ObjectDB Snapshots capture only memory owned by the ObjectDB.")));
	l1->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	l2->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	l3->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	explainer_lines->add_child(l1);
	explainer_lines->add_child(l2);
	explainer_lines->add_child(l3);

	ScrollContainer *sc = memnew(ScrollContainer);
	sc->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);
	sc->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	sc->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	content->add_child(sc);

	blurb_list = memnew(VBoxContainer);
	sc->add_child(blurb_list);
	blurb_list->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	blurb_list->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
}

void SnapshotSummaryView::show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) {
	SnapshotView::show_snapshot(p_data, p_diff_data);
	explainer_text->set_visible(false);

	String snapshot_a_name = diff_data == nullptr ? TTR("Snapshot") : TTR("Snapshot A");
	String snapshot_b_name = TTR("Snapshot B");

	_push_overview_blurb(snapshot_a_name + TTR(" Overview"), snapshot_data);
	if (diff_data) {
		_push_overview_blurb(snapshot_b_name + TTR(" Overview"), diff_data);
	}

	_push_node_blurb(snapshot_a_name + TTR(" Nodes"), snapshot_data);
	if (diff_data) {
		_push_node_blurb(snapshot_b_name + TTR(" Nodes"), diff_data);
	}

	_push_refcounted_blurb(snapshot_a_name + TTR(" RefCounteds"), snapshot_data);
	if (diff_data) {
		_push_refcounted_blurb(snapshot_b_name + TTR(" RefCounteds"), diff_data);
	}

	_push_object_blurb(snapshot_a_name + TTR(" Objects"), snapshot_data);
	if (diff_data) {
		_push_object_blurb(snapshot_b_name + TTR(" Objects"), diff_data);
	}
}

void SnapshotSummaryView::clear_snapshot() {
	// Just clear out the blurbs and leave the explainer.
	for (int i = 0; i < blurb_list->get_child_count(); i++) {
		blurb_list->get_child(i)->queue_free();
	}
	snapshot_data = nullptr;
	diff_data = nullptr;
	explainer_text->set_visible(true);
}

SummaryBlurb::SummaryBlurb(const String &p_title, const String &p_rtl_content) {
	add_theme_constant_override("margin_left", 2);
	add_theme_constant_override("margin_right", 2);
	add_theme_constant_override("margin_top", 2);
	add_theme_constant_override("margin_bottom", 2);

	label = memnew(RichTextLabel);
	label->add_theme_constant_override(SceneStringName(line_separation), 6);
	label->set_fit_content(true);
	label->set_use_bbcode(true);
	label->add_newline();
	label->push_bold();
	label->add_text(p_title);
	label->pop();
	label->add_newline();
	label->add_newline();
	label->append_text(p_rtl_content);
	add_child(label);
}

void SnapshotSummaryView::_push_overview_blurb(const String &p_title, GameStateSnapshot *p_snapshot) {
	String c = "";

	c += "[ul]\n";
	c += TTR(" [i]Name:[/i] ") + p_snapshot->name + "\n";
	if (p_snapshot->snapshot_context.has("timestamp")) {
		c += TTR(" [i]Timestamp:[/i] ") + Time::get_singleton()->get_datetime_string_from_unix_time((double)p_snapshot->snapshot_context["timestamp"]) + "\n";
	}
	if (p_snapshot->snapshot_context.has("game_version")) {
		c += TTR(" [i]Game Version:[/i] ") + (String)p_snapshot->snapshot_context["game_version"] + "\n";
	}
	if (p_snapshot->snapshot_context.has("editor_version")) {
		c += TTR(" [i]Editor Version:[/i] ") + (String)p_snapshot->snapshot_context["editor_version"] + "\n";
	}

	double bytes_to_mb = 0.000001;
	if (p_snapshot->snapshot_context.has("mem_usage")) {
		c += TTR(" [i]Memory Used:[/i] ") + String::num((double)((uint64_t)p_snapshot->snapshot_context["mem_usage"]) * bytes_to_mb, 3) + " MB\n";
	}
	if (p_snapshot->snapshot_context.has("mem_max_usage")) {
		c += TTR(" [i]Max Memory Used:[/i] ") + String::num((double)((uint64_t)p_snapshot->snapshot_context["mem_max_usage"]) * bytes_to_mb, 3) + " MB\n";
	}
	if (p_snapshot->snapshot_context.has("mem_available")) {
		// I'm guessing pretty hard about what this is supposed to be. It's hard coded to be -1 cast to a uint64_t in Memory.h,
		// so it _could_ be checking if we're on a 64 bit system, I think...
		c += TTR(" [i]Max uint64 value:[/i] ") + String::num_uint64((uint64_t)p_snapshot->snapshot_context["mem_available"]) + "\n";
	}
	c += TTR(" [i]Total Objects:[/i] ") + itos(p_snapshot->objects.size()) + "\n";

	int node_count = 0;
	for (const KeyValue<ObjectID, SnapshotDataObject *> &pair : p_snapshot->objects) {
		if (pair.value->is_node()) {
			node_count++;
		}
	}
	c += TTR(" [i]Total Nodes:[/i] ") + itos(node_count) + "\n";
	c += "[/ul]\n";

	blurb_list->add_child(memnew(SummaryBlurb(p_title, c)));
}

void SnapshotSummaryView::_push_node_blurb(const String &p_title, GameStateSnapshot *p_snapshot) {
	List<String> nodes;
	for (const KeyValue<ObjectID, SnapshotDataObject *> &pair : p_snapshot->objects) {
		// if it's a node AND it doesn't have a parent node
		if (pair.value->is_node() && !pair.value->extra_debug_data.has("node_parent") && pair.value->extra_debug_data.has("node_is_scene_root") && !pair.value->extra_debug_data["node_is_scene_root"]) {
			const String &node_name = pair.value->extra_debug_data["node_name"];
			nodes.push_back(node_name != "" ? node_name : pair.value->get_name());
		}
	}

	if (nodes.size() <= 1) {
		return;
	}

	String c = TTR("Multiple root nodes [i](possible call to 'remove_child' without 'queue_free')[/i]\n");
	c += "[ul]\n";
	for (const String &node : nodes) {
		c += " " + node + "\n";
	}
	c += "[/ul]\n";

	blurb_list->add_child(memnew(SummaryBlurb(p_title, c)));
}

void SnapshotSummaryView::_push_refcounted_blurb(const String &p_title, GameStateSnapshot *p_snapshot) {
	List<String> rcs;
	for (const KeyValue<ObjectID, SnapshotDataObject *> &pair : p_snapshot->objects) {
		if (pair.value->is_refcounted()) {
			int ref_count = (uint64_t)pair.value->extra_debug_data["ref_count"];
			Array ref_cycles = (Array)pair.value->extra_debug_data["ref_cycles"];

			if (ref_count == ref_cycles.size()) {
				rcs.push_back(pair.value->get_name());
			}
		}
	}

	if (rcs.is_empty()) {
		return;
	}

	String c = TTR("RefCounted objects only referenced in cycles [i](cycles often indicate a memory leaks)[/i]\n");
	c += "[ul]\n";
	for (const String &rc : rcs) {
		c += " " + rc + "\n";
	}
	c += "[/ul]\n";

	blurb_list->add_child(memnew(SummaryBlurb(p_title, c)));
}

void SnapshotSummaryView::_push_object_blurb(const String &p_title, GameStateSnapshot *p_snapshot) {
	List<String> objects;
	for (const KeyValue<ObjectID, SnapshotDataObject *> &pair : p_snapshot->objects) {
		if (pair.value->inbound_references.is_empty() && pair.value->outbound_references.is_empty()) {
			if (!pair.value->get_script().is_null()) {
				// This blurb will have a lot of false positives, but we can at least suppress false positives
				// from unreferenced nodes that are part of the scene tree.
				if (pair.value->is_node() && (bool)pair.value->extra_debug_data["node_is_scene_root"]) {
					objects.push_back(pair.value->get_name());
				}
			}
		}
	}

	if (objects.is_empty()) {
		return;
	}

	String c = TTR("Scripted objects not referenced by any other objects [i](unreferenced objects may indicate a memory leak)[/i]\n");
	c += "[ul]\n";
	for (const String &object : objects) {
		c += " " + object + "\n";
	}
	c += "[/ul]\n";

	blurb_list->add_child(memnew(SummaryBlurb(p_title, c)));
}
