/**************************************************************************/
/*  code_editor.cpp                                                       */
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

#include "code_editor.h"

#include "core/input/input.h"
#include "core/os/keyboard.h"
#include "core/string/string_builder.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/separator.h"
#include "scene/resources/font.h"

void GotoLineDialog::popup_find_line(CodeEdit *p_edit) {
	text_editor = p_edit;

	// Add 1 because text_editor->get_caret_line() starts from 0, but the editor user interface starts from 1.
	line->set_text(itos(text_editor->get_caret_line() + 1));
	line->select_all();
	popup_centered(Size2(180, 80) * EDSCALE);
	line->grab_focus();
}

int GotoLineDialog::get_line() const {
	return line->get_text().to_int();
}

void GotoLineDialog::ok_pressed() {
	// Subtract 1 because the editor user interface starts from 1, but text_editor->set_caret_line(n) starts from 0.
	const int line_number = get_line() - 1;
	if (line_number < 0 || line_number >= text_editor->get_line_count()) {
		return;
	}
	text_editor->remove_secondary_carets();
	text_editor->unfold_line(line_number);
	text_editor->set_caret_line(line_number);
	text_editor->set_code_hint("");
	text_editor->cancel_code_completion();
	hide();
}

GotoLineDialog::GotoLineDialog() {
	set_title(TTR("Go to Line"));

	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->set_anchor_and_offset(SIDE_LEFT, Control::ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_TOP, Control::ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_RIGHT, Control::ANCHOR_END, -8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_BOTTOM, Control::ANCHOR_END, -8 * EDSCALE);
	add_child(vbc);

	Label *l = memnew(Label);
	l->set_text(TTR("Line Number:"));
	vbc->add_child(l);

	line = memnew(LineEdit);
	vbc->add_child(line);
	register_text_enter(line);
	text_editor = nullptr;

	line_label = nullptr;

	set_hide_on_ok(false);
}

void FindReplaceBar::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorThemeManager::is_generated_theme_outdated()) {
				break;
			}
			[[fallthrough]];
		}
		case NOTIFICATION_READY: {
			find_prev->set_icon(get_editor_theme_icon(SNAME("MoveUp")));
			find_next->set_icon(get_editor_theme_icon(SNAME("MoveDown")));
			hide_button->set_texture_normal(get_editor_theme_icon(SNAME("Close")));
			hide_button->set_texture_hover(get_editor_theme_icon(SNAME("Close")));
			hide_button->set_texture_pressed(get_editor_theme_icon(SNAME("Close")));
			hide_button->set_custom_minimum_size(hide_button->get_texture_normal()->get_size());
			_update_toggle_replace_button(replace_text->is_visible_in_tree());
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			set_process_unhandled_input(is_visible_in_tree());
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			matches_label->add_theme_color_override(SceneStringName(font_color), results_count > 0 ? get_theme_color(SceneStringName(font_color), SNAME("Label")) : get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		} break;

		case NOTIFICATION_PREDELETE: {
			if (base_text_editor) {
				base_text_editor->remove_find_replace_bar();
				base_text_editor = nullptr;
			}
		} break;
	}
}

void FindReplaceBar::unhandled_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> k = p_event;

	if (k.is_valid() && k->is_action_pressed(SNAME("ui_cancel"), false, true)) {
		Control *focus_owner = get_viewport()->gui_get_focus_owner();

		if (text_editor->has_focus() || (focus_owner && is_ancestor_of(focus_owner))) {
			_hide_bar();
			accept_event();
		}
	}
}

void FindReplaceBar::_focus_lost() {
	if (Input::get_singleton()->is_action_pressed(SNAME("ui_cancel"))) {
		// Unfocused after pressing Escape, so hide the bar.
		_hide_bar(true);
	}
}

void FindReplaceBar::_update_flags(bool p_direction_backwards) {
	flags = 0;

	if (is_whole_words()) {
		flags |= TextEdit::SEARCH_WHOLE_WORDS;
	}
	if (is_case_sensitive()) {
		flags |= TextEdit::SEARCH_MATCH_CASE;
	}
	if (p_direction_backwards) {
		flags |= TextEdit::SEARCH_BACKWARDS;
	}
}

bool FindReplaceBar::_search(uint32_t p_flags, int p_from_line, int p_from_col) {
	if (!preserve_cursor) {
		text_editor->remove_secondary_carets();
	}
	String text = get_search_text();
	Point2i pos = text_editor->search(text, p_flags, p_from_line, p_from_col);

	if (pos.x != -1) {
		if (!preserve_cursor && !is_selection_only()) {
			text_editor->unfold_line(pos.y);
			text_editor->select(pos.y, pos.x, pos.y, pos.x + text.length());
			text_editor->center_viewport_to_caret(0);
			text_editor->set_code_hint("");
			text_editor->cancel_code_completion();

			line_col_changed_for_result = true;
		}

		text_editor->set_search_text(text);
		text_editor->set_search_flags(p_flags);

		result_line = pos.y;
		result_col = pos.x;

		_update_results_count();
	} else {
		results_count = 0;
		result_line = -1;
		result_col = -1;
		text_editor->set_search_text("");
		text_editor->set_search_flags(p_flags);
	}

	_update_matches_display();

	return pos.x != -1;
}

void FindReplaceBar::_replace() {
	text_editor->begin_complex_operation();
	text_editor->remove_secondary_carets();
	bool selection_enabled = text_editor->has_selection(0);
	Point2i selection_begin, selection_end;
	if (selection_enabled) {
		selection_begin = Point2i(text_editor->get_selection_from_line(0), text_editor->get_selection_from_column(0));
		selection_end = Point2i(text_editor->get_selection_to_line(0), text_editor->get_selection_to_column(0));
	}

	String repl_text = get_replace_text();
	int search_text_len = get_search_text().length();

	if (selection_enabled && is_selection_only()) {
		// Restrict search_current() to selected region.
		text_editor->set_caret_line(selection_begin.width, false, true, -1, 0);
		text_editor->set_caret_column(selection_begin.height, true, 0);
	}

	if (search_current()) {
		text_editor->unfold_line(result_line);
		text_editor->select(result_line, result_col, result_line, result_col + search_text_len, 0);

		if (selection_enabled && is_selection_only()) {
			Point2i match_from(result_line, result_col);
			Point2i match_to(result_line, result_col + search_text_len);
			if (!(match_from < selection_begin || match_to > selection_end)) {
				text_editor->insert_text_at_caret(repl_text, 0);
				if (match_to.x == selection_end.x) {
					// Adjust selection bounds if necessary.
					selection_end.y += repl_text.length() - search_text_len;
				}
			}
		} else {
			text_editor->insert_text_at_caret(repl_text, 0);
		}
	}
	text_editor->end_complex_operation();
	results_count = -1;
	results_count_to_current = -1;
	needs_to_count_results = true;

	if (selection_enabled && is_selection_only()) {
		// Reselect in order to keep 'Replace' restricted to selection.
		text_editor->select(selection_begin.x, selection_begin.y, selection_end.x, selection_end.y, 0);
	} else {
		text_editor->deselect(0);
	}
}

