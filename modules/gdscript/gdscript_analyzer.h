/**************************************************************************/
/*  gdscript_analyzer.h                                                   */
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

#include "gdscript_cache.h"
#include "gdscript_parser.h"

#include "core/object/object.h"
#include "core/object/ref_counted.h"

class GDScriptAnalyzer {
	GDScriptParser *parser = nullptr;

	template <typename Fn>
	class Finally {
		Fn fn;

	public:
		Finally(Fn p_fn) :
				fn(p_fn) {}
		~Finally() {
			fn();
		}
	};

	const GDScriptParser::EnumNode *current_enum = nullptr;
	GDScriptParser::LambdaNode *current_lambda = nullptr;
	List<GDScriptParser::LambdaNode *> pending_body_resolution_lambdas;
	HashMap<const GDScriptParser::ClassNode *, Ref<GDScriptParserRef>> external_class_parser_cache;
	bool static_context = false;

	// Tests for detecting invalid overloading of script members
	static _FORCE_INLINE_ bool has_member_name_conflict_in_script_class(const StringName &p_name, const GDScriptParser::ClassNode *p_current_class_node, const GDScriptParser::Node *p_member);
	static _FORCE_INLINE_ bool has_member_name_conflict_in_native_type(const StringName &p_name, const StringName &p_native_type_string);
	Error check_native_member_name_conflict(const StringName &p_member_name, const GDScriptParser::Node *p_member_node, const StringName &p_native_type_string);
	Error check_class_member_name_conflict(const GDScriptParser::ClassNode *p_class_node, const StringName &p_member_name, const GDScriptParser::Node *p_member_node);

	void get_class_node_current_scope_classes(GDScriptParser::ClassNode *p_node, List<GDScriptParser::ClassNode *> *p_list, GDScriptParser::Node *p_source);

	Error resolve_class_inheritance(GDScriptParser::ClassNode *p_class, const GDScriptParser::Node *p_source = nullptr);
	Error resolve_class_inheritance(GDScriptParser::ClassNode *p_class, bool p_recursive);
	GDScriptParser::DataType resolve_datatype(GDScriptParser::TypeNode *p_type);

	void decide_suite_type(GDScriptParser::Node *p_suite, GDScriptParser::Node *p_statement);

	void resolve_annotation(GDScriptParser::AnnotationNode *p_annotation);
	void resolve_class_member(GDScriptParser::ClassNode *p_class, const StringName &p_name, const GDScriptParser::Node *p_source = nullptr);
	void resolve_class_member(GDScriptParser::ClassNode *p_class, int p_index, const GDScriptParser::Node *p_source = nullptr);
	void resolve_class_interface(GDScriptParser::ClassNode *p_class, const GDScriptParser::Node *p_source = nullptr);
	void resolve_class_interface(GDScriptParser::ClassNode *p_class, bool p_recursive);
	void resolve_class_body(GDScriptParser::ClassNode *p_class, const GDScriptParser::Node *p_source = nullptr);
	void resolve_class_body(GDScriptParser::ClassNode *p_class, bool p_recursive);
	void resolve_function_signature(GDScriptParser::FunctionNode *p_function, const GDScriptParser::Node *p_source = nullptr, bool p_is_lambda = false);
	void resolve_function_body(GDScriptParser::FunctionNode *p_function, bool p_is_lambda = false);
	void resolve_node(GDScriptParser::Node *p_node, bool p_is_root = true);
	void resolve_suite(GDScriptParser::SuiteNode *p_suite);
	void resolve_assignable(GDScriptParser::AssignableNode *p_assignable, const char *p_kind);
	void resolve_variable(GDScriptParser::VariableNode *p_variable, bool p_is_local);
	void resolve_constant(GDScriptParser::ConstantNode *p_constant, bool p_is_local);
	void resolve_parameter(GDScriptParser::ParameterNode *p_parameter);
	void resolve_if(GDScriptParser::IfNode *p_if);
	void resolve_for(GDScriptParser::ForNode *p_for);
	void resolve_while(GDScriptParser::WhileNode *p_while);
	void resolve_assert(GDScriptParser::AssertNode *p_assert);
	void resolve_match(GDScriptParser::MatchNode *p_match);
	void resolve_match_branch(GDScriptParser::MatchBranchNode *p_match_branch, GDScriptParser::ExpressionNode *p_match_test);
	void resolve_match_pattern(GDScriptParser::PatternNode *p_match_pattern, GDScriptParser::ExpressionNode *p_match_test);
	void resolve_return(GDScriptParser::ReturnNode *p_return);

