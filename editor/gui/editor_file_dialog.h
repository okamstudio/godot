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

#pragma once

#include "core/io/dir_access.h"
#include "editor/file_info.h"
#include "scene/gui/dialogs.h"
#include "scene/property_list_helper.h"

class DependencyRemoveDialog;
class GridContainer;
class HSplitContainer;
class HFlowContainer;
class ItemList;
class MenuButton;
class OptionButton;
class PopupMenu;
class TextureRect;
class VSeparator;

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

	enum FileMode {
		FILE_MODE_OPEN_FILE,
		FILE_MODE_OPEN_FILES,
		FILE_MODE_OPEN_DIR,
		FILE_MODE_OPEN_ANY,
		FILE_MODE_SAVE_FILE
	};

	typedef Ref<Texture2D> (*GetIconFunc)(const String &);
	typedef void (*RegisterFunc)(EditorFileDialog *);

	static GetIconFunc get_icon_func;
	static GetIconFunc get_thumbnail_func;
	static RegisterFunc register_func;
	static RegisterFunc unregister_func;

private:
	enum ItemMenu {
		ITEM_MENU_COPY_PATH,
		ITEM_MENU_DELETE,
		ITEM_MENU_REFRESH,
		ITEM_MENU_NEW_FOLDER,
		ITEM_MENU_SHOW_IN_EXPLORER,
		ITEM_MENU_SHOW_BUNDLE_CONTENT,
	};

	ConfirmationDialog *makedialog = nullptr;
	LineEdit *makedirname = nullptr;

	VSeparator *makedir_sep = nullptr;
	Button *makedir = nullptr;
	Access access = ACCESS_RESOURCES;

	HFlowContainer *flow_checkbox_options = nullptr;
	GridContainer *grid_select_options = nullptr;
	VBoxContainer *vbox = nullptr;
	FileMode mode = FILE_MODE_SAVE_FILE;
	bool can_create_dir = false;
	LineEdit *dir = nullptr;

	Button *dir_prev = nullptr;
	Button *dir_next = nullptr;
	Button *dir_up = nullptr;

	HBoxContainer *drives_container = nullptr;
	HBoxContainer *shortcuts_container = nullptr;
	OptionButton *drives = nullptr;
	ItemList *item_list = nullptr;
	PopupMenu *item_menu = nullptr;
	TextureRect *preview = nullptr;
	VBoxContainer *preview_vb = nullptr;
	HSplitContainer *body_hsplit = nullptr;
	HSplitContainer *list_hb = nullptr;
	HBoxContainer *file_box = nullptr;
	LineEdit *file = nullptr;
	OptionButton *filter = nullptr;
	AcceptDialog *error_dialog = nullptr;
	Ref<DirAccess> dir_access;
	ConfirmationDialog *confirm_save = nullptr;
	DependencyRemoveDialog *dep_remove_dialog = nullptr;
	ConfirmationDialog *global_remove_dialog = nullptr;
	VBoxContainer *side_vbox = nullptr;
	VBoxContainer *vbc = nullptr;
	HBoxContainer *pathhb = nullptr;

	Button *mode_thumbnails = nullptr;
	Button *mode_list = nullptr;

	Button *refresh = nullptr;
	Button *favorite = nullptr;
	Button *show_hidden = nullptr;
	Button *show_search_filter_button = nullptr;

	String search_string;
	bool show_search_filter = false;
	HBoxContainer *filter_hb = nullptr;
	LineEdit *filter_box = nullptr;
	FileSortOption file_sort = FileSortOption::FILE_SORT_NAME;
	MenuButton *file_sort_button = nullptr;

	Button *fav_up = nullptr;
	Button *fav_down = nullptr;

	ItemList *favorites = nullptr;
	ItemList *recent = nullptr;

	Vector<String> local_history;
	int local_history_pos = 0;
	void _push_history();

	Vector<String> filters;
	Vector<String> processed_filters;

	bool previews_enabled = true;
	bool preview_waiting = false;
	int preview_wheel_index = 0;
	float preview_wheel_timeout = 0.0f;

	static bool default_show_hidden_files;
	static DisplayMode default_display_mode;
	bool show_hidden_files;
	DisplayMode display_mode;

	bool disable_overwrite_warning = false;
	bool is_invalidating = false;

	struct ThemeCache {
		Ref<Texture2D> parent_folder;
		Ref<Texture2D> forward_folder;
		Ref<Texture2D> back_folder;
		Ref<Texture2D> open_folder;
		Ref<Texture2D> reload;
		Ref<Texture2D> toggle_hidden;
		Ref<Texture2D> toggle_filename_filter;
		Ref<Texture2D> favorite;
		Ref<Texture2D> mode_thumbnails;
		Ref<Texture2D> mode_list;
		Ref<Texture2D> create_folder;
		Ref<Texture2D> favorites_up;
		Ref<Texture2D> favorites_down;

		Ref<Texture2D> filter_box;
		Ref<Texture2D> file_sort_button;

		Ref<Texture2D> folder;
		Color folder_icon_color;

		Ref<Texture2D> action_copy;
		Ref<Texture2D> action_delete;
		Ref<Texture2D> filesystem;

		Ref<Texture2D> folder_medium_thumbnail;
		Ref<Texture2D> file_medium_thumbnail;
		Ref<Texture2D> folder_big_thumbnail;
		Ref<Texture2D> file_big_thumbnail;

		Ref<Texture2D> progress[8]{};
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
	void update_search_filter_gui();
	void update_filters();

	void _focus_file_text();

	void _update_favorites();
	void _favorite_pressed();
	void _favorite_selected(int p_idx);
	void _favorite_move_up();
	void _favorite_move_down();

	void _update_recent();
	void _recent_selected(int p_idx);

	void _item_selected(int p_item);
	void _multi_selected(int p_item, bool p_selected);
	void _items_clear_selection(const Vector2 &p_pos, MouseButton p_mouse_button_index);
	void _item_dc_selected(int p_item);

	void _item_list_item_rmb_clicked(int p_item, const Vector2 &p_pos, MouseButton p_mouse_button_index);
	void _item_list_empty_clicked(const Vector2 &p_pos, MouseButton p_mouse_button_index);
	void _item_menu_id_pressed(int p_option);

	void _select_drive(int p_idx);
	void _dir_submitted(const String &p_dir);
	void _action_pressed();
	void _save_confirm_pressed();
	void _cancel_pressed();
	void _filter_selected(int);
	void _make_dir();
	void _make_dir_confirm();

	void _focus_filter_box();
	void _filter_changed(const String &p_text);
	void _search_filter_selected();
	void _file_sort_popup(int p_id);

	void _delete_items();
	void _delete_files_global();

	void _update_drives(bool p_select = true);
	void _update_icons();

	void _go_up();
	void _go_back();
	void _go_forward();

	void _invalidate();

	virtual void _post_popup() override;

	void _save_to_recent();
	// Callback function is callback(String p_path,Ref<Texture2D> preview,Variant udata) preview null if could not load.

	void _thumbnail_result(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata);
	void _thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata);
	void _request_single_thumbnail(const String &p_path);

	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

	bool _is_open_should_be_disabled();

	void _update_side_menu_visibility(bool p_native_dlg);

	void _native_popup();
	void _native_dialog_cb(bool p_ok, const Vector<String> &p_files, int p_filter, const Dictionary &p_selected_options);

	TypedArray<Dictionary> _get_options() const;
	void _update_option_controls();
	void _option_changed_checkbox_toggled(bool p_pressed, const String &p_name);
	void _option_changed_item_selected(int p_idx, const String &p_name);

