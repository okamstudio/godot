/**************************************************************************/
/*  gdscript_parser.h                                                     */
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

#ifndef GDSCRIPT_PARSER_H
#define GDSCRIPT_PARSER_H

#include "gdscript_cache.h"
#include "gdscript_tokenizer.h"

#ifdef DEBUG_ENABLED
#include "gdscript_warning.h"
#endif

#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "core/object/script_language.h"
#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/templates/rb_map.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

#ifdef DEBUG_ENABLED
#include "core/string/string_builder.h"
#endif

class GDScriptParser {
	struct AnnotationInfo;

public:
	// Forward-declare all parser nodes, to avoid ordering issues.
	struct AnnotationNode;
	struct ArrayNode;
	struct AssertNode;
	struct AssignableNode;
	struct AssignmentNode;
	struct AwaitNode;
	struct BinaryOpNode;
	struct BreakNode;
	struct BreakpointNode;
	struct CallNode;
	struct CastNode;
	struct ClassNode;
	struct ConstantNode;
	struct ContinueNode;
	struct DictionaryNode;
	struct EnumNode;
	struct ExpressionNode;
	struct ForNode;
	struct FunctionNode;
	struct GetNodeNode;
	struct IdentifierNode;
	struct IfNode;
	struct LambdaNode;
	struct LiteralNode;
	struct MatchNode;
	struct MatchBranchNode;
	struct ParameterNode;
	struct PassNode;
	struct PatternNode;
	struct PreloadNode;
	struct ReturnNode;
	struct SelfNode;
	struct SignalNode;
	struct SubscriptNode;
	struct SuiteNode;
	struct TernaryOpNode;
	struct TypeNode;
	struct TypeTestNode;
	struct UnaryOpNode;
	struct VariableNode;
	struct WhileNode;

	class DataType {
	public:
		Vector<DataType> container_element_types;

		enum Kind {
			BUILTIN,
			NATIVE,
			SCRIPT,
			CLASS, // GDScript.
			ENUM, // Enumeration.
			VARIANT, // Can be any type.
			RESOLVING, // Currently resolving.
			UNRESOLVED,
		};
		Kind kind = UNRESOLVED;

		enum TypeSource {
			UNDETECTED, // Can be any type.
			INFERRED, // Has inferred type, but still dynamic.
			ANNOTATED_EXPLICIT, // Has a specific type annotated.
			ANNOTATED_INFERRED, // Has a static type but comes from the assigned value.
		};
		TypeSource type_source = UNDETECTED;

		bool is_constant = false;
		bool is_read_only = false;
		bool is_meta_type = false;
		bool is_pseudo_type = false; // For global names that can't be used standalone.
		bool is_coroutine = false; // For function calls.

		Variant::Type builtin_type = Variant::NIL;
		StringName native_type;
		StringName enum_type; // Enum name or the value name in an enum.
		Ref<Script> script_type;
		String script_path;
		ClassNode *class_type = nullptr;

		MethodInfo method_info; // For callable/signals.
		HashMap<StringName, int64_t> enum_values; // For enums.

		_FORCE_INLINE_ bool is_set() const { return kind != RESOLVING && kind != UNRESOLVED; }
		_FORCE_INLINE_ bool is_resolving() const { return kind == RESOLVING; }
		_FORCE_INLINE_ bool has_no_type() const { return type_source == UNDETECTED; }
		_FORCE_INLINE_ bool is_variant() const { return kind == VARIANT || kind == RESOLVING || kind == UNRESOLVED; }
		_FORCE_INLINE_ bool is_hard_type() const { return type_source > INFERRED; }

		String to_string() const;
		_FORCE_INLINE_ String to_string_strict() const { return is_hard_type() ? to_string() : "Variant"; }
		PropertyInfo to_property_info(const String &p_name) const;

		_FORCE_INLINE_ static DataType get_variant_type() { // Default DataType for container elements.
			DataType datatype;
			datatype.kind = VARIANT;
			datatype.type_source = INFERRED;
			return datatype;
		}

		_FORCE_INLINE_ void set_container_element_type(int p_index, const DataType &p_type) {
			ERR_FAIL_COND(p_index < 0);
			while (p_index >= container_element_types.size()) {
				container_element_types.push_back(get_variant_type());
			}
			container_element_types.write[p_index] = DataType(p_type);
		}

		_FORCE_INLINE_ int get_container_element_type_count() const {
			return container_element_types.size();
		}

		_FORCE_INLINE_ DataType get_container_element_type(int p_index) const {
			ERR_FAIL_INDEX_V(p_index, container_element_types.size(), get_variant_type());
			return container_element_types[p_index];
		}

		_FORCE_INLINE_ DataType get_container_element_type_or_variant(int p_index) const {
			if (p_index < 0 || p_index >= container_element_types.size()) {
				return get_variant_type();
			}
			return container_element_types[p_index];
		}

		_FORCE_INLINE_ bool has_container_element_type(int p_index) const {
			return p_index >= 0 && p_index < container_element_types.size();
		}

		_FORCE_INLINE_ bool has_container_element_types() const {
			return !container_element_types.is_empty();
		}

		bool is_typed_container_type() const;

		GDScriptParser::DataType get_typed_container_type() const;

		bool can_reference(const DataType &p_other) const;

		bool operator==(const DataType &p_other) const {
			if (type_source == UNDETECTED || p_other.type_source == UNDETECTED) {
				return true; // Can be considered equal for parsing purposes.
			}

			if (type_source == INFERRED || p_other.type_source == INFERRED) {
				return true; // Can be considered equal for parsing purposes.
			}

			if (kind != p_other.kind) {
				return false;
			}

			switch (kind) {
				case VARIANT:
					return true; // All variants are the same.
				case BUILTIN:
					return builtin_type == p_other.builtin_type;
				case NATIVE:
				case ENUM: // Enums use native_type to identify the enum and its base class.
					return native_type == p_other.native_type;
				case SCRIPT:
					return script_type == p_other.script_type;
				case CLASS:
					return class_type == p_other.class_type || class_type->fqcn == p_other.class_type->fqcn;
				case RESOLVING:
				case UNRESOLVED:
					break;
			}

			return false;
		}

		bool operator!=(const DataType &p_other) const {
			return !(*this == p_other);
		}

