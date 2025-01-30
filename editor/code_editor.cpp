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

#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/string_builder.h"
#include "editor/editor_scale.h"
#include "editor_node.h"
#include "editor_settings.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/separator.h"
#include "scene/resources/dynamic_font.h"

void GotoLineDialog::popup_find_line(TextEdit *p_edit) {
	text_editor = p_edit;

	line->set_text(itos(text_editor->cursor_get_line()));
	line->select_all();
	popup_centered(Size2(180, 80) * EDSCALE);
	line->grab_focus();
}

int GotoLineDialog::get_line() const {
	return line->get_text().to_int();
}

void GotoLineDialog::ok_pressed() {
	if (get_line() < 1 || get_line() > text_editor->get_line_count()) {
		return;
	}
	text_editor->unfold_line(get_line() - 1);
	text_editor->cursor_set_line(get_line() - 1);
	hide();
}

GotoLineDialog::GotoLineDialog() {
	set_title(TTR("Go to Line"));

	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_margin(MARGIN_TOP, ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, -8 * EDSCALE);
	vbc->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, -8 * EDSCALE);
	add_child(vbc);

	Label *l = memnew(Label);
	l->set_text(TTR("Line Number:"));
	vbc->add_child(l);

	line = memnew(LineEdit);
	vbc->add_child(line);
	register_text_enter(line);
	text_editor = nullptr;

	set_hide_on_ok(false);
}

void FindReplaceBar::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		find_prev->set_icon(get_icon("MoveUp", "EditorIcons"));
		find_next->set_icon(get_icon("MoveDown", "EditorIcons"));
		hide_button->set_normal_texture(get_icon("Close", "EditorIcons"));
		hide_button->set_hover_texture(get_icon("Close", "EditorIcons"));
		hide_button->set_pressed_texture(get_icon("Close", "EditorIcons"));
		hide_button->set_custom_minimum_size(hide_button->get_normal_texture()->get_size());
	} else if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		set_process_unhandled_input(is_visible_in_tree());
	} else if (p_what == EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED) {
		find_prev->set_icon(get_icon("MoveUp", "EditorIcons"));
		find_next->set_icon(get_icon("MoveDown", "EditorIcons"));
		hide_button->set_normal_texture(get_icon("Close", "EditorIcons"));
		hide_button->set_hover_texture(get_icon("Close", "EditorIcons"));
		hide_button->set_pressed_texture(get_icon("Close", "EditorIcons"));
		hide_button->set_custom_minimum_size(hide_button->get_normal_texture()->get_size());
	} else if (p_what == NOTIFICATION_THEME_CHANGED) {
		matches_label->add_color_override("font_color", results_count > 0 ? get_color("font_color", "Label") : get_color("error_color", "Editor"));
	}
}

void FindReplaceBar::_unhandled_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	if (!k.is_valid() || !k->is_pressed()) {
		return;
	}

	Control *focus_owner = get_focus_owner();
	if (text_edit->has_focus() || (focus_owner && vbc_lineedit->is_a_parent_of(focus_owner))) {
		bool accepted = true;

		switch (k->get_scancode()) {
			case KEY_ESCAPE: {
				_hide_bar();
			} break;
			default: {
				accepted = false;
			} break;
		}

		if (accepted) {
			accept_event();
		}
	}
}

bool FindReplaceBar::_search(uint32_t p_flags, int p_from_line, int p_from_col) {
	int line, col;
	String text = get_search_text();

	bool found = text_edit->search(text, p_flags, p_from_line, p_from_col, line, col);

	if (found) {
		if (!preserve_cursor && !is_selection_only()) {
			text_edit->unfold_line(line);
			text_edit->cursor_set_line(line, false);
			text_edit->cursor_set_column(col + text.length(), false);
			text_edit->center_viewport_to_cursor();
			text_edit->select(line, col, line, col + text.length());
		}

		text_edit->set_search_text(text);
		text_edit->set_search_flags(p_flags);
		text_edit->set_current_search_result(line, col);

		result_line = line;
		result_col = col;

		_update_results_count();
	} else {
		results_count = 0;
		result_line = -1;
		result_col = -1;
		text_edit->set_search_text("");
		text_edit->set_search_flags(p_flags);
		text_edit->set_current_search_result(line, col);
	}

	_update_matches_label();

	return found;
}

void FindReplaceBar::_replace() {
	bool selection_enabled = text_edit->is_selection_active();
	Point2i selection_begin, selection_end;
	if (selection_enabled) {
		selection_begin = Point2i(text_edit->get_selection_from_line(), text_edit->get_selection_from_column());
		selection_end = Point2i(text_edit->get_selection_to_line(), text_edit->get_selection_to_column());
	}

	String replace_text = get_replace_text();
	int search_text_len = get_search_text().length();

	text_edit->begin_complex_operation();
	if (selection_enabled && is_selection_only()) { // To restrict search_current() to selected region
		text_edit->cursor_set_line(selection_begin.width);
		text_edit->cursor_set_column(selection_begin.height);
	}

	if (search_current()) {
		text_edit->unfold_line(result_line);
		text_edit->select(result_line, result_col, result_line, result_col + search_text_len);

		if (selection_enabled && is_selection_only()) {
			Point2i match_from(result_line, result_col);
			Point2i match_to(result_line, result_col + search_text_len);
			if (!(match_from < selection_begin || match_to > selection_end)) {
				text_edit->insert_text_at_cursor(replace_text);
				if (match_to.x == selection_end.x) { // Adjust selection bounds if necessary
					selection_end.y += replace_text.length() - search_text_len;
				}
			}
		} else {
			text_edit->insert_text_at_cursor(replace_text);
		}
	}
	text_edit->end_complex_operation();
	results_count = -1;

	if (selection_enabled && is_selection_only()) {
		// Reselect in order to keep 'Replace' restricted to selection
		text_edit->select(selection_begin.x, selection_begin.y, selection_end.x, selection_end.y);
	} else {
		text_edit->deselect();
	}
}

void FindReplaceBar::_replace_all() {
	text_edit->disconnect("text_changed", this, "_editor_text_changed");
	// Line as x so it gets priority in comparison, column as y.
	Point2i orig_cursor(text_edit->cursor_get_line(), text_edit->cursor_get_column());
	Point2i prev_match = Point2(-1, -1);

	bool selection_enabled = text_edit->is_selection_active();
	Point2i selection_begin, selection_end;
	if (selection_enabled) {
		selection_begin = Point2i(text_edit->get_selection_from_line(), text_edit->get_selection_from_column());
		selection_end = Point2i(text_edit->get_selection_to_line(), text_edit->get_selection_to_column());
	}

	int vsval = text_edit->get_v_scroll();

	text_edit->cursor_set_line(0);
	text_edit->cursor_set_column(0);

	String replace_text = get_replace_text();
	int search_text_len = get_search_text().length();

	int rc = 0;

	replace_all_mode = true;

	text_edit->begin_complex_operation();

	if (selection_enabled && is_selection_only()) {
		text_edit->cursor_set_line(selection_begin.width);
		text_edit->cursor_set_column(selection_begin.height);
	}
	if (search_current()) {
		do {
			// replace area
			Point2i match_from(result_line, result_col);
			Point2i match_to(result_line, result_col + search_text_len);

			if (match_from < prev_match) {
				break; // Done.
			}

			prev_match = Point2i(result_line, result_col + replace_text.length());

			text_edit->unfold_line(result_line);
			text_edit->select(result_line, result_col, result_line, match_to.y);

			if (selection_enabled && is_selection_only()) {
				if (match_from < selection_begin || match_to > selection_end) {
					break; // Done.
				}

				// Replace but adjust selection bounds.
				text_edit->insert_text_at_cursor(replace_text);
				if (match_to.x == selection_end.x) {
					selection_end.y += replace_text.length() - search_text_len;
				}

			} else {
				// Just replace.
				text_edit->insert_text_at_cursor(replace_text);
			}

			rc++;
		} while (search_next());
	}

	text_edit->end_complex_operation();

	replace_all_mode = false;

	// Restore editor state (selection, cursor, scroll).
	text_edit->cursor_set_line(orig_cursor.x);
	text_edit->cursor_set_column(orig_cursor.y);

	if (selection_enabled && is_selection_only()) {
		// Reselect.
		text_edit->select(selection_begin.x, selection_begin.y, selection_end.x, selection_end.y);
	} else {
		text_edit->deselect();
	}

	text_edit->set_v_scroll(vsval);
	matches_label->add_color_override("font_color", rc > 0 ? get_color("font_color", "Label") : get_color("error_color", "Editor"));
	matches_label->set_text(vformat(TTR("%d replaced."), rc));

	text_edit->call_deferred("connect", "text_changed", this, "_editor_text_changed");
	results_count = -1;
}

