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

#include "core/engine.h"
#include "core/global_constants.h"
#include "core/os/file_access.h"
#include "gdscript_compiler.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#include "editor/editor_settings.h"
#endif

void GDScriptLanguage::get_comment_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("#");
}

void GDScriptLanguage::get_string_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("\"\"\" \"\"\"");
	p_delimiters->push_back("\" \"");
	p_delimiters->push_back("' '");
}

String GDScriptLanguage::_get_processed_template(const String &p_template, const String &p_base_class_name) const {
	String processed_template = p_template;

#ifdef TOOLS_ENABLED
	if (EDITOR_DEF("text_editor/completion/add_type_hints", false)) {
		processed_template = processed_template.replace("%INT_TYPE%", ": int");
		processed_template = processed_template.replace("%STRING_TYPE%", ": String");
		processed_template = processed_template.replace("%FLOAT_TYPE%", ": float");
		processed_template = processed_template.replace("%VOID_RETURN%", " -> void");
	} else {
		processed_template = processed_template.replace("%INT_TYPE%", "");
		processed_template = processed_template.replace("%STRING_TYPE%", "");
		processed_template = processed_template.replace("%FLOAT_TYPE%", "");
		processed_template = processed_template.replace("%VOID_RETURN%", "");
	}
#else
	processed_template = processed_template.replace("%INT_TYPE%", "");
	processed_template = processed_template.replace("%STRING_TYPE%", "");
	processed_template = processed_template.replace("%FLOAT_TYPE%", "");
	processed_template = processed_template.replace("%VOID_RETURN%", "");
#endif

	processed_template = processed_template.replace("%BASE%", p_base_class_name);
	processed_template = processed_template.replace("%TS%", _get_indentation());
#ifdef TOOLS_ENABLED
	processed_template = processed_template.replace("%DATE%", get_current_system_date());
	processed_template = processed_template.replace("%USER%", get_current_app_user());
	processed_template = processed_template.replace("%COMPANY%", get_current_user_company());
	processed_template = processed_template.replace("%PROJECT%", get_current_project_name());
	processed_template = processed_template.replace("%VERSION%", get_current_project_version());
#endif
	return processed_template;
}

Ref<Script> GDScriptLanguage::get_template(const String &p_class_name, const String &p_base_class_name) const {
	String _template = "extends %BASE%\n"
					   "\n"
					   "\n"
					   "# Declare member variables here. Examples:\n"
					   "# var a%INT_TYPE% = 2\n"
					   "# var b%STRING_TYPE% = \"text\"\n"
					   "\n"
					   "\n"
					   "# Called when the node enters the scene tree for the first time.\n"
					   "func _ready()%VOID_RETURN%:\n"
					   "%TS%pass # Replace with function body.\n"
					   "\n"
					   "\n"
					   "# Called every frame. 'delta' is the elapsed time since the previous frame.\n"
					   "#func _process(delta%FLOAT_TYPE%)%VOID_RETURN%:\n"
					   "#%TS%pass\n";

	_template = _get_processed_template(_template, p_base_class_name);

	Ref<GDScript> script;
	script.instance();
	script->set_source_code(_template);

	return script;
}

bool GDScriptLanguage::is_using_templates() {
	return true;
}

void GDScriptLanguage::make_template(const String &p_class_name, const String &p_base_class_name, Ref<Script> &p_script) {
	String _template = _get_processed_template(p_script->get_source_code(), p_base_class_name);
	p_script->set_source_code(_template);
}

bool GDScriptLanguage::validate(const String &p_script, int &r_line_error, int &r_col_error, String &r_test_error, const String &p_path, List<String> *r_functions, List<ScriptLanguage::Warning> *r_warnings, Set<int> *r_safe_lines) const {
	GDScriptParser parser;

	Error err = parser.parse(p_script, p_path.get_base_dir(), true, p_path, false, r_safe_lines);
#ifdef DEBUG_ENABLED
	if (r_warnings) {
		for (const List<GDScriptWarning>::Element *E = parser.get_warnings().front(); E; E = E->next()) {
			const GDScriptWarning &warn = E->get();
			ScriptLanguage::Warning w;
			w.line = warn.line;
			w.code = (int)warn.code;
			w.string_code = GDScriptWarning::get_name_from_code(warn.code);
			w.message = warn.get_message();
			r_warnings->push_back(w);
		}
	}
#endif
	if (err) {
		r_line_error = parser.get_error_line();
		r_col_error = parser.get_error_column();
		r_test_error = parser.get_error();
		return false;
	} else {
		const GDScriptParser::Node *root = parser.get_parse_tree();
		ERR_FAIL_COND_V(root->type != GDScriptParser::Node::TYPE_CLASS, false);

		const GDScriptParser::ClassNode *cl = static_cast<const GDScriptParser::ClassNode *>(root);
		Map<int, String> funcs;
		for (int i = 0; i < cl->functions.size(); i++) {
			funcs[cl->functions[i]->line] = cl->functions[i]->name;
		}

		for (int i = 0; i < cl->static_functions.size(); i++) {
			funcs[cl->static_functions[i]->line] = cl->static_functions[i]->name;
		}

		for (int i = 0; i < cl->subclasses.size(); i++) {
			for (int j = 0; j < cl->subclasses[i]->functions.size(); j++) {
				funcs[cl->subclasses[i]->functions[j]->line] = String(cl->subclasses[i]->name) + "." + cl->subclasses[i]->functions[j]->name;
			}
			for (int j = 0; j < cl->subclasses[i]->static_functions.size(); j++) {
				funcs[cl->subclasses[i]->static_functions[j]->line] = String(cl->subclasses[i]->name) + "." + cl->subclasses[i]->static_functions[j]->name;
			}
		}

		for (Map<int, String>::Element *E = funcs.front(); E; E = E->next()) {
			r_functions->push_back(E->get() + ":" + itos(E->key()));
		}
	}

	return true;
}

bool GDScriptLanguage::has_named_classes() const {
	return false;
}

bool GDScriptLanguage::supports_builtin_mode() const {
	return true;
}

int GDScriptLanguage::find_function(const String &p_function, const String &p_code) const {
	GDScriptTokenizerText tokenizer;
	tokenizer.set_code(p_code);
	int indent = 0;
	while (tokenizer.get_token() != GDScriptTokenizer::TK_EOF && tokenizer.get_token() != GDScriptTokenizer::TK_ERROR) {
		if (tokenizer.get_token() == GDScriptTokenizer::TK_NEWLINE) {
			indent = tokenizer.get_token_line_indent();
		}
		if (indent == 0 && tokenizer.get_token() == GDScriptTokenizer::TK_PR_FUNCTION && tokenizer.get_token(1) == GDScriptTokenizer::TK_IDENTIFIER) {
			String identifier = tokenizer.get_token_identifier(1);
			if (identifier == p_function) {
				return tokenizer.get_token_line();
			}
		}
		tokenizer.advance();
	}
	return -1;
}

Script *GDScriptLanguage::create_script() const {
	return memnew(GDScript);
}

/* DEBUGGER FUNCTIONS */

bool GDScriptLanguage::debug_break_parse(const String &p_file, int p_line, const String &p_error) {
	//break because of parse error

	if (ScriptDebugger::get_singleton() && Thread::get_caller_id() == Thread::get_main_id()) {
		_debug_parse_err_line = p_line;
		_debug_parse_err_file = p_file;
		_debug_error = p_error;
		ScriptDebugger::get_singleton()->debug(this, false, true);
		return true;
	} else {
		return false;
	}
}

bool GDScriptLanguage::debug_break(const String &p_error, bool p_allow_continue) {
	if (ScriptDebugger::get_singleton() && Thread::get_caller_id() == Thread::get_main_id()) {
		_debug_parse_err_line = -1;
		_debug_parse_err_file = "";
		_debug_error = p_error;
		bool is_error_breakpoint = p_error != "Breakpoint";
		ScriptDebugger::get_singleton()->debug(this, p_allow_continue, is_error_breakpoint);
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

	return _debug_call_stack_pos;
}
int GDScriptLanguage::debug_get_stack_level_line(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return _debug_parse_err_line;
	}

	ERR_FAIL_INDEX_V(p_level, _debug_call_stack_pos, -1);

	int l = _debug_call_stack_pos - p_level - 1;

	return *(_call_stack[l].line);
}
String GDScriptLanguage::debug_get_stack_level_function(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return "";
	}

	ERR_FAIL_INDEX_V(p_level, _debug_call_stack_pos, "");
	int l = _debug_call_stack_pos - p_level - 1;
	return _call_stack[l].function->get_name();
}
String GDScriptLanguage::debug_get_stack_level_source(int p_level) const {
	if (_debug_parse_err_line >= 0) {
		return _debug_parse_err_file;
	}

	ERR_FAIL_INDEX_V(p_level, _debug_call_stack_pos, "");
	int l = _debug_call_stack_pos - p_level - 1;
	return _call_stack[l].function->get_source();
}
void GDScriptLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	if (_debug_parse_err_line >= 0) {
		return;
	}

	ERR_FAIL_INDEX(p_level, _debug_call_stack_pos);
	int l = _debug_call_stack_pos - p_level - 1;

	GDScriptFunction *f = _call_stack[l].function;

	List<Pair<StringName, int>> locals;

	f->debug_get_stack_member_state(*_call_stack[l].line, &locals);
	for (List<Pair<StringName, int>>::Element *E = locals.front(); E; E = E->next()) {
		p_locals->push_back(E->get().first);
		p_values->push_back(_call_stack[l].stack[E->get().second]);
	}
}
void GDScriptLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	if (_debug_parse_err_line >= 0) {
		return;
	}

	ERR_FAIL_INDEX(p_level, _debug_call_stack_pos);
	int l = _debug_call_stack_pos - p_level - 1;

	GDScriptInstance *instance = _call_stack[l].instance;

	if (!instance) {
		return;
	}

	Ref<GDScript> script = instance->get_script();
	ERR_FAIL_COND(script.is_null());

	const Map<StringName, GDScript::MemberInfo> &mi = script->debug_get_member_indices();

	for (const Map<StringName, GDScript::MemberInfo>::Element *E = mi.front(); E; E = E->next()) {
		p_members->push_back(E->key());
		p_values->push_back(instance->debug_get_member_by_index(E->get().index));
	}
}

ScriptInstance *GDScriptLanguage::debug_get_stack_level_instance(int p_level) {
	if (_debug_parse_err_line >= 0) {
		return nullptr;
	}

	ERR_FAIL_INDEX_V(p_level, _debug_call_stack_pos, nullptr);

	int l = _debug_call_stack_pos - p_level - 1;
	ScriptInstance *instance = _call_stack[l].instance;

	return instance;
}

void GDScriptLanguage::debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	const Map<StringName, int> &name_idx = GDScriptLanguage::get_singleton()->get_global_map();
	const Variant *globals = GDScriptLanguage::get_singleton()->get_global_array();

	List<Pair<String, Variant>> cinfo;
	get_public_constants(&cinfo);

	for (const Map<StringName, int>::Element *E = name_idx.front(); E; E = E->next()) {
		if (ClassDB::class_exists(E->key()) || Engine::get_singleton()->has_singleton(E->key())) {
			continue;
		}

		bool is_script_constant = false;
		for (List<Pair<String, Variant>>::Element *CE = cinfo.front(); CE; CE = CE->next()) {
			if (CE->get().first == E->key()) {
				is_script_constant = true;
				break;
			}
		}
		if (is_script_constant) {
			continue;
		}

		const Variant &var = globals[E->value()];
		if (Object *obj = var) {
			if (Object::cast_to<GDScriptNativeClass>(obj)) {
				continue;
			}
		}

		bool skip = false;
		for (int i = 0; i < GlobalConstants::get_global_constant_count(); i++) {
			if (E->key() == GlobalConstants::get_global_constant_name(i)) {
				skip = true;
				break;
			}
		}
		if (skip) {
			continue;
		}

		p_globals->push_back(E->key());
		p_values->push_back(var);
	}
}

String GDScriptLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) {
	return "";
}

void GDScriptLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gd");
}

void GDScriptLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
	for (int i = 0; i < GDScriptFunctions::FUNC_MAX; i++) {
		p_functions->push_back(GDScriptFunctions::get_info(GDScriptFunctions::Function(i)));
	}

	//not really "functions", but..
	{
		MethodInfo mi;
		mi.name = "preload";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "path"));
		mi.return_val = PropertyInfo(Variant::OBJECT, "", PROPERTY_HINT_RESOURCE_TYPE, "Resource");
		p_functions->push_back(mi);
	}
	{
		MethodInfo mi;
		mi.name = "yield";
		mi.arguments.push_back(PropertyInfo(Variant::OBJECT, "object"));
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "signal"));
		mi.default_arguments.push_back(Variant());
		mi.default_arguments.push_back(String());
		mi.return_val = PropertyInfo(Variant::OBJECT, "", PROPERTY_HINT_RESOURCE_TYPE, "GDScriptFunctionState");
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
	infinity.second = Math_INF;
	p_constants->push_back(infinity);

	Pair<String, Variant> nan;
	nan.first = "NAN";
	nan.second = Math_NAN;
	p_constants->push_back(nan);
}

String GDScriptLanguage::make_function(const String &p_class, const String &p_name, const PoolStringArray &p_args) const {
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
				if (!type.empty() && type != "var") {
					s += ": " + type;
				}
			}
		}
	}
	s += String(")") + (th ? " -> void" : "") + ":\n" + _get_indentation() + "pass # Replace with function body.\n";

	return s;
}

//////// COMPLETION //////////

#if defined(DEBUG_METHODS_ENABLED) && defined(TOOLS_ENABLED)

struct GDScriptCompletionContext {
	const GDScriptParser::ClassNode *_class;
	const GDScriptParser::FunctionNode *function;
	const GDScriptParser::BlockNode *block;
	Object *base;
	String base_path;
	int line;
	uint32_t depth;

	GDScriptCompletionContext() :
			_class(nullptr),
			function(nullptr),
			block(nullptr),
			base(nullptr),
			line(0),
			depth(0) {}
};