void FindReplaceBar::_replace_all() {
	text_editor->begin_complex_operation();
	text_editor->remove_secondary_carets();
	text_editor->disconnect(SceneStringName(text_changed), callable_mp(this, &FindReplaceBar::_editor_text_changed));
	// Line as x so it gets priority in comparison, column as y.
	Point2i orig_cursor(text_editor->get_caret_line(0), text_editor->get_caret_column(0));
	Point2i prev_match = Point2(-1, -1);

	bool selection_enabled = text_editor->has_selection(0);
	if (!is_selection_only()) {
		text_editor->deselect();
		selection_enabled = false;
	} else {
		result_line = -1;
		result_col = -1;
	}

	Point2i selection_begin, selection_end;
	if (selection_enabled) {
		selection_begin = Point2i(text_editor->get_selection_from_line(0), text_editor->get_selection_from_column(0));
		selection_end = Point2i(text_editor->get_selection_to_line(0), text_editor->get_selection_to_column(0));
	}

	int vsval = text_editor->get_v_scroll();

	String repl_text = get_replace_text();
	int search_text_len = get_search_text().length();

	int rc = 0;

	replace_all_mode = true;

	if (selection_enabled && is_selection_only()) {
		text_editor->set_caret_line(selection_begin.width, false, true, -1, 0);
		text_editor->set_caret_column(selection_begin.height, true, 0);
	} else {
		text_editor->set_caret_line(0, false, true, -1, 0);
		text_editor->set_caret_column(0, true, 0);
	}

	if (search_current()) {
		do {
			// Replace area.
			Point2i match_from(result_line, result_col);
			Point2i match_to(result_line, result_col + search_text_len);

			if (match_from < prev_match) {
				break; // Done.
			}

			prev_match = Point2i(result_line, result_col + repl_text.length());

			text_editor->unfold_line(result_line);
			text_editor->select(result_line, result_col, result_line, match_to.y, 0);

			if (selection_enabled) {
				if (match_from < selection_begin || match_to > selection_end) {
					break; // Done.
				}

				// Replace but adjust selection bounds.
				text_editor->insert_text_at_caret(repl_text, 0);
				if (match_to.x == selection_end.x) {
					selection_end.y += repl_text.length() - search_text_len;
				}

			} else {
				// Just replace.
				text_editor->insert_text_at_caret(repl_text, 0);
			}

			rc++;
		} while (search_next());
	}

	text_editor->end_complex_operation();

	replace_all_mode = false;

	// Restore editor state (selection, cursor, scroll).
	text_editor->set_caret_line(orig_cursor.x, false, true, 0, 0);
	text_editor->set_caret_column(orig_cursor.y, true, 0);

	if (selection_enabled) {
		// Reselect.
		text_editor->select(selection_begin.x, selection_begin.y, selection_end.x, selection_end.y, 0);
	}

	text_editor->set_v_scroll(vsval);
	matches_label->add_theme_color_override(SceneStringName(font_color), rc > 0 ? get_theme_color(SceneStringName(font_color), SNAME("Label")) : get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
	matches_label->set_text(vformat(TTR("%d replaced."), rc));

	callable_mp((Object *)text_editor, &Object::connect).call_deferred(SceneStringName(text_changed), callable_mp(this, &FindReplaceBar::_editor_text_changed), 0U);
	results_count = -1;
	results_count_to_current = -1;
	needs_to_count_results = true;
}

void FindReplaceBar::_get_search_from(int &r_line, int &r_col, SearchMode p_search_mode) {
	if (!text_editor->has_selection(0) || is_selection_only()) {
		r_line = text_editor->get_caret_line(0);
		r_col = text_editor->get_caret_column(0);

		if (p_search_mode == SEARCH_PREV && r_line == result_line && r_col >= result_col && r_col <= result_col + get_search_text().length()) {
			r_col = result_col;
		}
		return;
	}

	if (p_search_mode == SEARCH_NEXT) {
		r_line = text_editor->get_selection_to_line();
		r_col = text_editor->get_selection_to_column();
	} else {
		r_line = text_editor->get_selection_from_line();
		r_col = text_editor->get_selection_from_column();
	}
}

void FindReplaceBar::_update_results_count() {
	int caret_line, caret_column;
	_get_search_from(caret_line, caret_column, SEARCH_CURRENT);
	bool match_selected = caret_line == result_line && caret_column == result_col && !is_selection_only() && text_editor->has_selection(0);

	if (match_selected && !needs_to_count_results && result_line != -1 && results_count_to_current > 0) {
		results_count_to_current += (flags & TextEdit::SEARCH_BACKWARDS) ? -1 : 1;

		if (results_count_to_current > results_count) {
			results_count_to_current = results_count_to_current - results_count;
		} else if (results_count_to_current <= 0) {
			results_count_to_current = results_count;
		}

		return;
	}

	String searched = get_search_text();
	if (searched.is_empty()) {
		return;
	}

	needs_to_count_results = !match_selected;

	results_count = 0;
	results_count_to_current = 0;

	for (int i = 0; i < text_editor->get_line_count(); i++) {
		String line_text = text_editor->get_line(i);

		int col_pos = 0;

		bool searched_start_is_symbol = is_symbol(searched[0]);
		bool searched_end_is_symbol = is_symbol(searched[searched.length() - 1]);

		while (true) {
			col_pos = is_case_sensitive() ? line_text.find(searched, col_pos) : line_text.findn(searched, col_pos);

			if (col_pos == -1) {
				break;
			}

			if (is_whole_words()) {
				if (!searched_start_is_symbol && col_pos > 0 && !is_symbol(line_text[col_pos - 1])) {
					col_pos += searched.length();
					continue;
				}
				if (!searched_end_is_symbol && col_pos + searched.length() < line_text.length() && !is_symbol(line_text[col_pos + searched.length()])) {
					col_pos += searched.length();
					continue;
				}
			}

			results_count++;

			if (i <= result_line && col_pos <= result_col) {
				results_count_to_current = results_count;
			}
			if (i == result_line && col_pos < result_col && col_pos + searched.length() > result_col) {
				// Searching forwards and backwards with repeating text can lead to different matches.
				col_pos = result_col;
			}
			col_pos += searched.length();
		}
	}
	if (!match_selected) {
		// Current result should refer to the match before the caret, if the caret is not on a match.
		if (caret_line != result_line || caret_column != result_col) {
			results_count_to_current -= 1;
		}
		if (results_count_to_current == 0 && (caret_line > result_line || (caret_line == result_line && caret_column > result_col))) {
			// Caret is after all matches.
			results_count_to_current = results_count;
		}
	}
}

void FindReplaceBar::_update_matches_display() {
	if (search_text->get_text().is_empty() || results_count == -1) {
		matches_label->hide();
	} else {
		matches_label->show();

		matches_label->add_theme_color_override(SceneStringName(font_color), results_count > 0 ? get_theme_color(SceneStringName(font_color), SNAME("Label")) : get_theme_color(SNAME("error_color"), EditorStringName(Editor)));

		if (results_count == 0) {
			matches_label->set_text(TTR("No match"));
		} else if (results_count_to_current == -1) {
			matches_label->set_text(vformat(TTRN("%d match", "%d matches", results_count), results_count));
		} else {
			matches_label->set_text(vformat(TTRN("%d of %d match", "%d of %d matches", results_count), results_count_to_current, results_count));
		}
	}
	find_prev->set_disabled(results_count < 1);
	find_next->set_disabled(results_count < 1);
	replace->set_disabled(search_text->get_text().is_empty());
	replace_all->set_disabled(search_text->get_text().is_empty());
}

bool FindReplaceBar::search_current() {
	_update_flags(false);

	int line, col;
	_get_search_from(line, col, SEARCH_CURRENT);

	return _search(flags, line, col);
}

bool FindReplaceBar::search_prev() {
	if (is_selection_only() && !replace_all_mode) {
		return false;
	}

	if (!is_visible()) {
		popup_search(true);
	}

	String text = get_search_text();

	if ((flags & TextEdit::SEARCH_BACKWARDS) == 0) {
		needs_to_count_results = true;
	}

	_update_flags(true);

	int line, col;
	_get_search_from(line, col, SEARCH_PREV);

	col -= text.length();
	if (col < 0) {
		line -= 1;
		if (line < 0) {
			line = text_editor->get_line_count() - 1;
		}
		col = text_editor->get_line(line).length();
	}

	return _search(flags, line, col);
}

bool FindReplaceBar::search_next() {
	if (is_selection_only() && !replace_all_mode) {
		return false;
	}

	if (!is_visible()) {
		popup_search(true);
	}

	if (flags & TextEdit::SEARCH_BACKWARDS) {
		needs_to_count_results = true;
	}

	_update_flags(false);

	int line, col;
	_get_search_from(line, col, SEARCH_NEXT);

	return _search(flags, line, col);
}

void FindReplaceBar::_hide_bar(bool p_force_focus) {
	if (replace_text->has_focus() || search_text->has_focus() || p_force_focus) {
		text_editor->grab_focus();
	}

	text_editor->set_search_text("");
	result_line = -1;
	result_col = -1;
	hide();
}

void FindReplaceBar::_update_toggle_replace_button(bool p_replace_visible) {
	String tooltip = p_replace_visible ? TTR("Hide Replace") : TTR("Show Replace");
	String shortcut = ED_GET_SHORTCUT(p_replace_visible ? "script_text_editor/find" : "script_text_editor/replace")->get_as_text();
	toggle_replace_button->set_tooltip_text(vformat("%s (%s)", tooltip, shortcut));
	StringName rtl_compliant_arrow = is_layout_rtl() ? SNAME("GuiTreeArrowLeft") : SNAME("GuiTreeArrowRight");
	toggle_replace_button->set_icon(get_editor_theme_icon(p_replace_visible ? SNAME("GuiTreeArrowDown") : rtl_compliant_arrow));
}

void FindReplaceBar::_show_search(bool p_with_replace, bool p_show_only) {
	show();
	if (p_show_only) {
		return;
	}

	const bool on_one_line = text_editor->has_selection(0) && text_editor->get_selection_from_line(0) == text_editor->get_selection_to_line(0);
	const bool focus_replace = p_with_replace && on_one_line;

	if (focus_replace) {
		search_text->deselect();
		callable_mp((Control *)replace_text, &Control::grab_focus).call_deferred();
	} else {
		replace_text->deselect();
		callable_mp((Control *)search_text, &Control::grab_focus).call_deferred();
	}

	if (on_one_line) {
		search_text->set_text(text_editor->get_selected_text(0));
		result_line = text_editor->get_selection_from_line();
		result_col = text_editor->get_selection_from_column();
	}

	if (!get_search_text().is_empty()) {
		if (focus_replace) {
			replace_text->select_all();
			replace_text->set_caret_column(replace_text->get_text().length());
		} else {
			search_text->select_all();
			search_text->set_caret_column(search_text->get_text().length());
		}

		preserve_cursor = true;
		_search_text_changed(get_search_text());
		preserve_cursor = false;
	}
}

void FindReplaceBar::popup_search(bool p_show_only) {
	replace_text->hide();
	hbc_button_replace->hide();
	hbc_option_replace->hide();
	selection_only->set_pressed(false);
	_update_toggle_replace_button(false);

	_show_search(false, p_show_only);
}

void FindReplaceBar::popup_replace() {
	if (!replace_text->is_visible_in_tree()) {
		replace_text->show();
		hbc_button_replace->show();
		hbc_option_replace->show();
		_update_toggle_replace_button(true);
	}

	selection_only->set_pressed(text_editor->has_selection(0) && text_editor->get_selection_from_line(0) < text_editor->get_selection_to_line(0));

	_show_search(true, false);
}

void FindReplaceBar::_search_options_changed(bool p_pressed) {
	results_count = -1;
	results_count_to_current = -1;
	needs_to_count_results = true;
	search_current();
}

void FindReplaceBar::_editor_text_changed() {
	results_count = -1;
	results_count_to_current = -1;
	needs_to_count_results = true;
	if (is_visible_in_tree()) {
		preserve_cursor = true;
		search_current();
		preserve_cursor = false;
	}
}

void FindReplaceBar::_search_text_changed(const String &p_text) {
	results_count = -1;
	results_count_to_current = -1;
	needs_to_count_results = true;
	search_current();
}

void FindReplaceBar::_search_text_submitted(const String &p_text) {
	if (Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
		search_prev();
	} else {
		search_next();
	}

	callable_mp(search_text, &LineEdit::edit).call_deferred();
}

void FindReplaceBar::_replace_text_submitted(const String &p_text) {
	if (selection_only->is_pressed() && text_editor->has_selection(0)) {
		_replace_all();
		_hide_bar();
	} else if (Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
		_replace();
		search_prev();
	} else {
		_replace();
		search_next();
	}
}

void FindReplaceBar::_toggle_replace_pressed() {
	bool replace_visible = replace_text->is_visible_in_tree();
	replace_visible ? popup_search(true) : popup_replace();
}

String FindReplaceBar::get_search_text() const {
	return search_text->get_text();
}

String FindReplaceBar::get_replace_text() const {
	return replace_text->get_text();
}

bool FindReplaceBar::is_case_sensitive() const {
	return case_sensitive->is_pressed();
}

bool FindReplaceBar::is_whole_words() const {
	return whole_words->is_pressed();
}

bool FindReplaceBar::is_selection_only() const {
	return selection_only->is_pressed();
}

void FindReplaceBar::set_error(const String &p_label) {
	emit_signal(SNAME("error"), p_label);
}

void FindReplaceBar::set_text_edit(CodeTextEditor *p_text_editor) {
	if (p_text_editor == base_text_editor) {
		return;
	}

	if (base_text_editor) {
		text_editor->set_search_text(String());
		base_text_editor->remove_find_replace_bar();
		base_text_editor = nullptr;
		text_editor->disconnect(SceneStringName(text_changed), callable_mp(this, &FindReplaceBar::_editor_text_changed));
		text_editor = nullptr;
	}

	if (!p_text_editor) {
		return;
	}

	results_count = -1;
	results_count_to_current = -1;
	needs_to_count_results = true;
	base_text_editor = p_text_editor;
	text_editor = base_text_editor->get_text_editor();
	text_editor->connect(SceneStringName(text_changed), callable_mp(this, &FindReplaceBar::_editor_text_changed));

	_editor_text_changed();
}

void FindReplaceBar::_bind_methods() {
	ClassDB::bind_method("_search_current", &FindReplaceBar::search_current);

	ADD_SIGNAL(MethodInfo("error"));
}

FindReplaceBar::FindReplaceBar() {
	toggle_replace_button = memnew(Button);
	add_child(toggle_replace_button);
	toggle_replace_button->set_flat(true);
	toggle_replace_button->set_focus_mode(FOCUS_NONE);
	toggle_replace_button->connect(SceneStringName(pressed), callable_mp(this, &FindReplaceBar::_toggle_replace_pressed));

	vbc_lineedit = memnew(VBoxContainer);
	add_child(vbc_lineedit);
	vbc_lineedit->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	vbc_lineedit->set_h_size_flags(SIZE_EXPAND_FILL);
	VBoxContainer *vbc_button = memnew(VBoxContainer);
	add_child(vbc_button);
	VBoxContainer *vbc_option = memnew(VBoxContainer);
	add_child(vbc_option);

	HBoxContainer *hbc_button_search = memnew(HBoxContainer);
	vbc_button->add_child(hbc_button_search);
	hbc_button_search->set_alignment(BoxContainer::ALIGNMENT_END);
	hbc_button_replace = memnew(HBoxContainer);
	vbc_button->add_child(hbc_button_replace);
	hbc_button_replace->set_alignment(BoxContainer::ALIGNMENT_END);

	HBoxContainer *hbc_option_search = memnew(HBoxContainer);
	vbc_option->add_child(hbc_option_search);
	hbc_option_replace = memnew(HBoxContainer);
	vbc_option->add_child(hbc_option_replace);

	// Search toolbar
	search_text = memnew(LineEdit);
	vbc_lineedit->add_child(search_text);
	search_text->set_placeholder(TTR("Find"));
	search_text->set_tooltip_text(TTR("Find"));
	search_text->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	search_text->connect(SceneStringName(text_changed), callable_mp(this, &FindReplaceBar::_search_text_changed));
	search_text->connect("text_submitted", callable_mp(this, &FindReplaceBar::_search_text_submitted));
	search_text->connect(SceneStringName(focus_exited), callable_mp(this, &FindReplaceBar::_focus_lost));

	matches_label = memnew(Label);
	hbc_button_search->add_child(matches_label);
	matches_label->hide();

	find_prev = memnew(Button);
	find_prev->set_flat(true);
	find_prev->set_tooltip_text(TTR("Previous Match"));
	hbc_button_search->add_child(find_prev);
	find_prev->set_focus_mode(FOCUS_NONE);
	find_prev->connect(SceneStringName(pressed), callable_mp(this, &FindReplaceBar::search_prev));

	find_next = memnew(Button);
	find_next->set_flat(true);
	find_next->set_tooltip_text(TTR("Next Match"));
	hbc_button_search->add_child(find_next);
	find_next->set_focus_mode(FOCUS_NONE);
	find_next->connect(SceneStringName(pressed), callable_mp(this, &FindReplaceBar::search_next));

	case_sensitive = memnew(CheckBox);
	hbc_option_search->add_child(case_sensitive);
	case_sensitive->set_text(TTR("Match Case"));
	case_sensitive->set_focus_mode(FOCUS_NONE);
	case_sensitive->connect(SceneStringName(toggled), callable_mp(this, &FindReplaceBar::_search_options_changed));

	whole_words = memnew(CheckBox);
	hbc_option_search->add_child(whole_words);
	whole_words->set_text(TTR("Whole Words"));
	whole_words->set_focus_mode(FOCUS_NONE);
	whole_words->connect(SceneStringName(toggled), callable_mp(this, &FindReplaceBar::_search_options_changed));

	// Replace toolbar
	replace_text = memnew(LineEdit);
	vbc_lineedit->add_child(replace_text);
	replace_text->set_placeholder(TTR("Replace"));
	replace_text->set_tooltip_text(TTR("Replace"));
	replace_text->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	replace_text->connect("text_submitted", callable_mp(this, &FindReplaceBar::_replace_text_submitted));
	replace_text->connect(SceneStringName(focus_exited), callable_mp(this, &FindReplaceBar::_focus_lost));

	replace = memnew(Button);
	hbc_button_replace->add_child(replace);
	replace->set_text(TTR("Replace"));
	replace->connect(SceneStringName(pressed), callable_mp(this, &FindReplaceBar::_replace));

	replace_all = memnew(Button);
	hbc_button_replace->add_child(replace_all);
	replace_all->set_text(TTR("Replace All"));
	replace_all->connect(SceneStringName(pressed), callable_mp(this, &FindReplaceBar::_replace_all));

	selection_only = memnew(CheckBox);
	hbc_option_replace->add_child(selection_only);
	selection_only->set_text(TTR("Selection Only"));
	selection_only->set_focus_mode(FOCUS_NONE);
	selection_only->connect(SceneStringName(toggled), callable_mp(this, &FindReplaceBar::_search_options_changed));

	hide_button = memnew(TextureButton);
	add_child(hide_button);
	hide_button->set_tooltip_text(TTR("Hide"));
	hide_button->set_focus_mode(FOCUS_NONE);
	hide_button->connect(SceneStringName(pressed), callable_mp(this, &FindReplaceBar::_hide_bar).bind(false));
	hide_button->set_v_size_flags(SIZE_SHRINK_CENTER);
}

/*** CODE EDITOR ****/

static constexpr float ZOOM_FACTOR_PRESETS[8] = { 0.5f, 0.75f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 2.0f };

// This function should be used to handle shortcuts that could otherwise
// be handled too late if they weren't handled here.
void CodeTextEditor::input(const Ref<InputEvent> &event) {
	ERR_FAIL_COND(event.is_null());

	const Ref<InputEventKey> key_event = event;

	if (!key_event.is_valid()) {
		return;
	}
	if (!key_event->is_pressed()) {
		return;
	}

	if (!text_editor->has_focus()) {
		if ((find_replace_bar != nullptr && find_replace_bar->is_visible()) && (find_replace_bar->has_focus() || (get_viewport()->gui_get_focus_owner() && find_replace_bar->is_ancestor_of(get_viewport()->gui_get_focus_owner())))) {
			if (ED_IS_SHORTCUT("script_text_editor/find_next", key_event)) {
				find_replace_bar->search_next();
				accept_event();
				return;
			}
			if (ED_IS_SHORTCUT("script_text_editor/find_previous", key_event)) {
				find_replace_bar->search_prev();
				accept_event();
				return;
			}
		}
		return;
	}

	if (ED_IS_SHORTCUT("script_text_editor/move_up", key_event)) {
		text_editor->move_lines_up();
		accept_event();
		return;
	}
	if (ED_IS_SHORTCUT("script_text_editor/move_down", key_event)) {
		text_editor->move_lines_down();
		accept_event();
		return;
	}
	if (ED_IS_SHORTCUT("script_text_editor/delete_line", key_event)) {
		text_editor->delete_lines();
		accept_event();
		return;
	}
	if (ED_IS_SHORTCUT("script_text_editor/duplicate_selection", key_event)) {
		text_editor->duplicate_selection();
		accept_event();
		return;
	}
	if (ED_IS_SHORTCUT("script_text_editor/duplicate_lines", key_event)) {
		text_editor->duplicate_lines();
		accept_event();
		return;
	}
}

void CodeTextEditor::_text_editor_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->is_pressed() && mb->is_command_or_control_pressed()) {
			if (mb->get_button_index() == MouseButton::WHEEL_UP) {
				_zoom_in();
				accept_event();
				return;
			}
			if (mb->get_button_index() == MouseButton::WHEEL_DOWN) {
				_zoom_out();
				accept_event();
				return;
			}
		}
	}

	Ref<InputEventMagnifyGesture> magnify_gesture = p_event;
	if (magnify_gesture.is_valid()) {
		_zoom_to(zoom_factor * powf(magnify_gesture->get_factor(), 0.25f));
		accept_event();
		return;
	}

	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (k->is_pressed()) {
			if (ED_IS_SHORTCUT("script_editor/zoom_in", p_event)) {
				_zoom_in();
				accept_event();
				return;
			}
			if (ED_IS_SHORTCUT("script_editor/zoom_out", p_event)) {
				_zoom_out();
				accept_event();
				return;
			}
			if (ED_IS_SHORTCUT("script_editor/reset_zoom", p_event)) {
				_zoom_to(1);
				accept_event();
				return;
			}
		}
	}
}

