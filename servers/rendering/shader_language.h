/**************************************************************************/
/*  shader_language.h                                                     */
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

#ifndef SHADER_LANGUAGE_H
#define SHADER_LANGUAGE_H

#include "core/object/script_language.h"
#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/templates/rb_map.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"
#include "scene/resources/shader_include.h"

#ifdef DEBUG_ENABLED
#include "shader_warnings.h"
#endif // DEBUG_ENABLED

class ShaderLanguage {
public:
	struct TkPos {
		int char_idx;
		int tk_line;
	};

	enum TokenType {
		TK_EMPTY,
		TK_IDENTIFIER,
		TK_TRUE,
		TK_FALSE,
		TK_FLOAT_CONSTANT,
		TK_INT_CONSTANT,
		TK_UINT_CONSTANT,
		TK_STRING_CONSTANT,
		TK_TYPE_VOID,
		TK_TYPE_BOOL,
		TK_TYPE_BVEC2,
		TK_TYPE_BVEC3,
		TK_TYPE_BVEC4,
		TK_TYPE_INT,
		TK_TYPE_IVEC2,
		TK_TYPE_IVEC3,
		TK_TYPE_IVEC4,
		TK_TYPE_UINT,
		TK_TYPE_UVEC2,
		TK_TYPE_UVEC3,
		TK_TYPE_UVEC4,
		TK_TYPE_FLOAT,
		TK_TYPE_VEC2,
		TK_TYPE_VEC3,
		TK_TYPE_VEC4,
		TK_TYPE_MAT2,
		TK_TYPE_MAT3,
		TK_TYPE_MAT4,
		TK_TYPE_SAMPLER2D,
		TK_TYPE_ISAMPLER2D,
		TK_TYPE_USAMPLER2D,
		TK_TYPE_SAMPLER2DARRAY,
		TK_TYPE_ISAMPLER2DARRAY,
		TK_TYPE_USAMPLER2DARRAY,
		TK_TYPE_SAMPLER3D,
		TK_TYPE_ISAMPLER3D,
		TK_TYPE_USAMPLER3D,
		TK_TYPE_SAMPLERCUBE,
		TK_TYPE_SAMPLERCUBEARRAY,
		TK_TYPE_SAMPLEREXT,
		TK_INTERPOLATION_FLAT,
		TK_INTERPOLATION_SMOOTH,
		TK_CONST,
		TK_STRUCT,
		TK_PRECISION_LOW,
		TK_PRECISION_MID,
		TK_PRECISION_HIGH,
		TK_OP_EQUAL,
		TK_OP_NOT_EQUAL,
		TK_OP_LESS,
		TK_OP_LESS_EQUAL,
		TK_OP_GREATER,
		TK_OP_GREATER_EQUAL,
		TK_OP_AND,
		TK_OP_OR,
		TK_OP_NOT,
		TK_OP_ADD,
		TK_OP_SUB,
		TK_OP_MUL,
		TK_OP_DIV,
		TK_OP_MOD,
		TK_OP_SHIFT_LEFT,
		TK_OP_SHIFT_RIGHT,
		TK_OP_ASSIGN,
		TK_OP_ASSIGN_ADD,
		TK_OP_ASSIGN_SUB,
		TK_OP_ASSIGN_MUL,
		TK_OP_ASSIGN_DIV,
		TK_OP_ASSIGN_MOD,
		TK_OP_ASSIGN_SHIFT_LEFT,
		TK_OP_ASSIGN_SHIFT_RIGHT,
		TK_OP_ASSIGN_BIT_AND,
		TK_OP_ASSIGN_BIT_OR,
		TK_OP_ASSIGN_BIT_XOR,
		TK_OP_BIT_AND,
		TK_OP_BIT_OR,
		TK_OP_BIT_XOR,
		TK_OP_BIT_INVERT,
		TK_OP_INCREMENT,
		TK_OP_DECREMENT,
		TK_CF_IF,
		TK_CF_ELSE,
		TK_CF_FOR,
		TK_CF_WHILE,
		TK_CF_DO,
		TK_CF_SWITCH,
		TK_CF_CASE,
		TK_CF_DEFAULT,
		TK_CF_BREAK,
		TK_CF_CONTINUE,
		TK_CF_RETURN,
		TK_CF_DISCARD,
		TK_BRACKET_OPEN,
		TK_BRACKET_CLOSE,
		TK_CURLY_BRACKET_OPEN,
		TK_CURLY_BRACKET_CLOSE,
		TK_PARENTHESIS_OPEN,
		TK_PARENTHESIS_CLOSE,
		TK_QUESTION,
		TK_COMMA,
		TK_COLON,
		TK_SEMICOLON,
		TK_PERIOD,
		TK_UNIFORM,
		TK_UNIFORM_GROUP,
		TK_INSTANCE,
		TK_GLOBAL,
		TK_VARYING,
		TK_ARG_IN,
		TK_ARG_OUT,
		TK_ARG_INOUT,
		TK_RENDER_MODE,
		TK_HINT_DEFAULT_WHITE_TEXTURE,
		TK_HINT_DEFAULT_BLACK_TEXTURE,
		TK_HINT_DEFAULT_TRANSPARENT_TEXTURE,
		TK_HINT_NORMAL_TEXTURE,
		TK_HINT_ROUGHNESS_NORMAL_TEXTURE,
		TK_HINT_ROUGHNESS_R,
		TK_HINT_ROUGHNESS_G,
		TK_HINT_ROUGHNESS_B,
		TK_HINT_ROUGHNESS_A,
		TK_HINT_ROUGHNESS_GRAY,
		TK_HINT_ANISOTROPY_TEXTURE,
		TK_HINT_SOURCE_COLOR,
		TK_HINT_RANGE,
		TK_HINT_ENUM,
		TK_HINT_INSTANCE_INDEX,
		TK_HINT_SCREEN_TEXTURE,
		TK_HINT_NORMAL_ROUGHNESS_TEXTURE,
		TK_HINT_DEPTH_TEXTURE,
		TK_FILTER_NEAREST,
		TK_FILTER_LINEAR,
		TK_FILTER_NEAREST_MIPMAP,
		TK_FILTER_LINEAR_MIPMAP,
		TK_FILTER_NEAREST_MIPMAP_ANISOTROPIC,
		TK_FILTER_LINEAR_MIPMAP_ANISOTROPIC,
		TK_REPEAT_ENABLE,
		TK_REPEAT_DISABLE,
		TK_SHADER_TYPE,
		TK_CURSOR,
		TK_ERROR,
		TK_EOF,
		TK_MAX
	};

/* COMPILER */

// lame work around to Apple defining this as a macro in 10.12 SDK
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif

	enum DataType {
		TYPE_VOID,
		TYPE_BOOL,
		TYPE_BVEC2,
		TYPE_BVEC3,
		TYPE_BVEC4,
		TYPE_INT,
		TYPE_IVEC2,
		TYPE_IVEC3,
		TYPE_IVEC4,
		TYPE_UINT,
		TYPE_UVEC2,
		TYPE_UVEC3,
		TYPE_UVEC4,
		TYPE_FLOAT,
		TYPE_VEC2,
		TYPE_VEC3,
		TYPE_VEC4,
		TYPE_MAT2,
		TYPE_MAT3,
		TYPE_MAT4,
		TYPE_SAMPLER2D,
		TYPE_ISAMPLER2D,
		TYPE_USAMPLER2D,
		TYPE_SAMPLER2DARRAY,
		TYPE_ISAMPLER2DARRAY,
		TYPE_USAMPLER2DARRAY,
		TYPE_SAMPLER3D,
		TYPE_ISAMPLER3D,
		TYPE_USAMPLER3D,
		TYPE_SAMPLERCUBE,
		TYPE_SAMPLERCUBEARRAY,
		TYPE_SAMPLEREXT,
		TYPE_STRUCT,
		TYPE_MAX
	};

	enum DataPrecision {
		PRECISION_LOWP,
		PRECISION_MEDIUMP,
		PRECISION_HIGHP,
		PRECISION_DEFAULT,
	};

	enum DataInterpolation {
		INTERPOLATION_FLAT,
		INTERPOLATION_SMOOTH,
		INTERPOLATION_DEFAULT,
	};

	enum Operator {
		OP_EQUAL,
		OP_NOT_EQUAL,
		OP_LESS,
		OP_LESS_EQUAL,
		OP_GREATER,
		OP_GREATER_EQUAL,
		OP_AND,
		OP_OR,
		OP_NOT,
		OP_NEGATE,
		OP_ADD,
		OP_SUB,
		OP_MUL,
		OP_DIV,
		OP_MOD,
		OP_SHIFT_LEFT,
		OP_SHIFT_RIGHT,
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
		OP_BIT_INVERT,
		OP_INCREMENT,
		OP_DECREMENT,
		OP_SELECT_IF,
		OP_SELECT_ELSE, //used only internally, then only IF appears with 3 arguments
		OP_POST_INCREMENT,
		OP_POST_DECREMENT,
		OP_CALL,
		OP_CONSTRUCT,
		OP_STRUCT,
		OP_INDEX,
		OP_EMPTY,
		OP_MAX
	};

	enum FlowOperation {
		FLOW_OP_IF,
		FLOW_OP_RETURN,
		FLOW_OP_FOR,
		FLOW_OP_WHILE,
		FLOW_OP_DO,
		FLOW_OP_BREAK,
		FLOW_OP_SWITCH,
		FLOW_OP_CASE,
		FLOW_OP_DEFAULT,
		FLOW_OP_CONTINUE,
		FLOW_OP_DISCARD
	};

	enum ArgumentQualifier {
		ARGUMENT_QUALIFIER_IN,
		ARGUMENT_QUALIFIER_OUT,
		ARGUMENT_QUALIFIER_INOUT,
	};

	enum SubClassTag {
		TAG_GLOBAL,
		TAG_ARRAY,
	};

	enum TextureFilter {
		FILTER_NEAREST,
		FILTER_LINEAR,
		FILTER_NEAREST_MIPMAP,
		FILTER_LINEAR_MIPMAP,
		FILTER_NEAREST_MIPMAP_ANISOTROPIC,
		FILTER_LINEAR_MIPMAP_ANISOTROPIC,
		FILTER_DEFAULT,
	};

	enum TextureRepeat {
		REPEAT_DISABLE,
		REPEAT_ENABLE,
		REPEAT_DEFAULT,
	};

	enum {
		MAX_INSTANCE_UNIFORM_INDICES = 16
	};

	struct VaryingFunctionNames {
		StringName fragment;
		StringName vertex;
		StringName light;
		VaryingFunctionNames() {
			fragment = "fragment";
			vertex = "vertex";
			light = "light";
		}
	};

