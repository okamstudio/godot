/*************************************************************************/
/*  group_settings_editor.cpp                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "group_settings_editor.h"

#include "core/project_settings.h"
#include "editor/editor_scale.h"
#include "editor/scene_tree_dock.h"
#include "editor_node.h"
#include "scene/resources/packed_scene.h"

void GroupSettingsEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			update_tree();
		} break;
	}
}

HashMap<StringName, String> GroupSettingsEditor::get_groups() const {
	return _groups_cache;
}

void GroupSettingsEditor::_item_edited() {
	if (updating_group) {
		return;
	}

	TreeItem *ti = tree->get_edited();

	if (!ti) {
		return;
	}

	UndoRedo *undo_redo = EditorNode::get_undo_redo();

	if (tree->get_edited_column() == 0) {
		// Name Edited
		String new_name = ti->get_text(0);
		String old_name = ti->get_meta("__name");

		if (new_name == old_name) {
			return;
		}

		String error = _check_new_group_name(new_name);
		if (!error.empty()) {
			ti->set_text(0, old_name);
			show_message(error);
			return;
		}

		undo_redo->create_action(TTR("Rename Group"));

		undo_redo->add_do_method(this, "_rename_group", old_name, new_name);
		undo_redo->add_undo_method(this, "_rename_group", new_name, old_name);

		undo_redo->add_do_method(this, "_rename_references", old_name, new_name);
		undo_redo->add_undo_method(this, "_rename_references", new_name, old_name);

		undo_redo->add_do_method(this, "call_deferred", "update_tree");
		undo_redo->add_undo_method(this, "call_deferred", "update_tree");

		undo_redo->add_do_method(this, "emit_signal", group_changed);
		undo_redo->add_undo_method(this, "emit_signal", group_changed);

		undo_redo->add_do_method(this, "_save_groups");
		undo_redo->add_undo_method(this, "_save_groups");

		undo_redo->commit_action();
	} else if (tree->get_edited_column() == 1) {
		// Description Edited
		String name = ti->get_text(0);
		String new_description = ti->get_text(1);
		String old_description = ti->get_meta("__description");

		if (new_description == old_description) {
			return;
		}

		undo_redo->create_action(TTR("Set Group Description"));

		undo_redo->add_do_method(this, "_set_description", name, new_description);
		undo_redo->add_undo_method(this, "_set_description", name, old_description);

		undo_redo->add_do_method(this, "call_deferred", "update_tree");
		undo_redo->add_undo_method(this, "call_deferred", "update_tree");

		undo_redo->add_do_method(this, "emit_signal", group_changed);
		undo_redo->add_undo_method(this, "emit_signal", group_changed);

		undo_redo->add_do_method(this, "_save_groups");
		undo_redo->add_undo_method(this, "_save_groups");

		undo_redo->commit_action();
	}
}

void GroupSettingsEditor::_item_button_pressed(Object *p_item, int p_column, int p_button) {
	TreeItem *ti = Object::cast_to<TreeItem>(p_item);

	if (!ti) {
		return;
	}

	ti->select(0);

	_show_remove_dialog();
}

String GroupSettingsEditor::_check_new_group_name(const String &p_name) {
	if (p_name.empty()) {
		return TTR("Invalid action name. It cannot be empty.");
	}

	if (_has_group(p_name)) {
		return vformat(TTR("A group with the name '%s' already exists."), p_name);
	}

	return "";
}

bool GroupSettingsEditor::_has_group(const String &p_name) const {
	return _groups_cache.has(p_name);
}

void GroupSettingsEditor::_create_group(const String &p_name, const String &p_description) {
	_groups_cache[p_name] = p_description;
}

void GroupSettingsEditor::_delete_group(const String &p_name) {
	_groups_cache.erase(p_name);
}

void GroupSettingsEditor::_rename_group(const String &p_old_name, const String &p_new_name) {
	_groups_cache[p_new_name] = _groups_cache[p_old_name];
	_groups_cache.erase(p_old_name);
}

void GroupSettingsEditor::_set_description(const String &p_name, const String &p_description) {
	_groups_cache[p_name] = p_description;
}

void GroupSettingsEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_load_groups"), &GroupSettingsEditor::_load_groups);
	ClassDB::bind_method(D_METHOD("_save_groups"), &GroupSettingsEditor::_save_groups);

	ClassDB::bind_method(D_METHOD("_create_group"), &GroupSettingsEditor::_create_group);
	ClassDB::bind_method(D_METHOD("_delete_group"), &GroupSettingsEditor::_delete_group);
	ClassDB::bind_method(D_METHOD("_rename_group"), &GroupSettingsEditor::_rename_group);
	ClassDB::bind_method(D_METHOD("_set_description"), &GroupSettingsEditor::_set_description);
	ClassDB::bind_method(D_METHOD("_remove_references"), &GroupSettingsEditor::_remove_references);
	ClassDB::bind_method(D_METHOD("_rename_references"), &GroupSettingsEditor::_rename_references);

	ClassDB::bind_method(D_METHOD("_item_edited"), &GroupSettingsEditor::_item_edited);
	ClassDB::bind_method(D_METHOD("_item_button_pressed"), &GroupSettingsEditor::_item_button_pressed);
	ClassDB::bind_method(D_METHOD("_group_name_text_changed"), &GroupSettingsEditor::_group_name_text_changed);
	ClassDB::bind_method(D_METHOD("_add_button_pressed"), &GroupSettingsEditor::_add_button_pressed);
	ClassDB::bind_method(D_METHOD("_confirm_delete"), &GroupSettingsEditor::_confirm_delete);

	ClassDB::bind_method(D_METHOD("add_group"), &GroupSettingsEditor::add_group);
	ClassDB::bind_method(D_METHOD("rename_group"), &GroupSettingsEditor::rename_group);
	ClassDB::bind_method(D_METHOD("remove_group"), &GroupSettingsEditor::remove_group);
	ClassDB::bind_method(D_METHOD("update_tree"), &GroupSettingsEditor::update_tree);

	ADD_SIGNAL(MethodInfo("group_changed"));
}

void GroupSettingsEditor::_add_group(const String &p_name, const String &p_description) {
	String error = _check_new_group_name(p_name);
	if (!error.empty()) {
		show_message(error);
		return;
	}

	UndoRedo *undo_redo = EditorNode::get_singleton()->get_undo_redo();

	undo_redo->create_action(TTR("Add Group"));

	undo_redo->add_do_method(this, "_create_group", p_name, p_description);
	undo_redo->add_undo_method(this, "_delete_group", p_name);

	undo_redo->add_do_method(this, "call_deferred", "update_tree");
	undo_redo->add_undo_method(this, "call_deferred", "update_tree");

	undo_redo->add_do_method(this, "emit_signal", group_changed);
	undo_redo->add_undo_method(this, "emit_signal", group_changed);

	undo_redo->add_do_method(this, "_save_groups");
	undo_redo->add_undo_method(this, "_save_groups");

	undo_redo->commit_action();

	group_name->clear();
	group_description->clear();
}

void GroupSettingsEditor::_group_name_text_changed(const String &p_name) {
	String error = _check_new_group_name(p_name);
	add_button->set_tooltip(error);
	add_button->set_disabled(!error.empty());
}

void GroupSettingsEditor::_save_groups() {
	List<StringName> keys;
	_groups_cache.get_key_list(&keys);

	Dictionary d;
	for (List<StringName>::Element *E = keys.front(); E; E = E->next()) {
		d[E->get()] = _groups_cache[E->get()];
	}

	if (d.empty()) {
		if (ProjectSettings::get_singleton()->has_setting("_global_groups")) {
			ProjectSettings::get_singleton()->clear("_global_groups");
		}
	} else {
		ProjectSettings::get_singleton()->set("_global_groups", d);
	}

	ProjectSettings::get_singleton()->save();
}

void GroupSettingsEditor::_load_groups() {
	_groups_cache.clear();

	if (ProjectSettings::get_singleton()->has_setting("_global_groups")) {
		Dictionary d = ProjectSettings::get_singleton()->get("_global_groups");
		List<Variant> keys;
		d.get_key_list(&keys);

		for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
			String name = E->get().operator String();
			_groups_cache[name] = d[name];
		}
	}
}

void GroupSettingsEditor::_get_all_scenes(EditorFileSystemDirectory *p_dir, Set<String> &r_list) {
	for (int i = 0; i < p_dir->get_file_count(); i++) {
		if (p_dir->get_file_type(i) == "PackedScene")
			r_list.insert(p_dir->get_file_path(i));
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_get_all_scenes(p_dir->get_subdir(i), r_list);
	}
}

void GroupSettingsEditor::_remove_references(const String &p_name) {
	Set<String> scenes;
	_get_all_scenes(EditorFileSystem::get_singleton()->get_filesystem(), scenes);
	for (Set<String>::Element *E = scenes.front(); E; E = E->next()) {
		Ref<PackedScene> packed_scene = ResourceLoader::load(E->get());
		if (packed_scene->get_state()->remove_group_references(StringName(p_name))) {
			ResourceSaver::save(E->get(), packed_scene);
		}
	}

	// Update opened scenes
	Node *es = EditorNode::get_singleton()->get_edited_scene();
	if (es != nullptr) {
		_remove_node_references(es, p_name);
		EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor()->update_tree();
	}
}

void GroupSettingsEditor::_rename_references(const String &p_old_name, const String &p_new_name) {
	Set<String> scenes;
	_get_all_scenes(EditorFileSystem::get_singleton()->get_filesystem(), scenes);
	for (Set<String>::Element *E = scenes.front(); E; E = E->next()) {
		Ref<PackedScene> packed_scene = ResourceLoader::load(E->get());
		if (packed_scene->get_state()->rename_group_references(StringName(p_old_name), StringName(p_new_name))) {
			ResourceSaver::save(E->get(), packed_scene);
		}
	}

	// Update opened scenes
	Node *es = EditorNode::get_singleton()->get_edited_scene();
	if (es != nullptr) {
		_rename_node_references(es, p_old_name, p_new_name);
		EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor()->update_tree();
	}
}

void GroupSettingsEditor::_remove_node_references(Node *p_node, const String &p_name) {
	if (p_node->is_in_group(p_name)) {
		p_node->remove_from_group(p_name);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_remove_node_references(p_node->get_child(i), p_name);
	}
}

void GroupSettingsEditor::_rename_node_references(Node *p_node, const String &p_old_name, const String &p_new_name) {
	if (p_node->is_in_group(p_old_name)) {
		p_node->remove_from_group(p_old_name);
		p_node->add_to_group(p_new_name, true);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_rename_node_references(p_node->get_child(i), p_old_name, p_new_name);
	}
}

void GroupSettingsEditor::update_tree() {
	if (updating_group) {
		return;
	}

	updating_group = true;

	tree->clear();

	TreeItem *root = tree->create_item();

	List<StringName> keys;
	_groups_cache.get_key_list(&keys);
	keys.sort_custom<NoCaseComparator>();

	for (List<StringName>::Element *E = keys.front(); E; E = E->next()) {
		TreeItem *item = tree->create_item(root);
		item->set_meta("__name", E->get());
		item->set_meta("__description", _groups_cache[E->get()]);

		item->set_text(0, E->get());
		item->set_editable(0, true);

		item->set_text(1, _groups_cache[E->get()]);
		item->set_editable(1, true);
		item->add_button(2, get_icon("Remove", "EditorIcons"));
		item->set_selectable(2, false);
	}

	updating_group = false;
}

void GroupSettingsEditor::_confirm_delete() {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	String name = ti->get_text(0);
	String description = _groups_cache[name];

	UndoRedo *undo_redo = EditorNode::get_undo_redo();

	undo_redo->create_action(TTR("Remove Group"));

	undo_redo->add_do_method(this, "_delete_group", name);
	undo_redo->add_undo_method(this, "_create_group", name, description);

	if (remove_check_box->is_pressed()) {
		undo_redo->add_do_method(this, "_remove_references", name);
	}

	undo_redo->add_do_method(this, "call_deferred", "update_tree");
	undo_redo->add_undo_method(this, "call_deferred", "update_tree");

	undo_redo->add_do_method(this, "emit_signal", group_changed);
	undo_redo->add_undo_method(this, "emit_signal", group_changed);

	undo_redo->add_do_method(this, "_save_groups");
	undo_redo->add_undo_method(this, "_save_groups");

	undo_redo->commit_action();
}

void GroupSettingsEditor::show_message(const String &p_message) {
	message->set_text(p_message);
	message->popup_centered();
}

void GroupSettingsEditor::_add_button_pressed() {
	_add_group(group_name->get_text(), group_description->get_text());
}

void GroupSettingsEditor::add_group(const String &p_name, const String &p_description) {
	_create_group(p_name, p_description);
	_changed();
}

void GroupSettingsEditor::rename_group(const String &p_old_name, const String &p_new_name) {
	_rename_group(p_old_name, p_new_name);
	_rename_references(p_old_name, p_new_name);
	_changed();
}

void GroupSettingsEditor::remove_group(const String &p_name, bool p_with_references) {
	_delete_group(p_name);
	if (p_with_references) {
		_remove_references(p_name);
	}
	_changed();
}

void GroupSettingsEditor::_changed() {
	emit_signal(group_changed);
	call_deferred("update_tree");
	_save_groups();
}

void GroupSettingsEditor::_show_remove_dialog() {
	if (!remove_dialog) {
		remove_dialog = memnew(ConfirmationDialog);
		remove_dialog->set_text(TTR("Delete the group and its description from Global Groups?"));
		remove_dialog->get_ok()->set_text(TTR("Delete"));
		remove_dialog->set_custom_minimum_size(Size2(400, 100) * EDSCALE);
		remove_dialog->connect("confirmed", this, "_confirm_delete");

		remove_check_box = memnew(CheckBox);
		remove_check_box->set_text(TTR("Delete references from all scenes."));
		remove_dialog->add_child(remove_check_box);

		add_child(remove_dialog);
	}
	remove_check_box->set_pressed(false);
	remove_dialog->popup_centered();
}

GroupSettingsEditor::GroupSettingsEditor() {
	_load_groups();

	HBoxContainer *hbc = memnew(HBoxContainer);
	add_child(hbc);

	Label *l = memnew(Label);
	l->set_text(TTR("Name:"));
	hbc->add_child(l);

	group_name = memnew(LineEdit);
	group_name->set_h_size_flags(SIZE_EXPAND_FILL);
	group_name->set_clear_button_enabled(true);
	group_name->connect("text_changed", this, "_group_name_text_changed");
	hbc->add_child(group_name);

	l = memnew(Label);
	l->set_text(TTR("Description:"));
	hbc->add_child(l);

	group_description = memnew(LineEdit);
	group_description->set_clear_button_enabled(true);
	group_description->set_h_size_flags(SIZE_EXPAND_FILL);
	hbc->add_child(group_description);

	add_button = memnew(Button);
	add_button->set_text(TTR("Add"));
	add_button->connect("pressed", this, "_add_button_pressed");
	hbc->add_child(add_button);

	tree = memnew(Tree);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_SINGLE);
	tree->set_allow_reselect(true);

	tree->set_columns(3);
	tree->set_column_titles_visible(true);

	tree->set_column_title(0, TTR("Name"));
	tree->set_column_expand(0, true);
	tree->set_column_min_width(0, 100 * EDSCALE);

	tree->set_column_title(1, TTR("Description"));
	tree->set_column_expand(1, true);
	tree->set_column_min_width(1, 100 * EDSCALE);

	tree->set_column_expand(2, false);
	tree->set_column_min_width(2, 24 * EDSCALE);

	tree->connect("item_edited", this, "_item_edited");
	tree->connect("button_pressed", this, "_item_button_pressed");
	tree->set_v_size_flags(SIZE_EXPAND_FILL);

	add_child(tree, true);

	message = memnew(AcceptDialog);
	add_child(message);
}
