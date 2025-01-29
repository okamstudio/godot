/**************************************************************************/
/*  history_dock.cpp                                                      */
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

#include "history_dock.h"

#include "core/io/config_file.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/gui/check_box.h"
#include "scene/gui/item_list.h"

struct SortActionsByTimestamp {
	bool operator()(const EditorUndoRedoManager::Action &l, const EditorUndoRedoManager::Action &r) const {
		return l.timestamp > r.timestamp;
	}
};

void HistoryDock::on_history_changed() {
	if (is_visible_in_tree()) {
		refresh_history();
	} else {
		need_refresh = true;
	}
}

void HistoryDock::refresh_history() {
	action_list->clear();
	bool include_scene = current_scene_checkbox->is_pressed();
	bool include_global = global_history_checkbox->is_pressed();

	if (!include_scene && !include_global) {
		action_list->add_item(TTR("The Beginning"));
		return;
	}

	const EditorUndoRedoManager::History &current_scene_history = ur_manager->get_or_create_history(EditorNode::get_editor_data().get_current_edited_scene_history_id());
	const EditorUndoRedoManager::History &global_history = ur_manager->get_or_create_history(EditorUndoRedoManager::GLOBAL_HISTORY);

	Vector<EditorUndoRedoManager::Action> full_history;
	{
		int full_size = 0;
		if (include_scene) {
			full_size += current_scene_history.redo_stack.size() + current_scene_history.undo_stack.size();
		}
		if (include_global) {
			full_size += global_history.redo_stack.size() + global_history.undo_stack.size();
		}
		full_history.resize(full_size);
	}

	int i = 0;
	if (include_scene) {
		for (const EditorUndoRedoManager::Action &E : current_scene_history.redo_stack) {
			full_history.write[i] = E;
			i++;
		}
		for (const EditorUndoRedoManager::Action &E : current_scene_history.undo_stack) {
			full_history.write[i] = E;
			i++;
		}
	}
	if (include_global) {
		for (const EditorUndoRedoManager::Action &E : global_history.redo_stack) {
			full_history.write[i] = E;
			i++;
		}
		for (const EditorUndoRedoManager::Action &E : global_history.undo_stack) {
			full_history.write[i] = E;
			i++;
		}
	}

	full_history.sort_custom<SortActionsByTimestamp>();
	for (const EditorUndoRedoManager::Action &E : full_history) {
		action_list->add_item(E.action_name);
		if (E.history_id == EditorUndoRedoManager::GLOBAL_HISTORY) {
			action_list->set_item_custom_fg_color(-1, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
		}
	}

	action_list->add_item(TTR("The Beginning"));
	refresh_version();
}

void HistoryDock::on_version_changed() {
	if (is_visible_in_tree()) {
		refresh_version();
	} else {
		need_refresh = true;
	}
}

void HistoryDock::refresh_version() {
	int idx = 0;
	bool include_scene = current_scene_checkbox->is_pressed();
	bool include_global = global_history_checkbox->is_pressed();

	if (!include_scene && !include_global) {
		current_version = idx;
		action_list->set_current(idx);
		return;
	}

	const EditorUndoRedoManager::History &current_scene_history = ur_manager->get_or_create_history(EditorNode::get_editor_data().get_current_edited_scene_history_id());
	const EditorUndoRedoManager::History &global_history = ur_manager->get_or_create_history(EditorUndoRedoManager::GLOBAL_HISTORY);
	double newest_undo_timestamp = 0;

	if (include_scene && !current_scene_history.undo_stack.is_empty()) {
		newest_undo_timestamp = current_scene_history.undo_stack.front()->get().timestamp;
	}

	if (include_global && !global_history.undo_stack.is_empty()) {
		double global_undo_timestamp = global_history.undo_stack.front()->get().timestamp;
		if (global_undo_timestamp > newest_undo_timestamp) {
			newest_undo_timestamp = global_undo_timestamp;
		}
	}

	if (include_scene) {
		int skip = 0;
		for (const EditorUndoRedoManager::Action &E : current_scene_history.redo_stack) {
			if (E.timestamp < newest_undo_timestamp) {
				skip++;
			} else {
				break;
			}
		}
		idx += current_scene_history.redo_stack.size() - skip;
	}

	if (include_global) {
		int skip = 0;
		for (const EditorUndoRedoManager::Action &E : global_history.redo_stack) {
			if (E.timestamp < newest_undo_timestamp) {
				skip++;
			} else {
				break;
			}
		}
		idx += global_history.redo_stack.size() - skip;
	}

	current_version = idx;
	action_list->set_current(idx);
}

void HistoryDock::_save_layout_to_config(Ref<ConfigFile> p_layout, const String &p_section) const {
	p_layout->set_value(p_section, "dock_history_include_scene", current_scene_checkbox->is_pressed());
	p_layout->set_value(p_section, "dock_history_include_global", global_history_checkbox->is_pressed());
}

void HistoryDock::_load_layout_from_config(Ref<ConfigFile> p_layout, const String &p_section) {
	current_scene_checkbox->set_pressed_no_signal(p_layout->get_value(p_section, "dock_history_include_scene", true));
	global_history_checkbox->set_pressed_no_signal(p_layout->get_value(p_section, "dock_history_include_global", true));
	refresh_history();
}

void HistoryDock::seek_history(int p_index) {
	bool include_scene = current_scene_checkbox->is_pressed();
	bool include_global = global_history_checkbox->is_pressed();

	if (!include_scene && !include_global) {
		return;
	}
	int current_scene_id = EditorNode::get_editor_data().get_current_edited_scene_history_id();

	while (current_version < p_index) {
		if (include_scene) {
			if (include_global) {
				ur_manager->undo();
			} else {
				ur_manager->undo_history(current_scene_id);
			}
		} else {
			ur_manager->undo_history(EditorUndoRedoManager::GLOBAL_HISTORY);
		}
	}

	while (current_version > p_index) {
		if (include_scene) {
			if (include_global) {
				ur_manager->redo();
			} else {
				ur_manager->redo_history(current_scene_id);
			}
		} else {
			ur_manager->redo_history(EditorUndoRedoManager::GLOBAL_HISTORY);
		}
	}
}

void HistoryDock::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_READY: {
			EditorNode::get_singleton()->connect("scene_changed", callable_mp(this, &HistoryDock::on_history_changed));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible_in_tree() && need_refresh) {
				refresh_history();
			}
		} break;
	}
}

void HistoryDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_save_layout_to_config"), &HistoryDock::_save_layout_to_config);
	ClassDB::bind_method(D_METHOD("_load_layout_from_config"), &HistoryDock::_load_layout_from_config);
}

HistoryDock::HistoryDock() {
	set_name("History");

	ur_manager = EditorUndoRedoManager::get_singleton();
	ur_manager->connect("history_changed", callable_mp(this, &HistoryDock::on_history_changed));
	ur_manager->connect("version_changed", callable_mp(this, &HistoryDock::on_version_changed));

	HBoxContainer *mode_hb = memnew(HBoxContainer);
	add_child(mode_hb);

	current_scene_checkbox = memnew(CheckBox);
	mode_hb->add_child(current_scene_checkbox);
	current_scene_checkbox->set_flat(true);
	current_scene_checkbox->set_text(TTR("Scene"));
	current_scene_checkbox->set_h_size_flags(SIZE_EXPAND_FILL);
	current_scene_checkbox->set_clip_text(true);
	current_scene_checkbox->connect(SceneStringName(toggled), callable_mp(this, &HistoryDock::refresh_history).unbind(1));

	global_history_checkbox = memnew(CheckBox);
	mode_hb->add_child(global_history_checkbox);
	global_history_checkbox->set_flat(true);
	global_history_checkbox->set_text(TTR("Global"));
	global_history_checkbox->set_h_size_flags(SIZE_EXPAND_FILL);
	global_history_checkbox->set_clip_text(true);
	global_history_checkbox->connect(SceneStringName(toggled), callable_mp(this, &HistoryDock::refresh_history).unbind(1));

	action_list = memnew(ItemList);
	action_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	add_child(action_list);
	action_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	action_list->connect(SceneStringName(item_selected), callable_mp(this, &HistoryDock::seek_history));
}
