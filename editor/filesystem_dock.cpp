/**************************************************************************/
/*  filesystem_dock.cpp                                                   */
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

#include "filesystem_dock.h"

#include "core/io/resource_loader.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "editor/directory_create_dialog.h"
#include "editor/scene_create_dialog.h"
#include "editor_feature_profile.h"
#include "editor_node.h"
#include "editor_resource_preview.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "import_dock.h"
#include "scene/main/viewport.h"
#include "scene/resources/packed_scene.h"

Ref<Texture> FileSystemDock::_get_tree_item_icon(bool p_is_valid, String p_file_type) {
	Ref<Texture> file_icon;
	if (!p_is_valid) {
		file_icon = get_icon("ImportFail", "EditorIcons");
	} else {
		file_icon = (has_icon(p_file_type, "EditorIcons")) ? get_icon(p_file_type, "EditorIcons") : get_icon("File", "EditorIcons");
	}
	return file_icon;
}

bool FileSystemDock::_create_tree(TreeItem *p_parent, EditorFileSystemDirectory *p_dir, Vector<String> &uncollapsed_paths, bool p_select_in_favorites, bool p_unfold_path) {
	bool parent_should_expand = false;

	// Create a tree item for the subdirectory.
	TreeItem *subdirectory_item = tree->create_item(p_parent);
	String dname = p_dir->get_name();
	if (dname == "") {
		dname = "res://";
	}

	subdirectory_item->set_text(0, dname);
	subdirectory_item->set_icon(0, get_icon("Folder", "EditorIcons"));
	subdirectory_item->set_icon_modulate(0, get_color("folder_icon_modulate", "FileDialog"));
	subdirectory_item->set_selectable(0, true);
	String lpath = p_dir->get_path();
	subdirectory_item->set_metadata(0, lpath);
	if (!p_select_in_favorites && (path == lpath || ((display_mode == DISPLAY_MODE_SPLIT) && path.get_base_dir() == lpath))) {
		subdirectory_item->select(0);
		// Keep select an item when re-created a tree
		// To prevent crashing when nothing is selected.
		subdirectory_item->set_as_cursor(0);
	}

	if (p_unfold_path && path.begins_with(lpath) && path != lpath) {
		subdirectory_item->set_collapsed(false);
	} else {
		subdirectory_item->set_collapsed(uncollapsed_paths.find(lpath) < 0);
	}
	if (searched_string.length() > 0 && dname.to_lower().find(searched_string) >= 0) {
		parent_should_expand = true;
	}

	// Create items for all subdirectories.
	bool reversed = file_sort == FILE_SORT_NAME_REVERSE;
	for (int i = reversed ? p_dir->get_subdir_count() - 1 : 0;
			reversed ? i >= 0 : i < p_dir->get_subdir_count();
			reversed ? i-- : i++) {
		parent_should_expand = (_create_tree(subdirectory_item, p_dir->get_subdir(i), uncollapsed_paths, p_select_in_favorites, p_unfold_path) || parent_should_expand);
	}

	// Create all items for the files in the subdirectory.
	if (display_mode == DISPLAY_MODE_TREE_ONLY) {
		String main_scene = ProjectSettings::get_singleton()->get("application/run/main_scene");

		// Build the list of the files to display.
		List<FileInfo> file_list;
		for (int i = 0; i < p_dir->get_file_count(); i++) {
			String file_type = p_dir->get_file_type(i);

			if (_is_file_type_disabled_by_feature_profile(file_type)) {
				// If type is disabled, file won't be displayed.
				continue;
			}

			String file_name = p_dir->get_file(i);
			if (searched_string.length() > 0) {
				if (file_name.to_lower().find(searched_string) < 0) {
					// The searched string is not in the file name, we skip it.
					continue;
				} else {
					// We expand all parents.
					parent_should_expand = true;
				}
			}

			FileInfo fi;
			fi.name = p_dir->get_file(i);
			fi.type = p_dir->get_file_type(i);
			fi.import_broken = !p_dir->get_file_import_is_valid(i);
			fi.modified_time = p_dir->get_file_modified_time(i);

			file_list.push_back(fi);
		}

		// Sort the file list if needed.
		_sort_file_info_list(file_list);

		// Build the tree.
		for (List<FileInfo>::Element *E = file_list.front(); E; E = E->next()) {
			FileInfo fi = E->get();

			TreeItem *file_item = tree->create_item(subdirectory_item);
			file_item->set_text(0, fi.name);
			file_item->set_icon(0, _get_tree_item_icon(!fi.import_broken, fi.type));
			String file_metadata = lpath.plus_file(fi.name);
			file_item->set_metadata(0, file_metadata);
			if (!p_select_in_favorites && path == file_metadata) {
				file_item->select(0);
				file_item->set_as_cursor(0);
			}
			if (main_scene == file_metadata) {
				file_item->set_custom_color(0, get_color("accent_color", "Editor"));
			}
			Array udata;
			udata.push_back(tree_update_id);
			udata.push_back(file_item);
			EditorResourcePreview::get_singleton()->queue_resource_preview(file_metadata, this, "_tree_thumbnail_done", udata);
		}
	} else if (display_mode == DISPLAY_MODE_SPLIT) {
		if (lpath.get_base_dir() == path.get_base_dir()) {
			subdirectory_item->select(0);
			subdirectory_item->set_as_cursor(0);
		}
	}

	if (searched_string.length() > 0) {
		if (parent_should_expand) {
			subdirectory_item->set_collapsed(false);
		} else if (dname != "res://") {
			subdirectory_item->get_parent()->remove_child(subdirectory_item);
			memdelete(subdirectory_item);
		}
	}

	return parent_should_expand;
}

Vector<String> FileSystemDock::_compute_uncollapsed_paths() {
	// Register currently collapsed paths.
	Vector<String> uncollapsed_paths;
	TreeItem *root = tree->get_root();
	if (root) {
		TreeItem *favorites_item = root->get_children();
		if (!favorites_item->is_collapsed()) {
			uncollapsed_paths.push_back(favorites_item->get_metadata(0));
		}

		TreeItem *resTree = root->get_children()->get_next();
		if (resTree) {
			Vector<TreeItem *> needs_check;
			needs_check.push_back(resTree);

			while (needs_check.size()) {
				if (!needs_check[0]->is_collapsed()) {
					uncollapsed_paths.push_back(needs_check[0]->get_metadata(0));
					TreeItem *child = needs_check[0]->get_children();
					while (child) {
						needs_check.push_back(child);
						child = child->get_next();
					}
				}
				needs_check.remove(0);
			}
		}
	}
	return uncollapsed_paths;
}

void FileSystemDock::_update_tree(const Vector<String> &p_uncollapsed_paths, bool p_uncollapse_root, bool p_select_in_favorites, bool p_unfold_path) {
	// Recreate the tree.
	tree->clear();
	tree_update_id++;
	updating_tree = true;
	TreeItem *root = tree->create_item();

	// Handles the favorites.
	TreeItem *favorites = tree->create_item(root);
	favorites->set_icon(0, get_icon("Favorites", "EditorIcons"));
	favorites->set_text(0, TTR("Favorites:"));
	favorites->set_metadata(0, "Favorites");
	favorites->set_collapsed(p_uncollapsed_paths.find("Favorites") < 0);

	Vector<String> favorite_paths = EditorSettings::get_singleton()->get_favorites();

	DirAccessRef da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	bool fav_changed = false;
	for (int i = favorite_paths.size() - 1; i >= 0; i--) {
		if (da->dir_exists(favorite_paths[i]) || da->file_exists(favorite_paths[i])) {
			continue;
		}
		favorite_paths.remove(i);
		fav_changed = true;
	}
	if (fav_changed) {
		EditorSettings::get_singleton()->set_favorites(favorite_paths);
	}

	for (int i = 0; i < favorite_paths.size(); i++) {
		String fave = favorite_paths[i];
		if (!fave.begins_with("res://")) {
			continue;
		}

		Ref<Texture> folder_icon = get_icon("Folder", "EditorIcons");
		const Color folder_color = get_color("folder_icon_modulate", "FileDialog");

		String text;
		Ref<Texture> icon;
		Color color;
		if (fave == "res://") {
			text = "/";
			icon = folder_icon;
			color = folder_color;
		} else if (fave.ends_with("/")) {
			text = fave.substr(0, fave.length() - 1).get_file();
			icon = folder_icon;
			color = folder_color;
		} else {
			text = fave.get_file();
			int index;
			EditorFileSystemDirectory *dir = EditorFileSystem::get_singleton()->find_file(fave, &index);
			if (dir) {
				icon = _get_tree_item_icon(dir->get_file_import_is_valid(index), dir->get_file_type(index));
			} else {
				icon = get_icon("File", "EditorIcons");
			}
			color = Color(1, 1, 1);
		}

		if (searched_string.length() == 0 || text.to_lower().find(searched_string) >= 0) {
			TreeItem *ti = tree->create_item(favorites);
			ti->set_text(0, text);
			ti->set_icon(0, icon);
			ti->set_icon_modulate(0, color);
			ti->set_tooltip(0, fave);
			ti->set_selectable(0, true);
			ti->set_metadata(0, fave);
			if (p_select_in_favorites && fave == path) {
				ti->select(0);
				ti->set_as_cursor(0);
			}
			if (!fave.ends_with("/")) {
				Array udata;
				udata.push_back(tree_update_id);
				udata.push_back(ti);
				EditorResourcePreview::get_singleton()->queue_resource_preview(fave, this, "_tree_thumbnail_done", udata);
			}
		}
	}

	Vector<String> uncollapsed_paths = p_uncollapsed_paths;
	if (p_uncollapse_root) {
		uncollapsed_paths.push_back("res://");
	}

	// Create the remaining of the tree.
	_create_tree(root, EditorFileSystem::get_singleton()->get_filesystem(), uncollapsed_paths, p_select_in_favorites, p_unfold_path);
	tree->ensure_cursor_is_visible();
	updating_tree = false;
}

void FileSystemDock::set_display_mode(DisplayMode p_display_mode) {
	display_mode = p_display_mode;
	_update_display_mode(false);
}

void FileSystemDock::_update_display_mode(bool p_force) {
	// Compute the new display mode.
	if (p_force || old_display_mode != display_mode) {
		button_toggle_display_mode->set_pressed(display_mode == DISPLAY_MODE_SPLIT);
		switch (display_mode) {
			case DISPLAY_MODE_TREE_ONLY:
				tree->show();
				tree->set_v_size_flags(SIZE_EXPAND_FILL);
				toolbar2_hbc->show();

				_update_tree(_compute_uncollapsed_paths());
				file_list_vb->hide();
				break;

			case DISPLAY_MODE_SPLIT:
				tree->show();
				tree->set_v_size_flags(SIZE_EXPAND_FILL);
				tree->ensure_cursor_is_visible();
				toolbar2_hbc->hide();
				_update_tree(_compute_uncollapsed_paths());

				file_list_vb->show();
				_update_file_list(true);
				break;
		}
		old_display_mode = display_mode;
	}
}

void FileSystemDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (initialized) {
				return;
			}
			initialized = true;
			EditorFeatureProfileManager::get_singleton()->connect("current_feature_profile_changed", this, "_feature_profile_changed");

			EditorFileSystem::get_singleton()->connect("filesystem_changed", this, "_fs_changed");
			EditorResourcePreview::get_singleton()->connect("preview_invalidated", this, "_preview_invalidated");

			String ei = "EditorIcons";
			button_reload->set_icon(get_icon("Reload", ei));
			button_toggle_display_mode->set_icon(get_icon("Panels2", ei));
			button_file_list_display_mode->connect("pressed", this, "_toggle_file_display");

			files->connect("item_activated", this, "_file_list_activate_file");
			button_hist_next->connect("pressed", this, "_fw_history");
			button_hist_prev->connect("pressed", this, "_bw_history");
			tree_search_box->set_right_icon(get_icon("Search", ei));
			tree_search_box->set_clear_button_enabled(true);
			tree_button_sort->set_icon(get_icon("Sort", ei));

			file_list_search_box->set_right_icon(get_icon("Search", ei));
			file_list_search_box->set_clear_button_enabled(true);
			file_list_button_sort->set_icon(get_icon("Sort", ei));

			button_hist_next->set_icon(get_icon("Forward", ei));
			button_hist_prev->set_icon(get_icon("Back", ei));
			file_list_popup->connect("id_pressed", this, "_file_list_rmb_option");
			tree_popup->connect("id_pressed", this, "_tree_rmb_option");

			current_path->connect("text_entered", this, "_navigate_to_path");

			always_show_folders = bool(EditorSettings::get_singleton()->get("docks/filesystem/always_show_folders"));

			set_file_list_display_mode(FileSystemDock::FILE_LIST_DISPLAY_LIST);

			_update_display_mode();

			if (EditorFileSystem::get_singleton()->is_scanning()) {
				_set_scanning_mode();
			} else {
				_update_tree(Vector<String>(), true);
			}
		} break;

		case NOTIFICATION_PROCESS: {
			if (EditorFileSystem::get_singleton()->is_scanning()) {
				scanning_progress->set_value(EditorFileSystem::get_singleton()->get_scanning_progress() * 100);
			}
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			Dictionary dd = get_viewport()->gui_get_drag_data();
			if (tree->is_visible_in_tree() && dd.has("type")) {
				if (dd.has("favorite")) {
					if ((String(dd["favorite"]) == "all")) {
						tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);
					}
				} else if ((String(dd["type"]) == "files") || (String(dd["type"]) == "files_and_dirs") || (String(dd["type"]) == "resource")) {
					tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM | Tree::DROP_MODE_INBETWEEN);
				}
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			tree->set_drop_mode_flags(0);
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			if (is_visible_in_tree()) {
				_update_display_mode(true);
			}
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			// Update icons.
			String ei = "EditorIcons";
			button_reload->set_icon(get_icon("Reload", ei));
			button_toggle_display_mode->set_icon(get_icon("Panels2", ei));
			button_hist_next->set_icon(get_icon("Forward", ei));
			button_hist_prev->set_icon(get_icon("Back", ei));
			if (file_list_display_mode == FILE_LIST_DISPLAY_LIST) {
				button_file_list_display_mode->set_icon(get_icon("FileThumbnail", "EditorIcons"));
			} else {
				button_file_list_display_mode->set_icon(get_icon("FileList", "EditorIcons"));
			}

			tree_search_box->set_right_icon(get_icon("Search", ei));
			tree_search_box->set_clear_button_enabled(true);
			tree_button_sort->set_icon(get_icon("Sort", ei));

			file_list_search_box->set_right_icon(get_icon("Search", ei));
			file_list_search_box->set_clear_button_enabled(true);
			file_list_button_sort->set_icon(get_icon("Sort", ei));

			// Update always show folders.
			bool new_always_show_folders = bool(EditorSettings::get_singleton()->get("docks/filesystem/always_show_folders"));
			if (new_always_show_folders != always_show_folders) {
				always_show_folders = new_always_show_folders;
				_update_file_list(true);
			}

			// Change full tree mode.
			_update_display_mode();
		} break;
	}
}

