/**************************************************************************/
/*  text_editor.cpp                                                       */
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

#include "text_editor.h"

#include "core/io/json.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/split_container.h"

void TextEditor::add_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) {
	ERR_FAIL_COND(p_highlighter.is_null());

	highlighters[p_highlighter->_get_name()] = p_highlighter;
	highlighter_menu->add_radio_check_item(p_highlighter->_get_name());
}

void TextEditor::set_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) {
	ERR_FAIL_COND(p_highlighter.is_null());

	HashMap<String, Ref<EditorSyntaxHighlighter>>::Iterator el = highlighters.begin();
	while (el) {
		int highlighter_index = highlighter_menu->get_item_idx_from_text(el->key);
		highlighter_menu->set_item_checked(highlighter_index, el->value == p_highlighter);
		++el;
	}

	CodeEdit *te = code_editor->get_text_editor();
	te->set_syntax_highlighter(p_highlighter);
}

void TextEditor::_change_syntax_highlighter(int p_idx) {
	set_syntax_highlighter(highlighters[highlighter_menu->get_item_text(p_idx)]);
}

void TextEditor::_load_theme_settings() {
	code_editor->get_text_editor()->get_syntax_highlighter()->update_cache();
}

String TextEditor::get_name() {
	String name;

	name = edited_res->get_path().get_file();
	if (name.is_empty()) {
		// This appears for newly created built-in text_files before saving the scene.
		name = TTR("[unsaved]");
	} else if (edited_res->is_built_in()) {
		const String &text_file_name = edited_res->get_name();
		if (!text_file_name.is_empty()) {
			// If the built-in text_file has a custom resource name defined,
			// display the built-in text_file name as follows: `ResourceName (scene_file.tscn)`
			name = vformat("%s (%s)", text_file_name, name.get_slice("::", 0));
		}
	}

	if (is_unsaved()) {
		name += "(*)";
	}

	return name;
}

Ref<Texture2D> TextEditor::get_theme_icon() {
	return EditorNode::get_singleton()->get_object_icon(edited_res.ptr(), "TextFile");
}

Ref<Resource> TextEditor::get_edited_resource() const {
	return edited_res;
}

void TextEditor::set_edited_resource(const Ref<Resource> &p_res) {
	ERR_FAIL_COND(edited_res.is_valid());
	ERR_FAIL_COND(p_res.is_null());

	edited_res = p_res;

	Ref<TextFile> text_file = edited_res;
	if (text_file.is_valid()) {
		code_editor->get_text_editor()->set_text(text_file->get_text());
	}

	Ref<JSON> json_file = edited_res;
	if (json_file.is_valid()) {
		code_editor->get_text_editor()->set_text(json_file->get_parsed_text());
	}

	code_editor->get_text_editor()->clear_undo_history();
	code_editor->get_text_editor()->tag_saved_version();

	emit_signal(SNAME("name_changed"));
	code_editor->update_line_and_column();
}

void TextEditor::enable_editor(Control *p_shortcut_context) {
	if (editor_enabled) {
		return;
	}

	editor_enabled = true;

	_load_theme_settings();

	_validate_script();

	if (p_shortcut_context) {
		for (int i = 0; i < edit_hb->get_child_count(); ++i) {
			Control *c = cast_to<Control>(edit_hb->get_child(i));
			if (c) {
				c->set_shortcut_context(p_shortcut_context);
			}
		}
	}
}

void TextEditor::add_callback(const String &p_function, const PackedStringArray &p_args) {
}

void TextEditor::set_debugger_active(bool p_active) {
}

Control *TextEditor::get_base_editor() const {
	return code_editor->get_text_editor();
}

CodeTextEditor *TextEditor::get_code_editor() const {
	return code_editor;
}

PackedInt32Array TextEditor::get_breakpoints() {
	return PackedInt32Array();
}

void TextEditor::reload_text() {
	ERR_FAIL_COND(edited_res.is_null());

	CodeEdit *te = code_editor->get_text_editor();
	int column = te->get_caret_column();
	int row = te->get_caret_line();
	int h = te->get_h_scroll();
	int v = te->get_v_scroll();

	Ref<TextFile> text_file = edited_res;
	if (text_file.is_valid()) {
		te->set_text(text_file->get_text());
	}

	Ref<JSON> json_file = edited_res;
	if (json_file.is_valid()) {
		te->set_text(json_file->get_parsed_text());
	}

	te->set_caret_line(row);
	te->set_caret_column(column);
	te->set_h_scroll(h);
	te->set_v_scroll(v);

	te->tag_saved_version();

	code_editor->update_line_and_column();
	_validate_script();
}

