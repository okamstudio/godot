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

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/templates/list.h"
#include "editor/create_dialog.h"
#include "editor/directory_create_dialog.h"
#include "editor/editor_dock_manager.h"
#include "editor/editor_feature_profile.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_dir_dialog.h"
#include "editor/gui/editor_scene_tabs.h"
#include "editor/import/3d/scene_import_settings.h"
#include "editor/import_dock.h"
#include "editor/plugins/editor_context_menu_plugin.h"
#include "editor/plugins/editor_resource_conversion_plugin.h"
#include "editor/plugins/editor_resource_tooltip_plugins.h"
#include "editor/scene_create_dialog.h"
#include "editor/scene_tree_dock.h"
#include "editor/shader_create_dialog.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/progress_bar.h"
#include "scene/resources/packed_scene.h"
#include "servers/display_server.h"

Control *FileSystemTree::make_custom_tooltip(const String &p_text) const {
	TreeItem *item = get_item_at_position(get_local_mouse_position());
	if (!item) {
		return nullptr;
	}
	return FileSystemDock::get_singleton()->create_tooltip_for_path(item->get_metadata(0));
}

Control *FileSystemList::make_custom_tooltip(const String &p_text) const {
	int idx = get_item_at_position(get_local_mouse_position());
	if (idx == -1) {
		return nullptr;
	}
	return FileSystemDock::get_singleton()->create_tooltip_for_path(get_item_metadata(idx));
}

void FileSystemList::_line_editor_submit(const String &p_text) {
	if (popup_edit_committed) {
		return; // Already processed by _text_editor_popup_modal_close
	}

	if (popup_editor->get_hide_reason() == Popup::HIDE_REASON_CANCELED) {
		return; // ESC pressed, app focus lost, or forced close from code.
	}

	popup_edit_committed = true; // End edit popup processing.
	popup_editor->hide();

	emit_signal(SNAME("item_edited"));
	queue_redraw();
}

bool FileSystemList::edit_selected() {
	ERR_FAIL_COND_V_MSG(!is_anything_selected(), false, "No item selected.");
	int s = get_current();
	ERR_FAIL_COND_V_MSG(s < 0, false, "No current item selected.");
	ensure_current_is_visible();

	Rect2 rect;
	Rect2 popup_rect;
	Vector2 ofs;

	Vector2 icon_size = get_item_icon(s)->get_size();

	// Handles the different icon modes (TOP/LEFT).
	switch (get_icon_mode()) {
		case ItemList::ICON_MODE_LEFT:
			rect = get_item_rect(s, true);
			ofs = Vector2(0, Math::floor((MAX(line_editor->get_minimum_size().height, rect.size.height) - rect.size.height) / 2));
			popup_rect.position = get_screen_position() + rect.position - ofs;
			popup_rect.size = rect.size;

			// Adjust for icon position and size.
			popup_rect.size.x -= icon_size.x;
			popup_rect.position.x += icon_size.x;
			break;
		case ItemList::ICON_MODE_TOP:
			rect = get_item_rect(s, false);
			popup_rect.position = get_screen_position() + rect.position;
			popup_rect.size = rect.size;

			// Adjust for icon position and size.
			popup_rect.size.y -= icon_size.y;
			popup_rect.position.y += icon_size.y;
			break;
	}

	popup_editor->set_position(popup_rect.position);
	popup_editor->set_size(popup_rect.size);

	String name = get_item_text(s);
	line_editor->set_text(name);
	line_editor->select(0, name.rfind_char('.'));

	popup_edit_committed = false; // Start edit popup processing.
	popup_editor->popup();
	popup_editor->child_controls_changed();
	line_editor->grab_focus();
	return true;
}

String FileSystemList::get_edit_text() {
	return line_editor->get_text();
}

void FileSystemList::_text_editor_popup_modal_close() {
	if (popup_edit_committed) {
		return; // Already processed by _text_editor_popup_modal_close
	}

	if (popup_editor->get_hide_reason() == Popup::HIDE_REASON_CANCELED) {
		return; // ESC pressed, app focus lost, or forced close from code.
	}

	_line_editor_submit(line_editor->get_text());
}

void FileSystemList::_bind_methods() {
	ADD_SIGNAL(MethodInfo("item_edited"));
}

FileSystemList::FileSystemList() {
	set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

	popup_editor = memnew(Popup);
	add_child(popup_editor);

	popup_editor_vb = memnew(VBoxContainer);
	popup_editor_vb->add_theme_constant_override("separation", 0);
	popup_editor_vb->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	popup_editor->add_child(popup_editor_vb);

	line_editor = memnew(LineEdit);
	line_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	popup_editor_vb->add_child(line_editor);
	line_editor->connect(SceneStringName(text_submitted), callable_mp(this, &FileSystemList::_line_editor_submit));
	popup_editor->connect("popup_hide", callable_mp(this, &FileSystemList::_text_editor_popup_modal_close));
}

Ref<Texture2D> FileSystemDock::_get_tree_item_icon(bool p_is_valid, const String &p_file_type, const String &p_icon_path) {
	if (!p_icon_path.is_empty()) {
		Ref<Texture2D> icon = ResourceLoader::load(p_icon_path);
		if (icon.is_valid()) {
			return icon;
		}
	}

	if (!p_is_valid) {
		return get_editor_theme_icon(SNAME("ImportFail"));
	} else if (has_theme_icon(p_file_type, EditorStringName(EditorIcons))) {
		return get_editor_theme_icon(p_file_type);
	} else {
		return get_editor_theme_icon(SNAME("File"));
	}
}

void FileSystemDock::_create_tree(TreeItem *p_parent, EditorFileSystemDirectory *p_dir, Vector<String> &uncollapsed_paths, bool p_select_in_favorites, bool p_unfold_path) {
	// Create a tree item for the subdirectory.
	TreeItem *subdirectory_item = tree->create_item(p_parent);
	String dname = p_dir->get_name();
	String lpath = p_dir->get_path();

	if (dname.is_empty()) {
		dname = "res://";
		resources_item = subdirectory_item;
	}

	// Set custom folder color (if applicable).
	bool has_custom_color = assigned_folder_colors.has(lpath);
	Color custom_color = has_custom_color ? folder_colors[assigned_folder_colors[lpath]] : Color();
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);

	if (has_custom_color) {
		subdirectory_item->set_icon_modulate(0, editor_is_dark_theme ? custom_color : custom_color * ITEM_COLOR_SCALE);
		subdirectory_item->set_custom_bg_color(0, Color(custom_color, editor_is_dark_theme ? ITEM_ALPHA_MIN : ITEM_ALPHA_MAX));
	} else {
		TreeItem *parent = subdirectory_item->get_parent();
		if (parent) {
			Color parent_bg_color = parent->get_custom_bg_color(0);
			if (parent_bg_color != Color()) {
				bool parent_has_custom_color = assigned_folder_colors.has(parent->get_metadata(0));
				subdirectory_item->set_custom_bg_color(0, parent_has_custom_color ? parent_bg_color.darkened(ITEM_BG_DARK_SCALE) : parent_bg_color);
				subdirectory_item->set_icon_modulate(0, parent->get_icon_modulate(0));
			} else {
				subdirectory_item->set_icon_modulate(0, get_theme_color(SNAME("folder_icon_color"), SNAME("FileDialog")));
			}
		}
	}

	subdirectory_item->set_text(0, dname);
	subdirectory_item->set_structured_text_bidi_override(0, TextServer::STRUCTURED_TEXT_FILE);
	subdirectory_item->set_icon(0, get_editor_theme_icon(SNAME("Folder")));
	if (da->is_link(lpath)) {
		subdirectory_item->set_icon_overlay(0, get_editor_theme_icon(SNAME("LinkOverlay")));
		subdirectory_item->set_tooltip_text(0, vformat(TTR("Link to: %s"), da->read_link(lpath)));
	}
	subdirectory_item->set_selectable(0, true);
	subdirectory_item->set_metadata(0, lpath);
	folder_map[lpath] = subdirectory_item;

	if (!p_select_in_favorites && (current_path == lpath || ((display_mode != DISPLAY_MODE_TREE_ONLY) && current_path.get_base_dir() == lpath))) {
		subdirectory_item->select(0);
		// Keep select an item when re-created a tree
		// To prevent crashing when nothing is selected.
		subdirectory_item->set_as_cursor(0);
	}

	if (p_unfold_path && current_path.begins_with(lpath) && current_path != lpath) {
		subdirectory_item->set_collapsed(false);
	} else {
		subdirectory_item->set_collapsed(!uncollapsed_paths.has(lpath));
	}

	// Create items for all subdirectories.
	bool reversed = file_sort == FileSortOption::FILE_SORT_NAME_REVERSE;
	for (int i = reversed ? p_dir->get_subdir_count() - 1 : 0;
			reversed ? i >= 0 : i < p_dir->get_subdir_count();
			reversed ? i-- : i++) {
		_create_tree(subdirectory_item, p_dir->get_subdir(i), uncollapsed_paths, p_select_in_favorites, p_unfold_path);
	}

	// Create all items for the files in the subdirectory.
	if (display_mode == DISPLAY_MODE_TREE_ONLY) {
		const String main_scene = ResourceUID::ensure_path(GLOBAL_GET("application/run/main_scene"));

		// Build the list of the files to display.
		List<FileInfo> file_list;
		for (int i = 0; i < p_dir->get_file_count(); i++) {
			String file_type = p_dir->get_file_type(i);
			if (file_type != "TextFile" && file_type != "OtherFile" && _is_file_type_disabled_by_feature_profile(file_type)) {
				// If type is disabled, file won't be displayed.
				continue;
			}

			FileInfo file_info;
			file_info.name = p_dir->get_file(i);
			file_info.type = p_dir->get_file_type(i);
			file_info.icon_path = p_dir->get_file_icon_path(i);
			file_info.import_broken = !p_dir->get_file_import_is_valid(i);
			file_info.modified_time = p_dir->get_file_modified_time(i);

			file_list.push_back(file_info);
		}

		// Sort the file list if needed.
		sort_file_info_list(file_list, file_sort);

		// Build the tree.
		const int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));

		for (const FileInfo &file_info : file_list) {
			TreeItem *file_item = tree->create_item(subdirectory_item);
			const String file_metadata = lpath.path_join(file_info.name);
			file_item->set_text(0, file_info.name);
			file_item->set_structured_text_bidi_override(0, TextServer::STRUCTURED_TEXT_FILE);
			file_item->set_icon(0, _get_tree_item_icon(!file_info.import_broken, file_info.type, file_info.icon_path));
			if (da->is_link(file_metadata)) {
				file_item->set_icon_overlay(0, get_editor_theme_icon(SNAME("LinkOverlay")));
				file_item->set_tooltip_text(0, vformat(TTR("Link to: %s"), da->read_link(file_metadata)));
			}
			file_item->set_icon_max_width(0, icon_size);
			Color parent_bg_color = subdirectory_item->get_custom_bg_color(0);
			if (has_custom_color) {
				file_item->set_custom_bg_color(0, parent_bg_color.darkened(ITEM_BG_DARK_SCALE));
			} else if (parent_bg_color != Color()) {
				file_item->set_custom_bg_color(0, parent_bg_color);
			}
			file_item->set_metadata(0, file_metadata);
			if (!p_select_in_favorites && current_path == file_metadata) {
				file_item->select(0);
				file_item->set_as_cursor(0);
			}
			if (main_scene == file_metadata) {
				file_item->set_custom_color(0, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
			}
			Array udata;
			udata.push_back(tree_update_id);
			udata.push_back(file_item);
			EditorResourcePreview::get_singleton()->queue_resource_preview(file_metadata, this, "_tree_thumbnail_done", udata);
		}
	} else {
		if (lpath.get_base_dir() == current_path.get_base_dir()) {
			subdirectory_item->select(0);
			subdirectory_item->set_as_cursor(0);
		}
	}
}

Vector<String> FileSystemDock::get_uncollapsed_paths() const {
	Vector<String> uncollapsed_paths;
	TreeItem *root = tree->get_root();
	if (root) {
		if (!favorites_item->is_collapsed()) {
			uncollapsed_paths.push_back(favorites_item->get_metadata(0));
		}

		// BFS to find all uncollapsed paths of the resource directory.
		TreeItem *res_subtree = root->get_first_child()->get_next();
		if (res_subtree) {
			List<TreeItem *> queue;
			queue.push_back(res_subtree);

			while (!queue.is_empty()) {
				TreeItem *ti = queue.back()->get();
				queue.pop_back();
				if (!ti->is_collapsed() && ti->get_child_count() > 0) {
					Variant path = ti->get_metadata(0);
					if (path) {
						uncollapsed_paths.push_back(path);
					}
				}
				for (int i = 0; i < ti->get_child_count(); i++) {
					queue.push_back(ti->get_child(i));
				}
			}
		}
	}
	return uncollapsed_paths;
}

void FileSystemDock::_update_tree(const Vector<String> &p_uncollapsed_paths, bool p_uncollapse_root, bool p_scroll_to_selected) {
	// Recreate the tree.
	tree->clear();
	tree_update_id++;
	updating_tree = true;
	TreeItem *root = tree->create_item();
	folder_map.clear();

	// Handles the favorites.
	favorites_item = tree->create_item(root);
	favorites_item->set_icon(0, get_editor_theme_icon(SNAME("Favorites")));
	favorites_item->set_text(0, TTR("Favorites:"));
	favorites_item->set_metadata(0, "Favorites");
	favorites_item->set_collapsed(!p_uncollapsed_paths.has("Favorites"));

	Vector<String> favorite_paths = EditorSettings::get_singleton()->get_favorites();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	bool fav_changed = false;
	for (int i = favorite_paths.size() - 1; i >= 0; i--) {
		if (da->dir_exists(favorite_paths[i]) || da->file_exists(favorite_paths[i])) {
			continue;
		}
		favorite_paths.remove_at(i);
		fav_changed = true;
	}
	if (fav_changed) {
		EditorSettings::get_singleton()->set_favorites(favorite_paths);
	}

	Ref<Texture2D> folder_icon = get_editor_theme_icon(SNAME("Folder"));
	const Color default_folder_color = get_theme_color(SNAME("folder_icon_color"), SNAME("FileDialog"));

	for (int i = 0; i < favorite_paths.size(); i++) {
		const String &favorite = favorite_paths[i];
		if (!favorite.begins_with("res://")) {
			continue;
		}

		String text;
		Ref<Texture2D> icon;
		Color color;
		if (favorite == "res://") {
			text = "/";
			icon = folder_icon;
			color = default_folder_color;
		} else if (favorite.ends_with("/")) {
			text = favorite.substr(0, favorite.length() - 1).get_file();
			icon = folder_icon;
			color = assigned_folder_colors.has(favorite) ? folder_colors[assigned_folder_colors[favorite]] : default_folder_color;
		} else {
			text = favorite.get_file();
			int index;
			EditorFileSystemDirectory *dir = EditorFileSystem::get_singleton()->find_file(favorite, &index);
			if (dir) {
				icon = _get_tree_item_icon(dir->get_file_import_is_valid(index), dir->get_file_type(index), dir->get_file_icon_path(index));
			} else {
				icon = get_editor_theme_icon(SNAME("File"));
			}
			color = Color(1, 1, 1);
		}

		TreeItem *ti = tree->create_item(favorites_item);
		ti->set_text(0, text);
		ti->set_icon(0, icon);
		ti->set_icon_modulate(0, color);
		ti->set_tooltip_text(0, favorite);
		ti->set_selectable(0, true);
		ti->set_metadata(0, favorite);

		if (!favorite.ends_with("/")) {
			Array udata;
			udata.push_back(tree_update_id);
			udata.push_back(ti);
			EditorResourcePreview::get_singleton()->queue_resource_preview(favorite, this, "_tree_thumbnail_done", udata);
		}
	}

	Vector<String> uncollapsed_paths = p_uncollapsed_paths;
	if (p_uncollapse_root) {
		uncollapsed_paths.push_back("res://");
	}

	// Create the remaining of the tree.
	_create_tree(root, EditorFileSystem::get_singleton()->get_filesystem(), uncollapsed_paths, false);
	if (!searched_tokens.is_empty()) {
		_update_filtered_items();
	}

	if (p_scroll_to_selected) {
		tree->ensure_cursor_is_visible();
	}

	updating_tree = false;
}

void FileSystemDock::set_display_mode(DisplayMode p_display_mode) {
	display_mode = p_display_mode;
	_update_display_mode(false);
}

void FileSystemDock::_update_display_mode(bool p_force) {
	// Compute the new display mode.
	if (p_force || old_display_mode != display_mode) {
		switch (display_mode) {
			case DISPLAY_MODE_TREE_ONLY:
				button_toggle_display_mode->set_button_icon(get_editor_theme_icon(SNAME("Panels1")));
				tree->show();
				tree->set_v_size_flags(SIZE_EXPAND_FILL);
				toolbar2_hbc->show();

				_update_tree(get_uncollapsed_paths());
				file_list_vb->hide();
				break;

			case DISPLAY_MODE_HSPLIT:
			case DISPLAY_MODE_VSPLIT:
				const bool is_vertical = display_mode == DISPLAY_MODE_VSPLIT;
				split_box->set_vertical(is_vertical);

				const int actual_offset = is_vertical ? split_box_offset_v : split_box_offset_h;
				split_box->set_split_offset(actual_offset);
				const StringName icon = is_vertical ? SNAME("Panels2") : SNAME("Panels2Alt");
				button_toggle_display_mode->set_button_icon(get_editor_theme_icon(icon));

				tree->show();
				tree->set_v_size_flags(SIZE_EXPAND_FILL);
				tree->ensure_cursor_is_visible();
				toolbar2_hbc->hide();
				_update_tree(get_uncollapsed_paths());

				file_list_vb->show();
				_update_file_list(true);
				break;
		}
		old_display_mode = display_mode;
	}
}

void FileSystemDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			EditorFeatureProfileManager::get_singleton()->connect("current_feature_profile_changed", callable_mp(this, &FileSystemDock::_feature_profile_changed));
			EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &FileSystemDock::_fs_changed));
			EditorResourcePreview::get_singleton()->connect("preview_invalidated", callable_mp(this, &FileSystemDock::_preview_invalidated));

			button_file_list_display_mode->connect(SceneStringName(pressed), callable_mp(this, &FileSystemDock::_toggle_file_display));
			files->connect("item_activated", callable_mp(this, &FileSystemDock::_file_list_activate_file));
			button_hist_next->connect(SceneStringName(pressed), callable_mp(this, &FileSystemDock::_fw_history));
			button_hist_prev->connect(SceneStringName(pressed), callable_mp(this, &FileSystemDock::_bw_history));
			file_list_popup->connect(SceneStringName(id_pressed), callable_mp(this, &FileSystemDock::_file_list_rmb_option));
			tree_popup->connect(SceneStringName(id_pressed), callable_mp(this, &FileSystemDock::_tree_rmb_option));
			current_path_line_edit->connect(SceneStringName(text_submitted), callable_mp(this, &FileSystemDock::_navigate_to_path).bind(false));

			always_show_folders = bool(EDITOR_GET("docks/filesystem/always_show_folders"));

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
				scanning_progress->set_value(EditorFileSystem::get_singleton()->get_scanning_progress() * 100.0f);
			}
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			Dictionary dd = get_viewport()->gui_get_drag_data();
			if (tree->is_visible_in_tree() && dd.has("type")) {
				if (dd.has("favorite")) {
					if ((String(dd["favorite"]) == "all")) {
						tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);
					}
				} else if ((String(dd["type"]) == "files") || (String(dd["type"]) == "files_and_dirs")) {
					tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM | Tree::DROP_MODE_INBETWEEN);
				} else if ((String(dd["type"]) == "nodes") || (String(dd["type"]) == "resource")) {
					holding_branch = true;
					TreeItem *item = tree->get_next_selected(tree->get_root());
					while (item) {
						tree_items_selected_on_drag_begin.push_back(item);
						item = tree->get_next_selected(item);
					}
					list_items_selected_on_drag_begin = files->get_selected_items();
				}
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			tree->set_drop_mode_flags(0);

			if (holding_branch) {
				holding_branch = false;
				_reselect_items_selected_on_drag_begin(true);
			}
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			_update_display_mode(true);

			button_reload->set_button_icon(get_editor_theme_icon(SNAME("Reload")));

			StringName mode_icon = "Panels1";
			if (display_mode == DISPLAY_MODE_VSPLIT) {
				mode_icon = "Panels2";
			} else if (display_mode == DISPLAY_MODE_HSPLIT) {
				mode_icon = "Panels2Alt";
			}
			button_toggle_display_mode->set_button_icon(get_editor_theme_icon(mode_icon));

			if (file_list_display_mode == FILE_LIST_DISPLAY_LIST) {
				button_file_list_display_mode->set_button_icon(get_editor_theme_icon(SNAME("FileThumbnail")));
			} else {
				button_file_list_display_mode->set_button_icon(get_editor_theme_icon(SNAME("FileList")));
			}

			tree_search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			tree_button_sort->set_button_icon(get_editor_theme_icon(SNAME("Sort")));

			file_list_search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			file_list_button_sort->set_button_icon(get_editor_theme_icon(SNAME("Sort")));

			button_dock_placement->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));

			if (is_layout_rtl()) {
				button_hist_next->set_button_icon(get_editor_theme_icon(SNAME("Back")));
				button_hist_prev->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
			} else {
				button_hist_next->set_button_icon(get_editor_theme_icon(SNAME("Forward")));
				button_hist_prev->set_button_icon(get_editor_theme_icon(SNAME("Back")));
			}

			overwrite_dialog_scroll->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), "Tree"));
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			// Update editor dark theme & always show folders states from editor settings, redraw if needed.
			bool do_redraw = false;

			bool new_editor_is_dark_theme = EditorThemeManager::is_dark_theme();
			if (new_editor_is_dark_theme != editor_is_dark_theme) {
				editor_is_dark_theme = new_editor_is_dark_theme;
				do_redraw = true;
			}

			bool new_always_show_folders = bool(EDITOR_GET("docks/filesystem/always_show_folders"));
			if (new_always_show_folders != always_show_folders) {
				always_show_folders = new_always_show_folders;
				do_redraw = true;
			}

			if (do_redraw) {
				update_all();
			}

			if (EditorThemeManager::is_generated_theme_outdated()) {
				// Change full tree mode.
				_update_display_mode();
			}
		} break;
	}
}

void FileSystemDock::_tree_multi_selected(Object *p_item, int p_column, bool p_selected) {
	// Update the import dock.
	import_dock_needs_update = true;
	callable_mp(this, &FileSystemDock::_update_import_dock).call_deferred();

	// Return if we don't select something new.
	if (!p_selected) {
		return;
	}

	// Tree item selected.
	TreeItem *selected = tree->get_selected();
	if (!selected) {
		return;
	}

	if (selected->get_parent() == favorites_item && !String(selected->get_metadata(0)).ends_with("/")) {
		// Go to the favorites if we click in the favorites and the path has changed.
		current_path = "Favorites";
	} else {
		current_path = selected->get_metadata(0);
		// Note: the "Favorites" item also leads to this path.
	}

	// Display the current path.
	_set_current_path_line_edit_text(current_path);
	_push_to_history();

	// Update the file list.
	if (!updating_tree && display_mode != DISPLAY_MODE_TREE_ONLY) {
		_update_file_list(false);
	}
}

Vector<String> FileSystemDock::get_selected_paths() const {
	if (display_mode == DISPLAY_MODE_TREE_ONLY) {
		return _tree_get_selected(false);
	} else {
		Vector<String> selected = _file_list_get_selected();
		if (selected.is_empty()) {
			selected.push_back(get_current_directory());
		}
		return selected;
	}
}

String FileSystemDock::get_current_path() const {
	return current_path;
}

String FileSystemDock::get_current_directory() const {
	if (current_path.ends_with("/")) {
		return current_path;
	} else {
		return current_path.get_base_dir();
	}
}

void FileSystemDock::_set_current_path_line_edit_text(const String &p_path) {
	if (p_path == "Favorites") {
		current_path_line_edit->set_text(TTR("Favorites"));
	} else {
		current_path_line_edit->set_text(current_path);
	}
}

void FileSystemDock::_navigate_to_path(const String &p_path, bool p_select_in_favorites) {
	String target_path = p_path;
	bool is_directory = false;

	if (p_path.is_empty()) {
		target_path = "res://";
		is_directory = true;
	} else if (p_path != "Favorites") {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (da->dir_exists(p_path)) {
			is_directory = true;
			if (!p_path.ends_with("/")) {
				target_path += "/";
			}
		} else if (!da->file_exists(p_path)) {
			ERR_FAIL_MSG(vformat("Cannot navigate to '%s' as it has not been found in the file system!", p_path));
		}
	}

	current_path = target_path;
	_set_current_path_line_edit_text(current_path);
	_push_to_history();

	String base_dir_path = target_path.get_base_dir();
	if (base_dir_path != "res://") {
		base_dir_path += "/";
	}

	TreeItem **directory_ptr = folder_map.getptr(base_dir_path);
	if (!directory_ptr) {
		return;
	}

	// Unfold all folders along the path.
	TreeItem *ti = *directory_ptr;
	while (ti) {
		ti->set_collapsed(false);
		ti = ti->get_parent();
	}

	// Select the file or directory in the tree.
	tree->deselect_all();
	if (display_mode == DISPLAY_MODE_TREE_ONLY) {
		const String file_name = is_directory ? target_path.trim_suffix("/").get_file() + "/" : target_path.get_file();
		TreeItem *item = is_directory ? *directory_ptr : (*directory_ptr)->get_first_child();
		while (item) {
			if (item->get_metadata(0).operator String().ends_with(file_name)) {
				item->select(0);
				break;
			}
			item = item->get_next();
		}
	} else {
		(*directory_ptr)->select(0);
		_update_file_list(false);
	}
	tree->ensure_cursor_is_visible();
}

bool FileSystemDock::_update_filtered_items(TreeItem *p_tree_item) {
	TreeItem *item = p_tree_item;
	if (!item) {
		item = tree->get_root();
	}
	ERR_FAIL_NULL_V(item, false);

	bool keep_visible = false;
	for (TreeItem *child = item->get_first_child(); child; child = child->get_next()) {
		keep_visible = _update_filtered_items(child) || keep_visible;
	}

	if (searched_tokens.is_empty()) {
		item->set_visible(true);
		// Always uncollapse root (the hidden item above res:// and favorites).
		item->set_collapsed(item != tree->get_root() && !uncollapsed_paths_before_search.has(item->get_metadata(0)));
		return true;
	}

	if (keep_visible) {
		item->set_collapsed(false);
	} else {
		// res:// and favorites are always visible.
		keep_visible = item == resources_item || item == favorites_item;
		keep_visible = keep_visible || _matches_all_search_tokens(item->get_text(0));
	}
	item->set_visible(keep_visible);
	return keep_visible;
}

void FileSystemDock::navigate_to_path(const String &p_path) {
	file_list_search_box->clear();
	_navigate_to_path(p_path);

	// Ensure that the FileSystem dock is visible.
	EditorDockManager::get_singleton()->focus_dock(this);
	import_dock_needs_update = true;
	_update_import_dock();
}

