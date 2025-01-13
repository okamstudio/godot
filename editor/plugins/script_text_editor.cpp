/**************************************************************************/
/*  script_text_editor.cpp                                                */
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

#include "script_text_editor.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/math/expression.h"
#include "core/os/keyboard.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_help.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_toaster.h"
#include "editor/plugins/editor_context_menu_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/split_container.h"

void ConnectionInfoDialog::ok_pressed() {
}

void ConnectionInfoDialog::popup_connections(const String &p_method, const Vector<Node *> &p_nodes) {
	method->set_text(p_method);

	tree->clear();
	TreeItem *root = tree->create_item();

	for (int i = 0; i < p_nodes.size(); i++) {
		List<Connection> all_connections;
		p_nodes[i]->get_signals_connected_to_this(&all_connections);

		for (const Connection &connection : all_connections) {
			if (connection.callable.get_method() != p_method) {
				continue;
			}

			TreeItem *node_item = tree->create_item(root);

			node_item->set_text(0, Object::cast_to<Node>(connection.signal.get_object())->get_name());
			node_item->set_icon(0, EditorNode::get_singleton()->get_object_icon(connection.signal.get_object(), "Node"));
			node_item->set_selectable(0, false);
			node_item->set_editable(0, false);

			node_item->set_text(1, connection.signal.get_name());
			Control *p = Object::cast_to<Control>(get_parent());
			node_item->set_icon(1, p->get_editor_theme_icon(SNAME("Slot")));
			node_item->set_selectable(1, false);
			node_item->set_editable(1, false);

			node_item->set_text(2, Object::cast_to<Node>(connection.callable.get_object())->get_name());
			node_item->set_icon(2, EditorNode::get_singleton()->get_object_icon(connection.callable.get_object(), "Node"));
			node_item->set_selectable(2, false);
			node_item->set_editable(2, false);
		}
	}

	popup_centered(Size2(600, 300) * EDSCALE);
}

ConnectionInfoDialog::ConnectionInfoDialog() {
	set_title(TTR("Connections to method:"));

	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->set_anchor_and_offset(SIDE_LEFT, Control::ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_TOP, Control::ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_RIGHT, Control::ANCHOR_END, -8 * EDSCALE);
	vbc->set_anchor_and_offset(SIDE_BOTTOM, Control::ANCHOR_END, -8 * EDSCALE);
	add_child(vbc);

	method = memnew(Label);
	method->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	vbc->add_child(method);

	tree = memnew(Tree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_columns(3);
	tree->set_hide_root(true);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, TTR("Source"));
	tree->set_column_title(1, TTR("Signal"));
	tree->set_column_title(2, TTR("Target"));
	vbc->add_child(tree);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_allow_rmb_select(true);
}

////////////////////////////////////////////////////////////////////////////////

Vector<String> ScriptTextEditor::get_functions() {
	CodeEdit *te = code_editor->get_text_editor();
	String text = te->get_text();
	List<String> fnc;

	if (script->get_language()->validate(text, script->get_path(), &fnc)) {
		//if valid rewrite functions to latest
		functions.clear();
		for (const String &E : fnc) {
			functions.push_back(E);
		}
	}

	return functions;
}

void ScriptTextEditor::apply_code() {
	if (script.is_null()) {
		return;
	}
	script->set_source_code(code_editor->get_text_editor()->get_text());
	script->update_exports();
	code_editor->get_text_editor()->get_syntax_highlighter()->update_cache();
}

Ref<Resource> ScriptTextEditor::get_edited_resource() const {
	return script;
}

void ScriptTextEditor::set_edited_resource(const Ref<Resource> &p_res) {
	ERR_FAIL_COND(script.is_valid());
	ERR_FAIL_COND(p_res.is_null());

	script = p_res;

	code_editor->get_text_editor()->set_text(script->get_source_code());
	code_editor->get_text_editor()->clear_undo_history();
	code_editor->get_text_editor()->tag_saved_version();

	emit_signal(SNAME("name_changed"));
	code_editor->update_line_and_column();
}

void ScriptTextEditor::enable_editor(Control *p_shortcut_context) {
	if (editor_enabled) {
		return;
	}

	editor_enabled = true;

	_enable_code_editor();

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

void ScriptTextEditor::_load_theme_settings() {
	CodeEdit *text_edit = code_editor->get_text_editor();

	Color updated_marked_line_color = EDITOR_GET("text_editor/theme/highlighting/mark_color");
	Color updated_safe_line_number_color = EDITOR_GET("text_editor/theme/highlighting/safe_line_number_color");
	Color updated_folded_code_region_color = EDITOR_GET("text_editor/theme/highlighting/folded_code_region_color");

	bool safe_line_number_color_updated = updated_safe_line_number_color != safe_line_number_color;
	bool marked_line_color_updated = updated_marked_line_color != marked_line_color;
	bool folded_code_region_color_updated = updated_folded_code_region_color != folded_code_region_color;
	if (safe_line_number_color_updated || marked_line_color_updated || folded_code_region_color_updated) {
		safe_line_number_color = updated_safe_line_number_color;
		for (int i = 0; i < text_edit->get_line_count(); i++) {
			if (marked_line_color_updated && text_edit->get_line_background_color(i) == marked_line_color) {
				text_edit->set_line_background_color(i, updated_marked_line_color);
			}

			if (safe_line_number_color_updated && text_edit->get_line_gutter_item_color(i, line_number_gutter) != default_line_number_color) {
				text_edit->set_line_gutter_item_color(i, line_number_gutter, safe_line_number_color);
			}

			if (folded_code_region_color_updated && text_edit->get_line_background_color(i) == folded_code_region_color) {
				text_edit->set_line_background_color(i, updated_folded_code_region_color);
			}
		}
		marked_line_color = updated_marked_line_color;
		folded_code_region_color = updated_folded_code_region_color;
	}

	theme_loaded = true;
	if (script.is_valid()) {
		_set_theme_for_script();
	}
}

void ScriptTextEditor::_set_theme_for_script() {
	if (!theme_loaded) {
		return;
	}

	CodeEdit *text_edit = code_editor->get_text_editor();
	text_edit->get_syntax_highlighter()->update_cache();

	List<String> strings;
	script->get_language()->get_string_delimiters(&strings);
	text_edit->clear_string_delimiters();
	for (const String &string : strings) {
		String beg = string.get_slice(" ", 0);
		String end = string.get_slice_count(" ") > 1 ? string.get_slice(" ", 1) : String();
		if (!text_edit->has_string_delimiter(beg)) {
			text_edit->add_string_delimiter(beg, end, end.is_empty());
		}

		if (!end.is_empty() && !text_edit->has_auto_brace_completion_open_key(beg)) {
			text_edit->add_auto_brace_completion_pair(beg, end);
		}
	}

	text_edit->clear_comment_delimiters();

	List<String> comments;
	script->get_language()->get_comment_delimiters(&comments);
	for (const String &comment : comments) {
		String beg = comment.get_slice(" ", 0);
		String end = comment.get_slice_count(" ") > 1 ? comment.get_slice(" ", 1) : String();
		text_edit->add_comment_delimiter(beg, end, end.is_empty());

		if (!end.is_empty() && !text_edit->has_auto_brace_completion_open_key(beg)) {
			text_edit->add_auto_brace_completion_pair(beg, end);
		}
	}

	List<String> doc_comments;
	script->get_language()->get_doc_comment_delimiters(&doc_comments);
	for (const String &doc_comment : doc_comments) {
		String beg = doc_comment.get_slice(" ", 0);
		String end = doc_comment.get_slice_count(" ") > 1 ? doc_comment.get_slice(" ", 1) : String();
		text_edit->add_comment_delimiter(beg, end, end.is_empty());

		if (!end.is_empty() && !text_edit->has_auto_brace_completion_open_key(beg)) {
			text_edit->add_auto_brace_completion_pair(beg, end);
		}
	}
}

void ScriptTextEditor::_show_errors_panel(bool p_show) {
	errors_panel->set_visible(p_show);
}

void ScriptTextEditor::_show_warnings_panel(bool p_show) {
	warnings_panel->set_visible(p_show);
}

void ScriptTextEditor::_warning_clicked(const Variant &p_line) {
	if (p_line.get_type() == Variant::INT) {
		goto_line_centered(p_line.operator int64_t());
	} else if (p_line.get_type() == Variant::DICTIONARY) {
		Dictionary meta = p_line.operator Dictionary();
		const int line = meta["line"].operator int64_t() - 1;
		const String code = meta["code"].operator String();
		const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";

		CodeEdit *text_editor = code_editor->get_text_editor();
		String prev_line = line > 0 ? text_editor->get_line(line - 1) : "";
		if (prev_line.contains("@warning_ignore")) {
			const int closing_bracket_idx = prev_line.find_char(')');
			const String text_to_insert = ", " + code.quote(quote_style);
			text_editor->insert_text(text_to_insert, line - 1, closing_bracket_idx);
		} else {
			const int indent = text_editor->get_indent_level(line) / text_editor->get_indent_size();
			String annotation_indent;
			if (!text_editor->is_indent_using_spaces()) {
				annotation_indent = String("\t").repeat(indent);
			} else {
				annotation_indent = String(" ").repeat(text_editor->get_indent_size() * indent);
			}
			text_editor->insert_line_at(line, annotation_indent + "@warning_ignore(" + code.quote(quote_style) + ")");
		}

		_validate_script();
	}
}

void ScriptTextEditor::_error_clicked(const Variant &p_line) {
	if (p_line.get_type() == Variant::INT) {
		goto_line_centered(p_line.operator int64_t());
	} else if (p_line.get_type() == Variant::DICTIONARY) {
		Dictionary meta = p_line.operator Dictionary();
		const String path = meta["path"].operator String();
		const int line = meta["line"].operator int64_t();
		const int column = meta["column"].operator int64_t();
		if (path.is_empty()) {
			goto_line_centered(line, column);
		} else {
			Ref<Resource> scr = ResourceLoader::load(path);
			if (scr.is_null()) {
				EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + path, TTR("Error!"));
			} else {
				int corrected_column = column;

				const String line_text = code_editor->get_text_editor()->get_line(line);
				const int indent_size = code_editor->get_text_editor()->get_indent_size();
				if (indent_size > 1) {
					const int tab_count = line_text.length() - line_text.lstrip("\t").length();
					corrected_column -= tab_count * (indent_size - 1);
				}

				ScriptEditor::get_singleton()->edit(scr, line, corrected_column);
			}
		}
	}
}

void ScriptTextEditor::reload_text() {
	ERR_FAIL_COND(script.is_null());

	CodeEdit *te = code_editor->get_text_editor();
	int column = te->get_caret_column();
	int row = te->get_caret_line();
	int h = te->get_h_scroll();
	int v = te->get_v_scroll();

	te->set_text(script->get_source_code());
	te->set_caret_line(row);
	te->set_caret_column(column);
	te->set_h_scroll(h);
	te->set_v_scroll(v);

	te->tag_saved_version();

	code_editor->update_line_and_column();
	if (editor_enabled) {
		_validate_script();
	}
}

void ScriptTextEditor::add_callback(const String &p_function, const PackedStringArray &p_args) {
	ScriptLanguage *language = script->get_language();
	if (!language->can_make_function()) {
		return;
	}
	code_editor->get_text_editor()->begin_complex_operation();
	code_editor->get_text_editor()->remove_secondary_carets();
	code_editor->get_text_editor()->deselect();
	String code = code_editor->get_text_editor()->get_text();
	int pos = language->find_function(p_function, code);
	if (pos == -1) {
		// Function does not exist, create it at the end of the file.
		int last_line = code_editor->get_text_editor()->get_line_count() - 1;
		String func = language->make_function("", p_function, p_args);
		code_editor->get_text_editor()->insert_text("\n\n" + func, last_line, code_editor->get_text_editor()->get_line(last_line).length());
		pos = last_line + 3;
	}
	// Put caret on the line after the function, after the indent.
	int indent_column = 1;
	if (EDITOR_GET("text_editor/behavior/indent/type")) {
		indent_column = EDITOR_GET("text_editor/behavior/indent/size");
	}
	code_editor->get_text_editor()->set_caret_line(pos, true, true, -1);
	code_editor->get_text_editor()->set_caret_column(indent_column);
	code_editor->get_text_editor()->end_complex_operation();
}

bool ScriptTextEditor::show_members_overview() {
	return true;
}

