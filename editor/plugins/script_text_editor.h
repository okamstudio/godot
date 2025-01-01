/**************************************************************************/
/*  script_text_editor.h                                                  */
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

#ifndef SCRIPT_TEXT_EDITOR_H
#define SCRIPT_TEXT_EDITOR_H

#include "script_editor_plugin.h"

#include "editor/code_editor.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tree.h"

class RichTextLabel;

class ConnectionInfoDialog : public AcceptDialog {
	GDCLASS(ConnectionInfoDialog, AcceptDialog);

	Label *method = nullptr;
	Tree *tree = nullptr;

	virtual void ok_pressed() override;

public:
	void popup_connections(const String &p_method, const Vector<Node *> &p_nodes);

	ConnectionInfoDialog();
};

class ScriptTextEditor : public ScriptEditorBase {
	GDCLASS(ScriptTextEditor, ScriptEditorBase);

	CodeTextEditor *code_editor = nullptr;
	RichTextLabel *warnings_panel = nullptr;
	RichTextLabel *errors_panel = nullptr;

	Ref<Script> script;
	bool script_is_valid = false;
	bool editor_enabled = false;

	Vector<String> functions;
	List<ScriptLanguage::Warning> warnings;
	List<ScriptLanguage::ScriptError> errors;
	HashMap<String, List<ScriptLanguage::ScriptError>> depended_errors;
	HashSet<int> safe_lines;

	List<Connection> missing_connections;

	Vector<String> member_keywords;

	HBoxContainer *edit_hb = nullptr;

	MenuButton *edit_menu = nullptr;
	MenuButton *search_menu = nullptr;
	MenuButton *goto_menu = nullptr;
	PopupMenu *bookmarks_menu = nullptr;
	PopupMenu *breakpoints_menu = nullptr;
	PopupMenu *highlighter_menu = nullptr;
	PopupMenu *context_menu = nullptr;

	GotoLinePopup *goto_line_popup = nullptr;
	ScriptEditorQuickOpen *quick_open = nullptr;
	ConnectionInfoDialog *connection_info_dialog = nullptr;

	int connection_gutter = -1;
	void _gutter_clicked(int p_line, int p_gutter);
	void _update_gutter_indexes();

	int line_number_gutter = -1;
	Color default_line_number_color = Color(1, 1, 1);
	Color safe_line_number_color = Color(1, 1, 1);

	Color marked_line_color = Color(1, 1, 1);
	Color folded_code_region_color = Color(1, 1, 1);
	int previous_line = 0;

	PopupPanel *color_panel = nullptr;
	ColorPicker *color_picker = nullptr;
	Vector2 color_position;
	String color_args;

	bool theme_loaded = false;

	enum {
		EDIT_UNDO,
		EDIT_REDO,
		EDIT_CUT,
		EDIT_COPY,
		EDIT_PASTE,
		EDIT_SELECT_ALL,
		EDIT_COMPLETE,
		EDIT_AUTO_INDENT,
		EDIT_TRIM_TRAILING_WHITESAPCE,
		EDIT_TRIM_FINAL_NEWLINES,
		EDIT_CONVERT_INDENT_TO_SPACES,
		EDIT_CONVERT_INDENT_TO_TABS,
		EDIT_TOGGLE_COMMENT,
		EDIT_MOVE_LINE_UP,
		EDIT_MOVE_LINE_DOWN,
		EDIT_INDENT,
		EDIT_UNINDENT,
		EDIT_DELETE_LINE,
		EDIT_DUPLICATE_SELECTION,
		EDIT_DUPLICATE_LINES,
		EDIT_PICK_COLOR,
		EDIT_TO_UPPERCASE,
		EDIT_TO_LOWERCASE,
		EDIT_CAPITALIZE,
		EDIT_EVALUATE,
		EDIT_TOGGLE_WORD_WRAP,
		EDIT_TOGGLE_FOLD_LINE,
		EDIT_FOLD_ALL_LINES,
		EDIT_CREATE_CODE_REGION,
		EDIT_UNFOLD_ALL_LINES,
		SEARCH_FIND,
		SEARCH_FIND_NEXT,
		SEARCH_FIND_PREV,
		SEARCH_REPLACE,
		SEARCH_LOCATE_FUNCTION,
		SEARCH_GOTO_LINE,
		SEARCH_IN_FILES,
		REPLACE_IN_FILES,
		BOOKMARK_TOGGLE,
		BOOKMARK_GOTO_NEXT,
		BOOKMARK_GOTO_PREV,
		BOOKMARK_REMOVE_ALL,
		DEBUG_TOGGLE_BREAKPOINT,
		DEBUG_REMOVE_ALL_BREAKPOINTS,
		DEBUG_GOTO_NEXT_BREAKPOINT,
		DEBUG_GOTO_PREV_BREAKPOINT,
		HELP_CONTEXTUAL,
		LOOKUP_SYMBOL,
	};

