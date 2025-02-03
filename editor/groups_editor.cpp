/**************************************************************************/
/*  groups_editor.cpp                                                     */
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

#include "groups_editor.h"

#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/project_settings_editor.h"
#include "editor/scene_tree_dock.h"
#include "editor/scene_tree_editor.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/resources/packed_scene.h"

static bool can_edit(Node *p_node, const String &p_group) {
	Node *n = p_node;
	bool can_edit = true;
	while (n) {
		Ref<SceneState> ss = (n == EditorNode::get_singleton()->get_edited_scene()) ? n->get_scene_inherited_state() : n->get_scene_instance_state();
		if (ss.is_valid()) {
			int path = ss->find_node_by_path(n->get_path_to(p_node));
			if (path != -1) {
				if (ss->is_node_in_group(path, p_group)) {
					can_edit = false;
					break;
				}
			}
		}
		n = n->get_owner();
	}
	return can_edit;
}

struct _GroupInfoComparator {
	bool operator()(const Node::GroupInfo &p_a, const Node::GroupInfo &p_b) const {
		return p_a.name.operator String() < p_b.name.operator String();
	}
};

void GroupsEditor::_add_group(const String &p_name, const String &p_description, bool p_global) {
	if (!node) {
		return;
	}

	if (p_global) {
		ProjectSettingsEditor::get_singleton()->get_group_settings()->add_group(p_name, p_description);
	} else {
		scene_groups[p_name] = true;
	}
}

void GroupsEditor::_remove_group(const String &p_name) {
	if (!node) {
		return;
	}

	if (global_groups.has(p_name)) {
		ProjectSettingsEditor::get_singleton()->get_group_settings()->remove_group(p_name, true);
	} else {
		scene_groups.erase(p_name);
		_remove_node_references(scene_root_node, p_name);
	}
}

void GroupsEditor::_rename_group(const String &p_old_name, const String &p_new_name) {
	if (!node) {
		return;
	}

	if (global_groups.has(p_old_name)) {
		ProjectSettingsEditor::get_singleton()->get_group_settings()->rename_group(p_old_name, p_new_name);
	} else {
		scene_groups[p_new_name] = scene_groups[p_old_name];
		scene_groups.erase(p_old_name);
		_rename_node_references(scene_root_node, p_old_name, p_new_name);
	}
}

void GroupsEditor::_remove_node_references(Node *p_node, const String &p_name) {
	bool is_editable = can_edit(p_node, p_name);

	if (is_editable && p_node->is_in_group(p_name)) {
		p_node->remove_from_group(p_name);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_remove_node_references(p_node->get_child(i), p_name);
	}
}

void GroupsEditor::_rename_node_references(Node *p_node, const String &p_old_name, const String &p_new_name) {
	if (p_node->is_in_group(p_old_name)) {
		p_node->remove_from_group(p_old_name);
		p_node->add_to_group(p_new_name, true);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_rename_node_references(p_node->get_child(i), p_old_name, p_new_name);
	}
}

String GroupsEditor::_check_new_group_name(const String &p_name) {
	if (p_name.empty()) {
		return TTR("Group can't be empty.");
	}

	if (_has_group(p_name)) {
		return TTR("Group already exists.");
	}

	return "";
}

bool GroupsEditor::_has_group(const String &p_name) {
	return global_groups.has(p_name) || scene_groups.has(p_name);
}

void GroupsEditor::_modify_group(Object *p_item, int p_column, int p_id) {
	if (!node) {
		return;
	}

	TreeItem *ti = Object::cast_to<TreeItem>(p_item);
	if (!ti) {
		return;
	}

	switch (p_id) {
		case COPY_GROUP: {
			OS::get_singleton()->set_clipboard(ti->get_text(p_column));
		} break;
	}
}

void GroupsEditor::_filter_changed(const String &p_new_text) {
	_update_tree();
}

