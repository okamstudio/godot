/**************************************************************************/
/*  group_settings_editor.h                                               */
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

class CheckBox;
class EditorFileSystemDirectory;
class EditorValidationPanel;
class FileSystemDock;
class Label;
class Tree;

class GroupSettingsEditor : public VBoxContainer {
	GDCLASS(GroupSettingsEditor, VBoxContainer);

	const String GLOBAL_GROUP_PREFIX = "global_group/";
	const StringName group_changed = "group_changed";

	HashMap<StringName, String> groups_cache;

	bool updating_groups = false;

	AcceptDialog *message = nullptr;
	Tree *tree = nullptr;
	LineEdit *group_name = nullptr;
	LineEdit *group_description = nullptr;
	Button *add_button = nullptr;

	ConfirmationDialog *remove_dialog = nullptr;
	CheckBox *remove_check_box = nullptr;
	Label *remove_label = nullptr;

	ConfirmationDialog *rename_group_dialog = nullptr;
	LineEdit *rename_group = nullptr;
	CheckBox *rename_check_box = nullptr;
	EditorValidationPanel *rename_validation_panel = nullptr;

	void _show_remove_dialog();
	void _show_rename_dialog();

	String _check_new_group_name(const String &p_name);
	void _check_rename();

	void _add_group();
	void _add_group(const String &p_name, const String &p_description);

	void _modify_references(const StringName &p_name, const StringName &p_new_name, bool p_is_rename);

	void _confirm_rename();
	void _confirm_delete();

	void _text_submitted(const String &p_text);
	void _group_name_text_changed(const String &p_name);

	void _item_edited();
	void _item_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	LineEdit *get_name_box() const;
	void show_message(const String &p_message);

	void remove_references(const StringName &p_name);
	void rename_references(const StringName &p_old_name, const StringName &p_new_name);

	bool remove_node_references(Node *p_node, const StringName &p_name);
	bool rename_node_references(Node *p_node, const StringName &p_old_name, const StringName &p_new_name);

	void update_groups();
	void connect_filesystem_dock_signals(FileSystemDock *p_fs_dock);

	GroupSettingsEditor();
};
