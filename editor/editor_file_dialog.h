/**************************************************************************/
/*  editor_file_dialog.h                                                  */
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

#ifndef EDITOR_FILE_DIALOG_H
#define EDITOR_FILE_DIALOG_H

#include "core/os/dir_access.h"
#include "scene/gui/box_container.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tool_button.h"

class DependencyRemoveDialog;

class EditorFileDialog : public ConfirmationDialog {
	GDCLASS(EditorFileDialog, ConfirmationDialog);

public:
	enum DisplayMode {
		DISPLAY_THUMBNAILS,
		DISPLAY_LIST
	};

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
	typedef void (*RegisterFunc)(EditorFileDialog *);

	static GetIconFunc get_icon_func;
	static GetIconFunc get_large_icon_func;
	static RegisterFunc register_func;
	static RegisterFunc unregister_func;

private:
	enum ItemMenu {
		ITEM_MENU_COPY_PATH,
		ITEM_MENU_DELETE,
		ITEM_MENU_REFRESH,
		ITEM_MENU_NEW_FOLDER,
		ITEM_MENU_SHOW_IN_EXPLORER
	};

	ConfirmationDialog *makedialog;
	LineEdit *makedirname;

	Button *makedir;
	Access access;
	//Button *action;
	VBoxContainer *vbox;
	Mode mode;
	bool can_create_dir;
	LineEdit *dir;

	ToolButton *dir_prev;
	ToolButton *dir_next;
	ToolButton *dir_up;

	HBoxContainer *drives_container;
	HBoxContainer *shortcuts_container;
	OptionButton *drives;
	ItemList *item_list;
	PopupMenu *item_menu;
	TextureRect *preview;
	VBoxContainer *preview_vb;
	HSplitContainer *list_hb;
	HBoxContainer *file_box;
	LineEdit *file;
	OptionButton *filter;
	AcceptDialog *mkdirerr;
	DirAccess *dir_access;
	ConfirmationDialog *confirm_save;
	DependencyRemoveDialog *remove_dialog;

	ToolButton *mode_thumbnails;
	ToolButton *mode_list;

	ToolButton *refresh;
	ToolButton *favorite;
	ToolButton *show_hidden;

	ToolButton *fav_up;
	ToolButton *fav_down;

	ItemList *favorites;
	ItemList *recent;

	Vector<String> local_history;
	int local_history_pos;
	void _push_history();

	Vector<String> filters;

	bool preview_waiting;
	int preview_wheel_index;
	float preview_wheel_timeout;
	static bool default_show_hidden_files;
	static DisplayMode default_display_mode;
	bool show_hidden_files;
	DisplayMode display_mode;

	bool disable_overwrite_warning;
	bool invalidated;

	void update_dir();
	void update_file_name();
	void update_file_list();
	void update_filters();

	void _update_favorites();
	void _favorite_pressed();
	void _favorite_selected(int p_idx);
	void _favorite_move_up();
	void _favorite_move_down();

	void _recent_selected(int p_idx);

	void _item_selected(int p_item);
	void _multi_selected(int p_item, bool p_selected);
	void _items_clear_selection();
	void _item_dc_selected(int p_item);

	void _item_list_item_rmb_selected(int p_item, const Vector2 &p_pos);
	void _item_list_rmb_clicked(const Vector2 &p_pos);
	void _item_menu_id_pressed(int p_option);

	void _select_drive(int p_idx);
	void _dir_entered(String p_dir);
	void _file_entered(const String &p_file);
	void _action_pressed();
	void _save_confirm_pressed();
	void _cancel_pressed();
	void _filter_selected(int);
	void _make_dir();
	void _make_dir_confirm();

	void _delete_items();

	void _update_drives(bool p_select = true);

	void _go_up();
	void _go_back();
	void _go_forward();

	virtual void _post_popup();

	void _save_to_recent();
	//callback function is callback(String p_path,Ref<Texture> preview,Variant udata) preview null if could not load

	void _thumbnail_result(const String &p_path, const Ref<Texture> &p_preview, const Ref<Texture> &p_small_preview, const Variant &p_udata);
	void _thumbnail_done(const String &p_path, const Ref<Texture> &p_preview, const Ref<Texture> &p_small_preview, const Variant &p_udata);
	void _request_single_thumbnail(const String &p_path);

	void _unhandled_input(const Ref<InputEvent> &p_event);

	bool _is_open_should_be_disabled();

protected:
	void _notification(int p_what);
	static void _bind_methods();
	//bind helpers
public:
	void clear_filters();
	void add_filter(const String &p_filter, const String &p_description = "");

	void set_enable_multiple_selection(bool p_enable);
	Vector<String> get_selected_files() const;

	String get_current_dir() const;
	String get_current_file() const;
	String get_current_path() const;
	void set_current_dir(const String &p_dir);
	void set_current_file(const String &p_file);
	void set_current_path(const String &p_path);

	void set_display_mode(DisplayMode p_mode);
	DisplayMode get_display_mode() const;

	void set_mode(Mode p_mode);
	Mode get_mode() const;

	VBoxContainer *get_vbox();
	LineEdit *get_line_edit() { return file; }

	void set_access(Access p_access);
	Access get_access() const;

	void set_show_hidden_files(bool p_show);
	bool is_showing_hidden_files() const;

	static void set_default_show_hidden_files(bool p_show);
	static void set_default_display_mode(DisplayMode p_mode);

	void invalidate();

	void set_disable_overwrite_warning(bool p_disable);
	bool is_overwrite_warning_disabled() const;

	EditorFileDialog();
	~EditorFileDialog();
};

class EditorLineEditFileChooser : public HBoxContainer {
	GDCLASS(EditorLineEditFileChooser, HBoxContainer);
	Button *button;
	LineEdit *line_edit;
	EditorFileDialog *dialog;

	void _chosen(const String &p_text);
	void _browse();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	Button *get_button() { return button; }
	LineEdit *get_line_edit() { return line_edit; }
	EditorFileDialog *get_file_dialog() { return dialog; }

	EditorLineEditFileChooser();
};

VARIANT_ENUM_CAST(EditorFileDialog::Mode);
VARIANT_ENUM_CAST(EditorFileDialog::Access);
VARIANT_ENUM_CAST(EditorFileDialog::DisplayMode);

#endif // EDITOR_FILE_DIALOG_H
