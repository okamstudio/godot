/**************************************************************************/
/*  text_editor.h                                                         */
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

#ifndef TEXT_EDITOR_H
#define TEXT_EDITOR_H

#include "script_editor_plugin.h"

#include "editor/code_editor.h"

class TextEditor : public ScriptEditorBase {
	GDCLASS(TextEditor, ScriptEditorBase);

	static ScriptEditorBase *create_editor(const Ref<Resource> &p_resource);

private:
	CodeTextEditor *code_editor = nullptr;

	Ref<Resource> edited_res;
	bool editor_enabled = false;

	HBoxContainer *edit_hb = nullptr;
	MenuButton *edit_menu = nullptr;
	PopupMenu *highlighter_menu = nullptr;
	MenuButton *search_menu = nullptr;
	PopupMenu *bookmarks_menu = nullptr;
	PopupMenu *context_menu = nullptr;

	GotoLinePopup *goto_line_popup = nullptr;

	enum {
		EDIT_UNDO,
		EDIT_REDO,
		EDIT_CUT,
		EDIT_COPY,
		EDIT_PASTE,
		EDIT_SELECT_ALL,
		EDIT_TRIM_TRAILING_WHITESAPCE,
		EDIT_TRIM_FINAL_NEWLINES,
		EDIT_CONVERT_INDENT_TO_SPACES,
		EDIT_CONVERT_INDENT_TO_TABS,
		EDIT_MOVE_LINE_UP,
		EDIT_MOVE_LINE_DOWN,
		EDIT_INDENT,
		EDIT_UNINDENT,
		EDIT_DELETE_LINE,
		EDIT_DUPLICATE_SELECTION,
		EDIT_DUPLICATE_LINES,
		EDIT_TO_UPPERCASE,
		EDIT_TO_LOWERCASE,
		EDIT_CAPITALIZE,
		EDIT_TOGGLE_WORD_WRAP,
		EDIT_TOGGLE_FOLD_LINE,
		EDIT_FOLD_ALL_LINES,
		EDIT_UNFOLD_ALL_LINES,
		SEARCH_FIND,
		SEARCH_FIND_NEXT,
		SEARCH_FIND_PREV,
		SEARCH_REPLACE,
		SEARCH_IN_FILES,
		REPLACE_IN_FILES,
		SEARCH_GOTO_LINE,
		BOOKMARK_TOGGLE,
		BOOKMARK_GOTO_NEXT,
		BOOKMARK_GOTO_PREV,
		BOOKMARK_REMOVE_ALL,
		EDIT_EMOJI_AND_SYMBOL,
	};

protected:
	void _edit_option(int p_op);
	void _make_context_menu(bool p_selection, bool p_can_fold, bool p_is_folded, Vector2 p_position);
	void _text_edit_gui_input(const Ref<InputEvent> &ev);
	void _prepare_edit_menu();

	HashMap<String, Ref<EditorSyntaxHighlighter>> highlighters;
	void _change_syntax_highlighter(int p_idx);
	void _load_theme_settings();

	void _convert_case(CodeTextEditor::CaseStyle p_case);

	void _validate_script();

	void _update_bookmark_list();
	void _bookmark_item_pressed(int p_idx);

public:
	virtual void add_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) override;
	virtual void set_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) override;

	virtual String get_name() override;
	virtual Ref<Texture2D> get_theme_icon() override;
	virtual Ref<Resource> get_edited_resource() const override;
	virtual void set_edited_resource(const Ref<Resource> &p_res) override;
	virtual void enable_editor(Control *p_shortcut_context = nullptr) override;
	virtual void reload_text() override;
	virtual void apply_code() override;
	virtual bool is_unsaved() override;
	virtual Variant get_edit_state() override;
	virtual void set_edit_state(const Variant &p_state) override;
	virtual Variant get_navigation_state() override;
	virtual Vector<String> get_functions() override;
	virtual PackedInt32Array get_breakpoints() override;
	virtual void set_breakpoint(int p_line, bool p_enabled) override {}
	virtual void clear_breakpoints() override {}
	virtual void goto_line(int p_line, int p_column = 0) override;
	void goto_line_selection(int p_line, int p_begin, int p_end);
	virtual void set_executing_line(int p_line) override;
	virtual void clear_executing_line() override;
	virtual void trim_trailing_whitespace() override;
	virtual void trim_final_newlines() override;
	virtual void insert_final_newline() override;
	virtual void convert_indent() override;
	virtual void ensure_focus() override;
	virtual void tag_saved_version() override;
	virtual void update_settings() override;
	virtual bool show_members_overview() override;
	virtual bool can_lose_focus_on_node_selection() override { return true; }
	virtual void set_debugger_active(bool p_active) override;
	virtual void set_tooltip_request_func(const Callable &p_toolip_callback) override;
	virtual void add_callback(const String &p_function, const PackedStringArray &p_args) override;
	void update_toggle_scripts_button() override;

	virtual Control *get_edit_menu() override;
	virtual void clear_edit_menu() override;
	virtual void set_find_replace_bar(FindReplaceBar *p_bar) override;

	virtual void validate() override;

	virtual Control *get_base_editor() const override;
	virtual CodeTextEditor *get_code_editor() const override;

	static void register_editor();

	TextEditor();
	~TextEditor();
};

#endif // TEXT_EDITOR_H