void FileSystemDock::_tree_multi_selected(Object *p_item, int p_column, bool p_selected) {
	// Update the import dock.
	import_dock_needs_update = true;
	call_deferred("_update_import_dock");

	// Return if we don't select something new.
	if (!p_selected) {
		return;
	}

	// Tree item selected.
	TreeItem *selected = tree->get_selected();
	if (!selected) {
		return;
	}

	TreeItem *favorites_item = tree->get_root()->get_children();
	if (selected->get_parent() == favorites_item && !String(selected->get_metadata(0)).ends_with("/")) {
		// Go to the favorites if we click in the favorites and the path has changed.
		path = "Favorites";
	} else {
		path = selected->get_metadata(0);
		// Note: the "Favorites" item also leads to this path.
	}

	// Set the current path.
	_set_current_path_text(path);
	_push_to_history();

	// Update the file list.
	if (!updating_tree && display_mode == DISPLAY_MODE_SPLIT) {
		_update_file_list(false);
	}
}

String FileSystemDock::get_selected_path() const {
	if (path.ends_with("/")) {
		return path;
	} else {
		return path.get_base_dir();
	}
}

String FileSystemDock::get_current_path() const {
	return path;
}

void FileSystemDock::_set_current_path_text(const String &p_path) {
	if (p_path == "Favorites") {
		current_path->set_text(TTR("Favorites"));
	} else {
		current_path->set_text(path);
	}
}

void FileSystemDock::_navigate_to_path(const String &p_path, bool p_select_in_favorites) {
	if (p_path == "Favorites") {
		path = p_path;
	} else {
		String target_path = p_path;
		// If the path is a file, do not only go to the directory in the tree, also select the file in the file list.
		if (target_path.ends_with("/")) {
			target_path = target_path.substr(0, target_path.length() - 1);
		}
		DirAccess *dirAccess = DirAccess::open("res://");
		if (dirAccess->file_exists(p_path)) {
			path = target_path;
		} else if (dirAccess->dir_exists(p_path)) {
			path = target_path + "/";
		} else {
			memdelete(dirAccess);
			ERR_FAIL_MSG(vformat("Cannot navigate to '%s' as it has not been found in the file system!", p_path));
		}
		memdelete(dirAccess);
	}

	_set_current_path_text(path);
	_push_to_history();

	_update_tree(_compute_uncollapsed_paths(), false, p_select_in_favorites, true);
	if (display_mode == DISPLAY_MODE_SPLIT) {
		_update_file_list(false);
		files->get_v_scroll()->set_value(0);
	}

	String file_name = p_path.get_file();
	if (!file_name.empty()) {
		for (int i = 0; i < files->get_item_count(); i++) {
			if (files->get_item_text(i) == file_name) {
				files->select(i, true);
				files->ensure_current_is_visible();
				break;
			}
		}
	}
}

void FileSystemDock::navigate_to_path(const String &p_path) {
	file_list_search_box->clear();
	_navigate_to_path(p_path);
}

void FileSystemDock::_file_list_thumbnail_done(const String &p_path, const Ref<Texture> &p_preview, const Ref<Texture> &p_small_preview, const Variant &p_udata) {
	if ((file_list_vb->is_visible_in_tree() || path == p_path.get_base_dir()) && p_preview.is_valid()) {
		Array uarr = p_udata;
		int idx = uarr[0];
		String file = uarr[1];
		if (idx < files->get_item_count() && files->get_item_text(idx) == file && files->get_item_metadata(idx) == p_path) {
			if (file_list_display_mode == FILE_LIST_DISPLAY_LIST) {
				if (p_small_preview.is_valid()) {
					files->set_item_icon(idx, p_small_preview);
				}
			} else {
				files->set_item_icon(idx, p_preview);
			}
		}
	}
}

void FileSystemDock::_tree_thumbnail_done(const String &p_path, const Ref<Texture> &p_preview, const Ref<Texture> &p_small_preview, const Variant &p_udata) {
	if (p_small_preview.is_valid()) {
		Array uarr = p_udata;
		if (tree_update_id == (int)uarr[0]) {
			TreeItem *file_item = Object::cast_to<TreeItem>(uarr[1]);
			if (file_item) {
				file_item->set_icon(0, p_small_preview);
			}
		}
	}
}

void FileSystemDock::_toggle_file_display() {
	_set_file_display(file_list_display_mode != FILE_LIST_DISPLAY_LIST);
	emit_signal("display_mode_changed");
}

void FileSystemDock::_set_file_display(bool p_active) {
	if (p_active) {
		file_list_display_mode = FILE_LIST_DISPLAY_LIST;
		button_file_list_display_mode->set_icon(get_icon("FileThumbnail", "EditorIcons"));
		button_file_list_display_mode->set_tooltip(TTR("View items as a grid of thumbnails."));
	} else {
		file_list_display_mode = FILE_LIST_DISPLAY_THUMBNAILS;
		button_file_list_display_mode->set_icon(get_icon("FileList", "EditorIcons"));
		button_file_list_display_mode->set_tooltip(TTR("View items as a list."));
	}

	_update_file_list(true);
}

bool FileSystemDock::_is_file_type_disabled_by_feature_profile(const StringName &p_class) {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_null()) {
		return false;
	}

	StringName class_name = p_class;

	while (class_name != StringName()) {
		if (profile->is_class_disabled(class_name)) {
			return true;
		}
		class_name = ClassDB::get_parent_class(class_name);
	}

	return false;
}

void FileSystemDock::_search(EditorFileSystemDirectory *p_path, List<FileInfo> *matches, int p_max_items) {
	if (matches->size() > p_max_items) {
		return;
	}

	for (int i = 0; i < p_path->get_subdir_count(); i++) {
		_search(p_path->get_subdir(i), matches, p_max_items);
	}

	for (int i = 0; i < p_path->get_file_count(); i++) {
		String file = p_path->get_file(i);

		if (file.to_lower().find(searched_string) != -1) {
			FileInfo fi;
			fi.name = file;
			fi.type = p_path->get_file_type(i);
			fi.path = p_path->get_file_path(i);
			fi.import_broken = !p_path->get_file_import_is_valid(i);

			if (_is_file_type_disabled_by_feature_profile(fi.type)) {
				// This type is disabled, will not appear here.
				continue;
			}

			matches->push_back(fi);
			if (matches->size() > p_max_items) {
				return;
			}
		}
	}
}

struct FileSystemDock::FileInfoTypeComparator {
	bool operator()(const FileInfo &p_a, const FileInfo &p_b) const {
		// Uses the extension, then the icon name to distinguish file types.
		String icon_path_a = "";
		String icon_path_b = "";
		Ref<Texture> icon_a = EditorNode::get_singleton()->get_class_icon(p_a.type);
		if (icon_a.is_valid()) {
			icon_path_a = icon_a->get_name();
		}
		Ref<Texture> icon_b = EditorNode::get_singleton()->get_class_icon(p_b.type);
		if (icon_b.is_valid()) {
			icon_path_b = icon_b->get_name();
		}
		return NaturalNoCaseComparator()(p_a.name.get_extension() + icon_path_a + p_a.name.get_basename(), p_b.name.get_extension() + icon_path_b + p_b.name.get_basename());
	}
};

struct FileSystemDock::FileInfoModifiedTimeComparator {
	bool operator()(const FileInfo &p_a, const FileInfo &p_b) const {
		return p_a.modified_time > p_b.modified_time;
	}
};

void FileSystemDock::_sort_file_info_list(List<FileSystemDock::FileInfo> &r_file_list) {
	// Sort the file list if needed.
	switch (file_sort) {
		case FILE_SORT_TYPE:
			r_file_list.sort_custom<FileInfoTypeComparator>();
			break;
		case FILE_SORT_TYPE_REVERSE:
			r_file_list.sort_custom<FileInfoTypeComparator>();
			r_file_list.invert();
			break;
		case FILE_SORT_MODIFIED_TIME:
			r_file_list.sort_custom<FileInfoModifiedTimeComparator>();
			break;
		case FILE_SORT_MODIFIED_TIME_REVERSE:
			r_file_list.sort_custom<FileInfoModifiedTimeComparator>();
			r_file_list.invert();
			break;
		case FILE_SORT_NAME_REVERSE:
			r_file_list.sort();
			r_file_list.invert();
			break;
		default: // FILE_SORT_NAME
			r_file_list.sort();
			break;
	}
}

