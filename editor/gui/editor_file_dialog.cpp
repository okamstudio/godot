/**************************************************************************/
/*  editor_file_dialog.cpp                                                */
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

#include "editor_file_dialog.h"

#include "core/config/project_settings.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "editor/dependency_editor.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/filesystem_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/center_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/gui/texture_rect.h"
#include "servers/display_server.h"

EditorFileDialog::GetIconFunc EditorFileDialog::get_icon_func = nullptr;
EditorFileDialog::GetIconFunc EditorFileDialog::get_thumbnail_func = nullptr;

EditorFileDialog::RegisterFunc EditorFileDialog::register_func = nullptr;
EditorFileDialog::RegisterFunc EditorFileDialog::unregister_func = nullptr;

void EditorFileDialog::_native_popup() {
	// Show native dialog directly.
	String root;
	if (access == ACCESS_RESOURCES) {
		root = ProjectSettings::get_singleton()->get_resource_path();
	} else if (access == ACCESS_USERDATA) {
		root = OS::get_singleton()->get_user_data_dir();
	}
	DisplayServer::get_singleton()->file_dialog_with_options_show(get_translated_title(), ProjectSettings::get_singleton()->globalize_path(dir->get_text()), root, file->get_text().get_file(), show_hidden_files, DisplayServer::FileDialogMode(mode), processed_filters, _get_options(), callable_mp(this, &EditorFileDialog::_native_dialog_cb));
}

void EditorFileDialog::popup(const Rect2i &p_rect) {
	_update_option_controls();

	bool use_native = DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_DIALOG_FILE) && (bool(EDITOR_GET("interface/editor/use_native_file_dialogs")) || OS::get_singleton()->is_sandboxed());
	if (!side_vbox && use_native) {
		_native_popup();
	} else {
		// Show custom file dialog (full dialog or side menu only).
		_update_side_menu_visibility(use_native);
		ConfirmationDialog::popup(p_rect);
	}
}

void EditorFileDialog::set_visible(bool p_visible) {
	if (p_visible) {
		bool use_native = DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_DIALOG_FILE) && (bool(EDITOR_GET("interface/editor/use_native_file_dialogs")) || OS::get_singleton()->is_sandboxed());
		_update_option_controls();
		if (!side_vbox && use_native) {
			_native_popup();
		} else {
			// Show custom file dialog (full dialog or side menu only).
			_update_side_menu_visibility(use_native);
			ConfirmationDialog::set_visible(p_visible);
		}
	} else {
		ConfirmationDialog::set_visible(p_visible);
	}
}

void EditorFileDialog::_native_dialog_cb(bool p_ok, const Vector<String> &p_files, int p_filter, const Dictionary &p_selected_options) {
	if (!p_ok) {
		file->set_text("");
		emit_signal(SNAME("canceled"));
		return;
	}

	if (p_files.is_empty()) {
		return;
	}

	Vector<String> files = p_files;
	if (access != ACCESS_FILESYSTEM) {
		for (String &file_name : files) {
			file_name = ProjectSettings::get_singleton()->localize_path(file_name);
		}
	}
	selected_options = p_selected_options;

	String f = files[0];

	filter->select(p_filter);
	dir->set_text(f.get_base_dir());
	file->set_text(f.get_file());
	_dir_submitted(f.get_base_dir());

	if (mode == FILE_MODE_OPEN_FILES) {
		emit_signal(SNAME("files_selected"), files);
	} else {
		if (mode == FILE_MODE_SAVE_FILE) {
			if (p_filter != 0 && p_filter != filter->get_item_count() - 1) {
				bool valid = false;
				int idx = p_filter;
				if (filters.size() > 1) {
					idx--;
				}
				if (idx >= 0 && idx < filters.size()) {
					String flt = filters[idx].get_slice(";", 0);
					int filter_slice_count = flt.get_slice_count(",");
					for (int j = 0; j < filter_slice_count; j++) {
						String str = (flt.get_slice(",", j).strip_edges());
						if (f.match(str)) {
							valid = true;
							break;
						}
					}

					if (!valid && filter_slice_count > 0) {
						String str = (flt.get_slice(",", 0).strip_edges());
						f += str.substr(1, str.length() - 1);
					}
				}
			}
			emit_signal(SNAME("file_selected"), f);
		} else if ((mode == FILE_MODE_OPEN_ANY || mode == FILE_MODE_OPEN_FILE) && dir_access->file_exists(f)) {
			emit_signal(SNAME("file_selected"), f);
		} else if (mode == FILE_MODE_OPEN_ANY || mode == FILE_MODE_OPEN_DIR) {
			emit_signal(SNAME("dir_selected"), f);
		}
	}
}

void EditorFileDialog::popup_file_dialog() {
	popup_centered_clamped(Size2(1050, 700) * EDSCALE, 0.8);
	_focus_file_text();
}

void EditorFileDialog::_focus_file_text() {
	int lp = file->get_text().rfind_char('.');
	if (lp != -1) {
		file->select(0, lp);
		file->grab_focus();
	}
}

VBoxContainer *EditorFileDialog::get_vbox() {
	return vbox;
}

void EditorFileDialog::_update_theme_item_cache() {
	ConfirmationDialog::_update_theme_item_cache();

	theme_cache.parent_folder = get_editor_theme_icon(SNAME("ArrowUp"));
	theme_cache.forward_folder = get_editor_theme_icon(SNAME("Forward"));
	theme_cache.back_folder = get_editor_theme_icon(SNAME("Back"));
	theme_cache.reload = get_editor_theme_icon(SNAME("Reload"));
	theme_cache.toggle_hidden = get_editor_theme_icon(SNAME("GuiVisibilityVisible"));
	theme_cache.toggle_filename_filter = get_editor_theme_icon(SNAME("FilenameFilter"));
	theme_cache.favorite = get_editor_theme_icon(SNAME("Favorites"));
	theme_cache.mode_thumbnails = get_editor_theme_icon(SNAME("FileThumbnail"));
	theme_cache.mode_list = get_editor_theme_icon(SNAME("FileList"));
	theme_cache.favorites_up = get_editor_theme_icon(SNAME("MoveUp"));
	theme_cache.favorites_down = get_editor_theme_icon(SNAME("MoveDown"));
	theme_cache.create_folder = get_editor_theme_icon(SNAME("FolderCreate"));
	theme_cache.open_folder = get_editor_theme_icon(SNAME("FolderBrowse"));

	theme_cache.filter_box = get_editor_theme_icon(SNAME("Search"));
	theme_cache.file_sort_button = get_editor_theme_icon(SNAME("Sort"));

	theme_cache.folder = get_editor_theme_icon(SNAME("Folder"));
	theme_cache.folder_icon_color = get_theme_color(SNAME("folder_icon_color"), SNAME("FileDialog"));

	theme_cache.action_copy = get_editor_theme_icon(SNAME("ActionCopy"));
	theme_cache.action_delete = get_editor_theme_icon(SNAME("Remove"));
	theme_cache.filesystem = get_editor_theme_icon(SNAME("Filesystem"));

	theme_cache.folder_medium_thumbnail = get_editor_theme_icon(SNAME("FolderMediumThumb"));
	theme_cache.file_medium_thumbnail = get_editor_theme_icon(SNAME("FileMediumThumb"));
	theme_cache.folder_big_thumbnail = get_editor_theme_icon(SNAME("FolderBigThumb"));
	theme_cache.file_big_thumbnail = get_editor_theme_icon(SNAME("FileBigThumb"));

	theme_cache.progress[0] = get_editor_theme_icon("Progress1");
	theme_cache.progress[1] = get_editor_theme_icon("Progress2");
	theme_cache.progress[2] = get_editor_theme_icon("Progress3");
	theme_cache.progress[3] = get_editor_theme_icon("Progress4");
	theme_cache.progress[4] = get_editor_theme_icon("Progress5");
	theme_cache.progress[5] = get_editor_theme_icon("Progress6");
	theme_cache.progress[6] = get_editor_theme_icon("Progress7");
	theme_cache.progress[7] = get_editor_theme_icon("Progress8");
}

void EditorFileDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			set_translation_domain(SNAME("godot.editor"));
		} break;

		case NOTIFICATION_THEME_CHANGED:
		case Control::NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			_update_icons();
			invalidate();
		} break;

		case NOTIFICATION_PROCESS: {
			if (preview_waiting) {
				preview_wheel_timeout -= get_process_delta_time();
				if (preview_wheel_timeout <= 0) {
					preview_wheel_index++;
					if (preview_wheel_index >= 8) {
						preview_wheel_index = 0;
					}

					Ref<Texture2D> frame = theme_cache.progress[preview_wheel_index];
					preview->set_texture(frame);
					preview_wheel_timeout = 0.1;
				}
			}
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("filesystem/file_dialog")) {
				break;
			}
			bool is_showing_hidden = EDITOR_GET("filesystem/file_dialog/show_hidden_files");
			if (show_hidden_files != is_showing_hidden) {
				set_show_hidden_files(is_showing_hidden);
			}
			set_display_mode((DisplayMode)EDITOR_GET("filesystem/file_dialog/display_mode").operator int());

			// DO NOT CALL UPDATE FILE LIST HERE, ALL HUNDREDS OF HIDDEN DIALOGS WILL RESPOND, CALL INVALIDATE INSTEAD
			invalidate();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				set_process_shortcut_input(false);
			}

			invalidate(); // For consistency with the standard FileDialog.
		} break;

		case NOTIFICATION_WM_WINDOW_FOCUS_IN: {
			// Check if the current directory was removed externally (much less likely to happen while editor window is focused).
			String previous_dir = get_current_dir();
			while (!dir_access->dir_exists(get_current_dir())) {
				_go_up();

				// In case we can't go further up, use some fallback and break.
				if (get_current_dir() == previous_dir) {
					_dir_submitted(OS::get_singleton()->get_user_data_dir());
					break;
				}
			}
		} break;
	}
}

void EditorFileDialog::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (k->is_pressed()) {
			bool handled = false;

			if (ED_IS_SHORTCUT("file_dialog/go_back", p_event)) {
				_go_back();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/go_forward", p_event)) {
				_go_forward();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/go_up", p_event)) {
				_go_up();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/refresh", p_event)) {
				invalidate();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/toggle_hidden_files", p_event)) {
				set_show_hidden_files(!show_hidden_files);
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/toggle_favorite", p_event)) {
				_favorite_pressed();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/toggle_mode", p_event)) {
				if (mode_thumbnails->is_pressed()) {
					set_display_mode(DISPLAY_LIST);
				} else {
					set_display_mode(DISPLAY_THUMBNAILS);
				}
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/create_folder", p_event)) {
				_make_dir();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/delete", p_event)) {
				_delete_items();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/focus_path", p_event)) {
				dir->grab_focus();
				dir->select_all();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/focus_filter", p_event)) {
				show_search_filter_button->set_pressed(!show_search_filter_button->is_pressed());
				_focus_filter_box();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/move_favorite_up", p_event)) {
				_favorite_move_up();
				handled = true;
			}
			if (ED_IS_SHORTCUT("file_dialog/move_favorite_down", p_event)) {
				_favorite_move_down();
				handled = true;
			}

			if (handled) {
				set_input_as_handled();
			}
		}
	}
}

void EditorFileDialog::set_enable_multiple_selection(bool p_enable) {
	item_list->set_select_mode(p_enable ? ItemList::SELECT_MULTI : ItemList::SELECT_SINGLE);
}

Vector<String> EditorFileDialog::get_selected_files() const {
	Vector<String> list;
	for (int i = 0; i < item_list->get_item_count(); i++) {
		if (item_list->is_selected(i)) {
			list.push_back(item_list->get_item_text(i));
		}
	}
	return list;
}

