/**************************************************************************/
/*  text_shader_editor.cpp                                                */
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

#include "text_shader_editor.h"

#include "core/config/project_settings.h"
#include "core/version_generated.gen.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/split_container.h"
#include "servers/rendering/shader_preprocessor.h"
#include "servers/rendering/shader_types.h"

/*** SHADER SYNTAX HIGHLIGHTER ****/

Dictionary GDShaderSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	Dictionary color_map;

	for (const Point2i &region : disabled_branch_regions) {
		if (p_line >= region.x && p_line <= region.y) {
			// When "color_regions[0].p_start_key.length() > 2",
			// disabled_branch_region causes color_region to break.
			// This should be seen as a temporary solution.
			CodeHighlighter::_get_line_syntax_highlighting_impl(p_line);

			Dictionary highlighter_info;
			highlighter_info["color"] = disabled_branch_color;

			color_map[0] = highlighter_info;
			return color_map;
		}
	}

	return CodeHighlighter::_get_line_syntax_highlighting_impl(p_line);
}

void GDShaderSyntaxHighlighter::add_disabled_branch_region(const Point2i &p_region) {
	ERR_FAIL_COND(p_region.x < 0);
	ERR_FAIL_COND(p_region.y < 0);

	for (int i = 0; i < disabled_branch_regions.size(); i++) {
		ERR_FAIL_COND_MSG(disabled_branch_regions[i].x == p_region.x, "Branch region with a start line '" + itos(p_region.x) + "' already exists.");
	}

	Point2i disabled_branch_region;
	disabled_branch_region.x = p_region.x;
	disabled_branch_region.y = p_region.y;
	disabled_branch_regions.push_back(disabled_branch_region);

	clear_highlighting_cache();
}

void GDShaderSyntaxHighlighter::clear_disabled_branch_regions() {
	disabled_branch_regions.clear();
	clear_highlighting_cache();
}

void GDShaderSyntaxHighlighter::set_disabled_branch_color(const Color &p_color) {
	disabled_branch_color = p_color;
	clear_highlighting_cache();
}

/*** SHADER SCRIPT EDITOR ****/

static bool saved_warnings_enabled = false;
static bool saved_treat_warning_as_errors = false;
static HashMap<ShaderWarning::Code, bool> saved_warnings;
static uint32_t saved_warning_flags = 0U;

void ShaderTextEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			if (is_visible_in_tree()) {
				_load_theme_settings();
				if (warnings.size() > 0 && last_compile_result == OK) {
					warnings_panel->clear();
					_update_warning_panel();
				}
			}
		} break;
	}
}

Ref<Shader> ShaderTextEditor::get_edited_shader() const {
	return shader;
}

Ref<ShaderInclude> ShaderTextEditor::get_edited_shader_include() const {
	return shader_inc;
}

void ShaderTextEditor::set_edited_shader(const Ref<Shader> &p_shader) {
	set_edited_shader(p_shader, p_shader->get_code());
}

void ShaderTextEditor::set_edited_shader(const Ref<Shader> &p_shader, const String &p_code) {
	if (shader == p_shader) {
		return;
	}
	if (shader.is_valid()) {
		shader->disconnect_changed(callable_mp(this, &ShaderTextEditor::_shader_changed));
	}
	shader = p_shader;
	shader_inc = Ref<ShaderInclude>();

	set_edited_code(p_code);

	if (shader.is_valid()) {
		shader->connect_changed(callable_mp(this, &ShaderTextEditor::_shader_changed));
	}
}

void ShaderTextEditor::set_edited_shader_include(const Ref<ShaderInclude> &p_shader_inc) {
	set_edited_shader_include(p_shader_inc, p_shader_inc->get_code());
}

void ShaderTextEditor::_shader_changed() {
	// This function is used for dependencies (include changing changes main shader and forces it to revalidate)
	if (block_shader_changed) {
		return;
	}
	dependencies_version++;
	_validate_script();
}

void ShaderTextEditor::set_edited_shader_include(const Ref<ShaderInclude> &p_shader_inc, const String &p_code) {
	if (shader_inc == p_shader_inc) {
		return;
	}
	if (shader_inc.is_valid()) {
		shader_inc->disconnect_changed(callable_mp(this, &ShaderTextEditor::_shader_changed));
	}
	shader_inc = p_shader_inc;
	shader = Ref<Shader>();

	set_edited_code(p_code);

	if (shader_inc.is_valid()) {
		shader_inc->connect_changed(callable_mp(this, &ShaderTextEditor::_shader_changed));
	}
}

void ShaderTextEditor::set_edited_code(const String &p_code) {
	_load_theme_settings();

	get_text_editor()->set_text(p_code);
	get_text_editor()->clear_undo_history();
	callable_mp((TextEdit *)get_text_editor(), &TextEdit::set_h_scroll).call_deferred(0);
	callable_mp((TextEdit *)get_text_editor(), &TextEdit::set_v_scroll).call_deferred(0);
	get_text_editor()->tag_saved_version();

	_validate_script();
	_line_col_changed();
}

void ShaderTextEditor::reload_text() {
	ERR_FAIL_COND(shader.is_null() && shader_inc.is_null());

	String code;
	if (shader.is_valid()) {
		code = shader->get_code();
	} else {
		code = shader_inc->get_code();
	}

	CodeEdit *te = get_text_editor();
	int column = te->get_caret_column();
	int row = te->get_caret_line();
	int h = te->get_h_scroll();
	int v = te->get_v_scroll();

	te->set_text(code);
	te->set_caret_line(row);
	te->set_caret_column(column);
	te->set_h_scroll(h);
	te->set_v_scroll(v);

	te->tag_saved_version();

	update_line_and_column();
}

