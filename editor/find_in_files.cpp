/**************************************************************************/
/*  find_in_files.cpp                                                     */
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

#include "find_in_files.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/tree.h"

const char *FindInFiles::SIGNAL_RESULT_FOUND = "result_found";

// TODO: Would be nice in Vector and Vectors.
template <typename T>
inline void pop_back(T &container) {
	container.resize(container.size() - 1);
}

static bool find_next(const String &line, const String &pattern, int from, bool match_case, bool whole_words, int &out_begin, int &out_end) {
	int end = from;

	while (true) {
		int begin = match_case ? line.find(pattern, end) : line.findn(pattern, end);

		if (begin == -1) {
			return false;
		}

		end = begin + pattern.length();
		out_begin = begin;
		out_end = end;

		if (whole_words) {
			if (begin > 0 && (is_ascii_identifier_char(line[begin - 1]))) {
				continue;
			}
			if (end < line.size() && (is_ascii_identifier_char(line[end]))) {
				continue;
			}
		}

		return true;
	}
}

//--------------------------------------------------------------------------------

void FindInFiles::set_search_text(const String &p_pattern) {
	_pattern = p_pattern;
}

void FindInFiles::set_whole_words(bool p_whole_word) {
	_whole_words = p_whole_word;
}

void FindInFiles::set_match_case(bool p_match_case) {
	_match_case = p_match_case;
}

void FindInFiles::set_folder(const String &folder) {
	_root_dir = folder;
}

void FindInFiles::set_filter(const HashSet<String> &exts) {
	_extension_filter = exts;
}

void FindInFiles::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			_process();
		} break;
	}
}

void FindInFiles::start() {
	if (_pattern.is_empty()) {
		print_verbose("Nothing to search, pattern is empty");
		emit_signal(SceneStringName(finished));
		return;
	}
	if (_extension_filter.size() == 0) {
		print_verbose("Nothing to search, filter matches no files");
		emit_signal(SceneStringName(finished));
		return;
	}

	// Init search.
	_current_dir = "";
	PackedStringArray init_folder;
	init_folder.push_back(_root_dir);
	_folders_stack.clear();
	_folders_stack.push_back(init_folder);

	_initial_files_count = 0;

	_searching = true;
	set_process(true);
}

void FindInFiles::stop() {
	_searching = false;
	_current_dir = "";
	set_process(false);
}

void FindInFiles::_process() {
	// This part can be moved to a thread if needed.

	OS &os = *OS::get_singleton();
	uint64_t time_before = os.get_ticks_msec();
	while (is_processing()) {
		_iterate();
		uint64_t elapsed = (os.get_ticks_msec() - time_before);
		if (elapsed > 8) { // Process again after waiting 8 ticks.
			break;
		}
	}
}

void FindInFiles::_iterate() {
	if (_folders_stack.size() != 0) {
		// Scan folders first so we can build a list of files and have progress info later.

		PackedStringArray &folders_to_scan = _folders_stack.write[_folders_stack.size() - 1];

		if (folders_to_scan.size() != 0) {
			// Scan one folder below.

			String folder_name = folders_to_scan[folders_to_scan.size() - 1];
			pop_back(folders_to_scan);

			_current_dir = _current_dir.path_join(folder_name);

			PackedStringArray sub_dirs;
			PackedStringArray files_to_scan;
			_scan_dir("res://" + _current_dir, sub_dirs, files_to_scan);

			_folders_stack.push_back(sub_dirs);
			_files_to_scan.append_array(files_to_scan);

		} else {
			// Go back one level.

			pop_back(_folders_stack);
			_current_dir = _current_dir.get_base_dir();

			if (_folders_stack.size() == 0) {
				// All folders scanned.
				_initial_files_count = _files_to_scan.size();
			}
		}

	} else if (_files_to_scan.size() != 0) {
		// Then scan files.

		String fpath = _files_to_scan[_files_to_scan.size() - 1];
		pop_back(_files_to_scan);
		_scan_file(fpath);

	} else {
		print_verbose("Search complete");
		set_process(false);
		_current_dir = "";
		_searching = false;
		emit_signal(SceneStringName(finished));
	}
}

