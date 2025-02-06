/**************************************************************************/
/*  json_view.cpp                                                         */
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

#include "json_view.h"

#include "core/io/json.h"
#include "scene/gui/center_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/split_container.h"
#include "shared_controls.h"

SnapshotJsonView::SnapshotJsonView() {
	set_name(TTR("JSON"));
}

void SnapshotJsonView::show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) {
	// Lock isn't released until the data processing background thread has finished running
	// and the json has been passed back to the main thread and displayed.
	SnapshotView::show_snapshot(p_data, p_diff_data);

	HSplitContainer *box = memnew(HSplitContainer);
	box->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);
	add_child(box);

	loading_panel = memnew(DarkPanelContainer);
	CenterContainer *loading_center = memnew(CenterContainer);
	Label *loading_label = memnew(Label(TTR("Loading")));
	add_child(loading_panel);
	loading_panel->add_child(loading_center);
	loading_center->add_child(loading_label);
	loading_panel->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);
	loading_center->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);

	VBoxContainer *json_box = memnew(VBoxContainer);
	json_box->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	json_box->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	box->add_child(json_box);
	String hdr_a_text = diff_data ? TTR("Snapshot A JSON") : TTR("Snapshot JSON");
	SpanningHeader *hdr_a = memnew(SpanningHeader(hdr_a_text));
	if (diff_data) {
		hdr_a->set_tooltip_text(TTR("Snapshot A: ") + snapshot_data->name);
	}
	json_box->add_child(hdr_a);

	Ref<EditorJsonVisualizerSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate(List<String>());

	json_content = memnew(EditorJsonVisualizer);
	json_content->load_theme(syntax_highlighter);
	json_content->set_name(hdr_a_text);
	json_content->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	json_content->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
	json_box->add_child(json_content);

	if (diff_data) {
		VBoxContainer *diff_json_box = memnew(VBoxContainer);
		diff_json_box->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
		diff_json_box->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
		box->add_child(diff_json_box);
		String hrd_b_text = TTR("Snapshot B JSON");
		SpanningHeader *hdr_b = memnew(SpanningHeader(hrd_b_text));
		hdr_b->set_tooltip_text(TTR("Snapshot B: ") + diff_data->name);
		diff_json_box->add_child(hdr_b);

		diff_json_content = memnew(EditorJsonVisualizer);
		diff_json_content->load_theme(syntax_highlighter);
		diff_json_content->set_name(hrd_b_text);
		diff_json_content->set_v_size_flags(SizeFlags::SIZE_EXPAND_FILL);
		diff_json_content->set_h_size_flags(SizeFlags::SIZE_EXPAND_FILL);
		diff_json_box->add_child(diff_json_content);
	}

	WorkerThreadPool::get_singleton()->add_native_task(&SnapshotJsonView::_serialization_worker, this);
}

String SnapshotJsonView::_snapshot_to_json(GameStateSnapshot *p_snapshot) {
	if (p_snapshot == nullptr) {
		return "";
	}
	Dictionary json_data;
	json_data["name"] = p_snapshot->name;
	Dictionary objects;
	for (const KeyValue<ObjectID, SnapshotDataObject *> &obj : p_snapshot->objects) {
		Dictionary obj_data;
		obj_data["type_name"] = obj.value->type_name;

		Array prop_list;
		for (const PropertyInfo &prop : obj.value->prop_list) {
			prop_list.push_back((Dictionary)prop);
		}
		objects["prop_list"] = prop_list;

		Dictionary prop_values;
		for (const KeyValue<StringName, Variant> &prop : obj.value->prop_values) {
			prop_values[prop.key] = prop.value;
		}
		obj_data["prop_values"] = prop_values;

		objects[obj.key] = obj_data;
	}
	json_data["objects"] = objects;
	return JSON::stringify(json_data, "    ", true, true);
}

void SnapshotJsonView::_serialization_worker(void *p_ud) {
	// About 0.3s to serialize snapshots in a small game.
	SnapshotJsonView *self = static_cast<SnapshotJsonView *>(p_ud);
	GameStateSnapshot *snapshot_data = self->snapshot_data;
	GameStateSnapshot *diff_data = self->diff_data;
	// let the message queue figure out if self is still a valid object or if it's been destroyed.
	MessageQueue::get_singleton()->push_call(self, "_update_text",
			snapshot_data, diff_data,
			_snapshot_to_json(snapshot_data),
			_snapshot_to_json(diff_data));
}

void SnapshotJsonView::_update_text(GameStateSnapshot *p_data_ptr, GameStateSnapshot *p_diff_ptr, const String &p_data_str, const String &p_diff_data_str) {
	if (p_data_ptr != snapshot_data || p_diff_ptr != diff_data) {
		// If the GameStateSnapshots we generated strings for no longer match the snapshots we asked for,
		// throw these results away. We'll get more from a different worker process.
		return;
	}

	// About 5s to insert the string into the editor.
	json_content->set_text(p_data_str);
	if (diff_data) {
		diff_json_content->set_text(p_diff_data_str);
	}
	loading_panel->queue_free();
	// Loading json done, release the lock.
}

void SnapshotJsonView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_text", "p_data_ptr", "p_diff_ptr", "p_data_str", "p_diff_data_str"), &SnapshotJsonView::_update_text);
}