void EditorFileDialog::update_dir() {
	full_dir = dir_access->get_current_dir();
	if (drives->is_visible()) {
		if (dir_access->get_current_dir().is_network_share_path()) {
			_update_drives(false);
			drives->add_item(RTR("Network"));
			drives->set_item_disabled(-1, true);
			drives->select(drives->get_item_count() - 1);
		} else {
			drives->select(dir_access->get_current_drive());
		}
	}
	dir->set_text(dir_access->get_current_dir(false));

	filter_box->clear();

	// Disable "Open" button only when selecting file(s) mode.
	get_ok_button()->set_disabled(_is_open_should_be_disabled());
	switch (mode) {
		case FILE_MODE_OPEN_FILE:
		case FILE_MODE_OPEN_FILES:
			file->set_text("");
			set_ok_button_text(TTR("Open"));
			break;
		case FILE_MODE_OPEN_ANY:
		case FILE_MODE_OPEN_DIR:
			file->set_text("");
			set_ok_button_text(TTR("Select Current Folder"));
			break;
		case FILE_MODE_SAVE_FILE:
			// FIXME: Implement, or refactor to avoid duplication with set_mode
			break;
	}
}

void EditorFileDialog::_dir_submitted(const String &p_dir) {
	String new_dir = p_dir;
#ifdef WINDOWS_ENABLED
	if (drives->is_visible() && !new_dir.is_network_share_path() && new_dir.is_absolute_path() && new_dir.find(":/") == -1 && new_dir.find(":\\") == -1) {
		// Non network path without X:/ prefix on Windows, add drive letter.
		new_dir = drives->get_item_text(drives->get_selected()).path_join(new_dir);
	}
#endif
	dir_access->change_dir(new_dir);
	invalidate();
	update_dir();
	_push_history();
}

void EditorFileDialog::_file_submitted(const String &p_file) {
	_action_pressed();
}

void EditorFileDialog::_save_confirm_pressed() {
	String f = dir_access->get_current_dir().path_join(file->get_text());
	_save_to_recent();
	hide();
	emit_signal(SNAME("file_selected"), f);
}

void EditorFileDialog::_post_popup() {
	ConfirmationDialog::_post_popup();

	// Check if the current path doesn't exist and correct it.
	String current = dir_access->get_current_dir();
	while (!dir_access->dir_exists(current)) {
		current = current.get_base_dir();
	}
	set_current_dir(current);

	if (mode == FILE_MODE_SAVE_FILE) {
		file->grab_focus();
	} else {
		item_list->grab_focus();
	}

	bool is_open_directory_mode = mode == FILE_MODE_OPEN_DIR;
	PopupMenu *p = file_sort_button->get_popup();
	p->set_item_disabled(2, is_open_directory_mode);
	p->set_item_disabled(3, is_open_directory_mode);
	p->set_item_disabled(4, is_open_directory_mode);
	p->set_item_disabled(5, is_open_directory_mode);
	if (is_open_directory_mode) {
		file_box->set_visible(false);
	} else {
		file_box->set_visible(true);
	}

	if (!get_current_file().is_empty()) {
		_request_single_thumbnail(get_current_dir().path_join(get_current_file()));
	}

	local_history.clear();
	local_history_pos = -1;
	_push_history();

	set_process_shortcut_input(true);
}

void EditorFileDialog::_thumbnail_result(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata) {
	if (display_mode == DISPLAY_LIST || p_preview.is_null()) {
		return;
	}

	for (int i = 0; i < item_list->get_item_count(); i++) {
		Dictionary d = item_list->get_item_metadata(i);
		String pname = d["path"];
		if (pname == p_path) {
			item_list->set_item_icon(i, p_preview);
			item_list->set_item_tag_icon(i, Ref<Texture2D>());
		}
	}
}

void EditorFileDialog::_thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata) {
	set_process(false);
	preview_waiting = false;

	if (p_preview.is_valid() && get_current_path() == p_path) {
		preview->set_texture(p_preview);
		if (display_mode == DISPLAY_THUMBNAILS) {
			preview_vb->hide();
		} else {
			preview_vb->show();
		}

	} else {
		preview_vb->hide();
		preview->set_texture(Ref<Texture2D>());
	}
}

void EditorFileDialog::_request_single_thumbnail(const String &p_path) {
	if (!FileAccess::exists(p_path) || !previews_enabled) {
		return;
	}

	set_process(true);
	preview_waiting = true;
	preview_wheel_timeout = 0;
	EditorResourcePreview::get_singleton()->queue_resource_preview(p_path, this, "_thumbnail_done", p_path);
}

void EditorFileDialog::_action_pressed() {
	// Accept side menu properties and show native dialog.
	if (side_vbox && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_DIALOG_FILE) && (bool(EDITOR_GET("interface/editor/use_native_file_dialogs")) || OS::get_singleton()->is_sandboxed())) {
		hide();
		_native_popup();

		return;
	}

	// Accept selection in the custom dialog.
	if (mode == FILE_MODE_OPEN_FILES) {
		String fbase = dir_access->get_current_dir();

		Vector<String> files;
		for (int i = 0; i < item_list->get_item_count(); i++) {
			if (item_list->is_selected(i)) {
				files.push_back(fbase.path_join(item_list->get_item_text(i)));
			}
		}

		if (files.size()) {
			_save_to_recent();
			hide();
			emit_signal(SNAME("files_selected"), files);
		}

		return;
	}

	String file_text = file->get_text();
	String f = file_text.is_absolute_path() ? file_text : dir_access->get_current_dir().path_join(file_text);

	if ((mode == FILE_MODE_OPEN_ANY || mode == FILE_MODE_OPEN_FILE) && (dir_access->file_exists(f) || dir_access->is_bundle(f))) {
		_save_to_recent();
		hide();
		emit_signal(SNAME("file_selected"), f);
	} else if (mode == FILE_MODE_OPEN_ANY || mode == FILE_MODE_OPEN_DIR) {
		String path = dir_access->get_current_dir();

		path = path.replace("\\", "/");

		for (int i = 0; i < item_list->get_item_count(); i++) {
			if (item_list->is_selected(i)) {
				Dictionary d = item_list->get_item_metadata(i);
				if (d["dir"]) {
					path = path.path_join(d["name"]);

					break;
				}
			}
		}

		_save_to_recent();
		hide();
		emit_signal(SNAME("dir_selected"), path);
	}

	if (mode == FILE_MODE_SAVE_FILE) {
		bool valid = false;

		if (filter->get_selected() == filter->get_item_count() - 1) {
			valid = true; // match none
		} else if (filters.size() > 1 && filter->get_selected() == 0) {
			// match all filters
			for (int i = 0; i < filters.size(); i++) {
				String flt = filters[i].get_slice(";", 0);
				for (int j = 0; j < flt.get_slice_count(","); j++) {
					String str = flt.get_slice(",", j).strip_edges();
					if (f.matchn(str)) {
						valid = true;
						break;
					}
				}
				if (valid) {
					break;
				}
			}
		} else {
			int idx = filter->get_selected();
			if (filters.size() > 1) {
				idx--;
			}
			if (idx >= 0 && idx < filters.size()) {
				String flt = filters[idx].get_slice(";", 0);
				int filter_slice_count = flt.get_slice_count(",");
				for (int j = 0; j < filter_slice_count; j++) {
					String str = (flt.get_slice(",", j).strip_edges());
					if (f.matchn(str)) {
						valid = true;
						break;
					}
				}

				if (!valid && filter_slice_count > 0) {
					String str = (flt.get_slice(",", 0).strip_edges());
					f += str.substr(1, str.length() - 1);
					_request_single_thumbnail(get_current_dir().path_join(f.get_file()));
					file->set_text(f.get_file());
					valid = true;
				}
			} else {
				valid = true;
			}
		}

		// First check we're not having an empty name.
		String file_name = file_text.strip_edges().get_file();
		if (file_name.is_empty()) {
			error_dialog->set_text(TTR("Cannot save file with an empty filename."));
			error_dialog->popup_centered(Size2(250, 80) * EDSCALE);
			return;
		}

		// Add first extension of filter if no valid extension is found.
		if (!valid) {
			int idx = filter->get_selected();
			String flt = filters[idx].get_slice(";", 0);
			String ext = flt.get_slice(",", 0).strip_edges().get_extension();
			f += "." + ext;
		}

		if (file_name.begins_with(".")) { // Could still happen if typed manually.
			error_dialog->set_text(TTR("Cannot save file with a name starting with a dot."));
			error_dialog->popup_centered(Size2(250, 80) * EDSCALE);
			return;
		}

		if (dir_access->file_exists(f) && !disable_overwrite_warning) {
			confirm_save->set_text(vformat(TTR("File \"%s\" already exists.\nDo you want to overwrite it?"), f));
			confirm_save->popup_centered(Size2(250, 80) * EDSCALE);
		} else {
			_save_to_recent();
			hide();
			emit_signal(SNAME("file_selected"), f);
		}
	}
}

void EditorFileDialog::_cancel_pressed() {
	file->set_text("");
	invalidate();
	hide();
}

void EditorFileDialog::_item_selected(int p_item) {
	int current = p_item;
	if (current < 0 || current >= item_list->get_item_count()) {
		return;
	}

	Dictionary d = item_list->get_item_metadata(current);

	if (!d["dir"]) {
		file->set_text(d["name"]);
		_request_single_thumbnail(get_current_dir().path_join(get_current_file()));

		if (mode != FILE_MODE_SAVE_FILE) {
			// FILE_MODE_OPEN_ANY can alternate this text depending on what's selected.
			set_ok_button_text(TTR("Open"));
		}
	} else if (mode == FILE_MODE_OPEN_DIR || mode == FILE_MODE_OPEN_ANY) {
		file->set_text("");
		set_ok_button_text(TTR("Select This Folder"));
	}

	get_ok_button()->set_disabled(_is_open_should_be_disabled());
}

void EditorFileDialog::_multi_selected(int p_item, bool p_selected) {
	int current = p_item;
	if (current < 0 || current >= item_list->get_item_count()) {
		return;
	}

	Dictionary d = item_list->get_item_metadata(current);

	if (!d["dir"] && p_selected) {
		file->set_text(d["name"]);
		_request_single_thumbnail(get_current_dir().path_join(get_current_file()));
	}

	get_ok_button()->set_disabled(_is_open_should_be_disabled());
}

void EditorFileDialog::_items_clear_selection(const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index != MouseButton::LEFT) {
		return;
	}

	item_list->deselect_all();

	// If nothing is selected, then block Open button.
	switch (mode) {
		case FILE_MODE_OPEN_FILE:
		case FILE_MODE_OPEN_FILES:
			set_ok_button_text(TTR("Open"));
			get_ok_button()->set_disabled(!item_list->is_anything_selected());
			break;

		case FILE_MODE_OPEN_ANY:
		case FILE_MODE_OPEN_DIR:
			file->set_text("");
			get_ok_button()->set_disabled(false);
			set_ok_button_text(TTR("Select Current Folder"));
			break;

		case FILE_MODE_SAVE_FILE:
			// FIXME: Implement, or refactor to avoid duplication with set_mode
			break;
	}
}

void EditorFileDialog::_push_history() {
	local_history.resize(local_history_pos + 1);
	String new_path = dir_access->get_current_dir();
	if (local_history.size() == 0 || new_path != local_history[local_history_pos]) {
		local_history.push_back(new_path);
		local_history_pos++;
		dir_prev->set_disabled(local_history_pos == 0);
		dir_next->set_disabled(true);
	}
}

void EditorFileDialog::_item_dc_selected(int p_item) {
	int current = p_item;
	if (current < 0 || current >= item_list->get_item_count()) {
		return;
	}

	Dictionary d = item_list->get_item_metadata(current);

	if (d["dir"]) {
		dir_access->change_dir(d["name"]);
		callable_mp(this, &EditorFileDialog::update_file_list).call_deferred();
		callable_mp(this, &EditorFileDialog::update_dir).call_deferred();

		_push_history();

	} else {
		_action_pressed();
	}
}

