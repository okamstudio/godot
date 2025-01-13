/**************************************************************************/
/*  project_list.h                                                        */
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

#ifndef PROJECT_LIST_H
#define PROJECT_LIST_H

#include "core/io/config_file.h"
#include "scene/gui/box_container.h"
#include "scene/gui/scroll_container.h"

class AcceptDialog;
class Button;
class Label;
class ProjectList;
class TextureButton;
class TextureRect;

class ProjectListItemControl : public HBoxContainer {
	GDCLASS(ProjectListItemControl, HBoxContainer)

	VBoxContainer *main_vbox = nullptr;
	TextureButton *favorite_button = nullptr;
	Button *explore_button = nullptr;

	TextureRect *project_icon = nullptr;
	Label *project_title = nullptr;
	Label *project_path = nullptr;
	Label *last_edited_info = nullptr;
	Label *project_version = nullptr;
	TextureRect *project_unsupported_features = nullptr;
	HBoxContainer *tag_container = nullptr;

	bool project_is_missing = false;
	bool icon_needs_reload = true;
	bool is_selected = false;
	bool is_hovering = false;

	void _favorite_button_pressed();
	void _explore_button_pressed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_project_title(const String &p_title);
	void set_project_path(const String &p_path);
	void set_tags(const PackedStringArray &p_tags, ProjectList *p_parent_list);
	void set_project_icon(const Ref<Texture2D> &p_icon);
	void set_last_edited_info(const String &p_info);
	void set_project_version(const String &p_version);
	void set_unsupported_features(PackedStringArray p_features);

	bool should_load_project_icon() const;
	void set_selected(bool p_selected);

	void set_is_favorite(bool p_favorite);
	void set_is_missing(bool p_missing);
	void set_is_grayed(bool p_grayed);

	ProjectListItemControl();
};

class ProjectList : public ScrollContainer {
	GDCLASS(ProjectList, ScrollContainer)

	friend class ProjectManager;

public:
	enum FilterOption {
		EDIT_DATE,
		NAME,
		PATH,
		TAGS,
	};

	// Can often be passed by copy.
	struct Item {
		String project_name;
		String description;
		String project_version;
		PackedStringArray tags;
		String tag_sort_string;
		String path;
		String icon;
		String main_scene;
		PackedStringArray unsupported_features;
		uint64_t last_edited = 0;
		bool favorite = false;
		bool grayed = false;
		bool missing = false;
		bool recovery_mode = false;
		int version = 0;

		ProjectListItemControl *control = nullptr;

		Item() {}

		Item(const String &p_name,
				const String &p_description,
				const String &p_project_version,
				const PackedStringArray &p_tags,
				const String &p_path,
				const String &p_icon,
				const String &p_main_scene,
				const PackedStringArray &p_unsupported_features,
				uint64_t p_last_edited,
				bool p_favorite,
				bool p_grayed,
				bool p_missing,
				bool p_recovery_mode,
				int p_version) {
			project_name = p_name;
			description = p_description;
			project_version = p_project_version;
			tags = p_tags;
			path = p_path;
			icon = p_icon;
			main_scene = p_main_scene;
			unsupported_features = p_unsupported_features;
			last_edited = p_last_edited;
			favorite = p_favorite;
			grayed = p_grayed;
			missing = p_missing;
			recovery_mode = p_recovery_mode;
			version = p_version;

			control = nullptr;

			PackedStringArray sorted_tags = tags;
			sorted_tags.sort();
			tag_sort_string = String().join(sorted_tags);
		}

		_FORCE_INLINE_ bool operator==(const Item &l) const {
			return path == l.path;
		}
	};

private:
	String _config_path;
	ConfigFile _config;

	Vector<Item> _projects;

	int _icon_load_index = 0;
	bool project_opening_initiated = false;

	String _search_term;
	FilterOption _order_option = FilterOption::EDIT_DATE;
	HashSet<String> _selected_project_paths;
	String _last_clicked; // Project key

	VBoxContainer *project_list_vbox = nullptr;

	// Projects scan.

	struct ScanData {
		Thread *thread = nullptr;
		PackedStringArray paths_to_scan;
		List<String> found_projects;
		SafeFlag scan_in_progress;
	};
	ScanData *scan_data = nullptr;
	AcceptDialog *scan_progress = nullptr;

	static void _scan_thread(void *p_scan_data);
	void _scan_finished();

	// Initialization & loading.

	void _migrate_config();

	static Item load_project_data(const String &p_property_key, bool p_favorite);
	void _update_icons_async();
	void _load_project_icon(int p_index);

	// Project list updates.

	static void _scan_folder_recursive(const String &p_path, List<String> *r_projects, const SafeFlag &p_scan_active);

	// Project list items.

	void _create_project_item_control(int p_index);
	void _toggle_project(int p_index);
	void _remove_project(int p_index, bool p_update_settings);

	void _list_item_input(const Ref<InputEvent> &p_ev, Node *p_hb);
	void _on_favorite_pressed(Node *p_hb);
	void _on_explore_pressed(const String &p_path);

	// Project list selection.

	void _clear_project_selection();
	void _select_project_nocheck(int p_index);
	void _deselect_project_nocheck(int p_index);
	void _select_project_range(int p_begin, int p_end);

	// Global menu integration.

	void _global_menu_new_window(const Variant &p_tag);
	void _global_menu_open_project(const Variant &p_tag);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static const char *SIGNAL_LIST_CHANGED;
	static const char *SIGNAL_SELECTION_CHANGED;
	static const char *SIGNAL_PROJECT_ASK_OPEN;

	static bool project_feature_looks_like_version(const String &p_feature);

	// Initialization & loading.

	void save_config();

	// Project list updates.

	void load_project_list();
	void update_project_list();
	void sort_projects();
	int get_project_count() const;

	void find_projects(const String &p_path);
	void find_projects_multiple(const PackedStringArray &p_paths);

	// Project list items.

	void add_project(const String &dir_path, bool favorite);
	void set_project_version(const String &p_project_path, int version);
	int refresh_project(const String &dir_path);
	void ensure_project_visible(int p_index);

	// Project list selection.

	void select_project(int p_index);
	void select_first_visible_project();
	Vector<Item> get_selected_projects() const;
	const HashSet<String> &get_selected_project_keys() const;
	int get_single_selected_index() const;
	void erase_selected_projects(bool p_delete_project_contents);

	// Missing projects.

	bool is_any_project_missing() const;
	void erase_missing_projects();

	// Project list sorting and filtering.

	void set_search_term(String p_search_term);
	void add_search_tag(const String &p_tag);
	void set_order_option(int p_option);

	// Global menu integration.

	void update_dock_menu();

	ProjectList();
};

#endif // PROJECT_LIST_H
