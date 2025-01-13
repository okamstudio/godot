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

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

#ifdef MINGW_ENABLED
#undef CONST
#undef IN
#undef VOID
#endif

class GDScriptTokenizer {
public:
	enum CursorPlace {
		CURSOR_NONE,
		CURSOR_BEGINNING,
		CURSOR_MIDDLE,
		CURSOR_END,
	};

	struct Token {
		enum Type {
			EMPTY,
			// Basic
			ANNOTATION,
			IDENTIFIER,
			LITERAL,
			// Comparison
			LESS,
			LESS_EQUAL,
			GREATER,
			GREATER_EQUAL,
			EQUAL_EQUAL,
			BANG_EQUAL,
			// Logical
			AND,
			OR,
			NOT,
			AMPERSAND_AMPERSAND,
			PIPE_PIPE,
			BANG,
			// Bitwise
			AMPERSAND,
			PIPE,
			TILDE,
			CARET,
			LESS_LESS,
			GREATER_GREATER,
			// Math
			PLUS,
			MINUS,
			STAR,
			STAR_STAR,
			SLASH,
			PERCENT,
			// Assignment
			EQUAL,
			PLUS_EQUAL,
			MINUS_EQUAL,
			STAR_EQUAL,
			STAR_STAR_EQUAL,
			SLASH_EQUAL,
			PERCENT_EQUAL,
			LESS_LESS_EQUAL,
			GREATER_GREATER_EQUAL,
			AMPERSAND_EQUAL,
			PIPE_EQUAL,
			CARET_EQUAL,
			// Control flow
			IF,
			ELIF,
			ELSE,
			FOR,
			WHILE,
			BREAK,
			CONTINUE,
			PASS,
			RETURN,
			MATCH,
			WHEN,
			// Keywords
			AS,
			ASSERT,
			AWAIT,
			BREAKPOINT,
			CLASS,
			CLASS_NAME,
			CONST,
			ENUM,
			EXTENDS,
			FUNC,
			IN,
			IS,
			NAMESPACE,
			PRELOAD,
			SELF,
			SIGNAL,
			STATIC,
			SUPER,
			TRAIT,
			VAR,
			VOID,
			YIELD,
			// Punctuation
			BRACKET_OPEN,
			BRACKET_CLOSE,
			BRACE_OPEN,
			BRACE_CLOSE,
			PARENTHESIS_OPEN,
			PARENTHESIS_CLOSE,
			COMMA,
			SEMICOLON,
			PERIOD,
			PERIOD_PERIOD,
			COLON,
			DOLLAR,
			FORWARD_ARROW,
			UNDERSCORE,
			// Whitespace
			NEWLINE,
			INDENT,
			DEDENT,
			// Constants
			CONST_PI,
			CONST_TAU,
			CONST_INF,
			CONST_NAN,
			// Error message improvement
			VCS_CONFLICT_MARKER,
			BACKTICK,
			QUESTION_MARK,
			// Special
			ERROR,
			TK_EOF, // "EOF" is reserved
			TK_MAX
		};

		Type type = EMPTY;
		Variant literal;
		int start_line = 0, end_line = 0, start_column = 0, end_column = 0;
		int leftmost_column = 0, rightmost_column = 0; // Column span for multiline tokens.
		int cursor_position = -1;
		CursorPlace cursor_place = CURSOR_NONE;
		String source;

		const char *get_name() const;
		String get_debug_name() const;
		bool can_precede_bin_op() const;
		bool is_identifier() const;
		bool is_node_name() const;
		StringName get_identifier() const { return literal; }

		Token(Type p_type) {
			type = p_type;
		}

		Token() {}
	};

#ifdef TOOLS_ENABLED
	struct CommentData {
		String comment;
		// true: Comment starts at beginning of line or after indentation.
		// false: Inline comment (starts after some code).
		bool new_line = false;
		CommentData() {}
		CommentData(const String &p_comment, bool p_new_line) {
			comment = p_comment;
			new_line = p_new_line;
		}
	};
	virtual const HashMap<int, CommentData> &get_comments() const = 0;
#endif // TOOLS_ENABLED

