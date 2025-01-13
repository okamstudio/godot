/**************************************************************************/
/*  group_settings_editor.cpp                                             */
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

#include "group_settings_editor.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/filesystem_dock.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/packed_scene.h"

void GroupSettingsEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			update_groups();
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			add_button->set_button_icon(get_editor_theme_icon(SNAME("Add")));
		} break;
	}
}

void GroupSettingsEditor::_item_edited() {
	if (updating_groups) {
		return;
	}

	TreeItem *ti = tree->get_edited();
	int column = tree->get_edited_column();

	if (!ti) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (column == 1) {
		// Description Edited.
		String name = ti->get_text(0);
		String new_description = ti->get_text(1);
		String old_description = ti->get_meta("__description");

		if (new_description == old_description) {
			return;
		}

		name = GLOBAL_GROUP_PREFIX + name;

		undo_redo->create_action(TTR("Set Group Description"));

		undo_redo->add_do_property(ProjectSettings::get_singleton(), name, new_description);
		undo_redo->add_undo_property(ProjectSettings::get_singleton(), name, old_description);

		undo_redo->add_do_method(this, CoreStringName(call_deferred), "update_groups");
		undo_redo->add_undo_method(this, CoreStringName(call_deferred), "update_groups");

		undo_redo->add_do_method(this, "emit_signal", group_changed);
		undo_redo->add_undo_method(this, "emit_signal", group_changed);

		undo_redo->commit_action();
	}
}

void GroupSettingsEditor::_item_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	TreeItem *ti = Object::cast_to<TreeItem>(p_item);

	if (!ti) {
		return;
	}
	ti->select(0);
	_show_remove_dialog();
}

String GroupSettingsEditor::_check_new_group_name(const String &p_name) {
	if (p_name.is_empty()) {
		return TTR("Invalid group name. It cannot be empty.");
	}

	if (ProjectSettings::get_singleton()->has_global_group(p_name)) {
		return vformat(TTR("A group with the name '%s' already exists."), p_name);
	}

	return "";
}

void GroupSettingsEditor::_check_rename() {
	String new_name = rename_group->get_text().strip_edges();
	String old_name = rename_group_dialog->get_meta("__name");

	if (new_name == old_name) {
		return;
	}

	if (new_name.is_empty()) {
		rename_validation_panel->set_message(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Group can't be empty."), EditorValidationPanel::MSG_ERROR);
	} else if (ProjectSettings::get_singleton()->has_global_group(new_name)) {
		rename_validation_panel->set_message(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Group already exists."), EditorValidationPanel::MSG_ERROR);
	}
}

void GroupSettingsEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("remove_references"), &GroupSettingsEditor::remove_references);
	ClassDB::bind_method(D_METHOD("rename_references"), &GroupSettingsEditor::rename_references);

	ClassDB::bind_method(D_METHOD("update_groups"), &GroupSettingsEditor::update_groups);

	ADD_SIGNAL(MethodInfo("group_changed"));
}