void FindReplaceBar::_get_search_from(int &r_line, int &r_col) {
	r_line = text_edit->cursor_get_line();
	r_col = text_edit->cursor_get_column();

	if (text_edit->is_selection_active() && is_selection_only()) {
		return;
	}

	if (r_line == result_line && r_col >= result_col && r_col <= result_col + get_search_text().length()) {
		r_col = result_col;
	}
}

void FindReplaceBar::_update_results_count() {
	if (results_count != -1) {
		return;
	}

	results_count = 0;

	String searched = get_search_text();
	if (searched.empty()) {
		return;
	}

	String full_text = text_edit->get_text();

	int from_pos = 0;

	while (true) {
		int pos = is_case_sensitive() ? full_text.find(searched, from_pos) : full_text.findn(searched, from_pos);
		if (pos == -1) {
			break;
		}

		int pos_subsequent = pos + searched.length();
		if (is_whole_words()) {
			from_pos = pos + 1; // Making sure we won't hit the same match next time, if we get out via a continue.
			if (pos > 0 && !(is_symbol(full_text[pos - 1]) || full_text[pos - 1] == '\n')) {
				continue;
			}
			if (pos_subsequent < full_text.length() && !(is_symbol(full_text[pos_subsequent]) || full_text[pos_subsequent] == '\n')) {
				continue;
			}
		}

		results_count++;
		from_pos = pos_subsequent;
	}
}

void FindReplaceBar::_update_matches_label() {
	if (search_text->get_text().empty() || results_count == -1) {
		matches_label->hide();
	} else {
		matches_label->show();

		matches_label->add_color_override("font_color", results_count > 0 ? get_color("font_color", "Label") : get_color("error_color", "Editor"));
		matches_label->set_text(vformat(results_count == 1 ? TTR("%d match.") : TTR("%d matches."), results_count));
	}
}

bool FindReplaceBar::search_current() {
	uint32_t flags = 0;

	if (is_whole_words()) {
		flags |= TextEdit::SEARCH_WHOLE_WORDS;
	}
	if (is_case_sensitive()) {
		flags |= TextEdit::SEARCH_MATCH_CASE;
	}

	int line, col;
	_get_search_from(line, col);

	return _search(flags, line, col);
}

bool FindReplaceBar::search_prev() {
	if (!is_visible()) {
		popup_search(true);
	}

	uint32_t flags = 0;
	String text = get_search_text();

	if (is_whole_words()) {
		flags |= TextEdit::SEARCH_WHOLE_WORDS;
	}
	if (is_case_sensitive()) {
		flags |= TextEdit::SEARCH_MATCH_CASE;
	}

	flags |= TextEdit::SEARCH_BACKWARDS;

	int line, col;
	_get_search_from(line, col);
	if (text_edit->is_selection_active()) {
		col--; // Skip currently selected word.
	}

	col -= text.length();
	if (col < 0) {
		line -= 1;
		if (line < 0) {
			line = text_edit->get_line_count() - 1;
		}
		col = text_edit->get_line(line).length();
	}

	return _search(flags, line, col);
}

bool FindReplaceBar::search_next() {
	if (!is_visible()) {
		popup_search(true);
	}

	uint32_t flags = 0;
	String text;
	if (replace_all_mode) {
		text = get_replace_text();
	} else {
		text = get_search_text();
	}

	if (is_whole_words()) {
		flags |= TextEdit::SEARCH_WHOLE_WORDS;
	}
	if (is_case_sensitive()) {
		flags |= TextEdit::SEARCH_MATCH_CASE;
	}

	int line, col;
	_get_search_from(line, col);

	if (line == result_line && col == result_col) {
		col += text.length();
		if (col > text_edit->get_line(line).length()) {
			line += 1;
			if (line >= text_edit->get_line_count()) {
				line = 0;
			}
			col = 0;
		}
	}

	return _search(flags, line, col);
}

void FindReplaceBar::_hide_bar() {
	if (replace_text->has_focus() || search_text->has_focus()) {
		text_edit->grab_focus();
	}

	text_edit->set_search_text("");
	result_line = -1;
	result_col = -1;
	hide();
}

void FindReplaceBar::_show_search(bool p_focus_replace, bool p_show_only) {
	show();
	if (p_show_only) {
		return;
	}

	if (p_focus_replace) {
		search_text->deselect();
		replace_text->call_deferred("grab_focus");
	} else {
		replace_text->deselect();
		search_text->call_deferred("grab_focus");
	}

	if (text_edit->is_selection_active() && !selection_only->is_pressed()) {
		search_text->set_text(text_edit->get_selection_text());
	}

	if (!get_search_text().empty()) {
		if (p_focus_replace) {
			replace_text->select_all();
			replace_text->set_cursor_position(replace_text->get_text().length());
		} else {
			search_text->select_all();
			search_text->set_cursor_position(search_text->get_text().length());
		}

		results_count = -1;
		_update_results_count();
		_update_matches_label();
	}
}

void FindReplaceBar::popup_search(bool p_show_only) {
	replace_text->hide();
	hbc_button_replace->hide();
	hbc_option_replace->hide();

	_show_search(false, p_show_only);
}

void FindReplaceBar::popup_replace() {
	if (!replace_text->is_visible_in_tree()) {
		replace_text->show();
		hbc_button_replace->show();
		hbc_option_replace->show();
	}

	selection_only->set_pressed((text_edit->is_selection_active() && text_edit->get_selection_from_line() < text_edit->get_selection_to_line()));

	_show_search(is_visible() || text_edit->is_selection_active());
}

void FindReplaceBar::_search_options_changed(bool p_pressed) {
	results_count = -1;
	search_current();
}

void FindReplaceBar::_editor_text_changed() {
	results_count = -1;
	if (is_visible_in_tree()) {
		preserve_cursor = true;
		search_current();
		preserve_cursor = false;
	}
}

void FindReplaceBar::_search_text_changed(const String &p_text) {
	results_count = -1;
	search_current();
}

void FindReplaceBar::_search_text_entered(const String &p_text) {
	if (Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
		search_prev();
	} else {
		search_next();
	}
}

void FindReplaceBar::_replace_text_entered(const String &p_text) {
	if (selection_only->is_pressed() && text_edit->is_selection_active()) {
		_replace_all();
		_hide_bar();
	}
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
	emit_signal("error", p_label);
}

