/**************************************************************************/
/*  code_editor.h                                                         */
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

#ifndef CODE_EDITOR_H
#define CODE_EDITOR_H

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/code_edit.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/popup.h"
#include "scene/main/timer.h"

class MenuButton;
class CodeTextEditor;
class LineEdit;

class GotoLinePopup : public PopupPanel {
	GDCLASS(GotoLinePopup, PopupPanel);

	Variant original_state;
	LineEdit *line_input = nullptr;
	CodeTextEditor *text_editor = nullptr;

	void _goto_line();
	void _submit();

protected:
	void _notification(int p_what);
	virtual void _input_from_window(const Ref<InputEvent> &p_event) override;

public:
	void popup_find_line(CodeTextEditor *p_text_editor);

	GotoLinePopup();
};

class FindReplaceBar : public HBoxContainer {
	GDCLASS(FindReplaceBar, HBoxContainer);

	enum SearchMode {
		SEARCH_CURRENT,
		SEARCH_NEXT,
		SEARCH_PREV,
	};

	Button *toggle_replace_button = nullptr;
	LineEdit *search_text = nullptr;
	Label *matches_label = nullptr;
	Button *find_prev = nullptr;
	Button *find_next = nullptr;
	CheckBox *case_sensitive = nullptr;
	CheckBox *whole_words = nullptr;
	TextureButton *hide_button = nullptr;

	LineEdit *replace_text = nullptr;
	Button *replace = nullptr;
	Button *replace_all = nullptr;
	CheckBox *selection_only = nullptr;

	VBoxContainer *vbc_lineedit = nullptr;
	HBoxContainer *hbc_button_replace = nullptr;
	HBoxContainer *hbc_option_replace = nullptr;

	CodeTextEditor *base_text_editor = nullptr;
	CodeEdit *text_editor = nullptr;

	uint32_t flags = 0;

	int result_line = 0;
	int result_col = 0;
	int results_count = -1;
	int results_count_to_current = -1;

	bool replace_all_mode = false;
	bool preserve_cursor = false;

	void _get_search_from(int &r_line, int &r_col, SearchMode p_search_mode);
	void _update_results_count();
	void _update_matches_display();

	void _show_search(bool p_with_replace, bool p_show_only);
	void _hide_bar(bool p_force_focus = false);
	void _update_toggle_replace_button(bool p_replace_visible);

	void _editor_text_changed();
	void _search_options_changed(bool p_pressed);
	void _search_text_changed(const String &p_text);
	void _search_text_submitted(const String &p_text);
	void _replace_text_submitted(const String &p_text);
	void _toggle_replace_pressed();

protected:
	void _notification(int p_what);
	virtual void unhandled_input(const Ref<InputEvent> &p_event) override;
	void _focus_lost();

	void _update_flags(bool p_direction_backwards);

	bool _search(uint32_t p_flags, int p_from_line, int p_from_col);

	void _replace();
	void _replace_all();

	static void _bind_methods();

public:
	String get_search_text() const;
	String get_replace_text() const;

	bool is_case_sensitive() const;
	bool is_whole_words() const;
	bool is_selection_only() const;
	void set_error(const String &p_label);

	void set_text_edit(CodeTextEditor *p_text_editor);

	void popup_search(bool p_show_only = false);
	void popup_replace();

	bool search_current();
	bool search_prev();
	bool search_next();

	bool needs_to_count_results = true;
	bool line_col_changed_for_result = false;

	FindReplaceBar();
};

typedef void (*CodeTextEditorCodeCompleteFunc)(void *p_ud, const String &p_code, List<ScriptLanguage::CodeCompletionOption> *r_options, bool &r_forced);

class CodeTextEditor : public VBoxContainer {
	GDCLASS(CodeTextEditor, VBoxContainer);

	CodeEdit *text_editor = nullptr;
	FindReplaceBar *find_replace_bar = nullptr;
	HBoxContainer *status_bar = nullptr;

	Button *toggle_scripts_button = nullptr;
	Control *toggle_scripts_list = nullptr;
	Button *error_button = nullptr;
	Button *warning_button = nullptr;

	MenuButton *zoom_button = nullptr;
	Label *line_and_col_txt = nullptr;
	Label *indentation_txt = nullptr;