	static String get_token_name(Token::Type p_token_type);

	virtual int get_cursor_line() const = 0;
	virtual int get_cursor_column() const = 0;
	virtual void set_cursor_position(int p_line, int p_column) = 0;
	virtual void set_multiline_mode(bool p_state) = 0;
	virtual bool is_past_cursor() const = 0;
	virtual void push_expression_indented_block() = 0; // For lambdas, or blocks inside expressions.
	virtual void pop_expression_indented_block() = 0; // For lambdas, or blocks inside expressions.
	virtual bool is_text() = 0;

	virtual Token scan() = 0;

	virtual ~GDScriptTokenizer() {}
};

class GDScriptTokenizerText : public GDScriptTokenizer {
	String source;
	const char32_t *_source = nullptr;
	const char32_t *_current = nullptr;
	int line = -1, column = -1;
	int cursor_line = -1, cursor_column = -1;
	int tab_size = 4;

	// Keep track of multichar tokens.
	const char32_t *_start = nullptr;
	int start_line = 0, start_column = 0;
	int leftmost_column = 0, rightmost_column = 0;

	// Info cache.
	bool line_continuation = false; // Whether this line is a continuation of the previous, like when using '\'.
	bool multiline_mode = false;
	List<Token> error_stack;
	bool pending_newline = false;
	Token last_token;
	Token last_newline;
	int pending_indents = 0;
	List<int> indent_stack;
	List<List<int>> indent_stack_stack; // For lambdas, which require manipulating the indentation point.
	List<char32_t> paren_stack;
	char32_t indent_char = '\0';
	int position = 0;
	int length = 0;
	Vector<int> continuation_lines;
#ifdef DEBUG_ENABLED
	Vector<String> keyword_list;
#endif // DEBUG_ENABLED

#ifdef TOOLS_ENABLED
	HashMap<int, CommentData> comments;
#endif // TOOLS_ENABLED

	_FORCE_INLINE_ bool _is_at_end() { return position >= length; }
	_FORCE_INLINE_ char32_t _peek(int p_offset = 0) { return position + p_offset >= 0 && position + p_offset < length ? _current[p_offset] : '\0'; }
	int indent_level() const { return indent_stack.size(); }
	bool has_error() const { return !error_stack.is_empty(); }
	Token pop_error();
	char32_t _advance();
	String _get_indent_char_name(char32_t ch);
	void _skip_whitespace();
	void check_indent();

#ifdef DEBUG_ENABLED
	void make_keyword_list();
#endif // DEBUG_ENABLED

	Token make_error(const String &p_message);
	void push_error(const String &p_message);
	void push_error(const Token &p_error);
	Token make_paren_error(char32_t p_paren);
	Token make_token(Token::Type p_type);
	Token make_literal(const Variant &p_literal);
	Token make_identifier(const StringName &p_identifier);
	Token check_vcs_marker(char32_t p_test, Token::Type p_double_type);
	void push_paren(char32_t p_char);
	bool pop_paren(char32_t p_expected);

	void newline(bool p_make_token);
	Token number();
	Token potential_identifier();
	Token string();
	Token annotation();

public:
	void set_source_code(const String &p_source_code);

	const Vector<int> &get_continuation_lines() const { return continuation_lines; }

	virtual int get_cursor_line() const override;
	virtual int get_cursor_column() const override;
	virtual void set_cursor_position(int p_line, int p_column) override;
	virtual void set_multiline_mode(bool p_state) override;
	virtual bool is_past_cursor() const override;
	virtual void push_expression_indented_block() override; // For lambdas, or blocks inside expressions.
	virtual void pop_expression_indented_block() override; // For lambdas, or blocks inside expressions.
	virtual bool is_text() override { return true; }

#ifdef TOOLS_ENABLED
	virtual const HashMap<int, CommentData> &get_comments() const override {
		return comments;
	}
#endif // TOOLS_ENABLED

	virtual Token scan() override;

	GDScriptTokenizerText();
};

#endif // GDSCRIPT_TOKENIZER_H
