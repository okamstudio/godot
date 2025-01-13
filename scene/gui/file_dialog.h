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
#include "core/io/dir_access.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/tree.h"
#include "scene/property_list_helper.h"

class GridContainer;
class PopupMenu;

class FileDialog : public ConfirmationDialog {
	GDCLASS(FileDialog, ConfirmationDialog);

public:
	enum Access {
		ACCESS_RESOURCES,
		ACCESS_USERDATA,
		ACCESS_FILESYSTEM
	};

	enum FileMode {
		FILE_MODE_OPEN_FILE,
		FILE_MODE_OPEN_FILES,
		FILE_MODE_OPEN_DIR,
		FILE_MODE_OPEN_ANY,
		FILE_MODE_SAVE_FILE
	};

	enum ItemMenu {
		ITEM_MENU_COPY_PATH,
		ITEM_MENU_SHOW_IN_EXPLORER,
		ITEM_MENU_SHOW_BUNDLE_CONTENT,
	};

	typedef Ref<Texture2D> (*GetIconFunc)(const String &);
	typedef void (*RegisterFunc)(FileDialog *);

	static GetIconFunc get_icon_func;
	static RegisterFunc register_func;
	static RegisterFunc unregister_func;

private:
	ConfirmationDialog *makedialog = nullptr;
	LineEdit *makedirname = nullptr;

	Button *makedir = nullptr;
	Access access = ACCESS_RESOURCES;
	VBoxContainer *vbox = nullptr;
	GridContainer *grid_options = nullptr;
	FileMode mode;
	LineEdit *dir = nullptr;
	HBoxContainer *drives_container = nullptr;
	HBoxContainer *shortcuts_container = nullptr;
	OptionButton *drives = nullptr;
	Tree *tree = nullptr;
	HBoxContainer *filename_filter_box = nullptr;
	LineEdit *filename_filter = nullptr;
	HBoxContainer *file_box = nullptr;
	LineEdit *file = nullptr;
	OptionButton *filter = nullptr;
	AcceptDialog *mkdirerr = nullptr;
	AcceptDialog *exterr = nullptr;
	Ref<DirAccess> dir_access;
	ConfirmationDialog *confirm_save = nullptr;
	PopupMenu *item_menu = nullptr;

	Label *message = nullptr;

	Button *dir_prev = nullptr;
	Button *dir_next = nullptr;
	Button *dir_up = nullptr;

	Button *refresh = nullptr;
	Button *show_hidden = nullptr;
	Button *show_filename_filter_button = nullptr;

	Vector<String> filters;
	Vector<String> processed_filters;
	String file_name_filter;
	bool show_filename_filter = false;

	Vector<String> local_history;
	int local_history_pos = 0;
	void _push_history();

	bool mode_overrides_title = true;
	String root_subfolder;
	String root_prefix;

	static bool default_show_hidden_files;
	bool show_hidden_files = false;
	bool use_native_dialog = false;

	bool is_invalidating = false;

	struct ThemeCache {
		Ref<Texture2D> parent_folder;
		Ref<Texture2D> forward_folder;
		Ref<Texture2D> back_folder;
		Ref<Texture2D> reload;
		Ref<Texture2D> toggle_hidden;
		Ref<Texture2D> toggle_filename_filter;
		Ref<Texture2D> folder;
		Ref<Texture2D> file;
		Ref<Texture2D> create_folder;

		Color folder_icon_color;
		Color file_icon_color;
		Color file_disabled_color;

		Color icon_normal_color;
		Color icon_hover_color;
		Color icon_focus_color;
		Color icon_pressed_color;
	} theme_cache;

	struct Option {
		String name;
		Vector<String> values;
		int default_idx = 0;
	};

	static inline PropertyListHelper base_property_helper;
	PropertyListHelper property_helper;

	Vector<Option> options;
	Dictionary selected_options;
	bool options_dirty = false;
	String full_dir;

	void update_dir();
	void update_file_name();
	void update_file_list();
	void update_filename_filter();
	void update_filename_filter_gui();
	void update_filters();

	void _item_menu_id_pressed(int p_option);
	void _empty_clicked(const Vector2 &p_pos, MouseButton p_button);
	void _rmb_select(const Vector2 &p_pos, MouseButton p_button = MouseButton::RIGHT);