void ScriptTextEditor::update_settings() {
	code_editor->get_text_editor()->set_gutter_draw(connection_gutter, EDITOR_GET("text_editor/appearance/gutters/show_info_gutter"));
	code_editor->update_editor_settings();
}

bool ScriptTextEditor::is_unsaved() {
	const bool unsaved =
			code_editor->get_text_editor()->get_version() != code_editor->get_text_editor()->get_saved_version() ||
			script->get_path().is_empty(); // In memory.
	return unsaved;
}

Variant ScriptTextEditor::get_edit_state() {
	return code_editor->get_edit_state();
}

void ScriptTextEditor::set_edit_state(const Variant &p_state) {
	code_editor->set_edit_state(p_state);

	Dictionary state = p_state;
	if (state.has("syntax_highlighter")) {
		int idx = highlighter_menu->get_item_idx_from_text(state["syntax_highlighter"]);
		if (idx >= 0) {
			_change_syntax_highlighter(idx);
		}
	}

	if (editor_enabled) {
#ifndef ANDROID_ENABLED
		ensure_focus();
#endif
	}
}

Variant ScriptTextEditor::get_navigation_state() {
	return code_editor->get_navigation_state();
}

Variant ScriptTextEditor::get_previous_state() {
	return code_editor->get_previous_state();
}

void ScriptTextEditor::store_previous_state() {
	return code_editor->store_previous_state();
}

void ScriptTextEditor::_convert_case(CodeTextEditor::CaseStyle p_case) {
	code_editor->convert_case(p_case);
}

void ScriptTextEditor::trim_trailing_whitespace() {
	code_editor->trim_trailing_whitespace();
}

void ScriptTextEditor::trim_final_newlines() {
	code_editor->trim_final_newlines();
}

void ScriptTextEditor::insert_final_newline() {
	code_editor->insert_final_newline();
}

void ScriptTextEditor::convert_indent() {
	code_editor->get_text_editor()->convert_indent();
}

void ScriptTextEditor::tag_saved_version() {
	code_editor->get_text_editor()->tag_saved_version();
}

void ScriptTextEditor::goto_line(int p_line, int p_column) {
	code_editor->goto_line(p_line, p_column);
}

void ScriptTextEditor::goto_line_selection(int p_line, int p_begin, int p_end) {
	code_editor->goto_line_selection(p_line, p_begin, p_end);
}

void ScriptTextEditor::goto_line_centered(int p_line, int p_column) {
	code_editor->goto_line_centered(p_line, p_column);
}

void ScriptTextEditor::set_executing_line(int p_line) {
	code_editor->set_executing_line(p_line);
}

void ScriptTextEditor::clear_executing_line() {
	code_editor->clear_executing_line();
}

void ScriptTextEditor::ensure_focus() {
	code_editor->get_text_editor()->grab_focus();
}

String ScriptTextEditor::get_name() {
	String name;

	name = script->get_path().get_file();
	if (name.is_empty()) {
		// This appears for newly created built-in scripts before saving the scene.
		name = TTR("[unsaved]");
	} else if (script->is_built_in()) {
		const String &script_name = script->get_name();
		if (!script_name.is_empty()) {
			// If the built-in script has a custom resource name defined,
			// display the built-in script name as follows: `ResourceName (scene_file.tscn)`
			name = vformat("%s (%s)", script_name, name.get_slice("::", 0));
		}
	}

	if (is_unsaved()) {
		name += "(*)";
	}

	return name;
}

Ref<Texture2D> ScriptTextEditor::get_theme_icon() {
	if (get_parent_control()) {
		String icon_name = script->get_class();
		if (script->is_built_in()) {
			icon_name += "Internal";
		}

		if (get_parent_control()->has_theme_icon(icon_name, EditorStringName(EditorIcons))) {
			return get_parent_control()->get_editor_theme_icon(icon_name);
		} else if (get_parent_control()->has_theme_icon(script->get_class(), EditorStringName(EditorIcons))) {
			return get_parent_control()->get_editor_theme_icon(script->get_class());
		}
	}

	return Ref<Texture2D>();
}

void ScriptTextEditor::_validate_script() {
	CodeEdit *te = code_editor->get_text_editor();

	String text = te->get_text();
	List<String> fnc;

	warnings.clear();
	errors.clear();
	depended_errors.clear();
	safe_lines.clear();

	if (!script->get_language()->validate(text, script->get_path(), &fnc, &errors, &warnings, &safe_lines)) {
		List<ScriptLanguage::ScriptError>::Element *E = errors.front();
		while (E) {
			List<ScriptLanguage::ScriptError>::Element *next_E = E->next();
			if ((E->get().path.is_empty() && !script->get_path().is_empty()) || E->get().path != script->get_path()) {
				depended_errors[E->get().path].push_back(E->get());
				E->erase();
			}
			E = next_E;
		}

		if (errors.size() > 0) {
			// TRANSLATORS: Script error pointing to a line and column number.
			String error_text = vformat(TTR("Error at (%d, %d):"), errors.front()->get().line, errors.front()->get().column) + " " + errors.front()->get().message;
			code_editor->set_error(error_text);
			code_editor->set_error_pos(errors.front()->get().line - 1, errors.front()->get().column - 1);
		}
		script_is_valid = false;
	} else {
		code_editor->set_error("");
		if (!script->is_tool()) {
			script->set_source_code(text);
			script->update_exports();
			te->get_syntax_highlighter()->update_cache();
		}

		functions.clear();
		for (const String &E : fnc) {
			functions.push_back(E);
		}
		script_is_valid = true;
	}
	_update_connected_methods();
	_update_warnings();
	_update_errors();

	emit_signal(SNAME("name_changed"));
	emit_signal(SNAME("edited_script_changed"));
}