void CodeTextEditor::_line_col_changed() {
	if (!code_complete_timer->is_stopped() && code_complete_timer_line != text_editor->get_caret_line()) {
		code_complete_timer->stop();
	}

	String line = text_editor->get_line(text_editor->get_caret_line());

	int positional_column = 0;
	for (int i = 0; i < text_editor->get_caret_column(); i++) {
		if (line[i] == '\t') {
			positional_column += text_editor->get_indent_size(); // Tab size
		} else {
			positional_column += 1;
		}
	}

	StringBuilder sb;
	sb.append(itos(text_editor->get_caret_line() + 1).lpad(4));
	sb.append(" : ");
	sb.append(itos(positional_column + 1).lpad(3));

	line_and_col_txt->set_text(sb.as_string());

	if (find_replace_bar) {
		if (!find_replace_bar->line_col_changed_for_result) {
			find_replace_bar->needs_to_count_results = true;
		}

		find_replace_bar->line_col_changed_for_result = false;
	}
}

void CodeTextEditor::_text_changed() {
	if (code_complete_enabled && text_editor->is_insert_text_operation()) {
		code_complete_timer_line = text_editor->get_caret_line();
		code_complete_timer->start();
	}

	idle->start();

	if (find_replace_bar) {
		find_replace_bar->needs_to_count_results = true;
	}
}