void FileSystemDock::_file_list_thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata) {
	if (p_preview.is_valid()) {
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

void FileSystemDock::_tree_thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata) {
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
	emit_signal(SNAME("display_mode_changed"));
}

void FileSystemDock::_set_file_display(bool p_active) {
	if (p_active) {
		file_list_display_mode = FILE_LIST_DISPLAY_LIST;
		button_file_list_display_mode->set_button_icon(get_editor_theme_icon(SNAME("FileThumbnail")));
		button_file_list_display_mode->set_tooltip_text(TTR("View items as a grid of thumbnails."));
	} else {
		file_list_display_mode = FILE_LIST_DISPLAY_THUMBNAILS;
		button_file_list_display_mode->set_button_icon(get_editor_theme_icon(SNAME("FileList")));
		button_file_list_display_mode->set_tooltip_text(TTR("View items as a list."));
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

		if (_matches_all_search_tokens(file)) {
			FileInfo file_info;
			file_info.name = file;
			file_info.type = p_path->get_file_type(i);
			file_info.path = p_path->get_file_path(i);
			file_info.import_broken = !p_path->get_file_import_is_valid(i);
			file_info.modified_time = p_path->get_file_modified_time(i);

			if (_is_file_type_disabled_by_feature_profile(file_info.type)) {
				// This type is disabled, will not appear here.
				continue;
			}

			matches->push_back(file_info);
			if (matches->size() > p_max_items) {
				return;
			}
		}
	}
}

void FileSystemDock::_update_file_list(bool p_keep_selection) {
	// Register the previously current and selected items.
	HashSet<String> previous_selection;
	HashSet<int> valid_selection;
	if (p_keep_selection) {
		for (int i = 0; i < files->get_item_count(); i++) {
			if (files->is_selected(i)) {
				previous_selection.insert(files->get_item_text(i));
			}
		}
	}

	files->clear();

	_set_current_path_line_edit_text(current_path);

	String directory = current_path;
	String file = "";

	int thumbnail_size = EDITOR_GET("docks/filesystem/thumbnail_size");
	thumbnail_size *= EDSCALE;
	Ref<Texture2D> folder_thumbnail;
	Ref<Texture2D> file_thumbnail;
	Ref<Texture2D> file_thumbnail_broken;

	bool use_thumbnails = (file_list_display_mode == FILE_LIST_DISPLAY_THUMBNAILS);

	if (use_thumbnails) {
		// Thumbnails mode.
		files->set_max_columns(0);
		files->set_icon_mode(ItemList::ICON_MODE_TOP);
		files->set_fixed_column_width(thumbnail_size * 3 / 2);
		files->set_max_text_lines(2);
		files->set_fixed_icon_size(Size2(thumbnail_size, thumbnail_size));

		const int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
		files->set_fixed_tag_icon_size(Size2(icon_size, icon_size));

		if (thumbnail_size < 64) {
			folder_thumbnail = get_editor_theme_icon(SNAME("FolderMediumThumb"));
			file_thumbnail = get_editor_theme_icon(SNAME("FileMediumThumb"));
			file_thumbnail_broken = get_editor_theme_icon(SNAME("FileDeadMediumThumb"));
		} else {
			folder_thumbnail = get_editor_theme_icon(SNAME("FolderBigThumb"));
			file_thumbnail = get_editor_theme_icon(SNAME("FileBigThumb"));
			file_thumbnail_broken = get_editor_theme_icon(SNAME("FileDeadBigThumb"));
		}
	} else {
		// No thumbnails.
		files->set_icon_mode(ItemList::ICON_MODE_LEFT);
		files->set_max_columns(1);
		files->set_max_text_lines(1);
		files->set_fixed_column_width(0);
		const int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
		files->set_fixed_icon_size(Size2(icon_size, icon_size));
	}

	Ref<Texture2D> folder_icon = (use_thumbnails) ? folder_thumbnail : get_theme_icon(SNAME("folder"), SNAME("FileDialog"));
	const Color default_folder_color = get_theme_color(SNAME("folder_icon_color"), SNAME("FileDialog"));

	// Build the FileInfo list.
	List<FileInfo> file_list;
	if (current_path == "Favorites") {
		// Display the favorites.
		Vector<String> favorites_list = EditorSettings::get_singleton()->get_favorites();
		for (const String &favorite : favorites_list) {
			String text;
			Ref<Texture2D> icon;
			if (favorite == "res://") {
				text = "/";
				icon = folder_icon;
				if (searched_tokens.is_empty() || _matches_all_search_tokens(text)) {
					files->add_item(text, icon, true);
					files->set_item_metadata(-1, favorite);
				}
			} else if (favorite.ends_with("/")) {
				text = favorite.substr(0, favorite.length() - 1).get_file();
				icon = folder_icon;
				if (searched_tokens.is_empty() || _matches_all_search_tokens(text)) {
					files->add_item(text, icon, true);
					files->set_item_metadata(-1, favorite);
				}
			} else {
				int index;
				EditorFileSystemDirectory *efd = EditorFileSystem::get_singleton()->find_file(favorite, &index);

				FileInfo file_info;
				file_info.name = favorite.get_file();
				file_info.path = favorite;
				if (efd) {
					file_info.type = efd->get_file_type(index);
					file_info.icon_path = efd->get_file_icon_path(index);
					file_info.import_broken = !efd->get_file_import_is_valid(index);
					file_info.modified_time = efd->get_file_modified_time(index);
				} else {
					file_info.type = "";
					file_info.import_broken = true;
					file_info.modified_time = 0;
				}

				if (searched_tokens.is_empty() || _matches_all_search_tokens(file_info.name)) {
					file_list.push_back(file_info);
				}
			}
		}
	} else {
		if (!directory.begins_with("res://")) {
			directory = "res://" + directory;
		}
		// Get infos on the directory + file.
		if (directory.ends_with("/") && directory != "res://") {
			directory = directory.substr(0, directory.length() - 1);
		}
		EditorFileSystemDirectory *efd = EditorFileSystem::get_singleton()->get_filesystem_path(directory);
		if (!efd) {
			directory = current_path.get_base_dir();
			file = current_path.get_file();
			efd = EditorFileSystem::get_singleton()->get_filesystem_path(directory);
		}
		if (!efd) {
			return;
		}

		if (!searched_tokens.is_empty()) {
			// Display the search results.
			// Limit the number of results displayed to avoid an infinite loop.
			_search(EditorFileSystem::get_singleton()->get_filesystem(), &file_list, 10000);
		} else {
			if (display_mode == DISPLAY_MODE_TREE_ONLY || always_show_folders) {
				// Check for a folder color to inherit (if one is assigned).
				Color inherited_folder_color = default_folder_color;
				String color_scan_dir = directory;
				while (color_scan_dir != "res://" && inherited_folder_color == default_folder_color) {
					if (!color_scan_dir.ends_with("/")) {
						color_scan_dir += "/";
					}

					if (assigned_folder_colors.has(color_scan_dir)) {
						inherited_folder_color = folder_colors[assigned_folder_colors[color_scan_dir]];
					}

					color_scan_dir = color_scan_dir.rstrip("/").get_base_dir();
				}

				// Display folders in the list.
				if (directory != "res://") {
					files->add_item("..", folder_icon, true);

					String bd = directory.get_base_dir();
					if (bd != "res://" && !bd.ends_with("/")) {
						bd += "/";
					}

					files->set_item_metadata(-1, bd);
					files->set_item_selectable(-1, false);
					files->set_item_icon_modulate(-1, editor_is_dark_theme ? inherited_folder_color : inherited_folder_color * ITEM_COLOR_SCALE);
				}

				bool reversed = file_sort == FileSortOption::FILE_SORT_NAME_REVERSE;
				for (int i = reversed ? efd->get_subdir_count() - 1 : 0;
						reversed ? i >= 0 : i < efd->get_subdir_count();
						reversed ? i-- : i++) {
					String dname = efd->get_subdir(i)->get_name();
					String dpath = directory.path_join(dname) + "/";
					bool has_custom_color = assigned_folder_colors.has(dpath);

					files->add_item(dname, folder_icon, true);
					files->set_item_metadata(-1, dpath);
					Color this_folder_color = has_custom_color ? folder_colors[assigned_folder_colors[dpath]] : inherited_folder_color;
					files->set_item_icon_modulate(-1, editor_is_dark_theme ? this_folder_color : this_folder_color * ITEM_COLOR_SCALE);

					if (previous_selection.has(dname)) {
						files->select(files->get_item_count() - 1, false);
						valid_selection.insert(files->get_item_count() - 1);
					}
				}
			}

			// Display the folder content.
			for (int i = 0; i < efd->get_file_count(); i++) {
				FileInfo file_info;
				file_info.name = efd->get_file(i);
				file_info.path = directory.path_join(file_info.name);
				file_info.type = efd->get_file_type(i);
				file_info.icon_path = efd->get_file_icon_path(i);
				file_info.import_broken = !efd->get_file_import_is_valid(i);
				file_info.modified_time = efd->get_file_modified_time(i);

				file_list.push_back(file_info);
			}
		}
	}

	// Sort the file list if needed.
	sort_file_info_list(file_list, file_sort);

	// Fills the ItemList control node from the FileInfos.
	const String main_scene = ResourceUID::ensure_path(GLOBAL_GET("application/run/main_scene"));
	for (FileInfo &E : file_list) {
		FileInfo *finfo = &(E);
		String fname = finfo->name;
		String fpath = finfo->path;

		Ref<Texture2D> type_icon;
		Ref<Texture2D> big_icon;

		String tooltip = fpath;

		// Select the icons.
		type_icon = _get_tree_item_icon(!finfo->import_broken, finfo->type, finfo->icon_path);
		if (!finfo->import_broken) {
			big_icon = file_thumbnail;
		} else {
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
			files->set_item_custom_fg_color(item_index, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)));
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
		if (previous_selection.has(fname)) {
			files->select(item_index, false);
			valid_selection.insert(item_index);
		}

		if (!p_keep_selection && !file.is_empty() && fname == file) {
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

	// If we only have any selected items retained, we need to update the current idx.
	if (!valid_selection.is_empty()) {
		files->set_current(*valid_selection.begin());
	}
}

HashSet<String> FileSystemDock::_get_valid_conversions_for_file_paths(const Vector<String> &p_paths) {
	HashSet<String> all_valid_conversion_to_targets;
	for (const String &fpath : p_paths) {
		if (fpath.is_empty() || fpath == "res://" || !FileAccess::exists(fpath) || FileAccess::exists(fpath + ".import")) {
			return HashSet<String>();
		}

		Vector<Ref<EditorResourceConversionPlugin>> conversions = EditorNode::get_singleton()->find_resource_conversion_plugin_for_type_name(EditorFileSystem::get_singleton()->get_file_type(fpath));

		if (conversions.is_empty()) {
			// This resource can't convert to anything, so return an empty list.
			return HashSet<String>();
		}

		// Get a list of all potential conversion-to targets.
		HashSet<String> current_valid_conversion_to_targets;
		for (const Ref<EditorResourceConversionPlugin> &E : conversions) {
			const String what = E->converts_to();
			current_valid_conversion_to_targets.insert(what);
		}

		if (all_valid_conversion_to_targets.is_empty()) {
			// If we have no existing valid conversions, this is the first one, so copy them directly.
			all_valid_conversion_to_targets = current_valid_conversion_to_targets;
		} else {
			// Check existing conversion targets and remove any which are not in the current list.
			for (const String &S : all_valid_conversion_to_targets) {
				if (!current_valid_conversion_to_targets.has(S)) {
					all_valid_conversion_to_targets.erase(S);
				}
			}
			// We have no more remaining valid conversions, so break the loop.
			if (all_valid_conversion_to_targets.is_empty()) {
				break;
			}
		}
	}

	return all_valid_conversion_to_targets;
}

void FileSystemDock::_select_file(const String &p_path, bool p_select_in_favorites, bool p_navigate) {
	String fpath = p_path;
	if (fpath.ends_with("/")) {
		// Ignore a directory.
	} else if (fpath != "Favorites") {
		if (FileAccess::exists(fpath + ".import")) {
			Ref<ConfigFile> config;
			config.instantiate();
			Error err = config->load(fpath + ".import");
			if (err == OK) {
				if (config->has_section_key("remap", "importer")) {
					String importer = config->get_value("remap", "importer");
					if (importer == "keep" || importer == "skip") {
						EditorNode::get_singleton()->show_warning(TTR("Importing has been disabled for this file, so it can't be opened for editing."));
						return;
					}
				}
			}
		}

		String resource_type = ResourceLoader::get_resource_type(fpath);

		if (resource_type == "PackedScene" || resource_type == "AnimationLibrary") {
			bool is_imported = false;

			{
				List<String> importer_exts;
				ResourceImporterScene::get_scene_importer_extensions(&importer_exts);
				String extension = fpath.get_extension();
				for (const String &E : importer_exts) {
					if (extension.nocasecmp_to(E) == 0) {
						is_imported = true;
						break;
					}
				}
			}

			if (is_imported) {
				SceneImportSettingsDialog::get_singleton()->open_settings(p_path, resource_type);
			} else if (resource_type == "PackedScene") {
				EditorNode::get_singleton()->open_request(fpath);
			} else {
				EditorNode::get_singleton()->load_resource(fpath);
			}
		} else if (ResourceLoader::is_imported(fpath)) {
			// If the importer has advanced settings, show them.
			int order;
			bool can_threads;
			String name;
			Error err = ResourceFormatImporter::get_singleton()->get_import_order_threads_and_importer(fpath, order, can_threads, name);
			bool used_advanced_settings = false;
			if (err == OK) {
				Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(name);
				if (importer.is_valid() && importer->has_advanced_options()) {
					importer->show_advanced_options(fpath);
					used_advanced_settings = true;
				}
			}

			if (!used_advanced_settings) {
				EditorNode::get_singleton()->load_resource(fpath);
			}

		} else {
			EditorNode::get_singleton()->load_resource(fpath);
		}
	}
	if (p_navigate) {
		_navigate_to_path(fpath, p_select_in_favorites);
	}
}

void FileSystemDock::_tree_activate_file() {
	TreeItem *selected = tree->get_selected();
	if (selected) {
		String file_path = selected->get_metadata(0);
		TreeItem *parent = selected->get_parent();
		bool is_favorite = parent != nullptr && parent->get_metadata(0) == "Favorites";

		if ((!is_favorite && file_path.ends_with("/")) || file_path == "Favorites") {
			bool collapsed = selected->is_collapsed();
			selected->set_collapsed(!collapsed);
		} else {
			_select_file(file_path, is_favorite && !file_path.ends_with("/"), false);
		}
	}
}

void FileSystemDock::_file_list_activate_file(int p_idx) {
	_select_file(files->get_item_metadata(p_idx));
}

void FileSystemDock::_preview_invalidated(const String &p_path) {
	if (file_list_display_mode == FILE_LIST_DISPLAY_THUMBNAILS && p_path.get_base_dir() == current_path && searched_tokens.is_empty() && file_list_vb->is_visible_in_tree()) {
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

	update_all();

	if (!select_after_scan.is_empty()) {
		_navigate_to_path(select_after_scan);
		select_after_scan.clear();
		import_dock_needs_update = true;
		_update_import_dock();
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
	current_path = history[history_pos];
	_set_current_path_line_edit_text(current_path);

	if (tree->is_visible()) {
		_update_tree(get_uncollapsed_paths());
		tree->grab_focus();
	}

	if (file_list_vb->is_visible()) {
		_update_file_list(false);
	}

	button_hist_prev->set_disabled(history_pos == 0);
	button_hist_next->set_disabled(history_pos == history.size() - 1);
}

void FileSystemDock::_push_to_history() {
	if (history[history_pos] != current_path) {
		history.resize(history_pos + 1);
		history.push_back(current_path);
		history_pos++;

		if (history.size() > history_max_size) {
			history.remove_at(0);
			history_pos = history_max_size - 1;
		}
	}

	button_hist_prev->set_disabled(history_pos == 0);
	button_hist_next->set_disabled(history_pos == history.size() - 1);
}

void FileSystemDock::_get_all_items_in_dir(EditorFileSystemDirectory *p_efsd, Vector<String> &r_files, Vector<String> &r_folders) const {
	if (p_efsd == nullptr) {
		return;
	}

	for (int i = 0; i < p_efsd->get_subdir_count(); i++) {
		r_folders.push_back(p_efsd->get_subdir(i)->get_path());
		_get_all_items_in_dir(p_efsd->get_subdir(i), r_files, r_folders);
	}
	for (int i = 0; i < p_efsd->get_file_count(); i++) {
		r_files.push_back(p_efsd->get_file_path(i));
	}
}

void FileSystemDock::_find_file_owners(EditorFileSystemDirectory *p_efsd, const HashSet<String> &p_renames, HashSet<String> &r_file_owners) const {
	for (int i = 0; i < p_efsd->get_subdir_count(); i++) {
		_find_file_owners(p_efsd->get_subdir(i), p_renames, r_file_owners);
	}
	for (int i = 0; i < p_efsd->get_file_count(); i++) {
		Vector<String> deps = p_efsd->get_file_deps(i);
		for (int j = 0; j < deps.size(); j++) {
			if (p_renames.has(deps[j])) {
				r_file_owners.insert(p_efsd->get_file_path(i));
				break;
			}
		}
	}
}

void FileSystemDock::_try_move_item(const FileOrFolder &p_item, const String &p_new_path,
		HashMap<String, String> &p_file_renames, HashMap<String, String> &p_folder_renames) {
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

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
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

		if (p_item.is_file && FileAccess::exists(old_path + ".uid")) {
			err = da->rename(old_path + ".uid", new_path + ".uid");
			if (err != OK) {
				EditorNode::get_singleton()->add_io_error(TTR("Error moving:") + "\n" + old_path + ".uid\n");
			}
		}

		// Update scene if it is open.
		for (int i = 0; i < file_changed_paths.size(); ++i) {
			String new_item_path = p_item.is_file ? new_path : file_changed_paths[i].replace_first(old_path, new_path);
			if (ResourceLoader::get_resource_type(new_item_path) == "PackedScene" && EditorNode::get_singleton()->is_scene_open(file_changed_paths[i])) {
				EditorData *ed = &EditorNode::get_editor_data();
				for (int j = 0; j < ed->get_edited_scene_count(); j++) {
					if (ed->get_scene_path(j) == file_changed_paths[i]) {
						ed->get_edited_scene_root(j)->set_scene_file_path(new_item_path);
						EditorNode::get_singleton()->save_editor_layout_delayed();
						break;
					}
				}
			}
		}

		// Only treat as a changed dependency if it was successfully moved.
		for (int i = 0; i < file_changed_paths.size(); ++i) {
			p_file_renames[file_changed_paths[i]] = file_changed_paths[i].replace_first(old_path, new_path);
			print_verbose("  Remap: " + file_changed_paths[i] + " -> " + p_file_renames[file_changed_paths[i]]);
			emit_signal(SNAME("files_moved"), file_changed_paths[i], p_file_renames[file_changed_paths[i]]);
		}
		for (int i = 0; i < folder_changed_paths.size(); ++i) {
			p_folder_renames[folder_changed_paths[i]] = folder_changed_paths[i].replace_first(old_path, new_path);
			emit_signal(SNAME("folder_moved"), folder_changed_paths[i], p_folder_renames[folder_changed_paths[i]].substr(0, p_folder_renames[folder_changed_paths[i]].length() - 1));
		}
	} else {
		EditorNode::get_singleton()->add_io_error(TTR("Error moving:") + "\n" + old_path + "\n");
	}
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

	if (p_item.is_file) {
		print_verbose("Duplicating " + old_path + " -> " + new_path);

		// Create the directory structure.
		EditorFileSystem::get_singleton()->make_dir_recursive(p_new_path.get_base_dir());

		Error err = EditorFileSystem::get_singleton()->copy_file(old_path, new_path);
		if (err != OK) {
			EditorNode::get_singleton()->add_io_error(TTR("Error duplicating:") + "\n" + old_path + ": " + error_names[err] + "\n");
		}
	} else {
		Error err = EditorFileSystem::get_singleton()->copy_directory(old_path, new_path);
		if (err != OK) {
			EditorNode::get_singleton()->add_io_error(TTR("Error duplicating directory:") + "\n" + old_path + "\n");
		}
	}
}

void FileSystemDock::_update_resource_paths_after_move(const HashMap<String, String> &p_renames, const HashMap<String, ResourceUID::ID> &p_uids) const {
	for (const KeyValue<String, String> &pair : p_renames) {
		// Update UID path.
		const String &old_path = pair.key;
		const String &new_path = pair.value;

		const HashMap<String, ResourceUID::ID>::ConstIterator I = p_uids.find(old_path);
		if (I) {
			ResourceUID::get_singleton()->set_id(I->value, new_path);
		}
		EditorFileSystem::get_singleton()->register_global_class_script(old_path, new_path);
	}

	// Rename all resources loaded, be it subresources or actual resources.
	List<Ref<Resource>> cached;
	ResourceCache::get_cached_resources(&cached);

	for (Ref<Resource> &r : cached) {
		String base_path = r->get_path();
		String extra_path;
		int sep_pos = r->get_path().find("::");
		if (sep_pos >= 0) {
			extra_path = base_path.substr(sep_pos, base_path.length());
			base_path = base_path.substr(0, sep_pos);
		}

		if (p_renames.has(base_path)) {
			base_path = p_renames[base_path];
			r->set_path(base_path + extra_path);
		}
	}

	ScriptServer::save_global_classes();
	EditorNode::get_editor_data().script_class_save_icon_paths();
	EditorFileSystem::get_singleton()->emit_signal(SNAME("script_classes_updated"));
}

void FileSystemDock::_update_dependencies_after_move(const HashMap<String, String> &p_renames, const HashSet<String> &p_file_owners) const {
	// The following code assumes that the following holds:
	// 1) EditorFileSystem contains the old paths/folder structure from before the rename/move.
	// 2) ResourceLoader can use the new paths without needing to call rescan.

	// The currently edited scene should be reloaded first, so get it's path (GH-82652).
	const String &edited_scene_path = EditorNode::get_editor_data().get_scene_path(EditorNode::get_editor_data().get_edited_scene());
	List<String> scenes_to_reload;
	for (const String &E : p_file_owners) {
		// Because we haven't called a rescan yet the found remap might still be an old path itself.
		const HashMap<String, String>::ConstIterator I = p_renames.find(E);
		const String file = I ? I->value : E;
		print_verbose("Remapping dependencies for: " + file);
		const Error err = ResourceLoader::rename_dependencies(file, p_renames);
		if (err == OK) {
			if (ResourceLoader::get_resource_type(file) == "PackedScene") {
				if (file == edited_scene_path) {
					scenes_to_reload.push_front(file);
				} else {
					scenes_to_reload.push_back(file);
				}
			}
		} else {
			EditorNode::get_singleton()->add_io_error(TTR("Unable to update dependencies for:") + "\n" + E + "\n");
		}
	}

	for (const String &E : scenes_to_reload) {
		EditorNode::get_singleton()->reload_scene(E);
	}
}

void FileSystemDock::_update_project_settings_after_move(const HashMap<String, String> &p_renames, const HashMap<String, String> &p_folders_renames) {
	// Find all project settings of type FILE and replace them if needed.
	const HashMap<StringName, PropertyInfo> prop_info = ProjectSettings::get_singleton()->get_custom_property_info();
	for (const KeyValue<StringName, PropertyInfo> &E : prop_info) {
		if (E.value.hint == PROPERTY_HINT_FILE) {
			String old_path = GLOBAL_GET(E.key);
			if (p_renames.has(old_path)) {
				ProjectSettings::get_singleton()->set_setting(E.key, p_renames[old_path]);
			}
		};
	}

	// Also search for the file in autoload, as they are stored differently from normal files.
	List<PropertyInfo> property_list;
	ProjectSettings::get_singleton()->get_property_list(&property_list);
	for (const PropertyInfo &E : property_list) {
		if (E.name.begins_with("autoload/")) {
			// If the autoload resource paths has a leading "*", it indicates that it is a Singleton,
			// so we have to handle both cases when updating.
			String autoload = GLOBAL_GET(E.name);
			String autoload_singleton = autoload.substr(1, autoload.length());
			if (p_renames.has(autoload)) {
				ProjectSettings::get_singleton()->set_setting(E.name, p_renames[autoload]);
			} else if (autoload.begins_with("*") && p_renames.has(autoload_singleton)) {
				ProjectSettings::get_singleton()->set_setting(E.name, "*" + p_renames[autoload_singleton]);
			}
		}
	}

	// Update folder colors.
	for (const KeyValue<String, String> &rename : p_folders_renames) {
		if (assigned_folder_colors.has(rename.key)) {
			assigned_folder_colors[rename.value] = assigned_folder_colors[rename.key];
			assigned_folder_colors.erase(rename.key);
		}
	}
	ProjectSettings::get_singleton()->save();
}

String FileSystemDock::_get_unique_name(const FileOrFolder &p_entry, const String &p_at_path) {
	String new_path;
	String new_path_base;

	if (p_entry.is_file) {
		new_path = p_at_path.path_join(p_entry.path.get_file());
		new_path_base = new_path.get_basename() + " (%d)." + new_path.get_extension();
	} else {
		PackedStringArray path_split = p_entry.path.split("/");
		new_path = p_at_path.path_join(path_split[path_split.size() - 2]);
		new_path_base = new_path + " (%d)";
	}

	int exist_counter = 1;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	while (da->file_exists(new_path) || da->dir_exists(new_path)) {
		exist_counter++;
		new_path = vformat(new_path_base, exist_counter);
	}

	return new_path;
}

void FileSystemDock::_update_favorites_after_move(const HashMap<String, String> &p_files_renames, const HashMap<String, String> &p_folders_renames) const {
	Vector<String> favorite_files = EditorSettings::get_singleton()->get_favorites();
	Vector<String> new_favorite_files;
	for (const String &old_path : favorite_files) {
		if (p_folders_renames.has(old_path)) {
			new_favorite_files.push_back(p_folders_renames[old_path]);
		} else if (p_files_renames.has(old_path)) {
			new_favorite_files.push_back(p_files_renames[old_path]);
		} else {
			new_favorite_files.push_back(old_path);
		}
	}
	EditorSettings::get_singleton()->set_favorites(new_favorite_files);

	HashMap<String, PackedStringArray> favorite_properties = EditorSettings::get_singleton()->get_favorite_properties();
	for (const KeyValue<String, String> &KV : p_files_renames) {
		if (favorite_properties.has(KV.key)) {
			favorite_properties.replace_key(KV.key, KV.value);
		}
	}
	EditorSettings::get_singleton()->set_favorite_properties(favorite_properties);
}

void FileSystemDock::_make_scene_confirm() {
	const String scene_path = make_scene_dialog->get_scene_path();

	int idx = EditorNode::get_singleton()->new_scene();
	EditorNode::get_editor_data().set_scene_path(idx, scene_path);
	EditorNode::get_singleton()->set_edited_scene(make_scene_dialog->create_scene_root());
	EditorNode::get_singleton()->save_scene_if_open(scene_path);
}

void FileSystemDock::_resource_removed(const Ref<Resource> &p_resource) {
	const Ref<Script> &scr = p_resource;
	if (scr.is_valid()) {
		ScriptServer::remove_global_class_by_path(scr->get_path());
		ScriptServer::save_global_classes();
		EditorNode::get_editor_data().script_class_save_icon_paths();
		EditorFileSystem::get_singleton()->emit_signal(SNAME("script_classes_updated"));
	}
	emit_signal(SNAME("resource_removed"), p_resource);
}

void FileSystemDock::_file_removed(const String &p_file) {
	emit_signal(SNAME("file_removed"), p_file);

	// Find the closest parent directory available, in case multiple items were deleted along the same path.
	current_path = p_file.get_base_dir();
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	while (!da->dir_exists(current_path)) {
		current_path = current_path.get_base_dir();
	}

	current_path_line_edit->set_text(current_path);
}

void FileSystemDock::_folder_removed(const String &p_folder) {
	emit_signal(SNAME("folder_removed"), p_folder);

	// Find the closest parent directory available, in case multiple items were deleted along the same path.
	current_path = p_folder.get_base_dir();
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	while (!da->dir_exists(current_path)) {
		current_path = current_path.get_base_dir();
	}

	// Remove assigned folder color for all subfolders.
	bool folder_colors_updated = false;
	List<Variant> paths;
	assigned_folder_colors.get_key_list(&paths);
	for (const Variant &E : paths) {
		const String &path = E;
		// These folder paths are guaranteed to end with a "/".
		if (path.begins_with(p_folder)) {
			assigned_folder_colors.erase(path);
			folder_colors_updated = true;
		}
	}
	if (folder_colors_updated) {
		_update_folder_colors_setting();
	}

	current_path_line_edit->set_text(current_path);
	EditorFileSystemDirectory *efd = EditorFileSystem::get_singleton()->get_filesystem_path(current_path);
	if (efd) {
		efd->force_update();
	}
}

void FileSystemDock::_rename_operation_confirm() {
	String new_name;
	TreeItem *ti = tree->get_edited();
	int col_index = tree->get_edited_column();

	if (ti) {
		new_name = ti->get_text(col_index).strip_edges();
	} else {
		new_name = files->get_edit_text().strip_edges();
	}
	String old_name = to_rename.is_file ? to_rename.path.get_file() : to_rename.path.left(-1).get_file();

	bool rename_error = false;
	if (new_name.length() == 0) {
		EditorNode::get_singleton()->show_warning(TTR("No name provided."));
		rename_error = true;
	} else if (new_name.contains_char('/') || new_name.contains_char('\\') || new_name.contains_char(':')) {
		EditorNode::get_singleton()->show_warning(TTR("Name contains invalid characters."));
		rename_error = true;
	} else if (new_name[0] == '.') {
		EditorNode::get_singleton()->show_warning(TTR("This filename begins with a dot rendering the file invisible to the editor.\nIf you want to rename it anyway, use your operating system's file manager."));
		rename_error = true;
	} else if (to_rename.is_file && to_rename.path.get_extension() != new_name.get_extension()) {
		if (!EditorFileSystem::get_singleton()->get_valid_extensions().find(new_name.get_extension())) {
			EditorNode::get_singleton()->show_warning(TTR("This file extension is not recognized by the editor.\nIf you want to rename it anyway, use your operating system's file manager.\nAfter renaming to an unknown extension, the file won't be shown in the editor anymore."));
			rename_error = true;
		}
	}

	// Restore original name.
	if (rename_error) {
		if (ti) {
			ti->set_text(col_index, old_name);
		}
		return;
	}

	String old_path = to_rename.path.ends_with("/") ? to_rename.path.left(-1) : to_rename.path;
	String new_path = old_path.get_base_dir().path_join(new_name);
	if (old_path == new_path) {
		return;
	}

	if (EditorFileSystem::get_singleton()->is_group_file(old_path)) {
		EditorFileSystem::get_singleton()->move_group_file(old_path, new_path);
	}

	// Present a more user friendly warning for name conflict.
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);

	bool new_exist = (da->file_exists(new_path) || da->dir_exists(new_path));
	if (!da->is_case_sensitive(new_path.get_base_dir())) {
		new_exist = new_exist && (new_path.to_lower() != old_path.to_lower());
	}
	if (new_exist) {
		EditorNode::get_singleton()->show_warning(TTR("A file or folder with this name already exists."));
		if (ti) {
			ti->set_text(col_index, old_name);
		}
		return;
	}

	HashMap<String, ResourceUID::ID> uids;
	HashSet<String> file_owners; // The files that use these moved/renamed resource files.
	_before_move(uids, file_owners);

	HashMap<String, String> file_renames;
	HashMap<String, String> folder_renames;
	_try_move_item(to_rename, new_path, file_renames, folder_renames);

	int current_tab = EditorSceneTabs::get_singleton()->get_current_tab();
	_update_resource_paths_after_move(file_renames, uids);
	_update_dependencies_after_move(file_renames, file_owners);
	_update_project_settings_after_move(file_renames, folder_renames);
	_update_favorites_after_move(file_renames, folder_renames);

	EditorSceneTabs::get_singleton()->set_current_tab(current_tab);

	if (ti) {
		current_path = new_path;
		current_path_line_edit->set_text(current_path);
	}

	print_verbose("FileSystem: calling rescan.");
	_rescan();
}

void FileSystemDock::_duplicate_operation_confirm(const String &p_path) {
	const String base_dir = p_path.trim_suffix("/").get_base_dir();
	if (!DirAccess::dir_exists_absolute(base_dir)) {
		Error err = EditorFileSystem::get_singleton()->make_dir_recursive(base_dir);
		if (err != OK) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Could not create base directory: %s"), error_names[err]));
			return;
		}
	}
	_try_duplicate_item(to_duplicate, p_path);
}

void FileSystemDock::_overwrite_dialog_action(bool p_overwrite) {
	overwrite_dialog->hide();
	_move_operation_confirm(to_move_path, to_move_or_copy, p_overwrite ? OVERWRITE_REPLACE : OVERWRITE_RENAME);
}

void FileSystemDock::_convert_dialog_action() {
	Vector<Ref<Resource>> selected_resources;
	for (const String &S : to_convert) {
		Ref<Resource> res = ResourceLoader::load(S);
		ERR_FAIL_COND(res.is_null());
		selected_resources.push_back(res);
	}

	Vector<Ref<Resource>> converted_resources;
	HashSet<Ref<Resource>> resources_to_erase_history_for;
	for (Ref<Resource> res : selected_resources) {
		Vector<Ref<EditorResourceConversionPlugin>> conversions = EditorNode::get_singleton()->find_resource_conversion_plugin_for_resource(res);
		for (const Ref<EditorResourceConversionPlugin> &conversion : conversions) {
			int conversion_id = 0;
			for (const String &target : cached_valid_conversion_targets) {
				if (conversion_id == selected_conversion_id && conversion->converts_to() == target) {
					Ref<Resource> converted_res = conversion->convert(res);
					ERR_FAIL_COND(res.is_null());
					converted_resources.push_back(converted_res);
					resources_to_erase_history_for.insert(res);
					break;
				}
				conversion_id++;
			}
		}
	}

	// Clear history for the objects being replaced.
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	for (Ref<Resource> res : resources_to_erase_history_for) {
		undo_redo->clear_history(true, undo_redo->get_history_id_for_object(res.ptr()));
	}

	// Updates all the resources existing as node properties.
	EditorNode::get_singleton()->replace_resources_in_scenes(selected_resources, converted_resources);

	// Overwrite the old resources.
	for (int i = 0; i < converted_resources.size(); i++) {
		Ref<Resource> original_resource = selected_resources.get(i);
		Ref<Resource> new_resource = converted_resources.get(i);

		// Overwrite the path.
		new_resource->set_path(original_resource->get_path(), true);

		ResourceSaver::save(new_resource);
	}
}

Vector<String> FileSystemDock::_check_existing() {
	Vector<String> conflicting_items;
	for (const FileOrFolder &item : to_move) {
		String old_path = item.path.trim_suffix("/");
		String new_path = to_move_path.path_join(old_path.get_file());

		if ((item.is_file && FileAccess::exists(new_path)) || (!item.is_file && DirAccess::exists(new_path))) {
			conflicting_items.push_back(old_path);
		}
	}
	return conflicting_items;
}

void FileSystemDock::_move_operation_confirm(const String &p_to_path, bool p_copy, Overwrite p_overwrite) {
	if (p_overwrite == OVERWRITE_UNDECIDED) {
		to_move_path = p_to_path;
		to_move_or_copy = p_copy;

		Vector<String> conflicting_items = _check_existing();
		if (!conflicting_items.is_empty()) {
			// Ask to do something.
			overwrite_dialog_header->set_text(vformat(
					TTR("The following files or folders conflict with items in the target location '%s':"), to_move_path));
			overwrite_dialog_file_list->set_text(String("\n").join(conflicting_items));
			overwrite_dialog_footer->set_text(
					p_copy ? TTR("Do you wish to overwrite them or rename the copied files?")
						   : TTR("Do you wish to overwrite them or rename the moved files?"));
			overwrite_dialog->popup_centered();
			return;
		}
	}

	Vector<String> new_paths;
	new_paths.resize(to_move.size());
	for (int i = 0; i < to_move.size(); i++) {
		if (p_overwrite == OVERWRITE_RENAME) {
			new_paths.write[i] = _get_unique_name(to_move[i], p_to_path);
		} else {
			new_paths.write[i] = p_to_path.path_join(to_move[i].path.trim_suffix("/").get_file());
		}
	}

	if (p_copy) {
		bool is_copied = false;
		for (int i = 0; i < to_move.size(); i++) {
			if (to_move[i].path != new_paths[i]) {
				_try_duplicate_item(to_move[i], new_paths[i]);
				select_after_scan = new_paths[i];
				is_copied = true;
			}
		}

		if (is_copied) {
			_rescan();
		}
	} else {
		// Check groups.
		for (int i = 0; i < to_move.size(); i++) {
			if (to_move[i].is_file && EditorFileSystem::get_singleton()->is_group_file(to_move[i].path)) {
				EditorFileSystem::get_singleton()->move_group_file(to_move[i].path, new_paths[i]);
			}
		}

		HashMap<String, ResourceUID::ID> uids;
		HashSet<String> file_owners; // The files that use these moved/renamed resource files.
		_before_move(uids, file_owners);

		bool is_moved = false;
		HashMap<String, String> file_renames;
		HashMap<String, String> folder_renames;

		for (int i = 0; i < to_move.size(); i++) {
			if (to_move[i].path != new_paths[i]) {
				_try_move_item(to_move[i], new_paths[i], file_renames, folder_renames);
				is_moved = true;
			}
		}

		if (is_moved) {
			int current_tab = EditorSceneTabs::get_singleton()->get_current_tab();
			_update_resource_paths_after_move(file_renames, uids);
			_update_dependencies_after_move(file_renames, file_owners);
			_update_project_settings_after_move(file_renames, folder_renames);
			_update_favorites_after_move(file_renames, folder_renames);

			EditorSceneTabs::get_singleton()->set_current_tab(current_tab);

			print_verbose("FileSystem: calling rescan.");
			_rescan();

			current_path = p_to_path;
			current_path_line_edit->set_text(current_path);
		}
	}
}

void FileSystemDock::_before_move(HashMap<String, ResourceUID::ID> &r_uids, HashSet<String> &r_file_owners) const {
	HashSet<String> renamed_files;
	for (int i = 0; i < to_move.size(); i++) {
		if (to_move[i].is_file) {
			renamed_files.insert(to_move[i].path);
			ResourceUID::ID uid = ResourceLoader::get_resource_uid(to_move[i].path);
			if (uid != ResourceUID::INVALID_ID) {
				r_uids[to_move[i].path] = uid;
			}
		} else {
			EditorFileSystemDirectory *current_folder = EditorFileSystem::get_singleton()->get_filesystem_path(to_move[i].path);
			List<EditorFileSystemDirectory *> folders;
			folders.push_back(current_folder);
			while (folders.front()) {
				current_folder = folders.front()->get();
				for (int j = 0; j < current_folder->get_file_count(); j++) {
					const String file_path = current_folder->get_file_path(j);
					renamed_files.insert(file_path);
					ResourceUID::ID uid = ResourceLoader::get_resource_uid(file_path);
					if (uid != ResourceUID::INVALID_ID) {
						r_uids[file_path] = uid;
					}
				}
				for (int j = 0; j < current_folder->get_subdir_count(); j++) {
					folders.push_back(current_folder->get_subdir(j));
				}
				folders.pop_front();
			}
		}
	}

	// Look for files that use these moved/renamed resource files.
	_find_file_owners(EditorFileSystem::get_singleton()->get_filesystem(), renamed_files, r_file_owners);

	// Open scenes with dependencies on the ones about to be moved will be reloaded,
	// so save them first to prevent losing unsaved changes.
	EditorNode::get_singleton()->save_scene_list(r_file_owners);
}

Vector<String> FileSystemDock::_tree_get_selected(bool remove_self_inclusion, bool p_include_unselected_cursor) const {
	// Build a list of selected items with the active one at the first position.
	Vector<String> selected_strings;

	TreeItem *cursor_item = tree->get_selected();
	if (cursor_item && (p_include_unselected_cursor || cursor_item->is_selected(0)) && cursor_item != favorites_item) {
		selected_strings.push_back(cursor_item->get_metadata(0));
	}

	TreeItem *selected = tree->get_root();
	selected = tree->get_next_selected(selected);
	while (selected) {
		if (selected != cursor_item && selected != favorites_item) {
			selected_strings.push_back(selected->get_metadata(0));
		}
		selected = tree->get_next_selected(selected);
	}

	if (remove_self_inclusion) {
		selected_strings = _remove_self_included_paths(selected_strings);
	}
	return selected_strings;
}

Vector<String> FileSystemDock::_file_list_get_selected() const {
	Vector<String> selected;

	for (int idx : files->get_selected_items()) {
		selected.push_back(files->get_item_metadata(idx));
	}
	return selected;
}

Vector<String> FileSystemDock::_remove_self_included_paths(Vector<String> selected_strings) {
	// Remove paths or files that are included into another.
	if (selected_strings.size() > 1) {
		selected_strings.sort_custom<FileNoCaseComparator>();
		String last_path = "";
		for (int i = 0; i < selected_strings.size(); i++) {
			if (!last_path.is_empty() && selected_strings[i].begins_with(last_path)) {
				selected_strings.remove_at(i);
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
				tree->get_selected()->set_collapsed_recursive(p_option == FOLDER_COLLAPSE_ALL);
			}
		} break;
		case FILE_RENAME: {
			selected_strings = _tree_get_selected(false, true);
			[[fallthrough]];
		}
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

void FileSystemDock::_generic_rmb_option_selected(int p_option) {
	// Used for submenu commands where we don't know whether we're
	// calling from the file_list_rmb menu or the _tree_rmb option.
	if (files->has_focus()) {
		_file_list_rmb_option(p_option);
	} else {
		_tree_rmb_option(p_option);
	}
}

void FileSystemDock::_file_option(int p_option, const Vector<String> &p_selected) {
	// The first one should be the active item.

	switch (p_option) {
		case FILE_SHOW_IN_EXPLORER: {
			// Show the file/folder in the OS explorer.
			String fpath = current_path;
			if (current_path == "Favorites") {
				fpath = p_selected[0];
			}

			String dir = ProjectSettings::get_singleton()->globalize_path(fpath);
			OS::get_singleton()->shell_show_in_file_manager(dir, true);
		} break;

		case FILE_OPEN_EXTERNAL: {
			String fpath = current_path;
			if (current_path == "Favorites") {
				fpath = p_selected[0];
			}

			const String file = ProjectSettings::get_singleton()->globalize_path(fpath);
			const String extension = file.get_extension();

			const String resource_type = ResourceLoader::get_resource_type(fpath);
			String external_program;

			if (ClassDB::is_parent_class(resource_type, "Script") || extension == "tres" || extension == "tscn") {
				external_program = EDITOR_GET("text_editor/external/exec_path");
			} else if (extension == "res" || extension == "scn") {
				// Binary resources have no meaningful editor outside Godot, so just fallback to something default.
			} else if (resource_type == "CompressedTexture2D" || resource_type == "Image") {
				if (extension == "svg" || extension == "svgz") {
					external_program = EDITOR_GET("filesystem/external_programs/vector_image_editor");
				} else {
					external_program = EDITOR_GET("filesystem/external_programs/raster_image_editor");
				}
			} else if (ClassDB::is_parent_class(resource_type, "AudioStream")) {
				external_program = EDITOR_GET("filesystem/external_programs/audio_editor");
			} else if (resource_type == "PackedScene") {
				external_program = EDITOR_GET("filesystem/external_programs/3d_model_editor");
			}

			if (external_program.is_empty()) {
				OS::get_singleton()->shell_open(file);
			} else {
				List<String> args;
				args.push_back(file);
				OS::get_singleton()->create_process(external_program, args);
			}
		} break;

		case FILE_OPEN_IN_TERMINAL: {
			String fpath = current_path;
			if (current_path == "Favorites") {
				fpath = p_selected[0];
			}

			Vector<String> terminal_emulators;
			const String terminal_emulator_setting = EDITOR_GET("filesystem/external_programs/terminal_emulator");
			if (terminal_emulator_setting.is_empty()) {
				// Figure out a default terminal emulator to use.
#if defined(WINDOWS_ENABLED)
				// Default to PowerShell as done by Windows 10 and later.
				terminal_emulators.push_back("powershell");
#elif defined(MACOS_ENABLED)
				terminal_emulators.push_back("/System/Applications/Utilities/Terminal.app");
#elif defined(LINUXBSD_ENABLED)
				// Try terminal emulators that ship with common Linux distributions first.
				terminal_emulators.push_back("gnome-terminal");
				terminal_emulators.push_back("konsole");
				terminal_emulators.push_back("xfce4-terminal");
				terminal_emulators.push_back("lxterminal");
				terminal_emulators.push_back("kitty");
				terminal_emulators.push_back("alacritty");
				terminal_emulators.push_back("urxvt");
				terminal_emulators.push_back("xterm");
#endif
			} else {
				// Use the user-specified terminal.
				terminal_emulators.push_back(terminal_emulator_setting);
			}

			String flags = EDITOR_GET("filesystem/external_programs/terminal_emulator_flags");
			String arguments = flags;
			if (arguments.is_empty()) {
				// NOTE: This default value is ignored further below if the terminal executable is `powershell` or `cmd`,
				// due to these terminals requiring nonstandard syntax to start in a specified folder.
				arguments = "{directory}";
			}

#ifdef LINUXBSD_ENABLED
			String chosen_terminal_emulator;
			for (const String &terminal_emulator : terminal_emulators) {
				String pipe;
				List<String> test_args; // Required for `execute()`, as it doesn't accept `Vector<String>`.
				test_args.push_back("-cr");
				test_args.push_back("command -v " + terminal_emulator);
				const Error err = OS::get_singleton()->execute("bash", test_args, &pipe);
				// Check if a path to the terminal executable exists.
				if (err == OK && pipe.contains_char('/')) {
					chosen_terminal_emulator = terminal_emulator;
					break;
				} else if (err == ERR_CANT_FORK) {
					ERR_PRINT_ED(vformat(TTR("Couldn't run external program to check for terminal emulator presence: command -v %s"), terminal_emulator));
				}
			}
#else
			// On Windows and macOS, the first (and only) terminal emulator in the list is always available.
			String chosen_terminal_emulator = terminal_emulators[0];
#endif

			List<String> terminal_emulator_args; // Required for `execute()`, as it doesn't accept `Vector<String>`.
#ifdef LINUXBSD_ENABLED
			// Prepend default arguments based on the terminal emulator name.
			// Use `String.ends_with()` so that installations in non-default paths
			// or `/usr/local/bin` are detected correctly.
			if (flags.is_empty()) {
				if (chosen_terminal_emulator.ends_with("konsole")) {
					terminal_emulator_args.push_back("--workdir");
				} else if (chosen_terminal_emulator.ends_with("gnome-terminal")) {
					terminal_emulator_args.push_back("--working-directory");
				} else if (chosen_terminal_emulator.ends_with("urxvt")) {
					terminal_emulator_args.push_back("-cd");
				} else if (chosen_terminal_emulator.ends_with("xfce4-terminal")) {
					terminal_emulator_args.push_back("--working-directory");
				}
			}
#endif

			bool append_default_args = true;

#ifdef WINDOWS_ENABLED
			// Prepend default arguments based on the terminal emulator name.
			// Use `String.get_basename().to_lower()` to handle Windows' case-insensitive paths
			// with optional file extensions for executables in `PATH`.
			if (chosen_terminal_emulator.get_basename().to_lower() == "powershell") {
				terminal_emulator_args.push_back("-noexit");
				terminal_emulator_args.push_back("-command");
				terminal_emulator_args.push_back("cd '{directory}'");
				append_default_args = false;
			} else if (chosen_terminal_emulator.get_basename().to_lower() == "cmd") {
				terminal_emulator_args.push_back("/K");
				terminal_emulator_args.push_back("cd /d {directory}");
				append_default_args = false;
			}
#endif

			Vector<String> arguments_array = arguments.split(" ");
			for (const String &argument : arguments_array) {
				if (!append_default_args && argument == "{directory}") {
					// Prevent appending a `{directory}` placeholder twice when using powershell or cmd.
					// This allows users to enter the path to cmd or PowerShell in the custom terminal emulator path,
					// and make it work without having to enter custom arguments.
					continue;
				}
				terminal_emulator_args.push_back(argument);
			}

			const bool is_directory = fpath.ends_with("/");
			for (String &terminal_emulator_arg : terminal_emulator_args) {
				if (is_directory) {
					terminal_emulator_arg = terminal_emulator_arg.replace("{directory}", ProjectSettings::get_singleton()->globalize_path(fpath));
				} else {
					terminal_emulator_arg = terminal_emulator_arg.replace("{directory}", ProjectSettings::get_singleton()->globalize_path(fpath).get_base_dir());
				}
			}

			if (OS::get_singleton()->is_stdout_verbose()) {
				// Print full command line to help with troubleshooting.
				String command_string = chosen_terminal_emulator;
				for (const String &arg : terminal_emulator_args) {
					command_string += " " + arg;
				}
				print_line("Opening terminal emulator:", command_string);
			}

			const Error err = OS::get_singleton()->create_process(chosen_terminal_emulator, terminal_emulator_args, nullptr, true);
			if (err != OK) {
				String args_string;
				for (const String &terminal_emulator_arg : terminal_emulator_args) {
					args_string += terminal_emulator_arg;
				}
				ERR_PRINT_ED(vformat(TTR("Couldn't run external terminal program (error code %d): %s %s\nCheck `filesystem/external_programs/terminal_emulator` and `filesystem/external_programs/terminal_emulator_flags` in the Editor Settings."), err, chosen_terminal_emulator, args_string));
			}
		} break;

		case FILE_OPEN: {
			// Open folders.
			TreeItem *selected = tree->get_root();
			selected = tree->get_next_selected(selected);
			while (selected) {
				if (p_selected.has(selected->get_metadata(0))) {
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
				emit_signal(SNAME("inherit"), p_selected[0]);
			}
		} break;

		case FILE_MAIN_SCENE: {
			// Set as main scene with selected scene file.
			if (p_selected.size() == 1) {
				ProjectSettings::get_singleton()->set("application/run/main_scene", ResourceUID::path_to_uid(p_selected[0]));
				ProjectSettings::get_singleton()->save();
				_update_tree(get_uncollapsed_paths());
				_update_file_list(true);
			}
		} break;

		case FILE_INSTANTIATE: {
			// Instantiate all selected scenes.
			Vector<String> paths;
			for (int i = 0; i < p_selected.size(); i++) {
				const String &fpath = p_selected[i];
				if (EditorFileSystem::get_singleton()->get_file_type(fpath) == "PackedScene") {
					paths.push_back(fpath);
				}
			}
			if (!paths.is_empty()) {
				emit_signal(SNAME("instantiate"), paths);
			}
		} break;

		case FILE_ADD_FAVORITE: {
			// Add the files from favorites.
			Vector<String> favorites_list = EditorSettings::get_singleton()->get_favorites();
			for (int i = 0; i < p_selected.size(); i++) {
				if (!favorites_list.has(p_selected[i])) {
					favorites_list.push_back(p_selected[i]);
				}
			}
			EditorSettings::get_singleton()->set_favorites(favorites_list);
			_update_tree(get_uncollapsed_paths());
		} break;

		case FILE_REMOVE_FAVORITE: {
			// Remove the files from favorites.
			Vector<String> favorites_list = EditorSettings::get_singleton()->get_favorites();
			for (int i = 0; i < p_selected.size(); i++) {
				favorites_list.erase(p_selected[i]);
			}
			EditorSettings::get_singleton()->set_favorites(favorites_list);
			_update_tree(get_uncollapsed_paths());
			if (current_path == "Favorites") {
				_update_file_list(true);
			}
		} break;

		case FILE_SHOW_IN_FILESYSTEM: {
			if (!p_selected.is_empty()) {
				navigate_to_path(p_selected[0]);
			}
		} break;

		case FILE_DEPENDENCIES: {
			// Checkout the file dependencies.
			if (!p_selected.is_empty()) {
				const String &fpath = p_selected[0];
				deps_editor->edit(fpath);
			}
		} break;

		case FILE_OWNERS: {
			// Checkout the file owners.
			if (!p_selected.is_empty()) {
				const String &fpath = p_selected[0];
				owners_editor->show(fpath);
			}
		} break;

		case FILE_MOVE: {
			// Move or copy the files to a given location.
			to_move.clear();
			Vector<String> collapsed_paths = _remove_self_included_paths(p_selected);
			for (int i = collapsed_paths.size() - 1; i >= 0; i--) {
				const String &fpath = collapsed_paths[i];
				if (fpath != "res://") {
					to_move.push_back(FileOrFolder(fpath, !fpath.ends_with("/")));
				}
			}
			if (to_move.size() > 0) {
				move_dialog->config(p_selected);
				move_dialog->popup_centered_ratio(0.4);
			}
		} break;

		case FILE_RENAME: {
			if (!p_selected.is_empty()) {
				// Set to_rename variable for callback execution.
				to_rename.path = p_selected[0];
				to_rename.is_file = !to_rename.path.ends_with("/");
				if (to_rename.path == "res://") {
					break;
				}

				// Rename has same logic as move for resource files.
				to_move.clear();
				to_move.push_back(to_rename);

				if (tree->has_focus()) {
					// Edit node in Tree.
					tree->edit_selected(true);

					if (to_rename.is_file) {
						String name = to_rename.path.get_file();
						tree->set_editor_selection(0, name.rfind_char('.'));
					} else {
						String name = to_rename.path.left(-1).get_file(); // Removes the "/" suffix for folders.
						tree->set_editor_selection(0, name.length());
					}
				} else if (files->has_focus()) {
					files->edit_selected();
				}
			}
		} break;

		case FILE_REMOVE: {
			// Remove the selected files.
			Vector<String> remove_files;
			Vector<String> remove_folders;
			Vector<String> collapsed_paths = _remove_self_included_paths(p_selected);

			for (int i = 0; i < collapsed_paths.size(); i++) {
				const String &fpath = collapsed_paths[i];
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
			if (p_selected.size() != 1) {
				return;
			}

			to_duplicate.path = p_selected[0];
			to_duplicate.is_file = !to_duplicate.path.ends_with("/");
			if (to_duplicate.is_file) {
				String name = to_duplicate.path.get_file();
				make_dir_dialog->config(to_duplicate.path.get_base_dir(), callable_mp(this, &FileSystemDock::_duplicate_operation_confirm),
						DirectoryCreateDialog::MODE_FILE, TTR("Duplicating file:") + " " + name, name);
			} else {
				String name = to_duplicate.path.trim_suffix("/").get_file();
				make_dir_dialog->config(to_duplicate.path.trim_suffix("/").get_base_dir(), callable_mp(this, &FileSystemDock::_duplicate_operation_confirm),
						DirectoryCreateDialog::MODE_DIRECTORY, TTR("Duplicating folder:") + " " + name, name);
			}
			make_dir_dialog->popup_centered();
		} break;

		case FILE_REIMPORT: {
			ImportDock::get_singleton()->reimport_resources(p_selected);
		} break;

		case FILE_NEW_FOLDER: {
			String directory = current_path;
			if (!directory.ends_with("/")) {
				directory = directory.get_base_dir();
			}
			make_dir_dialog->config(directory, callable_mp(this, &FileSystemDock::create_directory).bind(directory),
					DirectoryCreateDialog::MODE_DIRECTORY, TTR("Create Folder"), "new folder");
			make_dir_dialog->popup_centered();
		} break;

		case FILE_NEW_SCENE: {
			String directory = current_path;
			if (!directory.ends_with("/")) {
				directory = directory.get_base_dir();
			}
			make_scene_dialog->config(directory);
			make_scene_dialog->popup_centered();
		} break;

		case FILE_NEW_SCRIPT: {
			String fpath = current_path;
			if (!fpath.ends_with("/")) {
				fpath = fpath.get_base_dir();
			}
			make_script_dialog->config("Node", fpath.path_join("new_script.gd"), false, false);
			make_script_dialog->popup_centered();
		} break;

		case FILE_COPY_PATH: {
			if (!p_selected.is_empty()) {
				const String &fpath = p_selected[0];
				DisplayServer::get_singleton()->clipboard_set(fpath);
			}
		} break;

		case FILE_COPY_ABSOLUTE_PATH: {
			if (!p_selected.is_empty()) {
				const String &fpath = p_selected[0];
				const String absolute_path = ProjectSettings::get_singleton()->globalize_path(fpath);
				DisplayServer::get_singleton()->clipboard_set(absolute_path);
			}
		} break;

		case FILE_COPY_UID: {
			if (!p_selected.is_empty()) {
				ResourceUID::ID uid = ResourceLoader::get_resource_uid(p_selected[0]);
				if (uid != ResourceUID::INVALID_ID) {
					String uid_string = ResourceUID::get_singleton()->id_to_text(uid);
					DisplayServer::get_singleton()->clipboard_set(uid_string);
				}
			}
		} break;

		case FILE_NEW_RESOURCE: {
			new_resource_dialog->popup_create(true);
		} break;
		case FILE_NEW_TEXTFILE: {
			String fpath = current_path;
			if (!fpath.ends_with("/")) {
				fpath = fpath.get_base_dir();
			}
			String dir = ProjectSettings::get_singleton()->globalize_path(fpath);
			ScriptEditor::get_singleton()->open_text_file_create_dialog(dir);
		} break;

		default: {
			if (p_option >= EditorContextMenuPlugin::BASE_ID) {
				if (!EditorContextMenuPluginManager::get_singleton()->activate_custom_option(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, p_option, p_selected)) {
					EditorContextMenuPluginManager::get_singleton()->activate_custom_option(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM_CREATE, p_option, p_selected);
				}
			} else if (p_option >= CONVERT_BASE_ID) {
				selected_conversion_id = p_option - CONVERT_BASE_ID;
				ERR_FAIL_INDEX(selected_conversion_id, (int)cached_valid_conversion_targets.size());

				to_convert.clear();
				for (const String &S : p_selected) {
					to_convert.push_back(S);
				}

				int conversion_id = 0;
				for (const String &E : cached_valid_conversion_targets) {
					if (conversion_id == selected_conversion_id) {
						conversion_dialog->set_text(vformat(TTR("Do you wish to convert these files to %s? (This operation cannot be undone!)"), E));
						conversion_dialog->popup_centered();
						break;
					}
					conversion_id++;
				}
			}
			break;
		}
	}
}

void FileSystemDock::_resource_created() {
	String fpath = current_path;
	if (!fpath.ends_with("/")) {
		fpath = fpath.get_base_dir();
	}

	String type_name = new_resource_dialog->get_selected_type();
	if (type_name == "Shader") {
		make_shader_dialog->config(fpath.path_join("new_shader"), false, false, 0);
		make_shader_dialog->popup_centered();
		return;
	} else if (type_name == "VisualShader") {
		make_shader_dialog->config(fpath.path_join("new_shader"), false, false, 1);
		make_shader_dialog->popup_centered();
		return;
	} else if (type_name == "ShaderInclude") {
		make_shader_dialog->config(fpath.path_join("new_shader_include"), false, false, 2);
		make_shader_dialog->popup_centered();
		return;
	}

	Variant c = new_resource_dialog->instantiate_selected();

	ERR_FAIL_COND(!c);
	Resource *r = Object::cast_to<Resource>(c);
	ERR_FAIL_NULL(r);

	PackedScene *scene = Object::cast_to<PackedScene>(r);
	if (scene) {
		Node *node = memnew(Node);
		node->set_name("Node");
		scene->pack(node);
		memdelete(node);
	}

	EditorNode::get_singleton()->push_item(r);
	EditorNode::get_singleton()->save_resource_as(Ref<Resource>(r), fpath);
}

void FileSystemDock::_search_changed(const String &p_text, const Control *p_from) {
	if (searched_tokens.is_empty()) {
		// Register the uncollapsed paths before they change.
		uncollapsed_paths_before_search = get_uncollapsed_paths();
	}

	const String searched_string = p_text.to_lower();
	searched_tokens = searched_string.split(" ", false);

	if (p_from == tree_search_box) {
		file_list_search_box->set_text(searched_string);
	} else { // File_list_search_box.
		tree_search_box->set_text(searched_string);
	}

	_update_filtered_items();
	if (display_mode == DISPLAY_MODE_HSPLIT || display_mode == DISPLAY_MODE_VSPLIT) {
		_update_file_list(false);
	}
	if (searched_tokens.is_empty()) {
		_navigate_to_path(current_path);
	}
}

bool FileSystemDock::_matches_all_search_tokens(const String &p_text) {
	if (searched_tokens.is_empty()) {
		return false;
	}
	const String s = p_text.to_lower();
	for (const String &t : searched_tokens) {
		if (!s.contains(t)) {
			return false;
		}
	}
	return true;
}

void FileSystemDock::_rescan() {
	_set_scanning_mode();
	EditorFileSystem::get_singleton()->scan();
}

void FileSystemDock::_change_bottom_dock_placement() {
	EditorDockManager::get_singleton()->bottom_dock_show_placement_popup(button_dock_placement->get_screen_rect(), this);
}

void FileSystemDock::_change_split_mode() {
	DisplayMode next_mode = DISPLAY_MODE_TREE_ONLY;
	if (display_mode == DISPLAY_MODE_VSPLIT) {
		next_mode = DISPLAY_MODE_HSPLIT;
	} else if (display_mode == DISPLAY_MODE_TREE_ONLY) {
		next_mode = DISPLAY_MODE_VSPLIT;
	}

	set_display_mode(next_mode);
	emit_signal(SNAME("display_mode_changed"));
}

void FileSystemDock::_split_dragged(int p_offset) {
	if (split_box->is_vertical()) {
		split_box_offset_v = p_offset;
	} else {
		split_box_offset_h = p_offset;
	}
}

void FileSystemDock::fix_dependencies(const String &p_for_file) {
	deps_editor->edit(p_for_file);
}

void FileSystemDock::update_all() {
	if (tree->is_visible()) {
		_update_tree(get_uncollapsed_paths(), false, false);
	}

	if (file_list_vb->is_visible()) {
		_update_file_list(true);
	}
}

void FileSystemDock::focus_on_path() {
	current_path_line_edit->grab_focus();
	current_path_line_edit->select_all();
}

void FileSystemDock::focus_on_filter() {
	LineEdit *current_search_box = nullptr;
	if (display_mode == DISPLAY_MODE_TREE_ONLY) {
		current_search_box = tree_search_box;
	} else {
		current_search_box = file_list_search_box;
	}

	if (current_search_box) {
		current_search_box->grab_focus();
		current_search_box->select_all();
	}
}

void FileSystemDock::create_directory(const String &p_path, const String &p_base_dir) {
	Error err = EditorFileSystem::get_singleton()->make_dir_recursive(p_path.trim_prefix(p_base_dir), p_base_dir);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Could not create folder: %s"), error_names[err]));
	}
}

ScriptCreateDialog *FileSystemDock::get_script_create_dialog() const {
	return make_script_dialog;
}

void FileSystemDock::set_file_list_display_mode(FileListDisplayMode p_mode) {
	if (p_mode == file_list_display_mode) {
		return;
	}

	_toggle_file_display();
}

void FileSystemDock::add_resource_tooltip_plugin(const Ref<EditorResourceTooltipPlugin> &p_plugin) {
	tooltip_plugins.push_back(p_plugin);
}

void FileSystemDock::remove_resource_tooltip_plugin(const Ref<EditorResourceTooltipPlugin> &p_plugin) {
	int index = tooltip_plugins.find(p_plugin);
	ERR_FAIL_COND_MSG(index == -1, "Can't remove plugin that wasn't registered.");
	tooltip_plugins.remove_at(index);
}

String FileSystemDock::get_folder_path_at_mouse_position() const {
	TreeItem *item = tree->get_item_at_position(tree->get_local_mouse_position());
	if (!item) {
		return String();
	}
	String fpath = item->get_metadata(0);
	return fpath.get_base_dir();
}

Control *FileSystemDock::create_tooltip_for_path(const String &p_path) const {
	if (p_path == "Favorites") {
		// No tooltip for the "Favorites" group.
		return nullptr;
	}
	if (DirAccess::exists(p_path)) {
		// No tooltip for directory.
		return nullptr;
	}
	ERR_FAIL_COND_V(!FileAccess::exists(p_path), nullptr);

	const String type = ResourceLoader::get_resource_type(p_path);
	Control *tooltip = EditorResourceTooltipPlugin::make_default_tooltip(p_path);

	for (const Ref<EditorResourceTooltipPlugin> &plugin : tooltip_plugins) {
		if (plugin->handles(type)) {
			tooltip = plugin->make_tooltip_for_path(p_path, EditorResourcePreview::get_singleton()->get_preview_metadata(p_path), tooltip);
		}
	}
	return tooltip;
}

Variant FileSystemDock::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	bool all_favorites = true;
	bool all_not_favorites = true;

	Vector<String> paths;

	if (p_from == tree) {
		// Check if the first selected is in favorite.
		TreeItem *selected = tree->get_next_selected(tree->get_root());
		while (selected) {
			if (selected == favorites_item) {
				// The "Favorites" item is not draggable.
				return Variant();
			}

			bool is_favorite = selected->get_parent() != nullptr && tree->get_root()->get_first_child() == selected->get_parent();
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

	if (paths.is_empty()) {
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
		return !favorite;
	}

	if (drag_data.has("type") && (String(drag_data["type"]) == "files" || String(drag_data["type"]) == "files_and_dirs")) {
		// Move files or dir.
		String to_dir;
		bool favorite;
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);

		if (favorite) {
			return true;
		}

		if (to_dir.is_empty()) {
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
		Vector<String> drag_files = drag_data["files"];
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
		for (int i = 0; i < drag_files.size(); i++) {
			int to_remove_pos = dirs.find(drag_files[i]);
			to_remove.push_back(to_remove_pos);
			if (to_remove_pos < drop_position) {
				offset++;
			}
		}
		drop_position -= offset;
		to_remove.sort();
		for (int i = 0; i < to_remove.size(); i++) {
			dirs.remove_at(to_remove[i] - i);
		}

		// Re-add them at the right position.
		for (int i = 0; i < drag_files.size(); i++) {
			dirs.insert(drop_position, drag_files[i]);
			drop_position++;
		}

		EditorSettings::get_singleton()->set_favorites(dirs);
		_update_tree(get_uncollapsed_paths());

		if (display_mode != DISPLAY_MODE_TREE_ONLY && current_path == "Favorites") {
			_update_file_list(true);
		}
		return;
	}

	if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
		// Moving resource.
		Ref<Resource> res = drag_data["resource"];
		String to_dir;
		bool favorite;
		tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);
		if (to_dir.is_empty()) {
			to_dir = get_current_directory();
		}

		if (res.is_valid() && !to_dir.is_empty()) {
			EditorNode::get_singleton()->push_item(res.ptr());
			EditorNode::get_singleton()->save_resource_as(res, to_dir);
		}
	}

	if (drag_data.has("type") && (String(drag_data["type"]) == "files" || String(drag_data["type"]) == "files_and_dirs")) {
		// Move files or add to favorites.
		String to_dir;
		bool favorite;
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);
		if (!to_dir.is_empty()) {
			Vector<String> fnames = drag_data["files"];
			to_move.clear();
			String target_dir = to_dir == "res://" ? to_dir : to_dir.trim_suffix("/");

			for (int i = 0; i < fnames.size(); i++) {
				if (fnames[i].trim_suffix("/").get_base_dir() != target_dir) {
					to_move.push_back(FileOrFolder(fnames[i], !fnames[i].ends_with("/")));
				}
			}
			if (!to_move.is_empty()) {
				if (Input::get_singleton()->is_key_pressed(Key::CMD_OR_CTRL)) {
					_move_operation_confirm(to_dir, true);
				} else {
					_move_operation_confirm(to_dir);
				}
			}
		} else if (favorite) {
			// Add the files from favorites.
			Vector<String> fnames = drag_data["files"];
			Vector<String> favorites_list = EditorSettings::get_singleton()->get_favorites();
			for (int i = 0; i < fnames.size(); i++) {
				if (!favorites_list.has(fnames[i])) {
					favorites_list.push_back(fnames[i]);
				}
			}
			EditorSettings::get_singleton()->set_favorites(favorites_list);
			_update_tree(get_uncollapsed_paths());
		}
	}

	if (drag_data.has("type") && String(drag_data["type"]) == "nodes") {
		String to_dir;
		bool favorite;
		tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
		_get_drag_target_folder(to_dir, favorite, p_point, p_from);
		if (to_dir.is_empty()) {
			to_dir = get_current_directory();
		}
		SceneTreeDock::get_singleton()->save_branch_to_file(to_dir);
	}
}

void FileSystemDock::_get_drag_target_folder(String &target, bool &target_favorites, const Point2 &p_point, Control *p_from) const {
	target = String();
	target_favorites = false;

	// In the file list.
	if (p_from == files) {
		int pos = files->get_item_at_position(p_point, true);
		if (pos == -1) {
			target = get_current_directory();
			return;
		}

		String ltarget = files->get_item_metadata(pos);
		target = ltarget.ends_with("/") ? ltarget : current_path.get_base_dir();
		return;
	}

	// In the tree.
	if (p_from == tree) {
		TreeItem *ti = tree->get_item_at_position(p_point);
		int section = tree->get_drop_section_at_position(p_point);
		if (ti) {
			// Check the favorites first.
			if (ti == tree->get_root()->get_first_child() && section >= 0) {
				target_favorites = true;
				return;
			} else if (ti->get_parent() == tree->get_root()->get_first_child()) {
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
					if (ti->get_parent() != tree->get_root()->get_first_child()) {
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

void FileSystemDock::_update_folder_colors_setting() {
	if (!ProjectSettings::get_singleton()->has_setting("file_customization/folder_colors")) {
		ProjectSettings::get_singleton()->set_setting("file_customization/folder_colors", assigned_folder_colors);
	} else if (assigned_folder_colors.is_empty()) {
		ProjectSettings::get_singleton()->set_setting("file_customization/folder_colors", Variant());
	}
	ProjectSettings::get_singleton()->save();
}

void FileSystemDock::_folder_color_index_pressed(int p_index, PopupMenu *p_menu) {
	Variant chosen_color_name = p_menu->get_item_metadata(p_index);
	Vector<String> selected;

	// Get all selected folders based on whether the files panel or tree panel is currently focused.
	if (files->has_focus()) {
		Vector<int> files_selected_ids = files->get_selected_items();
		for (int i = 0; i < files_selected_ids.size(); i++) {
			selected.push_back(files->get_item_metadata(files_selected_ids[i]));
		}
	} else {
		TreeItem *tree_selected = tree->get_root();
		tree_selected = tree->get_next_selected(tree_selected);
		while (tree_selected) {
			selected.push_back(tree_selected->get_metadata(0));
			tree_selected = tree->get_next_selected(tree_selected);
		}
	}

	// Update project settings with new folder colors.
	for (int i = 0; i < selected.size(); i++) {
		const String &fpath = selected[i];

		if (chosen_color_name) {
			assigned_folder_colors[fpath] = chosen_color_name;
		} else {
			assigned_folder_colors.erase(fpath);
		}
	}

	_update_folder_colors_setting();
	update_all();

	emit_signal(SNAME("folder_color_changed"));
}

void FileSystemDock::_file_and_folders_fill_popup(PopupMenu *p_popup, const Vector<String> &p_paths, bool p_display_path_dependent_options) {
	// Add options for files and folders.
	ERR_FAIL_COND_MSG(p_paths.is_empty(), "Path cannot be empty.");

	Vector<String> filenames;
	Vector<String> foldernames;

	Vector<String> favorites_list = EditorSettings::get_singleton()->get_favorites();

	bool all_files = true;
	bool all_files_scenes = true;
	bool all_folders = true;
	bool all_favorites = true;
	bool all_not_favorites = true;

	for (int i = 0; i < p_paths.size(); i++) {
		const String &fpath = p_paths[i];
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
		for (int j = 0; j < favorites_list.size(); j++) {
			if (favorites_list[j] == fpath) {
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
				p_popup->add_icon_item(get_editor_theme_icon(SNAME("Load")), TTR("Open Scene"), FILE_OPEN);
				p_popup->add_icon_item(get_editor_theme_icon(SNAME("CreateNewSceneFrom")), TTR("New Inherited Scene"), FILE_INHERIT);
				if (GLOBAL_GET("application/run/main_scene") != filenames[0]) {
					p_popup->add_icon_item(get_editor_theme_icon(SNAME("PlayScene")), TTR("Set as Main Scene"), FILE_MAIN_SCENE);
				}
			} else {
				p_popup->add_icon_item(get_editor_theme_icon(SNAME("Load")), TTR("Open Scenes"), FILE_OPEN);
			}
			p_popup->add_icon_item(get_editor_theme_icon(SNAME("Instance")), TTR("Instantiate"), FILE_INSTANTIATE);
			p_popup->add_separator();
		} else if (filenames.size() == 1) {
			p_popup->add_icon_item(get_editor_theme_icon(SNAME("Load")), TTR("Open"), FILE_OPEN);
			p_popup->add_separator();
		}

		if (filenames.size() == 1) {
			p_popup->add_item(TTR("Edit Dependencies..."), FILE_DEPENDENCIES);
			p_popup->add_item(TTR("View Owners..."), FILE_OWNERS);
			p_popup->add_separator();
		}
	}

	if (p_paths.size() == 1 && p_display_path_dependent_options) {
		PopupMenu *new_menu = memnew(PopupMenu);
		new_menu->connect(SceneStringName(id_pressed), callable_mp(this, &FileSystemDock::_generic_rmb_option_selected));

		p_popup->add_submenu_node_item(TTR("Create New"), new_menu, FILE_NEW);
		p_popup->set_item_icon(p_popup->get_item_index(FILE_NEW), get_editor_theme_icon(SNAME("Add")));

		new_menu->add_icon_item(get_editor_theme_icon(SNAME("Folder")), TTR("Folder..."), FILE_NEW_FOLDER);
		new_menu->add_icon_item(get_editor_theme_icon(SNAME("PackedScene")), TTR("Scene..."), FILE_NEW_SCENE);
		new_menu->add_icon_item(get_editor_theme_icon(SNAME("Script")), TTR("Script..."), FILE_NEW_SCRIPT);
		new_menu->add_icon_item(get_editor_theme_icon(SNAME("Object")), TTR("Resource..."), FILE_NEW_RESOURCE);
		new_menu->add_icon_item(get_editor_theme_icon(SNAME("TextFile")), TTR("TextFile..."), FILE_NEW_TEXTFILE);

		EditorContextMenuPluginManager::get_singleton()->add_options_from_plugins(new_menu, EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM_CREATE, p_paths);
		p_popup->add_separator();
	}

	// Check if the root path is selected, we must check p_paths[1] because the first string in
	// the list of paths obtained by _tree_get_selected(...) is not always the root path.
	bool root_path_not_selected = p_paths[0] != "res://" && (p_paths.size() <= 1 || p_paths[1] != "res://");

	if (all_folders && foldernames.size() > 0) {
		p_popup->add_icon_item(get_editor_theme_icon(SNAME("Load")), TTR("Expand Folder"), FILE_OPEN);

		if (foldernames.size() == 1) {
			p_popup->add_icon_item(get_editor_theme_icon(SNAME("GuiTreeArrowDown")), TTR("Expand Hierarchy"), FOLDER_EXPAND_ALL);
			p_popup->add_icon_item(get_editor_theme_icon(SNAME("GuiTreeArrowRight")), TTR("Collapse Hierarchy"), FOLDER_COLLAPSE_ALL);
		}

		p_popup->add_separator();

		// Only add the 'Set Folder Color...' option if the root path is not selected.
		if (root_path_not_selected) {
			PopupMenu *folder_colors_menu = memnew(PopupMenu);
			folder_colors_menu->connect(SceneStringName(id_pressed), callable_mp(this, &FileSystemDock::_folder_color_index_pressed).bind(folder_colors_menu));

			p_popup->add_submenu_node_item(TTR("Set Folder Color..."), folder_colors_menu);
			p_popup->set_item_icon(-1, get_editor_theme_icon(SNAME("Paint")));

			folder_colors_menu->add_icon_item(get_editor_theme_icon(SNAME("Folder")), TTR("Default (Reset)"));
			folder_colors_menu->set_item_icon_modulate(0, get_theme_color(SNAME("folder_icon_color"), SNAME("FileDialog")));
			folder_colors_menu->add_separator();

			for (const KeyValue<String, Color> &E : folder_colors) {
				folder_colors_menu->add_icon_item(get_editor_theme_icon(SNAME("Folder")), E.key.capitalize());

				folder_colors_menu->set_item_icon_modulate(-1, editor_is_dark_theme ? E.value : E.value * 2);
				folder_colors_menu->set_item_metadata(-1, E.key);
			}
		}
	}

	// Add the options that are only available when a single item is selected.
	if (p_paths.size() == 1) {
		p_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("ActionCopy")), ED_GET_SHORTCUT("filesystem_dock/copy_path"), FILE_COPY_PATH);
		p_popup->add_shortcut(ED_GET_SHORTCUT("filesystem_dock/copy_absolute_path"), FILE_COPY_ABSOLUTE_PATH);
		if (ResourceLoader::get_resource_uid(p_paths[0]) != ResourceUID::INVALID_ID) {
			p_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Instance")), ED_GET_SHORTCUT("filesystem_dock/copy_uid"), FILE_COPY_UID);
		}
		if (root_path_not_selected) {
			p_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Rename")), ED_GET_SHORTCUT("filesystem_dock/rename"), FILE_RENAME);
			p_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Duplicate")), ED_GET_SHORTCUT("filesystem_dock/duplicate"), FILE_DUPLICATE);
		}
	}

	// Add the options that are only available when the root path is not selected.
	if (root_path_not_selected) {
		p_popup->add_icon_item(get_editor_theme_icon(SNAME("MoveUp")), TTR("Move/Duplicate To..."), FILE_MOVE);
		p_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Remove")), ED_GET_SHORTCUT("filesystem_dock/delete"), FILE_REMOVE);
	}

	// Only add a separator if we have actually placed any options in the menu since the last separator.
	if (p_paths.size() == 1 || root_path_not_selected) {
		p_popup->add_separator();
	}

	// Add the options that are available when one or more items are selected.
	if (p_paths.size() >= 1) {
		if (!all_favorites) {
			p_popup->add_icon_item(get_editor_theme_icon(SNAME("Favorites")), TTR("Add to Favorites"), FILE_ADD_FAVORITE);
		}
		if (!all_not_favorites) {
			p_popup->add_icon_item(get_editor_theme_icon(SNAME("NonFavorite")), TTR("Remove from Favorites"), FILE_REMOVE_FAVORITE);
		}

		if (root_path_not_selected) {
			cached_valid_conversion_targets = _get_valid_conversions_for_file_paths(p_paths);

			int relative_id = 0;
			if (!cached_valid_conversion_targets.is_empty()) {
				p_popup->add_separator();

				// If we have more than one type we can convert into, collapse it into a submenu.
				const int CONVERSION_SUBMENU_THRESHOLD = 1;

				PopupMenu *container_menu = p_popup;
				String conversion_string_template = "Convert to %s";

				if (cached_valid_conversion_targets.size() > CONVERSION_SUBMENU_THRESHOLD) {
					container_menu = memnew(PopupMenu);
					container_menu->connect(SceneStringName(id_pressed), callable_mp(this, &FileSystemDock::_generic_rmb_option_selected));

					p_popup->add_submenu_node_item(TTR("Convert to..."), container_menu, FILE_NEW);
					conversion_string_template = "%s";
				}

				for (const String &E : cached_valid_conversion_targets) {
					Ref<Texture2D> icon;
					if (has_theme_icon(E, SNAME("EditorIcons"))) {
						icon = get_editor_theme_icon(E);
					} else {
						icon = get_editor_theme_icon(SNAME("Object"));
					}

					container_menu->add_icon_item(icon, vformat(TTR(conversion_string_template), E), CONVERT_BASE_ID + relative_id);
					relative_id++;
				}
			}
		}

		{
			List<String> resource_extensions;
			ResourceFormatImporter::get_singleton()->get_recognized_extensions_for_type("Resource", &resource_extensions);
			HashSet<String> extension_list;
			for (const String &extension : resource_extensions) {
				extension_list.insert(extension);
			}

			bool resource_valid = true;
			String main_extension;

			for (int i = 0; i != p_paths.size(); ++i) {
				String extension = p_paths[i].get_extension();
				if (extension_list.has(extension)) {
					if (main_extension.is_empty()) {
						main_extension = extension;
					} else if (extension != main_extension) {
						resource_valid = false;
						break;
					}
				} else {
					resource_valid = false;
					break;
				}
			}

			if (resource_valid) {
				p_popup->add_icon_item(get_editor_theme_icon(SNAME("Load")), TTR("Reimport"), FILE_REIMPORT);
			}
		}
	}

	if (p_paths.size() == 1) {
		const String &fpath = p_paths[0];

		[[maybe_unused]] bool added_separator = false;

		if (favorites_list.has(fpath)) {
			TreeItem *cursor_item = tree->get_selected();
			bool is_item_in_favorites = false;
			while (cursor_item != nullptr) {
				if (cursor_item == favorites_item) {
					is_item_in_favorites = true;
					break;
				}

				cursor_item = cursor_item->get_parent();
			}

			if (is_item_in_favorites) {
				p_popup->add_separator();
				added_separator = true;
				p_popup->add_icon_item(get_editor_theme_icon(SNAME("ShowInFileSystem")), TTR("Show in FileSystem"), FILE_SHOW_IN_FILESYSTEM);
			}
		}

#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
		if (!added_separator) {
			p_popup->add_separator();
			added_separator = true;
		}

		// Opening the system file manager is not supported on the Android and web editors.
		const bool is_directory = fpath.ends_with("/");

		p_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Terminal")), ED_GET_SHORTCUT("filesystem_dock/open_in_terminal"), FILE_OPEN_IN_TERMINAL);
		p_popup->set_item_text(p_popup->get_item_index(FILE_OPEN_IN_TERMINAL), is_directory ? TTR("Open in Terminal") : TTR("Open Folder in Terminal"));

		if (!is_directory) {
			p_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("ExternalLink")), ED_GET_SHORTCUT("filesystem_dock/open_in_external_program"), FILE_OPEN_EXTERNAL);
		}

		p_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Filesystem")), ED_GET_SHORTCUT("filesystem_dock/show_in_explorer"), FILE_SHOW_IN_EXPLORER);
		p_popup->set_item_text(p_popup->get_item_index(FILE_SHOW_IN_EXPLORER), is_directory ? TTR("Open in File Manager") : TTR("Show in File Manager"));
#endif

		current_path = fpath;
	}
	EditorContextMenuPluginManager::get_singleton()->add_options_from_plugins(p_popup, EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, p_paths);
}

void FileSystemDock::_tree_rmb_select(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}
	tree->grab_focus();

	// Right click is pressed in the tree.
	Vector<String> paths = _tree_get_selected(false);

	tree_popup->clear();

	// Popup.
	if (!paths.is_empty()) {
		tree_popup->reset_size();
		_file_and_folders_fill_popup(tree_popup, paths);
		tree_popup->set_position(tree->get_screen_position() + p_pos);
		tree_popup->reset_size();
		tree_popup->popup();
	}
}

void FileSystemDock::_tree_empty_click(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}
	// Right click is pressed in the empty space of the tree.
	current_path = "res://";
	tree_popup->clear();
	tree_popup->reset_size();
	tree_popup->add_icon_item(get_editor_theme_icon(SNAME("Folder")), TTR("New Folder..."), FILE_NEW_FOLDER);
	tree_popup->add_icon_item(get_editor_theme_icon(SNAME("PackedScene")), TTR("New Scene..."), FILE_NEW_SCENE);
	tree_popup->add_icon_item(get_editor_theme_icon(SNAME("Script")), TTR("New Script..."), FILE_NEW_SCRIPT);
	tree_popup->add_icon_item(get_editor_theme_icon(SNAME("Object")), TTR("New Resource..."), FILE_NEW_RESOURCE);
	tree_popup->add_icon_item(get_editor_theme_icon(SNAME("TextFile")), TTR("New TextFile..."), FILE_NEW_TEXTFILE);
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	// Opening the system file manager is not supported on the Android and web editors.
	tree_popup->add_separator();
	tree_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Terminal")), ED_GET_SHORTCUT("filesystem_dock/open_in_terminal"), FILE_OPEN_IN_TERMINAL);
	tree_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Filesystem")), ED_GET_SHORTCUT("filesystem_dock/show_in_explorer"), FILE_SHOW_IN_EXPLORER);
#endif

	tree_popup->set_position(tree->get_screen_position() + p_pos);
	tree_popup->reset_size();
	tree_popup->popup();
}

void FileSystemDock::_tree_empty_selected() {
	tree->deselect_all();
}

void FileSystemDock::_file_list_item_clicked(int p_item, const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index != MouseButton::RIGHT) {
		return;
	}
	files->grab_focus();

	// Right click is pressed in the file list.
	Vector<String> paths;
	for (int i = 0; i < files->get_item_count(); i++) {
		if (!files->is_selected(i)) {
			continue;
		}
		if (files->get_item_text(p_item) == "..") {
			files->deselect(i);
			continue;
		}
		paths.push_back(files->get_item_metadata(i));
	}

	// Popup.
	if (!paths.is_empty()) {
		file_list_popup->clear();
		_file_and_folders_fill_popup(file_list_popup, paths, searched_tokens.is_empty());
		file_list_popup->set_position(files->get_screen_position() + p_pos);
		file_list_popup->reset_size();
		file_list_popup->popup();
	}
}

