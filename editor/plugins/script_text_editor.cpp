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

#include "core/math/expression.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/script_editor_debugger.h"

void ConnectionInfoDialog::ok_pressed() {
}

void ConnectionInfoDialog::popup_connections(String p_method, Vector<Node *> p_nodes) {
	method->set_text(p_method);

	tree->clear();
	TreeItem *root = tree->create_item();

	for (int i = 0; i < p_nodes.size(); i++) {
		List<Connection> all_connections;
		p_nodes[i]->get_signals_connected_to_this(&all_connections);

		for (List<Connection>::Element *E = all_connections.front(); E; E = E->next()) {
			Connection connection = E->get();

			if (connection.method != p_method) {
				continue;
			}

			TreeItem *node_item = tree->create_item(root);

			node_item->set_text(0, Object::cast_to<Node>(connection.source)->get_name());
			node_item->set_icon(0, EditorNode::get_singleton()->get_object_icon(connection.source, "Node"));
			node_item->set_selectable(0, false);
			node_item->set_editable(0, false);

			node_item->set_text(1, connection.signal);
			node_item->set_icon(1, get_parent_control()->get_icon("Slot", "EditorIcons"));
			node_item->set_selectable(1, false);
			node_item->set_editable(1, false);

			node_item->set_text(2, Object::cast_to<Node>(connection.target)->get_name());
			node_item->set_icon(2, EditorNode::get_singleton()->get_object_icon(connection.target, "Node"));
			node_item->set_selectable(2, false);
			node_item->set_editable(2, false);
		}
	}

	popup_centered(Size2(600, 300) * EDSCALE);
}

ConnectionInfoDialog::ConnectionInfoDialog() {
	set_title(TTR("Connections to method:"));

	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->set_anchor_and_margin(MARGIN_LEFT, ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_margin(MARGIN_TOP, ANCHOR_BEGIN, 8 * EDSCALE);
	vbc->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, -8 * EDSCALE);
	vbc->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, -8 * EDSCALE);
	add_child(vbc);

	method = memnew(Label);
	method->set_align(Label::ALIGN_CENTER);
	vbc->add_child(method);

	tree = memnew(Tree);
	tree->set_columns(3);
	tree->set_hide_root(true);
	tree->set_column_titles_visible(true);
	tree->set_column_title(0, TTR("Source"));
	tree->set_column_title(1, TTR("Signal"));
	tree->set_column_title(2, TTR("Target"));
	vbc->add_child(tree);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_allow_rmb_select(true);
}

////////////////////////////////////////////////////////////////////////////////

Vector<String> ScriptTextEditor::get_functions() {
	String errortxt;
	int line = -1, col;
	TextEdit *te = code_editor->get_text_edit();
	String text = te->get_text();
	List<String> fnc;

	if (script->get_language()->validate(text, line, col, errortxt, script->get_path(), &fnc)) {
		//if valid rewrite functions to latest
		functions.clear();
		for (List<String>::Element *E = fnc.front(); E; E = E->next()) {
			functions.push_back(E->get());
		}
	}

	return functions;
}

void ScriptTextEditor::apply_code() {
	if (script.is_null()) {
		return;
	}
	script->set_source_code(code_editor->get_text_edit()->get_text());
	script->update_exports();
	_update_member_keywords();
}

RES ScriptTextEditor::get_edited_resource() const {
	return script;
}

void ScriptTextEditor::set_edited_resource(const RES &p_res) {
	ERR_FAIL_COND(script.is_valid());
	ERR_FAIL_COND(p_res.is_null());

	script = p_res;

	code_editor->get_text_edit()->set_text(script->get_source_code());
	code_editor->get_text_edit()->clear_undo_history();
	code_editor->get_text_edit()->tag_saved_version();

	emit_signal("name_changed");
	code_editor->update_line_and_column();
}

void ScriptTextEditor::enable_editor() {
	if (editor_enabled) {
		return;
	}

	editor_enabled = true;

	_enable_code_editor();
	_set_theme_for_script();

	_validate_script();
}

void ScriptTextEditor::_update_member_keywords() {
	member_keywords.clear();
	code_editor->get_text_edit()->clear_member_keywords();
	Color member_variable_color = EDITOR_GET("text_editor/highlighting/member_variable_color");

	StringName instance_base = script->get_instance_base_type();

	if (instance_base == StringName()) {
		return;
	}
	List<PropertyInfo> plist;
	ClassDB::get_property_list(instance_base, &plist);

	for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
		String name = E->get().name;
		if (E->get().usage & PROPERTY_USAGE_CATEGORY || E->get().usage & PROPERTY_USAGE_GROUP) {
			continue;
		}
		if (name.find("/") != -1) {
			continue;
		}

		code_editor->get_text_edit()->add_member_keyword(name, member_variable_color);
	}

	List<String> clist;
	ClassDB::get_integer_constant_list(instance_base, &clist);

	for (List<String>::Element *E = clist.front(); E; E = E->next()) {
		code_editor->get_text_edit()->add_member_keyword(E->get(), member_variable_color);
	}
}

void ScriptTextEditor::_load_theme_settings() {
	TextEdit *text_edit = code_editor->get_text_edit();

	text_edit->clear_colors();

	Color background_color = EDITOR_GET("text_editor/highlighting/background_color");
	Color completion_background_color = EDITOR_GET("text_editor/highlighting/completion_background_color");
	Color completion_selected_color = EDITOR_GET("text_editor/highlighting/completion_selected_color");
	Color completion_existing_color = EDITOR_GET("text_editor/highlighting/completion_existing_color");
	Color completion_scroll_color = EDITOR_GET("text_editor/highlighting/completion_scroll_color");
	Color completion_font_color = EDITOR_GET("text_editor/highlighting/completion_font_color");
	Color text_color = EDITOR_GET("text_editor/highlighting/text_color");
	Color line_number_color = EDITOR_GET("text_editor/highlighting/line_number_color");
	Color safe_line_number_color = EDITOR_GET("text_editor/highlighting/safe_line_number_color");
	Color caret_color = EDITOR_GET("text_editor/highlighting/caret_color");
	Color caret_background_color = EDITOR_GET("text_editor/highlighting/caret_background_color");
	Color text_selected_color = EDITOR_GET("text_editor/highlighting/text_selected_color");
	Color selection_color = EDITOR_GET("text_editor/highlighting/selection_color");
	Color brace_mismatch_color = EDITOR_GET("text_editor/highlighting/brace_mismatch_color");
	Color current_line_color = EDITOR_GET("text_editor/highlighting/current_line_color");
	Color line_length_guideline_color = EDITOR_GET("text_editor/highlighting/line_length_guideline_color");
	Color word_highlighted_color = EDITOR_GET("text_editor/highlighting/word_highlighted_color");
	Color number_color = EDITOR_GET("text_editor/highlighting/number_color");
	Color function_color = EDITOR_GET("text_editor/highlighting/function_color");
	Color member_variable_color = EDITOR_GET("text_editor/highlighting/member_variable_color");
	Color mark_color = EDITOR_GET("text_editor/highlighting/mark_color");
	Color bookmark_color = EDITOR_GET("text_editor/highlighting/bookmark_color");
	Color breakpoint_color = EDITOR_GET("text_editor/highlighting/breakpoint_color");
	Color executing_line_color = EDITOR_GET("text_editor/highlighting/executing_line_color");
	Color code_folding_color = EDITOR_GET("text_editor/highlighting/code_folding_color");
	Color search_result_color = EDITOR_GET("text_editor/highlighting/search_result_color");
	Color search_result_border_color = EDITOR_GET("text_editor/highlighting/search_result_border_color");
	Color symbol_color = EDITOR_GET("text_editor/highlighting/symbol_color");
	Color keyword_color = EDITOR_GET("text_editor/highlighting/keyword_color");
	Color control_flow_keyword_color = EDITOR_GET("text_editor/highlighting/control_flow_keyword_color");
	Color basetype_color = EDITOR_GET("text_editor/highlighting/base_type_color");
	Color type_color = EDITOR_GET("text_editor/highlighting/engine_type_color");
	Color usertype_color = EDITOR_GET("text_editor/highlighting/user_type_color");
	Color comment_color = EDITOR_GET("text_editor/highlighting/comment_color");
	Color string_color = EDITOR_GET("text_editor/highlighting/string_color");

	text_edit->add_color_override("background_color", background_color);
	text_edit->add_color_override("completion_background_color", completion_background_color);
	text_edit->add_color_override("completion_selected_color", completion_selected_color);
	text_edit->add_color_override("completion_existing_color", completion_existing_color);
	text_edit->add_color_override("completion_scroll_color", completion_scroll_color);
	text_edit->add_color_override("completion_font_color", completion_font_color);
	text_edit->add_color_override("font_color", text_color);
	text_edit->add_color_override("line_number_color", line_number_color);
	text_edit->add_color_override("safe_line_number_color", safe_line_number_color);
	text_edit->add_color_override("caret_color", caret_color);
	text_edit->add_color_override("caret_background_color", caret_background_color);
	text_edit->add_color_override("font_color_selected", text_selected_color);
	text_edit->add_color_override("selection_color", selection_color);
	text_edit->add_color_override("brace_mismatch_color", brace_mismatch_color);
	text_edit->add_color_override("current_line_color", current_line_color);
	text_edit->add_color_override("line_length_guideline_color", line_length_guideline_color);
	text_edit->add_color_override("word_highlighted_color", word_highlighted_color);
	text_edit->add_color_override("number_color", number_color);
	text_edit->add_color_override("function_color", function_color);
	text_edit->add_color_override("member_variable_color", member_variable_color);
	text_edit->add_color_override("bookmark_color", bookmark_color);
	text_edit->add_color_override("breakpoint_color", breakpoint_color);
	text_edit->add_color_override("executing_line_color", executing_line_color);
	text_edit->add_color_override("mark_color", mark_color);
	text_edit->add_color_override("code_folding_color", code_folding_color);
	text_edit->add_color_override("search_result_color", search_result_color);
	text_edit->add_color_override("search_result_border_color", search_result_border_color);
	text_edit->add_color_override("symbol_color", symbol_color);

	text_edit->add_constant_override("line_spacing", EDITOR_GET("text_editor/theme/line_spacing"));

	colors_cache.symbol_color = symbol_color;
	colors_cache.keyword_color = keyword_color;
	colors_cache.control_flow_keyword_color = control_flow_keyword_color;
	colors_cache.basetype_color = basetype_color;
	colors_cache.type_color = type_color;
	colors_cache.usertype_color = usertype_color;
	colors_cache.comment_color = comment_color;
	colors_cache.string_color = string_color;

	theme_loaded = true;
	if (!script.is_null()) {
		_set_theme_for_script();
	}
}

