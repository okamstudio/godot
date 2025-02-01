/**************************************************************************/
/*  groups_editor.h                                                       */
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

#pragma once

#include "scene/gui/dialogs.h"

class Button;
class CheckBox;
class CheckButton;
class EditorValidationPanel;
class Label;
class LineEdit;
class PopupMenu;
class Tree;
class TreeItem;

class GroupsEditor : public VBoxContainer {
	GDCLASS(GroupsEditor, VBoxContainer);

	const String GLOBAL_GROUP_PREFIX = "global_group/";

	bool updating_tree = false;
	bool updating_groups = false;
	bool groups_dirty = false;
	bool update_groups_and_tree_queued = false;

	Node *node = nullptr;
	Node *scene_root_node = nullptr;
	SceneTree *scene_tree = nullptr;

	ConfirmationDialog *add_group_dialog = nullptr;
	LineEdit *add_group_name = nullptr;
	LineEdit *add_group_description = nullptr;
	CheckButton *global_group_button = nullptr;
	EditorValidationPanel *add_validation_panel = nullptr;

	ConfirmationDialog *rename_group_dialog = nullptr;
	LineEdit *rename_group = nullptr;
	CheckBox *rename_check_box = nullptr;
	EditorValidationPanel *rename_validation_panel = nullptr;

	ConfirmationDialog *remove_group_dialog = nullptr;
	CheckBox *remove_check_box = nullptr;
	Label *remove_label = nullptr;

	PopupMenu *menu = nullptr;

	LineEdit *filter = nullptr;
	Button *add = nullptr;
	Tree *tree = nullptr;

	HashMap<ObjectID, HashMap<StringName, bool>> scene_groups_cache;
	HashMap<StringName, bool> scene_groups_for_caching;

	HashMap<StringName, bool> scene_groups;
	HashMap<StringName, String> global_groups;

	void _update_scene_groups(const ObjectID &p_id);
	void _cache_scene_groups(const ObjectID &p_id);

	void _show_add_group_dialog();
	void _show_rename_group_dialog();
	void _show_remove_group_dialog();

	void _check_add();
	void _check_rename();
	void _validate_name(const String &p_name, EditorValidationPanel *p_validation_panel);

	void _update_tree();

	void _update_groups();
	void _load_scene_groups(Node *p_node);

	void _add_scene_group(const String &p_name);
	void _rename_scene_group(const String &p_old_name, const String &p_new_name);
	void _remove_scene_group(const String &p_name);

	bool _has_group(const String &p_name);
	void _set_group_checked(const String &p_name, bool p_checked);

	void _confirm_add();
	void _confirm_rename();
	void _confirm_delete();

	void _item_edited();
	void _item_mouse_selected(const Vector2 &p_pos, MouseButton p_mouse_button);
	void _modify_group(Object *p_item, int p_column, int p_id, MouseButton p_mouse_button);
	void _menu_id_pressed(int p_id);

	void _update_groups_and_tree();
	void _queue_update_groups_and_tree();

	void _groups_gui_input(Ref<InputEvent> p_event);

	void _node_removed(Node *p_node);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	enum ModifyButton {
		DELETE_GROUP,
		COPY_GROUP,
		RENAME_GROUP,
		CONVERT_GROUP,
	};

	void set_current(Node *p_node);

	GroupsEditor();
	~GroupsEditor();
};
