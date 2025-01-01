/**************************************************************************/
/*  code_edit.h                                                           */
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

#ifndef CODE_EDIT_H
#define CODE_EDIT_H

#include "core/object/script_language.h"
#include "scene/gui/text_edit.h"

class CodeEdit : public TextEdit {
	GDCLASS(CodeEdit, TextEdit)

public:
	// Keep enums in sync with:
	// core/object/script_language.h - ScriptLanguage::CodeCompletionKind
	enum CodeCompletionKind {
		KIND_CLASS,
		KIND_FUNCTION,
		KIND_SIGNAL,
		KIND_VARIABLE,
		KIND_MEMBER,
		KIND_ENUM,
		KIND_CONSTANT,
		KIND_NODE_PATH,
		KIND_FILE_PATH,
		KIND_PLAIN_TEXT,
	};

	// core/object/script_language.h - ScriptLanguage::CodeCompletionLocation
	enum CodeCompletionLocation {
		LOCATION_LOCAL = 0,
		LOCATION_PARENT_MASK = 1 << 8,
		LOCATION_OTHER_USER_CODE = 1 << 9,
		LOCATION_OTHER = 1 << 10,
	};

private:
	/* Indent management */
	int indent_size = 4;
	String indent_text = "\t";

	bool auto_indent = false;
	HashSet<char32_t> auto_indent_prefixes;

	bool indent_using_spaces = false;
	int _calculate_spaces_till_next_left_indent(int p_column) const;
	int _calculate_spaces_till_next_right_indent(int p_column) const;

	void _new_line(bool p_split_current_line = true, bool p_above = false);

	/* Auto brace completion */
	bool auto_brace_completion_enabled = false;

	/* BracePair open_key must be uniquie and ordered by length. */
	struct BracePair {
		String open_key = "";
		String close_key = "";
	};
	Vector<BracePair> auto_brace_completion_pairs;

	int _get_auto_brace_pair_open_at_pos(int p_line, int p_col);
	int _get_auto_brace_pair_close_at_pos(int p_line, int p_col);

	/* Main Gutter */
	enum MainGutterType {
		MAIN_GUTTER_BREAKPOINT = 0x01,
		MAIN_GUTTER_BOOKMARK = 0x02,
		MAIN_GUTTER_EXECUTING = 0x04
	};

	int main_gutter = -1;
	void _update_draw_main_gutter();
	void _main_gutter_draw_callback(int p_line, int p_gutter, const Rect2 &p_region);

	// breakpoints
	HashMap<int, bool> breakpointed_lines;
	bool draw_breakpoints = false;

	// bookmarks
	bool draw_bookmarks = false;

	// executing lines
	bool draw_executing_lines = false;

	/* Line numbers */
	int line_number_gutter = -1;
	int line_number_digits = 1;
	String line_number_padding = " ";
	HashMap<int, RID> line_number_text_cache;
	void _clear_line_number_text_cache();
	void _update_line_number_gutter_width();
	void _line_number_draw_callback(int p_line, int p_gutter, const Rect2 &p_region);

	/* Fold Gutter */
	int fold_gutter = -1;
	bool draw_fold_gutter = false;
	void _fold_gutter_draw_callback(int p_line, int p_gutter, Rect2 p_region);

	void _gutter_clicked(int p_line, int p_gutter);
	void _update_gutter_indexes();

	/* Line Folding */
	bool line_folding_enabled = false;
	String code_region_start_string;
	String code_region_end_string;
	String code_region_start_tag = "region";
	String code_region_end_tag = "endregion";
	void _update_code_region_tags();

	/* Delimiters */
	enum DelimiterType {
		TYPE_STRING,
		TYPE_COMMENT,
	};

	struct Delimiter {
		DelimiterType type;
		String start_key = "";
		String end_key = "";
		bool line_only = true;
	};
	bool setting_delimiters = false;
	Vector<Delimiter> delimiters;
	/*
	 *  Vector entry per line, contains a Map of column numbers to delimiter index, -1 marks the end of a region.
	 *  e.g the following text will be stored as so:
	 *
	 *  0: nothing here
	 *  1:
	 *  2: # test
	 *  3: "test" text "multiline
	 *  4:
	 *  5: test
	 *  6: string"
	 *
	 *  Vector [
	 *      0 = []
	 *      1 = []
	 *      2 = [
	 *          1 = 1
	 *          6 = -1
	 *      ]
	 *      3 = [
	 *	        1 = 0
	 *          6 = -1
	 *          13 = 0
	 *      ]
	 *      4 = [
	 *          0 = 0
	 *      ]
	 *      5 = [
	 *          5 = 0
	 *      ]
	 *      6 = [
	 *          7 = -1
	 *      ]
	 *  ]
	 */
	Vector<RBMap<int, int>> delimiter_cache;