void GroupsEditor::_load_scene_groups(Node *p_node) {
	List<Node::GroupInfo> groups;
	p_node->get_groups(&groups);

	for (List<GroupInfo>::Element *E = groups.front(); E; E = E->next()) {
		GroupInfo &gi = E->get();
		if (!gi.persistent) {
			continue;
		}

		if (global_groups.has(gi.name)) {
			continue;
		}

		bool is_editable = can_edit(p_node, gi.name);
		if (scene_groups.has(gi.name)) {
			scene_groups[gi.name] = scene_groups[gi.name] && is_editable;
		} else {
			scene_groups[gi.name] = is_editable;
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_load_scene_groups(p_node->get_child(i));
	}
}

void GroupsEditor::_update_groups() {
	if (updating_groups) {
		return;
	}

	updating_groups = true;

	global_groups = ProjectSettingsEditor::get_singleton()->get_group_settings()->get_groups();

	_load_scene_groups(scene_root_node);

	List<StringName> keys;
	scene_groups.get_key_list(&keys);

	for (List<StringName>::Element *E = keys.front(); E; E = E->next()) {
		if (global_groups.has(E->get())) {
			scene_groups.erase(E->get());
		}
	}

	updating_groups = false;
}

void GroupsEditor::_update_tree() {
	if (!node) {
		return;
	}

	if (updating_tree) {
		return;
	}

	updating_tree = true;

	tree->clear();

	List<Node::GroupInfo> groups;
	node->get_groups(&groups);
	groups.sort_custom<_GroupInfoComparator>();

	List<StringName> current_groups;
	for (List<Node::GroupInfo>::Element *E = groups.front(); E; E = E->next()) {
		current_groups.push_back(E->get().name);
	}

	TreeItem *root = tree->create_item();

	TreeItem *local_root = tree->create_item(root);
	local_root->set_text(0, "Scene Groups");
	local_root->set_icon(0, get_icon("PackedScene", "EditorIcons"));
	local_root->set_custom_bg_color(0, get_color("prop_subsection", "Editor"));
	local_root->set_selectable(0, false);

	List<StringName> scene_keys;
	scene_groups.get_key_list(&scene_keys);
	scene_keys.sort_custom<NoCaseComparator>();

	for (List<StringName>::Element *E = scene_keys.front(); E; E = E->next()) {
		if (!filter->get_text().is_subsequence_of(E->get())) {
			continue;
		}

		bool is_editable = can_edit(node, E->get());

		TreeItem *item = tree->create_item(local_root);
		item->set_cell_mode(0, is_editable ? TreeItem::CELL_MODE_CHECK : TreeItem::CELL_MODE_STRING);
		item->set_editable(0, is_editable);
		item->set_checked(0, current_groups.find(E->get()) != nullptr);
		item->set_text(0, E->get());
		item->set_meta("__local", true);
		item->set_meta("__name", E->get());
		item->set_meta("__description", "");
		if (!is_editable) {
			item->set_icon(0, get_icon("GuiCheckedDisabled", "EditorIcons"));
		}
		if (!scene_groups[E->get()]) {
			item->add_button(0, get_icon("Lock", "EditorIcons"), -1, true, TTR("This group can't be edited outside a packed scene."));
		}
		item->add_button(0, get_icon("ActionCopy", "EditorIcons"), COPY_GROUP);
	}

	List<StringName> keys;
	global_groups.get_key_list(&keys);
	keys.sort_custom<NoCaseComparator>();

	TreeItem *global_root = tree->create_item(root);
	global_root->set_text(0, "Global Groups");
	global_root->set_icon(0, get_icon("Environment", "EditorIcons"));
	global_root->set_custom_bg_color(0, get_color("prop_subsection", "Editor"));
	global_root->set_selectable(0, false);

	for (List<StringName>::Element *E = keys.front(); E; E = E->next()) {
		if (!filter->get_text().is_subsequence_of(E->get())) {
			continue;
		}
		TreeItem *item = tree->create_item(global_root);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_editable(0, true);
		item->set_checked(0, current_groups.find(E->get()) != nullptr);
		item->set_text(0, E->get());
		item->set_meta("__local", false);
		item->set_meta("__name", E->get());
		item->set_meta("__description", global_groups[E->get()]);
		if (!global_groups[E->get()].empty()) {
			item->set_tooltip(0, vformat("%s\nDescription: %s", E->get(), global_groups[E->get()]));
		}
		item->add_button(0, get_icon("ActionCopy", "EditorIcons"), COPY_GROUP);
	}

	updating_tree = false;
}

void GroupsEditor::_global_group_changed() {
	_update_groups();
	call_deferred("_update_tree");
}

void GroupsEditor::set_undo_redo(UndoRedo *p_undo_redo) {
	undo_redo = p_undo_redo;
}

void GroupsEditor::_update_scene_groups(Node *p_node) {
	if (scene_groups_cache.has(p_node)) {
		scene_groups = scene_groups_cache[p_node];
		scene_groups_cache.erase(p_node);
	} else {
		scene_groups = HashMap<StringName, bool>();
	}
}

void GroupsEditor::_cache_scene_groups(Node *p_node) {
	const int edited_scene_count = EditorNode::get_editor_data().get_edited_scene_count();
	for (int i = 0; i < edited_scene_count; i++) {
		if (p_node == EditorNode::get_editor_data().get_edited_scene_root(i)) {
			scene_groups_cache[p_node] = scene_groups_for_caching;
			break;
		}
	}
}

void GroupsEditor::set_current(Node *p_node) {
	node = p_node;

	if (!node) {
		return;
	}

	if (scene_tree->get_edited_scene_root() != scene_root_node) {
		scene_root_node = scene_tree->get_edited_scene_root();
		_update_scene_groups(scene_root_node);
		_update_groups();
	}

	_update_tree();
}

void GroupsEditor::_item_edited() {
	TreeItem *ti = tree->get_edited();
	if (!ti) {
		return;
	}

	if (ti->is_checked(0)) {
		undo_redo->create_action(TTR("Add to Group"));

		undo_redo->add_do_method(node, "add_to_group", ti->get_text(0), true);
		undo_redo->add_undo_method(node, "remove_from_group", ti->get_text(0));

		undo_redo->add_do_method(this, "_set_group_checked", ti->get_text(0), true);
		undo_redo->add_undo_method(this, "_set_group_checked", ti->get_text(0), false);

		// To force redraw of scene tree.
		undo_redo->add_do_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");
		undo_redo->add_undo_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");

		undo_redo->commit_action();

	} else {
		undo_redo->create_action(TTR("Remove from Group"));

		undo_redo->add_do_method(node, "remove_from_group", ti->get_text(0));
		undo_redo->add_undo_method(node, "add_to_group", ti->get_text(0), true);

		undo_redo->add_do_method(this, "_set_group_checked", ti->get_text(0), false);
		undo_redo->add_undo_method(this, "_set_group_checked", ti->get_text(0), true);

		// To force redraw of scene tree.
		undo_redo->add_do_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");
		undo_redo->add_undo_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");

		undo_redo->commit_action();
	}
}

void GroupsEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			filter->set_right_icon(get_icon("Search", "EditorIcons"));
			add->set_icon(get_icon("Add", "EditorIcons"));
		} break;
	}
}

