/**************************************************************************/
/*  script_editor_plugin.cpp                                              */
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

#include "script_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/version.h"
#include "editor/code_editor.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_help_search.h"
#include "editor/editor_interface.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_script.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/filesystem_dock.h"
#include "editor/find_in_files.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/gui/editor_toaster.h"
#include "editor/inspector_dock.h"
#include "editor/node_dock.h"
#include "editor/plugins/editor_context_menu_plugin.h"
#include "editor/plugins/shader_editor_plugin.h"
#include "editor/plugins/text_shader_editor.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "editor/window_wrapper.h"
#include "scene/main/node.h"
#include "scene/main/window.h"
#include "script_text_editor.h"
#include "servers/display_server.h"
#include "text_editor.h"

/*** SYNTAX HIGHLIGHTER ****/

String EditorSyntaxHighlighter::_get_name() const {
	String ret = "Unnamed";
	GDVIRTUAL_CALL(_get_name, ret);
	return ret;
}

PackedStringArray EditorSyntaxHighlighter::_get_supported_languages() const {
	PackedStringArray ret;
	GDVIRTUAL_CALL(_get_supported_languages, ret);
	return ret;
}

Ref<EditorSyntaxHighlighter> EditorSyntaxHighlighter::_create() const {
	Ref<EditorSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	if (get_script_instance()) {
		syntax_highlighter->set_script(get_script_instance()->get_script());
	}
	return syntax_highlighter;
}

void EditorSyntaxHighlighter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_get_edited_resource"), &EditorSyntaxHighlighter::_get_edited_resource);

	GDVIRTUAL_BIND(_get_name)
	GDVIRTUAL_BIND(_get_supported_languages)
}

////

void EditorStandardSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_function_color(EDITOR_GET("text_editor/theme/highlighting/function_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	highlighter->set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/member_variable_color"));

	/* Engine types. */
	const Color type_color = EDITOR_GET("text_editor/theme/highlighting/engine_type_color");
	List<StringName> types;
	ClassDB::get_class_list(&types);
	for (const StringName &E : types) {
		highlighter->add_keyword_color(E, type_color);
	}

	/* User types. */
	const Color usertype_color = EDITOR_GET("text_editor/theme/highlighting/user_type_color");
	List<StringName> global_classes;
	ScriptServer::get_global_class_list(&global_classes);
	for (const StringName &E : global_classes) {
		highlighter->add_keyword_color(E, usertype_color);
	}

	/* Autoloads. */
	HashMap<StringName, ProjectSettings::AutoloadInfo> autoloads = ProjectSettings::get_singleton()->get_autoload_list();
	for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
		const ProjectSettings::AutoloadInfo &info = E.value;
		if (info.is_singleton) {
			highlighter->add_keyword_color(info.name, usertype_color);
		}
	}

	const ScriptLanguage *scr_lang = script_language;
	StringName instance_base;

	if (scr_lang == nullptr) {
		const Ref<Script> scr = _get_edited_resource();
		if (scr.is_valid()) {
			scr_lang = scr->get_language();
			instance_base = scr->get_instance_base_type();
		}
	}

	if (scr_lang != nullptr) {
		/* Core types. */
		const Color basetype_color = EDITOR_GET("text_editor/theme/highlighting/base_type_color");
		List<String> core_types;
		scr_lang->get_core_type_words(&core_types);
		for (const String &E : core_types) {
			highlighter->add_keyword_color(E, basetype_color);
		}

		/* Reserved words. */
		const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
		const Color control_flow_keyword_color = EDITOR_GET("text_editor/theme/highlighting/control_flow_keyword_color");
		List<String> keywords;
		scr_lang->get_reserved_words(&keywords);
		for (const String &E : keywords) {
			if (scr_lang->is_control_flow_keyword(E)) {
				highlighter->add_keyword_color(E, control_flow_keyword_color);
			} else {
				highlighter->add_keyword_color(E, keyword_color);
			}
		}

		/* Member types. */
		const Color member_variable_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
		if (instance_base != StringName()) {
			List<PropertyInfo> plist;
			ClassDB::get_property_list(instance_base, &plist);
			for (const PropertyInfo &E : plist) {
				String prop_name = E.name;
				if (E.usage & PROPERTY_USAGE_CATEGORY || E.usage & PROPERTY_USAGE_GROUP || E.usage & PROPERTY_USAGE_SUBGROUP) {
					continue;
				}
				if (prop_name.contains("/")) {
					continue;
				}
				highlighter->add_member_keyword_color(prop_name, member_variable_color);
			}

			List<String> clist;
			ClassDB::get_integer_constant_list(instance_base, &clist);
			for (const String &E : clist) {
				highlighter->add_member_keyword_color(E, member_variable_color);
			}
		}

		/* Comments */
		const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
		List<String> comments;
		scr_lang->get_comment_delimiters(&comments);
		for (const String &comment : comments) {
			String beg = comment.get_slice(" ", 0);
			String end = comment.get_slice_count(" ") > 1 ? comment.get_slice(" ", 1) : String();
			highlighter->add_color_region(beg, end, comment_color, end.is_empty());
		}

		/* Doc comments */
		const Color doc_comment_color = EDITOR_GET("text_editor/theme/highlighting/doc_comment_color");
		List<String> doc_comments;
		scr_lang->get_doc_comment_delimiters(&doc_comments);
		for (const String &doc_comment : doc_comments) {
			String beg = doc_comment.get_slice(" ", 0);
			String end = doc_comment.get_slice_count(" ") > 1 ? doc_comment.get_slice(" ", 1) : String();
			highlighter->add_color_region(beg, end, doc_comment_color, end.is_empty());
		}

		/* Strings */
		const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
		List<String> strings;
		scr_lang->get_string_delimiters(&strings);
		for (const String &string : strings) {
			String beg = string.get_slice(" ", 0);
			String end = string.get_slice_count(" ") > 1 ? string.get_slice(" ", 1) : String();
			highlighter->add_color_region(beg, end, string_color, end.is_empty());
		}
	}
}

Ref<EditorSyntaxHighlighter> EditorStandardSyntaxHighlighter::_create() const {
	Ref<EditorStandardSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

Ref<EditorSyntaxHighlighter> EditorPlainTextSyntaxHighlighter::_create() const {
	Ref<EditorPlainTextSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

void EditorJSONSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));

	const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region("\"", "\"", string_color);
}

Ref<EditorSyntaxHighlighter> EditorJSONSyntaxHighlighter::_create() const {
	Ref<EditorJSONSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////

void EditorMarkdownSyntaxHighlighter::_update_cache() {
	highlighter->set_text_edit(text_edit);
	highlighter->clear_keyword_colors();
	highlighter->clear_member_keyword_colors();
	highlighter->clear_color_regions();

	// Disable automatic symbolic highlights, as these don't make sense for prose.
	highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));
	highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));
	highlighter->set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));
	highlighter->set_function_color(EDITOR_GET("text_editor/theme/highlighting/text_color"));

	// Headings (any level).
	const Color function_color = EDITOR_GET("text_editor/theme/highlighting/function_color");
	highlighter->add_color_region("#", "", function_color);

	// Bold.
	highlighter->add_color_region("**", "**", function_color);
	// `__bold__` syntax is not supported as color regions must begin with a symbol,
	// not a character that is valid in an identifier.

	// Code (both inline code and triple-backticks code blocks).
	const Color code_color = EDITOR_GET("text_editor/theme/highlighting/engine_type_color");
	highlighter->add_color_region("`", "`", code_color);

	// Link (both references and inline links with URLs). The URL is not highlighted.
	const Color link_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	highlighter->add_color_region("[", "]", link_color);

	// Quote.
	const Color quote_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	highlighter->add_color_region(">", "", quote_color, true);

	// HTML comment, which is also supported in Markdown.
	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	highlighter->add_color_region("<!--", "-->", comment_color);
}

Ref<EditorSyntaxHighlighter> EditorMarkdownSyntaxHighlighter::_create() const {
	Ref<EditorMarkdownSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}

////////////////////////////////////////////////////////////////////////////////

/*** SCRIPT EDITOR ****/

void ScriptEditorBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_base_editor"), &ScriptEditorBase::get_base_editor);
	ClassDB::bind_method(D_METHOD("add_syntax_highlighter", "highlighter"), &ScriptEditorBase::add_syntax_highlighter);

	ADD_SIGNAL(MethodInfo("name_changed"));
	ADD_SIGNAL(MethodInfo("edited_script_changed"));
	ADD_SIGNAL(MethodInfo("request_help", PropertyInfo(Variant::STRING, "topic")));
	ADD_SIGNAL(MethodInfo("request_open_script_at_line", PropertyInfo(Variant::OBJECT, "script"), PropertyInfo(Variant::INT, "line")));
	ADD_SIGNAL(MethodInfo("request_save_history"));
	ADD_SIGNAL(MethodInfo("request_save_previous_state", PropertyInfo(Variant::DICTIONARY, "state")));
	ADD_SIGNAL(MethodInfo("go_to_help", PropertyInfo(Variant::STRING, "what")));
	ADD_SIGNAL(MethodInfo("search_in_files_requested", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("replace_in_files_requested", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("go_to_method", PropertyInfo(Variant::OBJECT, "script"), PropertyInfo(Variant::STRING, "method")));
}

class EditorScriptCodeCompletionCache : public ScriptCodeCompletionCache {
	struct Cache {
		uint64_t time_loaded = 0;
		Ref<Resource> cache;
	};

	HashMap<String, Cache> cached;

public:
	uint64_t max_time_cache = 5 * 60 * 1000; //minutes, five
	uint32_t max_cache_size = 128;

	void cleanup() {
		List<String> to_clean;

		HashMap<String, Cache>::Iterator I = cached.begin();
		while (I) {
			if ((OS::get_singleton()->get_ticks_msec() - I->value.time_loaded) > max_time_cache) {
				to_clean.push_back(I->key);
			}
			++I;
		}

		while (to_clean.front()) {
			cached.erase(to_clean.front()->get());
			to_clean.pop_front();
		}
	}

	virtual Ref<Resource> get_cached_resource(const String &p_path) {
		HashMap<String, Cache>::Iterator E = cached.find(p_path);
		if (!E) {
			Cache c;
			c.cache = ResourceLoader::load(p_path);
			E = cached.insert(p_path, c);
		}

		E->value.time_loaded = OS::get_singleton()->get_ticks_msec();

		if (cached.size() > max_cache_size) {
			uint64_t older;
			HashMap<String, Cache>::Iterator O = cached.begin();
			older = O->value.time_loaded;
			HashMap<String, Cache>::Iterator I = O;
			while (I) {
				if (I->value.time_loaded < older) {
					older = I->value.time_loaded;
					O = I;
				}
				++I;
			}

			if (O != E) { //should never happen..
				cached.remove(O);
			}
		}

		return E->value.cache;
	}

	virtual ~EditorScriptCodeCompletionCache() {}
};

void ScriptEditorQuickOpen::popup_dialog(const Vector<String> &p_functions, bool p_dontclear) {
	popup_centered_ratio(0.6);
	if (p_dontclear) {
		search_box->select_all();
	} else {
		search_box->clear();
	}
	search_box->grab_focus();
	functions = p_functions;
	_update_search();
}

void ScriptEditorQuickOpen::_text_changed(const String &p_newtext) {
	_update_search();
}

void ScriptEditorQuickOpen::_sbox_input(const Ref<InputEvent> &p_event) {
	// Redirect navigational key events to the tree.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (key->is_action("ui_up", true) || key->is_action("ui_down", true) || key->is_action("ui_page_up") || key->is_action("ui_page_down")) {
			search_options->gui_input(key);
			search_box->accept_event();
		}
	}
}

void ScriptEditorQuickOpen::_update_search() {
	search_options->clear();
	TreeItem *root = search_options->create_item();

	for (int i = 0; i < functions.size(); i++) {
		String file = functions[i];
		if ((search_box->get_text().is_empty() || file.containsn(search_box->get_text()))) {
			TreeItem *ti = search_options->create_item(root);
			ti->set_text(0, file);
			if (root->get_first_child() == ti) {
				ti->select(0);
			}
		}
	}

	get_ok_button()->set_disabled(root->get_first_child() == nullptr);
}

void ScriptEditorQuickOpen::_confirmed() {
	TreeItem *ti = search_options->get_selected();
	if (!ti) {
		return;
	}
	int line = ti->get_text(0).get_slice(":", 1).to_int();

	emit_signal(SNAME("goto_line"), line - 1);
	hide();
}

void ScriptEditorQuickOpen::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			connect(SceneStringName(confirmed), callable_mp(this, &ScriptEditorQuickOpen::_confirmed));

			search_box->set_clear_button_enabled(true);
			[[fallthrough]];
		}
		case NOTIFICATION_VISIBILITY_CHANGED: {
			search_box->set_right_icon(search_options->get_editor_theme_icon(SNAME("Search")));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			disconnect(SceneStringName(confirmed), callable_mp(this, &ScriptEditorQuickOpen::_confirmed));
		} break;
	}
}

void ScriptEditorQuickOpen::_bind_methods() {
	ADD_SIGNAL(MethodInfo("goto_line", PropertyInfo(Variant::INT, "line")));
}

ScriptEditorQuickOpen::ScriptEditorQuickOpen() {
	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);
	search_box = memnew(LineEdit);
	vbc->add_margin_child(TTR("Search:"), search_box);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &ScriptEditorQuickOpen::_text_changed));
	search_box->connect(SceneStringName(gui_input), callable_mp(this, &ScriptEditorQuickOpen::_sbox_input));
	search_options = memnew(Tree);
	vbc->add_margin_child(TTR("Matches:"), search_options, true);
	set_ok_button_text(TTR("Open"));
	get_ok_button()->set_disabled(true);
	register_text_enter(search_box);
	set_hide_on_ok(false);
	search_options->connect("item_activated", callable_mp(this, &ScriptEditorQuickOpen::_confirmed));
	search_options->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	search_options->set_hide_root(true);
	search_options->set_hide_folding(true);
	search_options->add_theme_constant_override("draw_guides", 1);
}

/////////////////////////////////

ScriptEditor *ScriptEditor::script_editor = nullptr;

/*** SCRIPT EDITOR ******/

String ScriptEditor::_get_debug_tooltip(const String &p_text, Node *_se) {
	String val = EditorDebuggerNode::get_singleton()->get_var_value(p_text);
	const int display_limit = 300;
	if (!val.is_empty()) {
		if (val.size() > display_limit) {
			val = val.left(display_limit) + " [...] truncated!";
		}
		return p_text + ": " + val;
	} else {
		return String();
	}
}

void ScriptEditor::_breaked(bool p_breaked, bool p_can_debug) {
	if (external_editor_active) {
		return;
	}

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		se->set_debugger_active(p_breaked);
	}
}

void ScriptEditor::_script_created(Ref<Script> p_script) {
	EditorNode::get_singleton()->push_item(p_script.operator->());
}

void ScriptEditor::_goto_script_line2(int p_line) {
	ScriptEditorBase *current = _get_current_editor();
	if (current) {
		current->goto_line(p_line);
	}
}

void ScriptEditor::_goto_script_line(Ref<RefCounted> p_script, int p_line) {
	Ref<Script> scr = Object::cast_to<Script>(*p_script);
	if (scr.is_valid() && (scr->has_source_code() || scr->get_path().is_resource_file())) {
		if (edit(p_script, p_line, 0)) {
			EditorNode::get_singleton()->push_item(p_script.ptr());

			ScriptEditorBase *current = _get_current_editor();
			if (ScriptTextEditor *script_text_editor = Object::cast_to<ScriptTextEditor>(current)) {
				script_text_editor->goto_line_centered(p_line);
			} else if (current) {
				current->goto_line(p_line);
			}

			_save_history();
		}
	}
}

void ScriptEditor::_set_execution(Ref<RefCounted> p_script, int p_line) {
	Ref<Script> scr = Object::cast_to<Script>(*p_script);
	if (scr.is_valid() && (scr->has_source_code() || scr->get_path().is_resource_file())) {
		for (int i = 0; i < tab_container->get_tab_count(); i++) {
			ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
			if (!se) {
				continue;
			}

			if ((scr.is_valid() && se->get_edited_resource() == p_script) || se->get_edited_resource()->get_path() == scr->get_path()) {
				se->set_executing_line(p_line);
			}
		}
	}
}

void ScriptEditor::_clear_execution(Ref<RefCounted> p_script) {
	Ref<Script> scr = Object::cast_to<Script>(*p_script);
	if (scr.is_valid() && (scr->has_source_code() || scr->get_path().is_resource_file())) {
		for (int i = 0; i < tab_container->get_tab_count(); i++) {
			ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
			if (!se) {
				continue;
			}

			if ((scr.is_valid() && se->get_edited_resource() == p_script) || se->get_edited_resource()->get_path() == scr->get_path()) {
				se->clear_executing_line();
			}
		}
	}
}

void ScriptEditor::_set_breakpoint(Ref<RefCounted> p_script, int p_line, bool p_enabled) {
	Ref<Script> scr = Object::cast_to<Script>(*p_script);
	if (scr.is_valid() && (scr->has_source_code() || scr->get_path().is_resource_file())) {
		// Update if open.
		for (int i = 0; i < tab_container->get_tab_count(); i++) {
			ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
			if (se && se->get_edited_resource()->get_path() == scr->get_path()) {
				se->set_breakpoint(p_line, p_enabled);
				return;
			}
		}

		// Handle closed.
		Dictionary state = script_editor_cache->get_value(scr->get_path(), "state");
		Array breakpoints;
		if (state.has("breakpoints")) {
			breakpoints = state["breakpoints"];
		}

		if (breakpoints.has(p_line)) {
			if (!p_enabled) {
				breakpoints.erase(p_line);
			}
		} else if (p_enabled) {
			breakpoints.push_back(p_line);
		}
		state["breakpoints"] = breakpoints;
		script_editor_cache->set_value(scr->get_path(), "state", state);
		EditorDebuggerNode::get_singleton()->set_breakpoint(scr->get_path(), p_line + 1, p_enabled);
	}
}

void ScriptEditor::_clear_breakpoints() {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (se) {
			se->clear_breakpoints();
		}
	}

	// Clear from closed scripts.
	List<String> cached_editors;
	script_editor_cache->get_sections(&cached_editors);
	for (const String &E : cached_editors) {
		Array breakpoints = _get_cached_breakpoints_for_script(E);
		for (int breakpoint : breakpoints) {
			EditorDebuggerNode::get_singleton()->set_breakpoint(E, (int)breakpoint + 1, false);
		}

		if (breakpoints.size() > 0) {
			Dictionary state = script_editor_cache->get_value(E, "state");
			state["breakpoints"] = Array();
			script_editor_cache->set_value(E, "state", state);
		}
	}
}

Array ScriptEditor::_get_cached_breakpoints_for_script(const String &p_path) const {
	if (!ResourceLoader::exists(p_path, "Script") || p_path.begins_with("local://") || !script_editor_cache->has_section_key(p_path, "state")) {
		return Array();
	}

	Dictionary state = script_editor_cache->get_value(p_path, "state");
	if (!state.has("breakpoints")) {
		return Array();
	}
	return state["breakpoints"];
}

ScriptEditorBase *ScriptEditor::_get_current_editor() const {
	int selected = tab_container->get_current_tab();
	if (selected < 0 || selected >= tab_container->get_tab_count()) {
		return nullptr;
	}

	return Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(selected));
}

