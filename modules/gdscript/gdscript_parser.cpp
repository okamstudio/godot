/**************************************************************************/
/*  gdscript_parser.cpp                                                   */
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

#include "gdscript_parser.h"

#include "gdscript.h"
#include "gdscript_tokenizer_buffer.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/math_defs.h"
#include "scene/main/multiplayer_api.h"

#ifdef DEBUG_ENABLED
#include "core/os/os.h"
#include "core/string/string_builder.h"
#include "servers/text_server.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

// This function is used to determine that a type is "built-in" as opposed to native
// and custom classes. So `Variant::NIL` and `Variant::OBJECT` are excluded:
// `Variant::NIL` - `null` is literal, not a type.
// `Variant::OBJECT` - `Object` should be treated as a class, not as a built-in type.
static HashMap<StringName, Variant::Type> builtin_types;
Variant::Type GDScriptParser::get_builtin_type(const StringName &p_type) {
	if (unlikely(builtin_types.is_empty())) {
		for (int i = 0; i < Variant::VARIANT_MAX; i++) {
			Variant::Type type = (Variant::Type)i;
			if (type != Variant::NIL && type != Variant::OBJECT) {
				builtin_types[Variant::get_type_name(type)] = type;
			}
		}
	}

	if (builtin_types.has(p_type)) {
		return builtin_types[p_type];
	}
	return Variant::VARIANT_MAX;
}

#ifdef TOOLS_ENABLED
HashMap<String, String> GDScriptParser::theme_color_names;
#endif

HashMap<StringName, GDScriptParser::AnnotationInfo> GDScriptParser::valid_annotations;

void GDScriptParser::cleanup() {
	builtin_types.clear();
	valid_annotations.clear();
}

void GDScriptParser::get_annotation_list(List<MethodInfo> *r_annotations) const {
	for (const KeyValue<StringName, AnnotationInfo> &E : valid_annotations) {
		r_annotations->push_back(E.value.info);
	}
}

bool GDScriptParser::annotation_exists(const String &p_annotation_name) const {
	return valid_annotations.has(p_annotation_name);
}

GDScriptParser::GDScriptParser() {
	// Register valid annotations.
	if (unlikely(valid_annotations.is_empty())) {
		// Script annotations.
		register_annotation(MethodInfo("@tool"), AnnotationInfo::SCRIPT, &GDScriptParser::tool_annotation);
		register_annotation(MethodInfo("@icon", PropertyInfo(Variant::STRING, "icon_path")), AnnotationInfo::SCRIPT, &GDScriptParser::icon_annotation);
		register_annotation(MethodInfo("@static_unload"), AnnotationInfo::SCRIPT, &GDScriptParser::static_unload_annotation);
		// Onready annotation.
		register_annotation(MethodInfo("@onready"), AnnotationInfo::VARIABLE, &GDScriptParser::onready_annotation);
		// Export annotations.
		register_annotation(MethodInfo("@export"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_NONE, Variant::NIL>);
		register_annotation(MethodInfo("@export_enum", PropertyInfo(Variant::STRING, "names")), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_ENUM, Variant::NIL>, varray(), true);
		register_annotation(MethodInfo("@export_file", PropertyInfo(Variant::STRING, "filter")), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_FILE, Variant::STRING>, varray(""), true);
		register_annotation(MethodInfo("@export_dir"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_DIR, Variant::STRING>);
		register_annotation(MethodInfo("@export_global_file", PropertyInfo(Variant::STRING, "filter")), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_GLOBAL_FILE, Variant::STRING>, varray(""), true);
		register_annotation(MethodInfo("@export_global_dir"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_GLOBAL_DIR, Variant::STRING>);
		register_annotation(MethodInfo("@export_multiline"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_MULTILINE_TEXT, Variant::STRING>);
		register_annotation(MethodInfo("@export_placeholder", PropertyInfo(Variant::STRING, "placeholder")), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_PLACEHOLDER_TEXT, Variant::STRING>);
		register_annotation(MethodInfo("@export_range", PropertyInfo(Variant::FLOAT, "min"), PropertyInfo(Variant::FLOAT, "max"), PropertyInfo(Variant::FLOAT, "step"), PropertyInfo(Variant::STRING, "extra_hints")), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_RANGE, Variant::FLOAT>, varray(1.0, ""), true);
		register_annotation(MethodInfo("@export_exp_easing", PropertyInfo(Variant::STRING, "hints")), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_EXP_EASING, Variant::FLOAT>, varray(""), true);
		register_annotation(MethodInfo("@export_color_no_alpha"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_COLOR_NO_ALPHA, Variant::COLOR>);
		register_annotation(MethodInfo("@export_node_path", PropertyInfo(Variant::STRING, "type")), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_NODE_PATH_VALID_TYPES, Variant::NODE_PATH>, varray(""), true);
		register_annotation(MethodInfo("@export_flags", PropertyInfo(Variant::STRING, "names")), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_FLAGS, Variant::INT>, varray(), true);
		register_annotation(MethodInfo("@export_flags_2d_render"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_LAYERS_2D_RENDER, Variant::INT>);
		register_annotation(MethodInfo("@export_flags_2d_physics"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_LAYERS_2D_PHYSICS, Variant::INT>);
		register_annotation(MethodInfo("@export_flags_2d_navigation"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_LAYERS_2D_NAVIGATION, Variant::INT>);
		register_annotation(MethodInfo("@export_flags_3d_render"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_LAYERS_3D_RENDER, Variant::INT>);
		register_annotation(MethodInfo("@export_flags_3d_physics"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_LAYERS_3D_PHYSICS, Variant::INT>);
		register_annotation(MethodInfo("@export_flags_3d_navigation"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_LAYERS_3D_NAVIGATION, Variant::INT>);
		register_annotation(MethodInfo("@export_flags_avoidance"), AnnotationInfo::VARIABLE, &GDScriptParser::export_annotations<PROPERTY_HINT_LAYERS_AVOIDANCE, Variant::INT>);
		register_annotation(MethodInfo("@export_storage"), AnnotationInfo::VARIABLE, &GDScriptParser::export_storage_annotation);
		register_annotation(MethodInfo("@export_custom", PropertyInfo(Variant::INT, "hint", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CLASS_IS_ENUM, "PropertyHint"), PropertyInfo(Variant::STRING, "hint_string"), PropertyInfo(Variant::INT, "usage", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_CLASS_IS_BITFIELD, "PropertyUsageFlags")), AnnotationInfo::VARIABLE, &GDScriptParser::export_custom_annotation, varray(PROPERTY_USAGE_DEFAULT));
		register_annotation(MethodInfo("@export_tool_button", PropertyInfo(Variant::STRING, "text"), PropertyInfo(Variant::STRING, "icon")), AnnotationInfo::VARIABLE, &GDScriptParser::export_tool_button_annotation, varray(""));
		// Export grouping annotations.
		register_annotation(MethodInfo("@export_category", PropertyInfo(Variant::STRING, "name")), AnnotationInfo::STANDALONE, &GDScriptParser::export_group_annotations<PROPERTY_USAGE_CATEGORY>);
		register_annotation(MethodInfo("@export_group", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "prefix")), AnnotationInfo::STANDALONE, &GDScriptParser::export_group_annotations<PROPERTY_USAGE_GROUP>, varray(""));
		register_annotation(MethodInfo("@export_subgroup", PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::STRING, "prefix")), AnnotationInfo::STANDALONE, &GDScriptParser::export_group_annotations<PROPERTY_USAGE_SUBGROUP>, varray(""));
		// Warning annotations.
		register_annotation(MethodInfo("@warning_ignore", PropertyInfo(Variant::STRING, "warning")), AnnotationInfo::CLASS_LEVEL | AnnotationInfo::STATEMENT, &GDScriptParser::warning_ignore_annotation, varray(), true);
		register_annotation(MethodInfo("@warning_ignore_start", PropertyInfo(Variant::STRING, "warning")), AnnotationInfo::STANDALONE, &GDScriptParser::warning_ignore_region_annotations, varray(), true);
		register_annotation(MethodInfo("@warning_ignore_restore", PropertyInfo(Variant::STRING, "warning")), AnnotationInfo::STANDALONE, &GDScriptParser::warning_ignore_region_annotations, varray(), true);
		// Networking.
		register_annotation(MethodInfo("@rpc", PropertyInfo(Variant::STRING, "mode"), PropertyInfo(Variant::STRING, "sync"), PropertyInfo(Variant::STRING, "transfer_mode"), PropertyInfo(Variant::INT, "transfer_channel")), AnnotationInfo::FUNCTION, &GDScriptParser::rpc_annotation, varray("authority", "call_remote", "unreliable", 0));
	}

#ifdef DEBUG_ENABLED
	is_ignoring_warnings = !(bool)GLOBAL_GET("debug/gdscript/warnings/enable");
	for (int i = 0; i < GDScriptWarning::WARNING_MAX; i++) {
		warning_ignore_start_lines[i] = INT_MAX;
	}
#endif

#ifdef TOOLS_ENABLED
	if (unlikely(theme_color_names.is_empty())) {
		// Vectors.
		theme_color_names.insert("x", "axis_x_color");
		theme_color_names.insert("y", "axis_y_color");
		theme_color_names.insert("z", "axis_z_color");
		theme_color_names.insert("w", "axis_w_color");

		// Color.
		theme_color_names.insert("r", "axis_x_color");
		theme_color_names.insert("r8", "axis_x_color");
		theme_color_names.insert("g", "axis_y_color");
		theme_color_names.insert("g8", "axis_y_color");
		theme_color_names.insert("b", "axis_z_color");
		theme_color_names.insert("b8", "axis_z_color");
		theme_color_names.insert("a", "axis_w_color");
		theme_color_names.insert("a8", "axis_w_color");
	}
#endif
}

GDScriptParser::~GDScriptParser() {
	while (list != nullptr) {
		Node *element = list;
		list = list->next;
		memdelete(element);
	}
}

void GDScriptParser::clear() {
	GDScriptParser tmp;
	tmp = *this;
	*this = GDScriptParser();
}

void GDScriptParser::push_error(const String &p_message, const Node *p_origin) {
	// TODO: Improve error reporting by pointing at source code.
	// TODO: Errors might point at more than one place at once (e.g. show previous declaration).
	panic_mode = true;
	// TODO: Improve positional information.
	if (p_origin == nullptr) {
		errors.push_back({ p_message, previous.start_line, previous.start_column });
	} else {
		errors.push_back({ p_message, p_origin->start_line, p_origin->leftmost_column });
	}
}

#ifdef DEBUG_ENABLED
void GDScriptParser::push_warning(const Node *p_source, GDScriptWarning::Code p_code, const Vector<String> &p_symbols) {
	ERR_FAIL_NULL(p_source);
	ERR_FAIL_INDEX(p_code, GDScriptWarning::WARNING_MAX);

	if (is_ignoring_warnings) {
		return;
	}
	if (GLOBAL_GET("debug/gdscript/warnings/exclude_addons").booleanize() && script_path.begins_with("res://addons/")) {
		return;
	}
	GDScriptWarning::WarnLevel warn_level = (GDScriptWarning::WarnLevel)(int)GLOBAL_GET(GDScriptWarning::get_settings_path_from_code(p_code));
	if (warn_level == GDScriptWarning::IGNORE) {
		return;
	}

	PendingWarning pw;
	pw.source = p_source;
	pw.code = p_code;
	pw.treated_as_error = warn_level == GDScriptWarning::ERROR;
	pw.symbols = p_symbols;

	pending_warnings.push_back(pw);
}

void GDScriptParser::apply_pending_warnings() {
	for (const PendingWarning &pw : pending_warnings) {
		if (warning_ignored_lines[pw.code].has(pw.source->start_line)) {
			continue;
		}
		if (warning_ignore_start_lines[pw.code] <= pw.source->start_line) {
			continue;
		}

		GDScriptWarning warning;
		warning.code = pw.code;
		warning.symbols = pw.symbols;
		warning.start_line = pw.source->start_line;
		warning.end_line = pw.source->end_line;
		warning.leftmost_column = pw.source->leftmost_column;
		warning.rightmost_column = pw.source->rightmost_column;

		if (pw.treated_as_error) {
			push_error(warning.get_message() + String(" (Warning treated as error.)"), pw.source);
			continue;
		}

		List<GDScriptWarning>::Element *before = nullptr;
		for (List<GDScriptWarning>::Element *E = warnings.front(); E; E = E->next()) {
			if (E->get().start_line > warning.start_line) {
				break;
			}
			before = E;
		}
		if (before) {
			warnings.insert_after(before, warning);
		} else {
			warnings.push_front(warning);
		}
	}

	pending_warnings.clear();
}
#endif // DEBUG_ENABLED

void GDScriptParser::override_completion_context(const Node *p_for_node, CompletionType p_type, Node *p_node, int p_argument) {
	if (!for_completion) {
		return;
	}
	if (p_for_node == nullptr || completion_context.node != p_for_node) {
		return;
	}
	CompletionContext context;
	context.type = p_type;
	context.current_class = current_class;
	context.current_function = current_function;
	context.current_suite = current_suite;
	context.current_line = tokenizer->get_cursor_line();
	context.current_argument = p_argument;
	context.node = p_node;
	context.parser = this;
	completion_context = context;
}

void GDScriptParser::make_completion_context(CompletionType p_type, Node *p_node, int p_argument, bool p_force) {
	if (!for_completion || (!p_force && completion_context.type != COMPLETION_NONE)) {
		return;
	}
	if (previous.cursor_place != GDScriptTokenizerText::CURSOR_MIDDLE && previous.cursor_place != GDScriptTokenizerText::CURSOR_END && current.cursor_place == GDScriptTokenizerText::CURSOR_NONE) {
		return;
	}
	CompletionContext context;
	context.type = p_type;
	context.current_class = current_class;
	context.current_function = current_function;
	context.current_suite = current_suite;
	context.current_line = tokenizer->get_cursor_line();
	context.current_argument = p_argument;
	context.node = p_node;
	context.parser = this;
	completion_context = context;
}

void GDScriptParser::make_completion_context(CompletionType p_type, Variant::Type p_builtin_type, bool p_force) {
	if (!for_completion || (!p_force && completion_context.type != COMPLETION_NONE)) {
		return;
	}
	if (previous.cursor_place != GDScriptTokenizerText::CURSOR_MIDDLE && previous.cursor_place != GDScriptTokenizerText::CURSOR_END && current.cursor_place == GDScriptTokenizerText::CURSOR_NONE) {
		return;
	}
	CompletionContext context;
	context.type = p_type;
	context.current_class = current_class;
	context.current_function = current_function;
	context.current_suite = current_suite;
	context.current_line = tokenizer->get_cursor_line();
	context.builtin_type = p_builtin_type;
	context.parser = this;
	completion_context = context;
}

void GDScriptParser::push_completion_call(Node *p_call) {
	if (!for_completion) {
		return;
	}
	CompletionCall call;
	call.call = p_call;
	call.argument = 0;
	completion_call_stack.push_back(call);
	if (previous.cursor_place == GDScriptTokenizerText::CURSOR_MIDDLE || previous.cursor_place == GDScriptTokenizerText::CURSOR_END || current.cursor_place == GDScriptTokenizerText::CURSOR_BEGINNING) {
		completion_call = call;
	}
}

void GDScriptParser::pop_completion_call() {
	if (!for_completion) {
		return;
	}
	ERR_FAIL_COND_MSG(completion_call_stack.is_empty(), "Trying to pop empty completion call stack");
	completion_call_stack.pop_back();
}

void GDScriptParser::set_last_completion_call_arg(int p_argument) {
	if (!for_completion || passed_cursor) {
		return;
	}
	ERR_FAIL_COND_MSG(completion_call_stack.is_empty(), "Trying to set argument on empty completion call stack");
	completion_call_stack.back()->get().argument = p_argument;
}

Error GDScriptParser::parse(const String &p_source_code, const String &p_script_path, bool p_for_completion, bool p_parse_body) {
	clear();

	String source = p_source_code;
	int cursor_line = -1;
	int cursor_column = -1;
	for_completion = p_for_completion;
	parse_body = p_parse_body;

	int tab_size = 4;
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
		tab_size = EditorSettings::get_singleton()->get_setting("text_editor/behavior/indent/size");
	}
#endif // TOOLS_ENABLED

	if (p_for_completion) {
		// Remove cursor sentinel char.
		const Vector<String> lines = p_source_code.split("\n");
		cursor_line = 1;
		cursor_column = 1;
		for (int i = 0; i < lines.size(); i++) {
			bool found = false;
			const String &line = lines[i];
			for (int j = 0; j < line.size(); j++) {
				if (line[j] == char32_t(0xFFFF)) {
					found = true;
					break;
				} else if (line[j] == '\t') {
					cursor_column += tab_size - 1;
				}
				cursor_column++;
			}
			if (found) {
				break;
			}
			cursor_line++;
			cursor_column = 1;
		}

		source = source.replace_first(String::chr(0xFFFF), String());
	}

	GDScriptTokenizerText *text_tokenizer = memnew(GDScriptTokenizerText);
	text_tokenizer->set_source_code(source);

	tokenizer = text_tokenizer;

	tokenizer->set_cursor_position(cursor_line, cursor_column);
	script_path = p_script_path.simplify_path();
	current = tokenizer->scan();
	// Avoid error or newline as the first token.
	// The latter can mess with the parser when opening files filled exclusively with comments and newlines.
	while (current.type == GDScriptTokenizer::Token::ERROR || current.type == GDScriptTokenizer::Token::NEWLINE) {
		if (current.type == GDScriptTokenizer::Token::ERROR) {
			push_error(current.literal);
		}
		current = tokenizer->scan();
	}

#ifdef DEBUG_ENABLED
	// Warn about parsing an empty script file:
	if (current.type == GDScriptTokenizer::Token::TK_EOF) {
		// Create a dummy Node for the warning, pointing to the very beginning of the file
		Node *nd = alloc_node<PassNode>();
		nd->start_line = 1;
		nd->start_column = 0;
		nd->end_line = 1;
		nd->leftmost_column = 0;
		nd->rightmost_column = 0;
		push_warning(nd, GDScriptWarning::EMPTY_FILE);
	}
#endif

	push_multiline(false); // Keep one for the whole parsing.
	parse_program();
	pop_multiline();

#ifdef TOOLS_ENABLED
	comment_data = tokenizer->get_comments();
#endif

	memdelete(text_tokenizer);
	tokenizer = nullptr;

#ifdef DEBUG_ENABLED
	if (multiline_stack.size() > 0) {
		ERR_PRINT("Parser bug: Imbalanced multiline stack.");
	}
#endif

	if (errors.is_empty()) {
		return OK;
	} else {
		return ERR_PARSE_ERROR;
	}
}

Error GDScriptParser::parse_binary(const Vector<uint8_t> &p_binary, const String &p_script_path) {
	GDScriptTokenizerBuffer *buffer_tokenizer = memnew(GDScriptTokenizerBuffer);
	Error err = buffer_tokenizer->set_code_buffer(p_binary);

	if (err) {
		memdelete(buffer_tokenizer);
		return err;
	}

	tokenizer = buffer_tokenizer;
	script_path = p_script_path.simplify_path();
	current = tokenizer->scan();
	// Avoid error or newline as the first token.
	// The latter can mess with the parser when opening files filled exclusively with comments and newlines.
	while (current.type == GDScriptTokenizer::Token::ERROR || current.type == GDScriptTokenizer::Token::NEWLINE) {
		if (current.type == GDScriptTokenizer::Token::ERROR) {
			push_error(current.literal);
		}
		current = tokenizer->scan();
	}

	push_multiline(false); // Keep one for the whole parsing.
	parse_program();
	pop_multiline();

	memdelete(buffer_tokenizer);
	tokenizer = nullptr;

	if (errors.is_empty()) {
		return OK;
	} else {
		return ERR_PARSE_ERROR;
	}
}

GDScriptTokenizer::Token GDScriptParser::advance() {
	lambda_ended = false; // Empty marker since we're past the end in any case.

	if (current.type == GDScriptTokenizer::Token::TK_EOF) {
		ERR_FAIL_COND_V_MSG(current.type == GDScriptTokenizer::Token::TK_EOF, current, "GDScript parser bug: Trying to advance past the end of stream.");
	}
	if (for_completion && !completion_call_stack.is_empty()) {
		if (completion_call.call == nullptr && tokenizer->is_past_cursor()) {
			completion_call = completion_call_stack.back()->get();
			passed_cursor = true;
		}
	}
	previous = current;
	current = tokenizer->scan();
	while (current.type == GDScriptTokenizer::Token::ERROR) {
		push_error(current.literal);
		current = tokenizer->scan();
	}
	if (previous.type != GDScriptTokenizer::Token::DEDENT) { // `DEDENT` belongs to the next non-empty line.
		for (Node *n : nodes_in_progress) {
			update_extents(n);
		}
	}
	return previous;
}

bool GDScriptParser::match(GDScriptTokenizer::Token::Type p_token_type) {
	if (!check(p_token_type)) {
		return false;
	}
	advance();
	return true;
}

bool GDScriptParser::check(GDScriptTokenizer::Token::Type p_token_type) const {
	if (p_token_type == GDScriptTokenizer::Token::IDENTIFIER) {
		return current.is_identifier();
	}
	return current.type == p_token_type;
}

bool GDScriptParser::consume(GDScriptTokenizer::Token::Type p_token_type, const String &p_error_message) {
	if (match(p_token_type)) {
		return true;
	}
	push_error(p_error_message);
	return false;
}

bool GDScriptParser::is_at_end() const {
	return check(GDScriptTokenizer::Token::TK_EOF);
}

void GDScriptParser::synchronize() {
	panic_mode = false;
	while (!is_at_end()) {
		if (previous.type == GDScriptTokenizer::Token::NEWLINE || previous.type == GDScriptTokenizer::Token::SEMICOLON) {
			return;
		}

		switch (current.type) {
			case GDScriptTokenizer::Token::CLASS:
			case GDScriptTokenizer::Token::FUNC:
			case GDScriptTokenizer::Token::STATIC:
			case GDScriptTokenizer::Token::VAR:
			case GDScriptTokenizer::Token::CONST:
			case GDScriptTokenizer::Token::SIGNAL:
			//case GDScriptTokenizer::Token::IF: // Can also be inside expressions.
			case GDScriptTokenizer::Token::FOR:
			case GDScriptTokenizer::Token::WHILE:
			case GDScriptTokenizer::Token::MATCH:
			case GDScriptTokenizer::Token::RETURN:
			case GDScriptTokenizer::Token::ANNOTATION:
				return;
			default:
				// Do nothing.
				break;
		}

		advance();
	}
}

void GDScriptParser::push_multiline(bool p_state) {
	multiline_stack.push_back(p_state);
	tokenizer->set_multiline_mode(p_state);
	if (p_state) {
		// Consume potential whitespace tokens already waiting in line.
		while (current.type == GDScriptTokenizer::Token::NEWLINE || current.type == GDScriptTokenizer::Token::INDENT || current.type == GDScriptTokenizer::Token::DEDENT) {
			current = tokenizer->scan(); // Don't call advance() here, as we don't want to change the previous token.
		}
	}
}

void GDScriptParser::pop_multiline() {
	ERR_FAIL_COND_MSG(multiline_stack.is_empty(), "Parser bug: trying to pop from multiline stack without available value.");
	multiline_stack.pop_back();
	tokenizer->set_multiline_mode(multiline_stack.size() > 0 ? multiline_stack.back()->get() : false);
}

bool GDScriptParser::is_statement_end_token() const {
	return check(GDScriptTokenizer::Token::NEWLINE) || check(GDScriptTokenizer::Token::SEMICOLON) || check(GDScriptTokenizer::Token::TK_EOF);
}

bool GDScriptParser::is_statement_end() const {
	return lambda_ended || in_lambda || is_statement_end_token();
}

void GDScriptParser::end_statement(const String &p_context) {
	bool found = false;
	while (is_statement_end() && !is_at_end()) {
		// Remove sequential newlines/semicolons.
		if (is_statement_end_token()) {
			// Only consume if this is an actual token.
			advance();
		} else if (lambda_ended) {
			lambda_ended = false; // Consume this "token".
			found = true;
			break;
		} else {
			if (!found) {
				lambda_ended = true; // Mark the lambda as done since we found something else to end the statement.
				found = true;
			}
			break;
		}

		found = true;
	}
	if (!found && !is_at_end()) {
		push_error(vformat(R"(Expected end of statement after %s, found "%s" instead.)", p_context, current.get_name()));
	}
}

void GDScriptParser::parse_program() {
	head = alloc_node<ClassNode>();
	head->start_line = 1;
	head->end_line = 1;
	head->fqcn = GDScript::canonicalize_path(script_path);
	current_class = head;
	bool can_have_class_or_extends = true;

#define PUSH_PENDING_ANNOTATIONS_TO_HEAD                  \
	if (!annotation_stack.is_empty()) {                   \
		for (AnnotationNode * annot : annotation_stack) { \
			head->annotations.push_back(annot);           \
		}                                                 \
		annotation_stack.clear();                         \
	}

	while (!check(GDScriptTokenizer::Token::TK_EOF)) {
		if (match(GDScriptTokenizer::Token::ANNOTATION)) {
			AnnotationNode *annotation = parse_annotation(AnnotationInfo::SCRIPT | AnnotationInfo::CLASS_LEVEL | AnnotationInfo::STANDALONE);
			if (annotation != nullptr) {
				if (annotation->applies_to(AnnotationInfo::CLASS)) {
					// We do not know in advance what the annotation will be applied to: the `head` class or the subsequent inner class.
					// If we encounter `class_name`, `extends` or pure `SCRIPT` annotation, then it's `head`, otherwise it's an inner class.
					annotation_stack.push_back(annotation);
				} else if (annotation->applies_to(AnnotationInfo::SCRIPT)) {
					PUSH_PENDING_ANNOTATIONS_TO_HEAD;
					if (annotation->name == SNAME("@tool") || annotation->name == SNAME("@icon")) {
						// Some annotations need to be resolved and applied in the parser.
						annotation->apply(this, head, nullptr); // `head->outer == nullptr`.
					} else {
						head->annotations.push_back(annotation);
					}
				} else if (annotation->applies_to(AnnotationInfo::STANDALONE)) {
					if (previous.type != GDScriptTokenizer::Token::NEWLINE) {
						push_error(R"(Expected newline after a standalone annotation.)");
					}
					if (annotation->name == SNAME("@export_category") || annotation->name == SNAME("@export_group") || annotation->name == SNAME("@export_subgroup")) {
						head->add_member_group(annotation);
						// This annotation must appear after script-level annotations and `class_name`/`extends`,
						// so we stop looking for script-level stuff.
						can_have_class_or_extends = false;
						break;
					} else if (annotation->name == SNAME("@warning_ignore_start") || annotation->name == SNAME("@warning_ignore_restore")) {
						// Some annotations need to be resolved and applied in the parser.
						annotation->apply(this, nullptr, nullptr);
					} else {
						push_error(R"(Unexpected standalone annotation.)");
					}
				} else {
					annotation_stack.push_back(annotation);
					// This annotation must appear after script-level annotations and `class_name`/`extends`,
					// so we stop looking for script-level stuff.
					can_have_class_or_extends = false;
					break;
				}
			}
		} else if (check(GDScriptTokenizer::Token::LITERAL) && current.literal.get_type() == Variant::STRING) {
			// Allow strings in class body as multiline comments.
			advance();
			if (!match(GDScriptTokenizer::Token::NEWLINE)) {
				push_error("Expected newline after comment string.");
			}
		} else {
			break;
		}
	}

	while (can_have_class_or_extends) {
		// Order here doesn't matter, but there should be only one of each at most.
		switch (current.type) {
			case GDScriptTokenizer::Token::CLASS_NAME:
				PUSH_PENDING_ANNOTATIONS_TO_HEAD;
				if (head->start_line == 1) {
					reset_extents(head, current);
				}
				advance();
				if (head->identifier != nullptr) {
					push_error(R"("class_name" can only be used once.)");
				} else {
					parse_class_name();
				}
				break;
			case GDScriptTokenizer::Token::EXTENDS:
				PUSH_PENDING_ANNOTATIONS_TO_HEAD;
				if (head->start_line == 1) {
					reset_extents(head, current);
				}
				advance();
				if (head->extends_used) {
					push_error(R"("extends" can only be used once.)");
				} else {
					parse_extends();
					end_statement("superclass");
				}
				break;
			case GDScriptTokenizer::Token::TK_EOF:
				PUSH_PENDING_ANNOTATIONS_TO_HEAD;
				can_have_class_or_extends = false;
				break;
			case GDScriptTokenizer::Token::LITERAL:
				if (current.literal.get_type() == Variant::STRING) {
					// Allow strings in class body as multiline comments.
					advance();
					if (!match(GDScriptTokenizer::Token::NEWLINE)) {
						push_error("Expected newline after comment string.");
					}
					break;
				}
				[[fallthrough]];
			default:
				// No tokens are allowed between script annotations and class/extends.
				can_have_class_or_extends = false;
				break;
		}

		if (panic_mode) {
			synchronize();
		}
	}

	// When the only thing needed is the class name and the icon, we don't need to parse the hole file.
	// It really speed up the call to GDScriptLanguage::get_global_class_name especially for large script.
	if (!parse_body) {
		return;
	}

#undef PUSH_PENDING_ANNOTATIONS_TO_HEAD

	parse_class_body(true);
	complete_extents(head);

#ifdef TOOLS_ENABLED
	const HashMap<int, GDScriptTokenizer::CommentData> &comments = tokenizer->get_comments();
	int line = MIN(max_script_doc_line, head->end_line);
	while (line > 0) {
		if (comments.has(line) && comments[line].new_line && comments[line].comment.begins_with("##")) {
			head->doc_data = parse_class_doc_comment(line);
			break;
		}
		line--;
	}

#endif // TOOLS_ENABLED

	if (!check(GDScriptTokenizer::Token::TK_EOF)) {
		push_error("Expected end of file.");
	}

	clear_unused_annotations();
}

Ref<GDScriptParserRef> GDScriptParser::get_depended_parser_for(const String &p_path) {
	Ref<GDScriptParserRef> ref;
	if (depended_parsers.has(p_path)) {
		ref = depended_parsers[p_path];
	} else {
		Error err = OK;
		ref = GDScriptCache::get_parser(p_path, GDScriptParserRef::EMPTY, err, script_path);
		if (ref.is_valid()) {
			depended_parsers[p_path] = ref;
		}
	}

	return ref;
}

const HashMap<String, Ref<GDScriptParserRef>> &GDScriptParser::get_depended_parsers() {
	return depended_parsers;
}

GDScriptParser::ClassNode *GDScriptParser::find_class(const String &p_qualified_name) const {
	String first = p_qualified_name.get_slice("::", 0);

	Vector<String> class_names;
	GDScriptParser::ClassNode *result = nullptr;
	// Empty initial name means start at the head.
	if (first.is_empty() || (head->identifier && first == head->identifier->name)) {
		class_names = p_qualified_name.split("::");
		result = head;
	} else if (p_qualified_name.begins_with(script_path)) {
		// Script path could have a class path separator("::") in it.
		class_names = p_qualified_name.trim_prefix(script_path).split("::");
		result = head;
	} else if (head->has_member(first)) {
		class_names = p_qualified_name.split("::");
		GDScriptParser::ClassNode::Member member = head->get_member(first);
		if (member.type == GDScriptParser::ClassNode::Member::CLASS) {
			result = member.m_class;
		}
	}

	// Starts at index 1 because index 0 was handled above.
	for (int i = 1; result != nullptr && i < class_names.size(); i++) {
		const String &current_name = class_names[i];
		GDScriptParser::ClassNode *next = nullptr;
		if (result->has_member(current_name)) {
			GDScriptParser::ClassNode::Member member = result->get_member(current_name);
			if (member.type == GDScriptParser::ClassNode::Member::CLASS) {
				next = member.m_class;
			}
		}
		result = next;
	}

	return result;
}

bool GDScriptParser::has_class(const GDScriptParser::ClassNode *p_class) const {
	if (head->fqcn.is_empty() && p_class->fqcn.get_slice("::", 0).is_empty()) {
		return p_class == head;
	} else if (p_class->fqcn.begins_with(head->fqcn)) {
		return find_class(p_class->fqcn.trim_prefix(head->fqcn)) == p_class;
	}

	return false;
}

GDScriptParser::ClassNode *GDScriptParser::parse_class(bool p_is_static) {
	ClassNode *n_class = alloc_node<ClassNode>();

	ClassNode *previous_class = current_class;
	current_class = n_class;
	n_class->outer = previous_class;

	if (consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected identifier for the class name after "class".)")) {
		n_class->identifier = parse_identifier();
		if (n_class->outer) {
			String fqcn = n_class->outer->fqcn;
			if (fqcn.is_empty()) {
				fqcn = GDScript::canonicalize_path(script_path);
			}
			n_class->fqcn = fqcn + "::" + n_class->identifier->name;
		} else {
			n_class->fqcn = n_class->identifier->name;
		}
	}

	if (match(GDScriptTokenizer::Token::EXTENDS)) {
		parse_extends();
	}

	consume(GDScriptTokenizer::Token::COLON, R"(Expected ":" after class declaration.)");

	bool multiline = match(GDScriptTokenizer::Token::NEWLINE);

	if (multiline && !consume(GDScriptTokenizer::Token::INDENT, R"(Expected indented block after class declaration.)")) {
		current_class = previous_class;
		complete_extents(n_class);
		return n_class;
	}

	if (match(GDScriptTokenizer::Token::EXTENDS)) {
		if (n_class->extends_used) {
			push_error(R"(Cannot use "extends" more than once in the same class.)");
		}
		parse_extends();
		end_statement("superclass");
	}

	parse_class_body(multiline);
	complete_extents(n_class);

	if (multiline) {
		consume(GDScriptTokenizer::Token::DEDENT, R"(Missing unindent at the end of the class body.)");
	}

	current_class = previous_class;
	return n_class;
}

void GDScriptParser::parse_class_name() {
	if (consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected identifier for the global class name after "class_name".)")) {
		current_class->identifier = parse_identifier();
		current_class->fqcn = String(current_class->identifier->name);
	}

	if (match(GDScriptTokenizer::Token::EXTENDS)) {
		// Allow extends on the same line.
		parse_extends();
		end_statement("superclass");
	} else {
		end_statement("class_name statement");
	}
}

void GDScriptParser::parse_extends() {
	current_class->extends_used = true;

	int chain_index = 0;

	if (match(GDScriptTokenizer::Token::LITERAL)) {
		if (previous.literal.get_type() != Variant::STRING) {
			push_error(vformat(R"(Only strings or identifiers can be used after "extends", found "%s" instead.)", Variant::get_type_name(previous.literal.get_type())));
		}
		current_class->extends_path = previous.literal;

		if (!match(GDScriptTokenizer::Token::PERIOD)) {
			return;
		}
	}

	make_completion_context(COMPLETION_INHERIT_TYPE, current_class, chain_index++);

	if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected superclass name after "extends".)")) {
		return;
	}
	current_class->extends.push_back(parse_identifier());

	while (match(GDScriptTokenizer::Token::PERIOD)) {
		make_completion_context(COMPLETION_INHERIT_TYPE, current_class, chain_index++);
		if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected superclass name after ".".)")) {
			return;
		}
		current_class->extends.push_back(parse_identifier());
	}
}

template <typename T>
void GDScriptParser::parse_class_member(T *(GDScriptParser::*p_parse_function)(bool), AnnotationInfo::TargetKind p_target, const String &p_member_kind, bool p_is_static) {
	advance();

	// Consume annotations.
	List<AnnotationNode *> annotations;
	while (!annotation_stack.is_empty()) {
		AnnotationNode *last_annotation = annotation_stack.back()->get();
		if (last_annotation->applies_to(p_target)) {
			annotations.push_front(last_annotation);
			annotation_stack.pop_back();
		} else {
			push_error(vformat(R"(Annotation "%s" cannot be applied to a %s.)", last_annotation->name, p_member_kind));
			clear_unused_annotations();
		}
	}

	T *member = (this->*p_parse_function)(p_is_static);
	if (member == nullptr) {
		return;
	}

#ifdef TOOLS_ENABLED
	int doc_comment_line = member->start_line - 1;
#endif // TOOLS_ENABLED

	for (AnnotationNode *&annotation : annotations) {
		member->annotations.push_back(annotation);
#ifdef TOOLS_ENABLED
		if (annotation->start_line <= doc_comment_line) {
			doc_comment_line = annotation->start_line - 1;
		}
#endif // TOOLS_ENABLED
	}

#ifdef TOOLS_ENABLED
	if constexpr (std::is_same_v<T, ClassNode>) {
		if (has_comment(member->start_line, true)) {
			// Inline doc comment.
			member->doc_data = parse_class_doc_comment(member->start_line, true);
		} else if (has_comment(doc_comment_line, true) && tokenizer->get_comments()[doc_comment_line].new_line) {
			// Normal doc comment. Don't check `min_member_doc_line` because a class ends parsing after its members.
			// This may not work correctly for cases like `var a; class B`, but it doesn't matter in practice.
			member->doc_data = parse_class_doc_comment(doc_comment_line);
		}
	} else {
		if (has_comment(member->start_line, true)) {
			// Inline doc comment.
			member->doc_data = parse_doc_comment(member->start_line, true);
		} else if (doc_comment_line >= min_member_doc_line && has_comment(doc_comment_line, true) && tokenizer->get_comments()[doc_comment_line].new_line) {
			// Normal doc comment.
			member->doc_data = parse_doc_comment(doc_comment_line);
		}
	}

	min_member_doc_line = member->end_line + 1; // Prevent multiple members from using the same doc comment.
#endif // TOOLS_ENABLED

	if (member->identifier != nullptr) {
		if (!((String)member->identifier->name).is_empty()) { // Enums may be unnamed.
			if (current_class->members_indices.has(member->identifier->name)) {
				push_error(vformat(R"(%s "%s" has the same name as a previously declared %s.)", p_member_kind.capitalize(), member->identifier->name, current_class->get_member(member->identifier->name).get_type_name()), member->identifier);
			} else {
				current_class->add_member(member);
			}
		} else {
			current_class->add_member(member);
		}
	}
}

void GDScriptParser::parse_class_body(bool p_is_multiline) {
	bool class_end = false;
	bool next_is_static = false;
	while (!class_end && !is_at_end()) {
		GDScriptTokenizer::Token token = current;
		switch (token.type) {
			case GDScriptTokenizer::Token::VAR:
				parse_class_member(&GDScriptParser::parse_variable, AnnotationInfo::VARIABLE, "variable", next_is_static);
				if (next_is_static) {
					current_class->has_static_data = true;
				}
				break;
			case GDScriptTokenizer::Token::CONST:
				parse_class_member(&GDScriptParser::parse_constant, AnnotationInfo::CONSTANT, "constant");
				break;
			case GDScriptTokenizer::Token::SIGNAL:
				parse_class_member(&GDScriptParser::parse_signal, AnnotationInfo::SIGNAL, "signal");
				break;
			case GDScriptTokenizer::Token::FUNC:
				parse_class_member(&GDScriptParser::parse_function, AnnotationInfo::FUNCTION, "function", next_is_static);
				break;
			case GDScriptTokenizer::Token::CLASS:
				parse_class_member(&GDScriptParser::parse_class, AnnotationInfo::CLASS, "class");
				break;
			case GDScriptTokenizer::Token::ENUM:
				parse_class_member(&GDScriptParser::parse_enum, AnnotationInfo::NONE, "enum");
				break;
			case GDScriptTokenizer::Token::STATIC: {
				advance();
				next_is_static = true;
				if (!check(GDScriptTokenizer::Token::FUNC) && !check(GDScriptTokenizer::Token::VAR)) {
					push_error(R"(Expected "func" or "var" after "static".)");
				}
			} break;
			case GDScriptTokenizer::Token::ANNOTATION: {
				advance();

				// Check for class-level and standalone annotations.
				AnnotationNode *annotation = parse_annotation(AnnotationInfo::CLASS_LEVEL | AnnotationInfo::STANDALONE);
				if (annotation != nullptr) {
					if (annotation->applies_to(AnnotationInfo::STANDALONE)) {
						if (previous.type != GDScriptTokenizer::Token::NEWLINE) {
							push_error(R"(Expected newline after a standalone annotation.)");
						}
						if (annotation->name == SNAME("@export_category") || annotation->name == SNAME("@export_group") || annotation->name == SNAME("@export_subgroup")) {
							current_class->add_member_group(annotation);
						} else if (annotation->name == SNAME("@warning_ignore_start") || annotation->name == SNAME("@warning_ignore_restore")) {
							// Some annotations need to be resolved and applied in the parser.
							annotation->apply(this, nullptr, nullptr);
						} else {
							push_error(R"(Unexpected standalone annotation.)");
						}
					} else { // `AnnotationInfo::CLASS_LEVEL`.
						annotation_stack.push_back(annotation);
					}
				}
				break;
			}
			case GDScriptTokenizer::Token::PASS:
				advance();
				end_statement(R"("pass")");
				break;
			case GDScriptTokenizer::Token::DEDENT:
				class_end = true;
				break;
			case GDScriptTokenizer::Token::LITERAL:
				if (current.literal.get_type() == Variant::STRING) {
					// Allow strings in class body as multiline comments.
					advance();
					if (!match(GDScriptTokenizer::Token::NEWLINE)) {
						push_error("Expected newline after comment string.");
					}
					break;
				}
				[[fallthrough]];
			default:
				// Display a completion with identifiers.
				make_completion_context(COMPLETION_IDENTIFIER, nullptr);
				advance();
				push_error(vformat(R"(Unexpected %s in class body.)", previous.get_debug_name()));
				break;
		}
		if (token.type != GDScriptTokenizer::Token::STATIC) {
			next_is_static = false;
		}
		if (panic_mode) {
			synchronize();
		}
		if (!p_is_multiline) {
			class_end = true;
		}
	}
}

GDScriptParser::VariableNode *GDScriptParser::parse_variable(bool p_is_static) {
	return parse_variable(p_is_static, true);
}

GDScriptParser::VariableNode *GDScriptParser::parse_variable(bool p_is_static, bool p_allow_property) {
	VariableNode *variable = alloc_node<VariableNode>();

	if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected variable name after "var".)")) {
		complete_extents(variable);
		return nullptr;
	}

	variable->identifier = parse_identifier();
	variable->export_info.name = variable->identifier->name;
	variable->is_static = p_is_static;

	if (match(GDScriptTokenizer::Token::COLON)) {
		if (check(GDScriptTokenizer::Token::NEWLINE)) {
			if (p_allow_property) {
				advance();
				return parse_property(variable, true);
			} else {
				push_error(R"(Expected type after ":")");
				complete_extents(variable);
				return nullptr;
			}
		} else if (check((GDScriptTokenizer::Token::EQUAL))) {
			// Infer type.
			variable->infer_datatype = true;
		} else {
			if (p_allow_property) {
				make_completion_context(COMPLETION_PROPERTY_DECLARATION_OR_TYPE, variable);
				if (check(GDScriptTokenizer::Token::IDENTIFIER)) {
					// Check if get or set.
					if (current.get_identifier() == "get" || current.get_identifier() == "set") {
						return parse_property(variable, false);
					}
				}
			}

			// Parse type.
			variable->datatype_specifier = parse_type();
		}
	}

	if (match(GDScriptTokenizer::Token::EQUAL)) {
		// Initializer.
		variable->initializer = parse_expression(false);
		if (variable->initializer == nullptr) {
			push_error(R"(Expected expression for variable initial value after "=".)");
		}
		variable->assignments++;
	}

	if (p_allow_property && match(GDScriptTokenizer::Token::COLON)) {
		if (match(GDScriptTokenizer::Token::NEWLINE)) {
			return parse_property(variable, true);
		} else {
			return parse_property(variable, false);
		}
	}

	complete_extents(variable);
	end_statement("variable declaration");

	return variable;
}