void ScriptTextEditor::_update_warnings() {
	int warning_nb = warnings.size();
	warnings_panel->clear();

	bool has_connections_table = false;
	// Add missing connections.
	if (GLOBAL_GET("debug/gdscript/warnings/enable")) {
		Node *base = get_tree()->get_edited_scene_root();
		if (base && missing_connections.size() > 0) {
			has_connections_table = true;
			warnings_panel->push_table(1);
			for (const Connection &connection : missing_connections) {
				String base_path = base->get_name();
				String source_path = base == connection.signal.get_object() ? base_path : base_path + "/" + base->get_path_to(Object::cast_to<Node>(connection.signal.get_object()));
				String target_path = base == connection.callable.get_object() ? base_path : base_path + "/" + base->get_path_to(Object::cast_to<Node>(connection.callable.get_object()));

				warnings_panel->push_cell();
				warnings_panel->push_color(warnings_panel->get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
				warnings_panel->add_text(vformat(TTR("Missing connected method '%s' for signal '%s' from node '%s' to node '%s'."), connection.callable.get_method(), connection.signal.get_name(), source_path, target_path));
				warnings_panel->pop(); // Color.
				warnings_panel->pop(); // Cell.
			}
			warnings_panel->pop(); // Table.

			warning_nb += missing_connections.size();
		}
	}

	code_editor->set_warning_count(warning_nb);

	if (has_connections_table) {
		warnings_panel->add_newline();
	}

	// Add script warnings.
	warnings_panel->push_table(3);
	for (const ScriptLanguage::Warning &w : warnings) {
		Dictionary ignore_meta;
		ignore_meta["line"] = w.start_line;
		ignore_meta["code"] = w.string_code.to_lower();
		warnings_panel->push_cell();
		warnings_panel->push_meta(ignore_meta);
		warnings_panel->push_color(
				warnings_panel->get_theme_color(SNAME("accent_color"), EditorStringName(Editor)).lerp(warnings_panel->get_theme_color(SNAME("mono_color"), EditorStringName(Editor)), 0.5f));
		warnings_panel->add_text(TTR("[Ignore]"));
		warnings_panel->pop(); // Color.
		warnings_panel->pop(); // Meta ignore.
		warnings_panel->pop(); // Cell.

		warnings_panel->push_cell();
		warnings_panel->push_meta(w.start_line - 1);
		warnings_panel->push_color(warnings_panel->get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		warnings_panel->add_text(vformat(TTR("Line %d (%s):"), w.start_line, w.string_code));
		warnings_panel->pop(); // Color.
		warnings_panel->pop(); // Meta goto.
		warnings_panel->pop(); // Cell.

		warnings_panel->push_cell();
		warnings_panel->add_text(w.message);
		warnings_panel->add_newline();
		warnings_panel->pop(); // Cell.
	}
	warnings_panel->pop(); // Table.
}

void ScriptTextEditor::_update_errors() {
	code_editor->set_error_count(errors.size());

	errors_panel->clear();
	errors_panel->push_table(2);
	for (const ScriptLanguage::ScriptError &err : errors) {
		Dictionary click_meta;
		click_meta["line"] = err.line;
		click_meta["column"] = err.column;

		errors_panel->push_cell();
		errors_panel->push_meta(err.line - 1);
		errors_panel->push_color(warnings_panel->get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		errors_panel->add_text(vformat(TTR("Line %d:"), err.line));
		errors_panel->pop(); // Color.
		errors_panel->pop(); // Meta goto.
		errors_panel->pop(); // Cell.

		errors_panel->push_cell();
		errors_panel->add_text(err.message);
		errors_panel->add_newline();
		errors_panel->pop(); // Cell.
	}
	errors_panel->pop(); // Table

	for (const KeyValue<String, List<ScriptLanguage::ScriptError>> &KV : depended_errors) {
		Dictionary click_meta;
		click_meta["path"] = KV.key;
		click_meta["line"] = 1;

		errors_panel->add_newline();
		errors_panel->add_newline();
		errors_panel->push_meta(click_meta);
		errors_panel->add_text(vformat(R"(%s:)", KV.key));
		errors_panel->pop(); // Meta goto.
		errors_panel->add_newline();

		errors_panel->push_indent(1);
		errors_panel->push_table(2);
		String filename = KV.key.get_file();
		for (const ScriptLanguage::ScriptError &err : KV.value) {
			click_meta["line"] = err.line;
			click_meta["column"] = err.column;

			errors_panel->push_cell();
			errors_panel->push_meta(click_meta);
			errors_panel->push_color(errors_panel->get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
			errors_panel->add_text(vformat(TTR("Line %d:"), err.line));
			errors_panel->pop(); // Color.
			errors_panel->pop(); // Meta goto.
			errors_panel->pop(); // Cell.

			errors_panel->push_cell();
			errors_panel->add_text(err.message);
			errors_panel->pop(); // Cell.
		}
		errors_panel->pop(); // Table
		errors_panel->pop(); // Indent.
	}

	CodeEdit *te = code_editor->get_text_editor();
	bool highlight_safe = EDITOR_GET("text_editor/appearance/gutters/highlight_type_safe_lines");
	bool last_is_safe = false;
	for (int i = 0; i < te->get_line_count(); i++) {
		if (errors.is_empty()) {
			bool is_folded_code_region = te->is_line_code_region_start(i) && te->is_line_folded(i);
			te->set_line_background_color(i, is_folded_code_region ? folded_code_region_color : Color(0, 0, 0, 0));
		} else {
			for (const ScriptLanguage::ScriptError &E : errors) {
				bool error_line = i == E.line - 1;
				te->set_line_background_color(i, error_line ? marked_line_color : Color(0, 0, 0, 0));
				if (error_line) {
					break;
				}
			}
		}

		if (highlight_safe) {
			if (safe_lines.has(i + 1)) {
				te->set_line_gutter_item_color(i, line_number_gutter, safe_line_number_color);
				last_is_safe = true;
			} else if (last_is_safe && (te->is_in_comment(i) != -1 || te->get_line(i).strip_edges().is_empty())) {
				te->set_line_gutter_item_color(i, line_number_gutter, safe_line_number_color);
			} else {
				te->set_line_gutter_item_color(i, line_number_gutter, default_line_number_color);
				last_is_safe = false;
			}
		} else {
			te->set_line_gutter_item_color(i, 1, default_line_number_color);
		}
	}
}

void ScriptTextEditor::_update_bookmark_list() {
	bookmarks_menu->clear();
	bookmarks_menu->reset_size();

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
		// Strip edges to remove spaces or tabs.
		// Also replace any tabs by spaces, since we can't print tabs in the menu.
		String line = code_editor->get_text_editor()->get_line(bookmark_list[i]).replace("\t", "  ").strip_edges();

		// Limit the size of the line if too big.
		if (line.length() > 50) {
			line = line.substr(0, 50);
		}

		bookmarks_menu->add_item(String::num((int)bookmark_list[i] + 1) + " - `" + line + "`");
		bookmarks_menu->set_item_metadata(-1, bookmark_list[i]);
	}
}

void ScriptTextEditor::_bookmark_item_pressed(int p_idx) {
	if (p_idx < 4) { // Any item before the separator.
		_edit_option(bookmarks_menu->get_item_id(p_idx));
	} else {
		code_editor->goto_line_centered(bookmarks_menu->get_item_metadata(p_idx));
	}
}

static Vector<Node *> _find_all_node_for_script(Node *p_base, Node *p_current, const Ref<Script> &p_script) {
	Vector<Node *> nodes;

	if (p_current->get_owner() != p_base && p_base != p_current) {
		return nodes;
	}

	Ref<Script> c = p_current->get_script();
	if (c == p_script) {
		nodes.push_back(p_current);
	}

	for (int i = 0; i < p_current->get_child_count(); i++) {
		Vector<Node *> found = _find_all_node_for_script(p_base, p_current->get_child(i), p_script);
		nodes.append_array(found);
	}

	return nodes;
}

static Node *_find_node_for_script(Node *p_base, Node *p_current, const Ref<Script> &p_script) {
	if (p_current->get_owner() != p_base && p_base != p_current) {
		return nullptr;
	}
	Ref<Script> c = p_current->get_script();
	if (c == p_script) {
		return p_current;
	}
	for (int i = 0; i < p_current->get_child_count(); i++) {
		Node *found = _find_node_for_script(p_base, p_current->get_child(i), p_script);
		if (found) {
			return found;
		}
	}

	return nullptr;
}

static void _find_changed_scripts_for_external_editor(Node *p_base, Node *p_current, HashSet<Ref<Script>> &r_scripts) {
	if (p_current->get_owner() != p_base && p_base != p_current) {
		return;
	}
	Ref<Script> c = p_current->get_script();

	if (c.is_valid()) {
		r_scripts.insert(c);
	}

	for (int i = 0; i < p_current->get_child_count(); i++) {
		_find_changed_scripts_for_external_editor(p_base, p_current->get_child(i), r_scripts);
	}
}

void ScriptEditor::_update_modified_scripts_for_external_editor(Ref<Script> p_for_script) {
	bool use_external_editor = bool(EDITOR_GET("text_editor/external/use_external_editor"));

	ERR_FAIL_NULL(get_tree());

	HashSet<Ref<Script>> scripts;

	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		_find_changed_scripts_for_external_editor(base, base, scripts);
	}

	for (const Ref<Script> &E : scripts) {
		Ref<Script> scr = E;

		if (!use_external_editor && !scr->get_language()->overrides_external_editor()) {
			continue; // We're not using an external editor for this script.
		}

		if (p_for_script.is_valid() && p_for_script != scr) {
			continue;
		}

		if (scr->is_built_in()) {
			continue; //internal script, who cares, though weird
		}

		uint64_t last_date = scr->get_last_modified_time();
		uint64_t date = FileAccess::get_modified_time(scr->get_path());

		if (last_date != date) {
			Ref<Script> rel_scr = ResourceLoader::load(scr->get_path(), scr->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
			ERR_CONTINUE(rel_scr.is_null());
			scr->set_source_code(rel_scr->get_source_code());
			scr->set_last_modified_time(rel_scr->get_last_modified_time());
			scr->update_exports();

			trigger_live_script_reload(scr->get_path());
		}
	}
}

void ScriptTextEditor::_code_complete_scripts(void *p_ud, const String &p_code, List<ScriptLanguage::CodeCompletionOption> *r_options, bool &r_force) {
	ScriptTextEditor *ste = (ScriptTextEditor *)p_ud;
	ste->_code_complete_script(p_code, r_options, r_force);
}

void ScriptTextEditor::_code_complete_script(const String &p_code, List<ScriptLanguage::CodeCompletionOption> *r_options, bool &r_force) {
	if (color_panel->is_visible()) {
		return;
	}
	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		base = _find_node_for_script(base, base, script);
	}
	String hint;
	Error err = script->get_language()->complete_code(p_code, script->get_path(), base, r_options, r_force, hint);

	if (err == OK) {
		code_editor->get_text_editor()->set_code_hint(hint);
	}
}

void ScriptTextEditor::_update_breakpoint_list() {
	breakpoints_menu->clear();
	breakpoints_menu->reset_size();

	breakpoints_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_breakpoint"), DEBUG_TOGGLE_BREAKPOINT);
	breakpoints_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/remove_all_breakpoints"), DEBUG_REMOVE_ALL_BREAKPOINTS);
	breakpoints_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_next_breakpoint"), DEBUG_GOTO_NEXT_BREAKPOINT);
	breakpoints_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_previous_breakpoint"), DEBUG_GOTO_PREV_BREAKPOINT);

	PackedInt32Array breakpoint_list = code_editor->get_text_editor()->get_breakpointed_lines();
	if (breakpoint_list.size() == 0) {
		return;
	}

	breakpoints_menu->add_separator();

	for (int i = 0; i < breakpoint_list.size(); i++) {
		// Strip edges to remove spaces or tabs.
		// Also replace any tabs by spaces, since we can't print tabs in the menu.
		String line = code_editor->get_text_editor()->get_line(breakpoint_list[i]).replace("\t", "  ").strip_edges();

		// Limit the size of the line if too big.
		if (line.length() > 50) {
			line = line.substr(0, 50);
		}

		breakpoints_menu->add_item(String::num((int)breakpoint_list[i] + 1) + " - `" + line + "`");
		breakpoints_menu->set_item_metadata(-1, breakpoint_list[i]);
	}
}

void ScriptTextEditor::_breakpoint_item_pressed(int p_idx) {
	if (p_idx < 4) { // Any item before the separator.
		_edit_option(breakpoints_menu->get_item_id(p_idx));
	} else {
		code_editor->goto_line_centered(breakpoints_menu->get_item_metadata(p_idx));
	}
}

void ScriptTextEditor::_breakpoint_toggled(int p_row) {
	EditorDebuggerNode::get_singleton()->set_breakpoint(script->get_path(), p_row + 1, code_editor->get_text_editor()->is_line_breakpointed(p_row));
}

void ScriptTextEditor::_on_caret_moved() {
	if (code_editor->is_previewing_navigation_change()) {
		return;
	}
	int current_line = code_editor->get_text_editor()->get_caret_line();
	if (ABS(current_line - previous_line) >= 10) {
		Dictionary nav_state = get_navigation_state();
		nav_state["row"] = previous_line;
		nav_state["scroll_position"] = -1;
		emit_signal(SNAME("request_save_previous_state"), nav_state);
		store_previous_state();
	}
	previous_line = current_line;
}

void ScriptTextEditor::_lookup_symbol(const String &p_symbol, int p_row, int p_column) {
	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		base = _find_node_for_script(base, base, script);
	}

	ScriptLanguage::LookupResult result;
	String code_text = code_editor->get_text_editor()->get_text_with_cursor_char(p_row, p_column);
	Error lc_error = script->get_language()->lookup_code(code_text, p_symbol, script->get_path(), base, result);
	if (ScriptServer::is_global_class(p_symbol)) {
		EditorNode::get_singleton()->load_resource(ScriptServer::get_global_class_path(p_symbol));
	} else if (p_symbol.is_resource_file() || p_symbol.begins_with("uid://")) {
		String symbol = p_symbol;
		if (symbol.begins_with("uid://")) {
			symbol = ResourceUID::uid_to_path(symbol);
		}

		List<String> scene_extensions;
		ResourceLoader::get_recognized_extensions_for_type("PackedScene", &scene_extensions);

		if (scene_extensions.find(symbol.get_extension())) {
			EditorNode::get_singleton()->load_scene(symbol);
		} else {
			EditorNode::get_singleton()->load_resource(symbol);
		}
	} else if (lc_error == OK) {
		_goto_line(p_row);

		if (!result.class_name.is_empty() && EditorHelp::get_doc_data()->class_list.has(result.class_name) && !EditorHelp::get_doc_data()->class_list[result.class_name].is_script_doc) {
			switch (result.type) {
				case ScriptLanguage::LOOKUP_RESULT_CLASS: {
					emit_signal(SNAME("go_to_help"), "class_name:" + result.class_name);
				} break;
				case ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT: {
					StringName cname = result.class_name;
					while (ClassDB::class_exists(cname)) {
						if (ClassDB::has_integer_constant(cname, result.class_member, true)) {
							result.class_name = cname;
							break;
						}
						cname = ClassDB::get_parent_class(cname);
					}
					emit_signal(SNAME("go_to_help"), "class_constant:" + result.class_name + ":" + result.class_member);
				} break;
				case ScriptLanguage::LOOKUP_RESULT_CLASS_PROPERTY: {
					StringName cname = result.class_name;
					while (ClassDB::class_exists(cname)) {
						if (ClassDB::has_property(cname, result.class_member, true)) {
							result.class_name = cname;
							break;
						}
						cname = ClassDB::get_parent_class(cname);
					}
					emit_signal(SNAME("go_to_help"), "class_property:" + result.class_name + ":" + result.class_member);
				} break;
				case ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD: {
					StringName cname = result.class_name;
					while (ClassDB::class_exists(cname)) {
						if (ClassDB::has_method(cname, result.class_member, true)) {
							result.class_name = cname;
							break;
						}
						cname = ClassDB::get_parent_class(cname);
					}
					emit_signal(SNAME("go_to_help"), "class_method:" + result.class_name + ":" + result.class_member);
				} break;
				case ScriptLanguage::LOOKUP_RESULT_CLASS_SIGNAL: {
					StringName cname = result.class_name;
					while (ClassDB::class_exists(cname)) {
						if (ClassDB::has_signal(cname, result.class_member, true)) {
							result.class_name = cname;
							break;
						}
						cname = ClassDB::get_parent_class(cname);
					}
					emit_signal(SNAME("go_to_help"), "class_signal:" + result.class_name + ":" + result.class_member);
				} break;
				case ScriptLanguage::LOOKUP_RESULT_CLASS_ENUM: {
					StringName cname = result.class_name;
					while (ClassDB::class_exists(cname)) {
						if (ClassDB::has_enum(cname, result.class_member, true)) {
							result.class_name = cname;
							break;
						}
						cname = ClassDB::get_parent_class(cname);
					}
					emit_signal(SNAME("go_to_help"), "class_enum:" + result.class_name + ":" + result.class_member);
				} break;
				case ScriptLanguage::LOOKUP_RESULT_CLASS_ANNOTATION: {
					emit_signal(SNAME("go_to_help"), "class_annotation:" + result.class_name + ":" + result.class_member);
				} break;
				case ScriptLanguage::LOOKUP_RESULT_CLASS_TBD_GLOBALSCOPE: { // Deprecated.
					emit_signal(SNAME("go_to_help"), "class_global:" + result.class_name + ":" + result.class_member);
				} break;
				case ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION:
				case ScriptLanguage::LOOKUP_RESULT_LOCAL_CONSTANT:
				case ScriptLanguage::LOOKUP_RESULT_LOCAL_VARIABLE:
				case ScriptLanguage::LOOKUP_RESULT_MAX: {
					// Nothing to do.
				} break;
			}
		} else if (result.location >= 0) {
			if (result.script.is_valid()) {
				emit_signal(SNAME("request_open_script_at_line"), result.script, result.location - 1);
			} else {
				emit_signal(SNAME("request_save_history"));
				goto_line_centered(result.location - 1);
			}
		}
	} else if (ProjectSettings::get_singleton()->has_autoload(p_symbol)) {
		// Check for Autoload scenes.
		const ProjectSettings::AutoloadInfo &info = ProjectSettings::get_singleton()->get_autoload(p_symbol);
		if (info.is_singleton) {
			EditorNode::get_singleton()->load_scene(info.path);
		}
	} else if (p_symbol.is_relative_path()) {
		// Every symbol other than absolute path is relative path so keep this condition at last.
		String path = _get_absolute_path(p_symbol);
		if (FileAccess::exists(path)) {
			List<String> scene_extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &scene_extensions);

			if (scene_extensions.find(path.get_extension())) {
				EditorNode::get_singleton()->load_scene(path);
			} else {
				EditorNode::get_singleton()->load_resource(path);
			}
		}
	}
}