float FindInFiles::get_progress() const {
	if (_initial_files_count != 0) {
		return static_cast<float>(_initial_files_count - _files_to_scan.size()) / static_cast<float>(_initial_files_count);
	}
	return 0;
}

void FindInFiles::_scan_dir(const String &path, PackedStringArray &out_folders, PackedStringArray &out_files_to_scan) {
	Ref<DirAccess> dir = DirAccess::open(path);
	if (dir.is_null()) {
		print_verbose("Cannot open directory! " + path);
		return;
	}

	dir->list_dir_begin();

	// Limit to 100,000 iterations to avoid an infinite loop just in case
	// (this technically limits results to 100,000 files per folder).
	for (int i = 0; i < 100'000; ++i) {
		String file = dir->get_next();

		if (file.is_empty()) {
			break;
		}

		// If there is a .gdignore file in the directory, clear all the files/folders
		// to be searched on this path and skip searching the directory.
		if (file == ".gdignore") {
			out_folders.clear();
			out_files_to_scan.clear();
			break;
		}

		// Ignore special directories (such as those beginning with . and the project data directory).
		String project_data_dir_name = ProjectSettings::get_singleton()->get_project_data_dir_name();
		if (file.begins_with(".") || file == project_data_dir_name) {
			continue;
		}
		if (dir->current_is_hidden()) {
			continue;
		}

		if (dir->current_is_dir()) {
			out_folders.push_back(file);

		} else {
			String file_ext = file.get_extension();
			if (_extension_filter.has(file_ext)) {
				out_files_to_scan.push_back(path.path_join(file));
			}
		}
	}
}

void FindInFiles::_scan_file(const String &fpath) {
	Ref<FileAccess> f = FileAccess::open(fpath, FileAccess::READ);
	if (f.is_null()) {
		print_verbose(String("Cannot open file ") + fpath);
		return;
	}

	int line_number = 0;

	while (!f->eof_reached()) {
		// Line number starts at 1.
		++line_number;

		int begin = 0;
		int end = 0;

		String line = f->get_line();

		while (find_next(line, _pattern, end, _match_case, _whole_words, begin, end)) {
			emit_signal(SNAME(SIGNAL_RESULT_FOUND), fpath, line_number, begin, end, line);
		}
	}
}

void FindInFiles::_bind_methods() {
	ADD_SIGNAL(MethodInfo(SIGNAL_RESULT_FOUND,
			PropertyInfo(Variant::STRING, "path"),
			PropertyInfo(Variant::INT, "line_number"),
			PropertyInfo(Variant::INT, "begin"),
			PropertyInfo(Variant::INT, "end"),
			PropertyInfo(Variant::STRING, "text")));

	ADD_SIGNAL(MethodInfo("finished"));
}

//-----------------------------------------------------------------------------
const char *FindInFilesDialog::SIGNAL_FIND_REQUESTED = "find_requested";
const char *FindInFilesDialog::SIGNAL_REPLACE_REQUESTED = "replace_requested";

