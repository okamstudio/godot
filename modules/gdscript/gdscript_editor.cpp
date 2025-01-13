/**************************************************************************/
/*  gdscript_editor.cpp                                                   */
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

#include "gdscript.h"

#include "gdscript_analyzer.h"
#include "gdscript_compiler.h"
#include "gdscript_parser.h"
#include "gdscript_tokenizer.h"
#include "gdscript_utility_functions.h"

#ifdef TOOLS_ENABLED
#include "editor/script_templates/templates.gen.h"
#endif

#include "core/config/engine.h"
#include "core/core_constants.h"
#include "core/io/file_access.h"
#include "core/math/expression.h"

#ifdef TOOLS_ENABLED
#include "core/config/project_settings.h"
#include "editor/editor_file_system.h"
#include "editor/editor_settings.h"
#endif

void GDScriptLanguage::get_comment_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("#");
}

void GDScriptLanguage::get_doc_comment_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("##");
}

void GDScriptLanguage::get_string_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("\" \"");
	p_delimiters->push_back("' '");
	p_delimiters->push_back("\"\"\" \"\"\"");
	p_delimiters->push_back("''' '''");
	// NOTE: StringName, NodePath and r-strings are not listed here.
}

bool GDScriptLanguage::is_using_templates() {
	return true;
}

Ref<Script> GDScriptLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	Ref<GDScript> scr;
	scr.instantiate();
	String processed_template = p_template;
	bool type_hints = false;
#ifdef TOOLS_ENABLED
	type_hints = EDITOR_GET("text_editor/completion/add_type_hints");
#endif
	if (!type_hints) {
		processed_template = processed_template.replace(": int", "")
									 .replace(": Shader.Mode", "")
									 .replace(": VisualShader.Type", "")
									 .replace(": float", "")
									 .replace(": String", "")
									 .replace(": Array[String]", "")
									 .replace(": Node", "")
									 .replace(": CharFXTransform", "")
									 .replace(":=", "=")
									 .replace(" -> void", "")
									 .replace(" -> bool", "")
									 .replace(" -> int", "")
									 .replace(" -> PortType", "")
									 .replace(" -> String", "")
									 .replace(" -> Object", "");
	}

	processed_template = processed_template.replace("_BASE_", p_base_class_name)
								 .replace("_CLASS_SNAKE_CASE_", p_class_name.to_snake_case().validate_unicode_identifier())
								 .replace("_CLASS_", p_class_name.to_pascal_case().validate_unicode_identifier())
								 .replace("_TS_", _get_indentation());
	scr->set_source_code(processed_template);
	return scr;
}

Vector<ScriptLanguage::ScriptTemplate> GDScriptLanguage::get_built_in_templates(const StringName &p_object) {
	Vector<ScriptLanguage::ScriptTemplate> templates;
#ifdef TOOLS_ENABLED
	for (int i = 0; i < TEMPLATES_ARRAY_SIZE; i++) {
		if (TEMPLATES[i].inherit == p_object) {
			templates.append(TEMPLATES[i]);
		}
	}
#endif
	return templates;
}

static void get_function_names_recursively(const GDScriptParser::ClassNode *p_class, const String &p_prefix, HashMap<int, String> &r_funcs) {
	for (int i = 0; i < p_class->members.size(); i++) {
		if (p_class->members[i].type == GDScriptParser::ClassNode::Member::FUNCTION) {
			const GDScriptParser::FunctionNode *function = p_class->members[i].function;
			r_funcs[function->start_line] = p_prefix.is_empty() ? String(function->identifier->name) : p_prefix + "." + String(function->identifier->name);
		} else if (p_class->members[i].type == GDScriptParser::ClassNode::Member::CLASS) {
			String new_prefix = p_class->members[i].m_class->identifier->name;
			get_function_names_recursively(p_class->members[i].m_class, p_prefix.is_empty() ? new_prefix : p_prefix + "." + new_prefix, r_funcs);
		}
	}
}

bool GDScriptLanguage::validate(const String &p_script, const String &p_path, List<String> *r_functions, List<ScriptLanguage::ScriptError> *r_errors, List<ScriptLanguage::Warning> *r_warnings, HashSet<int> *r_safe_lines) const {
	GDScriptParser parser;
	GDScriptAnalyzer analyzer(&parser);

	Error err = parser.parse(p_script, p_path, false);
	if (err == OK) {
		err = analyzer.analyze();
	}
#ifdef DEBUG_ENABLED
	if (r_warnings) {
		for (const GDScriptWarning &E : parser.get_warnings()) {
			const GDScriptWarning &warn = E;
			ScriptLanguage::Warning w;
			w.start_line = warn.start_line;
			w.end_line = warn.end_line;
			w.leftmost_column = warn.leftmost_column;
			w.rightmost_column = warn.rightmost_column;
			w.code = (int)warn.code;
			w.string_code = GDScriptWarning::get_name_from_code(warn.code);
			w.message = warn.get_message();
			r_warnings->push_back(w);
		}
	}
#endif
	if (err) {
		if (r_errors) {
			for (const GDScriptParser::ParserError &pe : parser.get_errors()) {
				ScriptLanguage::ScriptError e;
				e.path = p_path;
				e.line = pe.line;
				e.column = pe.column;
				e.message = pe.message;
				r_errors->push_back(e);
			}

			for (KeyValue<String, Ref<GDScriptParserRef>> E : parser.get_depended_parsers()) {
				GDScriptParser *depended_parser = E.value->get_parser();
				for (const GDScriptParser::ParserError &pe : depended_parser->get_errors()) {
					ScriptLanguage::ScriptError e;
					e.path = E.key;
					e.line = pe.line;
					e.column = pe.column;
					e.message = pe.message;
					r_errors->push_back(e);
				}
			}
		}
		return false;
	} else {
		const GDScriptParser::ClassNode *cl = parser.get_tree();
		HashMap<int, String> funcs;

		get_function_names_recursively(cl, "", funcs);

		for (const KeyValue<int, String> &E : funcs) {
			r_functions->push_back(E.value + ":" + itos(E.key));
		}
	}

#ifdef DEBUG_ENABLED
	if (r_safe_lines) {
		const HashSet<int> &unsafe_lines = parser.get_unsafe_lines();
		for (int i = 1; i <= parser.get_last_line_number(); i++) {
			if (!unsafe_lines.has(i)) {
				r_safe_lines->insert(i);
			}
		}
	}
#endif

	return true;
}

bool GDScriptLanguage::supports_builtin_mode() const {
	return true;
}

bool GDScriptLanguage::supports_documentation() const {
	return true;
}

int GDScriptLanguage::find_function(const String &p_function, const String &p_code) const {
	GDScriptTokenizerText tokenizer;
	tokenizer.set_source_code(p_code);
	int indent = 0;
	GDScriptTokenizer::Token current = tokenizer.scan();
	while (current.type != GDScriptTokenizer::Token::TK_EOF && current.type != GDScriptTokenizer::Token::ERROR) {
		if (current.type == GDScriptTokenizer::Token::INDENT) {
			indent++;
		} else if (current.type == GDScriptTokenizer::Token::DEDENT) {
			indent--;
		}
		if (indent == 0 && current.type == GDScriptTokenizer::Token::FUNC) {
			current = tokenizer.scan();
			if (current.is_identifier()) {
				String identifier = current.get_identifier();
				if (identifier == p_function) {
					return current.start_line;
				}
			}
		}
		current = tokenizer.scan();
	}
	return -1;
}

Script *GDScriptLanguage::create_script() const {
	return memnew(GDScript);
}

/* DEBUGGER FUNCTIONS */

thread_local int GDScriptLanguage::_debug_parse_err_line = -1;
thread_local String GDScriptLanguage::_debug_parse_err_file;
thread_local String GDScriptLanguage::_debug_error;

bool GDScriptLanguage::debug_break_parse(const String &p_file, int p_line, const String &p_error) {
	// break because of parse error

	if (EngineDebugger::is_active() && Thread::get_caller_id() == Thread::get_main_id()) {
		_debug_parse_err_line = p_line;
		_debug_parse_err_file = p_file;
		_debug_error = p_error;
		EngineDebugger::get_script_debugger()->debug(this, false, true);
		// Because this is thread local, clear the memory afterwards.
		_debug_parse_err_file = String();
		_debug_error = String();
		return true;
	} else {
		return false;
	}
}

bool GDScriptLanguage::debug_break(const String &p_error, bool p_allow_continue) {
	if (EngineDebugger::is_active()) {
		_debug_parse_err_line = -1;
		_debug_parse_err_file = "";
		_debug_error = p_error;
		bool is_error_breakpoint = p_error != "Breakpoint";
		EngineDebugger::get_script_debugger()->debug(this, p_allow_continue, is_error_breakpoint);
		// Because this is thread local, clear the memory afterwards.
		_debug_parse_err_file = String();
		_debug_error = String();
		return true;
	} else {
		return false;
	}
}

String GDScriptLanguage::debug_get_error() const {
	return _debug_error;
}

int GDScriptLanguage::debug_get_stack_level_count() const {
	if (_debug_parse_err_line >= 0) {
		return 1;
	}

	return _call_stack.stack_pos;
}

int GDScriptLanguage::debug_get_stack_level_line(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return _debug_parse_err_line;
	}

	ERR_FAIL_INDEX_V(p_level, _call_stack.stack_pos, -1);

	int l = _call_stack.stack_pos - p_level - 1;

	return *(_call_stack.levels[l].line);
}

String GDScriptLanguage::debug_get_stack_level_function(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return "";
	}

	ERR_FAIL_INDEX_V(p_level, _call_stack.stack_pos, "");
	int l = _call_stack.stack_pos - p_level - 1;
	return _call_stack.levels[l].function->get_name();
}

String GDScriptLanguage::debug_get_stack_level_source(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return _debug_parse_err_file;
	}

	ERR_FAIL_INDEX_V(p_level, _call_stack.stack_pos, "");
	int l = _call_stack.stack_pos - p_level - 1;
	return _call_stack.levels[l].function->get_source();
}

void GDScriptLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	if (_debug_parse_err_line >= 0) {
		return;
	}

	ERR_FAIL_INDEX(p_level, _call_stack.stack_pos);
	int l = _call_stack.stack_pos - p_level - 1;

	GDScriptFunction *f = _call_stack.levels[l].function;

	List<Pair<StringName, int>> locals;

	f->debug_get_stack_member_state(*_call_stack.levels[l].line, &locals);
	for (const Pair<StringName, int> &E : locals) {
		p_locals->push_back(E.first);
		p_values->push_back(_call_stack.levels[l].stack[E.second]);
	}
}

void GDScriptLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	if (_debug_parse_err_line >= 0) {
		return;
	}

	ERR_FAIL_INDEX(p_level, _call_stack.stack_pos);
	int l = _call_stack.stack_pos - p_level - 1;

	GDScriptInstance *instance = _call_stack.levels[l].instance;

	if (!instance) {
		return;
	}

	Ref<GDScript> scr = instance->get_script();
	ERR_FAIL_COND(scr.is_null());

	const HashMap<StringName, GDScript::MemberInfo> &mi = scr->debug_get_member_indices();

	for (const KeyValue<StringName, GDScript::MemberInfo> &E : mi) {
		p_members->push_back(E.key);
		p_values->push_back(instance->debug_get_member_by_index(E.value.index));
	}
}

ScriptInstance *GDScriptLanguage::debug_get_stack_level_instance(int p_level) {
	if (_debug_parse_err_line >= 0) {
		return nullptr;
	}

	ERR_FAIL_INDEX_V(p_level, _call_stack.stack_pos, nullptr);

	int l = _call_stack.stack_pos - p_level - 1;
	ScriptInstance *instance = _call_stack.levels[l].instance;

	return instance;
}

void GDScriptLanguage::debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	const HashMap<StringName, int> &name_idx = GDScriptLanguage::get_singleton()->get_global_map();
	const Variant *gl_array = GDScriptLanguage::get_singleton()->get_global_array();

	List<Pair<String, Variant>> cinfo;
	get_public_constants(&cinfo);

	for (const KeyValue<StringName, int> &E : name_idx) {
		if (ClassDB::class_exists(E.key) || Engine::get_singleton()->has_singleton(E.key)) {
			continue;
		}

		bool is_script_constant = false;
		for (List<Pair<String, Variant>>::Element *CE = cinfo.front(); CE; CE = CE->next()) {
			if (CE->get().first == E.key) {
				is_script_constant = true;
				break;
			}
		}
		if (is_script_constant) {
			continue;
		}

		const Variant &var = gl_array[E.value];
		bool freed = false;
		const Object *obj = var.get_validated_object_with_check(freed);
		if (obj && !freed) {
			if (Object::cast_to<GDScriptNativeClass>(obj)) {
				continue;
			}
		}

		bool skip = false;
		for (int i = 0; i < CoreConstants::get_global_constant_count(); i++) {
			if (E.key == CoreConstants::get_global_constant_name(i)) {
				skip = true;
				break;
			}
		}
		if (skip) {
			continue;
		}

		p_globals->push_back(E.key);
		p_values->push_back(var);
	}
}

String GDScriptLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) {
	List<String> names;
	List<Variant> values;
	debug_get_stack_level_locals(p_level, &names, &values, p_max_subitems, p_max_depth);

	Vector<String> name_vector;
	for (const String &name : names) {
		name_vector.push_back(name);
	}

	Array value_array;
	for (const Variant &value : values) {
		value_array.push_back(value);
	}

	Expression expression;
	if (expression.parse(p_expression, name_vector) == OK) {
		ScriptInstance *instance = debug_get_stack_level_instance(p_level);
		if (instance) {
			Variant return_val = expression.execute(value_array, instance->get_owner());
			return return_val.get_construct_string();
		}
	}

	return String();
}

void GDScriptLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gd");
}

void GDScriptLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
	List<StringName> functions;
	GDScriptUtilityFunctions::get_function_list(&functions);

	for (const StringName &E : functions) {
		p_functions->push_back(GDScriptUtilityFunctions::get_function_info(E));
	}

	// Not really "functions", but show in documentation.
	{
		MethodInfo mi;
		mi.name = "preload";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "path"));
		mi.return_val = PropertyInfo(Variant::OBJECT, "", PROPERTY_HINT_RESOURCE_TYPE, "Resource");
		p_functions->push_back(mi);
	}
	{
		MethodInfo mi;
		mi.name = "assert";
		mi.return_val.type = Variant::NIL;
		mi.arguments.push_back(PropertyInfo(Variant::BOOL, "condition"));
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "message"));
		mi.default_arguments.push_back(String());
		p_functions->push_back(mi);
	}
}

void GDScriptLanguage::get_public_constants(List<Pair<String, Variant>> *p_constants) const {
	Pair<String, Variant> pi;
	pi.first = "PI";
	pi.second = Math_PI;
	p_constants->push_back(pi);

	Pair<String, Variant> tau;
	tau.first = "TAU";
	tau.second = Math_TAU;
	p_constants->push_back(tau);

	Pair<String, Variant> infinity;
	infinity.first = "INF";
	infinity.second = INFINITY;
	p_constants->push_back(infinity);

	Pair<String, Variant> nan;
	nan.first = "NAN";
	nan.second = NAN;
	p_constants->push_back(nan);
}

void GDScriptLanguage::get_public_annotations(List<MethodInfo> *p_annotations) const {
	GDScriptParser parser;
	List<MethodInfo> annotations;
	parser.get_annotation_list(&annotations);

	for (const MethodInfo &E : annotations) {
		p_annotations->push_back(E);
	}
}

String GDScriptLanguage::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args) const {
#ifdef TOOLS_ENABLED
	bool th = EditorSettings::get_singleton()->get_setting("text_editor/completion/add_type_hints");
#else
	bool th = false;
#endif

	String s = "func " + p_name + "(";
	if (p_args.size()) {
		for (int i = 0; i < p_args.size(); i++) {
			if (i > 0) {
				s += ", ";
			}
			s += p_args[i].get_slice(":", 0);
			if (th) {
				String type = p_args[i].get_slice(":", 1);
				if (!type.is_empty()) {
					s += ": " + type;
				}
			}
		}
	}
	s += String(")") + (th ? " -> void" : "") + ":\n" + _get_indentation() + "pass # Replace with function body.\n";

	return s;
}

//////// COMPLETION //////////

#ifdef TOOLS_ENABLED

#define COMPLETION_RECURSION_LIMIT 200

struct GDScriptCompletionIdentifier {
	GDScriptParser::DataType type;
	String enumeration;
	Variant value;
	const GDScriptParser::ExpressionNode *assigned_expression = nullptr;
};

// LOCATION METHODS
// These methods are used to populate the `CodeCompletionOption::location` integer.
// For these methods, the location is based on the depth in the inheritance chain that the property
// appears. For example, if you are completing code in a class that inherits Node2D, a property found on Node2D
// will have a "better" (lower) location "score" than a property that is found on CanvasItem.