void GroupsEditor::_menu_id_pressed(int p_id) {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	bool is_local = ti->get_meta("__local");

	switch (p_id) {
		case DELETE_GROUP: {
			_show_remove_group_dialog();
		} break;
		case RENAME_GROUP: {
			if (!is_local || scene_groups[ti->get_meta("__name")]) {
				_show_rename_group_dialog();
			}
		} break;
		case CONVERT_GROUP: {
			String name = ti->get_meta("__name");
			String description = ti->get_meta("__description");

			if (is_local) {
				undo_redo->create_action(TTR("Convert to Global Group"));
				undo_redo->add_do_method(ProjectSettingsEditor::get_singleton()->get_group_settings(), "add_group", name, "");
				undo_redo->add_undo_method(ProjectSettingsEditor::get_singleton()->get_group_settings(), "remove_group", name, false);

				undo_redo->add_undo_method(this, "_add_group", name, "", false);

				undo_redo->add_do_method(this, "_update_groups");
				undo_redo->add_undo_method(this, "_update_groups");

				undo_redo->add_do_method(this, "_update_tree");
				undo_redo->add_undo_method(this, "_update_tree");

				undo_redo->commit_action();
			} else {
				undo_redo->create_action(TTR("Convert to Scene Group"));
				undo_redo->add_do_method(ProjectSettingsEditor::get_singleton()->get_group_settings(), "remove_group", name, false);
				undo_redo->add_undo_method(ProjectSettingsEditor::get_singleton()->get_group_settings(), "add_group", name, description);

				undo_redo->add_do_method(this, "_add_group", name, "", false);

				undo_redo->add_do_method(this, "_update_groups");
				undo_redo->add_undo_method(this, "_update_groups");

				undo_redo->add_do_method(this, "_update_tree");
				undo_redo->add_undo_method(this, "_update_tree");

				undo_redo->commit_action();
			}
		} break;
	}
}