FindInFilesDialog::FindInFilesDialog() {
	set_min_size(Size2(500 * EDSCALE, 0));
	set_title(TTR("Find in Files"));

	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->set_anchor_and_offset(SIDE_LEFT, Control::ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_TOP, Control::ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_RIGHT, Control::ANCHOR_END, -8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_BOTTOM, Control::ANCHOR_END, -8 * EDSCALE);
	add_child(vbc);

	GridContainer *gc = memnew(GridContainer);
	gc->set_columns(2);
	vbc->add_child(gc);

	Label *find_label = memnew(Label);
	find_label->set_text(TTR("Find:"));
	gc->add_child(find_label);

	_search_text_line_edit = memnew(LineEdit);
	_search_text_line_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_search_text_line_edit->connect(SceneStringName(text_changed), callable_mp(this, &FindInFilesDialog::_on_search_text_modified));
	_search_text_line_edit->connect(SceneStringName(text_submitted), callable_mp(this, &FindInFilesDialog::_on_search_text_submitted));
	gc->add_child(_search_text_line_edit);

	_replace_label = memnew(Label);
	_replace_label->set_text(TTR("Replace:"));
	_replace_label->hide();
	gc->add_child(_replace_label);

	_replace_text_line_edit = memnew(LineEdit);
	_replace_text_line_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_replace_text_line_edit->connect(SceneStringName(text_submitted), callable_mp(this, &FindInFilesDialog::_on_replace_text_submitted));
	_replace_text_line_edit->hide();
	gc->add_child(_replace_text_line_edit);

	gc->add_child(memnew(Control)); // Space to maintain the grid alignment.

	{
		HBoxContainer *hbc = memnew(HBoxContainer);

		_whole_words_checkbox = memnew(CheckBox);
		_whole_words_checkbox->set_text(TTR("Whole Words"));
		hbc->add_child(_whole_words_checkbox);

		_match_case_checkbox = memnew(CheckBox);
		_match_case_checkbox->set_text(TTR("Match Case"));
		hbc->add_child(_match_case_checkbox);

		gc->add_child(hbc);
	}

	Label *folder_label = memnew(Label);
	folder_label->set_text(TTR("Folder:"));
	gc->add_child(folder_label);

	{
		HBoxContainer *hbc = memnew(HBoxContainer);

		Label *prefix_label = memnew(Label);
		prefix_label->set_text("res://");
		hbc->add_child(prefix_label);

		_folder_line_edit = memnew(LineEdit);
		_folder_line_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hbc->add_child(_folder_line_edit);

		Button *folder_button = memnew(Button);
		folder_button->set_text("...");
		folder_button->connect(SceneStringName(pressed), callable_mp(this, &FindInFilesDialog::_on_folder_button_pressed));
		hbc->add_child(folder_button);

		_folder_dialog = memnew(FileDialog);
		_folder_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_DIR);
		_folder_dialog->connect("dir_selected", callable_mp(this, &FindInFilesDialog::_on_folder_selected));
		add_child(_folder_dialog);

		gc->add_child(hbc);
	}

	Label *filter_label = memnew(Label);
	filter_label->set_text(TTR("Filters:"));
	filter_label->set_tooltip_text(TTR("Include the files with the following extensions. Add or remove them in ProjectSettings."));
	gc->add_child(filter_label);

	_filters_container = memnew(HBoxContainer);
	gc->add_child(_filters_container);

	_find_button = add_button(TTR("Find..."), false, "find");
	_find_button->set_disabled(true);

	_replace_button = add_button(TTR("Replace..."), false, "replace");
	_replace_button->set_disabled(true);

	Button *cancel_button = get_ok_button();
	cancel_button->set_text(TTR("Cancel"));

	_mode = SEARCH_MODE;
}

void FindInFilesDialog::set_search_text(const String &text) {
	if (_mode == SEARCH_MODE) {
		if (!text.is_empty()) {
			_search_text_line_edit->set_text(text);
			_on_search_text_modified(text);
		}
		callable_mp((Control *)_search_text_line_edit, &Control::grab_focus).call_deferred();
		_search_text_line_edit->select_all();
	} else if (_mode == REPLACE_MODE) {
		if (!text.is_empty()) {
			_search_text_line_edit->set_text(text);
			callable_mp((Control *)_replace_text_line_edit, &Control::grab_focus).call_deferred();
			_replace_text_line_edit->select_all();
			_on_search_text_modified(text);
		} else {
			callable_mp((Control *)_search_text_line_edit, &Control::grab_focus).call_deferred();
			_search_text_line_edit->select_all();
		}
	}
}

void FindInFilesDialog::set_replace_text(const String &text) {
	_replace_text_line_edit->set_text(text);
}

void FindInFilesDialog::set_find_in_files_mode(FindInFilesMode p_mode) {
	if (_mode == p_mode) {
		return;
	}

	_mode = p_mode;

	if (p_mode == SEARCH_MODE) {
		set_title(TTR("Find in Files"));
		_replace_label->hide();
		_replace_text_line_edit->hide();
	} else if (p_mode == REPLACE_MODE) {
		set_title(TTR("Replace in Files"));
		_replace_label->show();
		_replace_text_line_edit->show();
	}

	// Recalculate the dialog size after hiding child controls.
	set_size(Size2(get_size().x, 0));
}

String FindInFilesDialog::get_search_text() const {
	return _search_text_line_edit->get_text();
}

String FindInFilesDialog::get_replace_text() const {
	return _replace_text_line_edit->get_text();
}