		void operator=(const DataType &p_other) {
			kind = p_other.kind;
			type_source = p_other.type_source;
			is_read_only = p_other.is_read_only;
			is_constant = p_other.is_constant;
			is_meta_type = p_other.is_meta_type;
			is_pseudo_type = p_other.is_pseudo_type;
			is_coroutine = p_other.is_coroutine;
			builtin_type = p_other.builtin_type;
			native_type = p_other.native_type;
			enum_type = p_other.enum_type;
			script_type = p_other.script_type;
			script_path = p_other.script_path;
			class_type = p_other.class_type;
			method_info = p_other.method_info;
			enum_values = p_other.enum_values;
			container_element_types = p_other.container_element_types;
		}

		DataType() = default;

		DataType(const DataType &p_other) {
			*this = p_other;
		}

		~DataType() {}
	};

	struct ParserError {
		// TODO: Do I really need a "type"?
		// enum Type {
		//     NO_ERROR,
		//     EMPTY_FILE,
		//     CLASS_NAME_USED_TWICE,
		//     EXTENDS_USED_TWICE,
		//     EXPECTED_END_STATEMENT,
		// };
		// Type type = NO_ERROR;
		String message;
		int line = 0, column = 0;
	};

#ifdef TOOLS_ENABLED
	struct ClassDocData {
		String brief;
		String description;
		Vector<Pair<String, String>> tutorials;
		bool is_deprecated = false;
		String deprecated_message;
		bool is_experimental = false;
		String experimental_message;
	};

	struct MemberDocData {
		String description;
		bool is_deprecated = false;
		String deprecated_message;
		bool is_experimental = false;
		String experimental_message;
	};
#endif // TOOLS_ENABLED

	struct Node {
		enum Type {
			NONE,
			ANNOTATION,
			ARRAY,
			ASSERT,
			ASSIGNMENT,
			AWAIT,
			BINARY_OPERATOR,
			BREAK,
			BREAKPOINT,
			CALL,
			CAST,
			CLASS,
			CONSTANT,
			CONTINUE,
			DICTIONARY,
			ENUM,
			FOR,
			FUNCTION,
			GET_NODE,
			IDENTIFIER,
			IF,
			LAMBDA,
			LITERAL,
			MATCH,
			MATCH_BRANCH,
			PARAMETER,
			PASS,
			PATTERN,
			PRELOAD,
			RETURN,
			SELF,
			SIGNAL,
			SUBSCRIPT,
			SUITE,
			TERNARY_OPERATOR,
			TYPE,
			TYPE_TEST,
			UNARY_OPERATOR,
			VARIABLE,
			WHILE,
		};

		Type type = NONE;
		int start_line = 0, end_line = 0;
		int start_column = 0, end_column = 0;
		int leftmost_column = 0, rightmost_column = 0;
		Node *next = nullptr;
		List<AnnotationNode *> annotations;

		DataType datatype;

		virtual DataType get_datatype() const { return datatype; }
		virtual void set_datatype(const DataType &p_datatype) { datatype = p_datatype; }

		virtual bool is_expression() const { return false; }

		virtual ~Node() {}
	};

	struct ExpressionNode : public Node {
		// Base type for all expression kinds.
		bool reduced = false;
		bool is_constant = false;
		Variant reduced_value;

		virtual bool is_expression() const override { return true; }
		virtual ~ExpressionNode() {}

	protected:
		ExpressionNode() {}
	};

	struct AnnotationNode : public Node {
		StringName name;
		Vector<ExpressionNode *> arguments;
		Vector<Variant> resolved_arguments;

		AnnotationInfo *info = nullptr;
		PropertyInfo export_info;
		bool is_resolved = false;
		bool is_applied = false;

		bool apply(GDScriptParser *p_this, Node *p_target, ClassNode *p_class);
		bool applies_to(uint32_t p_target_kinds) const;

		AnnotationNode() {
			type = ANNOTATION;
		}
	};

	struct ArrayNode : public ExpressionNode {
		Vector<ExpressionNode *> elements;

		ArrayNode() {
			type = ARRAY;
		}
	};

	struct AssertNode : public Node {
		ExpressionNode *condition = nullptr;
		ExpressionNode *message = nullptr;

		AssertNode() {
			type = ASSERT;
		}
	};

	struct AssignableNode : public Node {
		IdentifierNode *identifier = nullptr;
		ExpressionNode *initializer = nullptr;
		TypeNode *datatype_specifier = nullptr;
		bool infer_datatype = false;
		bool use_conversion_assign = false;
		int usages = 0;

		virtual ~AssignableNode() {}

	protected:
		AssignableNode() {}
	};

	struct AssignmentNode : public ExpressionNode {
		// Assignment is not really an expression but it's easier to parse as if it were.
		enum Operation {
			OP_NONE,
			OP_ADDITION,
			OP_SUBTRACTION,
			OP_MULTIPLICATION,
			OP_DIVISION,
			OP_MODULO,
			OP_POWER,
			OP_BIT_SHIFT_LEFT,
			OP_BIT_SHIFT_RIGHT,
			OP_BIT_AND,
			OP_BIT_OR,
			OP_BIT_XOR,
		};

		Operation operation = OP_NONE;
		Variant::Operator variant_op = Variant::OP_MAX;
		ExpressionNode *assignee = nullptr;
		ExpressionNode *assigned_value = nullptr;
		bool use_conversion_assign = false;

		AssignmentNode() {
			type = ASSIGNMENT;
		}
	};

	struct AwaitNode : public ExpressionNode {
		ExpressionNode *to_await = nullptr;

		AwaitNode() {
			type = AWAIT;
		}
	};

	struct BinaryOpNode : public ExpressionNode {
		enum OpType {
			OP_ADDITION,
			OP_SUBTRACTION,
			OP_MULTIPLICATION,
			OP_DIVISION,
			OP_MODULO,
			OP_POWER,
			OP_BIT_LEFT_SHIFT,
			OP_BIT_RIGHT_SHIFT,
			OP_BIT_AND,
			OP_BIT_OR,
			OP_BIT_XOR,
			OP_LOGIC_AND,
			OP_LOGIC_OR,
			OP_CONTENT_TEST,
			OP_COMP_EQUAL,
			OP_COMP_NOT_EQUAL,
			OP_COMP_LESS,
			OP_COMP_LESS_EQUAL,
			OP_COMP_GREATER,
			OP_COMP_GREATER_EQUAL,
		};