void FindReplaceBar::set_text_edit(TextEdit *p_text_edit) {
	results_count = -1;
	text_edit = p_text_edit;
	text_edit->connect("text_changed", this, "_editor_text_changed");
}

void FindReplaceBar::_bind_methods() {
	ClassDB::bind_method("_unhandled_input", &FindReplaceBar::_unhandled_input);

	ClassDB::bind_method("_editor_text_changed", &FindReplaceBar::_editor_text_changed);
	ClassDB::bind_method("_search_text_changed", &FindReplaceBar::_search_text_changed);
	ClassDB::bind_method("_search_text_entered", &FindReplaceBar::_search_text_entered);
	ClassDB::bind_method("_replace_text_entered", &FindReplaceBar::_replace_text_entered);
	ClassDB::bind_method("_search_current", &FindReplaceBar::search_current);
	ClassDB::bind_method("_search_next", &FindReplaceBar::search_next);
	ClassDB::bind_method("_search_prev", &FindReplaceBar::search_prev);
	ClassDB::bind_method("_replace_pressed", &FindReplaceBar::_replace);
	ClassDB::bind_method("_replace_all_pressed", &FindReplaceBar::_replace_all);
	ClassDB::bind_method("_search_options_changed", &FindReplaceBar::_search_options_changed);
	ClassDB::bind_method("_hide_pressed", &FindReplaceBar::_hide_bar);

	ADD_SIGNAL(MethodInfo("search"));
	ADD_SIGNAL(MethodInfo("error"));
}

FindReplaceBar::FindReplaceBar() {
	results_count = -1;
	replace_all_mode = false;
	preserve_cursor = false;

	vbc_lineedit = memnew(VBoxContainer);
	add_child(vbc_lineedit);
	vbc_lineedit->set_alignment(ALIGN_CENTER);
	vbc_lineedit->set_h_size_flags(SIZE_EXPAND_FILL);
	VBoxContainer *vbc_button = memnew(VBoxContainer);
	add_child(vbc_button);
	VBoxContainer *vbc_option = memnew(VBoxContainer);
	add_child(vbc_option);

	HBoxContainer *hbc_button_search = memnew(HBoxContainer);
	vbc_button->add_child(hbc_button_search);
	hbc_button_search->set_alignment(ALIGN_END);
	hbc_button_replace = memnew(HBoxContainer);
	vbc_button->add_child(hbc_button_replace);
	hbc_button_replace->set_alignment(ALIGN_END);

	HBoxContainer *hbc_option_search = memnew(HBoxContainer);
	vbc_option->add_child(hbc_option_search);
	hbc_option_replace = memnew(HBoxContainer);
	vbc_option->add_child(hbc_option_replace);

	// search toolbar
	search_text = memnew(LineEdit);
	vbc_lineedit->add_child(search_text);
	search_text->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	search_text->connect("text_changed", this, "_search_text_changed");
	search_text->connect("text_entered", this, "_search_text_entered");

	matches_label = memnew(Label);
	hbc_button_search->add_child(matches_label);
	matches_label->hide();

	find_prev = memnew(ToolButton);
	hbc_button_search->add_child(find_prev);
	find_prev->set_focus_mode(FOCUS_NONE);
	find_prev->connect("pressed", this, "_search_prev");

	find_next = memnew(ToolButton);
	hbc_button_search->add_child(find_next);
	find_next->set_focus_mode(FOCUS_NONE);
	find_next->connect("pressed", this, "_search_next");

	case_sensitive = memnew(CheckBox);
	hbc_option_search->add_child(case_sensitive);
	case_sensitive->set_text(TTR("Match Case"));
	case_sensitive->set_focus_mode(FOCUS_NONE);
	case_sensitive->connect("toggled", this, "_search_options_changed");

	whole_words = memnew(CheckBox);
	hbc_option_search->add_child(whole_words);
	whole_words->set_text(TTR("Whole Words"));
	whole_words->set_focus_mode(FOCUS_NONE);
	whole_words->connect("toggled", this, "_search_options_changed");

	// replace toolbar
	replace_text = memnew(LineEdit);
	vbc_lineedit->add_child(replace_text);
	replace_text->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	replace_text->connect("text_entered", this, "_replace_text_entered");

	replace = memnew(Button);
	hbc_button_replace->add_child(replace);
	replace->set_text(TTR("Replace"));
	replace->connect("pressed", this, "_replace_pressed");

	replace_all = memnew(Button);
	hbc_button_replace->add_child(replace_all);
	replace_all->set_text(TTR("Replace All"));
	replace_all->connect("pressed", this, "_replace_all_pressed");

	selection_only = memnew(CheckBox);
	hbc_option_replace->add_child(selection_only);
	selection_only->set_text(TTR("Selection Only"));
	selection_only->set_focus_mode(FOCUS_NONE);
	selection_only->connect("toggled", this, "_search_options_changed");

	hide_button = memnew(TextureButton);
	add_child(hide_button);
	hide_button->set_focus_mode(FOCUS_NONE);
	hide_button->connect("pressed", this, "_hide_pressed");
	hide_button->set_v_size_flags(SIZE_SHRINK_CENTER);
}

/*** CODE EDITOR ****/

// This function should be used to handle shortcuts that could otherwise
// be handled too late if they weren't handled here.
void CodeTextEditor::_input(const Ref<InputEvent> &event) {
	const Ref<InputEventKey> key_event = event;
	if (!key_event.is_valid() || !key_event->is_pressed()) {
		return;
	}

	if (ED_IS_SHORTCUT("script_text_editor/move_up", key_event)) {
		move_lines_up();
		accept_event();
		return;
	}
	if (ED_IS_SHORTCUT("script_text_editor/move_down", key_event)) {
		move_lines_down();
		accept_event();
		return;
	}
	if (ED_IS_SHORTCUT("script_text_editor/delete_line", key_event)) {
		delete_lines();
		accept_event();
		return;
	}
	if (ED_IS_SHORTCUT("script_text_editor/duplicate_selection", key_event)) {
		duplicate_selection();
		accept_event();
		return;
	}
}

void CodeTextEditor::_text_editor_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->is_pressed() && mb->get_command()) {
			if (mb->get_button_index() == BUTTON_WHEEL_UP) {
				_zoom_in();
			} else if (mb->get_button_index() == BUTTON_WHEEL_DOWN) {
				_zoom_out();
			}
		}
	}

	Ref<InputEventMagnifyGesture> magnify_gesture = p_event;
	if (magnify_gesture.is_valid()) {
		Ref<DynamicFont> font = text_editor->get_font("font");

		if (font.is_valid()) {
			if (font->get_size() != (int)font_size) {
				font_size = font->get_size();
			}

			font_size *= powf(magnify_gesture->get_factor(), 0.25);

			_add_font_size((int)font_size - font->get_size());
		}
		return;
	}

	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (k->is_pressed()) {
			if (ED_IS_SHORTCUT("script_editor/zoom_in", p_event)) {
				_zoom_in();
			}
			if (ED_IS_SHORTCUT("script_editor/zoom_out", p_event)) {
				_zoom_out();
			}
			if (ED_IS_SHORTCUT("script_editor/reset_zoom", p_event)) {
				_reset_zoom();
			}
		}
	}
}

void CodeTextEditor::_zoom_in() {
	font_resize_val += MAX(EDSCALE, 1.0f);
	_zoom_changed();
}

void CodeTextEditor::_zoom_out() {
	font_resize_val -= MAX(EDSCALE, 1.0f);
	_zoom_changed();
}

void CodeTextEditor::_zoom_changed() {
	if (font_resize_timer->get_time_left() == 0) {
		font_resize_timer->start();
	}
}

