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

#include "core/map.h"
#include "core/object.h"
#include "core/script_language.h"
#include "gdscript_functions.h"
#include "gdscript_tokenizer.h"

struct GDScriptDataType;
struct GDScriptWarning;

class GDScriptParser {
public:
	struct ClassNode;

	struct DataType {
		enum {
			BUILTIN,
			NATIVE,
			SCRIPT,
			GDSCRIPT,
			CLASS,
			UNRESOLVED
		} kind;

		bool has_type;
		bool is_constant;
		bool is_meta_type; // Whether the value can be used as a type
		bool infer_type;
		bool may_yield; // For function calls

		Variant::Type builtin_type;
		StringName native_type;
		Ref<Script> script_type;
		ClassNode *class_type;

		String to_string() const;

		_FORCE_INLINE_ bool operator!=(const DataType &other) const {
			return !operator==(other);
		}

		bool operator==(const DataType &other) const {
			if (!has_type || !other.has_type) {
				return true; // Can be considered equal for parsing purpose
			}
			if (kind != other.kind) {
				return false;
			}
			switch (kind) {
				case BUILTIN: {
					return builtin_type == other.builtin_type;
				} break;
				case NATIVE: {
					return native_type == other.native_type;
				} break;
				case GDSCRIPT:
				case SCRIPT: {
					return script_type == other.script_type;
				} break;
				case CLASS: {
					return class_type == other.class_type;
				} break;
				case UNRESOLVED: {
				} break;
			}
			return false;
		}

		DataType() :
				kind(UNRESOLVED),
				has_type(false),
				is_constant(false),
				is_meta_type(false),
				infer_type(false),
				may_yield(false),
				builtin_type(Variant::NIL),
				class_type(nullptr) {}
	};

	struct Node {
		enum Type {
			TYPE_CLASS,
			TYPE_FUNCTION,
			TYPE_BUILT_IN_FUNCTION,
			TYPE_BLOCK,
			TYPE_IDENTIFIER,
			TYPE_TYPE,
			TYPE_CONSTANT,
			TYPE_ARRAY,
			TYPE_DICTIONARY,
			TYPE_SELF,
			TYPE_OPERATOR,
			TYPE_CONTROL_FLOW,
			TYPE_LOCAL_VAR,
			TYPE_CAST,
			TYPE_ASSERT,
			TYPE_BREAKPOINT,
			TYPE_NEWLINE,
		};

		Node *next;
		int line;
		int column;
		Type type;

		virtual DataType get_datatype() const { return DataType(); }
		virtual void set_datatype(const DataType &p_datatype) {}

		virtual ~Node() {}
	};

	struct FunctionNode;
	struct BlockNode;
	struct ConstantNode;
	struct LocalVarNode;
	struct OperatorNode;

	struct ClassNode : public Node {
		bool tool;
		StringName name;
		bool extends_used;
		bool classname_used;
		StringName extends_file;
		Vector<StringName> extends_class;
		DataType base_type;
		String icon_path;

		struct Member {
			PropertyInfo _export;
#ifdef TOOLS_ENABLED
			Variant default_value;
#endif
			StringName identifier;
			DataType data_type;
			StringName setter;
			StringName getter;
			int line;
			Node *expression;
			OperatorNode *initial_assignment;
			MultiplayerAPI::RPCMode rpc_mode;
			int usages;
		};

		struct Constant {
			Node *expression;
			DataType type;
		};

		struct Signal {
			StringName name;
			Vector<StringName> arguments;
			int emissions;
			int line;
		};

		Vector<ClassNode *> subclasses;
		Vector<Member> variables;
		Map<StringName, Constant> constant_expressions;
		Vector<FunctionNode *> functions;
		Vector<FunctionNode *> static_functions;
		Vector<Signal> _signals;
		BlockNode *initializer;
		BlockNode *ready;
		ClassNode *owner;
		//Vector<Node*> initializers;
		int end_line;

		ClassNode() {
			tool = false;
			type = TYPE_CLASS;
			extends_used = false;
			classname_used = false;
			end_line = -1;
			owner = nullptr;
		}
	};