void ShaderTextEditor::set_warnings_panel(RichTextLabel *p_warnings_panel) {
	warnings_panel = p_warnings_panel;
}

void ShaderTextEditor::_load_theme_settings() {
	CodeEdit *te = get_text_editor();
	Color updated_marked_line_color = EDITOR_GET("text_editor/theme/highlighting/mark_color");
	if (updated_marked_line_color != marked_line_color) {
		for (int i = 0; i < te->get_line_count(); i++) {
			if (te->get_line_background_color(i) == marked_line_color) {
				te->set_line_background_color(i, updated_marked_line_color);
			}
		}
		marked_line_color = updated_marked_line_color;
	}

	syntax_highlighter->set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	syntax_highlighter->set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	syntax_highlighter->set_function_color(EDITOR_GET("text_editor/theme/highlighting/function_color"));
	syntax_highlighter->set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/member_variable_color"));

	syntax_highlighter->clear_keyword_colors();

	const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	const Color control_flow_keyword_color = EDITOR_GET("text_editor/theme/highlighting/control_flow_keyword_color");

	List<String> keywords;
	ShaderLanguage::get_keyword_list(&keywords);

	for (const String &E : keywords) {
		if (ShaderLanguage::is_control_flow_keyword(E)) {
			syntax_highlighter->add_keyword_color(E, control_flow_keyword_color);
		} else {
			syntax_highlighter->add_keyword_color(E, keyword_color);
		}
	}

	List<String> pp_keywords;
	ShaderPreprocessor::get_keyword_list(&pp_keywords, false);

	for (const String &E : pp_keywords) {
		syntax_highlighter->add_keyword_color(E, control_flow_keyword_color);
	}

	// Colorize built-ins like `COLOR` differently to make them easier
	// to distinguish from keywords at a quick glance.

	List<String> built_ins;

	if (shader_inc.is_valid()) {
		for (int i = 0; i < RenderingServer::SHADER_MAX; i++) {
			for (const KeyValue<StringName, ShaderLanguage::FunctionInfo> &E : ShaderTypes::get_singleton()->get_functions(RenderingServer::ShaderMode(i))) {
				for (const KeyValue<StringName, ShaderLanguage::BuiltInInfo> &F : E.value.built_ins) {
					built_ins.push_back(F.key);
				}
			}

			const Vector<ShaderLanguage::ModeInfo> &modes = ShaderTypes::get_singleton()->get_modes(RenderingServer::ShaderMode(i));

			for (int j = 0; j < modes.size(); j++) {
				const ShaderLanguage::ModeInfo &mode_info = modes[j];

				if (!mode_info.options.is_empty()) {
					for (int k = 0; k < mode_info.options.size(); k++) {
						built_ins.push_back(String(mode_info.name) + "_" + String(mode_info.options[k]));
					}
				} else {
					built_ins.push_back(String(mode_info.name));
				}
			}
		}
	} else if (shader.is_valid()) {
		for (const KeyValue<StringName, ShaderLanguage::FunctionInfo> &E : ShaderTypes::get_singleton()->get_functions(RenderingServer::ShaderMode(shader->get_mode()))) {
			for (const KeyValue<StringName, ShaderLanguage::BuiltInInfo> &F : E.value.built_ins) {
				built_ins.push_back(F.key);
			}
		}

		const Vector<ShaderLanguage::ModeInfo> &modes = ShaderTypes::get_singleton()->get_modes(RenderingServer::ShaderMode(shader->get_mode()));

		for (int i = 0; i < modes.size(); i++) {
			const ShaderLanguage::ModeInfo &mode_info = modes[i];

			if (!mode_info.options.is_empty()) {
				for (int j = 0; j < mode_info.options.size(); j++) {
					built_ins.push_back(String(mode_info.name) + "_" + String(mode_info.options[j]));
				}
			} else {
				built_ins.push_back(String(mode_info.name));
			}
		}
	}

	const Color user_type_color = EDITOR_GET("text_editor/theme/highlighting/user_type_color");

	for (const String &E : built_ins) {
		syntax_highlighter->add_keyword_color(E, user_type_color);
	}

	// Colorize comments.
	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	syntax_highlighter->clear_color_regions();
	syntax_highlighter->add_color_region("/*", "*/", comment_color, false);
	syntax_highlighter->add_color_region("//", "", comment_color, true);

	const Color doc_comment_color = EDITOR_GET("text_editor/theme/highlighting/doc_comment_color");
	syntax_highlighter->add_color_region("/**", "*/", doc_comment_color, false);
	// "/**/" will be treated as the start of the "/**" region, this line is guaranteed to end the color_region.
	syntax_highlighter->add_color_region("/**/", "", comment_color, true);

	// Disabled preprocessor branches use translucent text color to be easier to distinguish from comments.
	syntax_highlighter->set_disabled_branch_color(Color(EDITOR_GET("text_editor/theme/highlighting/text_color")) * Color(1, 1, 1, 0.5));

	te->clear_comment_delimiters();
	te->add_comment_delimiter("/*", "*/", false);
	te->add_comment_delimiter("//", "", true);

	if (!te->has_auto_brace_completion_open_key("/*")) {
		te->add_auto_brace_completion_pair("/*", "*/");
	}

	// Colorize preprocessor include strings.
	const Color string_color = EDITOR_GET("text_editor/theme/highlighting/string_color");
	syntax_highlighter->add_color_region("\"", "\"", string_color, false);

	if (warnings_panel) {
		// Warnings panel.
		warnings_panel->add_theme_font_override("normal_font", EditorNode::get_singleton()->get_editor_theme()->get_font(SNAME("main"), EditorStringName(EditorFonts)));
		warnings_panel->add_theme_font_size_override("normal_font_size", EditorNode::get_singleton()->get_editor_theme()->get_font_size(SNAME("main_size"), EditorStringName(EditorFonts)));
	}

	syntax_highlighter->set_uint_suffix_enabled(true);
}