void EditorFileDialog::_item_list_item_rmb_clicked(int p_item, const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index != MouseButton::RIGHT) {
		return;
	}

	// Right click on specific file(s) or folder(s).
	item_menu->clear();
	item_menu->reset_size();

	// Allow specific actions only on one item.
	bool single_item_selected = item_list->get_selected_items().size() == 1;

	// Disallow deleting the .import folder, Godot kills a cat if you do and it is possibly a senseless novice action.
	bool allow_delete = true;
	for (int i = 0; i < item_list->get_item_count(); i++) {
		if (!item_list->is_selected(i)) {
			continue;
		}
		Dictionary item_meta = item_list->get_item_metadata(i);
		if (String(item_meta["path"]).begins_with(ProjectSettings::get_singleton()->get_project_data_path())) {
			allow_delete = false;
			break;
		}
	}

	if (single_item_selected) {
		item_menu->add_icon_item(theme_cache.action_copy, TTR("Copy Path"), ITEM_MENU_COPY_PATH);
	}
	if (allow_delete) {
		item_menu->add_icon_item(theme_cache.action_delete, TTR("Delete"), ITEM_MENU_DELETE, Key::KEY_DELETE);
	}

#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	// Opening the system file manager is not supported on the Android and web editors.
	if (single_item_selected) {
		item_menu->add_separator();
		Dictionary item_meta = item_list->get_item_metadata(p_item);
		String item_text = item_meta["dir"] ? TTR("Open in File Manager") : TTR("Show in File Manager");
		item_menu->add_icon_item(theme_cache.filesystem, item_text, ITEM_MENU_SHOW_IN_EXPLORER);
	}
#endif
	if (single_item_selected) {
		Dictionary item_meta = item_list->get_item_metadata(p_item);
		if (item_meta["bundle"]) {
			item_menu->add_icon_item(theme_cache.open_folder, TTR("Show Package Contents"), ITEM_MENU_SHOW_BUNDLE_CONTENT);
		}
	}

	if (item_menu->get_item_count() > 0) {
		item_menu->set_position(item_list->get_screen_position() + p_pos);
		item_menu->reset_size();
		item_menu->popup();
	}
}

void EditorFileDialog::_item_list_empty_clicked(const Vector2 &p_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index != MouseButton::RIGHT && p_mouse_button_index != MouseButton::LEFT) {
		return;
	}

	// Left or right click on folder background. Deselect all files so that actions are applied on the current folder.
	for (int i = 0; i < item_list->get_item_count(); i++) {
		item_list->deselect(i);
	}

	if (p_mouse_button_index != MouseButton::RIGHT) {
		return;
	}

	item_menu->clear();
	item_menu->reset_size();

	if (can_create_dir) {
		item_menu->add_icon_item(theme_cache.folder, TTR("New Folder..."), ITEM_MENU_NEW_FOLDER, KeyModifierMask::CMD_OR_CTRL | Key::N);
	}
	item_menu->add_icon_item(theme_cache.reload, TTR("Refresh"), ITEM_MENU_REFRESH, Key::F5);
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	// Opening the system file manager is not supported on the Android and web editors.
	item_menu->add_separator();
	item_menu->add_icon_item(theme_cache.filesystem, TTR("Open in File Manager"), ITEM_MENU_SHOW_IN_EXPLORER);
#endif

	item_menu->set_position(item_list->get_screen_position() + p_pos);
	item_menu->reset_size();
	item_menu->popup();
}

void EditorFileDialog::_item_menu_id_pressed(int p_option) {
	switch (p_option) {
		case ITEM_MENU_COPY_PATH: {
			Dictionary item_meta = item_list->get_item_metadata(item_list->get_current());
			DisplayServer::get_singleton()->clipboard_set(item_meta["path"]);
		} break;

		case ITEM_MENU_DELETE: {
			_delete_items();
		} break;

		case ITEM_MENU_REFRESH: {
			invalidate();
		} break;

		case ITEM_MENU_NEW_FOLDER: {
			_make_dir();
		} break;

		case ITEM_MENU_SHOW_IN_EXPLORER: {
			String path;
			int idx = item_list->get_current();
			if (idx == -1 || !item_list->is_anything_selected()) {
				// Folder background was clicked. Open this folder.
				path = ProjectSettings::get_singleton()->globalize_path(dir_access->get_current_dir());
			} else {
				// Specific item was clicked. Open folders directly, or the folder containing a selected file.
				Dictionary item_meta = item_list->get_item_metadata(idx);
				path = ProjectSettings::get_singleton()->globalize_path(item_meta["path"]);
			}
			OS::get_singleton()->shell_show_in_file_manager(path, true);
		} break;

		case ITEM_MENU_SHOW_BUNDLE_CONTENT: {
			String path;
			int idx = item_list->get_current();
			if (idx == -1 || !item_list->is_anything_selected()) {
				return;
			}
			Dictionary item_meta = item_list->get_item_metadata(idx);
			dir_access->change_dir(item_meta["path"]);
			callable_mp(this, &EditorFileDialog::update_file_list).call_deferred();
			callable_mp(this, &EditorFileDialog::update_dir).call_deferred();

			_push_history();
		} break;
	}
}

bool EditorFileDialog::_is_open_should_be_disabled() {
	if (mode == FILE_MODE_OPEN_ANY || mode == FILE_MODE_SAVE_FILE) {
		return false;
	}

	Vector<int> items = item_list->get_selected_items();
	if (items.size() == 0) {
		return mode != FILE_MODE_OPEN_DIR; // In "Open folder" mode, having nothing selected picks the current folder.
	}

	for (int i = 0; i < items.size(); i++) {
		Dictionary d = item_list->get_item_metadata(items.get(i));

		if (((mode == FILE_MODE_OPEN_FILE || mode == FILE_MODE_OPEN_FILES) && d["dir"]) || (mode == FILE_MODE_OPEN_DIR && !d["dir"])) {
			return true;
		}
	}

	return false;
}

void EditorFileDialog::update_file_name() {
	int idx = filter->get_selected() - 1;
	if ((idx == -1 && filter->get_item_count() == 2) || (filter->get_item_count() > 2 && idx >= 0 && idx < filter->get_item_count() - 2)) {
		if (idx == -1) {
			idx += 1;
		}
		String filter_str = filters[idx];
		String file_str = file->get_text();
		String base_name = file_str.get_basename();
		Vector<String> filter_substr = filter_str.split(";");
		if (filter_substr.size() >= 2) {
			file_str = base_name + "." + filter_substr[0].strip_edges().get_extension().to_lower();
		} else {
			file_str = base_name + "." + filter_str.strip_edges().get_extension().to_lower();
		}
		file->set_text(file_str);
	}
}

// TODO: Could use a unit test.
Color EditorFileDialog::get_dir_icon_color(const String &p_dir_path) {
	if (!FileSystemDock::get_singleton()) { // This dialog can be called from the project manager.
		return theme_cache.folder_icon_color;
	}

	const HashMap<String, Color> &folder_colors = FileSystemDock::get_singleton()->get_folder_colors();
	Dictionary assigned_folder_colors = FileSystemDock::get_singleton()->get_assigned_folder_colors();

	Color folder_icon_color = theme_cache.folder_icon_color;

	// Check for a folder color to inherit (if one is assigned).
	String parent_dir = ProjectSettings::get_singleton()->localize_path(p_dir_path);
	while (!parent_dir.is_empty() && parent_dir != "res://") {
		if (!parent_dir.ends_with("/")) {
			parent_dir += "/";
		}
		if (assigned_folder_colors.has(parent_dir)) {
			folder_icon_color = folder_colors[assigned_folder_colors[parent_dir]];
			if (folder_icon_color != theme_cache.folder_icon_color) {
				break;
			}
		}
		parent_dir = parent_dir.trim_suffix("/").get_base_dir();
	}

	return folder_icon_color;
}

// DO NOT USE THIS FUNCTION UNLESS NEEDED, CALL INVALIDATE() INSTEAD.
void EditorFileDialog::update_file_list() {
	int thumbnail_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
	thumbnail_size *= EDSCALE;
	Ref<Texture2D> folder_thumbnail;
	Ref<Texture2D> file_thumbnail;

	item_list->clear();

	// Scroll back to the top after opening a directory
	item_list->get_v_scroll_bar()->set_value(0);

	if (display_mode == DISPLAY_THUMBNAILS) {
		item_list->set_max_columns(0);
		item_list->set_icon_mode(ItemList::ICON_MODE_TOP);
		item_list->set_fixed_column_width(thumbnail_size * 3 / 2);
		item_list->set_max_text_lines(2);
		item_list->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		item_list->set_fixed_icon_size(Size2(thumbnail_size, thumbnail_size));

		if (thumbnail_size < 64) {
			folder_thumbnail = theme_cache.folder_medium_thumbnail;
			file_thumbnail = theme_cache.file_medium_thumbnail;
		} else {
			folder_thumbnail = theme_cache.folder_big_thumbnail;
			file_thumbnail = theme_cache.file_big_thumbnail;
		}

		preview_vb->hide();

	} else {
		item_list->set_icon_mode(ItemList::ICON_MODE_LEFT);
		item_list->set_max_columns(1);
		item_list->set_max_text_lines(1);
		item_list->set_fixed_column_width(0);
		item_list->set_fixed_icon_size(Size2());
		if (preview->get_texture().is_valid()) {
			preview_vb->show();
		}
	}

	String cdir = dir_access->get_current_dir();

	dir_access->list_dir_begin();

	List<FileInfo> file_infos;
	List<String> dirs;

	String item = dir_access->get_next();

	while (!item.is_empty()) {
		if (item == "." || item == "..") {
			item = dir_access->get_next();
			continue;
		}

		bool matches_search = true;
		if (search_string.length() > 0) {
			matches_search = item.find(search_string) != -1;
		}

		FileInfo file_info;
		file_info.name = item;
		file_info.path = cdir.path_join(file_info.name);
		file_info.type = item.get_extension();
		file_info.modified_time = FileAccess::get_modified_time(file_info.path);

		if (matches_search) {
			if (show_hidden_files) {
				if (!dir_access->current_is_dir()) {
					file_infos.push_back(file_info);
				} else {
					dirs.push_back(file_info.name);
				}
			} else if (!dir_access->current_is_hidden()) {
				String full_path = cdir == "res://" ? file_info.name : dir_access->get_current_dir() + "/" + file_info.name;
				if (dir_access->current_is_dir()) {
					if (Engine::get_singleton()->is_project_manager_hint() || !EditorFileSystem::_should_skip_directory(full_path)) {
						dirs.push_back(file_info.name);
					}
				} else {
					file_infos.push_back(file_info);
				}
			}
		}
		item = dir_access->get_next();
	}

	dirs.sort_custom<FileNoCaseComparator>();
	bool reverse_directories = file_sort == FileSortOption::FILE_SORT_NAME_REVERSE;
	if (reverse_directories) {
		dirs.reverse();
	}
	sort_file_info_list(file_infos, file_sort);

	List<String> patterns;
	// build filter
	if (filter->get_selected() == filter->get_item_count() - 1) {
		// match all
	} else if (filters.size() > 1 && filter->get_selected() == 0) {
		// match all filters
		for (int i = 0; i < filters.size(); i++) {
			String f = filters[i].get_slice(";", 0);
			for (int j = 0; j < f.get_slice_count(","); j++) {
				patterns.push_back(f.get_slice(",", j).strip_edges());
			}
		}
	} else {
		int idx = filter->get_selected();
		if (filters.size() > 1) {
			idx--;
		}

		if (idx >= 0 && idx < filters.size()) {
			String f = filters[idx].get_slice(";", 0);
			for (int j = 0; j < f.get_slice_count(","); j++) {
				patterns.push_back(f.get_slice(",", j).strip_edges());
			}
		}
	}

	while (!dirs.is_empty()) {
		const String &dir_name = dirs.front()->get();

		bool bundle = dir_access->is_bundle(dir_name);
		bool found = true;
		if (bundle) {
			bool match = patterns.is_empty();
			for (const String &E : patterns) {
				if (dir_name.matchn(E)) {
					match = true;
					break;
				}
			}
			found = match;
		}

		if (found) {
			item_list->add_item(dir_name);

			if (display_mode == DISPLAY_THUMBNAILS) {
				item_list->set_item_icon(-1, folder_thumbnail);
			} else {
				item_list->set_item_icon(-1, theme_cache.folder);
			}

			Dictionary d;
			d["name"] = dir_name;
			d["path"] = cdir.path_join(dir_name);
			d["dir"] = !bundle;
			d["bundle"] = bundle;

			item_list->set_item_metadata(-1, d);
			item_list->set_item_icon_modulate(-1, get_dir_icon_color(String(d["path"])));
		}

		dirs.pop_front();
	}

	while (!file_infos.is_empty()) {
		bool match = patterns.is_empty();

		FileInfo file_info = file_infos.front()->get();
		for (const String &E : patterns) {
			if (file_info.name.matchn(E)) {
				match = true;
				break;
			}
		}

		if (match) {
			item_list->add_item(file_info.name);

			if (get_icon_func) {
				Ref<Texture2D> icon = get_icon_func(file_info.path);
				if (display_mode == DISPLAY_THUMBNAILS) {
					Ref<Texture2D> thumbnail;
					if (get_thumbnail_func) {
						thumbnail = get_thumbnail_func(file_info.path);
					}
					if (thumbnail.is_null()) {
						thumbnail = file_thumbnail;
					}

					item_list->set_item_icon(-1, thumbnail);
					item_list->set_item_tag_icon(-1, icon);
				} else {
					item_list->set_item_icon(-1, icon);
				}
			}

			Dictionary d;
			d["name"] = file_info.name;
			d["dir"] = false;
			d["bundle"] = false;
			d["path"] = file_info.path;
			item_list->set_item_metadata(-1, d);

			if (display_mode == DISPLAY_THUMBNAILS && previews_enabled) {
				EditorResourcePreview::get_singleton()->queue_resource_preview(file_info.path, this, "_thumbnail_result", file_info.path);
			}

			if (file->get_text() == file_info.name) {
				item_list->set_current(item_list->get_item_count() - 1);
			}
		}

		file_infos.pop_front();
	}

	if (favorites->get_current() >= 0) {
		favorites->deselect(favorites->get_current());
	}

	favorite->set_pressed(false);
	fav_up->set_disabled(true);
	fav_down->set_disabled(true);
	get_ok_button()->set_disabled(_is_open_should_be_disabled());
	for (int i = 0; i < favorites->get_item_count(); i++) {
		if (favorites->get_item_metadata(i) == cdir || favorites->get_item_metadata(i) == cdir + "/") {
			favorites->select(i);
			favorite->set_pressed(true);
			if (i > 0) {
				fav_up->set_disabled(false);
			}
			if (i < favorites->get_item_count() - 1) {
				fav_down->set_disabled(false);
			}
			break;
		}
	}
}