	void _focus_file_text();

	void _tree_multi_selected(Object *p_object, int p_cell, bool p_selected);
	void _tree_selected();

	void _select_drive(int p_idx);
	void _tree_item_activated();
	void _dir_submitted(String p_dir);
	void _file_submitted(const String &p_file);
	void _action_pressed();
	void _save_confirm_pressed();
	void _cancel_pressed();
	void _filter_selected(int);
	void _filename_filter_changed();
	void _filename_filter_selected();
	void _tree_select_first();
	void _make_dir();
	void _make_dir_confirm();
	void _go_up();
	void _go_back();
	void _go_forward();

	void _change_dir(const String &p_new_dir);
	void _update_drives(bool p_select = true);

	void _invalidate();

	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

	bool _can_use_native_popup();
	void _native_popup();
	void _native_dialog_cb(bool p_ok, const Vector<String> &p_files, int p_filter);
	void _native_dialog_cb_with_options(bool p_ok, const Vector<String> &p_files, int p_filter, const Dictionary &p_selected_options);

	bool _is_open_should_be_disabled();

	TypedArray<Dictionary> _get_options() const;
	void _update_option_controls();
	void _option_changed_checkbox_toggled(bool p_pressed, const String &p_name);
	void _option_changed_item_selected(int p_idx, const String &p_name);

	virtual void _post_popup() override;

protected:
	void _validate_property(PropertyInfo &p_property) const;
	void _notification(int p_what);
	bool _set(const StringName &p_name, const Variant &p_value) { return property_helper.property_set_value(p_name, p_value); }
	bool _get(const StringName &p_name, Variant &r_ret) const { return property_helper.property_get_value(p_name, r_ret); }
	void _get_property_list(List<PropertyInfo> *p_list) const { property_helper.get_property_list(p_list); }
	bool _property_can_revert(const StringName &p_name) const { return property_helper.property_can_revert(p_name); }
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const { return property_helper.property_get_revert(p_name, r_property); }
	static void _bind_methods();

public:
	virtual void set_visible(bool p_visible) override;
	virtual void popup(const Rect2i &p_rect = Rect2i()) override;

	void popup_file_dialog();
	void clear_filters();
	void add_filter(const String &p_filter, const String &p_description = "");
	void set_filters(const Vector<String> &p_filters);
	Vector<String> get_filters() const;
	void clear_filename_filter();
	void set_filename_filter(const String &p_filename_filter);
	String get_filename_filter() const;

	void set_enable_multiple_selection(bool p_enable);
	Vector<String> get_selected_files() const;

	String get_current_dir() const;
	String get_current_file() const;
	String get_current_path() const;
	void set_current_dir(const String &p_dir);
	void set_current_file(const String &p_file);
	void set_current_path(const String &p_path);

	String get_option_name(int p_option) const;
	Vector<String> get_option_values(int p_option) const;
	int get_option_default(int p_option) const;
	void set_option_name(int p_option, const String &p_name);
	void set_option_values(int p_option, const Vector<String> &p_values);
	void set_option_default(int p_option, int p_index);

	void add_option(const String &p_name, const Vector<String> &p_values, int p_index);

	void set_option_count(int p_count);
	int get_option_count() const;

	Dictionary get_selected_options() const;

	void set_root_subfolder(const String &p_root);
	String get_root_subfolder() const;

	void set_mode_overrides_title(bool p_override);
	bool is_mode_overriding_title() const;

	void set_use_native_dialog(bool p_native);
	bool get_use_native_dialog() const;

	void set_file_mode(FileMode p_mode);
	FileMode get_file_mode() const;

	VBoxContainer *get_vbox();
	LineEdit *get_line_edit() { return file; }

	void set_access(Access p_access);
	Access get_access() const;

	void set_show_hidden_files(bool p_show);
	bool is_showing_hidden_files() const;
	void set_show_filename_filter(bool p_show);
	bool get_show_filename_filter() const;

	static void set_default_show_hidden_files(bool p_show);

	void invalidate();

	void deselect_all();

	FileDialog();
	~FileDialog();
};

VARIANT_ENUM_CAST(FileDialog::FileMode);
VARIANT_ENUM_CAST(FileDialog::Access);

#endif // FILE_DIALOG_H