void ShaderTextEditor::_check_shader_mode() {
	String type = ShaderLanguage::get_shader_type(get_text_editor()->get_text());

	Shader::Mode mode;

	if (type == "canvas_item") {
		mode = Shader::MODE_CANVAS_ITEM;
	} else if (type == "particles") {
		mode = Shader::MODE_PARTICLES;
	} else if (type == "sky") {
		mode = Shader::MODE_SKY;
	} else if (type == "fog") {
		mode = Shader::MODE_FOG;
	} else {
		mode = Shader::MODE_SPATIAL;
	}

	if (shader->get_mode() != mode) {
		set_block_shader_changed(true);
		shader->set_code(get_text_editor()->get_text());
		set_block_shader_changed(false);
		_load_theme_settings();
	}
}

static ShaderLanguage::DataType _get_global_shader_uniform_type(const StringName &p_variable) {
	RS::GlobalShaderParameterType gvt = RS::get_singleton()->global_shader_parameter_get_type(p_variable);
	return (ShaderLanguage::DataType)RS::global_shader_uniform_type_get_shader_datatype(gvt);
}

static String complete_from_path;

static void _complete_include_paths_search(EditorFileSystemDirectory *p_efsd, List<ScriptLanguage::CodeCompletionOption> *r_options) {
	if (!p_efsd) {
		return;
	}
	for (int i = 0; i < p_efsd->get_file_count(); i++) {
		if (p_efsd->get_file_type(i) == SNAME("ShaderInclude")) {
			String path = p_efsd->get_file_path(i);
			if (path.begins_with(complete_from_path)) {
				path = path.replace_first(complete_from_path, "");
			}
			r_options->push_back(ScriptLanguage::CodeCompletionOption(path, ScriptLanguage::CODE_COMPLETION_KIND_FILE_PATH));
		}
	}
	for (int j = 0; j < p_efsd->get_subdir_count(); j++) {
		_complete_include_paths_search(p_efsd->get_subdir(j), r_options);
	}
}

static void _complete_include_paths(List<ScriptLanguage::CodeCompletionOption> *r_options) {
	_complete_include_paths_search(EditorFileSystem::get_singleton()->get_filesystem(), r_options);
}

void ShaderTextEditor::_code_complete_script(const String &p_code, List<ScriptLanguage::CodeCompletionOption> *r_options) {
	List<ScriptLanguage::CodeCompletionOption> pp_options;
	List<ScriptLanguage::CodeCompletionOption> pp_defines;
	ShaderPreprocessor preprocessor;
	String code;
	String resource_path = (shader.is_valid() ? shader->get_path() : shader_inc->get_path());
	complete_from_path = resource_path.get_base_dir();
	if (!complete_from_path.ends_with("/")) {
		complete_from_path += "/";
	}
	preprocessor.preprocess(p_code, resource_path, code, nullptr, nullptr, nullptr, nullptr, &pp_options, &pp_defines, _complete_include_paths);
	complete_from_path = String();
	if (pp_options.size()) {
		for (const ScriptLanguage::CodeCompletionOption &E : pp_options) {
			r_options->push_back(E);
		}
		return;
	}
	for (const ScriptLanguage::CodeCompletionOption &E : pp_defines) {
		r_options->push_back(E);
	}

	ShaderLanguage sl;
	String calltip;
	ShaderLanguage::ShaderCompileInfo comp_info;
	comp_info.global_shader_uniform_type_func = _get_global_shader_uniform_type;

	if (shader.is_null()) {
		comp_info.is_include = true;

		sl.complete(code, comp_info, r_options, calltip);
		get_text_editor()->set_code_hint(calltip);
		return;
	}
	_check_shader_mode();
	comp_info.functions = ShaderTypes::get_singleton()->get_functions(RenderingServer::ShaderMode(shader->get_mode()));
	comp_info.render_modes = ShaderTypes::get_singleton()->get_modes(RenderingServer::ShaderMode(shader->get_mode()));
	comp_info.shader_types = ShaderTypes::get_singleton()->get_types();

	sl.complete(code, comp_info, r_options, calltip);
	get_text_editor()->set_code_hint(calltip);
}