void ScriptTextEditor::_set_theme_for_script() {
	if (!theme_loaded) {
		return;
	}

	TextEdit *text_edit = code_editor->get_text_edit();

	List<String> keywords;
	script->get_language()->get_reserved_words(&keywords);

	for (List<String>::Element *E = keywords.front(); E; E = E->next()) {
		if (script->get_language()->is_control_flow_keyword(E->get())) {
			// Use a different color for control flow keywords to make them easier to distinguish.
			text_edit->add_keyword_color(E->get(), colors_cache.control_flow_keyword_color);
		} else {
			text_edit->add_keyword_color(E->get(), colors_cache.keyword_color);
		}
	}

	//colorize core types
	const Color basetype_color = colors_cache.basetype_color;
	text_edit->add_keyword_color("String", basetype_color);
	text_edit->add_keyword_color("Vector2", basetype_color);
	text_edit->add_keyword_color("Rect2", basetype_color);
	text_edit->add_keyword_color("Transform2D", basetype_color);
	text_edit->add_keyword_color("Vector3", basetype_color);
	text_edit->add_keyword_color("AABB", basetype_color);
	text_edit->add_keyword_color("Basis", basetype_color);
	text_edit->add_keyword_color("Plane", basetype_color);
	text_edit->add_keyword_color("Transform", basetype_color);
	text_edit->add_keyword_color("Quat", basetype_color);
	text_edit->add_keyword_color("Color", basetype_color);
	text_edit->add_keyword_color("Object", basetype_color);
	text_edit->add_keyword_color("NodePath", basetype_color);
	text_edit->add_keyword_color("RID", basetype_color);
	text_edit->add_keyword_color("Dictionary", basetype_color);
	text_edit->add_keyword_color("Array", basetype_color);
	text_edit->add_keyword_color("PoolByteArray", basetype_color);
	text_edit->add_keyword_color("PoolIntArray", basetype_color);
	text_edit->add_keyword_color("PoolRealArray", basetype_color);
	text_edit->add_keyword_color("PoolStringArray", basetype_color);
	text_edit->add_keyword_color("PoolVector2Array", basetype_color);
	text_edit->add_keyword_color("PoolVector3Array", basetype_color);
	text_edit->add_keyword_color("PoolColorArray", basetype_color);

	//colorize engine types
	List<StringName> types;
	ClassDB::get_class_list(&types);

	for (List<StringName>::Element *E = types.front(); E; E = E->next()) {
		String n = E->get();
		if (n.begins_with("_")) {
			n = n.substr(1, n.length());
		}

		text_edit->add_keyword_color(n, colors_cache.type_color);
	}
	_update_member_keywords();

	//colorize user types
	List<StringName> global_classes;
	ScriptServer::get_global_class_list(&global_classes);

	for (List<StringName>::Element *E = global_classes.front(); E; E = E->next()) {
		text_edit->add_keyword_color(E->get(), colors_cache.usertype_color);
	}

	//colorize singleton autoloads (as types, just as engine singletons are)
	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);
	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		String s = E->get().name;
		if (!s.begins_with("autoload/")) {
			continue;
		}
		String path = ProjectSettings::get_singleton()->get(s);
		if (path.begins_with("*")) {
			text_edit->add_keyword_color(s.get_slice("/", 1), colors_cache.usertype_color);
		}
	}

	//colorize comments
	List<String> comments;
	script->get_language()->get_comment_delimiters(&comments);

	for (List<String>::Element *E = comments.front(); E; E = E->next()) {
		String comment = E->get();
		String beg = comment.get_slice(" ", 0);
		String end = comment.get_slice_count(" ") > 1 ? comment.get_slice(" ", 1) : String();

		text_edit->add_color_region(beg, end, colors_cache.comment_color, end == "");
	}

	//colorize strings
	List<String> strings;
	script->get_language()->get_string_delimiters(&strings);
	for (List<String>::Element *E = strings.front(); E; E = E->next()) {
		String string = E->get();
		String beg = string.get_slice(" ", 0);
		String end = string.get_slice_count(" ") > 1 ? string.get_slice(" ", 1) : String();
		text_edit->add_color_region(beg, end, colors_cache.string_color, end == "");
	}
}

void ScriptTextEditor::_show_warnings_panel(bool p_show) {
	warnings_panel->set_visible(p_show);
}

void ScriptTextEditor::_warning_clicked(Variant p_line) {
	if (p_line.get_type() == Variant::INT) {
		goto_line_centered(p_line.operator int64_t());
	} else if (p_line.get_type() == Variant::DICTIONARY) {
		Dictionary meta = p_line.operator Dictionary();
		code_editor->get_text_edit()->insert_at("# warning-ignore:" + meta["code"].operator String(), meta["line"].operator int64_t() - 1);
		_validate_script();
	}
}

void ScriptTextEditor::reload_text() {
	ERR_FAIL_COND(script.is_null());

	TextEdit *te = code_editor->get_text_edit();
	int column = te->cursor_get_column();
	int row = te->cursor_get_line();
	int h = te->get_h_scroll();
	int v = te->get_v_scroll();

	te->set_text(script->get_source_code());
	te->cursor_set_line(row);
	te->cursor_set_column(column);
	te->set_h_scroll(h);
	te->set_v_scroll(v);

	te->tag_saved_version();

	code_editor->update_line_and_column();
}

void ScriptTextEditor::add_callback(const String &p_function, PoolStringArray p_args) {
	String code = code_editor->get_text_edit()->get_text();
	int pos = script->get_language()->find_function(p_function, code);
	if (pos == -1) {
		//does not exist
		code_editor->get_text_edit()->deselect();
		pos = code_editor->get_text_edit()->get_line_count() + 2;
		String func = script->get_language()->make_function("", p_function, p_args);
		//code=code+func;
		code_editor->get_text_edit()->cursor_set_line(pos + 1);
		code_editor->get_text_edit()->cursor_set_column(1000000); //none shall be that big
		code_editor->get_text_edit()->insert_text_at_cursor("\n\n" + func);
	}
	code_editor->get_text_edit()->cursor_set_line(pos);
	code_editor->get_text_edit()->cursor_set_column(1);
}

bool ScriptTextEditor::show_members_overview() {
	return true;
}

void ScriptTextEditor::update_settings() {
	code_editor->update_editor_settings();
}