static int _get_property_location(const StringName &p_class, const StringName &p_property) {
	if (!ClassDB::has_property(p_class, p_property)) {
		return ScriptLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (class_test && !ClassDB::has_property(class_test, p_property, true)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | ScriptLanguage::LOCATION_PARENT_MASK;
}

static int _get_property_location(Ref<Script> p_script, const StringName &p_property) {
	int depth = 0;
	Ref<Script> scr = p_script;
	while (scr.is_valid()) {
		if (scr->get_member_line(p_property) != -1) {
			return depth | ScriptLanguage::LOCATION_PARENT_MASK;
		}
		depth++;
		scr = scr->get_base_script();
	}
	return depth + _get_property_location(p_script->get_instance_base_type(), p_property);
}

static int _get_constant_location(const StringName &p_class, const StringName &p_constant) {
	if (!ClassDB::has_integer_constant(p_class, p_constant)) {
		return ScriptLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (class_test && !ClassDB::has_integer_constant(class_test, p_constant, true)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | ScriptLanguage::LOCATION_PARENT_MASK;
}

static int _get_constant_location(Ref<Script> p_script, const StringName &p_constant) {
	int depth = 0;
	Ref<Script> scr = p_script;
	while (scr.is_valid()) {
		if (scr->get_member_line(p_constant) != -1) {
			return depth | ScriptLanguage::LOCATION_PARENT_MASK;
		}
		depth++;
		scr = scr->get_base_script();
	}
	return depth + _get_constant_location(p_script->get_instance_base_type(), p_constant);
}

static int _get_signal_location(const StringName &p_class, const StringName &p_signal) {
	if (!ClassDB::has_signal(p_class, p_signal)) {
		return ScriptLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (class_test && !ClassDB::has_signal(class_test, p_signal, true)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | ScriptLanguage::LOCATION_PARENT_MASK;
}

static int _get_signal_location(Ref<Script> p_script, const StringName &p_signal) {
	int depth = 0;
	Ref<Script> scr = p_script;
	while (scr.is_valid()) {
		if (scr->get_member_line(p_signal) != -1) {
			return depth | ScriptLanguage::LOCATION_PARENT_MASK;
		}
		depth++;
		scr = scr->get_base_script();
	}
	return depth + _get_signal_location(p_script->get_instance_base_type(), p_signal);
}

static int _get_method_location(const StringName &p_class, const StringName &p_method) {
	if (!ClassDB::has_method(p_class, p_method)) {
		return ScriptLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (class_test && !ClassDB::has_method(class_test, p_method, true)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | ScriptLanguage::LOCATION_PARENT_MASK;
}

static int _get_enum_constant_location(const StringName &p_class, const StringName &p_enum_constant) {
	if (!ClassDB::get_integer_constant_enum(p_class, p_enum_constant)) {
		return ScriptLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (class_test && !ClassDB::get_integer_constant_enum(class_test, p_enum_constant, true)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | ScriptLanguage::LOCATION_PARENT_MASK;
}

static int _get_enum_location(const StringName &p_class, const StringName &p_enum) {
	if (!ClassDB::has_enum(p_class, p_enum)) {
		return ScriptLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (class_test && !ClassDB::has_enum(class_test, p_enum, true)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | ScriptLanguage::LOCATION_PARENT_MASK;
}

// END LOCATION METHODS

static String _trim_parent_class(const String &p_class, const String &p_base_class) {
	if (p_base_class.is_empty()) {
		return p_class;
	}
	Vector<String> names = p_class.split(".", false, 1);
	if (names.size() == 2) {
		const String &first = names[0];
		if (ClassDB::class_exists(p_base_class) && ClassDB::class_exists(first) && ClassDB::is_parent_class(p_base_class, first)) {
			const String &rest = names[1];
			return rest;
		}
	}
	return p_class;
}

static String _get_visual_datatype(const PropertyInfo &p_info, bool p_is_arg, const String &p_base_class = "") {
	String class_name = p_info.class_name;
	bool is_enum = p_info.type == Variant::INT && p_info.usage & PROPERTY_USAGE_CLASS_IS_ENUM;
	// PROPERTY_USAGE_CLASS_IS_BITFIELD: BitField[T] isn't supported (yet?), use plain int.

	if ((p_info.type == Variant::OBJECT || is_enum) && !class_name.is_empty()) {
		if (is_enum && CoreConstants::is_global_enum(p_info.class_name)) {
			return class_name;
		}
		return _trim_parent_class(class_name, p_base_class);
	} else if (p_info.type == Variant::ARRAY && p_info.hint == PROPERTY_HINT_ARRAY_TYPE && !p_info.hint_string.is_empty()) {
		return "Array[" + _trim_parent_class(p_info.hint_string, p_base_class) + "]";
	} else if (p_info.type == Variant::DICTIONARY && p_info.hint == PROPERTY_HINT_DICTIONARY_TYPE && !p_info.hint_string.is_empty()) {
		const String key = p_info.hint_string.get_slice(";", 0);
		const String value = p_info.hint_string.get_slice(";", 1);
		return "Dictionary[" + _trim_parent_class(key, p_base_class) + ", " + _trim_parent_class(value, p_base_class) + "]";
	} else if (p_info.type == Variant::NIL) {
		if (p_is_arg || (p_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT)) {
			return "Variant";
		} else {
			return "void";
		}
	}

	return Variant::get_type_name(p_info.type);
}

static String _make_arguments_hint(const MethodInfo &p_info, int p_arg_idx, bool p_is_annotation = false) {
	String arghint;
	if (!p_is_annotation) {
		arghint += _get_visual_datatype(p_info.return_val, false) + " ";
	}
	arghint += p_info.name + "(";

	int def_args = p_info.arguments.size() - p_info.default_arguments.size();
	int i = 0;
	for (const PropertyInfo &E : p_info.arguments) {
		if (i > 0) {
			arghint += ", ";
		}

		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}
		arghint += E.name + ": " + _get_visual_datatype(E, true);

		if (i - def_args >= 0) {
			arghint += String(" = ") + p_info.default_arguments[i - def_args].get_construct_string();
		}

		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}

		i++;
	}

	if (p_info.flags & METHOD_FLAG_VARARG) {
		if (p_info.arguments.size() > 0) {
			arghint += ", ";
		}
		if (p_arg_idx >= p_info.arguments.size()) {
			arghint += String::chr(0xFFFF);
		}
		arghint += "...";
		if (p_arg_idx >= p_info.arguments.size()) {
			arghint += String::chr(0xFFFF);
		}
	}

	arghint += ")";

	return arghint;
}

static String _make_arguments_hint(const GDScriptParser::FunctionNode *p_function, int p_arg_idx, bool p_just_args = false) {
	String arghint;

	if (p_just_args) {
		arghint = "(";
	} else {
		if (p_function->get_datatype().builtin_type == Variant::NIL) {
			arghint = "void " + p_function->identifier->name.operator String() + "(";
		} else {
			arghint = p_function->get_datatype().to_string() + " " + p_function->identifier->name.operator String() + "(";
		}
	}

	for (int i = 0; i < p_function->parameters.size(); i++) {
		if (i > 0) {
			arghint += ", ";
		}

		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}
		const GDScriptParser::ParameterNode *par = p_function->parameters[i];
		if (!par->get_datatype().is_hard_type()) {
			arghint += par->identifier->name.operator String() + ": Variant";
		} else {
			arghint += par->identifier->name.operator String() + ": " + par->get_datatype().to_string();
		}

		if (par->initializer) {
			String def_val = "<unknown>";
			switch (par->initializer->type) {
				case GDScriptParser::Node::LITERAL: {
					const GDScriptParser::LiteralNode *literal = static_cast<const GDScriptParser::LiteralNode *>(par->initializer);
					def_val = literal->value.get_construct_string();
				} break;
				case GDScriptParser::Node::IDENTIFIER: {
					const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(par->initializer);
					def_val = id->name.operator String();
				} break;
				case GDScriptParser::Node::CALL: {
					const GDScriptParser::CallNode *call = static_cast<const GDScriptParser::CallNode *>(par->initializer);
					if (call->is_constant && call->reduced) {
						def_val = call->reduced_value.get_construct_string();
					} else {
						def_val = call->function_name.operator String() + (call->arguments.is_empty() ? "()" : "(...)");
					}
				} break;
				case GDScriptParser::Node::ARRAY: {
					const GDScriptParser::ArrayNode *arr = static_cast<const GDScriptParser::ArrayNode *>(par->initializer);
					if (arr->is_constant && arr->reduced) {
						def_val = arr->reduced_value.get_construct_string();
					} else {
						def_val = arr->elements.is_empty() ? "[]" : "[...]";
					}
				} break;
				case GDScriptParser::Node::DICTIONARY: {
					const GDScriptParser::DictionaryNode *dict = static_cast<const GDScriptParser::DictionaryNode *>(par->initializer);
					if (dict->is_constant && dict->reduced) {
						def_val = dict->reduced_value.get_construct_string();
					} else {
						def_val = dict->elements.is_empty() ? "{}" : "{...}";
					}
				} break;
				case GDScriptParser::Node::SUBSCRIPT: {
					const GDScriptParser::SubscriptNode *sub = static_cast<const GDScriptParser::SubscriptNode *>(par->initializer);
					if (sub->is_attribute && sub->datatype.kind == GDScriptParser::DataType::ENUM && !sub->datatype.is_meta_type) {
						def_val = sub->get_datatype().to_string() + "." + sub->attribute->name;
					} else if (sub->is_constant && sub->reduced) {
						def_val = sub->reduced_value.get_construct_string();
					}
				} break;
				default:
					break;
			}
			arghint += " = " + def_val;
		}
		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}
	}

	arghint += ")";

	return arghint;
}

static void _get_directory_contents(EditorFileSystemDirectory *p_dir, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_list, const StringName &p_required_type = StringName()) {
	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";
	const bool requires_type = p_required_type;

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		if (requires_type && !ClassDB::is_parent_class(p_dir->get_file_type(i), p_required_type)) {
			continue;
		}
		ScriptLanguage::CodeCompletionOption option(p_dir->get_file_path(i), ScriptLanguage::CODE_COMPLETION_KIND_FILE_PATH);
		option.insert_text = option.display.quote(quote_style);
		r_list.insert(option.display, option);
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_get_directory_contents(p_dir->get_subdir(i), r_list, p_required_type);
	}
}

static void _find_annotation_arguments(const GDScriptParser::AnnotationNode *p_annotation, int p_argument, const String p_quote_style, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, String &r_arghint) {
	r_arghint = _make_arguments_hint(p_annotation->info->info, p_argument, true);
	if (p_annotation->name == SNAME("@export_range")) {
		if (p_argument == 3 || p_argument == 4 || p_argument == 5) {
			// Slider hint.
			ScriptLanguage::CodeCompletionOption slider1("or_greater", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			slider1.insert_text = slider1.display.quote(p_quote_style);
			r_result.insert(slider1.display, slider1);
			ScriptLanguage::CodeCompletionOption slider2("or_less", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			slider2.insert_text = slider2.display.quote(p_quote_style);
			r_result.insert(slider2.display, slider2);
			ScriptLanguage::CodeCompletionOption slider3("hide_slider", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			slider3.insert_text = slider3.display.quote(p_quote_style);
			r_result.insert(slider3.display, slider3);
		}
	} else if (p_annotation->name == SNAME("@export_exp_easing")) {
		if (p_argument == 0 || p_argument == 1) {
			// Easing hint.
			ScriptLanguage::CodeCompletionOption hint1("attenuation", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			hint1.insert_text = hint1.display.quote(p_quote_style);
			r_result.insert(hint1.display, hint1);
			ScriptLanguage::CodeCompletionOption hint2("inout", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			hint2.insert_text = hint2.display.quote(p_quote_style);
			r_result.insert(hint2.display, hint2);
		}
	} else if (p_annotation->name == SNAME("@export_node_path")) {
		ScriptLanguage::CodeCompletionOption node("Node", ScriptLanguage::CODE_COMPLETION_KIND_CLASS);
		node.insert_text = node.display.quote(p_quote_style);
		r_result.insert(node.display, node);

		List<StringName> native_classes;
		ClassDB::get_inheriters_from_class("Node", &native_classes);
		for (const StringName &E : native_classes) {
			if (!ClassDB::is_class_exposed(E)) {
				continue;
			}
			ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_CLASS);
			option.insert_text = option.display.quote(p_quote_style);
			r_result.insert(option.display, option);
		}

		List<StringName> global_script_classes;
		ScriptServer::get_global_class_list(&global_script_classes);
		for (const StringName &E : global_script_classes) {
			if (!ClassDB::is_parent_class(ScriptServer::get_global_class_native_base(E), "Node")) {
				continue;
			}
			ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_CLASS);
			option.insert_text = option.display.quote(p_quote_style);
			r_result.insert(option.display, option);
		}
	} else if (p_annotation->name == SNAME("@export_custom")) {
		switch (p_argument) {
			case 0: {
				static HashMap<StringName, int64_t> items;
				if (unlikely(items.is_empty())) {
					CoreConstants::get_enum_values(SNAME("PropertyHint"), &items);
				}
				for (const KeyValue<StringName, int64_t> &item : items) {
					ScriptLanguage::CodeCompletionOption option(item.key, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT);
					r_result.insert(option.display, option);
				}
			} break;
			case 2: {
				static HashMap<StringName, int64_t> items;
				if (unlikely(items.is_empty())) {
					CoreConstants::get_enum_values(SNAME("PropertyUsageFlags"), &items);
				}
				for (const KeyValue<StringName, int64_t> &item : items) {
					ScriptLanguage::CodeCompletionOption option(item.key, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT);
					r_result.insert(option.display, option);
				}
			} break;
		}
	} else if (p_annotation->name == SNAME("@warning_ignore") || p_annotation->name == SNAME("@warning_ignore_start") || p_annotation->name == SNAME("@warning_ignore_restore")) {
		for (int warning_code = 0; warning_code < GDScriptWarning::WARNING_MAX; warning_code++) {
			if (warning_code == GDScriptWarning::RENAMED_IN_GODOT_4_HINT) {
				continue;
			}
			ScriptLanguage::CodeCompletionOption warning(GDScriptWarning::get_name_from_code((GDScriptWarning::Code)warning_code).to_lower(), ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			warning.insert_text = warning.display.quote(p_quote_style);
			r_result.insert(warning.display, warning);
		}
	} else if (p_annotation->name == SNAME("@rpc")) {
		if (p_argument == 0 || p_argument == 1 || p_argument == 2) {
			static const char *options[7] = { "call_local", "call_remote", "any_peer", "authority", "reliable", "unreliable", "unreliable_ordered" };
			for (int i = 0; i < 7; i++) {
				ScriptLanguage::CodeCompletionOption option(options[i], ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
				option.insert_text = option.display.quote(p_quote_style);
				r_result.insert(option.display, option);
			}
		}
	}
}

static void _find_built_in_variants(HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, bool exclude_nil = false) {
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (!exclude_nil && Variant::Type(i) == Variant::Type::NIL) {
			ScriptLanguage::CodeCompletionOption option("null", ScriptLanguage::CODE_COMPLETION_KIND_CLASS);
			r_result.insert(option.display, option);
		} else {
			ScriptLanguage::CodeCompletionOption option(Variant::get_type_name(Variant::Type(i)), ScriptLanguage::CODE_COMPLETION_KIND_CLASS);
			r_result.insert(option.display, option);
		}
	}
}

static void _list_available_types(bool p_inherit_only, GDScriptParser::CompletionContext &p_context, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result) {
	// Built-in Variant Types
	_find_built_in_variants(r_result, true);

	List<StringName> native_types;
	ClassDB::get_class_list(&native_types);
	for (const StringName &E : native_types) {
		if (ClassDB::is_class_exposed(E) && !Engine::get_singleton()->has_singleton(E)) {
			ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_CLASS);
			r_result.insert(option.display, option);
		}
	}

	if (p_context.current_class) {
		if (!p_inherit_only && p_context.current_class->base_type.is_set()) {
			// Native enums from base class
			List<StringName> enums;
			ClassDB::get_enum_list(p_context.current_class->base_type.native_type, &enums);
			for (const StringName &E : enums) {
				ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_ENUM);
				r_result.insert(option.display, option);
			}
		}
		// Check current class for potential types
		const GDScriptParser::ClassNode *current = p_context.current_class;
		while (current) {
			for (int i = 0; i < current->members.size(); i++) {
				const GDScriptParser::ClassNode::Member &member = current->members[i];
				switch (member.type) {
					case GDScriptParser::ClassNode::Member::CLASS: {
						ScriptLanguage::CodeCompletionOption option(member.m_class->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_CLASS, ScriptLanguage::LOCATION_LOCAL);
						r_result.insert(option.display, option);
					} break;
					case GDScriptParser::ClassNode::Member::ENUM: {
						if (!p_inherit_only) {
							ScriptLanguage::CodeCompletionOption option(member.m_enum->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_ENUM, ScriptLanguage::LOCATION_LOCAL);
							r_result.insert(option.display, option);
						}
					} break;
					case GDScriptParser::ClassNode::Member::CONSTANT: {
						if (member.constant->get_datatype().is_meta_type && p_context.current_class->outer != nullptr) {
							ScriptLanguage::CodeCompletionOption option(member.constant->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_CLASS, ScriptLanguage::LOCATION_LOCAL);
							r_result.insert(option.display, option);
						}
					} break;
					default:
						break;
				}
			}
			current = current->outer;
		}
	}

	// Global scripts
	List<StringName> global_classes;
	ScriptServer::get_global_class_list(&global_classes);
	for (const StringName &E : global_classes) {
		ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_CLASS, ScriptLanguage::LOCATION_OTHER_USER_CODE);
		r_result.insert(option.display, option);
	}

	// Autoload singletons
	HashMap<StringName, ProjectSettings::AutoloadInfo> autoloads = ProjectSettings::get_singleton()->get_autoload_list();

	for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
		const ProjectSettings::AutoloadInfo &info = E.value;
		if (!info.is_singleton || info.path.get_extension().to_lower() != "gd") {
			continue;
		}
		ScriptLanguage::CodeCompletionOption option(info.name, ScriptLanguage::CODE_COMPLETION_KIND_CLASS, ScriptLanguage::LOCATION_OTHER_USER_CODE);
		r_result.insert(option.display, option);
	}
}

static void _find_identifiers_in_suite(const GDScriptParser::SuiteNode *p_suite, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, int p_recursion_depth = 0) {
	for (int i = 0; i < p_suite->locals.size(); i++) {
		ScriptLanguage::CodeCompletionOption option;
		int location = p_recursion_depth == 0 ? ScriptLanguage::LOCATION_LOCAL : (p_recursion_depth | ScriptLanguage::LOCATION_PARENT_MASK);
		if (p_suite->locals[i].type == GDScriptParser::SuiteNode::Local::CONSTANT) {
			option = ScriptLanguage::CodeCompletionOption(p_suite->locals[i].name, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
			option.default_value = p_suite->locals[i].constant->initializer->reduced_value;
		} else {
			option = ScriptLanguage::CodeCompletionOption(p_suite->locals[i].name, ScriptLanguage::CODE_COMPLETION_KIND_VARIABLE, location);
		}
		r_result.insert(option.display, option);
	}
	if (p_suite->parent_block) {
		_find_identifiers_in_suite(p_suite->parent_block, r_result, p_recursion_depth + 1);
	}
}

static void _find_identifiers_in_base(const GDScriptCompletionIdentifier &p_base, bool p_only_functions, bool p_types_only, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, int p_recursion_depth);

static void _find_identifiers_in_class(const GDScriptParser::ClassNode *p_class, bool p_only_functions, bool p_types_only, bool p_static, bool p_parent_only, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, int p_recursion_depth) {
	ERR_FAIL_COND(p_recursion_depth > COMPLETION_RECURSION_LIMIT);

	if (!p_parent_only) {
		bool outer = false;
		const GDScriptParser::ClassNode *clss = p_class;
		int classes_processed = 0;
		while (clss) {
			for (int i = 0; i < clss->members.size(); i++) {
				const int location = p_recursion_depth == 0 ? classes_processed : (p_recursion_depth | ScriptLanguage::LOCATION_PARENT_MASK);
				const GDScriptParser::ClassNode::Member &member = clss->members[i];
				ScriptLanguage::CodeCompletionOption option;
				switch (member.type) {
					case GDScriptParser::ClassNode::Member::VARIABLE:
						if (p_types_only || p_only_functions || outer || (p_static && !member.variable->is_static)) {
							continue;
						}
						option = ScriptLanguage::CodeCompletionOption(member.variable->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER, location);
						break;
					case GDScriptParser::ClassNode::Member::CONSTANT:
						if (p_types_only || p_only_functions) {
							continue;
						}
						if (r_result.has(member.constant->identifier->name)) {
							continue;
						}
						option = ScriptLanguage::CodeCompletionOption(member.constant->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
						if (member.constant->initializer) {
							option.default_value = member.constant->initializer->reduced_value;
						}
						break;
					case GDScriptParser::ClassNode::Member::CLASS:
						if (p_only_functions) {
							continue;
						}
						option = ScriptLanguage::CodeCompletionOption(member.m_class->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_CLASS, location);
						break;
					case GDScriptParser::ClassNode::Member::ENUM_VALUE:
						if (p_types_only || p_only_functions) {
							continue;
						}
						option = ScriptLanguage::CodeCompletionOption(member.enum_value.identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
						break;
					case GDScriptParser::ClassNode::Member::ENUM:
						if (p_only_functions) {
							continue;
						}
						option = ScriptLanguage::CodeCompletionOption(member.m_enum->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_ENUM, location);
						break;
					case GDScriptParser::ClassNode::Member::FUNCTION:
						if (p_types_only || outer || (p_static && !member.function->is_static) || member.function->identifier->name.operator String().begins_with("@")) {
							continue;
						}
						option = ScriptLanguage::CodeCompletionOption(member.function->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION, location);
						if (member.function->parameters.size() > 0 || (member.function->info.flags & METHOD_FLAG_VARARG)) {
							option.insert_text += "(";
							option.display += U"(\u2026)";
						} else {
							option.insert_text += "()";
							option.display += "()";
						}
						break;
					case GDScriptParser::ClassNode::Member::SIGNAL:
						if (p_types_only || p_only_functions || outer || p_static) {
							continue;
						}
						option = ScriptLanguage::CodeCompletionOption(member.signal->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_SIGNAL, location);
						break;
					case GDScriptParser::ClassNode::Member::GROUP:
						break; // No-op, but silences warnings.
					case GDScriptParser::ClassNode::Member::UNDEFINED:
						break;
				}
				r_result.insert(option.display, option);
			}
			if (p_types_only) {
				break; // Otherwise, it will fill the results with types from the outer class (which is undesired for that case).
			}

			outer = true;
			clss = clss->outer;
			classes_processed++;
		}
	}

	// Parents.
	GDScriptCompletionIdentifier base_type;
	base_type.type = p_class->base_type;
	base_type.type.is_meta_type = p_static;

	_find_identifiers_in_base(base_type, p_only_functions, p_types_only, r_result, p_recursion_depth + 1);
}

static void _find_identifiers_in_base(const GDScriptCompletionIdentifier &p_base, bool p_only_functions, bool p_types_only, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, int p_recursion_depth) {
	ERR_FAIL_COND(p_recursion_depth > COMPLETION_RECURSION_LIMIT);

	GDScriptParser::DataType base_type = p_base.type;

	if (!p_types_only && base_type.is_meta_type && base_type.kind != GDScriptParser::DataType::BUILTIN && base_type.kind != GDScriptParser::DataType::ENUM) {
		ScriptLanguage::CodeCompletionOption option("new", ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION, ScriptLanguage::LOCATION_LOCAL);
		option.insert_text += "(";
		option.display += U"(\u2026)";
		r_result.insert(option.display, option);
	}

	while (!base_type.has_no_type()) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS: {
				_find_identifiers_in_class(base_type.class_type, p_only_functions, p_types_only, base_type.is_meta_type, false, r_result, p_recursion_depth);
				// This already finds all parent identifiers, so we are done.
				base_type = GDScriptParser::DataType();
			} break;
			case GDScriptParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					if (p_types_only) {
						// TODO: Need to implement Script::get_script_enum_list and retrieve the enum list from a script.
					} else if (!p_only_functions) {
						if (!base_type.is_meta_type) {
							List<PropertyInfo> members;
							scr->get_script_property_list(&members);
							for (const PropertyInfo &E : members) {
								if (E.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL)) {
									continue;
								}
								if (E.name.contains_char('/')) {
									continue;
								}
								int location = p_recursion_depth + _get_property_location(scr, E.name);
								ScriptLanguage::CodeCompletionOption option(E.name, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER, location);
								r_result.insert(option.display, option);
							}

							List<MethodInfo> signals;
							scr->get_script_signal_list(&signals);
							for (const MethodInfo &E : signals) {
								int location = p_recursion_depth + _get_signal_location(scr, E.name);
								ScriptLanguage::CodeCompletionOption option(E.name, ScriptLanguage::CODE_COMPLETION_KIND_SIGNAL, location);
								r_result.insert(option.display, option);
							}
						}
						HashMap<StringName, Variant> constants;
						scr->get_constants(&constants);
						for (const KeyValue<StringName, Variant> &E : constants) {
							int location = p_recursion_depth + _get_constant_location(scr, E.key);
							ScriptLanguage::CodeCompletionOption option(E.key.operator String(), ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
							r_result.insert(option.display, option);
						}
					}

					if (!p_types_only) {
						List<MethodInfo> methods;
						scr->get_script_method_list(&methods);
						for (const MethodInfo &E : methods) {
							if (E.name.begins_with("@")) {
								continue;
							}
							int location = p_recursion_depth + _get_method_location(scr->get_class_name(), E.name);
							ScriptLanguage::CodeCompletionOption option(E.name, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION, location);
							if (E.arguments.size() || (E.flags & METHOD_FLAG_VARARG)) {
								option.insert_text += "(";
								option.display += U"(\u2026)";
							} else {
								option.insert_text += "()";
								option.display += "()";
							}
							r_result.insert(option.display, option);
						}
					}

					Ref<Script> base_script = scr->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.builtin_type = Variant::OBJECT;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName type = base_type.native_type;
				if (!ClassDB::class_exists(type)) {
					return;
				}

				List<StringName> enums;
				ClassDB::get_enum_list(type, &enums);
				for (const StringName &E : enums) {
					int location = p_recursion_depth + _get_enum_location(type, E);
					ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_ENUM, location);
					r_result.insert(option.display, option);
				}

				if (p_types_only) {
					return;
				}

				if (!p_only_functions) {
					List<String> constants;
					ClassDB::get_integer_constant_list(type, &constants);
					for (const String &E : constants) {
						int location = p_recursion_depth + _get_constant_location(type, StringName(E));
						ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
						r_result.insert(option.display, option);
					}

					if (!base_type.is_meta_type || Engine::get_singleton()->has_singleton(type)) {
						List<PropertyInfo> pinfo;
						ClassDB::get_property_list(type, &pinfo);
						for (const PropertyInfo &E : pinfo) {
							if (E.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL)) {
								continue;
							}
							if (E.name.contains_char('/')) {
								continue;
							}
							int location = p_recursion_depth + _get_property_location(type, E.name);
							ScriptLanguage::CodeCompletionOption option(E.name, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER, location);
							r_result.insert(option.display, option);
						}

						List<MethodInfo> signals;
						ClassDB::get_signal_list(type, &signals);
						for (const MethodInfo &E : signals) {
							int location = p_recursion_depth + _get_signal_location(type, StringName(E.name));
							ScriptLanguage::CodeCompletionOption option(E.name, ScriptLanguage::CODE_COMPLETION_KIND_SIGNAL, location);
							r_result.insert(option.display, option);
						}
					}
				}

				bool only_static = base_type.is_meta_type && !Engine::get_singleton()->has_singleton(type);

				List<MethodInfo> methods;
				ClassDB::get_method_list(type, &methods, false, true);
				for (const MethodInfo &E : methods) {
					if (only_static && (E.flags & METHOD_FLAG_STATIC) == 0) {
						continue;
					}
					if (E.name.begins_with("_")) {
						continue;
					}
					int location = p_recursion_depth + _get_method_location(type, E.name);
					ScriptLanguage::CodeCompletionOption option(E.name, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION, location);
					if (E.arguments.size() || (E.flags & METHOD_FLAG_VARARG)) {
						option.insert_text += "(";
						option.display += U"(\u2026)";
					} else {
						option.insert_text += "()";
						option.display += "()";
					}
					r_result.insert(option.display, option);
				}
				return;
			} break;
			case GDScriptParser::DataType::ENUM: {
				String type_str = base_type.native_type;
				StringName type = type_str.get_slicec('.', 0);
				StringName type_enum = base_type.enum_type;

				List<StringName> enum_values;
				ClassDB::get_enum_constants(type, type_enum, &enum_values);
				for (const StringName &E : enum_values) {
					int location = p_recursion_depth + _get_enum_constant_location(type, E);
					ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
					r_result.insert(option.display, option);
				}
			}
				[[fallthrough]];
			case GDScriptParser::DataType::BUILTIN: {
				if (p_types_only) {
					return;
				}

				Callable::CallError err;
				Variant tmp;
				Variant::construct(base_type.builtin_type, tmp, nullptr, 0, err);
				if (err.error != Callable::CallError::CALL_OK) {
					return;
				}

				int location = ScriptLanguage::LOCATION_OTHER;

				if (!p_only_functions) {
					List<PropertyInfo> members;
					if (p_base.value.get_type() != Variant::NIL) {
						p_base.value.get_property_list(&members);
					} else {
						tmp.get_property_list(&members);
					}

					for (const PropertyInfo &E : members) {
						if (E.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL)) {
							continue;
						}
						if (!String(E.name).contains_char('/')) {
							ScriptLanguage::CodeCompletionOption option(E.name, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER, location);
							if (base_type.kind == GDScriptParser::DataType::ENUM) {
								// Sort enum members in their declaration order.
								location += 1;
							}
							if (GDScriptParser::theme_color_names.has(E.name)) {
								option.theme_color_name = GDScriptParser::theme_color_names[E.name];
							}
							r_result.insert(option.display, option);
						}
					}
				}

				List<MethodInfo> methods;
				tmp.get_method_list(&methods);
				for (const MethodInfo &E : methods) {
					if (base_type.kind == GDScriptParser::DataType::ENUM && base_type.is_meta_type && !(E.flags & METHOD_FLAG_CONST)) {
						// Enum types are static and cannot change, therefore we skip non-const dictionary methods.
						continue;
					}
					ScriptLanguage::CodeCompletionOption option(E.name, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION, location);
					if (E.arguments.size() || (E.flags & METHOD_FLAG_VARARG)) {
						option.insert_text += "(";
						option.display += U"(\u2026)";
					} else {
						option.insert_text += "()";
						option.display += "()";
					}
					r_result.insert(option.display, option);
				}

				return;
			} break;
			default: {
				return;
			} break;
		}
	}
}

static void _find_identifiers(const GDScriptParser::CompletionContext &p_context, bool p_only_functions, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, int p_recursion_depth) {
	if (!p_only_functions && p_context.current_suite) {
		// This includes function parameters, since they are also locals.
		_find_identifiers_in_suite(p_context.current_suite, r_result);
	}

	if (p_context.current_class) {
		_find_identifiers_in_class(p_context.current_class, p_only_functions, false, (!p_context.current_function || p_context.current_function->is_static), false, r_result, p_recursion_depth);
	}

	List<StringName> functions;
	GDScriptUtilityFunctions::get_function_list(&functions);

	for (const StringName &E : functions) {
		MethodInfo function = GDScriptUtilityFunctions::get_function_info(E);
		ScriptLanguage::CodeCompletionOption option(String(E), ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
		if (function.arguments.size() || (function.flags & METHOD_FLAG_VARARG)) {
			option.insert_text += "(";
			option.display += U"(\u2026)";
		} else {
			option.insert_text += "()";
			option.display += "()";
		}
		r_result.insert(option.display, option);
	}

	if (p_only_functions) {
		return;
	}

	_find_built_in_variants(r_result);

	static const char *_keywords[] = {
		"true", "false", "PI", "TAU", "INF", "NAN", "null", "self", "super",
		"break", "breakpoint", "continue", "pass", "return",
		nullptr
	};

	const char **kw = _keywords;
	while (*kw) {
		ScriptLanguage::CodeCompletionOption option(*kw, ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
		r_result.insert(option.display, option);
		kw++;
	}

	static const char *_keywords_with_space[] = {
		"and", "not", "or", "in", "as", "class", "class_name", "extends", "is", "func", "signal", "await",
		"const", "enum", "static", "var", "if", "elif", "else", "for", "match", "when", "while",
		nullptr
	};

	const char **kws = _keywords_with_space;
	while (*kws) {
		ScriptLanguage::CodeCompletionOption option(*kws, ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
		option.insert_text += " ";
		r_result.insert(option.display, option);
		kws++;
	}

	static const char *_keywords_with_args[] = {
		"assert", "preload",
		nullptr
	};

	const char **kwa = _keywords_with_args;
	while (*kwa) {
		ScriptLanguage::CodeCompletionOption option(*kwa, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
		option.insert_text += "(";
		option.display += U"(\u2026)";
		r_result.insert(option.display, option);
		kwa++;
	}

	List<StringName> utility_func_names;
	Variant::get_utility_function_list(&utility_func_names);

	for (List<StringName>::Element *E = utility_func_names.front(); E; E = E->next()) {
		ScriptLanguage::CodeCompletionOption option(E->get(), ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
		option.insert_text += "(";
		option.display += U"(\u2026)"; // As all utility functions contain an argument or more, this is hardcoded here.
		r_result.insert(option.display, option);
	}

	for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : ProjectSettings::get_singleton()->get_autoload_list()) {
		if (!E.value.is_singleton) {
			continue;
		}
		ScriptLanguage::CodeCompletionOption option(E.key, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT);
		r_result.insert(option.display, option);
	}

	// Native classes and global constants.
	for (const KeyValue<StringName, int> &E : GDScriptLanguage::get_singleton()->get_global_map()) {
		ScriptLanguage::CodeCompletionOption option;
		if (ClassDB::class_exists(E.key) || Engine::get_singleton()->has_singleton(E.key)) {
			option = ScriptLanguage::CodeCompletionOption(E.key.operator String(), ScriptLanguage::CODE_COMPLETION_KIND_CLASS);
		} else {
			option = ScriptLanguage::CodeCompletionOption(E.key.operator String(), ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT);
		}
		r_result.insert(option.display, option);
	}

	// Global classes
	List<StringName> global_classes;
	ScriptServer::get_global_class_list(&global_classes);
	for (const StringName &E : global_classes) {
		ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_CLASS, ScriptLanguage::LOCATION_OTHER_USER_CODE);
		r_result.insert(option.display, option);
	}
}

static GDScriptCompletionIdentifier _type_from_variant(const Variant &p_value, GDScriptParser::CompletionContext &p_context) {
	GDScriptCompletionIdentifier ci;
	ci.value = p_value;
	ci.type.is_constant = true;
	ci.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	ci.type.kind = GDScriptParser::DataType::BUILTIN;
	ci.type.builtin_type = p_value.get_type();

	if (ci.type.builtin_type == Variant::OBJECT) {
		Object *obj = p_value.operator Object *();
		if (!obj) {
			return ci;
		}
		ci.type.native_type = obj->get_class_name();
		Ref<Script> scr = p_value;
		if (scr.is_valid()) {
			ci.type.is_meta_type = true;
		} else {
			ci.type.is_meta_type = false;
			scr = obj->get_script();
		}
		if (scr.is_valid()) {
			ci.type.script_path = scr->get_path();
			ci.type.script_type = scr;
			ci.type.native_type = scr->get_instance_base_type();
			ci.type.kind = GDScriptParser::DataType::SCRIPT;

			if (scr->get_path().ends_with(".gd")) {
				Ref<GDScriptParserRef> parser = p_context.parser->get_depended_parser_for(scr->get_path());
				if (parser.is_valid() && parser->raise_status(GDScriptParserRef::INTERFACE_SOLVED) == OK) {
					ci.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
					ci.type.class_type = parser->get_parser()->get_tree();
					ci.type.kind = GDScriptParser::DataType::CLASS;
					return ci;
				}
			}
		} else {
			ci.type.kind = GDScriptParser::DataType::NATIVE;
		}
	}

	return ci;
}

static GDScriptCompletionIdentifier _type_from_property(const PropertyInfo &p_property) {
	GDScriptCompletionIdentifier ci;

	if (p_property.type == Variant::NIL) {
		// Variant
		return ci;
	}

	if (p_property.usage & (PROPERTY_USAGE_CLASS_IS_ENUM | PROPERTY_USAGE_CLASS_IS_BITFIELD)) {
		ci.enumeration = p_property.class_name;
	}

	ci.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	ci.type.builtin_type = p_property.type;
	if (p_property.type == Variant::OBJECT) {
		if (ScriptServer::is_global_class(p_property.class_name)) {
			ci.type.kind = GDScriptParser::DataType::SCRIPT;
			ci.type.script_path = ScriptServer::get_global_class_path(p_property.class_name);
			ci.type.native_type = ScriptServer::get_global_class_native_base(p_property.class_name);

			Ref<Script> scr = ResourceLoader::load(ScriptServer::get_global_class_path(p_property.class_name));
			if (scr.is_valid()) {
				ci.type.script_type = scr;
			}
		} else {
			ci.type.kind = GDScriptParser::DataType::NATIVE;
			ci.type.native_type = p_property.class_name == StringName() ? "Object" : p_property.class_name;
		}
	} else {
		ci.type.kind = GDScriptParser::DataType::BUILTIN;
	}
	return ci;
}

#define MAX_COMPLETION_RECURSION 100
struct RecursionCheck {
	int *counter;
	_FORCE_INLINE_ bool check() {
		return (*counter) > MAX_COMPLETION_RECURSION;
	}
	RecursionCheck(int *p_counter) :
			counter(p_counter) {
		(*counter)++;
	}
	~RecursionCheck() {
		(*counter)--;
	}
};

static bool _guess_identifier_type(GDScriptParser::CompletionContext &p_context, const GDScriptParser::IdentifierNode *p_identifier, GDScriptCompletionIdentifier &r_type);
static bool _guess_identifier_type_from_base(GDScriptParser::CompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_identifier, GDScriptCompletionIdentifier &r_type);
static bool _guess_method_return_type_from_base(GDScriptParser::CompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_method, GDScriptCompletionIdentifier &r_type);

static bool _is_expression_named_identifier(const GDScriptParser::ExpressionNode *p_expression, const StringName &p_name) {
	if (p_expression) {
		switch (p_expression->type) {
			case GDScriptParser::Node::IDENTIFIER: {
				const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(p_expression);
				if (id->name == p_name) {
					return true;
				}
			} break;
			case GDScriptParser::Node::CAST: {
				const GDScriptParser::CastNode *cn = static_cast<const GDScriptParser::CastNode *>(p_expression);
				return _is_expression_named_identifier(cn->operand, p_name);
			} break;
			default:
				break;
		}
	}

	return false;
}

static bool _guess_expression_type(GDScriptParser::CompletionContext &p_context, const GDScriptParser::ExpressionNode *p_expression, GDScriptCompletionIdentifier &r_type) {
	bool found = false;

	if (p_expression == nullptr) {
		return false;
	}

	static int recursion_depth = 0;
	RecursionCheck recursion(&recursion_depth);
	if (unlikely(recursion.check())) {
		ERR_FAIL_V_MSG(false, "Reached recursion limit while trying to guess type.");
	}

	if (p_expression->is_constant) {
		// Already has a value, so just use that.
		r_type = _type_from_variant(p_expression->reduced_value, p_context);
		switch (p_expression->get_datatype().kind) {
			case GDScriptParser::DataType::ENUM:
			case GDScriptParser::DataType::CLASS:
				r_type.type = p_expression->get_datatype();
				break;
			default:
				break;
		}
		found = true;
	} else {
		switch (p_expression->type) {
			case GDScriptParser::Node::LITERAL: {
				const GDScriptParser::LiteralNode *literal = static_cast<const GDScriptParser::LiteralNode *>(p_expression);
				r_type = _type_from_variant(literal->value, p_context);
				found = true;
			} break;
			case GDScriptParser::Node::SELF: {
				if (p_context.current_class) {
					r_type.type = p_context.current_class->get_datatype();
					r_type.type.is_meta_type = false;
					found = true;
				}
			} break;
			case GDScriptParser::Node::IDENTIFIER: {
				const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(p_expression);
				found = _guess_identifier_type(p_context, id, r_type);
			} break;
			case GDScriptParser::Node::DICTIONARY: {
				// Try to recreate the dictionary.
				const GDScriptParser::DictionaryNode *dn = static_cast<const GDScriptParser::DictionaryNode *>(p_expression);
				Dictionary d;
				bool full = true;
				for (int i = 0; i < dn->elements.size(); i++) {
					GDScriptCompletionIdentifier key;
					if (_guess_expression_type(p_context, dn->elements[i].key, key)) {
						if (!key.type.is_constant) {
							full = false;
							break;
						}
						GDScriptCompletionIdentifier value;
						if (_guess_expression_type(p_context, dn->elements[i].value, value)) {
							if (!value.type.is_constant) {
								full = false;
								break;
							}
							d[key.value] = value.value;
						} else {
							full = false;
							break;
						}
					} else {
						full = false;
						break;
					}
				}
				if (full) {
					r_type.value = d;
					r_type.type.is_constant = true;
				}
				r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
				r_type.type.kind = GDScriptParser::DataType::BUILTIN;
				r_type.type.builtin_type = Variant::DICTIONARY;
				found = true;
			} break;
			case GDScriptParser::Node::ARRAY: {
				// Try to recreate the array
				const GDScriptParser::ArrayNode *an = static_cast<const GDScriptParser::ArrayNode *>(p_expression);
				Array a;
				bool full = true;
				a.resize(an->elements.size());
				for (int i = 0; i < an->elements.size(); i++) {
					GDScriptCompletionIdentifier value;
					if (_guess_expression_type(p_context, an->elements[i], value)) {
						if (value.type.is_constant) {
							a[i] = value.value;
						} else {
							full = false;
							break;
						}
					} else {
						full = false;
						break;
					}
				}
				if (full) {
					// If not fully constant, setting this value is detrimental to the inference.
					r_type.value = a;
					r_type.type.is_constant = true;
				}
				r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
				r_type.type.kind = GDScriptParser::DataType::BUILTIN;
				r_type.type.builtin_type = Variant::ARRAY;
				found = true;
			} break;
			case GDScriptParser::Node::CAST: {
				const GDScriptParser::CastNode *cn = static_cast<const GDScriptParser::CastNode *>(p_expression);
				GDScriptCompletionIdentifier value;
				if (_guess_expression_type(p_context, cn->operand, r_type)) {
					r_type.type = cn->get_datatype();
					found = true;
				}
			} break;
			case GDScriptParser::Node::CALL: {
				const GDScriptParser::CallNode *call = static_cast<const GDScriptParser::CallNode *>(p_expression);
				if (GDScriptParser::get_builtin_type(call->function_name) < Variant::VARIANT_MAX) {
					r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
					r_type.type.kind = GDScriptParser::DataType::BUILTIN;
					r_type.type.builtin_type = GDScriptParser::get_builtin_type(call->function_name);
					found = true;
					break;
				} else if (GDScriptUtilityFunctions::function_exists(call->function_name)) {
					MethodInfo mi = GDScriptUtilityFunctions::get_function_info(call->function_name);
					r_type = _type_from_property(mi.return_val);
					found = true;
					break;
				} else {
					GDScriptParser::CompletionContext c = p_context;
					c.current_line = call->start_line;

					GDScriptParser::Node::Type callee_type = call->get_callee_type();

					GDScriptCompletionIdentifier base;
					if (callee_type == GDScriptParser::Node::IDENTIFIER || call->is_super) {
						// Simple call, so base is 'self'.
						if (p_context.current_class) {
							if (call->is_super) {
								base.type = p_context.current_class->base_type;
								base.value = p_context.base;
							} else {
								base.type.kind = GDScriptParser::DataType::CLASS;
								base.type.type_source = GDScriptParser::DataType::INFERRED;
								base.type.is_constant = true;
								base.type.class_type = p_context.current_class;
								base.value = p_context.base;
							}
						} else {
							break;
						}
					} else if (callee_type == GDScriptParser::Node::SUBSCRIPT && static_cast<const GDScriptParser::SubscriptNode *>(call->callee)->is_attribute) {
						if (!_guess_expression_type(c, static_cast<const GDScriptParser::SubscriptNode *>(call->callee)->base, base)) {
							found = false;
							break;
						}
					} else {
						break;
					}

					// Try call if constant methods with constant arguments
					if (base.type.is_constant && base.value.get_type() == Variant::OBJECT) {
						GDScriptParser::DataType native_type = base.type;

						while (native_type.kind == GDScriptParser::DataType::CLASS) {
							native_type = native_type.class_type->base_type;
						}

						while (native_type.kind == GDScriptParser::DataType::SCRIPT) {
							if (native_type.script_type.is_valid()) {
								Ref<Script> parent = native_type.script_type->get_base_script();
								if (parent.is_valid()) {
									native_type.script_type = parent;
								} else {
									native_type.kind = GDScriptParser::DataType::NATIVE;
									native_type.builtin_type = Variant::OBJECT;
									native_type.native_type = native_type.script_type->get_instance_base_type();
									if (!ClassDB::class_exists(native_type.native_type)) {
										native_type.kind = GDScriptParser::DataType::UNRESOLVED;
									}
								}
							}
						}

						if (native_type.kind == GDScriptParser::DataType::NATIVE) {
							MethodBind *mb = ClassDB::get_method(native_type.native_type, call->function_name);
							if (mb && mb->is_const()) {
								bool all_is_const = true;
								Vector<Variant> args;
								for (int i = 0; all_is_const && i < call->arguments.size(); i++) {
									GDScriptCompletionIdentifier arg;

									if (!call->arguments[i]->is_constant) {
										all_is_const = false;
									}
								}

								Object *baseptr = base.value;

								if (all_is_const && call->function_name == SNAME("get_node") && ClassDB::is_parent_class(native_type.native_type, SNAME("Node")) && args.size()) {
									String arg1 = args[0];
									if (arg1.begins_with("/root/")) {
										String which = arg1.get_slice("/", 2);
										if (!which.is_empty()) {
											// Try singletons first
											if (GDScriptLanguage::get_singleton()->get_named_globals_map().has(which)) {
												r_type = _type_from_variant(GDScriptLanguage::get_singleton()->get_named_globals_map()[which], p_context);
												found = true;
											} else {
												for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : ProjectSettings::get_singleton()->get_autoload_list()) {
													String name = E.key;
													if (name == which) {
														String script = E.value.path;

														if (!script.begins_with("res://")) {
															script = "res://" + script;
														}

														if (!script.ends_with(".gd")) {
															// not a script, try find the script anyway,
															// may have some success
															script = script.get_basename() + ".gd";
														}

														if (FileAccess::exists(script)) {
															Ref<GDScriptParserRef> parser = p_context.parser->get_depended_parser_for(script);
															if (parser.is_valid() && parser->raise_status(GDScriptParserRef::INTERFACE_SOLVED) == OK) {
																r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
																r_type.type.script_path = script;
																r_type.type.class_type = parser->get_parser()->get_tree();
																r_type.type.is_constant = false;
																r_type.type.kind = GDScriptParser::DataType::CLASS;
																r_type.value = Variant();
																found = true;
															}
														}
														break;
													}
												}
											}
										}
									}
								}

								if (!found && all_is_const && baseptr) {
									Vector<const Variant *> argptr;
									for (int i = 0; i < args.size(); i++) {
										argptr.push_back(&args[i]);
									}

									Callable::CallError ce;
									Variant ret = mb->call(baseptr, (const Variant **)argptr.ptr(), argptr.size(), ce);

									if (ce.error == Callable::CallError::CALL_OK && ret.get_type() != Variant::NIL) {
										if (ret.get_type() != Variant::OBJECT || ret.operator Object *() != nullptr) {
											r_type = _type_from_variant(ret, p_context);
											found = true;
										}
									}
								}
							}
						}
					}

					if (!found && base.value.get_type() != Variant::NIL) {
						found = _guess_method_return_type_from_base(c, base, call->function_name, r_type);
					}
				}
			} break;
			case GDScriptParser::Node::SUBSCRIPT: {
				const GDScriptParser::SubscriptNode *subscript = static_cast<const GDScriptParser::SubscriptNode *>(p_expression);
				if (subscript->is_attribute) {
					GDScriptParser::CompletionContext c = p_context;
					c.current_line = subscript->start_line;

					GDScriptCompletionIdentifier base;
					if (!_guess_expression_type(c, subscript->base, base)) {
						found = false;
						break;
					}

					if (base.value.get_type() == Variant::DICTIONARY && base.value.operator Dictionary().has(String(subscript->attribute->name))) {
						Variant value = base.value.operator Dictionary()[String(subscript->attribute->name)];
						r_type = _type_from_variant(value, p_context);
						found = true;
						break;
					}

					const GDScriptParser::DictionaryNode *dn = nullptr;
					if (subscript->base->type == GDScriptParser::Node::DICTIONARY) {
						dn = static_cast<const GDScriptParser::DictionaryNode *>(subscript->base);
					} else if (base.assigned_expression && base.assigned_expression->type == GDScriptParser::Node::DICTIONARY) {
						dn = static_cast<const GDScriptParser::DictionaryNode *>(base.assigned_expression);
					}

					if (dn) {
						for (int i = 0; i < dn->elements.size(); i++) {
							GDScriptCompletionIdentifier key;
							if (!_guess_expression_type(c, dn->elements[i].key, key)) {
								continue;
							}
							if (key.value == String(subscript->attribute->name)) {
								r_type.assigned_expression = dn->elements[i].value;
								found = _guess_expression_type(c, dn->elements[i].value, r_type);
								break;
							}
						}
					}

					if (!found) {
						found = _guess_identifier_type_from_base(c, base, subscript->attribute->name, r_type);
					}
				} else {
					if (subscript->index == nullptr) {
						found = false;
						break;
					}

					GDScriptParser::CompletionContext c = p_context;
					c.current_line = subscript->start_line;

					GDScriptCompletionIdentifier base;
					if (!_guess_expression_type(c, subscript->base, base)) {
						found = false;
						break;
					}

					GDScriptCompletionIdentifier index;
					if (!_guess_expression_type(c, subscript->index, index)) {
						found = false;
						break;
					}

					{
						bool valid;
						Variant value = base.value.get(index.value, &valid);
						if (valid) {
							r_type = _type_from_variant(value, p_context);
							found = true;
							break;
						}
					}

					// Look if it is a dictionary node.
					const GDScriptParser::DictionaryNode *dn = nullptr;
					if (subscript->base->type == GDScriptParser::Node::DICTIONARY) {
						dn = static_cast<const GDScriptParser::DictionaryNode *>(subscript->base);
					} else if (base.assigned_expression && base.assigned_expression->type == GDScriptParser::Node::DICTIONARY) {
						dn = static_cast<const GDScriptParser::DictionaryNode *>(base.assigned_expression);
					}

					if (dn) {
						for (int i = 0; i < dn->elements.size(); i++) {
							GDScriptCompletionIdentifier key;
							if (!_guess_expression_type(c, dn->elements[i].key, key)) {
								continue;
							}
							if (key.value == index.value) {
								r_type.assigned_expression = dn->elements[i].value;
								found = _guess_expression_type(p_context, dn->elements[i].value, r_type);
								break;
							}
						}
					}

					// Look if it is an array node.
					if (!found && index.value.is_num()) {
						int idx = index.value;
						const GDScriptParser::ArrayNode *an = nullptr;
						if (subscript->base->type == GDScriptParser::Node::ARRAY) {
							an = static_cast<const GDScriptParser::ArrayNode *>(subscript->base);
						} else if (base.assigned_expression && base.assigned_expression->type == GDScriptParser::Node::ARRAY) {
							an = static_cast<const GDScriptParser::ArrayNode *>(base.assigned_expression);
						}

						if (an && idx >= 0 && an->elements.size() > idx) {
							r_type.assigned_expression = an->elements[idx];
							found = _guess_expression_type(c, an->elements[idx], r_type);
							break;
						}
					}

					// Look for valid indexing in other types
					if (!found && (index.value.is_string() || index.value.get_type() == Variant::NODE_PATH)) {
						StringName id = index.value;
						found = _guess_identifier_type_from_base(c, base, id, r_type);
					} else if (!found && index.type.kind == GDScriptParser::DataType::BUILTIN) {
						Callable::CallError err;
						Variant base_val;
						Variant::construct(base.type.builtin_type, base_val, nullptr, 0, err);
						bool valid = false;
						Variant res = base_val.get(index.value, &valid);
						if (valid) {
							r_type = _type_from_variant(res, p_context);
							r_type.value = Variant();
							r_type.type.is_constant = false;
							found = true;
						}
					}
				}
			} break;
			case GDScriptParser::Node::BINARY_OPERATOR: {
				const GDScriptParser::BinaryOpNode *op = static_cast<const GDScriptParser::BinaryOpNode *>(p_expression);

				if (op->variant_op == Variant::OP_MAX) {
					break;
				}

				GDScriptParser::CompletionContext context = p_context;
				context.current_line = op->start_line;

				GDScriptCompletionIdentifier p1;
				GDScriptCompletionIdentifier p2;

				if (!_guess_expression_type(context, op->left_operand, p1)) {
					found = false;
					break;
				}

				if (!_guess_expression_type(context, op->right_operand, p2)) {
					found = false;
					break;
				}

				Callable::CallError ce;
				bool v1_use_value = p1.value.get_type() != Variant::NIL && p1.value.get_type() != Variant::OBJECT;
				Variant d1;
				Variant::construct(p1.type.builtin_type, d1, nullptr, 0, ce);
				Variant d2;
				Variant::construct(p2.type.builtin_type, d2, nullptr, 0, ce);

				Variant v1 = (v1_use_value) ? p1.value : d1;
				bool v2_use_value = p2.value.get_type() != Variant::NIL && p2.value.get_type() != Variant::OBJECT;
				Variant v2 = (v2_use_value) ? p2.value : d2;
				// avoid potential invalid ops
				if ((op->variant_op == Variant::OP_DIVIDE || op->variant_op == Variant::OP_MODULE) && v2.get_type() == Variant::INT) {
					v2 = 1;
					v2_use_value = false;
				}
				if (op->variant_op == Variant::OP_DIVIDE && v2.get_type() == Variant::FLOAT) {
					v2 = 1.0;
					v2_use_value = false;
				}

				Variant res;
				bool valid;
				Variant::evaluate(op->variant_op, v1, v2, res, valid);
				if (!valid) {
					found = false;
					break;
				}
				r_type = _type_from_variant(res, p_context);
				if (!v1_use_value || !v2_use_value) {
					r_type.value = Variant();
					r_type.type.is_constant = false;
				}

				found = true;
			} break;
			default:
				break;
		}
	}

	// It may have found a null, but that's never useful
	if (found && r_type.type.kind == GDScriptParser::DataType::BUILTIN && r_type.type.builtin_type == Variant::NIL) {
		found = false;
	}

	// If the found type was not fully analyzed we analyze it now.
	if (found && r_type.type.kind == GDScriptParser::DataType::CLASS && !r_type.type.class_type->resolved_body) {
		Error err;
		Ref<GDScriptParserRef> r = GDScriptCache::get_parser(r_type.type.script_path, GDScriptParserRef::FULLY_SOLVED, err);
	}

	// Check type hint last. For collections we want chance to get the actual value first
	// This way we can detect types from the content of dictionaries and arrays
	if (!found && p_expression->get_datatype().is_hard_type()) {
		r_type.type = p_expression->get_datatype();
		if (!r_type.assigned_expression) {
			r_type.assigned_expression = p_expression;
		}
		found = true;
	}

	return found;
}

static bool _guess_identifier_type(GDScriptParser::CompletionContext &p_context, const GDScriptParser::IdentifierNode *p_identifier, GDScriptCompletionIdentifier &r_type) {
	static int recursion_depth = 0;
	RecursionCheck recursion(&recursion_depth);
	if (unlikely(recursion.check())) {
		ERR_FAIL_V_MSG(false, "Reached recursion limit while trying to guess type.");
	}

	// Look in blocks first.
	int last_assign_line = -1;
	const GDScriptParser::ExpressionNode *last_assigned_expression = nullptr;
	GDScriptCompletionIdentifier id_type;
	GDScriptParser::SuiteNode *suite = p_context.current_suite;
	bool is_function_parameter = false;

	bool can_be_local = true;
	switch (p_identifier->source) {
		case GDScriptParser::IdentifierNode::MEMBER_VARIABLE:
		case GDScriptParser::IdentifierNode::MEMBER_CONSTANT:
		case GDScriptParser::IdentifierNode::MEMBER_FUNCTION:
		case GDScriptParser::IdentifierNode::MEMBER_SIGNAL:
		case GDScriptParser::IdentifierNode::MEMBER_CLASS:
		case GDScriptParser::IdentifierNode::INHERITED_VARIABLE:
		case GDScriptParser::IdentifierNode::STATIC_VARIABLE:
			can_be_local = false;
			break;
		default:
			break;
	}

	if (can_be_local && suite && suite->has_local(p_identifier->name)) {
		const GDScriptParser::SuiteNode::Local &local = suite->get_local(p_identifier->name);

		id_type.type = local.get_datatype();

		// Check initializer as the first assignment.
		switch (local.type) {
			case GDScriptParser::SuiteNode::Local::VARIABLE:
				if (local.variable->initializer) {
					last_assign_line = local.variable->initializer->end_line;
					last_assigned_expression = local.variable->initializer;
				}
				break;
			case GDScriptParser::SuiteNode::Local::CONSTANT:
				if (local.constant->initializer) {
					last_assign_line = local.constant->initializer->end_line;
					last_assigned_expression = local.constant->initializer;
				}
				break;
			case GDScriptParser::SuiteNode::Local::PARAMETER:
				if (local.parameter->initializer) {
					last_assign_line = local.parameter->initializer->end_line;
					last_assigned_expression = local.parameter->initializer;
				}
				is_function_parameter = true;
				break;
			default:
				break;
		}
	} else {
		if (p_context.current_class) {
			GDScriptCompletionIdentifier base_identifier;

			GDScriptCompletionIdentifier base;
			base.value = p_context.base;
			base.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
			base.type.kind = GDScriptParser::DataType::CLASS;
			base.type.class_type = p_context.current_class;
			base.type.is_meta_type = p_context.current_function && p_context.current_function->is_static;

			if (_guess_identifier_type_from_base(p_context, base, p_identifier->name, base_identifier)) {
				id_type = base_identifier;
			}
		}
	}

	while (suite) {
		for (int i = 0; i < suite->statements.size(); i++) {
			if (suite->statements[i]->end_line >= p_context.current_line) {
				break;
			}

			switch (suite->statements[i]->type) {
				case GDScriptParser::Node::ASSIGNMENT: {
					const GDScriptParser::AssignmentNode *assign = static_cast<const GDScriptParser::AssignmentNode *>(suite->statements[i]);
					if (assign->end_line > last_assign_line && assign->assignee && assign->assigned_value && assign->assignee->type == GDScriptParser::Node::IDENTIFIER) {
						const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(assign->assignee);
						if (id->name == p_identifier->name && id->source == p_identifier->source) {
							last_assign_line = assign->assigned_value->end_line;
							last_assigned_expression = assign->assigned_value;
						}
					}
				} break;
				default:
					// TODO: Check sub blocks (control flow statements) as they might also reassign stuff.
					break;
			}
		}

		if (suite->parent_if && suite->parent_if->condition && suite->parent_if->condition->type == GDScriptParser::Node::TYPE_TEST) {
			// Operator `is` used, check if identifier is in there! this helps resolve in blocks that are (if (identifier is value)): which are very common..
			// Super dirty hack, but very useful.
			// Credit: Zylann.
			// TODO: this could be hacked to detect ANDed conditions too...
			const GDScriptParser::TypeTestNode *type_test = static_cast<const GDScriptParser::TypeTestNode *>(suite->parent_if->condition);
			if (type_test->operand && type_test->test_type && type_test->operand->type == GDScriptParser::Node::IDENTIFIER && static_cast<const GDScriptParser::IdentifierNode *>(type_test->operand)->name == p_identifier->name && static_cast<const GDScriptParser::IdentifierNode *>(type_test->operand)->source == p_identifier->source) {
				// Bingo.
				GDScriptParser::CompletionContext c = p_context;
				c.current_line = type_test->operand->start_line;
				c.current_suite = suite;
				if (type_test->test_datatype.is_hard_type()) {
					id_type.type = type_test->test_datatype;
					if (last_assign_line < c.current_line) {
						// Override last assignment.
						last_assign_line = c.current_line;
						last_assigned_expression = nullptr;
					}
				}
			}
		}

		suite = suite->parent_block;
	}

	if (last_assigned_expression && last_assign_line < p_context.current_line) {
		GDScriptParser::CompletionContext c = p_context;
		c.current_line = last_assign_line;
		GDScriptCompletionIdentifier assigned_type;
		if (_guess_expression_type(c, last_assigned_expression, assigned_type)) {
			if (id_type.type.is_set() && assigned_type.type.is_set() && !GDScriptAnalyzer::check_type_compatibility(id_type.type, assigned_type.type)) {
				// The assigned type is incompatible. The annotated type takes priority.
				r_type = id_type;
				r_type.assigned_expression = last_assigned_expression;
			} else {
				r_type = assigned_type;
			}
			return true;
		}
	}

	if (is_function_parameter && p_context.current_function && p_context.current_function->source_lambda == nullptr && p_context.current_class) {
		// Check if it's override of native function, then we can assume the type from the signature.
		GDScriptParser::DataType base_type = p_context.current_class->base_type;
		while (base_type.is_set()) {
			switch (base_type.kind) {
				case GDScriptParser::DataType::CLASS:
					if (base_type.class_type->has_function(p_context.current_function->identifier->name)) {
						GDScriptParser::FunctionNode *parent_function = base_type.class_type->get_member(p_context.current_function->identifier->name).function;
						if (parent_function->parameters_indices.has(p_identifier->name)) {
							const GDScriptParser::ParameterNode *parameter = parent_function->parameters[parent_function->parameters_indices[p_identifier->name]];
							if ((!id_type.type.is_set() || id_type.type.is_variant()) && parameter->get_datatype().is_hard_type()) {
								id_type.type = parameter->get_datatype();
							}
							if (parameter->initializer) {
								GDScriptParser::CompletionContext c = p_context;
								c.current_function = parent_function;
								c.current_class = base_type.class_type;
								c.base = nullptr;
								if (_guess_expression_type(c, parameter->initializer, r_type)) {
									return true;
								}
							}
						}
					}
					base_type = base_type.class_type->base_type;
					break;
				case GDScriptParser::DataType::NATIVE: {
					if (id_type.type.is_set() && !id_type.type.is_variant()) {
						base_type = GDScriptParser::DataType();
						break;
					}
					MethodInfo info;
					if (ClassDB::get_method_info(base_type.native_type, p_context.current_function->identifier->name, &info)) {
						for (const PropertyInfo &E : info.arguments) {
							if (E.name == p_identifier->name) {
								r_type = _type_from_property(E);
								return true;
							}
						}
					}
					base_type = GDScriptParser::DataType();
				} break;
				default:
					break;
			}
		}
	}

	if (id_type.type.is_set() && !id_type.type.is_variant()) {
		r_type = id_type;
		return true;
	}

	// Check global scripts.
	if (ScriptServer::is_global_class(p_identifier->name)) {
		String script = ScriptServer::get_global_class_path(p_identifier->name);
		if (script.to_lower().ends_with(".gd")) {
			Ref<GDScriptParserRef> parser = p_context.parser->get_depended_parser_for(script);
			if (parser.is_valid() && parser->raise_status(GDScriptParserRef::INTERFACE_SOLVED) == OK) {
				r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
				r_type.type.script_path = script;
				r_type.type.class_type = parser->get_parser()->get_tree();
				r_type.type.is_meta_type = true;
				r_type.type.is_constant = false;
				r_type.type.kind = GDScriptParser::DataType::CLASS;
				r_type.value = Variant();
				return true;
			}
		} else {
			Ref<Script> scr = ResourceLoader::load(ScriptServer::get_global_class_path(p_identifier->name));
			if (scr.is_valid()) {
				r_type = _type_from_variant(scr, p_context);
				r_type.type.is_meta_type = true;
				return true;
			}
		}
		return false;
	}

	// Check global variables (including autoloads).
	if (GDScriptLanguage::get_singleton()->get_named_globals_map().has(p_identifier->name)) {
		r_type = _type_from_variant(GDScriptLanguage::get_singleton()->get_named_globals_map()[p_identifier->name], p_context);
		return true;
	}

	// Check ClassDB.
	if (ClassDB::class_exists(p_identifier->name) && ClassDB::is_class_exposed(p_identifier->name)) {
		r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
		r_type.type.kind = GDScriptParser::DataType::NATIVE;
		r_type.type.builtin_type = Variant::OBJECT;
		r_type.type.native_type = p_identifier->name;
		r_type.type.is_constant = true;
		if (Engine::get_singleton()->has_singleton(p_identifier->name)) {
			r_type.type.is_meta_type = false;
			r_type.value = Engine::get_singleton()->get_singleton_object(p_identifier->name);
		} else {
			r_type.type.is_meta_type = true;
			r_type.value = Variant();
		}
	}

	return false;
}

static bool _guess_identifier_type_from_base(GDScriptParser::CompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_identifier, GDScriptCompletionIdentifier &r_type) {
	static int recursion_depth = 0;
	RecursionCheck recursion(&recursion_depth);
	if (unlikely(recursion.check())) {
		ERR_FAIL_V_MSG(false, "Reached recursion limit while trying to guess type.");
	}

	GDScriptParser::DataType base_type = p_base.type;
	bool is_static = base_type.is_meta_type;
	while (base_type.is_set()) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS:
				if (base_type.class_type->has_member(p_identifier)) {
					const GDScriptParser::ClassNode::Member &member = base_type.class_type->get_member(p_identifier);
					switch (member.type) {
						case GDScriptParser::ClassNode::Member::CONSTANT:
							r_type.type = member.constant->get_datatype();
							if (member.constant->initializer && member.constant->initializer->is_constant) {
								r_type.value = member.constant->initializer->reduced_value;
							}
							return true;
						case GDScriptParser::ClassNode::Member::VARIABLE:
							if (!is_static || member.variable->is_static) {
								if (member.variable->get_datatype().is_set() && !member.variable->get_datatype().is_variant()) {
									r_type.type = member.variable->get_datatype();
									return true;
								} else if (member.variable->initializer) {
									const GDScriptParser::ExpressionNode *init = member.variable->initializer;
									if (init->is_constant) {
										r_type.value = init->reduced_value;
										r_type = _type_from_variant(init->reduced_value, p_context);
										return true;
									} else if (init->start_line == p_context.current_line) {
										return false;
										// Detects if variable is assigned to itself
									} else if (_is_expression_named_identifier(init, member.variable->identifier->name)) {
										if (member.variable->initializer->get_datatype().is_set()) {
											r_type.type = member.variable->initializer->get_datatype();
										} else if (member.variable->get_datatype().is_set() && !member.variable->get_datatype().is_variant()) {
											r_type.type = member.variable->get_datatype();
										}
										return true;
									} else if (_guess_expression_type(p_context, init, r_type)) {
										return true;
									} else if (init->get_datatype().is_set() && !init->get_datatype().is_variant()) {
										r_type.type = init->get_datatype();
										return true;
									}
								}
							}
							// TODO: Check assignments in constructor.
							return false;
						case GDScriptParser::ClassNode::Member::ENUM:
							r_type.type = member.m_enum->get_datatype();
							r_type.enumeration = member.m_enum->identifier->name;
							return true;
						case GDScriptParser::ClassNode::Member::ENUM_VALUE:
							r_type = _type_from_variant(member.enum_value.value, p_context);
							return true;
						case GDScriptParser::ClassNode::Member::SIGNAL:
							r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
							r_type.type.kind = GDScriptParser::DataType::BUILTIN;
							r_type.type.builtin_type = Variant::SIGNAL;
							return true;
						case GDScriptParser::ClassNode::Member::FUNCTION:
							if (is_static && !member.function->is_static) {
								return false;
							}
							r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
							r_type.type.kind = GDScriptParser::DataType::BUILTIN;
							r_type.type.builtin_type = Variant::CALLABLE;
							return true;
						case GDScriptParser::ClassNode::Member::CLASS:
							r_type.type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
							r_type.type.kind = GDScriptParser::DataType::CLASS;
							r_type.type.class_type = member.m_class;
							r_type.type.is_meta_type = true;
							return true;
						case GDScriptParser::ClassNode::Member::GROUP:
							return false; // No-op, but silences warnings.
						case GDScriptParser::ClassNode::Member::UNDEFINED:
							return false; // Unreachable.
					}
					return false;
				}
				base_type = base_type.class_type->base_type;
				break;
			case GDScriptParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					HashMap<StringName, Variant> constants;
					scr->get_constants(&constants);
					if (constants.has(p_identifier)) {
						r_type = _type_from_variant(constants[p_identifier], p_context);
						return true;
					}

					List<PropertyInfo> members;
					if (is_static) {
						scr->get_property_list(&members);
					} else {
						scr->get_script_property_list(&members);
					}
					for (const PropertyInfo &prop : members) {
						if (prop.name == p_identifier) {
							r_type = _type_from_property(prop);
							return true;
						}
					}

					Ref<Script> parent = scr->get_base_script();
					if (parent.is_valid()) {
						base_type.script_type = parent;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.builtin_type = Variant::OBJECT;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return false;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName class_name = base_type.native_type;
				if (!ClassDB::class_exists(class_name)) {
					return false;
				}

				// Skip constants since they're all integers. Type does not matter because int has no members.

				PropertyInfo prop;
				if (ClassDB::get_property_info(class_name, p_identifier, &prop)) {
					StringName getter = ClassDB::get_property_getter(class_name, p_identifier);
					if (getter != StringName()) {
						MethodBind *g = ClassDB::get_method(class_name, getter);
						if (g) {
							r_type = _type_from_property(g->get_return_info());
							return true;
						}
					} else {
						r_type = _type_from_property(prop);
						return true;
					}
				}
				return false;
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				Callable::CallError err;
				Variant tmp;
				Variant::construct(base_type.builtin_type, tmp, nullptr, 0, err);

				if (err.error != Callable::CallError::CALL_OK) {
					return false;
				}
				bool valid = false;
				Variant res = tmp.get(p_identifier, &valid);
				if (valid) {
					r_type = _type_from_variant(res, p_context);
					r_type.value = Variant();
					r_type.type.is_constant = false;
					return true;
				}
				return false;
			} break;
			default: {
				return false;
			} break;
		}
	}
	return false;
}

static void _find_last_return_in_block(GDScriptParser::CompletionContext &p_context, int &r_last_return_line, const GDScriptParser::ExpressionNode **r_last_returned_value) {
	if (!p_context.current_suite) {
		return;
	}

	for (int i = 0; i < p_context.current_suite->statements.size(); i++) {
		if (p_context.current_suite->statements[i]->start_line < r_last_return_line) {
			break;
		}

		GDScriptParser::CompletionContext c = p_context;
		switch (p_context.current_suite->statements[i]->type) {
			case GDScriptParser::Node::FOR:
				c.current_suite = static_cast<const GDScriptParser::ForNode *>(p_context.current_suite->statements[i])->loop;
				_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				break;
			case GDScriptParser::Node::WHILE:
				c.current_suite = static_cast<const GDScriptParser::WhileNode *>(p_context.current_suite->statements[i])->loop;
				_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				break;
			case GDScriptParser::Node::IF: {
				const GDScriptParser::IfNode *_if = static_cast<const GDScriptParser::IfNode *>(p_context.current_suite->statements[i]);
				c.current_suite = _if->true_block;
				_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				if (_if->false_block) {
					c.current_suite = _if->false_block;
					_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				}
			} break;
			case GDScriptParser::Node::MATCH: {
				const GDScriptParser::MatchNode *match = static_cast<const GDScriptParser::MatchNode *>(p_context.current_suite->statements[i]);
				for (int j = 0; j < match->branches.size(); j++) {
					c.current_suite = match->branches[j]->block;
					_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				}
			} break;
			case GDScriptParser::Node::RETURN: {
				const GDScriptParser::ReturnNode *ret = static_cast<const GDScriptParser::ReturnNode *>(p_context.current_suite->statements[i]);
				if (ret->return_value) {
					if (ret->start_line > r_last_return_line) {
						r_last_return_line = ret->start_line;
						*r_last_returned_value = ret->return_value;
					}
				}
			} break;
			default:
				break;
		}
	}
}

static bool _guess_method_return_type_from_base(GDScriptParser::CompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_method, GDScriptCompletionIdentifier &r_type) {
	static int recursion_depth = 0;
	RecursionCheck recursion(&recursion_depth);
	if (unlikely(recursion.check())) {
		ERR_FAIL_V_MSG(false, "Reached recursion limit while trying to guess type.");
	}

	GDScriptParser::DataType base_type = p_base.type;
	bool is_static = base_type.is_meta_type;

	if (is_static && p_method == SNAME("new")) {
		r_type.type = base_type;
		r_type.type.is_meta_type = false;
		r_type.type.is_constant = false;
		return true;
	}

	while (base_type.is_set() && !base_type.is_variant()) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS:
				if (base_type.class_type->has_function(p_method)) {
					GDScriptParser::FunctionNode *method = base_type.class_type->get_member(p_method).function;
					if (!is_static || method->is_static) {
						if (method->get_datatype().is_set() && !method->get_datatype().is_variant()) {
							r_type.type = method->get_datatype();
							return true;
						}

						int last_return_line = -1;
						const GDScriptParser::ExpressionNode *last_returned_value = nullptr;
						GDScriptParser::CompletionContext c = p_context;
						c.current_class = base_type.class_type;
						c.current_function = method;
						c.current_suite = method->body;

						_find_last_return_in_block(c, last_return_line, &last_returned_value);
						if (last_returned_value) {
							c.current_line = c.current_suite->end_line;
							if (_guess_expression_type(c, last_returned_value, r_type)) {
								return true;
							}
						}
					}
				}
				base_type = base_type.class_type->base_type;
				break;
			case GDScriptParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					List<MethodInfo> methods;
					scr->get_script_method_list(&methods);
					for (const MethodInfo &mi : methods) {
						if (mi.name == p_method) {
							r_type = _type_from_property(mi.return_val);
							return true;
						}
					}
					Ref<Script> base_script = scr->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.builtin_type = Variant::OBJECT;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return false;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				if (!ClassDB::class_exists(base_type.native_type)) {
					return false;
				}
				MethodBind *mb = ClassDB::get_method(base_type.native_type, p_method);
				if (mb) {
					r_type = _type_from_property(mb->get_return_info());
					return true;
				}
				return false;
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				Callable::CallError err;
				Variant tmp;
				Variant::construct(base_type.builtin_type, tmp, nullptr, 0, err);
				if (err.error != Callable::CallError::CALL_OK) {
					return false;
				}

				List<MethodInfo> methods;
				tmp.get_method_list(&methods);

				for (const MethodInfo &mi : methods) {
					if (mi.name == p_method) {
						r_type = _type_from_property(mi.return_val);
						return true;
					}
				}
				return false;
			} break;
			default: {
				return false;
			}
		}
	}

	return false;
}

static void _find_enumeration_candidates(GDScriptParser::CompletionContext &p_context, const String &p_enum_hint, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result) {
	if (!p_enum_hint.contains_char('.')) {
		// Global constant or in the current class.
		StringName current_enum = p_enum_hint;
		if (p_context.current_class && p_context.current_class->has_member(current_enum) && p_context.current_class->get_member(current_enum).type == GDScriptParser::ClassNode::Member::ENUM) {
			const GDScriptParser::EnumNode *_enum = p_context.current_class->get_member(current_enum).m_enum;
			for (int i = 0; i < _enum->values.size(); i++) {
				ScriptLanguage::CodeCompletionOption option(_enum->values[i].identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_ENUM);
				r_result.insert(option.display, option);
			}
		} else {
			for (int i = 0; i < CoreConstants::get_global_constant_count(); i++) {
				if (CoreConstants::get_global_constant_enum(i) == current_enum) {
					ScriptLanguage::CodeCompletionOption option(CoreConstants::get_global_constant_name(i), ScriptLanguage::CODE_COMPLETION_KIND_ENUM);
					r_result.insert(option.display, option);
				}
			}
		}
	} else {
		String class_name = p_enum_hint.get_slice(".", 0);
		String enum_name = p_enum_hint.get_slice(".", 1);

		if (!ClassDB::class_exists(class_name)) {
			return;
		}

		List<StringName> enum_constants;
		ClassDB::get_enum_constants(class_name, enum_name, &enum_constants);
		for (const StringName &E : enum_constants) {
			String candidate = class_name + "." + E;
			int location = _get_enum_constant_location(class_name, E);
			ScriptLanguage::CodeCompletionOption option(candidate, ScriptLanguage::CODE_COMPLETION_KIND_ENUM, location);
			r_result.insert(option.display, option);
		}
	}
}

static void _find_call_arguments(GDScriptParser::CompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_method, int p_argidx, bool p_static, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, String &r_arghint) {
	Variant base = p_base.value;
	GDScriptParser::DataType base_type = p_base.type;

	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";
	const bool use_string_names = EDITOR_GET("text_editor/completion/add_string_name_literals");
	const bool use_node_paths = EDITOR_GET("text_editor/completion/add_node_path_literals");

	while (base_type.is_set() && !base_type.is_variant()) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS: {
				if (base_type.is_meta_type && p_method == SNAME("new")) {
					const GDScriptParser::ClassNode *current = base_type.class_type;

					do {
						if (current->has_member("_init")) {
							const GDScriptParser::ClassNode::Member &member = current->get_member("_init");

							if (member.type == GDScriptParser::ClassNode::Member::FUNCTION) {
								r_arghint = base_type.class_type->get_datatype().to_string() + " new" + _make_arguments_hint(member.function, p_argidx, true);
								return;
							}
						}
						current = current->base_type.class_type;
					} while (current != nullptr);

					r_arghint = base_type.class_type->get_datatype().to_string() + " new()";
					return;
				}

				if (base_type.class_type->has_member(p_method)) {
					const GDScriptParser::ClassNode::Member &member = base_type.class_type->get_member(p_method);

					if (member.type == GDScriptParser::ClassNode::Member::FUNCTION) {
						r_arghint = _make_arguments_hint(member.function, p_argidx);
						return;
					}
				}

				base_type = base_type.class_type->base_type;
			} break;
			case GDScriptParser::DataType::SCRIPT: {
				if (base_type.script_type->is_valid() && base_type.script_type->has_method(p_method)) {
					r_arghint = _make_arguments_hint(base_type.script_type->get_method_info(p_method), p_argidx);
					return;
				}
				Ref<Script> base_script = base_type.script_type->get_base_script();
				if (base_script.is_valid()) {
					base_type.script_type = base_script;
				} else {
					base_type.kind = GDScriptParser::DataType::NATIVE;
					base_type.builtin_type = Variant::OBJECT;
					base_type.native_type = base_type.script_type->get_instance_base_type();
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName class_name = base_type.native_type;
				if (!ClassDB::class_exists(class_name)) {
					base_type.kind = GDScriptParser::DataType::UNRESOLVED;
					break;
				}

				MethodInfo info;
				int method_args = 0;

				if (ClassDB::get_method_info(class_name, p_method, &info)) {
					method_args = info.arguments.size();
					if (base.get_type() == Variant::OBJECT) {
						Object *obj = base.operator Object *();
						if (obj) {
							List<String> options;
							obj->get_argument_options(p_method, p_argidx, &options);
							for (String &opt : options) {
								// Handle user preference.
								if (opt.is_quoted()) {
									opt = opt.unquote().quote(quote_style);
									if (use_string_names && info.arguments.get(p_argidx).type == Variant::STRING_NAME) {
										opt = "&" + opt;
									} else if (use_node_paths && info.arguments.get(p_argidx).type == Variant::NODE_PATH) {
										opt = "^" + opt;
									}
								}
								ScriptLanguage::CodeCompletionOption option(opt, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
								r_result.insert(option.display, option);
							}
						}
					}

					if (p_argidx < method_args) {
						const PropertyInfo &arg_info = info.arguments.get(p_argidx);
						if (arg_info.usage & (PROPERTY_USAGE_CLASS_IS_ENUM | PROPERTY_USAGE_CLASS_IS_BITFIELD)) {
							_find_enumeration_candidates(p_context, arg_info.class_name, r_result);
						}
					}

					r_arghint = _make_arguments_hint(info, p_argidx);
				}

				if (p_argidx == 1 && p_context.node && p_context.node->type == GDScriptParser::Node::CALL && ClassDB::is_parent_class(class_name, SNAME("Tween")) && p_method == SNAME("tween_property")) {
					// Get tweened objects properties.
					GDScriptParser::ExpressionNode *tweened_object = static_cast<GDScriptParser::CallNode *>(p_context.node)->arguments[0];
					StringName native_type = tweened_object->datatype.native_type;
					switch (tweened_object->datatype.kind) {
						case GDScriptParser::DataType::SCRIPT: {
							Ref<Script> script = tweened_object->datatype.script_type;
							native_type = script->get_instance_base_type();
							int n = 0;
							while (script.is_valid()) {
								List<PropertyInfo> properties;
								script->get_script_property_list(&properties);
								for (const PropertyInfo &E : properties) {
									if (E.usage & (PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_INTERNAL)) {
										continue;
									}
									String name = E.name.quote(quote_style);
									if (use_node_paths) {
										name = "^" + name;
									}
									ScriptLanguage::CodeCompletionOption option(name, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER, ScriptLanguage::CodeCompletionLocation::LOCATION_LOCAL + n);
									r_result.insert(option.display, option);
								}
								script = script->get_base_script();
								n++;
							}
						} break;
						case GDScriptParser::DataType::CLASS: {
							GDScriptParser::ClassNode *clss = tweened_object->datatype.class_type;
							native_type = clss->base_type.native_type;
							int n = 0;
							while (clss) {
								for (GDScriptParser::ClassNode::Member member : clss->members) {
									if (member.type == GDScriptParser::ClassNode::Member::VARIABLE) {
										String name = member.get_name().quote(quote_style);
										if (use_node_paths) {
											name = "^" + name;
										}
										ScriptLanguage::CodeCompletionOption option(name, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER, ScriptLanguage::CodeCompletionLocation::LOCATION_LOCAL + n);
										r_result.insert(option.display, option);
									}
								}
								if (clss->base_type.kind == GDScriptParser::DataType::Kind::CLASS) {
									clss = clss->base_type.class_type;
									n++;
								} else {
									native_type = clss->base_type.native_type;
									clss = nullptr;
								}
							}
						} break;
						default:
							break;
					}

					List<PropertyInfo> properties;
					ClassDB::get_property_list(native_type, &properties);
					for (const PropertyInfo &E : properties) {
						if (E.usage & (PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_INTERNAL)) {
							continue;
						}
						String name = E.name.quote(quote_style);
						if (use_node_paths) {
							name = "^" + name;
						}
						ScriptLanguage::CodeCompletionOption option(name, ScriptLanguage::CODE_COMPLETION_KIND_MEMBER);
						r_result.insert(option.display, option);
					}
				}

				if (p_argidx == 0 && ClassDB::is_parent_class(class_name, SNAME("Node")) && (p_method == SNAME("get_node") || p_method == SNAME("has_node"))) {
					// Get autoloads
					List<PropertyInfo> props;
					ProjectSettings::get_singleton()->get_property_list(&props);

					for (const PropertyInfo &E : props) {
						String s = E.name;
						if (!s.begins_with("autoload/")) {
							continue;
						}
						String name = s.get_slice("/", 1);
						String path = ("/root/" + name).quote(quote_style);
						if (use_node_paths) {
							path = "^" + path;
						}
						ScriptLanguage::CodeCompletionOption option(path, ScriptLanguage::CODE_COMPLETION_KIND_NODE_PATH);
						r_result.insert(option.display, option);
					}
				}

				if (p_argidx == 0 && method_args > 0 && ClassDB::is_parent_class(class_name, SNAME("InputEvent")) && p_method.operator String().contains("action")) {
					// Get input actions
					List<PropertyInfo> props;
					ProjectSettings::get_singleton()->get_property_list(&props);
					for (const PropertyInfo &E : props) {
						String s = E.name;
						if (!s.begins_with("input/")) {
							continue;
						}
						String name = s.get_slice("/", 1).quote(quote_style);
						if (use_string_names) {
							name = "&" + name;
						}
						ScriptLanguage::CodeCompletionOption option(name, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT);
						r_result.insert(option.display, option);
					}
				}
				if (EDITOR_GET("text_editor/completion/complete_file_paths")) {
					if (p_argidx == 0 && p_method == SNAME("change_scene_to_file") && ClassDB::is_parent_class(class_name, SNAME("SceneTree"))) {
						HashMap<String, ScriptLanguage::CodeCompletionOption> list;
						_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), list, SNAME("PackedScene"));
						for (const KeyValue<String, ScriptLanguage::CodeCompletionOption> &key_value_pair : list) {
							ScriptLanguage::CodeCompletionOption option = key_value_pair.value;
							r_result.insert(option.display, option);
						}
					}
				}

				base_type.kind = GDScriptParser::DataType::UNRESOLVED;
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				if (base.get_type() == Variant::NIL) {
					Callable::CallError err;
					Variant::construct(base_type.builtin_type, base, nullptr, 0, err);
					if (err.error != Callable::CallError::CALL_OK) {
						return;
					}
				}

				List<MethodInfo> methods;
				base.get_method_list(&methods);
				for (const MethodInfo &E : methods) {
					if (E.name == p_method) {
						r_arghint = _make_arguments_hint(E, p_argidx);
						return;
					}
				}

				base_type.kind = GDScriptParser::DataType::UNRESOLVED;
			} break;
			default: {
				base_type.kind = GDScriptParser::DataType::UNRESOLVED;
			} break;
		}
	}
}