void TextEditor::_validate_script() {
	emit_signal(SNAME("name_changed"));
	emit_signal(SNAME("edited_script_changed"));

	Ref<JSON> json_file = edited_res;
	if (json_file.is_valid()) {
		CodeEdit *te = code_editor->get_text_editor();

		te->set_line_background_color(code_editor->get_error_pos().x, Color(0, 0, 0, 0));
		code_editor->set_error("");

		if (json_file->parse(te->get_text(), true) != OK) {
			code_editor->set_error(json_file->get_error_message());
			code_editor->set_error_pos(json_file->get_error_line(), 0);
			te->set_line_background_color(code_editor->get_error_pos().x, EDITOR_GET("text_editor/theme/highlighting/mark_color"));
		}
	}
}

void TextEditor::_update_bookmark_list() {
	bookmarks_menu->clear();

	bookmarks_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_bookmark"), BOOKMARK_TOGGLE);
	bookmarks_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/remove_all_bookmarks"), BOOKMARK_REMOVE_ALL);
	bookmarks_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_next_bookmark"), BOOKMARK_GOTO_NEXT);
	bookmarks_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_previous_bookmark"), BOOKMARK_GOTO_PREV);

	PackedInt32Array bookmark_list = code_editor->get_text_editor()->get_bookmarked_lines();
	if (bookmark_list.size() == 0) {
		return;
	}

	bookmarks_menu->add_separator();

	for (int i = 0; i < bookmark_list.size(); i++) {
		String line = code_editor->get_text_editor()->get_line(bookmark_list[i]).strip_edges();
		// Limit the size of the line if too big.
		if (line.length() > 50) {
			line = line.substr(0, 50);
		}

		bookmarks_menu->add_item(String::num((int)bookmark_list[i] + 1) + " - \"" + line + "\"");
		bookmarks_menu->set_item_metadata(-1, bookmark_list[i]);
	}
}

void TextEditor::_bookmark_item_pressed(int p_idx) {
	if (p_idx < 4) { // Any item before the separator.
		_edit_option(bookmarks_menu->get_item_id(p_idx));
	} else {
		code_editor->goto_line(bookmarks_menu->get_item_metadata(p_idx));
	}
}

void TextEditor::apply_code() {
	Ref<TextFile> text_file = edited_res;
	if (text_file.is_valid()) {
		text_file->set_text(code_editor->get_text_editor()->get_text());
	}

	Ref<JSON> json_file = edited_res;
	if (json_file.is_valid()) {
		json_file->parse(code_editor->get_text_editor()->get_text(), true);
	}
	code_editor->get_text_editor()->get_syntax_highlighter()->update_cache();
}

bool TextEditor::is_unsaved() {
	const bool unsaved =
			code_editor->get_text_editor()->get_version() != code_editor->get_text_editor()->get_saved_version() ||
			edited_res->get_path().is_empty(); // In memory.
	return unsaved;
}

Variant TextEditor::get_edit_state() {
	return code_editor->get_edit_state();
}

void TextEditor::set_edit_state(const Variant &p_state) {
	code_editor->set_edit_state(p_state);

	Dictionary state = p_state;
	if (state.has("syntax_highlighter")) {
		int idx = highlighter_menu->get_item_idx_from_text(state["syntax_highlighter"]);
		if (idx >= 0) {
			_change_syntax_highlighter(idx);
		}
	}

	ensure_focus();
}

Variant TextEditor::get_navigation_state() {
	return code_editor->get_navigation_state();
}

void TextEditor::trim_trailing_whitespace() {
	code_editor->trim_trailing_whitespace();
}

void TextEditor::trim_final_newlines() {
	code_editor->trim_final_newlines();
}

void TextEditor::insert_final_newline() {
	code_editor->insert_final_newline();
}

void TextEditor::convert_indent() {
	code_editor->get_text_editor()->convert_indent();
}