void CodeTextEditor::_code_complete_timer_timeout() {
	if (!is_visible_in_tree()) {
		return;
	}
	text_editor->request_code_completion();
}

void CodeTextEditor::_complete_request() {
	List<ScriptLanguage::CodeCompletionOption> entries;
	String ctext = text_editor->get_text_for_code_completion();
	_code_complete_script(ctext, &entries);
	bool forced = false;
	if (code_complete_func) {
		code_complete_func(code_complete_ud, ctext, &entries, forced);
	}

	for (const ScriptLanguage::CodeCompletionOption &e : entries) {
		Color font_color = completion_font_color;
		if (!e.theme_color_name.is_empty() && EDITOR_GET("text_editor/completion/colorize_suggestions")) {
			font_color = get_theme_color(e.theme_color_name, SNAME("Editor"));
		} else if (e.insert_text.begins_with("\"") || e.insert_text.begins_with("\'")) {
			font_color = completion_string_color;
		} else if (e.insert_text.begins_with("##") || e.insert_text.begins_with("///")) {
			font_color = completion_doc_comment_color;
		} else if (e.insert_text.begins_with("&")) {
			font_color = completion_string_name_color;
		} else if (e.insert_text.begins_with("^")) {
			font_color = completion_node_path_color;
		} else if (e.insert_text.begins_with("#") || e.insert_text.begins_with("//")) {
			font_color = completion_comment_color;
		}
		text_editor->add_code_completion_option((CodeEdit::CodeCompletionKind)e.kind, e.display, e.insert_text, font_color, _get_completion_icon(e), e.default_value, e.location);
	}
	text_editor->update_code_completion_options(forced);
}

Ref<Texture2D> CodeTextEditor::_get_completion_icon(const ScriptLanguage::CodeCompletionOption &p_option) {
	Ref<Texture2D> tex;
	switch (p_option.kind) {
		case ScriptLanguage::CODE_COMPLETION_KIND_CLASS: {
			if (has_theme_icon(p_option.display, EditorStringName(EditorIcons))) {
				tex = get_editor_theme_icon(p_option.display);
			} else {
				tex = EditorNode::get_singleton()->get_class_icon(p_option.display);
				if (!tex.is_valid()) {
					tex = get_editor_theme_icon(SNAME("Object"));
				}
			}
		} break;
		case ScriptLanguage::CODE_COMPLETION_KIND_ENUM:
			tex = get_editor_theme_icon(SNAME("Enum"));
			break;
		case ScriptLanguage::CODE_COMPLETION_KIND_FILE_PATH:
			tex = get_editor_theme_icon(SNAME("File"));
			break;
		case ScriptLanguage::CODE_COMPLETION_KIND_NODE_PATH:
			tex = get_editor_theme_icon(SNAME("NodePath"));
			break;
		case ScriptLanguage::CODE_COMPLETION_KIND_VARIABLE:
			tex = get_editor_theme_icon(SNAME("Variant"));
			break;
		case ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT:
			tex = get_editor_theme_icon(SNAME("MemberConstant"));
			break;
		case ScriptLanguage::CODE_COMPLETION_KIND_MEMBER:
			tex = get_editor_theme_icon(SNAME("MemberProperty"));
			break;
		case ScriptLanguage::CODE_COMPLETION_KIND_SIGNAL:
			tex = get_editor_theme_icon(SNAME("MemberSignal"));
			break;
		case ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION:
			tex = get_editor_theme_icon(SNAME("MemberMethod"));
			break;
		case ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT:
			tex = get_editor_theme_icon(SNAME("BoxMesh"));
			break;
		default:
			tex = get_editor_theme_icon(SNAME("String"));
			break;
	}
	return tex;
}