static bool _get_subscript_type(GDScriptParser::CompletionContext &p_context, const GDScriptParser::SubscriptNode *p_subscript, GDScriptParser::DataType &r_base_type, Variant *r_base = nullptr) {
	if (p_context.base == nullptr) {
		return false;
	}

	const GDScriptParser::GetNodeNode *get_node = nullptr;

	switch (p_subscript->base->type) {
		case GDScriptParser::Node::GET_NODE: {
			get_node = static_cast<GDScriptParser::GetNodeNode *>(p_subscript->base);
		} break;

		case GDScriptParser::Node::IDENTIFIER: {
			const GDScriptParser::IdentifierNode *identifier_node = static_cast<GDScriptParser::IdentifierNode *>(p_subscript->base);

			switch (identifier_node->source) {
				case GDScriptParser::IdentifierNode::Source::MEMBER_VARIABLE: {
					if (p_context.current_class != nullptr) {
						const StringName &member_name = identifier_node->name;
						const GDScriptParser::ClassNode *current_class = p_context.current_class;

						if (current_class->has_member(member_name)) {
							const GDScriptParser::ClassNode::Member &member = current_class->get_member(member_name);

							if (member.type == GDScriptParser::ClassNode::Member::VARIABLE) {
								const GDScriptParser::VariableNode *variable = static_cast<GDScriptParser::VariableNode *>(member.variable);

								if (variable->initializer && variable->initializer->type == GDScriptParser::Node::GET_NODE) {
									get_node = static_cast<GDScriptParser::GetNodeNode *>(variable->initializer);
								}
							}
						}
					}
				} break;
				case GDScriptParser::IdentifierNode::Source::LOCAL_VARIABLE: {
					if (identifier_node->next != nullptr && identifier_node->next->type == GDScriptParser::ClassNode::Node::GET_NODE) {
						get_node = static_cast<GDScriptParser::GetNodeNode *>(identifier_node->next);
					}
				} break;
				default: {
				} break;
			}
		} break;
		default: {
		} break;
	}

	if (get_node != nullptr) {
		const Object *node = p_context.base->call("get_node_or_null", NodePath(get_node->full_path));
		if (node != nullptr) {
			GDScriptParser::DataType assigned_type = _type_from_variant(node, p_context).type;
			GDScriptParser::DataType base_type = p_subscript->base->datatype;

			if (p_subscript->base->type == GDScriptParser::Node::IDENTIFIER && base_type.type_source == GDScriptParser::DataType::ANNOTATED_EXPLICIT && (assigned_type.kind != base_type.kind || assigned_type.script_path != base_type.script_path || assigned_type.native_type != base_type.native_type)) {
				// Annotated type takes precedence.
				return false;
			}

			if (r_base != nullptr) {
				*r_base = node;
			}

			r_base_type.type_source = GDScriptParser::DataType::INFERRED;
			r_base_type.builtin_type = Variant::OBJECT;
			r_base_type.native_type = node->get_class_name();

			Ref<Script> scr = node->get_script();
			if (scr.is_null()) {
				r_base_type.kind = GDScriptParser::DataType::NATIVE;
			} else {
				r_base_type.kind = GDScriptParser::DataType::SCRIPT;
				r_base_type.script_type = scr;
			}

			return true;
		}
	}

	return false;
}