void ScriptEditor::_update_history_arrows() {
	script_back->set_disabled(history_pos <= 0);
	script_forward->set_disabled(history_pos >= history.size() - 1);
}

void ScriptEditor::_save_history() {
	if (history_pos >= 0 && history_pos < history.size() && history[history_pos].control == tab_container->get_current_tab_control()) {
		Node *n = tab_container->get_current_tab_control();

		if (Object::cast_to<ScriptEditorBase>(n)) {
			history.write[history_pos].state = Object::cast_to<ScriptEditorBase>(n)->get_navigation_state();
		}
		if (Object::cast_to<EditorHelp>(n)) {
			history.write[history_pos].state = Object::cast_to<EditorHelp>(n)->get_scroll();
		}
	}

	history.resize(history_pos + 1);
	ScriptHistory sh;
	sh.control = tab_container->get_current_tab_control();
	sh.state = Variant();

	history.push_back(sh);
	history_pos++;

	_update_history_arrows();
}

void ScriptEditor::_save_previous_state(Dictionary p_state) {
	if (lock_history) {
		// Done as a result of a deferred call triggered by set_edit_state().
		lock_history = false;
		return;
	}

	if (history_pos >= 0 && history_pos < history.size() && history[history_pos].control == tab_container->get_current_tab_control()) {
		Node *n = tab_container->get_current_tab_control();

		if (Object::cast_to<ScriptTextEditor>(n)) {
			history.write[history_pos].state = p_state;
		}
	}

	history.resize(history_pos + 1);
	ScriptHistory sh;
	sh.control = tab_container->get_current_tab_control();
	sh.state = Variant();

	history.push_back(sh);
	history_pos++;

	_update_history_arrows();
}

void ScriptEditor::_go_to_tab(int p_idx) {
	ScriptEditorBase *current = _get_current_editor();
	if (current) {
		if (current->is_unsaved()) {
			current->apply_code();
		}
	}

	Control *c = tab_container->get_tab_control(p_idx);
	if (!c) {
		return;
	}

	if (history_pos >= 0 && history_pos < history.size() && history[history_pos].control == tab_container->get_current_tab_control()) {
		Node *n = tab_container->get_current_tab_control();

		if (Object::cast_to<ScriptEditorBase>(n)) {
			history.write[history_pos].state = Object::cast_to<ScriptEditorBase>(n)->get_navigation_state();
		}
		if (Object::cast_to<EditorHelp>(n)) {
			history.write[history_pos].state = Object::cast_to<EditorHelp>(n)->get_scroll();
		}
	}

	history.resize(history_pos + 1);
	ScriptHistory sh;
	sh.control = c;
	sh.state = Variant();

	if (!lock_history && (history.is_empty() || history[history.size() - 1].control != sh.control)) {
		history.push_back(sh);
		history_pos++;
	}

	tab_container->set_current_tab(p_idx);

	c = tab_container->get_current_tab_control();

	if (Object::cast_to<ScriptEditorBase>(c)) {
		script_name_label->set_text(Object::cast_to<ScriptEditorBase>(c)->get_name());
		script_icon->set_texture(Object::cast_to<ScriptEditorBase>(c)->get_theme_icon());
		if (is_visible_in_tree()) {
			Object::cast_to<ScriptEditorBase>(c)->ensure_focus();
		}

		Ref<Script> scr = Object::cast_to<ScriptEditorBase>(c)->get_edited_resource();
		if (scr.is_valid()) {
			notify_script_changed(scr);
		}

		Object::cast_to<ScriptEditorBase>(c)->validate();
	}
	if (Object::cast_to<EditorHelp>(c)) {
		script_name_label->set_text(Object::cast_to<EditorHelp>(c)->get_class());
		script_icon->set_texture(get_editor_theme_icon(SNAME("Help")));
		if (is_visible_in_tree()) {
			Object::cast_to<EditorHelp>(c)->set_focused();
		}
	}

	c->set_meta("__editor_pass", ++edit_pass);
	_update_history_arrows();
	_update_script_colors();
	_update_members_overview();
	_update_help_overview();
	_update_selected_editor_menu();
	_update_online_doc();
	_update_members_overview_visibility();
	_update_help_overview_visibility();
}

void ScriptEditor::_add_recent_script(const String &p_path) {
	if (p_path.is_empty()) {
		return;
	}

	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scripts", Array());
	if (rc.has(p_path)) {
		rc.erase(p_path);
	}
	rc.push_front(p_path);
	if (rc.size() > 10) {
		rc.resize(10);
	}

	EditorSettings::get_singleton()->set_project_metadata("recent_files", "scripts", rc);
	_update_recent_scripts();
}

void ScriptEditor::_update_recent_scripts() {
	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scripts", Array());
	recent_scripts->clear();

	String path;
	for (int i = 0; i < rc.size(); i++) {
		path = rc[i];
		recent_scripts->add_item(path.replace("res://", ""));
	}

	recent_scripts->add_separator();
	recent_scripts->add_shortcut(ED_GET_SHORTCUT("script_editor/clear_recent"));
	recent_scripts->set_item_disabled(recent_scripts->get_item_id(recent_scripts->get_item_count() - 1), rc.is_empty());

	recent_scripts->reset_size();
}

void ScriptEditor::_open_recent_script(int p_idx) {
	// clear button
	if (p_idx == recent_scripts->get_item_count() - 1) {
		EditorSettings::get_singleton()->set_project_metadata("recent_files", "scripts", Array());
		callable_mp(this, &ScriptEditor::_update_recent_scripts).call_deferred();
		return;
	}

	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scripts", Array());
	ERR_FAIL_INDEX(p_idx, rc.size());

	String path = rc[p_idx];
	// if its not on disk its a help file or deleted
	if (FileAccess::exists(path)) {
		List<String> extensions;
		ResourceLoader::get_recognized_extensions_for_type("Script", &extensions);
		ResourceLoader::get_recognized_extensions_for_type("JSON", &extensions);

		if (extensions.find(path.get_extension())) {
			Ref<Resource> scr = ResourceLoader::load(path);
			if (scr.is_valid()) {
				edit(scr, true);
				return;
			}
		}

		Error err;
		Ref<TextFile> text_file = _load_text_file(path, &err);
		if (text_file.is_valid()) {
			edit(text_file, true);
			return;
		}
		// if it's a path then it's most likely a deleted file not help
	} else if (path.contains("::")) {
		// built-in script
		String res_path = path.get_slice("::", 0);
		if (ResourceLoader::get_resource_type(res_path) == "PackedScene") {
			if (!EditorNode::get_singleton()->is_scene_open(res_path)) {
				EditorNode::get_singleton()->load_scene(res_path);
			}
		} else {
			EditorNode::get_singleton()->load_resource(res_path);
		}
		Ref<Script> scr = ResourceLoader::load(path);
		if (scr.is_valid()) {
			edit(scr, true);
			return;
		}
	} else if (!path.is_resource_file()) {
		_help_class_open(path);
		return;
	}

	rc.remove_at(p_idx);
	EditorSettings::get_singleton()->set_project_metadata("recent_files", "scripts", rc);
	_update_recent_scripts();
	_show_error_dialog(path);
}

void ScriptEditor::_show_error_dialog(const String &p_path) {
	error_dialog->set_text(vformat(TTR("Can't open '%s'. The file could have been moved or deleted."), p_path));
	error_dialog->popup_centered();
}

void ScriptEditor::_close_tab(int p_idx, bool p_save, bool p_history_back) {
	int selected = p_idx;
	if (selected < 0 || selected >= tab_container->get_tab_count()) {
		return;
	}

	Node *tselected = tab_container->get_tab_control(selected);

	ScriptEditorBase *current = Object::cast_to<ScriptEditorBase>(tselected);
	if (current) {
		Ref<Resource> file = current->get_edited_resource();
		if (p_save && file.is_valid()) {
			// Do not try to save internal scripts, but prompt to save in-memory
			// scripts which are not saved to disk yet (have empty path).
			if (!file->is_built_in()) {
				save_current_script();
			}
		}
		if (file.is_valid()) {
			if (!file->get_path().is_empty()) {
				// Only saved scripts can be restored.
				previous_scripts.push_back(file->get_path());
			}

			Ref<Script> scr = file;
			if (scr.is_valid()) {
				notify_script_close(scr);
			}
		}
	}

	// roll back to previous tab
	if (p_history_back) {
		_history_back();
	}

	//remove from history
	history.resize(history_pos + 1);

	for (int i = 0; i < history.size(); i++) {
		if (history[i].control == tselected) {
			history.remove_at(i);
			i--;
			history_pos--;
		}
	}

	if (history_pos >= history.size()) {
		history_pos = history.size() - 1;
	}

	int idx = tab_container->get_current_tab();
	if (current) {
		current->clear_edit_menu();
		_save_editor_state(current);
	}
	memdelete(tselected);

	if (script_close_queue.is_empty()) {
		if (idx >= tab_container->get_tab_count()) {
			idx = tab_container->get_tab_count() - 1;
		}
		if (idx >= 0) {
			if (history_pos >= 0) {
				idx = tab_container->get_tab_idx_from_control(history[history_pos].control);
			}
			_go_to_tab(idx);
		} else {
			_update_selected_editor_menu();
			_update_online_doc();
		}

		_update_history_arrows();
		_update_script_names();
		_save_layout();
		_update_find_replace_bar();
	}
}

void ScriptEditor::_close_current_tab(bool p_save, bool p_history_back) {
	_close_tab(tab_container->get_current_tab(), p_save, p_history_back);
}

void ScriptEditor::_close_discard_current_tab(const String &p_str) {
	Ref<Script> scr = _get_current_script();
	if (scr.is_valid()) {
		scr->reload_from_file();
	}
	_close_tab(tab_container->get_current_tab(), false);
	erase_tab_confirm->hide();
}

void ScriptEditor::_close_docs_tab() {
	int child_count = tab_container->get_tab_count();
	for (int i = child_count - 1; i >= 0; i--) {
		EditorHelp *se = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));

		if (se) {
			_close_tab(i, true, false);
		}
	}
}

void ScriptEditor::_copy_script_path() {
	ScriptEditorBase *se = _get_current_editor();
	if (se) {
		Ref<Resource> scr = se->get_edited_resource();
		DisplayServer::get_singleton()->clipboard_set(scr->get_path());
	}
}

void ScriptEditor::_close_other_tabs() {
	int current_idx = tab_container->get_current_tab();
	for (int i = tab_container->get_tab_count() - 1; i >= 0; i--) {
		if (i != current_idx) {
			script_close_queue.push_back(i);
		}
	}
	_queue_close_tabs();
}

void ScriptEditor::_close_all_tabs() {
	for (int i = tab_container->get_tab_count() - 1; i >= 0; i--) {
		script_close_queue.push_back(i);
	}
	_queue_close_tabs();
}

void ScriptEditor::_queue_close_tabs() {
	while (!script_close_queue.is_empty()) {
		int idx = script_close_queue.front()->get();
		script_close_queue.pop_front();

		tab_container->set_current_tab(idx);
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(idx));
		if (se) {
			// Maybe there are unsaved changes.
			if (se->is_unsaved()) {
				_ask_close_current_unsaved_tab(se);
				erase_tab_confirm->connect(SceneStringName(visibility_changed), callable_mp(this, &ScriptEditor::_queue_close_tabs), CONNECT_ONE_SHOT);
				break;
			}
		}

		_close_current_tab(false, false);
	}
	_update_find_replace_bar();
}

void ScriptEditor::_ask_close_current_unsaved_tab(ScriptEditorBase *current) {
	erase_tab_confirm->set_text(TTR("Close and save changes?") + "\n\"" + current->get_name() + "\"");
	erase_tab_confirm->popup_centered();
}

void ScriptEditor::_resave_scripts(const String &p_str) {
	apply_scripts();

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		Ref<Resource> scr = se->get_edited_resource();

		if (scr->is_built_in()) {
			continue; //internal script, who cares
		}

		if (trim_trailing_whitespace_on_save) {
			se->trim_trailing_whitespace();
		}

		if (trim_final_newlines_on_save) {
			se->trim_final_newlines();
		}

		if (convert_indent_on_save) {
			se->convert_indent();
		}

		Ref<TextFile> text_file = scr;
		if (text_file.is_valid()) {
			se->apply_code();
			_save_text_file(text_file, text_file->get_path());
			break;
		} else {
			EditorNode::get_singleton()->save_resource(scr);
		}
		se->tag_saved_version();
	}

	disk_changed->hide();
}

void ScriptEditor::_res_saved_callback(const Ref<Resource> &p_res) {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		Ref<Resource> scr = se->get_edited_resource();

		if (scr == p_res) {
			se->tag_saved_version();
		}
	}

	if (p_res.is_valid()) {
		// In case the Resource has built-in scripts.
		_mark_built_in_scripts_as_saved(p_res->get_path());
	}

	_update_script_names();
	Ref<Script> scr = p_res;
	if (scr.is_valid()) {
		trigger_live_script_reload(scr->get_path());
	}
}

void ScriptEditor::_scene_saved_callback(const String &p_path) {
	// If scene was saved, mark all built-in scripts from that scene as saved.
	_mark_built_in_scripts_as_saved(p_path);
}

void ScriptEditor::_mark_built_in_scripts_as_saved(const String &p_parent_path) {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		Ref<Resource> edited_res = se->get_edited_resource();

		if (!edited_res->is_built_in()) {
			continue; // External script, who cares.
		}

		if (edited_res->get_path().get_slice("::", 0) == p_parent_path) {
			se->tag_saved_version();
		}

		Ref<Script> scr = edited_res;
		if (scr.is_valid()) {
			trigger_live_script_reload(scr->get_path());

			if (scr->is_tool()) {
				scr->reload(true);
			}
		}
	}
}

void ScriptEditor::trigger_live_script_reload(const String &p_script_path) {
	if (!script_paths_to_reload.has(p_script_path)) {
		script_paths_to_reload.append(p_script_path);
	}
	if (!pending_auto_reload && auto_reload_running_scripts) {
		callable_mp(this, &ScriptEditor::_live_auto_reload_running_scripts).call_deferred();
		pending_auto_reload = true;
	}
}

void ScriptEditor::trigger_live_script_reload_all() {
	if (!pending_auto_reload && auto_reload_running_scripts) {
		call_deferred(SNAME("_live_auto_reload_running_scripts"));
		pending_auto_reload = true;
		reload_all_scripts = true;
	}
}

void ScriptEditor::_live_auto_reload_running_scripts() {
	pending_auto_reload = false;
	if (reload_all_scripts) {
		EditorDebuggerNode::get_singleton()->reload_all_scripts();
	} else {
		EditorDebuggerNode::get_singleton()->reload_scripts(script_paths_to_reload);
	}
	reload_all_scripts = false;
	script_paths_to_reload.clear();
}

bool ScriptEditor::_test_script_times_on_disk(Ref<Resource> p_for_script) {
	disk_changed_list->clear();
	TreeItem *r = disk_changed_list->create_item();
	disk_changed_list->set_hide_root(true);

	bool need_ask = false;
	bool need_reload = false;
	bool use_autoreload = bool(EDITOR_GET("text_editor/behavior/files/auto_reload_scripts_on_external_change"));

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (se) {
			Ref<Resource> edited_res = se->get_edited_resource();
			if (p_for_script.is_valid() && edited_res.is_valid() && p_for_script != edited_res) {
				continue;
			}

			if (edited_res->is_built_in()) {
				continue; //internal script, who cares
			}

			uint64_t last_date = edited_res->get_last_modified_time();
			uint64_t date = FileAccess::get_modified_time(edited_res->get_path());

			if (last_date != date) {
				TreeItem *ti = disk_changed_list->create_item(r);
				ti->set_text(0, edited_res->get_path().get_file());

				if (!use_autoreload || se->is_unsaved()) {
					need_ask = true;
				}
				need_reload = true;
			}
		}
	}

	if (need_reload) {
		if (!need_ask) {
			script_editor->reload_scripts();
			need_reload = false;
		} else {
			callable_mp((Window *)disk_changed, &Window::popup_centered_ratio).call_deferred(0.3);
		}
	}

	return need_reload;
}

void ScriptEditor::_file_dialog_action(const String &p_file) {
	switch (file_dialog_option) {
		case FILE_NEW_TEXTFILE: {
			Error err;
			{
				Ref<FileAccess> file = FileAccess::open(p_file, FileAccess::WRITE, &err);
				if (err) {
					EditorNode::get_singleton()->show_warning(TTR("Error writing TextFile:") + "\n" + p_file, TTR("Error!"));
					break;
				}
			}

			if (EditorFileSystem::get_singleton()) {
				if (textfile_extensions.has(p_file.get_extension())) {
					EditorFileSystem::get_singleton()->update_file(p_file);
				}
			}

			if (!open_textfile_after_create) {
				return;
			}
			[[fallthrough]];
		}
		case FILE_OPEN: {
			open_file(p_file);
			file_dialog_option = -1;
		} break;
		case FILE_SAVE_AS: {
			ScriptEditorBase *current = _get_current_editor();
			if (current) {
				Ref<Resource> resource = current->get_edited_resource();
				String path = ProjectSettings::get_singleton()->localize_path(p_file);
				Error err = _save_text_file(resource, path);

				if (err != OK) {
					EditorNode::get_singleton()->show_accept(TTR("Error saving file!"), TTR("OK"));
					return;
				}

				resource->set_path(path);
				_update_script_names();
			}
		} break;
		case THEME_SAVE_AS: {
			if (!EditorSettings::get_singleton()->save_text_editor_theme_as(p_file)) {
				EditorNode::get_singleton()->show_warning(TTR("Error while saving theme."), TTR("Error Saving"));
			}
		} break;
		case THEME_IMPORT: {
			if (!EditorSettings::get_singleton()->import_text_editor_theme(p_file)) {
				EditorNode::get_singleton()->show_warning(TTR("Error importing theme."), TTR("Error Importing"));
			}
		} break;
	}
	file_dialog_option = -1;
}

Ref<Script> ScriptEditor::_get_current_script() {
	ScriptEditorBase *current = _get_current_editor();

	if (current) {
		Ref<Script> scr = current->get_edited_resource();
		return scr.is_valid() ? scr : nullptr;
	} else {
		return nullptr;
	}
}

TypedArray<Script> ScriptEditor::_get_open_scripts() const {
	TypedArray<Script> ret;
	Vector<Ref<Script>> scripts = get_open_scripts();
	int scrits_amount = scripts.size();
	for (int idx_script = 0; idx_script < scrits_amount; idx_script++) {
		ret.push_back(scripts[idx_script]);
	}
	return ret;
}

bool ScriptEditor::toggle_scripts_panel() {
	list_split->set_visible(!list_split->is_visible());
	EditorSettings::get_singleton()->set_project_metadata("scripts_panel", "show_scripts_panel", list_split->is_visible());
	return list_split->is_visible();
}

bool ScriptEditor::is_scripts_panel_toggled() {
	return list_split->is_visible();
}