		OpType operation = OpType::OP_ADDITION;
		Variant::Operator variant_op = Variant::OP_MAX;
		ExpressionNode *left_operand = nullptr;
		ExpressionNode *right_operand = nullptr;

		BinaryOpNode() {
			type = BINARY_OPERATOR;
		}
	};

	struct BreakNode : public Node {
		BreakNode() {
			type = BREAK;
		}
	};

	struct BreakpointNode : public Node {
		BreakpointNode() {
			type = BREAKPOINT;
		}
	};

	struct CallNode : public ExpressionNode {
		ExpressionNode *callee = nullptr;
		Vector<ExpressionNode *> arguments;
		StringName function_name;
		bool is_super = false;
		bool is_static = false;

		CallNode() {
			type = CALL;
		}

		Type get_callee_type() const {
			if (callee == nullptr) {
				return Type::NONE;
			} else {
				return callee->type;
			}
		}
	};

	struct CastNode : public ExpressionNode {
		ExpressionNode *operand = nullptr;
		TypeNode *cast_type = nullptr;

		CastNode() {
			type = CAST;
		}
	};

	struct EnumNode : public Node {
		struct Value {
			IdentifierNode *identifier = nullptr;
			ExpressionNode *custom_value = nullptr;
			EnumNode *parent_enum = nullptr;
			int index = -1;
			bool resolved = false;
			int64_t value = 0;
			int line = 0;
			int leftmost_column = 0;
			int rightmost_column = 0;
#ifdef TOOLS_ENABLED
			MemberDocData doc_data;
#endif // TOOLS_ENABLED
		};

		IdentifierNode *identifier = nullptr;
		Vector<Value> values;
		Variant dictionary;
#ifdef TOOLS_ENABLED
		MemberDocData doc_data;
#endif // TOOLS_ENABLED

		EnumNode() {
			type = ENUM;
		}
	};

	struct ClassNode : public Node {
		struct Member {
			enum Type {
				UNDEFINED,
				CLASS,
				CONSTANT,
				FUNCTION,
				SIGNAL,
				VARIABLE,
				ENUM,
				ENUM_VALUE, // For unnamed enums.
				GROUP, // For member grouping.
			};

			Type type = UNDEFINED;

			union {
				ClassNode *m_class = nullptr;
				ConstantNode *constant;
				FunctionNode *function;
				SignalNode *signal;
				VariableNode *variable;
				EnumNode *m_enum;
				AnnotationNode *annotation;
			};
			EnumNode::Value enum_value;

			String get_name() const {
				switch (type) {
					case UNDEFINED:
						return "<undefined member>";
					case CLASS:
						// All class-type members have an id.
						return m_class->identifier->name;
					case CONSTANT:
						return constant->identifier->name;
					case FUNCTION:
						return function->identifier->name;
					case SIGNAL:
						return signal->identifier->name;
					case VARIABLE:
						return variable->identifier->name;
					case ENUM:
						// All enum-type members have an id.
						return m_enum->identifier->name;
					case ENUM_VALUE:
						return enum_value.identifier->name;
					case GROUP:
						return annotation->export_info.name;
				}
				return "";
			}

			String get_type_name() const {
				switch (type) {
					case UNDEFINED:
						return "???";
					case CLASS:
						return "class";
					case CONSTANT:
						return "constant";
					case FUNCTION:
						return "function";
					case SIGNAL:
						return "signal";
					case VARIABLE:
						return "variable";
					case ENUM:
						return "enum";
					case ENUM_VALUE:
						return "enum value";
					case GROUP:
						return "group";
				}
				return "";
			}

			int get_line() const {
				switch (type) {
					case CLASS:
						return m_class->start_line;
					case CONSTANT:
						return constant->start_line;
					case FUNCTION:
						return function->start_line;
					case VARIABLE:
						return variable->start_line;
					case ENUM_VALUE:
						return enum_value.line;
					case ENUM:
						return m_enum->start_line;
					case SIGNAL:
						return signal->start_line;
					case GROUP:
						return annotation->start_line;
					case UNDEFINED:
						ERR_FAIL_V_MSG(-1, "Reaching undefined member type.");
				}
				ERR_FAIL_V_MSG(-1, "Reaching unhandled type.");
			}

			DataType get_datatype() const {
				switch (type) {
					case CLASS:
						return m_class->get_datatype();
					case CONSTANT:
						return constant->get_datatype();
					case FUNCTION:
						return function->get_datatype();
					case VARIABLE:
						return variable->get_datatype();
					case ENUM:
						return m_enum->get_datatype();
					case ENUM_VALUE:
						return enum_value.identifier->get_datatype();
					case SIGNAL:
						return signal->get_datatype();
					case GROUP:
						return DataType();
					case UNDEFINED:
						return DataType();
				}
				ERR_FAIL_V_MSG(DataType(), "Reaching unhandled type.");
			}

			Node *get_source_node() const {
				switch (type) {
					case CLASS:
						return m_class;
					case CONSTANT:
						return constant;
					case FUNCTION:
						return function;
					case VARIABLE:
						return variable;
					case ENUM:
						return m_enum;
					case ENUM_VALUE:
						return enum_value.identifier;
					case SIGNAL:
						return signal;
					case GROUP:
						return annotation;
					case UNDEFINED:
						return nullptr;
				}
				ERR_FAIL_V_MSG(nullptr, "Reaching unhandled type.");
			}

			Member() {}

			Member(ClassNode *p_class) {
				type = CLASS;
				m_class = p_class;
			}
			Member(ConstantNode *p_constant) {
				type = CONSTANT;
				constant = p_constant;
			}
			Member(VariableNode *p_variable) {
				type = VARIABLE;
				variable = p_variable;
			}
			Member(SignalNode *p_signal) {
				type = SIGNAL;
				signal = p_signal;
			}
			Member(FunctionNode *p_function) {
				type = FUNCTION;
				function = p_function;
			}
			Member(EnumNode *p_enum) {
				type = ENUM;
				m_enum = p_enum;
			}
			Member(const EnumNode::Value &p_enum_value) {
				type = ENUM_VALUE;
				enum_value = p_enum_value;
			}
			Member(AnnotationNode *p_annotation) {
				type = GROUP;
				annotation = p_annotation;
			}
		};