bool FindInFilesDialog::is_match_case() const {
	return _match_case_checkbox->is_pressed();
}

bool FindInFilesDialog::is_whole_words() const {
	return _whole_words_checkbox->is_pressed();
}

String FindInFilesDialog::get_folder() const {
	String text = _folder_line_edit->get_text();
	return text.strip_edges();
}

HashSet<String> FindInFilesDialog::get_filter() const {
	// Could check the _filters_preferences but it might not have been generated yet.
	HashSet<String> filters;
	for (int i = 0; i < _filters_container->get_child_count(); ++i) {
		CheckBox *cb = static_cast<CheckBox *>(_filters_container->get_child(i));
		if (cb->is_pressed()) {
			filters.insert(cb->get_text());
		}
	}
	return filters;
}

void FindInFilesDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				// Extensions might have changed in the meantime, we clean them and instance them again.
				for (int i = 0; i < _filters_container->get_child_count(); i++) {
					_filters_container->get_child(i)->queue_free();
				}
				Array exts = GLOBAL_GET("editor/script/search_in_file_extensions");
				for (int i = 0; i < exts.size(); ++i) {
					CheckBox *cb = memnew(CheckBox);
					cb->set_text(exts[i]);
					if (!_filters_preferences.has(exts[i])) {
						_filters_preferences[exts[i]] = true;
					}
					cb->set_pressed(_filters_preferences[exts[i]]);
					_filters_container->add_child(cb);
				}
			}
		} break;
	}
}

void FindInFilesDialog::_on_folder_button_pressed() {
	_folder_dialog->popup_file_dialog();
}

void FindInFilesDialog::custom_action(const String &p_action) {
	for (int i = 0; i < _filters_container->get_child_count(); ++i) {
		CheckBox *cb = static_cast<CheckBox *>(_filters_container->get_child(i));
		_filters_preferences[cb->get_text()] = cb->is_pressed();
	}

	if (p_action == "find") {
		emit_signal(SNAME(SIGNAL_FIND_REQUESTED));
		hide();
	} else if (p_action == "replace") {
		emit_signal(SNAME(SIGNAL_REPLACE_REQUESTED));
		hide();
	}
}

void FindInFilesDialog::_on_search_text_modified(const String &text) {
	ERR_FAIL_NULL(_find_button);
	ERR_FAIL_NULL(_replace_button);

	_find_button->set_disabled(get_search_text().is_empty());
	_replace_button->set_disabled(get_search_text().is_empty());
}

void FindInFilesDialog::_on_search_text_submitted(const String &text) {
	// This allows to trigger a global search without leaving the keyboard.
	if (!_find_button->is_disabled()) {
		if (_mode == SEARCH_MODE) {
			custom_action("find");
		}
	}

	if (!_replace_button->is_disabled()) {
		if (_mode == REPLACE_MODE) {
			custom_action("replace");
		}
	}
}

void FindInFilesDialog::_on_replace_text_submitted(const String &text) {
	// This allows to trigger a global search without leaving the keyboard.
	if (!_replace_button->is_disabled()) {
		if (_mode == REPLACE_MODE) {
			custom_action("replace");
		}
	}
}

void FindInFilesDialog::_on_folder_selected(String path) {
	int i = path.find("://");
	if (i != -1) {
		path = path.substr(i + 3);
	}
	_folder_line_edit->set_text(path);
}

void FindInFilesDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo(SIGNAL_FIND_REQUESTED));
	ADD_SIGNAL(MethodInfo(SIGNAL_REPLACE_REQUESTED));
}

//-----------------------------------------------------------------------------
const char *FindInFilesPanel::SIGNAL_RESULT_SELECTED = "result_selected";
const char *FindInFilesPanel::SIGNAL_FILES_MODIFIED = "files_modified";
const char *FindInFilesPanel::SIGNAL_CLOSE_BUTTON_CLICKED = "close_button_clicked";