struct GDScriptCompletionIdentifier {
	GDScriptParser::DataType type;
	String enumeration;
	Variant value;
	const GDScriptParser::Node *assigned_expression;

	GDScriptCompletionIdentifier() :
			assigned_expression(nullptr) {}
};

static void _get_directory_contents(EditorFileSystemDirectory *p_dir, Map<String, ScriptCodeCompletionOption> &r_list, String p_ends_with = "") {
	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		ScriptCodeCompletionOption option(p_dir->get_file_path(i), ScriptCodeCompletionOption::KIND_FILE_PATH);
		option.insert_text = quote_style + option.display + quote_style;
		if (!p_ends_with.empty()) {
			if (option.display.ends_with(p_ends_with)) {
				r_list.insert(option.display, option);
			}
		} else {
			r_list.insert(option.display, option);
		}
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_get_directory_contents(p_dir->get_subdir(i), r_list, p_ends_with);
	}
}

static String _get_visual_datatype(const PropertyInfo &p_info, bool p_isarg = true) {
	if (p_info.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
		String enum_name = p_info.class_name;
		if (enum_name.find(".") == -1) {
			return enum_name;
		}
		return enum_name.get_slice(".", 1);
	}

	String n = p_info.name;
	int idx = n.find(":");
	if (idx != -1) {
		return n.substr(idx + 1, n.length());
	}

	if (p_info.type == Variant::OBJECT) {
		if (p_info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
			return p_info.hint_string;
		} else {
			return p_info.class_name.operator String();
		}
	}
	if (p_info.type == Variant::NIL) {
		if (p_isarg || (p_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT)) {
			return "Variant";
		} else {
			return "void";
		}
	}

	return Variant::get_type_name(p_info.type);
}

static GDScriptCompletionIdentifier _type_from_variant(const Variant &p_value) {
	GDScriptCompletionIdentifier ci;
	ci.value = p_value;
	ci.type.is_constant = true;
	ci.type.has_type = true;
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
			ci.type.script_type = scr;
			Ref<GDScript> gds = scr;
			if (gds.is_valid()) {
				ci.type.kind = GDScriptParser::DataType::GDSCRIPT;
			} else {
				ci.type.kind = GDScriptParser::DataType::SCRIPT;
			}
			ci.type.native_type = scr->get_instance_base_type();
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

	if (p_property.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
		ci.enumeration = p_property.class_name;
	}

	ci.type.has_type = true;
	ci.type.builtin_type = p_property.type;
	if (p_property.type == Variant::OBJECT) {
		ci.type.kind = GDScriptParser::DataType::NATIVE;
		ci.type.native_type = p_property.class_name == StringName() ? "Object" : p_property.class_name;
	} else {
		ci.type.kind = GDScriptParser::DataType::BUILTIN;
	}
	return ci;
}

static GDScriptCompletionIdentifier _type_from_gdtype(const GDScriptDataType &p_gdtype) {
	GDScriptCompletionIdentifier ci;
	if (!p_gdtype.has_type) {
		return ci;
	}

	ci.type.has_type = true;
	ci.type.builtin_type = p_gdtype.builtin_type;
	ci.type.native_type = p_gdtype.native_type;
	ci.type.script_type = Ref<Script>(p_gdtype.script_type);

	switch (p_gdtype.kind) {
		case GDScriptDataType::UNINITIALIZED: {
			ERR_PRINT("Uninitialized completion. Please report a bug.");
		} break;
		case GDScriptDataType::BUILTIN: {
			ci.type.kind = GDScriptParser::DataType::BUILTIN;
		} break;
		case GDScriptDataType::NATIVE: {
			ci.type.kind = GDScriptParser::DataType::NATIVE;
		} break;
		case GDScriptDataType::GDSCRIPT: {
			ci.type.kind = GDScriptParser::DataType::GDSCRIPT;
		} break;
		case GDScriptDataType::SCRIPT: {
			ci.type.kind = GDScriptParser::DataType::SCRIPT;
		} break;
	}
	return ci;
}

static bool _guess_identifier_type(GDScriptCompletionContext &p_context, const StringName &p_identifier, GDScriptCompletionIdentifier &r_type);
static bool _guess_identifier_type_from_base(GDScriptCompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_identifier, GDScriptCompletionIdentifier &r_type);
static bool _guess_method_return_type_from_base(GDScriptCompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_method, GDScriptCompletionIdentifier &r_type);