	Label *info = nullptr;
	Timer *idle = nullptr;
	float idle_time = 0.0f;
	float idle_time_with_errors = 0.0f;
	bool code_complete_enabled = true;
	Timer *code_complete_timer = nullptr;
	int code_complete_timer_line = 0;

	float zoom_factor = 1.0f;

	Label *error = nullptr;
	int error_line;
	int error_column;

	bool preview_navigation_change = false;
	Dictionary previous_state;

	void _update_text_editor_theme();
	void _update_font_ligatures();
	void _complete_request();
	Ref<Texture2D> _get_completion_icon(const ScriptLanguage::CodeCompletionOption &p_option);

	virtual void input(const Ref<InputEvent> &event) override;
	void _text_editor_gui_input(const Ref<InputEvent> &p_event);

	Color completion_font_color;
	Color completion_string_color;
	Color completion_string_name_color;
	Color completion_node_path_color;
	Color completion_comment_color;
	Color completion_doc_comment_color;
	CodeTextEditorCodeCompleteFunc code_complete_func;
	void *code_complete_ud = nullptr;

	void _zoom_in();
	void _zoom_out();
	void _zoom_to(float p_zoom_factor);

	void _error_button_pressed();
	void _warning_button_pressed();
	void _set_show_errors_panel(bool p_show);
	void _set_show_warnings_panel(bool p_show);
	void _error_pressed(const Ref<InputEvent> &p_event);

	void _zoom_popup_id_pressed(int p_idx);

	void _toggle_scripts_pressed();

protected:
	virtual void _load_theme_settings() {}
	virtual void _validate_script() {}
	virtual void _code_complete_script(const String &p_code, List<ScriptLanguage::CodeCompletionOption> *r_options) {}

	void _text_changed_idle_timeout();
	void _code_complete_timer_timeout();
	void _text_changed();
	void _line_col_changed();
	void _notification(int);
	static void _bind_methods();

	bool is_warnings_panel_opened = false;
	bool is_errors_panel_opened = false;

public:
	void trim_trailing_whitespace();
	void trim_final_newlines();
	void insert_final_newline();

	enum CaseStyle {
		UPPER,
		LOWER,
		CAPITALIZE,
	};
	void convert_case(CaseStyle p_case);

	void set_indent_using_spaces(bool p_use_spaces);

	/// Toggle inline comment on currently selected lines, or on current line if nothing is selected,
	/// by adding or removing comment delimiter
	void toggle_inline_comment(const String &delimiter);

	void goto_line(int p_line, int p_column = 0);
	void goto_line_selection(int p_line, int p_begin, int p_end);
	void goto_line_centered(int p_line, int p_column = 0);
	void set_executing_line(int p_line);
	void clear_executing_line();

	Variant get_edit_state();
	void set_edit_state(const Variant &p_state);
	Variant get_navigation_state();
	Variant get_previous_state();
	void store_previous_state();

	bool is_previewing_navigation_change() const;
	void set_preview_navigation_change(bool p_preview);

	void set_error_count(int p_error_count);
	void set_warning_count(int p_warning_count);

	void update_editor_settings();
	void set_error(const String &p_error);
	void set_error_pos(int p_line, int p_column);
	Point2i get_error_pos() const;
	void update_line_and_column() { _line_col_changed(); }
	CodeEdit *get_text_editor() { return text_editor; }
	FindReplaceBar *get_find_replace_bar() { return find_replace_bar; }
	void set_find_replace_bar(FindReplaceBar *p_bar);
	void remove_find_replace_bar();
	virtual void apply_code() {}
	virtual void goto_error();

	void toggle_bookmark();
	void goto_next_bookmark();
	void goto_prev_bookmark();
	void remove_all_bookmarks();

	void set_zoom_factor(float p_zoom_factor);
	float get_zoom_factor();

	void set_code_complete_func(CodeTextEditorCodeCompleteFunc p_code_complete_func, void *p_ud);

	void validate_script();

	void set_toggle_list_control(Control *p_control);
	void show_toggle_scripts_button();
	void update_toggle_scripts_button();

	CodeTextEditor();
};

#endif // CODE_EDITOR_H