void FileSystemDock::_file_list_empty_clicked(const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index != MouseButton::RIGHT) {
		return;
	}

	// Right click on empty space for file list.
	if (!searched_tokens.is_empty()) {
		return;
	}

	current_path = current_path_line_edit->get_text();

	// Favorites isn't a directory so don't show menu.
	if (current_path == "Favorites") {
		return;
	}

	file_list_popup->clear();
	file_list_popup->reset_size();

	file_list_popup->add_icon_item(get_editor_theme_icon(SNAME("Folder")), TTR("New Folder..."), FILE_NEW_FOLDER);
	file_list_popup->add_icon_item(get_editor_theme_icon(SNAME("PackedScene")), TTR("New Scene..."), FILE_NEW_SCENE);
	file_list_popup->add_icon_item(get_editor_theme_icon(SNAME("Script")), TTR("New Script..."), FILE_NEW_SCRIPT);
	file_list_popup->add_icon_item(get_editor_theme_icon(SNAME("Object")), TTR("New Resource..."), FILE_NEW_RESOURCE);
	file_list_popup->add_icon_item(get_editor_theme_icon(SNAME("TextFile")), TTR("New TextFile..."), FILE_NEW_TEXTFILE);
	file_list_popup->add_separator();
	file_list_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Terminal")), ED_GET_SHORTCUT("filesystem_dock/open_in_terminal"), FILE_OPEN_IN_TERMINAL);
	file_list_popup->add_icon_shortcut(get_editor_theme_icon(SNAME("Filesystem")), ED_GET_SHORTCUT("filesystem_dock/show_in_explorer"), FILE_SHOW_IN_EXPLORER);

	file_list_popup->set_position(files->get_screen_position() + p_pos);
	file_list_popup->reset_size();
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
			current_path = fpath;
		}
	}

	// Update the import dock.
	import_dock_needs_update = true;
	callable_mp(this, &FileSystemDock::_update_import_dock).call_deferred();
}