	void _update_delimiter_cache(int p_from_line = 0, int p_to_line = -1);
	int _is_in_delimiter(int p_line, int p_column, DelimiterType p_type) const;

	void _add_delimiter(const String &p_start_key, const String &p_end_key, bool p_line_only, DelimiterType p_type);
	void _remove_delimiter(const String &p_start_key, DelimiterType p_type);
	bool _has_delimiter(const String &p_start_key, DelimiterType p_type) const;

	void _set_delimiters(const TypedArray<String> &p_delimiters, DelimiterType p_type);
	void _clear_delimiters(DelimiterType p_type);
	TypedArray<String> _get_delimiters(DelimiterType p_type) const;

	/* Code Hint */
	String code_hint = "";

	bool code_hint_draw_below = true;
	int code_hint_xpos = -0xFFFF;

	/* Code Completion */
	bool code_completion_enabled = false;
	bool code_completion_forced = false;

	bool code_completion_active = false;
	bool is_code_completion_scroll_hovered = false;
	bool is_code_completion_scroll_pressed = false;
	bool is_code_completion_drag_started = false;
	Vector<ScriptLanguage::CodeCompletionOption> code_completion_options;
	int code_completion_line_ofs = 0;
	int code_completion_current_selected = 0;
	int code_completion_force_item_center = -1;
	int code_completion_longest_line = 0;
	Rect2i code_completion_rect;
	Rect2i code_completion_scroll_rect;
	float code_completion_pan_offset = 0.0f;

	HashSet<char32_t> code_completion_prefixes;
	List<ScriptLanguage::CodeCompletionOption> code_completion_option_submitted;
	List<ScriptLanguage::CodeCompletionOption> code_completion_option_sources;
	String code_completion_base;

	void _update_scroll_selected_line(float p_mouse_y);
	void _filter_code_completion_candidates_impl();
	bool _should_reset_selected_option_for_new_options(const Vector<ScriptLanguage::CodeCompletionOption> &p_new_options);

	/* Line length guidelines */
	TypedArray<int> line_length_guideline_columns;

	/* Symbol lookup */
	bool symbol_lookup_on_click_enabled = false;
	Point2i symbol_lookup_pos; // Column and line.
	String symbol_lookup_new_word;
	String symbol_lookup_word;

	/* Symbol tooltip */
	bool symbol_tooltip_on_hover_enabled = false;
	Point2i symbol_tooltip_pos; // Column and line.
	String symbol_tooltip_word;
	Timer *symbol_tooltip_timer = nullptr;
	void _on_symbol_tooltip_timer_timeout();

	/* Visual */
	struct ThemeCache {
		/* Gutters */
		Color code_folding_color = Color(1, 1, 1);
		Color folded_code_region_color = Color(1, 1, 1);
		Ref<Texture2D> can_fold_icon;
		Ref<Texture2D> folded_icon;
		Ref<Texture2D> can_fold_code_region_icon;
		Ref<Texture2D> folded_code_region_icon;
		Ref<Texture2D> folded_eol_icon;
		Ref<Texture2D> completion_color_bg;

		Color breakpoint_color = Color(1, 1, 1);
		Ref<Texture2D> breakpoint_icon;

		Color bookmark_color = Color(1, 1, 1);
		Ref<Texture2D> bookmark_icon;

		Color executing_line_color = Color(1, 1, 1);
		Ref<Texture2D> executing_line_icon;

		Color line_number_color = Color(1, 1, 1);

		/* Code Completion */
		Ref<StyleBox> code_completion_style;
		int code_completion_icon_separation = 0;

		int code_completion_max_width = 0;
		int code_completion_max_lines = 7;
		int code_completion_scroll_width = 0;
		Color code_completion_scroll_color = Color(0, 0, 0, 0);
		Color code_completion_scroll_hovered_color = Color(0, 0, 0, 0);
		Color code_completion_background_color = Color(0, 0, 0, 0);
		Color code_completion_selected_color = Color(0, 0, 0, 0);
		Color code_completion_existing_color = Color(0, 0, 0, 0);

		/* Code hint */
		Ref<StyleBox> code_hint_style;
		Color code_hint_color;

		/* Line length guideline */
		Color line_length_guideline_color;

		/* Other visuals */
		Ref<StyleBox> style_normal;

		Color brace_mismatch_color;

