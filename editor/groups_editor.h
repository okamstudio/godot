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

#ifndef GROUPS_EDITOR_H
#define GROUPS_EDITOR_H

#include "editor/scene_tree_editor.h"
#include "scene/gui/button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/popup.h"
#include "scene/gui/tree.h"

class GroupsEditor : public VBoxContainer {
	GDCLASS(GroupsEditor, VBoxContainer);

	bool updating_tree = false;
	bool updating_groups = false;

	Node *node = nullptr;
	Node *scene_root_node = nullptr;
	SceneTree *scene_tree = nullptr;

	ConfirmationDialog *add_group_dialog = nullptr;
	ConfirmationDialog *rename_group_dialog = nullptr;
	ConfirmationDialog *remove_group_dialog = nullptr;

	LineEdit *rename_group = nullptr;
	Label *rename_error = nullptr;

	LineEdit *add_group_name = nullptr;
	LineEdit *add_group_description = nullptr;
	Label *add_error = nullptr;
	CheckButton *global_group_button = nullptr;

	PopupMenu *menu = nullptr;

	LineEdit *filter = nullptr;
	Button *add = nullptr;
	Tree *tree = nullptr;

	UndoRedo *undo_redo;

	HashMap<Node *, HashMap<StringName, bool>> scene_groups_cache;
	HashMap<StringName, bool> scene_groups_for_caching;

	HashMap<StringName, bool> scene_groups;
	HashMap<StringName, String> global_groups;

	void _update_scene_groups(Node *p_node);
	void _cache_scene_groups(Node *p_node);

	void _show_add_group_dialog();
	void _show_rename_group_dialog();
	void _show_remove_group_dialog();

	void _check_add(const String &p_new_text);
	void _check_rename(const String &p_new_text);

	void _update_tree();

	void _update_groups();
	void _load_scene_groups(Node *p_node);

	String _check_new_group_name(const String &p_name);

	void _add_group(const String &p_name, const String &p_description, bool p_global = false);
	bool _has_group(const String &p_name);
	void _remove_group(const String &p_name);
	void _rename_group(const String &p_old_name, const String &p_new_name);
	void _set_group_checked(const String &p_name, bool checked);

	void _remove_node_references(Node *p_node, const String &p_name);
	void _rename_node_references(Node *p_node, const String &p_old_name, const String &p_new_name);

	void _confirm_add();
	void _confirm_rename();
	void _confirm_delete();

	void _item_edited();
	void _item_rmb_selected(const Vector2 &p_pos);
	void _modify_group(Object *p_item, int p_column, int p_id);
	void _menu_id_pressed(int p_id);

	void _filter_changed(const String &p_new_text);
	void _global_group_changed();

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

	void set_undo_redo(UndoRedo *p_undo_redo);
	void set_current(Node *p_node);

	GroupsEditor();
	~GroupsEditor();
};

#endif // GROUPS_EDITOR_H