void GroupSettingsEditor::_add_group(const String &p_name, const String &p_description) {
	String name = p_name.strip_edges();

	String error = _check_new_group_name(name);
	if (!error.is_empty()) {
		show_message(error);
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Group"));

	name = GLOBAL_GROUP_PREFIX + name;

	undo_redo->add_do_property(ProjectSettings::get_singleton(), name, p_description);
	undo_redo->add_undo_property(ProjectSettings::get_singleton(), name, Variant());

	undo_redo->add_do_method(this, CoreStringName(call_deferred), "update_groups");
	undo_redo->add_undo_method(this, CoreStringName(call_deferred), "update_groups");

	undo_redo->add_do_method(this, "emit_signal", group_changed);
	undo_redo->add_undo_method(this, "emit_signal", group_changed);

	undo_redo->commit_action();

	group_name->clear();
	group_description->clear();
}

void GroupSettingsEditor::_add_group() {
	_add_group(group_name->get_text(), group_description->get_text());
}

void GroupSettingsEditor::_text_submitted(const String &p_text) {
	if (!add_button->is_disabled()) {
		_add_group();
	}
}

void GroupSettingsEditor::_group_name_text_changed(const String &p_name) {
	String error = _check_new_group_name(p_name.strip_edges());
	add_button->set_tooltip_text(error);
	add_button->set_disabled(!error.is_empty());
}

void GroupSettingsEditor::_modify_references(const StringName &p_name, const StringName &p_new_name, bool p_is_rename) {
	HashSet<String> scenes;

	HashMap<StringName, HashSet<StringName>> scene_groups_cache = ProjectSettings::get_singleton()->get_scene_groups_cache();
	for (const KeyValue<StringName, HashSet<StringName>> &E : scene_groups_cache) {
		if (E.value.has(p_name)) {
			scenes.insert(E.key);
		}
	}

	int steps = scenes.size();
	Vector<EditorData::EditedScene> edited_scenes = EditorNode::get_editor_data().get_edited_scenes();
	for (const EditorData::EditedScene &es : edited_scenes) {
		if (!es.root) {
			continue;
		}
		if (es.path.is_empty()) {
			++steps;
		} else if (!scenes.has(es.path)) {
			++steps;
		}
	}

	String progress_task = p_is_rename ? "rename_reference" : "remove_references";
	String progress_label = p_is_rename ? TTR("Renaming Group References") : TTR("Removing Group References");
	EditorProgress progress(progress_task, progress_label, steps);

	int step = 0;
	// Update opened scenes.
	HashSet<String> edited_scenes_path;
	for (const EditorData::EditedScene &es : edited_scenes) {
		if (!es.root) {
			continue;
		}
		progress.step(es.path, step++);
		bool edited = p_is_rename ? rename_node_references(es.root, p_name, p_new_name) : remove_node_references(es.root, p_name);
		if (!es.path.is_empty()) {
			scenes.erase(es.path);
			if (edited) {
				edited_scenes_path.insert(es.path);
			}
		}
	}
	if (!edited_scenes_path.is_empty()) {
		EditorNode::get_singleton()->save_scene_list(edited_scenes_path);
		SceneTreeDock::get_singleton()->get_tree_editor()->update_tree();
	}

	for (const String &E : scenes) {
		Ref<PackedScene> packed_scene = ResourceLoader::load(E);
		progress.step(E, step++);
		ERR_CONTINUE(packed_scene.is_null());
		if (p_is_rename) {
			if (packed_scene->get_state()->rename_group_references(p_name, p_new_name)) {
				ResourceSaver::save(packed_scene, E);
			}
		} else {
			if (packed_scene->get_state()->remove_group_references(p_name)) {
				ResourceSaver::save(packed_scene, E);
			}
		}
	}
}

void GroupSettingsEditor::remove_references(const StringName &p_name) {
	_modify_references(p_name, StringName(), false);
}

void GroupSettingsEditor::rename_references(const StringName &p_old_name, const StringName &p_new_name) {
	_modify_references(p_old_name, p_new_name, true);
}

bool GroupSettingsEditor::remove_node_references(Node *p_node, const StringName &p_name) {
	bool edited = false;
	if (p_node->is_in_group(p_name)) {
		p_node->remove_from_group(p_name);
		edited = true;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		edited |= remove_node_references(p_node->get_child(i), p_name);
	}
	return edited;
}

bool GroupSettingsEditor::rename_node_references(Node *p_node, const StringName &p_old_name, const StringName &p_new_name) {
	bool edited = false;
	if (p_node->is_in_group(p_old_name)) {
		p_node->remove_from_group(p_old_name);
		p_node->add_to_group(p_new_name, true);
		edited = true;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		edited |= rename_node_references(p_node->get_child(i), p_old_name, p_new_name);
	}
	return edited;
}

void GroupSettingsEditor::update_groups() {
	if (updating_groups) {
		return;
	}
	updating_groups = true;
	groups_cache = ProjectSettings::get_singleton()->get_global_groups_list();

	tree->clear();
	TreeItem *root = tree->create_item();

	List<StringName> keys;
	for (const KeyValue<StringName, String> &E : groups_cache) {
		keys.push_back(E.key);
	}
	keys.sort_custom<NoCaseComparator>();

	for (const StringName &E : keys) {
		TreeItem *item = tree->create_item(root);
		item->set_meta("__name", E);
		item->set_meta("__description", groups_cache[E]);

		item->set_text(0, E);
		item->set_editable(0, false);

		item->set_text(1, groups_cache[E]);
		item->set_editable(1, true);
		item->add_button(2, get_editor_theme_icon(SNAME("Remove")));
		item->set_selectable(2, false);
	}

	updating_groups = false;
}

void GroupSettingsEditor::connect_filesystem_dock_signals(FileSystemDock *p_fs_dock) {
	p_fs_dock->connect("files_moved", callable_mp(ProjectSettings::get_singleton(), &ProjectSettings::remove_scene_groups_cache).unbind(1));
	p_fs_dock->connect("file_removed", callable_mp(ProjectSettings::get_singleton(), &ProjectSettings::remove_scene_groups_cache));
}

void GroupSettingsEditor::_confirm_rename() {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	String old_name = ti->get_meta("__name");
	String new_name = rename_group->get_text().strip_edges();

	if (old_name == new_name) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Rename Group"));

	String property_new_name = GLOBAL_GROUP_PREFIX + new_name;
	String property_old_name = GLOBAL_GROUP_PREFIX + old_name;

	String description = ti->get_meta("__description");

	undo_redo->add_do_property(ProjectSettings::get_singleton(), property_new_name, description);
	undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_new_name, Variant());

	undo_redo->add_do_property(ProjectSettings::get_singleton(), property_old_name, Variant());
	undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_old_name, description);

	if (rename_check_box->is_pressed()) {
		undo_redo->add_do_method(this, "rename_references", old_name, new_name);
		undo_redo->add_undo_method(this, "rename_references", new_name, old_name);
	}

	undo_redo->add_do_method(this, CoreStringName(call_deferred), "update_groups");
	undo_redo->add_undo_method(this, CoreStringName(call_deferred), "update_groups");

	undo_redo->add_do_method(this, "emit_signal", group_changed);
	undo_redo->add_undo_method(this, "emit_signal", group_changed);

	undo_redo->commit_action();
}