void FileSystemDock::_tree_mouse_exited() {
	if (holding_branch) {
		_reselect_items_selected_on_drag_begin();
	}
}

void FileSystemDock::_reselect_items_selected_on_drag_begin(bool reset) {
	TreeItem *selected_item = tree->get_next_selected(tree->get_root());
	if (selected_item) {
		selected_item->deselect(0);
	}
	if (!tree_items_selected_on_drag_begin.is_empty()) {
		bool reselected = false;
		for (TreeItem *item : tree_items_selected_on_drag_begin) {
			if (item->get_tree()) {
				item->select(0);
				reselected = true;
			}
		}

		if (reset) {
			tree_items_selected_on_drag_begin.clear();
		}

		if (!reselected) {
			// If couldn't reselect the items selected on drag begin, select the "res://" item.
			tree->get_root()->get_child(1)->select(0);
		}
	}

	files->deselect_all();
	if (!list_items_selected_on_drag_begin.is_empty()) {
		for (const int idx : list_items_selected_on_drag_begin) {
			files->select(idx, false);
		}

		if (reset) {
			list_items_selected_on_drag_begin.clear();
		}
	}
}

void FileSystemDock::_tree_gui_input(Ref<InputEvent> p_event) {
	Ref<InputEventKey> key = p_event;

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		TreeItem *item = tree->get_item_at_position(mm->get_position());
		if (item && holding_branch) {
			String fpath = item->get_metadata(0);
			while (!fpath.ends_with("/") && fpath != "res://" && item->get_parent()) { // Find the parent folder tree item.
				item = item->get_parent();
				fpath = item->get_metadata(0);
			}

			TreeItem *deselect_item = tree->get_next_selected(tree->get_root());
			while (deselect_item) {
				deselect_item->deselect(0);
				deselect_item = tree->get_next_selected(deselect_item);
			}
			item->select(0);

			if (display_mode != DisplayMode::DISPLAY_MODE_TREE_ONLY) {
				files->deselect_all();
				// Try to select the corresponding file list item.
				const int files_item_idx = files->find_metadata(fpath);
				if (files_item_idx != -1) {
					files->select(files_item_idx);
				}
			}
		}
	}

	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (ED_IS_SHORTCUT("filesystem_dock/duplicate", p_event)) {
			_tree_rmb_option(FILE_DUPLICATE);
		} else if (ED_IS_SHORTCUT("filesystem_dock/copy_path", p_event)) {
			_tree_rmb_option(FILE_COPY_PATH);
		} else if (ED_IS_SHORTCUT("filesystem_dock/copy_absolute_path", p_event)) {
			_tree_rmb_option(FILE_COPY_ABSOLUTE_PATH);
		} else if (ED_IS_SHORTCUT("filesystem_dock/copy_uid", p_event)) {
			_tree_rmb_option(FILE_COPY_UID);
		} else if (ED_IS_SHORTCUT("filesystem_dock/delete", p_event)) {
			_tree_rmb_option(FILE_REMOVE);
		} else if (ED_IS_SHORTCUT("filesystem_dock/rename", p_event)) {
			_tree_rmb_option(FILE_RENAME);
		} else if (ED_IS_SHORTCUT("filesystem_dock/show_in_explorer", p_event)) {
			_tree_rmb_option(FILE_SHOW_IN_EXPLORER);
		} else if (ED_IS_SHORTCUT("filesystem_dock/open_in_external_program", p_event)) {
			_tree_rmb_option(FILE_OPEN_EXTERNAL);
		} else if (ED_IS_SHORTCUT("filesystem_dock/open_in_terminal", p_event)) {
			_tree_rmb_option(FILE_OPEN_IN_TERMINAL);
		} else if (ED_IS_SHORTCUT("file_dialog/focus_path", p_event)) {
			focus_on_path();
		} else if (ED_IS_SHORTCUT("editor/open_search", p_event)) {
			focus_on_filter();
		} else {
			Callable custom_callback = EditorContextMenuPluginManager::get_singleton()->match_custom_shortcut(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, p_event);
			if (!custom_callback.is_valid()) {
				custom_callback = EditorContextMenuPluginManager::get_singleton()->match_custom_shortcut(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM_CREATE, p_event);
			}

			if (custom_callback.is_valid()) {
				EditorContextMenuPluginManager::get_singleton()->invoke_callback(custom_callback, _tree_get_selected(false));
			} else {
				return;
			}
		}

		accept_event();
	}
}