void ShaderTextEditor::_validate_script() {
	emit_signal(CoreStringName(script_changed)); // Ensure to notify that it changed, so it is applied

	String code;

	if (shader.is_valid()) {
		_check_shader_mode();
		code = shader->get_code();
	} else {
		code = shader_inc->get_code();
	}

	ShaderPreprocessor preprocessor;
	String code_pp;
	String error_pp;
	List<ShaderPreprocessor::FilePosition> err_positions;
	List<ShaderPreprocessor::Region> regions;
	String filename;
	if (shader.is_valid()) {
		filename = shader->get_path();
	} else if (shader_inc.is_valid()) {
		filename = shader_inc->get_path();
	}
	last_compile_result = preprocessor.preprocess(code, filename, code_pp, &error_pp, &err_positions, &regions);

	for (int i = 0; i < get_text_editor()->get_line_count(); i++) {
		get_text_editor()->set_line_background_color(i, Color(0, 0, 0, 0));
	}

	syntax_highlighter->clear_disabled_branch_regions();
	for (const ShaderPreprocessor::Region &region : regions) {
		if (!region.enabled) {
			if (filename != region.file) {
				continue;
			}
			syntax_highlighter->add_disabled_branch_region(Point2i(region.from_line, region.to_line));
		}
	}

	set_error("");
	set_error_count(0);

	if (last_compile_result != OK) {
		//preprocessor error
		ERR_FAIL_COND(err_positions.is_empty());

		String err_text = error_pp;
		int err_line = err_positions.front()->get().line;
		if (err_positions.size() == 1) {
			// Error in main file
			err_text = "error(" + itos(err_line) + "): " + err_text;
		} else {
			err_text = "error(" + itos(err_line) + ") in include " + err_positions.back()->get().file.get_file() + ":" + itos(err_positions.back()->get().line) + ": " + err_text;
			set_error_count(err_positions.size() - 1);
		}

		set_error(err_text);
		set_error_pos(err_line - 1, 0);
		for (int i = 0; i < get_text_editor()->get_line_count(); i++) {
			get_text_editor()->set_line_background_color(i, Color(0, 0, 0, 0));
		}
		get_text_editor()->set_line_background_color(err_line - 1, marked_line_color);

		set_warning_count(0);

	} else {
		ShaderLanguage sl;

		sl.enable_warning_checking(saved_warnings_enabled);
		uint32_t flags = saved_warning_flags;
		if (shader.is_null()) {
			if (flags & ShaderWarning::UNUSED_CONSTANT) {
				flags &= ~(ShaderWarning::UNUSED_CONSTANT);
			}
			if (flags & ShaderWarning::UNUSED_FUNCTION) {
				flags &= ~(ShaderWarning::UNUSED_FUNCTION);
			}
			if (flags & ShaderWarning::UNUSED_STRUCT) {
				flags &= ~(ShaderWarning::UNUSED_STRUCT);
			}
			if (flags & ShaderWarning::UNUSED_UNIFORM) {
				flags &= ~(ShaderWarning::UNUSED_UNIFORM);
			}
			if (flags & ShaderWarning::UNUSED_VARYING) {
				flags &= ~(ShaderWarning::UNUSED_VARYING);
			}
		}
		sl.set_warning_flags(flags);

		ShaderLanguage::ShaderCompileInfo comp_info;
		comp_info.global_shader_uniform_type_func = _get_global_shader_uniform_type;

		if (shader.is_null()) {
			comp_info.is_include = true;
		} else {
			Shader::Mode mode = shader->get_mode();
			comp_info.functions = ShaderTypes::get_singleton()->get_functions(RenderingServer::ShaderMode(mode));
			comp_info.render_modes = ShaderTypes::get_singleton()->get_modes(RenderingServer::ShaderMode(mode));
			comp_info.shader_types = ShaderTypes::get_singleton()->get_types();
		}

		code = code_pp;
		//compiler error
		last_compile_result = sl.compile(code, comp_info);

		if (last_compile_result != OK) {
			String err_text;
			int err_line;
			Vector<ShaderLanguage::FilePosition> include_positions = sl.get_include_positions();
			if (include_positions.size() > 1) {
				//error is in an include
				err_line = include_positions[0].line;
				err_text = "error(" + itos(err_line) + ") in include " + include_positions[include_positions.size() - 1].file + ":" + itos(include_positions[include_positions.size() - 1].line) + ": " + sl.get_error_text();
				set_error_count(include_positions.size() - 1);
			} else {
				err_line = sl.get_error_line();
				err_text = "error(" + itos(err_line) + "): " + sl.get_error_text();
				set_error_count(0);
			}
			set_error(err_text);
			set_error_pos(err_line - 1, 0);
			get_text_editor()->set_line_background_color(err_line - 1, marked_line_color);
		} else {
			set_error("");
		}

		if (warnings.size() > 0 || last_compile_result != OK) {
			warnings_panel->clear();
		}
		warnings.clear();
		for (List<ShaderWarning>::Element *E = sl.get_warnings_ptr(); E; E = E->next()) {
			warnings.push_back(E->get());
		}
		if (warnings.size() > 0 && last_compile_result == OK) {
			warnings.sort_custom<WarningsComparator>();
			_update_warning_panel();
		} else {
			set_warning_count(0);
		}
	}

	emit_signal(SNAME("script_validated"), last_compile_result == OK); // Notify that validation finished, to update the list of scripts
}