	union Scalar {
		bool boolean = false;
		float real;
		int32_t sint;
		uint32_t uint;
	};

	struct Node {
		Node *next = nullptr;

		enum Type {
			NODE_TYPE_SHADER,
			NODE_TYPE_FUNCTION,
			NODE_TYPE_BLOCK,
			NODE_TYPE_VARIABLE,
			NODE_TYPE_VARIABLE_DECLARATION,
			NODE_TYPE_CONSTANT,
			NODE_TYPE_OPERATOR,
			NODE_TYPE_CONTROL_FLOW,
			NODE_TYPE_MEMBER,
			NODE_TYPE_ARRAY,
			NODE_TYPE_ARRAY_CONSTRUCT,
			NODE_TYPE_STRUCT,
		};

		Type type;

		virtual DataType get_datatype() const { return TYPE_VOID; }
		virtual String get_datatype_name() const { return ""; }
		virtual int get_array_size() const { return 0; }
		virtual bool is_indexed() const { return false; }
		virtual Vector<Scalar> get_values() const { return Vector<Scalar>(); }

		Node(Type t) :
				type(t) {}
		virtual ~Node() {}
	};

	template <typename T>
	T *alloc_node() {
		T *node = memnew(T);
		node->next = nodes;
		nodes = node;
		return node;
	}

	Node *nodes = nullptr;

	struct OperatorNode : public Node {
		DataType return_cache = TYPE_VOID;
		DataPrecision return_precision_cache = PRECISION_DEFAULT;
		int return_array_size = 0;
		Operator op = OP_EQUAL;
		StringName struct_name;
		Vector<Node *> arguments;
		Vector<Scalar> values;

		virtual DataType get_datatype() const override { return return_cache; }
		virtual String get_datatype_name() const override { return String(struct_name); }
		virtual int get_array_size() const override { return return_array_size; }
		virtual bool is_indexed() const override { return op == OP_INDEX; }
		virtual Vector<Scalar> get_values() const override { return values; }

		OperatorNode() :
				Node(NODE_TYPE_OPERATOR) {}
	};

	struct VariableNode : public Node {
		DataType datatype_cache = TYPE_VOID;
		StringName name;
		StringName rname;
		StringName struct_name;
		bool is_const = false;
		bool is_local = false;

		virtual DataType get_datatype() const override { return datatype_cache; }
		virtual String get_datatype_name() const override { return String(struct_name); }

		VariableNode() :
				Node(NODE_TYPE_VARIABLE) {}
	};

	struct VariableDeclarationNode : public Node {
		DataPrecision precision = PRECISION_DEFAULT;
		DataType datatype = TYPE_VOID;
		String struct_name;
		bool is_const = false;

		struct Declaration {
			StringName name;
			uint32_t size = 0U;
			Node *size_expression = nullptr;
			Vector<Node *> initializer;
			bool single_expression = false;
		};
		Vector<Declaration> declarations;

		virtual DataType get_datatype() const override { return datatype; }

		VariableDeclarationNode() :
				Node(NODE_TYPE_VARIABLE_DECLARATION) {}
	};

	struct ArrayNode : public Node {
		DataType datatype_cache = TYPE_VOID;
		StringName struct_name;
		StringName name;
		Node *index_expression = nullptr;
		Node *call_expression = nullptr;
		Node *assign_expression = nullptr;
		bool is_const = false;
		int array_size = 0;
		bool is_local = false;

		virtual DataType get_datatype() const override { return call_expression ? call_expression->get_datatype() : datatype_cache; }
		virtual String get_datatype_name() const override { return call_expression ? call_expression->get_datatype_name() : String(struct_name); }
		virtual int get_array_size() const override { return (index_expression || call_expression) ? 0 : array_size; }
		virtual bool is_indexed() const override { return index_expression != nullptr; }

		ArrayNode() :
				Node(NODE_TYPE_ARRAY) {}
	};

	struct ArrayConstructNode : public Node {
		DataType datatype = TYPE_VOID;
		String struct_name;
		Vector<Node *> initializer;

		virtual DataType get_datatype() const override { return datatype; }
		virtual String get_datatype_name() const override { return struct_name; }
		virtual int get_array_size() const override { return initializer.size(); }

		ArrayConstructNode() :
				Node(NODE_TYPE_ARRAY_CONSTRUCT) {}
	};

	struct ConstantNode : public Node {
		DataType datatype = TYPE_VOID;
		String struct_name = "";
		int array_size = 0;

		Vector<Scalar> values;
		Vector<VariableDeclarationNode::Declaration> array_declarations;

		virtual DataType get_datatype() const override { return datatype; }
		virtual String get_datatype_name() const override { return struct_name; }
		virtual int get_array_size() const override { return array_size; }
		virtual Vector<Scalar> get_values() const override {
			return values;
		}

		ConstantNode() :
				Node(NODE_TYPE_CONSTANT) {}
	};

	struct FunctionNode;

	struct BlockNode : public Node {
		FunctionNode *parent_function = nullptr;
		BlockNode *parent_block = nullptr;

		enum BlockType {
			BLOCK_TYPE_STANDARD,
			BLOCK_TYPE_FOR_INIT,
			BLOCK_TYPE_FOR_CONDITION,
			BLOCK_TYPE_FOR_EXPRESSION,
			BLOCK_TYPE_SWITCH,
			BLOCK_TYPE_CASE,
			BLOCK_TYPE_DEFAULT,
		};