void CodeTextEditor::update_editor_settings() {
	// Theme: Highlighting
	completion_font_color = EDITOR_GET("text_editor/theme/highlighting/completion_font_color");
	completion_string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	completion_string_name_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/string_name_color");
	completion_node_path_color = EDITOR_GET("text_editor/theme/highlighting/gdscript/node_path_color");
	completion_comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	completion_doc_comment_color = EDITOR_GET("text_editor/theme/highlighting/doc_comment_color");

	// Appearance: Caret
	text_editor->set_caret_type((TextEdit::CaretType)EDITOR_GET("text_editor/appearance/caret/type").operator int());
	text_editor->set_caret_blink_enabled(EDITOR_GET("text_editor/appearance/caret/caret_blink"));
	text_editor->set_caret_blink_interval(EDITOR_GET("text_editor/appearance/caret/caret_blink_interval"));
	text_editor->set_highlight_current_line(EDITOR_GET("text_editor/appearance/caret/highlight_current_line"));
	text_editor->set_highlight_all_occurrences(EDITOR_GET("text_editor/appearance/caret/highlight_all_occurrences"));

	// Appearance: Gutters
	text_editor->set_draw_line_numbers(EDITOR_GET("text_editor/appearance/gutters/show_line_numbers"));
	text_editor->set_line_numbers_zero_padded(EDITOR_GET("text_editor/appearance/gutters/line_numbers_zero_padded"));

	// Appearance: Minimap
	text_editor->set_draw_minimap(EDITOR_GET("text_editor/appearance/minimap/show_minimap"));
	text_editor->set_minimap_width((int)EDITOR_GET("text_editor/appearance/minimap/minimap_width") * EDSCALE);

	// Appearance: Lines
	text_editor->set_line_folding_enabled(EDITOR_GET("text_editor/appearance/lines/code_folding"));
	text_editor->set_draw_fold_gutter(EDITOR_GET("text_editor/appearance/lines/code_folding"));
	text_editor->set_line_wrapping_mode((TextEdit::LineWrappingMode)EDITOR_GET("text_editor/appearance/lines/word_wrap").operator int());
	text_editor->set_autowrap_mode((TextServer::AutowrapMode)EDITOR_GET("text_editor/appearance/lines/autowrap_mode").operator int());

	// Appearance: Whitespace
	text_editor->set_draw_tabs(EDITOR_GET("text_editor/appearance/whitespace/draw_tabs"));
	text_editor->set_draw_spaces(EDITOR_GET("text_editor/appearance/whitespace/draw_spaces"));
	text_editor->add_theme_constant_override("line_spacing", EDITOR_GET("text_editor/appearance/whitespace/line_spacing"));

	// Behavior: General
	text_editor->set_empty_selection_clipboard_enabled(EDITOR_GET("text_editor/behavior/general/empty_selection_clipboard"));

	// Behavior: Navigation
	text_editor->set_scroll_past_end_of_file_enabled(EDITOR_GET("text_editor/behavior/navigation/scroll_past_end_of_file"));
	text_editor->set_smooth_scroll_enabled(EDITOR_GET("text_editor/behavior/navigation/smooth_scrolling"));
	text_editor->set_v_scroll_speed(EDITOR_GET("text_editor/behavior/navigation/v_scroll_speed"));
	text_editor->set_drag_and_drop_selection_enabled(EDITOR_GET("text_editor/behavior/navigation/drag_and_drop_selection"));
	text_editor->set_use_default_word_separators(EDITOR_GET("text_editor/behavior/navigation/use_default_word_separators"));
	text_editor->set_use_custom_word_separators(EDITOR_GET("text_editor/behavior/navigation/use_custom_word_separators"));
	text_editor->set_custom_word_separators(EDITOR_GET("text_editor/behavior/navigation/custom_word_separators"));

	// Behavior: Indent
	set_indent_using_spaces(EDITOR_GET("text_editor/behavior/indent/type"));
	text_editor->set_indent_size(EDITOR_GET("text_editor/behavior/indent/size"));
	text_editor->set_auto_indent_enabled(EDITOR_GET("text_editor/behavior/indent/auto_indent"));
	text_editor->set_indent_wrapped_lines(EDITOR_GET("text_editor/behavior/indent/indent_wrapped_lines"));

	// Completion
	text_editor->set_auto_brace_completion_enabled(EDITOR_GET("text_editor/completion/auto_brace_complete"));
	text_editor->set_code_hint_draw_below(EDITOR_GET("text_editor/completion/put_callhint_tooltip_below_current_line"));
	code_complete_enabled = EDITOR_GET("text_editor/completion/code_complete_enabled");
	code_complete_timer->set_wait_time(EDITOR_GET("text_editor/completion/code_complete_delay"));
	idle->set_wait_time(EDITOR_GET("text_editor/completion/idle_parse_delay"));

	// Appearance: Guidelines
	if (EDITOR_GET("text_editor/appearance/guidelines/show_line_length_guidelines")) {
		TypedArray<int> guideline_cols;
		guideline_cols.append(EDITOR_GET("text_editor/appearance/guidelines/line_length_guideline_hard_column"));
		if (EDITOR_GET("text_editor/appearance/guidelines/line_length_guideline_soft_column") != guideline_cols[0]) {
			guideline_cols.append(EDITOR_GET("text_editor/appearance/guidelines/line_length_guideline_soft_column"));
		}
		text_editor->set_line_length_guidelines(guideline_cols);
	} else {
		text_editor->set_line_length_guidelines(TypedArray<int>());
	}

	set_zoom_factor(zoom_factor);
}

void CodeTextEditor::set_find_replace_bar(FindReplaceBar *p_bar) {
	if (find_replace_bar) {
		return;
	}

	find_replace_bar = p_bar;
	find_replace_bar->set_text_edit(this);
	find_replace_bar->connect("error", callable_mp(error, &Label::set_text));
}

void CodeTextEditor::remove_find_replace_bar() {
	if (!find_replace_bar) {
		return;
	}

	find_replace_bar->disconnect("error", callable_mp(error, &Label::set_text));
	find_replace_bar = nullptr;
}

void CodeTextEditor::trim_trailing_whitespace() {
	bool trimmed_whitespace = false;
	for (int i = 0; i < text_editor->get_line_count(); i++) {
		String line = text_editor->get_line(i);
		if (line.ends_with(" ") || line.ends_with("\t")) {
			if (!trimmed_whitespace) {
				text_editor->begin_complex_operation();
				trimmed_whitespace = true;
			}

			int end = 0;
			for (int j = line.length() - 1; j > -1; j--) {
				if (line[j] != ' ' && line[j] != '\t') {
					end = j + 1;
					break;
				}
			}
			text_editor->remove_text(i, end, i, line.length());
		}
	}

	if (trimmed_whitespace) {
		text_editor->merge_overlapping_carets();
		text_editor->end_complex_operation();
	}
}

void CodeTextEditor::trim_final_newlines() {
	int final_line = text_editor->get_line_count() - 1;
	int check_line = final_line;

	String line = text_editor->get_line(check_line);

	while (line.is_empty() && check_line > -1) {
		--check_line;

		line = text_editor->get_line(check_line);
	}

	++check_line;

	if (check_line < final_line) {
		text_editor->begin_complex_operation();

		text_editor->remove_text(check_line, 0, final_line, 0);

		text_editor->merge_overlapping_carets();
		text_editor->end_complex_operation();
		text_editor->queue_redraw();
	}
}

void CodeTextEditor::insert_final_newline() {
	int final_line = text_editor->get_line_count() - 1;
	String line = text_editor->get_line(final_line);

	// Length 0 means it's already an empty line, no need to add a newline.
	if (line.length() > 0 && !line.ends_with("\n")) {
		text_editor->insert_text("\n", final_line, line.length(), false);
	}
}