		IdentifierNode *identifier = nullptr;
		String icon_path;
		String simplified_icon_path;
		Vector<Member> members;
		HashMap<StringName, int> members_indices;
		ClassNode *outer = nullptr;
		bool extends_used = false;
		bool onready_used = false;
		bool has_static_data = false;
		bool annotated_static_unload = false;
		String extends_path;
		Vector<IdentifierNode *> extends; // List for indexing: extends A.B.C
		DataType base_type;
		String fqcn; // Fully-qualified class name. Identifies uniquely any class in the project.
#ifdef TOOLS_ENABLED
		ClassDocData doc_data;

		// EnumValue docs are parsed after itself, so we need a method to add/modify the doc property later.
		void set_enum_value_doc_data(const StringName &p_name, const MemberDocData &p_doc_data) {
			ERR_FAIL_INDEX(members_indices[p_name], members.size());
			members.write[members_indices[p_name]].enum_value.doc_data = p_doc_data;
		}
#endif // TOOLS_ENABLED

		bool resolved_interface = false;
		bool resolved_body = false;

		StringName get_global_name() const {
			return (outer == nullptr && identifier != nullptr) ? identifier->name : StringName();
		}

		Member get_member(const StringName &p_name) const {
			return members[members_indices[p_name]];
		}
		bool has_member(const StringName &p_name) const {
			return members_indices.has(p_name);
		}
		bool has_function(const StringName &p_name) const {
			return has_member(p_name) && members[members_indices[p_name]].type == Member::FUNCTION;
		}
		template <typename T>
		void add_member(T *p_member_node) {
			members_indices[p_member_node->identifier->name] = members.size();
			members.push_back(Member(p_member_node));
		}
		void add_member(const EnumNode::Value &p_enum_value) {
			members_indices[p_enum_value.identifier->name] = members.size();
			members.push_back(Member(p_enum_value));
		}
		void add_member_group(AnnotationNode *p_annotation_node) {
			// Avoid name conflict. See GH-78252.
			StringName name = vformat("@group_%d_%s", members.size(), p_annotation_node->export_info.name);
			members_indices[name] = members.size();
			members.push_back(Member(p_annotation_node));
		}

		ClassNode() {
			type = CLASS;
		}
	};

	struct ConstantNode : public AssignableNode {
#ifdef TOOLS_ENABLED
		MemberDocData doc_data;
#endif // TOOLS_ENABLED

		ConstantNode() {
			type = CONSTANT;
		}
	};

	struct ContinueNode : public Node {
		ContinueNode() {
			type = CONTINUE;
		}
	};

	struct DictionaryNode : public ExpressionNode {
		struct Pair {
			ExpressionNode *key = nullptr;
			ExpressionNode *value = nullptr;
		};
		Vector<Pair> elements;

		enum Style {
			LUA_TABLE,
			PYTHON_DICT,
		};
		Style style = PYTHON_DICT;

		DictionaryNode() {
			type = DICTIONARY;
		}
	};

	struct ForNode : public Node {
		IdentifierNode *variable = nullptr;
		TypeNode *datatype_specifier = nullptr;
		bool use_conversion_assign = false;
		ExpressionNode *list = nullptr;
		SuiteNode *loop = nullptr;

		ForNode() {
			type = FOR;
		}
	};

	struct FunctionNode : public Node {
		IdentifierNode *identifier = nullptr;
		Vector<ParameterNode *> parameters;
		HashMap<StringName, int> parameters_indices;
		TypeNode *return_type = nullptr;
		SuiteNode *body = nullptr;
		bool is_static = false; // For lambdas it's determined in the analyzer.
		bool is_coroutine = false;
		Variant rpc_config;
		MethodInfo info;
		LambdaNode *source_lambda = nullptr;
		Vector<Variant> default_arg_values;
#ifdef TOOLS_ENABLED
		MemberDocData doc_data;
#endif // TOOLS_ENABLED

		bool resolved_signature = false;
		bool resolved_body = false;

		FunctionNode() {
			type = FUNCTION;
		}
	};

	struct GetNodeNode : public ExpressionNode {
		String full_path;
		bool use_dollar = true;

		GetNodeNode() {
			type = GET_NODE;
		}
	};

	struct IdentifierNode : public ExpressionNode {
		StringName name;
		SuiteNode *suite = nullptr; // The block in which the identifier is used.

		enum Source {
			UNDEFINED_SOURCE,
			FUNCTION_PARAMETER,
			LOCAL_VARIABLE,
			LOCAL_CONSTANT,
			LOCAL_ITERATOR, // `for` loop iterator.
			LOCAL_BIND, // Pattern bind.
			MEMBER_VARIABLE,
			MEMBER_CONSTANT,
			MEMBER_FUNCTION,
			MEMBER_SIGNAL,
			MEMBER_CLASS,
			INHERITED_VARIABLE,
			STATIC_VARIABLE,
		};
		Source source = UNDEFINED_SOURCE;

		union {
			ParameterNode *parameter_source = nullptr;
			IdentifierNode *bind_source;
			VariableNode *variable_source;
			ConstantNode *constant_source;
			SignalNode *signal_source;
			FunctionNode *function_source;
		};
		bool function_source_is_static = false; // For non-GDScript scripts.

		FunctionNode *source_function = nullptr; // TODO: Rename to disambiguate `function_source`.

		int usages = 0; // Useful for binds/iterator variable.

		IdentifierNode() {
			type = IDENTIFIER;
		}
	};

	struct IfNode : public Node {
		ExpressionNode *condition = nullptr;
		SuiteNode *true_block = nullptr;
		SuiteNode *false_block = nullptr;

		IfNode() {
			type = IF;
		}
	};

	struct LambdaNode : public ExpressionNode {
		FunctionNode *function = nullptr;
		FunctionNode *parent_function = nullptr;
		LambdaNode *parent_lambda = nullptr;
		Vector<IdentifierNode *> captures;
		HashMap<StringName, int> captures_indices;
		bool use_self = false;

		bool has_name() const {
			return function && function->identifier;
		}

		LambdaNode() {
			type = LAMBDA;
		}
	};

	struct LiteralNode : public ExpressionNode {
		Variant value;

		LiteralNode() {
			type = LITERAL;
		}
	};

	struct MatchNode : public Node {
		ExpressionNode *test = nullptr;
		Vector<MatchBranchNode *> branches;

		MatchNode() {
			type = MATCH;
		}
	};