	struct FunctionNode : public Node {
		bool _static;
		MultiplayerAPI::RPCMode rpc_mode;
		bool has_yield;
		bool has_unreachable_code;
		StringName name;
		DataType return_type;
		Vector<StringName> arguments;
		Vector<DataType> argument_types;
		Vector<Node *> default_values;
		BlockNode *body;
#ifdef DEBUG_ENABLED
		Vector<int> arguments_usage;
#endif // DEBUG_ENABLED

		virtual DataType get_datatype() const { return return_type; }
		virtual void set_datatype(const DataType &p_datatype) { return_type = p_datatype; }
		int get_required_argument_count() { return arguments.size() - default_values.size(); }

		FunctionNode() {
			type = TYPE_FUNCTION;
			_static = false;
			rpc_mode = MultiplayerAPI::RPC_MODE_DISABLED;
			has_yield = false;
			has_unreachable_code = false;
		}
	};

	struct BlockNode : public Node {
		ClassNode *parent_class;
		BlockNode *parent_block;
		Vector<Node *> statements;
		Map<StringName, LocalVarNode *> variables;
		bool has_return = false;
		bool can_break = false;
		bool can_continue = false;

		Node *if_condition; //tiny hack to improve code completion on if () blocks

		//the following is useful for code completion
		List<BlockNode *> sub_blocks;
		int end_line;
		BlockNode() {
			if_condition = nullptr;
			type = TYPE_BLOCK;
			end_line = -1;
			parent_block = nullptr;
			parent_class = nullptr;
		}
	};

	struct TypeNode : public Node {
		Variant::Type vtype;
		TypeNode() { type = TYPE_TYPE; }
	};
	struct BuiltInFunctionNode : public Node {
		GDScriptFunctions::Function function;
		BuiltInFunctionNode() { type = TYPE_BUILT_IN_FUNCTION; }
	};

	struct IdentifierNode : public Node {
		StringName name;
		BlockNode *declared_block; // Simplify lookup by checking if it is declared locally
		DataType datatype;
		virtual DataType get_datatype() const { return datatype; }
		virtual void set_datatype(const DataType &p_datatype) { datatype = p_datatype; }
		IdentifierNode() {
			type = TYPE_IDENTIFIER;
			declared_block = nullptr;
		}
	};

	struct LocalVarNode : public Node {
		StringName name;
		Node *assign;
		OperatorNode *assign_op;
		int assignments;
		int usages;
		DataType datatype;
		virtual DataType get_datatype() const { return datatype; }
		virtual void set_datatype(const DataType &p_datatype) { datatype = p_datatype; }
		LocalVarNode() {
			type = TYPE_LOCAL_VAR;
			assign = nullptr;
			assign_op = nullptr;
			assignments = 0;
			usages = 0;
		}
	};

	struct ConstantNode : public Node {
		Variant value;
		DataType datatype;
		virtual DataType get_datatype() const { return datatype; }
		virtual void set_datatype(const DataType &p_datatype) { datatype = p_datatype; }
		ConstantNode() { type = TYPE_CONSTANT; }
	};

	struct ArrayNode : public Node {
		Vector<Node *> elements;
		DataType datatype;
		virtual DataType get_datatype() const { return datatype; }
		virtual void set_datatype(const DataType &p_datatype) { datatype = p_datatype; }
		ArrayNode() {
			type = TYPE_ARRAY;
			datatype.has_type = true;
			datatype.kind = DataType::BUILTIN;
			datatype.builtin_type = Variant::ARRAY;
		}
	};

	struct DictionaryNode : public Node {
		struct Pair {
			Node *key;
			Node *value;
		};

		Vector<Pair> elements;
		DataType datatype;
		virtual DataType get_datatype() const { return datatype; }
		virtual void set_datatype(const DataType &p_datatype) { datatype = p_datatype; }
		DictionaryNode() {
			type = TYPE_DICTIONARY;
			datatype.has_type = true;
			datatype.kind = DataType::BUILTIN;
			datatype.builtin_type = Variant::DICTIONARY;
		}
	};

	struct SelfNode : public Node {
		SelfNode() { type = TYPE_SELF; }
	};

