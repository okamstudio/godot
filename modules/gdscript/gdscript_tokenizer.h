/**************************************************************************/
/*  gdscript_tokenizer.h                                                  */
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

#ifndef GDSCRIPT_TOKENIZER_H
#define GDSCRIPT_TOKENIZER_H

#include "core/oa_hash_map.h"
#include "core/pair.h"
#include "core/string_name.h"
#include "core/ustring.h"
#include "core/variant.h"
#include "core/vmap.h"
#include "gdscript_functions.h"

class GDScriptTokenizer {
public:
	enum Token {

		TK_EMPTY,
		TK_IDENTIFIER,
		TK_CONSTANT,
		TK_SELF,
		TK_BUILT_IN_TYPE,
		TK_BUILT_IN_FUNC,
		TK_OP_IN,
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
		//TK_OP_PLUS_PLUS,
		//TK_OP_MINUS_MINUS,
		TK_CF_IF,
		TK_CF_ELIF,
		TK_CF_ELSE,
		TK_CF_FOR,
		TK_CF_WHILE,
		TK_CF_BREAK,
		TK_CF_CONTINUE,
		TK_CF_PASS,
		TK_CF_RETURN,
		TK_CF_MATCH,
		TK_PR_FUNCTION,
		TK_PR_CLASS,
		TK_PR_CLASS_NAME,
		TK_PR_EXTENDS,
		TK_PR_IS,
		TK_PR_ONREADY,
		TK_PR_TOOL,
		TK_PR_STATIC,
		TK_PR_EXPORT,
		TK_PR_SETGET,
		TK_PR_CONST,
		TK_PR_VAR,
		TK_PR_AS,
		TK_PR_VOID,
		TK_PR_ENUM,
		TK_PR_PRELOAD,
		TK_PR_ASSERT,
		TK_PR_YIELD,
		TK_PR_SIGNAL,
		TK_PR_BREAKPOINT,
		TK_PR_REMOTE,
		TK_PR_SYNC,
		TK_PR_MASTER,
		TK_PR_SLAVE, // Deprecated by TK_PR_PUPPET, to remove in 4.0
		TK_PR_PUPPET,
		TK_PR_REMOTESYNC,
		TK_PR_MASTERSYNC,
		TK_PR_PUPPETSYNC,
		TK_BRACKET_OPEN,
		TK_BRACKET_CLOSE,
		TK_CURLY_BRACKET_OPEN,
		TK_CURLY_BRACKET_CLOSE,
		TK_PARENTHESIS_OPEN,
		TK_PARENTHESIS_CLOSE,
		TK_COMMA,
		TK_SEMICOLON,
		TK_PERIOD,
		TK_QUESTION_MARK,
		TK_COLON,
		TK_DOLLAR,
		TK_FORWARD_ARROW,
		TK_NEWLINE,
		TK_CONST_PI,
		TK_CONST_TAU,
		TK_WILDCARD,
		TK_CONST_INF,
		TK_CONST_NAN,
		TK_ERROR,
		TK_EOF,
		TK_CURSOR, //used for code completion
		TK_TOOLTIP,
		TK_MAX
	};

protected:
	enum StringMode {
		STRING_SINGLE_QUOTE,
		STRING_DOUBLE_QUOTE,
		STRING_MULTILINE
	};

	static const char *token_names[TK_MAX];

	enum {
		TOKEN_HASH_TABLE_TYPE_START = 3,
		TOKEN_HASH_TABLE_BUILTIN_START = TOKEN_HASH_TABLE_TYPE_START + Variant::VARIANT_MAX,
		TOKEN_HASH_TABLE_KEYWORD_START = TOKEN_HASH_TABLE_BUILTIN_START + GDScriptFunctions::FUNC_MAX,
	};

	static OAHashMap<String, int> *token_hashtable;

public:
	static const char *get_token_name(Token p_token);

	static void initialize();
	static void terminate();

	bool is_token_literal(int p_offset = 0, bool variable_safe = false) const;
	StringName get_token_literal(int p_offset = 0) const;

	virtual const Variant &get_token_constant(int p_offset = 0) const = 0;
	virtual Token get_token(int p_offset = 0) const = 0;
	virtual StringName get_token_identifier(int p_offset = 0) const = 0;
	virtual GDScriptFunctions::Function get_token_built_in_func(int p_offset = 0) const = 0;
	virtual Variant::Type get_token_type(int p_offset = 0) const = 0;
	virtual int get_token_line(int p_offset = 0) const = 0;
	virtual int get_token_column(int p_offset = 0) const = 0;
	virtual int get_token_line_indent(int p_offset = 0) const = 0;
	virtual int get_token_line_tab_indent(int p_offset = 0) const = 0;
	virtual String get_token_error(int p_offset = 0) const = 0;
	virtual void advance(int p_amount = 1) = 0;
#ifdef DEBUG_ENABLED
	virtual const Vector<Pair<int, String>> &get_warning_skips() const = 0;
	virtual const Set<String> &get_warning_global_skips() const = 0;
	virtual bool is_ignoring_warnings() const = 0;
#endif // DEBUG_ENABLED