static bool _guess_expression_type(GDScriptCompletionContext &p_context, const GDScriptParser::Node *p_expression, GDScriptCompletionIdentifier &r_type) {
	bool found = false;

	if (++p_context.depth > 100) {
		print_error("Maximum _guess_expression_type depth limit reached. Please file a bugreport.");
		return false;
	}

	switch (p_expression->type) {
		case GDScriptParser::Node::TYPE_CONSTANT: {
			const GDScriptParser::ConstantNode *cn = static_cast<const GDScriptParser::ConstantNode *>(p_expression);
			r_type = _type_from_variant(cn->value);
			found = true;
		} break;
		case GDScriptParser::Node::TYPE_SELF: {
			if (p_context._class) {
				r_type.type.has_type = true;
				r_type.type.kind = GDScriptParser::DataType::CLASS;
				r_type.type.class_type = const_cast<GDScriptParser::ClassNode *>(p_context._class);
				r_type.type.is_constant = true;
				r_type.value = p_context.base;
				found = true;
			}
		} break;
		case GDScriptParser::Node::TYPE_IDENTIFIER: {
			const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(p_expression);
			found = _guess_identifier_type(p_context, id->name, r_type);
		} break;
		case GDScriptParser::Node::TYPE_DICTIONARY: {
			// Try to recreate the dictionary
			const GDScriptParser::DictionaryNode *dn = static_cast<const GDScriptParser::DictionaryNode *>(p_expression);
			Dictionary d;
			bool full = true;
			for (int i = 0; i < dn->elements.size(); i++) {
				GDScriptCompletionIdentifier key;
				if (_guess_expression_type(p_context, dn->elements[i].key, key)) {
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
				// If not fully constant, setting this value is detrimental to the inference
				r_type.value = d;
				r_type.type.is_constant = true;
			}
			r_type.type.has_type = true;
			r_type.type.kind = GDScriptParser::DataType::BUILTIN;
			r_type.type.builtin_type = Variant::DICTIONARY;
		} break;
		case GDScriptParser::Node::TYPE_ARRAY: {
			// Try to recreate the array
			const GDScriptParser::ArrayNode *an = static_cast<const GDScriptParser::ArrayNode *>(p_expression);
			Array a;
			bool full = true;
			a.resize(an->elements.size());
			for (int i = 0; i < an->elements.size(); i++) {
				GDScriptCompletionIdentifier value;
				if (_guess_expression_type(p_context, an->elements[i], value)) {
					a[i] = value.value;
				} else {
					full = false;
					break;
				}
			}
			if (full) {
				// If not fully constant, setting this value is detrimental to the inference
				r_type.value = a;
			}
			r_type.type.has_type = true;
			r_type.type.kind = GDScriptParser::DataType::BUILTIN;
			r_type.type.builtin_type = Variant::ARRAY;
		} break;
		case GDScriptParser::Node::TYPE_CAST: {
			const GDScriptParser::CastNode *cn = static_cast<const GDScriptParser::CastNode *>(p_expression);
			GDScriptCompletionIdentifier value;
			if (_guess_expression_type(p_context, cn->source_node, r_type)) {
				r_type.type = cn->get_datatype();
				found = true;
			}
		} break;
		case GDScriptParser::Node::TYPE_OPERATOR: {
			const GDScriptParser::OperatorNode *op = static_cast<const GDScriptParser::OperatorNode *>(p_expression);
			switch (op->op) {
				case GDScriptParser::OperatorNode::OP_CALL: {
					if (op->arguments[0]->type == GDScriptParser::Node::TYPE_TYPE) {
						const GDScriptParser::TypeNode *tn = static_cast<const GDScriptParser::TypeNode *>(op->arguments[0]);
						r_type.type.has_type = true;
						r_type.type.kind = GDScriptParser::DataType::BUILTIN;
						r_type.type.builtin_type = tn->vtype;
						found = true;
						break;
					} else if (op->arguments[0]->type == GDScriptParser::Node::TYPE_BUILT_IN_FUNCTION) {
						const GDScriptParser::BuiltInFunctionNode *bin = static_cast<const GDScriptParser::BuiltInFunctionNode *>(op->arguments[0]);
						MethodInfo mi = GDScriptFunctions::get_info(bin->function);
						r_type = _type_from_property(mi.return_val);
						found = true;
						break;
					} else if (op->arguments.size() >= 2 && op->arguments[1]->type == GDScriptParser::Node::TYPE_IDENTIFIER) {
						StringName id = static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[1])->name;

						GDScriptCompletionContext c = p_context;
						c.line = op->line;

						GDScriptCompletionIdentifier base;
						if (!_guess_expression_type(c, op->arguments[0], base)) {
							found = false;
							break;
						}

						// Try call if constant methods with constant arguments
						if (base.type.is_constant && base.value.get_type() == Variant::OBJECT) {
							GDScriptParser::DataType native_type = base.type;

							while (native_type.kind == GDScriptParser::DataType::CLASS) {
								native_type = native_type.class_type->base_type;
							}

							while (native_type.kind == GDScriptParser::DataType::GDSCRIPT || native_type.kind == GDScriptParser::DataType::SCRIPT) {
								if (native_type.script_type.is_valid()) {
									Ref<Script> parent = native_type.script_type->get_base_script();
									if (parent.is_valid()) {
										native_type.script_type = parent;
									} else {
										native_type.kind = GDScriptParser::DataType::NATIVE;
										native_type.native_type = native_type.script_type->get_instance_base_type();
										if (!ClassDB::class_exists(native_type.native_type)) {
											native_type.native_type = String("_") + native_type.native_type;
											if (!ClassDB::class_exists(native_type.native_type)) {
												native_type.has_type = false;
											}
										}
									}
								}
							}

							if (native_type.has_type && native_type.kind == GDScriptParser::DataType::NATIVE) {
								MethodBind *mb = ClassDB::get_method(native_type.native_type, id);
								if (mb && mb->is_const()) {
									bool all_is_const = true;
									Vector<Variant> args;
									GDScriptCompletionContext c2 = p_context;
									c2.line = op->line;
									for (int i = 2; all_is_const && i < op->arguments.size(); i++) {
										GDScriptCompletionIdentifier arg;

										if (_guess_expression_type(c2, op->arguments[i], arg)) {
											if (arg.type.has_type && arg.type.is_constant && arg.value.get_type() != Variant::OBJECT) {
												args.push_back(arg.value);
											} else {
												all_is_const = false;
											}
										} else {
											all_is_const = false;
										}
									}

									Object *baseptr = base.value;

									if (all_is_const && String(id) == "get_node" && ClassDB::is_parent_class(native_type.native_type, "Node") && args.size()) {
										String arg1 = args[0];
										if (arg1.begins_with("/root/")) {
											String which = arg1.get_slice("/", 2);
											if (which != "") {
												// Try singletons first
												if (GDScriptLanguage::get_singleton()->get_named_globals_map().has(which)) {
													r_type = _type_from_variant(GDScriptLanguage::get_singleton()->get_named_globals_map()[which]);
													found = true;
												} else {
													List<PropertyInfo> props;
													ProjectSettings::get_singleton()->get_property_list(&props);

													for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
														String s = E->get().name;
														if (!s.begins_with("autoload/")) {
															continue;
														}
														String name = s.get_slice("/", 1);
														if (name == which) {
															String script = ProjectSettings::get_singleton()->get(s);

															if (script.begins_with("*")) {
																script = script.right(1);
															}

															if (!script.begins_with("res://")) {
																script = "res://" + script;
															}

															if (!script.ends_with(".gd")) {
																//not a script, try find the script anyway,
																//may have some success
																script = script.get_basename() + ".gd";
															}

															if (FileAccess::exists(script)) {
																Ref<Script> scr;
																if (ScriptCodeCompletionCache::get_singleton()) {
																	scr = ScriptCodeCompletionCache::get_singleton()->get_cached_resource(script);
																} else {
																	scr = ResourceLoader::load(script);
																}
																if (scr.is_valid()) {
																	r_type.type.has_type = true;
																	r_type.type.script_type = scr;
																	r_type.type.is_constant = false;
																	Ref<GDScript> gds = scr;
																	if (gds.is_valid()) {
																		r_type.type.kind = GDScriptParser::DataType::GDSCRIPT;
																	} else {
																		r_type.type.kind = GDScriptParser::DataType::SCRIPT;
																	}
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

										Variant::CallError ce;
										Variant ret = mb->call(baseptr, (const Variant **)argptr.ptr(), argptr.size(), ce);

										if (ce.error == Variant::CallError::CALL_OK && ret.get_type() != Variant::NIL) {
											if (ret.get_type() != Variant::OBJECT || ret.operator Object *() != nullptr) {
												r_type = _type_from_variant(ret);
												found = true;
											}
										}
									}
								}
							}
						}

						if (!found) {
							found = _guess_method_return_type_from_base(c, base, id, r_type);
						}
					}
				} break;
				case GDScriptParser::OperatorNode::OP_PARENT_CALL: {
					if (!p_context._class || !op->arguments.size() || op->arguments[0]->type != GDScriptParser::Node::TYPE_IDENTIFIER) {
						break;
					}

					StringName id = static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[0])->name;

					GDScriptCompletionIdentifier base;
					base.value = p_context.base;
					base.type = p_context._class->base_type;

					GDScriptCompletionContext c = p_context;
					c.line = op->line;

					found = _guess_method_return_type_from_base(c, base, id, r_type);
				} break;
				case GDScriptParser::OperatorNode::OP_INDEX_NAMED: {
					if (op->arguments.size() < 2 || op->arguments[1]->type != GDScriptParser::Node::TYPE_IDENTIFIER) {
						found = false;
						break;
					}
					const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[1]);

					GDScriptCompletionContext c = p_context;
					c.line = op->line;

					GDScriptCompletionIdentifier base;
					if (!_guess_expression_type(c, op->arguments[0], base)) {
						found = false;
						break;
					}

					if (base.value.get_type() == Variant::DICTIONARY && base.value.operator Dictionary().has(String(id->name))) {
						Variant value = base.value.operator Dictionary()[String(id->name)];
						r_type = _type_from_variant(value);
						found = true;
						break;
					}

					const GDScriptParser::DictionaryNode *dn = nullptr;
					if (op->arguments[0]->type == GDScriptParser::Node::TYPE_DICTIONARY) {
						dn = static_cast<const GDScriptParser::DictionaryNode *>(op->arguments[0]);
					} else if (base.assigned_expression && base.assigned_expression->type == GDScriptParser::Node::TYPE_DICTIONARY) {
						dn = static_cast<const GDScriptParser::DictionaryNode *>(base.assigned_expression);
					}

					if (dn) {
						for (int i = 0; i < dn->elements.size(); i++) {
							GDScriptCompletionIdentifier key;
							if (!_guess_expression_type(c, dn->elements[i].key, key)) {
								continue;
							}
							if (key.value == String(id->name)) {
								r_type.assigned_expression = dn->elements[i].value;
								found = _guess_expression_type(c, dn->elements[i].value, r_type);
								break;
							}
						}
					}

					if (!found) {
						found = _guess_identifier_type_from_base(c, base, id->name, r_type);
					}
				} break;
				case GDScriptParser::OperatorNode::OP_INDEX: {
					if (op->arguments.size() < 2) {
						found = false;
						break;
					}

					GDScriptCompletionContext c = p_context;
					c.line = op->line;

					GDScriptCompletionIdentifier base;
					if (!_guess_expression_type(c, op->arguments[0], base)) {
						found = false;
						break;
					}

					GDScriptCompletionIdentifier index;
					if (!_guess_expression_type(c, op->arguments[1], index)) {
						found = false;
						break;
					}

					if (base.value.in(index.value)) {
						Variant value = base.value.get(index.value);
						r_type = _type_from_variant(value);
						found = true;
						break;
					}

					// Look if it is a dictionary node
					const GDScriptParser::DictionaryNode *dn = nullptr;
					if (op->arguments[0]->type == GDScriptParser::Node::TYPE_DICTIONARY) {
						dn = static_cast<const GDScriptParser::DictionaryNode *>(op->arguments[0]);
					} else if (base.assigned_expression && base.assigned_expression->type == GDScriptParser::Node::TYPE_DICTIONARY) {
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

					// Look if it is an array node
					if (!found && index.value.is_num()) {
						int idx = index.value;
						const GDScriptParser::ArrayNode *an = nullptr;
						if (op->arguments[0]->type == GDScriptParser::Node::TYPE_ARRAY) {
							an = static_cast<const GDScriptParser::ArrayNode *>(op->arguments[0]);
						} else if (base.assigned_expression && base.assigned_expression->type == GDScriptParser::Node::TYPE_ARRAY) {
							an = static_cast<const GDScriptParser::ArrayNode *>(base.assigned_expression);
						}

						if (an && idx >= 0 && an->elements.size() > idx) {
							r_type.assigned_expression = an->elements[idx];
							found = _guess_expression_type(c, an->elements[idx], r_type);
							break;
						}
					}

					// Look for valid indexing in other types
					if (!found && (index.value.get_type() == Variant::STRING || index.value.get_type() == Variant::NODE_PATH)) {
						StringName id = index.value;
						found = _guess_identifier_type_from_base(c, base, id, r_type);
					} else if (!found && index.type.kind == GDScriptParser::DataType::BUILTIN) {
						Variant::CallError err;
						Variant base_val = Variant::construct(base.type.builtin_type, nullptr, 0, err);
						bool valid = false;
						Variant res = base_val.get(index.value, &valid);
						if (valid) {
							r_type = _type_from_variant(res);
							r_type.value = Variant();
							r_type.type.is_constant = false;
							found = true;
						}
					}
				} break;
				default: {
					if (op->arguments.size() < 2) {
						found = false;
						break;
					}

					Variant::Operator vop = Variant::OP_MAX;
					switch (op->op) {
						case GDScriptParser::OperatorNode::OP_ADD:
							vop = Variant::OP_ADD;
							break;
						case GDScriptParser::OperatorNode::OP_SUB:
							vop = Variant::OP_SUBTRACT;
							break;
						case GDScriptParser::OperatorNode::OP_MUL:
							vop = Variant::OP_MULTIPLY;
							break;
						case GDScriptParser::OperatorNode::OP_DIV:
							vop = Variant::OP_DIVIDE;
							break;
						case GDScriptParser::OperatorNode::OP_MOD:
							vop = Variant::OP_MODULE;
							break;
						case GDScriptParser::OperatorNode::OP_SHIFT_LEFT:
							vop = Variant::OP_SHIFT_LEFT;
							break;
						case GDScriptParser::OperatorNode::OP_SHIFT_RIGHT:
							vop = Variant::OP_SHIFT_RIGHT;
							break;
						case GDScriptParser::OperatorNode::OP_BIT_AND:
							vop = Variant::OP_BIT_AND;
							break;
						case GDScriptParser::OperatorNode::OP_BIT_OR:
							vop = Variant::OP_BIT_OR;
							break;
						case GDScriptParser::OperatorNode::OP_BIT_XOR:
							vop = Variant::OP_BIT_XOR;
							break;
						default: {
						}
					}

					if (vop == Variant::OP_MAX) {
						break;
					}

					GDScriptCompletionContext context = p_context;
					context.line = op->line;

					GDScriptCompletionIdentifier p1;
					GDScriptCompletionIdentifier p2;

					if (!_guess_expression_type(context, op->arguments[0], p1)) {
						found = false;
						break;
					}

					if (!_guess_expression_type(context, op->arguments[1], p2)) {
						found = false;
						break;
					}

					Variant::CallError ce;
					bool v1_use_value = p1.value.get_type() != Variant::NIL && p1.value.get_type() != Variant::OBJECT;
					Variant v1 = (v1_use_value) ? p1.value : Variant::construct(p1.type.builtin_type, nullptr, 0, ce);
					bool v2_use_value = p2.value.get_type() != Variant::NIL && p2.value.get_type() != Variant::OBJECT;
					Variant v2 = (v2_use_value) ? p2.value : Variant::construct(p2.type.builtin_type, nullptr, 0, ce);
					// avoid potential invalid ops
					if ((vop == Variant::OP_DIVIDE || vop == Variant::OP_MODULE) && v2.get_type() == Variant::INT) {
						v2 = 1;
						v2_use_value = false;
					}
					if (vop == Variant::OP_DIVIDE && v2.get_type() == Variant::REAL) {
						v2 = 1.0;
						v2_use_value = false;
					}

					Variant res;
					bool valid;
					Variant::evaluate(vop, v1, v2, res, valid);
					if (!valid) {
						found = false;
						break;
					}
					r_type = _type_from_variant(res);
					if (!v1_use_value || !v2_use_value) {
						r_type.value = Variant();
						r_type.type.is_constant = false;
					}

					found = true;
				} break;
			}
		} break;
		default: {
		}
	}

	// It may have found a null, but that's never useful
	if (found && r_type.type.has_type && r_type.type.kind == GDScriptParser::DataType::BUILTIN && r_type.type.builtin_type == Variant::NIL) {
		found = false;
	}

	// Check type hint last. For collections we want chance to get the actual value first
	// This way we can detect types from the content of dictionaries and arrays
	if (!found && p_expression->get_datatype().has_type) {
		r_type.type = p_expression->get_datatype();
		if (!r_type.assigned_expression) {
			r_type.assigned_expression = p_expression;
		}
		found = true;
	}

	return found;
}

static bool _guess_identifier_type(GDScriptCompletionContext &p_context, const StringName &p_identifier, GDScriptCompletionIdentifier &r_type) {
	// Look in blocks first
	const GDScriptParser::BlockNode *blk = p_context.block;
	int last_assign_line = -1;
	const GDScriptParser::Node *last_assigned_expression = nullptr;
	GDScriptParser::DataType var_type;
	while (blk) {
		if (blk->variables.has(p_identifier)) {
			if (blk->variables[p_identifier]->line > p_context.line) {
				return false;
			}

			var_type = blk->variables[p_identifier]->datatype;

			if (!last_assigned_expression && blk->variables[p_identifier]->assign && blk->variables[p_identifier]->assign->type == GDScriptParser::Node::TYPE_OPERATOR) {
				const GDScriptParser::OperatorNode *op = static_cast<const GDScriptParser::OperatorNode *>(blk->variables[p_identifier]->assign);
				if (op->op == GDScriptParser::OperatorNode::OP_ASSIGN && op->arguments.size() >= 2) {
					last_assign_line = op->line;
					last_assigned_expression = op->arguments[1];
				}
			}
		}

		for (int z = 0; z < blk->statements.size(); z++) {
			const GDScriptParser::Node *expr = blk->statements[z];
			if (expr->line > p_context.line || expr->type != GDScriptParser::Node::TYPE_OPERATOR) {
				continue;
			}

			const GDScriptParser::OperatorNode *op = static_cast<const GDScriptParser::OperatorNode *>(expr);
			if (op->op != GDScriptParser::OperatorNode::OP_ASSIGN || op->line < last_assign_line) {
				continue;
			}

			if (op->arguments.size() >= 2 && op->arguments[0]->type == GDScriptParser::Node::TYPE_IDENTIFIER) {
				const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[0]);
				if (id->name == p_identifier) {
					last_assign_line = op->line;
					last_assigned_expression = op->arguments[1];
				}
			}
		}

		if (blk->if_condition && blk->if_condition->type == GDScriptParser::Node::TYPE_OPERATOR && static_cast<const GDScriptParser::OperatorNode *>(blk->if_condition)->op == GDScriptParser::OperatorNode::OP_IS) {
			//is used, check if identifier is in there! this helps resolve in blocks that are (if (identifier is value)): which are very common..
			//super dirty hack, but very useful
			//credit: Zylann
			//TODO: this could be hacked to detect ANDed conditions too..
			const GDScriptParser::OperatorNode *op = static_cast<const GDScriptParser::OperatorNode *>(blk->if_condition);
			if (op->arguments[0]->type == GDScriptParser::Node::TYPE_IDENTIFIER && static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[0])->name == p_identifier) {
				//bingo
				GDScriptCompletionContext c = p_context;
				c.line = op->line;
				c.block = blk;
				if (_guess_expression_type(p_context, op->arguments[1], r_type)) {
					r_type.type.is_meta_type = false; // Right-hand of `is` will be a meta type, but the left-hand value is not
					// Not an assignment, it shouldn't carry any value
					r_type.value = Variant();
					r_type.assigned_expression = nullptr;

					return true;
				}
			}
		}

		blk = blk->parent_block;
	}

	if (last_assigned_expression && last_assign_line != p_context.line) {
		GDScriptCompletionContext c = p_context;
		c.line = last_assign_line;
		r_type.assigned_expression = last_assigned_expression;
		if (_guess_expression_type(c, last_assigned_expression, r_type)) {
			if (var_type.has_type) {
				r_type.type = var_type;
			}
			return true;
		}
	}

	if (var_type.has_type) {
		r_type.type = var_type;
		return true;
	}

	if (p_context.function) {
		for (int i = 0; i < p_context.function->arguments.size(); i++) {
			if (p_context.function->arguments[i] == p_identifier) {
				if (p_context.function->argument_types[i].has_type) {
					r_type.type = p_context.function->argument_types[i];
					return true;
				}

				int def_from = p_context.function->arguments.size() - p_context.function->default_values.size();
				if (i >= def_from) {
					int def_idx = i - def_from;
					if (p_context.function->default_values[def_idx]->type == GDScriptParser::Node::TYPE_OPERATOR) {
						const GDScriptParser::OperatorNode *op = static_cast<const GDScriptParser::OperatorNode *>(p_context.function->default_values[def_idx]);
						if (op->arguments.size() < 2) {
							return false;
						}
						GDScriptCompletionContext c = p_context;
						c.function = nullptr;
						c.block = nullptr;
						return _guess_expression_type(c, op->arguments[1], r_type);
					}
				}
				break;
			}
		}

		GDScriptParser::DataType base_type = p_context._class->base_type;
		while (base_type.has_type) {
			switch (base_type.kind) {
				case GDScriptParser::DataType::GDSCRIPT: {
					Ref<GDScript> gds = base_type.script_type;
					if (gds.is_valid() && gds->has_method(p_context.function->name)) {
						GDScriptFunction *func = gds->get_member_functions()[p_context.function->name];
						if (func) {
							for (int i = 0; i < func->get_argument_count(); i++) {
								if (func->get_argument_name(i) == p_identifier) {
									r_type = _type_from_gdtype(func->get_argument_type(i));
									return true;
								}
							}
						}
						Ref<GDScript> base_gds = gds->get_base_script();
						if (base_gds.is_valid()) {
							base_type.kind = GDScriptParser::DataType::GDSCRIPT;
							base_type.script_type = base_gds;
						} else {
							base_type.kind = GDScriptParser::DataType::NATIVE;
							base_type.native_type = gds->get_instance_base_type();
						}
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.native_type = gds->get_instance_base_type();
					}
				} break;
				case GDScriptParser::DataType::NATIVE: {
					List<MethodInfo> methods;
					ClassDB::get_method_list(base_type.native_type, &methods);
					ClassDB::get_virtual_methods(base_type.native_type, &methods);

					for (List<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
						if (E->get().name == p_context.function->name) {
							MethodInfo &mi = E->get();
							for (List<PropertyInfo>::Element *F = mi.arguments.front(); F; F = F->next()) {
								if (F->get().name == p_identifier) {
									r_type = _type_from_property(F->get());
									return true;
								}
							}
						}
					}
					base_type.has_type = false;
				} break;
				default: {
					base_type.has_type = false;
				} break;
			}
		}
	}

	// Check current class (including inheritance)
	if (p_context._class) {
		GDScriptCompletionIdentifier context_base;
		context_base.value = p_context.base;
		context_base.type.has_type = true;
		context_base.type.kind = GDScriptParser::DataType::CLASS;
		context_base.type.class_type = const_cast<GDScriptParser::ClassNode *>(p_context._class);
		context_base.type.is_meta_type = p_context.function && p_context.function->_static;

		if (_guess_identifier_type_from_base(p_context, context_base, p_identifier, r_type)) {
			return true;
		}
	}

	// Check named scripts
	if (ScriptServer::is_global_class(p_identifier)) {
		Ref<Script> scr = ResourceLoader::load(ScriptServer::get_global_class_path(p_identifier));
		if (scr.is_valid()) {
			r_type = _type_from_variant(scr);
			r_type.type.is_meta_type = true;
			return true;
		}
		return false;
	}

	for (int i = 0; i < 2; i++) {
		StringName target_id;
		switch (i) {
			case 0:
				// Check ClassDB
				target_id = p_identifier;
				break;
			case 1:
				// ClassDB again for underscore-prefixed classes
				target_id = String("_") + p_identifier;
				break;
		}

		if (ClassDB::class_exists(target_id)) {
			r_type.type.has_type = true;
			r_type.type.kind = GDScriptParser::DataType::NATIVE;
			r_type.type.native_type = target_id;
			if (Engine::get_singleton()->has_singleton(target_id)) {
				r_type.type.is_meta_type = false;
				r_type.value = Engine::get_singleton()->get_singleton_object(target_id);
			} else {
				r_type.type.is_meta_type = true;
				const Map<StringName, int>::Element *target_elem = GDScriptLanguage::get_singleton()->get_global_map().find(target_id);
				// Check because classes like EditorNode are in ClassDB by now, but unknown to GDScript
				if (!target_elem) {
					return false;
				}
				int idx = target_elem->get();
				r_type.value = GDScriptLanguage::get_singleton()->get_global_array()[idx];
			}
			return true;
		}
	}

	// Check autoload singletons
	if (GDScriptLanguage::get_singleton()->get_named_globals_map().has(p_identifier)) {
		r_type = _type_from_variant(GDScriptLanguage::get_singleton()->get_named_globals_map()[p_identifier]);
		return true;
	}

	return false;
}

static bool _guess_identifier_type_from_base(GDScriptCompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_identifier, GDScriptCompletionIdentifier &r_type) {
	GDScriptParser::DataType base_type = p_base.type;
	bool _static = base_type.is_meta_type;
	while (base_type.has_type) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS: {
				if (base_type.class_type->constant_expressions.has(p_identifier)) {
					GDScriptParser::ClassNode::Constant c = base_type.class_type->constant_expressions[p_identifier];
					r_type.type = c.type;
					if (c.expression->type == GDScriptParser::Node::TYPE_CONSTANT) {
						r_type.value = static_cast<const GDScriptParser::ConstantNode *>(c.expression)->value;
					}
					return true;
				}

				if (!_static) {
					for (int i = 0; i < base_type.class_type->variables.size(); i++) {
						GDScriptParser::ClassNode::Member m = base_type.class_type->variables[i];
						if (m.identifier == p_identifier) {
							if (m.expression) {
								if (p_context.line == m.expression->line) {
									// Variable used in the same expression
									return false;
								}
								if (_guess_expression_type(p_context, m.expression, r_type)) {
									return true;
								}
								if (m.expression->get_datatype().has_type) {
									r_type.type = m.expression->get_datatype();
									return true;
								}
							}
							if (m.data_type.has_type) {
								r_type.type = m.data_type;
								return true;
							}
							return false;
						}
					}
				}
				base_type = base_type.class_type->base_type;
			} break;
			case GDScriptParser::DataType::GDSCRIPT: {
				Ref<GDScript> gds = base_type.script_type;
				if (gds.is_valid()) {
					if (gds->get_constants().has(p_identifier)) {
						r_type = _type_from_variant(gds->get_constants()[p_identifier]);
						return true;
					}
					if (!_static) {
						const Set<StringName>::Element *m = gds->get_members().find(p_identifier);
						if (m) {
							r_type = _type_from_gdtype(gds->get_member_type(p_identifier));
							return true;
						}
					}
					Ref<GDScript> parent = gds->get_base_script();
					if (parent.is_valid()) {
						base_type.script_type = parent;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.native_type = gds->get_instance_base_type();
					}
				} else {
					return false;
				}
			} break;
			case GDScriptParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					Map<StringName, Variant> constants;
					scr->get_constants(&constants);
					if (constants.has(p_identifier)) {
						r_type = _type_from_variant(constants[p_identifier]);
						return true;
					}

					if (!_static) {
						List<PropertyInfo> members;
						scr->get_script_property_list(&members);
						for (const List<PropertyInfo>::Element *E = members.front(); E; E = E->next()) {
							const PropertyInfo &prop = E->get();
							if (prop.name == p_identifier) {
								r_type = _type_from_property(prop);
								return true;
							}
						}
					}
					Ref<Script> parent = scr->get_base_script();
					if (parent.is_valid()) {
						base_type.script_type = parent;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return false;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName class_name = base_type.native_type;
				if (!ClassDB::class_exists(class_name)) {
					class_name = String("_") + class_name;
					if (!ClassDB::class_exists(class_name)) {
						return false;
					}
				}

				// Skip constants since they're all integers. Type does not matter because int has no members

				List<PropertyInfo> props;
				ClassDB::get_property_list(class_name, &props);
				for (const List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
					const PropertyInfo &prop = E->get();

					// Ignore groups and categories in code completion.
					if (prop.usage & (PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY)) {
						continue;
					}

					if (prop.name == p_identifier) {
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
						break;
					}
				}
				return false;
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				Variant::CallError err;
				Variant tmp = Variant::construct(base_type.builtin_type, nullptr, 0, err);

				if (err.error != Variant::CallError::CALL_OK) {
					return false;
				}
				bool valid = false;
				Variant res = tmp.get(p_identifier, &valid);
				if (valid) {
					r_type = _type_from_variant(res);
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

static bool _find_last_return_in_block(const GDScriptCompletionContext &p_context, int &r_last_return_line, const GDScriptParser::Node **r_last_returned_value) {
	if (!p_context.block) {
		return false;
	}

	for (int i = 0; i < p_context.block->statements.size(); i++) {
		if (p_context.block->statements[i]->line < r_last_return_line) {
			continue;
		}
		if (p_context.block->statements[i]->type != GDScriptParser::Node::TYPE_CONTROL_FLOW) {
			continue;
		}

		const GDScriptParser::ControlFlowNode *cf = static_cast<const GDScriptParser::ControlFlowNode *>(p_context.block->statements[i]);
		if (cf->cf_type == GDScriptParser::ControlFlowNode::CF_RETURN && cf->arguments.size() > 0) {
			if (cf->line > r_last_return_line) {
				r_last_return_line = cf->line;
				*r_last_returned_value = cf->arguments[0];
			}
		}
	}

	// Recurse into subblocks
	for (int i = 0; i < p_context.block->sub_blocks.size(); i++) {
		GDScriptCompletionContext c = p_context;
		c.block = p_context.block->sub_blocks[i];
		_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
	}

	return false;
}

static bool _guess_method_return_type_from_base(GDScriptCompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_method, GDScriptCompletionIdentifier &r_type) {
	GDScriptParser::DataType base_type = p_base.type;
	bool _static = base_type.is_meta_type;

	if (_static && p_method == "new") {
		r_type.type = base_type;
		r_type.type.is_meta_type = false;
		r_type.type.is_constant = false;
		return true;
	}

	while (base_type.has_type) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS: {
				if (!base_type.class_type) {
					base_type.has_type = false;
					break;
				}

				for (int i = 0; i < base_type.class_type->static_functions.size(); i++) {
					if (base_type.class_type->static_functions[i]->name == p_method) {
						int last_return_line = -1;
						const GDScriptParser::Node *last_returned_value = nullptr;
						GDScriptCompletionContext c = p_context;
						c._class = base_type.class_type;
						c.function = base_type.class_type->static_functions[i];
						c.block = c.function->body;

						_find_last_return_in_block(c, last_return_line, &last_returned_value);
						if (last_returned_value) {
							c.line = c.block->end_line;
							return _guess_expression_type(c, last_returned_value, r_type);
						}
					}
				}
				if (!_static) {
					for (int i = 0; i < base_type.class_type->functions.size(); i++) {
						if (base_type.class_type->functions[i]->name == p_method) {
							int last_return_line = -1;
							const GDScriptParser::Node *last_returned_value = nullptr;
							GDScriptCompletionContext c = p_context;
							c._class = base_type.class_type;
							c.function = base_type.class_type->functions[i];
							c.block = c.function->body;

							_find_last_return_in_block(c, last_return_line, &last_returned_value);
							if (last_returned_value) {
								c.line = c.block->end_line;
								return _guess_expression_type(c, last_returned_value, r_type);
							}
						}
					}
				}

				base_type = base_type.class_type->base_type;
			} break;
			case GDScriptParser::DataType::GDSCRIPT: {
				Ref<GDScript> gds = base_type.script_type;
				if (gds.is_valid()) {
					if (gds->get_member_functions().has(p_method)) {
						r_type = _type_from_gdtype(gds->get_member_functions()[p_method]->get_return_type());
						return true;
					}
					Ref<GDScript> base_script = gds->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.native_type = gds->get_instance_base_type();
					}
				} else {
					return false;
				}
			} break;
			case GDScriptParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					List<MethodInfo> methods;
					scr->get_script_method_list(&methods);
					for (List<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
						MethodInfo &mi = E->get();
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
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return false;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName native = base_type.native_type;
				if (!ClassDB::class_exists(native)) {
					native = String("_") + native;
					if (!ClassDB::class_exists(native)) {
						return false;
					}
				}
				MethodBind *mb = ClassDB::get_method(native, p_method);
				if (mb) {
					r_type = _type_from_property(mb->get_return_info());
					return true;
				}
				return false;
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				Variant::CallError err;
				Variant tmp = Variant::construct(base_type.builtin_type, nullptr, 0, err);
				if (err.error != Variant::CallError::CALL_OK) {
					return false;
				}

				List<MethodInfo> methods;
				tmp.get_method_list(&methods);

				for (List<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
					MethodInfo &mi = E->get();
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

static String _make_arguments_hint(const MethodInfo &p_info, int p_arg_idx) {
	String arghint = _get_visual_datatype(p_info.return_val, false) + " " + p_info.name + "(";

	int def_args = p_info.arguments.size() - p_info.default_arguments.size();
	int i = 0;
	for (const List<PropertyInfo>::Element *E = p_info.arguments.front(); E; E = E->next()) {
		if (i > 0) {
			arghint += ", ";
		}

		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}
		arghint += E->get().name + ": " + _get_visual_datatype(E->get(), true);

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

static String _make_arguments_hint(const GDScriptParser::FunctionNode *p_function, int p_arg_idx) {
	String arghint = p_function->return_type.to_string() + " " + p_function->name.operator String() + "(";

	int def_args = p_function->arguments.size() - p_function->default_values.size();
	for (int i = 0; i < p_function->arguments.size(); i++) {
		if (i > 0) {
			arghint += ", ";
		}

		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}
		arghint += p_function->arguments[i].operator String() + ": " + p_function->argument_types[i].to_string();

		if (i - def_args >= 0) {
			String def_val = "<unknown>";
			if (p_function->default_values[i - def_args] && p_function->default_values[i - def_args]->type == GDScriptParser::Node::TYPE_OPERATOR) {
				const GDScriptParser::OperatorNode *assign = static_cast<const GDScriptParser::OperatorNode *>(p_function->default_values[i - def_args]);

				if (assign->arguments.size() >= 2) {
					if (assign->arguments[1]->type == GDScriptParser::Node::TYPE_CONSTANT) {
						const GDScriptParser::ConstantNode *cn = static_cast<const GDScriptParser::ConstantNode *>(assign->arguments[1]);
						def_val = cn->value.get_construct_string();
					} else if (assign->arguments[1]->type == GDScriptParser::Node::TYPE_IDENTIFIER) {
						const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(assign->arguments[1]);
						def_val = id->name.operator String();
					}
				}
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

static void _find_enumeration_candidates(const String p_enum_hint, Map<String, ScriptCodeCompletionOption> &r_result) {
	if (p_enum_hint.find(".") == -1) {
		// Global constant
		StringName current_enum = p_enum_hint;
		for (int i = 0; i < GlobalConstants::get_global_constant_count(); i++) {
			if (GlobalConstants::get_global_constant_enum(i) == current_enum) {
				ScriptCodeCompletionOption option(GlobalConstants::get_global_constant_name(i), ScriptCodeCompletionOption::KIND_ENUM);
				r_result.insert(option.display, option);
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
		for (List<StringName>::Element *E = enum_constants.front(); E; E = E->next()) {
			String candidate = class_name + "." + E->get();
			if (candidate[0] == '_') { // Trim leading underscore.
				candidate = candidate.substr(1);
			}
			ScriptCodeCompletionOption option(candidate, ScriptCodeCompletionOption::KIND_ENUM);
			r_result.insert(option.display, option);
		}
	}
}

static void _find_identifiers_in_block(const GDScriptCompletionContext &p_context, Map<String, ScriptCodeCompletionOption> &r_result) {
	for (Map<StringName, GDScriptParser::LocalVarNode *>::Element *E = p_context.block->variables.front(); E; E = E->next()) {
		if (E->get()->line < p_context.line) {
			ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_VARIABLE);
			r_result.insert(option.display, option);
		}
	}
	if (p_context.block->parent_block) {
		GDScriptCompletionContext c = p_context;
		c.block = p_context.block->parent_block;
		_find_identifiers_in_block(c, r_result);
	}
}

static void _find_identifiers_in_base(const GDScriptCompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, bool p_only_functions, Map<String, ScriptCodeCompletionOption> &r_result);

static void _find_identifiers_in_class(const GDScriptCompletionContext &p_context, bool p_static, bool p_only_functions, bool p_parent_only, Map<String, ScriptCodeCompletionOption> &r_result) {
	if (!p_parent_only) {
		if (!p_static && !p_only_functions) {
			for (int i = 0; i < p_context._class->variables.size(); i++) {
				ScriptCodeCompletionOption option(p_context._class->variables[i].identifier, ScriptCodeCompletionOption::KIND_MEMBER);
				r_result.insert(option.display, option);
			}
		}

		if (!p_only_functions) {
			for (Map<StringName, GDScriptParser::ClassNode::Constant>::Element *E = p_context._class->constant_expressions.front(); E; E = E->next()) {
				ScriptCodeCompletionOption option(E->key(), ScriptCodeCompletionOption::KIND_CONSTANT);
				if (E->get().expression && E->get().expression->type == GDScriptParser::Node::TYPE_CONSTANT) {
					GDScriptParser::ConstantNode *cnode = (GDScriptParser::ConstantNode *)E->get().expression;
					if (cnode) {
						option.default_value = cnode->value;
					}
				}
				r_result.insert(option.display, option);
			}
			for (int i = 0; i < p_context._class->subclasses.size(); i++) {
				ScriptCodeCompletionOption option(p_context._class->subclasses[i]->name, ScriptCodeCompletionOption::KIND_CLASS);
				r_result.insert(option.display, option);
			}
		}

		for (int i = 0; i < p_context._class->static_functions.size(); i++) {
			ScriptCodeCompletionOption option(p_context._class->static_functions[i]->name.operator String(), ScriptCodeCompletionOption::KIND_FUNCTION);
			if (p_context._class->static_functions[i]->arguments.size()) {
				option.insert_text += "(";
			} else {
				option.insert_text += "()";
			}
			r_result.insert(option.display, option);
		}

		if (!p_static) {
			for (int i = 0; i < p_context._class->functions.size(); i++) {
				ScriptCodeCompletionOption option(p_context._class->functions[i]->name.operator String(), ScriptCodeCompletionOption::KIND_FUNCTION);
				if (p_context._class->functions[i]->arguments.size()) {
					option.insert_text += "(";
				} else {
					option.insert_text += "()";
				}
				r_result.insert(option.display, option);
			}
		}
	}

	// Parents
	GDScriptCompletionIdentifier base_type;
	base_type.type = p_context._class->base_type;
	base_type.type.is_meta_type = p_static;
	base_type.value = p_context.base;

	GDScriptCompletionContext c = p_context;
	c.block = nullptr;
	c.function = nullptr;

	_find_identifiers_in_base(c, base_type, p_only_functions, r_result);
}

static void _find_identifiers_in_base(const GDScriptCompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, bool p_only_functions, Map<String, ScriptCodeCompletionOption> &r_result) {
	GDScriptParser::DataType base_type = p_base.type;
	bool _static = base_type.is_meta_type;

	if (_static && base_type.kind != GDScriptParser::DataType::BUILTIN) {
		ScriptCodeCompletionOption option("new", ScriptCodeCompletionOption::KIND_FUNCTION);
		option.insert_text += "(";
		r_result.insert(option.display, option);
	}

	while (base_type.has_type) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS: {
				GDScriptCompletionContext c = p_context;
				c._class = base_type.class_type;
				c.block = nullptr;
				c.function = nullptr;
				_find_identifiers_in_class(c, _static, p_only_functions, false, r_result);
				base_type = base_type.class_type->base_type;
			} break;
			case GDScriptParser::DataType::GDSCRIPT: {
				Ref<GDScript> script = base_type.script_type;
				if (script.is_valid()) {
					if (!_static && !p_only_functions) {
						if (p_context.base && p_context.base->get_script_instance()) {
							List<PropertyInfo> members;
							p_context.base->get_script_instance()->get_property_list(&members);
							for (List<PropertyInfo>::Element *E = members.front(); E; E = E->next()) {
								ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_MEMBER);
								r_result.insert(option.display, option);
							}
						}
						for (const Set<StringName>::Element *E = script->get_members().front(); E; E = E->next()) {
							ScriptCodeCompletionOption option(E->get().operator String(), ScriptCodeCompletionOption::KIND_MEMBER);
							r_result.insert(option.display, option);
						}
					}
					if (!p_only_functions) {
						for (const Map<StringName, Variant>::Element *E = script->get_constants().front(); E; E = E->next()) {
							ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_CONSTANT);
							r_result.insert(option.display, option);
						}
					}
					for (const Map<StringName, GDScriptFunction *>::Element *E = script->get_member_functions().front(); E; E = E->next()) {
						if (!_static || E->get()->is_static()) {
							ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_FUNCTION);
							if (E->get()->get_argument_count()) {
								option.insert_text += "(";
							} else {
								option.insert_text += "()";
							}
							r_result.insert(option.display, option);
						}
					}
					if (!p_only_functions) {
						for (const Map<StringName, Ref<GDScript>>::Element *E = script->get_subclasses().front(); E; E = E->next()) {
							ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
							r_result.insert(option.display, option);
						}
					}
					base_type = GDScriptParser::DataType();
					if (script->get_base().is_valid()) {
						base_type.has_type = true;
						base_type.kind = GDScriptParser::DataType::GDSCRIPT;
						base_type.script_type = script->get_base();
					} else {
						base_type.has_type = script->get_instance_base_type() != StringName();
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.native_type = script->get_instance_base_type();
					}
				} else {
					return;
				}
			} break;
			case GDScriptParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					if (!_static && !p_only_functions) {
						List<PropertyInfo> members;
						scr->get_script_property_list(&members);
						for (List<PropertyInfo>::Element *E = members.front(); E; E = E->next()) {
							ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_MEMBER);
							r_result.insert(option.display, option);
						}
					}
					if (!p_only_functions) {
						Map<StringName, Variant> constants;
						scr->get_constants(&constants);
						for (Map<StringName, Variant>::Element *E = constants.front(); E; E = E->next()) {
							ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_CONSTANT);
							r_result.insert(option.display, option);
						}
					}

					List<MethodInfo> methods;
					scr->get_script_method_list(&methods);
					for (List<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
						ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_FUNCTION);
						if (E->get().arguments.size()) {
							option.insert_text += "(";
						} else {
							option.insert_text += "()";
						}
						r_result.insert(option.display, option);
					}

					Ref<Script> base_script = scr->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName type = base_type.native_type;
				if (!ClassDB::class_exists(type)) {
					type = String("_") + type;
					if (!ClassDB::class_exists(type)) {
						return;
					}
				}

				if (!p_only_functions) {
					List<String> constants;
					ClassDB::get_integer_constant_list(type, &constants);
					for (List<String>::Element *E = constants.front(); E; E = E->next()) {
						ScriptCodeCompletionOption option(E->get(), ScriptCodeCompletionOption::KIND_CONSTANT);
						r_result.insert(option.display, option);
					}

					if (!_static) {
						List<PropertyInfo> pinfo;
						ClassDB::get_property_list(type, &pinfo);
						for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
							if (E->get().usage & (PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY)) {
								continue;
							}
							if (E->get().name.find("/") != -1) {
								continue;
							}
							ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_MEMBER);
							r_result.insert(option.display, option);
						}
					}
				}

				if (!_static) {
					List<MethodInfo> methods;
					bool is_autocompleting_getters = GLOBAL_GET("debug/gdscript/completion/autocomplete_setters_and_getters").booleanize();
					ClassDB::get_method_list(type, &methods, false, !is_autocompleting_getters);
					for (List<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
						if (E->get().name.begins_with("_")) {
							continue;
						}
						ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_FUNCTION);
						if (E->get().arguments.size()) {
							option.insert_text += "(";
						} else {
							option.insert_text += "()";
						}
						r_result.insert(option.display, option);
					}
				}

				return;
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				Variant::CallError err;
				Variant tmp = Variant::construct(base_type.builtin_type, nullptr, 0, err);
				if (err.error != Variant::CallError::CALL_OK) {
					return;
				}

				if (!p_only_functions) {
					List<PropertyInfo> members;
					if (p_base.value.get_type() != Variant::NIL) {
						p_base.value.get_property_list(&members);
					} else {
						tmp.get_property_list(&members);
					}

					for (List<PropertyInfo>::Element *E = members.front(); E; E = E->next()) {
						if (String(E->get().name).find("/") == -1) {
							ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_MEMBER);
							r_result.insert(option.display, option);
						}
					}
				}

				List<MethodInfo> methods;
				tmp.get_method_list(&methods);
				for (List<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
					ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_FUNCTION);
					if (E->get().arguments.size()) {
						option.insert_text += "(";
					} else {
						option.insert_text += "()";
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

static void _find_identifiers(const GDScriptCompletionContext &p_context, bool p_only_functions, Map<String, ScriptCodeCompletionOption> &r_result) {
	const GDScriptParser::BlockNode *block = p_context.block;

	if (p_context.function) {
		const GDScriptParser::FunctionNode *f = p_context.function;

		for (int i = 0; i < f->arguments.size(); i++) {
			ScriptCodeCompletionOption option(f->arguments[i].operator String(), ScriptCodeCompletionOption::KIND_PLAIN_TEXT);
			r_result.insert(option.display, option);
		}
	}

	if (!p_only_functions && block) {
		GDScriptCompletionContext c = p_context;
		c.block = block;
		_find_identifiers_in_block(c, r_result);
	}

	const GDScriptParser::ClassNode *clss = p_context._class;
	bool _static = p_context.function && p_context.function->_static;

	while (clss) {
		GDScriptCompletionContext c = p_context;
		c._class = clss;
		c.block = nullptr;
		c.function = nullptr;
		_find_identifiers_in_class(c, _static, p_only_functions, false, r_result);
		_static = true;
		clss = clss->owner;
	}

	for (int i = 0; i < GDScriptFunctions::FUNC_MAX; i++) {
		MethodInfo mi = GDScriptFunctions::get_info(GDScriptFunctions::Function(i));
		ScriptCodeCompletionOption option(String(GDScriptFunctions::get_func_name(GDScriptFunctions::Function(i))), ScriptCodeCompletionOption::KIND_FUNCTION);
		if (mi.arguments.size() || (mi.flags & METHOD_FLAG_VARARG)) {
			option.insert_text += "(";
		} else {
			option.insert_text += "()";
		}
		r_result.insert(option.display, option);
	}

	static const char *_type_names[Variant::VARIANT_MAX] = {
		"null", "bool", "int", "float", "String", "Vector2", "Rect2", "Vector3", "Transform2D", "Plane", "Quat", "AABB", "Basis", "Transform",
		"Color", "NodePath", "RID", "Object", "Dictionary", "Array", "PoolByteArray", "PoolIntArray", "PoolRealArray", "PoolStringArray",
		"PoolVector2Array", "PoolVector3Array", "PoolColorArray"
	};

	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		ScriptCodeCompletionOption option(_type_names[i], ScriptCodeCompletionOption::KIND_CLASS);
		r_result.insert(option.display, option);
	}

	static const char *_keywords[] = {
		"and", "in", "not", "or", "false", "PI", "TAU", "INF", "NAN", "self", "true", "as", "assert",
		"breakpoint", "class", "class_name", "extends", "is", "func", "preload", "setget", "signal", "tool", "yield",
		"const", "enum", "export", "onready", "static", "var", "break", "continue", "if", "elif",
		"else", "for", "pass", "return", "match", "while", "remote", "sync", "master", "puppet", "slave",
		"remotesync", "mastersync", "puppetsync",
		nullptr
	};

	const char **kw = _keywords;
	while (*kw) {
		ScriptCodeCompletionOption option(*kw, ScriptCodeCompletionOption::KIND_PLAIN_TEXT);
		r_result.insert(option.display, option);
		kw++;
	}

	// Autoload singletons
	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);
	for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
		String s = E->get().name;
		if (!s.begins_with("autoload/")) {
			continue;
		}
		String path = ProjectSettings::get_singleton()->get(s);
		if (path.begins_with("*")) {
			ScriptCodeCompletionOption option(s.get_slice("/", 1), ScriptCodeCompletionOption::KIND_CONSTANT);
			r_result.insert(option.display, option);
		}
	}

	// Named scripts
	List<StringName> named_scripts;
	ScriptServer::get_global_class_list(&named_scripts);
	for (List<StringName>::Element *E = named_scripts.front(); E; E = E->next()) {
		ScriptCodeCompletionOption option(E->get().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
		r_result.insert(option.display, option);
	}

	// Native classes
	for (const Map<StringName, int>::Element *E = GDScriptLanguage::get_singleton()->get_global_map().front(); E; E = E->next()) {
		ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
		r_result.insert(option.display, option);
	}
}

static void _find_call_arguments(const GDScriptCompletionContext &p_context, const GDScriptCompletionIdentifier &p_base, const StringName &p_method, int p_argidx, bool p_static, Map<String, ScriptCodeCompletionOption> &r_result, String &r_arghint) {
	Variant base = p_base.value;
	GDScriptParser::DataType base_type = p_base.type;

	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";

#define IS_METHOD_SIGNAL(m_method) (m_method == "connect" || m_method == "disconnect" || m_method == "is_connected" || m_method == "emit_signal")

	while (base_type.has_type) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS: {
				for (int i = 0; i < base_type.class_type->static_functions.size(); i++) {
					if (base_type.class_type->static_functions[i]->name == p_method) {
						r_arghint = _make_arguments_hint(base_type.class_type->static_functions[i], p_argidx);
						return;
					}
				}
				for (int i = 0; i < base_type.class_type->functions.size(); i++) {
					if (base_type.class_type->functions[i]->name == p_method) {
						r_arghint = _make_arguments_hint(base_type.class_type->functions[i], p_argidx);
						return;
					}
				}

				if (IS_METHOD_SIGNAL(p_method) && p_argidx == 0) {
					for (int i = 0; i < base_type.class_type->_signals.size(); i++) {
						ScriptCodeCompletionOption option(base_type.class_type->_signals[i].name.operator String(), ScriptCodeCompletionOption::KIND_SIGNAL);
						option.insert_text = quote_style + option.display + quote_style;
						r_result.insert(option.display, option);
					}
				}

				base_type = base_type.class_type->base_type;
			} break;
			case GDScriptParser::DataType::GDSCRIPT: {
				Ref<GDScript> gds = base_type.script_type;
				if (gds.is_valid()) {
					if (IS_METHOD_SIGNAL(p_method) && p_argidx == 0) {
						List<MethodInfo> signals;
						gds->get_script_signal_list(&signals);
						for (List<MethodInfo>::Element *E = signals.front(); E; E = E->next()) {
							ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_SIGNAL);
							option.insert_text = quote_style + option.display + quote_style;
							r_result.insert(option.display, option);
						}
					}
					Ref<GDScript> base_script = gds->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.native_type = gds->get_instance_base_type();
					}
				} else {
					return;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName class_name = base_type.native_type;
				if (!ClassDB::class_exists(class_name)) {
					class_name = String("_") + class_name;
					if (!ClassDB::class_exists(class_name)) {
						base_type.has_type = false;
						break;
					}
				}

				List<MethodInfo> methods;
				ClassDB::get_method_list(class_name, &methods);
				ClassDB::get_virtual_methods(class_name, &methods);
				int method_args = 0;

				for (List<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
					if (E->get().name == p_method) {
						method_args = E->get().arguments.size();
						if (base.get_type() == Variant::OBJECT) {
							Object *obj = base.operator Object *();
							if (obj) {
								List<String> options;
								obj->get_argument_options(p_method, p_argidx, &options);
								for (List<String>::Element *F = options.front(); F; F = F->next()) {
									ScriptCodeCompletionOption option(F->get(), ScriptCodeCompletionOption::KIND_FUNCTION);
									r_result.insert(option.display, option);
								}
							}
						}

						if (p_argidx < method_args) {
							PropertyInfo arg_info = E->get().arguments[p_argidx];
							if (arg_info.usage & PROPERTY_USAGE_CLASS_IS_ENUM) {
								_find_enumeration_candidates(arg_info.class_name, r_result);
							}
						}

						r_arghint = _make_arguments_hint(E->get(), p_argidx);
						break;
					}
				}

				if (IS_METHOD_SIGNAL(p_method) && p_argidx == 0) {
					List<MethodInfo> signals;
					ClassDB::get_signal_list(class_name, &signals);
					for (List<MethodInfo>::Element *E = signals.front(); E; E = E->next()) {
						ScriptCodeCompletionOption option(E->get().name, ScriptCodeCompletionOption::KIND_SIGNAL);
						option.insert_text = quote_style + option.display + quote_style;
						r_result.insert(option.display, option);
					}
				}
#undef IS_METHOD_SIGNAL

				if (ClassDB::is_parent_class(class_name, "Node") && (p_method == "get_node" || p_method == "has_node") && p_argidx == 0) {
					// Get autoloads
					List<PropertyInfo> props;
					ProjectSettings::get_singleton()->get_property_list(&props);

					for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
						String s = E->get().name;
						if (!s.begins_with("autoload/")) {
							continue;
						}
						String name = s.get_slice("/", 1);
						ScriptCodeCompletionOption option("/root/" + name, ScriptCodeCompletionOption::KIND_NODE_PATH);
						option.insert_text = quote_style + option.display + quote_style;
						r_result.insert(option.display, option);
					}
				}

				if (p_argidx == 0 && method_args > 0 && ClassDB::is_parent_class(class_name, "InputEvent") && p_method.operator String().find("action") != -1) {
					// Get input actions
					List<PropertyInfo> props;
					ProjectSettings::get_singleton()->get_property_list(&props);
					for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
						String s = E->get().name;
						if (!s.begins_with("input/")) {
							continue;
						}
						String name = s.get_slice("/", 1);
						ScriptCodeCompletionOption option(name, ScriptCodeCompletionOption::KIND_CONSTANT);
						option.insert_text = quote_style + option.display + quote_style;
						r_result.insert(option.display, option);
					}
				}

				base_type.has_type = false;
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				if (base.get_type() == Variant::NIL) {
					Variant::CallError err;
					base = Variant::construct(base_type.builtin_type, nullptr, 0, err);
					if (err.error != Variant::CallError::CALL_OK) {
						return;
					}
				}

				List<MethodInfo> methods;
				base.get_method_list(&methods);
				for (List<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
					if (E->get().name == p_method) {
						r_arghint = _make_arguments_hint(E->get(), p_argidx);
						return;
					}
				}

				base_type.has_type = false;
			} break;
			default: {
				base_type.has_type = false;
			} break;
		}
	}
}