FindInFilesPanel::FindInFilesPanel() {
	_finder = memnew(FindInFiles);
	_finder->connect(FindInFiles::SIGNAL_RESULT_FOUND, callable_mp(this, &FindInFilesPanel::_on_result_found));
	_finder->connect(SceneStringName(finished), callable_mp(this, &FindInFilesPanel::_on_finished));
	add_child(_finder);

	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->set_anchor_and_offset(SIDE_LEFT, ANCHOR_BEGIN, 0);
	vbc->set_anchor_and_offset(SIDE_TOP, ANCHOR_BEGIN, 0);
	vbc->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, 0);
	vbc->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, 0);
	add_child(vbc);

	{
		HBoxContainer *hbc = memnew(HBoxContainer);

		Label *find_label = memnew(Label);
		find_label->set_text(TTR("Find:"));
		hbc->add_child(find_label);

		_search_text_label = memnew(Label);
		hbc->add_child(_search_text_label);

		_progress_bar = memnew(ProgressBar);
		_progress_bar->set_h_size_flags(SIZE_EXPAND_FILL);
		_progress_bar->set_v_size_flags(SIZE_SHRINK_CENTER);
		hbc->add_child(_progress_bar);
		set_progress_visible(false);

		_status_label = memnew(Label);
		hbc->add_child(_status_label);

		_refresh_button = memnew(Button);
		_refresh_button->set_text(TTR("Refresh"));
		_refresh_button->connect(SceneStringName(pressed), callable_mp(this, &FindInFilesPanel::_on_refresh_button_clicked));
		_refresh_button->hide();
		hbc->add_child(_refresh_button);

		_cancel_button = memnew(Button);
		_cancel_button->set_text(TTR("Cancel"));
		_cancel_button->connect(SceneStringName(pressed), callable_mp(this, &FindInFilesPanel::_on_cancel_button_clicked));
		_cancel_button->hide();
		hbc->add_child(_cancel_button);

		_close_button = memnew(Button);
		_close_button->set_text(TTR("Close"));
		_close_button->connect(SceneStringName(pressed), callable_mp(this, &FindInFilesPanel::_on_close_button_clicked));
		hbc->add_child(_close_button);

		vbc->add_child(hbc);
	}

	_results_display = memnew(Tree);
	_results_display->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	_results_display->set_v_size_flags(SIZE_EXPAND_FILL);
	_results_display->connect(SceneStringName(item_selected), callable_mp(this, &FindInFilesPanel::_on_result_selected));
	_results_display->connect("item_edited", callable_mp(this, &FindInFilesPanel::_on_item_edited));
	_results_display->connect("button_clicked", callable_mp(this, &FindInFilesPanel::_on_button_clicked));
	_results_display->set_hide_root(true);
	_results_display->set_select_mode(Tree::SELECT_ROW);
	_results_display->set_allow_rmb_select(true);
	_results_display->set_allow_reselect(true);
	_results_display->add_theme_constant_override("inner_item_margin_left", 0);
	_results_display->add_theme_constant_override("inner_item_margin_right", 0);
	_results_display->create_item(); // Root
	vbc->add_child(_results_display);

	{
		_replace_container = memnew(HBoxContainer);

		Label *replace_label = memnew(Label);
		replace_label->set_text(TTR("Replace:"));
		_replace_container->add_child(replace_label);

		_replace_line_edit = memnew(LineEdit);
		_replace_line_edit->set_h_size_flags(SIZE_EXPAND_FILL);
		_replace_line_edit->connect(SceneStringName(text_changed), callable_mp(this, &FindInFilesPanel::_on_replace_text_changed));
		_replace_container->add_child(_replace_line_edit);

		_replace_all_button = memnew(Button);
		_replace_all_button->set_text(TTR("Replace all (no undo)"));
		_replace_all_button->connect(SceneStringName(pressed), callable_mp(this, &FindInFilesPanel::_on_replace_all_clicked));
		_replace_container->add_child(_replace_all_button);

		_replace_container->hide();

		vbc->add_child(_replace_container);
	}
}

void FindInFilesPanel::set_with_replace(bool with_replace) {
	_with_replace = with_replace;
	_replace_container->set_visible(with_replace);

	if (with_replace) {
		// Results show checkboxes on their left so they can be opted out.
		_results_display->set_columns(2);
		_results_display->set_column_expand(0, false);
		_results_display->set_column_custom_minimum_width(0, 48 * EDSCALE);
	} else {
		// Results are single-cell items.
		_results_display->set_column_expand(0, true);
		_results_display->set_columns(1);
	}
}

