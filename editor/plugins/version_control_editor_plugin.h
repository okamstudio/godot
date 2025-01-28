/**************************************************************************/
/*  version_control_editor_plugin.h                                       */
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

#ifndef VERSION_CONTROL_EDITOR_PLUGIN_H
#define VERSION_CONTROL_EDITOR_PLUGIN_H

#include "editor/editor_vcs_interface.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/check_button.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"

class VersionControlEditorPlugin : public EditorPlugin {
	GDCLASS(VersionControlEditorPlugin, EditorPlugin)

public:
	enum ButtonType {
		BUTTON_TYPE_OPEN = 0,
		BUTTON_TYPE_DISCARD = 1,
	};

	enum DiffViewType {
		DIFF_VIEW_TYPE_SPLIT = 0,
		DIFF_VIEW_TYPE_UNIFIED = 1,
	};

	enum ExtraOption {
		EXTRA_OPTION_FORCE_PUSH,
		EXTRA_OPTION_CREATE_BRANCH,
		EXTRA_OPTION_CREATE_REMOTE,
	};

private:
	static VersionControlEditorPlugin *singleton;

	List<StringName> available_plugins;

	PopupMenu *version_control_actions = nullptr;
	ConfirmationDialog *metadata_dialog = nullptr;
	OptionButton *metadata_selection = nullptr;
	AcceptDialog *set_up_dialog = nullptr;
	CheckButton *toggle_vcs_choice = nullptr;
	OptionButton *set_up_choice = nullptr;
	VBoxContainer *set_up_vbc = nullptr;
	VBoxContainer *set_up_settings_vbc = nullptr;
	LineEdit *set_up_username = nullptr;
	LineEdit *set_up_password = nullptr;
	LineEdit *set_up_ssh_public_key_path = nullptr;
	LineEdit *set_up_ssh_private_key_path = nullptr;
	LineEdit *set_up_ssh_passphrase = nullptr;
	FileDialog *set_up_ssh_public_key_file_dialog = nullptr;
	FileDialog *set_up_ssh_private_key_file_dialog = nullptr;
	Label *set_up_warning_text = nullptr;

	AcceptDialog *discard_all_confirm = nullptr;

	OptionButton *commit_list_size_button = nullptr;

	AcceptDialog *branch_create_confirm = nullptr;
	LineEdit *branch_create_name_input = nullptr;
	Button *branch_create_ok = nullptr;

	AcceptDialog *remote_create_confirm = nullptr;
	LineEdit *remote_create_name_input = nullptr;
	LineEdit *remote_create_url_input = nullptr;
	Button *remote_create_ok = nullptr;

	HashMap<EditorVCSInterface::ChangeType, String> change_type_to_strings;
	HashMap<EditorVCSInterface::ChangeType, Color> change_type_to_color;
	HashMap<EditorVCSInterface::ChangeType, Ref<Texture>> change_type_to_icon;

	VBoxContainer *version_commit_dock = nullptr;
	Tree *staged_files = nullptr;
	Tree *unstaged_files = nullptr;
	Tree *commit_list = nullptr;

	OptionButton *branch_select = nullptr;
	Button *branch_remove_button = nullptr;
	AcceptDialog *branch_remove_confirm = nullptr;

	Button *fetch_button = nullptr;
	Button *pull_button = nullptr;
	Button *push_button = nullptr;
	OptionButton *remote_select = nullptr;
	Button *remote_remove_button = nullptr;
	AcceptDialog *remote_remove_confirm = nullptr;
	MenuButton *extra_options = nullptr;
	PopupMenu *extra_options_remove_branch_list = nullptr;
	PopupMenu *extra_options_remove_remote_list = nullptr;

	String branch_to_remove;
	String remote_to_remove;

	Button *stage_all_button = nullptr;
	Button *unstage_all_button = nullptr;
	Button *discard_all_button = nullptr;
	Button *refresh_button = nullptr;
	TextEdit *commit_message = nullptr;
	Button *commit_button = nullptr;

	VBoxContainer *version_control_dock = nullptr;
	Button *version_control_dock_button = nullptr;
	Label *diff_title = nullptr;
	RichTextLabel *diff = nullptr;
	OptionButton *diff_view_type_select = nullptr;
	bool show_commit_diff_header = false;
	List<EditorVCSInterface::DiffFile> diff_content;

	void _notification(int p_what);
	void _initialize_vcs();
	void _set_vcs_ui_state(bool p_enabled);
	void _set_credentials();
	void _ssh_public_key_selected(const String &p_path);
	void _ssh_private_key_selected(const String &p_path);
	void _populate_available_vcs_names();
	void _update_remotes_list();
	void _update_set_up_warning(const String &p_new_text);
	void _update_opened_tabs();
	void _update_extra_options();

	bool _load_plugin(const String &p_name);

	void _pull();
	void _push();
	void _force_push();
	void _fetch();
	void _commit();
	void _confirm_discard_all();
	void _discard_all();
	void _refresh_stage_area();
	void _refresh_branch_list();
	void _set_commit_list_size(int p_index);
	void _refresh_commit_list();
	void _refresh_remote_list();
	void _display_diff(int p_idx);
	void _move_all(Object *p_tree);
	void _load_diff(Object *p_tree);
	void _clear_diff();
	int _get_item_count(Tree *p_tree);
	void _item_activated(Object *p_tree);
	void _create_branch();
	void _create_remote();
	void _update_branch_create_button(const String &p_new_text);
	void _update_remote_create_button(const String &p_new_text);
	void _branch_item_selected(int p_index);
	void _remote_selected(int p_index);
	void _remove_branch();
	void _remove_remote();
	void _popup_branch_remove_confirm(int p_index);
	void _popup_remote_remove_confirm(int p_index);
	void _move_item(Tree *p_tree, TreeItem *p_itme);
	void _display_diff_split_view(List<EditorVCSInterface::DiffLine> &p_diff_content);
	void _display_diff_unified_view(List<EditorVCSInterface::DiffLine> &p_diff_content);
	void _discard_file(const String &p_file_path, EditorVCSInterface::ChangeType p_change);
	void _cell_button_pressed(Object *p_item, int p_column, int p_id, int p_mouse_button_index);
	void _add_new_item(Tree *p_tree, const String &p_file_path, EditorVCSInterface::ChangeType p_change);
	void _update_commit_button();
	void _commit_message_gui_input(const Ref<InputEvent> &p_event);
	void _extra_option_selected(int p_index);
	bool _is_staging_area_empty();
	String _get_date_string_from(int64_t p_unix_timestamp, int64_t p_offset_minutes) const;
	void _create_vcs_metadata_files();
	void _popup_file_dialog(const Variant &p_file_dialog_variant);
	void _toggle_vcs_integration(bool p_toggled);

	friend class EditorVCSInterface;

protected:
	static void _bind_methods();

public:
	static VersionControlEditorPlugin *get_singleton();

	void popup_vcs_metadata_dialog();
	void popup_vcs_set_up_dialog(const Control *p_gui_base);

	PopupMenu *get_version_control_actions_panel() const { return version_control_actions; }

	void register_editor();
	void fetch_available_vcs_plugin_names();
	void shut_down();

	VersionControlEditorPlugin();
	~VersionControlEditorPlugin();
};

#endif // VERSION_CONTROL_EDITOR_PLUGIN_H