void FileSystemDock::_update_file_list(bool p_keep_selection) {
	// Register the previously selected items.
	Set<String> cselection;
	if (p_keep_selection) {
		for (int i = 0; i < files->get_item_count(); i++) {
			if (files->is_selected(i)) {
				cselection.insert(files->get_item_text(i));
			}
		}
	}

	files->clear();

	_set_current_path_text(path);

	String directory = path;
	String file = "";

	String ei = "EditorIcons";
	int thumbnail_size = EditorSettings::get_singleton()->get("docks/filesystem/thumbnail_size");
	thumbnail_size *= EDSCALE;
	Ref<Texture> folder_thumbnail;
	Ref<Texture> file_thumbnail;
	Ref<Texture> file_thumbnail_broken;

	bool use_thumbnails = (file_list_display_mode == FILE_LIST_DISPLAY_THUMBNAILS);

	if (use_thumbnails) {
		// Thumbnails mode.
		files->set_max_columns(0);
		files->set_icon_mode(ItemList::ICON_MODE_TOP);
		files->set_fixed_column_width(thumbnail_size * 3 / 2);
		files->set_max_text_lines(2);
		files->set_fixed_icon_size(Size2(thumbnail_size, thumbnail_size));

		if (thumbnail_size < 64) {
			folder_thumbnail = get_icon("FolderMediumThumb", ei);
			file_thumbnail = get_icon("FileMediumThumb", ei);
			file_thumbnail_broken = get_icon("FileDeadMediumThumb", ei);
		} else {
			folder_thumbnail = get_icon("FolderBigThumb", ei);
			file_thumbnail = get_icon("FileBigThumb", ei);
			file_thumbnail_broken = get_icon("FileDeadBigThumb", ei);
		}
	} else {
		// No thumbnails.
		files->set_icon_mode(ItemList::ICON_MODE_LEFT);
		files->set_max_columns(1);
		files->set_max_text_lines(1);
		files->set_fixed_column_width(0);
		files->set_fixed_icon_size(Size2());
	}

	Ref<Texture> folder_icon = (use_thumbnails) ? folder_thumbnail : get_icon("folder", "FileDialog");
	const Color folder_color = get_color("folder_icon_modulate", "FileDialog");

	// Build the FileInfo list.
	List<FileInfo> file_list;
	if (path == "Favorites") {
		// Display the favorites.
		Vector<String> favorites = EditorSettings::get_singleton()->get_favorites();
		for (int i = 0; i < favorites.size(); i++) {
			String favorite = favorites[i];
			String text;
			Ref<Texture> icon;
			if (favorite == "res://") {
				text = "/";
				icon = folder_icon;
				if (searched_string.length() == 0 || text.to_lower().find(searched_string) >= 0) {
					files->add_item(text, icon, true);
					files->set_item_metadata(files->get_item_count() - 1, favorite);
				}
			} else if (favorite.ends_with("/")) {
				text = favorite.substr(0, favorite.length() - 1).get_file();
				icon = folder_icon;
				if (searched_string.length() == 0 || text.to_lower().find(searched_string) >= 0) {
					files->add_item(text, icon, true);
					files->set_item_metadata(files->get_item_count() - 1, favorite);
				}
			} else {
				int index;
				EditorFileSystemDirectory *efd = EditorFileSystem::get_singleton()->find_file(favorite, &index);

				FileInfo fi;
				fi.name = favorite.get_file();
				fi.path = favorite;
				if (efd) {
					fi.type = efd->get_file_type(index);
					fi.import_broken = !efd->get_file_import_is_valid(index);
					fi.modified_time = efd->get_file_modified_time(index);
				} else {
					fi.type = "";
					fi.import_broken = true;
					fi.modified_time = 0;
				}

				if (searched_string.length() == 0 || fi.name.to_lower().find(searched_string) >= 0) {
					file_list.push_back(fi);
				}
			}
		}
	} else {
		// Get infos on the directory + file.
		if (directory.ends_with("/") && directory != "res://") {
			directory = directory.substr(0, directory.length() - 1);
		}
		EditorFileSystemDirectory *efd = EditorFileSystem::get_singleton()->get_filesystem_path(directory);
		if (!efd) {
			directory = path.get_base_dir();
			file = path.get_file();
			efd = EditorFileSystem::get_singleton()->get_filesystem_path(directory);
		}
		if (!efd) {
			return;
		}

		if (searched_string.length() > 0) {
			// Display the search results.
			// Limit the number of results displayed to avoid an infinite loop.
			_search(EditorFileSystem::get_singleton()->get_filesystem(), &file_list, 10000);
		} else {
			if (display_mode == DISPLAY_MODE_TREE_ONLY || always_show_folders) {
				// Display folders in the list.
				if (directory != "res://") {
					files->add_item("..", folder_icon, true);

					String bd = directory.get_base_dir();
					if (bd != "res://" && !bd.ends_with("/")) {
						bd += "/";
					}

					files->set_item_metadata(files->get_item_count() - 1, bd);
					files->set_item_selectable(files->get_item_count() - 1, false);
					files->set_item_icon_modulate(files->get_item_count() - 1, folder_color);
				}

				bool reversed = file_sort == FILE_SORT_NAME_REVERSE;
				for (int i = reversed ? efd->get_subdir_count() - 1 : 0;
						reversed ? i >= 0 : i < efd->get_subdir_count();
						reversed ? i-- : i++) {
					String dname = efd->get_subdir(i)->get_name();

					files->add_item(dname, folder_icon, true);
					files->set_item_metadata(files->get_item_count() - 1, directory.plus_file(dname) + "/");
					files->set_item_icon_modulate(files->get_item_count() - 1, folder_color);

					if (cselection.has(dname)) {
						files->select(files->get_item_count() - 1, false);
					}
				}
			}

			// Display the folder content.
			for (int i = 0; i < efd->get_file_count(); i++) {
				FileInfo fi;
				fi.name = efd->get_file(i);
				fi.path = directory.plus_file(fi.name);
				fi.type = efd->get_file_type(i);
				fi.import_broken = !efd->get_file_import_is_valid(i);
				fi.modified_time = efd->get_file_modified_time(i);

				file_list.push_back(fi);
			}
		}
		file_list.sort();
	}

	_sort_file_info_list(file_list);

	// Fills the ItemList control node from the FileInfos.
	String main_scene = ProjectSettings::get_singleton()->get("application/run/main_scene");
	String oi = "Object";
	for (List<FileInfo>::Element *E = file_list.front(); E; E = E->next()) {
		FileInfo *finfo = &(E->get());
		String fname = finfo->name;
		String fpath = finfo->path;
		String ftype = finfo->type;

		Ref<Texture> type_icon;
		Ref<Texture> big_icon;

		String tooltip = fpath;

		// Select the icons.
		if (!finfo->import_broken) {
			type_icon = (has_icon(ftype, ei)) ? get_icon(ftype, ei) : get_icon(oi, ei);
			big_icon = file_thumbnail;
		} else {
			type_icon = get_icon("ImportFail", ei);
			big_icon = file_thumbnail_broken;
			tooltip += "\n" + TTR("Status: Import of file failed. Please fix file and reimport manually.");
		}

		// Add the item to the ItemList.
		int item_index;
		if (use_thumbnails) {
			files->add_item(fname, big_icon, true);
			item_index = files->get_item_count() - 1;
			files->set_item_metadata(item_index, fpath);
			files->set_item_tag_icon(item_index, type_icon);

		} else {
			files->add_item(fname, type_icon, true);
			item_index = files->get_item_count() - 1;
			files->set_item_metadata(item_index, fpath);
		}

		if (fpath == main_scene) {
			files->set_item_custom_fg_color(item_index, get_color("accent_color", "Editor"));
		}

		// Generate the preview.
		if (!finfo->import_broken) {
			Array udata;
			udata.resize(2);
			udata[0] = item_index;
			udata[1] = fname;
			EditorResourcePreview::get_singleton()->queue_resource_preview(fpath, this, "_file_list_thumbnail_done", udata);
		}

		// Select the items.
		if (cselection.has(fname)) {
			files->select(item_index, false);
		}

		if (!p_keep_selection && file != "" && fname == file) {
			files->select(item_index, true);
			files->ensure_current_is_visible();
		}

		// Tooltip.
		if (finfo->sources.size()) {
			for (int j = 0; j < finfo->sources.size(); j++) {
				tooltip += "\nSource: " + finfo->sources[j];
			}
		}
		files->set_item_tooltip(item_index, tooltip);
	}
}

void FileSystemDock::_select_file(const String &p_path, bool p_select_in_favorites) {
	String fpath = p_path;
	if (fpath.ends_with("/")) {
		if (fpath != "res://") {
			fpath = fpath.substr(0, fpath.length() - 1);
		}
	} else if (fpath != "Favorites") {
		if (FileAccess::exists(fpath + ".import")) {
			Ref<ConfigFile> config;
			config.instance();
			Error err = config->load(fpath + ".import");
			if (err == OK) {
				if (config->has_section_key("remap", "importer")) {
					String importer = config->get_value("remap", "importer");
					if (importer == "keep") {
						EditorNode::get_singleton()->show_warning(TTR("Importing has been disabled for this file, so it can't be opened for editing."));
						return;
					}
				}
			}
		}

		if (ResourceLoader::get_resource_type(fpath) == "PackedScene") {
			editor->open_request(fpath);
		} else {
			editor->load_resource(fpath);
		}
	}
	_navigate_to_path(fpath, p_select_in_favorites);
}

void FileSystemDock::_tree_activate_file() {
	TreeItem *selected = tree->get_selected();
	if (selected) {
		String path = selected->get_metadata(0);
		TreeItem *parent = selected->get_parent();
		bool is_favorite = parent != nullptr && parent->get_metadata(0) == "Favorites";

		if ((!is_favorite && path.ends_with("/")) || path == "Favorites") {
			bool collapsed = selected->is_collapsed();
			selected->set_collapsed(!collapsed);
		} else {
			_select_file(path, is_favorite && !path.ends_with("/"));
		}
	}
}

void FileSystemDock::_file_list_activate_file(int p_idx) {
	_select_file(files->get_item_metadata(p_idx));
}

void FileSystemDock::_preview_invalidated(const String &p_path) {
	if (file_list_display_mode == FILE_LIST_DISPLAY_THUMBNAILS && p_path.get_base_dir() == path && searched_string.length() == 0 && file_list_vb->is_visible_in_tree()) {
		for (int i = 0; i < files->get_item_count(); i++) {
			if (files->get_item_metadata(i) == p_path) {
				// Re-request preview.
				Array udata;
				udata.resize(2);
				udata[0] = i;
				udata[1] = files->get_item_text(i);
				EditorResourcePreview::get_singleton()->queue_resource_preview(p_path, this, "_file_list_thumbnail_done", udata);
				break;
			}
		}
	}
}

void FileSystemDock::_fs_changed() {
	button_hist_prev->set_disabled(history_pos == 0);
	button_hist_next->set_disabled(history_pos == history.size() - 1);
	scanning_vb->hide();
	split_box->show();

	if (tree->is_visible()) {
		_update_tree(_compute_uncollapsed_paths());
	}

	if (file_list_vb->is_visible()) {
		_update_file_list(true);
	}

	set_process(false);
}

void FileSystemDock::_set_scanning_mode() {
	button_hist_prev->set_disabled(true);
	button_hist_next->set_disabled(true);
	split_box->hide();
	scanning_vb->show();
	set_process(true);
	if (EditorFileSystem::get_singleton()->is_scanning()) {
		scanning_progress->set_value(EditorFileSystem::get_singleton()->get_scanning_progress() * 100);
	} else {
		scanning_progress->set_value(0);
	}
}

void FileSystemDock::_fw_history() {
	if (history_pos < history.size() - 1) {
		history_pos++;
	}

	_update_history();
}

void FileSystemDock::_bw_history() {
	if (history_pos > 0) {
		history_pos--;
	}

	_update_history();
}

void FileSystemDock::_update_history() {
	path = history[history_pos];
	_set_current_path_text(path);

	if (tree->is_visible()) {
		_update_tree(_compute_uncollapsed_paths());
		tree->grab_focus();
		tree->ensure_cursor_is_visible();
	}

	if (file_list_vb->is_visible()) {
		_update_file_list(false);
	}

	button_hist_prev->set_disabled(history_pos == 0);
	button_hist_next->set_disabled(history_pos == history.size() - 1);
}

void FileSystemDock::_push_to_history() {
	if (history[history_pos] != path) {
		history.resize(history_pos + 1);
		history.push_back(path);
		history_pos++;

		if (history.size() > history_max_size) {
			history.remove(0);
			history_pos = history_max_size - 1;
		}
	}

	button_hist_prev->set_disabled(history_pos == 0);
	button_hist_next->set_disabled(history_pos == history.size() - 1);
}

void FileSystemDock::_get_all_items_in_dir(EditorFileSystemDirectory *efsd, Vector<String> &files, Vector<String> &folders) const {
	if (efsd == nullptr) {
		return;
	}

	for (int i = 0; i < efsd->get_subdir_count(); i++) {
		folders.push_back(efsd->get_subdir(i)->get_path());
		_get_all_items_in_dir(efsd->get_subdir(i), files, folders);
	}
	for (int i = 0; i < efsd->get_file_count(); i++) {
		files.push_back(efsd->get_file_path(i));
	}
}

void FileSystemDock::_find_remaps(EditorFileSystemDirectory *efsd, const Map<String, String> &renames, Vector<String> &to_remaps) const {
	for (int i = 0; i < efsd->get_subdir_count(); i++) {
		_find_remaps(efsd->get_subdir(i), renames, to_remaps);
	}
	for (int i = 0; i < efsd->get_file_count(); i++) {
		Vector<String> deps = efsd->get_file_deps(i);
		for (int j = 0; j < deps.size(); j++) {
			if (renames.has(deps[j])) {
				to_remaps.push_back(efsd->get_file_path(i));
				break;
			}
		}
	}
}

void FileSystemDock::_try_move_item(const FileOrFolder &p_item, const String &p_new_path,
		Map<String, String> &p_file_renames, Map<String, String> &p_folder_renames) {
	// Ensure folder paths end with "/".
	String old_path = (p_item.is_file || p_item.path.ends_with("/")) ? p_item.path : (p_item.path + "/");
	String new_path = (p_item.is_file || p_new_path.ends_with("/")) ? p_new_path : (p_new_path + "/");

	if (new_path == old_path) {
		return;
	} else if (old_path == "res://") {
		EditorNode::get_singleton()->add_io_error(TTR("Cannot move/rename resources root."));
		return;
	} else if (!p_item.is_file && new_path.begins_with(old_path)) {
		// This check doesn't erroneously catch renaming to a longer name as folder paths always end with "/".
		EditorNode::get_singleton()->add_io_error(TTR("Cannot move a folder into itself.") + "\n" + old_path + "\n");
		return;
	}

	// Build a list of files which will have new paths as a result of this operation.
	Vector<String> file_changed_paths;
	Vector<String> folder_changed_paths;
	if (p_item.is_file) {
		file_changed_paths.push_back(old_path);
	} else {
		folder_changed_paths.push_back(old_path);
		_get_all_items_in_dir(EditorFileSystem::get_singleton()->get_filesystem_path(old_path), file_changed_paths, folder_changed_paths);
	}

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	print_verbose("Moving " + old_path + " -> " + new_path);
	Error err = da->rename(old_path, new_path);
	if (err == OK) {
		// Move/Rename any corresponding import settings too.
		if (p_item.is_file && FileAccess::exists(old_path + ".import")) {
			err = da->rename(old_path + ".import", new_path + ".import");
			if (err != OK) {
				EditorNode::get_singleton()->add_io_error(TTR("Error moving:") + "\n" + old_path + ".import\n");
			}
		}

		// Update scene if it is open.
		for (int i = 0; i < file_changed_paths.size(); ++i) {
			String new_item_path = p_item.is_file ? new_path : file_changed_paths[i].replace_first(old_path, new_path);
			if (ResourceLoader::get_resource_type(new_item_path) == "PackedScene" && editor->is_scene_open(file_changed_paths[i])) {
				EditorData *ed = &editor->get_editor_data();
				for (int j = 0; j < ed->get_edited_scene_count(); j++) {
					if (ed->get_scene_path(j) == file_changed_paths[i]) {
						ed->get_edited_scene_root(j)->set_filename(new_item_path);
						editor->save_layout();
						break;
					}
				}
			}
		}

		// Only treat as a changed dependency if it was successfully moved.
		for (int i = 0; i < file_changed_paths.size(); ++i) {
			p_file_renames[file_changed_paths[i]] = file_changed_paths[i].replace_first(old_path, new_path);
			print_verbose("  Remap: " + file_changed_paths[i] + " -> " + p_file_renames[file_changed_paths[i]]);
			emit_signal("files_moved", file_changed_paths[i], p_file_renames[file_changed_paths[i]]);
		}
		for (int i = 0; i < folder_changed_paths.size(); ++i) {
			p_folder_renames[folder_changed_paths[i]] = folder_changed_paths[i].replace_first(old_path, new_path);
			emit_signal("folder_moved", folder_changed_paths[i], p_folder_renames[folder_changed_paths[i]].substr(0, p_folder_renames[folder_changed_paths[i]].length() - 1));
		}
	} else {
		EditorNode::get_singleton()->add_io_error(TTR("Error moving:") + "\n" + old_path + "\n");
	}
	memdelete(da);
}