void ScriptTextEditor::_validate_symbol(const String &p_symbol) {
	CodeEdit *text_edit = code_editor->get_text_editor();

	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		base = _find_node_for_script(base, base, script);
	}

	ScriptLanguage::LookupResult result;
	String lc_text = code_editor->get_text_editor()->get_text_for_symbol_lookup();
	Error lc_error = script->get_language()->lookup_code(lc_text, p_symbol, script->get_path(), base, result);
	bool is_singleton = ProjectSettings::get_singleton()->has_autoload(p_symbol) && ProjectSettings::get_singleton()->get_autoload(p_symbol).is_singleton;
	if (lc_error == OK || is_singleton || ScriptServer::is_global_class(p_symbol) || p_symbol.is_resource_file() || p_symbol.begins_with("uid://")) {
		text_edit->set_symbol_lookup_word_as_valid(true);
	} else if (p_symbol.is_relative_path()) {
		String path = _get_absolute_path(p_symbol);
		if (FileAccess::exists(path)) {
			text_edit->set_symbol_lookup_word_as_valid(true);
		} else {
			text_edit->set_symbol_lookup_word_as_valid(false);
		}
	} else {
		text_edit->set_symbol_lookup_word_as_valid(false);
	}
}

void ScriptTextEditor::_show_symbol_tooltip(const String &p_symbol, int p_row, int p_column) {
	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		base = _find_node_for_script(base, base, script);
	}

	ScriptLanguage::LookupResult result;
	String doc_symbol;
	const String code_text = code_editor->get_text_editor()->get_text_with_cursor_char(p_row, p_column);
	const Error lc_error = script->get_language()->lookup_code(code_text, p_symbol, script->get_path(), base, result);
	if (lc_error == OK) {
		switch (result.type) {
			case ScriptLanguage::LOOKUP_RESULT_CLASS: {
				doc_symbol = "class|" + result.class_name + "|";
			} break;
			case ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT: {
				StringName cname = result.class_name;
				while (ClassDB::class_exists(cname)) {
					if (ClassDB::has_integer_constant(cname, result.class_member, true)) {
						result.class_name = cname;
						break;
					}
					cname = ClassDB::get_parent_class(cname);
				}
				doc_symbol = "constant|" + result.class_name + "|" + result.class_member;
			} break;
			case ScriptLanguage::LOOKUP_RESULT_CLASS_PROPERTY: {
				StringName cname = result.class_name;
				while (ClassDB::class_exists(cname)) {
					if (ClassDB::has_property(cname, result.class_member, true)) {
						result.class_name = cname;
						break;
					}
					cname = ClassDB::get_parent_class(cname);
				}
				doc_symbol = "property|" + result.class_name + "|" + result.class_member;
			} break;
			case ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD: {
				StringName cname = result.class_name;
				while (ClassDB::class_exists(cname)) {
					if (ClassDB::has_method(cname, result.class_member, true)) {
						result.class_name = cname;
						break;
					}
					cname = ClassDB::get_parent_class(cname);
				}
				doc_symbol = "method|" + result.class_name + "|" + result.class_member;
			} break;
			case ScriptLanguage::LOOKUP_RESULT_CLASS_SIGNAL: {
				StringName cname = result.class_name;
				while (ClassDB::class_exists(cname)) {
					if (ClassDB::has_signal(cname, result.class_member, true)) {
						result.class_name = cname;
						break;
					}
					cname = ClassDB::get_parent_class(cname);
				}
				doc_symbol = "signal|" + result.class_name + "|" + result.class_member;
			} break;
			case ScriptLanguage::LOOKUP_RESULT_CLASS_ENUM: {
				StringName cname = result.class_name;
				while (ClassDB::class_exists(cname)) {
					if (ClassDB::has_enum(cname, result.class_member, true)) {
						result.class_name = cname;
						break;
					}
					cname = ClassDB::get_parent_class(cname);
				}
				doc_symbol = "enum|" + result.class_name + "|" + result.class_member;
			} break;
			case ScriptLanguage::LOOKUP_RESULT_CLASS_ANNOTATION: {
				doc_symbol = "annotation|" + result.class_name + "|" + result.class_member;
			} break;
			case ScriptLanguage::LOOKUP_RESULT_LOCAL_CONSTANT:
			case ScriptLanguage::LOOKUP_RESULT_LOCAL_VARIABLE: {
				const String item_type = (result.type == ScriptLanguage::LOOKUP_RESULT_LOCAL_CONSTANT) ? "local_constant" : "local_variable";
				Dictionary item_data;
				item_data["description"] = result.description;
				item_data["is_deprecated"] = result.is_deprecated;
				item_data["deprecated_message"] = result.deprecated_message;
				item_data["is_experimental"] = result.is_experimental;
				item_data["experimental_message"] = result.experimental_message;
				item_data["doc_type"] = result.doc_type;
				item_data["enumeration"] = result.enumeration;
				item_data["is_bitfield"] = result.is_bitfield;
				item_data["value"] = result.value;
				doc_symbol = item_type + "||" + p_symbol + "|" + JSON::stringify(item_data);
			} break;
			case ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION:
			case ScriptLanguage::LOOKUP_RESULT_CLASS_TBD_GLOBALSCOPE: // Deprecated.
			case ScriptLanguage::LOOKUP_RESULT_MAX: {
				// Nothing to do.
			} break;
		}
	}

	String debug_value = EditorDebuggerNode::get_singleton()->get_var_value(p_symbol);
	if (!debug_value.is_empty()) {
		constexpr int DISPLAY_LIMIT = 1024;
		if (debug_value.size() > DISPLAY_LIMIT) {
			debug_value = debug_value.left(DISPLAY_LIMIT) + "... " + TTR("(truncated)");
		}
		debug_value = debug_value.replace("[", "[lb]");

		if (doc_symbol.is_empty()) {
			debug_value = p_symbol + ": " + debug_value;
		} else {
			debug_value = TTR("Current value: ") + debug_value;
		}
	}

	if (!doc_symbol.is_empty() || !debug_value.is_empty()) {
		EditorHelpBitTooltip::show_tooltip(code_editor->get_text_editor(), doc_symbol, debug_value, true);
	}
}

String ScriptTextEditor::_get_absolute_path(const String &rel_path) {
	String base_path = script->get_path().get_base_dir();
	String path = base_path.path_join(rel_path);
	return path.replace("///", "//").simplify_path();
}

void ScriptTextEditor::update_toggle_scripts_button() {
	code_editor->update_toggle_scripts_button();
}

void ScriptTextEditor::_update_connected_methods() {
	CodeEdit *text_edit = code_editor->get_text_editor();
	text_edit->set_gutter_width(connection_gutter, text_edit->get_line_height());
	for (int i = 0; i < text_edit->get_line_count(); i++) {
		text_edit->set_line_gutter_metadata(i, connection_gutter, Dictionary());
		text_edit->set_line_gutter_icon(i, connection_gutter, nullptr);
		text_edit->set_line_gutter_clickable(i, connection_gutter, false);
	}
	missing_connections.clear();

	if (!script_is_valid) {
		return;
	}

	Node *base = get_tree()->get_edited_scene_root();
	if (!base) {
		return;
	}

	// Add connection icons to methods.
	Vector<Node *> nodes = _find_all_node_for_script(base, base, script);
	HashSet<StringName> methods_found;
	for (int i = 0; i < nodes.size(); i++) {
		List<Connection> signal_connections;
		nodes[i]->get_signals_connected_to_this(&signal_connections);

		for (const Connection &connection : signal_connections) {
			if (!(connection.flags & CONNECT_PERSIST)) {
				continue;
			}

			// As deleted nodes are still accessible via the undo/redo system, check if they're still on the tree.
			Node *source = Object::cast_to<Node>(connection.signal.get_object());
			if (source && !source->is_inside_tree()) {
				continue;
			}

			const StringName method = connection.callable.get_method();
			if (methods_found.has(method)) {
				continue;
			}

			if (!ClassDB::has_method(script->get_instance_base_type(), method)) {
				int line = -1;

				for (int j = 0; j < functions.size(); j++) {
					String name = functions[j].get_slice(":", 0);
					if (name == method) {
						Dictionary line_meta;
						line_meta["type"] = "connection";
						line_meta["method"] = method;
						line = functions[j].get_slice(":", 1).to_int() - 1;
						text_edit->set_line_gutter_metadata(line, connection_gutter, line_meta);
						text_edit->set_line_gutter_icon(line, connection_gutter, get_parent_control()->get_editor_theme_icon(SNAME("Slot")));
						text_edit->set_line_gutter_clickable(line, connection_gutter, true);
						methods_found.insert(method);
						break;
					}
				}

				if (line >= 0) {
					continue;
				}

				// There is a chance that the method is inherited from another script.
				bool found_inherited_function = false;
				Ref<Script> inherited_script = script->get_base_script();
				while (inherited_script.is_valid()) {
					if (inherited_script->has_method(method)) {
						found_inherited_function = true;
						break;
					}

					inherited_script = inherited_script->get_base_script();
				}

				if (!found_inherited_function) {
					missing_connections.push_back(connection);
				}
			}
		}
	}

	// Add override icons to methods.
	methods_found.clear();
	for (int i = 0; i < functions.size(); i++) {
		String raw_name = functions[i].get_slice(":", 0);
		StringName name = StringName(raw_name);
		if (methods_found.has(name)) {
			continue;
		}

		// Account for inner classes by stripping the class names from the method,
		// starting from the right since our inner class might be inside of another inner class.
		int pos = raw_name.rfind_char('.');
		if (pos != -1) {
			name = raw_name.substr(pos + 1);
		}

		String found_base_class;
		StringName base_class = script->get_instance_base_type();
		Ref<Script> inherited_script = script->get_base_script();
		while (inherited_script.is_valid()) {
			if (inherited_script->has_method(name)) {
				found_base_class = "script:" + inherited_script->get_path();
				break;
			}

			base_class = inherited_script->get_instance_base_type();
			inherited_script = inherited_script->get_base_script();
		}

		if (found_base_class.is_empty()) {
			while (base_class) {
				List<MethodInfo> methods;
				ClassDB::get_method_list(base_class, &methods, true);
				for (const MethodInfo &mi : methods) {
					if (mi.name == name) {
						found_base_class = "builtin:" + base_class;
						break;
					}
				}

				ClassDB::ClassInfo *base_class_ptr = ClassDB::classes.getptr(base_class)->inherits_ptr;
				if (base_class_ptr == nullptr) {
					break;
				}
				base_class = base_class_ptr->name;
			}
		}

		if (!found_base_class.is_empty()) {
			int line = functions[i].get_slice(":", 1).to_int() - 1;

			Dictionary line_meta = text_edit->get_line_gutter_metadata(line, connection_gutter);
			if (line_meta.is_empty()) {
				// Add override icon to gutter.
				line_meta["type"] = "inherits";
				line_meta["method"] = name;
				line_meta["base_class"] = found_base_class;
				text_edit->set_line_gutter_icon(line, connection_gutter, get_parent_control()->get_editor_theme_icon(SNAME("MethodOverride")));
				text_edit->set_line_gutter_clickable(line, connection_gutter, true);
			} else {
				// If method is also connected to signal, then merge icons and keep the click behavior of the slot.
				text_edit->set_line_gutter_icon(line, connection_gutter, get_parent_control()->get_editor_theme_icon(SNAME("MethodOverrideAndSlot")));
			}

			methods_found.insert(StringName(raw_name));
		}
	}
}

void ScriptTextEditor::_update_gutter_indexes() {
	for (int i = 0; i < code_editor->get_text_editor()->get_gutter_count(); i++) {
		if (code_editor->get_text_editor()->get_gutter_name(i) == "connection_gutter") {
			connection_gutter = i;
			continue;
		}

		if (code_editor->get_text_editor()->get_gutter_name(i) == "line_numbers") {
			line_number_gutter = i;
			continue;
		}
	}
}