	struct OperatorNode : public Node {
		enum Operator {
			//call/constructor operator
			OP_CALL,
			OP_PARENT_CALL,
			OP_YIELD,
			OP_IS,
			OP_IS_BUILTIN,
			//indexing operator
			OP_INDEX,
			OP_INDEX_NAMED,
			//unary operators
			OP_NEG,
			OP_POS,
			OP_NOT,
			OP_BIT_INVERT,
			//binary operators (in precedence order)
			OP_IN,
			OP_EQUAL,
			OP_NOT_EQUAL,
			OP_LESS,
			OP_LESS_EQUAL,
			OP_GREATER,
			OP_GREATER_EQUAL,
			OP_AND,
			OP_OR,
			OP_ADD,
			OP_SUB,
			OP_MUL,
			OP_DIV,
			OP_MOD,
			OP_SHIFT_LEFT,
			OP_SHIFT_RIGHT,
			OP_INIT_ASSIGN,
			OP_ASSIGN,
			OP_ASSIGN_ADD,
			OP_ASSIGN_SUB,
			OP_ASSIGN_MUL,
			OP_ASSIGN_DIV,
			OP_ASSIGN_MOD,
			OP_ASSIGN_SHIFT_LEFT,
			OP_ASSIGN_SHIFT_RIGHT,
			OP_ASSIGN_BIT_AND,
			OP_ASSIGN_BIT_OR,
			OP_ASSIGN_BIT_XOR,
			OP_BIT_AND,
			OP_BIT_OR,
			OP_BIT_XOR,
			//ternary operators
			OP_TERNARY_IF,
			OP_TERNARY_ELSE,
		};

		Operator op;

		Vector<Node *> arguments;
		DataType datatype;
		virtual DataType get_datatype() const { return datatype; }
		virtual void set_datatype(const DataType &p_datatype) { datatype = p_datatype; }
		OperatorNode() { type = TYPE_OPERATOR; }
	};

	struct PatternNode : public Node {
		enum PatternType {
			PT_CONSTANT,
			PT_BIND,
			PT_DICTIONARY,
			PT_ARRAY,
			PT_IGNORE_REST,
			PT_WILDCARD
		};

		PatternType pt_type;

		Node *constant;
		StringName bind;
		Map<ConstantNode *, PatternNode *> dictionary;
		Vector<PatternNode *> array;
	};

	struct PatternBranchNode : public Node {
		Vector<PatternNode *> patterns;
		BlockNode *body;
	};

	struct MatchNode : public Node {
		Node *val_to_match;
		Vector<PatternBranchNode *> branches;

		struct CompiledPatternBranch {
			Node *compiled_pattern;
			BlockNode *body;
		};

		Vector<CompiledPatternBranch> compiled_pattern_branches;
	};

	struct ControlFlowNode : public Node {
		enum CFType {
			CF_IF,
			CF_FOR,
			CF_WHILE,
			CF_BREAK,
			CF_CONTINUE,
			CF_RETURN,
			CF_MATCH
		};

		CFType cf_type;
		Vector<Node *> arguments;
		BlockNode *body;
		BlockNode *body_else;

		MatchNode *match;

		ControlFlowNode *_else; //used for if
		ControlFlowNode() {
			type = TYPE_CONTROL_FLOW;
			cf_type = CF_IF;
			body = nullptr;
			body_else = nullptr;
		}
	};

	struct CastNode : public Node {
		Node *source_node;
		DataType cast_type;
		DataType return_type;
		virtual DataType get_datatype() const { return return_type; }
		virtual void set_datatype(const DataType &p_datatype) { return_type = p_datatype; }
		CastNode() { type = TYPE_CAST; }
	};

	struct AssertNode : public Node {
		Node *condition;
		Node *message;
		AssertNode() :
				condition(nullptr),
				message(nullptr) {
			type = TYPE_ASSERT;
		}
	};

	struct BreakpointNode : public Node {
		BreakpointNode() { type = TYPE_BREAKPOINT; }
	};

	struct NewLineNode : public Node {
		NewLineNode() { type = TYPE_NEWLINE; }
	};

	struct Expression {
		bool is_op;
		union {
			OperatorNode::Operator op;
			Node *node;
		};
	};