		int block_type = BLOCK_TYPE_STANDARD;
		SubClassTag block_tag = SubClassTag::TAG_GLOBAL;

		struct Variable {
			DataType type;
			StringName struct_name;
			DataPrecision precision;
			int line; //for completion
			int array_size;
			bool is_const;
			Vector<Scalar> values;
		};

		HashMap<StringName, Variable> variables;
		List<Node *> statements;
		bool single_statement = false;
		bool use_comma_between_statements = false;
		bool use_op_eval = true;

		DataType expected_type = TYPE_VOID;
		HashSet<int> constants;

		BlockNode() :
				Node(NODE_TYPE_BLOCK) {}
	};

	struct ControlFlowNode : public Node {
		FlowOperation flow_op = FLOW_OP_IF;
		Vector<Node *> expressions;
		Vector<BlockNode *> blocks;

		ControlFlowNode() :
				Node(NODE_TYPE_CONTROL_FLOW) {}
	};

	struct MemberNode : public Node {
		DataType basetype = TYPE_VOID;
		bool basetype_const = false;
		StringName base_struct_name;
		DataPrecision precision = PRECISION_DEFAULT;
		DataType datatype = TYPE_VOID;
		int array_size = 0;
		StringName struct_name;
		StringName name;
		Node *owner = nullptr;
		Node *index_expression = nullptr;
		Node *assign_expression = nullptr;
		Node *call_expression = nullptr;
		bool has_swizzling_duplicates = false;

		virtual DataType get_datatype() const override { return call_expression ? call_expression->get_datatype() : datatype; }
		virtual String get_datatype_name() const override { return call_expression ? call_expression->get_datatype_name() : String(struct_name); }
		virtual int get_array_size() const override { return (index_expression || call_expression) ? 0 : array_size; }
		virtual bool is_indexed() const override { return index_expression != nullptr || call_expression != nullptr; }

		MemberNode() :
				Node(NODE_TYPE_MEMBER) {}
	};

	struct StructNode : public Node {
		List<MemberNode *> members;
		StructNode() :
				Node(NODE_TYPE_STRUCT) {}
	};

	struct ShaderNode : public Node {
		struct Constant {
			StringName name;
			DataType type;
			StringName struct_name;
			DataPrecision precision;
			Node *initializer = nullptr;
			int array_size;
		};

		struct Function {
			StringName name;
			StringName rname;
			FunctionNode *function = nullptr;
			HashSet<StringName> uses_function;
			bool callable;
		};

		struct Struct {
			StringName name;
			StructNode *shader_struct = nullptr;
		};

		struct Varying {
			enum Stage {
				STAGE_UNKNOWN,
				STAGE_VERTEX, // transition stage to STAGE_VERTEX_TO_FRAGMENT_LIGHT, emits warning if it's not used
				STAGE_FRAGMENT, // transition stage to STAGE_FRAGMENT_TO_LIGHT, emits warning if it's not used
				STAGE_VERTEX_TO_FRAGMENT_LIGHT,
				STAGE_FRAGMENT_TO_LIGHT,
			};

			Stage stage = STAGE_UNKNOWN;
			DataType type = TYPE_VOID;
			DataInterpolation interpolation = INTERPOLATION_FLAT;
			DataPrecision precision = PRECISION_DEFAULT;
			int array_size = 0;
			TkPos tkpos;

			Varying() {}
		};

		struct Uniform {
			enum Hint {
				HINT_NONE,
				HINT_RANGE,
				HINT_ENUM,
				HINT_SOURCE_COLOR,
				HINT_NORMAL,
				HINT_ROUGHNESS_NORMAL,
				HINT_ROUGHNESS_R,
				HINT_ROUGHNESS_G,
				HINT_ROUGHNESS_B,
				HINT_ROUGHNESS_A,
				HINT_ROUGHNESS_GRAY,
				HINT_DEFAULT_BLACK,
				HINT_DEFAULT_WHITE,
				HINT_DEFAULT_TRANSPARENT,
				HINT_ANISOTROPY,
				HINT_SCREEN_TEXTURE,
				HINT_NORMAL_ROUGHNESS_TEXTURE,
				HINT_DEPTH_TEXTURE,
				HINT_MAX
			};

			enum Scope {
				SCOPE_LOCAL,
				SCOPE_INSTANCE,
				SCOPE_GLOBAL,
			};

			int order = 0;
			int prop_order = 0;
			int texture_order = 0;
			int texture_binding = 0;
			DataType type = TYPE_VOID;
			DataPrecision precision = PRECISION_DEFAULT;
			int array_size = 0;
			Vector<Scalar> default_value;
			Scope scope = SCOPE_LOCAL;
			Hint hint = HINT_NONE;
			bool use_color = false;
			TextureFilter filter = FILTER_DEFAULT;
			TextureRepeat repeat = REPEAT_DEFAULT;
			float hint_range[3];
			PackedStringArray hint_enum_names;
			int instance_index = 0;
			String group;
			String subgroup;

			_FORCE_INLINE_ bool is_texture() const {
				// Order is assigned to -1 for texture uniforms.
				return order < 0;
			}

			Uniform() {
				hint_range[0] = 0.0f;
				hint_range[1] = 1.0f;
				hint_range[2] = 0.001f;
			}
		};