void CodeTextEditor::_reset_zoom() {
	Ref<DynamicFont> font = text_editor->get_font("font"); // Reset source font size to default.

	if (font.is_valid()) {
		EditorSettings::get_singleton()->set("interface/editor/code_font_size", 14);
		font->set_size(14);
	}
}

void CodeTextEditor::_line_col_changed() {
	String line = text_editor->get_line(text_editor->cursor_get_line());

	int positional_column = 0;
	for (int i = 0; i < text_editor->cursor_get_column(); i++) {
		if (line[i] == '\t') {
			positional_column += text_editor->get_indent_size(); //tab size
		} else {
			positional_column += 1;
		}
	}

	StringBuilder sb;
	sb.append(itos(text_editor->cursor_get_line() + 1).lpad(4));
	sb.append(" : ");
	sb.append(itos(positional_column + 1).lpad(3));

	line_and_col_txt->set_text(sb.as_string());
}

void CodeTextEditor::_text_changed() {
	if (text_editor->is_insert_text_operation()) {
		code_complete_timer->start();
	}

	idle->start();
}

void CodeTextEditor::_code_complete_timer_timeout() {
	if (!is_visible_in_tree()) {
		return;
	}
	text_editor->query_code_comple();
}

void CodeTextEditor::_complete_request() {
	List<ScriptCodeCompletionOption> entries;
	String ctext = text_editor->get_text_for_completion();
	_code_complete_script(ctext, &entries);
	bool forced = false;
	if (code_complete_func) {
		code_complete_func(code_complete_ud, ctext, &entries, forced);
	}
	if (entries.size() == 0) {
		return;
	}

	for (List<ScriptCodeCompletionOption>::Element *E = entries.front(); E; E = E->next()) {
		E->get().icon = _get_completion_icon(E->get());
	}
	text_editor->code_complete(entries, forced);
}

Ref<Texture> CodeTextEditor::_get_completion_icon(const ScriptCodeCompletionOption &p_option) {
	Ref<Texture> tex;
	switch (p_option.kind) {
		case ScriptCodeCompletionOption::KIND_CLASS: {
			if (has_icon(p_option.display, "EditorIcons")) {
				tex = get_icon(p_option.display, "EditorIcons");
			} else {
				tex = get_icon("Object", "EditorIcons");
			}
		} break;
		case ScriptCodeCompletionOption::KIND_ENUM:
			tex = get_icon("Enum", "EditorIcons");
			break;
		case ScriptCodeCompletionOption::KIND_FILE_PATH:
			tex = get_icon("File", "EditorIcons");
			break;
		case ScriptCodeCompletionOption::KIND_NODE_PATH:
			tex = get_icon("NodePath", "EditorIcons");
			break;
		case ScriptCodeCompletionOption::KIND_VARIABLE:
			tex = get_icon("Variant", "EditorIcons");
			break;
		case ScriptCodeCompletionOption::KIND_CONSTANT:
			tex = get_icon("MemberConstant", "EditorIcons");
			break;
		case ScriptCodeCompletionOption::KIND_MEMBER:
			tex = get_icon("MemberProperty", "EditorIcons");
			break;
		case ScriptCodeCompletionOption::KIND_SIGNAL:
			tex = get_icon("MemberSignal", "EditorIcons");
			break;
		case ScriptCodeCompletionOption::KIND_FUNCTION:
			tex = get_icon("MemberMethod", "EditorIcons");
			break;
		case ScriptCodeCompletionOption::KIND_PLAIN_TEXT:
			tex = get_icon("CubeMesh", "EditorIcons");
			break;
		default:
			tex = get_icon("String", "EditorIcons");
			break;
	}
	return tex;
}

void CodeTextEditor::_font_resize_timeout() {
	if (_add_font_size(font_resize_val)) {
		font_resize_val = 0;
	}
}

bool CodeTextEditor::_add_font_size(int p_delta) {
	Ref<DynamicFont> font = text_editor->get_font("font");

	if (font.is_valid()) {
		int new_size = CLAMP(font->get_size() + p_delta, 8 * EDSCALE, 96 * EDSCALE);

		if (new_size != font->get_size()) {
			EditorSettings::get_singleton()->set("interface/editor/code_font_size", new_size / EDSCALE);
			font->set_size(new_size);
		}

		return true;
	} else {
		return false;
	}
}

void CodeTextEditor::update_editor_settings() {
	text_editor->set_syntax_coloring(EditorSettings::get_singleton()->get("text_editor/highlighting/syntax_highlighting"));
	text_editor->set_highlight_all_occurrences(EditorSettings::get_singleton()->get("text_editor/highlighting/highlight_all_occurrences"));
	text_editor->set_highlight_current_line(EditorSettings::get_singleton()->get("text_editor/highlighting/highlight_current_line"));
	text_editor->set_indent_using_spaces(EditorSettings::get_singleton()->get("text_editor/indent/type"));
	text_editor->set_indent_size(EditorSettings::get_singleton()->get("text_editor/indent/size"));
	text_editor->set_auto_indent(EditorSettings::get_singleton()->get("text_editor/indent/auto_indent"));
	text_editor->set_draw_tabs(EditorSettings::get_singleton()->get("text_editor/indent/draw_tabs"));
	text_editor->set_draw_spaces(EditorSettings::get_singleton()->get("text_editor/indent/draw_spaces"));
	text_editor->set_smooth_scroll_enabled(EditorSettings::get_singleton()->get("text_editor/navigation/smooth_scrolling"));
	text_editor->set_v_scroll_speed(EditorSettings::get_singleton()->get("text_editor/navigation/v_scroll_speed"));
	text_editor->set_draw_minimap(EditorSettings::get_singleton()->get("text_editor/navigation/show_minimap"));
	text_editor->set_minimap_width((int)EditorSettings::get_singleton()->get("text_editor/navigation/minimap_width") * EDSCALE);
	text_editor->set_drag_and_drop_selection_enabled(EditorSettings::get_singleton()->get("text_editor/navigation/drag_and_drop_selection"));
	text_editor->set_show_line_numbers(EditorSettings::get_singleton()->get("text_editor/appearance/show_line_numbers"));
	text_editor->set_line_numbers_zero_padded(EditorSettings::get_singleton()->get("text_editor/appearance/line_numbers_zero_padded"));
	text_editor->set_bookmark_gutter_enabled(EditorSettings::get_singleton()->get("text_editor/appearance/show_bookmark_gutter"));
	text_editor->set_breakpoint_gutter_enabled(EditorSettings::get_singleton()->get("text_editor/appearance/show_breakpoint_gutter"));
	text_editor->set_draw_info_gutter(EditorSettings::get_singleton()->get("text_editor/appearance/show_info_gutter"));
	text_editor->set_hiding_enabled(EditorSettings::get_singleton()->get("text_editor/appearance/code_folding"));
	text_editor->set_draw_fold_gutter(EditorSettings::get_singleton()->get("text_editor/appearance/code_folding"));
	text_editor->set_wrap_enabled(EditorSettings::get_singleton()->get("text_editor/appearance/word_wrap"));
	text_editor->set_show_line_length_guidelines(EditorSettings::get_singleton()->get("text_editor/appearance/show_line_length_guidelines"));
	text_editor->set_line_length_guideline_soft_column(EditorSettings::get_singleton()->get("text_editor/appearance/line_length_guideline_soft_column"));
	text_editor->set_line_length_guideline_hard_column(EditorSettings::get_singleton()->get("text_editor/appearance/line_length_guideline_hard_column"));
	text_editor->set_scroll_pass_end_of_file(EditorSettings::get_singleton()->get("text_editor/cursor/scroll_past_end_of_file"));
	text_editor->cursor_set_block_mode(EditorSettings::get_singleton()->get("text_editor/cursor/block_caret"));
	text_editor->cursor_set_blink_enabled(EditorSettings::get_singleton()->is_caret_blink_active());
	text_editor->cursor_set_blink_speed(EditorSettings::get_singleton()->get("text_editor/cursor/caret_blink_speed"));
	text_editor->set_auto_brace_completion(EditorSettings::get_singleton()->get("text_editor/completion/auto_brace_complete"));
}