void ScriptTextEditor::_gutter_clicked(int p_line, int p_gutter) {
	if (p_gutter != connection_gutter) {
		return;
	}

	Dictionary meta = code_editor->get_text_editor()->get_line_gutter_metadata(p_line, p_gutter);
	String type = meta.get("type", "");
	if (type.is_empty()) {
		return;
	}

	// All types currently need a method name.
	String method = meta.get("method", "");
	if (method.is_empty()) {
		return;
	}

	if (type == "connection") {
		Node *base = get_tree()->get_edited_scene_root();
		if (!base) {
			return;
		}

		Vector<Node *> nodes = _find_all_node_for_script(base, base, script);
		connection_info_dialog->popup_connections(method, nodes);
	} else if (type == "inherits") {
		String base_class_raw = meta["base_class"];
		PackedStringArray base_class_split = base_class_raw.split(":", true, 1);

		if (base_class_split[0] == "script") {
			// Go to function declaration.
			Ref<Script> base_script = ResourceLoader::load(base_class_split[1]);
			ERR_FAIL_COND(base_script.is_null());
			emit_signal(SNAME("go_to_method"), base_script, method);
		} else if (base_class_split[0] == "builtin") {
			// Open method documentation.
			emit_signal(SNAME("go_to_help"), "class_method:" + base_class_split[1] + ":" + method);
		}
	}
}

void ScriptTextEditor::_edit_option(int p_op) {
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
			Ref<Script> scr = script;
			if (scr.is_null()) {
				return;
			}
			tx->indent_lines();
		} break;
		case EDIT_UNINDENT: {
			Ref<Script> scr = script;
			if (scr.is_null()) {
				return;
			}
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
		case EDIT_CREATE_CODE_REGION: {
			tx->create_code_region();
		} break;
		case EDIT_TOGGLE_COMMENT: {
			_edit_option_toggle_inline_comment();
		} break;
		case EDIT_COMPLETE: {
			tx->request_code_completion(true);
		} break;
		case EDIT_AUTO_INDENT: {
			String text = tx->get_text();
			Ref<Script> scr = script;
			if (scr.is_null()) {
				return;
			}

			tx->begin_complex_operation();
			tx->begin_multicaret_edit();
			int begin = tx->get_line_count() - 1, end = 0;
			if (tx->has_selection()) {
				// Auto indent all lines that have a caret or selection on it.
				Vector<Point2i> line_ranges = tx->get_line_ranges_from_carets();
				for (Point2i line_range : line_ranges) {
					scr->get_language()->auto_indent_code(text, line_range.x, line_range.y);
					if (line_range.x < begin) {
						begin = line_range.x;
					}
					if (line_range.y > end) {
						end = line_range.y;
					}
				}
			} else {
				// Auto indent entire text.
				begin = 0;
				end = tx->get_line_count() - 1;
				scr->get_language()->auto_indent_code(text, begin, end);
			}

			// Apply auto indented code.
			Vector<String> lines = text.split("\n");
			for (int i = begin; i <= end; ++i) {
				tx->set_line(i, lines[i]);
			}

			tx->end_multicaret_edit();
			tx->end_complex_operation();
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
		case EDIT_PICK_COLOR: {
			color_panel->popup();
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
		case EDIT_EVALUATE: {
			Expression expression;
			tx->begin_complex_operation();
			for (int caret_idx = 0; caret_idx < tx->get_caret_count(); caret_idx++) {
				Vector<String> lines = tx->get_selected_text(caret_idx).split("\n");
				PackedStringArray results;

				for (int i = 0; i < lines.size(); i++) {
					const String &line = lines[i];
					String whitespace = line.substr(0, line.size() - line.strip_edges(true, false).size()); // Extract the whitespace at the beginning.
					if (expression.parse(line) == OK) {
						Variant result = expression.execute(Array(), Variant(), false, true);
						if (expression.get_error_text().is_empty()) {
							results.push_back(whitespace + result.get_construct_string());
						} else {
							results.push_back(line);
						}
					} else {
						results.push_back(line);
					}
				}
				tx->insert_text_at_caret(String("\n").join(results), caret_idx);
			}
			tx->end_complex_operation();
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
			String selected_text = tx->get_selected_text();

			// Yep, because it doesn't make sense to instance this dialog for every single script open...
			// So this will be delegated to the ScriptEditor.
			emit_signal(SNAME("search_in_files_requested"), selected_text);
		} break;
		case REPLACE_IN_FILES: {
			String selected_text = tx->get_selected_text();

			emit_signal(SNAME("replace_in_files_requested"), selected_text);
		} break;
		case SEARCH_LOCATE_FUNCTION: {
			quick_open->popup_dialog(get_functions());
			quick_open->set_title(TTR("Go to Function"));
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
		case DEBUG_TOGGLE_BREAKPOINT: {
			Vector<int> sorted_carets = tx->get_sorted_carets();
			int last_line = -1;
			for (const int &c : sorted_carets) {
				int from = tx->get_selection_from_line(c);
				from += from == last_line ? 1 : 0;
				int to = tx->get_selection_to_line(c);
				if (to < from) {
					continue;
				}
				// Check first if there's any lines with breakpoints in the selection.
				bool selection_has_breakpoints = false;
				for (int line = from; line <= to; line++) {
					if (tx->is_line_breakpointed(line)) {
						selection_has_breakpoints = true;
						break;
					}
				}

				// Set breakpoint on caret or remove all bookmarks from the selection.
				if (!selection_has_breakpoints) {
					if (tx->get_caret_line(c) != last_line) {
						tx->set_line_as_breakpoint(tx->get_caret_line(c), true);
					}
				} else {
					for (int line = from; line <= to; line++) {
						tx->set_line_as_breakpoint(line, false);
					}
				}
				last_line = to;
			}
		} break;
		case DEBUG_REMOVE_ALL_BREAKPOINTS: {
			PackedInt32Array bpoints = tx->get_breakpointed_lines();

			for (int i = 0; i < bpoints.size(); i++) {
				int line = bpoints[i];
				bool dobreak = !tx->is_line_breakpointed(line);
				tx->set_line_as_breakpoint(line, dobreak);
				EditorDebuggerNode::get_singleton()->set_breakpoint(script->get_path(), line + 1, dobreak);
			}
		} break;
		case DEBUG_GOTO_NEXT_BREAKPOINT: {
			PackedInt32Array bpoints = tx->get_breakpointed_lines();
			if (bpoints.size() <= 0) {
				return;
			}

			int current_line = tx->get_caret_line();
			int bpoint_idx = 0;
			if (current_line < (int)bpoints[bpoints.size() - 1]) {
				while (bpoint_idx < bpoints.size() && bpoints[bpoint_idx] <= current_line) {
					bpoint_idx++;
				}
			}
			code_editor->goto_line_centered(bpoints[bpoint_idx]);
		} break;
		case DEBUG_GOTO_PREV_BREAKPOINT: {
			PackedInt32Array bpoints = tx->get_breakpointed_lines();
			if (bpoints.size() <= 0) {
				return;
			}

			int current_line = tx->get_caret_line();
			int bpoint_idx = bpoints.size() - 1;
			if (current_line > (int)bpoints[0]) {
				while (bpoint_idx >= 0 && bpoints[bpoint_idx] >= current_line) {
					bpoint_idx--;
				}
			}
			code_editor->goto_line_centered(bpoints[bpoint_idx]);
		} break;
		case HELP_CONTEXTUAL: {
			String text = tx->get_selected_text(0);
			if (text.is_empty()) {
				text = tx->get_word_under_caret(0);
			}
			if (!text.is_empty()) {
				emit_signal(SNAME("request_help"), text);
			}
		} break;
		case LOOKUP_SYMBOL: {
			String text = tx->get_word_under_caret(0);
			if (text.is_empty()) {
				text = tx->get_selected_text(0);
			}
			if (!text.is_empty()) {
				_lookup_symbol(text, tx->get_caret_line(0), tx->get_caret_column(0));
			}
		} break;
		case EDIT_EMOJI_AND_SYMBOL: {
			code_editor->get_text_editor()->show_emoji_and_symbol_picker();
		} break;
		default: {
			if (p_op >= EditorContextMenuPlugin::BASE_ID) {
				EditorContextMenuPluginManager::get_singleton()->activate_custom_option(EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR_CODE, p_op, tx);
			}
		}
	}
}

void ScriptTextEditor::_edit_option_toggle_inline_comment() {
	if (script.is_null()) {
		return;
	}

	String delimiter = "#";
	List<String> comment_delimiters;
	script->get_language()->get_comment_delimiters(&comment_delimiters);

	for (const String &script_delimiter : comment_delimiters) {
		if (!script_delimiter.contains_char(' ')) {
			delimiter = script_delimiter;
			break;
		}
	}

	code_editor->toggle_inline_comment(delimiter);
}

void ScriptTextEditor::add_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) {
	ERR_FAIL_COND(p_highlighter.is_null());

	highlighters[p_highlighter->_get_name()] = p_highlighter;
	highlighter_menu->add_radio_check_item(p_highlighter->_get_name());
}

void ScriptTextEditor::set_syntax_highlighter(Ref<EditorSyntaxHighlighter> p_highlighter) {
	ERR_FAIL_COND(p_highlighter.is_null());

	HashMap<String, Ref<EditorSyntaxHighlighter>>::Iterator el = highlighters.begin();
	while (el) {
		int highlighter_index = highlighter_menu->get_item_idx_from_text(el->key);
		highlighter_menu->set_item_checked(highlighter_index, el->value == p_highlighter);
		++el;
	}

	CodeEdit *te = code_editor->get_text_editor();
	p_highlighter->_set_edited_resource(script);
	te->set_syntax_highlighter(p_highlighter);
}

void ScriptTextEditor::_change_syntax_highlighter(int p_idx) {
	set_syntax_highlighter(highlighters[highlighter_menu->get_item_text(p_idx)]);
}

void ScriptTextEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
			if (!editor_enabled) {
				break;
			}
			if (is_visible_in_tree()) {
				_update_warnings();
				_update_errors();
			}
			[[fallthrough]];
		case NOTIFICATION_ENTER_TREE: {
			code_editor->get_text_editor()->set_gutter_width(connection_gutter, code_editor->get_text_editor()->get_line_height());
		} break;
	}
}

Control *ScriptTextEditor::get_edit_menu() {
	return edit_hb;
}

void ScriptTextEditor::clear_edit_menu() {
	if (editor_enabled) {
		memdelete(edit_hb);
	}
}

void ScriptTextEditor::set_find_replace_bar(FindReplaceBar *p_bar) {
	code_editor->set_find_replace_bar(p_bar);
}

void ScriptTextEditor::reload(bool p_soft) {
	CodeEdit *te = code_editor->get_text_editor();
	Ref<Script> scr = script;
	if (scr.is_null()) {
		return;
	}
	scr->set_source_code(te->get_text());
	bool soft = p_soft || ClassDB::is_parent_class(scr->get_instance_base_type(), "EditorPlugin"); // Always soft-reload editor plugins.

	scr->get_language()->reload_tool_script(scr, soft);
}

PackedInt32Array ScriptTextEditor::get_breakpoints() {
	return code_editor->get_text_editor()->get_breakpointed_lines();
}

void ScriptTextEditor::set_breakpoint(int p_line, bool p_enabled) {
	code_editor->get_text_editor()->set_line_as_breakpoint(p_line, p_enabled);
}

void ScriptTextEditor::clear_breakpoints() {
	code_editor->get_text_editor()->clear_breakpointed_lines();
}

void ScriptTextEditor::set_tooltip_request_func(const Callable &p_toolip_callback) {
	Variant args[1] = { this };
	const Variant *argp[] = { &args[0] };
	code_editor->get_text_editor()->set_tooltip_request_func(p_toolip_callback.bindp(argp, 1));
}

void ScriptTextEditor::set_debugger_active(bool p_active) {
}

Control *ScriptTextEditor::get_base_editor() const {
	return code_editor->get_text_editor();
}

CodeTextEditor *ScriptTextEditor::get_code_editor() const {
	return code_editor;
}

Variant ScriptTextEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	return Variant();
}

bool ScriptTextEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary d = p_data;
	if (d.has("type") &&
			(String(d["type"]) == "resource" ||
					String(d["type"]) == "files" ||
					String(d["type"]) == "nodes" ||
					String(d["type"]) == "obj_property" ||
					String(d["type"]) == "files_and_dirs")) {
		return true;
	}

	return false;
}