void FileSystemDock::_file_list_gui_input(Ref<InputEvent> p_event) {
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && holding_branch) {
		const int item_idx = files->get_item_at_position(mm->get_position(), true);
		files->deselect_all();
		String fpath;
		if (item_idx != -1) {
			fpath = files->get_item_metadata(item_idx);
			if (fpath.ends_with("/") || fpath == "res://") {
				files->select(item_idx);
			}
		} else {
			fpath = get_current_directory();
		}

		TreeItem *deselect_item = tree->get_next_selected(tree->get_root());
		while (deselect_item) {
			deselect_item->deselect(0);
			deselect_item = tree->get_next_selected(deselect_item);
		}

		// Try to select the corresponding tree item.
		TreeItem *tree_item = (item_idx != -1) ? tree->get_item_with_text(files->get_item_text(item_idx)) : nullptr;

		if (tree_item) {
			tree_item->select(0);
		} else {
			// Find parent folder.
			fpath = fpath.substr(0, fpath.rfind_char('/') + 1);
			if (fpath.size() > String("res://").size()) {
				fpath = fpath.left(fpath.size() - 2); // Remove last '/'.
				const int slash_idx = fpath.rfind_char('/');
				fpath = fpath.substr(slash_idx + 1, fpath.size() - slash_idx - 1);
			}

			tree_item = tree->get_item_with_text(fpath);
			if (tree_item) {
				tree_item->select(0);
			}
		}
	}

	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (ED_IS_SHORTCUT("filesystem_dock/duplicate", p_event)) {
			_file_list_rmb_option(FILE_DUPLICATE);
		} else if (ED_IS_SHORTCUT("filesystem_dock/copy_path", p_event)) {
			_file_list_rmb_option(FILE_COPY_PATH);
		} else if (ED_IS_SHORTCUT("filesystem_dock/copy_absolute_path", p_event)) {
			_file_list_rmb_option(FILE_COPY_ABSOLUTE_PATH);
		} else if (ED_IS_SHORTCUT("filesystem_dock/delete", p_event)) {
			_file_list_rmb_option(FILE_REMOVE);
		} else if (ED_IS_SHORTCUT("filesystem_dock/rename", p_event)) {
			_file_list_rmb_option(FILE_RENAME);
		} else if (ED_IS_SHORTCUT("filesystem_dock/show_in_explorer", p_event)) {
			_file_list_rmb_option(FILE_SHOW_IN_EXPLORER);
		} else if (ED_IS_SHORTCUT("filesystem_dock/open_in_terminal", p_event)) {
			_file_list_rmb_option(FILE_OPEN_IN_TERMINAL);
		} else if (ED_IS_SHORTCUT("editor/open_search", p_event)) {
			focus_on_filter();
		} else {
			Callable custom_callback = EditorContextMenuPluginManager::get_singleton()->match_custom_shortcut(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, p_event);
			if (!custom_callback.is_valid()) {
				custom_callback = EditorContextMenuPluginManager::get_singleton()->match_custom_shortcut(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM_CREATE, p_event);
			}

			if (custom_callback.is_valid()) {
				EditorContextMenuPluginManager::get_singleton()->invoke_callback(custom_callback, files->get_selected_items());
			} else {
				return;
			}
		}

		accept_event();
	}
}