void GroupsEditor::_item_rmb_selected(const Vector2 &p_pos) {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	menu->clear();
	if (ti->get_meta("__local")) {
		menu->add_icon_item(get_icon("Environment", "EditorIcons"), TTR("Convert to Global Group"), CONVERT_GROUP);
	} else {
		menu->add_icon_item(get_icon("PackedScene", "EditorIcons"), TTR("Convert to Scene Group"), CONVERT_GROUP);
	}

	menu->add_separator();
	menu->add_icon_shortcut(get_icon("Rename", "EditorIcons"), ED_GET_SHORTCUT("groups_editor/rename"), RENAME_GROUP);
	menu->add_icon_shortcut(get_icon("Remove", "EditorIcons"), ED_GET_SHORTCUT("groups_editor/delete"), DELETE_GROUP);

	menu->set_position(tree->get_global_position() + p_pos);
	menu->popup();
}

void GroupsEditor::_confirm_add() {
	String name = add_group_name->get_text().strip_edges();

	String error = _check_new_group_name(name);
	if (!error.empty()) {
		return;
	}

	String description = add_group_description->get_text().strip_edges();

	undo_redo->create_action(TTR("Add to Group"));

	undo_redo->add_do_method(node, "add_to_group", name, true);
	undo_redo->add_undo_method(node, "remove_from_group", name);

	undo_redo->add_do_method(this, "_add_group", name, description, global_group_button->is_pressed());
	undo_redo->add_undo_method(this, "_remove_group", name);

	undo_redo->add_do_method(this, "_update_tree");
	undo_redo->add_undo_method(this, "_update_tree");

	// To force redraw of scene tree.
	undo_redo->add_do_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");
	undo_redo->add_undo_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");

	undo_redo->commit_action();
	tree->grab_focus();
}

void GroupsEditor::_confirm_rename() {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	String old_name = ti->get_meta("__name");
	String new_name = rename_group->get_text().strip_edges();

	if (old_name == new_name) {
		return;
	}

	undo_redo->create_action(TTR("Rename Group"));

	undo_redo->add_do_method(this, "_rename_group", old_name, new_name);
	undo_redo->add_undo_method(this, "_rename_group", new_name, old_name);

	undo_redo->add_do_method(this, "_update_tree");
	undo_redo->add_undo_method(this, "_update_tree");

	undo_redo->commit_action();

	tree->grab_focus();
}

void GroupsEditor::_confirm_delete() {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	String name = ti->get_meta("__name");
	String description = ti->get_meta("__description");
	bool local = ti->get_meta("__local");

	undo_redo->create_action(TTR("Remove Group"));

	undo_redo->add_do_method(this, "_remove_group", name);
	undo_redo->add_undo_method(this, "_add_group", name, description, !local);

	undo_redo->add_do_method(this, "_update_groups");
	undo_redo->add_undo_method(this, "_update_groups");

	undo_redo->add_do_method(this, "call_deferred", "_update_tree");
	undo_redo->add_undo_method(this, "call_deferred", "_update_tree");

	undo_redo->commit_action();
	tree->grab_focus();
}

void GroupsEditor::_show_add_group_dialog() {
	if (!add_group_dialog) {
		add_group_dialog = memnew(ConfirmationDialog);
		add_group_dialog->set_title(TTR("Create New Group"));
		add_group_dialog->connect("confirmed", this, "_confirm_add");

		VBoxContainer *vbc = memnew(VBoxContainer);
		add_group_dialog->add_child(vbc);

		GridContainer *gc = memnew(GridContainer);
		gc->set_columns(2);
		vbc->add_child(gc);

		Label *label_name = memnew(Label(TTR("Name:")));
		label_name->set_h_size_flags(0); // Size flags: shrink begin
		gc->add_child(label_name);

		HBoxContainer *hbc = memnew(HBoxContainer);
		hbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		gc->add_child(hbc);

		add_group_name = memnew(LineEdit);
		add_group_name->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
		add_group_name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		add_group_name->connect("text_changed", this, "_check_add");
		hbc->add_child(add_group_name);

		global_group_button = memnew(CheckButton);
		global_group_button->set_text(TTR("Global"));
		hbc->add_child(global_group_button);

		Label *label_description = memnew(Label(TTR("Description:")));
		label_name->set_h_size_flags(0); // Size flags: shrink begin
		gc->add_child(label_description);

		add_group_description = memnew(LineEdit);
		add_group_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		add_group_description->set_editable(false);
		gc->add_child(add_group_description);

		global_group_button->connect("toggled", add_group_description, "set_editable");

		add_error = memnew(Label);
		vbc->add_child(add_error);

		add_child(add_group_dialog);
	}
	add_group_name->clear();
	add_group_description->clear();

	global_group_button->set_pressed(false);

	_check_add("");

	add_group_dialog->popup_centered();
	add_group_name->grab_focus();
}