void ShaderTextEditor::_update_warning_panel() {
	int warning_count = 0;

	warnings_panel->push_table(2);
	for (const ShaderWarning &w : warnings) {
		if (warning_count == 0) {
			if (saved_treat_warning_as_errors) {
				String error_text = "error(" + itos(w.get_line()) + "): " + w.get_message() + " " + TTR("Warnings should be fixed to prevent errors.");
				set_error_pos(w.get_line() - 1, 0);
				set_error(error_text);
				get_text_editor()->set_line_background_color(w.get_line() - 1, marked_line_color);
			}
		}

		warning_count++;
		int line = w.get_line();

		// First cell.
		warnings_panel->push_cell();
		warnings_panel->push_color(warnings_panel->get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		if (line != -1) {
			warnings_panel->push_meta(line - 1);
			warnings_panel->add_text(vformat(TTR("Line %d (%s):"), line, w.get_name()));
			warnings_panel->pop(); // Meta goto.
		} else {
			warnings_panel->add_text(w.get_name() + ":");
		}
		warnings_panel->pop(); // Color.
		warnings_panel->pop(); // Cell.

		// Second cell.
		warnings_panel->push_cell();
		warnings_panel->add_text(w.get_message());
		warnings_panel->pop(); // Cell.
	}
	warnings_panel->pop(); // Table.

	set_warning_count(warning_count);
}

void ShaderTextEditor::_bind_methods() {
	ADD_SIGNAL(MethodInfo("script_validated", PropertyInfo(Variant::BOOL, "valid")));
}

ShaderTextEditor::ShaderTextEditor() {
	syntax_highlighter.instantiate();
	get_text_editor()->set_syntax_highlighter(syntax_highlighter);
}

/*** SCRIPT EDITOR ******/

void TextShaderEditor::_menu_option(int p_option) {
	code_editor->get_text_editor()->apply_ime();

	switch (p_option) {
		case EDIT_UNDO: {
			code_editor->get_text_editor()->undo();
		} break;
		case EDIT_REDO: {
			code_editor->get_text_editor()->redo();
		} break;
		case EDIT_CUT: {
			code_editor->get_text_editor()->cut();
		} break;
		case EDIT_COPY: {
			code_editor->get_text_editor()->copy();
		} break;
		case EDIT_PASTE: {
			code_editor->get_text_editor()->paste();
		} break;
		case EDIT_SELECT_ALL: {
			code_editor->get_text_editor()->select_all();
		} break;
		case EDIT_MOVE_LINE_UP: {
			code_editor->get_text_editor()->move_lines_up();
		} break;
		case EDIT_MOVE_LINE_DOWN: {
			code_editor->get_text_editor()->move_lines_down();
		} break;
		case EDIT_INDENT: {
			if (shader.is_null() && shader_inc.is_null()) {
				return;
			}
			code_editor->get_text_editor()->indent_lines();
		} break;
		case EDIT_UNINDENT: {
			if (shader.is_null() && shader_inc.is_null()) {
				return;
			}
			code_editor->get_text_editor()->unindent_lines();
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
		case EDIT_TOGGLE_WORD_WRAP: {
			TextEdit::LineWrappingMode wrap = code_editor->get_text_editor()->get_line_wrapping_mode();
			code_editor->get_text_editor()->set_line_wrapping_mode(wrap == TextEdit::LINE_WRAPPING_BOUNDARY ? TextEdit::LINE_WRAPPING_NONE : TextEdit::LINE_WRAPPING_BOUNDARY);
		} break;
		case EDIT_TOGGLE_COMMENT: {
			if (shader.is_null() && shader_inc.is_null()) {
				return;
			}
			code_editor->toggle_inline_comment("//");
		} break;
		case EDIT_COMPLETE: {
			code_editor->get_text_editor()->request_code_completion();
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
		case HELP_DOCS: {
			OS::get_singleton()->shell_open(vformat("%s/tutorials/shaders/shader_reference/index.html", VERSION_DOCS_URL));
		} break;
	}
	if (p_option != SEARCH_FIND && p_option != SEARCH_REPLACE && p_option != SEARCH_GOTO_LINE) {
		callable_mp((Control *)code_editor->get_text_editor(), &Control::grab_focus).call_deferred();
	}
}

void TextShaderEditor::_prepare_edit_menu() {
	const CodeEdit *tx = code_editor->get_text_editor();
	PopupMenu *popup = edit_menu->get_popup();
	popup->set_item_disabled(popup->get_item_index(EDIT_UNDO), !tx->has_undo());
	popup->set_item_disabled(popup->get_item_index(EDIT_REDO), !tx->has_redo());
}

void TextShaderEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			PopupMenu *popup = help_menu->get_popup();
			popup->set_item_icon(popup->get_item_index(HELP_DOCS), get_editor_theme_icon(SNAME("ExternalLink")));
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_IN: {
			_check_for_external_edit();
		} break;
	}
}

void TextShaderEditor::_editor_settings_changed() {
	if (!EditorThemeManager::is_generated_theme_outdated() &&
			!EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor") &&
			!EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor")) {
		return;
	}

	_apply_editor_settings();
}

void TextShaderEditor::_apply_editor_settings() {
	code_editor->update_editor_settings();

	trim_trailing_whitespace_on_save = EDITOR_GET("text_editor/behavior/files/trim_trailing_whitespace_on_save");
	trim_final_newlines_on_save = EDITOR_GET("text_editor/behavior/files/trim_final_newlines_on_save");
}

void TextShaderEditor::_show_warnings_panel(bool p_show) {
	warnings_panel->set_visible(p_show);
}

void TextShaderEditor::_warning_clicked(const Variant &p_line) {
	if (p_line.get_type() == Variant::INT) {
		code_editor->goto_line_centered(p_line.operator int64_t());
	}
}

void TextShaderEditor::_bind_methods() {
	ClassDB::bind_method("_show_warnings_panel", &TextShaderEditor::_show_warnings_panel);
	ClassDB::bind_method("_warning_clicked", &TextShaderEditor::_warning_clicked);

	ADD_SIGNAL(MethodInfo("validation_changed"));
}

void TextShaderEditor::ensure_select_current() {
}

void TextShaderEditor::goto_line_selection(int p_line, int p_begin, int p_end) {
	code_editor->goto_line_selection(p_line, p_begin, p_end);
}

void TextShaderEditor::_project_settings_changed() {
	_update_warnings(true);
}

void TextShaderEditor::_update_warnings(bool p_validate) {
	bool changed = false;

	bool warnings_enabled = GLOBAL_GET("debug/shader_language/warnings/enable").booleanize();
	if (warnings_enabled != saved_warnings_enabled) {
		saved_warnings_enabled = warnings_enabled;
		changed = true;
	}

	bool treat_warning_as_errors = GLOBAL_GET("debug/shader_language/warnings/treat_warnings_as_errors").booleanize();
	if (treat_warning_as_errors != saved_treat_warning_as_errors) {
		saved_treat_warning_as_errors = treat_warning_as_errors;
		changed = true;
	}

	bool update_flags = false;

	for (int i = 0; i < ShaderWarning::WARNING_MAX; i++) {
		ShaderWarning::Code code = (ShaderWarning::Code)i;
		bool value = GLOBAL_GET("debug/shader_language/warnings/" + ShaderWarning::get_name_from_code(code).to_lower());

		if (saved_warnings[code] != value) {
			saved_warnings[code] = value;
			update_flags = true;
			changed = true;
		}
	}

	if (update_flags) {
		saved_warning_flags = (uint32_t)ShaderWarning::get_flags_from_codemap(saved_warnings);
	}

	if (p_validate && changed && code_editor && code_editor->get_edited_shader().is_valid()) {
		code_editor->validate_script();
	}
}

void TextShaderEditor::_check_for_external_edit() {
	bool use_autoreload = bool(EDITOR_GET("text_editor/behavior/files/auto_reload_scripts_on_external_change"));

	if (shader_inc.is_valid()) {
		if (shader_inc->get_last_modified_time() != FileAccess::get_modified_time(shader_inc->get_path())) {
			if (use_autoreload) {
				_reload_shader_include_from_disk();
			} else {
				callable_mp((Window *)disk_changed, &Window::popup_centered).call_deferred(Size2i());
			}
		}
		return;
	}

	if (shader.is_null() || shader->is_built_in()) {
		return;
	}

	if (shader->get_last_modified_time() != FileAccess::get_modified_time(shader->get_path())) {
		if (use_autoreload) {
			_reload_shader_from_disk();
		} else {
			callable_mp((Window *)disk_changed, &Window::popup_centered).call_deferred(Size2i());
		}
	}
}

void TextShaderEditor::_reload_shader_from_disk() {
	Ref<Shader> rel_shader = ResourceLoader::load(shader->get_path(), shader->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_FAIL_COND(rel_shader.is_null());

	code_editor->set_block_shader_changed(true);
	shader->set_code(rel_shader->get_code());
	code_editor->set_block_shader_changed(false);
	shader->set_last_modified_time(rel_shader->get_last_modified_time());
	code_editor->reload_text();
}

void TextShaderEditor::_reload_shader_include_from_disk() {
	Ref<ShaderInclude> rel_shader_include = ResourceLoader::load(shader_inc->get_path(), shader_inc->get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_FAIL_COND(rel_shader_include.is_null());

	code_editor->set_block_shader_changed(true);
	shader_inc->set_code(rel_shader_include->get_code());
	code_editor->set_block_shader_changed(false);
	shader_inc->set_last_modified_time(rel_shader_include->get_last_modified_time());
	code_editor->reload_text();
}

void TextShaderEditor::_reload() {
	if (shader.is_valid()) {
		_reload_shader_from_disk();
	} else if (shader_inc.is_valid()) {
		_reload_shader_include_from_disk();
	}
}

void TextShaderEditor::edit_shader(const Ref<Shader> &p_shader) {
	if (p_shader.is_null() || !p_shader->is_text_shader()) {
		return;
	}

	if (shader == p_shader) {
		return;
	}

	shader = p_shader;
	shader_inc = Ref<ShaderInclude>();

	code_editor->set_edited_shader(shader);
}

void TextShaderEditor::edit_shader_include(const Ref<ShaderInclude> &p_shader_inc) {
	if (p_shader_inc.is_null()) {
		return;
	}

	if (shader_inc == p_shader_inc) {
		return;
	}

	shader_inc = p_shader_inc;
	shader = Ref<Shader>();

	code_editor->set_edited_shader_include(p_shader_inc);
}

void TextShaderEditor::save_external_data(const String &p_str) {
	if (shader.is_null() && shader_inc.is_null()) {
		disk_changed->hide();
		return;
	}

	if (trim_trailing_whitespace_on_save) {
		trim_trailing_whitespace();
	}

	if (trim_final_newlines_on_save) {
		trim_final_newlines();
	}

	apply_shaders();

	Ref<Shader> edited_shader = code_editor->get_edited_shader();
	if (edited_shader.is_valid()) {
		ResourceSaver::save(edited_shader);
	}
	if (shader.is_valid() && shader != edited_shader) {
		ResourceSaver::save(shader);
	}

	Ref<ShaderInclude> edited_shader_inc = code_editor->get_edited_shader_include();
	if (edited_shader_inc.is_valid()) {
		ResourceSaver::save(edited_shader_inc);
	}
	if (shader_inc.is_valid() && shader_inc != edited_shader_inc) {
		ResourceSaver::save(shader_inc);
	}
	code_editor->get_text_editor()->tag_saved_version();

	disk_changed->hide();
}

void TextShaderEditor::trim_trailing_whitespace() {
	code_editor->trim_trailing_whitespace();
}

void TextShaderEditor::trim_final_newlines() {
	code_editor->trim_final_newlines();
}

void TextShaderEditor::validate_script() {
	code_editor->_validate_script();
}

bool TextShaderEditor::is_unsaved() const {
	return code_editor->get_text_editor()->get_saved_version() != code_editor->get_text_editor()->get_version();
}

void TextShaderEditor::tag_saved_version() {
	code_editor->get_text_editor()->tag_saved_version();
}

void TextShaderEditor::apply_shaders() {
	String editor_code = code_editor->get_text_editor()->get_text();
	if (shader.is_valid()) {
		String shader_code = shader->get_code();
		if (shader_code != editor_code || dependencies_version != code_editor->get_dependencies_version()) {
			code_editor->set_block_shader_changed(true);
			shader->set_code(editor_code);
			code_editor->set_block_shader_changed(false);
			shader->set_edited(true);
		}
	}
	if (shader_inc.is_valid()) {
		String shader_inc_code = shader_inc->get_code();
		if (shader_inc_code != editor_code || dependencies_version != code_editor->get_dependencies_version()) {
			code_editor->set_block_shader_changed(true);
			shader_inc->set_code(editor_code);
			code_editor->set_block_shader_changed(false);
			shader_inc->set_edited(true);
		}
	}

	dependencies_version = code_editor->get_dependencies_version();
}

void TextShaderEditor::_text_edit_gui_input(const Ref<InputEvent> &ev) {
	Ref<InputEventMouseButton> mb = ev;

	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed()) {
			CodeEdit *tx = code_editor->get_text_editor();

			tx->apply_ime();

			Point2i pos = tx->get_line_column_at_pos(mb->get_global_position() - tx->get_global_position());
			int row = pos.y;
			int col = pos.x;
			tx->set_move_caret_on_right_click_enabled(EDITOR_GET("text_editor/behavior/navigation/move_caret_on_right_click"));

			if (tx->is_move_caret_on_right_click_enabled()) {
				tx->remove_secondary_carets();
				if (tx->has_selection()) {
					int from_line = tx->get_selection_from_line();
					int to_line = tx->get_selection_to_line();
					int from_column = tx->get_selection_from_column();
					int to_column = tx->get_selection_to_column();

					if (row < from_line || row > to_line || (row == from_line && col < from_column) || (row == to_line && col > to_column)) {
						// Right click is outside the selected text
						tx->deselect();
					}
				}
				if (!tx->has_selection()) {
					tx->set_caret_line(row, true, false, -1);
					tx->set_caret_column(col);
				}
			}
			_make_context_menu(tx->has_selection(), get_local_mouse_position());
		}
	}

	Ref<InputEventKey> k = ev;
	if (k.is_valid() && k->is_pressed() && k->is_action("ui_menu", true)) {
		CodeEdit *tx = code_editor->get_text_editor();
		tx->adjust_viewport_to_caret();
		_make_context_menu(tx->has_selection(), (get_global_transform().inverse() * tx->get_global_transform()).xform(tx->get_caret_draw_pos()));
		context_menu->grab_focus();
	}
}

void TextShaderEditor::_update_bookmark_list() {
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

void TextShaderEditor::_bookmark_item_pressed(int p_idx) {
	if (p_idx < 4) { // Any item before the separator.
		_menu_option(bookmarks_menu->get_item_id(p_idx));
	} else {
		code_editor->goto_line(bookmarks_menu->get_item_metadata(p_idx));
	}
}

void TextShaderEditor::_make_context_menu(bool p_selection, Vector2 p_position) {
	context_menu->clear();
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
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_comment"), EDIT_TOGGLE_COMMENT);
	context_menu->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_bookmark"), BOOKMARK_TOGGLE);

	context_menu->set_item_disabled(context_menu->get_item_index(EDIT_UNDO), !code_editor->get_text_editor()->has_undo());
	context_menu->set_item_disabled(context_menu->get_item_index(EDIT_REDO), !code_editor->get_text_editor()->has_redo());

	context_menu->set_position(get_screen_position() + p_position);
	context_menu->reset_size();
	context_menu->popup();
}

TextShaderEditor::TextShaderEditor() {
	_update_warnings(false);

	code_editor = memnew(ShaderTextEditor);

	code_editor->connect("script_validated", callable_mp(this, &TextShaderEditor::_script_validated));

	code_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	code_editor->add_theme_constant_override("separation", 0);
	code_editor->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);

	code_editor->connect("show_warnings_panel", callable_mp(this, &TextShaderEditor::_show_warnings_panel));
	code_editor->connect(CoreStringName(script_changed), callable_mp(this, &TextShaderEditor::apply_shaders));
	EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &TextShaderEditor::_editor_settings_changed));
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &TextShaderEditor::_project_settings_changed));

	code_editor->get_text_editor()->set_symbol_lookup_on_click_enabled(true);
	code_editor->get_text_editor()->set_context_menu_enabled(false);
	code_editor->get_text_editor()->set_draw_breakpoints_gutter(false);
	code_editor->get_text_editor()->set_draw_executing_lines_gutter(false);
	code_editor->get_text_editor()->connect(SceneStringName(gui_input), callable_mp(this, &TextShaderEditor::_text_edit_gui_input));

	code_editor->update_editor_settings();

	context_menu = memnew(PopupMenu);
	add_child(context_menu);
	context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &TextShaderEditor::_menu_option));

	VBoxContainer *main_container = memnew(VBoxContainer);
	main_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	HBoxContainer *hbc = memnew(HBoxContainer);

	edit_menu = memnew(MenuButton);
	edit_menu->set_shortcut_context(this);
	edit_menu->set_text(TTR("Edit"));
	edit_menu->set_switch_on_hover(true);
	edit_menu->connect("about_to_popup", callable_mp(this, &TextShaderEditor::_prepare_edit_menu));

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
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_comment"), EDIT_TOGGLE_COMMENT);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/duplicate_selection"), EDIT_DUPLICATE_SELECTION);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/duplicate_lines"), EDIT_DUPLICATE_LINES);
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/toggle_word_wrap"), EDIT_TOGGLE_WORD_WRAP);
	edit_menu->get_popup()->add_separator();
	edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("ui_text_completion_query"), EDIT_COMPLETE);
	edit_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TextShaderEditor::_menu_option));

	search_menu = memnew(MenuButton);
	search_menu->set_shortcut_context(this);
	search_menu->set_text(TTR("Search"));
	search_menu->set_switch_on_hover(true);

	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find"), SEARCH_FIND);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_next"), SEARCH_FIND_NEXT);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/find_previous"), SEARCH_FIND_PREV);
	search_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/replace"), SEARCH_REPLACE);
	search_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TextShaderEditor::_menu_option));

	MenuButton *goto_menu = memnew(MenuButton);
	goto_menu->set_shortcut_context(this);
	goto_menu->set_text(TTR("Go To"));
	goto_menu->set_switch_on_hover(true);
	goto_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TextShaderEditor::_menu_option));

	goto_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("script_text_editor/goto_line"), SEARCH_GOTO_LINE);
	goto_menu->get_popup()->add_separator();

	bookmarks_menu = memnew(PopupMenu);
	goto_menu->get_popup()->add_submenu_node_item(TTR("Bookmarks"), bookmarks_menu);
	_update_bookmark_list();
	bookmarks_menu->connect("about_to_popup", callable_mp(this, &TextShaderEditor::_update_bookmark_list));
	bookmarks_menu->connect("index_pressed", callable_mp(this, &TextShaderEditor::_bookmark_item_pressed));

	help_menu = memnew(MenuButton);
	help_menu->set_text(TTR("Help"));
	help_menu->set_switch_on_hover(true);
	help_menu->get_popup()->add_item(TTR("Online Docs"), HELP_DOCS);
	help_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &TextShaderEditor::_menu_option));

	add_child(main_container);
	main_container->add_child(hbc);
	hbc->add_child(search_menu);
	hbc->add_child(edit_menu);
	hbc->add_child(goto_menu);
	hbc->add_child(help_menu);
	hbc->add_theme_style_override(SceneStringName(panel), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SNAME("ScriptEditorPanel"), EditorStringName(EditorStyles)));

	VSplitContainer *editor_box = memnew(VSplitContainer);
	main_container->add_child(editor_box);
	editor_box->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	editor_box->set_v_size_flags(SIZE_EXPAND_FILL);
	editor_box->add_child(code_editor);

	FindReplaceBar *bar = memnew(FindReplaceBar);
	main_container->add_child(bar);
	bar->hide();
	code_editor->set_find_replace_bar(bar);

	warnings_panel = memnew(RichTextLabel);
	warnings_panel->set_custom_minimum_size(Size2(0, 100 * EDSCALE));
	warnings_panel->set_h_size_flags(SIZE_EXPAND_FILL);
	warnings_panel->set_meta_underline(true);
	warnings_panel->set_selection_enabled(true);
	warnings_panel->set_context_menu_enabled(true);
	warnings_panel->set_focus_mode(FOCUS_CLICK);
	warnings_panel->hide();
	warnings_panel->connect("meta_clicked", callable_mp(this, &TextShaderEditor::_warning_clicked));
	editor_box->add_child(warnings_panel);
	code_editor->set_warnings_panel(warnings_panel);

	goto_line_popup = memnew(GotoLinePopup);
	add_child(goto_line_popup);

	disk_changed = memnew(ConfirmationDialog);

	VBoxContainer *vbc = memnew(VBoxContainer);
	disk_changed->add_child(vbc);

	Label *dl = memnew(Label);
	dl->set_text(TTR("This shader has been modified on disk.\nWhat action should be taken?"));
	vbc->add_child(dl);

	disk_changed->connect(SceneStringName(confirmed), callable_mp(this, &TextShaderEditor::_reload));
	disk_changed->set_ok_button_text(TTR("Reload"));

	disk_changed->add_button(TTR("Resave"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "resave");
	disk_changed->connect("custom_action", callable_mp(this, &TextShaderEditor::save_external_data));

	add_child(disk_changed);

	_editor_settings_changed();
	code_editor->show_toggle_scripts_button(); // TODO: Disabled for now, because it doesn't work properly.
}