GDScriptParser::VariableNode *GDScriptParser::parse_property(VariableNode *p_variable, bool p_need_indent) {
	if (p_need_indent) {
		if (!consume(GDScriptTokenizer::Token::INDENT, R"(Expected indented block for property after ":".)")) {
			complete_extents(p_variable);
			return nullptr;
		}
	}

	VariableNode *property = p_variable;

	make_completion_context(COMPLETION_PROPERTY_DECLARATION, property);

	if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected "get" or "set" for property declaration.)")) {
		complete_extents(p_variable);
		return nullptr;
	}

	IdentifierNode *function = parse_identifier();

	if (check(GDScriptTokenizer::Token::EQUAL)) {
		p_variable->property = VariableNode::PROP_SETGET;
	} else {
		p_variable->property = VariableNode::PROP_INLINE;
		if (!p_need_indent) {
			push_error("Property with inline code must go to an indented block.");
		}
	}

	bool getter_used = false;
	bool setter_used = false;

	// Run with a loop because order doesn't matter.
	for (int i = 0; i < 2; i++) {
		if (function->name == SNAME("set")) {
			if (setter_used) {
				push_error(R"(Properties can only have one setter.)");
			} else {
				parse_property_setter(property);
				setter_used = true;
			}
		} else if (function->name == SNAME("get")) {
			if (getter_used) {
				push_error(R"(Properties can only have one getter.)");
			} else {
				parse_property_getter(property);
				getter_used = true;
			}
		} else {
			// TODO: Update message to only have the missing one if it's the case.
			push_error(R"(Expected "get" or "set" for property declaration.)");
		}

		if (i == 0 && p_variable->property == VariableNode::PROP_SETGET) {
			if (match(GDScriptTokenizer::Token::COMMA)) {
				// Consume potential newline.
				if (match(GDScriptTokenizer::Token::NEWLINE)) {
					if (!p_need_indent) {
						push_error(R"(Inline setter/getter setting cannot span across multiple lines (use "\\"" if needed).)");
					}
				}
			} else {
				break;
			}
		}

		if (!match(GDScriptTokenizer::Token::IDENTIFIER)) {
			break;
		}
		function = parse_identifier();
	}
	complete_extents(p_variable);

	if (p_variable->property == VariableNode::PROP_SETGET) {
		end_statement("property declaration");
	}

	if (p_need_indent) {
		consume(GDScriptTokenizer::Token::DEDENT, R"(Expected end of indented block for property.)");
	}
	return property;
}

void GDScriptParser::parse_property_setter(VariableNode *p_variable) {
	switch (p_variable->property) {
		case VariableNode::PROP_INLINE: {
			FunctionNode *function = alloc_node<FunctionNode>();
			IdentifierNode *identifier = alloc_node<IdentifierNode>();
			complete_extents(identifier);
			identifier->name = "@" + p_variable->identifier->name + "_setter";
			function->identifier = identifier;
			function->is_static = p_variable->is_static;

			consume(GDScriptTokenizer::Token::PARENTHESIS_OPEN, R"(Expected "(" after "set".)");

			ParameterNode *parameter = alloc_node<ParameterNode>();
			if (consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected parameter name after "(".)")) {
				reset_extents(parameter, previous);
				p_variable->setter_parameter = parse_identifier();
				parameter->identifier = p_variable->setter_parameter;
				function->parameters_indices[parameter->identifier->name] = 0;
				function->parameters.push_back(parameter);
			}
			complete_extents(parameter);

			consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected ")" after parameter name.)*");
			consume(GDScriptTokenizer::Token::COLON, R"*(Expected ":" after ")".)*");

			FunctionNode *previous_function = current_function;
			current_function = function;
			if (p_variable->setter_parameter != nullptr) {
				SuiteNode *body = alloc_node<SuiteNode>();
				body->add_local(parameter, function);
				function->body = parse_suite("setter declaration", body);
				p_variable->setter = function;
			}
			current_function = previous_function;
			complete_extents(function);
			break;
		}
		case VariableNode::PROP_SETGET:
			consume(GDScriptTokenizer::Token::EQUAL, R"(Expected "=" after "set")");
			make_completion_context(COMPLETION_PROPERTY_METHOD, p_variable);
			if (consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected setter function name after "=".)")) {
				p_variable->setter_pointer = parse_identifier();
			}
			break;
		case VariableNode::PROP_NONE:
			break; // Unreachable.
	}
}

void GDScriptParser::parse_property_getter(VariableNode *p_variable) {
	switch (p_variable->property) {
		case VariableNode::PROP_INLINE: {
			FunctionNode *function = alloc_node<FunctionNode>();

			if (match(GDScriptTokenizer::Token::PARENTHESIS_OPEN)) {
				consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected ")" after "get(".)*");
				consume(GDScriptTokenizer::Token::COLON, R"*(Expected ":" after "get()".)*");
			} else {
				consume(GDScriptTokenizer::Token::COLON, R"(Expected ":" or "(" after "get".)");
			}

			IdentifierNode *identifier = alloc_node<IdentifierNode>();
			complete_extents(identifier);
			identifier->name = "@" + p_variable->identifier->name + "_getter";
			function->identifier = identifier;
			function->is_static = p_variable->is_static;

			FunctionNode *previous_function = current_function;
			current_function = function;

			SuiteNode *body = alloc_node<SuiteNode>();
			function->body = parse_suite("getter declaration", body);
			p_variable->getter = function;

			current_function = previous_function;
			complete_extents(function);
			break;
		}
		case VariableNode::PROP_SETGET:
			consume(GDScriptTokenizer::Token::EQUAL, R"(Expected "=" after "get")");
			make_completion_context(COMPLETION_PROPERTY_METHOD, p_variable);
			if (consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected getter function name after "=".)")) {
				p_variable->getter_pointer = parse_identifier();
			}
			break;
		case VariableNode::PROP_NONE:
			break; // Unreachable.
	}
}

GDScriptParser::ConstantNode *GDScriptParser::parse_constant(bool p_is_static) {
	ConstantNode *constant = alloc_node<ConstantNode>();

	if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected constant name after "const".)")) {
		complete_extents(constant);
		return nullptr;
	}

	constant->identifier = parse_identifier();

	if (match(GDScriptTokenizer::Token::COLON)) {
		if (check((GDScriptTokenizer::Token::EQUAL))) {
			// Infer type.
			constant->infer_datatype = true;
		} else {
			// Parse type.
			constant->datatype_specifier = parse_type();
		}
	}

	if (consume(GDScriptTokenizer::Token::EQUAL, R"(Expected initializer after constant name.)")) {
		// Initializer.
		constant->initializer = parse_expression(false);

		if (constant->initializer == nullptr) {
			push_error(R"(Expected initializer expression for constant.)");
			complete_extents(constant);
			return nullptr;
		}
	} else {
		complete_extents(constant);
		return nullptr;
	}

	complete_extents(constant);
	end_statement("constant declaration");

	return constant;
}

GDScriptParser::ParameterNode *GDScriptParser::parse_parameter() {
	if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected parameter name.)")) {
		return nullptr;
	}

	ParameterNode *parameter = alloc_node<ParameterNode>();
	parameter->identifier = parse_identifier();

	if (match(GDScriptTokenizer::Token::COLON)) {
		if (check((GDScriptTokenizer::Token::EQUAL))) {
			// Infer type.
			parameter->infer_datatype = true;
		} else {
			// Parse type.
			make_completion_context(COMPLETION_TYPE_NAME, parameter);
			parameter->datatype_specifier = parse_type();
		}
	}

	if (match(GDScriptTokenizer::Token::EQUAL)) {
		// Default value.
		parameter->initializer = parse_expression(false);
	}

	complete_extents(parameter);
	return parameter;
}

GDScriptParser::SignalNode *GDScriptParser::parse_signal(bool p_is_static) {
	SignalNode *signal = alloc_node<SignalNode>();

	if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected signal name after "signal".)")) {
		complete_extents(signal);
		return nullptr;
	}

	signal->identifier = parse_identifier();

	if (check(GDScriptTokenizer::Token::PARENTHESIS_OPEN)) {
		push_multiline(true);
		advance();
		do {
			if (check(GDScriptTokenizer::Token::PARENTHESIS_CLOSE)) {
				// Allow for trailing comma.
				break;
			}

			ParameterNode *parameter = parse_parameter();
			if (parameter == nullptr) {
				push_error("Expected signal parameter name.");
				break;
			}
			if (parameter->initializer != nullptr) {
				push_error(R"(Signal parameters cannot have a default value.)");
			}
			if (signal->parameters_indices.has(parameter->identifier->name)) {
				push_error(vformat(R"(Parameter with name "%s" was already declared for this signal.)", parameter->identifier->name));
			} else {
				signal->parameters_indices[parameter->identifier->name] = signal->parameters.size();
				signal->parameters.push_back(parameter);
			}
		} while (match(GDScriptTokenizer::Token::COMMA) && !is_at_end());

		pop_multiline();
		consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected closing ")" after signal parameters.)*");
	}

	complete_extents(signal);
	end_statement("signal declaration");

	return signal;
}

GDScriptParser::EnumNode *GDScriptParser::parse_enum(bool p_is_static) {
	EnumNode *enum_node = alloc_node<EnumNode>();
	bool named = false;

	if (match(GDScriptTokenizer::Token::IDENTIFIER)) {
		enum_node->identifier = parse_identifier();
		named = true;
	}

	push_multiline(true);
	consume(GDScriptTokenizer::Token::BRACE_OPEN, vformat(R"(Expected "{" after %s.)", named ? "enum name" : R"("enum")"));
#ifdef TOOLS_ENABLED
	int min_enum_value_doc_line = previous.end_line + 1;
#endif

	HashMap<StringName, int> elements;

#ifdef DEBUG_ENABLED
	List<MethodInfo> gdscript_funcs;
	GDScriptLanguage::get_singleton()->get_public_functions(&gdscript_funcs);
#endif

	do {
		if (check(GDScriptTokenizer::Token::BRACE_CLOSE)) {
			break; // Allow trailing comma.
		}
		if (consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected identifier for enum key.)")) {
			GDScriptParser::IdentifierNode *identifier = parse_identifier();

			EnumNode::Value item;
			item.identifier = identifier;
			item.parent_enum = enum_node;
			item.line = previous.start_line;
			item.leftmost_column = previous.leftmost_column;
			item.rightmost_column = previous.rightmost_column;

			if (elements.has(item.identifier->name)) {
				push_error(vformat(R"(Name "%s" was already in this enum (at line %d).)", item.identifier->name, elements[item.identifier->name]), item.identifier);
			} else if (!named) {
				if (current_class->members_indices.has(item.identifier->name)) {
					push_error(vformat(R"(Name "%s" is already used as a class %s.)", item.identifier->name, current_class->get_member(item.identifier->name).get_type_name()));
				}
			}

			elements[item.identifier->name] = item.line;

			if (match(GDScriptTokenizer::Token::EQUAL)) {
				ExpressionNode *value = parse_expression(false);
				if (value == nullptr) {
					push_error(R"(Expected expression value after "=".)");
				}
				item.custom_value = value;
			}

			item.index = enum_node->values.size();
			enum_node->values.push_back(item);
			if (!named) {
				// Add as member of current class.
				current_class->add_member(item);
			}
		}
	} while (match(GDScriptTokenizer::Token::COMMA));

#ifdef TOOLS_ENABLED
	// Enum values documentation.
	for (int i = 0; i < enum_node->values.size(); i++) {
		int enum_value_line = enum_node->values[i].line;
		int doc_comment_line = enum_value_line - 1;

		MemberDocData doc_data;
		if (has_comment(enum_value_line, true)) {
			// Inline doc comment.
			if (i == enum_node->values.size() - 1 || enum_node->values[i + 1].line > enum_value_line) {
				doc_data = parse_doc_comment(enum_value_line, true);
			}
		} else if (doc_comment_line >= min_enum_value_doc_line && has_comment(doc_comment_line, true) && tokenizer->get_comments()[doc_comment_line].new_line) {
			// Normal doc comment.
			doc_data = parse_doc_comment(doc_comment_line);
		}

		if (named) {
			enum_node->values.write[i].doc_data = doc_data;
		} else {
			current_class->set_enum_value_doc_data(enum_node->values[i].identifier->name, doc_data);
		}

		min_enum_value_doc_line = enum_value_line + 1; // Prevent multiple enum values from using the same doc comment.
	}
#endif // TOOLS_ENABLED

	pop_multiline();
	consume(GDScriptTokenizer::Token::BRACE_CLOSE, R"(Expected closing "}" for enum.)");
	complete_extents(enum_node);
	end_statement("enum");

	return enum_node;
}