static void _find_call_arguments(GDScriptCompletionContext &p_context, const GDScriptParser::Node *p_node, int p_argidx, Map<String, ScriptCodeCompletionOption> &r_result, bool &r_forced, String &r_arghint) {
	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";

	if (!p_node || p_node->type != GDScriptParser::Node::TYPE_OPERATOR) {
		return;
	}

	Variant base;
	GDScriptParser::DataType base_type;
	StringName function;
	bool _static = false;
	const GDScriptParser::OperatorNode *op = static_cast<const GDScriptParser::OperatorNode *>(p_node);

	GDScriptCompletionIdentifier connect_base;

	if (op->op != GDScriptParser::OperatorNode::OP_CALL && op->op != GDScriptParser::OperatorNode::OP_PARENT_CALL) {
		return;
	}

	if (!op->arguments.size()) {
		return;
	}

	if (op->op == GDScriptParser::OperatorNode::OP_CALL) {
		if (op->arguments[0]->type == GDScriptParser::Node::TYPE_BUILT_IN_FUNCTION) {
			// Complete built-in function
			const GDScriptParser::BuiltInFunctionNode *fn = static_cast<const GDScriptParser::BuiltInFunctionNode *>(op->arguments[0]);
			MethodInfo mi = GDScriptFunctions::get_info(fn->function);

			if ((mi.name == "load" || mi.name == "preload") && bool(EditorSettings::get_singleton()->get("text_editor/completion/complete_file_paths"))) {
				_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), r_result);
			}

			r_arghint = _make_arguments_hint(mi, p_argidx);
			return;

		} else if (op->arguments[0]->type == GDScriptParser::Node::TYPE_TYPE) {
			// Complete constructor
			const GDScriptParser::TypeNode *tn = static_cast<const GDScriptParser::TypeNode *>(op->arguments[0]);

			List<MethodInfo> constructors;
			Variant::get_constructor_list(tn->vtype, &constructors);

			int i = 0;
			for (List<MethodInfo>::Element *E = constructors.front(); E; E = E->next()) {
				if (p_argidx >= E->get().arguments.size()) {
					continue;
				}
				if (i > 0) {
					r_arghint += "\n";
				}
				r_arghint += _make_arguments_hint(E->get(), p_argidx);
				i++;
			}
			return;
		} else if (op->arguments[0]->type == GDScriptParser::Node::TYPE_SELF) {
			if (op->arguments.size() < 2 || op->arguments[1]->type != GDScriptParser::Node::TYPE_IDENTIFIER) {
				return;
			}

			base = p_context.base;

			const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[1]);
			function = id->name;
			base_type.has_type = true;
			base_type.kind = GDScriptParser::DataType::CLASS;
			base_type.class_type = const_cast<GDScriptParser::ClassNode *>(p_context._class);
			_static = p_context.function && p_context.function->_static;

			if (function == "connect" && op->arguments.size() >= 4) {
				_guess_expression_type(p_context, op->arguments[3], connect_base);
			}

		} else {
			if (op->arguments.size() < 2 || op->arguments[1]->type != GDScriptParser::Node::TYPE_IDENTIFIER) {
				return;
			}
			const GDScriptParser::IdentifierNode *id = static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[1]);
			function = id->name;

			GDScriptCompletionIdentifier ci;
			if (_guess_expression_type(p_context, op->arguments[0], ci)) {
				base_type = ci.type;
				base = ci.value;
			} else {
				return;
			}
			_static = ci.type.is_meta_type;

			if (function == "connect" && op->arguments.size() >= 4) {
				_guess_expression_type(p_context, op->arguments[3], connect_base);
			}
		}
	} else {
		if (!p_context._class || op->arguments.size() < 1 || op->arguments[0]->type != GDScriptParser::Node::TYPE_IDENTIFIER) {
			return;
		}
		base_type.has_type = true;
		base_type.kind = GDScriptParser::DataType::CLASS;
		base_type.class_type = const_cast<GDScriptParser::ClassNode *>(p_context._class);
		base_type.is_meta_type = p_context.function && p_context.function->_static;
		base = p_context.base;

		function = static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[0])->name;

		if (function == "connect" && op->arguments.size() >= 4) {
			_guess_expression_type(p_context, op->arguments[3], connect_base);
		}
	}

	GDScriptCompletionIdentifier ci;
	ci.type = base_type;
	ci.value = base;
	_find_call_arguments(p_context, ci, function, p_argidx, _static, r_result, r_arghint);

	if (function == "connect" && p_argidx == 2) {
		Map<String, ScriptCodeCompletionOption> methods;
		_find_identifiers_in_base(p_context, connect_base, true, methods);
		for (Map<String, ScriptCodeCompletionOption>::Element *E = methods.front(); E; E = E->next()) {
			ScriptCodeCompletionOption &option = E->value();
			option.insert_text = quote_style + option.display + quote_style;
			r_result.insert(option.display, option);
		}
	}

	r_forced = r_result.size() > 0;
}