void TextEditor::tag_saved_version() {
	code_editor->get_text_editor()->tag_saved_version();
}

void TextEditor::goto_line(int p_line, int p_column) {
	code_editor->goto_line(p_line, p_column);
}

void TextEditor::goto_line_selection(int p_line, int p_begin, int p_end) {
	code_editor->goto_line_selection(p_line, p_begin, p_end);
}

void TextEditor::set_executing_line(int p_line) {
	code_editor->set_executing_line(p_line);
}

void TextEditor::clear_executing_line() {
	code_editor->clear_executing_line();
}

void TextEditor::ensure_focus() {
	code_editor->get_text_editor()->grab_focus();
}

Vector<String> TextEditor::get_functions() {
	return Vector<String>();
}

bool TextEditor::show_members_overview() {
	return true;
}

void TextEditor::update_settings() {
	code_editor->update_editor_settings();
}

void TextEditor::set_tooltip_request_func(const Callable &p_toolip_callback) {
	Variant args[1] = { this };
	const Variant *argp[] = { &args[0] };
	code_editor->get_text_editor()->set_tooltip_request_func(p_toolip_callback.bindp(argp, 1));
}

Control *TextEditor::get_edit_menu() {
	return edit_hb;
}

void TextEditor::clear_edit_menu() {
	memdelete(edit_hb);
}

void TextEditor::set_find_replace_bar(FindReplaceBar *p_bar) {
	code_editor->set_find_replace_bar(p_bar);
}

void TextEditor::_edit_option(int p_op) {
	CodeEdit *tx = code_editor->get_text_editor();
	tx->apply_ime();

	switch (p_op) {
		case EDIT_UNDO: {
			tx->undo();
			callable_mp((Control *)tx, &Control::grab_focus).call_deferred();
		} break;
		case EDIT_REDO: {
			tx->redo();
			callable_mp((Control *)tx, &Control::grab_focus).call_deferred();
		} break;
		case EDIT_CUT: {
			tx->cut();
			callable_mp((Control *)tx, &Control::grab_focus).call_deferred();
		} break;
		case EDIT_COPY: {
			tx->copy();
			callable_mp((Control *)tx, &Control::grab_focus).call_deferred();
		} break;
		case EDIT_PASTE: {
			tx->paste();
			callable_mp((Control *)tx, &Control::grab_focus).call_deferred();
		} break;
		case EDIT_SELECT_ALL: {
			tx->select_all();
			callable_mp((Control *)tx, &Control::grab_focus).call_deferred();
		} break;
		case EDIT_MOVE_LINE_UP: {
			code_editor->get_text_editor()->move_lines_up();
		} break;
		case EDIT_MOVE_LINE_DOWN: {
			code_editor->get_text_editor()->move_lines_down();
		} break;
		case EDIT_INDENT: {
			tx->indent_lines();
		} break;
		case EDIT_UNINDENT: {
			tx->unindent_lines();
		} break;
		case EDIT_DELETE_LINE: {
			code_editor->get_text_editor()->delete_lines();
		} break;
		case EDIT_DUPLICATE_SELECTION: {
			code_editor->get_text_editor()->duplicate_selection();
		} break;
		case EDIT_DUPLICATE_LINES: {
			code_editor->get_text_editor()->duplicate_lines();
		} break;
		case EDIT_TOGGLE_FOLD_LINE: {
			tx->toggle_foldable_lines_at_carets();
		} break;
		case EDIT_FOLD_ALL_LINES: {
			tx->fold_all_lines();
			tx->queue_redraw();
		} break;
		case EDIT_UNFOLD_ALL_LINES: {
			tx->unfold_all_lines();
			tx->queue_redraw();
		} break;
		case EDIT_TRIM_TRAILING_WHITESAPCE: {
			trim_trailing_whitespace();
		} break;
		case EDIT_TRIM_FINAL_NEWLINES: {
			trim_final_newlines();
		} break;
		case EDIT_CONVERT_INDENT_TO_SPACES: {
			code_editor->set_indent_using_spaces(true);
			convert_indent();
		} break;
		case EDIT_CONVERT_INDENT_TO_TABS: {
			code_editor->set_indent_using_spaces(false);
			convert_indent();
		} break;
		case EDIT_TO_UPPERCASE: {
			_convert_case(CodeTextEditor::UPPER);
		} break;
		case EDIT_TO_LOWERCASE: {
			_convert_case(CodeTextEditor::LOWER);
		} break;
		case EDIT_CAPITALIZE: {
			_convert_case(CodeTextEditor::CAPITALIZE);
		} break;
		case EDIT_TOGGLE_WORD_WRAP: {
			TextEdit::LineWrappingMode wrap = code_editor->get_text_editor()->get_line_wrapping_mode();
			code_editor->get_text_editor()->set_line_wrapping_mode(wrap == TextEdit::LINE_WRAPPING_BOUNDARY ? TextEdit::LINE_WRAPPING_NONE : TextEdit::LINE_WRAPPING_BOUNDARY);
		} break;
		case SEARCH_FIND: {
			code_editor->get_find_replace_bar()->popup_search();
		} break;
		case SEARCH_FIND_NEXT: {
			code_editor->get_find_replace_bar()->search_next();
		} break;
		case SEARCH_FIND_PREV: {
			code_editor->get_find_replace_bar()->search_prev();
		} break;
		case SEARCH_REPLACE: {
			code_editor->get_find_replace_bar()->popup_replace();
		} break;
		case SEARCH_IN_FILES: {
			String selected_text = code_editor->get_text_editor()->get_selected_text();

			// Yep, because it doesn't make sense to instance this dialog for every single script open...
			// So this will be delegated to the ScriptEditor.
			emit_signal(SNAME("search_in_files_requested"), selected_text);
		} break;
		case REPLACE_IN_FILES: {
			String selected_text = code_editor->get_text_editor()->get_selected_text();

			emit_signal(SNAME("replace_in_files_requested"), selected_text);
		} break;
		case SEARCH_GOTO_LINE: {
			goto_line_popup->popup_find_line(code_editor);
		} break;
		case BOOKMARK_TOGGLE: {
			code_editor->toggle_bookmark();
		} break;
		case BOOKMARK_GOTO_NEXT: {
			code_editor->goto_next_bookmark();
		} break;
		case BOOKMARK_GOTO_PREV: {
			code_editor->goto_prev_bookmark();
		} break;
		case BOOKMARK_REMOVE_ALL: {
			code_editor->remove_all_bookmarks();
		} break;
		case EDIT_EMOJI_AND_SYMBOL: {
			code_editor->get_text_editor()->show_emoji_and_symbol_picker();
		} break;
	}
}