		HashMap<StringName, Constant> constants;
		HashMap<StringName, Varying> varyings;
		HashMap<StringName, Uniform> uniforms;
		HashMap<StringName, Struct> structs;
		HashMap<StringName, Function> functions;
		Vector<StringName> render_modes;

		Vector<Function> vfunctions;
		Vector<Constant> vconstants;
		Vector<Struct> vstructs;

		ShaderNode() :
				Node(NODE_TYPE_SHADER) {}
	};

	struct FunctionNode : public Node {
		struct Argument {
			ArgumentQualifier qualifier;
			StringName name;
			DataType type;
			StringName struct_name;
			DataPrecision precision;
			//for passing textures as arguments
			bool tex_argument_check;
			TextureFilter tex_argument_filter;
			TextureRepeat tex_argument_repeat;
			bool tex_builtin_check;
			StringName tex_builtin;
			ShaderNode::Uniform::Hint tex_hint;
			bool is_const;
			int array_size;

			HashMap<StringName, HashSet<int>> tex_argument_connect;
		};

		StringName name;
		StringName rname;
		DataType return_type = TYPE_VOID;
		StringName return_struct_name;
		DataPrecision return_precision = PRECISION_DEFAULT;
		int return_array_size = 0;
		Vector<Argument> arguments;
		BlockNode *body = nullptr;
		bool can_discard = false;

		virtual DataType get_datatype() const override { return return_type; }
		virtual String get_datatype_name() const override { return String(return_struct_name); }
		virtual int get_array_size() const override { return return_array_size; }

		FunctionNode() :
				Node(NODE_TYPE_FUNCTION) {}
	};

	struct UniformOrderComparator {
		_FORCE_INLINE_ bool operator()(const Pair<StringName, int> &A, const Pair<StringName, int> &B) const {
			return A.second < B.second;
		}
	};

	struct Expression {
		bool is_op;
		union {
			Operator op;
			Node *node = nullptr;
		};
	};

	struct ExpressionInfo {
		Vector<Expression> *expression = nullptr;
		TokenType tt_break = TK_EMPTY;
		bool is_last_expr = false;
	};

	struct VarInfo {
		StringName name;
		DataType type;
	};

	enum CompletionType {
		COMPLETION_NONE,
		COMPLETION_SHADER_TYPE,
		COMPLETION_RENDER_MODE,
		COMPLETION_MAIN_FUNCTION,
		COMPLETION_IDENTIFIER,
		COMPLETION_FUNCTION_CALL,
		COMPLETION_CALL_ARGUMENTS,
		COMPLETION_INDEX,
		COMPLETION_STRUCT,
		COMPLETION_HINT,
	};

	struct Token {
		TokenType type;
		StringName text;
		double constant;
		uint16_t line;
		bool is_integer_constant() const {
			return type == TK_INT_CONSTANT || type == TK_UINT_CONSTANT;
		}
	};

	static String get_operator_text(Operator p_op);
	static String get_token_text(Token p_token);

	static bool is_token_datatype(TokenType p_type);
	static bool is_token_variable_datatype(TokenType p_type);
	static DataType get_token_datatype(TokenType p_type);
	static bool is_token_interpolation(TokenType p_type);
	static DataInterpolation get_token_interpolation(TokenType p_type);
	static bool is_token_precision(TokenType p_type);
	static bool is_token_arg_qual(TokenType p_type);
	static DataPrecision get_token_precision(TokenType p_type);
	static String get_precision_name(DataPrecision p_type);
	static String get_interpolation_name(DataInterpolation p_interpolation);
	static String get_datatype_name(DataType p_type);
	static String get_uniform_hint_name(ShaderNode::Uniform::Hint p_hint);
	static String get_texture_filter_name(TextureFilter p_filter);
	static String get_texture_repeat_name(TextureRepeat p_repeat);
	static bool is_token_nonvoid_datatype(TokenType p_type);
	static bool is_token_operator(TokenType p_type);
	static bool is_token_operator_assign(TokenType p_type);
	static bool is_token_hint(TokenType p_type);

	static bool convert_constant(ConstantNode *p_constant, DataType p_to_type, Scalar *p_value = nullptr);
	static DataType get_scalar_type(DataType p_type);
	static int get_cardinality(DataType p_type);
	static bool is_scalar_type(DataType p_type);
	static bool is_float_type(DataType p_type);
	static bool is_sampler_type(DataType p_type);
	static Variant constant_value_to_variant(const Vector<Scalar> &p_value, DataType p_type, int p_array_size, ShaderLanguage::ShaderNode::Uniform::Hint p_hint = ShaderLanguage::ShaderNode::Uniform::HINT_NONE);
	static PropertyInfo uniform_to_property_info(const ShaderNode::Uniform &p_uniform);
	static uint32_t get_datatype_size(DataType p_type);
	static uint32_t get_datatype_component_count(DataType p_type);

	static void get_keyword_list(List<String> *r_keywords);
	static bool is_control_flow_keyword(String p_keyword);
	static void get_builtin_funcs(List<String> *r_keywords);

	static int instance_counter;

	struct BuiltInInfo {
		DataType type = TYPE_VOID;
		bool constant = false;

		BuiltInInfo() {}

		BuiltInInfo(DataType p_type, bool p_constant = false) :
				type(p_type),
				constant(p_constant) {}
	};

	struct StageFunctionInfo {
		struct Argument {
			StringName name;
			DataType type;