Error GDScriptLanguage::complete_code(const String &p_code, const String &p_path, Object *p_owner, List<ScriptCodeCompletionOption> *r_options, bool &r_forced, String &r_call_hint) {
	const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";

	GDScriptParser parser;

	parser.parse(p_code, p_path.get_base_dir(), false, p_path, true);
	r_forced = false;
	Map<String, ScriptCodeCompletionOption> options;
	GDScriptCompletionContext context;
	context._class = parser.get_completion_class();
	context.block = parser.get_completion_block();
	context.function = parser.get_completion_function();
	context.line = parser.get_completion_line();

	if (!context._class || context._class->owner == nullptr) {
		context.base = p_owner;
		context.base_path = p_path.get_base_dir();
	}

	bool is_function = false;

	switch (parser.get_completion_type()) {
		case GDScriptParser::COMPLETION_NONE: {
		} break;
		case GDScriptParser::COMPLETION_BUILT_IN_TYPE_CONSTANT: {
			List<StringName> constants;
			Variant::get_constants_for_type(parser.get_completion_built_in_constant(), &constants);
			for (List<StringName>::Element *E = constants.front(); E; E = E->next()) {
				ScriptCodeCompletionOption option(E->get().operator String(), ScriptCodeCompletionOption::KIND_CONSTANT);
				bool valid = false;
				Variant default_value = Variant::get_constant_value(parser.get_completion_built_in_constant(), E->get(), &valid);
				if (valid) {
					option.default_value = default_value;
				}
				options.insert(option.display, option);
			}
		} break;
		case GDScriptParser::COMPLETION_PARENT_FUNCTION: {
			_find_identifiers_in_class(context, !context.function || context.function->_static, true, true, options);
		} break;
		case GDScriptParser::COMPLETION_FUNCTION: {
			is_function = true;
			FALLTHROUGH;
		}
		case GDScriptParser::COMPLETION_IDENTIFIER: {
			_find_identifiers(context, is_function, options);
		} break;
		case GDScriptParser::COMPLETION_EXTENDS: {
			// Native classes.
			List<StringName> class_list;
			ClassDB::get_class_list(&class_list);
			for (int i = 0; i < class_list.size(); i++) {
				ScriptCodeCompletionOption option(class_list[i], ScriptCodeCompletionOption::KIND_CLASS);
				options.insert(option.display, option);
			}

			// GDScript classes.
			if (EditorSettings::get_singleton()->get("text_editor/completion/complete_file_paths")) {
				for (int i = 0; i < ScriptServer::get_language_count(); i++) {
					if (ScriptServer::get_language(i)->get_name() == "GDScript") {
						List<String> extensions;
						ScriptServer::get_language(i)->get_recognized_extensions(&extensions);
						for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
							_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), options, String("." + E->get()));
						}
					}
				}
				r_forced = true;
			}

			// Named Scripts.
			List<StringName> named_scripts;
			ScriptServer::get_global_class_list(&named_scripts);
			for (List<StringName>::Element *E = named_scripts.front(); E; E = E->next()) {
				ScriptCodeCompletionOption option(E->get().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
				options.insert(option.display, option);
			}
		} break;
		case GDScriptParser::COMPLETION_GET_NODE: {
			if (p_owner) {
				List<String> opts;
				p_owner->get_argument_options("get_node", 0, &opts);

				for (List<String>::Element *E = opts.front(); E; E = E->next()) {
					String opt = E->get().strip_edges();
					if (opt.is_quoted()) {
						r_forced = true;
						String idopt = opt.unquote();
						if (idopt.replace("/", "_").is_valid_identifier()) {
							ScriptCodeCompletionOption option(idopt, ScriptCodeCompletionOption::KIND_NODE_PATH);
							options.insert(option.display, option);
						} else {
							ScriptCodeCompletionOption option(opt, ScriptCodeCompletionOption::KIND_NODE_PATH);
							options.insert(option.display, option);
						}
					}
				}

				// Get autoloads
				List<PropertyInfo> props;
				ProjectSettings::get_singleton()->get_property_list(&props);

				for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
					String s = E->get().name;
					if (!s.begins_with("autoload/")) {
						continue;
					}
					String name = s.get_slice("/", 1);
					ScriptCodeCompletionOption option(quote_style + "/root/" + name + quote_style, ScriptCodeCompletionOption::KIND_NODE_PATH);
					options.insert(option.display, option);
				}
			}
		} break;
		case GDScriptParser::COMPLETION_METHOD: {
			is_function = true;
			FALLTHROUGH;
		}
		case GDScriptParser::COMPLETION_INDEX: {
			const GDScriptParser::Node *node = parser.get_completion_node();
			if (node->type != GDScriptParser::Node::TYPE_OPERATOR) {
				break;
			}
			const GDScriptParser::OperatorNode *op = static_cast<const GDScriptParser::OperatorNode *>(node);
			if (op->arguments.size() < 1) {
				break;
			}

			GDScriptCompletionIdentifier base;
			if (!_guess_expression_type(context, op->arguments[0], base)) {
				break;
			}

			GDScriptCompletionContext c = context;
			c.function = nullptr;
			c.block = nullptr;
			c.base = base.value.get_type() == Variant::OBJECT ? base.value.operator Object *() : nullptr;
			if (base.type.kind == GDScriptParser::DataType::CLASS) {
				c._class = base.type.class_type;
			} else {
				c._class = nullptr;
			}

			_find_identifiers_in_base(c, base, is_function, options);
		} break;
		case GDScriptParser::COMPLETION_CALL_ARGUMENTS: {
			_find_call_arguments(context, parser.get_completion_node(), parser.get_completion_argument_index(), options, r_forced, r_call_hint);
		} break;
		case GDScriptParser::COMPLETION_VIRTUAL_FUNC: {
			GDScriptParser::DataType native_type = context._class->base_type;
			while (native_type.has_type && native_type.kind != GDScriptParser::DataType::NATIVE) {
				switch (native_type.kind) {
					case GDScriptParser::DataType::CLASS: {
						native_type = native_type.class_type->base_type;
					} break;
					case GDScriptParser::DataType::GDSCRIPT: {
						Ref<GDScript> gds = native_type.script_type;
						if (gds.is_valid()) {
							Ref<GDScript> base = gds->get_base_script();
							if (base.is_valid()) {
								native_type.script_type = base;
							} else {
								native_type.native_type = gds->get_instance_base_type();
								native_type.kind = GDScriptParser::DataType::NATIVE;
							}
						} else {
							native_type.has_type = false;
						}
					} break;
					default: {
						native_type.has_type = false;
					} break;
				}
			}

			if (!native_type.has_type) {
				break;
			}

			StringName class_name = native_type.native_type;
			if (!ClassDB::class_exists(class_name)) {
				class_name = String("_") + class_name;
				if (!ClassDB::class_exists(class_name)) {
					break;
				}
			}

			bool use_type_hint = EditorSettings::get_singleton()->get_setting("text_editor/completion/add_type_hints").operator bool();

			List<MethodInfo> virtual_methods;
			ClassDB::get_virtual_methods(class_name, &virtual_methods);
			for (List<MethodInfo>::Element *E = virtual_methods.front(); E; E = E->next()) {
				MethodInfo &mi = E->get();
				String method_hint = mi.name;
				if (method_hint.find(":") != -1) {
					method_hint = method_hint.get_slice(":", 0);
				}
				method_hint += "(";

				if (mi.arguments.size()) {
					for (int i = 0; i < mi.arguments.size(); i++) {
						if (i > 0) {
							method_hint += ", ";
						}
						String arg = mi.arguments[i].name;
						if (arg.find(":") != -1) {
							arg = arg.substr(0, arg.find(":"));
						}
						method_hint += arg;
						if (use_type_hint && mi.arguments[i].type != Variant::NIL) {
							method_hint += ": ";
							if (mi.arguments[i].type == Variant::OBJECT && mi.arguments[i].class_name != StringName()) {
								method_hint += mi.arguments[i].class_name.operator String();
							} else {
								method_hint += Variant::get_type_name(mi.arguments[i].type);
							}
						}
					}
				}
				method_hint += ")";
				if (use_type_hint && (mi.return_val.type != Variant::NIL || !(mi.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT))) {
					method_hint += " -> ";
					if (mi.return_val.type == Variant::NIL) {
						method_hint += "void";
					} else if (mi.return_val.type == Variant::OBJECT && mi.return_val.class_name != StringName()) {
						method_hint += mi.return_val.class_name.operator String();
					} else {
						method_hint += Variant::get_type_name(mi.return_val.type);
					}
				}
				method_hint += ":";

				ScriptCodeCompletionOption option(method_hint, ScriptCodeCompletionOption::KIND_FUNCTION);
				options.insert(option.display, option);
			}
		} break;
		case GDScriptParser::COMPLETION_YIELD: {
			const GDScriptParser::Node *node = parser.get_completion_node();

			GDScriptCompletionContext c = context;
			c.line = node->line;
			GDScriptCompletionIdentifier type;
			if (!_guess_expression_type(c, node, type)) {
				break;
			}

			GDScriptParser::DataType base_type = type.type;
			while (base_type.has_type) {
				switch (base_type.kind) {
					case GDScriptParser::DataType::CLASS: {
						for (int i = 0; i < base_type.class_type->_signals.size(); i++) {
							ScriptCodeCompletionOption option(base_type.class_type->_signals[i].name.operator String(), ScriptCodeCompletionOption::KIND_SIGNAL);
							option.insert_text = quote_style + option.display + quote_style;
							options.insert(option.display, option);
						}
						base_type = base_type.class_type->base_type;
					} break;
					case GDScriptParser::DataType::SCRIPT:
					case GDScriptParser::DataType::GDSCRIPT: {
						Ref<Script> scr = base_type.script_type;
						if (scr.is_valid()) {
							List<MethodInfo> signals;
							scr->get_script_signal_list(&signals);
							for (List<MethodInfo>::Element *E = signals.front(); E; E = E->next()) {
								ScriptCodeCompletionOption option(quote_style + E->get().name + quote_style, ScriptCodeCompletionOption::KIND_SIGNAL);
								options.insert(option.display, option);
							}
							Ref<Script> base_script = scr->get_base_script();
							if (base_script.is_valid()) {
								base_type.script_type = base_script;
							} else {
								base_type.kind = GDScriptParser::DataType::NATIVE;
								base_type.native_type = scr->get_instance_base_type();
							}
						} else {
							base_type.has_type = false;
						}
					} break;
					case GDScriptParser::DataType::NATIVE: {
						base_type.has_type = false;

						StringName class_name = base_type.native_type;
						if (!ClassDB::class_exists(class_name)) {
							class_name = String("_") + class_name;
							if (!ClassDB::class_exists(class_name)) {
								break;
							}
						}

						List<MethodInfo> signals;
						ClassDB::get_signal_list(class_name, &signals);
						for (List<MethodInfo>::Element *E = signals.front(); E; E = E->next()) {
							ScriptCodeCompletionOption option(quote_style + E->get().name + quote_style, ScriptCodeCompletionOption::KIND_SIGNAL);
							options.insert(option.display, option);
						}
					} break;
					default: {
						base_type.has_type = false;
					}
				}
			}
		} break;
		case GDScriptParser::COMPLETION_RESOURCE_PATH: {
			if (EditorSettings::get_singleton()->get("text_editor/completion/complete_file_paths")) {
				_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), options);
				r_forced = true;
			}
		} break;
		case GDScriptParser::COMPLETION_ASSIGN: {
			GDScriptCompletionIdentifier type;
			if (!_guess_expression_type(context, parser.get_completion_node(), type)) {
				break;
			}

			if (!type.enumeration.empty()) {
				_find_enumeration_candidates(type.enumeration, options);
				r_forced = options.size() > 0;
			}
		} break;
		case GDScriptParser::COMPLETION_TYPE_HINT: {
			const GDScriptParser::ClassNode *clss = context._class;
			while (clss) {
				for (Map<StringName, GDScriptParser::ClassNode::Constant>::Element *E = clss->constant_expressions.front(); E; E = E->next()) {
					GDScriptCompletionIdentifier constant;
					GDScriptCompletionContext c = context;
					c.function = nullptr;
					c.block = nullptr;
					c.line = E->value().expression->line;
					if (_guess_expression_type(c, E->value().expression, constant)) {
						if (constant.type.has_type && constant.type.is_meta_type) {
							ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
							options.insert(option.display, option);
						}
					}
				}
				for (int i = 0; i < clss->subclasses.size(); i++) {
					if (clss->subclasses[i]->name != StringName()) {
						ScriptCodeCompletionOption option(clss->subclasses[i]->name.operator String(), ScriptCodeCompletionOption::KIND_CLASS);
						options.insert(option.display, option);
					}
				}
				clss = clss->owner;
				for (int i = 0; i < Variant::VARIANT_MAX; i++) {
					ScriptCodeCompletionOption option(Variant::get_type_name((Variant::Type)i), ScriptCodeCompletionOption::KIND_CLASS);
					options.insert(option.display, option);
				}
				List<PropertyInfo> props;
				ProjectSettings::get_singleton()->get_property_list(&props);
				for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
					String s = E->get().name;
					if (!s.begins_with("autoload/")) {
						continue;
					}
					ScriptCodeCompletionOption option(s.get_slice("/", 1), ScriptCodeCompletionOption::KIND_CLASS);
					options.insert(option.display, option);
				}
			}

			List<StringName> native_classes;
			ClassDB::get_class_list(&native_classes);
			for (List<StringName>::Element *E = native_classes.front(); E; E = E->next()) {
				String class_name = E->get().operator String();
				if (class_name.begins_with("_")) {
					class_name = class_name.right(1);
				}
				if (Engine::get_singleton()->has_singleton(class_name)) {
					continue;
				}
				ScriptCodeCompletionOption option(class_name, ScriptCodeCompletionOption::KIND_CLASS);
				options.insert(option.display, option);
			}

			// Named scripts
			List<StringName> named_scripts;
			ScriptServer::get_global_class_list(&named_scripts);
			for (List<StringName>::Element *E = named_scripts.front(); E; E = E->next()) {
				ScriptCodeCompletionOption option(E->get().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
				options.insert(option.display, option);
			}

			if (parser.get_completion_identifier_is_function()) {
				ScriptCodeCompletionOption option("void", ScriptCodeCompletionOption::KIND_PLAIN_TEXT);
				options.insert(option.display, option);
			}
			r_forced = true;
		} break;
		case GDScriptParser::COMPLETION_TYPE_HINT_INDEX: {
			GDScriptCompletionIdentifier base;
			String index = parser.get_completion_cursor().operator String();
			if (!_guess_identifier_type(context, index.get_slice(".", 0), base)) {
				break;
			}

			GDScriptCompletionContext c = context;
			c._class = nullptr;
			c.function = nullptr;
			c.block = nullptr;
			bool finding = true;
			index = index.right(index.find(".") + 1);
			while (index.find(".") != -1) {
				String id = index.get_slice(".", 0);

				GDScriptCompletionIdentifier sub_base;
				if (!_guess_identifier_type_from_base(c, base, id, sub_base)) {
					finding = false;
					break;
				}
				index = index.right(index.find(".") + 1);
				base = sub_base;
			}

			if (!finding) {
				break;
			}

			GDScriptParser::DataType base_type = base.type;
			while (base_type.has_type) {
				switch (base_type.kind) {
					case GDScriptParser::DataType::CLASS: {
						if (base_type.class_type) {
							for (Map<StringName, GDScriptParser::ClassNode::Constant>::Element *E = base_type.class_type->constant_expressions.front(); E; E = E->next()) {
								GDScriptCompletionIdentifier constant;
								GDScriptCompletionContext c2 = context;
								c2._class = base_type.class_type;
								c2.function = nullptr;
								c2.block = nullptr;
								c2.line = E->value().expression->line;
								if (_guess_expression_type(c2, E->value().expression, constant)) {
									if (constant.type.has_type && constant.type.is_meta_type) {
										ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
										options.insert(option.display, option);
									}
								}
							}
							for (int i = 0; i < base_type.class_type->subclasses.size(); i++) {
								if (base_type.class_type->subclasses[i]->name != StringName()) {
									ScriptCodeCompletionOption option(base_type.class_type->subclasses[i]->name.operator String(), ScriptCodeCompletionOption::KIND_CLASS);
									options.insert(option.display, option);
								}
							}

							base_type = base_type.class_type->base_type;
						} else {
							base_type.has_type = false;
						}
					} break;
					case GDScriptParser::DataType::GDSCRIPT: {
						Ref<GDScript> scr = base_type.script_type;
						if (scr.is_valid()) {
							for (const Map<StringName, Ref<GDScript>>::Element *E = scr->get_subclasses().front(); E; E = E->next()) {
								ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
								options.insert(option.display, option);
							}
						}
						FALLTHROUGH;
					}
					case GDScriptParser::DataType::SCRIPT: {
						Ref<Script> scr = base_type.script_type;
						if (scr.is_valid()) {
							Map<StringName, Variant> constants;
							scr->get_constants(&constants);
							for (Map<StringName, Variant>::Element *E = constants.front(); E; E = E->next()) {
								Ref<Script> const_scr = E->value();
								if (const_scr.is_valid()) {
									ScriptCodeCompletionOption option(E->key().operator String(), ScriptCodeCompletionOption::KIND_CLASS);
									options.insert(option.display, option);
								}
							}
							Ref<Script> base_script = scr->get_base_script();
							if (base_script.is_valid()) {
								base_type.script_type = base_script;
							} else {
								base_type.has_type = false;
							}
						} else {
							base_type.has_type = false;
						}
					} break;
					default: {
						base_type.has_type = false;
					} break;
				}
			}
			r_forced = options.size() > 0;
		} break;
	}

	for (Map<String, ScriptCodeCompletionOption>::Element *E = options.front(); E; E = E->next()) {
		r_options->push_back(E->get());
	}

	return OK;
}