void GDScriptParser::parse_function_signature(FunctionNode *p_function, SuiteNode *p_body, const String &p_type) {
	if (!check(GDScriptTokenizer::Token::PARENTHESIS_CLOSE) && !is_at_end()) {
		bool default_used = false;
		do {
			if (check(GDScriptTokenizer::Token::PARENTHESIS_CLOSE)) {
				// Allow for trailing comma.
				break;
			}
			ParameterNode *parameter = parse_parameter();
			if (parameter == nullptr) {
				break;
			}
			if (parameter->initializer != nullptr) {
				default_used = true;
			} else {
				if (default_used) {
					push_error("Cannot have mandatory parameters after optional parameters.");
					continue;
				}
			}
			if (p_function->parameters_indices.has(parameter->identifier->name)) {
				push_error(vformat(R"(Parameter with name "%s" was already declared for this %s.)", parameter->identifier->name, p_type));
			} else {
				p_function->parameters_indices[parameter->identifier->name] = p_function->parameters.size();
				p_function->parameters.push_back(parameter);
				p_body->add_local(parameter, current_function);
			}
		} while (match(GDScriptTokenizer::Token::COMMA));
	}

	pop_multiline();
	consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, vformat(R"*(Expected closing ")" after %s parameters.)*", p_type));

	if (match(GDScriptTokenizer::Token::FORWARD_ARROW)) {
		make_completion_context(COMPLETION_TYPE_NAME_OR_VOID, p_function);
		p_function->return_type = parse_type(true);
		if (p_function->return_type == nullptr) {
			push_error(R"(Expected return type or "void" after "->".)");
		}
	}

	if (!p_function->source_lambda && p_function->identifier && p_function->identifier->name == GDScriptLanguage::get_singleton()->strings._static_init) {
		if (!p_function->is_static) {
			push_error(R"(Static constructor must be declared static.)");
		}
		if (p_function->parameters.size() != 0) {
			push_error(R"(Static constructor cannot have parameters.)");
		}
		current_class->has_static_data = true;
	}

	// TODO: Improve token consumption so it synchronizes to a statement boundary. This way we can get into the function body with unrecognized tokens.
	consume(GDScriptTokenizer::Token::COLON, vformat(R"(Expected ":" after %s declaration.)", p_type));
}

GDScriptParser::FunctionNode *GDScriptParser::parse_function(bool p_is_static) {
	FunctionNode *function = alloc_node<FunctionNode>();

	make_completion_context(COMPLETION_OVERRIDE_METHOD, function);

	if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected function name after "func".)")) {
		complete_extents(function);
		return nullptr;
	}

	FunctionNode *previous_function = current_function;
	current_function = function;

	function->identifier = parse_identifier();
	function->is_static = p_is_static;

	SuiteNode *body = alloc_node<SuiteNode>();
	SuiteNode *previous_suite = current_suite;
	current_suite = body;

	push_multiline(true);
	consume(GDScriptTokenizer::Token::PARENTHESIS_OPEN, R"(Expected opening "(" after function name.)");
	parse_function_signature(function, body, "function");

	current_suite = previous_suite;
	function->body = parse_suite("function declaration", body);

	current_function = previous_function;
	complete_extents(function);
	return function;
}

GDScriptParser::AnnotationNode *GDScriptParser::parse_annotation(uint32_t p_valid_targets) {
	AnnotationNode *annotation = alloc_node<AnnotationNode>();

	annotation->name = previous.literal;

	make_completion_context(COMPLETION_ANNOTATION, annotation);

	bool valid = true;

	if (!valid_annotations.has(annotation->name)) {
		if (annotation->name == "@deprecated") {
			push_error(R"("@deprecated" annotation does not exist. Use "## @deprecated: Reason here." instead.)");
		} else if (annotation->name == "@experimental") {
			push_error(R"("@experimental" annotation does not exist. Use "## @experimental: Reason here." instead.)");
		} else if (annotation->name == "@tutorial") {
			push_error(R"("@tutorial" annotation does not exist. Use "## @tutorial(Title): https://example.com" instead.)");
		} else {
			push_error(vformat(R"(Unrecognized annotation: "%s".)", annotation->name));
		}
		valid = false;
	}

	if (valid) {
		annotation->info = &valid_annotations[annotation->name];

		if (!annotation->applies_to(p_valid_targets)) {
			if (annotation->applies_to(AnnotationInfo::SCRIPT)) {
				push_error(vformat(R"(Annotation "%s" must be at the top of the script, before "extends" and "class_name".)", annotation->name));
			} else {
				push_error(vformat(R"(Annotation "%s" is not allowed in this level.)", annotation->name));
			}
			valid = false;
		}
	}

	if (check(GDScriptTokenizer::Token::PARENTHESIS_OPEN)) {
		push_multiline(true);
		advance();
		// Arguments.
		push_completion_call(annotation);
		int argument_index = 0;
		do {
			make_completion_context(COMPLETION_ANNOTATION_ARGUMENTS, annotation, argument_index);
			set_last_completion_call_arg(argument_index);
			if (check(GDScriptTokenizer::Token::PARENTHESIS_CLOSE)) {
				// Allow for trailing comma.
				break;
			}

			ExpressionNode *argument = parse_expression(false);

			if (argument == nullptr) {
				push_error("Expected expression as the annotation argument.");
				valid = false;
			} else {
				annotation->arguments.push_back(argument);

				if (argument->type == Node::LITERAL) {
					override_completion_context(argument, COMPLETION_ANNOTATION_ARGUMENTS, annotation, argument_index);
				}
			}

			argument_index++;
		} while (match(GDScriptTokenizer::Token::COMMA) && !is_at_end());

		pop_multiline();
		consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected ")" after annotation arguments.)*");
		pop_completion_call();
	}
	complete_extents(annotation);

	match(GDScriptTokenizer::Token::NEWLINE); // Newline after annotation is optional.

	if (valid) {
		valid = validate_annotation_arguments(annotation);
	}

	return valid ? annotation : nullptr;
}

void GDScriptParser::clear_unused_annotations() {
	for (const AnnotationNode *annotation : annotation_stack) {
		push_error(vformat(R"(Annotation "%s" does not precede a valid target, so it will have no effect.)", annotation->name), annotation);
	}

	annotation_stack.clear();
}

bool GDScriptParser::register_annotation(const MethodInfo &p_info, uint32_t p_target_kinds, AnnotationAction p_apply, const Vector<Variant> &p_default_arguments, bool p_is_vararg) {
	ERR_FAIL_COND_V_MSG(valid_annotations.has(p_info.name), false, vformat(R"(Annotation "%s" already registered.)", p_info.name));

	AnnotationInfo new_annotation;
	new_annotation.info = p_info;
	new_annotation.info.default_arguments = p_default_arguments;
	if (p_is_vararg) {
		new_annotation.info.flags |= METHOD_FLAG_VARARG;
	}
	new_annotation.apply = p_apply;
	new_annotation.target_kind = p_target_kinds;

	valid_annotations[p_info.name] = new_annotation;
	return true;
}

GDScriptParser::SuiteNode *GDScriptParser::parse_suite(const String &p_context, SuiteNode *p_suite, bool p_for_lambda) {
	SuiteNode *suite = p_suite != nullptr ? p_suite : alloc_node<SuiteNode>();
	suite->parent_block = current_suite;
	suite->parent_function = current_function;
	current_suite = suite;

	if (!p_for_lambda && suite->parent_block != nullptr && suite->parent_block->is_in_loop) {
		// Do not reset to false if true is set before calling parse_suite().
		suite->is_in_loop = true;
	}

	bool multiline = false;

	if (match(GDScriptTokenizer::Token::NEWLINE)) {
		multiline = true;
	}

	if (multiline) {
		if (!consume(GDScriptTokenizer::Token::INDENT, vformat(R"(Expected indented block after %s.)", p_context))) {
			current_suite = suite->parent_block;
			complete_extents(suite);
			return suite;
		}
	}
	reset_extents(suite, current);

	int error_count = 0;

	do {
		if (is_at_end() || (!multiline && previous.type == GDScriptTokenizer::Token::SEMICOLON && check(GDScriptTokenizer::Token::NEWLINE))) {
			break;
		}
		Node *statement = parse_statement();
		if (statement == nullptr) {
			if (error_count++ > 100) {
				push_error("Too many statement errors.", suite);
				break;
			}
			continue;
		}
		suite->statements.push_back(statement);

		// Register locals.
		switch (statement->type) {
			case Node::VARIABLE: {
				VariableNode *variable = static_cast<VariableNode *>(statement);
				const SuiteNode::Local &local = current_suite->get_local(variable->identifier->name);
				if (local.type != SuiteNode::Local::UNDEFINED) {
					push_error(vformat(R"(There is already a %s named "%s" declared in this scope.)", local.get_name(), variable->identifier->name), variable->identifier);
				}
				current_suite->add_local(variable, current_function);
				break;
			}
			case Node::CONSTANT: {
				ConstantNode *constant = static_cast<ConstantNode *>(statement);
				const SuiteNode::Local &local = current_suite->get_local(constant->identifier->name);
				if (local.type != SuiteNode::Local::UNDEFINED) {
					String name;
					if (local.type == SuiteNode::Local::CONSTANT) {
						name = "constant";
					} else {
						name = "variable";
					}
					push_error(vformat(R"(There is already a %s named "%s" declared in this scope.)", name, constant->identifier->name), constant->identifier);
				}
				current_suite->add_local(constant, current_function);
				break;
			}
			default:
				break;
		}

	} while ((multiline || previous.type == GDScriptTokenizer::Token::SEMICOLON) && !check(GDScriptTokenizer::Token::DEDENT) && !lambda_ended && !is_at_end());

	complete_extents(suite);

	if (multiline) {
		if (!lambda_ended) {
			consume(GDScriptTokenizer::Token::DEDENT, vformat(R"(Missing unindent at the end of %s.)", p_context));

		} else {
			match(GDScriptTokenizer::Token::DEDENT);
		}
	} else if (previous.type == GDScriptTokenizer::Token::SEMICOLON) {
		consume(GDScriptTokenizer::Token::NEWLINE, vformat(R"(Expected newline after ";" at the end of %s.)", p_context));
	}

	if (p_for_lambda) {
		lambda_ended = true;
	}
	current_suite = suite->parent_block;
	return suite;
}

GDScriptParser::Node *GDScriptParser::parse_statement() {
	Node *result = nullptr;
#ifdef DEBUG_ENABLED
	bool unreachable = current_suite->has_return && !current_suite->has_unreachable_code;
#endif

	List<AnnotationNode *> annotations;
	if (current.type != GDScriptTokenizer::Token::ANNOTATION) {
		while (!annotation_stack.is_empty()) {
			AnnotationNode *last_annotation = annotation_stack.back()->get();
			if (last_annotation->applies_to(AnnotationInfo::STATEMENT)) {
				annotations.push_front(last_annotation);
				annotation_stack.pop_back();
			} else {
				push_error(vformat(R"(Annotation "%s" cannot be applied to a statement.)", last_annotation->name));
				clear_unused_annotations();
			}
		}
	}

	switch (current.type) {
		case GDScriptTokenizer::Token::PASS:
			advance();
			result = alloc_node<PassNode>();
			complete_extents(result);
			end_statement(R"("pass")");
			break;
		case GDScriptTokenizer::Token::VAR:
			advance();
			result = parse_variable(false, false);
			break;
		case GDScriptTokenizer::Token::CONST:
			advance();
			result = parse_constant(false);
			break;
		case GDScriptTokenizer::Token::IF:
			advance();
			result = parse_if();
			break;
		case GDScriptTokenizer::Token::FOR:
			advance();
			result = parse_for();
			break;
		case GDScriptTokenizer::Token::WHILE:
			advance();
			result = parse_while();
			break;
		case GDScriptTokenizer::Token::MATCH:
			advance();
			result = parse_match();
			break;
		case GDScriptTokenizer::Token::BREAK:
			advance();
			result = parse_break();
			break;
		case GDScriptTokenizer::Token::CONTINUE:
			advance();
			result = parse_continue();
			break;
		case GDScriptTokenizer::Token::RETURN: {
			advance();
			ReturnNode *n_return = alloc_node<ReturnNode>();
			if (!is_statement_end()) {
				if (current_function && (current_function->identifier->name == GDScriptLanguage::get_singleton()->strings._init || current_function->identifier->name == GDScriptLanguage::get_singleton()->strings._static_init)) {
					push_error(R"(Constructor cannot return a value.)");
				}
				n_return->return_value = parse_expression(false);
			} else if (in_lambda && !is_statement_end_token()) {
				// Try to parse it anyway as this might not be the statement end in a lambda.
				// If this fails the expression will be nullptr, but that's the same as no return, so it's fine.
				n_return->return_value = parse_expression(false);
			}
			complete_extents(n_return);
			result = n_return;

			current_suite->has_return = true;

			end_statement("return statement");
			break;
		}
		case GDScriptTokenizer::Token::BREAKPOINT:
			advance();
			result = alloc_node<BreakpointNode>();
			complete_extents(result);
			end_statement(R"("breakpoint")");
			break;
		case GDScriptTokenizer::Token::ASSERT:
			advance();
			result = parse_assert();
			break;
		case GDScriptTokenizer::Token::ANNOTATION: {
			advance();
			AnnotationNode *annotation = parse_annotation(AnnotationInfo::STATEMENT | AnnotationInfo::STANDALONE);
			if (annotation != nullptr) {
				if (annotation->applies_to(AnnotationInfo::STANDALONE)) {
					if (previous.type != GDScriptTokenizer::Token::NEWLINE) {
						push_error(R"(Expected newline after a standalone annotation.)");
					}
					if (annotation->name == SNAME("@warning_ignore_start") || annotation->name == SNAME("@warning_ignore_restore")) {
						// Some annotations need to be resolved and applied in the parser.
						annotation->apply(this, nullptr, nullptr);
					} else {
						push_error(R"(Unexpected standalone annotation.)");
					}
				} else {
					annotation_stack.push_back(annotation);
				}
			}
			break;
		}
		default: {
			// Expression statement.
			ExpressionNode *expression = parse_expression(true); // Allow assignment here.
			bool has_ended_lambda = false;
			if (expression == nullptr) {
				if (in_lambda) {
					// If it's not a valid expression beginning, it might be the continuation of the outer expression where this lambda is.
					lambda_ended = true;
					has_ended_lambda = true;
				} else {
					advance();
					push_error(vformat(R"(Expected statement, found "%s" instead.)", previous.get_name()));
				}
			} else {
				end_statement("expression");
			}
			lambda_ended = lambda_ended || has_ended_lambda;
			result = expression;

#ifdef DEBUG_ENABLED
			if (expression != nullptr) {
				switch (expression->type) {
					case Node::ASSIGNMENT:
					case Node::AWAIT:
					case Node::CALL:
						// Fine.
						break;
					case Node::PRELOAD:
						// `preload` is a function-like keyword.
						push_warning(expression, GDScriptWarning::RETURN_VALUE_DISCARDED, "preload");
						break;
					case Node::LAMBDA:
						// Standalone lambdas can't be used, so make this an error.
						push_error("Standalone lambdas cannot be accessed. Consider assigning it to a variable.", expression);
						break;
					case Node::LITERAL:
						// Allow strings as multiline comments.
						if (static_cast<GDScriptParser::LiteralNode *>(expression)->value.get_type() != Variant::STRING) {
							push_warning(expression, GDScriptWarning::STANDALONE_EXPRESSION);
						}
						break;
					case Node::TERNARY_OPERATOR:
						push_warning(expression, GDScriptWarning::STANDALONE_TERNARY);
						break;
					default:
						push_warning(expression, GDScriptWarning::STANDALONE_EXPRESSION);
				}
			}
#endif
			break;
		}
	}

	if (result != nullptr && !annotations.is_empty()) {
		for (AnnotationNode *&annotation : annotations) {
			result->annotations.push_back(annotation);
		}
	}

#ifdef DEBUG_ENABLED
	if (unreachable && result != nullptr) {
		current_suite->has_unreachable_code = true;
		if (current_function) {
			push_warning(result, GDScriptWarning::UNREACHABLE_CODE, current_function->identifier ? current_function->identifier->name : "<anonymous lambda>");
		} else {
			// TODO: Properties setters and getters with unreachable code are not being warned
		}
	}
#endif

	if (panic_mode) {
		synchronize();
	}

	return result;
}

GDScriptParser::AssertNode *GDScriptParser::parse_assert() {
	// TODO: Add assert message.
	AssertNode *assert = alloc_node<AssertNode>();

	push_multiline(true);
	consume(GDScriptTokenizer::Token::PARENTHESIS_OPEN, R"(Expected "(" after "assert".)");

	assert->condition = parse_expression(false);
	if (assert->condition == nullptr) {
		push_error("Expected expression to assert.");
		pop_multiline();
		complete_extents(assert);
		return nullptr;
	}

	if (match(GDScriptTokenizer::Token::COMMA) && !check(GDScriptTokenizer::Token::PARENTHESIS_CLOSE)) {
		assert->message = parse_expression(false);
		if (assert->message == nullptr) {
			push_error(R"(Expected error message for assert after ",".)");
			pop_multiline();
			complete_extents(assert);
			return nullptr;
		}
		match(GDScriptTokenizer::Token::COMMA);
	}

	pop_multiline();
	consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected ")" after assert expression.)*");

	complete_extents(assert);
	end_statement(R"("assert")");

	return assert;
}

GDScriptParser::BreakNode *GDScriptParser::parse_break() {
	if (!can_break) {
		push_error(R"(Cannot use "break" outside of a loop.)");
	}
	BreakNode *break_node = alloc_node<BreakNode>();
	complete_extents(break_node);
	end_statement(R"("break")");
	return break_node;
}

GDScriptParser::ContinueNode *GDScriptParser::parse_continue() {
	if (!can_continue) {
		push_error(R"(Cannot use "continue" outside of a loop.)");
	}
	current_suite->has_continue = true;
	ContinueNode *cont = alloc_node<ContinueNode>();
	complete_extents(cont);
	end_statement(R"("continue")");
	return cont;
}

GDScriptParser::ForNode *GDScriptParser::parse_for() {
	ForNode *n_for = alloc_node<ForNode>();

	if (consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected loop variable name after "for".)")) {
		n_for->variable = parse_identifier();
	}

	if (match(GDScriptTokenizer::Token::COLON)) {
		n_for->datatype_specifier = parse_type();
		if (n_for->datatype_specifier == nullptr) {
			push_error(R"(Expected type specifier after ":".)");
		}
	}

	if (n_for->datatype_specifier == nullptr) {
		consume(GDScriptTokenizer::Token::IN, R"(Expected "in" or ":" after "for" variable name.)");
	} else {
		consume(GDScriptTokenizer::Token::IN, R"(Expected "in" after "for" variable type specifier.)");
	}

	n_for->list = parse_expression(false);

	if (!n_for->list) {
		push_error(R"(Expected iterable after "in".)");
	}

	consume(GDScriptTokenizer::Token::COLON, R"(Expected ":" after "for" condition.)");

	// Save break/continue state.
	bool could_break = can_break;
	bool could_continue = can_continue;

	// Allow break/continue.
	can_break = true;
	can_continue = true;

	SuiteNode *suite = alloc_node<SuiteNode>();
	if (n_for->variable) {
		const SuiteNode::Local &local = current_suite->get_local(n_for->variable->name);
		if (local.type != SuiteNode::Local::UNDEFINED) {
			push_error(vformat(R"(There is already a %s named "%s" declared in this scope.)", local.get_name(), n_for->variable->name), n_for->variable);
		}
		suite->add_local(SuiteNode::Local(n_for->variable, current_function));
	}
	suite->is_in_loop = true;
	n_for->loop = parse_suite(R"("for" block)", suite);
	complete_extents(n_for);

	// Reset break/continue state.
	can_break = could_break;
	can_continue = could_continue;

	return n_for;
}

GDScriptParser::IfNode *GDScriptParser::parse_if(const String &p_token) {
	IfNode *n_if = alloc_node<IfNode>();

	n_if->condition = parse_expression(false);
	if (n_if->condition == nullptr) {
		push_error(vformat(R"(Expected conditional expression after "%s".)", p_token));
	}

	consume(GDScriptTokenizer::Token::COLON, vformat(R"(Expected ":" after "%s" condition.)", p_token));

	n_if->true_block = parse_suite(vformat(R"("%s" block)", p_token));
	n_if->true_block->parent_if = n_if;

	if (n_if->true_block->has_continue) {
		current_suite->has_continue = true;
	}

	if (match(GDScriptTokenizer::Token::ELIF)) {
		SuiteNode *else_block = alloc_node<SuiteNode>();
		else_block->parent_function = current_function;
		else_block->parent_block = current_suite;

		SuiteNode *previous_suite = current_suite;
		current_suite = else_block;

		IfNode *elif = parse_if("elif");
		else_block->statements.push_back(elif);
		complete_extents(else_block);
		n_if->false_block = else_block;

		current_suite = previous_suite;
	} else if (match(GDScriptTokenizer::Token::ELSE)) {
		consume(GDScriptTokenizer::Token::COLON, R"(Expected ":" after "else".)");
		n_if->false_block = parse_suite(R"("else" block)");
	}
	complete_extents(n_if);

	if (n_if->false_block != nullptr && n_if->false_block->has_return && n_if->true_block->has_return) {
		current_suite->has_return = true;
	}
	if (n_if->false_block != nullptr && n_if->false_block->has_continue) {
		current_suite->has_continue = true;
	}

	return n_if;
}

GDScriptParser::MatchNode *GDScriptParser::parse_match() {
	MatchNode *match_node = alloc_node<MatchNode>();

	match_node->test = parse_expression(false);
	if (match_node->test == nullptr) {
		push_error(R"(Expected expression to test after "match".)");
	}

	consume(GDScriptTokenizer::Token::COLON, R"(Expected ":" after "match" expression.)");
	consume(GDScriptTokenizer::Token::NEWLINE, R"(Expected a newline after "match" statement.)");

	if (!consume(GDScriptTokenizer::Token::INDENT, R"(Expected an indented block after "match" statement.)")) {
		complete_extents(match_node);
		return match_node;
	}

	bool all_have_return = true;
	bool have_wildcard = false;

	List<AnnotationNode *> match_branch_annotation_stack;

	while (!check(GDScriptTokenizer::Token::DEDENT) && !is_at_end()) {
		if (match(GDScriptTokenizer::Token::PASS)) {
			consume(GDScriptTokenizer::Token::NEWLINE, R"(Expected newline after "pass".)");
			continue;
		}

		if (match(GDScriptTokenizer::Token::ANNOTATION)) {
			AnnotationNode *annotation = parse_annotation(AnnotationInfo::STATEMENT);
			if (annotation == nullptr) {
				continue;
			}
			if (annotation->name != SNAME("@warning_ignore")) {
				push_error(vformat(R"(Annotation "%s" is not allowed in this level.)", annotation->name), annotation);
				continue;
			}
			match_branch_annotation_stack.push_back(annotation);
			continue;
		}

		MatchBranchNode *branch = parse_match_branch();
		if (branch == nullptr) {
			advance();
			continue;
		}

		for (AnnotationNode *annotation : match_branch_annotation_stack) {
			branch->annotations.push_back(annotation);
		}
		match_branch_annotation_stack.clear();

#ifdef DEBUG_ENABLED
		if (have_wildcard && !branch->patterns.is_empty()) {
			push_warning(branch->patterns[0], GDScriptWarning::UNREACHABLE_PATTERN);
		}
#endif

		have_wildcard = have_wildcard || branch->has_wildcard;
		all_have_return = all_have_return && branch->block->has_return;
		match_node->branches.push_back(branch);
	}
	complete_extents(match_node);

	consume(GDScriptTokenizer::Token::DEDENT, R"(Expected an indented block after "match" statement.)");

	if (all_have_return && have_wildcard) {
		current_suite->has_return = true;
	}

	for (const AnnotationNode *annotation : match_branch_annotation_stack) {
		push_error(vformat(R"(Annotation "%s" does not precede a valid target, so it will have no effect.)", annotation->name), annotation);
	}
	match_branch_annotation_stack.clear();

	return match_node;
}

GDScriptParser::MatchBranchNode *GDScriptParser::parse_match_branch() {
	MatchBranchNode *branch = alloc_node<MatchBranchNode>();
	reset_extents(branch, current);

	bool has_bind = false;

	do {
		PatternNode *pattern = parse_match_pattern();
		if (pattern == nullptr) {
			continue;
		}
		if (pattern->binds.size() > 0) {
			has_bind = true;
		}
		if (branch->patterns.size() > 0 && has_bind) {
			push_error(R"(Cannot use a variable bind with multiple patterns.)");
		}
		if (pattern->pattern_type == PatternNode::PT_REST) {
			push_error(R"(Rest pattern can only be used inside array and dictionary patterns.)");
		} else if (pattern->pattern_type == PatternNode::PT_BIND || pattern->pattern_type == PatternNode::PT_WILDCARD) {
			branch->has_wildcard = true;
		}
		branch->patterns.push_back(pattern);
	} while (match(GDScriptTokenizer::Token::COMMA));

	if (branch->patterns.is_empty()) {
		push_error(R"(No pattern found for "match" branch.)");
	}

	bool has_guard = false;
	if (match(GDScriptTokenizer::Token::WHEN)) {
		// Pattern guard.
		// Create block for guard because it also needs to access the bound variables from patterns, and we don't want to add them to the outer scope.
		branch->guard_body = alloc_node<SuiteNode>();
		if (branch->patterns.size() > 0) {
			for (const KeyValue<StringName, IdentifierNode *> &E : branch->patterns[0]->binds) {
				SuiteNode::Local local(E.value, current_function);
				local.type = SuiteNode::Local::PATTERN_BIND;
				branch->guard_body->add_local(local);
			}
		}

		SuiteNode *parent_block = current_suite;
		branch->guard_body->parent_block = parent_block;
		current_suite = branch->guard_body;

		ExpressionNode *guard = parse_expression(false);
		if (guard == nullptr) {
			push_error(R"(Expected expression for pattern guard after "when".)");
		} else {
			branch->guard_body->statements.append(guard);
		}
		current_suite = parent_block;
		complete_extents(branch->guard_body);

		has_guard = true;
		branch->has_wildcard = false; // If it has a guard, the wildcard might still not match.
	}

	if (!consume(GDScriptTokenizer::Token::COLON, vformat(R"(Expected ":"%s after "match" %s.)", has_guard ? "" : R"( or "when")", has_guard ? "pattern guard" : "patterns"))) {
		complete_extents(branch);
		return nullptr;
	}

	SuiteNode *suite = alloc_node<SuiteNode>();
	if (branch->patterns.size() > 0) {
		for (const KeyValue<StringName, IdentifierNode *> &E : branch->patterns[0]->binds) {
			SuiteNode::Local local(E.value, current_function);
			local.type = SuiteNode::Local::PATTERN_BIND;
			suite->add_local(local);
		}
	}

	branch->block = parse_suite("match pattern block", suite);
	complete_extents(branch);

	return branch;
}

GDScriptParser::PatternNode *GDScriptParser::parse_match_pattern(PatternNode *p_root_pattern) {
	PatternNode *pattern = alloc_node<PatternNode>();
	reset_extents(pattern, current);

	switch (current.type) {
		case GDScriptTokenizer::Token::VAR: {
			// Bind.
			advance();
			if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected bind name after "var".)")) {
				complete_extents(pattern);
				return nullptr;
			}
			pattern->pattern_type = PatternNode::PT_BIND;
			pattern->bind = parse_identifier();

			PatternNode *root_pattern = p_root_pattern == nullptr ? pattern : p_root_pattern;

			if (p_root_pattern != nullptr) {
				if (p_root_pattern->has_bind(pattern->bind->name)) {
					push_error(vformat(R"(Bind variable name "%s" was already used in this pattern.)", pattern->bind->name));
					complete_extents(pattern);
					return nullptr;
				}
			}

			if (current_suite->has_local(pattern->bind->name)) {
				push_error(vformat(R"(There's already a %s named "%s" in this scope.)", current_suite->get_local(pattern->bind->name).get_name(), pattern->bind->name));
				complete_extents(pattern);
				return nullptr;
			}

			root_pattern->binds[pattern->bind->name] = pattern->bind;

		} break;
		case GDScriptTokenizer::Token::UNDERSCORE:
			// Wildcard.
			advance();
			pattern->pattern_type = PatternNode::PT_WILDCARD;
			break;
		case GDScriptTokenizer::Token::PERIOD_PERIOD:
			// Rest.
			advance();
			pattern->pattern_type = PatternNode::PT_REST;
			break;
		case GDScriptTokenizer::Token::BRACKET_OPEN: {
			// Array.
			push_multiline(true);
			advance();
			pattern->pattern_type = PatternNode::PT_ARRAY;
			do {
				if (is_at_end() || check(GDScriptTokenizer::Token::BRACKET_CLOSE)) {
					break;
				}
				PatternNode *sub_pattern = parse_match_pattern(p_root_pattern != nullptr ? p_root_pattern : pattern);
				if (sub_pattern == nullptr) {
					continue;
				}
				if (pattern->rest_used) {
					push_error(R"(The ".." pattern must be the last element in the pattern array.)");
				} else if (sub_pattern->pattern_type == PatternNode::PT_REST) {
					pattern->rest_used = true;
				}
				pattern->array.push_back(sub_pattern);
			} while (match(GDScriptTokenizer::Token::COMMA));
			consume(GDScriptTokenizer::Token::BRACKET_CLOSE, R"(Expected "]" to close the array pattern.)");
			pop_multiline();
			break;
		}
		case GDScriptTokenizer::Token::BRACE_OPEN: {
			// Dictionary.
			push_multiline(true);
			advance();
			pattern->pattern_type = PatternNode::PT_DICTIONARY;
			do {
				if (check(GDScriptTokenizer::Token::BRACE_CLOSE) || is_at_end()) {
					break;
				}
				if (match(GDScriptTokenizer::Token::PERIOD_PERIOD)) {
					// Rest.
					if (pattern->rest_used) {
						push_error(R"(The ".." pattern must be the last element in the pattern dictionary.)");
					} else {
						PatternNode *sub_pattern = alloc_node<PatternNode>();
						complete_extents(sub_pattern);
						sub_pattern->pattern_type = PatternNode::PT_REST;
						pattern->dictionary.push_back({ nullptr, sub_pattern });
						pattern->rest_used = true;
					}
				} else {
					ExpressionNode *key = parse_expression(false);
					if (key == nullptr) {
						push_error(R"(Expected expression as key for dictionary pattern.)");
					}
					if (match(GDScriptTokenizer::Token::COLON)) {
						// Value pattern.
						PatternNode *sub_pattern = parse_match_pattern(p_root_pattern != nullptr ? p_root_pattern : pattern);
						if (sub_pattern == nullptr) {
							continue;
						}
						if (pattern->rest_used) {
							push_error(R"(The ".." pattern must be the last element in the pattern dictionary.)");
						} else if (sub_pattern->pattern_type == PatternNode::PT_REST) {
							push_error(R"(The ".." pattern cannot be used as a value.)");
						} else {
							pattern->dictionary.push_back({ key, sub_pattern });
						}
					} else {
						// Key match only.
						pattern->dictionary.push_back({ key, nullptr });
					}
				}
			} while (match(GDScriptTokenizer::Token::COMMA));
			consume(GDScriptTokenizer::Token::BRACE_CLOSE, R"(Expected "}" to close the dictionary pattern.)");
			pop_multiline();
			break;
		}
		default: {
			// Expression.
			ExpressionNode *expression = parse_expression(false);
			if (expression == nullptr) {
				push_error(R"(Expected expression for match pattern.)");
				complete_extents(pattern);
				return nullptr;
			} else {
				if (expression->type == GDScriptParser::Node::LITERAL) {
					pattern->pattern_type = PatternNode::PT_LITERAL;
				} else {
					pattern->pattern_type = PatternNode::PT_EXPRESSION;
				}
				pattern->expression = expression;
			}
			break;
		}
	}
	complete_extents(pattern);

	return pattern;
}