protected:
	virtual void _update_theme_item_cache() override;

	void _notification(int p_what);
	bool _set(const StringName &p_name, const Variant &p_value) { return property_helper.property_set_value(p_name, p_value); }
	bool _get(const StringName &p_name, Variant &r_ret) const { return property_helper.property_get_value(p_name, r_ret); }
	void _get_property_list(List<PropertyInfo> *p_list) const { property_helper.get_property_list(p_list); }
	bool _property_can_revert(const StringName &p_name) const { return property_helper.property_can_revert(p_name); }
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const { return property_helper.property_get_revert(p_name, r_property); }
	static void _bind_methods();

public:
	Color get_dir_icon_color(const String &p_dir_path);

	virtual void set_visible(bool p_visible) override;
	virtual void popup(const Rect2i &p_rect = Rect2i()) override;

	// Public for use with callable_mp.
	void _file_submitted(const String &p_file);

	void popup_file_dialog();
	void clear_filters();
	void add_filter(const String &p_filter, const String &p_description = "");
	void set_filters(const Vector<String> &p_filters);
	Vector<String> get_filters() const;
	void clear_search_filter();
	void set_search_filter(const String &p_search_filter);
	String get_search_filter() const;

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

	void set_display_mode(DisplayMode p_mode);
	DisplayMode get_display_mode() const;

	void set_file_mode(FileMode p_mode);
	FileMode get_file_mode() const;

	VBoxContainer *get_vbox();
	LineEdit *get_line_edit() { return file; }

	void set_access(Access p_access);
	Access get_access() const;

	static void set_default_show_hidden_files(bool p_show);
	static void set_default_display_mode(DisplayMode p_mode);
	void set_show_hidden_files(bool p_show);
	void set_show_search_filter(bool p_show);
	bool is_showing_hidden_files() const;

	void invalidate();

	void set_disable_overwrite_warning(bool p_disable);
	bool is_overwrite_warning_disabled() const;

	void set_previews_enabled(bool p_enabled);
	bool are_previews_enabled();

	void add_side_menu(Control *p_menu, const String &p_title = "");

	EditorFileDialog();
	~EditorFileDialog();
};

VARIANT_ENUM_CAST(EditorFileDialog::FileMode);
VARIANT_ENUM_CAST(EditorFileDialog::Access);
VARIANT_ENUM_CAST(EditorFileDialog::DisplayMode);