bool ScriptTextEditor::is_unsaved() {
	return code_editor->get_text_edit()->get_version() != code_editor->get_text_edit()->get_saved_version();
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

void ScriptTextEditor::_convert_case(CodeTextEditor::CaseStyle p_case) {
	code_editor->convert_case(p_case);
}

void ScriptTextEditor::trim_trailing_whitespace() {
	code_editor->trim_trailing_whitespace();
}

void ScriptTextEditor::insert_final_newline() {
	code_editor->insert_final_newline();
}

void ScriptTextEditor::convert_indent_to_spaces() {
	code_editor->convert_indent_to_spaces();
}

void ScriptTextEditor::convert_indent_to_tabs() {
	code_editor->convert_indent_to_tabs();
}

void ScriptTextEditor::tag_saved_version() {
	code_editor->get_text_edit()->tag_saved_version();
}

void ScriptTextEditor::goto_line(int p_line, bool p_with_error) {
	code_editor->goto_line(p_line);
}

void ScriptTextEditor::goto_line_selection(int p_line, int p_begin, int p_end) {
	code_editor->goto_line_selection(p_line, p_begin, p_end);
}

void ScriptTextEditor::goto_line_centered(int p_line) {
	code_editor->goto_line_centered(p_line);
}

void ScriptTextEditor::set_executing_line(int p_line) {
	code_editor->set_executing_line(p_line);
}

void ScriptTextEditor::clear_executing_line() {
	code_editor->clear_executing_line();
}

void ScriptTextEditor::ensure_focus() {
	code_editor->get_text_edit()->grab_focus();
}

String ScriptTextEditor::get_name() {
	String name;

	name = script->get_path().get_file();
	if (name.empty()) {
		// This appears for newly created built-in scripts before saving the scene.
		name = TTR("[unsaved]");
	} else if (script->get_path().find("local://") == -1 || script->get_path().find("::") == -1) {
		const String &script_name = script->get_name();
		if (script_name != "") {
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

Ref<Texture> ScriptTextEditor::get_icon() {
	if (get_parent_control() && get_parent_control()->has_icon(script->get_class(), "EditorIcons")) {
		return get_parent_control()->get_icon(script->get_class(), "EditorIcons");
	}

	return Ref<Texture>();
}

void ScriptTextEditor::_validate_script() {
	String errortxt;
	int line = -1, col;
	TextEdit *te = code_editor->get_text_edit();

	String text = te->get_text();
	List<String> fnc;
	Set<int> safe_lines;
	List<ScriptLanguage::Warning> warnings;

	if (!script->get_language()->validate(text, line, col, errortxt, script->get_path(), &fnc, &warnings, &safe_lines)) {
		String error_text = "error(" + itos(line) + "," + itos(col) + "): " + errortxt;
		code_editor->set_error(error_text);
		code_editor->set_error_pos(line - 1, col - 1);
		script_is_valid = false;
	} else {
		code_editor->set_error("");
		line = -1;
		if (!script->is_tool()) {
			script->set_source_code(text);
			script->update_exports();
			_update_member_keywords();
		}

		functions.clear();
		for (List<String>::Element *E = fnc.front(); E; E = E->next()) {
			functions.push_back(E->get());
		}
		script_is_valid = true;
	}
	_update_connected_methods();

	int warning_nb = warnings.size();
	warnings_panel->clear();

	// Add missing connections.
	if (GLOBAL_GET("debug/gdscript/warnings/enable").booleanize()) {
		Node *base = get_tree()->get_edited_scene_root();
		if (base && missing_connections.size() > 0) {
			warnings_panel->push_table(1);
			for (List<Connection>::Element *E = missing_connections.front(); E; E = E->next()) {
				Connection connection = E->get();

				String base_path = base->get_name();
				String source_path = base == connection.source ? base_path : base_path + "/" + base->get_path_to(Object::cast_to<Node>(connection.source));
				String target_path = base == connection.target ? base_path : base_path + "/" + base->get_path_to(Object::cast_to<Node>(connection.target));

				warnings_panel->push_cell();
				warnings_panel->push_color(warnings_panel->get_color("warning_color", "Editor"));
				warnings_panel->add_text(vformat(TTR("Missing connected method '%s' for signal '%s' from node '%s' to node '%s'."), connection.method, connection.signal, source_path, target_path));
				warnings_panel->pop(); // Color.
				warnings_panel->pop(); // Cell.
			}
			warnings_panel->pop(); // Table.

			warning_nb += missing_connections.size();
		}
	}

	code_editor->set_warning_nb(warning_nb);

	// Add script warnings.
	warnings_panel->push_table(3);
	for (List<ScriptLanguage::Warning>::Element *E = warnings.front(); E; E = E->next()) {
		ScriptLanguage::Warning w = E->get();

		Dictionary ignore_meta;
		ignore_meta["line"] = w.line;
		ignore_meta["code"] = w.string_code.to_lower();
		warnings_panel->push_cell();
		warnings_panel->push_meta(ignore_meta);
		warnings_panel->push_color(
				warnings_panel->get_color("accent_color", "Editor").linear_interpolate(warnings_panel->get_color("mono_color", "Editor"), 0.5));
		warnings_panel->add_text(TTR("[Ignore]"));
		warnings_panel->pop(); // Color.
		warnings_panel->pop(); // Meta ignore.
		warnings_panel->pop(); // Cell.

		warnings_panel->push_cell();
		warnings_panel->push_meta(w.line - 1);
		warnings_panel->push_color(warnings_panel->get_color("warning_color", "Editor"));
		warnings_panel->add_text(TTR("Line") + " " + itos(w.line));
		warnings_panel->add_text(" (" + w.string_code + "):");
		warnings_panel->pop(); // Color.
		warnings_panel->pop(); // Meta goto.
		warnings_panel->pop(); // Cell.

		warnings_panel->push_cell();
		warnings_panel->add_text(w.message);
		warnings_panel->pop(); // Cell.
	}
	warnings_panel->pop(); // Table.

	line--;
	bool highlight_safe = EDITOR_DEF("text_editor/highlighting/highlight_type_safe_lines", true);
	bool last_is_safe = false;
	for (int i = 0; i < te->get_line_count(); i++) {
		te->set_line_as_marked(i, line == i);
		if (highlight_safe) {
			if (safe_lines.has(i + 1)) {
				te->set_line_as_safe(i, true);
				last_is_safe = true;
			} else if (last_is_safe && (te->is_line_comment(i) || te->get_line(i).strip_edges().empty())) {
				te->set_line_as_safe(i, true);
			} else {
				te->set_line_as_safe(i, false);
				last_is_safe = false;
			}
		} else {
			te->set_line_as_safe(i, false);
		}
	}

	emit_signal("name_changed");
	emit_signal("edited_script_changed");
}

void ScriptTextEditor::_update_bookmark_list() {
	bookmarks_menu->clear();
	bookmarks_menu->set_size(Size2(1, 1));

	bookmarks_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_bookmark"), BOOKMARK_TOGGLE);
	bookmarks_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/remove_all_bookmarks"), BOOKMARK_REMOVE_ALL);
	bookmarks_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_next_bookmark"), BOOKMARK_GOTO_NEXT);
	bookmarks_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_previous_bookmark"), BOOKMARK_GOTO_PREV);

	Array bookmark_list = code_editor->get_text_edit()->get_bookmarks_array();
	if (bookmark_list.size() == 0) {
		return;
	}

	bookmarks_menu->add_separator();

	for (int i = 0; i < bookmark_list.size(); i++) {
		// Strip edges to remove spaces or tabs.
		// Also replace any tabs by spaces, since we can't print tabs in the menu.
		String line = code_editor->get_text_edit()->get_line(bookmark_list[i]).replace("\t", "  ").strip_edges();

		// Limit the size of the line if too big.
		if (line.length() > 50) {
			line = line.substr(0, 50);
		}

		bookmarks_menu->add_item(String::num((int)bookmark_list[i] + 1) + " - `" + line + "`");
		bookmarks_menu->set_item_metadata(bookmarks_menu->get_item_count() - 1, bookmark_list[i]);
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

static void _find_changed_scripts_for_external_editor(Node *p_base, Node *p_current, Set<Ref<Script>> &r_scripts) {
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
	if (!bool(EditorSettings::get_singleton()->get("text_editor/external/use_external_editor"))) {
		return;
	}

	ERR_FAIL_COND(!get_tree());

	Set<Ref<Script>> scripts;

	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		_find_changed_scripts_for_external_editor(base, base, scripts);
	}

	for (Set<Ref<Script>>::Element *E = scripts.front(); E; E = E->next()) {
		Ref<Script> script = E->get();

		if (p_for_script.is_valid() && p_for_script != script) {
			continue;
		}

		if (script->get_path() == "" || script->get_path().find("local://") != -1 || script->get_path().find("::") != -1) {
			continue; //internal script, who cares, though weird
		}

		uint64_t last_date = script->get_last_modified_time();
		uint64_t date = FileAccess::get_modified_time(script->get_path());

		if (last_date != date) {
			Ref<Script> rel_script = ResourceLoader::load(script->get_path(), script->get_class(), true);
			ERR_CONTINUE(!rel_script.is_valid());
			script->set_source_code(rel_script->get_source_code());
			script->set_last_modified_time(rel_script->get_last_modified_time());
			script->update_exports();

			_trigger_live_script_reload();
		}
	}
}

void ScriptTextEditor::_code_complete_scripts(void *p_ud, const String &p_code, List<ScriptCodeCompletionOption> *r_options, bool &r_force) {
	ScriptTextEditor *ste = (ScriptTextEditor *)p_ud;
	ste->_code_complete_script(p_code, r_options, r_force);
}

void ScriptTextEditor::_code_complete_script(const String &p_code, List<ScriptCodeCompletionOption> *r_options, bool &r_force) {
	if (color_panel->is_visible_in_tree()) {
		return;
	}
	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		base = _find_node_for_script(base, base, script);
	}
	String hint;
	Error err = script->get_language()->complete_code(p_code, script->get_path(), base, r_options, r_force, hint);
	if (err == OK) {
		code_editor->get_text_edit()->set_code_hint(hint);
	}
}

void ScriptTextEditor::_update_breakpoint_list() {
	breakpoints_menu->clear();
	breakpoints_menu->set_size(Size2(1, 1));

	breakpoints_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_breakpoint"), DEBUG_TOGGLE_BREAKPOINT);
	breakpoints_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/remove_all_breakpoints"), DEBUG_REMOVE_ALL_BREAKPOINTS);
	breakpoints_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_next_breakpoint"), DEBUG_GOTO_NEXT_BREAKPOINT);
	breakpoints_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_previous_breakpoint"), DEBUG_GOTO_PREV_BREAKPOINT);

	Array breakpoint_list = code_editor->get_text_edit()->get_breakpoints_array();
	if (breakpoint_list.size() == 0) {
		return;
	}

	breakpoints_menu->add_separator();

	for (int i = 0; i < breakpoint_list.size(); i++) {
		// Strip edges to remove spaces or tabs.
		// Also replace any tabs by spaces, since we can't print tabs in the menu.
		String line = code_editor->get_text_edit()->get_line(breakpoint_list[i]).replace("\t", "  ").strip_edges();

		// Limit the size of the line if too big.
		if (line.length() > 50) {
			line = line.substr(0, 50);
		}

		breakpoints_menu->add_item(String::num((int)breakpoint_list[i] + 1) + " - `" + line + "`");
		breakpoints_menu->set_item_metadata(breakpoints_menu->get_item_count() - 1, breakpoint_list[i]);
	}
}

void ScriptTextEditor::_breakpoint_item_pressed(int p_idx) {
	if (p_idx < 4) { // Any item before the separator.
		_edit_option(breakpoints_menu->get_item_id(p_idx));
	} else {
		code_editor->goto_line(breakpoints_menu->get_item_metadata(p_idx));
		code_editor->get_text_edit()->call_deferred("center_viewport_to_cursor"); //Need to be deferred, because goto uses call_deferred().
	}
}

void ScriptTextEditor::_breakpoint_toggled(int p_row) {
	ScriptEditor::get_singleton()->get_debugger()->set_breakpoint(script->get_path(), p_row + 1, code_editor->get_text_edit()->is_line_set_as_breakpoint(p_row));
}

void ScriptTextEditor::lookup_symbol_under_cursor() {
	TextEdit *tx = code_editor->get_text_edit();

	String text = tx->get_word_under_cursor();
	if (text == "") {
		text = tx->get_selection_text();
	}
	if (text != "") {
		_lookup_symbol(text, tx->cursor_get_line(), tx->cursor_get_column());
	}
}

void ScriptTextEditor::_lookup_symbol(const String &p_symbol, int p_row, int p_column) {
	Node *base = get_tree()->get_edited_scene_root();
	if (base) {
		base = _find_node_for_script(base, base, script);
	}

	ScriptLanguage::LookupResult result;
	if (ScriptServer::is_global_class(p_symbol)) {
		EditorNode::get_singleton()->load_resource(ScriptServer::get_global_class_path(p_symbol));
	} else if (p_symbol.is_resource_file()) {
		List<String> scene_extensions;
		ResourceLoader::get_recognized_extensions_for_type("PackedScene", &scene_extensions);

		if (scene_extensions.find(p_symbol.get_extension())) {
			EditorNode::get_singleton()->load_scene(p_symbol);
		} else {
			EditorNode::get_singleton()->load_resource(p_symbol);
		}

	} else if (script->get_language()->lookup_code(code_editor->get_text_edit()->get_text_for_lookup_completion(), p_symbol, script->get_path(), base, result) == OK) {
		_goto_line(p_row);

		result.class_name = result.class_name.trim_prefix("_");

		switch (result.type) {
			case ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION: {
				if (result.script.is_valid()) {
					emit_signal("request_open_script_at_line", result.script, result.location - 1);
				} else {
					emit_signal("request_save_history");
					goto_line_centered(result.location - 1);
				}
			} break;
			case ScriptLanguage::LookupResult::RESULT_CLASS: {
				emit_signal("go_to_help", "class_name:" + result.class_name);
			} break;
			case ScriptLanguage::LookupResult::RESULT_CLASS_CONSTANT: {
				StringName cname = result.class_name;
				bool success;
				while (true) {
					ClassDB::get_integer_constant(cname, result.class_member, &success);
					if (success) {
						result.class_name = cname;
						cname = ClassDB::get_parent_class(cname);
					} else {
						break;
					}
				}

				emit_signal("go_to_help", "class_constant:" + result.class_name + ":" + result.class_member);

			} break;
			case ScriptLanguage::LookupResult::RESULT_CLASS_PROPERTY: {
				emit_signal("go_to_help", "class_property:" + result.class_name + ":" + result.class_member);

			} break;
			case ScriptLanguage::LookupResult::RESULT_CLASS_METHOD: {
				StringName cname = result.class_name;

				while (true) {
					if (ClassDB::has_method(cname, result.class_member)) {
						result.class_name = cname;
						cname = ClassDB::get_parent_class(cname);
					} else {
						break;
					}
				}

				emit_signal("go_to_help", "class_method:" + result.class_name + ":" + result.class_member);

			} break;
			case ScriptLanguage::LookupResult::RESULT_CLASS_ENUM: {
				StringName cname = result.class_name;
				StringName success;
				while (true) {
					success = ClassDB::get_integer_constant_enum(cname, result.class_member, true);
					if (success != StringName()) {
						result.class_name = cname;
						cname = ClassDB::get_parent_class(cname);
					} else {
						break;
					}
				}

				emit_signal("go_to_help", "class_enum:" + result.class_name + ":" + result.class_member);

			} break;
			case ScriptLanguage::LookupResult::RESULT_CLASS_TBD_GLOBALSCOPE: {
				emit_signal("go_to_help", "class_global:" + result.class_name + ":" + result.class_member);
			} break;
		}
	} else if (ProjectSettings::get_singleton()->has_setting("autoload/" + p_symbol)) {
		//check for Autoload scenes
		String path = ProjectSettings::get_singleton()->get("autoload/" + p_symbol);
		if (path.begins_with("*")) {
			path = path.substr(1, path.length());
			EditorNode::get_singleton()->load_scene(path);
		}
	} else if (p_symbol.is_rel_path()) {
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

String ScriptTextEditor::_get_absolute_path(const String &rel_path) {
	String base_path = script->get_path().get_base_dir();
	String path = base_path.plus_file(rel_path);
	return path.replace("///", "//").simplify_path();
}

void ScriptTextEditor::update_toggle_scripts_button() {
	if (code_editor != nullptr) {
		code_editor->update_toggle_scripts_button();
	}
}

void ScriptTextEditor::_update_connected_methods() {
	TextEdit *text_edit = code_editor->get_text_edit();
	text_edit->clear_info_icons();
	missing_connections.clear();

	if (!script_is_valid) {
		return;
	}

	Node *base = get_tree()->get_edited_scene_root();
	if (!base) {
		return;
	}

	Vector<Node *> nodes = _find_all_node_for_script(base, base, script);
	Set<StringName> methods_found;
	for (int i = 0; i < nodes.size(); i++) {
		List<Connection> connections;
		nodes[i]->get_signals_connected_to_this(&connections);

		for (List<Connection>::Element *E = connections.front(); E; E = E->next()) {
			Connection connection = E->get();
			if (!(connection.flags & CONNECT_PERSIST)) {
				continue;
			}

			// As deleted nodes are still accessible via the undo/redo system, check if they're still on the tree.
			Node *source = Object::cast_to<Node>(connection.source);
			if (source && !source->is_inside_tree()) {
				continue;
			}

			if (methods_found.has(connection.method)) {
				continue;
			}

			if (!ClassDB::has_method(script->get_instance_base_type(), connection.method)) {
				int line = -1;

				for (int j = 0; j < functions.size(); j++) {
					String name = functions[j].get_slice(":", 0);
					if (name == connection.method) {
						line = functions[j].get_slice(":", 1).to_int();
						text_edit->set_line_info_icon(line - 1, get_parent_control()->get_icon("Slot", "EditorIcons"), connection.method);
						methods_found.insert(connection.method);
						break;
					}
				}

				if (line >= 0) {
					continue;
				}

				// There is a chance that the method is inherited from another script.
				bool found_inherited_function = false;
				Ref<Script> inherited_script = script->get_base_script();
				while (!inherited_script.is_null()) {
					if (inherited_script->has_method(connection.method)) {
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
}

void ScriptTextEditor::_lookup_connections(int p_row, String p_method) {
	Node *base = get_tree()->get_edited_scene_root();
	if (!base) {
		return;
	}

	Vector<Node *> nodes = _find_all_node_for_script(base, base, script);
	connection_info_dialog->popup_connections(p_method, nodes);
}

void ScriptTextEditor::_edit_option(int p_op) {
	TextEdit *tx = code_editor->get_text_edit();

	switch (p_op) {
		case EDIT_UNDO: {
			tx->undo();
			tx->call_deferred("grab_focus");
		} break;
		case EDIT_REDO: {
			tx->redo();
			tx->call_deferred("grab_focus");
		} break;
		case EDIT_CUT: {
			tx->cut();
			tx->call_deferred("grab_focus");
		} break;
		case EDIT_COPY: {
			tx->copy();
			tx->call_deferred("grab_focus");
		} break;
		case EDIT_PASTE: {
			tx->paste();
			tx->call_deferred("grab_focus");
		} break;
		case EDIT_SELECT_ALL: {
			tx->select_all();
			tx->call_deferred("grab_focus");
		} break;
		case EDIT_MOVE_LINE_UP: {
			code_editor->move_lines_up();
		} break;
		case EDIT_MOVE_LINE_DOWN: {
			code_editor->move_lines_down();
		} break;
		case EDIT_INDENT_LEFT: {
			Ref<Script> scr = script;
			if (scr.is_null()) {
				return;
			}

			tx->indent_left();
		} break;
		case EDIT_INDENT_RIGHT: {
			Ref<Script> scr = script;
			if (scr.is_null()) {
				return;
			}

			tx->indent_right();
		} break;
		case EDIT_DELETE_LINE: {
			code_editor->delete_lines();
		} break;
		case EDIT_DUPLICATE_SELECTION: {
			code_editor->duplicate_selection();
		} break;
		case EDIT_TOGGLE_FOLD_LINE: {
			tx->toggle_fold_line(tx->cursor_get_line());
			tx->update();
		} break;
		case EDIT_FOLD_ALL_LINES: {
			tx->fold_all_lines();
			tx->update();
		} break;
		case EDIT_UNFOLD_ALL_LINES: {
			tx->unhide_all_lines();
			tx->update();
		} break;
		case EDIT_TOGGLE_COMMENT: {
			_edit_option_toggle_inline_comment();
		} break;
		case EDIT_COMPLETE: {
			tx->query_code_comple();
		} break;
		case EDIT_AUTO_INDENT: {
			String text = tx->get_text();
			Ref<Script> scr = script;
			if (scr.is_null()) {
				return;
			}

			tx->begin_complex_operation();
			int begin, end;
			if (tx->is_selection_active()) {
				begin = tx->get_selection_from_line();
				end = tx->get_selection_to_line();
				// ignore if the cursor is not past the first column
				if (tx->get_selection_to_column() == 0) {
					end--;
				}
			} else {
				begin = 0;
				end = tx->get_line_count() - 1;
			}
			scr->get_language()->auto_indent_code(text, begin, end);
			Vector<String> lines = text.split("\n");
			for (int i = begin; i <= end; ++i) {
				tx->set_line(i, lines[i]);
			}

			tx->end_complex_operation();
		} break;
		case EDIT_TRIM_TRAILING_WHITESAPCE: {
			trim_trailing_whitespace();
		} break;
		case EDIT_CONVERT_INDENT_TO_SPACES: {
			convert_indent_to_spaces();
		} break;
		case EDIT_CONVERT_INDENT_TO_TABS: {
			convert_indent_to_tabs();
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
			Vector<String> lines = code_editor->get_text_edit()->get_selection_text().split("\n");
			PoolStringArray results;

			for (int i = 0; i < lines.size(); i++) {
				String line = lines[i];
				String whitespace = line.substr(0, line.size() - line.strip_edges(true, false).size()); //extract the whitespace at the beginning

				if (expression.parse(line) == OK) {
					Variant result = expression.execute(Array(), Variant(), false);
					if (expression.get_error_text() == "") {
						results.append(whitespace + result.get_construct_string());
					} else {
						results.append(line);
					}
				} else {
					results.append(line);
				}
			}

			code_editor->get_text_edit()->begin_complex_operation(); //prevents creating a two-step undo
			code_editor->get_text_edit()->insert_text_at_cursor(results.join("\n"));
			code_editor->get_text_edit()->end_complex_operation();
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
			String selected_text = code_editor->get_text_edit()->get_selection_text();

			// Yep, because it doesn't make sense to instance this dialog for every single script open...
			// So this will be delegated to the ScriptEditor.
			emit_signal("search_in_files_requested", selected_text);
		} break;
		case REPLACE_IN_FILES: {
			String selected_text = code_editor->get_text_edit()->get_selection_text();

			emit_signal("replace_in_files_requested", selected_text);
		} break;
		case SEARCH_LOCATE_FUNCTION: {
			quick_open->popup_dialog(get_functions());
			quick_open->set_title(TTR("Go to Function"));
		} break;
		case SEARCH_GOTO_LINE: {
			goto_line_dialog->popup_find_line(tx);
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
			int line = tx->cursor_get_line();
			bool dobreak = !tx->is_line_set_as_breakpoint(line);
			tx->set_line_as_breakpoint(line, dobreak);
			ScriptEditor::get_singleton()->get_debugger()->set_breakpoint(script->get_path(), line + 1, dobreak);
		} break;
		case DEBUG_REMOVE_ALL_BREAKPOINTS: {
			List<int> bpoints;
			tx->get_breakpoints(&bpoints);

			for (List<int>::Element *E = bpoints.front(); E; E = E->next()) {
				int line = E->get();
				bool dobreak = !tx->is_line_set_as_breakpoint(line);
				tx->set_line_as_breakpoint(line, dobreak);
				ScriptEditor::get_singleton()->get_debugger()->set_breakpoint(script->get_path(), line + 1, dobreak);
			}
		} break;
		case DEBUG_GOTO_NEXT_BREAKPOINT: {
			List<int> bpoints;
			tx->get_breakpoints(&bpoints);
			if (bpoints.size() <= 0) {
				return;
			}

			int line = tx->cursor_get_line();

			// wrap around
			if (line >= bpoints[bpoints.size() - 1]) {
				tx->unfold_line(bpoints[0]);
				tx->cursor_set_line(bpoints[0]);
				tx->center_viewport_to_cursor();
			} else {
				for (List<int>::Element *E = bpoints.front(); E; E = E->next()) {
					int bline = E->get();
					if (bline > line) {
						tx->unfold_line(bline);
						tx->cursor_set_line(bline);
						tx->center_viewport_to_cursor();
						return;
					}
				}
			}

		} break;
		case DEBUG_GOTO_PREV_BREAKPOINT: {
			List<int> bpoints;
			tx->get_breakpoints(&bpoints);
			if (bpoints.size() <= 0) {
				return;
			}

			int line = tx->cursor_get_line();
			// wrap around
			if (line <= bpoints[0]) {
				tx->unfold_line(bpoints[bpoints.size() - 1]);
				tx->cursor_set_line(bpoints[bpoints.size() - 1]);
				tx->center_viewport_to_cursor();
			} else {
				for (List<int>::Element *E = bpoints.back(); E; E = E->prev()) {
					int bline = E->get();
					if (bline < line) {
						tx->unfold_line(bline);
						tx->cursor_set_line(bline);
						tx->center_viewport_to_cursor();
						return;
					}
				}
			}

		} break;
		case HELP_CONTEXTUAL: {
			String text = tx->get_selection_text();
			if (text == "") {
				text = tx->get_word_under_cursor();
			}
			if (text != "") {
				emit_signal("request_help", text);
			}
		} break;
		case LOOKUP_SYMBOL: {
			lookup_symbol_under_cursor();
		} break;
	}
}

void ScriptTextEditor::_edit_option_toggle_inline_comment() {
	if (script.is_null()) {
		return;
	}

	String delimiter = "#";
	List<String> comment_delimiters;
	script->get_language()->get_comment_delimiters(&comment_delimiters);

	for (List<String>::Element *E = comment_delimiters.front(); E; E = E->next()) {
		String script_delimiter = E->get();
		if (script_delimiter.find(" ") == -1) {
			delimiter = script_delimiter;
			break;
		}
	}

	code_editor->toggle_inline_comment(delimiter);
}

void ScriptTextEditor::add_syntax_highlighter(SyntaxHighlighter *p_highlighter) {
	highlighters[p_highlighter->get_name()] = p_highlighter;
	highlighter_menu->add_radio_check_item(p_highlighter->get_name());
}

void ScriptTextEditor::set_syntax_highlighter(SyntaxHighlighter *p_highlighter) {
	TextEdit *te = code_editor->get_text_edit();
	te->_set_syntax_highlighting(p_highlighter);
	if (p_highlighter != nullptr) {
		highlighter_menu->set_item_checked(highlighter_menu->get_item_idx_from_text(p_highlighter->get_name()), true);
	} else {
		highlighter_menu->set_item_checked(highlighter_menu->get_item_idx_from_text(TTR("Standard")), true);
	}
}

void ScriptTextEditor::_change_syntax_highlighter(int p_idx) {
	Map<String, SyntaxHighlighter *>::Element *el = highlighters.front();
	while (el != nullptr) {
		highlighter_menu->set_item_checked(highlighter_menu->get_item_idx_from_text(el->key()), false);
		el = el->next();
	}
	// highlighter_menu->set_item_checked(p_idx, true);
	set_syntax_highlighter(highlighters[highlighter_menu->get_item_text(p_idx)]);
}

void ScriptTextEditor::_bind_methods() {
	ClassDB::bind_method("_validate_script", &ScriptTextEditor::_validate_script);
	ClassDB::bind_method("_update_bookmark_list", &ScriptTextEditor::_update_bookmark_list);
	ClassDB::bind_method("_bookmark_item_pressed", &ScriptTextEditor::_bookmark_item_pressed);
	ClassDB::bind_method("_load_theme_settings", &ScriptTextEditor::_load_theme_settings);
	ClassDB::bind_method("_update_breakpoint_list", &ScriptTextEditor::_update_breakpoint_list);
	ClassDB::bind_method("_breakpoint_item_pressed", &ScriptTextEditor::_breakpoint_item_pressed);
	ClassDB::bind_method("_breakpoint_toggled", &ScriptTextEditor::_breakpoint_toggled);
	ClassDB::bind_method("_lookup_connections", &ScriptTextEditor::_lookup_connections);
	ClassDB::bind_method("_update_connected_methods", &ScriptTextEditor::_update_connected_methods);
	ClassDB::bind_method("_change_syntax_highlighter", &ScriptTextEditor::_change_syntax_highlighter);
	ClassDB::bind_method("_edit_option", &ScriptTextEditor::_edit_option);
	ClassDB::bind_method("_goto_line", &ScriptTextEditor::_goto_line);
	ClassDB::bind_method("_lookup_symbol", &ScriptTextEditor::_lookup_symbol);
	ClassDB::bind_method("_text_edit_gui_input", &ScriptTextEditor::_text_edit_gui_input);
	ClassDB::bind_method("_show_warnings_panel", &ScriptTextEditor::_show_warnings_panel);
	ClassDB::bind_method("_warning_clicked", &ScriptTextEditor::_warning_clicked);
	ClassDB::bind_method("_color_changed", &ScriptTextEditor::_color_changed);
	ClassDB::bind_method("_prepare_edit_menu", &ScriptTextEditor::_prepare_edit_menu);

	ClassDB::bind_method("get_drag_data_fw", &ScriptTextEditor::get_drag_data_fw);
	ClassDB::bind_method("can_drop_data_fw", &ScriptTextEditor::can_drop_data_fw);
	ClassDB::bind_method("drop_data_fw", &ScriptTextEditor::drop_data_fw);
}

Control *ScriptTextEditor::get_edit_menu() {
	return edit_hb;
}

Control *ScriptTextEditor::get_code_editor_text_edit() {
	return code_editor->get_text_edit();
}

void ScriptTextEditor::clear_edit_menu() {
	memdelete(edit_hb);
}

void ScriptTextEditor::reload(bool p_soft) {
	TextEdit *te = code_editor->get_text_edit();
	Ref<Script> scr = script;
	if (scr.is_null()) {
		return;
	}
	scr->set_source_code(te->get_text());
	bool soft = p_soft || scr->get_instance_base_type() == "EditorPlugin"; //always soft-reload editor plugins

	scr->get_language()->reload_tool_script(scr, soft);
}

void ScriptTextEditor::get_breakpoints(List<int> *p_breakpoints) {
	code_editor->get_text_edit()->get_breakpoints(p_breakpoints);
}

void ScriptTextEditor::set_tooltip_request_func(String p_method, Object *p_obj) {
	code_editor->get_text_edit()->set_tooltip_request_func(p_obj, p_method, this);
}

void ScriptTextEditor::set_debugger_active(bool p_active) {
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

void ScriptTextEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";

	Dictionary d = p_data;

	TextEdit *te = code_editor->get_text_edit();
	int row, col;
	te->_get_mouse_pos(p_point, row, col);

	if (d.has("type") && String(d["type"]) == "resource") {
		Ref<Resource> res = d["resource"];
		if (!res.is_valid()) {
			return;
		}

		if (res->get_path().is_resource_file()) {
			EditorNode::get_singleton()->show_warning(TTR("Only resources from filesystem can be dropped."));
			return;
		}

		te->cursor_set_line(row);
		te->cursor_set_column(col);
		te->insert_text_at_cursor(res->get_path());
	}

	if (d.has("type") && (String(d["type"]) == "files" || String(d["type"]) == "files_and_dirs")) {
		Array files = d["files"];

		String text_to_drop;
		bool preload = Input::get_singleton()->is_key_pressed(KEY_CONTROL);
		for (int i = 0; i < files.size(); i++) {
			if (i > 0) {
				text_to_drop += ", ";
			}

			if (preload) {
				text_to_drop += "preload(" + String(files[i]).c_escape().quote(quote_style) + ")";
			} else {
				text_to_drop += String(files[i]).c_escape().quote(quote_style);
			}
		}

		te->cursor_set_line(row);
		te->cursor_set_column(col);
		te->insert_text_at_cursor(text_to_drop);
	}

	if (d.has("type") && String(d["type"]) == "nodes") {
		Node *sn = _find_script_node(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root(), script);

		if (!sn) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Can't drop nodes because script '%s' is not used in this scene."), get_name()));
			return;
		}

		Array nodes = d["nodes"];
		String text_to_drop;

		if (Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
			bool use_type = EDITOR_GET("text_editor/completion/add_type_hints");
			for (int i = 0; i < nodes.size(); i++) {
				NodePath np = nodes[i];
				Node *node = get_node(np);
				if (!node) {
					continue;
				}

				String path;
				if (node->is_unique_name_in_owner()) {
					path = "%" + node->get_name();
				} else {
					path = sn->get_path_to(node);
				}
				Vector<String> segments = path.split("/");
				for (int j = 0; j < segments.size(); j++) {
					if (!segments[j].is_valid_identifier()) {
						path = path.c_escape().quote(quote_style);
						break;
					}
				}

				String variable_name = String(node->get_name()).camelcase_to_underscore(true).validate_identifier();
				if (use_type) {
					text_to_drop += vformat("onready var %s: %s = $%s\n", variable_name, node->get_class_name(), path);
				} else {
					text_to_drop += vformat("onready var %s = $%s\n", variable_name, path);
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

				String path;
				if (node->is_unique_name_in_owner()) {
					path = "%" + node->get_name();
				} else {
					path = sn->get_path_to(node);
				}
				Vector<String> segments = path.split("/");
				for (int j = 0; j < segments.size(); j++) {
					if (!segments[j].is_valid_identifier()) {
						path = path.c_escape().quote(quote_style);
						break;
					}
				}
				text_to_drop += "$" + path;
			}
		}

		te->cursor_set_line(row);
		te->cursor_set_column(col);
		te->insert_text_at_cursor(text_to_drop);
	}

	if (d.has("type") && String(d["type"]) == "obj_property") {
		const String text_to_drop = String(d["property"]).c_escape().quote(quote_style);

		te->cursor_set_line(row);
		te->cursor_set_column(col);
		te->insert_text_at_cursor(text_to_drop);
	}
}

void ScriptTextEditor::_text_edit_gui_input(const Ref<InputEvent> &ev) {
	Ref<InputEventMouseButton> mb = ev;
	Ref<InputEventKey> k = ev;
	Point2 local_pos;
	bool create_menu = false;

	TextEdit *tx = code_editor->get_text_edit();
	if (mb.is_valid() && mb->get_button_index() == BUTTON_RIGHT && mb->is_pressed()) {
		local_pos = mb->get_global_position() - tx->get_global_position();
		create_menu = true;
	} else if (k.is_valid() && k->get_scancode() == KEY_MENU) {
		local_pos = tx->_get_cursor_pixel_pos();
		create_menu = true;
	}

	if (create_menu) {
		int col, row;
		tx->_get_mouse_pos(local_pos, row, col);

		tx->set_right_click_moves_caret(EditorSettings::get_singleton()->get("text_editor/cursor/right_click_moves_caret"));
		if (tx->is_right_click_moving_caret()) {
			if (tx->is_selection_active()) {
				int from_line = tx->get_selection_from_line();
				int to_line = tx->get_selection_to_line();
				int from_column = tx->get_selection_from_column();
				int to_column = tx->get_selection_to_column();

				if (row < from_line || row > to_line || (row == from_line && col < from_column) || (row == to_line && col > to_column)) {
					// Right click is outside the selected text
					tx->deselect();
				}
			}
			if (!tx->is_selection_active()) {
				tx->cursor_set_line(row, true, false);
				tx->cursor_set_column(col);
			}
		}

		String word_at_pos = tx->get_word_at_pos(local_pos);
		if (word_at_pos == "") {
			word_at_pos = tx->get_word_under_cursor();
		}
		if (word_at_pos == "") {
			word_at_pos = tx->get_selection_text();
		}

		bool has_color = (word_at_pos == "Color");
		bool foldable = tx->can_fold(row) || tx->is_folded(row);
		bool open_docs = false;
		bool goto_definition = false;

		if (word_at_pos.is_resource_file()) {
			open_docs = true;
		} else {
			Node *base = get_tree()->get_edited_scene_root();
			if (base) {
				base = _find_node_for_script(base, base, script);
			}
			ScriptLanguage::LookupResult result;
			if (script->get_language()->lookup_code(code_editor->get_text_edit()->get_text_for_lookup_completion(), word_at_pos, script->get_path(), base, result) == OK) {
				open_docs = true;
			}
		}

		if (has_color) {
			String line = tx->get_line(row);
			color_position.x = row;
			color_position.y = col;

			int begin = 0;
			int end = 0;
			bool valid = false;
			for (int i = col; i < line.length(); i++) {
				if (line[i] == '(') {
					begin = i;
					continue;
				} else if (line[i] == ')') {
					end = i + 1;
					valid = true;
					break;
				}
			}
			if (valid) {
				color_args = line.substr(begin, end - begin);
				String stripped = color_args.replace(" ", "").replace("(", "").replace(")", "");
				Vector<float> color = stripped.split_floats(",");
				if (color.size() > 2) {
					float alpha = color.size() > 3 ? color[3] : 1.0f;
					color_picker->set_pick_color(Color(color[0], color[1], color[2], alpha));
				}
				color_panel->set_position(get_global_transform().xform(local_pos));
			} else {
				has_color = false;
			}
		}
		_make_context_menu(tx->is_selection_active(), has_color, foldable, open_docs, goto_definition, local_pos);
	}
}

void ScriptTextEditor::_color_changed(const Color &p_color) {
	String new_args;
	if (p_color.a == 1.0f) {
		new_args = String("(" + rtos(p_color.r) + ", " + rtos(p_color.g) + ", " + rtos(p_color.b) + ")");
	} else {
		new_args = String("(" + rtos(p_color.r) + ", " + rtos(p_color.g) + ", " + rtos(p_color.b) + ", " + rtos(p_color.a) + ")");
	}

	String line = code_editor->get_text_edit()->get_line(color_position.x);
	int color_args_pos = line.find(color_args, color_position.y);
	String line_with_replaced_args = line;
	line_with_replaced_args.erase(color_args_pos, color_args.length());
	line_with_replaced_args = line_with_replaced_args.insert(color_args_pos, new_args);

	color_args = new_args;
	code_editor->get_text_edit()->begin_complex_operation();
	code_editor->get_text_edit()->set_line(color_position.x, line_with_replaced_args);
	code_editor->get_text_edit()->end_complex_operation();
	code_editor->get_text_edit()->update();
}

void ScriptTextEditor::_prepare_edit_menu() {
	const TextEdit *tx = code_editor->get_text_edit();
	PopupMenu *popup = edit_menu->get_popup();
	popup->set_item_disabled(popup->get_item_index(EDIT_UNDO), !tx->has_undo());
	popup->set_item_disabled(popup->get_item_index(EDIT_REDO), !tx->has_redo());
}

void ScriptTextEditor::_make_context_menu(bool p_selection, bool p_color, bool p_foldable, bool p_open_docs, bool p_goto_definition, Vector2 p_pos) {
	context_menu->clear();
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/undo"), EDIT_UNDO);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/redo"), EDIT_REDO);

	context_menu->add_separator();
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/cut"), EDIT_CUT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/copy"), EDIT_COPY);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/paste"), EDIT_PASTE);

	context_menu->add_separator();
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/select_all"), EDIT_SELECT_ALL);

	context_menu->add_separator();
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/indent_left"), EDIT_INDENT_LEFT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/indent_right"), EDIT_INDENT_RIGHT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_comment"), EDIT_TOGGLE_COMMENT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_bookmark"), BOOKMARK_TOGGLE);

	if (p_selection) {
		context_menu->add_separator();
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_uppercase"), EDIT_TO_UPPERCASE);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_to_lowercase"), EDIT_TO_LOWERCASE);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/evaluate_selection"), EDIT_EVALUATE);
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

	const TextEdit *tx = code_editor->get_text_edit();
	context_menu->set_item_disabled(context_menu->get_item_index(EDIT_UNDO), !tx->has_undo());
	context_menu->set_item_disabled(context_menu->get_item_index(EDIT_REDO), !tx->has_redo());

	context_menu->set_position(get_global_transform().xform(p_pos));
	context_menu->set_size(Vector2(1, 1));
	context_menu->popup();
}

void ScriptTextEditor::_enable_code_editor() {
	ERR_FAIL_COND(code_editor->get_parent());

	VSplitContainer *editor_box = memnew(VSplitContainer);
	add_child(editor_box);
	editor_box->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	editor_box->set_v_size_flags(SIZE_EXPAND_FILL);

	editor_box->add_child(code_editor);
	code_editor->connect("show_warnings_panel", this, "_show_warnings_panel");
	code_editor->connect("validate_script", this, "_validate_script");
	code_editor->connect("load_theme_settings", this, "_load_theme_settings");
	code_editor->get_text_edit()->connect("breakpoint_toggled", this, "_breakpoint_toggled");
	code_editor->get_text_edit()->connect("symbol_lookup", this, "_lookup_symbol");
	code_editor->get_text_edit()->connect("info_clicked", this, "_lookup_connections");
	code_editor->get_text_edit()->connect("gui_input", this, "_text_edit_gui_input");
	code_editor->show_toggle_scripts_button();

	editor_box->add_child(warnings_panel);
	warnings_panel->add_font_override(
			"normal_font", EditorNode::get_singleton()->get_gui_base()->get_font("main", "EditorFonts"));
	warnings_panel->connect("meta_clicked", this, "_warning_clicked");

	add_child(context_menu);
	context_menu->connect("id_pressed", this, "_edit_option");
	context_menu->set_hide_on_window_lose_focus(true);

	add_child(color_panel);

	color_picker = memnew(ColorPicker);
	color_picker->set_deferred_mode(true);
	color_picker->connect("color_changed", this, "_color_changed");

	color_panel->add_child(color_picker);

	// get default color picker mode from editor settings
	int default_color_mode = EDITOR_GET("interface/inspector/default_color_picker_mode");
	if (default_color_mode == 1) {
		color_picker->set_hsv_mode(true);
	} else if (default_color_mode == 2) {
		color_picker->set_raw_mode(true);
	}

	quick_open = memnew(ScriptEditorQuickOpen);
	quick_open->connect("goto_line", this, "_goto_line");
	add_child(quick_open);

	goto_line_dialog = memnew(GotoLineDialog);
	add_child(goto_line_dialog);

	add_child(connection_info_dialog);

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
	search_menu->get_popup()->connect("id_pressed", this, "_edit_option");
	edit_hb->add_child(edit_menu);
	edit_menu->connect("about_to_show", this, "_prepare_edit_menu");
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/undo"), EDIT_UNDO);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/redo"), EDIT_REDO);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/cut"), EDIT_CUT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/copy"), EDIT_COPY);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/paste"), EDIT_PASTE);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/select_all"), EDIT_SELECT_ALL);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/move_up"), EDIT_MOVE_LINE_UP);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/move_down"), EDIT_MOVE_LINE_DOWN);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/indent_left"), EDIT_INDENT_LEFT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/indent_right"), EDIT_INDENT_RIGHT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/delete_line"), EDIT_DELETE_LINE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_comment"), EDIT_TOGGLE_COMMENT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_fold_line"), EDIT_TOGGLE_FOLD_LINE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/fold_all_lines"), EDIT_FOLD_ALL_LINES);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/unfold_all_lines"), EDIT_UNFOLD_ALL_LINES);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/duplicate_selection"), EDIT_DUPLICATE_SELECTION);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/complete_symbol"), EDIT_COMPLETE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/evaluate_selection"), EDIT_EVALUATE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/trim_trailing_whitespace"), EDIT_TRIM_TRAILING_WHITESAPCE);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_indent_to_spaces"), EDIT_CONVERT_INDENT_TO_SPACES);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/convert_indent_to_tabs"), EDIT_CONVERT_INDENT_TO_TABS);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/auto_indent"), EDIT_AUTO_INDENT);
	edit_menu->get_popup()->connect("id_pressed", this, "_edit_option");
	edit_menu->get_popup()->add_separator();

	edit_menu->get_popup()->add_child(convert_case);
	edit_menu->get_popup()->add_submenu_item(TTR("Convert Case"), "convert_case");
	convert_case->add_shortcut(ED_SHORTCUT("script_text_editor/convert_to_uppercase", TTR("Uppercase"), KEY_MASK_SHIFT | KEY_F4), EDIT_TO_UPPERCASE);
	convert_case->add_shortcut(ED_SHORTCUT("script_text_editor/convert_to_lowercase", TTR("Lowercase"), KEY_MASK_SHIFT | KEY_F5), EDIT_TO_LOWERCASE);
	convert_case->add_shortcut(ED_SHORTCUT("script_text_editor/capitalize", TTR("Capitalize"), KEY_MASK_SHIFT | KEY_F6), EDIT_CAPITALIZE);
	convert_case->connect("id_pressed", this, "_edit_option");

	edit_menu->get_popup()->add_child(highlighter_menu);
	edit_menu->get_popup()->add_submenu_item(TTR("Syntax Highlighter"), "highlighter_menu");
	highlighter_menu->connect("id_pressed", this, "_change_syntax_highlighter");

	_load_theme_settings();

	edit_hb->add_child(goto_menu);
	goto_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_function"), SEARCH_LOCATE_FUNCTION);
	goto_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_line"), SEARCH_GOTO_LINE);
	goto_menu->get_popup()->add_separator();

	goto_menu->get_popup()->add_child(bookmarks_menu);
	goto_menu->get_popup()->add_submenu_item(TTR("Bookmarks"), "Bookmarks");
	_update_bookmark_list();
	bookmarks_menu->connect("about_to_show", this, "_update_bookmark_list");
	bookmarks_menu->connect("index_pressed", this, "_bookmark_item_pressed");

	goto_menu->get_popup()->add_child(breakpoints_menu);
	goto_menu->get_popup()->add_submenu_item(TTR("Breakpoints"), "Breakpoints");
	_update_breakpoint_list();
	breakpoints_menu->connect("about_to_show", this, "_update_breakpoint_list");
	breakpoints_menu->connect("index_pressed", this, "_breakpoint_item_pressed");

	goto_menu->get_popup()->connect("id_pressed", this, "_edit_option");
}