bool GDScriptParser::PatternNode::has_bind(const StringName &p_name) {
	return binds.has(p_name);
}

GDScriptParser::IdentifierNode *GDScriptParser::PatternNode::get_bind(const StringName &p_name) {
	return binds[p_name];
}

GDScriptParser::WhileNode *GDScriptParser::parse_while() {
	WhileNode *n_while = alloc_node<WhileNode>();

	n_while->condition = parse_expression(false);
	if (n_while->condition == nullptr) {
		push_error(R"(Expected conditional expression after "while".)");
	}

	consume(GDScriptTokenizer::Token::COLON, R"(Expected ":" after "while" condition.)");

	// Save break/continue state.
	bool could_break = can_break;
	bool could_continue = can_continue;

	// Allow break/continue.
	can_break = true;
	can_continue = true;

	SuiteNode *suite = alloc_node<SuiteNode>();
	suite->is_in_loop = true;
	n_while->loop = parse_suite(R"("while" block)", suite);
	complete_extents(n_while);

	// Reset break/continue state.
	can_break = could_break;
	can_continue = could_continue;

	return n_while;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_precedence(Precedence p_precedence, bool p_can_assign, bool p_stop_on_assign) {
	// Switch multiline mode on for grouping tokens.
	// Do this early to avoid the tokenizer generating whitespace tokens.
	switch (current.type) {
		case GDScriptTokenizer::Token::PARENTHESIS_OPEN:
		case GDScriptTokenizer::Token::BRACE_OPEN:
		case GDScriptTokenizer::Token::BRACKET_OPEN:
			push_multiline(true);
			break;
		default:
			break; // Nothing to do.
	}

	// Completion can appear whenever an expression is expected.
	make_completion_context(COMPLETION_IDENTIFIER, nullptr, -1, false);

	GDScriptTokenizer::Token token = current;
	GDScriptTokenizer::Token::Type token_type = token.type;
	if (token.is_identifier()) {
		// Allow keywords that can be treated as identifiers.
		token_type = GDScriptTokenizer::Token::IDENTIFIER;
	}
	ParseFunction prefix_rule = get_rule(token_type)->prefix;

	if (prefix_rule == nullptr) {
		// Expected expression. Let the caller give the proper error message.
		return nullptr;
	}

	advance(); // Only consume the token if there's a valid rule.

	// After a token was consumed, update the completion context regardless of a previously set context.

	ExpressionNode *previous_operand = (this->*prefix_rule)(nullptr, p_can_assign);

#ifdef TOOLS_ENABLED
	// HACK: We can't create a context in parse_identifier since it is used in places were we don't want completion.
	if (previous_operand != nullptr && previous_operand->type == GDScriptParser::Node::IDENTIFIER && prefix_rule == static_cast<ParseFunction>(&GDScriptParser::parse_identifier)) {
		make_completion_context(COMPLETION_IDENTIFIER, previous_operand);
	}
#endif

	while (p_precedence <= get_rule(current.type)->precedence) {
		if (previous_operand == nullptr || (p_stop_on_assign && current.type == GDScriptTokenizer::Token::EQUAL) || lambda_ended) {
			return previous_operand;
		}
		// Also switch multiline mode on here for infix operators.
		switch (current.type) {
			// case GDScriptTokenizer::Token::BRACE_OPEN: // Not an infix operator.
			case GDScriptTokenizer::Token::PARENTHESIS_OPEN:
			case GDScriptTokenizer::Token::BRACKET_OPEN:
				push_multiline(true);
				break;
			default:
				break; // Nothing to do.
		}
		token = advance();
		ParseFunction infix_rule = get_rule(token.type)->infix;
		previous_operand = (this->*infix_rule)(previous_operand, p_can_assign);
	}

	return previous_operand;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_expression(bool p_can_assign, bool p_stop_on_assign) {
	return parse_precedence(PREC_ASSIGNMENT, p_can_assign, p_stop_on_assign);
}

GDScriptParser::IdentifierNode *GDScriptParser::parse_identifier() {
	IdentifierNode *identifier = static_cast<IdentifierNode *>(parse_identifier(nullptr, false));
#ifdef DEBUG_ENABLED
	// Check for spoofing here (if available in TextServer) since this isn't called inside expressions. This is only relevant for declarations.
	if (identifier && TS->has_feature(TextServer::FEATURE_UNICODE_SECURITY) && TS->spoof_check(identifier->name)) {
		push_warning(identifier, GDScriptWarning::CONFUSABLE_IDENTIFIER, identifier->name.operator String());
	}
#endif
	return identifier;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_identifier(ExpressionNode *p_previous_operand, bool p_can_assign) {
	if (!previous.is_identifier()) {
		ERR_FAIL_V_MSG(nullptr, "Parser bug: parsing identifier node without identifier token.");
	}
	IdentifierNode *identifier = alloc_node<IdentifierNode>();
	complete_extents(identifier);
	identifier->name = previous.get_identifier();
	if (identifier->name.operator String().is_empty()) {
		print_line("Empty identifier found.");
	}
	identifier->suite = current_suite;

	if (current_suite != nullptr && current_suite->has_local(identifier->name)) {
		const SuiteNode::Local &declaration = current_suite->get_local(identifier->name);

		identifier->source_function = declaration.source_function;
		switch (declaration.type) {
			case SuiteNode::Local::CONSTANT:
				identifier->source = IdentifierNode::LOCAL_CONSTANT;
				identifier->constant_source = declaration.constant;
				declaration.constant->usages++;
				break;
			case SuiteNode::Local::VARIABLE:
				identifier->source = IdentifierNode::LOCAL_VARIABLE;
				identifier->variable_source = declaration.variable;
				declaration.variable->usages++;
				break;
			case SuiteNode::Local::PARAMETER:
				identifier->source = IdentifierNode::FUNCTION_PARAMETER;
				identifier->parameter_source = declaration.parameter;
				declaration.parameter->usages++;
				break;
			case SuiteNode::Local::FOR_VARIABLE:
				identifier->source = IdentifierNode::LOCAL_ITERATOR;
				identifier->bind_source = declaration.bind;
				declaration.bind->usages++;
				break;
			case SuiteNode::Local::PATTERN_BIND:
				identifier->source = IdentifierNode::LOCAL_BIND;
				identifier->bind_source = declaration.bind;
				declaration.bind->usages++;
				break;
			case SuiteNode::Local::UNDEFINED:
				ERR_FAIL_V_MSG(nullptr, "Undefined local found.");
		}
	}

	return identifier;
}

GDScriptParser::LiteralNode *GDScriptParser::parse_literal() {
	return static_cast<LiteralNode *>(parse_literal(nullptr, false));
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_literal(ExpressionNode *p_previous_operand, bool p_can_assign) {
	if (previous.type != GDScriptTokenizer::Token::LITERAL) {
		push_error("Parser bug: parsing literal node without literal token.");
		ERR_FAIL_V_MSG(nullptr, "Parser bug: parsing literal node without literal token.");
	}

	LiteralNode *literal = alloc_node<LiteralNode>();
	literal->value = previous.literal;
	reset_extents(literal, p_previous_operand);
	update_extents(literal);
	make_completion_context(COMPLETION_NONE, literal, -1);
	complete_extents(literal);
	return literal;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_self(ExpressionNode *p_previous_operand, bool p_can_assign) {
	if (current_function && current_function->is_static) {
		push_error(R"(Cannot use "self" inside a static function.)");
	}
	SelfNode *self = alloc_node<SelfNode>();
	complete_extents(self);
	self->current_class = current_class;
	return self;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_builtin_constant(ExpressionNode *p_previous_operand, bool p_can_assign) {
	GDScriptTokenizer::Token::Type op_type = previous.type;
	LiteralNode *constant = alloc_node<LiteralNode>();
	complete_extents(constant);

	switch (op_type) {
		case GDScriptTokenizer::Token::CONST_PI:
			constant->value = Math_PI;
			break;
		case GDScriptTokenizer::Token::CONST_TAU:
			constant->value = Math_TAU;
			break;
		case GDScriptTokenizer::Token::CONST_INF:
			constant->value = INFINITY;
			break;
		case GDScriptTokenizer::Token::CONST_NAN:
			constant->value = NAN;
			break;
		default:
			return nullptr; // Unreachable.
	}

	return constant;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_unary_operator(ExpressionNode *p_previous_operand, bool p_can_assign) {
	GDScriptTokenizer::Token::Type op_type = previous.type;
	UnaryOpNode *operation = alloc_node<UnaryOpNode>();

	switch (op_type) {
		case GDScriptTokenizer::Token::MINUS:
			operation->operation = UnaryOpNode::OP_NEGATIVE;
			operation->variant_op = Variant::OP_NEGATE;
			operation->operand = parse_precedence(PREC_SIGN, false);
			if (operation->operand == nullptr) {
				push_error(R"(Expected expression after "-" operator.)");
			}
			break;
		case GDScriptTokenizer::Token::PLUS:
			operation->operation = UnaryOpNode::OP_POSITIVE;
			operation->variant_op = Variant::OP_POSITIVE;
			operation->operand = parse_precedence(PREC_SIGN, false);
			if (operation->operand == nullptr) {
				push_error(R"(Expected expression after "+" operator.)");
			}
			break;
		case GDScriptTokenizer::Token::TILDE:
			operation->operation = UnaryOpNode::OP_COMPLEMENT;
			operation->variant_op = Variant::OP_BIT_NEGATE;
			operation->operand = parse_precedence(PREC_BIT_NOT, false);
			if (operation->operand == nullptr) {
				push_error(R"(Expected expression after "~" operator.)");
			}
			break;
		case GDScriptTokenizer::Token::NOT:
		case GDScriptTokenizer::Token::BANG:
			operation->operation = UnaryOpNode::OP_LOGIC_NOT;
			operation->variant_op = Variant::OP_NOT;
			operation->operand = parse_precedence(PREC_LOGIC_NOT, false);
			if (operation->operand == nullptr) {
				push_error(vformat(R"(Expected expression after "%s" operator.)", op_type == GDScriptTokenizer::Token::NOT ? "not" : "!"));
			}
			break;
		default:
			complete_extents(operation);
			return nullptr; // Unreachable.
	}
	complete_extents(operation);

	return operation;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_binary_not_in_operator(ExpressionNode *p_previous_operand, bool p_can_assign) {
	// check that NOT is followed by IN by consuming it before calling parse_binary_operator which will only receive a plain IN
	UnaryOpNode *operation = alloc_node<UnaryOpNode>();
	reset_extents(operation, p_previous_operand);
	update_extents(operation);
	consume(GDScriptTokenizer::Token::IN, R"(Expected "in" after "not" in content-test operator.)");
	ExpressionNode *in_operation = parse_binary_operator(p_previous_operand, p_can_assign);
	operation->operation = UnaryOpNode::OP_LOGIC_NOT;
	operation->variant_op = Variant::OP_NOT;
	operation->operand = in_operation;
	complete_extents(operation);
	return operation;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_binary_operator(ExpressionNode *p_previous_operand, bool p_can_assign) {
	GDScriptTokenizer::Token op = previous;
	BinaryOpNode *operation = alloc_node<BinaryOpNode>();
	reset_extents(operation, p_previous_operand);
	update_extents(operation);

	Precedence precedence = (Precedence)(get_rule(op.type)->precedence + 1);
	operation->left_operand = p_previous_operand;
	operation->right_operand = parse_precedence(precedence, false);
	complete_extents(operation);

	if (operation->right_operand == nullptr) {
		push_error(vformat(R"(Expected expression after "%s" operator.)", op.get_name()));
	}

	// TODO: Also for unary, ternary, and assignment.
	switch (op.type) {
		case GDScriptTokenizer::Token::PLUS:
			operation->operation = BinaryOpNode::OP_ADDITION;
			operation->variant_op = Variant::OP_ADD;
			break;
		case GDScriptTokenizer::Token::MINUS:
			operation->operation = BinaryOpNode::OP_SUBTRACTION;
			operation->variant_op = Variant::OP_SUBTRACT;
			break;
		case GDScriptTokenizer::Token::STAR:
			operation->operation = BinaryOpNode::OP_MULTIPLICATION;
			operation->variant_op = Variant::OP_MULTIPLY;
			break;
		case GDScriptTokenizer::Token::SLASH:
			operation->operation = BinaryOpNode::OP_DIVISION;
			operation->variant_op = Variant::OP_DIVIDE;
			break;
		case GDScriptTokenizer::Token::PERCENT:
			operation->operation = BinaryOpNode::OP_MODULO;
			operation->variant_op = Variant::OP_MODULE;
			break;
		case GDScriptTokenizer::Token::STAR_STAR:
			operation->operation = BinaryOpNode::OP_POWER;
			operation->variant_op = Variant::OP_POWER;
			break;
		case GDScriptTokenizer::Token::LESS_LESS:
			operation->operation = BinaryOpNode::OP_BIT_LEFT_SHIFT;
			operation->variant_op = Variant::OP_SHIFT_LEFT;
			break;
		case GDScriptTokenizer::Token::GREATER_GREATER:
			operation->operation = BinaryOpNode::OP_BIT_RIGHT_SHIFT;
			operation->variant_op = Variant::OP_SHIFT_RIGHT;
			break;
		case GDScriptTokenizer::Token::AMPERSAND:
			operation->operation = BinaryOpNode::OP_BIT_AND;
			operation->variant_op = Variant::OP_BIT_AND;
			break;
		case GDScriptTokenizer::Token::PIPE:
			operation->operation = BinaryOpNode::OP_BIT_OR;
			operation->variant_op = Variant::OP_BIT_OR;
			break;
		case GDScriptTokenizer::Token::CARET:
			operation->operation = BinaryOpNode::OP_BIT_XOR;
			operation->variant_op = Variant::OP_BIT_XOR;
			break;
		case GDScriptTokenizer::Token::AND:
		case GDScriptTokenizer::Token::AMPERSAND_AMPERSAND:
			operation->operation = BinaryOpNode::OP_LOGIC_AND;
			operation->variant_op = Variant::OP_AND;
			break;
		case GDScriptTokenizer::Token::OR:
		case GDScriptTokenizer::Token::PIPE_PIPE:
			operation->operation = BinaryOpNode::OP_LOGIC_OR;
			operation->variant_op = Variant::OP_OR;
			break;
		case GDScriptTokenizer::Token::IN:
			operation->operation = BinaryOpNode::OP_CONTENT_TEST;
			operation->variant_op = Variant::OP_IN;
			break;
		case GDScriptTokenizer::Token::EQUAL_EQUAL:
			operation->operation = BinaryOpNode::OP_COMP_EQUAL;
			operation->variant_op = Variant::OP_EQUAL;
			break;
		case GDScriptTokenizer::Token::BANG_EQUAL:
			operation->operation = BinaryOpNode::OP_COMP_NOT_EQUAL;
			operation->variant_op = Variant::OP_NOT_EQUAL;
			break;
		case GDScriptTokenizer::Token::LESS:
			operation->operation = BinaryOpNode::OP_COMP_LESS;
			operation->variant_op = Variant::OP_LESS;
			break;
		case GDScriptTokenizer::Token::LESS_EQUAL:
			operation->operation = BinaryOpNode::OP_COMP_LESS_EQUAL;
			operation->variant_op = Variant::OP_LESS_EQUAL;
			break;
		case GDScriptTokenizer::Token::GREATER:
			operation->operation = BinaryOpNode::OP_COMP_GREATER;
			operation->variant_op = Variant::OP_GREATER;
			break;
		case GDScriptTokenizer::Token::GREATER_EQUAL:
			operation->operation = BinaryOpNode::OP_COMP_GREATER_EQUAL;
			operation->variant_op = Variant::OP_GREATER_EQUAL;
			break;
		default:
			return nullptr; // Unreachable.
	}

	return operation;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_ternary_operator(ExpressionNode *p_previous_operand, bool p_can_assign) {
	// Only one ternary operation exists, so no abstraction here.
	TernaryOpNode *operation = alloc_node<TernaryOpNode>();
	reset_extents(operation, p_previous_operand);
	update_extents(operation);

	operation->true_expr = p_previous_operand;
	operation->condition = parse_precedence(PREC_TERNARY, false);

	if (operation->condition == nullptr) {
		push_error(R"(Expected expression as ternary condition after "if".)");
	}

	consume(GDScriptTokenizer::Token::ELSE, R"(Expected "else" after ternary operator condition.)");

	operation->false_expr = parse_precedence(PREC_TERNARY, false);

	if (operation->false_expr == nullptr) {
		push_error(R"(Expected expression after "else".)");
	}

	complete_extents(operation);
	return operation;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_assignment(ExpressionNode *p_previous_operand, bool p_can_assign) {
	if (!p_can_assign) {
		push_error("Assignment is not allowed inside an expression.");
		return parse_expression(false); // Return the following expression.
	}
	if (p_previous_operand == nullptr) {
		return parse_expression(false); // Return the following expression.
	}

	switch (p_previous_operand->type) {
		case Node::IDENTIFIER: {
#ifdef DEBUG_ENABLED
			// Get source to store assignment count.
			// Also remove one usage since assignment isn't usage.
			IdentifierNode *id = static_cast<IdentifierNode *>(p_previous_operand);
			switch (id->source) {
				case IdentifierNode::LOCAL_VARIABLE:
					id->variable_source->usages--;
					break;
				case IdentifierNode::LOCAL_CONSTANT:
					id->constant_source->usages--;
					break;
				case IdentifierNode::FUNCTION_PARAMETER:
					id->parameter_source->usages--;
					break;
				case IdentifierNode::LOCAL_ITERATOR:
				case IdentifierNode::LOCAL_BIND:
					id->bind_source->usages--;
					break;
				default:
					break;
			}
#endif
		} break;
		case Node::SUBSCRIPT:
			// Okay.
			break;
		default:
			push_error(R"(Only identifier, attribute access, and subscription access can be used as assignment target.)");
			return parse_expression(false); // Return the following expression.
	}

	AssignmentNode *assignment = alloc_node<AssignmentNode>();
	reset_extents(assignment, p_previous_operand);
	update_extents(assignment);

	make_completion_context(COMPLETION_ASSIGN, assignment);
	switch (previous.type) {
		case GDScriptTokenizer::Token::EQUAL:
			assignment->operation = AssignmentNode::OP_NONE;
			assignment->variant_op = Variant::OP_MAX;
			break;
		case GDScriptTokenizer::Token::PLUS_EQUAL:
			assignment->operation = AssignmentNode::OP_ADDITION;
			assignment->variant_op = Variant::OP_ADD;
			break;
		case GDScriptTokenizer::Token::MINUS_EQUAL:
			assignment->operation = AssignmentNode::OP_SUBTRACTION;
			assignment->variant_op = Variant::OP_SUBTRACT;
			break;
		case GDScriptTokenizer::Token::STAR_EQUAL:
			assignment->operation = AssignmentNode::OP_MULTIPLICATION;
			assignment->variant_op = Variant::OP_MULTIPLY;
			break;
		case GDScriptTokenizer::Token::STAR_STAR_EQUAL:
			assignment->operation = AssignmentNode::OP_POWER;
			assignment->variant_op = Variant::OP_POWER;
			break;
		case GDScriptTokenizer::Token::SLASH_EQUAL:
			assignment->operation = AssignmentNode::OP_DIVISION;
			assignment->variant_op = Variant::OP_DIVIDE;
			break;
		case GDScriptTokenizer::Token::PERCENT_EQUAL:
			assignment->operation = AssignmentNode::OP_MODULO;
			assignment->variant_op = Variant::OP_MODULE;
			break;
		case GDScriptTokenizer::Token::LESS_LESS_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_SHIFT_LEFT;
			assignment->variant_op = Variant::OP_SHIFT_LEFT;
			break;
		case GDScriptTokenizer::Token::GREATER_GREATER_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_SHIFT_RIGHT;
			assignment->variant_op = Variant::OP_SHIFT_RIGHT;
			break;
		case GDScriptTokenizer::Token::AMPERSAND_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_AND;
			assignment->variant_op = Variant::OP_BIT_AND;
			break;
		case GDScriptTokenizer::Token::PIPE_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_OR;
			assignment->variant_op = Variant::OP_BIT_OR;
			break;
		case GDScriptTokenizer::Token::CARET_EQUAL:
			assignment->operation = AssignmentNode::OP_BIT_XOR;
			assignment->variant_op = Variant::OP_BIT_XOR;
			break;
		default:
			break; // Unreachable.
	}
	assignment->assignee = p_previous_operand;
	assignment->assigned_value = parse_expression(false);
#ifdef TOOLS_ENABLED
	if (assignment->assigned_value != nullptr && assignment->assigned_value->type == GDScriptParser::Node::IDENTIFIER) {
		override_completion_context(assignment->assigned_value, COMPLETION_ASSIGN, assignment);
	}
#endif
	if (assignment->assigned_value == nullptr) {
		push_error(R"(Expected an expression after "=".)");
	}
	complete_extents(assignment);

	return assignment;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_await(ExpressionNode *p_previous_operand, bool p_can_assign) {
	AwaitNode *await = alloc_node<AwaitNode>();
	ExpressionNode *element = parse_precedence(PREC_AWAIT, false);
	if (element == nullptr) {
		push_error(R"(Expected signal or coroutine after "await".)");
	}
	await->to_await = element;
	complete_extents(await);

	if (current_function) { // Might be null in a getter or setter.
		current_function->is_coroutine = true;
	}

	return await;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_array(ExpressionNode *p_previous_operand, bool p_can_assign) {
	ArrayNode *array = alloc_node<ArrayNode>();

	if (!check(GDScriptTokenizer::Token::BRACKET_CLOSE)) {
		do {
			if (check(GDScriptTokenizer::Token::BRACKET_CLOSE)) {
				// Allow for trailing comma.
				break;
			}

			ExpressionNode *element = parse_expression(false);
			if (element == nullptr) {
				push_error(R"(Expected expression as array element.)");
			} else {
				array->elements.push_back(element);
			}
		} while (match(GDScriptTokenizer::Token::COMMA) && !is_at_end());
	}
	pop_multiline();
	consume(GDScriptTokenizer::Token::BRACKET_CLOSE, R"(Expected closing "]" after array elements.)");
	complete_extents(array);

	return array;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_dictionary(ExpressionNode *p_previous_operand, bool p_can_assign) {
	DictionaryNode *dictionary = alloc_node<DictionaryNode>();

	bool decided_style = false;
	if (!check(GDScriptTokenizer::Token::BRACE_CLOSE)) {
		do {
			if (check(GDScriptTokenizer::Token::BRACE_CLOSE)) {
				// Allow for trailing comma.
				break;
			}

			// Key.
			ExpressionNode *key = parse_expression(false, true); // Stop on "=" so we can check for Lua table style.

			if (key == nullptr) {
				push_error(R"(Expected expression as dictionary key.)");
			}

			if (!decided_style) {
				switch (current.type) {
					case GDScriptTokenizer::Token::COLON:
						dictionary->style = DictionaryNode::PYTHON_DICT;
						break;
					case GDScriptTokenizer::Token::EQUAL:
						dictionary->style = DictionaryNode::LUA_TABLE;
						break;
					default:
						push_error(R"(Expected ":" or "=" after dictionary key.)");
						break;
				}
				decided_style = true;
			}

			switch (dictionary->style) {
				case DictionaryNode::LUA_TABLE:
					if (key != nullptr && key->type != Node::IDENTIFIER && key->type != Node::LITERAL) {
						push_error(R"(Expected identifier or string as Lua-style dictionary key (e.g "{ key = value }").)");
						advance();
						break;
					}
					if (key != nullptr && key->type == Node::LITERAL && static_cast<LiteralNode *>(key)->value.get_type() != Variant::STRING) {
						push_error(R"(Expected identifier or string as Lua-style dictionary key (e.g "{ key = value }").)");
						advance();
						break;
					}
					if (!match(GDScriptTokenizer::Token::EQUAL)) {
						if (match(GDScriptTokenizer::Token::COLON)) {
							push_error(R"(Expected "=" after dictionary key. Mixing dictionary styles is not allowed.)");
							advance(); // Consume wrong separator anyway.
						} else {
							push_error(R"(Expected "=" after dictionary key.)");
						}
					}
					if (key != nullptr) {
						key->is_constant = true;
						if (key->type == Node::IDENTIFIER) {
							key->reduced_value = static_cast<IdentifierNode *>(key)->name;
						} else if (key->type == Node::LITERAL) {
							key->reduced_value = StringName(static_cast<LiteralNode *>(key)->value.operator String());
						}
					}
					break;
				case DictionaryNode::PYTHON_DICT:
					if (!match(GDScriptTokenizer::Token::COLON)) {
						if (match(GDScriptTokenizer::Token::EQUAL)) {
							push_error(R"(Expected ":" after dictionary key. Mixing dictionary styles is not allowed.)");
							advance(); // Consume wrong separator anyway.
						} else {
							push_error(R"(Expected ":" after dictionary key.)");
						}
					}
					break;
			}

			// Value.
			ExpressionNode *value = parse_expression(false);
			if (value == nullptr) {
				push_error(R"(Expected expression as dictionary value.)");
			}

			if (key != nullptr && value != nullptr) {
				dictionary->elements.push_back({ key, value });
			}
		} while (match(GDScriptTokenizer::Token::COMMA) && !is_at_end());
	}
	pop_multiline();
	consume(GDScriptTokenizer::Token::BRACE_CLOSE, R"(Expected closing "}" after dictionary elements.)");
	complete_extents(dictionary);

	return dictionary;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_grouping(ExpressionNode *p_previous_operand, bool p_can_assign) {
	ExpressionNode *grouped = parse_expression(false);
	pop_multiline();
	if (grouped == nullptr) {
		push_error(R"(Expected grouping expression.)");
	} else {
		consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected closing ")" after grouping expression.)*");
	}
	return grouped;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_attribute(ExpressionNode *p_previous_operand, bool p_can_assign) {
	SubscriptNode *attribute = alloc_node<SubscriptNode>();
	reset_extents(attribute, p_previous_operand);
	update_extents(attribute);

	if (for_completion) {
		bool is_builtin = false;
		if (p_previous_operand && p_previous_operand->type == Node::IDENTIFIER) {
			const IdentifierNode *id = static_cast<const IdentifierNode *>(p_previous_operand);
			Variant::Type builtin_type = get_builtin_type(id->name);
			if (builtin_type < Variant::VARIANT_MAX) {
				make_completion_context(COMPLETION_BUILT_IN_TYPE_CONSTANT_OR_STATIC_METHOD, builtin_type);
				is_builtin = true;
			}
		}
		if (!is_builtin) {
			make_completion_context(COMPLETION_ATTRIBUTE, attribute, -1);
		}
	}

	attribute->base = p_previous_operand;

	if (current.is_node_name()) {
		current.type = GDScriptTokenizer::Token::IDENTIFIER;
	}
	if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected identifier after "." for attribute access.)")) {
		complete_extents(attribute);
		return attribute;
	}

	attribute->is_attribute = true;
	attribute->attribute = parse_identifier();

	complete_extents(attribute);
	return attribute;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_subscript(ExpressionNode *p_previous_operand, bool p_can_assign) {
	SubscriptNode *subscript = alloc_node<SubscriptNode>();
	reset_extents(subscript, p_previous_operand);
	update_extents(subscript);

	make_completion_context(COMPLETION_SUBSCRIPT, subscript);

	subscript->base = p_previous_operand;
	subscript->index = parse_expression(false);

#ifdef TOOLS_ENABLED
	if (subscript->index != nullptr && subscript->index->type == Node::LITERAL) {
		override_completion_context(subscript->index, COMPLETION_SUBSCRIPT, subscript);
	}
#endif

	if (subscript->index == nullptr) {
		push_error(R"(Expected expression after "[".)");
	}

	pop_multiline();
	consume(GDScriptTokenizer::Token::BRACKET_CLOSE, R"(Expected "]" after subscription index.)");
	complete_extents(subscript);

	return subscript;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_cast(ExpressionNode *p_previous_operand, bool p_can_assign) {
	CastNode *cast = alloc_node<CastNode>();
	reset_extents(cast, p_previous_operand);
	update_extents(cast);

	cast->operand = p_previous_operand;
	cast->cast_type = parse_type();
	complete_extents(cast);

	if (cast->cast_type == nullptr) {
		push_error(R"(Expected type specifier after "as".)");
		return p_previous_operand;
	}

	return cast;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_call(ExpressionNode *p_previous_operand, bool p_can_assign) {
	CallNode *call = alloc_node<CallNode>();
	reset_extents(call, p_previous_operand);

	if (previous.type == GDScriptTokenizer::Token::SUPER) {
		// Super call.
		call->is_super = true;
		push_multiline(true);
		if (match(GDScriptTokenizer::Token::PARENTHESIS_OPEN)) {
			// Implicit call to the parent method of the same name.
			if (current_function == nullptr) {
				push_error(R"(Cannot use implicit "super" call outside of a function.)");
				pop_multiline();
				complete_extents(call);
				return nullptr;
			}
			if (current_function->identifier) {
				call->function_name = current_function->identifier->name;
			} else {
				call->function_name = SNAME("<anonymous>");
			}
		} else {
			consume(GDScriptTokenizer::Token::PERIOD, R"(Expected "." or "(" after "super".)");
			make_completion_context(COMPLETION_SUPER_METHOD, call, true);
			if (!consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected function name after ".".)")) {
				pop_multiline();
				complete_extents(call);
				return nullptr;
			}
			IdentifierNode *identifier = parse_identifier();
			call->callee = identifier;
			call->function_name = identifier->name;
			consume(GDScriptTokenizer::Token::PARENTHESIS_OPEN, R"(Expected "(" after function name.)");
		}
	} else {
		call->callee = p_previous_operand;

		if (call->callee == nullptr) {
			push_error(R"*(Cannot call on an expression. Use ".call()" if it's a Callable.)*");
		} else if (call->callee->type == Node::IDENTIFIER) {
			call->function_name = static_cast<IdentifierNode *>(call->callee)->name;
			make_completion_context(COMPLETION_METHOD, call->callee);
		} else if (call->callee->type == Node::SUBSCRIPT) {
			SubscriptNode *attribute = static_cast<SubscriptNode *>(call->callee);
			if (attribute->is_attribute) {
				if (attribute->attribute) {
					call->function_name = attribute->attribute->name;
				}
				make_completion_context(COMPLETION_ATTRIBUTE_METHOD, call->callee);
			} else {
				// TODO: The analyzer can see if this is actually a Callable and give better error message.
				push_error(R"*(Cannot call on an expression. Use ".call()" if it's a Callable.)*");
			}
		} else {
			push_error(R"*(Cannot call on an expression. Use ".call()" if it's a Callable.)*");
		}
	}

	// Arguments.
	CompletionType ct = COMPLETION_CALL_ARGUMENTS;
	if (call->function_name == SNAME("load")) {
		ct = COMPLETION_RESOURCE_PATH;
	}
	push_completion_call(call);
	int argument_index = 0;
	do {
		make_completion_context(ct, call, argument_index);
		if (check(GDScriptTokenizer::Token::PARENTHESIS_CLOSE)) {
			// Allow for trailing comma.
			break;
		}
		ExpressionNode *argument = parse_expression(false);
		if (argument == nullptr) {
			push_error(R"(Expected expression as the function argument.)");
		} else {
			call->arguments.push_back(argument);

			if (argument->type == Node::LITERAL) {
				override_completion_context(argument, ct, call, argument_index);
			}
		}

		ct = COMPLETION_CALL_ARGUMENTS;
		argument_index++;
	} while (match(GDScriptTokenizer::Token::COMMA));
	pop_completion_call();

	pop_multiline();
	consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected closing ")" after call arguments.)*");
	complete_extents(call);

	return call;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_get_node(ExpressionNode *p_previous_operand, bool p_can_assign) {
	// We want code completion after a DOLLAR even if the current code is invalid.
	make_completion_context(COMPLETION_GET_NODE, nullptr, -1);

	if (!current.is_node_name() && !check(GDScriptTokenizer::Token::LITERAL) && !check(GDScriptTokenizer::Token::SLASH) && !check(GDScriptTokenizer::Token::PERCENT)) {
		push_error(vformat(R"(Expected node path as string or identifier after "%s".)", previous.get_name()));
		return nullptr;
	}

	if (check(GDScriptTokenizer::Token::LITERAL)) {
		if (current.literal.get_type() != Variant::STRING) {
			push_error(vformat(R"(Expected node path as string or identifier after "%s".)", previous.get_name()));
			return nullptr;
		}
	}

	GetNodeNode *get_node = alloc_node<GetNodeNode>();

	// Store the last item in the path so the parser knows what to expect.
	// Allow allows more specific error messages.
	enum PathState {
		PATH_STATE_START,
		PATH_STATE_SLASH,
		PATH_STATE_PERCENT,
		PATH_STATE_NODE_NAME,
	} path_state = PATH_STATE_START;

	if (previous.type == GDScriptTokenizer::Token::DOLLAR) {
		// Detect initial slash, which will be handled in the loop if it matches.
		match(GDScriptTokenizer::Token::SLASH);
	} else {
		get_node->use_dollar = false;
	}

	int context_argument = 0;

	do {
		if (previous.type == GDScriptTokenizer::Token::PERCENT) {
			if (path_state != PATH_STATE_START && path_state != PATH_STATE_SLASH) {
				push_error(R"("%" is only valid in the beginning of a node name (either after "$" or after "/"))");
				complete_extents(get_node);
				return nullptr;
			}

			get_node->full_path += "%";

			path_state = PATH_STATE_PERCENT;
		} else if (previous.type == GDScriptTokenizer::Token::SLASH) {
			if (path_state != PATH_STATE_START && path_state != PATH_STATE_NODE_NAME) {
				push_error(R"("/" is only valid at the beginning of the path or after a node name.)");
				complete_extents(get_node);
				return nullptr;
			}

			get_node->full_path += "/";

			path_state = PATH_STATE_SLASH;
		}

		make_completion_context(COMPLETION_GET_NODE, get_node, context_argument++);

		if (match(GDScriptTokenizer::Token::LITERAL)) {
			if (previous.literal.get_type() != Variant::STRING) {
				String previous_token;
				switch (path_state) {
					case PATH_STATE_START:
						previous_token = "$";
						break;
					case PATH_STATE_PERCENT:
						previous_token = "%";
						break;
					case PATH_STATE_SLASH:
						previous_token = "/";
						break;
					default:
						break;
				}
				push_error(vformat(R"(Expected node path as string or identifier after "%s".)", previous_token));
				complete_extents(get_node);
				return nullptr;
			}

			get_node->full_path += previous.literal.operator String();

			path_state = PATH_STATE_NODE_NAME;
		} else if (current.is_node_name()) {
			advance();

			String identifier = previous.get_identifier();
#ifdef DEBUG_ENABLED
			// Check spoofing.
			if (TS->has_feature(TextServer::FEATURE_UNICODE_SECURITY) && TS->spoof_check(identifier)) {
				push_warning(get_node, GDScriptWarning::CONFUSABLE_IDENTIFIER, identifier);
			}
#endif
			get_node->full_path += identifier;

			path_state = PATH_STATE_NODE_NAME;
		} else if (!check(GDScriptTokenizer::Token::SLASH) && !check(GDScriptTokenizer::Token::PERCENT)) {
			push_error(vformat(R"(Unexpected "%s" in node path.)", current.get_name()));
			complete_extents(get_node);
			return nullptr;
		}
	} while (match(GDScriptTokenizer::Token::SLASH) || match(GDScriptTokenizer::Token::PERCENT));

	complete_extents(get_node);
	return get_node;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_preload(ExpressionNode *p_previous_operand, bool p_can_assign) {
	PreloadNode *preload = alloc_node<PreloadNode>();
	preload->resolved_path = "<missing path>";

	push_multiline(true);
	consume(GDScriptTokenizer::Token::PARENTHESIS_OPEN, R"(Expected "(" after "preload".)");

	make_completion_context(COMPLETION_RESOURCE_PATH, preload);
	push_completion_call(preload);

	preload->path = parse_expression(false);

	if (preload->path == nullptr) {
		push_error(R"(Expected resource path after "(".)");
	}

	pop_completion_call();

	pop_multiline();
	consume(GDScriptTokenizer::Token::PARENTHESIS_CLOSE, R"*(Expected ")" after preload path.)*");
	complete_extents(preload);

	return preload;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_lambda(ExpressionNode *p_previous_operand, bool p_can_assign) {
	LambdaNode *lambda = alloc_node<LambdaNode>();
	lambda->parent_function = current_function;
	lambda->parent_lambda = current_lambda;

	FunctionNode *function = alloc_node<FunctionNode>();
	function->source_lambda = lambda;

	function->is_static = current_function != nullptr ? current_function->is_static : false;

	if (match(GDScriptTokenizer::Token::IDENTIFIER)) {
		function->identifier = parse_identifier();
	}

	bool multiline_context = multiline_stack.back()->get();

	// Reset the multiline stack since we don't want the multiline mode one in the lambda body.
	push_multiline(false);
	if (multiline_context) {
		tokenizer->push_expression_indented_block();
	}

	push_multiline(true); // For the parameters.
	if (function->identifier) {
		consume(GDScriptTokenizer::Token::PARENTHESIS_OPEN, R"(Expected opening "(" after lambda name.)");
	} else {
		consume(GDScriptTokenizer::Token::PARENTHESIS_OPEN, R"(Expected opening "(" after "func".)");
	}

	FunctionNode *previous_function = current_function;
	current_function = function;

	LambdaNode *previous_lambda = current_lambda;
	current_lambda = lambda;

	SuiteNode *body = alloc_node<SuiteNode>();
	body->parent_function = current_function;
	body->parent_block = current_suite;

	SuiteNode *previous_suite = current_suite;
	current_suite = body;

	parse_function_signature(function, body, "lambda");

	current_suite = previous_suite;

	bool previous_in_lambda = in_lambda;
	in_lambda = true;

	// Save break/continue state.
	bool could_break = can_break;
	bool could_continue = can_continue;

	// Disallow break/continue.
	can_break = false;
	can_continue = false;

	function->body = parse_suite("lambda declaration", body, true);
	complete_extents(function);
	complete_extents(lambda);

	pop_multiline();

	if (multiline_context) {
		// If we're in multiline mode, we want to skip the spurious DEDENT and NEWLINE tokens.
		while (check(GDScriptTokenizer::Token::DEDENT) || check(GDScriptTokenizer::Token::INDENT) || check(GDScriptTokenizer::Token::NEWLINE)) {
			current = tokenizer->scan(); // Not advance() since we don't want to change the previous token.
		}
		tokenizer->pop_expression_indented_block();
	}

	current_function = previous_function;
	current_lambda = previous_lambda;
	in_lambda = previous_in_lambda;
	lambda->function = function;

	// Reset break/continue state.
	can_break = could_break;
	can_continue = could_continue;

	return lambda;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_type_test(ExpressionNode *p_previous_operand, bool p_can_assign) {
	// x is not int
	// ^        ^^^ ExpressionNode, TypeNode
	// ^^^^^^^^^^^^ TypeTestNode
	// ^^^^^^^^^^^^ UnaryOpNode
	UnaryOpNode *not_node = nullptr;
	if (match(GDScriptTokenizer::Token::NOT)) {
		not_node = alloc_node<UnaryOpNode>();
		not_node->operation = UnaryOpNode::OP_LOGIC_NOT;
		not_node->variant_op = Variant::OP_NOT;
		reset_extents(not_node, p_previous_operand);
		update_extents(not_node);
	}

	TypeTestNode *type_test = alloc_node<TypeTestNode>();
	reset_extents(type_test, p_previous_operand);
	update_extents(type_test);

	type_test->operand = p_previous_operand;
	type_test->test_type = parse_type();
	complete_extents(type_test);

	if (not_node != nullptr) {
		not_node->operand = type_test;
		complete_extents(not_node);
	}

	if (type_test->test_type == nullptr) {
		if (not_node == nullptr) {
			push_error(R"(Expected type specifier after "is".)");
		} else {
			push_error(R"(Expected type specifier after "is not".)");
		}
	}

	if (not_node != nullptr) {
		return not_node;
	}

	return type_test;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_yield(ExpressionNode *p_previous_operand, bool p_can_assign) {
	push_error(R"("yield" was removed in Godot 4. Use "await" instead.)");
	return nullptr;
}

GDScriptParser::ExpressionNode *GDScriptParser::parse_invalid_token(ExpressionNode *p_previous_operand, bool p_can_assign) {
	// Just for better error messages.
	GDScriptTokenizer::Token::Type invalid = previous.type;

	switch (invalid) {
		case GDScriptTokenizer::Token::QUESTION_MARK:
			push_error(R"(Unexpected "?" in source. If you want a ternary operator, use "truthy_value if true_condition else falsy_value".)");
			break;
		default:
			return nullptr; // Unreachable.
	}

	// Return the previous expression.
	return p_previous_operand;
}

GDScriptParser::TypeNode *GDScriptParser::parse_type(bool p_allow_void) {
	TypeNode *type = alloc_node<TypeNode>();
	make_completion_context(p_allow_void ? COMPLETION_TYPE_NAME_OR_VOID : COMPLETION_TYPE_NAME, type);
	if (!match(GDScriptTokenizer::Token::IDENTIFIER)) {
		if (match(GDScriptTokenizer::Token::VOID)) {
			if (p_allow_void) {
				complete_extents(type);
				TypeNode *void_type = type;
				return void_type;
			} else {
				push_error(R"("void" is only allowed for a function return type.)");
			}
		}
		// Leave error message to the caller who knows the context.
		complete_extents(type);
		return nullptr;
	}

	IdentifierNode *type_element = parse_identifier();

	type->type_chain.push_back(type_element);

	if (match(GDScriptTokenizer::Token::BRACKET_OPEN)) {
		// Typed collection (like Array[int], Dictionary[String, int]).
		bool first_pass = true;
		do {
			TypeNode *container_type = parse_type(false); // Don't allow void for element type.
			if (container_type == nullptr) {
				push_error(vformat(R"(Expected type for collection after "%s".)", first_pass ? "[" : ","));
				complete_extents(type);
				type = nullptr;
				break;
			} else if (container_type->container_types.size() > 0) {
				push_error("Nested typed collections are not supported.");
			} else {
				type->container_types.append(container_type);
			}
			first_pass = false;
		} while (match(GDScriptTokenizer::Token::COMMA));
		consume(GDScriptTokenizer::Token::BRACKET_CLOSE, R"(Expected closing "]" after collection type.)");
		if (type != nullptr) {
			complete_extents(type);
		}
		return type;
	}

	int chain_index = 1;
	while (match(GDScriptTokenizer::Token::PERIOD)) {
		make_completion_context(COMPLETION_TYPE_ATTRIBUTE, type, chain_index++);
		if (consume(GDScriptTokenizer::Token::IDENTIFIER, R"(Expected inner type name after ".".)")) {
			type_element = parse_identifier();
			type->type_chain.push_back(type_element);
		}
	}

	complete_extents(type);
	return type;
}

#ifdef TOOLS_ENABLED
enum DocLineState {
	DOC_LINE_NORMAL,
	DOC_LINE_IN_CODE,
	DOC_LINE_IN_CODEBLOCK,
	DOC_LINE_IN_KBD,
};

static String _process_doc_line(const String &p_line, const String &p_text, const String &p_space_prefix, DocLineState &r_state) {
	String line = p_line;
	if (r_state == DOC_LINE_NORMAL) {
		line = line.strip_edges(true, false);
	} else {
		line = line.trim_prefix(p_space_prefix);
	}

	String line_join;
	if (!p_text.is_empty()) {
		if (r_state == DOC_LINE_NORMAL) {
			if (p_text.ends_with("[/codeblock]")) {
				line_join = "\n";
			} else if (!p_text.ends_with("[br]")) {
				line_join = " ";
			}
		} else {
			line_join = "\n";
		}
	}

	String result;
	int from = 0;
	int buffer_start = 0;
	const int len = line.length();
	bool process = true;
	while (process) {
		switch (r_state) {
			case DOC_LINE_NORMAL: {
				int lb_pos = line.find_char('[', from);
				if (lb_pos < 0) {
					process = false;
					break;
				}
				int rb_pos = line.find_char(']', lb_pos + 1);
				if (rb_pos < 0) {
					process = false;
					break;
				}

				from = rb_pos + 1;

				String tag = line.substr(lb_pos + 1, rb_pos - lb_pos - 1);
				if (tag == "code" || tag.begins_with("code ")) {
					r_state = DOC_LINE_IN_CODE;
				} else if (tag == "codeblock" || tag.begins_with("codeblock ")) {
					if (lb_pos == 0) {
						line_join = "\n";
					} else {
						result += line.substr(buffer_start, lb_pos - buffer_start) + '\n';
					}
					result += "[" + tag + "]";
					if (from < len) {
						result += '\n';
					}

					r_state = DOC_LINE_IN_CODEBLOCK;
					buffer_start = from;
				} else if (tag == "kbd") {
					r_state = DOC_LINE_IN_KBD;
				}
			} break;
			case DOC_LINE_IN_CODE: {
				int pos = line.find("[/code]", from);
				if (pos < 0) {
					process = false;
					break;
				}

				from = pos + 7; // `len("[/code]")`.

				r_state = DOC_LINE_NORMAL;
			} break;
			case DOC_LINE_IN_CODEBLOCK: {
				int pos = line.find("[/codeblock]", from);
				if (pos < 0) {
					process = false;
					break;
				}

				from = pos + 12; // `len("[/codeblock]")`.

				if (pos == 0) {
					line_join = "\n";
				} else {
					result += line.substr(buffer_start, pos - buffer_start) + '\n';
				}
				result += "[/codeblock]";
				if (from < len) {
					result += '\n';
				}

				r_state = DOC_LINE_NORMAL;
				buffer_start = from;
			} break;
			case DOC_LINE_IN_KBD: {
				int pos = line.find("[/kbd]", from);
				if (pos < 0) {
					process = false;
					break;
				}

				from = pos + 6; // `len("[/kbd]")`.

				r_state = DOC_LINE_NORMAL;
			} break;
		}
	}

	result += line.substr(buffer_start);
	if (r_state == DOC_LINE_NORMAL) {
		result = result.strip_edges(false, true);
	}

	return line_join + result;
}

bool GDScriptParser::has_comment(int p_line, bool p_must_be_doc) {
	bool has_comment = tokenizer->get_comments().has(p_line);
	// If there are no comments or if we don't care whether the comment
	// is a docstring, we have our result.
	if (!p_must_be_doc || !has_comment) {
		return has_comment;
	}

	return tokenizer->get_comments()[p_line].comment.begins_with("##");
}

GDScriptParser::MemberDocData GDScriptParser::parse_doc_comment(int p_line, bool p_single_line) {
	ERR_FAIL_COND_V(!has_comment(p_line, true), MemberDocData());

	const HashMap<int, GDScriptTokenizer::CommentData> &comments = tokenizer->get_comments();
	int line = p_line;

	if (!p_single_line) {
		while (comments.has(line - 1) && comments[line - 1].new_line && comments[line - 1].comment.begins_with("##")) {
			line--;
		}
	}

	max_script_doc_line = MIN(max_script_doc_line, line - 1);

	String space_prefix;
	{
		int i = 2;
		for (; i < comments[line].comment.length(); i++) {
			if (comments[line].comment[i] != ' ') {
				break;
			}
		}
		space_prefix = String(" ").repeat(i - 2);
	}

	DocLineState state = DOC_LINE_NORMAL;
	MemberDocData result;

	while (line <= p_line) {
		String doc_line = comments[line].comment.trim_prefix("##");
		line++;

		if (state == DOC_LINE_NORMAL) {
			String stripped_line = doc_line.strip_edges();
			if (stripped_line == "@deprecated" || stripped_line.begins_with("@deprecated:")) {
				result.is_deprecated = true;
				if (stripped_line.begins_with("@deprecated:")) {
					result.deprecated_message = stripped_line.trim_prefix("@deprecated:").strip_edges();
				}
				continue;
			} else if (stripped_line == "@experimental" || stripped_line.begins_with("@experimental:")) {
				result.is_experimental = true;
				if (stripped_line.begins_with("@experimental:")) {
					result.experimental_message = stripped_line.trim_prefix("@experimental:").strip_edges();
				}
				continue;
			}
		}

		result.description += _process_doc_line(doc_line, result.description, space_prefix, state);
	}

	return result;
}

GDScriptParser::ClassDocData GDScriptParser::parse_class_doc_comment(int p_line, bool p_single_line) {
	ERR_FAIL_COND_V(!has_comment(p_line, true), ClassDocData());

	const HashMap<int, GDScriptTokenizer::CommentData> &comments = tokenizer->get_comments();
	int line = p_line;

	if (!p_single_line) {
		while (comments.has(line - 1) && comments[line - 1].new_line && comments[line - 1].comment.begins_with("##")) {
			line--;
		}
	}

	max_script_doc_line = MIN(max_script_doc_line, line - 1);

	String space_prefix;
	{
		int i = 2;
		for (; i < comments[line].comment.length(); i++) {
			if (comments[line].comment[i] != ' ') {
				break;
			}
		}
		space_prefix = String(" ").repeat(i - 2);
	}

	DocLineState state = DOC_LINE_NORMAL;
	bool is_in_brief = true;
	ClassDocData result;

	while (line <= p_line) {
		String doc_line = comments[line].comment.trim_prefix("##");
		line++;

		if (state == DOC_LINE_NORMAL) {
			String stripped_line = doc_line.strip_edges();

			// A blank line separates the description from the brief.
			if (is_in_brief && !result.brief.is_empty() && stripped_line.is_empty()) {
				is_in_brief = false;
				continue;
			}

			if (stripped_line.begins_with("@tutorial")) {
				String title, link;

				int begin_scan = String("@tutorial").length();
				if (begin_scan >= stripped_line.length()) {
					continue; // Invalid syntax.
				}

				if (stripped_line[begin_scan] == ':') { // No title.
					// Syntax: ## @tutorial: https://godotengine.org/ // The title argument is optional.
					title = "";
					link = stripped_line.trim_prefix("@tutorial:").strip_edges();
				} else {
					/* Syntax:
					 *   @tutorial ( The Title Here )         :         https://the.url/
					 *             ^ open           ^ close   ^ colon   ^ url
					 */
					int open_bracket_pos = begin_scan, close_bracket_pos = 0;
					while (open_bracket_pos < stripped_line.length() && (stripped_line[open_bracket_pos] == ' ' || stripped_line[open_bracket_pos] == '\t')) {
						open_bracket_pos++;
					}
					if (open_bracket_pos == stripped_line.length() || stripped_line[open_bracket_pos++] != '(') {
						continue; // Invalid syntax.
					}
					close_bracket_pos = open_bracket_pos;
					while (close_bracket_pos < stripped_line.length() && stripped_line[close_bracket_pos] != ')') {
						close_bracket_pos++;
					}
					if (close_bracket_pos == stripped_line.length()) {
						continue; // Invalid syntax.
					}

					int colon_pos = close_bracket_pos + 1;
					while (colon_pos < stripped_line.length() && (stripped_line[colon_pos] == ' ' || stripped_line[colon_pos] == '\t')) {
						colon_pos++;
					}
					if (colon_pos == stripped_line.length() || stripped_line[colon_pos++] != ':') {
						continue; // Invalid syntax.
					}

					title = stripped_line.substr(open_bracket_pos, close_bracket_pos - open_bracket_pos).strip_edges();
					link = stripped_line.substr(colon_pos).strip_edges();
				}

				result.tutorials.append(Pair<String, String>(title, link));
				continue;
			} else if (stripped_line == "@deprecated" || stripped_line.begins_with("@deprecated:")) {
				result.is_deprecated = true;
				if (stripped_line.begins_with("@deprecated:")) {
					result.deprecated_message = stripped_line.trim_prefix("@deprecated:").strip_edges();
				}
				continue;
			} else if (stripped_line == "@experimental" || stripped_line.begins_with("@experimental:")) {
				result.is_experimental = true;
				if (stripped_line.begins_with("@experimental:")) {
					result.experimental_message = stripped_line.trim_prefix("@experimental:").strip_edges();
				}
				continue;
			}
		}

		if (is_in_brief) {
			result.brief += _process_doc_line(doc_line, result.brief, space_prefix, state);
		} else {
			result.description += _process_doc_line(doc_line, result.description, space_prefix, state);
		}
	}

	return result;
}
#endif // TOOLS_ENABLED

GDScriptParser::ParseRule *GDScriptParser::get_rule(GDScriptTokenizer::Token::Type p_token_type) {
	// Function table for expression parsing.
	// clang-format destroys the alignment here, so turn off for the table.
	/* clang-format off */
	static ParseRule rules[] = {
		// PREFIX                                           INFIX                                           PRECEDENCE (for infix)
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // EMPTY,
		// Basic
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ANNOTATION,
		{ &GDScriptParser::parse_identifier,             	nullptr,                                        PREC_NONE }, // IDENTIFIER,
		{ &GDScriptParser::parse_literal,                	nullptr,                                        PREC_NONE }, // LITERAL,
		// Comparison
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_COMPARISON }, // LESS,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_COMPARISON }, // LESS_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_COMPARISON }, // GREATER,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_COMPARISON }, // GREATER_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_COMPARISON }, // EQUAL_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_COMPARISON }, // BANG_EQUAL,
		// Logical
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_LOGIC_AND }, // AND,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_LOGIC_OR }, // OR,
		{ &GDScriptParser::parse_unary_operator,         	&GDScriptParser::parse_binary_not_in_operator,	PREC_CONTENT_TEST }, // NOT,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,			PREC_LOGIC_AND }, // AMPERSAND_AMPERSAND,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,			PREC_LOGIC_OR }, // PIPE_PIPE,
		{ &GDScriptParser::parse_unary_operator,			nullptr,                                        PREC_NONE }, // BANG,
		// Bitwise
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_BIT_AND }, // AMPERSAND,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_BIT_OR }, // PIPE,
		{ &GDScriptParser::parse_unary_operator,         	nullptr,                                        PREC_NONE }, // TILDE,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_BIT_XOR }, // CARET,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_BIT_SHIFT }, // LESS_LESS,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_BIT_SHIFT }, // GREATER_GREATER,
		// Math
		{ &GDScriptParser::parse_unary_operator,         	&GDScriptParser::parse_binary_operator,      	PREC_ADDITION_SUBTRACTION }, // PLUS,
		{ &GDScriptParser::parse_unary_operator,         	&GDScriptParser::parse_binary_operator,      	PREC_ADDITION_SUBTRACTION }, // MINUS,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_FACTOR }, // STAR,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_POWER }, // STAR_STAR,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_FACTOR }, // SLASH,
		{ &GDScriptParser::parse_get_node,                  &GDScriptParser::parse_binary_operator,      	PREC_FACTOR }, // PERCENT,
		// Assignment
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // PLUS_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // MINUS_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // STAR_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // STAR_STAR_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // SLASH_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // PERCENT_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // LESS_LESS_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // GREATER_GREATER_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // AMPERSAND_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // PIPE_EQUAL,
		{ nullptr,                                          &GDScriptParser::parse_assignment,           	PREC_ASSIGNMENT }, // CARET_EQUAL,
		// Control flow
		{ nullptr,                                          &GDScriptParser::parse_ternary_operator,     	PREC_TERNARY }, // IF,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ELIF,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ELSE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // FOR,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // WHILE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BREAK,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CONTINUE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // PASS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // RETURN,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // MATCH,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // WHEN,
		// Keywords
		{ nullptr,                                          &GDScriptParser::parse_cast,                 	PREC_CAST }, // AS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ASSERT,
		{ &GDScriptParser::parse_await,                  	nullptr,                                        PREC_NONE }, // AWAIT,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BREAKPOINT,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CLASS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CLASS_NAME,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // CONST,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ENUM,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // EXTENDS,
		{ &GDScriptParser::parse_lambda,                    nullptr,                                        PREC_NONE }, // FUNC,
		{ nullptr,                                          &GDScriptParser::parse_binary_operator,      	PREC_CONTENT_TEST }, // IN,
		{ nullptr,                                          &GDScriptParser::parse_type_test,            	PREC_TYPE_TEST }, // IS,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // NAMESPACE,
		{ &GDScriptParser::parse_preload,					nullptr,                                        PREC_NONE }, // PRELOAD,
		{ &GDScriptParser::parse_self,                   	nullptr,                                        PREC_NONE }, // SELF,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // SIGNAL,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // STATIC,
		{ &GDScriptParser::parse_call,						nullptr,                                        PREC_NONE }, // SUPER,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // TRAIT,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // VAR,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // VOID,
		{ &GDScriptParser::parse_yield,                     nullptr,                                        PREC_NONE }, // YIELD,
		// Punctuation
		{ &GDScriptParser::parse_array,                  	&GDScriptParser::parse_subscript,            	PREC_SUBSCRIPT }, // BRACKET_OPEN,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BRACKET_CLOSE,
		{ &GDScriptParser::parse_dictionary,             	nullptr,                                        PREC_NONE }, // BRACE_OPEN,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BRACE_CLOSE,
		{ &GDScriptParser::parse_grouping,               	&GDScriptParser::parse_call,                 	PREC_CALL }, // PARENTHESIS_OPEN,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // PARENTHESIS_CLOSE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // COMMA,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // SEMICOLON,
		{ nullptr,                                          &GDScriptParser::parse_attribute,            	PREC_ATTRIBUTE }, // PERIOD,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // PERIOD_PERIOD,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // COLON,
		{ &GDScriptParser::parse_get_node,               	nullptr,                                        PREC_NONE }, // DOLLAR,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // FORWARD_ARROW,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // UNDERSCORE,
		// Whitespace
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // NEWLINE,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // INDENT,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // DEDENT,
		// Constants
		{ &GDScriptParser::parse_builtin_constant,			nullptr,                                        PREC_NONE }, // CONST_PI,
		{ &GDScriptParser::parse_builtin_constant,			nullptr,                                        PREC_NONE }, // CONST_TAU,
		{ &GDScriptParser::parse_builtin_constant,			nullptr,                                        PREC_NONE }, // CONST_INF,
		{ &GDScriptParser::parse_builtin_constant,			nullptr,                                        PREC_NONE }, // CONST_NAN,
		// Error message improvement
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // VCS_CONFLICT_MARKER,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // BACKTICK,
		{ nullptr,                                          &GDScriptParser::parse_invalid_token,        	PREC_CAST }, // QUESTION_MARK,
		// Special
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // ERROR,
		{ nullptr,                                          nullptr,                                        PREC_NONE }, // TK_EOF,
	};
	/* clang-format on */
	// Avoid desync.
	static_assert(sizeof(rules) / sizeof(rules[0]) == GDScriptTokenizer::Token::TK_MAX, "Amount of parse rules don't match the amount of token types.");

	// Let's assume this is never invalid, since nothing generates a TK_MAX.
	return &rules[p_token_type];
}

bool GDScriptParser::SuiteNode::has_local(const StringName &p_name) const {
	if (locals_indices.has(p_name)) {
		return true;
	}
	if (parent_block != nullptr) {
		return parent_block->has_local(p_name);
	}
	return false;
}

const GDScriptParser::SuiteNode::Local &GDScriptParser::SuiteNode::get_local(const StringName &p_name) const {
	if (locals_indices.has(p_name)) {
		return locals[locals_indices[p_name]];
	}
	if (parent_block != nullptr) {
		return parent_block->get_local(p_name);
	}
	return empty;
}

bool GDScriptParser::AnnotationNode::apply(GDScriptParser *p_this, Node *p_target, ClassNode *p_class) {
	if (is_applied) {
		return true;
	}
	is_applied = true;
	return (p_this->*(p_this->valid_annotations[name].apply))(this, p_target, p_class);
}

bool GDScriptParser::AnnotationNode::applies_to(uint32_t p_target_kinds) const {
	return (info->target_kind & p_target_kinds) > 0;
}

bool GDScriptParser::validate_annotation_arguments(AnnotationNode *p_annotation) {
	ERR_FAIL_COND_V_MSG(!valid_annotations.has(p_annotation->name), false, vformat(R"(Annotation "%s" not found to validate.)", p_annotation->name));

	const MethodInfo &info = valid_annotations[p_annotation->name].info;

	if (((info.flags & METHOD_FLAG_VARARG) == 0) && p_annotation->arguments.size() > info.arguments.size()) {
		push_error(vformat(R"(Annotation "%s" requires at most %d arguments, but %d were given.)", p_annotation->name, info.arguments.size(), p_annotation->arguments.size()));
		return false;
	}

	if (p_annotation->arguments.size() < info.arguments.size() - info.default_arguments.size()) {
		push_error(vformat(R"(Annotation "%s" requires at least %d arguments, but %d were given.)", p_annotation->name, info.arguments.size() - info.default_arguments.size(), p_annotation->arguments.size()));
		return false;
	}

	// Some annotations need to be resolved and applied in the parser.
	if (p_annotation->name == SNAME("@icon") || p_annotation->name == SNAME("@warning_ignore_start") || p_annotation->name == SNAME("@warning_ignore_restore")) {
		for (int i = 0; i < p_annotation->arguments.size(); i++) {
			ExpressionNode *argument = p_annotation->arguments[i];

			if (argument->type != Node::LITERAL) {
				push_error(vformat(R"(Argument %d of annotation "%s" must be a string literal.)", i + 1, p_annotation->name), argument);
				return false;
			}

			Variant value = static_cast<LiteralNode *>(argument)->value;

			if (value.get_type() != Variant::STRING) {
				push_error(vformat(R"(Argument %d of annotation "%s" must be a string literal.)", i + 1, p_annotation->name), argument);
				return false;
			}

			p_annotation->resolved_arguments.push_back(value);
		}
	}

	// For other annotations, see `GDScriptAnalyzer::resolve_annotation()`.

	return true;
}

bool GDScriptParser::tool_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
#ifdef DEBUG_ENABLED
	if (_is_tool) {
		push_error(R"("@tool" annotation can only be used once.)", p_annotation);
		return false;
	}
#endif // DEBUG_ENABLED
	_is_tool = true;
	return true;
}

bool GDScriptParser::icon_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
	ERR_FAIL_COND_V_MSG(p_target->type != Node::CLASS, false, R"("@icon" annotation can only be applied to classes.)");
	ERR_FAIL_COND_V(p_annotation->resolved_arguments.is_empty(), false);

	ClassNode *class_node = static_cast<ClassNode *>(p_target);
	String path = p_annotation->resolved_arguments[0];

#ifdef DEBUG_ENABLED
	if (!class_node->icon_path.is_empty()) {
		push_error(R"("@icon" annotation can only be used once.)", p_annotation);
		return false;
	}
	if (path.is_empty()) {
		push_error(R"("@icon" annotation argument must contain the path to the icon.)", p_annotation->arguments[0]);
		return false;
	}
#endif // DEBUG_ENABLED

	class_node->icon_path = path;

	if (path.is_empty() || path.is_absolute_path()) {
		class_node->simplified_icon_path = path.simplify_path();
	} else if (path.is_relative_path()) {
		class_node->simplified_icon_path = script_path.get_base_dir().path_join(path).simplify_path();
	} else {
		class_node->simplified_icon_path = path;
	}

	return true;
}

bool GDScriptParser::static_unload_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
	ERR_FAIL_COND_V_MSG(p_target->type != Node::CLASS, false, vformat(R"("%s" annotation can only be applied to classes.)", p_annotation->name));
	ClassNode *class_node = static_cast<ClassNode *>(p_target);
	if (class_node->annotated_static_unload) {
		push_error(vformat(R"("%s" annotation can only be used once per script.)", p_annotation->name), p_annotation);
		return false;
	}
	class_node->annotated_static_unload = true;
	return true;
}

bool GDScriptParser::onready_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
	ERR_FAIL_COND_V_MSG(p_target->type != Node::VARIABLE, false, R"("@onready" annotation can only be applied to class variables.)");

	if (current_class && !ClassDB::is_parent_class(current_class->get_datatype().native_type, SNAME("Node"))) {
		push_error(R"("@onready" can only be used in classes that inherit "Node".)", p_annotation);
		return false;
	}

	VariableNode *variable = static_cast<VariableNode *>(p_target);
	if (variable->is_static) {
		push_error(R"("@onready" annotation cannot be applied to a static variable.)", p_annotation);
		return false;
	}
	if (variable->onready) {
		push_error(R"("@onready" annotation can only be used once per variable.)", p_annotation);
		return false;
	}
	variable->onready = true;
	current_class->onready_used = true;
	return true;
}

static String _get_annotation_error_string(const StringName &p_annotation_name, const Vector<Variant::Type> &p_expected_types, const GDScriptParser::DataType &p_provided_type) {
	Vector<String> types;
	for (int i = 0; i < p_expected_types.size(); i++) {
		const Variant::Type &type = p_expected_types[i];
		types.push_back(Variant::get_type_name(type));
		types.push_back("Array[" + Variant::get_type_name(type) + "]");
		switch (type) {
			case Variant::INT:
				types.push_back("PackedByteArray");
				types.push_back("PackedInt32Array");
				types.push_back("PackedInt64Array");
				break;
			case Variant::FLOAT:
				types.push_back("PackedFloat32Array");
				types.push_back("PackedFloat64Array");
				break;
			case Variant::STRING:
				types.push_back("PackedStringArray");
				break;
			case Variant::VECTOR2:
				types.push_back("PackedVector2Array");
				break;
			case Variant::VECTOR3:
				types.push_back("PackedVector3Array");
				break;
			case Variant::COLOR:
				types.push_back("PackedColorArray");
				break;
			case Variant::VECTOR4:
				types.push_back("PackedVector4Array");
				break;
			default:
				break;
		}
	}

	String string;
	if (types.size() == 1) {
		string = types[0].quote();
	} else if (types.size() == 2) {
		string = types[0].quote() + " or " + types[1].quote();
	} else if (types.size() >= 3) {
		string = types[0].quote();
		for (int i = 1; i < types.size() - 1; i++) {
			string += ", " + types[i].quote();
		}
		string += ", or " + types[types.size() - 1].quote();
	}

	return vformat(R"("%s" annotation requires a variable of type %s, but type "%s" was given instead.)", p_annotation_name, string, p_provided_type.to_string());
}

static StringName _find_narrowest_native_or_global_class(const GDScriptParser::DataType &p_type) {
	switch (p_type.kind) {
		case GDScriptParser::DataType::NATIVE: {
			if (p_type.is_meta_type) {
				return Object::get_class_static(); // `GDScriptNativeClass` is not an exposed class.
			}
			return p_type.native_type;
		} break;
		case GDScriptParser::DataType::SCRIPT: {
			Ref<Script> script;
			if (p_type.script_type.is_valid()) {
				script = p_type.script_type;
			} else {
				script = ResourceLoader::load(p_type.script_path, SNAME("Script"));
			}

			if (p_type.is_meta_type) {
				return script.is_valid() ? script->get_class() : Script::get_class_static();
			}
			if (script.is_null()) {
				return p_type.native_type;
			}
			if (script->get_global_name() != StringName()) {
				return script->get_global_name();
			}

			Ref<Script> base_script = script->get_base_script();
			if (base_script.is_null()) {
				return script->get_instance_base_type();
			}

			GDScriptParser::DataType base_type;
			base_type.kind = GDScriptParser::DataType::SCRIPT;
			base_type.builtin_type = Variant::OBJECT;
			base_type.native_type = base_script->get_instance_base_type();
			base_type.script_type = base_script;
			base_type.script_path = base_script->get_path();

			return _find_narrowest_native_or_global_class(base_type);
		} break;
		case GDScriptParser::DataType::CLASS: {
			if (p_type.is_meta_type) {
				return GDScript::get_class_static();
			}
			if (p_type.class_type == nullptr) {
				return p_type.native_type;
			}
			if (p_type.class_type->get_global_name() != StringName()) {
				return p_type.class_type->get_global_name();
			}
			return _find_narrowest_native_or_global_class(p_type.class_type->base_type);
		} break;
		default: {
			ERR_FAIL_V(StringName());
		} break;
	}
}

template <PropertyHint t_hint, Variant::Type t_type>
bool GDScriptParser::export_annotations(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
	ERR_FAIL_COND_V_MSG(p_target->type != Node::VARIABLE, false, vformat(R"("%s" annotation can only be applied to variables.)", p_annotation->name));
	ERR_FAIL_NULL_V(p_class, false);

	VariableNode *variable = static_cast<VariableNode *>(p_target);
	if (variable->is_static) {
		push_error(vformat(R"(Annotation "%s" cannot be applied to a static variable.)", p_annotation->name), p_annotation);
		return false;
	}
	if (variable->exported) {
		push_error(vformat(R"(Annotation "%s" cannot be used with another "@export" annotation.)", p_annotation->name), p_annotation);
		return false;
	}

	variable->exported = true;

	variable->export_info.type = t_type;
	variable->export_info.hint = t_hint;

	String hint_string;
	for (int i = 0; i < p_annotation->resolved_arguments.size(); i++) {
		String arg_string = String(p_annotation->resolved_arguments[i]);

		if (p_annotation->name != SNAME("@export_placeholder")) {
			if (arg_string.is_empty()) {
				push_error(vformat(R"(Argument %d of annotation "%s" is empty.)", i + 1, p_annotation->name), p_annotation->arguments[i]);
				return false;
			}
			if (arg_string.contains_char(',')) {
				push_error(vformat(R"(Argument %d of annotation "%s" contains a comma. Use separate arguments instead.)", i + 1, p_annotation->name), p_annotation->arguments[i]);
				return false;
			}
		}

		// WARNING: Do not merge with the previous `if` because there `!=`, not `==`!
		if (p_annotation->name == SNAME("@export_flags")) {
			const int64_t max_flags = 32;
			Vector<String> t = arg_string.split(":", true, 1);
			if (t[0].is_empty()) {
				push_error(vformat(R"(Invalid argument %d of annotation "@export_flags": Expected flag name.)", i + 1), p_annotation->arguments[i]);
				return false;
			}
			if (t.size() == 2) {
				if (t[1].is_empty()) {
					push_error(vformat(R"(Invalid argument %d of annotation "@export_flags": Expected flag value.)", i + 1), p_annotation->arguments[i]);
					return false;
				}
				if (!t[1].is_valid_int()) {
					push_error(vformat(R"(Invalid argument %d of annotation "@export_flags": The flag value must be a valid integer.)", i + 1), p_annotation->arguments[i]);
					return false;
				}
				int64_t value = t[1].to_int();
				if (value < 1 || value >= (1LL << max_flags)) {
					push_error(vformat(R"(Invalid argument %d of annotation "@export_flags": The flag value must be at least 1 and at most 2 ** %d - 1.)", i + 1, max_flags), p_annotation->arguments[i]);
					return false;
				}
			} else if (i >= max_flags) {
				push_error(vformat(R"(Invalid argument %d of annotation "@export_flags": Starting from argument %d, the flag value must be specified explicitly.)", i + 1, max_flags + 1), p_annotation->arguments[i]);
				return false;
			}
		} else if (p_annotation->name == SNAME("@export_node_path")) {
			String native_class = arg_string;
			if (ScriptServer::is_global_class(arg_string)) {
				native_class = ScriptServer::get_global_class_native_base(arg_string);
			}
			if (!ClassDB::class_exists(native_class)) {
				push_error(vformat(R"(Invalid argument %d of annotation "@export_node_path": The class "%s" was not found in the global scope.)", i + 1, arg_string), p_annotation->arguments[i]);
				return false;
			} else if (!ClassDB::is_parent_class(native_class, SNAME("Node"))) {
				push_error(vformat(R"(Invalid argument %d of annotation "@export_node_path": The class "%s" does not inherit "Node".)", i + 1, arg_string), p_annotation->arguments[i]);
				return false;
			}
		}

		if (i > 0) {
			hint_string += ",";
		}
		hint_string += arg_string;
	}
	variable->export_info.hint_string = hint_string;

	// This is called after the analyzer is done finding the type, so this should be set here.
	DataType export_type = variable->get_datatype();

	// Use initializer type if specified type is `Variant`.
	if (export_type.is_variant() && variable->initializer != nullptr && variable->initializer->datatype.is_set()) {
		export_type = variable->initializer->get_datatype();
		export_type.type_source = DataType::INFERRED;
	}

	const Variant::Type original_export_type_builtin = export_type.builtin_type;

	// Process array and packed array annotations on the element type.
	bool is_array = false;
	if (export_type.builtin_type == Variant::ARRAY && export_type.has_container_element_type(0)) {
		is_array = true;
		export_type = export_type.get_container_element_type(0);
	} else if (export_type.is_typed_container_type()) {
		is_array = true;
		export_type = export_type.get_typed_container_type();
		export_type.type_source = variable->datatype.type_source;
	}

	bool is_dict = false;
	if (export_type.builtin_type == Variant::DICTIONARY && export_type.has_container_element_types()) {
		is_dict = true;
		DataType inner_type = export_type.get_container_element_type_or_variant(1);
		export_type = export_type.get_container_element_type_or_variant(0);
		export_type.set_container_element_type(0, inner_type); // Store earlier extracted value within key to separately parse after.
	}

	bool use_default_variable_type_check = true;

	if (p_annotation->name == SNAME("@export_range")) {
		if (export_type.builtin_type == Variant::INT) {
			variable->export_info.type = Variant::INT;
		}
	} else if (p_annotation->name == SNAME("@export_multiline")) {
		use_default_variable_type_check = false;

		if (export_type.builtin_type != Variant::STRING && export_type.builtin_type != Variant::DICTIONARY) {
			Vector<Variant::Type> expected_types = { Variant::STRING, Variant::DICTIONARY };
			push_error(_get_annotation_error_string(p_annotation->name, expected_types, variable->get_datatype()), p_annotation);
			return false;
		}

		if (export_type.builtin_type == Variant::DICTIONARY) {
			variable->export_info.type = Variant::DICTIONARY;
		}
	} else if (p_annotation->name == SNAME("@export")) {
		use_default_variable_type_check = false;

		if (variable->datatype_specifier == nullptr && variable->initializer == nullptr) {
			push_error(R"(Cannot use simple "@export" annotation with variable without type or initializer, since type can't be inferred.)", p_annotation);
			return false;
		}

		if (export_type.is_variant() || export_type.has_no_type()) {
			if (is_dict) {
				// Dictionary allowed to have a variant key/value.
				export_type.kind = GDScriptParser::DataType::BUILTIN;
			} else {
				push_error(R"(Cannot use simple "@export" annotation because the type of the initialized value can't be inferred.)", p_annotation);
				return false;
			}
		}

		switch (export_type.kind) {
			case GDScriptParser::DataType::BUILTIN:
				variable->export_info.type = export_type.builtin_type;
				variable->export_info.hint = PROPERTY_HINT_NONE;
				variable->export_info.hint_string = String();
				break;
			case GDScriptParser::DataType::NATIVE:
			case GDScriptParser::DataType::SCRIPT:
			case GDScriptParser::DataType::CLASS: {
				const StringName class_name = _find_narrowest_native_or_global_class(export_type);
				if (ClassDB::is_parent_class(export_type.native_type, SNAME("Resource"))) {
					variable->export_info.type = Variant::OBJECT;
					variable->export_info.hint = PROPERTY_HINT_RESOURCE_TYPE;
					variable->export_info.hint_string = class_name;
				} else if (ClassDB::is_parent_class(export_type.native_type, SNAME("Node"))) {
					variable->export_info.type = Variant::OBJECT;
					variable->export_info.hint = PROPERTY_HINT_NODE_TYPE;
					variable->export_info.hint_string = class_name;
				} else {
					push_error(R"(Export type can only be built-in, a resource, a node, or an enum.)", p_annotation);
					return false;
				}
			} break;
			case GDScriptParser::DataType::ENUM: {
				if (export_type.is_meta_type) {
					variable->export_info.type = Variant::DICTIONARY;
				} else {
					variable->export_info.type = Variant::INT;
					variable->export_info.hint = PROPERTY_HINT_ENUM;

					String enum_hint_string;
					bool first = true;
					for (const KeyValue<StringName, int64_t> &E : export_type.enum_values) {
						if (!first) {
							enum_hint_string += ",";
						} else {
							first = false;
						}
						enum_hint_string += E.key.operator String().capitalize().xml_escape();
						enum_hint_string += ":";
						enum_hint_string += String::num_int64(E.value).xml_escape();
					}

					variable->export_info.hint_string = enum_hint_string;
					variable->export_info.usage |= PROPERTY_USAGE_CLASS_IS_ENUM;
					variable->export_info.class_name = String(export_type.native_type).replace("::", ".");
				}
			} break;
			default:
				push_error(R"(Export type can only be built-in, a resource, a node, or an enum.)", p_annotation);
				return false;
		}

		if (variable->export_info.hint == PROPERTY_HINT_NODE_TYPE && !ClassDB::is_parent_class(p_class->base_type.native_type, SNAME("Node"))) {
			push_error(vformat(R"(Node export is only supported in Node-derived classes, but the current class inherits "%s".)", p_class->base_type.to_string()), p_annotation);
			return false;
		}

		if (is_dict) {
			String key_prefix = itos(variable->export_info.type);
			if (variable->export_info.hint) {
				key_prefix += "/" + itos(variable->export_info.hint);
			}
			key_prefix += ":" + variable->export_info.hint_string;

			// Now parse value.
			export_type = export_type.get_container_element_type(0);

			if (export_type.is_variant() || export_type.has_no_type()) {
				export_type.kind = GDScriptParser::DataType::BUILTIN;
			}
			switch (export_type.kind) {
				case GDScriptParser::DataType::BUILTIN:
					variable->export_info.type = export_type.builtin_type;
					variable->export_info.hint = PROPERTY_HINT_NONE;
					variable->export_info.hint_string = String();
					break;
				case GDScriptParser::DataType::NATIVE:
				case GDScriptParser::DataType::SCRIPT:
				case GDScriptParser::DataType::CLASS: {
					const StringName class_name = _find_narrowest_native_or_global_class(export_type);
					if (ClassDB::is_parent_class(export_type.native_type, SNAME("Resource"))) {
						variable->export_info.type = Variant::OBJECT;
						variable->export_info.hint = PROPERTY_HINT_RESOURCE_TYPE;
						variable->export_info.hint_string = class_name;
					} else if (ClassDB::is_parent_class(export_type.native_type, SNAME("Node"))) {
						variable->export_info.type = Variant::OBJECT;
						variable->export_info.hint = PROPERTY_HINT_NODE_TYPE;
						variable->export_info.hint_string = class_name;
					} else {
						push_error(R"(Export type can only be built-in, a resource, a node, or an enum.)", p_annotation);
						return false;
					}
				} break;
				case GDScriptParser::DataType::ENUM: {
					if (export_type.is_meta_type) {
						variable->export_info.type = Variant::DICTIONARY;
					} else {
						variable->export_info.type = Variant::INT;
						variable->export_info.hint = PROPERTY_HINT_ENUM;

						String enum_hint_string;
						bool first = true;
						for (const KeyValue<StringName, int64_t> &E : export_type.enum_values) {
							if (!first) {
								enum_hint_string += ",";
							} else {
								first = false;
							}
							enum_hint_string += E.key.operator String().capitalize().xml_escape();
							enum_hint_string += ":";
							enum_hint_string += String::num_int64(E.value).xml_escape();
						}

						variable->export_info.hint_string = enum_hint_string;
						variable->export_info.usage |= PROPERTY_USAGE_CLASS_IS_ENUM;
						variable->export_info.class_name = String(export_type.native_type).replace("::", ".");
					}
				} break;
				default:
					push_error(R"(Export type can only be built-in, a resource, a node, or an enum.)", p_annotation);
					return false;
			}

			if (variable->export_info.hint == PROPERTY_HINT_NODE_TYPE && !ClassDB::is_parent_class(p_class->base_type.native_type, SNAME("Node"))) {
				push_error(vformat(R"(Node export is only supported in Node-derived classes, but the current class inherits "%s".)", p_class->base_type.to_string()), p_annotation);
				return false;
			}

			String value_prefix = itos(variable->export_info.type);
			if (variable->export_info.hint) {
				value_prefix += "/" + itos(variable->export_info.hint);
			}
			value_prefix += ":" + variable->export_info.hint_string;

			variable->export_info.type = Variant::DICTIONARY;
			variable->export_info.hint = PROPERTY_HINT_TYPE_STRING;
			variable->export_info.hint_string = key_prefix + ";" + value_prefix;
			variable->export_info.usage = PROPERTY_USAGE_DEFAULT;
			variable->export_info.class_name = StringName();
		}
	} else if (p_annotation->name == SNAME("@export_enum")) {
		use_default_variable_type_check = false;

		Variant::Type enum_type = Variant::INT;

		if (export_type.kind == DataType::BUILTIN && export_type.builtin_type == Variant::STRING) {
			enum_type = Variant::STRING;
		}

		variable->export_info.type = enum_type;

		if (!export_type.is_variant() && (export_type.kind != DataType::BUILTIN || export_type.builtin_type != enum_type)) {
			Vector<Variant::Type> expected_types = { Variant::INT, Variant::STRING };
			push_error(_get_annotation_error_string(p_annotation->name, expected_types, variable->get_datatype()), p_annotation);
			return false;
		}
	}

	if (use_default_variable_type_check) {
		// Validate variable type with export.
		if (!export_type.is_variant() && (export_type.kind != DataType::BUILTIN || export_type.builtin_type != t_type)) {
			// Allow float/int conversion.
			if ((t_type != Variant::FLOAT || export_type.builtin_type != Variant::INT) && (t_type != Variant::INT || export_type.builtin_type != Variant::FLOAT)) {
				Vector<Variant::Type> expected_types = { t_type };
				push_error(_get_annotation_error_string(p_annotation->name, expected_types, variable->get_datatype()), p_annotation);
				return false;
			}
		}
	}

	if (is_array) {
		String hint_prefix = itos(variable->export_info.type);
		if (variable->export_info.hint) {
			hint_prefix += "/" + itos(variable->export_info.hint);
		}
		variable->export_info.type = original_export_type_builtin;
		variable->export_info.hint = PROPERTY_HINT_TYPE_STRING;
		variable->export_info.hint_string = hint_prefix + ":" + variable->export_info.hint_string;
		variable->export_info.usage = PROPERTY_USAGE_DEFAULT;
		variable->export_info.class_name = StringName();
	}

	return true;
}

// For `@export_storage` and `@export_custom`, there is no need to check the variable type, argument values,
// or handle array exports in a special way, so they are implemented as separate methods.

bool GDScriptParser::export_storage_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
	ERR_FAIL_COND_V_MSG(p_target->type != Node::VARIABLE, false, vformat(R"("%s" annotation can only be applied to variables.)", p_annotation->name));

	VariableNode *variable = static_cast<VariableNode *>(p_target);
	if (variable->is_static) {
		push_error(vformat(R"(Annotation "%s" cannot be applied to a static variable.)", p_annotation->name), p_annotation);
		return false;
	}
	if (variable->exported) {
		push_error(vformat(R"(Annotation "%s" cannot be used with another "@export" annotation.)", p_annotation->name), p_annotation);
		return false;
	}

	variable->exported = true;

	// Save the info because the compiler uses export info for overwriting member info.
	variable->export_info = variable->get_datatype().to_property_info(variable->identifier->name);
	variable->export_info.usage |= PROPERTY_USAGE_STORAGE;

	return true;
}

bool GDScriptParser::export_custom_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
	ERR_FAIL_COND_V_MSG(p_target->type != Node::VARIABLE, false, vformat(R"("%s" annotation can only be applied to variables.)", p_annotation->name));
	ERR_FAIL_COND_V_MSG(p_annotation->resolved_arguments.size() < 2, false, R"(Annotation "@export_custom" requires 2 arguments.)");

	VariableNode *variable = static_cast<VariableNode *>(p_target);
	if (variable->is_static) {
		push_error(vformat(R"(Annotation "%s" cannot be applied to a static variable.)", p_annotation->name), p_annotation);
		return false;
	}
	if (variable->exported) {
		push_error(vformat(R"(Annotation "%s" cannot be used with another "@export" annotation.)", p_annotation->name), p_annotation);
		return false;
	}

	variable->exported = true;

	DataType export_type = variable->get_datatype();

	variable->export_info.type = export_type.builtin_type;
	variable->export_info.hint = static_cast<PropertyHint>(p_annotation->resolved_arguments[0].operator int64_t());
	variable->export_info.hint_string = p_annotation->resolved_arguments[1];

	if (p_annotation->resolved_arguments.size() >= 3) {
		variable->export_info.usage = p_annotation->resolved_arguments[2].operator int64_t();
	}
	return true;
}

bool GDScriptParser::export_tool_button_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(p_target->type != Node::VARIABLE, false, vformat(R"("%s" annotation can only be applied to variables.)", p_annotation->name));
	ERR_FAIL_COND_V(p_annotation->resolved_arguments.is_empty(), false);

	if (!is_tool()) {
		push_error(R"(Tool buttons can only be used in tool scripts (add "@tool" to the top of the script).)", p_annotation);
		return false;
	}

	VariableNode *variable = static_cast<VariableNode *>(p_target);

	if (variable->is_static) {
		push_error(vformat(R"(Annotation "%s" cannot be applied to a static variable.)", p_annotation->name), p_annotation);
		return false;
	}
	if (variable->exported) {
		push_error(vformat(R"(Annotation "%s" cannot be used with another "@export" annotation.)", p_annotation->name), p_annotation);
		return false;
	}

	const DataType variable_type = variable->get_datatype();
	if (!variable_type.is_variant() && variable_type.is_hard_type()) {
		if (variable_type.kind != DataType::BUILTIN || variable_type.builtin_type != Variant::CALLABLE) {
			push_error(vformat(R"("@export_tool_button" annotation requires a variable of type "Callable", but type "%s" was given instead.)", variable_type.to_string()), p_annotation);
			return false;
		}
	}

	variable->exported = true;

	// Build the hint string (format: `<text>[,<icon>]`).
	String hint_string = p_annotation->resolved_arguments[0].operator String(); // Button text.
	if (p_annotation->resolved_arguments.size() > 1) {
		hint_string += "," + p_annotation->resolved_arguments[1].operator String(); // Button icon.
	}

	variable->export_info.type = Variant::CALLABLE;
	variable->export_info.hint = PROPERTY_HINT_TOOL_BUTTON;
	variable->export_info.hint_string = hint_string;
	variable->export_info.usage = PROPERTY_USAGE_EDITOR;
#endif // TOOLS_ENABLED

	return true; // Only available in editor.
}

template <PropertyUsageFlags t_usage>
bool GDScriptParser::export_group_annotations(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
	ERR_FAIL_COND_V(p_annotation->resolved_arguments.is_empty(), false);

	p_annotation->export_info.name = p_annotation->resolved_arguments[0];

	switch (t_usage) {
		case PROPERTY_USAGE_CATEGORY: {
			p_annotation->export_info.usage = t_usage;
		} break;

		case PROPERTY_USAGE_GROUP: {
			p_annotation->export_info.usage = t_usage;
			if (p_annotation->resolved_arguments.size() == 2) {
				p_annotation->export_info.hint_string = p_annotation->resolved_arguments[1];
			}
		} break;

		case PROPERTY_USAGE_SUBGROUP: {
			p_annotation->export_info.usage = t_usage;
			if (p_annotation->resolved_arguments.size() == 2) {
				p_annotation->export_info.hint_string = p_annotation->resolved_arguments[1];
			}
		} break;
	}

	return true;
}

bool GDScriptParser::warning_ignore_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
#ifdef DEBUG_ENABLED
	if (is_ignoring_warnings) {
		return true; // We already ignore all warnings, let's optimize it.
	}

	bool has_error = false;
	for (const Variant &warning_name : p_annotation->resolved_arguments) {
		GDScriptWarning::Code warning_code = GDScriptWarning::get_code_from_name(String(warning_name).to_upper());
		if (warning_code == GDScriptWarning::WARNING_MAX) {
			push_error(vformat(R"(Invalid warning name: "%s".)", warning_name), p_annotation);
			has_error = true;
		} else {
			int start_line = p_annotation->start_line;
			int end_line = p_target->end_line;

			switch (p_target->type) {
#define SIMPLE_CASE(m_type, m_class, m_property)          \
	case m_type: {                                        \
		m_class *node = static_cast<m_class *>(p_target); \
		if (node->m_property == nullptr) {                \
			end_line = node->start_line;                  \
		} else {                                          \
			end_line = node->m_property->end_line;        \
		}                                                 \
	} break;

				// Can contain properties (set/get).
				SIMPLE_CASE(Node::VARIABLE, VariableNode, initializer)

				// Contain bodies.
				SIMPLE_CASE(Node::FOR, ForNode, list)
				SIMPLE_CASE(Node::IF, IfNode, condition)
				SIMPLE_CASE(Node::MATCH, MatchNode, test)
				SIMPLE_CASE(Node::WHILE, WhileNode, condition)
#undef SIMPLE_CASE

				case Node::CLASS: {
					end_line = p_target->start_line;
					for (const AnnotationNode *annotation : p_target->annotations) {
						start_line = MIN(start_line, annotation->start_line);
						end_line = MAX(end_line, annotation->end_line);
					}
				} break;

				case Node::FUNCTION: {
					FunctionNode *function = static_cast<FunctionNode *>(p_target);
					end_line = function->start_line;
					for (int i = 0; i < function->parameters.size(); i++) {
						end_line = MAX(end_line, function->parameters[i]->end_line);
						if (function->parameters[i]->initializer != nullptr) {
							end_line = MAX(end_line, function->parameters[i]->initializer->end_line);
						}
					}
				} break;

				case Node::MATCH_BRANCH: {
					MatchBranchNode *branch = static_cast<MatchBranchNode *>(p_target);
					end_line = branch->start_line;
					for (int i = 0; i < branch->patterns.size(); i++) {
						end_line = MAX(end_line, branch->patterns[i]->end_line);
					}
				} break;

				default: {
				} break;
			}

			end_line = MAX(start_line, end_line); // Prevent infinite loop.
			for (int line = start_line; line <= end_line; line++) {
				warning_ignored_lines[warning_code].insert(line);
			}
		}
	}
	return !has_error;
#else // !DEBUG_ENABLED
	// Only available in debug builds.
	return true;
#endif // DEBUG_ENABLED
}

bool GDScriptParser::warning_ignore_region_annotations(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
#ifdef DEBUG_ENABLED
	bool has_error = false;
	const bool is_start = p_annotation->name == SNAME("@warning_ignore_start");
	for (const Variant &warning_name : p_annotation->resolved_arguments) {
		GDScriptWarning::Code warning_code = GDScriptWarning::get_code_from_name(String(warning_name).to_upper());
		if (warning_code == GDScriptWarning::WARNING_MAX) {
			push_error(vformat(R"(Invalid warning name: "%s".)", warning_name), p_annotation);
			has_error = true;
			continue;
		}
		if (is_start) {
			if (warning_ignore_start_lines[warning_code] != INT_MAX) {
				push_error(vformat(R"(Warning "%s" is already being ignored by "@warning_ignore_start" at line %d.)", String(warning_name).to_upper(), warning_ignore_start_lines[warning_code]), p_annotation);
				has_error = true;
				continue;
			}
			warning_ignore_start_lines[warning_code] = p_annotation->start_line;
		} else {
			if (warning_ignore_start_lines[warning_code] == INT_MAX) {
				push_error(vformat(R"(Warning "%s" is not being ignored by "@warning_ignore_start".)", String(warning_name).to_upper()), p_annotation);
				has_error = true;
				continue;
			}
			const int start_line = warning_ignore_start_lines[warning_code];
			const int end_line = MAX(start_line, p_annotation->start_line); // Prevent infinite loop.
			for (int i = start_line; i <= end_line; i++) {
				warning_ignored_lines[warning_code].insert(i);
			}
			warning_ignore_start_lines[warning_code] = INT_MAX;
		}
	}
	return !has_error;
#else // !DEBUG_ENABLED
	// Only available in debug builds.
	return true;
#endif // DEBUG_ENABLED
}

bool GDScriptParser::rpc_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class) {
	ERR_FAIL_COND_V_MSG(p_target->type != Node::FUNCTION, false, vformat(R"("%s" annotation can only be applied to functions.)", p_annotation->name));

	FunctionNode *function = static_cast<FunctionNode *>(p_target);
	if (function->rpc_config.get_type() != Variant::NIL) {
		push_error(R"(RPC annotations can only be used once per function.)", p_annotation);
		return false;
	}

	Dictionary rpc_config;
	rpc_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	if (!p_annotation->resolved_arguments.is_empty()) {
		unsigned char locality_args = 0;
		unsigned char permission_args = 0;
		unsigned char transfer_mode_args = 0;

		for (int i = 0; i < p_annotation->resolved_arguments.size(); i++) {
			if (i == 3) {
				rpc_config["channel"] = p_annotation->resolved_arguments[i].operator int();
				continue;
			}

			String arg = p_annotation->resolved_arguments[i].operator String();
			if (arg == "call_local") {
				locality_args++;
				rpc_config["call_local"] = true;
			} else if (arg == "call_remote") {
				locality_args++;
				rpc_config["call_local"] = false;
			} else if (arg == "any_peer") {
				permission_args++;
				rpc_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
			} else if (arg == "authority") {
				permission_args++;
				rpc_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
			} else if (arg == "reliable") {
				transfer_mode_args++;
				rpc_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
			} else if (arg == "unreliable") {
				transfer_mode_args++;
				rpc_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_UNRELIABLE;
			} else if (arg == "unreliable_ordered") {
				transfer_mode_args++;
				rpc_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_UNRELIABLE_ORDERED;
			} else {
				push_error(R"(Invalid RPC argument. Must be one of: "call_local"/"call_remote" (local calls), "any_peer"/"authority" (permission), "reliable"/"unreliable"/"unreliable_ordered" (transfer mode).)", p_annotation);
			}
		}

		if (locality_args > 1) {
			push_error(R"(Invalid RPC config. The locality ("call_local"/"call_remote") must be specified no more than once.)", p_annotation);
		} else if (permission_args > 1) {
			push_error(R"(Invalid RPC config. The permission ("any_peer"/"authority") must be specified no more than once.)", p_annotation);
		} else if (transfer_mode_args > 1) {
			push_error(R"(Invalid RPC config. The transfer mode ("reliable"/"unreliable"/"unreliable_ordered") must be specified no more than once.)", p_annotation);
		}
	}
	function->rpc_config = rpc_config;
	return true;
}

GDScriptParser::DataType GDScriptParser::SuiteNode::Local::get_datatype() const {
	switch (type) {
		case CONSTANT:
			return constant->get_datatype();
		case VARIABLE:
			return variable->get_datatype();
		case PARAMETER:
			return parameter->get_datatype();
		case FOR_VARIABLE:
		case PATTERN_BIND:
			return bind->get_datatype();
		case UNDEFINED:
			return DataType();
	}
	return DataType();
}

String GDScriptParser::SuiteNode::Local::get_name() const {
	switch (type) {
		case SuiteNode::Local::PARAMETER:
			return "parameter";
		case SuiteNode::Local::CONSTANT:
			return "constant";
		case SuiteNode::Local::VARIABLE:
			return "variable";
		case SuiteNode::Local::FOR_VARIABLE:
			return "for loop iterator";
		case SuiteNode::Local::PATTERN_BIND:
			return "pattern bind";
		case SuiteNode::Local::UNDEFINED:
			return "<undefined>";
		default:
			return String();
	}
}

String GDScriptParser::DataType::to_string() const {
	switch (kind) {
		case VARIANT:
			return "Variant";
		case BUILTIN:
			if (builtin_type == Variant::NIL) {
				return "null";
			}
			if (builtin_type == Variant::ARRAY && has_container_element_type(0)) {
				return vformat("Array[%s]", get_container_element_type(0).to_string());
			}
			if (builtin_type == Variant::DICTIONARY && has_container_element_types()) {
				return vformat("Dictionary[%s, %s]", get_container_element_type_or_variant(0).to_string(), get_container_element_type_or_variant(1).to_string());
			}
			return Variant::get_type_name(builtin_type);
		case NATIVE:
			if (is_meta_type) {
				return GDScriptNativeClass::get_class_static();
			}
			return native_type.operator String();
		case CLASS:
			if (class_type->identifier != nullptr) {
				return class_type->identifier->name.operator String();
			}
			return class_type->fqcn;
		case SCRIPT: {
			if (is_meta_type) {
				return script_type.is_valid() ? script_type->get_class_name().operator String() : "";
			}
			String name = script_type.is_valid() ? script_type->get_name() : "";
			if (!name.is_empty()) {
				return name;
			}
			name = script_path;
			if (!name.is_empty()) {
				return name;
			}
			return native_type.operator String();
		}
		case ENUM: {
			// native_type contains either the native class defining the enum
			// or the fully qualified class name of the script defining the enum
			return String(native_type).get_file(); // Remove path, keep filename
		}
		case RESOLVING:
		case UNRESOLVED:
			return "<unresolved type>";
	}

	ERR_FAIL_V_MSG("<unresolved type>", "Kind set outside the enum range.");
}

PropertyInfo GDScriptParser::DataType::to_property_info(const String &p_name) const {
	PropertyInfo result;
	result.name = p_name;
	result.usage = PROPERTY_USAGE_NONE;

	if (!is_hard_type()) {
		result.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
		return result;
	}

	switch (kind) {
		case BUILTIN:
			result.type = builtin_type;
			if (builtin_type == Variant::ARRAY && has_container_element_type(0)) {
				const DataType elem_type = get_container_element_type(0);
				switch (elem_type.kind) {
					case BUILTIN:
						result.hint = PROPERTY_HINT_ARRAY_TYPE;
						result.hint_string = Variant::get_type_name(elem_type.builtin_type);
						break;
					case NATIVE:
						result.hint = PROPERTY_HINT_ARRAY_TYPE;
						result.hint_string = elem_type.native_type;
						break;
					case SCRIPT:
						result.hint = PROPERTY_HINT_ARRAY_TYPE;
						if (elem_type.script_type.is_valid() && elem_type.script_type->get_global_name() != StringName()) {
							result.hint_string = elem_type.script_type->get_global_name();
						} else {
							result.hint_string = elem_type.native_type;
						}
						break;
					case CLASS:
						result.hint = PROPERTY_HINT_ARRAY_TYPE;
						if (elem_type.class_type != nullptr && elem_type.class_type->get_global_name() != StringName()) {
							result.hint_string = elem_type.class_type->get_global_name();
						} else {
							result.hint_string = elem_type.native_type;
						}
						break;
					case ENUM:
						result.hint = PROPERTY_HINT_ARRAY_TYPE;
						result.hint_string = String(elem_type.native_type).replace("::", ".");
						break;
					case VARIANT:
					case RESOLVING:
					case UNRESOLVED:
						break;
				}
			} else if (builtin_type == Variant::DICTIONARY && has_container_element_types()) {
				const DataType key_type = get_container_element_type_or_variant(0);
				const DataType value_type = get_container_element_type_or_variant(1);
				if ((key_type.kind == VARIANT && value_type.kind == VARIANT) || key_type.kind == RESOLVING ||
						key_type.kind == UNRESOLVED || value_type.kind == RESOLVING || value_type.kind == UNRESOLVED) {
					break;
				}
				String key_hint, value_hint;
				switch (key_type.kind) {
					case BUILTIN:
						key_hint = Variant::get_type_name(key_type.builtin_type);
						break;
					case NATIVE:
						key_hint = key_type.native_type;
						break;
					case SCRIPT:
						if (key_type.script_type.is_valid() && key_type.script_type->get_global_name() != StringName()) {
							key_hint = key_type.script_type->get_global_name();
						} else {
							key_hint = key_type.native_type;
						}
						break;
					case CLASS:
						if (key_type.class_type != nullptr && key_type.class_type->get_global_name() != StringName()) {
							key_hint = key_type.class_type->get_global_name();
						} else {
							key_hint = key_type.native_type;
						}
						break;
					case ENUM:
						key_hint = String(key_type.native_type).replace("::", ".");
						break;
					default:
						key_hint = "Variant";
						break;
				}
				switch (value_type.kind) {
					case BUILTIN:
						value_hint = Variant::get_type_name(value_type.builtin_type);
						break;
					case NATIVE:
						value_hint = value_type.native_type;
						break;
					case SCRIPT:
						if (value_type.script_type.is_valid() && value_type.script_type->get_global_name() != StringName()) {
							value_hint = value_type.script_type->get_global_name();
						} else {
							value_hint = value_type.native_type;
						}
						break;
					case CLASS:
						if (value_type.class_type != nullptr && value_type.class_type->get_global_name() != StringName()) {
							value_hint = value_type.class_type->get_global_name();
						} else {
							value_hint = value_type.native_type;
						}
						break;
					case ENUM:
						value_hint = String(value_type.native_type).replace("::", ".");
						break;
					default:
						value_hint = "Variant";
						break;
				}
				result.hint = PROPERTY_HINT_DICTIONARY_TYPE;
				result.hint_string = key_hint + ";" + value_hint;
			}
			break;
		case NATIVE:
			result.type = Variant::OBJECT;
			if (is_meta_type) {
				result.class_name = GDScriptNativeClass::get_class_static();
			} else {
				result.class_name = native_type;
			}
			break;
		case SCRIPT:
			result.type = Variant::OBJECT;
			if (is_meta_type) {
				result.class_name = script_type.is_valid() ? script_type->get_class() : Script::get_class_static();
			} else if (script_type.is_valid() && script_type->get_global_name() != StringName()) {
				result.class_name = script_type->get_global_name();
			} else {
				result.class_name = native_type;
			}
			break;
		case CLASS:
			result.type = Variant::OBJECT;
			if (is_meta_type) {
				result.class_name = GDScript::get_class_static();
			} else if (class_type != nullptr && class_type->get_global_name() != StringName()) {
				result.class_name = class_type->get_global_name();
			} else {
				result.class_name = native_type;
			}
			break;
		case ENUM:
			if (is_meta_type) {
				result.type = Variant::DICTIONARY;
			} else {
				result.type = Variant::INT;
				result.usage |= PROPERTY_USAGE_CLASS_IS_ENUM;
				result.class_name = String(native_type).replace("::", ".");
			}
			break;
		case VARIANT:
		case RESOLVING:
		case UNRESOLVED:
			result.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
			break;
	}

	return result;
}

static Variant::Type _variant_type_to_typed_array_element_type(Variant::Type p_type) {
	switch (p_type) {
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
			return Variant::INT;
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
			return Variant::FLOAT;
		case Variant::PACKED_STRING_ARRAY:
			return Variant::STRING;
		case Variant::PACKED_VECTOR2_ARRAY:
			return Variant::VECTOR2;
		case Variant::PACKED_VECTOR3_ARRAY:
			return Variant::VECTOR3;
		case Variant::PACKED_COLOR_ARRAY:
			return Variant::COLOR;
		case Variant::PACKED_VECTOR4_ARRAY:
			return Variant::VECTOR4;
		default:
			return Variant::NIL;
	}
}

bool GDScriptParser::DataType::is_typed_container_type() const {
	return kind == GDScriptParser::DataType::BUILTIN && _variant_type_to_typed_array_element_type(builtin_type) != Variant::NIL;
}

GDScriptParser::DataType GDScriptParser::DataType::get_typed_container_type() const {
	GDScriptParser::DataType type;
	type.kind = GDScriptParser::DataType::BUILTIN;
	type.builtin_type = _variant_type_to_typed_array_element_type(builtin_type);
	return type;
}

bool GDScriptParser::DataType::can_reference(const GDScriptParser::DataType &p_other) const {
	if (p_other.is_meta_type) {
		return false;
	} else if (builtin_type != p_other.builtin_type) {
		return false;
	} else if (builtin_type != Variant::OBJECT) {
		return true;
	}

	if (native_type == StringName()) {
		return true;
	} else if (p_other.native_type == StringName()) {
		return false;
	} else if (native_type != p_other.native_type && !ClassDB::is_parent_class(p_other.native_type, native_type)) {
		return false;
	}

	Ref<Script> script = script_type;
	if (kind == GDScriptParser::DataType::CLASS && script.is_null()) {
		Error err = OK;
		Ref<GDScript> scr = GDScriptCache::get_shallow_script(script_path, err);
		ERR_FAIL_COND_V_MSG(err, false, vformat(R"(Error while getting cache for script "%s".)", script_path));
		script.reference_ptr(scr->find_class(class_type->fqcn));
	}

	Ref<Script> script_other = p_other.script_type;
	if (p_other.kind == GDScriptParser::DataType::CLASS && script_other.is_null()) {
		Error err = OK;
		Ref<GDScript> scr = GDScriptCache::get_shallow_script(p_other.script_path, err);
		ERR_FAIL_COND_V_MSG(err, false, vformat(R"(Error while getting cache for script "%s".)", p_other.script_path));
		script_other.reference_ptr(scr->find_class(p_other.class_type->fqcn));
	}

	if (script.is_null()) {
		return true;
	} else if (script_other.is_null()) {
		return false;
	} else if (script != script_other && !script_other->inherits_script(script)) {
		return false;
	}

	return true;
}

void GDScriptParser::complete_extents(Node *p_node) {
	while (!nodes_in_progress.is_empty() && nodes_in_progress.back()->get() != p_node) {
		ERR_PRINT("Parser bug: Mismatch in extents tracking stack.");
		nodes_in_progress.pop_back();
	}
	if (nodes_in_progress.is_empty()) {
		ERR_PRINT("Parser bug: Extents tracking stack is empty.");
	} else {
		nodes_in_progress.pop_back();
	}
}

void GDScriptParser::update_extents(Node *p_node) {
	p_node->end_line = previous.end_line;
	p_node->end_column = previous.end_column;
	p_node->leftmost_column = MIN(p_node->leftmost_column, previous.leftmost_column);
	p_node->rightmost_column = MAX(p_node->rightmost_column, previous.rightmost_column);
}

void GDScriptParser::reset_extents(Node *p_node, GDScriptTokenizer::Token p_token) {
	p_node->start_line = p_token.start_line;
	p_node->end_line = p_token.end_line;
	p_node->start_column = p_token.start_column;
	p_node->end_column = p_token.end_column;
	p_node->leftmost_column = p_token.leftmost_column;
	p_node->rightmost_column = p_token.rightmost_column;
}

void GDScriptParser::reset_extents(Node *p_node, Node *p_from) {
	if (p_from == nullptr) {
		return;
	}
	p_node->start_line = p_from->start_line;
	p_node->end_line = p_from->end_line;
	p_node->start_column = p_from->start_column;
	p_node->end_column = p_from->end_column;
	p_node->leftmost_column = p_from->leftmost_column;
	p_node->rightmost_column = p_from->rightmost_column;
}

/*---------- PRETTY PRINT FOR DEBUG ----------*/

#ifdef DEBUG_ENABLED

void GDScriptParser::TreePrinter::increase_indent() {
	indent_level++;
	indent = "";
	for (int i = 0; i < indent_level * 4; i++) {
		if (i % 4 == 0) {
			indent += "|";
		} else {
			indent += " ";
		}
	}
}

void GDScriptParser::TreePrinter::decrease_indent() {
	indent_level--;
	indent = "";
	for (int i = 0; i < indent_level * 4; i++) {
		if (i % 4 == 0) {
			indent += "|";
		} else {
			indent += " ";
		}
	}
}

void GDScriptParser::TreePrinter::push_line(const String &p_line) {
	if (!p_line.is_empty()) {
		push_text(p_line);
	}
	printed += "\n";
	pending_indent = true;
}

void GDScriptParser::TreePrinter::push_text(const String &p_text) {
	if (pending_indent) {
		printed += indent;
		pending_indent = false;
	}
	printed += p_text;
}

void GDScriptParser::TreePrinter::print_annotation(const AnnotationNode *p_annotation) {
	push_text(p_annotation->name);
	push_text(" (");
	for (int i = 0; i < p_annotation->arguments.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_expression(p_annotation->arguments[i]);
	}
	push_line(")");
}

void GDScriptParser::TreePrinter::print_array(ArrayNode *p_array) {
	push_text("[ ");
	for (int i = 0; i < p_array->elements.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_expression(p_array->elements[i]);
	}
	push_text(" ]");
}

void GDScriptParser::TreePrinter::print_assert(AssertNode *p_assert) {
	push_text("Assert ( ");
	print_expression(p_assert->condition);
	push_line(" )");
}

void GDScriptParser::TreePrinter::print_assignment(AssignmentNode *p_assignment) {
	switch (p_assignment->assignee->type) {
		case Node::IDENTIFIER:
			print_identifier(static_cast<IdentifierNode *>(p_assignment->assignee));
			break;
		case Node::SUBSCRIPT:
			print_subscript(static_cast<SubscriptNode *>(p_assignment->assignee));
			break;
		default:
			break; // Unreachable.
	}

	push_text(" ");
	switch (p_assignment->operation) {
		case AssignmentNode::OP_ADDITION:
			push_text("+");
			break;
		case AssignmentNode::OP_SUBTRACTION:
			push_text("-");
			break;
		case AssignmentNode::OP_MULTIPLICATION:
			push_text("*");
			break;
		case AssignmentNode::OP_DIVISION:
			push_text("/");
			break;
		case AssignmentNode::OP_MODULO:
			push_text("%");
			break;
		case AssignmentNode::OP_POWER:
			push_text("**");
			break;
		case AssignmentNode::OP_BIT_SHIFT_LEFT:
			push_text("<<");
			break;
		case AssignmentNode::OP_BIT_SHIFT_RIGHT:
			push_text(">>");
			break;
		case AssignmentNode::OP_BIT_AND:
			push_text("&");
			break;
		case AssignmentNode::OP_BIT_OR:
			push_text("|");
			break;
		case AssignmentNode::OP_BIT_XOR:
			push_text("^");
			break;
		case AssignmentNode::OP_NONE:
			break;
	}
	push_text("= ");
	print_expression(p_assignment->assigned_value);
	push_line();
}

void GDScriptParser::TreePrinter::print_await(AwaitNode *p_await) {
	push_text("Await ");
	print_expression(p_await->to_await);
}

void GDScriptParser::TreePrinter::print_binary_op(BinaryOpNode *p_binary_op) {
	// Surround in parenthesis for disambiguation.
	push_text("(");
	print_expression(p_binary_op->left_operand);
	switch (p_binary_op->operation) {
		case BinaryOpNode::OP_ADDITION:
			push_text(" + ");
			break;
		case BinaryOpNode::OP_SUBTRACTION:
			push_text(" - ");
			break;
		case BinaryOpNode::OP_MULTIPLICATION:
			push_text(" * ");
			break;
		case BinaryOpNode::OP_DIVISION:
			push_text(" / ");
			break;
		case BinaryOpNode::OP_MODULO:
			push_text(" % ");
			break;
		case BinaryOpNode::OP_POWER:
			push_text(" ** ");
			break;
		case BinaryOpNode::OP_BIT_LEFT_SHIFT:
			push_text(" << ");
			break;
		case BinaryOpNode::OP_BIT_RIGHT_SHIFT:
			push_text(" >> ");
			break;
		case BinaryOpNode::OP_BIT_AND:
			push_text(" & ");
			break;
		case BinaryOpNode::OP_BIT_OR:
			push_text(" | ");
			break;
		case BinaryOpNode::OP_BIT_XOR:
			push_text(" ^ ");
			break;
		case BinaryOpNode::OP_LOGIC_AND:
			push_text(" AND ");
			break;
		case BinaryOpNode::OP_LOGIC_OR:
			push_text(" OR ");
			break;
		case BinaryOpNode::OP_CONTENT_TEST:
			push_text(" IN ");
			break;
		case BinaryOpNode::OP_COMP_EQUAL:
			push_text(" == ");
			break;
		case BinaryOpNode::OP_COMP_NOT_EQUAL:
			push_text(" != ");
			break;
		case BinaryOpNode::OP_COMP_LESS:
			push_text(" < ");
			break;
		case BinaryOpNode::OP_COMP_LESS_EQUAL:
			push_text(" <= ");
			break;
		case BinaryOpNode::OP_COMP_GREATER:
			push_text(" > ");
			break;
		case BinaryOpNode::OP_COMP_GREATER_EQUAL:
			push_text(" >= ");
			break;
	}
	print_expression(p_binary_op->right_operand);
	// Surround in parenthesis for disambiguation.
	push_text(")");
}

void GDScriptParser::TreePrinter::print_call(CallNode *p_call) {
	if (p_call->is_super) {
		push_text("super");
		if (p_call->callee != nullptr) {
			push_text(".");
			print_expression(p_call->callee);
		}
	} else {
		print_expression(p_call->callee);
	}
	push_text("( ");
	for (int i = 0; i < p_call->arguments.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_expression(p_call->arguments[i]);
	}
	push_text(" )");
}

void GDScriptParser::TreePrinter::print_cast(CastNode *p_cast) {
	print_expression(p_cast->operand);
	push_text(" AS ");
	print_type(p_cast->cast_type);
}

void GDScriptParser::TreePrinter::print_class(ClassNode *p_class) {
	push_text("Class ");
	if (p_class->identifier == nullptr) {
		push_text("<unnamed>");
	} else {
		print_identifier(p_class->identifier);
	}

	if (p_class->extends_used) {
		bool first = true;
		push_text(" Extends ");
		if (!p_class->extends_path.is_empty()) {
			push_text(vformat(R"("%s")", p_class->extends_path));
			first = false;
		}
		for (int i = 0; i < p_class->extends.size(); i++) {
			if (!first) {
				push_text(".");
			} else {
				first = false;
			}
			push_text(p_class->extends[i]->name);
		}
	}

	push_line(" :");

	increase_indent();

	for (int i = 0; i < p_class->members.size(); i++) {
		const ClassNode::Member &m = p_class->members[i];

		switch (m.type) {
			case ClassNode::Member::CLASS:
				print_class(m.m_class);
				break;
			case ClassNode::Member::VARIABLE:
				print_variable(m.variable);
				break;
			case ClassNode::Member::CONSTANT:
				print_constant(m.constant);
				break;
			case ClassNode::Member::SIGNAL:
				print_signal(m.signal);
				break;
			case ClassNode::Member::FUNCTION:
				print_function(m.function);
				break;
			case ClassNode::Member::ENUM:
				print_enum(m.m_enum);
				break;
			case ClassNode::Member::ENUM_VALUE:
				break; // Nothing. Will be printed by enum.
			case ClassNode::Member::GROUP:
				break; // Nothing. Groups are only used by inspector.
			case ClassNode::Member::UNDEFINED:
				push_line("<unknown member>");
				break;
		}
	}

	decrease_indent();
}

void GDScriptParser::TreePrinter::print_constant(ConstantNode *p_constant) {
	push_text("Constant ");
	print_identifier(p_constant->identifier);

	increase_indent();

	push_line();
	push_text("= ");
	if (p_constant->initializer == nullptr) {
		push_text("<missing value>");
	} else {
		print_expression(p_constant->initializer);
	}
	decrease_indent();
	push_line();
}

void GDScriptParser::TreePrinter::print_dictionary(DictionaryNode *p_dictionary) {
	push_line("{");
	increase_indent();
	for (int i = 0; i < p_dictionary->elements.size(); i++) {
		print_expression(p_dictionary->elements[i].key);
		if (p_dictionary->style == DictionaryNode::PYTHON_DICT) {
			push_text(" : ");
		} else {
			push_text(" = ");
		}
		print_expression(p_dictionary->elements[i].value);
		push_line(" ,");
	}
	decrease_indent();
	push_text("}");
}

void GDScriptParser::TreePrinter::print_expression(ExpressionNode *p_expression) {
	if (p_expression == nullptr) {
		push_text("<invalid expression>");
		return;
	}
	switch (p_expression->type) {
		case Node::ARRAY:
			print_array(static_cast<ArrayNode *>(p_expression));
			break;
		case Node::ASSIGNMENT:
			print_assignment(static_cast<AssignmentNode *>(p_expression));
			break;
		case Node::AWAIT:
			print_await(static_cast<AwaitNode *>(p_expression));
			break;
		case Node::BINARY_OPERATOR:
			print_binary_op(static_cast<BinaryOpNode *>(p_expression));
			break;
		case Node::CALL:
			print_call(static_cast<CallNode *>(p_expression));
			break;
		case Node::CAST:
			print_cast(static_cast<CastNode *>(p_expression));
			break;
		case Node::DICTIONARY:
			print_dictionary(static_cast<DictionaryNode *>(p_expression));
			break;
		case Node::GET_NODE:
			print_get_node(static_cast<GetNodeNode *>(p_expression));
			break;
		case Node::IDENTIFIER:
			print_identifier(static_cast<IdentifierNode *>(p_expression));
			break;
		case Node::LAMBDA:
			print_lambda(static_cast<LambdaNode *>(p_expression));
			break;
		case Node::LITERAL:
			print_literal(static_cast<LiteralNode *>(p_expression));
			break;
		case Node::PRELOAD:
			print_preload(static_cast<PreloadNode *>(p_expression));
			break;
		case Node::SELF:
			print_self(static_cast<SelfNode *>(p_expression));
			break;
		case Node::SUBSCRIPT:
			print_subscript(static_cast<SubscriptNode *>(p_expression));
			break;
		case Node::TERNARY_OPERATOR:
			print_ternary_op(static_cast<TernaryOpNode *>(p_expression));
			break;
		case Node::TYPE_TEST:
			print_type_test(static_cast<TypeTestNode *>(p_expression));
			break;
		case Node::UNARY_OPERATOR:
			print_unary_op(static_cast<UnaryOpNode *>(p_expression));
			break;
		default:
			push_text(vformat("<unknown expression %d>", p_expression->type));
			break;
	}
}

void GDScriptParser::TreePrinter::print_enum(EnumNode *p_enum) {
	push_text("Enum ");
	if (p_enum->identifier != nullptr) {
		print_identifier(p_enum->identifier);
	} else {
		push_text("<unnamed>");
	}

	push_line(" {");
	increase_indent();
	for (int i = 0; i < p_enum->values.size(); i++) {
		const EnumNode::Value &item = p_enum->values[i];
		print_identifier(item.identifier);
		push_text(" = ");
		push_text(itos(item.value));
		push_line(" ,");
	}
	decrease_indent();
	push_line("}");
}

void GDScriptParser::TreePrinter::print_for(ForNode *p_for) {
	push_text("For ");
	print_identifier(p_for->variable);
	push_text(" IN ");
	print_expression(p_for->list);
	push_line(" :");

	increase_indent();

	print_suite(p_for->loop);

	decrease_indent();
}

void GDScriptParser::TreePrinter::print_function(FunctionNode *p_function, const String &p_context) {
	for (const AnnotationNode *E : p_function->annotations) {
		print_annotation(E);
	}
	if (p_function->is_static) {
		push_text("Static ");
	}
	push_text(p_context);
	push_text(" ");
	if (p_function->identifier) {
		print_identifier(p_function->identifier);
	} else {
		push_text("<anonymous>");
	}
	push_text("( ");
	for (int i = 0; i < p_function->parameters.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_parameter(p_function->parameters[i]);
	}
	push_line(" ) :");
	increase_indent();
	print_suite(p_function->body);
	decrease_indent();
}

void GDScriptParser::TreePrinter::print_get_node(GetNodeNode *p_get_node) {
	if (p_get_node->use_dollar) {
		push_text("$");
	}
	push_text(p_get_node->full_path);
}

void GDScriptParser::TreePrinter::print_identifier(IdentifierNode *p_identifier) {
	if (p_identifier != nullptr) {
		push_text(p_identifier->name);
	} else {
		push_text("<invalid identifier>");
	}
}

void GDScriptParser::TreePrinter::print_if(IfNode *p_if, bool p_is_elif) {
	if (p_is_elif) {
		push_text("Elif ");
	} else {
		push_text("If ");
	}
	print_expression(p_if->condition);
	push_line(" :");

	increase_indent();
	print_suite(p_if->true_block);
	decrease_indent();

	// FIXME: Properly detect "elif" blocks.
	if (p_if->false_block != nullptr) {
		push_line("Else :");
		increase_indent();
		print_suite(p_if->false_block);
		decrease_indent();
	}
}

void GDScriptParser::TreePrinter::print_lambda(LambdaNode *p_lambda) {
	print_function(p_lambda->function, "Lambda");
	push_text("| captures [ ");
	for (int i = 0; i < p_lambda->captures.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		push_text(p_lambda->captures[i]->name.operator String());
	}
	push_line(" ]");
}

void GDScriptParser::TreePrinter::print_literal(LiteralNode *p_literal) {
	// Prefix for string types.
	switch (p_literal->value.get_type()) {
		case Variant::NODE_PATH:
			push_text("^\"");
			break;
		case Variant::STRING:
			push_text("\"");
			break;
		case Variant::STRING_NAME:
			push_text("&\"");
			break;
		default:
			break;
	}
	push_text(p_literal->value);
	// Suffix for string types.
	switch (p_literal->value.get_type()) {
		case Variant::NODE_PATH:
		case Variant::STRING:
		case Variant::STRING_NAME:
			push_text("\"");
			break;
		default:
			break;
	}
}

void GDScriptParser::TreePrinter::print_match(MatchNode *p_match) {
	push_text("Match ");
	print_expression(p_match->test);
	push_line(" :");

	increase_indent();
	for (int i = 0; i < p_match->branches.size(); i++) {
		print_match_branch(p_match->branches[i]);
	}
	decrease_indent();
}

void GDScriptParser::TreePrinter::print_match_branch(MatchBranchNode *p_match_branch) {
	for (int i = 0; i < p_match_branch->patterns.size(); i++) {
		if (i > 0) {
			push_text(" , ");
		}
		print_match_pattern(p_match_branch->patterns[i]);
	}

	push_line(" :");

	increase_indent();
	print_suite(p_match_branch->block);
	decrease_indent();
}

void GDScriptParser::TreePrinter::print_match_pattern(PatternNode *p_match_pattern) {
	switch (p_match_pattern->pattern_type) {
		case PatternNode::PT_LITERAL:
			print_literal(p_match_pattern->literal);
			break;
		case PatternNode::PT_WILDCARD:
			push_text("_");
			break;
		case PatternNode::PT_REST:
			push_text("..");
			break;
		case PatternNode::PT_BIND:
			push_text("Var ");
			print_identifier(p_match_pattern->bind);
			break;
		case PatternNode::PT_EXPRESSION:
			print_expression(p_match_pattern->expression);
			break;
		case PatternNode::PT_ARRAY:
			push_text("[ ");
			for (int i = 0; i < p_match_pattern->array.size(); i++) {
				if (i > 0) {
					push_text(" , ");
				}
				print_match_pattern(p_match_pattern->array[i]);
			}
			push_text(" ]");
			break;
		case PatternNode::PT_DICTIONARY:
			push_text("{ ");
			for (int i = 0; i < p_match_pattern->dictionary.size(); i++) {
				if (i > 0) {
					push_text(" , ");
				}
				if (p_match_pattern->dictionary[i].key != nullptr) {
					// Key can be null for rest pattern.
					print_expression(p_match_pattern->dictionary[i].key);
					push_text(" : ");
				}
				print_match_pattern(p_match_pattern->dictionary[i].value_pattern);
			}
			push_text(" }");
			break;
	}
}

void GDScriptParser::TreePrinter::print_parameter(ParameterNode *p_parameter) {
	print_identifier(p_parameter->identifier);
	if (p_parameter->datatype_specifier != nullptr) {
		push_text(" : ");
		print_type(p_parameter->datatype_specifier);
	}
	if (p_parameter->initializer != nullptr) {
		push_text(" = ");
		print_expression(p_parameter->initializer);
	}
}

void GDScriptParser::TreePrinter::print_preload(PreloadNode *p_preload) {
	push_text(R"(Preload ( ")");
	push_text(p_preload->resolved_path);
	push_text(R"(" )");
}

void GDScriptParser::TreePrinter::print_return(ReturnNode *p_return) {
	push_text("Return");
	if (p_return->return_value != nullptr) {
		push_text(" ");
		print_expression(p_return->return_value);
	}
	push_line();
}

void GDScriptParser::TreePrinter::print_self(SelfNode *p_self) {
	push_text("Self(");
	if (p_self->current_class->identifier != nullptr) {
		print_identifier(p_self->current_class->identifier);
	} else {
		push_text("<main class>");
	}
	push_text(")");
}

void GDScriptParser::TreePrinter::print_signal(SignalNode *p_signal) {
	push_text("Signal ");
	print_identifier(p_signal->identifier);
	push_text("( ");
	for (int i = 0; i < p_signal->parameters.size(); i++) {
		print_parameter(p_signal->parameters[i]);
	}
	push_line(" )");
}

void GDScriptParser::TreePrinter::print_subscript(SubscriptNode *p_subscript) {
	print_expression(p_subscript->base);
	if (p_subscript->is_attribute) {
		push_text(".");
		print_identifier(p_subscript->attribute);
	} else {
		push_text("[ ");
		print_expression(p_subscript->index);
		push_text(" ]");
	}
}

void GDScriptParser::TreePrinter::print_statement(Node *p_statement) {
	switch (p_statement->type) {
		case Node::ASSERT:
			print_assert(static_cast<AssertNode *>(p_statement));
			break;
		case Node::VARIABLE:
			print_variable(static_cast<VariableNode *>(p_statement));
			break;
		case Node::CONSTANT:
			print_constant(static_cast<ConstantNode *>(p_statement));
			break;
		case Node::IF:
			print_if(static_cast<IfNode *>(p_statement));
			break;
		case Node::FOR:
			print_for(static_cast<ForNode *>(p_statement));
			break;
		case Node::WHILE:
			print_while(static_cast<WhileNode *>(p_statement));
			break;
		case Node::MATCH:
			print_match(static_cast<MatchNode *>(p_statement));
			break;
		case Node::RETURN:
			print_return(static_cast<ReturnNode *>(p_statement));
			break;
		case Node::BREAK:
			push_line("Break");
			break;
		case Node::CONTINUE:
			push_line("Continue");
			break;
		case Node::PASS:
			push_line("Pass");
			break;
		case Node::BREAKPOINT:
			push_line("Breakpoint");
			break;
		case Node::ASSIGNMENT:
			print_assignment(static_cast<AssignmentNode *>(p_statement));
			break;
		default:
			if (p_statement->is_expression()) {
				print_expression(static_cast<ExpressionNode *>(p_statement));
				push_line();
			} else {
				push_line(vformat("<unknown statement %d>", p_statement->type));
			}
			break;
	}
}

void GDScriptParser::TreePrinter::print_suite(SuiteNode *p_suite) {
	for (int i = 0; i < p_suite->statements.size(); i++) {
		print_statement(p_suite->statements[i]);
	}
}

void GDScriptParser::TreePrinter::print_ternary_op(TernaryOpNode *p_ternary_op) {
	// Surround in parenthesis for disambiguation.
	push_text("(");
	print_expression(p_ternary_op->true_expr);
	push_text(") IF (");
	print_expression(p_ternary_op->condition);
	push_text(") ELSE (");
	print_expression(p_ternary_op->false_expr);
	push_text(")");
}

void GDScriptParser::TreePrinter::print_type(TypeNode *p_type) {
	if (p_type->type_chain.is_empty()) {
		push_text("Void");
	} else {
		for (int i = 0; i < p_type->type_chain.size(); i++) {
			if (i > 0) {
				push_text(".");
			}
			print_identifier(p_type->type_chain[i]);
		}
	}
}

void GDScriptParser::TreePrinter::print_type_test(TypeTestNode *p_test) {
	print_expression(p_test->operand);
	push_text(" IS ");
	print_type(p_test->test_type);
}

void GDScriptParser::TreePrinter::print_unary_op(UnaryOpNode *p_unary_op) {
	// Surround in parenthesis for disambiguation.
	push_text("(");
	switch (p_unary_op->operation) {
		case UnaryOpNode::OP_POSITIVE:
			push_text("+");
			break;
		case UnaryOpNode::OP_NEGATIVE:
			push_text("-");
			break;
		case UnaryOpNode::OP_LOGIC_NOT:
			push_text("NOT");
			break;
		case UnaryOpNode::OP_COMPLEMENT:
			push_text("~");
			break;
	}
	print_expression(p_unary_op->operand);
	// Surround in parenthesis for disambiguation.
	push_text(")");
}

void GDScriptParser::TreePrinter::print_variable(VariableNode *p_variable) {
	for (const AnnotationNode *E : p_variable->annotations) {
		print_annotation(E);
	}

	if (p_variable->is_static) {
		push_text("Static ");
	}
	push_text("Variable ");
	print_identifier(p_variable->identifier);

	push_text(" : ");
	if (p_variable->datatype_specifier != nullptr) {
		print_type(p_variable->datatype_specifier);
	} else if (p_variable->infer_datatype) {
		push_text("<inferred type>");
	} else {
		push_text("Variant");
	}

	increase_indent();

	push_line();
	push_text("= ");
	if (p_variable->initializer == nullptr) {
		push_text("<default value>");
	} else {
		print_expression(p_variable->initializer);
	}
	push_line();

	if (p_variable->property != VariableNode::PROP_NONE) {
		if (p_variable->getter != nullptr) {
			push_text("Get");
			if (p_variable->property == VariableNode::PROP_INLINE) {
				push_line(":");
				increase_indent();
				print_suite(p_variable->getter->body);
				decrease_indent();
			} else {
				push_line(" =");
				increase_indent();
				print_identifier(p_variable->getter_pointer);
				push_line();
				decrease_indent();
			}
		}
		if (p_variable->setter != nullptr) {
			push_text("Set (");
			if (p_variable->property == VariableNode::PROP_INLINE) {
				if (p_variable->setter_parameter != nullptr) {
					print_identifier(p_variable->setter_parameter);
				} else {
					push_text("<missing>");
				}
				push_line("):");
				increase_indent();
				print_suite(p_variable->setter->body);
				decrease_indent();
			} else {
				push_line(" =");
				increase_indent();
				print_identifier(p_variable->setter_pointer);
				push_line();
				decrease_indent();
			}
		}
	}

	decrease_indent();
	push_line();
}

void GDScriptParser::TreePrinter::print_while(WhileNode *p_while) {
	push_text("While ");
	print_expression(p_while->condition);
	push_line(" :");

	increase_indent();
	print_suite(p_while->loop);
	decrease_indent();
}

void GDScriptParser::TreePrinter::print_tree(const GDScriptParser &p_parser) {
	ERR_FAIL_NULL_MSG(p_parser.get_tree(), "Parse the code before printing the parse tree.");

	if (p_parser.is_tool()) {
		push_line("@tool");
	}
	if (!p_parser.get_tree()->icon_path.is_empty()) {
		push_text(R"(@icon (")");
		push_text(p_parser.get_tree()->icon_path);
		push_line("\")");
	}
	print_class(p_parser.get_tree());

	print_line(String(printed));
}

#endif // DEBUG_ENABLED