void EditorFileDialog::_filter_selected(int) {
	update_file_name();
	update_file_list();
}

void EditorFileDialog::_search_filter_selected() {
	Vector<int> items = item_list->get_selected_items();
	if (!items.is_empty()) {
		int index = items[0];
		file->set_text(item_list->get_item_text(index));
		file->emit_signal(SceneStringName(text_submitted), file->get_text());
	}
}

void EditorFileDialog::update_filters() {
	filter->clear();
	processed_filters.clear();

	if (filters.size() > 1) {
		String all_filters;
		String all_mime;
		String all_filters_full;
		String all_mime_full;

		const int max_filters = 5;

		// "All Recognized" display name.
		for (int i = 0; i < MIN(max_filters, filters.size()); i++) {
			String flt = filters[i].get_slicec(';', 0).strip_edges();
			if (!all_filters.is_empty() && !flt.is_empty()) {
				all_filters += ", ";
			}
			all_filters += flt;

			String mime = filters[i].get_slicec(';', 2).strip_edges();
			if (!all_mime.is_empty() && !mime.is_empty()) {
				all_mime += ", ";
			}
			all_mime += mime;
		}

		// "All Recognized" filter.
		for (int i = 0; i < filters.size(); i++) {
			String flt = filters[i].get_slicec(';', 0).strip_edges();
			if (!all_filters_full.is_empty() && !flt.is_empty()) {
				all_filters_full += ",";
			}
			all_filters_full += flt;

			String mime = filters[i].get_slicec(';', 2).strip_edges();
			if (!all_mime_full.is_empty() && !mime.is_empty()) {
				all_mime_full += ",";
			}
			all_mime_full += mime;
		}

		String native_all_name;
		if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_DIALOG_FILE_MIME)) {
			native_all_name += all_filters;
		}
		if (!native_all_name.is_empty()) {
			native_all_name += ", ";
		}
		native_all_name += all_mime;

		if (max_filters < filters.size()) {
			all_filters += ", ...";
			native_all_name += ", ...";
		}

		filter->add_item(atr(ETR("All Recognized")) + " (" + all_filters + ")");
		processed_filters.push_back(all_filters_full + ";" + atr(ETR("All Recognized")) + " (" + native_all_name + ")" + ";" + all_mime_full);
	}
	for (int i = 0; i < filters.size(); i++) {
		String flt = filters[i].get_slicec(';', 0).strip_edges();
		String desc = filters[i].get_slicec(';', 1).strip_edges();
		String mime = filters[i].get_slicec(';', 2).strip_edges();
		String native_name;
		if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_DIALOG_FILE_MIME)) {
			native_name += flt;
		}
		if (!native_name.is_empty() && !mime.is_empty()) {
			native_name += ", ";
		}
		native_name += mime;
		if (!desc.is_empty()) {
			filter->add_item(atr(desc) + " (" + flt + ")");
			processed_filters.push_back(flt + ";" + atr(desc) + " (" + native_name + ");" + mime);
		} else {
			filter->add_item("(" + flt + ")");
			processed_filters.push_back(flt + ";(" + native_name + ");" + mime);
		}
	}

	String f = TTR("All Files") + " (*.*)";
	filter->add_item(f);
	processed_filters.push_back("*.*;" + f + ";application/octet-stream");
}

void EditorFileDialog::clear_filters() {
	filters.clear();
	update_filters();
	invalidate();
}

void EditorFileDialog::clear_search_filter() {
	set_search_filter("");
	update_search_filter_gui();
	invalidate();
}

void EditorFileDialog::update_search_filter_gui() {
	filter_hb->set_visible(show_search_filter);
	if (!show_search_filter) {
		search_string.clear();
	}
	if (filter_box->get_text() == search_string) {
		return;
	}
	filter_box->set_text(search_string);
}

void EditorFileDialog::add_filter(const String &p_filter, const String &p_description) {
	if (p_description.is_empty()) {
		filters.push_back(p_filter);
	} else {
		filters.push_back(vformat("%s ; %s", p_filter, p_description));
	}
	update_filters();
	invalidate();
}

void EditorFileDialog::set_filters(const Vector<String> &p_filters) {
	if (filters == p_filters) {
		return;
	}
	filters = p_filters;
	update_filters();
	invalidate();
}

void EditorFileDialog::set_search_filter(const String &p_search_filter) {
	if (search_string == p_search_filter) {
		return;
	}
	search_string = p_search_filter;
	update_search_filter_gui();
	emit_signal(SNAME("filename_filter_changed"), filter);
	invalidate();
}

Vector<String> EditorFileDialog::get_filters() const {
	return filters;
}

String EditorFileDialog::get_search_filter() const {
	return search_string;
}

String EditorFileDialog::get_current_dir() const {
	return dir_access->get_current_dir();
}

String EditorFileDialog::get_current_file() const {
	return file->get_text();
}

String EditorFileDialog::get_current_path() const {
	return dir_access->get_current_dir().path_join(file->get_text());
}

void EditorFileDialog::set_current_dir(const String &p_dir) {
	if (p_dir.is_relative_path()) {
		dir_access->change_dir(OS::get_singleton()->get_resource_dir());
	}
	dir_access->change_dir(p_dir);
	update_dir();
	invalidate();
}

void EditorFileDialog::set_current_file(const String &p_file) {
	file->set_text(p_file);
	update_dir();
	invalidate();
	_focus_file_text();

	if (is_visible()) {
		_request_single_thumbnail(get_current_dir().path_join(get_current_file()));
	}
}

void EditorFileDialog::set_current_path(const String &p_path) {
	if (!p_path.size()) {
		return;
	}
	int pos = MAX(p_path.rfind_char('/'), p_path.rfind_char('\\'));
	if (pos == -1) {
		set_current_file(p_path);
	} else {
		String path_dir = p_path.substr(0, pos);
		String path_file = p_path.substr(pos + 1, p_path.length());
		set_current_dir(path_dir);
		set_current_file(path_file);
	}
}

void EditorFileDialog::set_file_mode(FileMode p_mode) {
	mode = p_mode;
	switch (mode) {
		case FILE_MODE_OPEN_FILE:
			set_ok_button_text(TTR("Open"));
			set_title(TTR("Open a File"));
			can_create_dir = false;
			break;
		case FILE_MODE_OPEN_FILES:
			set_ok_button_text(TTR("Open"));
			set_title(TTR("Open File(s)"));
			can_create_dir = false;
			break;
		case FILE_MODE_OPEN_DIR:
			set_ok_button_text(TTR("Open"));
			set_title(TTR("Open a Directory"));
			can_create_dir = true;
			break;
		case FILE_MODE_OPEN_ANY:
			set_ok_button_text(TTR("Open"));
			set_title(TTR("Open a File or Directory"));
			can_create_dir = true;
			break;
		case FILE_MODE_SAVE_FILE:
			set_ok_button_text(TTR("Save"));
			set_title(TTR("Save a File"));
			can_create_dir = true;
			break;
	}

	if (mode == FILE_MODE_OPEN_FILES) {
		item_list->set_select_mode(ItemList::SELECT_MULTI);
	} else {
		item_list->set_select_mode(ItemList::SELECT_SINGLE);
	}

	makedir_sep->set_visible(can_create_dir);
	makedir->set_visible(can_create_dir);
}

EditorFileDialog::FileMode EditorFileDialog::get_file_mode() const {
	return mode;
}

void EditorFileDialog::set_access(Access p_access) {
	ERR_FAIL_INDEX(p_access, 3);
	if (access == p_access) {
		return;
	}
	switch (p_access) {
		case ACCESS_FILESYSTEM: {
			dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		} break;
		case ACCESS_RESOURCES: {
			dir_access = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		} break;
		case ACCESS_USERDATA: {
			dir_access = DirAccess::create(DirAccess::ACCESS_USERDATA);
		} break;
	}
	access = p_access;
	_update_drives();
	invalidate();
	update_filters();
	update_dir();
}

void EditorFileDialog::invalidate() {
	if (!is_visible() || is_invalidating) {
		return;
	}

	is_invalidating = true;
	callable_mp(this, &EditorFileDialog::_invalidate).call_deferred();
}

void EditorFileDialog::_invalidate() {
	if (!is_invalidating) {
		return;
	}

	update_file_list();
	_update_favorites();
	_update_recent();

	is_invalidating = false;
}

EditorFileDialog::Access EditorFileDialog::get_access() const {
	return access;
}

