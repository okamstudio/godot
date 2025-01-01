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

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/project_settings_editor.h"
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_button.h"
#include "scene/gui/grid_container.h"
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

void GroupsEditor::_add_scene_group(const String &p_name) {
	scene_groups[p_name] = true;
}

void GroupsEditor::_remove_scene_group(const String &p_name) {
	scene_groups.erase(p_name);
	ProjectSettingsEditor::get_singleton()->get_group_settings()->remove_node_references(scene_root_node, p_name);
}

void GroupsEditor::_rename_scene_group(const String &p_old_name, const String &p_new_name) {
	scene_groups[p_new_name] = scene_groups[p_old_name];
	scene_groups.erase(p_old_name);
	ProjectSettingsEditor::get_singleton()->get_group_settings()->rename_node_references(scene_root_node, p_old_name, p_new_name);
}

void GroupsEditor::_set_group_checked(const String &p_name, bool p_checked) {
	TreeItem *ti = tree->get_item_with_text(p_name);
	if (!ti) {
		return;
	}

	ti->set_checked(0, p_checked);
}

bool GroupsEditor::_has_group(const String &p_name) {
	return global_groups.has(p_name) || scene_groups.has(p_name);
}

void GroupsEditor::_modify_group(Object *p_item, int p_column, int p_id, MouseButton p_mouse_button) {
	if (p_mouse_button != MouseButton::LEFT) {
		return;
	}

	if (!node) {
		return;
	}

	TreeItem *ti = Object::cast_to<TreeItem>(p_item);
	if (!ti) {
		return;
	}

	if (p_id == COPY_GROUP) {
		DisplayServer::get_singleton()->clipboard_set(ti->get_text(p_column));
	}
}

void GroupsEditor::_load_scene_groups(Node *p_node) {
	List<Node::GroupInfo> groups;
	p_node->get_groups(&groups);

	for (const GroupInfo &gi : groups) {
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
	if (!is_visible_in_tree()) {
		groups_dirty = true;
		return;
	}

	if (updating_groups) {
		return;
	}

	updating_groups = true;

	global_groups = ProjectSettings::get_singleton()->get_global_groups_list();

	_load_scene_groups(scene_root_node);

	for (HashMap<StringName, bool>::Iterator E = scene_groups.begin(); E;) {
		HashMap<StringName, bool>::Iterator next = E;
		++next;

		if (global_groups.has(E->key)) {
			scene_groups.erase(E->key);
		}
		E = next;
	}

	updating_groups = false;
}

void GroupsEditor::_update_tree() {
	if (!is_visible_in_tree()) {
		groups_dirty = true;
		return;
	}

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
	for (const Node::GroupInfo &gi : groups) {
		current_groups.push_back(gi.name);
	}

	TreeItem *root = tree->create_item();

	TreeItem *local_root = tree->create_item(root);
	local_root->set_text(0, TTR("Scene Groups"));
	local_root->set_icon(0, get_editor_theme_icon(SNAME("PackedScene")));
	local_root->set_custom_bg_color(0, get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));
	local_root->set_selectable(0, false);

	List<StringName> scene_keys;
	for (const KeyValue<StringName, bool> &E : scene_groups) {
		scene_keys.push_back(E.key);
	}
	scene_keys.sort_custom<NoCaseComparator>();

	for (const StringName &E : scene_keys) {
		if (!filter->get_text().is_subsequence_ofn(E)) {
			continue;
		}

		TreeItem *item = tree->create_item(local_root);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_editable(0, can_edit(node, E));
		item->set_checked(0, current_groups.find(E) != nullptr);
		item->set_text(0, E);
		item->set_meta("__local", true);
		item->set_meta("__name", E);
		item->set_meta("__description", "");
		if (!scene_groups[E]) {
			item->add_button(0, get_editor_theme_icon(SNAME("Lock")), -1, true, TTR("This group belongs to another scene and can't be edited."));
		}
		item->add_button(0, get_editor_theme_icon(SNAME("ActionCopy")), COPY_GROUP, false, TTR("Copy group name to clipboard."));
	}

	List<StringName> keys;
	for (const KeyValue<StringName, String> &E : global_groups) {
		keys.push_back(E.key);
	}
	keys.sort_custom<NoCaseComparator>();

	TreeItem *global_root = tree->create_item(root);
	global_root->set_text(0, TTR("Global Groups"));
	global_root->set_icon(0, get_editor_theme_icon(SNAME("Environment")));
	global_root->set_custom_bg_color(0, get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));
	global_root->set_selectable(0, false);

	for (const StringName &E : keys) {
		if (!filter->get_text().is_subsequence_ofn(E)) {
			continue;
		}

		TreeItem *item = tree->create_item(global_root);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_editable(0, can_edit(node, E));
		item->set_checked(0, current_groups.find(E) != nullptr);
		item->set_text(0, E);
		item->set_meta("__local", false);
		item->set_meta("__name", E);
		item->set_meta("__description", global_groups[E]);
		if (!global_groups[E].is_empty()) {
			item->set_tooltip_text(0, vformat("%s\n\n%s", E, global_groups[E]));
		}
		item->add_button(0, get_editor_theme_icon(SNAME("ActionCopy")), COPY_GROUP, false, TTR("Copy group name to clipboard."));
	}

	updating_tree = false;
}