	// Reduction functions.
	void reduce_expression(GDScriptParser::ExpressionNode *p_expression, bool p_is_root = false);
	void reduce_array(GDScriptParser::ArrayNode *p_array);
	void reduce_assignment(GDScriptParser::AssignmentNode *p_assignment);
	void reduce_await(GDScriptParser::AwaitNode *p_await);
	void reduce_binary_op(GDScriptParser::BinaryOpNode *p_binary_op);
	void reduce_call(GDScriptParser::CallNode *p_call, bool p_is_await = false, bool p_is_root = false);
	void reduce_cast(GDScriptParser::CastNode *p_cast);
	void reduce_dictionary(GDScriptParser::DictionaryNode *p_dictionary);
	void reduce_get_node(GDScriptParser::GetNodeNode *p_get_node);
	void reduce_identifier(GDScriptParser::IdentifierNode *p_identifier, bool can_be_builtin = false);
	void reduce_identifier_from_base(GDScriptParser::IdentifierNode *p_identifier, GDScriptParser::DataType *p_base = nullptr);
	void reduce_lambda(GDScriptParser::LambdaNode *p_lambda);
	void reduce_literal(GDScriptParser::LiteralNode *p_literal);
	void reduce_preload(GDScriptParser::PreloadNode *p_preload);
	void reduce_self(GDScriptParser::SelfNode *p_self);
	void reduce_subscript(GDScriptParser::SubscriptNode *p_subscript, bool p_can_be_pseudo_type = false);
	void reduce_ternary_op(GDScriptParser::TernaryOpNode *p_ternary_op, bool p_is_root = false);
	void reduce_type_test(GDScriptParser::TypeTestNode *p_type_test);
	void reduce_unary_op(GDScriptParser::UnaryOpNode *p_unary_op);

	Variant make_expression_reduced_value(GDScriptParser::ExpressionNode *p_expression, bool &is_reduced);
	Variant make_array_reduced_value(GDScriptParser::ArrayNode *p_array, bool &is_reduced);
	Variant make_dictionary_reduced_value(GDScriptParser::DictionaryNode *p_dictionary, bool &is_reduced);
	Variant make_subscript_reduced_value(GDScriptParser::SubscriptNode *p_subscript, bool &is_reduced);