	void _enable_code_editor();

protected:
	void _update_breakpoint_list();
	void _breakpoint_item_pressed(int p_idx);
	void _breakpoint_toggled(int p_row);

	void _on_caret_moved();

	void _validate_script(); // No longer virtual.
	void _update_warnings();
	void _update_errors();
	void _update_bookmark_list();
	void _bookmark_item_pressed(int p_idx);

	static void _code_complete_scripts(void *p_ud, const String &p_code, List<ScriptLanguage::CodeCompletionOption> *r_options, bool &r_force);
	void _code_complete_script(const String &p_code, List<ScriptLanguage::CodeCompletionOption> *r_options, bool &r_force);

	void _load_theme_settings();
	void _set_theme_for_script();
	void _show_errors_panel(bool p_show);
	void _show_warnings_panel(bool p_show);
	void _error_clicked(const Variant &p_line);
	void _warning_clicked(const Variant &p_line);

	void _notification(int p_what);

	HashMap<String, Ref<EditorSyntaxHighlighter>> highlighters;
	void _change_syntax_highlighter(int p_idx);

	void _edit_option(int p_op);
	void _edit_option_toggle_inline_comment();
	void _make_context_menu(bool p_selection, bool p_color, bool p_foldable, bool p_open_docs, bool p_goto_definition, Vector2 p_pos);
	void _text_edit_gui_input(const Ref<InputEvent> &ev);
	void _color_changed(const Color &p_color);
	void _prepare_edit_menu();

	void _goto_line(int p_line) { goto_line(p_line); }
	void _lookup_symbol(const String &p_symbol, int p_row, int p_column);
	void _validate_symbol(const String &p_symbol);

	void _show_symbol_tooltip(const String &p_symbol, int p_row, int p_column);

	void _convert_case(CodeTextEditor::CaseStyle p_case);

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	String _get_absolute_path(const String &rel_path);

public:
	void _update_connected_methods();

	virtual void add_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) override;
	virtual void set_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) override;
	void update_toggle_scripts_button() override;

	virtual void apply_code() override;
	virtual Ref<Resource> get_edited_resource() const override;
	virtual void set_edited_resource(const Ref<Resource> &p_res) override;
	virtual void enable_editor(Control *p_shortcut_context = nullptr) override;
	virtual Vector<String> get_functions() override;
	virtual void reload_text() override;
	virtual String get_name() override;
	virtual Ref<Texture2D> get_theme_icon() override;
	virtual bool is_unsaved() override;
	virtual Variant get_edit_state() override;
	virtual void set_edit_state(const Variant &p_state) override;
	virtual Variant get_navigation_state() override;
	virtual void ensure_focus() override;
	virtual void trim_trailing_whitespace() override;
	virtual void trim_final_newlines() override;
	virtual void insert_final_newline() override;
	virtual void convert_indent() override;
	virtual void tag_saved_version() override;

	virtual void goto_line(int p_line, int p_column = 0) override;
	void goto_line_selection(int p_line, int p_begin, int p_end);
	void goto_line_centered(int p_line, int p_column = 0);
	virtual void set_executing_line(int p_line) override;
	virtual void clear_executing_line() override;

	virtual void reload(bool p_soft) override;
	virtual PackedInt32Array get_breakpoints() override;
	virtual void set_breakpoint(int p_line, bool p_enabled) override;
	virtual void clear_breakpoints() override;

	virtual void add_callback(const String &p_function, const PackedStringArray &p_args) override;
	virtual void update_settings() override;

	virtual bool show_members_overview() override;

	virtual void set_tooltip_request_func(const Callable &p_toolip_callback) override;

	virtual void set_debugger_active(bool p_active) override;

	Control *get_edit_menu() override;
	virtual void clear_edit_menu() override;
	virtual void set_find_replace_bar(FindReplaceBar *p_bar) override;

	static void register_editor();

	virtual Control *get_base_editor() const override;
	virtual CodeTextEditor *get_code_editor() const override;

	virtual void validate() override;

	Variant get_previous_state();
	void store_previous_state();

	ScriptTextEditor();
	~ScriptTextEditor();
};

#endif // SCRIPT_TEXT_EDITOR_H