void EditorFileDialog::_make_dir_confirm() {
	const String stripped_dirname = makedirname->get_text().strip_edges();

	if (stripped_dirname.is_empty()) {
		error_dialog->set_text(TTR("The path specified is invalid."));
		error_dialog->popup_centered(Size2(250, 50) * EDSCALE);
		makedirname->set_text(""); // Reset label.
		return;
	}

	if (dir_access->dir_exists(stripped_dirname)) {
		error_dialog->set_text(TTR("Could not create folder. File with that name already exists."));
		error_dialog->popup_centered(Size2(250, 50) * EDSCALE);
		makedirname->set_text(""); // Reset label.
		return;
	}

	Error err = dir_access->make_dir(stripped_dirname);
	if (err == OK) {
		dir_access->change_dir(stripped_dirname);
		invalidate();
		update_filters();
		update_dir();
		_push_history();
		if (access != ACCESS_FILESYSTEM) {
			EditorFileSystem::get_singleton()->scan_changes(); //we created a dir, so rescan changes
		}
	} else {
		error_dialog->set_text(TTR("Could not create folder."));
		error_dialog->popup_centered(Size2(250, 50) * EDSCALE);
	}
	makedirname->set_text(""); // reset label
}

void EditorFileDialog::_make_dir() {
	makedialog->popup_centered(Size2(250, 80) * EDSCALE);
	makedirname->grab_focus();
}

void EditorFileDialog::_focus_filter_box() {
	filter_box->grab_focus();
	filter_box->select_all();
}

void EditorFileDialog::_filter_changed(const String &p_text) {
	search_string = p_text;
	invalidate();

	if (item_list->get_selected_items().size() > 0) {
		item_list->deselect_all();
		if (item_list->get_item_count() > 0) {
			item_list->call_deferred("select", 0);
		}
	}
}

void EditorFileDialog::_file_sort_popup(int p_id) {
	for (int i = 0; i != static_cast<int>(FileSortOption::FILE_SORT_MAX); i++) {
		file_sort_button->get_popup()->set_item_checked(i, (i == p_id));
	}
	file_sort = static_cast<FileSortOption>(p_id);
	invalidate();
}

void EditorFileDialog::_delete_items() {
	// Collect the selected folders and files to delete and check them in the deletion dependency dialog.
	Vector<String> folders;
	Vector<String> files;
	for (int i = 0; i < item_list->get_item_count(); i++) {
		if (!item_list->is_selected(i)) {
			continue;
		}
		Dictionary item_meta = item_list->get_item_metadata(i);
		if (item_meta["dir"]) {
			folders.push_back(item_meta["path"]);
		} else {
			files.push_back(item_meta["path"]);
		}
	}
	if (folders.size() + files.size() > 0) {
		if (access == ACCESS_FILESYSTEM) {
			global_remove_dialog->popup_centered();
		} else {
			dep_remove_dialog->reset_size();
			dep_remove_dialog->show(folders, files);
		}
	}
}

void EditorFileDialog::_delete_files_global() {
	// Delete files outside of the project directory without dependency checks.
	for (int i = 0; i < item_list->get_item_count(); i++) {
		if (!item_list->is_selected(i)) {
			continue;
		}
		Dictionary item_meta = item_list->get_item_metadata(i);
		// Only delete empty directories for safety.
		dir_access->remove(item_meta["path"]);
	}
	update_file_list();
}

void EditorFileDialog::_select_drive(int p_idx) {
	String d = drives->get_item_text(p_idx);
	dir_access->change_dir(d);
	file->set_text("");
	invalidate();
	update_dir();
	_push_history();
}

void EditorFileDialog::_update_drives(bool p_select) {
	int dc = dir_access->get_drive_count();
	if (dc == 0 || access != ACCESS_FILESYSTEM) {
		shortcuts_container->hide();
		drives_container->hide();
		drives->hide();
	} else {
		drives->clear();
		Node *dp = drives->get_parent();
		if (dp) {
			dp->remove_child(drives);
		}
		dp = dir_access->drives_are_shortcuts() ? shortcuts_container : drives_container;
		shortcuts_container->set_visible(dir_access->drives_are_shortcuts());
		drives_container->set_visible(!dir_access->drives_are_shortcuts());
		dp->add_child(drives);
		drives->show();

		for (int i = 0; i < dir_access->get_drive_count(); i++) {
			String d = dir_access->get_drive(i);
			drives->add_item(dir_access->get_drive(i));
		}
		if (p_select) {
			drives->select(dir_access->get_current_drive());
		}
	}
}

void EditorFileDialog::_update_icons() {
	// Update icons.

	mode_thumbnails->set_button_icon(theme_cache.mode_thumbnails);
	mode_list->set_button_icon(theme_cache.mode_list);

	if (is_layout_rtl()) {
		dir_prev->set_button_icon(theme_cache.forward_folder);
		dir_next->set_button_icon(theme_cache.back_folder);
	} else {
		dir_prev->set_button_icon(theme_cache.back_folder);
		dir_next->set_button_icon(theme_cache.forward_folder);
	}
	dir_up->set_button_icon(theme_cache.parent_folder);

	refresh->set_button_icon(theme_cache.reload);
	favorite->set_button_icon(theme_cache.favorite);
	show_hidden->set_button_icon(theme_cache.toggle_hidden);
	makedir->set_button_icon(theme_cache.create_folder);

	filter_box->set_right_icon(theme_cache.filter_box);
	file_sort_button->set_button_icon(theme_cache.file_sort_button);
	show_search_filter_button->set_button_icon(theme_cache.toggle_filename_filter);
	filter_box->set_clear_button_enabled(true);

	fav_up->set_button_icon(theme_cache.favorites_up);
	fav_down->set_button_icon(theme_cache.favorites_down);
}

void EditorFileDialog::_favorite_selected(int p_idx) {
	Error change_dir_result = dir_access->change_dir(favorites->get_item_metadata(p_idx));
	if (change_dir_result != OK) {
		error_dialog->set_text(TTR("Favorited folder does not exist anymore and will be removed."));
		error_dialog->popup_centered(Size2(250, 50) * EDSCALE);

		bool res = (access == ACCESS_RESOURCES);

		Vector<String> favorited = EditorSettings::get_singleton()->get_favorites();
		String dir_to_remove = favorites->get_item_metadata(p_idx);

		bool found = false;
		for (int i = 0; i < favorited.size(); i++) {
			bool cres = favorited[i].begins_with("res://");
			if (cres != res) {
				continue;
			}

			if (favorited[i] == dir_to_remove) {
				found = true;
				break;
			}
		}

		if (found) {
			favorited.erase(favorites->get_item_metadata(p_idx));
			favorites->remove_item(p_idx);
			EditorSettings::get_singleton()->set_favorites(favorited);
		}
	} else {
		update_dir();
		invalidate();
		_push_history();
	}
}

void EditorFileDialog::_favorite_move_up() {
	int current = favorites->get_current();

	if (current > 0 && current < favorites->get_item_count()) {
		Vector<String> favorited = EditorSettings::get_singleton()->get_favorites();

		int a_idx = favorited.find(String(favorites->get_item_metadata(current - 1)));
		int b_idx = favorited.find(String(favorites->get_item_metadata(current)));

		if (a_idx == -1 || b_idx == -1) {
			return;
		}
		SWAP(favorited.write[a_idx], favorited.write[b_idx]);

		EditorSettings::get_singleton()->set_favorites(favorited);

		_update_favorites();
		update_file_list();
	}
}

void EditorFileDialog::_favorite_move_down() {
	int current = favorites->get_current();

	if (current >= 0 && current < favorites->get_item_count() - 1) {
		Vector<String> favorited = EditorSettings::get_singleton()->get_favorites();

		int a_idx = favorited.find(String(favorites->get_item_metadata(current + 1)));
		int b_idx = favorited.find(String(favorites->get_item_metadata(current)));

		if (a_idx == -1 || b_idx == -1) {
			return;
		}
		SWAP(favorited.write[a_idx], favorited.write[b_idx]);

		EditorSettings::get_singleton()->set_favorites(favorited);

		_update_favorites();
		update_file_list();
	}
}

void EditorFileDialog::_update_favorites() {
	bool access_resources = (access == ACCESS_RESOURCES);

	String current = get_current_dir();
	favorites->clear();

	favorite->set_pressed(false);

	Vector<String> favorited = EditorSettings::get_singleton()->get_favorites();
	Vector<String> favorited_paths;
	Vector<String> favorited_names;

	bool fav_changed = false;
	int current_favorite = -1;
	for (int i = 0; i < favorited.size(); i++) {
		String name = favorited[i];

		if (access_resources != name.begins_with("res://")) {
			continue;
		}

		if (!name.ends_with("/")) {
			continue;
		}

		if (!dir_access->dir_exists(name)) {
			// Remove invalid directory from the list of Favorited directories.
			favorited.remove_at(i--);
			fav_changed = true;
			continue;
		}

		// Compute favorite display text.
		if (access_resources && name == "res://") {
			if (name == current) {
				current_favorite = favorited_paths.size();
			}
			name = "/";
			favorited_paths.append(favorited[i]);
			favorited_names.append(name);
		} else {
			if (name == current || name == current + "/") {
				current_favorite = favorited_paths.size();
			}
			name = name.trim_suffix("/");
			name = name.get_file();
			favorited_paths.append(favorited[i]);
			favorited_names.append(name);
		}
	}

	if (fav_changed) {
		EditorSettings::get_singleton()->set_favorites(favorited);
	}

	EditorNode::disambiguate_filenames(favorited_paths, favorited_names);

	for (int i = 0; i < favorited_paths.size(); i++) {
		favorites->add_item(favorited_names[i], theme_cache.folder);
		favorites->set_item_tooltip(-1, favorited_paths[i]);
		favorites->set_item_metadata(-1, favorited_paths[i]);
		favorites->set_item_icon_modulate(-1, get_dir_icon_color(favorited_paths[i]));

		if (i == current_favorite) {
			favorite->set_pressed(true);
			favorites->set_current(favorites->get_item_count() - 1);
			recent->deselect_all();
		}
	}

	fav_up->set_disabled(current_favorite < 1);
	fav_down->set_disabled(current_favorite == -1 || favorited_paths.size() - 1 <= current_favorite);
}

void EditorFileDialog::_favorite_pressed() {
	bool access_resources = (access == ACCESS_RESOURCES);

	String cd = get_current_dir();
	if (!cd.ends_with("/")) {
		cd += "/";
	}

	Vector<String> favorited = EditorSettings::get_singleton()->get_favorites();

	bool found = false;
	for (const String &name : favorited) {
		if (access_resources != name.begins_with("res://")) {
			continue;
		}

		if (name == cd) {
			found = true;
			break;
		}
	}

	if (found) {
		favorited.erase(cd);
	} else {
		favorited.push_back(cd);
	}

	EditorSettings::get_singleton()->set_favorites(favorited);

	_update_favorites();
}

void EditorFileDialog::_update_recent() {
	recent->clear();

	bool access_resources = (access == ACCESS_RESOURCES);
	Vector<String> recentd = EditorSettings::get_singleton()->get_recent_dirs();
	Vector<String> recentd_paths;
	Vector<String> recentd_names;
	bool modified = false;

	for (int i = 0; i < recentd.size(); i++) {
		String name = recentd[i];
		if (access_resources != name.begins_with("res://")) {
			continue;
		}

		if (!dir_access->dir_exists(name)) {
			// Remove invalid directory from the list of Recent directories.
			recentd.remove_at(i--);
			modified = true;
			continue;
		}

		// Compute recent directory display text.
		if (access_resources && name == "res://") {
			name = "/";
		} else {
			name = name.trim_suffix("/");
			name = name.get_file();
		}
		recentd_paths.append(recentd[i]);
		recentd_names.append(name);
	}

	EditorNode::disambiguate_filenames(recentd_paths, recentd_names);

	for (int i = 0; i < recentd_paths.size(); i++) {
		recent->add_item(recentd_names[i], theme_cache.folder);
		recent->set_item_tooltip(-1, recentd_paths[i]);
		recent->set_item_metadata(-1, recentd_paths[i]);
		recent->set_item_icon_modulate(-1, get_dir_icon_color(recentd_paths[i]));
	}

	if (modified) {
		EditorSettings::get_singleton()->set_recent_dirs(recentd);
	}
}