void GroupsEditor::_show_rename_group_dialog() {
	if (!rename_group_dialog) {
		rename_group_dialog = memnew(ConfirmationDialog);
		rename_group_dialog->set_title(TTR("Rename Group"));

		VBoxContainer *vbc = memnew(VBoxContainer);
		rename_group_dialog->add_child(vbc);

		HBoxContainer *hbc = memnew(HBoxContainer);
		hbc->add_child(memnew(Label(TTR("Name:"))));

		rename_group = memnew(LineEdit);
		rename_group->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
		rename_group->connect("text_changed", this, "_check_rename");
		hbc->add_child(rename_group);

		rename_error = memnew(Label);

		vbc->add_child(hbc);
		vbc->add_child(rename_error);

		rename_group_dialog->connect("confirmed", this, "_confirm_rename");
		add_child(rename_group_dialog);
	}

	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	String name = ti->get_meta("__name");

	rename_group->set_text(name);
	rename_group_dialog->set_meta("__name", name);

	_check_rename(name);

	rename_group_dialog->popup_centered();
	rename_group->select_all();
	rename_group->grab_focus();
}

void GroupsEditor::_show_remove_group_dialog() {
	if (!remove_group_dialog) {
		remove_group_dialog = memnew(ConfirmationDialog);
		remove_group_dialog->set_text(TTR("Delete the group and all its references"));
		remove_group_dialog->connect("confirmed", this, "_confirm_delete");
		add_child(remove_group_dialog);
	}

	remove_group_dialog->popup_centered();
}

void GroupsEditor::_check_add(const String &p_new_text) {
	String error = _check_new_group_name(p_new_text.strip_edges());

	if (!error.empty()) {
		add_error->add_color_override("font_color", get_color("error_color", "Editor"));
		add_error->set_text(error);
		add_group_dialog->get_ok()->set_disabled(true);
	} else {
		add_error->add_color_override("font_color", get_color("success_color", "Editor"));
		add_error->set_text(TTR("Group name is valid."));
		add_group_dialog->get_ok()->set_disabled(false);
	}
}

void GroupsEditor::_check_rename(const String &p_new_text) {
	String error = "";

	String old_name = rename_group_dialog->get_meta("__name");
	if (p_new_text != old_name) {
		error = _check_new_group_name(p_new_text.strip_edges());
	}

	if (!error.empty()) {
		rename_error->add_color_override("font_color", get_color("error_color", "Editor"));
		rename_error->set_text(error);
		rename_group_dialog->get_ok()->set_disabled(true);
	} else {
		rename_error->add_color_override("font_color", get_color("success_color", "Editor"));
		rename_error->set_text(TTR("Group name is valid."));
		rename_group_dialog->get_ok()->set_disabled(false);
	}
}

void GroupsEditor::_set_group_checked(const String &p_name, bool checked) {
	TreeItem *ti = tree->get_item_with_text(p_name);
	if (!ti) {
		return;
	}

	ti->set_checked(0, checked);
}

void GroupsEditor::_groups_gui_input(Ref<InputEvent> p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (ED_IS_SHORTCUT("groups_editor/delete", p_event)) {
			_menu_id_pressed(DELETE_GROUP);
		} else if (ED_IS_SHORTCUT("groups_editor/rename", p_event)) {
			_menu_id_pressed(RENAME_GROUP);
		} else {
			return;
		}

		accept_event();
	}
}