void FileSystemDock::_try_duplicate_item(const FileOrFolder &p_item, const String &p_new_path) const {
	// Ensure folder paths end with "/".
	String old_path = (p_item.is_file || p_item.path.ends_with("/")) ? p_item.path : (p_item.path + "/");
	String new_path = (p_item.is_file || p_new_path.ends_with("/")) ? p_new_path : (p_new_path + "/");

	if (new_path == old_path) {
		return;
	} else if (old_path == "res://") {
		EditorNode::get_singleton()->add_io_error(TTR("Cannot move/rename resources root."));
		return;
	} else if (!p_item.is_file && new_path.begins_with(old_path)) {
		// This check doesn't erroneously catch renaming to a longer name as folder paths always end with "/".
		EditorNode::get_singleton()->add_io_error(TTR("Cannot move a folder into itself.") + "\n" + old_path + "\n");
		return;
	}

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	print_verbose("Duplicating " + old_path + " -> " + new_path);
	Error err = p_item.is_file ? da->copy(old_path, new_path) : da->copy_dir(old_path, new_path);
	if (err == OK) {
		// Move/Rename any corresponding import settings too.
		if (p_item.is_file && FileAccess::exists(old_path + ".import")) {
			err = da->copy(old_path + ".import", new_path + ".import");
			if (err != OK) {
				EditorNode::get_singleton()->add_io_error(TTR("Error duplicating:") + "\n" + old_path + ".import\n");
			}
		}
	} else {
		EditorNode::get_singleton()->add_io_error(TTR("Error duplicating:") + "\n" + old_path + "\n");
	}
	memdelete(da);
}

void FileSystemDock::_update_resource_paths_after_move(const Map<String, String> &p_renames) const {
	// Rename all resources loaded, be it subresources or actual resources.
	List<Ref<Resource>> cached;
	ResourceCache::get_cached_resources(&cached);

	for (List<Ref<Resource>>::Element *E = cached.front(); E; E = E->next()) {
		Ref<Resource> r = E->get();

		String base_path = r->get_path();
		String extra_path;
		int sep_pos = r->get_path().find("::");
		if (sep_pos >= 0) {
			extra_path = base_path.substr(sep_pos, base_path.length());
			base_path = base_path.substr(0, sep_pos);
		}

		if (p_renames.has(base_path)) {
			base_path = p_renames[base_path];
		}

		r->set_path(base_path + extra_path);
	}

	for (int i = 0; i < EditorNode::get_editor_data().get_edited_scene_count(); i++) {
		String path;
		if (i == EditorNode::get_editor_data().get_edited_scene()) {
			if (!get_tree()->get_edited_scene_root()) {
				continue;
			}

			path = get_tree()->get_edited_scene_root()->get_filename();
		} else {
			path = EditorNode::get_editor_data().get_scene_path(i);
		}

		if (p_renames.has(path)) {
			path = p_renames[path];
		}

		if (i == EditorNode::get_editor_data().get_edited_scene()) {
			get_tree()->get_edited_scene_root()->set_filename(path);
		} else {
			EditorNode::get_editor_data().set_scene_path(i, path);
		}
	}
}

void FileSystemDock::_update_dependencies_after_move(const Map<String, String> &p_renames) const {
	// The following code assumes that the following holds:
	// 1) EditorFileSystem contains the old paths/folder structure from before the rename/move.
	// 2) ResourceLoader can use the new paths without needing to call rescan.
	Vector<String> remaps;
	_find_remaps(EditorFileSystem::get_singleton()->get_filesystem(), p_renames, remaps);
	for (int i = 0; i < remaps.size(); ++i) {
		// Because we haven't called a rescan yet the found remap might still be an old path itself.
		String file = p_renames.has(remaps[i]) ? p_renames[remaps[i]] : remaps[i];
		print_verbose("Remapping dependencies for: " + file);
		Error err = ResourceLoader::rename_dependencies(file, p_renames);
		if (err == OK) {
			if (ResourceLoader::get_resource_type(file) == "PackedScene") {
				editor->reload_scene(file);
			}
		} else {
			EditorNode::get_singleton()->add_io_error(TTR("Unable to update dependencies:") + "\n" + remaps[i] + "\n");
		}
	}
}

void FileSystemDock::_update_project_settings_after_move(const Map<String, String> &p_renames) const {
	// Find all project settings of type FILE and replace them if needed.
	const Map<StringName, PropertyInfo> prop_info = ProjectSettings::get_singleton()->get_custom_property_info();
	for (const Map<StringName, PropertyInfo>::Element *E = prop_info.front(); E; E = E->next()) {
		if (E->get().hint == PROPERTY_HINT_FILE) {
			String old_path = GLOBAL_GET(E->key());
			if (p_renames.has(old_path)) {
				ProjectSettings::get_singleton()->set_setting(E->key(), p_renames[old_path]);
			}
		};
	}

	// Also search for the file in autoload, as they are stored differently from normal files.
	List<PropertyInfo> property_list;
	ProjectSettings::get_singleton()->get_property_list(&property_list);
	for (const List<PropertyInfo>::Element *E = property_list.front(); E; E = E->next()) {
		if (E->get().name.begins_with("autoload/")) {
			// If the autoload resource paths has a leading "*", it indicates that it is a Singleton,
			// so we have to handle both cases when updating.
			String autoload = GLOBAL_GET(E->get().name);
			String autoload_singleton = autoload.substr(1, autoload.length());
			if (p_renames.has(autoload)) {
				ProjectSettings::get_singleton()->set_setting(E->get().name, p_renames[autoload]);
			} else if (autoload.begins_with("*") && p_renames.has(autoload_singleton)) {
				ProjectSettings::get_singleton()->set_setting(E->get().name, "*" + p_renames[autoload_singleton]);
			}
		}
	}
	ProjectSettings::get_singleton()->save();
}

void FileSystemDock::_update_favorites_list_after_move(const Map<String, String> &p_files_renames, const Map<String, String> &p_folders_renames) const {
	Vector<String> favorites = EditorSettings::get_singleton()->get_favorites();
	Vector<String> new_favorites;

	for (int i = 0; i < favorites.size(); i++) {
		String old_path = favorites[i];
		if (p_folders_renames.has(old_path)) {
			new_favorites.push_back(p_folders_renames[old_path]);
		} else if (p_files_renames.has(old_path)) {
			new_favorites.push_back(p_files_renames[old_path]);
		} else {
			new_favorites.push_back(old_path);
		}
	}

	EditorSettings::get_singleton()->set_favorites(new_favorites);
}

void FileSystemDock::_save_scenes_after_move(const Map<String, String> &p_renames) const {
	Vector<String> remaps;
	_find_remaps(EditorFileSystem::get_singleton()->get_filesystem(), p_renames, remaps);
	Vector<String> new_filenames;

	for (int i = 0; i < remaps.size(); ++i) {
		String file = p_renames.has(remaps[i]) ? p_renames[remaps[i]] : remaps[i];
		if (ResourceLoader::get_resource_type(file) == "PackedScene") {
			new_filenames.push_back(file);
		}
	}

	editor->save_scene_list(new_filenames);
}

void FileSystemDock::_make_scene_confirm() {
	const String scene_path = make_scene_dialog->get_scene_path();

	int idx = editor->new_scene();
	EditorNode::get_singleton()->get_editor_data().set_scene_path(idx, scene_path);
	EditorNode::get_singleton()->set_edited_scene(make_scene_dialog->create_scene_root());
	Vector<String> scenes;
	scenes.push_back(scene_path);
	EditorNode::get_singleton()->save_scene_list(scenes);
}

void FileSystemDock::_file_removed(String p_file) {
	emit_signal("file_removed", p_file);

	// Find the closest parent directory available, in case multiple items were deleted along the same path.
	path = p_file.get_base_dir();
	DirAccessRef da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	while (!da->dir_exists(path)) {
		path = path.get_base_dir();
	}

	current_path->set_text(path);
}

void FileSystemDock::_folder_removed(String p_folder) {
	emit_signal("folder_removed", p_folder);

	// Find the closest parent directory available, in case multiple items were deleted along the same path.
	path = p_folder.get_base_dir();
	DirAccessRef da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	while (!da->dir_exists(path)) {
		path = path.get_base_dir();
	}

	current_path->set_text(path);
	EditorFileSystemDirectory *efd = EditorFileSystem::get_singleton()->get_filesystem_path(path);
	if (efd) {
		efd->force_update();
	}
}