void EditorFileDialog::_recent_selected(int p_idx) {
	Vector<String> recentd = EditorSettings::get_singleton()->get_recent_dirs();
	ERR_FAIL_INDEX(p_idx, recentd.size());

	dir_access->change_dir(recent->get_item_metadata(p_idx));
	update_file_list();
	update_dir();
	_push_history();
}

void EditorFileDialog::_go_up() {
	dir_access->change_dir(get_current_dir().trim_suffix("/").get_base_dir());
	update_file_list();
	update_dir();
	_push_history();
}

void EditorFileDialog::_go_back() {
	if (local_history_pos <= 0) {
		return;
	}

	local_history_pos--;
	dir_access->change_dir(local_history[local_history_pos]);
	update_file_list();
	update_dir();

	dir_prev->set_disabled(local_history_pos == 0);
	dir_next->set_disabled(local_history_pos == local_history.size() - 1);
}

void EditorFileDialog::_go_forward() {
	if (local_history_pos >= local_history.size() - 1) {
		return;
	}

	local_history_pos++;
	dir_access->change_dir(local_history[local_history_pos]);
	update_file_list();
	update_dir();

	dir_prev->set_disabled(local_history_pos == 0);
	dir_next->set_disabled(local_history_pos == local_history.size() - 1);
}

bool EditorFileDialog::default_show_hidden_files = false;

EditorFileDialog::DisplayMode EditorFileDialog::default_display_mode = DISPLAY_THUMBNAILS;

void EditorFileDialog::set_display_mode(DisplayMode p_mode) {
	if (display_mode == p_mode) {
		return;
	}
	if (p_mode == DISPLAY_THUMBNAILS) {
		mode_list->set_pressed(false);
		mode_thumbnails->set_pressed(true);
	} else {
		mode_thumbnails->set_pressed(false);
		mode_list->set_pressed(true);
	}
	display_mode = p_mode;
	invalidate();
}

EditorFileDialog::DisplayMode EditorFileDialog::get_display_mode() const {
	return display_mode;
}

TypedArray<Dictionary> EditorFileDialog::_get_options() const {
	TypedArray<Dictionary> out;
	for (const EditorFileDialog::Option &opt : options) {
		Dictionary dict;
		dict["name"] = opt.name;
		dict["values"] = opt.values;
		dict["default"] = (int)selected_options.get(opt.name, opt.default_idx);
		out.push_back(dict);
	}
	return out;
}

void EditorFileDialog::_option_changed_checkbox_toggled(bool p_pressed, const String &p_name) {
	if (selected_options.has(p_name)) {
		selected_options[p_name] = p_pressed;
	}
}

void EditorFileDialog::_option_changed_item_selected(int p_idx, const String &p_name) {
	if (selected_options.has(p_name)) {
		selected_options[p_name] = p_idx;
	}
}

void EditorFileDialog::_update_option_controls() {
	if (!options_dirty) {
		return;
	}
	options_dirty = false;

	while (flow_checkbox_options->get_child_count() > 0) {
		Node *child = flow_checkbox_options->get_child(0);
		flow_checkbox_options->remove_child(child);
		child->queue_free();
	}
	while (grid_select_options->get_child_count() > 0) {
		Node *child = grid_select_options->get_child(0);
		grid_select_options->remove_child(child);
		child->queue_free();
	}

	selected_options.clear();

	for (const EditorFileDialog::Option &opt : options) {
		if (opt.values.is_empty()) {
			CheckBox *cb = memnew(CheckBox);
			cb->set_pressed(opt.default_idx);
			cb->set_text(opt.name);
			flow_checkbox_options->add_child(cb);
			cb->connect(SceneStringName(toggled), callable_mp(this, &EditorFileDialog::_option_changed_checkbox_toggled).bind(opt.name));
			selected_options[opt.name] = (bool)opt.default_idx;
		} else {
			Label *lbl = memnew(Label);
			lbl->set_text(opt.name);
			grid_select_options->add_child(lbl);

			OptionButton *ob = memnew(OptionButton);
			for (const String &val : opt.values) {
				ob->add_item(val);
			}
			ob->select(opt.default_idx);
			grid_select_options->add_child(ob);
			ob->connect(SceneStringName(item_selected), callable_mp(this, &EditorFileDialog::_option_changed_item_selected).bind(opt.name));
			selected_options[opt.name] = opt.default_idx;
		}
	}
}

Dictionary EditorFileDialog::get_selected_options() const {
	return selected_options;
}

String EditorFileDialog::get_option_name(int p_option) const {
	ERR_FAIL_INDEX_V(p_option, options.size(), String());
	return options[p_option].name;
}

Vector<String> EditorFileDialog::get_option_values(int p_option) const {
	ERR_FAIL_INDEX_V(p_option, options.size(), Vector<String>());
	return options[p_option].values;
}

int EditorFileDialog::get_option_default(int p_option) const {
	ERR_FAIL_INDEX_V(p_option, options.size(), -1);
	return options[p_option].default_idx;
}

void EditorFileDialog::set_option_name(int p_option, const String &p_name) {
	if (p_option < 0) {
		p_option += get_option_count();
	}
	ERR_FAIL_INDEX(p_option, options.size());
	options.write[p_option].name = p_name;
	options_dirty = true;
	if (is_visible()) {
		_update_option_controls();
	}
}

void EditorFileDialog::set_option_values(int p_option, const Vector<String> &p_values) {
	if (p_option < 0) {
		p_option += get_option_count();
	}
	ERR_FAIL_INDEX(p_option, options.size());
	options.write[p_option].values = p_values;
	if (p_values.is_empty()) {
		options.write[p_option].default_idx = CLAMP(options[p_option].default_idx, 0, 1);
	} else {
		options.write[p_option].default_idx = CLAMP(options[p_option].default_idx, 0, options[p_option].values.size() - 1);
	}
	options_dirty = true;
	if (is_visible()) {
		_update_option_controls();
	}
}

void EditorFileDialog::set_option_default(int p_option, int p_index) {
	if (p_option < 0) {
		p_option += get_option_count();
	}
	ERR_FAIL_INDEX(p_option, options.size());
	if (options[p_option].values.is_empty()) {
		options.write[p_option].default_idx = CLAMP(p_index, 0, 1);
	} else {
		options.write[p_option].default_idx = CLAMP(p_index, 0, options[p_option].values.size() - 1);
	}
	options_dirty = true;
	if (is_visible()) {
		_update_option_controls();
	}
}

void EditorFileDialog::add_option(const String &p_name, const Vector<String> &p_values, int p_index) {
	Option opt;
	opt.name = p_name;
	opt.values = p_values;
	if (opt.values.is_empty()) {
		opt.default_idx = CLAMP(p_index, 0, 1);
	} else {
		opt.default_idx = CLAMP(p_index, 0, opt.values.size() - 1);
	}
	options.push_back(opt);
	options_dirty = true;
	if (is_visible()) {
		_update_option_controls();
	}
}

void EditorFileDialog::set_option_count(int p_count) {
	ERR_FAIL_COND(p_count < 0);

	if (options.size() == p_count) {
		return;
	}
	options.resize(p_count);

	options_dirty = true;
	notify_property_list_changed();
	if (is_visible()) {
		_update_option_controls();
	}
}

int EditorFileDialog::get_option_count() const {
	return options.size();
}

void EditorFileDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_cancel_pressed"), &EditorFileDialog::_cancel_pressed);

	ClassDB::bind_method(D_METHOD("clear_filters"), &EditorFileDialog::clear_filters);
	ClassDB::bind_method(D_METHOD("add_filter", "filter", "description"), &EditorFileDialog::add_filter, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("set_filters", "filters"), &EditorFileDialog::set_filters);
	ClassDB::bind_method(D_METHOD("get_filters"), &EditorFileDialog::get_filters);
	ClassDB::bind_method(D_METHOD("get_option_name", "option"), &EditorFileDialog::get_option_name);
	ClassDB::bind_method(D_METHOD("get_option_values", "option"), &EditorFileDialog::get_option_values);
	ClassDB::bind_method(D_METHOD("get_option_default", "option"), &EditorFileDialog::get_option_default);
	ClassDB::bind_method(D_METHOD("set_option_name", "option", "name"), &EditorFileDialog::set_option_name);
	ClassDB::bind_method(D_METHOD("set_option_values", "option", "values"), &EditorFileDialog::set_option_values);
	ClassDB::bind_method(D_METHOD("set_option_default", "option", "default_value_index"), &EditorFileDialog::set_option_default);
	ClassDB::bind_method(D_METHOD("set_option_count", "count"), &EditorFileDialog::set_option_count);
	ClassDB::bind_method(D_METHOD("get_option_count"), &EditorFileDialog::get_option_count);
	ClassDB::bind_method(D_METHOD("add_option", "name", "values", "default_value_index"), &EditorFileDialog::add_option);
	ClassDB::bind_method(D_METHOD("get_selected_options"), &EditorFileDialog::get_selected_options);
	ClassDB::bind_method(D_METHOD("clear_filename_filter"), &EditorFileDialog::clear_search_filter);
	ClassDB::bind_method(D_METHOD("set_filename_filter", "filter"), &EditorFileDialog::set_search_filter);
	ClassDB::bind_method(D_METHOD("get_filename_filter"), &EditorFileDialog::get_search_filter);
	ClassDB::bind_method(D_METHOD("get_current_dir"), &EditorFileDialog::get_current_dir);
	ClassDB::bind_method(D_METHOD("get_current_file"), &EditorFileDialog::get_current_file);
	ClassDB::bind_method(D_METHOD("get_current_path"), &EditorFileDialog::get_current_path);
	ClassDB::bind_method(D_METHOD("set_current_dir", "dir"), &EditorFileDialog::set_current_dir);
	ClassDB::bind_method(D_METHOD("set_current_file", "file"), &EditorFileDialog::set_current_file);
	ClassDB::bind_method(D_METHOD("set_current_path", "path"), &EditorFileDialog::set_current_path);
	ClassDB::bind_method(D_METHOD("set_file_mode", "mode"), &EditorFileDialog::set_file_mode);
	ClassDB::bind_method(D_METHOD("get_file_mode"), &EditorFileDialog::get_file_mode);
	ClassDB::bind_method(D_METHOD("get_vbox"), &EditorFileDialog::get_vbox);
	ClassDB::bind_method(D_METHOD("get_line_edit"), &EditorFileDialog::get_line_edit);
	ClassDB::bind_method(D_METHOD("set_access", "access"), &EditorFileDialog::set_access);
	ClassDB::bind_method(D_METHOD("get_access"), &EditorFileDialog::get_access);
	ClassDB::bind_method(D_METHOD("set_show_hidden_files", "show"), &EditorFileDialog::set_show_hidden_files);
	ClassDB::bind_method(D_METHOD("is_showing_hidden_files"), &EditorFileDialog::is_showing_hidden_files);
	ClassDB::bind_method(D_METHOD("_thumbnail_done"), &EditorFileDialog::_thumbnail_done);
	ClassDB::bind_method(D_METHOD("set_display_mode", "mode"), &EditorFileDialog::set_display_mode);
	ClassDB::bind_method(D_METHOD("get_display_mode"), &EditorFileDialog::get_display_mode);
	ClassDB::bind_method(D_METHOD("_thumbnail_result"), &EditorFileDialog::_thumbnail_result);
	ClassDB::bind_method(D_METHOD("set_disable_overwrite_warning", "disable"), &EditorFileDialog::set_disable_overwrite_warning);
	ClassDB::bind_method(D_METHOD("is_overwrite_warning_disabled"), &EditorFileDialog::is_overwrite_warning_disabled);
	ClassDB::bind_method(D_METHOD("add_side_menu", "menu", "title"), &EditorFileDialog::add_side_menu, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("popup_file_dialog"), &EditorFileDialog::popup_file_dialog);

	ClassDB::bind_method(D_METHOD("invalidate"), &EditorFileDialog::invalidate);

	ADD_SIGNAL(MethodInfo("file_selected", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("files_selected", PropertyInfo(Variant::PACKED_STRING_ARRAY, "paths")));
	ADD_SIGNAL(MethodInfo("dir_selected", PropertyInfo(Variant::STRING, "dir")));
	ADD_SIGNAL(MethodInfo("filename_filter_changed", PropertyInfo(Variant::STRING, "filter")));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "access", PROPERTY_HINT_ENUM, "Resources,User data,File system"), "set_access", "get_access");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "display_mode", PROPERTY_HINT_ENUM, "Thumbnails,List"), "set_display_mode", "get_display_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "file_mode", PROPERTY_HINT_ENUM, "Open one,Open many,Open folder,Open any,Save"), "set_file_mode", "get_file_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "current_dir", PROPERTY_HINT_DIR, "", PROPERTY_USAGE_NONE), "set_current_dir", "get_current_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "current_file", PROPERTY_HINT_FILE, "*", PROPERTY_USAGE_NONE), "set_current_file", "get_current_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "current_path", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_current_path", "get_current_path");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "filters"), "set_filters", "get_filters");
	ADD_ARRAY_COUNT("Options", "option_count", "set_option_count", "get_option_count", "option_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_hidden_files"), "set_show_hidden_files", "is_showing_hidden_files");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_overwrite_warning"), "set_disable_overwrite_warning", "is_overwrite_warning_disabled");

	BIND_ENUM_CONSTANT(FILE_MODE_OPEN_FILE);
	BIND_ENUM_CONSTANT(FILE_MODE_OPEN_FILES);
	BIND_ENUM_CONSTANT(FILE_MODE_OPEN_DIR);
	BIND_ENUM_CONSTANT(FILE_MODE_OPEN_ANY);
	BIND_ENUM_CONSTANT(FILE_MODE_SAVE_FILE);

	BIND_ENUM_CONSTANT(ACCESS_RESOURCES);
	BIND_ENUM_CONSTANT(ACCESS_USERDATA);
	BIND_ENUM_CONSTANT(ACCESS_FILESYSTEM);

	BIND_ENUM_CONSTANT(DISPLAY_THUMBNAILS);
	BIND_ENUM_CONSTANT(DISPLAY_LIST);

	Option defaults;

	base_property_helper.set_prefix("option_");
	base_property_helper.set_array_length_getter(&EditorFileDialog::get_option_count);
	base_property_helper.register_property(PropertyInfo(Variant::STRING, "name"), defaults.name, &EditorFileDialog::set_option_name, &EditorFileDialog::get_option_name);
	base_property_helper.register_property(PropertyInfo(Variant::PACKED_STRING_ARRAY, "values"), defaults.values, &EditorFileDialog::set_option_values, &EditorFileDialog::get_option_values);
	base_property_helper.register_property(PropertyInfo(Variant::INT, "default"), defaults.default_idx, &EditorFileDialog::set_option_default, &EditorFileDialog::get_option_default);
	PropertyListHelper::register_base_helper(&base_property_helper);
}