void GroupsEditor::_bind_methods() {
	ClassDB::bind_method("_update_tree", &GroupsEditor::_update_tree);
	ClassDB::bind_method("_update_groups", &GroupsEditor::_update_groups);

	ClassDB::bind_method("_add_group", &GroupsEditor::_add_group);
	ClassDB::bind_method("_rename_group", &GroupsEditor::_rename_group);
	ClassDB::bind_method("_remove_group", &GroupsEditor::_remove_group);
	ClassDB::bind_method("_set_group_checked", &GroupsEditor::_set_group_checked);

	ClassDB::bind_method("_remove_node_references", &GroupsEditor::_remove_node_references);
	ClassDB::bind_method("_rename_node_references", &GroupsEditor::_rename_node_references);

	ClassDB::bind_method("_node_removed", &GroupsEditor::_node_removed);
	ClassDB::bind_method("_confirm_add", &GroupsEditor::_confirm_add);
	ClassDB::bind_method("_confirm_rename", &GroupsEditor::_confirm_rename);
	ClassDB::bind_method("_confirm_delete", &GroupsEditor::_confirm_delete);
	ClassDB::bind_method("_check_add", &GroupsEditor::_check_add);
	ClassDB::bind_method("_check_rename", &GroupsEditor::_check_rename);
	ClassDB::bind_method("_filter_changed", &GroupsEditor::_filter_changed);
	ClassDB::bind_method("_modify_group", &GroupsEditor::_modify_group);
	ClassDB::bind_method("_item_edited", &GroupsEditor::_item_edited);
	ClassDB::bind_method("_item_rmb_selected", &GroupsEditor::_item_rmb_selected);
	ClassDB::bind_method("_groups_gui_input", &GroupsEditor::_groups_gui_input);
	ClassDB::bind_method("_menu_id_pressed", &GroupsEditor::_menu_id_pressed);
	ClassDB::bind_method("_show_add_group_dialog", &GroupsEditor::_show_add_group_dialog);
	ClassDB::bind_method("_load_scene_groups", &GroupsEditor::_load_scene_groups);
	ClassDB::bind_method("_global_group_changed", &GroupsEditor::_global_group_changed);
	ClassDB::bind_method("_cache_scene_groups", &GroupsEditor::_cache_scene_groups);
}

void GroupsEditor::_node_removed(Node *p_node) {
	if (scene_root_node == p_node) {
		scene_groups_for_caching = scene_groups;
		call_deferred("_cache_scene_groups", p_node);
		scene_root_node = nullptr;
	}

	if (scene_root_node && scene_root_node == p_node->get_owner()) {
		_global_group_changed();
	}
}

GroupsEditor::GroupsEditor() {
	node = nullptr;
	scene_tree = SceneTree::get_singleton();

	ED_SHORTCUT("groups_editor/delete", TTR("Delete"), KEY_DELETE);
#ifdef OSX_ENABLED
	ED_SHORTCUT("groups_editor/rename", TTR("Rename"), KEY_ENTER);
#else
	ED_SHORTCUT("groups_editor/rename", TTR("Rename"), KEY_F2);
#endif

	HBoxContainer *hbc = memnew(HBoxContainer);
	add_child(hbc);

	filter = memnew(LineEdit);
	filter->set_clear_button_enabled(true);
	filter->set_placeholder(TTR("Filter groups"));
	filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	filter->connect("text_changed", this, "_filter_changed");
	hbc->add_child(filter);

	add = memnew(Button);
	add->set_flat(true);
	add->set_tooltip(TTR("Add a new group."));
	add->connect("pressed", this, "_show_add_group_dialog");
	hbc->add_child(add);

	tree = memnew(Tree);
	tree->set_hide_root(true);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_allow_rmb_select(true);
	tree->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);
	tree->set_select_mode(Tree::SelectMode::SELECT_SINGLE);
	tree->connect("button_pressed", this, "_modify_group");
	tree->connect("item_edited", this, "_item_edited");
	tree->connect("item_rmb_selected", this, "_item_rmb_selected");
	tree->connect("gui_input", this, "_groups_gui_input");
	add_child(tree);

	menu = memnew(PopupMenu);
	menu->connect("id_pressed", this, "_menu_id_pressed");
	tree->add_child(menu);

	_filter_changed("");

	SceneTree::get_singleton()->connect("node_added", this, "_load_scene_groups");
	SceneTree::get_singleton()->connect("node_removed", this, "_node_removed");

	ProjectSettingsEditor::get_singleton()->get_group_settings()->connect("group_changed", this, "_global_group_changed");
}

GroupsEditor::~GroupsEditor() {
}