void CodeTextEditor::trim_trailing_whitespace() {
	bool trimed_whitespace = false;
	String spaces_below = "";
	for (int i = text_editor->get_line_count() - 1; i >= 0; i--) {
		String line = text_editor->get_line(i);
		bool empty_line = false;
		if (line.ends_with(" ") || line.ends_with("\t") || line == "") {
			if (!trimed_whitespace) {
				text_editor->begin_complex_operation();
				trimed_whitespace = true;
			}

			int end = 0;
			for (int j = line.length() - 1; j > -1; j--) {
				if (line[j] != ' ' && line[j] != '\t') {
					end = j + 1;
					break;
				}
			}
			// Lines without content should get same spaces as the line below.
			if (end == 0) {
				text_editor->set_line(i, spaces_below);
				empty_line = true;
			}
			else {
				text_editor->set_line(i, line.substr(0, end));
			}
		}

		// If the string is not empty, it will come in handy for restoring the spaces of the lines above.
		if (!empty_line) {
			for (int k = 0; k < line.length(); k++) {
				if (line[k] != ' ' && line[k] != '\t') {
					spaces_below = line.substr(0, k);
					break;
				}
			}
		}
	}

	if (trimed_whitespace) {
		text_editor->end_complex_operation();
		text_editor->update();
	}
}

void CodeTextEditor::insert_final_newline() {
	int final_line = text_editor->get_line_count() - 1;

	String line = text_editor->get_line(final_line);

	//length 0 means it's already an empty line,
	//no need to add a newline
	if (line.length() > 0 && !line.ends_with("\n")) {
		text_editor->begin_complex_operation();

		line += "\n";
		text_editor->set_line(final_line, line);

		text_editor->end_complex_operation();
		text_editor->update();
	}
}

void CodeTextEditor::convert_indent_to_spaces() {
	int indent_size = EditorSettings::get_singleton()->get("text_editor/indent/size");
	String indent = "";

	for (int i = 0; i < indent_size; i++) {
		indent += " ";
	}

	int cursor_line = text_editor->cursor_get_line();
	int cursor_column = text_editor->cursor_get_column();

	bool changed_indentation = false;
	for (int i = 0; i < text_editor->get_line_count(); i++) {
		String line = text_editor->get_line(i);

		if (line.length() <= 0) {
			continue;
		}

		int j = 0;
		while (j < line.length() && (line[j] == ' ' || line[j] == '\t')) {
			if (line[j] == '\t') {
				if (!changed_indentation) {
					text_editor->begin_complex_operation();
					changed_indentation = true;
				}
				if (cursor_line == i && cursor_column > j) {
					cursor_column += indent_size - 1;
				}
				line = line.left(j) + indent + line.right(j + 1);
			}
			j++;
		}
		if (changed_indentation) {
			text_editor->set_line(i, line);
		}
	}
	if (changed_indentation) {
		text_editor->cursor_set_column(cursor_column);
		text_editor->end_complex_operation();
		text_editor->update();
	}
}

void CodeTextEditor::convert_indent_to_tabs() {
	int indent_size = EditorSettings::get_singleton()->get("text_editor/indent/size");
	indent_size -= 1;

	int cursor_line = text_editor->cursor_get_line();
	int cursor_column = text_editor->cursor_get_column();

	bool changed_indentation = false;
	for (int i = 0; i < text_editor->get_line_count(); i++) {
		String line = text_editor->get_line(i);

		if (line.length() <= 0) {
			continue;
		}

		int j = 0;
		int space_count = -1;
		while (j < line.length() && (line[j] == ' ' || line[j] == '\t')) {
			if (line[j] != '\t') {
				space_count++;

				if (space_count == indent_size) {
					if (!changed_indentation) {
						text_editor->begin_complex_operation();
						changed_indentation = true;
					}
					if (cursor_line == i && cursor_column > j) {
						cursor_column -= indent_size;
					}
					line = line.left(j - indent_size) + "\t" + line.right(j + 1);
					j = 0;
					space_count = -1;
				}
			} else {
				space_count = -1;
			}
			j++;
		}
		if (changed_indentation) {
			text_editor->set_line(i, line);
		}
	}
	if (changed_indentation) {
		text_editor->cursor_set_column(cursor_column);
		text_editor->end_complex_operation();
		text_editor->update();
	}
}

void CodeTextEditor::convert_case(CaseStyle p_case) {
	if (!text_editor->is_selection_active()) {
		return;
	}

	text_editor->begin_complex_operation();

	int begin = text_editor->get_selection_from_line();
	int end = text_editor->get_selection_to_line();
	int begin_col = text_editor->get_selection_from_column();
	int end_col = text_editor->get_selection_to_column();

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
			new_line = new_line + text_editor->get_line(i).right(end_col);
		}
		text_editor->set_line(i, new_line);
	}
	text_editor->end_complex_operation();
}

void CodeTextEditor::move_lines_up() {
	text_editor->begin_complex_operation();
	if (text_editor->is_selection_active()) {
		int from_line = text_editor->get_selection_from_line();
		int from_col = text_editor->get_selection_from_column();
		int to_line = text_editor->get_selection_to_line();
		int to_column = text_editor->get_selection_to_column();
		int cursor_line = text_editor->cursor_get_line();

		for (int i = from_line; i <= to_line; i++) {
			int line_id = i;
			int next_id = i - 1;

			if (line_id == 0 || next_id < 0) {
				return;
			}

			text_editor->unfold_line(line_id);
			text_editor->unfold_line(next_id);

			text_editor->swap_lines(line_id, next_id);
			text_editor->cursor_set_line(next_id);
		}
		int from_line_up = from_line > 0 ? from_line - 1 : from_line;
		int to_line_up = to_line > 0 ? to_line - 1 : to_line;
		int cursor_line_up = cursor_line > 0 ? cursor_line - 1 : cursor_line;
		text_editor->select(from_line_up, from_col, to_line_up, to_column);
		text_editor->cursor_set_line(cursor_line_up);
	} else {
		int line_id = text_editor->cursor_get_line();
		int next_id = line_id - 1;

		if (line_id == 0 || next_id < 0) {
			return;
		}

		text_editor->unfold_line(line_id);
		text_editor->unfold_line(next_id);

		text_editor->swap_lines(line_id, next_id);
		text_editor->cursor_set_line(next_id);
	}
	text_editor->end_complex_operation();
	text_editor->update();
}

