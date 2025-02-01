/**************************************************************************/
/*  project_export.h                                                      */
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

#include "editor/export/editor_export_preset.h"
#include "scene/gui/dialogs.h"

class CheckBox;
class CheckButton;
class EditorFileDialog;
class EditorFileSystemDirectory;
class EditorInspector;
class EditorPropertyPath;
class ItemList;
class LinkButton;
class MenuButton;
class OptionButton;
class PopupMenu;
class RichTextLabel;
class TabContainer;
class Tree;
class TreeItem;

class ProjectExportTextureFormatError : public HBoxContainer {
	GDCLASS(ProjectExportTextureFormatError, HBoxContainer);

	Label *texture_format_error_label = nullptr;
	LinkButton *fix_texture_format_button = nullptr;
	String setting_identifier;
	void _on_fix_texture_format_pressed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void show_for_texture_format(const String &p_friendly_name, const String &p_setting_identifier);
	ProjectExportTextureFormatError();
};

class ProjectExportDialog : public ConfirmationDialog {
	GDCLASS(ProjectExportDialog, ConfirmationDialog);

	TabContainer *sections = nullptr;

	MenuButton *add_preset = nullptr;
	Button *duplicate_preset = nullptr;
	Button *delete_preset = nullptr;
	ItemList *presets = nullptr;

	LineEdit *name = nullptr;
	EditorPropertyPath *export_path = nullptr;
	EditorInspector *parameters = nullptr;
	CheckButton *runnable = nullptr;
	CheckButton *advanced_options = nullptr;

	Button *button_export = nullptr;
	bool updating = false;

	RichTextLabel *result_dialog_log = nullptr;
	AcceptDialog *result_dialog = nullptr;
	ConfirmationDialog *delete_confirm = nullptr;

	OptionButton *export_filter = nullptr;
	LineEdit *include_filters = nullptr;
	LineEdit *exclude_filters = nullptr;
	Tree *include_files = nullptr;
	Label *server_strip_message = nullptr;
	PopupMenu *file_mode_popup = nullptr;

	Label *include_label = nullptr;
	MarginContainer *include_margin = nullptr;

	Button *export_button = nullptr;
	Button *export_all_button = nullptr;
	AcceptDialog *export_all_dialog = nullptr;

	RBSet<String> feature_set;

	Tree *patches = nullptr;
	int patch_index = -1;
	EditorFileDialog *patch_dialog = nullptr;
	ConfirmationDialog *patch_erase = nullptr;
	Button *patch_add_btn = nullptr;

	LineEdit *custom_features = nullptr;
	RichTextLabel *custom_feature_display = nullptr;

	LineEdit *script_key = nullptr;
	Label *script_key_error = nullptr;

	ProjectExportTextureFormatError *export_texture_format_error = nullptr;
	Label *export_error = nullptr;
	Label *export_warning = nullptr;
	HBoxContainer *export_templates_error = nullptr;

	String default_filename;

	bool exporting = false;

	void _advanced_options_pressed();
	void _runnable_pressed();
	void _update_parameters(const String &p_edited_property);
	void _name_changed(const String &p_string);
	void _export_path_changed(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing);
	void _add_preset(int p_platform);
	void _edit_preset(int p_index);
	void _duplicate_preset();
	void _delete_preset();
	void _delete_preset_confirm();
	void _update_export_all();

	void _force_update_current_preset_parameters();
	void _update_current_preset();
	void _update_presets();

	void _export_type_changed(int p_which);
	void _filter_changed(const String &p_filter);
	String _get_resource_export_header(EditorExportPreset::ExportFilter p_filter) const;
	void _fill_resource_tree();
	void _setup_item_for_file_mode(TreeItem *p_item, EditorExportPreset::FileExportMode p_mode);
	bool _fill_tree(EditorFileSystemDirectory *p_dir, TreeItem *p_item, Ref<EditorExportPreset> &current, EditorExportPreset::ExportFilter p_export_filter);
	void _propagate_file_export_mode(TreeItem *p_item, EditorExportPreset::FileExportMode p_inherited_export_mode);
	void _tree_changed();
	void _check_propagated_to_item(Object *p_obj, int column);
	void _tree_popup_edited(bool p_arrow_clicked);
	void _set_file_export_mode(int p_id);

	void _patch_tree_button_clicked(Object *p_item, int p_column, int p_id, int p_mouse_button_index);
	void _patch_tree_item_edited();
	void _patch_file_selected(const String &p_path);
	void _patch_delete_confirmed();
	void _patch_add_pack_pressed();

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	EditorFileDialog *export_pck_zip = nullptr;
	EditorFileDialog *export_project = nullptr;

	CheckButton *enc_pck = nullptr;
	CheckButton *enc_directory = nullptr;
	LineEdit *enc_in_filters = nullptr;
	LineEdit *enc_ex_filters = nullptr;
	LineEdit *seed_input = nullptr;

	OptionButton *script_mode = nullptr;

	void _open_export_template_manager();

	void _export_pck_zip();
	void _export_pck_zip_selected(const String &p_path);

	void _validate_export_path(const String &p_path);
	void _export_project();
	void _export_project_to_path(const String &p_path);
	void _export_all_dialog();
	void _export_all_dialog_action(const String &p_str);
	void _export_all(bool p_debug);

	void _update_feature_list();
	void _custom_features_changed(const String &p_text);

	bool updating_script_key = false;
	bool updating_enc_filters = false;
	bool updating_seed = false;
	void _enc_pck_changed(bool p_pressed);
	void _enc_directory_changed(bool p_pressed);
	void _enc_filters_changed(const String &p_text);
	void _seed_input_changed(const String &p_text);
	void _script_encryption_key_changed(const String &p_key);
	bool _validate_script_encryption_key(const String &p_key);

	void _script_export_mode_changed(int p_mode);

	void _open_key_help_link();

	void _tab_changed(int);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void popup_export();

	void set_export_path(const String &p_value);
	String get_export_path();

	Ref<EditorExportPreset> get_current_preset() const;

	bool is_exporting() const { return exporting; }

	ProjectExportDialog();
	~ProjectExportDialog();
};