void TextEditor::_convert_case(CodeTextEditor::CaseStyle p_case) {
	code_editor->convert_case(p_case);
}

ScriptEditorBase *TextEditor::create_editor(const Ref<Resource> &p_resource) {
	if (Object::cast_to<TextFile>(*p_resource) || Object::cast_to<JSON>(*p_resource)) {
		return memnew(TextEditor);
	}
	return nullptr;
}

void TextEditor::register_editor() {
	ScriptEditor::register_create_script_editor_function(create_editor);
}

void TextEditor::_text_edit_gui_input(const Ref<InputEvent> &ev) {
	Ref<InputEventMouseButton> mb = ev;

	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::RIGHT) {
			CodeEdit *tx = code_editor->get_text_editor();

			tx->apply_ime();

			Point2i pos = tx->get_line_column_at_pos(mb->get_global_position() - tx->get_global_position());
			int row = pos.y;
			int col = pos.x;

			tx->set_move_caret_on_right_click_enabled(EDITOR_GET("text_editor/behavior/navigation/move_caret_on_right_click"));
			bool can_fold = tx->can_fold_line(row);
			bool is_folded = tx->is_line_folded(row);

			if (tx->is_move_caret_on_right_click_enabled()) {
				tx->remove_secondary_carets();
				if (tx->has_selection()) {
					int from_line = tx->get_selection_from_line();
					int to_line = tx->get_selection_to_line();
					int from_column = tx->get_selection_from_column();
					int to_column = tx->get_selection_to_column();

					if (row < from_line || row > to_line || (row == from_line && col < from_column) || (row == to_line && col > to_column)) {
						// Right click is outside the selected text.
						tx->deselect();
					}
				}
				if (!tx->has_selection()) {
					tx->set_caret_line(row, true, false, -1);
					tx->set_caret_column(col);
				}
			}

			if (!mb->is_pressed()) {
				_make_context_menu(tx->has_selection(), can_fold, is_folded, get_local_mouse_position());
			}
		}
	}

	Ref<InputEventKey> k = ev;
	if (k.is_valid() && k->is_pressed() && k->is_action("ui_menu", true)) {
		CodeEdit *tx = code_editor->get_text_editor();
		int line = tx->get_caret_line(0);
		tx->adjust_viewport_to_caret(0);
		_make_context_menu(tx->has_selection(0), tx->can_fold_line(line), tx->is_line_folded(line), (get_global_transform().inverse() * tx->get_global_transform()).xform(tx->get_caret_draw_pos(0)));
		context_menu->grab_focus();
	}
}