void ScriptEditor::_menu_option(int p_option) {
	ScriptEditorBase *current = _get_current_editor();
	switch (p_option) {
		case FILE_NEW: {
			script_create_dialog->config("Node", "new_script", false, false);
			script_create_dialog->popup_centered();
		} break;
		case FILE_NEW_TEXTFILE: {
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
			file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
			file_dialog_option = FILE_NEW_TEXTFILE;

			file_dialog->clear_filters();
			for (const String &E : textfile_extensions) {
				file_dialog->add_filter("*." + E, E.to_upper());
			}
			file_dialog->set_title(TTR("New Text File..."));
			file_dialog->popup_file_dialog();
			open_textfile_after_create = true;
		} break;
		case FILE_OPEN: {
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
			file_dialog_option = FILE_OPEN;

			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("Script", &extensions);
			file_dialog->clear_filters();
			for (const String &extension : extensions) {
				file_dialog->add_filter("*." + extension, extension.to_upper());
			}

			for (const String &E : textfile_extensions) {
				file_dialog->add_filter("*." + E, E.to_upper());
			}

			file_dialog->set_title(TTR("Open File"));
			file_dialog->popup_file_dialog();
			return;
		} break;
		case FILE_REOPEN_CLOSED: {
			if (previous_scripts.is_empty()) {
				return;
			}

			String path = previous_scripts.back()->get();
			previous_scripts.pop_back();

			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("Script", &extensions);
			ResourceLoader::get_recognized_extensions_for_type("JSON", &extensions);
			bool built_in = !path.is_resource_file();

			if (extensions.find(path.get_extension()) || built_in) {
				if (built_in) {
					String res_path = path.get_slice("::", 0);
					if (ResourceLoader::get_resource_type(res_path) == "PackedScene") {
						if (!EditorNode::get_singleton()->is_scene_open(res_path)) {
							EditorNode::get_singleton()->load_scene(res_path);
						}
					} else {
						EditorNode::get_singleton()->load_resource(res_path);
					}
				}

				Ref<Resource> scr = ResourceLoader::load(path);
				if (!scr.is_valid()) {
					EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + path, TTR("Error!"));
					file_dialog_option = -1;
					return;
				}

				edit(scr);
				file_dialog_option = -1;
			} else {
				Error error;
				Ref<TextFile> text_file = _load_text_file(path, &error);
				if (error != OK) {
					EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + path, TTR("Error!"));
				}

				if (text_file.is_valid()) {
					edit(text_file);
					file_dialog_option = -1;
				}
			}
		} break;
		case FILE_SAVE_ALL: {
			if (_test_script_times_on_disk()) {
				return;
			}

			save_all_scripts();
		} break;
		case SEARCH_IN_FILES: {
			_on_find_in_files_requested("");
		} break;
		case REPLACE_IN_FILES: {
			_on_replace_in_files_requested("");
		} break;
		case SEARCH_HELP: {
			help_search_dialog->popup_dialog();
		} break;
		case SEARCH_WEBSITE: {
			Control *tab = tab_container->get_current_tab_control();

			EditorHelp *eh = Object::cast_to<EditorHelp>(tab);
			bool native_class_doc = false;
			if (eh) {
				const HashMap<String, DocData::ClassDoc>::ConstIterator E = EditorHelp::get_doc_data()->class_list.find(eh->get_class());
				native_class_doc = E && !E->value.is_script_doc;
			}
			if (native_class_doc) {
				String name = eh->get_class().to_lower();
				String doc_url = vformat(VERSION_DOCS_URL "/classes/class_%s.html", name);
				OS::get_singleton()->shell_open(doc_url);
			} else {
				OS::get_singleton()->shell_open(VERSION_DOCS_URL "/");
			}
		} break;
		case WINDOW_NEXT: {
			_history_forward();
		} break;
		case WINDOW_PREV: {
			_history_back();
		} break;
		case WINDOW_SORT: {
			_sort_list_on_update = true;
			_update_script_names();
		} break;
		case TOGGLE_SCRIPTS_PANEL: {
			toggle_scripts_panel();
			if (current) {
				current->update_toggle_scripts_button();
			} else {
				Control *tab = tab_container->get_current_tab_control();
				EditorHelp *editor_help = Object::cast_to<EditorHelp>(tab);
				if (editor_help) {
					editor_help->update_toggle_scripts_button();
				}
			}
		}
	}

	if (p_option >= EditorContextMenuPlugin::BASE_ID) {
		Ref<Resource> resource;
		if (current) {
			resource = current->get_edited_resource();
		}
		EditorContextMenuPluginManager::get_singleton()->activate_custom_option(EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR, p_option, resource);
		return;
	}

	if (current) {
		switch (p_option) {
			case FILE_SAVE: {
				save_current_script();
			} break;
			case FILE_SAVE_AS: {
				if (trim_trailing_whitespace_on_save) {
					current->trim_trailing_whitespace();
				}

				if (trim_final_newlines_on_save) {
					current->trim_final_newlines();
				}

				if (convert_indent_on_save) {
					current->convert_indent();
				}

				Ref<Resource> resource = current->get_edited_resource();
				Ref<TextFile> text_file = resource;
				Ref<Script> scr = resource;

				if (text_file.is_valid()) {
					file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
					file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
					file_dialog_option = FILE_SAVE_AS;

					List<String> extensions;
					ResourceLoader::get_recognized_extensions_for_type("Script", &extensions);
					file_dialog->clear_filters();
					file_dialog->set_current_dir(text_file->get_path().get_base_dir());
					file_dialog->set_current_file(text_file->get_path().get_file());
					file_dialog->set_title(TTR("Save File As..."));
					file_dialog->popup_file_dialog();
					break;
				}

				if (scr.is_valid()) {
					clear_docs_from_script(scr);
				}

				EditorNode::get_singleton()->push_item(resource.ptr());
				EditorNode::get_singleton()->save_resource_as(resource);

				if (scr.is_valid()) {
					update_docs_from_script(scr);
				}
			} break;

			case FILE_TOOL_RELOAD_SOFT: {
				Ref<Script> scr = current->get_edited_resource();
				if (scr.is_null()) {
					EditorNode::get_singleton()->show_warning(TTR("Can't obtain the script for reloading."));
					break;
				}
				if (!scr->is_tool()) {
					EditorNode::get_singleton()->show_warning(TTR("Reload only takes effect on tool scripts."));
					return;
				}
				scr->reload(true);

			} break;

			case FILE_RUN: {
				Ref<Script> scr = current->get_edited_resource();
				if (scr.is_null()) {
					EditorToaster::get_singleton()->popup_str(TTR("Cannot run the edited file because it's not a script."), EditorToaster::SEVERITY_WARNING);
					break;
				}

				current->apply_code();

				Error err = scr->reload(true); // Always hard reload the script before running.
				if (err != OK || !scr->is_valid()) {
					EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it contains errors, check the output log."), EditorToaster::SEVERITY_WARNING);
					return;
				}

				// Perform additional checks on the script to evaluate if it's runnable.

				bool is_runnable = true;
				if (!ClassDB::is_parent_class(scr->get_instance_base_type(), "EditorScript")) {
					is_runnable = false;

					EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it doesn't extend EditorScript."), EditorToaster::SEVERITY_WARNING);
				}
				if (!scr->is_tool()) {
					is_runnable = false;

					if (scr->get_class() == "GDScript") {
						EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it's not a tool script (add the @tool annotation at the top)."), EditorToaster::SEVERITY_WARNING);
					} else {
						EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it's not a tool script."), EditorToaster::SEVERITY_WARNING);
					}
				}
				if (!is_runnable) {
					return;
				}

				Ref<EditorScript> es = memnew(EditorScript);
				es->set_script(scr);
				es->run();
			} break;

			case FILE_CLOSE: {
				if (current->is_unsaved()) {
					_ask_close_current_unsaved_tab(current);
				} else {
					_close_current_tab(false);
				}
			} break;
			case FILE_COPY_PATH: {
				_copy_script_path();
			} break;
			case SHOW_IN_FILE_SYSTEM: {
				const Ref<Resource> scr = current->get_edited_resource();
				String path = scr->get_path();
				if (!path.is_empty()) {
					if (scr->is_built_in()) {
						path = path.get_slice("::", 0); // Show the scene instead.
					}

					FileSystemDock::get_singleton()->navigate_to_path(path);
				}
			} break;
			case CLOSE_DOCS: {
				_close_docs_tab();
			} break;
			case CLOSE_OTHER_TABS: {
				_close_other_tabs();
			} break;
			case CLOSE_ALL: {
				_close_all_tabs();
			} break;
			case WINDOW_MOVE_UP: {
				if (tab_container->get_current_tab() > 0) {
					tab_container->move_child(current, tab_container->get_current_tab() - 1);
					tab_container->set_current_tab(tab_container->get_current_tab());
					_update_script_names();
				}
			} break;
			case WINDOW_MOVE_DOWN: {
				if (tab_container->get_current_tab() < tab_container->get_tab_count() - 1) {
					tab_container->move_child(current, tab_container->get_current_tab() + 1);
					tab_container->set_current_tab(tab_container->get_current_tab());
					_update_script_names();
				}
			} break;
			default: {
				if (p_option >= WINDOW_SELECT_BASE) {
					_go_to_tab(p_option - WINDOW_SELECT_BASE);
					_update_script_names();
				}
			}
		}
	} else {
		EditorHelp *help = Object::cast_to<EditorHelp>(tab_container->get_current_tab_control());
		if (help) {
			switch (p_option) {
				case HELP_SEARCH_FIND: {
					help->popup_search();
				} break;
				case HELP_SEARCH_FIND_NEXT: {
					help->search_again();
				} break;
				case HELP_SEARCH_FIND_PREVIOUS: {
					help->search_again(true);
				} break;
				case FILE_CLOSE: {
					_close_current_tab();
				} break;
				case CLOSE_DOCS: {
					_close_docs_tab();
				} break;
				case CLOSE_OTHER_TABS: {
					_close_other_tabs();
				} break;
				case CLOSE_ALL: {
					_close_all_tabs();
				} break;
				case WINDOW_MOVE_UP: {
					if (tab_container->get_current_tab() > 0) {
						tab_container->move_child(help, tab_container->get_current_tab() - 1);
						tab_container->set_current_tab(tab_container->get_current_tab());
						_update_script_names();
					}
				} break;
				case WINDOW_MOVE_DOWN: {
					if (tab_container->get_current_tab() < tab_container->get_tab_count() - 1) {
						tab_container->move_child(help, tab_container->get_current_tab() + 1);
						tab_container->set_current_tab(tab_container->get_current_tab());
						_update_script_names();
					}
				} break;
			}
		}
	}
}

void ScriptEditor::_theme_option(int p_option) {
	switch (p_option) {
		case THEME_IMPORT: {
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
			file_dialog_option = THEME_IMPORT;
			file_dialog->clear_filters();
			file_dialog->add_filter("*.tet");
			file_dialog->set_title(TTR("Import Theme"));
			file_dialog->popup_file_dialog();
		} break;
		case THEME_RELOAD: {
			EditorSettings::get_singleton()->load_text_editor_theme();
		} break;
		case THEME_SAVE: {
			if (EditorSettings::get_singleton()->is_default_text_editor_theme()) {
				ScriptEditor::_show_save_theme_as_dialog();
			} else if (!EditorSettings::get_singleton()->save_text_editor_theme()) {
				EditorNode::get_singleton()->show_warning(TTR("Error while saving theme"), TTR("Error saving"));
			}
		} break;
		case THEME_SAVE_AS: {
			ScriptEditor::_show_save_theme_as_dialog();
		} break;
	}
}

void ScriptEditor::_show_save_theme_as_dialog() {
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_dialog_option = THEME_SAVE_AS;
	file_dialog->clear_filters();
	file_dialog->add_filter("*.tet");
	file_dialog->set_current_path(EditorPaths::get_singleton()->get_text_editor_themes_dir().path_join(EDITOR_GET("text_editor/theme/color_theme")));
	file_dialog->set_title(TTR("Save Theme As..."));
	file_dialog->popup_file_dialog();
}

bool ScriptEditor::_has_docs_tab() const {
	const int child_count = tab_container->get_tab_count();
	for (int i = 0; i < child_count; i++) {
		if (Object::cast_to<EditorHelp>(tab_container->get_tab_control(i))) {
			return true;
		}
	}
	return false;
}

bool ScriptEditor::_has_script_tab() const {
	const int child_count = tab_container->get_tab_count();
	for (int i = 0; i < child_count; i++) {
		if (Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i))) {
			return true;
		}
	}
	return false;
}

void ScriptEditor::_prepare_file_menu() {
	PopupMenu *menu = file_menu->get_popup();
	const bool current_is_doc = _get_current_editor() == nullptr;

	menu->set_item_disabled(menu->get_item_index(FILE_REOPEN_CLOSED), previous_scripts.is_empty());

	menu->set_item_disabled(menu->get_item_index(FILE_SAVE), current_is_doc);
	menu->set_item_disabled(menu->get_item_index(FILE_SAVE_AS), current_is_doc);
	menu->set_item_disabled(menu->get_item_index(FILE_SAVE_ALL), !_has_script_tab());

	menu->set_item_disabled(menu->get_item_index(FILE_TOOL_RELOAD_SOFT), current_is_doc);
	menu->set_item_disabled(menu->get_item_index(FILE_COPY_PATH), current_is_doc);
	menu->set_item_disabled(menu->get_item_index(SHOW_IN_FILE_SYSTEM), current_is_doc);

	menu->set_item_disabled(menu->get_item_index(WINDOW_PREV), history_pos <= 0);
	menu->set_item_disabled(menu->get_item_index(WINDOW_NEXT), history_pos >= history.size() - 1);

	menu->set_item_disabled(menu->get_item_index(FILE_CLOSE), tab_container->get_tab_count() < 1);
	menu->set_item_disabled(menu->get_item_index(CLOSE_ALL), tab_container->get_tab_count() < 1);
	menu->set_item_disabled(menu->get_item_index(CLOSE_OTHER_TABS), tab_container->get_tab_count() <= 1);
	menu->set_item_disabled(menu->get_item_index(CLOSE_DOCS), !_has_docs_tab());

	menu->set_item_disabled(menu->get_item_index(FILE_RUN), current_is_doc);
}

void ScriptEditor::_file_menu_closed() {
	PopupMenu *menu = file_menu->get_popup();

	menu->set_item_disabled(menu->get_item_index(FILE_REOPEN_CLOSED), false);

	menu->set_item_disabled(menu->get_item_index(FILE_SAVE), false);
	menu->set_item_disabled(menu->get_item_index(FILE_SAVE_AS), false);
	menu->set_item_disabled(menu->get_item_index(FILE_SAVE_ALL), false);

	menu->set_item_disabled(menu->get_item_index(FILE_TOOL_RELOAD_SOFT), false);
	menu->set_item_disabled(menu->get_item_index(FILE_COPY_PATH), false);
	menu->set_item_disabled(menu->get_item_index(SHOW_IN_FILE_SYSTEM), false);

	menu->set_item_disabled(menu->get_item_index(WINDOW_PREV), false);
	menu->set_item_disabled(menu->get_item_index(WINDOW_NEXT), false);

	menu->set_item_disabled(menu->get_item_index(FILE_CLOSE), false);
	menu->set_item_disabled(menu->get_item_index(CLOSE_ALL), false);
	menu->set_item_disabled(menu->get_item_index(CLOSE_OTHER_TABS), false);
	menu->set_item_disabled(menu->get_item_index(CLOSE_DOCS), false);

	menu->set_item_disabled(menu->get_item_index(FILE_RUN), false);
}

void ScriptEditor::_tab_changed(int p_which) {
	ensure_select_current();
}

void ScriptEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			EditorRunBar::get_singleton()->connect("stop_pressed", callable_mp(this, &ScriptEditor::_editor_stop));
			_apply_editor_settings();
			[[fallthrough]];
		}

		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			tab_container->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("ScriptEditor"), EditorStringName(EditorStyles)));

			help_search->set_icon(get_editor_theme_icon(SNAME("HelpSearch")));
			site_search->set_icon(get_editor_theme_icon(SNAME("ExternalLink")));

			if (is_layout_rtl()) {
				script_forward->set_icon(get_editor_theme_icon(SNAME("Back")));
				script_back->set_icon(get_editor_theme_icon(SNAME("Forward")));
			} else {
				script_forward->set_icon(get_editor_theme_icon(SNAME("Forward")));
				script_back->set_icon(get_editor_theme_icon(SNAME("Back")));
			}

			members_overview_alphabeta_sort_button->set_icon(get_editor_theme_icon(SNAME("Sort")));

			filter_scripts->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			filter_methods->set_right_icon(get_editor_theme_icon(SNAME("Search")));

			filename->add_theme_style_override(CoreStringName(normal), get_theme_stylebox(CoreStringName(normal), SNAME("LineEdit")));

			recent_scripts->reset_size();

			if (is_inside_tree()) {
				_update_script_names();
			}
		} break;

		case NOTIFICATION_READY: {
			// Can't set own styles in NOTIFICATION_THEME_CHANGED, so for now this will do.
			add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("ScriptEditorPanel"), EditorStringName(EditorStyles)));

			get_tree()->connect("tree_changed", callable_mp(this, &ScriptEditor::_tree_changed));
			InspectorDock::get_singleton()->connect("request_help", callable_mp(this, &ScriptEditor::_help_class_open));
			EditorNode::get_singleton()->connect("request_help_search", callable_mp(this, &ScriptEditor::_help_search));
			EditorNode::get_singleton()->connect("scene_closed", callable_mp(this, &ScriptEditor::_close_builtin_scripts_from_scene));
			EditorNode::get_singleton()->connect("script_add_function_request", callable_mp(this, &ScriptEditor::_add_callback));
			EditorNode::get_singleton()->connect("resource_saved", callable_mp(this, &ScriptEditor::_res_saved_callback));
			EditorNode::get_singleton()->connect("scene_saved", callable_mp(this, &ScriptEditor::_scene_saved_callback));
			FileSystemDock::get_singleton()->connect("files_moved", callable_mp(this, &ScriptEditor::_files_moved));
			FileSystemDock::get_singleton()->connect("file_removed", callable_mp(this, &ScriptEditor::_file_removed));
			script_list->connect(SceneStringName(item_selected), callable_mp(this, &ScriptEditor::_script_selected));

			members_overview->connect(SceneStringName(item_selected), callable_mp(this, &ScriptEditor::_members_overview_selected));
			help_overview->connect(SceneStringName(item_selected), callable_mp(this, &ScriptEditor::_help_overview_selected));
			script_split->connect("dragged", callable_mp(this, &ScriptEditor::_split_dragged));
			list_split->connect("dragged", callable_mp(this, &ScriptEditor::_split_dragged));

			EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &ScriptEditor::_editor_settings_changed));
			EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &ScriptEditor::_filesystem_changed));
		} break;

		case NOTIFICATION_EXIT_TREE: {
			EditorRunBar::get_singleton()->disconnect("stop_pressed", callable_mp(this, &ScriptEditor::_editor_stop));
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_IN: {
			_test_script_times_on_disk();
			_update_modified_scripts_for_external_editor();
		} break;
	}
}

bool ScriptEditor::can_take_away_focus() const {
	ScriptEditorBase *current = _get_current_editor();
	if (current) {
		return current->can_lose_focus_on_node_selection();
	} else {
		return true;
	}
}