		Ref<Font> font;
		int font_size = 16;
		int line_spacing = 1;
	} theme_cache;

	virtual Color _get_brace_mismatch_color() const override;
	virtual Color _get_code_folding_color() const override;
	virtual Ref<Texture2D> _get_folded_eol_icon() const override;

	/* Callbacks */
	int lines_edited_changed = 0;
	int lines_edited_from = -1;
	int lines_edited_to = -1;

	void _lines_edited_from(int p_from_line, int p_to_line);
	void _text_set();
	void _text_changed();

	void _apply_project_settings();

protected:
	void _notification(int p_what);
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	String _get_text_for_symbol_lookup_bind_compat_73196();
	void _add_code_completion_option_compat_84906(CodeCompletionKind p_type, const String &p_display_text, const String &p_insert_text, const Color &p_text_color = Color(1, 1, 1), const Ref<Resource> &p_icon = Ref<Resource>(), const Variant &p_value = Variant::NIL, int p_location = LOCATION_OTHER);
	static void _bind_compatibility_methods();
#endif

	virtual void _unhide_carets() override;

	/* Text manipulation */

	// Overridable actions
	virtual void _handle_unicode_input_internal(const uint32_t p_unicode, int p_caret) override;
	virtual void _backspace_internal(int p_caret) override;
	virtual void _cut_internal(int p_caret) override;

	GDVIRTUAL1(_confirm_code_completion, bool)
	GDVIRTUAL1(_request_code_completion, bool)
	GDVIRTUAL1RC(TypedArray<Dictionary>, _filter_code_completion_candidates, TypedArray<Dictionary>)

public:
	/* General overrides */
	virtual void gui_input(const Ref<InputEvent> &p_gui_input) override;
	virtual CursorShape get_cursor_shape(const Point2 &p_pos = Point2i()) const override;

	/* Indent management */
	void set_indent_size(const int p_size);
	int get_indent_size() const;

	void set_indent_using_spaces(const bool p_use_spaces);
	bool is_indent_using_spaces() const;

	void set_auto_indent_enabled(bool p_enabled);
	bool is_auto_indent_enabled() const;

	void set_auto_indent_prefixes(const TypedArray<String> &p_prefixes);
	TypedArray<String> get_auto_indent_prefixes() const;

	void do_indent();

	void indent_lines();
	void unindent_lines();

	void convert_indent(int p_from_line = -1, int p_to_line = -1);

	/* Auto brace completion */
	void set_auto_brace_completion_enabled(bool p_enabled);
	bool is_auto_brace_completion_enabled() const;

	void set_highlight_matching_braces_enabled(bool p_enabled);
	bool is_highlight_matching_braces_enabled() const;

	void add_auto_brace_completion_pair(const String &p_open_key, const String &p_close_key);
	void set_auto_brace_completion_pairs(const Dictionary &p_auto_brace_completion_pairs);
	Dictionary get_auto_brace_completion_pairs() const;

	bool has_auto_brace_completion_open_key(const String &p_open_key) const;
	bool has_auto_brace_completion_close_key(const String &p_close_key) const;

	String get_auto_brace_completion_close_key(const String &p_open_key) const;

	/* Main Gutter */
	void set_draw_breakpoints_gutter(bool p_draw);
	bool is_drawing_breakpoints_gutter() const;

	void set_draw_bookmarks_gutter(bool p_draw);
	bool is_drawing_bookmarks_gutter() const;

	void set_draw_executing_lines_gutter(bool p_draw);
	bool is_drawing_executing_lines_gutter() const;

	// breakpoints
	void set_line_as_breakpoint(int p_line, bool p_breakpointed);
	bool is_line_breakpointed(int p_line) const;
	void clear_breakpointed_lines();
	PackedInt32Array get_breakpointed_lines() const;

	// bookmarks
	void set_line_as_bookmarked(int p_line, bool p_bookmarked);
	bool is_line_bookmarked(int p_line) const;
	void clear_bookmarked_lines();
	PackedInt32Array get_bookmarked_lines() const;

	// executing lines
	void set_line_as_executing(int p_line, bool p_executing);
	bool is_line_executing(int p_line) const;
	void clear_executing_lines();
	PackedInt32Array get_executing_lines() const;

	/* Line numbers */
	void set_draw_line_numbers(bool p_draw);
	bool is_draw_line_numbers_enabled() const;
	void set_line_numbers_zero_padded(bool p_zero_padded);
	bool is_line_numbers_zero_padded() const;