void FindInFilesPanel::set_replace_text(const String &text) {
	_replace_line_edit->set_text(text);
}

void FindInFilesPanel::clear() {
	_file_items.clear();
	_result_items.clear();
	_results_display->clear();
	_results_display->create_item(); // Root
}

void FindInFilesPanel::start_search() {
	clear();

	_status_label->set_text(TTR("Searching..."));
	_search_text_label->set_text(_finder->get_search_text());

	set_process(true);
	set_progress_visible(true);

	_finder->start();

	update_replace_buttons();
	_refresh_button->hide();
	_cancel_button->show();
}

void FindInFilesPanel::stop_search() {
	_finder->stop();

	_status_label->set_text("");
	update_replace_buttons();
	set_progress_visible(false);
	_refresh_button->show();
	_cancel_button->hide();
}

void FindInFilesPanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			_search_text_label->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("source"), EditorStringName(EditorFonts)));
			_search_text_label->add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size(SNAME("source_size"), EditorStringName(EditorFonts)));
			_results_display->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("source"), EditorStringName(EditorFonts)));
			_results_display->add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size(SNAME("source_size"), EditorStringName(EditorFonts)));

			// Rebuild search tree.
			if (!_finder->get_search_text().is_empty()) {
				start_search();
			}
		} break;

		case NOTIFICATION_PROCESS: {
			_progress_bar->set_as_ratio(_finder->get_progress());
		} break;
	}
}

void FindInFilesPanel::_on_result_found(const String &fpath, int line_number, int begin, int end, String text) {
	TreeItem *file_item;
	Ref<Texture2D> remove_texture = get_editor_theme_icon(SNAME("Close"));

	HashMap<String, TreeItem *>::Iterator E = _file_items.find(fpath);
	if (!E) {
		file_item = _results_display->create_item();
		file_item->set_text(0, fpath);
		file_item->set_metadata(0, fpath);
		file_item->add_button(0, remove_texture, -1, false, TTR("Remove result"));

		// The width of this column is restrained to checkboxes,
		// but that doesn't make sense for the parent items,
		// so we override their width so they can expand to full width.
		file_item->set_expand_right(0, true);

		_file_items[fpath] = file_item;
	} else {
		file_item = E->value;
	}

	Color file_item_color = _results_display->get_theme_color(SceneStringName(font_color)) * Color(1, 1, 1, 0.67);
	file_item->set_custom_color(0, file_item_color);
	file_item->set_selectable(0, false);

	int text_index = _with_replace ? 1 : 0;

	TreeItem *item = _results_display->create_item(file_item);

	// Do this first because it resets properties of the cell...
	item->set_cell_mode(text_index, TreeItem::CELL_MODE_CUSTOM);

	// Trim result item line.
	int old_text_size = text.size();
	text = text.strip_edges(true, false);
	int chars_removed = old_text_size - text.size();
	String start = vformat("%3s: ", line_number);

	item->set_text(text_index, start + text);
	item->set_custom_draw_callback(text_index, callable_mp(this, &FindInFilesPanel::draw_result_text));

	Result r;
	r.line_number = line_number;
	r.begin = begin;
	r.end = end;
	r.begin_trimmed = begin - chars_removed + start.size() - 1;
	_result_items[item] = r;

	if (_with_replace) {
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_checked(0, true);
		item->set_editable(0, true);
		item->add_button(1, remove_texture, -1, false, TTR("Remove result"));
	} else {
		item->add_button(0, remove_texture, -1, false, TTR("Remove result"));
	}
}