void CodeTextEditor::convert_case(CaseStyle p_case) {
	if (!text_editor->has_selection()) {
		return;
	}
	text_editor->begin_complex_operation();
	text_editor->begin_multicaret_edit();

	for (int c = 0; c < text_editor->get_caret_count(); c++) {
		if (text_editor->multicaret_edit_ignore_caret(c)) {
			continue;
		}
		if (!text_editor->has_selection(c)) {
			continue;
		}

		int begin = text_editor->get_selection_from_line(c);
		int end = text_editor->get_selection_to_line(c);
		int begin_col = text_editor->get_selection_from_column(c);
		int end_col = text_editor->get_selection_to_column(c);

		for (int i = begin; i <= end; i++) {
			int len = text_editor->get_line(i).length();
			if (i == end) {
				len = end_col;
			}
			if (i == begin) {
				len -= begin_col;
			}
			String new_line = text_editor->get_line(i).substr(i == begin ? begin_col : 0, len);

			switch (p_case) {
				case UPPER: {
					new_line = new_line.to_upper();
				} break;
				case LOWER: {
					new_line = new_line.to_lower();
				} break;
				case CAPITALIZE: {
					new_line = new_line.capitalize();
				} break;
			}

			if (i == begin) {
				new_line = text_editor->get_line(i).left(begin_col) + new_line;
			}
			if (i == end) {
				new_line = new_line + text_editor->get_line(i).substr(end_col);
			}
			text_editor->set_line(i, new_line);
		}
	}
	text_editor->end_multicaret_edit();
	text_editor->end_complex_operation();
}

void CodeTextEditor::set_indent_using_spaces(bool p_use_spaces) {
	text_editor->set_indent_using_spaces(p_use_spaces);
	indentation_txt->set_text(p_use_spaces ? TTR("Spaces", "Indentation") : TTR("Tabs", "Indentation"));
}

void CodeTextEditor::toggle_inline_comment(const String &delimiter) {
	text_editor->begin_complex_operation();
	text_editor->begin_multicaret_edit();

	Vector<Point2i> line_ranges = text_editor->get_line_ranges_from_carets();
	int folded_to = 0;
	for (Point2i line_range : line_ranges) {
		int from_line = line_range.x;
		int to_line = line_range.y;
		// If last line is folded, extends to the end of the folded section
		if (text_editor->is_line_folded(to_line)) {
			folded_to = text_editor->get_next_visible_line_offset_from(to_line + 1, 1) - 1;
			to_line += folded_to;
		}
		// Check first if there's any uncommented lines in selection.
		bool is_commented = true;
		bool is_all_empty = true;
		for (int line = from_line; line <= to_line; line++) {
			// `+ delimiter.length()` here because comment delimiter is not actually `in comment` so we check first character after it
			int delimiter_idx = text_editor->is_in_comment(line, text_editor->get_first_non_whitespace_column(line) + delimiter.length());
			// Empty lines should not be counted.
			bool is_empty = text_editor->get_line(line).strip_edges().is_empty();
			is_all_empty = is_all_empty && is_empty;
			// get_delimiter_start_key will return `##` instead of `#` when there is multiple comment delimiter in a line.
			if (!is_empty && (delimiter_idx == -1 || !text_editor->get_delimiter_start_key(delimiter_idx).begins_with(delimiter))) {
				is_commented = false;
				break;
			}
		}

		// Special case for commenting empty lines, treat it/them as uncommented lines.
		is_commented = is_commented && !is_all_empty;

		// Comment/uncomment.
		for (int line = from_line; line <= to_line; line++) {
			if (is_all_empty) {
				text_editor->insert_text(delimiter, line, 0);
				continue;
			}

			if (is_commented) {
				int delimiter_column = text_editor->get_line(line).find(delimiter);
				text_editor->remove_text(line, delimiter_column, line, delimiter_column + delimiter.length());
			} else {
				text_editor->insert_text(delimiter, line, text_editor->get_first_non_whitespace_column(line));
			}
		}
	}

	text_editor->end_multicaret_edit();
	text_editor->end_complex_operation();
}

void CodeTextEditor::goto_line(int p_line, int p_column) {
	text_editor->remove_secondary_carets();
	text_editor->deselect();
	text_editor->unfold_line(CLAMP(p_line, 0, text_editor->get_line_count() - 1));
	text_editor->set_caret_line(p_line, false);
	text_editor->set_caret_column(p_column, false);
	text_editor->set_code_hint("");
	text_editor->cancel_code_completion();
	// Defer in case the CodeEdit was just created and needs to be resized.
	callable_mp((TextEdit *)text_editor, &TextEdit::adjust_viewport_to_caret).call_deferred(0);
}

void CodeTextEditor::goto_line_selection(int p_line, int p_begin, int p_end) {
	text_editor->remove_secondary_carets();
	text_editor->unfold_line(CLAMP(p_line, 0, text_editor->get_line_count() - 1));
	text_editor->select(p_line, p_begin, p_line, p_end);
	text_editor->set_code_hint("");
	text_editor->cancel_code_completion();
	callable_mp((TextEdit *)text_editor, &TextEdit::adjust_viewport_to_caret).call_deferred(0);
}

void CodeTextEditor::goto_line_centered(int p_line, int p_column) {
	text_editor->remove_secondary_carets();
	text_editor->deselect();
	text_editor->unfold_line(CLAMP(p_line, 0, text_editor->get_line_count() - 1));
	text_editor->set_caret_line(p_line, false);
	text_editor->set_caret_column(p_column, false);
	text_editor->set_code_hint("");
	text_editor->cancel_code_completion();
	callable_mp((TextEdit *)text_editor, &TextEdit::center_viewport_to_caret).call_deferred(0);
}

void CodeTextEditor::set_executing_line(int p_line) {
	text_editor->set_line_as_executing(p_line, true);
}

void CodeTextEditor::clear_executing_line() {
	text_editor->clear_executing_lines();
}

Variant CodeTextEditor::get_edit_state() {
	Dictionary state;
	state.merge(get_navigation_state());

	state["folded_lines"] = text_editor->get_folded_lines();
	state["breakpoints"] = text_editor->get_breakpointed_lines();
	state["bookmarks"] = text_editor->get_bookmarked_lines();

	Ref<EditorSyntaxHighlighter> syntax_highlighter = text_editor->get_syntax_highlighter();
	state["syntax_highlighter"] = syntax_highlighter->_get_name();

	return state;
}

Variant CodeTextEditor::get_previous_state() {
	return previous_state;
}

void CodeTextEditor::store_previous_state() {
	previous_state = get_navigation_state();
}

void CodeTextEditor::set_edit_state(const Variant &p_state) {
	Dictionary state = p_state;

	/* update the row first as it sets the column to 0 */
	text_editor->set_caret_line(state["row"]);
	text_editor->set_caret_column(state["column"]);
	if (int(state["scroll_position"]) == -1) {
		// Special case for previous state.
		text_editor->center_viewport_to_caret();
	} else {
		text_editor->set_v_scroll(state["scroll_position"]);
	}
	text_editor->set_h_scroll(state["h_scroll_position"]);

	if (state.get("selection", false)) {
		text_editor->select(state["selection_from_line"], state["selection_from_column"], state["selection_to_line"], state["selection_to_column"]);
	} else {
		text_editor->deselect();
	}

	if (state.has("folded_lines")) {
		Vector<int> folded_lines = state["folded_lines"];
		for (int i = 0; i < folded_lines.size(); i++) {
			text_editor->fold_line(folded_lines[i]);
		}
	}

	if (state.has("breakpoints")) {
		Array breakpoints = state["breakpoints"];
		for (int i = 0; i < breakpoints.size(); i++) {
			text_editor->set_line_as_breakpoint(breakpoints[i], true);
		}
	}

	if (state.has("bookmarks")) {
		Array bookmarks = state["bookmarks"];
		for (int i = 0; i < bookmarks.size(); i++) {
			text_editor->set_line_as_bookmarked(bookmarks[i], true);
		}
	}

	if (previous_state.is_empty()) {
		previous_state = p_state;
	}
}

Variant CodeTextEditor::get_navigation_state() {
	Dictionary state;

	state["scroll_position"] = text_editor->get_v_scroll();
	state["h_scroll_position"] = text_editor->get_h_scroll();
	state["column"] = text_editor->get_caret_column();
	state["row"] = text_editor->get_caret_line();

	state["selection"] = get_text_editor()->has_selection();
	if (get_text_editor()->has_selection()) {
		state["selection_from_line"] = text_editor->get_selection_from_line();
		state["selection_from_column"] = text_editor->get_selection_from_column();
		state["selection_to_line"] = text_editor->get_selection_to_line();
		state["selection_to_column"] = text_editor->get_selection_to_column();
	}

	return state;
}

void CodeTextEditor::set_error(const String &p_error) {
	error->set_text(p_error);
	if (!p_error.is_empty()) {
		error->set_default_cursor_shape(CURSOR_POINTING_HAND);
	} else {
		error->set_default_cursor_shape(CURSOR_ARROW);
	}
}

void CodeTextEditor::set_error_pos(int p_line, int p_column) {
	error_line = p_line;
	error_column = p_column;
}

