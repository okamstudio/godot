/**************************************************************************/
/*  file_dialog.h                                                         */
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

#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include "box_container.h"
#include "core/os/dir_access.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/tool_button.h"
#include "scene/gui/tree.h"

class FileDialog : public ConfirmationDialog {
	GDCLASS(FileDialog, ConfirmationDialog);

public:
	enum Access {
		ACCESS_RESOURCES,
		ACCESS_USERDATA,
		ACCESS_FILESYSTEM
	};

	enum Mode {
		MODE_OPEN_FILE,
		MODE_OPEN_FILES,
		MODE_OPEN_DIR,
		MODE_OPEN_ANY,
		MODE_SAVE_FILE
	};

	typedef Ref<Texture> (*GetIconFunc)(const String &);
	typedef void (*RegisterFunc)(FileDialog *);

	static GetIconFunc get_icon_func;
	static GetIconFunc get_large_icon_func;
	static RegisterFunc register_func;
	static RegisterFunc unregister_func;

private:
	ConfirmationDialog *makedialog;
	LineEdit *makedirname;

	Button *makedir;
	Access access;
	//Button *action;
	VBoxContainer *vbox;
	Mode mode;
	LineEdit *dir;
	HBoxContainer *drives_container;
	HBoxContainer *shortcuts_container;
	OptionButton *drives;
	Tree *tree;
	HBoxContainer *file_box;
	LineEdit *file;
	OptionButton *filter;
	AcceptDialog *mkdirerr;
	AcceptDialog *exterr;
	DirAccess *dir_access;
	ConfirmationDialog *confirm_save;

	ToolButton *dir_up;

	ToolButton *refresh;
	ToolButton *show_hidden;

	Vector<String> filters;

	bool mode_overrides_title;
	String root_subfolder;
	String root_prefix;

	static bool default_show_hidden_files;
	bool show_hidden_files;

	bool invalidated;

	void update_dir();
	void update_file_name();
	void update_file_list();
	void update_filters();

	void _tree_multi_selected(Object *p_object, int p_cell, bool p_selected);
	void _tree_selected();

	void _select_drive(int p_idx);
	void _tree_item_activated();
	void _dir_entered(String p_dir);
	void _file_entered(const String &p_file);
	void _action_pressed();
	void _save_confirm_pressed();
	void _cancel_pressed();
	void _filter_selected(int);
	void _make_dir();
	void _make_dir_confirm();
	void _go_up();

	void _change_dir(const String &p_new_dir);
	void _update_drives(bool p_select = true);

	void _unhandled_input(const Ref<InputEvent> &p_event);

	bool _is_open_should_be_disabled();

	virtual void _post_popup();

protected:
	void _notification(int p_what);
	static void _bind_methods();
	//bind helpers
public:
	void clear_filters();
	void add_filter(const String &p_filter, const String &p_description = "");
	void set_filters(const Vector<String> &p_filters);
	Vector<String> get_filters() const;

	void set_enable_multiple_selection(bool p_enable);
	Vector<String> get_selected_files() const;

	String get_current_dir() const;
	String get_current_file() const;
	String get_current_path() const;
	void set_current_dir(const String &p_dir);
	void set_current_file(const String &p_file);
	void set_current_path(const String &p_path);

	void set_root_subfolder(const String &p_root);
	String get_root_subfolder() const;

	void set_mode_overrides_title(bool p_override);
	bool is_mode_overriding_title() const;

	void set_mode(Mode p_mode);
	Mode get_mode() const;

	VBoxContainer *get_vbox();
	LineEdit *get_line_edit() { return file; }

	void set_access(Access p_access);
	Access get_access() const;

	void set_show_hidden_files(bool p_show);
	bool is_showing_hidden_files() const;

	static void set_default_show_hidden_files(bool p_show);

	void invalidate();

	void deselect_items();

	FileDialog();
	~FileDialog();
};

class LineEditFileChooser : public HBoxContainer {
	GDCLASS(LineEditFileChooser, HBoxContainer);
	Button *button;
	LineEdit *line_edit;
	FileDialog *dialog;

	void _chosen(const String &p_text);
	void _browse();

protected:
	static void _bind_methods();

public:
	Button *get_button() { return button; }
	LineEdit *get_line_edit() { return line_edit; }
	FileDialog *get_file_dialog() { return dialog; }

	LineEditFileChooser();
};

VARIANT_ENUM_CAST(FileDialog::Mode);
VARIANT_ENUM_CAST(FileDialog::Access);

#endif // FILE_DIALOG_H