static Node *_find_script_node(Node *p_edited_scene, Node *p_current_node, const Ref<Script> &script) {
	// Check scripts only for the nodes belonging to the edited scene.
	if (p_current_node == p_edited_scene || p_current_node->get_owner() == p_edited_scene) {
		Ref<Script> scr = p_current_node->get_script();
		if (scr.is_valid() && scr == script) {
			return p_current_node;
		}
	}

	// Traverse all children, even the ones not owned by the edited scene as they
	// can still have child nodes added within the edited scene and thus owned by
	// it (e.g. nodes added to subscene's root or to its editable children).
	for (int i = 0; i < p_current_node->get_child_count(); i++) {
		Node *n = _find_script_node(p_edited_scene, p_current_node->get_child(i), script);
		if (n) {
			return n;
		}
	}

	return nullptr;
}

static String _quote_drop_data(const String &str) {
	// This function prepares a string for being "dropped" into the script editor.
	// The string can be a resource path, node path or property name.

	const bool using_single_quotes = EDITOR_GET("text_editor/completion/use_single_quotes");

	String escaped = str.c_escape();

	// If string is double quoted, there is no need to escape single quotes.
	// We can revert the extra escaping added in c_escape().
	if (!using_single_quotes) {
		escaped = escaped.replace("\\'", "\'");
	}

	return escaped.quote(using_single_quotes ? "'" : "\"");
}

static String _get_dropped_resource_line(const Ref<Resource> &p_resource, bool p_create_field) {
	const String &path = p_resource->get_path();
	const bool is_script = ClassDB::is_parent_class(p_resource->get_class(), "Script");

	if (!p_create_field) {
		return vformat("preload(%s)", _quote_drop_data(path));
	}

	String variable_name = p_resource->get_name();
	if (variable_name.is_empty()) {
		variable_name = path.get_file().get_basename();
	}

	if (is_script) {
		variable_name = variable_name.to_pascal_case().validate_unicode_identifier();
	} else {
		variable_name = variable_name.to_snake_case().to_upper().validate_unicode_identifier();
	}
	return vformat("const %s = preload(%s)", variable_name, _quote_drop_data(path));
}

void ScriptTextEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	Dictionary d = p_data;

	CodeEdit *te = code_editor->get_text_editor();
	Point2i pos = te->get_line_column_at_pos(p_point);
	int drop_at_line = pos.y;
	int drop_at_column = pos.x;
	int selection_index = te->get_selection_at_line_column(drop_at_line, drop_at_column);

	bool line_will_be_empty = false;
	if (selection_index >= 0) {
		// Dropped on a selection, it will be replaced.
		drop_at_line = te->get_selection_from_line(selection_index);
		drop_at_column = te->get_selection_from_column(selection_index);
		line_will_be_empty = drop_at_column <= te->get_first_non_whitespace_column(drop_at_line) && te->get_selection_to_column(selection_index) == te->get_line(te->get_selection_to_line(selection_index)).length();
	}

	String text_to_drop;

	const bool drop_modifier_pressed = Input::get_singleton()->is_key_pressed(Key::CMD_OR_CTRL);
	const String &line = te->get_line(drop_at_line);
	const bool is_empty_line = line_will_be_empty || line.is_empty() || te->get_first_non_whitespace_column(drop_at_line) == line.length();

	const String type = d.get("type", "");
	if (type == "resource") {
		Ref<Resource> resource = d["resource"];
		if (resource.is_null()) {
			return;
		}

		const String &path = resource->get_path();
		if (path.is_empty() || path.ends_with("::")) {
			String warning = TTR("The resource does not have a valid path because it has not been saved.\nPlease save the scene or resource that contains this resource and try again.");
			EditorToaster::get_singleton()->popup_str(warning, EditorToaster::SEVERITY_ERROR);
			return;
		}

		if (drop_modifier_pressed) {
			if (resource->is_built_in()) {
				String warning = TTR("Preloading internal resources is not supported.");
				EditorToaster::get_singleton()->popup_str(warning, EditorToaster::SEVERITY_ERROR);
			} else {
				text_to_drop = _get_dropped_resource_line(resource, is_empty_line);
			}
		} else {
			text_to_drop = _quote_drop_data(path);
		}
	}

	if (type == "files" || type == "files_and_dirs") {
		const PackedStringArray files = d["files"];
		PackedStringArray parts;

		for (const String &path : files) {
			if (drop_modifier_pressed && ResourceLoader::exists(path)) {
				Ref<Resource> resource = ResourceLoader::load(path);
				if (resource.is_null()) {
					// Resource exists, but failed to load. We need only path and name, so we can use a dummy Resource instead.
					resource.instantiate();
					resource->set_path_cache(path);
				}
				parts.append(_get_dropped_resource_line(resource, is_empty_line));
			} else {
				parts.append(_quote_drop_data(path));
			}
		}
		text_to_drop = String(is_empty_line ? "\n" : ", ").join(parts);
	}

	if (type == "nodes") {
		Node *scene_root = get_tree()->get_edited_scene_root();
		if (!scene_root) {
			EditorNode::get_singleton()->show_warning(TTR("Can't drop nodes without an open scene."));
			return;
		}

		if (!ClassDB::is_parent_class(script->get_instance_base_type(), "Node")) {
			EditorToaster::get_singleton()->popup_str(vformat(TTR("Can't drop nodes because script '%s' does not inherit Node."), get_name()), EditorToaster::SEVERITY_WARNING);
			return;
		}

		Node *sn = _find_script_node(scene_root, scene_root, script);
		if (!sn) {
			sn = scene_root;
		}

		Array nodes = d["nodes"];

		if (drop_modifier_pressed) {
			const bool use_type = EDITOR_GET("text_editor/completion/add_type_hints");

			for (int i = 0; i < nodes.size(); i++) {
				NodePath np = nodes[i];
				Node *node = get_node(np);
				if (!node) {
					continue;
				}

				bool is_unique = false;
				String path;
				if (node->is_unique_name_in_owner()) {
					path = node->get_name();
					is_unique = true;
				} else {
					path = sn->get_path_to(node);
				}
				for (const String &segment : path.split("/")) {
					if (!segment.is_valid_unicode_identifier()) {
						path = _quote_drop_data(path);
						break;
					}
				}

				String variable_name = String(node->get_name()).to_snake_case().validate_unicode_identifier();
				if (use_type) {
					StringName class_name = node->get_class_name();
					Ref<Script> node_script = node->get_script();
					if (node_script.is_valid()) {
						StringName global_node_script_name = node_script->get_global_name();
						if (global_node_script_name != StringName()) {
							class_name = global_node_script_name;
						}
					}
					text_to_drop += vformat("@onready var %s: %s = %c%s\n", variable_name, class_name, is_unique ? '%' : '$', path);
				} else {
					text_to_drop += vformat("@onready var %s = %c%s\n", variable_name, is_unique ? '%' : '$', path);
				}
			}
		} else {
			for (int i = 0; i < nodes.size(); i++) {
				if (i > 0) {
					text_to_drop += ", ";
				}

				NodePath np = nodes[i];
				Node *node = get_node(np);
				if (!node) {
					continue;
				}

				bool is_unique = false;
				String path;
				if (node->is_unique_name_in_owner()) {
					path = node->get_name();
					is_unique = true;
				} else {
					path = sn->get_path_to(node);
				}

				for (const String &segment : path.split("/")) {
					if (!segment.is_valid_ascii_identifier()) {
						path = _quote_drop_data(path);
						break;
					}
				}
				text_to_drop += (is_unique ? "%" : "$") + path;
			}
		}
	}

	if (type == "obj_property") {
		bool add_literal = EDITOR_GET("text_editor/completion/add_node_path_literals");
		text_to_drop = add_literal ? "^" : "";
		// It is unclear whether properties may contain single or double quotes.
		// Assume here that double-quotes may not exist. We are escaping single-quotes if necessary.
		text_to_drop += _quote_drop_data(String(d["property"]));
	}

	if (text_to_drop.is_empty()) {
		return;
	}

	// Remove drag caret before any actions so it is not included in undo.
	te->remove_drag_caret();
	te->begin_complex_operation();
	if (selection_index >= 0) {
		te->delete_selection(selection_index);
	}
	te->remove_secondary_carets();
	te->deselect();
	te->set_caret_line(drop_at_line);
	te->set_caret_column(drop_at_column);
	te->insert_text_at_caret(text_to_drop);
	te->end_complex_operation();
	te->grab_focus();
}

void ScriptTextEditor::_text_edit_gui_input(const Ref<InputEvent> &ev) {
	Ref<InputEventMouseButton> mb = ev;
	Ref<InputEventKey> k = ev;
	Point2 local_pos;
	bool create_menu = false;

	CodeEdit *tx = code_editor->get_text_editor();
	if (mb.is_valid() && mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed()) {
		local_pos = mb->get_global_position() - tx->get_global_position();
		create_menu = true;
	} else if (k.is_valid() && k->is_action("ui_menu", true)) {
		tx->adjust_viewport_to_caret(0);
		local_pos = tx->get_caret_draw_pos(0);
		create_menu = true;
	}

	if (create_menu) {
		tx->apply_ime();

		Point2i pos = tx->get_line_column_at_pos(local_pos);
		int mouse_line = pos.y;
		int mouse_column = pos.x;

		tx->set_move_caret_on_right_click_enabled(EDITOR_GET("text_editor/behavior/navigation/move_caret_on_right_click"));
		int selection_clicked = -1;
		if (tx->is_move_caret_on_right_click_enabled()) {
			selection_clicked = tx->get_selection_at_line_column(mouse_line, mouse_column, true);
			if (selection_clicked < 0) {
				tx->deselect();
				tx->remove_secondary_carets();
				selection_clicked = 0;
				tx->set_caret_line(mouse_line, false, false, -1);
				tx->set_caret_column(mouse_column);
			}
		}

		String word_at_pos = tx->get_word_at_pos(local_pos);
		if (word_at_pos.is_empty()) {
			word_at_pos = tx->get_word_under_caret(selection_clicked);
		}
		if (word_at_pos.is_empty()) {
			word_at_pos = tx->get_selected_text(selection_clicked);
		}

		bool has_color = (word_at_pos == "Color");
		bool foldable = tx->can_fold_line(mouse_line) || tx->is_line_folded(mouse_line);
		bool open_docs = false;
		bool goto_definition = false;

		if (ScriptServer::is_global_class(word_at_pos) || word_at_pos.is_resource_file()) {
			open_docs = true;
		} else {
			Node *base = get_tree()->get_edited_scene_root();
			if (base) {
				base = _find_node_for_script(base, base, script);
			}
			ScriptLanguage::LookupResult result;
			if (script->get_language()->lookup_code(tx->get_text_for_symbol_lookup(), word_at_pos, script->get_path(), base, result) == OK) {
				open_docs = true;
			}
		}

		if (has_color) {
			String line = tx->get_line(mouse_line);
			color_position.x = mouse_line;
			color_position.y = mouse_column;

			int begin = -1;
			int end = -1;
			enum EXPRESSION_PATTERNS {
				NOT_PARSED,
				RGBA_PARAMETER, // Color(float,float,float) or Color(float,float,float,float)
				COLOR_NAME, // Color.COLOR_NAME
			} expression_pattern = NOT_PARSED;

			for (int i = mouse_column; i < line.length(); i++) {
				if (line[i] == '(') {
					if (expression_pattern == NOT_PARSED) {
						begin = i;
						expression_pattern = RGBA_PARAMETER;
					} else {
						// Method call or '(' appearing twice.
						expression_pattern = NOT_PARSED;

						break;
					}
				} else if (expression_pattern == RGBA_PARAMETER && line[i] == ')' && end < 0) {
					end = i + 1;

					break;
				} else if (expression_pattern == NOT_PARSED && line[i] == '.') {
					begin = i;
					expression_pattern = COLOR_NAME;
				} else if (expression_pattern == COLOR_NAME && end < 0 && (line[i] == ' ' || line[i] == '\t')) {
					// Including '.' and spaces.
					continue;
				} else if (expression_pattern == COLOR_NAME && !(line[i] == '_' || ('A' <= line[i] && line[i] <= 'Z'))) {
					end = i;

					break;
				}
			}

			switch (expression_pattern) {
				case RGBA_PARAMETER: {
					color_args = line.substr(begin, end - begin);
					String stripped = color_args.replace(" ", "").replace("\t", "").replace("(", "").replace(")", "");
					PackedFloat64Array color = stripped.split_floats(",");
					if (color.size() > 2) {
						float alpha = color.size() > 3 ? color[3] : 1.0f;
						color_picker->set_pick_color(Color(color[0], color[1], color[2], alpha));
					}
				} break;
				case COLOR_NAME: {
					if (end < 0) {
						end = line.length();
					}
					color_args = line.substr(begin, end - begin);
					const String color_name = color_args.replace(" ", "").replace("\t", "").replace(".", "");
					const int color_index = Color::find_named_color(color_name);
					if (0 <= color_index) {
						const Color color_constant = Color::get_named_color(color_index);
						color_picker->set_pick_color(color_constant);
					} else {
						has_color = false;
					}
				} break;
				default:
					has_color = false;
					break;
			}
			if (has_color) {
				color_panel->set_position(get_screen_position() + local_pos);
			}
		}
		_make_context_menu(tx->has_selection(), has_color, foldable, open_docs, goto_definition, local_pos);
	}
}