void FileSystemDock::_rename_operation_confirm() {
	String new_name = rename_dialog_text->get_text().strip_edges();
	if (new_name.length() == 0) {
		EditorNode::get_singleton()->show_warning(TTR("No name provided."));
		return;
	} else if (new_name.find("/") != -1 || new_name.find("\\") != -1 || new_name.find(":") != -1) {
		EditorNode::get_singleton()->show_warning(TTR("Name contains invalid characters."));
		return;
	} else if (new_name[0] == '.') {
		EditorNode::get_singleton()->show_warning(TTR("This filename begins with a dot rendering the file invisible to the editor.\nIf you want to rename it anyway, use your operating system's file manager."));
		return;
	} else if (to_rename.is_file && to_rename.path.get_extension() != new_name.get_extension()) {
		if (!EditorFileSystem::get_singleton()->get_valid_extensions().find(new_name.get_extension())) {
			EditorNode::get_singleton()->show_warning(TTR("This file extension is not recognized by the editor.\nIf you want to rename it anyway, use your operating system's file manager.\nAfter renaming to an unknown extension, the file won't be shown in the editor anymore."));
			return;
		}
	}

	String old_path = to_rename.path.ends_with("/") ? to_rename.path.substr(0, to_rename.path.length() - 1) : to_rename.path;
	String new_path = old_path.get_base_dir().plus_file(new_name);
	if (old_path == new_path) {
		return;
	}

	if (EditorFileSystem::get_singleton()->is_group_file(old_path)) {
		EditorFileSystem::get_singleton()->move_group_file(old_path, new_path);
	}

	// Present a more user friendly warning for name conflict.
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
#if defined(WINDOWS_ENABLED) || defined(UWP_ENABLED)
	// Workaround case insensitivity on Windows.
	if ((da->file_exists(new_path) || da->dir_exists(new_path)) && new_path.to_lower() != old_path.to_lower()) {
#else
	if (da->file_exists(new_path) || da->dir_exists(new_path)) {
#endif
		EditorNode::get_singleton()->show_warning(TTR("A file or folder with this name already exists."));
		memdelete(da);
		return;
	}
	memdelete(da);

	Map<String, String> file_renames;
	Map<String, String> folder_renames;
	_try_move_item(to_rename, new_path, file_renames, folder_renames);

	int current_tab = editor->get_current_tab();
	_save_scenes_after_move(file_renames); // save scenes before updating
	_update_dependencies_after_move(file_renames);
	_update_resource_paths_after_move(file_renames);
	_update_project_settings_after_move(file_renames);
	_update_favorites_list_after_move(file_renames, folder_renames);

	editor->set_current_tab(current_tab);

	print_verbose("FileSystem: calling rescan.");
	_rescan();

	print_verbose("FileSystem: saving moved scenes.");
	_save_scenes_after_move(file_renames);

	path = new_path;
	current_path->set_text(path);
}

void FileSystemDock::_duplicate_operation_confirm() {
	String new_name = duplicate_dialog_text->get_text().strip_edges();
	if (new_name.length() == 0) {
		EditorNode::get_singleton()->show_warning(TTR("No name provided."));
		return;
	} else if (new_name.find("/") != -1 || new_name.find("\\") != -1 || new_name.find(":") != -1) {
		EditorNode::get_singleton()->show_warning(TTR("Name contains invalid characters."));
		return;
	} else if (new_name[0] == '.') {
		EditorNode::get_singleton()->show_warning(TTR("Name begins with a dot."));
		return;
	}

	String base_dir = to_duplicate.path.get_base_dir();
	// get_base_dir() returns "some/path" if the original path was "some/path/", so work it around.
	if (to_duplicate.path.ends_with("/")) {
		base_dir = base_dir.get_base_dir();
	}

	String new_path = base_dir.plus_file(new_name);

	// Present a more user friendly warning for name conflict
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (da->file_exists(new_path) || da->dir_exists(new_path)) {
		EditorNode::get_singleton()->show_warning(TTR("A file or folder with this name already exists."));
		memdelete(da);
		return;
	}
	memdelete(da);

	_try_duplicate_item(to_duplicate, new_path);

	// Rescan everything.
	print_verbose("FileSystem: calling rescan.");
	_rescan();
}

void FileSystemDock::_move_with_overwrite() {
	_move_operation_confirm(to_move_path, true);
}

Vector<String> FileSystemDock::_check_existing() {
	Vector<String> conflicting_items;
	String &p_to_path = to_move_path;
	for (int i = 0; i < to_move.size(); i++) {
		String ol_pth = to_move[i].path.ends_with("/") ? to_move[i].path.substr(0, to_move[i].path.length() - 1) : to_move[i].path;
		String p_new_path = p_to_path.plus_file(ol_pth.get_file());
		FileOrFolder p_item = to_move[i];

		String old_path = (p_item.is_file || p_item.path.ends_with("/")) ? p_item.path : (p_item.path + "/");
		String new_path = (p_item.is_file || p_new_path.ends_with("/")) ? p_new_path : (p_new_path + "/");

		if ((p_item.is_file && FileAccess::exists(new_path)) ||
				(!p_item.is_file && DirAccess::exists(new_path))) {
			conflicting_items.push_back(old_path);
		}
	}
	return conflicting_items;
}

void FileSystemDock::_move_operation_confirm(const String &p_to_path, bool overwrite) {
	if (!overwrite) {
		to_move_path = p_to_path;
		Vector<String> conflicting_items = _check_existing();
		if (!conflicting_items.empty()) {
			// Ask to do something.
			overwrite_dialog->set_text(vformat(
					TTR("The following files or folders conflict with items in the target location '%s':\n\n%s\n\nDo you wish to overwrite them?"),
					to_move_path,
					String("\n").join(conflicting_items)));
			overwrite_dialog->popup_centered_minsize();
			return;
		}
	}

	// Check groups.
	for (int i = 0; i < to_move.size(); i++) {
		if (to_move[i].is_file && EditorFileSystem::get_singleton()->is_group_file(to_move[i].path)) {
			EditorFileSystem::get_singleton()->move_group_file(to_move[i].path, p_to_path.plus_file(to_move[i].path.get_file()));
		}
	}

	Map<String, String> file_renames;
	Map<String, String> folder_renames;
	bool is_moved = false;
	for (int i = 0; i < to_move.size(); i++) {
		String old_path = to_move[i].path.ends_with("/") ? to_move[i].path.substr(0, to_move[i].path.length() - 1) : to_move[i].path;
		String new_path = p_to_path.plus_file(old_path.get_file());
		if (old_path != new_path) {
			_try_move_item(to_move[i], new_path, file_renames, folder_renames);
			is_moved = true;
		}
	}

	if (is_moved) {
		int current_tab = editor->get_current_tab();
		_save_scenes_after_move(file_renames); // Save scenes before updating.
		_update_dependencies_after_move(file_renames);
		_update_resource_paths_after_move(file_renames);
		_update_project_settings_after_move(file_renames);
		_update_favorites_list_after_move(file_renames, folder_renames);

		editor->set_current_tab(current_tab);

		print_verbose("FileSystem: calling rescan.");
		_rescan();

		print_verbose("FileSystem: saving moved scenes.");
		_save_scenes_after_move(file_renames);

		path = p_to_path;
		current_path->set_text(path);
	}
}

Vector<String> FileSystemDock::_tree_get_selected(bool remove_self_inclusion) {
	// Build a list of selected items with the active one at the first position.
	Vector<String> selected_strings;

	TreeItem *favorites_item = tree->get_root()->get_children();
	TreeItem *active_selected = tree->get_selected();
	if (active_selected && active_selected != favorites_item) {
		selected_strings.push_back(active_selected->get_metadata(0));
	}

	TreeItem *selected = tree->get_root();
	selected = tree->get_next_selected(selected);
	while (selected) {
		if (selected != active_selected && selected != favorites_item) {
			selected_strings.push_back(selected->get_metadata(0));
		}
		selected = tree->get_next_selected(selected);
	}

	if (remove_self_inclusion) {
		selected_strings = _remove_self_included_paths(selected_strings);
	}
	return selected_strings;
}

Vector<String> FileSystemDock::_remove_self_included_paths(Vector<String> selected_strings) {
	// Remove paths or files that are included into another.
	if (selected_strings.size() > 1) {
		selected_strings.sort_custom<NaturalNoCaseComparator>();
		String last_path = "";
		for (int i = 0; i < selected_strings.size(); i++) {
			if (last_path != "" && selected_strings[i].begins_with(last_path)) {
				selected_strings.remove(i);
				i--;
			}
			if (selected_strings[i].ends_with("/")) {
				last_path = selected_strings[i];
			}
		}
	}
	return selected_strings;
}

void FileSystemDock::_tree_rmb_option(int p_option) {
	Vector<String> selected_strings = _tree_get_selected(false);

	// Execute the current option.
	switch (p_option) {
		case FOLDER_EXPAND_ALL:
		case FOLDER_COLLAPSE_ALL: {
			// Expand or collapse the folder
			if (selected_strings.size() == 1) {
				bool is_collapsed = (p_option == FOLDER_COLLAPSE_ALL);

				Vector<TreeItem *> needs_check;
				needs_check.push_back(tree->get_selected());

				while (needs_check.size()) {
					needs_check[0]->set_collapsed(is_collapsed);

					TreeItem *child = needs_check[0]->get_children();
					while (child) {
						needs_check.push_back(child);
						child = child->get_next();
					}

					needs_check.remove(0);
				}
			}
		} break;
		default: {
			_file_option(p_option, selected_strings);
		} break;
	}
}

void FileSystemDock::_file_list_rmb_option(int p_option) {
	Vector<int> selected_id = files->get_selected_items();
	Vector<String> selected;
	for (int i = 0; i < selected_id.size(); i++) {
		selected.push_back(files->get_item_metadata(selected_id[i]));
	}
	_file_option(p_option, selected);
}

void FileSystemDock::_file_option(int p_option, const Vector<String> &p_selected) {
	// The first one should be the active item.

	switch (p_option) {
		case FILE_SHOW_IN_EXPLORER: {
			// Show the file/folder in the OS explorer.
			String fpath = path;
			if (path == "Favorites") {
				fpath = p_selected[0];
			}

			if (!fpath.ends_with("/")) {
				fpath = fpath.get_base_dir();
			}
			String dir = ProjectSettings::get_singleton()->globalize_path(fpath);
			OS::get_singleton()->shell_open(String("file://") + dir);
		} break;

		case FILE_OPEN: {
			// Open folders.
			TreeItem *selected = tree->get_root();
			selected = tree->get_next_selected(selected);
			while (selected) {
				if (p_selected.find(selected->get_metadata(0)) >= 0) {
					selected->set_collapsed(false);
				}
				selected = tree->get_next_selected(selected);
			}
			// Open the file.
			for (int i = 0; i < p_selected.size(); i++) {
				_select_file(p_selected[i]);
			}
		} break;

		case FILE_INHERIT: {
			// Create a new scene inherited from the selected one.
			if (p_selected.size() == 1) {
				emit_signal("inherit", p_selected[0]);
			}
		} break;

		case FILE_MAIN_SCENE: {
			// Set as main scene with selected scene file.
			if (p_selected.size() == 1) {
				ProjectSettings::get_singleton()->set("application/run/main_scene", p_selected[0]);
				ProjectSettings::get_singleton()->save();
				_update_tree(_compute_uncollapsed_paths());
				_update_file_list(true);
			}
		} break;

		case FILE_INSTANCE: {
			// Instance all selected scenes.
			Vector<String> paths;
			for (int i = 0; i < p_selected.size(); i++) {
				String fpath = p_selected[i];
				if (EditorFileSystem::get_singleton()->get_file_type(fpath) == "PackedScene") {
					paths.push_back(fpath);
				}
			}
			if (!paths.empty()) {
				emit_signal("instance", paths);
			}
		} break;

		case FILE_ADD_FAVORITE: {
			// Add the files from favorites.
			Vector<String> favorites = EditorSettings::get_singleton()->get_favorites();
			for (int i = 0; i < p_selected.size(); i++) {
				if (favorites.find(p_selected[i]) == -1) {
					favorites.push_back(p_selected[i]);
				}
			}
			EditorSettings::get_singleton()->set_favorites(favorites);
			_update_tree(_compute_uncollapsed_paths());
		} break;

		case FILE_REMOVE_FAVORITE: {
			// Remove the files from favorites.
			Vector<String> favorites = EditorSettings::get_singleton()->get_favorites();
			for (int i = 0; i < p_selected.size(); i++) {
				favorites.erase(p_selected[i]);
			}
			EditorSettings::get_singleton()->set_favorites(favorites);
			_update_tree(_compute_uncollapsed_paths());
			if (path == "Favorites") {
				_update_file_list(true);
			}
		} break;

		case FILE_DEPENDENCIES: {
			// Checkout the file dependencies.
			if (!p_selected.empty()) {
				String fpath = p_selected[0];
				deps_editor->edit(fpath);
			}
		} break;

		case FILE_OWNERS: {
			// Checkout the file owners.
			if (!p_selected.empty()) {
				String fpath = p_selected[0];
				owners_editor->show(fpath);
			}
		} break;

		case FILE_MOVE: {
			// Move the files to a given location.
			to_move.clear();
			Vector<String> collapsed_paths = _remove_self_included_paths(p_selected);
			for (int i = collapsed_paths.size() - 1; i >= 0; i--) {
				String fpath = collapsed_paths[i];
				if (fpath != "res://") {
					to_move.push_back(FileOrFolder(fpath, !fpath.ends_with("/")));
				}
			}
			if (to_move.size() > 0) {
				move_dialog->popup_centered_ratio();
			}
		} break;

		case FILE_RENAME: {
			// Rename the active file.
			if (!p_selected.empty()) {
				to_rename.path = p_selected[0];
				if (to_rename.path != "res://") {
					to_rename.is_file = !to_rename.path.ends_with("/");
					if (to_rename.is_file) {
						String name = to_rename.path.get_file();
						rename_dialog->set_title(TTR("Renaming file:") + " " + name);
						rename_dialog_text->set_text(name);
						rename_dialog_text->select(0, name.rfind("."));
					} else {
						String name = to_rename.path.substr(0, to_rename.path.length() - 1).get_file();
						rename_dialog->set_title(TTR("Renaming folder:") + " " + name);
						rename_dialog_text->set_text(name);
						rename_dialog_text->select(0, name.length());
					}
					rename_dialog->popup_centered_minsize(Size2(250, 80) * EDSCALE);
					rename_dialog_text->grab_focus();
				}
			}
		} break;

		case FILE_REMOVE: {
			// Remove the selected files.
			Vector<String> remove_files;
			Vector<String> remove_folders;
			Vector<String> collapsed_paths = _remove_self_included_paths(p_selected);

			for (int i = 0; i < collapsed_paths.size(); i++) {
				String fpath = collapsed_paths[i];
				if (fpath != "res://") {
					if (fpath.ends_with("/")) {
						remove_folders.push_back(fpath);
					} else {
						remove_files.push_back(fpath);
					}
				}
			}

			if (remove_files.size() + remove_folders.size() > 0) {
				remove_dialog->show(remove_folders, remove_files);
			}
		} break;

		case FILE_DUPLICATE: {
			// Duplicate the selected files.
			for (int i = 0; i < p_selected.size(); i++) {
				to_duplicate.path = p_selected[i];
				to_duplicate.is_file = !to_duplicate.path.ends_with("/");
				if (to_duplicate.is_file) {
					String name = to_duplicate.path.get_file();
					duplicate_dialog->set_title(TTR("Duplicating file:") + " " + name);
					duplicate_dialog_text->set_text(name);
					duplicate_dialog_text->select(0, name.rfind("."));
				} else {
					String name = to_duplicate.path.substr(0, to_duplicate.path.length() - 1).get_file();
					duplicate_dialog->set_title(TTR("Duplicating folder:") + " " + name);
					duplicate_dialog_text->set_text(name);
					duplicate_dialog_text->select(0, name.length());
				}
				duplicate_dialog->popup_centered_minsize(Size2(250, 80) * EDSCALE);
				duplicate_dialog_text->grab_focus();
			}
		} break;

		case FILE_INFO: {
		} break;

		case FILE_REIMPORT: {
			// Reimport all selected files.
			Vector<String> reimport;
			for (int i = 0; i < p_selected.size(); i++) {
				reimport.push_back(p_selected[i]);
			}

			ERR_FAIL_COND_MSG(reimport.size() == 0, "You need to select files to reimport them.");
		} break;

		case FILE_NEW_FOLDER: {
			String directory = path;
			if (!directory.ends_with("/")) {
				directory = directory.get_base_dir();
			}
			make_dir_dialog->config(directory);
			make_dir_dialog->popup_centered(Size2(1, 1));
		} break;

		case FILE_NEW_SCENE: {
			String directory = path;
			if (!directory.ends_with("/")) {
				directory = directory.get_base_dir();
			}
			make_scene_dialog->config(directory);
			make_scene_dialog->popup_centered();
		} break;

		case FILE_NEW_SCRIPT: {
			String fpath = path;
			if (!fpath.ends_with("/")) {
				fpath = fpath.get_base_dir();
			}
			make_script_dialog->config("Node", fpath.plus_file("new_script.gd"), false, false);
			make_script_dialog->popup_centered();
		} break;

		case FILE_COPY_PATH: {
			if (!p_selected.empty()) {
				String fpath = p_selected[0];
				OS::get_singleton()->set_clipboard(fpath);
			}
		} break;

		case FILE_NEW_RESOURCE: {
			new_resource_dialog->popup_create(true);
		} break;

		default:
			// context menu plugins option selected.
			EditorContextMenu::options_pressed(EditorContextMenu::CONTEXT_SLOT_FILESYSTEM, p_option, p_selected);
			break;
	}
}

void FileSystemDock::_resource_created() const {
	Variant c = new_resource_dialog->instance_selected();

	ERR_FAIL_COND(!c);
	Resource *r = Object::cast_to<Resource>(c);
	ERR_FAIL_COND(!r);

	PackedScene *scene = Object::cast_to<PackedScene>(r);
	if (scene) {
		Node *node = memnew(Node);
		node->set_name("Node");
		scene->pack(node);
		memdelete(node);
	}

	editor->push_item(r);

	String fpath = path;
	if (!fpath.ends_with("/")) {
		fpath = fpath.get_base_dir();
	}

	editor->save_resource_as(RES(r), fpath);
}

void FileSystemDock::_focus_current_search_box() {
	LineEdit *current_search_box = nullptr;
	if (display_mode == DISPLAY_MODE_TREE_ONLY) {
		current_search_box = tree_search_box;
	} else if (display_mode == DISPLAY_MODE_SPLIT) {
		current_search_box = file_list_search_box;
	}

	if (current_search_box) {
		current_search_box->grab_focus();
		current_search_box->select_all();
	}
}

void FileSystemDock::_search_changed(const String &p_text, const Control *p_from) {
	if (searched_string.length() == 0) {
		// Register the uncollapsed paths before they change.
		uncollapsed_paths_before_search = _compute_uncollapsed_paths();
	}

	searched_string = p_text.to_lower();

	if (p_from == tree_search_box) {
		file_list_search_box->set_text(searched_string);
	} else { // File_list_search_box.
		tree_search_box->set_text(searched_string);
	}

	bool unfold_path = (p_text == String() && path != String());
	switch (display_mode) {
		case DISPLAY_MODE_TREE_ONLY: {
			_update_tree(searched_string.length() == 0 ? uncollapsed_paths_before_search : Vector<String>(), false, false, unfold_path);
		} break;
		case DISPLAY_MODE_SPLIT: {
			_update_file_list(false);
			_update_tree(searched_string.length() == 0 ? uncollapsed_paths_before_search : Vector<String>(), false, false, unfold_path);
		} break;
	}
}

void FileSystemDock::_rescan() {
	_set_scanning_mode();
	EditorFileSystem::get_singleton()->scan();
}

void FileSystemDock::_toggle_split_mode(bool p_active) {
	set_display_mode(p_active ? DISPLAY_MODE_SPLIT : DISPLAY_MODE_TREE_ONLY);
	emit_signal("display_mode_changed");
}

void FileSystemDock::fix_dependencies(const String &p_for_file) {
	deps_editor->edit(p_for_file);
}

void FileSystemDock::focus_on_filter() {
	file_list_search_box->grab_focus();
}

void FileSystemDock::set_file_list_display_mode(FileListDisplayMode p_mode) {
	if (p_mode == file_list_display_mode) {
		return;
	}

	_toggle_file_display();
}

Variant FileSystemDock::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	bool all_favorites = true;
	bool all_not_favorites = true;

	Vector<String> paths;

	if (p_from == tree) {
		// Check if the first selected is in favorite.
		TreeItem *selected = tree->get_next_selected(tree->get_root());
		while (selected) {
			TreeItem *favorites_item = tree->get_root()->get_children();
			if (selected == favorites_item) {
				// The "Favorites" item is not draggable.
				return Variant();
			}

			bool is_favorite = selected->get_parent() != nullptr && tree->get_root()->get_children() == selected->get_parent();
			all_favorites &= is_favorite;
			all_not_favorites &= !is_favorite;
			selected = tree->get_next_selected(selected);
		}
		if (!all_not_favorites) {
			paths = _tree_get_selected(false);
		} else {
			paths = _tree_get_selected();
		}
	} else if (p_from == files) {
		for (int i = 0; i < files->get_item_count(); i++) {
			if (files->is_selected(i)) {
				paths.push_back(files->get_item_metadata(i));
			}
		}
		all_favorites = false;
		all_not_favorites = true;
	}

	if (paths.empty()) {
		return Variant();
	}

	Dictionary drag_data = EditorNode::get_singleton()->drag_files_and_dirs(paths, p_from);
	if (!all_not_favorites) {
		drag_data["favorite"] = all_favorites ? "all" : "mixed";
	}
	return drag_data;
}