	virtual ~GDScriptTokenizer() {}
};

class GDScriptTokenizerText : public GDScriptTokenizer {
	enum {
		MAX_LOOKAHEAD = 4,
		TK_RB_SIZE = MAX_LOOKAHEAD * 2 + 1

	};

	struct TokenData {
		Token type;
		StringName identifier; //for identifier types
		Variant constant; //for constant types
		union {
			Variant::Type vtype; //for type types
			GDScriptFunctions::Function func; //function for built in functions
			int warning_code; //for warning skip
		};
		int line, col;
		TokenData() {
			type = TK_EMPTY;
			line = col = 0;
			vtype = Variant::NIL;
		}
	};

	void _make_token(Token p_type);
	void _make_newline(int p_indentation = 0, int p_tabs = 0);
	void _make_identifier(const StringName &p_identifier);
	void _make_built_in_func(GDScriptFunctions::Function p_func);
	void _make_constant(const Variant &p_constant);
	void _make_type(const Variant::Type &p_type);
	void _make_error(const String &p_error);

	String code;
	int len;
	int code_pos;
	const CharType *_code;
	int line;
	int column;
	TokenData tk_rb[TK_RB_SIZE * 2 + 1];
	int tk_rb_pos;
	String last_error;
	bool error_flag;

#ifdef DEBUG_ENABLED
	Vector<Pair<int, String>> warning_skips;
	Set<String> warning_global_skips;
	bool ignore_warnings;
#endif // DEBUG_ENABLED

	void _advance();
	bool _parse_identifier(const String &p_str);

public:
	void set_code(const String &p_code);
	virtual Token get_token(int p_offset = 0) const;
	virtual StringName get_token_identifier(int p_offset = 0) const;
	virtual GDScriptFunctions::Function get_token_built_in_func(int p_offset = 0) const;
	virtual Variant::Type get_token_type(int p_offset = 0) const;
	virtual int get_token_line(int p_offset = 0) const;
	virtual int get_token_column(int p_offset = 0) const;
	virtual int get_token_line_indent(int p_offset = 0) const;
	virtual int get_token_line_tab_indent(int p_offset = 0) const;
	virtual const Variant &get_token_constant(int p_offset = 0) const;
	virtual String get_token_error(int p_offset = 0) const;
	virtual void advance(int p_amount = 1);
#ifdef DEBUG_ENABLED
	virtual const Vector<Pair<int, String>> &get_warning_skips() const { return warning_skips; }
	virtual const Set<String> &get_warning_global_skips() const { return warning_global_skips; }
	virtual bool is_ignoring_warnings() const { return ignore_warnings; }
#endif // DEBUG_ENABLED
};

class GDScriptTokenizerBuffer : public GDScriptTokenizer {
	enum {

		TOKEN_BYTE_MASK = 0x80,
		TOKEN_BITS = 8,
		TOKEN_MASK = (1 << TOKEN_BITS) - 1,
		TOKEN_LINE_BITS = 24,
		TOKEN_LINE_MASK = (1 << TOKEN_LINE_BITS) - 1,
	};

	Vector<StringName> identifiers;
	Vector<Variant> constants;
	VMap<uint32_t, uint32_t> lines;
	Vector<uint32_t> tokens;
	Variant nil;
	int token;

public:
	Error set_code_buffer(const Vector<uint8_t> &p_buffer);
	static Vector<uint8_t> parse_code_string(const String &p_code);
	virtual Token get_token(int p_offset = 0) const;
	virtual StringName get_token_identifier(int p_offset = 0) const;
	virtual GDScriptFunctions::Function get_token_built_in_func(int p_offset = 0) const;
	virtual Variant::Type get_token_type(int p_offset = 0) const;
	virtual int get_token_line(int p_offset = 0) const;
	virtual int get_token_column(int p_offset = 0) const;
	virtual int get_token_line_indent(int p_offset = 0) const;
	virtual int get_token_line_tab_indent(int p_offset = 0) const { return 0; }
	virtual const Variant &get_token_constant(int p_offset = 0) const;
	virtual String get_token_error(int p_offset = 0) const;
	virtual void advance(int p_amount = 1);
#ifdef DEBUG_ENABLED
	virtual const Vector<Pair<int, String>> &get_warning_skips() const {
		static Vector<Pair<int, String>> v;
		return v;
	}
	virtual const Set<String> &get_warning_global_skips() const {
		static Set<String> s;
		return s;
	}
	virtual bool is_ignoring_warnings() const { return true; }
#endif // DEBUG_ENABLED
	GDScriptTokenizerBuffer();
};

#endif // GDSCRIPT_TOKENIZER_H