void CodeTextEditor::move_lines_down() {
	text_editor->begin_complex_operation();
	if (text_editor->is_selection_active()) {
		int from_line = text_editor->get_selection_from_line();
		int from_col = text_editor->get_selection_from_column();
		int to_line = text_editor->get_selection_to_line();
		int to_column = text_editor->get_selection_to_column();
		int cursor_line = text_editor->cursor_get_line();

		for (int i = to_line; i >= from_line; i--) {
			int line_id = i;
			int next_id = i + 1;

			if (line_id == text_editor->get_line_count() - 1 || next_id > text_editor->get_line_count()) {
				return;
			}

			text_editor->unfold_line(line_id);
			text_editor->unfold_line(next_id);

			text_editor->swap_lines(line_id, next_id);
			text_editor->cursor_set_line(next_id);
		}
		int from_line_down = from_line < text_editor->get_line_count() ? from_line + 1 : from_line;
		int to_line_down = to_line < text_editor->get_line_count() ? to_line + 1 : to_line;
		int cursor_line_down = cursor_line < text_editor->get_line_count() ? cursor_line + 1 : cursor_line;
		text_editor->select(from_line_down, from_col, to_line_down, to_column);
		text_editor->cursor_set_line(cursor_line_down);
	} else {
		int line_id = text_editor->cursor_get_line();
		int next_id = line_id + 1;

		if (line_id == text_editor->get_line_count() - 1 || next_id > text_editor->get_line_count()) {
			return;
		}

		text_editor->unfold_line(line_id);
		text_editor->unfold_line(next_id);

		text_editor->swap_lines(line_id, next_id);
		text_editor->cursor_set_line(next_id);
	}
	text_editor->end_complex_operation();
	text_editor->update();
}

void CodeTextEditor::_delete_line(int p_line) {
	// this is currently intended to be called within delete_lines()
	// so `begin_complex_operation` is omitted here
	text_editor->set_line(p_line, "");
	if (p_line == 0 && text_editor->get_line_count() > 1) {
		text_editor->cursor_set_line(1);
		text_editor->cursor_set_column(0);
	}
	text_editor->backspace_at_cursor();
	if (p_line < text_editor->get_line_count()) {
		text_editor->unfold_line(p_line);
	}
	text_editor->cursor_set_line(p_line);
}

void CodeTextEditor::delete_lines() {
	text_editor->begin_complex_operation();
	if (text_editor->is_selection_active()) {
		int to_line = text_editor->get_selection_to_line();
		int from_line = text_editor->get_selection_from_line();
		int count = Math::abs(to_line - from_line) + 1;

		text_editor->cursor_set_line(from_line, false);
		for (int i = 0; i < count; i++) {
			_delete_line(from_line);
		}
		text_editor->deselect();
	} else {
		_delete_line(text_editor->cursor_get_line());
	}
	text_editor->end_complex_operation();
}

void CodeTextEditor::duplicate_selection() {
	const int cursor_column = text_editor->cursor_get_column();
	int from_line = text_editor->cursor_get_line();
	int to_line = text_editor->cursor_get_line();
	int from_column = 0;
	int to_column = 0;
	int cursor_new_line = to_line + 1;
	int cursor_new_column = text_editor->cursor_get_column();
	String new_text = "\n" + text_editor->get_line(from_line);
	bool selection_active = false;

	text_editor->cursor_set_column(text_editor->get_line(from_line).length());
	if (text_editor->is_selection_active()) {
		from_column = text_editor->get_selection_from_column();
		to_column = text_editor->get_selection_to_column();

		from_line = text_editor->get_selection_from_line();
		to_line = text_editor->get_selection_to_line();
		cursor_new_line = to_line + text_editor->cursor_get_line() - from_line;
		cursor_new_column = to_column == cursor_column ? 2 * to_column - from_column : to_column;
		new_text = text_editor->get_selection_text();
		selection_active = true;

		text_editor->cursor_set_line(to_line);
		text_editor->cursor_set_column(to_column);
	}

	text_editor->begin_complex_operation();

	for (int i = from_line; i <= to_line; i++) {
		text_editor->unfold_line(i);
	}
	text_editor->deselect();
	text_editor->insert_text_at_cursor(new_text);
	text_editor->cursor_set_line(cursor_new_line);
	text_editor->cursor_set_column(cursor_new_column);
	if (selection_active) {
		text_editor->select(to_line, to_column, 2 * to_line - from_line, to_line == from_line ? 2 * to_column - from_column : to_column);
	}

	text_editor->end_complex_operation();
	text_editor->update();
}

void CodeTextEditor::toggle_inline_comment(const String &delimiter) {
	text_editor->begin_complex_operation();
	if (text_editor->is_selection_active()) {
		int begin = text_editor->get_selection_from_line();
		int end = text_editor->get_selection_to_line();

		// End of selection ends on the first column of the last line, ignore it.
		if (text_editor->get_selection_to_column() == 0) {
			end -= 1;
		}

		int col_to = text_editor->get_selection_to_column();
		int cursor_pos = text_editor->cursor_get_column();

		// Check if all lines in the selected block are commented.
		bool is_commented = true;
		for (int i = begin; i <= end; i++) {
			if (!text_editor->get_line(i).begins_with(delimiter)) {
				is_commented = false;
				break;
			}
		}
		for (int i = begin; i <= end; i++) {
			String line_text = text_editor->get_line(i);

			if (line_text.strip_edges().empty()) {
				line_text = delimiter;
			} else {
				if (is_commented) {
					line_text = line_text.substr(delimiter.length(), line_text.length());
				} else {
					line_text = delimiter + line_text;
				}
			}
			text_editor->set_line(i, line_text);
		}

		// Adjust selection & cursor position.
		int offset = (is_commented ? -1 : 1) * delimiter.length();
		int col_from = text_editor->get_selection_from_column() > 0 ? text_editor->get_selection_from_column() + offset : 0;

		if (is_commented && text_editor->cursor_get_column() == text_editor->get_line(text_editor->cursor_get_line()).length() + 1) {
			cursor_pos += 1;
		}

		if (text_editor->get_selection_to_column() != 0 && col_to != text_editor->get_line(text_editor->get_selection_to_line()).length() + 1) {
			col_to += offset;
		}

		if (text_editor->cursor_get_column() != 0) {
			cursor_pos += offset;
		}

		text_editor->select(begin, col_from, text_editor->get_selection_to_line(), col_to);
		text_editor->cursor_set_column(cursor_pos);

	} else {
		int begin = text_editor->cursor_get_line();
		String line_text = text_editor->get_line(begin);
		int delimiter_length = delimiter.length();

		int col = text_editor->cursor_get_column();
		if (line_text.begins_with(delimiter)) {
			line_text = line_text.substr(delimiter_length, line_text.length());
			col -= delimiter_length;
		} else {
			line_text = delimiter + line_text;
			col += delimiter_length;
		}

		text_editor->set_line(begin, line_text);
		text_editor->cursor_set_column(col);
	}
	text_editor->end_complex_operation();
	text_editor->update();
}

void CodeTextEditor::goto_line(int p_line) {
	text_editor->deselect();
	text_editor->unfold_line(p_line);
	text_editor->call_deferred("cursor_set_line", p_line);
}

void CodeTextEditor::goto_line_selection(int p_line, int p_begin, int p_end) {
	text_editor->unfold_line(p_line);
	text_editor->call_deferred("cursor_set_line", p_line);
	text_editor->call_deferred("cursor_set_column", p_begin);
	text_editor->select(p_line, p_begin, p_line, p_end);
}

void CodeTextEditor::goto_line_centered(int p_line) {
	goto_line(p_line);
	text_editor->call_deferred("center_viewport_to_cursor");
}

void CodeTextEditor::set_executing_line(int p_line) {
	text_editor->set_executing_line(p_line);
}

void CodeTextEditor::clear_executing_line() {
	text_editor->clear_executing_line();
}