void ScriptTextEditor::_color_changed(const Color &p_color) {
	String new_args;
	const int decimals = 3;
	if (p_color.a == 1.0f) {
		new_args = String("(" + String::num(p_color.r, decimals) + ", " + String::num(p_color.g, decimals) + ", " + String::num(p_color.b, decimals) + ")");
	} else {
		new_args = String("(" + String::num(p_color.r, decimals) + ", " + String::num(p_color.g, decimals) + ", " + String::num(p_color.b, decimals) + ", " + String::num(p_color.a, decimals) + ")");
	}

	String line = code_editor->get_text_editor()->get_line(color_position.x);
	String line_with_replaced_args = line.replace(color_args, new_args);

	color_args = new_args;
	code_editor->get_text_editor()->begin_complex_operation();
	code_editor->get_text_editor()->set_line(color_position.x, line_with_replaced_args);
	code_editor->get_text_editor()->end_complex_operation();
}

void ScriptTextEditor::_prepare_edit_menu() {
	const CodeEdit *tx = code_editor->get_text_editor();
	PopupMenu *popup = edit_menu->get_popup();
	popup->set_item_disabled(popup->get_item_index(EDIT_UNDO), !tx->has_undo());
	popup->set_item_disabled(popup->get_item_index(EDIT_REDO), !tx->has_redo());
}

void ScriptTextEditor::_make_context_menu(bool p_selection, bool p_color, bool p_foldable, bool p_open_docs, bool p_goto_definition, Vector2 p_pos) {
	context_menu->clear();
	if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_EMOJI_AND_SYMBOL_PICKER)) {
		context_menu->add_item(TTR("Emoji & Symbols"), EDIT_EMOJI_AND_SYMBOL);
		context_menu->add_separator();
	}
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_undo"), EDIT_UNDO);
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_redo"), EDIT_REDO);

	context_menu->add_separator();
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_cut"), EDIT_CUT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_copy"), EDIT_COPY);
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_paste"), EDIT_PASTE);

	context_menu->add_separator();
	context_menu->add_shortcut(ED_GET_SHORTCUT("ui_text_select_all"), EDIT_SELECT_ALL);

	context_menu->add_separator();
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/indent"), EDIT_INDENT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/unindent"), EDIT_UNINDENT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_comment"), EDIT_TOGGLE_COMMENT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_bookmark"), BOOKMARK_TOGGLE);

	if (p_selection) {
		context_menu->add_separator();
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_uppercase"), EDIT_TO_UPPERCASE);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_lowercase"), EDIT_TO_LOWERCASE);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/evaluate_selection"), EDIT_EVALUATE);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/create_code_region"), EDIT_CREATE_CODE_REGION);
	}
	if (p_foldable) {
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_fold_line"), EDIT_TOGGLE_FOLD_LINE);
	}

	if (p_color || p_open_docs || p_goto_definition) {
		context_menu->add_separator();
		if (p_open_docs) {
			context_menu->add_item(TTR("Lookup Symbol"), LOOKUP_SYMBOL);
		}
		if (p_color) {
			context_menu->add_item(TTR("Pick Color"), EDIT_PICK_COLOR);
		}
	}

	const PackedStringArray paths = { code_editor->get_text_editor()->get_path() };
	EditorContextMenuPluginManager::get_singleton()->add_options_from_plugins(context_menu, EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR_CODE, paths);

	const CodeEdit *tx = code_editor->get_text_editor();
	context_menu->set_item_disabled(context_menu->get_item_index(EDIT_UNDO), !tx->has_undo());
	context_menu->set_item_disabled(context_menu->get_item_index(EDIT_REDO), !tx->has_redo());

	context_menu->set_position(get_screen_position() + p_pos);
	context_menu->reset_size();
	context_menu->popup();
}

void ScriptTextEditor::_enable_code_editor() {
	ERR_FAIL_COND(code_editor->get_parent());

	VSplitContainer *editor_box = memnew(VSplitContainer);
	add_child(editor_box);
	editor_box->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	editor_box->set_v_size_flags(SIZE_EXPAND_FILL);

	editor_box->add_child(code_editor);
	code_editor->connect("show_errors_panel", callable_mp(this, &ScriptTextEditor::_show_errors_panel));
	code_editor->connect("show_warnings_panel", callable_mp(this, &ScriptTextEditor::_show_warnings_panel));
	code_editor->connect("validate_script", callable_mp(this, &ScriptTextEditor::_validate_script));
	code_editor->connect("load_theme_settings", callable_mp(this, &ScriptTextEditor::_load_theme_settings));
	code_editor->get_text_editor()->connect("symbol_lookup", callable_mp(this, &ScriptTextEditor::_lookup_symbol));
	code_editor->get_text_editor()->connect("symbol_hovered", callable_mp(this, &ScriptTextEditor::_show_symbol_tooltip));
	code_editor->get_text_editor()->connect("symbol_validate", callable_mp(this, &ScriptTextEditor::_validate_symbol));
	code_editor->get_text_editor()->connect("gutter_added", callable_mp(this, &ScriptTextEditor::_update_gutter_indexes));
	code_editor->get_text_editor()->connect("gutter_removed", callable_mp(this, &ScriptTextEditor::_update_gutter_indexes));
	code_editor->get_text_editor()->connect("gutter_clicked", callable_mp(this, &ScriptTextEditor::_gutter_clicked));
	code_editor->get_text_editor()->connect(SceneStringName(gui_input), callable_mp(this, &ScriptTextEditor::_text_edit_gui_input));
	code_editor->show_toggle_scripts_button();
	_update_gutter_indexes();

	editor_box->add_child(warnings_panel);
	warnings_panel->add_theme_font_override(
			"normal_font", EditorNode::get_singleton()->get_editor_theme()->get_font(SNAME("main"), EditorStringName(EditorFonts)));
	warnings_panel->add_theme_font_size_override(
			"normal_font_size", EditorNode::get_singleton()->get_editor_theme()->get_font_size(SNAME("main_size"), EditorStringName(EditorFonts)));
	warnings_panel->connect("meta_clicked", callable_mp(this, &ScriptTextEditor::_warning_clicked));

	editor_box->add_child(errors_panel);
	errors_panel->add_theme_font_override(
			"normal_font", EditorNode::get_singleton()->get_editor_theme()->get_font(SNAME("main"), EditorStringName(EditorFonts)));
	errors_panel->add_theme_font_size_override(
			"normal_font_size", EditorNode::get_singleton()->get_editor_theme()->get_font_size(SNAME("main_size"), EditorStringName(EditorFonts)));
	errors_panel->connect("meta_clicked", callable_mp(this, &ScriptTextEditor::_error_clicked));

	add_child(context_menu);
	context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_edit_option));

	add_child(color_panel);

	color_picker = memnew(ColorPicker);
	color_picker->set_deferred_mode(true);
	color_picker->connect("color_changed", callable_mp(this, &ScriptTextEditor::_color_changed));
	color_panel->connect("about_to_popup", callable_mp(EditorNode::get_singleton(), &EditorNode::setup_color_picker).bind(color_picker));

	color_panel->add_child(color_picker);

	quick_open = memnew(ScriptEditorQuickOpen);
	quick_open->connect("goto_line", callable_mp(this, &ScriptTextEditor::_goto_line));
	add_child(quick_open);

	goto_line_popup = memnew(GotoLinePopup);
	add_child(goto_line_popup);

	add_child(connection_info_dialog);

	edit_hb->add_child(edit_menu);
	edit_menu->connect("about_to_popup", callable_mp(this, &ScriptTextEditor::_prepare_edit_menu));
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_undo"), EDIT_UNDO);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_redo"), EDIT_REDO);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_cut"), EDIT_CUT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_copy"), EDIT_COPY);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_paste"), EDIT_PASTE);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_text_select_all"), EDIT_SELECT_ALL);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/duplicate_selection"), EDIT_DUPLICATE_SELECTION);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/duplicate_lines"), EDIT_DUPLICATE_LINES);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/evaluate_selection"), EDIT_EVALUATE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_word_wrap"), EDIT_TOGGLE_WORD_WRAP);
	edit_menu->get_popup()->add_separator();
	{
		PopupMenu *sub_menu = memnew(PopupMenu);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/move_up"), EDIT_MOVE_LINE_UP);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/move_down"), EDIT_MOVE_LINE_DOWN);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/indent"), EDIT_INDENT);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/unindent"), EDIT_UNINDENT);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/delete_line"), EDIT_DELETE_LINE);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_comment"), EDIT_TOGGLE_COMMENT);
		sub_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_edit_option));
		edit_menu->get_popup()->add_submenu_node_item(TTR("Line"), sub_menu);
	}
	{
		PopupMenu *sub_menu = memnew(PopupMenu);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_fold_line"), EDIT_TOGGLE_FOLD_LINE);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/fold_all_lines"), EDIT_FOLD_ALL_LINES);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/unfold_all_lines"), EDIT_UNFOLD_ALL_LINES);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/create_code_region"), EDIT_CREATE_CODE_REGION);
		sub_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_edit_option));
		edit_menu->get_popup()->add_submenu_node_item(TTR("Folding"), sub_menu);
	}
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_text_completion_query"), EDIT_COMPLETE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/trim_trailing_whitespace"), EDIT_TRIM_TRAILING_WHITESAPCE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/trim_final_newlines"), EDIT_TRIM_FINAL_NEWLINES);
	{
		PopupMenu *sub_menu = memnew(PopupMenu);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_indent_to_spaces"), EDIT_CONVERT_INDENT_TO_SPACES);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_indent_to_tabs"), EDIT_CONVERT_INDENT_TO_TABS);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/auto_indent"), EDIT_AUTO_INDENT);
		sub_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_edit_option));
		edit_menu->get_popup()->add_submenu_node_item(TTR("Indentation"), sub_menu);
	}
	edit_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_edit_option));
	edit_menu->get_popup()->add_separator();
	{
		PopupMenu *sub_menu = memnew(PopupMenu);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_uppercase"), EDIT_TO_UPPERCASE);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_lowercase"), EDIT_TO_LOWERCASE);
		sub_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/capitalize"), EDIT_CAPITALIZE);
		sub_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_edit_option));
		edit_menu->get_popup()->add_submenu_node_item(TTR("Convert Case"), sub_menu);
	}
	edit_menu->get_popup()->add_submenu_node_item(TTR("Syntax Highlighter"), highlighter_menu);
	highlighter_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_change_syntax_highlighter));

	edit_hb->add_child(search_menu);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find"), SEARCH_FIND);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_next"), SEARCH_FIND_NEXT);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_previous"), SEARCH_FIND_PREV);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/replace"), SEARCH_REPLACE);
	search_menu->get_popup()->add_separator();
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_in_files"), SEARCH_IN_FILES);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/replace_in_files"), REPLACE_IN_FILES);
	search_menu->get_popup()->add_separator();
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/contextual_help"), HELP_CONTEXTUAL);
	search_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_edit_option));

	_load_theme_settings();

	edit_hb->add_child(goto_menu);
	goto_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_function"), SEARCH_LOCATE_FUNCTION);
	goto_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_line"), SEARCH_GOTO_LINE);
	goto_menu->get_popup()->add_separator();

	goto_menu->get_popup()->add_submenu_node_item(TTR("Bookmarks"), bookmarks_menu);
	_update_bookmark_list();
	bookmarks_menu->connect("about_to_popup", callable_mp(this, &ScriptTextEditor::_update_bookmark_list));
	bookmarks_menu->connect("index_pressed", callable_mp(this, &ScriptTextEditor::_bookmark_item_pressed));

	goto_menu->get_popup()->add_submenu_node_item(TTR("Breakpoints"), breakpoints_menu);
	_update_breakpoint_list();
	breakpoints_menu->connect("about_to_popup", callable_mp(this, &ScriptTextEditor::_update_breakpoint_list));
	breakpoints_menu->connect("index_pressed", callable_mp(this, &ScriptTextEditor::_breakpoint_item_pressed));

	goto_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptTextEditor::_edit_option));
}

