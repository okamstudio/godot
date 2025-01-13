/**************************************************************************/
/*  filesystem_dock.h                                                     */
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

#ifndef FILESYSTEM_DOCK_H
#define FILESYSTEM_DOCK_H

#include "editor/dependency_editor.h"
#include "editor/editor_file_system.h"
#include "editor/file_info.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/script_create_dialog.h"
#include "scene/gui/box_container.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"

class CreateDialog;
class EditorDirDialog;
class ItemList;
class LineEdit;
class ProgressBar;
class SceneCreateDialog;
class ShaderCreateDialog;
class DirectoryCreateDialog;
class EditorResourceTooltipPlugin;

class FileSystemTree : public Tree {
	virtual Control *make_custom_tooltip(const String &p_text) const;
};

class FileSystemList : public ItemList {
	GDCLASS(FileSystemList, ItemList);

	bool popup_edit_commited = true;
	VBoxContainer *popup_editor_vb = nullptr;
	Popup *popup_editor = nullptr;
	LineEdit *line_editor = nullptr;

	virtual Control *make_custom_tooltip(const String &p_text) const override;
	void _line_editor_submit(const String &p_text);
	void _text_editor_popup_modal_close();

protected:
	static void _bind_methods();

public:
	bool edit_selected();
	String get_edit_text();

	FileSystemList();
};

class FileSystemDock : public VBoxContainer {
	GDCLASS(FileSystemDock, VBoxContainer);

public:
	enum FileListDisplayMode {
		FILE_LIST_DISPLAY_THUMBNAILS,
		FILE_LIST_DISPLAY_LIST
	};

	enum DisplayMode {
		DISPLAY_MODE_TREE_ONLY,
		DISPLAY_MODE_VSPLIT,
		DISPLAY_MODE_HSPLIT,
	};

	enum Overwrite {
		OVERWRITE_UNDECIDED,
		OVERWRITE_REPLACE,
		OVERWRITE_RENAME,
	};

private:
	enum FileMenu {
		FILE_OPEN,
		FILE_INHERIT,
		FILE_MAIN_SCENE,
		FILE_INSTANTIATE,
		FILE_ADD_FAVORITE,
		FILE_REMOVE_FAVORITE,
		FILE_SHOW_IN_FILESYSTEM,
		FILE_DEPENDENCIES,
		FILE_OWNERS,
		FILE_MOVE,
		FILE_RENAME,
		FILE_REMOVE,
		FILE_DUPLICATE,
		FILE_REIMPORT,
		FILE_NEW,
		FILE_SHOW_IN_EXPLORER,
		FILE_OPEN_EXTERNAL,
		FILE_OPEN_IN_TERMINAL,
		FILE_COPY_PATH,
		FILE_COPY_ABSOLUTE_PATH,
		FILE_COPY_UID,
		FOLDER_EXPAND_ALL,
		FOLDER_COLLAPSE_ALL,
		FILE_NEW_RESOURCE,
		FILE_NEW_TEXTFILE,
		FILE_NEW_FOLDER,
		FILE_NEW_SCRIPT,
		FILE_NEW_SCENE,
		CONVERT_BASE_ID = 1000,
	};

	HashMap<String, TreeItem *> folder_map;
	HashMap<String, Color> folder_colors;
	Dictionary assigned_folder_colors;

	FileSortOption file_sort = FileSortOption::FILE_SORT_NAME;

	VBoxContainer *scanning_vb = nullptr;
	ProgressBar *scanning_progress = nullptr;
	SplitContainer *split_box = nullptr;
	VBoxContainer *file_list_vb = nullptr;

	int split_box_offset_h = 0;
	int split_box_offset_v = 0;

	HashSet<String> favorites;

	Button *button_dock_placement = nullptr;

	Button *button_toggle_display_mode = nullptr;
	Button *button_reload = nullptr;
	Button *button_file_list_display_mode = nullptr;
	Button *button_hist_next = nullptr;
	Button *button_hist_prev = nullptr;
	LineEdit *current_path_line_edit = nullptr;

	HBoxContainer *toolbar2_hbc = nullptr;
	LineEdit *tree_search_box = nullptr;
	MenuButton *tree_button_sort = nullptr;

	LineEdit *file_list_search_box = nullptr;
	MenuButton *file_list_button_sort = nullptr;

	PackedStringArray searched_tokens;
	Vector<String> uncollapsed_paths_before_search;

	TextureRect *search_icon = nullptr;
	HBoxContainer *path_hb = nullptr;

	FileListDisplayMode file_list_display_mode;
	DisplayMode display_mode;
	DisplayMode old_display_mode;

	PopupMenu *file_list_popup = nullptr;
	PopupMenu *tree_popup = nullptr;

	DependencyEditor *deps_editor = nullptr;
	DependencyEditorOwners *owners_editor = nullptr;
	DependencyRemoveDialog *remove_dialog = nullptr;

	EditorDirDialog *move_dialog = nullptr;
	DirectoryCreateDialog *make_dir_dialog = nullptr;