Point2i CodeTextEditor::get_error_pos() const {
	return Point2i(error_line, error_column);
}

void CodeTextEditor::goto_error() {
	if (!error->get_text().is_empty()) {
		int corrected_column = error_column;

		const String line_text = text_editor->get_line(error_line);
		const int indent_size = text_editor->get_indent_size();
		if (indent_size > 1) {
			const int tab_count = line_text.length() - line_text.lstrip("\t").length();
			corrected_column -= tab_count * (indent_size - 1);
		}

		goto_line_centered(error_line, corrected_column);
	}
}

void CodeTextEditor::_update_text_editor_theme() {
	emit_signal(SNAME("load_theme_settings"));

	error_button->set_icon(get_editor_theme_icon(SNAME("StatusError")));
	warning_button->set_icon(get_editor_theme_icon(SNAME("NodeWarning")));

	Ref<Font> status_bar_font = get_theme_font(SNAME("status_source"), EditorStringName(EditorFonts));
	int status_bar_font_size = get_theme_font_size(SNAME("status_source_size"), EditorStringName(EditorFonts));

	int count = status_bar->get_child_count();
	for (int i = 0; i < count; i++) {
		Control *n = Object::cast_to<Control>(status_bar->get_child(i));
		if (n) {
			n->add_theme_font_override(SceneStringName(font), status_bar_font);
			n->add_theme_font_size_override(SceneStringName(font_size), status_bar_font_size);
		}
	}

	const Color &error_color = get_theme_color(SNAME("error_color"), EditorStringName(Editor));
	const Color &warning_color = get_theme_color(SNAME("warning_color"), EditorStringName(Editor));

	error->add_theme_color_override(SceneStringName(font_color), error_color);
	error_button->add_theme_color_override(SceneStringName(font_color), error_color);
	warning_button->add_theme_color_override(SceneStringName(font_color), warning_color);

	_update_font_ligatures();
}

void CodeTextEditor::_update_font_ligatures() {
	int ot_mode = EDITOR_GET("interface/editor/code_font_contextual_ligatures");

	Ref<FontVariation> fc = text_editor->get_theme_font(SceneStringName(font));
	if (fc.is_valid()) {
		switch (ot_mode) {
			case 1: { // Disable ligatures.
				Dictionary ftrs;
				ftrs[TS->name_to_tag("calt")] = 0;
				fc->set_opentype_features(ftrs);
			} break;
			case 2: { // Custom.
				Vector<String> subtag = String(EDITOR_GET("interface/editor/code_font_custom_opentype_features")).split(",");
				Dictionary ftrs;
				for (int i = 0; i < subtag.size(); i++) {
					Vector<String> subtag_a = subtag[i].split("=");
					if (subtag_a.size() == 2) {
						ftrs[TS->name_to_tag(subtag_a[0])] = subtag_a[1].to_int();
					} else if (subtag_a.size() == 1) {
						ftrs[TS->name_to_tag(subtag_a[0])] = 1;
					}
				}
				fc->set_opentype_features(ftrs);
			} break;
			default: { // Enabled.
				Dictionary ftrs;
				ftrs[TS->name_to_tag("calt")] = 1;
				fc->set_opentype_features(ftrs);
			} break;
		}
	}
}

void CodeTextEditor::_text_changed_idle_timeout() {
	_validate_script();
	emit_signal(SNAME("validate_script"));
}

void CodeTextEditor::validate_script() {
	idle->start();
}

void CodeTextEditor::_error_button_pressed() {
	_set_show_errors_panel(!is_errors_panel_opened);
	_set_show_warnings_panel(false);
}

void CodeTextEditor::_warning_button_pressed() {
	_set_show_warnings_panel(!is_warnings_panel_opened);
	_set_show_errors_panel(false);
}

void CodeTextEditor::_zoom_popup_id_pressed(int p_idx) {
	_zoom_to(zoom_button->get_popup()->get_item_metadata(p_idx));
}

void CodeTextEditor::_set_show_errors_panel(bool p_show) {
	is_errors_panel_opened = p_show;
	emit_signal(SNAME("show_errors_panel"), p_show);
}

void CodeTextEditor::_set_show_warnings_panel(bool p_show) {
	is_warnings_panel_opened = p_show;
	emit_signal(SNAME("show_warnings_panel"), p_show);
}

void CodeTextEditor::_toggle_scripts_pressed() {
	ERR_FAIL_NULL(toggle_scripts_list);
	toggle_scripts_list->set_visible(!toggle_scripts_list->is_visible());
	update_toggle_scripts_button();
}

void CodeTextEditor::_error_pressed(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		goto_error();
	}
}

void CodeTextEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			if (toggle_scripts_button->is_visible()) {
				update_toggle_scripts_button();
			}
			_update_text_editor_theme();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (toggle_scripts_button->is_visible()) {
				update_toggle_scripts_button();
			}
			set_process_input(is_visible_in_tree());
		} break;

		case NOTIFICATION_PREDELETE: {
			if (find_replace_bar) {
				find_replace_bar->set_text_edit(nullptr);
			}
		} break;
	}
}

void CodeTextEditor::set_error_count(int p_error_count) {
	error_button->set_text(itos(p_error_count));
	error_button->set_visible(p_error_count > 0);
	if (!p_error_count) {
		_set_show_errors_panel(false);
	}
}

void CodeTextEditor::set_warning_count(int p_warning_count) {
	warning_button->set_text(itos(p_warning_count));
	warning_button->set_visible(p_warning_count > 0);
	if (!p_warning_count) {
		_set_show_warnings_panel(false);
	}
}

void CodeTextEditor::toggle_bookmark() {
	Vector<int> sorted_carets = text_editor->get_sorted_carets();
	int last_line = -1;
	for (const int &c : sorted_carets) {
		int from = text_editor->get_selection_from_line(c);
		from += from == last_line ? 1 : 0;
		int to = text_editor->get_selection_to_line(c);
		if (to < from) {
			continue;
		}
		// Check first if there's any bookmarked lines in the selection.
		bool selection_has_bookmarks = false;
		for (int line = from; line <= to; line++) {
			if (text_editor->is_line_bookmarked(line)) {
				selection_has_bookmarks = true;
				break;
			}
		}

		// Set bookmark on caret or remove all bookmarks from the selection.
		if (!selection_has_bookmarks) {
			if (text_editor->get_caret_line(c) != last_line) {
				text_editor->set_line_as_bookmarked(text_editor->get_caret_line(c), true);
			}
		} else {
			for (int line = from; line <= to; line++) {
				text_editor->set_line_as_bookmarked(line, false);
			}
		}
		last_line = to;
	}
}

void CodeTextEditor::goto_next_bookmark() {
	PackedInt32Array bmarks = text_editor->get_bookmarked_lines();
	if (bmarks.size() <= 0) {
		return;
	}

	int current_line = text_editor->get_caret_line();
	int bmark_idx = 0;
	if (current_line < (int)bmarks[bmarks.size() - 1]) {
		while (bmark_idx < bmarks.size() && bmarks[bmark_idx] <= current_line) {
			bmark_idx++;
		}
	}
	goto_line_centered(bmarks[bmark_idx]);
}

void CodeTextEditor::goto_prev_bookmark() {
	PackedInt32Array bmarks = text_editor->get_bookmarked_lines();
	if (bmarks.size() <= 0) {
		return;
	}

	int current_line = text_editor->get_caret_line();
	int bmark_idx = bmarks.size() - 1;
	if (current_line > (int)bmarks[0]) {
		while (bmark_idx >= 0 && bmarks[bmark_idx] >= current_line) {
			bmark_idx--;
		}
	}
	goto_line_centered(bmarks[bmark_idx]);
}

void CodeTextEditor::remove_all_bookmarks() {
	text_editor->clear_bookmarked_lines();
}

void CodeTextEditor::_zoom_in() {
	int s = text_editor->get_theme_font_size(SceneStringName(font_size));
	_zoom_to(zoom_factor * (s + MAX(1.0f, EDSCALE)) / s);
}

void CodeTextEditor::_zoom_out() {
	int s = text_editor->get_theme_font_size(SceneStringName(font_size));
	_zoom_to(zoom_factor * (s - MAX(1.0f, EDSCALE)) / s);
}

void CodeTextEditor::_zoom_to(float p_zoom_factor) {
	if (zoom_factor == p_zoom_factor) {
		return;
	}

	float old_zoom_factor = zoom_factor;

	set_zoom_factor(p_zoom_factor);

	if (old_zoom_factor != zoom_factor) {
		emit_signal(SNAME("zoomed"), zoom_factor);
	}
}