void GroupsEditor::_queue_update_groups_and_tree() {
	if (update_groups_and_tree_queued) {
		return;
	}
	update_groups_and_tree_queued = true;
	callable_mp(this, &GroupsEditor::_update_groups_and_tree).call_deferred();
}

void GroupsEditor::_update_groups_and_tree() {
	update_groups_and_tree_queued = false;
	// The scene_root_node could be unset before we actually run this code because this is queued with call_deferred().
	// In that case NOTIFICATION_VISIBILITY_CHANGED will call this function again soon.
	if (!scene_root_node) {
		return;
	}
	_update_groups();
	_update_tree();
}

void GroupsEditor::_update_scene_groups(const ObjectID &p_id) {
	HashMap<ObjectID, HashMap<StringName, bool>>::Iterator I = scene_groups_cache.find(p_id);
	if (I) {
		scene_groups = I->value;
		scene_groups_cache.remove(I);
	} else {
		scene_groups = HashMap<StringName, bool>();
	}
}

void GroupsEditor::_cache_scene_groups(const ObjectID &p_id) {
	const int edited_scene_count = EditorNode::get_editor_data().get_edited_scene_count();
	for (int i = 0; i < edited_scene_count; i++) {
		Node *edited_scene_root = EditorNode::get_editor_data().get_edited_scene_root(i);
		if (edited_scene_root && p_id == edited_scene_root->get_instance_id()) {
			scene_groups_cache[p_id] = scene_groups_for_caching;
			break;
		}
	}
}

void GroupsEditor::set_current(Node *p_node) {
	if (node == p_node) {
		return;
	}
	node = p_node;

	if (!node) {
		return;
	}

	if (scene_tree->get_edited_scene_root() != scene_root_node) {
		scene_root_node = scene_tree->get_edited_scene_root();
		_update_scene_groups(scene_root_node->get_instance_id());
		_update_groups();
	}

	_update_tree();
}

void GroupsEditor::_item_edited() {
	TreeItem *ti = tree->get_edited();
	if (!ti) {
		return;
	}

	String name = ti->get_text(0);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (ti->is_checked(0)) {
		undo_redo->create_action(TTR("Add to Group"));

		undo_redo->add_do_method(node, "add_to_group", name, true);
		undo_redo->add_undo_method(node, "remove_from_group", name);

		undo_redo->add_do_method(this, "_set_group_checked", name, true);
		undo_redo->add_undo_method(this, "_set_group_checked", name, false);

		// To force redraw of scene tree.
		undo_redo->add_do_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");
		undo_redo->add_undo_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");

		undo_redo->commit_action();

	} else {
		undo_redo->create_action(TTR("Remove from Group"));

		undo_redo->add_do_method(node, "remove_from_group", name);
		undo_redo->add_undo_method(node, "add_to_group", name, true);

		undo_redo->add_do_method(this, "_set_group_checked", name, false);
		undo_redo->add_undo_method(this, "_set_group_checked", name, true);

		// To force redraw of scene tree.
		undo_redo->add_do_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");
		undo_redo->add_undo_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");

		undo_redo->commit_action();
	}
}

void GroupsEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			get_tree()->connect("node_added", callable_mp(this, &GroupsEditor::_load_scene_groups));
			get_tree()->connect("node_removed", callable_mp(this, &GroupsEditor::_node_removed));
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			filter->set_right_icon(get_editor_theme_icon("Search"));
			add->set_button_icon(get_editor_theme_icon("Add"));
			_update_tree();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (groups_dirty && is_visible_in_tree()) {
				groups_dirty = false;
				_update_groups_and_tree();
			}
		} break;
	}
}

void GroupsEditor::_menu_id_pressed(int p_id) {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	bool is_local = ti->get_meta("__local");
	String group_name = ti->get_meta("__name");

	switch (p_id) {
		case DELETE_GROUP: {
			if (!is_local || scene_groups[group_name]) {
				_show_remove_group_dialog();
			}
		} break;
		case RENAME_GROUP: {
			if (!is_local || scene_groups[group_name]) {
				_show_rename_group_dialog();
			}
		} break;
		case CONVERT_GROUP: {
			String description = ti->get_meta("__description");
			String property_name = GLOBAL_GROUP_PREFIX + group_name;

			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			if (is_local) {
				undo_redo->create_action(TTR("Convert to Global Group"));

				undo_redo->add_do_property(ProjectSettings::get_singleton(), property_name, "");
				undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_name, Variant());

				undo_redo->add_do_method(ProjectSettings::get_singleton(), "save");
				undo_redo->add_undo_method(ProjectSettings::get_singleton(), "save");

				undo_redo->add_undo_method(this, "_add_scene_group", group_name);

				undo_redo->add_do_method(this, "_update_groups_and_tree");
				undo_redo->add_undo_method(this, "_update_groups_and_tree");

				undo_redo->commit_action();
			} else {
				undo_redo->create_action(TTR("Convert to Scene Group"));

				undo_redo->add_do_property(ProjectSettings::get_singleton(), property_name, Variant());
				undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_name, description);

				undo_redo->add_do_method(ProjectSettings::get_singleton(), "save");
				undo_redo->add_undo_method(ProjectSettings::get_singleton(), "save");

				undo_redo->add_do_method(this, "_add_scene_group", group_name);

				undo_redo->add_do_method(this, "_update_groups_and_tree");
				undo_redo->add_undo_method(this, "_update_groups_and_tree");

				undo_redo->commit_action();
			}
		} break;
	}
}

void GroupsEditor::_item_mouse_selected(const Vector2 &p_pos, MouseButton p_mouse_button) {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	if (p_mouse_button == MouseButton::LEFT) {
		callable_mp(this, &GroupsEditor::_item_edited).call_deferred();
	} else if (p_mouse_button == MouseButton::RIGHT) {
		// Restore the previous state after clicking RMB.
		if (ti->is_editable(0)) {
			ti->set_checked(0, !ti->is_checked(0));
		}

		menu->clear();
		if (ti->get_meta("__local")) {
			menu->add_icon_item(get_editor_theme_icon(SNAME("Environment")), TTR("Convert to Global Group"), CONVERT_GROUP);
		} else {
			menu->add_icon_item(get_editor_theme_icon(SNAME("PackedScene")), TTR("Convert to Scene Group"), CONVERT_GROUP);
		}

		String group_name = ti->get_meta("__name");
		if (global_groups.has(group_name) || scene_groups[group_name]) {
			menu->add_separator();
			menu->add_icon_shortcut(get_editor_theme_icon(SNAME("Rename")), ED_GET_SHORTCUT("groups_editor/rename"), RENAME_GROUP);
			menu->add_icon_shortcut(get_editor_theme_icon(SNAME("Remove")), ED_GET_SHORTCUT("groups_editor/delete"), DELETE_GROUP);
		}

		menu->set_position(tree->get_screen_position() + p_pos);
		menu->reset_size();
		menu->popup();
	}
}