ScriptTextEditor::ScriptTextEditor() {
	code_editor = memnew(CodeTextEditor);
	code_editor->set_toggle_list_control(ScriptEditor::get_singleton()->get_left_list_split());
	code_editor->add_theme_constant_override("separation", 2);
	code_editor->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	code_editor->set_code_complete_func(_code_complete_scripts, this);
	code_editor->set_v_size_flags(SIZE_EXPAND_FILL);

	code_editor->get_text_editor()->set_draw_breakpoints_gutter(true);
	code_editor->get_text_editor()->set_draw_executing_lines_gutter(true);
	code_editor->get_text_editor()->connect("breakpoint_toggled", callable_mp(this, &ScriptTextEditor::_breakpoint_toggled));
	code_editor->get_text_editor()->connect("caret_changed", callable_mp(this, &ScriptTextEditor::_on_caret_moved));
	code_editor->connect("navigation_preview_ended", callable_mp(this, &ScriptTextEditor::_on_caret_moved));

	connection_gutter = 1;
	code_editor->get_text_editor()->add_gutter(connection_gutter);
	code_editor->get_text_editor()->set_gutter_name(connection_gutter, "connection_gutter");
	code_editor->get_text_editor()->set_gutter_draw(connection_gutter, false);
	code_editor->get_text_editor()->set_gutter_overwritable(connection_gutter, true);
	code_editor->get_text_editor()->set_gutter_type(connection_gutter, TextEdit::GUTTER_TYPE_ICON);

	warnings_panel = memnew(RichTextLabel);
	warnings_panel->set_custom_minimum_size(Size2(0, 100 * EDSCALE));
	warnings_panel->set_h_size_flags(SIZE_EXPAND_FILL);
	warnings_panel->set_meta_underline(true);
	warnings_panel->set_selection_enabled(true);
	warnings_panel->set_context_menu_enabled(true);
	warnings_panel->set_focus_mode(FOCUS_CLICK);
	warnings_panel->hide();

	errors_panel = memnew(RichTextLabel);
	errors_panel->set_custom_minimum_size(Size2(0, 100 * EDSCALE));
	errors_panel->set_h_size_flags(SIZE_EXPAND_FILL);
	errors_panel->set_meta_underline(true);
	errors_panel->set_selection_enabled(true);
	errors_panel->set_context_menu_enabled(true);
	errors_panel->set_focus_mode(FOCUS_CLICK);
	errors_panel->hide();

	update_settings();

	code_editor->get_text_editor()->set_symbol_lookup_on_click_enabled(true);
	code_editor->get_text_editor()->set_symbol_tooltip_on_hover_enabled(true);
	code_editor->get_text_editor()->set_context_menu_enabled(false);

	context_menu = memnew(PopupMenu);

	color_panel = memnew(PopupPanel);

	edit_hb = memnew(HBoxContainer);

	edit_menu = memnew(MenuButton);
	edit_menu->set_text(TTR("Edit"));
	edit_menu->set_switch_on_hover(true);
	edit_menu->set_shortcut_context(this);

	highlighter_menu = memnew(PopupMenu);

	Ref<EditorPlainTextSyntaxHighlighter> plain_highlighter;
	plain_highlighter.instantiate();
	add_syntax_highlighter(plain_highlighter);

	Ref<EditorStandardSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	add_syntax_highlighter(highlighter);
	set_syntax_highlighter(highlighter);

	search_menu = memnew(MenuButton);
	search_menu->set_text(TTR("Search"));
	search_menu->set_switch_on_hover(true);
	search_menu->set_shortcut_context(this);

	goto_menu = memnew(MenuButton);
	goto_menu->set_text(TTR("Go To"));
	goto_menu->set_switch_on_hover(true);
	goto_menu->set_shortcut_context(this);

	bookmarks_menu = memnew(PopupMenu);
	breakpoints_menu = memnew(PopupMenu);

	connection_info_dialog = memnew(ConnectionInfoDialog);

	SET_DRAG_FORWARDING_GCD(code_editor->get_text_editor(), ScriptTextEditor);
}

ScriptTextEditor::~ScriptTextEditor() {
	highlighters.clear();

	if (!editor_enabled) {
		memdelete(code_editor);
		memdelete(warnings_panel);
		memdelete(errors_panel);
		memdelete(context_menu);
		memdelete(color_panel);
		memdelete(edit_hb);
		memdelete(edit_menu);
		memdelete(highlighter_menu);
		memdelete(search_menu);
		memdelete(goto_menu);
		memdelete(bookmarks_menu);
		memdelete(breakpoints_menu);
		memdelete(connection_info_dialog);
	}
}

static ScriptEditorBase *create_editor(const Ref<Resource> &p_resource) {
	if (Object::cast_to<Script>(*p_resource)) {
		return memnew(ScriptTextEditor);
	}
	return nullptr;
}

void ScriptTextEditor::register_editor() {
	ED_SHORTCUT("script_text_editor/move_up", TTRC("Move Up"), KeyModifierMask::ALT | Key::UP);
	ED_SHORTCUT("script_text_editor/move_down", TTRC("Move Down"), KeyModifierMask::ALT | Key::DOWN);
	ED_SHORTCUT("script_text_editor/delete_line", TTRC("Delete Line"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::K);

	// Leave these at zero, same can be accomplished with tab/shift-tab, including selection.
	// The next/previous in history shortcut in this case makes a lot more sense.

	ED_SHORTCUT("script_text_editor/indent", TTRC("Indent"), Key::NONE);
	ED_SHORTCUT("script_text_editor/unindent", TTRC("Unindent"), KeyModifierMask::SHIFT | Key::TAB);
	ED_SHORTCUT_ARRAY("script_text_editor/toggle_comment", TTRC("Toggle Comment"), { int32_t(KeyModifierMask::CMD_OR_CTRL | Key::K), int32_t(KeyModifierMask::CMD_OR_CTRL | Key::SLASH), int32_t(KeyModifierMask::CMD_OR_CTRL | Key::KP_DIVIDE), int32_t(KeyModifierMask::CMD_OR_CTRL | Key::NUMBERSIGN) });
	ED_SHORTCUT("script_text_editor/toggle_fold_line", TTRC("Fold/Unfold Line"), KeyModifierMask::ALT | Key::F);
	ED_SHORTCUT_OVERRIDE("script_text_editor/toggle_fold_line", "macos", KeyModifierMask::CTRL | KeyModifierMask::META | Key::F);
	ED_SHORTCUT("script_text_editor/fold_all_lines", TTRC("Fold All Lines"), Key::NONE);
	ED_SHORTCUT("script_text_editor/create_code_region", TTRC("Create Code Region"), KeyModifierMask::ALT | Key::R);
	ED_SHORTCUT("script_text_editor/unfold_all_lines", TTRC("Unfold All Lines"), Key::NONE);
	ED_SHORTCUT("script_text_editor/duplicate_selection", TTRC("Duplicate Selection"), KeyModifierMask::SHIFT | KeyModifierMask::CTRL | Key::D);
	ED_SHORTCUT_OVERRIDE("script_text_editor/duplicate_selection", "macos", KeyModifierMask::SHIFT | KeyModifierMask::META | Key::C);
	ED_SHORTCUT("script_text_editor/duplicate_lines", TTRC("Duplicate Lines"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::DOWN);
	ED_SHORTCUT_OVERRIDE("script_text_editor/duplicate_lines", "macos", KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::DOWN);
	ED_SHORTCUT("script_text_editor/evaluate_selection", TTRC("Evaluate Selection"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::E);
	ED_SHORTCUT("script_text_editor/toggle_word_wrap", TTRC("Toggle Word Wrap"), KeyModifierMask::ALT | Key::Z);
	ED_SHORTCUT("script_text_editor/trim_trailing_whitespace", TTRC("Trim Trailing Whitespace"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::T);
	ED_SHORTCUT("script_text_editor/trim_final_newlines", TTRC("Trim Final Newlines"), Key::NONE);
	ED_SHORTCUT("script_text_editor/convert_indent_to_spaces", TTRC("Convert Indent to Spaces"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::Y);
	ED_SHORTCUT("script_text_editor/convert_indent_to_tabs", TTRC("Convert Indent to Tabs"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::I);
	ED_SHORTCUT("script_text_editor/auto_indent", TTRC("Auto Indent"), KeyModifierMask::CMD_OR_CTRL | Key::I);

	ED_SHORTCUT_AND_COMMAND("script_text_editor/find", TTRC("Find..."), KeyModifierMask::CMD_OR_CTRL | Key::F);

	ED_SHORTCUT("script_text_editor/find_next", TTRC("Find Next"), Key::F3);
	ED_SHORTCUT_OVERRIDE("script_text_editor/find_next", "macos", KeyModifierMask::META | Key::G);

	ED_SHORTCUT("script_text_editor/find_previous", TTRC("Find Previous"), KeyModifierMask::SHIFT | Key::F3);
	ED_SHORTCUT_OVERRIDE("script_text_editor/find_previous", "macos", KeyModifierMask::META | KeyModifierMask::SHIFT | Key::G);

	ED_SHORTCUT_AND_COMMAND("script_text_editor/replace", TTRC("Replace..."), KeyModifierMask::CTRL | Key::R);
	ED_SHORTCUT_OVERRIDE("script_text_editor/replace", "macos", KeyModifierMask::ALT | KeyModifierMask::META | Key::F);

	ED_SHORTCUT("script_text_editor/find_in_files", TTRC("Find in Files..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::F);
	ED_SHORTCUT("script_text_editor/replace_in_files", TTRC("Replace in Files..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::R);

	ED_SHORTCUT("script_text_editor/contextual_help", TTRC("Contextual Help"), KeyModifierMask::ALT | Key::F1);
	ED_SHORTCUT_OVERRIDE("script_text_editor/contextual_help", "macos", KeyModifierMask::ALT | KeyModifierMask::SHIFT | Key::SPACE);

	ED_SHORTCUT("script_text_editor/toggle_bookmark", TTRC("Toggle Bookmark"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::B);

	ED_SHORTCUT("script_text_editor/goto_next_bookmark", TTRC("Go to Next Bookmark"), KeyModifierMask::CMD_OR_CTRL | Key::B);
	ED_SHORTCUT_OVERRIDE("script_text_editor/goto_next_bookmark", "macos", KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | KeyModifierMask::ALT | Key::B);

	ED_SHORTCUT("script_text_editor/goto_previous_bookmark", TTRC("Go to Previous Bookmark"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::B);
	ED_SHORTCUT("script_text_editor/remove_all_bookmarks", TTRC("Remove All Bookmarks"), Key::NONE);

	ED_SHORTCUT("script_text_editor/goto_function", TTRC("Go to Function..."), KeyModifierMask::ALT | KeyModifierMask::CTRL | Key::F);
	ED_SHORTCUT_OVERRIDE("script_text_editor/goto_function", "macos", KeyModifierMask::CTRL | KeyModifierMask::META | Key::J);

	ED_SHORTCUT("script_text_editor/goto_line", TTRC("Go to Line..."), KeyModifierMask::CMD_OR_CTRL | Key::L);

	ED_SHORTCUT("script_text_editor/toggle_breakpoint", TTRC("Toggle Breakpoint"), Key::F9);
	ED_SHORTCUT_OVERRIDE("script_text_editor/toggle_breakpoint", "macos", KeyModifierMask::META | KeyModifierMask::SHIFT | Key::B);

	ED_SHORTCUT("script_text_editor/remove_all_breakpoints", TTRC("Remove All Breakpoints"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::F9);
	// Using Control for these shortcuts even on macOS because Command+Comma is taken for opening Editor Settings.
	ED_SHORTCUT("script_text_editor/goto_next_breakpoint", TTRC("Go to Next Breakpoint"), KeyModifierMask::CTRL | Key::PERIOD);
	ED_SHORTCUT("script_text_editor/goto_previous_breakpoint", TTRC("Go to Previous Breakpoint"), KeyModifierMask::CTRL | Key::COMMA);

	ScriptEditor::register_create_script_editor_function(create_editor);
}

void ScriptTextEditor::validate() {
	code_editor->validate_script();
}