	struct MatchBranchNode : public Node {
		Vector<PatternNode *> patterns;
		SuiteNode *block = nullptr;
		bool has_wildcard = false;
		SuiteNode *guard_body = nullptr;

		MatchBranchNode() {
			type = MATCH_BRANCH;
		}
	};

	struct ParameterNode : public AssignableNode {
		ParameterNode() {
			type = PARAMETER;
		}
	};

	struct PassNode : public Node {
		PassNode() {
			type = PASS;
		}
	};

	struct PatternNode : public Node {
		enum Type {
			PT_LITERAL,
			PT_EXPRESSION,
			PT_BIND,
			PT_ARRAY,
			PT_DICTIONARY,
			PT_REST,
			PT_WILDCARD,
		};
		Type pattern_type = PT_LITERAL;

		union {
			LiteralNode *literal = nullptr;
			IdentifierNode *bind;
			ExpressionNode *expression;
		};
		Vector<PatternNode *> array;
		bool rest_used = false; // For array/dict patterns.

		struct Pair {
			ExpressionNode *key = nullptr;
			PatternNode *value_pattern = nullptr;
		};
		Vector<Pair> dictionary;

		HashMap<StringName, IdentifierNode *> binds;

		bool has_bind(const StringName &p_name);
		IdentifierNode *get_bind(const StringName &p_name);

		PatternNode() {
			type = PATTERN;
		}
	};
	struct PreloadNode : public ExpressionNode {
		ExpressionNode *path = nullptr;
		String resolved_path;
		Ref<Resource> resource;

		PreloadNode() {
			type = PRELOAD;
		}
	};

	struct ReturnNode : public Node {
		ExpressionNode *return_value = nullptr;
		bool void_return = false;

		ReturnNode() {
			type = RETURN;
		}
	};

	struct SelfNode : public ExpressionNode {
		ClassNode *current_class = nullptr;

		SelfNode() {
			type = SELF;
		}
	};

	struct SignalNode : public Node {
		IdentifierNode *identifier = nullptr;
		Vector<ParameterNode *> parameters;
		HashMap<StringName, int> parameters_indices;
		MethodInfo method_info;
#ifdef TOOLS_ENABLED
		MemberDocData doc_data;
#endif // TOOLS_ENABLED

		int usages = 0;

		SignalNode() {
			type = SIGNAL;
		}
	};

	struct SubscriptNode : public ExpressionNode {
		ExpressionNode *base = nullptr;
		union {
			ExpressionNode *index = nullptr;
			IdentifierNode *attribute;
		};

		bool is_attribute = false;

		SubscriptNode() {
			type = SUBSCRIPT;
		}
	};

	struct SuiteNode : public Node {
		SuiteNode *parent_block = nullptr;
		Vector<Node *> statements;
		struct Local {
			enum Type {
				UNDEFINED,
				CONSTANT,
				VARIABLE,
				PARAMETER,
				FOR_VARIABLE,
				PATTERN_BIND,
			};
			Type type = UNDEFINED;
			union {
				ConstantNode *constant = nullptr;
				VariableNode *variable;
				ParameterNode *parameter;
				IdentifierNode *bind;
			};
			StringName name;
			FunctionNode *source_function = nullptr;

			int start_line = 0, end_line = 0;
			int start_column = 0, end_column = 0;
			int leftmost_column = 0, rightmost_column = 0;

			DataType get_datatype() const;
			String get_name() const;

			Local() {}
			Local(ConstantNode *p_constant, FunctionNode *p_source_function) {
				type = CONSTANT;
				constant = p_constant;
				name = p_constant->identifier->name;
				source_function = p_source_function;

				start_line = p_constant->start_line;
				end_line = p_constant->end_line;
				start_column = p_constant->start_column;
				end_column = p_constant->end_column;
				leftmost_column = p_constant->leftmost_column;
				rightmost_column = p_constant->rightmost_column;
			}
			Local(VariableNode *p_variable, FunctionNode *p_source_function) {
				type = VARIABLE;
				variable = p_variable;
				name = p_variable->identifier->name;
				source_function = p_source_function;

				start_line = p_variable->start_line;
				end_line = p_variable->end_line;
				start_column = p_variable->start_column;
				end_column = p_variable->end_column;
				leftmost_column = p_variable->leftmost_column;
				rightmost_column = p_variable->rightmost_column;
			}
			Local(ParameterNode *p_parameter, FunctionNode *p_source_function) {
				type = PARAMETER;
				parameter = p_parameter;
				name = p_parameter->identifier->name;
				source_function = p_source_function;

				start_line = p_parameter->start_line;
				end_line = p_parameter->end_line;
				start_column = p_parameter->start_column;
				end_column = p_parameter->end_column;
				leftmost_column = p_parameter->leftmost_column;
				rightmost_column = p_parameter->rightmost_column;
			}
			Local(IdentifierNode *p_identifier, FunctionNode *p_source_function) {
				type = FOR_VARIABLE;
				bind = p_identifier;
				name = p_identifier->name;
				source_function = p_source_function;

				start_line = p_identifier->start_line;
				end_line = p_identifier->end_line;
				start_column = p_identifier->start_column;
				end_column = p_identifier->end_column;
				leftmost_column = p_identifier->leftmost_column;
				rightmost_column = p_identifier->rightmost_column;
			}
		};
		Local empty;
		Vector<Local> locals;
		HashMap<StringName, int> locals_indices;

		FunctionNode *parent_function = nullptr;
		IfNode *parent_if = nullptr;

		bool has_return = false;
		bool has_continue = false;
		bool has_unreachable_code = false; // Just so warnings aren't given more than once per block.
		bool is_in_loop = false; // The block is nested in a loop (directly or indirectly).

		bool has_local(const StringName &p_name) const;
		const Local &get_local(const StringName &p_name) const;
		template <typename T>
		void add_local(T *p_local, FunctionNode *p_source_function) {
			locals_indices[p_local->identifier->name] = locals.size();
			locals.push_back(Local(p_local, p_source_function));
		}
		void add_local(const Local &p_local) {
			locals_indices[p_local.name] = locals.size();
			locals.push_back(p_local);
		}

		SuiteNode() {
			type = SUITE;
		}
	};

	struct TernaryOpNode : public ExpressionNode {
		// Only one ternary operation exists, so no abstraction here.
		ExpressionNode *condition = nullptr;
		ExpressionNode *true_expr = nullptr;
		ExpressionNode *false_expr = nullptr;