void CodeTextEditor::set_zoom_factor(float p_zoom_factor) {
	zoom_factor = CLAMP(p_zoom_factor, 0.25f, 3.0f);
	int neutral_font_size = int(EDITOR_GET("interface/editor/code_font_size")) * EDSCALE;
	int new_font_size = Math::round(zoom_factor * neutral_font_size);

	zoom_button->set_text(itos(Math::round(zoom_factor * 100)) + " %");

	if (text_editor->has_theme_font_size_override(SceneStringName(font_size))) {
		text_editor->remove_theme_font_size_override(SceneStringName(font_size));
	}
	text_editor->add_theme_font_size_override(SceneStringName(font_size), new_font_size);
}

float CodeTextEditor::get_zoom_factor() {
	return zoom_factor;
}

void CodeTextEditor::_bind_methods() {
	ADD_SIGNAL(MethodInfo("validate_script"));
	ADD_SIGNAL(MethodInfo("load_theme_settings"));
	ADD_SIGNAL(MethodInfo("show_errors_panel"));
	ADD_SIGNAL(MethodInfo("show_warnings_panel"));
	ADD_SIGNAL(MethodInfo("zoomed", PropertyInfo(Variant::FLOAT, "p_zoom_factor")));
}

void CodeTextEditor::set_code_complete_func(CodeTextEditorCodeCompleteFunc p_code_complete_func, void *p_ud) {
	code_complete_func = p_code_complete_func;
	code_complete_ud = p_ud;
}

void CodeTextEditor::set_toggle_list_control(Control *p_control) {
	toggle_scripts_list = p_control;
}

void CodeTextEditor::show_toggle_scripts_button() {
	toggle_scripts_button->show();
}

void CodeTextEditor::update_toggle_scripts_button() {
	ERR_FAIL_NULL(toggle_scripts_list);
	bool forward = toggle_scripts_list->is_visible() == is_layout_rtl();
	toggle_scripts_button->set_icon(get_editor_theme_icon(forward ? SNAME("Forward") : SNAME("Back")));
	toggle_scripts_button->set_tooltip_text(vformat("%s (%s)", TTR("Toggle Scripts Panel"), ED_GET_SHORTCUT("script_editor/toggle_scripts_panel")->get_as_text()));
}

CodeTextEditor::CodeTextEditor() {
	code_complete_func = nullptr;
	ED_SHORTCUT("script_editor/zoom_in", TTR("Zoom In"), KeyModifierMask::CMD_OR_CTRL | Key::EQUAL);
	ED_SHORTCUT("script_editor/zoom_out", TTR("Zoom Out"), KeyModifierMask::CMD_OR_CTRL | Key::MINUS);
	ED_SHORTCUT_ARRAY("script_editor/reset_zoom", TTR("Reset Zoom"),
			{ int32_t(KeyModifierMask::CMD_OR_CTRL | Key::KEY_0), int32_t(KeyModifierMask::CMD_OR_CTRL | Key::KP_0) });

	text_editor = memnew(CodeEdit);
	add_child(text_editor);
	text_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	text_editor->set_structured_text_bidi_override(TextServer::STRUCTURED_TEXT_GDSCRIPT);
	text_editor->set_draw_bookmarks_gutter(true);

	text_editor->set_draw_line_numbers(true);
	text_editor->set_highlight_matching_braces_enabled(true);
	text_editor->set_auto_indent_enabled(true);
	text_editor->set_deselect_on_focus_loss_enabled(false);

	status_bar = memnew(HBoxContainer);
	add_child(status_bar);
	status_bar->set_h_size_flags(SIZE_EXPAND_FILL);
	status_bar->set_custom_minimum_size(Size2(0, 24 * EDSCALE)); // Adjust for the height of the warning icon.

	idle = memnew(Timer);
	add_child(idle);
	idle->set_one_shot(true);

	code_complete_enabled = EDITOR_GET("text_editor/completion/code_complete_enabled");
	code_complete_timer = memnew(Timer);
	add_child(code_complete_timer);
	code_complete_timer->set_one_shot(true);

	error_line = 0;
	error_column = 0;

	toggle_scripts_button = memnew(Button);
	toggle_scripts_button->set_flat(true);
	toggle_scripts_button->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	toggle_scripts_button->connect(SceneStringName(pressed), callable_mp(this, &CodeTextEditor::_toggle_scripts_pressed));
	status_bar->add_child(toggle_scripts_button);
	toggle_scripts_button->hide();

	// Error
	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	scroll->set_vertical_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	status_bar->add_child(scroll);

	error = memnew(Label);
	scroll->add_child(error);
	error->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	error->set_mouse_filter(MOUSE_FILTER_STOP);
	error->connect(SceneStringName(gui_input), callable_mp(this, &CodeTextEditor::_error_pressed));

	// Errors
	error_button = memnew(Button);
	error_button->set_flat(true);
	status_bar->add_child(error_button);
	error_button->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	error_button->set_default_cursor_shape(CURSOR_POINTING_HAND);
	error_button->connect(SceneStringName(pressed), callable_mp(this, &CodeTextEditor::_error_button_pressed));
	error_button->set_tooltip_text(TTR("Errors"));
	set_error_count(0);

	// Warnings
	warning_button = memnew(Button);
	warning_button->set_flat(true);
	status_bar->add_child(warning_button);
	warning_button->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	warning_button->set_default_cursor_shape(CURSOR_POINTING_HAND);
	warning_button->connect(SceneStringName(pressed), callable_mp(this, &CodeTextEditor::_warning_button_pressed));
	warning_button->set_tooltip_text(TTR("Warnings"));
	set_warning_count(0);

	status_bar->add_child(memnew(VSeparator));

	// Zoom
	zoom_button = memnew(MenuButton);
	status_bar->add_child(zoom_button);
	zoom_button->set_flat(true);
	zoom_button->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	zoom_button->set_tooltip_text(
			TTR("Zoom factor") + "\n" + vformat(TTR("%sMouse wheel, %s/%s: Finetune\n%s: Reset"), keycode_get_string((Key)KeyModifierMask::CMD_OR_CTRL), ED_GET_SHORTCUT("script_editor/zoom_in")->get_as_text(), ED_GET_SHORTCUT("script_editor/zoom_out")->get_as_text(), ED_GET_SHORTCUT("script_editor/reset_zoom")->get_as_text()));
	zoom_button->set_text("100 %");

	PopupMenu *zoom_menu = zoom_button->get_popup();
	int preset_count = sizeof(ZOOM_FACTOR_PRESETS) / sizeof(float);

	for (int i = 0; i < preset_count; i++) {
		float z = ZOOM_FACTOR_PRESETS[i];
		zoom_menu->add_item(itos(Math::round(z * 100)) + " %");
		zoom_menu->set_item_metadata(i, z);
	}

	zoom_menu->connect(SceneStringName(id_pressed), callable_mp(this, &CodeTextEditor::_zoom_popup_id_pressed));

	status_bar->add_child(memnew(VSeparator));

	// Line and column
	line_and_col_txt = memnew(Label);
	status_bar->add_child(line_and_col_txt);
	line_and_col_txt->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	line_and_col_txt->set_tooltip_text(TTR("Line and column numbers."));
	line_and_col_txt->set_mouse_filter(MOUSE_FILTER_STOP);

	status_bar->add_child(memnew(VSeparator));

	// Indentation
	indentation_txt = memnew(Label);
	status_bar->add_child(indentation_txt);
	indentation_txt->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	indentation_txt->set_tooltip_text(TTR("Indentation"));
	indentation_txt->set_mouse_filter(MOUSE_FILTER_STOP);

	text_editor->connect(SceneStringName(gui_input), callable_mp(this, &CodeTextEditor::_text_editor_gui_input));
	text_editor->connect("caret_changed", callable_mp(this, &CodeTextEditor::_line_col_changed));
	text_editor->connect(SceneStringName(text_changed), callable_mp(this, &CodeTextEditor::_text_changed));
	text_editor->connect("code_completion_requested", callable_mp(this, &CodeTextEditor::_complete_request));
	TypedArray<String> cs;
	cs.push_back(".");
	cs.push_back(",");
	cs.push_back("(");
	cs.push_back("=");
	cs.push_back("$");
	cs.push_back("@");
	cs.push_back("\"");
	cs.push_back("\'");
	text_editor->set_code_completion_prefixes(cs);
	idle->connect("timeout", callable_mp(this, &CodeTextEditor::_text_changed_idle_timeout));

	code_complete_timer->connect("timeout", callable_mp(this, &CodeTextEditor::_code_complete_timer_timeout));

	add_theme_constant_override("separation", 4 * EDSCALE);
}