bool FileSystemDock::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary drag_data = p_data;

	if (drag_data.has("favorite")) {
		if (String(drag_data["favorite"]) != "all") {
			return false;
		}

		// Moving favorite around.
		TreeItem *ti = tree->get_item_at_position(p_point);
		if (!ti) {
			return false;
		}

		int drop_section = tree->get_drop_section_at_position(p_point);
		TreeItem *favorites_item = tree->get_root()->get_children();

		TreeItem *resources_item = favorites_item->get_next();

		if (ti == favorites_item) {
			return (drop_section == 1); // The parent, first fav.
		}
		if (ti->get_parent() && favorites_item == ti->get_parent()) {
			return true; // A favorite
		}
		if (ti == resources_item) {
			return (drop_section == -1); // The tree, last fav.
		}

		return false;
	}

	if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
		// Move resources.
		String to_dir;
		bool favorite;
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);
		return !to_dir.empty();
	}

	if (drag_data.has("type") && (String(drag_data["type"]) == "files" || String(drag_data["type"]) == "files_and_dirs")) {
		// Move files or dir.
		String to_dir;
		bool favorite;
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);

		if (favorite) {
			return true;
		}

		if (to_dir.empty()) {
			return false;
		}

		// Attempting to move a folder into itself will fail later,
		// rather than bring up a message don't try to do it in the first place.
		to_dir = to_dir.ends_with("/") ? to_dir : (to_dir + "/");
		Vector<String> fnames = drag_data["files"];
		for (int i = 0; i < fnames.size(); ++i) {
			if (fnames[i].ends_with("/") && to_dir.begins_with(fnames[i])) {
				return false;
			}
		}

		return true;
	}

	if (drag_data.has("type") && String(drag_data["type"]) == "nodes") {
		// Save branch as scene.
		String to_dir;
		bool favorite;
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);
		return !favorite && Array(drag_data["nodes"]).size() == 1;
	}

	return false;
}

void FileSystemDock::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}
	Dictionary drag_data = p_data;

	Vector<String> dirs = EditorSettings::get_singleton()->get_favorites();

	if (drag_data.has("favorite")) {
		if (String(drag_data["favorite"]) != "all") {
			return;
		}
		// Moving favorite around.
		TreeItem *ti = tree->get_item_at_position(p_point);
		if (!ti) {
			return;
		}
		int drop_section = tree->get_drop_section_at_position(p_point);

		int drop_position;
		Vector<String> files = drag_data["files"];
		TreeItem *favorites_item = tree->get_root()->get_children();
		TreeItem *resources_item = favorites_item->get_next();

		if (ti == favorites_item) {
			// Drop on the favorite folder.
			drop_position = 0;
		} else if (ti == resources_item) {
			// Drop on the resource item.
			drop_position = dirs.size();
		} else {
			// Drop in the list.
			drop_position = dirs.find(ti->get_metadata(0));
			if (drop_section == 1) {
				drop_position++;
			}
		}

		// Remove dragged favorites.
		Vector<int> to_remove;
		int offset = 0;
		for (int i = 0; i < files.size(); i++) {
			int to_remove_pos = dirs.find(files[i]);
			to_remove.push_back(to_remove_pos);
			if (to_remove_pos < drop_position) {
				offset++;
			}
		}
		drop_position -= offset;
		to_remove.sort();
		for (int i = 0; i < to_remove.size(); i++) {
			dirs.remove(to_remove[i] - i);
		}

		// Re-add them at the right position.
		for (int i = 0; i < files.size(); i++) {
			dirs.insert(drop_position, files[i]);
			drop_position++;
		}

		EditorSettings::get_singleton()->set_favorites(dirs);
		_update_tree(_compute_uncollapsed_paths());

		if (display_mode == DISPLAY_MODE_SPLIT && path == "Favorites") {
			_update_file_list(true);
		}
		return;
	}

	if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
		// Moving resource.
		Ref<Resource> res = drag_data["resource"];
		String to_dir;
		bool favorite;
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);
		if (res.is_valid() && !to_dir.empty()) {
			EditorNode::get_singleton()->push_item(res.ptr());
			EditorNode::get_singleton()->save_resource_as(res, to_dir);
		}
	}

	if (drag_data.has("type") && (String(drag_data["type"]) == "files" || String(drag_data["type"]) == "files_and_dirs")) {
		// Move files or add to favorites.
		String to_dir;
		bool favorite;
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);
		if (!to_dir.empty()) {
			Vector<String> fnames = drag_data["files"];
			to_move.clear();
			for (int i = 0; i < fnames.size(); i++) {
				if (fnames[i].get_base_dir() != to_dir) {
					to_move.push_back(FileOrFolder(fnames[i], !fnames[i].ends_with("/")));
				}
			}
			if (!to_move.empty()) {
				if (Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
					for (int i = 0; i < to_move.size(); i++) {
						String new_path;
						String new_path_base;

						if (to_move[i].is_file) {
							new_path = to_dir.plus_file(to_move[i].path.get_file());
							new_path_base = new_path.get_basename() + " (%d)." + new_path.get_extension();
						} else {
							Vector<String> path_split = to_move[i].path.split("/");
							new_path = to_dir.plus_file(path_split[path_split.size() - 2]);
							new_path_base = new_path + " (%d)";
						}

						int exist_counter = 1;
						DirAccessRef da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
						while (da->file_exists(new_path) || da->dir_exists(new_path)) {
							exist_counter++;
							new_path = vformat(new_path_base, exist_counter);
						}
						_try_duplicate_item(to_move[i], new_path);
					}
					_rescan();
				} else {
					_move_operation_confirm(to_dir);
				}
			}
		} else if (favorite) {
			// Add the files from favorites.
			Vector<String> fnames = drag_data["files"];
			Vector<String> favorites = EditorSettings::get_singleton()->get_favorites();
			for (int i = 0; i < fnames.size(); i++) {
				if (favorites.find(fnames[i]) == -1) {
					favorites.push_back(fnames[i]);
				}
			}
			EditorSettings::get_singleton()->set_favorites(favorites);
			_update_tree(_compute_uncollapsed_paths());
		}
	}

	if (drag_data.has("type") && String(drag_data["type"]) == "nodes") {
		String to_dir;
		bool favorite;
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);
		EditorNode::get_singleton()->get_scene_tree_dock()->save_branch_to_file(to_dir);
	}
}

void FileSystemDock::_get_drag_target_folder(String &target, bool &target_favorites, const Point2 &p_point, Control *p_from) const {
	target = String();
	target_favorites = false;

	// In the file list.
	if (p_from == files) {
		int pos = files->get_item_at_position(p_point, true);
		if (pos == -1) {
			return;
		}

		String ltarget = files->get_item_metadata(pos);
		target = ltarget.ends_with("/") ? ltarget : path.get_base_dir();
		return;
	}

	// In the tree.
	if (p_from == tree) {
		TreeItem *ti = tree->get_item_at_position(p_point);
		int section = tree->get_drop_section_at_position(p_point);
		if (ti) {
			// Check the favorites first.
			if (ti == tree->get_root()->get_children() && section >= 0) {
				target_favorites = true;
				return;
			} else if (ti->get_parent() == tree->get_root()->get_children()) {
				target_favorites = true;
				return;
			} else {
				String fpath = ti->get_metadata(0);
				if (section == 0) {
					if (fpath.ends_with("/")) {
						// We drop on a folder.
						target = fpath;
						return;
					} else {
						// We drop on the folder that the target file is in.
						target = fpath.get_base_dir();
						return;
					}
				} else {
					if (ti->get_parent() != tree->get_root()->get_children()) {
						// Not in the favorite section.
						if (fpath != "res://") {
							// We drop between two files
							if (fpath.ends_with("/")) {
								fpath = fpath.substr(0, fpath.length() - 1);
							}
							target = fpath.get_base_dir();
							return;
						}
					}
				}
			}
		}
	}
}

void FileSystemDock::_file_and_folders_fill_popup(PopupMenu *p_popup, Vector<String> p_paths, bool p_display_path_dependent_options) {
	// Add options for files and folders.
	ERR_FAIL_COND_MSG(p_paths.empty(), "Path cannot be empty.");

	Vector<String> filenames;
	Vector<String> foldernames;

	Vector<String> favorites = EditorSettings::get_singleton()->get_favorites();

	bool all_files = true;
	bool all_files_scenes = true;
	bool all_folders = true;
	bool all_favorites = true;
	bool all_not_favorites = true;

	for (int i = 0; i < p_paths.size(); i++) {
		String fpath = p_paths[i];
		if (fpath.ends_with("/")) {
			foldernames.push_back(fpath);
			all_files = false;
		} else {
			filenames.push_back(fpath);
			all_folders = false;
			all_files_scenes &= (EditorFileSystem::get_singleton()->get_file_type(fpath) == "PackedScene");
		}

		// Check if in favorites.
		bool found = false;
		for (int j = 0; j < favorites.size(); j++) {
			if (favorites[j] == fpath) {
				found = true;
				break;
			}
		}
		if (found) {
			all_not_favorites = false;
		} else {
			all_favorites = false;
		}
	}

	if (all_files) {
		if (all_files_scenes) {
			if (filenames.size() == 1) {
				p_popup->add_icon_item(get_icon("Load", "EditorIcons"), TTR("Open Scene"), FILE_OPEN);
				p_popup->add_icon_item(get_icon("CreateNewSceneFrom", "EditorIcons"), TTR("New Inherited Scene"), FILE_INHERIT);
				if (ProjectSettings::get_singleton()->get("application/run/main_scene") != filenames[0]) {
					p_popup->add_icon_item(get_icon("PlayScene", "EditorIcons"), TTR("Set As Main Scene"), FILE_MAIN_SCENE);
				}
			} else {
				p_popup->add_icon_item(get_icon("Load", "EditorIcons"), TTR("Open Scenes"), FILE_OPEN);
			}
			p_popup->add_icon_item(get_icon("Instance", "EditorIcons"), TTR("Instance"), FILE_INSTANCE);
			p_popup->add_separator();
		} else if (filenames.size() == 1) {
			p_popup->add_icon_item(get_icon("Load", "EditorIcons"), TTR("Open"), FILE_OPEN);
			p_popup->add_separator();
		}
	}

	if (p_paths.size() >= 1) {
		if (!all_favorites) {
			p_popup->add_icon_item(get_icon("Favorites", "EditorIcons"), TTR("Add to Favorites"), FILE_ADD_FAVORITE);
		}
		if (!all_not_favorites) {
			p_popup->add_icon_item(get_icon("NonFavorite", "EditorIcons"), TTR("Remove from Favorites"), FILE_REMOVE_FAVORITE);
		}
		p_popup->add_separator();
	}

	if (all_files) {
		if (filenames.size() == 1) {
			p_popup->add_item(TTR("Edit Dependencies..."), FILE_DEPENDENCIES);
			p_popup->add_item(TTR("View Owners..."), FILE_OWNERS);
			p_popup->add_separator();
		}

	} else if (all_folders && foldernames.size() > 0) {
		p_popup->add_icon_item(get_icon("Load", "EditorIcons"), TTR("Open"), FILE_OPEN);
		p_popup->add_separator();
	}

	if (p_paths.size() == 1) {
		p_popup->add_icon_shortcut(get_icon("ActionCopy", "EditorIcons"), ED_GET_SHORTCUT("filesystem_dock/copy_path"), FILE_COPY_PATH);
		if (p_paths[0] != "res://") {
			p_popup->add_icon_shortcut(get_icon("Rename", "EditorIcons"), ED_GET_SHORTCUT("filesystem_dock/rename"), FILE_RENAME);
			p_popup->add_icon_shortcut(get_icon("Duplicate", "EditorIcons"), ED_GET_SHORTCUT("filesystem_dock/duplicate"), FILE_DUPLICATE);
		}
	}

	if (p_paths.size() > 1 || p_paths[0] != "res://") {
		p_popup->add_icon_item(get_icon("MoveUp", "EditorIcons"), TTR("Move To..."), FILE_MOVE);
		p_popup->add_icon_shortcut(get_icon("Remove", "EditorIcons"), ED_GET_SHORTCUT("filesystem_dock/delete"), FILE_REMOVE);
	}

	if (p_paths.size() == 1) {
		p_popup->add_separator();
		if (p_display_path_dependent_options) {
			p_popup->add_icon_item(get_icon("Folder", "EditorIcons"), TTR("New Folder..."), FILE_NEW_FOLDER);
			p_popup->add_icon_item(get_icon("PackedScene", "EditorIcons"), TTR("New Scene..."), FILE_NEW_SCENE);
			p_popup->add_icon_item(get_icon("Script", "EditorIcons"), TTR("New Script..."), FILE_NEW_SCRIPT);
			p_popup->add_icon_item(get_icon("Object", "EditorIcons"), TTR("New Resource..."), FILE_NEW_RESOURCE);
			p_popup->add_separator();
		}

		String fpath = p_paths[0];
		String item_text = fpath.ends_with("/") ? TTR("Open in File Manager") : TTR("Show in File Manager");
		p_popup->add_icon_item(get_icon("Filesystem", "EditorIcons"), item_text, FILE_SHOW_IN_EXPLORER);
		path = fpath;
	}

	EditorContextMenu::handle_plugins(EditorContextMenu::CONTEXT_SLOT_FILESYSTEM, p_paths, p_popup);
}