	// Helpers.
	Array make_array_from_element_datatype(const GDScriptParser::DataType &p_element_datatype, const GDScriptParser::Node *p_source_node = nullptr);
	Dictionary make_dictionary_from_element_datatype(const GDScriptParser::DataType &p_key_element_datatype, const GDScriptParser::DataType &p_value_element_datatype, const GDScriptParser::Node *p_source_node = nullptr);
	GDScriptParser::DataType type_from_variant(const Variant &p_value, const GDScriptParser::Node *p_source);
	GDScriptParser::DataType type_from_property(const PropertyInfo &p_property, bool p_is_arg = false, bool p_is_readonly = false) const;
	GDScriptParser::DataType make_global_class_meta_type(const StringName &p_class_name, const GDScriptParser::Node *p_source);
	bool get_function_signature(GDScriptParser::Node *p_source, bool p_is_constructor, GDScriptParser::DataType base_type, const StringName &p_function, GDScriptParser::DataType &r_return_type, List<GDScriptParser::DataType> &r_par_types, int &r_default_arg_count, BitField<MethodFlags> &r_method_flags, StringName *r_native_class = nullptr);
	bool function_signature_from_info(const MethodInfo &p_info, GDScriptParser::DataType &r_return_type, List<GDScriptParser::DataType> &r_par_types, int &r_default_arg_count, BitField<MethodFlags> &r_method_flags);
	void validate_call_arg(const List<GDScriptParser::DataType> &p_par_types, int p_default_args_count, bool p_is_vararg, const GDScriptParser::CallNode *p_call);
	void validate_call_arg(const MethodInfo &p_method, const GDScriptParser::CallNode *p_call);
	GDScriptParser::DataType get_operation_type(Variant::Operator p_operation, const GDScriptParser::DataType &p_a, const GDScriptParser::DataType &p_b, bool &r_valid, const GDScriptParser::Node *p_source);
	GDScriptParser::DataType get_operation_type(Variant::Operator p_operation, const GDScriptParser::DataType &p_a, bool &r_valid, const GDScriptParser::Node *p_source);
	void update_const_expression_builtin_type(GDScriptParser::ExpressionNode *p_expression, const GDScriptParser::DataType &p_type, const char *p_usage, bool p_is_cast = false);
	void update_array_literal_element_type(GDScriptParser::ArrayNode *p_array, const GDScriptParser::DataType &p_element_type);
	void update_dictionary_literal_element_type(GDScriptParser::DictionaryNode *p_dictionary, const GDScriptParser::DataType &p_key_element_type, const GDScriptParser::DataType &p_value_element_type);
	bool is_type_compatible(const GDScriptParser::DataType &p_target, const GDScriptParser::DataType &p_source, bool p_allow_implicit_conversion = false, const GDScriptParser::Node *p_source_node = nullptr);
	void push_error(const String &p_message, const GDScriptParser::Node *p_origin = nullptr);
	void mark_node_unsafe(const GDScriptParser::Node *p_node);
	void downgrade_node_type_source(GDScriptParser::Node *p_node);
	void mark_lambda_use_self();
	void resolve_pending_lambda_bodies();
	bool class_exists(const StringName &p_class) const;
	void reduce_identifier_from_base_set_class(GDScriptParser::IdentifierNode *p_identifier, GDScriptParser::DataType p_identifier_datatype);
	Ref<GDScriptParserRef> ensure_cached_external_parser_for_class(const GDScriptParser::ClassNode *p_class, const GDScriptParser::ClassNode *p_from_class, const char *p_context, const GDScriptParser::Node *p_source);
	Ref<GDScriptParserRef> find_cached_external_parser_for_class(const GDScriptParser::ClassNode *p_class, const Ref<GDScriptParserRef> &p_dependant_parser);
	Ref<GDScriptParserRef> find_cached_external_parser_for_class(const GDScriptParser::ClassNode *p_class, GDScriptParser *p_dependant_parser);
	Ref<GDScript> get_depended_shallow_script(const String &p_path, Error &r_error);
#ifdef DEBUG_ENABLED
	void is_shadowing(GDScriptParser::IdentifierNode *p_identifier, const String &p_context, const bool p_in_local_scope);
#endif

public:
	Error resolve_inheritance();
	Error resolve_interface();
	Error resolve_body();
	Error resolve_dependencies();
	Error analyze();

	Variant make_variable_default_value(GDScriptParser::VariableNode *p_variable);

	static bool check_type_compatibility(const GDScriptParser::DataType &p_target, const GDScriptParser::DataType &p_source, bool p_allow_implicit_conversion = false, const GDScriptParser::Node *p_source_node = nullptr);
	static GDScriptParser::DataType type_from_metatype(const GDScriptParser::DataType &p_meta_type);

	GDScriptAnalyzer(GDScriptParser *p_parser);
};