Variant CodeTextEditor::get_edit_state() {
	Dictionary state;

	state["scroll_position"] = text_editor->get_v_scroll();
	state["h_scroll_position"] = text_editor->get_h_scroll();
	state["column"] = text_editor->cursor_get_column();
	state["row"] = text_editor->cursor_get_line();

	state["selection"] = get_text_edit()->is_selection_active();
	if (get_text_edit()->is_selection_active()) {
		state["selection_from_line"] = text_editor->get_selection_from_line();
		state["selection_from_column"] = text_editor->get_selection_from_column();
		state["selection_to_line"] = text_editor->get_selection_to_line();
		state["selection_to_column"] = text_editor->get_selection_to_column();
	}

	state["folded_lines"] = text_editor->get_folded_lines();
	state["breakpoints"] = text_editor->get_breakpoints_array();
	state["bookmarks"] = text_editor->get_bookmarks_array();

	state["syntax_highlighter"] = TTR("Standard");
	SyntaxHighlighter *syntax_highlighter = text_editor->_get_syntax_highlighting();
	if (syntax_highlighter) {
		state["syntax_highlighter"] = syntax_highlighter->get_name();
	}

	return state;
}

void CodeTextEditor::set_edit_state(const Variant &p_state) {
	Dictionary state = p_state;

	/* update the row first as it sets the column to 0 */
	text_editor->cursor_set_line(state["row"]);
	text_editor->cursor_set_column(state["column"]);
	text_editor->set_v_scroll(state["scroll_position"]);
	text_editor->set_h_scroll(state["h_scroll_position"]);

	if (state.has("selection")) {
		text_editor->select(state["selection_from_line"], state["selection_from_column"], state["selection_to_line"], state["selection_to_column"]);
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
			text_editor->set_line_as_bookmark(bookmarks[i], true);
		}
	}
}

void CodeTextEditor::set_error(const String &p_error) {
	error->set_text(p_error);
	if (p_error != "") {
		error->set_default_cursor_shape(CURSOR_POINTING_HAND);
	} else {
		error->set_default_cursor_shape(CURSOR_ARROW);
	}
}

void CodeTextEditor::set_error_pos(int p_line, int p_column) {
	error_line = p_line;
	error_column = p_column;
}

void CodeTextEditor::goto_error() {
	if (error->get_text() != "") {
		text_editor->cursor_set_line(error_line);
		text_editor->cursor_set_column(error_column);
		text_editor->center_viewport_to_cursor();
	}
}

void CodeTextEditor::_update_font() {
	text_editor->add_font_override("font", get_font("source", "EditorFonts"));

	error->add_font_override("font", get_font("status_source", "EditorFonts"));
	error->add_color_override("font_color", get_color("error_color", "Editor"));

	Ref<Font> status_bar_font = get_font("status_source", "EditorFonts");
	error->add_font_override("font", status_bar_font);
	int count = status_bar->get_child_count();
	for (int i = 0; i < count; i++) {
		Control *n = Object::cast_to<Control>(status_bar->get_child(i));
		if (n) {
			n->add_font_override("font", status_bar_font);
		}
	}
}

void CodeTextEditor::_on_settings_change() {
	_update_font();

	font_size = EditorSettings::get_singleton()->get("interface/editor/code_font_size");

	// Auto brace completion.
	text_editor->set_auto_brace_completion(
			EDITOR_GET("text_editor/completion/auto_brace_complete"));

	code_complete_timer->set_wait_time(
			EDITOR_GET("text_editor/completion/code_complete_delay"));

	// Call hint settings.
	text_editor->set_callhint_settings(
			EDITOR_GET("text_editor/completion/put_callhint_tooltip_below_current_line"),
			EDITOR_GET("text_editor/completion/callhint_tooltip_offset"));

	idle->set_wait_time(EDITOR_GET("text_editor/completion/idle_parse_delay"));
}

void CodeTextEditor::_text_changed_idle_timeout() {
	_validate_script();
	emit_signal("validate_script");
}

void CodeTextEditor::validate_script() {
	idle->start();
}

void CodeTextEditor::_warning_label_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
		_warning_button_pressed();
	}
}

void CodeTextEditor::_warning_button_pressed() {
	_set_show_warnings_panel(!is_warnings_panel_opened);
}

void CodeTextEditor::_set_show_warnings_panel(bool p_show) {
	is_warnings_panel_opened = p_show;
	emit_signal("show_warnings_panel", p_show);
}

void CodeTextEditor::_toggle_scripts_pressed() {
	toggle_scripts_button->set_icon(ScriptEditor::get_singleton()->toggle_scripts_panel() ? get_icon("Back", "EditorIcons") : get_icon("Forward", "EditorIcons"));
}

void CodeTextEditor::_error_pressed(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
		goto_error();
	}
}

void CodeTextEditor::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			_load_theme_settings();
			emit_signal("load_theme_settings");
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			if (toggle_scripts_button->is_visible()) {
				update_toggle_scripts_button();
			}
			_update_font();
		} break;
		case NOTIFICATION_ENTER_TREE: {
			warning_button->set_icon(get_icon("NodeWarning", "EditorIcons"));
			add_constant_override("separation", 4 * EDSCALE);
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (toggle_scripts_button->is_visible()) {
				update_toggle_scripts_button();
			}
			set_process_input(is_visible_in_tree());
		} break;
		default:
			break;
	}
}

void CodeTextEditor::set_warning_nb(int p_warning_nb) {
	warning_count_label->set_text(itos(p_warning_nb));
	warning_count_label->set_visible(p_warning_nb > 0);
	warning_button->set_visible(p_warning_nb > 0);
	if (!p_warning_nb) {
		_set_show_warnings_panel(false);
	}
}

void CodeTextEditor::toggle_bookmark() {
	int line = text_editor->cursor_get_line();
	text_editor->set_line_as_bookmark(line, !text_editor->is_line_set_as_bookmark(line));
}

void CodeTextEditor::goto_next_bookmark() {
	List<int> bmarks;
	text_editor->get_bookmarks(&bmarks);
	if (bmarks.size() <= 0) {
		return;
	}

	int line = text_editor->cursor_get_line();
	if (line >= bmarks[bmarks.size() - 1]) {
		text_editor->unfold_line(bmarks[0]);
		text_editor->cursor_set_line(bmarks[0]);
		text_editor->center_viewport_to_cursor();
	} else {
		for (List<int>::Element *E = bmarks.front(); E; E = E->next()) {
			int bline = E->get();
			if (bline > line) {
				text_editor->unfold_line(bline);
				text_editor->cursor_set_line(bline);
				text_editor->center_viewport_to_cursor();
				return;
			}
		}
	}
}

void CodeTextEditor::goto_prev_bookmark() {
	List<int> bmarks;
	text_editor->get_bookmarks(&bmarks);
	if (bmarks.size() <= 0) {
		return;
	}

	int line = text_editor->cursor_get_line();
	if (line <= bmarks[0]) {
		text_editor->unfold_line(bmarks[bmarks.size() - 1]);
		text_editor->cursor_set_line(bmarks[bmarks.size() - 1]);
		text_editor->center_viewport_to_cursor();
	} else {
		for (List<int>::Element *E = bmarks.back(); E; E = E->prev()) {
			int bline = E->get();
			if (bline < line) {
				text_editor->unfold_line(bline);
				text_editor->cursor_set_line(bline);
				text_editor->center_viewport_to_cursor();
				return;
			}
		}
	}
}

void CodeTextEditor::remove_all_bookmarks() {
	List<int> bmarks;
	text_editor->get_bookmarks(&bmarks);

	for (List<int>::Element *E = bmarks.front(); E; E = E->next()) {
		text_editor->set_line_as_bookmark(E->get(), false);
	}
}

void CodeTextEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_input"), &CodeTextEditor::_input);
	ClassDB::bind_method("_text_editor_gui_input", &CodeTextEditor::_text_editor_gui_input);
	ClassDB::bind_method("_line_col_changed", &CodeTextEditor::_line_col_changed);
	ClassDB::bind_method("_text_changed", &CodeTextEditor::_text_changed);
	ClassDB::bind_method("_on_settings_change", &CodeTextEditor::_on_settings_change);
	ClassDB::bind_method("_text_changed_idle_timeout", &CodeTextEditor::_text_changed_idle_timeout);
	ClassDB::bind_method("_code_complete_timer_timeout", &CodeTextEditor::_code_complete_timer_timeout);
	ClassDB::bind_method("_complete_request", &CodeTextEditor::_complete_request);
	ClassDB::bind_method("_font_resize_timeout", &CodeTextEditor::_font_resize_timeout);
	ClassDB::bind_method("_error_pressed", &CodeTextEditor::_error_pressed);
	ClassDB::bind_method("_toggle_scripts_pressed", &CodeTextEditor::_toggle_scripts_pressed);
	ClassDB::bind_method("_warning_button_pressed", &CodeTextEditor::_warning_button_pressed);
	ClassDB::bind_method("_warning_label_gui_input", &CodeTextEditor::_warning_label_gui_input);

	ADD_SIGNAL(MethodInfo("validate_script"));
	ADD_SIGNAL(MethodInfo("load_theme_settings"));
	ADD_SIGNAL(MethodInfo("show_warnings_panel"));
}

void CodeTextEditor::set_code_complete_func(CodeTextEditorCodeCompleteFunc p_code_complete_func, void *p_ud) {
	code_complete_func = p_code_complete_func;
	code_complete_ud = p_ud;
}

void CodeTextEditor::show_toggle_scripts_button() {
	toggle_scripts_button->show();
}

void CodeTextEditor::update_toggle_scripts_button() {
	toggle_scripts_button->set_icon(ScriptEditor::get_singleton()->is_scripts_panel_toggled() ? get_icon("Back", "EditorIcons") : get_icon("Forward", "EditorIcons"));
	toggle_scripts_button->set_tooltip(TTR("Toggle Scripts Panel") + " (" + ED_GET_SHORTCUT("script_editor/toggle_scripts_panel")->get_as_text() + ")");
}

CodeTextEditor::CodeTextEditor() {
	code_complete_func = nullptr;
	ED_SHORTCUT("script_editor/zoom_in", TTR("Zoom In"), KEY_MASK_CMD | KEY_EQUAL);
	ED_SHORTCUT("script_editor/zoom_out", TTR("Zoom Out"), KEY_MASK_CMD | KEY_MINUS);
	ED_SHORTCUT("script_editor/reset_zoom", TTR("Reset Zoom"), KEY_MASK_CMD | KEY_0);

	text_editor = memnew(TextEdit);
	add_child(text_editor);
	text_editor->set_v_size_flags(SIZE_EXPAND_FILL);

	// Added second so it opens at the bottom, so it won't shift the entire text editor when opening.
	find_replace_bar = memnew(FindReplaceBar);
	add_child(find_replace_bar);
	find_replace_bar->set_h_size_flags(SIZE_EXPAND_FILL);
	find_replace_bar->hide();

	find_replace_bar->set_text_edit(text_editor);

	text_editor->set_show_line_numbers(true);
	text_editor->set_brace_matching(true);
	text_editor->set_auto_indent(true);
	text_editor->set_deselect_on_focus_loss_enabled(false);

	status_bar = memnew(HBoxContainer);
	add_child(status_bar);
	status_bar->set_h_size_flags(SIZE_EXPAND_FILL);
	status_bar->set_custom_minimum_size(Size2(0, 24 * EDSCALE)); // Adjust for the height of the warning icon.

	idle = memnew(Timer);
	add_child(idle);
	idle->set_one_shot(true);
	idle->set_wait_time(EDITOR_GET("text_editor/completion/idle_parse_delay"));

	code_complete_timer = memnew(Timer);
	add_child(code_complete_timer);
	code_complete_timer->set_one_shot(true);
	code_complete_timer->set_wait_time(EDITOR_GET("text_editor/completion/code_complete_delay"));

	error_line = 0;
	error_column = 0;

	toggle_scripts_button = memnew(ToolButton);
	toggle_scripts_button->connect("pressed", this, "_toggle_scripts_pressed");
	status_bar->add_child(toggle_scripts_button);
	toggle_scripts_button->hide();

	// Error
	ScrollContainer *scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	scroll->set_enable_v_scroll(false);
	status_bar->add_child(scroll);

	error = memnew(Label);
	scroll->add_child(error);
	error->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	error->set_mouse_filter(MOUSE_FILTER_STOP);
	error->connect("gui_input", this, "_error_pressed");
	find_replace_bar->connect("error", error, "set_text");

	// Warnings
	warning_button = memnew(ToolButton);
	status_bar->add_child(warning_button);
	warning_button->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	warning_button->set_default_cursor_shape(CURSOR_POINTING_HAND);
	warning_button->connect("pressed", this, "_warning_button_pressed");
	warning_button->set_tooltip(TTR("Warnings"));

	warning_count_label = memnew(Label);
	status_bar->add_child(warning_count_label);
	warning_count_label->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	warning_count_label->set_align(Label::ALIGN_RIGHT);
	warning_count_label->set_default_cursor_shape(CURSOR_POINTING_HAND);
	warning_count_label->set_mouse_filter(MOUSE_FILTER_STOP);
	warning_count_label->set_tooltip(TTR("Warnings"));
	warning_count_label->add_color_override("font_color", EditorNode::get_singleton()->get_gui_base()->get_color("warning_color", "Editor"));
	warning_count_label->add_font_override("font", EditorNode::get_singleton()->get_gui_base()->get_font("status_source", "EditorFonts"));
	warning_count_label->connect("gui_input", this, "_warning_label_gui_input");

	is_warnings_panel_opened = false;
	set_warning_nb(0);

	// Line and column
	line_and_col_txt = memnew(Label);
	status_bar->add_child(line_and_col_txt);
	line_and_col_txt->set_v_size_flags(SIZE_EXPAND | SIZE_SHRINK_CENTER);
	line_and_col_txt->add_font_override("font", EditorNode::get_singleton()->get_gui_base()->get_font("status_source", "EditorFonts"));
	line_and_col_txt->set_tooltip(TTR("Line and column numbers."));
	line_and_col_txt->set_mouse_filter(MOUSE_FILTER_STOP);

	text_editor->connect("gui_input", this, "_text_editor_gui_input");
	text_editor->connect("cursor_changed", this, "_line_col_changed");
	text_editor->connect("text_changed", this, "_text_changed");
	text_editor->connect("request_completion", this, "_complete_request");
	Vector<String> cs;
	cs.push_back(".");
	cs.push_back(",");
	cs.push_back("(");
	cs.push_back("=");
	cs.push_back("$");
	text_editor->set_completion(true, cs);
	idle->connect("timeout", this, "_text_changed_idle_timeout");

	code_complete_timer->connect("timeout", this, "_code_complete_timer_timeout");

	font_resize_val = 0;
	font_size = EditorSettings::get_singleton()->get("interface/editor/code_font_size");
	font_resize_timer = memnew(Timer);
	add_child(font_resize_timer);
	font_resize_timer->set_one_shot(true);
	font_resize_timer->set_wait_time(0.07);
	font_resize_timer->connect("timeout", this, "_font_resize_timeout");

	EditorSettings::get_singleton()->connect("settings_changed", this, "_on_settings_change");
}