void ScriptEditor::_close_builtin_scripts_from_scene(const String &p_scene) {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));

		if (se) {
			Ref<Script> scr = se->get_edited_resource();
			if (scr.is_null()) {
				continue;
			}

			if (scr->is_built_in() && scr->get_path().begins_with(p_scene)) { // Is an internal script and belongs to scene being closed.
				_close_tab(i, false);
				i--;
			}
		}
	}
}

void ScriptEditor::edited_scene_changed() {
	_update_modified_scripts_for_external_editor();
}

void ScriptEditor::notify_script_close(const Ref<Script> &p_script) {
	emit_signal(SNAME("script_close"), p_script);
}

void ScriptEditor::notify_script_changed(const Ref<Script> &p_script) {
	emit_signal(SNAME("editor_script_changed"), p_script);
}

Vector<String> ScriptEditor::_get_breakpoints() {
	Vector<String> ret;
	HashSet<String> loaded_scripts;
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		Ref<Script> scr = se->get_edited_resource();
		if (scr.is_null()) {
			continue;
		}

		String base = scr->get_path();
		loaded_scripts.insert(base);
		if (base.is_empty() || base.begins_with("local://")) {
			continue;
		}

		PackedInt32Array bpoints = se->get_breakpoints();
		for (int32_t bpoint : bpoints) {
			ret.push_back(base + ":" + itos((int)bpoint + 1));
		}
	}

	// Load breakpoints that are in closed scripts.
	List<String> cached_editors;
	script_editor_cache->get_sections(&cached_editors);
	for (const String &E : cached_editors) {
		if (loaded_scripts.has(E)) {
			continue;
		}

		Array breakpoints = _get_cached_breakpoints_for_script(E);
		for (int breakpoint : breakpoints) {
			ret.push_back(E + ":" + itos((int)breakpoint + 1));
		}
	}
	return ret;
}

void ScriptEditor::get_breakpoints(List<String> *p_breakpoints) {
	HashSet<String> loaded_scripts;
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		Ref<Script> scr = se->get_edited_resource();
		if (scr.is_null()) {
			continue;
		}

		String base = scr->get_path();
		loaded_scripts.insert(base);
		if (base.is_empty() || base.begins_with("local://")) {
			continue;
		}

		PackedInt32Array bpoints = se->get_breakpoints();
		for (int32_t bpoint : bpoints) {
			p_breakpoints->push_back(base + ":" + itos((int)bpoint + 1));
		}
	}

	// Load breakpoints that are in closed scripts.
	List<String> cached_editors;
	script_editor_cache->get_sections(&cached_editors);
	for (const String &E : cached_editors) {
		if (loaded_scripts.has(E)) {
			continue;
		}

		Array breakpoints = _get_cached_breakpoints_for_script(E);
		for (int breakpoint : breakpoints) {
			p_breakpoints->push_back(E + ":" + itos((int)breakpoint + 1));
		}
	}
}

void ScriptEditor::_members_overview_selected(int p_idx) {
	int line = members_overview->get_item_metadata(p_idx);
	ScriptEditorBase *current = _get_current_editor();
	if (ScriptTextEditor *script_text_editor = Object::cast_to<ScriptTextEditor>(current)) {
		script_text_editor->goto_line_centered(line);
	} else if (current) {
		current->goto_line(line);
	}
}

void ScriptEditor::_help_overview_selected(int p_idx) {
	Node *current = tab_container->get_tab_control(tab_container->get_current_tab());
	EditorHelp *se = Object::cast_to<EditorHelp>(current);
	if (!se) {
		return;
	}
	se->scroll_to_section(help_overview->get_item_metadata(p_idx));
}

void ScriptEditor::_script_selected(int p_idx) {
	grab_focus_block = !Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT); //amazing hack, simply amazing

	_go_to_tab(script_list->get_item_metadata(p_idx));
	grab_focus_block = false;
}

void ScriptEditor::ensure_select_current() {
	if (tab_container->get_tab_count() && tab_container->get_current_tab() >= 0) {
		ScriptEditorBase *se = _get_current_editor();
		if (se) {
			se->enable_editor(this);

			if (!grab_focus_block && is_visible_in_tree()) {
				se->ensure_focus();
			}
		}
	}
	_update_find_replace_bar();

	_update_selected_editor_menu();
}

bool ScriptEditor::is_editor_floating() {
	return is_floating;
}

void ScriptEditor::_find_scripts(Node *p_base, Node *p_current, HashSet<Ref<Script>> &used) {
	if (p_current != p_base && p_current->get_owner() != p_base) {
		return;
	}

	if (p_current->get_script_instance()) {
		Ref<Script> scr = p_current->get_script();
		if (scr.is_valid()) {
			used.insert(scr);
		}
	}

	for (int i = 0; i < p_current->get_child_count(); i++) {
		_find_scripts(p_base, p_current->get_child(i), used);
	}
}

struct _ScriptEditorItemData {
	String name;
	String sort_key;
	Ref<Texture2D> icon;
	bool tool = false;
	int index = 0;
	String tooltip;
	bool used = false;
	int category = 0;
	Node *ref = nullptr;

	bool operator<(const _ScriptEditorItemData &id) const {
		if (category == id.category) {
			if (sort_key == id.sort_key) {
				return index < id.index;
			} else {
				return sort_key.filenocasecmp_to(id.sort_key) < 0;
			}
		} else {
			return category < id.category;
		}
	}
};

void ScriptEditor::_update_members_overview_visibility() {
	ScriptEditorBase *se = _get_current_editor();
	if (!se) {
		members_overview_alphabeta_sort_button->set_visible(false);
		members_overview->set_visible(false);
		overview_vbox->set_visible(false);
		return;
	}

	if (members_overview_enabled && se->show_members_overview()) {
		members_overview_alphabeta_sort_button->set_visible(true);
		filter_methods->set_visible(true);
		members_overview->set_visible(true);
		overview_vbox->set_visible(true);
	} else {
		members_overview_alphabeta_sort_button->set_visible(false);
		filter_methods->set_visible(false);
		members_overview->set_visible(false);
		overview_vbox->set_visible(false);
	}
}

void ScriptEditor::_toggle_members_overview_alpha_sort(bool p_alphabetic_sort) {
	EditorSettings::get_singleton()->set("text_editor/script_list/sort_members_outline_alphabetically", p_alphabetic_sort);
	_update_members_overview();
}

void ScriptEditor::_update_members_overview() {
	members_overview->clear();

	ScriptEditorBase *se = _get_current_editor();
	if (!se) {
		return;
	}

	Vector<String> functions = se->get_functions();
	if (EDITOR_GET("text_editor/script_list/sort_members_outline_alphabetically")) {
		functions.sort();
	}

	for (int i = 0; i < functions.size(); i++) {
		String filter = filter_methods->get_text();
		String name = functions[i].get_slice(":", 0);
		if (filter.is_empty() || filter.is_subsequence_ofn(name)) {
			members_overview->add_item(name);
			members_overview->set_item_metadata(-1, functions[i].get_slice(":", 1).to_int() - 1);
		}
	}

	String path = se->get_edited_resource()->get_path();
	bool built_in = !path.is_resource_file();
	String name = built_in ? path.get_file() : se->get_name();
	filename->set_text(name);
}

void ScriptEditor::_update_help_overview_visibility() {
	int selected = tab_container->get_current_tab();
	if (selected < 0 || selected >= tab_container->get_tab_count()) {
		help_overview->set_visible(false);
		return;
	}

	Node *current = tab_container->get_tab_control(tab_container->get_current_tab());
	EditorHelp *se = Object::cast_to<EditorHelp>(current);
	if (!se) {
		help_overview->set_visible(false);
		return;
	}

	if (help_overview_enabled) {
		members_overview_alphabeta_sort_button->set_visible(false);
		filter_methods->set_visible(false);
		help_overview->set_visible(true);
		overview_vbox->set_visible(true);
		filename->set_text(se->get_name());
	} else {
		help_overview->set_visible(false);
		overview_vbox->set_visible(false);
	}
}

void ScriptEditor::_update_help_overview() {
	help_overview->clear();

	int selected = tab_container->get_current_tab();
	if (selected < 0 || selected >= tab_container->get_tab_count()) {
		return;
	}

	Node *current = tab_container->get_tab_control(tab_container->get_current_tab());
	EditorHelp *se = Object::cast_to<EditorHelp>(current);
	if (!se) {
		return;
	}

	Vector<Pair<String, int>> sections = se->get_sections();
	for (int i = 0; i < sections.size(); i++) {
		help_overview->add_item(sections[i].first);
		help_overview->set_item_metadata(i, sections[i].second);
	}
}

void ScriptEditor::_update_online_doc() {
	Node *current = tab_container->get_tab_control(tab_container->get_current_tab());

	EditorHelp *eh = Object::cast_to<EditorHelp>(current);
	bool native_class_doc = false;
	if (eh) {
		const HashMap<String, DocData::ClassDoc>::ConstIterator E = EditorHelp::get_doc_data()->class_list.find(eh->get_class());
		native_class_doc = E && !E->value.is_script_doc;
	}
	if (native_class_doc) {
		String name = eh->get_class();
		String tooltip = vformat(TTR("Open '%s' in Godot online documentation."), name);
		site_search->set_text(TTR("Open in Online Docs"));
		site_search->set_tooltip_text(tooltip);
	} else {
		site_search->set_text(TTR("Online Docs"));
		site_search->set_tooltip_text(TTR("Open Godot online documentation."));
	}
}

void ScriptEditor::_update_script_colors() {
	bool script_temperature_enabled = EDITOR_GET("text_editor/script_list/script_temperature_enabled");

	int hist_size = EDITOR_GET("text_editor/script_list/script_temperature_history_size");
	Color hot_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	hot_color.set_s(hot_color.get_s() * 0.9);
	Color cold_color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor));

	for (int i = 0; i < script_list->get_item_count(); i++) {
		int c = script_list->get_item_metadata(i);
		Node *n = tab_container->get_tab_control(c);
		if (!n) {
			continue;
		}

		script_list->set_item_custom_bg_color(i, Color(0, 0, 0, 0));

		if (script_temperature_enabled) {
			int pass = n->get_meta("__editor_pass", -1);
			if (pass < 0) {
				continue;
			}

			int h = edit_pass - pass;
			if (h > hist_size) {
				continue;
			}
			int non_zero_hist_size = (hist_size == 0) ? 1 : hist_size;
			float v = Math::ease((edit_pass - pass) / float(non_zero_hist_size), 0.4);

			script_list->set_item_custom_fg_color(i, hot_color.lerp(cold_color, v));
		}
	}
}

void ScriptEditor::_update_script_names() {
	if (restoring_layout) {
		return;
	}

	HashSet<Ref<Script>> used;
	Node *edited = EditorNode::get_singleton()->get_edited_scene();
	if (edited) {
		_find_scripts(edited, edited, used);
	}

	script_list->clear();
	bool split_script_help = EDITOR_GET("text_editor/script_list/group_help_pages");
	ScriptSortBy sort_by = (ScriptSortBy)(int)EDITOR_GET("text_editor/script_list/sort_scripts_by");
	ScriptListName display_as = (ScriptListName)(int)EDITOR_GET("text_editor/script_list/list_script_names_as");

	Vector<_ScriptEditorItemData> sedata;

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (se) {
			Ref<Texture2D> icon = se->get_theme_icon();
			String path = se->get_edited_resource()->get_path();
			bool saved = !path.is_empty();
			if (saved) {
				// The script might be deleted, moved, or renamed, so make sure
				// to update original path to previously edited resource.
				se->set_meta("_edit_res_path", path);
			}
			String name = se->get_name();
			Ref<Script> scr = se->get_edited_resource();

			_ScriptEditorItemData sd;
			sd.icon = icon;
			sd.name = name;
			sd.tooltip = saved ? path : TTR("Unsaved file.");
			sd.index = i;
			sd.used = used.has(se->get_edited_resource());
			sd.category = 0;
			sd.ref = se;
			if (scr.is_valid()) {
				sd.tool = scr->is_tool();
			}

			switch (sort_by) {
				case SORT_BY_NAME: {
					sd.sort_key = name.to_lower();
				} break;
				case SORT_BY_PATH: {
					sd.sort_key = path;
				} break;
				case SORT_BY_NONE: {
					sd.sort_key = "";
				} break;
			}

			switch (display_as) {
				case DISPLAY_NAME: {
					sd.name = name;
				} break;
				case DISPLAY_DIR_AND_NAME: {
					if (!path.get_base_dir().get_file().is_empty()) {
						sd.name = path.get_base_dir().get_file().path_join(name);
					} else {
						sd.name = name;
					}
				} break;
				case DISPLAY_FULL_PATH: {
					sd.name = path;
				} break;
			}
			if (!saved) {
				sd.name = se->get_name();
			}

			sedata.push_back(sd);
		}

		Vector<String> disambiguated_script_names;
		Vector<String> full_script_paths;
		for (int j = 0; j < sedata.size(); j++) {
			String name = sedata[j].name.replace("(*)", "");
			ScriptListName script_display = (ScriptListName)(int)EDITOR_GET("text_editor/script_list/list_script_names_as");
			switch (script_display) {
				case DISPLAY_NAME: {
					name = name.get_file();
				} break;
				case DISPLAY_DIR_AND_NAME: {
					name = name.get_base_dir().get_file().path_join(name.get_file());
				} break;
				default:
					break;
			}

			disambiguated_script_names.append(name);
			full_script_paths.append(sedata[j].tooltip);
		}

		EditorNode::disambiguate_filenames(full_script_paths, disambiguated_script_names);

		for (int j = 0; j < sedata.size(); j++) {
			if (sedata[j].name.ends_with("(*)")) {
				sedata.write[j].name = disambiguated_script_names[j] + "(*)";
			} else {
				sedata.write[j].name = disambiguated_script_names[j];
			}
		}

		EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));
		if (eh) {
			String name = eh->get_class();
			Ref<Texture2D> icon = get_editor_theme_icon(SNAME("Help"));
			String tooltip = vformat(TTR("%s Class Reference"), name);

			_ScriptEditorItemData sd;
			sd.icon = icon;
			sd.name = name;
			sd.sort_key = name.to_lower();
			sd.tooltip = tooltip;
			sd.index = i;
			sd.used = false;
			sd.category = split_script_help ? 1 : 0;
			sd.ref = eh;

			sedata.push_back(sd);
		}
	}

	if (_sort_list_on_update && !sedata.is_empty()) {
		sedata.sort();

		// change actual order of tab_container so that the order can be rearranged by user
		int cur_tab = tab_container->get_current_tab();
		int prev_tab = tab_container->get_previous_tab();
		int new_cur_tab = -1;
		int new_prev_tab = -1;
		for (int i = 0; i < sedata.size(); i++) {
			tab_container->move_child(sedata[i].ref, i);
			if (new_prev_tab == -1 && sedata[i].index == prev_tab) {
				new_prev_tab = i;
			}
			if (new_cur_tab == -1 && sedata[i].index == cur_tab) {
				new_cur_tab = i;
			}
			// Update index of sd entries for sorted order
			_ScriptEditorItemData sd = sedata[i];
			sd.index = i;
			sedata.set(i, sd);
		}

		lock_history = true;
		_go_to_tab(new_prev_tab);
		_go_to_tab(new_cur_tab);
		lock_history = false;
		_sort_list_on_update = false;
	}

	Vector<_ScriptEditorItemData> sedata_filtered;
	for (int i = 0; i < sedata.size(); i++) {
		String filter = filter_scripts->get_text();
		if (filter.is_empty() || filter.is_subsequence_ofn(sedata[i].name)) {
			sedata_filtered.push_back(sedata[i]);
		}
	}

	Color tool_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	tool_color.set_s(tool_color.get_s() * 1.5);
	for (int i = 0; i < sedata_filtered.size(); i++) {
		script_list->add_item(sedata_filtered[i].name, sedata_filtered[i].icon);
		if (sedata_filtered[i].tool) {
			script_list->set_item_icon_modulate(-1, tool_color);
		}

		int index = script_list->get_item_count() - 1;
		script_list->set_item_tooltip(index, sedata_filtered[i].tooltip);
		script_list->set_item_metadata(index, sedata_filtered[i].index); /* Saving as metadata the script's index in the tab container and not the filtered one */
		if (sedata_filtered[i].used) {
			script_list->set_item_custom_bg_color(index, Color(88 / 255.0, 88 / 255.0, 60 / 255.0));
		}
		if (tab_container->get_current_tab() == sedata_filtered[i].index) {
			script_list->select(index);

			script_name_label->set_text(sedata_filtered[i].name);
			script_icon->set_texture(sedata_filtered[i].icon);

			ScriptEditorBase *se = _get_current_editor();
			if (se) {
				se->enable_editor(this);
				_update_selected_editor_menu();
			}
		}
	}

	if (!waiting_update_names) {
		_update_members_overview();
		_update_help_overview();
	} else {
		waiting_update_names = false;
	}
	_update_members_overview_visibility();
	_update_help_overview_visibility();
	_update_script_colors();
}

Ref<TextFile> ScriptEditor::_load_text_file(const String &p_path, Error *r_error) const {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	String path = ResourceLoader::path_remap(local_path);

	TextFile *text_file = memnew(TextFile);
	Ref<TextFile> text_res(text_file);
	Error err = text_file->load_text(path);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Cannot load text file '" + path + "'.");

	text_file->set_file_path(local_path);
	text_file->set_path(local_path, true);

	if (ResourceLoader::get_timestamp_on_load()) {
		text_file->set_last_modified_time(FileAccess::get_modified_time(path));
	}

	if (r_error) {
		*r_error = OK;
	}

	return text_res;
}

Error ScriptEditor::_save_text_file(Ref<TextFile> p_text_file, const String &p_path) {
	Ref<TextFile> sqscr = p_text_file;
	ERR_FAIL_COND_V(sqscr.is_null(), ERR_INVALID_PARAMETER);

	String source = sqscr->get_text();

	Error err;
	{
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);

		ERR_FAIL_COND_V_MSG(err, err, "Cannot save text file '" + p_path + "'.");

		file->store_string(source);
		if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
			return ERR_CANT_CREATE;
		}
	}

	if (ResourceSaver::get_timestamp_on_save()) {
		p_text_file->set_last_modified_time(FileAccess::get_modified_time(p_path));
	}

	EditorFileSystem::get_singleton()->update_file(p_path);

	_res_saved_callback(sqscr);
	return OK;
}

