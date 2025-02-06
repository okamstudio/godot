/**************************************************************************/
/*  script_create_dialog.h                                                */
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

#include "core/object/script_language.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"

class CreateDialog;
class EditorFileDialog;
class EditorValidationPanel;
class LineEdit;

class ScriptCreateDialog : public ConfirmationDialog {
	GDCLASS(ScriptCreateDialog, ConfirmationDialog);

	enum {
		MSG_ID_SCRIPT,
		MSG_ID_PATH,
		MSG_ID_BUILT_IN,
		MSG_ID_TEMPLATE,
	};

	EditorValidationPanel *validation_panel = nullptr;
	LineEdit *parent_name = nullptr;
	Button *parent_browse_button = nullptr;
	Button *parent_search_button = nullptr;
	OptionButton *language_menu = nullptr;
	OptionButton *template_menu = nullptr;
	LineEdit *file_path = nullptr;
	LineEdit *built_in_name = nullptr;
	Button *path_button = nullptr;
	EditorFileDialog *file_browse = nullptr;
	CheckBox *built_in = nullptr;
	CheckBox *use_templates = nullptr;
	VBoxContainer *path_vb = nullptr;
	AcceptDialog *alert = nullptr;
	CreateDialog *select_class = nullptr;

	bool is_browsing_parent = false;
	String path_error;
	String template_inactive_message;
	bool is_new_script_created = true;
	bool is_path_valid = false;
	bool supports_built_in = false;
	bool can_inherit_from_file = false;
	bool is_parent_name_valid = false;
	bool is_class_name_valid = false;
	bool is_built_in = false;
	bool is_using_templates = true;
	bool built_in_enabled = true;
	bool load_enabled = true;
	int default_language;
	bool re_check_path = false;

	Control *path_controls[2];
	Control *name_controls[2];

	Vector<ScriptLanguage::ScriptTemplate> template_list;
	ScriptLanguage *language = nullptr;

	String base_type;

	void _path_hbox_sorted();
	bool _can_be_built_in();
	void _path_changed(const String &p_path = String());
	void _language_changed(int l = 0);
	void _built_in_pressed();
	void _use_template_pressed();
	bool _validate_parent(const String &p_string);
	String _validate_path(const String &p_path, bool p_file_must_exist);
	void _parent_name_changed(const String &p_parent);
	void _template_changed(int p_template = 0);
	void _browse_path(bool browse_parent, bool p_save);
	void _file_selected(const String &p_file);
	void _create();
	void _browse_class_in_tree();
	virtual void ok_pressed() override;
	void _create_new();
	void _load_exist();
	void _update_template_menu();
	void _update_dialog();
	ScriptLanguage::ScriptTemplate _get_current_template() const;
	Vector<ScriptLanguage::ScriptTemplate> _get_user_templates(const ScriptLanguage *p_language, const StringName &p_object, const String &p_dir, const ScriptLanguage::TemplateLocation &p_origin) const;
	ScriptLanguage::ScriptTemplate _parse_template(const ScriptLanguage *p_language, const String &p_path, const String &p_filename, const ScriptLanguage::TemplateLocation &p_origin, const String &p_inherits) const;
	String _get_script_origin_label(const ScriptLanguage::TemplateLocation &p_origin) const;
	String _adjust_file_path(const String &p_base_path) const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void config(const String &p_base_name, const String &p_base_path, bool p_built_in_enabled = true, bool p_load_enabled = true);
	void set_inheritance_base_type(const String &p_base);
	ScriptCreateDialog();
};