#else

Error GDScriptLanguage::complete_code(const String &p_code, const String &p_path, Object *p_owner, List<ScriptCodeCompletionOption> *r_options, bool &r_forced, String &r_call_hint) {
	return OK;
}

#endif

//////// END COMPLETION //////////

String GDScriptLanguage::_get_indentation() const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		bool use_space_indentation = EDITOR_GET("text_editor/indent/type");

		if (use_space_indentation) {
			int indent_size = EDITOR_GET("text_editor/indent/size");

			String space_indent = "";
			for (int i = 0; i < indent_size; i++) {
				space_indent += " ";
			}
			return space_indent;
		}
	}
#endif
	return "\t";
}

#ifdef TOOLS_ENABLED
String GDScriptLanguage::get_current_system_date() const {
	time_t currTime = time(NULL);
	struct tm buf;
	char dateString[100];
	time(&currTime);
	localtime_s(&buf, &currTime);
	strftime(dateString, 100, "%d/%m/%Y", &buf);
	return dateString;
}

String GDScriptLanguage::get_current_app_user() const {
	return (String)ProjectSettings::get_singleton()->get("application/config/user");
}

String GDScriptLanguage::get_current_user_company() const {
	return (String)ProjectSettings::get_singleton()->get("application/config/company_name");
}