bool ScriptEditor::edit(const Ref<Resource> &p_resource, int p_line, int p_col, bool p_grab_focus) {
	if (p_resource.is_null()) {
		return false;
	}

	Ref<Script> scr = p_resource;

	// Don't open dominant script if using an external editor.
	bool use_external_editor =
			external_editor_active ||
			(scr.is_valid() && scr->get_language()->overrides_external_editor());
	use_external_editor = use_external_editor && !(scr.is_valid() && scr->is_built_in()); // Ignore external editor for built-in scripts.
	const bool open_dominant = EDITOR_GET("text_editor/behavior/files/open_dominant_script_on_scene_change");

	const bool should_open = (open_dominant && !use_external_editor) || !EditorNode::get_singleton()->is_changing_scene();

	if (scr.is_valid() && scr->get_language()->overrides_external_editor()) {
		if (should_open) {
			Error err = scr->get_language()->open_in_external_editor(scr, p_line >= 0 ? p_line : 0, p_col);
			if (err != OK) {
				ERR_PRINT("Couldn't open script in the overridden external text editor");
			}
		}
		return false;
	}

	if (use_external_editor &&
			(EditorDebuggerNode::get_singleton()->get_dump_stack_script() != p_resource || EditorDebuggerNode::get_singleton()->get_debug_with_external_editor()) &&
			p_resource->get_path().is_resource_file()) {
		String path = EDITOR_GET("text_editor/external/exec_path");
		String flags = EDITOR_GET("text_editor/external/exec_flags");

		List<String> args;
		bool has_file_flag = false;
		String script_path = ProjectSettings::get_singleton()->globalize_path(p_resource->get_path());

		if (flags.size()) {
			String project_path = ProjectSettings::get_singleton()->get_resource_path();

			flags = flags.replacen("{line}", itos(p_line > 0 ? p_line : 0));
			flags = flags.replacen("{col}", itos(p_col));
			flags = flags.strip_edges().replace("\\\\", "\\");

			int from = 0;
			int num_chars = 0;
			bool inside_quotes = false;

			for (int i = 0; i < flags.size(); i++) {
				if (flags[i] == '"' && (!i || flags[i - 1] != '\\')) {
					if (!inside_quotes) {
						from++;
					}
					inside_quotes = !inside_quotes;

				} else if (flags[i] == '\0' || (!inside_quotes && flags[i] == ' ')) {
					String arg = flags.substr(from, num_chars);
					if (arg.contains("{file}")) {
						has_file_flag = true;
					}

					// do path replacement here, else there will be issues with spaces and quotes
					arg = arg.replacen("{project}", project_path);
					arg = arg.replacen("{file}", script_path);
					args.push_back(arg);

					from = i + 1;
					num_chars = 0;
				} else {
					num_chars++;
				}
			}
		}

		// Default to passing script path if no {file} flag is specified.
		if (!has_file_flag) {
			args.push_back(script_path);
		}

		if (!path.is_empty()) {
			Error err = OS::get_singleton()->create_process(path, args);
			if (err == OK) {
				return false;
			}
		}

		ERR_PRINT("Couldn't open external text editor, falling back to the internal editor. Review your `text_editor/external/` editor settings.");
	}

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		if ((scr.is_valid() && se->get_edited_resource() == p_resource) || se->get_edited_resource()->get_path() == p_resource->get_path()) {
			if (should_open) {
				se->enable_editor(this);

				if (tab_container->get_current_tab() != i) {
					_go_to_tab(i);
				}
				if (is_visible_in_tree()) {
					se->ensure_focus();
				}

				if (p_line > 0) {
					se->goto_line(p_line);
				}
			}
			_update_script_names();
			script_list->ensure_current_is_visible();
			return true;
		}
	}

	// doesn't have it, make a new one

	ScriptEditorBase *se = nullptr;

	for (int i = script_editor_func_count - 1; i >= 0; i--) {
		se = script_editor_funcs[i](p_resource);
		if (se) {
			break;
		}
	}
	ERR_FAIL_NULL_V(se, false);

	se->set_edited_resource(p_resource);

	// Syntax highlighting.
	bool highlighter_set = false;
	for (int i = 0; i < syntax_highlighters.size(); i++) {
		Ref<EditorSyntaxHighlighter> highlighter = syntax_highlighters[i]->_create();
		if (highlighter.is_null()) {
			continue;
		}
		se->add_syntax_highlighter(highlighter);

		if (highlighter_set) {
			continue;
		}

		PackedStringArray languages = highlighter->_get_supported_languages();
		// If script try language, else use extension.
		if (scr.is_valid()) {
			if (languages.has(scr->get_language()->get_name())) {
				se->set_syntax_highlighter(highlighter);
				highlighter_set = true;
			}
			continue;
		}

		if (languages.has(p_resource->get_path().get_extension())) {
			se->set_syntax_highlighter(highlighter);
			highlighter_set = true;
		}
	}

	tab_container->add_child(se);

	if (p_grab_focus) {
		se->enable_editor(this);
	}

	// If we delete a script within the filesystem, the original resource path
	// is lost, so keep it as metadata to figure out the exact tab to delete.
	se->set_meta("_edit_res_path", p_resource->get_path());
	se->set_tooltip_request_func(callable_mp(this, &ScriptEditor::_get_debug_tooltip));
	if (se->get_edit_menu()) {
		se->get_edit_menu()->hide();
		menu_hb->add_child(se->get_edit_menu());
		menu_hb->move_child(se->get_edit_menu(), 1);
	}

	if (p_grab_focus) {
		_go_to_tab(tab_container->get_tab_count() - 1);
		_add_recent_script(p_resource->get_path());
	}

	if (script_editor_cache->has_section(p_resource->get_path())) {
		se->set_edit_state(script_editor_cache->get_value(p_resource->get_path(), "state"));
		ScriptTextEditor *ste = Object::cast_to<ScriptTextEditor>(se);
		if (ste) {
			ste->store_previous_state();
		}
	}

	_sort_list_on_update = true;
	_update_script_names();
	_save_layout();
	se->connect("name_changed", callable_mp(this, &ScriptEditor::_update_script_names));
	se->connect("edited_script_changed", callable_mp(this, &ScriptEditor::_script_changed));
	se->connect("request_help", callable_mp(this, &ScriptEditor::_help_search));
	se->connect("request_open_script_at_line", callable_mp(this, &ScriptEditor::_goto_script_line));
	se->connect("go_to_help", callable_mp(this, &ScriptEditor::_help_class_goto));
	se->connect("request_save_history", callable_mp(this, &ScriptEditor::_save_history));
	se->connect("request_save_previous_state", callable_mp(this, &ScriptEditor::_save_previous_state));
	se->connect("search_in_files_requested", callable_mp(this, &ScriptEditor::_on_find_in_files_requested));
	se->connect("replace_in_files_requested", callable_mp(this, &ScriptEditor::_on_replace_in_files_requested));
	se->connect("go_to_method", callable_mp(this, &ScriptEditor::script_goto_method));

	CodeTextEditor *cte = se->get_code_editor();
	if (cte) {
		cte->set_zoom_factor(zoom_factor);
		cte->connect("zoomed", callable_mp(this, &ScriptEditor::_set_zoom_factor));
	}

	//test for modification, maybe the script was not edited but was loaded

	_test_script_times_on_disk(p_resource);
	_update_modified_scripts_for_external_editor(p_resource);

	if (p_line >= 0) {
		se->goto_line(p_line);
	}

	notify_script_changed(p_resource);
	return true;
}

PackedStringArray ScriptEditor::get_unsaved_scripts() const {
	PackedStringArray unsaved_list;

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (se && se->is_unsaved()) {
			unsaved_list.append(se->get_name());
		}
	}
	return unsaved_list;
}

void ScriptEditor::save_current_script() {
	ScriptEditorBase *current = _get_current_editor();
	if (!current || _test_script_times_on_disk()) {
		return;
	}

	if (trim_trailing_whitespace_on_save) {
		current->trim_trailing_whitespace();
	}

	if (trim_final_newlines_on_save) {
		current->trim_final_newlines();
	}

	if (convert_indent_on_save) {
		current->convert_indent();
	}

	Ref<Resource> resource = current->get_edited_resource();
	Ref<TextFile> text_file = resource;
	Ref<Script> scr = resource;

	if (text_file.is_valid()) {
		current->apply_code();
		_save_text_file(text_file, text_file->get_path());
		return;
	}

	if (scr.is_valid()) {
		clear_docs_from_script(scr);
	}

	EditorNode::get_singleton()->save_resource(resource);

	if (scr.is_valid()) {
		update_docs_from_script(scr);
	}
}

void ScriptEditor::save_all_scripts() {
	HashSet<String> scenes_to_save;

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		if (convert_indent_on_save) {
			se->convert_indent();
		}

		if (trim_trailing_whitespace_on_save) {
			se->trim_trailing_whitespace();
		}

		if (trim_final_newlines_on_save) {
			se->trim_final_newlines();
		}

		if (!se->is_unsaved()) {
			continue;
		}

		Ref<Resource> edited_res = se->get_edited_resource();
		if (edited_res.is_valid()) {
			se->apply_code();
		}

		if (!edited_res->is_built_in()) {
			Ref<TextFile> text_file = edited_res;
			Ref<Script> scr = edited_res;

			if (text_file.is_valid()) {
				_save_text_file(text_file, text_file->get_path());
				continue;
			}

			if (scr.is_valid()) {
				clear_docs_from_script(scr);
			}

			EditorNode::get_singleton()->save_resource(edited_res); //external script, save it

			if (scr.is_valid()) {
				update_docs_from_script(scr);
			}
		} else {
			// For built-in scripts, save their scenes instead.
			const String scene_path = edited_res->get_path().get_slice("::", 0);
			if (!scene_path.is_empty() && !scenes_to_save.has(scene_path)) {
				scenes_to_save.insert(scene_path);
			}
		}
	}

	if (!scenes_to_save.is_empty()) {
		EditorNode::get_singleton()->save_scene_list(scenes_to_save);
	}

	_update_script_names();
}

void ScriptEditor::apply_scripts() const {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}
		se->insert_final_newline();
		se->apply_code();
	}
}

void ScriptEditor::reload_scripts(bool p_refresh_only) {
	// Call deferred to make sure it runs on the main thread.
	callable_mp(this, &ScriptEditor::_reload_scripts).call_deferred(p_refresh_only);
}

void ScriptEditor::_reload_scripts(bool p_refresh_only) {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		Ref<Resource> edited_res = se->get_edited_resource();

		if (edited_res->is_built_in()) {
			continue; //internal script, who cares
		}

		if (!p_refresh_only) {
			uint64_t last_date = edited_res->get_last_modified_time();
			uint64_t date = FileAccess::get_modified_time(edited_res->get_path());

			if (last_date == date) {
				continue;
			}

			Ref<Script> scr = edited_res;
			if (scr.is_valid()) {
				Ref<Script> rel_scr = ResourceLoader::load(scr->get_path(), scr->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
				ERR_CONTINUE(!rel_scr.is_valid());
				scr->set_source_code(rel_scr->get_source_code());
				scr->set_last_modified_time(rel_scr->get_last_modified_time());
				scr->reload(true);

				update_docs_from_script(scr);
			}

			Ref<JSON> json = edited_res;
			if (json.is_valid()) {
				Ref<JSON> rel_json = ResourceLoader::load(json->get_path(), json->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
				ERR_CONTINUE(!rel_json.is_valid());
				json->parse(rel_json->get_parsed_text(), true);
				json->set_last_modified_time(rel_json->get_last_modified_time());
			}

			Ref<TextFile> text_file = edited_res;
			if (text_file.is_valid()) {
				text_file->reload_from_file();
			}
		}

		se->reload_text();
	}

	disk_changed->hide();
	_update_script_names();
}

void ScriptEditor::open_script_create_dialog(const String &p_base_name, const String &p_base_path) {
	_menu_option(FILE_NEW);
	script_create_dialog->config(p_base_name, p_base_path);
}

void ScriptEditor::open_text_file_create_dialog(const String &p_base_path, const String &p_base_name) {
	_menu_option(FILE_NEW_TEXTFILE);
	file_dialog->set_current_dir(p_base_path);
	file_dialog->set_current_file(p_base_name);
	open_textfile_after_create = false;
}

Ref<Resource> ScriptEditor::open_file(const String &p_file) {
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("Script", &extensions);
	ResourceLoader::get_recognized_extensions_for_type("JSON", &extensions);
	if (extensions.find(p_file.get_extension())) {
		Ref<Resource> scr = ResourceLoader::load(p_file);
		if (!scr.is_valid()) {
			EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + p_file, TTR("Error!"));
			return Ref<Resource>();
		}

		edit(scr);
		return scr;
	}

	Error error;
	Ref<TextFile> text_file = _load_text_file(p_file, &error);
	if (error != OK) {
		EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + p_file, TTR("Error!"));
		return Ref<Resource>();
	}

	if (text_file.is_valid()) {
		edit(text_file);
		return text_file;
	}
	return Ref<Resource>();
}

void ScriptEditor::_editor_stop() {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		se->set_debugger_active(false);
	}
}

void ScriptEditor::_add_callback(Object *p_obj, const String &p_function, const PackedStringArray &p_args) {
	ERR_FAIL_NULL(p_obj);
	Ref<Script> scr = p_obj->get_script();
	ERR_FAIL_COND(!scr.is_valid());

	if (!scr->get_language()->can_make_function()) {
		return;
	}

	EditorNode::get_singleton()->push_item(scr.ptr());

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}
		if (se->get_edited_resource() != scr) {
			continue;
		}

		se->add_callback(p_function, p_args);

		_go_to_tab(i);

		script_list->select(script_list->find_metadata(i));

		// Save the current script so the changes can be picked up by an external editor.
		if (!scr.ptr()->is_built_in()) { // But only if it's not built-in script.
			save_current_script();
		}

		break;
	}

	// Move back to the previously edited node to reselect it in the Inspector and the NodeDock.
	// We assume that the previous item is the node on which the callbacks were added.
	EditorNode::get_singleton()->edit_previous_item();
}

void ScriptEditor::_save_editor_state(ScriptEditorBase *p_editor) {
	if (restoring_layout) {
		return;
	}

	const String &path = p_editor->get_edited_resource()->get_path();
	if (!path.is_resource_file()) {
		return;
	}

	script_editor_cache->set_value(path, "state", p_editor->get_edit_state());
	// This is saved later when we save the editor layout.
}

void ScriptEditor::_save_layout() {
	if (restoring_layout) {
		return;
	}

	EditorNode::get_singleton()->save_editor_layout_delayed();
}

void ScriptEditor::_editor_settings_changed() {
	if (!EditorThemeManager::is_generated_theme_outdated() &&
			!EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor") &&
			!EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor") &&
			!EditorSettings::get_singleton()->check_changed_settings_in_group("docks/filesystem")) {
		return;
	}

	_apply_editor_settings();
}

void ScriptEditor::_apply_editor_settings() {
	textfile_extensions.clear();
	const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
	for (const String &E : textfile_ext) {
		textfile_extensions.insert(E);
	}

	trim_trailing_whitespace_on_save = EDITOR_GET("text_editor/behavior/files/trim_trailing_whitespace_on_save");
	trim_final_newlines_on_save = EDITOR_GET("text_editor/behavior/files/trim_final_newlines_on_save");
	convert_indent_on_save = EDITOR_GET("text_editor/behavior/files/convert_indent_on_save");

	members_overview_enabled = EDITOR_GET("text_editor/script_list/show_members_overview");
	help_overview_enabled = EDITOR_GET("text_editor/help/show_help_index");
	external_editor_active = EDITOR_GET("text_editor/external/use_external_editor");
	_update_members_overview_visibility();
	_update_help_overview_visibility();

	_update_autosave_timer();

	if (current_theme.is_empty()) {
		current_theme = EDITOR_GET("text_editor/theme/color_theme");
	} else if (current_theme != String(EDITOR_GET("text_editor/theme/color_theme"))) {
		current_theme = EDITOR_GET("text_editor/theme/color_theme");
		EditorSettings::get_singleton()->load_text_editor_theme();
	}

	_update_script_names();

	ScriptServer::set_reload_scripts_on_save(EDITOR_GET("text_editor/behavior/files/auto_reload_and_parse_scripts_on_save"));

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		se->update_settings();
	}
}

void ScriptEditor::_filesystem_changed() {
	_update_script_names();
}

void ScriptEditor::_files_moved(const String &p_old_file, const String &p_new_file) {
	if (!script_editor_cache->has_section(p_old_file)) {
		return;
	}
	Variant state = script_editor_cache->get_value(p_old_file, "state");
	script_editor_cache->erase_section(p_old_file);
	script_editor_cache->set_value(p_new_file, "state", state);

	// If Script, update breakpoints with debugger.
	Array breakpoints = _get_cached_breakpoints_for_script(p_new_file);
	for (int breakpoint : breakpoints) {
		int line = (int)breakpoint + 1;
		EditorDebuggerNode::get_singleton()->set_breakpoint(p_old_file, line, false);
		if (!p_new_file.begins_with("local://") && ResourceLoader::exists(p_new_file, "Script")) {
			EditorDebuggerNode::get_singleton()->set_breakpoint(p_new_file, line, true);
		}
	}
	// This is saved later when we save the editor layout.
}

void ScriptEditor::_file_removed(const String &p_removed_file) {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}
		if (se->get_meta("_edit_res_path") == p_removed_file) {
			// The script is deleted with no undo, so just close the tab.
			_close_tab(i, false, false);
		}
	}

	// Check closed.
	if (script_editor_cache->has_section(p_removed_file)) {
		Array breakpoints = _get_cached_breakpoints_for_script(p_removed_file);
		for (int breakpoint : breakpoints) {
			EditorDebuggerNode::get_singleton()->set_breakpoint(p_removed_file, (int)breakpoint + 1, false);
		}
		script_editor_cache->erase_section(p_removed_file);
	}
}

void ScriptEditor::_update_find_replace_bar() {
	ScriptEditorBase *se = _get_current_editor();
	if (se) {
		se->set_find_replace_bar(find_replace_bar);
	} else {
		find_replace_bar->set_text_edit(nullptr);
		find_replace_bar->hide();
	}
}

void ScriptEditor::_autosave_scripts() {
	save_all_scripts();
}

void ScriptEditor::_update_autosave_timer() {
	if (!autosave_timer->is_inside_tree()) {
		return;
	}

	float autosave_time = EDITOR_GET("text_editor/behavior/files/autosave_interval_secs");
	if (autosave_time > 0) {
		autosave_timer->set_wait_time(autosave_time);
		autosave_timer->start();
	} else {
		autosave_timer->stop();
	}
}

void ScriptEditor::_tree_changed() {
	if (waiting_update_names) {
		return;
	}

	waiting_update_names = true;
	callable_mp(this, &ScriptEditor::_update_script_names).call_deferred();
}

void ScriptEditor::_split_dragged(float) {
	_save_layout();
}

Variant ScriptEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (tab_container->get_tab_count() == 0) {
		return Variant();
	}

	Node *cur_node = tab_container->get_tab_control(tab_container->get_current_tab());

	HBoxContainer *drag_preview = memnew(HBoxContainer);
	String preview_name = "";
	Ref<Texture2D> preview_icon;

	ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(cur_node);
	if (se) {
		preview_name = se->get_name();
		preview_icon = se->get_theme_icon();
	}
	EditorHelp *eh = Object::cast_to<EditorHelp>(cur_node);
	if (eh) {
		preview_name = eh->get_class();
		preview_icon = get_editor_theme_icon(SNAME("Help"));
	}

	if (!preview_icon.is_null()) {
		TextureRect *tf = memnew(TextureRect);
		tf->set_texture(preview_icon);
		tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
		drag_preview->add_child(tf);
	}
	Label *label = memnew(Label(preview_name));
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // Don't translate script names and class names.
	drag_preview->add_child(label);
	set_drag_preview(drag_preview);

	Dictionary drag_data;
	drag_data["type"] = "script_list_element"; // using a custom type because node caused problems when dragging to scene tree
	drag_data["script_list_element"] = cur_node;

	return drag_data;
}