		TernaryOpNode() {
			type = TERNARY_OPERATOR;
		}
	};

	struct TypeNode : public Node {
		Vector<IdentifierNode *> type_chain;
		Vector<TypeNode *> container_types;

		TypeNode *get_container_type_or_null(int p_index) const {
			return p_index >= 0 && p_index < container_types.size() ? container_types[p_index] : nullptr;
		}

		TypeNode() {
			type = TYPE;
		}
	};

	struct TypeTestNode : public ExpressionNode {
		ExpressionNode *operand = nullptr;
		TypeNode *test_type = nullptr;
		DataType test_datatype;

		TypeTestNode() {
			type = TYPE_TEST;
		}
	};

	struct UnaryOpNode : public ExpressionNode {
		enum OpType {
			OP_POSITIVE,
			OP_NEGATIVE,
			OP_COMPLEMENT,
			OP_LOGIC_NOT,
		};

		OpType operation = OP_POSITIVE;
		Variant::Operator variant_op = Variant::OP_MAX;
		ExpressionNode *operand = nullptr;

		UnaryOpNode() {
			type = UNARY_OPERATOR;
		}
	};

	struct VariableNode : public AssignableNode {
		enum PropertyStyle {
			PROP_NONE,
			PROP_INLINE,
			PROP_SETGET,
		};

		PropertyStyle property = PROP_NONE;
		union {
			FunctionNode *setter = nullptr;
			IdentifierNode *setter_pointer;
		};
		IdentifierNode *setter_parameter = nullptr;
		union {
			FunctionNode *getter = nullptr;
			IdentifierNode *getter_pointer;
		};

		bool exported = false;
		bool onready = false;
		PropertyInfo export_info;
		int assignments = 0;
		bool is_static = false;
#ifdef TOOLS_ENABLED
		MemberDocData doc_data;
#endif // TOOLS_ENABLED

		VariableNode() {
			type = VARIABLE;
		}
	};

	struct WhileNode : public Node {
		ExpressionNode *condition = nullptr;
		SuiteNode *loop = nullptr;

		WhileNode() {
			type = WHILE;
		}
	};

	enum CompletionType {
		COMPLETION_NONE,
		COMPLETION_ANNOTATION, // Annotation (following @).
		COMPLETION_ANNOTATION_ARGUMENTS, // Annotation arguments hint.
		COMPLETION_ASSIGN, // Assignment based on type (e.g. enum values).
		COMPLETION_ATTRIBUTE, // After id.| to look for members.
		COMPLETION_ATTRIBUTE_METHOD, // After id.| to look for methods.
		COMPLETION_BUILT_IN_TYPE_CONSTANT_OR_STATIC_METHOD, // Constants inside a built-in type (e.g. Color.BLUE) or static methods (e.g. Color.html).
		COMPLETION_CALL_ARGUMENTS, // Complete with nodes, input actions, enum values (or usual expressions).
		// TODO: COMPLETION_DECLARATION, // Potential declaration (var, const, func).
		COMPLETION_GET_NODE, // Get node with $ notation.
		COMPLETION_IDENTIFIER, // List available identifiers in scope.
		COMPLETION_INHERIT_TYPE, // Type after extends. Exclude non-viable types (built-ins, enums, void). Includes subtypes using the argument index.
		COMPLETION_METHOD, // List available methods in scope.
		COMPLETION_OVERRIDE_METHOD, // Override implementation, also for native virtuals.
		COMPLETION_PROPERTY_DECLARATION, // Property declaration (get, set).
		COMPLETION_PROPERTY_DECLARATION_OR_TYPE, // Property declaration (get, set) or a type hint.
		COMPLETION_PROPERTY_METHOD, // Property setter or getter (list available methods).
		COMPLETION_RESOURCE_PATH, // For load/preload.
		COMPLETION_SUBSCRIPT, // Inside id[|].
		COMPLETION_SUPER_METHOD, // After super.
		COMPLETION_TYPE_ATTRIBUTE, // Attribute in type name (Type.|).
		COMPLETION_TYPE_NAME, // Name of type (after :).
		COMPLETION_TYPE_NAME_OR_VOID, // Same as TYPE_NAME, but allows void (in function return type).
	};

	struct CompletionContext {
		CompletionType type = COMPLETION_NONE;
		ClassNode *current_class = nullptr;
		FunctionNode *current_function = nullptr;
		SuiteNode *current_suite = nullptr;
		int current_line = -1;
		int current_argument = -1;
		Variant::Type builtin_type = Variant::VARIANT_MAX;
		Node *node = nullptr;
		Object *base = nullptr;
		GDScriptParser *parser = nullptr;
	};

	struct CompletionCall {
		Node *call = nullptr;
		int argument = -1;
	};

private:
	friend class GDScriptAnalyzer;
	friend class GDScriptParserRef;

	bool _is_tool = false;
	String script_path;
	bool for_completion = false;
	bool parse_body = true;
	bool panic_mode = false;
	bool can_break = false;
	bool can_continue = false;
	List<bool> multiline_stack;
	HashMap<String, Ref<GDScriptParserRef>> depended_parsers;

	ClassNode *head = nullptr;
	Node *list = nullptr;
	List<ParserError> errors;

#ifdef DEBUG_ENABLED
	struct PendingWarning {
		const Node *source = nullptr;
		GDScriptWarning::Code code = GDScriptWarning::WARNING_MAX;
		bool treated_as_error = false;
		Vector<String> symbols;
	};

	bool is_ignoring_warnings = false;
	List<GDScriptWarning> warnings;
	List<PendingWarning> pending_warnings;
	HashSet<int> warning_ignored_lines[GDScriptWarning::WARNING_MAX];
	int warning_ignore_start_lines[GDScriptWarning::WARNING_MAX];
	HashSet<int> unsafe_lines;
#endif

	GDScriptTokenizer *tokenizer = nullptr;
	GDScriptTokenizer::Token previous;
	GDScriptTokenizer::Token current;

	ClassNode *current_class = nullptr;
	FunctionNode *current_function = nullptr;
	LambdaNode *current_lambda = nullptr;
	SuiteNode *current_suite = nullptr;