			Argument(const StringName &p_name = StringName(), DataType p_type = TYPE_VOID) {
				name = p_name;
				type = p_type;
			}
		};

		Vector<Argument> arguments;
		DataType return_type = TYPE_VOID;
		String skip_function;
	};

	struct ModeInfo {
		StringName name;
		Vector<StringName> options;

		ModeInfo() {}

		ModeInfo(const StringName &p_name) :
				name(p_name) {
		}

		ModeInfo(const StringName &p_name, const StringName &p_arg1, const StringName &p_arg2) :
				name(p_name) {
			options.push_back(p_arg1);
			options.push_back(p_arg2);
		}

		ModeInfo(const StringName &p_name, const StringName &p_arg1, const StringName &p_arg2, const StringName &p_arg3) :
				name(p_name) {
			options.push_back(p_arg1);
			options.push_back(p_arg2);
			options.push_back(p_arg3);
		}

		ModeInfo(const StringName &p_name, const StringName &p_arg1, const StringName &p_arg2, const StringName &p_arg3, const StringName &p_arg4) :
				name(p_name) {
			options.push_back(p_arg1);
			options.push_back(p_arg2);
			options.push_back(p_arg3);
			options.push_back(p_arg4);
		}

		ModeInfo(const StringName &p_name, const StringName &p_arg1, const StringName &p_arg2, const StringName &p_arg3, const StringName &p_arg4, const StringName &p_arg5) :
				name(p_name) {
			options.push_back(p_arg1);
			options.push_back(p_arg2);
			options.push_back(p_arg3);
			options.push_back(p_arg4);
			options.push_back(p_arg5);
		}

		ModeInfo(const StringName &p_name, const StringName &p_arg1, const StringName &p_arg2, const StringName &p_arg3, const StringName &p_arg4, const StringName &p_arg5, const StringName &p_arg6) :
				name(p_name) {
			options.push_back(p_arg1);
			options.push_back(p_arg2);
			options.push_back(p_arg3);
			options.push_back(p_arg4);
			options.push_back(p_arg5);
			options.push_back(p_arg6);
		}
	};

	struct FunctionInfo {
		HashMap<StringName, BuiltInInfo> built_ins;
		HashMap<StringName, StageFunctionInfo> stage_functions;

		bool can_discard = false;
		bool main_function = false;
	};
	static bool has_builtin(const HashMap<StringName, ShaderLanguage::FunctionInfo> &p_functions, const StringName &p_name, bool p_check_global_funcs = false);

	typedef DataType (*GlobalShaderUniformGetTypeFunc)(const StringName &p_name);

	struct FilePosition {
		String file;
		int line = 0;
	};

private:
	struct KeyWord {
		TokenType token;
		const char *text;
		uint32_t flags;
		const Vector<String> excluded_shader_types;
		const Vector<String> excluded_functions;
	};

	static const KeyWord keyword_list[];

	GlobalShaderUniformGetTypeFunc global_shader_uniform_get_type_func = nullptr;

	bool error_set = false;
	String error_str;
	int error_line = 0;

	Vector<FilePosition> include_positions;
	HashSet<String> include_markers_handled;
	HashMap<StringName, int> function_overload_count;

	// Additional function information (eg. call hierarchy). No need to expose it to compiler.
	struct CallInfo {
		struct Item {
			enum ItemType {
				ITEM_TYPE_BUILTIN,
				ITEM_TYPE_VARYING,
			} type;

			TkPos pos;

			Item() {}
			Item(ItemType p_type, TkPos p_pos) :
					type(p_type), pos(p_pos) {}
		};

		StringName name;
		List<Pair<StringName, Item>> uses_restricted_items;
		List<CallInfo *> calls;
	};

	RBMap<StringName, CallInfo> calls_info;

#ifdef DEBUG_ENABLED
	struct Usage {
		int decl_line;
		bool used = false;
		Usage(int p_decl_line = -1) {
			decl_line = p_decl_line;
		}
	};

	HashMap<StringName, Usage> used_constants;
	HashMap<StringName, Usage> used_varyings;
	HashMap<StringName, Usage> used_uniforms;
	HashMap<StringName, Usage> used_functions;
	HashMap<StringName, Usage> used_structs;
	HashMap<ShaderWarning::Code, HashMap<StringName, Usage> *> warnings_check_map;

	HashMap<StringName, HashMap<StringName, Usage>> used_local_vars;
	HashMap<ShaderWarning::Code, HashMap<StringName, HashMap<StringName, Usage>> *> warnings_check_map2;

	List<ShaderWarning> warnings;

	bool check_warnings = false;
	uint32_t warning_flags = 0;

	void _add_line_warning(ShaderWarning::Code p_code, const StringName &p_subject = "", const Vector<Variant> &p_extra_args = Vector<Variant>()) {
		warnings.push_back(ShaderWarning(p_code, tk_line, p_subject, p_extra_args));
	}
	void _add_global_warning(ShaderWarning::Code p_code, const StringName &p_subject = "", const Vector<Variant> &p_extra_args = Vector<Variant>()) {
		warnings.push_back(ShaderWarning(p_code, -1, p_subject, p_extra_args));
	}
	void _add_warning(ShaderWarning::Code p_code, int p_line, const StringName &p_subject = "", const Vector<Variant> &p_extra_args = Vector<Variant>()) {
		warnings.push_back(ShaderWarning(p_code, p_line, p_subject, p_extra_args));
	}
	void _check_warning_accums();
#endif // DEBUG_ENABLED