static void _find_call_arguments(GDScriptParser::CompletionContext &p_context, const GDScriptParser::Node *p_call, int p_argidx, HashMap<String, ScriptLanguage::CodeCompletionOption> &r_result, bool &r_forced, String &r_arghint) {
	if (p_call->type == GDScriptParser::Node::PRELOAD) {
		if (p_argidx == 0 && bool(EDITOR_GET("text_editor/completion/complete_file_paths"))) {
			_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), r_result);
		}

		MethodInfo mi(PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource"), "preload", PropertyInfo(Variant::STRING, "path"));
		r_arghint = _make_arguments_hint(mi, p_argidx);
		return;
	} else if (p_call->type != GDScriptParser::Node::CALL) {
		return;
	}

	Variant base;
	GDScriptParser::DataType base_type;
	bool _static = false;
	const GDScriptParser::CallNode *call = static_cast<const GDScriptParser::CallNode *>(p_call);
	GDScriptParser::Node::Type callee_type = call->get_callee_type();

	if (callee_type == GDScriptParser::Node::SUBSCRIPT) {
		const GDScriptParser::SubscriptNode *subscript = static_cast<const GDScriptParser::SubscriptNode *>(call->callee);

		if (subscript->base != nullptr && subscript->base->type == GDScriptParser::Node::IDENTIFIER) {
			const GDScriptParser::IdentifierNode *base_identifier = static_cast<const GDScriptParser::IdentifierNode *>(subscript->base);

			Variant::Type method_type = GDScriptParser::get_builtin_type(base_identifier->name);
			if (method_type < Variant::VARIANT_MAX) {
				Variant v;
				Callable::CallError err;
				Variant::construct(method_type, v, nullptr, 0, err);
				if (err.error != Callable::CallError::CALL_OK) {
					return;
				}
				List<MethodInfo> methods;
				v.get_method_list(&methods);

				for (MethodInfo &E : methods) {
					if (p_argidx >= E.arguments.size()) {
						continue;
					}
					if (E.name == call->function_name) {
						r_arghint += _make_arguments_hint(E, p_argidx);
						return;
					}
				}
			}
		}

		if (subscript->is_attribute) {
			bool found_type = _get_subscript_type(p_context, subscript, base_type, &base);

			if (!found_type) {
				GDScriptCompletionIdentifier ci;
				if (_guess_expression_type(p_context, subscript->base, ci)) {
					base_type = ci.type;
					base = ci.value;
				} else {
					return;
				}
			}

			_static = base_type.is_meta_type;
		}
	} else if (Variant::has_utility_function(call->function_name)) {
		MethodInfo info = Variant::get_utility_function_info(call->function_name);
		r_arghint = _make_arguments_hint(info, p_argidx);
		return;
	} else if (GDScriptUtilityFunctions::function_exists(call->function_name)) {
		MethodInfo info = GDScriptUtilityFunctions::get_function_info(call->function_name);
		r_arghint = _make_arguments_hint(info, p_argidx);
		return;
	} else if (GDScriptParser::get_builtin_type(call->function_name) < Variant::VARIANT_MAX) {
		// Complete constructor.
		List<MethodInfo> constructors;
		Variant::get_constructor_list(GDScriptParser::get_builtin_type(call->function_name), &constructors);

		int i = 0;
		for (const MethodInfo &E : constructors) {
			if (p_argidx >= E.arguments.size()) {
				continue;
			}
			if (i > 0) {
				r_arghint += "\n";
			}
			r_arghint += _make_arguments_hint(E, p_argidx);
			i++;
		}
		return;
	} else if (call->is_super || callee_type == GDScriptParser::Node::IDENTIFIER) {
		base = p_context.base;

		if (p_context.current_class) {
			base_type = p_context.current_class->get_datatype();
			_static = !p_context.current_function || p_context.current_function->is_static;
		}
	} else {
		return;
	}

	GDScriptCompletionIdentifier ci;
	ci.type = base_type;
	ci.value = base;
	_find_call_arguments(p_context, ci, call->function_name, p_argidx, _static, r_result, r_arghint);

	r_forced = r_result.size() > 0;
}