	ConfirmationDialog *overwrite_dialog = nullptr;
	ScrollContainer *overwrite_dialog_scroll = nullptr;
	Label *overwrite_dialog_header = nullptr;
	Label *overwrite_dialog_footer = nullptr;
	Label *overwrite_dialog_file_list = nullptr;

	ConfirmationDialog *conversion_dialog = nullptr;

	SceneCreateDialog *make_scene_dialog = nullptr;
	ScriptCreateDialog *make_script_dialog = nullptr;
	ShaderCreateDialog *make_shader_dialog = nullptr;
	CreateDialog *new_resource_dialog = nullptr;

	bool always_show_folders = false;

	bool editor_is_dark_theme = false;

	class FileOrFolder {
	public:
		String path;
		bool is_file = false;

		FileOrFolder() {}
		FileOrFolder(const String &p_path, bool p_is_file) :
				path(p_path),
				is_file(p_is_file) {}
	};
	FileOrFolder to_rename;
	FileOrFolder to_duplicate;
	Vector<FileOrFolder> to_move;
	String to_move_path;
	bool to_move_or_copy = false;

	Vector<String> to_convert;
	int selected_conversion_id = 0;

	Vector<String> history;
	int history_pos;
	int history_max_size;

	String current_path;
	String select_after_scan;

	bool updating_tree = false;
	int tree_update_id;
	FileSystemTree *tree = nullptr;
	FileSystemList *files = nullptr;
	bool import_dock_needs_update = false;
	TreeItem *resources_item = nullptr;
	TreeItem *favorites_item = nullptr;

	bool holding_branch = false;
	Vector<TreeItem *> tree_items_selected_on_drag_begin;
	PackedInt32Array list_items_selected_on_drag_begin;

	LocalVector<Ref<EditorResourceTooltipPlugin>> tooltip_plugins;

	HashSet<String> cached_valid_conversion_targets;

	void _tree_mouse_exited();
	void _reselect_items_selected_on_drag_begin(bool reset = false);

	Ref<Texture2D> _get_tree_item_icon(bool p_is_valid, const String &p_file_type, const String &p_icon_path);
	void _create_tree(TreeItem *p_parent, EditorFileSystemDirectory *p_dir, Vector<String> &uncollapsed_paths, bool p_select_in_favorites, bool p_unfold_path = false);
	void _update_tree(const Vector<String> &p_uncollapsed_paths = Vector<String>(), bool p_uncollapse_root = false, bool p_select_in_favorites = false, bool p_unfold_path = false);
	void _navigate_to_path(const String &p_path, bool p_select_in_favorites = false);
	bool _update_filtered_items(TreeItem *p_tree_item = nullptr);

	void _file_list_gui_input(Ref<InputEvent> p_event);
	void _tree_gui_input(Ref<InputEvent> p_event);

	HashSet<String> _get_valid_conversions_for_file_paths(const Vector<String> &p_paths);

	void _update_file_list(bool p_keep_selection);
	void _toggle_file_display();
	void _set_file_display(bool p_active);
	void _fs_changed();

	void _select_file(const String &p_path, bool p_select_in_favorites = false, bool p_navigate = true);
	void _tree_activate_file();
	void _file_list_activate_file(int p_idx);
	void _file_multi_selected(int p_index, bool p_selected);
	void _tree_multi_selected(Object *p_item, int p_column, bool p_selected);

	bool _get_imported_files(const String &p_path, String &r_extension, Vector<String> &r_files) const;
	void _update_import_dock();

	void _get_all_items_in_dir(EditorFileSystemDirectory *p_efsd, Vector<String> &r_files, Vector<String> &r_folders) const;
	void _find_file_owners(EditorFileSystemDirectory *p_efsd, const HashSet<String> &p_renames, HashSet<String> &r_file_owners) const;
	void _try_move_item(const FileOrFolder &p_item, const String &p_new_path, HashMap<String, String> &p_file_renames, HashMap<String, String> &p_folder_renames);
	void _try_duplicate_item(const FileOrFolder &p_item, const String &p_new_path) const;
	void _before_move(HashMap<String, ResourceUID::ID> &r_uids, HashSet<String> &r_file_owners) const;
	void _update_dependencies_after_move(const HashMap<String, String> &p_renames, const HashSet<String> &p_file_owners) const;
	void _update_resource_paths_after_move(const HashMap<String, String> &p_renames, const HashMap<String, ResourceUID::ID> &p_uids) const;
	void _update_favorites_after_move(const HashMap<String, String> &p_files_renames, const HashMap<String, String> &p_folders_renames) const;
	void _update_project_settings_after_move(const HashMap<String, String> &p_renames, const HashMap<String, String> &p_folders_renames);
	String _get_unique_name(const FileOrFolder &p_entry, const String &p_at_path);

	void _update_folder_colors_setting();

	void _resource_removed(const Ref<Resource> &p_resource);
	void _file_removed(const String &p_file);
	void _folder_removed(const String &p_folder);