void FindInFilesPanel::draw_result_text(Object *item_obj, Rect2 rect) {
	TreeItem *item = Object::cast_to<TreeItem>(item_obj);
	if (!item) {
		return;
	}

	HashMap<TreeItem *, Result>::Iterator E = _result_items.find(item);
	if (!E) {
		return;
	}
	Result r = E->value;
	String item_text = item->get_text(_with_replace ? 1 : 0);
	Ref<Font> font = _results_display->get_theme_font(SceneStringName(font));
	int font_size = _results_display->get_theme_font_size(SceneStringName(font_size));

	Rect2 match_rect = rect;
	match_rect.position.x += font->get_string_size(item_text.left(r.begin_trimmed), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x - 1;
	match_rect.size.x = font->get_string_size(_search_text_label->get_text(), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x + 1;
	match_rect.position.y += 1 * EDSCALE;
	match_rect.size.y -= 2 * EDSCALE;

	_results_display->draw_rect(match_rect, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.33), false, 2.0);
	_results_display->draw_rect(match_rect, get_theme_color(SNAME("accent_color"), EditorStringName(Editor)) * Color(1, 1, 1, 0.17), true);

	// Text is drawn by Tree already.
}

void FindInFilesPanel::_on_item_edited() {
	TreeItem *item = _results_display->get_selected();

	// Change opacity to half if checkbox is checked, otherwise full.
	Color use_color = _results_display->get_theme_color(SceneStringName(font_color));
	if (!item->is_checked(0)) {
		use_color.a *= 0.5;
	}
	item->set_custom_color(1, use_color);
}

void FindInFilesPanel::_on_finished() {
	update_matches_text();
	update_replace_buttons();
	set_progress_visible(false);
	_refresh_button->show();
	_cancel_button->hide();
}

void FindInFilesPanel::_on_refresh_button_clicked() {
	start_search();
}

void FindInFilesPanel::_on_cancel_button_clicked() {
	stop_search();
}

void FindInFilesPanel::_on_close_button_clicked() {
	emit_signal(SNAME(SIGNAL_CLOSE_BUTTON_CLICKED));
}

void FindInFilesPanel::_on_result_selected() {
	TreeItem *item = _results_display->get_selected();
	HashMap<TreeItem *, Result>::Iterator E = _result_items.find(item);

	if (!E) {
		return;
	}
	Result r = E->value;

	TreeItem *file_item = item->get_parent();
	String fpath = file_item->get_metadata(0);

	emit_signal(SNAME(SIGNAL_RESULT_SELECTED), fpath, r.line_number, r.begin, r.end);
}

void FindInFilesPanel::_on_replace_text_changed(const String &text) {
	update_replace_buttons();
}

void FindInFilesPanel::_on_replace_all_clicked() {
	String replace_text = get_replace_text();

	PackedStringArray modified_files;

	for (KeyValue<String, TreeItem *> &E : _file_items) {
		TreeItem *file_item = E.value;
		String fpath = file_item->get_metadata(0);

		Vector<Result> locations;
		for (TreeItem *item = file_item->get_first_child(); item; item = item->get_next()) {
			if (!item->is_checked(0)) {
				continue;
			}

			HashMap<TreeItem *, Result>::Iterator F = _result_items.find(item);
			ERR_FAIL_COND(!F);
			locations.push_back(F->value);
		}

		if (locations.size() != 0) {
			// Results are sorted by file, so we can batch replaces.
			apply_replaces_in_file(fpath, locations, replace_text);
			modified_files.push_back(fpath);
		}
	}

	// Hide replace bar so we can't trigger the action twice without doing a new search.
	_replace_container->hide();

	emit_signal(SNAME(SIGNAL_FILES_MODIFIED), modified_files);
}

void FindInFilesPanel::_on_button_clicked(TreeItem *p_item, int p_column, int p_id, int p_mouse_button_index) {
	const String file_path = p_item->get_text(0);

	_result_items.erase(p_item);
	if (_file_items.find(file_path)) {
		TreeItem *file_result = _file_items.get(file_path);
		int match_count = file_result->get_child_count();

		for (int i = 0; i < match_count; i++) {
			TreeItem *child_item = file_result->get_child(i);
			_result_items.erase(child_item);
		}

		file_result->clear_children();
		_file_items.erase(file_path);
	}

	TreeItem *item_parent = p_item->get_parent();
	if (item_parent && item_parent->get_child_count() < 2) {
		_file_items.erase(item_parent->get_text(0));
		get_tree()->queue_delete(item_parent);
	}
	get_tree()->queue_delete(p_item);
	update_matches_text();
}

// Same as get_line, but preserves line ending characters.
class ConservativeGetLine {
public:
	String get_line(Ref<FileAccess> f) {
		_line_buffer.clear();

		char32_t c = f->get_8();

		while (!f->eof_reached()) {
			if (c == '\n') {
				_line_buffer.push_back(c);
				_line_buffer.push_back(0);
				return String::utf8(_line_buffer.ptr());

			} else if (c == '\0') {
				_line_buffer.push_back(c);
				return String::utf8(_line_buffer.ptr());

			} else if (c != '\r') {
				_line_buffer.push_back(c);
			}

			c = f->get_8();
		}

		_line_buffer.push_back(0);
		return String::utf8(_line_buffer.ptr());
	}

private:
	Vector<char> _line_buffer;
};

void FindInFilesPanel::apply_replaces_in_file(const String &fpath, const Vector<Result> &locations, const String &new_text) {
	// If the file is already open, I assume the editor will reload it.
	// If there are unsaved changes, the user will be asked on focus,
	// however that means either losing changes or losing replaces.

	Ref<FileAccess> f = FileAccess::open(fpath, FileAccess::READ);
	ERR_FAIL_COND_MSG(f.is_null(), "Cannot open file from path '" + fpath + "'.");

	String buffer;
	int current_line = 1;

	ConservativeGetLine conservative;

	String line = conservative.get_line(f);
	String search_text = _finder->get_search_text();

	int offset = 0;

	for (int i = 0; i < locations.size(); ++i) {
		int repl_line_number = locations[i].line_number;

		while (current_line < repl_line_number) {
			buffer += line;
			line = conservative.get_line(f);
			++current_line;
			offset = 0;
		}

		int repl_begin = locations[i].begin + offset;
		int repl_end = locations[i].end + offset;

		int _;
		if (!find_next(line, search_text, repl_begin, _finder->is_match_case(), _finder->is_whole_words(), _, _)) {
			// Make sure the replace is still valid in case the file was tampered with.
			print_verbose(String("Occurrence no longer matches, replace will be ignored in {0}: line {1}, col {2}").format(varray(fpath, repl_line_number, repl_begin)));
			continue;
		}

		line = line.left(repl_begin) + new_text + line.substr(repl_end);
		// Keep an offset in case there are successive replaces in the same line.
		offset += new_text.length() - (repl_end - repl_begin);
	}

	buffer += line;

	while (!f->eof_reached()) {
		buffer += conservative.get_line(f);
	}

	// Now the modified contents are in the buffer, rewrite the file with our changes.

	Error err = f->reopen(fpath, FileAccess::WRITE);
	ERR_FAIL_COND_MSG(err != OK, "Cannot create file in path '" + fpath + "'.");

	f->store_string(buffer);
}

String FindInFilesPanel::get_replace_text() {
	return _replace_line_edit->get_text();
}

void FindInFilesPanel::update_replace_buttons() {
	bool disabled = _finder->is_searching();

	_replace_all_button->set_disabled(disabled);
}

void FindInFilesPanel::update_matches_text() {
	String results_text;
	int result_count = _result_items.size();
	int file_count = _file_items.size();

	if (result_count == 1 && file_count == 1) {
		results_text = vformat(TTR("%d match in %d file"), result_count, file_count);
	} else if (result_count != 1 && file_count == 1) {
		results_text = vformat(TTR("%d matches in %d file"), result_count, file_count);
	} else {
		results_text = vformat(TTR("%d matches in %d files"), result_count, file_count);
	}

	_status_label->set_text(results_text);
}

void FindInFilesPanel::set_progress_visible(bool p_visible) {
	_progress_bar->set_self_modulate(Color(1, 1, 1, p_visible ? 1 : 0));
}

void FindInFilesPanel::_bind_methods() {
	ClassDB::bind_method("_on_result_found", &FindInFilesPanel::_on_result_found);
	ClassDB::bind_method("_on_finished", &FindInFilesPanel::_on_finished);

	ADD_SIGNAL(MethodInfo(SIGNAL_RESULT_SELECTED,
			PropertyInfo(Variant::STRING, "path"),
			PropertyInfo(Variant::INT, "line_number"),
			PropertyInfo(Variant::INT, "begin"),
			PropertyInfo(Variant::INT, "end")));

	ADD_SIGNAL(MethodInfo(SIGNAL_FILES_MODIFIED, PropertyInfo(Variant::STRING, "paths")));

	ADD_SIGNAL(MethodInfo(SIGNAL_CLOSE_BUTTON_CLICKED));
}
