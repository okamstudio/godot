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

#include "scene/gui/color_picker.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tree.h"
#include "script_editor_plugin.h"

class ConnectionInfoDialog : public AcceptDialog {
	GDCLASS(ConnectionInfoDialog, AcceptDialog);

	Label *method;
	Tree *tree;

	virtual void ok_pressed();

public:
	void popup_connections(String p_method, Vector<Node *> p_nodes);

	ConnectionInfoDialog();
};

class ScriptTextEditor : public ScriptEditorBase {
	GDCLASS(ScriptTextEditor, ScriptEditorBase);

	CodeTextEditor *code_editor;
	RichTextLabel *warnings_panel;

	Ref<Script> script;
	bool script_is_valid;
	bool editor_enabled;

	Vector<String> functions;

	List<Connection> missing_connections;

	Vector<String> member_keywords;

	HBoxContainer *edit_hb;

	MenuButton *edit_menu;
	MenuButton *search_menu;
	MenuButton *goto_menu;
	PopupMenu *bookmarks_menu;
	PopupMenu *breakpoints_menu;
	PopupMenu *highlighter_menu;
	PopupMenu *context_menu;
	PopupMenu *convert_case;

	GotoLineDialog *goto_line_dialog;
	ScriptEditorQuickOpen *quick_open;
	ConnectionInfoDialog *connection_info_dialog;

	PopupPanel *color_panel;
	ColorPicker *color_picker;
	Vector2 color_position;
	String color_args;

	void _update_member_keywords();

	struct ColorsCache {
		Color symbol_color;
		Color keyword_color;
		Color control_flow_keyword_color;
		Color basetype_color;
		Color type_color;
		Color usertype_color;
		Color comment_color;
		Color string_color;
	} colors_cache;

	bool theme_loaded;

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
		EDIT_CONVERT_INDENT_TO_SPACES,
		EDIT_CONVERT_INDENT_TO_TABS,
		EDIT_TOGGLE_COMMENT,
		EDIT_MOVE_LINE_UP,
		EDIT_MOVE_LINE_DOWN,
		EDIT_INDENT_RIGHT,
		EDIT_INDENT_LEFT,
		EDIT_DELETE_LINE,
		EDIT_DUPLICATE_SELECTION,
		EDIT_PICK_COLOR,
		EDIT_TO_UPPERCASE,
		EDIT_TO_LOWERCASE,
		EDIT_CAPITALIZE,
		EDIT_EVALUATE,
		EDIT_TOGGLE_FOLD_LINE,
		EDIT_FOLD_ALL_LINES,
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

	void _validate_script(); // No longer virtual.
	void _update_bookmark_list();
	void _bookmark_item_pressed(int p_idx);

	static void _code_complete_scripts(void *p_ud, const String &p_code, List<ScriptCodeCompletionOption> *r_options, bool &r_force);
	void _code_complete_script(const String &p_code, List<ScriptCodeCompletionOption> *r_options, bool &r_force);

	void _load_theme_settings();
	void _set_theme_for_script();
	void _show_warnings_panel(bool p_show);
	void _warning_clicked(Variant p_line);

	static void _bind_methods();

	Map<String, SyntaxHighlighter *> highlighters;
	void _change_syntax_highlighter(int p_idx);

	void _edit_option(int p_op);
	void _edit_option_toggle_inline_comment();
	void _make_context_menu(bool p_selection, bool p_color, bool p_foldable, bool p_open_docs, bool p_goto_definition, Vector2 p_pos);
	void _text_edit_gui_input(const Ref<InputEvent> &ev);
	void _color_changed(const Color &p_color);
	void _prepare_edit_menu();

	void _goto_line(int p_line) { goto_line(p_line); }
	void _lookup_symbol(const String &p_symbol, int p_row, int p_column);

	void _lookup_connections(int p_row, String p_method);

	void _convert_case(CodeTextEditor::CaseStyle p_case);

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	String _get_absolute_path(const String &rel_path);

public:
	void _update_connected_methods();

	virtual void add_syntax_highlighter(SyntaxHighlighter *p_highlighter);
	virtual void set_syntax_highlighter(SyntaxHighlighter *p_highlighter);
	void update_toggle_scripts_button();

	virtual void apply_code();
	virtual RES get_edited_resource() const;
	virtual void set_edited_resource(const RES &p_res);
	virtual void enable_editor();
	virtual Vector<String> get_functions();
	virtual void reload_text();
	virtual String get_name();
	virtual Ref<Texture> get_icon();
	virtual bool is_unsaved();
	virtual Variant get_edit_state();
	virtual void set_edit_state(const Variant &p_state);
	virtual void ensure_focus();
	virtual void trim_trailing_whitespace();
	virtual void insert_final_newline();
	virtual void convert_indent_to_spaces();
	virtual void convert_indent_to_tabs();
	virtual void tag_saved_version();

	virtual void goto_line(int p_line, bool p_with_error = false);
	void goto_line_selection(int p_line, int p_begin, int p_end);
	void goto_line_centered(int p_line);
	virtual void set_executing_line(int p_line);
	virtual void clear_executing_line();
	void lookup_symbol_under_cursor();

	virtual void reload(bool p_soft);
	virtual void get_breakpoints(List<int> *p_breakpoints);

	virtual void add_callback(const String &p_function, PoolStringArray p_args);
	virtual void update_settings();

	virtual bool show_members_overview();

	virtual void set_tooltip_request_func(String p_method, Object *p_obj);

	virtual void set_debugger_active(bool p_active);

	Control *get_edit_menu();
	Control *get_code_editor_text_edit();
	virtual void clear_edit_menu();
	static void register_editor();

	virtual void validate();

	ScriptTextEditor();
	~ScriptTextEditor();
};

#endif // SCRIPT_TEXT_EDITOR_H