void TextEditor::_prepare_edit_menu() {
	const CodeEdit *tx = code_editor->get_text_editor();
	PopupMenu *popup = edit_menu->get_popup();
	popup->set_item_disabled(popup->get_item_index(EDIT_UNDO), !tx->has_undo());
	popup->set_item_disabled(popup->get_item_index(EDIT_REDO), !tx->has_redo());
}

void TextEditor::_make_context_menu(bool p_selection, bool p_can_fold, bool p_is_folded, Vector2 p_position) {
	context_menu->clear();
	if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_EMOJI_AND_SYMBOL_PICKER)) {
		context_menu->add_item(TTR("Emoji & Symbols"), EDIT_EMOJI_AND_SYMBOL);
		context_menu->add_separator();
	}
	if (p_selection) {
		context_menu->add_shortcut(ED_GET_SHORTCUT("ui_cut"), EDIT_CUT);
		context_menu->add_shortcut(ED_GET_SHORTCUT("ui_copy"), EDIT_COPY);
	}

	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_paste"), EDIT_PASTE);
	context_menu->add_separator();
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_text_select_all"), EDIT_SELECT_ALL);
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_undo"), EDIT_UNDO);
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_redo"), EDIT_REDO);
	context_menu->add_separator();
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/indent"), EDIT_INDENT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/unindent"), EDIT_UNINDENT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_bookmark"), BOOKMARK_TOGGLE);

	if (p_selection) {
		context_menu->add_separator();
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_uppercase"), EDIT_TO_UPPERCASE);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_lowercase"), EDIT_TO_LOWERCASE);
	}
	if (p_can_fold || p_is_folded) {
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_fold_line"), EDIT_TOGGLE_FOLD_LINE);
	}

	const CodeEdit *tx = code_editor->get_text_editor();
	context_menu->set_item_disabled(context_menu->get_item_index(EDIT_UNDO), !tx->has_undo());
	context_menu->set_item_disabled(context_menu->get_item_index(EDIT_REDO), !tx->has_redo());

	context_menu->set_position(get_screen_position() + p_position);
	context_menu->reset_size();
	context_menu->popup();
}

void TextEditor::update_toggle_scripts_button() {
	code_editor->update_toggle_scripts_button();
}