	enum CompletionType {
		COMPLETION_NONE,
		COMPLETION_BUILT_IN_TYPE_CONSTANT,
		COMPLETION_GET_NODE,
		COMPLETION_FUNCTION,
		COMPLETION_IDENTIFIER,
		COMPLETION_EXTENDS,
		COMPLETION_PARENT_FUNCTION,
		COMPLETION_METHOD,
		COMPLETION_CALL_ARGUMENTS,
		COMPLETION_RESOURCE_PATH,
		COMPLETION_INDEX,
		COMPLETION_VIRTUAL_FUNC,
		COMPLETION_YIELD,
		COMPLETION_ASSIGN,
		COMPLETION_TYPE_HINT,
		COMPLETION_TYPE_HINT_INDEX,
	};

private:
	GDScriptTokenizer *tokenizer;

	Node *head;
	Node *list;
	template <class T>
	T *alloc_node();

	bool validating;
	bool for_completion;
	int parenthesis;
	bool error_set;
	String error;
	int error_line;
	int error_column;
	bool check_types;
	bool dependencies_only;
	List<String> dependencies;
#ifdef DEBUG_ENABLED
	Set<int> *safe_lines;
#endif // DEBUG_ENABLED

#ifdef DEBUG_ENABLED
	List<GDScriptWarning> warnings;
#endif // DEBUG_ENABLED

	int pending_newline;

	struct IndentLevel {
		int indent;
		int tabs;

		bool is_mixed(IndentLevel other) {
			return (
					(indent == other.indent && tabs != other.tabs) ||
					(indent > other.indent && tabs < other.tabs) ||
					(indent < other.indent && tabs > other.tabs));
		}

		IndentLevel() :
				indent(0),
				tabs(0) {}

		IndentLevel(int p_indent, int p_tabs) :
				indent(p_indent),
				tabs(p_tabs) {}
	};

	List<IndentLevel> indent_level;

	String base_path;
	String self_path;

	ClassNode *current_class;
	FunctionNode *current_function;
	BlockNode *current_block;

	bool _get_completable_identifier(CompletionType p_type, StringName &identifier);
	void _make_completable_call(int p_arg);

	CompletionType completion_type;
	StringName completion_cursor;
	Variant::Type completion_built_in_constant;
	Node *completion_node;
	ClassNode *completion_class;
	FunctionNode *completion_function;
	BlockNode *completion_block;
	int completion_line;
	int completion_argument;
	bool completion_found;
	bool completion_ident_is_call;

	PropertyInfo current_export;

	MultiplayerAPI::RPCMode rpc_mode;

	void _set_error(const String &p_error, int p_line = -1, int p_column = -1);
#ifdef DEBUG_ENABLED
	void _add_warning(int p_code, int p_line = -1, const String &p_symbol1 = String(), const String &p_symbol2 = String(), const String &p_symbol3 = String(), const String &p_symbol4 = String());
	void _add_warning(int p_code, int p_line, const Vector<String> &p_symbols);
#endif // DEBUG_ENABLED
	bool _recover_from_completion();

	bool _parse_arguments(Node *p_parent, Vector<Node *> &p_args, bool p_static, bool p_can_codecomplete = false, bool p_parsing_constant = false);
	bool _enter_indent_block(BlockNode *p_block = nullptr);
	bool _parse_newline();
	Node *_parse_expression(Node *p_parent, bool p_static, bool p_allow_assign = false, bool p_parsing_constant = false);
	Node *_reduce_expression(Node *p_node, bool p_to_const = false);
	Node *_parse_and_reduce_expression(Node *p_parent, bool p_static, bool p_reduce_const = false, bool p_allow_assign = false);
	bool _reduce_export_var_type(Variant &p_value, int p_line = 0);
	const Variant *_try_to_find_constant_value_for_expression(const Node *p_expr) const;

	PatternNode *_parse_pattern(bool p_static);
	void _parse_pattern_block(BlockNode *p_block, Vector<PatternBranchNode *> &p_branches, bool p_static);
	void _transform_match_statment(MatchNode *p_match_statement);
	void _generate_pattern(PatternNode *p_pattern, Node *p_node_to_match, Node *&p_resulting_node, Map<StringName, Node *> &p_bindings);