	String code;
	int char_idx = 0;
	int tk_line = 0;

	StringName shader_type_identifier;
	StringName current_function;
	bool is_const_decl = false;
	StringName last_name;
	bool is_shader_inc = false;

	String current_uniform_group_name;
	String current_uniform_subgroup_name;

	VaryingFunctionNames varying_function_names;

	TkPos _get_tkpos() {
		TkPos tkp;
		tkp.char_idx = char_idx;
		tkp.tk_line = tk_line;
		return tkp;
	}

	void _set_tkpos(TkPos p_pos) {
		char_idx = p_pos.char_idx;
		tk_line = p_pos.tk_line;
	}

	void _set_error(const String &p_str) {
		if (error_set) {
			return;
		}

		error_line = tk_line;
		error_set = true;
		error_str = p_str;
		include_positions.write[include_positions.size() - 1].line = tk_line;
	}

	void _set_expected_error(const String &p_what) {
		_set_error(vformat(RTR("Expected a '%s'."), p_what));
	}

	void _set_expected_error(const String &p_first, const String p_second) {
		_set_error(vformat(RTR("Expected a '%s' or '%s'."), p_first, p_second));
	}

	void _set_expected_after_error(const String &p_what, const String &p_after) {
		_set_error(vformat(RTR("Expected a '%s' after '%s'."), p_what, p_after));
	}

	void _set_redefinition_error(const String &p_what) {
		_set_error(vformat(RTR("Redefinition of '%s'."), p_what));
	}

	void _set_parsing_error() {
		_set_error("Parser bug.");
	}

	static const char *token_names[TK_MAX];

	Token _make_token(TokenType p_type, const StringName &p_text = StringName());
	Token _get_token();
	bool _lookup_next(Token &r_tk);
	Token _peek();

	ShaderNode *shader = nullptr;

	enum IdentifierType {
		IDENTIFIER_FUNCTION,
		IDENTIFIER_UNIFORM,
		IDENTIFIER_VARYING,
		IDENTIFIER_FUNCTION_ARGUMENT,
		IDENTIFIER_LOCAL_VAR,
		IDENTIFIER_BUILTIN_VAR,
		IDENTIFIER_CONSTANT,
		IDENTIFIER_MAX,
	};

	IdentifierType last_type = IDENTIFIER_MAX;

	bool _find_identifier(const BlockNode *p_block, bool p_allow_reassign, const FunctionInfo &p_function_info, const StringName &p_identifier, DataType *r_data_type = nullptr, IdentifierType *r_type = nullptr, bool *r_is_const = nullptr, int *r_array_size = nullptr, StringName *r_struct_name = nullptr, Vector<Scalar> *r_constant_values = nullptr);
#ifdef DEBUG_ENABLED
	void _parse_used_identifier(const StringName &p_identifier, IdentifierType p_type, const StringName &p_function);
#endif // DEBUG_ENABLED
	bool _is_operator_assign(Operator p_op) const;
	bool _validate_assign(Node *p_node, const FunctionInfo &p_function_info, String *r_message = nullptr);
	bool _validate_operator(const BlockNode *p_block, OperatorNode *p_op, DataType *r_ret_type = nullptr, int *r_ret_size = nullptr);

	Vector<Scalar> _get_node_values(const BlockNode *p_block, Node *p_node);
	bool _eval_operator(const BlockNode *p_block, OperatorNode *p_op);
	Scalar _eval_unary_scalar(const Scalar &p_a, Operator p_op, DataType p_ret_type);
	Scalar _eval_scalar(const Scalar &p_a, const Scalar &p_b, Operator p_op, DataType p_ret_type, bool &r_is_valid);
	Vector<Scalar> _eval_unary_vector(const Vector<Scalar> &p_va, DataType p_ret_type, Operator p_op);
	Vector<Scalar> _eval_vector(const Vector<Scalar> &p_va, const Vector<Scalar> &p_vb, DataType p_left_type, DataType p_right_type, DataType p_ret_type, Operator p_op, bool &r_is_valid);
	Vector<Scalar> _eval_vector_transform(const Vector<Scalar> &p_va, const Vector<Scalar> &p_vb, DataType p_left_type, DataType p_right_type, DataType p_ret_type);

	struct BuiltinEntry {
		const char *name;
	};

	struct BuiltinFuncDef {
		enum { MAX_ARGS = 5 };
		const char *name;
		DataType rettype;
		const DataType args[MAX_ARGS];
		const char *args_names[MAX_ARGS];
		SubClassTag tag;
		bool high_end;
	};

	struct BuiltinFuncOutArgs { //arguments used as out in built in functions
		enum { MAX_ARGS = 2 };
		const char *name;
		const int arguments[MAX_ARGS];
	};

	struct BuiltinFuncConstArgs {
		const char *name;
		int arg;
		int min;
		int max;
	};

	CompletionType completion_type;
	ShaderNode::Uniform::Hint current_uniform_hint = ShaderNode::Uniform::HINT_NONE;
	TextureFilter current_uniform_filter = FILTER_DEFAULT;
	TextureRepeat current_uniform_repeat = REPEAT_DEFAULT;
	bool current_uniform_instance_index_defined = false;
	int completion_line = 0;
	BlockNode *completion_block = nullptr;
	DataType completion_base;
	bool completion_base_array = false;
	SubClassTag completion_class;
	StringName completion_function;
	StringName completion_struct;
	int completion_argument = 0;

#ifdef DEBUG_ENABLED
	uint32_t keyword_completion_context;
#endif // DEBUG_ENABLED