String GDScriptLanguage::get_current_project_name() const {
	return (String)ProjectSettings::get_singleton()->get("application/config/project_name");
}

String GDScriptLanguage::get_current_project_version() const {
	return (String)ProjectSettings::get_singleton()->get("application/config/version");
}
#endif

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
		if (st == "" || st.begins_with("#")) {
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
				indent_stack.push_back(tc); //this is not right but gets the job done
			}
		}

		if (i >= p_from_line) {
			l = "";
			for (int j = 0; j < indent_stack.size(); j++) {
				l += indent;
			}
			l += st;

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

	while (base_type.has_type) {
		switch (base_type.kind) {
			case GDScriptParser::DataType::CLASS: {
				if (base_type.class_type) {
					if (p_is_function) {
						for (int i = 0; i < base_type.class_type->functions.size(); i++) {
							if (base_type.class_type->functions[i]->name == p_symbol) {
								r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
								r_result.location = base_type.class_type->functions[i]->line;
								return OK;
							}
						}
						for (int i = 0; i < base_type.class_type->static_functions.size(); i++) {
							if (base_type.class_type->static_functions[i]->name == p_symbol) {
								r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
								r_result.location = base_type.class_type->static_functions[i]->line;
								return OK;
							}
						}
					} else {
						if (base_type.class_type->constant_expressions.has(p_symbol)) {
							r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
							r_result.location = base_type.class_type->constant_expressions[p_symbol].expression->line;
							return OK;
						}

						for (int i = 0; i < base_type.class_type->variables.size(); i++) {
							if (base_type.class_type->variables[i].identifier == p_symbol) {
								r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
								r_result.location = base_type.class_type->variables[i].line;
								return OK;
							}
						}

						for (int i = 0; i < base_type.class_type->subclasses.size(); i++) {
							if (base_type.class_type->subclasses[i]->name == p_symbol) {
								r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
								r_result.location = base_type.class_type->subclasses[i]->line;
								return OK;
							}
						}
					}
					base_type = base_type.class_type->base_type;
				}
			} break;
			case GDScriptParser::DataType::SCRIPT:
			case GDScriptParser::DataType::GDSCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					int line = scr->get_member_line(p_symbol);
					if (line >= 0) {
						r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
						r_result.location = line;
						r_result.script = scr;
						return OK;
					}
					Ref<Script> base_script = scr->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = GDScriptParser::DataType::NATIVE;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					base_type.has_type = false;
				}
			} break;
			case GDScriptParser::DataType::NATIVE: {
				StringName class_name = base_type.native_type;
				if (!ClassDB::class_exists(class_name)) {
					class_name = String("_") + class_name;
					if (!ClassDB::class_exists(class_name)) {
						base_type.has_type = false;
						break;
					}
				}

				if (ClassDB::has_method(class_name, p_symbol, true)) {
					r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_METHOD;
					r_result.class_name = base_type.native_type;
					r_result.class_member = p_symbol;
					return OK;
				}

				List<MethodInfo> virtual_methods;
				ClassDB::get_virtual_methods(class_name, &virtual_methods, true);
				for (List<MethodInfo>::Element *E = virtual_methods.front(); E; E = E->next()) {
					if (E->get().name == p_symbol) {
						r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_METHOD;
						r_result.class_name = base_type.native_type;
						r_result.class_member = p_symbol;
						return OK;
					}
				}

				StringName enum_name = ClassDB::get_integer_constant_enum(class_name, p_symbol, true);
				if (enum_name != StringName()) {
					r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_ENUM;
					r_result.class_name = base_type.native_type;
					r_result.class_member = enum_name;
					return OK;
				}

				List<String> constants;
				ClassDB::get_integer_constant_list(class_name, &constants, true);
				for (List<String>::Element *E = constants.front(); E; E = E->next()) {
					if (E->get() == p_symbol) {
						r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_CONSTANT;
						r_result.class_name = base_type.native_type;
						r_result.class_member = p_symbol;
						return OK;
					}
				}

				List<PropertyInfo> properties;
				ClassDB::get_property_list(class_name, &properties, true);
				for (List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
					if (E->get().name == p_symbol) {
						r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_PROPERTY;
						r_result.class_name = base_type.native_type;
						r_result.class_member = p_symbol;
						return OK;
					}
				}

				StringName parent = ClassDB::get_parent_class(class_name);
				if (parent != StringName()) {
					if (String(parent).begins_with("_")) {
						base_type.native_type = String(parent).right(1);
					} else {
						base_type.native_type = parent;
					}
				} else {
					base_type.has_type = false;
				}
			} break;
			case GDScriptParser::DataType::BUILTIN: {
				base_type.has_type = false;

				if (Variant::has_constant(base_type.builtin_type, p_symbol)) {
					r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_CONSTANT;
					r_result.class_name = Variant::get_type_name(base_type.builtin_type);
					r_result.class_member = p_symbol;
					return OK;
				}

				Variant v;
				REF v_ref;
				if (base_type.builtin_type == Variant::OBJECT) {
					v_ref.instance();
					v = v_ref;
				} else {
					Variant::CallError err;
					v = Variant::construct(base_type.builtin_type, nullptr, 0, err);
					if (err.error != Variant::CallError::CALL_OK) {
						break;
					}
				}

				if (v.has_method(p_symbol)) {
					r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_METHOD;
					r_result.class_name = Variant::get_type_name(base_type.builtin_type);
					r_result.class_member = p_symbol;
					return OK;
				}

				bool valid = false;
				v.get(p_symbol, &valid);
				if (valid) {
					r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_PROPERTY;
					r_result.class_name = Variant::get_type_name(base_type.builtin_type);
					r_result.class_member = p_symbol;
					return OK;
				}
			} break;
			default: {
				base_type.has_type = false;
			} break;
		}
	}

	return ERR_CANT_RESOLVE;
}