bool ScriptEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary d = p_data;
	if (!d.has("type")) {
		return false;
	}

	if (String(d["type"]) == "script_list_element") {
		Node *node = Object::cast_to<Node>(d["script_list_element"]);

		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(node);
		if (se) {
			return true;
		}
		EditorHelp *eh = Object::cast_to<EditorHelp>(node);
		if (eh) {
			return true;
		}
	}

	if (String(d["type"]) == "nodes") {
		Array nodes = d["nodes"];
		if (nodes.size() == 0) {
			return false;
		}
		Node *node = get_node((nodes[0]));

		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(node);
		if (se) {
			return true;
		}
		EditorHelp *eh = Object::cast_to<EditorHelp>(node);
		if (eh) {
			return true;
		}
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		if (files.size() == 0) {
			return false; //weird
		}

		for (int i = 0; i < files.size(); i++) {
			const String &file = files[i];
			if (file.is_empty() || !FileAccess::exists(file)) {
				continue;
			}
			if (ResourceLoader::exists(file, "Script") || ResourceLoader::exists(file, "JSON")) {
				Ref<Resource> scr = ResourceLoader::load(file);
				if (scr.is_valid()) {
					return true;
				}
			}

			if (textfile_extensions.has(file.get_extension())) {
				Error err;
				Ref<TextFile> text_file = _load_text_file(file, &err);
				if (text_file.is_valid() && err == OK) {
					return true;
				}
			}
		}
		return false;
	}

	return false;
}

void ScriptEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	Dictionary d = p_data;
	if (!d.has("type")) {
		return;
	}

	if (String(d["type"]) == "script_list_element") {
		Node *node = Object::cast_to<Node>(d["script_list_element"]);

		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(node);
		EditorHelp *eh = Object::cast_to<EditorHelp>(node);
		if (se || eh) {
			int new_index = 0;
			if (script_list->get_item_count() > 0) {
				new_index = script_list->get_item_metadata(script_list->get_item_at_position(p_point));
			}
			tab_container->move_child(node, new_index);
			tab_container->set_current_tab(new_index);
			_update_script_names();
		}
	}

	if (String(d["type"]) == "nodes") {
		Array nodes = d["nodes"];
		if (nodes.size() == 0) {
			return;
		}
		Node *node = get_node(nodes[0]);

		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(node);
		EditorHelp *eh = Object::cast_to<EditorHelp>(node);
		if (se || eh) {
			int new_index = 0;
			if (script_list->get_item_count() > 0) {
				new_index = script_list->get_item_metadata(script_list->get_item_at_position(p_point));
			}
			tab_container->move_child(node, new_index);
			tab_container->set_current_tab(new_index);
			_update_script_names();
		}
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		int new_index = 0;
		if (script_list->get_item_count() > 0) {
			new_index = script_list->get_item_metadata(script_list->get_item_at_position(p_point));
		}
		int num_tabs_before = tab_container->get_tab_count();
		for (int i = 0; i < files.size(); i++) {
			const String &file = files[i];
			if (file.is_empty() || !FileAccess::exists(file)) {
				continue;
			}

			if (!ResourceLoader::exists(file, "Script") && !ResourceLoader::exists(file, "JSON") && !textfile_extensions.has(file.get_extension())) {
				continue;
			}

			Ref<Resource> res = open_file(file);
			if (res.is_valid()) {
				const int num_tabs = tab_container->get_tab_count();
				if (num_tabs > num_tabs_before) {
					tab_container->move_child(tab_container->get_tab_control(tab_container->get_tab_count() - 1), new_index);
					num_tabs_before = num_tabs;
				} else if (num_tabs > 0) { /* Maybe script was already open */
					tab_container->move_child(tab_container->get_tab_control(tab_container->get_current_tab()), new_index);
				}
			}
		}
		if (tab_container->get_tab_count() > 0) {
			tab_container->set_current_tab(new_index);
		}
		_update_script_names();
	}
}

void ScriptEditor::input(const Ref<InputEvent> &p_event) {
	// This is implemented in `input()` rather than `unhandled_input()` to allow
	// the shortcut to be used regardless of the click location.
	// This feature can be disabled to avoid interfering with other uses of the additional
	// mouse buttons, such as push-to-talk in a VoIP program.
	if (EDITOR_GET("interface/editor/mouse_extra_buttons_navigate_history")) {
		const Ref<InputEventMouseButton> mb = p_event;

		// Navigate the script history using additional mouse buttons present on some mice.
		// This must be hardcoded as the editor shortcuts dialog doesn't allow assigning
		// more than one shortcut per action.
		if (mb.is_valid() && mb->is_pressed() && is_visible_in_tree()) {
			if (mb->get_button_index() == MouseButton::MB_XBUTTON1) {
				_history_back();
			}

			if (mb->get_button_index() == MouseButton::MB_XBUTTON2) {
				_history_forward();
			}
		}
	}
}

void ScriptEditor::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (!is_visible_in_tree() || !p_event->is_pressed() || p_event->is_echo()) {
		return;
	}
	if (ED_IS_SHORTCUT("script_editor/next_script", p_event)) {
		if (script_list->get_item_count() > 1) {
			int next_tab = script_list->get_current() + 1;
			next_tab %= script_list->get_item_count();
			_go_to_tab(script_list->get_item_metadata(next_tab));
			_update_script_names();
		}
		accept_event();
	}
	if (ED_IS_SHORTCUT("script_editor/prev_script", p_event)) {
		if (script_list->get_item_count() > 1) {
			int next_tab = script_list->get_current() - 1;
			next_tab = next_tab >= 0 ? next_tab : script_list->get_item_count() - 1;
			_go_to_tab(script_list->get_item_metadata(next_tab));
			_update_script_names();
		}
		accept_event();
	}
	if (ED_IS_SHORTCUT("script_editor/window_move_up", p_event)) {
		_menu_option(WINDOW_MOVE_UP);
		accept_event();
	}
	if (ED_IS_SHORTCUT("script_editor/window_move_down", p_event)) {
		_menu_option(WINDOW_MOVE_DOWN);
		accept_event();
	}

	Callable custom_callback = EditorContextMenuPluginManager::get_singleton()->match_custom_shortcut(EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR, p_event);
	if (custom_callback.is_valid()) {
		Ref<Resource> resource;
		ScriptEditorBase *current = _get_current_editor();
		if (current) {
			resource = current->get_edited_resource();
		}
		EditorContextMenuPluginManager::get_singleton()->invoke_callback(custom_callback, resource);
		accept_event();
	}
}

void ScriptEditor::_script_list_clicked(int p_item, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index == MouseButton::MIDDLE) {
		script_list->select(p_item);
		_script_selected(p_item);
		_menu_option(FILE_CLOSE);
	}

	if (p_mouse_button_index == MouseButton::RIGHT) {
		_make_script_list_context_menu();
	}
}

void ScriptEditor::_make_script_list_context_menu() {
	context_menu->clear();

	int selected = tab_container->get_current_tab();
	if (selected < 0 || selected >= tab_container->get_tab_count()) {
		return;
	}

	ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(selected));
	if (se) {
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/save"), FILE_SAVE);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/save_as"), FILE_SAVE_AS);
	}
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_file"), FILE_CLOSE);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_all"), CLOSE_ALL);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_other_tabs"), CLOSE_OTHER_TABS);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/close_docs"), CLOSE_DOCS);
	context_menu->add_separator();
	if (se) {
		Ref<Script> scr = se->get_edited_resource();
		if (scr.is_valid()) {
			if (!scr.is_null() && scr->is_tool()) {
				context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/reload_script_soft"), FILE_TOOL_RELOAD_SOFT);
				context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/run_file"), FILE_RUN);
				context_menu->add_separator();
			}
		}
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/copy_path"), FILE_COPY_PATH);
		context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/show_in_file_system"), SHOW_IN_FILE_SYSTEM);
		context_menu->add_separator();
	}

	context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/window_move_up"), WINDOW_MOVE_UP);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/window_move_down"), WINDOW_MOVE_DOWN);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/window_sort"), WINDOW_SORT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_editor/toggle_scripts_panel"), TOGGLE_SCRIPTS_PANEL);

	context_menu->set_item_disabled(context_menu->get_item_index(CLOSE_ALL), tab_container->get_tab_count() <= 0);
	context_menu->set_item_disabled(context_menu->get_item_index(CLOSE_OTHER_TABS), tab_container->get_tab_count() <= 1);
	context_menu->set_item_disabled(context_menu->get_item_index(CLOSE_DOCS), !_has_docs_tab());
	context_menu->set_item_disabled(context_menu->get_item_index(WINDOW_MOVE_UP), tab_container->get_current_tab() <= 0);
	context_menu->set_item_disabled(context_menu->get_item_index(WINDOW_MOVE_DOWN), tab_container->get_current_tab() >= tab_container->get_tab_count() - 1);
	context_menu->set_item_disabled(context_menu->get_item_index(WINDOW_SORT), tab_container->get_tab_count() <= 1);

	// Context menu plugin.
	Vector<String> selected_paths;
	if (se) {
		Ref<Resource> scr = se->get_edited_resource();
		if (scr.is_valid()) {
			String path = scr->get_path();
			selected_paths.push_back(path);
		}
	}
	EditorContextMenuPluginManager::get_singleton()->add_options_from_plugins(context_menu, EditorContextMenuPlugin::CONTEXT_SLOT_SCRIPT_EDITOR, selected_paths);

	context_menu->set_position(get_screen_position() + get_local_mouse_position());
	context_menu->reset_size();
	context_menu->popup();
}

void ScriptEditor::set_window_layout(Ref<ConfigFile> p_layout) {
	if (!bool(EDITOR_GET("text_editor/behavior/files/restore_scripts_on_load"))) {
		return;
	}

	if (!p_layout->has_section_key("ScriptEditor", "open_scripts") && !p_layout->has_section_key("ScriptEditor", "open_help")) {
		return;
	}

	Array scripts = p_layout->get_value("ScriptEditor", "open_scripts");
	Array helps;
	if (p_layout->has_section_key("ScriptEditor", "open_help")) {
		helps = p_layout->get_value("ScriptEditor", "open_help");
	}

	restoring_layout = true;

	HashSet<String> loaded_scripts;
	List<String> extensions;
	ResourceLoader::get_recognized_extensions_for_type("Script", &extensions);
	ResourceLoader::get_recognized_extensions_for_type("JSON", &extensions);

	for (int i = 0; i < scripts.size(); i++) {
		String path = scripts[i];

		Dictionary script_info = scripts[i];
		if (!script_info.is_empty()) {
			path = script_info["path"];
		}

		if (!FileAccess::exists(path)) {
			if (script_editor_cache->has_section(path)) {
				script_editor_cache->erase_section(path);
			}
			continue;
		}
		loaded_scripts.insert(path);

		if (extensions.find(path.get_extension())) {
			Ref<Resource> scr = ResourceLoader::load(path);
			if (!scr.is_valid()) {
				continue;
			}
			if (!edit(scr, false)) {
				continue;
			}
		} else {
			Error error;
			Ref<TextFile> text_file = _load_text_file(path, &error);
			if (error != OK || !text_file.is_valid()) {
				continue;
			}
			if (!edit(text_file, false)) {
				continue;
			}
		}

		if (!script_info.is_empty()) {
			ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(tab_container->get_tab_count() - 1));
			if (se) {
				se->set_edit_state(script_info["state"]);
			}
		}
	}

	for (int i = 0; i < helps.size(); i++) {
		String path = helps[i];
		if (path.is_empty()) { // invalid, skip
			continue;
		}
		_help_class_open(path);
	}

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		tab_container->get_tab_control(i)->set_meta("__editor_pass", Variant());
	}

	if (p_layout->has_section_key("ScriptEditor", "script_split_offset")) {
		script_split->set_split_offset(p_layout->get_value("ScriptEditor", "script_split_offset"));
	}

	if (p_layout->has_section_key("ScriptEditor", "list_split_offset")) {
		list_split->set_split_offset(p_layout->get_value("ScriptEditor", "list_split_offset"));
	}

	// Remove any deleted editors that have been removed between launches.
	// and if a Script, register breakpoints with the debugger.
	List<String> cached_editors;
	script_editor_cache->get_sections(&cached_editors);
	for (const String &E : cached_editors) {
		if (loaded_scripts.has(E)) {
			continue;
		}

		if (!FileAccess::exists(E)) {
			script_editor_cache->erase_section(E);
			continue;
		}

		Array breakpoints = _get_cached_breakpoints_for_script(E);
		for (int breakpoint : breakpoints) {
			EditorDebuggerNode::get_singleton()->set_breakpoint(E, (int)breakpoint + 1, true);
		}
	}

	_set_zoom_factor(p_layout->get_value("ScriptEditor", "zoom_factor", 1.0f));

	restoring_layout = false;

	_update_script_names();

	if (p_layout->has_section_key("ScriptEditor", "selected_script")) {
		String selected_script = p_layout->get_value("ScriptEditor", "selected_script");
		// If the selected script is not in the list of open scripts, select nothing.
		for (int i = 0; i < tab_container->get_tab_count(); i++) {
			ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
			if (se && se->get_edited_resource()->get_path() == selected_script) {
				_go_to_tab(i);
				break;
			}
		}
	}
}

void ScriptEditor::get_window_layout(Ref<ConfigFile> p_layout) {
	Array scripts;
	Array helps;
	String selected_script;
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (se) {
			String path = se->get_edited_resource()->get_path();
			if (!path.is_resource_file()) {
				continue;
			}

			if (tab_container->get_current_tab_control() == tab_container->get_tab_control(i)) {
				selected_script = path;
			}

			_save_editor_state(se);
			scripts.push_back(path);
		}

		EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));

		if (eh) {
			helps.push_back(eh->get_class());
		}
	}

	p_layout->set_value("ScriptEditor", "open_scripts", scripts);
	p_layout->set_value("ScriptEditor", "selected_script", selected_script);
	p_layout->set_value("ScriptEditor", "open_help", helps);
	p_layout->set_value("ScriptEditor", "script_split_offset", script_split->get_split_offset());
	p_layout->set_value("ScriptEditor", "list_split_offset", list_split->get_split_offset());
	p_layout->set_value("ScriptEditor", "zoom_factor", zoom_factor);

	// Save the cache.
	script_editor_cache->save(EditorPaths::get_singleton()->get_project_settings_dir().path_join("script_editor_cache.cfg"));
}

void ScriptEditor::_help_class_open(const String &p_class) {
	if (p_class.is_empty()) {
		return;
	}

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));

		if (eh && eh->get_class() == p_class) {
			_go_to_tab(i);
			_update_script_names();
			return;
		}
	}

	EditorHelp *eh = memnew(EditorHelp);

	eh->set_name(p_class);
	tab_container->add_child(eh);
	_go_to_tab(tab_container->get_tab_count() - 1);
	eh->go_to_class(p_class);
	eh->connect("go_to_help", callable_mp(this, &ScriptEditor::_help_class_goto));
	eh->connect("request_save_history", callable_mp(this, &ScriptEditor::_save_history));
	_add_recent_script(p_class);
	_sort_list_on_update = true;
	_update_script_names();
	_save_layout();
}

void ScriptEditor::_help_class_goto(const String &p_desc) {
	String cname = p_desc.get_slice(":", 1);

	if (_help_tab_goto(cname, p_desc)) {
		return;
	}

	EditorHelp *eh = memnew(EditorHelp);

	eh->set_name(cname);
	tab_container->add_child(eh);
	eh->go_to_help(p_desc);
	eh->connect("go_to_help", callable_mp(this, &ScriptEditor::_help_class_goto));
	_add_recent_script(eh->get_class());
	_sort_list_on_update = true;
	_update_script_names();
	_save_layout();

	callable_mp(this, &ScriptEditor::_help_tab_goto).call_deferred(cname, p_desc);
}

bool ScriptEditor::_help_tab_goto(const String &p_name, const String &p_desc) {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));

		if (eh && eh->get_class() == p_name) {
			_go_to_tab(i);
			eh->go_to_help(p_desc);
			_update_script_names();
			return true;
		}
	}
	return false;
}

void ScriptEditor::update_doc(const String &p_name) {
	ERR_FAIL_COND(!EditorHelp::get_doc_data()->has_doc(p_name));

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_tab_control(i));
		if (eh && eh->get_class() == p_name) {
			eh->update_doc();
			return;
		}
	}
}

void ScriptEditor::clear_docs_from_script(const Ref<Script> &p_script) {
	ERR_FAIL_COND(p_script.is_null());

	for (const DocData::ClassDoc &cd : p_script->get_documentation()) {
		if (EditorHelp::get_doc_data()->has_doc(cd.name)) {
			EditorHelp::get_doc_data()->remove_doc(cd.name);
		}
	}
}

void ScriptEditor::update_docs_from_script(const Ref<Script> &p_script) {
	ERR_FAIL_COND(p_script.is_null());

	for (const DocData::ClassDoc &cd : p_script->get_documentation()) {
		EditorHelp::get_doc_data()->add_doc(cd);
		update_doc(cd.name);
	}
}

void ScriptEditor::_update_selected_editor_menu() {
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		bool current = tab_container->get_current_tab() == i;

		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (se && se->get_edit_menu()) {
			if (current) {
				se->get_edit_menu()->show();
			} else {
				se->get_edit_menu()->hide();
			}
		}
	}

	EditorHelp *eh = Object::cast_to<EditorHelp>(tab_container->get_current_tab_control());
	script_search_menu->get_popup()->clear();
	if (eh) {
		script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/find", TTR("Find..."), KeyModifierMask::CMD_OR_CTRL | Key::F), HELP_SEARCH_FIND);
		script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/find_next", TTR("Find Next"), Key::F3), HELP_SEARCH_FIND_NEXT);
		script_search_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/find_previous", TTR("Find Previous"), KeyModifierMask::SHIFT | Key::F3), HELP_SEARCH_FIND_PREVIOUS);
		script_search_menu->get_popup()->add_separator();
		script_search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/find_in_files"), SEARCH_IN_FILES);
		script_search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/replace_in_files"), REPLACE_IN_FILES);
		script_search_menu->show();
	} else {
		if (tab_container->get_tab_count() == 0) {
			script_search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/find_in_files"), SEARCH_IN_FILES);
			script_search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/replace_in_files"), REPLACE_IN_FILES);
			script_search_menu->show();
		} else {
			script_search_menu->hide();
		}
	}
}

void ScriptEditor::_update_history_pos(int p_new_pos) {
	Node *n = tab_container->get_current_tab_control();

	if (Object::cast_to<ScriptEditorBase>(n)) {
		history.write[history_pos].state = Object::cast_to<ScriptEditorBase>(n)->get_navigation_state();
	}
	if (Object::cast_to<EditorHelp>(n)) {
		history.write[history_pos].state = Object::cast_to<EditorHelp>(n)->get_scroll();
	}

	history_pos = p_new_pos;
	tab_container->set_current_tab(tab_container->get_tab_idx_from_control(history[history_pos].control));

	n = history[history_pos].control;

	ScriptEditorBase *seb = Object::cast_to<ScriptEditorBase>(n);
	if (seb) {
		lock_history = true;
		seb->set_edit_state(history[history_pos].state);
		seb->ensure_focus();

		Ref<Script> scr = seb->get_edited_resource();
		if (scr.is_valid()) {
			notify_script_changed(scr);
		}
	}

	EditorHelp *eh = Object::cast_to<EditorHelp>(n);
	if (eh) {
		eh->set_scroll(history[history_pos].state);
		eh->set_focused();
	}

	n->set_meta("__editor_pass", ++edit_pass);
	_update_script_names();
	_update_history_arrows();
	_update_selected_editor_menu();
}