TextEditor::TextEditor() {
	code_editor = memnew(CodeTextEditor);
	add_child(code_editor);
	code_editor->add_theme_constant_override("separation", 0);
	code_editor->connect("load_theme_settings", callable_mp(this, &TextEditor::_load_theme_settings));
	code_editor->connect("validate_script", callable_mp(this, &TextEditor::_validate_script));
	code_editor->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	code_editor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	code_editor->show_toggle_scripts_button();
	code_editor->set_toggle_list_control(ScriptEditor::get_singleton()->get_left_list_split());

	update_settings();

	code_editor->get_text_editor()->set_context_menu_enabled(false);
	code_editor->get_text_editor()->connect(SceneStringName(gui_input), callable_mp(this, &TextEditor::_text_edit_gui_input));

	context_menu = memnew(PopupMenu);
	add_child(context_menu);
	context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &TextEditor::_edit_option));

	edit_hb = memnew(HBoxContainer);

	edit_menu = memnew(MenuButton);
	edit_menu->set_shortcut_context(this);
	edit_hb->add_child(edit_menu);
	edit_menu->set_text(TTR("Edit"));
	edit_menu->set_switch_on_hover(true);
	edit_menu->connect("about_to_popup", callable_mp(this, &TextEditor::_prepare_edit_menu));
	edit_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TextEditor::_edit_option));

	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_undo"), EDIT_UNDO);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_redo"), EDIT_REDO);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_cut"), EDIT_CUT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_copy"), EDIT_COPY);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_paste"), EDIT_PASTE);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_text_select_all"), EDIT_SELECT_ALL);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/move_up"), EDIT_MOVE_LINE_UP);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/move_down"), EDIT_MOVE_LINE_DOWN);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/indent"), EDIT_INDENT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/unindent"), EDIT_UNINDENT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/delete_line"), EDIT_DELETE_LINE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_fold_line"), EDIT_TOGGLE_FOLD_LINE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/fold_all_lines"), EDIT_FOLD_ALL_LINES);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/unfold_all_lines"), EDIT_UNFOLD_ALL_LINES);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/duplicate_selection"), EDIT_DUPLICATE_SELECTION);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/duplicate_lines"), EDIT_DUPLICATE_LINES);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_word_wrap"), EDIT_TOGGLE_WORD_WRAP);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/trim_trailing_whitespace"), EDIT_TRIM_TRAILING_WHITESAPCE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/trim_final_newlines"), EDIT_TRIM_FINAL_NEWLINES);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_indent_to_spaces"), EDIT_CONVERT_INDENT_TO_SPACES);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_indent_to_tabs"), EDIT_CONVERT_INDENT_TO_TABS);

	edit_menu->get_popup()->add_separator();
	PopupMenu *convert_case = memnew(PopupMenu);
	edit_menu->get_popup()->add_submenu_node_item(TTR("Convert Case"), convert_case);
	convert_case->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_uppercase"), EDIT_TO_UPPERCASE);
	convert_case->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_lowercase"), EDIT_TO_LOWERCASE);
	convert_case->add_shortcut(ED_GET_SHORTCUT("script_text_editor/capitalize"), EDIT_CAPITALIZE);
	convert_case->connect(SceneStringName(id_pressed), callable_mp(this, &TextEditor::_edit_option));

	highlighter_menu = memnew(PopupMenu);
	edit_menu->get_popup()->add_submenu_node_item(TTR("Syntax Highlighter"), highlighter_menu);
	highlighter_menu->connect(SceneStringName(id_pressed), callable_mp(this, &TextEditor::_change_syntax_highlighter));

	Ref<EditorPlainTextSyntaxHighlighter> plain_highlighter;
	plain_highlighter.instantiate();
	add_syntax_highlighter(plain_highlighter);

	Ref<EditorStandardSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	add_syntax_highlighter(highlighter);
	set_syntax_highlighter(plain_highlighter);

	search_menu = memnew(MenuButton);
	search_menu->set_shortcut_context(this);
	edit_hb->add_child(search_menu);
	search_menu->set_text(TTR("Search"));
	search_menu->set_switch_on_hover(true);
	search_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TextEditor::_edit_option));

	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find"), SEARCH_FIND);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_next"), SEARCH_FIND_NEXT);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_previous"), SEARCH_FIND_PREV);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/replace"), SEARCH_REPLACE);
	search_menu->get_popup()->add_separator();
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_in_files"), SEARCH_IN_FILES);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/replace_in_files"), REPLACE_IN_FILES);

	MenuButton *goto_menu = memnew(MenuButton);
	goto_menu->set_shortcut_context(this);
	edit_hb->add_child(goto_menu);
	goto_menu->set_text(TTR("Go To"));
	goto_menu->set_switch_on_hover(true);
	goto_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TextEditor::_edit_option));

	goto_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_line"), SEARCH_GOTO_LINE);
	goto_menu->get_popup()->add_separator();

	bookmarks_menu = memnew(PopupMenu);
	goto_menu->get_popup()->add_submenu_node_item(TTR("Bookmarks"), bookmarks_menu);
	_update_bookmark_list();
	bookmarks_menu->connect("about_to_popup", callable_mp(this, &TextEditor::_update_bookmark_list));
	bookmarks_menu->connect("index_pressed", callable_mp(this, &TextEditor::_bookmark_item_pressed));

	goto_line_popup = memnew(GotoLinePopup);
	add_child(goto_line_popup);
}

TextEditor::~TextEditor() {
	highlighters.clear();
}

void TextEditor::validate() {
	code_editor->validate_script();
}
