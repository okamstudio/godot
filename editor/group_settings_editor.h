/*************************************************************************/
/*  group_settings_editor.h                                              */
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

#ifndef GROUP_SETTINGS_EDITOR_H
#define GROUP_SETTINGS_EDITOR_H

#include "core/hash_map.h"
#include "editor_file_system.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tree.h"

class GroupSettingsEditor : public VBoxContainer {
	GDCLASS(GroupSettingsEditor, VBoxContainer);

	String group_changed = "group_changed";

	HashMap<StringName, String> _groups_cache;

	bool updating_group = false;

	AcceptDialog *message = nullptr;
	Tree *tree = nullptr;
	LineEdit *group_name = nullptr;
	LineEdit *group_description = nullptr;
	Button *add_button = nullptr;

	ConfirmationDialog *remove_dialog = nullptr;
	CheckBox *remove_check_box = nullptr;

	void _show_remove_dialog();

	String _check_new_group_name(const String &p_name);

	void _add_group(const String &p_name, const String &p_description);

	bool _has_group(const String &p_name) const;
	void _create_group(const String &p_name, const String &p_description);
	void _delete_group(const String &p_name);
	void _rename_group(const String &p_old_name, const String &p_new_name);
	void _set_description(const String &p_name, const String &p_description);

	void _save_groups();
	void _load_groups();

	void _get_node(const String &p_name);
	void _get_all_scenes(EditorFileSystemDirectory *p_dir, Set<String> &r_list);

	void _remove_references(const String &p_name);
	void _rename_references(const String &p_old_name, const String &p_new_name);

	void _remove_node_references(Node *p_node, const String &p_name);
	void _rename_node_references(Node *p_node, const String &p_old_name, const String &p_new_name);

	void _confirm_delete();

	void _add_button_pressed();
	void _group_name_text_changed(const String &p_name);

	void _item_edited();
	void _item_button_pressed(Object *p_item, int p_column, int p_button);

	void _changed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void show_message(const String &p_message);
	HashMap<StringName, String> get_groups() const;

	void add_group(const String &p_name, const String &p_description);
	void rename_group(const String &p_old_name, const String &p_new_name);
	void remove_group(const String &p_name, bool p_with_references);

	void update_tree();

	GroupSettingsEditor();
};

#endif // GROUP_SETTINGS_EDITOR_H