	/* Fold gutter */
	void set_draw_fold_gutter(bool p_draw);
	bool is_drawing_fold_gutter() const;

	/* Line Folding */
	void set_line_folding_enabled(bool p_enabled);
	bool is_line_folding_enabled() const;

	bool can_fold_line(int p_line) const;

	void fold_line(int p_line);
	void unfold_line(int p_line);
	void fold_all_lines();
	void unfold_all_lines();
	void toggle_foldable_line(int p_line);
	void toggle_foldable_lines_at_carets();

	bool is_line_folded(int p_line) const;
	TypedArray<int> get_folded_lines() const;

	/* Code region */
	void create_code_region();
	String get_code_region_start_tag() const;
	String get_code_region_end_tag() const;
	void set_code_region_tags(const String &p_start = "region", const String &p_end = "endregion");
	bool is_line_code_region_start(int p_line) const;
	bool is_line_code_region_end(int p_line) const;

	/* Delimiters */
	void add_string_delimiter(const String &p_start_key, const String &p_end_key, bool p_line_only = false);
	void remove_string_delimiter(const String &p_start_key);
	bool has_string_delimiter(const String &p_start_key) const;

	void set_string_delimiters(const TypedArray<String> &p_string_delimiters);
	void clear_string_delimiters();
	TypedArray<String> get_string_delimiters() const;

	int is_in_string(int p_line, int p_column = -1) const;

	void add_comment_delimiter(const String &p_start_key, const String &p_end_key, bool p_line_only = false);
	void remove_comment_delimiter(const String &p_start_key);
	bool has_comment_delimiter(const String &p_start_key) const;

	void set_comment_delimiters(const TypedArray<String> &p_comment_delimiters);
	void clear_comment_delimiters();
	TypedArray<String> get_comment_delimiters() const;

	int is_in_comment(int p_line, int p_column = -1) const;

	String get_delimiter_start_key(int p_delimiter_idx) const;
	String get_delimiter_end_key(int p_delimiter_idx) const;

	Point2 get_delimiter_start_position(int p_line, int p_column) const;
	Point2 get_delimiter_end_position(int p_line, int p_column) const;

	/* Code hint */
	void set_code_hint(const String &p_hint);
	void set_code_hint_draw_below(bool p_below);

	/* Code Completion */
	void set_code_completion_enabled(bool p_enable);
	bool is_code_completion_enabled() const;

	void set_code_completion_prefixes(const TypedArray<String> &p_prefixes);
	TypedArray<String> get_code_completion_prefixes() const;

	String get_text_for_code_completion() const;

	void request_code_completion(bool p_force = false);

	void add_code_completion_option(CodeCompletionKind p_type, const String &p_display_text, const String &p_insert_text, const Color &p_text_color = Color(1, 1, 1), const Ref<Resource> &p_icon = Ref<Resource>(), const Variant &p_value = Variant(), int p_location = LOCATION_OTHER);
	void update_code_completion_options(bool p_forced = false);

	TypedArray<Dictionary> get_code_completion_options() const;
	Dictionary get_code_completion_option(int p_index) const;

	int get_code_completion_selected_index() const;
	void set_code_completion_selected_index(int p_index);

	void confirm_code_completion(bool p_replace = false);
	void cancel_code_completion();

	/* Line length guidelines */
	void set_line_length_guidelines(TypedArray<int> p_guideline_columns);
	TypedArray<int> get_line_length_guidelines() const;

	/* Symbol lookup */
	void set_symbol_lookup_on_click_enabled(bool p_enabled);
	bool is_symbol_lookup_on_click_enabled() const;

	String get_text_for_symbol_lookup() const;
	String get_text_with_cursor_char(int p_line, int p_column) const;

	void set_symbol_lookup_word_as_valid(bool p_valid);

	/* Symbol tooltip */
	void set_symbol_tooltip_on_hover_enabled(bool p_enabled);
	bool is_symbol_tooltip_on_hover_enabled() const;

	/* Text manipulation */
	void move_lines_up();
	void move_lines_down();
	void delete_lines();
	void duplicate_selection();
	void duplicate_lines();

	CodeEdit();
	~CodeEdit();
};

VARIANT_ENUM_CAST(CodeEdit::CodeCompletionKind);
VARIANT_ENUM_CAST(CodeEdit::CodeCompletionLocation);

// The custom comparer which will sort completion options.
struct CodeCompletionOptionCompare {
	_FORCE_INLINE_ bool operator()(const ScriptLanguage::CodeCompletionOption &l, const ScriptLanguage::CodeCompletionOption &r) const;
};

#endif // CODE_EDIT_H