void ScriptEditor::_history_forward() {
	if (history_pos < history.size() - 1) {
		_update_history_pos(history_pos + 1);
	}
}

void ScriptEditor::_history_back() {
	if (history_pos > 0) {
		_update_history_pos(history_pos - 1);
	}
}

Vector<Ref<Script>> ScriptEditor::get_open_scripts() const {
	Vector<Ref<Script>> out_scripts = Vector<Ref<Script>>();

	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}

		Ref<Script> scr = se->get_edited_resource();
		if (scr.is_valid()) {
			out_scripts.push_back(scr);
		}
	}

	return out_scripts;
}

TypedArray<ScriptEditorBase> ScriptEditor::_get_open_script_editors() const {
	TypedArray<ScriptEditorBase> script_editors;
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (!se) {
			continue;
		}
		script_editors.push_back(se);
	}
	return script_editors;
}

void ScriptEditor::set_scene_root_script(Ref<Script> p_script) {
	// Don't open dominant script if using an external editor.
	bool use_external_editor =
			external_editor_active ||
			(p_script.is_valid() && p_script->get_language()->overrides_external_editor());
	use_external_editor = use_external_editor && !(p_script.is_valid() && p_script->is_built_in()); // Ignore external editor for built-in scripts.
	const bool open_dominant = EDITOR_GET("text_editor/behavior/files/open_dominant_script_on_scene_change");

	if (open_dominant && !use_external_editor && p_script.is_valid()) {
		edit(p_script);
	}
}

bool ScriptEditor::script_goto_method(Ref<Script> p_script, const String &p_method) {
	int line = p_script->get_member_line(p_method);

	if (line == -1) {
		return false;
	}

	return edit(p_script, line, 0);
}

void ScriptEditor::set_live_auto_reload_running_scripts(bool p_enabled) {
	auto_reload_running_scripts = p_enabled;
}

void ScriptEditor::_help_search(const String &p_text) {
	help_search_dialog->popup_dialog(p_text);
}

void ScriptEditor::_open_script_request(const String &p_path) {
	Ref<Script> scr = ResourceLoader::load(p_path);
	if (scr.is_valid()) {
		script_editor->edit(scr, false);
		return;
	}

	Ref<JSON> json = ResourceLoader::load(p_path);
	if (json.is_valid()) {
		script_editor->edit(json, false);
		return;
	}

	Error err;
	Ref<TextFile> text_file = script_editor->_load_text_file(p_path, &err);
	if (text_file.is_valid()) {
		script_editor->edit(text_file, false);
		return;
	}
}

void ScriptEditor::register_syntax_highlighter(const Ref<EditorSyntaxHighlighter> &p_syntax_highlighter) {
	ERR_FAIL_COND(p_syntax_highlighter.is_null());

	if (!syntax_highlighters.has(p_syntax_highlighter)) {
		syntax_highlighters.push_back(p_syntax_highlighter);
	}
}

void ScriptEditor::unregister_syntax_highlighter(const Ref<EditorSyntaxHighlighter> &p_syntax_highlighter) {
	ERR_FAIL_COND(p_syntax_highlighter.is_null());

	syntax_highlighters.erase(p_syntax_highlighter);
}

int ScriptEditor::script_editor_func_count = 0;
CreateScriptEditorFunc ScriptEditor::script_editor_funcs[ScriptEditor::SCRIPT_EDITOR_FUNC_MAX];

void ScriptEditor::register_create_script_editor_function(CreateScriptEditorFunc p_func) {
	ERR_FAIL_COND(script_editor_func_count == SCRIPT_EDITOR_FUNC_MAX);
	script_editor_funcs[script_editor_func_count++] = p_func;
}

void ScriptEditor::_script_changed() {
	NodeDock::get_singleton()->update_lists();
}

void ScriptEditor::_on_find_in_files_requested(const String &text) {
	find_in_files_dialog->set_find_in_files_mode(FindInFilesDialog::SEARCH_MODE);
	find_in_files_dialog->set_search_text(text);
	find_in_files_dialog->popup_centered();
}

void ScriptEditor::_on_replace_in_files_requested(const String &text) {
	find_in_files_dialog->set_find_in_files_mode(FindInFilesDialog::REPLACE_MODE);
	find_in_files_dialog->set_search_text(text);
	find_in_files_dialog->set_replace_text("");
	find_in_files_dialog->popup_centered();
}

void ScriptEditor::_on_find_in_files_result_selected(const String &fpath, int line_number, int begin, int end) {
	if (ResourceLoader::exists(fpath)) {
		Ref<Resource> res = ResourceLoader::load(fpath);

		if (fpath.get_extension() == "gdshader") {
			ShaderEditorPlugin *shader_editor = Object::cast_to<ShaderEditorPlugin>(EditorNode::get_editor_data().get_editor_by_name("Shader"));
			shader_editor->edit(res.ptr());
			shader_editor->make_visible(true);
			TextShaderEditor *text_shader_editor = Object::cast_to<TextShaderEditor>(shader_editor->get_shader_editor(res));
			if (text_shader_editor) {
				text_shader_editor->goto_line_selection(line_number - 1, begin, end);
			}
			return;
		} else if (fpath.get_extension() == "tscn") {
			Ref<FileAccess> f = FileAccess::open(fpath, FileAccess::READ);
			bool is_script_found = false;

			// Starting from top of the tscn file.
			int scr_start_line = 1;

			String scr_header = "[sub_resource type=\"GDScript\" id=\"";
			String scr_id = "";
			String line = "";

			int l = 0;

			while (!f->eof_reached()) {
				line = f->get_line();
				l++;

				if (!line.begins_with(scr_header)) {
					continue;
				}

				// Found the end of the script.
				scr_id = line.get_slice(scr_header, 1);
				scr_id = scr_id.get_slice("\"", 0);

				scr_start_line = l + 1;
				int scr_line_count = 0;

				do {
					line = f->get_line();
					l++;
					String strline = line.strip_edges();

					if (strline.ends_with("\"") && !strline.ends_with("\\\"")) {
						// Found the end of script.
						break;
					}
					scr_line_count++;

				} while (!f->eof_reached());

				if (line_number > scr_start_line + scr_line_count) {
					// Find in another built-in GDScript.
					continue;
				}

				// Real line number of the built-in script.
				line_number = line_number - scr_start_line;

				is_script_found = true;
				break;
			}

			EditorNode::get_singleton()->load_scene(fpath);

			if (is_script_found && !scr_id.is_empty()) {
				Ref<Script> scr = ResourceLoader::load(fpath + "::" + scr_id, "Script");
				if (scr.is_valid()) {
					edit(scr);
					ScriptTextEditor *ste = Object::cast_to<ScriptTextEditor>(_get_current_editor());

					if (ste) {
						EditorInterface::get_singleton()->set_main_screen_editor("Script");
						ste->goto_line_selection(line_number, begin, end);
					}
				}
			}

			return;
		} else {
			Ref<Script> scr = res;
			Ref<JSON> json = res;
			if (scr.is_valid() || json.is_valid()) {
				edit(scr);

				ScriptTextEditor *ste = Object::cast_to<ScriptTextEditor>(_get_current_editor());
				if (ste) {
					EditorInterface::get_singleton()->set_main_screen_editor("Script");
					ste->goto_line_selection(line_number - 1, begin, end);
				}
				return;
			}
		}
	}

	// If the file is not a valid resource/script, load it as a text file.
	Error err;
	Ref<TextFile> text_file = _load_text_file(fpath, &err);
	if (text_file.is_valid()) {
		edit(text_file);

		TextEditor *te = Object::cast_to<TextEditor>(_get_current_editor());
		if (te) {
			te->goto_line_selection(line_number - 1, begin, end);
		}
	}
}

void ScriptEditor::_start_find_in_files(bool with_replace) {
	FindInFiles *f = find_in_files->get_finder();

	f->set_search_text(find_in_files_dialog->get_search_text());
	f->set_match_case(find_in_files_dialog->is_match_case());
	f->set_whole_words(find_in_files_dialog->is_whole_words());
	f->set_folder(find_in_files_dialog->get_folder());
	f->set_filter(find_in_files_dialog->get_filter());

	find_in_files->set_with_replace(with_replace);
	find_in_files->set_replace_text(find_in_files_dialog->get_replace_text());
	find_in_files->start_search();

	if (find_in_files_button->get_index() != find_in_files_button->get_parent()->get_child_count()) {
		find_in_files_button->get_parent()->move_child(find_in_files_button, -1);
	}
	if (!find_in_files_button->is_visible()) {
		find_in_files_button->show();
	}

	EditorNode::get_bottom_panel()->make_item_visible(find_in_files);
}

void ScriptEditor::_on_find_in_files_modified_files(const PackedStringArray &paths) {
	_test_script_times_on_disk();
	_update_modified_scripts_for_external_editor();
}

void ScriptEditor::_set_zoom_factor(float p_zoom_factor) {
	if (zoom_factor == p_zoom_factor) {
		return;
	}
	zoom_factor = p_zoom_factor;
	for (int i = 0; i < tab_container->get_tab_count(); i++) {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(tab_container->get_tab_control(i));
		if (se) {
			CodeTextEditor *cte = se->get_code_editor();
			if (cte) {
				if (zoom_factor != cte->get_zoom_factor()) {
					cte->set_zoom_factor(zoom_factor);
				}
			}
		}
	}
}

void ScriptEditor::_on_find_in_files_close_button_clicked() {
	EditorNode::get_bottom_panel()->hide_bottom_panel();
	find_in_files_button->hide();
}

void ScriptEditor::_window_changed(bool p_visible) {
	make_floating->set_visible(!p_visible);
	is_floating = p_visible;
}

void ScriptEditor::_filter_scripts_text_changed(const String &p_newtext) {
	_update_script_names();
}

void ScriptEditor::_filter_methods_text_changed(const String &p_newtext) {
	_update_members_overview();
}

void ScriptEditor::_bind_methods() {
	ClassDB::bind_method("_help_tab_goto", &ScriptEditor::_help_tab_goto);
	ClassDB::bind_method("get_current_editor", &ScriptEditor::_get_current_editor);
	ClassDB::bind_method("get_open_script_editors", &ScriptEditor::_get_open_script_editors);
	ClassDB::bind_method("get_breakpoints", &ScriptEditor::_get_breakpoints);

	ClassDB::bind_method(D_METHOD("register_syntax_highlighter", "syntax_highlighter"), &ScriptEditor::register_syntax_highlighter);
	ClassDB::bind_method(D_METHOD("unregister_syntax_highlighter", "syntax_highlighter"), &ScriptEditor::unregister_syntax_highlighter);

	ClassDB::bind_method(D_METHOD("goto_line", "line_number"), &ScriptEditor::_goto_script_line2);
	ClassDB::bind_method(D_METHOD("get_current_script"), &ScriptEditor::_get_current_script);
	ClassDB::bind_method(D_METHOD("get_open_scripts"), &ScriptEditor::_get_open_scripts);
	ClassDB::bind_method(D_METHOD("open_script_create_dialog", "base_name", "base_path"), &ScriptEditor::open_script_create_dialog);

	ClassDB::bind_method(D_METHOD("goto_help", "topic"), &ScriptEditor::goto_help);
	ClassDB::bind_method(D_METHOD("update_docs_from_script", "script"), &ScriptEditor::update_docs_from_script);

	ADD_SIGNAL(MethodInfo("editor_script_changed", PropertyInfo(Variant::OBJECT, "script", PROPERTY_HINT_RESOURCE_TYPE, "Script")));
	ADD_SIGNAL(MethodInfo("script_close", PropertyInfo(Variant::OBJECT, "script", PROPERTY_HINT_RESOURCE_TYPE, "Script")));
}