bool FileSystemDock::_get_imported_files(const String &p_path, String &r_extension, Vector<String> &r_files) const {
	if (!p_path.ends_with("/")) {
		if (FileAccess::exists(p_path + ".import")) {
			if (r_extension.is_empty()) {
				r_extension = p_path.get_extension();
			} else if (r_extension != p_path.get_extension()) {
				r_files.clear();
				return false; // File type mismatch, stop search.
			}

			r_files.push_back(p_path);
		}
		return true;
	}

	Ref<DirAccess> da = DirAccess::open(p_path);
	ERR_FAIL_COND_V(da.is_null(), false);

	da->list_dir_begin();
	String n = da->get_next();
	while (!n.is_empty()) {
		if (n != "." && n != ".." && !n.ends_with(".import")) {
			String npath = p_path + n + (da->current_is_dir() ? "/" : "");
			if (!_get_imported_files(npath, r_extension, r_files)) {
				return false;
			}
		}
		n = da->get_next();
	}
	da->list_dir_end();
	return true;
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

	if (!selected.is_empty() && selected[0] == "res://") {
		// Scanning res:// is costly and unlikely to yield any useful results.
		return;
	}

	// Expand directory selection.
	Vector<String> efiles;
	String extension;
	for (const String &fpath : selected) {
		_get_imported_files(fpath, extension, efiles);
	}

	// Check import.
	Vector<String> imports;
	String import_type;
	for (int i = 0; i < efiles.size(); i++) {
		const String &fpath = efiles[i];
		Ref<ConfigFile> cf;
		cf.instantiate();
		Error err = cf->load(fpath + ".import");
		if (err != OK) {
			imports.clear();
			break;
		}

		String type;
		if (cf->has_section_key("remap", "type")) {
			type = cf->get_value("remap", "type");
		}
		if (import_type.is_empty()) {
			import_type = type;
		} else if (import_type != type) {
			// All should be the same type.
			imports.clear();
			break;
		}
		imports.push_back(fpath);
	}

	if (imports.size() == 0) {
		ImportDock::get_singleton()->clear();
	} else if (imports.size() == 1) {
		ImportDock::get_singleton()->set_edit_path(imports[0]);
	} else {
		ImportDock::get_singleton()->set_edit_multiple_paths(imports);
	}

	import_dock_needs_update = false;
}

void FileSystemDock::_feature_profile_changed() {
	_update_display_mode(true);
}

void FileSystemDock::_project_settings_changed() {
	assigned_folder_colors = ProjectSettings::get_singleton()->get_setting("file_customization/folder_colors");
}

void FileSystemDock::set_file_sort(FileSortOption p_file_sort) {
	for (int i = 0; i != (int)FileSortOption::FILE_SORT_MAX; i++) {
		tree_button_sort->get_popup()->set_item_checked(i, (i == (int)p_file_sort));
		file_list_button_sort->get_popup()->set_item_checked(i, (i == (int)p_file_sort));
	}
	file_sort = p_file_sort;

	// Update everything needed.
	update_all();
}

void FileSystemDock::_file_sort_popup(int p_id) {
	set_file_sort((FileSortOption)p_id);
}

const HashMap<String, Color> &FileSystemDock::get_folder_colors() const {
	return folder_colors;
}

Dictionary FileSystemDock::get_assigned_folder_colors() const {
	return assigned_folder_colors;
}

MenuButton *FileSystemDock::_create_file_menu_button() {
	MenuButton *button = memnew(MenuButton);
	button->set_flat(false);
	button->set_theme_type_variation("FlatMenuButton");
	button->set_tooltip_text(TTR("Sort Files"));

	PopupMenu *p = button->get_popup();
	p->connect(SceneStringName(id_pressed), callable_mp(this, &FileSystemDock::_file_sort_popup));
	p->add_radio_check_item(TTR("Sort by Name (Ascending)"), (int)FileSortOption::FILE_SORT_NAME);
	p->add_radio_check_item(TTR("Sort by Name (Descending)"), (int)FileSortOption::FILE_SORT_NAME_REVERSE);
	p->add_radio_check_item(TTR("Sort by Type (Ascending)"), (int)FileSortOption::FILE_SORT_TYPE);
	p->add_radio_check_item(TTR("Sort by Type (Descending)"), (int)FileSortOption::FILE_SORT_TYPE_REVERSE);
	p->add_radio_check_item(TTR("Sort by Last Modified"), (int)FileSortOption::FILE_SORT_MODIFIED_TIME);
	p->add_radio_check_item(TTR("Sort by First Modified"), (int)FileSortOption::FILE_SORT_MODIFIED_TIME_REVERSE);
	p->set_item_checked((int)file_sort, true);
	return button;
}

bool FileSystemDock::_can_dock_horizontal() const {
	return true;
}

void FileSystemDock::_set_dock_horizontal(bool p_enable) {
	if (button_dock_placement->is_visible() == p_enable) {
		return;
	}

	if (p_enable) {
		set_meta("_dock_display_mode", get_display_mode());
		set_meta("_dock_file_display_mode", get_file_list_display_mode());

		FileSystemDock::DisplayMode new_display_mode = FileSystemDock::DisplayMode(int(get_meta("_bottom_display_mode", int(FileSystemDock::DISPLAY_MODE_HSPLIT))));
		FileSystemDock::FileListDisplayMode new_file_display_mode = FileSystemDock::FileListDisplayMode(int(get_meta("_bottom_file_display_mode", int(FileSystemDock::FILE_LIST_DISPLAY_THUMBNAILS))));

		set_display_mode(new_display_mode);
		set_file_list_display_mode(new_file_display_mode);
		set_custom_minimum_size(Size2(0, 200) * EDSCALE);
	} else {
		set_meta("_bottom_display_mode", get_display_mode());
		set_meta("_bottom_file_display_mode", get_file_list_display_mode());

		FileSystemDock::DisplayMode new_display_mode = FileSystemDock::DISPLAY_MODE_TREE_ONLY;
		FileSystemDock::FileListDisplayMode new_file_display_mode = FileSystemDock::FILE_LIST_DISPLAY_LIST;

		new_display_mode = FileSystemDock::DisplayMode(int(get_meta("_dock_display_mode", new_display_mode)));
		new_file_display_mode = FileSystemDock::FileListDisplayMode(int(get_meta("_dock_file_display_mode", new_file_display_mode)));

		set_display_mode(new_display_mode);
		set_file_list_display_mode(new_file_display_mode);
		set_custom_minimum_size(Size2(0, 0));
	}

	button_dock_placement->set_visible(p_enable);
}

void FileSystemDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_file_list_thumbnail_done"), &FileSystemDock::_file_list_thumbnail_done);
	ClassDB::bind_method(D_METHOD("_tree_thumbnail_done"), &FileSystemDock::_tree_thumbnail_done);

	ClassDB::bind_method(D_METHOD("navigate_to_path", "path"), &FileSystemDock::navigate_to_path);

	ClassDB::bind_method(D_METHOD("add_resource_tooltip_plugin", "plugin"), &FileSystemDock::add_resource_tooltip_plugin);
	ClassDB::bind_method(D_METHOD("remove_resource_tooltip_plugin", "plugin"), &FileSystemDock::remove_resource_tooltip_plugin);

	ClassDB::bind_method(D_METHOD("_set_dock_horizontal", "enable"), &FileSystemDock::_set_dock_horizontal);
	ClassDB::bind_method(D_METHOD("_can_dock_horizontal"), &FileSystemDock::_can_dock_horizontal);

	ClassDB::bind_method(D_METHOD("_save_layout_to_config"), &FileSystemDock::_save_layout_to_config);
	ClassDB::bind_method(D_METHOD("_load_layout_from_config"), &FileSystemDock::_load_layout_from_config);

	ADD_SIGNAL(MethodInfo("inherit", PropertyInfo(Variant::STRING, "file")));
	ADD_SIGNAL(MethodInfo("instantiate", PropertyInfo(Variant::PACKED_STRING_ARRAY, "files")));

	ADD_SIGNAL(MethodInfo("resource_removed", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
	ADD_SIGNAL(MethodInfo("file_removed", PropertyInfo(Variant::STRING, "file")));
	ADD_SIGNAL(MethodInfo("folder_removed", PropertyInfo(Variant::STRING, "folder")));
	ADD_SIGNAL(MethodInfo("files_moved", PropertyInfo(Variant::STRING, "old_file"), PropertyInfo(Variant::STRING, "new_file")));
	ADD_SIGNAL(MethodInfo("folder_moved", PropertyInfo(Variant::STRING, "old_folder"), PropertyInfo(Variant::STRING, "new_folder")));
	ADD_SIGNAL(MethodInfo("folder_color_changed"));

	ADD_SIGNAL(MethodInfo("display_mode_changed"));
}

void FileSystemDock::_save_layout_to_config(Ref<ConfigFile> p_layout, const String &p_section) const {
	p_layout->set_value(p_section, "dock_filesystem_h_split_offset", get_h_split_offset());
	p_layout->set_value(p_section, "dock_filesystem_v_split_offset", get_v_split_offset());
	p_layout->set_value(p_section, "dock_filesystem_display_mode", get_display_mode());
	p_layout->set_value(p_section, "dock_filesystem_file_sort", (int)get_file_sort());
	p_layout->set_value(p_section, "dock_filesystem_file_list_display_mode", get_file_list_display_mode());
	PackedStringArray selected_files = get_selected_paths();
	p_layout->set_value(p_section, "dock_filesystem_selected_paths", selected_files);
	Vector<String> uncollapsed_paths = get_uncollapsed_paths();
	p_layout->set_value(p_section, "dock_filesystem_uncollapsed_paths", searched_tokens.is_empty() ? uncollapsed_paths : uncollapsed_paths_before_search);
}

void FileSystemDock::_load_layout_from_config(Ref<ConfigFile> p_layout, const String &p_section) {
	if (p_layout->has_section_key(p_section, "dock_filesystem_h_split_offset")) {
		int fs_h_split_ofs = p_layout->get_value(p_section, "dock_filesystem_h_split_offset");
		set_h_split_offset(fs_h_split_ofs);
	}

	if (p_layout->has_section_key(p_section, "dock_filesystem_v_split_offset")) {
		int fs_v_split_ofs = p_layout->get_value(p_section, "dock_filesystem_v_split_offset");
		set_v_split_offset(fs_v_split_ofs);
	}

	if (p_layout->has_section_key(p_section, "dock_filesystem_display_mode")) {
		DisplayMode dock_filesystem_display_mode = DisplayMode(int(p_layout->get_value(p_section, "dock_filesystem_display_mode")));
		set_display_mode(dock_filesystem_display_mode);
	}

	if (p_layout->has_section_key(p_section, "dock_filesystem_file_sort")) {
		FileSortOption dock_filesystem_file_sort = FileSortOption(int(p_layout->get_value(p_section, "dock_filesystem_file_sort")));
		set_file_sort(dock_filesystem_file_sort);
	}

	if (p_layout->has_section_key(p_section, "dock_filesystem_file_list_display_mode")) {
		FileListDisplayMode dock_filesystem_file_list_display_mode = FileListDisplayMode(int(p_layout->get_value(p_section, "dock_filesystem_file_list_display_mode")));
		set_file_list_display_mode(dock_filesystem_file_list_display_mode);
	}

	if (p_layout->has_section_key(p_section, "dock_filesystem_selected_paths")) {
		PackedStringArray dock_filesystem_selected_paths = p_layout->get_value(p_section, "dock_filesystem_selected_paths");
		for (int i = 0; i < dock_filesystem_selected_paths.size(); i++) {
			select_file(dock_filesystem_selected_paths[i]);
		}
	}

	// Restore collapsed state.
	PackedStringArray uncollapsed_tis;
	if (p_layout->has_section_key(p_section, "dock_filesystem_uncollapsed_paths")) {
		uncollapsed_tis = p_layout->get_value(p_section, "dock_filesystem_uncollapsed_paths");
	} else {
		uncollapsed_tis = { "res://" };
	}

	if (!uncollapsed_tis.is_empty()) {
		for (int i = 0; i < uncollapsed_tis.size(); i++) {
			TreeItem *uncollapsed_ti = get_tree_control()->get_item_with_metadata(uncollapsed_tis[i], 0);
			if (uncollapsed_ti) {
				uncollapsed_ti->set_collapsed(false);
			}
		}
		get_tree_control()->queue_redraw();
	}
}

FileSystemDock::FileSystemDock() {
	singleton = this;
	set_name("FileSystem");
	current_path = "res://";

	ProjectSettings::get_singleton()->add_hidden_prefix("file_customization/");

	// `KeyModifierMask::CMD_OR_CTRL | Key::C` conflicts with other editor shortcuts.
	ED_SHORTCUT("filesystem_dock/copy_path", TTRC("Copy Path"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::C);
	ED_SHORTCUT("filesystem_dock/copy_absolute_path", TTRC("Copy Absolute Path"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::C);
	ED_SHORTCUT("filesystem_dock/copy_uid", TTRC("Copy UID"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | KeyModifierMask::SHIFT | Key::C);
	ED_SHORTCUT("filesystem_dock/duplicate", TTRC("Duplicate..."), KeyModifierMask::CMD_OR_CTRL | Key::D);
	ED_SHORTCUT("filesystem_dock/delete", TTRC("Delete"), Key::KEY_DELETE);
	ED_SHORTCUT("filesystem_dock/rename", TTRC("Rename..."), Key::F2);
	ED_SHORTCUT_OVERRIDE("filesystem_dock/rename", "macos", Key::ENTER);
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	// Opening the system file manager or opening in an external program is not supported on the Android and web editors.
	ED_SHORTCUT("filesystem_dock/show_in_explorer", TTRC("Open in File Manager"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::R);
	ED_SHORTCUT("filesystem_dock/open_in_external_program", TTRC("Open in External Program"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::E);
	ED_SHORTCUT("filesystem_dock/open_in_terminal", TTRC("Open in Terminal"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::T);
#endif

	// Properly translating color names would require a separate HashMap, so for simplicity they are provided as comments.
	folder_colors["red"] = Color(1.0, 0.271, 0.271); // TTR("Red")
	folder_colors["orange"] = Color(1.0, 0.561, 0.271); // TTR("Orange")
	folder_colors["yellow"] = Color(1.0, 0.890, 0.271); // TTR("Yellow")
	folder_colors["green"] = Color(0.502, 1.0, 0.271); // TTR("Green")
	folder_colors["teal"] = Color(0.271, 1.0, 0.635); // TTR("Teal")
	folder_colors["blue"] = Color(0.271, 0.843, 1.0); // TTR("Blue")
	folder_colors["purple"] = Color(0.502, 0.271, 1.0); // TTR("Purple")
	folder_colors["pink"] = Color(1.0, 0.271, 0.588); // TTR("Pink")
	folder_colors["gray"] = Color(0.616, 0.616, 0.616); // TTR("Gray")

	assigned_folder_colors = ProjectSettings::get_singleton()->get_setting("file_customization/folder_colors");

	editor_is_dark_theme = EditorThemeManager::is_dark_theme();

	VBoxContainer *top_vbc = memnew(VBoxContainer);
	add_child(top_vbc);

	HBoxContainer *toolbar_hbc = memnew(HBoxContainer);
	top_vbc->add_child(toolbar_hbc);

	HBoxContainer *nav_hbc = memnew(HBoxContainer);
	nav_hbc->add_theme_constant_override("separation", 0);
	toolbar_hbc->add_child(nav_hbc);

	button_hist_prev = memnew(Button);
	button_hist_prev->set_flat(true);
	button_hist_prev->set_disabled(true);
	button_hist_prev->set_focus_mode(FOCUS_NONE);
	button_hist_prev->set_tooltip_text(TTR("Go to previous selected folder/file."));
	nav_hbc->add_child(button_hist_prev);

	button_hist_next = memnew(Button);
	button_hist_next->set_flat(true);
	button_hist_next->set_disabled(true);
	button_hist_next->set_focus_mode(FOCUS_NONE);
	button_hist_next->set_tooltip_text(TTR("Go to next selected folder/file."));
	nav_hbc->add_child(button_hist_next);

	current_path_line_edit = memnew(LineEdit);
	current_path_line_edit->set_structured_text_bidi_override(TextServer::STRUCTURED_TEXT_FILE);
	current_path_line_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	_set_current_path_line_edit_text(current_path);
	toolbar_hbc->add_child(current_path_line_edit);

	button_reload = memnew(Button);
	button_reload->connect(SceneStringName(pressed), callable_mp(this, &FileSystemDock::_rescan));
	button_reload->set_focus_mode(FOCUS_NONE);
	button_reload->set_tooltip_text(TTR("Re-Scan Filesystem"));
	button_reload->hide();
	toolbar_hbc->add_child(button_reload);

	button_toggle_display_mode = memnew(Button);
	button_toggle_display_mode->connect(SceneStringName(pressed), callable_mp(this, &FileSystemDock::_change_split_mode));
	button_toggle_display_mode->set_focus_mode(FOCUS_NONE);
	button_toggle_display_mode->set_tooltip_text(TTR("Change Split Mode"));
	button_toggle_display_mode->set_theme_type_variation("FlatMenuButton");
	toolbar_hbc->add_child(button_toggle_display_mode);

	button_dock_placement = memnew(Button);
	button_dock_placement->set_theme_type_variation("FlatMenuButton");
	button_dock_placement->connect(SceneStringName(pressed), callable_mp(this, &FileSystemDock::_change_bottom_dock_placement));
	button_dock_placement->hide();
	toolbar_hbc->add_child(button_dock_placement);

	toolbar2_hbc = memnew(HBoxContainer);
	top_vbc->add_child(toolbar2_hbc);

	tree_search_box = memnew(LineEdit);
	tree_search_box->set_h_size_flags(SIZE_EXPAND_FILL);
	tree_search_box->set_placeholder(TTR("Filter Files"));
	tree_search_box->set_clear_button_enabled(true);
	tree_search_box->connect(SceneStringName(text_changed), callable_mp(this, &FileSystemDock::_search_changed).bind(tree_search_box));
	toolbar2_hbc->add_child(tree_search_box);

	tree_button_sort = _create_file_menu_button();
	toolbar2_hbc->add_child(tree_button_sort);

	file_list_popup = memnew(PopupMenu);

	add_child(file_list_popup);

	tree_popup = memnew(PopupMenu);

	add_child(tree_popup);

	split_box = memnew(SplitContainer);
	split_box->set_v_size_flags(SIZE_EXPAND_FILL);
	split_box->connect("dragged", callable_mp(this, &FileSystemDock::_split_dragged));
	split_box_offset_h = 240 * EDSCALE;
	add_child(split_box);

	tree = memnew(FileSystemTree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_hide_root(true);
	SET_DRAG_FORWARDING_GCD(tree, FileSystemDock);
	tree->set_allow_rmb_select(true);
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->set_custom_minimum_size(Size2(40 * EDSCALE, 15 * EDSCALE));
	tree->set_column_clip_content(0, true);
	split_box->add_child(tree);

	tree->connect("item_activated", callable_mp(this, &FileSystemDock::_tree_activate_file));
	tree->connect("multi_selected", callable_mp(this, &FileSystemDock::_tree_multi_selected));
	tree->connect("item_mouse_selected", callable_mp(this, &FileSystemDock::_tree_rmb_select));
	tree->connect("empty_clicked", callable_mp(this, &FileSystemDock::_tree_empty_click));
	tree->connect("nothing_selected", callable_mp(this, &FileSystemDock::_tree_empty_selected));
	tree->connect(SceneStringName(gui_input), callable_mp(this, &FileSystemDock::_tree_gui_input));
	tree->connect(SceneStringName(mouse_exited), callable_mp(this, &FileSystemDock::_tree_mouse_exited));
	tree->connect("item_edited", callable_mp(this, &FileSystemDock::_rename_operation_confirm));

	file_list_vb = memnew(VBoxContainer);
	file_list_vb->set_v_size_flags(SIZE_EXPAND_FILL);
	split_box->add_child(file_list_vb);

	path_hb = memnew(HBoxContainer);
	file_list_vb->add_child(path_hb);

	file_list_search_box = memnew(LineEdit);
	file_list_search_box->set_h_size_flags(SIZE_EXPAND_FILL);
	file_list_search_box->set_placeholder(TTR("Filter Files"));
	file_list_search_box->set_clear_button_enabled(true);
	file_list_search_box->connect(SceneStringName(text_changed), callable_mp(this, &FileSystemDock::_search_changed).bind(file_list_search_box));
	path_hb->add_child(file_list_search_box);

	file_list_button_sort = _create_file_menu_button();
	path_hb->add_child(file_list_button_sort);

	button_file_list_display_mode = memnew(Button);
	button_file_list_display_mode->set_theme_type_variation("FlatMenuButton");
	path_hb->add_child(button_file_list_display_mode);

	files = memnew(FileSystemList);
	files->set_v_size_flags(SIZE_EXPAND_FILL);
	files->set_select_mode(ItemList::SELECT_MULTI);
	files->set_theme_type_variation("ItemListSecondary");
	SET_DRAG_FORWARDING_GCD(files, FileSystemDock);
	files->connect("item_clicked", callable_mp(this, &FileSystemDock::_file_list_item_clicked));
	files->connect(SceneStringName(gui_input), callable_mp(this, &FileSystemDock::_file_list_gui_input));
	files->connect("multi_selected", callable_mp(this, &FileSystemDock::_file_multi_selected));
	files->connect("empty_clicked", callable_mp(this, &FileSystemDock::_file_list_empty_clicked));
	files->connect("item_edited", callable_mp(this, &FileSystemDock::_rename_operation_confirm));
	files->set_custom_minimum_size(Size2(0, 15 * EDSCALE));
	files->set_allow_rmb_select(true);
	file_list_vb->add_child(files);

	scanning_vb = memnew(VBoxContainer);
	scanning_vb->hide();
	add_child(scanning_vb);

	Label *slabel = memnew(Label);
	slabel->set_text(TTR("Scanning Files,\nPlease Wait..."));
	slabel->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	scanning_vb->add_child(slabel);

	scanning_progress = memnew(ProgressBar);
	scanning_vb->add_child(scanning_progress);

	deps_editor = memnew(DependencyEditor);
	add_child(deps_editor);

	owners_editor = memnew(DependencyEditorOwners());
	add_child(owners_editor);

	remove_dialog = memnew(DependencyRemoveDialog);
	remove_dialog->connect("resource_removed", callable_mp(this, &FileSystemDock::_resource_removed));
	remove_dialog->connect("file_removed", callable_mp(this, &FileSystemDock::_file_removed));
	remove_dialog->connect("folder_removed", callable_mp(this, &FileSystemDock::_folder_removed));
	add_child(remove_dialog);

	move_dialog = memnew(EditorDirDialog);
	add_child(move_dialog);
	move_dialog->connect("move_pressed", callable_mp(this, &FileSystemDock::_move_operation_confirm).bind(false, OVERWRITE_UNDECIDED));
	move_dialog->connect("copy_pressed", callable_mp(this, &FileSystemDock::_move_operation_confirm).bind(true, OVERWRITE_UNDECIDED));

	overwrite_dialog = memnew(ConfirmationDialog);
	add_child(overwrite_dialog);
	overwrite_dialog->set_ok_button_text(TTR("Overwrite"));
	overwrite_dialog->add_button(TTR("Keep Both"), true)->connect(SceneStringName(pressed), callable_mp(this, &FileSystemDock::_overwrite_dialog_action).bind(false));
	overwrite_dialog->connect(SceneStringName(confirmed), callable_mp(this, &FileSystemDock::_overwrite_dialog_action).bind(true));

	VBoxContainer *overwrite_dialog_vb = memnew(VBoxContainer);
	overwrite_dialog->add_child(overwrite_dialog_vb);

	overwrite_dialog_header = memnew(Label);
	overwrite_dialog_vb->add_child(overwrite_dialog_header);

	overwrite_dialog_scroll = memnew(ScrollContainer);
	overwrite_dialog_vb->add_child(overwrite_dialog_scroll);
	overwrite_dialog_scroll->set_custom_minimum_size(Vector2(400, 600) * EDSCALE);

	overwrite_dialog_file_list = memnew(Label);
	overwrite_dialog_scroll->add_child(overwrite_dialog_file_list);

	overwrite_dialog_footer = memnew(Label);
	overwrite_dialog_vb->add_child(overwrite_dialog_footer);

	make_dir_dialog = memnew(DirectoryCreateDialog);
	add_child(make_dir_dialog);

	make_scene_dialog = memnew(SceneCreateDialog);
	add_child(make_scene_dialog);
	make_scene_dialog->connect(SceneStringName(confirmed), callable_mp(this, &FileSystemDock::_make_scene_confirm));

	make_script_dialog = memnew(ScriptCreateDialog);
	make_script_dialog->set_title(TTR("Create Script"));
	add_child(make_script_dialog);

	make_shader_dialog = memnew(ShaderCreateDialog);
	add_child(make_shader_dialog);

	new_resource_dialog = memnew(CreateDialog);
	add_child(new_resource_dialog);
	new_resource_dialog->set_base_type("Resource");
	new_resource_dialog->connect("create", callable_mp(this, &FileSystemDock::_resource_created));

	conversion_dialog = memnew(ConfirmationDialog);
	add_child(conversion_dialog);
	conversion_dialog->set_ok_button_text(TTR("Convert"));
	conversion_dialog->connect(SceneStringName(confirmed), callable_mp(this, &FileSystemDock::_convert_dialog_action));

	uncollapsed_paths_before_search = Vector<String>();

	tree_update_id = 0;

	history_pos = 0;
	history_max_size = 20;
	history.push_back("res://");

	display_mode = DISPLAY_MODE_TREE_ONLY;
	old_display_mode = DISPLAY_MODE_TREE_ONLY;
	file_list_display_mode = FILE_LIST_DISPLAY_THUMBNAILS;

	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &FileSystemDock::_project_settings_changed));
	add_resource_tooltip_plugin(memnew(EditorTextureTooltipPlugin));
	add_resource_tooltip_plugin(memnew(EditorAudioStreamTooltipPlugin));
}

FileSystemDock::~FileSystemDock() {
	singleton = nullptr;
}
