/**************************************************************************/
/*  find_in_files.h                                                       */
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

#include "core/templates/hash_map.h"
#include "scene/gui/dialogs.h"

// Performs the actual search
class FindInFiles : public Node {
	GDCLASS(FindInFiles, Node);

public:
	static const char *SIGNAL_RESULT_FOUND;
	static const char *SIGNAL_FINISHED;

	void set_search_text(const String &p_pattern);
	void set_whole_words(bool p_whole_word);
	void set_match_case(bool p_match_case);
	void set_folder(const String &folder);
	void set_filter(const HashSet<String> &exts);

	String get_search_text() const { return _pattern; }

	bool is_whole_words() const { return _whole_words; }
	bool is_match_case() const { return _match_case; }

	void start();
	void stop();

	bool is_searching() const { return _searching; }
	float get_progress() const;

protected:
	void _notification(int p_what);

	static void _bind_methods();

private:
	void _process();
	void _iterate();
	void _scan_dir(const String &path, PackedStringArray &out_folders, PackedStringArray &out_files_to_scan);
	void _scan_file(const String &fpath);

	// Config
	String _pattern;
	HashSet<String> _extension_filter;
	String _root_dir;
	bool _whole_words = true;
	bool _match_case = true;

	// State
	bool _searching = false;
	String _current_dir;
	Vector<PackedStringArray> _folders_stack;
	Vector<String> _files_to_scan;
	int _initial_files_count = 0;
};

class LineEdit;
class CheckBox;
class FileDialog;
class HBoxContainer;

// Prompts search parameters
class FindInFilesDialog : public AcceptDialog {
	GDCLASS(FindInFilesDialog, AcceptDialog);

public:
	enum FindInFilesMode {
		SEARCH_MODE,
		REPLACE_MODE
	};

	static const char *SIGNAL_FIND_REQUESTED;
	static const char *SIGNAL_REPLACE_REQUESTED;

	FindInFilesDialog();

	void set_search_text(const String &text);
	void set_replace_text(const String &text);

	void set_find_in_files_mode(FindInFilesMode p_mode);

	String get_search_text() const;
	String get_replace_text() const;
	bool is_match_case() const;
	bool is_whole_words() const;
	String get_folder() const;
	HashSet<String> get_filter() const;

protected:
	void _notification(int p_what);

	void _visibility_changed();
	void custom_action(const String &p_action) override;
	static void _bind_methods();

private:
	void _on_folder_button_pressed();
	void _on_folder_selected(String path);
	void _on_search_text_modified(const String &text);
	void _on_search_text_submitted(const String &text);
	void _on_replace_text_submitted(const String &text);

	FindInFilesMode _mode;
	LineEdit *_search_text_line_edit = nullptr;

	Label *_replace_label = nullptr;
	LineEdit *_replace_text_line_edit = nullptr;

	LineEdit *_folder_line_edit = nullptr;
	CheckBox *_match_case_checkbox = nullptr;
	CheckBox *_whole_words_checkbox = nullptr;
	Button *_find_button = nullptr;
	Button *_replace_button = nullptr;
	FileDialog *_folder_dialog = nullptr;
	HBoxContainer *_filters_container = nullptr;
	HashMap<String, bool> _filters_preferences;
};

class Button;
class Tree;
class TreeItem;
class ProgressBar;

// Display search results
class FindInFilesPanel : public Control {
	GDCLASS(FindInFilesPanel, Control);

public:
	static const char *SIGNAL_RESULT_SELECTED;
	static const char *SIGNAL_FILES_MODIFIED;
	static const char *SIGNAL_CLOSE_BUTTON_CLICKED;

	FindInFilesPanel();

	FindInFiles *get_finder() const { return _finder; }

	void set_with_replace(bool with_replace);
	void set_replace_text(const String &text);

	void start_search();
	void stop_search();

protected:
	static void _bind_methods();

	void _notification(int p_what);

private:
	void _on_button_clicked(TreeItem *p_item, int p_column, int p_id, int p_mouse_button_index);
	void _on_result_found(const String &fpath, int line_number, int begin, int end, String text);
	void _on_finished();
	void _on_refresh_button_clicked();
	void _on_cancel_button_clicked();
	void _on_close_button_clicked();
	void _on_result_selected();
	void _on_item_edited();
	void _on_replace_text_changed(const String &text);
	void _on_replace_all_clicked();

	struct Result {
		int line_number = 0;
		int begin = 0;
		int end = 0;
		int begin_trimmed = 0;
	};

	void apply_replaces_in_file(const String &fpath, const Vector<Result> &locations, const String &new_text);
	void update_replace_buttons();
	void update_matches_text();
	String get_replace_text();

	void draw_result_text(Object *item_obj, Rect2 rect);

	void set_progress_visible(bool p_visible);
	void clear();

	FindInFiles *_finder = nullptr;
	Label *_search_text_label = nullptr;
	Tree *_results_display = nullptr;
	Label *_status_label = nullptr;
	Button *_refresh_button = nullptr;
	Button *_cancel_button = nullptr;
	Button *_close_button = nullptr;
	ProgressBar *_progress_bar = nullptr;
	HashMap<String, TreeItem *> _file_items;
	HashMap<TreeItem *, Result> _result_items;
	bool _with_replace = false;

	HBoxContainer *_replace_container = nullptr;
	LineEdit *_replace_line_edit = nullptr;
	Button *_replace_all_button = nullptr;
};