	const HashMap<StringName, FunctionInfo> *stages = nullptr;
	bool is_supported_frag_only_funcs = false;
	bool is_discard_supported = false;

	bool _get_completable_identifier(BlockNode *p_block, CompletionType p_type, StringName &identifier);
	static const BuiltinFuncDef builtin_func_defs[];
	static const BuiltinFuncOutArgs builtin_func_out_args[];
	static const BuiltinFuncConstArgs builtin_func_const_args[];
	static const BuiltinEntry frag_only_func_defs[];

	static bool is_const_suffix_lut_initialized;

	Error _validate_precision(DataType p_type, DataPrecision p_precision);
	bool _compare_datatypes(DataType p_datatype_a, String p_datatype_name_a, int p_array_size_a, DataType p_datatype_b, String p_datatype_name_b, int p_array_size_b);
	bool _compare_datatypes_in_nodes(Node *a, Node *b);

	bool _validate_function_call(BlockNode *p_block, const FunctionInfo &p_function_info, OperatorNode *p_func, DataType *r_ret_type, StringName *r_ret_type_str, bool *r_is_custom_function = nullptr);
	bool _parse_function_arguments(BlockNode *p_block, const FunctionInfo &p_function_info, OperatorNode *p_func, int *r_complete_arg = nullptr);
	ShaderNode::Uniform::Hint _sanitize_hint(ShaderNode::Uniform::Hint p_hint);
	bool _propagate_function_call_sampler_uniform_settings(const StringName &p_name, int p_argument, TextureFilter p_filter, TextureRepeat p_repeat, ShaderNode::Uniform::Hint p_hint);
	bool _propagate_function_call_sampler_builtin_reference(const StringName &p_name, int p_argument, const StringName &p_builtin);
	bool _validate_varying_assign(ShaderNode::Varying &p_varying, String *r_message);
	bool _check_node_constness(const Node *p_node) const;

	bool _check_restricted_func(const StringName &p_name, const StringName &p_current_function) const;
	bool _validate_restricted_func(const StringName &p_call_name, const CallInfo *p_func_info, bool p_is_builtin_hint = false);

	Node *_parse_expression(BlockNode *p_block, const FunctionInfo &p_function_info, const ExpressionInfo *p_previous_expression_info = nullptr);
	Error _parse_array_size(BlockNode *p_block, const FunctionInfo &p_function_info, bool p_forbid_unknown_size, Node **r_size_expression, int *r_array_size, bool *r_unknown_size);
	Node *_parse_array_constructor(BlockNode *p_block, const FunctionInfo &p_function_info);
	Node *_parse_array_constructor(BlockNode *p_block, const FunctionInfo &p_function_info, DataType p_type, const StringName &p_struct_name, int p_array_size);
	ShaderLanguage::Node *_reduce_expression(BlockNode *p_block, ShaderLanguage::Node *p_node);

	Node *_parse_and_reduce_expression(BlockNode *p_block, const FunctionInfo &p_function_info, const ExpressionInfo *p_previous_expression_info = nullptr);
	Error _parse_block(BlockNode *p_block, const FunctionInfo &p_function_info, bool p_just_one = false, bool p_can_break = false, bool p_can_continue = false);
	String _get_shader_type_list(const HashSet<String> &p_shader_types) const;
	String _get_qualifier_str(ArgumentQualifier p_qualifier) const;

	Error _parse_shader(const HashMap<StringName, FunctionInfo> &p_functions, const Vector<ModeInfo> &p_render_modes, const HashSet<String> &p_shader_types);

	Error _find_last_flow_op_in_block(BlockNode *p_block, FlowOperation p_op);
	Error _find_last_flow_op_in_op(ControlFlowNode *p_flow, FlowOperation p_op);

public:
#ifdef DEBUG_ENABLED
	List<ShaderWarning>::Element *get_warnings_ptr();

	void enable_warning_checking(bool p_enabled);
	bool is_warning_checking_enabled() const;

	void set_warning_flags(uint32_t p_flags);
	uint32_t get_warning_flags() const;
#endif // DEBUG_ENABLED

	//static void get_keyword_list(ShaderType p_type,List<String> *p_keywords);

	void clear();

	static String get_shader_type(const String &p_code);
	static bool is_builtin_func_out_parameter(const String &p_name, int p_param);

	struct ShaderCompileInfo {
		HashMap<StringName, FunctionInfo> functions;
		Vector<ModeInfo> render_modes;
		VaryingFunctionNames varying_function_names = VaryingFunctionNames();
		HashSet<String> shader_types;
		GlobalShaderUniformGetTypeFunc global_shader_uniform_type_func = nullptr;
		bool is_include = false;
	};

	Error compile(const String &p_code, const ShaderCompileInfo &p_info);
	Error complete(const String &p_code, const ShaderCompileInfo &p_info, List<ScriptLanguage::CodeCompletionOption> *r_options, String &r_call_hint);

	String get_error_text();
	Vector<FilePosition> get_include_positions();
	int get_error_line();

	ShaderNode *get_shader();

	String token_debug(const String &p_code);

	ShaderLanguage();
	~ShaderLanguage();
};

#endif // SHADER_LANGUAGE_H