void GroupSettingsEditor::_confirm_delete() {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	String name = ti->get_text(0);
	String description = groups_cache[name];
	String property_name = GLOBAL_GROUP_PREFIX + name;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove Group"));

	undo_redo->add_do_property(ProjectSettings::get_singleton(), property_name, Variant());
	undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_name, description);

	if (remove_check_box->is_pressed()) {
		undo_redo->add_do_method(this, "remove_references", name);
	}

	undo_redo->add_do_method(this, CoreStringName(call_deferred), "update_groups");
	undo_redo->add_undo_method(this, CoreStringName(call_deferred), "update_groups");

	undo_redo->add_do_method(this, "emit_signal", group_changed);
	undo_redo->add_undo_method(this, "emit_signal", group_changed);

	undo_redo->commit_action();
}

void GroupSettingsEditor::show_message(const String &p_message) {
	message->set_text(p_message);
	message->popup_centered();
}

void GroupSettingsEditor::_show_remove_dialog() {
	if (!remove_dialog) {
		remove_dialog = memnew(ConfirmationDialog);
		remove_dialog->connect(SceneStringName(confirmed), callable_mp(this, &GroupSettingsEditor::_confirm_delete));

		VBoxContainer *vbox = memnew(VBoxContainer);
		remove_label = memnew(Label);
		vbox->add_child(remove_label);

		remove_check_box = memnew(CheckBox);
		remove_check_box->set_text(TTR("Delete references from all scenes"));
		vbox->add_child(remove_check_box);

		remove_dialog->add_child(vbox);

		add_child(remove_dialog);
	}

	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	remove_check_box->set_pressed(false);
	remove_label->set_text(vformat(TTR("Delete group \"%s\"?"), ti->get_text(0)));

	remove_dialog->reset_size();
	remove_dialog->popup_centered();
}