void GroupsEditor::_confirm_add() {
	String name = add_group_name->get_text().strip_edges();

	String description = add_group_description->get_text();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add to Group"));

	undo_redo->add_do_method(node, "add_to_group", name, true);
	undo_redo->add_undo_method(node, "remove_from_group", name);

	bool is_local = !global_group_button->is_pressed();
	if (is_local) {
		undo_redo->add_do_method(this, "_add_scene_group", name);
		undo_redo->add_undo_method(this, "_remove_scene_group", name);
	} else {
		String property_name = GLOBAL_GROUP_PREFIX + name;

		undo_redo->add_do_property(ProjectSettings::get_singleton(), property_name, description);
		undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_name, Variant());

		undo_redo->add_do_method(ProjectSettings::get_singleton(), "save");
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "save");

		undo_redo->add_do_method(this, "_update_groups");
		undo_redo->add_undo_method(this, "_update_groups");
	}

	undo_redo->add_do_method(this, "_update_tree");
	undo_redo->add_undo_method(this, "_update_tree");

	// To force redraw of scene tree.
	undo_redo->add_do_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");
	undo_redo->add_undo_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");

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

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Rename Group"));

	if (!global_groups.has(old_name)) {
		undo_redo->add_do_method(this, "_rename_scene_group", old_name, new_name);
		undo_redo->add_undo_method(this, "_rename_scene_group", new_name, old_name);
	} else {
		String property_new_name = GLOBAL_GROUP_PREFIX + new_name;
		String property_old_name = GLOBAL_GROUP_PREFIX + old_name;

		String description = ti->get_meta("__description");

		undo_redo->add_do_property(ProjectSettings::get_singleton(), property_new_name, description);
		undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_new_name, Variant());

		undo_redo->add_do_property(ProjectSettings::get_singleton(), property_old_name, Variant());
		undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_old_name, description);

		if (rename_check_box->is_pressed()) {
			undo_redo->add_do_method(ProjectSettingsEditor::get_singleton()->get_group_settings(), "rename_references", old_name, new_name);
			undo_redo->add_undo_method(ProjectSettingsEditor::get_singleton()->get_group_settings(), "rename_references", new_name, old_name);
		}

		undo_redo->add_do_method(ProjectSettings::get_singleton(), "save");
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "save");

		undo_redo->add_do_method(this, "_update_groups");
		undo_redo->add_undo_method(this, "_update_groups");
	}

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
	bool is_local = ti->get_meta("__local");

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove Group"));

	if (is_local) {
		undo_redo->add_do_method(this, "_remove_scene_group", name);
		undo_redo->add_undo_method(this, "_add_scene_group", name);
	} else {
		String property_name = GLOBAL_GROUP_PREFIX + name;
		String description = ti->get_meta("__description");

		undo_redo->add_do_property(ProjectSettings::get_singleton(), property_name, Variant());
		undo_redo->add_undo_property(ProjectSettings::get_singleton(), property_name, description);

		if (remove_check_box->is_pressed()) {
			undo_redo->add_do_method(ProjectSettingsEditor::get_singleton()->get_group_settings(), "remove_references", name);
		}

		undo_redo->add_do_method(ProjectSettings::get_singleton(), "save");
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "save");

		undo_redo->add_do_method(this, "_update_groups");
		undo_redo->add_undo_method(this, "_update_groups");
	}

	undo_redo->add_do_method(this, "_update_tree");
	undo_redo->add_undo_method(this, "_update_tree");

	undo_redo->commit_action();
	tree->grab_focus();
}