Error GDScriptLanguage::lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner, LookupResult &r_result) {
	// Before parsing, try the usual stuff.
	if (ClassDB::class_exists(p_symbol)) {
		r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS;
		r_result.class_name = p_symbol;
		return OK;
	} else {
		String under_prefix = "_" + p_symbol;
		if (ClassDB::class_exists(under_prefix)) {
			r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS;
			r_result.class_name = p_symbol;
			return OK;
		}
	}

	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		Variant::Type t = Variant::Type(i);
		if (Variant::get_type_name(t) == p_symbol) {
			r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS;
			r_result.class_name = Variant::get_type_name(t);
			return OK;
		}
	}

	if ("PI" == p_symbol || "TAU" == p_symbol || "INF" == p_symbol || "NAN" == p_symbol) {
		r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_CONSTANT;
		r_result.class_name = "@GDScript";
		r_result.class_member = p_symbol;
		return OK;
	}

	GDScriptParser parser;
	parser.parse(p_code, p_path.get_base_dir(), false, p_path, true);

	if (parser.get_completion_type() == GDScriptParser::COMPLETION_NONE) {
		return ERR_CANT_RESOLVE;
	}

	GDScriptCompletionContext context;
	context._class = parser.get_completion_class();
	context.function = parser.get_completion_function();
	context.block = parser.get_completion_block();
	context.line = parser.get_completion_line();
	context.base = p_owner;
	context.base_path = p_path.get_base_dir();

	if (context._class && context._class->extends_class.size() > 0) {
		bool success = false;
		ClassDB::get_integer_constant(context._class->extends_class[0], p_symbol, &success);
		if (success) {
			r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_CONSTANT;
			r_result.class_name = context._class->extends_class[0];
			r_result.class_member = p_symbol;
			return OK;
		}
	}

	bool is_function = false;

	switch (parser.get_completion_type()) {
		case GDScriptParser::COMPLETION_BUILT_IN_TYPE_CONSTANT: {
			r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_CONSTANT;
			r_result.class_name = Variant::get_type_name(parser.get_completion_built_in_constant());
			r_result.class_member = p_symbol;
			return OK;
		} break;
		case GDScriptParser::COMPLETION_PARENT_FUNCTION:
		case GDScriptParser::COMPLETION_FUNCTION: {
			is_function = true;
			FALLTHROUGH;
		}
		case GDScriptParser::COMPLETION_IDENTIFIER: {
			if (!is_function) {
				is_function = parser.get_completion_identifier_is_function();
			}

			GDScriptParser::DataType base_type;
			if (context._class) {
				if (parser.get_completion_type() != GDScriptParser::COMPLETION_PARENT_FUNCTION) {
					base_type.has_type = true;
					base_type.kind = GDScriptParser::DataType::CLASS;
					base_type.class_type = const_cast<GDScriptParser::ClassNode *>(context._class);
				} else {
					base_type = context._class->base_type;
				}
			} else {
				break;
			}

			if (!is_function && context.block) {
				// Lookup local variables
				const GDScriptParser::BlockNode *block = context.block;
				while (block) {
					if (block->variables.has(p_symbol)) {
						r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
						r_result.location = block->variables[p_symbol]->line;
						return OK;
					}
					block = block->parent_block;
				}
			}

			if (context.function && context.function->name != StringName()) {
				// Lookup function arguments
				for (int i = 0; i < context.function->arguments.size(); i++) {
					if (context.function->arguments[i] == p_symbol) {
						r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
						r_result.location = context.function->line;
						return OK;
					}
				}
			}

			if (_lookup_symbol_from_base(base_type, p_symbol, is_function, r_result) == OK) {
				return OK;
			}

			if (!is_function) {
				// Guess in autoloads as singletons
				List<PropertyInfo> props;
				ProjectSettings::get_singleton()->get_property_list(&props);

				for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
					String s = E->get().name;
					if (!s.begins_with("autoload/")) {
						continue;
					}
					String name = s.get_slice("/", 1);
					if (name == String(p_symbol)) {
						String path = ProjectSettings::get_singleton()->get(s);
						if (path.begins_with("*")) {
							String script = path.substr(1, path.length());

							if (!script.ends_with(".gd")) {
								// Not a script, try find the script anyway,
								// may have some success
								script = script.get_basename() + ".gd";
							}

							if (FileAccess::exists(script)) {
								r_result.type = ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION;
								r_result.location = 0;
								r_result.script = ResourceLoader::load(script);
								return OK;
							}
						}
					}
				}

				// Global
				Map<StringName, int> classes = GDScriptLanguage::get_singleton()->get_global_map();
				if (classes.has(p_symbol)) {
					Variant value = GDScriptLanguage::get_singleton()->get_global_array()[classes[p_symbol]];
					if (value.get_type() == Variant::OBJECT) {
						Object *obj = value;
						if (obj) {
							if (Object::cast_to<GDScriptNativeClass>(obj)) {
								r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS;
								r_result.class_name = Object::cast_to<GDScriptNativeClass>(obj)->get_name();
							} else {
								r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS;
								r_result.class_name = obj->get_class();
							}

							// proxy class remove the underscore.
							if (r_result.class_name.begins_with("_")) {
								r_result.class_name = r_result.class_name.right(1);
							}
							return OK;
						}
					} else {
						/*
						// Because get_integer_constant_enum and get_integer_constant don't work on @GlobalScope
						// We cannot determine the exact nature of the identifier here
						// Otherwise these codes would work
						StringName enumName = ClassDB::get_integer_constant_enum("@GlobalScope", p_symbol, true);
						if (enumName != NULL) {
							r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_ENUM;
							r_result.class_name = "@GlobalScope";
							r_result.class_member = enumName;
							return OK;
						}
						else {
							r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_CONSTANT;
							r_result.class_name = "@GlobalScope";
							r_result.class_member = p_symbol;
							return OK;
						}*/
						r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_TBD_GLOBALSCOPE;
						r_result.class_name = "@GlobalScope";
						r_result.class_member = p_symbol;
						return OK;
					}
				}
			}
		} break;
		case GDScriptParser::COMPLETION_METHOD: {
			is_function = true;
			FALLTHROUGH;
		}
		case GDScriptParser::COMPLETION_INDEX: {
			const GDScriptParser::Node *node = parser.get_completion_node();
			if (node->type != GDScriptParser::Node::TYPE_OPERATOR) {
				break;
			}
			GDScriptCompletionIdentifier base;
			if (!_guess_expression_type(context, static_cast<const GDScriptParser::OperatorNode *>(node)->arguments[0], base)) {
				break;
			}

			if (_lookup_symbol_from_base(base.type, p_symbol, is_function, r_result) == OK) {
				return OK;
			}
		} break;
		case GDScriptParser::COMPLETION_VIRTUAL_FUNC: {
			GDScriptParser::DataType base_type = context._class->base_type;

			if (_lookup_symbol_from_base(base_type, p_symbol, true, r_result) == OK) {
				return OK;
			}
		} break;
		case GDScriptParser::COMPLETION_TYPE_HINT: {
			GDScriptParser::DataType base_type = context._class->base_type;
			base_type.has_type = true;
			base_type.kind = GDScriptParser::DataType::CLASS;
			base_type.class_type = const_cast<GDScriptParser::ClassNode *>(context._class);

			if (_lookup_symbol_from_base(base_type, p_symbol, false, r_result) == OK) {
				return OK;
			}
		} break;
		default: {
		}
	}

	// Must check GDScript functions after parsing to ensure functions like Array.max() are handled correctly.
	bool function_in_gdscript_functions = false;
	for (int i = 0; i < GDScriptFunctions::FUNC_MAX; i++) {
		if (GDScriptFunctions::get_func_name(GDScriptFunctions::Function(i)) == p_symbol) {
			function_in_gdscript_functions = true;
			break;
		}
	}

	// Need special checks for assert, preload, and yield as they are technically
	// keywords, so are not registered in GDScriptFunctions.
	if (function_in_gdscript_functions || "yield" == p_symbol || "assert" == p_symbol || "preload" == p_symbol) {
		r_result.type = ScriptLanguage::LookupResult::RESULT_CLASS_METHOD;
		r_result.class_name = "@GDScript";
		r_result.class_member = p_symbol;
		return OK;
	}

	return ERR_CANT_RESOLVE;
}

#endif