void GroupSettingsEditor::_show_rename_dialog() {
	if (!rename_group_dialog) {
		rename_group_dialog = memnew(ConfirmationDialog);
		rename_group_dialog->set_title(TTR("Rename Group"));
		rename_group_dialog->connect(SceneStringName(confirmed), callable_mp(this, &GroupSettingsEditor::_confirm_rename));

		VBoxContainer *vbc = memnew(VBoxContainer);
		rename_group_dialog->add_child(vbc);

		HBoxContainer *hbc = memnew(HBoxContainer);
		hbc->add_child(memnew(Label(TTR("Name:"))));

		rename_group = memnew(LineEdit);
		rename_group->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
		hbc->add_child(rename_group);
		vbc->add_child(hbc);

		rename_group_dialog->register_text_enter(rename_group);

		rename_validation_panel = memnew(EditorValidationPanel);
		rename_validation_panel->add_line(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Group name is valid."));
		rename_validation_panel->set_update_callback(callable_mp(this, &GroupSettingsEditor::_check_rename));
		rename_validation_panel->set_accept_button(rename_group_dialog->get_ok_button());

		rename_group->connect(SceneStringName(text_changed), callable_mp(rename_validation_panel, &EditorValidationPanel::update).unbind(1));

		vbc->add_child(rename_validation_panel);

		rename_check_box = memnew(CheckBox);
		rename_check_box->set_text(TTR("Rename references in all scenes"));
		vbc->add_child(rename_check_box);

		add_child(rename_group_dialog);
	}

	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	rename_check_box->set_pressed(false);

	String name = ti->get_meta("__name");

	rename_group->set_text(name);
	rename_group_dialog->set_meta("__name", name);

	rename_validation_panel->update();

	rename_group_dialog->reset_size();
	rename_group_dialog->popup_centered();
	rename_group->select_all();
	rename_group->grab_focus();
}

LineEdit *GroupSettingsEditor::get_name_box() const {
	return group_name;
}

GroupSettingsEditor::GroupSettingsEditor() {
	ProjectSettings::get_singleton()->add_hidden_prefix("global_group/");

	HBoxContainer *hbc = memnew(HBoxContainer);
	add_child(hbc);

	Label *l = memnew(Label);
	l->set_text(TTR("Name:"));
	hbc->add_child(l);

	group_name = memnew(LineEdit);
	group_name->set_h_size_flags(SIZE_EXPAND_FILL);
	group_name->set_clear_button_enabled(true);
	group_name->connect(SceneStringName(text_changed), callable_mp(this, &GroupSettingsEditor::_group_name_text_changed));
	group_name->connect(SceneStringName(text_submitted), callable_mp(this, &GroupSettingsEditor::_text_submitted));
	hbc->add_child(group_name);

	l = memnew(Label);
	l->set_text(TTR("Description:"));
	hbc->add_child(l);

	group_description = memnew(LineEdit);
	group_description->set_clear_button_enabled(true);
	group_description->set_h_size_flags(SIZE_EXPAND_FILL);
	group_description->connect(SceneStringName(text_submitted), callable_mp(this, &GroupSettingsEditor::_text_submitted));
	hbc->add_child(group_description);

	add_button = memnew(Button);
	add_button->set_text(TTR("Add"));
	add_button->set_disabled(true);
	add_button->connect(SceneStringName(pressed), callable_mp(this, &GroupSettingsEditor::_add_group));
	hbc->add_child(add_button);

	tree = memnew(Tree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_SINGLE);
	tree->set_allow_reselect(true);

	tree->set_columns(3);
	tree->set_column_titles_visible(true);

	tree->set_column_title(0, TTR("Name"));
	tree->set_column_title(1, TTR("Description"));
	tree->set_column_expand(2, false);

	tree->connect("item_edited", callable_mp(this, &GroupSettingsEditor::_item_edited));
	tree->connect("item_activated", callable_mp(this, &GroupSettingsEditor::_show_rename_dialog));
	tree->connect("button_clicked", callable_mp(this, &GroupSettingsEditor::_item_button_pressed));
	tree->set_v_size_flags(SIZE_EXPAND_FILL);

	add_child(tree, true);

	message = memnew(AcceptDialog);
	add_child(message);
}