	void _parse_block(BlockNode *p_block, bool p_static);
	void _parse_extends(ClassNode *p_class);
	void _parse_class(ClassNode *p_class);
	bool _end_statement();
	void _set_end_statement_error(String p_name);

	void _determine_inheritance(ClassNode *p_class, bool p_recursive = true);
	bool _parse_type(DataType &r_type, bool p_can_be_void = false);
	DataType _resolve_type(const DataType &p_source, int p_line);
	DataType _type_from_variant(const Variant &p_value) const;
	DataType _type_from_property(const PropertyInfo &p_property, bool p_nil_is_variant = true) const;
	DataType _type_from_gdtype(const GDScriptDataType &p_gdtype) const;
	DataType _get_operation_type(const Variant::Operator p_op, const DataType &p_a, const DataType &p_b, bool &r_valid) const;
	Variant::Operator _get_variant_operation(const OperatorNode::Operator &p_op) const;
	bool _get_function_signature(DataType &p_base_type, const StringName &p_function, DataType &r_return_type, List<DataType> &r_arg_types, int &r_default_arg_count, bool &r_static, bool &r_vararg) const;
	bool _get_member_type(const DataType &p_base_type, const StringName &p_member, DataType &r_member_type, bool *r_is_const = nullptr) const;
	bool _is_type_compatible(const DataType &p_container, const DataType &p_expression, bool p_allow_implicit_conversion = false) const;
	Node *_get_default_value_for_type(const DataType &p_type, int p_line = -1);

	DataType _reduce_node_type(Node *p_node);
	DataType _reduce_function_call_type(const OperatorNode *p_call);
	DataType _reduce_identifier_type(const DataType *p_base_type, const StringName &p_identifier, int p_line, bool p_is_indexing);
	void _check_class_level_types(ClassNode *p_class);
	void _check_class_blocks_types(ClassNode *p_class);
	void _check_function_types(FunctionNode *p_function);
	void _check_block_types(BlockNode *p_block);
	_FORCE_INLINE_ void _mark_line_as_safe(int p_line) const {
#ifdef DEBUG_ENABLED
		if (safe_lines) {
			safe_lines->insert(p_line);
		}
#endif // DEBUG_ENABLED
	}
	_FORCE_INLINE_ void _mark_line_as_unsafe(int p_line) const {
#ifdef DEBUG_ENABLED
		if (safe_lines) {
			safe_lines->erase(p_line);
		}
#endif // DEBUG_ENABLED
	}

	Error _parse(const String &p_base_path);

public:
	bool has_error() const;
	String get_error() const;
	int get_error_line() const;
	int get_error_column() const;
#ifdef DEBUG_ENABLED
	const List<GDScriptWarning> &get_warnings() const { return warnings; }
#endif // DEBUG_ENABLED
	Error parse(const String &p_code, const String &p_base_path = "", bool p_just_validate = false, const String &p_self_path = "", bool p_for_completion = false, Set<int> *r_safe_lines = nullptr, bool p_dependencies_only = false);
	Error parse_bytecode(const Vector<uint8_t> &p_bytecode, const String &p_base_path = "", const String &p_self_path = "");

	bool is_tool_script() const;
	const Node *get_parse_tree() const;

	//completion info

	CompletionType get_completion_type();
	StringName get_completion_cursor();
	int get_completion_line();
	Variant::Type get_completion_built_in_constant();
	Node *get_completion_node();
	ClassNode *get_completion_class();
	BlockNode *get_completion_block();
	FunctionNode *get_completion_function();
	int get_completion_argument_index();
	int get_completion_identifier_is_function();

	const List<String> &get_dependencies() const { return dependencies; }

	static bool is_type_assignable_from(const DataType &p_container, const DataType &p_expression, bool p_allow_implicit_conversion = false);
	static DataType get_base_data_type(const DataType &p_type);
	static DataType get_common_type(const DataType &p_type, const DataType &p_other_type);
	static bool has_common_type(const DataType &p_type, const DataType &p_other_type);
	static bool is_type_derived_from(const DataType &p_type, const DataType &p_other_type);
	static bool is_type_more_specific_than(const DataType &p_type, const DataType &p_other_type);

	void clear();
	GDScriptParser();
	~GDScriptParser();
};

#endif // GDSCRIPT_PARSER_H
