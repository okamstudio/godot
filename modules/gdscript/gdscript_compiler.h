/**************************************************************************/
/*  gdscript_compiler.h                                                   */
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

#pragma once

#include "gdscript.h"
#include "gdscript_codegen.h"
#include "gdscript_function.h"
#include "gdscript_parser.h"

#include "core/templates/hash_set.h"

class GDScriptCompiler {
	const GDScriptParser *parser = nullptr;
	HashSet<GDScript *> parsed_classes;
	HashSet<GDScript *> parsing_classes;
	GDScript *main_script = nullptr;

	struct FunctionLambdaInfo {
		GDScriptFunction *function = nullptr;
		GDScriptFunction *parent = nullptr;
		GDScript *script = nullptr;
		StringName name;
		int line = 0;
		int index = 0;
		int depth = 0;
		//uint64_t code_hash;
		//int code_size;
		int capture_count = 0;
		bool use_self = false;
		int arg_count = 0;
		int default_arg_count = 0;
		//Vector<GDScriptDataType> argument_types;
		//GDScriptDataType return_type;
		Vector<FunctionLambdaInfo> sublambdas;
	};

	struct ScriptLambdaInfo {
		Vector<FunctionLambdaInfo> implicit_initializer_info;
		Vector<FunctionLambdaInfo> implicit_ready_info;
		Vector<FunctionLambdaInfo> static_initializer_info;
		HashMap<StringName, Vector<FunctionLambdaInfo>> member_function_infos;
		Vector<FunctionLambdaInfo> other_function_infos;
		HashMap<StringName, ScriptLambdaInfo> subclass_info;
	};

	struct CodeGen {
		GDScript *script = nullptr;
		const GDScriptParser::ClassNode *class_node = nullptr;
		const GDScriptParser::FunctionNode *function_node = nullptr;
		StringName function_name;
		GDScriptCodeGenerator *generator = nullptr;
		HashMap<StringName, GDScriptCodeGenerator::Address> parameters;
		HashMap<StringName, GDScriptCodeGenerator::Address> locals;
		List<HashMap<StringName, GDScriptCodeGenerator::Address>> locals_stack;
		bool is_static = false;

		GDScriptCodeGenerator::Address add_local(const StringName &p_name, const GDScriptDataType &p_type) {
			uint32_t addr = generator->add_local(p_name, p_type);
			locals[p_name] = GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::LOCAL_VARIABLE, addr, p_type);
			return locals[p_name];
		}