ScriptEditor::ScriptEditor(WindowWrapper *p_wrapper) {
	window_wrapper = p_wrapper;
	current_theme = "";

	script_editor_cache.instantiate();
	script_editor_cache->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("script_editor_cache.cfg"));

	completion_cache = memnew(EditorScriptCodeCompletionCache);
	restoring_layout = false;
	waiting_update_names = false;
	pending_auto_reload = false;
	auto_reload_running_scripts = true;
	external_editor_active = false;
	members_overview_enabled = EDITOR_GET("text_editor/script_list/show_members_overview");
	help_overview_enabled = EDITOR_GET("text_editor/help/show_help_index");

	VBoxContainer *main_container = memnew(VBoxContainer);
	add_child(main_container);

	menu_hb = memnew(HBoxContainer);
	main_container->add_child(menu_hb);

	script_split = memnew(HSplitContainer);
	main_container->add_child(script_split);
	script_split->set_v_size_flags(SIZE_EXPAND_FILL);

	list_split = memnew(VSplitContainer);
	script_split->add_child(list_split);
	list_split->set_v_size_flags(SIZE_EXPAND_FILL);

	scripts_vbox = memnew(VBoxContainer);
	scripts_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
	list_split->add_child(scripts_vbox);

	filter_scripts = memnew(LineEdit);
	filter_scripts->set_placeholder(TTR("Filter Scripts"));
	filter_scripts->set_clear_button_enabled(true);
	filter_scripts->connect(SceneStringName(text_changed), callable_mp(this, &ScriptEditor::_filter_scripts_text_changed));
	scripts_vbox->add_child(filter_scripts);

	script_list = memnew(ItemList);
	script_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	scripts_vbox->add_child(script_list);
	script_list->set_custom_minimum_size(Size2(100, 60) * EDSCALE); //need to give a bit of limit to avoid it from disappearing
	script_list->set_v_size_flags(SIZE_EXPAND_FILL);
	script_split->set_split_offset(200 * EDSCALE);
	_sort_list_on_update = true;
	script_list->connect("item_clicked", callable_mp(this, &ScriptEditor::_script_list_clicked), CONNECT_DEFERRED);
	script_list->set_allow_rmb_select(true);
	SET_DRAG_FORWARDING_GCD(script_list, ScriptEditor);

	context_menu = memnew(PopupMenu);
	add_child(context_menu);
	context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptEditor::_menu_option));

	overview_vbox = memnew(VBoxContainer);
	overview_vbox->set_custom_minimum_size(Size2(0, 90));
	overview_vbox->set_v_size_flags(SIZE_EXPAND_FILL);

	list_split->add_child(overview_vbox);
	list_split->set_visible(EditorSettings::get_singleton()->get_project_metadata("scripts_panel", "show_scripts_panel", true));
	buttons_hbox = memnew(HBoxContainer);
	overview_vbox->add_child(buttons_hbox);

	filename = memnew(Label);
	filename->set_clip_text(true);
	filename->set_h_size_flags(SIZE_EXPAND_FILL);
	filename->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	filename->add_theme_style_override(CoreStringName(normal), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(CoreStringName(normal), SNAME("LineEdit")));
	buttons_hbox->add_child(filename);

	members_overview_alphabeta_sort_button = memnew(Button);
	members_overview_alphabeta_sort_button->set_flat(true);
	members_overview_alphabeta_sort_button->set_tooltip_text(TTR("Toggle alphabetical sorting of the method list."));
	members_overview_alphabeta_sort_button->set_toggle_mode(true);
	members_overview_alphabeta_sort_button->set_pressed(EDITOR_GET("text_editor/script_list/sort_members_outline_alphabetically"));
	members_overview_alphabeta_sort_button->connect(SceneStringName(toggled), callable_mp(this, &ScriptEditor::_toggle_members_overview_alpha_sort));

	buttons_hbox->add_child(members_overview_alphabeta_sort_button);

	filter_methods = memnew(LineEdit);
	filter_methods->set_placeholder(TTR("Filter Methods"));
	filter_methods->set_clear_button_enabled(true);
	filter_methods->connect(SceneStringName(text_changed), callable_mp(this, &ScriptEditor::_filter_methods_text_changed));
	overview_vbox->add_child(filter_methods);

	members_overview = memnew(ItemList);
	members_overview->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	overview_vbox->add_child(members_overview);

	members_overview->set_allow_reselect(true);
	members_overview->set_custom_minimum_size(Size2(0, 60) * EDSCALE); //need to give a bit of limit to avoid it from disappearing
	members_overview->set_v_size_flags(SIZE_EXPAND_FILL);
	members_overview->set_allow_rmb_select(true);

	help_overview = memnew(ItemList);
	help_overview->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	overview_vbox->add_child(help_overview);
	help_overview->set_allow_reselect(true);
	help_overview->set_custom_minimum_size(Size2(0, 60) * EDSCALE); //need to give a bit of limit to avoid it from disappearing
	help_overview->set_v_size_flags(SIZE_EXPAND_FILL);

	VBoxContainer *code_editor_container = memnew(VBoxContainer);
	script_split->add_child(code_editor_container);

	tab_container = memnew(TabContainer);
	tab_container->set_tabs_visible(false);
	tab_container->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	code_editor_container->add_child(tab_container);
	tab_container->set_h_size_flags(SIZE_EXPAND_FILL);
	tab_container->set_v_size_flags(SIZE_EXPAND_FILL);

	find_replace_bar = memnew(FindReplaceBar);
	code_editor_container->add_child(find_replace_bar);
	find_replace_bar->hide();

	ED_SHORTCUT("script_editor/window_sort", TTR("Sort"));
	ED_SHORTCUT("script_editor/window_move_up", TTR("Move Up"), KeyModifierMask::SHIFT | KeyModifierMask::ALT | Key::UP);
	ED_SHORTCUT("script_editor/window_move_down", TTR("Move Down"), KeyModifierMask::SHIFT | KeyModifierMask::ALT | Key::DOWN);
	// FIXME: These should be `Key::GREATER` and `Key::LESS` but those don't work.
	ED_SHORTCUT("script_editor/next_script", TTR("Next Script"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::PERIOD);
	ED_SHORTCUT("script_editor/prev_script", TTR("Previous Script"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::COMMA);
	set_process_input(true);
	set_process_shortcut_input(true);

	file_menu = memnew(MenuButton);
	file_menu->set_text(TTR("File"));
	file_menu->set_switch_on_hover(true);
	file_menu->set_shortcut_context(this);
	menu_hb->add_child(file_menu);

	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new", TTR("New Script..."), KeyModifierMask::CMD_OR_CTRL | Key::N), FILE_NEW);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new_textfile", TTR("New Text File..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::N), FILE_NEW_TEXTFILE);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/open", TTR("Open...")), FILE_OPEN);
	file_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_editor/reopen_closed_script"), FILE_REOPEN_CLOSED);

	recent_scripts = memnew(PopupMenu);
	file_menu->get_popup()->add_submenu_node_item(TTR("Open Recent"), recent_scripts, FILE_OPEN_RECENT);
	recent_scripts->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptEditor::_open_recent_script));

	_update_recent_scripts();

	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save", TTR("Save"), KeyModifierMask::ALT | KeyModifierMask::CMD_OR_CTRL | Key::S), FILE_SAVE);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save_as", TTR("Save As...")), FILE_SAVE_AS);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save_all", TTR("Save All"), KeyModifierMask::SHIFT | KeyModifierMask::ALT | Key::S), FILE_SAVE_ALL);
	ED_SHORTCUT_OVERRIDE("script_editor/save_all", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::S);
	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/reload_script_soft", TTR("Soft Reload Tool Script"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::ALT | Key::R), FILE_TOOL_RELOAD_SOFT);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/copy_path", TTR("Copy Script Path")), FILE_COPY_PATH);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/show_in_file_system", TTR("Show in FileSystem")), SHOW_IN_FILE_SYSTEM);
	file_menu->get_popup()->add_separator();

	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/history_previous", TTR("History Previous"), KeyModifierMask::ALT | Key::LEFT), WINDOW_PREV);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/history_next", TTR("History Next"), KeyModifierMask::ALT | Key::RIGHT), WINDOW_NEXT);
	ED_SHORTCUT_OVERRIDE("script_editor/history_previous", "macos", KeyModifierMask::ALT | KeyModifierMask::META | Key::LEFT);
	ED_SHORTCUT_OVERRIDE("script_editor/history_next", "macos", KeyModifierMask::ALT | KeyModifierMask::META | Key::RIGHT);

	file_menu->get_popup()->add_separator();

	theme_submenu = memnew(PopupMenu);
	theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/import_theme", TTR("Import Theme...")), THEME_IMPORT);
	theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/reload_theme", TTR("Reload Theme")), THEME_RELOAD);
	file_menu->get_popup()->add_submenu_node_item(TTR("Theme"), theme_submenu, FILE_THEME);
	theme_submenu->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptEditor::_theme_option));

	theme_submenu->add_separator();
	theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/save_theme", TTR("Save Theme")), THEME_SAVE);
	theme_submenu->add_shortcut(ED_SHORTCUT("script_editor/save_theme_as", TTR("Save Theme As...")), THEME_SAVE_AS);

	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_file", TTR("Close"), KeyModifierMask::CMD_OR_CTRL | Key::W), FILE_CLOSE);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_all", TTR("Close All")), CLOSE_ALL);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_other_tabs", TTR("Close Other Tabs")), CLOSE_OTHER_TABS);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_docs", TTR("Close Docs")), CLOSE_DOCS);

	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/run_file", TTR("Run"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::X), FILE_RUN);

	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/toggle_scripts_panel", TTR("Toggle Scripts Panel"), KeyModifierMask::CMD_OR_CTRL | Key::BACKSLASH), TOGGLE_SCRIPTS_PANEL);
	file_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptEditor::_menu_option));
	file_menu->get_popup()->connect("about_to_popup", callable_mp(this, &ScriptEditor::_prepare_file_menu));
	file_menu->get_popup()->connect("popup_hide", callable_mp(this, &ScriptEditor::_file_menu_closed));

	script_search_menu = memnew(MenuButton);
	script_search_menu->set_text(TTR("Search"));
	script_search_menu->set_switch_on_hover(true);
	script_search_menu->set_shortcut_context(this);
	script_search_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &ScriptEditor::_menu_option));
	menu_hb->add_child(script_search_menu);

	MenuButton *debug_menu_btn = memnew(MenuButton);
	menu_hb->add_child(debug_menu_btn);
	debug_menu_btn->hide(); // Handled by EditorDebuggerNode below.

	EditorDebuggerNode *debugger = EditorDebuggerNode::get_singleton();
	debugger->set_script_debug_button(debug_menu_btn);
	debugger->connect("goto_script_line", callable_mp(this, &ScriptEditor::_goto_script_line));
	debugger->connect("set_execution", callable_mp(this, &ScriptEditor::_set_execution));
	debugger->connect("clear_execution", callable_mp(this, &ScriptEditor::_clear_execution));
	debugger->connect("breaked", callable_mp(this, &ScriptEditor::_breaked));
	debugger->connect("breakpoint_set_in_tree", callable_mp(this, &ScriptEditor::_set_breakpoint));
	debugger->connect("breakpoints_cleared_in_tree", callable_mp(this, &ScriptEditor::_clear_breakpoints));

	menu_hb->add_spacer();

	script_icon = memnew(TextureRect);
	menu_hb->add_child(script_icon);
	script_name_label = memnew(Label);
	menu_hb->add_child(script_name_label);

	script_icon->hide();
	script_name_label->hide();

	menu_hb->add_spacer();

	site_search = memnew(Button);
	site_search->set_flat(true);
	site_search->connect(SceneStringName(pressed), callable_mp(this, &ScriptEditor::_menu_option).bind(SEARCH_WEBSITE));
	menu_hb->add_child(site_search);

	help_search = memnew(Button);
	help_search->set_flat(true);
	help_search->set_text(TTR("Search Help"));
	help_search->connect(SceneStringName(pressed), callable_mp(this, &ScriptEditor::_menu_option).bind(SEARCH_HELP));
	menu_hb->add_child(help_search);
	help_search->set_tooltip_text(TTR("Search the reference documentation."));

	menu_hb->add_child(memnew(VSeparator));

	script_back = memnew(Button);
	script_back->set_flat(true);
	script_back->connect(SceneStringName(pressed), callable_mp(this, &ScriptEditor::_history_back));
	menu_hb->add_child(script_back);
	script_back->set_disabled(true);
	script_back->set_tooltip_text(TTR("Go to previous edited document."));

	script_forward = memnew(Button);
	script_forward->set_flat(true);
	script_forward->connect(SceneStringName(pressed), callable_mp(this, &ScriptEditor::_history_forward));
	menu_hb->add_child(script_forward);
	script_forward->set_disabled(true);
	script_forward->set_tooltip_text(TTR("Go to next edited document."));

	menu_hb->add_child(memnew(VSeparator));

	make_floating = memnew(ScreenSelect);
	make_floating->set_flat(true);
	make_floating->connect("request_open_in_screen", callable_mp(window_wrapper, &WindowWrapper::enable_window_on_screen).bind(true));
	if (!make_floating->is_disabled()) {
		// Override default ScreenSelect tooltip if multi-window support is available.
		make_floating->set_tooltip_text(TTR("Make the script editor floating.\nRight-click to open the screen selector."));
	}

	menu_hb->add_child(make_floating);
	p_wrapper->connect("window_visibility_changed", callable_mp(this, &ScriptEditor::_window_changed));

	tab_container->connect("tab_changed", callable_mp(this, &ScriptEditor::_tab_changed));

	erase_tab_confirm = memnew(ConfirmationDialog);
	erase_tab_confirm->set_ok_button_text(TTR("Save"));
	erase_tab_confirm->add_button(TTR("Discard"), DisplayServer::get_singleton()->get_swap_cancel_ok(), "discard");
	erase_tab_confirm->connect(SceneStringName(confirmed), callable_mp(this, &ScriptEditor::_close_current_tab).bind(true, true));
	erase_tab_confirm->connect("custom_action", callable_mp(this, &ScriptEditor::_close_discard_current_tab));
	add_child(erase_tab_confirm);

	script_create_dialog = memnew(ScriptCreateDialog);
	script_create_dialog->set_title(TTR("Create Script"));
	add_child(script_create_dialog);
	script_create_dialog->connect("script_created", callable_mp(this, &ScriptEditor::_script_created));

	file_dialog_option = -1;
	file_dialog = memnew(EditorFileDialog);
	add_child(file_dialog);
	file_dialog->connect("file_selected", callable_mp(this, &ScriptEditor::_file_dialog_action));

	error_dialog = memnew(AcceptDialog);
	add_child(error_dialog);

	disk_changed = memnew(ConfirmationDialog);
	{
		disk_changed->set_title(TTR("Files have been modified on disk"));

		VBoxContainer *vbc = memnew(VBoxContainer);
		disk_changed->add_child(vbc);

		Label *files_are_newer_label = memnew(Label);
		files_are_newer_label->set_text(TTR("The following files are newer on disk."));
		vbc->add_child(files_are_newer_label);

		Label *what_action_label = memnew(Label);
		what_action_label->set_text(TTR("What action should be taken?:"));
		vbc->add_child(what_action_label);

		disk_changed_list = memnew(Tree);
		vbc->add_child(disk_changed_list);
		disk_changed_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		disk_changed_list->set_v_size_flags(SIZE_EXPAND_FILL);

		disk_changed->connect(SceneStringName(confirmed), callable_mp(this, &ScriptEditor::reload_scripts).bind(false));
		disk_changed->set_ok_button_text(TTR("Discard local changes and reload"));

		disk_changed->add_button(TTR("Keep local changes and overwrite"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "resave");
		disk_changed->connect("custom_action", callable_mp(this, &ScriptEditor::_resave_scripts));
	}

	add_child(disk_changed);

	script_editor = this;

	autosave_timer = memnew(Timer);
	autosave_timer->set_one_shot(false);
	autosave_timer->connect(SceneStringName(tree_entered), callable_mp(this, &ScriptEditor::_update_autosave_timer));
	autosave_timer->connect("timeout", callable_mp(this, &ScriptEditor::_autosave_scripts));
	add_child(autosave_timer);

	grab_focus_block = false;

	help_search_dialog = memnew(EditorHelpSearch);
	add_child(help_search_dialog);
	help_search_dialog->connect("go_to_help", callable_mp(this, &ScriptEditor::_help_class_goto));

	find_in_files_dialog = memnew(FindInFilesDialog);
	find_in_files_dialog->connect(FindInFilesDialog::SIGNAL_FIND_REQUESTED, callable_mp(this, &ScriptEditor::_start_find_in_files).bind(false));
	find_in_files_dialog->connect(FindInFilesDialog::SIGNAL_REPLACE_REQUESTED, callable_mp(this, &ScriptEditor::_start_find_in_files).bind(true));
	add_child(find_in_files_dialog);
	find_in_files = memnew(FindInFilesPanel);
	find_in_files_button = EditorNode::get_bottom_panel()->add_item(TTR("Search Results"), find_in_files, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_search_results_bottom_panel", TTR("Toggle Search Results Bottom Panel")));
	find_in_files->set_custom_minimum_size(Size2(0, 200) * EDSCALE);
	find_in_files->connect(FindInFilesPanel::SIGNAL_RESULT_SELECTED, callable_mp(this, &ScriptEditor::_on_find_in_files_result_selected));
	find_in_files->connect(FindInFilesPanel::SIGNAL_FILES_MODIFIED, callable_mp(this, &ScriptEditor::_on_find_in_files_modified_files));
	find_in_files->connect(FindInFilesPanel::SIGNAL_CLOSE_BUTTON_CLICKED, callable_mp(this, &ScriptEditor::_on_find_in_files_close_button_clicked));
	find_in_files->hide();
	find_in_files_button->hide();

	history_pos = -1;

	edit_pass = 0;
	trim_trailing_whitespace_on_save = EDITOR_GET("text_editor/behavior/files/trim_trailing_whitespace_on_save");
	trim_final_newlines_on_save = EDITOR_GET("text_editor/behavior/files/trim_final_newlines_on_save");
	convert_indent_on_save = EDITOR_GET("text_editor/behavior/files/convert_indent_on_save");

	ScriptServer::edit_request_func = _open_script_request;

	Ref<EditorJSONSyntaxHighlighter> json_syntax_highlighter;
	json_syntax_highlighter.instantiate();
	register_syntax_highlighter(json_syntax_highlighter);

	Ref<EditorMarkdownSyntaxHighlighter> markdown_syntax_highlighter;
	markdown_syntax_highlighter.instantiate();
	register_syntax_highlighter(markdown_syntax_highlighter);

	_update_online_doc();
}

ScriptEditor::~ScriptEditor() {
	memdelete(completion_cache);
}

void ScriptEditorPlugin::_focus_another_editor() {
	if (window_wrapper->get_window_enabled()) {
		ERR_FAIL_COND(last_editor.is_empty());
		EditorInterface::get_singleton()->set_main_screen_editor(last_editor);
	}
}

void ScriptEditorPlugin::_save_last_editor(const String &p_editor) {
	if (p_editor != get_name()) {
		last_editor = p_editor;
	}
}

void ScriptEditorPlugin::_window_visibility_changed(bool p_visible) {
	_focus_another_editor();
	if (p_visible) {
		script_editor->add_theme_style_override(SceneStringName(panel), script_editor->get_theme_stylebox("ScriptEditorPanelFloating", EditorStringName(EditorStyles)));
	} else {
		script_editor->add_theme_style_override(SceneStringName(panel), script_editor->get_theme_stylebox("ScriptEditorPanel", EditorStringName(EditorStyles)));
	}
}

void ScriptEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			connect("main_screen_changed", callable_mp(this, &ScriptEditorPlugin::_save_last_editor));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			disconnect("main_screen_changed", callable_mp(this, &ScriptEditorPlugin::_save_last_editor));
		} break;
	}
}

void ScriptEditorPlugin::edit(Object *p_object) {
	if (Object::cast_to<Script>(p_object)) {
		Script *p_script = Object::cast_to<Script>(p_object);
		String res_path = p_script->get_path().get_slice("::", 0);

		if (p_script->is_built_in() && !res_path.is_empty()) {
			if (ResourceLoader::get_resource_type(res_path) == "PackedScene") {
				if (!EditorNode::get_singleton()->is_scene_open(res_path)) {
					EditorNode::get_singleton()->load_scene(res_path);
				}
			} else {
				EditorNode::get_singleton()->load_resource(res_path);
			}
		}
		script_editor->edit(p_script);
	} else if (Object::cast_to<JSON>(p_object)) {
		script_editor->edit(Object::cast_to<JSON>(p_object));
	} else if (Object::cast_to<TextFile>(p_object)) {
		script_editor->edit(Object::cast_to<TextFile>(p_object));
	}
}

bool ScriptEditorPlugin::handles(Object *p_object) const {
	if (Object::cast_to<TextFile>(p_object)) {
		return true;
	}

	if (Object::cast_to<Script>(p_object)) {
		return true;
	}

	if (Object::cast_to<JSON>(p_object)) {
		return true;
	}

	return p_object->is_class("Script");
}

void ScriptEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		window_wrapper->show();
		script_editor->ensure_select_current();
	} else {
		window_wrapper->hide();
	}
}

void ScriptEditorPlugin::selected_notify() {
	script_editor->ensure_select_current();
	_focus_another_editor();
}

String ScriptEditorPlugin::get_unsaved_status(const String &p_for_scene) const {
	const PackedStringArray unsaved_scripts = script_editor->get_unsaved_scripts();
	if (unsaved_scripts.is_empty()) {
		return String();
	}

	PackedStringArray message;
	if (!p_for_scene.is_empty()) {
		PackedStringArray unsaved_built_in_scripts;

		const String scene_file = p_for_scene.get_file();
		for (const String &E : unsaved_scripts) {
			if (!E.is_resource_file() && E.contains(scene_file)) {
				unsaved_built_in_scripts.append(E);
			}
		}

		if (unsaved_built_in_scripts.is_empty()) {
			return String();
		} else {
			message.resize(unsaved_built_in_scripts.size() + 1);
			message.write[0] = TTR("There are unsaved changes in the following built-in script(s):");

			int i = 1;
			for (const String &E : unsaved_built_in_scripts) {
				message.write[i] = E.trim_suffix("(*)");
				i++;
			}
			return String("\n").join(message);
		}
	}

	message.resize(unsaved_scripts.size() + 1);
	message.write[0] = TTR("Save changes to the following script(s) before quitting?");

	int i = 1;
	for (const String &E : unsaved_scripts) {
		message.write[i] = E.trim_suffix("(*)");
		i++;
	}
	return String("\n").join(message);
}

void ScriptEditorPlugin::save_external_data() {
	if (!EditorNode::get_singleton()->is_exiting()) {
		script_editor->save_all_scripts();
	}
}

void ScriptEditorPlugin::apply_changes() {
	script_editor->apply_scripts();
}

void ScriptEditorPlugin::set_window_layout(Ref<ConfigFile> p_layout) {
	script_editor->set_window_layout(p_layout);

	if (EDITOR_GET("interface/multi_window/restore_windows_on_load") && window_wrapper->is_window_available() && p_layout->has_section_key("ScriptEditor", "window_rect")) {
		window_wrapper->restore_window_from_saved_position(
				p_layout->get_value("ScriptEditor", "window_rect", Rect2i()),
				p_layout->get_value("ScriptEditor", "window_screen", -1),
				p_layout->get_value("ScriptEditor", "window_screen_rect", Rect2i()));
	} else {
		window_wrapper->set_window_enabled(false);
	}
}

void ScriptEditorPlugin::get_window_layout(Ref<ConfigFile> p_layout) {
	script_editor->get_window_layout(p_layout);

	if (window_wrapper->get_window_enabled()) {
		p_layout->set_value("ScriptEditor", "window_rect", window_wrapper->get_window_rect());
		int screen = window_wrapper->get_window_screen();
		p_layout->set_value("ScriptEditor", "window_screen", screen);
		p_layout->set_value("ScriptEditor", "window_screen_rect", DisplayServer::get_singleton()->screen_get_usable_rect(screen));

	} else {
		if (p_layout->has_section_key("ScriptEditor", "window_rect")) {
			p_layout->erase_section_key("ScriptEditor", "window_rect");
		}
		if (p_layout->has_section_key("ScriptEditor", "window_screen")) {
			p_layout->erase_section_key("ScriptEditor", "window_screen");
		}
		if (p_layout->has_section_key("ScriptEditor", "window_screen_rect")) {
			p_layout->erase_section_key("ScriptEditor", "window_screen_rect");
		}
	}
}

void ScriptEditorPlugin::get_breakpoints(List<String> *p_breakpoints) {
	script_editor->get_breakpoints(p_breakpoints);
}

void ScriptEditorPlugin::edited_scene_changed() {
	script_editor->edited_scene_changed();
}

ScriptEditorPlugin::ScriptEditorPlugin() {
	ED_SHORTCUT("script_editor/reopen_closed_script", TTR("Reopen Closed Script"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::T);
	ED_SHORTCUT("script_editor/clear_recent", TTR("Clear Recent Scripts"));
	ED_SHORTCUT("script_editor/find_in_files", TTR("Find in Files"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::F);
	ED_SHORTCUT("script_editor/replace_in_files", TTR("Replace in Files"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::R);

	ED_SHORTCUT("script_text_editor/convert_to_uppercase", TTR("Uppercase"), KeyModifierMask::SHIFT | Key::F4);
	ED_SHORTCUT("script_text_editor/convert_to_lowercase", TTR("Lowercase"), KeyModifierMask::SHIFT | Key::F5);
	ED_SHORTCUT("script_text_editor/capitalize", TTR("Capitalize"), KeyModifierMask::SHIFT | Key::F6);

	window_wrapper = memnew(WindowWrapper);
	window_wrapper->set_window_title(vformat(TTR("%s - Godot Engine"), TTR("Script Editor")));
	window_wrapper->set_margins_enabled(true);

	script_editor = memnew(ScriptEditor(window_wrapper));
	Ref<Shortcut> make_floating_shortcut = ED_SHORTCUT_AND_COMMAND("script_editor/make_floating", TTR("Make Floating"));
	window_wrapper->set_wrapped_control(script_editor, make_floating_shortcut);

	EditorNode::get_singleton()->get_editor_main_screen()->get_control()->add_child(window_wrapper);
	window_wrapper->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	window_wrapper->hide();
	window_wrapper->connect("window_visibility_changed", callable_mp(this, &ScriptEditorPlugin::_window_visibility_changed));

	ScriptServer::set_reload_scripts_on_save(EDITOR_GET("text_editor/behavior/files/auto_reload_and_parse_scripts_on_save"));
}

ScriptEditorPlugin::~ScriptEditorPlugin() {
}
