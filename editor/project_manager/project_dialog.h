/**************************************************************************/
/*  project_dialog.h                                                      */
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
class EditorFileDialog;
class LineEdit;
class OptionButton;
class TextureRect;

class ProjectDialog : public ConfirmationDialog {
	GDCLASS(ProjectDialog, ConfirmationDialog);

public:
	enum Mode {
		MODE_NEW,
		MODE_IMPORT,
		MODE_INSTALL,
		MODE_RENAME,
	};

private:
	enum MessageType {
		MESSAGE_ERROR,
		MESSAGE_WARNING,
		MESSAGE_SUCCESS,
	};

	enum InputType {
		PROJECT_PATH,
		INSTALL_PATH,
	};

	Mode mode = MODE_NEW;
	bool is_folder_empty = true;
	ConfirmationDialog *nonempty_confirmation = nullptr;

	CheckButton *create_dir = nullptr;
	Button *project_browse = nullptr;
	Button *install_browse = nullptr;
	VBoxContainer *name_container = nullptr;
	VBoxContainer *project_path_container = nullptr;
	VBoxContainer *install_path_container = nullptr;

	VBoxContainer *renderer_container = nullptr;
	Label *renderer_info = nullptr;
	HBoxContainer *default_files_container = nullptr;
	Ref<ButtonGroup> renderer_button_group;
	bool rendering_device_supported = false;
	Label *rd_not_supported = nullptr;

	Label *msg = nullptr;
	LineEdit *project_name = nullptr;
	LineEdit *project_path = nullptr;
	LineEdit *install_path = nullptr;
	TextureRect *project_status_rect = nullptr;
	TextureRect *install_status_rect = nullptr;

	OptionButton *vcs_metadata_selection = nullptr;

	CheckBox *edit_check_box = nullptr;

	EditorFileDialog *fdialog_project = nullptr;
	EditorFileDialog *fdialog_install = nullptr;
	AcceptDialog *dialog_error = nullptr;

	String zip_path;
	String zip_title;

	void _set_message(const String &p_msg, MessageType p_type, InputType input_type = PROJECT_PATH);
	void _validate_path();

	// Project path for MODE_NEW and MODE_INSTALL. Install path for MODE_IMPORT.
	// Install path is only visible when importing a ZIP.
	String _get_target_path();
	void _set_target_path(const String &p_text);

	// Calculated from project name / ZIP name.
	String auto_dir;

	// Updates `auto_dir`. If the target path dir name is equal to `auto_dir` (the default state), the target path is also updated.
	void _update_target_auto_dir();

	// While `create_dir` is disabled, stores the last target path dir name, or an empty string if equal to `auto_dir`.
	String last_custom_target_dir;
	void _create_dir_toggled(bool p_pressed);

	void _project_name_changed();
	void _project_path_changed();
	void _install_path_changed();

	void _browse_project_path();
	void _browse_install_path();

	void _project_path_selected(const String &p_path);
	void _install_path_selected(const String &p_path);

	void _reset_name();
	void _renderer_selected();
	void _nonempty_confirmation_ok_pressed();

	void ok_pressed() override;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_mode(Mode p_mode);
	void set_project_name(const String &p_name);
	void set_project_path(const String &p_path);
	void set_zip_path(const String &p_path);
	void set_zip_title(const String &p_title);

	void ask_for_path_and_show();
	void show_dialog(bool p_reset_name = true);

	ProjectDialog();
};