	CompletionContext completion_context;
	CompletionCall completion_call;
	List<CompletionCall> completion_call_stack;
	bool passed_cursor = false;
	bool in_lambda = false;
	bool lambda_ended = false; // Marker for when a lambda ends, to apply an end of statement if needed.

	typedef bool (GDScriptParser::*AnnotationAction)(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	struct AnnotationInfo {
		enum TargetKind {
			NONE = 0,
			SCRIPT = 1 << 0,
			CLASS = 1 << 1,
			VARIABLE = 1 << 2,
			CONSTANT = 1 << 3,
			SIGNAL = 1 << 4,
			FUNCTION = 1 << 5,
			STATEMENT = 1 << 6,
			STANDALONE = 1 << 7,
			CLASS_LEVEL = CLASS | VARIABLE | CONSTANT | SIGNAL | FUNCTION,
		};
		uint32_t target_kind = 0; // Flags.
		AnnotationAction apply = nullptr;
		MethodInfo info;
	};
	static HashMap<StringName, AnnotationInfo> valid_annotations;
	List<AnnotationNode *> annotation_stack;

	typedef ExpressionNode *(GDScriptParser::*ParseFunction)(ExpressionNode *p_previous_operand, bool p_can_assign);
	// Higher value means higher precedence (i.e. is evaluated first).
	enum Precedence {
		PREC_NONE,
		PREC_ASSIGNMENT,
		PREC_CAST,
		PREC_TERNARY,
		PREC_LOGIC_OR,
		PREC_LOGIC_AND,
		PREC_LOGIC_NOT,
		PREC_CONTENT_TEST,
		PREC_COMPARISON,
		PREC_BIT_OR,
		PREC_BIT_XOR,
		PREC_BIT_AND,
		PREC_BIT_SHIFT,
		PREC_ADDITION_SUBTRACTION,
		PREC_FACTOR,
		PREC_SIGN,
		PREC_BIT_NOT,
		PREC_POWER,
		PREC_TYPE_TEST,
		PREC_AWAIT,
		PREC_CALL,
		PREC_ATTRIBUTE,
		PREC_SUBSCRIPT,
		PREC_PRIMARY,
	};
	struct ParseRule {
		ParseFunction prefix = nullptr;
		ParseFunction infix = nullptr;
		Precedence precedence = PREC_NONE;
	};
	static ParseRule *get_rule(GDScriptTokenizer::Token::Type p_token_type);

	List<Node *> nodes_in_progress;
	void complete_extents(Node *p_node);
	void update_extents(Node *p_node);
	void reset_extents(Node *p_node, GDScriptTokenizer::Token p_token);
	void reset_extents(Node *p_node, Node *p_from);

	template <typename T>
	T *alloc_node() {
		T *node = memnew(T);

		node->next = list;
		list = node;

		reset_extents(node, previous);
		nodes_in_progress.push_back(node);

		return node;
	}
	void clear();
	void push_error(const String &p_message, const Node *p_origin = nullptr);
#ifdef DEBUG_ENABLED
	void push_warning(const Node *p_source, GDScriptWarning::Code p_code, const Vector<String> &p_symbols);
	template <typename... Symbols>
	void push_warning(const Node *p_source, GDScriptWarning::Code p_code, const Symbols &...p_symbols) {
		push_warning(p_source, p_code, Vector<String>{ p_symbols... });
	}
	void apply_pending_warnings();
#endif
	// Setting p_force to false will prevent the completion context from being update if a context was already set before.
	// This should only be done when we push context before we consumed any tokens for the corresponding structure.
	// See parse_precedence for an example.
	void make_completion_context(CompletionType p_type, Node *p_node, int p_argument = -1, bool p_force = true);
	void make_completion_context(CompletionType p_type, Variant::Type p_builtin_type, bool p_force = true);
	// In some cases it might become necessary to alter the completion context after parsing a subexpression.
	// For example to not override COMPLETE_CALL_ARGUMENTS with COMPLETION_NONE from string literals.
	void override_completion_context(const Node *p_for_node, CompletionType p_type, Node *p_node, int p_argument = -1);
	void push_completion_call(Node *p_call);
	void pop_completion_call();
	void set_last_completion_call_arg(int p_argument);

	GDScriptTokenizer::Token advance();
	bool match(GDScriptTokenizer::Token::Type p_token_type);
	bool check(GDScriptTokenizer::Token::Type p_token_type) const;
	bool consume(GDScriptTokenizer::Token::Type p_token_type, const String &p_error_message);
	bool is_at_end() const;
	bool is_statement_end_token() const;
	bool is_statement_end() const;
	void end_statement(const String &p_context);
	void synchronize();
	void push_multiline(bool p_state);
	void pop_multiline();

	// Main blocks.
	void parse_program();
	ClassNode *parse_class(bool p_is_static);
	void parse_class_name();
	void parse_extends();
	void parse_class_body(bool p_is_multiline);
	template <typename T>
	void parse_class_member(T *(GDScriptParser::*p_parse_function)(bool), AnnotationInfo::TargetKind p_target, const String &p_member_kind, bool p_is_static = false);
	SignalNode *parse_signal(bool p_is_static);
	EnumNode *parse_enum(bool p_is_static);
	ParameterNode *parse_parameter();
	FunctionNode *parse_function(bool p_is_static);
	void parse_function_signature(FunctionNode *p_function, SuiteNode *p_body, const String &p_type);
	SuiteNode *parse_suite(const String &p_context, SuiteNode *p_suite = nullptr, bool p_for_lambda = false);
	// Annotations
	AnnotationNode *parse_annotation(uint32_t p_valid_targets);
	static bool register_annotation(const MethodInfo &p_info, uint32_t p_target_kinds, AnnotationAction p_apply, const Vector<Variant> &p_default_arguments = Vector<Variant>(), bool p_is_vararg = false);
	bool validate_annotation_arguments(AnnotationNode *p_annotation);
	void clear_unused_annotations();
	bool tool_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool icon_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool static_unload_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool onready_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	template <PropertyHint t_hint, Variant::Type t_type>
	bool export_annotations(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool export_storage_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool export_custom_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool export_tool_button_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	template <PropertyUsageFlags t_usage>
	bool export_group_annotations(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool warning_ignore_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool warning_ignore_region_annotations(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	bool rpc_annotation(AnnotationNode *p_annotation, Node *p_target, ClassNode *p_class);
	// Statements.
	Node *parse_statement();
	VariableNode *parse_variable(bool p_is_static);
	VariableNode *parse_variable(bool p_is_static, bool p_allow_property);
	VariableNode *parse_property(VariableNode *p_variable, bool p_need_indent);
	void parse_property_getter(VariableNode *p_variable);
	void parse_property_setter(VariableNode *p_variable);
	ConstantNode *parse_constant(bool p_is_static);
	AssertNode *parse_assert();
	BreakNode *parse_break();
	ContinueNode *parse_continue();
	ForNode *parse_for();
	IfNode *parse_if(const String &p_token = "if");
	MatchNode *parse_match();
	MatchBranchNode *parse_match_branch();
	PatternNode *parse_match_pattern(PatternNode *p_root_pattern = nullptr);
	WhileNode *parse_while();
	// Expressions.
	ExpressionNode *parse_expression(bool p_can_assign, bool p_stop_on_assign = false);
	ExpressionNode *parse_precedence(Precedence p_precedence, bool p_can_assign, bool p_stop_on_assign = false);
	ExpressionNode *parse_literal(ExpressionNode *p_previous_operand, bool p_can_assign);
	LiteralNode *parse_literal();
	ExpressionNode *parse_self(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_identifier(ExpressionNode *p_previous_operand, bool p_can_assign);
	IdentifierNode *parse_identifier();
	ExpressionNode *parse_builtin_constant(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_unary_operator(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_binary_operator(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_binary_not_in_operator(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_ternary_operator(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_assignment(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_array(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_dictionary(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_call(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_get_node(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_preload(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_grouping(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_cast(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_await(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_attribute(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_subscript(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_lambda(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_type_test(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_yield(ExpressionNode *p_previous_operand, bool p_can_assign);
	ExpressionNode *parse_invalid_token(ExpressionNode *p_previous_operand, bool p_can_assign);
	TypeNode *parse_type(bool p_allow_void = false);

#ifdef TOOLS_ENABLED
	int max_script_doc_line = INT_MAX;
	int min_member_doc_line = 1;
	bool has_comment(int p_line, bool p_must_be_doc = false);
	MemberDocData parse_doc_comment(int p_line, bool p_single_line = false);
	ClassDocData parse_class_doc_comment(int p_line, bool p_single_line = false);
#endif // TOOLS_ENABLED

public:
	Error parse(const String &p_source_code, const String &p_script_path, bool p_for_completion, bool p_parse_body = true);
	Error parse_binary(const Vector<uint8_t> &p_binary, const String &p_script_path);
	ClassNode *get_tree() const { return head; }
	bool is_tool() const { return _is_tool; }
	Ref<GDScriptParserRef> get_depended_parser_for(const String &p_path);
	const HashMap<String, Ref<GDScriptParserRef>> &get_depended_parsers();
	ClassNode *find_class(const String &p_qualified_name) const;
	bool has_class(const GDScriptParser::ClassNode *p_class) const;
	static Variant::Type get_builtin_type(const StringName &p_type); // Excluding `Variant::NIL` and `Variant::OBJECT`.

	CompletionContext get_completion_context() const { return completion_context; }
	CompletionCall get_completion_call() const { return completion_call; }
	void get_annotation_list(List<MethodInfo> *r_annotations) const;
	bool annotation_exists(const String &p_annotation_name) const;

	const List<ParserError> &get_errors() const { return errors; }
	const List<String> get_dependencies() const {
		// TODO: Keep track of deps.
		return List<String>();
	}
#ifdef DEBUG_ENABLED
	const List<GDScriptWarning> &get_warnings() const { return warnings; }
	const HashSet<int> &get_unsafe_lines() const { return unsafe_lines; }
	int get_last_line_number() const { return current.end_line; }
#endif

#ifdef TOOLS_ENABLED
	static HashMap<String, String> theme_color_names;

	HashMap<int, GDScriptTokenizer::CommentData> comment_data;
#endif // TOOLS_ENABLED

	GDScriptParser();
	~GDScriptParser();

#ifdef DEBUG_ENABLED
	class TreePrinter {
		int indent_level = 0;
		String indent;
		StringBuilder printed;
		bool pending_indent = false;

		void increase_indent();
		void decrease_indent();
		void push_line(const String &p_line = String());
		void push_text(const String &p_text);

		void print_annotation(const AnnotationNode *p_annotation);
		void print_array(ArrayNode *p_array);
		void print_assert(AssertNode *p_assert);
		void print_assignment(AssignmentNode *p_assignment);
		void print_await(AwaitNode *p_await);
		void print_binary_op(BinaryOpNode *p_binary_op);
		void print_call(CallNode *p_call);
		void print_cast(CastNode *p_cast);
		void print_class(ClassNode *p_class);
		void print_constant(ConstantNode *p_constant);
		void print_dictionary(DictionaryNode *p_dictionary);
		void print_expression(ExpressionNode *p_expression);
		void print_enum(EnumNode *p_enum);
		void print_for(ForNode *p_for);
		void print_function(FunctionNode *p_function, const String &p_context = "Function");
		void print_get_node(GetNodeNode *p_get_node);
		void print_if(IfNode *p_if, bool p_is_elif = false);
		void print_identifier(IdentifierNode *p_identifier);
		void print_lambda(LambdaNode *p_lambda);
		void print_literal(LiteralNode *p_literal);
		void print_match(MatchNode *p_match);
		void print_match_branch(MatchBranchNode *p_match_branch);
		void print_match_pattern(PatternNode *p_match_pattern);
		void print_parameter(ParameterNode *p_parameter);
		void print_preload(PreloadNode *p_preload);
		void print_return(ReturnNode *p_return);
		void print_self(SelfNode *p_self);
		void print_signal(SignalNode *p_signal);
		void print_statement(Node *p_statement);
		void print_subscript(SubscriptNode *p_subscript);
		void print_suite(SuiteNode *p_suite);
		void print_ternary_op(TernaryOpNode *p_ternary_op);
		void print_type(TypeNode *p_type);
		void print_type_test(TypeTestNode *p_type_test);
		void print_unary_op(UnaryOpNode *p_unary_op);
		void print_variable(VariableNode *p_variable);
		void print_while(WhileNode *p_while);

	public:
		void print_tree(const GDScriptParser &p_parser);
	};
#endif // DEBUG_ENABLED
	static void cleanup();
};

#endif // GDSCRIPT_PARSER_H