void EditorFileDialog::set_show_hidden_files(bool p_show) {
	if (p_show == show_hidden_files) {
		return;
	}

	EditorSettings::get_singleton()->set("filesystem/file_dialog/show_hidden_files", p_show);
	show_hidden_files = p_show;
	show_hidden->set_pressed(p_show);
	invalidate();
}

void EditorFileDialog::set_show_search_filter(bool p_show) {
	if (p_show == show_search_filter) {
		return;
	}
	if (p_show) {
		filter_box->grab_focus();
	} else {
		search_string.clear();
		filter_box->clear();
		if (filter_box->has_focus()) {
			item_list->call_deferred("grab_focus");
		}
	}
	show_search_filter = p_show;
	update_search_filter_gui();
	invalidate();
}

bool EditorFileDialog::is_showing_hidden_files() const {
	return show_hidden_files;
}

void EditorFileDialog::set_default_show_hidden_files(bool p_show) {
	default_show_hidden_files = p_show;
}

void EditorFileDialog::set_default_display_mode(DisplayMode p_mode) {
	default_display_mode = p_mode;
}

void EditorFileDialog::_save_to_recent() {
	String cur_dir = get_current_dir();
	Vector<String> recent_new = EditorSettings::get_singleton()->get_recent_dirs();

	const int max = 20;
	int count = 0;
	bool res = cur_dir.begins_with("res://");

	for (int i = 0; i < recent_new.size(); i++) {
		bool cres = recent_new[i].begins_with("res://");
		if (recent_new[i] == cur_dir || (res == cres && count > max)) {
			recent_new.remove_at(i);
			i--;
		} else {
			count++;
		}
	}

	recent_new.insert(0, cur_dir);

	EditorSettings::get_singleton()->set_recent_dirs(recent_new);
}

void EditorFileDialog::set_disable_overwrite_warning(bool p_disable) {
	disable_overwrite_warning = p_disable;
}

bool EditorFileDialog::is_overwrite_warning_disabled() const {
	return disable_overwrite_warning;
}

void EditorFileDialog::set_previews_enabled(bool p_enabled) {
	previews_enabled = p_enabled;
}

bool EditorFileDialog::are_previews_enabled() {
	return previews_enabled;
}

void EditorFileDialog::add_side_menu(Control *p_menu, const String &p_title) {
	// HSplitContainer has 3 children at maximum capacity, 1 of them is the SplitContainerDragger.
	ERR_FAIL_COND_MSG(body_hsplit->get_child_count() > 2, "EditorFileDialog: Only one side menu can be added.");
	// Everything for the side menu goes inside of a VBoxContainer.
	side_vbox = memnew(VBoxContainer);
	side_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	side_vbox->set_stretch_ratio(0.5);
	body_hsplit->add_child(side_vbox);
	// Add a Label to the VBoxContainer.
	if (!p_title.is_empty()) {
		Label *title_label = memnew(Label(p_title));
		title_label->set_theme_type_variation("HeaderSmall");
		side_vbox->add_child(title_label);
	}
	// Add the given menu to the VBoxContainer.
	p_menu->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	side_vbox->add_child(p_menu);
}

void EditorFileDialog::_update_side_menu_visibility(bool p_native_dlg) {
	if (p_native_dlg) {
		pathhb->set_visible(false);
		flow_checkbox_options->set_visible(false);
		grid_select_options->set_visible(false);
		list_hb->set_visible(false);
	} else {
		pathhb->set_visible(true);
		flow_checkbox_options->set_visible(true);
		grid_select_options->set_visible(true);
		list_hb->set_visible(true);
	}
}