::Error GDScriptLanguage::complete_code(const String &p_code, const String &p_path, Object *p_owner, List<ScriptLanguage::CodeCompletionOption> *r_options, bool &r_forced, String &r_call_hint) {
	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";

	GDScriptParser parser;
	GDScriptAnalyzer analyzer(&parser);

	parser.parse(p_code, p_path, true);
	analyzer.analyze();

	r_forced = false;
	HashMap<String, ScriptLanguage::CodeCompletionOption> options;

	GDScriptParser::CompletionContext completion_context = parser.get_completion_context();
	if (completion_context.current_class != nullptr && completion_context.current_class->outer == nullptr) {
		completion_context.base = p_owner;
	}
	bool is_function = false;

	switch (completion_context.type) {
		case GDScriptParser::COMPLETION_NONE:
			break;
		case GDScriptParser::COMPLETION_ANNOTATION: {
			List<MethodInfo> annotations;
			parser.get_annotation_list(&annotations);
			for (const MethodInfo &E : annotations) {
				ScriptLanguage::CodeCompletionOption option(E.name.substr(1), ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
				if (E.arguments.size() > 0) {
					option.insert_text += "(";
				}
				options.insert(option.display, option);
			}
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_ANNOTATION_ARGUMENTS: {
			if (completion_context.node == nullptr || completion_context.node->type != GDScriptParser::Node::ANNOTATION) {
				break;
			}
			const GDScriptParser::AnnotationNode *annotation = static_cast<const GDScriptParser::AnnotationNode *>(completion_context.node);
			_find_annotation_arguments(annotation, completion_context.current_argument, quote_style, options, r_call_hint);
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_BUILT_IN_TYPE_CONSTANT_OR_STATIC_METHOD: {
			// Constants.
			{
				List<StringName> constants;
				Variant::get_constants_for_type(completion_context.builtin_type, &constants);
				for (const StringName &E : constants) {
					ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT);
					bool valid = false;
					Variant default_value = Variant::get_constant_value(completion_context.builtin_type, E, &valid);
					if (valid) {
						option.default_value = default_value;
					}
					options.insert(option.display, option);
				}
			}
			// Methods.
			{
				List<StringName> methods;
				Variant::get_builtin_method_list(completion_context.builtin_type, &methods);
				for (const StringName &E : methods) {
					if (Variant::is_builtin_method_static(completion_context.builtin_type, E)) {
						ScriptLanguage::CodeCompletionOption option(E, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
						if (Variant::get_builtin_method_argument_count(completion_context.builtin_type, E) > 0 || Variant::is_builtin_method_vararg(completion_context.builtin_type, E)) {
							option.insert_text += "(";
						} else {
							option.insert_text += "()";
						}
						options.insert(option.display, option);
					}
				}
			}
		} break;
		case GDScriptParser::COMPLETION_INHERIT_TYPE: {
			_list_available_types(true, completion_context, options);
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_TYPE_NAME_OR_VOID: {
			ScriptLanguage::CodeCompletionOption option("void", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			options.insert(option.display, option);
		}
			[[fallthrough]];
		case GDScriptParser::COMPLETION_TYPE_NAME: {
			_list_available_types(false, completion_context, options);
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_PROPERTY_DECLARATION_OR_TYPE: {
			_list_available_types(false, completion_context, options);
			ScriptLanguage::CodeCompletionOption get("get", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			options.insert(get.display, get);
			ScriptLanguage::CodeCompletionOption set("set", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			options.insert(set.display, set);
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_PROPERTY_DECLARATION: {
			ScriptLanguage::CodeCompletionOption get("get", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			options.insert(get.display, get);
			ScriptLanguage::CodeCompletionOption set("set", ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			options.insert(set.display, set);
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_PROPERTY_METHOD: {
			if (!completion_context.current_class) {
				break;
			}
			for (int i = 0; i < completion_context.current_class->members.size(); i++) {
				const GDScriptParser::ClassNode::Member &member = completion_context.current_class->members[i];
				if (member.type != GDScriptParser::ClassNode::Member::FUNCTION) {
					continue;
				}
				if (member.function->is_static) {
					continue;
				}
				ScriptLanguage::CodeCompletionOption option(member.function->identifier->name, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
				options.insert(option.display, option);
			}
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_ASSIGN: {
			GDScriptCompletionIdentifier type;
			if (!completion_context.node || completion_context.node->type != GDScriptParser::Node::ASSIGNMENT) {
				break;
			}
			if (!_guess_expression_type(completion_context, static_cast<const GDScriptParser::AssignmentNode *>(completion_context.node)->assignee, type)) {
				_find_identifiers(completion_context, false, options, 0);
				r_forced = true;
				break;
			}

			if (!type.enumeration.is_empty()) {
				_find_enumeration_candidates(completion_context, type.enumeration, options);
				r_forced = options.size() > 0;
			} else {
				_find_identifiers(completion_context, false, options, 0);
				r_forced = true;
			}
		} break;
		case GDScriptParser::COMPLETION_METHOD:
			is_function = true;
			[[fallthrough]];
		case GDScriptParser::COMPLETION_IDENTIFIER: {
			_find_identifiers(completion_context, is_function, options, 0);
		} break;
		case GDScriptParser::COMPLETION_ATTRIBUTE_METHOD:
			is_function = true;
			[[fallthrough]];
		case GDScriptParser::COMPLETION_ATTRIBUTE: {
			r_forced = true;
			const GDScriptParser::SubscriptNode *attr = static_cast<const GDScriptParser::SubscriptNode *>(completion_context.node);
			if (attr->base) {
				GDScriptCompletionIdentifier base;
				bool found_type = _get_subscript_type(completion_context, attr, base.type);
				if (!found_type && !_guess_expression_type(completion_context, attr->base, base)) {
					break;
				}

				_find_identifiers_in_base(base, is_function, false, options, 0);
			}
		} break;
		case GDScriptParser::COMPLETION_SUBSCRIPT: {
			const GDScriptParser::SubscriptNode *subscript = static_cast<const GDScriptParser::SubscriptNode *>(completion_context.node);
			GDScriptCompletionIdentifier base;
			const bool res = _guess_expression_type(completion_context, subscript->base, base);

			// If the type is not known, we assume it is BUILTIN, since indices on arrays is the most common use case.
			if (!subscript->is_attribute && (!res || base.type.kind == GDScriptParser::DataType::BUILTIN || base.type.is_variant())) {
				if (base.value.get_type() == Variant::DICTIONARY) {
					List<PropertyInfo> members;
					base.value.get_property_list(&members);

					for (const PropertyInfo &E : members) {
						ScriptLanguage::CodeCompletionOption option(E.name.quote(quote_style), ScriptLanguage::CODE_COMPLETION_KIND_MEMBER, ScriptLanguage::LOCATION_LOCAL);
						options.insert(option.display, option);
					}
				}
				if (!subscript->index || subscript->index->type != GDScriptParser::Node::LITERAL) {
					_find_identifiers(completion_context, false, options, 0);
				}
			} else if (res) {
				if (!subscript->is_attribute) {
					// Quote the options if they are not accessed as attribute.

					HashMap<String, ScriptLanguage::CodeCompletionOption> opt;
					_find_identifiers_in_base(base, false, false, opt, 0);
					for (const KeyValue<String, CodeCompletionOption> &E : opt) {
						ScriptLanguage::CodeCompletionOption option(E.value.insert_text.quote(quote_style), E.value.kind, E.value.location);
						options.insert(option.display, option);
					}
				} else {
					_find_identifiers_in_base(base, false, false, options, 0);
				}
			}
		} break;
		case GDScriptParser::COMPLETION_TYPE_ATTRIBUTE: {
			if (!completion_context.current_class) {
				break;
			}
			const GDScriptParser::TypeNode *type = static_cast<const GDScriptParser::TypeNode *>(completion_context.node);
			bool found = true;

			GDScriptCompletionIdentifier base;
			base.type.kind = GDScriptParser::DataType::CLASS;
			base.type.type_source = GDScriptParser::DataType::INFERRED;
			base.type.is_constant = true;

			if (completion_context.current_argument == 1) {
				StringName type_name = type->type_chain[0]->name;

				if (ClassDB::class_exists(type_name)) {
					base.type.kind = GDScriptParser::DataType::NATIVE;
					base.type.native_type = type_name;
				} else if (ScriptServer::is_global_class(type_name)) {
					base.type.kind = GDScriptParser::DataType::SCRIPT;
					String scr_path = ScriptServer::get_global_class_path(type_name);
					base.type.script_type = ResourceLoader::load(scr_path);
				}
			}

			if (base.type.kind == GDScriptParser::DataType::CLASS) {
				base.type.class_type = completion_context.current_class;
				base.value = completion_context.base;

				for (int i = 0; i < completion_context.current_argument; i++) {
					GDScriptCompletionIdentifier ci;
					if (!_guess_identifier_type_from_base(completion_context, base, type->type_chain[i]->name, ci)) {
						found = false;
						break;
					}
					base = ci;
				}
			}

			if (found) {
				_find_identifiers_in_base(base, false, true, options, 0);
			}
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_RESOURCE_PATH: {
			if (EDITOR_GET("text_editor/completion/complete_file_paths")) {
				_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), options);
				r_forced = true;
			}
		} break;
		case GDScriptParser::COMPLETION_CALL_ARGUMENTS: {
			if (!completion_context.node) {
				break;
			}
			_find_call_arguments(completion_context, completion_context.node, completion_context.current_argument, options, r_forced, r_call_hint);
		} break;
		case GDScriptParser::COMPLETION_OVERRIDE_METHOD: {
			GDScriptParser::DataType native_type = completion_context.current_class->base_type;
			while (native_type.is_set() && native_type.kind != GDScriptParser::DataType::NATIVE) {
				switch (native_type.kind) {
					case GDScriptParser::DataType::CLASS: {
						native_type = native_type.class_type->base_type;
					} break;
					default: {
						native_type.kind = GDScriptParser::DataType::UNRESOLVED;
					} break;
				}
			}

			if (!native_type.is_set()) {
				break;
			}

			StringName class_name = native_type.native_type;
			if (!ClassDB::class_exists(class_name)) {
				break;
			}

			bool use_type_hint = EditorSettings::get_singleton()->get_setting("text_editor/completion/add_type_hints").operator bool();

			List<MethodInfo> virtual_methods;
			ClassDB::get_virtual_methods(class_name, &virtual_methods);

			{
				// Not truly a virtual method, but can also be "overridden".
				MethodInfo static_init("_static_init");
				static_init.return_val.type = Variant::NIL;
				static_init.flags |= METHOD_FLAG_STATIC | METHOD_FLAG_VIRTUAL;
				virtual_methods.push_back(static_init);
			}

			for (const MethodInfo &mi : virtual_methods) {
				String method_hint = mi.name;
				if (method_hint.contains_char(':')) {
					method_hint = method_hint.get_slice(":", 0);
				}
				method_hint += "(";

				for (List<PropertyInfo>::ConstIterator arg_itr = mi.arguments.begin(); arg_itr != mi.arguments.end(); ++arg_itr) {
					if (arg_itr != mi.arguments.begin()) {
						method_hint += ", ";
					}
					String arg = arg_itr->name;
					if (arg.contains_char(':')) {
						arg = arg.substr(0, arg.find_char(':'));
					}
					method_hint += arg;
					if (use_type_hint) {
						method_hint += ": " + _get_visual_datatype(*arg_itr, true, class_name);
					}
				}
				method_hint += ")";
				if (use_type_hint) {
					method_hint += " -> " + _get_visual_datatype(mi.return_val, false, class_name);
				}
				method_hint += ":";

				ScriptLanguage::CodeCompletionOption option(method_hint, ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION);
				options.insert(option.display, option);
			}
		} break;
		case GDScriptParser::COMPLETION_GET_NODE: {
			// Handles the `$Node/Path` or `$"Some NodePath"` syntax specifically.
			if (p_owner) {
				List<String> opts;
				p_owner->get_argument_options("get_node", 0, &opts);

				bool for_unique_name = false;
				if (completion_context.node != nullptr && completion_context.node->type == GDScriptParser::Node::GET_NODE && !static_cast<GDScriptParser::GetNodeNode *>(completion_context.node)->use_dollar) {
					for_unique_name = true;
				}

				for (const String &E : opts) {
					r_forced = true;
					String opt = E.strip_edges();
					if (opt.is_quoted()) {
						// Remove quotes so that we can handle user preferred quote style,
						// or handle NodePaths which are valid identifiers and don't need quotes.
						opt = opt.unquote();
					}

					if (for_unique_name) {
						if (!opt.begins_with("%")) {
							continue;
						}
						opt = opt.substr(1);
					}

					// The path needs quotes if at least one of its components (excluding `%` prefix and `/` separations)
					// is not a valid identifier.
					bool path_needs_quote = false;
					for (const String &part : opt.trim_prefix("%").split("/")) {
						if (!part.is_valid_ascii_identifier()) {
							path_needs_quote = true;
							break;
						}
					}

					if (path_needs_quote) {
						// Ignore quote_style and just use double quotes for paths with apostrophes.
						// Double quotes don't need to be checked because they're not valid in node and property names.
						opt = opt.quote(opt.contains_char('\'') ? "\"" : quote_style); // Handle user preference.
					}
					ScriptLanguage::CodeCompletionOption option(opt, ScriptLanguage::CODE_COMPLETION_KIND_NODE_PATH);
					options.insert(option.display, option);
				}

				if (!for_unique_name) {
					// Get autoloads.
					for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : ProjectSettings::get_singleton()->get_autoload_list()) {
						String path = "/root/" + E.key;
						ScriptLanguage::CodeCompletionOption option(path.quote(quote_style), ScriptLanguage::CODE_COMPLETION_KIND_NODE_PATH);
						options.insert(option.display, option);
					}
				}
			}
		} break;
		case GDScriptParser::COMPLETION_SUPER_METHOD: {
			if (!completion_context.current_class) {
				break;
			}
			_find_identifiers_in_class(completion_context.current_class, true, false, false, true, options, 0);
		} break;
	}

	for (const KeyValue<String, ScriptLanguage::CodeCompletionOption> &E : options) {
		r_options->push_back(E.value);
	}

	return OK;
}

#else // !TOOLS_ENABLED

Error GDScriptLanguage::complete_code(const String &p_code, const String &p_path, Object *p_owner, List<ScriptLanguage::CodeCompletionOption> *r_options, bool &r_forced, String &r_call_hint) {
	return OK;
}

#endif // TOOLS_ENABLED

//////// END COMPLETION //////////

String GDScriptLanguage::_get_indentation() const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		bool use_space_indentation = EDITOR_GET("text_editor/behavior/indent/type");

		if (use_space_indentation) {
			int indent_size = EDITOR_GET("text_editor/behavior/indent/size");
			return String(" ").repeat(indent_size);
		}
	}
#endif
	return "\t";
}

void GDScriptLanguage::auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {
	String indent = _get_indentation();

	Vector<String> lines = p_code.split("\n");
	List<int> indent_stack;

	for (int i = 0; i < lines.size(); i++) {
		String l = lines[i];
		int tc = 0;
		for (int j = 0; j < l.length(); j++) {
			if (l[j] == ' ' || l[j] == '\t') {
				tc++;
			} else {
				break;
			}
		}

		String st = l.substr(tc, l.length()).strip_edges();
		if (st.is_empty() || st.begins_with("#")) {
			continue; //ignore!
		}

		int ilevel = 0;
		if (indent_stack.size()) {
			ilevel = indent_stack.back()->get();
		}

		if (tc > ilevel) {
			indent_stack.push_back(tc);
		} else if (tc < ilevel) {
			while (indent_stack.size() && indent_stack.back()->get() > tc) {
				indent_stack.pop_back();
			}

			if (indent_stack.size() && indent_stack.back()->get() != tc) {
				indent_stack.push_back(tc); // this is not right but gets the job done
			}
		}

		if (i >= p_from_line) {
			l = indent.repeat(indent_stack.size()) + st;
		} else if (i > p_to_line) {
			break;
		}

		lines.write[i] = l;
	}

	p_code = "";
	for (int i = 0; i < lines.size(); i++) {
		if (i > 0) {
			p_code += "\n";
		}
		p_code += lines[i];
	}
}

#ifdef TOOLS_ENABLED

static Error _lookup_symbol_from_base(const GDScriptParser::DataType &p_base, const String &p_symbol, bool p_is_function, GDScriptLanguage::LookupResult &r_result) {
	GDScriptParser::DataType base_type = p_base;

	while (base_type.is_set()) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS: {
				if (base_type.class_type) {
					String name = p_symbol;
					if (name == "new") {
						name = "_init";
					}
					if (base_type.class_type->has_member(name)) {
						r_result.type = ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION;
						r_result.location = base_type.class_type->get_member(name).get_line();
						r_result.class_path = base_type.script_path;
						Error err = OK;
						r_result.script = GDScriptCache::get_shallow_script(r_result.class_path, err);
						return err;
					}
					base_type = base_type.class_type->base_type;
				}
			} break;
			case GDScriptParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					int line = scr->get_member_line(p_symbol);
					if (line >= 0) {
						r_result.type = ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION;
						r_result.location = line;
						r_result.script = scr;
						return OK;
					}
					Ref<Script> base_script = scr->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.builtin_type = Variant::OBJECT;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					base_type.kind = GDScriptParser::DataType::UNRESOLVED;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName class_name = base_type.native_type;
				if (!ClassDB::class_exists(class_name)) {
					base_type.kind = GDScriptParser::DataType::UNRESOLVED;
					break;
				}

				if (ClassDB::has_method(class_name, p_symbol, true)) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD;
					r_result.class_name = base_type.native_type;
					r_result.class_member = p_symbol;
					return OK;
				}

				List<MethodInfo> virtual_methods;
				ClassDB::get_virtual_methods(class_name, &virtual_methods, true);
				for (const MethodInfo &E : virtual_methods) {
					if (E.name == p_symbol) {
						r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD;
						r_result.class_name = base_type.native_type;
						r_result.class_member = p_symbol;
						return OK;
					}
				}

				if (ClassDB::has_signal(class_name, p_symbol, true)) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_SIGNAL;
					r_result.class_name = base_type.native_type;
					r_result.class_member = p_symbol;
					return OK;
				}

				List<StringName> enums;
				ClassDB::get_enum_list(class_name, &enums);
				for (const StringName &E : enums) {
					if (E == p_symbol) {
						r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_ENUM;
						r_result.class_name = base_type.native_type;
						r_result.class_member = p_symbol;
						return OK;
					}
				}

				if (!String(ClassDB::get_integer_constant_enum(class_name, p_symbol, true)).is_empty()) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
					r_result.class_name = base_type.native_type;
					r_result.class_member = p_symbol;
					return OK;
				}

				List<String> constants;
				ClassDB::get_integer_constant_list(class_name, &constants, true);
				for (const String &E : constants) {
					if (E == p_symbol) {
						r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
						r_result.class_name = base_type.native_type;
						r_result.class_member = p_symbol;
						return OK;
					}
				}

				if (ClassDB::has_property(class_name, p_symbol, true)) {
					PropertyInfo prop_info;
					ClassDB::get_property_info(class_name, p_symbol, &prop_info, true);
					if (prop_info.usage & PROPERTY_USAGE_INTERNAL) {
						return ERR_CANT_RESOLVE;
					}

					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_PROPERTY;
					r_result.class_name = base_type.native_type;
					r_result.class_member = p_symbol;
					return OK;
				}

				StringName parent = ClassDB::get_parent_class(class_name);
				if (parent != StringName()) {
					base_type.native_type = parent;
				} else {
					base_type.kind = GDScriptParser::DataType::UNRESOLVED;
				}
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				base_type.kind = GDScriptParser::DataType::UNRESOLVED;

				if (Variant::has_constant(base_type.builtin_type, p_symbol)) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
					r_result.class_name = Variant::get_type_name(base_type.builtin_type);
					r_result.class_member = p_symbol;
					return OK;
				}

				Variant v;
				Ref<RefCounted> v_ref;
				if (base_type.builtin_type == Variant::OBJECT) {
					v_ref.instantiate();
					v = v_ref;
				} else {
					Callable::CallError err;
					Variant::construct(base_type.builtin_type, v, nullptr, 0, err);
					if (err.error != Callable::CallError::CALL_OK) {
						break;
					}
				}

				if (v.has_method(p_symbol)) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD;
					r_result.class_name = Variant::get_type_name(base_type.builtin_type);
					r_result.class_member = p_symbol;
					return OK;
				}

				bool valid = false;
				v.get(p_symbol, &valid);
				if (valid) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_PROPERTY;
					r_result.class_name = Variant::get_type_name(base_type.builtin_type);
					r_result.class_member = p_symbol;
					return OK;
				}
			} break;
			case GDScriptParser::DataType::ENUM: {
				if (base_type.class_type && base_type.class_type->has_member(base_type.enum_type)) {
					GDScriptParser::EnumNode *base_enum = base_type.class_type->get_member(base_type.enum_type).m_enum;
					for (const GDScriptParser::EnumNode::Value &value : base_enum->values) {
						if (value.identifier && value.identifier->name == p_symbol) {
							r_result.type = ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION;
							r_result.class_path = base_type.script_path;
							r_result.location = value.line;
							Error err = OK;
							r_result.script = GDScriptCache::get_shallow_script(r_result.class_path, err);
							return err;
						}
					}
				} else if (base_type.enum_values.has(p_symbol)) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
					r_result.class_name = String(base_type.native_type).get_slicec('.', 0);
					r_result.class_member = p_symbol;
					return OK;
				}
				base_type.kind = GDScriptParser::DataType::UNRESOLVED;
			} break;
			default: {
				base_type.kind = GDScriptParser::DataType::UNRESOLVED;
			} break;
		}
	}

	return ERR_CANT_RESOLVE;
}