void GroupsEditor::_show_add_group_dialog() {
	if (!add_group_dialog) {
		add_group_dialog = memnew(ConfirmationDialog);
		add_group_dialog->set_title(TTR("Create New Group"));
		add_group_dialog->connect(SceneStringName(confirmed), callable_mp(this, &GroupsEditor::_confirm_add));

		VBoxContainer *vbc = memnew(VBoxContainer);
		add_group_dialog->add_child(vbc);

		GridContainer *gc = memnew(GridContainer);
		gc->set_columns(2);
		vbc->add_child(gc);

		Label *label_name = memnew(Label(TTR("Name:")));
		label_name->set_h_size_flags(SIZE_SHRINK_BEGIN);
		gc->add_child(label_name);

		HBoxContainer *hbc = memnew(HBoxContainer);
		hbc->set_h_size_flags(SIZE_EXPAND_FILL);
		gc->add_child(hbc);

		add_group_name = memnew(LineEdit);
		add_group_name->set_custom_minimum_size(Size2(200 * EDSCALE, 0));
		add_group_name->set_h_size_flags(SIZE_EXPAND_FILL);
		hbc->add_child(add_group_name);

		global_group_button = memnew(CheckButton);
		global_group_button->set_text(TTR("Global"));
		hbc->add_child(global_group_button);

		Label *label_description = memnew(Label(TTR("Description:")));
		label_name->set_h_size_flags(SIZE_SHRINK_BEGIN);
		gc->add_child(label_description);

		add_group_description = memnew(LineEdit);
		add_group_description->set_h_size_flags(SIZE_EXPAND_FILL);
		add_group_description->set_editable(false);
		gc->add_child(add_group_description);

		global_group_button->connect(SceneStringName(toggled), callable_mp(add_group_description, &LineEdit::set_editable));

		add_group_dialog->register_text_enter(add_group_name);
		add_group_dialog->register_text_enter(add_group_description);

		add_validation_panel = memnew(EditorValidationPanel);
		add_validation_panel->add_line(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Group name is valid."));
		add_validation_panel->set_update_callback(callable_mp(this, &GroupsEditor::_check_add));
		add_validation_panel->set_accept_button(add_group_dialog->get_ok_button());

		add_group_name->connect(SceneStringName(text_changed), callable_mp(add_validation_panel, &EditorValidationPanel::update).unbind(1));

		vbc->add_child(add_validation_panel);

		add_child(add_group_dialog);
	}
	add_group_name->clear();
	add_group_description->clear();

	global_group_button->set_pressed(false);

	add_validation_panel->update();

	add_group_dialog->popup_centered();
	add_group_name->grab_focus();
}

void GroupsEditor::_show_rename_group_dialog() {
	if (!rename_group_dialog) {
		rename_group_dialog = memnew(ConfirmationDialog);
		rename_group_dialog->set_title(TTR("Rename Group"));
		rename_group_dialog->connect(SceneStringName(confirmed), callable_mp(this, &GroupsEditor::_confirm_rename));

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
		rename_validation_panel->set_update_callback(callable_mp(this, &GroupsEditor::_check_rename));
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

	bool is_global = !ti->get_meta("__local");
	rename_check_box->set_visible(is_global);
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

void GroupsEditor::_show_remove_group_dialog() {
	if (!remove_group_dialog) {
		remove_group_dialog = memnew(ConfirmationDialog);
		remove_group_dialog->connect(SceneStringName(confirmed), callable_mp(this, &GroupsEditor::_confirm_delete));

		VBoxContainer *vbox = memnew(VBoxContainer);
		remove_label = memnew(Label);
		vbox->add_child(remove_label);

		remove_check_box = memnew(CheckBox);
		remove_check_box->set_text(TTR("Delete references from all scenes"));
		vbox->add_child(remove_check_box);

		remove_group_dialog->add_child(vbox);

		add_child(remove_group_dialog);
	}

	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}

	bool is_global = !ti->get_meta("__local");
	remove_check_box->set_visible(is_global);
	remove_check_box->set_pressed(false);
	remove_label->set_text(vformat(TTR("Delete group \"%s\" and all its references?"), ti->get_text(0)));

	remove_group_dialog->reset_size();
	remove_group_dialog->popup_centered();
}

void GroupsEditor::_check_add() {
	String group_name = add_group_name->get_text().strip_edges();
	_validate_name(group_name, add_validation_panel);
}

void GroupsEditor::_check_rename() {
	String group_name = rename_group->get_text().strip_edges();
	String old_name = rename_group_dialog->get_meta("__name");

	if (group_name == old_name) {
		return;
	}
	_validate_name(group_name, rename_validation_panel);
}

void GroupsEditor::_validate_name(const String &p_name, EditorValidationPanel *p_validation_panel) {
	if (p_name.is_empty()) {
		p_validation_panel->set_message(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Group can't be empty."), EditorValidationPanel::MSG_ERROR);
	} else if (_has_group(p_name)) {
		p_validation_panel->set_message(EditorValidationPanel::MSG_ID_DEFAULT, TTR("Group already exists."), EditorValidationPanel::MSG_ERROR);
	}
}

void GroupsEditor::_groups_gui_input(Ref<InputEvent> p_event) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (ED_IS_SHORTCUT("groups_editor/delete", p_event)) {
			_menu_id_pressed(DELETE_GROUP);
		} else if (ED_IS_SHORTCUT("groups_editor/rename", p_event)) {
			_menu_id_pressed(RENAME_GROUP);
		} else if (ED_IS_SHORTCUT("editor/open_search", p_event)) {
			filter->grab_focus();
			filter->select_all();
		} else {
			return;
		}

		accept_event();
	}
}

void GroupsEditor::_bind_methods() {
	ClassDB::bind_method("_update_tree", &GroupsEditor::_update_tree);
	ClassDB::bind_method("_update_groups", &GroupsEditor::_update_groups);
	ClassDB::bind_method("_update_groups_and_tree", &GroupsEditor::_update_groups_and_tree);

	ClassDB::bind_method("_add_scene_group", &GroupsEditor::_add_scene_group);
	ClassDB::bind_method("_rename_scene_group", &GroupsEditor::_rename_scene_group);
	ClassDB::bind_method("_remove_scene_group", &GroupsEditor::_remove_scene_group);
	ClassDB::bind_method("_set_group_checked", &GroupsEditor::_set_group_checked);
}

void GroupsEditor::_node_removed(Node *p_node) {
	if (scene_root_node == p_node) {
		scene_groups_for_caching = scene_groups;
		callable_mp(this, &GroupsEditor::_cache_scene_groups).call_deferred(p_node->get_instance_id());
		scene_root_node = nullptr;
	}

	if (scene_root_node && scene_root_node == p_node->get_owner()) {
		_queue_update_groups_and_tree();
	}
}

GroupsEditor::GroupsEditor() {
	node = nullptr;
	scene_tree = SceneTree::get_singleton();

	ED_SHORTCUT("groups_editor/delete", TTRC("Delete"), Key::KEY_DELETE);
	ED_SHORTCUT("groups_editor/rename", TTRC("Rename"), Key::F2);
	ED_SHORTCUT_OVERRIDE("groups_editor/rename", "macos", Key::ENTER);

	HBoxContainer *hbc = memnew(HBoxContainer);
	add_child(hbc);

	add = memnew(Button);
	add->set_theme_type_variation("FlatMenuButton");
	add->set_tooltip_text(TTR("Add a new group."));
	add->connect(SceneStringName(pressed), callable_mp(this, &GroupsEditor::_show_add_group_dialog));
	hbc->add_child(add);

	filter = memnew(LineEdit);
	filter->set_clear_button_enabled(true);
	filter->set_placeholder(TTR("Filter Groups"));
	filter->set_h_size_flags(SIZE_EXPAND_FILL);
	filter->connect(SceneStringName(text_changed), callable_mp(this, &GroupsEditor::_update_tree).unbind(1));
	hbc->add_child(filter);

	tree = memnew(Tree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_hide_root(true);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_allow_rmb_select(true);
	tree->set_select_mode(Tree::SelectMode::SELECT_SINGLE);
	tree->connect("button_clicked", callable_mp(this, &GroupsEditor::_modify_group));
	tree->connect("item_mouse_selected", callable_mp(this, &GroupsEditor::_item_mouse_selected));
	tree->connect(SceneStringName(gui_input), callable_mp(this, &GroupsEditor::_groups_gui_input));
	add_child(tree);

	menu = memnew(PopupMenu);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &GroupsEditor::_menu_id_pressed));
	tree->add_child(menu);

	ProjectSettingsEditor::get_singleton()->get_group_settings()->connect("group_changed", callable_mp(this, &GroupsEditor::_update_groups_and_tree));
}

GroupsEditor::~GroupsEditor() {
}