EditorFileDialog::EditorFileDialog() {
	show_hidden_files = default_show_hidden_files;
	display_mode = default_display_mode;

	vbc = memnew(VBoxContainer);
	add_child(vbc);

	set_title(TTR("Save a File"));

	ED_SHORTCUT("file_dialog/go_back", TTRC("Go Back"), KeyModifierMask::ALT | Key::LEFT);
	ED_SHORTCUT("file_dialog/go_forward", TTRC("Go Forward"), KeyModifierMask::ALT | Key::RIGHT);
	ED_SHORTCUT("file_dialog/go_up", TTRC("Go Up"), KeyModifierMask::ALT | Key::UP);
	ED_SHORTCUT("file_dialog/refresh", TTRC("Refresh"), Key::F5);
	ED_SHORTCUT("file_dialog/toggle_hidden_files", TTRC("Toggle Hidden Files"), KeyModifierMask::CTRL | Key::H);
	ED_SHORTCUT("file_dialog/toggle_favorite", TTRC("Toggle Favorite"), KeyModifierMask::ALT | Key::F);
	ED_SHORTCUT("file_dialog/toggle_mode", TTRC("Toggle Mode"), KeyModifierMask::ALT | Key::V);
	ED_SHORTCUT("file_dialog/create_folder", TTRC("Create Folder"), KeyModifierMask::CMD_OR_CTRL | Key::N);
	ED_SHORTCUT("file_dialog/delete", TTRC("Delete"), Key::KEY_DELETE);
	ED_SHORTCUT("file_dialog/focus_path", TTRC("Focus Path"), KeyModifierMask::CMD_OR_CTRL | Key::L);
	// Allow both Cmd + L and Cmd + Shift + G to match Safari's and Finder's shortcuts respectively.
	ED_SHORTCUT_OVERRIDE_ARRAY("file_dialog/focus_path", "macos",
			{ int32_t(KeyModifierMask::META | Key::L), int32_t(KeyModifierMask::META | KeyModifierMask::SHIFT | Key::G) });
	ED_SHORTCUT("file_dialog/focus_filter", TTRC("Focus Filter"), KeyModifierMask::CMD_OR_CTRL | Key::F);
	ED_SHORTCUT("file_dialog/move_favorite_up", TTRC("Move Favorite Up"), KeyModifierMask::CMD_OR_CTRL | Key::UP);
	ED_SHORTCUT("file_dialog/move_favorite_down", TTRC("Move Favorite Down"), KeyModifierMask::CMD_OR_CTRL | Key::DOWN);

	ED_SHORTCUT_OVERRIDE("file_dialog/toggle_hidden_files", "macos", KeyModifierMask::META | KeyModifierMask::SHIFT | Key::PERIOD);
	ED_SHORTCUT_OVERRIDE("file_dialog/toggle_favorite", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::F);
	ED_SHORTCUT_OVERRIDE("file_dialog/toggle_mode", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::V);

	pathhb = memnew(HBoxContainer);
	vbc->add_child(pathhb);

	dir_prev = memnew(Button);
	dir_prev->set_theme_type_variation(SceneStringName(FlatButton));
	dir_prev->set_tooltip_text(TTR("Go to previous folder."));
	dir_next = memnew(Button);
	dir_next->set_theme_type_variation(SceneStringName(FlatButton));
	dir_next->set_tooltip_text(TTR("Go to next folder."));
	dir_up = memnew(Button);
	dir_up->set_theme_type_variation(SceneStringName(FlatButton));
	dir_up->set_tooltip_text(TTR("Go to parent folder."));

	pathhb->add_child(dir_prev);
	pathhb->add_child(dir_next);
	pathhb->add_child(dir_up);

	dir_prev->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::_go_back));
	dir_next->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::_go_forward));
	dir_up->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::_go_up));

	Label *l = memnew(Label(TTR("Path:")));
	l->set_theme_type_variation("HeaderSmall");
	pathhb->add_child(l);

	drives_container = memnew(HBoxContainer);
	pathhb->add_child(drives_container);

	dir = memnew(LineEdit);
	dir->set_structured_text_bidi_override(TextServer::STRUCTURED_TEXT_FILE);
	pathhb->add_child(dir);

	refresh = memnew(Button);
	refresh->set_theme_type_variation(SceneStringName(FlatButton));
	refresh->set_tooltip_text(TTR("Refresh files."));
	refresh->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::update_file_list));
	pathhb->add_child(refresh);

	favorite = memnew(Button);
	favorite->set_theme_type_variation(SceneStringName(FlatButton));
	favorite->set_toggle_mode(true);
	favorite->set_tooltip_text(TTR("(Un)favorite current folder."));
	favorite->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::_favorite_pressed));
	pathhb->add_child(favorite);

	shortcuts_container = memnew(HBoxContainer);
	pathhb->add_child(shortcuts_container);

	drives = memnew(OptionButton);
	drives->connect(SceneStringName(item_selected), callable_mp(this, &EditorFileDialog::_select_drive));
	pathhb->add_child(drives);

	makedir_sep = memnew(VSeparator);
	pathhb->add_child(makedir_sep);

	makedir = memnew(Button);
	makedir->set_theme_type_variation(SceneStringName(FlatButton));
	makedir->set_tooltip_text(TTR("Create a new folder."));
	makedir->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::_make_dir));
	pathhb->add_child(makedir);

	dir->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	body_hsplit = memnew(HSplitContainer);
	body_hsplit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbc->add_child(body_hsplit);

	flow_checkbox_options = memnew(HFlowContainer);
	flow_checkbox_options->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	flow_checkbox_options->set_alignment(FlowContainer::ALIGNMENT_CENTER);
	vbc->add_child(flow_checkbox_options);

	grid_select_options = memnew(GridContainer);
	grid_select_options->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	grid_select_options->set_columns(2);
	vbc->add_child(grid_select_options);

	list_hb = memnew(HSplitContainer);
	list_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	body_hsplit->add_child(list_hb);

	VSplitContainer *vsc = memnew(VSplitContainer);
	list_hb->add_child(vsc);

	VBoxContainer *fav_vb = memnew(VBoxContainer);
	vsc->add_child(fav_vb);
	fav_vb->set_custom_minimum_size(Size2(150, 100) * EDSCALE);
	fav_vb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	HBoxContainer *fav_hb = memnew(HBoxContainer);
	fav_vb->add_child(fav_hb);

	l = memnew(Label(TTR("Favorites:")));
	l->set_theme_type_variation("HeaderSmall");
	fav_hb->add_child(l);

	fav_hb->add_spacer();
	fav_up = memnew(Button);
	fav_up->set_theme_type_variation(SceneStringName(FlatButton));
	fav_hb->add_child(fav_up);
	fav_up->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::_favorite_move_up));
	fav_down = memnew(Button);
	fav_down->set_theme_type_variation(SceneStringName(FlatButton));
	fav_hb->add_child(fav_down);
	fav_down->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::_favorite_move_down));

	favorites = memnew(ItemList);
	favorites->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	fav_vb->add_child(favorites);
	favorites->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	favorites->set_theme_type_variation("ItemListSecondary");
	favorites->connect(SceneStringName(item_selected), callable_mp(this, &EditorFileDialog::_favorite_selected));

	VBoxContainer *rec_vb = memnew(VBoxContainer);
	vsc->add_child(rec_vb);
	rec_vb->set_custom_minimum_size(Size2(150, 100) * EDSCALE);
	rec_vb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	recent = memnew(ItemList);
	recent->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	recent->set_allow_reselect(true);
	recent->set_theme_type_variation("ItemListSecondary");
	rec_vb->add_margin_child(TTR("Recent:"), recent, true);
	recent->connect(SceneStringName(item_selected), callable_mp(this, &EditorFileDialog::_recent_selected));

	VBoxContainer *item_vb = memnew(VBoxContainer);
	list_hb->add_child(item_vb);
	item_vb->set_custom_minimum_size(Size2(320, 0) * EDSCALE);

	HBoxContainer *preview_hb = memnew(HBoxContainer);
	preview_hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	item_vb->add_child(preview_hb);

	VBoxContainer *list_vb = memnew(VBoxContainer);
	list_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	HBoxContainer *lower_hb = memnew(HBoxContainer);
	lower_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	l = memnew(Label(TTR("Directories & Files:")));
	l->set_theme_type_variation("HeaderSmall");
	l->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	lower_hb->add_child(l);

	show_hidden = memnew(Button);
	show_hidden->set_theme_type_variation(SceneStringName(FlatButton));
	show_hidden->set_toggle_mode(true);
	show_hidden->set_pressed(is_showing_hidden_files());
	show_hidden->set_tooltip_text(TTR("Toggle the visibility of hidden files."));
	show_hidden->connect(SceneStringName(toggled), callable_mp(this, &EditorFileDialog::set_show_hidden_files));
	lower_hb->add_child(show_hidden);

	lower_hb->add_child(memnew(VSeparator));

	Ref<ButtonGroup> view_mode_group;
	view_mode_group.instantiate();

	mode_thumbnails = memnew(Button);
	mode_thumbnails->set_theme_type_variation(SceneStringName(FlatButton));
	mode_thumbnails->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::set_display_mode).bind(DISPLAY_THUMBNAILS));
	mode_thumbnails->set_toggle_mode(true);
	mode_thumbnails->set_pressed(display_mode == DISPLAY_THUMBNAILS);
	mode_thumbnails->set_button_group(view_mode_group);
	mode_thumbnails->set_tooltip_text(TTR("View items as a grid of thumbnails."));
	lower_hb->add_child(mode_thumbnails);

	mode_list = memnew(Button);
	mode_list->set_theme_type_variation(SceneStringName(FlatButton));
	mode_list->connect(SceneStringName(pressed), callable_mp(this, &EditorFileDialog::set_display_mode).bind(DISPLAY_LIST));
	mode_list->set_toggle_mode(true);
	mode_list->set_pressed(display_mode == DISPLAY_LIST);
	mode_list->set_button_group(view_mode_group);
	mode_list->set_tooltip_text(TTR("View items as a list."));
	lower_hb->add_child(mode_list);

	lower_hb->add_child(memnew(VSeparator));

	file_sort_button = memnew(MenuButton);
	file_sort_button->set_flat(false);
	file_sort_button->set_theme_type_variation("FlatMenuButton");
	file_sort_button->set_tooltip_text(TTR("Sort files"));

	show_search_filter_button = memnew(Button);
	show_search_filter_button->set_theme_type_variation(SceneStringName(FlatButton));
	show_search_filter_button->set_toggle_mode(true);
	show_search_filter_button->set_pressed(false);
	show_search_filter_button->set_tooltip_text(TTR("Toggle the visibility of the filter for file names."));
	show_search_filter_button->connect(SceneStringName(toggled), callable_mp(this, &EditorFileDialog::set_show_search_filter));
	lower_hb->add_child(show_search_filter_button);

	PopupMenu *p = file_sort_button->get_popup();
	p->connect(SceneStringName(id_pressed), callable_mp(this, &EditorFileDialog::_file_sort_popup));
	p->add_radio_check_item(TTR("Sort by Name (Ascending)"), static_cast<int>(FileSortOption::FILE_SORT_NAME));
	p->add_radio_check_item(TTR("Sort by Name (Descending)"), static_cast<int>(FileSortOption::FILE_SORT_NAME_REVERSE));
	p->add_radio_check_item(TTR("Sort by Type (Ascending)"), static_cast<int>(FileSortOption::FILE_SORT_TYPE));
	p->add_radio_check_item(TTR("Sort by Type (Descending)"), static_cast<int>(FileSortOption::FILE_SORT_TYPE_REVERSE));
	p->add_radio_check_item(TTR("Sort by Last Modified"), static_cast<int>(FileSortOption::FILE_SORT_MODIFIED_TIME));
	p->add_radio_check_item(TTR("Sort by First Modified"), static_cast<int>(FileSortOption::FILE_SORT_MODIFIED_TIME_REVERSE));
	p->set_item_checked(0, true);
	lower_hb->add_child(file_sort_button);

	list_vb->add_child(lower_hb);
	preview_hb->add_child(list_vb);

	// Item (files and folders) list with context menu.

	item_list = memnew(ItemList);
	item_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	item_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	item_list->connect("item_clicked", callable_mp(this, &EditorFileDialog::_item_list_item_rmb_clicked));
	item_list->connect("empty_clicked", callable_mp(this, &EditorFileDialog::_item_list_empty_clicked));
	item_list->set_allow_rmb_select(true);

	list_vb->add_child(item_list);

	item_menu = memnew(PopupMenu);
	item_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorFileDialog::_item_menu_id_pressed));
	add_child(item_menu);

	// Other stuff.

	preview_vb = memnew(VBoxContainer);
	preview_hb->add_child(preview_vb);
	CenterContainer *prev_cc = memnew(CenterContainer);
	preview_vb->add_margin_child(TTR("Preview:"), prev_cc);
	preview = memnew(TextureRect);
	prev_cc->add_child(preview);
	preview_vb->hide();

	filter_hb = memnew(HBoxContainer);
	filter_hb->add_child(memnew(Label(RTR("Filter:"))));
	filter_box = memnew(LineEdit);
	filter_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	filter_box->set_placeholder(TTR("Filter"));
	filter_hb->add_child(filter_box);
	filter_hb->set_visible(false);
	item_vb->add_child(filter_hb);

	file_box = memnew(HBoxContainer);

	l = memnew(Label(TTR("File:")));
	l->set_theme_type_variation("HeaderSmall");
	file_box->add_child(l);

	file = memnew(LineEdit);
	file->set_structured_text_bidi_override(TextServer::STRUCTURED_TEXT_FILE);
	file->set_stretch_ratio(4);
	file->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	file_box->add_child(file);
	filter = memnew(OptionButton);
	filter->set_stretch_ratio(3);
	filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	filter->set_clip_text(true); // Too many extensions overflow it.
	file_box->add_child(filter);
	file_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	item_vb->add_child(file_box);

	dir_access = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	_update_drives();

	connect(SceneStringName(confirmed), callable_mp(this, &EditorFileDialog::_action_pressed));
	item_list->connect(SceneStringName(item_selected), callable_mp(this, &EditorFileDialog::_item_selected), CONNECT_DEFERRED);
	item_list->connect("multi_selected", callable_mp(this, &EditorFileDialog::_multi_selected), CONNECT_DEFERRED);
	item_list->connect("item_activated", callable_mp(this, &EditorFileDialog::_item_dc_selected).bind());
	item_list->connect("empty_clicked", callable_mp(this, &EditorFileDialog::_items_clear_selection));
	dir->connect(SceneStringName(text_submitted), callable_mp(this, &EditorFileDialog::_dir_submitted));
	filter_box->connect(SceneStringName(text_changed), callable_mp(this, &EditorFileDialog::_filter_changed));
	filter_box->connect(SceneStringName(text_submitted), callable_mp(this, &EditorFileDialog::_search_filter_selected).unbind(1));
	file->connect(SceneStringName(text_submitted), callable_mp(this, &EditorFileDialog::_file_submitted));
	filter->connect(SceneStringName(item_selected), callable_mp(this, &EditorFileDialog::_filter_selected));

	confirm_save = memnew(ConfirmationDialog);
	add_child(confirm_save);
	confirm_save->connect(SceneStringName(confirmed), callable_mp(this, &EditorFileDialog::_save_confirm_pressed));

	dep_remove_dialog = memnew(DependencyRemoveDialog);
	add_child(dep_remove_dialog);

	global_remove_dialog = memnew(ConfirmationDialog);
	global_remove_dialog->set_text(TTR("Remove the selected files? For safety only files and empty directories can be deleted from here. (Cannot be undone.)\nDepending on your filesystem configuration, the files will either be moved to the system trash or deleted permanently."));
	global_remove_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorFileDialog::_delete_files_global));
	add_child(global_remove_dialog);

	makedialog = memnew(ConfirmationDialog);
	makedialog->set_title(TTR("Create Folder"));
	VBoxContainer *makevb = memnew(VBoxContainer);
	makedialog->add_child(makevb);

	makedirname = memnew(LineEdit);
	makedirname->set_structured_text_bidi_override(TextServer::STRUCTURED_TEXT_FILE);
	makevb->add_margin_child(TTR("Name:"), makedirname);
	add_child(makedialog);
	makedialog->register_text_enter(makedirname);
	makedialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorFileDialog::_make_dir_confirm));
	error_dialog = memnew(AcceptDialog);
	add_child(error_dialog);

	update_filters();
	update_dir();

	set_hide_on_ok(false);
	vbox = vbc;

	if (register_func) {
		register_func(this);
	}

	property_helper.setup_for_instance(base_property_helper, this);
}

EditorFileDialog::~EditorFileDialog() {
	if (unregister_func) {
		unregister_func(this);
	}
}