	void _resource_created();
	void _make_scene_confirm();
	void _rename_operation_confirm();
	void _duplicate_operation_confirm(const String &p_path);
	void _overwrite_dialog_action(bool p_overwrite);
	void _convert_dialog_action();
	Vector<String> _check_existing();
	void _move_operation_confirm(const String &p_to_path, bool p_copy = false, Overwrite p_overwrite = OVERWRITE_UNDECIDED);

	void _tree_rmb_option(int p_option);
	void _file_list_rmb_option(int p_option);
	void _generic_rmb_option_selected(int p_option);
	void _file_option(int p_option, const Vector<String> &p_selected);

	void _fw_history();
	void _bw_history();
	void _update_history();
	void _push_to_history();

	void _set_scanning_mode();
	void _rescan();

	void _change_split_mode();
	void _split_dragged(int p_offset);

	void _search_changed(const String &p_text, const Control *p_from);
	bool _matches_all_search_tokens(const String &p_text);

	MenuButton *_create_file_menu_button();
	void _file_sort_popup(int p_id);

	void _folder_color_index_pressed(int p_index, PopupMenu *p_menu);
	void _file_and_folders_fill_popup(PopupMenu *p_popup, const Vector<String> &p_paths, bool p_display_path_dependent_options = true);
	void _tree_rmb_select(const Vector2 &p_pos, MouseButton p_button);
	void _file_list_item_clicked(int p_item, const Vector2 &p_pos, MouseButton p_mouse_button_index);
	void _file_list_empty_clicked(const Vector2 &p_pos, MouseButton p_mouse_button_index);
	void _tree_empty_click(const Vector2 &p_pos, MouseButton p_button);
	void _tree_empty_selected();

	void _search(EditorFileSystemDirectory *p_path, List<FileInfo> *matches, int p_max_items);

	void _set_current_path_line_edit_text(const String &p_path);

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	void _get_drag_target_folder(String &target, bool &target_favorites, const Point2 &p_point, Control *p_from) const;

	void _preview_invalidated(const String &p_path);
	void _file_list_thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata);
	void _tree_thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata);

	void _update_display_mode(bool p_force = false);

	Vector<String> _tree_get_selected(bool remove_self_inclusion = true, bool p_include_unselected_cursor = false) const;
	Vector<String> _file_list_get_selected() const;

	bool _is_file_type_disabled_by_feature_profile(const StringName &p_class);

	void _feature_profile_changed();
	void _project_settings_changed();
	static Vector<String> _remove_self_included_paths(Vector<String> selected_strings);

	void _change_bottom_dock_placement();

	bool _can_dock_horizontal() const;
	void _set_dock_horizontal(bool p_enable);

private:
	static FileSystemDock *singleton;

public:
	static FileSystemDock *get_singleton() { return singleton; }

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static constexpr double ITEM_COLOR_SCALE = 1.75;
	static constexpr double ITEM_ALPHA_MIN = 0.1;
	static constexpr double ITEM_ALPHA_MAX = 0.15;
	static constexpr double ITEM_BG_DARK_SCALE = 0.3;

	const HashMap<String, Color> &get_folder_colors() const;
	Dictionary get_assigned_folder_colors() const;

	Vector<String> get_selected_paths() const;
	Vector<String> get_uncollapsed_paths() const;

	String get_current_path() const;
	String get_current_directory() const;
	String get_folder_path_at_mouse_position() const;

	void navigate_to_path(const String &p_path);
	void focus_on_path();
	void focus_on_filter();
	void create_directory(const String &p_path, const String &p_base_dir);

	ScriptCreateDialog *get_script_create_dialog() const;

	void fix_dependencies(const String &p_for_file);
	void update_all();

	int get_h_split_offset() const { return split_box_offset_h; }
	void set_h_split_offset(int p_offset) { split_box_offset_h = p_offset; }
	int get_v_split_offset() const { return split_box_offset_v; }
	void set_v_split_offset(int p_offset) { split_box_offset_v = p_offset; }
	void select_file(const String &p_file);

	void set_display_mode(DisplayMode p_display_mode);
	DisplayMode get_display_mode() const { return display_mode; }

	void set_file_sort(FileSortOption p_file_sort);
	FileSortOption get_file_sort() const { return file_sort; }

	void set_file_list_display_mode(FileListDisplayMode p_mode);
	FileListDisplayMode get_file_list_display_mode() const { return file_list_display_mode; }

	Tree *get_tree_control() { return tree; }

	void add_resource_tooltip_plugin(const Ref<EditorResourceTooltipPlugin> &p_plugin);
	void remove_resource_tooltip_plugin(const Ref<EditorResourceTooltipPlugin> &p_plugin);
	Control *create_tooltip_for_path(const String &p_path) const;

	void save_layout_to_config(Ref<ConfigFile> p_layout, const String &p_section) const;
	void load_layout_from_config(Ref<ConfigFile> p_layout, const String &p_section);

	FileSystemDock();
	~FileSystemDock();
};

VARIANT_ENUM_CAST(FileSystemDock::Overwrite);

#endif // FILESYSTEM_DOCK_H