void FileSystemDock::_tree_rmb_select(const Vector2 &p_pos) {
	// Right click is pressed in the tree.
	Vector<String> paths = _tree_get_selected(false);

	if (paths.size() == 1) {
		if (paths[0].ends_with("/")) {
			tree_popup->add_icon_item(get_icon("GuiTreeArrowDown", "EditorIcons"), TTR("Expand All"), FOLDER_EXPAND_ALL);
			tree_popup->add_icon_item(get_icon("GuiTreeArrowRight", "EditorIcons"), TTR("Collapse All"), FOLDER_COLLAPSE_ALL);
			tree_popup->add_separator();
		}
	}

	// Popup.
	if (!paths.empty()) {
		tree_popup->clear();
		tree_popup->set_size(Size2(1, 1));
		_file_and_folders_fill_popup(tree_popup, paths);
		tree_popup->set_position(tree->get_global_position() + p_pos);
		tree_popup->popup();
	}
}

void FileSystemDock::_tree_rmb_empty(const Vector2 &p_pos) {
	// Right click is pressed in the empty space of the tree.
	path = "res://";
	tree_popup->clear();
	tree_popup->set_size(Size2(1, 1));
	tree_popup->add_icon_item(get_icon("Folder", "EditorIcons"), TTR("New Folder..."), FILE_NEW_FOLDER);
	tree_popup->add_icon_item(get_icon("PackedScene", "EditorIcons"), TTR("New Scene..."), FILE_NEW_SCENE);
	tree_popup->add_icon_item(get_icon("Script", "EditorIcons"), TTR("New Script..."), FILE_NEW_SCRIPT);
	tree_popup->add_icon_item(get_icon("Object", "EditorIcons"), TTR("New Resource..."), FILE_NEW_RESOURCE);
	tree_popup->add_separator();
	tree_popup->add_icon_item(get_icon("Filesystem", "EditorIcons"), TTR("Open in File Manager"), FILE_SHOW_IN_EXPLORER);

	tree_popup->set_position(tree->get_global_position() + p_pos);
	tree_popup->popup();
}

void FileSystemDock::_tree_empty_selected() {
	tree->deselect_all();
}

void FileSystemDock::_file_list_rmb_select(int p_item, const Vector2 &p_pos) {
	// Right click is pressed in the file list.
	Vector<String> paths;
	for (int i = 0; i < files->get_item_count(); i++) {
		if (!files->is_selected(i)) {
			continue;
		}
		if (files->get_item_text(p_item) == "..") {
			files->unselect(i);
			continue;
		}
		paths.push_back(files->get_item_metadata(i));
	}

	// Popup.
	if (!paths.empty()) {
		file_list_popup->clear();
		file_list_popup->set_size(Size2(1, 1));
		_file_and_folders_fill_popup(file_list_popup, paths, searched_string.length() == 0);
		file_list_popup->set_position(files->get_global_position() + p_pos);
		file_list_popup->popup();
	}
}

void FileSystemDock::_file_list_rmb_pressed(const Vector2 &p_pos) {
	// Right click on empty space for file list.
	if (searched_string.length() > 0) {
		return;
	}

	path = current_path->get_text();

	file_list_popup->clear();
	file_list_popup->set_size(Size2(1, 1));

	file_list_popup->add_icon_item(get_icon("Folder", "EditorIcons"), TTR("New Folder..."), FILE_NEW_FOLDER);
	file_list_popup->add_icon_item(get_icon("PackedScene", "EditorIcons"), TTR("New Scene..."), FILE_NEW_SCENE);
	file_list_popup->add_icon_item(get_icon("Script", "EditorIcons"), TTR("New Script..."), FILE_NEW_SCRIPT);
	file_list_popup->add_icon_item(get_icon("Object", "EditorIcons"), TTR("New Resource..."), FILE_NEW_RESOURCE);
	file_list_popup->add_separator();
	file_list_popup->add_icon_item(get_icon("Filesystem", "EditorIcons"), TTR("Open in File Manager"), FILE_SHOW_IN_EXPLORER);
	file_list_popup->set_position(files->get_global_position() + p_pos);
	file_list_popup->popup();
}

void FileSystemDock::select_file(const String &p_file) {
	_navigate_to_path(p_file);
}

void FileSystemDock::_file_multi_selected(int p_index, bool p_selected) {
	// Set the path to the current focused item.
	int current = files->get_current();
	if (current == p_index) {
		String fpath = files->get_item_metadata(current);
		if (!fpath.ends_with("/")) {
			path = fpath;
			if (display_mode == DISPLAY_MODE_SPLIT) {
				_update_tree(_compute_uncollapsed_paths());
			}
		}
	}

	// Update the import dock.
	import_dock_needs_update = true;
	call_deferred("_update_import_dock");
}

void FileSystemDock::_tree_gui_input(Ref<InputEvent> p_event) {
	if (get_viewport()->get_modal_stack_top()) {
		return; // Ignore because of modal window.
	}

	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (ED_IS_SHORTCUT("filesystem_dock/duplicate", p_event)) {
			_tree_rmb_option(FILE_DUPLICATE);
		} else if (ED_IS_SHORTCUT("filesystem_dock/copy_path", p_event)) {
			_tree_rmb_option(FILE_COPY_PATH);
		} else if (ED_IS_SHORTCUT("filesystem_dock/delete", p_event)) {
			_tree_rmb_option(FILE_REMOVE);
		} else if (ED_IS_SHORTCUT("filesystem_dock/rename", p_event)) {
			_tree_rmb_option(FILE_RENAME);
		} else if (ED_IS_SHORTCUT("filesystem_dock/open_search", p_event)) {
			_focus_current_search_box();
		} else {
			return;
		}

		accept_event();
	}
}

void FileSystemDock::_file_list_gui_input(Ref<InputEvent> p_event) {
	if (get_viewport()->get_modal_stack_top()) {
		return; // Ignore because of modal window.
	}

	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (ED_IS_SHORTCUT("filesystem_dock/duplicate", p_event)) {
			_file_list_rmb_option(FILE_DUPLICATE);
		} else if (ED_IS_SHORTCUT("filesystem_dock/copy_path", p_event)) {
			_file_list_rmb_option(FILE_COPY_PATH);
		} else if (ED_IS_SHORTCUT("filesystem_dock/delete", p_event)) {
			_file_list_rmb_option(FILE_REMOVE);
		} else if (ED_IS_SHORTCUT("filesystem_dock/rename", p_event)) {
			_file_list_rmb_option(FILE_RENAME);
		} else if (ED_IS_SHORTCUT("filesystem_dock/open_search", p_event)) {
			_focus_current_search_box();
		} else {
			return;
		}

		accept_event();
	}
}

void FileSystemDock::_update_import_dock() {
	if (!import_dock_needs_update) {
		return;
	}

	// List selected.
	Vector<String> selected;
	if (display_mode == DISPLAY_MODE_TREE_ONLY) {
		// Use the tree
		selected = _tree_get_selected();

	} else {
		// Use the file list.
		for (int i = 0; i < files->get_item_count(); i++) {
			if (!files->is_selected(i)) {
				continue;
			}

			selected.push_back(files->get_item_metadata(i));
		}
	}

	// Check import.
	Vector<String> imports;
	String import_type;
	for (int i = 0; i < selected.size(); i++) {
		String fpath = selected[i];

		if (fpath.ends_with("/")) {
			imports.clear();
			break;
		}

		if (!FileAccess::exists(fpath + ".import")) {
			imports.clear();
			break;
		}
		Ref<ConfigFile> cf;
		cf.instance();
		Error err = cf->load(fpath + ".import");
		if (err != OK) {
			imports.clear();
			break;
		}

		String type;
		if (cf->has_section_key("remap", "type")) {
			type = cf->get_value("remap", "type");
		}
		if (import_type == "") {
			import_type = type;
		} else if (import_type != type) {
			// All should be the same type.
			imports.clear();
			break;
		}
		imports.push_back(fpath);
	}

	if (imports.size() == 0) {
		EditorNode::get_singleton()->get_import_dock()->clear();
	} else if (imports.size() == 1) {
		EditorNode::get_singleton()->get_import_dock()->set_edit_path(imports[0]);
	} else {
		EditorNode::get_singleton()->get_import_dock()->set_edit_multiple_paths(imports);
	}

	import_dock_needs_update = false;
}

void FileSystemDock::_feature_profile_changed() {
	_update_display_mode(true);
}

void FileSystemDock::set_file_sort(FileSortOption p_file_sort) {
	for (int i = 0; i != FILE_SORT_MAX; i++) {
		tree_button_sort->get_popup()->set_item_checked(i, (i == (int)p_file_sort));
		file_list_button_sort->get_popup()->set_item_checked(i, (i == (int)p_file_sort));
	}
	file_sort = p_file_sort;

	// Update everything needed.
	_update_tree(_compute_uncollapsed_paths());
	_update_file_list(true);
}

void FileSystemDock::_file_sort_popup(int p_id) {
	set_file_sort((FileSortOption)p_id);
}

MenuButton *FileSystemDock::_create_file_menu_button() {
	MenuButton *button = memnew(MenuButton);
	button->set_flat(true);
	button->set_tooltip(TTR("Sort files"));

	PopupMenu *p = button->get_popup();
	p->connect("id_pressed", this, "_file_sort_popup");
	p->add_radio_check_item(TTR("Sort by Name (Ascending)"), FILE_SORT_NAME);
	p->add_radio_check_item(TTR("Sort by Name (Descending)"), FILE_SORT_NAME_REVERSE);
	p->add_radio_check_item(TTR("Sort by Type (Ascending)"), FILE_SORT_TYPE);
	p->add_radio_check_item(TTR("Sort by Type (Descending)"), FILE_SORT_TYPE_REVERSE);
	p->add_radio_check_item(TTR("Sort by Last Modified"), FILE_SORT_MODIFIED_TIME);
	p->add_radio_check_item(TTR("Sort by First Modified"), FILE_SORT_MODIFIED_TIME_REVERSE);
	p->set_item_checked(file_sort, true);
	return button;
}

void FileSystemDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_file_list_gui_input"), &FileSystemDock::_file_list_gui_input);
	ClassDB::bind_method(D_METHOD("_tree_gui_input"), &FileSystemDock::_tree_gui_input);

	ClassDB::bind_method(D_METHOD("_update_tree"), &FileSystemDock::_update_tree);
	ClassDB::bind_method(D_METHOD("_rescan"), &FileSystemDock::_rescan);

	ClassDB::bind_method(D_METHOD("_file_sort_popup"), &FileSystemDock::_file_sort_popup);

	ClassDB::bind_method(D_METHOD("_toggle_split_mode"), &FileSystemDock::_toggle_split_mode);

	ClassDB::bind_method(D_METHOD("_tree_rmb_option", "option"), &FileSystemDock::_tree_rmb_option);
	ClassDB::bind_method(D_METHOD("_tree_rmb_select"), &FileSystemDock::_tree_rmb_select);
	ClassDB::bind_method(D_METHOD("_tree_empty_selected"), &FileSystemDock::_tree_empty_selected);

	ClassDB::bind_method(D_METHOD("_file_list_rmb_option", "option"), &FileSystemDock::_file_list_rmb_option);
	ClassDB::bind_method(D_METHOD("_file_list_rmb_select"), &FileSystemDock::_file_list_rmb_select);
	ClassDB::bind_method(D_METHOD("_file_list_rmb_pressed"), &FileSystemDock::_file_list_rmb_pressed);
	ClassDB::bind_method(D_METHOD("_tree_rmb_empty"), &FileSystemDock::_tree_rmb_empty);

	ClassDB::bind_method(D_METHOD("_file_removed"), &FileSystemDock::_file_removed);
	ClassDB::bind_method(D_METHOD("_folder_removed"), &FileSystemDock::_folder_removed);

	ClassDB::bind_method(D_METHOD("_file_list_thumbnail_done"), &FileSystemDock::_file_list_thumbnail_done);
	ClassDB::bind_method(D_METHOD("_tree_thumbnail_done"), &FileSystemDock::_tree_thumbnail_done);
	ClassDB::bind_method(D_METHOD("_file_list_activate_file"), &FileSystemDock::_file_list_activate_file);
	ClassDB::bind_method(D_METHOD("_tree_activate_file"), &FileSystemDock::_tree_activate_file);
	ClassDB::bind_method(D_METHOD("_select_file"), &FileSystemDock::_select_file);
	ClassDB::bind_method(D_METHOD("_navigate_to_path"), &FileSystemDock::_navigate_to_path, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("_toggle_file_display"), &FileSystemDock::_toggle_file_display);
	ClassDB::bind_method(D_METHOD("_fw_history"), &FileSystemDock::_fw_history);
	ClassDB::bind_method(D_METHOD("_bw_history"), &FileSystemDock::_bw_history);
	ClassDB::bind_method(D_METHOD("_fs_changed"), &FileSystemDock::_fs_changed);
	ClassDB::bind_method(D_METHOD("_tree_multi_selected"), &FileSystemDock::_tree_multi_selected);
	ClassDB::bind_method(D_METHOD("_make_scene_confirm"), &FileSystemDock::_make_scene_confirm);
	ClassDB::bind_method(D_METHOD("_resource_created"), &FileSystemDock::_resource_created);
	ClassDB::bind_method(D_METHOD("_move_operation_confirm", "to_path", "overwrite"), &FileSystemDock::_move_operation_confirm, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("_move_with_overwrite"), &FileSystemDock::_move_with_overwrite);
	ClassDB::bind_method(D_METHOD("_rename_operation_confirm"), &FileSystemDock::_rename_operation_confirm);
	ClassDB::bind_method(D_METHOD("_duplicate_operation_confirm"), &FileSystemDock::_duplicate_operation_confirm);

	ClassDB::bind_method(D_METHOD("_search_changed"), &FileSystemDock::_search_changed);

	ClassDB::bind_method(D_METHOD("get_drag_data_fw", "point", "from"), &FileSystemDock::get_drag_data_fw);
	ClassDB::bind_method(D_METHOD("can_drop_data_fw", "point", "data", "from"), &FileSystemDock::can_drop_data_fw);
	ClassDB::bind_method(D_METHOD("drop_data_fw", "point", "data", "from"), &FileSystemDock::drop_data_fw);
	ClassDB::bind_method(D_METHOD("navigate_to_path", "path"), &FileSystemDock::navigate_to_path);

	ClassDB::bind_method(D_METHOD("_preview_invalidated"), &FileSystemDock::_preview_invalidated);
	ClassDB::bind_method(D_METHOD("_file_multi_selected"), &FileSystemDock::_file_multi_selected);
	ClassDB::bind_method(D_METHOD("_update_import_dock"), &FileSystemDock::_update_import_dock);

	ClassDB::bind_method(D_METHOD("_feature_profile_changed"), &FileSystemDock::_feature_profile_changed);

	ADD_SIGNAL(MethodInfo("inherit", PropertyInfo(Variant::STRING, "file")));
	ADD_SIGNAL(MethodInfo("instance", PropertyInfo(Variant::POOL_STRING_ARRAY, "files")));

	ADD_SIGNAL(MethodInfo("file_removed", PropertyInfo(Variant::STRING, "file")));
	ADD_SIGNAL(MethodInfo("folder_removed", PropertyInfo(Variant::STRING, "folder")));
	ADD_SIGNAL(MethodInfo("files_moved", PropertyInfo(Variant::STRING, "old_file"), PropertyInfo(Variant::STRING, "new_file")));
	ADD_SIGNAL(MethodInfo("folder_moved", PropertyInfo(Variant::STRING, "old_folder"), PropertyInfo(Variant::STRING, "new_file")));

	ADD_SIGNAL(MethodInfo("display_mode_changed"));
}

FileSystemDock::FileSystemDock(EditorNode *p_editor) {
	set_name("FileSystem");
	editor = p_editor;
	path = "res://";

	// `KEY_MASK_CMD | KEY_C` conflicts with other editor shortcuts.
	ED_SHORTCUT("filesystem_dock/copy_path", TTR("Copy Path"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_C);
	ED_SHORTCUT("filesystem_dock/duplicate", TTR("Duplicate..."), KEY_MASK_CMD | KEY_D);
	ED_SHORTCUT("filesystem_dock/delete", TTR("Delete"), KEY_DELETE);
#ifdef OSX_ENABLED
	ED_SHORTCUT("filesystem_dock/rename", TTR("Rename..."), KEY_ENTER);
#else
	ED_SHORTCUT("filesystem_dock/rename", TTR("Rename..."), KEY_F2);
#endif
	ED_SHORTCUT("filesystem_dock/open_search", TTR("Focus the search box"), KEY_MASK_CMD | KEY_F);

	VBoxContainer *top_vbc = memnew(VBoxContainer);
	add_child(top_vbc);

	HBoxContainer *toolbar_hbc = memnew(HBoxContainer);
	toolbar_hbc->add_constant_override("separation", 0);
	top_vbc->add_child(toolbar_hbc);

	button_hist_prev = memnew(ToolButton);
	button_hist_prev->set_disabled(true);
	button_hist_prev->set_focus_mode(FOCUS_NONE);
	button_hist_prev->set_tooltip(TTR("Previous Folder/File"));
	toolbar_hbc->add_child(button_hist_prev);

	button_hist_next = memnew(ToolButton);
	button_hist_next->set_disabled(true);
	button_hist_next->set_focus_mode(FOCUS_NONE);
	button_hist_next->set_tooltip(TTR("Next Folder/File"));
	toolbar_hbc->add_child(button_hist_next);

	current_path = memnew(LineEdit);
	current_path->set_h_size_flags(SIZE_EXPAND_FILL);
	_set_current_path_text(path);
	toolbar_hbc->add_child(current_path);

	button_reload = memnew(Button);
	button_reload->set_flat(true);
	button_reload->connect("pressed", this, "_rescan");
	button_reload->set_focus_mode(FOCUS_NONE);
	button_reload->set_tooltip(TTR("Re-Scan Filesystem"));
	button_reload->hide();
	toolbar_hbc->add_child(button_reload);

	button_toggle_display_mode = memnew(Button);
	button_toggle_display_mode->set_flat(true);
	button_toggle_display_mode->set_toggle_mode(true);
	button_toggle_display_mode->connect("toggled", this, "_toggle_split_mode");
	button_toggle_display_mode->set_focus_mode(FOCUS_NONE);
	button_toggle_display_mode->set_tooltip(TTR("Toggle Split Mode"));
	toolbar_hbc->add_child(button_toggle_display_mode);

	toolbar2_hbc = memnew(HBoxContainer);
	toolbar2_hbc->add_constant_override("separation", 0);
	top_vbc->add_child(toolbar2_hbc);

	tree_search_box = memnew(LineEdit);
	tree_search_box->set_h_size_flags(SIZE_EXPAND_FILL);
	tree_search_box->set_placeholder(TTR("Search files"));
	tree_search_box->connect("text_changed", this, "_search_changed", varray(tree_search_box));
	toolbar2_hbc->add_child(tree_search_box);

	tree_button_sort = _create_file_menu_button();
	toolbar2_hbc->add_child(tree_button_sort);

	file_list_popup = memnew(PopupMenu);
	file_list_popup->set_hide_on_window_lose_focus(true);
	add_child(file_list_popup);

	tree_popup = memnew(PopupMenu);
	tree_popup->set_hide_on_window_lose_focus(true);
	add_child(tree_popup);

	split_box = memnew(VSplitContainer);
	split_box->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(split_box);

	tree = memnew(Tree);

	tree->set_hide_root(true);
	tree->set_drag_forwarding(this);
	tree->set_allow_rmb_select(true);
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->set_custom_minimum_size(Size2(0, 15 * EDSCALE));
	split_box->add_child(tree);

	tree->connect("item_activated", this, "_tree_activate_file");
	tree->connect("multi_selected", this, "_tree_multi_selected");
	tree->connect("item_rmb_selected", this, "_tree_rmb_select");
	tree->connect("empty_rmb", this, "_tree_rmb_empty");
	tree->connect("nothing_selected", this, "_tree_empty_selected");
	tree->connect("gui_input", this, "_tree_gui_input");

	file_list_vb = memnew(VBoxContainer);
	file_list_vb->set_v_size_flags(SIZE_EXPAND_FILL);
	split_box->add_child(file_list_vb);

	path_hb = memnew(HBoxContainer);
	file_list_vb->add_child(path_hb);

	file_list_search_box = memnew(LineEdit);
	file_list_search_box->set_h_size_flags(SIZE_EXPAND_FILL);
	file_list_search_box->set_placeholder(TTR("Search files"));
	file_list_search_box->connect("text_changed", this, "_search_changed", varray(file_list_search_box));
	path_hb->add_child(file_list_search_box);

	file_list_button_sort = _create_file_menu_button();
	path_hb->add_child(file_list_button_sort);

	button_file_list_display_mode = memnew(ToolButton);
	path_hb->add_child(button_file_list_display_mode);

	files = memnew(ItemList);
	files->set_v_size_flags(SIZE_EXPAND_FILL);
	files->set_select_mode(ItemList::SELECT_MULTI);
	files->set_drag_forwarding(this);
	files->connect("item_rmb_selected", this, "_file_list_rmb_select");
	files->connect("gui_input", this, "_file_list_gui_input");
	files->connect("multi_selected", this, "_file_multi_selected");
	files->connect("rmb_clicked", this, "_file_list_rmb_pressed");
	files->set_custom_minimum_size(Size2(0, 15 * EDSCALE));
	files->set_allow_rmb_select(true);
	file_list_vb->add_child(files);

	scanning_vb = memnew(VBoxContainer);
	scanning_vb->hide();
	add_child(scanning_vb);

	Label *slabel = memnew(Label);
	slabel->set_text(TTR("Scanning Files,\nPlease Wait..."));
	slabel->set_align(Label::ALIGN_CENTER);
	scanning_vb->add_child(slabel);

	scanning_progress = memnew(ProgressBar);
	scanning_vb->add_child(scanning_progress);

	deps_editor = memnew(DependencyEditor);
	add_child(deps_editor);

	owners_editor = memnew(DependencyEditorOwners(editor));
	add_child(owners_editor);

	remove_dialog = memnew(DependencyRemoveDialog);
	remove_dialog->connect("file_removed", this, "_file_removed");
	remove_dialog->connect("folder_removed", this, "_folder_removed");
	add_child(remove_dialog);

	move_dialog = memnew(EditorDirDialog);
	move_dialog->get_ok()->set_text(TTR("Move"));
	add_child(move_dialog);
	move_dialog->connect("dir_selected", this, "_move_operation_confirm");

	rename_dialog = memnew(ConfirmationDialog);
	VBoxContainer *rename_dialog_vb = memnew(VBoxContainer);
	rename_dialog->add_child(rename_dialog_vb);

	rename_dialog_text = memnew(LineEdit);
	rename_dialog_vb->add_margin_child(TTR("Name:"), rename_dialog_text);
	rename_dialog->get_ok()->set_text(TTR("Rename"));
	add_child(rename_dialog);
	rename_dialog->register_text_enter(rename_dialog_text);
	rename_dialog->connect("confirmed", this, "_rename_operation_confirm");

	overwrite_dialog = memnew(ConfirmationDialog);
	overwrite_dialog->get_ok()->set_text(TTR("Overwrite"));
	add_child(overwrite_dialog);
	overwrite_dialog->connect("confirmed", this, "_move_with_overwrite");

	duplicate_dialog = memnew(ConfirmationDialog);
	VBoxContainer *duplicate_dialog_vb = memnew(VBoxContainer);
	duplicate_dialog->add_child(duplicate_dialog_vb);

	duplicate_dialog_text = memnew(LineEdit);
	duplicate_dialog_vb->add_margin_child(TTR("Name:"), duplicate_dialog_text);
	duplicate_dialog->get_ok()->set_text(TTR("Duplicate"));
	add_child(duplicate_dialog);
	duplicate_dialog->register_text_enter(duplicate_dialog_text);
	duplicate_dialog->connect("confirmed", this, "_duplicate_operation_confirm");

	make_dir_dialog = memnew(DirectoryCreateDialog);
	add_child(make_dir_dialog);
	make_dir_dialog->connect("dir_created", this, "_rescan");

	make_scene_dialog = memnew(SceneCreateDialog);
	add_child(make_scene_dialog);
	make_scene_dialog->connect("confirmed", this, "_make_scene_confirm");

	make_script_dialog = memnew(ScriptCreateDialog);
	make_script_dialog->set_title(TTR("Create Script"));
	add_child(make_script_dialog);

	new_resource_dialog = memnew(CreateDialog);
	add_child(new_resource_dialog);
	new_resource_dialog->set_base_type("Resource");
	new_resource_dialog->connect("create", this, "_resource_created");

	searched_string = String();
	uncollapsed_paths_before_search = Vector<String>();

	updating_tree = false;
	tree_update_id = 0;
	initialized = false;
	import_dock_needs_update = false;

	history_pos = 0;
	history_max_size = 20;
	history.push_back("res://");

	display_mode = DISPLAY_MODE_TREE_ONLY;
	old_display_mode = DISPLAY_MODE_TREE_ONLY;
	file_list_display_mode = FILE_LIST_DISPLAY_THUMBNAILS;

	always_show_folders = false;
}

FileSystemDock::~FileSystemDock() {
}