::Error GDScriptLanguage::lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner, LookupResult &r_result) {
	// Before parsing, try the usual stuff.
	if (ClassDB::class_exists(p_symbol)) {
		r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS;
		r_result.class_name = p_symbol;
		return OK;
	}

	if (Variant::get_type_by_name(p_symbol) < Variant::VARIANT_MAX) {
		r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS;
		r_result.class_name = p_symbol;
		return OK;
	}

	if ("Variant" == p_symbol) {
		r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS;
		r_result.class_name = "Variant";
		return OK;
	}

	if ("PI" == p_symbol || "TAU" == p_symbol || "INF" == p_symbol || "NAN" == p_symbol) {
		r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
		r_result.class_name = "@GDScript";
		r_result.class_member = p_symbol;
		return OK;
	}

	GDScriptParser parser;
	parser.parse(p_code, p_path, true);

	GDScriptParser::CompletionContext context = parser.get_completion_context();
	context.base = p_owner;

	// Allows class functions with the names like built-ins to be handled properly.
	if (context.type != GDScriptParser::COMPLETION_ATTRIBUTE) {
		// Need special checks for assert and preload as they are technically
		// keywords, so are not registered in GDScriptUtilityFunctions.
		if (GDScriptUtilityFunctions::function_exists(p_symbol) || "assert" == p_symbol || "preload" == p_symbol) {
			r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD;
			r_result.class_name = "@GDScript";
			r_result.class_member = p_symbol;
			return OK;
		}
	}

	GDScriptAnalyzer analyzer(&parser);
	analyzer.analyze();

	if (context.current_class && context.current_class->extends.size() > 0) {
		StringName class_name = context.current_class->extends[0]->name;

		bool success = false;
		ClassDB::get_integer_constant(class_name, p_symbol, &success);
		if (success) {
			r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
			r_result.class_name = class_name;
			r_result.class_member = p_symbol;
			return OK;
		}
		do {
			List<StringName> enums;
			ClassDB::get_enum_list(class_name, &enums, true);
			for (const StringName &enum_name : enums) {
				if (enum_name == p_symbol) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_ENUM;
					r_result.class_name = class_name;
					r_result.class_member = p_symbol;
					return OK;
				}
			}
			class_name = ClassDB::get_parent_class_nocheck(class_name);
		} while (class_name != StringName());
	}

	const GDScriptParser::TypeNode *type_node = dynamic_cast<const GDScriptParser::TypeNode *>(context.node);
	if (type_node != nullptr && !type_node->type_chain.is_empty()) {
		StringName class_name = type_node->type_chain[0]->name;
		if (ScriptServer::is_global_class(class_name)) {
			class_name = ScriptServer::get_global_class_native_base(class_name);
		}
		do {
			List<StringName> enums;
			ClassDB::get_enum_list(class_name, &enums, true);
			for (const StringName &enum_name : enums) {
				if (enum_name == p_symbol) {
					r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_ENUM;
					r_result.class_name = class_name;
					r_result.class_member = p_symbol;
					return OK;
				}
			}
			class_name = ClassDB::get_parent_class_nocheck(class_name);
		} while (class_name != StringName());
	}

	bool is_function = false;

	switch (context.type) {
		case GDScriptParser::COMPLETION_BUILT_IN_TYPE_CONSTANT_OR_STATIC_METHOD: {
			if (!Variant::has_builtin_method(context.builtin_type, StringName(p_symbol))) {
				// A constant.
				r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
				r_result.class_name = Variant::get_type_name(context.builtin_type);
				r_result.class_member = p_symbol;
				return OK;
			}
			// A method.
			GDScriptParser::DataType base_type;
			base_type.kind = GDScriptParser::DataType::BUILTIN;
			base_type.builtin_type = context.builtin_type;
			if (_lookup_symbol_from_base(base_type, p_symbol, true, r_result) == OK) {
				return OK;
			}
		} break;
		case GDScriptParser::COMPLETION_SUPER_METHOD:
		case GDScriptParser::COMPLETION_METHOD: {
			is_function = true;
			[[fallthrough]];
		}
		case GDScriptParser::COMPLETION_ASSIGN:
		case GDScriptParser::COMPLETION_CALL_ARGUMENTS:
		case GDScriptParser::COMPLETION_IDENTIFIER:
		case GDScriptParser::COMPLETION_PROPERTY_METHOD:
		case GDScriptParser::COMPLETION_SUBSCRIPT: {
			GDScriptParser::DataType base_type;
			if (context.current_class) {
				if (context.type != GDScriptParser::COMPLETION_SUPER_METHOD) {
					base_type = context.current_class->get_datatype();
				} else {
					base_type = context.current_class->base_type;
				}
			} else {
				break;
			}

			if (!is_function && context.current_suite) {
				// Lookup local variables.
				const GDScriptParser::SuiteNode *suite = context.current_suite;
				while (suite) {
					if (suite->has_local(p_symbol)) {
						r_result.type = ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION;
						r_result.location = suite->get_local(p_symbol).start_line;
						return OK;
					}
					suite = suite->parent_block;
				}
			}

			if (_lookup_symbol_from_base(base_type, p_symbol, is_function, r_result) == OK) {
				return OK;
			}

			if (!is_function) {
				// Guess in autoloads as singletons.
				if (ProjectSettings::get_singleton()->has_autoload(p_symbol)) {
					const ProjectSettings::AutoloadInfo &autoload = ProjectSettings::get_singleton()->get_autoload(p_symbol);
					if (autoload.is_singleton) {
						String scr_path = autoload.path;
						if (!scr_path.ends_with(".gd")) {
							// Not a script, try find the script anyway,
							// may have some success.
							scr_path = scr_path.get_basename() + ".gd";
						}

						if (FileAccess::exists(scr_path)) {
							r_result.type = ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION;
							r_result.location = 0;
							r_result.script = ResourceLoader::load(scr_path);
							return OK;
						}
					}
				}

				// Global.
				HashMap<StringName, int> classes = GDScriptLanguage::get_singleton()->get_global_map();
				if (classes.has(p_symbol)) {
					Variant value = GDScriptLanguage::get_singleton()->get_global_array()[classes[p_symbol]];
					if (value.get_type() == Variant::OBJECT) {
						Object *obj = value;
						if (obj) {
							if (Object::cast_to<GDScriptNativeClass>(obj)) {
								r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS;
								r_result.class_name = Object::cast_to<GDScriptNativeClass>(obj)->get_name();
							} else {
								r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS;
								r_result.class_name = obj->get_class();
							}

							// proxy class remove the underscore.
							if (r_result.class_name.begins_with("_")) {
								r_result.class_name = r_result.class_name.substr(1);
							}
							return OK;
						}
					} else {
						/*
						// Because get_integer_constant_enum and get_integer_constant don't work on @GlobalScope
						// We cannot determine the exact nature of the identifier here
						// Otherwise these codes would work
						StringName enumName = ClassDB::get_integer_constant_enum("@GlobalScope", p_symbol, true);
						if (enumName != nullptr) {
							r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_ENUM;
							r_result.class_name = "@GlobalScope";
							r_result.class_member = enumName;
							return OK;
						}
						else {
							r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
							r_result.class_name = "@GlobalScope";
							r_result.class_member = p_symbol;
							return OK;
						}*/
						r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_TBD_GLOBALSCOPE;
						r_result.class_name = "@GlobalScope";
						r_result.class_member = p_symbol;
						return OK;
					}
				} else {
					List<StringName> utility_functions;
					Variant::get_utility_function_list(&utility_functions);
					if (utility_functions.find(p_symbol) != nullptr) {
						r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_TBD_GLOBALSCOPE;
						r_result.class_name = "@GlobalScope";
						r_result.class_member = p_symbol;
						return OK;
					}
				}
			}
		} break;
		case GDScriptParser::COMPLETION_ATTRIBUTE_METHOD: {
			is_function = true;
			[[fallthrough]];
		}
		case GDScriptParser::COMPLETION_ATTRIBUTE: {
			if (context.node->type != GDScriptParser::Node::SUBSCRIPT) {
				break;
			}
			const GDScriptParser::SubscriptNode *subscript = static_cast<const GDScriptParser::SubscriptNode *>(context.node);
			if (!subscript->is_attribute) {
				break;
			}
			GDScriptCompletionIdentifier base;

			bool found_type = _get_subscript_type(context, subscript, base.type);
			if (!found_type && !_guess_expression_type(context, subscript->base, base)) {
				break;
			}

			if (_lookup_symbol_from_base(base.type, p_symbol, is_function, r_result) == OK) {
				return OK;
			}
		} break;
		case GDScriptParser::COMPLETION_TYPE_ATTRIBUTE: {
			if (context.node == nullptr || context.node->type != GDScriptParser::Node::TYPE) {
				break;
			}
			const GDScriptParser::TypeNode *type = static_cast<const GDScriptParser::TypeNode *>(context.node);

			GDScriptParser::DataType base_type;
			const GDScriptParser::IdentifierNode *prev = nullptr;
			for (const GDScriptParser::IdentifierNode *E : type->type_chain) {
				if (E->name == p_symbol && prev != nullptr) {
					base_type = prev->get_datatype();
					break;
				}
				prev = E;
			}
			if (base_type.kind != GDScriptParser::DataType::CLASS) {
				GDScriptCompletionIdentifier base;
				if (!_guess_expression_type(context, prev, base)) {
					break;
				}
				base_type = base.type;
			}

			if (_lookup_symbol_from_base(base_type, p_symbol, is_function, r_result) == OK) {
				return OK;
			}
		} break;
		case GDScriptParser::COMPLETION_OVERRIDE_METHOD: {
			GDScriptParser::DataType base_type = context.current_class->base_type;

			if (_lookup_symbol_from_base(base_type, p_symbol, true, r_result) == OK) {
				return OK;
			}
		} break;
		case GDScriptParser::COMPLETION_PROPERTY_DECLARATION_OR_TYPE:
		case GDScriptParser::COMPLETION_TYPE_NAME_OR_VOID:
		case GDScriptParser::COMPLETION_TYPE_NAME: {
			GDScriptParser::DataType base_type = context.current_class->get_datatype();

			if (_lookup_symbol_from_base(base_type, p_symbol, false, r_result) == OK) {
				return OK;
			}
		} break;
		case GDScriptParser::COMPLETION_ANNOTATION: {
			const String annotation_symbol = "@" + p_symbol;
			if (parser.annotation_exists(annotation_symbol)) {
				r_result.type = ScriptLanguage::LOOKUP_RESULT_CLASS_ANNOTATION;
				r_result.class_name = "@GDScript";
				r_result.class_member = annotation_symbol;
				return OK;
			}
		} break;
		default: {
		}
	}

	return ERR_CANT_RESOLVE;
}

#endif // TOOLS_ENABLED