		GDScriptCodeGenerator::Address add_local_constant(const StringName &p_name, const Variant &p_value) {
			uint32_t addr = generator->add_local_constant(p_name, p_value);
			locals[p_name] = GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::CONSTANT, addr);
			return locals[p_name];
		}

		GDScriptCodeGenerator::Address add_temporary(const GDScriptDataType &p_type = GDScriptDataType()) {
			uint32_t addr = generator->add_temporary(p_type);
			return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::TEMPORARY, addr, p_type);
		}

		GDScriptCodeGenerator::Address add_constant(const Variant &p_constant) {
			GDScriptDataType type;
			type.has_type = true;
			type.kind = GDScriptDataType::BUILTIN;
			type.builtin_type = p_constant.get_type();
			if (type.builtin_type == Variant::OBJECT) {
				Object *obj = p_constant;
				if (obj) {
					type.kind = GDScriptDataType::NATIVE;
					type.native_type = obj->get_class_name();

					Ref<Script> scr = obj->get_script();
					if (scr.is_valid()) {
						type.script_type = scr.ptr();
						Ref<GDScript> gdscript = scr;
						if (gdscript.is_valid()) {
							type.kind = GDScriptDataType::GDSCRIPT;
						} else {
							type.kind = GDScriptDataType::SCRIPT;
						}
					}
				} else {
					type.builtin_type = Variant::NIL;
				}
			}

			uint32_t addr = generator->add_or_get_constant(p_constant);
			return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::CONSTANT, addr, type);
		}

		void start_block() {
			HashMap<StringName, GDScriptCodeGenerator::Address> old_locals = locals;
			locals_stack.push_back(old_locals);
			generator->start_block();
		}

		void end_block() {
			locals = locals_stack.back()->get();
			locals_stack.pop_back();
			generator->end_block();
		}
	};

	bool _is_class_member_property(CodeGen &codegen, const StringName &p_name);
	bool _is_class_member_property(GDScript *owner, const StringName &p_name);
	bool _is_local_or_parameter(CodeGen &codegen, const StringName &p_name);

	void _set_error(const String &p_error, const GDScriptParser::Node *p_node);

	GDScriptDataType _gdtype_from_datatype(const GDScriptParser::DataType &p_datatype, GDScript *p_owner, bool p_handle_metatype = true);

	GDScriptCodeGenerator::Address _parse_expression(CodeGen &codegen, Error &r_error, const GDScriptParser::ExpressionNode *p_expression, bool p_root = false, bool p_initializer = false);
	GDScriptCodeGenerator::Address _parse_match_pattern(CodeGen &codegen, Error &r_error, const GDScriptParser::PatternNode *p_pattern, const GDScriptCodeGenerator::Address &p_value_addr, const GDScriptCodeGenerator::Address &p_type_addr, const GDScriptCodeGenerator::Address &p_previous_test, bool p_is_first, bool p_is_nested);
	List<GDScriptCodeGenerator::Address> _add_block_locals(CodeGen &codegen, const GDScriptParser::SuiteNode *p_block);
	void _clear_block_locals(CodeGen &codegen, const List<GDScriptCodeGenerator::Address> &p_locals);
	Error _parse_block(CodeGen &codegen, const GDScriptParser::SuiteNode *p_block, bool p_add_locals = true, bool p_clear_locals = true);
	GDScriptFunction *_parse_function(Error &r_error, GDScript *p_script, const GDScriptParser::ClassNode *p_class, const GDScriptParser::FunctionNode *p_func, bool p_for_ready = false, bool p_for_lambda = false);
	GDScriptFunction *_make_static_initializer(Error &r_error, GDScript *p_script, const GDScriptParser::ClassNode *p_class);
	Error _parse_setter_getter(GDScript *p_script, const GDScriptParser::ClassNode *p_class, const GDScriptParser::VariableNode *p_variable, bool p_is_setter);
	Error _prepare_compilation(GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state);
	Error _compile_class(GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state);
	FunctionLambdaInfo _get_function_replacement_info(GDScriptFunction *p_func, int p_index = -1, int p_depth = 0, GDScriptFunction *p_parent_func = nullptr);
	Vector<FunctionLambdaInfo> _get_function_lambda_replacement_info(GDScriptFunction *p_func, int p_depth = 0, GDScriptFunction *p_parent_func = nullptr);
	ScriptLambdaInfo _get_script_lambda_replacement_info(GDScript *p_script);
	bool _do_function_infos_match(const FunctionLambdaInfo &p_old_info, const FunctionLambdaInfo *p_new_info);
	void _get_function_ptr_replacements(HashMap<GDScriptFunction *, GDScriptFunction *> &r_replacements, const FunctionLambdaInfo &p_old_info, const FunctionLambdaInfo *p_new_info);
	void _get_function_ptr_replacements(HashMap<GDScriptFunction *, GDScriptFunction *> &r_replacements, const Vector<FunctionLambdaInfo> &p_old_infos, const Vector<FunctionLambdaInfo> *p_new_infos);
	void _get_function_ptr_replacements(HashMap<GDScriptFunction *, GDScriptFunction *> &r_replacements, const ScriptLambdaInfo &p_old_info, const ScriptLambdaInfo *p_new_info);
	int err_line = 0;
	int err_column = 0;
	StringName source;
	String error;
	GDScriptParser::ExpressionNode *awaited_node = nullptr;
	bool has_static_data = false;

public:
	static void convert_to_initializer_type(Variant &p_variant, const GDScriptParser::VariableNode *p_node);
	static void make_scripts(GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state);
	Error compile(const GDScriptParser *p_parser, GDScript *p_script, bool p_keep_state = false);

	String get_error() const;
	int get_error_line() const;
	int get_error_column() const;

	GDScriptCompiler();
};