ScriptTextEditor::ScriptTextEditor() {
	theme_loaded = false;
	script_is_valid = false;
	editor_enabled = false;

	code_editor = memnew(CodeTextEditor);
	code_editor->add_constant_override("separation", 2);
	code_editor->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	code_editor->set_code_complete_func(_code_complete_scripts, this);
	code_editor->set_v_size_flags(SIZE_EXPAND_FILL);

	warnings_panel = memnew(RichTextLabel);
	warnings_panel->set_custom_minimum_size(Size2(0, 100 * EDSCALE));
	warnings_panel->set_h_size_flags(SIZE_EXPAND_FILL);
	warnings_panel->set_meta_underline(true);
	warnings_panel->set_selection_enabled(true);
	warnings_panel->set_focus_mode(FOCUS_CLICK);
	warnings_panel->hide();

	update_settings();

	code_editor->get_text_edit()->set_callhint_settings(
			EditorSettings::get_singleton()->get("text_editor/completion/put_callhint_tooltip_below_current_line"),
			EditorSettings::get_singleton()->get("text_editor/completion/callhint_tooltip_offset"));

	code_editor->get_text_edit()->set_select_identifiers_on_hover(true);
	code_editor->get_text_edit()->set_context_menu_enabled(false);

	context_menu = memnew(PopupMenu);

	color_panel = memnew(PopupPanel);
	color_picker = nullptr;

	edit_hb = memnew(HBoxContainer);

	edit_menu = memnew(MenuButton);
	edit_menu->set_text(TTR("Edit"));
	edit_menu->set_switch_on_hover(true);
	edit_menu->get_popup()->set_hide_on_window_lose_focus(true);

	convert_case = memnew(PopupMenu);
	convert_case->set_name("convert_case");

	highlighters[TTR("Standard")] = NULL;
	highlighter_menu = memnew(PopupMenu);
	highlighter_menu->set_name("highlighter_menu");
	highlighter_menu->add_radio_check_item(TTR("Standard"));

	search_menu = memnew(MenuButton);
	search_menu->set_text(TTR("Search"));
	search_menu->set_switch_on_hover(true);
	search_menu->get_popup()->set_hide_on_window_lose_focus(true);

	goto_menu = memnew(MenuButton);
	goto_menu->set_text(TTR("Go To"));
	goto_menu->set_switch_on_hover(true);

	bookmarks_menu = memnew(PopupMenu);
	bookmarks_menu->set_name("Bookmarks");

	breakpoints_menu = memnew(PopupMenu);
	breakpoints_menu->set_name("Breakpoints");

	quick_open = nullptr;
	goto_line_dialog = nullptr;

	connection_info_dialog = memnew(ConnectionInfoDialog);

	code_editor->get_text_edit()->set_drag_forwarding(this);
}

ScriptTextEditor::~ScriptTextEditor() {
	for (const Map<String, SyntaxHighlighter *>::Element *E = highlighters.front(); E; E = E->next()) {
		if (E->get() != NULL) {
			memdelete(E->get());
		}
	}
	highlighters.clear();

	if (!editor_enabled) {
		memdelete(code_editor);
		memdelete(warnings_panel);
		memdelete(context_menu);
		memdelete(color_panel);
		memdelete(edit_hb);
		memdelete(edit_menu);
		memdelete(convert_case);
		memdelete(highlighter_menu);
		memdelete(search_menu);
		memdelete(goto_menu);
		memdelete(bookmarks_menu);
		memdelete(breakpoints_menu);
		memdelete(connection_info_dialog);
	}
}

static ScriptEditorBase *create_editor(const RES &p_resource) {
	if (Object::cast_to<Script>(*p_resource)) {
		return memnew(ScriptTextEditor);
	}
	return nullptr;
}

void ScriptTextEditor::register_editor() {
	ED_SHORTCUT("script_text_editor/undo", TTR("Undo"), KEY_MASK_CMD | KEY_Z);
	ED_SHORTCUT("script_text_editor/redo", TTR("Redo"), KEY_MASK_CMD | KEY_Y);
	ED_SHORTCUT("script_text_editor/cut", TTR("Cut"), KEY_MASK_CMD | KEY_X);
	ED_SHORTCUT("script_text_editor/copy", TTR("Copy"), KEY_MASK_CMD | KEY_C);
	ED_SHORTCUT("script_text_editor/paste", TTR("Paste"), KEY_MASK_CMD | KEY_V);
	ED_SHORTCUT("script_text_editor/select_all", TTR("Select All"), KEY_MASK_CMD | KEY_A);
	ED_SHORTCUT("script_text_editor/move_up", TTR("Move Up"), KEY_MASK_ALT | KEY_UP);
	ED_SHORTCUT("script_text_editor/move_down", TTR("Move Down"), KEY_MASK_ALT | KEY_DOWN);
	ED_SHORTCUT("script_text_editor/delete_line", TTR("Delete Line"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_K);

	// Leave these at zero, same can be accomplished with tab/shift-tab, including selection.
	// The next/previous in history shortcut in this case makes a lot more sense.

	ED_SHORTCUT("script_text_editor/indent_left", TTR("Indent Left"), 0);
	ED_SHORTCUT("script_text_editor/indent_right", TTR("Indent Right"), 0);
	ED_SHORTCUT("script_text_editor/toggle_comment", TTR("Toggle Comment"), KEY_MASK_CMD | KEY_K);
	ED_SHORTCUT("script_text_editor/toggle_fold_line", TTR("Fold/Unfold Line"), KEY_MASK_ALT | KEY_F);
	ED_SHORTCUT("script_text_editor/fold_all_lines", TTR("Fold All Lines"), 0);
	ED_SHORTCUT("script_text_editor/unfold_all_lines", TTR("Unfold All Lines"), 0);
#ifdef OSX_ENABLED
	ED_SHORTCUT("script_text_editor/duplicate_selection", TTR("Duplicate Selection"), KEY_MASK_SHIFT | KEY_MASK_CMD | KEY_C);
	ED_SHORTCUT("script_text_editor/complete_symbol", TTR("Complete Symbol"), KEY_MASK_CTRL | KEY_SPACE);
#else
	ED_SHORTCUT("script_text_editor/duplicate_selection", TTR("Duplicate Selection"), KEY_MASK_CMD | KEY_D);
	ED_SHORTCUT("script_text_editor/complete_symbol", TTR("Complete Symbol"), KEY_MASK_CMD | KEY_SPACE);
#endif
	ED_SHORTCUT("script_text_editor/evaluate_selection", TTR("Evaluate Selection"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_E);
	ED_SHORTCUT("script_text_editor/trim_trailing_whitespace", TTR("Trim Trailing Whitespace"), KEY_MASK_CMD | KEY_MASK_ALT | KEY_T);
	ED_SHORTCUT("script_text_editor/convert_indent_to_spaces", TTR("Convert Indent to Spaces"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_Y);
	ED_SHORTCUT("script_text_editor/convert_indent_to_tabs", TTR("Convert Indent to Tabs"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_I);
	ED_SHORTCUT("script_text_editor/auto_indent", TTR("Auto Indent"), KEY_MASK_CMD | KEY_I);

	ED_SHORTCUT("script_text_editor/find", TTR("Find..."), KEY_MASK_CMD | KEY_F);
#ifdef OSX_ENABLED
	ED_SHORTCUT("script_text_editor/find_next", TTR("Find Next"), KEY_MASK_CMD | KEY_G);
	ED_SHORTCUT("script_text_editor/find_previous", TTR("Find Previous"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_G);
	ED_SHORTCUT("script_text_editor/replace", TTR("Replace..."), KEY_MASK_ALT | KEY_MASK_CMD | KEY_F);
#else
	ED_SHORTCUT("script_text_editor/find_next", TTR("Find Next"), KEY_F3);
	ED_SHORTCUT("script_text_editor/find_previous", TTR("Find Previous"), KEY_MASK_SHIFT | KEY_F3);
	ED_SHORTCUT("script_text_editor/replace", TTR("Replace..."), KEY_MASK_CMD | KEY_R);
#endif

	ED_SHORTCUT("script_text_editor/find_in_files", TTR("Find in Files..."), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_F);
	ED_SHORTCUT("script_text_editor/replace_in_files", TTR("Replace in Files..."), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_R);

#ifdef OSX_ENABLED
	ED_SHORTCUT("script_text_editor/contextual_help", TTR("Contextual Help"), KEY_MASK_ALT | KEY_MASK_SHIFT | KEY_SPACE);
#else
	ED_SHORTCUT("script_text_editor/contextual_help", TTR("Contextual Help"), KEY_MASK_ALT | KEY_F1);
#endif

	ED_SHORTCUT("script_text_editor/toggle_bookmark", TTR("Toggle Bookmark"), KEY_MASK_CMD | KEY_MASK_ALT | KEY_B);
	ED_SHORTCUT("script_text_editor/goto_next_bookmark", TTR("Go to Next Bookmark"), KEY_MASK_CMD | KEY_B);
	ED_SHORTCUT("script_text_editor/goto_previous_bookmark", TTR("Go to Previous Bookmark"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_B);
	ED_SHORTCUT("script_text_editor/remove_all_bookmarks", TTR("Remove All Bookmarks"), 0);

#ifdef OSX_ENABLED
	ED_SHORTCUT("script_text_editor/goto_function", TTR("Go to Function..."), KEY_MASK_CTRL | KEY_MASK_CMD | KEY_J);
#else
	ED_SHORTCUT("script_text_editor/goto_function", TTR("Go to Function..."), KEY_MASK_ALT | KEY_MASK_CMD | KEY_F);
#endif
	ED_SHORTCUT("script_text_editor/goto_line", TTR("Go to Line..."), KEY_MASK_CMD | KEY_L);

#ifdef OSX_ENABLED
	ED_SHORTCUT("script_text_editor/toggle_breakpoint", TTR("Toggle Breakpoint"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_B);
#else
	ED_SHORTCUT("script_text_editor/toggle_breakpoint", TTR("Toggle Breakpoint"), KEY_F9);
#endif
	ED_SHORTCUT("script_text_editor/remove_all_breakpoints", TTR("Remove All Breakpoints"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_F9);
	ED_SHORTCUT("script_text_editor/goto_next_breakpoint", TTR("Go to Next Breakpoint"), KEY_MASK_CMD | KEY_PERIOD);
	ED_SHORTCUT("script_text_editor/goto_previous_breakpoint", TTR("Go to Previous Breakpoint"), KEY_MASK_CMD | KEY_COMMA);

	ScriptEditor::register_create_script_editor_function(create_editor);
}

void ScriptTextEditor::validate() {
	this->code_editor->validate_script();
}
